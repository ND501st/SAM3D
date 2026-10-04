#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include "simcore/cabin.hpp"
#include "simcore/console_data.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

bool LoadOsaConsole(ConsoleData& console) { return LoadConsoleFile(kData + "/procedures/osa_ak.json", console); }

// The Osa console's five seats, at made-up eye points.
const char* kSeats = R"("stations": [ { "id": "senior_operator", "label": "SO", "eye_m": [0, 0, 1.5], "yaw_deg": -90 },
                        { "id": "operator", "label": "O", "eye_m": [0, 1, 1.5] },
                        { "id": "deputy_commander", "label": "DC", "eye_m": [1, 0, 1.5] },
                        { "id": "commander", "label": "C", "eye_m": [1, 3, 1.5] },
                        { "id": "driver", "label": "D", "eye_m": [-1, 3, 1.5] } ])";

// A small valid layout on the Osa console; `extra` is spliced in after the panels.
std::string Small(const std::string& extra = "") {
    return R"({
      "system_id": "osa_ak", )" + std::string(kSeats) + R"(,
      "panels": [ { "id": "p", "label": "P", "station": "senior_operator", "centre_m": [-1, 0, 1.3],
                    "facing_deg": 90, "tilt_deg": 0, "size_m": [1.0, 0.6] } ])" +
           extra + "}";
}

bool ErrorContains(const std::string& text, const std::string& expected) {
    ConsoleData console;
    if (!LoadOsaConsole(console)) return false;
    CabinLayout layout;
    std::string error;
    if (LoadCabin(text, console, layout, &error)) return false;
    if (error.find(expected) == std::string::npos) std::printf("    got: %s\n", error.c_str());
    return error.find(expected) != std::string::npos;
}

bool Near(const Vec3& a, const Vec3& b, double tol = 1e-9) { return (a - b).Length() <= tol; }

}  // namespace

SC_TEST("cabin: panel axes match hand calculations") {
    CabinPanel p;
    p.centre_m = Vec3{0.0, 4.0, 1.0};
    // Facing the rear (an operator looking forward at it), upright: right is +x, up is +z.
    p.facing_rad = DegToRad(180.0);
    SC_EXPECT(Near(p.Normal(), Vec3{0.0, -1.0, 0.0}));
    SC_EXPECT(Near(p.Right(), Vec3{1.0, 0.0, 0.0}));
    SC_EXPECT(Near(p.Up(), Vec3{0.0, 0.0, 1.0}));
    SC_EXPECT(Near(p.PointAt(0.2, 0.1), Vec3{0.2, 4.0, 1.1}));
    // Facing right, on the left wall (operator looks left): their right is forward.
    p.facing_rad = DegToRad(90.0);
    SC_EXPECT(Near(p.Right(), Vec3{0.0, 1.0, 0.0}));
    // A desk (tilt 90) facing the rear: normal up, "up the face" points away from the operator (forward).
    p.facing_rad = DegToRad(180.0);
    p.tilt_rad = DegToRad(90.0);
    SC_EXPECT(Near(p.Normal(), Vec3{0.0, 0.0, 1.0}));
    SC_EXPECT(Near(p.Up(), Vec3{0.0, 1.0, 0.0}));
    // Tilted 30 degrees back: up the face = (0, sin 30, cos 30).
    p.tilt_rad = DegToRad(30.0);
    SC_EXPECT(Near(p.Up(), Vec3{0.0, 0.5, std::sqrt(3.0) / 2.0}));
    p.width_m = 0.4;
    p.height_m = 0.2;
    SC_EXPECT(p.Contains(0.2, -0.1) && !p.Contains(0.21, 0.0) && !p.Contains(0.0, 0.11));
}

SC_TEST("cabin: default styles and click targets follow the control") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    const ControlPanel& panel = console.panel;
    SC_EXPECT(DefaultCabinStyle(*panel.Find("soc.search"), panel) == CabinStyle::Toggle);
    SC_EXPECT(DefaultCabinStyle(*panel.Find("soc.hv_cover"), panel) == CabinStyle::Cover);
    SC_EXPECT(DefaultCabinStyle(*panel.Find("soc.range"), panel) == CabinStyle::Rotary);
    SC_EXPECT(DefaultCabinStyle(*panel.Find("fo.key"), panel) == CabinStyle::Key);
    SC_EXPECT(DefaultCabinStyle(*panel.Find("fo.launch"), panel) == CabinStyle::Button);
    SC_EXPECT(DefaultCabinStyle(*panel.Find("ssc.az_wheel"), panel) == CabinStyle::Wheel);
    // A lock that only guards a wheel is a lever, not a cover.
    SC_EXPECT(DefaultCabinStyle(*panel.Find("ssc.az_lock"), panel) == CabinStyle::Toggle);

    SC_EXPECT(CabinClickTarget(*panel.Find("soc.search"), 0, -1) == 1);  // toggles flip either way
    SC_EXPECT(CabinClickTarget(*panel.Find("soc.search"), 1, 1) == 0);
    SC_EXPECT(CabinClickTarget(*panel.Find("soc.range"), 1, 1) == 2);   // rotaries step
    SC_EXPECT(CabinClickTarget(*panel.Find("soc.range"), 2, 1) == 2);   // and stop at the end
    SC_EXPECT(CabinClickTarget(*panel.Find("soc.range"), 0, -1) == 0);
    SC_EXPECT(CabinClickTarget(*panel.Find("fo.launch"), 0, -1) == 1);  // buttons press
    SC_EXPECT(CabinClickTarget(*panel.Find("ssc.az_wheel"), 0, 1) == -1);
}

SC_TEST("cabin: red dots mark a control's working position, only where the layout gives one") {
    // Hand calculations: toggles point up for on, down for off; a 3-way rotary spreads 30 degrees apart about up,
    // a 5-way one too (240 / 4 = 60 is capped at 30).
    SC_EXPECT_NEAR(CabinLayout::PositionAngleDeg(CabinStyle::Toggle, 2, 0), 180.0, 1e-12);
    SC_EXPECT_NEAR(CabinLayout::PositionAngleDeg(CabinStyle::Toggle, 2, 1), 0.0, 1e-12);
    SC_EXPECT_NEAR(CabinLayout::PositionAngleDeg(CabinStyle::Rotary, 3, 0), -30.0, 1e-12);
    SC_EXPECT_NEAR(CabinLayout::PositionAngleDeg(CabinStyle::Rotary, 5, 4), 60.0, 1e-12);
    SC_EXPECT_NEAR(CabinLayout::PositionAngleDeg(CabinStyle::Rotary, 9, 0), -120.0, 1e-12);  // 240 / 8 = 30
    SC_EXPECT_NEAR(CabinLayout::PositionAngleDeg(CabinStyle::Key, 3, 2), 90.0, 1e-12);

    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    double du = 0.0;
    double dv = 0.0;
    // РОД РАБОТЫ starts at ТР (position 1 of 4) but its dot is at БР (position 0): 45 degrees left of up.
    SC_EXPECT(console.panel.Find("fo.mode")->initial == 1);
    SC_EXPECT(CabinLayout::RedDot(*layout.FindControl("fo.mode"), *console.panel.Find("fo.mode"), du, dv));
    SC_EXPECT_NEAR(du, -0.032 * std::sin(DegToRad(45.0)), 1e-12);
    SC_EXPECT_NEAR(dv, 0.032 * std::cos(DegToRad(45.0)), 1e-12);
    // КАНАЛЫ starts on its dot at СОВМ. (position 2 of 3): 30 degrees right of up.
    SC_EXPECT(console.panel.Find("msl.channels")->initial == 2);
    SC_EXPECT(CabinLayout::RedDot(*layout.FindControl("msl.channels"), *console.panel.Find("msl.channels"), du, dv));
    SC_EXPECT_NEAR(du, 0.016, 1e-12);
    SC_EXPECT_NEAR(dv, 0.032 * std::cos(DegToRad(30.0)), 1e-12);
    // РАБОТА / КОНТРОЛЬ starts at КОНТРОЛЬ; the dot is at РАБОТА (position 0 of 3): 30 degrees left of up.
    SC_EXPECT(CabinLayout::RedDot(*layout.FindControl("ssc.check_mode"), *console.panel.Find("ssc.check_mode"), du, dv));
    SC_EXPECT_NEAR(du, -0.016, 1e-12);
    // No dot where none is known (ДАЛЬНОСТЬ has none on the photos), nor on keys, buttons, wheels and covers.
    for (const char* none : {"soc.range", "soc.search", "fo.key", "fo.launch", "ssc.az_wheel", "soc.hv_cover"}) {
        SC_EXPECT(!CabinLayout::RedDot(*layout.FindControl(none), *console.panel.Find(none), du, dv));
    }
}

SC_TEST("cabin: the Osa layout loads and places every control of the console") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(layout.stations.size() == 5 && layout.panels.size() == 7);
    for (const std::string& id : layout.UnplacedControls(console.panel)) std::printf("    unplaced: %s\n", id.c_str());
    SC_EXPECT(layout.UnplacedControls(console.panel).empty());
    // Covers nobody placed sit over the control they guard.
    const CabinControl* cover = layout.FindControl("soc.hv_cover");
    const CabinControl* hv = layout.FindControl("soc.hv");
    SC_EXPECT(cover != nullptr && hv != nullptr && cover->auto_placed && cover->style == CabinStyle::Cover);
    SC_EXPECT(cover != nullptr && hv != nullptr && Near(layout.PositionOf(*cover), layout.PositionOf(*hv)));
    const CabinControl* wheel = layout.FindControl("ssc.az_wheel");
    SC_EXPECT(wheel != nullptr && wheel->style == CabinStyle::Wheel);
    SC_EXPECT_NEAR(wheel != nullptr ? wheel->units_per_turn : 0.0, 60.0, 1e-12);
}

SC_TEST("cabin: the Osa operators share the left-wall bench, guidance in front, then search, then range") {
    // mon_2112_80 Fig. 3 (cabin plan) and the WZU crew photos (grezicki_wzu Fig. 4).
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const char* bench[] = {"deputy_commander", "senior_operator", "operator"};
    const char* consoles[] = {"fo_panel", "so_panel", "op_panel"};
    double previous_y = 1e9;
    for (int i = 0; i < 3; ++i) {
        const CabinStation* seat = layout.FindStation(bench[i]);
        const CabinPanel* panel = layout.FindPanel(consoles[i]);
        SC_EXPECT(seat != nullptr && panel != nullptr);
        if (seat == nullptr || panel == nullptr) return;
        SC_EXPECT(seat->eye_m.x < 0.0 && panel->centre_m.x < seat->eye_m.x);    // left wall, panel in front of the seat
        SC_EXPECT_NEAR(seat->yaw_rad, DegToRad(-90.0), 1e-12);                  // facing the wall
        SC_EXPECT_NEAR(panel->centre_m.y, seat->eye_m.y, 1e-9);                 // panel straight ahead
        SC_EXPECT(seat->eye_m.y < previous_y);                                   // front to rear
        previous_y = seat->eye_m.y;
    }
    // The three columns of the main console don't overlap along the wall.
    for (int i = 0; i + 1 < 3; ++i) {
        const CabinPanel* front = layout.FindPanel(consoles[i]);
        const CabinPanel* rear = layout.FindPanel(consoles[i + 1]);
        SC_EXPECT(front->centre_m.y - 0.5 * front->width_m >= rear->centre_m.y + 0.5 * rear->width_m);
    }
    // Each column is a tall stack of blocks (samsim_panels, cabin photos): the screen above the hand wheel, and in the front column
    // the launch block between the TV block and the elevation block.
    auto v_of = [&](const char* id) {
        for (const CabinControl& c : layout.controls) {
            if (c.control_id == id) return c.v_m;
        }
        for (const CabinDisplay& d : layout.displays) {
            if (d.id == id) return d.v_m;
        }
        return 0.0;
    };
    for (const char* id : consoles) {
        const CabinPanel* panel = layout.FindPanel(id);
        SC_EXPECT(panel->height_m > panel->width_m);
    }
    SC_EXPECT(v_of("tov_monitor") > v_of("msl.emergency") && v_of("msl.emergency") > v_of("ssc.el_wheel"));
    SC_EXPECT(v_of("soc_ppi") > v_of("ssc.az_wheel"));
    SC_EXPECT(v_of("ssc_range") > v_of("ssc.range_wheel"));
    // Driver front left, commander front right.
    SC_EXPECT(layout.FindStation("driver")->eye_m.x < 0.0 && layout.FindStation("commander")->eye_m.x > 0.0);
}

SC_TEST("cabin: lamps light from subsystem states and inputs") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const CabinLamp* in_zone = nullptr;
    const CabinLamp* gta = nullptr;
    for (const CabinLamp& l : layout.lamps) {
        if (l.id == "in_zone") in_zone = &l;
        if (l.id == "gta_on") gta = &l;
    }
    SC_EXPECT(in_zone != nullptr && gta != nullptr);
    if (in_zone == nullptr || gta == nullptr) return;
    SC_EXPECT(!CabinLayout::LampLit(*in_zone, console.panel, console.equipment));
    console.equipment.SetInput("target_in_zone", true);
    SC_EXPECT(CabinLayout::LampLit(*in_zone, console.panel, console.equipment));

    SC_EXPECT(!CabinLayout::LampLit(*gta, console.panel, console.equipment));
    console.panel.Set("pwr.gta", 1);
    for (int i = 0; i < 3000 && !console.equipment.IsReady("gta"); ++i) console.equipment.Update(0.1, console.panel);
    SC_EXPECT(CabinLayout::LampLit(*gta, console.panel, console.equipment));
}

SC_TEST("cabin: ЦУ lights ЦУ and АЗ and puts out РУ; РУ brings it back; РУ needs the hatches closed") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    auto lit = [&](const char* id) {
        for (const CabinLamp& l : layout.lamps) {
            if (l.id == id) return CabinLayout::LampLit(l, console.panel, console.equipment);
        }
        return false;
    };
    auto step = [&] {
        console.equipment.Update(0.1, console.panel);
        console.panel.ReleaseMomentaries();
    };
    console.panel.Set("pwr.gta", 1);
    console.panel.Set("pwr.equipment", 1);
    for (int i = 0; i < 3000 && !console.equipment.IsReady("hv_ready"); ++i) step();
    SC_EXPECT(lit("ru") && !lit("cu") && !lit("az") && lit("beam_1") && lit("soc_gotov") && !lit("br"));
    SC_EXPECT(lit("stowed") && lit("soc_rot_locked") && lit("apu_locked") && lit("gotov_spk_1"));
    console.panel.Set("fo.cu_az", 1);
    step();
    step();
    SC_EXPECT(!lit("ru") && lit("cu") && lit("az"));
    console.panel.Set("fo.az", 1);  // already armed by ЦУ
    step();
    SC_EXPECT(lit("cu") && lit("az"));
    console.panel.Set("fo.ru", 1);
    step();
    step();
    SC_EXPECT(lit("ru") && !lit("cu") && !lit("az"));
    console.panel.Set("fo.az", 1);  // АЗ alone arms auto-capture (voenizdat p99)
    step();
    step();
    SC_EXPECT(lit("az") && !lit("cu") && !lit("ru"));
    console.panel.Set("fo.ru", 1);
    step();
    step();
    console.panel.Set("veh.hatches", 0);  // РУ only lights with the hatches closed (voenizdat p99)
    step();
    SC_EXPECT(!lit("ru") && !lit("luki_zakr"));
    console.panel.Set("veh.hatches", 1);
    console.panel.Set("fo.mode", 0);  // РОД РАБОТЫ to БР
    step();
    SC_EXPECT(lit("br") && lit("ru"));
}

SC_TEST("cabin: the loader rejects bad layouts with a path to the problem") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabin(Small(), console, layout));

    SC_EXPECT(ErrorContains(R"({ "system_id": "shilka", "stations": [], "panels": [] })", "system_id"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "nope", "panel": "p", "at_m": [0, 0] } ])"),
                            "controls[0].control: unknown control 'nope'"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.mti", "panel": "q", "at_m": [0, 0] } ])"),
                            "controls[0].panel: unknown panel 'q'"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0.6, 0] } ])"),
                            "controls[0].at_m: outside panel"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0, 0] },
                                                     { "control": "soc.mti", "panel": "p", "at_m": [0.1, 0] } ])"),
                            "placed twice"));
    // fo.launch belongs to the deputy commander, not the senior operator's panel.
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "fo.launch", "panel": "p", "at_m": [0, 0] } ])"),
                            "belongs to station 'deputy_commander'"));
    // One seat per console station, no more, no fewer.
    std::string missing = Small();
    missing.erase(missing.find(R"(,
                        { "id": "driver")"), std::string(R"(,
                        { "id": "driver", "label": "D", "eye_m": [-1, 3, 1.5] })").size());
    SC_EXPECT(ErrorContains(missing, "stations: no seat for the console's 'driver'"));
    std::string extra_seat = Small();
    extra_seat.replace(extra_seat.find(R"("id": "driver")"), 14, R"("id": "loader")");
    SC_EXPECT(ErrorContains(extra_seat, "stations[4].id: 'loader' is not a station of the console"));
    SC_EXPECT(ErrorContains(
        Small(R"(, "controls": [ { "control": "soc.range", "panel": "p", "at_m": [0, 0], "style": "toggle" } ])"),
        "doesn't fit"));
    SC_EXPECT(ErrorContains(
        Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0, 0], "units_per_turn": 5 } ])"),
        "for wheels only"));
    SC_EXPECT(ErrorContains(
        Small(R"(, "controls": [ { "control": "soc.range", "panel": "p", "at_m": [0, 0], "red_dot": 3 } ])"),
        "controls[0].red_dot: not a position of 'soc.range'"));
    SC_EXPECT(ErrorContains(
        Small(R"(, "controls": [ { "control": "ssc.az_wheel", "panel": "p", "at_m": [0, 0], "red_dot": 0 } ])"),
        "for toggles and rotaries only"));
    SC_EXPECT(LoadCabin(Small(R"(, "controls": [ { "control": "soc.range", "panel": "p", "at_m": [0, 0], "red_dot": 1 } ])"),
                        console, layout));
    SC_EXPECT(ErrorContains(Small(R"(, "lamps": [ { "id": "l", "label": "L", "panel": "p", "at_m": [0, 0],
        "color": "blue", "lit_when": [ { "input": "target_in_zone", "is": true } ], "source": "s", "confidence": "high" } ])"),
                            "lamps[0].color"));
    SC_EXPECT(ErrorContains(Small(R"(, "lamps": [ { "id": "l", "label": "L", "panel": "p", "at_m": [0, 0],
        "color": "red", "lit_when": [ { "input": "nope", "is": true } ], "source": "s", "confidence": "high" } ])"),
                            "unknown input 'nope'"));
    SC_EXPECT(ErrorContains(Small(R"(, "lamps": [ { "id": "l", "label": "L", "panel": "p", "at_m": [0, 0],
        "color": "red", "lit_when": [ { "input": "target_in_zone", "is": true } ] } ])"),
                            "lamps[0].source"));
    SC_EXPECT(ErrorContains(Small(R"(, "lamps": [ { "id": "l", "label": "L", "panel": "p", "at_m": [0, 0],
        "color": "red", "source": "s", "confidence": "high" } ])"),
                            "needs lit_when"));
}

SC_TEST("cabin: player moves go through a procedure tracker into the debrief") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    def.triggers.clear();  // no crew orders: only the player acts
    ScenarioRunner runner(std::move(def), std::move(console));
    runner.Step(0.1);

    // Free play: only physical blocks apply.
    SC_EXPECT(runner.PlayerSet("soc.hv", 1) == StepResult::Blocked);  // cover closed
    SC_EXPECT(runner.Console().panel.Get("soc.hv") == 0);
    SC_EXPECT(runner.PlayerSet("soc.hv_cover", 1) == StepResult::ProcedureComplete);
    SC_EXPECT(runner.Console().panel.Get("soc.hv_cover") == 1);
    SC_EXPECT(runner.PlayerTurn("ssc.az_wheel", 10.0) == false);  // azimuth lock closed
    SC_EXPECT(runner.SetPlayerProcedure("nope") == false);

    // Searching on the march needs power first: switching ПОИСК on now is early, skipping a step is out of order.
    SC_EXPECT(runner.SetPlayerProcedure("search_on_march"));
    SC_EXPECT(runner.PlayerSet("soc.search", 1) == StepResult::AcceptedEarly);
    SC_EXPECT(runner.PlayerSet("soc.mti", 1) == StepResult::NotNextStep);
    int deviations = 0;
    for (const DebriefEntry& e : runner.Log().Entries()) {
        if (e.kind == DebriefEntry::Kind::Deviation && e.who == ScenarioRunner::kPlayerName) ++deviations;
    }
    SC_EXPECT(deviations == 2);

    // A one-step procedure done right is reported complete.
    SC_EXPECT(runner.SetPlayerProcedure("lower_search_antenna"));
    SC_EXPECT(runner.PlayerSet("apu.stow", 0) == StepResult::Accepted);
    bool completed = false;
    for (const DebriefEntry& e : runner.Log().Entries()) {
        completed = completed || (e.kind == DebriefEntry::Kind::Completed && e.who == ScenarioRunner::kPlayerName &&
                                  e.what == "lower_search_antenna");
    }
    SC_EXPECT(completed);
}

SC_TEST("cabin: blocks, fixtures and looks load and are checked") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    const std::string ok = Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0, 0],
                                                      "look": { "kind": "toggle", "size_m": 0.014, "color": "chrome" } } ],
          "blocks": [ { "id": "b", "panel": "p", "label": "ОП63-6М2", "at_m": [0, 0], "size_m": [0.9, 0.5] } ],
          "fixtures": [ { "panel": "p", "at_m": [0.1, 0.1], "kind": "lamp_button", "size_m": 0.044, "color": "red",
                          "ring": "yellow", "label": "ТСЦ" },
                        { "panel": "p", "at_m": [0.2, 0.1], "kind": "nameplate", "size_m": [0.05, 0.013], "label": "ОС81-9М" } ])");
    std::string error;
    SC_EXPECT(LoadCabin(ok, console, layout, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(layout.blocks.size() == 1 && layout.fixtures.size() == 2);
    SC_EXPECT(layout.fixtures.size() == 2 && layout.fixtures[0].look.kind == CabinLookKind::LampButton &&
              layout.fixtures[0].look.ring == "yellow");
    SC_EXPECT_NEAR(layout.fixtures.empty() ? 0.0 : layout.fixtures[0].look.width_m, 0.044, 1e-12);
    SC_EXPECT(!layout.controls.empty() && layout.controls[0].look.kind == CabinLookKind::Toggle);
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "gizmo", "size_m": 0.02 } ])"),
                            "fixtures[0].kind: unknown look 'gizmo'"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "knob" } ])"),
                            "fixtures[0].size_m: required"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "knob", "size_m": 0.02, "color": "purple" } ])"),
                            "fixtures[0].color: unknown colour 'purple'"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "nameplate", "size_m": [0.05, 0.01] } ])"),
                            "fixtures[0].label: required"));
    SC_EXPECT(ErrorContains(Small(R"(, "blocks": [ { "id": "b", "panel": "p", "label": "X", "at_m": [0, 0], "size_m": [1.5, 0.5] } ])"),
                            "blocks[0].at_m: block doesn't fit"));
    // The Osa layout has every column's blocks and fixtures from SAM Simulator's panel photos.
    CabinLayout osa;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, osa));
    int per_panel[3] = {0, 0, 0};
    for (const CabinFixture& f : osa.fixtures) {
        per_panel[0] += f.panel_id == "op_panel";
        per_panel[1] += f.panel_id == "so_panel";
        per_panel[2] += f.panel_id == "fo_panel";
    }
    SC_EXPECT(per_panel[0] > 50 && per_panel[1] > 50 && per_panel[2] > 50);
    SC_EXPECT(osa.blocks.size() >= 15);
    // The shell: floor, walls, bench and racks, two dome lamps.
    int lights = 0;
    for (const CabinShellPart& part : osa.shell) lights += part.kind == CabinShellKind::Light;
    SC_EXPECT(osa.shell.size() >= 15 && lights == 2);
    SC_EXPECT(ErrorContains(Small(R"(, "shell": [ { "id": "w", "label": "W", "kind": "wall", "centre_m": [0, 0, 1],
                                                   "size_m": [1, 1, 1], "source": "s", "confidence": "low" } ])"),
                            "shell[0].kind: must be box, glass or light"));
    SC_EXPECT(ErrorContains(Small(R"(, "shell": [ { "id": "w", "label": "W", "kind": "box", "centre_m": [0, 0, 1],
                                                   "size_m": [1, 0, 1], "source": "s", "confidence": "low" } ])"),
                            "shell[0].size_m: must be positive"));
}
