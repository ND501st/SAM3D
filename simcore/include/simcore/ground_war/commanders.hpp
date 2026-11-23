// simcore/ground_war/commanders.hpp — the ground war's commanders (track F, F3.4; docs/fronts/03_GROUND_WAR.md section 5).
//
// Commanders are people as the air war's officers are: drawn from a pool per side (airwar::OfficerPoolDef: names,
// backgrounds, the traits' mean and spread) or written by hand (GroundCommanderDef), one per formation at U0 and one per
// headquarters above them (the enemy's army or corps, our 3rd Army, a division whose regiments are the formations).
// Each decides at his next decision time, from FM 100-2-1's reaction and planning times (fm_100_2_1_1984 p. 5-14), or on
// an event (a breakthrough, a request from below, his unit crossing an effectiveness line or a breakpoint); GroundWar
// keeps those times as events in its queue (ground_war.hpp), so nobody polls.
//
// Here: the commander, his draw from the pool, the thresholds his traits move (by at most GroundTables::trait_shift of
// each range, README decision 6) and the side's rules as pure functions the tests check by hand:
//   * a unit's own decision on its strength (DecideUnit): the Pact's and our lines (report under 2/3, ask under 1/2,
//     withdraw only on the senior's order, disperse if no order comes while in contact), NATO's breakpoints (stop an
//     attack, break off a defence, on its own), the rebels' dispersal rather than retreat;
//   * where an echelon goes (EchelonTarget): to the axis that moves (the Pact and ours), else its own, else where its
//     weight brings the ratio to the attack ratio;
//   * our army's answer to a request to withdraw (AnswerRequest) and its counterattack (WantsCounterattack).
// Everything is pure: no state, no allocation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/ground_war/combat.hpp"
#include "simcore/ground_war/ground_data.hpp"
#include "simcore/ground_war/units.hpp"

namespace simcore {
namespace groundwar {

// ------------------------------------------------------------------------------------------------------- the commander

// The echelon a unit's kind decides at (FM 100-2-1 gives the division's, the regiment's and the battalion's times; an
// army, a corps or a rebel group's command decides at the division's).
inline Echelon EchelonOfKind(const std::string& kind) {
    if (kind == "brigade" || kind == "regiment") return Echelon::Regiment;
    if (kind == "battalion") return Echelon::Battalion;
    return Echelon::Division;
}

// The pool's rank key for a unit's kind (the pools name army, division, brigade, regiment, battalion).
inline const char* RankKeyOfKind(const std::string& kind) {
    if (kind == "army" || kind == "corps" || kind == "group") return "army";
    if (kind == "brigade") return "brigade";
    if (kind == "regiment") return "regiment";
    if (kind == "battalion") return "battalion";
    return "division";
}

struct GroundCommander {
    std::string id;
    std::string name;
    std::string rank;
    std::string unit;  // the order of battle's unit he commands
    std::string kind;  // its kind (army, division, brigade, regiment, group)
    std::string background;
    std::string bio;
    int age = 45;
    bool authored = false;
    Side side = Side::Enemy;
    Echelon echelon = Echelon::Division;
    int formation = -1;  // the formation he commands at U0, or -1 (a headquarters above them)
    int senior = -1;     // the commander he answers to, -1 the side's top
    bool top = false;    // the side's highest: his own senior (the plan's or the deployment's author)
    airwar::Traits traits;
    // His clock: the decision queued for him (a stale one in the queue carries an older gen) and how many he made.
    std::uint32_t gen = 0;
    double next_h = -1.0;
    int decisions = 0;

    std::string Title() const { return (rank.empty() ? std::string() : rank + " ") + name; }
};

// A commander drawn from the pool: a name, a background and traits (each normal about the pool's mean, kept in 0..1);
// `rng` is a fork keyed by the commander, so the draw is the same whatever else is drawn.
inline GroundCommander DrawCommander(const airwar::OfficerPoolDef& pool, const std::string& kind, airwar::Rng& rng) {
    GroundCommander c;
    c.kind = kind;
    const auto rank = pool.ranks.find(RankKeyOfKind(kind));
    c.rank = rank == pool.ranks.end() ? std::string() : rank->second;
    const auto pick = [&rng](const std::vector<std::string>& v) { return v.empty() ? std::string() : v[static_cast<std::size_t>(rng.Index(static_cast<int>(v.size())))]; };
    const std::string first = pick(pool.first_names);
    const std::string last = pick(pool.last_names);
    c.name = first.empty() ? last : first + " " + last;
    c.background = pick(pool.backgrounds);
    c.age = 38 + rng.Index(18);
    const auto draw = [&rng, &pool](double mean) { return airwar::Clamp01(rng.Normal(mean, pool.trait_sd)); };
    const airwar::Traits& m = pool.trait_mean;
    c.traits.boldness = draw(m.boldness);
    c.traits.methodical = draw(m.methodical);
    c.traits.creativity = draw(m.creativity);
    c.traits.rigidity = draw(m.rigidity);
    c.traits.ambition = draw(m.ambition);
    c.traits.skepticism = draw(m.skepticism);
    c.traits.vindictiveness = draw(m.vindictiveness);
    c.traits.composure = draw(m.composure);
    c.echelon = EchelonOfKind(kind);
    return c;
}

// ------------------------------------------------------------------------------------------- the thresholds he moves

// A trait's 0..1 as a pull in -1..1.
inline double TraitPull(double trait) { return 2.0 * airwar::Clamp01(trait) - 1.0; }

// The ratio by weighted strength he attacks (or resumes an attack) at: his side's, a bold man lower.
inline double AttackRatioOf(const GroundTables& t, const GroundDoctrineDef& d, const airwar::Traits& tr) {
    ValueRange r = t.attack_ratio_range;
    const double base = d.attack_ratio;
    if (base < r.lo) r.lo = base;
    if (base > r.hi) r.hi = base;
    return ShiftedThreshold(base, r, -TraitPull(tr.boldness), t.trait_shift);
}

// The ratio of his force over the penetration's at which our army commander counterattacks, a bold man lower.
inline double CounterattackRatioOf(const GroundTables& t, const airwar::Traits& tr) {
    return ShiftedThreshold(t.counterattack_ratio.Mid(), t.counterattack_ratio, -TraitPull(tr.boldness), t.trait_shift);
}

// The strength fraction under which a unit of the lines' doctrine asks to withdraw: the ineffective line, a rigid man
// later (lower; never below the line), a flexible one earlier, towards the degraded line.
inline double AskWithdrawLineOf(const GroundTables& t, const EffectivenessDef& e, const airwar::Traits& tr) {
    ValueRange r{e.ineffective_below, e.degraded_below};
    if (r.hi < r.lo) r.hi = r.lo;
    return ShiftedThreshold(e.ineffective_below, r, -TraitPull(tr.rigidity), t.trait_shift);
}

// How long he lets an axis go without advancing before he stops it (no echelon left for it), a rigid man longer.
inline double StallHoursOf(const GroundTables& t, const airwar::Traits& tr) {
    ValueRange r = t.stall_h;
    const double base = t.stall_halt_h;
    if (base < r.lo) r.lo = base;
    if (base > r.hi) r.hi = base;
    return ShiftedThreshold(base, r, TraitPull(tr.rigidity), t.trait_shift);
}

// How long a unit under its line holds in contact without an order before it breaks, a composed man longer.
inline double HoldWithoutOrderHoursOf(const GroundTables& t, const airwar::Traits& tr) {
    return ShiftedThreshold(t.hold_without_order_h.Mid(), t.hold_without_order_h, TraitPull(tr.composure), t.trait_shift);
}

// How long our division holds a security line before ordering the covering force back to the next (vr_krizek_1985's
// 1-2 h a line), drawn at `u` and held longer by a rigid man.
inline double SecurityLineHoursOf(const GroundTables& t, const GroundDoctrineDef& d, const airwar::Traits& tr, double u) {
    const ValueRange& r = d.security_delay_per_line_h;
    return ShiftedThreshold(r.At(airwar::Clamp01(u)), r, TraitPull(tr.rigidity), t.trait_shift);
}

// His reaction (or planning) time, drawn at `u`: a methodical man takes the long end.
inline double DecideHoursOf(const GroundTables& t, Echelon e, bool planning, const airwar::Traits& tr, double u) {
    return DecisionHours(t, e, planning, airwar::Clamp01(u + t.trait_shift * TraitPull(tr.methodical)));
}

// A breakpoint's pull (NATO and the small forces): a composed man fights on to higher losses.
inline double BreakpointPullOf(const airwar::Traits& tr) { return TraitPull(tr.composure); }

// --------------------------------------------------------------------------------------------------------- the rules

// What a unit's commander does about his own unit's strength (docs/fronts/03_GROUND_WAR.md sections 5.2-5.4, 8).
enum class UnitAct : std::uint8_t {
    None,
    Report,      // under the degraded line (2/3): reported to the senior, who replaces it first (the Pact's and ours)
    Ask,         // under his ask line: asks the senior for permission to withdraw
    Withdraw,    // withdraws as a unit (ordered, or for NATO decided), cohesive, to re-form behind
    Disperse,    // breaks: under the line with no order while in contact, past a rebel's breakpoint, or cohesion gone
    StopAttack,  // past its attack breakpoint: stops attacking and holds (NATO and the small forces, no permission)
};

inline const char* UnitActName(UnitAct a) {
    switch (a) {
        case UnitAct::None: return "none";
        case UnitAct::Report: return "report";
        case UnitAct::Ask: return "ask";
        case UnitAct::Withdraw: return "withdraw";
        case UnitAct::Disperse: return "disperse";
        case UnitAct::StopAttack: return "stop_attack";
    }
    return "?";
}

struct UnitSituation {
    bool lines = true;            // the doctrine's kind: lines (the Pact, ours) or breakpoints (NATO, the small forces)
    double fraction = 1.0;        // strength over establishment (as its own reports give it)
    double ask_line = 0.5;        // AskWithdrawLineOf
    double degraded_line = 0.667;
    double attack_stop = 1.0;     // breakpoints: cumulative losses at which an attack stops
    double defence_break = 1.0;   // and a defence breaks
    bool attacking = false;       // attacking on an axis
    bool attack_stopped = false;  // already stopped (breakpoints)
    bool engaged = false;         // fought in the last hour (in contact)
    bool has_order = false;       // an order to withdraw is in hand
    bool asked = false;           // has asked
    double asked_for_h = 0.0;     // hours since it asked
    double hold_h = 6.0;          // HoldWithoutOrderHoursOf
    bool needs_order = true;      // the doctrine's withdraw_needs_order
    bool is_top = false;          // its commander is the side's top: his own senior
    bool disperse_not_retreat = false;  // the rebels
    bool reported = false;        // already reported degraded
};

inline UnitAct DecideUnit(const UnitSituation& s) {
    const double losses = 1.0 - s.fraction;
    const bool own = !s.needs_order || s.is_top;  // may withdraw on his own decision
    if (s.has_order) return UnitAct::Withdraw;
    if (s.lines) {
        if (s.fraction < s.ask_line) {
            if (s.disperse_not_retreat) return UnitAct::Disperse;
            if (own) return UnitAct::Withdraw;
            if (!s.asked) return UnitAct::Ask;
            // Under the line with no order: it breaks once it has held in contact as long as it can.
            return s.engaged && s.asked_for_h >= s.hold_h - 1e-9 ? UnitAct::Disperse : UnitAct::None;
        }
        if (s.fraction < s.degraded_line && !s.reported) return UnitAct::Report;
        return UnitAct::None;
    }
    // Breakpoints: an attack stops on its own; a defence breaks.
    if (s.attacking && !s.attack_stopped && losses >= s.attack_stop) return s.disperse_not_retreat ? UnitAct::Disperse : UnitAct::StopAttack;
    if (losses >= s.defence_break) {
        if (s.disperse_not_retreat) return UnitAct::Disperse;
        if (own) return UnitAct::Withdraw;
        if (!s.asked) return UnitAct::Ask;
        return s.engaged && s.asked_for_h >= s.hold_h - 1e-9 ? UnitAct::Disperse : UnitAct::None;
    }
    return UnitAct::None;
}

// What a senior sees of an axis when he places an echelon.
struct AxisChoice {
    bool open = false;        // attacking, or halted short of its objective (not by our counterattack's hold)
    bool attacking = false;
    double pace_km_day = 0.0;  // its advance over about the last day
    double ratio_with = 0.0;   // the attackers' weighted strength with the echelon over the defenders'
};

// The axis an echelon goes to: to success (the axis that moves fastest, at least `moving`) for the Pact and ours; else its
// own axis while open; failing both, the open axis where its weight brings the ratio highest, if that reaches the attack
// ratio. -1: held back.
inline int EchelonTarget(const AxisChoice* axes, std::size_t n, int own, bool to_success, double moving, double attack_ratio) {
    int best = -1;
    const bool own_ok = own >= 0 && static_cast<std::size_t>(own) < n;
    if (to_success) {
        // Its own axis first if it moves; another only if it moves faster.
        double pace = moving;
        if (own_ok && axes[static_cast<std::size_t>(own)].attacking && axes[static_cast<std::size_t>(own)].pace_km_day >= moving) {
            best = own;
            pace = axes[static_cast<std::size_t>(own)].pace_km_day;
        }
        for (std::size_t a = 0; a < n; ++a) {
            if (!axes[a].attacking || static_cast<int>(a) == own) continue;
            if (best < 0 ? axes[a].pace_km_day < pace : axes[a].pace_km_day <= pace) continue;
            pace = axes[a].pace_km_day;
            best = static_cast<int>(a);
        }
        if (best >= 0) return best;
    } else if (own_ok && axes[static_cast<std::size_t>(own)].open) {
        return own;
    }
    double ratio = attack_ratio;
    for (std::size_t a = 0; a < n; ++a) {
        if (!axes[a].open || axes[a].ratio_with < ratio) continue;
        ratio = axes[a].ratio_with;
        best = static_cast<int>(a);
    }
    return best;
}

// Our army commander's answer to a request to withdraw (docs/fronts/03_GROUND_WAR.md section 5.2: a division below 1/2
// withdraws behind the next when the army commander allows it).
enum class RequestAnswer : std::uint8_t { Permit, Relieve, Refuse };

inline const char* RequestAnswerName(RequestAnswer a) {
    switch (a) {
        case RequestAnswer::Permit: return "permit";
        case RequestAnswer::Relieve: return "relieve";
        case RequestAnswer::Refuse: return "refuse";
    }
    return "?";
}

// `next_behind`: another of ours holds the same ground (the sector's second echelon engaged in depth); `relief`: a
// formation is free to march in. The Pact's senior (`always`) permits: its doctrine withdraws and re-forms the ineffective.
inline RequestAnswer AnswerRequest(bool next_behind, bool relief, bool always) {
    if (always || next_behind) return RequestAnswer::Permit;
    return relief ? RequestAnswer::Relieve : RequestAnswer::Refuse;
}

// Whether our army commander counterattacks a penetration: his force's weighted strength over the penetration's.
inline bool WantsCounterattack(double ratio, double threshold) { return ratio >= threshold; }

}  // namespace groundwar
}  // namespace simcore
