// simcore/em/propagation.hpp — how a signal gets from A to B: one model for every band.
//
// Radar, ESM, jamming and radio links all ask the same questions: is there a line of sight,
// how much does terrain cost if not, what does the air cost, and does the ground reflection
// help or hurt. The answers here are textbook propagation, chosen because they behave right
// across bands without per-band special cases:
//
//   * Free-space loss: 20 log10(4 pi R / lambda).
//   * Terrain: the path profile over a 4/3-Earth (as radar.hpp's HasLineOfSight), plus
//     single dominant knife-edge diffraction, ITU-R P.526 eq. (31):
//       J(v) = 6.9 + 20 log10( sqrt((v - 0.1)^2 + 1) + v - 0.1 )  dB, for v > -0.78, else 0.
//     At centimetre wavelengths a ridge costs tens of dB (radar behind a hill is blind);
//     at VHF the same ridge costs ~10 dB (radios still talk over it). One formula, both effects.
//   * Gases: a coarse sea-level table read from ITU-R P.676 (dry air plus 7.5 g/m^3 water
//     vapour), thinned with altitude. Small below 10 GHz, which is the point.
//   * Ground reflection (multipath), microwave: two-ray model over a spherical Earth with
//     effective heights (Blake, "Radar Range-Performance Analysis"; Skolnik, Radar Handbook ch. 2).
//     It makes low fliers fade in and out and shortens low-altitude detection ranges.
//   * Ground links below 1 GHz: plane-earth law (1/d^4 beyond the last lobe), which matches VHF
//     tactical radio ranges far better than a two-ray model with Earth-curvature heights.
//
// The terrain part of a path is frequency-independent: v = h * sqrt(2/lambda * (1/d1 + 1/d2)),
// so a path's worst  h * sqrt(1/d1 + 1/d2)  is computed once (PathGeometry) and reused for every
// band. PathCache keeps those per pair of platforms until either end moves.
#pragma once

#include <cmath>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "simcore/geo.hpp"
#include "simcore/radar.hpp"
#include "simcore/terrain.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace em {

constexpr double kSpeedOfLight = 299792458.0;  // m/s

inline double WavelengthOf(double freq_hz) { return freq_hz > 0.0 ? kSpeedOfLight / freq_hz : 0.0; }

// One-way free-space path loss (dB, positive).
inline double FreeSpacePathLossDb(double range_m, double freq_hz) {
    if (range_m <= 0.0 || freq_hz <= 0.0) return 0.0;
    return 20.0 * std::log10(4.0 * kPi * range_m / WavelengthOf(freq_hz));
}

// ITU-R P.526-15 eq. (31): single knife-edge diffraction loss (dB, >= 0) for parameter v.
inline double KnifeEdgeLossDb(double v) {
    if (v <= -0.78) return 0.0;
    const double t = v - 0.1;
    return 6.9 + 20.0 * std::log10(std::sqrt(t * t + 1.0) + t);
}

// Specific gaseous attenuation at sea level (dB/km, one way), standard atmosphere.
// APPROXIMATION: values read off ITU-R P.676 (Fig. 1, 7.5 g/m^3 water vapour) and interpolated
// in log frequency. Good to a factor of ~1.5 below 40 GHz, which is far below what matters here.
inline double GaseousAttenuationDbPerKm(double freq_hz) {
    static const double kTable[][2] = {
        {0.1e9, 0.0040}, {1.0e9, 0.0055}, {3.0e9, 0.0075}, {6.0e9, 0.0090}, {10.0e9, 0.0130},
        {15.0e9, 0.0280}, {20.0e9, 0.0900}, {22.2e9, 0.1800}, {25.0e9, 0.1400}, {30.0e9, 0.1100},
        {35.0e9, 0.1200}, {40.0e9, 0.1500}};
    const std::size_t n = sizeof(kTable) / sizeof(kTable[0]);
    if (freq_hz <= kTable[0][0]) return kTable[0][1];
    if (freq_hz >= kTable[n - 1][0]) return kTable[n - 1][1];
    for (std::size_t i = 1; i < n; ++i) {
        if (freq_hz <= kTable[i][0]) {
            const double t = std::log(freq_hz / kTable[i - 1][0]) / std::log(kTable[i][0] / kTable[i - 1][0]);
            return kTable[i - 1][1] + t * (kTable[i][1] - kTable[i - 1][1]);
        }
    }
    return kTable[n - 1][1];
}

// Specific attenuation of rain (dB/km, one way) for a rain rate (mm/h): gamma = k R^alpha, with k and alpha from the
// ITU-R P.838-3 table for horizontal polarisation (1-40 GHz), interpolated in log frequency (k in log, alpha linear).
inline double RainSpecificAttenuationDbPerKm(double freq_hz, double rain_mmh) {
    if (rain_mmh <= 0.0 || freq_hz <= 0.0) return 0.0;
    static const double kTable[][3] = {
        {1.0e9, 0.0000259, 0.9691}, {2.0e9, 0.0000847, 1.0664}, {4.0e9, 0.0001071, 1.6009}, {6.0e9, 0.0007056, 1.5900},
        {8.0e9, 0.004115, 1.3905},  {10.0e9, 0.01217, 1.2571},  {12.0e9, 0.02386, 1.1825},  {15.0e9, 0.04481, 1.1233},
        {20.0e9, 0.09164, 1.0568},  {25.0e9, 0.1571, 0.9991},   {30.0e9, 0.2403, 0.9485},   {35.0e9, 0.3374, 0.9047},
        {40.0e9, 0.4431, 0.8673}};
    const std::size_t n = sizeof(kTable) / sizeof(kTable[0]);
    double k = kTable[0][1], a = kTable[0][2];
    if (freq_hz >= kTable[n - 1][0]) {
        k = kTable[n - 1][1];
        a = kTable[n - 1][2];
    } else if (freq_hz > kTable[0][0]) {
        for (std::size_t i = 1; i < n; ++i) {
            if (freq_hz <= kTable[i][0]) {
                const double t = std::log(freq_hz / kTable[i - 1][0]) / std::log(kTable[i][0] / kTable[i - 1][0]);
                k = std::exp(std::log(kTable[i - 1][1]) + t * (std::log(kTable[i][1]) - std::log(kTable[i - 1][1])));
                a = kTable[i - 1][2] + t * (kTable[i][2] - kTable[i - 1][2]);
                break;
            }
        }
    }
    const double below = freq_hz < kTable[0][0] ? (freq_hz / kTable[0][0]) * (freq_hz / kTable[0][0]) : 1.0;  // below the table it falls off as f^2
    return below * k * std::pow(rain_mmh, a);
}

// One-way rain loss (dB) over a horizontal range. Rain is not uniform over a long path: the effective length is the
// range times 1 / (1 + d / d0), d0 = 35 exp(-0.015 R) km (ITU-R P.530 distance factor).
inline double RainLossDb(double range_m, double freq_hz, double rain_mmh) {
    if (rain_mmh <= 0.0 || range_m <= 0.0) return 0.0;
    const double d_km = range_m / 1000.0;
    const double d0 = 35.0 * std::exp(-0.015 * (rain_mmh < 100.0 ? rain_mmh : 100.0));
    return RainSpecificAttenuationDbPerKm(freq_hz, rain_mmh) * d_km / (1.0 + d_km / d0);
}

// One-way gaseous loss (dB) along a path whose ends are at the given altitudes. The air thins
// with height (exponential with a ~6 km scale height for the mix of oxygen and water vapour),
// so the sea-level rate is scaled by the path's mean exp(-h / 6 km).
inline double GaseousLossDb(double range_m, double alt_a_m, double alt_b_m, double freq_hz) {
    if (range_m <= 0.0) return 0.0;
    constexpr double kScaleHeightM = 6000.0;
    const double ha = alt_a_m > 0.0 ? alt_a_m : 0.0;
    const double hb = alt_b_m > 0.0 ? alt_b_m : 0.0;
    const double dh = hb - ha;
    double mean_factor = std::exp(-ha / kScaleHeightM);
    if (std::fabs(dh) >= 1.0) {  // average of exp(-h/H) along a straight climb from ha to hb
        mean_factor = kScaleHeightM * (std::exp(-ha / kScaleHeightM) - std::exp(-hb / kScaleHeightM)) /
                      (dh > 0.0 ? dh : (dh < 0.0 ? dh : 1.0));
    }
    return GaseousAttenuationDbPerKm(freq_hz) * (range_m / 1000.0) * mean_factor;
}

// The frequency-independent part of a path over terrain.
struct PathGeometry {
    double ground_range_m = 0.0;
    double slant_range_m = 0.0;
    bool line_of_sight = true;
    // Smallest clearance of the ray above terrain + Earth bulge (m); negative = blocked by this much.
    double min_clearance_m = 1e9;
    // max over the profile of h * sqrt(1/d1 + 1/d2) [m^0.5], h = obstacle height above the ray.
    // v for a wavelength = worst_v_geometry * sqrt(2 / lambda).
    double worst_v_geometry = -1e9;
    // Deygout's second and third edges (ITU-R P.526, section 4.3): when the main edge blocks the ray, the worst edge
    // between the first terminal and the main edge's top, and between the top and the second terminal, in the same
    // units. -1e9: none. Only these two are used (the classic three-edge form).
    double sub_v_geometry_a = -1e9;
    double sub_v_geometry_b = -1e9;
    int samples = 0;  // profile samples taken (for profiling)
};

// Sample the terrain profile between two ENU points. Endpoint altitudes are above the ellipsoid
// (LocalFrame::AltitudeOf). Spacing is relaxed for long paths so a profile never exceeds
// `max_samples` (the profile still catches ridges wider than the spacing).
inline PathGeometry AnalysePath(const Terrain& terrain, const Vec3& a_enu, double a_alt_m, const Vec3& b_enu,
                                double b_alt_m, double spacing_m = 100.0, int max_samples = 600,
                                double k = kStandardRefractionK) {
    PathGeometry g;
    const double dx = b_enu.x - a_enu.x;
    const double dy = b_enu.y - a_enu.y;
    g.ground_range_m = std::sqrt(dx * dx + dy * dy);
    const double dz = b_alt_m - a_alt_m;
    g.slant_range_m = std::sqrt(g.ground_range_m * g.ground_range_m + dz * dz);
    if (g.ground_range_m < 1.0) return g;
    const double re = k * wgs84::kMeanRadius;
    int n = static_cast<int>(g.ground_range_m / (spacing_m > 1.0 ? spacing_m : 1.0));
    if (n > max_samples) n = max_samples;
    if (n < 2) n = 2;
    g.samples = n - 1;
    const double inv_n = 1.0 / static_cast<double>(n);
    // The profile with the Earth's bulge added: straight lines stay straight in it, so the main ray and every
    // sub-path's ray are lines in these coordinates.
    std::vector<double> prof_s(static_cast<std::size_t>(n - 1));
    std::vector<double> prof_e(static_cast<std::size_t>(n - 1));
    int worst_i = -1;
    for (int i = 1; i < n; ++i) {
        const double t = static_cast<double>(i) * inv_n;
        const double s = t * g.ground_range_m;
        const double rest = g.ground_range_m - s;
        const double ray_alt = a_alt_m + dz * t;
        const double bulge = s * rest / (2.0 * re);
        const double ground = terrain.HeightAt(a_enu.x + dx * t, a_enu.y + dy * t) + bulge;
        const double h = ground - ray_alt;  // > 0: the ground is above the ray
        if (-h < g.min_clearance_m) g.min_clearance_m = -h;
        if (h >= 0.0) g.line_of_sight = false;
        const double vg = h * std::sqrt(1.0 / s + 1.0 / rest);
        prof_s[static_cast<std::size_t>(i - 1)] = s;
        prof_e[static_cast<std::size_t>(i - 1)] = ground;
        if (vg > g.worst_v_geometry) {
            g.worst_v_geometry = vg;
            worst_i = i;
        }
    }
    // Deygout: if the main edge blocks the ray, look for the worst edge on each side of it against the line to its top.
    if (worst_i > 0 && g.worst_v_geometry >= 0.0) {
        const std::size_t m = static_cast<std::size_t>(worst_i - 1);
        const double sm = prof_s[m];
        const double em = prof_e[m];
        // A sub-edge must be a crest of its own, with a valley of at least kValleyM between it and the main edge:
        // the flat top of one wide ridge is one edge, not many.
        constexpr double kValleyM = 20.0;
        for (int side = 0; side < 2; ++side) {
            const bool before = side == 0;
            const double s0 = before ? 0.0 : sm;
            const double e0 = before ? a_alt_m : em;
            const double s1 = before ? sm : g.ground_range_m;
            const double e1 = before ? em : b_alt_m;
            const double len = s1 - s0;
            if (len < 1.0) continue;
            double low = 1e18;  // lowest ground between the candidate and the main edge
            double& slot = before ? g.sub_v_geometry_a : g.sub_v_geometry_b;
            const std::ptrdiff_t step = before ? -1 : 1;
            std::ptrdiff_t i = static_cast<std::ptrdiff_t>(m) + step;
            for (; i >= 0 && i < static_cast<std::ptrdiff_t>(prof_s.size()); i += step) {
                const std::size_t u = static_cast<std::size_t>(i);
                const double ground_here = prof_e[u];
                const double lower_of_crests = ground_here < em ? ground_here : em;
                if (low < lower_of_crests - kValleyM) {
                    const double line = e0 + (e1 - e0) * (prof_s[u] - s0) / len;
                    const double d0 = prof_s[u] - s0;
                    const double d1 = s1 - prof_s[u];
                    if (d0 >= 1.0 && d1 >= 1.0) {
                        const double vg = (ground_here - line) * std::sqrt(1.0 / d0 + 1.0 / d1);
                        if (vg > slot) slot = vg;
                    }
                }
                if (ground_here < low) low = ground_here;
            }
        }
    }
    return g;
}

// Diffraction loss (dB) of a path at a frequency: the main edge plus Deygout's two sub-edges (ITU-R P.526 section
// 4.3; a ridge line costs more than its highest crest alone, as it does in measurements).
inline double DiffractionLossDb(const PathGeometry& g, double freq_hz) {
    if (g.ground_range_m < 1.0 || freq_hz <= 0.0) return 0.0;
    const double k = std::sqrt(2.0 / WavelengthOf(freq_hz));
    double loss = KnifeEdgeLossDb(g.worst_v_geometry * k);
    if (g.sub_v_geometry_a > -1e8) loss += KnifeEdgeLossDb(g.sub_v_geometry_a * k);
    if (g.sub_v_geometry_b > -1e8) loss += KnifeEdgeLossDb(g.sub_v_geometry_b * k);
    return loss;
}

// --------------------------------------------------------------------------- ground reflection

// Geometry of the specular ground reflection between two heights above a smooth spherical Earth.
struct ReflectionGeometry {
    bool valid = false;           // false only for degenerate input (zero range, an end on the ground)
    bool near_horizon = false;    // an effective height hit the floor: the rays cancel almost fully
    double d1_m = 0.0;            // ground range from end 1 to the reflection point
    double h1_eff_m = 0.0;        // heights above the tangent plane at the reflection point
    double h2_eff_m = 0.0;
    double grazing_rad = 0.0;     // grazing angle at the reflection point
    double path_difference_m = 0.0;
};

// h1, h2: heights above the local ground (m); d: ground range (m). Iterates the reflection point
// (Blake's method, simplified): d1 = d h1' / (h1' + h2'), h' = h - d_i^2 / (2 k R). Near and beyond
// the horizon the effective heights shrink to a floor, so the direct and reflected rays cancel and
// the factor falls towards zero, joining the diffraction loss beyond the horizon.
inline ReflectionGeometry ComputeReflection(double h1_m, double h2_m, double d_m, double k = kStandardRefractionK) {
    ReflectionGeometry r;
    if (d_m < 1.0 || h1_m <= 0.0 || h2_m <= 0.0) return r;
    constexpr double kFloorM = 0.1;
    const double re = k * wgs84::kMeanRadius;
    double d1 = d_m * h1_m / (h1_m + h2_m);
    double h1e = h1_m, h2e = h2_m;
    for (int it = 0; it < 8; ++it) {
        const double d2 = d_m - d1;
        h1e = h1_m - d1 * d1 / (2.0 * re);
        h2e = h2_m - d2 * d2 / (2.0 * re);
        if (h1e < kFloorM) {
            h1e = kFloorM;
            r.near_horizon = true;
        }
        if (h2e < kFloorM) {
            h2e = kFloorM;
            r.near_horizon = true;
        }
        d1 = d_m * h1e / (h1e + h2e);
    }
    r.valid = true;
    r.d1_m = d1;
    r.h1_eff_m = h1e;
    r.h2_eff_m = h2e;
    r.grazing_rad = std::atan((h1e + h2e) / d_m);
    r.path_difference_m = 2.0 * h1e * h2e / d_m;
    return r;
}

// One-way two-ray propagation factor F (amplitude ratio to free space). `rho` is the effective
// reflection coefficient magnitude (smooth ground at grazing incidence ~1; roughness lowers it at
// short wavelengths); near grazing the reflection reverses phase. `gain_ratio` is the antenna's
// amplitude gain towards the reflection point divided by its gain towards the direct path (a beam
// pointed up suppresses the reflection). Radar SNR scales with F^4, a one-way link with F^2.
inline double TwoRayFactor(const ReflectionGeometry& r, double wavelength_m, double rho, double gain_ratio = 1.0) {
    if (!r.valid || wavelength_m <= 0.0) return 1.0;
    const double g = rho * gain_ratio;
    const double phase = kTwoPi * r.path_difference_m / wavelength_m;
    double f2 = 1.0 + g * g - 2.0 * g * std::cos(phase);
    if (f2 < 1e-6) f2 = 1e-6;  // exact nulls don't survive real terrain
    return std::sqrt(f2);
}

// Plane-earth factor for links between antennas near the ground (VHF/UHF radios): beyond the
// last interference lobe the two rays leave F = 4 pi h1 h2 / (lambda d), i.e. received power
// falling as 1/d^4 (Parsons, "The Mobile Radio Propagation Channel", ch. 2). Capped at 1:
// "the larger of the free-space and the plane-earth loss".
inline double PlaneEarthFactor(double h1_m, double h2_m, double d_m, double wavelength_m) {
    if (d_m < 1.0 || wavelength_m <= 0.0) return 1.0;
    const double h1 = h1_m > 0.5 ? h1_m : 0.5, h2 = h2_m > 0.5 ? h2_m : 0.5;
    const double f = 4.0 * kPi * h1 * h2 / (wavelength_m * d_m);
    return f < 1.0 ? f : 1.0;
}

// --------------------------------------------------------------------------- cache

// Keeps PathGeometry per ordered pair of keys until either end moves more than a tolerance: the
// larger of a fixed distance and a fraction of the path length (a 25 m move hardly changes a
// 100 km profile; heights matter more, so altitude changes use the fixed tolerance).
class PathCache {
public:
    explicit PathCache(double tolerance_m = 25.0, double relative_tolerance = 0.002)
        : tolerance_m_(tolerance_m), relative_(relative_tolerance) {}

    const PathGeometry& Get(std::uint64_t key, const Terrain& terrain, const Vec3& a, double a_alt, const Vec3& b,
                            double b_alt, double spacing_m = 100.0) {
        Entry& e = entries_[key];
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double tol = tolerance_m_ > relative_ * std::sqrt(dx * dx + dy * dy) ? tolerance_m_
                                                                                     : relative_ * std::sqrt(dx * dx + dy * dy);
        const double tol2 = tol * tol;
        if (e.valid && (e.a - a).LengthSquared() < tol2 && (e.b - b).LengthSquared() < tol2 &&
            std::fabs(e.a_alt - a_alt) < tolerance_m_ && std::fabs(e.b_alt - b_alt) < tolerance_m_) {
            ++hits_;
            return e.geometry;
        }
        ++misses_;
        e.valid = true;
        e.a = a;
        e.b = b;
        e.a_alt = a_alt;
        e.b_alt = b_alt;
        e.geometry = AnalysePath(terrain, a, a_alt, b, b_alt, spacing_m);
        samples_ += static_cast<std::uint64_t>(e.geometry.samples);
        return e.geometry;
    }

    static std::uint64_t PairKey(int a, int b) {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32) |
               static_cast<std::uint64_t>(static_cast<std::uint32_t>(b));
    }

    void Clear() { entries_.clear(); }
    std::uint64_t Hits() const { return hits_; }
    std::uint64_t Misses() const { return misses_; }
    std::uint64_t SamplesTaken() const { return samples_; }
    std::size_t Size() const { return entries_.size(); }

private:
    struct Entry {
        bool valid = false;
        Vec3 a, b;
        double a_alt = 0.0, b_alt = 0.0;
        PathGeometry geometry;
    };
    double tolerance_m_;
    double relative_;
    std::unordered_map<std::uint64_t, Entry> entries_;
    std::uint64_t hits_ = 0, misses_ = 0, samples_ = 0;
};

}  // namespace em
}  // namespace simcore
