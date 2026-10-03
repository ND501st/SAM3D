#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/cabin.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/scopes.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

Contact At(const std::string& id, double az_deg, double range_m) { return {id, range_m, az_deg, 1.0}; }

}  // namespace

SC_TEST("scopes: the sweep paints a contact only when it passes over it") {
    SearchScope scope(2.0, 3.0);  // one turn in 2 s: 180 deg/s
    const std::vector<Contact> picture = {At("t1", 90.0, 20000.0)};
    scope.Update(0.4, true, picture);  // sweep 0 -> 72 deg: not there yet
    SC_EXPECT(scope.Blips().empty());
    SC_EXPECT_NEAR(scope.SweepAzimuthRad(), DegToRad(72.0), 1e-9);
    scope.Update(0.2, true, picture);  // 72 -> 108 deg: passes 90 deg 0.1 s before the end of the step
    const std::vector<SearchScope::Blip> blips = scope.Blips();
    SC_EXPECT(blips.size() == 1);
    if (blips.size() != 1) return;
    SC_EXPECT_NEAR(blips[0].azimuth_rad, DegToRad(90.0), 1e-9);
    SC_EXPECT_NEAR(blips[0].range_m, 20000.0, 1e-9);
    SC_EXPECT_NEAR(blips[0].brightness, 1.0 - 0.1 / 3.0, 1e-9);
}

SC_TEST("scopes: echoes fade over the afterglow and repaint once per turn") {
    SearchScope scope(2.0, 3.0);
    const std::vector<Contact> picture = {At("t1", -85.0, 10000.0)};  // 275 deg: painted at 275/180 s
    for (int i = 0; i < 20; ++i) scope.Update(0.1, true, picture);   // exactly one turn
    SC_EXPECT(scope.Blips().size() == 1);
    for (int i = 0; i < 20; ++i) scope.Update(0.1, true, picture);   // second turn: a second, brighter paint
    std::vector<SearchScope::Blip> blips = scope.Blips();
    SC_EXPECT(blips.size() == 2);
    if (blips.size() == 2) SC_EXPECT_NEAR(blips[0].brightness, 1.0 - (4.0 - 275.0 / 180.0) / 3.0, 1e-6);
    for (int i = 0; i < 40; ++i) scope.Update(0.1, false, picture);  // antenna stopped: nothing new, all fade
    SC_EXPECT(scope.Blips().empty());
    const double sweep = scope.SweepAzimuthRad();  // back at the start, give or take rounding
    SC_EXPECT(std::fmin(sweep, kTwoPi - sweep) < 1e-6);
}

SC_TEST("scopes: a step longer than a turn paints everything once") {
    SearchScope scope(1.0, 3.0);
    scope.Update(1.5, true, {At("a", 10.0, 1000.0), At("b", 200.0, 2000.0)});
    SC_EXPECT(scope.Blips().size() == 2);
}

SC_TEST("scopes: the tracking radar's range indicator shows targets in its beam while it radiates") {
    // The operators acquire; once the SSC tracks the target, its echo is on the range indicator.
    const std::string text = R"({
      "scenario_id": "e", "name": "E", "source": "sam3d", "confidence": "estimate", "seed": 1,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 },
      "terrain": { "type": "flat", "height_m": 0 },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [ { "id": "t1", "name": "T", "class": "drone", "start": { "east_m": 1500, "north_m": 30000, "up_m": 1000 }, "appear_at_s": 230,
                     "waypoints": [ { "east_m": 1500, "north_m": -20000, "up_m": 1000 } ], "speed_mps": 150 } ],
      "triggers": [
        { "id": "up", "when": {}, "do": [ { "order": "power_up", "crew": "Driver" },
                                           { "queue": "pto_on_line", "crew": "Driver" },
                                           { "order": "equipment_on", "crew": "Operator" },
                                           { "queue": "deploy_at_halt", "crew": "Operator" },
                                           { "order": "search_on_march", "crew": "Senior operator" } ] },
        { "id": "acq", "when": { "conditions": [ { "subsystem": "soc_transmitter", "is": "ready" } ] },
          "do": [ { "order": "acquire_with_soc", "crew": "Senior operator" }, { "order": "ready_capture", "crew": "Deputy" },
                  { "order": "capture_with_ssc", "crew": "Operator" } ] } ],
      "end": { "time_limit_s": 600 } })";
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(text, kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner runner(std::move(def), std::move(console));
    bool silent_before = true;
    while (!runner.Finished() && runner.Engagement().TrackedTarget().empty()) {
        runner.Step(0.1);
        if (!runner.Console().equipment.IsReady(runner.Console().track.transmitter)) {
            silent_before = silent_before && runner.Engagement().TrackReturns().empty();
        }
    }
    SC_EXPECT(silent_before);
    SC_EXPECT(runner.Engagement().TrackedTarget() == "t1");
    runner.Step(0.1);
    const std::vector<Contact>& returns = runner.Engagement().TrackReturns();
    SC_EXPECT(returns.size() == 1 && returns[0].target_id == "t1");
    if (returns.size() == 1) {
        // The radar's range wheel follows the target, so the echo sits in the gate.
        SC_EXPECT_NEAR(returns[0].range_m, runner.Console().panel.GetValue(runner.Console().track.range_wheel), 50.0);
    }
}

SC_TEST("scopes: the Osa layout's displays load and switch with the console") {
    ConsoleData console;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", console));
    CabinLayout layout;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(layout.displays.size() == 7);
    const CabinDisplay* beta = nullptr;
    const CabinDisplay* dts = nullptr;
    for (const CabinDisplay& d : layout.displays) {
        if (d.id == "beta_c") beta = &d;
        if (d.id == "dts_voltmeter") dts = &d;
    }
    SC_EXPECT(beta != nullptr && dts != nullptr);
    if (beta == nullptr || dts == nullptr) return;
    SC_EXPECT(beta->kind == CabinDisplayKind::Dial && beta->control == "ssc.az_wheel");
    SC_EXPECT_NEAR(dts->scale * 10000.0, 10.0, 1e-12);  // 10 km reads 10 V

    // The dial needs power to show, and its ПОДСВЕТКА switch to be lit.
    SC_EXPECT(!CabinLayout::DisplayOn(*beta, console.panel, console.equipment));
    console.panel.Set("pwr.gta", 1);
    for (int i = 0; i < 3000 && !console.equipment.IsReady("power"); ++i) console.equipment.Update(0.1, console.panel);
    SC_EXPECT(CabinLayout::DisplayOn(*beta, console.panel, console.equipment));
    SC_EXPECT(!CabinLayout::DisplayLit(*beta, console.panel, console.equipment));
    console.panel.Set("ssc.az_light", 1);
    SC_EXPECT(CabinLayout::DisplayLit(*beta, console.panel, console.equipment));
    // The voltmeter only reads while the SSC tracks in range.
    SC_EXPECT(!CabinLayout::DisplayOn(*dts, console.panel, console.equipment));
}

SC_TEST("scopes: the loader rejects bad displays") {
    ConsoleData console;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", console));
    auto Error = [&](const std::string& display) {
        const std::string text = R"({ "system_id": "osa_ak",
          "stations": [ { "id": "senior_operator", "label": "SO", "eye_m": [0, 0, 1.5] },
                        { "id": "operator", "label": "O", "eye_m": [0, 1, 1.5] },
                        { "id": "deputy_commander", "label": "DC", "eye_m": [1, 0, 1.5] },
                        { "id": "commander", "label": "C", "eye_m": [1, 3, 1.5] },
                        { "id": "driver", "label": "D", "eye_m": [-1, 3, 1.5] } ],
          "panels": [ { "id": "p", "label": "P", "station": "senior_operator", "centre_m": [-1, 0, 1.3],
                        "facing_deg": 90, "size_m": [1.0, 0.6] } ],
          "displays": [ )" + display + " ] }";
        CabinLayout layout;
        std::string error;
        if (LoadCabin(text, console, layout, &error)) return std::string("loaded");
        return error;
    };
    const std::string ok = R"({ "id": "d", "label": "D", "kind": "dial", "panel": "p", "at_m": [0, 0], "size_m": [0.1, 0.1],
                                "control": "ssc.az_wheel", "source": "s", "confidence": "low" })";
    SC_EXPECT(Error(ok) == "loaded");
    const auto Has = [](const std::string& error, const std::string& want) {
        if (error.find(want) == std::string::npos) std::printf("    got: %s\n", error.c_str());
        return error.find(want) != std::string::npos;
    };
    SC_EXPECT(Has(Error(R"({ "id": "d", "label": "D", "kind": "dial", "panel": "p", "at_m": [0.48, 0], "size_m": [0.1, 0.1],
                            "control": "ssc.az_wheel", "source": "s", "confidence": "low" })"),
                  "displays[0].at_m: display doesn't fit"));
    SC_EXPECT(Has(Error(R"({ "id": "d", "label": "D", "kind": "dial", "panel": "p", "at_m": [0, 0], "size_m": [0.1, 0.1],
                            "source": "s", "confidence": "low" })"),
                  "displays[0].control: required"));
    SC_EXPECT(Has(Error(R"({ "id": "d", "label": "D", "kind": "ppi", "panel": "p", "at_m": [0, 0], "size_m": [0.1, 0.1],
                            "control": "ssc.az_wheel", "source": "s", "confidence": "low" })"),
                  "displays[0].control: not used by a ppi"));
    SC_EXPECT(Has(Error(R"({ "id": "d", "label": "D", "kind": "radar", "panel": "p", "at_m": [0, 0], "size_m": [0.1, 0.1],
                            "source": "s", "confidence": "low" })"),
                  "displays[0].kind"));
    SC_EXPECT(Has(Error(R"({ "id": "d", "label": "D", "kind": "ppi", "panel": "p", "at_m": [0, 0], "size_m": [0.1, 0.1],
                            "sweep_period_s": 0, "source": "s", "confidence": "low" })"),
                  "must be positive"));
    SC_EXPECT(Has(Error(R"({ "id": "d", "label": "D", "kind": "tv", "panel": "p", "at_m": [0, 0], "size_m": [0.1, 0.1] })"),
                  "displays[0].source"));
}

namespace {

Contact Up(const std::string& id, double range_m, double el_deg) { return {id, range_m, 0.0, el_deg}; }

}  // namespace

SC_TEST("scopes: the elevation-search scan paints a target in its beam and range window") {
    ElevationScope scope(2.0, 1.5, 1.0);  // up 0 -> 6 deg in 1 s, back down in 1 s; beam 1 deg
    const std::vector<Contact> returns = {Up("near", 10300.0, 4.0), Up("far", 12000.0, 4.0)};
    // Gate at 10 km, 2 km window: "far" (+2 km) is off the screen.
    scope.Update(0.5, true, true, 0.0, 0.0, 6.0, 10000.0, 2000.0, returns);  // 0 -> 3 deg: below 4 - 0.5
    SC_EXPECT(scope.Marks().empty());
    SC_EXPECT_NEAR(scope.AntennaElevationDeg(), 3.0, 1e-9);
    scope.Update(0.25, true, true, 0.0, 0.0, 6.0, 10000.0, 2000.0, returns);  // 3 -> 4.5 deg: passes 4
    const std::vector<ElevationScope::Mark> marks = scope.Marks();
    SC_EXPECT(marks.size() == 1);
    if (marks.size() != 1) return;
    SC_EXPECT(marks[0].target_id == "near");
    SC_EXPECT_NEAR(marks[0].range_offset_m, 300.0, 1e-9);
    SC_EXPECT_NEAR(marks[0].elevation_deg, 4.0, 1e-9);
    SC_EXPECT_NEAR(marks[0].brightness, 1.0, 1e-9);
    // Top of the sweep and back down past it again within the afterglow.
    scope.Update(0.75, true, true, 0.0, 0.0, 6.0, 10000.0, 2000.0, returns);
    SC_EXPECT(scope.Marks().size() == 1);
    // Transmitter off: nothing new, the mark fades out.
    for (int i = 0; i < 20; ++i) scope.Update(0.1, false, true, 0.0, 0.0, 6.0, 10000.0, 2000.0, returns);
    SC_EXPECT(scope.Marks().empty());
}

SC_TEST("scopes: without the scan the elevation-search indicator shows only what the antenna points at") {
    ElevationScope scope(2.0, 1.5, 1.0);
    const std::vector<Contact> returns = {Up("t", 8000.0, 7.0)};
    scope.Update(0.1, true, false, 5.0, 3.0, 9.0, 8000.0, 2000.0, returns);  // antenna at 5 deg, target at 7
    SC_EXPECT(scope.Marks().empty());
    SC_EXPECT_NEAR(scope.AntennaElevationDeg(), 5.0, 1e-9);
    scope.Update(0.1, true, false, 6.6, 3.0, 9.0, 8000.0, 2000.0, returns);  // within half a beam width
    SC_EXPECT(scope.Marks().size() == 1);
    // Tracking: the antenna stays on the target, so the mark is repainted every step and stays bright.
    for (int i = 0; i < 30; ++i) scope.Update(0.1, true, false, 7.0, 3.0, 9.0, 8000.0, 2000.0, returns);
    SC_EXPECT(scope.Marks().size() == 1 && scope.Marks()[0].brightness > 0.99);
}

SC_TEST("scopes: the Osa's elevation-search indicator follows the beam selector and scans during target designation") {
    ConsoleData console;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", console));
    CabinLayout layout;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout));
    const CabinDisplay* elev = nullptr;
    for (const CabinDisplay& d : layout.displays) {
        if (d.id == "elev_search") elev = &d;
    }
    SC_EXPECT(elev != nullptr);
    if (elev == nullptr) return;
    SC_EXPECT(elev->kind == CabinDisplayKind::ElevationScope && elev->rasters.size() == 3);
    // voenizdat_osa_1991 p95: 31 deg of elevation (-1 to 30) over the three beams, 2 km of range.
    SC_EXPECT_NEAR(elev->rasters.front().el_low_deg, -1.0, 1e-12);
    SC_EXPECT_NEAR(elev->rasters.back().el_high_deg, 30.0, 1e-12);
    SC_EXPECT_NEAR(elev->range_window_m, 2000.0, 1e-12);
    // ЛУЧ I, II, III pick their raster; the automatic programmes pick the one holding the antenna's elevation.
    for (int beam = 0; beam < 3; ++beam) {
        console.panel.Set("soc.beam", beam);
        SC_EXPECT(CabinLayout::ElevationRasterFor(*elev, console.panel, 0.0) == beam);
    }
    console.panel.Set("soc.beam", 3);
    SC_EXPECT(CabinLayout::ElevationRasterFor(*elev, console.panel, 2.0) == 0);
    SC_EXPECT(CabinLayout::ElevationRasterFor(*elev, console.panel, 20.0) == 2);
    SC_EXPECT(CabinLayout::ElevationRasterFor(*elev, console.panel, 45.0) == 2);
    // Dark and still until the equipment has warmed up and ЦУ is engaged.
    SC_EXPECT(!CabinLayout::DisplayOn(*elev, console.panel, console.equipment));
    SC_EXPECT(!CabinLayout::ElevationScanOn(*elev, console.panel, console.equipment));
}

SC_TEST("scopes: the loader checks an elevation scope's rasters and beam map") {
    ConsoleData console;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", console));
    auto Error = [&](const std::string& extra) {
        const std::string text = R"({ "system_id": "osa_ak",
          "stations": [ { "id": "senior_operator", "label": "SO", "eye_m": [0, 0, 1.5] },
                        { "id": "operator", "label": "O", "eye_m": [0, 1, 1.5] },
                        { "id": "deputy_commander", "label": "DC", "eye_m": [1, 0, 1.5] },
                        { "id": "commander", "label": "C", "eye_m": [1, 3, 1.5] },
                        { "id": "driver", "label": "D", "eye_m": [-1, 3, 1.5] } ],
          "panels": [ { "id": "p", "label": "P", "station": "senior_operator", "centre_m": [-1, 0, 1.3],
                        "facing_deg": 90, "size_m": [1.0, 0.6] } ],
          "displays": [ { "id": "e", "label": "E", "kind": "elevation_scope", "panel": "p", "at_m": [0, 0],
                          "size_m": [0.1, 0.1], "source": "s", "confidence": "low")" + extra + " } ] }";
        CabinLayout layout;
        std::string error;
        if (LoadCabin(text, console, layout, &error)) return std::string("loaded");
        return error;
    };
    const auto Has = [](const std::string& error, const std::string& want) {
        if (error.find(want) == std::string::npos) std::printf("    got: %s\n", error.c_str());
        return error.find(want) != std::string::npos;
    };
    const std::string rasters = R"(, "rasters": [ { "el_deg": [-1, 5], "screen": [0, 0.7] } ])";
    SC_EXPECT(Error(rasters) == "loaded");
    SC_EXPECT(Has(Error(""), "displays[0].rasters: required"));
    SC_EXPECT(Has(Error(R"(, "rasters": [ { "el_deg": [5, -1], "screen": [0, 0.7] } ])"), "rasters[0].el_deg"));
    SC_EXPECT(Has(Error(R"(, "rasters": [ { "el_deg": [-1, 5], "screen": [0.2, 1.4] } ])"), "rasters[0].screen"));
    SC_EXPECT(Has(Error(rasters + R"(, "beam_control": "soc.beam", "raster_by_position": [0, 0])"),
                  "one entry per position"));
    SC_EXPECT(Has(Error(rasters + R"(, "beam_control": "soc.beam", "raster_by_position": [0, 0, 1, -1, -1])"),
                  "raster_by_position[2]"));
    SC_EXPECT(Has(Error(rasters + R"(, "beam_control": "soc.beam")"), "raster_by_position: required"));
    SC_EXPECT(Has(Error(rasters + R"(, "magnified_window_m": 100)"), "not used by a elevation_scope"));
}
