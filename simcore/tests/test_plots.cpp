// The test plots (simcore/scenery/plots.hpp, roadmap L1): the data loads and refuses what is wrong, a field_layer plot is
// found whole in one parcel of its layer, and every plot's three cameras stand where they should, looking at its middle.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "simcore/scenery/plots.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::scenery;

namespace {

const std::string kData = SIMCORE_DATA_DIR;
constexpr int kDay = 288;  // 15 October (1985), the scenarios' day

const DressingRules& PlotDressing() {
    static DressingRules rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadDressingRules(kData + "/scenery/map_dressing.json", rules, &e)) std::printf("    %s\n", e.c_str());
    }
    return rules;
}

const GroundLayerRules& PlotLayers() {
    static GroundLayerRules rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadGroundLayers(kData + "/scenery/ground_layers.json", PlotDressing(), rules, &e)) std::printf("    %s\n", e.c_str());
    }
    return rules;
}

PlotSet Plots() {
    ViewSet views;
    std::string e;
    if (!LoadViews(kData + "/scenery/views.json", views, &e)) std::printf("    %s\n", e.c_str());
    PlotSet set;
    if (!LoadPlots(kData + "/scenery/plots.json", set, &e, &views)) std::printf("    %s\n", e.c_str());
    return set;
}

const PlotDef* Find(const PlotSet& set, const std::string& id) {
    for (const PlotDef& p : set.plots) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

}  // namespace

SC_TEST("plots: the data loads, each plot with photos from the views' register") {
    const PlotSet set = Plots();
    SC_EXPECT(set.plots.size() >= 1);
    SC_EXPECT(Find(set, "p01_ploughland") != nullptr);
    for (const PlotDef& p : set.plots) SC_EXPECT(!p.photos.empty());
}

SC_TEST("plots: the loader refuses a field_layer without its layer, an unknown kind, a photo not in the register") {
    ViewSet views;
    std::string e;
    SC_EXPECT(LoadViews(kData + "/scenery/views.json", views, &e));
    PlotSet set;
    SC_EXPECT(!LoadPlots("{\"plots\": [{\"id\": \"a\", \"anchor\": {\"kind\": \"field_layer\", \"close_km\": [0, 0]}}]}", set, &e));
    SC_EXPECT(!LoadPlots("{\"plots\": [{\"id\": \"a\", \"anchor\": {\"kind\": \"meadowland\"}}]}", set, &e));
    SC_EXPECT(!LoadPlots("{\"plots\": [{\"id\": \"a\", \"anchor\": {\"kind\": \"field_layer\", \"layer\": \"ploughed\", \"close_km\": [0, 0]},"
                         " \"photos\": [\"P99999\"]}]}", set, &e, &views));
    SC_EXPECT(LoadPlots("{\"plots\": [{\"id\": \"a\", \"anchor\": {\"kind\": \"road\", \"road\": \"f_vrbice\"}}]}", set, &e));
}

SC_TEST("plots: the ploughland plot is a whole 50 m square of one ploughed parcel, its cameras at 1.7, 25 and 150 m") {
    std::string e;
    std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (map == nullptr) return;
    const CoverRaster cover = BakeCover(*map, PlotDressing());
    const PlotSet set = Plots();
    const PlotDef* p = Find(set, "p01_ploughland");
    SC_EXPECT(p != nullptr);
    if (p == nullptr) return;
    const std::vector<ResolvedView> cams = ResolvePlot(*map, PlotDressing(), cover, PlotLayers(), kDay, *p);
    SC_EXPECT(cams.size() == 3);
    if (cams.size() != 3) return;
    const int ploughed = PlotLayers().Index("ploughed");
    const double heights[] = {1.7, 25.0, 150.0};
    for (std::size_t i = 0; i < cams.size(); ++i) {
        const ResolvedView& c = cams[i];
        SC_EXPECT(c.ok);
        if (!c.ok) {
            std::printf("    %s: %s\n", c.id.c_str(), c.missing_reason.c_str());
            continue;
        }
        SC_EXPECT_NEAR(c.eye_up_m, heights[i], 1e-9);
        // Every camera looks at the same middle, which is ploughed and well inside its parcel.
        SC_EXPECT_NEAR(c.target_e, cams[0].target_e, 1e-9);
        SC_EXPECT_NEAR(c.target_n, cams[0].target_n, 1e-9);
        SC_EXPECT(PointLayer(cover, PlotDressing(), PlotLayers(), c.target_e, c.target_n, kDay) == ploughed);
        // The low and high cameras look down at about 30 degrees.
        if (i > 0) {
            const double back = std::hypot(c.eye_e - c.target_e, c.eye_n - c.target_n);
            const double down = std::atan2(c.eye_up_m, back) * 180.0 / kPi;
            SC_EXPECT(down > 25.0 && down < 35.0);
        }
    }
    const CoverGrid* g = cover.GridAt(cams[0].target_e, cams[0].target_n);
    SC_EXPECT(g != nullptr);
    if (g == nullptr) return;
    const std::uint64_t parcel = FieldParcel(g->fields, cams[0].target_e, cams[0].target_n);
    for (double de = -25.0; de <= 25.0; de += 5.0) {
        for (double dn = -25.0; dn <= 25.0; dn += 5.0) {
            SC_EXPECT(FieldParcel(g->fields, cams[0].target_e + de, cams[0].target_n + dn) == parcel);
        }
    }
    // Resolved again, the same cameras (the plot is found the same way every time, on every machine).
    const std::vector<ResolvedView> again = ResolvePlot(*map, PlotDressing(), cover, PlotLayers(), kDay, *p);
    SC_EXPECT(again.size() == 3 && again[2].eye_e == cams[2].eye_e && again[2].eye_n == cams[2].eye_n);
}

SC_TEST("plots: every plot on the Kesmark map gives its three cameras, or says why not") {
    std::string e;
    std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (map == nullptr) return;
    const CoverRaster cover = BakeCover(*map, PlotDressing());
    for (const PlotDef& p : Plots().plots) {
        const std::vector<ResolvedView> cams = ResolvePlot(*map, PlotDressing(), cover, PlotLayers(), kDay, p);
        SC_EXPECT(cams.size() == 3);
        for (const ResolvedView& c : cams) {
            SC_EXPECT(c.ok || !c.missing_reason.empty());
            if (!c.ok) {
                std::printf("    (missing) %s: %s\n", c.id.c_str(), c.missing_reason.c_str());
                continue;
            }
            const double ground = map->GroundHeight(c.eye_e, c.eye_n);
            SC_EXPECT(map->ObstacleTop(c.eye_e, c.eye_n) < ground + c.eye_up_m);
        }
    }
}
