// simcore/scenery/ground_layers.hpp — the ground's surface layers (roadmap T1, docs/scenery/TERRAIN_ROADMAP.md): which
// textured layer covers each cell of the land cover (a field's by its crop and the season, the rest by their cover),
// the way a field's furrows run, and a number per field for its own shade, baked into a mask Unreal's ground material
// reads (ASimMapActor). Rules from data/scenery/ground_layers.json (LoadGroundLayers).
//
// Presentation only: nothing in the simulation reads it. A function of the land cover and the rules alone, so every
// client bakes the same mask (docs/MULTIPLAYER.md).
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/json.hpp"
#include "simcore/scenery/map_dressing.hpp"

namespace simcore {
namespace scenery {

// The ground material blends at most this many layers (its texture arrays' slices; its per-layer tiling in four vectors).
constexpr int kMaxGroundLayers = 16;

// The ground material lays its textures from the world's position cut into squares this wide (exact in double before
// the GPU's floats, so the textures neither swim nor block up far from the scenario's origin); a layer's repeat divides
// it, so its textures run on unbroken from one square into the next.
constexpr double kGroundTilePeriodM = 600.0;

enum class Season : std::uint8_t { Summer, Autumn };

inline const char* SeasonName(Season s) { return s == Season::Autumn ? "autumn" : "summer"; }

// One layer: a surface the ground material draws from its own textures.
struct GroundLayerDef {
    std::string id;
    double tile_m = 3.0;   // one repeat of its textures on the ground (divides kGroundTilePeriodM)
    bool rows = false;     // its textures turn with the field's furrows (their furrows along the textures' U, across the image)
    std::string texture;   // the texture register's id; empty: not chosen yet (the land cover's colour only)
};

struct GroundLayerRules {
    std::vector<GroundLayerDef> layers;  // the mask's layer numbers are indices into this
    // The layer under each cover; a field's is its crop's.
    std::array<int, static_cast<std::size_t>(Cover::Count)> cover_layer{};
    std::vector<int> crop_summer, crop_autumn;  // by the dressing's crop index
    std::vector<int> autumn_months{9, 10, 11};

    int Index(const std::string& id) const {
        for (std::size_t i = 0; i < layers.size(); ++i) {
            if (layers[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }
    Season SeasonOfMonth(int month) const {
        for (int m : autumn_months) {
            if (m == month) return Season::Autumn;
        }
        return Season::Summer;
    }
    // The layer at a cell of `cover` (a field's crop `crop`, an index into the dressing's crops) in season `s`.
    int LayerAt(Cover cover, int crop, Season s) const {
        if (cover == Cover::Field) {
            const std::vector<int>& by_crop = s == Season::Autumn ? crop_autumn : crop_summer;
            if (crop >= 0 && static_cast<std::size_t>(crop) < by_crop.size()) return by_crop[static_cast<std::size_t>(crop)];
        }
        return cover_layer[static_cast<std::size_t>(cover)];
    }
};

namespace ground_detail {

inline bool Fail(std::string* error, const std::string& what) {
    if (error != nullptr) *error = what;
    return false;
}

}  // namespace ground_detail

// Reads the rules from a file's path or the JSON text itself (it starts with '{'). Every cover but a field names a layer,
// and every crop of `dressing` names one for summer and one for autumn. False with the reason in `error` (the rules are
// left as they were).
inline bool LoadGroundLayers(const std::string& path_or_json, const DressingRules& dressing, GroundLayerRules& out,
                             std::string* error = nullptr) {
    using ground_detail::Fail;
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
    GroundLayerRules r;
    const json::Value* layers = root.Find("layers");
    if (layers == nullptr || !layers->Is(json::Type::Array) || layers->items.empty()) return Fail(error, "no \"layers\" list");
    if (layers->items.size() > static_cast<std::size_t>(kMaxGroundLayers)) {
        return Fail(error, "more than " + std::to_string(kMaxGroundLayers) + " layers");
    }
    for (const json::Value& item : layers->items) {
        const json::Value* id = item.Find("id");
        if (id == nullptr || !id->Is(json::Type::String) || id->string.empty()) return Fail(error, "a layer without an id");
        GroundLayerDef d;
        d.id = id->string;
        if (r.Index(d.id) >= 0) return Fail(error, "the layer \"" + d.id + "\" twice");
        if (const json::Value* t = item.Find("tile_m")) {
            if (!t->Is(json::Type::Number) || t->number < 0.25 || t->number > 64.0) return Fail(error, d.id + ": tile_m is not in 0.25..64");
            const double repeats = kGroundTilePeriodM / t->number;
            if (std::fabs(repeats - std::round(repeats)) > 1e-6) {
                return Fail(error, d.id + ": tile_m does not divide " + std::to_string(static_cast<int>(kGroundTilePeriodM)) + " m");
            }
            d.tile_m = t->number;
        }
        if (const json::Value* rows = item.Find("rows")) {
            if (!rows->Is(json::Type::Bool)) return Fail(error, d.id + ": rows is not true or false");
            d.rows = rows->boolean;
        }
        if (const json::Value* tex = item.Find("texture")) {
            if (!tex->Is(json::Type::String)) return Fail(error, d.id + ": texture is not a string");
            d.texture = tex->string;
        }
        r.layers.push_back(d);
    }
    const auto layer_of = [&](const json::Value* v, const std::string& what, int& to) {
        if (v == nullptr || !v->Is(json::Type::String)) return Fail(error, what + " names no layer");
        to = r.Index(v->string);
        if (to < 0) return Fail(error, what + ": no layer \"" + v->string + "\"");
        return true;
    };
    const json::Value* cover = root.Find("cover");
    if (cover == nullptr || !cover->Is(json::Type::Object)) return Fail(error, "no \"cover\" object");
    for (int k = 1; k < static_cast<int>(Cover::Count); ++k) {
        const char* name = CoverName(static_cast<Cover>(k));
        if (!layer_of(cover->Find(name), std::string("cover ") + name, r.cover_layer[static_cast<std::size_t>(k)])) return false;
    }
    for (const std::string& key : cover->keys) {
        bool known = false;
        for (int k = 1; k < static_cast<int>(Cover::Count); ++k) known = known || key == CoverName(static_cast<Cover>(k));
        if (!known) return Fail(error, "cover: no cover \"" + key + "\" (a field's layer is its crop's)");
    }
    const json::Value* crops = root.Find("crops");
    if (crops == nullptr || !crops->Is(json::Type::Object)) return Fail(error, "no \"crops\" object");
    for (const CropDef& c : dressing.crops) {
        const json::Value* by_season = crops->Find(c.id);
        if (by_season == nullptr || !by_season->Is(json::Type::Object)) return Fail(error, "crops: no layers for the crop \"" + c.id + "\"");
        int summer = -1, autumn = -1;
        if (!layer_of(by_season->Find("summer"), "crop " + c.id + " in summer", summer)) return false;
        if (!layer_of(by_season->Find("autumn"), "crop " + c.id + " in autumn", autumn)) return false;
        r.crop_summer.push_back(summer);
        r.crop_autumn.push_back(autumn);
    }
    // Field cells take the first crop's layer when the raster has none, so the field cover's own slot is that.
    r.cover_layer[static_cast<std::size_t>(Cover::Field)] = r.crop_summer.empty() ? 0 : r.crop_summer[0];
    for (const std::string& key : crops->keys) {
        bool known = false;
        for (const CropDef& c : dressing.crops) known = known || key == c.id;
        if (!known) return Fail(error, "crops: the dressing has no crop \"" + key + "\"");
    }
    if (const json::Value* months = root.Find("autumn_months")) {
        if (!months->Is(json::Type::Array)) return Fail(error, "\"autumn_months\" is not a list");
        r.autumn_months.clear();
        for (const json::Value& m : months->items) {
            if (!m.Is(json::Type::Number) || m.number < 1.0 || m.number > 12.0 || m.number != std::floor(m.number)) {
                return Fail(error, "autumn_months: a month is a whole number 1..12");
            }
            r.autumn_months.push_back(static_cast<int>(m.number));
        }
    }
    out = r;
    return true;
}

// The mask over one square of the land cover: a texel per cell, row 0 the southernmost, column 0 the westernmost, each
// RGBA: R the layer's number, G the furrows' heading (0..255 over 0..pi, clockwise from north; FieldHeading), B the
// field's own shade in its low 7 bits (a draw per parcel, 64 off the fields) and 128 added where the field is household
// strips (CoverGrid::strips), A 255 where the layer turns with the furrows, else 0.
struct GroundMask {
    std::string id;  // the cover grid's
    double west_m = 0.0, south_m = 0.0, cell_m = 25.0;
    int cols = 0, rows = 0;
    std::vector<std::uint8_t> texels;  // cols * rows * 4

    const std::uint8_t* At(int c, int r) const {
        return &texels[(static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(c)) * 4];
    }
};

// The furrows' heading as the mask's G: 0..255 over [0, pi). A heading a hair under pi rounds to 0, the same furrows.
inline std::uint8_t HeadingTexel(double heading_rad) {
    const double h = std::fmod(std::fmod(heading_rad, kPi) + kPi, kPi);
    return static_cast<std::uint8_t>(std::lround(h / kPi * 256.0) & 255L);
}

inline GroundMask BakeGroundMask(const CoverGrid& g, const DressingRules& /*dressing*/, const GroundLayerRules& layers, Season season) {
    GroundMask m;
    m.id = g.id;
    m.west_m = g.west_m;
    m.south_m = g.south_m;
    m.cell_m = g.cell_m;
    m.cols = g.cols;
    m.rows = g.rows;
    m.texels.assign(static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows) * 4, 0);
    for (int r = 0; r < g.rows; ++r) {
        for (int c = 0; c < g.cols; ++c) {
            const std::size_t i = g.Index(c, r);
            const Cover cover = static_cast<Cover>(g.cover[i]);
            const int layer = layers.LayerAt(cover, g.crop[i], season);
            const bool rows = layer >= 0 && static_cast<std::size_t>(layer) < layers.layers.size() && layers.layers[static_cast<std::size_t>(layer)].rows;
            std::uint8_t* t = &m.texels[i * 4];
            t[0] = static_cast<std::uint8_t>(layer < 0 ? 0 : layer);
            t[1] = 0;
            t[2] = 64;
            t[3] = 0;
            if (cover == Cover::Field) {
                const FieldCell f = FieldAt(g.fields, g.CentreEast(c), g.CentreNorth(r));
                t[1] = HeadingTexel(f.heading_rad);
                t[2] = static_cast<std::uint8_t>(((f.id >> 41) & 0x7Fu) | (g.strips[i] != 0 ? 0x80u : 0u));
                t[3] = rows ? 255 : 0;
            }
        }
    }
    return m;
}

}  // namespace scenery
}  // namespace simcore
