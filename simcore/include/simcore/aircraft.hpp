// simcore/aircraft.hpp — waypoint-following aircraft (targets), flown with an attitude (roadmap R3).
//
// Two ways of flying, picked by what the aircraft is told of its type:
//   * Bank-to-turn (FliesByBank: the type's roll rate, g limit or energy model given; the air war's types give them).
//     A point mass with a heading, a bank and a path angle. The pilot banks to turn: a coordinated turn at bank phi
//     turns at omega = g tan(phi) / v with a load factor n = 1 / cos(phi). The turn rate stays under the type's
//     instantaneous rate (max_turn_rate_rad_s) and its g limit (omega <= g sqrt(n_max^2 - 1) / v), and the bank moves
//     no faster than the roll rate, so a turn point takes a roll-in and a roll-out. With an energy model (accel_mps2
//     and max_speed_mps) the airspeed changes (docs/air_battle/02_FLIGHTS_AND_PILOTS.md §3.1):
//         dv/dt = A (throttle - (v / v_max)^2) - K (n^2 - 1) - g sin(gamma)
//     with K set so the type's sustained turn holds its speed at the corner speed; the pilot's throttle holds the speed
//     he is told (speed_mps) while it can, so a climb or a turn harder than the sustained one costs speed. A helicopter
//     leans its nose down to fly forward and hangs level in the hover; it flies its told speed (no energy model yet).
//   * The old way (none of those given: the hand-made scenarios): constant airspeed, the velocity turned toward the
//     waypoint at the turn rate in 3D. Its attitude is read off the path it flew (the bank for the turn rate it flew,
//     rolled into at a shown rate), for the drawing only: nothing it does depends on it.
// Either way `velocity` is over the ground (the wind included), and heading, pitch and bank say where the nose points.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {

struct Aircraft {
    int id = 0;
    std::string name;
    std::string type = "Air+FixedWing";  // Tacview type tags ("Air+Rotorcraft": a helicopter)
    Vec3 position;                        // ENU [m]
    Vec3 velocity;                        // ENU [m/s], over the ground
    double speed_mps = 200.0;             // the airspeed it is told to fly (with an energy model the pilot holds it if he can)
    double max_turn_rate_rad_s = DegToRad(6.0);  // its turn rate (bank-to-turn: the instantaneous rate, at the corner speed)
    double rcs_m2 = 5.0;                  // radar cross-section (placeholder; aspect-dependent later)
    std::vector<Vec3> waypoints;          // ENU [m]
    std::size_t next_waypoint = 0;
    double waypoint_capture_radius_m = 500.0;
    bool alive = true;
    double bottom_m = 1.5;                // its lowest point below `position` (the pilot's ground floor; a game value)

    // Its type's limits (roadmap R3; the air war's AircraftType). 0: not given. With none of them it flies the old way.
    double max_load_g = 0.0;                 // n_max; a helicopter's bank limit is acos(1 / n_max)
    double sustained_turn_rate_rad_s = 0.0;  // the turn it holds without losing speed (sets K)
    double roll_rate_rad_s = 0.0;
    double max_speed_mps = 0.0;              // with accel_mps2: the energy model
    double accel_mps2 = 0.0;                 // level acceleration at full throttle, from slow
    double max_climb_mps = 0.0;

    // Its attitude (R3), set by every step.
    double heading_rad = 0.0;     // the nose's azimuth, clockwise from North (crabbed into a wind: the air vector's)
    double pitch_rad = 0.0;       // the nose over the horizon: the path angle and the angle of attack (a helicopter: its lean)
    double bank_rad = 0.0;        // right wing down > 0
    double path_angle_rad = 0.0;  // the flight path through the air over the horizon (gamma)
    double load_g = 1.0;          // n
    double airspeed_mps = -1.0;   // the airspeed it has (< 0: not flown yet, speed_mps)
    double throttle = 0.0;        // the energy model's: -0.5 (idle and the air brakes) .. 1
    bool attitude_set = false;    // the heading and path angle were taken from its velocity on its first step
    // Its height is set by a pilot after each step (TargetPilot, target_behaviour.hpp): the step flies the horizontal.
    bool height_held = false;
};

// Game values of the flight model (docs/air_battle/02_FLIGHTS_AND_PILOTS.md §3).
namespace flight {
constexpr double kHeadingTauS = 1.0;   // the pilot eases onto a new heading over about this (or the roll's time)
constexpr double kPathTauS = 1.0;      // and onto a new path angle
constexpr double kSpeedTauS = 4.0;     // and onto a new speed, throttle permitting
constexpr double kMinThrottle = -0.5;  // idle and the air brakes
constexpr double kAoa1gRad = DegToRad(3.0);   // angle of attack at 1 g at the speed it is told to fly
constexpr double kMaxAoaRad = DegToRad(15.0);
constexpr double kHeliLeanAtTopRad = DegToRad(10.0);  // a helicopter's nose-down lean at its top speed
constexpr double kHeliMaxLeanRad = DegToRad(20.0);
constexpr double kHeliTopMps = 80.0;   // a helicopter's top speed when its type gives none
constexpr double kHoverMps = 2.0;      // slower than this a helicopter turns on the spot, level
constexpr double kShownRollRateRadS = DegToRad(90.0);  // the old way's bank is rolled into at this, for the drawing
constexpr double kMaxShownBankRad = DegToRad(80.0);
constexpr double kHoldSpeedFraction = 0.85;  // below this share of the speed told he turns and climbs no harder than sustained
}  // namespace flight

inline bool IsRotorcraft(const Aircraft& a) { return a.type.find("Rotorcraft") != std::string::npos; }
inline bool HasEnergyModel(const Aircraft& a) {
    return a.accel_mps2 > 0.0 && a.max_speed_mps > 0.0 && !IsRotorcraft(a);
}
inline bool FliesByBank(const Aircraft& a) { return a.roll_rate_rad_s > 0.0 || a.max_load_g > 1.0 || HasEnergyModel(a); }
inline double AirspeedOf(const Aircraft& a) { return a.airspeed_mps >= 0.0 ? a.airspeed_mps : a.speed_mps; }

// A coordinated turn (no slip) at airspeed v and bank phi: its turn rate, load factor and radius; and the bank for a rate.
inline double CoordinatedTurnRate(double airspeed_mps, double bank_rad) {
    return airspeed_mps > 1e-6 ? kStandardGravity * std::tan(bank_rad) / airspeed_mps : 0.0;
}
inline double LoadFactorOfBank(double bank_rad) { return 1.0 / std::cos(bank_rad); }
inline double TurnRadiusM(double airspeed_mps, double bank_rad) {
    return airspeed_mps * airspeed_mps / (kStandardGravity * std::tan(bank_rad));
}
inline double BankForTurnRate(double airspeed_mps, double turn_rate_rad_s) {
    return std::atan(airspeed_mps * turn_rate_rad_s / kStandardGravity);
}
// The turn rate a g limit allows at airspeed v: g sqrt(n_max^2 - 1) / v.
inline double GLimitedTurnRate(double airspeed_mps, double max_load_g) {
    if (airspeed_mps <= 1e-6 || max_load_g <= 1.0) return 1e9;
    return kStandardGravity * std::sqrt(max_load_g * max_load_g - 1.0) / airspeed_mps;
}
// The speed where the g limit and the instantaneous rate meet: g-limited above it, held to the rate below.
inline double CornerSpeedMps(double max_load_g, double turn_rate_rad_s) {
    if (max_load_g <= 1.0 || turn_rate_rad_s <= 0.0) return 0.0;
    return kStandardGravity * std::sqrt(max_load_g * max_load_g - 1.0) / turn_rate_rad_s;
}
// K of the energy model: the sustained turn holds the speed at the corner speed (or at 90 % of the top speed, when the
// corner lies above it): K = A (1 - (v_c / v_max)^2) / (n_s^2 - 1), n_s^2 - 1 = (v_c omega_s / g)^2.
inline double TurnDragK(const Aircraft& a) {
    if (!HasEnergyModel(a) || a.sustained_turn_rate_rad_s <= 0.0) return 0.0;
    double v_ref = 0.9 * a.max_speed_mps;
    const double corner = CornerSpeedMps(a.max_load_g, a.max_turn_rate_rad_s);
    if (corner > 0.0) v_ref = std::min(v_ref, corner);
    const double x = v_ref * a.sustained_turn_rate_rad_s / kStandardGravity;
    if (x * x < 1e-9) return 0.0;
    const double r = v_ref / a.max_speed_mps;
    return a.accel_mps2 * (1.0 - r * r) / (x * x);
}
// The speed he means to keep in a turn or a climb: the speed he is told, or 90 % of his top speed.
inline double KeepSpeedMps(const Aircraft& a) {
    return HasEnergyModel(a) ? std::min(a.speed_mps, 0.9 * a.max_speed_mps) : 0.0;
}
// The speed a pilot won't bleed below: a share of the speed he keeps.
inline double HoldSpeedMps(const Aircraft& a) { return flight::kHoldSpeedFraction * KeepSpeedMps(a); }
// The excess thrust at airspeed v on full throttle, A (1 - (v / v_max)^2), as an acceleration.
inline double ExcessThrustMps2(const Aircraft& a, double airspeed_mps) {
    if (!HasEnergyModel(a)) return 0.0;
    const double r = airspeed_mps / a.max_speed_mps;
    return a.accel_mps2 * (1.0 - r * r);
}
// The highest turn rate at airspeed v: the type's instantaneous rate, under its g limit when it has one. Slowed in a
// turn, the pilot eases it: from the full rate at the speed he keeps down to the sustained turn at the speed he holds,
// g sqrt(n^2 - 1) / v with K (n^2 - 1) = the excess thrust, and no harder below it.
inline double TurnRateLimit(const Aircraft& a, double airspeed_mps) {
    double w = a.max_turn_rate_rad_s;
    if (a.max_load_g > 1.0) w = std::min(w, GLimitedTurnRate(airspeed_mps, a.max_load_g));
    const double keep = KeepSpeedMps(a), hold = HoldSpeedMps(a);
    if (airspeed_mps < keep && keep > hold) {
        const double k = TurnDragK(a);
        if (k > 0.0) {
            const double n2m1 = std::max(0.0, ExcessThrustMps2(a, airspeed_mps) / k);
            const double w_sus = kStandardGravity * std::sqrt(n2m1) / std::max(1.0, airspeed_mps);
            if (w_sus < w) w = w_sus + (w - w_sus) * std::max(0.0, (airspeed_mps - hold) / (keep - hold));
        }
    }
    return w;
}
// The most bank he may hold with `heading_left_rad` still to turn: rolling out from it at the roll rate p turns him
// (g / (v p)) (-ln cos phi) more, so cos phi >= exp(-left v p / g).
inline double RollOutBank(double heading_left_rad, double airspeed_mps, double roll_rate_rad_s) {
    if (roll_rate_rad_s <= 0.0) return kPi / 2.0;
    return std::acos(std::exp(-std::fabs(heading_left_rad) * airspeed_mps * roll_rate_rad_s / kStandardGravity));
}

// A waypoint inside one of the aircraft's turn circles (beside it, closer than its turn radius) can't be flown over
// at its turn rate: it would circle it for ever. A pilot who has overshot a turn point flies on to the next.
inline bool WaypointInsideTurn(const Aircraft& a, const Vec3& dir, const Vec3& wp) {
    const bool by_bank = FliesByBank(a);
    const double v = by_bank ? AirspeedOf(a) : a.speed_mps;
    const double w = by_bank ? TurnRateLimit(a, v) : a.max_turn_rate_rad_s;
    if (w <= 1e-6 || v <= 0.0) return false;
    const double h = std::hypot(dir.x, dir.y);
    if (h <= 1e-9) return false;
    const double radius_m = v / w;
    const double lx = -dir.y / h, ly = dir.x / h;  // to the left, in the horizontal
    for (double side : {1.0, -1.0}) {
        const double cx = a.position.x + side * radius_m * lx, cy = a.position.y + side * radius_m * ly;
        if (std::hypot(wp.x - cx, wp.y - cy) < 0.98 * radius_m) return true;
    }
    return false;
}

namespace flight {
inline double Approach(double from, double to, double max_step) {
    if (to > from) return std::min(to, from + max_step);
    return std::max(to, from - max_step);
}
inline double Clamp(double x, double lo, double hi) { return std::max(lo, std::min(hi, x)); }

// The nose over the horizon: the path angle and the angle of attack (alpha grows with n and as the speed falls below
// the one it is told), or a helicopter's lean (nose down for its speed and for speeding up, level in the hover).
inline void SetPitch(Aircraft& a, double airspeed_mps, double dvdt_mps2) {
    if (IsRotorcraft(a)) {
        const double top = a.max_speed_mps > 0.0 ? a.max_speed_mps : kHeliTopMps;
        const double r = airspeed_mps / top;
        const double lean = std::atan(r * r * std::tan(kHeliLeanAtTopRad) + dvdt_mps2 / kStandardGravity);
        a.pitch_rad = -Clamp(lean, -kHeliMaxLeanRad, kHeliMaxLeanRad);
        return;
    }
    const double ref = std::max(a.speed_mps, airspeed_mps);
    const double aoa = airspeed_mps > 1.0 ? kAoa1gRad * a.load_g * (ref / airspeed_mps) * (ref / airspeed_mps) : kMaxAoaRad;
    a.pitch_rad = a.path_angle_rad + Clamp(aoa, 0.0, kMaxAoaRad);
}

// The old way's attitude, read off the step it flew (`dir`: where the nose pointed, the air vector's direction).
inline void ShowAttitude(Aircraft& a, const Vec3& dir, double dt) {
    const double v = a.speed_mps;
    double omega = 0.0;
    if (dir.HorizontalLength() > 1e-6 && v > 0.01) {
        const double heading = AzimuthOf(dir);
        if (a.attitude_set && dt > 0.0) omega = std::remainder(heading - a.heading_rad, kTwoPi) / dt;
        a.heading_rad = heading;
        a.path_angle_rad = ElevationOf(dir);
    }
    const double lateral_g = v * std::cos(a.path_angle_rad) * omega / kStandardGravity;
    const double want = Clamp(std::atan(lateral_g), -kMaxShownBankRad, kMaxShownBankRad);
    a.bank_rad = a.attitude_set ? Approach(a.bank_rad, want, kShownRollRateRadS * dt) : want;
    a.load_g = std::sqrt(1.0 + lateral_g * lateral_g);
    a.airspeed_mps = v;
    a.throttle = 0.0;
    SetPitch(a, v, 0.0);
    a.attitude_set = true;
}

// Bank-to-turn: one step of the point mass.
inline void StepByBank(Aircraft& a, double dt, const Vec3& wind_mps) {
    const double g = kStandardGravity;
    const bool windy = wind_mps.LengthSquared() > 0.0;
    if (a.airspeed_mps < 0.0) a.airspeed_mps = a.speed_mps;
    if (!a.attitude_set) {
        Vec3 air = windy ? a.velocity - wind_mps : a.velocity;
        if (air.HorizontalLength() < 1e-6 && a.next_waypoint < a.waypoints.size()) air = a.waypoints[a.next_waypoint] - a.position;
        a.heading_rad = air.HorizontalLength() > 1e-9 ? AzimuthOf(air) : 0.0;
        a.path_angle_rad = a.height_held || air.LengthSquared() <= 0.0 ? 0.0 : ElevationOf(air);
        a.bank_rad = 0.0;
        a.attitude_set = true;
    }
    double v = a.airspeed_mps;

    // Where the pilot steers: the next waypoint (one he can't turn onto passed for the one after), else straight on.
    double want_heading = a.heading_rad;
    double want_gamma = a.path_angle_rad;
    if (a.next_waypoint < a.waypoints.size()) {
        const Vec3 nose{std::sin(a.heading_rad), std::cos(a.heading_rad), 0.0};
        const Vec3 to_wp = a.waypoints[a.next_waypoint] - a.position;
        if (to_wp.Length() <= a.waypoint_capture_radius_m || WaypointInsideTurn(a, nose, a.waypoints[a.next_waypoint])) {
            ++a.next_waypoint;
        }
    }
    if (a.next_waypoint < a.waypoints.size()) {
        const Vec3 to_wp = a.waypoints[a.next_waypoint] - a.position;
        const double h = to_wp.HorizontalLength();
        if (h > 1e-6) {
            Vec3 want{to_wp.x / h, to_wp.y / h, 0.0};
            const double vh = v * std::cos(a.path_angle_rad);
            const Vec3 wind_h{wind_mps.x, wind_mps.y, 0.0};
            if (windy && vh > 0.0) {  // crabbed into the wind: the air vector a, |a| = vh, a + wind along the track
                const double gw = Dot(want, wind_h);
                const double disc = gw * gw - wind_h.LengthSquared() + vh * vh;
                if (disc > 0.0) want = (want * (gw + std::sqrt(disc)) - wind_h) / vh;
            }
            if (want.HorizontalLength() > 1e-9) want_heading = AzimuthOf(want);
        }
        if (!a.height_held) want_gamma = std::atan2(to_wp.z, h);
    }
    if (!a.height_held && a.max_climb_mps > 0.0 && v > a.max_climb_mps) {
        want_gamma = std::min(want_gamma, std::asin(a.max_climb_mps / v));
    }
    if (!a.height_held && v < HoldSpeedMps(a)) {  // slow: he climbs only on the thrust the turn leaves
        const double spare = ExcessThrustMps2(a, v) - TurnDragK(a) * std::max(0.0, a.load_g * a.load_g - 1.0);
        want_gamma = std::min(want_gamma, std::asin(Clamp(spare / g, 0.0, 1.0)));
    }

    // The turn: bank toward the turn wanted, within the turn rate, the g limit, the roll rate and the roll-out ahead.
    const double w_lim = TurnRateLimit(a, v);
    const double err = std::remainder(want_heading - a.heading_rad, kTwoPi);
    const double p = a.roll_rate_rad_s;
    const double roll_step = p > 0.0 ? p * dt : 1e9;
    double omega = 0.0;
    if (v < kHoverMps) {  // hovering: it turns on the spot, level
        a.bank_rad = Approach(a.bank_rad, 0.0, roll_step);
        omega = (err < 0.0 ? -1.0 : 1.0) * std::min(w_lim, std::fabs(err) / kHeadingTauS);
    } else {
        const double bank_lim = std::atan(v * w_lim / g);
        const double tau = p > 0.0 ? std::max(kHeadingTauS, bank_lim / p) : kHeadingTauS;
        double bank_want = std::min({bank_lim, std::atan(v * std::fabs(err) / (g * tau)), RollOutBank(err, v, p)});
        if (err < 0.0) bank_want = -bank_want;
        // A limit that falls (slower, or eased) is rolled down to at the roll rate; the g limit's bank never passed.
        const double g_bank = a.max_load_g > 1.0 ? std::acos(1.0 / a.max_load_g) : kPi / 2.0 - 1e-3;
        a.bank_rad = Clamp(Approach(a.bank_rad, bank_want, roll_step), -g_bank, g_bank);
        omega = CoordinatedTurnRate(v, a.bank_rad);
    }
    if (omega * err > 0.0 && std::fabs(omega * dt) > std::fabs(err)) omega = err / dt;  // not past the heading wanted
    a.heading_rad = std::fmod(a.heading_rad + omega * dt + kTwoPi, kTwoPi);

    // The path angle: pulled toward the one wanted within the same rate (a pilot holding the height sets it himself).
    double gamma_rate = 0.0;
    if (!a.height_held) {
        const double gerr = want_gamma - a.path_angle_rad;
        gamma_rate = Clamp(gerr / kPathTauS, -w_lim, w_lim);
        if (std::fabs(gamma_rate * dt) > std::fabs(gerr)) gamma_rate = dt > 0.0 ? gerr / dt : 0.0;
        a.path_angle_rad += gamma_rate * dt;
    }
    const double cg = std::cos(a.path_angle_rad), sg = std::sin(a.path_angle_rad);
    const double n_lat = v * cg * omega / g, n_up = cg + v * gamma_rate / g;
    a.load_g = std::sqrt(n_lat * n_lat + n_up * n_up);

    // The speed: the energy model, or the speed told straight.
    double dvdt = 0.0;
    if (HasEnergyModel(a)) {
        const double accel = a.accel_mps2, r = v / a.max_speed_mps;
        const double turn = TurnDragK(a) * std::max(0.0, a.load_g * a.load_g - 1.0);
        const double climb = g * sg;
        const double th = r * r + (turn + climb) / accel + (a.speed_mps - v) / (kSpeedTauS * accel);
        a.throttle = Clamp(th, kMinThrottle, 1.0);
        dvdt = accel * (a.throttle - r * r) - turn - climb;
        v = std::max(0.0, v + dvdt * dt);
    } else {
        v = a.speed_mps;
        a.throttle = 0.0;
    }
    a.airspeed_mps = v;

    const Vec3 air = Vec3{std::sin(a.heading_rad) * cg, std::cos(a.heading_rad) * cg, a.height_held ? 0.0 : sg} * v;
    a.velocity = air + (windy ? wind_mps : Vec3{});
    a.position += a.velocity * dt;
    SetPitch(a, v, dvdt);
}
}  // namespace flight

// One step along the waypoints. `wind_mps` (ENU, the way the air moves) carries the aircraft with it: `velocity` is
// over the ground, and the pilot crabs into the wind so the track over the ground still runs to the next waypoint
// (the wind triangle: the air vector a with |a| = speed and a + wind along the track).
inline void StepAircraft(Aircraft& a, double dt, const Vec3& wind_mps = Vec3{}) {
    if (!a.alive) return;
    if (FliesByBank(a)) {
        flight::StepByBank(a, dt, wind_mps);
        return;
    }
    // The old way: constant airspeed, the velocity turned toward the waypoint in 3D.
    const bool windy = wind_mps.LengthSquared() > 0.0;
    Vec3 dir = (windy ? a.velocity - wind_mps : a.velocity).Normalized();  // where the nose points
    if (dir.LengthSquared() == 0.0) dir = Vec3{0.0, 1.0, 0.0};

    if (a.next_waypoint < a.waypoints.size()) {
        const Vec3 to_wp = a.waypoints[a.next_waypoint] - a.position;
        if (to_wp.Length() <= a.waypoint_capture_radius_m || WaypointInsideTurn(a, dir, a.waypoints[a.next_waypoint])) {
            ++a.next_waypoint;
        } else {
            Vec3 want = to_wp.Normalized();
            if (windy) {
                const double gw = Dot(want, wind_mps);
                const double disc = gw * gw - wind_mps.LengthSquared() + a.speed_mps * a.speed_mps;
                if (disc > 0.0 && a.speed_mps > 0.0) want = (want * (gw + std::sqrt(disc)) - wind_mps) / a.speed_mps;
            }
            dir = TurnToward(dir, want.Normalized(), a.max_turn_rate_rad_s * dt);
        }
    }
    a.velocity = dir * a.speed_mps + (windy ? wind_mps : Vec3{});
    a.position += a.velocity * dt;
    flight::ShowAttitude(a, dir, dt);
}

// After a pilot set the height (target_behaviour.hpp): the path angle and the pitch from the vertical speed it flew.
inline void SyncPathAngle(Aircraft& a) {
    if (!a.attitude_set) return;
    const double v = FliesByBank(a) ? AirspeedOf(a) : a.speed_mps;
    const double gamma = v > 0.1 ? std::asin(flight::Clamp(a.velocity.z / v, -1.0, 1.0)) : 0.0;
    const double aoa = a.pitch_rad - a.path_angle_rad;
    a.path_angle_rad = gamma;
    if (!IsRotorcraft(a)) a.pitch_rad = gamma + aoa;
}

}  // namespace simcore
