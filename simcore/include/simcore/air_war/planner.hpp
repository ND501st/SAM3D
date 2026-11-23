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
#include <cstdint>
#include <map>
#include <unordered_map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/air_fight.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/front_line.hpp"
#include "simcore/air_war/intel.hpp"
#include "simcore/air_war/logistics.hpp"
#include "simcore/air_war/officers.hpp"
#include "simcore/air_war/route_planner.hpp"
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
    // The support orbits (docs/air_war/11_SUPPORT.md): the share of the day an AWACS is on station, and the aircraft the
    // tankers can still refuel this cycle (null: no tankers counted, every reach unlimited by them).
    double awacs = 0.0;
    double* tanker_receivers = nullptr;
    // The player's side's supply lines (logistics.hpp): the roads and railways are on every map, so the enemy knows where
    // its armed reconnaissance must search; never where the columns are (null: no armed reconnaissance).
    const SupplyNetwork* supply = nullptr;
    // The map's features and ground for the route planner (route.mode 1, route_planner.hpp; null: the planner builds its
    // own from the data).
    const RouteGround* ground = nullptr;
    // The front's line of contact (track F, front_line.hpp): what the target filter's depths are measured from (null: the
    // front's base line; the same at zero offsets).
    const LineOfContact* front_line = nullptr;
    // The enemy's air front by sector (track F, F2.4; the campaign's FrontView()): close support, harassment and the
    // battlefield interdiction's nomination reach its fixed-wing depth further, helicopter raids its helicopters' depth,
    // the Pact's helicopter umbrella and the routes' let-down move with it. Off (air_front.mode 0): every depth 0.
    AirFrontView air_front;
    // Our fighters by sector as the enemy's staff believes them (track F, F4.3; the campaign's BelievedFighterView()):
    // the escort's need, the expected losses to our fighters and the routes' fighter threats at a point. Off: the front's
    // number everywhere, as before.
    FighterCoverView fighters;

    // The defender's fighter threat over the front, less what an AWACS on station takes away (warning, vectoring): only
    // an AWACS that controls fighters in the war's year (roadmap B8: the Pact's A-50 not before 1989).
    double FriendlyFighters(const FrontDef& f, const Vec3& at) const {
        const double up = AwacsControlsFighters(*data) ? Clamp01(awacs) : 0.0;
        return fighters.At(at, f.friendly_fighters) * (1.0 - data->tuning.Get("support.awacs_fighter_cut") * up);
    }

    Weather WeatherAt(double h) const {
        const double abs_h = data->theatre.start_hour + h;
        const int d = static_cast<int>(std::floor(abs_h / 24.0));
        if (weather.empty()) return Weather::Clear;
        return weather[static_cast<std::size_t>(std::min(std::max(d, 0), static_cast<int>(weather.size()) - 1))];
    }
};

// Whether a template's medium profile is offered now (10_FACTIONS.md §9.1): with no line, always; with one, only once the
// believed working air defences are below it (hot_phase.mode 1; a save made before roadmap B11 has no gate).
inline bool MediumOffered(const PackageTemplate& t, double believed_iads, int hot_mode) {
    return hot_mode < 1 || t.medium_below_iads < 0.0 || believed_iads < t.medium_below_iads;
}

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
                             double evasion, const TuningTable& tune, bool masked_chord = false) {
    if (chord_m <= 0.0 || radius_m <= 0.0) return 0.0;
    const double exposure = std::min(1.0, chord_m / (2.0 * radius_m));
    double alt = 1.0;
    const bool long_range = radius_m > 15000.0;
    const Profile p = helicopter ? Profile::Low : profile;
    if (p == Profile::Low) {
        // A chord the ground masked (route.mode 1 on a map) is already what the site sees of a low flight.
        alt = low_cover ? ((long_range && !masked_chord) ? tune.Get("attrition.low_vs_long_range_sam") : 1.0) : 0.0;
    } else if (p == Profile::Medium) {
        alt = medium_cover ? 1.0 : tune.Get("attrition.medium_vs_guns");
    } else {
        alt = (medium_cover && long_range) ? 1.0 : tune.Get("attrition.high_vs_short_range");
    }
    double k = lethality * tune.Get("attrition.pass_scale") * exposure * alt;
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
// the chance an aircraft attacking it is shot down on the attack, by how much the attack exposes it (`exposure`: 1 for
// a low pass over the target, 0 for an attack from above their reach or from stand-off: DeliveryExposure). Troops carry
// the most (their own anti-aircraft guns and missiles); towns none (Dugman 5: most aircraft were hit by guns at the low
// approach height, wiki_model5; Desert Storm: AAA and infrared missiles took 25 of 39, rjlee_ds_losses). Shared by the
// planner and the resolver.
inline double TargetGunsChance(ObjectKind k, double exposure, double evasion, double env, const TuningTable& tune) {
    if (exposure <= 0.0 || k == ObjectKind::City) return 0.0;
    const double g = k == ObjectKind::Troops ? tune.Get("attrition.target_guns_troops") : tune.Get("attrition.target_guns_site");
    return std::min(tune.Get("attrition.max_per_pass"), g * exposure * env * (1.0 - 0.5 * evasion));
}

// ---- Weaponeering (docs/air_war/10_WEAPONS.md): which weapon and attack profile, how accurate, what it does.

// How exposed an attack leaves the attacker to the target's own guns and missiles (game values "delivery.exposure_*").
inline double DeliveryExposure(Delivery d, const TuningTable& tune) {
    if (d == Delivery::Count) return 0.0;
    return tune.Get(std::string("delivery.exposure_") + DeliveryName(d));
}

// A flight's exposure at the target: its delivery's, or (a flight with no attack profile) a low pass or none.
inline double FlightExposure(const Flight& fl, bool helicopter, const TuningTable& tune) {
    if (fl.delivery == Delivery::Count) return helicopter || fl.profile == Profile::Low ? 1.0 : 0.0;
    return DeliveryExposure(fl.delivery, tune);
}

// How far short of the target the weapons go and the flight turns away (0: over the target).
inline double ReleaseShort(Delivery d, const WeaponDef* w, const TuningTable& tune) {
    if (d == Delivery::Count) return 0.0;
    if (d == Delivery::Standoff || d == Delivery::HoverFire) {
        const double own = w != nullptr ? w->standoff_m : 0.0;
        return own > 0.0 ? own : tune.Get(std::string("delivery.release_") + DeliveryName(d) + "_m");
    }
    return tune.Get(std::string("delivery.release_") + DeliveryName(d) + "_m");
}

// Whether this aircraft can put this weapon on a target this way, flying `profile` at this time and in this weather:
// the weapon's deliveries and year; low deliveries from low level, the others from medium altitude and above; a
// helicopter hovers or pops up, a jet never hovers; laser guidance needs a designator, a clear enough sky and, at night,
// a night designator; TV needs daylight; a visual attack needs to see the ground.
inline bool CanDeliver(const WeaponDef& w, Delivery d, const AircraftType& a, Profile profile, DayPart part, Weather weather, int year) {
    if (!w.InService(year) || !w.Can(d)) return false;
    if (a.helicopter && d != Delivery::HoverFire && d != Delivery::PopUp) return false;
    if (!a.helicopter && d == Delivery::HoverFire) return false;
    if (d != Delivery::Standoff) {
        if (DeliveryIsLow(d) != (profile == Profile::Low)) return false;
    }
    const bool night = part == DayPart::Night;
    if (w.guidance == "laser" && (a.designator != "laser" || weather == Weather::Poor || (night && !a.night))) return false;
    if (w.guidance == "tv" && (night || weather == Weather::Poor)) return false;
    if (w.guidance == "ir" && weather == Weather::Poor) return false;
    if (w.guidance == "wire" && night && !a.night) return false;
    const bool visual = d == Delivery::PopUp || d == Delivery::Dive || d == Delivery::HoverFire || d == Delivery::Strafe || d == Delivery::GentleDive;
    if (visual && !w.Guided() && weather == Weather::Poor) return false;
    if (d == Delivery::Level && weather == Weather::Poor && !a.all_weather) return false;
    return true;
}

// The circular error probable (m) of a weapon delivered so: its guidance's, or the delivery's for an unguided one
// (worse with a poor sight and at night by eye), and the crew's airmanship.
inline double DeliveryCep(const WeaponDef& w, Delivery d, const AircraftType& a, DayPart part, Weather weather, double airmanship,
                          const TuningTable& tune) {
    double cep = 0.0;
    if (w.guidance == "laser") {
        cep = tune.Get("delivery.cep_laser_m");
    } else if (w.guidance == "tv" || w.guidance == "ir") {
        cep = tune.Get("delivery.cep_tv_m");
    } else if (w.guidance == "wire") {
        cep = tune.Get("delivery.cep_wire_m");
    } else {
        cep = tune.Get(std::string("delivery.cep_") + DeliveryName(d) + "_m") * (1.4 - 0.8 * Clamp01(a.computing));
        const bool visual = d == Delivery::PopUp || d == Delivery::Dive || d == Delivery::HoverFire || d == Delivery::Strafe || d == Delivery::GentleDive;
        if (visual && part == DayPart::Night && !a.night) cep *= tune.Get("delivery.night_visual_factor");
        if (weather == Weather::Poor) cep *= tune.Get("delivery.poor_weather_factor");
    }
    return cep * (1.25 - 0.5 * Clamp01(airmanship));
}

// The chance one weapon puts an aim point of this class out of action: it falls within its footprint (circular normal
// error with this CEP), it goes off, and it does it (the game values of weapons.json). A bomb's footprint grows against
// soft and light targets (delivery.footprint_soft, footprint_light); a runway is cut by a crater anywhere across its
// width: there the footprint is at least the strip's (delivery.runway_strip_m).
inline double WeaponKill(const WeaponDef& w, Delivery d, TargetClass c, double cep, const TuningTable& tune) {
    double footprint = w.footprint_m;
    // A bomb's blast and fragments reach further against people, trucks and stores than against armour or concrete.
    const bool blast = w.kind == "bomb" || w.kind == "retarded_bomb" || w.kind == "guided_bomb";
    if (blast && c == TargetClass::Soft) footprint *= tune.Get("delivery.footprint_soft");
    if (blast && c == TargetClass::Light) footprint *= tune.Get("delivery.footprint_light");
    if (c == TargetClass::Runway) footprint = std::max(footprint, tune.Get("delivery.runway_strip_m"));
    const double r = footprint / std::max(0.1, cep);
    const double inside = 1.0 - std::pow(0.5, r * r);
    const double rel = d == Delivery::Laydown && w.laydown_reliability >= 0.0 ? w.laydown_reliability : w.reliability;
    return inside * rel * w.effect[static_cast<int>(c)];
}

// The expected share of a target's aim points `weapons` such weapons put out of action, spread over its aim points.
inline double ExpectedShare(double kill, double weapons, int aim_points) {
    if (aim_points <= 0 || weapons <= 0.0) return 0.0;
    return 1.0 - std::pow(1.0 - Clamp01(kill), weapons / aim_points);
}

// The planner's choice of weapon and attack for one aircraft type against a target (the weaponeering of the tasking
// cycle, usaf_ato_doctrine): of what it can carry and deliver now, the most effect for the exposure the doctrine
// accepts at the target (`risk`: 0 effect alone .. 1 avoid the target's guns at almost any cost).
struct Weaponeering {
    const WeaponDef* weapon = nullptr;
    int count = 0;  // on each aircraft
    Delivery delivery = Delivery::Count;
    double kill = 0.0;      // one weapon on one aim point
    double exposure = 0.0;  // at the target
    double score = 0.0;
};

inline Weaponeering Weaponeer(const AirWarData& d, const AircraftType& a, TargetClass c, int aim_points, Profile profile, DayPart part,
                              Weather weather, double airmanship, double risk) {
    Weaponeering best;
    const TuningTable& tune = d.tuning;
    for (const auto& load : a.loads) {
        const WeaponDef* w = d.Weapon(load.first);
        if (w == nullptr) continue;
        for (int di = 0; di < kDeliveries; ++di) {
            const Delivery dl = static_cast<Delivery>(di);
            if (!CanDeliver(*w, dl, a, profile, part, weather, d.theatre.year)) continue;
            const double cep = DeliveryCep(*w, dl, a, part, weather, airmanship, tune);
            double kill = WeaponKill(*w, dl, c, cep, tune);
            if (w->guidance == "laser" && weather == Weather::Cloudy) kill *= tune.Get("delivery.cloudy_laser");
            const double share = ExpectedShare(kill, load.second, aim_points);
            const double exposure = DeliveryExposure(dl, tune);
            const double score = share * (1.0 - Clamp01(risk) * exposure);
            if (score > best.score + 1e-12) {
                best.weapon = w;
                best.count = load.second;
                best.delivery = dl;
                best.kill = kill;
                best.exposure = exposure;
                best.score = score;
            }
        }
    }
    return best;
}

// A target in the battlefield interdiction area learned during the war waits for the corps' nomination cycle: it is
// flown no sooner than the faction's nomination_h after the enemy learned of it (gallant_knight_1983: 48 h for NATO's BAI;
// roadmap S2, c2.live_mode 1). What it knew before the war is on the plan already. `to_front_m`: the target's distance
// from the line of contact; `air_front_m`: its sector's air front, which carries the interdiction area forward with it
// (track F, F2.4; 0 at air_front.mode 0). `bai_depth_m`: the area's depth there where the enemy's corps draws an FSCL
// (F3.7, AirFrontView::BaiDepth), < 0 c2.bai_depth_m.
inline bool NominationWaits(const AirWarData& d, const IntelEntry& e, double to_front_m, double now_h, double air_front_m = 0.0,
                            double bai_depth_m = -1.0) {
    const FactionC2& c2 = d.faction.c2;
    if (!c2.present || c2.nomination_h <= 0.0 || d.tuning.Get("c2.mode") < 1.0 || d.tuning.Get("c2.live_mode") < 1.0) return false;
    if (to_front_m > (bai_depth_m >= 0.0 ? bai_depth_m : d.tuning.Get("c2.bai_depth_m")) + air_front_m || e.first_h <= 0.0) return false;
    return now_h < e.first_h + c2.nomination_h;
}

// Whether a helicopter target lies beyond the Pact's umbrella (its own air defence's cover): further than `umbrella_m`
// from the entry point, less its sector's fixed-wing air front, which carries the cover forward (track F, F2.4; 0 off).
inline bool BeyondHeloUmbrella(const Vec3& target, const Vec3& entry, const AirFrontView& af, double umbrella_m) {
    return Dist2d(target, entry) - af.At(target) > umbrella_m;
}

class Planner {
public:
    std::vector<Package> Plan(PlanContext& c) {
        c_ = &c;
        out_.clear();
        // The route planner's caches and book are this cycle's (route.mode 1).
        legs_.clear();
        routes_.clear();
        route_threats_.clear();
        chords_.clear();
        gate_book_.clear();
        const AirWarData& d = *c.data;
        const FactionDef& f = d.faction;
        Officer* cdr = c.staff->HolderPtr(StaffRole::Commander);
        Officer* pln = c.staff->HolderPtr(StaffRole::Planner);
        if (cdr == nullptr || pln == nullptr) return out_;
        front_ = d.theatre.FrontFor(f.front);
        if (front_ == nullptr) return out_;
        if (Mode() >= 1) {
            Ground();
            SetRules();
        }
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
            double s = phase.Apportion(HotMode())[k];
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

        // 2-4. Mission kinds in the order the staff fills them (a light air arm's nuisance raids before its strikes: the
        // same few aircraft fly both).
        const MissionKind order[] = {MissionKind::Missile, MissionKind::Dead,       MissionKind::Hunter,
                                     MissionKind::Deception, MissionKind::Harassment, MissionKind::Strike,
                                     MissionKind::Recce,   MissionKind::Cas,        MissionKind::ArmedRecce,
                                     MissionKind::HeloRaid, MissionKind::Bda};
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

    // What the target filter admits for a mission kind now, before any worth is weighed: the kind's depths from the line
    // of contact (and its sector's air front, F2.4), the politics' and the no-strike zones (for tests and tools).
    std::vector<const IntelEntry*> CandidatesFor(PlanContext& c, MissionKind mk) {
        c_ = &c;
        front_ = c.data->theatre.FrontFor(c.data->faction.front);
        if (front_ == nullptr) return {};
        return Candidates(mk);
    }

    // An on-call package from an alert pool (roadmap B5, docs/air_battle/05_C2_AND_RETASKING.md §4.4): the main flights
    // of `mk` from the pool's squadron only (its `aircraft`, nothing else of the cycle's), against `target` as the report
    // has it (its place, sigma and age), in at the front's entry nearest it, its first aircraft up at `takeoff_h` (the
    // alert airfield behind that entry). Its expected loss is the planner's estimate; the cycle's books are not touched
    // (the pool's sorties were held back from them). False if the squadron can't man it.
    bool OnCall(PlanContext& c, MissionKind mk, const IntelEntry& target, const std::string& squadron, int aircraft, double takeoff_h,
                Package& out) {
        c_ = &c;
        out_.clear();
        const AirWarData& d = *c.data;
        const FactionDef& f = d.faction;
        front_ = d.theatre.FrontFor(f.front);
        if (front_ == nullptr || front_->entries.empty() || c.staff->HolderPtr(StaffRole::Planner) == nullptr) return false;
        // The route planner's caches are this call's (route.mode 1, roadmap B2): the pool flies a route like the plan's.
        legs_.clear();
        routes_.clear();
        route_threats_.clear();
        chords_.clear();
        if (Mode() >= 1) {
            Ground();
            SetRules();
        }
        const PackageTemplate& t = f.doctrine.packages[static_cast<int>(mk)];
        Option o;
        double best = 1e18;
        for (std::size_t i = 0; i < front_->entries.size(); ++i) {
            const double dist = Dist2d(front_->entries[i].pos, target.pos);
            if (dist < best) {
                best = dist;
                o.entry = o.exit = static_cast<int>(i);
            }
        }
        o.profile = t.profiles.empty() ? Profile::Low : t.profiles.front();
        o.tot_h = takeoff_h + 0.5;
        o.part = TheatrePartAt(d.theatre, d.theatre.start_hour + o.tot_h);
        o.size = 0.01;  // one main flight
        std::vector<double> saved;
        for (SquadronState& s : *c.squadrons) {
            saved.push_back(s.sorties_left);
            s.sorties_left = s.id == squadron ? static_cast<double>(aircraft) : 0.0;
        }
        std::vector<std::string> why;
        Package p;
        const bool ok = Build(mk, target, o, p, why);
        for (std::size_t i = 0; i < saved.size(); ++i) (*c.squadrons)[i].sorties_left = saved[i];
        if (!ok || p.flights.empty()) return false;
        const double shift = takeoff_h - p.FirstH();
        for (Flight& fl : p.flights) {
            for (RoutePoint& rp : fl.route) rp.t_h += shift;
        }
        p.tot_h += shift;
        p.part = TheatrePartAt(d.theatre, d.theatre.start_hour + p.tot_h);
        p.expected_losses = ExpectedLosses(p, target, nullptr);
        p.rationale = {"on call from the alert"};
        p.commander = c.staff->Holder(StaffRole::Commander);
        p.planner = c.staff->Holder(StaffRole::Planner);
        p.id = (*c.next_id)++;
        const Officer* so = SeadOfficer();
        p.coordination = Clamp01(0.45 + 0.25 * PlannerOfficer().def.traits.methodical + 0.2 * (so != nullptr ? so->Effective(so->def.skills.sead) : 0.3));
        const int lead = c.staff->LeaderOf(squadron);
        if (lead >= 0 && c.staff->officers[static_cast<std::size_t>(lead)].active) {
            p.flights.front().officer = lead;
            p.leader = lead;
        }
        p.name = p.flights.front().callsign;
        out = p;
        return true;
    }

private:
    PlanContext* c_ = nullptr;
    const FrontDef* front_ = nullptr;
    std::vector<Package> out_;
    // The route planner (route.mode 1, docs/air_battle/01_ROUTES.md): its ground, the planner officer's rules (the
    // faction's, bent by his character), and this cycle's caches (a cache never changes what is planned) and gate book.
    std::shared_ptr<RouteGround> own_ground_;
    mutable RouteRules rules_;
    mutable LegCache legs_;
    mutable std::map<std::string, PlannedRoute> routes_;
    mutable std::map<std::string, std::vector<RouteThreat>> route_threats_;
    mutable std::unordered_map<std::pair<std::uint64_t, std::uint64_t>, double, RouteKeyHash> chords_;
    std::map<std::string, std::vector<double>> gate_book_;

    int Mode() const { return static_cast<int>(Tune().Get("route.mode")); }
    // roadmap B11 (10_FACTIONS.md §9): 1 the phases' new shares and medium altitude gated by the believed defences; 0 a
    // save made before it.
    int HotMode() const { return static_cast<int>(Tune().Get("hot_phase.mode")); }
    const RouteGround& Ground() {
        if (c_->ground != nullptr) return *c_->ground;
        if (!own_ground_) {
            own_ground_ = std::make_shared<RouteGround>();
            own_ground_->Build(*c_->data);
        }
        return *own_ground_;
    }
    const RouteGround& GroundC() const { return c_->ground != nullptr ? *c_->ground : *own_ground_; }
    // The faction's routing rules as this planner applies them: the methodical weigh exposure more, the bold less.
    void SetRules() {
        const Officer& pln = *c_->staff->HolderPtr(StaffRole::Planner);
        rules_ = c_->data->faction.doctrine.routing;
        rules_.w_exp *= std::max(0.2, 1.0 + 0.4 * (pln.def.traits.methodical - 0.5) - 0.4 * (pln.def.traits.boldness - 0.5));
    }
    // The believed threats as a flight of this type and profile prices them (this cycle's, cached).
    const std::vector<RouteThreat>& RouteThreatsFor(const AircraftType& a, Profile prof) const {
        const std::string k = a.id + "|" + std::to_string(static_cast<int>(prof));
        const auto it = route_threats_.find(k);
        if (it != route_threats_.end()) return it->second;
        std::vector<RouteThreat> t = BelievedRouteThreats(c_->intel->Threats(), a, prof, GroundC().HasGround(), Tune());
        if (Mode() >= 2 && front_ != nullptr) {
            // Our fighters as it believes them (roadmap S7): a cost on the legs near our believed air bases.
            std::vector<RouteThreat> f = BelievedFighterThreats(c_->intel->entries, front_->friendly_fighters, a, prof, Tune(), c_->fighters);
            t.insert(t.end(), f.begin(), f.end());
        }
        return route_threats_[k] = std::move(t);
    }
    // One flight's route by the route planner (cached by the request for this cycle).
    PlannedRoute RouteFor(const AircraftType& a, Profile prof, DayPart part, const EntryPoint& gate, const std::vector<const EntryPoint*>& exits,
                          const Vec3& target, double release_short, Delivery dl, int variant, double ip_dist, bool hide, bool loiter,
                          const std::vector<std::string>& avoid) const {
        char buf[160];
        // The part of the day matters to a route only by the height it flies (a night on terrain-following radar).
        std::snprintf(buf, sizeof(buf), "|%d|%.0f|%.0f|%.0f|%.0f|%d|%d|%.0f|%d|%d|", static_cast<int>(prof), std::round(RouteProfileAgl(a, prof, part, Tune())),
                      std::round(target.x), std::round(target.y), std::round(release_short), static_cast<int>(dl), variant,
                      std::round(ip_dist), hide ? 1 : 0, loiter ? 1 : 0);
        std::string key = a.id + "|" + gate.id + buf;
        for (const EntryPoint* x : exits) key += x->id + ",";
        key += "|";
        for (const std::string& v : avoid) key += v + ",";
        const auto it = routes_.find(key);
        if (it != routes_.end()) return it->second;
        RouteRequest rq;
        rq.bases = front_->bases;
        rq.gate_in = &gate;
        rq.exits = exits;
        rq.target = target;
        rq.release_short_m = release_short;
        rq.delivery = dl;
        rq.type = &a;
        rq.profile = prof;
        rq.part = part;
        rq.variant = variant;
        rq.ip_dist_m = ip_dist;
        rq.helicopter_hide = hide;
        rq.loiter = loiter;
        rq.avoid = avoid;
        rq.air_front = c_->air_front;
        const RoutePlanner planner(GroundC(), Tune(), rules_);
        return routes_[key] = planner.Plan(rq, RouteThreatsFor(a, prof), &legs_);
    }
    // The exits a route may come home by (the faction's "return"): its own gate, the others, or any.
    std::vector<const EntryPoint*> ExitsFor(const EntryPoint& entry) const {
        std::vector<const EntryPoint*> out;
        if (rules_.ret == "same_entry" || front_->entries.size() < 2) return {&entry};
        for (const EntryPoint& x : front_->entries) {
            if (rules_.ret == "other_gate" && x.id == entry.id) continue;
            out.push_back(&x);
        }
        return out;
    }
    // A flight following the main flights' route at its own height and speeds (escort, SEAD, decoys, reconnaissance):
    // the attack's own points left out, the release point kept as its anchor.
    std::vector<RoutePoint> FollowRoute(const PlannedRoute& pr, const AircraftType& a, Profile prof, DayPart part, double anchor_h) const {
        std::vector<RoutePoint> r;
        const double agl = RouteProfileAgl(a, prof, part, Tune());
        const double transit = a.helicopter ? agl : std::max(agl, a.medium_alt_m);
        const double mps = a.helicopter ? a.cruise_mps : (prof == Profile::Low ? RouteLowMps(a) : a.cruise_mps);
        // High from the entry to the let-down and from the climb on: inside the air front (F2.4) the let-down is past the
        // gate and the climb short of it, so the gate and the turns between are flown high too.
        bool before_letdown = RouteIndexOf(pr.points, "letdown") >= 0, after_climb = false;
        for (const RoutePoint& q : pr.points) {
            if (q.tag == "pull_up" || q.tag == "apex" || q.tag == "roll_in") continue;
            if (q.tag == "letdown") before_letdown = false;
            if (q.tag == "climb") after_climb = true;
            RoutePoint x = q;
            const bool high = q.tag == "entry" || q.tag == "climb" || q.tag == "exit" || before_letdown || after_climb;
            x.agl_m = high ? transit : agl;
            x.leg_mps = q.tag == "entry" || q.tag == "climb"         ? a.cruise_mps
                        : (q.tag == "target" || q.tag == "egress") ? std::max(mps, a.attack_mps)
                        : (high && q.tag != "exit")                ? a.cruise_mps
                                                                   : mps;
            if (q.tag == "hide") x.tag = "ip";
            r.push_back(x);
        }
        TimeRoute(r, anchor_h);
        return r;
    }
    // A flight's distance out to its farthest point and back, from its bases (helicopters: from their holding area).
    double RadiusNeeded(const Flight& fl, const AircraftType& a) const {
        if (fl.route.empty()) return 0.0;
        double total = RouteLength(fl.route);
        if (!a.helicopter) total += Dist2d(front_->bases, fl.route.front().pos) + Dist2d(fl.route.back().pos, front_->bases);
        return 0.5 * total;
    }
    // The gate book (route.mode 1): a crossing through a gate at least gate_separation_s from every other this cycle;
    // returns the shift (h) that frees the nearest slot.
    double GateShift(const std::string& gate, double gate_h) const {
        const double sep = rules_.gate_separation_s / 3600.0;
        const auto it = gate_book_.find(gate);
        if (sep <= 0.0 || it == gate_book_.end()) return 0.0;
        for (int k = 0; k < 40; ++k) {
            const double shift = (k % 2 == 0 ? 1.0 : -1.0) * sep * ((k + 1) / 2);
            bool free = true;
            for (double t : it->second) {
                if (std::fabs(gate_h + shift - t) < sep - 1e-9) free = false;
            }
            if (free) return shift;
        }
        return 0.0;
    }


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

    // The least a package must achieve and be worth: the doctrine's, or the tuning's.
    double MinEffect() const {
        const DoctrineDef& doc = c_->data->faction.doctrine;
        return doc.has_min_effect ? doc.min_effect : Tune().Get("utility.min_effect");
    }
    double MinValue() const {
        const DoctrineDef& doc = c_->data->faction.doctrine;
        return doc.has_min_value ? doc.min_value : Tune().Get("utility.min_value");
    }

    // Candidates for a mission kind from the intelligence picture.
    std::vector<const IntelEntry*> Candidates(MissionKind mk) const {
        std::vector<const IntelEntry*> out;
        const double now = c_->now_h;
        const bool air_front_on = c_->air_front.On();
        for (const IntelEntry& e : c_->intel->entries) {
            // An armed reconnaissance searches the road whatever the last sighting says.
            if (e.Live() < 0.15 && mk != MissionKind::Recce && mk != MissionKind::Bda && mk != MissionKind::ArmedRecce) continue;
            // A column on the move is found by searching its road, not struck where it was seen.
            if (IsColumn(e.kind) && mk != MissionKind::ArmedRecce) continue;
            const double to_front = c_->front_line != nullptr ? c_->front_line->AbsDistance(e.pos) : DistanceToLine(front_->line, e.pos);
            const bool ad = IsAirDefence(e.kind) || e.kind == ObjectKind::EwRadar;
            // The low-level work's depths reach the target's sector's air front further (F2.4, 02_AIR_FRONT.md §4): its
            // sector looked up only for the kinds that read it, and only with the air front on.
            double front_m = 0.0;
            if (air_front_on && (mk == MissionKind::Strike || mk == MissionKind::Cas || mk == MissionKind::Harassment || mk == MissionKind::HeloRaid)) {
                const int s = c_->air_front.line->SectorOf(e.pos);
                front_m = mk == MissionKind::HeloRaid ? c_->air_front.HeloIn(s) : c_->air_front.DepthIn(s);
            }
            switch (mk) {
                case MissionKind::Strike:
                    if (ad || e.kind == ObjectKind::Troops || e.kind == ObjectKind::City) continue;
                    if (NominationWaits(*c_->data, e, to_front, now, front_m, c_->air_front.BaiDepth(e.pos, -1.0))) continue;
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
                    if (e.kind != ObjectKind::Troops || to_front > 40000.0 + front_m) continue;
                    break;
                case MissionKind::HeloRaid:
                    if (to_front > 25000.0 + front_m || e.kind == ObjectKind::City || e.kind == ObjectKind::Airfield) continue;
                    break;
                case MissionKind::Harassment:
                    if (to_front > 70000.0 + front_m || e.kind == ObjectKind::City) continue;
                    break;
                case MissionKind::Missile:
                    // Fixed targets only: a missile flies to where it was told, and finds nothing that has moved.
                    if (e.mobile || e.kind == ObjectKind::Troops || e.kind == ObjectKind::City || IsAirDefence(e.kind)) continue;
                    break;
                case MissionKind::ArmedRecce:
                    if (!IsColumn(e.kind) || c_->supply == nullptr || LineOf(e) < 0) continue;
                    break;
                case MissionKind::Count: continue;
            }
            if (c_->politics->Banned(e.kind)) continue;
            // The politics' depth is from the border, whatever the line of contact (§4; the same while it has not moved).
            const double to_border = c_->front_line != nullptr && c_->front_line->Moved() ? DistanceToLine(c_->front_line->BaseLine(), e.pos) : to_front;
            if (c_->politics->max_depth_m > 0.0 && to_border > c_->politics->max_depth_m) continue;
            if (InNoStrikeZone(e.pos) && mk != MissionKind::Recce && mk != MissionKind::Bda) continue;
            out.push_back(&e);
        }
        if (mk == MissionKind::ArmedRecce) {
            // One search a line: its freshest sighting stands for it.
            std::stable_sort(out.begin(), out.end(), [](const IntelEntry* a, const IntelEntry* b) { return a->observed_h > b->observed_h; });
            std::vector<const IntelEntry*> one;
            std::vector<int> lines;
            for (const IntelEntry* e : out) {
                const int li = LineOf(*e);
                if (std::find(lines.begin(), lines.end(), li) != lines.end()) continue;
                lines.push_back(li);
                one.push_back(e);
            }
            out.swap(one);
        }
        return out;
    }

    // The supply line a sighting is of (a column's), or -1.
    int LineOf(const IntelEntry& e) const {
        const TheatreObject* o = e.object_id.empty() ? nullptr : c_->data->theatre.Find(e.object_id);
        return o != nullptr ? o->supply_line : -1;
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
        double size = 1.0;  // the strike group's size against the template's (a pair to a massed blow)
        int variant = 0;    // the route's (route.mode 1): 0 covered, 1 other flank, 2 direct
    };

    bool NextTot(DayPart part, double& tot_h) const {
        const FactionDef& f = c_->data->faction;
        const double start = c_->now_h + f.doctrine.lead_time_h;
        const double end = start + f.doctrine.planning_cycle_h;
        const double abs_local = c_->data->theatre.start_hour + start;
        const double local = std::fmod(abs_local, 24.0);
        double delta = std::fmod(TheatrePartHour(c_->data->theatre, abs_local, part) - local + 48.0, 24.0);
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
        int main_flights = std::max(1, static_cast<int>(t.main_flights * mass * o.size + 0.5));
        if (mk == MissionKind::Recce || mk == MissionKind::Bda) main_flights = 1;
        const int fsize = t.flight_size;
        const int sq_main = PickSquadron(main_role, fsize, o.profile, o.part, w, reserved, t.aircraft.empty() ? nullptr : &t.aircraft);
        if (sq_main < 0) return false;
        const AircraftType& ma = *TypeOf((*c_->squadrons)[static_cast<std::size_t>(sq_main)]);
        if (ma.helicopter && o.profile != Profile::Low) return false;
        // The weapons and the attack (weaponeering): the target as its kind stands, against the guns the doctrine accepts
        // at the target. A type that can deliver nothing it carries this way, now, flies another way or not at all.
        Weaponeering wpn;
        const bool attacks = main_role == FlightRole::Strike || main_role == FlightRole::Dead || main_role == FlightRole::Helicopter;
        if (attacks && !ma.loads.empty() && !d.weapons.weapons.empty()) {
            const TargetProfileDef tp = d.weapons.targets[static_cast<int>(e.kind)];
            wpn = Weaponeer(d, ma, tp.target_class, tp.aim_points, o.profile, o.part, w, 0.5, f.doctrine.delivery_risk);
            if (wpn.weapon == nullptr) return false;
        }
        const double release_short = ReleaseShort(wpn.delivery, wpn.weapon, d.tuning);
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
        // The route planner's way (route.mode 1, docs/air_battle/01_ROUTES.md): the main flights' route over the threats
        // the staff believes in and the ground, by the option's variant; the other flank is planned against the covered.
        const bool routed = Mode() >= 1;
        PlannedRoute main_pr;
        if (routed) {
            const std::vector<const EntryPoint*> exits = ExitsFor(entry);
            const bool hide = mk == MissionKind::HeloRaid, loiter = mk == MissionKind::Hunter;
            const PlannedRoute v0 = RouteFor(ma, o.profile, o.part, entry, exits, e.pos, release_short, wpn.delivery, 0, ip_dist, hide, loiter, {});
            if (!v0.ok) return false;
            if (o.variant == 0) {
                main_pr = v0;
            } else {
                std::vector<std::string> avoid;
                if (o.variant == 1) {
                    // The covered route's turn points, in and home (the features on its straights are not charged).
                    avoid = v0.turns;
                    avoid.insert(avoid.end(), v0.home.begin(), v0.home.end());
                }
                main_pr = RouteFor(ma, o.profile, o.part, entry, exits, e.pos, release_short, wpn.delivery, o.variant, ip_dist, hide, loiter, avoid);
                if (!main_pr.ok) return false;
                // The same way as the covered route is no other option; a flank far dearer is not offered.
                const auto flown = [](const PlannedRoute& pr) {
                    std::vector<std::string> features;
                    for (const RoutePoint& rp : pr.points) features.push_back(rp.feature);
                    return features;
                };
                if (flown(main_pr) == flown(v0) && main_pr.exit == v0.exit) return false;
                if (o.variant == 1 && main_pr.cost > Tune().Get("route.prune_ratio") * std::max(1.0, v0.cost)) return false;
            }
            p.exit = main_pr.exit;
            p.corridor = main_pr.corridor;
            p.variant = o.variant;
            p.tactic = TacticKey(entry.id, main_pr.corridor, main_pr.exit, o.profile, o.part, mk);
            p.corridor_suppression = rules_.corridor_suppression;
        }
        for (int i = 0; i < flights_ok; ++i) {
            Flight fl;
            fl.callsign = callsign(cs++);
            fl.role = main_role;
            fl.squadron = (*c_->squadrons)[static_cast<std::size_t>(sq_main)].id;
            fl.aircraft = ma.id;
            fl.count = fsize;
            fl.profile = o.profile;
            fl.arms = main_role == FlightRole::Hunter ? ma.arms * fsize : 0;
            if (wpn.weapon != nullptr) {
                fl.weapon = wpn.weapon->id;
                fl.weapons = wpn.count;
                fl.delivery = wpn.delivery;
            }
            const double offset_h = (1.0 - f.doctrine.simultaneous_bias) * 0.05 * i;  // successive flights a few minutes apart
            if (routed) {
                fl.route = main_pr.points;
                TimeRoute(fl.route, o.tot_h + (mk == MissionKind::Hunter ? 0.0 : offset_h));
            } else if (mk == MissionKind::Hunter) {
                fl.route = LoiterRoute(entry, exit, e.pos, ProfileHeight(ma, o.profile), o.tot_h, ma.cruise_mps);
            } else {
                fl.route = MainRoute(entry, exit, e.pos, ProfileHeight(ma, o.profile), o.tot_h + offset_h, ma, ip_dist,
                                     mk == MissionKind::HeloRaid, release_short, fl.delivery);
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
            if (!th->threat_radar) continue;
            // route.mode 1: the union over the main flights, each threat once (01_ROUTES.md section 8).
            bool on = RouteChord(p.flights.front().route, th->pos, th->threat_radius_m) > 0.0;
            for (std::size_t fi = 1; routed && !on && fi < p.flights.size(); ++fi) {
                on = RouteChord(p.flights[fi].route, th->pos, th->threat_radius_m) > 0.0;
            }
            if (on) ++radar_threats;
        }
        // SEAD with the package.
        const bool seady = mk == MissionKind::Strike || mk == MissionKind::Dead || mk == MissionKind::Cas ||
                           mk == MissionKind::Recce || mk == MissionKind::Deception;
        // A corridor the staff believes too exposed: the suppression group goes ahead of the strike to open it, aimed at
        // the worst defence on it (hines_1981; route.mode 1).
        const RouteThreat* blast_at = nullptr;
        if (routed && seady && rules_.blast_exposure_s >= 0.0 && main_pr.exposure_s > rules_.blast_exposure_s) {
            blast_at = WorstOnRoute(p.flights.front().route, RouteThreatsFor(ma, o.profile), e.pos);
        }
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
                if (routed) {
                    const double lead_h = blast_at != nullptr ? rules_.blast_lead_s / 3600.0 : 0.05;
                    fl.route = FollowRoute(main_pr, sa, fl.profile, o.part, o.tot_h - lead_h);
                } else {
                    fl.route = MainRoute(entry, exit, e.pos, ProfileHeight(sa, fl.profile), o.tot_h - 0.05, sa, ip_dist, false);
                }
                fl.release_index = ReleaseIndex(fl.route);
                fl.aim_key = blast_at != nullptr ? blast_at->key : e.key;
                fl.aim_pos = blast_at != nullptr ? blast_at->pos : e.pos;
                p.flights.push_back(fl);
                want -= n;
            }
            if (p.Has(FlightRole::Sead)) {
                why.push_back("SEAD: " + std::to_string(p.Count(FlightRole::Sead)) + " aircraft for " + std::to_string(radar_threats) +
                              " radar threat(s) believed on the route");
                if (blast_at != nullptr) {
                    why.push_back("the corridor is too exposed: the suppression group goes " + std::to_string(static_cast<int>(rules_.blast_lead_s / 60.0 + 0.5)) +
                                  " min ahead to open it at " + blast_at->key);
                }
            }
        }
        // Escort against the defender's fighters.
        if (t.escort_ratio > 0.0 && c_->FriendlyFighters(*front_, p.target_pos) > 0.05) {
            const int want = static_cast<int>(std::round(p.Main() * t.escort_ratio * c_->FriendlyFighters(*front_, p.target_pos) * 2.0));
            if (want > 0) {
                const int n = std::min(want, 4);
                // How the faction sends them (roadmap B8, air_combat.json's fighter_plan): close escort, a sweep ahead on the
                // route, a CAP held off the target. Four go as two pairs in the two likeliest roles; fewer go as one.
                for (const auto& part : FighterParts(n)) {
                    const int sq = PickSquadron(FlightRole::Escort, part.second, Profile::Medium, o.part, w, reserved);
                    if (sq < 0) continue;
                    const AircraftType& ea = *TypeOf((*c_->squadrons)[static_cast<std::size_t>(sq)]);
                    reserved[static_cast<std::size_t>(sq)] += part.second;
                    Flight fl;
                    fl.callsign = callsign(cs++);
                    fl.role = part.first;
                    fl.squadron = (*c_->squadrons)[static_cast<std::size_t>(sq)].id;
                    fl.aircraft = ea.id;
                    fl.count = part.second;
                    fl.profile = Profile::Medium;
                    const AirDoctrineDef* doc = d.air_combat.Doctrine(d.faction.id);
                    const double lead_h = part.first == FlightRole::Sweep && doc != nullptr ? doc->sweep_lead_s / 3600.0 : 0.0;
                    if (part.first == FlightRole::Cap) {
                        fl.route = OrbitRoute(entry, e.pos, ProfileHeight(ea, Profile::Medium), o.tot_h, ea.cruise_mps);
                    } else {
                        fl.route = routed ? FollowRoute(main_pr, ea, Profile::Medium, o.part, o.tot_h - lead_h)
                                          : MainRoute(entry, exit, e.pos, ProfileHeight(ea, Profile::Medium), o.tot_h - lead_h, ea, ip_dist, false);
                    }
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
                    fl.route = routed ? FollowRoute(main_pr, ja, Profile::Medium, o.part, o.tot_h)
                                      : MainRoute(entry, exit, e.pos, ProfileHeight(ja, Profile::Medium), o.tot_h, ja, ip_dist, false);
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
                fl.route = routed ? FollowRoute(main_pr, da, Profile::Medium, o.part, o.tot_h - 0.07)
                                  : MainRoute(entry, exit, e.pos, ProfileHeight(da, Profile::Medium), o.tot_h - 0.07, da, ip_dist, false);
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
                fl.route = routed ? FollowRoute(main_pr, ra, o.profile, o.part, o.tot_h - 0.13)
                                  : MainRoute(entry, exit, e.pos, ProfileHeight(ra, o.profile), o.tot_h - 0.13, ra, ip_dist, false);
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
                const PlannedRoute fr = routed ? RouteFor(fa, Profile::Medium, o.part, front_->entries[other], {&front_->entries[other]}, short_of,
                                                          0.0, Delivery::Count, 0, 10000.0, false, false, {})
                                               : PlannedRoute();
                if (fr.ok) {
                    fl.route = fr.points;
                    TimeRoute(fl.route, o.tot_h - 0.2);
                } else {
                    fl.route = MainRoute(front_->entries[other], front_->entries[other], short_of, ProfileHeight(fa, Profile::Medium),
                                         o.tot_h - 0.2, fa, 10000.0, false);
                }
                p.flights.push_back(fl);
                why.push_back("a feint from " + front_->entries[other].id + " first, the planner's signature");
            }
        }
        // The Pact's counter-SAM programme (route.mode 1, 01_ROUTES.md §9): every flight, one escape for each believed zone
        // its route crosses at its height.
        if (routed && rules_.zone_escapes) {
            for (Flight& fl : p.flights) {
                const AircraftType* fa = f.Aircraft(fl.aircraft);
                if (fa == nullptr || fa->missile) continue;
                fl.escapes = RoutePlanner::Escapes(fl.route, RouteThreatsFor(*fa, fl.profile));
            }
        }
        // Reach: a flight whose type can't get to the target and back on its own fuel needs the tankers; beyond even a

        // refuelled radius, or with no tanker to spare this cycle, this way can't be flown (docs/air_war/11_SUPPORT.md).
        if (c_->tanker_receivers != nullptr) {
            // Jets fly from the bases behind the entry point, helicopters from forward sites at it.
            const double in = Dist2d(entry.pos, e.pos), depth = Dist2d(front_->bases, entry.pos);
            int receivers = 0;
            for (const Flight& fl : p.flights) {
                const AircraftType* a = f.Aircraft(fl.aircraft);
                if (a == nullptr || a->radius_m <= 0.0 || a->missile || a->drone) continue;
                // The route planner's routes: their own length (route.mode 1).
                const double dist = routed ? RadiusNeeded(fl, *a) : in + (a->helicopter ? 0.0 : depth);
                const double radius = a->radius_m * (fl.profile == Profile::Low ? d.tuning.Get("support.low_radius_share") : 1.0);
                if (dist <= radius) continue;
                if (dist > radius * d.tuning.Get("support.refuelled_radius")) return false;
                receivers += fl.count;
            }
            if (receivers > 0) {
                if (*c_->tanker_receivers + 1e-9 < receivers) return false;
                p.refuelled = receivers;
                why.push_back("refuelled on the way: " + std::to_string(receivers) + " aircraft beyond their own radius");
            }
        }
        return true;
    }

    // entry -> IP -> target -> egress -> exit. Times anchored at the target. `release_short`: the attack releases this far
    // short of the target ("target" is the release point) and turns away there (a loft, a stand-off launch); a pass over
    // the target flies on past it.
    std::vector<RoutePoint> MainRoute(const EntryPoint& entry, const EntryPoint& exit, const Vec3& target, double agl,
                                      double tot_h, const AircraftType& a, double ip_dist, bool helicopter_hide,
                                      double release_short = 0.0, Delivery delivery = Delivery::Count) const {
        std::vector<RoutePoint> r;
        const Vec3 in = target - entry.pos;
        const double len = std::max(1.0, Dist2d(entry.pos, target));
        const Vec3 dir = Vec3{in.x / len, in.y / len, 0.0};
        const Vec3 side{-dir.y, dir.x, 0.0};
        const double ipd = std::min(ip_dist, 0.6 * len);
        const double rs = std::min(release_short, 0.8 * ipd);
        RoutePoint p0{entry.pos, agl, 0.0, "entry"};
        RoutePoint ip{target - dir * ipd + side * (0.25 * ipd), helicopter_hide ? 5.0 : agl, 0.0, helicopter_hide ? "hide" : "ip"};
        RoutePoint tg{target - dir * rs, agl, 0.0, "target"};
        // The attack between the initial point and the release, as the delivery flies it (docs/air_war/10_WEAPONS.md): a
        // pop-up's pull-up to its apex and the dive onto the target, a toss's pull-up and the release climbing, a dive's
        // roll-in from height. A helicopter keeps its hide and its own pop-up.
        std::vector<RoutePoint> attack;
        const TuningTable& tune = Tune();
        const auto room = [&](double m) { return std::min(m, 0.9 * ipd); };
        if (!a.helicopter) {
            switch (delivery) {
                case Delivery::PopUp: {
                    const double pull = room(tune.Get("delivery.popup_pull_m"));
                    attack.push_back({target - dir * pull + side * 1500.0, agl, 0.0, "pull_up"});
                    attack.push_back({target - dir * (0.5 * (pull + rs)) + side * 800.0, tune.Get("delivery.popup_apex_m"), 0.0, "apex"});
                    tg.agl_m = tune.Get("delivery.popup_release_m");
                    break;
                }
                case Delivery::Loft:
                    attack.push_back({target - dir * room(tune.Get("delivery.loft_pull_m")), agl, 0.0, "pull_up"});
                    tg.agl_m = tune.Get("delivery.loft_release_m");
                    break;
                case Delivery::Dive:
                    attack.push_back({target - dir * room(tune.Get("delivery.dive_roll_in_m")), agl, 0.0, "roll_in"});
                    tg.agl_m = std::min(agl, tune.Get("delivery.dive_release_m"));
                    break;
                case Delivery::GentleDive: {  // roadmap B6: a shallow climb to a low apex and a 10-25 deg dive (mt_1970_bukhantsev)
                    const double pull = room(tune.Get("delivery.gentle_dive_pull_m"));
                    attack.push_back({target - dir * pull + side * 800.0, agl, 0.0, "pull_up"});
                    attack.push_back({target - dir * (0.5 * (pull + rs)) + side * 400.0, tune.Get("delivery.gentle_dive_apex_m"), 0.0, "apex"});
                    tg.agl_m = tune.Get("delivery.gentle_dive_release_m");
                    break;
                }
                default: break;
            }
        } else if (delivery == Delivery::HoverFire) {
            tg.agl_m = tune.Get("delivery.hover_agl_m");
        }
        const Vec3 out = rs > 1500.0 ? tg.pos + side * 12000.0 - dir * 2000.0 : target + side * 12000.0 + dir * 4000.0;
        RoutePoint eg{out, agl, 0.0, "egress"};
        RoutePoint ex{exit.pos, agl, 0.0, "exit"};
        r = {p0, ip};
        r.insert(r.end(), attack.begin(), attack.end());
        r.push_back(tg);
        r.push_back(eg);
        r.push_back(ex);
        TimeRoute(r, 2 + attack.size(), tot_h, helicopter_hide ? a.cruise_mps : a.attack_mps);
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

    // The package's fighters by the faction's plan (roadmap B8): `n` aircraft as (role, count) flights. Fewer than four fly
    // as one flight in the likeliest role; four as two pairs in the two likeliest (a tie: close escort, then the sweep,
    // then the CAP). A faction with no plan sends close escort.
    std::vector<std::pair<FlightRole, int>> FighterParts(int n) const {
        const AirWarData& d = *c_->data;
        const AirDoctrineDef* doc = d.air_combat.Doctrine(d.faction.id);
        std::vector<std::pair<double, FlightRole>> share = {{doc != nullptr ? doc->plan_close : 1.0, FlightRole::Escort},
                                                            {doc != nullptr ? doc->plan_sweep : 0.0, FlightRole::Sweep},
                                                            {doc != nullptr ? doc->plan_cap : 0.0, FlightRole::Cap}};
        std::stable_sort(share.begin(), share.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        if (n >= 4 && share[1].first > 0.0) return {{share[0].second, n / 2}, {share[1].second, n - n / 2}};
        return {{share[0].first > 0.0 ? share[0].second : FlightRole::Escort, n}};
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
    // What a believed site sees of a low route over the ground (route.mode 1 on a map).
    double MaskedChord(const std::vector<RoutePoint>& r, std::uint64_t route_hash, const IntelEntry& th, double ring) const {
        std::uint64_t tk = Fnv1a(th.key);
        for (double v : {ring, th.pos.x, th.pos.y}) {
            tk ^= static_cast<std::uint64_t>(std::llround(v));
            tk *= 1099511628211ull;
        }
        const std::pair<std::uint64_t, std::uint64_t> key{route_hash, tk};
        const auto it = chords_.find(key);
        if (it != chords_.end()) return it->second;
        double s = 0.0;
        for (std::size_t i = 0; i + 1 < r.size(); ++i) {
            s += GroundC().Seen(th.key, th.pos, ring, r[i].pos, r[i + 1].pos, r[i].agl_m, r[i + 1].agl_m, true);
        }
        return chords_[key] = s;
    }
    // A route's shape as a key (its points and heights; times apart), for the chord cache.
    static std::uint64_t RouteShapeHash(const std::vector<RoutePoint>& r) {
        std::uint64_t h = 1469598103934665603ull;
        for (const RoutePoint& p : r) {
            for (double v : {p.pos.x, p.pos.y, p.agl_m}) {
                h ^= static_cast<std::uint64_t>(std::llround(v * 10.0));
                h *= 1099511628211ull;
            }
        }
        return h;
    }
    // The believed threat a route is most exposed to short of its target's own zone (the blast's aim).
    static const RouteThreat* WorstOnRoute(const std::vector<RoutePoint>& r, const std::vector<RouteThreat>& threats, const Vec3& target) {
        const RouteThreat* best = nullptr;
        double best_w = 0.0;
        for (const RouteThreat& th : threats) {
            if (th.weight <= 0.0 || th.fighter || Dist2d(th.pos, target) < th.ring_m) continue;  // the group ahead suppresses sites
            const double w = th.weight * RouteChord(r, th.pos, th.ring_m);
            if (w > best_w + 1e-9) {
                best_w = w;
                best = &th;
            }
        }
        return best;
    }

    // The share of the target's aim points the package's attacking flights expect to put out of action with their
    // weapons (docs/air_war/10_WEAPONS.md); -1 when a flight attacks with no weapon (its strike_power instead).
    double WeaponsEffect(const Package& p, const IntelEntry& target) const {
        const AirWarData& d = *c_->data;
        const TargetProfileDef tp = d.weapons.targets[static_cast<int>(target.kind)];
        const Weather w = c_->WeatherAt(p.tot_h);
        double survive = 1.0;
        bool any = false;
        for (const Flight& fl : p.flights) {
            if (fl.role != FlightRole::Strike && fl.role != FlightRole::Dead && fl.role != FlightRole::Helicopter) continue;
            const WeaponDef* wd = d.Weapon(fl.weapon);
            const AircraftType* a = d.faction.Aircraft(fl.aircraft);
            if (wd == nullptr || a == nullptr) return -1.0;
            double kill = WeaponKill(*wd, fl.delivery, tp.target_class, DeliveryCep(*wd, fl.delivery, *a, p.part, w, 0.5, d.tuning), d.tuning);
            if (wd->guidance == "laser" && w == Weather::Cloudy) kill *= d.tuning.Get("delivery.cloudy_laser");
            survive *= 1.0 - ExpectedShare(kill, static_cast<double>(fl.count) * fl.weapons * p.load_factor, tp.aim_points);
            any = true;
        }
        return any ? 1.0 - survive : -1.0;
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
        const bool routed = Mode() >= 1;
        for (const Flight& fl : p.flights) {
            if (fl.role == FlightRole::Jammer && !fl.route.empty() && fl.route.front().tag == "entry" &&
                fl.route[1].tag == "orbit") {
                continue;  // stand-off: out of reach
            }
            if (routed && fl.role == FlightRole::Jammer && RouteIndexOf(fl.route, "orbit") >= 0) continue;
            const AircraftType* a = c_->data->faction.Aircraft(fl.aircraft);
            if (a == nullptr) continue;
            // A low flight on a map: each site sees only what the ground lets it (route.mode 1).
            const bool masked = routed && (fl.profile == Profile::Low || a->helicopter) && GroundC().HasGround();
            const std::uint64_t shape = masked ? RouteShapeHash(fl.route) : 0;
            // The route's box: a ring clear of it has no chord (route.mode 1; the old shapes keep their path).
            double bx0 = 1e300, bx1 = -1e300, by0 = 1e300, by1 = -1e300;
            for (const RoutePoint& rp : fl.route) {
                bx0 = std::min(bx0, rp.pos.x);
                bx1 = std::max(bx1, rp.pos.x);
                by0 = std::min(by0, rp.pos.y);
                by1 = std::max(by1, rp.pos.y);
            }
            double per = 0.0;
            for (const IntelEntry* th : c_->intel->Threats()) {
                if (th->threat_max_target_mps > 0.0 && a->cruise_mps > th->threat_max_target_mps) continue;  // too fast for it
                // A threat whose place is uncertain covers a wider ring, but is less likely to be on any one route
                // through it: the lethality is spread over the ring's area.
                const double ring = th->threat_radius_m + 0.5 * th->sigma_m;
                const double spread = (th->threat_radius_m / ring) * (th->threat_radius_m / ring);
                const bool clear = routed && (th->pos.x + ring < bx0 || th->pos.x - ring > bx1 || th->pos.y + ring < by0 || th->pos.y - ring > by1);
                const double chord = clear ? 0.0 : (masked ? MaskedChord(fl.route, shape, *th, ring) : RouteChord(fl.route, th->pos, ring));
                const bool optical = !th->threat_radar;
                double env = 1.0;
                if (optical && p.part == DayPart::Night) env *= tune.Get("attrition.night");
                if (optical && w == Weather::Poor) env *= tune.Get("attrition.poor_weather");
                const double supp = sead > 0 ? tune.Get("attrition.sead_suppression") * std::min(1.0, sead / 2.0) * (0.6 + 0.4 * sead_skill) : 0.0;
                const double engaged = fl.count > 0 ? EngagedAircraft(th->threat_channels, th->function, fl.count) / static_cast<double>(fl.count) : 1.0;
                per += th->Live() * spread * engaged *
                       PassKillChance(th->threat_lethality, chord, ring, fl.profile, a->helicopter, th->threat_low, th->threat_medium,
                                      th->threat_radar, supp, jam ? tune.Get("attrition.jamming") : 0.0, env, a->evasion, tune, masked);
            }
            const bool attacks = fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Helicopter;
            if (attacks && !target.IsThreat() && !target.ghost) {
                double env = 1.0;
                if (p.part == DayPart::Night) env *= tune.Get("attrition.night");
                if (w == Weather::Poor) env *= tune.Get("attrition.poor_weather");
                per += Clamp01(target.exists) * TargetGunsChance(target.kind, FlightExposure(fl, a->helicopter, tune), a->evasion, env, tune);
            }
            if (a->drone) per = 0.0;  // decoys are meant to be lost
            const double fl_loss = std::min(1.0, per) * fl.count;
            total += fl_loss;
            if (fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Helicopter ||
                fl.role == FlightRole::Hunter || fl.role == FlightRole::Recce) {
                main += fl_loss;
            }
        }
        // Our fighters (roadmap B8): the abstract air battle's expectation, its escorts' cover and the AWACS where it
        // controls fighters.
        const AbstractAirFight air_fight(*c_->data);
        const double fighters = air_fight.ExpectedMainLoss(p, AbstractAirFight::SetupFor(*c_->data, p, c_->awacs, w, c_->fighters, p.target_pos));
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
        // An armed reconnaissance searches each line once a cycle.
        std::set<int> used_lines;
        if (mk == MissionKind::ArmedRecce) {
            for (const std::string& k : used) {
                const TheatreObject* uo = d.theatre.Find(k);
                if (uo != nullptr && uo->supply_line >= 0) used_lines.insert(uo->supply_line);
            }
        }
        const bool routed = Mode() >= 1;
        const std::vector<int> variants = routed ? rules_.variants : std::vector<int>{0};
        for (const IntelEntry* seen : Candidates(mk)) {
            if (used.count(seen->key) > 0) continue;
            // An armed reconnaissance flies to the stretch of the line nearest the front, not to where a column was seen.
            const IntelEntry* e = seen;
            IntelEntry patrol;
            const int line = mk == MissionKind::ArmedRecce ? LineOf(*seen) : -1;
            if (mk == MissionKind::ArmedRecce) {
                if (line < 0 || used_lines.count(line) > 0) continue;
                patrol = *seen;
                patrol.pos = c_->supply->PatrolPoint(static_cast<std::size_t>(line), front_->line, c_->front_line);
                patrol.sigma_m = 0.0;
                e = &patrol;
            }
            // One option weighed: this gate, profile, part of the day, size of strike group and route variant.
            const auto weigh = [&](std::size_t ei, Profile prof, DayPart part, double size, int variant, double* u_out) -> bool {
                Option o;
                o.variant = variant;
                o.entry = static_cast<int>(ei);
                o.exit = static_cast<int>(ei);
                o.profile = prof;
                o.part = part;
                o.size = size;
                if (!NextTot(part, o.tot_h)) return false;
                Package p;
                std::vector<std::string> why;
                if (!Build(mk, *e, o, p, why)) return false;
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
                if (mk == MissionKind::ArmedRecce) {
                    // A column on its stretch, and seen there (logistics.hpp).
                    const AircraftType* lead = f.Aircraft(p.flights.front().aircraft);
                    p_loc = c_->supply->OnStretchChance(d, static_cast<std::size_t>(line), part) *
                            ColumnFindChance(part, c_->WeatherAt(o.tot_h), lead != nullptr && lead->night, tune);
                }
                const double main_n = std::max(1, p.Main());
                const double p_deliver = Clamp01(1.0 - main_loss / main_n);
                double effect = 1.0;
                const AircraftType* ma = f.Aircraft(p.flights.front().aircraft);
                if (mk == MissionKind::Strike || mk == MissionKind::Dead || mk == MissionKind::Cas ||
                    mk == MissionKind::HeloRaid || mk == MissionKind::Harassment || mk == MissionKind::ArmedRecce) {
                    const double hard = HardnessOf(*e);
                    effect = Clamp01(p.Main() * (ma != nullptr ? ma->strike_power : 0.2) * tune.Get("strike.hit_chance") /
                                     (0.5 + hard));
                    const double we = WeaponsEffect(p, *e);
                    if (we >= 0.0) effect = we;
                }
                double live = e->Live();
                if (mk == MissionKind::Recce) live = std::max(0.3, 1.0 - e->exists) + std::min(1.0, age / 24.0);
                if (mk == MissionKind::Bda || mk == MissionKind::ArmedRecce) live = 1.0;
                if (pln.Quirk("elint_trust") > 0.0 && e->source == IntelSource::Elint) live = std::min(1.0, live + 0.3);
                // The Pact's helicopters beyond the cover of their own air defence: worth less to risk them
                // there (route.mode 1, the doctrine's helo_umbrella). The umbrella goes forward with the target's
                // sector's air front (F2.4: within it of the air front, not of the entry point; 0 off).
                double umbrella = 1.0;
                if (routed && rules_.helo_umbrella && mk == MissionKind::HeloRaid &&
                    BeyondHeloUmbrella(e->pos, front_->entries[ei].pos, c_->air_front, tune.Get("route.helo_umbrella_m"))) {
                    umbrella = tune.Get("route.helo_beyond_umbrella");
                }
                const double ev = v * live * p_loc * p_deliver * effect * umbrella;
                double u = tune.Get("utility.value_weight") * ev - tune.Get("utility.loss_cost") * eloss * loss_aversion -
                           tune.Get("utility.sortie_cost") * p.Aircraft();
                // His habits.
                const double qs = tune.Get("utility.quirk_scale");
                const char* pref[] = {"pref_night", "pref_dawn", "pref_day", "pref_dusk"};
                const double tod = pln.Quirk(pref[static_cast<int>(part)]);
                u += qs * tod;
                // The same way in as last time: by the gate (the old shapes) or by the corridor (route.mode 1).
                const bool same_axis = routed ? (!p.corridor.empty() && p.corridor == pln.last_corridor)
                                              : front_->entries[ei].id == pln.last_entry;
                if (same_axis) u += qs * (pln.Quirk("axis_repeat") - pln.Quirk("axis_vary"));
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
                if (routed) {
                    // A stream of its own for each option (keyed): how many options are weighed changes no
                    // other draw.
                    Rng sub = c_->rng->Fork(Fnv1a(std::string(MissionKindName(mk)) + "|" + e->key + "|" + front_->entries[ei].id + "|" +
                                                  ProfileName(prof) + "|" + DayPartName(part) + "|" + Num1(size) + "|" +
                                                  std::to_string(variant)));
                    u += sub.Normal(0.0, noise_sd);
                } else {
                    u += c_->rng->Normal(0.0, noise_sd);
                }
                // A nuisance raid is flown for the sleep it costs, not the damage; an armed reconnaissance is
                // judged by what it does when it finds a column.
                const double gate = mk == MissionKind::ArmedRecce ? v * p_deliver * effect : ev;
                const bool achieves = gate >= MinEffect() || mk == MissionKind::Deception || mk == MissionKind::Hunter ||
                                      mk == MissionKind::Recce || mk == MissionKind::Bda || mk == MissionKind::Harassment;
                if (u > best_u && achieves) {
                    best_u = u;
                    p.expected_losses = eloss;
                    p.expected_value = ev;
                    p.utility = u;
                    best_out = p;
                    best_why = vwhy;
                    for (const std::string& w : why) best_why.push_back(w);
                    if (tod > 0.15) best_why.push_back(std::string("chose ") + DayPartName(part) + ": the planner's habit");
                    if (same_axis && pln.Quirk("axis_repeat") > 0.0) {
                        best_why.push_back("the same way in as last time (" + (routed ? p.corridor : front_->entries[ei].id) + ")");
                    }
                    if (routed && variant == 1) best_why.push_back("in by the other flank, " + p.corridor);
                    if (routed && variant == 2) best_why.push_back("straight in, " + p.corridor + ": time over exposure");
                    if (mem != pln.tactics.end() && mem->second.loss_rate > 0.1) {
                        best_why.push_back("remembers losing " + Pct(mem->second.loss_rate) + " this way, but the target is worth it");
                    }
                    if (e->mobile && age > 6.0) {
                        best_why.push_back("last seen " + std::to_string(static_cast<int>(age + 0.5)) + " h ago: it may have moved");
                    }
                    found = true;
                }
                *u_out = u;
                return true;
            };
            // Every gate, profile, part and size by the faction's first route variant; the other variants (route.mode 1)
            // on the best of those shapes only, so that weighing them costs a few options a target, not a multiple.
            std::size_t top_ei = 0;
            Profile top_prof = Profile::Low;
            DayPart top_part = DayPart::Day;
            double top_size = 1.0, top_u = -1e300;
            // Medium altitude only once the defences are broken (10_FACTIONS.md §9.1: NATO's strike, close support and armed
            // reconnaissance fly low while the believed working defences are at or above the template's line). A political
            // order to fly high stands above it.
            const bool medium_shut = c_->politics->min_profile < 1 && !MediumOffered(t, c_->intel->BelievedIadsShare(), HotMode());
            for (std::size_t ei = 0; ei < front_->entries.size(); ++ei) {
                for (Profile prof : t.profiles) {
                    if (c_->politics->min_profile >= 1 && prof == Profile::Low) continue;
                    if (medium_shut && prof == Profile::Medium) continue;
                    for (DayPart part : t.day_parts) {
                        // The strike group's size: with a cost for every sortie, the planner sizes it to the target (a pair
                        // for a radar, a massed blow for an airfield; docs/air_war/13_PACKAGES.md).
                        const bool sized = tune.Get("utility.sortie_cost") > 0.0 &&
                                           (mk == MissionKind::Strike || mk == MissionKind::Dead || mk == MissionKind::Cas);
                        for (double sz : sized ? std::vector<double>{0.5, 1.0, 2.0} : std::vector<double>{1.0}) {
                            double u = 0.0;
                            if (weigh(ei, prof, part, sz, variants.front(), &u) && u > top_u) {
                                top_u = u;
                                top_ei = ei;
                                top_prof = prof;
                                top_part = part;
                                top_size = sz;
                            }
                        }
                    }
                }
            }
            for (std::size_t vi = 1; routed && top_u > -1e299 && vi < variants.size(); ++vi) {
                double u = 0.0;
                weigh(top_ei, top_prof, top_part, top_size, variants[vi], &u);
            }
        }
        if (!found || best_u < MinValue()) return false;
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
                    if (Mode() >= 1) {
                        Rng sub = c_->rng->Fork(Fnv1a(std::string("missile|") + e->key + "|" + entry.id + "|" + DayPartName(part)));
                        u += sub.Normal(0.0, noise_sd);
                    } else {
                        u += c_->rng->Normal(0.0, noise_sd);
                    }
                    if (u > best_u && ev >= MinEffect()) {
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
        return found && best_u >= MinValue();
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
        // The gate (route.mode 1, docs/air_battle/01_ROUTES.md §6.7): a crossing by a gate gets its time in the gate book,
        // at least the faction's separation from every other this cycle; close support crosses where it must.
        if (Mode() >= 1 && p.kind != MissionKind::Missile) {
            pln.last_corridor = p.corridor;
            const bool gated = (rules_.crossing == "gate" || rules_.crossing == "checkpoint_gate") && !(rules_.cas_free && p.kind == MissionKind::Cas);
            double gate_h = -1.0;
            for (const Flight& fl : p.flights) {
                const int gi = RouteIndexOf(fl.route, "gate");
                if (gi >= 0 && (fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Helicopter ||
                                fl.role == FlightRole::Hunter || fl.role == FlightRole::Recce || fl.role == FlightRole::Decoy)) {
                    gate_h = fl.route[static_cast<std::size_t>(gi)].t_h;
                    break;
                }
            }
            if (gated && gate_h >= 0.0) {
                const double gshift = GateShift(p.entry, gate_h);
                if (std::fabs(gshift) > 1e-9) {
                    for (Flight& fl : p.flights) {
                        for (RoutePoint& rp : fl.route) rp.t_h += gshift;
                    }
                    p.tot_h += gshift;
                    p.rationale.push_back("the time moved " + std::to_string(static_cast<int>(std::lround(gshift * 60.0))) + " min for a free slot at gate " +
                                          p.entry);
                }
                gate_h += gshift;
                gate_book_[p.entry].push_back(gate_h);
                p.gate_h = gate_h;
                p.gate_window_s = rules_.gate_window_s;
            }
        }

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
        if (c_->tanker_receivers != nullptr) *c_->tanker_receivers = std::max(0.0, *c_->tanker_receivers - p.refuelled);
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
        p.part = TheatrePartAt(c_->data->theatre, c_->data->theatre.start_hour + p.tot_h);
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
