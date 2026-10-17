// simcore/crew_decisions.hpp — the drill master's evaluation of the decisions that aren't in the console's graph
// (roadmap 8.3, docs/crew/06_REFERENCE_PERFORMER.md §3.6): which target first and when to launch, scored with the
// gameplay outcome model's own numbers (engagement.hpp: the firing zones, the class's hit chance, the low-altitude
// factor, the missile's average speed). Like a chess engine's evaluation: deterministic, explainable, on straight-line
// tracks (what a crew extrapolates from its scope).
//
// The rules a source states win over the score: a friend is never chosen; the first missile goes as soon as the target
// is in the zone, so that it meets the target at the zone's far edge and another can follow while ЦЕЛЬ В ЗОНЕ stays lit
// (voenizdat_osa_1991 p257).
//
//   ZoneWindowOf     when a target's track enters and leaves the firing zone
//   EvaluateLaunch   a launch at a time: the intercept, the chance per missile, how many shots the window allows, the
//                    chance of a kill with them
//   BestLaunch       the best moment to launch (the zone's entry)
//   ScoreTargets     the targets in order: urgency × what an engagement could do; friends last, never chosen
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "simcore/engagement.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// A target as the evaluation sees it: from the unit (ENU m), straight and level.
struct DecisionTrack {
    std::string target;
    std::string target_class = "default";
    Vec3 position;
    Vec3 velocity;
    double agl_m = 0.0;
    bool friendly = false;
};

struct ZoneWindow {
    double enter_s = -1.0;  // from now; < 0: it never enters within the horizon
    double leave_s = -1.0;
};

namespace detail {

inline EngagementTuning::ZoneKind ZoneAt(const EngagementTuning& t, const DecisionTrack& d, double in_s) {
    const Vec3 p = d.position + d.velocity * in_s;
    const double range = p.Length();
    if (range < 1.0) return EngagementTuning::ZoneKind::None;
    const double closing = -Dot(d.velocity, p / range);
    const double vh = std::hypot(d.velocity.x, d.velocity.y);
    const double parameter = vh > 1.0 ? std::fabs(p.x * d.velocity.y - p.y * d.velocity.x) / vh : std::hypot(p.x, p.y);
    return t.ZoneFor(range, d.agl_m, d.velocity.Length(), closing, parameter);
}

}  // namespace detail

inline ZoneWindow ZoneWindowOf(const EngagementTuning& t, const DecisionTrack& d, double horizon_s = 300.0, double step_s = 0.25) {
    ZoneWindow w;
    for (double s = 0.0; s <= horizon_s; s += step_s) {
        const bool in = detail::ZoneAt(t, d, s) != EngagementTuning::ZoneKind::None;
        if (in && w.enter_s < 0.0) w.enter_s = s;
        if (!in && w.enter_s >= 0.0) {
            w.leave_s = s;
            return w;
        }
    }
    if (w.enter_s >= 0.0) w.leave_s = horizon_s;
    return w;
}

// One launch `in_s` from now.
struct LaunchOption {
    double launch_in_s = 0.0;
    double flight_s = 0.0;           // to the intercept, at the missile's average speed
    double intercept_range_m = 0.0;
    bool in_zone = false;            // the target in the zone at the launch (the outcome model's zone factor)
    bool reachable = true;           // the intercept within the missile's flight time
    double chance = 0.0;             // per missile
    int shots = 0;                   // missiles the zone window allows, one after another's result
    double kill = 0.0;               // 1 - (1 - chance)^shots
};

// `cycle_s`: after a missile's flight, until the next can go at the same target (watching the result, launching again).
inline LaunchOption EvaluateLaunch(const EngagementTuning& t, const DecisionTrack& d, double in_s, double cycle_s,
                                   const ZoneWindow& window) {
    LaunchOption o;
    o.launch_in_s = in_s;
    const double vm = t.missile_average_speed_mps > 0.0 ? t.missile_average_speed_mps : 500.0;
    double tf = (d.position + d.velocity * in_s).Length() / vm;
    for (int i = 0; i < 8; ++i) tf = (d.position + d.velocity * (in_s + tf)).Length() / vm;  // fixed point: the meeting
    o.flight_s = tf;
    o.intercept_range_m = (d.position + d.velocity * (in_s + tf)).Length();
    o.reachable = t.missile_max_flight_time_s <= 0.0 || tf <= t.missile_max_flight_time_s;
    o.in_zone = detail::ZoneAt(t, d, in_s) != EngagementTuning::ZoneKind::None;
    const double zone = o.in_zone ? 1.0 : t.outside_zone_factor;
    o.chance = o.reachable ? t.HitChanceFor(d.target_class) * zone * t.LowAltitudeFactor(d.target_class, d.agl_m) : 0.0;
    o.shots = 0;
    if (o.reachable) {
        const double until = window.leave_s > in_s ? window.leave_s : in_s;
        const double step = tf + (cycle_s > 1.0 ? cycle_s : 1.0);
        for (double s = in_s; s <= until + 1e-9 || o.shots == 0; s += step) {
            ++o.shots;
            if (o.shots >= 6) break;  // a launcher's load
        }
    }
    o.kill = 1.0 - std::pow(1.0 - o.chance, o.shots);
    return o;
}

struct LaunchTiming {
    bool reachable = false;   // the target enters the zone within the horizon
    ZoneWindow window;
    LaunchOption best;        // the launch at the zone's entry
};

inline LaunchTiming BestLaunch(const EngagementTuning& t, const DecisionTrack& d, double cycle_s, double horizon_s = 300.0) {
    LaunchTiming out;
    out.window = ZoneWindowOf(t, d, horizon_s);
    if (out.window.enter_s < 0.0) return out;
    out.reachable = true;
    out.best = EvaluateLaunch(t, d, out.window.enter_s, cycle_s, out.window);
    return out;
}

struct TargetScore {
    std::string target;
    bool friendly = false;
    double closest_in_s = 0.0;  // until it is nearest the unit (its threat: the sooner, the more urgent)
    double kill = 0.0;          // what an engagement from its zone entry could do
    double zone_in_s = -1.0;    // when it enters the zone (< 0: never)
    double score = 0.0;         // kill × urgency; < 0 for a friend
};

// The targets best first. Urgency 1 / (1 + closest_in_s / 60 s): a target a minute from its closest point counts half
// one that is there now (sam3d).
inline std::vector<TargetScore> ScoreTargets(const EngagementTuning& t, const std::vector<DecisionTrack>& tracks, double cycle_s) {
    std::vector<TargetScore> out;
    for (const DecisionTrack& d : tracks) {
        TargetScore s;
        s.target = d.target;
        s.friendly = d.friendly;
        const double v2 = d.velocity.LengthSquared();
        s.closest_in_s = v2 > 1e-6 ? std::max(0.0, -Dot(d.position, d.velocity) / v2) : 0.0;
        const LaunchTiming lt = BestLaunch(t, d, cycle_s);
        s.zone_in_s = lt.window.enter_s;
        s.kill = lt.reachable ? lt.best.kill : 0.0;
        s.score = d.friendly ? -1.0 : s.kill / (1.0 + s.closest_in_s / 60.0);
        out.push_back(s);
    }
    std::stable_sort(out.begin(), out.end(), [](const TargetScore& a, const TargetScore& b) { return a.score > b.score; });
    return out;
}

}  // namespace simcore
