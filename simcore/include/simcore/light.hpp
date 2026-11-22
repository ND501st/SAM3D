// simcore/light.hpp — one sun for a session (roadmap R7, docs/LIGHT.md).
//
// A scenario has a place (its origin), a date and the local hour of its second 0: its sky clock. The sun's elevation
// there and then (solar.hpp) decides the light everything else reads: the TV sight's daylight (em/optical.hpp), the
// battery's night (lamps for flags, the night march), the visual range (nav/session.hpp), the AAA's optical fire control,
// the waves' night factor, the couriers' night speed. Unreal's sky is turned by the same clock. The thresholds are
// data/gameplay/light.json.
//
// Roadmap S8: the light on the ground in lux (the sun's clear sky through the twilight, and the moon by its phase and
// height), the TV's contrast threshold raised as that light fails through the twilight (a gradient, not only the yes or
// no at -6 deg), and the moon's share of a night's light for the eye's range.
//
// The missions menu's time of day is a preset (the mission's own, dawn, day, dusk, night, an hour) turned into a start
// hour by the scenario's own sun (PresetHour).
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"  // detail::ReadTextFile
#include "simcore/json.hpp"
#include "simcore/solar.hpp"

namespace simcore {

// ---------------------------------------------------------------------------------------------- the sky clock

struct SkyClock {
    double lat_deg = 46.9;       // the scenario's origin (the Kesmark range by default)
    double lon_deg = 19.7;
    int day_of_year = 288;       // 15 October unless the scenario has a "date"
    double utc_offset_h = 1.0;   // the local clock ahead of UTC
    double start_local_h = 10.5; // the local hour at the session's second 0
    int year = 1985;             // the date's year: the sun's place repeats year on year, the moon's does not

    // Whole days and the local hour 0-24 at a session second.
    double LocalHourAt(double t_s) const {
        double h = start_local_h + t_s / 3600.0;
        h -= 24.0 * std::floor(h / 24.0);
        return h;
    }
    int DayOfYearAt(double t_s) const {
        const int days = static_cast<int>(std::floor((start_local_h + t_s / 3600.0) / 24.0));
        if (days == 0) return day_of_year;
        const int year_len = day_of_year == 366 ? 366 : 365;  // the year after the last is a common one, near enough
        int d = (day_of_year - 1 + days) % year_len;
        if (d < 0) d += year_len;
        return d + 1;
    }
    // The sun at a session second: the same wrap of days Unreal's sky used (SimSkyWeatherActor), moved here.
    SunPosition SunAtSession(double t_s) const {
        return SunAt(lat_deg, lon_deg, DayOfYearAt(t_s), LocalHourAt(t_s) - utc_offset_h);
    }
    // The moon at a session second, in the date's own year (the next one past its last day, the one before its first).
    MoonPosition MoonAtSession(double t_s) const {
        const int days = static_cast<int>(std::floor((start_local_h + t_s / 3600.0) / 24.0));
        const int doy = DayOfYearAt(t_s);
        const int y = year + (days > 0 && doy < day_of_year ? 1 : 0) - (days < 0 && doy > day_of_year ? 1 : 0);
        return MoonAt(lat_deg, lon_deg, y, doy, LocalHourAt(t_s) - utc_offset_h);
    }
};

// ---------------------------------------------------------------------------------------------- light on the ground (S8)

// The clear sky's light on level ground from the sun, in lux, by its elevation: log-interpolated between the natural
// illumination tables' points (brown_1952): about 400 lux at sunset, 3.4 at the end of civil twilight (-6 deg), 0.008 at
// the end of nautical twilight (-12 deg), the moonless night sky's own (starlight and airglow) about 0.001 below.
inline double SkyLuxOfSun(double sun_elevation_deg) {
    static const double kDeg[] = {-18.0, -12.0, -6.0, -0.8, 0.0, 5.0, 10.0, 20.0, 30.0, 45.0, 60.0, 90.0};
    static const double kLux[] = {0.001, 0.008, 3.4, 400.0, 700.0, 5000.0, 15000.0, 35000.0, 55000.0, 80000.0, 100000.0, 115000.0};
    const std::size_t n = sizeof(kDeg) / sizeof(kDeg[0]);
    if (sun_elevation_deg <= kDeg[0]) return kLux[0];
    if (sun_elevation_deg >= kDeg[n - 1]) return kLux[n - 1];
    std::size_t i = 1;
    while (sun_elevation_deg > kDeg[i]) ++i;
    const double f = (sun_elevation_deg - kDeg[i - 1]) / (kDeg[i] - kDeg[i - 1]);
    return std::exp(std::log(kLux[i - 1]) + f * (std::log(kLux[i]) - std::log(kLux[i - 1])));
}

// The moon's light on level ground in lux (krisciunas_schaefer_1991): its magnitude by the phase angle (Allen's
// -12.73 + 0.026 |a| + 4e-9 a^4, a in degrees), 2.54e-6 lux from a source of magnitude 0, on level ground by the sine of
// its height, dimmed by the air it shines through (0.2 magnitudes an air mass, a clear lowland sky: an estimate; the air
// mass (1 - 0.96 sin^2 z)^-1/2). 0 with the moon down. A full moon high gives about a quarter of a lux.
inline double MoonLux(double moon_elevation_deg, double phase_angle_deg) {
    if (moon_elevation_deg <= 0.0) return 0.0;
    const double a = std::fabs(phase_angle_deg);
    const double mag = -12.73 + 0.026 * a + 4e-9 * a * a * a * a;
    const double sin_h = std::sin(DegToRad(moon_elevation_deg));
    const double sin_z = std::cos(DegToRad(moon_elevation_deg));
    const double air_mass = 1.0 / std::sqrt(1.0 - 0.96 * sin_z * sin_z);
    return 2.54e-6 * std::pow(10.0, -0.4 * mag) * sin_h * std::pow(10.0, -0.4 * 0.2 * air_mass);
}

// ---------------------------------------------------------------------------------------------- the light

enum class LightPhase { Day, Dawn, Dusk, Night };

inline const char* LightPhaseName(LightPhase p) {
    switch (p) {
        case LightPhase::Day: return "day";
        case LightPhase::Dawn: return "dawn";
        case LightPhase::Dusk: return "dusk";
        case LightPhase::Night: return "night";
    }
    return "?";
}

// The thresholds (data/gameplay/light.json; these defaults when the data folder has none).
struct LightValues {
    double twilight_deg = 6.0;           // night: the sun more than this below the horizon (civil twilight)
    double low_sun_deg = 6.0;            // day: the sun at least this high; between the two, dawn or dusk
    double tv_min_elevation_deg = -6.0;  // the TV sight has daylight with the sun at or above this
    double optical_night_deg = -6.0;     // dark (lamps, night march, the guns' optical sights blind) below this
    // Roadmap S8. The TV works at its full contrast threshold in this much light or more; below it the threshold rises as
    // the square root of the light's fall (Rose's law, rose_1948). 0: no gradient (only the yes or no at
    // tv_min_elevation_deg), the code's default for a data folder without the value.
    double tv_full_lux = 0.0;
    double moonless_lux = 0.001;  // a moonless night's light: the moon's share 0 at or below it
    double full_moon_lux = 0.25;  // a full moon high: the moon's share 1 (log between)
};

struct SunLight {
    double sun_elevation_deg = 30.0;
    double sun_azimuth_deg = 180.0;
    LightPhase phase = LightPhase::Day;
    double night_share = 0.0;  // 0 by day to 1 by night, smooth through twilight (the fx's tracers and flashes)
    bool daylight = true;      // the TV sight's
    bool dark = false;
    // Roadmap S8.
    double lux = 100000.0;             // the light on level ground: the sun's sky and the moon's
    double tv_threshold_factor = 1.0;  // the TV's contrast threshold times this (1 in full light, more in the twilight)
    double moon_elevation_deg = -90.0;
    double moon_azimuth_deg = 0.0;
    double moon_illuminated = 0.0;     // the lit share of its disc
    double moon_lux = 0.0;
    double moon_share = 0.0;           // 0 no moon (down, new or thin) to 1 a full moon high: the night eye's light
};

// The TV's threshold factor in `lux` of light (Rose: the threshold contrast as one over the square root of the light).
inline double TvThresholdFactor(double lux, const LightValues& v) {
    if (v.tv_full_lux <= 0.0 || lux >= v.tv_full_lux) return 1.0;
    return std::sqrt(v.tv_full_lux / (lux > 1e-9 ? lux : 1e-9));
}

// 0 with the sun 6 deg up or higher, 1 at 6 deg below or lower, a smoothstep between (Unreal's fx, SimFxLogic).
inline double NightShare(double sun_elevation_deg) {
    double x = (sun_elevation_deg + 6.0) / 12.0;
    x = x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
    return 1.0 - x * x * (3.0 - 2.0 * x);
}

// The light from a sun elevation, rising or not.
inline SunLight LightOfSun(double elevation_deg, double azimuth_deg, bool rising, const LightValues& v) {
    SunLight l;
    l.sun_elevation_deg = elevation_deg;
    l.sun_azimuth_deg = azimuth_deg;
    if (elevation_deg >= v.low_sun_deg) {
        l.phase = LightPhase::Day;
    } else if (elevation_deg < -v.twilight_deg) {
        l.phase = LightPhase::Night;
    } else {
        l.phase = rising ? LightPhase::Dawn : LightPhase::Dusk;
    }
    l.night_share = NightShare(elevation_deg);
    l.daylight = elevation_deg >= v.tv_min_elevation_deg;
    l.dark = elevation_deg < v.optical_night_deg;
    l.lux = SkyLuxOfSun(elevation_deg);
    l.tv_threshold_factor = TvThresholdFactor(l.lux, v);
    return l;
}

// The moon's part in a light: its place, its light added to the sun's, the TV's threshold again, the moon's share.
inline void AddMoon(SunLight& l, const MoonPosition& m, const LightValues& v) {
    l.moon_elevation_deg = RadToDeg(m.elevation_rad);
    l.moon_azimuth_deg = RadToDeg(m.azimuth_rad);
    l.moon_illuminated = m.illuminated;
    l.moon_lux = MoonLux(l.moon_elevation_deg, RadToDeg(m.phase_angle_rad));
    l.lux += l.moon_lux;
    l.tv_threshold_factor = TvThresholdFactor(l.lux, v);
    l.moon_share = 0.0;
    if (l.moon_lux > v.moonless_lux && v.full_moon_lux > v.moonless_lux) {
        const double share = std::log(l.moon_lux / v.moonless_lux) / std::log(v.full_moon_lux / v.moonless_lux);
        l.moon_share = share > 1.0 ? 1.0 : share;
    }
}

// The light at a session second. Rising: the sun higher a minute later.
inline SunLight LightAt(const SkyClock& c, double t_s, const LightValues& v) {
    const SunPosition s = c.SunAtSession(t_s);
    const SunPosition later = c.SunAtSession(t_s + 60.0);
    SunLight l = LightOfSun(RadToDeg(s.elevation_rad), RadToDeg(s.azimuth_rad), later.elevation_rad > s.elevation_rad, v);
    AddMoon(l, c.MoonAtSession(t_s), v);
    return l;
}

// ---------------------------------------------------------------------------------------------- the menu's time of day

enum class TimePreset { Mission, Dawn, Day, Dusk, Night, Hour };

inline const char* TimePresetName(TimePreset p) {
    switch (p) {
        case TimePreset::Mission: return "mission";
        case TimePreset::Dawn: return "dawn";
        case TimePreset::Day: return "day";
        case TimePreset::Dusk: return "dusk";
        case TimePreset::Night: return "night";
        case TimePreset::Hour: return "hour";
    }
    return "?";
}

inline bool ParseTimePreset(const std::string& s, TimePreset& out) {
    for (TimePreset p : {TimePreset::Mission, TimePreset::Dawn, TimePreset::Day, TimePreset::Dusk, TimePreset::Night, TimePreset::Hour}) {
        if (s == TimePresetName(p)) {
            out = p;
            return true;
        }
    }
    return false;
}

// The local hour of the sun's transit (local apparent noon) on the clock's first day.
inline double SolarNoonLocal(const SkyClock& c) {
    double utc = 12.0 - c.lon_deg / 15.0;
    for (int i = 0; i < 2; ++i) utc = 12.0 - c.lon_deg / 15.0 - SunAt(c.lat_deg, c.lon_deg, c.day_of_year, utc).equation_of_time_min / 60.0;
    double h = utc + c.utc_offset_h;
    h -= 24.0 * std::floor(h / 24.0);
    return h;
}

// The start hour a preset gives on the clock's day: dawn and dusk with the sun 3 deg below the horizon (mid civil
// twilight), rising and setting; day an hour and a half before noon; night at solar midnight. Without a crossing (the
// polar day or night) dawn and dusk fall back to 05:30 and 18:30. Mission and Hour keep the clock's own start.
inline double PresetHour(const SkyClock& c, TimePreset p) {
    double h = c.start_local_h;
    switch (p) {
        case TimePreset::Mission:
        case TimePreset::Hour: return c.start_local_h;
        case TimePreset::Dawn:
            if (!SunCrossing(c.lat_deg, c.lon_deg, c.day_of_year, c.utc_offset_h, -3.0, true, h)) h = 5.5;
            return h;
        case TimePreset::Dusk:
            if (!SunCrossing(c.lat_deg, c.lon_deg, c.day_of_year, c.utc_offset_h, -3.0, false, h)) h = 18.5;
            return h;
        case TimePreset::Day: h = SolarNoonLocal(c) - 1.5; break;
        case TimePreset::Night: h = SolarNoonLocal(c) + 12.0; break;
    }
    h -= 24.0 * std::floor(h / 24.0);
    return h;
}

// "17:08" for a local hour.
inline std::string FormatLocalHour(double h) {
    long long m = std::llround(h * 60.0) % (24 * 60);
    if (m < 0) m += 24 * 60;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld", m / 60, m % 60);
    return buf;
}

// ---------------------------------------------------------------------------------------------- data

namespace detail {

class LightValuesLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, LightValues& out) {
        if (!Keys(root, "", {"light_id", "status", "notes", "source", "confidence", "values"}) || !Provenance(root, "")) return false;
        const json::Value* values = root.Find("values");
        if (values == nullptr) return Fail("values", "required");
        struct Entry {
            const char* key;
            double* to;
            double lo;
            double hi;
            bool required;  // the S8 values may be left out (the code's: no TV gradient)
            const char* what;
        };
        const Entry each[] = {{"twilight_deg", &out.twilight_deg, -18.0, 18.0, true, "degrees, -18 to 18"},
                              {"low_sun_deg", &out.low_sun_deg, -18.0, 18.0, true, "degrees, -18 to 18"},
                              {"tv_min_elevation_deg", &out.tv_min_elevation_deg, -18.0, 18.0, true, "degrees, -18 to 18"},
                              {"optical_night_deg", &out.optical_night_deg, -18.0, 18.0, true, "degrees, -18 to 18"},
                              {"tv_full_lux", &out.tv_full_lux, 0.0, 200000.0, false, "lux, 0 to 200000"},
                              {"moonless_lux", &out.moonless_lux, 1e-6, 1.0, false, "lux, 1e-6 to 1"},
                              {"full_moon_lux", &out.full_moon_lux, 1e-6, 10.0, false, "lux, 1e-6 to 10"}};
        if (!Keys(*values, "values",
                  {"twilight_deg", "low_sun_deg", "tv_min_elevation_deg", "optical_night_deg", "tv_full_lux", "moonless_lux", "full_moon_lux"})) {
            return false;
        }
        for (const Entry& e : each) {
            const std::string p = Join("values", e.key);
            const json::Value* v = values->Find(e.key);
            if (v == nullptr) {
                if (e.required) return Fail(p, "required");
                continue;
            }
            if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number >= e.lo && n->number <= e.hi)) return Fail(Join(p, "value"), e.what);
            *e.to = n->number;
        }
        if (out.twilight_deg < 0.0) return Fail("values.twilight_deg.value", "degrees below the horizon, >= 0");
        if (-out.twilight_deg > out.low_sun_deg) return Fail("values.low_sun_deg.value", "must be above -twilight_deg");
        if (out.full_moon_lux <= out.moonless_lux) return Fail("values.full_moon_lux.value", "must be above moonless_lux");
        return true;
    }
};

}  // namespace detail

inline bool LoadLightValues(const std::string& data_dir, const std::string& rel, LightValues& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "light: cannot read " + rel;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "light: " + e;
        return false;
    }
    LightValues v;
    detail::LightValuesLoader loader;
    if (!loader.Load(root, v)) {
        if (error != nullptr) *error = "light: " + loader.Message();
        return false;
    }
    out = v;
    return true;
}

}  // namespace simcore
