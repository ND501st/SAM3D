// simcore/aircraft.hpp — simple waypoint-following aircraft (targets).
//
// Deliberately simple: constant speed, turn-rate-limited steering toward the
// next waypoint. Replace with an energy-based model (thrust/drag/g limits)
// once the engagement loop works end to end.
#pragma once

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

inline void StepAircraft(Aircraft& a, double dt) {
    if (!a.alive) return;
    Vec3 dir = a.velocity.Normalized();
    if (dir.LengthSquared() == 0.0) dir = Vec3{0.0, 1.0, 0.0};

    if (a.next_waypoint < a.waypoints.size()) {
        const Vec3 to_wp = a.waypoints[a.next_waypoint] - a.position;
        if (to_wp.Length() <= a.waypoint_capture_radius_m) {
            ++a.next_waypoint;
        } else {
            dir = TurnToward(dir, to_wp.Normalized(), a.max_turn_rate_rad_s * dt);
        }
    }
    a.velocity = dir * a.speed_mps;
    a.position += a.velocity * dt;
}

}  // namespace simcore
