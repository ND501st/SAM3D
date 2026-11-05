// simcore/scenery/clutter.hpp — the ground clutter round the camera (roadmap T6.1 and L5, docs/scenery/TERRAIN_ROADMAP.md):
// tufts of grass, stubble, maize stumps, dry stalks, reeds and clods scattered over the ground within a ring of the
// camera. The ring and the kinds (what a clod or a tuft is) are data/scenery/clutter.json's (LoadClutterRules); where
// they go is the land's recipes' (data/scenery/recipes/, LoadClutterRecipes, simcore/scenery/recipes.hpp): the recipe of
// the layer under each plant (PointLayer: the exact field edge, the crop calendar's day) and the band of its parcel it
// lies in.
//
// Each 4 m cell of the world draws its candidate plants from its own position alone, so the same cell gives the same
// plants every time the ring moves back over it and on every client (docs/MULTIPLAYER.md); each is kept by its distance to
// the ring's centre (its fixed threshold under 1 / (1 + (d / falloff)^2)), so a ring's edge has few plants left and a
// moving ring thins and thickens plants one by one. Presentation only: nothing in the simulation reads it.
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
#include "simcore/scenery/recipes.hpp"

namespace simcore {
namespace scenery {

struct ClutterKind {
    std::string id, part;
    float height_min_m = 0.1f, height_max_m = 0.3f, width_min_m = 0.1f, width_max_m = 0.3f;
    double brightness = 0.0;
    // Lies along the parcel's rows, turned at most this far either way (0..90 degrees); below 0 (the default), any way.
    // Off a field the yaw is any way.
    double along_rows_deg = -1.0;
};

struct ClutterRules {
    double ring_m = 100.0, cell_m = 4.0, falloff_m = 30.0, rebuild_m = 16.0;
    std::vector<ClutterKind> kinds;
    std::vector<std::string> parts;  // the distinct part keys, in first-use order
    RecipeSet recipes;               // where the kinds go (LoadClutterRecipes)
    std::vector<std::string> KindIds() const {
        std::vector<std::string> ids;
        for (const ClutterKind& k : kinds) ids.push_back(k.id);
        return ids;
    }
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

inline double Unit(std::uint64_t h) { return static_cast<double>(h >> 11) / 9007199254740992.0; }

inline bool Range(const json::Value* v, float& lo, float& hi) {
    if (v == nullptr || !v->Is(json::Type::Array) || v->items.size() != 2 || !v->items[0].Is(json::Type::Number) || !v->items[1].Is(json::Type::Number)) return false;
    lo = static_cast<float>(v->items[0].number);
    hi = static_cast<float>(v->items[1].number);
    return lo > 0.0f && hi >= lo && hi <= 4.0f;
}

}  // namespace clutter_detail

// Loads the ring and the kinds from a file path or a JSON text (the recipes after, by LoadClutterRecipes).
inline bool LoadClutterRules(const std::string& path_or_json, ClutterRules& out, std::string* error = nullptr) {
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
    if (!number("ring_m", r.ring_m, 5.0, 500.0) || !number("cell_m", r.cell_m, 1.0, 20.0) || !number("falloff_m", r.falloff_m, 1.0, 500.0) ||
        !number("rebuild_m", r.rebuild_m, 1.0, 100.0)) {
        return false;
    }
    if (root.Find("layers") != nullptr) return Fail(error, "\"layers\" is no longer read: where the kinds go is the recipes' (data/scenery/recipes/)");
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
        if (const json::Value* along = k.Find("along_rows_deg")) {
            if (!along->Is(json::Type::Number) || along->number < 0.0 || along->number > 90.0) return Fail(error, c.id + ": along_rows_deg is not in 0..90");
            c.along_rows_deg = along->number;
        }
        if (r.PartIndex(c.part) < 0) r.parts.push_back(c.part);
        r.kinds.push_back(c);
    }
    out = r;
    return true;
}

// Loads the land's recipes from `dir` (data/scenery/recipes/) into `rules`, their kinds named as in rules.kinds.
inline bool LoadClutterRecipes(const std::string& dir, const GroundLayerRules& layers, ClutterRules& rules, std::string* error = nullptr) {
    RecipeSet set;
    if (!LoadRecipes(dir, layers, rules.KindIds(), set, error)) return false;
    rules.recipes = set;
    return true;
}

// The plants of the ring round (east, north) on the day: the cells whose centres lie within ring_m, each plant a recipe's
// candidate (the recipe of the layer under it, in its band) kept by its threshold under the ring's falloff. Appends to
// `out`.
inline void ClutterAround(const ClutterRules& rules, const CoverRaster& cover, const DressingRules& dressing, const GroundLayerRules& layers,
                          int day_of_year, double east_m, double north_m, std::vector<ClutterPlant>& out) {
    using clutter_detail::Unit;
    const double cell = rules.cell_m;
    const std::int64_t c0 = static_cast<std::int64_t>(std::floor((east_m - rules.ring_m) / cell));
    const std::int64_t c1 = static_cast<std::int64_t>(std::floor((east_m + rules.ring_m) / cell));
    const std::int64_t r0 = static_cast<std::int64_t>(std::floor((north_m - rules.ring_m) / cell));
    const std::int64_t r1 = static_cast<std::int64_t>(std::floor((north_m + rules.ring_m) / cell));
    std::vector<RecipeCandidate> found;
    // A candidate stays inside the ring while its fixed threshold is under the falloff at its distance: the ring moving
    // thins and thickens the plants one by one instead of reshuffling a cell.
    const auto keep = [&](double e, double n, double threshold) {
        const double dp = std::hypot(e - east_m, n - north_m);
        return dp <= rules.ring_m && threshold < 1.0 / (1.0 + (dp / rules.falloff_m) * (dp / rules.falloff_m));
    };
    for (std::int64_t cy = r0; cy <= r1; ++cy) {
        for (std::int64_t cx = c0; cx <= c1; ++cx) {
            const double ce = (static_cast<double>(cx) + 0.5) * cell, cn = (static_cast<double>(cy) + 0.5) * cell;
            if (std::hypot(ce - east_m, cn - north_m) > rules.ring_m + cell) continue;
            found.clear();
            RecipeCandidates(rules.recipes, cover, dressing, layers, day_of_year, cx, cy, cell, keep, found);
            for (const RecipeCandidate& c : found) {
                if (c.kind < 0 || static_cast<std::size_t>(c.kind) >= rules.kinds.size()) continue;
                const ClutterKind& kind = rules.kinds[static_cast<std::size_t>(c.kind)];
                ClutterPlant plant;
                plant.east_m = c.east_m;
                plant.north_m = c.north_m;
                const auto draw = [&](std::uint64_t k) { return Unit(recipe_detail::Mix(c.hash ^ (0xA5A5u + k))); };
                plant.height_m = static_cast<float>(kind.height_min_m + (kind.height_max_m - kind.height_min_m) * draw(1));
                plant.width_m = static_cast<float>(kind.width_min_m + (kind.width_max_m - kind.width_min_m) * draw(2));
                // Unreal turns a part's local x (north) clockwise by the yaw: the heading lays it along the rows; either
                // end first, as a stalk falls.
                if (kind.along_rows_deg >= 0.0 && c.on_field) {
                    const double spread = (draw(3) * 2.0 - 1.0) * kind.along_rows_deg * kPi / 180.0;
                    const double flip = draw(5) < 0.5 ? 0.0 : kPi;
                    plant.yaw_rad = static_cast<float>(std::fmod(c.heading_rad + spread + flip + 4.0 * kPi, 2.0 * kPi));
                } else {
                    plant.yaw_rad = static_cast<float>(draw(3) * 2.0 * kPi);
                }
                plant.brightness = static_cast<float>(1.0 + (draw(4) - 0.5) * 2.0 * kind.brightness);
                plant.kind = static_cast<std::uint8_t>(c.kind);
                out.push_back(plant);
            }
        }
    }
}

}  // namespace scenery
}  // namespace simcore
