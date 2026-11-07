// The hill country (roadmap L11, docs/scenery/TERRAIN_ROADMAP.md A.5; the look target's K1, docs/scenery/LOOK_TARGET.md):
// terrain.landform in data/maps/kesmark_range.json (and the same in maps/pannonia.json). Where the owner chose it (west
// and north-west), nowhere else; its relief and slopes measured against what A.5 asks; and how it drains (A.5 step 2).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <queue>
#include <string>
#include <vector>

#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

bool LoadDistrict(MapDef& out) {
    std::string text, e;
    if (!detail::ReadTextFile(kData + "/maps/kesmark_range.json", text) || !LoadMapDefJson(text, out, &e)) {
        std::printf("    %s\n", e.c_str());
        return false;
    }
    return true;
}

double Percentile(std::vector<double> v, double q) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[static_cast<std::size_t>(q * static_cast<double>(v.size() - 1))];
}

bool Refused(const std::string& text, const std::string& from, const std::string& to, const std::string& expect) {
    std::string t = text;
    if (t.find(from) == std::string::npos) return false;
    t.replace(t.find(from), from.size(), to);
    MapDef d;
    std::string e;
    if (LoadMapDefJson(t, d, &e)) return false;
    if (e.find(expect) == std::string::npos) std::printf("    unexpected refusal: %s\n", e.c_str());
    return e.find(expect) != std::string::npos;
}

}  // namespace

SC_TEST("landform: the hill country lies west and north-west; the plain is kept at battery 21, the bridges, Kesmark and Kecel") {
    MapDef with;
    if (!LoadDistrict(with)) return;
    SC_EXPECT(with.landform.regions.size() == 2);
    MapDef without = with;
    without.landform.regions.clear();
    without.landform.bake.reset();
    const auto Added = [&](double e_km, double n_km) {
        return NaturalGround(with, e_km * 1000.0, n_km * 1000.0) - NaturalGround(without, e_km * 1000.0, n_km * 1000.0);
    };
    // Untouched to the last bit: battery 21 and its dummies, the bridges, Kesmark, Vrbice, the P-40 on Polany hill,
    // the Kecel airfield, Varos hill's bunker, the capital.
    const double kept[][2] = {{6.4, -9.6}, {4.5, -9.4}, {10.7, -9.5}, {12, -6}, {14, -4}, {9, -1}, {2.5, -12.5},
                              {2, 14}, {-25, -20}, {-34, 36}, {36, 58}};
    for (const auto& k : kept) {
        if (Added(k[0], k[1]) != 0.0) std::printf("    the landform reaches (%.1f, %.1f) km\n", k[0], k[1]);
        SC_EXPECT(Added(k[0], k[1]) == 0.0);
    }
    // And the battery 21 detail area, whole.
    for (const MapDetailDef& a : with.detail) {
        if (a.id != "bty21") continue;
        double worst = 0.0;
        for (double n = a.centre.north_m - a.half_m; n <= a.centre.north_m + a.half_m; n += 250.0) {
            for (double e = a.centre.east_m - a.half_m; e <= a.centre.east_m + a.half_m; e += 250.0) {
                worst = std::max(worst, std::fabs(NaturalGround(with, e, n) - NaturalGround(without, e, n)));
            }
        }
        SC_EXPECT(worst == 0.0);
    }
    // Hills round the regiment's post in the Lazne woods, the division's posts and Ludas hill.
    double raised = 0.0;
    for (double e = -16.0; e <= -10.0; e += 0.25) {
        for (double n = -4.0; n <= 12.0; n += 0.25) raised = std::max(raised, Added(e, n));
    }
    std::printf("    the highest the landform adds round the Lazne woods and the division's posts: %.1f m\n", raised);
    SC_EXPECT(raised > 45.0);
    // Never below the plain it rises from; above it by the relief and the valley floors' climb into the hills.
    double top = 0.0;
    for (double e = -20.0; e <= 2.0; e += 0.5) {
        for (double n = -20.0; n <= 20.0; n += 0.5) {
            const double a = Added(e, n);
            top = std::max(top, a);
            SC_EXPECT(a >= 0.0 && a <= 130.0 * with.landform.exaggeration);
        }
    }
    std::printf("    the highest the landform adds in the range: %.1f m\n", top);
}

SC_TEST("landform: the hills' forms: 40-80 m from floor to crest over a few kilometres, slopes mostly gentle, a few steep") {
    MapDef d;
    if (!LoadDistrict(d)) return;
    MapDef plain = d;
    plain.landform.regions.clear();
    plain.landform.bake.reset();
    // The fully weighted hills, west of the blend: windows 2 km square on a 50 m lattice, the landform's own part.
    std::vector<double> window_relief, slopes;
    const double step = 50.0;
    for (double wn = -2000.0; wn < 26000.0; wn += 2000.0) {
        for (double we = -58000.0; we < -16000.0; we += 2000.0) {
            double lo = 1e9, hi = -1e9;
            for (double n = wn; n < wn + 2000.0; n += step) {
                for (double e = we; e < we + 2000.0; e += step) {
                    const double h = NaturalGround(d, e, n) - NaturalGround(plain, e, n);
                    lo = std::min(lo, h);
                    hi = std::max(hi, h);
                    const double dx = (NaturalGround(d, e + step, n) - NaturalGround(d, e - step, n)) / (2.0 * step);
                    const double dy = (NaturalGround(d, e, n + step) - NaturalGround(d, e, n - step)) / (2.0 * step);
                    slopes.push_back(std::atan(std::hypot(dx, dy)) * 180.0 / kPi);
                }
            }
            window_relief.push_back(hi - lo);
        }
    }
    const double relief_med = Percentile(window_relief, 0.5), slope_med = Percentile(slopes, 0.5), slope_95 = Percentile(slopes, 0.95);
    double steep = 0.0;
    for (double s : slopes) steep += s > 20.0 ? 1.0 : 0.0;
    steep /= static_cast<double>(slopes.size());
    std::printf("    hills: relief in 2 km windows %.0f m (p10 %.0f, p90 %.0f); slopes median %.1f deg, p95 %.1f deg, over 20 deg %.1f%%\n",
                relief_med, Percentile(window_relief, 0.1), Percentile(window_relief, 0.9), slope_med, slope_95, steep * 100.0);
    SC_EXPECT(relief_med >= 40.0 && relief_med <= 80.0);
    SC_EXPECT(slope_med >= 2.5 && slope_med <= 10.0);
    SC_EXPECT(slope_95 <= 20.0);
    SC_EXPECT(steep < 0.03);
}

SC_TEST("landform: the hills drain: no closed basin, brooks that run downhill out of the hills or into another brook") {
    MapDef d;
    if (!LoadDistrict(d)) return;
    SC_EXPECT(d.landform.bake != nullptr);
    if (!d.landform.bake) return;
    const LandformBake& b = *d.landform.bake;
    SC_EXPECT(b.cols > 100 && b.rows > 100);
    // A flood from the bake's edge and the plain (where the hills add nothing) raises no cell: every cell has a way out
    // that never climbs.
    const int cols = b.cols, rows = b.rows;
    const std::size_t count = static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
    std::vector<char> seen(count, 0);
    // No cliff between two cells: where two valleys' sides meet, the ground steps no steeper than 30 degrees.
    double cliff = 0.0;
    int cliffs = 0;
    for (int r = 0; r + 1 < rows; ++r) {
        for (int c = 0; c + 1 < cols; ++c) {
            const std::size_t i = static_cast<std::size_t>(r * cols + c);
            for (const std::size_t j : {i + 1, i + static_cast<std::size_t>(cols)}) {
                const double s = std::fabs(static_cast<double>(b.height[j]) - b.height[i]) / b.spacing_m;
                cliff = std::max(cliff, s);
                cliffs += s > 0.577 ? 1 : 0;
            }
        }
    }
    std::printf("    the steepest step between two cells: %.0f deg; %d steps over 30 deg\n", std::atan(cliff) * 180.0 / kPi, cliffs);
    SC_EXPECT(cliff < 0.577);
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> edge;
    for (int i = 0; i < static_cast<int>(count); ++i) {
        const int c = i % cols, r = i / cols;
        if (b.height[static_cast<std::size_t>(i)] < 0.01f || c == 0 || r == 0 || c == cols - 1 || r == rows - 1) {
            seen[static_cast<std::size_t>(i)] = 1;
            edge.push({b.height[static_cast<std::size_t>(i)], i});
        }
    }
    double worst = 0.0;
    while (!edge.empty()) {
        const double level = edge.top().first;
        const int i = edge.top().second;
        edge.pop();
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int c = i % cols + dx, r = i / cols + dy;
                if (c < 0 || r < 0 || c >= cols || r >= rows) continue;
                const std::size_t j = static_cast<std::size_t>(r * cols + c);
                if (seen[j]) continue;
                seen[j] = 1;
                worst = std::max(worst, level - b.height[j]);
                edge.push({std::max(level, static_cast<double>(b.height[j])), static_cast<int>(j)});
            }
        }
    }
    std::printf("    the deepest closed hollow: %.3f m; %zu brooks\n", worst, b.brooks.size());
    SC_EXPECT(worst < 0.01);
    // Brooks: some, each draining more downstream, lying in its valley (its bed below the ground 150 m to either side).
    SC_EXPECT(b.brooks.size() > 10);
    int in_valley = 0, points = 0;
    for (const MapBrookDef& br : b.brooks) {
        SC_EXPECT(br.points.size() == br.area_m2.size());
        SC_EXPECT(br.area_m2.back() >= br.area_m2.front());
        SC_EXPECT(br.area_m2.front() >= d.landform.drainage.brook_area_m2 * 0.99);
        for (std::size_t k = 1; k + 1 < br.points.size(); k += 4) {
            const MapPoint& p = br.points[k];
            const MapPoint& q = br.points[k + 1];
            const double len = std::hypot(q.east_m - p.east_m, q.north_m - p.north_m);
            if (len <= 0.0) continue;
            const double ux = -(q.north_m - p.north_m) / len, uy = (q.east_m - p.east_m) / len;  // across the brook
            const double here = b.Height(p.east_m, p.north_m);
            const double left = b.Height(p.east_m + 150.0 * ux, p.north_m + 150.0 * uy);
            const double right = b.Height(p.east_m - 150.0 * ux, p.north_m - 150.0 * uy);
            ++points;
            in_valley += (here <= left + 0.5 && here <= right + 0.5) ? 1 : 0;
        }
    }
    std::printf("    brook points in their valley: %d of %d\n", in_valley, points);
    SC_EXPECT(points > 100 && in_valley >= 0.9 * points);
    // The same bake for the same landform: built once and shared (the country's file carries the same block).
    MapDef again;
    SC_EXPECT(LoadDistrict(again) && again.landform.bake.get() == d.landform.bake.get());
}

SC_TEST("landform: the loader refuses what it should") {
    std::string text;
    SC_EXPECT(detail::ReadTextFile(kData + "/maps/kesmark_range.json", text));
    const std::string ex = "\"exaggeration\": 1.0";
    SC_EXPECT(Refused(text, ex, "\"exaggeration\": 5", "exaggeration"));
    SC_EXPECT(Refused(text, ex, "\"exaggeration\": 1.0, \"height\": 3", "height"));
    SC_EXPECT(Refused(text, "\"relief_m\": 70", "\"relief_m\": -1", "relief_m"));
    SC_EXPECT(Refused(text, "\"blend_km\": 5", "\"blend_km\": 0", "blend_km"));
    SC_EXPECT(Refused(text, "[[-60, -8], [-9, -8], [-5, 0], [-7, 9], [-4, 18], [-6, 30], [-60, 30]]", "[[-60, -8], [-9, -8]]", "points"));
    SC_EXPECT(Refused(text, "\"landform\": {", "\"landforms\": {", "landforms"));  // terrain's keys are checked
    SC_EXPECT(Refused(text, "\"hillslope\": 0.26", "\"hillslope\": 0", "hillslope"));
    SC_EXPECT(Refused(text, "\"brook_area_km2\": 3", "\"brook_area_km2\": 0.1", "brook_area_km2"));
    SC_EXPECT(Refused(text, "\"spacing_m\": 50, \"channel", "\"spacing_m\": 5, \"channel", "spacing_m"));
}

SC_TEST("land use: the hills' steep sides and crests wooded, their wet floors and steep slopes meadow, the plain untouched") {
    std::string e;
    const std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (!map) return;
    const MapDef& d = map->Def();
    SC_EXPECT(d.land_use.laid);
    const LandformBake& b = *d.landform.bake;
    // Over the hills inside the map proper, every 100 m.
    int all = 0, wood = 0, meadow = 0, steep = 0, steep_wood = 0, gentle = 0, gentle_wood = 0, wet = 0, wet_meadow = 0;
    std::vector<double> wetness;
    for (double n = -2000.0; n < 20000.0; n += 100.0) {
        for (double x = -19950.0; x < -9000.0; x += 100.0) {
            if (b.Height(x, n) < d.land_use.min_relief_m) continue;
            ++all;
            const bool w = map->LandUseWood(x, n), m = !w && map->LandUseMeadow(x, n);
            wood += w ? 1 : 0;
            meadow += m ? 1 : 0;
            const double s = b.SlopeDeg(x, n), t = b.Wetness(x, n);
            wetness.push_back(t);
            if (s >= 15.0) {
                ++steep;
                steep_wood += w ? 1 : 0;
            }
            if (s < 5.0) {
                ++gentle;
                gentle_wood += w ? 1 : 0;
            }
            if (t >= 13.0 && s < 5.0) {
                ++wet;
                wet_meadow += m ? 1 : 0;
            }
        }
    }
    const auto share = [](int a, int of) { return of > 0 ? static_cast<double>(a) / of : 0.0; };
    std::printf("    hills in the map: %d points, wooded %.0f%%, meadow %.0f%%; over 15 deg wooded %.0f%% (%d), under 5 deg wooded %.0f%% (%d); "
                "wet floors meadow %.0f%% (%d); wetness p10 %.1f, p50 %.1f, p90 %.1f, p99 %.1f\n",
                all, 100.0 * share(wood, all), 100.0 * share(meadow, all), 100.0 * share(steep_wood, steep), steep,
                100.0 * share(gentle_wood, gentle), gentle, 100.0 * share(wet_meadow, wet), wet, Percentile(wetness, 0.1),
                Percentile(wetness, 0.5), Percentile(wetness, 0.9), Percentile(wetness, 0.99));
    SC_EXPECT(all > 5000);
    SC_EXPECT(share(wood, all) > 0.15 && share(wood, all) < 0.5);
    SC_EXPECT(share(meadow, all) > 0.05 && share(meadow, all) < 0.4);
    SC_EXPECT(share(steep_wood, steep) > 0.8);
    SC_EXPECT(share(gentle_wood, gentle) < 0.25);
    SC_EXPECT(wet > 50 && share(wet_meadow, wet) > 0.9);
    // The woods are laid: canopy over the hills in the map proper.
    int canopy = 0;
    for (const MapBox& bx : map->Layout().boxes) {
        canopy += (bx.part == MapPart::Canopy && bx.east_m < -9000.0 && bx.north_m > -2000.0 && bx.north_m < 20000.0) ? 1 : 0;
    }
    std::printf("    canopy blocks over the hills in the map: %d\n", canopy);
    SC_EXPECT(canopy > 1000);
    // The plain: nothing of it.
    for (const auto& p : {std::pair<double, double>{6400.0, -9600.0}, {12000.0, -6000.0}, {2500.0, -12500.0}, {9000.0, -1000.0}}) {
        SC_EXPECT(!map->LandUseWood(p.first, p.second) && !map->LandUseMeadow(p.first, p.second));
    }
}

SC_TEST("roads in the hills: their grades by class (A.5 step 5, the look target's K8)") {
    std::string e;
    const std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(map != nullptr);
    if (!map) return;
    const LandformBake& b = *map->Def().landform.bake;
    // Every pair of a strip's points over the hills, off the bridges' ramps, as a grade in per cent.
    // In the map proper, off its last 500 m (beyond, the country's ground, unfitted).
    const MapDef& d = map->Def();
    const auto inner = [&d](const Vec3& p) {
        return p.x > d.west_m + 500.0 && p.x < d.east_m - 500.0 && p.y > d.south_m + 500.0 && p.y < d.north_m - 500.0;
    };
    std::vector<double> grades[5];
    double worst[5] = {0, 0, 0, 0, 0};
    Vec3 worst_at[5];
    for (const MapStrip& s : map->Layout().strips) {
        if (s.surface == MapSurface::Water) continue;
        for (std::size_t i = 1; i < s.points.size(); ++i) {
            const Vec3& p = s.points[i - 1];
            const Vec3& q = s.points[i];
            if (!inner(p) || !inner(q)) continue;
            if (b.Height(p.x, p.y) < 3.0 || b.Height(q.x, q.y) < 3.0) continue;
            if (p.z - map->GroundHeight(p.x, p.y) > 1.0 || q.z - map->GroundHeight(q.x, q.y) > 1.0) continue;
            const double run = std::hypot(q.x - p.x, q.y - p.y);
            if (run < 1.0) continue;
            const int k = static_cast<int>(s.surface);
            const double g = 100.0 * std::fabs(q.z - p.z) / run;
            grades[k].push_back(g);
            if (g > worst[k]) {
                worst[k] = g;
                worst_at[k] = p;
            }
        }
    }
    // How deep the cuts and fills run under the lines.
    double deepest = 0.0;
    for (const MapStrip& s : map->Layout().strips) {
        if (s.surface == MapSurface::Water) continue;
        for (const Vec3& p : s.points) {
            if (inner(p) && p.z - map->GroundHeight(p.x, p.y) < 1.0) deepest = std::max(deepest, std::fabs(map->GroundHeight(p.x, p.y) - NaturalGround(d, p.x, p.y)));
        }
    }
    std::printf("    the deepest cut or fill under a line: %.1f m\n", deepest);
    for (int k = 1; k < 5; ++k) {
        const std::vector<double>& g = grades[k];
        std::printf("    %s in the hills: %zu spans, grade p50 %.1f%%, p95 %.1f%%, p99 %.1f%%, max %.1f%% at (%.0f, %.0f)\n",
                    MapSurfaceName(static_cast<MapSurface>(k)), g.size(), Percentile(g, 0.5), Percentile(g, 0.95), Percentile(g, 0.99), worst[k],
                    worst_at[k].x, worst_at[k].y);
    }
    // The profiles keep land_use.json's grades (main 6%, secondary 8%, tracks 14%, the railway 2.5%); the 50 m ground
    // they are cut into carries them only to a cell (a node 25-35 m off the line sits on the cutting's side), so the
    // strips measure somewhat steeper. On a detail area's finer ground they keep them (A.5 step 6). Before the fit
    // (6 Nov 2026), in the same spans: p95 16% main, 15% secondary, 15% tracks, 13% rail; max 44-59%.
    SC_EXPECT(Percentile(grades[1], 0.95) <= 8.5 && worst[1] <= 14.0);
    SC_EXPECT(Percentile(grades[2], 0.95) <= 10.5 && worst[2] <= 18.0);
    SC_EXPECT(Percentile(grades[3], 0.95) <= 12.5 && worst[3] <= 25.0);
    SC_EXPECT(Percentile(grades[4], 0.95) <= 6.5 && worst[4] <= 10.0);
    SC_EXPECT(deepest > 2.0 && deepest < 30.0);
}

SC_TEST("land use: the loader refuses what it should") {
    std::string text;
    SC_EXPECT(detail::ReadTextFile(kData + "/scenery/land_use.json", text));
    MapLandUseDef u;
    std::string e;
    SC_EXPECT(LoadLandUseJson(text, u, &e) && u.laid && u.meadow_crop == "pasture");
    std::string bad = text;
    const std::string share_was = "\"crest_share\": { \"value\": 0.5";
    bad.replace(bad.find(share_was), share_was.size(), "\"crest_share\": { \"value\": 1.5");
    SC_EXPECT(!LoadLandUseJson(bad, u, &e) && e.find("crest_share") != std::string::npos);
    bad = text;
    bad.replace(bad.find("\"crest_share\""), 13, "\"crest_sharp\"");
    SC_EXPECT(!LoadLandUseJson(bad, u, &e) && e.find("crest_sharp") != std::string::npos);
}
