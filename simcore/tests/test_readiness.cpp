// Start-up and readiness states (roadmap 6.2): the cold switch-on cycle and function check, the readiness states a
// scenario can start in, and the published timings as tests (march to combat, reaction time).
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

// An Osa scenario with five crew, the gameplay engagement model, a readiness state and extra triggers.
std::string Osa(const std::string& readiness, const std::string& target, const std::string& triggers, bool stopped) {
    return R"({
      "scenario_id": "r", "name": "R", "source": "sam3d", "confidence": "estimate", "seed": 1,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 },
      "terrain": { "type": "flat", "height_m": 0 },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", )" +
           (readiness.empty() ? std::string() : "\"readiness\": \"" + readiness + "\", ") +
           R"("position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": )" +
           (stopped ? "true" : "false") + R"( } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [ )" + target + R"( ],
      "triggers": [ )" + triggers + R"( ],
      "end": { "time_limit_s": 900, "when_targets_gone": false } })";
}

std::unique_ptr<ScenarioRunner> Load(const std::string& text) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

// Steps until `done` holds; the time it first held, or a negative number if not within `max_s`.
template <typename F>
double TimeUntil(ScenarioRunner& r, F done, double max_s) {
    while (r.Time() < max_s) {
        r.Step(0.1);
        if (done()) return r.Time();
    }
    return -1.0;
}

// The time of the first debrief line containing `what`, or a negative number.
double LoggedAt(ScenarioRunner& r, const std::string& what) {
    const std::string d = r.Debrief();
    const std::size_t at = d.find(what);
    if (at == std::string::npos) return -1.0;
    const std::size_t line = d.rfind('\n', at) + 1;
    return std::atof(d.c_str() + line);
}

const char* kPopUp = R"({ "id": "t1", "name": "T", "class": "fighter", "start": { "east_m": 1500, "north_m": 9500, "up_m": 1000 },
    "waypoints": [ { "east_m": 1500, "north_m": -20000, "up_m": 1000 } ], "speed_mps": 250, "appear_at_s": 5 })";

const char* kEngage = R"({ "id": "search", "when": {}, "do": [ { "order": "search_on_march", "crew": "Senior operator" } ] },
    { "id": "acq", "when": { "conditions": [ { "subsystem": "soc_transmitter", "is": "ready" } ] },
      "do": [ { "order": "acquire_with_soc", "crew": "Senior operator" }, { "order": "ready_capture", "crew": "Deputy" },
              { "order": "capture_with_ssc", "crew": "Operator" },
              { "queue": "prepare_firing", "crew": "Deputy" }, { "queue": "launch", "crew": "Deputy" } ] })";

}  // namespace

SC_TEST("readiness: a cold start takes the documented switch-on cycle and function check (flak11: 3.8 + 3.0 min)") {
    auto r = Load(Osa("", "", R"({ "id": "go", "when": {}, "do": [ { "order": "power_up", "crew": "Driver" },
        { "order": "equipment_on", "crew": "Operator" }, { "order": "function_check", "crew": "Deputy" } ] })", false));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const Equipment& eq = r->Console().equipment;
    SC_EXPECT(!eq.IsReady("power") && !eq.IsReady("hv_ready"));  // cold
    const double warm = TimeUntil(*r, [&] { return eq.IsReady("hv_ready"); }, 900);
    const double checked = TimeUntil(*r, [&] { return eq.IsReady("function_checked"); }, 900);
    std::printf("    cold: equipment warm at %.2f min, function check done at %.2f min\n", warm / 60.0, checked / 60.0);
    SC_EXPECT(warm > 3.8 * 60.0 * 0.9 && warm < 3.8 * 60.0 * 1.1);
    SC_EXPECT(checked > 6.8 * 60.0 * 0.95 && checked < 6.8 * 60.0 * 1.1);
    TimeUntil(*r, [] { return false; }, checked + 5.0);  // the deputy turns РОД РАБОТЫ back after a moment
    SC_EXPECT(r->Console().panel.Get("fo.mode") == 1);  // back to ТР after the check
    SC_EXPECT(r->Crew("Deputy")->Tracker().Deviations() == 0);
}

SC_TEST("readiness: a unit can start powered on the march or deployed and oriented") {
    auto powered = Load(Osa("powered", "", "", false));
    SC_EXPECT(powered != nullptr);
    if (!powered) return;
    const Equipment& pe = powered->Console().equipment;
    for (const char* sub : {"power", "gyro_erected", "equipment_warm", "hv_ready", "stabilisation", "search_antenna",
                            "fire_control_computer"}) {
        SC_EXPECT(pe.IsReady(sub));
    }
    SC_EXPECT(!pe.IsReady("antenna_drives") && !pe.GetInput("vehicle_stopped"));
    powered->Step(0.1);
    SC_EXPECT(pe.IsReady("hv_ready") && pe.IsReady("gyro_erected"));  // still warm a step later (АРРЕТИР. released)

    auto deployed = Load(Osa("deployed", "", "", true));
    SC_EXPECT(deployed != nullptr);
    if (!deployed) return;
    const Equipment& de = deployed->Console().equipment;
    deployed->Step(0.1);
    SC_EXPECT(de.IsReady("pto_generator") && de.IsReady("antenna_drives") && de.IsReady("apu_free"));
    // Oriented, with the site's latitude on the correction: only the gyro's own drift is left (far below 0.001 deg in 0.1 s).
    SC_EXPECT(std::fabs(deployed->Engagement().GyroHeadingDeg() - deployed->Engagement().UnitHeadingDeg()) < 1e-3);
    SC_EXPECT(deployed->Engagement().Oriented() && deployed->OrientedAtHalt());
    SC_EXPECT_NEAR(deployed->Console().panel.GetValue("orient.latitude"), 45.0, 1e-9);
    SC_EXPECT(deployed->Debrief().find("starts deployed") != std::string::npos);

    // An unknown state is refused at load, naming the ones there are.
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(!LoadScenario(Osa("parked", "", "", true), kData, def, console, &error) &&
              error.find("own_unit.readiness: unknown state 'parked' (cold, powered, deployed)") != std::string::npos);
    SC_EXPECT(LoadScenario(Osa("cold", "", "", true), kData, def, console, &error) && def.readiness.empty());
}

SC_TEST("readiness: from a powered march the vehicle is ready for combat inside 4 min (nvo_2015)") {
    auto r = Load(Osa("powered", "", R"({ "id": "halt", "when": {}, "do": [ { "set_input": "vehicle_stopped", "value": true },
        { "order": "deploy_at_halt", "crew": "Operator" }, { "order": "pto_on_line", "crew": "Driver" },
        { "order": "orient_vehicle", "crew": "Deputy" } ] })", false));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    TimeUntil(*r, [] { return false; }, 300.0);
    const double deployed = LoggedAt(*r, "completed  deploy_at_halt");
    const double oriented = LoggedAt(*r, "completed  orient_vehicle");
    std::printf("    march to combat: deployed %.1f s, oriented %.1f s\n", deployed, oriented);
    // The switches are modelled, not the physical work of stopping and unlocking the launcher: SAM3D is well inside
    // the manufacturer's 4 min (and faster than the unverified 35-50 s of video_osa).
    SC_EXPECT(deployed > 0.0 && oriented > 0.0 && deployed < 240.0 && oriented < 240.0);
    SC_EXPECT(r->Console().equipment.IsReady("antenna_drives") && r->Console().equipment.IsReady("pto_generator"));
}

SC_TEST("readiness: a deployed vehicle's reaction time, detection to launch, is inside the published 26-40 s") {
    // korovin_tiv_2010 (Osa-AK and Osa-AKM: 27-39 s), nvo_2015 (26-40 s): for a target that appears inside the zone.
    auto r = Load(Osa("deployed", kPopUp, kEngage, true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    double contact = -1.0, launch = -1.0;
    while (r->Time() < 300 && launch < 0) {
        r->Step(0.1);
        if (contact < 0 && !r->Engagement().SearchPicture().empty()) contact = r->Time();
        if (r->Launches() > 0) launch = r->Time();
    }
    std::printf("    pop-up: contact %.1f s, radar track %.1f s, launch %.1f s: reaction %.1f s\n", contact,
                LoggedAt(*r, "radar track"), launch, launch - contact);
    SC_EXPECT(contact > 0.0 && launch > 0.0);
    SC_EXPECT(launch - contact >= 26.0 && launch - contact <= 40.0);
}
