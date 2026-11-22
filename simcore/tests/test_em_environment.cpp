// Roadmap E9's last parts (docs/EM_ARCHITECTURE.md section 37): the clutter an antenna stands in (ITU-R P.2108-1
// section 3.1), water under a path (ITU-R P.527-6's pure water, Fresnel's reflection, P.1546-6's mixed path), checked
// against hand calculations and the recommendations' own numbers. Every terrain and link here is a generic placeholder,
// not any real system's data.
#include <cmath>
#include <vector>

#include "simcore/em.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::em;

namespace {

// Flat ground at 0 m with water between two eastings (a lake across the path, or none).
class LakeTerrain final : public Terrain {
public:
    LakeTerrain(double from_east_m, double to_east_m) : from_(from_east_m), to_(to_east_m) {}
    double HeightAt(double, double) const override { return 0.0; }
    bool WaterAt(double e, double) const override { return e >= from_ && e <= to_; }
    bool HasWater() const override { return to_ > from_; }

private:
    double from_, to_;
};

// Flat ground with a wood of `height_m` within `radius_m` of the origin.
class WoodAtOrigin final : public Terrain {
public:
    WoodAtOrigin(double height_m, double radius_m) : height_(height_m), radius_(radius_m) {}
    double HeightAt(double, double) const override { return 0.0; }
    double ObstacleHeightAt(double e, double n) const override { return std::hypot(e, n) < radius_ ? height_ : 0.0; }
    double ObstacleTallestM() const override { return height_; }

private:
    double height_, radius_;
};

}  // namespace

// --------------------------------------------------------------------------- the clutter an antenna stands in

SC_TEST("em environment: P.2108 section 3.1's terminal clutter loss against hand calculations") {
    // Dense urban, R = 20 m, a 5 m antenna at 50 MHz: h_dif = 15 m, theta = atan(15 / 27) = 29.0546 degrees,
    // K_nu = 0.342 sqrt(0.05) = 0.076474, nu = K_nu sqrt(15 x 29.0546) = 1.5965, J(nu) = 17.261 dB (eq. 1),
    // A_h = J - 6.03 = 11.231 dB (eq. 2a).
    SC_EXPECT_NEAR(TerminalClutterLossDb(5.0, 20.0, 50e6), 11.2306, 1e-3);
    // A 3 m whip in a 15 m wood at 50 MHz: h_dif 12, theta 23.962 degrees, nu 1.2968: 9.677 dB.
    SC_EXPECT_NEAR(TerminalClutterLossDb(3.0, 15.0, 50e6), 9.6767, 1e-3);
    // The same wood round a 5 m antenna at 1 GHz: h_dif 10, theta 20.323 degrees, nu = 0.342 sqrt(203.23) = 4.8755: 20.565 dB.
    SC_EXPECT_NEAR(TerminalClutterLossDb(5.0, 15.0, 1e9), 20.5647, 1e-3);
    // Section 3.1 holds to 3 GHz: a 6 GHz radar takes the 3 GHz loss.
    SC_EXPECT_NEAR(TerminalClutterLossDb(5.0, 15.0, 3e9), 25.3498, 1e-3);
    SC_EXPECT_NEAR(TerminalClutterLossDb(5.0, 15.0, 6e9), TerminalClutterLossDb(5.0, 15.0, 3e9), 1e-12);
    // Just under the canopy little is lost; at or over it, or with no clutter, nothing.
    SC_EXPECT_NEAR(TerminalClutterLossDb(14.0, 15.0, 50e6), 0.9688, 1e-3);
    SC_EXPECT_NEAR(TerminalClutterLossDb(15.0, 15.0, 50e6), 0.0, 1e-12);
    SC_EXPECT_NEAR(TerminalClutterLossDb(5.0, 0.0, 50e6), 0.0, 1e-12);
    // It grows the deeper the antenna stands and the higher the frequency.
    SC_EXPECT(TerminalClutterLossDb(2.0, 15.0, 50e6) > TerminalClutterLossDb(8.0, 15.0, 50e6));
    SC_EXPECT(TerminalClutterLossDb(3.0, 15.0, 300e6) > TerminalClutterLossDb(3.0, 15.0, 50e6));
}

SC_TEST("em environment: an antenna in a wood loses its end's clutter, and its path starts from the canopy") {
    const WoodAtOrigin wood(15.0, 50.0);
    const FlatTerrain open(0.0);
    const Vec3 a{0.0, 0.0, 3.0}, b{20000.0, 0.0, 3.0};
    const PathGeometry g = AnalysePath(wood, a, 3.0, b, 3.0, 100.0);
    SC_EXPECT_NEAR(g.clutter_a_m, 15.0, 1e-12);
    SC_EXPECT_NEAR(g.antenna_a_m, 3.0, 1e-12);
    SC_EXPECT_NEAR(g.clutter_b_m, 0.0, 1e-12);  // the far end on open ground
    // The ray from the canopy's top: the profile's own ground (and the wood within the skip) never blocks it.
    SC_EXPECT(g.line_of_sight);
    // Asked the other way round, the clutter is the second end's.
    const PathGeometry back = AnalysePath(wood, b, 3.0, a, 3.0, 100.0);
    SC_EXPECT_NEAR(back.clutter_b_m, 15.0, 1e-12);
    SC_EXPECT_NEAR(TerminalClutterLossDb(back, 50e6), TerminalClutterLossDb(g, 50e6), 1e-12);
    // A VHF link: the plane earth with the whips' own heights is the larger loss (Bullington's max, LinkLossCore), as on
    // open ground, so the wood costs exactly its end's loss: 9.677 dB at 50 MHz.
    Environment env;
    const PathGeometry bare = AnalysePath(open, a, 3.0, b, 3.0, 100.0);
    const LinkLossResult in_wood = LinkLossCore(env, g, 3.0, 3.0, 3.0, 3.0, 20000.0, 50e6);
    const LinkLossResult in_field = LinkLossCore(env, bare, 3.0, 3.0, 3.0, 3.0, 20000.0, 50e6);
    SC_EXPECT_NEAR(in_wood.clutter_db, 9.6767, 1e-3);
    SC_EXPECT_NEAR(in_field.clutter_db, 0.0, 1e-12);
    SC_EXPECT_NEAR(in_wood.TotalOneWayDb() - in_field.TotalOneWayDb(), 9.6767, 1e-3);
    // A 5 m radar mast in the wood at 6 GHz: the 3 GHz loss, 25.35 dB one way.
    const PathGeometry radar = AnalysePath(wood, Vec3{0.0, 0.0, 5.0}, 5.0, Vec3{20000.0, 0.0, 300.0}, 300.0, 100.0);
    SC_EXPECT_NEAR(LinkLossCore(env, radar, 5.0, 300.0, 5.0, 300.0, 20000.0, 6e9).clutter_db, 25.3498, 1e-3);
    // A mast over the canopy, or an aircraft over the wood, stands in no clutter.
    SC_EXPECT_NEAR(AnalysePath(wood, Vec3{0.0, 0.0, 18.0}, 18.0, b, 3.0, 100.0).clutter_a_m, 0.0, 1e-12);
    SC_EXPECT_NEAR(AnalysePath(wood, b, 3.0, Vec3{0.0, 0.0, 60.0}, 60.0, 100.0).clutter_b_m, 0.0, 1e-12);
}

// --------------------------------------------------------------------------- water

SC_TEST("em environment: P.527 pure water's permittivity, its conductivity and its Fresnel reflection") {
    // Eqs. (5)-(13) at 20 C: the static permittivity eps_s = 77.66 + 103.3 x 0.023367 = 80.074 (1 MHz), and at 10 GHz
    // 60.79 - j 32.72 (the recommendation's Fig. 2 reads about 61 and 33 there).
    const WaterPermittivity still = PureWaterPermittivity(1e6, 20.0);
    SC_EXPECT_NEAR(still.real, 80.0738, 1e-3);
    SC_EXPECT(still.imag < 0.01);
    const WaterPermittivity x_band = PureWaterPermittivity(10e9, 20.0);
    SC_EXPECT_NEAR(x_band.real, 60.7886, 1e-3);
    SC_EXPECT_NEAR(x_band.imag, 32.7208, 1e-3);
    // Eq. (3a): sigma = 0.05563 eps'' f_GHz; at 1 GHz and 20 C eps'' = 4.394: 0.2445 S/m.
    SC_EXPECT_NEAR(PureWaterPermittivity(1e9, 20.0).ConductivitySm(1e9), 0.24446, 1e-4);
    // Fresnel, horizontal polarisation: at normal incidence |(1 - sqrt(eps)) / (1 + sqrt(eps))| = 0.7979 for
    // 69.37 - j 30.37 (6 GHz, 10 C); near grazing almost 1 (0.9988 at 0.3 degrees, 0.9961 at 1 degree).
    const WaterPermittivity c_band = PureWaterPermittivity(6e9);
    SC_EXPECT_NEAR(c_band.real, 69.3682, 1e-3);
    SC_EXPECT_NEAR(FresnelReflectionH(c_band, 0.5 * kPi), 0.79789, 1e-4);
    SC_EXPECT_NEAR(FresnelReflectionH(c_band, DegToRad(0.3)), 0.99882, 1e-4);
    SC_EXPECT_NEAR(FresnelReflectionH(c_band, DegToRad(1.0)), 0.99606, 1e-4);
}

SC_TEST("em environment: P.1546 section 8's mixed-path interpolation against hand calculations") {
    // Land 60 dB, water 40 dB, half over water: A0 = 1 - 0.5^(2/3) = 0.37004 (eq. 19), Delta = 20, V = 1.5 (eq. 20),
    // A = 0.37004^1.5 = 0.22510 (eq. 18): L = 60 - 0.2251 x 20 = 55.498 dB (eq. 17).
    SC_EXPECT_NEAR(MixedPathLossDb(60.0, 40.0, 0.5), 55.4980, 1e-3);
    // Water costing more (Delta < 0): V = 1, A = A0: 40 + 0.37004 x 20 = 47.401 dB.
    SC_EXPECT_NEAR(MixedPathLossDb(40.0, 60.0, 0.5), 47.4008, 1e-3);
    SC_EXPECT_NEAR(MixedPathLossDb(60.0, 40.0, 0.0), 60.0, 1e-12);
    SC_EXPECT_NEAR(MixedPathLossDb(60.0, 40.0, 1.0), 40.0, 1e-12);
    // The fraction's effect is not linear: a quarter over water moves it 1.46 dB of 20 (A = 0.0729).
    SC_EXPECT_NEAR(MixedPathLossDb(60.0, 40.0, 0.25), 58.5419, 1e-3);
}

SC_TEST("em environment: water under a path is found stretch by stretch") {
    const Vec3 a{0.0, 0.0, 5.0}, b{10000.0, 0.0, 50.0};
    const PathGeometry half = AnalysePath(LakeTerrain(5000.0, 20000.0), a, 5.0, b, 50.0, 100.0);
    // The far half of 64 stretches (156.25 m each): bits 32-63.
    SC_EXPECT(half.water_mask == 0xFFFFFFFF00000000ull);
    SC_EXPECT_NEAR(WaterFraction(half), 0.5, 1e-12);
    SC_EXPECT_NEAR(WaterShareBetween(half, 0.0, 5000.0), 0.0, 1e-12);
    SC_EXPECT_NEAR(WaterShareBetween(half, 5000.0, 10000.0), 1.0, 1e-12);
    // 4 to 6 km: the middles of stretches 26-37, six of them water.
    SC_EXPECT_NEAR(WaterShareBetween(half, 4000.0, 6000.0), 0.5, 1e-12);
    // Narrower than a stretch: the one that holds its middle.
    SC_EXPECT_NEAR(WaterShareBetween(half, 5100.0, 5110.0), 1.0, 1e-12);
    SC_EXPECT_NEAR(WaterShareBetween(half, 4900.0, 4910.0), 0.0, 1e-12);
    // No water: a terrain without it is never asked.
    SC_EXPECT(AnalysePath(LakeTerrain(0.0, 0.0), a, 5.0, b, 50.0, 100.0).water_mask == 0);
    SC_EXPECT(AnalysePath(FlatTerrain(0.0), a, 5.0, b, 50.0, 100.0).water_mask == 0);
}

SC_TEST("em environment: a low microwave ray reflects off a lake under its Fresnel zone, not off rough land") {
    // A 5 m radar and a target 50 m up 10 km away at 6 GHz. The reflection lies 985 m from the radar (over the 4/3 earth),
    // grazing 0.29 degrees; its first Fresnel zone reaches sqrt(lambda d1 d2 / d) / tan(psi) = 1.33 km either side, from
    // the radar to 2.3 km out.
    Environment env;
    const double f = 6e9, lambda = WavelengthOf(f);
    const Vec3 radar{0.0, 0.0, 5.0}, target{10000.0, 0.0, 50.0};
    const ReflectionGeometry rg = ComputeReflection(5.0, 50.0, 10000.0, env.k_factor);
    const double land = EffectiveReflection(env, rg.grazing_rad, lambda);
    const double water = WaterReflection(env, rg.grazing_rad, f);
    // Land: 0.9 x exp(-8 (pi 1 m sin psi / lambda)^2) = 0.41: rolling ground scatters half of it. Water: Fresnel's 0.9988
    // and the lake's 5 cm waves: 0.997.
    SC_EXPECT(land < 0.5);
    SC_EXPECT(water > 0.99);
    const PathGeometry over_lake = AnalysePath(LakeTerrain(-1000.0, 20000.0), radar, 5.0, target, 50.0, 100.0);
    const PathGeometry over_land = AnalysePath(FlatTerrain(0.0), radar, 5.0, target, 50.0, 100.0);
    const PathGeometry lake_far = AnalysePath(LakeTerrain(5000.0, 20000.0), radar, 5.0, target, 50.0, 100.0);
    SC_EXPECT_NEAR(PathReflection(env, over_lake, rg, f, false), water, 1e-12);
    SC_EXPECT_NEAR(PathReflection(env, over_land, rg, f, false), land, 1e-12);
    // A lake beyond the zone (5-10 km out, under the target) does not reflect the ray.
    SC_EXPECT_NEAR(PathReflection(env, lake_far, rg, f, false), land, 1e-12);
    const LinkLossResult l_lake = LinkLossCore(env, over_lake, 5.0, 50.0, 5.0, 50.0, 10000.0, f);
    const LinkLossResult l_land = LinkLossCore(env, over_land, 5.0, 50.0, 5.0, 50.0, 10000.0, f);
    SC_EXPECT_NEAR(l_lake.multipath_factor, TwoRayFactor(rg, lambda, water), 1e-12);
    SC_EXPECT_NEAR(l_land.multipath_factor, TwoRayFactor(rg, lambda, land), 1e-12);
    // Over the lake the two rays reach a factor of up to 2 or cancel nearly whole: lobes and nulls along the target's
    // range, as over the sea; over land they stay near 1.
    double lake_min = 9.0, lake_max = 0.0, land_min = 9.0, land_max = 0.0;
    for (double d = 4000.0; d <= 12000.0; d += 10.0) {
        const ReflectionGeometry r = ComputeReflection(5.0, 50.0, d, env.k_factor);
        const double fl = TwoRayFactor(r, lambda, WaterReflection(env, r.grazing_rad, f));
        const double fg = TwoRayFactor(r, lambda, EffectiveReflection(env, r.grazing_rad, lambda));
        lake_min = std::min(lake_min, fl);
        lake_max = std::max(lake_max, fl);
        land_min = std::min(land_min, fg);
        land_max = std::max(land_max, fg);
    }
    SC_EXPECT(lake_max > 1.95 && lake_min < 0.05);
    SC_EXPECT(land_max < 1.5 && land_min > 0.5);
    // The same path asked from the target's end: a lake near the radar is found at the radar's end either way.
    const PathGeometry near_radar = AnalysePath(LakeTerrain(-1000.0, 2500.0), radar, 5.0, target, 50.0, 100.0);
    const PathGeometry near_radar_back = AnalysePath(LakeTerrain(-1000.0, 2500.0), target, 50.0, radar, 5.0, 100.0);
    const double share = ReflectionWaterShare(near_radar, rg, lambda, false);
    SC_EXPECT(share > 0.95);
    SC_EXPECT_NEAR(ReflectionWaterShare(near_radar_back, rg, lambda, true), share, 1e-12);
    SC_EXPECT_NEAR(LinkLossCore(env, near_radar_back, 5.0, 50.0, 5.0, 50.0, 10000.0, f, 1.0, true).multipath_factor,
                   LinkLossCore(env, near_radar, 5.0, 50.0, 5.0, 50.0, 10000.0, f).multipath_factor, 1e-12);
    // Below 1 GHz the plane earth holds over any surface: no change.
    SC_EXPECT_NEAR(LinkLossCore(env, over_lake, 5.0, 50.0, 5.0, 50.0, 10000.0, 50e6).multipath_factor,
                   LinkLossCore(env, over_land, 5.0, 50.0, 5.0, 50.0, 10000.0, 50e6).multipath_factor, 1e-12);
}

SC_TEST("em environment: past the horizon VHF carries further over water (P.526 with P.527's water, P.1546's mixed path)") {
    // Pure water at 50 MHz and 10 C: eps' 83.81, sigma 0.00086 S/m; K (eq. 12a) 0.0441 against the medium land's 0.0192.
    const WaterPermittivity w = PureWaterPermittivity(50e6);
    const double sigma = w.ConductivitySm(50e6);
    SC_EXPECT_NEAR(w.real, 83.8061, 1e-3);
    SC_EXPECT_NEAR(SurfaceAdmittanceK(50.0, 8500.0, true, w.real, sigma), 0.044098, 2e-5);
    SC_EXPECT_NEAR(SurfaceAdmittanceK(50.0, 8500.0), 0.019198, 2e-5);
    // Two 3 m whips 40 km apart (past their 14 km horizon): the height gains' floor 2 + 20 log K is 7.2 dB higher at each
    // end over water: 66.35 dB over land, 51.84 dB over water. A 3 m whip to 8,000 m at 500 km, the whip's floor alone:
    // 73.98 and 66.56.
    SC_EXPECT_NEAR(SphericalEarthLossDb(40e3, 3.0, 3.0, 50e6, 8500e3), 66.3531, 1e-3);
    SC_EXPECT_NEAR(SphericalEarthLossDb(40e3, 3.0, 3.0, 50e6, 8500e3, w.real, sigma), 51.8418, 1e-3);
    SC_EXPECT_NEAR(SphericalEarthLossDb(500e3, 3.0, 8000.0, 50e6, 8500e3, w.real, sigma), 66.5567, 1e-3);
    // A path over flat ground: DiffractionLossDb's spherical term mixed by the share over water (P.1546 eqs. 17-21).
    const Vec3 a{0.0, 0.0, 3.0}, b{40000.0, 0.0, 3.0};
    const PathGeometry land = AnalysePath(FlatTerrain(0.0), a, 3.0, b, 3.0, 100.0);
    const PathGeometry lake = AnalysePath(LakeTerrain(-1.0, 50000.0), a, 3.0, b, 3.0, 100.0);
    const PathGeometry half = AnalysePath(LakeTerrain(20000.0, 50000.0), a, 3.0, b, 3.0, 100.0);
    const double smooth = KnifeEdgeLossDb(land.smooth_v_geometry * std::sqrt(2.0 / WavelengthOf(50e6)));
    const double sph_land = SphericalEarthLossDb(land.ground_range_m, land.smooth_h1_m, land.smooth_h2_m, 50e6, land.earth_radius_m);
    const double sph_water = SphericalEarthLossDb(land.ground_range_m, land.smooth_h1_m, land.smooth_h2_m, 50e6,
                                                  land.earth_radius_m, w.real, sigma);
    const double edges = TerrainEdgesLossDb(land, 50e6);
    SC_EXPECT_NEAR(DiffractionLossDb(land, 50e6), edges + sph_land - smooth, 1e-9);
    SC_EXPECT_NEAR(DiffractionLossDb(lake, 50e6), edges + sph_water - smooth, 1e-9);
    SC_EXPECT_NEAR(DiffractionLossDb(half, 50e6), edges + MixedPathLossDb(sph_land, sph_water, 0.5) - smooth, 1e-9);
    SC_EXPECT(DiffractionLossDb(half, 50e6) < DiffractionLossDb(land, 50e6) - 2.0);
    // Microwaves take no spherical term (kSphericalEarthMaxFreqHz): water leaves their diffraction as it was.
    SC_EXPECT_NEAR(DiffractionLossDb(lake, 6e9), DiffractionLossDb(land, 6e9), 1e-12);
}
