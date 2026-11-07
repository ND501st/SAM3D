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

#include "simcore/radar.hpp"
#include "simcore/scenery/map_dressing.hpp"
#include "simcore/scenery/recipes.hpp"
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
    const FieldGrid fg = m->Fields();
    int same = 0, total = 0;
    for (double n = -15000.0; n < 15000.0; n += 731.0) {
        for (double e = -15000.0; e < 15000.0; e += 617.0) {
            ++total;
            same += FieldParcel(fg, e, n) == FieldParcel(fg, e + 25.0, n) ? 1 : 0;
            SC_EXPECT(FieldParcel(fg, e, n) == FieldParcel(fg, e, n));
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
    std::size_t in_areas = 0, interior = 0;
    std::vector<std::size_t> kinds(static_cast<std::size_t>(TreeKind::Count), 0);
    for (const TreeInstance& t : d.trees) {
        const bool inside = m->Def().InCore(t.east_m, t.north_m) || m->InDetail(t.east_m, t.north_m);
        SC_EXPECT(inside);
        in_areas += m->InDetail(t.east_m, t.north_m) ? 1 : 0;
        // A wood's inside (A.3.19): in the map proper, never in a detail area's wood (planted whole), never a shrub.
        if (t.interior != 0) {
            ++interior;
            SC_EXPECT(!m->InDetail(t.east_m, t.north_m));
            SC_EXPECT(!IsShrub(static_cast<TreeKind>(t.kind)));
        }
        SC_EXPECT(t.kind < static_cast<std::uint8_t>(TreeKind::Count));
        if (t.kind < kinds.size()) ++kinds[t.kind];
    }
    std::printf("    %zu trees and shrubs (%zu in the detail areas, %zu inside the woods), %zu masses; by kind:", d.trees.size(), in_areas, interior,
                d.masses.size());
    for (std::size_t k = 0; k < kinds.size(); ++k) std::printf(" %s %zu", TreeKindName(static_cast<TreeKind>(k)), kinds[k]);
    std::printf("\n");
    // The budget grew with the hill country's woods (terrain roadmap A.3.17: about 500 000; the views' GPU time measured
    // unchanged at about 10 ms, l11b against lu2).
    // And again with the woods' insides (A.3.19: about 700 000 of them, drawn only within 2.5 km of the camera).
    SC_EXPECT(d.trees.size() < 1400000);
    SC_EXPECT(interior > 100000);
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
    std::size_t tree_count = 0;
    for (std::size_t i = 0; i < d.trees.size(); ++i) {
        const TreeInstance& t = d.trees[i];
        const bool shrub = IsShrub(static_cast<TreeKind>(t.kind));
        const double top = t.base_m + t.height_m;
        const double limit = m->GroundHeight(t.east_m, t.north_m) + m->ObstacleHeight(t.east_m, t.north_m) + 0.3;
        over += top > limit + 1e-3 ? 1 : 0;
        short_trees += t.height_m < (shrub ? 1.0f : 3.0f) ? 1 : 0;
        SC_EXPECT(!shrub || t.height_m <= static_cast<float>(r.shrub_height_max_m + 0.3 + 1e-3));
        SC_EXPECT_NEAR(t.base_m, m->GroundHeight(t.east_m, t.north_m) - 0.3, 1e-6);
        SC_EXPECT(t.crown_m > 0.0f && t.crown_m < t.height_m);
        if (shrub) continue;
        ++tree_count;
        mean_height += t.height_m;
        if (i % 97 == 0) {  // a sample, against the lines themselves
            ++checked;
            near_line += FromLines(lines, t.east_m, t.north_m) < r.line_clear_m - 1e-6 ? 1 : 0;
            wet += FromLines(rivers, t.east_m, t.north_m) < 0.0 ? 1 : 0;
        }
    }
    if (tree_count > 0) mean_height /= static_cast<double>(tree_count);
    std::printf("    mean height %.1f m; %d trees checked against the lines\n", mean_height, checked);
    SC_EXPECT(over == 0);
    SC_EXPECT(short_trees == 0);
    SC_EXPECT(near_line == 0);
    SC_EXPECT(wet == 0);
    SC_EXPECT(checked > 500);
    SC_EXPECT(mean_height > 0.7 * m->Def().canopy_min_m && mean_height < m->Def().canopy_max_m + 1.0);
}

SC_TEST("map dressing: woods by stands, a mantle and a fringe of shrubs on the open edges, plantations in rows") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const MapDressing& d = Dressing();
    const DressingRules& r = Rules();
    SC_EXPECT(r.stands.size() >= 4);
    // Wet ground has its own stands: alder only near the water (a river's, or a brook's row of them, A.3.20), beech and
    // pine only away from it.
    std::vector<const MapBox*> brook_rows;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part == MapPart::TreeRow && b.by_water) brook_rows.push_back(&b);
    }
    const auto in_brook_row = [&](const TreeInstance& t) {
        for (const MapBox* b : brook_rows) {
            const double de = t.east_m - b->east_m, dn = t.north_m - b->north_m;
            const double sh = std::sin(b->heading_rad), ch = std::cos(b->heading_rad);
            if (std::fabs(de * sh + dn * ch) <= 0.5 * b->length_m + 3.0 && std::fabs(de * ch - dn * sh) <= 0.5 * b->width_m + 3.0) return true;
        }
        return false;
    };
    int alder_dry = 0, alder = 0, alder_brook = 0, pine_wet = 0, shrubs = 0, shrubs_deep = 0;
    for (const TreeInstance& t : d.trees) {
        const TreeKind k = static_cast<TreeKind>(t.kind);
        const bool wet = m->NearWater(t.east_m, t.north_m, r.wet_m);
        if (k == TreeKind::Alder) {
            ++alder;
            // An alder off the wet ground only as a stand's admixture... none is: alder is wet stands' alone.
            const bool brook = !wet && in_brook_row(t);
            alder_brook += brook ? 1 : 0;
            alder_dry += wet || brook ? 0 : 1;
        }
        if (k == TreeKind::Pine || k == TreeKind::Beech) pine_wet += wet ? 1 : 0;
        if (IsShrub(k)) ++shrubs;
    }
    // Shrubs stand only in the belt: from the inset to the belt's deepest and a jitter in from a block's side, never on
    // the open ground outside it.
    int shrubs_out = 0;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::Canopy || !m->InDetail(b.east_m, b.north_m)) continue;
        for (const TreeInstance& t : d.trees) {
            if (!IsShrub(static_cast<TreeKind>(t.kind))) continue;
            const double de = std::fabs(t.east_m - b.east_m), dn = std::fabs(t.north_m - b.north_m);
            if (de > 0.5 * b.length_m + 2.0 || dn > 0.5 * b.width_m + 2.0) continue;
            const double in = std::min(0.5 * b.length_m - de, 0.5 * b.width_m - dn);
            shrubs_deep += in > r.shrub_inset_m + r.shrub_belt_max_m + r.jitter * r.shrub_spacing_m + 0.5 ? 1 : 0;
            shrubs_out += in < 0.0 ? 1 : 0;
        }
        break;  // one block is enough to look inside
    }
    SC_EXPECT(shrubs_out == 0);
    std::printf("    %d alders (%d along the brooks in %zu rows, %d off the wet ground), %d pine or beech on the wet ground, %d shrubs\n", alder,
                alder_brook, brook_rows.size(), alder_dry, pine_wet, shrubs);
    SC_EXPECT(alder > 100);
    SC_EXPECT(brook_rows.size() > 100 && alder_brook > 1000);
    SC_EXPECT(alder_dry == 0);
    SC_EXPECT(pine_wet == 0);
    SC_EXPECT(shrubs > 1000);
    SC_EXPECT(shrubs_deep == 0);
    // Shrubs stay the lesser part; the hills' woods (A.3.17) have long winding edges, so their share rose to about 40%.
    SC_EXPECT(shrubs < static_cast<int>(d.trees.size()) / 2);
}

SC_TEST("map dressing: a wood's edge stands back in bays and steps down to the field, never a wall (plot 11)") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const MapDressing& d = Dressing();
    const DressingRules& r = Rules();
    SC_EXPECT(r.edge_setback_max_m > r.edge_setback_min_m && r.edge_height < 1.0 && r.edge_crown > 1.0);
    // The detail areas' blocks open on the north (no block beyond): the trees (not shrubs) within the edge band by how
    // far in they stand, and the first tree's depth along the side in 7 m slices.
    double h_near = 0.0, h_far = 0.0, ratio_near = 0.0, ratio_far = 0.0;
    int n_near = 0, n_far = 0, sides = 0;
    double first_lo = 1e9, first_hi = -1e9;
    for (const MapBox& b : m->Layout().boxes) {
        if (sides >= 20) break;
        if (b.part != MapPart::Canopy || !m->InDetail(b.east_m, b.north_m) || m->NearLine(b.east_m, b.north_m, 0.5 * b.length_m + 10.0)) continue;
        bool open = true;
        for (const MapBox& o : m->Layout().boxes) {
            open = open && !(o.part == MapPart::Canopy && std::fabs(o.east_m - b.east_m) < 1.0 && std::fabs(o.north_m - b.north_m - b.width_m) < 1.0);
        }
        if (!open) continue;
        ++sides;
        const double top = b.north_m + 0.5 * b.width_m;
        std::vector<double> first(static_cast<std::size_t>(b.length_m / 7.0), 1e9);
        for (const TreeInstance& t : d.trees) {
            if (IsShrub(static_cast<TreeKind>(t.kind))) continue;
            const double de = t.east_m - b.east_m, in = top - t.north_m;
            if (std::fabs(de) > 0.5 * b.length_m - r.edge_band_m || in < -2.0 || in > r.edge_band_m + 4.0) continue;
            const std::size_t slice = static_cast<std::size_t>((de + 0.5 * b.length_m) / 7.0);
            if (slice < first.size()) first[slice] = std::min(first[slice], in);
            if (in < 0.4 * r.edge_band_m) {
                h_near += t.height_m;
                ratio_near += t.crown_m / t.height_m;
                ++n_near;
            } else if (in > r.edge_band_m) {
                h_far += t.height_m;
                ratio_far += t.crown_m / t.height_m;
                ++n_far;
            }
        }
        for (double f : first) {
            if (f > 1e8) continue;
            first_lo = std::min(first_lo, f);
            first_hi = std::max(first_hi, f);
        }
    }
    SC_EXPECT(sides >= 5 && n_near > 20 && n_far > 20);
    if (n_near == 0 || n_far == 0) return;
    std::printf("    %d open sides: the trees within %.1f m %.1f m high (crown %.2f of height), past %.0f m %.1f m (%.2f); the first tree "
                "%.1f-%.1f m in\n",
                sides, 0.4 * r.edge_band_m, h_near / n_near, ratio_near / n_near, r.edge_band_m, h_far / n_far, ratio_far / n_far, first_lo, first_hi);
    SC_EXPECT(h_near / n_near < 0.85 * h_far / n_far);              // lower at the side
    SC_EXPECT(ratio_near / n_near > 1.1 * ratio_far / n_far);       // and wider for their height
    SC_EXPECT(first_hi - first_lo > 0.5 * (r.edge_setback_max_m - r.edge_setback_min_m));  // bays and tongues: no straight wall
}

SC_TEST("recipes: a wood's floor is measured in from its open side") {
    CoverRaster cover;
    CoverGrid g;
    g.west_m = 0.0;
    g.south_m = 0.0;
    g.cell_m = 25.0;
    g.cols = 4;
    g.rows = 4;
    g.cover.assign(16, static_cast<std::uint8_t>(Cover::Forest));
    g.crop.assign(16, 0);
    g.strips.assign(16, 0);
    for (int c = 0; c < 4; ++c) g.cover[g.Index(c, 3)] = static_cast<std::uint8_t>(Cover::Field);  // the top row open: the edge at 75 m
    cover.grids.push_back(g);
    SC_EXPECT_NEAR(WoodEdgeDistance(cover, 50.0, 73.0, 12.0), 2.0, 1e-9);
    SC_EXPECT_NEAR(WoodEdgeDistance(cover, 50.0, 66.0, 12.0), 9.0, 1e-9);
    SC_EXPECT(std::isinf(WoodEdgeDistance(cover, 50.0, 40.0, 12.0)));  // deeper than the reach
    SC_EXPECT(std::isinf(WoodEdgeDistance(cover, 50.0, 80.0, 12.0)));  // on the field: not a wood
}

SC_TEST("map dressing: the stand rules refuse what they cannot use") {
    std::string e;
    const auto load = [&](const std::string& values) {
        DressingRules x;
        return LoadDressingRules("{\"values\": {" + values + "}}", x, &e);
    };
    SC_EXPECT(load(R"("stands": {"value": [{"id": "a", "site": "dry", "weight": 1, "mix": {"oak": 1}}, {"id": "b", "site": "wet", "weight": 1, "mix": {"alder": 1}}]})"));
    SC_EXPECT(!load(R"("stands": {"value": [{"id": "a", "site": "dry", "weight": 1, "mix": {"oak": 1}}]})"));  // no wet stand
    SC_EXPECT(!load(R"("stands": {"value": [{"id": "a", "site": "damp", "weight": 1, "mix": {"oak": 1}}, {"id": "b", "site": "wet", "weight": 1, "mix": {"alder": 1}}]})"));
    SC_EXPECT(!load(R"("stands": {"value": [{"id": "a", "site": "dry", "weight": 1, "mix": {"hazel": 1}}, {"id": "b", "site": "wet", "weight": 1, "mix": {"alder": 1}}]})"));
    SC_EXPECT(!load(R"("stands": {"value": [{"id": "a", "site": "dry", "weight": 1, "mix": {"baobab": 1}}, {"id": "b", "site": "wet", "weight": 1, "mix": {"alder": 1}}]})"));
    SC_EXPECT(!load(R"("shrub_mix": {"value": {"oak": 1}})"));
    SC_EXPECT(!load(R"("edge_mix": {"value": {"oak": 0}})"));
    SC_EXPECT(!load(R"("shrub_height_m": {"value": [4, 1]})"));
    std::printf("    the last refusal: %s\n", e.c_str());
}

SC_TEST("map dressing: no wood's mass tops what blocks a sight line, in the map proper or beyond it") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const MapDressing& d = Dressing();
    int over = 0, beyond = 0;
    double worst = -1e9;
    for (const CanopyMass& c : d.masses) {
        const bool far_out = !m->Def().InCore(c.east_m, c.north_m);
        beyond += far_out ? 1 : 0;
        const double top = c.base_m + c.height_m;
        const double sh = std::sin(c.heading_rad), ch = std::cos(c.heading_rad);
        const double hl = 0.5 * c.length_m - 0.5, hw = 0.5 * c.width_m - 0.5;
        const double at[5][2] = {{0.0, 0.0}, {hl, hw}, {hl, -hw}, {-hl, hw}, {-hl, -hw}};
        bool short_of = false;
        for (const auto& p : at) {
            const double e = c.east_m + p[0] * sh + p[1] * ch, n = c.north_m + p[0] * ch - p[1] * sh;
            const double gap = top + c.rise_e * (e - c.east_m) + c.rise_n * (n - c.north_m) - (m->GroundHeight(e, n) + m->ObstacleHeight(e, n));
            worst = std::max(worst, gap);
            short_of = short_of || gap > 0.3;  // the trees' allowance
        }
        over += short_of ? 1 : 0;
    }
    std::printf("    %zu masses (%d beyond the map proper), %d above their obstacle (worst %.2f m)\n", d.masses.size(), beyond, over, worst);
    SC_EXPECT(beyond > 1000);
    SC_EXPECT(over == 0);
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
        int row0 = 0, row1 = 0, inside = 0, inside_flagged = 0;
        const double top = b.north_m + 0.5 * b.width_m;
        for (const TreeInstance& t : d.trees) {
            if (!in_block(t, b)) continue;
            const double in = top - t.north_m;
            if (in > 0.0 && in < r.edge_spacing_m) ++row0;
            else if (in >= r.edge_spacing_m && in < 2.0 * r.edge_spacing_m) ++row1;
            else if (in > 2.0 * r.edge_spacing_m + 4.0 && std::fabs(t.east_m - b.east_m) < 0.5 * b.length_m - 2.0 * r.edge_spacing_m - 4.0) {
                ++inside;
                inside_flagged += t.interior != 0 ? 1 : 0;
            }
        }
        const int per_row = static_cast<int>(std::floor(b.length_m / r.edge_spacing_m));
        SC_EXPECT(row0 >= per_row * 7 / 10);
        SC_EXPECT(row1 >= per_row * 7 / 10);
        // Its inside (A.3.19): canopy trees on the interior lattice, every one flagged (drawn near; the mass far).
        const double inner = b.length_m - 2.0 * (2.0 * r.edge_spacing_m + 4.0);
        const int lattice = static_cast<int>(std::floor(inner / r.interior_spacing_m));
        SC_EXPECT(inside >= lattice * lattice / 2);
        SC_EXPECT(inside_flagged == inside);
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

SC_TEST("map dressing: the field function is the ground material's, pinned by its hash's values") {
    // The material's HLSL (tools/ue_build_ground_material.py) repeats these, so a change here must change it too.
    SC_EXPECT(Hash32(0u) == 0u);
    SC_EXPECT(Hash32(1u) == 1753845952u);
    SC_EXPECT(Hash32(0xDEADBEEFu) == 3861431939u);
    SC_EXPECT(FieldHash(0x5EEDu, 3, -2) == 1850613118u);
    SC_EXPECT(FieldHash(0u, 0, 0) == 2488251732u);
    SC_EXPECT(FieldHash(123456u, -7, 1000) == 3906896019u);
    SC_EXPECT(Unit24(0xFFFFFFFFu) < 1.0 && Unit24(0u) == 0.0);
}

SC_TEST("map dressing: a field is a collective farm's, its numbers within the rules") {
    FieldGrid grid;
    grid.seed = 0xABCDEFu;
    int checked = 0, centre_ok = 0;
    double min_ha = 1e9, max_ha = 0.0;
    for (double n = -23000.0; n < 23000.0; n += 1301.0) {
        for (double e = -23000.0; e < 23000.0; e += 1117.0) {
            const FieldCell f = FieldAt(grid, e, n);
            // The nominal centre is the field's own unless an estate's border cuts it off from the field (the ground
            // material reads the mask there only when it is).
            if (FieldAt(grid, f.centre_e, f.centre_n).id == f.id) ++centre_ok;
            SC_EXPECT(f.heading_rad >= 0.0 && f.heading_rad < kPi);
            SC_EXPECT(f.length_m >= grid.length_min_m && f.length_m <= grid.length_max_m);
            SC_EXPECT(f.width_m >= grid.width_min_m && f.width_m <= grid.width_max_m);
            const double ha = f.length_m * f.width_m / 10000.0;
            min_ha = std::min(min_ha, ha);
            max_ha = std::max(max_ha, ha);
            ++checked;
        }
    }
    std::printf("    %d fields sampled (%d with their centre in them), %.0f-%.0f ha\n", checked, centre_ok, min_ha, max_ha);
    SC_EXPECT(min_ha >= 80.0 && max_ha <= 210.0);
    SC_EXPECT(centre_ok > checked * 7 / 10);
    // The old names are the same function.
    SC_EXPECT(FieldHeading(grid, 100.0, 200.0) == FieldAt(grid, 100.0, 200.0).heading_rad);
    SC_EXPECT(FieldParcel(grid, 100.0, 200.0) == FieldAt(grid, 100.0, 200.0).id);
}

SC_TEST("map dressing: the fields near a settlement are household strips, and only those") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const DressingRules& r = Rules();
    const CoverRaster a = BakeCover(*m, r);
    const CoverGrid& g = a.grids.back();  // the map proper
    std::size_t with = 0, fields = 0, far_with = 0;
    for (int row = 0; row < g.rows; row += 2) {
        for (int c = 0; c < g.cols; c += 2) {
            const std::size_t i = g.Index(c, row);
            SC_EXPECT(g.strips[i] <= 1);
            if (g.cover[i] != static_cast<std::uint8_t>(Cover::Field)) continue;
            ++fields;
            const FieldCell f = FieldAt(g.fields, g.CentreEast(c), g.CentreNorth(row));
            double nearest = 1e18;
            for (const MapSettlementDef& s : m->Def().settlements) {
                nearest = std::min(nearest, std::hypot(f.centre_e - s.centre.east_m, f.centre_n - s.centre.north_m) - s.radius_m);
            }
            const bool near_village = nearest < r.strip_reach_m;
            SC_EXPECT((g.strips[i] != 0) == near_village);
            with += g.strips[i] != 0;
            far_with += g.strips[i] != 0 && nearest > r.strip_reach_m + 1.0;
        }
    }
    std::printf("    %zu of %zu field cells are household strips\n", with, fields);
    SC_EXPECT(with > 0 && with < fields / 3);
    SC_EXPECT(far_with == 0);
}

SC_TEST("map dressing: farm tracks, tree rows and groves lie along the fields, clear of what stands and of the sites") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const MapDef& d = m->Def();
    SC_EXPECT(d.farmland.laid);
    std::size_t tracks = 0, farm_points = 0;
    for (const MapStrip& s : m->Layout().strips) {
        if (s.id.compare(0, 11, "farm_track_") != 0) continue;
        ++tracks;
        farm_points += s.points.size();
        SC_EXPECT(s.surface == MapSurface::Track && s.points.size() >= 3 && s.width_m == d.farmland.track_width_m);
        for (const Vec3& p : s.points) SC_EXPECT(d.InCore(p.x, p.y));
    }
    std::size_t rows = 0, bad_site = 0, bad_water = 0, bad_town = 0, bad_height = 0, outside = 0;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::TreeRow) continue;
        ++rows;
        outside += d.InCore(b.east_m, b.north_m) ? 0 : 1;
        for (const MapSiteDef& s : d.sites) bad_site += std::hypot(b.east_m - s.centre.east_m, b.north_m - s.centre.north_m) < d.farmland.site_clear_m - 1.0 ? 1 : 0;
        bad_water += m->NearWater(b.east_m, b.north_m, 0.0) ? 1 : 0;
        for (const MapSettlementDef& s : d.settlements) bad_town += std::hypot(b.east_m - s.centre.east_m, b.north_m - s.centre.north_m) < s.radius_m ? 1 : 0;
        // The height above the ground: a row's or a grove's (the sink is under it).
        const double up = b.height_m - 0.3;
        bad_height += up < d.farmland.row_height_min_m - 1e-6 && up < d.farmland.grove_height_min_m - 1e-6 ? 1 : 0;
        bad_height += up > std::max(d.farmland.row_height_max_m, d.farmland.grove_height_max_m) + 1e-6 ? 1 : 0;
    }
    std::printf("    %zu farm tracks (%zu points), %zu tree rows and grove belts\n", tracks, farm_points, rows);
    SC_EXPECT(tracks > 20 && rows > 100);
    SC_EXPECT(outside == 0 && bad_site == 0 && bad_water == 0 && bad_town == 0 && bad_height == 0);
    // Each is a box that blocks sight (the obstacle top at its centre is its top), and the same each time.
    int checked = 0;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::TreeRow || checked >= 200) continue;
        SC_EXPECT(m->ObstacleTop(b.east_m, b.north_m) >= b.base_m + b.height_m - 1e-6);
        ++checked;
    }
    SC_EXPECT(checked > 50);
}

SC_TEST("map dressing: the tree rows are planted with trees that keep to their boxes' height") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const MapDressing& dress = Dressing();
    std::size_t near_rows = 0, looked = 0;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::TreeRow || looked >= 300) continue;
        ++looked;
        for (const TreeInstance& t : dress.trees) {
            if (std::fabs(t.east_m - b.east_m) > 5.0 || std::fabs(t.north_m - b.north_m) > 5.0) continue;
            ++near_rows;
            break;
        }
    }
    SC_EXPECT(near_rows > looked * 8 / 10);
}

SC_TEST("map dressing: the Osa and dummy positions keep their rows off, and battery 21 on its rise sees a low flier all round") {
    std::string e;
    const std::shared_ptr<const MapData> m = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(m != nullptr);
    if (!m) return;
    const MapDef& d = m->Def();
    SC_EXPECT(d.farmland.position_clear_m > d.farmland.site_clear_m);
    std::size_t near_position = 0;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::TreeRow) continue;
        for (const MapSiteDef& s : d.sites) {
            if (s.kind != "osa_position" && s.kind != "dummy_position") continue;
            near_position += std::hypot(b.east_m - s.centre.east_m, b.north_m - s.centre.north_m) < d.farmland.position_clear_m - 1.0 ? 1 : 0;
        }
    }
    SC_EXPECT(near_position == 0);
    // Battery 21's launchers (places_pannonia bty21_v1..v4) on their rise: in every direction one of them sees a target
    // 50 m over the ground 15 km out, by the sight rule of the battery fire model (engagement.hpp SearchAntennaSees).
    const double launchers[4][2] = {{6000.0, -9200.0}, {6900.0, -9900.0}, {5400.0, -10400.0}, {6300.0, -10900.0}};
    const MapTerrain terrain(m, d.origin);
    int hidden = 0;
    for (int az = 0; az < 360; az += 5) {
        bool seen = false;
        for (const auto& l : launchers) {
            const double eye = m->GroundHeight(l[0], l[1]) + 5.0;
            const double a = az * kPi / 180.0, te = l[0] + 15000.0 * std::sin(a), tn = l[1] + 15000.0 * std::cos(a);
            const double top = m->GroundHeight(te, tn) + 50.0;
            if (HasLineOfSight(terrain, Vec3{l[0], l[1], eye}, eye, Vec3{te, tn, top}, top, 100.0)) {
                seen = true;
                break;
            }
        }
        hidden += seen ? 0 : 1;
    }
    std::printf("    battery 21: %d of 72 directions hide a target 50 m up at 15 km\n", hidden);
    SC_EXPECT(hidden == 0);
}
