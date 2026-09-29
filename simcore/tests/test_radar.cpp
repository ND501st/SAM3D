#include <cmath>
#include <vector>

#include "simcore/radar.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {
// Generic test radar — NOT any real system.
RadarParams GenericRadar() {
    RadarParams p;
    p.peak_power_w = 100e3;
    p.antenna_gain_db = 35.0;
    p.wavelength_m = 0.03;
    p.bandwidth_hz = 1e6;
    p.noise_figure_db = 5.0;
    p.system_losses_db = 6.0;
    p.beamwidth_3db_rad = DegToRad(2.0);
    p.detection_threshold_db = 13.0;
    return p;
}
}  // namespace

SC_TEST("radar: SNR falls 12.04 dB per doubling of range (R^4)") {
    const RadarParams p = GenericRadar();
    const double a = RadarSnrDb(p, 10000.0, 5.0);
    const double b = RadarSnrDb(p, 20000.0, 5.0);
    SC_EXPECT_NEAR(a - b, 40.0 * std::log10(2.0), 1e-9);
}

SC_TEST("radar: SNR scales linearly with RCS") {
    const RadarParams p = GenericRadar();
    SC_EXPECT_NEAR(RadarSnrDb(p, 15000.0, 10.0) - RadarSnrDb(p, 15000.0, 1.0), 10.0, 1e-9);
}

SC_TEST("radar: hand-computed SNR for the generic radar") {
    // Pt G^2 l^2 s / ((4pi)^3 R^4 k T0 B F L), computed independently here.
    const RadarParams p = GenericRadar();
    const double g = std::pow(10.0, 3.5);
    const double num = 100e3 * g * g * 0.03 * 0.03 * 5.0;
    const double den = std::pow(4.0 * kPi, 3) * std::pow(20000.0, 4) * 1.380649e-23 * 290.0 * 1e6 *
                       std::pow(10.0, 0.5) * std::pow(10.0, 0.6);
    SC_EXPECT_NEAR(RadarSnrDb(p, 20000.0, 5.0), 10.0 * std::log10(num / den), 1e-9);
}

SC_TEST("radar: detection range gives SNR exactly at threshold") {
    const RadarParams p = GenericRadar();
    const double r = RadarDetectionRange(p, 5.0);
    SC_EXPECT_NEAR(RadarSnrDb(p, r, 5.0), p.detection_threshold_db, 1e-9);
    SC_EXPECT(r > 30000.0 && r < 50000.0);
}

SC_TEST("radar: Gaussian beam loss is -3 dB one-way (-6 dB two-way) at half the 3 dB beamwidth") {
    const double bw = DegToRad(2.0);
    SC_EXPECT_NEAR(BeamShapeLossDb(bw / 2.0, bw), -6.0, 1e-9);
    SC_EXPECT_NEAR(BeamShapeLossDb(0.0, bw), 0.0, 1e-12);
}

SC_TEST("radar: 4/3-Earth horizon, 10 m antenna vs 100 m target is ~54 km") {
    const double d = RadarHorizonRange(10.0, 100.0);
    SC_EXPECT(d > 53000.0 && d < 55000.0);
}

SC_TEST("radar: line of sight over flat Earth agrees with the horizon formula") {
    const FlatTerrain flat(0.0);
    const double h_radar = 10.0, h_target = 300.0;
    const double horizon = RadarHorizonRange(h_radar, h_target);
    const Vec3 radar{0.0, 0.0, h_radar};
    SC_EXPECT(HasLineOfSight(flat, radar, h_radar, Vec3{0.0, horizon * 0.97, h_target}, h_target, 20.0));
    SC_EXPECT(!HasLineOfSight(flat, radar, h_radar, Vec3{0.0, horizon * 1.03, h_target}, h_target, 20.0));
}

SC_TEST("radar: a ridge masks a low target but not a high one") {
    // 1 km grid, 20x20 km, with a 400 m ridge running east-west at north = 10 km.
    const int n = 21;
    std::vector<double> h(static_cast<size_t>(n * n), 0.0);
    for (int col = 0; col < n; ++col) h[static_cast<size_t>(10 * n + col)] = 400.0;
    const GridTerrain terrain(0.0, 0.0, 1000.0, n, n, h);
    SC_EXPECT_NEAR(terrain.HeightAt(5000.0, 10000.0), 400.0, 1e-9);
    SC_EXPECT_NEAR(terrain.HeightAt(5000.0, 9500.0), 200.0, 1e-9);  // bilinear

    const Vec3 radar{10000.0, 1000.0, 10.0};
    SC_EXPECT(!HasLineOfSight(terrain, radar, 10.0, Vec3{10000.0, 19000.0, 300.0}, 300.0));
    SC_EXPECT(HasLineOfSight(terrain, radar, 10.0, Vec3{10000.0, 19000.0, 3000.0}, 3000.0));
}

SC_TEST("radar: EvaluateDetection combines range, LOS and SNR") {
    const RadarParams p = GenericRadar();
    const LocalFrame frame(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    const FlatTerrain flat(0.0);
    const Vec3 radar{0.0, 0.0, 10.0};
    const DetectionResult near_high = EvaluateDetection(p, frame, flat, radar, Vec3{}, Vec3{0.0, 20000.0, 5000.0}, 5.0);
    SC_EXPECT(near_high.detected);
    SC_EXPECT_NEAR(RadToDeg(near_high.azimuth_rad), 0.0, 1e-9);
    const DetectionResult far_high = EvaluateDetection(p, frame, flat, radar, Vec3{}, Vec3{0.0, 80000.0, 5000.0}, 5.0);
    SC_EXPECT(!far_high.detected);  // beyond SNR detection range
    // A target 20 m above the ellipsoid ~55 km away is below the radar horizon (~31 km).
    // Note: ENU z is NOT altitude far from the origin, so build the point from geodetic coords.
    const Vec3 low_far = frame.GeodeticToEnu(GeoPoint::FromDegrees(45.5, 25.0, 20.0));
    const DetectionResult masked = EvaluateDetection(p, frame, flat, radar, Vec3{}, low_far, 5.0);
    SC_EXPECT(!masked.line_of_sight);
    const Vec3 low_near = frame.GeodeticToEnu(GeoPoint::FromDegrees(45.2, 25.0, 20.0));
    SC_EXPECT(EvaluateDetection(p, frame, flat, radar, Vec3{}, low_near, 5.0).line_of_sight);
}
