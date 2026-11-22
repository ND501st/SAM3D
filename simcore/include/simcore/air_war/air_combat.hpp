// simcore/air_war/air_combat.hpp — the shot as a chain of chances (roadmap track B, B7; docs/air_battle/07_AIR_TO_AIR.md
// section 3). Pure functions over the data in air_combat_types.hpp: nothing here holds state, reads a clock or draws from
// a shared stream.
//
//     P(kill) = P(in envelope) x P(works) x P(guides) x P(lethal)
//
//  - In envelope: the pilot fires when he BELIEVES he is inside the missile's zone; his belief of range, aspect, the
//    seeker's angle and his own load carries an error that shrinks with skill and grows with stress (Believe,
//    PilotFires). A launch outside the TRUE zone fails (CheckEnvelope): out-of-parameter launches come from judgement,
//    not from a separate roll.
//  - Works: the round's reliability after carriage, and the switch error of a pilot changing mode.
//  - Guides: a base by guidance class, then multipliers for the aspect, the sun, the ground behind the target, a
//    countermeasure, the target's break and, for a semi-active round, the launcher's lock held to impact.
//  - Lethal: the designers' kill chance as the ceiling, cut by the miss a late break adds against the warhead's lethal
//    radius.
//
// Every draw is keyed (em::DrawHash(seed, Stream::AirCombat, shooter, shot, link)): the same shot gives the same answer
// whatever order shots are resolved in. The numbers are the data's `calibration.chain`, game values held to the period's
// rates by SimulateLaunches (test_air_combat.cpp): 11.1 % of AIM-7s and 18.6 % of AIM-9s hit, 35 % and 26 % launched
// outside the permitted parameters, for the crews of 1967 (checo_1967_a2a p. 44).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "simcore/air_war/air_combat_types.hpp"
#include "simcore/em/random.hpp"

namespace simcore {
namespace airwar {

// The shooter: skill 0..1, stress 0..1 (what he heard and saw: the crew's stress model), and whether he changed mode
// (after hard manoeuvring, or from ground attack to air-to-air) just before the shot.
struct Pilot {
    double skill = 0.5;
    double stress = 0.0;
    bool mode_change = false;
};

// The shot's geometry at launch.
struct ShotGeometry {
    double range_m = 0.0;
    double aspect_rad = 0.0;          // the angle off the target's tail: 0 from astern, pi head-on
    double off_boresight_rad = 0.0;   // the target's angle off the nose (or the radar's boresight)
    double altitude_m = 0.0;          // the shooter's height
    double launch_g = 1.0;            // the shooter's load
    bool look_down = false;           // the target is below the shooter against the ground
    double sun_angle_rad = 3.14159265358979323846;  // between the line of sight and the sun; pi: no sun in the seeker's view
};

// What the target and the sky do.
struct ShotConditions {
    double target_g = 1.0;                // the target's load
    double break_timing = 0.0;            // 0: no break; 1: a break at the right moment (the last seconds)
    double countermeasures = 0.0;         // 0..1: flares in the seeker's cone (infrared), chaff or jamming (semi-active)
    double lock_lost_before_impact_s = 0.0;  // a semi-active round: the launcher's lock lost this long before impact (0: held)
    int end_notch = -1;  // roadmap S3, a flown round: 1 the target in the beam against the ground at its end, 0 not; -1 the launch's geometry
};

enum class MissReason {
    None,          // it killed
    TooClose,      // the launch was inside the minimum range
    TooFar,        // ... beyond the maximum for this aspect and height
    Aspect,        // an uncooled seeker fired from the front
    OffBoresight,  // the target outside the seeker's angle
    LaunchG,       // the launcher pulled more than the missile allows
    Dud,           // the round did not work (maintenance, wear)
    SwitchError,   // a wrong switch after a change of mode
    SeekerFailure, // the seeker did not lock or guide, for no particular reason
    Sun,           // the sun in the infrared seeker
    Clutter,       // the ground behind the target
    Countermeasure,// flares, chaff or jamming
    Break,         // the target's break
    LockLost,      // the launcher lost its lock, beyond the missile's own flight
    Fuze,          // it guided and did not kill (the fuze, the warhead, a late break's miss)
    Energy         // it ran out of energy before it reached a target that turned away (roadmap S3: flown missiles only)
};
constexpr int kMissReasonCount = 16;

inline const char* MissReasonName(MissReason r) {
    switch (r) {
        case MissReason::None: return "none";
        case MissReason::TooClose: return "too_close";
        case MissReason::TooFar: return "too_far";
        case MissReason::Aspect: return "aspect";
        case MissReason::OffBoresight: return "off_boresight";
        case MissReason::LaunchG: return "launch_g";
        case MissReason::Dud: return "dud";
        case MissReason::SwitchError: return "switch_error";
        case MissReason::SeekerFailure: return "seeker_failure";
        case MissReason::Sun: return "sun";
        case MissReason::Clutter: return "clutter";
        case MissReason::Countermeasure: return "countermeasure";
        case MissReason::Break: return "break";
        case MissReason::LockLost: return "lock_lost";
        case MissReason::Fuze: return "fuze";
        case MissReason::Energy: return "energy";
    }
    return "?";
}

// A launch outside the permitted parameters (the first five reasons).
inline bool IsOutOfEnvelope(MissReason r) {
    return r == MissReason::TooClose || r == MissReason::TooFar || r == MissReason::Aspect || r == MissReason::OffBoresight ||
           r == MissReason::LaunchG;
}

// The links of one shot, each a chance 0..1 (in_envelope is 0 or 1 for the truth).
struct ShotLinks {
    double in_envelope = 0.0;
    double works = 0.0;
    double guides = 0.0;
    double lethal = 0.0;
    MissReason envelope_reason = MissReason::None;
    // What lowers `guides`, as the loss (-ln of the multiplier) of each cause; infinity for a certain failure.
    double loss_base = 0.0, loss_aspect = 0.0, loss_sun = 0.0, loss_clutter = 0.0, loss_cm = 0.0, loss_break = 0.0, loss_lock = 0.0;
    double switch_error = 0.0;  // of the round working's chance lost to a wrong switch

    double Product() const { return in_envelope * works * guides * lethal; }
};

struct ShotResult {
    bool kill = false;
    MissReason reason = MissReason::None;
    ShotLinks links;
};

namespace chain_detail {

constexpr double kPi = 3.14159265358979323846;

inline double Clamp(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

// The standard normal's distribution function.
inline double NormalCdf(double x) { return 0.5 * std::erfc(-x / 1.4142135623730951); }

inline double UniformOf(std::uint64_t seed, std::uint64_t shooter, std::uint64_t shot, std::uint64_t link) {
    return em::Uniform01(em::DrawHash(seed, em::Stream::AirCombat, shooter, shot, link));
}
inline double NormalOf(std::uint64_t seed, std::uint64_t shooter, std::uint64_t shot, std::uint64_t link) {
    return em::StandardNormal(em::DrawHash(seed, em::Stream::AirCombat, shooter, shot, link));
}

// The links' keys.
enum Link : std::uint64_t {
    kJudgeRange = 1, kJudgeAspect = 2, kJudgeCone = 3, kJudgeG = 4,
    kWorks = 10, kWorksWhy = 11, kGuides = 12, kGuidesWhy = 13, kLethal = 14, kGun = 15
};

inline double LossOf(double multiplier) {
    if (multiplier <= 0.0) return 1.0e300;
    return multiplier >= 1.0 ? 0.0 : -std::log(multiplier);
}

}  // namespace chain_detail

// ------------------------------------------------------------------------------------------------ the envelope

// The most the missile reaches at this aspect and height: the rear and head-on maxima blended by aspect, then the
// share left at low level (the R-60 keeps a third at the ground).
inline double MaxRangeM(const AamDef& a, const ChainParams& c, double aspect_rad, double altitude_m) {
    using chain_detail::Clamp;
    const double t = Clamp(aspect_rad / chain_detail::kPi, 0.0, 1.0);
    const double r = a.max_rear_m + (a.max_head_m - a.max_rear_m) * t;
    const double h = Clamp(altitude_m / c.alt_ref_m, 0.0, 1.0);
    return r * (a.low_factor + (1.0 - a.low_factor) * h);
}

struct EnvelopeCheck {
    bool ok = false;
    MissReason reason = MissReason::None;
};

// Is a launch with this geometry inside the missile's zone? (On the truth for a result; on the pilot's belief for his
// decision to fire.)
inline EnvelopeCheck CheckEnvelope(const AamDef& a, const ChainParams& c, const ShotGeometry& g) {
    EnvelopeCheck e;
    if (a.guidance == Guidance::IrRear && g.aspect_rad > c.rear_aspect_limit_rad) {
        e.reason = MissReason::Aspect;
    } else if (g.off_boresight_rad > a.seeker_cone_rad) {
        e.reason = MissReason::OffBoresight;
    } else if (g.launch_g > a.launch_g) {
        e.reason = MissReason::LaunchG;
    } else if (g.range_m < a.min_m) {
        e.reason = MissReason::TooClose;
    } else if (g.range_m > MaxRangeM(a, c, g.aspect_rad, g.altitude_m)) {
        e.reason = MissReason::TooFar;
    } else {
        e.ok = true;
    }
    return e;
}

// ------------------------------------------------------------------------------------------------ judgement

// The spread of the pilot's judgement, as a multiple of the chain's sigmas.
inline double JudgementScale(const ChainParams& c, const Pilot& p, Guidance g) {
    const double s = (1.0 - c.judge_skill_gain * chain_detail::Clamp(p.skill, 0.0, 1.0)) *
                     (1.0 + c.judge_stress_gain * chain_detail::Clamp(p.stress, 0.0, 1.0));
    return IsSemiActive(g) ? s * c.judge_sarh_scale : s;
}

// What the pilot believes the geometry to be: the truth with the error of his judgement, from keyed draws.
inline ShotGeometry Believe(const ShotGeometry& truth, const ChainParams& c, const Pilot& p, Guidance guidance, std::uint64_t seed,
                            std::uint64_t shooter, std::uint64_t shot) {
    using namespace chain_detail;
    const double s = JudgementScale(c, p, guidance);
    ShotGeometry b = truth;
    b.range_m = truth.range_m * std::exp(c.judge_range_sigma * s * NormalOf(seed, shooter, shot, kJudgeRange));
    b.aspect_rad = Clamp(truth.aspect_rad + c.judge_aspect_sigma_rad * s * NormalOf(seed, shooter, shot, kJudgeAspect), 0.0, kPi);
    b.off_boresight_rad = std::fabs(truth.off_boresight_rad + c.judge_cone_sigma_rad * s * NormalOf(seed, shooter, shot, kJudgeCone));
    b.launch_g = std::max(1.0, truth.launch_g * std::exp(c.judge_g_sigma * s * NormalOf(seed, shooter, shot, kJudgeG)));
    return b;
}

// Does the pilot, believing `b`, take the shot?
inline bool PilotFires(const AamDef& a, const ChainParams& c, const ShotGeometry& believed) { return CheckEnvelope(a, c, believed).ok; }

// The chance a pilot who believes `b` is in fact inside the zone (his error as the normal spread of the judgement): the
// planner's and the abstract fight's expectation. Range, aspect, angle and load are taken as independent.
inline double EnvelopeProbability(const AamDef& a, const ChainParams& c, const ShotGeometry& believed, const Pilot& p) {
    using namespace chain_detail;
    const double s = JudgementScale(c, p, a.guidance);
    double pr = 1.0;
    const double sr = std::max(1e-9, c.judge_range_sigma * s * believed.range_m);
    const double hi = MaxRangeM(a, c, believed.aspect_rad, believed.altitude_m);
    pr *= NormalCdf((hi - believed.range_m) / sr) - NormalCdf((a.min_m - believed.range_m) / sr);
    if (a.guidance == Guidance::IrRear) {
        const double sa = std::max(1e-9, c.judge_aspect_sigma_rad * s);
        pr *= NormalCdf((c.rear_aspect_limit_rad - believed.aspect_rad) / sa);
    }
    const double sc = std::max(1e-9, c.judge_cone_sigma_rad * s);
    pr *= NormalCdf((a.seeker_cone_rad - believed.off_boresight_rad) / sc) - NormalCdf((-a.seeker_cone_rad - believed.off_boresight_rad) / sc);
    const double sg = std::max(1e-9, c.judge_g_sigma * s * believed.launch_g);
    pr *= NormalCdf((a.launch_g - believed.launch_g) / sg);
    return Clamp(pr, 0.0, 1.0);
}

// ------------------------------------------------------------------------------------------------ the links

// A wrong switch after a change of mode (the F-105 pilots' complaint, the 1972 CHECO report).
inline double SwitchErrorChance(const ChainParams& c, const Pilot& p) {
    using chain_detail::Clamp;
    const double mode = p.mode_change ? 1.0 : c.switch_idle_share;
    return Clamp(c.switch_error_base * mode * (1.0 - c.switch_skill_gain * Clamp(p.skill, 0.0, 1.0)) *
                     (1.0 + c.switch_stress_gain * Clamp(p.stress, 0.0, 1.0)),
                 0.0, 1.0);
}

inline double GuidesBase(const ChainParams& c, Guidance g) {
    switch (g) {
        case Guidance::IrRear: return c.guides_ir_rear;
        case Guidance::IrAllAspect: return c.guides_ir_all_aspect;
        case Guidance::Sarh: return c.guides_sarh;
        case Guidance::SarhInertial: return c.guides_sarh_inertial;
    }
    return 0.0;
}

// The share of a perfectly timed break at the missile's limit that a break of this load and timing reaches.
inline double BreakEffectiveness(const AamDef& a, const ShotConditions& k) {
    return chain_detail::Clamp(k.break_timing, 0.0, 1.0) * chain_detail::Clamp(k.target_g / a.target_g, 0.0, 1.0);
}

// The links of a shot at the true geometry: the expectation of ResolveShot over its draws.
inline ShotLinks ShotOdds(const AamDef& a, const ChainParams& c, const ShotGeometry& g, const ShotConditions& k, const Pilot& p) {
    using namespace chain_detail;
    ShotLinks l;
    const EnvelopeCheck env = CheckEnvelope(a, c, g);
    l.in_envelope = env.ok ? 1.0 : 0.0;
    l.envelope_reason = env.reason;
    l.switch_error = SwitchErrorChance(c, p);
    l.works = a.reliability * (1.0 - l.switch_error);

    const double base = GuidesBase(c, a.guidance);
    // Aspect: an uncooled seeker is weaker toward the edge of its hemisphere; an all-aspect one head-on.
    double f_aspect = 1.0;
    if (a.guidance == Guidance::IrRear) {
        f_aspect = 1.0 - (1.0 - c.aspect_ir_rear_edge) * Clamp(g.aspect_rad / c.rear_aspect_limit_rad, 0.0, 1.0);
    } else if (a.guidance == Guidance::IrAllAspect) {
        f_aspect = 1.0 - (1.0 - c.aspect_ir_all_head) * Clamp(g.aspect_rad / kPi, 0.0, 1.0);
    }
    // The sun in an infrared seeker.
    double f_sun = 1.0;
    if (IsInfrared(a.guidance) && a.sun_exclusion_rad > 0.0) {
        const double edge = a.sun_exclusion_rad * c.sun_soft_factor;
        if (g.sun_angle_rad < a.sun_exclusion_rad) {
            f_sun = 0.0;
        } else if (g.sun_angle_rad < edge) {
            f_sun = c.sun_soft_floor + (1.0 - c.sun_soft_floor) * (g.sun_angle_rad - a.sun_exclusion_rad) / (edge - a.sun_exclusion_rad);
        }
    }
    // The ground behind the target; a radar round is weaker still in the beam against it.
    double f_clutter = 1.0;
    if (g.look_down || k.end_notch == 1) {
        f_clutter = a.clutter_below;
        const bool beam = k.end_notch >= 0 ? k.end_notch == 1 : std::fabs(g.aspect_rad - kPi / 2.0) < c.beam_notch_half_rad;
        if (IsSemiActive(a.guidance) && beam) f_clutter *= c.beam_notch;
    }
    const double f_cm = 1.0 - Clamp(k.countermeasures, 0.0, 1.0) * (1.0 - a.flare_rejection);
    const double eff = BreakEffectiveness(a, k);
    const double f_break = 1.0 - c.break_max * eff;
    // The launcher's lock held to impact (a radar round).
    double f_lock = 1.0;
    if (IsSemiActive(a.guidance) && k.lock_lost_before_impact_s > 0.0) f_lock = k.lock_lost_before_impact_s <= a.autonomous_s ? 1.0 : 0.0;

    l.loss_base = LossOf(base);
    l.loss_aspect = LossOf(f_aspect);
    l.loss_sun = LossOf(f_sun);
    l.loss_clutter = LossOf(f_clutter);
    l.loss_cm = LossOf(f_cm);
    l.loss_break = LossOf(f_break);
    l.loss_lock = LossOf(f_lock);
    l.guides = base * f_aspect * f_sun * f_clutter * f_cm * f_break * f_lock;

    // The last link: the design figure as the ceiling, cut by the miss a break adds against the lethal radius.
    const double nominal = c.nominal_miss_m;
    const double broken = std::sqrt(nominal * nominal + (c.break_miss_m * eff) * (c.break_miss_m * eff));
    const double hit_nominal = 1.0 - std::exp(-a.lethal_m * a.lethal_m / (2.0 * nominal * nominal));
    const double hit_broken = 1.0 - std::exp(-a.lethal_m * a.lethal_m / (2.0 * broken * broken));
    l.lethal = a.design_pk * (hit_nominal > 0.0 ? hit_broken / hit_nominal : 1.0);
    return l;
}

// The cause of a failed guidance draw: chosen among the losses in proportion to them, by a keyed draw. A certain
// failure (a loss of 1e300: the sun inside the exclusion, a lock lost for good) takes it all.
inline MissReason GuidanceCause(const ShotLinks& l, double u) {
    struct Cause {
        double loss;
        MissReason reason;
    };
    const Cause causes[] = {{l.loss_base, MissReason::SeekerFailure}, {l.loss_aspect, MissReason::SeekerFailure}, {l.loss_sun, MissReason::Sun},
                            {l.loss_clutter, MissReason::Clutter},    {l.loss_cm, MissReason::Countermeasure},   {l.loss_break, MissReason::Break},
                            {l.loss_lock, MissReason::LockLost}};
    double total = 0.0;
    for (const Cause& c : causes) total += c.loss >= 1.0e299 ? 1.0e300 : c.loss;
    if (total <= 0.0) return MissReason::SeekerFailure;
    if (total >= 1.0e299) {
        for (const Cause& c : causes) {
            if (c.loss >= 1.0e299) return c.reason;
        }
    }
    double at = u * total;
    for (const Cause& c : causes) {
        if (at < c.loss) return c.reason;
        at -= c.loss;
    }
    return MissReason::SeekerFailure;
}

// One shot resolved: each link its own keyed draw, so the answer does not depend on the order shots are resolved in.
inline ShotResult ResolveShot(const AamDef& a, const ChainParams& c, const ShotGeometry& g, const ShotConditions& k, const Pilot& p,
                              std::uint64_t seed, std::uint64_t shooter, std::uint64_t shot) {
    using namespace chain_detail;
    ShotResult r;
    r.links = ShotOdds(a, c, g, k, p);
    if (r.links.in_envelope <= 0.0) {
        r.reason = r.links.envelope_reason;
        return r;
    }
    if (UniformOf(seed, shooter, shot, kWorks) >= r.links.works) {
        const double dud = 1.0 - a.reliability;
        const double sw = a.reliability * r.links.switch_error;
        r.reason = UniformOf(seed, shooter, shot, kWorksWhy) * (dud + sw) < dud ? MissReason::Dud : MissReason::SwitchError;
        return r;
    }
    if (UniformOf(seed, shooter, shot, kGuides) >= r.links.guides) {
        r.reason = GuidanceCause(r.links, UniformOf(seed, shooter, shot, kGuidesWhy));
        return r;
    }
    if (UniformOf(seed, shooter, shot, kLethal) >= r.links.lethal) {
        r.reason = MissReason::Fuze;
        return r;
    }
    r.kill = true;
    return r;
}

// ------------------------------------------------------------------------------------------------ guns

// A gun pass: how long the target is inside the gun's range and angle (the firing window), at what range, and the load
// the target pulls.
struct GunPass {
    double range_m = 0.0;     // the mean range of the burst
    double window_s = 0.0;    // seconds the target is inside the range and angle
    double target_g = 1.0;
};

struct GunResult {
    bool kill = false;
    MissReason reason = MissReason::None;  // TooFar (out of range), TooClose (no window), Fuze (the burst missed)
    double chance = 0.0;
};

// P(kill) = 1 - exp(-kill_per_s x seconds of fire x aim): aim falls with range and the target's load and grows with
// skill (07 section 3.3).
inline double GunKillChance(const AirGunDef& gun, const ChainParams& c, const GunPass& pass, const Pilot& p) {
    using chain_detail::Clamp;
    if (pass.range_m > gun.effective_m || pass.window_s <= 0.0) return 0.0;
    const double seconds = std::min(pass.window_s, gun.firing_s);
    const double skill = c.gun_skill_floor + (1.0 - c.gun_skill_floor) * Clamp(p.skill, 0.0, 1.0);
    const double range = 1.0 - (1.0 - c.gun_range_edge) * Clamp(pass.range_m / gun.effective_m, 0.0, 1.0);
    const double manoeuvre = 1.0 - c.gun_g_loss * Clamp((pass.target_g - 1.0) / (c.gun_g_full - 1.0), 0.0, 1.0);
    return 1.0 - std::exp(-gun.kill_per_s * seconds * skill * range * manoeuvre);
}

inline GunResult ResolveGunPass(const AirGunDef& gun, const ChainParams& c, const GunPass& pass, const Pilot& p, std::uint64_t seed,
                                std::uint64_t shooter, std::uint64_t shot) {
    GunResult r;
    r.chance = GunKillChance(gun, c, pass, p);
    if (pass.range_m > gun.effective_m) {
        r.reason = MissReason::TooFar;
    } else if (pass.window_s <= 0.0) {
        r.reason = MissReason::TooClose;
    } else if (chain_detail::UniformOf(seed, shooter, shot, chain_detail::kGun) < r.chance) {
        r.kill = true;
    } else {
        r.reason = MissReason::Fuze;
    }
    return r;
}

// ------------------------------------------------------------------------------------------------ calibration

// What a run of launches came to.
struct LaunchStats {
    int launches = 0;
    int kills = 0;
    int reasons[kMissReasonCount] = {};

    int OutOfEnvelope() const {
        int n = 0;
        for (int i = 0; i < kMissReasonCount; ++i) n += IsOutOfEnvelope(static_cast<MissReason>(i)) ? reasons[i] : 0;
        return n;
    }
    double HitRate() const { return launches > 0 ? static_cast<double>(kills) / launches : 0.0; }
    double OutOfEnvelopeShare() const { return launches > 0 ? static_cast<double>(OutOfEnvelope()) / launches : 0.0; }
};

// `launches` shots by a pilot of this skill at the calibration's engagement mix: the true geometry drawn from the mix,
// the pilot firing when his belief puts him in the zone (a belief outside it, he waits: the next draw), the shot then
// resolved on the truth. This is how the data's chain is held to 11.1 % and 18.6 % per launch and 35 % and 26 % outside
// the permitted parameters. `shooter_base` keys the run.
inline LaunchStats SimulateLaunches(const AamDef& a, const CalibrationDef& cal, const Pilot& pilot, int launches, std::uint64_t seed,
                                    std::uint64_t shooter_base = 0) {
    using namespace chain_detail;
    const ChainParams& c = cal.chain;
    const CalibrationMix& m = cal.mix;
    LaunchStats st;
    for (int i = 0; i < launches; ++i) {
        const std::uint64_t shooter = shooter_base + static_cast<std::uint64_t>(i);
        ShotGeometry truth;
        ShotConditions cond;
        Pilot p = pilot;
        bool fired = false;
        for (std::uint64_t attempt = 0; attempt < 400 && !fired; ++attempt) {
            const std::uint64_t shot = attempt;
            auto u = [&](std::uint64_t link) { return UniformOf(seed, shooter, shot, 100 + link); };
            auto z = [&](std::uint64_t link) { return NormalOf(seed, shooter, shot, 100 + link); };
            truth = ShotGeometry();
            truth.range_m = m.range_median_m * std::exp(m.range_sigma_ln * z(1));
            truth.aspect_rad = u(2) < m.aspect_rear_share ? u(3) * kPi / 2.0 : kPi / 2.0 + u(4) * kPi / 2.0;
            truth.off_boresight_rad = std::fabs(m.off_boresight_sigma_rad * z(5));
            truth.altitude_m = m.altitude_m;
            truth.launch_g = 1.0 - (m.launch_g_mean - 1.0) * std::log(std::max(1e-12, 1.0 - u(6)));
            truth.look_down = u(7) < m.look_down_share;
            truth.sun_angle_rad = u(8) < m.sun_share ? u(9) * 60.0 * kPi / 180.0 : kPi;
            cond = ShotConditions();
            if (u(10) < m.break_share) {
                cond.target_g = m.break_g;
                cond.break_timing = u(11);
            }
            cond.countermeasures = u(12) < m.countermeasure_share ? 0.7 : 0.0;
            p.mode_change = u(13) < m.mode_change_share;
            const ShotGeometry belief = Believe(truth, c, p, a.guidance, seed, shooter, 1000 + shot);
            fired = PilotFires(a, c, belief);
        }
        const ShotResult r = ResolveShot(a, c, truth, cond, p, seed, shooter, 0);
        ++st.launches;
        if (r.kill) ++st.kills;
        ++st.reasons[static_cast<int>(r.reason)];
    }
    return st;
}

}  // namespace airwar
}  // namespace simcore
