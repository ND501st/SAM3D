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
//     phase's threshold).
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
    l.escorts = p.Count(FlightRole::Escort);
    l.arms_fired = o.arms_fired;
    for (const airwar::LossRecord& loss : o.losses) (loss.by == own_object ? l.downed_by_battery : l.downed_by_others) += 1;
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
    static std::string Pct(double x) {
        char b[16];
        std::snprintf(b, sizeof(b), "%.0f %%", 100.0 * x);
        return b;
    }

    std::vector<CoveredAsset> assets_;
    std::vector<PackageLedger> packages_;
    std::vector<SuppressionCheck> checks_;
};

}  // namespace campaign
}  // namespace simcore
