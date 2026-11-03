// The ground's surface layers (simcore/scenery/ground_layers.hpp, roadmap T1): the rules, the season, and the mask the
// ground material reads: each cell's layer by its cover and crop, the furrows' heading and the field's own shade.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

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
        s += "\"" + c.id + "\": {\"summer\": \"" + layer + "\", \"autumn\": \"" + layer + "\"}";
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
    SC_EXPECT(r.crop_summer.size() == Dressing().crops.size());
    SC_EXPECT(r.crop_autumn.size() == Dressing().crops.size());
    // The plain in October: the wheat is stubble, a meadow stays a meadow.
    SC_EXPECT(r.layers[static_cast<std::size_t>(r.LayerAt(Cover::Field, 0, Season::Autumn))].id == "stubble");
    SC_EXPECT(r.layers[static_cast<std::size_t>(r.LayerAt(Cover::Forest, 0, Season::Autumn))].id == "forest_floor");
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
    SC_EXPECT(!LoadGroundLayers(Rules("{\"id\": \"a\"}", "\"wheat\": {\"summer\": \"a\", \"autumn\": \"a\"}"), Dressing(), r, &e));
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

SC_TEST("ground layers: the season by the scenario's month") {
    const GroundLayerRules& r = Layers();
    SC_EXPECT(r.SeasonOfMonth(10) == Season::Autumn);
    SC_EXPECT(r.SeasonOfMonth(9) == Season::Autumn);
    SC_EXPECT(r.SeasonOfMonth(7) == Season::Summer);
    SC_EXPECT(r.SeasonOfMonth(1) == Season::Summer);
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
    const DressingRules& d = Dressing();
    const std::uint64_t seed = 0x5EEDull;
    int along = 0, across = 0, n = 0;
    for (int i = 0; i < 40; ++i) {
        for (int j = 0; j < 40; ++j) {
            const double e = 137.0 + i * 977.0, nn = -211.0 + j * 1013.0;
            const double h = FieldHeading(d, seed, e, nn);
            const double de = std::sin(h), dn = std::cos(h);
            const std::uint64_t p = FieldParcel(d, seed, e, nn);
            along += FieldParcel(d, seed, e + 300.0 * de, nn + 300.0 * dn) == p && FieldParcel(d, seed, e - 300.0 * de, nn - 300.0 * dn) == p;
            across += FieldParcel(d, seed, e + 300.0 * dn, nn - 300.0 * de) == p && FieldParcel(d, seed, e - 300.0 * dn, nn + 300.0 * de) == p;
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
        const GroundMask m = BakeGroundMask(g, Dressing(), r, Season::Autumn);
        SC_EXPECT(m.cols == g.cols && m.rows == g.rows);
        SC_EXPECT(m.texels.size() == static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows) * 4);
        SC_EXPECT(m.id == g.id);
        int bad = 0, fields = 0, rows = 0;
        for (int row = 0; row < g.rows; row += 7) {
            for (int c = 0; c < g.cols; c += 7) {
                const std::size_t i = g.Index(c, row);
                const Cover k = static_cast<Cover>(g.cover[i]);
                const std::uint8_t* t = m.At(c, row);
                const int layer = r.LayerAt(k, g.crop[i], Season::Autumn);
                bad += t[0] != layer;
                if (k == Cover::Field) {
                    ++fields;
                    const double ce = g.CentreEast(c), cn = g.CentreNorth(row);
                    bad += t[1] != HeadingTexel(FieldHeading(Dressing(), g.parcel_seed, ce, cn));
                    bad += t[3] != (r.layers[static_cast<std::size_t>(layer)].rows ? 255 : 0);
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
    const GroundMask a = BakeGroundMask(g, Dressing(), r, Season::Summer), b = BakeGroundMask(g, Dressing(), r, Season::Summer);
    SC_EXPECT(a.texels == b.texels);
    int same = 0, pairs = 0;
    for (int row = 0; row + 1 < g.rows && pairs < 2000; row += 3) {
        for (int c = 0; c + 1 < g.cols && pairs < 2000; c += 3) {
            if (g.cover[g.Index(c, row)] != static_cast<std::uint8_t>(Cover::Field) || g.cover[g.Index(c + 1, row)] != static_cast<std::uint8_t>(Cover::Field)) continue;
            if (FieldParcel(Dressing(), g.parcel_seed, g.CentreEast(c), g.CentreNorth(row)) !=
                FieldParcel(Dressing(), g.parcel_seed, g.CentreEast(c + 1), g.CentreNorth(row))) {
                continue;
            }
            ++pairs;
            same += a.At(c, row)[2] == a.At(c + 1, row)[2];
        }
    }
    SC_EXPECT(pairs > 100);
    SC_EXPECT(same == pairs);
}
