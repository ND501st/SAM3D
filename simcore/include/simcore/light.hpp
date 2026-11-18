// simcore/light.hpp — one sun for a session (roadmap R7, docs/LIGHT.md).
//
// A scenario has a place (its origin), a date and the local hour of its second 0: its sky clock. The sun's elevation
// there and then (solar.hpp) decides the light everything else reads: the TV sight's daylight (em/optical.hpp), the
// battery's night (lamps for flags, the night march), the visual range (nav/session.hpp), the AAA's optical fire control,
// the waves' night factor, the couriers' night speed. Unreal's sky is turned by the same clock. The thresholds are
// data/gameplay/light.json.
//
// The missions menu's time of day is a preset (the mission's own, dawn, day, dusk, night, an hour) turned into a start
// hour by the scenario's own sun (PresetHour).
#pragma once

#include <cmath>
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
};

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
};

struct SunLight {
    double sun_elevation_deg = 30.0;
    double sun_azimuth_deg = 180.0;
    LightPhase phase = LightPhase::Day;
    double night_share = 0.0;  // 0 by day to 1 by night, smooth through twilight (the fx's tracers and flashes)
    bool daylight = true;      // the TV sight's
    bool dark = false;
};

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
    return l;
}

// The light at a session second. Rising: the sun higher a minute later.
inline SunLight LightAt(const SkyClock& c, double t_s, const LightValues& v) {
    const SunPosition s = c.SunAtSession(t_s);
    const SunPosition later = c.SunAtSession(t_s + 60.0);
    return LightOfSun(RadToDeg(s.elevation_rad), RadToDeg(s.azimuth_rad), later.elevation_rad > s.elevation_rad, v);
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
    char buf[16];
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
        const std::pair<const char*, double*> each[] = {{"twilight_deg", &out.twilight_deg},
                                                        {"low_sun_deg", &out.low_sun_deg},
                                                        {"tv_min_elevation_deg", &out.tv_min_elevation_deg},
                                                        {"optical_night_deg", &out.optical_night_deg}};
        if (!Keys(*values, "values", {"twilight_deg", "low_sun_deg", "tv_min_elevation_deg", "optical_night_deg"})) return false;
        for (const auto& e : each) {
            const std::string p = Join("values", e.first);
            const json::Value* v = values->Find(e.first);
            if (v == nullptr) return Fail(p, "required");
            if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number >= -18.0 && n->number <= 18.0)) {
                return Fail(Join(p, "value"), "degrees, -18 to 18");
            }
            *e.second = n->number;
        }
        if (out.twilight_deg < 0.0) return Fail("values.twilight_deg.value", "degrees below the horizon, >= 0");
        if (-out.twilight_deg > out.low_sun_deg) return Fail("values.low_sun_deg.value", "must be above -twilight_deg");
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
