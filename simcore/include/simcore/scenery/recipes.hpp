// simcore/scenery/recipes.hpp — the land's recipes as data (roadmap L2, docs/scenery/TERRAIN_ROADMAP.md part A, decision
// D3; the method in docs/research/TERRAIN_PROCEDURAL_METHOD_2026-11.md sections 2 and 6.3).
//
// A recipe says how one land type looks, once, and is applied wherever the land type is: it applies to ground layers
// (a field's crop state is already its layer), its bands divide a parcel by the distance in from its edge (a field's own
// rectangle from farm_fields.hpp, exact at every distance: the headlands at its ends, a margin at its sides), and each
// band scatters kinds of clutter by a point process:
//   uniform     a Poisson-like scatter, so many a hectare, drawn on 4 m squares of its own over the whole plane;
//   blue_noise  a jittered lattice, no two close together (a crop's clods, single plants);
//   thomas      clusters: parents kappa_per_ha, offspring per_ha in all, displaced by a normal of spread_m each way;
//   matern      the same with the offspring uniform in a disc of radius spread_m (shrub clumps, reed tussocks);
//   rows        a crop's rows along the parcel's furrows, row_m apart, the plants along them per_ha's spacing, jittered
//               (maize stumps 70-76 cm apart); on a field only.
// Every candidate is drawn from its own position's hash (the same on every client and wherever the camera is) and carries
// a fixed threshold in [0, 1): a use that thins (the ring's falloff, a lower density) keeps the candidates under its
// fraction, so thinning never moves a plant; a blue-noise lattice orders its thresholds by a 4 x 4 dither, so a thinned
// lattice stays even. Recipes are read from data/scenery/recipes/ (LoadRecipes); RecipeCandidates gives a cell's points
// and RecipeMeasure the report (hectares per band, instances a hectare against each recipe's targets). Presentation
// only: nothing in the simulation reads it.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/json.hpp"
#include "simcore/scenery/ground_layers.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace scenery {

enum class ScatterProcess { Uniform, BlueNoise, Thomas, Matern, Rows };

struct RecipeScatter {
    std::string kind;       // a clutter kind's id (data/scenery/clutter.json)
    int kind_index = -1;    // its index in the kinds the recipes were loaded against
    ScatterProcess process = ScatterProcess::Uniform;
    double per_ha = 0.0;        // instances a hectare at full density
    double kappa_per_ha = 0.0;  // clusters a hectare (thomas, matern)
    double spread_m = 0.0;      // thomas: the offspring's standard deviation each way; matern: the disc's radius
    double row_m = 0.0;         // rows: the rows' spacing across the furrows (the spacing along them is per_ha's)
    double across_m = 0.04;     // rows: how far a point wanders across its row either way (a drill's 4 cm, a swath's half width)
    std::vector<int> crops;     // only on a field of these crops (the dressing's crop indices); empty: any
};

// Which of the parcel's edges a band's distance is measured from: its ends (across the furrows: the headlands), its sides
// (along them) or the nearer of the two; or, under a wood, its open side (the land cover's nearest cell that is not
// forest: a wood's herb fringe, roadmap L7).
enum class RecipeEdge { Any, Ends, Sides, Wood };

struct RecipeBand {
    std::string id;
    bool whole = true;  // no distance: the rest of the land type (a field's body, a meadow)
    RecipeEdge edge = RecipeEdge::Any;
    double from_m = 0.0, to_m = 0.0;  // distance in from the edge, from_m <= d < to_m
    std::vector<RecipeScatter> scatter;
};

struct RecipeTarget {
    std::string band, kind;
    double lo_per_ha = 0.0, hi_per_ha = 0.0;
};

struct Recipe {
    std::string id;
    std::vector<int> layers;  // the ground layers it applies to
    int priority = 0;
    std::vector<RecipeBand> bands;  // a point takes the first band it lies in
    std::vector<RecipeTarget> targets;
    std::string source, confidence;
    std::uint64_t salt = 0;  // from the id: a recipe's points are its own whatever the file order
    bool HasEdgeBands() const {
        for (const RecipeBand& b : bands) {
            if (!b.whole) return true;
        }
        return false;
    }
    // How far in from a wood's side its bands reach (0: none measured from a wood's side).
    double WoodReachM() const {
        double reach = 0.0;
        for (const RecipeBand& b : bands) {
            if (!b.whole && b.edge == RecipeEdge::Wood) reach = std::max(reach, b.to_m);
        }
        return reach;
    }
};

struct RecipeSet {
    std::vector<Recipe> recipes;
    std::vector<int> by_layer;  // the recipe of each ground layer, -1 none
    const Recipe* ForLayer(int layer) const {
        if (layer < 0 || static_cast<std::size_t>(layer) >= by_layer.size() || by_layer[static_cast<std::size_t>(layer)] < 0) return nullptr;
        return &recipes[static_cast<std::size_t>(by_layer[static_cast<std::size_t>(layer)])];
    }
};

// One candidate: where, which band and scatter of its recipe, its fixed threshold and its own hash (for its size and turn).
struct RecipeCandidate {
    double east_m = 0.0, north_m = 0.0;
    double threshold = 0.0;
    std::uint64_t hash = 0;
    int recipe = -1, band = -1, scatter = -1;
    int kind = -1;
    bool on_field = false;
    double heading_rad = 0.0;  // the parcel's furrows, clockwise from north (on a field)
    double stand_m = 0.0;      // how high its field's crop stands here on the day (StandAtPoint; 0 off a field or where nothing stands)
};

namespace recipe_detail {

inline bool Fail(std::string* error, const std::string& why) {
    if (error != nullptr) *error = why;
    return false;
}

inline std::uint64_t Mix(std::uint64_t x) {
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDull;
    x ^= x >> 33;
    x *= 0xC4CEB9FE1A85EC53ull;
    x ^= x >> 33;
    return x;
}

inline std::uint64_t Hash(std::int64_t a, std::int64_t b, std::uint64_t salt, std::uint64_t j) {
    std::uint64_t x = static_cast<std::uint64_t>(a) * 0x9E3779B97F4A7C15ull;
    x ^= static_cast<std::uint64_t>(b) * 0xC2B2AE3D27D4EB4Full;
    x ^= salt * 0x165667B19E3779F9ull;
    x ^= j * 0x27D4EB2F165667C5ull;
    return Mix(x);
}

// [0, 1) from a hash's top 53 bits; a second draw from the same hash rehashed.
inline double Unit(std::uint64_t h) { return static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0); }

inline std::uint64_t IdSalt(const std::string& id) {
    std::uint64_t h = 1469598103934665603ull;  // FNV-1a
    for (const char c : id) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;
    }
    return Mix(h);
}

// The 4 x 4 ordered dither: a lattice thinned by it stays even.
inline int Bayer4(std::int64_t i, std::int64_t j) {
    static const int kM[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    return kM[static_cast<std::uint64_t>(i) & 3u][static_cast<std::uint64_t>(j) & 3u];
}

inline bool NumberIn(const json::Value& o, const char* key, double lo, double hi, double& to, bool required) {
    const json::Value* v = o.Find(key);
    if (v == nullptr) return !required;
    if (!v->Is(json::Type::Number) || v->number < lo || v->number > hi) return false;
    to = v->number;
    return true;
}

inline bool ReadText(const std::string& path_or_json, std::string& text) {
    const std::size_t first = path_or_json.find_first_not_of(" \t\r\n");
    if (first != std::string::npos && path_or_json[first] == '{') {
        text = path_or_json;
        return true;
    }
    std::ifstream f(path_or_json, std::ios::binary);
    if (!f) return false;
    std::ostringstream s;
    s << f.rdbuf();
    text = s.str();
    return true;
}

}  // namespace recipe_detail

// Reads one recipe (a file's path or its JSON text) against the ground layers and the clutter kinds' ids. False with the
// reason in `error`.
inline bool LoadRecipe(const std::string& path_or_json, const GroundLayerRules& layers, const std::vector<std::string>& kind_ids, Recipe& out,
                       std::string* error = nullptr) {
    using recipe_detail::Fail;
    using recipe_detail::NumberIn;
    std::string text;
    if (!recipe_detail::ReadText(path_or_json, text)) return Fail(error, "cannot open " + path_or_json);
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) return Fail(error, parse_error);
    Recipe r;
    const json::Value* id = root.Find("recipe_id");
    if (id == nullptr || !id->Is(json::Type::String) || id->string.empty()) return Fail(error, "no recipe_id");
    r.id = id->string;
    r.salt = recipe_detail::IdSalt(r.id);
    const std::string at = r.id + ": ";
    const json::Value* applies = root.Find("applies");
    const json::Value* ls = applies != nullptr ? applies->Find("layers") : nullptr;
    if (ls == nullptr || !ls->Is(json::Type::Array) || ls->items.empty()) return Fail(error, at + "applies.layers is not a list of ground layers");
    for (const json::Value& l : ls->items) {
        const int li = l.Is(json::Type::String) ? layers.Index(l.string) : -1;
        if (li < 0) return Fail(error, at + "applies.layers: no ground layer \"" + (l.Is(json::Type::String) ? l.string : std::string("?")) + "\"");
        r.layers.push_back(li);
    }
    double priority = 0.0;
    if (!NumberIn(root, "priority", -1000.0, 1000.0, priority, false)) return Fail(error, at + "priority is not in -1000..1000");
    r.priority = static_cast<int>(priority);
    const json::Value* bands = root.Find("bands");
    if (bands == nullptr || !bands->Is(json::Type::Array) || bands->items.empty()) return Fail(error, at + "no \"bands\" list");
    bool whole_seen = false;
    for (const json::Value& bv : bands->items) {
        RecipeBand b;
        const json::Value* bid = bv.Find("id");
        if (bid == nullptr || !bid->Is(json::Type::String) || bid->string.empty()) return Fail(error, at + "a band without an id");
        b.id = bid->string;
        for (const RecipeBand& o : r.bands) {
            if (o.id == b.id) return Fail(error, at + "the band \"" + b.id + "\" twice");
        }
        if (whole_seen) return Fail(error, at + b.id + ": after a band with no distance (it would never be reached)");
        const json::Value* from = bv.Find("from_m");
        const json::Value* to = bv.Find("to_m");
        if ((from == nullptr) != (to == nullptr)) return Fail(error, at + b.id + ": from_m and to_m go together");
        if (from != nullptr) {
            b.whole = false;
            if (!NumberIn(bv, "from_m", 0.0, 500.0, b.from_m, true) || !NumberIn(bv, "to_m", 0.0, 500.0, b.to_m, true) || b.to_m <= b.from_m) {
                return Fail(error, at + b.id + ": from_m and to_m are not 0 <= from < to <= 500");
            }
            const json::Value* edge = bv.Find("edge");
            const std::string e = edge != nullptr && edge->Is(json::Type::String) ? edge->string : std::string("any");
            if (e == "any") b.edge = RecipeEdge::Any;
            else if (e == "ends") b.edge = RecipeEdge::Ends;
            else if (e == "sides") b.edge = RecipeEdge::Sides;
            else if (e == "wood") b.edge = RecipeEdge::Wood;
            else return Fail(error, at + b.id + ": edge is not any, ends, sides or wood");
        } else {
            whole_seen = true;
        }
        const json::Value* sc = bv.Find("scatter");
        if (sc == nullptr || !sc->Is(json::Type::Array)) return Fail(error, at + b.id + ": scatter is not a list");
        for (const json::Value& sv : sc->items) {
            RecipeScatter s;
            const json::Value* kind = sv.Find("kind");
            if (kind == nullptr || !kind->Is(json::Type::String)) return Fail(error, at + b.id + ": a scatter names no kind");
            s.kind = kind->string;
            for (std::size_t k = 0; k < kind_ids.size(); ++k) {
                if (kind_ids[k] == s.kind) s.kind_index = static_cast<int>(k);
            }
            if (s.kind_index < 0) return Fail(error, at + b.id + ": no clutter kind \"" + s.kind + "\"");
            const json::Value* process = sv.Find("process");
            const std::string p = process != nullptr && process->Is(json::Type::String) ? process->string : std::string();
            if (p == "uniform") s.process = ScatterProcess::Uniform;
            else if (p == "blue_noise") s.process = ScatterProcess::BlueNoise;
            else if (p == "thomas") s.process = ScatterProcess::Thomas;
            else if (p == "matern") s.process = ScatterProcess::Matern;
            else if (p == "rows") s.process = ScatterProcess::Rows;
            else return Fail(error, at + b.id + ": process is not uniform, blue_noise, thomas, matern or rows");
            // At most 10 a square metre (a lattice 32 cm apart).
            if (!NumberIn(sv, "per_ha", 1.0, 100000.0, s.per_ha, true)) return Fail(error, at + b.id + ": per_ha is not in 1..100000");
            if (s.process == ScatterProcess::Thomas || s.process == ScatterProcess::Matern) {
                if (!NumberIn(sv, "kappa_per_ha", 0.01, s.per_ha, s.kappa_per_ha, true)) return Fail(error, at + b.id + ": kappa_per_ha is not in 0.01..per_ha");
                if (!NumberIn(sv, "spread_m", 0.05, 20.0, s.spread_m, true)) return Fail(error, at + b.id + ": spread_m is not in 0.05..20");
            }
            if (s.process == ScatterProcess::Rows) {
                // Up to 12 m: a combine's swaths lie a header's width apart.
                if (!NumberIn(sv, "row_m", 0.1, 12.0, s.row_m, true)) return Fail(error, at + b.id + ": row_m is not in 0.1..12");
                if (10000.0 / (s.per_ha * s.row_m) < 0.05) return Fail(error, at + b.id + ": the rows' plants under 5 cm apart (per_ha too high for row_m)");
                if (!NumberIn(sv, "across_m", 0.0, 0.4 * s.row_m, s.across_m, false)) return Fail(error, at + b.id + ": across_m is not in 0..0.4 of row_m");
            } else if (sv.Find("across_m") != nullptr) {
                return Fail(error, at + b.id + ": across_m is for rows only");
            }
            if (const json::Value* crops = sv.Find("crops")) {
                if (!crops->Is(json::Type::Array) || crops->items.empty()) return Fail(error, at + b.id + ": crops is a list of the dressing's crops");
                for (const json::Value& c : crops->items) {
                    const int ci = c.Is(json::Type::String) ? layers.CropIndex(c.string) : -1;
                    if (ci < 0) return Fail(error, at + b.id + ": crops names no crop of the dressing's");
                    s.crops.push_back(ci);
                }
            }
            b.scatter.push_back(s);
        }
        r.bands.push_back(b);
    }
    if (const json::Value* targets = root.Find("targets")) {
        if (!targets->Is(json::Type::Array)) return Fail(error, at + "targets is not a list");
        for (const json::Value& tv : targets->items) {
            RecipeTarget t;
            const json::Value* band = tv.Find("band");
            const json::Value* kind = tv.Find("kind");
            const json::Value* per = tv.Find("per_ha");
            if (band == nullptr || !band->Is(json::Type::String) || kind == nullptr || !kind->Is(json::Type::String)) return Fail(error, at + "a target names no band or kind");
            t.band = band->string;
            t.kind = kind->string;
            bool known = false;
            for (const RecipeBand& b : r.bands) known = known || b.id == t.band;
            if (!known) return Fail(error, at + "a target for no band \"" + t.band + "\"");
            if (per == nullptr || !per->Is(json::Type::Array) || per->items.size() != 2 || !per->items[0].Is(json::Type::Number) ||
                !per->items[1].Is(json::Type::Number) || per->items[0].number < 0.0 || per->items[1].number < per->items[0].number) {
                return Fail(error, at + "a target's per_ha is not [low, high]");
            }
            t.lo_per_ha = per->items[0].number;
            t.hi_per_ha = per->items[1].number;
            r.targets.push_back(t);
        }
    }
    const json::Value* source = root.Find("source");
    const json::Value* confidence = root.Find("confidence");
    if (source == nullptr || !source->Is(json::Type::String) || source->string.empty()) return Fail(error, at + "no source");
    if (confidence == nullptr || !confidence->Is(json::Type::String) || confidence->string.empty()) return Fail(error, at + "no confidence");
    r.source = source->string;
    r.confidence = confidence->string;
    out = r;
    return true;
}

// Reads every *.json in `dir` (in name order) as a recipe. A ground layer takes the recipe of the highest priority that
// applies to it; two of the same priority for one layer, or one id twice, is refused.
inline bool LoadRecipes(const std::string& dir, const GroundLayerRules& layers, const std::vector<std::string>& kind_ids, RecipeSet& out,
                        std::string* error = nullptr) {
    using recipe_detail::Fail;
    std::vector<std::string> files;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file() && it->path().extension() == ".json") files.push_back(it->path().string());
    }
    if (ec) return Fail(error, "cannot read " + dir);
    if (files.empty()) return Fail(error, "no recipes in " + dir);
    std::sort(files.begin(), files.end());
    RecipeSet set;
    set.by_layer.assign(layers.layers.size(), -1);
    for (const std::string& f : files) {
        Recipe r;
        std::string e;
        if (!LoadRecipe(f, layers, kind_ids, r, &e)) return Fail(error, std::filesystem::path(f).filename().string() + ": " + e);
        for (const Recipe& o : set.recipes) {
            if (o.id == r.id) return Fail(error, "the recipe \"" + r.id + "\" twice");
        }
        const int index = static_cast<int>(set.recipes.size());
        for (const int l : r.layers) {
            int& slot = set.by_layer[static_cast<std::size_t>(l)];
            if (slot >= 0) {
                const Recipe& o = set.recipes[static_cast<std::size_t>(slot)];
                if (o.priority == r.priority) {
                    return Fail(error, r.id + " and " + o.id + " both apply to " + layers.layers[static_cast<std::size_t>(l)].id + " at one priority");
                }
                if (o.priority > r.priority) continue;
            }
            slot = index;
        }
        set.recipes.push_back(r);
    }
    out = set;
    return true;
}

// The parcel (east, north) lies in: false off the fields.
inline bool ParcelAt(const CoverRaster& cover, double east_m, double north_m, FieldCell& out) {
    const CoverGrid* g = cover.GridAt(east_m, north_m);
    if (g == nullptr || static_cast<Cover>(g->cover[g->IndexAt(east_m, north_m)]) != Cover::Field) return false;
    out = FieldAt(g->fields, east_m, north_m);
    return true;
}

// The distance of (east, north) in from its parcel `f`'s ends (across the furrows) and sides (along them), m.
inline void ParcelEdgeDistance(const FieldCell& f, double east_m, double north_m, double& ends_m, double& sides_m) {
    const double sh = std::sin(f.heading_rad), ch = std::cos(f.heading_rad);
    const double de = east_m - f.centre_e, dn = north_m - f.centre_n;
    ends_m = 0.5 * f.length_m - std::fabs(de * sh + dn * ch);
    sides_m = 0.5 * f.width_m - std::fabs(de * ch - dn * sh);
}

// The same found from the map: both infinite off the fields.
inline void ParcelEdgeDistance(const CoverRaster& cover, double east_m, double north_m, double& ends_m, double& sides_m) {
    ends_m = sides_m = std::numeric_limits<double>::infinity();
    FieldCell f;
    if (ParcelAt(cover, east_m, north_m, f)) ParcelEdgeDistance(f, east_m, north_m, ends_m, sides_m);
}

// How far (east, north), in a wood (forest in the land cover), is in from the wood's open side: the distance to the
// nearest cell of its grid within `reach_m` that is not forest; infinite off the woods or with none so near.
inline double WoodEdgeDistance(const CoverRaster& cover, double east_m, double north_m, double reach_m) {
    const double none = std::numeric_limits<double>::infinity();
    const CoverGrid* g = cover.GridAt(east_m, north_m);
    if (g == nullptr || reach_m <= 0.0 || static_cast<Cover>(g->cover[g->IndexAt(east_m, north_m)]) != Cover::Forest) return none;
    const int c0 = std::max(0, static_cast<int>(std::floor((east_m - reach_m - g->west_m) / g->cell_m)));
    const int c1 = std::min(g->cols - 1, static_cast<int>(std::floor((east_m + reach_m - g->west_m) / g->cell_m)));
    const int r0 = std::max(0, static_cast<int>(std::floor((north_m - reach_m - g->south_m) / g->cell_m)));
    const int r1 = std::min(g->rows - 1, static_cast<int>(std::floor((north_m + reach_m - g->south_m) / g->cell_m)));
    double best = none;
    for (int r = r0; r <= r1; ++r) {
        for (int c = c0; c <= c1; ++c) {
            if (static_cast<Cover>(g->cover[g->Index(c, r)]) == Cover::Forest) continue;
            const double x0 = g->west_m + g->cell_m * c, y0 = g->south_m + g->cell_m * r;
            const double dx = std::max({x0 - east_m, 0.0, east_m - (x0 + g->cell_m)});
            const double dy = std::max({y0 - north_m, 0.0, north_m - (y0 + g->cell_m)});
            best = std::min(best, std::hypot(dx, dy));
        }
    }
    return best <= reach_m ? best : none;
}

// The band of `recipe` a point lies in, -1 none. `wood_m`: WoodEdgeDistance's (infinite where no band reads it).
inline int RecipeBandAt(const Recipe& recipe, double ends_m, double sides_m, double wood_m = std::numeric_limits<double>::infinity()) {
    for (std::size_t i = 0; i < recipe.bands.size(); ++i) {
        const RecipeBand& b = recipe.bands[i];
        if (b.whole) return static_cast<int>(i);
        const double d = b.edge == RecipeEdge::Wood ? wood_m
                         : b.edge == RecipeEdge::Ends ? ends_m
                         : (b.edge == RecipeEdge::Sides ? sides_m : std::min(ends_m, sides_m));
        if (d >= b.from_m && d < b.to_m) return static_cast<int>(i);
    }
    return -1;
}

// One scatter's candidates in the square cell (cx, cy) of side `cell_m`, every one from its own position's hash: emit(east,
// north, threshold, hash). The lattice and the clusters run over the whole plane, so cells meet without a seam. Rows need
// the furrows' heading there (clockwise from north; NaN off the fields: none).
template <class Emit>
inline void ScatterInCell(const RecipeScatter& s, std::uint64_t salt, std::int64_t cx, std::int64_t cy, double cell_m, Emit&& emit,
                          double heading_rad = std::numeric_limits<double>::quiet_NaN()) {
    using recipe_detail::Hash;
    using recipe_detail::Unit;
    const double e0 = static_cast<double>(cx) * cell_m, n0 = static_cast<double>(cy) * cell_m;
    const double e1 = e0 + cell_m, n1 = n0 + cell_m;
    const auto inside = [&](double e, double n) { return e >= e0 && e < e1 && n >= n0 && n < n1; };
    switch (s.process) {
        case ScatterProcess::Uniform: {
            // On squares of its own over the whole plane, the near ring's 4 m cells (clutter.hpp), so a cell of any size
            // has the same points (a 4 m cell is one square, as before A.3.39).
            const double g = 4.0;
            const double expect = s.per_ha * g * g / 10000.0;
            const auto i0 = static_cast<std::int64_t>(std::floor(e0 / g)), i1 = static_cast<std::int64_t>(std::ceil(e1 / g)) - 1;
            const auto j0 = static_cast<std::int64_t>(std::floor(n0 / g)), j1 = static_cast<std::int64_t>(std::ceil(n1 / g)) - 1;
            for (std::int64_t sj = j0; sj <= j1; ++sj) {
                for (std::int64_t si = i0; si <= i1; ++si) {
                    const int count = static_cast<int>(std::floor(expect + Unit(Hash(si, sj, salt, 0))));
                    const double se = static_cast<double>(si) * g, sn = static_cast<double>(sj) * g;
                    for (int j = 0; j < count; ++j) {
                        const std::uint64_t h = Hash(si, sj, salt, 16u + static_cast<std::uint64_t>(j));
                        const double e = se + Unit(h) * g, n = sn + Unit(recipe_detail::Mix(h ^ 1u)) * g;
                        if (!inside(e, n)) continue;
                        emit(e, n, Unit(recipe_detail::Mix(h ^ 2u)), h);
                    }
                }
            }
            break;
        }
        case ScatterProcess::BlueNoise: {
            // One point a lattice site, jittered within the middle 80% of its square: never closer than a fifth of the spacing.
            const double a = std::sqrt(10000.0 / s.per_ha);
            const std::int64_t i0 = static_cast<std::int64_t>(std::floor(e0 / a)) - 1, i1 = static_cast<std::int64_t>(std::floor(e1 / a)) + 1;
            const std::int64_t j0 = static_cast<std::int64_t>(std::floor(n0 / a)) - 1, j1 = static_cast<std::int64_t>(std::floor(n1 / a)) + 1;
            for (std::int64_t j = j0; j <= j1; ++j) {
                for (std::int64_t i = i0; i <= i1; ++i) {
                    const std::uint64_t h = Hash(i, j, salt, 7u);
                    const double e = (static_cast<double>(i) + 0.1 + 0.8 * Unit(h)) * a;
                    const double n = (static_cast<double>(j) + 0.1 + 0.8 * Unit(recipe_detail::Mix(h ^ 1u))) * a;
                    if (!inside(e, n)) continue;
                    emit(e, n, (static_cast<double>(recipe_detail::Bayer4(i, j)) + Unit(recipe_detail::Mix(h ^ 2u))) / 16.0, h);
                }
            }
            break;
        }
        case ScatterProcess::Rows: {
            // The rows in the furrows' frame (farm_fields.hpp's: u along, v across), the same lattice over the whole estate.
            if (!std::isfinite(heading_rad)) break;
            const double sh = std::sin(heading_rad), ch = std::cos(heading_rad);
            const double along = 10000.0 / (s.per_ha * s.row_m);
            double umin = std::numeric_limits<double>::infinity(), umax = -umin, vmin = umin, vmax = -umin;
            for (int k = 0; k < 4; ++k) {
                const double e = (k & 1) != 0 ? e1 : e0, n = (k & 2) != 0 ? n1 : n0;
                const double u = e * sh + n * ch, v = e * ch - n * sh;
                umin = std::min(umin, u);
                umax = std::max(umax, u);
                vmin = std::min(vmin, v);
                vmax = std::max(vmax, v);
            }
            // The heading's own bits in the salt: two estates' lattices are not one.
            const std::uint64_t hs = salt ^ recipe_detail::Mix(static_cast<std::uint64_t>(std::llround(heading_rad * 1e6)));
            const double jitter_v = std::min(0.4, s.across_m / s.row_m);  // a drill's row wanders some 4 cm (across_m)
            for (auto k = static_cast<std::int64_t>(std::floor(vmin / s.row_m)) - 1; k <= static_cast<std::int64_t>(std::floor(vmax / s.row_m)) + 1; ++k) {
                for (auto j = static_cast<std::int64_t>(std::floor(umin / along)) - 1; j <= static_cast<std::int64_t>(std::floor(umax / along)) + 1; ++j) {
                    const std::uint64_t h = Hash(j, k, hs, 11u);
                    const double u = (static_cast<double>(j) + 0.5 + 0.6 * (Unit(h) - 0.5)) * along;
                    const double v = (static_cast<double>(k) + 0.5 + 2.0 * jitter_v * (Unit(recipe_detail::Mix(h ^ 1u)) - 0.5)) * s.row_m;
                    const double e = u * sh + v * ch, n = u * ch - v * sh;
                    if (!inside(e, n)) continue;
                    emit(e, n, Unit(recipe_detail::Mix(h ^ 2u)), h);
                }
            }
            break;
        }
        case ScatterProcess::Thomas:
        case ScatterProcess::Matern: {
            // Parents on their own grid of squares, looked for within the offspring's reach of the cell.
            const bool thomas = s.process == ScatterProcess::Thomas;
            const double reach = thomas ? 3.0 * s.spread_m : s.spread_m;
            const double g = std::max(10.0, 4.0 * s.spread_m);
            const double mu = s.per_ha / s.kappa_per_ha;
            const std::int64_t i0 = static_cast<std::int64_t>(std::floor((e0 - reach) / g)), i1 = static_cast<std::int64_t>(std::floor((e1 + reach) / g));
            const std::int64_t j0 = static_cast<std::int64_t>(std::floor((n0 - reach) / g)), j1 = static_cast<std::int64_t>(std::floor((n1 + reach) / g));
            const std::uint64_t ps = salt ^ 0x5A5A5A5A5A5A5A5Aull;
            for (std::int64_t j = j0; j <= j1; ++j) {
                for (std::int64_t i = i0; i <= i1; ++i) {
                    const int parents = static_cast<int>(std::floor(s.kappa_per_ha * g * g / 10000.0 + Unit(Hash(i, j, ps, 0))));
                    for (int p = 0; p < parents; ++p) {
                        const std::uint64_t hp = Hash(i, j, ps, 16u + static_cast<std::uint64_t>(p));
                        const double pe = (static_cast<double>(i) + Unit(hp)) * g, pn = (static_cast<double>(j) + Unit(recipe_detail::Mix(hp ^ 1u))) * g;
                        if (pe < e0 - reach || pe >= e1 + reach || pn < n0 - reach || pn >= n1 + reach) continue;
                        const int children = static_cast<int>(std::floor(mu + Unit(recipe_detail::Mix(hp ^ 2u))));
                        for (int c = 0; c < children; ++c) {
                            const std::uint64_t h = Hash(i, j, ps, (static_cast<std::uint64_t>(p) << 20) + 4096u + static_cast<std::uint64_t>(c));
                            const double u1 = Unit(h), u2 = Unit(recipe_detail::Mix(h ^ 1u));
                            const double r = thomas ? s.spread_m * std::sqrt(-2.0 * std::log(1.0 - u1)) : s.spread_m * std::sqrt(u1);
                            const double e = pe + r * std::cos(2.0 * kPi * u2), n = pn + r * std::sin(2.0 * kPi * u2);
                            if (!inside(e, n)) continue;
                            emit(e, n, Unit(recipe_detail::Mix(h ^ 2u)), h);
                        }
                    }
                }
            }
            break;
        }
    }
}

// The candidates of the cell (cx, cy) of side `cell_m` on the day: the recipe of the ground layer under each, in the band
// it lies in. `keep(east, north, threshold)` is asked first (a ring's distance and falloff), before the layer and the band
// are looked up. Appends to `out`.
template <class Keep>
inline void RecipeCandidates(const RecipeSet& set, const CoverRaster& cover, const DressingRules& dressing, const GroundLayerRules& layers,
                             int day_of_year, std::int64_t cx, std::int64_t cy, double cell_m, Keep&& keep, std::vector<RecipeCandidate>& out) {
    const double ce = (static_cast<double>(cx) + 0.5) * cell_m, cn = (static_cast<double>(cy) + 0.5) * cell_m, h = 0.5 * cell_m;
    // The layers under the cell: its centre and corners, the recipes to draw from; each candidate is then checked against
    // the layer at its own point, so a point is the same whatever the cell's size (a layer no sample falls on, a sliver
    // under a cell's width, is left out).
    int sample[5];
    sample[0] = PointLayer(cover, dressing, layers, ce, cn, day_of_year);
    sample[1] = PointLayer(cover, dressing, layers, ce - h, cn - h, day_of_year);
    sample[2] = PointLayer(cover, dressing, layers, ce + h, cn - h, day_of_year);
    sample[3] = PointLayer(cover, dressing, layers, ce - h, cn + h, day_of_year);
    sample[4] = PointLayer(cover, dressing, layers, ce + h, cn + h, day_of_year);
    bool mixed = false;
    for (int k = 1; k < 5; ++k) mixed = mixed || sample[k] != sample[0];
    for (int si = 0; si < (mixed ? 5 : 1); ++si) {
        const int layer = sample[si];
        bool seen = false;
        for (int p = 0; p < si; ++p) seen = seen || sample[p] == layer;
        if (seen) continue;
        const Recipe* recipe = set.ForLayer(layer);
        if (recipe == nullptr) continue;
        const int ri = set.by_layer[static_cast<std::size_t>(layer)];
        const bool edges = recipe->HasEdgeBands();
        const double wood_reach = recipe->WoodReachM();
        // The furrows' heading at the cell (rows run along it); a candidate in another estate's heading is not this lattice's.
        FieldCell cell_field;
        const double cell_heading = ParcelAt(cover, ce, cn, cell_field) ? cell_field.heading_rad : std::numeric_limits<double>::quiet_NaN();
        for (std::size_t b = 0; b < recipe->bands.size(); ++b) {
            const RecipeBand& band = recipe->bands[b];
            for (std::size_t s = 0; s < band.scatter.size(); ++s) {
                const std::uint64_t salt = recipe->salt ^ ((static_cast<std::uint64_t>(b) * 131u + static_cast<std::uint64_t>(s) * 7u + 1u) * 0x9E3779B97F4A7C15ull);
                const bool rows = band.scatter[s].process == ScatterProcess::Rows;
                ScatterInCell(band.scatter[s], salt, cx, cy, cell_m, [&](double e, double n, double threshold, std::uint64_t hash) {
                    if (!keep(e, n, threshold)) return;
                    if (PointLayer(cover, dressing, layers, e, n, day_of_year) != layer) return;
                    FieldCell f;
                    const bool on_field = ParcelAt(cover, e, n, f);
                    if (rows && (!on_field || f.heading_rad != cell_heading)) return;
                    const std::vector<int>& crops = band.scatter[s].crops;
                    if (!crops.empty() && (!on_field || std::find(crops.begin(), crops.end(), CropOf(dressing, f.id)) == crops.end())) return;
                    if (edges) {
                        double ends = std::numeric_limits<double>::infinity(), sides = ends;
                        if (on_field) ParcelEdgeDistance(f, e, n, ends, sides);
                        const double wood = wood_reach > 0.0 ? WoodEdgeDistance(cover, e, n, wood_reach) : std::numeric_limits<double>::infinity();
                        if (RecipeBandAt(*recipe, ends, sides, wood) != static_cast<int>(b)) return;
                    }
                    RecipeCandidate c;
                    c.east_m = e;
                    c.north_m = n;
                    c.threshold = threshold;
                    c.hash = hash;
                    c.recipe = ri;
                    c.band = static_cast<int>(b);
                    c.scatter = static_cast<int>(s);
                    c.kind = band.scatter[s].kind_index;
                    c.on_field = on_field;
                    c.heading_rad = on_field ? f.heading_rad : 0.0;
                    c.stand_m = on_field ? layers.StandAtPoint(CropOf(dressing, f.id), f.id, day_of_year, e, n) : 0.0;
                    out.push_back(c);
                }, cell_heading);
            }
        }
    }
}

// The report over a rectangle (roadmap L2): each recipe's bands' area, measured on a lattice of `step_m`, and the
// instances of each kind a hectare at full density, against the recipe's targets.
struct RecipeMeasureRow {
    int recipe = -1, band = -1, kind = -1;
    double area_ha = 0.0;
    std::size_t count = 0;
    double per_ha = 0.0;
    bool has_target = false, on_target = false;
    double lo_per_ha = 0.0, hi_per_ha = 0.0;
};

inline std::vector<RecipeMeasureRow> RecipeMeasure(const RecipeSet& set, const std::vector<std::string>& kind_ids, const CoverRaster& cover,
                                                   const DressingRules& dressing, const GroundLayerRules& layers, int day_of_year, double east0_m,
                                                   double north0_m, double east1_m, double north1_m, double cell_m = 4.0, double step_m = 1.0) {
    std::vector<RecipeMeasureRow> rows;
    const auto row_of = [&](int recipe, int band, int kind) -> RecipeMeasureRow& {
        for (RecipeMeasureRow& r : rows) {
            if (r.recipe == recipe && r.band == band && r.kind == kind) return r;
        }
        RecipeMeasureRow r;
        r.recipe = recipe;
        r.band = band;
        r.kind = kind;
        rows.push_back(r);
        return rows.back();
    };
    // Areas: each band's lattice points; a scatter kept to some crops, its own (the band's points on those crops' fields).
    std::vector<std::vector<double>> area(set.recipes.size());
    std::vector<std::vector<std::vector<double>>> crop_area(set.recipes.size());
    for (std::size_t i = 0; i < set.recipes.size(); ++i) {
        area[i].assign(set.recipes[i].bands.size(), 0.0);
        crop_area[i].resize(set.recipes[i].bands.size());
        for (std::size_t b = 0; b < set.recipes[i].bands.size(); ++b) crop_area[i][b].assign(set.recipes[i].bands[b].scatter.size(), 0.0);
    }
    for (double n = north0_m + 0.5 * step_m; n < north1_m; n += step_m) {
        for (double e = east0_m + 0.5 * step_m; e < east1_m; e += step_m) {
            const int layer = PointLayer(cover, dressing, layers, e, n, day_of_year);
            const Recipe* r = set.ForLayer(layer);
            if (r == nullptr) continue;
            double ends = 0.0, sides = 0.0;
            if (r->HasEdgeBands()) ParcelEdgeDistance(cover, e, n, ends, sides);
            const double wood = r->WoodReachM() > 0.0 ? WoodEdgeDistance(cover, e, n, r->WoodReachM()) : std::numeric_limits<double>::infinity();
            const int b = RecipeBandAt(*r, ends, sides, wood);
            if (b < 0) continue;
            const std::size_t ri = static_cast<std::size_t>(set.by_layer[static_cast<std::size_t>(layer)]);
            area[ri][static_cast<std::size_t>(b)] += step_m * step_m / 10000.0;
            const RecipeBand& band = r->bands[static_cast<std::size_t>(b)];
            FieldCell f;
            int crop = -2;
            for (std::size_t s = 0; s < band.scatter.size(); ++s) {
                if (band.scatter[s].crops.empty()) continue;
                if (crop == -2) crop = ParcelAt(cover, e, n, f) ? CropOf(dressing, f.id) : -1;
                if (std::find(band.scatter[s].crops.begin(), band.scatter[s].crops.end(), crop) != band.scatter[s].crops.end()) {
                    crop_area[ri][static_cast<std::size_t>(b)][s] += step_m * step_m / 10000.0;
                }
            }
        }
    }
    // Counts: every candidate at full density.
    std::vector<RecipeCandidate> found;
    const std::int64_t c0 = static_cast<std::int64_t>(std::floor(east0_m / cell_m)), c1 = static_cast<std::int64_t>(std::floor(east1_m / cell_m));
    const std::int64_t r0 = static_cast<std::int64_t>(std::floor(north0_m / cell_m)), r1 = static_cast<std::int64_t>(std::floor(north1_m / cell_m));
    for (std::int64_t cy = r0; cy <= r1; ++cy) {
        for (std::int64_t cx = c0; cx <= c1; ++cx) {
            RecipeCandidates(set, cover, dressing, layers, day_of_year, cx, cy, cell_m,
                             [&](double e, double n, double) { return e >= east0_m && e < east1_m && n >= north0_m && n < north1_m; }, found);
        }
    }
    for (const RecipeCandidate& c : found) ++row_of(c.recipe, c.band, c.kind).count;
    for (std::size_t ri = 0; ri < set.recipes.size(); ++ri) {
        const Recipe& r = set.recipes[ri];
        for (const RecipeTarget& t : r.targets) {
            int b = -1, k = -1;
            for (std::size_t i = 0; i < r.bands.size(); ++i) {
                if (r.bands[i].id == t.band) b = static_cast<int>(i);
            }
            for (std::size_t i = 0; i < kind_ids.size(); ++i) {
                if (kind_ids[i] == t.kind) k = static_cast<int>(i);
            }
            if (b < 0 || area[ri][static_cast<std::size_t>(b)] <= 0.0) continue;
            RecipeMeasureRow& row = row_of(static_cast<int>(ri), b, k);
            row.has_target = true;
            row.lo_per_ha = t.lo_per_ha;
            row.hi_per_ha = t.hi_per_ha;
        }
    }
    for (RecipeMeasureRow& row : rows) {
        const std::size_t ri = static_cast<std::size_t>(row.recipe), bi = static_cast<std::size_t>(row.band);
        row.area_ha = area[ri][bi];
        const RecipeBand& band = set.recipes[ri].bands[bi];
        for (std::size_t s = 0; s < band.scatter.size(); ++s) {
            if (band.scatter[s].kind_index == row.kind && !band.scatter[s].crops.empty()) row.area_ha = crop_area[ri][bi][s];
        }
        if (row.area_ha <= 0.0) row.has_target = false;  // none of its crop's ground in the rectangle: nothing to measure
        row.per_ha = row.area_ha > 0.0 ? static_cast<double>(row.count) / row.area_ha : 0.0;
        row.on_target = row.has_target && row.per_ha >= row.lo_per_ha && row.per_ha <= row.hi_per_ha;
    }
    return rows;
}

}  // namespace scenery
}  // namespace simcore
