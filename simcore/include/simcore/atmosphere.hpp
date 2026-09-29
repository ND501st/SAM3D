// simcore/atmosphere.hpp — International Standard Atmosphere (ISA), 0–32 km.
// Reference: ISO 2533:1975 / U.S. Standard Atmosphere 1976 (first three layers).
#pragma once

#include <cmath>

#include "simcore/vec3.hpp"

namespace simcore {

struct AtmosphereState {
    double temperature_k;
    double pressure_pa;
    double density_kg_m3;
    double speed_of_sound_mps;
};

namespace isa {
constexpr double kSeaLevelTemperature = 288.15;   // K
constexpr double kSeaLevelPressure = 101325.0;    // Pa
constexpr double kGasConstantAir = 287.05287;     // J/(kg K)
constexpr double kGamma = 1.4;
}  // namespace isa

// Geopotential altitude is treated as geometric altitude (error < 0.5 % below 32 km).
inline AtmosphereState StandardAtmosphere(double altitude_m) {
    using namespace isa;
    const double g0 = kStandardGravity;
    const double h = Clamp(altitude_m, -500.0, 32000.0);

    double t = 0.0;
    double p = 0.0;
    if (h <= 11000.0) {  // troposphere, lapse 6.5 K/km
        constexpr double lapse = 0.0065;
        t = kSeaLevelTemperature - lapse * h;
        p = kSeaLevelPressure * std::pow(t / kSeaLevelTemperature, g0 / (lapse * kGasConstantAir));
    } else {
        constexpr double t11 = 216.65;
        const double p11 = kSeaLevelPressure * std::pow(t11 / kSeaLevelTemperature, g0 / (0.0065 * kGasConstantAir));
        if (h <= 20000.0) {  // tropopause, isothermal
            t = t11;
            p = p11 * std::exp(-g0 * (h - 11000.0) / (kGasConstantAir * t11));
        } else {  // lower stratosphere, +1 K/km
            constexpr double lapse = 0.001;
            const double p20 = p11 * std::exp(-g0 * 9000.0 / (kGasConstantAir * t11));
            t = t11 + lapse * (h - 20000.0);
            p = p20 * std::pow(t / t11, -g0 / (lapse * kGasConstantAir));
        }
    }
    // Above 32 km: continue isothermally so density keeps falling smoothly.
    if (altitude_m > 32000.0) {
        p *= std::exp(-g0 * (altitude_m - 32000.0) / (kGasConstantAir * t));
    }
    const double rho = p / (kGasConstantAir * t);
    return {t, p, rho, std::sqrt(kGamma * kGasConstantAir * t)};
}

}  // namespace simcore
