// simcore/weather_data.hpp — loads the weather presets (data/weather/presets.json) a scenario names in its "weather".
#pragma once

#include <string>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"
#include "simcore/weather.hpp"

namespace simcore {

constexpr const char* kWeatherPresetsFile = "weather/presets.json";

namespace detail {

class WeatherLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, std::vector<WeatherPreset>& out) {
        if (!Keys(root, "", {"weather_id", "display_name", "status", "source", "confidence", "notes", "presets"})) return false;
        const json::Value* list = OptionalArray(root, "", "presets");
        if (list == nullptr || list->items.empty()) return Fail("presets", "required, a non-empty list");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            WeatherPreset p;
            if (!LoadPreset(list->items[i], Index("presets", i), p)) return false;
            for (const WeatherPreset& q : out) {
                if (q.id == p.id) return Fail(Index("presets", i), "'" + p.id + "' is listed twice");
            }
            out.push_back(std::move(p));
        }
        return true;
    }

private:
    bool Number(const json::Value& o, const std::string& path, const char* key, double& out) {
        const json::Value* v = o.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        return OptionalNumber(o, path, key, out);
    }

    bool LoadWind(const json::Value& w, const std::string& p, bool needs_alt, WindDef& out) {
        if (!Keys(w, p, {"alt_m", "speed_mps", "from_deg", "notes"})) return false;
        if ((needs_alt && !Number(w, p, "alt_m", out.alt_m)) || !Number(w, p, "speed_mps", out.speed_mps) ||
            !Number(w, p, "from_deg", out.from_deg)) {
            return false;
        }
        if (out.speed_mps < 0.0) return Fail(Join(p, "speed_mps"), "must be >= 0");
        return true;
    }

    bool LoadPreset(const json::Value& e, const std::string& p, WeatherPreset& out) {
        if (!Keys(e, p, {"id", "name", "air_war", "source", "confidence", "notes", "surface", "boundary_layer", "winds_aloft", "layers"}) ||
            !RequireString(e, p, "id", out.id) || !RequireString(e, p, "name", out.name) ||
            !OptionalString(e, p, "air_war", out.air_war) || !Provenance(e, p)) {
            return false;
        }
        const json::Value* s = e.Find("surface");
        if (s == nullptr) return Fail(Join(p, "surface"), "required");
        const std::string sp = Join(p, "surface");
        if (!Keys(*s, sp, {"temperature_c", "dewpoint_c", "qnh_hpa", "visibility_km", "rain_mmh", "wind_10m", "notes"}) ||
            !Number(*s, sp, "temperature_c", out.surface_temperature_c) || !Number(*s, sp, "dewpoint_c", out.surface_dewpoint_c) ||
            !OptionalNumber(*s, sp, "qnh_hpa", out.qnh_hpa) || !Number(*s, sp, "visibility_km", out.visibility_km) ||
            !OptionalNumber(*s, sp, "rain_mmh", out.rain_mmh)) {
            return false;
        }
        if (out.surface_dewpoint_c > out.surface_temperature_c) return Fail(Join(sp, "dewpoint_c"), "above the temperature");
        if (out.visibility_km <= 0.0) return Fail(Join(sp, "visibility_km"), "must be > 0");
        if (out.qnh_hpa < 900.0 || out.qnh_hpa > 1100.0) return Fail(Join(sp, "qnh_hpa"), "outside 900-1100 hPa");
        if (const json::Value* w = s->Find("wind_10m")) {
            if (!LoadWind(*w, Join(sp, "wind_10m"), false, out.surface_wind)) return false;
            out.surface_wind.alt_m = 10.0;
        }
        if (const json::Value* b = e.Find("boundary_layer")) {
            const std::string bp = Join(p, "boundary_layer");
            if (!Keys(*b, bp, {"surface_layer_m", "top_m", "veer_deg", "notes"}) ||
                !OptionalNumber(*b, bp, "surface_layer_m", out.surface_layer_m) || !OptionalNumber(*b, bp, "top_m", out.boundary_layer_m) ||
                !OptionalNumber(*b, bp, "veer_deg", out.veer_deg)) {
                return false;
            }
            if (out.surface_layer_m < 10.0 || out.boundary_layer_m <= out.surface_layer_m) {
                return Fail(bp, "needs 10 m <= surface_layer_m < top_m");
            }
        }
        if (const json::Value* a = OptionalArray(e, p, "winds_aloft")) {
            for (std::size_t i = 0; i < a->items.size(); ++i) {
                WindDef w;
                if (!LoadWind(a->items[i], Index(Join(p, "winds_aloft"), i), true, w)) return false;
                if (!out.winds_aloft.empty() && w.alt_m <= out.winds_aloft.back().alt_m) {
                    return Fail(Index(Join(p, "winds_aloft"), i), "altitudes must rise");
                }
                out.winds_aloft.push_back(w);
            }
        } else if (!Message().empty()) {
            return false;
        }
        if (const json::Value* ls = OptionalArray(e, p, "layers")) {
            for (std::size_t i = 0; i < ls->items.size(); ++i) {
                CloudLayerDef l;
                if (!LoadLayer(ls->items[i], Index(Join(p, "layers"), i), l)) return false;
                out.layers.push_back(std::move(l));
            }
        } else if (!Message().empty()) {
            return false;
        }
        return true;
    }

    bool LoadLayer(const json::Value& l, const std::string& p, CloudLayerDef& out) {
        if (!Keys(l, p, {"id", "kind", "base_m", "base", "thickness_m", "cover_oktas", "cell_m", "lwc_g_m3", "extinction_per_km",
                         "source", "notes"}) ||
            !RequireString(l, p, "id", out.id) || !RequireString(l, p, "kind", out.kind) ||
            !Number(l, p, "thickness_m", out.thickness_m) || !Number(l, p, "cover_oktas", out.cover_oktas) ||
            !Number(l, p, "cell_m", out.cell_m) || !OptionalNumber(l, p, "lwc_g_m3", out.lwc_g_m3) ||
            !OptionalNumber(l, p, "extinction_per_km", out.extinction_per_km)) {
            return false;
        }
        static const char* kinds[] = {"cumulus", "cumulonimbus", "stratocumulus", "stratus", "nimbostratus", "altocumulus", "altostratus", "cirrus"};
        bool known = false;
        for (const char* k : kinds) known = known || out.kind == k;
        if (!known) return Fail(Join(p, "kind"), "unknown cloud kind '" + out.kind + "'");
        std::string base;
        if (!OptionalString(l, p, "base", base)) return false;
        if (base == "lcl") {
            out.base_from_lcl = true;
        } else if (!base.empty()) {
            return Fail(Join(p, "base"), "only \"lcl\" (the lifting condensation level)");
        } else if (!Number(l, p, "base_m", out.base_m)) {
            return false;
        }
        if (out.thickness_m <= 0.0) return Fail(Join(p, "thickness_m"), "must be > 0");
        if (out.cover_oktas < 0.0 || out.cover_oktas > 8.0) return Fail(Join(p, "cover_oktas"), "must be 0 to 8");
        if (out.cell_m < 50.0) return Fail(Join(p, "cell_m"), "must be >= 50 m");
        if (out.lwc_g_m3 <= 0.0 && out.extinction_per_km <= 0.0) return Fail(p, "needs lwc_g_m3 or extinction_per_km");
        return true;
    }
};

}  // namespace detail

inline bool LoadWeatherPresetsJson(const std::string& text, std::vector<WeatherPreset>& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) {
        if (error) *error = "weather presets: " + parse_error;
        return false;
    }
    detail::WeatherLoader loader;
    if (!loader.Load(root, out)) {
        if (error) *error = "weather presets: " + loader.Message();
        return false;
    }
    return true;
}

inline bool LoadWeatherPresets(const std::string& data_dir, std::vector<WeatherPreset>& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(data_dir + "/" + kWeatherPresetsFile, text)) {
        if (error) *error = std::string("cannot open ") + kWeatherPresetsFile;
        return false;
    }
    return LoadWeatherPresetsJson(text, out, error);
}

// The preset `id` from data/weather/presets.json, or the one that plays the air war's weather `id` (clear, cloudy, poor).
inline bool FindWeatherPreset(const std::string& data_dir, const std::string& id, WeatherPreset& out, std::string* error = nullptr) {
    std::vector<WeatherPreset> all;
    if (!LoadWeatherPresets(data_dir, all, error)) return false;
    for (const WeatherPreset& p : all) {
        if (p.id == id) {
            out = p;
            return true;
        }
    }
    for (const WeatherPreset& p : all) {
        if (p.air_war == id) {
            out = p;
            return true;
        }
    }
    if (error) *error = "no weather preset '" + id + "' in " + kWeatherPresetsFile;
    return false;
}

}  // namespace simcore
