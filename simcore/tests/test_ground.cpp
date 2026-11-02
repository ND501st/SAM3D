// Roadmap R5, nothing under the map: one ground for the sim and the picture (simcore/terrain_map.hpp's levels sampled
// on the drawn triangles, MapFrameLink::GroundPoint and geo.hpp's GroundPointInFrame), the check that lists anything
// under it (simcore/ground_check.hpp), and the aircraft and weapons held over it in simcore (target_behaviour.hpp's
// ground floor, the runner's weapon impacts). Placeholder flight numbers, not a real aircraft's.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war/test_waves.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/geo.hpp"
#include "simcore/ground_check.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/target_behaviour.hpp"
#include "simcore/terrain.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::shared_ptr<const MapData> KesmarkMap() {
    std::string e;
    std::shared_ptr<const MapData> m = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    if (!m) std::printf("    %s\n", e.c_str());
    return m;
}

// The testbed's origin (data/scenarios/dynamic_campaign_test.json): 68 km off the map's.
GeoPoint TestbedOrigin() { return GeoPoint::FromDegrees(47.420689, 20.177063, 108.9); }

bool LoadScenario(const std::string& file, ScenarioDef& def, ConsoleData& console) {
    std::string e;
    const bool ok = LoadScenarioFile(kData + "/" + file, kData, def, console, &e);
    if (!ok) std::printf("    %s: %s\n", file.c_str(), e.c_str());
    return ok;
}

// A weapon on a route, as the waves make one (air_war/test_waves.hpp).
ScenarioTarget Weapon(const std::string& id, const Vec3& start, const std::vector<Vec3>& waypoints) {
    ScenarioTarget t;
    t.id = id;
    t.name = id;
    t.type = "Weapon+Missile";
    t.weapon = true;
    t.start = start;
    t.waypoints = waypoints;
    t.speed_mps = 250.0;
    t.max_turn_rate_rad_s = DegToRad(8.0);
    return t;
}

}  // namespace

// ---------------------------------------------------------------------------------------------- one ground

SC_TEST("ground: a cell is sampled on the two triangles it is drawn as, split along its (c+1, r)-(c, r+1) diagonal") {
    MapGrid g;
    g.west_m = 0.0;
    g.south_m = 0.0;
    g.spacing_m = 100.0;
    g.cols = 2;
    g.rows = 2;
    g.values = {10.0, 20.0, 40.0, 0.0};  // h00 (0,0), h10 (100,0), h01 (0,100), h11 (100,100)
    // The corners.
    SC_EXPECT_NEAR(g.SampleTri(0.0, 0.0), 10.0, 1e-12);
    SC_EXPECT_NEAR(g.SampleTri(100.0, 0.0), 20.0, 1e-12);
    SC_EXPECT_NEAR(g.SampleTri(0.0, 100.0), 40.0, 1e-12);
    SC_EXPECT_NEAR(g.SampleTri(100.0, 100.0), 0.0, 1e-12);
    // The diagonal B-C: the mean of 20 and 40 halfway, whichever triangle; the bilinear sample there is the four's mean.
    SC_EXPECT_NEAR(g.SampleTri(50.0, 50.0), 30.0, 1e-12);
    SC_EXPECT_NEAR(g.SampleTri(25.0, 75.0), 0.25 * 20.0 + 0.75 * 40.0, 1e-12);
    SC_EXPECT_NEAR(g.Sample(50.0, 50.0), 17.5, 1e-12);
    // The south-west triangle {A, B, C}: 10 + 10 tx + 30 ty at (20, 30): 10 + 2 + 9.
    SC_EXPECT_NEAR(g.SampleTri(20.0, 30.0), 21.0, 1e-12);
    // The north-east triangle {B, D, C}: 0 + 40 (1 - tx) + 20 (1 - ty) at (80, 70): 8 + 6.
    SC_EXPECT_NEAR(g.SampleTri(80.0, 70.0), 14.0, 1e-12);
}

SC_TEST("ground: beyond the far ground a district's ground is the country's lattices as drawn, not the analytic ground") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    SC_EXPECT(m != nullptr);
    if (!m) return;
    const std::vector<const MapGrid*> levels = m->DrawnLevels();
    SC_EXPECT(levels.size() == 4);  // the map proper, the far ground, the country's 1 km, the 4 km beyond
    SC_EXPECT(m->CountryGrid().spacing_m == 1000.0 && m->BeyondGrid().spacing_m == 4000.0);
    SC_EXPECT(m->LevelAt(0.0, 0.0) == 0 && m->LevelAt(60000.0, 0.0) == 1 && m->LevelAt(-170000.0, -20000.0) == 2 &&
              m->LevelAt(-250000.0, 0.0) == 3 && m->LevelAt(1.0e7, 0.0) == -1);
    // At a node of the country's grid outside the far ground (the country spans -180 to 150 km east, -180 to 140 km north): the country's natural ground (country_map.hpp's BakeGround).
    const MapDef& cg = *m->Def().far_ground;
    SC_EXPECT_NEAR(m->GroundHeight(-170000.0, -20000.0), NaturalGround(cg, -170000.0, -20000.0), 1e-9);
    // Between its nodes: its triangle, which is not the analytic ground there.
    const double e = -170400.0, n = -20300.0;
    SC_EXPECT_NEAR(m->GroundHeight(e, n), m->CountryGrid().SampleTri(e, n), 1e-9);
    SC_EXPECT_NEAR(m->GroundHeight(-250000.0, 1000.0), m->BeyondGrid().SampleTri(-250000.0, 1000.0), 1e-9);
    // The cells drawn: a far cell inside the map proper is not; a country cell under the far ground is not, one beyond is.
    int c = 0, r = 0;
    m->FarGrid().CellOf(100.0, 100.0, c, r);
    SC_EXPECT(!m->DrawsCell(1, c, r));
    m->FarGrid().CellOf(60000.0, 100.0, c, r);
    SC_EXPECT(m->DrawsCell(1, c, r));
    m->CountryGrid().CellOf(100.0, 100.0, c, r);
    SC_EXPECT(!m->DrawsCell(2, c, r));
    m->CountryGrid().CellOf(-170400.0, -20300.0, c, r);
    SC_EXPECT(m->DrawsCell(2, c, r));
}

SC_TEST("ground: a map point's ground point in a frame 68 km off is straight under the frame's up, and maps back exactly") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    const MapFrameLink link(m->Def().origin, TestbedOrigin());
    // The testbed's origin in the map, and points 0, 68 and 150 km from the frame's origin.
    const MapPoint o = link.FrameToMap(0.0, 0.0);
    std::printf("    the testbed's origin is (%.0f, %.0f) m in the map\n", o.east_m, o.north_m);
    for (double d : {0.0, 68000.0, 150000.0}) {
        for (double bearing : {0.0, 2.1, 4.0}) {
            const double x = d * std::sin(bearing), y = d * std::cos(bearing);
            const MapPoint p = link.FrameToMap(x, y);
            const double h = m->GroundHeight(p.east_m, p.north_m);
            const Vec3 g = link.GroundPoint(p.east_m, p.north_m, h);
            SC_EXPECT(std::fabs(g.x - x) < 1e-3 && std::fabs(g.y - y) < 1e-3);
            const MapPoint back = link.FrameToMap(g.x, g.y);
            SC_EXPECT(std::fabs(back.east_m - p.east_m) < 1e-3 && std::fabs(back.north_m - p.north_m) < 1e-3);
            SC_EXPECT_NEAR(link.Frame().AltitudeOf(g), h, 1e-3);
            // The frame's ground point is the sim's: the same function the runner's GroundEnu uses.
            const Vec3 sim = GroundPointInFrame(link.Frame(), x, y, h);
            SC_EXPECT((sim - g).Length() < 1e-3);
        }
    }
}

SC_TEST("ground: the runner's ground point keeps east and north and stands at the terrain's height") {
    ScenarioDef def;
    ConsoleData console;
    if (!LoadScenario("scenarios/kesmark_range_test.json", def, console)) return;
    ScenarioRunner r(std::move(def), std::move(console));
    for (const Vec3& p : {Vec3{0.0, 0.0, 0.0}, Vec3{12345.0, -6789.0, 0.0}, Vec3{-50000.0, 30000.0, 0.0}, Vec3{140000.0, 10000.0, 0.0}}) {
        const Vec3 g = r.GroundEnu(p.x, p.y);
        SC_EXPECT(g.x == p.x && g.y == p.y);
        SC_EXPECT_NEAR(r.GetWorld().Frame().AltitudeOf(g), r.GroundHeightAt(p.x, p.y), 1e-3);
    }
}

SC_TEST("ground: what the sim samples is what is drawn, on every level of the Kesmark map, seen from the testbed's frame") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    std::string why;
    const MapFrameLink testbed(m->Def().origin, TestbedOrigin());
    SC_EXPECT(DrawnGroundAgrees(*m, testbed, 400, &why));
    if (!why.empty()) std::printf("    testbed: %s\n", why.c_str());
    why.clear();
    const MapFrameLink own(m->Def().origin, m->Def().origin);
    SC_EXPECT(DrawnGroundAgrees(*m, own, 200, &why));
    if (!why.empty()) std::printf("    the map's frame: %s\n", why.c_str());
}

// ---------------------------------------------------------------------------------------------- the check

SC_TEST("ground check: an aircraft under a ridge is listed, one over it is not, and the ground items stand on the ground") {
    ScenarioDef def;
    ConsoleData console;
    if (!LoadScenario("scenarios/osa_practice.json", def, console)) return;
    TerrainRidge ridge;
    ridge.a_east_m = 30000.0;
    ridge.a_north_m = -5000.0;
    ridge.b_east_m = 30000.0;
    ridge.b_north_m = 5000.0;
    ridge.height_m = 300.0;
    ridge.width_m = 1000.0;
    def.ridges.push_back(ridge);
    ScenarioRunner r(std::move(def), std::move(console));
    const LocalFrame& frame = r.GetWorld().Frame();
    const double top = r.GroundHeightAt(30000.0, 0.0);
    SC_EXPECT(top > r.GroundHeightAt(0.0, 0.0) + 250.0);
    Aircraft under;
    under.name = "under the ridge";
    under.position = GroundPointInFrame(frame, 30000.0, 0.0, top - 40.0);
    Aircraft over;
    over.name = "over the ridge";
    over.position = GroundPointInFrame(frame, 30000.0, 100.0, top + 60.0);
    Aircraft skimming;  // its belly 1.5 m under its centre, the centre 1 m over the ground: the belly is under it
    skimming.name = "skimming";
    skimming.position = GroundPointInFrame(frame, 30000.0, 200.0, r.GroundHeightAt(30000.0, 200.0) + 1.0);
    r.GetWorld().AddAircraft(under);
    r.GetWorld().AddAircraft(over);
    r.GetWorld().AddAircraft(skimming);
    const std::vector<BelowGround> listed = EntitiesOffGround(r);
    bool found_under = false, found_over = false, found_skim = false;
    for (const BelowGround& b : listed) {
        std::printf("    %s\n", b.Line().c_str());
        found_under = found_under || b.id == "under the ridge";
        found_over = found_over || b.id == "over the ridge";
        found_skim = found_skim || b.id == "skimming";
        if (b.id == "under the ridge") SC_EXPECT_NEAR(b.below_m, 40.0 + 1.5, 0.01);
        if (b.id == "skimming") SC_EXPECT_NEAR(b.below_m, 0.5, 0.01);
    }
    SC_EXPECT(found_under && !found_over && found_skim);
    SC_EXPECT(listed.size() == 2);  // nothing else: the battery and the own vehicle stand on the ground
}

// ---------------------------------------------------------------------------------------------- held over the ground

SC_TEST("ground floor: a terrain-following helicopter over a ridge keeps its skids a metre over the ground") {
    const LocalFrame frame(GeoPoint::FromDegrees(47.0, 20.0, 0.0));
    TerrainRidge ridge{4000.0, -5000.0, 4000.0, 5000.0, 180.0, 600.0};
    const RidgeTerrain ground(100.0, {ridge});
    TargetBehaviourDef b;
    b.kind = TargetBehaviourDef::Kind::TerrainFollowing;
    b.agl_m = 3.0;  // lower than its skids allow: the floor holds it
    b.max_climb_mps = 8.0;
    TargetPilot pilot(b, EvadeDef());
    Aircraft a;
    a.type = "Air+Rotorcraft";
    a.bottom_m = 2.5;
    a.speed_mps = 60.0;
    a.position = GroundPointInFrame(frame, 0.0, 0.0, 120.0);
    a.waypoints = {GroundPointInFrame(frame, 9000.0, 0.0, 120.0)};
    double least = 1e9;
    for (double t = 0.0; t < 140.0; t += 0.1) {
        pilot.BeforeStep(a);
        StepAircraft(a, 0.1);
        pilot.AfterStep(a, ground, frame, 0.1, t);
        least = std::min(least, frame.AltitudeOf(a.position) - a.bottom_m - ground.HeightAt(a.position.x, a.position.y));
    }
    std::printf("    the skids' least height over the ground: %.2f m (the ridge 180 m)\n", least);
    SC_EXPECT(least >= 1.0 - 1e-6);
    SC_EXPECT(a.position.x > 8000.0);  // flew over it
}

SC_TEST("ground floor: an aircraft flying a straight line through a hill pulls up over it and never under the ground") {
    const LocalFrame frame(GeoPoint::FromDegrees(47.0, 20.0, 0.0));
    TerrainRidge hill{5000.0, -3000.0, 5000.0, 3000.0, 250.0, 800.0};
    const RidgeTerrain ground(100.0, {hill});
    TargetPilot pilot;  // Kind::None: the world flies it at its waypoints' heights
    Aircraft a;
    a.bottom_m = 1.5;
    a.speed_mps = 220.0;
    a.max_turn_rate_rad_s = DegToRad(3.0);
    a.position = GroundPointInFrame(frame, 0.0, 0.0, 200.0);
    a.velocity = Vec3{220.0, 0.0, 0.0};  // already heading east, at the hill
    a.waypoints = {GroundPointInFrame(frame, 12000.0, 0.0, 200.0)};
    double least = 1e9;
    int pulled = 0;
    for (double t = 0.0; t < 50.0; t += 0.1) {
        pilot.BeforeStep(a);
        StepAircraft(a, 0.1);
        if (pilot.AfterStep(a, ground, frame, 0.1, t) == "pulled up over the ground") ++pulled;
        least = std::min(least, frame.AltitudeOf(a.position) - a.bottom_m - ground.HeightAt(a.position.x, a.position.y));
    }
    std::printf("    pulled up %d times; its belly's least height over the ground %.2f m\n", pulled, least);
    SC_EXPECT(pulled >= 1);
    SC_EXPECT(least >= -1e-6);
    SC_EXPECT(a.position.x > 8000.0);  // flew over it
}

SC_TEST("ground floor: the waves' reconnaissance helicopters fly low, and every wave aircraft has its belly's depth") {
    auto def = std::make_shared<airwar::WavesDef>();
    std::string e;
    SC_EXPECT(airwar::LoadWavesFile(kData, "gameplay/test_waves.json", *def, &e));
    FlatTerrain flat(100.0);
    const LocalFrame frame(GeoPoint::FromDegrees(47.42, 20.18, 100.0));
    airwar::WaveScene scene;
    scene.frame = &frame;
    scene.terrain = &flat;
    scene.defences = {Vec3{1000.0, 500.0, 0.0}};
    airwar::TestWaves waves(def, 11);
    double t = def->first_wave_s;
    int recce_helicopters = 0, low = 0, weapons = 0, low_weapons = 0;
    for (int i = 0; i < 200; ++i) {
        const airwar::Wave w = waves.Next(t, scene);
        for (const ScenarioTarget& a : w.aircraft) {
            SC_EXPECT(a.bottom_m > 0.0);
            if (a.weapon) {
                ++weapons;
                SC_EXPECT_NEAR(a.BottomM(), 0.3, 1e-12);
                // A low one ends at its aim (no waypoint past it); a high one dives on 2 km under the ground.
                if (a.behaviour.kind == TargetBehaviourDef::Kind::TerrainFollowing) {
                    ++low_weapons;
                    SC_EXPECT(a.behaviour.waypoint_agl_m.size() == a.waypoints.size() && a.behaviour.waypoint_agl_m.back() == 0.0);
                }
            }
            if (a.type.find("Rotorcraft") == std::string::npos || a.name.find(airwar::FlightRoleName(airwar::FlightRole::Recce)) == std::string::npos) {
                continue;
            }
            ++recce_helicopters;
            low += a.behaviour.kind == TargetBehaviourDef::Kind::TerrainFollowing ? 1 : 0;
            SC_EXPECT_NEAR(a.BottomM(), 2.5, 1e-12);
        }
        t = waves.NextWaveS();
    }
    std::printf("    %d reconnaissance helicopters, %d low; %d weapons, %d low\n", recce_helicopters, low, weapons, low_weapons);
    SC_EXPECT(low == recce_helicopters);
    SC_EXPECT(weapons > 0);
}

SC_TEST("ground floor: a low cruise missile ends on its aim, on the ground; a diving one where it strikes the ground") {
    ScenarioDef def;
    ConsoleData console;
    if (!LoadScenario("scenarios/osa_practice.json", def, console)) return;
    ScenarioRunner r(std::move(def), std::move(console));
    const LocalFrame& frame = r.GetWorld().Frame();
    // Out of the Osa's reach: 20 km east.
    const double base = r.GroundHeightAt(20000.0, 0.0);
    ScenarioTarget low = Weapon("low cruise", GroundPointInFrame(frame, 20000.0, -6000.0, base + 50.0),
                                {GroundPointInFrame(frame, 20000.0, 4000.0, base + 50.0), r.GroundEnu(20000.0, 14000.0)});
    low.behaviour.kind = TargetBehaviourDef::Kind::TerrainFollowing;
    low.behaviour.agl_m = 50.0;
    low.behaviour.max_climb_mps = 120.0;
    low.behaviour.waypoint_agl_m = {50.0, 0.0};
    const Vec3 low_aim = low.waypoints.back();
    ScenarioTarget dive = Weapon("diving", GroundPointInFrame(frame, 26000.0, -6000.0, base + 3000.0),
                                 {r.GroundEnu(26000.0, 6000.0), r.GroundEnu(26000.0, 9000.0) - Vec3{0.0, 0.0, 2000.0}});
    std::string why;
    SC_EXPECT(r.AddTargets({low, dive}, "test", &why));
    if (!why.empty()) std::printf("    %s\n", why.c_str());
    std::uint64_t cursor = 0;
    int impacts = 0;
    while (r.Time() < 200.0 && !r.Finished() && impacts < 2) {
        r.Step(0.1);
        for (const fx::Event& ev : r.Fx().Since(cursor)) {
            if (ev.kind != fx::EventKind::CruiseImpact) continue;
            ++impacts;
            const double above = frame.AltitudeOf(ev.position) - r.GroundHeightAt(ev.position.x, ev.position.y);
            std::printf("    %s strikes at %.0f s, %.2f m over the ground, (%.0f, %.0f)\n", ev.source.c_str(), ev.t_s, above,
                        ev.position.x, ev.position.y);
            SC_EXPECT(std::fabs(above) <= 0.5);
            if (ev.source == "low cruise") SC_EXPECT((ev.position - low_aim).HorizontalLength() <= 50.0);
            // The diving one strikes the first ground on its line, near its aim: a rise short of the aim (osa_practice's
            // ground is not flat) takes it a little early.
            if (ev.source == "diving") SC_EXPECT(ev.position.y > 6000.0 - 500.0 && ev.position.y < 9000.0);
        }
        cursor = r.Fx().Last();
    }
    SC_EXPECT(impacts == 2 && r.WeaponImpacts() == 2);
    SC_EXPECT(r.TerrainStrikes() == 0);
}

SC_TEST("ground floor: an aircraft past its last waypoint, flying on down its line, is still held over the ground") {
    ScenarioDef def;
    ConsoleData console;
    if (!LoadScenario("scenarios/osa_practice.json", def, console)) return;
    ScenarioRunner r(std::move(def), std::move(console));
    const LocalFrame& frame = r.GetWorld().Frame();
    // Out of the Osa's reach, 20 km east: a shallow descent that ends 150 m over the ground and carries on down its line.
    const double base = r.GroundHeightAt(20000.0, -8000.0);
    ScenarioTarget t;
    t.id = t.name = "past its last waypoint";
    t.type = "Air+FixedWing";
    t.start = GroundPointInFrame(frame, 20000.0, -8000.0, base + 400.0);
    t.waypoints = {GroundPointInFrame(frame, 20000.0, -4000.0, r.GroundHeightAt(20000.0, -4000.0) + 150.0)};
    t.speed_mps = 200.0;
    t.max_turn_rate_rad_s = DegToRad(4.0);
    std::string why;
    SC_EXPECT(r.AddTargets({t}, "test", &why));
    double least = 1e9;
    while (r.Time() < 90.0 && !r.Finished()) {
        r.Step(0.1);
        for (const Aircraft& a : r.GetWorld().AircraftList()) {
            if (a.name != t.name || !a.alive) continue;
            least = std::min(least, frame.AltitudeOf(a.position) - a.bottom_m - r.GroundHeightAt(a.position.x, a.position.y));
        }
    }
    std::printf("    its belly's least height over the ground: %.2f m\n", least);
    SC_EXPECT(r.Debrief().find("passed its last waypoint") != std::string::npos);
    SC_EXPECT(least >= -1e-6 && least < 50.0);  // it came down to the ground and was held over it
    SC_EXPECT(r.TerrainStrikes() == 0);
}
