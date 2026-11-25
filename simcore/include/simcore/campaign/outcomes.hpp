// simcore/campaign/outcomes.hpp — the player's two outcomes, measured (roadmap 14.6, first form;
// docs/campaign/05_THE_SYSTEMS.md §12 and §14, decision B14).
//
// The player cannot change the war; they change two outcomes. Both are kept here as ledgers from the truth, for the
// mission's debrief and the campaign's reveal, never for anyone in play: the combat report from the records
// (report.hpp) stays apart and never sees them.
//   - Assets kept: the object each of the battery's tasks covered, the hours it stood covered and working, the hits on
//     it, and the releases prevented against it (flights whose release point was in the session and that released
//     nothing: shot down first, or broken away).
//   - Freedom of action denied: what each enemy package that came at the battery brought, spent and lost, with the
//     enemy's own reasons (its plan's rationale); and whether the battery's entry in the enemy's picture held the
//     enemy's suppression phase open (the believed share of working air defences with and without it, against the
//     phase's threshold); and (track F, F6.2, docs/fronts/06_BALANCE.md section 4) the enemy's air front in the battery's
//     sector: the hours its gates stood shut only for the battery's entry (with every other gate open, they would have
//     held without it), the air front's delay the battery bought, and the steps it made there meanwhile.
// Left: each package planned again without the battery's entry (what was added for it), the rebuilds under cover.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"

namespace simcore {
namespace campaign {

// One object a task of the battery covered.
struct CoveredAsset {
    std::string place;            // the task's covered place (an anchor)
    std::string object;           // the theatre object it stands at ("" none: not in the air war)
    std::string name;
    double covered_h = 0.0;       // the battery ready for action on its task
    double working_h = 0.0;       // those hours weighted by the object's working share
    double function_start = 1.0;  // its working share when first covered
    double function_now = 1.0;
    int hits = 0;                 // the played packages' hits on it
    double damage = 0.0;          // the working share they removed
    int releases_prevented = 0;   // flights against it that released nothing
};

// One enemy package that came at the battery: what it brought, spent and lost, and why it came.
struct PackageLedger {
    int id = 0;
    std::string name;
    std::string kind;              // its mission kind
    std::string target;            // its target as the enemy labelled it
    std::string target_object;     // the real object behind it ("" a ghost)
    bool against_battery = false;  // planned against the battery itself
    int aircraft = 0;
    int missiles = 0;              // cruise missiles
    int sead = 0, jammers = 0, decoys = 0, escorts = 0;
    int arms_fired = 0;            // anti-radiation missiles it fired in the session
    int downed_by_battery = 0;     // its aircraft and missiles the battery shot down
    int downed_by_others = 0;      // by our other defences
    int downed_by_fighters = 0;    // by our fighters (roadmap B8; counted apart from downed_by_others)
    int our_fighters_lost = 0;     // our fighters its aircraft shot down
    int release_flights = 0;       // flights with their release point in the session
    int released = 0;              // ... that released
    int prevented_downed = 0;      // ... that released nothing, all shot down first
    int prevented_broke_away = 0;  // ... that released nothing and broke away
    int prevented_jettisoned = 0;  // ... that released nothing, their stores thrown off under fire (roadmap B4)
    std::vector<std::string> why;  // the enemy's own reasons
    int Prevented() const { return prevented_downed + prevented_broke_away + prevented_jettisoned; }
};

// Whether the battery's entry in the enemy's picture holds the enemy's present phase open.
struct SuppressionCheck {
    double h = 0.0;               // the campaign's hour
    std::string phase;
    double threshold = -1.0;      // the believed share of working SAM radars below which it moves on (< 0: never on them)
    bool known = false;           // the battery is in the enemy's picture at all
    double with_battery = 1.0;
    double without_battery = 1.0;
    bool HoldsOpen() const { return threshold >= 0.0 && with_battery >= threshold && without_battery < threshold; }
};

// The enemy's air front in the battery's sector at a moment (F6.2), on the last planning cycle's measure (what its air
// staff decided on, air_front.hpp): the believed radar SAM cover over the sector's next band and our mobile air defence
// there believed destroyed, with the battery's entry and without it (its believed working share taken off the sums, the
// band's baseline kept: a unit gone lowers the cover as a destroyed one does), against the gates as the chief's traits move
// them; and whether the rest of the gates were open (the band contested, something worth flying for, losses under the
// line, room for a step, the politics and a surge's rest).
struct AirFrontCheck {
    double h = 0.0;
    int sector = -1;            // the battery's sector of the line of contact (-1: no air front, air_front.mode 0)
    std::string sector_id;
    double depth_m = 0.0;       // the sector's air front beyond the line of contact
    int advances = 0;           // its steps forward and back so far
    int pullbacks = 0;
    bool known = false;         // the battery's entry in the picture, its ring reaching the next band
    double cover_with = 0.0, cover_without = 0.0;
    double mobile_with = 1.0, mobile_without = 1.0;
    double gate_cover = 0.0, gate_mobile = 1.0;
    bool contested = false;
    bool others_open = false;   // every gate but the cover and the mobile air defence open
    static bool Gates(double cover, double mobile, double gate_cover, double gate_mobile) {
        return cover <= gate_cover + 1e-9 && mobile >= gate_mobile - 1e-9;
    }
    bool OpenWith() const { return contested && others_open && Gates(cover_with, mobile_with, gate_cover, gate_mobile); }
    bool OpenWithout() const { return contested && others_open && Gates(cover_without, mobile_without, gate_cover, gate_mobile); }
    // The battery's entry is what keeps the sector's air front where it is.
    bool HoldsBack() const { return sector >= 0 && !OpenWith() && OpenWithout(); }
};

// The battery's own theatre object ("" none).
inline std::string OwnObject(const airwar::AirCampaign& c) {
    const int i = c.PlayerIndex();
    return i >= 0 && static_cast<std::size_t>(i) < c.Data().theatre.objects.size() ? c.Data().theatre.objects[static_cast<std::size_t>(i)].id
                                                                                     : std::string();
}

// A theatre object's working share now (1 if the theatre has no such object).
inline double ObjectFunction(const airwar::AirCampaign& c, const std::string& object) {
    for (const airwar::ObjectState& s : c.Truth()) {
        if (s.id == object) return s.function;
    }
    return 1.0;
}

// Sector s's measure `cov` (SectorCoverBook::Measure) as it would be without the believed entry `e`: its believed
// working share off the radar SAM and the mobile air defence sums where its ring reaches the band, the baselines kept (a
// unit gone counts as one destroyed). Returns whether it reached the band; `cover` and `mobile_destroyed` are left as
// they are when it does not.
inline bool CoverWithout(const airwar::LineOfContact& line, std::size_t s, const airwar::SectorCover& cov, const airwar::IntelEntry& e, double& cover,
                         double& mobile_destroyed) {
    const bool sam = e.IsSamRadar(), mobile = airwar::IsMobileAirDefence(e);
    if (!sam && !mobile) return false;
    const airwar::FrontPlace pl = line.Place(e.pos);
    const double depth = pl.side * pl.distance_m - line.Offset(pl.along_m);
    if (!airwar::RingReachesBand(line, s, pl.along_m, depth, e.threat_radius_m, cov.near_m, cov.far_m)) return false;
    const double working = e.BelievedThere() * airwar::Clamp01(e.function);
    if (sam && cov.sam_baseline > 0.0) cover = airwar::Clamp01((cov.sam_working - working) / cov.sam_baseline);
    if (mobile && cov.mobile_baseline > 0.0) mobile_destroyed = airwar::Clamp01(1.0 - (cov.mobile_working - working) / cov.mobile_baseline);
    return true;
}

// The enemy's air front in the sector where the battery stands now (none without an air front).
inline AirFrontCheck CheckAirFront(const airwar::AirCampaign& c, double h) {
    using namespace airwar;
    AirFrontCheck a;
    a.h = h;
    const int player = c.PlayerIndex();
    const LineOfContact& line = c.FrontLine();
    const AirFrontDef& def = c.Data().faction.air_front;
    if (player < 0 || line.Empty() || !def.present || c.AirFront().sectors.size() != line.SectorCount() ||
        c.SectorCovers().size() != line.SectorCount()) {
        return a;
    }
    const int si = line.SectorOf(c.Truth()[static_cast<std::size_t>(player)].pos);
    if (si < 0 || static_cast<std::size_t>(si) >= line.SectorCount()) return a;
    const std::size_t s = static_cast<std::size_t>(si);
    const SectorFront& f = c.AirFront().sectors[s];
    const SectorCover& cov = c.SectorCovers()[s];
    a.sector = si;
    a.sector_id = line.Sector(s).id;
    a.depth_m = f.depth_m;
    a.advances = f.advances;
    a.pullbacks = f.pullbacks;
    a.contested = f.contested;
    a.cover_with = a.cover_without = cov.cover;
    a.mobile_with = a.mobile_without = cov.mobile_destroyed;
    AirFrontRules rules;
    AirFrontInputs in;
    AirFrontChief who;
    c.AirFrontDecisionInputs(rules, in, who);
    const AirFrontGates g = GatesOf(def, rules, who);
    a.gate_cover = g.cover;
    a.gate_mobile = g.mobile;
    // The battery's believed entry over the band, taken off the measure's sums.
    if (const IntelEntry* e = c.Intel().Find(OwnObject(c))) a.known = CoverWithout(line, s, cov, *e, a.cover_without, a.mobile_without);
    // The rest of the gates, as DecideAirFront reads them.
    const int day = SectorLossWindow::DayOf(h);
    const SectorLossWindow& losses = c.SectorLosses();
    const bool over = losses.Sorties(si, day, def.loss_days) >= rules.min_sorties && losses.Rate(si, day, def.loss_days) > g.loss_line;
    const bool override_losses = h < f.crisis_until_h && def.crisis == AirFrontCrisis::Advance;
    const bool may_move = def.kind != AirFrontKind::Border || in.event_fired;
    const double cap = AirFrontCap(def, line, s, in.politics_max_depth_m, may_move);
    const double reward = s < c.AirFrontRewards().size() ? c.AirFrontRewards()[s] : 0.0;
    a.others_open = may_move && reward >= g.min_reward && (!over || override_losses) && f.depth_m + rules.step_m <= cap + 1e-6 &&
                    (def.kind != AirFrontKind::Surge || h >= f.rest_until_h);
    return a;
}

inline SuppressionCheck CheckSuppression(const airwar::AirCampaign& c, double h) {
    SuppressionCheck s;
    s.h = h;
    const std::vector<airwar::PhaseDef>& phases = c.Data().faction.doctrine.phases;
    if (c.Phase() >= 0 && static_cast<std::size_t>(c.Phase()) < phases.size()) {
        const airwar::PhaseDef& ph = phases[static_cast<std::size_t>(c.Phase())];
        s.phase = ph.name;
        // The last phase has nowhere to move on to.
        if (static_cast<std::size_t>(c.Phase()) + 1 < phases.size()) s.threshold = ph.until_iads;
    }
    const std::string own = OwnObject(c);
    s.known = !own.empty() && c.Intel().Find(own) != nullptr;
    s.with_battery = c.Intel().BelievedIadsShare();
    s.without_battery = c.Intel().BelievedIadsShare(own);
    return s;
}

// One played package's ledger: the package as planned, its outcome, the flights whose release point was in the
// session (indices into its flights), and the battery's own object.
inline PackageLedger LedgerOf(const airwar::Package& p, const airwar::PackageOutcome& o, const std::vector<int>& release_flights,
                              const std::string& own_object) {
    using airwar::FlightRole;
    PackageLedger l;
    l.id = p.id;
    l.name = p.name;
    l.kind = airwar::MissionKindName(p.kind);
    l.target = p.target_label;
    l.target_object = p.target_object;
    l.against_battery = !own_object.empty() && p.target_object == own_object;
    l.aircraft = p.Aircraft();
    l.missiles = p.Missiles();
    l.sead = p.Count(FlightRole::Sead) + p.Count(FlightRole::Hunter);
    l.jammers = p.Count(FlightRole::Jammer);
    l.decoys = p.Count(FlightRole::Decoy);
    l.escorts = p.Count(FlightRole::Escort) + p.Count(FlightRole::Sweep) + p.Count(FlightRole::Cap);
    l.arms_fired = o.arms_fired;
    for (const airwar::LossRecord& loss : o.losses) (loss.fighter ? l.downed_by_fighters : loss.by == own_object ? l.downed_by_battery : l.downed_by_others) += 1;
    l.our_fighters_lost = static_cast<int>(o.our_fighters_lost.size());
    for (const std::string& by : o.missiles_downed) (by == own_object ? l.downed_by_battery : l.downed_by_others) += 1;
    const auto has = [](const std::vector<int>& v, int f) { return std::find(v.begin(), v.end(), f) != v.end(); };
    for (int f : release_flights) {
        ++l.release_flights;
        if (has(o.released_flights, f)) {
            ++l.released;
        } else if (has(o.jettisoned_flights, f)) {
            ++l.prevented_jettisoned;  // its stores thrown off short of the release point (it may have turned for home too)
        } else if (has(o.aborted_flights, f)) {
            ++l.prevented_broke_away;
        } else {
            ++l.prevented_downed;  // the bridge's "broke away" needs some of it alive: none released, none left
        }
    }
    l.why = p.rationale;
    return l;
}

class OutcomeLedger {
public:
    // A task's covered object (once per place): its working share when first covered.
    void Cover(const std::string& place, const std::string& object, const std::string& name, double function_now) {
        if (Find(place) != nullptr) return;
        CoveredAsset a;
        a.place = place;
        a.object = object;
        a.name = name.empty() ? place : name;
        a.function_start = a.function_now = function_now;
        assets_.push_back(a);
    }
    // `dt_s` with the battery ready for action on its task covering `place`; the object's working share now.
    void Covered(const std::string& place, double dt_s, double function_now) {
        CoveredAsset* a = Mutable(place);
        if (a == nullptr) return;
        a->covered_h += dt_s / 3600.0;
        a->working_h += dt_s / 3600.0 * std::max(0.0, std::min(1.0, function_now));
        a->function_now = function_now;
    }
    // The object's working share now, covered or not (the raids resolved off the scene count too).
    void Seen(const std::string& place, double function_now) {
        if (CoveredAsset* a = Mutable(place)) a->function_now = function_now;
    }
    // A package back from the session: its ledger, and its hits and prevented releases on the covered objects.
    void Played(const airwar::Package& p, const airwar::PackageOutcome& o, const std::vector<int>& release_flights,
                const std::string& own_object) {
        const PackageLedger l = LedgerOf(p, o, release_flights, own_object);
        for (CoveredAsset& a : assets_) {
            if (a.object.empty()) continue;
            for (const auto& d : o.damage) {
                if (d.first != a.object) continue;
                ++a.hits;
                a.damage += d.second;
            }
            if (p.target_object == a.object) a.releases_prevented += l.Prevented();
        }
        packages_.push_back(l);
    }
    void Check(const SuppressionCheck& s) { checks_.push_back(s); }
    // The air front in the battery's sector at a moment, standing for the next `dt_h` hours (F6.2): the hours its gates
    // were shut only for the battery's entry add to the delay; the steps made since the first check are counted.
    void AirFront(const AirFrontCheck& a, double dt_h) {
        if (a.sector < 0) return;
        if (air_front_checks_ == 0) {
            air_front_first_ = a;
        } else if (a.sector != air_front_last_.sector) {
            // The battery moved to another sector: the steps count from where that sector's front stood.
            steps_before_ += air_front_last_.advances - air_front_first_.advances;
            back_before_ += air_front_last_.pullbacks - air_front_first_.pullbacks;
            air_front_first_ = a;
        }
        air_front_last_ = a;
        ++air_front_checks_;
        air_front_h_ += dt_h;
        if (a.HoldsBack()) air_front_held_h_ += dt_h;
    }
    // The hours the battery's entry held the enemy's air front in its sector (the gates open without it).
    double AirFrontHeldH() const { return air_front_held_h_; }
    // The hours the air front was watched (checks with a sector).
    double AirFrontWatchedH() const { return air_front_h_; }
    // Its steps forward and back in the battery's sectors since the first check.
    int AirFrontSteps() const { return air_front_checks_ > 0 ? steps_before_ + air_front_last_.advances - air_front_first_.advances : 0; }
    int AirFrontPullbacks() const { return air_front_checks_ > 0 ? back_before_ + air_front_last_.pullbacks - air_front_first_.pullbacks : 0; }
    const AirFrontCheck* LastAirFront() const { return air_front_checks_ > 0 ? &air_front_last_ : nullptr; }

    const std::vector<CoveredAsset>& Assets() const { return assets_; }
    const std::vector<PackageLedger>& Packages() const { return packages_; }
    const std::vector<SuppressionCheck>& Checks() const { return checks_; }
    const CoveredAsset* Find(const std::string& place) const {
        for (const CoveredAsset& a : assets_) {
            if (a.place == place) return &a;
        }
        return nullptr;
    }

    // The debrief's section. `end`: the phase check at the debrief's moment (none: the last kept).
    std::string Text(const SuppressionCheck* end = nullptr) const {
        std::string out = "The battery's two outcomes (from the truth, for the debrief; nobody in the battery knows them):\n";
        out += "  Assets kept:\n";
        if (assets_.empty()) out += "    no object covered\n";
        for (const CoveredAsset& a : assets_) {
            out += "    " + a.name + " (" + a.place + "): covered " + Hours(a.covered_h) + ", " +
                   (a.covered_h > 0.0 ? Pct(a.working_h / a.covered_h) + " of it working" : std::string("never ready over it")) +
                   "; working " + Pct(a.function_start) + " when first covered, " + Pct(a.function_now) + " now";
            out += "; " + std::to_string(a.hits) + (a.hits == 1 ? " hit" : " hits") + " on it in the battery's fights";
            if (a.hits > 0) out += " (" + Pct(a.damage) + " of its working taken)";
            out += "; " + std::to_string(a.releases_prevented) + " flight" + (a.releases_prevented == 1 ? "" : "s") +
                   " against it released nothing\n";
        }
        out += "  Freedom of action denied:\n";
        if (packages_.empty()) out += "    no package came at the battery\n";
        for (const PackageLedger& l : packages_) {
            out += "    package " + std::to_string(l.id) + " '" + l.name + "', " + l.kind + " on " + l.target +
                   (l.against_battery ? " (the battery itself)" : "") + ": " + std::to_string(l.aircraft) + " aircraft";
            if (l.missiles > 0) out += " and " + std::to_string(l.missiles) + " cruise missiles";
            std::string brought;
            const auto add = [&brought](int n, const char* what) {
                if (n > 0) brought += (brought.empty() ? "" : ", ") + std::to_string(n) + " " + what;
            };
            add(l.sead, "suppressing");
            add(l.jammers, "jamming");
            add(l.decoys, "decoys");
            add(l.escorts, "escorting");
            if (!brought.empty()) out += " (" + brought + ")";
            out += "; " + std::to_string(l.arms_fired) + " anti-radiation missiles fired; " + std::to_string(l.downed_by_battery) +
                   " shot down by the battery, " + std::to_string(l.downed_by_others) + " by our others";
            if (l.downed_by_fighters > 0 || l.our_fighters_lost > 0) {
                out += ", " + std::to_string(l.downed_by_fighters) + " by our fighters (" + std::to_string(l.our_fighters_lost) + " of ours lost)";
            }
            if (l.release_flights > 0) {
                out += "; of " + std::to_string(l.release_flights) + " flights to release here " + std::to_string(l.released) + " released, " +
                       std::to_string(l.prevented_downed) + " shot down first, " + std::to_string(l.prevented_broke_away) + " broke away";
                if (l.prevented_jettisoned > 0) out += ", " + std::to_string(l.prevented_jettisoned) + " threw their stores off";
            }
            out += "\n";
            for (const std::string& w : l.why) out += "      its reason: " + w + "\n";
        }
        const SuppressionCheck* last = end != nullptr ? end : checks_.empty() ? nullptr : &checks_.back();
        if (last != nullptr) {
            out += "    The enemy's phase '" + last->phase + "'";
            if (last->threshold < 0.0) {
                out += " does not end on the air defences";
            } else {
                out += " (it moves on below " + Pct(last->threshold) + " of the SAM radars believed working): " + Pct(last->with_battery) +
                       " believed with the battery, " + Pct(last->without_battery) + " without it" +
                       (!last->known ? "; the battery is not in its picture"
                                     : last->HoldsOpen() ? "; the battery holds it open" : "; the battery does not tip it");
            }
            out += "\n";
        }
        if (const AirFrontCheck* af = LastAirFront()) {
            out += "    The enemy's air front in the battery's sector '" + af->sector_id + "': " + Km(af->depth_m) +
                   " beyond the line of contact, " + std::to_string(AirFrontSteps()) + " step" + (AirFrontSteps() == 1 ? "" : "s") + " forward and " +
                   std::to_string(AirFrontPullbacks()) + " back while watched (" + Hours(air_front_h_) + "); held there by the battery's entry " +
                   Hours(air_front_held_h_) + " (its gates open without it)\n";
        }
        return out;
    }

private:
    CoveredAsset* Mutable(const std::string& place) {
        for (CoveredAsset& a : assets_) {
            if (a.place == place) return &a;
        }
        return nullptr;
    }
    static std::string Hours(double h) {
        char b[32];
        std::snprintf(b, sizeof(b), "%.1f h", h);
        return b;
    }
    static std::string Km(double m) {
        char b[64];
        std::snprintf(b, sizeof(b), "%.0f km", m / 1000.0);
        return b;
    }
    static std::string Pct(double x) {
        char b[16];
        std::snprintf(b, sizeof(b), "%.0f %%", 100.0 * x);
        return b;
    }

    std::vector<CoveredAsset> assets_;
    std::vector<PackageLedger> packages_;
    std::vector<SuppressionCheck> checks_;
    AirFrontCheck air_front_first_, air_front_last_;
    int air_front_checks_ = 0;
    int steps_before_ = 0, back_before_ = 0;
    double air_front_h_ = 0.0, air_front_held_h_ = 0.0;
};

}  // namespace campaign
}  // namespace simcore
