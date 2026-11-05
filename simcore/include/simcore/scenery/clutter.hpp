// simcore/scenery/clutter.hpp — the ground clutter round the camera (roadmap T6.1, docs/scenery/T5_T8_PLAN.md): tufts of
// grass, stubble, maize stumps, dry stalks and reeds scattered over the ground layers within a ring of the camera, from
// the layer under each plant (PointLayer: the exact field edge, the crop calendar's day). Rules from
// data/scenery/clutter.json (LoadClutterRules).
//
// Each 4 m cell of the world draws its candidate plants from its own position alone, so the same cell gives the same
// plants every time the ring moves back over it and on every client (docs/MULTIPLAYER.md); each is kept by its distance to
// the ring's centre (density 1 / (1 + (d / falloff)^2)), so a ring's edge has few plants left and a moving ring thins and
// thickens plants one by one. Presentation only: nothing in the simulation reads it.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/json.hpp"
#include "simcore/scenery/ground_layers.hpp"
#include "simcore/scenery/map_meshes.hpp"

namespace simcore {
namespace scenery {

struct ClutterKind {
    std::string id, part;
    float height_min_m = 0.1f, height_max_m = 0.3f, width_min_m = 0.1f, width_max_m = 0.3f;
    double brightness = 0.0;
};

struct ClutterEntry {
    int kind = 0;
    double per_m2 = 0.0;  // plants a square metre at the camera
};

struct ClutterRules {
    double ring_m = 100.0, cell_m = 4.0, falloff_m = 30.0, rebuild_m = 16.0;
    int max_per_entry_per_cell = 32;
    std::vector<ClutterKind> kinds;
    std::vector<std::vector<ClutterEntry>> by_layer;  // indexed by the ground layer's number
    std::vector<std::string> parts;                    // the distinct part keys, in first-use order
    int PartIndex(const std::string& part) const {
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (parts[i] == part) return static_cast<int>(i);
        }
        return -1;
    }
};

// One plant: where, how big, which kind, how bright.
struct ClutterPlant {
    double east_m = 0.0, north_m = 0.0;
    float height_m = 0.0f, width_m = 0.0f, yaw_rad = 0.0f, brightness = 1.0f;
    std::uint8_t kind = 0;
};

namespace clutter_detail {

inline bool Fail(std::string* error, const std::string& why) {
    if (error != nullptr) *error = why;
    return false;
}

inline std::uint64_t Mix(std::int64_t cx, std::int64_t cy, std::uint32_t salt) {
    std::uint64_t x = static_cast<std::uint64_t>(cx) * 0x9E3779B97F4A7C15ull;
    x ^= static_cast<std::uint64_t>(cy) * 0xC2B2AE3D27D4EB4Full + salt * 0x165667B19E3779F9ull;
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDull;
    x ^= x >> 33;
    x *= 0xC4CEB9FE1A85EC53ull;
    x ^= x >> 33;
    return x;
}

inline double Unit(std::uint64_t h) { return static_cast<double>(h >> 11) / 9007199254740992.0; }

inline bool Range(const json::Value* v, float& lo, float& hi) {
    if (v == nullptr || !v->Is(json::Type::Array) || v->items.size() != 2 || !v->items[0].Is(json::Type::Number) || !v->items[1].Is(json::Type::Number)) return false;
    lo = static_cast<float>(v->items[0].number);
    hi = static_cast<float>(v->items[1].number);
    return lo > 0.0f && hi >= lo && hi <= 4.0f;
}

}  // namespace clutter_detail

// Loads the rules from a file path or a JSON text, the layers named as in `layers` (ground_layers.json).
inline bool LoadClutterRules(const std::string& path_or_json, const GroundLayerRules& layers, ClutterRules& out, std::string* error = nullptr) {
    using clutter_detail::Fail;
    std::string text = path_or_json;
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || text[first] != '{') {
        std::ifstream f(path_or_json, std::ios::binary);
        if (!f) return Fail(error, "cannot open " + path_or_json);
        std::ostringstream s;
        s << f.rdbuf();
        text = s.str();
    }
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) return Fail(error, parse_error);
    ClutterRules r;
    const auto number = [&](const char* key, double& to, double lo, double hi) {
        if (const json::Value* v = root.Find(key)) {
            if (!v->Is(json::Type::Number) || v->number < lo || v->number > hi) return Fail(error, std::string(key) + " is not in " + std::to_string(lo) + ".." + std::to_string(hi));
            to = v->number;
        }
        return true;
    };
    double cap = r.max_per_entry_per_cell;
    if (!number("ring_m", r.ring_m, 5.0, 500.0) || !number("cell_m", r.cell_m, 1.0, 20.0) || !number("falloff_m", r.falloff_m, 1.0, 500.0) ||
        !number("rebuild_m", r.rebuild_m, 1.0, 100.0) || !number("max_per_entry_per_cell", cap, 1.0, 200.0)) {
        return false;
    }
    r.max_per_entry_per_cell = static_cast<int>(cap);
    const json::Value* kinds = root.Find("kinds");
    if (kinds == nullptr || !kinds->Is(json::Type::Object) || kinds->keys.empty()) return Fail(error, "no \"kinds\" object");
    const std::vector<std::string> known = MapMeshParts();
    for (std::size_t i = 0; i < kinds->keys.size(); ++i) {
        const json::Value& k = kinds->items[i];
        ClutterKind c;
        c.id = kinds->keys[i];
        const json::Value* part = k.Find("part");
        if (part == nullptr || !part->Is(json::Type::String) || std::find(known.begin(), known.end(), part->string) == known.end()) {
            return Fail(error, c.id + ": part is not a part the map draws (MapMeshParts)");
        }
        c.part = part->string;
        if (!clutter_detail::Range(k.Find("height_m"), c.height_min_m, c.height_max_m)) return Fail(error, c.id + ": height_m is two metres in 0..4, low first");
        if (!clutter_detail::Range(k.Find("width_m"), c.width_min_m, c.width_max_m)) return Fail(error, c.id + ": width_m is two metres in 0..4, low first");
        const json::Value* bright = k.Find("brightness");
        if (bright == nullptr || !bright->Is(json::Type::Number) || bright->number < 0.0 || bright->number > 0.5) return Fail(error, c.id + ": brightness is not in 0..0.5");
        c.brightness = bright->number;
        if (r.PartIndex(c.part) < 0) r.parts.push_back(c.part);
        r.kinds.push_back(c);
    }
    r.by_layer.assign(layers.layers.size(), {});
    const json::Value* by = root.Find("layers");
    if (by == nullptr || !by->Is(json::Type::Object)) return Fail(error, "no \"layers\" object");
    for (std::size_t i = 0; i < by->keys.size(); ++i) {
        int layer = -1;
        for (std::size_t l = 0; l < layers.layers.size(); ++l) {
            if (layers.layers[l].id == by->keys[i]) layer = static_cast<int>(l);
        }
        if (layer < 0) return Fail(error, "layers: no ground layer \"" + by->keys[i] + "\"");
        if (!by->items[i].Is(json::Type::Array)) return Fail(error, "layers: " + by->keys[i] + " is not a list");
        for (const json::Value& e : by->items[i].items) {
            const json::Value* kind = e.Find("kind");
            const json::Value* per = e.Find("per_m2");
            if (kind == nullptr || !kind->Is(json::Type::String)) return Fail(error, by->keys[i] + ": an entry names no kind");
            int ki = -1;
            for (std::size_t k = 0; k < r.kinds.size(); ++k) {
                if (r.kinds[k].id == kind->string) ki = static_cast<int>(k);
            }
            if (ki < 0) return Fail(error, by->keys[i] + ": no kind \"" + kind->string + "\"");
            if (per == nullptr || !per->Is(json::Type::Number) || per->number <= 0.0 || per->number > 8.0) return Fail(error, by->keys[i] + ": per_m2 is not in 0..8");
            r.by_layer[static_cast<std::size_t>(layer)].push_back(ClutterEntry{ki, per->number});
        }
    }
    out = r;
    return true;
}

// The plants of the ring round (east, north) on the day: the cells whose centres lie within ring_m, each plant at its own
// point, on the layer its kind is listed for. Appends to `out`.
inline void ClutterAround(const ClutterRules& rules, const CoverRaster& cover, const DressingRules& dressing, const GroundLayerRules& layers,
                          int day_of_year, double east_m, double north_m, std::vector<ClutterPlant>& out) {
    using clutter_detail::Mix;
    using clutter_detail::Unit;
    const double cell = rules.cell_m;
    const std::int64_t c0 = static_cast<std::int64_t>(std::floor((east_m - rules.ring_m) / cell));
    const std::int64_t c1 = static_cast<std::int64_t>(std::floor((east_m + rules.ring_m) / cell));
    const std::int64_t r0 = static_cast<std::int64_t>(std::floor((north_m - rules.ring_m) / cell));
    const std::int64_t r1 = static_cast<std::int64_t>(std::floor((north_m + rules.ring_m) / cell));
    for (std::int64_t cy = r0; cy <= r1; ++cy) {
        for (std::int64_t cx = c0; cx <= c1; ++cx) {
            const double ce = (static_cast<double>(cx) + 0.5) * cell, cn = (static_cast<double>(cy) + 0.5) * cell;
            const double d = std::hypot(ce - east_m, cn - north_m);
            if (d > rules.ring_m) continue;
            // The layers under the cell: its centre and corners. One layer: every plant takes it. Several: each plant is checked.
            int sample[5];
            const double h = 0.5 * cell;
            sample[0] = PointLayer(cover, dressing, layers, ce, cn, day_of_year);
            sample[1] = PointLayer(cover, dressing, layers, ce - h, cn - h, day_of_year);
            sample[2] = PointLayer(cover, dressing, layers, ce + h, cn - h, day_of_year);
            sample[3] = PointLayer(cover, dressing, layers, ce - h, cn + h, day_of_year);
            sample[4] = PointLayer(cover, dressing, layers, ce + h, cn + h, day_of_year);
            bool mixed = false;
            for (int k = 1; k < 5; ++k) mixed = mixed || sample[k] != sample[0];
            for (int s = 0; s < (mixed ? 5 : 1); ++s) {
                const int layer = sample[s];
                if (layer < 0 || static_cast<std::size_t>(layer) >= rules.by_layer.size()) continue;
                bool seen = false;
                for (int p = 0; p < s; ++p) seen = seen || sample[p] == layer;
                if (seen) continue;  // each layer's entries once per cell
                const std::vector<ClutterEntry>& entries = rules.by_layer[static_cast<std::size_t>(layer)];
                for (std::size_t en = 0; en < entries.size(); ++en) {
                    const ClutterEntry& entry = entries[en];
                    const ClutterKind& kind = rules.kinds[static_cast<std::size_t>(entry.kind)];
                    const std::uint32_t salt = static_cast<std::uint32_t>(layer * 8 + en);
                    // The cell's candidates are fixed (the full density); a plant is kept by its distance to the ring's centre, so the
                    // ring moving thins and thickens the plants one by one instead of reshuffling a cell.
                    const double expect = entry.per_m2 * cell * cell;
                    const int count = std::min(rules.max_per_entry_per_cell, static_cast<int>(std::floor(expect + Unit(Mix(cx, cy, salt * 977u + 1u)))));
                    for (int j = 0; j < count; ++j) {
                        const std::uint32_t sj = salt * 977u + 16u + static_cast<std::uint32_t>(j) * 7u;
                        ClutterPlant plant;
                        plant.east_m = (static_cast<double>(cx) + Unit(Mix(cx, cy, sj))) * cell;
                        plant.north_m = (static_cast<double>(cy) + Unit(Mix(cx, cy, sj + 1u))) * cell;
                        const double dp = std::hypot(plant.east_m - east_m, plant.north_m - north_m);
                        if (dp > rules.ring_m) continue;
                        if (Unit(Mix(cx, cy, sj + 6u)) >= 1.0 / (1.0 + (dp / rules.falloff_m) * (dp / rules.falloff_m))) continue;
                        if (mixed && PointLayer(cover, dressing, layers, plant.east_m, plant.north_m, day_of_year) != layer) continue;
                        const double u = Unit(Mix(cx, cy, sj + 2u)), v = Unit(Mix(cx, cy, sj + 3u));
                        plant.height_m = static_cast<float>(kind.height_min_m + (kind.height_max_m - kind.height_min_m) * u);
                        plant.width_m = static_cast<float>(kind.width_min_m + (kind.width_max_m - kind.width_min_m) * Unit(Mix(cx, cy, sj + 4u)));
                        plant.yaw_rad = static_cast<float>(v * 2.0 * kPi);
                        plant.brightness = static_cast<float>(1.0 + (Unit(Mix(cx, cy, sj + 5u)) - 0.5) * 2.0 * kind.brightness);
                        plant.kind = static_cast<std::uint8_t>(entry.kind);
                        out.push_back(plant);
                    }
                }
            }
        }
    }
}

}  // namespace scenery
}  // namespace simcore
