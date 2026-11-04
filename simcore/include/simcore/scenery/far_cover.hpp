// simcore/scenery/far_cover.hpp — the land cover of the far ground (roadmap T3.2, docs/scenery/T3_T4_PLAN.md): the country's
// land cover (CountryMap::LandCoverAt: lakes, rivers, marshes, forests, towns, open land) over the whole far ground at 250 m
// cells, as one CoverGrid in the map's frame that the ground material reads like the map proper's. The open land is farmland:
// the map's field grid (farm_fields.hpp) is closed-form and goes on past the map proper, so the far ground's fields are the
// same fields at the same scale, each with its crop, and the map proper and the far ground meet without a seam.
//
// Presentation only: nothing in the simulation reads it. A function of the map, the country and the rules alone, so every
// client bakes the same grid.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "simcore/country_map.hpp"
#include "simcore/scenery/map_dressing.hpp"
#include "simcore/terrain_map.hpp"

namespace simcore {
namespace scenery {

// The cover a country's land cover name stands for (a field for open land: its crop is the field's).
inline Cover CoverOfLand(const char* land) {
    if (std::strcmp(land, "lake") == 0 || std::strcmp(land, "river") == 0) return Cover::Water;
    if (std::strcmp(land, "marsh") == 0) return Cover::Marsh;
    if (std::strcmp(land, "forest") == 0) return Cover::Forest;
    if (std::strcmp(land, "town") == 0) return Cover::Town;
    return Cover::Field;
}

// The far ground's land cover over the map's far extent (MapDef::far_m each way from the origin), `cell_m` cells.
inline CoverGrid BakeFarCover(const MapData& map, const CountryMap& country, const DressingRules& rules, double cell_m = 250.0) {
    const MapDef& def = map.Def();
    // The map's frame to the country's: a map point seen from the country's origin (the same link the ground is drawn by).
    const MapFrameLink to_country(country.Def().ground.origin, def.origin);
    CoverGrid g;
    g.id = "far";
    g.fields = map.Fields();
    g.cell_m = cell_m;
    g.west_m = -def.far_m;
    g.south_m = -def.far_m;
    g.cols = std::max(1, static_cast<int>(std::ceil(2.0 * def.far_m / cell_m - 1e-9)));
    g.rows = g.cols;
    const std::size_t count = static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows);
    g.cover.assign(count, static_cast<std::uint8_t>(Cover::Field));
    g.crop.assign(count, 0);
    g.strips.assign(count, 0);
    for (int r = 0; r < g.rows; ++r) {
        for (int c = 0; c < g.cols; ++c) {
            const double e = g.CentreEast(c), n = g.CentreNorth(r);
            const MapPoint p = to_country.FrameToMap(e, n);
            const std::size_t i = g.Index(c, r);
            g.cover[i] = static_cast<std::uint8_t>(CoverOfLand(country.LandCoverAt(p.east_m, p.north_m)));
            g.crop[i] = CropOf(rules, FieldAt(g.fields, e, n).id);
        }
    }
    return g;
}

}  // namespace scenery
}  // namespace simcore
