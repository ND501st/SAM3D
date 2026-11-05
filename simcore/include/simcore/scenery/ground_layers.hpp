// simcore/scenery/ground_layers.hpp — the ground's surface layers (roadmap T1, docs/scenery/TERRAIN_ROADMAP.md): which
// textured layer covers each cell of the land cover (a field's by its crop's calendar and the day of the year, the rest by
// their cover),
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

// The day of the year the ground is read on when the scenario gives none: 15 October (the sky clock's default).
constexpr int kDefaultGroundDay = 288;

// One layer: a surface the ground material draws from its own textures.
struct GroundLayerDef {
    std::string id;
    double tile_m = 3.0;   // one repeat of its textures on the ground (divides kGroundTilePeriodM)
    bool rows = false;     // its textures turn with the field's furrows (their furrows along the textures' U, across the image)
    std::string texture;   // the texture register's id; empty: not chosen yet (the land cover's colour only)
    double patch = 0.0;    // damp and dry patches 15-40 m across: the share they darken or lighten it by (0..0.5)
    double break_up = 0.0; // a second, shifted sample in blobs about 8 m across, so its repeat does not show (0..1)
    // Procedural furrows drawn by the material on a layer that turns with them: their spacing (0 none, else 0.05..1 m, each
    // field's own within 12%) and their depth crest to trough (0..0.3 m). The scan's own furrows are damped under them.
    double furrow_m = 0.0;
    double furrow_depth_m = 0.0;
};

struct GroundLayerRules {
    std::vector<GroundLayerDef> layers;  // the mask's layer numbers are indices into this
    // The layer under each cover; a field's is its crop's.
    std::array<int, static_cast<std::size_t>(Cover::Count)> cover_layer{};
    // A crop's year (roadmap T4.3): the layer it starts the year with, then the states that follow it, each with a window of
    // days (1..366, in order, not overlapping) in which each field takes the state on a day of its own, drawn from its number:
    // the plain changes field by field as the days pass, the same on every client. By the dressing's crop index.
    struct CropState {
        int layer = 0;
        int from_day = 0, to_day = 0;  // the window (the first state has none: it holds from the year's start)
        // How high the crop stands in this state (0..5 m: what blocks sight, scenery/standing_crops.hpp), reached
        // grow_days after the field's own day (0: at once).
        double stand_m = 0.0;
        int grow_days = 0;
    };
    std::vector<std::vector<CropState>> calendar;

    int Index(const std::string& id) const {
        for (std::size_t i = 0; i < layers.size(); ++i) {
            if (layers[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }
    // The day a field enters a crop's state `state` (1..): inside the state's window, by the field's number.
    int OwnDay(const CropState& s, int state, std::uint64_t field_id) const {
        const std::uint32_t h = Hash32(static_cast<std::uint32_t>(field_id >> 32) ^ (static_cast<std::uint32_t>(state) * 0x85EBCA6Bu));
        return s.from_day + static_cast<int>(static_cast<double>(s.to_day - s.from_day + 1) * Unit24(h));
    }
    // The layer at a cell of `cover` on day `day` of the year (a field's crop `crop`, an index into the dressing's crops, and
    // its number `field_id`, FieldAt's): a field is in the last state whose day of its own has come.
    int LayerOnDay(Cover cover, int crop, std::uint64_t field_id, int day) const {
        if (cover == Cover::Field && crop >= 0 && static_cast<std::size_t>(crop) < calendar.size()) {
            const std::vector<CropState>& states = calendar[static_cast<std::size_t>(crop)];
            int layer = states[0].layer;
            for (std::size_t i = 1; i < states.size(); ++i) {
                if (OwnDay(states[i], static_cast<int>(i), field_id) > day) break;
                layer = states[i].layer;
            }
            return layer;
        }
        return cover_layer[static_cast<std::size_t>(cover)];
    }
    // How high a field of crop `crop` stands on day `day` (0: nothing stands): its state's stand_m, grown over its
    // grow_days from the field's own day.
    double StandOnDay(int crop, std::uint64_t field_id, int day) const {
        if (crop < 0 || static_cast<std::size_t>(crop) >= calendar.size()) return 0.0;
        const std::vector<CropState>& states = calendar[static_cast<std::size_t>(crop)];
        std::size_t at = 0;
        int since = 1;
        for (std::size_t i = 1; i < states.size(); ++i) {
            const int own = OwnDay(states[i], static_cast<int>(i), field_id);
            if (own > day) break;
            at = i;
            since = own;
        }
        const CropState& s = states[at];
        if (s.stand_m <= 0.0) return 0.0;
        if (s.grow_days <= 0) return s.stand_m;
        return s.stand_m * std::min(1.0, std::max(0.0, static_cast<double>(day - since) / static_cast<double>(s.grow_days)));
    }
    // The tallest any crop stands in any state.
    double TallestStand() const {
        double most = 0.0;
        for (const std::vector<CropState>& states : calendar) {
            for (const CropState& s : states) most = std::max(most, s.stand_m);
        }
        return most;
    }
};

namespace ground_detail {

inline bool Fail(std::string* error, const std::string& what) {
    if (error != nullptr) *error = what;
    return false;
}

}  // namespace ground_detail

// Reads the rules from a file's path or the JSON text itself (it starts with '{'). Every cover but a field names a layer,
// and every crop of `dressing` has a calendar of states. False with the reason in `error` (the rules are
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
        if (const json::Value* pa = item.Find("patch")) {
            if (!pa->Is(json::Type::Number) || pa->number < 0.0 || pa->number > 0.5) return Fail(error, d.id + ": patch is not in 0..0.5");
            d.patch = pa->number;
        }
        if (const json::Value* bu = item.Find("break_up")) {
            if (!bu->Is(json::Type::Number) || bu->number < 0.0 || bu->number > 1.0) return Fail(error, d.id + ": break_up is not in 0..1");
            d.break_up = bu->number;
        }
        if (const json::Value* fu = item.Find("furrow_m")) {
            if (!fu->Is(json::Type::Number) || (fu->number != 0.0 && (fu->number < 0.05 || fu->number > 1.0))) {
                return Fail(error, d.id + ": furrow_m is not 0 or in 0.05..1");
            }
            d.furrow_m = fu->number;
        }
        if (const json::Value* fd = item.Find("furrow_depth_m")) {
            if (!fd->Is(json::Type::Number) || fd->number < 0.0 || fd->number > 0.3) return Fail(error, d.id + ": furrow_depth_m is not in 0..0.3");
            d.furrow_depth_m = fd->number;
        }
        if (d.furrow_m > 0.0 && !d.rows) return Fail(error, d.id + ": furrows on a layer that does not turn with the field (rows false)");
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
        const json::Value* states = by_season->Find("states");
        if (states == nullptr || !states->Is(json::Type::Array) || states->items.empty()) return Fail(error, "crop " + c.id + ": no \"states\" list");
        std::vector<GroundLayerRules::CropState> list;
        int last_day = 0;
        for (std::size_t i = 0; i < states->items.size(); ++i) {
            const json::Value& item = states->items[i];
            const std::string what = "crop " + c.id + " state " + std::to_string(i + 1);
            GroundLayerRules::CropState st;
            if (!layer_of(item.Find("layer"), what, st.layer)) return false;
            if (i > 0) {
                const json::Value* from = item.Find("from_day");
                const json::Value* to = item.Find("to_day");
                if (from == nullptr || to == nullptr || !from->Is(json::Type::Number) || !to->Is(json::Type::Number) ||
                    from->number != std::floor(from->number) || to->number != std::floor(to->number)) {
                    return Fail(error, what + " needs a from_day and a to_day, whole numbers");
                }
                st.from_day = static_cast<int>(from->number);
                st.to_day = static_cast<int>(to->number);
                if (st.from_day < 1 || st.to_day > 366 || st.from_day > st.to_day) return Fail(error, what + ": its window is not within 1..366");
                if (st.from_day <= last_day) return Fail(error, what + ": its window overlaps the one before (the states run in order)");
                last_day = st.to_day;
            }
            if (const json::Value* stand = item.Find("stand_m")) {
                if (!stand->Is(json::Type::Number) || stand->number < 0.0 || stand->number > 5.0) return Fail(error, what + ": stand_m is not in 0..5");
                st.stand_m = stand->number;
            }
            if (const json::Value* grow = item.Find("grow_days")) {
                if (!grow->Is(json::Type::Number) || grow->number != std::floor(grow->number) || grow->number < 0.0 || grow->number > 200.0 || st.stand_m <= 0.0) {
                    return Fail(error, what + ": grow_days is a whole number of days in 0..200, for a state that stands");
                }
                st.grow_days = static_cast<int>(grow->number);
            }
            list.push_back(st);
        }
        r.calendar.push_back(list);
    }
    // Field cells take the first crop's first layer when the raster has none, so the field cover's own slot is that.
    r.cover_layer[static_cast<std::size_t>(Cover::Field)] = r.calendar.empty() ? 0 : r.calendar[0][0].layer;
    for (const std::string& key : crops->keys) {
        bool known = false;
        for (const CropDef& c : dressing.crops) known = known || key == c.id;
        if (!known) return Fail(error, "crops: the dressing has no crop \"" + key + "\"");
    }
    out = r;
    return true;
}

// The mask over one square of the land cover: a texel per cell, row 0 the southernmost, column 0 the westernmost, each
// RGBA: R the layer's number, G the furrows' heading (0..255 over 0..pi, clockwise from north; FieldHeading), B the
// field's own shade in its low 7 bits (a draw per parcel, 64 off the fields) and 128 added where the field is household
// strips (CoverGrid::strips), A 255 on farmland where the layer turns with the furrows, 128 on farmland where it does not,
// 0 off the fields (the ground material finds a field's own cells by it: roadmap T3.1).
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

inline GroundMask BakeGroundMask(const CoverGrid& g, const DressingRules& /*dressing*/, const GroundLayerRules& layers, int day_of_year) {
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
            const FieldCell f = cover == Cover::Field ? FieldAt(g.fields, g.CentreEast(c), g.CentreNorth(r)) : FieldCell{};
            const int layer = layers.LayerOnDay(cover, g.crop[i], f.id, day_of_year);
            const bool rows = layer >= 0 && static_cast<std::size_t>(layer) < layers.layers.size() && layers.layers[static_cast<std::size_t>(layer)].rows;
            std::uint8_t* t = &m.texels[i * 4];
            t[0] = static_cast<std::uint8_t>(layer < 0 ? 0 : layer);
            t[1] = 0;
            t[2] = 64;
            t[3] = 0;
            if (cover == Cover::Field) {
                t[1] = HeadingTexel(f.heading_rad);
                t[2] = static_cast<std::uint8_t>(((f.id >> 41) & 0x7Fu) | (g.strips[i] != 0 ? 0x80u : 0u));
                t[3] = rows ? 255 : 128;
            }
        }
    }
    return m;
}

// The layer at one point of the map (the layer BakeGroundMask gives the cell, found at the exact point: a field's own
// edges are straight, not the cell's 25 m), on the day; -1 outside every land cover square.
inline int PointLayer(const CoverRaster& cover, const DressingRules& dressing, const GroundLayerRules& layers, double east_m, double north_m,
                      int day_of_year) {
    const CoverGrid* g = cover.GridAt(east_m, north_m);
    if (g == nullptr) return -1;
    const std::size_t i = g->IndexAt(east_m, north_m);
    const Cover c = static_cast<Cover>(g->cover[i]);
    if (c != Cover::Field) return layers.LayerOnDay(c, g->crop[i], 0, day_of_year);
    const FieldCell f = FieldAt(g->fields, east_m, north_m);
    return layers.LayerOnDay(c, CropOf(dressing, f.id), f.id, day_of_year);
}

}  // namespace scenery
}  // namespace simcore
