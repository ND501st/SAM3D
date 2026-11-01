// The dynamic campaign testbed (docs/DYNAMIC_CAMPAIGN_TEST.md): its scenario round the capital, and the random waves of
// simcore/air_war/test_waves.hpp flown by the runner (scenario "waves").
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war/test_waves.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/scenery/basic_models.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::shared_ptr<const WavesDef> LoadWaves() {
    auto w = std::make_shared<WavesDef>();
    std::string e;
    if (!LoadWavesFile(kData, "gameplay/test_waves.json", *w, &e)) std::printf("    %s\n", e.c_str());
    return w;
}

}  // namespace

SC_TEST("dynamic campaign: the scenario loads round the capital with every kind of our air defence and radar, S-60s aplenty") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/dynamic_campaign_test.json", kData, def, console, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(def.waves != nullptr && def.waves_file == "gameplay/test_waves.json");
    SC_EXPECT(def.npc_commander && def.has_battery && def.has_em && def.time_limit_s >= 4.0 * 3600.0 && !def.end_when_targets_gone &&
              !def.end_on_unit_hit);  // an ARM on the vehicle does not end the testbed
    std::set<std::string> systems;
    int s60 = 0;
    for (const ScenarioNeighbour& nb : def.neighbours) {
        systems.insert(nb.system);
        s60 += nb.system == "s60" ? 1 : 0;
        SC_EXPECT(nb.position.HorizontalLength() < 9000.0);  // in or by the city
    }
    for (const char* s : {"osa", "kub", "s75m2", "s125m1", "strela10", "strela1", "zsu234", "s60", "ks30", "zu23", "aaa", "manpads"}) {
        SC_EXPECT(systems.count(s) == 1);
    }
    SC_EXPECT(s60 >= 8);
    scenery::ModelLibrary lib;
    SC_EXPECT(scenery::LoadModelLibrary(kData + "/scenery/basic_models.json", lib, &e));
    std::set<std::string> models;
    for (const ScenarioScenery& s : def.scenery) {
        SC_EXPECT(lib.Find(s.model) != nullptr);
        models.insert(s.model);
    }
    for (const char* m : {"p14", "p15", "p18", "p19", "p40", "prv11"}) SC_EXPECT(models.count(m) == 1);
    // The waves' weapons and our own types are drawn with models of the library.
    for (const WaveWeaponKind& k : def.waves->weapon_kinds) SC_EXPECT(lib.Find(k.model) != nullptr);
    for (const WaveFriendlyType& f : def.waves->friendly) SC_EXPECT(lib.Find(f.model) != nullptr);
}

SC_TEST("dynamic campaign: the waves bring every adversary's aircraft type round, and ours, each flight on a route") {
    const std::shared_ptr<const WavesDef> def = LoadWaves();
    SC_EXPECT(def->factions.size() == 4);
    FlatTerrain ground(100.0);
    const LocalFrame frame(GeoPoint::FromDegrees(47.42, 20.18, 100.0));
    WaveScene scene;
    scene.frame = &frame;
    scene.terrain = &ground;
    scene.defences = {Vec3{1000.0, 500.0, 0.0}, Vec3{-3000.0, 2000.0, 0.0}};
    scene.objects = {Vec3{-2000.0, -2000.0, 0.0}};
    TestWaves waves(def, 7);
    SC_EXPECT(!waves.Due(0.0, 0) && waves.Due(def->first_wave_s, 0));
    SC_EXPECT(!waves.Due(def->first_wave_s + 1.0, def->max_airborne));  // the sky is full
    std::size_t types = 0;
    for (const FactionDef& f : def->factions) types += f.aircraft.size();
    double t = def->first_wave_s;
    int aircraft = 0, friends = 0, weapons = 0;
    std::set<std::string> ids;
    for (int i = 0; i < 60; ++i) {
        const Wave w = waves.Next(t, scene);
        SC_EXPECT(w.number == i + 1 && !w.flights.empty());
        for (const ScenarioTarget& a : w.aircraft) {
            ++aircraft;
            friends += a.friendly ? 1 : 0;
            weapons += a.weapon ? 1 : 0;  // cruise missiles
            SC_EXPECT(!a.waypoints.empty() && ids.insert(a.id).second);
            SC_EXPECT(a.behaviour.kind == TargetBehaviourDef::Kind::None || a.behaviour.waypoint_agl_m.size() == a.waypoints.size());
            SC_EXPECT(a.appear_at_s >= t);
        }
        t = waves.NextWaveS();
    }
    SC_EXPECT(waves.EnemyTypesFlown().size() == types);
    SC_EXPECT(waves.FriendlyTypesFlown().size() == def->friendly.size());
    SC_EXPECT(friends > 0 && friends < aircraft && weapons > 0);
}

SC_TEST("dynamic campaign: a strike aircraft at its release range drops its load, which flies to the ground at its aim") {
    const std::shared_ptr<const WavesDef> def = LoadWaves();
    FlatTerrain ground(100.0);
    const LocalFrame frame(GeoPoint::FromDegrees(47.42, 20.18, 100.0));
    WaveScene scene;
    scene.frame = &frame;
    scene.terrain = &ground;
    scene.defences = {Vec3{1000.0, 500.0, 0.0}};
    TestWaves waves(def, 3);
    // Every aircraft is put on its aim at once: those with a load to drop release it.
    std::vector<ScenarioTarget> all;
    double t = def->first_wave_s;
    for (int i = 0; i < 40; ++i) {
        Wave w = waves.Next(t, scene);
        for (ScenarioTarget& a : w.aircraft) all.push_back(a);
        t = waves.NextWaveS();
    }
    // Far from any aim nothing drops; within the release range of each it does.
    const auto far = [](const std::string&, Vec3& p, Vec3& v) {
        p = Vec3{500000.0, 0.0, 300.0};
        v = Vec3{200.0, 0.0, 0.0};
        return true;
    };
    SC_EXPECT(waves.Releases(t, scene, far).empty());
    SC_EXPECT(!waves.WeaponsLoaded().empty());
    // Now a lookup that places each carrier over its aim (the drop range is what the release looks at).
    std::vector<ScenarioTarget> dropped;
    for (int pass = 0; pass < 4; ++pass) {
        const auto over = [&](const std::string& id, Vec3& p, Vec3& v) {
            for (const ScenarioTarget& a : all) {
                if (a.id != id) continue;
                // Over each of its waypoints in turn: a strike's aim is the one after its IP, a recce's the first.
                const std::size_t k = std::min(a.waypoints.size() - 1, static_cast<std::size_t>(pass));
                p = Vec3{a.waypoints[k].x, a.waypoints[k].y, 300.0};
                v = Vec3{150.0, 0.0, 0.0};
                return true;
            }
            return false;
        };
        for (ScenarioTarget& w : waves.Releases(t + 1.0, scene, over)) dropped.push_back(w);
    }
    SC_EXPECT(!dropped.empty() && waves.Drops() > 0);
    for (const ScenarioTarget& w : dropped) {
        SC_EXPECT(w.weapon && !w.friendly && w.waypoints.size() == 2);
        // Its first waypoint is on the ground, the second below it on the same line.
        SC_EXPECT(std::fabs(frame.AltitudeOf(w.waypoints[0]) - 100.0) < 1.0);
        SC_EXPECT(frame.AltitudeOf(w.waypoints[1]) < frame.AltitudeOf(w.waypoints[0]));
        SC_EXPECT(!w.model.empty() && w.appear_at_s >= t + 1.0);
    }
}

SC_TEST("dynamic campaign: the session keeps the waves coming, their loads strike, and the long gone leave the world") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/dynamic_campaign_test.json", kData, def, console, &e));
    if (def.waves == nullptr) return;
    ScenarioRunner r(std::move(def), std::move(console));
    const auto t0 = std::chrono::steady_clock::now();
    std::size_t most = 0;
    while (r.Time() < 1800.0 && !r.Finished()) {
        r.Step(0.1);
        most = std::max(most, r.GetWorld().AircraftList().size());
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const TestWaves* w = r.Waves();
    if (r.Finished()) std::printf("    ended at %.0f s: %s\n", r.Time(), r.EndReason().c_str());
    SC_EXPECT(w != nullptr && !r.Finished());
    if (w == nullptr) return;
    std::printf("    30 min: %d waves, %d drops, %d impacts, %zu targets, at most %zu aircraft in the world, %.1f s wall\n", w->Waves(),
                w->Drops(), r.WeaponImpacts(), r.Def().targets.size(), most, wall);
    SC_EXPECT(w->Waves() >= 10);
    SC_EXPECT(w->Drops() > 0 && r.WeaponImpacts() > 0);
    SC_EXPECT(r.WavesAirborne() <= r.Def().waves->max_airborne + 40);  // a wave may come on top of a nearly full sky
    // The world holds what flies, not everything that has flown.
    SC_EXPECT(r.GetWorld().AircraftList().size() < r.Def().targets.size());
    bool friend_seen = false, enemy_seen = false;
    for (const ScenarioTarget& t : r.Def().targets) {
        friend_seen = friend_seen || t.friendly;
        enemy_seen = enemy_seen || (!t.friendly && !t.weapon);
    }
    SC_EXPECT(friend_seen && enemy_seen);
    const std::string d = r.Debrief();
    SC_EXPECT(d.find("wave 1") != std::string::npos && d.find("impact") != std::string::npos);
}
