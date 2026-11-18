// simcore/air_war/hot_phase.hpp — how a flight chooses in the air: its faction's hot-phase value moved by its officer and
// its pilot (docs/air_battle/10_FACTIONS.md §7-8, roadmap B11).
//
//   effective = clamp01(faction + d_officer + d_pilot),
//   d_officer = clamp(sum of w x (trait - the pool's mean) + the quirks' steps, +-0.2), at half weight when he does not fly
//   d_pilot   = clamp(experience, currency and nerve, +-0.15)
//
// Traits are centred on the faction's own officer pool, so an average officer of that faction leaves its doctrine as it
// is. Pure: no random draw here (decision 6 of docs/air_battle/README.md); the callers draw from their keyed streams.
// The faction's value is read once per flight at the hand-over to A2 or A1 and kept on the flight (raid_scenario.hpp
// writes the officer's shifts into each aircraft's "reactions"); the abstract resolver does not read it (§7.5: phase
// one changes no abstract number).
//
// The gates (§7.3) are lists, not numbers: `BeyondTaskOffered`, `FreeHuntOffered` take no officer, so no bold or creative
// leader opens one. The latencies (§7.4): `RetaskAirS`, `AlertTakeoffS`.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/officers.hpp"

namespace simcore {
namespace airwar {

// A pilot as the hot phase reads him (§8.3): combat sorties survived, his flying hours against his faction's reference
// (1 = current, capped at 1.2), and his nerve (0..1, 0.5 his faction's own; stress lowers it).
struct HotPilot {
    int combat_sorties = 0;
    double currency = 1.0;
    double nerve = 0.5;
};

// Who moves a flight's values: its officer (the one flying in it at full weight, or his squadron's leader at half: the
// habits he trains), its pilot, and two facts of the moment.
struct HotWho {
    const Officer* leader = nullptr;
    bool leader_flies = true;
    const HotPilot* pilot = nullptr;
    bool control_lost = false;   // the flight has lost its controller (turn_back by role, the controller-bound's step)
    bool grudge_target = false;  // the target is what his grudge is against (vindictiveness and the grudge on opportunity)
};

// The bounds (factions_cases.md §1: "about +-0.2" for an officer, "about +-0.15" for a pilot).
constexpr double kHotOfficerBound = 0.2;
constexpr double kHotPilotBound = 0.15;

namespace hot_phase_detail {

inline double Clamp(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

}  // namespace hot_phase_detail

// The officer's shift on one parameter, before his weight (§8.2's trait weights and §8.4-8.5's quirks; game values).
inline double OfficerShift(HotParam p, const OfficerPoolDef& pool, const Officer& o, bool control_lost = false, bool grudge_target = false) {
    const Traits& t = o.def.traits;
    const Traits& m = pool.trait_mean;
    const double bold = t.boldness - m.boldness, meth = t.methodical - m.methodical, crea = t.creativity - m.creativity;
    const double rigid = t.rigidity - m.rigidity, amb = t.ambition - m.ambition, comp = t.composure - m.composure;
    const double vind = t.vindictiveness - m.vindictiveness;
    const double abort_q = o.Quirk("abort_threshold");  // + presses on, - turns back at the first loss
    double d = 0.0;
    switch (p) {
        case HotParam::Initiative:
            d = 0.3 * crea + 0.1 * bold - 0.3 * rigid - 0.1 * o.Quirk("doctrine_weight") - 0.1 * o.Quirk("control_bound");
            break;
        case HotParam::PlanAdherence:
            d = 0.3 * rigid + 0.2 * meth - 0.2 * crea + 0.1 * o.Quirk("doctrine_weight") + 0.05 * o.Quirk("punctuality");
            break;
        case HotParam::PressOn: d = 0.3 * bold + 0.1 * amb + 0.1 * abort_q + 0.05 * o.Quirk("risk"); break;
        case HotParam::Jettison: d = -0.2 * comp - 0.1 * bold - 0.05 * abort_q; break;
        case HotParam::TurnBack:
            d = -0.3 * comp - 0.1 * bold - 0.1 * abort_q + (control_lost ? 0.15 * o.Quirk("control_bound") : 0.0);
            break;
        case HotParam::Opportunity:
            d = 0.2 * bold + 0.1 * amb - 0.1 * meth + 0.1 * o.Quirk("opportunity");
            if (grudge_target) d += 0.2 * vind + 0.1 * o.Quirk("grudge");
            break;
        case HotParam::RadioDiscipline:
            d = 0.2 * meth - 0.1 * o.Quirk("radio_chatter") - (o.Quirk("comsec_leak") > 0.0 ? 0.2 : 0.0);
            break;
        case HotParam::EmissionDiscipline: d = 0.2 * meth; break;
        case HotParam::AirCombat: d = 0.4 * (o.Effective(o.def.skills.airmanship) - pool.skill_mean.airmanship); break;
        case HotParam::Husbanding:
        case HotParam::Count: break;
    }
    return hot_phase_detail::Clamp(d, -kHotOfficerBound, kHotOfficerBound);
}

// The pilot's shift (§8.3): experience on air combat, currency on air combat and timing, nerve on press-on, turn-back
// and jettison.
inline double PilotShift(HotParam p, const HotPilot& pl) {
    const double exp = std::min(1.0, std::max(0, pl.combat_sorties) / 10.0);
    const double cur = hot_phase_detail::Clamp(0.25 * (std::min(1.2, std::max(0.0, pl.currency)) - 1.0), -0.05, 0.05);
    const double nerve = 0.2 * (hot_phase_detail::Clamp(pl.nerve, 0.0, 1.0) - 0.5);  // +-0.1
    double d = 0.0;
    switch (p) {
        case HotParam::AirCombat: d = 0.1 * exp + cur; break;
        case HotParam::PlanAdherence: d = cur; break;
        case HotParam::PressOn: d = nerve; break;
        case HotParam::TurnBack: d = -nerve; break;
        case HotParam::Jettison: d = -nerve; break;
        default: break;
    }
    return hot_phase_detail::Clamp(d, -kHotPilotBound, kHotPilotBound);
}

// A flight's working value of one parameter (§8.1).
inline double HotPhase(const FactionDef& f, HotParam p, FlightRole role, const OfficerPoolDef& pool, const HotWho& who) {
    const double base = f.hot.Value(p).For(role, who.control_lost);
    double d = 0.0;
    if (who.leader != nullptr) d += (who.leader_flies ? 1.0 : 0.5) * OfficerShift(p, pool, *who.leader, who.control_lost, who.grudge_target);
    if (who.pilot != nullptr) d += PilotShift(p, *who.pilot);
    return Clamp01(base + d);
}
// The doc's form: the officer flying (or his squadron's leader at half weight) and the pilot.
inline double HotPhase(const FactionDef& f, HotParam p, FlightRole role, const OfficerPoolDef& pool, const Officer* leader,
                       const HotPilot* pilot, bool leader_flies = true) {
    HotWho w;
    w.leader = leader;
    w.leader_flies = leader_flies;
    w.pilot = pilot;
    return HotPhase(f, p, role, pool, w);
}

// The officer's part only, as the raid builder hands it to a played flight (the pilot's part is the session's: his
// nerve in "reactions").
inline double HotOfficerShift(const FactionDef& f, HotParam p, FlightRole role, const Officer* leader, bool leader_flies) {
    if (leader == nullptr) return 0.0;
    HotWho w;
    w.leader = leader;
    w.leader_flies = leader_flies;
    return HotPhase(f, p, role, f.pool, w) - Clamp01(f.hot.Value(p).For(role, false));
}

// The reaction delay band, cue to manoeuvre (§7.2, §8.2-8.3): the faction's trained pilot's, an improvised manoeuvre
// slower; his officer's airmanship up to 20 % quicker (or slower below the pool's mean), the pilot's experience up to 30 %.
struct HotDelay {
    double lo_s = 0.0, hi_s = 0.0;
};
inline HotDelay ReactionDelayS(const FactionDef& f, const OfficerPoolDef& pool, const HotWho& who, bool improvised) {
    const HotDelayDef& d = f.hot.reaction_delay;
    double k = 1.0;
    if (who.leader != nullptr) {
        const double mean = pool.skill_mean.airmanship;
        const double above = who.leader->Effective(who.leader->def.skills.airmanship) - mean;
        const double span = above >= 0.0 ? std::max(1e-6, 1.0 - mean) : std::max(1e-6, mean);
        k *= 1.0 - (who.leader_flies ? 0.2 : 0.1) * hot_phase_detail::Clamp(above / span, -1.0, 1.0);
    }
    if (who.pilot != nullptr) k *= 1.0 - 0.3 * std::min(1.0, std::max(0, who.pilot->combat_sorties) / 10.0);
    const double extra = improvised ? d.improvised_extra_s : 0.0;
    return HotDelay{d.lo_s * k + extra, d.hi_s * k + extra};
}

// An airborne flight changed (§7.4): seconds from the decision to change it until the flight has the new task, or < 0
// never. The faction must have posts that may retask in the air; a mission its authority lists is retasked within
// retask_air.delay_s; any other is never retasked with the chance retask_air.never (1: never). A recall-only faction
// retasks nothing (its recall is RecallS). The post's officer bends the delay by his plan-adherence shift (the rigid
// slower, §8.2). `mission`: a MissionKind name or "bai". u_never, u_delay: the caller's keyed draws.
inline double RetaskAirS(const FactionDef& f, const std::string& mission, double u_never, double u_delay, const Officer* post_officer = nullptr) {
    const HotPhaseDef& h = f.hot;
    if (!h.authority.set || h.authority.recall_only || h.authority.retask_by.empty() || !h.retask_air.delay_s.Set()) return -1.0;
    if (!h.authority.RetaskKind(mission) && (h.retask_air.never >= 1.0 || u_never < h.retask_air.never)) return -1.0;
    const C2Band& b = h.retask_air.delay_s;
    double s = b.lo + (b.hi - b.lo) * Clamp01(u_delay);
    if (post_officer != nullptr) s *= 1.0 + OfficerShift(HotParam::PlanAdherence, f.pool, *post_officer);
    return std::max(b.lo * (1.0 - kHotOfficerBound), s);
}
// A recall (the neutral's only act in the air): within retask_air.delay_s; < 0 when its posts may not recall.
inline double RecallS(const FactionDef& f, double u_delay) {
    const HotPhaseDef& h = f.hot;
    bool may = false;
    for (const C2AuthorityDef& a : f.c2.authority) may = may || a.act == "recall";
    if (!may || !h.retask_air.delay_s.Set()) return -1.0;
    return h.retask_air.delay_s.lo + (h.retask_air.delay_s.hi - h.retask_air.delay_s.lo) * Clamp01(u_delay);
}

// A new tasking flown from an alert pool (§7.4): seconds from the order to the wheels up (0 for an airborne pool), < 0 for
// a pool the faction has not. The commander bends it as a post officer does a retask.
inline double AlertTakeoffS(const FactionDef& f, const std::string& pool, double u, const Officer* commander = nullptr) {
    const C2AlertDef* a = f.hot.Alert(pool);
    if (a == nullptr) return -1.0;
    if (a->airborne) return 0.0;
    double s = a->takeoff_s.lo + (a->takeoff_s.hi - a->takeoff_s.lo) * Clamp01(u);
    if (commander != nullptr) s *= 1.0 + OfficerShift(HotParam::PlanAdherence, f.pool, *commander);
    return s;
}

// The gates (§7.3). May a flight of this mission attack outside its task at all (armed reconnaissance, a hunter; NATO's
// fired-on and beyond-the-FSCL cases are the opportunity rules')? No officer can open it.
inline bool BeyondTaskOffered(const FactionDef& f, MissionKind mission) {
    return f.hot.authority.set && f.hot.authority.BeyondTask(MissionKindName(mission));
}
// The free hunt: a flight allowed beyond its task, in the plan's zone, once the doctrine's phase holds the air.
inline bool FreeHuntOffered(const FactionDef& f, MissionKind mission, int phase, bool in_zone) {
    const HotAuthorityDef& a = f.hot.authority;
    return BeyondTaskOffered(f, mission) && a.free_hunt_from_phase >= 0 && phase >= a.free_hunt_from_phase && in_zone;
}

// The officer's hot-phase shifts in words, for the debrief's reveal (§10: AirCampaign::EnemyMind): those of at least
// 0.02, as a flight he leads would feel them.
inline std::string HotShiftsLine(const FactionDef& f, const Officer& o) {
    std::string s;
    for (int i = 0; i < kHotParams; ++i) {
        const HotParam p = static_cast<HotParam>(i);
        const double d = OfficerShift(p, f.pool, o, true, false);
        if (std::fabs(d) < 0.02) continue;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %+.2f", HotParamName(p), d);
        s += (s.empty() ? "" : ", ") + std::string(buf);
    }
    return s;
}

}  // namespace airwar
}  // namespace simcore
