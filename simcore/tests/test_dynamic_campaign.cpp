// The dynamic campaign testbed (docs/DYNAMIC_CAMPAIGN_TEST.md): its scenario round the capital, the random waves of
// simcore/air_war/test_waves.hpp flown by the runner (scenario "waves"), and what they give the presentation feed.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war/test_waves.hpp"
#include "simcore/ground_check.hpp"
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
    // Every one of them but the Osa batteries (the battery fire model's) shoots with its system's figures in the theatre.
    for (const ScenarioNeighbour& nb : def.neighbours) SC_EXPECT(def.waves->Defence(nb.system) != nullptr);
    // The search radars with a sourced range feed the regiment's picture: the P-15, P-18, P-19 and P-40.
    std::set<std::string> fed;
    for (const C2SourceDef& src : def.c2_sources) {
        if (src.id != "pu12") continue;
        for (const RegimentRadar& rr : src.feed) fed.insert(rr.type);
    }
    for (const char* t : {"p15", "p18", "p19", "p40"}) SC_EXPECT(fed.count(t) == 1);
    scenery::ModelLibrary lib;
    SC_EXPECT(scenery::LoadModelLibrary(kData + "/scenery/basic_models.json", lib, &e));
    std::set<std::string> models;
    for (const ScenarioScenery& s : def.scenery) {
        SC_EXPECT(lib.Find(s.model) != nullptr);
        models.insert(s.model);
    }
    for (const char* m : {"p14", "p15", "p18", "p19", "p40", "prv11"}) SC_EXPECT(models.count(m) == 1);
    // Our own types are drawn with models of the library (the loads by the effects, from the munitions library).
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

SC_TEST("dynamic campaign: a strike aircraft at its release range lets its load go, timed to land on its aim") {
    const std::shared_ptr<const WavesDef> def = LoadWaves();
    FlatTerrain ground(100.0);
    const LocalFrame frame(GeoPoint::FromDegrees(47.42, 20.18, 100.0));
    WaveScene scene;
    scene.frame = &frame;
    scene.terrain = &ground;
    scene.defences = {Vec3{1000.0, 500.0, 0.0}};
    TestWaves waves(def, 3);
    std::vector<ScenarioTarget> all;
    double t = def->first_wave_s;
    for (int i = 0; i < 40; ++i) {
        Wave w = waves.Next(t, scene);
        for (ScenarioTarget& a : w.aircraft) all.push_back(a);
        t = waves.NextWaveS();
    }
    // Far from any aim nothing is let go.
    const auto far = [](const std::string&, Vec3& p, Vec3& v) {
        p = Vec3{500000.0, 0.0, 300.0};
        v = Vec3{200.0, 0.0, 0.0};
        return true;
    };
    SC_EXPECT(waves.Releases(t, scene, far).empty());
    SC_EXPECT(!waves.WeaponsLoaded().empty());
    // Each carrier put over each of its waypoints in turn: a strike's aim is the one after its IP, a recce's the first.
    std::vector<WaveRelease> loads;
    for (int pass = 0; pass < 4; ++pass) {
        const auto over = [&](const std::string& id, Vec3& p, Vec3& v) {
            for (const ScenarioTarget& a : all) {
                if (a.id != id) continue;
                const std::size_t k = std::min(a.waypoints.size() - 1, static_cast<std::size_t>(pass));
                p = Vec3{a.waypoints[k].x, a.waypoints[k].y, 300.0};
                v = Vec3{150.0, 0.0, 0.0};
                return true;
            }
            return false;
        };
        for (WaveRelease& r : waves.Releases(t + 1.0, scene, over)) loads.push_back(r);
    }
    SC_EXPECT(!loads.empty() && waves.Drops() == static_cast<int>(loads.size()));
    std::set<std::string> deliveries;
    for (const WaveRelease& r : loads) {
        const WeaponDef* wd = def->weapons.Find(r.weapon);
        SC_EXPECT(wd != nullptr && r.count >= 1 && !r.carrier.empty());
        // On the ground, after a flight of 1-60 s (weapon_flight.hpp), by a delivery its weapon allows.
        SC_EXPECT(std::fabs(frame.AltitudeOf(r.impact) - 100.0) < 1.0);
        SC_EXPECT(r.impact_t_s >= r.t_s + 1.0 - 1e-9 && r.impact_t_s <= r.t_s + 60.0 + 1e-9);
        if (wd != nullptr && !wd->deliveries.empty()) {
            bool allowed = false;
            for (Delivery d : wd->deliveries) allowed = allowed || r.delivery == DeliveryName(d);
            SC_EXPECT(allowed);
        }
        deliveries.insert(r.delivery);
    }
    SC_EXPECT(deliveries.size() >= 2);
}

SC_TEST("dynamic campaign: a defence's chance to bring an aircraft down on its pass is the raids' own, on its cover") {
    const std::shared_ptr<const WavesDef> def = LoadWaves();
    const DefenceDef* s125 = def->Defence("s125m1");
    const DefenceDef* s75 = def->Defence("s75m2");
    const DefenceDef* s60 = def->Defence("s60");
    SC_EXPECT(s125 != nullptr && s75 != nullptr && s60 != nullptr);
    if (s125 == nullptr || s75 == nullptr || s60 == nullptr) return;
    const TuningTable& tune = def->tuning;
    SC_EXPECT_NEAR(tune.Get("attrition.pass_scale"), 0.5, 1e-9);  // gameplay/air_war.json, not the defaults
    const Vec3 site{0.0, 0.0, 0.0};
    const double r = 22000.0;
    // At medium height through the middle from the edge: the whole chord, lethality x pass_scale = 0.25 x 0.5.
    SC_EXPECT_NEAR(WavePassKillChance(*s125, r, site, Vec3{-r, 0.0, 5000.0}, Vec3{250.0, 0.0, 0.0}, 5000.0, false, tune), 0.125, 1e-9);
    // Half way past the middle: a quarter of the chord is left.
    SC_EXPECT_NEAR(WavePassKillChance(*s125, r, site, Vec3{0.5 * r, 0.0, 5000.0}, Vec3{250.0, 0.0, 0.0}, 5000.0, false, tune),
                   0.125 * 0.25, 1e-9);
    // Too fast for it (560 m/s), and low under the S-75's cover (it has none low).
    SC_EXPECT(WavePassKillChance(*s125, r, site, Vec3{-r, 0.0, 5000.0}, Vec3{600.0, 0.0, 0.0}, 5000.0, false, tune) == 0.0);
    SC_EXPECT(WavePassKillChance(*s75, 40000.0, site, Vec3{-40000.0, 0.0, 200.0}, Vec3{250.0, 0.0, 0.0}, 200.0, false, tune) == 0.0);
    // An S-60 against a slow low flier: 0.07 x 0.5.
    SC_EXPECT_NEAR(WavePassKillChance(*s60, 6000.0, site, Vec3{-6000.0, 0.0, 300.0}, Vec3{200.0, 0.0, 0.0}, 300.0, false, tune), 0.035, 1e-9);
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
    // The presentation feed, read as the game reads it (fx/events.hpp): the loads' releases and impacts, the cruise
    // missiles' warheads, the guns' passes.
    std::uint64_t cursor = 0;
    int releases = 0, impacts = 0, cruise = 0;
    std::set<std::string> guns, sams;
    int sam_launches = 0, sam_hits = 0, sam_misses = 0, sam_near = 0;
    int flak = 0, damaged = 0, jettisons = 0;  // the heavy guns' shells burst, an aircraft's damage risen (damage.hpp)
    std::set<std::string> defences;  // the neighbours the battery fire model does not fight
    for (const ScenarioNeighbour& nb : r.Def().neighbours) {
        if (!nb.fights) defences.insert(nb.id);
    }
    bool well_formed = true;
    std::map<std::string, double> released_at;
    while (r.Time() < 1800.0 && !r.Finished()) {
        r.Step(0.1);
        most = std::max(most, r.GetWorld().AircraftList().size());
        for (const fx::Event& ev : r.Fx().Since(cursor)) {
            if (ev.kind == fx::EventKind::WeaponRelease) {
                ++releases;
                jettisons += ev.delivery == "jettison" ? 1 : 0;
                released_at[ev.source] = ev.t_s;
                well_formed = well_formed && ev.end_t_s > ev.t_s && !ev.weapon.empty() && ev.world_id >= 0;
            } else if (ev.kind == fx::EventKind::WeaponImpact) {
                ++impacts;
                const auto it = released_at.find(ev.source);
                well_formed = well_formed && it != released_at.end() && ev.t_s > it->second;
            } else if (ev.kind == fx::EventKind::CruiseImpact) {
                ++cruise;
            } else if (ev.kind == fx::EventKind::SamLaunch && defences.count(ev.source) == 1) {
                ++sam_launches;
                sams.insert(ev.weapon);
                well_formed = well_formed && ev.end_t_s > ev.t_s && ev.world_id >= 0;
            } else if (ev.kind == fx::EventKind::SamEnd && defences.count(ev.source) == 1) {
                sam_hits += ev.end == fx::BurstEnd::Hit ? 1 : 0;
                sam_misses += ev.end == fx::BurstEnd::SelfDestruct ? 1 : 0;
                sam_near += ev.end == fx::BurstEnd::NearBurst ? 1 : 0;
            } else if (ev.kind == fx::EventKind::FlakBurst) {
                ++flak;
                well_formed = well_formed && ev.weapon == "ks30";
            } else if (ev.kind == fx::EventKind::AircraftDamaged) {
                ++damaged;
                well_formed = well_formed && ev.number > 0 && !ev.target.empty();
            } else if (ev.kind == fx::EventKind::GunPass) {
                guns.insert(ev.weapon);
                well_formed = well_formed && ev.end_t_s > ev.t_s && ev.number > 0;
            }
        }
        cursor = r.Fx().Last();
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
    std::printf("    the feed: %d releases, %d impacts, %d cruise missiles' warheads, gun passes from %zu kinds of gun\n", releases,
                impacts, cruise, guns.size());
    SC_EXPECT(releases == w->Drops() + jettisons && impacts > 0 && impacts <= releases && well_formed);
    SC_EXPECT(guns.count("s60") == 1 && guns.size() >= 3);
    // Our air defence round the capital fights: missiles from more than one kind of site, aircraft brought down.
    std::string kinds;
    for (const std::string& k : sams) kinds += " " + k;
    std::printf("    our defences: %d missiles launched (%d hit, %d missed) from%s; %d aircraft brought down by them\n", sam_launches,
                sam_hits, sam_misses, kinds.c_str(), r.DefenceKills());
    SC_EXPECT(sam_launches == r.DefenceLaunches() && sam_launches > 0 && sams.size() >= 2);
    SC_EXPECT(sam_hits + sam_misses + sam_near <= sam_launches);
    // Damage by chance (damage.hpp, aaa.hpp): the guns fire rounds, a few strike; the heavy guns' shells burst in the
    // sky, mostly to no effect; aircraft are damaged short of destroyed and some fly on.
    const int raised = r.DefenceDamaged("light") + r.DefenceDamaged("moderate") + r.DefenceDamaged("heavy");
    std::printf("    damage: %d rounds fired, %d struck, %d heavy shells burst; risen to light %d, moderate %d, heavy %d; %d missiles burst near, %d loads jettisoned\n",
                r.DefenceRoundsFired(), r.DefenceRoundHits(), r.DefenceShellBursts(), r.DefenceDamaged("light"), r.DefenceDamaged("moderate"),
                r.DefenceDamaged("heavy"), sam_near, jettisons);
    SC_EXPECT(r.DefenceRoundsFired() > 0 && r.DefenceRoundHits() < r.DefenceRoundsFired() / 10);
    // Light damage leaves an aircraft as it was: only moderate and heavy show in the feed.
    SC_EXPECT(flak == r.DefenceShellBursts() && raised > 0 && damaged > 0);
    SC_EXPECT(damaged <= r.DefenceDamaged("moderate") + r.DefenceDamaged("heavy"));
    SC_EXPECT(r.DefenceKills() > 0);
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
    SC_EXPECT(d.find("wave 1") != std::string::npos && d.find("impact") != std::string::npos && d.find("destroyed by") != std::string::npos);
}

// The player's battery stands on open ground with sight along the attack directions (voenizdat_osa_1991 pp. 232-239,
// docs/campaign/11_BATTERY_LIFE_PROPOSAL.md "Main positions"), not among the capital's blocks of flats (roadmap R6): from
// each of battery 21's Osas, a 5 m antenna (data/em/osa_ak.json) sees over the ground, the woods and the buildings
// within 6 km at under half a degree in every direction. Sited in the dense core (1.2 km east, 0.9 km north) a ring of
// flats 285-400 m off masked 1-3 degrees, and a cruise missile 60 m up was hidden until it was inside the ring.
SC_TEST("dynamic campaign: battery 21's Osas see over the capital's roofs, sited on open ground at its edge") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/dynamic_campaign_test.json", kData, def, console, &e));
    if (!def.has_battery) return;
    std::vector<Vec3> osas{def.unit_position};
    for (const BatteryVehicleDef& v : def.battery_vehicles) {
        if (!v.own && v.kind == BatteryVehicleKind::Launcher) osas.push_back(v.start);
    }
    SC_EXPECT(osas.size() == 4u);
    ScenarioRunner r(std::move(def), std::move(console));
    const Terrain& t = r.GetWorld().GetTerrain();
    const double earth_m = kStandardRefractionK * wgs84::kMeanRadius;
    const double antenna_m = 5.0;  // test value: the SOC's height over the ground, as data/em/osa_ak.json gives it
    double worst_deg = -90.0;
    for (const Vec3& p : osas) {
        const double eye = t.HeightAt(p.x, p.y) + antenna_m;
        for (int az = 0; az < 360; az += 5) {
            const double a = DegToRad(static_cast<double>(az));
            for (double s = kObstacleSkipM; s <= 6000.0; s += 10.0) {
                const double x = p.x + s * std::sin(a), y = p.y + s * std::cos(a);
                const double top = t.HeightAt(x, y) + t.ObstacleHeightAt(x, y) - s * s / (2.0 * earth_m);
                worst_deg = std::max(worst_deg, RadToDeg(std::atan2(top - eye, s)));
            }
        }
    }
    std::printf("    worst mask from battery 21's Osas: %.2f deg\n", worst_deg);
    SC_EXPECT(worst_deg < 0.5);
}

// Roadmap R5, "nothing under the map": ten minutes of the testbed's waves, judged every five seconds against the ground
// the game draws (simcore/ground_check.hpp): no aircraft, weapon, missile or ground item under it, none flown into it.
SC_TEST("dynamic campaign: nothing is under the ground, and no aircraft flies into it") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/dynamic_campaign_test.json", kData, def, console, &e));
    if (def.waves == nullptr) return;
    ScenarioRunner r(std::move(def), std::move(console));
    int checks = 0;
    std::size_t listed = 0;
    double next_s = 0.0;
    while (r.Time() < 600.0 && !r.Finished()) {
        r.Step(0.1);
        if (r.Time() + 1e-9 < next_s) continue;
        next_s += 5.0;
        ++checks;
        for (const BelowGround& b : EntitiesOffGround(r)) {
            if (++listed <= 5) std::printf("    %s\n", b.Line().c_str());
        }
    }
    SC_EXPECT(checks >= 100);
    SC_EXPECT(listed == 0u);
    SC_EXPECT(r.TerrainStrikes() == 0);
}
