// The map's leftovers (roadmap S9, R6's Left): ITU-R P.1812's clutter heights checked against the text and a class per
// cell from the country's land cover on its own ground (simcore/clutter.hpp, CountryMap::BakeClutter, CountryGround);
// the three Rava-bank positions as map sites whose levelled ground stops short of the river's bed; no country's line
// drawn twice where the map lays it as its own (MapData::LaysCountryLinesAt); the land cover's lines at least a cell wide
// and unbroken (scenery/map_dressing.hpp StampCrossed); the ground material's detail by cover (CoverDetail).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/clutter.hpp"
#include "simcore/command/em_radio.hpp"
#include "simcore/country_map.hpp"
#include "simcore/em/propagation.hpp"
#include "simcore/radar.hpp"
#include "simcore/scenery/map_dressing.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

const MapData* Kesmark() {
    static std::shared_ptr<const MapData> map;
    if (!map) {
        std::string e;
        map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
        if (!map) std::printf("    %s\n", e.c_str());
    }
    return map.get();
}

std::shared_ptr<const CountryMap> Country() {
    static std::shared_ptr<const CountryMap> map;
    if (!map) {
        std::string e;
        map = LoadCountryMap(kData, "maps/pannonia.json", &e);
        if (!map) std::printf("    %s\n", e.c_str());
    }
    return map;
}

ClutterHeights Heights() {
    ClutterHeights h;
    std::string e;
    if (!LoadClutterHeights(kData, h, &e)) std::printf("    %s\n", e.c_str());
    return h;
}

const MapSiteDef* Site(const MapData& m, const std::string& id) {
    for (const MapSiteDef& s : m.Def().sites) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

}  // namespace

SC_TEST("clutter: the heights are P.1812's Table 2, a town's middle dense urban, a village suburban, a bad file refused") {
    const ClutterHeights h = Heights();
    // ITU-R P.1812-6 section 3.2, Table 2 (P.1812-8 section 3.2.1): water/sea 0, open/rural 0, suburban 10,
    // urban/trees/forest 15, dense urban 20 m.
    SC_EXPECT(h.water_sea_m == 0.0 && h.open_rural_m == 0.0 && h.suburban_m == 10.0 && h.urban_trees_forest_m == 15.0 && h.dense_urban_m == 20.0);
    SC_EXPECT(h.HeightOf(ClutterClass::UrbanTreesForest) == 15.0 && h.Tallest() == 20.0);
    SC_EXPECT_NEAR(h.dense_share, 0.45, 1e-12);
    SC_EXPECT(TownClutterClass("city", 100.0, 3000.0, 0.45) == ClutterClass::DenseUrban);
    SC_EXPECT(TownClutterClass("town", 1400.0, 3000.0, 0.45) == ClutterClass::Suburban);
    SC_EXPECT(TownClutterClass("village", 10.0, 800.0, 0.45) == ClutterClass::Suburban);
    SC_EXPECT(TownClutterClass("capital", 7000.0, 7000.0, 0.45) == ClutterClass::OpenRural);
    ClutterHeights bad = h;
    std::string e;
    SC_EXPECT(!ParseClutterHeights("{\"values\": {\"suburban_m\": {\"value\": 10}}}", bad, &e) && e.find("water_sea_m") != std::string::npos);
    SC_EXPECT(!ParseClutterHeights(
        "{\"values\": {\"water_sea_m\": {\"value\": 0}, \"open_rural_m\": {\"value\": 0}, \"suburban_m\": {\"value\": -1}, "
        "\"urban_trees_forest_m\": {\"value\": 15}, \"dense_urban_m\": {\"value\": 20}, \"dense_share\": {\"value\": 0.45}}}",
        bad, &e));
    SC_EXPECT(bad.suburban_m == 10.0);  // untouched
    ClutterHeights none;
    SC_EXPECT(none.Tallest() == 0.0);  // no file: no clutter
}

SC_TEST("clutter: the country's land cover as a class per cell, the same as asked point by point") {
    const std::shared_ptr<const CountryMap> c = Country();
    SC_EXPECT(c != nullptr);
    if (!c) return;
    const ClutterRaster r = c->BakeClutter(250.0, 0.45);
    SC_EXPECT(r.cols > 1000 && r.rows > 1000);
    std::vector<int> per(static_cast<std::size_t>(ClutterClass::Count), 0);
    int checked = 0, agree = 0;
    for (int y = 0; y < r.rows; ++y) {
        for (int x = 0; x < r.cols; ++x) {
            const ClutterClass k = r.ClassAt(r.CentreEast(x), r.CentreNorth(y));
            ++per[static_cast<std::size_t>(k)];
            if ((x * 7 + y * 13) % 41 != 0) continue;
            ++checked;
            agree += k == c->ClutterClassAt(r.CentreEast(x), r.CentreNorth(y), 0.45) ? 1 : 0;
        }
    }
    std::printf("    the country at 250 m:");
    for (std::size_t k = 0; k < per.size(); ++k) std::printf(" %s %d", ClutterClassName(static_cast<ClutterClass>(k)), per[k]);
    std::printf("; %d of %d cells as asked point by point\n", agree, checked);
    SC_EXPECT(checked > 10000 && agree == checked);
    SC_EXPECT(per[static_cast<std::size_t>(ClutterClass::UrbanTreesForest)] > 1000 && per[static_cast<std::size_t>(ClutterClass::DenseUrban)] > 100 &&
              per[static_cast<std::size_t>(ClutterClass::Suburban)] > 100 && per[static_cast<std::size_t>(ClutterClass::WaterSea)] > 10);
    const CountrySettlementDef* cap = c->Capital();
    SC_EXPECT(cap != nullptr);
    if (cap != nullptr) SC_EXPECT(r.ClassAt(cap->centre.east_m + 10.0, cap->centre.north_m + 10.0) == ClutterClass::DenseUrban);
    for (const CountrySettlementDef& s : c->Def().settlements) {
        if (s.kind != "village") continue;
        const ClutterClass k = c->ClutterClassAt(s.centre.east_m, s.centre.north_m, 0.45);
        SC_EXPECT(k == ClutterClass::Suburban || k == ClutterClass::UrbanTreesForest || k == ClutterClass::WaterSea);
    }
}

SC_TEST("clutter: the country's own ground carries its woods and towns, a forest between two low posts hides one from the other") {
    const std::shared_ptr<const CountryMap> c = Country();
    if (!c) return;
    const command::CountryGround bare(c);
    const command::CountryGround cluttered(c, Heights());
    SC_EXPECT(bare.ObstacleTallestM() == 0.0 && cluttered.ObstacleTallestM() == 20.0);
    const CountrySettlementDef* cap = c->Capital();
    if (cap != nullptr) {
        SC_EXPECT(cluttered.ObstacleHeightAt(cap->centre.east_m, cap->centre.north_m) == 20.0);
        SC_EXPECT(bare.ObstacleHeightAt(cap->centre.east_m, cap->centre.north_m) == 0.0);
    }
    // Two posts 1.2 km apart, 8 m over the ground, either side of a wooded cell: the bare ground lets them see each other
    // where it is flat enough; the forest's 15 m stands between them.
    const ClutterRaster& r = cluttered.Clutter();
    int pairs = 0, hidden = 0, open_pairs = 0, open_same = 0;
    for (int y = 2; y < r.rows - 2; y += 9) {
        for (int x = 2; x < r.cols - 2; x += 9) {
            const double e = r.CentreEast(x), n = r.CentreNorth(y);
            const ClutterClass k = r.ClassAt(e, n);
            const Vec3 a(e - 600.0, n, 0.0), b(e + 600.0, n, 0.0);
            const double ha = cluttered.HeightAt(a.x, a.y) + 8.0, hb = cluttered.HeightAt(b.x, b.y) + 8.0;
            if (!HasLineOfSight(bare, a, ha, b, hb)) continue;
            if (k == ClutterClass::UrbanTreesForest && r.ClassAt(e - 250.0, n) == k && r.ClassAt(e + 250.0, n) == k) {
                ++pairs;
                hidden += HasLineOfSight(cluttered, a, ha, b, hb) ? 0 : 1;
            }
            bool all_open = true;
            for (double s = -600.0; s <= 600.0 && all_open; s += 125.0) all_open = r.ClassAt(e + s, n) == ClutterClass::OpenRural;
            if (all_open && open_pairs < 400) {
                ++open_pairs;
                open_same += HasLineOfSight(cluttered, a, ha, b, hb) ? 1 : 0;
            }
        }
    }
    std::printf("    across a wood: %d of %d pairs hidden; over open ground: %d of %d pairs still in sight\n", hidden, pairs, open_same, open_pairs);
    SC_EXPECT(pairs > 20 && hidden == pairs);
    SC_EXPECT(open_pairs > 100 && open_same == open_pairs);
}

SC_TEST("map: the three Rava-bank positions are sites, cleared, their levelled ground short of the river's bed, their pits dry") {
    const MapData* m = Kesmark();
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    const MapDef& d = m->Def();
    // The rivers whose beds NaturalGround carves: the map's own and its far ground's.
    std::vector<const MapLineDef*> rivers;
    for (const MapLineDef& r : d.rivers) rivers.push_back(&r);
    if (d.far_ground) {
        for (const MapLineDef& r : d.far_ground->rivers) rivers.push_back(&r);
    }
    const auto from_bed = [&](double e, double n, const MapLineDef** nearest) {
        double best = 1e300;
        for (const MapLineDef* r : rivers) {
            const double v = mapgeo::DistToLine(r->points, e, n) - (0.5 * r->width_m + kRiverBedBankM);
            if (v < best) {
                best = v;
                if (nearest != nullptr) *nearest = r;
            }
        }
        return best;
    };
    // No site's levelled ground reaches a river's bed.
    for (const MapData::Flat& f : m->Flats()) SC_EXPECT(f.r <= from_bed(f.e, f.n, nullptr) + 1e-9);
    for (const char* id : {"capital_zu23_bridge", "capital_zu23_port", "capital_s60_6"}) {
        const MapSiteDef* s = Site(*m, id);
        SC_EXPECT(s != nullptr);
        if (s == nullptr) continue;
        const MapLineDef* river = nullptr;
        const double bed = from_bed(s->centre.east_m, s->centre.north_m, &river);
        // The bed at the river's nearest point is as nature carved it: no flat raised it.
        double best = 1e300, be = 0.0, bn = 0.0;
        for (std::size_t i = 1; river != nullptr && i < river->points.size(); ++i) {
            double along = 0.0;
            const MapPoint& p = river->points[i - 1];
            const MapPoint& q = river->points[i];
            const double dist = mapgeo::DistToSegment(s->centre.east_m, s->centre.north_m, p.east_m, p.north_m, q.east_m, q.north_m, &along);
            if (dist < best) {
                best = dist;
                const double len = std::hypot(q.east_m - p.east_m, q.north_m - p.north_m);
                const double t = len > 0.0 ? std::min(1.0, std::max(0.0, along / len)) : 0.0;
                be = p.east_m + t * (q.east_m - p.east_m);
                bn = p.north_m + t * (q.north_m - p.north_m);
            }
        }
        const double raised = m->Analytic(be, bn) - NaturalGround(d, be, bn);
        // Nothing stands on it, and its pits are dug, none within 25 m of the water.
        int pits = 0, wet = 0;
        for (const MapBox& b : m->Layout().boxes) {
            if (b.part != MapPart::Revetment || std::hypot(b.east_m - s->centre.east_m, b.north_m - s->centre.north_m) > 90.0) continue;
            ++pits;
            wet += m->NearWater(b.east_m, b.north_m, 20.0) ? 1 : 0;
        }
        std::printf("    %s: %.0f m from the river's bed, the bed raised %.3f m; %.1f m standing on it; %d pit walls, %d near the water\n", id, bed,
                    raised, m->ObstacleHeight(s->centre.east_m, s->centre.north_m), pits, wet);
        SC_EXPECT(bed < 60.0);  // on the bank
        SC_EXPECT(std::fabs(raised) < 1e-9);
        SC_EXPECT(m->ObstacleHeight(s->centre.east_m, s->centre.north_m) < 1.0);
        SC_EXPECT(pits == 16 && wet == 0);  // four pits of four walls
    }
}

SC_TEST("map: no country's line is drawn twice: where the map lays it as its own, and only there, the country's is left out") {
    const MapData* m = Kesmark();
    if (m == nullptr) return;
    const MapDef& d = m->Def();
    SC_EXPECT(d.far_ground != nullptr);
    if (!d.far_ground) return;
    const MapData::DetailLines& imported = m->ImportedLines();
    const auto imported_near = [&](const std::string& id, double e, double n) {
        double best = 1e300;
        for (const std::vector<MapLineDef>* list : {&imported.roads, &imported.railways, &imported.rivers}) {
            for (const MapLineDef& l : *list) {
                if (l.id != id && l.id.rfind(id + "#", 0) != 0) continue;
                best = std::min(best, mapgeo::DistToLine(l.points, e, n));
            }
        }
        return best;
    };
    int band = 0, band_laid = 0, out = 0, out_laid = 0, in_core = 0;
    for (const std::vector<MapLineDef>* list : {&d.far_ground->roads, &d.far_ground->railways, &d.far_ground->rivers}) {
        for (const MapLineDef& l : *list) {
            for (std::size_t i = 1; i < l.points.size(); ++i) {
                const MapPoint& p = l.points[i - 1];
                const MapPoint& q = l.points[i];
                const double len = std::hypot(q.east_m - p.east_m, q.north_m - p.north_m);
                for (double s = 0.0; s < len; s += 100.0) {
                    const double e = p.east_m + (q.east_m - p.east_m) * s / len, n = p.north_m + (q.north_m - p.north_m) * s / len;
                    if (d.InCore(e, n)) {
                        in_core += m->LaysCountryLinesAt(e, n) ? 1 : 0;
                        continue;
                    }
                    if (d.InDetail(e, n)) continue;
                    if (m->LaysCountryLinesAt(e, n)) {
                        ++band;  // the kilometre round the capital: the map's imported line lies here
                        band_laid += imported_near(l.id, e, n) < 1.0 ? 1 : 0;
                    } else {
                        ++out;  // the country's ribbon is drawn here: the map lays none of it
                        // Within 50 m of the band's edge the map's line ends where the country's begins (clipped there).
                        const bool at_edge = m->LaysCountryLinesAt(e + 50.0, n) || m->LaysCountryLinesAt(e - 50.0, n) ||
                                             m->LaysCountryLinesAt(e, n + 50.0) || m->LaysCountryLinesAt(e, n - 50.0);
                        out_laid += imported_near(l.id, e, n) < (at_edge ? 1.0 : 50.0) ? 1 : 0;
                    }
                }
            }
        }
    }
    std::printf("    the country's lines every 100 m: %d points in the kilometre round the capital, %d of them laid by the map; %d beyond, %d "
                "laid; %d in the map proper\n",
                band, band_laid, out, out_laid, in_core);
    SC_EXPECT(band > 50 && band_laid == band);
    SC_EXPECT(out > 1000 && out_laid == 0);
    SC_EXPECT(in_core > 100);
    // Every point of the map's imported lines lies where the map lays them.
    int strays = 0;
    for (const std::vector<MapLineDef>* list : {&imported.roads, &imported.railways, &imported.rivers}) {
        for (const MapLineDef& l : *list) {
            for (const MapPoint& p : l.points) strays += m->LaysCountryLinesAt(p.east_m, p.north_m) ? 0 : 1;
        }
    }
    SC_EXPECT(strays == 0);
    // Beyond the kilometre the country's own.
    const MapDetailDef* cap = nullptr;
    for (const MapDetailDef& a : d.detail) cap = a.id == "capital" ? &a : cap;
    SC_EXPECT(cap != nullptr);
    if (cap != nullptr) {
        SC_EXPECT(m->LaysCountryLinesAt(cap->centre.east_m + cap->half_m + 900.0, cap->centre.north_m));
        SC_EXPECT(!m->LaysCountryLinesAt(cap->centre.east_m + cap->half_m + 1100.0, cap->centre.north_m));
    }
}

SC_TEST("water: the map's river and its country's lakes, and the country's own ground's, are water to a path (E9's sea areas)") {
    const MapData* m = Kesmark();
    const std::shared_ptr<const CountryMap> c = Country();
    SC_EXPECT(m != nullptr && c != nullptr);
    if (m == nullptr || c == nullptr) return;
    const MapDef& d = m->Def();
    SC_EXPECT(m->HasWater() && !d.rivers.empty() && d.far_ground != nullptr);
    if (d.rivers.empty() || d.far_ground == nullptr) return;
    // On the Rava's centre line it is water; 300 m off it, across, it is not.
    const MapLineDef& rava = d.rivers.front();
    int on = 0, off = 0, tried = 0;
    for (std::size_t i = 1; i < rava.points.size(); ++i) {
        const MapPoint& p = rava.points[i - 1];
        const MapPoint& q = rava.points[i];
        const double len = std::hypot(q.east_m - p.east_m, q.north_m - p.north_m);
        if (len < 1.0) continue;
        const double me = 0.5 * (p.east_m + q.east_m), mn = 0.5 * (p.north_m + q.north_m);
        const double ne = -(q.north_m - p.north_m) / len, nn = (q.east_m - p.east_m) / len;  // across the river
        ++tried;
        on += m->WaterAt(me, mn) ? 1 : 0;
        off += m->WaterAt(me + 300.0 * ne, mn + 300.0 * nn) || m->WaterAt(me - 300.0 * ne, mn - 300.0 * nn) ? 1 : 0;
    }
    std::printf("    the Rava: %d of %d middles water, %d with water 300 m across\n", on, tried, off);
    SC_EXPECT(tried > 5 && on == tried && off == 0);
    // The country's lakes beyond the map proper, through the scenario's terrain too; and a path across one finds it.
    const MapTerrain ground(std::shared_ptr<const MapData>(m, [](const MapData*) {}), d.origin);
    const command::CountryGround cluttered(c, Heights());
    const command::CountryGround bare(c);
    SC_EXPECT(cluttered.HasWater() && !bare.HasWater());
    const ClutterRaster& r = cluttered.Clutter();
    int lakes = 0;
    for (const MapLakeDef& l : d.far_ground->lakes) {
        // A cell of the country's raster whose middle lies in the lake (BakeClutter's rule): water to both grounds.
        bool found = false;
        for (int y = 0; y < r.rows && !found; ++y) {
            for (int x = 0; x < r.cols && !found; ++x) {
                const double e = r.CentreEast(x), n = r.CentreNorth(y);
                if (d.InCore(e, n) || !mapgeo::InPolygon(l.points, e, n) || !mapgeo::InPolygon(l.points, e - 200.0, n) ||
                    !mapgeo::InPolygon(l.points, e + 200.0, n)) {
                    continue;
                }
                found = true;
                ++lakes;
                SC_EXPECT(m->WaterAt(e, n) && ground.WaterAt(e, n) && cluttered.WaterAt(e, n) && !bare.WaterAt(e, n));
                const em::PathGeometry g = em::AnalysePath(ground, Vec3{e - 5000.0, n, 0.0}, ground.HeightAt(e - 5000.0, n) + 5.0,
                                                           Vec3{e + 5000.0, n, 0.0}, ground.HeightAt(e + 5000.0, n) + 50.0, 100.0);
                SC_EXPECT(((g.water_mask >> 31) & 3u) == 3u);  // the two stretches either side of the path's middle, in the lake
                const em::PathGeometry gc = em::AnalysePath(cluttered, Vec3{e - 5000.0, n, 0.0}, cluttered.HeightAt(e - 5000.0, n) + 5.0,
                                                            Vec3{e + 5000.0, n, 0.0}, cluttered.HeightAt(e + 5000.0, n) + 50.0, 100.0);
                SC_EXPECT(gc.water_mask != 0);
            }
        }
    }
    std::printf("    %d of %zu lakes of the country found by both grounds\n", lakes, d.far_ground->lakes.size());
    SC_EXPECT(lakes == static_cast<int>(d.far_ground->lakes.size()) && lakes > 0);
}

SC_TEST("map dressing: a line in the land cover is unbroken and at least a cell wide, whichever way it runs") {
    using namespace simcore::scenery;
    CoverGrid g;
    g.cell_m = 25.0;
    g.cols = g.rows = 40;
    g.cover.assign(1600, static_cast<std::uint8_t>(Cover::Field));
    g.crop.assign(1600, 0);
    const auto connected = [&g](double ae, double an, double be, double bn) {
        // From the cell under a, through road cells sharing sides, to the cell under b.
        std::vector<int> seen(1600, 0), stack;
        const auto idx = [&g](double e, double n) { return static_cast<int>(g.IndexAt(e, n)); };
        stack.push_back(idx(ae, an));
        seen[static_cast<std::size_t>(stack.back())] = 1;
        while (!stack.empty()) {
            const int i = stack.back();
            stack.pop_back();
            if (i == idx(be, bn)) return true;
            const int c = i % 40, r = i / 40;
            const int nb[4][2] = {{c + 1, r}, {c - 1, r}, {c, r + 1}, {c, r - 1}};
            for (const auto& q : nb) {
                if (q[0] < 0 || q[1] < 0 || q[0] >= 40 || q[1] >= 40) continue;
                const int j = q[1] * 40 + q[0];
                if (seen[static_cast<std::size_t>(j)] || g.cover[static_cast<std::size_t>(j)] != static_cast<std::uint8_t>(Cover::Road)) continue;
                seen[static_cast<std::size_t>(j)] = 1;
                stack.push_back(j);
            }
        }
        return false;
    };
    // A narrow road (4 m) at 45 degrees through the cells' centres: the band alone gave cells touching at their corners
    // only; through their corners exactly; at a shallow slant; straight along a row.
    const double lines[][4] = {{12.5, 12.5, 962.5, 962.5}, {0.0, 0.0, 975.0, 975.0}, {10.0, 300.0, 990.0, 610.0}, {5.0, 512.5, 995.0, 512.5},
                               {980.0, 30.0, 20.0, 940.0}};
    for (const auto& l : lines) {
        std::fill(g.cover.begin(), g.cover.end(), static_cast<std::uint8_t>(Cover::Field));
        dressing_detail::StampSegment(g, l[0], l[1], l[2], l[3], std::max(2.0, 0.5 * g.cell_m), Cover::Road);
        dressing_detail::StampCrossed(g, l[0], l[1], l[2], l[3], Cover::Road);
        SC_EXPECT(connected(l[0], l[1], l[2], l[3]));
        // Every cell the line's middle passes through is road (looked at every metre).
        const double len = std::hypot(l[2] - l[0], l[3] - l[1]);
        int dry = 0;
        for (double s = 0.0; s <= len; s += 1.0) {
            const double e = l[0] + (l[2] - l[0]) * s / len, n = l[1] + (l[3] - l[1]) * s / len;
            dry += g.cover[g.IndexAt(e, n)] == static_cast<std::uint8_t>(Cover::Road) ? 0 : 1;
        }
        SC_EXPECT(dry == 0);
    }
    // On the Kesmark map: every cell a road's, railway's or river's middle passes through is road or water.
    const MapData* m = Kesmark();
    if (m == nullptr) return;
    DressingRules rules;
    std::string e;
    SC_EXPECT(LoadDressingRules(kData + "/scenery/map_dressing.json", rules, &e));
    const CoverRaster raster = BakeCover(*m, rules);
    int looked = 0, off = 0;
    for (const MapStrip& s : m->Layout().strips) {
        for (std::size_t i = 1; i < s.points.size(); ++i) {
            const Vec3& a = s.points[i - 1];
            const Vec3& b = s.points[i];
            const double len = std::hypot(b.x - a.x, b.y - a.y);
            for (double t = 0.0; t <= len; t += 5.0) {
                const double pe = a.x + (b.x - a.x) * t / std::max(len, 1e-9), pn = a.y + (b.y - a.y) * t / std::max(len, 1e-9);
                if (!raster.Has(pe, pn)) continue;
                ++looked;
                const Cover c = raster.At(pe, pn);
                off += c == Cover::Road || c == Cover::Water ? 0 : 1;
            }
        }
    }
    std::printf("    %d points along the map's lines, %d not on a road or water cell\n", looked, off);
    SC_EXPECT(looked > 50000 && off == 0);
    // The ground material's detail by cover: fields tilled, woods and marsh grass and litter, the rest hard.
    SC_EXPECT(CoverDetail(Cover::Field) == 0 && CoverDetail(Cover::Forest) == 1 && CoverDetail(Cover::Marsh) == 1 && CoverDetail(Cover::Town) == 2 &&
              CoverDetail(Cover::Road) == 2 && CoverDetail(Cover::Military) == 2);
}
