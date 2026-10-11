#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/debrief.hpp"
#include "simcore/json.hpp"
#include "test_framework.hpp"

using namespace simcore;

#ifndef SIMCORE_DATA_DIR
#error "SIMCORE_DATA_DIR must point at the repo's data/ folder"
#endif

namespace {

std::string OsaPath() { return std::string(SIMCORE_DATA_DIR) + "/procedures/osa_ak.json"; }

// One sim step in the documented order: equipment, then let go of buttons, then crew.
void Tick(ConsoleData& c, CrewMember& crew, double dt = 0.1) {
    c.equipment.Update(dt, c.panel);
    c.panel.ReleaseMomentaries();
    crew.Update(dt, c.panel, &c.equipment);
}

// The whole crew: one member per console station. Orders go to the seat that owns the procedure.
class Seats {
public:
    explicit Seats(ConsoleData& c) : c_(c) {
        for (const ConsoleStation& st : c.stations) crew_.emplace_back(st.label, st.id, 1.0);
    }

    CrewMember& At(const std::string& station) {
        for (CrewMember& m : crew_) {
            if (m.Role() == station) return m;
        }
        return crew_.front();
    }

    void Order(const char* id) {
        const Procedure& p = *c_.FindProcedure(id);
        At(p.station).Order(p);
    }

    void Queue(const char* id) {
        const Procedure& p = *c_.FindProcedure(id);
        At(p.station).Queue(p);
    }

    // One sim step in the documented order: equipment, then let go of buttons, then crew.
    void Tick(double dt = 0.1) {
        c_.equipment.Update(dt, c_.panel);
        c_.panel.ReleaseMomentaries();
        for (CrewMember& m : crew_) m.Update(dt, c_.panel, &c_.equipment);
    }

    bool Busy() const {
        for (const CrewMember& m : crew_) {
            if (m.IsBusy()) return true;
        }
        return false;
    }

    // Run until nobody has anything left to do (or a time limit). Returns the time taken.
    double RunUntilIdle(double limit_s = 600.0) {
        double t = 0.0;
        for (; Busy() && t < limit_s; t += 0.1) Tick();
        return t;
    }

    void ClearReports() {
        for (CrewMember& m : crew_) m.TakeReports();
    }

private:
    ConsoleData& c_;
    std::vector<CrewMember> crew_;
};

bool AnyFault(ConsoleData& c) {
    bool fault = false;
    for (const SubsystemTransition& tr : c.equipment.TakeTransitions()) {
        if (tr.to == SubsystemState::Fault) {
            std::printf("    fault: %s at %.1f s\n", tr.id.c_str(), tr.time_s);
            fault = true;
        }
    }
    return fault;
}

// From the march: stop, start the GTA, switch the equipment on and come into action. Returns once the
// transmitters can take high voltage (the three-minute relay) and leaves the crew idle.
void PowerUpAndDeploy(ConsoleData& c, Seats& seats) {
    c.equipment.SetInput("vehicle_stopped", true);
    seats.Order("power_up");
    seats.Queue("pto_on_line");     // both the driver's (PU-303)
    seats.Order("equipment_on");
    seats.Queue("deploy_at_halt");  // both the operator's (cabinet 0096-9M1)
    seats.RunUntilIdle();
    for (int i = 0; i < 3000 && !c.equipment.IsReady("hv_ready"); ++i) seats.Tick();
    seats.ClearReports();
}

bool LoadErrorContains(const std::string& text, const std::string& expected) {
    ConsoleData c;
    std::string error;
    if (LoadConsole(text, c, &error)) return false;
    return error.find(expected) != std::string::npos;
}

const char* kMinimal = R"({
  "system_id": "t", "display_name": "T",
  "controls": [
    { "id": "cover", "label": "C", "kind": "switch", "source": "s", "confidence": "estimate" },
    { "id": "sw", "label": "S", "kind": "switch", "guard": "cover", "source": "s", "confidence": "estimate" }
  ]
})";

}  // namespace

SC_TEST("panel: wheels clamp or wrap, and obey their lock") {
    ControlPanel p;
    SC_EXPECT(p.AddControl({"lock", "LOCK", 2, 0}));
    ControlDef az{"az", "AZ", 2, 0, "lock"};
    az.kind = ControlKind::Continuous;
    az.min_value = -330.0;
    az.max_value = 330.0;
    SC_EXPECT(p.AddControl(az));
    ControlDef dial{"dial", "DIAL", 2, 0};
    dial.kind = ControlKind::Continuous;
    dial.max_value = 360.0;
    dial.wraps = true;
    SC_EXPECT(p.AddControl(dial));
    ControlDef bad = dial;
    bad.id = "bad";
    bad.initial_value = 400.0;
    SC_EXPECT(!p.AddControl(bad));
    ControlDef guarded_by_wheel{"x", "X", 2, 0, "dial"};
    SC_EXPECT(!p.AddControl(guarded_by_wheel));

    SC_EXPECT(!p.SetValue("az", 10.0));  // locked
    SC_EXPECT(p.Set("lock", 1));
    SC_EXPECT(p.SetValue("az", 500.0));
    SC_EXPECT_NEAR(p.GetValue("az"), 330.0, 1e-12);
    SC_EXPECT(p.Nudge("az", -700.0));
    SC_EXPECT_NEAR(p.GetValue("az"), -330.0, 1e-12);
    SC_EXPECT(p.SetValue("dial", 370.0));
    SC_EXPECT_NEAR(p.GetValue("dial"), 10.0, 1e-9);
    SC_EXPECT(p.SetValue("dial", -30.0));
    SC_EXPECT_NEAR(p.GetValue("dial"), 330.0, 1e-9);
    SC_EXPECT(p.Get("az") == -1 && !p.Set("az", 1) && !p.SetValue("lock", 1.0));
}

SC_TEST("panel: a push-button reads pressed until the step's release") {
    ControlPanel p;
    ControlDef b{"fire", "FIRE", 2, 0};
    b.kind = ControlKind::Momentary;
    SC_EXPECT(p.AddControl(b));
    ControlDef bad = b;
    bad.id = "bad";
    bad.positions = 3;
    SC_EXPECT(!p.AddControl(bad));
    SC_EXPECT(p.Set("fire", 1) && p.Get("fire") == 1);
    p.ReleaseMomentaries();
    SC_EXPECT(p.Get("fire") == 0);
}

SC_TEST("equipment: a button latches a mode until its release; inputs and any-of enables") {
    ControlPanel p;
    ControlDef on{"on", "ON", 2, 0};
    on.kind = ControlKind::Momentary;
    ControlDef off = on;
    off.id = "off";
    p.AddControl(on);
    p.AddControl(off);
    p.AddControl({"manual", "MANUAL", 2, 0});
    Equipment eq;
    SC_EXPECT(eq.AddInput("near") && !eq.AddInput("near"));
    SubsystemDef mode{"mode", "Mode", {Condition::Control("on", 1)}, {}, 0.0, 0.0};
    mode.latching = true;
    SC_EXPECT(!eq.AddSubsystem(mode, p));  // latching needs release conditions
    mode.release = {Condition::Control("off", 1)};
    SC_EXPECT(eq.AddSubsystem(mode, p));
    SubsystemDef prep{"prep", "Prep", {}, {}, 2.0, 0.0};
    prep.enable_any = {Condition::Control("manual", 1), Condition::Input("near")};
    SC_EXPECT(eq.AddSubsystem(prep, p));
    SubsystemDef bad{"bad", "Bad", {Condition::Input("nope")}, {}, 0.0, 0.0};
    SC_EXPECT(!eq.AddSubsystem(bad, p));

    p.Set("on", 1);
    eq.Update(0.1, p);
    p.ReleaseMomentaries();
    SC_EXPECT(eq.IsReady("mode"));
    eq.Update(0.1, p);
    SC_EXPECT(eq.IsReady("mode"));  // still latched after the button came back up
    p.Set("off", 1);
    eq.Update(0.1, p);
    p.ReleaseMomentaries();
    SC_EXPECT(eq.State("mode") == SubsystemState::Off);

    SC_EXPECT(eq.State("prep") == SubsystemState::Off);
    SC_EXPECT(eq.SetInput("near", true) && !eq.SetInput("nope", true));
    eq.Update(0.1, p);
    SC_EXPECT(eq.State("prep") == SubsystemState::Starting);
    eq.SetInput("near", false);
    p.Set("manual", 1);  // the other way in keeps it going
    for (int i = 0; i < 20; ++i) eq.Update(0.1, p);
    SC_EXPECT(eq.IsReady("prep"));
}

SC_TEST("json: values, escapes, UTF-8 and errors with positions") {
    json::Value v;
    std::string error;
    SC_EXPECT(json::Parse("\xEF\xBB\xBF{\"a\": [1, -2.5e2, true, null], \"s\": \"\\u0414\xD0\xB0 \\\"q\\\" \\uD83D\\uDE00\"}", v, &error));
    SC_EXPECT(v.Is(json::Type::Object) && v.keys.size() == 2);
    const json::Value* a = v.Find("a");
    SC_EXPECT(a != nullptr && a->items.size() == 4);
    SC_EXPECT_NEAR(a->items[1].number, -250.0, 1e-12);
    SC_EXPECT(a->items[2].boolean && a->items[3].Is(json::Type::Null));
    SC_EXPECT(v.Find("s")->string == "\xD0\x94\xD0\xB0 \"q\" \xF0\x9F\x98\x80");  // "Да" + quote + emoji
    SC_EXPECT(v.Find("missing") == nullptr);

    SC_EXPECT(!json::Parse("{\n  \"a\": 1,\n  \"a\": 2\n}", v, &error));
    SC_EXPECT(error.find("line 3") != std::string::npos && error.find("duplicate key") != std::string::npos);
    SC_EXPECT(!json::Parse("[1, 2", v, &error));
    SC_EXPECT(!json::Parse("{\"a\": 01}", v, &error));
    SC_EXPECT(!json::Parse("\"\\uD800\"", v, &error) && error.find("surrogate") != std::string::npos);
    SC_EXPECT(!json::Parse("[1] x", v, &error));
    SC_EXPECT(!json::Parse(std::string(100, '[') + std::string(100, ']'), v, &error));
}

SC_TEST("console loader: strict validation with paths in the messages") {
    ConsoleData c;
    std::string error;
    SC_EXPECT(LoadConsole(kMinimal, c, &error));
    SC_EXPECT(c.system_id == "t" && c.panel.IsBlocked("sw", 1));

    std::string t = kMinimal;
    SC_EXPECT(LoadErrorContains(std::string(kMinimal).replace(t.find("\"kind\": \"switch\""), 16, "\"kind\": \"lever\""),
                                "controls[0].kind"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","colour":1})", "colour: unknown key"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","controls":[{"id":"a","label":"A","kind":"switch","source":"s"}]})",
                                "controls[0].confidence: required"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","controls":[{"id":"a","label":"A","kind":"switch","source":"s","confidence":"sure"}]})",
                                "must be high, medium, low or estimate"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","controls":[{"id":"a","label":"A","kind":"switch","guard":"b","source":"s","confidence":"low"}]})",
                                "controls[0].guard: unknown control 'b'"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","controls":[{"id":"w","label":"W","kind":"wheel","source":"s","confidence":"low"}]})",
                                "a wheel needs min and max"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","subsystems":[{"id":"x","name":"X","enable":[{"subsystem":"y","is":"ready"}],"source":"s","confidence":"low"}]})",
                                "subsystems[0].enable[0]: unknown subsystem 'y'"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","procedures":[{"id":"p","name":"P","source":"s","confidence":"low","steps":[{"control":"nope","position":1,"description":"d"}]}]})",
                                "procedures[0].steps[0]: control 'nope'"));
    SC_EXPECT(LoadErrorContains("{\"system_id\": \"t\",", "line 1"));

    ConsoleData untouched;
    untouched.system_id = "keep";
    SC_EXPECT(!LoadConsole("{}", untouched, &error) && untouched.system_id == "keep");
    SC_EXPECT(!LoadConsoleFile("does/not/exist.json", untouched, &error) && error.find("cannot open") != std::string::npos);
}

SC_TEST("osa data: loads, with the manual's covers, key, wheels and buttons") {
    ConsoleData c;
    std::string error;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(c.system_id == "osa_ak");
    SC_EXPECT(c.panel.IsBlocked("soc.hv", 1) && c.panel.IsBlocked("ssc.tx", 1) && c.panel.IsBlocked("fo.bort_spk", 1));
    SC_EXPECT(c.panel.IsBlocked("fo.key", 2));  // insert before turning
    SC_EXPECT(!c.panel.SetValue("ssc.az_wheel", 10.0));  // locked until right-clicked
    SC_EXPECT(c.panel.Find("fo.launch")->kind == ControlKind::Momentary);
    SC_EXPECT(c.panel.Find("soc.range")->position_labels[2] == "10–45 km");
    // The search antenna starts stowed until the equipment and stabilisation are on (voenizdat p53), with its
    // ПРОХОД ПОД МОСТОМ switch allowing it up; each launch restarts missile preparation.
    SC_EXPECT(c.panel.Get("apu.stow") == 1 && !c.equipment.IsReady("search_antenna"));
    SC_EXPECT(c.launch_restarts == "missile_prep");
}

SC_TEST("osa data: start positions as measured in the running game; unknown controls are present but inert") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    // samsim_game (console map §12): 0-35 km, beam I, ТР, missile selector ОТКЛ., channels СОВМ., ЦЕЛЬ centre.
    SC_EXPECT(c.panel.Get("soc.range") == 1 && c.panel.Get("soc.beam") == 0);
    SC_EXPECT(c.panel.Get("fo.mode") == 1 && c.panel.Find("fo.mode")->position_labels[1] == "ТР");
    SC_EXPECT(c.panel.Get("msl.emergency_select") == 3 && c.panel.Find("msl.emergency_select")->positions == 7);
    SC_EXPECT(c.panel.Get("msl.channels") == 2 && c.panel.Get("ssc.target_select") == 1);
    SC_EXPECT(c.panel.Get("ssc.check_mode") == 1);  // away from РАБОТА
    // СМЕНА ЛИТ. is one stepwise control: the latch opens before the knob turns.
    SC_EXPECT(c.panel.Find("spk.letter")->stepwise && c.panel.IsBlocked("spk.letter", 2));
    SC_EXPECT(c.panel.Find("fo.mode")->positions == 4 && c.panel.Get("veh.hatches") == 1);  // БР ТР РР ФК; closed
    SC_EXPECT(c.panel.IsBlocked("msl.emergency", 1));  // under its guard
    // Elevation travel −12° to +78° (flak11, voenizdat_osa_1991 p66).
    SC_EXPECT_NEAR(c.panel.Find("ssc.el_wheel")->min_value, -12.0, 1e-12);
    SC_EXPECT_NEAR(c.panel.Find("ssc.el_wheel")->max_value, 78.0, 1e-12);
    // Inert: no subsystem depends on them, so working them changes nothing.
    c.panel.Set("pwr.gta", 1);
    for (int i = 0; i < 700; ++i) c.equipment.Update(0.1, c.panel);
    const std::vector<SubsystemTransition> before = c.equipment.TakeTransitions();
    c.panel.Set("msl.emergency_select", 0);
    c.panel.Set("svr.beam", 1);
    c.panel.Set("msl.channels", 0);
    c.panel.Set("ssc.target_select", 0);
    c.panel.Set("ssc.check_mode", 0);
    for (int i = 0; i < 10; ++i) c.equipment.Update(0.1, c.panel);
    SC_EXPECT(c.equipment.TakeTransitions().empty());
}

SC_TEST("osa data: a crew of five; every control and procedure belongs to one seat") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    SC_EXPECT(c.stations.size() == 5);
    for (const char* id : {"commander", "deputy_commander", "senior_operator", "operator", "driver"}) {
        SC_EXPECT(c.FindStation(id) != nullptr);
    }
    for (const ControlDef& def : c.panel.Controls()) SC_EXPECT(c.FindStation(def.station) != nullptr);
    for (const Procedure& p : c.procedures) SC_EXPECT(c.FindStation(p.station) != nullptr);
    // Who does what (flak11, 9A33-3).
    const std::pair<const char*, const char*> owners[] = {
        {"power_up", "driver"},
        {"power_down", "driver"},
        {"equipment_on", "operator"},
        {"equipment_off", "operator"},
        {"deploy_at_halt", "operator"},
        {"lower_search_antenna", "commander"},
        {"raise_search_antenna", "commander"},
        {"pto_on_line", "driver"},
        {"pto_off_line", "driver"},
        {"search_on_march", "senior_operator"},
        {"acquire_with_soc", "senior_operator"},
        {"emcon_tv_azimuth", "senior_operator"},
        {"stop_search", "senior_operator"},
        {"capture_with_ssc", "operator"},
        {"track_range_semi_auto", "operator"},
        {"stow_ssc", "operator"},
        {"ready_capture", "deputy_commander"},
        {"track_low_target", "deputy_commander"},
        {"prepare_firing", "deputy_commander"},
        {"launch", "deputy_commander"},
        {"emcon_tv_engagement", "deputy_commander"},
        {"stow_launch_console", "deputy_commander"}};
    for (const auto& o : owners) {
        const Procedure* p = c.FindProcedure(o.first);
        SC_EXPECT(p != nullptr && p->station == o.second);
    }
    SC_EXPECT(c.panel.Find("ssc.az_wheel")->station == "senior_operator");
    SC_EXPECT(c.panel.Find("ssc.range_wheel")->station == "operator");
    SC_EXPECT(c.panel.Find("ssc.el_wheel")->station == "deputy_commander");
}

SC_TEST("console loader: stations own controls, and a procedure stays at one station") {
    const std::string head = R"({"system_id":"t","display_name":"T",
      "stations":[{"id":"a","label":"A","source":"s","confidence":"low"},{"id":"b","label":"B","source":"s","confidence":"low"}],
      "controls":[{"id":"x","label":"X","station":"a","kind":"switch","source":"s","confidence":"low"},
                  {"id":"y","label":"Y","station":"b","kind":"switch","source":"s","confidence":"low"}])";
    ConsoleData c;
    std::string error;
    SC_EXPECT(LoadConsole(head + R"(,"procedures":[{"id":"p","name":"P","source":"s","confidence":"low",
      "steps":[{"control":"y","position":1,"description":"d"}]}]})", c, &error));
    SC_EXPECT(c.FindProcedure("p")->station == "b" && c.FindStation("a")->label == "A");
    SC_EXPECT(LoadErrorContains(head + R"(,"procedures":[{"id":"p","name":"P","source":"s","confidence":"low",
      "steps":[{"control":"x","position":1,"description":"d"},{"control":"y","position":1,"description":"d"}]}]})",
                                "procedures[0].steps[1]: control 'y' is at station 'b'"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","stations":[{"id":"a","label":"A","source":"s","confidence":"low"}],
      "controls":[{"id":"x","label":"X","station":"c","kind":"switch","source":"s","confidence":"low"}]})",
                                "controls[0].station: unknown station 'c'"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","stations":[{"id":"a","label":"A","source":"s","confidence":"low"}],
      "controls":[{"id":"x","label":"X","kind":"switch","source":"s","confidence":"low"}]})",
                                "controls[0].station: required"));
    SC_EXPECT(LoadErrorContains(R"({"system_id":"t","display_name":"T","stations":[{"id":"a","label":"A","source":"s","confidence":"low"},
      {"id":"a","label":"A2","source":"s","confidence":"low"}]})", "stations[1].id: duplicate station 'a'"));
}

SC_TEST("crew: a crew member refuses another seat's procedure and touches nothing") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    CrewMember driver("Driver", "driver", 1.0);
    driver.Order(*c.FindProcedure("prepare_firing"));
    SC_EXPECT(!driver.IsBusy());
    for (int i = 0; i < 50; ++i) Tick(c, driver);
    SC_EXPECT(c.panel.Get("fo.key") == 0);
    const std::vector<CrewReport> reports = driver.TakeReports();
    SC_EXPECT(reports.size() == 1 && reports[0].kind == CrewReport::Kind::Aborted);
    SC_EXPECT(reports[0].procedure_id == "prepare_firing" && reports[0].detail == "station deputy_commander");
    driver.Queue(*c.FindProcedure("pto_on_line"));  // its own seat's procedure is fine
    SC_EXPECT(driver.IsBusy());
}

SC_TEST("osa data: nothing works without power; transmitters wait for the three-minute relay") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    for (const char* id : {"pwr.equipment", "stab.power", "soc.rotation_lock", "tov.supply", "soc.search", "soc.hv_cover",
                           "soc.hv", "tov.power"}) {
        c.panel.Set(id, 1);
    }
    for (int i = 0; i < 50; ++i) c.equipment.Update(0.1, c.panel);
    SC_EXPECT(!c.equipment.IsReady("soc_antenna") && !c.equipment.IsReady("soc_transmitter"));
    SC_EXPECT(!c.equipment.IsReady("tov_camera") && !AnyFault(c));
    c.panel.Set("pwr.gta", 1);
    for (int i = 0; i < 605; ++i) c.equipment.Update(0.1, c.panel);  // GTA start (60 s, estimate)
    SC_EXPECT(c.equipment.IsReady("power") && c.equipment.IsReady("tov_camera"));
    SC_EXPECT(!c.equipment.IsReady("equipment_warm") && !c.equipment.IsReady("soc_transmitter"));
    for (int i = 0; i < 605; ++i) c.equipment.Update(0.1, c.panel);  // anode supplies after 1 min (voenizdat p165)
    SC_EXPECT(c.equipment.IsReady("equipment_warm") && !c.equipment.IsReady("hv_ready"));
    for (int i = 0; i < 1205; ++i) c.equipment.Update(0.1, c.panel);  // the three-minute relay
    SC_EXPECT(c.equipment.IsReady("hv_ready") && c.equipment.IsReady("soc_transmitter") &&
              c.equipment.IsReady("stabilisation"));
    for (int i = 0; i < 105; ++i) c.equipment.Update(0.1, c.panel);  // the antenna rises (10 s, estimate)
    SC_EXPECT(c.equipment.IsReady("search_antenna") && c.equipment.IsReady("soc_antenna") && !AnyFault(c));
}

SC_TEST("osa data: search on the march; tracking needs a halt with the АПУ unlocked") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    Seats seats(c);
    seats.Order("power_up");
    seats.Order("equipment_on");     // the operator waits for power
    seats.Order("search_on_march");  // the senior operator waits for the raised, stabilised antenna
    const double t = seats.RunUntilIdle();
    seats.Tick();
    SC_EXPECT(c.equipment.IsReady("soc_antenna") && c.equipment.IsReady("soc_transmitter"));  // while moving
    // GTA (60 s, estimate) + stabilisation (3 min) + raising (10 s, estimate): the documented 4 min to readiness.
    SC_EXPECT(t > 245.0 && t < 265.0);
    c.panel.Set("tov.power", 1);
    c.panel.Set("fo.pa", 1);
    seats.Tick();
    seats.Tick();
    SC_EXPECT(!c.equipment.IsReady("tv_angle_track"));  // АПУ still locked for the march
    seats.Order("deploy_at_halt");
    seats.Order("pto_on_line");
    seats.RunUntilIdle(5.0);
    SC_EXPECT(seats.At("operator").Status() == CrewStatus::Waiting);  // both wait for the vehicle to stop
    SC_EXPECT(seats.At("driver").Status() == CrewStatus::Waiting);
    c.equipment.SetInput("vehicle_stopped", true);
    seats.RunUntilIdle();
    seats.Tick();
    SC_EXPECT(c.equipment.IsReady("apu_free") && c.equipment.IsReady("antenna_drives") && c.equipment.IsReady("pto_generator"));
    SC_EXPECT(!AnyFault(c));
}

SC_TEST("osa data: stow for the march, then driving off is safe; driving unlocked faults the АПУ") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    Seats seats(c);
    PowerUpAndDeploy(c, seats);
    SC_EXPECT(c.equipment.IsReady("apu_free"));
    seats.Order("stow_ssc");
    seats.Order("stow_launch_console");
    seats.Order("pto_off_line");
    seats.RunUntilIdle();
    const CrewMember& op = seats.At("operator");
    SC_EXPECT(op.Tracker().GetProcedure().id == "stow_ssc");
    SC_EXPECT(op.Tracker().IsComplete() && op.Tracker().Deviations() == 0);
    seats.Tick();
    SC_EXPECT(c.equipment.State("apu_free") == SubsystemState::Off && c.panel.Get("pwr.pto") == 0);
    c.equipment.SetInput("vehicle_stopped", false);
    seats.Tick();
    SC_EXPECT(!AnyFault(c) && c.equipment.IsReady("power"));  // GTA keeps powering on the move
    SC_EXPECT(!c.equipment.IsReady("pto_generator"));

    c.equipment.SetInput("vehicle_stopped", true);
    seats.Order("deploy_at_halt");
    seats.RunUntilIdle();
    seats.Tick();
    c.equipment.SetInput("vehicle_stopped", false);  // drives off without locking the АПУ
    seats.Tick();
    SC_EXPECT(c.equipment.State("apu_free") == SubsystemState::Fault);
}

SC_TEST("osa data: shutting down stows the search antenna before the GTA stops") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    Seats seats(c);
    PowerUpAndDeploy(c, seats);
    seats.Order("search_on_march");
    seats.RunUntilIdle();
    SC_EXPECT(c.equipment.IsReady("soc_antenna"));
    seats.Order("stop_search");
    seats.Order("pto_off_line");
    seats.Order("equipment_off");  // waits until the senior operator has stopped the SOC
    seats.Queue("power_down");     // the driver waits until the antenna is stowed
    seats.RunUntilIdle();
    for (int i = 0; i < 20; ++i) seats.Tick();
    const CrewMember& op = seats.At("operator");
    SC_EXPECT(op.Tracker().GetProcedure().id == "equipment_off" && op.Tracker().Deviations() == 0);
    SC_EXPECT(seats.At("senior_operator").Tracker().GetProcedure().id == "stop_search");
    SC_EXPECT(seats.At("driver").Tracker().GetProcedure().id == "power_down" && seats.At("driver").Tracker().IsComplete());
    SC_EXPECT(c.equipment.State("search_antenna") == SubsystemState::Off && c.panel.Get("pwr.gta") == 0);
    SC_EXPECT(!c.equipment.IsReady("power") && !AnyFault(c));
}

SC_TEST("osa data: three operators acquire with the SOC, then the SSC locks on when the target is captured") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    Seats seats(c);
    PowerUpAndDeploy(c, seats);
    seats.Order("acquire_with_soc");
    seats.Order("ready_capture");
    seats.Order("capture_with_ssc");  // waits for ЦУ АЗ before the transmitter goes on
    seats.RunUntilIdle(100.0);
    for (const char* station : {"senior_operator", "deputy_commander", "operator"}) {
        SC_EXPECT(seats.At(station).Tracker().IsComplete() && seats.At(station).Tracker().Deviations() == 0);
    }
    seats.Tick();
    SC_EXPECT(c.equipment.IsReady("soc_transmitter") && c.equipment.IsReady("ssc_transmitter"));
    SC_EXPECT(c.equipment.IsReady("auto_acquisition") && c.equipment.IsReady("auto_capture"));  // ЦУ arms АЗ too
    SC_EXPECT(!c.equipment.IsReady("auto_tracking"));
    c.equipment.SetInput("target_captured", true);
    seats.Tick();
    SC_EXPECT(c.equipment.IsReady("auto_tracking"));
}

SC_TEST("osa data: prepare, then launch waits for ПУСК ГОТОВ and ЦЕЛЬ В ЗОНЕ") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    Seats seats(c);
    PowerUpAndDeploy(c, seats);
    seats.Order("prepare_firing");
    seats.Queue("launch");
    const CrewMember& crew = seats.At("deputy_commander");
    double t = 0.0;
    auto run = [&](double seconds) {
        for (double end = t + seconds; t < end - 1e-9; t += 0.1) seats.Tick();
    };
    run(20.0);
    SC_EXPECT(crew.Tracker().GetProcedure().id == "launch" && crew.Status() == CrewStatus::Waiting);
    SC_EXPECT(c.panel.Get("fo.key") == 1);
    c.equipment.SetInput("target_near_zone", true);  // ВНИМАНИЕ: missiles selected, transmitters on (flak11)
    run(10.0);
    SC_EXPECT(c.equipment.State("missile_prep") == SubsystemState::Off);  // preparation waits for the zone
    c.equipment.SetInput("target_in_zone", true);
    run(10.0);
    SC_EXPECT(c.equipment.State("missile_prep") == SubsystemState::Starting && crew.IsBusy());
    bool launched = false;
    for (int i = 0; i < 100 && !launched; ++i) {
        seats.Tick();
        launched = c.panel.Get("fo.launch") == 1;
    }
    SC_EXPECT(launched && c.equipment.IsReady("launch_armed"));
    SC_EXPECT(crew.Tracker().IsComplete() && crew.Tracker().Deviations() == 0);
}

SC_TEST("osa data: TV-only engagement with the radars silent") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    Seats seats(c);
    PowerUpAndDeploy(c, seats);
    seats.Order("emcon_tv_azimuth");
    seats.Order("emcon_tv_engagement");
    CrewMember& crew = seats.At("deputy_commander");
    bool launched = false;
    double t = 0.0;
    for (; t < 120.0 && !launched; t += 0.1) {
        seats.Tick();
        launched = c.panel.Get("fo.launch") == 1;
    }
    SC_EXPECT(seats.At("senior_operator").Tracker().IsComplete() && c.panel.Get("ssc.az_lock") == 1);
    SC_EXPECT(launched && crew.Tracker().Deviations() == 0);
    SC_EXPECT(c.equipment.IsReady("tv_angle_track"));
    SC_EXPECT(!c.equipment.IsReady("soc_transmitter") && !c.equipment.IsReady("ssc_transmitter"));
    SC_EXPECT(t > 15.0 && t < 30.0);  // ~7 s of switches, 13 s preparation, then key and launch
    SC_EXPECT(crew.TakeReports().back().kind == CrewReport::Kind::Completed);
}

SC_TEST("debrief: crew reports, faults and player deviations on one timeline") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    CrewMember crew("Operator", "operator", 1.0);
    SessionLog log;
    double t = 0.0;
    auto step = [&] {
        t += 0.1;
        Tick(c, crew);
        log.CollectEquipment(c.equipment);
        log.CollectCrew(crew, t);
    };
    c.equipment.SetInput("vehicle_stopped", true);
    crew.Order(*c.FindProcedure("deploy_at_halt"));
    for (int i = 0; i < 50; ++i) step();
    c.equipment.SetInput("vehicle_stopped", false);  // drives off with the АПУ unlocked
    step();

    ProcedureTracker player(*c.FindProcedure("launch"));
    c.panel.Set("fo.key", 1);
    log.RecordAttempt(t, "Player", player, player.Attempt(c.panel, "fo.key", 2, &c.equipment));   // early
    log.RecordAttempt(t, "Player", player, player.Attempt(c.panel, "tov.power", 1, &c.equipment));  // out of order
    log.RecordAttempt(t, "Player", player, player.Attempt(c.panel, "fo.launch", 1, &c.equipment));  // early: not armed
    log.RecordAttempt(t, "Player", player, player.Attempt(c.panel, "fo.launch", 1, &c.equipment));  // done: not logged

    const DebriefSummary s = log.Summary();
    SC_EXPECT(s.procedures_completed == 1 && s.equipment_faults == 1);
    SC_EXPECT(s.early_actions == 2 && s.out_of_order_actions == 1);
    const std::string text = log.Format();
    SC_EXPECT(text.find("FAULT") != std::string::npos && text.find("apu_free") != std::string::npos);
    SC_EXPECT(text.find("step due: ПУСК") != std::string::npos);
    SC_EXPECT(text.find("2 early, 1 out of order") != std::string::npos);
}

SC_TEST("osa data: a player who turns the key before ПУСК ГОТОВ is judged early") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    ProcedureTracker tr(*c.FindProcedure("launch"));
    c.panel.Set("fo.key", 1);
    SC_EXPECT(tr.Attempt(c.panel, "fo.key", 2, &c.equipment) == StepResult::AcceptedEarly);
    SC_EXPECT(tr.DeviationLog().front().kind == DeviationKind::Early);
}
