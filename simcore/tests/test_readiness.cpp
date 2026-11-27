// Start-up and readiness states (roadmap 6.2): the cold switch-on cycle and function check, the readiness states a
// scenario can start in, and the published timings as tests (march to combat, reaction time).
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "simcore/cabin.hpp"
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
    // He reads the check's instruments (АНОД, КЗ, ПУСК, РЕЖИМЫ: docs/FAULTS.md), then turns РОД РАБОТЫ back.
    TimeUntil(*r, [] { return false; }, checked + 20.0);
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

SC_TEST("readiness: the stabilisation's drives, СТОП, the loads and СВЯЗЬ act on a deployed Osa") {
    auto r = Load(Osa("deployed", "", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    const Equipment& e = c.equipment;
    auto run = [&](double s) { TimeUntil(*r, [] { return false; }, r->Time() + s); };
    auto press = [&](const char* id) {
        c.panel.Set(id, 1);
        r->Step(0.1);  // seen once by the equipment, then let go
    };
    run(1.0);
    SC_EXPECT(e.IsReady("stabilisation") && e.IsReady("search_antenna") && e.IsReady("gta") && e.IsReady("power"));

    // 0009-5M (voenizdat_osa_1991 p51): ПРИВОД Ψ, ПРИВОД γ or +27 В off stops the stabilisation, and the search with it.
    for (const char* sw : {"drv.psi_service", "drv.gamma_service", "drv.plus27"}) {
        c.panel.Set(sw, 0);
        run(1.0);
        SC_EXPECT(!e.IsReady("stabilisation") && !e.IsReady("search_antenna"));
        c.panel.Set(sw, 1);
        run(200.0);  // the stabilisation's 3 min start
        SC_EXPECT(e.IsReady("stabilisation"));
    }

    // PU-303: СТОП stops the turbine until ПУСК is put off; the take-off generator keeps the power on meanwhile.
    press("gta.stop");
    run(5.0);
    SC_EXPECT(!e.IsReady("gta") && e.IsReady("power"));
    c.panel.Set("pwr.gta", 0);
    run(1.0);
    c.panel.Set("pwr.gta", 1);
    run(70.0);  // the ГТА's start
    SC_EXPECT(e.IsReady("gta"));

    // ОТКЛ. НАГР. takes the loads off: no power until ГТА (or ИВП) puts them back.
    press("gta.loads_off");
    run(1.0);
    SC_EXPECT(!e.IsReady("power") && e.IsReady("gta"));
    press("gta.load_gta");
    run(1.0);
    SC_EXPECT(e.IsReady("power"));

    // СВЯЗЬ on the driver's strip: the sets and the intercom lose their 27 V.
    SC_EXPECT(e.IsReady("r123m_left") && e.IsReady("r124_intercom"));
    c.panel.Set("veh.comms_power", 0);
    run(1.0);
    SC_EXPECT(!e.IsReady("r123m_left") && !e.IsReady("r123m_right") && !e.IsReady("r124_intercom"));
}

SC_TEST("readiness: ТРЕНИРОВКА keeps the transmitters off the air; АВТОМАТ, ПОЛУАВТОМАТ and РУЧНОЕ set the range's source") {
    // The console alone, warmed up: a crew would keep a deployed vehicle's radars silent.
    ConsoleData c;
    std::string error;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error));
    Equipment& e = c.equipment;
    e.SetInput("vehicle_stopped", true);
    for (const char* on : {"pwr.gta", "pwr.equipment", "soc.hv_cover", "soc.hv", "ssc.tx_cover", "ssc.tx"}) {
        SC_EXPECT(c.panel.Set(on, 1));
    }
    for (const std::string& s : e.UnitIds()) {
        e.Warm(s, c.panel);
        e.Update(0.0, c.panel);
    }
    auto step = [&] {
        e.Update(0.1, c.panel);
        c.panel.ReleaseMomentaries();
    };
    auto press = [&](const char* id) {
        c.panel.Set(id, 1);
        step();
    };
    step();
    SC_EXPECT(e.IsReady("soc_transmitter") && e.IsReady("ssc_transmitter"));

    // 0091-18 (voenizdat_osa_1991 p49, p97): at ТРЕНИРОВКА the high voltage is lowered to train the magnetron.
    c.panel.Set("soc.training", 0);
    c.panel.Set("ssc.training", 0);
    step();
    SC_EXPECT(!e.IsReady("soc_transmitter") && !e.IsReady("ssc_transmitter"));
    c.panel.Set("soc.training", 1);
    c.panel.Set("ssc.training", 1);
    step();
    SC_EXPECT(e.IsReady("soc_transmitter") && e.IsReady("ssc_transmitter"));

    // OS81-18M (p98): АВТОМАТ puts the range on the radar as ПОЛУАВТОМАТ does; РУЧНОЕ gives it back to the wheel.
    SC_EXPECT(!e.IsReady("range_semi_auto"));
    press("ro.auto");
    SC_EXPECT(e.IsReady("range_semi_auto"));
    step();
    SC_EXPECT(e.IsReady("range_semi_auto"));  // latched
    press("ro.manual");
    SC_EXPECT(!e.IsReady("range_semi_auto"));
    press("ro.semi_auto");
    SC_EXPECT(e.IsReady("range_semi_auto"));
    c.panel.Set("ssc.tx", 0);  // and the transmitter off ends it, as before
    step();
    SC_EXPECT(!e.IsReady("range_semi_auto"));
}

SC_TEST("readiness: ВЛЕВО — ВПРАВО, held, slews the antenna at its rate in РУ and ЦУ, with the drives on and the wheel unlocked") {
    auto r = Load(Osa("deployed", "", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("senior_operator", "player"));  // nobody else turns the azimuth
    ConsoleData& c = r->Console();
    auto run = [&](double s) { TimeUntil(*r, [] { return false; }, r->Time() + s); };
    auto az = [&] { return c.panel.GetValue("ssc.az_wheel"); };
    const double kStep = 3.1;  // one 0.1 s step at 30°/s: the run may take one step more or fewer
    run(1.0);
    SC_EXPECT(c.equipment.IsReady("antenna_drives"));
    SC_EXPECT_NEAR(c.track.slew_rate_deg_s, 30.0, 1e-9);
    c.panel.Set("ssc.az_lock", 1);
    const double az0 = az();
    // OS52-12M1 (voenizdat_osa_1991 p99, p87-88): the АПУ's slew; 30°/s, SAM Simulator's. Held: it turns.
    c.panel.Set("ssc.slew", 2);  // ВПРАВО
    run(2.0);
    SC_EXPECT_NEAR(az(), az0 + 60.0, kStep);
    c.panel.Set("ssc.slew", 0);  // ВЛЕВО
    run(1.0);
    SC_EXPECT_NEAR(az(), az0 + 30.0, kStep);
    // Let go, the lever springs back to 0 and the antenna stops.
    SC_EXPECT(c.panel.LetGo("ssc.slew") && c.panel.Get("ssc.slew") == 1);
    run(1.0);
    SC_EXPECT_NEAR(az(), az0 + 30.0, kStep);
    // In ЦУ it slews as in РУ.
    c.panel.Set("fo.cu_az", 1);
    run(0.2);
    SC_EXPECT(c.equipment.IsReady("auto_acquisition"));
    c.panel.Set("ssc.slew", 2);
    run(1.0);
    SC_EXPECT_NEAR(az(), az0 + 60.0, kStep);
    c.panel.LetGo("ssc.slew");
    c.panel.Set("fo.ru", 1);  // back to РУ
    run(0.2);
    SC_EXPECT(!c.equipment.IsReady("auto_acquisition"));
    // In ПА (the TV's angle tracking) the lever does nothing (p87-88: РУ or ЦУ only).
    c.panel.Set("tov.supply", 1);
    c.panel.Set("tov.power", 1);
    run(30.0);
    c.panel.Set("fo.pa", 1);
    run(0.2);
    SC_EXPECT(c.equipment.IsReady("tv_angle_track"));
    double az1 = az();
    c.panel.Set("ssc.slew", 2);
    run(1.0);
    SC_EXPECT_NEAR(az(), az1, 1e-6);
    c.panel.LetGo("ssc.slew");
    c.panel.Set("fo.ru", 1);
    run(0.2);
    SC_EXPECT(!c.equipment.IsReady("tv_angle_track"));
    az1 = az();
    // With the wheel locked, or the drives off, the lever does nothing.
    c.panel.Set("ssc.az_lock", 0);
    c.panel.Set("ssc.slew", 2);
    run(1.0);
    SC_EXPECT_NEAR(az(), az1, 1e-6);
    c.panel.LetGo("ssc.slew");
    c.panel.Set("ssc.az_lock", 1);
    c.panel.Set("drv.az", 0);
    run(1.0);
    SC_EXPECT(!c.equipment.IsReady("antenna_drives"));
    c.panel.Set("ssc.slew", 2);
    run(1.0);
    SC_EXPECT_NEAR(az(), az1, 1e-6);
    c.panel.LetGo("ssc.slew");
}


SC_TEST("readiness: ЭМУ off stops the azimuth drive; in ПА the antenna carries on at the rate the wheel gave until БАЛАНС ПА") {
    auto r = Load(Osa("deployed", "", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("senior_operator", "player"));
    SC_EXPECT(r->TakeSeat("deputy_commander", "player"));
    ConsoleData& c = r->Console();
    const Equipment& e = c.equipment;
    auto run = [&](double s) { TimeUntil(*r, [] { return false; }, r->Time() + s); };
    auto az = [&] { return c.panel.GetValue("ssc.az_wheel"); };
    run(1.0);
    SC_EXPECT(e.IsReady("azimuth_drive") && c.track.slew == "azimuth_drive");
    // OS52-12M1 (voenizdat_osa_1991 p99): ЭМУ ОТКЛ. cuts the ЭМУ's control winding: the turret stops in azimuth.
    c.panel.Set("ssc.emu", 1);
    run(0.5);
    SC_EXPECT(!e.IsReady("azimuth_drive") && e.IsReady("antenna_drives"));
    c.panel.Set("ssc.az_lock", 1);
    c.panel.Set("ssc.slew", 2);
    const double held = az();
    run(1.0);
    SC_EXPECT_NEAR(az(), held, 1e-6);  // the fast slew can't turn it either
    c.panel.LetGo("ssc.slew");
    c.panel.Set("ssc.emu", 0);
    run(0.5);
    SC_EXPECT(e.IsReady("azimuth_drive"));

    // ПА (p87-88): the TV on, ПА pressed. A hand turns the wheel 4 deg and lets go: the integrator holds 2 deg/s.
    c.panel.Set("tov.supply", 1);
    c.panel.Set("tov.power", 1);
    run(30.0);
    c.panel.Set("fo.pa", 1);
    run(0.2);
    SC_EXPECT(e.IsReady("tv_angle_track"));
    const double before = az();
    run(1.0);
    SC_EXPECT_NEAR(az(), before, 1e-6);  // no rate yet: it stands
    c.panel.SetValue("ssc.az_wheel", az() + 4.0);
    run(0.1);
    const double from = az();
    run(2.0);
    SC_EXPECT_NEAR(az() - from, 4.0, 0.3);  // 2 deg/s on its own
    // БАЛАНС ПА (АЗИМУТ) clears the rate: it stops.
    c.panel.Set("ssc.pa_balance_az", 1);
    run(0.2);
    const double stopped = az();
    run(1.0);
    SC_EXPECT_NEAR(az(), stopped, 1e-6);
    // The rate is held to the drive's 30 deg/s (p85).
    c.panel.SetValue("ssc.az_wheel", az() + 100.0);
    run(0.1);
    const double fast = az();
    run(1.0);
    SC_EXPECT_NEAR(az() - fast, 30.0, 3.1);
    c.panel.Set("ssc.pa_balance_az", 1);
    run(0.2);
    // Out of ПА (РУ) nothing carries on.
    c.panel.Set("fo.ru", 1);
    run(0.2);
    SC_EXPECT(!e.IsReady("tv_angle_track"));
    c.panel.SetValue("ssc.az_wheel", az() - 4.0);
    run(0.1);
    const double ru = az();
    run(1.0);
    SC_EXPECT_NEAR(az(), ru, 1e-6);
}

namespace {
const CabinLamp* LampOf(const CabinLayout& layout, const std::string& id) {
    for (const CabinLamp& l : layout.lamps)
        if (l.id == id) return &l;
    return nullptr;
}
const CabinDisplay* DisplayOf(const CabinLayout& layout, const std::string& id) {
    for (const CabinDisplay& d : layout.displays)
        if (d.id == id) return &d;
    return nullptr;
}
}  // namespace

// Roadmap C4: the faults a scenario sets (set_input) and how they show: the lamps of 0005-8M, 0096-9M1, the stowing panel and
// 0004-11M1 light, and each takes out what the textbook says it does.
SC_TEST("faults: ОТКАЗ Qc, ОБРЫВ ФАЗЫ, ПРЕД. УГОЛ and ОТКАЗ СРП light their lamps and take out what they stand for") {
    auto r = Load(Osa("deployed", "", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    Equipment& e = c.equipment;
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", c, layout));
    auto lit = [&](const char* id) {
        const CabinLamp* l = LampOf(layout, id);
        SC_EXPECT(l != nullptr);
        return l != nullptr && CabinLayout::LampLit(*l, c.panel, e);
    };
    auto run = [&](double s) { TimeUntil(*r, [] { return false; }, r->Time() + s); };
    run(1.0);
    for (const char* id : {"qc_fail", "phase_break_stab", "phase_break_gon", "limit_angle", "srp_fail"}) SC_EXPECT(!lit(id));
    SC_EXPECT(e.IsReady("stabilisation") && e.IsReady("search_antenna") && e.IsReady("fire_control_computer"));
    // ОТКАЗ Qc (p51): the follow-up stops, the frame keeps its heading.
    e.SetInput("gyro_phase_lost", true);
    run(1.0);
    SC_EXPECT(lit("qc_fail"));
    // ОБРЫВ ФАЗЫ ψ, γ (p50): the drives' pumps lose a phase, the stabilisation faults and the search antenna goes down.
    e.SetInput("hydraulic_phase_break", true);
    run(1.0);
    SC_EXPECT(lit("phase_break_stab") && e.State("stabilisation") == SubsystemState::Fault && !e.IsReady("search_antenna"));
    e.SetInput("hydraulic_phase_break", false);
    // ОТКАЗ СРП (p137): the computer faults, no missile is prepared.
    e.SetInput("srp_fault", true);
    run(1.0);
    SC_EXPECT(lit("srp_fail") && e.State("fire_control_computer") == SubsystemState::Fault && !e.IsReady("missile_prep"));
    // ОБРЫВ ФАЗЫ ГОН (p96-97): the lamp; the radar's angle tracking cannot come up.
    e.SetInput("scanner_phase_break", true);
    run(1.0);
    SC_EXPECT(lit("phase_break_gon") && !e.IsReady("auto_tracking"));
}

SC_TEST("faults: ПРЕД. УГОЛ switches the search off until the antenna is stowed, and trips again while the drive is still at its limit") {
    auto r = Load(Osa("deployed", "", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("commander", "player"));
    ConsoleData& c = r->Console();
    Equipment& e = c.equipment;
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", c, layout));
    auto lit = [&](const char* id) { const CabinLamp* l = LampOf(layout, id); return l != nullptr && CabinLayout::LampLit(*l, c.panel, e); };
    auto run = [&](double s) { TimeUntil(*r, [] { return false; }, r->Time() + s); };
    run(1.0);
    SC_EXPECT(e.IsReady("search_antenna"));
    e.SetInput("stab_limit", true);  // a drive on its hard stop (p48)
    run(1.0);
    SC_EXPECT(lit("limit_angle") && !lit("stowed") && !e.IsReady("stabilisation") && !e.IsReady("search_antenna"));
    c.panel.Set("apu.stow", 0);  // ПРОХОД ПОД МОСТОМ ВКЛ. still on the slope: it trips again
    run(1.0);
    SC_EXPECT(lit("limit_angle"));
    e.SetInput("stab_limit", false);  // moved to level ground
    TimeUntil(*r, [&] { return e.State("search_antenna") == SubsystemState::Off; }, r->Time() + 60.0);  // stowing
    if (lit("limit_angle") || !lit("stowed"))
        std::printf("    stow %d tripped %s stabilisation %s search antenna %s\n", c.panel.Get("apu.stow"),
                    ToString(e.State("stab_limit_tripped")), ToString(e.State("stabilisation")), ToString(e.State("search_antenna")));
    SC_EXPECT(!lit("limit_angle") && lit("stowed"));
    c.panel.Set("apu.stow", 1);  // ВЫКЛ.: the antenna rises and stabilises again
    run(200.0);
    SC_EXPECT(e.IsReady("search_antenna"));
}

SC_TEST("ШИР. — СРЕДН. switches the medium-beam antenna in only in РР, and СР. Л. shows it (p119, p142)") {
    auto r = Load(Osa("deployed", "", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("deputy_commander", "player"));
    ConsoleData& c = r->Console();
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", c, layout));
    auto lit = [&](const char* id) { const CabinLamp* l = LampOf(layout, id); return l != nullptr && CabinLayout::LampLit(*l, c.panel, c.equipment); };
    auto run = [&](double s) { TimeUntil(*r, [] { return false; }, r->Time() + s); };
    run(1.0);
    c.panel.Set("svr.beam", 1);  // СРЕДН. in ТР: nothing
    run(1.0);
    SC_EXPECT(!c.equipment.IsReady("svr_medium_beam") && !lit("ch1_sr_l") && !lit("ch2_sr_l"));
    c.panel.Set("fo.mode", 2);  // РР
    run(1.0);
    SC_EXPECT(c.equipment.IsReady("svr_medium_beam") && lit("ch1_sr_l") && lit("ch2_sr_l"));
    c.panel.Set("svr.beam", 0);  // ШИР.
    run(1.0);
    SC_EXPECT(!lit("ch1_sr_l") && !lit("ch2_sr_l"));
}

SC_TEST("function check: АНОД ВКЛ. and КЗ light the K3 lamp in ФК if the channel is sound; the meters show РЕЖИМЫ's check") {
    auto r = Load(Osa("deployed", "", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("deputy_commander", "player"));
    ConsoleData& c = r->Console();
    Equipment& e = c.equipment;
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", c, layout));
    auto lit = [&](const char* id) { const CabinLamp* l = LampOf(layout, id); return l != nullptr && CabinLayout::LampLit(*l, c.panel, e); };
    auto meter = [&](const char* id) { const CabinDisplay* d = DisplayOf(layout, id); return d != nullptr ? MeterValue(*d, c) : -99.0; };  // levels or the checks' readings
    auto run = [&](double s) { TimeUntil(*r, [] { return false; }, r->Time() + s); };
    run(1.0);
    c.panel.Set("fk.kz", 1);
    run(0.2);
    SC_EXPECT(!lit("fk_k3"));  // not in ФК: the check set has no power (p158)
    c.panel.Set("fo.mode", 3);  // ФК
    run(181.0);
    SC_EXPECT(e.IsReady("function_check"));
    c.panel.Set("fk.anode", 1);
    run(0.2);
    SC_EXPECT(e.IsReady("fk_anode"));
    SC_EXPECT(c.panel.Set("fk.kz", 1));
    SC_EXPECT(lit("fk_k3"));  // while it is held (a push-button: the step lets it go)
    e.SetInput("k3_fault", true);
    SC_EXPECT(!lit("fk_k3"));
    c.panel.Set("err.mode", 0);
    run(0.1);
    SC_EXPECT_NEAR(meter("err_target_eps"), 5.0, 1e-9);
    c.panel.Set("err.mode", 1);  // СМЕНА ЗНАКА
    run(0.1);
    SC_EXPECT_NEAR(meter("err_missile_beta"), -5.0, 1e-9);
    e.SetInput("error_channel_fault", true);
    run(0.1);
    SC_EXPECT_NEAR(meter("err_target_beta"), 0.0, 1e-9);  // the dead channel
    c.panel.Set("fo.mode", 1);  // out of ФК: the anode drops
    run(0.5);
    SC_EXPECT(!e.IsReady("fk_anode"));
}

SC_TEST("TV and IFF checks: ВПУ at КОНТРОЛЬ shows the test chart and the TV cannot track; КОНТР. ИНД. puts a check mark on the PPI") {
    auto r = Load(Osa("deployed", "", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("deputy_commander", "player"));
    SC_EXPECT(r->TakeSeat("senior_operator", "player"));
    ConsoleData& c = r->Console();
    Equipment& e = c.equipment;
    auto run = [&](double s) { TimeUntil(*r, [] { return false; }, r->Time() + s); };
    // The search transmitter on (the interrogator works with it): the senior operator's seat is the test's.
    SC_EXPECT(c.panel.Set("soc.training", 1) && c.panel.Set("soc.hv_cover", 1) && c.panel.Set("soc.hv", 1));
    c.panel.Set("tov.supply", 1);
    c.panel.Set("tov.power", 1);
    run(30.0);
    SC_EXPECT(e.IsReady("tv_picture") && !e.IsReady("tv_test_chart"));
    c.panel.Set("tov.check", 1);
    run(0.5);
    SC_EXPECT(!e.IsReady("tv_picture") && e.IsReady("tv_test_chart"));
    c.panel.Set("fo.pa", 1);
    run(0.5);
    SC_EXPECT(!e.IsReady("tv_angle_track"));  // the crosshair is out (p102-104)
    c.panel.Set("tov.check", 0);
    // КОНТР. ИНД. (p66) with the interrogator ready: the check mark at 10 km on the sweep.
    SC_EXPECT(c.search.iff_check_control == "iff.ind_check");
    TimeUntil(*r, [&] { return e.IsReady(c.search.iff_ready); }, r->Time() + 400.0);
    SC_EXPECT(e.IsReady(c.search.iff_ready));
    c.panel.Set("iff.ind_check", 1);
    run(0.1);
    bool mark = false;
    for (const Contact& k : r->Engagement().IffReplies()) mark = mark || (k.target_id == EngagementModel::kIffCheckMark && std::fabs(k.range_m - 10000.0) < 1e-6);
    SC_EXPECT(mark);
}
