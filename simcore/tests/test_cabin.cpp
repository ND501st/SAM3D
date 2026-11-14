#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include "simcore/cabin.hpp"
#include "simcore/console_data.hpp"
#include "simcore/readouts.hpp"
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
    // A spring lever is held, as in SAM Simulator: the left button to ВЛЕВО, the right to ВПРАВО.
    SC_EXPECT(panel.Find("ssc.slew")->springs_back);
    SC_EXPECT(CabinClickTarget(*panel.Find("ssc.slew"), 1, 1) == 0);
    SC_EXPECT(CabinClickTarget(*panel.Find("ssc.slew"), 1, -1) == 2);
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
    SC_EXPECT(layout.stations.size() == 5 && layout.panels.size() == 40);  // with the R-124 A-3 and A-5 boxes' faces (the headsets), the ПАНЕЛЬ ИНДИКАЦИИ, АРРЕТИР., the walls' levers, the cab's units (fire, pre-heater, DP-3B...), the driver's (stalk, gear, handbrake, stowage) and the 0096's cover and toggles
    // The R-123M's and the R-124 boxes' faces are bare panels: the meshes carry their own fronts.
    for (const char* id : {"radio_left_panel", "radio_right_panel", "r124_a1_panel", "r124_a2_panel", "gyro_arrest_panel",
                            "air_panel", "bulkhead_panel", "bulkhead_left_panel", "vent_hatch_right", "vent_hatch_left",
                            "fire_panel", "fire_valve_panel", "heater_panel", "dp3b_panel", "post_valve_panel", "radio_well_panel",
                            "header_valve_left", "header_valve_right", "column_stalk_panel", "gear_lever_panel",
                            "handbrake_panel", "stowage_panel", "repair_lid_panel"}) {
        const CabinPanel* bare = layout.FindPanel(id);
        SC_EXPECT(bare != nullptr && bare->bare);
    }
    for (const std::string& id : layout.UnplacedControls(console.panel)) std::printf("    unplaced: %s\n", id.c_str());
    SC_EXPECT(layout.UnplacedControls(console.panel).empty());
    // Covers nobody placed sit over the control they guard.
    const CabinControl* cover = layout.FindControl("fire.cover");
    const CabinControl* hv = layout.FindControl("fire.button");
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
        // Close to the console: Fig. 3 leaves 0.19 m between the bench and SŁ-2M (the operators' knees touch the
        // console in usaf_shaw_2017 -019), so the eyes are about 0.4 m from its face.
        const double reach = seat->eye_m.x - panel->centre_m.x;
        SC_EXPECT(reach > 0.3 && reach < 0.5);
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
    // mon_2112_80 Fig. 3: on the left wall SŁ-1M stands ahead of the main console and SŁ-3M behind it; the driver
    // and the commander sit at the front of the same compartment, the driver beside SŁ-1M with PU-303 at his left.
    const CabinPanel* sl1m = layout.FindPanel("gyro_panel");
    const CabinPanel* sl3m = layout.FindPanel("sl3m_panel");
    const CabinPanel* pu303 = layout.FindPanel("pu303_panel");
    const CabinPanel* guidance = layout.FindPanel("fo_panel");
    const CabinPanel* range = layout.FindPanel("op_panel");
    SC_EXPECT(sl1m != nullptr && sl3m != nullptr && pu303 != nullptr);
    if (sl1m == nullptr || sl3m == nullptr || pu303 == nullptr) return;
    SC_EXPECT(sl1m->centre_m.y - 0.5 * sl1m->width_m >= guidance->centre_m.y + 0.5 * guidance->width_m);
    SC_EXPECT(sl3m->centre_m.y + 0.5 * sl3m->width_m <= range->centre_m.y - 0.5 * range->width_m);
    const CabinStation* driver = layout.FindStation("driver");
    SC_EXPECT(std::fabs(driver->eye_m.y - sl1m->centre_m.y) < 0.5 * sl1m->width_m + 0.1);
    SC_EXPECT(pu303->centre_m.x < driver->eye_m.x && pu303->centre_m.y > driver->eye_m.y);
    // Across the aisle, the right wall's racks SP-1M, SP-2M and SP-3M-2 face SŁ-1M, SŁ-2M and SŁ-3M (Fig. 3).
    const char* left[] = {"gyro_panel", "so_panel", "sl3m_panel"};
    const char* right[] = {"sp1m_panel", "sp2m_panel", "sp3m_panel"};
    for (int i = 0; i < 3; ++i) {
        const CabinPanel* l = layout.FindPanel(left[i]);
        const CabinPanel* r = layout.FindPanel(right[i]);
        SC_EXPECT(r != nullptr);
        if (r == nullptr) return;
        SC_EXPECT(r->centre_m.x > 0.0 && Dot(r->Normal(), Vec3{-1.0, 0.0, 0.0}) > 0.99);   // right wall, facing the aisle
        // SP-3M-2 was re-measured from the book's photos and stands 0.11 m aft of SL-3M, its end at the bulkhead.
        SC_EXPECT(std::fabs(r->centre_m.y - l->centre_m.y) < 0.15);
    }
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

SC_TEST("cabin: the 0096 cabinet's voltmeter reads 220 V once a supply is on line, before АППАРАТУРА ВКЛ.") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const CabinDisplay* volts = nullptr;
    for (const CabinDisplay& d : layout.displays) {
        if (d.id == "pwr_voltmeter") volts = &d;
    }
    SC_EXPECT(volts != nullptr);
    if (volts == nullptr) return;
    SC_EXPECT(volts->kind == CabinDisplayKind::Meter && volts->panel_id == "pwr_cabinet");
    SC_EXPECT_NEAR(MeterValue(*volts, console), 0.0, 1e-9);   // no source on line
    console.panel.Set("pwr.gta", 1);
    for (int i = 0; i < 3000 && !console.equipment.IsReady("power"); ++i) console.equipment.Update(0.1, console.panel);
    SC_EXPECT(console.panel.Get("pwr.equipment") == 0);
    SC_EXPECT_NEAR(MeterValue(*volts, console), 220.0, 1e-9);  // the GTA up: the operator reads 220 V (voenizdat_osa_1991 p247)
    console.panel.Set("pwr.gta", 0);
    for (int i = 0; i < 100; ++i) console.equipment.Update(0.1, console.panel);
    SC_EXPECT_NEAR(MeterValue(*volts, console), 0.0, 1e-9);
}

SC_TEST("cabin: the 0096's cover shuts over the repair toggles and lifts on its rear edge like an awning when opened") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const CabinHinge* lid = nullptr;
    for (const CabinHinge& h : layout.hinges) {
        if (h.id == "repair_lid") lid = &h;
    }
    SC_EXPECT(lid != nullptr);
    if (lid == nullptr) return;
    SC_EXPECT(lid->subsystem == "repair_lid_shut" && lid->shell.size() == 1 && lid->shell[0] == "cabinet_0096_lid");
    SC_EXPECT(std::fabs(lid->axis.z) < 1e-9 && std::fabs(lid->axis.Length() - 1.0) < 1e-9);   // level, along the cabinet's width
    // Shut at the start (the session starts with the cover as built); the screw turns it open.
    SC_EXPECT(console.equipment.IsReady("repair_lid_shut") && console.panel.Get("pwr.repair_lid") == 0);
    console.panel.Set("pwr.repair_lid", 1);
    for (int i = 0; i < 200; ++i) console.equipment.Update(0.1, console.panel);
    SC_EXPECT_NEAR(console.equipment.Progress("repair_lid_shut"), 0.0, 1e-9);
    // Open: the screw on its free edge rises well above the toggles' plate and stays over the cabinet, and the plate is clear.
    const CabinPanel* screw = layout.FindPanel("repair_lid_panel");
    const CabinPanel* plate = layout.FindPanel("repair_toggles_panel");
    SC_EXPECT(screw != nullptr && plate != nullptr);
    if (screw == nullptr || plate == nullptr) return;
    const Vec3 open = lid->Turned(screw->centre_m, lid->folded_rad);
    std::printf("    the screw: shut z %.3f, open z %.3f (plate %.3f)\n", screw->centre_m.z, open.z, plate->centre_m.z);
    SC_EXPECT(open.z > plate->centre_m.z + 0.12 && open.z > screw->centre_m.z + 0.1);
    const Vec3 n = plate->Normal();
    SC_EXPECT(Dot(open - plate->centre_m, n) > 0.1);                     // in front of the plate's face, out of the way
    SC_EXPECT(Near(lid->Turned(screw->centre_m, 0.0), screw->centre_m)); // shut: as built
    int toggles = 0;
    for (const CabinFixture& f : layout.fixtures) {
        if (f.panel_id == "repair_toggles_panel" && f.look.kind == CabinLookKind::Toggle) ++toggles;
    }
    SC_EXPECT(toggles == 23);                                            // 13 and 10 (wwp_gecko g097-a)
}

SC_TEST("cabin: a fixture drawn by a shell's mesh says so, and the SL-2M's mounts do") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    int meshed = 0;
    for (const CabinFixture& f : layout.fixtures) {
        if (f.mesh.empty()) continue;
        ++meshed;
        SC_EXPECT(f.mesh == "SM_RackSL2M");
        SC_EXPECT(f.panel_id == "op_panel" || f.panel_id == "so_panel" || f.panel_id == "fo_panel");
    }
    SC_EXPECT(meshed == 14);                                             // so 7, fo 6 (with БР's block, 59), op 1; ПЕЛЕНГ's are hex_button parts (docs/OSA_INTERIOR_HOLISTIC.md 54, 59, 60)
}

SC_TEST("cabin: an upright hinge turns clockwise seen from above") {
    CabinHinge h;
    h.pivot_m = Vec3{1.0, 1.0, 0.0};
    const Vec3 p = h.Turned(Vec3{1.0, 2.0, 0.5}, DegToRad(90.0));       // a metre ahead of the pivot goes a metre to its right
    SC_EXPECT(Near(p, Vec3{2.0, 1.0, 0.5}, 1e-12));
    const std::string hinge = R"(, "hinges": [ {"id": "h", "panels": ["p"], "pivot_m": [0, 0, 0], "axis": [0, 0, 0], "folded_deg": 10,
                                                 "subsystem": "power", "source": "sam3d", "confidence": "estimate"} ])";
    SC_EXPECT(ErrorContains(Small(hinge), "hinges[0].axis: must not be zero"));
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
    SC_EXPECT(!lit("beam_1"));  // the beam lamps follow the engagement model's inputs, not the selector
    console.equipment.SetInput("search_beam_1", true);
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
    // The SŁ-2M's signal armature (a lamp's look only, its cap black by the ring) and the hex-nut push-button.
    CabinLayout armature;
    std::string error;
    SC_EXPECT(LoadCabin(Small(R"(, "lamps": [ { "id": "l", "label": "L", "panel": "p", "at_m": [0, 0], "color": "red",
        "lit_when": [ { "input": "target_in_zone", "is": true } ], "source": "s", "confidence": "high",
        "look": { "kind": "signal", "size_m": 0.016, "ring": "black" } } ],
        "controls": [ { "control": "soc.search", "panel": "p", "at_m": [0.1, 0],
                        "look": { "kind": "hex_button", "size_m": 0.024, "color": "red" } } ],
        "fixtures": [ { "panel": "p", "at_m": [0.2, 0], "kind": "hex_button", "size_m": 0.02, "color": "red" } ])"),
                        console, armature, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(armature.lamps.size() == 1 && armature.lamps[0].look.kind == CabinLookKind::Signal &&
              armature.lamps[0].look.ring == "black" && std::string(ToString(CabinLookKind::Signal)) == "signal");
    SC_EXPECT(armature.controls.size() == 1 && armature.controls[0].look.kind == CabinLookKind::HexButton &&
              std::string(ToString(CabinLookKind::HexButton)) == "hex_button");
    SC_EXPECT(armature.fixtures.size() == 1 && armature.fixtures[0].look.kind == CabinLookKind::HexButton);
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.search", "panel": "p", "at_m": [0, 0],
                                                       "look": { "kind": "signal", "size_m": 0.016 } } ])"),
                            "signal is for lamps only"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "signal", "size_m": 0.016 } ])"),
                            "signal is for lamps only"));
    // Silver is a look colour (ЗАПРЕТ ПУСКА's polished barrel).
    CabinLayout silver;
    SC_EXPECT(LoadCabin(Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0, 0],
                                                   "look": { "size_m": 0.041, "color": "silver" } } ])"), console, silver, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(silver.controls.size() == 1 && silver.controls[0].look.color == "silver");
}

SC_TEST("cabin: a hand wheel names the display its crewman watches while turning it") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout, &error));
    const CabinControl* az = layout.FindControl("ssc.az_wheel");
    const CabinControl* range = layout.FindControl("ssc.range_wheel");
    const CabinControl* el = layout.FindControl("ssc.el_wheel");
    SC_EXPECT(az != nullptr && az->watch == "soc_ppi");
    SC_EXPECT(range != nullptr && range->watch == "ssc_range");
    SC_EXPECT(el != nullptr && el->watch == "elev_search");
    SC_EXPECT(ErrorContains(
        Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0, 0], "watch": "x" } ])"), "for wheels only"));
    SC_EXPECT(ErrorContains(
        Small(R"(, "controls": [ { "control": "ssc.az_wheel", "panel": "p", "at_m": [0, 0], "watch": "nope" } ])"),
        "controls[0].watch: no display 'nope'"));
}

SC_TEST("cabin: a crewman up from his seat walks the floor, slides along its edges and sits down where he stands by a seat") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout cabin;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, cabin, &error));
    SC_EXPECT(!cabin.walk.empty() && cabin.stand_eye_m > 1.83);
    // Every seat can get up onto the floor, and is the seat found where he gets up.
    for (const CabinStation& st : cabin.stations) {
        Vec3 up;
        SC_EXPECT(cabin.StandUpFrom(st, up) && cabin.Walkable(up.x, up.y));
        SC_EXPECT_NEAR(up.z, cabin.stand_eye_m, 1e-9);
        const CabinStation* near = cabin.SeatNear(up, 0.05);
        SC_EXPECT(near != nullptr && near->id == st.id);
    }
    // The commander gets up into the gap between the cab seats and walks back down the aisle to the operator.
    Vec3 at;
    SC_EXPECT(cabin.StandUpFrom(*cabin.FindStation("commander"), at));
    SC_EXPECT_NEAR(at.x, 0.1, 1e-9);
    SC_EXPECT_NEAR(at.y, 3.72, 1e-9);
    // Into the right-hand racks: he slides along them instead.
    const Vec3 slid = cabin.WalkStep(Vec3{0.3, 2.5, at.z}, Vec3{0.9, 2.3, at.z});
    SC_EXPECT_NEAR(slid.x, 0.3, 1e-9);
    SC_EXPECT_NEAR(slid.y, 2.3, 1e-9);
    // Into a rack straight on: he stays.
    const Vec3 stopped = cabin.WalkStep(Vec3{0.42, 2.0, at.z}, Vec3{0.6, 2.0, at.z});
    SC_EXPECT_NEAR(stopped.x, 0.42, 1e-9);
    for (int i = 0; i < 400; ++i) at = cabin.WalkStep(at, at + Vec3{0.0, -0.01, 0.0});
    const CabinStation* seat = cabin.SeatNear(at, 0.6);
    SC_EXPECT(seat != nullptr && seat->id == "operator");
    // A walk needs the standing eye, under the roof; an area must be wide enough to stand in.
    SC_EXPECT(ErrorContains(Small(R"(, "walk": [ { "from_m": [0, 0], "to_m": [1, 1] } ])"), "stand_eye_m: required with walk"));
    SC_EXPECT(ErrorContains(Small(R"(, "stand_eye_m": 1.9, "walk": [ { "from_m": [0, 0], "to_m": [0.02, 1] } ])"),
                            "walk[0]: too narrow"));
}

SC_TEST("cabin: the PU-12M's crew can get up too") {
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/pu12m.json", console, &error));
    CabinLayout cabin;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/pu12m.json", console, cabin, &error));
    SC_EXPECT(cabin.stand_eye_m < cabin.ceiling_m - 0.08);
    for (const CabinStation& st : cabin.stations) {
        Vec3 up;
        SC_EXPECT(cabin.StandUpFrom(st, up));
        SC_EXPECT((Vec3{up.x, up.y, 0.0} - Vec3{st.eye_m.x, st.eye_m.y, 0.0}).Length() < 0.6);  // beside his seat
    }
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
    // A legend lamp shows its label on the glass, so it needs one.
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "legend", "size_m": 0.024 } ])"),
                            "fixtures[0].label: required"));
    CabinLayout legend;
    SC_EXPECT(LoadCabin(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "legend", "size_m": 0.024,
                                                   "color": "red", "label": "СХОД" } ])"), console, legend, &error));
    SC_EXPECT(legend.fixtures.size() == 1 && legend.fixtures[0].look.kind == CabinLookKind::Legend &&
              std::string(ToString(CabinLookKind::Legend)) == "legend");
    // Gauges, and block plates in a colour of their own (PU-303 is black).
    CabinLayout extra;
    SC_EXPECT(LoadCabin(Small(R"(, "blocks": [ { "id": "b", "panel": "p", "label": "ПУ-303", "at_m": [0, 0], "size_m": [0.3, 0.2],
                                                 "color": "black" } ],
          "fixtures": [ { "panel": "p", "at_m": [0.1, 0.1], "kind": "gauge", "size_m": 0.06 } ])"), console, extra, &error));
    SC_EXPECT(extra.blocks.size() == 1 && extra.blocks[0].color == "black" && extra.fixtures.size() == 1 &&
              extra.fixtures[0].look.kind == CabinLookKind::Gauge);
    // The R-123M's T-bar knob: the loader reads it by name.
    CabinLayout bar;
    SC_EXPECT(LoadCabin(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "bar_knob", "size_m": 0.03 } ])"),
                        console, bar, &error));
    SC_EXPECT(bar.fixtures.size() == 1 && bar.fixtures[0].look.kind == CabinLookKind::BarKnob &&
              std::string(ToString(CabinLookKind::BarKnob)) == "bar_knob");
    // A control whose part is in the shell's mesh: read by name, the last look kind; never a fixture.
    CabinLayout in_mesh;
    SC_EXPECT(LoadCabin(Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0, 0],
                                                    "look": { "kind": "in_mesh" } } ])"),
                        console, in_mesh, &error));
    SC_EXPECT(in_mesh.controls.size() == 1 && in_mesh.controls[0].look.kind == CabinLookKind::InMesh &&
              std::string(ToString(CabinLookKind::InMesh)) == "in_mesh");
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "in_mesh", "size_m": 0.02 } ])"),
                            "in_mesh is for controls only"));
    // The domed hand wheel: read by name too.
    CabinLayout dome;
    SC_EXPECT(LoadCabin(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "dome", "size_m": 0.135 } ])"),
                        console, dome, &error));
    SC_EXPECT(dome.fixtures.size() == 1 && dome.fixtures[0].look.kind == CabinLookKind::Dome &&
              std::string(ToString(CabinLookKind::Dome)) == "dome");
    // A gauge's printed scale: compass or linear, for gauges only.
    CabinLayout scaled;
    SC_EXPECT(LoadCabin(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0.1, 0.1], "kind": "gauge", "size_m": [0.2, 0.12],
                                                   "color": "black", "scale": "compass" } ])"), console, scaled, &error));
    SC_EXPECT(scaled.fixtures.size() == 1 && scaled.fixtures[0].look.scale == "compass");
    CabinLayout counters;
    SC_EXPECT(LoadCabin(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0.1, 0.1], "kind": "gauge", "size_m": [0.2, 0.05],
                                                   "scale": "counters" } ])"), console, counters, &error));
    SC_EXPECT(counters.fixtures.size() == 1 && counters.fixtures[0].look.scale == "counters");
    CabinLayout hours;   // the driver's engine-hour meter (wwp_gecko g085-a)
    SC_EXPECT(LoadCabin(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "gauge", "size_m": 0.06, "scale": "hours" } ])"),
                        console, hours, &error));
    SC_EXPECT(hours.fixtures.size() == 1 && hours.fixtures[0].look.scale == "hours");
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "gauge", "size_m": 0.05, "scale": "log" } ])"),
                            "fixtures[0].scale: unknown scale 'log'"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "knob", "size_m": 0.02, "scale": "linear" } ])"),
                            "fixtures[0].scale: for gauges only"));
    // A linear dial's figures and unit, as read off a photograph (the driver's speedometer, wwp_gecko g085-a).
    CabinLayout marked;
    SC_EXPECT(LoadCabin(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "gauge", "size_m": 0.12, "color": "black",
                                                   "marks": ["0", "20", "40", "60", "80", "100", "120"], "unit": "km/h" } ])"),
                        console, marked, &error));
    SC_EXPECT(marked.fixtures.size() == 1 && marked.fixtures[0].look.marks.size() == 7 &&
              marked.fixtures[0].look.marks.back() == "120" && marked.fixtures[0].look.unit == "km/h");
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "knob", "size_m": 0.02, "unit": "A" } ])"),
                            "fixtures[0].unit: for gauges only"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "gauge", "size_m": 0.05, "marks": ["0"] } ])"),
                            "fixtures[0].marks: a linear dial's figures, two or more"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "gauge", "size_m": 0.05, "scale": "compass",
                                                       "marks": ["0", "1"] } ])"),
                            "fixtures[0].marks: a linear dial's figures, two or more"));
    // Engraved text, a printed scale round a knob, a chart's ruled frame, a label beside its item (SAM's AK photo).
    CabinLayout printed;
    SC_EXPECT(LoadCabin(Small(R"(, "fixtures": [
        { "panel": "p", "at_m": [0, 0], "kind": "text", "size_m": 0.004, "label": "ГРУБО", "angle_deg": 90 },
        { "panel": "p", "at_m": [0.05, 0], "kind": "scale", "size_m": 0.045, "angles_deg": [-135, 0, 135], "marks": ["0°", "", "270°"] },
        { "panel": "p", "at_m": [0.1, 0], "kind": "frame", "size_m": [0.02, 0.01] },
        { "panel": "p", "at_m": [0.15, 0], "kind": "jack", "size_m": 0.009, "color": "copper", "label": "+27В", "label_at": "below" },
        { "panel": "p", "at_m": [0.2, 0], "kind": "latch", "size_m": 0.022, "color": "cream" } ])"),
                        console, printed, &error));
    SC_EXPECT(printed.fixtures.size() == 5);
    if (printed.fixtures.size() == 5) {
        SC_EXPECT(printed.fixtures[0].look.kind == CabinLookKind::Text && printed.fixtures[0].look.angle_deg == 90.0);
        SC_EXPECT(printed.fixtures[1].look.kind == CabinLookKind::Scale && printed.fixtures[1].look.angles_deg.size() == 3 &&
                  printed.fixtures[1].look.marks[1].empty());
        SC_EXPECT(printed.fixtures[2].look.kind == CabinLookKind::Frame);
        SC_EXPECT(printed.fixtures[3].look.label_at == "below" && printed.fixtures[3].look.color == "copper");
        SC_EXPECT(printed.fixtures[4].look.color == "cream");
    }
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "scale", "size_m": 0.04,
                                                       "angles_deg": [0], "marks": ["0", "1"] } ])"),
                            "a scale needs as many angles as marks"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "knob", "size_m": 0.02,
                                                       "angles_deg": [0] } ])"),
                            "for scales only"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "knob", "size_m": 0.02, "marks": [""] } ])"),
                            "expected a non-empty string"));
    SC_EXPECT(!LoadCabin(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "knob", "size_m": 0.02, "label": "X",
                                                    "label_at": "behind" } ])"), console, printed, &error));
    SC_EXPECT(ErrorContains(Small(R"(, "blocks": [ { "id": "b", "panel": "p", "label": "X", "at_m": [0, 0], "size_m": [0.3, 0.2],
                                                     "color": "purple" } ])"),
                            "blocks[0].color: unknown colour 'purple'"));
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
    // The knobs and switches nothing reads are inert controls now, no longer fixtures: count both.
    for (const CabinControl& c : osa.controls) {
        per_panel[0] += c.panel_id == "op_panel";
        per_panel[1] += c.panel_id == "so_panel";
        per_panel[2] += c.panel_id == "fo_panel";
    }
    SC_EXPECT(per_panel[0] > 70 && per_panel[1] > 70 && per_panel[2] > 70);
    SC_EXPECT(osa.blocks.size() >= 15);
    // The shell: the modelled compartment (hull, six racks, bench, seats, cab, equipment) and its lamps.
    int lights = 0;
    int meshes = 0;
    bool hull = false;
    for (const CabinShellPart& part : osa.shell) {
        lights += part.kind == CabinShellKind::Light;
        meshes += part.kind == CabinShellKind::Mesh;
        hull = hull || part.mesh == "SM_OsaHull";
    }
    SC_EXPECT(hull && meshes >= 20 && lights >= 3);
    SC_EXPECT(ErrorContains(Small(R"(, "shell": [ { "id": "w", "label": "W", "kind": "wall", "centre_m": [0, 0, 1],
                                                   "size_m": [1, 1, 1], "source": "s", "confidence": "low" } ])"),
                            "shell[0].kind: must be box, glass, light or mesh"));
    SC_EXPECT(ErrorContains(Small(R"(, "shell": [ { "id": "w", "label": "W", "kind": "box", "centre_m": [0, 0, 1],
                                                   "size_m": [1, 0, 1], "source": "s", "confidence": "low" } ])"),
                            "shell[0].size_m: must be positive"));
}

SC_TEST("cabin: a scale's arrowhead, a legend's ink and stadium cap, a toggle that stands up off, screw heads (SAM's AK photo)") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    std::string error;
    SC_EXPECT(LoadCabin(Small(R"(, "fixtures": [
        { "panel": "p", "at_m": [0, 0], "kind": "scale", "size_m": 0.06, "angles_deg": [-163, -56], "marks": ["", ""], "arrow": "first" },
        { "panel": "p", "at_m": [0.1, 0], "kind": "legend", "size_m": [0.05, 0.023], "label": "ПУСК ГОТОВ", "ink": "red",
          "shape": "stadium" },
        { "panel": "p", "at_m": [0.2, 0], "kind": "screw", "size_m": 0.007 } ])"), console, layout, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(layout.fixtures.size() == 3);
    if (layout.fixtures.size() == 3) {
        SC_EXPECT(layout.fixtures[0].look.arrow == "first");
        SC_EXPECT(layout.fixtures[1].look.ink == "red" && layout.fixtures[1].look.shape == "stadium");
        SC_EXPECT(layout.fixtures[2].look.kind == CabinLookKind::Screw && std::string(ToString(CabinLookKind::Screw)) == "screw");
        SC_EXPECT_NEAR(layout.fixtures[2].look.width_m, 0.007, 1e-12);
    }
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "knob", "size_m": 0.02, "arrow": "first" } ])"),
                            "a scale's arrowhead"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "scale", "size_m": 0.04,
                                                       "angles_deg": [0, 90], "marks": ["", ""], "arrow": "middle" } ])"),
                            "a scale's arrowhead"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "knob", "size_m": 0.02, "ink": "red" } ])"),
                            "for legends only"));
    SC_EXPECT(ErrorContains(Small(R"(, "fixtures": [ { "panel": "p", "at_m": [0, 0], "kind": "legend", "size_m": 0.02, "label": "X",
                                                       "shape": "oval" } ])"),
                            "for legends only"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0, 0],
                                                       "look": { "kind": "toggle", "size_m": 0.014, "up": 2 } } ])"),
                            "0 or 1"));
    // A screw is a fixture: no control looks like one.
    SC_EXPECT(!LoadCabin(Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0, 0],
                                                    "look": { "kind": "screw", "size_m": 0.007 } } ])"), console, layout, &error));
    // A toggle engraved with its off position up (УГОЛ ЗРЕНИЯ's ШИРОКИЙ) points up at 0 and down at 1.
    SC_EXPECT_NEAR(CabinLayout::PositionAngleDeg(CabinStyle::Toggle, 2, 0, 0), 0.0, 1e-12);
    SC_EXPECT_NEAR(CabinLayout::PositionAngleDeg(CabinStyle::Toggle, 2, 1, 0), 180.0, 1e-12);
    SC_EXPECT_NEAR(CabinLayout::PositionAngleDeg(CabinStyle::Toggle, 2, 1), 0.0, 1e-12);
}

SC_TEST("cabin: shell mesh parts name their mesh, turn by yaw and take no colour; lights can be dim or blue") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    std::string error;
    SC_EXPECT(LoadCabin(Small(R"(, "shell": [
        { "id": "s", "label": "Seat", "kind": "mesh", "mesh": "SM_Seat", "centre_m": [0, 1, 1], "size_m": [0.5, 0.4, 0.9],
          "yaw_deg": -90, "source": "s", "confidence": "low" },
        { "id": "l", "label": "Lamp", "kind": "light", "centre_m": [0, 1, 2], "size_m": [1, 1, 1], "color": "blue",
          "lumens": 15, "source": "s", "confidence": "low" } ])"), console, layout, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(layout.shell.size() == 2 && layout.shell[0].kind == CabinShellKind::Mesh && layout.shell[0].mesh == "SM_Seat");
    SC_EXPECT_NEAR(layout.shell.size() == 2 ? layout.shell[0].yaw_rad : 0.0, DegToRad(-90.0), 1e-12);
    SC_EXPECT_NEAR(layout.shell.size() == 2 ? layout.shell[1].lumens : 0.0, 15.0, 1e-12);
    auto shell = [](const char* part) { return Small(std::string(R"(, "shell": [ )") + part + " ]"); };
    SC_EXPECT(ErrorContains(shell(R"({ "id": "s", "label": "S", "kind": "mesh", "centre_m": [0, 0, 1], "size_m": [1, 1, 1],
                                       "source": "s", "confidence": "low" })"), "shell[0].mesh: required for a mesh part"));
    SC_EXPECT(ErrorContains(shell(R"({ "id": "s", "label": "S", "kind": "box", "mesh": "SM_X", "centre_m": [0, 0, 1],
                                       "size_m": [1, 1, 1], "source": "s", "confidence": "low" })"), "shell[0].mesh: for mesh parts only"));
    SC_EXPECT(ErrorContains(shell(R"({ "id": "s", "label": "S", "kind": "box", "yaw_deg": 10, "centre_m": [0, 0, 1],
                                       "size_m": [1, 1, 1], "source": "s", "confidence": "low" })"), "shell[0].yaw_deg: for mesh parts only"));
    SC_EXPECT(ErrorContains(shell(R"({ "id": "s", "label": "S", "kind": "mesh", "mesh": "SM_X", "color": "red", "centre_m": [0, 0, 1],
                                       "size_m": [1, 1, 1], "source": "s", "confidence": "low" })"),
                            "shell[0].color: a mesh part has the mesh's own colours"));
    SC_EXPECT(ErrorContains(shell(R"({ "id": "s", "label": "S", "kind": "box", "lumens": 10, "centre_m": [0, 0, 1],
                                       "size_m": [1, 1, 1], "source": "s", "confidence": "low" })"), "shell[0].lumens: for lights only"));
    SC_EXPECT(ErrorContains(shell(R"({ "id": "s", "label": "S", "kind": "box", "color": "blue", "centre_m": [0, 0, 1],
                                       "size_m": [1, 1, 1], "source": "s", "confidence": "low" })"), "shell[0].color: unknown colour 'blue'"));
}

SC_TEST("cabin: an ammunition lamp flashes while its missile's gyro is overheated, and only then") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const CabinLamp* ammo = nullptr;
    const CabinLamp* gta = nullptr;
    for (const CabinLamp& l : layout.lamps) {
        if (l.id == "ammo_3_i") ammo = &l;
        if (l.id == "gta_on") gta = &l;
    }
    SC_EXPECT(ammo != nullptr && gta != nullptr);
    if (ammo == nullptr || gta == nullptr) return;
    SC_EXPECT(!CabinLayout::LampFlashing(*ammo, console.panel, console.equipment));  // empty container: dark
    console.equipment.SetInput("ammo_3_i", true);
    SC_EXPECT(CabinLayout::LampLit(*ammo, console.panel, console.equipment));
    SC_EXPECT(!CabinLayout::LampFlashing(*ammo, console.panel, console.equipment));  // steady
    console.equipment.SetInput("hot_3_i", true);
    SC_EXPECT(CabinLayout::LampFlashing(*ammo, console.panel, console.equipment));
    console.equipment.SetInput("ammo_3_i", false);  // no missile: nothing to flash
    SC_EXPECT(!CabinLayout::LampFlashing(*ammo, console.panel, console.equipment));
    SC_EXPECT(!CabinLayout::LampFlashing(*gta, console.panel, console.equipment));  // a lamp without flash_when never does
}

SC_TEST("cabin: 0009-5M's ОШИБКА indicators glow 15-30 s after the stabilisation is switched on; OR-11M repeats the launch табло; OR-31M's ψ and θ read level") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    auto lamp = [&](const char* id) -> const CabinLamp* {
        for (const CabinLamp& l : layout.lamps) {
            if (l.id == id) return &l;
        }
        return nullptr;
    };
    const CabinLamp* psi = lamp("stab_err_psi");
    const CabinLamp* gamma = lamp("stab_err_gamma");
    const CabinLamp* zone = lamp("or11_in_zone");
    const CabinLamp* narrow = lamp("or11_uzkiy_luch");
    const CabinLamp* k3 = lamp("or11_k3");
    SC_EXPECT(psi != nullptr && gamma != nullptr && zone != nullptr && narrow != nullptr && k3 != nullptr);
    if (psi == nullptr || gamma == nullptr || zone == nullptr || narrow == nullptr || k3 == nullptr) return;
    auto lit = [&](const CabinLamp* l) { return CabinLayout::LampLit(*l, console.panel, console.equipment); };
    auto run = [&](double s) {
        for (double t = 0.0; t < s; t += 0.1) console.equipment.Update(0.1, console.panel);
    };
    // Voenizdat p52: dark until the system is on, glowing 15-30 s after.
    console.panel.Set("pwr.gta", 1);
    run(80.0);
    console.panel.Set("pwr.equipment", 1);
    run(5.0);
    SC_EXPECT(!lit(psi) && !lit(gamma));
    console.panel.Set("stab.power", 1);
    run(10.0);
    SC_EXPECT(!lit(psi) && !lit(gamma));
    run(20.0);
    SC_EXPECT(lit(psi) && lit(gamma));
    console.panel.Set("stab.power", 0);
    run(1.0);
    SC_EXPECT(!lit(psi));

    // OR-11M's табло: the launch signals, one lamp for both missile channels.
    SC_EXPECT(!lit(zone) && !lit(narrow) && !lit(k3));
    console.equipment.SetInput("target_in_zone", true);
    console.equipment.SetInput("ch2_uzkiy_luch", true);
    SC_EXPECT(lit(zone) && lit(narrow) && !lit(k3));
    console.equipment.SetInput("ch1_k3", true);
    SC_EXPECT(lit(k3));

    // OR-31M: ψ and θ beside Qc, at zero on a level vehicle while powered.
    const CabinDisplay* psi_scale = nullptr;
    const CabinDisplay* theta_scale = nullptr;
    for (const CabinDisplay& d : layout.displays) {
        if (d.id == "or31_psi") psi_scale = &d;
        if (d.id == "or31_theta") theta_scale = &d;
    }
    SC_EXPECT(psi_scale != nullptr && theta_scale != nullptr);
    if (psi_scale == nullptr || theta_scale == nullptr) return;
    SC_EXPECT(CabinLayout::DisplayOn(*psi_scale, console.panel, console.equipment));
    SC_EXPECT_NEAR(DialReading(*theta_scale, console, 0.0), 0.0, 1e-9);
}

SC_TEST("cabin: gauges read the console by their levels: 0062's meters, 0004-14M's centre-zero meters, the PU-303's turbine") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    auto display = [&](const char* id) -> const CabinDisplay* {
        for (const CabinDisplay& d : layout.displays) {
            if (d.id == id) return &d;
        }
        return nullptr;
    };
    const CabinDisplay* current = display("tx_current");
    const CabinDisplay* error = display("err_target_eps");
    const CabinDisplay* rpm = display("gta_rpm");
    SC_EXPECT(current != nullptr && error != nullptr && rpm != nullptr);
    if (current == nullptr || error == nullptr || rpm == nullptr) return;
    auto run = [&](double s) {
        for (double t = 0.0; t < s; t += 0.1) console.equipment.Update(0.1, console.panel);
    };
    // The turbine: the rpm runs up with its start and holds running.
    SC_EXPECT_NEAR(DialReading(*rpm, console, 0.0), 0.0, 1e-9);
    console.panel.Set("pwr.gta", 1);
    run(30.0);
    const double half = DialReading(*rpm, console, 0.0);
    SC_EXPECT(half > 3.0 && half < 6.5);
    run(40.0);
    SC_EXPECT_NEAR(DialReading(*rpm, console, 0.0), 9.5, 1e-9);
    // 0062-6M1: the selector at СОЦ reads nothing until the SOC radiates, then in tolerance; turned to ССЦ, nothing.
    console.panel.Set("pwr.equipment", 1);
    run(200.0);
    SC_EXPECT(console.panel.Get("mon.tx_select") == 0);
    SC_EXPECT_NEAR(MeterValue(*current, console), 0.0, 1e-9);
    console.panel.Set("soc.hv_cover", 1);
    console.panel.Set("soc.hv", 1);
    run(1.0);
    SC_EXPECT(console.equipment.IsReady("soc_transmitter"));
    SC_EXPECT_NEAR(MeterValue(*current, console), 6.0, 1e-9);
    console.panel.Set("mon.tx_select", 1);
    SC_EXPECT_NEAR(MeterValue(*current, console), 0.0, 1e-9);
    // A centre-zero meter: zero stands mid-scale, the scale runs -10..10.
    SC_EXPECT_NEAR(MeterValue(*error, console), 0.0, 1e-9);
    SC_EXPECT_NEAR(MeterFraction(*error, 0.0), 0.5, 1e-9);
    SC_EXPECT_NEAR(MeterMark(*error, 0), -10.0, 1e-9);
    SC_EXPECT_NEAR(MeterMark(*error, 4), 10.0, 1e-9);
    SC_EXPECT_NEAR(MeterFraction(*current, 6.0), 0.6, 1e-9);
}

SC_TEST("cabin: the compartments' pressurisation lights ПОДКАЧКА ОТСЕКОВ ВКЛ. until 0,23 and Р=0,18 from 0,18 (voenizdat_osa_1991 p172)") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    auto lamp = [&](const char* id) -> const CabinLamp* {
        for (const CabinLamp& l : layout.lamps) {
            if (l.id == id) return &l;
        }
        return nullptr;
    };
    const CabinLamp* filling = lamp("press_on");
    const CabinLamp* p018 = lamp("press_018");
    const CabinLamp* compressor = lamp("compressor_on");
    const CabinLamp* gabarit = lamp("gabarit");
    SC_EXPECT(filling != nullptr && p018 != nullptr && compressor != nullptr && gabarit != nullptr);
    if (filling == nullptr || p018 == nullptr || compressor == nullptr || gabarit == nullptr) return;
    auto lit = [&](const CabinLamp* l) { return CabinLayout::LampLit(*l, console.panel, console.equipment); };
    auto run = [&](double s) {
        for (double t = 0.0; t < s; t += 0.1) console.equipment.Update(0.1, console.panel);
    };
    console.panel.Set("pwr.gta", 1);
    run(70.0);
    // The compressor runs to charge the receiver once 220 V is on line, then stops.
    SC_EXPECT(lit(compressor));
    // Stowed and locked with the equipment on: ГАБАРИТ 3800.
    console.panel.Set("pwr.equipment", 1);
    run(1.0);
    SC_EXPECT(lit(gabarit));
    // ШТАТНАЯ pressed once: the fill runs on with the button released.
    console.panel.Set("press.standard", 1);
    run(0.2);
    console.panel.Set("press.standard", 0);
    run(0.2);
    SC_EXPECT(lit(filling) && !lit(p018));
    run(32.0);
    SC_EXPECT(lit(filling) && lit(p018));
    run(10.0);
    SC_EXPECT(!lit(filling) && lit(p018));
    run(100.0);
    SC_EXPECT(!lit(compressor));
}

SC_TEST("cabin: a scope's knobs set its picture: brightness, focus, contrast, marks, cursor, gain (voenizdat_osa_1991 p50, p52, p266)") {
    // The curves by hand: dark below a fifth of a turn, set up at the middle, overdriven to 1.8; focus sharp at the middle.
    SC_EXPECT_NEAR(PictureLevel(0.1), 0.0, 1e-12);
    SC_EXPECT_NEAR(PictureLevel(0.35), 0.5, 1e-12);
    SC_EXPECT_NEAR(PictureLevel(0.5), 1.0, 1e-12);
    SC_EXPECT_NEAR(PictureLevel(1.0), 1.8, 1e-12);
    SC_EXPECT_NEAR(PictureBlur(0.5), 0.0, 1e-12);
    SC_EXPECT_NEAR(PictureBlur(0.65), 0.5, 1e-12);
    SC_EXPECT_NEAR(PictureBlur(0.0), 1.0, 1e-12);
    // The Osa's search scope names OP81-16M2's knobs and OP63-6M3's РРУ; set up as the console starts, the picture is 1.
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout osa;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, osa));
    const CabinDisplay* ppi = nullptr;
    for (const CabinDisplay& d : osa.displays) {
        if (d.id == "soc_ppi") ppi = &d;
    }
    SC_EXPECT(ppi != nullptr);
    if (ppi == nullptr) return;
    SC_EXPECT(ppi->picture.brightness == "soc.bright" && ppi->picture.gain == "soc.gain" && ppi->picture.cursor == "soc.cursor_bright");
    // ЯРКОСТЬ starts turned down (the senior operator brings it up once the sweep shows, p52): the scope dark.
    ScopePicture p = ScopePictureOf(*ppi, console.panel);
    SC_EXPECT_NEAR(p.trace, 0.0, 1e-9);
    console.panel.SetValue("soc.bright", 0.5);
    p = ScopePictureOf(*ppi, console.panel);
    SC_EXPECT_NEAR(p.trace, 1.0, 1e-9);
    SC_EXPECT_NEAR(p.blur, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.contrast, 1.0, 1e-9);
    SC_EXPECT_NEAR(p.gain, 1.0, 1e-9);          // full gain for combat (p52)
    SC_EXPECT_NEAR(JamSectorScale(p), 1.0, 1e-9);
    // ЯРКОСТЬ turned down below the cut-off: the sweep goes out; the gain halved: a jammer's sector narrows (p266).
    console.panel.SetValue("soc.bright", 0.15);
    console.panel.SetValue("soc.gain", 0.5);
    console.panel.SetValue("soc.focus", 0.8);
    p = ScopePictureOf(*ppi, console.panel);
    SC_EXPECT_NEAR(p.trace, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.blur, 1.0, 1e-9);
    SC_EXPECT_NEAR(JamSectorScale(p), 0.2, 1e-9);   // never below its own beam
    console.panel.SetValue("soc.bright", 0.5);
    SC_EXPECT_NEAR(JamSectorScale(ScopePictureOf(*ppi, console.panel)), 0.5, 1e-9);
    // Overdriven: brighter, and the spot blooms.
    console.panel.SetValue("soc.bright", 1.0);
    console.panel.SetValue("soc.focus", 0.5);
    p = ScopePictureOf(*ppi, console.panel);
    SC_EXPECT_NEAR(p.trace, 1.8, 1e-9);
    SC_EXPECT_NEAR(p.blur, 0.5, 1e-9);
    // A display without knobs keeps a set-up picture.
    CabinDisplay plain;
    SC_EXPECT_NEAR(ScopePictureOf(plain, console.panel).trace, 1.0, 1e-12);
    // The loader: knobs only, and known ones.
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "s", "label": "S", "kind": "ppi", "panel": "p", "at_m": [0, 0],
        "size_m": [0.2, 0.2], "picture": { "brightness": "soc.mti" }, "source": "sam3d", "confidence": "estimate" } ])"),
                            "is not a knob that turns"));
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "s", "label": "S", "kind": "ppi", "panel": "p", "at_m": [0, 0],
        "size_m": [0.2, 0.2], "picture": { "focus": "soc.nope" }, "source": "sam3d", "confidence": "estimate" } ])"),
                            "unknown control 'soc.nope'"));
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "s", "label": "S", "kind": "ppi", "panel": "p", "at_m": [0, 0],
        "size_m": [0.2, 0.2], "picture": { }, "source": "sam3d", "confidence": "estimate" } ])"),
                            "names no control"));
}

SC_TEST("cabin: the range indicator's astigmatism, shift and gain by hand (voenizdat_osa_1991 p98-99)") {
    SC_EXPECT_NEAR(PictureShift(0.5), 0.0, 1e-12);
    SC_EXPECT_NEAR(PictureShift(1.0), kPictureShift, 1e-12);
    SC_EXPECT_NEAR(PictureShift(0.0), -kPictureShift, 1e-12);
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout osa;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, osa));
    const CabinDisplay* range = nullptr;
    for (const CabinDisplay& d : osa.displays) {
        if (d.id == "ssc_range") range = &d;
    }
    SC_EXPECT(range != nullptr);
    if (range == nullptr) return;
    // As the console starts: sharp, centred, the automatic gain control's picture.
    ScopePicture p = ScopePictureOf(*range, console.panel);
    SC_EXPECT_NEAR(p.blur, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.blur_2, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.shift_x, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.shift_y_2, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.gain, 1.0, 1e-9);
    // АСТИГМАТ. off its middle smears its own sweep as focus does; the shift knobs move their own sweep.
    console.panel.SetValue("ssc.astig_2", 0.65);
    console.panel.SetValue("ssc.hshift_1", 1.0);
    console.panel.SetValue("ssc.vshift_2", 0.25);
    p = ScopePictureOf(*range, console.panel);
    SC_EXPECT_NEAR(p.blur, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.blur_2, 0.5, 1e-9);
    SC_EXPECT_NEAR(p.shift_x, kPictureShift, 1e-9);
    SC_EXPECT_NEAR(p.shift_y, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.shift_y_2, -0.5 * kPictureShift, 1e-9);
    // РРУ КШ ОСН. works only at РРУ; at АРУ the automatic control holds the gain whatever the knob.
    console.panel.SetValue("ssc.rru_main", 0.3);
    SC_EXPECT_NEAR(ScopePictureOf(*range, console.panel).gain, 1.0, 1e-9);
    console.panel.Set("ssc.agc", 1);
    SC_EXPECT_NEAR(ScopePictureOf(*range, console.panel).gain, 0.3, 1e-9);
    // ДОП. УСИЛ. raises it by half.
    console.panel.Set("ssc.extra_gain", 1);
    SC_EXPECT_NEAR(ScopePictureOf(*range, console.panel).gain, 0.3 * kPictureGainBoost, 1e-9);
    // The loader: gain_when names controls only and needs a gain knob; gain_boost is a switch.
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "s", "label": "S", "kind": "ppi", "panel": "p", "at_m": [0, 0],
        "size_m": [0.2, 0.2], "picture": { "gain": "soc.gain", "gain_when": [ { "subsystem": "power", "is": "ready" } ] },
        "source": "sam3d", "confidence": "estimate" } ])"), "only controls' positions"));
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "s", "label": "S", "kind": "ppi", "panel": "p", "at_m": [0, 0],
        "size_m": [0.2, 0.2], "picture": { "brightness": "soc.bright", "gain_when": [ { "control": "soc.mti", "is": 1 } ] },
        "source": "sam3d", "confidence": "estimate" } ])"), "needs a gain knob"));
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "s", "label": "S", "kind": "ppi", "panel": "p", "at_m": [0, 0],
        "size_m": [0.2, 0.2], "picture": { "gain_boost": "soc.gain" }, "source": "sam3d", "confidence": "estimate" } ])"),
                            "is not a switch"));
}

SC_TEST("cabin: ПРИБОРЫ lights the driver's fuel gauge and ОСВЕЩ. the guidance error meters") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout osa;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, osa));
    auto lit = [&](const std::string& id) {
        for (const CabinDisplay& d : osa.displays) {
            if (d.id == id) return CabinLayout::DisplayLit(d, console.panel, console.equipment);
        }
        return false;
    };
    SC_EXPECT(!lit("fuel_gauge") && !lit("err_target_eps") && !lit("err_missile_beta"));
    console.panel.Set("veh.instr_lights", 1);
    console.panel.Set("err.light", 1);
    SC_EXPECT(lit("fuel_gauge") && lit("err_target_eps") && lit("err_missile_beta"));
}

SC_TEST("cabin: a gauge's levels are checked on load") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    std::string error;
    SC_EXPECT(LoadCabin(Small(R"(, "displays": [ { "id": "m", "label": "M", "kind": "meter", "panel": "p", "at_m": [0, 0],
        "size_m": [0.05, 0.04], "levels": [ { "when": [ { "subsystem": "power", "is": "ready" } ], "value": 3, "ramp": "gta" } ],
        "scale_from": -5, "full_scale": 5, "source": "sam3d", "confidence": "estimate" } ])"), console, layout, &error));
    SC_EXPECT(layout.displays.size() == 1 && layout.displays[0].levels.size() == 1 && layout.displays[0].levels[0].ramp == "gta");
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "m", "label": "M", "kind": "meter", "panel": "p", "at_m": [0, 0],
        "size_m": [0.05, 0.04], "levels": [ { "value": 1, "ramp": "nope" } ], "source": "sam3d", "confidence": "estimate" } ])"),
                            "unknown subsystem 'nope'"));
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "m", "label": "M", "kind": "meter", "panel": "p", "at_m": [0, 0],
        "size_m": [0.05, 0.04], "control": "soc.mti", "levels": [ { "value": 1 } ], "source": "sam3d", "confidence": "estimate" } ])"),
                            "a gauge with levels reads nothing else"));
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "m", "label": "M", "kind": "meter", "panel": "p", "at_m": [0, 0],
        "size_m": [0.05, 0.04], "levels": [ { "when": [] } ], "source": "sam3d", "confidence": "estimate" } ])"),
                            "value: required"));
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "m", "label": "M", "kind": "meter", "panel": "p", "at_m": [0, 0],
        "size_m": [0.05, 0.04], "levels": [ { "value": 1 } ], "scale_from": 2, "full_scale": 1, "source": "sam3d", "confidence": "estimate" } ])"),
                            "must be below full_scale"));
}

SC_TEST("cabin: АВАР. ОТКЛ. СЭП on the ПАНЕЛЬ ИНДИКАЦИИ cuts the power until the ГТА and АППАРАТУРА ВКЛ. are off, its lamp lit meanwhile; ДВЕРЬ ЗАКРЫТА lights with the power") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    SC_EXPECT(layout.FindPanel("ind_panel") != nullptr && layout.FindControl("pwr.emergency_off") != nullptr);
    const CabinLamp* door = nullptr;
    const CabinLamp* cut = nullptr;
    const CabinLamp* hatches = nullptr;
    for (const CabinLamp& l : layout.lamps) {
        if (l.id == "dver_zakryta") door = &l;
        if (l.id == "sep_avar_otkl") cut = &l;
        if (l.id == "lyuki_rta") hatches = &l;
    }
    SC_EXPECT(door != nullptr && cut != nullptr && hatches != nullptr);
    if (door == nullptr || cut == nullptr || hatches == nullptr) return;
    SC_EXPECT(cut->look.kind == CabinLookKind::LampButton);  // a lamp on the vehicle: the lit push-button's lens
    const ControlDef* sel = console.panel.Find("ind.select");
    SC_EXPECT(sel != nullptr && sel->positions == 7);       // 3—II ... ОТКЛ. ... 3—I
    auto run = [&](double s) {
        for (int i = 0; i < static_cast<int>(s * 10.0); ++i) console.equipment.Update(0.1, console.panel);
    };
    console.panel.Set("pwr.gta", 1);
    console.panel.Set("pwr.equipment", 1);
    run(120.0);
    SC_EXPECT(console.equipment.IsReady("power"));
    SC_EXPECT(CabinLayout::LampLit(*door, console.panel, console.equipment));
    SC_EXPECT(!CabinLayout::LampLit(*cut, console.panel, console.equipment));
    SC_EXPECT(CabinLayout::LampLit(*hatches, console.panel, console.equipment));   // the РТА's hatches start shut (p168)
    console.panel.Set("vent.hatch_left", 1);
    SC_EXPECT(!CabinLayout::LampLit(*hatches, console.panel, console.equipment));
    console.panel.Set("vent.hatch_left", 0);
    console.panel.Set("hull.door_dog_upper", 0);  // the engine door undogged
    SC_EXPECT(!CabinLayout::LampLit(*door, console.panel, console.equipment));
    console.panel.Set("hull.door_dog_upper", 2);

    console.panel.Set("pwr.emergency_off", 1);
    run(0.2);
    console.panel.Set("pwr.emergency_off", 0);
    run(5.0);
    SC_EXPECT(!console.equipment.IsReady("power"));
    SC_EXPECT(console.equipment.IsReady("gta"));  // the turbine runs on; its load is cut
    SC_EXPECT(!CabinLayout::LampLit(*door, console.panel, console.equipment));
    SC_EXPECT(CabinLayout::LampLit(*cut, console.panel, console.equipment));
    console.panel.Set("pwr.equipment", 0);
    run(1.0);
    SC_EXPECT(!console.equipment.IsReady("power"));  // still cut: the ГТА is on
    console.panel.Set("pwr.gta", 0);
    run(1.0);
    SC_EXPECT(!CabinLayout::LampLit(*cut, console.panel, console.equipment));
    console.panel.Set("pwr.gta", 1);
    run(120.0);
    SC_EXPECT(console.equipment.IsReady("power"));
}

SC_TEST("cabin: the PPI's course-parameter scale is seven chords 2 km apart on the 0-35 km scale, turned by its knob (voenizdat_osa_1991 p41-42)") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const CabinDisplay* ppi = nullptr;
    for (const CabinDisplay& d : layout.displays) {
        if (d.id == "soc_ppi") ppi = &d;
    }
    SC_EXPECT(ppi != nullptr);
    if (ppi == nullptr) return;
    std::vector<CabinScaleLine> lines = CourseScaleLines(*ppi, console);
    SC_EXPECT(lines.size() == 7);
    if (lines.size() != 7) return;
    // At 0 degrees the lines run up the scale: vertical, the middle one through the centre, 2/35 of the radius apart.
    SC_EXPECT_NEAR(lines[3].x0, 0.0, 1e-12);
    SC_EXPECT_NEAR(lines[3].y1, 1.0, 1e-12);
    SC_EXPECT_NEAR(lines[4].x0 - lines[3].x0, 2000.0 / 35000.0, 1e-12);
    SC_EXPECT_NEAR(lines[0].x0, lines[0].x1, 1e-12);
    // Each end on the tube's edge.
    SC_EXPECT_NEAR(std::hypot(lines[6].x1, lines[6].y1), 1.0, 1e-12);
    // Turned to 90 (east), they run across: horizontal.
    console.panel.SetValue("soc.course_scale", 90.0);
    lines = CourseScaleLines(*ppi, console);
    SC_EXPECT_NEAR(lines[2].y0, lines[2].y1, 1e-12);
    SC_EXPECT_NEAR(lines[3].x1, 1.0, 1e-12);
}

SC_TEST("cabin: a rotary's own stop angles; СМЕНА ЛИТ.'s locking bar shut over the knob, swung open from its second step") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const CabinControl* letter = layout.FindControl("spk.letter");
    SC_EXPECT(letter != nullptr);
    if (letter == nullptr) return;
    // НОРМ. with the bar shut and with it opened, СМЕНА when turned (the engraved words' bearings, samsim_panels).
    SC_EXPECT_NEAR(CabinLayout::ControlAngleDeg(*letter, 3, 0), 28.0, 1e-12);
    SC_EXPECT_NEAR(CabinLayout::ControlAngleDeg(*letter, 3, 1), 28.0, 1e-12);
    SC_EXPECT_NEAR(CabinLayout::ControlAngleDeg(*letter, 3, 2), -20.0, 1e-12);
    SC_EXPECT(letter->has_bar && !letter->BarOpen(0) && letter->BarOpen(1) && letter->BarOpen(2));
    SC_EXPECT(letter->bar_hinge_u_m < 0.0 && letter->bar_length_m > 0.04 && letter->bar_length_m < 0.05);
    // The stepwise control: the bar must be opened before the knob turns (osa050-052, one control of three steps).
    const ControlDef* def = console.panel.Find("spk.letter");
    SC_EXPECT(def != nullptr && def->stepwise && def->positions == 3);
    // Without angles a rotary spreads its stops evenly.
    const CabinControl* range = layout.FindControl("soc.range");
    SC_EXPECT(range != nullptr && range->angles_deg.empty() && !range->has_bar);
    if (range != nullptr) SC_EXPECT_NEAR(CabinLayout::ControlAngleDeg(*range, 3, 0), -30.0, 1e-12);

    SC_EXPECT(LoadCabin(Small(R"(, "controls": [ { "control": "soc.range", "panel": "p", "at_m": [0, 0], "angles_deg": [-40, 0, 25],
        "bar": { "hinge_m": [-0.03, 0.01], "length_m": 0.04 } } ])"), console, layout));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.range", "panel": "p", "at_m": [0, 0], "angles_deg": [0, 10] } ])"),
                            "one angle per position"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.mti", "panel": "p", "at_m": [0, 0], "angles_deg": [0, 10] } ])"),
                            "for rotaries only"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.range", "panel": "p", "at_m": [0, 0],
        "bar": { "hinge_m": [0, 0], "length_m": 0.04, "open_from": 3 } } ])"), "bar.open_from"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.range", "panel": "p", "at_m": [0, 0],
        "bar": { "hinge_m": [0, 0] } } ])"), "bar.length_m"));
}

SC_TEST("cabin: SL-2M's guards are hinged lids and a lever: one angle per position, a lid lifted out, a lever turned in the face") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    // АВАРИЙНЫЙ ПУСК's lid: placed over its button, hinged along its foot, opened through 180 degrees to hang inside out.
    const CabinControl* avar = layout.FindControl("msl.emergency_cover");
    const CabinControl* button = layout.FindControl("msl.emergency");
    SC_EXPECT(avar != nullptr && button != nullptr && avar->has_flap && !avar->auto_placed);
    if (avar == nullptr || button == nullptr) return;
    SC_EXPECT(Near(layout.PositionOf(*avar), layout.PositionOf(*button)));
    SC_EXPECT(!avar->flap.in_plane && avar->flap.hinge_v_m < -0.04 && avar->flap.lift_m > 0.0);
    SC_EXPECT_NEAR(avar->flap.AngleDeg(0), 0.0, 1e-12);
    SC_EXPECT_NEAR(avar->flap.AngleDeg(1), 180.0, 1e-12);
    SC_EXPECT_NEAR(avar->flap.AngleDeg(7), 180.0, 1e-12);  // past the last position: the last angle
    SC_EXPECT(avar->flap.parts.size() == 2 && avar->flap.parts[1].color == "red" && avar->flap.fixed.size() == 1);
    // The elevation wheel's lock: the knurled collar on its folded crank, turned in the face, not a toggle.
    const CabinControl* lock = layout.FindControl("ssc.el_lock");
    SC_EXPECT(lock != nullptr && lock->has_flap && lock->flap.in_plane && lock->flap.angles_deg.size() == 2 && lock->flap.parts[0].part == "SM_WheelLockCollar");
    // Every SL-2M guard has its real cover; one placed by nobody (the fire extinguisher's) still has none.
    for (const char* id : {"soc.hv_cover", "ssc.tx_cover", "fo.bort_spk_cover", "ssc.pa_balance_el_cover"}) {
        const CabinControl* c = layout.FindControl(id);
        SC_EXPECT(c != nullptr && c->has_flap);
    }
    const CabinControl* fire = layout.FindControl("fire.cover");
    SC_EXPECT(fire != nullptr && fire->auto_placed && !fire->has_flap);

    SC_EXPECT(LoadCabin(Small(R"(, "controls": [ { "control": "soc.hv_cover", "panel": "p", "at_m": [0, 0],
        "flap": { "parts": [ { "part": "SM_CoverHv" } ], "hinge_m": [0.01, 0], "axis_deg": -90, "angles_deg": [0, 110] } } ])"), console, layout));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.hv_cover", "panel": "p", "at_m": [0, 0],
        "flap": { "parts": [ { "part": "SM_CoverHv" } ], "hinge_m": [0, 0], "angles_deg": [0] } } ])"), "one angle per position"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.hv_cover", "panel": "p", "at_m": [0, 0],
        "flap": { "parts": [ { "part": "SM_CoverHv" } ], "hinge_m": [0, 0], "swing": "sideways", "angles_deg": [0, 90] } } ])"),
                            "out or plane"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.hv_cover", "panel": "p", "at_m": [0, 0],
        "flap": { "parts": [ { "part": "CoverHv" } ], "hinge_m": [0, 0], "angles_deg": [0, 90] } } ])"), "SM_"));
    SC_EXPECT(ErrorContains(Small(R"(, "controls": [ { "control": "soc.hv_cover", "panel": "p", "at_m": [0, 0],
        "flap": { "parts": [], "hinge_m": [0, 0], "angles_deg": [0, 90] } } ])"), "at least one part"));
}

SC_TEST("cabin: a meter's face is plain or arched (ДЦ), and nothing else") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const CabinDisplay* dc = nullptr;
    for (const CabinDisplay& d : layout.displays) {
        if (d.id == "dts_voltmeter") dc = &d;
    }
    SC_EXPECT(dc != nullptr && dc->face == "arched");
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "m", "label": "M", "kind": "meter", "panel": "p", "at_m": [0, 0],
        "size_m": [0.05, 0.04], "control": "soc.mti", "face": "round", "source": "sam3d", "confidence": "estimate" } ])"),
                            "arched or none"));
    SC_EXPECT(ErrorContains(Small(R"(, "displays": [ { "id": "m", "label": "M", "kind": "dial", "panel": "p", "at_m": [0, 0],
        "size_m": [0.05, 0.05], "control": "soc.mti", "face": "arched", "source": "sam3d", "confidence": "estimate" } ])"),
                            "not used by a dial"));
}

SC_TEST("cabin: a potentiometer turns between its stops through 270 degrees, a hand wheel round and round") {
    ConsoleData console;
    SC_EXPECT(LoadOsaConsole(console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const CabinControl* bright = layout.FindControl("soc.bright");
    const ControlDef* bright_def = console.panel.Find("soc.bright");
    SC_EXPECT(bright != nullptr && bright_def != nullptr);
    if (bright == nullptr || bright_def == nullptr) return;
    SC_EXPECT(bright->potentiometer);
    SC_EXPECT_NEAR(bright->units_per_turn, 360.0 / 270.0, 1e-12);  // a whole turn of the hand: more than its travel
    SC_EXPECT_NEAR(CabinLayout::WheelAngleDeg(*bright, *bright_def, 0.0), -135.0, 1e-9);
    SC_EXPECT_NEAR(CabinLayout::WheelAngleDeg(*bright, *bright_def, 0.5), 0.0, 1e-9);
    SC_EXPECT_NEAR(CabinLayout::WheelAngleDeg(*bright, *bright_def, 1.0), 135.0, 1e-9);
    // A scroll notch (1/24 of a turn) moves it a few hundredths of its range, not end to end.
    SC_EXPECT_NEAR(bright->units_per_turn / 24.0, 1.0 / 18.0, 1e-12);
    // The range wheel is a hand wheel with its own units per turn, not a potentiometer.
    const CabinControl* range = layout.FindControl("ssc.range_wheel");
    const ControlDef* range_def = console.panel.Find("ssc.range_wheel");
    SC_EXPECT(range != nullptr && range_def != nullptr && !range->potentiometer);
    if (range != nullptr && range_def != nullptr) SC_EXPECT_NEAR(CabinLayout::WheelAngleDeg(*range, *range_def, 5000.0), 360.0, 1e-9);
    // Every knob of the scopes' pictures is a potentiometer.
    for (const char* id : {"soc.focus", "soc.contrast", "soc.marks_bright", "soc.cursor_bright", "soc.gain", "ssc.bright_1",
                           "tov.bright", "fo.el_bright"}) {
        const CabinControl* c = layout.FindControl(id);
        SC_EXPECT(c != nullptr && c->potentiometer);
    }
}
