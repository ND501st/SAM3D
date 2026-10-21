// simcore/weather.hpp — the air the battery fights in: temperature, pressure, the wind at every height and the cloud.
//
// A scenario's "weather" (a preset from data/weather/, loaded by weather_data.hpp) becomes one Weather for the whole
// airspace, which reaches far beyond any map: aircraft 150 km out fly in the same winds and clouds.
//
// * The air: the standard atmosphere (atmosphere.hpp) shifted by the day's temperature and sea-level pressure (QNH),
//   integrated hydrostatically with the standard lapse rate (ISO 2533; ICAO Doc 7488).
// * The wind: a logarithmic surface layer over the ground's roughness up to the top of the surface layer, then a blend
//   to the winds aloft at the top of the boundary layer, veering with height (R. B. Stull, An Introduction to Boundary
//   Layer Meteorology, 1988, §9.7 and §8.5), then the winds-aloft table up to the tropopause and above.
// * The cloud: layers, each a slab between a flat base and a top, covering its share of the sky (oktas, WMO-No. 306).
//   Where a cloud stands is a repeatable noise field (noise.hpp) with features the size of the layer's cells, its
//   threshold set so the field covers the layer's share; cumulus tops rise where the field is strongest. The field
//   drifts with the wind at the layer's height. Inside a cloud, light dies away with the extinction of its droplets
//   (Kunkel 1984: extinction = 144.7 LWC^0.88 per km, LWC in g/m3), so the TV sight cannot see through one.
//
// Radar is not stopped by cloud droplets at the Osa's frequencies; rain is (em/propagation.hpp, ITU-R P.838).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "simcore/atmosphere.hpp"
#include "simcore/geo.hpp"
#include "simcore/noise.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// One wind: where it blows from (true bearing, as the forecast gives it) and how fast.
struct WindDef {
    double alt_m = 0.0;  // above mean sea level (winds aloft); 10 m above the ground for the surface wind
    double speed_mps = 0.0;
    double from_deg = 0.0;
};

struct CloudLayerDef {
    std::string id;
    std::string kind;          // cumulus, cumulonimbus, stratocumulus, stratus, nimbostratus, altocumulus, altostratus, cirrus
    double base_m = 0.0;       // above mean sea level (or set from the lifting condensation level)
    bool base_from_lcl = false;  // the base where the surface air condenses (cumulus): 125 m per degree of dewpoint spread
    double thickness_m = 0.0;  // base to the highest tops
    double cover_oktas = 0.0;  // 0 to 8
    double cell_m = 1000.0;    // the size of one cloud (cumulus) or of the gaps' pattern (stratiform)
    double lwc_g_m3 = 0.0;     // liquid water content, for the extinction (Kunkel 1984)
    double extinction_per_km = 0.0;  // given directly (ice clouds); else from lwc_g_m3
};

struct WeatherPreset {
    std::string id;
    std::string name;
    std::string air_war;              // the air war's weather this preset plays (clear, cloudy, poor)
    double surface_temperature_c = 15.0;
    double surface_dewpoint_c = 5.0;
    double qnh_hpa = 1013.25;
    double visibility_km = 40.0;
    double rain_mmh = 0.0;
    WindDef surface_wind;             // at 10 m
    double surface_layer_m = 100.0;   // the top of the logarithmic layer, above the ground
    double boundary_layer_m = 1000.0; // above the ground: from here the winds aloft
    double veer_deg = 25.0;           // how far the wind turns (clockwise) from the surface to the top of the boundary layer
    std::vector<WindDef> winds_aloft; // ascending by altitude
    std::vector<CloudLayerDef> layers;
};

// Kunkel (1984): the extinction coefficient of a water cloud of liquid water content `lwc_g_m3`, per km.
inline double CloudExtinctionPerKm(double lwc_g_m3) {
    return lwc_g_m3 > 0.0 ? 144.7 * std::pow(lwc_g_m3, 0.88) : 0.0;
}

// Espy's rule: the height above the ground where rising surface air reaches saturation (the cumulus base).
inline double LiftingCondensationHeightM(double temperature_c, double dewpoint_c) {
    return 125.0 * std::max(0.0, temperature_c - dewpoint_c);
}

// A piece of a cloud layer, for drawing (and for tests): a column `size_m` across from base to top.
struct CloudCell {
    double east_m = 0.0;
    double north_m = 0.0;
    double base_m = 0.0;  // above mean sea level
    double top_m = 0.0;
    double size_m = 0.0;
};

class Weather {
public:
    // Still, clear, standard air: what a scenario without weather gets.
    Weather() = default;

    // `ground_m`: the ground's height above sea level where the scenario plays (for the surface temperature's
    // offset and the cumulus base); `seed`: where the clouds stand.
    Weather(WeatherPreset preset, double ground_m, std::uint64_t seed) : preset_(std::move(preset)), ground_m_(ground_m) {
        const double t_isa = StandardAtmosphere(ground_m_).temperature_k;
        temperature_offset_k_ = (preset_.surface_temperature_c + 273.15) - t_isa;
        std::uint64_t s = noise::Mix64(seed ^ 0xC10D5ull);
        for (const CloudLayerDef& d : preset_.layers) {
            Layer l;
            l.def = d;
            if (d.base_from_lcl) {
                l.def.base_m = ground_m_ + LiftingCondensationHeightM(preset_.surface_temperature_c, preset_.surface_dewpoint_c);
            }
            l.cover = Clamp(d.cover_oktas / 8.0, 0.0, 1.0);
            l.extinction_per_m = (d.extinction_per_km > 0.0 ? d.extinction_per_km : CloudExtinctionPerKm(d.lwc_g_m3)) / 1000.0;
            l.cumuliform = d.kind == "cumulus" || d.kind == "cumulonimbus" || d.kind == "altocumulus";
            l.seed = s = noise::Mix64(s);
            Calibrate(l);
            const Vec3 w = WindAt(l.def.base_m + 0.5 * l.def.thickness_m, 1e9);
            l.drift_east_mps = w.x;
            l.drift_north_mps = w.y;
            layers_.push_back(l);
        }
    }

    const WeatherPreset& Preset() const { return preset_; }
    bool HasClouds() const { return !layers_.empty(); }
    std::size_t LayerCount() const { return layers_.size(); }
    const CloudLayerDef& LayerDef(std::size_t i) const { return layers_[i].def; }
    double LayerCover(std::size_t i) const { return layers_[i].cover; }
    double TemperatureOffsetK() const { return temperature_offset_k_; }

    // Where the clouds have drifted to: the field moves with the wind at each layer's height.
    void SetTime(double t_s) { time_s_ = t_s; }
    double Time() const { return time_s_; }
    Vec3 LayerDrift(std::size_t i) const {
        return {layers_[i].drift_east_mps * time_s_, layers_[i].drift_north_mps * time_s_, 0.0};
    }

    // ------------------------------------------------------------------ the air

    // Temperature, pressure, density and the speed of sound at `alt_m` above sea level. Without weather, exactly ISA.
    AtmosphereState Air(double alt_m) const {
        const double qnh_pa = preset_.qnh_hpa * 100.0;
        if (temperature_offset_k_ == 0.0 && qnh_pa == isa::kSeaLevelPressure) return StandardAtmosphere(alt_m);
        constexpr double lapse = 0.0065;
        const double t0 = isa::kSeaLevelTemperature + temperature_offset_k_;
        const double h = Clamp(alt_m, -500.0, 11000.0);
        const double exponent = kStandardGravity / (lapse * isa::kGasConstantAir);
        double t = t0 - lapse * h;
        double p = qnh_pa * std::pow(t / t0, exponent);
        if (alt_m > 11000.0) {  // above the tropopause: the standard shape, from the day's tropopause
            const AtmosphereState s11 = StandardAtmosphere(11000.0), s = StandardAtmosphere(alt_m);
            p *= s.pressure_pa / s11.pressure_pa;
            t = s.temperature_k + temperature_offset_k_ * 0.5;  // the offset fades in the stratosphere
        }
        const double rho = p / (isa::kGasConstantAir * t);
        return {t, p, rho, std::sqrt(isa::kGamma * isa::kGasConstantAir * t)};
    }

    // ------------------------------------------------------------------ the wind

    // The wind (the way the air moves, ENU m/s) at `alt_m` above sea level, `agl_m` above the ground there.
    Vec3 WindAt(double alt_m, double agl_m) const {
        double speed = 0.0, from_deg = 0.0;
        WindProfile(alt_m, agl_m, speed, from_deg);
        if (speed <= 0.0) return {};
        const double to = DegToRad(from_deg + 180.0);
        return {speed * std::sin(to), speed * std::cos(to), 0.0};
    }

    // The ground's roughness length under the wind (m): open farmland by default; the map sets its own.
    void SetRoughnessM(double z0_m) { roughness_m_ = z0_m > 1e-4 ? z0_m : 1e-4; }
    double RoughnessM() const { return roughness_m_; }

    // ------------------------------------------------------------------ the cloud

    // How much of a layer's cloud stands over (east, north) now: the field's value against its threshold, 0 if clear.
    // Coordinates are the scenario's ENU east and north.
    bool CloudColumnAt(std::size_t layer, double east_m, double north_m, double& base_m, double& top_m) const {
        const Layer& l = layers_[layer];
        if (l.cover <= 0.0) return false;
        const double x = east_m - l.drift_east_mps * time_s_, y = north_m - l.drift_north_mps * time_s_;
        return Column(l, x, y, base_m, top_m);
    }

    // Is the point at (east, north) and `alt_m` above sea level inside a cloud?
    bool InCloud(double east_m, double north_m, double alt_m) const {
        for (std::size_t i = 0; i < layers_.size(); ++i) {
            const Layer& l = layers_[i];
            if (alt_m < l.def.base_m || alt_m > l.def.base_m + l.def.thickness_m) continue;
            double b = 0.0, t = 0.0;
            if (CloudColumnAt(i, east_m, north_m, b, t) && alt_m >= b && alt_m <= t) return true;
        }
        return false;
    }

    // The optical depth of the cloud along the straight line from `a` (at `a_alt_m` above sea level) to `b`: the light
    // that gets through is exp(-depth). ENU positions in the scenario's frame; the line's height over the curved Earth
    // sags below the straight blend of the two altitudes by s(1-s)L^2/2R.
    double CloudOpticalDepth(const Vec3& a, double a_alt_m, const Vec3& b, double b_alt_m) const {
        if (layers_.empty()) return 0.0;
        const Vec3 d = b - a;
        const double length = d.Length();
        if (length <= 0.0) return 0.0;
        const double horizontal = d.HorizontalLength();
        const double sag_coeff = horizontal * horizontal / (2.0 * wgs84::kMeanRadius);
        double depth = 0.0;
        for (std::size_t i = 0; i < layers_.size(); ++i) {
            const Layer& l = layers_[i];
            if (l.cover <= 0.0 || l.extinction_per_m <= 0.0) continue;
            const double lo = l.def.base_m, hi = l.def.base_m + l.def.thickness_m;
            // The part of the line inside the slab, with the sag as a margin (it is at most L^2/8R).
            const double sag_max = 0.25 * sag_coeff;
            const double amin = std::min(a_alt_m, b_alt_m) - sag_max, amax = std::max(a_alt_m, b_alt_m);
            if (amax < lo || amin > hi) continue;
            double s0 = 0.0, s1 = 1.0;
            const double dalt = b_alt_m - a_alt_m;
            if (std::fabs(dalt) > 1e-6) {
                const double sa = (lo - sag_max - a_alt_m) / dalt, sb = (hi + sag_max - a_alt_m) / dalt;
                s0 = Clamp(std::min(sa, sb), 0.0, 1.0);
                s1 = Clamp(std::max(sa, sb), 0.0, 1.0);
            }
            const double span = (s1 - s0) * length;
            if (span <= 0.0) continue;
            const double step = std::min(250.0, 0.25 * l.def.cell_m);
            const int n = std::min(600, std::max(1, static_cast<int>(std::ceil(span / step))));
            const double ds = span / n;
            for (int k = 0; k < n; ++k) {
                const double s = s0 + (s1 - s0) * (k + 0.5) / n;
                const double alt = a_alt_m + dalt * s - s * (1.0 - s) * sag_coeff;
                if (alt < lo || alt > hi) continue;
                double cb = 0.0, ct = 0.0;
                if (CloudColumnAt(i, a.x + d.x * s, a.y + d.y * s, cb, ct) && alt >= cb && alt <= ct) {
                    depth += l.extinction_per_m * ds;
                }
            }
        }
        return depth;
    }

    double CloudTransmission(const Vec3& a, double a_alt_m, const Vec3& b, double b_alt_m) const {
        return std::exp(-CloudOpticalDepth(a, a_alt_m, b, b_alt_m));
    }

    // The clouds of one layer over a rectangle (at time 0: add LayerDrift for where they are now), sampled every
    // `step_m`: one cell per sample inside a cloud. For drawing; a cell is the column of cloud over its square.
    std::vector<CloudCell> Cells(std::size_t layer, double west_m, double south_m, double east_m, double north_m,
                                 double step_m) const {
        std::vector<CloudCell> out;
        const Layer& l = layers_[layer];
        if (l.cover <= 0.0 || step_m <= 0.0) return out;
        for (double y = south_m + 0.5 * step_m; y < north_m; y += step_m) {
            for (double x = west_m + 0.5 * step_m; x < east_m; x += step_m) {
                double b = 0.0, t = 0.0;
                if (Column(l, x, y, b, t)) out.push_back({x, y, b, t, step_m});
            }
        }
        return out;
    }

    // The share of the sky a layer covers, measured on its own field over a square `half_m` around the origin.
    double MeasuredCover(std::size_t layer, double half_m, int samples) const {
        int in = 0;
        for (int j = 0; j < samples; ++j) {
            for (int i = 0; i < samples; ++i) {
                double b = 0.0, t = 0.0;
                const double x = -half_m + 2.0 * half_m * (i + 0.5) / samples;
                const double y = -half_m + 2.0 * half_m * (j + 0.5) / samples;
                in += Column(layers_[layer], x, y, b, t) ? 1 : 0;
            }
        }
        return static_cast<double>(in) / (samples * samples);
    }

private:
    struct Layer {
        CloudLayerDef def;
        double cover = 0.0;
        double extinction_per_m = 0.0;
        bool cumuliform = false;
        std::uint64_t seed = 0;
        double threshold = 1.0;  // field values above it are cloud
        double strong = 1.0;     // the field's 99th percentile: where the tallest tops stand
        double drift_east_mps = 0.0, drift_north_mps = 0.0;
    };

    static double Field(const Layer& l, double x, double y) { return noise::Fbm(l.seed, x, y, 2.0 * l.def.cell_m, 3); }

    // The threshold that makes the field cover the layer's share, read off the field's own distribution.
    static void Calibrate(Layer& l) {
        if (l.cover >= 1.0) {
            l.threshold = -1.0;
            l.strong = 1.0;
            return;
        }
        constexpr int n = 96;
        std::vector<double> v;
        v.reserve(n * n);
        const double span = 60.0 * l.def.cell_m;
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < n; ++i) v.push_back(Field(l, span * (i + 0.37) / n, span * (j + 0.61) / n));
        }
        std::sort(v.begin(), v.end());
        const auto at = [&](double q) { return v[static_cast<std::size_t>(Clamp(q, 0.0, 1.0) * static_cast<double>(v.size() - 1))]; };
        l.threshold = l.cover <= 0.0 ? 2.0 : at(1.0 - l.cover);
        l.strong = std::max(at(0.99), l.threshold + 1e-6);
    }

    static bool Column(const Layer& l, double x, double y, double& base_m, double& top_m) {
        const double f = Field(l, x, y);
        if (f <= l.threshold) return false;
        base_m = l.def.base_m;
        if (l.cumuliform) {  // flat bases, tops rising toward the strongest part of the field
            const double strength = l.threshold < 0.0 ? 1.0 : Clamp((f - l.threshold) / (l.strong - l.threshold), 0.0, 1.0);
            top_m = base_m + l.def.thickness_m * (0.3 + 0.7 * strength);
        } else {  // a sheet whose top undulates a little
            const double u = noise::Value(l.seed ^ 0x70Bull, x, y, l.def.cell_m);
            top_m = base_m + l.def.thickness_m * (0.85 + 0.15 * u);
        }
        return true;
    }

    void WindProfile(double alt_m, double agl_m, double& speed, double& from_deg) const {
        const WindDef& s = preset_.surface_wind;
        const std::vector<WindDef>& aloft = preset_.winds_aloft;
        const double agl = std::min(agl_m, alt_m - ground_m_);
        const double bl_top = preset_.boundary_layer_m;
        // The first wind aloft (or the surface wind veered, without a table) at the top of the boundary layer.
        WindDef top = aloft.empty() ? WindDef{ground_m_ + bl_top, s.speed_mps * 1.8, s.from_deg + preset_.veer_deg}
                                    : AloftAt(ground_m_ + bl_top);
        if (agl >= bl_top || s.speed_mps <= 0.0) {
            if (aloft.empty()) {
                speed = top.speed_mps;
                from_deg = top.from_deg;
            } else {
                const WindDef w = AloftAt(alt_m);
                speed = w.speed_mps;
                from_deg = w.from_deg;
            }
            return;
        }
        const double z0 = roughness_m_;
        const double sl_top = std::max(preset_.surface_layer_m, 11.0);
        const auto log_wind = [&](double z) { return s.speed_mps * std::log(std::max(z, z0 * 1.01) / z0) / std::log(10.0 / z0); };
        const double z = std::max(agl, 0.0);
        if (z <= sl_top) {
            speed = z <= z0 ? 0.0 : log_wind(z);
            from_deg = s.from_deg;
            return;
        }
        // Between the surface layer and the boundary layer's top: blend speed and direction (the Ekman turn).
        const double f = (z - sl_top) / std::max(bl_top - sl_top, 1.0);
        speed = log_wind(sl_top) + (top.speed_mps - log_wind(sl_top)) * f;
        from_deg = s.from_deg + AngleDiff(top.from_deg, s.from_deg) * f;
    }

    // The winds-aloft table at `alt_m`, linear between its rows (directions the short way round).
    WindDef AloftAt(double alt_m) const {
        const std::vector<WindDef>& a = preset_.winds_aloft;
        if (alt_m <= a.front().alt_m) return a.front();
        if (alt_m >= a.back().alt_m) return a.back();
        for (std::size_t i = 1; i < a.size(); ++i) {
            if (alt_m <= a[i].alt_m) {
                const double f = (alt_m - a[i - 1].alt_m) / std::max(a[i].alt_m - a[i - 1].alt_m, 1e-6);
                return {alt_m, a[i - 1].speed_mps + (a[i].speed_mps - a[i - 1].speed_mps) * f,
                        a[i - 1].from_deg + AngleDiff(a[i].from_deg, a[i - 1].from_deg) * f};
            }
        }
        return a.back();
    }

    static double AngleDiff(double to_deg, double from_deg) {
        double d = std::fmod(to_deg - from_deg, 360.0);
        if (d > 180.0) d -= 360.0;
        if (d < -180.0) d += 360.0;
        return d;
    }

    WeatherPreset preset_{"standard", "Standard atmosphere, calm", "", 15.0, 5.0, 1013.25, 40.0, 0.0, {}, 100.0, 1000.0, 25.0, {}, {}};
    double ground_m_ = 0.0;
    double temperature_offset_k_ = 0.0;
    double roughness_m_ = 0.1;
    double time_s_ = 0.0;
    std::vector<Layer> layers_;
};

}  // namespace simcore
