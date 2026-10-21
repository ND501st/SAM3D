#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
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
    // The EM layer's auto-capture rule is data: 22 hits in the last 40 looks for the Osa.
    SC_EXPECT(t.em_capture_looks == 40 && t.em_capture_hits == 22);
    SC_EXPECT(!LoadEngagementTuning(broken("\"em_capture_hits\": 22", "\"em_capture_hits\": 41"), t, &error) &&
              error.find("track.em_capture_hits") != std::string::npos);
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
    // The NPC deputy waits until the target fills the crosshair (within range); a player in his seat launches as soon as
    // the TV holds it and a missile is ready, about 17 km out. Against a slow target (50 m/s) the missile would meet it
    // only after 30 s: beyond its flight time.
    const std::string slow = R"({ "id": "t1", "name": "T", "class": "drone", "start": { "east_m": 1500, "north_m": 19000, "up_m": 800 },
        "waypoints": [ { "east_m": 1500, "north_m": -20000, "up_m": 800 } ], "speed_mps": 50, "appear_at_s": 230 })";
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(Engage(slow, emcon), kData, def, console, &error));
    ScenarioRunner runner(std::move(def), std::move(console));
    while (!runner.Finished() && runner.Time() < 700.0) {
        runner.Step(0.1);
        const Equipment& eq = runner.Console().equipment;
        if (runner.Launches() == 0 && runner.Engagement().Method() == TrackMethod::Tv && eq.IsReady("missile_prep")) {
            if (runner.Console().panel.Get("fo.key") == 1) runner.PlayerSet("fo.key", 2);
            if (eq.IsReady("launch_armed")) runner.PlayerSet("fo.launch", 1);
        }
    }
    Run r;
    r.debrief = runner.Debrief();
    SC_EXPECT(Has(r, "OUTSIDE the zone"));
    SC_EXPECT(Has(r, "out of reach: flight time ran out"));
}

SC_TEST("engagement: under EMCON the NPC deputy launches only once the target fills the crosshair (within range)") {
    const std::string emcon = R"(,
        { "id": "tv", "when": { "target": "t1", "target_within_m": 30000, "conditions": [ { "subsystem": "apu_free", "is": "ready" } ] },
          "do": [ { "order": "emcon_tv_azimuth", "crew": "Senior operator" },
                  { "order": "emcon_tv_engagement", "crew": "Deputy" } ] })";
    const Run r = Play(Engage(Target(19000, 800), emcon));
    SC_EXPECT(Has(r, "TV track, in the zone"));
    SC_EXPECT(r.debrief.find("OUTSIDE the zone") == std::string::npos);
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

SC_TEST("engagement: a pair's second missile waits the launch interval; the next pair waits for its preparation") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(Engage(Target(24000, 1000), kAcquireAndFire), kData, def, console));
    ScenarioRunner runner(std::move(def), std::move(console));
    while (!runner.Finished() && runner.Launches() == 0) runner.Step(0.1);
    SC_EXPECT(runner.Launches() == 1);
    runner.Step(0.1);
    const Equipment& eq = runner.Console().equipment;
    // The pair's second missile (channel II) is still prepared: only the 4 s between launches.
    SC_EXPECT(eq.IsReady("missile_prep") && eq.GetInput("launch_hold") && !eq.IsReady("launch_armed"));
    double t0 = runner.Time();
    while (!runner.Finished() && !eq.IsReady("launch_armed") && runner.Time() < t0 + 60.0) runner.Step(0.1);
    SC_EXPECT_NEAR(runner.Time() - t0, 3.9, 0.3);
    runner.Console().panel.Set("fo.launch", 1);  // the second missile, by hand
    runner.Step(0.1);
    SC_EXPECT(runner.Launches() == 2);
    // The pair is spent: the next one is selected and prepared again (13 s, data/procedures/osa_ak.json).
    SC_EXPECT(eq.State("missile_prep") == SubsystemState::Starting && !eq.IsReady("launch_armed"));
    t0 = runner.Time();
    while (!runner.Finished() && !eq.IsReady("launch_armed") && runner.Time() < t0 + 60.0) runner.Step(0.1);
    // Ready again once the 13 s preparation is done and a channel is free: both are guiding their missiles, so
    // the third waits for the first shot to end as well.
    SC_EXPECT(runner.Time() - t0 >= 12.9);
    SC_EXPECT(eq.IsReady("missile_prep") && runner.Engagement().ShotDone(1));
    const std::string text = runner.Debrief();
    SC_EXPECT(text.find("missile 1 from 3-I, channel I") != std::string::npos);
    SC_EXPECT(text.find("missile 2 from 3-II, channel II") != std::string::npos);
}

namespace {

// Runs an Osa acquisition until target designation (ЦУ) is engaged; false if it never is.
bool RunToDesignation(ScenarioRunner& runner) {
    while (!runner.Finished() && !runner.Console().equipment.IsReady("auto_acquisition")) runner.Step(0.1);
    return runner.Console().equipment.IsReady("auto_acquisition");
}

std::unique_ptr<ScenarioRunner> Acquisition(double north_m, double up_m) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(Engage(Target(north_m, up_m), kAcquireAndFire), kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

}  // namespace

SC_TEST("engagement: in ЦУ the tracking antenna scans the search beam and captures as it crosses the target") {
    std::unique_ptr<ScenarioRunner> runner = Acquisition(24000, 1000);
    SC_EXPECT(runner != nullptr && RunToDesignation(*runner));
    if (runner == nullptr) return;
    const ConsoleData& c = runner->Console();
    const EngagementModel& e = runner->Engagement();
    // Once the senior operator has the automatic programme (АВТ. I–III) on and the azimuth line near the echo, the
    // antenna searches beam I (-1 to 5 deg), which holds the target (about 2-3 deg up).
    double low = 1e9;
    double high = -1e9;
    int samples = 0;
    bool wheel_follows = true;
    bool was_auto = false;  // the model sees a selector change on the step after the crew makes it
    while (!runner->Finished() && e.TrackedTarget().empty()) {
        runner->Step(0.1);
        const bool sample = was_auto && e.TrackedTarget().empty() &&
                            std::fabs(c.panel.GetValue(c.track.az_wheel) - 3.6) <= 5.0;
        was_auto = c.panel.Get("soc.beam") == 3;
        if (!sample) continue;
        ++samples;
        SC_EXPECT(e.ElevationBeam() == 0);
        low = std::fmin(low, e.AntennaElevationDeg());
        high = std::fmax(high, e.AntennaElevationDeg());
        wheel_follows = wheel_follows && std::fabs(c.panel.GetValue(c.track.el_wheel) - e.AntennaElevationDeg()) < 1e-9;
    }
    SC_EXPECT(samples > 0);
    SC_EXPECT(wheel_follows);  // the antenna drives the elevation wheel while it scans
    SC_EXPECT(low >= -1.0 - 1e-9 && high <= 5.0 + 1e-9);
    SC_EXPECT(e.TrackedTarget() == "t1" && e.Method() == TrackMethod::Radar);
    // At the capture the antenna was on the target: it now follows it.
    SC_EXPECT_NEAR(e.AntennaElevationDeg(), c.panel.GetValue(c.track.el_wheel), 1e-9);
}

SC_TEST("engagement: searching the wrong beam in ЦУ never captures; the right beam does") {
    std::unique_ptr<ScenarioRunner> runner = Acquisition(24000, 1000);
    SC_EXPECT(runner != nullptr && RunToDesignation(*runner));
    if (runner == nullptr) return;
    ConsoleData& c = runner->Console();
    const EngagementModel& e = runner->Engagement();
    // Beam III (6 to 30 deg) is above a target 2-3 deg up: the scan never crosses it.
    for (int i = 0; i < 400 && !runner->Finished(); ++i) {
        c.panel.Set("soc.beam", 2);
        runner->Step(0.1);
    }
    SC_EXPECT(e.TrackedTarget().empty());
    SC_EXPECT(e.ElevationBeam() == 2 && e.AntennaElevationDeg() >= 6.0 - 1e-9);
    c.panel.Set("soc.beam", 0);  // beam I
    for (int i = 0; i < 100 && e.TrackedTarget().empty(); ++i) runner->Step(0.1);
    SC_EXPECT(e.TrackedTarget() == "t1");
}

SC_TEST("engagement: the console's elevation search is checked when it loads") {
    std::ifstream f(kData + "/procedures/osa_ak.json", std::ios::binary);
    std::stringstream text;
    text << f.rdbuf();
    const std::string osa = text.str();
    ConsoleData console;
    SC_EXPECT(LoadConsole(osa, console));
    SC_EXPECT(console.track.beams_deg.size() == 3 && console.track.beam_width_deg > 0.0);
    auto Error = [&](const std::string& from, const std::string& to) {
        std::string t = osa;
        const std::size_t at = t.find(from);
        if (at == std::string::npos) return std::string("pattern not found: ") + from;
        t.replace(at, from.size(), to);
        ConsoleData c;
        std::string error;
        return LoadConsole(t, c, &error) ? std::string("loaded") : error;
    };
    const auto Has = [](const std::string& error, const std::string& want) {
        if (error.find(want) == std::string::npos) std::printf("    got: %s\n", error.c_str());
        return error.find(want) != std::string::npos;
    };
    SC_EXPECT(Has(Error(R"("beam_by_position": [0, 1, 2, -1, -1])", R"("beam_by_position": [0, 1])"),
                  "elevation_search.beam_by_position: needs one entry per position"));
    SC_EXPECT(Has(Error(R"("beam_by_position": [0, 1, 2, -1, -1])", R"("beam_by_position": [0, 1, 3, -1, -1])"),
                  "beam_by_position[2]"));
    SC_EXPECT(Has(Error(R"([ [-1, 5], [3, 9], [6, 30] ])", R"([ [5, -1], [3, 9], [6, 30] ])"), "beams_deg[0]"));
    SC_EXPECT(Has(Error(R"("beam_width_deg": 1.0)", R"("beam_width_deg": 0)"), "beam_width_deg"));
}

namespace {

// An Osa scenario with the hull facing `heading_deg`, the given target and extra triggers.
std::unique_ptr<ScenarioRunner> Heading(double heading_deg, const std::string& target, const std::string& triggers) {
    std::string text = Engage(target, triggers);
    const std::string at = R"("up_m": 3 }, "inputs")";
    const std::size_t i = text.find(at);
    if (i == std::string::npos) return nullptr;
    text.replace(i, at.size(), R"("up_m": 3 }, "heading_deg": )" + std::to_string(heading_deg) + R"(, "inputs")");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

}  // namespace

SC_TEST("engagement: an unoriented vehicle sees the picture turned by its heading; orienting it fixes the frame") {
    const char* orient = R"(,
        { "id": "or", "when": { "conditions": [ { "subsystem": "soc_transmitter", "is": "ready" } ] },
          "do": [ { "order": "orient_vehicle", "crew": "Deputy" } ] })";
    std::unique_ptr<ScenarioRunner> runner = Heading(40.0, Target(30000, 1000), orient);
    SC_EXPECT(runner != nullptr);
    if (runner == nullptr) return;
    const EngagementModel& e = runner->Engagement();
    ConsoleData& c = runner->Console();
    // Before orientation the gyro takes the hull to face north: the crew's frame is 40 deg off true north.
    runner->Step(0.1);
    SC_EXPECT_NEAR(e.IndicatedToTrueDeg(10.0), 50.0, 1e-9);
    while (!runner->Finished() && !c.equipment.GetInput("heading_synced") && runner->Time() < 400.0) runner->Step(0.1);
    SC_EXPECT(c.equipment.GetInput("heading_synced"));
    // The deputy finishes the drill (АК, РЕЛЕ); the frame is now true north.
    for (int i = 0; i < 100 && c.panel.Get("orient.relay") != 0; ++i) runner->Step(0.1);
    SC_EXPECT(c.panel.Get("orient.ak_zk") == 0 && c.panel.Get("orient.relay") == 0);
    // The crew chief's Qc came from his compass work (its error is 0.2 deg, one sigma): the frame is true north to that.
    SC_EXPECT(runner->Debrief().find("Qc by compass") != std::string::npos);
    SC_EXPECT_NEAR(e.GyroHeadingDeg(), 40.0, 0.6);
    SC_EXPECT_NEAR(e.IndicatedToTrueDeg(10.0), 10.0, 0.6);
    SC_EXPECT_NEAR(e.GyroHeadingDeg(), e.CommandedQcDeg(), 0.06);
    SC_EXPECT(e.Oriented() && c.equipment.GetInput("vehicle_oriented"));
    // The hull turns: the gyro follows, so the frame stays where it was set.
    double error = e.GyroHeadingDeg() - 40.0;
    if (error > 180.0) error -= 360.0;
    if (error < -180.0) error += 360.0;
    runner->SetUnitHeading(70.0);
    runner->Step(0.1);
    SC_EXPECT_NEAR(e.GyroHeadingDeg(), 70.0 + error, 0.01);
    // Power off: orientation lost.
    c.panel.Set("pwr.gta", 0);
    c.panel.Set("pwr.pto", 0);  // both sources: the power take-off generator feeds it too
    for (int i = 0; i < 600 && c.equipment.IsReady("power"); ++i) runner->Step(0.1);
    runner->Step(0.1);
    SC_EXPECT(!c.equipment.IsReady("power"));
    SC_EXPECT_NEAR(e.GyroHeadingDeg(), 0.0, 1e-9);
    SC_EXPECT(!c.equipment.GetInput("heading_synced"));
}

SC_TEST("engagement: a friendly target replies to ЗАПРОС; firing at it is a crew mistake") {
    const Run friend_run = Play(Engage(Target(24000, 1000, R"(, "iff": "friend")"), kAcquireAndFire));
    SC_EXPECT(Has(friend_run, "IFF reply: friend"));
    SC_EXPECT(Has(friend_run, "fired at a friendly aircraft"));
    const Run hostile = Play(Engage(Target(24000, 1000), kAcquireAndFire));
    SC_EXPECT(hostile.debrief.find("IFF reply") == std::string::npos);
    SC_EXPECT(hostile.debrief.find("friendly aircraft") == std::string::npos);
}

SC_TEST("engagement: in the gated IFF mode only aircraft on the search scope reply") {
    std::unique_ptr<ScenarioRunner> runner = Heading(0.0, Target(30000, 1000, R"(, "iff": "friend")"), "");
    SC_EXPECT(runner != nullptr);
    if (runner == nullptr) return;
    ConsoleData& c = runner->Console();
    const EngagementModel& e = runner->Engagement();
    while (!runner->Finished() && runner->Time() < 260.0) runner->Step(0.1);  // the friend is up, about 26 km out
    SC_EXPECT(c.equipment.IsReady("soc_transmitter"));
    auto Query = [&](int gate, int scale) {
        c.panel.Set("iff.gate", gate);
        c.panel.Set("soc.range", scale);
        c.panel.Set("iff.query", 1);
        runner->Step(0.1);
        return e.IffReplies().size();
    };
    SC_EXPECT(Query(0, 0) == 1);  // open reception: replies though off the 0-15 km scale
    SC_EXPECT(c.equipment.GetInput("iff_interrogating"));
    SC_EXPECT(Query(1, 0) == 0);  // КЛАПАН: no echo on the scale, no reply
    SC_EXPECT(Query(1, 1) == 1);  // on the 0-35 km scale it shows, so it replies
    for (int i = 0; i < 15; ++i) runner->Step(0.1);  // the interrogation lasts about 1 s
    SC_EXPECT(e.IffReplies().empty() && !c.equipment.GetInput("iff_interrogating"));
}

SC_TEST("engagement: the loaders check a target's iff and the console's orientation") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(!LoadScenario(Engage(Target(24000, 1000, R"(, "iff": "maybe")")), kData, def, console, &error));
    SC_EXPECT(error.find("targets[0].iff: must be friend or none") != std::string::npos);
    std::ifstream f(kData + "/procedures/osa_ak.json", std::ios::binary);
    std::stringstream text;
    text << f.rdbuf();
    std::string osa = text.str();
    const std::string from = R"("set_position": 1)";
    osa.replace(osa.find(from), from.size(), R"("set_position": 2)");
    ConsoleData c;
    SC_EXPECT(!LoadConsole(osa, c, &error));
    SC_EXPECT(error.find("sensors.orientation.set_position") != std::string::npos);
}

// ---- Roadmap 6.7: firing zones by approach and speed, guidance methods, and hit chances against low targets.

namespace {

EngagementTuning OsaTuning() {
    EngagementTuning t;
    std::string error;
    if (!LoadEngagementTuningFile(kData + "/gameplay/osa_ak.json", t, &error)) std::printf("    tuning: %s\n", error.c_str());
    return t;
}

}  // namespace

SC_TEST("zones: the Osa's zones, methods and low-altitude points load; bad ones are refused with paths") {
    const EngagementTuning t = OsaTuning();
    SC_EXPECT(t.has_zones && t.has_methods && t.head_on.size() == 2);
    // The fire-control computer's limits (voenizdat_osa_1991 p124-128) and the crews' Table 7.1 (p257).
    SC_EXPECT_NEAR(t.pursuit_max_range_m, 8700.0, 1e-9);
    SC_EXPECT_NEAR(t.pursuit_range_loss_s, 8.0, 1e-9);
    SC_EXPECT_NEAR(t.max_slant_parameter_m, 8700.0, 1e-9);
    SC_EXPECT_NEAR(t.slant_parameter_loss_s, 8.0, 1e-9);
    SC_EXPECT(t.has_launch_ranges && t.launch_range_slow.size() == 4 && t.launch_range_fast.size() == 4);
    SC_EXPECT_NEAR(t.launch_split_speed_mps, 300.0, 1e-9);
    SC_EXPECT_NEAR(t.low_target_elevation_deg, 1.8, 1e-9);
    SC_EXPECT(t.low_altitude.count("default") == 1 && t.low_altitude.count("helicopter") == 1);

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
    EngagementTuning out;
    std::string error;
    SC_EXPECT(!LoadEngagementTuning(broken("{ \"max_speed_mps\": 500,", "{ \"max_speed_mps\": 200,"), out, &error) &&
              error.find("zones.head_on[1]: speed bands must be in ascending order") != std::string::npos);
    SC_EXPECT(!LoadEngagementTuning(broken("[50, 0.70]", "[50, 1.70]"), out, &error) &&
              error.find("outcome.low_altitude.default") != std::string::npos);
    SC_EXPECT(!LoadEngagementTuning(broken("[10, 0.45], [50, 0.70]", "[10, 0.45], [5, 0.70]"), out, &error) &&
              error.find("outcome.low_altitude.default") != std::string::npos);
    SC_EXPECT(!LoadEngagementTuning(broken("\"gorka_max_agl_m\": 100", "\"gorka_max_agl\": 100"), out, &error) &&
              error.find("methods.gorka_max_agl: unknown key") != std::string::npos);
    SC_EXPECT(!LoadEngagementTuning(broken("[100, 11000], [150, 13000]", "[150, 11000], [100, 13000]"), out, &error) &&
              error.find("zones.launch_range_m.slow") != std::string::npos);
    SC_EXPECT(!LoadEngagementTuning(broken("\"range_loss_s\": 8", "\"range_loss_s\": -8"), out, &error) &&
              error.find("zones.pursuit.range_loss_s") != std::string::npos);
}

SC_TEST("zones: head-on, pursuit and optical zones follow the NVA's limits") {
    const EngagementTuning t = OsaTuning();
    using Z = EngagementTuning::ZoneKind;
    // Head-on: 25-5000 m up to 300 m/s, 100-5000 m up to 500 m/s, 1.5-10.3 km.
    SC_EXPECT(t.ZoneFor(8000.0, 30.0, 250.0, 250.0) == Z::HeadOn);
    SC_EXPECT(t.ZoneFor(8000.0, 20.0, 250.0, 250.0) == Z::None);    // below 25 m
    SC_EXPECT(t.ZoneFor(8000.0, 50.0, 400.0, 400.0) == Z::None);    // fast and below 100 m
    SC_EXPECT(t.ZoneFor(8000.0, 150.0, 400.0, 400.0) == Z::HeadOn);
    SC_EXPECT(t.ZoneFor(8000.0, 150.0, 600.0, 600.0) == Z::None);   // faster than 500 m/s
    SC_EXPECT(t.ZoneFor(11000.0, 1000.0, 200.0, 200.0) == Z::None);  // beyond 10.3 km
    SC_EXPECT(t.ZoneFor(1000.0, 1000.0, 200.0, 200.0) == Z::None);   // inside 1.5 km
    // Pursuit: a receding target to 8700 - 8 V m (7.1 km at 200 m/s), 100-5000 m, up to 300 m/s.
    SC_EXPECT(t.ZoneFor(7000.0, 500.0, 200.0, -200.0) == Z::Pursuit);
    SC_EXPECT(t.ZoneFor(7200.0, 500.0, 200.0, -200.0) == Z::None);
    SC_EXPECT(t.ZoneFor(6000.0, 500.0, 300.0, -300.0) == Z::Pursuit);  // 6.3 km at 300 m/s
    SC_EXPECT(t.ZoneFor(6400.0, 500.0, 300.0, -300.0) == Z::None);
    SC_EXPECT(t.ZoneFor(6000.0, 500.0, 400.0, -400.0) == Z::None);
    SC_EXPECT(t.ZoneFor(6000.0, 50.0, 200.0, -200.0) == Z::None);    // low and receding
    // Optical: slow or hovering, 2-6.5 km, 10-25 m.
    SC_EXPECT(t.ZoneFor(3000.0, 15.0, 0.0, 0.0) == Z::Optical);
    SC_EXPECT(t.ZoneFor(3000.0, 15.0, 90.0, 90.0) == Z::Optical);
    SC_EXPECT(t.ZoneFor(7000.0, 15.0, 0.0, 0.0) == Z::None);
    SC_EXPECT(t.ZoneFor(3000.0, 40.0, 0.0, 0.0) == Z::None);
    SC_EXPECT(t.ZoneFor(3000.0, 15.0, 150.0, 150.0) == Z::None);     // too fast for the optical zone, too low for head-on
    // Head-on, the slant parameter (crossing parameter through the height) at most 8700 - 8 V m (eq. 2.18).
    SC_EXPECT(t.ZoneFor(8000.0, 3000.0, 250.0, 100.0, 5500.0) == Z::HeadOn);   // hypot 6265 <= 6700
    SC_EXPECT(t.ZoneFor(8000.0, 4000.0, 250.0, 100.0, 5500.0) == Z::None);     // hypot 6801 > 6700
    // Without the zones block the one box is every target's zone.
    EngagementTuning plain = t;
    plain.has_zones = false;
    SC_EXPECT(plain.ZoneFor(8000.0, 30.0, 600.0, 600.0) == Z::HeadOn);
    SC_EXPECT(plain.ZoneFor(6000.0, 500.0, 200.0, -200.0) == Z::HeadOn);
}

SC_TEST("zones: the meeting point: the target straight on, the missile along the line of sight at 490 m/s") {
    const EngagementTuning t = OsaTuning();
    SC_EXPECT_NEAR(t.missile_average_speed_mps, 490.0, 1e-9);  // 10.3 km in 21 s (voenizdat_osa_1991 p129)
    // Head-on at 300 m/s from 15.8 km: 15800 - 300 t = 490 t, t = 20 s, met at 9.8 km.
    EngagementTuning::Meeting m = t.MeetingFor(Vec3{0.0, 15800.0, 0.0}, Vec3{0.0, -300.0, 0.0}, 1000.0);
    SC_EXPECT(m.ok);
    SC_EXPECT_NEAR(m.time_s, 20.0, 1e-9);
    SC_EXPECT_NEAR(m.range_m, 9800.0, 1e-6);
    SC_EXPECT_NEAR(m.height_m, 1000.0, 1e-9);
    SC_EXPECT(m.closing_mps > 0.0);
    // Receding at 200 m/s from 4 km: 4000 + 200 t = 490 t, t = 13.793 s, met at 6758.6 m, still receding.
    m = t.MeetingFor(Vec3{0.0, 4000.0, 0.0}, Vec3{0.0, 200.0, 0.0}, 500.0);
    SC_EXPECT(m.ok && m.closing_mps < 0.0);
    SC_EXPECT_NEAR(m.time_s, 4000.0 / 290.0, 1e-9);
    SC_EXPECT_NEAR(m.range_m, 490.0 * 4000.0 / 290.0, 1e-6);
    // A climbing target is met higher, by its climb rate times the flight time (4900^2 + (100 t)^2 = (490 t)^2: 10.2 s).
    m = t.MeetingFor(Vec3{0.0, 4900.0, 0.0}, Vec3{0.0, 0.0, 100.0}, 1000.0);
    SC_EXPECT(m.ok);
    SC_EXPECT_NEAR(m.height_m, 1000.0 + 100.0 * m.time_s, 1e-9);
    // Receding faster than the missile: never met.
    SC_EXPECT(!t.MeetingFor(Vec3{0.0, 4000.0, 0.0}, Vec3{0.0, 600.0, 0.0}, 500.0).ok);
}

SC_TEST("zones: ЦЕЛЬ В ЗОНЕ is the meeting point's zone, within the crews' far edge of the launch zone (Table 7.1)") {
    const EngagementTuning t = OsaTuning();
    using Z = EngagementTuning::ZoneKind;
    const Vec3 inbound300{0.0, -300.0, 0.0};
    // At 1000 m, up to 300 m/s, the far edge of the launch zone is 16 km (voenizdat_osa_1991 p257): from 15.8 km the
    // missile meets the target at 9.8 km; from 16.1 km it would meet it at 10.0 km, but that is past the crews' edge.
    SC_EXPECT_NEAR(t.LaunchRangeFor(1000.0, 300.0), 16000.0, 1e-9);
    SC_EXPECT(t.LaunchZoneFor(Vec3{0.0, 15800.0, 0.0}, inbound300, 1000.0) == Z::HeadOn);
    SC_EXPECT(t.LaunchZoneFor(Vec3{0.0, 16100.0, 0.0}, inbound300, 1000.0) == Z::None);
    // Without the table the edge is where the meeting reaches 10.3 km: 10300 x 790 / 490 = 16606 m.
    EngagementTuning no_table = t;
    no_table.has_launch_ranges = false;
    SC_EXPECT(no_table.LaunchZoneFor(Vec3{0.0, 16500.0, 0.0}, inbound300, 1000.0) == Z::HeadOn);
    SC_EXPECT(no_table.LaunchZoneFor(Vec3{0.0, 16700.0, 0.0}, inbound300, 1000.0) == Z::None);
    // Inside 10.3 km the meeting point can be too close: from 2.5 km at 300 m/s it is met at 1.55 km; from 2.4 km at
    // 1.49 km, inside the near edge.
    SC_EXPECT(t.LaunchZoneFor(Vec3{0.0, 2500.0, 0.0}, inbound300, 1000.0) == Z::HeadOn);
    SC_EXPECT(t.LaunchZoneFor(Vec3{0.0, 2400.0, 0.0}, inbound300, 1000.0) == Z::None);
    // A low target: 8 km at 25-50 m, 9.5 km at 75 m (halfway to 11 km at 100 m); faster than 300 m/s, 9 km.
    SC_EXPECT_NEAR(t.LaunchRangeFor(40.0, 250.0), 8000.0, 1e-9);
    SC_EXPECT_NEAR(t.LaunchRangeFor(75.0, 250.0), 9500.0, 1e-9);
    SC_EXPECT_NEAR(t.LaunchRangeFor(40.0, 450.0), 9000.0, 1e-9);
    SC_EXPECT_NEAR(t.LaunchRangeFor(3000.0, 450.0), 15000.0, 1e-9);
    const Vec3 inbound250{0.0, -250.0, 0.0};
    SC_EXPECT(t.LaunchZoneFor(Vec3{0.0, 7900.0, 0.0}, inbound250, 40.0) == Z::HeadOn);
    SC_EXPECT(t.LaunchZoneFor(Vec3{0.0, 8100.0, 0.0}, inbound250, 40.0) == Z::None);
    // Pursuit: receding at 200 m/s from 4 km the meeting point is 6.76 km out (in); from 5 km, 8.45 km (beyond 7.1 km).
    const Vec3 outbound200{0.0, 200.0, 0.0};
    SC_EXPECT(t.LaunchZoneFor(Vec3{0.0, 4000.0, 0.0}, outbound200, 500.0) == Z::Pursuit);
    SC_EXPECT(t.LaunchZoneFor(Vec3{0.0, 5000.0, 0.0}, outbound200, 500.0) == Z::None);
    // A meeting later than the missile's flight time is out of reach.
    EngagementTuning short_flight = t;
    short_flight.missile_max_flight_time_s = 15.0;
    SC_EXPECT(short_flight.LaunchZoneFor(Vec3{0.0, 15800.0, 0.0}, inbound300, 1000.0) == Z::None);  // 20 s
}

SC_TEST("zones: ВНИМАНИЕ lights for an approaching target within 10.3 km + (21 s + 15 s) x its speed (eq. 2.24)") {
    const EngagementTuning t = OsaTuning();
    // 10300 + (10300 / 490 + 15) x 300 = 21106 m.
    const Vec3 inbound300{0.0, -300.0, 0.0};
    SC_EXPECT(t.AttentionFor(Vec3{0.0, 21000.0, 0.0}, inbound300, 1000.0));
    SC_EXPECT(!t.AttentionFor(Vec3{0.0, 21200.0, 0.0}, inbound300, 1000.0));
    SC_EXPECT(!t.AttentionFor(Vec3{0.0, 15000.0, 0.0}, Vec3{0.0, 300.0, 0.0}, 1000.0));  // receding beyond the zone
    SC_EXPECT(t.AttentionFor(Vec3{0.0, 4000.0, 0.0}, Vec3{0.0, 200.0, 0.0}, 500.0));     // receding, in the pursuit zone
}

SC_TEST("engagement: the crew launches before the target is within 10.3 km, and the missile meets it inside") {
    const Run r = Play(Engage(Target(24000, 1000), kAcquireAndFire));
    const std::size_t at = r.debrief.find("guided at t1, ");
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    const double launch_km = std::atof(r.debrief.c_str() + at + 14);
    const std::size_t meet = r.debrief.find("meeting at ", at);
    SC_EXPECT(meet != std::string::npos);
    if (meet == std::string::npos) return;
    const double meet_km = std::atof(r.debrief.c_str() + meet + 11);
    std::printf("    launched at %.1f km, met at %.1f km\n", launch_km, meet_km);
    SC_EXPECT(launch_km > 10.3 && meet_km <= 10.3 + 0.05);
}

SC_TEST("zones: the guidance method follows height, elevation and speed") {
    const EngagementTuning t = OsaTuning();
    using M = EngagementTuning::GuidanceMethod;
    SC_EXPECT(t.MethodFor(1000.0, 5.0, 200.0) == M::ThreePoint);
    SC_EXPECT(t.MethodFor(60.0, 0.5, 250.0) == M::ZoomClimb);    // fast and low, under 00-30 elevation
    SC_EXPECT(t.MethodFor(60.0, 3.0, 250.0) == M::ThreePoint);   // low but up in elevation
    SC_EXPECT(t.MethodFor(400.0, 0.5, 250.0) == M::ThreePoint);  // low in elevation but not low
    SC_EXPECT(t.MethodFor(15.0, 0.3, 0.0) == M::Optical);        // hovering near the ground
    SC_EXPECT(t.MethodFor(15.0, 0.3, 90.0) == M::Optical);
    SC_EXPECT(t.MethodFor(15.0, 0.3, 200.0) == M::ZoomClimb);
    SC_EXPECT(t.MethodFor(1000.0, 5.0, 200.0, true) == M::Jammer);  // ПОМЕХА on: method Н, whatever the height
    SC_EXPECT(t.MethodFor(15.0, 0.3, 0.0, true) == M::Optical);  // a hovering helicopter: Ф, ПОМЕХА isn't used there
    SC_EXPECT(t.MethodFor(60.0, 0.5, 250.0, true) == M::Jammer);  // otherwise it replaces Горка
    SC_EXPECT(std::string(EngagementTuning::ToString(M::Jammer)) == "N");
}

SC_TEST("zones: hit chances against low targets sit in the published kill-probability ranges") {
    const EngagementTuning t = OsaTuning();
    auto chance = [&](const char* cls, double agl_m) { return t.HitChanceFor(cls) * t.LowAltitudeFactor(cls, agl_m); };
    // flak11: manoeuvring targets 0.55-0.85 (taken as the high-level chance).
    for (const char* cls : {"fighter", "drone", "bomber", "helicopter"}) {
        SC_EXPECT(chance(cls, 2000.0) >= 0.55 && chance(cls, 2000.0) <= 0.85);
    }
    // korovin_tiv_2010 (original 9K33 against an F-4C): 0.35-0.4 at 50 m, 0.42-0.85 above 100 m.
    SC_EXPECT(chance("fighter", 50.0) >= 0.35 && chance("fighter", 50.0) <= 0.40);
    SC_EXPECT(chance("fighter", 100.0) >= 0.42 && chance("fighter", 100.0) <= 0.85);
    SC_EXPECT(chance("fighter", 300.0) >= 0.42 && chance("fighter", 300.0) <= 0.85);
    // flak11 at 10 m: strike aircraft 0.12-0.55, helicopters 0.07-0.12, cruise missiles 0.12-0.38.
    SC_EXPECT(chance("bomber", 10.0) >= 0.12 && chance("bomber", 10.0) <= 0.55);
    SC_EXPECT(chance("helicopter", 10.0) >= 0.07 && chance("helicopter", 10.0) <= 0.12);
    SC_EXPECT(chance("cruise_missile", 10.0) >= 0.12 && chance("cruise_missile", 10.0) <= 0.38);
    // It rises with height and is flat outside the points.
    SC_EXPECT(chance("fighter", 5.0) <= chance("fighter", 30.0) && chance("fighter", 30.0) < chance("fighter", 70.0));
    SC_EXPECT_NEAR(t.LowAltitudeFactor("fighter", 3.0), t.LowAltitudeFactor("fighter", 10.0), 1e-12);
    SC_EXPECT_NEAR(t.LowAltitudeFactor("fighter", 5000.0), 1.0, 1e-12);
    SC_EXPECT_NEAR(t.LowAltitudeFactor("fighter", 30.0), 0.45 + 0.25 * 20.0 / 40.0, 1e-12);  // halfway from 10 m to 50 m
}

SC_TEST("zones: a low target's shot is printed with its zone, method and height factor") {
    const std::string emcon = R"(,
        { "id": "tv", "when": { "target": "t1", "target_within_m": 30000, "conditions": [ { "subsystem": "apu_free", "is": "ready" } ] },
          "do": [ { "order": "emcon_tv_azimuth", "crew": "Senior operator" },
                  { "order": "emcon_tv_engagement", "crew": "Deputy" } ] })";
    // 60 m up in the flat frame at 9 km: about 70 m above the ground, 0.4 degrees up: factor about 0.76. The TV
    // drill switches ПОМЕХА on (the range is tracked by hand), so the commands come from method Н, not Горка.
    const Run r = Play(Engage(Target(9000, 60), emcon));
    SC_EXPECT(Has(r, "head-on zone, N method"));
    SC_EXPECT(Has(r, "x height 0.7"));
    // A high target is the ordinary method at full height factor.
    const Run high = Play(Engage(Target(24000, 1000), kAcquireAndFire));
    SC_EXPECT(Has(high, "head-on zone, T/T method"));
    SC_EXPECT(Has(high, "x height 1.00"));
}

// ---- Roadmap 6.6: manual TV tracking.

namespace {

const char* kTvOnly = R"(,
        { "id": "tv", "when": { "target": "t1", "target_within_m": 30000, "conditions": [ { "subsystem": "apu_free", "is": "ready" } ] },
          "do": [ { "order": "emcon_tv_azimuth", "crew": "Senior operator" },
                  { "order": "emcon_tv_engagement", "crew": "Deputy" } ] })";

}  // namespace

SC_TEST("tv: the manual-tracking group and the lens field's shape load; bad ones are refused with paths") {
    EngagementTuning t = OsaTuning();
    SC_EXPECT(t.has_tv_manual);
    SC_EXPECT_NEAR(t.tv_reticle_deg, 0.18, 1e-12);
    SC_EXPECT_NEAR(t.tv_memory_s, 5.0, 1e-12);
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", console, &error));
    SC_EXPECT_NEAR(console.track.fov_height_ratio, 0.75, 1e-12);

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
    EngagementTuning out;
    SC_EXPECT(!LoadEngagementTuning(broken("\"reticle_deg\": 0.18", "\"reticle\": 0.18"), out, &error) &&
              error.find("tv.reticle: unknown key") != std::string::npos);
    SC_EXPECT(!LoadEngagementTuning(broken("\"error_factor_min\": 0.4", "\"error_factor_min\": 1.4"), out, &error) &&
              error.find("tv.error_factor_min") != std::string::npos);
}

SC_TEST("tv: without the tv group the servo still drives the wheels onto the target itself") {
    std::string text;
    {
        std::ifstream f(kData + "/gameplay/osa_ak.json", std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        text = ss.str();
    }
    const std::size_t a = text.find("\"tv\": {");
    const std::size_t b = text.find("\"methods\": {");
    SC_EXPECT(a != std::string::npos && b != std::string::npos && a < b);
    EngagementTuning t;
    std::string error;
    SC_EXPECT(LoadEngagementTuning(text.substr(0, a) + text.substr(b), t, &error));
    SC_EXPECT(!t.has_tv_manual);
}

SC_TEST("tv: a distant target is tracked by the NPC operators, and the shot is charged for their aim") {
    const Run r = Play(Engage(Target(12000, 800), kTvOnly));
    SC_EXPECT(Has(r, "TV track"));
    SC_EXPECT(r.summary.launches == 1 && r.summary.hits + r.summary.misses == 1);
    SC_EXPECT(!(r.debrief.find("target left the TV field") != std::string::npos));
    SC_EXPECT(Has(r, "x TV aim 0."));
    SC_EXPECT(Has(r, "deg off the crosshair"));
}

SC_TEST("tv: a target crossing close in leaves the lens field faster than the operators can follow") {
    const Run r = Play(Engage(Target(6000, 800), kTvOnly));
    SC_EXPECT(Has(r, "track lost: target left the TV field"));
}

SC_TEST("tv: terrain hiding the target is carried on in memory for a few seconds, then the track is lost") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(Engage(Target(12000, 800), kTvOnly), kData, def, console));
    ScenarioRunner runner(std::move(def), std::move(console));
    while (!runner.Finished() && runner.Engagement().Method() != TrackMethod::Tv) runner.Step(0.1);
    SC_EXPECT(runner.Engagement().Method() == TrackMethod::Tv);
    // A 3 km-high ridge between the unit and the target (grid covers 30 x 30 km).
    std::vector<double> heights(61 * 61, 0.0);
    for (int col = 0; col < 61; ++col) heights[static_cast<std::size_t>(40 * 61 + col)] = 3000.0;
    const GridTerrain ridge(-15000.0, -15000.0, 500.0, 61, 61, heights);
    runner.SetTerrain(&ridge);
    for (int i = 0; i < 30; ++i) runner.Step(0.1);  // 3 s masked: still held
    SC_EXPECT(runner.Engagement().Method() == TrackMethod::Tv);
    for (int i = 0; i < 40; ++i) runner.Step(0.1);  // 7 s masked: the 5 s of memory ran out
    SC_EXPECT(runner.Engagement().Method() == TrackMethod::None);
    SC_EXPECT(runner.Debrief().find("track lost: masked: the memory ran out") != std::string::npos);
}

SC_TEST("engagement: a two-missile salvo, one missile on each channel, both guided and resolved") {
    const std::string salvo = std::string(kAcquireAndFire) + R"(,
        { "id": "salvo", "when": { "launches_at_least": 1 }, "do": [ { "queue": "launch", "crew": "Deputy" } ] })";
    const Run r = Play(Engage(Target(24000, 1000), salvo));
    SC_EXPECT(r.summary.launches == 2);
    SC_EXPECT(Has(r, "missile 1 from 3-I, channel I"));
    SC_EXPECT(Has(r, "missile 2 from 3-II, channel II"));
    SC_EXPECT(r.summary.hits + r.summary.misses == 2);
    SC_EXPECT(r.summary.early_actions == 0 && r.summary.out_of_order_actions == 0);
}

SC_TEST("engagement: an unoriented vehicle points board designations wrong, so the TV finds nothing") {
    // Radars silent: the azimuth comes from the plotting board, in true azimuth, set on the wheel as it is. Only an
    // oriented vehicle's wheel frame is true north (flak11: the vehicle must be oriented for correct designations).
    for (double heading : {0.0, 40.0}) {
        std::unique_ptr<ScenarioRunner> runner = Heading(heading, Target(12000, 800), kTvOnly);
        SC_EXPECT(runner != nullptr);
        if (runner == nullptr) return;
        bool tv = false;
        while (!runner->Finished() && runner->Time() < 400.0 && !tv) {
            runner->Step(0.1);
            tv = runner->Engagement().Method() == TrackMethod::Tv;
        }
        // Facing north the frames agree and the TV takes the target; 40 degrees off, the wheel puts it far outside
        // the lens field.
        SC_EXPECT(tv == (heading == 0.0));
    }
}

SC_TEST("engagement: a low target holds on the tracking radar only with its СДЦ on") {
    // At 0 m in the local frame the target's true height is about 50-90 m at 20-35 km (Earth curvature): over the
    // radar's 50 m limit, under the 100 m clutter height.
    auto run = [](bool mti_forced_off, bool& radar_tracked, bool& lost_in_clutter) {
        ScenarioDef def;
        ConsoleData console;
        if (!LoadScenario(Engage(Target(40000, 0), kAcquireAndFire), kData, def, console)) return false;
        ScenarioRunner runner(std::move(def), std::move(console));
        radar_tracked = lost_in_clutter = false;
        for (int i = 0; i < 6000 && !runner.Finished(); ++i) {
            runner.Step(0.1);
            if (mti_forced_off) runner.Console().panel.Set("ssc.mti", 0);
            radar_tracked = radar_tracked || runner.Engagement().Method() == TrackMethod::Radar;
        }
        lost_in_clutter = runner.Debrief().find("clutter") != std::string::npos;
        return true;
    };
    bool tracked = false, clutter = false;
    SC_EXPECT(run(false, tracked, clutter));
    SC_EXPECT(tracked);  // the drill switches СДЦ on
    SC_EXPECT(run(true, tracked, clutter));
    SC_EXPECT(!tracked);
}

SC_TEST("tuning: the tracking radar's clutter height is optional and the sensors' mti_control is checked") {
    EngagementTuning t;
    std::string error;
    SC_EXPECT(LoadEngagementTuningFile(kData + "/gameplay/osa_ak.json", t, &error));
    SC_EXPECT_NEAR(t.track_clutter_altitude_m, 100.0, 1e-12);
    std::ifstream f(kData + "/procedures/osa_ak.json", std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    const std::size_t at = text.find("\"mti_control\": \"ssc.mti\"");
    SC_EXPECT(at != std::string::npos);
    text.replace(at, std::string("\"mti_control\": \"ssc.mti\"").size(), "\"mti_control\": \"ssc.nothing\"");
    ConsoleData c;
    SC_EXPECT(!LoadConsole(text, c, &error) && error.find("sensors.track.mti_control") != std::string::npos);
}
