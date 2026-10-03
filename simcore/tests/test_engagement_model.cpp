#include <string>
#include <utility>

#include "simcore/engagement.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

// An Osa scenario with the gameplay engagement model: the unit starts halted; the commander
// powers up and deploys, the driver puts the generator on line, the senior operator searches.
// `target` is one target object; `triggers` are extra triggers (with a leading comma).
std::string Engage(const std::string& target, const std::string& triggers = "", unsigned seed = 1) {
    return R"({
      "scenario_id": "e", "name": "E", "source": "sam3d", "confidence": "estimate", "seed": )" +
           std::to_string(seed) + R"(,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 },
      "terrain": { "type": "flat", "height_m": 0 },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [ )" +
           target + R"( ],
      "triggers": [
        { "id": "up", "when": {}, "do": [ { "order": "power_up", "crew": "Driver" },
                                           { "queue": "pto_on_line", "crew": "Driver" },
                                           { "order": "equipment_on", "crew": "Operator" },
                                           { "queue": "deploy_at_halt", "crew": "Operator" },
                                           { "order": "search_on_march", "crew": "Senior operator" } ] } )" +
           triggers + R"( ],
      "end": { "time_limit_s": 700 } })";
}

const char* kAcquireAndFire = R"(,
        { "id": "acq", "when": { "conditions": [ { "subsystem": "soc_transmitter", "is": "ready" } ] },
          "do": [ { "order": "acquire_with_soc", "crew": "Senior operator" }, { "order": "ready_capture", "crew": "Deputy" },
                  { "order": "capture_with_ssc", "crew": "Operator" },
                  { "queue": "prepare_firing", "crew": "Deputy" }, { "queue": "launch", "crew": "Deputy" } ] })";

std::string Target(double north_m, double up_m, const std::string& extra = "") {
    return R"({ "id": "t1", "name": "T", "class": "drone", "start": { "east_m": 1500, "north_m": )" +
           std::to_string(north_m) + R"(, "up_m": )" + std::to_string(up_m) +
           R"( }, "waypoints": [ { "east_m": 1500, "north_m": -20000, "up_m": )" + std::to_string(up_m) +
           R"( } ], "speed_mps": 150, "appear_at_s": 230)" + extra + " }";  // after the crew's 4 min to readiness
}

struct Run {
    std::string debrief;
    DebriefSummary summary;
    bool loaded = false;
};

Run Play(const std::string& text, double max_s = 700.0) {
    Run r;
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    r.loaded = LoadScenario(text, kData, def, console, &error);
    if (!r.loaded) {
        std::printf("    load: %s\n", error.c_str());
        return r;
    }
    ScenarioRunner runner(std::move(def), std::move(console));
    runner.RunToEnd(0.1, max_s);
    r.debrief = runner.Debrief();
    r.summary = runner.Log().Summary();
    return r;
}

bool Has(const Run& r, const std::string& text) {
    const bool found = r.debrief.find(text) != std::string::npos;
    if (!found) std::printf("    missing \"%s\" in:\n%s\n", text.c_str(), r.debrief.c_str());
    return found;
}

}  // namespace

SC_TEST("tuning: the Osa game-balance file loads; bad values are rejected with paths") {
    EngagementTuning t;
    std::string error;
    SC_EXPECT(LoadEngagementTuningFile(kData + "/gameplay/osa_ak.json", t, &error));
    SC_EXPECT(t.system_id == "osa_ak" && t.aim_az_role == "senior_operator");
    SC_EXPECT(t.aim_el_role == "deputy_commander" && t.aim_range_role == "operator");
    SC_EXPECT_NEAR(t.HitChanceFor("drone"), 0.8, 1e-12);
    SC_EXPECT_NEAR(t.HitChanceFor("zeppelin"), t.HitChanceFor("default"), 1e-12);
    SC_EXPECT_NEAR(t.zone_max_range_m, 10300.0, 1e-9);

    std::string text;
    {
        std::ifstream f(kData + "/gameplay/osa_ak.json", std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        text = ss.str();
    }
    auto broken = [&](const std::string& from, const std::string& to) {
        std::string s = text;
        const std::size_t at = s.find(from);
        if (at == std::string::npos) return std::string("{}");
        return s.replace(at, from.size(), to);
    };
    SC_EXPECT(!LoadEngagementTuning(broken("\"default\": 0.6", "\"standard\": 0.6"), t, &error) &&
              error.find("needs a \"default\" entry") != std::string::npos);
    SC_EXPECT(!LoadEngagementTuning(broken("\"tv_factor\": 0.85", "\"tv_factor\": 1.5"), t, &error) &&
              error.find("outcome.tv_factor") != std::string::npos);
    SC_EXPECT(!LoadEngagementTuning(broken("\"lock_time_s\"", "\"lock_tme_s\""), t, &error) &&
              error.find("track.lock_tme_s: unknown key") != std::string::npos);
}

SC_TEST("engagement: search contact, aimed radar lock, zone lamps, launch and a resolved outcome") {
    const Run r = Play(Engage(Target(24000, 1000), kAcquireAndFire));
    SC_EXPECT(r.loaded);
    SC_EXPECT(Has(r, "SOC              event      t1  contact"));
    SC_EXPECT(Has(r, "radar track"));
    SC_EXPECT(Has(r, "in the zone"));
    SC_EXPECT(r.summary.launches == 1 && r.summary.hits + r.summary.misses == 1);
    SC_EXPECT(Has(r, "chance 0.80 = drone 0.80 x radar track 1.00 x zone 1.00"));
    SC_EXPECT(r.summary.early_actions == 0 && r.summary.equipment_faults == 0);
}

SC_TEST("engagement: the same seed gives the same outcome; over many seeds hits follow the chance") {
    SC_EXPECT(Play(Engage(Target(24000, 1000), kAcquireAndFire, 3)).debrief ==
              Play(Engage(Target(24000, 1000), kAcquireAndFire, 3)).debrief);
    int hits = 0;
    int shots = 0;
    for (unsigned seed = 1; seed <= 20; ++seed) {
        const Run r = Play(Engage(Target(24000, 1000), kAcquireAndFire, seed));
        hits += r.summary.hits;
        shots += r.summary.hits + r.summary.misses;
    }
    SC_EXPECT(shots == 20);
    SC_EXPECT(hits >= 11 && hits <= 19);  // chance 0.8: expect about 16
}

SC_TEST("engagement: losing the track after launch loses the missile") {
    const std::string stow = R"(,
        { "id": "stow", "when": { "launches_at_least": 1 }, "do": [ { "order": "stow_ssc", "crew": "Operator" } ] })";
    const Run r = Play(Engage(Target(24000, 1000), std::string(kAcquireAndFire) + stow));
    SC_EXPECT(r.summary.launches == 1 && r.summary.misses == 1 && r.summary.hits == 0);
    SC_EXPECT(Has(r, "track lost: transmitter off"));
    SC_EXPECT(Has(r, "track lost: no guidance commands"));
}

SC_TEST("engagement: a target too low for the radar lights НЛЦ and is never radar-tracked") {
    ScenarioDef def;
    ConsoleData console;
    // 10 m in the local ENU frame: with Earth curvature its true height is 41 m at 20 km, less closer in,
    // so always under the radar's 50 m game limit.
    SC_EXPECT(LoadScenario(Engage(Target(20000, 10), kAcquireAndFire), kData, def, console));
    ScenarioRunner runner(std::move(def), std::move(console));
    bool too_low = false;
    bool radar_tracked = false;
    for (int i = 0; i < 5000 && !runner.Finished(); ++i) {
        runner.Step(0.1);
        too_low = too_low || runner.Console().equipment.GetInput("target_too_low");
        radar_tracked = radar_tracked || runner.Engagement().Method() == TrackMethod::Radar;
    }
    SC_EXPECT(too_low && !radar_tracked);
    SC_EXPECT(runner.Launches() == 0);  // never in the zone: the crew waits for ЦЕЛЬ В ЗОНЕ
}

SC_TEST("engagement: TV-only engagement tracks with the camera and uses the TV factor") {
    const std::string emcon = R"(,
        { "id": "tv", "when": { "target": "t1", "target_within_m": 30000, "conditions": [ { "subsystem": "apu_free", "is": "ready" } ] },
          "do": [ { "order": "emcon_tv_azimuth", "crew": "Senior operator" },
                  { "order": "emcon_tv_engagement", "crew": "Deputy" } ] })";
    // No search: the senior operator's emissions-control order replaces the search order (still waiting for power).
    const Run r = Play(Engage(Target(12000, 800), emcon));
    SC_EXPECT(Has(r, "TV track"));
    SC_EXPECT(r.summary.launches == 1 && r.summary.hits + r.summary.misses == 1);
    SC_EXPECT(Has(r, "x TV track 0.85"));
}

SC_TEST("engagement: launching beyond reach runs out of flight time") {
    const std::string emcon = R"(,
        { "id": "tv", "when": { "target": "t1", "target_within_m": 30000, "conditions": [ { "subsystem": "apu_free", "is": "ready" } ] },
          "do": [ { "order": "emcon_tv_azimuth", "crew": "Senior operator" },
                  { "order": "emcon_tv_engagement", "crew": "Deputy" } ] })";
    const Run r = Play(Engage(Target(19000, 800), emcon));
    SC_EXPECT(Has(r, "OUTSIDE the zone"));
    SC_EXPECT(Has(r, "out of reach: flight time ran out"));
}

SC_TEST("engagement: swapped-in terrain masks a target; moving the unit moves the picture") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(Engage(Target(24000, 300), kAcquireAndFire), kData, def, console));
    ScenarioRunner runner(std::move(def), std::move(console));
    // A 1 km-high ridge running east-west, 5 km north of the unit (grid covers 30 x 30 km).
    std::vector<double> heights(61 * 61, 0.0);
    for (int col = 0; col < 61; ++col) heights[static_cast<std::size_t>(40 * 61 + col)] = 1000.0;
    const GridTerrain ridge(-15000.0, -15000.0, 500.0, 61, 61, heights);
    runner.SetTerrain(&ridge);
    bool seen = false;
    for (int i = 0; i < 2800; ++i) {  // 280 s: powered, searching, target (launched at 230 s) behind the ridge
        runner.Step(0.1);
        seen = seen || !runner.Engagement().SearchPicture().empty();
    }
    SC_EXPECT(!seen);
    const Vec3 moved{0.0, 3000.0, 3.0};
    runner.SetUnitPosition(moved);
    const Vec3 target_pos = runner.GetWorld().AircraftList().front().position;
    SC_EXPECT_NEAR(runner.TargetRange("t1"), (target_pos - moved).Length(), 1e-6);
    runner.SetTerrain(nullptr);  // back to flat ground: the target comes into view
    for (int i = 0; i < 20; ++i) runner.Step(0.1);
    SC_EXPECT(!runner.Engagement().SearchPicture().empty());
}

SC_TEST("engagement: an evading target breaks away and the manoeuvre lowers the chance") {
    const Run r = Play(Engage(Target(24000, 1000, R"(, "evade_on_launch": true, "evade_turn_rate_deg_s": 12)"),
                              kAcquireAndFire));
    SC_EXPECT(Has(r, "breaks away onto a beam heading"));
    SC_EXPECT(Has(r, "manoeuvre 0.52 (12 deg/s)"));
}

SC_TEST("engagement: after a launch ПУСК ГОТОВ goes out until the next missile is prepared") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(Engage(Target(24000, 1000), kAcquireAndFire), kData, def, console));
    ScenarioRunner runner(std::move(def), std::move(console));
    while (!runner.Finished() && runner.Launches() == 0) runner.Step(0.1);
    SC_EXPECT(runner.Launches() == 1);
    runner.Step(0.1);
    const Equipment& eq = runner.Console().equipment;
    SC_EXPECT(!eq.IsReady("launch_armed") && eq.State("missile_prep") == SubsystemState::Starting);
    // Lit again after the 13 s preparation (data/procedures/osa_ak.json), while the missile is still flying.
    const double t0 = runner.Time();
    while (!runner.Finished() && !eq.IsReady("launch_armed") && runner.Time() < t0 + 60.0) runner.Step(0.1);
    SC_EXPECT_NEAR(runner.Time() - t0, 13.0, 0.3);
}
