// simcore/fx/air.hpp — how humid the air is at a height, for contrails (docs/fx/README.md 3.3): the weather presets
// give the surface dew point and the cloud layers, not the humidity aloft, so it is filled in the way a forecaster would
// read a sounding. Near the ground the surface air's own moisture (its mixing ratio kept as it rises, so its dew point
// falls about 1.8 K per km while the air cools 6.5 K per km and grows more humid); above the boundary layer, the free
// troposphere's humidity (a preset's optional "upper_rh_pct", else 40 %, an estimate); inside a cloud layer, saturated
// over water, or for an ice cloud (cirrus, or any layer colder than -38 °C, where droplets freeze) supersaturated over
// ice by 10 % (the air persistent contrails and cirrus live in). The changes are spread over 300 m at each edge.
//
// Presentation only: the game asks ContrailAt for each aircraft's height to decide its trail.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "simcore/fx/acoustics.hpp"
#include "simcore/fx/contrails.hpp"
#include "simcore/weather.hpp"

namespace simcore {
namespace fx {

constexpr double kUpperRhDefault = 0.40;          // the free troposphere's relative humidity over water: an estimate
constexpr double kDewpointLapseKPerM = 0.0018;    // a rising parcel's dew point falls 1.8 K/km (constant mixing ratio)
constexpr double kHumidityEdgeM = 300.0;          // the blend at a layer's edge, centred on it
constexpr double kCirrusIceSupersaturation = 1.1;  // relative humidity over ice inside an ice cloud
constexpr double kIceCloudBelowK = 235.15;        // -38 °C: a cloud colder than this is ice whatever its kind

namespace air_detail {

// 0 below `edge_m - 150 m`, 1 above `edge_m + 150 m`, a smoothstep between.
inline double Rise(double alt_m, double edge_m) {
    const double u = Clamp((alt_m - edge_m) / kHumidityEdgeM + 0.5, 0.0, 1.0);
    return u * u * (3.0 - 2.0 * u);
}

}  // namespace air_detail

// The relative humidity over water (0..1) at `alt_m` above sea level, in the weather `w`.
inline double RelativeHumidityAloft(const Weather& w, double alt_m) {
    const WeatherPreset& p = w.Preset();
    const double temperature_k = w.Air(alt_m).temperature_k;
    // The boundary layer: the surface air's dew point, falling slowly with height.
    const double agl = std::max(alt_m - w.GroundM(), 0.0);
    const double dewpoint_c = p.surface_dewpoint_c - kDewpointLapseKPerM * agl;
    const double near_ground = RelativeHumidityPct(temperature_k - 273.15, dewpoint_c) / 100.0;
    const double upper = w.UpperRhPct() >= 0.0 ? Clamp(w.UpperRhPct() / 100.0, 0.0, 1.0) : kUpperRhDefault;
    const double free_air = air_detail::Rise(alt_m, w.GroundM() + p.boundary_layer_m);
    double rh = near_ground + (upper - near_ground) * free_air;
    // The cloud layers, in their order.
    for (std::size_t i = 0; i < w.LayerCount(); ++i) {
        const CloudLayerDef& l = w.LayerDef(i);
        if (w.LayerCover(i) <= 0.0) continue;
        const double inside = air_detail::Rise(alt_m, l.base_m) * (1.0 - air_detail::Rise(alt_m, l.base_m + l.thickness_m));
        if (inside <= 0.0) continue;
        const bool ice = l.kind == "cirrus" || temperature_k < kIceCloudBelowK;
        const double cloud = ice ? kCirrusIceSupersaturation * SaturationPressureIcePa(temperature_k) /
                                       SaturationPressureWaterPa(temperature_k)
                                 : 1.0;
        rh += (cloud - rh) * inside;
    }
    return Clamp(rh, 0.0, 1.0);
}

// The trail behind an engine of overall propulsion efficiency `eta` flying at `alt_m` above sea level in the weather `w`:
// the Schmidt-Appleman criterion (contrails.hpp) in the air's temperature and pressure there and its humidity aloft.
inline ContrailState ContrailAt(const Weather& w, double alt_m, double eta, double persistent_cap_s = 900.0) {
    const AtmosphereState air = w.Air(alt_m);
    return Contrail(air.temperature_k, air.pressure_pa, RelativeHumidityAloft(w, alt_m), eta, persistent_cap_s);
}

}  // namespace fx
}  // namespace simcore
