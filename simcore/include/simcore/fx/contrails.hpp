// simcore/fx/contrails.hpp — when a jet leaves a condensation trail and how long it lasts (docs/fx/README.md,
// "Contrails"): the Schmidt-Appleman criterion (U. Schumann, "On conditions for contrail formation from aircraft
// exhausts", Meteorol. Z. 5, 1996) for whether one forms, and ice supersaturation for whether it persists.
//
// The exhaust's water and heat mix with the ambient air along a straight line in (temperature, vapour pressure); a trail
// forms when that line crosses liquid-water saturation, which happens when the air is colder than a threshold set by
// the pressure, the humidity and the engine's overall propulsion efficiency. Once formed, the ice crystals live on only
// where the air is supersaturated over ice: otherwise the trail evaporates within seconds to a minute.
//
// Presentation only: the game decides from this whether to draw a trail behind each engine, how long and how wide.
#pragma once

#include <algorithm>
#include <cmath>

#include "simcore/vec3.hpp"

namespace simcore {
namespace fx {

// Saturation vapour pressure (Pa) over liquid water and over ice at `temperature_k` (Sonntag 1994, as Schumann uses).
inline double SaturationPressureWaterPa(double temperature_k) {
    const double t = temperature_k;
    return 100.0 * std::exp(-6096.9385 / t + 16.635794 - 2.711193e-2 * t + 1.673952e-5 * t * t + 2.433502 * std::log(t));
}
inline double SaturationPressureIcePa(double temperature_k) {
    const double t = temperature_k;
    return 100.0 * std::exp(-6024.5282 / t + 24.7219 + 1.0613868e-2 * t - 1.3198825e-5 * t * t - 0.49382577 * std::log(t));
}

// Kerosene's numbers and the mixing line's slope.
struct ContrailFuel {
    double ei_h2o = 1.25;        // kg of water per kg of fuel burnt
    double heat_j_per_kg = 43.2e6;
};

// The mixing line's slope G (Pa/K) at `pressure_pa` for an engine of overall propulsion efficiency `eta` (0.2-0.25 for
// 1960s turbojets, 0.3 for 1970s-80s turbofans).
inline double MixingLineSlope(double pressure_pa, double eta, const ContrailFuel& fuel = {}) {
    constexpr double kCp = 1004.0, kEpsilon = 0.622;
    return fuel.ei_h2o * kCp * pressure_pa / (kEpsilon * fuel.heat_j_per_kg * (1.0 - Clamp(eta, 0.0, 0.6)));
}

// The threshold temperature (K) for air saturated over water (Schumann's eq. 31): with G in Pa/K,
// T_LM = -46.46 + 9.43 ln(G - 0.053) + 0.720 ln^2(G - 0.053) °C.
inline double ThresholdSaturatedK(double g_pa_per_k) {
    const double x = std::log(std::max(g_pa_per_k - 0.053, 1e-6));
    return 273.15 - 46.46 + 9.43 * x + 0.720 * x * x;
}

// The threshold temperature (K) at relative humidity over water `rh_water` (0..1): below it a trail forms. Solves
// T_LC = T_LM - (e_w(T_LM) - U e_w(T_LC)) / G (Schumann 1996), saturated air's being T_LM itself, as the root of
// f(T) = T - T_LM + (e_w(T_LM) - U e_w(T)) / G by bisection. f(T_LM) = e_w(T_LM) (1 - U) / G >= 0, and below T_LM f rises
// with T (f' = 1 - U e_w'(T) / G, where e_w' < G: the mixing line touches e_w at T_LM), so the root is the one in
// [T_LM - e_w(T_LM) / G - 1 K, T_LM], where f starts below 0. (Iterating the equation itself converges ever more slowly
// as U nears 1, its slope U e_w'(T) / G tending to 1: twenty steps left 0.13 K at U = 0.99.)
inline double ContrailThresholdK(double pressure_pa, double rh_water, double eta, const ContrailFuel& fuel = {}) {
    const double g = MixingLineSlope(pressure_pa, eta, fuel);
    const double t_lm = ThresholdSaturatedK(g);
    const double u = Clamp(rh_water, 0.0, 1.0);
    if (u >= 1.0) return t_lm;
    const double e_lm = SaturationPressureWaterPa(t_lm);
    double lo = t_lm - e_lm / g - 1.0, hi = t_lm;  // f(lo) = -1 - U e_w(lo) / G < 0
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double f = mid - t_lm + (e_lm - u * SaturationPressureWaterPa(mid)) / g;
        (f < 0.0 ? lo : hi) = mid;
    }
    return 0.5 * (lo + hi);
}

// Relative humidity over ice from relative humidity over water (both 0..1) at `temperature_k`.
inline double IceHumidity(double rh_water, double temperature_k) {
    return rh_water * SaturationPressureWaterPa(temperature_k) / SaturationPressureIcePa(temperature_k);
}

enum class ContrailKind {
    None,        // too warm (or too dry for its temperature): no trail
    Short,       // forms and evaporates behind the aircraft within seconds to a minute
    Persistent,  // ice-supersaturated air: the trail lives on and spreads (minutes to hours)
};

struct ContrailState {
    ContrailKind kind = ContrailKind::None;
    double threshold_k = 0.0;   // the temperature it forms below, here
    double margin_k = 0.0;      // how far below the threshold the air is (> 0: a trail)
    double rh_ice = 0.0;        // relative humidity over ice (0..1)
    double lifetime_s = 0.0;    // how long a stretch of the trail stays visible
    double spread_mps = 0.0;    // how fast it widens
};

// The trail behind an engine of efficiency `eta` in air at `temperature_k`, `pressure_pa` and relative humidity over
// water `rh_water`. The lifetimes are the drawing's, chosen from what is observed: a short trail just under the threshold
// is a few hundred metres long (a few seconds), far under it some kilometres (tens of seconds); a persistent one is
// drawn for `persistent_cap_s` (it lives hours) and widens at about 1-2 m/s (spreading contrails, Schumann 2005).
inline ContrailState Contrail(double temperature_k, double pressure_pa, double rh_water, double eta,
                              double persistent_cap_s = 900.0) {
    ContrailState s;
    s.threshold_k = ContrailThresholdK(pressure_pa, rh_water, eta);
    s.margin_k = s.threshold_k - temperature_k;
    s.rh_ice = IceHumidity(rh_water, temperature_k);
    if (s.margin_k <= 0.0) return s;
    if (s.rh_ice >= 1.0) {
        s.kind = ContrailKind::Persistent;
        s.lifetime_s = persistent_cap_s;
        s.spread_mps = 1.0 + 0.5 * Clamp(s.rh_ice - 1.0, 0.0, 0.3) / 0.3;
    } else {
        s.kind = ContrailKind::Short;
        // The drier the air over ice and the nearer the threshold, the sooner the crystals sublimate.
        const double cold = Clamp(s.margin_k / 10.0, 0.0, 1.0);
        const double wet = Clamp(s.rh_ice / 1.0, 0.0, 1.0);
        s.lifetime_s = 2.0 + 38.0 * cold * wet * wet;
        s.spread_mps = 0.3;
    }
    return s;
}

}  // namespace fx
}  // namespace simcore
