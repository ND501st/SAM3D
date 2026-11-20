// simcore/clutter.hpp — ITU-R P.1812's representative clutter (roadmap R6, S9; docs/EM_ARCHITECTURE.md section 27): the
// ground cover's categories and the height each adds to a path's profile where no wood or building of its own stands.
//
// P.1812-6 (09/2021) section 3.2 "Radio path profile", equation (1c) and Table 2 "Default representative clutter height
// values" (the same table in P.1812-8, 09/2025, section 3.2.1): water/sea 0 m, open/rural 0 m, suburban 10 m,
// urban/trees/forest 15 m, dense urban 20 m, added to the profile's intermediate points (not at either terminal). The
// heights are data (data/gameplay/clutter.json); the categories are the recommendation's.
//
// Used where the ground carries no footprints of its own: the far ground's towns the map does not lay out
// (terrain_map.hpp MapData::ObstacleHeight) and the country's own ground under the radios of the information pipeline
// (command/em_radio.hpp CountryGround), a class per cell from the country's land cover (ClutterRaster).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"

namespace simcore {

// P.1812 Table 2's categories.
enum class ClutterClass : std::uint8_t { WaterSea, OpenRural, Suburban, UrbanTreesForest, DenseUrban, Count };

inline const char* ClutterClassName(ClutterClass c) {
    switch (c) {
    case ClutterClass::WaterSea: return "water/sea";
    case ClutterClass::OpenRural: return "open/rural";
    case ClutterClass::Suburban: return "suburban";
    case ClutterClass::UrbanTreesForest: return "urban/trees/forest";
    case ClutterClass::DenseUrban: return "dense urban";
    case ClutterClass::Count: break;
    }
    return "?";
}

// The representative clutter height of each category (data/gameplay/clutter.json), and how much of a town's radius is
// its dense middle (a game value). All 0 by default: no clutter, as a terrain without the file.
struct ClutterHeights {
    double water_sea_m = 0.0;
    double open_rural_m = 0.0;
    double suburban_m = 0.0;
    double urban_trees_forest_m = 0.0;
    double dense_urban_m = 0.0;
    double dense_share = 0.45;

    double HeightOf(ClutterClass c) const {
        switch (c) {
        case ClutterClass::WaterSea: return water_sea_m;
        case ClutterClass::OpenRural: return open_rural_m;
        case ClutterClass::Suburban: return suburban_m;
        case ClutterClass::UrbanTreesForest: return urban_trees_forest_m;
        case ClutterClass::DenseUrban: return dense_urban_m;
        case ClutterClass::Count: break;
        }
        return 0.0;
    }
    double Tallest() const { return std::max({0.0, water_sea_m, open_rural_m, suburban_m, urban_trees_forest_m, dense_urban_m}); }
};

// A town's cover at `dist_m` from its middle (radius `radius_m`): a capital's, a city's or a town's middle (within
// `dense_share` of its radius) dense urban, the rest of it suburban; a village suburban throughout; beyond its radius
// open/rural (the caller's land cover decides what else stands there).
inline ClutterClass TownClutterClass(const std::string& kind, double dist_m, double radius_m, double dense_share) {
    if (dist_m >= radius_m) return ClutterClass::OpenRural;
    if (kind != "village" && dist_m < dense_share * radius_m) return ClutterClass::DenseUrban;
    return ClutterClass::Suburban;
}

// A class per square cell, west to east in a row, rows south to north; outside it open/rural.
struct ClutterRaster {
    double west_m = 0.0, south_m = 0.0, cell_m = 250.0;
    int cols = 0, rows = 0;
    std::vector<std::uint8_t> cells;

    bool Contains(double e, double n) const {
        return cols > 0 && rows > 0 && e >= west_m && n >= south_m && e < west_m + cell_m * cols && n < south_m + cell_m * rows;
    }
    ClutterClass ClassAt(double e, double n) const {
        if (!Contains(e, n)) return ClutterClass::OpenRural;
        const int c = std::min(cols - 1, static_cast<int>((e - west_m) / cell_m));
        const int r = std::min(rows - 1, static_cast<int>((n - south_m) / cell_m));
        return static_cast<ClutterClass>(cells[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(c)]);
    }
    double CentreEast(int c) const { return west_m + (c + 0.5) * cell_m; }
    double CentreNorth(int r) const { return south_m + (r + 0.5) * cell_m; }
    void Set(int c, int r, ClutterClass k) {
        cells[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(k);
    }
    // The cells whose centres lie in [w, e) x [s, n), clamped to the raster: [c0, c1] x [r0, r1]; false if none.
    bool CellsIn(double w, double s, double e, double n, int& c0, int& c1, int& r0, int& r1) const {
        c0 = std::max(0, static_cast<int>(std::ceil((w - west_m) / cell_m - 0.5)));
        c1 = std::min(cols - 1, static_cast<int>(std::floor((e - west_m) / cell_m - 0.5)));
        r0 = std::max(0, static_cast<int>(std::ceil((s - south_m) / cell_m - 0.5)));
        r1 = std::min(rows - 1, static_cast<int>(std::floor((n - south_m) / cell_m - 0.5)));
        return c0 <= c1 && r0 <= r1;
    }
};

// Reads clutter.json's text: its values into `out` (each {"value": ...}); false with a message if it does not parse or a
// value is missing or negative (`out` then untouched).
inline bool ParseClutterHeights(const std::string& text, ClutterHeights& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) {
        if (error) *error = "gameplay/clutter.json: " + parse_error;
        return false;
    }
    const json::Value* values = root.Find("values");
    if (values == nullptr) {
        if (error) *error = "gameplay/clutter.json: no \"values\"";
        return false;
    }
    ClutterHeights h;
    const std::pair<const char*, double*> keys[] = {{"water_sea_m", &h.water_sea_m},
                                                     {"open_rural_m", &h.open_rural_m},
                                                     {"suburban_m", &h.suburban_m},
                                                     {"urban_trees_forest_m", &h.urban_trees_forest_m},
                                                     {"dense_urban_m", &h.dense_urban_m},
                                                     {"dense_share", &h.dense_share}};
    for (const auto& k : keys) {
        const json::Value* v = values->Find(k.first);
        const json::Value* n = v != nullptr ? v->Find("value") : nullptr;
        if (n == nullptr || !n->Is(json::Type::Number) || n->number < 0.0) {
            if (error) *error = std::string("gameplay/clutter.json: values.") + k.first + " must be a number of 0 or more";
            return false;
        }
        *k.second = n->number;
    }
    if (h.dense_share > 1.0) {
        if (error) *error = "gameplay/clutter.json: values.dense_share must be 0 to 1";
        return false;
    }
    out = h;
    return true;
}

// Reads data/gameplay/clutter.json into `out`. A missing file is not an error: `out` is left as it was (no clutter).
inline bool LoadClutterHeights(const std::string& data_dir, ClutterHeights& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(data_dir + "/gameplay/clutter.json", text)) return true;
    return ParseClutterHeights(text, out, error);
}

}  // namespace simcore
