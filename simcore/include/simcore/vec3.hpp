// simcore/vec3.hpp — minimal 3D vector in double precision.
//
// Conventions (see docs/ARCHITECTURE.md):
//   * SI units everywhere (metres, seconds, radians, kilograms, newtons).
//   * Local world frame is ENU: x = East, y = North, z = Up.
#pragma once

#include <cmath>

namespace simcore {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kStandardGravity = 9.80665;  // m/s^2

constexpr double DegToRad(double deg) { return deg * kPi / 180.0; }
constexpr double RadToDeg(double rad) { return rad * 180.0 / kPi; }

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3() = default;
    constexpr Vec3(double ax, double ay, double az) : x(ax), y(ay), z(az) {}

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(double s) { x *= s; y *= s; z *= s; return *this; }

    double Length() const { return std::sqrt(x * x + y * y + z * z); }
    constexpr double LengthSquared() const { return x * x + y * y + z * z; }
    double HorizontalLength() const { return std::sqrt(x * x + y * y); }

    // Returns a unit vector, or the zero vector if the length is ~0.
    Vec3 Normalized() const {
        const double len = Length();
        return len > 1e-12 ? (*this) / len : Vec3{};
    }
};

constexpr Vec3 operator*(double s, const Vec3& v) { return v * s; }

constexpr double Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

constexpr Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline double Clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Angle between two vectors in radians, in [0, pi].
inline double AngleBetween(const Vec3& a, const Vec3& b) {
    const double la = a.Length();
    const double lb = b.Length();
    if (la < 1e-12 || lb < 1e-12) return 0.0;
    return std::acos(Clamp(Dot(a, b) / (la * lb), -1.0, 1.0));
}

// Rotate vector v about unit axis k by angle (Rodrigues' formula).
inline Vec3 RotateAboutAxis(const Vec3& v, const Vec3& unit_axis, double angle) {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return v * c + Cross(unit_axis, v) * s + unit_axis * (Dot(unit_axis, v) * (1.0 - c));
}

// Turn unit direction `from` toward unit direction `to` by at most `max_angle` radians.
inline Vec3 TurnToward(const Vec3& from, const Vec3& to, double max_angle) {
    const double angle = AngleBetween(from, to);
    if (angle <= max_angle || angle < 1e-12) return to.Normalized();
    Vec3 axis = Cross(from, to);
    if (axis.Length() < 1e-12) {
        // Opposite directions: pick any perpendicular axis (prefer turning in the horizontal plane).
        axis = Cross(from, Vec3{0.0, 0.0, 1.0});
        if (axis.Length() < 1e-12) axis = Cross(from, Vec3{1.0, 0.0, 0.0});
    }
    return RotateAboutAxis(from, axis.Normalized(), max_angle).Normalized();
}

// Azimuth (radians, clockwise from North, [0, 2pi)) of an ENU vector.
inline double AzimuthOf(const Vec3& enu) {
    double az = std::atan2(enu.x, enu.y);
    if (az < 0.0) az += kTwoPi;
    return az;
}

// Elevation (radians above the local horizontal plane) of an ENU vector.
inline double ElevationOf(const Vec3& enu) { return std::atan2(enu.z, enu.HorizontalLength()); }

// Unit ENU direction from azimuth/elevation.
inline Vec3 DirectionFromAzEl(double azimuth, double elevation) {
    const double ce = std::cos(elevation);
    return {ce * std::sin(azimuth), ce * std::cos(azimuth), std::sin(elevation)};
}

}  // namespace simcore
