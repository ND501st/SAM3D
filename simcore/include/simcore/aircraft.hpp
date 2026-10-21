// simcore/aircraft.hpp — simple waypoint-following aircraft (targets).
//
// Deliberately simple: constant airspeed, turn-rate-limited steering toward the
// next waypoint, drifting with the wind (weather.hpp). Replace with an energy-based model (thrust/drag/g limits)
// once the engagement loop works end to end.
#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {

struct Aircraft {
    int id = 0;
    std::string name;
    std::string type = "Air+FixedWing";  // Tacview type tags
    Vec3 position;                        // ENU [m]
    Vec3 velocity;                        // ENU [m/s]
    double speed_mps = 200.0;
    double max_turn_rate_rad_s = DegToRad(6.0);
    double rcs_m2 = 5.0;                  // radar cross-section (placeholder; aspect-dependent later)
    std::vector<Vec3> waypoints;          // ENU [m]
    std::size_t next_waypoint = 0;
    double waypoint_capture_radius_m = 500.0;
    bool alive = true;
};

// A waypoint inside one of the aircraft's turn circles (beside it, closer than its turn radius) can't be flown over
// at its turn rate: it would circle it for ever. A pilot who has overshot a turn point flies on to the next.
inline bool WaypointInsideTurn(const Aircraft& a, const Vec3& dir, const Vec3& wp) {
    if (a.max_turn_rate_rad_s <= 1e-6 || a.speed_mps <= 0.0) return false;
    const double h = std::hypot(dir.x, dir.y);
    if (h <= 1e-9) return false;
    const double radius_m = a.speed_mps / a.max_turn_rate_rad_s;
    const double lx = -dir.y / h, ly = dir.x / h;  // to the left, in the horizontal
    for (double side : {1.0, -1.0}) {
        const double cx = a.position.x + side * radius_m * lx, cy = a.position.y + side * radius_m * ly;
        if (std::hypot(wp.x - cx, wp.y - cy) < 0.98 * radius_m) return true;
    }
    return false;
}

// One step along the waypoints at constant airspeed. `wind_mps` (ENU, the way the air moves) carries the aircraft
// with it: `velocity` is over the ground, and the pilot crabs into the wind so the track over the ground still runs to
// the next waypoint (the wind triangle: the air vector a with |a| = speed and a + wind along the track).
inline void StepAircraft(Aircraft& a, double dt, const Vec3& wind_mps = Vec3{}) {
    if (!a.alive) return;
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
}

}  // namespace simcore
