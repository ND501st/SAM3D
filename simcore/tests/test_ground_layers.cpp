// The ground's surface layers (simcore/scenery/ground_layers.hpp, roadmap T1): the rules, the season, and the mask the
// ground material reads: each cell's layer by its cover and crop, the furrows' heading and the field's own shade.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "simcore/scenery/far_cover.hpp"
#include "simcore/scenery/ground_layers.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::scenery;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

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

// The rules as JSON with `layers` and `crops` swapped in, for the loader's refusals.
std::string Rules(const std::string& layers, const std::string& crops) {
    return "{\"layers\": [" + layers +
           "], \"cover\": {\"forest\": \"a\", \"marsh\": \"a\", \"town\": \"a\", \"military\": \"a\", \"road\": \"a\", \"water\": \"a\"},"
           " \"crops\": {" + crops + "}}";
}

std::string AllCrops(const std::string& layer) {
    std::string s;
    for (const CropDef& c : Dressing().crops) {
        if (!s.empty()) s += ", ";
        s += "\"" + c.id + "\": {\"states\": [{\"layer\": \"" + layer + "\"}]}";
    }
    return s;
}

}  // namespace

SC_TEST("ground layers: the rules load from the data, and every cover and crop has a layer") {
    const GroundLayerRules& r = Layers();
    SC_EXPECT(!r.layers.empty());
    SC_EXPECT(r.layers.size() <= static_cast<std::size_t>(kMaxGroundLayers));
    for (int k = 0; k < static_cast<int>(Cover::Count); ++k) {
        const int layer = r.cover_layer[static_cast<std::size_t>(k)];
        SC_EXPECT(layer >= 0 && static_cast<std::size_t>(layer) < r.layers.size());
    }
    SC_EXPECT(r.calendar.size() == Dressing().crops.size());
    SC_EXPECT(r.layers[static_cast<std::size_t>(r.LayerOnDay(Cover::Forest, 0, 0, kDefaultGroundDay))].id == "forest_floor");
    for (const GroundLayerDef& d : r.layers) {
        SC_EXPECT(d.tile_m >= 0.25 && d.tile_m <= 64.0);
        // Whole repeats in the material's square, so the textures run on unbroken across its edges.
        const double repeats = kGroundTilePeriodM / d.tile_m;
        SC_EXPECT_NEAR(repeats, std::round(repeats), 1e-9);
    }
    SC_EXPECT(r.layers[static_cast<std::size_t>(r.Index("ploughed"))].rows);
    SC_EXPECT(!r.layers[static_cast<std::size_t>(r.Index("meadow"))].rows);
}

SC_TEST("ground layers: the loader refuses a layer twice, an unknown layer, a crop left out, too many layers and a repeat that breaks") {
    GroundLayerRules r;
    std::string e;
    SC_EXPECT(LoadGroundLayers(Rules("{\"id\": \"a\"}", AllCrops("a")), Dressing(), r, &e));
    SC_EXPECT(r.layers.size() == 1);
    SC_EXPECT(!LoadGroundLayers(Rules("{\"id\": \"a\"}, {\"id\": \"a\"}", AllCrops("a")), Dressing(), r, &e));
    SC_EXPECT(e.find("twice") != std::string::npos);
    SC_EXPECT(!LoadGroundLayers(Rules("{\"id\": \"a\"}", AllCrops("b")), Dressing(), r, &e));
    SC_EXPECT(e.find("no layer \"b\"") != std::string::npos);
    SC_EXPECT(!LoadGroundLayers(Rules("{\"id\": \"a\"}", "\"wheat\": {\"states\": [{\"layer\": \"a\"}]}"), Dressing(), r, &e));
    SC_EXPECT(e.find("crops: no layers for the crop") != std::string::npos);
    std::string many;
    for (int i = 0; i <= kMaxGroundLayers; ++i) many += std::string(i == 0 ? "" : ", ") + "{\"id\": \"l" + std::to_string(i) + "\"}";
    SC_EXPECT(!LoadGroundLayers(Rules(many, AllCrops("l0")), Dressing(), r, &e));
    SC_EXPECT(e.find("more than") != std::string::npos);
    SC_EXPECT(!LoadGroundLayers(Rules("{\"id\": \"a\", \"tile_m\": 0.1}", AllCrops("a")), Dressing(), r, &e));
    // 1.75 m (a scan's own size) leaves 342.9 repeats in 600 m, a seam every 600 m; 1.875 m fits 320.
    SC_EXPECT(!LoadGroundLayers(Rules("{\"id\": \"a\", \"tile_m\": 1.75}", AllCrops("a")), Dressing(), r, &e));
    SC_EXPECT(e.find("does not divide 600 m") != std::string::npos);
    GroundLayerRules fits;
    SC_EXPECT(LoadGroundLayers(Rules("{\"id\": \"a\", \"tile_m\": 1.875}", AllCrops("a")), Dressing(), fits, &e));
    SC_EXPECT_NEAR(fits.layers[0].tile_m, 1.875, 1e-12);
    // A refusal leaves the rules as they were.
    SC_EXPECT(r.layers.size() == 1 && r.layers[0].id == "a");
}

// The share of `fields` numbered fields of crop `crop` whose layer on `day` is the layer named `id`.
double CalendarShare(const GroundLayerRules& r, int crop, int day, const std::string& id) {
    int hit = 0;
    const int n = 4000;
    for (int k = 1; k <= n; ++k) {
        const std::uint64_t field = (static_cast<std::uint64_t>(Hash32(static_cast<std::uint32_t>(k))) << 32) | Hash32(static_cast<std::uint32_t>(k) ^ 0x9E3779B9u);
        hit += r.layers[static_cast<std::size_t>(r.LayerOnDay(Cover::Field, crop, field, day))].id == id ? 1 : 0;
    }
    return static_cast<double>(hit) / n;
}

int CropIndex(const std::string& id) {
    for (std::size_t i = 0; i < Dressing().crops.size(); ++i) {
        if (Dressing().crops[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

SC_TEST("ground layers: the crop calendar on 15 October is the patchwork of the photographs") {
    const GroundLayerRules& r = Layers();
    const int day = kDefaultGroundDay;
    const int wheat = CropIndex("wheat"), maize = CropIndex("maize"), sunflower = CropIndex("sunflower"), ploughed = CropIndex("ploughed"), pasture = CropIndex("pasture");
    SC_EXPECT(wheat >= 0 && maize >= 0 && sunflower >= 0 && ploughed >= 0 && pasture >= 0);
    // Every crop's state is one of its October layers.
    const auto october = [&](int crop, std::initializer_list<const char*> ids) {
        for (int k = 1; k <= 500; ++k) {
            const std::uint64_t field = (static_cast<std::uint64_t>(Hash32(static_cast<std::uint32_t>(k))) << 32) | Hash32(static_cast<std::uint32_t>(k) ^ 0x9E3779B9u);
            const std::string& id = r.layers[static_cast<std::size_t>(r.LayerOnDay(Cover::Field, crop, field, day))].id;
            bool ok = false;
            for (const char* want : ids) ok = ok || id == want;
            if (!ok) return false;
        }
        return true;
    };
    SC_EXPECT(october(wheat, {"stubble", "ploughed", "sown"}));
    SC_EXPECT(october(maize, {"dry_stalks", "maize_stubble", "ploughed"}));
    SC_EXPECT(october(sunflower, {"dry_stalks", "ploughed"}));
    SC_EXPECT(october(ploughed, {"ploughed", "sown"}));
    SC_EXPECT(october(pasture, {"meadow"}));
    // The research (T11): maize mostly in by mid-October (87 percent by the 16th), wheat stubble mostly ploughed under.
    const double maize_standing = CalendarShare(r, maize, day, "dry_stalks");
    SC_EXPECT(maize_standing > 0.05 && maize_standing < 0.25);
    SC_EXPECT(CalendarShare(r, wheat, day, "ploughed") > 0.6);
    SC_EXPECT(CalendarShare(r, wheat, day, "stubble") < 0.3);
    SC_EXPECT(CalendarShare(r, ploughed, day, "sown") > 0.1);
    std::printf("    15 October: %.0f%% of maize still standing, %.0f%% of wheat ground ploughed, %.0f%% sown\n", 100.0 * maize_standing,
                100.0 * CalendarShare(r, wheat, day, "ploughed"), 100.0 * CalendarShare(r, ploughed, day, "sown"));
}

SC_TEST("ground layers: a field's state never goes backwards through the year, and the year's start is the first state") {
    const GroundLayerRules& r = Layers();
    for (std::size_t crop = 0; crop < r.calendar.size(); ++crop) {
        const std::vector<GroundLayerRules::CropState>& states = r.calendar[crop];
        for (int k = 1; k <= 300; ++k) {
            const std::uint64_t field = (static_cast<std::uint64_t>(Hash32(static_cast<std::uint32_t>(k))) << 32) | Hash32(static_cast<std::uint32_t>(k) ^ 0x9E3779B9u);
            SC_EXPECT(r.LayerOnDay(Cover::Field, static_cast<int>(crop), field, 1) == states[0].layer);
            int last_state = 0;
            for (int day = 1; day <= 366; ++day) {
                int state = 0;  // the state in force: the last whose own day has come
                for (std::size_t i = 1; i < states.size(); ++i) {
                    if (r.OwnDay(states[i], static_cast<int>(i), field) <= day) state = static_cast<int>(i);
                }
                SC_EXPECT(state >= last_state);
                SC_EXPECT(r.LayerOnDay(Cover::Field, static_cast<int>(crop), field, day) == states[static_cast<std::size_t>(state)].layer);
                last_state = state;
            }
            // Each state is entered inside its window.
            for (std::size_t i = 1; i < states.size(); ++i) {
                const int own = r.OwnDay(states[i], static_cast<int>(i), field);
                SC_EXPECT(own >= states[i].from_day && own <= states[i].to_day);
            }
        }
    }
}

SC_TEST("ground layers: the calendar's loader refuses overlapping windows, a state without a window and a window off the year") {
    GroundLayerRules r;
    std::string e;
    const std::string layers = "{\"id\": \"a\"}, {\"id\": \"b\"}";
    const auto crops = [&](const std::string& states) {
        std::string s;
        for (const CropDef& c : Dressing().crops) s += std::string(s.empty() ? "" : ", ") + "\"" + c.id + "\": {\"states\": [" + states + "]}";
        return s;
    };
    SC_EXPECT(LoadGroundLayers(Rules(layers, crops("{\"layer\": \"a\"}, {\"layer\": \"b\", \"from_day\": 10, \"to_day\": 20}")), Dressing(), r, &e));
    SC_EXPECT(!LoadGroundLayers(Rules(layers, crops("{\"layer\": \"a\"}, {\"layer\": \"b\", \"from_day\": 10, \"to_day\": 20}, {\"layer\": \"a\", \"from_day\": 20, \"to_day\": 30}")), Dressing(), r, &e));
    SC_EXPECT(e.find("overlaps") != std::string::npos);
    SC_EXPECT(!LoadGroundLayers(Rules(layers, crops("{\"layer\": \"a\"}, {\"layer\": \"b\"}")), Dressing(), r, &e));
    SC_EXPECT(e.find("needs a from_day") != std::string::npos);
    SC_EXPECT(!LoadGroundLayers(Rules(layers, crops("{\"layer\": \"a\"}, {\"layer\": \"b\", \"from_day\": 300, \"to_day\": 400}")), Dressing(), r, &e));
    SC_EXPECT(e.find("1..366") != std::string::npos);
    SC_EXPECT(!LoadGroundLayers(Rules(layers, crops("")), Dressing(), r, &e));
    SC_EXPECT(e.find("no \"states\"") != std::string::npos);
}

SC_TEST("ground layers: the furrows' heading as a texel, half a turn round to the same furrows") {
    SC_EXPECT(HeadingTexel(0.0) == 0);
    SC_EXPECT(HeadingTexel(0.5 * kPi) == 128);
    SC_EXPECT(HeadingTexel(kPi - 1e-9) == 0);
    SC_EXPECT(HeadingTexel(kPi) == 0);
    SC_EXPECT(HeadingTexel(-0.25 * kPi) == 192);
    SC_EXPECT(HeadingTexel(0.25 * kPi + kPi) == 64);
}

SC_TEST("ground layers: a field runs along its furrows' heading, and its width across them") {
    // Walking 300 m both ways along the heading stays in the same field (1.2-2 km long) more often than 300 m both ways
    // across it (700-1000 m wide). The ground material turns the furrows' textures by this heading.
    FieldGrid grid;
    grid.seed = 0x5EEDu;
    int along = 0, across = 0, n = 0;
    for (int i = 0; i < 40; ++i) {
        for (int j = 0; j < 40; ++j) {
            const double e = 137.0 + i * 977.0, nn = -211.0 + j * 1013.0;
            const double h = FieldHeading(grid, e, nn);
            const double de = std::sin(h), dn = std::cos(h);
            const std::uint64_t p = FieldParcel(grid, e, nn);
            along += FieldParcel(grid, e + 300.0 * de, nn + 300.0 * dn) == p && FieldParcel(grid, e - 300.0 * de, nn - 300.0 * dn) == p;
            across += FieldParcel(grid, e + 300.0 * dn, nn - 300.0 * de) == p && FieldParcel(grid, e - 300.0 * dn, nn + 300.0 * de) == p;
            ++n;
        }
    }
    std::printf("    along %d of %d, across %d\n", along, n, across);
    SC_EXPECT(along > n * 4 / 10);
    SC_EXPECT(along > across + n / 6);
}

SC_TEST("ground layers: the mask carries each cell's layer, its furrows' heading and its field's own shade") {
    std::string e;
    std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (!map) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    const CoverRaster cover = BakeCover(*map, Dressing());
    SC_EXPECT(!cover.grids.empty());
    const GroundLayerRules& r = Layers();
    for (const CoverGrid& g : cover.grids) {
        const GroundMask m = BakeGroundMask(g, Dressing(), r, kDefaultGroundDay);
        SC_EXPECT(m.cols == g.cols && m.rows == g.rows);
        SC_EXPECT(m.texels.size() == static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows) * 4);
        SC_EXPECT(m.id == g.id);
        int bad = 0, fields = 0, rows = 0;
        for (int row = 0; row < g.rows; row += 7) {
            for (int c = 0; c < g.cols; c += 7) {
                const std::size_t i = g.Index(c, row);
                const Cover k = static_cast<Cover>(g.cover[i]);
                const std::uint8_t* t = m.At(c, row);
                const int layer = r.LayerOnDay(k, g.crop[i], k == Cover::Field ? FieldParcel(g.fields, g.CentreEast(c), g.CentreNorth(row)) : 0, kDefaultGroundDay);
                bad += t[0] != layer;
                if (k == Cover::Field) {
                    ++fields;
                    const double ce = g.CentreEast(c), cn = g.CentreNorth(row);
                    bad += t[1] != HeadingTexel(FieldHeading(g.fields, ce, cn));
                    bad += t[3] != (r.layers[static_cast<std::size_t>(layer)].rows ? 255 : 128);
                    bad += (t[2] >= 128) != (g.strips[i] != 0);
                    rows += t[3] == 255;
                } else {
                    bad += t[1] != 0 || t[2] != 64 || t[3] != 0;
                }
            }
        }
        std::printf("    %s: %d x %d cells, %d fields sampled, %d with rows\n", g.id.c_str(), g.cols, g.rows, fields, rows);
        SC_EXPECT(bad == 0);
        SC_EXPECT(fields > 0);
    }
    // One field, one shade: two cells of the same parcel carry the same B; the bake is the same twice.
    const CoverGrid& g = cover.grids.back();
    const GroundMask a = BakeGroundMask(g, Dressing(), r, 180), b = BakeGroundMask(g, Dressing(), r, 180);
    SC_EXPECT(a.texels == b.texels);
    int same = 0, pairs = 0;
    for (int row = 0; row + 1 < g.rows && pairs < 2000; row += 3) {
        for (int c = 0; c + 1 < g.cols && pairs < 2000; c += 3) {
            if (g.cover[g.Index(c, row)] != static_cast<std::uint8_t>(Cover::Field) || g.cover[g.Index(c + 1, row)] != static_cast<std::uint8_t>(Cover::Field)) continue;
            if (FieldParcel(g.fields, g.CentreEast(c), g.CentreNorth(row)) !=
                FieldParcel(g.fields, g.CentreEast(c + 1), g.CentreNorth(row))) {
                continue;
            }
            ++pairs;
            same += a.At(c, row)[2] == a.At(c + 1, row)[2];
        }
    }
    SC_EXPECT(pairs > 100);
    SC_EXPECT(same == pairs);
}

SC_TEST("far cover: the country's land cover over the far ground at 250 m, its fields the map's own fields past the map proper") {
    std::string e;
    const std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    const std::shared_ptr<const CountryMap> country = LoadCountryMap(kData, "maps/pannonia.json", &e);
    SC_EXPECT(map != nullptr && country != nullptr);
    if (!map || !country) return;
    const auto t0 = std::chrono::steady_clock::now();
    const CoverGrid far = BakeFarCover(*map, *country, Dressing());
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    SC_EXPECT(far.id == "far");
    SC_EXPECT(far.cols == 1200 && far.rows == 1200 && far.cell_m == 250.0);
    SC_EXPECT(far.cover.size() == static_cast<std::size_t>(far.cols) * static_cast<std::size_t>(far.rows));
    std::size_t by_cover[static_cast<std::size_t>(Cover::Count)] = {};
    for (const std::uint8_t c : far.cover) ++by_cover[c];
    std::printf("    the far cover in %.0f ms: %zu field, %zu forest, %zu marsh, %zu town, %zu water cells\n", ms,
                by_cover[static_cast<std::size_t>(Cover::Field)], by_cover[static_cast<std::size_t>(Cover::Forest)],
                by_cover[static_cast<std::size_t>(Cover::Marsh)], by_cover[static_cast<std::size_t>(Cover::Town)],
                by_cover[static_cast<std::size_t>(Cover::Water)]);
    // Open land is most of the country, and the country has woods, towns and water too.
    SC_EXPECT(by_cover[static_cast<std::size_t>(Cover::Field)] > far.cover.size() / 2);
    SC_EXPECT(by_cover[static_cast<std::size_t>(Cover::Forest)] > 0 && by_cover[static_cast<std::size_t>(Cover::Town)] > 0);
    // The fields are the map's fields: the crop at a field cell is the crop of the same field in the map proper's own cover.
    const CoverRaster raster = BakeCover(*map, Dressing());
    const CoverGrid& proper = raster.grids.back();
    int same_field = 0, same_crop = 0, looked = 0;
    for (int r = 0; r < proper.rows && looked < 4000; r += 17) {
        for (int c = 0; c < proper.cols && looked < 4000; c += 17) {
            const double ce = proper.CentreEast(c), cn = proper.CentreNorth(r);
            const std::size_t fi = far.IndexAt(ce, cn);
            // The far cell's crop is its centre's field: compare at the far cell's own centre against the map proper's cell there.
            const double fe = far.CentreEast(static_cast<int>(std::floor((ce - far.west_m) / far.cell_m))), fn = far.CentreNorth(static_cast<int>(std::floor((cn - far.south_m) / far.cell_m)));
            const std::size_t pc = proper.IndexAt(fe, fn);
            if (proper.cover[pc] != static_cast<std::uint8_t>(Cover::Field) || far.cover[fi] != static_cast<std::uint8_t>(Cover::Field)) continue;
            ++looked;
            same_field += FieldAt(far.fields, fe, fn).id == FieldAt(proper.fields, fe, fn).id ? 1 : 0;
            same_crop += far.crop[fi] == proper.crop[pc] ? 1 : 0;
        }
    }
    std::printf("    the far cover against the map proper: %d of %d fields, %d crops the same\n", same_field, looked, same_crop);
    SC_EXPECT(looked > 500);
    SC_EXPECT(same_field == looked);  // one field grid near and far
    // The map proper's 25 m cell round the far cell's centre can lie across a field's edge from it: nearly all, not all.
    SC_EXPECT(same_crop * 100 >= looked * 95);
    // A mask baked from it is the same twice (every client).
    const GroundMask a = BakeGroundMask(far, Dressing(), Layers(), 288), b = BakeGroundMask(far, Dressing(), Layers(), 288);
    SC_EXPECT(a.texels == b.texels && a.cols == far.cols);
}
