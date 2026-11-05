// simcore/scenery/map_dressing.hpp — the map's dressing (roadmap R6, docs/scenery/README.md "The map's dressing"): the
// land cover as a raster (fields in parcels with their crops, woods, marsh, towns and gardens, military ground, roads,
// water) that the ground is coloured from, the trees planted in the map's woods, the masses woods are drawn as where
// single trees are not, and a building's variant. Rules from data/scenery/map_dressing.json (LoadDressingRules).
//
// Presentation only: nothing in the simulation reads it. The sight lines read the map's obstacle heights
// (MapData::ObstacleHeight), and no tree planted here ever stands above them: a tree's top is the wood's top less a
// little, never above the ground plus the obstacle height where it stands (PlaceTrees). Trees stand only in the map
// proper and the detail areas: a wood's block inside a detail area is planted whole; elsewhere in the map proper its open
// sides get rows of trees and its inside is a mass; the far ground's woods are masses only.
//
// Everything is a function of the map and the rules alone, in the layout's order (no unordered container's order is
// read), so every client builds the same dressing (docs/MULTIPLAYER.md).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "simcore/farm_fields.hpp"
#include "simcore/json.hpp"
#include "simcore/noise.hpp"
#include "simcore/scenery/map_meshes.hpp"
#include "simcore/terrain_map.hpp"

namespace simcore {
namespace scenery {

// What covers a cell of the ground.
enum class Cover : std::uint8_t { Field, Forest, Marsh, Town, Military, Road, Water, Count };

inline const char* CoverName(Cover c) {
    switch (c) {
    case Cover::Field: return "field";
    case Cover::Forest: return "forest";
    case Cover::Marsh: return "marsh";
    case Cover::Town: return "town";
    case Cover::Military: return "military";
    case Cover::Road: return "road";
    case Cover::Water: return "water";
    case Cover::Count: break;
    }
    return "?";
}

using CoverColour = std::array<float, 3>;  // linear RGB

struct CropDef {
    std::string id;
    double share = 0.0;
    CoverColour colour{{0.3f, 0.3f, 0.15f}};
};

// A kind of stand: where it grows, how often, whether it is planted in rows, and its trees' mix by kind.
struct StandType {
    std::string id;
    bool wet = false;
    double weight = 1.0;
    bool plantation = false;
    std::array<double, static_cast<std::size_t>(TreeKind::Count)> mix{};
};

// The rules (data/scenery/map_dressing.json); the defaults are the file's values.
struct DressingRules {
    double near_spacing_m = 7.0;  // a wood planted whole (in a detail area): a tree every this far each way
    double edge_spacing_m = 8.0;  // the rows along a wood's open sides, and the trees along them
    int edge_rows = 2;
    double jitter = 0.35;        // of the spacing, each way
    double line_clear_m = 4.0;   // no tree this close to a road's, railway's or river's edge
    double crown_ratio = 0.5;    // the crown's width to the tree's height
    double crown_jitter = 0.2;
    double top_drop = 0.15;      // a tree's top up to this share of the wood's height below the wood's top
    double poplar_river_m = 60.0;
    std::array<double, static_cast<std::size_t>(TreeKind::Count)> species_shares{{0.55, 0.1, 0.15, 0.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    // Stands (docs/scenery/VEGETATION.md): a wood is cut into compartments on a stand_m grid, each of one stand type drawn
    // by its site (wet: within wet_m of water; dry elsewhere) and of one age (its trees' height times a factor in
    // age_min..1). A plantation's trees stand in rows (jitter times plantation_jitter). With no stand types the species
    // shares are used for every tree, as before.
    std::vector<StandType> stands;
    double stand_m = 150.0;
    double wet_m = 80.0;
    double age_min = 0.75;
    double plantation_jitter = 0.15;
    // A wood's open edge: the trees within edge_band_m of an open side are its mantle (edge_mix, edge_height of the
    // stand's), and a fringe of shrubs (shrub_mix) stands shrub_inset_m inside it every shrub_spacing_m, keep_share of them.
    double edge_band_m = 12.0;
    double edge_height = 0.8;
    std::array<double, static_cast<std::size_t>(TreeKind::Count)> edge_mix{{0.4, 0.0, 0.0, 0.25, 0.15, 0.0, 0.0, 0.2, 0.0, 0.0}};
    std::array<double, static_cast<std::size_t>(TreeKind::Count)> shrub_mix{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.6, 0.4}};
    double shrub_spacing_m = 3.0;
    double shrub_inset_m = 1.5;
    double shrub_keep = 0.75;
    double shrub_height_min_m = 1.5, shrub_height_max_m = 4.0;
    double mass_inset_m = 3.0;
    double mass_top = 0.92;      // a mass's height, of its wood's
    double cover_cell_m = 25.0;
    double garden_m = 15.0;
    // The fields themselves are the map's (MapDef::farmland, farm_fields.hpp: they set the farm tracks and tree rows, which
    // block sight); a field within strip_reach_m of a settlement's edge is cut into household strips.
    double strip_width_min_m = 20.0, strip_width_max_m = 40.0;
    double strip_reach_m = 600.0;
    std::vector<CropDef> crops{{"wheat", 0.3, {{0.36f, 0.33f, 0.14f}}},
                               {"maize", 0.25, {{0.17f, 0.25f, 0.08f}}},
                               {"sunflower", 0.1, {{0.3f, 0.29f, 0.09f}}},
                               {"ploughed", 0.15, {{0.24f, 0.18f, 0.12f}}},
                               {"pasture", 0.2, {{0.2f, 0.27f, 0.1f}}}};
    // By Cover (a field's colour is its crop's).
    std::array<CoverColour, static_cast<std::size_t>(Cover::Count)> cover_colours{{{{0.26f, 0.28f, 0.12f}},
                                                                                  {{0.07f, 0.13f, 0.05f}},
                                                                                  {{0.16f, 0.2f, 0.12f}},
                                                                                  {{0.26f, 0.25f, 0.2f}},
                                                                                  {{0.3f, 0.27f, 0.19f}},
                                                                                  {{0.22f, 0.22f, 0.21f}},
                                                                                  {{0.05f, 0.11f, 0.15f}}}};
    int house_variants = kHouseVariants;
};

namespace dressing_detail {

inline bool Fail(std::string* error, const std::string& what) {
    if (error != nullptr) *error = "map_dressing: " + what;
    return false;
}

// A number in [0, 1) from a hash.
inline double Unit(std::uint64_t h) { return static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0); }

inline std::uint64_t Bits(long long v) { return static_cast<std::uint64_t>(v); }

// A point's own number in [0, 1) for field `seed` (decimetres: two points a decimetre apart differ).
inline double At(std::uint64_t seed, double e, double n) {
    return noise::Lattice(seed, static_cast<std::int64_t>(std::llround(e * 10.0)), static_cast<std::int64_t>(std::llround(n * 10.0)));
}

inline bool Colour(const json::Value& v, CoverColour& out) {
    if (!v.Is(json::Type::Array) || v.items.size() != 3) return false;
    for (std::size_t i = 0; i < 3; ++i) {
        if (!v.items[i].Is(json::Type::Number) || v.items[i].number < 0.0 || v.items[i].number > 1.0) return false;
        out[i] = static_cast<float>(v.items[i].number);
    }
    return true;
}

}  // namespace dressing_detail

// Reads the rules from a file's path or the JSON text itself (it starts with '{'): each value as {"value": ..,
// "source": .., "confidence": ..} under "values"; a value left out keeps its default. False with the reason in `error`
// (the rules are left as they were).
inline bool LoadDressingRules(const std::string& path_or_json, DressingRules& out, std::string* error = nullptr) {
    using dressing_detail::Fail;
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
    const json::Value* values = root.Find("values");
    if (values == nullptr || !values->Is(json::Type::Object)) return Fail(error, "no \"values\" object");
    DressingRules r = out;
    const auto value = [&](const char* key) -> const json::Value* {
        const json::Value* v = values->Find(key);
        if (v == nullptr) return nullptr;
        if (!v->Is(json::Type::Object)) return v;  // a bare value
        return v->Find("value");
    };
    bool ok = true;
    std::string bad;
    const auto number = [&](const char* key, double& to, double lo, double hi) {
        const json::Value* v = value(key);
        if (v == nullptr) return;
        if (!v->Is(json::Type::Number) || v->number < lo || v->number > hi) {
            ok = false;
            bad = key;
            return;
        }
        to = v->number;
    };
    const auto range = [&](const char* key, double& lo_out, double& hi_out, double lo, double hi) {
        const json::Value* v = value(key);
        if (v == nullptr) return;
        if (!v->Is(json::Type::Array) || v->items.size() != 2 || !v->items[0].Is(json::Type::Number) || !v->items[1].Is(json::Type::Number) ||
            v->items[0].number < lo || v->items[1].number > hi || v->items[0].number > v->items[1].number) {
            ok = false;
            bad = key;
            return;
        }
        lo_out = v->items[0].number;
        hi_out = v->items[1].number;
    };
    number("near_spacing_m", r.near_spacing_m, 2.0, 50.0);
    number("edge_spacing_m", r.edge_spacing_m, 2.0, 50.0);
    double rows = r.edge_rows;
    number("edge_rows", rows, 0.0, 6.0);
    r.edge_rows = static_cast<int>(std::lround(rows));
    number("jitter", r.jitter, 0.0, 0.5);
    number("line_clear_m", r.line_clear_m, 0.0, 50.0);
    number("crown_ratio", r.crown_ratio, 0.1, 1.5);
    number("crown_jitter", r.crown_jitter, 0.0, 0.9);
    number("top_drop", r.top_drop, 0.0, 0.5);
    number("poplar_river_m", r.poplar_river_m, 0.0, 120.0);
    number("mass_inset_m", r.mass_inset_m, 0.0, 20.0);
    number("mass_top", r.mass_top, 0.2, 1.0);
    number("cover_cell_m", r.cover_cell_m, 5.0, 100.0);
    number("garden_m", r.garden_m, 0.0, 60.0);
    range("strip_width_m", r.strip_width_min_m, r.strip_width_max_m, 5.0, 200.0);
    number("strip_reach_m", r.strip_reach_m, 0.0, 5000.0);
    double variants = r.house_variants;
    number("house_variants", variants, 1.0, static_cast<double>(kHouseVariants));
    r.house_variants = static_cast<int>(std::lround(variants));
    number("stand_m", r.stand_m, 20.0, 2000.0);
    number("wet_m", r.wet_m, 0.0, 500.0);
    number("age_min", r.age_min, 0.3, 1.0);
    number("plantation_jitter", r.plantation_jitter, 0.0, 1.0);
    number("edge_band_m", r.edge_band_m, 0.0, 60.0);
    number("edge_height", r.edge_height, 0.3, 1.0);
    number("shrub_spacing_m", r.shrub_spacing_m, 1.0, 50.0);
    number("shrub_inset_m", r.shrub_inset_m, 0.0, 20.0);
    number("shrub_keep", r.shrub_keep, 0.0, 1.0);
    range("shrub_height_m", r.shrub_height_min_m, r.shrub_height_max_m, 0.5, 8.0);
    if (!ok) return Fail(error, "\"" + bad + "\" is missing its number or out of range");
    // A mix by kind: {"oak": 0.5, ...}, kinds named, shares not negative, summing to more than nothing.
    const auto mix = [&](const json::Value& s, std::array<double, static_cast<std::size_t>(TreeKind::Count)>& to, const std::string& what) {
        if (!s.Is(json::Type::Object)) return Fail(error, what + " is not an object of kinds");
        std::array<double, static_cast<std::size_t>(TreeKind::Count)> m{};
        double sum = 0.0;
        for (std::size_t i = 0; i < s.keys.size(); ++i) {
            int found = -1;
            for (int k = 0; k < static_cast<int>(TreeKind::Count); ++k) found = s.keys[i] == TreeKindName(static_cast<TreeKind>(k)) ? k : found;
            if (found < 0) return Fail(error, what + ": no tree kind \"" + s.keys[i] + "\"");
            if (!s.items[i].Is(json::Type::Number) || s.items[i].number < 0.0) return Fail(error, what + ": a share is not a number of 0 or more");
            m[static_cast<std::size_t>(found)] = s.items[i].number;
            sum += s.items[i].number;
        }
        if (sum <= 0.0) return Fail(error, what + ": the shares add up to nothing");
        to = m;
        return true;
    };
    if (const json::Value* s = value("edge_mix")) {
        if (!mix(*s, r.edge_mix, "edge_mix")) return false;
    }
    if (const json::Value* s = value("shrub_mix")) {
        if (!mix(*s, r.shrub_mix, "shrub_mix")) return false;
        for (std::size_t k = 0; k < r.shrub_mix.size(); ++k) {
            if (r.shrub_mix[k] > 0.0 && !IsShrub(static_cast<TreeKind>(k))) return Fail(error, "shrub_mix: a tree is not a shrub");
        }
    }
    if (const json::Value* s = value("stands")) {
        if (!s->Is(json::Type::Array)) return Fail(error, "\"stands\" is not a list");
        std::vector<StandType> stands;
        for (const json::Value& item : s->items) {
            const json::Value* id = item.Find("id");
            const json::Value* site = item.Find("site");
            const json::Value* weight = item.Find("weight");
            const json::Value* m = item.Find("mix");
            if (id == nullptr || !id->Is(json::Type::String) || site == nullptr || !site->Is(json::Type::String) ||
                (site->string != "wet" && site->string != "dry") || weight == nullptr || !weight->Is(json::Type::Number) || weight->number <= 0.0 ||
                m == nullptr) {
                return Fail(error, "a stand needs an id, a site (wet or dry), a weight above 0 and a mix");
            }
            StandType t;
            t.id = id->string;
            t.wet = site->string == "wet";
            t.weight = weight->number;
            const json::Value* plantation = item.Find("plantation");
            t.plantation = plantation != nullptr && plantation->Is(json::Type::Bool) && plantation->boolean;
            if (!mix(*m, t.mix, "stand " + t.id)) return false;
            for (std::size_t k = 0; k < t.mix.size(); ++k) {
                if (t.mix[k] > 0.0 && IsShrub(static_cast<TreeKind>(k))) return Fail(error, "stand " + t.id + ": a shrub is not a stand's tree");
            }
            stands.push_back(t);
        }
        bool wet = false, dry = false;
        for (const StandType& t : stands) (t.wet ? wet : dry) = true;
        if (!stands.empty() && (!wet || !dry)) return Fail(error, "stands: at least one wet and one dry stand type");
        r.stands = stands;
    }
    if (const json::Value* s = value("species_shares")) {
        if (!s->Is(json::Type::Object)) return Fail(error, "\"species_shares\" is not an object");
        double sum = 0.0;
        for (std::size_t k = 0; k < static_cast<std::size_t>(TreeKind::Count); ++k) {
            const json::Value* v = s->Find(TreeKindName(static_cast<TreeKind>(k)));
            const double share = v != nullptr && v->Is(json::Type::Number) ? v->number : 0.0;
            if (share < 0.0) return Fail(error, "a negative species share");
            r.species_shares[k] = share;
            sum += share;
        }
        for (std::size_t i = 0; i < s->keys.size(); ++i) {
            bool known = false;
            for (int k = 0; k < static_cast<int>(TreeKind::Count); ++k) known = known || s->keys[i] == TreeKindName(static_cast<TreeKind>(k));
            if (!known) return Fail(error, "species_shares: no tree kind \"" + s->keys[i] + "\"");
        }
        if (sum <= 0.0) return Fail(error, "the species shares add up to nothing");
    }
    if (const json::Value* c = value("crops")) {
        if (!c->Is(json::Type::Array) || c->items.empty()) return Fail(error, "\"crops\" is not a list of crops");
        std::vector<CropDef> crops;
        double sum = 0.0;
        for (const json::Value& item : c->items) {
            const json::Value* id = item.Find("id");
            const json::Value* share = item.Find("share");
            const json::Value* colour = item.Find("colour");
            CropDef d;
            if (id == nullptr || !id->Is(json::Type::String) || share == nullptr || !share->Is(json::Type::Number) || share->number < 0.0 ||
                colour == nullptr || !dressing_detail::Colour(*colour, d.colour)) {
                return Fail(error, "a crop needs an id, a share and a linear colour in 0..1");
            }
            d.id = id->string;
            d.share = share->number;
            sum += d.share;
            crops.push_back(d);
        }
        if (crops.size() > 250) return Fail(error, "too many crops");
        if (sum <= 0.0) return Fail(error, "the crops' shares add up to nothing");
        r.crops = crops;
    }
    if (const json::Value* c = value("cover_colours")) {
        if (!c->Is(json::Type::Object)) return Fail(error, "\"cover_colours\" is not an object");
        for (std::size_t i = 0; i < c->keys.size(); ++i) {
            int found = -1;
            for (int k = 1; k < static_cast<int>(Cover::Count); ++k) found = c->keys[i] == CoverName(static_cast<Cover>(k)) ? k : found;
            if (found < 0) return Fail(error, "cover_colours: no cover \"" + c->keys[i] + "\" (a field's colour is its crop's)");
            if (!dressing_detail::Colour(c->items[i], r.cover_colours[static_cast<std::size_t>(found)])) {
                return Fail(error, "cover_colours: " + c->keys[i] + " is not a linear colour in 0..1");
            }
        }
    }
    out = r;
    return true;
}

// ---------------------------------------------------------------------------------------------- the land cover

// The crop a parcel grows: its index in the rules' crops, by their shares.
inline std::uint8_t CropOf(const DressingRules& r, std::uint64_t parcel) {
    double sum = 0.0;
    for (const CropDef& c : r.crops) sum += c.share;
    const double pick = dressing_detail::Unit(noise::Mix64(parcel ^ 0xC40Full)) * sum;
    double run = 0.0;
    for (std::size_t i = 0; i < r.crops.size(); ++i) {
        run += r.crops[i].share;
        if (pick < run) return static_cast<std::uint8_t>(i);
    }
    return static_cast<std::uint8_t>(r.crops.empty() ? 0 : r.crops.size() - 1);
}

// One square of the land cover: cells `cell_m` across from (west, south), each its cover and, on a field, its crop.
struct CoverGrid {
    std::string id;  // the detail area's, or "map" for the map proper
    FieldGrid fields;  // the map's fields (FieldAt)
    double west_m = 0.0, south_m = 0.0, cell_m = 25.0;
    int cols = 0, rows = 0;
    std::vector<std::uint8_t> cover;  // Cover
    std::vector<std::uint8_t> crop;   // the crop's index on a field (also kept under whatever covers it)
    std::vector<std::uint8_t> strips; // 1 where the field is household strips (FieldAt's field near a settlement)

    bool Covers(double e, double n) const {
        return cols > 0 && rows > 0 && e >= west_m && n >= south_m && e <= west_m + cell_m * cols && n <= south_m + cell_m * rows;
    }
    std::size_t Index(int c, int r) const { return static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(c); }
    std::size_t IndexAt(double e, double n) const {
        const int c = std::min(cols - 1, std::max(0, static_cast<int>(std::floor((e - west_m) / cell_m))));
        const int r = std::min(rows - 1, std::max(0, static_cast<int>(std::floor((n - south_m) / cell_m))));
        return Index(c, r);
    }
    double CentreEast(int c) const { return west_m + cell_m * (c + 0.5); }
    double CentreNorth(int r) const { return south_m + cell_m * (r + 0.5); }
};

// The land cover over the detail areas (first, in the map's order) and the map proper.
struct CoverRaster {
    std::vector<CoverGrid> grids;

    const CoverGrid* GridAt(double e, double n) const {
        for (const CoverGrid& g : grids) {
            if (g.Covers(e, n)) return &g;
        }
        return nullptr;
    }
    bool Has(double e, double n) const { return GridAt(e, n) != nullptr; }
    // The cover at (east, north): a field where the raster has none.
    Cover At(double e, double n) const {
        const CoverGrid* g = GridAt(e, n);
        return g != nullptr ? static_cast<Cover>(g->cover[g->IndexAt(e, n)]) : Cover::Field;
    }
    int CropAt(double e, double n) const {
        const CoverGrid* g = GridAt(e, n);
        return g != nullptr ? g->crop[g->IndexAt(e, n)] : -1;
    }
    // The ground's colour there (linear RGB): its cover's, a field's its crop's.
    CoverColour ColourAt(const DressingRules& r, double e, double n) const {
        const Cover c = At(e, n);
        if (c == Cover::Field) {
            const int crop = CropAt(e, n);
            if (crop >= 0 && static_cast<std::size_t>(crop) < r.crops.size()) return r.crops[static_cast<std::size_t>(crop)].colour;
        }
        return r.cover_colours[static_cast<std::size_t>(c)];
    }
};

namespace dressing_detail {

// Every cell of `g` whose centre lies in the box (centre, length along heading, width across) grown by `margin_m`.
inline void StampRect(CoverGrid& g, double e, double n, double length, double width, double heading, double margin_m, Cover value) {
    const double reach = 0.5 * std::hypot(length, width) + margin_m;
    if (e + reach < g.west_m || n + reach < g.south_m || e - reach > g.west_m + g.cell_m * g.cols || n - reach > g.south_m + g.cell_m * g.rows) return;
    const int c0 = std::max(0, static_cast<int>(std::floor((e - reach - g.west_m) / g.cell_m)));
    const int c1 = std::min(g.cols - 1, static_cast<int>(std::floor((e + reach - g.west_m) / g.cell_m)));
    const int r0 = std::max(0, static_cast<int>(std::floor((n - reach - g.south_m) / g.cell_m)));
    const int r1 = std::min(g.rows - 1, static_cast<int>(std::floor((n + reach - g.south_m) / g.cell_m)));
    const double sh = std::sin(heading), ch = std::cos(heading);
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            const double de = g.CentreEast(c) - e, dn = g.CentreNorth(r) - n;
            const double along = de * sh + dn * ch, across = de * ch - dn * sh;
            if (std::fabs(along) > 0.5 * length + margin_m || std::fabs(across) > 0.5 * width + margin_m) continue;
            g.cover[g.Index(c, r)] = static_cast<std::uint8_t>(value);
        }
    }
}

inline void StampDisc(CoverGrid& g, double e, double n, double radius, Cover value) {
    if (radius <= 0.0) return;
    const int c0 = std::max(0, static_cast<int>(std::floor((e - radius - g.west_m) / g.cell_m)));
    const int c1 = std::min(g.cols - 1, static_cast<int>(std::floor((e + radius - g.west_m) / g.cell_m)));
    const int r0 = std::max(0, static_cast<int>(std::floor((n - radius - g.south_m) / g.cell_m)));
    const int r1 = std::min(g.rows - 1, static_cast<int>(std::floor((n + radius - g.south_m) / g.cell_m)));
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            if (std::hypot(g.CentreEast(c) - e, g.CentreNorth(r) - n) <= radius) g.cover[g.Index(c, r)] = static_cast<std::uint8_t>(value);
        }
    }
}

// Every cell whose centre lies within `half_m` of the segment a-b.
inline void StampSegment(CoverGrid& g, double ae, double an, double be, double bn, double half_m, Cover value) {
    const double lo_e = std::min(ae, be) - half_m, hi_e = std::max(ae, be) + half_m;
    const double lo_n = std::min(an, bn) - half_m, hi_n = std::max(an, bn) + half_m;
    if (hi_e < g.west_m || hi_n < g.south_m || lo_e > g.west_m + g.cell_m * g.cols || lo_n > g.south_m + g.cell_m * g.rows) return;
    const int c0 = std::max(0, static_cast<int>(std::floor((lo_e - g.west_m) / g.cell_m)));
    const int c1 = std::min(g.cols - 1, static_cast<int>(std::floor((hi_e - g.west_m) / g.cell_m)));
    const int r0 = std::max(0, static_cast<int>(std::floor((lo_n - g.south_m) / g.cell_m)));
    const int r1 = std::min(g.rows - 1, static_cast<int>(std::floor((hi_n - g.south_m) / g.cell_m)));
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            if (mapgeo::DistToSegment(g.CentreEast(c), g.CentreNorth(r), ae, an, be, bn) <= half_m) {
                g.cover[g.Index(c, r)] = static_cast<std::uint8_t>(value);
            }
        }
    }
}

inline bool IsBuilding(MapPart p) { return p == MapPart::House || p == MapPart::Flats || p == MapPart::Farm || p == MapPart::Industrial; }

inline bool IsMilitaryGround(MapPart p) {
    return p == MapPart::Military || p == MapPart::Hangar || p == MapPart::Shelter || p == MapPart::Pad || p == MapPart::Runway ||
           p == MapPart::Revetment || p == MapPart::Bunker;
}

}  // namespace dressing_detail

// The land cover of the map proper and its detail areas, from what the map has laid out: farmland in parcels with their
// crops, then over it the levelled sites (military ground), the woods, the marshes, the buildings with their gardens,
// the military works, the roads and railways, the water.
inline CoverRaster BakeCover(const MapData& map, const DressingRules& rules) {
    using namespace dressing_detail;
    const MapDef& def = map.Def();
    CoverRaster raster;
    const double cell = rules.cover_cell_m;
    const auto add = [&](const std::string& id, double west, double south, double east, double north) {
        CoverGrid g;
        g.id = id;
        g.west_m = west;
        g.south_m = south;
        g.cell_m = cell;
        g.cols = std::max(1, static_cast<int>(std::ceil((east - west) / cell - 1e-9)));
        g.rows = std::max(1, static_cast<int>(std::ceil((north - south) / cell - 1e-9)));
        const std::size_t count = static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows);
        g.cover.assign(count, static_cast<std::uint8_t>(Cover::Field));
        g.crop.assign(count, 0);
        g.strips.assign(count, 0);
        g.fields = map.Fields();
        for (int r = 0; r < g.rows; ++r) {
            for (int c = 0; c < g.cols; ++c) {
                const FieldCell f = FieldAt(g.fields, g.CentreEast(c), g.CentreNorth(r));
                g.crop[g.Index(c, r)] = CropOf(rules, f.id);
                // A field whose centre is near a settlement's edge is the household's: strips, not a collective's field.
                for (const MapSettlementDef& s : def.settlements) {
                    if (std::hypot(f.centre_e - s.centre.east_m, f.centre_n - s.centre.north_m) < s.radius_m + rules.strip_reach_m) {
                        g.strips[g.Index(c, r)] = 1;
                        break;
                    }
                }
            }
        }
        raster.grids.push_back(std::move(g));
    };
    for (const MapDetailDef& a : def.detail) {
        add(a.id, a.centre.east_m - a.half_m, a.centre.north_m - a.half_m, a.centre.east_m + a.half_m, a.centre.north_m + a.half_m);
    }
    add("map", def.west_m, def.south_m, def.east_m, def.north_m);
    for (CoverGrid& g : raster.grids) {
        for (const MapData::Flat& f : map.Flats()) StampDisc(g, f.e, f.n, f.r - std::min(0.4 * f.r, 500.0), Cover::Military);
        for (const Cover pass : {Cover::Forest, Cover::Marsh, Cover::Town, Cover::Military}) {
            for (const MapBox& b : map.Layout().boxes) {
                if (pass == Cover::Forest && b.part == MapPart::Canopy) StampRect(g, b.east_m, b.north_m, b.length_m, b.width_m, b.heading_rad, 0.0, pass);
                if (pass == Cover::Marsh && b.part == MapPart::Marsh) StampRect(g, b.east_m, b.north_m, b.length_m, b.width_m, b.heading_rad, 0.0, pass);
                if (pass == Cover::Town && IsBuilding(b.part)) {
                    StampRect(g, b.east_m, b.north_m, b.length_m, b.width_m, b.heading_rad, rules.garden_m, pass);
                }
                if (pass == Cover::Military && IsMilitaryGround(b.part)) StampRect(g, b.east_m, b.north_m, b.length_m, b.width_m, b.heading_rad, 0.0, pass);
            }
        }
        for (const bool water : {false, true}) {
            for (const MapStrip& s : map.Layout().strips) {
                if ((s.surface == MapSurface::Water) != water) continue;
                const double half = std::max(0.5 * s.width_m, 0.5 * cell);
                for (std::size_t i = 1; i < s.points.size(); ++i) {
                    StampSegment(g, s.points[i - 1].x, s.points[i - 1].y, s.points[i].x, s.points[i].y, half, water ? Cover::Water : Cover::Road);
                }
            }
        }
    }
    return raster;
}

// ---------------------------------------------------------------------------------------------- the trees

// One tree: its base (east, north, height above sea level), its height and crown, its kind and the way it is turned.
struct TreeInstance {
    double east_m = 0.0, north_m = 0.0, base_m = 0.0;
    float height_m = 0.0f, crown_m = 0.0f, yaw_rad = 0.0f;
    std::uint8_t kind = 0;  // TreeKind
};

// A wood's block drawn as one mass (scenery/map_meshes.hpp CanopyMassMesh): a box as the map's boxes are.
struct CanopyMass {
    double east_m = 0.0, north_m = 0.0, base_m = 0.0;
    double length_m = 0.0, width_m = 0.0, height_m = 0.0, heading_rad = 0.0;
    std::uint8_t variant = 0;
};

struct MapDressing {
    std::vector<TreeInstance> trees;
    std::vector<CanopyMass> masses;
};

// Plants the map's woods: a block inside a detail area whole (no mass), a block elsewhere in the map proper with
// `edge_rows` rows along each side open to the fields and a mass inside, a block of the far ground a mass alone. Each
// tree stands on the ground 0.3 m into it, its top the wood's top less up to top_drop of its height and never above the
// ground plus the obstacle height there (MapData::ObstacleHeight: what blocks a sight line); none within line_clear_m
// of a road's, railway's or river's edge, none in the water, none outside the map proper and the detail areas.
inline MapDressing PlaceTrees(const MapData& map, const DressingRules& rules) {
    using namespace dressing_detail;
    const MapDef& def = map.Def();
    MapDressing out;
    const auto key = [](double e, double n) {
        return static_cast<long long>(std::floor(e / 10.0)) * 2000003LL + static_cast<long long>(std::floor(n / 10.0));
    };
    std::unordered_set<long long> blocks;  // looked up only, never walked
    for (const MapBox& b : map.Layout().boxes) {
        if (b.part == MapPart::Canopy) blocks.insert(key(b.east_m, b.north_m));
    }
    const std::uint64_t seed = noise::Mix64(def.seed ^ 0x7EE5ull);
    // A kind drawn from a mix by a number in [0, 1).
    const auto draw = [](const std::array<double, static_cast<std::size_t>(TreeKind::Count)>& m, double u) {
        double sum = 0.0;
        for (double s : m) sum += s;
        const double pick = u * sum;
        double run = 0.0;
        for (std::size_t k = 0; k < m.size(); ++k) {
            run += m[k];
            if (m[k] > 0.0 && pick < run) return static_cast<int>(k);
        }
        for (std::size_t k = m.size(); k-- > 0;) {
            if (m[k] > 0.0) return static_cast<int>(k);
        }
        return 0;
    };
    // The stand a point is in: its compartment's type (by its site) and age factor. Null with no stand types.
    struct Stand {
        const StandType* type = nullptr;
        double age = 1.0;
    };
    const auto stand_at = [&](double e, double n) {
        Stand s;
        if (rules.stands.empty()) return s;
        const long long ce = static_cast<long long>(std::floor(e / rules.stand_m)), cn = static_cast<long long>(std::floor(n / rules.stand_m));
        const std::uint64_t h = noise::Mix64(seed ^ 0x57A2Dull ^ Bits(ce) * 0x9E3779B97F4A7C15ull ^ noise::Mix64(Bits(cn)));
        const bool wet = map.NearWater(e, n, rules.wet_m);
        double sum = 0.0;
        for (const StandType& t : rules.stands) sum += t.wet == wet ? t.weight : 0.0;
        const double pick = Unit(h) * sum;
        double run = 0.0;
        for (const StandType& t : rules.stands) {
            if (t.wet != wet) continue;
            run += t.weight;
            s.type = &t;
            if (pick < run) break;
        }
        s.age = rules.age_min + (1.0 - rules.age_min) * Unit(noise::Mix64(h ^ 0xA6Eull));
        return s;
    };
    // Plants one tree (or, for a shrub's kind, one shrub) of the kind at the point, its height times `height_factor`.
    const auto plant = [&](const MapBox& b, double e, double n, int kind, double height_factor = 1.0) {
        if (!def.InCore(e, n) && !map.InDetail(e, n)) return;
        if (map.NearLine(e, n, rules.line_clear_m) || map.NearWater(e, n, 0.0)) return;
        const double ground = map.GroundHeight(e, n);
        const double top_limit = ground + map.ObstacleHeight(e, n);
        const double base = ground - 0.3;
        const bool shrub = IsShrub(static_cast<TreeKind>(kind));
        double top = 0.0;
        if (shrub) {
            top = ground + rules.shrub_height_min_m + (rules.shrub_height_max_m - rules.shrub_height_min_m) * At(seed ^ 0x5B2ull, e, n);
        } else {
            top = b.base_m + b.height_m - At(seed ^ 0x70Full, e, n) * rules.top_drop * b.height_m;
        }
        top = std::min(top, top_limit);
        double height = top - base;
        if (!shrub) height *= height_factor;
        if (height < (shrub ? 1.0 : 3.0)) return;
        TreeInstance t;
        t.east_m = e;
        t.north_m = n;
        t.base_m = base;
        t.height_m = static_cast<float>(height);
        t.crown_m = static_cast<float>(height * rules.crown_ratio * (1.0 + rules.crown_jitter * (2.0 * At(seed ^ 0xC20ull, e, n) - 1.0)));
        t.yaw_rad = static_cast<float>(At(seed ^ 0xA3ull, e, n) * kTwoPi);
        t.kind = static_cast<std::uint8_t>(kind);
        out.trees.push_back(t);
    };
    // A wood's tree at a point: the mantle's kind and height within the edge band (`edge`), else its stand's (or, with no
    // stand types, the species shares, poplar by the water).
    const auto wood_tree = [&](const MapBox& b, double e, double n, bool edge) {
        const double u = At(seed ^ 0x5Full, e, n);
        const Stand s = stand_at(e, n);
        if (edge) {
            plant(b, e, n, draw(rules.edge_mix, u), rules.edge_height * s.age);
            return;
        }
        if (s.type != nullptr) {
            plant(b, e, n, draw(s.type->mix, u), s.age);
            return;
        }
        if (rules.species_shares[static_cast<std::size_t>(TreeKind::Poplar)] > 0.0 && map.NearWater(e, n, rules.poplar_river_m)) {
            plant(b, e, n, static_cast<int>(TreeKind::Poplar));
            return;
        }
        plant(b, e, n, draw(rules.species_shares, u));
    };
    // The jitter at a point: a plantation's rows keep nearly to the lattice.
    const auto jitter_at = [&](double e, double n) {
        const Stand s = stand_at(e, n);
        return s.type != nullptr && s.type->plantation ? rules.plantation_jitter : 1.0;
    };
    // A point of the block's frame (along its heading, right of it) jittered by up to `jitter` of the spacing each way.
    const auto jittered = [&](const MapBox& b, double fwd, double right, double spacing, double& e, double& n, bool rows = false) {
        const double sh = std::sin(b.heading_rad), ch = std::cos(b.heading_rad);
        const double e0 = b.east_m + fwd * sh + right * ch, n0 = b.north_m + fwd * ch - right * sh;
        const double j = rules.jitter * spacing * (rows ? jitter_at(e0, n0) : 1.0);
        e = e0 + j * (2.0 * At(seed ^ 0x1E5ull, e0, n0) - 1.0);
        n = n0 + j * (2.0 * At(seed ^ 0x1E6ull, e0, n0) - 1.0);
    };
    for (const MapBox& b : map.Layout().boxes) {
        if (b.part == MapPart::TreeRow) {
            // A row or a grove (the map's farmland, roadmap T4): trees on a lattice of the row's own spacing, poplar and
            // black locust (some oak in a grove), the row's own kind each. Their tops keep to the box's height, which
            // the sight lines read.
            constexpr double kRowSpacingM = 7.0;
            const int count_l = std::max(1, static_cast<int>(std::floor(b.length_m / kRowSpacingM)));
            const int count_w = std::max(1, static_cast<int>(std::floor(b.width_m / kRowSpacingM)));
            const double step_l = b.length_m / count_l, step_w = b.width_m / count_w;
            const double pick = At(seed ^ 0x8Eull, b.east_m, b.north_m);
            const int kind = static_cast<int>(pick < 0.55 ? TreeKind::Poplar : (pick < 0.9 ? TreeKind::Acacia : TreeKind::Oak));
            for (int i = 0; i < count_l; ++i) {
                for (int j = 0; j < count_w; ++j) {
                    double e = 0.0, n = 0.0;
                    jittered(b, (i + 0.5) * step_l - 0.5 * b.length_m, (j + 0.5) * step_w - 0.5 * b.width_m, std::min(step_l, step_w), e, n);
                    plant(b, e, n, kind);
                }
            }
            continue;
        }
        if (b.part != MapPart::Canopy) continue;
        const bool whole = map.InDetail(b.east_m, b.north_m);
        const bool core = def.InCore(b.east_m, b.north_m);
        // The block's sides open to the fields: no wood block of its size next to it (the map's blocks are square and
        // turned north: forward is north, right is east).
        const double size = b.length_m;
        const bool open_n = blocks.count(key(b.east_m, b.north_m + size)) == 0;
        const bool open_s = blocks.count(key(b.east_m, b.north_m - size)) == 0;
        const bool open_e = blocks.count(key(b.east_m + size, b.north_m)) == 0;
        const bool open_w = blocks.count(key(b.east_m - size, b.north_m)) == 0;
        // How far a point (forward, right of the centre) is in from the nearest open side.
        const auto in_from_open = [&](double fwd, double right) {
            double d = 1e9;
            if (open_n) d = std::min(d, 0.5 * size - fwd);
            if (open_s) d = std::min(d, 0.5 * size + fwd);
            if (open_e) d = std::min(d, 0.5 * size - right);
            if (open_w) d = std::min(d, 0.5 * size + right);
            return d;
        };
        // The fringe: shrubs along each open side, shrub_inset_m in, some left out, in the map proper and the areas.
        if ((core || whole) && rules.shrub_keep > 0.0) {
            const int along = std::max(1, static_cast<int>(std::floor(size / rules.shrub_spacing_m)));
            const double step = size / along;
            for (int k = 0; k < along; ++k) {
                const double s = (k + 0.5) * step - 0.5 * size;
                const double in = rules.shrub_inset_m;
                const double sides[4][3] = {{open_n ? 1.0 : 0.0, 0.5 * size - in, s},
                                            {open_s ? 1.0 : 0.0, -0.5 * size + in, s},
                                            {open_e ? 1.0 : 0.0, s, 0.5 * size - in},
                                            {open_w ? 1.0 : 0.0, s, -0.5 * size + in}};
                for (const auto& side : sides) {
                    if (side[0] == 0.0) continue;
                    double e = 0.0, n = 0.0;
                    jittered(b, side[1], side[2], step, e, n);
                    if (At(seed ^ 0x5E7ull, e, n) >= rules.shrub_keep) continue;
                    plant(b, e, n, draw(rules.shrub_mix, At(seed ^ 0x5E8ull, e, n)));
                }
            }
        }
        if (whole) {
            const int count_l = std::max(1, static_cast<int>(std::floor(b.length_m / rules.near_spacing_m)));
            const int count_w = std::max(1, static_cast<int>(std::floor(b.width_m / rules.near_spacing_m)));
            const double step_l = b.length_m / count_l, step_w = b.width_m / count_w;
            for (int i = 0; i < count_l; ++i) {
                for (int j = 0; j < count_w; ++j) {
                    const double fwd = (i + 0.5) * step_l - 0.5 * b.length_m, right = (j + 0.5) * step_w - 0.5 * b.width_m;
                    double e = 0.0, n = 0.0;
                    jittered(b, fwd, right, std::min(step_l, step_w), e, n, true);
                    wood_tree(b, e, n, in_from_open(fwd, right) < rules.edge_band_m);
                }
            }
            continue;
        }
        if (core && rules.edge_rows > 0) {
            const double band = std::min(0.5 * size, rules.edge_rows * rules.edge_spacing_m);
            const int along = std::max(1, static_cast<int>(std::floor(size / rules.edge_spacing_m)));
            const double step = size / along;
            for (int row = 0; row < rules.edge_rows; ++row) {
                const double in = (row + 0.5) * rules.edge_spacing_m;
                if (in >= 0.5 * size) break;
                for (int k = 0; k < along; ++k) {
                    const double s = (k + 0.5) * step - 0.5 * size;  // along the side
                    double e = 0.0, n = 0.0;
                    const bool mantle = in < rules.edge_band_m;
                    if (open_n) {
                        jittered(b, 0.5 * size - in, s, rules.edge_spacing_m, e, n);
                        wood_tree(b, e, n, mantle);
                    }
                    if (open_s) {
                        jittered(b, -0.5 * size + in, s, rules.edge_spacing_m, e, n);
                        wood_tree(b, e, n, mantle);
                    }
                    // The east and west rows stop short of a north or south side's rows (no tree planted twice).
                    if ((open_n && s > 0.5 * size - band) || (open_s && s < -0.5 * size + band)) continue;
                    if (open_e) {
                        jittered(b, s, 0.5 * size - in, rules.edge_spacing_m, e, n);
                        wood_tree(b, e, n, mantle);
                    }
                    if (open_w) {
                        jittered(b, s, -0.5 * size + in, rules.edge_spacing_m, e, n);
                        wood_tree(b, e, n, mantle);
                    }
                }
            }
        }
        CanopyMass m;
        m.east_m = b.east_m;
        m.north_m = b.north_m;
        m.base_m = b.base_m;
        m.length_m = std::max(1.0, b.length_m - 2.0 * rules.mass_inset_m);
        m.width_m = std::max(1.0, b.width_m - 2.0 * rules.mass_inset_m);
        m.height_m = rules.mass_top * b.height_m;
        m.heading_rad = b.heading_rad;
        m.variant = static_cast<std::uint8_t>(noise::Mix64(seed ^ Bits(std::llround(b.east_m)) * 0x9E37ull ^ Bits(std::llround(b.north_m))) %
                                              static_cast<std::uint64_t>(kCanopyMassVariants));
        out.masses.push_back(m);
    }
    return out;
}

// ---------------------------------------------------------------------------------------------- the buildings

// A building's variant (0 .. house_variants - 1) by its place: the same house the same on every client.
inline int BuildingVariant(const MapBox& b, int variants = kHouseVariants) {
    const std::uint64_t h = noise::Mix64(dressing_detail::Bits(std::llround(b.east_m * 10.0)) * 0x8CB92BA72F3D8DD7ull ^
                                         noise::Mix64(dressing_detail::Bits(std::llround(b.north_m * 10.0))));
    return static_cast<int>(h % static_cast<std::uint64_t>(std::max(1, variants)));
}

// A block of flats' storeys (scenery/map_meshes.hpp FlatsMesh's variant): its height above the ground over 2.8 m each.
inline int FlatsFloors(const MapBox& b) {
    return std::max(1, std::min(kFlatsMaxFloors, static_cast<int>(std::lround((b.height_m - 0.5) / 2.8))));
}

// The part key of scenery/map_meshes.hpp (and the asset register) a box is drawn by, and its variant; "" for a box that
// stays a box (the engine's cube or cylinder).
inline std::string BuildingPart(const MapBox& b, int& variant, int house_variants = kHouseVariants) {
    variant = 0;
    switch (b.part) {
    case MapPart::House:
        variant = BuildingVariant(b, house_variants);
        return "house";
    case MapPart::Flats:
        variant = FlatsFloors(b);
        return "flats";
    case MapPart::Farm: return "barn";
    case MapPart::Industrial: return "industrial";
    default: break;
    }
    return "";
}

}  // namespace scenery
}  // namespace simcore
