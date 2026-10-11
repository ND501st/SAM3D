// simcore/em/common.hpp — small helpers shared by the electromagnetic layer.
#pragma once

#include <cmath>

#include "simcore/vec3.hpp"

namespace simcore {
namespace em {

inline double WrapTwoPi(double a) {
    a = std::fmod(a, kTwoPi);
    return a < 0.0 ? a + kTwoPi : a;
}

inline double WrapPi(double a) {  // (-pi, pi]
    a = WrapTwoPi(a);
    return a > kPi ? a - kTwoPi : a;
}

inline double MaxOf(double a, double b) { return a > b ? a : b; }
inline double MinOf(double a, double b) { return a < b ? a : b; }

// Ground (horizontal) distance between two ENU points.
inline double GroundRange(const Vec3& a, const Vec3& b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Azimuth (clockwise from north) from a to b.
inline double AzimuthFromTo(const Vec3& a, const Vec3& b) { return AzimuthOf(b - a); }

// Radial velocity (m/s, positive = opening) of `target` seen from `sensor`.
inline double RadialSpeed(const Vec3& sensor_pos, const Vec3& sensor_vel, const Vec3& target_pos,
                          const Vec3& target_vel) {
    const Vec3 los = target_pos - sensor_pos;
    const double r = los.Length();
    if (r < 1e-6) return 0.0;
    return Dot(target_vel - sensor_vel, los) / r;
}

}  // namespace em
}  // namespace simcore
