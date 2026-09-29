// simcore/geo.hpp — WGS-84 geodesy and the local ENU (East-North-Up) frame.
//
// The simulation runs in a local ENU frame anchored at a geodetic origin
// (normally the battery / launcher site). Geodetic coordinates are only needed
// for terrain lookups, Cesium georeferencing and Tacview export.
#pragma once

#include <cmath>

#include "simcore/vec3.hpp"

namespace simcore {

namespace wgs84 {
constexpr double kSemiMajorAxis = 6378137.0;                 // a [m]
constexpr double kFlattening = 1.0 / 298.257223563;          // f
constexpr double kSemiMinorAxis = kSemiMajorAxis * (1.0 - kFlattening);
constexpr double kEccentricitySq = kFlattening * (2.0 - kFlattening);  // e^2
constexpr double kMeanRadius = 6371008.8;                     // mean Earth radius [m]
}  // namespace wgs84

// Geodetic position. Angles in radians, altitude in metres above the WGS-84 ellipsoid.
struct GeoPoint {
    double lat_rad = 0.0;
    double lon_rad = 0.0;
    double alt_m = 0.0;

    static GeoPoint FromDegrees(double lat_deg, double lon_deg, double alt_m) {
        return {DegToRad(lat_deg), DegToRad(lon_deg), alt_m};
    }
    double LatDeg() const { return RadToDeg(lat_rad); }
    double LonDeg() const { return RadToDeg(lon_rad); }
};

inline Vec3 GeodeticToEcef(const GeoPoint& g) {
    const double sin_lat = std::sin(g.lat_rad);
    const double cos_lat = std::cos(g.lat_rad);
    const double n = wgs84::kSemiMajorAxis / std::sqrt(1.0 - wgs84::kEccentricitySq * sin_lat * sin_lat);
    return {(n + g.alt_m) * cos_lat * std::cos(g.lon_rad),
            (n + g.alt_m) * cos_lat * std::sin(g.lon_rad),
            (n * (1.0 - wgs84::kEccentricitySq) + g.alt_m) * sin_lat};
}

// Iterative inverse; converges to sub-millimetre in a few iterations for terrestrial altitudes.
inline GeoPoint EcefToGeodetic(const Vec3& ecef) {
    const double p = std::sqrt(ecef.x * ecef.x + ecef.y * ecef.y);
    GeoPoint g;
    g.lon_rad = std::atan2(ecef.y, ecef.x);
    if (p < 1e-6) {  // on the polar axis
        g.lat_rad = ecef.z >= 0.0 ? kPi / 2.0 : -kPi / 2.0;
        g.alt_m = std::fabs(ecef.z) - wgs84::kSemiMinorAxis;
        return g;
    }
    double lat = std::atan2(ecef.z, p * (1.0 - wgs84::kEccentricitySq));
    double alt = 0.0;
    for (int i = 0; i < 8; ++i) {
        const double sin_lat = std::sin(lat);
        const double n = wgs84::kSemiMajorAxis / std::sqrt(1.0 - wgs84::kEccentricitySq * sin_lat * sin_lat);
        alt = p / std::cos(lat) - n;
        lat = std::atan2(ecef.z, p * (1.0 - wgs84::kEccentricitySq * n / (n + alt)));
    }
    g.lat_rad = lat;
    g.alt_m = alt;
    return g;
}

// Local tangent-plane frame (ENU) anchored at a geodetic origin.
class LocalFrame {
public:
    LocalFrame() : LocalFrame(GeoPoint{}) {}

    explicit LocalFrame(const GeoPoint& origin) : origin_(origin), origin_ecef_(GeodeticToEcef(origin)) {
        const double sl = std::sin(origin.lat_rad), cl = std::cos(origin.lat_rad);
        const double so = std::sin(origin.lon_rad), co = std::cos(origin.lon_rad);
        east_ = {-so, co, 0.0};
        north_ = {-sl * co, -sl * so, cl};
        up_ = {cl * co, cl * so, sl};
    }

    const GeoPoint& Origin() const { return origin_; }

    Vec3 EcefToEnu(const Vec3& ecef) const {
        const Vec3 d = ecef - origin_ecef_;
        return {Dot(d, east_), Dot(d, north_), Dot(d, up_)};
    }

    Vec3 EnuToEcef(const Vec3& enu) const {
        return origin_ecef_ + east_ * enu.x + north_ * enu.y + up_ * enu.z;
    }

    Vec3 GeodeticToEnu(const GeoPoint& g) const { return EcefToEnu(GeodeticToEcef(g)); }
    GeoPoint EnuToGeodetic(const Vec3& enu) const { return EcefToGeodetic(EnuToEcef(enu)); }

    // Altitude above the ellipsoid of an ENU point (exact, via ECEF).
    double AltitudeOf(const Vec3& enu) const { return EnuToGeodetic(enu).alt_m; }

private:
    GeoPoint origin_;
    Vec3 origin_ecef_;
    Vec3 east_, north_, up_;
};

}  // namespace simcore
