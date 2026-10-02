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

SC_TEST("cabin: red dots mark each toggle's and rotary's starting position") {
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
    // ПОИСК starts off: the dot is below the switch.
    SC_EXPECT(CabinLayout::StartMark(*layout.FindControl("soc.search"), *console.panel.Find("soc.search"), du, dv));
    SC_EXPECT_NEAR(du, 0.0, 1e-12);
    SC_EXPECT_NEAR(dv, -0.03, 1e-12);
    // РОД РАБОТЫ starts at "other" (position 1 of 2): up.
    SC_EXPECT(CabinLayout::StartMark(*layout.FindControl("fo.mode"), *console.panel.Find("fo.mode"), du, dv));
    SC_EXPECT_NEAR(dv, 0.03, 1e-12);
    // ДАЛЬНОСТЬ starts at 0-15 km (position 0 of 3): 30 degrees left of up.
    SC_EXPECT(CabinLayout::StartMark(*layout.FindControl("soc.range"), *console.panel.Find("soc.range"), du, dv));
    SC_EXPECT_NEAR(du, -0.016, 1e-12);
    SC_EXPECT_NEAR(dv, 0.032 * std::cos(DegToRad(30.0)), 1e-12);
    // ЛУЧ starts at I (position 0 of 5): 60 degrees left of up.
    SC_EXPECT(CabinLayout::StartMark(*layout.FindControl("soc.beam"), *console.panel.Find("soc.beam"), du, dv));
    SC_EXPECT_NEAR(du, -0.032 * std::sin(DegToRad(60.0)), 1e-12);
    SC_EXPECT_NEAR(dv, 0.032 * std::cos(DegToRad(60.0)), 1e-12);
    for (const char* none : {"fo.key", "fo.launch", "ssc.az_wheel", "soc.hv_cover"}) {
        SC_EXPECT(!CabinLayout::StartMark(*layout.FindControl(none), *console.panel.Find(none), du, dv));
    }
}

SC_TEST("cabin: the Osa layout loads and places every control of the console") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(layout.stations.size() == 5 && layout.panels.size() == 5);
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
    SC_EXPECT(runner.SetPlayerProcedure("power_up"));
    SC_EXPECT(runner.PlayerSet("pwr.gta", 1) == StepResult::Accepted);
    bool completed = false;
    for (const DebriefEntry& e : runner.Log().Entries()) {
        completed = completed || (e.kind == DebriefEntry::Kind::Completed && e.who == ScenarioRunner::kPlayerName &&
                                  e.what == "power_up");
    }
    SC_EXPECT(completed);
}
