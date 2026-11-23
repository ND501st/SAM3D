// simcore/ground_war/combat.hpp — the sector's combat model (track F, F3.2; docs/fronts/03_GROUND_WAR.md section 4) as
// pure tables: no state, no draws, no allocation. The caller (F3.3) sums each side's power over its units in a contested
// sector, says each side's posture and how long the action has gone on, the ground, the night and the river, and gives
// the day's lognormal draw; ResolveSector returns the force ratio, the opposition it means, each side's losses and the
// attacker's advance over the hours asked.
//
// The norms (docs/research/FRONTS_RESEARCH_2026-11.md section 2): Dupuy's posture factors (ciano_1988_qjm fig. 24);
// FM 101-10-1's daily losses by action (fm_101_10_1_1976 table 5-8), which hold at the reference ratio of 3:1 and are
// scaled away from it by (3/R)^k for the attacker and (R/3)^k for the defender (game values); HERO's advance by
// opposition (hero_1972_oralfore p. IV-2), the opposition read from the ratio (a SAM3D mapping), capped by the faction's
// own norms (mt_kurkov_tank_army, fm_100_5_1982). A river stops the line at its bank until the attacker has a crossing.
//
// Cost: a few comparisons, three pow() and no allocation a call (tests/test_ground_combat.cpp measures it), against the
// README's budget of 20 ms CPU a campaign day for the whole nation.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "simcore/ground_war/ground_data.hpp"

namespace simcore {
namespace groundwar {

inline double PostureFactor(const GroundTables& t, Posture p) { return t.posture[static_cast<std::size_t>(p)]; }

inline bool IsDefence(Posture p) {
    return p == Posture::HastyDefence || p == Posture::PreparedDefence || p == Posture::FortifiedDefence;
}

// The attacker's action against a defender in posture `def` (FM 101-10-1's rows): a position, a fortified zone, a
// meeting engagement against one that also attacks, a pursuit of one that delays or withdraws.
inline LossAction AttackerAction(Posture def) {
    switch (def) {
        case Posture::Attack: return LossAction::AttackMeeting;
        case Posture::HastyDefence:
        case Posture::PreparedDefence: return LossAction::AttackPosition;
        case Posture::FortifiedDefence: return LossAction::AttackFortified;
        case Posture::Delay:
        case Posture::Withdrawal: return LossAction::Pursuit;
    }
    return LossAction::AttackPosition;
}

// The defender's action in posture `def`: holding a sector wider than its doctrine's frontage is "defence of a sector";
// a covering force or security zone fights as one; a delay and a withdrawal are FM 101-10-1's retirement and delay.
inline LossAction DefenderAction(Posture def, bool thin, bool covering) {
    if (covering) return LossAction::Covering;
    switch (def) {
        case Posture::Attack: return LossAction::DefenceMeeting;
        case Posture::HastyDefence:
        case Posture::PreparedDefence:
        case Posture::FortifiedDefence: return thin ? LossAction::DefenceSector : LossAction::DefencePosition;
        case Posture::Delay:
        case Posture::Withdrawal: return LossAction::Delay;
    }
    return LossAction::DefencePosition;
}

// A day's losses for an action at the reference ratio, a fraction of strength: battle (first day or later) + nonbattle.
inline double DailyLoss(const GroundTables& t, LossAction a, int contact_day) {
    const LossRow& r = t.losses[static_cast<std::size_t>(a)];
    return (contact_day <= 0 ? r.battle_first : r.battle_later) + r.nonbattle;
}

// The scale on a side's battle losses at ratio R: (ref/R)^k for the attacker, (R/ref)^k for the defender, clipped.
inline double LossScale(const GroundTables& t, double ratio, bool attacker) {
    if (!(ratio > 0.0)) return attacker ? t.scale_lo : t.scale_hi;
    const double x = attacker ? t.reference_ratio / ratio : ratio / t.reference_ratio;
    return std::min(t.scale_hi, std::max(t.scale_lo, std::pow(x, t.ratio_exponent)));
}

// The share of strength lost over `hours` at a daily rate, compounded: 1 - (1 - daily)^(hours/24). Closed form, so a
// quiet sector takes its days at once when it is next touched (README, efficiency 3).
inline double LossOverHours(double daily, double hours) {
    if (!(daily > 0.0) || !(hours > 0.0)) return 0.0;
    const double d = std::min(daily, 1.0);
    return d >= 1.0 ? 1.0 : 1.0 - std::pow(1.0 - d, hours / 24.0);
}

// HERO's opposition at ratio R (the table's thresholds; Stalled below the first above 0).
inline Opposition OppositionOf(const GroundTables& t, double ratio) {
    std::size_t k = 0;
    for (std::size_t i = 1; i < kAdvanceClassCount; ++i) k = ratio >= t.class_from_ratio[i] ? i : k;
    return static_cast<Opposition>(k);
}

inline double AdvanceKmPerDay(const GroundTables& t, Opposition o) {
    return o == Opposition::Unopposed ? t.unopposed_km_per_day : t.class_km_per_day[static_cast<std::size_t>(o)];
}

// The faction's cap on a day's advance: breaking through a defence, in the depth against a delay, withdrawal or
// meeting engagement, or against a broken enemy or none.
inline double AdvanceCap(const AdvanceCaps& c, Posture def, bool defender_broken) {
    if (defender_broken) return c.broken_km_per_day;
    return IsDefence(def) ? c.breakthrough_km_per_day : c.depth_km_per_day;
}

// Hours to cross a river where the bridge is intact, or cut (or none), the engineers' pontoon park shortening the second.
inline double CrossingHours(const GroundTables& t, bool bridge_intact, bool pontoon_park) {
    if (bridge_intact) return t.crossing_intact_h;
    return t.crossing_cut_h * (pontoon_park ? t.pontoon_factor : 1.0);
}

// FM 101-10-1 table 7-7: the share of a division's operational tanks lost on day `day` (1-3) of a commitment, of those
// on hand at the start of day 1; 0 outside the table's three days (the caller falls back on the personnel rate).
inline double TankLossShare(const GroundTables& t, TankCommitment c, int day) {
    if (day < 1 || day > 3) return 0.0;
    return t.tank_losses[static_cast<std::size_t>(c)][static_cast<std::size_t>(day - 1)];
}

// A commander's reaction or planning time at his echelon, drawn at `u` in [0, 1] (fm_100_2_1_1984 p. 5-14).
inline double DecisionHours(const GroundTables& t, Echelon e, bool planning, double u) {
    const ValueRange& r = planning ? t.planning_h[static_cast<std::size_t>(e)] : t.reaction_h[static_cast<std::size_t>(e)];
    return r.At(std::min(1.0, std::max(0.0, u)));
}

// One side of a contested sector.
struct SectorSide {
    double power = 0.0;  // the sum of its units' UnitPower (strength x supply x cohesion), before posture and ground
    Posture posture = Posture::Attack;
    int contact_day = 0;    // whole days this action has gone on (0: its first day)
    bool thin = false;      // a defender holding more than its doctrine's frontage
    bool covering = false;  // a covering force or security zone
    bool broken = false;    // ineffective or past its breakpoint
};

struct SectorConditions {
    Terrain terrain = Terrain::Open;  // the defender's ground
    bool night = false;
    bool poor_visibility = false;
    bool river_line = false;  // the defender holds a river line the attacker has not crossed
    double river_ahead_km = std::numeric_limits<double>::infinity();  // how far the attacker is from that river's bank
};

struct SectorResult {
    double ratio = 0.0;  // attacker's power over the defender's, both with their factors (infinity with no defender)
    Opposition opposition = Opposition::Stalled;
    double attacker_daily = 0.0;  // the day's loss rates after the ratio's scale, fractions of strength
    double defender_daily = 0.0;
    double attacker_loss = 0.0;  // the share of strength lost over the hours asked
    double defender_loss = 0.0;
    double advance_km_per_day = 0.0;  // the table's rate x the ground x the draw, then the faction's cap
    double advance_km = 0.0;          // over the hours asked, stopped at a river's bank
    bool at_river = false;            // the advance was stopped at the bank
};

// The sector's hour (or hours). `att` attacks (its posture Attack); if it does not, the sector is inactive: both take the
// inactive rate and nobody moves. `draw` is the sector's day's lognormal factor on the advance (1: the table's rate).
inline SectorResult ResolveSector(const GroundTables& t, const SectorSide& att, const SectorSide& def, const SectorConditions& c,
                                  const AdvanceCaps& caps, double draw, double hours) {
    SectorResult r;
    if (att.posture != Posture::Attack || !(att.power > 0.0)) {
        r.attacker_daily = DailyLoss(t, LossAction::Inactive, att.contact_day);
        r.defender_daily = DailyLoss(t, LossAction::Inactive, def.contact_day);
        r.attacker_loss = LossOverHours(r.attacker_daily, hours);
        r.defender_loss = LossOverHours(r.defender_daily, hours);
        return r;
    }
    const std::size_t terrain = static_cast<std::size_t>(c.terrain);
    // The table's rate on this ground and this day, then the faction's cap (a norm the draw never exceeds).
    const double spread = t.terrain_advance[terrain] * std::max(0.0, draw);
    if (!(def.power > 0.0)) {
        r.ratio = std::numeric_limits<double>::infinity();
        r.opposition = Opposition::Unopposed;
        r.attacker_daily = t.losses[static_cast<std::size_t>(LossAction::Pursuit)].nonbattle;  // nobody fires on it
        r.advance_km_per_day = std::min(t.unopposed_km_per_day * spread, caps.broken_km_per_day);
    } else {
        const double att_power = att.power * (c.night ? t.night_attacker : 1.0) * (c.poor_visibility ? t.poor_visibility_attacker : 1.0);
        const double def_power =
            def.power * PostureFactor(t, def.posture) * t.terrain_defender[terrain] * (c.river_line ? t.river_line_defender : 1.0);
        r.ratio = att_power / def_power;
        r.opposition = OppositionOf(t, r.ratio);
        const LossRow& a = t.losses[static_cast<std::size_t>(AttackerAction(def.posture))];
        const LossRow& d = t.losses[static_cast<std::size_t>(DefenderAction(def.posture, def.thin, def.covering))];
        r.attacker_daily = (att.contact_day <= 0 ? a.battle_first : a.battle_later) * LossScale(t, r.ratio, true) + a.nonbattle;
        r.defender_daily = (def.contact_day <= 0 ? d.battle_first : d.battle_later) * LossScale(t, r.ratio, false) + d.nonbattle;
        r.advance_km_per_day = std::min(AdvanceKmPerDay(t, r.opposition) * spread, AdvanceCap(caps, def.posture, def.broken));
    }
    r.attacker_loss = LossOverHours(r.attacker_daily, hours);
    r.defender_loss = LossOverHours(r.defender_daily, hours);
    r.advance_km = r.advance_km_per_day * std::max(0.0, hours) / 24.0;
    if (c.river_line && r.advance_km >= c.river_ahead_km) {
        r.advance_km = std::max(0.0, c.river_ahead_km);
        r.at_river = true;
    }
    return r;
}

}  // namespace groundwar
}  // namespace simcore
