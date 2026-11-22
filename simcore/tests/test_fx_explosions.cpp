// How loud and how big an explosion is (simcore/fx/explosions.hpp, docs/fx/README.md 3.3): Kinney and Graham's fit
// worked by hand, its far-field limit, Sachs scaling aloft, the fireball's W^(1/3) sizes for the charges the game shows,
// and the look's scaling from its reference.
#include <cmath>

#include "simcore/fx/explosions.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::fx;

SC_TEST("fx: kinney-graham overpressure worked by hand") {
    // Z = 10: 808 (1 + (10/4.5)^2) = 808 x 5.93827 = 4798.1; sqrt(1 + 208.33^2) sqrt(1 + 31.25^2) sqrt(1 + 7.4074^2)
    // = 208.346 x 31.266 x 7.4746 = 48690; 4798.1 / 48690 = 0.09855.
    SC_EXPECT_NEAR(KinneyGrahamRatio(10.0), 0.09855, 0.09855 * 0.01);
    // Z = 1: 808 x 1.04938 = 847.90 over 20.857 x 3.2811 x 1.2445 = 85.164: 9.956.
    SC_EXPECT_NEAR(KinneyGrahamRatio(1.0), 9.956, 9.956 * 0.01);
    SC_EXPECT_NEAR(KinneyGrahamRatio(0.0), 808.0, 1e-9);
    SC_EXPECT_NEAR(ScaledDistance(100.0, 1000.0), 10.0, 1e-9);
    // 1000 kg at 100 m at sea level: 0.09855 x 101325 Pa.
    SC_EXPECT_NEAR(PeakOverpressurePa(1000.0, 100.0), 0.09855 * 101325.0, 0.09855 * 101325.0 * 0.01);
    // It falls with range and grows with charge.
    SC_EXPECT(PeakOverpressurePa(100.0, 50.0) > PeakOverpressurePa(100.0, 60.0));
    SC_EXPECT(PeakOverpressurePa(120.0, 50.0) > PeakOverpressurePa(100.0, 50.0));
    SC_EXPECT_NEAR(PeakOverpressurePa(0.0, 50.0), 0.0, 1e-12);
    // A ground burst is the free-air burst of 1.8 times the charge.
    SC_EXPECT_NEAR(PeakOverpressurePa(100.0, 200.0, isa::kSeaLevelPressure, kSurfaceBurstFactor),
                   PeakOverpressurePa(180.0, 200.0), 1e-9);
    SC_EXPECT_NEAR(PeakSplDb(100.0, 200.0, isa::kSeaLevelPressure, kSurfaceBurstFactor) - PeakSplDb(100.0, 200.0),
                   20.0 * std::log10(PeakOverpressurePa(180.0, 200.0) / PeakOverpressurePa(100.0, 200.0)), 1e-9);
}

SC_TEST("fx: blast level meets its far-field limit") {
    // dp/p0 -> 0.8274 / Z: L_pk = 20 log10(0.8274 x 101325 / 2e-5) + (20/3) log10 W - 20 log10 R = 192.45 + 6.67 log10 W
    // - 20 log10 R.
    SC_EXPECT_NEAR(kKinneyGrahamFar, 0.827392, 1e-6);
    for (double w : {0.02, 15.0, 95.0, 200.0, 450.0}) {
        for (double r : {300.0, 1000.0, 10000.0}) {
            SC_EXPECT_NEAR(FarFieldPeakSplDb(w, r), 192.4 + 6.67 * std::log10(w) - 20.0 * std::log10(r), 0.1);
        }
        // The fit runs above its limit, by 1.52 dB at Z = 10, 1.39 dB at 10.5, 0.41 dB at 20.
        for (double z : {10.0, 10.5, 12.0, 20.0, 50.0, 200.0}) {
            const double r = z * std::cbrt(w);
            const double over = PeakSplDb(w, r) - FarFieldPeakSplDb(w, r);
            SC_EXPECT(over >= 0.0 && over <= 1.55);
            if (z >= 10.5) SC_EXPECT(over <= 1.4);
            if (z >= 20.0) SC_EXPECT(over <= 0.42);
        }
    }
    // The research table's rows: a 23 mm shell (0.02 kg) at 1 km, 121 dB; a Mk 84 (450 kg) at 10 km, 130 dB.
    SC_EXPECT_NEAR(FarFieldPeakSplDb(0.02, 1000.0), 121.1, 0.1);
    SC_EXPECT_NEAR(FarFieldPeakSplDb(450.0, 10000.0), 130.1, 0.1);
}

SC_TEST("fx: a blast aloft scales by sachs") {
    // In about half the pressure (50 kPa) the scaled distance shrinks by about cbrt(2) (times cbrt(50000 / 101325) =
    // 0.7902) and the ratio multiplies the lower p0: far out the overpressure goes as p0^(2/3) (0.6247 at 50 kPa), close
    // in it is weaker in pascals and stronger as a share.
    const double p_half = 50000.0;
    const double far_ratio = PeakOverpressurePa(1.0, 200.0, p_half) / PeakOverpressurePa(1.0, 200.0);
    SC_EXPECT_NEAR(far_ratio, std::pow(p_half / isa::kSeaLevelPressure, 2.0 / 3.0), 0.005);
    const double z = ScaledDistance(30.0, 15.0) * 0.790226;
    SC_EXPECT(z < ScaledDistance(30.0, 15.0));  // aloft the same range is a smaller scaled distance: nearer, as it were
    SC_EXPECT_NEAR(PeakOverpressurePa(15.0, 30.0, p_half), KinneyGrahamRatio(z) * p_half, 1e-5 * PeakOverpressurePa(15.0, 30.0, p_half));
    SC_EXPECT(PeakOverpressurePa(15.0, 30.0, p_half) / p_half > PeakOverpressurePa(15.0, 30.0) / isa::kSeaLevelPressure);
    SC_EXPECT(PeakOverpressurePa(15.0, 30.0, p_half) < PeakOverpressurePa(15.0, 30.0));
}

SC_TEST("fx: fireballs scale by the cube root of the charge") {
    // 1.1 m/kg^(1/3): 23 mm shell 0.02 kg, a SAM warhead 15 kg, Mk 82 95 kg, FAB-500 200 kg, Mk 84 450 kg.
    SC_EXPECT_NEAR(FireballRadiusM(0.02), 0.2986, 0.001);
    SC_EXPECT_NEAR(FireballRadiusM(15.0), 2.713, 0.005);
    SC_EXPECT_NEAR(FireballRadiusM(95.0), 5.019, 0.005);
    SC_EXPECT_NEAR(FireballRadiusM(200.0), 6.433, 0.005);
    SC_EXPECT_NEAR(FireballRadiusM(450.0), 8.429, 0.005);
    // The measured spread at 100 kg (cbrt 100 = 4.642): 3.30 to 6.87 m.
    SC_EXPECT_NEAR(FireballRadiusM(100.0, kFireballScaledRadiusLow), 3.296, 0.005);
    SC_EXPECT_NEAR(FireballRadiusM(100.0, kFireballScaledRadiusHigh), 6.870, 0.005);
    // 0.6 s at 100 kg; eight times the charge, twice as long.
    SC_EXPECT_NEAR(FireballDurationS(100.0), 0.6, 1e-12);
    SC_EXPECT_NEAR(FireballDurationS(800.0), 1.2, 1e-9);
    SC_EXPECT_NEAR(FireballDurationS(0.02), 0.0351, 0.0005);
    SC_EXPECT_NEAR(FireballDurationS(450.0), 0.991, 0.002);
}

SC_TEST("fx: an explosion's look scales from its reference") {
    // The reference, 100 kg on the ground.
    const ExplosionLook ref = LookFor(100.0, true, false);
    SC_EXPECT(ref.ground);
    SC_EXPECT_NEAR(ref.fireball_m, 1.1 * std::cbrt(100.0), 1e-9);
    SC_EXPECT_NEAR(ref.fireball_s, 0.6, 1e-12);
    SC_EXPECT_NEAR(ref.smoke_m, 16.0, 1e-9);
    SC_EXPECT_NEAR(ref.smoke_s, 45.0, 1e-9);
    SC_EXPECT_NEAR(ref.dust_m, 20.0, 1e-9);
    SC_EXPECT_NEAR(ref.crater_m, 3.5, 1e-9);
    SC_EXPECT_NEAR(ref.flash_m, 18.0, 1e-9);
    SC_EXPECT(ref.debris == 40);
    SC_EXPECT_NEAR(ref.light_lumens, 1.0e8, 1.0);
    // Eight times the charge: every size twice, the light four times.
    const ExplosionLook big = LookFor(800.0, true, false);
    SC_EXPECT_NEAR(big.fireball_m, 2.0 * ref.fireball_m, 1e-9);
    SC_EXPECT_NEAR(big.smoke_m, 2.0 * ref.smoke_m, 1e-9);
    SC_EXPECT_NEAR(big.smoke_s, 2.0 * ref.smoke_s, 1e-9);
    SC_EXPECT_NEAR(big.dust_m, 2.0 * ref.dust_m, 1e-9);
    SC_EXPECT_NEAR(big.crater_m, 2.0 * ref.crater_m, 1e-9);
    SC_EXPECT_NEAR(big.flash_m, 2.0 * ref.flash_m, 1e-9);
    SC_EXPECT(big.debris == 80);
    SC_EXPECT_NEAR(big.light_lumens, 4.0e8, 4.0);
    // A SAM's warhead high in the air: no dust, no crater, fragments only, a smaller puff.
    const ExplosionLook sam = LookFor(15.0, false, true);
    SC_EXPECT(!sam.ground);
    SC_EXPECT_NEAR(sam.fireball_m, 2.713, 0.005);
    SC_EXPECT_NEAR(sam.dust_m, 0.0, 1e-12);
    SC_EXPECT_NEAR(sam.crater_m, 0.0, 1e-12);
    SC_EXPECT_NEAR(sam.smoke_m, 16.0 * 0.8 * std::cbrt(0.15), 1e-9);
    SC_EXPECT(sam.debris == 11);  // 40 x 0.5 x cbrt(0.15) = 10.6
    // A proximity-fuzed bomb a few metres up: dust but no crater.
    const ExplosionLook vt = LookFor(95.0, true, true);
    SC_EXPECT(vt.ground && vt.crater_m == 0.0 && vt.dust_m > 0.0);
    SC_EXPECT_NEAR(vt.dust_m, 20.0 * 0.6 * std::cbrt(0.95), 1e-9);
    // A 23 mm shell's burst is small but not nothing; a Mk 84's crater is several metres across.
    const ExplosionLook shell = LookFor(0.02, false, false);
    SC_EXPECT(shell.fireball_m > 0.25 && shell.fireball_m < 0.35);
    SC_EXPECT(shell.debris >= 1 && shell.smoke_m > 0.5 && shell.smoke_m < 1.5);
    const ExplosionLook mk84 = LookFor(450.0, true, false);
    SC_EXPECT(mk84.crater_m > 5.0 && mk84.crater_m < 7.0);
    SC_EXPECT(mk84.smoke_m > 25.0 && mk84.dust_m > 30.0);
}
