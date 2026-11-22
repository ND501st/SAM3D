#include <string>
#include <utility>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string PracticePath() { return kData + "/scenarios/osa_practice.json"; }

bool Load(const std::string& text, ScenarioDef& def, ConsoleData& console, std::string* error = nullptr) {
    return LoadScenario(text, kData, def, console, error);
}

bool ErrorContains(const std::string& text, const std::string& expected) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (Load(text, def, console, &error)) return false;
    if (error.find(expected) == std::string::npos) std::printf("    got: %s\n", error.c_str());
    return error.find(expected) != std::string::npos;
}

// A small valid scenario around the Osa console; `extra` is spliced in before the closing brace.
std::string Small(const std::string& extra = "") {
    return R"({
      "scenario_id": "s", "name": "S", "source": "sam3d", "confidence": "estimate",
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 },
      "terrain": { "type": "flat", "height_m": 0 },
      "own_unit": { "console": "procedures/osa_ak.json", "position": { "east_m": 0, "north_m": 0, "up_m": 0 } },
      "crew": [ { "name": "Cmdr", "role": "commander" } ],
      "targets": [ { "id": "t1", "name": "T", "start": { "east_m": 0, "north_m": 20000, "up_m": 500 },
                     "waypoints": [ { "east_m": 0, "north_m": -20000, "up_m": 500 } ], "speed_mps": 200 } ])" +
           extra + "}";
}

std::string RunPractice(unsigned seed_override, bool override_seed) {
    ScenarioDef def;
    ConsoleData console;
    if (!LoadScenarioFile(PracticePath(), kData, def, console)) return "load failed";
    if (override_seed) def.seed = seed_override;
    ScenarioRunner runner(std::move(def), std::move(console));
    runner.RunToEnd(0.1);
    return runner.Debrief();
}

}  // namespace

SC_TEST("scenario: the practice shoot runs from the march to a launch with no mistakes") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(PracticePath(), kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(def.crew.size() == 5 && def.targets.size() == 1 && def.triggers.size() == 5);
    ScenarioRunner runner(std::move(def), std::move(console));
    SC_EXPECT(runner.RunToEnd(0.1));
    SC_EXPECT(runner.EndReason() == "all targets gone");
    SC_EXPECT(runner.Launches() == 1);
    const DebriefSummary s = runner.Log().Summary();
    SC_EXPECT(s.equipment_faults == 0 && s.early_actions == 0 && s.out_of_order_actions == 0);
    SC_EXPECT(s.procedures_aborted == 0 && s.procedures_completed >= 6);
    const std::string text = runner.Debrief();
    SC_EXPECT(text.find("LAUNCH") != std::string::npos && text.find("t1") != std::string::npos);
    // Resolved by the gameplay engagement model; the scenario's seed (1) gives a hit, so the practice ends well.
    SC_EXPECT(s.hits == 1 && s.misses == 0);
    SC_EXPECT(runner.Acmi().Text().find("Name=9K33M2 Osa-AK") != std::string::npos);
    // Ends when the drone goes down: it is launched at 200 s (after the crew's 4 min to readiness); ЦЕЛЬ В ЗОНЕ at
    // about 412 s (the meeting point at the zone's far edge), our launch at about 427 s (13 s of preparation from
    // "target in zone"), about 17 s of flight to the meeting point.
    SC_EXPECT(runner.Time() > 438.0 && runner.Time() < 452.0);
}

SC_TEST("scenario: same seed, same run; another seed changes crew timing") {
    const std::string a = RunPractice(0, false);
    const std::string b = RunPractice(0, false);
    const std::string c = RunPractice(7, true);
    SC_EXPECT(a == b);
    SC_EXPECT(a != c);
    SC_EXPECT(c.find("LAUNCH") != std::string::npos);
}

SC_TEST("scenario: loader checks references against the console") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(Load(Small(), def, console, &error));
    SC_EXPECT(def.targets[0].waypoints.size() == 1 && console.system_id == "osa_ak");

    SC_EXPECT(ErrorContains(Small(R"(, "triggers": [ { "id": "x", "when": {}, "do": [ { "set_input": "nope" } ] } ])"),
                            "triggers[0].do[0].set_input: unknown input 'nope'"));
    SC_EXPECT(ErrorContains(Small(R"(, "triggers": [ { "id": "x", "when": {}, "do": [ { "order": "dance", "crew": "Cmdr" } ] } ])"),
                            "unknown procedure 'dance'"));
    SC_EXPECT(ErrorContains(Small(R"(, "triggers": [ { "id": "x", "when": {}, "do": [ { "order": "launch", "crew": "Bob" } ] } ])"),
                            "unknown crew member 'Bob'"));
    // Orders go to the seat that has the controls; crew roles are the console's stations, one each.
    SC_EXPECT(ErrorContains(Small(R"(, "triggers": [ { "id": "x", "when": {}, "do": [ { "order": "launch", "crew": "Cmdr" } ] } ])"),
                            "triggers[0].do[0].crew: 'launch' is done at station 'deputy_commander', Cmdr sits at 'commander'"));
    std::string loader = Small();
    loader.replace(loader.find(R"("role": "commander")"), 19, R"("role": "loader")");
    SC_EXPECT(ErrorContains(loader, "crew[0].role: 'loader' is not a station of the console"));
    std::string twice = Small();
    twice.replace(twice.find(R"({ "name": "Cmdr", "role": "commander" })"), 39,
                  R"({ "name": "Cmdr", "role": "commander" }, { "name": "Cmdr 2", "role": "commander" })");
    SC_EXPECT(ErrorContains(twice, "crew[1].role: 'commander' already has a crew member"));
    SC_EXPECT(ErrorContains(Small(R"(, "triggers": [ { "id": "x", "when": { "target": "t9", "target_within_m": 5 }, "do": [ { "end": "e" } ] } ])"),
                            "unknown target 't9'"));
    SC_EXPECT(ErrorContains(Small(R"(, "triggers": [ { "id": "x", "when": { "target": "t1" }, "do": [ { "end": "e" } ] } ])"),
                            "target and target_within_m go together"));
    SC_EXPECT(ErrorContains(Small(R"(, "triggers": [ { "id": "x", "when": { "conditions": [ { "subsystem": "warp", "is": "ready" } ] }, "do": [ { "end": "e" } ] } ])"),
                            "unknown subsystem 'warp'"));
    std::string bad_console = Small();
    bad_console.replace(bad_console.find("procedures/osa_ak.json"), 22, "procedures/missing.json");
    SC_EXPECT(ErrorContains(bad_console, "own_unit.console: cannot open"));
    const std::string start = R"("start": { "east_m": 0, "north_m": 20000, "up_m": 500 })";
    std::string mixed = Small();
    const std::size_t at = mixed.find(start);
    SC_EXPECT(at != std::string::npos);
    if (at != std::string::npos) {
        mixed.replace(at, start.size(), R"("start": { "east_m": 0, "north_m": 20000, "alt_m": 500 })");
        SC_EXPECT(ErrorContains(mixed, "use either east/north/up or lat/lon/alt"));
    }
}

SC_TEST("scenario: triggers fire once; time limit and end actions stop the run") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(Load(Small(R"(, "triggers": [
        { "id": "close", "when": { "target": "t1", "target_within_m": 10000 }, "do": [ { "set_input": "target_in_zone" } ] },
        { "id": "stop", "when": { "after_s": 80 }, "do": [ { "end": "enough" } ] } ],
      "end": { "time_limit_s": 30 } )"),
                   def, console));
    {
        ScenarioRunner r(def, console);
        SC_EXPECT(r.RunToEnd(0.1));
        SC_EXPECT(r.EndReason() == "time limit" && r.Time() < 30.1);
        SC_EXPECT(!r.Console().equipment.GetInput("target_in_zone"));  // still 20 - 6 = 14 km away
    }
    def.time_limit_s = 1000.0;
    ScenarioRunner r(def, console);
    SC_EXPECT(r.RunToEnd(0.1));
    SC_EXPECT(r.EndReason() == "enough" && r.Time() > 79.9 && r.Time() < 80.2);
    SC_EXPECT(r.Console().equipment.GetInput("target_in_zone"));  // 20 km at 200 m/s: within 10 km from 50 s
}

SC_TEST("scenario: ПУСК without the launch circuit armed is noted, not counted") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(Load(Small(), def, console));
    ScenarioRunner r(def, console);
    r.Step(0.1);
    r.Console().panel.Set("fo.launch", 1);
    r.Step(0.1);
    SC_EXPECT(r.Launches() == 0);
    SC_EXPECT(r.Debrief().find("is not ready: no launch") != std::string::npos);
}

SC_TEST("scenario: a target that passes its last waypoint ends the run") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(Load(Small(), def, console));
    ScenarioRunner r(def, console);
    SC_EXPECT(r.RunToEnd(0.1, 1000.0));
    SC_EXPECT(r.EndReason() == "all targets gone");
    SC_EXPECT(r.Time() > 195.0 && r.Time() < 205.0);  // 40 km at 200 m/s, less the 500 m capture radius
    SC_EXPECT(r.TargetRange("t1") > 19000.0);
}

SC_TEST("scenario: the chain's messages in the session (P1): said on their channel, a rider seen coming with a paper") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(Load(Small(R"js(, "end": { "time_limit_s": 400, "when_targets_gone": false },
      "deliveries": [
        { "id": "warn", "after_s": 20, "to": "commander", "means": "radio", "net": "battery net",
          "from": "Captain Andrej Novak (Commander, battery 21)", "words": "Vozdukh! A raid over the sector in ten minutes.",
          "air_warning": true, "azimuth_deg": 265 },
        { "id": "summary", "after_s": 300, "means": "despatch_rider", "from": "Captain Milos Jelinek (Chief of signals)",
          "words": "From the regiment: the day's summary.", "paper": { "title": "Intelligence summary", "text": "Two raids from the west." },
          "from_bearing_deg": 90 } ] 
)js"),
                   def, console));
    SC_EXPECT(def.deliveries.size() == 2);
    ScenarioRunner r(def, console);
    // The rider is on the road before he arrives: east of the vehicle, coming closer.
    for (int i = 0; i < 1500; ++i) r.Step(0.1);  // 150 s
    std::vector<ScenarioRunner::VisitorState> v = r.Visitors();
    SC_EXPECT(v.size() == 1);
    double first_east = 0.0;
    if (!v.empty()) {
        SC_EXPECT(v[0].model == "motorcycle" && !v[0].at_vehicle);
        first_east = v[0].position.x;
        SC_EXPECT(first_east > 100.0 && std::fabs(v[0].position.y) < 1.0 && std::fabs(v[0].heading_deg - 270.0) < 1e-6);
    }
    bool warned = false;
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) warned = warned || (m.net == "battery net" && m.text.find("Vozdukh") == 0);
    SC_EXPECT(warned);
    // The warning gave the raid's direction: whoever heard it has it in his picture, as told (P4).
    const CrewPicture* cmdr = r.PictureOf("commander");
    const KnownTarget* raid = cmdr != nullptr ? cmdr->Find("warning:warn") : nullptr;
    SC_EXPECT(raid != nullptr && raid->has_azimuth && raid->true_azimuth && std::fabs(raid->azimuth_deg - 265.0) < 1e-9 && !raid->has_range &&
              raid->source.find("Novak") != std::string::npos);
    SC_EXPECT(r.MapCase("").empty());
    for (int i = 0; i < 1550; ++i) r.Step(0.1);  // 305 s: he has come
    v = r.Visitors();
    SC_EXPECT(v.size() == 1 && v[0].at_vehicle && v[0].position.x < first_east && v[0].position.x < 7.0);
    const std::vector<HeldPaper> papers = r.MapCase("commander");
    SC_EXPECT(papers.size() == 1 && papers[0].title == "Intelligence summary" && papers[0].time_s > 299.0);
    bool at_hatch = false;
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) at_hatch = at_hatch || m.net == "at the hatch";
    SC_EXPECT(at_hatch);
    // The loader checks the means, the net and the station.
    SC_EXPECT(ErrorContains(Small(R"js(, "deliveries": [ { "id": "x", "means": "pigeon", "from": "A", "words": "B" } ]
)js"), "unknown means"));
    SC_EXPECT(ErrorContains(Small(R"js(, "deliveries": [ { "id": "x", "means": "radio", "from": "A", "words": "B" } ]
)js"), "names its net"));
    SC_EXPECT(ErrorContains(Small(R"js(, "deliveries": [ { "id": "x", "means": "intercom", "from": "A", "words": "B", "azimuth_deg": 400 } ]
)js"), "[0, 360)"));
    SC_EXPECT(ErrorContains(Small(R"js(, "deliveries": [ { "id": "x", "means": "runner", "to": "cook", "from": "A", "words": "B" } ]
)js"),
                            "unknown station"));
}
