// When jets leave contrails (simcore/fx/contrails.hpp, docs/fx/README.md "Contrails"): the saturation pressures against
// Sonntag's values, the Schmidt-Appleman threshold against Schumann (1996), and the trail's kind in a standard atmosphere.
#include <cmath>

#include "simcore/atmosphere.hpp"
#include "simcore/fx/contrails.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::fx;

SC_TEST("fx: saturation pressures over water and ice") {
    SC_EXPECT_NEAR(SaturationPressureWaterPa(273.15), 611.2, 0.5);
    SC_EXPECT_NEAR(SaturationPressureIcePa(273.15), 611.15, 0.5);
    SC_EXPECT_NEAR(SaturationPressureWaterPa(253.15), 125.6, 0.3);
    SC_EXPECT_NEAR(SaturationPressureIcePa(253.15), 103.2, 0.3);
    // At -50 °C water's saturation pressure is 1.64 times ice's: 61 % over water is saturated over ice.
    SC_EXPECT_NEAR(IceHumidity(0.611, 223.15), 1.0, 0.01);
}

SC_TEST("fx: schmidt-appleman threshold matches schumann") {
    // 250 hPa, eta 0.3: G = 1.67 Pa/K; T_LM -41.8 °C; dry air -51.3 °C (Schumann 1996, fig. 2).
    SC_EXPECT_NEAR(MixingLineSlope(25000.0, 0.3), 1.668, 0.002);
    SC_EXPECT_NEAR(ContrailThresholdK(25000.0, 1.0, 0.3) - 273.15, -41.77, 0.02);
    SC_EXPECT_NEAR(ContrailThresholdK(25000.0, 0.0, 0.3) - 273.15, -51.26, 0.05);
    SC_EXPECT_NEAR(ContrailThresholdK(25000.0, 0.5, 0.3) - 273.15, -49.13, 0.05);
    // A more efficient engine makes trails in warmer air (the "efficient engines make more contrails" result).
    SC_EXPECT(ContrailThresholdK(25000.0, 0.5, 0.35) > ContrailThresholdK(25000.0, 0.5, 0.2) + 1.5);
}

SC_TEST("fx: the contrail threshold solves its equation near saturation too") {
    // Near saturation the equation's own iteration crawls (its slope U e_w'(T) / G tends to 1): at U = 0.99, 250 hPa
    // and eta 0.3 the root is -43.0765 °C (bisected by hand on f(T) = T - T_LM + (e_w(T_LM) - U e_w(T)) / G), where
    // twenty steps of the iteration stopped at -42.948 °C. At every humidity and pressure the answer satisfies
    // T_LC = T_LM - (e_w(T_LM) - U e_w(T_LC)) / G, and it falls as the air dries.
    SC_EXPECT_NEAR(ContrailThresholdK(25000.0, 0.99, 0.3) - 273.15, -43.0765, 0.001);
    SC_EXPECT_NEAR(ContrailThresholdK(25000.0, 0.95, 0.3) - 273.15, -44.5623, 0.001);
    for (const double p : {5000.0, 15000.0, 25000.0, 40000.0}) {
        const double g = MixingLineSlope(p, 0.3), t_lm = ThresholdSaturatedK(g), e_lm = SaturationPressureWaterPa(t_lm);
        double previous = t_lm + 1.0;
        for (const double u : {1.0, 0.999, 0.99, 0.95, 0.8, 0.5, 0.2, 0.0}) {
            const double t = ContrailThresholdK(p, u, 0.3);
            SC_EXPECT_NEAR(t, t_lm - (e_lm - u * SaturationPressureWaterPa(t)) / g, 1e-9);
            SC_EXPECT(t <= t_lm && t < previous);
            previous = t;
        }
    }
}

SC_TEST("fx: contrails form high up in a standard atmosphere and not low down") {
    const AtmosphereState high = StandardAtmosphere(10500.0), low = StandardAtmosphere(7000.0);
    SC_EXPECT(Contrail(high.temperature_k, high.pressure_pa, 0.3, 0.3).kind != ContrailKind::None);
    SC_EXPECT(Contrail(low.temperature_k, low.pressure_pa, 1.0, 0.3).kind == ContrailKind::None);
    // At 10.5 km (-53 °C) moist air is ice-supersaturated: the trail persists and spreads; dry air: a short trail.
    const ContrailState wet = Contrail(high.temperature_k, high.pressure_pa, 0.7, 0.3);
    const ContrailState dry = Contrail(high.temperature_k, high.pressure_pa, 0.3, 0.3);
    SC_EXPECT(wet.kind == ContrailKind::Persistent && dry.kind == ContrailKind::Short);
    SC_EXPECT(wet.lifetime_s > 10.0 * dry.lifetime_s);
    SC_EXPECT(wet.spread_mps > dry.spread_mps);
    SC_EXPECT(dry.lifetime_s >= 2.0 && dry.lifetime_s <= 40.0);
    SC_EXPECT(wet.margin_k > 0.0 && wet.rh_ice > 1.0);
}
