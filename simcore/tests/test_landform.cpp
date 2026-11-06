// The hill country (roadmap L11, docs/scenery/TERRAIN_ROADMAP.md A.5; the look target's K1, docs/scenery/LOOK_TARGET.md):
// terrain.landform in data/maps/kesmark_range.json (and the same in maps/pannonia.json). Where the owner chose it (west
// and north-west), nowhere else; its relief and slopes measured against what A.5 asks.
#include <algorithm>
#include <cmath>
#include <cstdio>
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
    // Never below the plain it rises from, never above its relief.
    for (double e = -20.0; e <= 2.0; e += 0.5) {
        for (double n = -20.0; n <= 20.0; n += 0.5) {
            const double a = Added(e, n);
            SC_EXPECT(a >= 0.0 && a <= 70.0 * with.landform.exaggeration + 1e-9);
        }
    }
}

SC_TEST("landform: the hills' forms: 40-80 m from floor to crest over a few kilometres, slopes mostly gentle, a few steep") {
    MapDef d;
    if (!LoadDistrict(d)) return;
    MapDef plain = d;
    plain.landform.regions.clear();
    // The fully weighted hills, west of the blend: windows 2 km square on a 50 m lattice, the landform's own part.
    std::vector<double> window_relief, slopes;
    std::vector<double> shape;
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
                    shape.push_back(LandformShape(d.landform, d.seed, e, n));
                }
            }
            window_relief.push_back(hi - lo);
        }
    }
    const double relief_med = Percentile(window_relief, 0.5), slope_med = Percentile(slopes, 0.5), slope_95 = Percentile(slopes, 0.95);
    double steep = 0.0, floors = 0.0, crests = 0.0, mean = 0.0;
    for (double s : slopes) steep += s > 20.0 ? 1.0 : 0.0;
    for (double s : shape) {
        floors += s < 0.02 ? 1.0 : 0.0;
        crests += s > 0.98 ? 1.0 : 0.0;
        mean += s;
    }
    steep /= static_cast<double>(slopes.size());
    floors /= static_cast<double>(shape.size());
    crests /= static_cast<double>(shape.size());
    mean /= static_cast<double>(shape.size());
    std::printf("    hills: relief in 2 km windows %.0f m (p10 %.0f, p90 %.0f); slopes median %.1f deg, p95 %.1f deg, over 20 deg %.1f%%; "
                "shape mean %.2f, floors %.0f%%, crests %.0f%%\n",
                relief_med, Percentile(window_relief, 0.1), Percentile(window_relief, 0.9), slope_med, slope_95, steep * 100.0, mean,
                floors * 100.0, crests * 100.0);
    SC_EXPECT(relief_med >= 40.0 && relief_med <= 80.0);
    SC_EXPECT(slope_med >= 2.5 && slope_med <= 10.0);
    SC_EXPECT(slope_95 <= 20.0);
    SC_EXPECT(steep < 0.03);
    // The stretch (kLandformMid, kLandformSpread) uses the whole relief without piling the ground on its floor or crest.
    SC_EXPECT(mean > 0.4 && mean < 0.6);
    SC_EXPECT(floors > 0.01 && floors < 0.2);
    SC_EXPECT(crests > 0.01 && crests < 0.2);
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
}
