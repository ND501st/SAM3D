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
//   * The earth's curve, 10 MHz to 1 GHz: ITU-R P.526-16's smooth spherical earth (section 3.2, the residue series' first
//     term past the horizon), joined to the terrain's edges by section 4.5.2's eq. (66). One knife edge over the bulge lost
//     far too little past the horizon (24 dB at 670 km for a 3 m whip and an 8,000 m listener; the first term 131 dB).
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

#include <algorithm>
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

// --------------------------------------------------------------------------- the smooth spherical earth
//
// ITU-R P.526-16 (11/2025) section 3, "Diffraction over a spherical Earth", for 10 MHz and above. Past the radio horizon
// only the first term of the residue series counts (section 3.1), the product of a distance term and two height-gain terms:
//   20 log10(E/E0) = F(X) + G(Y1) + G(Y2)                                       eq. (13)
//   X = 2.188 beta f^(1/3) ae^(-2/3) d,   Y = 9.575e-3 beta f^(2/3) ae^(-1/3) h  eqs. (14a), (15a)   (f MHz, ae km, d km, h m)
//   F(X) = 11 + 10 log10 X - 17.6 X (X >= 1.6),  -20 log10 X - 5.6488 X^1.425 (X < 1.6)            eqs. (17a), (17b)
//   G(Y) = 17.6 (B - 1.1)^0.5 - 5 log10(B - 1.1) - 8 (B > 2),  20 log10(B + 0.1 B^3) (B <= 2),  B = beta Y,
//          never below 2 + 20 log10 K                                          eqs. (18), (18a), (18b)
// with K the surface admittance (eqs. 11a, 12a) and beta from it (eq. 16). Section 3.2 makes it a loss for any distance:
// beyond the marginal line-of-sight distance (eq. 21) the first term; short of it, none if the ray clears the curved earth
// by 0.552 of the first Fresnel radius (eqs. 22-23), else the first term over a notional earth just grazed at this distance
// (eq. 24), scaled down by the clearance (eq. 25). The ground is the land of section 3.1.1.1's Fig. 2 (epsilon 15, sigma
// 1e-3 S/m), vertically polarised (the radios' whips); at these frequencies the ground changes only G's floor.
constexpr double kSphericalEarthMinFreqHz = 10e6;  // section 3: below 10 MHz only "LFMF-SmoothEarth" (ITU-R P.368)
constexpr double kGroundPermittivity = 15.0;       // P.526-16 Fig. 2's medium land
constexpr double kGroundConductivitySm = 1e-3;

// Normalised surface admittance K, eqs. (11a) and (12a): f MHz, ae km.
inline double SurfaceAdmittanceK(double freq_mhz, double ae_km, bool vertical = true, double permittivity = kGroundPermittivity,
                                 double conductivity_s_m = kGroundConductivitySm) {
    const double x = 18000.0 * conductivity_s_m / freq_mhz;
    const double kh = 0.36 * std::pow(ae_km * freq_mhz, -1.0 / 3.0) *
                      std::pow((permittivity - 1.0) * (permittivity - 1.0) + x * x, -0.25);
    return vertical ? kh * std::sqrt(permittivity * permittivity + x * x) : kh;
}

// beta from K, eq. (16).
inline double SphericalBeta(double k) {
    const double k2 = k * k, k4 = k2 * k2;
    return (1.0 + 1.6 * k2 + 0.67 * k4) / (1.0 + 4.5 * k2 + 1.53 * k4);
}

// The distance term F(X) (dB), eqs. (17a) and (17b).
inline double SphericalDistanceTermDb(double x) {
    if (x <= 0.0) return 0.0;
    if (x >= 1.6) return 11.0 + 10.0 * std::log10(x) - 17.6 * x;
    return -20.0 * std::log10(x) - 5.6488 * std::pow(x, 1.425);
}

// The height-gain term G(Y) (dB), eqs. (18), (18a), (18b) and the floor 2 + 20 log10 K.
inline double SphericalHeightGainDb(double y, double beta, double k) {
    const double b = beta * y;
    double g;
    if (b > 2.0) g = 17.6 * std::sqrt(b - 1.1) - 5.0 * std::log10(b - 1.1) - 8.0;
    else g = 20.0 * std::log10(std::max(b + 0.1 * b * b * b, 1e-30));
    const double least = 2.0 + 20.0 * std::log10(std::max(k, 1e-30));
    return g < least ? least : g;
}

// The residue series' first term, 20 log10(E/E0) in dB (eq. 13; generally negative), for antennas h1, h2 (m) above a
// sphere of radius ae (m), d (m) apart along it, at freq_hz.
inline double SphericalFirstTermDb(double d_m, double h1_m, double h2_m, double freq_hz, double ae_m) {
    const double f = freq_hz * 1e-6, ae = ae_m * 1e-3, d = d_m * 1e-3;
    const double k = SurfaceAdmittanceK(f, ae);
    const double beta = SphericalBeta(k);
    const double cf = std::cbrt(f), ca = std::cbrt(ae);
    const double x = 2.188 * beta * cf / (ca * ca) * d;
    const double ky = 9.575e-3 * beta * cf * cf / ca;
    return SphericalDistanceTermDb(x) + SphericalHeightGainDb(ky * h1_m, beta, k) + SphericalHeightGainDb(ky * h2_m, beta, k);
}

// Section 3.2's geometry, the same at every frequency: whether the path is past the marginal line of sight (eq. 21) and,
// short of it, the smallest clearance h between the curved earth and the ray and d1 d2 / d at that point (eqs. 22-22e),
// so that h_req (eq. 23) is 0.552 sqrt(fresnel_m2 lambda).
struct SphericalClearance {
    bool beyond = false;
    double h_m = 0.0;
    double fresnel_m2 = 0.0;
};
inline SphericalClearance SphericalClearanceOf(double d_m, double h1, double h2, double ae_m) {
    SphericalClearance out;
    const double d_los = std::sqrt(2.0 * ae_m) * (std::sqrt(h1) + std::sqrt(h2));  // eq. (21)
    if (d_m >= d_los) {
        out.beyond = true;
        return out;
    }
    // h1 + h2 > 0 here, since d >= 1 m.
    const double c = (h1 - h2) / (h1 + h2);
    const double m = d_m * d_m / (4.0 * ae_m * (h1 + h2));
    double arg = 1.5 * c * std::sqrt(3.0 * m / ((m + 1.0) * (m + 1.0) * (m + 1.0)));
    arg = arg < -1.0 ? -1.0 : (arg > 1.0 ? 1.0 : arg);
    const double b = 2.0 * std::sqrt((m + 1.0) / (3.0 * m)) * std::cos(kPi / 3.0 + std::acos(arg) / 3.0);
    const double d1 = 0.5 * d_m * (1.0 + b);
    const double d2 = d_m - d1;
    out.h_m = ((h1 - d1 * d1 / (2.0 * ae_m)) * d2 + (h2 - d2 * d2 / (2.0 * ae_m)) * d1) / d_m;
    out.fresnel_m2 = std::max(d1 * d2, 0.0) / d_m;
    return out;
}

// Section 3.2's diffraction loss (dB, >= 0) over a smooth sphere of effective radius ae (m) for any distance, eqs. (21)-(25).
// 0 below 10 MHz (section 3) and for degenerate input.
inline double SphericalEarthLossDb(double d_m, double h1_m, double h2_m, double freq_hz, double ae_m) {
    if (freq_hz < kSphericalEarthMinFreqHz || d_m < 1.0 || ae_m <= 0.0) return 0.0;
    const double h1 = h1_m > 0.0 ? h1_m : 0.0, h2 = h2_m > 0.0 ? h2_m : 0.0;
    const SphericalClearance cl = SphericalClearanceOf(d_m, h1, h2, ae_m);
    if (cl.beyond) {
        const double a = -SphericalFirstTermDb(d_m, h1, h2, freq_hz, ae_m);
        return a > 0.0 ? a : 0.0;
    }
    const double h = cl.h_m;
    const double h_req = 0.552 * std::sqrt(cl.fresnel_m2 * WavelengthOf(freq_hz));  // eq. (23)
    if (h > h_req) return 0.0;
    const double s = std::sqrt(h1) + std::sqrt(h2);
    const double aem = 0.5 * (d_m / s) * (d_m / s);  // eq. (24)
    const double ah = -SphericalFirstTermDb(d_m, h1, h2, freq_hz, aem);
    if (ah < 0.0) return 0.0;
    return h_req > 0.0 ? (1.0 - h / h_req) * ah : ah;  // eq. (25)
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
    // ITU-R P.526-16 section 4.5.2's smooth path: the antennas' heights above a smooth surface fitted to the profile (eqs.
    // 58-64; 0: not worked out, a path under 1 m), the earth's effective radius, and the same edge as worst_v_geometry
    // found over that smooth surface with the earth's bulge alone. DiffractionLossDb adds the spherical earth's loss over
    // what this smooth edge already costs (eq. 66).
    double smooth_h1_m = 0.0;
    double smooth_h2_m = 0.0;
    double earth_radius_m = 0.0;
    double smooth_v_geometry = -1e9;
    SphericalClearance smooth_clearance;  // section 3.2's geometry over the smooth surface (frequency-free; a path clear of
                                          // h_req at a band skips the spherical term's work)
    int samples = 0;  // profile samples taken (for profiling)
};

// Sample the terrain profile between two ENU points. Endpoint altitudes are above the ellipsoid
// (LocalFrame::AltitudeOf). Spacing is relaxed for long paths so a profile never exceeds
// `max_samples` (the profile still catches ridges wider than the spacing).
// Woods and buildings (Terrain::ObstacleHeightAt, roadmap E9) are added to the profile as ITU-R P.1812 adds its
// representative clutter heights (itu_r_p1812), beyond `obstacle_skip_m` from either end (terrain.hpp kObstacleSkipM).
// Where the terrain has them as footprints (roadmap R6), each stretch between two samples is looked at at the terrain's
// own spacing wherever the ray runs within kFootprintNearRayM of what may stand there (ObstaclesAlong), and the highest
// point found over the ray joins the profile: a narrow building is neither stepped over nor widened to the sampling.
constexpr double kFootprintNearRayM = 10.0;  // a game value: below this a footprint matters to neither the sight nor the edge
// Below the ray by more than this, woods and buildings make no diffraction edge down to 30 MHz (v under -0.78 for a
// 10 m wavelength midway along 10 km): the profile does not look them up there.
constexpr double kObstacleFarBelowRayM = 100.0;
inline PathGeometry AnalysePath(const Terrain& terrain, const Vec3& a_enu, double a_alt_m, const Vec3& b_enu,
                                double b_alt_m, double spacing_m = 100.0, int max_samples = 600,
                                double k = kStandardRefractionK, double obstacle_skip_m = kObstacleSkipM) {
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
    const double range = g.ground_range_m;
    const auto bulge_at = [&](double s) { return s * (range - s) / (2.0 * re); };
    const auto ray_at = [&](double s) { return a_alt_m + dz * (s / range); };
    // The profile, ends excluded: (distance along, the ground with the woods and buildings on it away from the ends
    // and the Earth's bulge), in order.
    struct Point {
        double s, h;
    };
    std::vector<Point> profile;
    profile.reserve(static_cast<std::size_t>(n));
    const bool footprints = terrain.ObstacleSpacingM() > 0.0;
    const double tallest = terrain.ObstacleTallestM();
    const double ux = dx / range, uy = dy / range;
    for (int i = 1; i <= n; ++i) {
        const double s = static_cast<double>(i) * inv_n * range;
        if (footprints) {  // the stretch up to this sample: its highest footprint over the ray, if one comes near it
            const double s0 = static_cast<double>(i - 1) * inv_n * range;
            const double half = 0.5 * range;
            const double most_bulge = s0 <= half && half <= s ? bulge_at(half) : std::max(bulge_at(s0), bulge_at(s));
            const double below = std::min(ray_at(s0), ray_at(s)) - most_bulge - kFootprintNearRayM;
            Point worst{0.0, 0.0};
            double worst_over = -1e300;
            ObstaclesAlong(terrain, a_enu.x, a_enu.y, ux, uy, range, s0, s, below, obstacle_skip_m, [&](double si, double top) {
                const double h = top + bulge_at(si);
                if (h - ray_at(si) > worst_over && si > 0.0 && si < range) {
                    worst_over = h - ray_at(si);
                    worst = Point{si, h};
                }
                return true;
            });
            if (worst_over > -1e299) profile.push_back(worst);
        }
        if (i == n) break;
        const double x = a_enu.x + ux * s, y = a_enu.y + uy * s;
        double h = terrain.HeightAt(x, y);
        // What stands here, unless the ray runs so high over it that it could make no edge (kObstacleFarBelowRayM).
        if (s > obstacle_skip_m && range - s > obstacle_skip_m && h + tallest + bulge_at(s) >= ray_at(s) - kObstacleFarBelowRayM) {
            h = std::max(h, terrain.ObstacleTopAt(x, y));
        }
        profile.push_back(Point{s, h + bulge_at(s)});
    }
    const int count = static_cast<int>(profile.size());
    int worst_i = -1;
    for (int i = 0; i < count; ++i) {
        const double s = profile[static_cast<std::size_t>(i)].s;
        const double rest = range - s;
        const double h = profile[static_cast<std::size_t>(i)].h - ray_at(s);  // > 0: the ground is above the ray
        if (-h < g.min_clearance_m) g.min_clearance_m = -h;
        if (h >= 0.0) g.line_of_sight = false;
        const double vg = h * std::sqrt(1.0 / s + 1.0 / rest);
        if (vg > g.worst_v_geometry) {
            g.worst_v_geometry = vg;
            worst_i = i;
        }
    }
    // Deygout: if the main edge blocks the ray, look for the worst edge on each side of it against the line to its top.
    // The profile with the Earth's bulge added is used: straight lines stay straight in it, so the main ray and every
    // sub-path's ray are lines in these coordinates.
    if (worst_i >= 0 && g.worst_v_geometry >= 0.0) {
        auto pos = [&](int i) { return profile[static_cast<std::size_t>(i)].s; };
        auto ground_at = [&](int i) { return profile[static_cast<std::size_t>(i)].h; };
        const double sm = pos(worst_i);
        const double em = ground_at(worst_i);
        // A sub-edge must be a crest of its own, with a valley of at least kValleyM between it and the main edge:
        // the flat top of one wide ridge is one edge, not many.
        constexpr double kValleyM = 20.0;
        for (int side = 0; side < 2; ++side) {
            const bool before = side == 0;
            const double s0 = before ? 0.0 : sm;
            const double e0 = before ? a_alt_m : em;
            const double s1 = before ? sm : range;
            const double e1 = before ? em : b_alt_m;
            const double len = s1 - s0;
            if (len < 1.0) continue;
            double low = 1e18;  // lowest ground between the candidate and the main edge
            double& slot = before ? g.sub_v_geometry_a : g.sub_v_geometry_b;
            const int step = before ? -1 : 1;
            for (int i = worst_i + step; i >= 0 && i < count; i += step) {
                const double ground_here = ground_at(i);
                const double lower_of_crests = ground_here < em ? ground_here : em;
                if (low < lower_of_crests - kValleyM) {
                    const double si = pos(i);
                    const double line = e0 + (e1 - e0) * (si - s0) / len;
                    const double d0 = si - s0;
                    const double d1 = s1 - si;
                    if (d0 >= 1.0 && d1 >= 1.0) {
                        const double vg = (ground_here - line) * std::sqrt(1.0 / d0 + 1.0 / d1);
                        if (vg > slot) slot = vg;
                    }
                }
                if (ground_here < low) low = ground_here;
            }
        }
    }
    // ITU-R P.526-16 section 4.5.2: a smooth surface fitted to the profile by least squares (eqs. 58-60), lowered under the
    // highest obstruction of the straight line between the antennas (eqs. 61-62) and never above the ground under either
    // antenna (eq. 63); the antennas' heights above it (eq. 64). Flat-earth geometry, the woods and buildings on the profile,
    // the bare ground under the antennas.
    {
        const double ground_a = terrain.HeightAt(a_enu.x, a_enu.y);
        const double ground_b = terrain.HeightAt(b_enu.x, b_enu.y);
        double v1 = 0.0, v2 = 0.0;
        double prev_s = 0.0, prev_h = ground_a;
        double h_obs = -1e300, a_obt = -1e300, a_obr = -1e300;
        const auto add = [&](double s, double h) {
            const double ds = s - prev_s;
            v1 += ds * (h + prev_h);                                            // eq. (58)
            v2 += ds * (h * (2.0 * s + prev_s) + prev_h * (s + 2.0 * prev_s));  // eq. (59)
            prev_s = s;
            prev_h = h;
        };
        for (int i = 0; i < count; ++i) {
            const double s = profile[static_cast<std::size_t>(i)].s;
            const double h = profile[static_cast<std::size_t>(i)].h - bulge_at(s);
            add(s, h);
            const double obi = h - ray_at(s);  // eq. (61d)
            if (obi > h_obs) h_obs = obi;      // eq. (61a)
            // eqs. (61b), (61c): only an obstruction (obi > 0) can set the slopes the correction uses, which it applies
            // only when there is one
            if (obi > 0.0) {
                if (obi / s > a_obt) a_obt = obi / s;
                if (obi / (range - s) > a_obr) a_obr = obi / (range - s);
            }
        }
        add(range, ground_b);
        double hst = (2.0 * v1 * range - v2) / (range * range);  // eq. (60a)
        double hsr = (v2 - v1 * range) / (range * range);        // eq. (60b)
        if (h_obs > 0.0 && a_obt + a_obr > 0.0) {                // eqs. (62a)-(62f)
            hst -= h_obs * a_obt / (a_obt + a_obr);
            hsr -= h_obs * a_obr / (a_obt + a_obr);
        }
        if (hst > ground_a) hst = ground_a;  // eq. (63)
        if (hsr > ground_b) hsr = ground_b;
        g.smooth_h1_m = a_alt_m - hst;  // eq. (64)
        g.smooth_h2_m = b_alt_m - hsr;
        g.earth_radius_m = re;
        g.smooth_clearance = SphericalClearanceOf(range, g.smooth_h1_m > 0.0 ? g.smooth_h1_m : 0.0, g.smooth_h2_m > 0.0 ? g.smooth_h2_m : 0.0, re);
        // The edge over the smooth surface: the bulge above the ray from the antennas' heights over it (section 4.5.2's
        // smooth Bullington path, as this file's edge rather than Bullington's). The bulge less a line, weighted by
        // sqrt(1/s + 1/(d - s)), has one peak between the ends: found by golden section, not sample by sample.
        const double h1s = g.smooth_h1_m, h2s = g.smooth_h2_m;
        const auto smooth_vg = [&](double s) {
            return (bulge_at(s) - (h1s + (h2s - h1s) * (s / range))) * std::sqrt(1.0 / s + 1.0 / (range - s));
        };
        constexpr double kGolden = 0.6180339887498949;
        double lo = 1e-6 * range, hi = range - 1e-6 * range;
        double x1 = hi - kGolden * (hi - lo), x2 = lo + kGolden * (hi - lo);
        double f1 = smooth_vg(x1), f2 = smooth_vg(x2);
        for (int it = 0; it < 48; ++it) {
            if (f1 < f2) {
                lo = x1;
                x1 = x2;
                f1 = f2;
                x2 = lo + kGolden * (hi - lo);
                f2 = smooth_vg(x2);
            } else {
                hi = x2;
                x2 = x1;
                f2 = f1;
                x1 = hi - kGolden * (hi - lo);
                f1 = smooth_vg(x1);
            }
        }
        g.smooth_v_geometry = f1 > f2 ? f1 : f2;
    }
    return g;
}

// Above this the spherical earth's term is left out of DiffractionLossDb: the microwave links take the earth's curve through
// the two-ray reflection over the sphere (ComputeReflection, LinkLossCore), whose rays cancel near the horizon; P.526's
// interpolation inside the horizon would count the same cancelling twice (a 5 m radar at 10 GHz on a 50 m target at its
// 38 km horizon: two rays and the edge 19 dB one way, P.526 15 dB). Past the horizon the two rays lose less than P.526's first
// term (35 dB against 56 dB 22 km past it): left (docs/EM_ARCHITECTURE.md, "Smooth-earth diffraction").
constexpr double kSphericalEarthMaxFreqHz = 1e9;

// The terrain's edges alone (dB): the main edge plus Deygout's two sub-edges (ITU-R P.526 section 4.3; a ridge line costs
// more than its highest crest alone, as it does in measurements).
inline double TerrainEdgesLossDb(const PathGeometry& g, double freq_hz) {
    if (g.ground_range_m < 1.0 || freq_hz <= 0.0) return 0.0;
    const double k = std::sqrt(2.0 / WavelengthOf(freq_hz));
    double loss = KnifeEdgeLossDb(g.worst_v_geometry * k);
    if (g.sub_v_geometry_a > -1e8) loss += KnifeEdgeLossDb(g.sub_v_geometry_a * k);
    if (g.sub_v_geometry_b > -1e8) loss += KnifeEdgeLossDb(g.sub_v_geometry_b * k);
    return loss;
}

// Diffraction loss (dB) of a path at a frequency, ITU-R P.526-16 section 4.5.2's eq. (66) with this file's edges in place
// of Bullington's: L = L_edges(actual profile) + max(L_sph - L_edges(smooth profile), 0). The terrain's edges as before,
// plus the smooth spherical earth's loss (section 3.2) over what the edge of the bare bulge already costs, so a path over
// smooth ground takes the spherical earth's loss and a ridge adds its own. From 10 MHz (section 3) to 1 GHz
// (kSphericalEarthMaxFreqHz); the edges alone outside it.
inline double DiffractionLossDb(const PathGeometry& g, double freq_hz) {
    const double edges = TerrainEdgesLossDb(g, freq_hz);
    if (g.earth_radius_m <= 0.0 || freq_hz < kSphericalEarthMinFreqHz || freq_hz >= kSphericalEarthMaxFreqHz) return edges;
    // Short of the horizon with the clearance over h_req (eq. 23) the spherical term is zero: most paths stop here.
    const SphericalClearance& cl = g.smooth_clearance;
    if (!cl.beyond && cl.h_m > 0.552 * std::sqrt(cl.fresnel_m2 * WavelengthOf(freq_hz))) return edges;
    const double sph = SphericalEarthLossDb(g.ground_range_m, g.smooth_h1_m, g.smooth_h2_m, freq_hz, g.earth_radius_m);
    if (sph <= 0.0) return edges;
    const double smooth = KnifeEdgeLossDb(g.smooth_v_geometry * std::sqrt(2.0 / WavelengthOf(freq_hz)));
    return edges + (sph > smooth ? sph - smooth : 0.0);
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
