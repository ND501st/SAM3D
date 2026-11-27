// Electromagnetic layer: the maths, checked against hand calculations and textbook values.
// Every number here is a generic placeholder, not data for any real system.
#include <cmath>
#include <vector>

#include "simcore/em.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::em;

// --------------------------------------------------------------------------- random

SC_TEST("em random: draws are pure functions of seed and keys") {
    Random a(42), b(42), c(43);
    SC_EXPECT(a.Uniform(Stream::Detection, 1, 2, 3) == b.Uniform(Stream::Detection, 1, 2, 3));
    SC_EXPECT(a.Uniform(Stream::Detection, 1, 2, 3) != c.Uniform(Stream::Detection, 1, 2, 3));
    SC_EXPECT(a.Uniform(Stream::Detection, 1, 2, 3) != a.Uniform(Stream::RangeNoise, 1, 2, 3));
    // Python mirror (prototype/samproto/em.py) must give the same first values.
    SC_EXPECT_NEAR(Uniform01(Mix64(0)), 0.8833108082, 1e-9);
    // Moments of the normal stream.
    double sum = 0.0, sum2 = 0.0;
    const int n = 20000;
    for (int i = 0; i < n; ++i) {
        const double x = a.Normal(Stream::AzimuthNoise, static_cast<std::uint64_t>(i));
        sum += x;
        sum2 += x * x;
    }
    SC_EXPECT_NEAR(sum / n, 0.0, 0.03);
    SC_EXPECT_NEAR(sum2 / n, 1.0, 0.04);
}

// --------------------------------------------------------------------------- propagation

SC_TEST("em propagation: free-space loss and knife-edge diffraction (ITU-R P.526)") {
    // 10 km at 1 GHz: 20 log10(4 pi 1e4 / 0.29979) = 112.45 dB.
    SC_EXPECT_NEAR(FreeSpacePathLossDb(10000.0, 1e9), 112.45, 0.01);
    // J(v): ~6 dB at grazing (v = 0), 0 below v = -0.78, ~20.5 dB at v = 2.4 (P.526 Fig. 9).
    SC_EXPECT_NEAR(KnifeEdgeLossDb(0.0), 6.03, 0.01);
    SC_EXPECT_NEAR(KnifeEdgeLossDb(-0.8), 0.0, 1e-12);
    SC_EXPECT_NEAR(KnifeEdgeLossDb(2.4), 20.54, 0.05);
}

SC_TEST("em propagation: a ridge blinds a microwave radar but not a VHF radio") {
    // A 300 m ridge halfway along a 20 km path between two antennas 5 m above flat ground.
    std::vector<double> h(101 * 3, 0.0);
    for (int r = 0; r < 3; ++r)
        for (int c = 48; c <= 52; ++c) h[static_cast<std::size_t>(r * 101 + c)] = 300.0;
    GridTerrain ridge(-1000.0, -100.0, 200.0, 101, 3, h);  // columns along east, 200 m apart
    const Vec3 a{0.0, 0.0, 5.0}, b{20000.0, 0.0, 5.0};
    const PathGeometry g = AnalysePath(ridge, a, 5.0, b, 5.0, 50.0);
    SC_EXPECT(!g.line_of_sight);
    const double vhf = DiffractionLossDb(g, 50e6), cband = DiffractionLossDb(g, 6e9);
    // v = h sqrt(2/lambda (1/d1 + 1/d2)): h ~ 300 m over the bulge; at 50 MHz v ~ 1.8, at 6 GHz v ~ 19.
    SC_EXPECT(vhf > 12.0 && vhf < 22.0);
    SC_EXPECT(cband > 38.0);
    // Clear path: no diffraction loss at either band.
    const PathGeometry clear = AnalysePath(FlatTerrain(0.0), a, 200.0, b, 200.0, 50.0);
    SC_EXPECT(clear.line_of_sight);
    SC_EXPECT_NEAR(DiffractionLossDb(clear, 6e9), 0.0, 1e-9);
}

namespace {
// Two ridges of the same height across the path (east-west), each 400 m wide.
class TwoRidges final : public Terrain {
public:
    explicit TwoRidges(double h2) : h2_(h2) {}
    double HeightAt(double east, double /*north*/) const override {
        if (east > 6800.0 && east < 7200.0) return 300.0;
        if (east > 12800.0 && east < 13200.0) return h2_;
        return 0.0;
    }

private:
    double h2_;
};
}  // namespace

SC_TEST("em propagation: two ridges cost more than the higher one alone (Deygout, ITU-R P.526 sec. 4.3)") {
    const Vec3 a{0.0, 0.0, 5.0}, b{20000.0, 0.0, 5.0};
    const PathGeometry both = AnalysePath(TwoRidges(300.0), a, 5.0, b, 5.0, 50.0);
    const PathGeometry one = AnalysePath(TwoRidges(0.0), a, 5.0, b, 5.0, 50.0);
    SC_EXPECT(both.sub_v_geometry_a > -1e8 || both.sub_v_geometry_b > -1e8);
    const double l_both = DiffractionLossDb(both, 50e6);
    const double l_one = DiffractionLossDb(one, 50e6);
    std::printf("    VHF: one ridge %.1f dB, two %.1f dB; 6 GHz: %.1f, %.1f dB\n", l_one, l_both, DiffractionLossDb(one, 6e9),
                DiffractionLossDb(both, 6e9));
    SC_EXPECT(l_both > l_one + 5.0);
    // the terrain's edges are exactly the three knife edges: J(main) + J(sub a) + J(sub b); the spherical earth's term (20 km
    // is past the 18 km horizon of two 5 m masts) comes on top of them, never in place of them (P.526 eq. 66)
    const double k = std::sqrt(2.0 / WavelengthOf(50e6));
    double expect = KnifeEdgeLossDb(both.worst_v_geometry * k);
    if (both.sub_v_geometry_a > -1e8) expect += KnifeEdgeLossDb(both.sub_v_geometry_a * k);
    if (both.sub_v_geometry_b > -1e8) expect += KnifeEdgeLossDb(both.sub_v_geometry_b * k);
    SC_EXPECT_NEAR(TerrainEdgesLossDb(both, 50e6), expect, 1e-9);
    SC_EXPECT(l_both >= expect - 1e-9);
    // a path the main edge does not block has no sub-edges
    const PathGeometry high = AnalysePath(TwoRidges(300.0), Vec3{0.0, 0.0, 900.0}, 900.0, Vec3{20000.0, 0.0, 900.0}, 900.0, 50.0);
    SC_EXPECT(high.line_of_sight && high.sub_v_geometry_a < -1e8 && high.sub_v_geometry_b < -1e8);
    // a single ridge is unchanged: the old single-edge value (test above)
    SC_EXPECT(one.sub_v_geometry_a < -1e8 || one.sub_v_geometry_b < -1e8 || DiffractionLossDb(one, 50e6) > 0.0);
}

SC_TEST("em propagation: the smooth spherical earth's terms against hand calculations (ITU-R P.526-16 section 3.1.1)") {
    // F(X), eqs. (17a) and (17b), and their join at X = 1.6.
    SC_EXPECT_NEAR(SphericalDistanceTermDb(2.0), 11.0 + 10.0 * std::log10(2.0) - 35.2, 1e-9);  // -21.190
    SC_EXPECT_NEAR(SphericalDistanceTermDb(1.0), -5.6488, 1e-9);
    SC_EXPECT_NEAR(SphericalDistanceTermDb(1.6 - 1e-9), SphericalDistanceTermDb(1.6), 0.01);  // -15.1188 and -15.1182
    // G(Y), eqs. (18), (18a): B = 3 gives 17.6 sqrt(1.9) - 5 log10(1.9) - 8 = 14.866; B = 1 gives 20 log10(1.1) = 0.828; the
    // two forms meet within 0.02 dB at B = 2; the floor 2 + 20 log10 K holds for an antenna on the ground.
    SC_EXPECT_NEAR(SphericalHeightGainDb(3.0, 1.0, 0.01), 14.8662, 1e-3);
    SC_EXPECT_NEAR(SphericalHeightGainDb(1.0, 1.0, 0.01), 0.8279, 1e-3);
    SC_EXPECT_NEAR(SphericalHeightGainDb(2.0, 1.0, 0.01), SphericalHeightGainDb(2.0 + 1e-9, 1.0, 0.01), 0.02);
    SC_EXPECT_NEAR(SphericalHeightGainDb(1e-4, 1.0, 0.02), 2.0 + 20.0 * std::log10(0.02), 1e-9);
    // K, eqs. (11a), (12a), for Fig. 2's land (epsilon 15, sigma 1e-3) at 100 MHz, ae 8,500 km: horizontal 0.00102, vertical
    // 0.0152 (the figure's curves read about 0.001 and 0.015 there); beta (eq. 16) is 1 to a thousandth, as section 3.1.1.2 says
    // for vertical polarisation above 20 MHz over land.
    SC_EXPECT_NEAR(SurfaceAdmittanceK(100.0, 8500.0, false), 0.0010157, 2e-6);
    SC_EXPECT_NEAR(SurfaceAdmittanceK(100.0, 8500.0, true), 0.015236, 2e-5);
    SC_EXPECT_NEAR(SphericalBeta(SurfaceAdmittanceK(30.0, 8500.0)), 1.0, 2e-3);
    // The first term by hand (eqs. 13-18): 50 MHz, a 3 m whip and a listener at 8,000 m, 500 km apart over ae 8,500 km.
    // K_V 0.019198, beta 0.998933; X = 2.188 beta 50^(1/3) 8500^(-2/3) 500 = 9.6664, F = -149.276; Y1 = 0.019083, G(Y1) =
    // -34.40 below its floor 2 + 20 log10 K = -32.335; Y2 = 50.887, G(Y2) = 107.634. 20 log10(E/E0) = -73.976 dB.
    SC_EXPECT_NEAR(SphericalFirstTermDb(500e3, 3.0, 8000.0, 50e6, 8500e3), -73.976, 0.005);
    SC_EXPECT_NEAR(SphericalEarthLossDb(500e3, 3.0, 8000.0, 50e6, 8500e3), 73.976, 0.005);
}

SC_TEST("em propagation: the smooth spherical earth for any distance (ITU-R P.526-16 section 3.2)") {
    const double ae = 8500e3;
    // Well inside the horizon with the first Fresnel zone clear: no loss (eq. 23).
    SC_EXPECT_NEAR(SphericalEarthLossDb(10e3, 100.0, 100.0, 100e6, ae), 0.0, 1e-12);
    // At the marginal line of sight (eq. 21) the interpolation (eqs. 24-25: clearance 0, the notional radius the real one)
    // meets the first term: no step at the horizon.
    const double d_los = std::sqrt(2.0 * ae) * (std::sqrt(3.0) + std::sqrt(8000.0));
    const double in = SphericalEarthLossDb(d_los - 1.0, 3.0, 8000.0, 50e6, ae);
    const double out = SphericalEarthLossDb(d_los + 1.0, 3.0, 8000.0, 50e6, ae);
    std::printf("    3 m to 8,000 m at 50 MHz: horizon %.1f km, loss %.2f dB just inside, %.2f dB just past\n", d_los / 1e3, in, out);
    SC_EXPECT_NEAR(in, out, 0.05);
    // Past it the loss grows with distance, a few tenths of a dB a km at 50 MHz; short of it it falls towards zero.
    double last = 0.0;
    for (double d = 100e3; d <= 700e3; d += 50e3) {
        const double l = SphericalEarthLossDb(d, 3.0, 8000.0, 50e6, ae);
        SC_EXPECT(l >= last - 1e-9);
        last = l;
    }
    SC_EXPECT(SphericalEarthLossDb(450e3, 3.0, 8000.0, 50e6, ae) > SphericalEarthLossDb(380e3, 3.0, 8000.0, 50e6, ae) + 15.0);
    // Below 10 MHz section 3 sends the user to P.368: none here.
    SC_EXPECT_NEAR(SphericalEarthLossDb(500e3, 3.0, 8000.0, 5e6, ae), 0.0, 1e-12);
}

SC_TEST("em propagation: past the horizon the smooth earth takes over from the one knife edge over the bulge (ITU-R P.526-16 eq. 66)") {
    const FlatTerrain flat(0.0);
    // A 3 m whip and a listening aircraft at 8,000 m, 670 km apart, at 50 MHz: the bulge's single knife edge lost about 24 dB;
    // the smooth earth past the 376 km horizon loses over a hundred.
    const PathGeometry g = AnalysePath(flat, Vec3{0.0, 0.0, 3.0}, 3.0, Vec3{0.0, 670e3, 8000.0}, 8000.0, 100.0);
    const double edges = TerrainEdgesLossDb(g, 50e6);
    const double all = DiffractionLossDb(g, 50e6);
    std::printf("    670 km: the bulge's knife edge %.1f dB, with the smooth earth %.1f dB\n", edges, all);
    SC_EXPECT(edges > 15.0 && edges < 35.0);
    SC_EXPECT(all > 120.0);
    // Over flat ground the fitted smooth surface is the ground itself (eqs. 58-64): the antennas' heights over it are their
    // heights above the ground, and the actual path's edge is the smooth one's (the smooth one's peak found exactly, the
    // profile's at its samples 1.1 km apart), so the loss is the larger of the two.
    SC_EXPECT_NEAR(g.smooth_h1_m, 3.0, 1e-6);
    SC_EXPECT_NEAR(g.smooth_h2_m, 8000.0, 1e-6);
    SC_EXPECT(g.smooth_v_geometry >= g.worst_v_geometry - 1e-9);
    SC_EXPECT_NEAR(g.smooth_v_geometry, g.worst_v_geometry, 1e-4 * std::fabs(g.worst_v_geometry));
    const double sph = SphericalEarthLossDb(g.ground_range_m, 3.0, 8000.0, 50e6, g.earth_radius_m);
    SC_EXPECT_NEAR(all, std::max(edges, sph), 0.01);
    // Raised flat ground: the surface follows it.
    const FlatTerrain plateau(200.0);
    const PathGeometry p = AnalysePath(plateau, Vec3{0.0, 0.0, 205.0}, 205.0, Vec3{0.0, 40e3, 230.0}, 230.0, 100.0);
    SC_EXPECT_NEAR(p.smooth_h1_m, 5.0, 1e-6);
    SC_EXPECT_NEAR(p.smooth_h2_m, 30.0, 1e-6);
    // A ridge adds its own edges on top of the smooth earth's excess (eq. 66 with this file's edges in Bullington's place).
    const PathGeometry r = AnalysePath(TwoRidges(300.0), Vec3{0.0, 0.0, 5.0}, 5.0, Vec3{60e3, 0.0, 5.0}, 5.0, 50.0);
    const double r_edges = TerrainEdgesLossDb(r, 50e6);
    const double r_sph = SphericalEarthLossDb(r.ground_range_m, r.smooth_h1_m, r.smooth_h2_m, 50e6, r.earth_radius_m);
    const double r_smooth = KnifeEdgeLossDb(r.smooth_v_geometry * std::sqrt(2.0 / WavelengthOf(50e6)));
    SC_EXPECT_NEAR(DiffractionLossDb(r, 50e6), r_edges + std::max(0.0, r_sph - r_smooth), 1e-9);
    SC_EXPECT(r.smooth_h1_m > 5.0);  // the surface lowered under the ridges (eqs. 61-62): the masts stand higher above it
    // Microwaves keep the edges alone (the two rays over the sphere take the curve: kSphericalEarthMaxFreqHz).
    SC_EXPECT_NEAR(DiffractionLossDb(g, 3e9), TerrainEdgesLossDb(g, 3e9), 1e-12);
}

SC_TEST("em propagation: rain attenuation follows ITU-R P.838 and costs microwaves, not VHF") {
    // 10 GHz at 50 mm/h: 0.01217 * 50^1.2571 = 1.66 dB/km (the familiar ~1.7)
    SC_EXPECT_NEAR(RainSpecificAttenuationDbPerKm(10e9, 50.0), 0.01217 * std::pow(50.0, 1.2571), 1e-9);
    SC_EXPECT_NEAR(RainSpecificAttenuationDbPerKm(10e9, 50.0), 1.66, 0.02);
    SC_EXPECT(RainSpecificAttenuationDbPerKm(15e9, 25.0) > RainSpecificAttenuationDbPerKm(6e9, 25.0) * 5.0);
    SC_EXPECT_NEAR(RainSpecificAttenuationDbPerKm(10e9, 0.0), 0.0, 1e-12);
    SC_EXPECT(RainSpecificAttenuationDbPerKm(50e6, 50.0) < 1e-3);  // below the table: the 1 GHz value, tiny
    // the long path is not wholly under the cell: 30 km at 10 GHz, 50 mm/h is far less than 30 x 1.66 dB
    const double path = RainLossDb(30000.0, 10e9, 50.0);
    SC_EXPECT(path < 30.0 * 1.66 * 0.7 && path > 10.0);
    std::printf("    rain, 30 km at 10 GHz, 50 mm/h: %.1f dB one way\n", path);
    // rain never enters a calibration
    EmWorld w(GeoPoint::FromDegrees(46.9, 19.7, 110.0), nullptr, 1);
    Environment env = w.Scene().env;
    env.rain_rate_mmh = 40.0;
    w.SetEnvironment(env);
    SC_EXPECT_NEAR(EmWorld::Dry(env).rain_rate_mmh, 0.0, 0.0);
}

SC_TEST("em propagation: gases cost little below 10 GHz and peak near 22 GHz") {
    SC_EXPECT_NEAR(GaseousLossDb(10000.0, 0.0, 0.0, 6e9), 0.09, 0.01);
    SC_EXPECT(GaseousAttenuationDbPerKm(22.2e9) > GaseousAttenuationDbPerKm(15e9));
    SC_EXPECT(GaseousLossDb(50000.0, 0.0, 10000.0, 10e9) < GaseousLossDb(50000.0, 0.0, 0.0, 10e9));
}

SC_TEST("em propagation: two-ray multipath lobes and the plane-earth law") {
    // Flat geometry check: heights 10 m and 100 m, 1 km, lambda 0.05 m.
    const ReflectionGeometry r = ComputeReflection(10.0, 100.0, 1000.0);
    SC_EXPECT(r.valid);
    SC_EXPECT_NEAR(r.path_difference_m, 2.0 * 10.0 * 100.0 / 1000.0, 0.01);
    // A full-strength reflection doubles the field at a lobe peak and nulls it at a minimum.
    ReflectionGeometry peak = r;
    peak.path_difference_m = 0.025;  // half a wavelength: phase pi, with the reversal -> in phase
    SC_EXPECT_NEAR(TwoRayFactor(peak, 0.05, 1.0), 2.0, 1e-9);
    ReflectionGeometry null = r;
    null.path_difference_m = 0.05;
    SC_EXPECT(TwoRayFactor(null, 0.05, 1.0) < 0.01);
    // Plane earth: F = 4 pi h1 h2 / (lambda d).
    SC_EXPECT_NEAR(PlaneEarthFactor(3.0, 3.0, 15000.0, 7.5), 4.0 * kPi * 9.0 / (7.5 * 15000.0), 1e-12);
    SC_EXPECT_NEAR(PlaneEarthFactor(30.0, 30.0, 100.0, 1.0), 1.0, 1e-12);
    // Rough ground scatters microwaves but still reflects VHF.
    Environment env;
    env.surface_reflection = 1.0;
    env.surface_roughness_m = 3.0;
    SC_EXPECT(EffectiveReflection(env, 0.01, 0.05) < 0.5);
    SC_EXPECT(EffectiveReflection(env, 0.01, 6.0) > 0.99);
}

SC_TEST("em propagation: path cache reuses a profile until an end moves") {
    PathCache cache(25.0);
    FlatTerrain flat(0.0);
    const Vec3 a{0, 0, 10}, b{10000, 0, 1000};
    cache.Get(PathCache::PairKey(1, 2), flat, a, 10.0, b, 1000.0);
    cache.Get(PathCache::PairKey(1, 2), flat, a, 10.0, b + Vec3{10, 0, 0}, 1000.0);
    SC_EXPECT(cache.Hits() == 1 && cache.Misses() == 1);
    cache.Get(PathCache::PairKey(1, 2), flat, a, 10.0, b + Vec3{100, 0, 0}, 1000.0);
    SC_EXPECT(cache.Misses() == 2);
}

// --------------------------------------------------------------------------- detection

SC_TEST("em detection: Swerling I probabilities and required SNR") {
    // ln(1e-6) / ln(0.9) - 1 = 130.1 (21.14 dB); ln(1e-6) / ln(0.5) - 1 = 18.93 (12.77 dB).
    SC_EXPECT_NEAR(LinearToDb(SwerlingIRequiredSnr(0.9, 1e-6)), 21.14, 0.01);
    SC_EXPECT_NEAR(LinearToDb(SwerlingIRequiredSnr(0.5, 1e-6)), 12.77, 0.01);
    SC_EXPECT_NEAR(SwerlingIPd(SwerlingIRequiredSnr(0.5, 1e-6), 1e-6), 0.5, 1e-9);
    SC_EXPECT(SwerlingIPd(1.0, 1e-6) < 0.01);
}

SC_TEST("em detection: measurement accuracy improves with SNR, down to its floor") {
    // c tau / (2 sqrt(2 SNR)): 1 us, SNR 100 -> 150 m / sqrt(200) = 10.6 m.
    SC_EXPECT_NEAR(RangeSigmaM(1e-6, 100.0, 0.0), 10.60, 0.01);
    SC_EXPECT_NEAR(AngleSigmaRad(DegToRad(1.0), 50.0, 1.6, 0.0), DegToRad(1.0) / (1.6 * 10.0), 1e-9);
    SC_EXPECT(AngleSigmaRad(0.02, 1e6, 1.6, 0.001) >= 0.001);
}

SC_TEST("em detection: barrage spreads its power, spot concentrates it") {
    SC_EXPECT_NEAR(JammerBandOverlap(6e9, 500e6, 6e9, 1e6), 1e6 / 500e6, 1e-12);
    SC_EXPECT_NEAR(JammerBandOverlap(6e9, 0.5e6, 6e9, 1e6), 1.0, 1e-12);
    SC_EXPECT_NEAR(JammerBandOverlap(9e9, 100e6, 6e9, 1e6), 0.0, 1e-12);
}

SC_TEST("em detection: self-screening burn-through range solves SNR / (1 + J/N) = needed") {
    const double snr1 = 1e20, jn1 = 1e9, req = 20.0;
    const double r = SelfScreenBurnThroughRangeM(snr1, jn1, req);
    const double snr = snr1 / std::pow(r, 4.0), jn = jn1 / (r * r);
    SC_EXPECT_NEAR(snr / (1.0 + jn), req, 1e-6);
    // Without jamming it is the plain detection range (K / req)^(1/4).
    SC_EXPECT_NEAR(SelfScreenBurnThroughRangeM(snr1, 0.0, req), std::pow(snr1 / req, 0.25), 1e-3);
}

static RadarDef GenericSearchRadar() {
    RadarDef d;
    d.id = "test_search";
    d.role = RadarRole::Search;
    d.emitter.waveform.freq_hz = 6e9;
    d.emitter.waveform.pulse_width_s = 1e-6;
    d.emitter.peak_power_w = 200000.0;
    d.emitter.antenna = {34.0, DegToRad(1.5), DegToRad(4.0), -25.0};
    d.emitter.scan = ScanKind::Circular;
    d.emitter.scan_period_s = 3.0;
    d.emitter.beams = {{DegToRad(3.0), DegToRad(6.0)}, {DegToRad(13.0), DegToRad(14.0)}};
    d.calibration = {true, 40000.0, 2.0, 3000.0, 0.5, "PLACEHOLDER"};
    d.antenna_height_m = 5.0;
    d.max_range_m = 80000.0;
    return d;
}

SC_TEST("em radar calibration: the fitted model reproduces its anchor") {
    RadarDef d = GenericSearchRadar();
    Environment env;
    SC_EXPECT(Calibrate(d, env));
    SC_EXPECT_NEAR(AnchorPd(d, env, 40000.0, 3000.0, 2.0), 0.5, 1e-6);
    // Inbound at the anchor altitude: first Pd 0.5 at the anchor range (within the scan step).
    SC_EXPECT_NEAR(DetectionRangeAt(d, env, 3000.0, 2.0, 0.5, 25.0), 40000.0, 300.0);
    // Free-space R ~ sigma^(1/4): sixteen times the RCS doubles the range.
    SC_EXPECT_NEAR(FreeSpaceDetectionRange(d, 32.0, 0.5) / FreeSpaceDetectionRange(d, 2.0, 0.5), 2.0, 1e-9);
    // Noise jamming at J/N = 15 (12 dB) cuts free-space range by (1 + J/N)^(1/4) = 2.
    SC_EXPECT_NEAR(FreeSpaceDetectionRange(d, 2.0, 0.5) / FreeSpaceDetectionRange(d, 2.0, 0.5, 15.0), 2.0, 1e-9);
}

SC_TEST("em radar calibration: low fliers are seen later than the free-space range") {
    RadarDef d = GenericSearchRadar();
    Environment env;
    Calibrate(d, env);
    const double high = DetectionRangeAt(d, env, 3000.0, 2.0, 0.5);
    const double low = DetectionRangeAt(d, env, 50.0, 2.0, 0.5);
    const double horizon = RadarHorizonRange(5.0, 50.0);
    SC_EXPECT(low < high);
    SC_EXPECT(low < horizon);  // multipath near the horizon, not just geometry, sets it
    SC_EXPECT(low > 0.3 * horizon);
}

// --------------------------------------------------------------------------- geolocation

SC_TEST("em geolocation: exact bearings fix the emitter exactly") {
    const Vec3 truth{20000.0, 35000.0, 0.0};
    std::vector<BearingLine> lines;
    for (const Vec3& o : {Vec3{0, 0, 0}, Vec3{30000, 0, 0}, Vec3{-10000, 10000, 0}})
        lines.push_back({o, AzimuthFromTo(o, truth), DegToRad(1.0), 0.0});
    const FixResult f = BearingFix(lines);
    SC_EXPECT(f.valid);
    SC_EXPECT_NEAR(f.position.x, truth.x, 1.0);
    SC_EXPECT_NEAR(f.position.y, truth.y, 1.0);
    SC_EXPECT(f.chi2 < 1e-6);
}

SC_TEST("em geolocation: nearly parallel bearings give no fix") {
    std::vector<BearingLine> lines = {{{0, 0, 0}, DegToRad(45.0), 0.02, 0.0}, {{100, 0, 0}, DegToRad(45.5), 0.02, 0.0}};
    SC_EXPECT(!BearingFix(lines).valid);
}

SC_TEST("em geolocation: more stations, smaller and honest error ellipses (Monte Carlo)") {
    const Vec3 truth{15000.0, 40000.0, 0.0};
    const std::vector<Vec3> sites = {{0, 0, 0}, {30000, 0, 0}, {-20000, 10000, 0}, {45000, 15000, 0}};
    Random rng(7);
    const double sigma = DegToRad(2.0);
    double rms[5] = {0, 0, 0, 0, 0};
    for (std::size_t n = 2; n <= 4; ++n) {
        double err2 = 0.0, nees = 0.0;
        const int trials = 400;
        for (int t = 0; t < trials; ++t) {
            std::vector<BearingLine> lines;
            for (std::size_t i = 0; i < n; ++i)
                lines.push_back({sites[i],
                                 AzimuthFromTo(sites[i], truth) +
                                     sigma * rng.Normal(Stream::EsmBearing, static_cast<std::uint64_t>(t), i, n),
                                 sigma, 0.0});
            const FixResult f = BearingFix(lines);
            SC_EXPECT(f.valid);
            const double ex = f.position.x - truth.x, ey = f.position.y - truth.y;
            err2 += ex * ex + ey * ey;
            Mat2 inv;
            Inverse(f.covariance, inv);
            nees += inv(0, 0) * ex * ex + 2.0 * inv(0, 1) * ex * ey + inv(1, 1) * ey * ey;
        }
        rms[n] = std::sqrt(err2 / trials);
        // Normalised error squared averages ~2 (2 dof) when the covariance is honest.
        SC_EXPECT(nees / trials > 1.6 && nees / trials < 2.6);
    }
    SC_EXPECT(rms[3] < 0.8 * rms[2]);
    SC_EXPECT(rms[4] < rms[3]);
}

SC_TEST("em geolocation: one moving receiver ranges a stationary emitter over time") {
    const Vec3 emitter{0.0, 60000.0, 0.0};
    std::vector<BearingLine> lines;
    for (int i = 0; i <= 10; ++i) {
        const Vec3 rx{-30000.0 + 6000.0 * i, 0.0, 6000.0};  // 60 km flown across the line of sight
        lines.push_back({rx, AzimuthFromTo(rx, emitter), DegToRad(3.0), static_cast<double>(i) * 30.0});
    }
    const FixResult f = BearingFix(lines, 300.0, 0.0);
    SC_EXPECT(f.valid);
    SC_EXPECT_NEAR(f.position.y, 60000.0, 50.0);
}

SC_TEST("em geolocation: TDOA multilateration (Ramona/Tamara style)") {
    const Vec3 truth{40000.0, 120000.0, 8000.0};
    const std::vector<Vec3> sites = {{0, 0, 30}, {-25000, 5000, 30}, {25000, 3000, 30}, {0, 30000, 30}};
    std::vector<TdoaSite> exact;
    for (const Vec3& p : sites) exact.push_back({p, (truth - p).Length() / kSpeedOfLight});
    const TdoaResult r = SolveTdoa(exact, 30e-9, Vec3{0.0, 60000.0, 5000.0});
    SC_EXPECT(r.valid);
    SC_EXPECT_NEAR(r.position.x, truth.x, 1.0);
    SC_EXPECT_NEAR(r.position.y, truth.y, 1.0);
    SC_EXPECT_NEAR(r.position.z, truth.z, 5.0);
    // With 30 ns timing noise the horizontal error stays in the hundreds of metres at 120 km.
    Random rng(3);
    std::vector<TdoaSite> noisy = exact;
    for (std::size_t i = 0; i < noisy.size(); ++i) noisy[i].arrival_time_s += 30e-9 * rng.Normal(Stream::TdoaNoise, i);
    const TdoaResult rn = SolveTdoa(noisy, 30e-9, Vec3{0.0, 60000.0, 5000.0});
    SC_EXPECT(rn.valid);
    SC_EXPECT(GroundRange(rn.position, truth) < 2000.0);
    SC_EXPECT(CepM(Horizontal(rn.covariance)) < 2000.0);
    // 2-D with a known altitude needs only three sites.
    std::vector<TdoaSite> three(exact.begin(), exact.begin() + 3);
    Vec3 guess{0.0, 60000.0, truth.z};
    const TdoaResult r2 = SolveTdoa(three, 30e-9, guess, true);
    SC_EXPECT(r2.valid);
    SC_EXPECT_NEAR(GroundRange(r2.position, truth), 0.0, 5.0);
}

SC_TEST("em geolocation: probability ellipse scales") {
    SC_EXPECT_NEAR(EllipseScaleForProbability(0.5), 1.1774, 1e-4);
    SC_EXPECT_NEAR(EllipseScaleForProbability(0.95), 2.4477, 1e-4);
    Mat2 c;
    c(0, 0) = 100.0 * 100.0;
    c(1, 1) = 400.0 * 400.0;
    const EllipseDesc e = EllipseOf(c);
    SC_EXPECT_NEAR(e.semi_major_m, 400.0, 1e-9);
    SC_EXPECT_NEAR(e.semi_minor_m, 100.0, 1e-9);
    SC_EXPECT_NEAR(e.major_azimuth_rad, 0.0, 1e-9);  // along north
}

// --------------------------------------------------------------------------- tracking

static RadarPlot PlotOf(const Vec3& sensor, const Vec3& target, double t, double sr, double sa, Random& rng, int key) {
    RadarPlot p;
    p.time_s = t;
    p.sensor_position = sensor;
    const Vec3 rel = target - sensor;
    p.sigma_range_m = sr;
    p.sigma_azimuth_rad = sa;
    p.sigma_elevation_rad = sa;
    p.range_m = rel.Length() + sr * rng.Normal(Stream::RangeNoise, static_cast<std::uint64_t>(key));
    p.azimuth_rad = AzimuthOf(rel) + sa * rng.Normal(Stream::AzimuthNoise, static_cast<std::uint64_t>(key));
    p.elevation_rad = ElevationOf(rel) + sa * rng.Normal(Stream::ElevationNoise, static_cast<std::uint64_t>(key));
    return p;
}

SC_TEST("em tracking: plots become a confirmed track that estimates velocity") {
    Tracker trk;
    Random rng(11);
    const Vec3 sensor{0, 0, 5};
    Vec3 pos{0, 40000, 3000};
    const Vec3 vel{0, -250, 0};
    int confirmed_at = -1;
    for (int k = 0; k < 15; ++k) {
        const double t = 3.0 * k;
        std::vector<RadarPlot> plots = {PlotOf(sensor, pos + vel * t, t, 40.0, DegToRad(0.3), rng, k)};
        trk.ProcessLook(t, plots, [](const SensorTrack&) { return true; });
        if (confirmed_at < 0 && !trk.Tracks().empty() && trk.Tracks()[0].status == TrackStatus::Confirmed) confirmed_at = k;
    }
    SC_EXPECT(confirmed_at == 1);  // two of three looks
    SC_EXPECT(trk.Tracks().size() == 1);
    const SensorTrack& t = trk.Tracks()[0];
    SC_EXPECT_NEAR(t.Velocity().y, -250.0, 25.0);
    SC_EXPECT((t.Position() - (pos + vel * 42.0)).Length() < 4.0 * t.PositionSigmaM() + 50.0);
}

SC_TEST("em tracking: a target that disappears coasts with growing uncertainty, then is dropped") {
    Tracker trk;
    Random rng(5);
    const Vec3 sensor{0, 0, 5};
    for (int k = 0; k < 6; ++k) {
        const double t = 2.0 * k;
        std::vector<RadarPlot> plots = {PlotOf(sensor, Vec3{10000, 20000, 1000} + Vec3{200, 0, 0} * t, t, 30.0, 0.005, rng, k)};
        trk.ProcessLook(t, plots, [](const SensorTrack&) { return true; });
    }
    SC_EXPECT(trk.Tracks().size() == 1 && trk.Tracks()[0].status == TrackStatus::Confirmed);
    double last_sigma = trk.Tracks()[0].PositionSigmaM();
    bool grew = true;
    int looks = 0;
    for (int k = 6; k < 20 && !trk.Tracks().empty(); ++k) {
        trk.ProcessLook(2.0 * k, {}, [](const SensorTrack&) { return true; });
        ++looks;
        if (!trk.Tracks().empty()) {
            grew = grew && trk.Tracks()[0].PositionSigmaM() > last_sigma;
            last_sigma = trk.Tracks()[0].PositionSigmaM();
        }
    }
    SC_EXPECT(grew);
    SC_EXPECT(trk.Tracks().empty());
    SC_EXPECT(looks == trk.Params().drop_confirmed_misses);
}

SC_TEST("em tracking: misses only count where the sensor looked") {
    Tracker trk;
    Random rng(9);
    const Vec3 sensor{0, 0, 5};
    for (int k = 0; k < 4; ++k)
        trk.ProcessLook(2.0 * k, {PlotOf(sensor, Vec3{0, 30000, 2000}, 2.0 * k, 30.0, 0.005, rng, k)},
                        [](const SensorTrack&) { return true; });
    for (int k = 4; k < 40; ++k) trk.ProcessLook(2.0 * k, {}, [](const SensorTrack&) { return false; });
    // Never looked at again: still held (stationary target, small process noise growth over 72 s).
    SC_EXPECT(trk.Tracks().size() == 1);
}

SC_TEST("em tracking: two separated targets make two tracks") {
    Tracker trk;
    Random rng(2);
    const Vec3 sensor{0, 0, 5};
    for (int k = 0; k < 5; ++k) {
        const double t = 2.0 * k;
        trk.ProcessLook(t, {PlotOf(sensor, Vec3{0, 30000, 2000}, t, 30.0, 0.004, rng, 2 * k),
                            PlotOf(sensor, Vec3{8000, 30000, 2000}, t, 30.0, 0.004, rng, 2 * k + 1)},
                        [](const SensorTrack&) { return true; });
    }
    int confirmed = 0;
    for (const SensorTrack& t : trk.Tracks()) confirmed += t.status == TrackStatus::Confirmed ? 1 : 0;
    SC_EXPECT(confirmed == 2);
}

// --------------------------------------------------------------------------- knowledge

static KnowledgeTrack Report(int node, int sensor, int item, const Vec3& p, double sigma, double t) {
    KnowledgeTrack k;
    k.origin = {node, sensor, item};
    k.category = Category::Air;
    k.position_valid = true;
    k.position = p;
    k.cov = Mat3::Diagonal(sigma * sigma);
    k.time_s = t;
    return k;
}

SC_TEST("em knowledge: covariance intersection never double-counts a report") {
    const Vec3 x{1000, 2000, 300};
    const Mat3 p = Mat3::Diagonal(200.0 * 200.0);
    Vec3 xf;
    Mat3 pf;
    SC_EXPECT(CovarianceIntersect(x, p, x, p, xf, pf));
    // The same estimate arriving twice (two routes through the network) must not shrink.
    SC_EXPECT_NEAR(pf(0, 0), p(0, 0), 1e-6);
    SC_EXPECT_NEAR(xf.x, x.x, 1e-9);
}

SC_TEST("em knowledge: two sources on one object fuse; two objects stay two") {
    KnowledgeBase kb;
    kb.Upsert(Report(1, 10, 1, {10000, 20000, 1000}, 300.0, 0.0));
    kb.Upsert(Report(2, 20, 7, {10200, 20100, 1000}, 500.0, 0.0));  // same aircraft, other sensor
    kb.Upsert(Report(2, 20, 8, {30000, 5000, 1000}, 500.0, 0.0));   // another aircraft
    const std::vector<FusedTrack> pic = kb.Picture(0.0);
    SC_EXPECT(pic.size() == 2);
    int fused = 0;
    for (const FusedTrack& f : pic) fused += f.contributors == 2 ? 1 : 0;
    SC_EXPECT(fused == 1);
}

SC_TEST("em knowledge: unrefreshed knowledge grows uncertain, then is forgotten") {
    KnowledgeParams prm;
    prm.max_age_s = 60.0;
    KnowledgeBase kb(prm);
    KnowledgeTrack k = Report(1, 10, 1, {0, 30000, 2000}, 100.0, 0.0);
    k.velocity_valid = true;
    k.velocity = {0, -200, 0};
    k.velocity_sigma_mps = 10.0;
    kb.Upsert(k);
    const double s10 = kb.Picture(10.0)[0].fused.PositionSigmaM();
    const double s30 = kb.Picture(30.0)[0].fused.PositionSigmaM();
    SC_EXPECT(s30 > s10 && s10 > 100.0);
    SC_EXPECT_NEAR(kb.Picture(30.0)[0].fused.position.y, 30000.0 - 6000.0, 1e-6);
    kb.Age(61.0);
    SC_EXPECT(kb.Size() == 0);
}

SC_TEST("em knowledge: bearing-only reports from two sites become an emitter fix") {
    KnowledgeBase kb;
    const Vec3 emitter{5000, 40000, 0};
    EmitterSignature sig;
    sig.freq_hz = 9.5e9;
    sig.prf_hz = 2000.0;
    for (int site = 0; site < 2; ++site) {
        const Vec3 o{site == 0 ? -20000.0 : 25000.0, 0.0, 0.0};
        KnowledgeTrack k;
        k.origin = {1, 100 + site, 1};
        k.category = Category::Emitter;
        k.has_emitter = true;
        k.emitter = sig;
        k.bearings.push_back({o, AzimuthFromTo(o, emitter), DegToRad(1.5), 0.0});
        k.time_s = 0.0;
        kb.Upsert(k);
    }
    const std::vector<FusedTrack> pic = kb.Picture(0.0);
    SC_EXPECT(pic.size() == 1);
    SC_EXPECT(pic[0].from_fix);
    SC_EXPECT(GroundRange(pic[0].fused.position, emitter) < 10.0);
    // One site alone: direction only.
    KnowledgeBase one;
    KnowledgeTrack k;
    k.origin = {1, 100, 1};
    k.category = Category::Emitter;
    k.has_emitter = true;
    k.emitter = sig;
    k.bearings.push_back({Vec3{}, AzimuthFromTo(Vec3{}, emitter), DegToRad(1.5), 0.0});
    one.Upsert(k);
    SC_EXPECT(one.Picture(0.0)[0].bearing_only);
}
