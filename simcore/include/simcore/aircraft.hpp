// simcore/aircraft.hpp — simple waypoint-following aircraft (targets).
//
// Deliberately simple: constant speed, turn-rate-limited steering toward the
// next waypoint. Replace with an energy-based model (thrust/drag/g limits)
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

inline void StepAircraft(Aircraft& a, double dt) {
    if (!a.alive) return;
    Vec3 dir = a.velocity.Normalized();
    if (dir.LengthSquared() == 0.0) dir = Vec3{0.0, 1.0, 0.0};

    if (a.next_waypoint < a.waypoints.size()) {
        const Vec3 to_wp = a.waypoints[a.next_waypoint] - a.position;
        if (to_wp.Length() <= a.waypoint_capture_radius_m || WaypointInsideTurn(a, dir, a.waypoints[a.next_waypoint])) {
            ++a.next_waypoint;
        } else {
            dir = TurnToward(dir, to_wp.Normalized(), a.max_turn_rate_rad_s * dt);
        }
    }
    a.velocity = dir * a.speed_mps;
    a.position += a.velocity * dt;
}

}  // namespace simcore
