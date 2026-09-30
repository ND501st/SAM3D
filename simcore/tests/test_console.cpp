#include <string>
#include <vector>

#include "simcore/console_data.hpp"
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
    for (const char* id : {"acquire_with_soc", "track_low_target", "prepare_firing", "launch", "emcon_tv_engagement"}) {
        SC_EXPECT(c.FindProcedure(id) != nullptr);
    }
}

SC_TEST("osa data: crew acquires with the SOC, then the SSC locks on when the target is captured") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    CrewMember crew("Range officer", "range_officer", 1.0);
    crew.Order(*c.FindProcedure("acquire_with_soc"));
    for (int i = 0; i < 1000 && crew.IsBusy(); ++i) Tick(c, crew);
    SC_EXPECT(crew.Tracker().IsComplete() && crew.Tracker().Deviations() == 0);
    Tick(c, crew);
    SC_EXPECT(c.equipment.IsReady("soc_transmitter") && c.equipment.IsReady("ssc_transmitter"));
    SC_EXPECT(c.equipment.IsReady("auto_acquisition"));  // latched by the ЦУ АЗ press
    SC_EXPECT(!c.equipment.IsReady("auto_tracking"));
    c.equipment.SetInput("target_captured", true);
    Tick(c, crew);
    SC_EXPECT(c.equipment.IsReady("auto_tracking"));
}

SC_TEST("osa data: prepare, then launch waits for ПУСК ГОТОВ and ЦЕЛЬ В ЗОНЕ") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    CrewMember crew("Firing officer", "firing_officer", 1.0);
    crew.Order(*c.FindProcedure("prepare_firing"));
    crew.Queue(*c.FindProcedure("launch"));
    double t = 0.0;
    auto run = [&](double seconds) {
        for (double end = t + seconds; t < end - 1e-9; t += 0.1) Tick(c, crew);
    };
    run(20.0);
    SC_EXPECT(crew.Tracker().GetProcedure().id == "launch" && crew.Status() == CrewStatus::Waiting);
    SC_EXPECT(c.panel.Get("fo.key") == 1);
    c.equipment.SetInput("target_near_zone", true);  // ВНИМАНИЕ starts missile preparation
    run(10.0);
    SC_EXPECT(c.equipment.State("missile_prep") == SubsystemState::Starting);
    run(6.0);
    SC_EXPECT(c.equipment.IsReady("missile_prep") && crew.IsBusy());  // ПУСК ГОТОВ, but not yet in the zone
    c.equipment.SetInput("target_in_zone", true);
    bool launched = false;
    for (int i = 0; i < 100 && !launched; ++i) {
        Tick(c, crew);
        launched = c.panel.Get("fo.launch") == 1;
    }
    SC_EXPECT(launched && c.equipment.IsReady("launch_armed"));
    SC_EXPECT(crew.Tracker().IsComplete() && crew.Tracker().Deviations() == 0);
}

SC_TEST("osa data: TV-only engagement with the radars silent") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    CrewMember crew("Firing officer", "firing_officer", 1.0);
    crew.Order(*c.FindProcedure("emcon_tv_engagement"));
    bool launched = false;
    double t = 0.0;
    for (; t < 120.0 && !launched; t += 0.1) {
        Tick(c, crew);
        launched = c.panel.Get("fo.launch") == 1;
    }
    SC_EXPECT(launched && crew.Tracker().Deviations() == 0);
    SC_EXPECT(c.equipment.IsReady("tv_angle_track"));
    SC_EXPECT(!c.equipment.IsReady("soc_transmitter") && !c.equipment.IsReady("ssc_transmitter"));
    SC_EXPECT(t > 20.0 && t < 35.0);  // ~9 s of switches, 15 s preparation, then key and launch
    SC_EXPECT(crew.TakeReports().back().kind == CrewReport::Kind::Completed);
}

SC_TEST("osa data: a player who turns the key before ПУСК ГОТОВ is judged early") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(OsaPath(), c));
    ProcedureTracker tr(*c.FindProcedure("launch"));
    c.panel.Set("fo.key", 1);
    SC_EXPECT(tr.Attempt(c.panel, "fo.key", 2, &c.equipment) == StepResult::AcceptedEarly);
    SC_EXPECT(tr.DeviationLog().front().kind == DeviationKind::Early);
}
