// simcore/air_war/planner.hpp — one planning cycle of the enemy air command: the air tasking order
// (docs/air_war/04_PLANNING.md).
//
// The cycle, as the staff works it:
//   1. The commander sets the tempo and the apportionment: the doctrine's phase, bent by his character (the methodical
//      suppress and look first, the bold strike), by what the intelligence believes (many working air defences: more
//      SEAD; a stale picture: more reconnaissance) and by the pressure from above.
//   2. The intelligence officer's picture gives the candidates for each mission kind.
//   3. The planner weighs each candidate over the ways to fly it (entry point, height, time of day) by utility: what it
//      is worth to him, the chance it is really there and working, the losses he expects from the threats he believes
//      in (less with SEAD and jamming), what he remembers of that way of doing it, his quirks, the grudge, and noise
//      that shrinks with his skill. He builds the package as the doctrine's template says (strike group and support
//      groups: SEAD, escort, jamming, decoys, additional reconnaissance; mai_tactics_vvs §3.3.3, afmag_1989_slam).
//   4. The commander approves or rejects each one (expected losses against his boldness and the loss tolerance left
//      today; politics' bans and no-strike zones), and higher authority may move a time on target over his objection
//      (hgram_080).
// Every package keeps its reasons, in the words of the officer who decided.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/intel.hpp"
#include "simcore/air_war/officers.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"

namespace simcore {
namespace airwar {

struct PlanContext {
    const AirWarData* data = nullptr;
    AirStaff* staff = nullptr;
    IntelPicture* intel = nullptr;
    std::vector<SquadronState>* squadrons = nullptr;
    PoliticsState* politics = nullptr;
    std::vector<Weather> weather;  // by campaign day (index 0: day 1)
    double now_h = 0.0;
    int phase = 0;
    double* arm_stock = nullptr;
    std::map<std::string, double>* missile_stock = nullptr;  // cruise missiles left, by type
    Rng* rng = nullptr;
    int* next_id = nullptr;
    std::vector<JournalEntry>* journal = nullptr;
    const std::vector<ObjectState>* truth = nullptr;  // only to know which objects exist (hardness, value): never positions

    Weather WeatherAt(double h) const {
        const double abs_h = data->theatre.start_hour + h;
        const int d = static_cast<int>(std::floor(abs_h / 24.0));
        if (weather.empty()) return Weather::Clear;
        return weather[static_cast<std::size_t>(std::min(std::max(d, 0), static_cast<int>(weather.size()) - 1))];
    }
};

inline double ProfileHeight(const AircraftType& a, Profile p) {
    switch (p) {
        case Profile::Low: return a.low_agl_m;
        case Profile::Medium: return a.medium_alt_m;
        case Profile::High: return std::min(10000.0, a.medium_alt_m * 2.0);
    }
    return a.low_agl_m;
}

// Times along a route: the flight is over point `anchor` at `anchor_h`, at `speed_mps` throughout.
inline void TimeRoute(std::vector<RoutePoint>& r, std::size_t anchor, double anchor_h, double speed_mps) {
    if (r.empty()) return;
    if (anchor >= r.size()) anchor = r.size() - 1;
    r[anchor].t_h = anchor_h;
    for (std::size_t i = anchor + 1; i < r.size(); ++i) r[i].t_h = r[i - 1].t_h + Dist2d(r[i - 1].pos, r[i].pos) / speed_mps / 3600.0;
    for (std::size_t i = anchor; i-- > 0;) r[i].t_h = r[i + 1].t_h - Dist2d(r[i].pos, r[i + 1].pos) / speed_mps / 3600.0;
}

// The share of a pass through a believed threat ring, the threat's reach at that height, and the kill chance per
// aircraft: shared by the planner's estimate and the resolver (with true values there).
inline double PassKillChance(double lethality, double chord_m, double radius_m, Profile profile, bool helicopter,
                             bool low_cover, bool medium_cover, bool radar, double sead_supp, double jam, double env,
                             double evasion, const TuningTable& tune) {
    if (chord_m <= 0.0 || radius_m <= 0.0) return 0.0;
    const double exposure = std::min(1.0, chord_m / (2.0 * radius_m));
    double alt = 1.0;
    const bool long_range = radius_m > 15000.0;
    const Profile p = helicopter ? Profile::Low : profile;
    if (p == Profile::Low) {
        alt = low_cover ? (long_range ? tune.Get("attrition.low_vs_long_range_sam") : 1.0) : 0.0;
    } else if (p == Profile::Medium) {
        alt = medium_cover ? 1.0 : tune.Get("attrition.medium_vs_guns");
    } else {
        alt = (medium_cover && long_range) ? 1.0 : tune.Get("attrition.high_vs_short_range");
    }
    double k = lethality * exposure * alt;
    if (radar) k *= (1.0 - sead_supp) * (1.0 - jam);
    k *= env * (1.0 - 0.5 * evasion);
    return std::min(k, tune.Get("attrition.max_per_pass"));
}

// How many aircraft of a passing flight a defence engages: its fire channels, scaled by how much of it works (an Osa
// battery that has lost two of its four radars engages two); without channels every aircraft in reach. Shared by the
// planner and the resolver.
inline int EngagedAircraft(int channels, double function, int in_reach) {
    if (channels <= 0) return in_reach;
    const int working = std::max(1, static_cast<int>(std::lround(channels * Clamp01(function))));
    return std::min(in_reach, working);
}

// Light guns and shoulder-fired missiles around a military target that has no air defence of its own in the theatre:
// the chance an aircraft attacking it at low level (or a helicopter) is shot down on the attack. Troops carry the most
// (their own anti-aircraft guns and missiles); towns none; medium altitude is out of their reach (Dugman 5: most
// aircraft were hit by guns at the low approach height, wiki_model5). Shared by the planner and the resolver.
inline double TargetGunsChance(ObjectKind k, Profile profile, bool helicopter, double evasion, double env, const TuningTable& tune) {
    if (!helicopter && profile != Profile::Low) return 0.0;
    if (k == ObjectKind::City) return 0.0;
    const double g = k == ObjectKind::Troops ? tune.Get("attrition.target_guns_troops") : tune.Get("attrition.target_guns_site");
    return std::min(tune.Get("attrition.max_per_pass"), g * env * (1.0 - 0.5 * evasion));
}

class Planner {
public:
    std::vector<Package> Plan(PlanContext& c) {
        c_ = &c;
        out_.clear();
        const AirWarData& d = *c.data;
        const FactionDef& f = d.faction;
        Officer* cdr = c.staff->HolderPtr(StaffRole::Commander);
        Officer* pln = c.staff->HolderPtr(StaffRole::Planner);
        if (cdr == nullptr || pln == nullptr) return out_;
        front_ = d.theatre.FrontFor(f.front);
        if (front_ == nullptr) return out_;
        if (c.politics->paused_until_h > c.now_h) {
            Log("decision", cdr->Title(), "No packages this cycle: operations paused by higher authority until " +
                                              FormatHour(c.politics->paused_until_h, d.theatre.start_hour) + ".");
            return out_;
        }
        const PhaseDef& phase = f.doctrine.phases[static_cast<std::size_t>(std::max(0, std::min(c.phase, static_cast<int>(f.doctrine.phases.size()) - 1)))];

        // 1. Tempo and apportionment.
        const double cycle = f.doctrine.planning_cycle_h;
        double available = 0.0;
        for (SquadronState& s : *c.squadrons) {
            const double per_cycle = s.Serviceable(c.now_h, f.doctrine.serviceability) * f.doctrine.sortie_rate_per_day * phase.tempo *
                                     std::min(1.0, cycle / 24.0);
            s.sorties_left = per_cycle;
            available += per_cycle;
        }
        const Traits& ct = cdr->def.traits;
        double tempo = 0.7 + 0.3 * ct.boldness + 0.3 * c.politics->pressure - 0.4 * cdr->stress;
        tempo = ClampTo(tempo, 0.35, 1.3) * c.politics->sortie_factor * (1.0 - f.doctrine.on_call_fraction);
        double budget = available * tempo;
        for (SquadronState& s : *c.squadrons) s.sorties_left *= ClampTo(tempo, 0.0, 1.0);

        double share[kMissionKinds];
        const double iads = c.intel->BelievedIadsShare();
        int fresh = 0;
        for (const IntelEntry& e : c.intel->entries) {
            if (!e.ghost && c.now_h - e.observed_h < 24.0) ++fresh;
        }
        const double coverage = c.intel->entries.empty() ? 0.0 : static_cast<double>(fresh) / static_cast<double>(c.intel->entries.size());
        const double doctrine_weight = Clamp01(0.4 + 0.5 * ct.rigidity + 0.3 * cdr->Quirk("doctrine_weight"));
        std::string bent;
        for (int k = 0; k < kMissionKinds; ++k) {
            double s = phase.apportion[k];
            const MissionKind mk = static_cast<MissionKind>(k);
            if (!f.doctrine.packages[k].allowed || (mk == MissionKind::Missile && MissileType() == nullptr)) {
                share[k] = 0.0;
                continue;
            }
            double m = 1.0;
            if (mk == MissionKind::Dead || mk == MissionKind::Hunter) m *= 1.0 + 0.4 * (ct.methodical - 0.5) + 0.4 * (iads - 0.5);
            if (mk == MissionKind::Recce || mk == MissionKind::Bda) m *= 1.0 + 0.6 * (0.5 - coverage) + 0.3 * cdr->Quirk("recce_first");
            if (mk == MissionKind::Strike || mk == MissionKind::Cas) m *= 1.0 + 0.4 * (ct.boldness - 0.5) + 0.5 * c.politics->pressure;
            if (mk == MissionKind::Deception) m *= 1.0 + 0.6 * (ct.creativity - 0.5) + 0.5 * cdr->Quirk("decoys");
            if (mk == MissionKind::HeloRaid) m *= std::max(0.0, 1.0 + 0.5 * cdr->Quirk("helicopter_use"));
            share[k] = s * (doctrine_weight + (1.0 - doctrine_weight) * m);
        }
        double total = 0.0;
        for (double s : share) total += s;
        if (total <= 0.0) return out_;
        for (double& s : share) s /= total;
        if (c.politics->pressure > 0.6) bent += " under pressure from above for results;";
        if (iads > 0.6 && AirStaff::HasSead(f)) bent += " many of the enemy's air defences still work;";
        if (coverage < 0.3) bent += " the picture is stale;";
        Log("decision", cdr->Title(),
            "Phase '" + phase.name + "'. Tempo " + Pct(tempo) + " of the " + std::to_string(static_cast<int>(available + 0.5)) +
                " sorties available this cycle." + (bent.empty() ? std::string() : " Apportionment bent:" + bent));

        // 2-4. Mission kinds in the order the staff fills them.
        const MissionKind order[] = {MissionKind::Missile, MissionKind::Dead,     MissionKind::Hunter,
                                     MissionKind::Deception, MissionKind::Strike, MissionKind::Recce,
                                     MissionKind::Cas,      MissionKind::HeloRaid, MissionKind::Harassment,
                                     MissionKind::Bda};
        const int max_packages = static_cast<int>(f.doctrine.max_packages_per_cycle * phase.tempo * ClampTo(tempo, 0.3, 1.2) + 0.5);
        for (MissionKind mk : order) {
            const int k = static_cast<int>(mk);
            if (share[k] <= 0.0) continue;
            double kind_budget = budget * share[k];
            std::set<std::string> used;
            // No kind takes more than its share of the package slots (and at least one).
            const int kind_cap = std::max(1, static_cast<int>(std::ceil(max_packages * share[k] * 1.5)));
            int kind_count = 0;
            int rejected = 0;
            for (int guard = 0; guard < 16 && kind_budget >= 1.0 && kind_count < kind_cap &&
                                static_cast<int>(out_.size()) < max_packages;
                 ++guard) {
                Package p;
                if (!(mk == MissionKind::Missile ? BestMissilePackage(used, p) : BestPackage(mk, used, p))) break;
                used.insert(p.target_key);
                if (!Approve(p, *cdr)) {
                    if (++rejected >= 2) break;  // the planner stops offering what the commander won't take
                    continue;
                }
                Commit(p);
                kind_budget -= p.Aircraft();
                out_.push_back(p);
                ++kind_count;
                if (pln->Quirk("double_tap") > 0.0 && (mk == MissionKind::Strike || mk == MissionKind::Dead) &&
                    static_cast<int>(out_.size()) < max_packages) {
                    Package second;
                    if (DoubleTap(out_.back(), second)) {
                        Commit(second);
                        kind_budget -= second.Aircraft();
                        out_.push_back(second);
                    }
                }
            }
        }
        Interference(*cdr);
        int sorties = 0;
        for (const Package& p : out_) sorties += p.Aircraft();
        Log("plan", pln->Title(), "Air tasking order: " + std::to_string(out_.size()) + " packages, " + std::to_string(sorties) +
                                      " sorties, " + FormatHour(c.now_h + f.doctrine.lead_time_h, d.theatre.start_hour) + " to " +
                                      FormatHour(c.now_h + f.doctrine.lead_time_h + cycle, d.theatre.start_hour) + ".");
        pln->packages_planned += static_cast<int>(out_.size());
        return out_;
    }

private:
    PlanContext* c_ = nullptr;
    const FrontDef* front_ = nullptr;
    std::vector<Package> out_;

    void Log(const std::string& kind, const std::string& who, const std::string& text) {
        c_->journal->push_back({c_->now_h, kind, who, text, false});
    }
    static std::string Pct(double x) { return std::to_string(static_cast<int>(x * 100.0 + 0.5)) + "%"; }
    static std::string Num1(double x) {
        char b[32];
        std::snprintf(b, sizeof(b), "%.1f", x);
        return b;
    }

    const TuningTable& Tune() const { return c_->data->tuning; }
    Officer& PlannerOfficer() { return *c_->staff->HolderPtr(StaffRole::Planner); }
    const Officer* SeadOfficer() const { return c_->staff->HolderPtr(StaffRole::Sead); }

    const AircraftType* TypeOf(const SquadronState& s) const { return c_->data->faction.Aircraft(s.aircraft); }

    static bool Fits(const AircraftType& a, FlightRole r) {
        if (a.Has(r)) return true;
        if (r == FlightRole::Dead) return a.Has(FlightRole::Strike);
        if (r == FlightRole::Hunter) return a.Has(FlightRole::Sead) && a.arms > 0;
        return false;
    }

    // The squadron that can fly `count` aircraft in this role, most sorties left first (the preferred types first, in
    // their order, when the doctrine names them). -1: none.
    int PickSquadron(FlightRole r, int count, Profile p, DayPart part, Weather w, const std::vector<double>& reserved,
                     const std::vector<std::string>* preferred = nullptr) const {
        if (preferred != nullptr) {
            for (const std::string& want : *preferred) {
                std::vector<std::string> one = {want};
                const int i = PickSquadronOf(r, count, p, part, w, reserved, &one);
                if (i >= 0) return i;
            }
        }
        return PickSquadronOf(r, count, p, part, w, reserved, nullptr);
    }
    int PickSquadronOf(FlightRole r, int count, Profile p, DayPart part, Weather w, const std::vector<double>& reserved,
                       const std::vector<std::string>* only) const {
        int best = -1;
        double best_left = 0.0;
        const auto& sq = *c_->squadrons;
        for (std::size_t i = 0; i < sq.size(); ++i) {
            const AircraftType* a = TypeOf(sq[i]);
            if (a == nullptr || a->missile || !a->InService(c_->data->theatre.year) || !Fits(*a, r)) continue;
            if (only != nullptr && std::find(only->begin(), only->end(), a->id) == only->end()) continue;
            if (a->helicopter && p != Profile::Low) continue;
            if (part == DayPart::Night && !a->night) continue;
            if (w == Weather::Poor && !a->all_weather && PlannerQuirk("weather_tolerance") <= 0.0) continue;
            if ((r == FlightRole::Sead || r == FlightRole::Hunter) && a->arms <= 0) continue;
            const double left = sq[i].sorties_left - reserved[i];
            if (left + 1e-9 < count) continue;
            if (left > best_left) {
                best_left = left;
                best = static_cast<int>(i);
            }
        }
        return best;
    }
    double PlannerQuirk(const std::string& k) const { return c_->staff->HolderPtr(StaffRole::Planner)->Quirk(k); }

    // Candidates for a mission kind from the intelligence picture.
    std::vector<const IntelEntry*> Candidates(MissionKind mk) const {
        std::vector<const IntelEntry*> out;
        const double now = c_->now_h;
        for (const IntelEntry& e : c_->intel->entries) {
            if (e.Live() < 0.15 && mk != MissionKind::Recce && mk != MissionKind::Bda) continue;
            const double to_front = DistanceToLine(front_->line, e.pos);
            const bool ad = IsAirDefence(e.kind) || e.kind == ObjectKind::EwRadar;
            switch (mk) {
                case MissionKind::Strike:
                    if (ad || e.kind == ObjectKind::Troops || e.kind == ObjectKind::City) continue;
                    break;
                case MissionKind::Dead:
                    if (!ad || e.sigma_m > 4000.0) continue;
                    if (IsAirDefence(e.kind) && !e.IsSamRadar()) continue;  // guns are for the strike's escort, not DEAD
                    break;
                case MissionKind::Hunter:
                    if (!e.IsSamRadar()) continue;
                    break;
                case MissionKind::Deception:
                    if (!e.IsSamRadar()) continue;
                    break;
                case MissionKind::Recce:
                    if (e.kind == ObjectKind::City) continue;
                    if (!(e.mobile && (now - e.observed_h > 8.0 || e.sigma_m > 2500.0)) && e.exists > 0.6) continue;
                    break;
                case MissionKind::Bda:
                    if (e.strikes == 0 || now - e.last_struck_h > 30.0 || e.observed_h > e.last_struck_h + 0.5) continue;
                    break;
                case MissionKind::Cas:
                    if (e.kind != ObjectKind::Troops || to_front > 40000.0) continue;
                    break;
                case MissionKind::HeloRaid:
                    if (to_front > 25000.0 || e.kind == ObjectKind::City || e.kind == ObjectKind::Airfield) continue;
                    break;
                case MissionKind::Harassment:
                    if (to_front > 70000.0 || e.kind == ObjectKind::City) continue;
                    break;
                case MissionKind::Missile:
                    // Fixed targets only: a missile flies to where it was told, and finds nothing that has moved.
                    if (e.mobile || e.kind == ObjectKind::Troops || e.kind == ObjectKind::City || IsAirDefence(e.kind)) continue;
                    break;
                case MissionKind::Count: continue;
            }
            if (c_->politics->Banned(e.kind)) continue;
            if (c_->politics->max_depth_m > 0.0 && to_front > c_->politics->max_depth_m) continue;
            if (InNoStrikeZone(e.pos) && mk != MissionKind::Recce && mk != MissionKind::Bda) continue;
            out.push_back(&e);
        }
        return out;
    }

    bool InNoStrikeZone(const Vec3& p) const {
        const double r = c_->politics->city_radius_m;
        if (r <= 0.0) return false;
        for (const TheatreObject& o : c_->data->theatre.objects) {
            if (o.kind == ObjectKind::City && Dist2d(o.pos, p) < o.radius_m + r) return true;
        }
        return false;
    }

    // The worth of an entry to this staff.
    double ValueOf(const IntelEntry& e, MissionKind mk, std::vector<std::string>* why) const {
        const PoliticsDef& pol = c_->data->faction.politics;
        double v = e.value * pol.target_weight[static_cast<int>(e.kind)];
        if (mk == MissionKind::Dead || mk == MissionKind::Hunter || mk == MissionKind::Deception) {
            v = std::max(v, 0.5 * e.threat_lethality / 0.2 + 0.3);  // what it costs to leave it alive
        }
        const Officer* cdr = c_->staff->HolderPtr(StaffRole::Commander);
        double grudge = 0.0;
        if (cdr != nullptr) {
            const auto it = cdr->grudges.find(e.key);
            if (it != cdr->grudges.end()) grudge = it->second * (0.5 + cdr->def.traits.vindictiveness + cdr->Quirk("grudge"));
        }
        if (grudge > 0.0) {
            v += Tune().Get("utility.grudge_weight") * grudge;
            if (why != nullptr) {
                why->push_back("this one has cost us " + std::to_string(static_cast<int>(e.losses_credited + 0.5)) +
                               " aircraft: the commander wants it gone");
            }
        }
        return v;
    }

    struct Option {
        int entry = 0;
        int exit = 0;
        Profile profile = Profile::Low;
        DayPart part = DayPart::Day;
        double tot_h = 0.0;
    };

    bool NextTot(DayPart part, double& tot_h) const {
        const FactionDef& f = c_->data->faction;
        const double start = c_->now_h + f.doctrine.lead_time_h;
        const double end = start + f.doctrine.planning_cycle_h;
        const double local = std::fmod(c_->data->theatre.start_hour + start, 24.0);
        double delta = std::fmod(DayPartHour(part) - local + 48.0, 24.0);
        tot_h = start + delta;
        return tot_h < end + 1e-9;
    }

    // Build the package for one option. Returns false if the squadrons can't man it.
    bool Build(MissionKind mk, const IntelEntry& e, const Option& o, Package& p, std::vector<std::string>& why) const {
        const AirWarData& d = *c_->data;
        const FactionDef& f = d.faction;
        const PackageTemplate& t = f.doctrine.packages[static_cast<int>(mk)];
        const EntryPoint& entry = front_->entries[static_cast<std::size_t>(o.entry)];
        const EntryPoint& exit = front_->entries[static_cast<std::size_t>(o.exit)];
        const Weather w = c_->WeatherAt(o.tot_h);
        const Officer& pln = *c_->staff->HolderPtr(StaffRole::Planner);
        std::vector<double> reserved(c_->squadrons->size(), 0.0);
        p = Package();
        p.kind = mk;
        p.target_key = e.key;
        p.target_object = e.object_id;
        p.target_label = e.label;
        p.target_pos = e.pos;
        p.tot_h = o.tot_h;
        p.part = o.part;
        p.profile = o.profile;
        p.entry = entry.id;
        p.exit = exit.id;
        p.tactic = TacticKey(entry.id, o.profile, o.part, mk);

        FlightRole main_role = FlightRole::Strike;
        switch (mk) {
            case MissionKind::Dead: main_role = FlightRole::Dead; break;
            case MissionKind::Hunter: main_role = FlightRole::Hunter; break;
            case MissionKind::Recce:
            case MissionKind::Bda: main_role = FlightRole::Recce; break;
            case MissionKind::HeloRaid: main_role = FlightRole::Helicopter; break;
            case MissionKind::Deception: main_role = FlightRole::Decoy; break;
            default: main_role = FlightRole::Strike; break;
        }
        // Massed blows or a drip of small raids (doctrine, character).
        double mass = 1.0 + (f.doctrine.simultaneous_bias - 0.5) + 0.5 * pln.Quirk("massing") - 0.4 * pln.Quirk("drip_feed");
        int main_flights = std::max(1, static_cast<int>(t.main_flights * mass + 0.5));
        if (mk == MissionKind::Recce || mk == MissionKind::Bda) main_flights = 1;
        const int fsize = t.flight_size;
        const int sq_main = PickSquadron(main_role, fsize, o.profile, o.part, w, reserved, t.aircraft.empty() ? nullptr : &t.aircraft);
        if (sq_main < 0) return false;
        const AircraftType& ma = *TypeOf((*c_->squadrons)[static_cast<std::size_t>(sq_main)]);
        if (ma.helicopter && o.profile != Profile::Low) return false;
        // As many main flights as the squadron can give, up to the plan.
        int flights_ok = 0;
        for (int i = 0; i < main_flights; ++i) {
            if ((*c_->squadrons)[static_cast<std::size_t>(sq_main)].sorties_left - reserved[static_cast<std::size_t>(sq_main)] + 1e-9 < fsize) break;
            reserved[static_cast<std::size_t>(sq_main)] += fsize;
            ++flights_ok;
        }
        if (flights_ok == 0) return false;
        int cs = 0;
        auto callsign = [&](int n) {
            const auto& list = f.callsigns;
            const int base = (*c_->next_id * 7 + n * 3) % static_cast<int>(list.size());
            return list[static_cast<std::size_t>(base)] + " " + std::to_string(10 + (*c_->next_id % 9) * 10 + n);
        };
        const double ip_dist = mk == MissionKind::HeloRaid ? 6000.0 : 20000.0;
        for (int i = 0; i < flights_ok; ++i) {
            Flight fl;
            fl.callsign = callsign(cs++);
            fl.role = main_role;
            fl.squadron = (*c_->squadrons)[static_cast<std::size_t>(sq_main)].id;
            fl.aircraft = ma.id;
            fl.count = fsize;
            fl.profile = o.profile;
            fl.arms = main_role == FlightRole::Hunter ? ma.arms * fsize : 0;
            const double offset_h = (1.0 - f.doctrine.simultaneous_bias) * 0.05 * i;  // successive flights a few minutes apart
            if (mk == MissionKind::Hunter) {
                fl.route = LoiterRoute(entry, exit, e.pos, ProfileHeight(ma, o.profile), o.tot_h, ma.cruise_mps);
            } else {
                fl.route = MainRoute(entry, exit, e.pos, ProfileHeight(ma, o.profile), o.tot_h + offset_h, ma, ip_dist,
                                     mk == MissionKind::HeloRaid);
            }
            fl.release_index = ReleaseIndex(fl.route);
            fl.aim_key = e.key;
            fl.aim_pos = e.pos;
            p.flights.push_back(fl);
        }
        // Threats the planner believes are on the route.
        int radar_threats = 0;
        for (const IntelEntry* th : c_->intel->Threats()) {
            if (th->key == e.key && mk == MissionKind::Dead) {
                ++radar_threats;
                continue;
            }
            if (RouteChord(p.flights.front().route, th->pos, th->threat_radius_m) > 0.0 && th->threat_radar) ++radar_threats;
        }
        // SEAD with the package.
        const bool seady = mk == MissionKind::Strike || mk == MissionKind::Dead || mk == MissionKind::Cas ||
                           mk == MissionKind::Recce || mk == MissionKind::Deception;
        if (seady && t.sead_per_threat > 0.0 && radar_threats > 0) {
            const double ratio = t.sead_per_threat *
                                 (1.0 + pln.Quirk("sead_ratio") + pln.lesson_sead + 0.4 * (pln.def.traits.methodical - 0.5));
            int want = std::min(t.sead_max, static_cast<int>(std::ceil(radar_threats * ratio)));
            const Officer* sead_off = SeadOfficer();
            const double hoard = std::max(pln.Quirk("arm_reserve"), sead_off != nullptr ? sead_off->Quirk("arm_reserve") : 0.0);
            const double reserve = *c_->arm_stock * Clamp01(0.3 * hoard);
            for (int tries = 0; want > 0 && tries < 4; ++tries) {
                const int n = std::min(want, 2);
                const int sq = PickSquadron(FlightRole::Sead, n, Profile::Medium, o.part, w, reserved);
                if (sq < 0) break;
                const AircraftType& sa = *TypeOf((*c_->squadrons)[static_cast<std::size_t>(sq)]);
                if (*c_->arm_stock - reserve < sa.arms * n) {
                    why.push_back("no ARMs to spare for SEAD");
                    break;
                }
                reserved[static_cast<std::size_t>(sq)] += n;
                Flight fl;
                fl.callsign = callsign(cs++);
                fl.role = FlightRole::Sead;
                fl.squadron = (*c_->squadrons)[static_cast<std::size_t>(sq)].id;
                fl.aircraft = sa.id;
                fl.count = n;
                fl.profile = sa.helicopter ? Profile::Low : (o.profile == Profile::Low ? Profile::Low : Profile::Medium);
                fl.arms = sa.arms * n;
                fl.route = MainRoute(entry, exit, e.pos, ProfileHeight(sa, fl.profile), o.tot_h - 0.05, sa, ip_dist, false);
                fl.release_index = ReleaseIndex(fl.route);
                fl.aim_key = e.key;
                fl.aim_pos = e.pos;
                p.flights.push_back(fl);
                want -= n;
            }
            if (p.Has(FlightRole::Sead)) {
                why.push_back("SEAD: " + std::to_string(p.Count(FlightRole::Sead)) + " aircraft for " + std::to_string(radar_threats) +
                              " radar threat(s) believed on the route");
            }
        }
        // Escort against the defender's fighters.
        if (t.escort_ratio > 0.0 && front_->friendly_fighters > 0.05) {
            const int want = static_cast<int>(std::round(p.Main() * t.escort_ratio * front_->friendly_fighters * 2.0));
            if (want > 0) {
                const int n = std::min(want, 4);
                const int sq = PickSquadron(FlightRole::Escort, n, Profile::Medium, o.part, w, reserved);
                if (sq >= 0) {
                    const AircraftType& ea = *TypeOf((*c_->squadrons)[static_cast<std::size_t>(sq)]);
                    reserved[static_cast<std::size_t>(sq)] += n;
                    Flight fl;
                    fl.callsign = callsign(cs++);
                    fl.role = FlightRole::Escort;
                    fl.squadron = (*c_->squadrons)[static_cast<std::size_t>(sq)].id;
                    fl.aircraft = ea.id;
                    fl.count = n;
                    fl.profile = Profile::Medium;
                    fl.route = MainRoute(entry, exit, e.pos, ProfileHeight(ea, Profile::Medium), o.tot_h, ea, ip_dist, false);
                    p.flights.push_back(fl);
                }
            }
        }
        // Jammers: stand-off orbit or with the package.
        if (t.jammers > 0 && radar_threats > 0) {
            const int sq = PickSquadron(FlightRole::Jammer, t.jammers, Profile::Medium, o.part, w, reserved);
            if (sq >= 0) {
                const AircraftType& ja = *TypeOf((*c_->squadrons)[static_cast<std::size_t>(sq)]);
                reserved[static_cast<std::size_t>(sq)] += t.jammers;
                Flight fl;
                fl.callsign = callsign(cs++);
                fl.role = FlightRole::Jammer;
                fl.squadron = (*c_->squadrons)[static_cast<std::size_t>(sq)].id;
                fl.aircraft = ja.id;
                fl.count = t.jammers;
                fl.profile = Profile::Medium;
                if (t.escort_jamming) {
                    fl.route = MainRoute(entry, exit, e.pos, ProfileHeight(ja, Profile::Medium), o.tot_h, ja, ip_dist, false);
                } else {
                    fl.route = OrbitRoute(entry, e.pos, ProfileHeight(ja, Profile::Medium), o.tot_h, ja.cruise_mps);
                }
                p.flights.push_back(fl);
                why.push_back(std::string(t.escort_jamming ? "escort" : "stand-off") + " jamming");
            }
        }
        // Decoys ahead of the strike, to make the radars radiate and fire (the Bekaa).
        const int decoys = static_cast<int>(t.decoys * (1.0 + pln.Quirk("decoys")) + 0.5);
        if (decoys > 0 && radar_threats > 0 && mk != MissionKind::Deception) {
            const int sq = PickSquadron(FlightRole::Decoy, decoys, Profile::Medium, o.part, w, reserved);
            if (sq >= 0) {
                const AircraftType& da = *TypeOf((*c_->squadrons)[static_cast<std::size_t>(sq)]);
                reserved[static_cast<std::size_t>(sq)] += decoys;
                Flight fl;
                fl.callsign = callsign(cs++);
                fl.role = FlightRole::Decoy;
                fl.squadron = (*c_->squadrons)[static_cast<std::size_t>(sq)].id;
                fl.aircraft = da.id;
                fl.count = decoys;
                fl.profile = Profile::Medium;
                fl.route = MainRoute(entry, exit, e.pos, ProfileHeight(da, Profile::Medium), o.tot_h - 0.07, da, ip_dist, false);
                p.flights.push_back(fl);
                why.push_back("decoys ahead of the strike to draw the radars on");
            }
        }
        // Additional reconnaissance ahead (mai_tactics_vvs: доразведка).
        if (t.recce_lead > 0 && mk != MissionKind::Recce && mk != MissionKind::Bda) {
            const int sq = PickSquadron(FlightRole::Recce, t.recce_lead, o.profile, o.part, w, reserved);
            if (sq >= 0) {
                const AircraftType& ra = *TypeOf((*c_->squadrons)[static_cast<std::size_t>(sq)]);
                reserved[static_cast<std::size_t>(sq)] += t.recce_lead;
                Flight fl;
                fl.callsign = callsign(cs++);
                fl.role = FlightRole::Recce;
                fl.squadron = (*c_->squadrons)[static_cast<std::size_t>(sq)].id;
                fl.aircraft = ra.id;
                fl.count = t.recce_lead;
                fl.profile = o.profile;
                fl.route = MainRoute(entry, exit, e.pos, ProfileHeight(ra, o.profile), o.tot_h - 0.13, ra, ip_dist, false);
                fl.release_index = ReleaseIndex(fl.route);
                fl.aim_key = e.key;
                fl.aim_pos = e.pos;
                p.flights.push_back(fl);
            }
        }
        // A feint from another entry point (his signature).
        if (pln.Quirk("feint") > 0.0 && front_->entries.size() > 1 && (mk == MissionKind::Strike || mk == MissionKind::Dead)) {
            const int sq = PickSquadron(FlightRole::Strike, 2, Profile::Medium, o.part, w, reserved);
            if (sq >= 0) {
                const AircraftType& fa = *TypeOf((*c_->squadrons)[static_cast<std::size_t>(sq)]);
                reserved[static_cast<std::size_t>(sq)] += 2;
                const std::size_t other = (static_cast<std::size_t>(o.entry) + 1) % front_->entries.size();
                Flight fl;
                fl.callsign = callsign(cs++);
                fl.role = FlightRole::Decoy;
                fl.squadron = (*c_->squadrons)[static_cast<std::size_t>(sq)].id;
                fl.aircraft = fa.id;
                fl.count = 2;
                fl.profile = Profile::Medium;
                const Vec3 short_of = front_->entries[other].pos + (e.pos - front_->entries[other].pos) * 0.75;
                fl.route = MainRoute(front_->entries[other], front_->entries[other], short_of, ProfileHeight(fa, Profile::Medium),
                                     o.tot_h - 0.2, fa, 10000.0, false);
                p.flights.push_back(fl);
                why.push_back("a feint from " + front_->entries[other].id + " first, the planner's signature");
            }
        }
        return true;
    }

    // entry -> IP -> target -> egress -> exit. Times anchored at the target.
    std::vector<RoutePoint> MainRoute(const EntryPoint& entry, const EntryPoint& exit, const Vec3& target, double agl,
                                      double tot_h, const AircraftType& a, double ip_dist, bool helicopter_hide) const {
        std::vector<RoutePoint> r;
        const Vec3 in = target - entry.pos;
        const double len = std::max(1.0, Dist2d(entry.pos, target));
        const Vec3 dir = Vec3{in.x / len, in.y / len, 0.0};
        const Vec3 side{-dir.y, dir.x, 0.0};
        const double ipd = std::min(ip_dist, 0.6 * len);
        RoutePoint p0{entry.pos, agl, 0.0, "entry"};
        RoutePoint ip{target - dir * ipd + side * (0.25 * ipd), helicopter_hide ? 5.0 : agl, 0.0, helicopter_hide ? "hide" : "ip"};
        RoutePoint tg{target, agl, 0.0, "target"};
        const Vec3 out = target + side * 12000.0 + dir * 4000.0;
        RoutePoint eg{out, agl, 0.0, "egress"};
        RoutePoint ex{exit.pos, agl, 0.0, "exit"};
        r = {p0, ip, tg, eg, ex};
        TimeRoute(r, 2, tot_h, helicopter_hide ? a.cruise_mps : a.attack_mps);
        return r;
    }

    // A hunter patrol: in to the area, three loiter points around it, out.
    std::vector<RoutePoint> LoiterRoute(const EntryPoint& entry, const EntryPoint& exit, const Vec3& area, double agl,
                                        double on_station_h, double speed) const {
        std::vector<RoutePoint> r;
        r.push_back({entry.pos, agl, 0.0, "entry"});
        const double rad = 9000.0;
        for (int i = 0; i < 4; ++i) {
            const double a = 1.5707963 * i;
            r.push_back({area + Vec3{std::cos(a) * rad, std::sin(a) * rad, 0.0}, agl, 0.0, i == 0 ? "target" : "loiter"});
        }
        r.push_back({exit.pos, agl, 0.0, "exit"});
        TimeRoute(r, 1, on_station_h, speed);
        return r;
    }

    // A stand-off orbit on the own side of the line, on the axis to the target.
    std::vector<RoutePoint> OrbitRoute(const EntryPoint& entry, const Vec3& target, double agl, double tot_h, double speed) const {
        const Vec3 back = entry.pos - target;
        const double len = std::max(1.0, Dist2d(entry.pos, target));
        const Vec3 dir{back.x / len, back.y / len, 0.0};
        const Vec3 side{-dir.y, dir.x, 0.0};
        const Vec3 c = entry.pos + dir * 5000.0;
        std::vector<RoutePoint> r = {{front_->bases, agl, 0.0, "entry"},
                                     {c - side * 12000.0, agl, 0.0, "orbit"},
                                     {c + side * 12000.0, agl, 0.0, "orbit"},
                                     {c - side * 12000.0, agl, 0.0, "orbit"},
                                     {front_->bases, agl, 0.0, "exit"}};
        TimeRoute(r, 1, tot_h - 0.15, speed);
        return r;
    }

    static int ReleaseIndex(const std::vector<RoutePoint>& r) {
        for (std::size_t i = 0; i < r.size(); ++i) {
            if (r[i].tag == "target") return static_cast<int>(i);
        }
        return -1;
    }
    static double RouteChord(const std::vector<RoutePoint>& r, const Vec3& c, double radius) {
        double s = 0.0;
        for (std::size_t i = 0; i + 1 < r.size(); ++i) s += ChordInCircle2d(r[i].pos, r[i + 1].pos, c, radius);
        return s;
    }

    // Losses the planner expects from the threats he believes in.
    double ExpectedLosses(const Package& p, const IntelEntry& target, double* main_loss) const {
        const TuningTable& tune = Tune();
        const int sead = p.Count(FlightRole::Sead);
        const bool jam = p.Has(FlightRole::Jammer);
        const Weather w = c_->WeatherAt(p.tot_h);
        const Officer* so = SeadOfficer();
        const double sead_skill = so != nullptr ? so->Effective(so->def.skills.sead) : 0.4;
        double total = 0.0, main = 0.0;
        for (const Flight& fl : p.flights) {
            if (fl.role == FlightRole::Jammer && !fl.route.empty() && fl.route.front().tag == "entry" &&
                fl.route[1].tag == "orbit") {
                continue;  // stand-off: out of reach
            }
            const AircraftType* a = c_->data->faction.Aircraft(fl.aircraft);
            if (a == nullptr) continue;
            double per = 0.0;
            for (const IntelEntry* th : c_->intel->Threats()) {
                if (th->threat_max_target_mps > 0.0 && a->cruise_mps > th->threat_max_target_mps) continue;  // too fast for it
                // A threat whose place is uncertain covers a wider ring, but is less likely to be on any one route
                // through it: the lethality is spread over the ring's area.
                const double ring = th->threat_radius_m + 0.5 * th->sigma_m;
                const double spread = (th->threat_radius_m / ring) * (th->threat_radius_m / ring);
                const double chord = RouteChord(fl.route, th->pos, ring);
                const bool optical = !th->threat_radar;
                double env = 1.0;
                if (optical && p.part == DayPart::Night) env *= tune.Get("attrition.night");
                if (optical && w == Weather::Poor) env *= tune.Get("attrition.poor_weather");
                const double supp = sead > 0 ? tune.Get("attrition.sead_suppression") * std::min(1.0, sead / 2.0) * (0.6 + 0.4 * sead_skill) : 0.0;
                const double engaged = fl.count > 0 ? EngagedAircraft(th->threat_channels, th->function, fl.count) / static_cast<double>(fl.count) : 1.0;
                per += th->Live() * spread * engaged *
                       PassKillChance(th->threat_lethality, chord, ring, fl.profile, a->helicopter, th->threat_low, th->threat_medium,
                                      th->threat_radar, supp, jam ? tune.Get("attrition.jamming") : 0.0, env, a->evasion, tune);
            }
            const bool attacks = fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Helicopter;
            if (attacks && !target.IsThreat() && !target.ghost) {
                double env = 1.0;
                if (p.part == DayPart::Night) env *= tune.Get("attrition.night");
                if (w == Weather::Poor) env *= tune.Get("attrition.poor_weather");
                per += Clamp01(target.exists) * TargetGunsChance(target.kind, fl.profile, a->helicopter, a->evasion, env, tune);
            }
            if (a->drone) per = 0.0;  // decoys are meant to be lost
            const double fl_loss = std::min(1.0, per) * fl.count;
            total += fl_loss;
            if (fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Helicopter ||
                fl.role == FlightRole::Hunter || fl.role == FlightRole::Recce) {
                main += fl_loss;
            }
        }
        const double fighters = front_->friendly_fighters * tune.Get("attrition.escort_fighter_kill") *
                                (p.Has(FlightRole::Escort) ? 0.3 : 1.0) * p.Main();
        total += fighters;
        main += fighters;
        if (main_loss != nullptr) *main_loss = main;
        return total;
    }

    bool BestPackage(MissionKind mk, const std::set<std::string>& used, Package& best_out) {
        const AirWarData& d = *c_->data;
        const FactionDef& f = d.faction;
        const PackageTemplate& t = f.doctrine.packages[static_cast<int>(mk)];
        Officer& pln = PlannerOfficer();
        const TuningTable& tune = Tune();
        double best_u = -1e18;
        bool found = false;
        std::vector<std::string> best_why;
        const double noise_sd = tune.Get("utility.noise_at_zero_skill") * (1.0 - pln.Effective(pln.def.skills.planning));
        const double tolerance = std::max(0.3, f.politics.loss_tolerance_per_day);
        const double over = std::max(0.0, c_->politics->losses_today - tolerance) / tolerance;
        const double loss_aversion = (1.5 - pln.def.traits.boldness) * std::sqrt(3.0 / tolerance) * (1.0 + over) *
                                     (1.0 - 0.3 * Clamp01(pln.Quirk("risk")));
        for (const IntelEntry* e : Candidates(mk)) {
            if (used.count(e->key) > 0) continue;
            for (std::size_t ei = 0; ei < front_->entries.size(); ++ei) {
                for (Profile prof : t.profiles) {
                    if (c_->politics->min_profile >= 1 && prof == Profile::Low) continue;
                    for (DayPart part : t.day_parts) {
                        Option o;
                        o.entry = static_cast<int>(ei);
                        o.exit = static_cast<int>(ei);
                        o.profile = prof;
                        o.part = part;
                        if (!NextTot(part, o.tot_h)) continue;
                        Package p;
                        std::vector<std::string> why;
                        if (!Build(mk, *e, o, p, why)) continue;
                        double main_loss = 0.0;
                        const double eloss = ExpectedLosses(p, *e, &main_loss);
                        // Worth and the chance the effort lands on something real and working.
                        std::vector<std::string> vwhy;
                        const double v = ValueOf(*e, mk, &vwhy);
                        const double age = c_->now_h - e->observed_h;
                        const double stale = tune.Get("strike.stale_miss_m");
                        double p_loc = e->mobile ? 1.0 / (1.0 + (e->sigma_m / stale) * (e->sigma_m / stale)) : 1.0;
                        if (mk == MissionKind::Recce || mk == MissionKind::Bda || mk == MissionKind::Hunter ||
                            mk == MissionKind::Deception) {
                            p_loc = 1.0;
                        }
                        const double main_n = std::max(1, p.Main());
                        const double p_deliver = Clamp01(1.0 - main_loss / main_n);
                        double effect = 1.0;
                        const AircraftType* ma = f.Aircraft(p.flights.front().aircraft);
                        if (mk == MissionKind::Strike || mk == MissionKind::Dead || mk == MissionKind::Cas ||
                            mk == MissionKind::HeloRaid || mk == MissionKind::Harassment) {
                            const double hard = HardnessOf(*e);
                            effect = Clamp01(p.Main() * (ma != nullptr ? ma->strike_power : 0.2) * tune.Get("strike.hit_chance") /
                                             (0.5 + hard));
                        }
                        double live = e->Live();
                        if (mk == MissionKind::Recce) live = std::max(0.3, 1.0 - e->exists) + std::min(1.0, age / 24.0);
                        if (mk == MissionKind::Bda) live = 1.0;
                        if (pln.Quirk("elint_trust") > 0.0 && e->source == IntelSource::Elint) live = std::min(1.0, live + 0.3);
                        const double ev = v * live * p_loc * p_deliver * effect;
                        double u = tune.Get("utility.value_weight") * ev - tune.Get("utility.loss_cost") * eloss * loss_aversion;
                        // His habits.
                        const double qs = tune.Get("utility.quirk_scale");
                        const char* pref[] = {"pref_night", "pref_dawn", "pref_day", "pref_dusk"};
                        const double tod = pln.Quirk(pref[static_cast<int>(part)]);
                        u += qs * tod;
                        if (front_->entries[ei].id == pln.last_entry) u += qs * (pln.Quirk("axis_repeat") - pln.Quirk("axis_vary"));
                        if (prof == Profile::Low) u += qs * pln.Quirk("low_level");
                        if (prof == Profile::Medium) u += qs * pln.Quirk("medium_altitude");
                        if (c_->WeatherAt(o.tot_h) == Weather::Poor) u -= qs * (1.0 - Clamp01(pln.Quirk("weather_tolerance")));
                        if (e->mobile && age > 6.0 && (mk == MissionKind::Strike || mk == MissionKind::Dead)) {
                            u -= qs * 2.0 * pln.Quirk("imagery_wait");
                            u -= tune.Get("utility.stale_penalty") * Clamp01((age - 6.0) / 24.0) * (0.5 + pln.def.traits.methodical);
                        }
                        if (e->strikes > 0) u += qs * pln.Quirk("restrike");
                        if (mk == MissionKind::HeloRaid) u += qs * pln.Quirk("helicopter_use");
                        // What he remembers of this way of doing it, and his curiosity about untried ways.
                        const auto mem = pln.tactics.find(p.tactic);
                        int uses = 0;
                        if (mem != pln.tactics.end()) {
                            uses = mem->second.uses;
                            u -= tune.Get("utility.loss_cost") * loss_aversion * mem->second.loss_rate * p.Aircraft() * 0.5;
                            u += tune.Get("utility.value_weight") * 0.15 * (mem->second.success - 0.5);
                        }
                        u += tune.Get("utility.exploration") * pln.def.traits.creativity / std::sqrt(1.0 + uses);
                        u += c_->rng->Normal(0.0, noise_sd);
                        const bool achieves = ev >= tune.Get("utility.min_effect") || mk == MissionKind::Deception ||
                                              mk == MissionKind::Hunter || mk == MissionKind::Recce || mk == MissionKind::Bda;
                        if (u > best_u && achieves) {
                            best_u = u;
                            p.expected_losses = eloss;
                            p.expected_value = ev;
                            p.utility = u;
                            best_out = p;
                            best_why = vwhy;
                            for (const std::string& w : why) best_why.push_back(w);
                            if (tod > 0.15) best_why.push_back(std::string("chose ") + DayPartName(part) + ": the planner's habit");
                            if (front_->entries[ei].id == pln.last_entry && pln.Quirk("axis_repeat") > 0.0) {
                                best_why.push_back("the same way in as last time (" + front_->entries[ei].id + ")");
                            }
                            if (mem != pln.tactics.end() && mem->second.loss_rate > 0.1) {
                                best_why.push_back("remembers losing " + Pct(mem->second.loss_rate) + " this way, but the target is worth it");
                            }
                            if (e->mobile && age > 6.0) {
                                best_why.push_back("last seen " + std::to_string(static_cast<int>(age + 0.5)) + " h ago: it may have moved");
                            }
                            found = true;
                        }
                    }
                }
            }
        }
        if (!found || best_u < tune.Get("utility.min_value")) return false;
        best_out.rationale = best_why;
        best_out.rationale.insert(best_out.rationale.begin(),
                                  "expects to lose " + Num1(best_out.expected_losses) + " of " + std::to_string(best_out.Aircraft()) +
                                      " aircraft for an expected effect of " + Num1(best_out.expected_value));
        return true;
    }

    // The cruise missile type the staff would fire now: the doctrine's first choice in service in the war's year with a
    // salvo in stock. Null: none.
    const AircraftType* MissileType() const {
        const FactionDef& f = c_->data->faction;
        const PackageTemplate& t = f.doctrine.packages[static_cast<int>(MissionKind::Missile)];
        if (!t.allowed || c_->missile_stock == nullptr) return nullptr;
        for (const std::string& id : t.aircraft) {
            const AircraftType* a = f.Aircraft(id);
            if (a == nullptr || !a->missile || !a->InService(c_->data->theatre.year)) continue;
            const auto it = c_->missile_stock->find(id);
            if (it != c_->missile_stock->end() && it->second + 1e-9 >= t.flight_size) return a;
        }
        return nullptr;
    }

    // Where a salvo is launched and how it flies in: a carrier releases it at the border entry ("air"); ships or ground
    // launchers fire it from the front's launch area and it comes in over that entry ("sea"). High: at height to a dive
    // 15 km short, then down onto the target; low: terrain-following all the way.
    std::vector<RoutePoint> MissileRoute(const AircraftType& m, const EntryPoint& entry, const Vec3& target, Profile prof,
                                         double tot_h) const {
        const double agl = ProfileHeight(m, prof);
        std::vector<RoutePoint> r;
        if (m.launch == "sea" && front_->has_missile_launch) r.push_back({front_->missile_launch, agl, 0.0, "launch"});
        r.push_back({entry.pos, agl, 0.0, r.empty() ? "launch" : "entry"});
        const Vec3 d = target - entry.pos;
        const double len = std::max(1.0, Dist2d(target, entry.pos));
        if (prof == Profile::High && len > 20000.0) r.push_back({target - d * (15000.0 / len), agl, 0.0, "dive"});
        r.push_back({target, 10.0, 0.0, "target"});
        TimeRoute(r, r.size() - 1, tot_h, m.cruise_mps);
        return r;
    }

    // A cruise missile salvo (MissionKind::Missile): one flight of missiles at a fixed target within their range, worth
    // its expected effect (the missiles that get through, their warheads, their accuracy) less the missiles spent.
    bool BestMissilePackage(const std::set<std::string>& used, Package& best_out) {
        const AircraftType* mt = MissileType();
        if (mt == nullptr) return false;
        const FactionDef& f = c_->data->faction;
        const PackageTemplate& t = f.doctrine.packages[static_cast<int>(MissionKind::Missile)];
        Officer& pln = PlannerOfficer();
        const TuningTable& tune = Tune();
        const double noise_sd = tune.Get("utility.noise_at_zero_skill") * (1.0 - pln.Effective(pln.def.skills.planning));
        const double left = (*c_->missile_stock)[mt->id];
        const double scarcity = std::sqrt(std::max(1.0, static_cast<double>(mt->stock)) / std::max(1.0, left));
        const double hit = mt->hit_chance > 0.0 ? mt->hit_chance : tune.Get("strike.hit_chance");
        const Profile prof = t.profiles.empty() ? Profile::Low : t.profiles.front();
        double best_u = -1e18;
        bool found = false;
        for (const IntelEntry* e : Candidates(MissionKind::Missile)) {
            if (used.count(e->key) > 0) continue;
            for (std::size_t ei = 0; ei < front_->entries.size(); ++ei) {
                const EntryPoint& entry = front_->entries[ei];
                for (DayPart part : t.day_parts) {
                    double tot_h = 0.0;
                    if (!NextTot(part, tot_h)) continue;
                    Package p;
                    p.kind = MissionKind::Missile;
                    p.target_key = e->key;
                    p.target_object = e->object_id;
                    p.target_label = e->label;
                    p.target_pos = e->pos;
                    p.tot_h = tot_h;
                    p.part = part;
                    p.profile = prof;
                    p.entry = entry.id;
                    p.exit = entry.id;
                    p.tactic = std::string("missile:") + mt->id;
                    Flight fl;
                    fl.callsign = f.callsigns[static_cast<std::size_t>((*c_->next_id * 7) % static_cast<int>(f.callsigns.size()))] + " " +
                                  std::to_string(10 + (*c_->next_id % 9) * 10);
                    fl.role = FlightRole::Missile;
                    fl.aircraft = mt->id;
                    fl.count = t.flight_size;
                    fl.profile = prof;
                    fl.route = MissileRoute(*mt, entry, e->pos, prof, tot_h);
                    double flown = 0.0;
                    for (std::size_t i = 0; i + 1 < fl.route.size(); ++i) flown += Dist2d(fl.route[i].pos, fl.route[i + 1].pos);
                    if (flown > mt->range_m) continue;
                    fl.release_index = ReleaseIndex(fl.route);
                    fl.aim_key = e->key;
                    fl.aim_pos = e->pos;
                    p.flights.push_back(fl);
                    const double lost = std::min(static_cast<double>(fl.count), ExpectedLosses(p, *e, nullptr));
                    const double through = fl.count - lost;
                    const double effect = Clamp01(through * mt->strike_power * hit / (0.5 + HardnessOf(*e)));
                    std::vector<std::string> vwhy;
                    const double ev = ValueOf(*e, MissionKind::Missile, &vwhy) * e->Live() * effect;
                    double u = tune.Get("utility.value_weight") * ev - tune.Get("utility.missile_cost") * scarcity * fl.count;
                    u += c_->rng->Normal(0.0, noise_sd);
                    if (u > best_u && ev >= tune.Get("utility.min_effect")) {
                        best_u = u;
                        p.expected_losses = 0.0;
                        p.expected_value = ev;
                        p.utility = u;
                        p.rationale = vwhy;
                        p.rationale.insert(p.rationale.begin(), "expects " + Num1(through) + " of " + std::to_string(fl.count) + " " +
                                                                    mt->name + " to get through, for an expected effect of " + Num1(ev) +
                                                                    "; no crew at risk");
                        best_out = p;
                        found = true;
                    }
                }
            }
        }
        return found && best_u >= tune.Get("utility.min_value");
    }

    double HardnessOf(const IntelEntry& e) const {
        if (!e.object_id.empty()) {
            if (const TheatreObject* o = c_->data->theatre.Find(e.object_id)) return o->hardness;
        }
        return 0.5;
    }

    bool Approve(Package& p, Officer& cdr) {
        const FactionDef& f = c_->data->faction;
        const double risk = p.kind == MissionKind::Missile ? 0.0 : p.expected_losses / std::max(1, p.Aircraft());
        const double tolerance = std::max(0.3, f.politics.loss_tolerance_per_day);
        const double left = std::max(0.0, tolerance - c_->politics->losses_today) / tolerance;
        double limit = 0.04 + 0.16 * cdr.def.traits.boldness + 0.08 * c_->politics->pressure + 0.06 * cdr.Quirk("risk");
        limit *= 0.5 + 0.5 * left;
        if (risk > limit && !(cdr.def.traits.ambition > 0.7 && c_->politics->pressure > 0.6)) {
            Log("decision", cdr.Title(), "Rejected " + std::string(MissionKindName(p.kind)) + " against " + p.target_label + ": expects to lose " +
                                             Num1(p.expected_losses) + " of " + std::to_string(p.Aircraft()) + " aircraft (" + Pct(risk) +
                                             ", the limit " + Pct(limit) + ").");
            return false;
        }
        if (const Officer* pol = c_->staff->HolderPtr(StaffRole::Political)) {
            if (InNoStrikeZone(p.target_pos) && p.kind != MissionKind::Recce) {
                Log("decision", pol->Title(), "Vetoed the strike on " + p.target_label + ": too close to a town.");
                return false;
            }
        }
        p.commander = c_->staff->Holder(StaffRole::Commander);
        p.planner = c_->staff->Holder(StaffRole::Planner);
        return true;
    }

    void Commit(Package& p) {
        const AirWarData& d = *c_->data;
        Officer& pln = PlannerOfficer();
        p.id = (*c_->next_id)++;
        const Officer* so = SeadOfficer();
        const double sead_skill = so != nullptr ? so->Effective(so->def.skills.sead) : 0.3;
        p.coordination = Clamp01(0.45 + 0.25 * pln.def.traits.methodical + 0.2 * sead_skill + 0.1 * pln.Quirk("punctuality") -
                                 0.2 * pln.stress);
        // Punctuality: centralised doctrines fly to the minute; ragged planners don't.
        const double jitter_h = (1.0 - Clamp01(0.5 + 0.5 * d.faction.doctrine.centralisation + 0.3 * pln.Quirk("punctuality"))) * 0.15;
        const double shift = c_->rng->Normal(0.0, jitter_h);
        for (Flight& fl : p.flights) {
            for (RoutePoint& rp : fl.route) rp.t_h += shift;
            const int lead = c_->staff->LeaderOf(fl.squadron);
            if (lead >= 0 && fl.officer < 0 && p.leader < 0 && c_->staff->officers[static_cast<std::size_t>(lead)].active) {
                if (fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Hunter ||
                    fl.role == FlightRole::Helicopter || fl.role == FlightRole::Sead) {
                    fl.officer = lead;
                    p.leader = lead;
                }
            }
        }
        p.tot_h += shift;
        for (SquadronState& s : *c_->squadrons) {
            for (const Flight& fl : p.flights) {
                if (fl.squadron == s.id) s.sorties_left -= fl.count;
            }
        }
        for (const Flight& fl : p.flights) *c_->arm_stock -= fl.arms;
        if (*c_->arm_stock < 0.0) *c_->arm_stock = 0.0;
        for (const Flight& fl : p.flights) {
            if (fl.role != FlightRole::Missile || c_->missile_stock == nullptr) continue;
            double& left = (*c_->missile_stock)[fl.aircraft];
            left = std::max(0.0, left - fl.count);
        }
        p.name = p.flights.empty() ? std::string("Package") : p.flights.front().callsign;
        pln.last_entry = p.entry;
        std::string comp;
        int n[static_cast<int>(FlightRole::Count)] = {};
        for (const Flight& fl : p.flights) n[static_cast<int>(fl.role)] += fl.count;
        for (int r = 0; r < static_cast<int>(FlightRole::Count); ++r) {
            if (n[r] == 0) continue;
            comp += (comp.empty() ? "" : ", ") + std::to_string(n[r]) + " " + FlightRoleName(static_cast<FlightRole>(r));
        }
        std::string text = std::string(MissionKindName(p.kind)) + " '" + p.name + "' against " + p.target_label + ", " +
                           FormatHour(p.tot_h, d.theatre.start_hour) + " (" + DayPartName(p.part) + "), in at " + p.entry + ", " +
                           ProfileName(p.profile) + ": " + comp + ".";
        for (const std::string& r : p.rationale) text += " " + r + ".";
        Log("plan", pln.Title(), text);
    }

    bool DoubleTap(const Package& first, Package& second) {
        second = first;
        second.id = 0;
        second.rationale = {"a second strike on the same target soon after the first, to catch the repair crews"};
        const double delay = 0.6;
        second.tot_h += delay;
        std::vector<Flight> kept;
        for (Flight fl : second.flights) {
            if (fl.role != FlightRole::Strike && fl.role != FlightRole::Dead) continue;
            fl.count = std::max(1, fl.count / 2);
            for (RoutePoint& rp : fl.route) rp.t_h += delay;
            fl.officer = -1;
            kept.push_back(fl);
            break;
        }
        if (kept.empty()) return false;
        for (const SquadronState& s : *c_->squadrons) {
            if (s.id == kept.front().squadron && s.sorties_left + 1e-9 < kept.front().count) return false;
        }
        second.flights = kept;
        second.leader = -1;
        second.expected_losses = first.expected_losses * 0.5;
        return true;
    }

    void Interference(Officer& cdr) {
        if (out_.empty()) return;
        const PoliticsDef& pol = c_->data->faction.politics;
        if (!c_->rng->Chance(pol.interference_per_cycle * (1.0 - 0.5 * cdr.Effective(cdr.def.skills.politics)))) return;
        // The most visible strike.
        int best = -1;
        for (std::size_t i = 0; i < out_.size(); ++i) {
            if (out_[i].kind != MissionKind::Strike && out_[i].kind != MissionKind::Dead) continue;
            if (best < 0 || out_[i].expected_value > out_[static_cast<std::size_t>(best)].expected_value) best = static_cast<int>(i);
        }
        if (best < 0) return;
        Package& p = out_[static_cast<std::size_t>(best)];
        const double earlier = 1.0 + c_->rng->Uniform() * 2.0;
        const double min_t = c_->now_h + 1.0;
        const double shift = std::max(-earlier, min_t - p.FirstH());
        for (Flight& fl : p.flights) {
            for (RoutePoint& rp : fl.route) rp.t_h += shift;
        }
        p.tot_h += shift;
        p.part = DayPartAt(c_->data->theatre.start_hour + p.tot_h);
        p.interference = true;
        p.coordination *= 0.5;
        p.load_factor = 0.6;
        p.rationale.push_back("higher authority moved the time on target " + Num1(-shift) +
                              " h earlier over the commander's objection: loads wrong or partial, crews under-briefed, no time for deception");
        Log("politics", cdr.Title(), "Higher authority has moved '" + p.name + "' " + Num1(-shift) + " h earlier. The commander objected and was overruled.");
        cdr.stress = Clamp01(cdr.stress + 0.08);
    }
};

}  // namespace airwar
}  // namespace simcore
