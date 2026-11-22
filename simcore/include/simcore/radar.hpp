// simcore/radar.hpp — generic radar physics (textbook level).
//
// References (see docs/SOURCES.md):
//   * Skolnik, "Introduction to Radar Systems", ch. 1–2 (radar range equation).
//   * Skolnik, "Radar Handbook", propagation chapter (4/3 effective Earth radius).
//   * Mahafza, "Radar Systems Analysis and Design Using MATLAB" (Gaussian beam approximation).
//
// Everything here is system-agnostic. Per-system numbers live in data/systems/
// and must be calibrated from cited public sources.
#pragma once

#include <algorithm>
#include <cmath>

#include "simcore/geo.hpp"
#include "simcore/terrain.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

constexpr double kBoltzmann = 1.380649e-23;   // J/K
constexpr double kReferenceNoiseTemp = 290.0;  // K
constexpr double kStandardRefractionK = 4.0 / 3.0;

inline double DbToLinear(double db) { return std::pow(10.0, db / 10.0); }
inline double LinearToDb(double lin) { return 10.0 * std::log10(lin); }

struct RadarParams {
    double peak_power_w = 0.0;
    double antenna_gain_db = 0.0;     // one-way, same antenna for Tx/Rx
    double wavelength_m = 0.0;
    double bandwidth_hz = 0.0;        // receiver noise bandwidth
    double noise_figure_db = 0.0;
    double system_losses_db = 0.0;    // total losses (Tx, Rx, processing, atmosphere...)
    double beamwidth_3db_rad = 0.0;   // for the Gaussian beam-shape loss
    double detection_threshold_db = 13.0;  // single-pulse SNR needed for a detection
    double min_range_m = 0.0;
    double max_instrumented_range_m = 0.0;  // 0 = unlimited
};

// Single-pulse signal-to-noise ratio (dB) from the monostatic radar range equation:
//   SNR = Pt G^2 lambda^2 sigma / ((4 pi)^3 R^4 k T0 B F L)
inline double RadarSnrDb(const RadarParams& p, double range_m, double rcs_m2) {
    if (range_m <= 0.0 || rcs_m2 <= 0.0) return -300.0;
    const double g = DbToLinear(p.antenna_gain_db);
    const double numerator = p.peak_power_w * g * g * p.wavelength_m * p.wavelength_m * rcs_m2;
    const double four_pi_cubed = std::pow(4.0 * kPi, 3.0);
    const double noise = kBoltzmann * kReferenceNoiseTemp * p.bandwidth_hz * DbToLinear(p.noise_figure_db);
    const double denominator = four_pi_cubed * std::pow(range_m, 4.0) * noise * DbToLinear(p.system_losses_db);
    return LinearToDb(numerator / denominator);
}

// Range (m) at which a target of `rcs_m2` gives exactly the detection threshold (free space).
inline double RadarDetectionRange(const RadarParams& p, double rcs_m2) {
    const double snr_at_1m = DbToLinear(RadarSnrDb(p, 1.0, rcs_m2));
    return std::pow(snr_at_1m / DbToLinear(p.detection_threshold_db), 0.25);
}

// Two-way gain loss (dB, <= 0) for a target `off_axis_rad` from boresight, Gaussian beam:
//   one-way loss = -12 (theta / theta_3dB)^2 dB  -> two-way doubles it.
inline double BeamShapeLossDb(double off_axis_rad, double beamwidth_3db_rad) {
    if (beamwidth_3db_rad <= 0.0) return 0.0;
    const double r = off_axis_rad / beamwidth_3db_rad;
    return -2.0 * 12.0 * r * r;
}

// Radar horizon (m) between two heights above a smooth Earth with effective radius k*R.
inline double RadarHorizonRange(double radar_height_m, double target_height_m,
                                double k = kStandardRefractionK) {
    const double re = k * wgs84::kMeanRadius;
    const double h1 = radar_height_m > 0.0 ? radar_height_m : 0.0;
    const double h2 = target_height_m > 0.0 ? target_height_m : 0.0;
    return std::sqrt(2.0 * re * h1) + std::sqrt(2.0 * re * h2);
}

// Line of sight between two ENU points over terrain, using the classic path-profile
// method: a straight ray over an Earth "bulge" of s(D-s)/(2kR). Endpoint heights are
// altitudes above the ellipsoid (use LocalFrame::AltitudeOf). Small-area approximation,
// fine for ranges below ~300 km. Woods and buildings (Terrain::ObstacleTopAt) stand on
// the profile beyond `obstacle_skip_m` from either end (roadmap E9, terrain.hpp kObstacleSkipM); where the terrain has
// them as footprints, between the samples too at its own spacing wherever the ray runs low enough to meet one
// (ObstaclesAlong, roadmap R6), so a narrow building is neither stepped over nor widened to the sampling.
inline bool HasLineOfSight(const Terrain& terrain, const Vec3& a_enu, double a_alt_m,
                           const Vec3& b_enu, double b_alt_m, double sample_spacing_m = 50.0,
                           double k = kStandardRefractionK, double clearance_m = 0.0,
                           double obstacle_skip_m = kObstacleSkipM) {
    const double dx = b_enu.x - a_enu.x;
    const double dy = b_enu.y - a_enu.y;
    const double ground_dist = std::sqrt(dx * dx + dy * dy);
    if (ground_dist < 1.0) return true;
    const double re = k * wgs84::kMeanRadius;
    int samples = static_cast<int>(ground_dist / sample_spacing_m);
    if (samples < 2) samples = 2;
    const double tallest = terrain.ObstacleTallestM();
    for (int i = 1; i < samples; ++i) {
        const double t = static_cast<double>(i) / samples;
        const double s = t * ground_dist;
        const double ray_alt = a_alt_m + (b_alt_m - a_alt_m) * t;
        const double bulge = s * (ground_dist - s) / (2.0 * re);
        const double x = a_enu.x + dx * t, y = a_enu.y + dy * t;
        double ground = terrain.HeightAt(x, y);
        if (s > obstacle_skip_m && ground_dist - s > obstacle_skip_m && ground + tallest + bulge + clearance_m >= ray_alt) {
            ground = std::max(ground, terrain.ObstacleTopAt(x, y));
        }
        if (ground + bulge + clearance_m >= ray_alt) return false;
    }
    if (terrain.ObstacleSpacingM() <= 0.0) return true;
    // The footprints between the samples: each stretch looked at where the ray over it runs lower than what may stand there.
    const double ux = dx / ground_dist, uy = dy / ground_dist;
    const auto ray_at = [&](double s) { return a_alt_m + (b_alt_m - a_alt_m) * (s / ground_dist); };
    const auto bulge_at = [&](double s) { return s * (ground_dist - s) / (2.0 * re); };
    bool blocked = false;
    for (int i = 1; i <= samples && !blocked; ++i) {
        const double s0 = ground_dist * static_cast<double>(i - 1) / samples, s1 = ground_dist * static_cast<double>(i) / samples;
        const double half = 0.5 * ground_dist;
        const double most_bulge = s0 <= half && half <= s1 ? bulge_at(half) : std::max(bulge_at(s0), bulge_at(s1));
        const double below = std::min(ray_at(s0), ray_at(s1)) - most_bulge - clearance_m;
        ObstaclesAlong(terrain, a_enu.x, a_enu.y, ux, uy, ground_dist, s0, s1, below, obstacle_skip_m, [&](double s, double top) {
            blocked = top + bulge_at(s) + clearance_m >= ray_at(s);
            return !blocked;
        });
    }
    return !blocked;
}

struct DetectionResult {
    bool line_of_sight = false;
    bool in_range = false;
    bool detected = false;
    double range_m = 0.0;
    double azimuth_rad = 0.0;
    double elevation_rad = 0.0;
    double snr_db = -300.0;
};

// Evaluate whether a radar at `radar_enu` (pointing at `boresight`, or omnidirectional
// search if boresight is the zero vector) detects a target at `target_enu`.
inline DetectionResult EvaluateDetection(const RadarParams& p, const LocalFrame& frame,
                                         const Terrain& terrain, const Vec3& radar_enu,
                                         const Vec3& boresight, const Vec3& target_enu,
                                         double rcs_m2) {
    DetectionResult r;
    const Vec3 los = target_enu - radar_enu;
    r.range_m = los.Length();
    r.azimuth_rad = AzimuthOf(los);
    r.elevation_rad = ElevationOf(los);
    r.in_range = r.range_m >= p.min_range_m &&
                 (p.max_instrumented_range_m <= 0.0 || r.range_m <= p.max_instrumented_range_m);
    r.line_of_sight = HasLineOfSight(terrain, radar_enu, frame.AltitudeOf(radar_enu), target_enu,
                                     frame.AltitudeOf(target_enu));
    double snr = RadarSnrDb(p, r.range_m, rcs_m2);
    if (boresight.LengthSquared() > 0.0) snr += BeamShapeLossDb(AngleBetween(boresight, los), p.beamwidth_3db_rad);
    r.snr_db = snr;
    r.detected = r.in_range && r.line_of_sight && snr >= p.detection_threshold_db;
    return r;
}

}  // namespace simcore
