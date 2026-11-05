// The standing crops (simcore/scenery/standing_crops.hpp, roadmap L5 and D4): the calendar's heights; the bake against
// the truth (the cover, the field and its crop's state on the day) at every point; a path's chords exact at a field's
// edge; a sight line through standing maize blocked at a man's eye and clear over it; a scenario on the map gets them.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/radar.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenery/standing_crops.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::scenery;

namespace {

const std::string kData = SIMCORE_DATA_DIR;
constexpr int kDay = 288;  // 15 October

struct Rig {
    std::shared_ptr<const MapData> map;
    DressingRules dressing;
    GroundLayerRules layers;
    std::shared_ptr<const CoverRaster> cover;
    std::shared_ptr<const StandingCrops> crops;
    double bake_ms = 0.0;
};

const Rig& TheRig() {
    static Rig rig;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        rig.map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
        if (!rig.map || !LoadDressingRules(kData + "/scenery/map_dressing.json", rig.dressing, &e) ||
            !LoadGroundLayers(kData + "/scenery/ground_layers.json", rig.dressing, rig.layers, &e)) {
            std::printf("    %s\n", e.c_str());
            return rig;
        }
        rig.cover = std::make_shared<const CoverRaster>(BakeCover(*rig.map, rig.dressing));
        const auto t0 = std::chrono::steady_clock::now();
        rig.crops = StandingCrops::Bake(rig.map, rig.cover, rig.dressing, rig.layers, kDay);
        rig.bake_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
    return rig;
}

// The truth at a point: what the cover and the field's calendar say stands there.
double Truth(const Rig& r, double e, double n) {
    const CoverGrid* g = r.cover->GridAt(e, n);
    if (g == nullptr || static_cast<Cover>(g->cover[g->IndexAt(e, n)]) != Cover::Field) return 0.0;
    const FieldCell f = FieldAt(g->fields, e, n);
    return r.layers.StandOnDay(CropOf(r.dressing, f.id), f.id, kDay);
}

struct Collect final : ObstacleChordSink {
    struct C {
        double a, b, top;
    };
    std::vector<C> chords;
    bool Chord(double t_in, double t_out, double top_m) override {
        chords.push_back({t_in, t_out, top_m});
        return true;
    }
};

// A spot in a standing field whose 400 m along its furrows through it all stands: (e, n) its middle, heading the field's.
bool FindStandingRun(const Rig& r, double& e_out, double& n_out, double& heading_out) {
    const CoverGrid& g = r.cover->grids.back();  // the map proper's
    for (int row = 40; row < g.rows - 40; row += 7) {
        for (int col = 40; col < g.cols - 40; col += 7) {
            const double e = g.CentreEast(col), n = g.CentreNorth(row);
            if (Truth(r, e, n) < 2.0) continue;
            const FieldCell f = FieldAt(g.fields, e, n);
            const double sh = std::sin(f.heading_rad), ch = std::cos(f.heading_rad);
            bool all = true;
            for (double s = -200.0; s <= 200.0 && all; s += 2.0) all = Truth(r, f.centre_e + s * sh, f.centre_n + s * ch) >= 2.0;
            if (!all) continue;
            e_out = f.centre_e;
            n_out = f.centre_n;
            heading_out = f.heading_rad;
            return true;
        }
    }
    return false;
}

}  // namespace

SC_TEST("standing crops: the calendar's heights grow from a field's own day, and the loader refuses what is wrong") {
    const Rig& r = TheRig();
    SC_EXPECT(r.crops != nullptr);
    if (r.crops == nullptr) return;
    int maize = -1;
    for (std::size_t i = 0; i < r.dressing.crops.size(); ++i) {
        if (r.dressing.crops[i].id == "maize") maize = static_cast<int>(i);
    }
    SC_EXPECT(maize >= 0);
    SC_EXPECT(std::fabs(r.layers.TallestStand() - 2.4) < 1e-9);
    // A maize field in March: ploughland, nothing stands; in late July grown; in August dry and standing.
    std::size_t grown = 0, fields = 0;
    for (std::uint64_t id = 1; id < 400; ++id) {
        const std::uint64_t f = id * 0x9E3779B97F4A7C15ull;
        ++fields;
        SC_EXPECT(r.layers.StandOnDay(maize, f, 60) == 0.0);
        SC_EXPECT(r.layers.StandOnDay(maize, f, 230) == 2.4);  // every maize field is dry and standing by 230 (its own day 225..230 or still green, grown)
        const double may = r.layers.StandOnDay(maize, f, 150);
        SC_EXPECT(may >= 0.0 && may < 2.4);
        grown += r.layers.StandOnDay(maize, f, 300) == 0.0 ? 1 : 0;  // all harvested by day 295
    }
    SC_EXPECT(grown == fields);
    // The loader.
    GroundLayerRules l;
    std::string e;
    const auto with = [&](const std::string& extra) {
        std::ifstream f(kData + "/scenery/ground_layers.json", std::ios::binary);
        std::ostringstream s;
        s << f.rdbuf();
        std::string text = s.str();
        const std::string at = "{ \"layer\": \"maize\", \"from_day\": 110, \"to_day\": 140, \"stand_m\": 2.4, \"grow_days\": 75 }";
        const std::size_t p = text.find(at);
        if (p == std::string::npos) return std::string();
        return text.replace(p, at.size(), "{ \"layer\": \"maize\", \"from_day\": 110, \"to_day\": 140" + extra + " }");
    };
    SC_EXPECT(!with("").empty());
    SC_EXPECT(LoadGroundLayers(with(", \"stand_m\": 2.0"), r.dressing, l, &e));
    SC_EXPECT(!LoadGroundLayers(with(", \"stand_m\": 9"), r.dressing, l, &e));
    SC_EXPECT(e.find("stand_m") != std::string::npos);
    SC_EXPECT(!LoadGroundLayers(with(", \"grow_days\": 30"), r.dressing, l, &e));  // grows to nothing
    SC_EXPECT(!LoadGroundLayers(with(", \"stand_m\": 2.0, \"grow_days\": 2.5"), r.dressing, l, &e));
}

SC_TEST("standing crops: the bake says what stands at every point, as the cover and the calendar do") {
    const Rig& r = TheRig();
    if (r.crops == nullptr) return;
    std::printf("    baked in %.0f ms: tallest %.1f m\n", r.bake_ms, r.crops->TallestStand());
    SC_EXPECT(r.crops->Any() && std::fabs(r.crops->TallestStand() - 2.4) < 1e-9);
    // A lattice of points over the map, a little off the cells' lines.
    std::size_t points = 0, standing = 0, wrong = 0, sliver = 0;
    const CoverGrid& g = r.cover->grids.back();
    for (double n = g.south_m + 3.3; n < g.south_m + g.cell_m * g.rows; n += 61.7) {
        for (double e = g.west_m + 7.1; e < g.west_m + g.cell_m * g.cols; e += 61.7) {
            ++points;
            const double truth = Truth(r, e, n), baked = r.crops->StandAt(e, n);
            standing += truth > 0.0 ? 1 : 0;
            if (baked == truth) continue;
            // A sliver of a standing field in a cell no sample of it found (CellByte): nothing baked there.
            if (baked == 0.0) ++sliver;
            else ++wrong;
        }
    }
    std::printf("    %zu points, %zu standing, %zu wrong, %zu in an unbaked sliver\n", points, standing, wrong, sliver);
    SC_EXPECT(standing > points / 200 && wrong == 0 && sliver * 2000 <= points);
    // The top: the ground plus the stand.
    double e = 0.0, n = 0.0, h = 0.0;
    SC_EXPECT(FindStandingRun(r, e, n, h));
    SC_EXPECT(std::fabs(r.crops->TopAt(e, n) - (r.map->GroundHeight(e, n) + 2.4)) < 1e-9);
    SC_EXPECT(r.crops->StandWithin(e, n, 30.0) >= 2.4);
    SC_EXPECT(r.crops->TopBound(e - 10.0, n - 10.0, e + 10.0, n + 10.0) >= r.crops->TopAt(e, n));
}

SC_TEST("standing crops: a path's chords cover exactly where a crop stands, to a field's edge") {
    const Rig& r = TheRig();
    if (r.crops == nullptr) return;
    double ce = 0.0, cn = 0.0, h = 0.0;
    SC_EXPECT(FindStandingRun(r, ce, cn, h));
    std::size_t looked = 0, missed = 0, extra = 0, bad_top = 0, chords = 0;
    for (int k = 0; k < 120; ++k) {
        // Paths from round the standing field out through its edges, at all angles, 300 m to 3 km.
        const double a = 0.618034 * k * 6.283185, len = 300.0 + 2700.0 * std::fmod(0.371 * k, 1.0);
        const double e0 = ce + 150.0 * std::cos(1.7 * k), n0 = cn + 150.0 * std::sin(1.3 * k);
        const double e1 = e0 + len * std::cos(a), n1 = n0 + len * std::sin(a);
        Collect c;
        r.crops->Chords(e0, n0, e1, n1, -1e9, c);
        chords += c.chords.size();
        for (const Collect::C& ch : c.chords) {
            const double tm = 0.5 * (ch.a + ch.b);
            const double stand = Truth(r, e0 + (e1 - e0) * tm, n0 + (n1 - n0) * tm);
            const double ground = r.map->GroundHeight(e0 + (e1 - e0) * tm, n0 + (n1 - n0) * tm);
            bad_top += ch.top - ground >= stand - 1e-6 && ch.top - ground <= stand + 0.5 ? 0 : 1;
        }
        const int steps = static_cast<int>(len / 0.5);
        for (int i = 0; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            const double truth = Truth(r, e0 + (e1 - e0) * t, n0 + (n1 - n0) * t);
            bool in = false;
            for (const Collect::C& ch : c.chords) in = in || (t >= ch.a - 1e-9 && t <= ch.b + 1e-9);
            // Within a millimetre of an edge either answer is right.
            bool near_edge = false;
            for (const Collect::C& ch : c.chords) near_edge = near_edge || std::fabs(t - ch.a) * len < 1e-3 || std::fabs(t - ch.b) * len < 1e-3;
            ++looked;
            if (near_edge) continue;
            if (truth > 0.0 && !in) {
                // A sliver of a standing field in a cell its samples missed (CellByte) is left out.
                if (r.crops->StandAt(e0 + (e1 - e0) * t, n0 + (n1 - n0) * t) > 0.0) ++missed;
            }
            if (truth == 0.0 && in) ++extra;
        }
    }
    std::printf("    120 paths, %zu chords, %zu points looked at: %zu missed, %zu extra, %zu with the wrong top\n", chords, looked, missed,
                extra, bad_top);
    SC_EXPECT(chords > 100 && missed == 0 && extra == 0 && bad_top == 0);
}

SC_TEST("standing crops: a man in standing maize does not see 400 m through it; over it, he does") {
    const Rig& r = TheRig();
    if (r.crops == nullptr) return;
    double e = 0.0, n = 0.0, h = 0.0;
    SC_EXPECT(FindStandingRun(r, e, n, h));
    MapTerrain bare(r.map, r.map->Def().origin), crops(r.map, r.map->Def().origin);
    crops.SetStanding(r.crops);
    // The map's own frame is the scenario's here.
    const double sh = std::sin(h), ch = std::cos(h);
    const Vec3 a{e - 200.0 * sh, n - 200.0 * ch, 0.0}, b{e + 200.0 * sh, n + 200.0 * ch, 0.0};
    const double ga = bare.HeightAt(a.x, a.y), gb = bare.HeightAt(b.x, b.y);
    const bool open_bare = HasLineOfSight(bare, a, ga + 1.7, b, gb + 1.7);
    const bool open_crops = HasLineOfSight(crops, a, ga + 1.7, b, gb + 1.7);
    const bool over = HasLineOfSight(crops, a, ga + 5.0, b, gb + 5.0);
    std::printf("    across the maize at (%.0f, %.0f): bare %s, with the crops %s, from 5 m %s\n", e, n, open_bare ? "clear" : "blocked",
                open_crops ? "clear" : "blocked", over ? "clear" : "blocked");
    SC_EXPECT(open_bare && !open_crops && over);
    SC_EXPECT(crops.ObstacleHeightAt(e, n) >= 2.4 && bare.ObstacleHeightAt(e, n) < 2.4);
    SC_EXPECT(crops.ObstacleTallestM() >= 2.4);
}

SC_TEST("standing crops: the block Unreal draws covers what stands, to the square metre, with walls facing lower ground") {
    const Rig& r = TheRig();
    if (r.crops == nullptr) return;
    double e = 0.0, n = 0.0, h = 0.0;
    SC_EXPECT(FindStandingRun(r, e, n, h));
    // A 1 km square on the map proper's cells round the run, clear of the detail areas.
    const CoverGrid& g = r.cover->grids.back();
    const double w = g.west_m + g.cell_m * std::floor((e - 500.0 - g.west_m) / g.cell_m), s = g.south_m + g.cell_m * std::floor((n - 500.0 - g.south_m) / g.cell_m);
    for (std::size_t k = 0; k + 1 < r.cover->grids.size(); ++k) {
        const CoverGrid& o = r.cover->grids[k];
        SC_EXPECT(o.west_m >= w + 1000.0 || o.west_m + o.cell_m * o.cols <= w || o.south_m >= s + 1000.0 || o.south_m + o.cell_m * o.rows <= s);
    }
    const auto t0 = std::chrono::steady_clock::now();
    const TriMesh m = r.crops->BlockMesh(w, s, w + 1000.0, s + 1000.0, {0.2f, 0.15f, 0.08f}, {0.15f, 0.11f, 0.06f});
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    double top_area = 0.0;
    std::size_t walls = 0, wall_wrong = 0;
    for (std::size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const Vec3& a = m.positions[m.indices[i]];
        const Vec3& b = m.positions[m.indices[i + 1]];
        const Vec3& c = m.positions[m.indices[i + 2]];
        const Vec3& nrm = m.normals[m.indices[i]];
        const double cross_z = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (nrm.z > 0.5) {
            top_area += 0.5 * cross_z;  // counter-clockwise: positive
            continue;
        }
        if (m.indices[i + 1] != m.indices[i] + 1) continue;  // a wall's second triangle (k, k + 2, k + 3)
        ++walls;
        // Beyond the wall the crop stands lower.
        const double me = (a.x + b.x) / 2.0 + 0.2 * nrm.x, mn = (a.y + b.y) / 2.0 + 0.2 * nrm.y;
        const double here = r.crops->StandAt((a.x + b.x) / 2.0 - 0.2 * nrm.x, (a.y + b.y) / 2.0 - 0.2 * nrm.y);
        wall_wrong += r.crops->StandAt(me, mn) < here ? 0 : 1;
    }
    std::size_t standing = 0;
    for (double y = s + 0.5; y < s + 1000.0; y += 1.0) {
        for (double x = w + 0.5; x < w + 1000.0; x += 1.0) standing += Truth(r, x, y) > 0.0 ? 1 : 0;
    }
    std::printf("    a km square: %zu triangles in %.0f ms, tops %.0f m2 against %zu m2 standing, %zu walls, %zu facing as tall\n", m.Triangles(), ms,
                top_area, standing, walls, wall_wrong);
    SC_EXPECT(standing > 50000);
    SC_EXPECT(std::fabs(top_area - static_cast<double>(standing)) < 0.003 * static_cast<double>(standing));
    SC_EXPECT(walls > 0 && wall_wrong * 100 <= walls);
}

SC_TEST("standing crops: a scenario on the map has them for its date, and a session's ground carries them") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/tutorial_basics.json", kData, def, console, &e));
    if (!def.map) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    SC_EXPECT(def.crops != nullptr && def.crops->Day() == def.clock.day_of_year);
    ScenarioDef again;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/tutorial_basics.json", kData, again, console, &e));
    SC_EXPECT(again.crops == def.crops);  // baked once
}
