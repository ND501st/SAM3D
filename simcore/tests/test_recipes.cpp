// The land's recipes (simcore/scenery/recipes.hpp, roadmap L2): every recipe loads and covers the layers the clutter had;
// a cell's points are the same however they are asked for, cells meet without a seam, thinning never moves a point; the
// processes' patterns (blue noise even, clusters clumped); the parcel's bands; every target met on the plot's ground;
// the loader's refusals.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/scenery/clutter.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::scenery;

namespace {

const std::string kData = SIMCORE_DATA_DIR;
constexpr int kDay = 288;  // 15 October

const DressingRules& Dressing() {
    static DressingRules rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadDressingRules(kData + "/scenery/map_dressing.json", rules, &e)) std::printf("    %s\n", e.c_str());
    }
    return rules;
}

const GroundLayerRules& Layers() {
    static GroundLayerRules rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadGroundLayers(kData + "/scenery/ground_layers.json", Dressing(), rules, &e)) std::printf("    %s\n", e.c_str());
    }
    return rules;
}

const ClutterRules& Rules() {
    static ClutterRules rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadClutterRules(kData + "/scenery/clutter.json", rules, &e) || !LoadClutterRecipes(kData + "/scenery/recipes", Layers(), rules, &e)) {
            std::printf("    %s\n", e.c_str());
        }
    }
    return rules;
}

const CoverRaster& Raster() {
    static CoverRaster raster;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        const std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
        if (map) raster = BakeCover(*map, Dressing());
        else std::printf("    %s\n", e.c_str());
    }
    return raster;
}

int RecipeIndex(const std::string& id) {
    const std::vector<Recipe>& rs = Rules().recipes.recipes;
    for (std::size_t i = 0; i < rs.size(); ++i) {
        if (rs[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

// Every candidate of the cells over a rectangle at full density.
std::vector<RecipeCandidate> All(double e0, double n0, double e1, double n1, double cell = 4.0) {
    std::vector<RecipeCandidate> out;
    for (auto cy = static_cast<std::int64_t>(std::floor(n0 / cell)); cy <= static_cast<std::int64_t>(std::floor(n1 / cell)); ++cy) {
        for (auto cx = static_cast<std::int64_t>(std::floor(e0 / cell)); cx <= static_cast<std::int64_t>(std::floor(e1 / cell)); ++cx) {
            RecipeCandidates(Rules().recipes, Raster(), Dressing(), Layers(), kDay, cx, cy, cell,
                             [&](double e, double n, double) { return e >= e0 && e < e1 && n >= n0 && n < n1; }, out);
        }
    }
    return out;
}

// A spot whose 120 m square is all one recipe's, near (east, north): the plot lab's way of finding ground.
bool FindGround(const std::string& recipe, double east, double north, double& e_out, double& n_out) {
    const int want = RecipeIndex(recipe);
    for (double r = 0.0; r < 4000.0; r += 150.0) {
        for (double a = 0.0; a < 6.283; a += (r > 0.0 ? 150.0 / r : 7.0)) {
            const double e = east + r * std::cos(a), n = north + r * std::sin(a);
            bool all = true;
            for (double dn = -60.0; dn <= 60.0 && all; dn += 20.0) {
                for (double de = -60.0; de <= 60.0 && all; de += 20.0) {
                    const int layer = PointLayer(Raster(), Dressing(), Layers(), e + de, n + dn, kDay);
                    all = layer >= 0 && Rules().recipes.by_layer[static_cast<std::size_t>(layer)] == want;
                }
            }
            if (all) {
                e_out = e;
                n_out = n;
                return true;
            }
        }
    }
    return false;
}

}  // namespace

SC_TEST("recipes: every recipe loads, and every layer the clutter covered has one") {
    const ClutterRules& rules = Rules();
    SC_EXPECT(rules.recipes.recipes.size() == 12);
    for (const char* id : {"ploughed", "sown", "stubble", "maize_stubble", "dry_stalks", "meadow", "verge", "trampled", "bank", "forest_floor", "marsh",
                           "tall_grass"}) {
        const int layer = Layers().Index(id);
        SC_EXPECT(layer >= 0 && rules.recipes.ForLayer(layer) != nullptr);
    }
    for (const Recipe& r : rules.recipes.recipes) {
        SC_EXPECT(!r.source.empty() && !r.confidence.empty());
        SC_EXPECT(r.bands.back().whole);  // the rest of the land type always has a band
    }
}

SC_TEST("recipes: a cell's points are its own: asked twice, from either side of a seam, or thinned, they do not move") {
    const double e0 = 5700.0, n0 = -9100.0;
    const std::vector<RecipeCandidate> a = All(e0, n0, e0 + 60.0, n0 + 60.0);
    const std::vector<RecipeCandidate> b = All(e0, n0, e0 + 60.0, n0 + 60.0);
    SC_EXPECT(!a.empty() && a.size() == b.size());
    bool same = a.size() == b.size();
    for (std::size_t i = 0; same && i < a.size(); ++i) same = a[i].east_m == b[i].east_m && a[i].north_m == b[i].north_m && a[i].hash == b[i].hash;
    SC_EXPECT(same);
    // The same ground cut into 8 m cells: the same points (the processes run over the plane, not the cell).
    const std::vector<RecipeCandidate> c = All(e0, n0, e0 + 60.0, n0 + 60.0, 8.0);
    std::size_t found = 0;
    for (const RecipeCandidate& p : a) {
        for (const RecipeCandidate& q : c) {
            if (q.east_m == p.east_m && q.north_m == p.north_m && q.hash == p.hash) {
                ++found;
                break;
            }
        }
    }
    std::printf("    %zu points in 4 m cells, %zu in 8 m cells, %zu the same\n", a.size(), c.size(), found);
    // Every point of the small cells is one of the large cells' where it was; the large ones may add a point in a sliver of
    // another layer that no small cell's samples fell on (RecipeCandidates), a few in a thousand at most.
    SC_EXPECT(found == a.size());
    SC_EXPECT(c.size() >= a.size() && (c.size() - a.size()) * 200 <= a.size());
    // Thinned to half: every point kept is one of the full set's, where it was.
    std::vector<RecipeCandidate> half;
    for (auto cy = static_cast<std::int64_t>(std::floor(n0 / 4.0)); cy < static_cast<std::int64_t>(std::floor((n0 + 60.0) / 4.0)); ++cy) {
        for (auto cx = static_cast<std::int64_t>(std::floor(e0 / 4.0)); cx < static_cast<std::int64_t>(std::floor((e0 + 60.0) / 4.0)); ++cx) {
            RecipeCandidates(Rules().recipes, Raster(), Dressing(), Layers(), kDay, cx, cy, 4.0, [](double, double, double t) { return t < 0.5; }, half);
        }
    }
    std::size_t moved = 0;
    for (const RecipeCandidate& p : half) {
        bool in = false;
        for (const RecipeCandidate& q : a) in = in || (q.east_m == p.east_m && q.north_m == p.north_m && q.hash == p.hash);
        moved += in ? 0 : 1;
    }
    SC_EXPECT(moved == 0);
    SC_EXPECT(half.size() > a.size() / 3 && half.size() < a.size() * 2 / 3);
}

SC_TEST("recipes: blue noise is even and keeps its distance, clusters clump (variance against mean in 2 m squares)") {
    RecipeScatter blue;
    blue.process = ScatterProcess::BlueNoise;
    blue.per_ha = 18000.0;
    RecipeScatter thomas;
    thomas.process = ScatterProcess::Thomas;
    thomas.per_ha = 9000.0;
    thomas.kappa_per_ha = 600.0;
    thomas.spread_m = 1.2;
    RecipeScatter uniform;
    uniform.process = ScatterProcess::Uniform;
    uniform.per_ha = 9000.0;
    const auto dispersion = [](const RecipeScatter& s, double& per_ha, double& nearest_m) {
        const int side = 25;  // 25 x 25 cells of 4 m: 1 ha
        std::vector<double> pe, pn;
        for (int cy = 0; cy < side; ++cy) {
            for (int cx = 0; cx < side; ++cx) {
                ScatterInCell(s, 12345u, cx, cy, 4.0, [&](double e, double n, double, std::uint64_t) {
                    pe.push_back(e);
                    pn.push_back(n);
                });
            }
        }
        per_ha = static_cast<double>(pe.size());
        std::vector<int> q(50 * 50, 0);
        for (std::size_t i = 0; i < pe.size(); ++i) ++q[static_cast<std::size_t>(std::floor(pn[i] / 2.0)) * 50 + static_cast<std::size_t>(std::floor(pe[i] / 2.0))];
        double mean = 0.0, var = 0.0;
        for (const int v : q) mean += v;
        mean /= static_cast<double>(q.size());
        for (const int v : q) var += (v - mean) * (v - mean);
        var /= static_cast<double>(q.size() - 1);
        nearest_m = 1e9;
        for (std::size_t i = 0; i < pe.size() && i < 4000; ++i) {
            for (std::size_t j = i + 1; j < pe.size(); ++j) {
                const double d = std::hypot(pe[i] - pe[j], pn[i] - pn[j]);
                if (d < nearest_m) nearest_m = d;
            }
        }
        return var / mean;
    };
    double nb = 0.0, nt = 0.0, nu = 0.0, db = 0.0, dt = 0.0, du = 0.0;
    const double vb = dispersion(blue, nb, db), vt = dispersion(thomas, nt, dt), vu = dispersion(uniform, nu, du);
    std::printf("    a hectare: blue noise %.0f (var/mean %.2f, nearest %.2f m), uniform %.0f (%.2f), Thomas %.0f (%.2f)\n", nb, vb, db, nu, vu, nt, vt);
    SC_EXPECT(std::fabs(nb - 18000.0) < 600.0);
    SC_EXPECT(std::fabs(nu - 9000.0) < 900.0);
    SC_EXPECT(std::fabs(nt - 9000.0) < 2500.0);  // a cluster process varies more
    SC_EXPECT(vb < 0.6);                         // even
    SC_EXPECT(vu > 0.6 && vu < 1.6);             // about Poisson
    SC_EXPECT(vt > 2.0);                         // clumped
    SC_EXPECT(db >= 0.2 * std::sqrt(10000.0 / 18000.0) - 1e-9);
}

SC_TEST("recipes: a crop's rows lie along its parcel's furrows, row_m apart, on fields only; the stover lies along them") {
    // The process alone: a hectare at a heading of 30 degrees, rows 0.75 m apart.
    RecipeScatter rows;
    rows.process = ScatterProcess::Rows;
    rows.per_ha = 13333.0;
    rows.row_m = 0.75;
    const double heading = 30.0 * kPi / 180.0, sh = std::sin(heading), ch = std::cos(heading);
    std::size_t count = 0, off_row = 0;
    for (int cy = 0; cy < 25; ++cy) {
        for (int cx = 0; cx < 25; ++cx) {
            ScatterInCell(rows, 777u, cx, cy, 4.0, [&](double e, double n, double, std::uint64_t) {
                ++count;
                const double v = (e * ch - n * sh) / rows.row_m;
                off_row += std::fabs(v - std::floor(v) - 0.5) <= 0.04 / rows.row_m + 1e-9 ? 0 : 1;
            }, heading);
        }
    }
    // With no heading (off a field) there are no rows.
    std::size_t none = 0;
    ScatterInCell(rows, 777u, 0, 0, 4.0, [&](double, double, double, std::uint64_t) { ++none; });
    std::printf("    rows at 30 degrees: %zu a hectare, %zu off their row, %zu with no heading\n", count, off_row, none);
    SC_EXPECT(std::fabs(static_cast<double>(count) - 13333.0) < 400.0);
    SC_EXPECT(off_row == 0 && none == 0);

    // On the map: a picked maize field's stumps stand on fields, each in a row of its own parcel's heading.
    double e = 0.0, n = 0.0;
    SC_EXPECT(FindGround("maize_stubble", 8000.0, -8000.0, e, n));
    const int ri = RecipeIndex("maize_stubble");
    const int stump = [&] {
        const std::vector<std::string> ids = Rules().KindIds();
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] == "stump") return static_cast<int>(i);
        }
        return -1;
    }();
    std::size_t stumps = 0, astray = 0;
    for (const RecipeCandidate& c : All(e - 60.0, n - 60.0, e + 60.0, n + 60.0)) {
        if (c.recipe != ri || c.kind != stump) continue;
        ++stumps;
        FieldCell f;
        const bool field = ParcelAt(Raster(), c.east_m, c.north_m, f);
        const double v = (c.east_m * std::cos(f.heading_rad) - c.north_m * std::sin(f.heading_rad)) / 0.75;
        astray += c.on_field && field && f.heading_rad == c.heading_rad && std::fabs(v - std::floor(v) - 0.5) <= 0.054 ? 0 : 1;
    }
    std::printf("    maize stubble round (%.0f, %.0f): %zu stumps, %zu astray\n", e, n, stumps, astray);
    SC_EXPECT(stumps > 5000 && astray == 0);

    // The ring: the stover lies within 35 degrees of its parcel's rows, either end first.
    std::vector<ClutterPlant> ring;
    ClutterAround(Rules(), Raster(), Dressing(), Layers(), kDay, e, n, ring);
    std::size_t stover = 0, across = 0;
    for (const ClutterPlant& p : ring) {
        if (Rules().kinds[p.kind].id != "stover") continue;
        FieldCell f;
        if (!ParcelAt(Raster(), p.east_m, p.north_m, f)) continue;
        ++stover;
        double d = std::fmod(static_cast<double>(p.yaw_rad) - f.heading_rad + 8.0 * kPi, kPi);  // either end first
        if (d > kPi / 2.0) d = kPi - d;
        across += d <= 35.0 * kPi / 180.0 + 1e-4 ? 0 : 1;
    }
    std::printf("    the ring there: %zu plants, %zu stover on fields, %zu lying across the rows\n", ring.size(), stover, across);
    SC_EXPECT(stover > 500 && across == 0);
    // The rows' ring (21,043 at b3, 29,121 once A.3.40 thickened the sward round them), under the clutter test's 35,000.
    SC_EXPECT(ring.size() < 35000);
}

SC_TEST("recipes: a scatter kept to a crop stands only on its fields; the wheat's swaths lie a header's width apart") {
    const std::vector<std::string> ids = Rules().KindIds();
    const auto kind = [&](const char* id) {
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] == id) return static_cast<int>(i);
        }
        return -1;
    };
    const int dry = Layers().Index("dry_stalks"), maize = Layers().CropIndex("maize"), sunflower = Layers().CropIndex("sunflower");
    SC_EXPECT(dry >= 0 && maize >= 0 && sunflower >= 0 && kind("maize") >= 0 && kind("sunflower") >= 0);
    // A field of each crop standing dry on the day, near the battery.
    const auto find = [&](int crop, double& e_out, double& n_out) {
        for (double r = 0.0; r < 6000.0; r += 50.0) {
            for (double a = 0.0; a < 2.0 * kPi; a += r > 0.0 ? 50.0 / r : 7.0) {
                const double e = 8000.0 + r * std::cos(a), n = -8000.0 + r * std::sin(a);
                FieldCell f;
                if (PointLayer(Raster(), Dressing(), Layers(), e, n, kDay) != dry || !ParcelAt(Raster(), e, n, f) || CropOf(Dressing(), f.id) != crop) continue;
                e_out = e;
                n_out = n;
                return true;
            }
        }
        return false;
    };
    const int ri = RecipeIndex("dry_stalks");
    for (const int crop : {maize, sunflower}) {
        double e = 0.0, n = 0.0;
        SC_EXPECT(find(crop, e, n));
        std::size_t mine = 0, wrong = 0;
        for (const RecipeCandidate& c : All(e - 40.0, n - 40.0, e + 40.0, n + 40.0)) {
            if (c.recipe != ri) continue;
            FieldCell f;
            const int here = ParcelAt(Raster(), c.east_m, c.north_m, f) ? CropOf(Dressing(), f.id) : -1;
            const int want = c.kind == kind("maize") ? maize : sunflower;
            wrong += here == want ? 0 : 1;
            mine += here == crop ? 1 : 0;
        }
        std::printf("    dry %s round (%.0f, %.0f): %zu plants of its own, %zu on another crop's field\n", Layers().crop_ids[static_cast<std::size_t>(crop)].c_str(), e, n,
                    mine, wrong);
        SC_EXPECT(mine > 2000 && wrong == 0);
    }
    // The wheat stubble's swaths: each within a quarter metre of its line, the lines 6 m apart along the furrows.
    double e = 0.0, n = 0.0;
    SC_EXPECT(FindGround("wheat_stubble", 8000.0, -8000.0, e, n));
    const int wi = RecipeIndex("wheat_stubble");
    std::size_t swaths = 0, off = 0, stubble = 0;
    for (const RecipeCandidate& c : All(e - 60.0, n - 60.0, e + 60.0, n + 60.0)) {
        if (c.recipe != wi) continue;
        stubble += c.kind == kind("stubble") ? 1 : 0;
        if (c.kind != kind("swath")) continue;
        ++swaths;
        const double v = (c.east_m * std::cos(c.heading_rad) - c.north_m * std::sin(c.heading_rad)) / 6.0;
        off += std::fabs(v - std::floor(v) - 0.5) * 6.0 <= 0.25 + 1e-6 ? 0 : 1;
    }
    std::printf("    wheat stubble round (%.0f, %.0f): %zu stubble tufts, %zu swath pieces, %zu off their line\n", e, n, stubble, swaths, off);
    SC_EXPECT(swaths > 500 && off == 0 && stubble > swaths * 5);
    // The loader: across_m for rows only and within 0.4 of row_m; crops the dressing's.
    Recipe r;
    std::string err;
    const auto with = [&](const std::string& scatter) {
        return R"({"recipe_id": "x", "applies": {"layers": ["stubble"]}, "priority": 1, "bands": [{"id": "body", "scatter": [)" + scatter +
               R"(]}], "source": "s", "confidence": "c"})";
    };
    SC_EXPECT(LoadRecipe(with(R"({"kind": "swath", "process": "rows", "per_ha": 1400, "row_m": 6, "across_m": 0.25})"), Layers(), ids, r, &err));
    SC_EXPECT(!LoadRecipe(with(R"({"kind": "swath", "process": "rows", "per_ha": 1400, "row_m": 6, "across_m": 3})"), Layers(), ids, r, &err));
    SC_EXPECT(!LoadRecipe(with(R"({"kind": "swath", "process": "uniform", "per_ha": 1400, "across_m": 0.2})"), Layers(), ids, r, &err));
    SC_EXPECT(!LoadRecipe(with(R"({"kind": "swath", "process": "rows", "per_ha": 1400, "row_m": 13})"), Layers(), ids, r, &err));
    SC_EXPECT(LoadRecipe(with(R"({"kind": "maize", "process": "uniform", "per_ha": 100, "crops": ["maize"]})"), Layers(), ids, r, &err));
    SC_EXPECT(!LoadRecipe(with(R"({"kind": "maize", "process": "uniform", "per_ha": 100, "crops": ["rice"]})"), Layers(), ids, r, &err));
    SC_EXPECT(!LoadRecipe(with(R"({"kind": "maize", "process": "uniform", "per_ha": 100, "crops": []})"), Layers(), ids, r, &err));
}

SC_TEST("recipes: a ploughed parcel's edge strip is weeds within 0.6 m of its edge, its headlands at its ends") {
    double e = 0.0, n = 0.0;
    SC_EXPECT(FindGround("ploughland", 8000.0, -8000.0, e, n));
    // Look over a 1 km square round it for the bands' points and check each against its parcel.
    const std::vector<RecipeCandidate> pts = All(e - 500.0, n - 500.0, e + 500.0, n + 500.0);
    const int ri = RecipeIndex("ploughland");
    std::size_t margin = 0, headland = 0, wrong = 0;
    for (const RecipeCandidate& c : pts) {
        if (c.recipe != ri) continue;
        double ends = 0.0, sides = 0.0;
        ParcelEdgeDistance(Raster(), c.east_m, c.north_m, ends, sides);
        const std::string& band = Rules().recipes.recipes[static_cast<std::size_t>(ri)].bands[static_cast<std::size_t>(c.band)].id;
        if (band == "margin") {
            ++margin;
            wrong += std::min(ends, sides) < 0.6 ? 0 : 1;
        } else if (band == "headland") {
            ++headland;
            wrong += ends >= 0.6 && ends < 18.0 ? 0 : 1;
        } else {
            wrong += std::min(ends, sides) >= 0.6 && ends >= 18.0 ? 0 : 1;
        }
    }
    std::printf("    ploughland round (%.0f, %.0f): %zu in the edge strip, %zu on the headlands, %zu in the wrong band\n", e, n, margin, headland, wrong);
    SC_EXPECT(margin > 0 && headland > 0 && wrong == 0);
}

SC_TEST("recipes: every recipe's targets are met on its own ground (the report)") {
    const std::vector<std::string> kinds = Rules().KindIds();
    int checked = 0;
    for (const Recipe& r : Rules().recipes.recipes) {
        if (r.targets.empty()) continue;
        double e = 0.0, n = 0.0;
        if (!FindGround(r.id, 8000.0, -8000.0, e, n)) {
            std::printf("    %s: no 120 m of its ground on the map to measure on\n", r.id.c_str());
            continue;
        }
        // The square round the spot, out to 300 m (bands at parcel edges included where they fall).
        const std::vector<RecipeMeasureRow> rows = RecipeMeasure(Rules().recipes, kinds, Raster(), Dressing(), Layers(), kDay, e - 150.0, n - 150.0, e + 150.0, n + 150.0);
        for (const RecipeMeasureRow& row : rows) {
            if (!row.has_target || &Rules().recipes.recipes[static_cast<std::size_t>(row.recipe)] != &r) continue;
            // A band of under a quarter hectare is too small a sample to hold to a target.
            if (row.area_ha < 0.25) continue;
            ++checked;
            std::printf("    %s %s %s: %.0f a hectare over %.2f ha (target %.0f-%.0f)\n", r.id.c_str(), r.bands[static_cast<std::size_t>(row.band)].id.c_str(),
                        kinds[static_cast<std::size_t>(row.kind)].c_str(), row.per_ha, row.area_ha, row.lo_per_ha, row.hi_per_ha);
            SC_EXPECT(row.on_target);
        }
    }
    SC_EXPECT(checked >= 6);
}

SC_TEST("recipes: a village pond's reeds stand in a margin at its water's edge, its grassed bank beyond (A.3.35-36)") {
    const CoverRaster& raster = Raster();
    const DressingRules& d = Dressing();
    SC_EXPECT(raster.ponds.size() >= 3);
    const int marsh = Layers().LayerOnDay(Cover::Marsh, 0, 0, kDay);
    const std::vector<std::string> ids = Rules().KindIds();
    const int reed = static_cast<int>(std::find(ids.begin(), ids.end(), std::string("reed")) - ids.begin());
    for (const CoverRaster::Pond& p : raster.ponds) {
        // Round its edge: in the margin the marsh's layer, well beyond it the ground's own (unless the raster has a marsh).
        for (int k = 0; k < 16; ++k) {
            const double a = 2.0 * 3.14159265358979 * k / 16.0;
            const double x = std::cos(a), y = std::sin(a);
            const double ue = std::sin(p.heading), un = std::cos(p.heading);
            const auto at = [&](double off) {
                // A point `off` beyond the edge along the ray from the middle (the edge where rho = rx*ry/|(ry x, rx y)|).
                const double edge = p.rx * p.ry / std::hypot(p.ry * x, p.rx * y);
                const double r = edge + off, lx = r * x, ly = r * y;
                return PointLayer(raster, d, Layers(), p.e + lx * ue + ly * un, p.n + lx * un - ly * ue, kDay);
            };
            SC_EXPECT(at(0.5 * d.pond_reeds_m) == marsh);
            SC_EXPECT(at(-0.5 * d.pond_reeds_in_m) == marsh);
            SC_EXPECT(at(0.5 * (d.pond_reeds_m + d.pond_bank_m)) == Layers().Index("meadow"));  // its grassed bank (A.3.36)
            const double fe = p.e + (p.rx + d.pond_reeds_m + 6.0) * ue, fn = p.n + (p.rx + d.pond_reeds_m + 6.0) * un;
            if (k == 0 && raster.At(fe, fn) != Cover::Marsh) SC_EXPECT(PointLayer(raster, d, Layers(), fe, fn, kDay) != marsh);
        }
        // Reeds there, at full density: none farther than the margin from the water's edge.
        int reeds = 0, beyond = 0;
        const double reach = p.rx + d.pond_reeds_m + 8.0;
        for (const RecipeCandidate& c : All(p.e - reach, p.n - reach, p.e + reach, p.n + reach)) {
            if (c.kind != reed) continue;
            const double off = EllipseEdgeDistance(p.e, p.n, p.rx, p.ry, p.heading, c.east_m, c.north_m);
            const Cover under = raster.At(c.east_m, c.north_m);
            if (off <= d.pond_reeds_m + 1e-9 && off >= -d.pond_reeds_in_m - 1e-9) {
                ++reeds;
            } else if (under != Cover::Marsh && under != Cover::Water) {  // a marsh's or a bank's own reeds are theirs
                ++beyond;
            }
        }
        std::printf("    pond at (%.0f, %.0f): %d reed candidates in its margin, %d beyond it\n", p.e, p.n, reeds, beyond);
        SC_EXPECT(reeds > 10 && beyond == 0);
    }
}

SC_TEST("recipes: the loader refuses what is wrong") {
    const std::vector<std::string> kinds = {"grass", "clod"};
    Recipe r;
    std::string e;
    const auto make = [](const std::string& bands, const std::string& extra = "") {
        return "{\"recipe_id\": \"t\", \"applies\": {\"layers\": [\"meadow\"]}, \"bands\": [" + bands + "]" + extra +
               ", \"source\": \"game value: sam3d estimate\", \"confidence\": \"estimate\"}";
    };
    const std::string body = "{\"id\": \"body\", \"scatter\": [{\"kind\": \"grass\", \"process\": \"uniform\", \"per_ha\": 100}]}";
    SC_EXPECT(LoadRecipe(make(body), Layers(), kinds, r, &e));
    SC_EXPECT(r.bands.size() == 1 && r.bands[0].whole && r.bands[0].scatter[0].kind_index == 0);
    SC_EXPECT(!LoadRecipe(make("{\"id\": \"body\", \"scatter\": [{\"kind\": \"moss\", \"process\": \"uniform\", \"per_ha\": 100}]}"), Layers(), kinds, r, &e));
    SC_EXPECT(e.find("moss") != std::string::npos);
    SC_EXPECT(!LoadRecipe(make("{\"id\": \"body\", \"scatter\": [{\"kind\": \"grass\", \"process\": \"poisson\", \"per_ha\": 100}]}"), Layers(), kinds, r, &e));
    SC_EXPECT(!LoadRecipe(make("{\"id\": \"body\", \"scatter\": [{\"kind\": \"grass\", \"process\": \"thomas\", \"per_ha\": 100}]}"), Layers(), kinds, r, &e));
    SC_EXPECT(e.find("kappa") != std::string::npos);
    SC_EXPECT(LoadRecipe(make("{\"id\": \"body\", \"scatter\": [{\"kind\": \"grass\", \"process\": \"rows\", \"per_ha\": 13333, \"row_m\": 0.75}]}"), Layers(), kinds, r, &e));
    SC_EXPECT(!LoadRecipe(make("{\"id\": \"body\", \"scatter\": [{\"kind\": \"grass\", \"process\": \"rows\", \"per_ha\": 13333}]}"), Layers(), kinds, r, &e));
    SC_EXPECT(e.find("row_m") != std::string::npos);
    SC_EXPECT(!LoadRecipe(make("{\"id\": \"body\", \"scatter\": [{\"kind\": \"grass\", \"process\": \"rows\", \"per_ha\": 400000, \"row_m\": 0.75}]}"), Layers(), kinds, r, &e));
    SC_EXPECT(!LoadRecipe(make(body + ", " + body), Layers(), kinds, r, &e));  // twice
    SC_EXPECT(!LoadRecipe(make(body + ", {\"id\": \"edge\", \"from_m\": 0, \"to_m\": 1, \"scatter\": []}"), Layers(), kinds, r, &e));
    SC_EXPECT(e.find("never be reached") != std::string::npos);
    SC_EXPECT(!LoadRecipe(make("{\"id\": \"edge\", \"from_m\": 2, \"to_m\": 1, \"scatter\": []}, " + body), Layers(), kinds, r, &e));
    SC_EXPECT(!LoadRecipe(make("{\"id\": \"edge\", \"edge\": \"middle\", \"from_m\": 0, \"to_m\": 1, \"scatter\": []}, " + body), Layers(), kinds, r, &e));
    SC_EXPECT(!LoadRecipe(make(body, ", \"targets\": [{\"band\": \"rim\", \"kind\": \"grass\", \"per_ha\": [1, 2]}]"), Layers(), kinds, r, &e));
    SC_EXPECT(!LoadRecipe("{\"recipe_id\": \"t\", \"applies\": {\"layers\": [\"lava\"]}, \"bands\": [" + body + "], \"source\": \"x\", \"confidence\": \"y\"}", Layers(), kinds, r, &e));
    SC_EXPECT(!LoadRecipe("{\"recipe_id\": \"t\", \"applies\": {\"layers\": [\"meadow\"]}, \"bands\": [" + body + "]}", Layers(), kinds, r, &e));  // no source
    RecipeSet set;
    SC_EXPECT(!LoadRecipes(kData + "/scenery/no_recipes_here", Layers(), kinds, set, &e));
}
