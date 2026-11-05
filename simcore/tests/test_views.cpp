// The terrain's check views (simcore/scenery/views.hpp, roadmap T2): the data loads and refuses what is wrong, and every
// view on the Kesmark map resolves to a camera above the ground and outside what stands there, or says why it cannot.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "simcore/scenery/views.hpp"
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

ViewSet Views() {
    ViewSet set;
    std::string e;
    if (!LoadViews(kData + "/scenery/views.json", set, &e)) std::printf("    %s\n", e.c_str());
    return set;
}

// One view as JSON with `anchor` and the rest swapped in, for the loader's refusals.
std::string One(const std::string& anchor, const std::string& rest = "") {
    return "{\"views\": [{\"id\": \"v\", \"anchor\": " + anchor + rest + "}]}";
}

}  // namespace

SC_TEST("views: the data loads, with every photo a view names listed with its page, licence and author") {
    const ViewSet set = Views();
    SC_EXPECT(set.views.size() == 12);
    for (const ViewDef& v : set.views) {
        SC_EXPECT(!v.photos.empty());
        for (const std::string& id : v.photos) {
            const PhotoRef* p = set.Photo(id);
            SC_EXPECT(p != nullptr);
            if (p == nullptr) continue;
            // Only openly licensed photographs, linked and never copied.
            const bool open = p->licence.compare(0, 3, "CC0") == 0 || p->licence.compare(0, 5, "CC BY") == 0 || p->licence == "Public domain";
            SC_EXPECT(p->licence.find("NC") == std::string::npos && p->licence.find("ND") == std::string::npos);
            SC_EXPECT(open);
            SC_EXPECT(p->page_url.compare(0, 8, "https://") == 0);
        }
    }
}

SC_TEST("views: the loader refuses what is wrong") {
    ViewSet set;
    std::string e;
    SC_EXPECT(LoadViews(One("{\"kind\": \"site\", \"site\": \"bty21\"}"), set, &e));
    SC_EXPECT(!LoadViews(One("{\"kind\": \"moon\"}"), set, &e));
    SC_EXPECT(!LoadViews(One("{\"kind\": \"site\"}"), set, &e));                                   // no site named
    SC_EXPECT(!LoadViews(One("{\"kind\": \"parcel_edge\"}"), set, &e));                            // no close_km
    SC_EXPECT(!LoadViews(One("{\"kind\": \"site\", \"site\": \"a\"}", ", \"look\": \"up\""), set, &e));
    SC_EXPECT(!LoadViews(One("{\"kind\": \"site\", \"site\": \"a\"}", ", \"fov_deg\": 5"), set, &e));
    SC_EXPECT(!LoadViews(One("{\"kind\": \"site\", \"site\": \"a\"}", ", \"eye_m\": 0"), set, &e));
    SC_EXPECT(!LoadViews(One("{\"kind\": \"site\", \"site\": \"a\"}", ", \"photos\": [\"P99\"]"), set, &e));  // not listed
    SC_EXPECT(!LoadViews("{\"views\": []}", set, &e));
    SC_EXPECT(!LoadViews("{\"views\": [{\"id\": \"v\", \"anchor\": {\"kind\": \"site\", \"site\": \"a\"}},"
                         " {\"id\": \"v\", \"anchor\": {\"kind\": \"site\", \"site\": \"a\"}}]}", set, &e));  // twice
    SC_EXPECT(!LoadViews(kData + "/scenery/nothing.json", set, &e));
}

SC_TEST("views: every view on the Kesmark map resolves to a camera above the ground, outside what stands there, or says why not") {
    std::string e;
    std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (!map) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    const CoverRaster cover = BakeCover(*map, Dressing());
    const ViewSet set = Views();
    int resolved = 0;
    for (const ViewDef& v : set.views) {
        const std::vector<ResolvedView> cams = ResolveView(*map, Dressing(), cover, v);
        SC_EXPECT(!cams.empty());
        for (const ResolvedView& c : cams) {
            SC_EXPECT(c.id.compare(0, v.id.size(), v.id) == 0);
            if (!c.ok) {
                SC_EXPECT(!c.missing_reason.empty());
                std::printf("    %s: %s\n", c.id.c_str(), c.missing_reason.c_str());
                continue;
            }
            ++resolved;
            SC_EXPECT(c.eye_up_m >= 0.5);
            const double ground = map->GroundHeight(c.eye_e, c.eye_n);
            SC_EXPECT(map->ObstacleTop(c.eye_e, c.eye_n) < ground + c.eye_up_m - 0.25);
            SC_EXPECT(std::hypot(c.target_e - c.eye_e, c.target_n - c.eye_n) > 1.0);
            SC_EXPECT(c.fov_rad > 0.3 && c.fov_rad < 2.1);
        }
    }
    SC_EXPECT(resolved >= 8);
}

SC_TEST("views: a site anchor with from_m and a bearing puts the eye that far off the site on that bearing (V7 clear of the net)") {
    std::string e;
    std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (!map) return;
    const CoverRaster cover = BakeCover(*map, Dressing());
    const ViewSet set = Views();
    const MapSiteDef* site = nullptr;
    for (const MapSiteDef& s : map->Def().sites) site = s.id == "bty21" ? &s : site;
    SC_EXPECT(site != nullptr);
    int seen = 0;
    for (const ViewDef& v : set.views) {
        if (v.id != "v7_osa_position" || site == nullptr) continue;
        for (const ResolvedView& c : ResolveView(*map, Dressing(), cover, v)) {
            ++seen;
            const double de = c.eye_e - site->centre.east_m, dn = c.eye_n - site->centre.north_m;
            SC_EXPECT_NEAR(std::hypot(de, dn), 25.0, 0.01);
            SC_EXPECT_NEAR(std::atan2(de, dn), 110.0 * kPi / 180.0, 1e-6);  // east of north
        }
    }
    SC_EXPECT(seen == 4);  // the panorama's four bearings
}

SC_TEST("views: the wood-edge view stands in the open with a wood ahead, the edge 25 m on") {
    std::string e;
    std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (!map) return;
    const CoverRaster cover = BakeCover(*map, Dressing());
    int seen = 0;
    for (const ViewDef& v : Views().views) {
        if (v.id != "v12_wood_edge") continue;
        for (const ResolvedView& c : ResolveView(*map, Dressing(), cover, v)) {
            ++seen;
            SC_EXPECT(c.ok);
            if (!c.ok) continue;
            SC_EXPECT(map->ObstacleHeight(c.eye_e, c.eye_n) < 1.7);  // in the open
            // Walking on along the look (north) meets something over 3 m within 60 m: the wood.
            bool wood = false;
            for (double t = 5.0; t <= 60.0; t += 5.0) {
                const double pn = c.eye_n + t;
                wood = wood || map->ObstacleHeight(c.eye_e, pn) > 3.0;
            }
            SC_EXPECT(wood);
        }
    }
    SC_EXPECT(seen == 1);
}

SC_TEST("views: the farmyard view says why it is missing (no farmyard site kind yet), the rest are given") {
    std::string e;
    std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (!map) return;
    const CoverRaster cover = BakeCover(*map, Dressing());
    const ViewSet set = Views();
    for (const ViewDef& v : set.views) {
        const std::vector<ResolvedView> cams = ResolveView(*map, Dressing(), cover, v);
        if (v.anchor.kind == "farmyard") {
            SC_EXPECT(!cams.front().ok);
            SC_EXPECT(cams.front().missing_reason.find("farmyard") != std::string::npos);
        } else {
            for (const ResolvedView& c : cams) SC_EXPECT(c.ok);
        }
    }
}

SC_TEST("views: an anchor is the map's own, so a view follows what it names") {
    std::string e;
    std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (!map) return;
    const CoverRaster cover = BakeCover(*map, Dressing());
    ViewSet set;
    SC_EXPECT(LoadViews(One("{\"kind\": \"settlement\", \"settlement\": \"vrbice\", \"from_m\": 1000, \"bearing_deg\": 90}"), set, &e));
    const std::vector<ResolvedView> a = ResolveView(*map, Dressing(), cover, set.views[0]);
    SC_EXPECT(a.size() == 1 && a[0].ok);
    for (const MapSettlementDef& s : map->Def().settlements) {
        if (s.id != "vrbice") continue;
        // 1000 m east of the village, looking back west at it.
        SC_EXPECT_NEAR(a[0].eye_e, s.centre.east_m + 1000.0, 6.0);
        SC_EXPECT_NEAR(a[0].eye_n, s.centre.north_m, 6.0);
        SC_EXPECT_NEAR(a[0].target_e, s.centre.east_m, 1.0);
    }
    SC_EXPECT(LoadViews(One("{\"kind\": \"settlement\", \"settlement\": \"nowhere\", \"from_m\": 1000, \"bearing_deg\": 90}"), set, &e));
    const std::vector<ResolvedView> b = ResolveView(*map, Dressing(), cover, set.views[0]);
    SC_EXPECT(b.size() == 1 && !b[0].ok);
    SC_EXPECT(b[0].missing_reason.find("nowhere") != std::string::npos);
}

SC_TEST("views: a panorama gives a camera every step, named by its bearing, and a parcel's edge is where the parcel ends") {
    std::string e;
    std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (!map) return;
    const CoverRaster cover = BakeCover(*map, Dressing());
    const ViewSet set = Views();
    for (const ViewDef& v : set.views) {
        if (v.look == "panorama") {
            const std::vector<ResolvedView> cams = ResolveView(*map, Dressing(), cover, v);
            SC_EXPECT(cams.size() == 4);  // step_deg 90
            SC_EXPECT(cams.front().id == v.id + "_h000");
            SC_EXPECT(cams.back().id == v.id + "_h270");
        }
        if (v.anchor.kind == "parcel_edge") {
            const std::vector<ResolvedView> cams = ResolveView(*map, Dressing(), cover, v);
            SC_EXPECT(cams.size() == 1 && cams[0].ok);
            if (!cams[0].ok) continue;
            const CoverGrid* g = cover.GridAt(cams[0].eye_e, cams[0].eye_n);
            SC_EXPECT(g != nullptr);
            if (g == nullptr) continue;
            // 2 m on, along the view, is a different parcel from 6 m on: the eye stands at an edge, looking across the next.
            const double se = std::sin(cams[0].bearing_rad), cn = std::cos(cams[0].bearing_rad);
            const std::uint64_t here = FieldParcel(g->fields, cams[0].eye_e, cams[0].eye_n);
            const std::uint64_t ahead = FieldParcel(g->fields, cams[0].eye_e + 4.0 * se, cams[0].eye_n + 4.0 * cn);
            SC_EXPECT(here != ahead);
        }
    }
}
