// The battery command post's plotting board (roadmap 7.1): data, updates once a minute, the board's notation, the
// navigation error, and the crews aiming by it with the radars silent.
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/pu12.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

PlottingBoardDef Exact() {  // the Osa data's board without navigation error
    PlottingBoardDef d;
    std::string error;
    if (!LoadPlottingBoardFile(kData + "/command/pu12.json", d, &error)) std::printf("    board: %s\n", error.c_str());
    d.navigation_error_deg = 0.0;
    d.navigation_error_m = 0.0;
    return d;
}

double Wrap180(double deg) {
    double d = std::fmod(deg + 180.0, 360.0);
    if (d < 0.0) d += 360.0;
    return d - 180.0;
}

BoardContact At(const std::string& id, double az_deg, double range_m, double alt_m, int type = 9, int formation = 1) {
    BoardContact c;
    c.target_id = id;
    c.position = Vec3{range_m * std::sin(DegToRad(az_deg)), range_m * std::cos(DegToRad(az_deg)), alt_m};
    c.altitude_m = alt_m;
    c.type = type;
    c.formation = formation;
    return c;
}

}  // namespace

namespace {

std::string ReadScenarioText() {
    return R"({
      "scenario_id": "b", "name": "B", "source": "sam3d", "confidence": "estimate", "seed": 3,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 }, "terrain": { "type": "flat", "height_m": 0 },
      "command_post": "command/pu12.json",
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [ { "id": "t1", "name": "T", "class": "drone", "board_type": 2,
                     "start": { "east_m": 2000, "north_m": 45000, "up_m": 1000 },
                     "waypoints": [ { "east_m": 2000, "north_m": -30000, "up_m": 1000 } ], "speed_mps": 150 } ],
      "triggers": [ { "id": "tv", "when": {}, "do": [ { "order": "emcon_tv_azimuth", "crew": "Senior operator" },
                                                     { "order": "emcon_tv_engagement", "crew": "Deputy" } ] } ],
      "end": { "time_limit_s": 400, "when_targets_gone": false } })";
}

}  // namespace

SC_TEST("pu12: the board's data loads with every value cited; bad files are refused with paths") {
    PlottingBoardDef d;
    std::string error;
    SC_EXPECT(LoadPlottingBoardFile(kData + "/command/pu12.json", d, &error));
    SC_EXPECT_NEAR(d.update_period_s, 60.0, 1e-12);
    SC_EXPECT(d.radar_battery_number == 62 && d.range_rings_m.size() == 4);
    SC_EXPECT_NEAR(d.navigation_error_deg, 5.0, 1e-12);
    SC_EXPECT_NEAR(d.navigation_error_m, 3000.0, 1e-12);
    SC_EXPECT(!LoadPlottingBoardFile(kData + "/command/nothing.json", d, &error) && error.find("cannot open") != std::string::npos);
}

SC_TEST("pu12: the board is written once a minute, each target keeping its number") {
    PlottingBoard b(Exact(), Vec3{}, 1);
    SC_EXPECT(b.Update(0.0, {At("a", 0.0, 40000.0, 1000.0), At("b", 90.0, 80000.0, 3000.0)}));
    SC_EXPECT(b.Stamp() == 1 && b.Current().size() == 2);
    SC_EXPECT(b.Current()[0].number == 6201 && b.Current()[1].number == 6202);
    SC_EXPECT(!b.Update(30.0, {At("a", 0.0, 35000.0, 1000.0)}));  // not yet a minute
    SC_EXPECT(b.Update(60.0, {At("b", 90.0, 70000.0, 3000.0), At("c", 180.0, 250000.0, 3000.0)}));
    SC_EXPECT(b.Stamp() == 2);
    const std::vector<BoardMark> now = b.Current();
    SC_EXPECT(now.size() == 1 && now[0].number == 6202 && now[0].target_id == "b");  // c is beyond the 200 km ring
    SC_EXPECT(b.Marks().size() == 3);  // the history stays on the board
}

SC_TEST("pu12: the notation reads as the manual's example: 6201 16/51, one own plane at 15-50 mils, 35 km") {
    // samsim_osa p20: "One own plane, direction 15-50 angular mil, range 35km, height 1,600m": 6201, 16 over 51.
    PlottingBoard b(Exact(), Vec3{}, 1);
    b.Update(0.0, {At("own", 1550.0 / 6000.0 * 360.0, 35000.0, 1600.0, 5, 1)});
    const BoardMark m = b.Current().front();
    SC_EXPECT(m.number == 6201 && m.height_hm == 16 && m.Code() == 51);
    SC_EXPECT(b.Describe(m) == "6201 16/51 at 35 km, 15-50");
    const PlottingBoard::Designation d = b.DesignationFor(m, Vec3{});
    SC_EXPECT(d.azimuth_mils == 1550);
    SC_EXPECT_NEAR(d.range_m, 35000.0, 1e-6);
}

SC_TEST("pu12: the navigation error is the same for every mark of a board, and inside 5 degrees and 3 km") {
    PlottingBoardDef def;
    std::string error;
    SC_EXPECT(LoadPlottingBoardFile(kData + "/command/pu12.json", def, &error));
    double worst_az = 0.0;
    bool any = false;
    for (unsigned seed = 1; seed <= 40; ++seed) {
        PlottingBoard b(def, Vec3{}, seed);
        b.Update(0.0, {At("a", 0.0, 100000.0, 1000.0), At("b", 90.0, 100000.0, 1000.0)});
        const std::vector<BoardMark> m = b.Current();
        const double az_a = Wrap180(RadToDeg(AzimuthOf(m[0].position)));
        const double az_b = Wrap180(RadToDeg(AzimuthOf(m[1].position)) - 90.0);
        // At 100 km a 3 km offset is under 1.8 degrees: the two errors agree within that, and stay inside 5 + 1.8.
        SC_EXPECT(std::fabs(az_a - az_b) < 3.5);
        worst_az = std::fmax(worst_az, std::fabs(az_a));
        SC_EXPECT(std::fabs(az_a) < 5.0 + 1.8);
        any = any || std::fabs(az_a) > 0.5;
    }
    SC_EXPECT(any);  // the error isn't zero
    std::printf("    worst azimuth error over 40 boards: %.1f deg\n", worst_az);
}

SC_TEST("pu12: a scenario's command post plots the targets and the crew aims by the board, not the truth") {
    const std::string text = ReadScenarioText();
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(text, kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner runner(std::move(def), std::move(console));
    SC_EXPECT(runner.Board() != nullptr);
    if (runner.Board() == nullptr) return;
    for (int i = 0; i < 1210; ++i) runner.Step(0.1);  // 121 s: the board is written at 0.1, 60.1 and 120.1 s
    const PlottingBoard& b = *runner.Board();
    SC_EXPECT(b.Stamp() == 3);  // at 0, 60 and 120 s
    SC_EXPECT(b.Current().size() == 1 && b.Current()[0].Code() == 21);  // training target (bguir_tsp), one aircraft
    // 1000 m in the flat frame at 45 km is about 1160 m above sea level (the Earth's curvature): 12 hectometres.
    SC_EXPECT(runner.Debrief().find("PU-12            event      t1  on the board: 6201 12/21") != std::string::npos);
    // The senior operator's azimuth wheel is on the last designation the crew was told (true azimuth), not on the
    // target: the 120 s plot is still on the air (4 s + 8 s + 2 s), so it is the 60 s one.
    SC_EXPECT(b.Known(runner.Time()).size() == 1 && b.Known(runner.Time())[0].stamp == 2);
    const BoardMark mark = b.Known(runner.Time())[0];
    const double board_az = Wrap180(RadToDeg(AzimuthOf(mark.position - runner.UnitPosition())));
    const double wheel = runner.Console().panel.GetValue("ssc.az_wheel");
    std::printf("    wheel %.2f deg, board %.2f deg\n", wheel, board_az);
    SC_EXPECT(std::fabs(wheel - board_az) < 0.5);
}

SC_TEST("pu12: the battery commander designates by voice, one message at a time, and the crew knows it after the pass-on") {
    PlottingBoardDef def = Exact();
    SC_EXPECT_NEAR(def.designation_range_m, 50000.0, 1e-9);
    SC_EXPECT_NEAR(def.operator_delay_s + def.message_s + def.pass_on_s, 14.0, 1e-9);
    PlottingBoard b(def, Vec3{}, 1);
    b.Update(0.0, {At("a", 0.0, 40000.0, 1000.0), At("b", 90.0, 45000.0, 1000.0), At("far", 180.0, 80000.0, 3000.0)});
    SC_EXPECT(b.MessageCount() == 2);  // the target 80 km out isn't designated
    SC_EXPECT(b.Known(13.9).empty() && b.PollHeard(13.9).empty());
    // "a": 4 s delay + 8 s message + 2 s pass-on = 14 s. "b" waits for the radio: 12 s + 4 + 8 + 2 = 26 s.
    std::vector<BoardMark> heard = b.PollHeard(14.0);
    SC_EXPECT(heard.size() == 1 && heard[0].target_id == "a");
    SC_EXPECT(b.Known(20.0).size() == 1);
    SC_EXPECT(b.PollHeard(25.9).empty());
    heard = b.PollHeard(26.0);
    SC_EXPECT(heard.size() == 1 && heard[0].target_id == "b");
    SC_EXPECT(b.Known(30.0).size() == 2);
    SC_EXPECT(b.PollHeard(100.0).empty());  // each message is reported once
}

SC_TEST("pu12: a new plot of a target is designated again, and the crew knows the newest one") {
    PlottingBoard b(Exact(), Vec3{}, 1);
    b.Update(0.0, {At("a", 0.0, 40000.0, 1000.0)});
    b.Update(60.0, {At("a", 0.0, 30000.0, 1000.0)});
    SC_EXPECT(b.MessageCount() == 2);
    SC_EXPECT(b.Known(30.0).size() == 1 && b.Known(30.0)[0].stamp == 1);   // the first is still the latest told
    SC_EXPECT(b.Known(75.0).size() == 1 && b.Known(75.0)[0].stamp == 2);   // told again 14 s after the second plot
    const PlottingBoard::Designation d = b.DesignationFor(b.Known(75.0)[0], Vec3{});
    SC_EXPECT_NEAR(d.range_m, 30000.0, 1e-6);
}

SC_TEST("pu12: before the first designation is passed on, the senior operator's wheel doesn't move") {
    // Same scenario as above: the target is first on the board at 0.1 s, designated by 14.1 s.
    const std::string text = ReadScenarioText();
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(text, kData, def, console, &error));
    ScenarioRunner runner(std::move(def), std::move(console));
    const double wheel0 = runner.Console().panel.GetValue("ssc.az_wheel");
    for (int i = 0; i < 130; ++i) runner.Step(0.1);  // 13 s
    SC_EXPECT_NEAR(runner.Console().panel.GetValue("ssc.az_wheel"), wheel0, 1e-9);
    SC_EXPECT(runner.Debrief().find("Commander        event      t1  designation 6201") == std::string::npos);
    for (int i = 0; i < 120; ++i) runner.Step(0.1);  // 25 s: told at 14.1 s
    SC_EXPECT(runner.Debrief().find("Commander        event      t1  designation 6201") != std::string::npos);
    SC_EXPECT(std::fabs(runner.Console().panel.GetValue("ssc.az_wheel") - wheel0) > 1.0);
}
