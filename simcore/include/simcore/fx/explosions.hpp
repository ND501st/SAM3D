// simcore/fx/explosions.hpp — how loud, how big and how long an explosion is (docs/fx/README.md 3.3): the blast's peak
// overpressure by Kinney and Graham's fit and the peak sound level it makes, and what the eye sees (the fireball, the
// smoke, the dust, the debris, the crater, the flash), every size scaled from one reference charge by the cube root of
// the charge (Hopkinson-Cranz: distances and times of similar explosions scale with W^(1/3)).
//
// Charges are TNT equivalents in kg. Presentation only: nothing here decides damage (that is the simulation's own
// weapons model); the game sizes its effects and sounds from it.
#pragma once

#include <algorithm>
#include <cmath>

#include "simcore/atmosphere.hpp"
#include "simcore/fx/acoustics.hpp"

namespace simcore {
namespace fx {

// ------------------------------------------------------------------ the blast

// The Hopkinson-Cranz scaled distance Z = R / W^(1/3) (m/kg^(1/3)) of a range from a charge of W kg TNT.
inline double ScaledDistance(double range_m, double charge_kg) {
    return std::max(range_m, 0.0) / std::cbrt(std::max(charge_kg, 1e-12));
}

// Kinney and Graham's fit to a free-air TNT burst's peak overpressure over the ambient pressure at scaled distance Z
// (G. F. Kinney, K. J. Graham, Explosive Shocks in Air, 2nd ed., 1985):
//   dp/p0 = 808 [1 + (Z/4.5)^2] / (sqrt(1 + (Z/0.048)^2) sqrt(1 + (Z/0.32)^2) sqrt(1 + (Z/1.35)^2)).
// Z = 10: 0.0985. Far out it falls as kKinneyGrahamFar / Z, a weak shock spreading spherically.
inline double KinneyGrahamRatio(double z_m_per_kg13) {
    const double z = std::max(z_m_per_kg13, 0.0);
    const double a = z / 4.5, b = z / 0.048, c = z / 0.32, d = z / 1.35;
    return 808.0 * (1.0 + a * a) / (std::sqrt(1.0 + b * b) * std::sqrt(1.0 + c * c) * std::sqrt(1.0 + d * d));
}

// The fit's far-field constant: 808 (0.048 0.32 1.35) / 4.5^2 = 0.8274 (dp/p0 -> 0.8274 / Z).
constexpr double kKinneyGrahamFar = 808.0 * 0.048 * 0.32 * 1.35 / (4.5 * 4.5);

// A burst on the ground drives its blast into a hemisphere instead of a sphere: the air sees it as a free-air burst of
// about 1.8 times the charge (2 for a perfectly rigid surface; less, as the ground is cratered and heaved). An estimate,
// the usual rule of thumb, not checked against a test series. Multiply the charge by it for a ground burst.
constexpr double kSurfaceBurstFactor = 1.8;

// The peak overpressure (Pa) `range_m` from a burst of `charge_kg` TNT, in air of ambient pressure `p0_pa`. Away from sea
// level by Sachs scaling: the scaled distance is multiplied by (p0 / p_sea)^(1/3) (smaller aloft: 0.79 times at 50 kPa)
// and the ratio multiplies the local p0 (thinner air: a weaker blast in pascals, a stronger one as a share of the
// ambient). `burst_factor` multiplies the charge (kSurfaceBurstFactor for a ground burst).
inline double PeakOverpressurePa(double charge_kg, double range_m, double p0_pa = isa::kSeaLevelPressure,
                                 double burst_factor = 1.0) {
    const double w = charge_kg * burst_factor;
    if (w <= 0.0 || p0_pa <= 0.0) return 0.0;
    const double z = ScaledDistance(range_m, w) * std::cbrt(p0_pa / isa::kSeaLevelPressure);
    return KinneyGrahamRatio(z) * p0_pa;
}

// The blast's peak sound pressure level (dB re 20 µPa) at `range_m`, before the air's absorption (acoustics.hpp's
// AbsorptionCutoffHz takes the high frequencies off a far one) and refraction.
inline double PeakSplDb(double charge_kg, double range_m, double p0_pa = isa::kSeaLevelPressure, double burst_factor = 1.0) {
    return PressureToDb(PeakOverpressurePa(charge_kg, range_m, p0_pa, burst_factor));
}

// The far-field limit at sea level, L_pk = 192.4 + 6.67 log10 W - 20 log10 R (dB): within 1.52 dB of the fit at Z = 10
// and closer further out (0.4 dB at Z = 20). The simple form for a sound designer's table.
inline double FarFieldPeakSplDb(double charge_kg, double range_m) {
    return 20.0 * std::log10(kKinneyGrahamFar * isa::kSeaLevelPressure / 2e-5) +
           (20.0 / 3.0) * std::log10(std::max(charge_kg, 1e-12)) - 20.0 * std::log10(std::max(range_m, 1e-6));
}

// ------------------------------------------------------------------ the fireball

// The primary fireball's radius scales as W^(1/3): reported between 0.71 and 1.48 m/kg^(1/3) for high explosives
// (OSTI 1469550, as cited by the FX research notes; not checked against the paper); the middle, 1.1, is used.
constexpr double kFireballScaledRadius = 1.1;
constexpr double kFireballScaledRadiusLow = 0.71;
constexpr double kFireballScaledRadiusHigh = 1.48;

// The fireball's radius (m): 0.3 m for 0.02 kg (a 23 mm shell), 2.7 m for 15 kg (a SAM warhead), 5.0 m for 95 kg (a
// Mk 82), 6.4 m for 200 kg (a FAB-500), 8.4 m for 450 kg (a Mk 84).
inline double FireballRadiusM(double charge_kg, double scaled_radius = kFireballScaledRadius) {
    return scaled_radius * std::cbrt(std::max(charge_kg, 0.0));
}

// How long the fireball glows (s), scaled by W^(1/3) from 0.6 s for 100 kg (an estimate from footage of bomb bursts; the
// scaling is Hopkinson-Cranz's). 0.035 s for a 23 mm shell (a flash), 1.0 s for a Mk 84.
constexpr double kFireballReferenceKg = 100.0;
constexpr double kFireballReferenceS = 0.6;
inline double FireballDurationS(double charge_kg) {
    return kFireballReferenceS * std::cbrt(std::max(charge_kg, 0.0) / kFireballReferenceKg);
}

// ------------------------------------------------------------------ the look

// What an explosion looks like, for the effects to draw. Sizes are radii.
struct ExplosionLook {
    double fireball_m = 0.0;    // the fireball's radius at its largest
    double fireball_s = 0.0;    // how long it glows
    double smoke_m = 0.0;       // the smoke cloud's radius once grown
    double smoke_s = 0.0;       // how long the smoke hangs before it has thinned away (the wind carries it meanwhile)
    double dust_m = 0.0;        // the dust thrown up round a burst on or near the ground (0 in the air)
    int debris = 0;             // pieces thrown out: clods and stones on the ground, fragments as sparks in the air
    double crater_m = 0.0;      // the crater's radius (a burst on the ground only)
    double flash_m = 0.0;       // the flash sprite's radius (the first frame or two)
    double light_lumens = 0.0;  // the flash's light, a game value for the light pool (lights the smoke and the ground)
    bool ground = false;        // on or near the ground: dust, scorch and crater decals
};

// The reference: 100 kg TNT bursting on soil, between a Mk 82 and a FAB-500. Every value an estimate (sam3d, from
// footage and photographs of bomb bursts and craters), except the fireball's (above). Each scales by (W / 100)^(1/3);
// the light, a power (the energy over a time that grows as W^(1/3)), by (W / 100)^(2/3).
struct ExplosionReference {
    static constexpr double kChargeKg = 100.0;
    static constexpr double kSmokeM = 16.0;
    static constexpr double kSmokeS = 45.0;
    static constexpr double kDustM = 20.0;
    static constexpr double kDebris = 40.0;
    static constexpr double kCraterM = 3.5;      // about 0.75 m/kg^(1/3), a contact burst in ordinary soil
    static constexpr double kFlashM = 18.0;
    static constexpr double kLightLumens = 1.0e8;
    // A burst in the air throws no soil: a smaller, darker puff that thins sooner, and only fragments.
    static constexpr double kAirSmoke = 0.8;
    static constexpr double kAirSmokeTime = 0.7;
    static constexpr double kAirDebris = 0.5;
    // A burst a few metres over the ground (a proximity fuze) raises dust but digs no crater.
    static constexpr double kLowAirDust = 0.6;
};

// The look of a burst of `charge_kg` TNT. `ground`: on or near the ground (dust, decals); `airburst`: fuzed above it, not
// on contact (no crater). `ground` false is a burst high in the air (a SAM's warhead, a flak shell): `airburst` then
// changes nothing.
inline ExplosionLook LookFor(double charge_kg, bool ground, bool airburst) {
    using R = ExplosionReference;
    const double w = std::max(charge_kg, 0.0);
    const double s = std::cbrt(w / R::kChargeKg);
    const bool in_air = !ground || airburst;
    ExplosionLook l;
    l.ground = ground;
    l.fireball_m = FireballRadiusM(w);
    l.fireball_s = FireballDurationS(w);
    l.smoke_m = R::kSmokeM * s * (ground ? 1.0 : R::kAirSmoke);
    l.smoke_s = R::kSmokeS * s * (ground ? 1.0 : R::kAirSmokeTime);
    l.dust_m = ground ? R::kDustM * s * (airburst ? R::kLowAirDust : 1.0) : 0.0;
    l.debris = static_cast<int>(std::lround(R::kDebris * s * (in_air ? R::kAirDebris : 1.0)));
    l.crater_m = ground && !airburst ? R::kCraterM * s : 0.0;
    l.flash_m = R::kFlashM * s;
    l.light_lumens = R::kLightLumens * s * s;
    return l;
}

}  // namespace fx
}  // namespace simcore
