// The map's dressing (simcore/scenery/map_dressing.hpp, roadmap R6): the land cover, the trees in the woods and the woods'
// masses, a building's variant. Deterministic; no tree above what blocks a sight line, near a line, in the water or
// outside the map proper and its detail areas; a budget of trees.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "simcore/scenery/map_dressing.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::scenery;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

// Built once for the file (the map takes seconds to lay out), the dressing too.
const MapData* Kesmark() {
    static std::shared_ptr<const MapData> map;
    if (!map) {
        std::string e;
        map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
        if (!map) std::printf("    %s\n", e.c_str());
    }
    return map.get();
}

const DressingRules& Rules() {
    static DressingRules rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadDressingRules(kData + "/scenery/map_dressing.json", rules, &e)) std::printf("    %s\n", e.c_str());
    }
    return rules;
}

const MapDressing& Dressing() {
    static MapDressing d;
    static bool built = false;
    if (!built && Kesmark() != nullptr) {
        built = true;
        d = PlaceTrees(*Kesmark(), Rules());
    }
    return d;
}

// The distance from (e, n) to the nearest edge of any of the lines (their half widths off).
double FromLines(const std::vector<MapLineDef>& lines, double e, double n) {
    double best = 1e18;
    for (const MapLineDef& l : lines) best = std::min(best, mapgeo::DistToLine(l.points, e, n) - 0.5 * l.width_m);
    return best;
}

}  // namespace

SC_TEST("map dressing: the rules load from the data and keep their bounds") {
    DressingRules r;
    std::string e;
    SC_EXPECT(LoadDressingRules(kData + "/scenery/map_dressing.json", r, &e));
    SC_EXPECT_NEAR(r.near_spacing_m, 7.0, 1e-9);
    SC_EXPECT(r.edge_rows == 2);
    SC_EXPECT(r.crops.size() == 5);
    SC_EXPECT(r.crops[0].id == "wheat");
    SC_EXPECT_NEAR(r.cover_colours[static_cast<std::size_t>(Cover::Water)][2], 0.15, 1e-6);
    // A bad file leaves the rules as they were.
    DressingRules kept = r;
    SC_EXPECT(!LoadDressingRules(R"({"values": {"near_spacing_m": {"value": -1}}})", kept, &e));
    SC_EXPECT_NEAR(kept.near_spacing_m, 7.0, 1e-9);
    SC_EXPECT(!LoadDressingRules(R"({"values": {"species_shares": {"value": {"birch": 1}}}})", kept, &e));
    SC_EXPECT(!LoadDressingRules(R"({"values": {"crops": {"value": [{"id": "rye", "share": 1, "colour": [2, 0, 0]}]}}})", kept, &e));
    SC_EXPECT(!LoadDressingRules(R"({"values": {"cover_colours": {"value": {"field": [0.1, 0.1, 0.1]}}}})", kept, &e));
    SC_EXPECT(kept.crops.size() == 5);
}

SC_TEST("map dressing: every field cell grows a crop, its parcel the same each time") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const DressingRules& r = Rules();
    const CoverRaster a = BakeCover(*m, r);
    SC_EXPECT(a.grids.size() == m->Def().detail.size() + 1);
    std::vector<std::size_t> per_cover(static_cast<std::size_t>(Cover::Count), 0);
    std::set<int> crops;
    for (const CoverGrid& g : a.grids) {
        for (std::size_t i = 0; i < g.cover.size(); ++i) {
            ++per_cover[g.cover[i]];
            SC_EXPECT(g.crop[i] < r.crops.size());
            if (g.cover[i] == static_cast<std::uint8_t>(Cover::Field)) crops.insert(g.crop[i]);
        }
    }
    std::printf("    cover cells:");
    for (std::size_t c = 0; c < per_cover.size(); ++c) std::printf(" %s %zu", CoverName(static_cast<Cover>(c)), per_cover[c]);
    std::printf("\n");
    SC_EXPECT(crops.size() == r.crops.size());  // every crop grown somewhere
    for (std::size_t c = 0; c < per_cover.size(); ++c) SC_EXPECT(per_cover[c] > 0);
    // Fields are parcels, not noise: most cells' neighbours a cell east lie in the same parcel.
    const std::uint64_t seed = 12345;
    int same = 0, total = 0;
    for (double n = -15000.0; n < 15000.0; n += 731.0) {
        for (double e = -15000.0; e < 15000.0; e += 617.0) {
            ++total;
            same += FieldParcel(r, seed, e, n) == FieldParcel(r, seed, e + 25.0, n) ? 1 : 0;
            SC_EXPECT(FieldParcel(r, seed, e, n) == FieldParcel(r, seed, e, n));
        }
    }
    SC_EXPECT(same > total * 7 / 10);
    // Baked again: the same raster.
    const CoverRaster b = BakeCover(*m, r);
    bool equal = a.grids.size() == b.grids.size();
    for (std::size_t i = 0; equal && i < a.grids.size(); ++i) equal = a.grids[i].cover == b.grids[i].cover && a.grids[i].crop == b.grids[i].crop;
    SC_EXPECT(equal);
    // What lies where the map laid it: a wood's middle is forest, a river's water, a house in a town.
    int forest = 0, water = 0, town = 0, checked_forest = 0, checked_town = 0;
    for (const MapBox& bx : m->Layout().boxes) {
        if (!a.Has(bx.east_m, bx.north_m)) continue;
        if (bx.part == MapPart::Canopy && checked_forest < 200) {
            ++checked_forest;
            forest += a.At(bx.east_m, bx.north_m) == Cover::Forest || a.At(bx.east_m, bx.north_m) == Cover::Road ||
                              a.At(bx.east_m, bx.north_m) == Cover::Water
                          ? 1
                          : 0;
        }
        if (bx.part == MapPart::House && checked_town < 200) {
            ++checked_town;
            // A house on a street may lie in the road's cell (a road is at least a cell wide).
            town += a.At(bx.east_m, bx.north_m) == Cover::Town || a.At(bx.east_m, bx.north_m) == Cover::Road ? 1 : 0;
        }
    }
    SC_EXPECT(checked_forest == 200 && forest == checked_forest);
    SC_EXPECT(checked_town == 200 && town >= 190);
    for (const MapStrip& s : m->Layout().strips) {
        if (s.surface != MapSurface::Water || s.points.size() < 3) continue;
        const Vec3& p = s.points[s.points.size() / 2];
        if (a.Has(p.x, p.y)) water += a.At(p.x, p.y) == Cover::Water ? 1 : 0;
        else water += 1;
    }
    SC_EXPECT(water > 0);
}

SC_TEST("map dressing: the trees are planted the same each time, within budget, only in the map proper and the areas") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const MapDressing& d = Dressing();
    const MapDressing again = PlaceTrees(*m, Rules());
    SC_EXPECT(d.trees.size() == again.trees.size());
    SC_EXPECT(d.masses.size() == again.masses.size());
    if (!d.trees.empty() && d.trees.size() == again.trees.size()) {
        const TreeInstance& x = d.trees.back();
        const TreeInstance& y = again.trees.back();
        SC_EXPECT(x.east_m == y.east_m && x.north_m == y.north_m && x.base_m == y.base_m && x.height_m == y.height_m && x.kind == y.kind);
    }
    std::size_t in_areas = 0;
    std::vector<std::size_t> kinds(static_cast<std::size_t>(TreeKind::Count), 0);
    for (const TreeInstance& t : d.trees) {
        const bool inside = m->Def().InCore(t.east_m, t.north_m) || m->InDetail(t.east_m, t.north_m);
        SC_EXPECT(inside);
        in_areas += m->InDetail(t.east_m, t.north_m) ? 1 : 0;
        SC_EXPECT(t.kind < static_cast<std::uint8_t>(TreeKind::Count));
        if (t.kind < kinds.size()) ++kinds[t.kind];
    }
    std::printf("    %zu trees (%zu in the detail areas: oak %zu, poplar %zu, pine %zu, acacia %zu), %zu masses\n", d.trees.size(), in_areas,
                kinds[0], kinds[1], kinds[2], kinds[3], d.masses.size());
    SC_EXPECT(d.trees.size() < 400000);
    SC_EXPECT(d.trees.size() > 50000);
    SC_EXPECT(in_areas > 10000);
    for (std::size_t k : kinds) SC_EXPECT(k > 0);
    // Every wood block not planted whole is a mass; the far ground's (400 m) among them.
    std::size_t canopy = 0, whole = 0, far_masses = 0;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::Canopy) continue;
        ++canopy;
        whole += m->InDetail(b.east_m, b.north_m) ? 1 : 0;
    }
    for (const CanopyMass& c : d.masses) {
        far_masses += c.length_m > 300.0 ? 1 : 0;
        SC_EXPECT(!m->InDetail(c.east_m, c.north_m));
        SC_EXPECT(c.variant < kCanopyMassVariants);
    }
    SC_EXPECT(d.masses.size() == canopy - whole);
    SC_EXPECT(far_masses > 1000);
}

SC_TEST("map dressing: no tree tops what blocks a sight line, stands near a line or in the water") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const MapDressing& d = Dressing();
    const DressingRules& r = Rules();
    int over = 0, short_trees = 0, near_line = 0, wet = 0, checked = 0;
    std::vector<MapLineDef> lines = m->Def().roads;
    lines.insert(lines.end(), m->Def().railways.begin(), m->Def().railways.end());
    lines.insert(lines.end(), m->Def().rivers.begin(), m->Def().rivers.end());
    const MapData::DetailLines& imported = m->ImportedLines();
    for (const std::vector<MapLineDef>* l : {&imported.roads, &imported.railways, &imported.rivers}) lines.insert(lines.end(), l->begin(), l->end());
    std::vector<MapLineDef> rivers = m->Def().rivers;
    rivers.insert(rivers.end(), imported.rivers.begin(), imported.rivers.end());
    double mean_height = 0.0;
    for (std::size_t i = 0; i < d.trees.size(); ++i) {
        const TreeInstance& t = d.trees[i];
        const double top = t.base_m + t.height_m;
        const double limit = m->GroundHeight(t.east_m, t.north_m) + m->ObstacleHeight(t.east_m, t.north_m) + 0.3;
        over += top > limit + 1e-3 ? 1 : 0;
        short_trees += t.height_m < 3.0f ? 1 : 0;
        SC_EXPECT_NEAR(t.base_m, m->GroundHeight(t.east_m, t.north_m) - 0.3, 1e-6);
        SC_EXPECT(t.crown_m > 0.0f && t.crown_m < t.height_m);
        mean_height += t.height_m;
        if (i % 97 == 0) {  // a sample, against the lines themselves
            ++checked;
            near_line += FromLines(lines, t.east_m, t.north_m) < r.line_clear_m - 1e-6 ? 1 : 0;
            wet += FromLines(rivers, t.east_m, t.north_m) < 0.0 ? 1 : 0;
        }
    }
    if (!d.trees.empty()) mean_height /= static_cast<double>(d.trees.size());
    std::printf("    mean height %.1f m; %d trees checked against the lines\n", mean_height, checked);
    SC_EXPECT(over == 0);
    SC_EXPECT(short_trees == 0);
    SC_EXPECT(near_line == 0);
    SC_EXPECT(wet == 0);
    SC_EXPECT(checked > 500);
    SC_EXPECT(mean_height > 0.8 * m->Def().canopy_min_m && mean_height < m->Def().canopy_max_m + 1.0);
}

SC_TEST("map dressing: a wood's open side has its rows of trees, a wood in a detail area is planted whole") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const MapDressing& d = Dressing();
    const DressingRules& r = Rules();
    // The trees by 100 m block, to count them per block.
    const auto in_block = [](const TreeInstance& t, const MapBox& b) {
        return std::fabs(t.east_m - b.east_m) <= 0.5 * b.length_m + 4.0 && std::fabs(t.north_m - b.north_m) <= 0.5 * b.width_m + 4.0;
    };
    // A core block with an open north side, far from every line: both rows along it.
    int found_edge = 0, found_whole = 0;
    const auto block_at = [&](double e, double n) {
        for (const MapBox& o : m->Layout().boxes) {
            if (o.part == MapPart::Canopy && std::fabs(o.east_m - e) < 1.0 && std::fabs(o.north_m - n) < 1.0) return true;
        }
        return false;
    };
    for (const MapBox& b : m->Layout().boxes) {
        if (found_edge >= 3 && found_whole >= 3) break;
        if (b.part != MapPart::Canopy || !m->Def().InCore(b.east_m, b.north_m) || m->NearLine(b.east_m, b.north_m, 100.0)) continue;
        if (m->NearWater(b.east_m, b.north_m, 110.0)) continue;
        const bool whole = m->InDetail(b.east_m, b.north_m);
        if (whole && found_whole < 3) {
            int count = 0;
            for (const TreeInstance& t : d.trees) count += in_block(t, b) ? 1 : 0;
            const int expected = static_cast<int>(std::floor(b.length_m / r.near_spacing_m)) * static_cast<int>(std::floor(b.width_m / r.near_spacing_m));
            SC_EXPECT(count >= expected * 9 / 10);
            ++found_whole;
            continue;
        }
        if (whole || found_edge >= 3 || block_at(b.east_m, b.north_m + b.width_m)) continue;
        int row0 = 0, row1 = 0, inside = 0;
        const double top = b.north_m + 0.5 * b.width_m;
        for (const TreeInstance& t : d.trees) {
            if (!in_block(t, b)) continue;
            const double in = top - t.north_m;
            if (in > 0.0 && in < r.edge_spacing_m) ++row0;
            else if (in >= r.edge_spacing_m && in < 2.0 * r.edge_spacing_m) ++row1;
            else if (in > 2.0 * r.edge_spacing_m + 4.0 && std::fabs(t.east_m - b.east_m) < 0.5 * b.length_m - 2.0 * r.edge_spacing_m - 4.0) ++inside;
        }
        const int per_row = static_cast<int>(std::floor(b.length_m / r.edge_spacing_m));
        SC_EXPECT(row0 >= per_row * 7 / 10);
        SC_EXPECT(row1 >= per_row * 7 / 10);
        SC_EXPECT(inside == 0);  // its inside is a mass
        ++found_edge;
    }
    SC_EXPECT(found_edge == 3);
    SC_EXPECT(found_whole == 3);
}

SC_TEST("map dressing: a building's variant and part by its place") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    std::vector<int> per_variant(kHouseVariants, 0);
    int flats = 0;
    for (const MapBox& b : m->Layout().boxes) {
        int v = -1;
        const std::string part = BuildingPart(b, v);
        if (b.part == MapPart::House) {
            SC_EXPECT(part == "house" && v >= 0 && v < kHouseVariants);
            if (v >= 0 && v < kHouseVariants) ++per_variant[static_cast<std::size_t>(v)];
            SC_EXPECT(v == BuildingVariant(b));
        } else if (b.part == MapPart::Flats) {
            SC_EXPECT(part == "flats" && v >= 1 && v <= kFlatsMaxFloors);
            ++flats;
        } else if (b.part == MapPart::Farm) {
            SC_EXPECT(part == "barn");
        } else if (b.part == MapPart::Industrial) {
            SC_EXPECT(part == "industrial");
        } else {
            SC_EXPECT(part.empty());
        }
    }
    for (int c : per_variant) SC_EXPECT(c > 100);
    SC_EXPECT(flats > 100);
    MapBox b;
    b.part = MapPart::Flats;
    b.height_m = 30.5;  // a 30 m block sunk half a metre
    SC_EXPECT(FlatsFloors(b) == 11);
}
