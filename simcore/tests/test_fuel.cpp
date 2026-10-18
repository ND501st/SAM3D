// The vehicle's fuel (roadmap 7.4): burn rates by power source and movement, the half-tank rule, and refuelling.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/cabin.hpp"
#include "simcore/fuel.hpp"
#include "simcore/readouts.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/scope_feed.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string Osa(const std::string& own_unit_extra, const std::string& triggers) {
    return R"({
      "scenario_id": "f", "name": "F", "source": "sam3d", "confidence": "estimate", "seed": 1,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 }, "terrain": { "type": "flat", "height_m": 0 },
      "own_unit": { "console": "procedures/osa_ak.json", "readiness": "deployed", )" + own_unit_extra +
           R"("position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "triggers": [ )" + triggers + R"( ],
      "end": { "time_limit_s": 100000, "when_targets_gone": false } })";
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

void Run(ScenarioRunner& r, double seconds, double dt = 1.0) {
    for (double end = r.Time() + seconds; r.Time() < end - 1e-9;) r.Step(dt);
}

}  // namespace

SC_TEST("fuel: the Osa's fuel block loads; bad ones are refused with paths") {
    ConsoleData c;
    std::string error;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error));
    SC_EXPECT(c.has_fuel && c.fuel.sources.size() == 2);
    SC_EXPECT_NEAR(c.fuel.tank_l, 300.0, 1e-12);
    SC_EXPECT_NEAR(c.fuel.sources[0].stationary_lph, 75.0, 1e-12);
    SC_EXPECT_NEAR(c.fuel.sources[0].moving_lph, 100.0, 1e-12);
    SC_EXPECT_NEAR(c.fuel.sources[1].stationary_lph, 25.0, 1e-12);
    std::ifstream file(kData + "/procedures/osa_ak.json", std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    const std::string text = ss.str();
    auto bad = [&](const std::string& from, const std::string& to, const std::string& expect) {
        std::string s = text;
        const std::size_t at = s.find(from);
        if (at == std::string::npos) return false;
        s.replace(at, from.size(), to);
        ConsoleData d;
        std::string e;
        const bool ok = LoadConsole(s, d, &e);
        if (ok || e.find(expect) == std::string::npos) std::printf("    expected \"%s\", got \"%s\"\n", expect.c_str(), e.c_str());
        return !ok && e.find(expect) != std::string::npos;
    };
    SC_EXPECT(bad("\"reserve_fraction\": 0.5", "\"reserve_fraction\": 1.5", "fuel.reserve_fraction"));
    SC_EXPECT(bad("\"subsystem\": \"gta\", \"stationary_lph\"", "\"subsystem\": \"nothing\", \"stationary_lph\"", "fuel.sources[0].subsystem"));
    SC_EXPECT(bad("\"tank_l\": 300", "\"tank_l\": 0", "fuel.tank_l: must be > 0"));
}

SC_TEST("fuel: the turbine burns 75 l/h at a halt and 100 l/h on the move, the take-off generator 25 l/h") {
    // Deployed: on the take-off generator and the turbine both running at a halt (the readiness state starts both).
    auto r = Load(Osa("", ""));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->Fuel() != nullptr && r->Fuel()->LevelL() == 300.0);
    Run(*r, 3600.0);
    SC_EXPECT_NEAR(r->Fuel()->BurnedL(), 75.0 + 25.0, 0.5);  // an hour of both at a halt
    // On the move, on the turbine alone.
    auto m = Load(Osa("", R"({ "id": "go", "when": {}, "do": [ { "set_input": "vehicle_stopped", "value": false } ] })"));
    SC_EXPECT(m != nullptr);
    if (!m) return;
    Run(*m, 3600.0);
    SC_EXPECT_NEAR(m->Fuel()->BurnedL(), 100.0, 1.0);  // the generator drops out when the vehicle moves
}

SC_TEST("fuel: at half the tank the crew asks for fuel and the power is cut, keeping the rest for driving") {
    auto r = Load(Osa("", ""));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const Equipment& eq = r->Console().equipment;
    SC_EXPECT(eq.IsReady("power"));
    Run(*r, 3600.0);
    SC_EXPECT(r->Fuel()->LevelL() > 150.0 && eq.IsReady("power"));
    Run(*r, 3600.0);  // 150 l at 100 l/h: the reserve is reached after 1.5 h
    SC_EXPECT_NEAR(r->Fuel()->LevelL(), 150.0, 3.0);
    SC_EXPECT(r->Fuel()->PowerCut() && eq.GetInput("fuel_power_cut"));
    Run(*r, 120.0);
    SC_EXPECT(!eq.IsReady("power") && !eq.IsReady("gta") && !eq.IsReady("pto_generator"));
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("Commander        event      fuel  asks for fuel: 150 l left") != std::string::npos);
    // The request is made once, and nothing more is burned.
    const double level = r->Fuel()->LevelL();
    Run(*r, 600.0);
    SC_EXPECT_NEAR(r->Fuel()->LevelL(), level, 0.05);
    std::size_t asks = 0;
    for (std::size_t at = 0; (at = r->Debrief().find("asks for fuel", at)) != std::string::npos; ++at) ++asks;
    SC_EXPECT(asks == 1);
}

SC_TEST("fuel: the TZM refuels the vehicle at a halt, and the power can be started again") {
    auto r = Load(Osa(R"("fuel_fraction": 0.4, )", ""));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const Equipment& eq = r->Console().equipment;
    r->Step(1.0);
    SC_EXPECT(eq.GetInput("fuel_power_cut") && !eq.IsReady("power"));  // started below the reserve: no power
    r->Console().equipment.SetInput("tzm_alongside", true);
    Run(*r, 60.0);
    SC_EXPECT_NEAR(r->Fuel()->LevelL(), 120.0 + 60.0, 1.5);  // 60 l/min
    SC_EXPECT(!eq.GetInput("fuel_power_cut"));                // above the reserve again
    // Above the reserve the turbine and generator run again, burning a little while the TZM pumps: a bit over 3 min.
    for (int i = 0; i < 400 && r->Fuel()->LevelL() < 300.0 - 1e-6; ++i) r->Step(1.0);
    SC_EXPECT_NEAR(r->Fuel()->LevelL(), 300.0, 1e-6);
    SC_EXPECT(r->Time() > 60.0 + 120.0 && r->Time() < 60.0 + 200.0);
    SC_EXPECT(r->Debrief().find("tank full") != std::string::npos);
    r->Console().equipment.SetInput("tzm_alongside", false);
    Run(*r, 200.0);  // the crew's turbine and generator come back on their switches
    SC_EXPECT(eq.IsReady("power"));
}

SC_TEST("fuel: an external supply powers the equipment at a halt, burns no fuel and is cut when the vehicle moves") {
    // Deployed, the turbine running; the crew shuts both generators down and the external supply carries the load.
    auto r = Load(Osa("", R"({ "id": "plug", "when": {}, "do": [ { "set_input": "external_power", "value": true } ] })"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Equipment& eq = r->Console().equipment;
    Run(*r, 2.0);
    SC_EXPECT(eq.IsReady("external_supply") && eq.IsReady("power"));
    r->Console().panel.Set("pwr.gta", 0);
    r->Console().panel.Set("pwr.pto", 0);
    Run(*r, 2.0);
    SC_EXPECT(!eq.IsReady("gta") && !eq.IsReady("pto_generator") && eq.IsReady("power"));
    const double level = r->Fuel()->LevelL();
    Run(*r, 3600.0);
    SC_EXPECT_NEAR(r->Fuel()->LevelL(), level, 0.05);  // nothing burns
    SC_EXPECT(!r->Fuel()->PowerCut() && eq.IsReady("power"));
    eq.SetInput("vehicle_stopped", false);  // driving off: the cable comes out
    Run(*r, 2.0);
    SC_EXPECT(!eq.IsReady("external_supply") && !eq.IsReady("power"));
}

// ---- Roadmap 7.6: a prepared position's grid and diesel set.

namespace {

std::unique_ptr<ScenarioRunner> Prepared(const std::string& block) {
    auto r = Load(Osa(R"("position_power": )" + block + ", ", ""));
    if (r) {  // the vehicle's own generators are off: the position carries the load
        r->Console().panel.Set("pwr.gta", 0);
        r->Console().panel.Set("pwr.pto", 0);
    }
    return r;
}

}  // namespace

SC_TEST("position power: the grid carries the vehicle, then the diesel set after the cut and its start delay") {
    auto r = Prepared(R"({ "grid": true, "grid_cut_s": 600, "diesel_start_s": 30, "diesel_hours": 1,
                           "source": "video_osa", "confidence": "low" })");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const Equipment& eq = r->Console().equipment;
    Run(*r, 10.0);
    SC_EXPECT(eq.IsReady("external_supply") && eq.IsReady("power") && !eq.IsReady("gta"));
    Run(*r, 580.0);  // t = 590
    SC_EXPECT(eq.IsReady("power"));
    Run(*r, 15.0);  // t = 605: the grid is cut, the diesel set is still starting
    SC_EXPECT(!eq.IsReady("power") && !eq.GetInput("external_power"));
    Run(*r, 30.0);  // t = 635
    SC_EXPECT(eq.IsReady("power") && eq.GetInput("external_power"));
    Run(*r, 3500.0);  // t = 4135: still within the hour (630 to 4230)
    SC_EXPECT(eq.IsReady("power"));
    Run(*r, 200.0);  // t = 4335: the diesel's tank is empty
    SC_EXPECT(!eq.IsReady("power"));
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("grid power on") != std::string::npos);
    SC_EXPECT(d.find("the grid is cut: the diesel set carries the position") != std::string::npos);
    SC_EXPECT(d.find("no external power") != std::string::npos);
}

SC_TEST("position power: with no diesel set the cut leaves the position dark, and a moving vehicle drops the cable") {
    auto r = Prepared(R"({ "grid": true, "grid_cut_s": 100, "source": "video_osa", "confidence": "low" })");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const Equipment& eq = r->Console().equipment;
    Run(*r, 50.0);
    SC_EXPECT(eq.IsReady("power"));
    r->Console().equipment.SetInput("vehicle_stopped", false);  // driving away from the position
    Run(*r, 2.0);
    SC_EXPECT(!eq.IsReady("external_supply") && !eq.IsReady("power"));
    r->Console().equipment.SetInput("vehicle_stopped", true);
    Run(*r, 2.0);
    SC_EXPECT(eq.IsReady("power"));  // back on the grid
    Run(*r, 100.0);
    SC_EXPECT(!eq.IsReady("power"));  // cut, and nothing else runs
    SC_EXPECT(r->Debrief().find("no diesel set runs") != std::string::npos);
}

SC_TEST("position power: the block's values are checked with paths") {
    auto r = Prepared(R"({ "grid": true, "diesel_hours": -1, "source": "video_osa", "confidence": "low" })");
    SC_EXPECT(r == nullptr);
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    const std::string text = Osa(R"("position_power": { "grid": true, "diesel_start_s": -5, "source": "x", "confidence": "low" }, )", "");
    SC_EXPECT(!LoadScenario(text, kData, def, console, &error) && error.find("own_unit.position_power.diesel_start_s") != std::string::npos);
    const std::string no_source = Osa(R"("position_power": { "grid": true }, )", "");
    SC_EXPECT(!LoadScenario(no_source, kData, def, console, &error) && error.find("own_unit.position_power") != std::string::npos);
}

SC_TEST("fuel: the driver's gauge reads the tank in litres, through the feed and the wire") {
    ConsoleData console;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", console));
    CabinLayout layout;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", console, layout, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    const CabinDisplay* gauge = nullptr;
    for (const CabinDisplay& d : layout.displays) {
        if (d.id == "fuel_gauge") gauge = &d;
    }
    SC_EXPECT(gauge != nullptr && gauge->readout == "fuel" && gauge->panel_id == "driver_panel");
    if (gauge == nullptr) return;
    auto r = Load(Osa("", ""));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ScopeFeed f = ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time(), r->Fuel());
    SC_EXPECT_NEAR(DialReading(*gauge, r->Console(), f.gyro_heading_deg, f.fuel_l), 300.0, 1e-9);
    Run(*r, 1800.0);  // half an hour of the turbine and the generator: 50 l
    f = ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time(), r->Fuel());
    SC_EXPECT_NEAR(DialReading(*gauge, r->Console(), f.gyro_heading_deg, f.fuel_l), 250.0, 0.5);
    // A client's cabin reads the same level from the knowledge stream (to the metre-step, 1 l).
    EchoKeys keys;
    const std::vector<std::uint8_t> bytes = em::wire::ScopeBytes(ToViewScope(f, keys));
    em::wire::Reader rd(bytes);
    em::ViewScope back;
    SC_EXPECT(em::wire::GetScope(rd, back));
    SC_EXPECT_NEAR(FromViewScope(back).fuel_l, f.fuel_l, 0.5);
    // Without a fuel model the feed says so and the gauge rests at zero.
    const ScopeFeed none = ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time());
    SC_EXPECT(none.fuel_l < 0.0);
    SC_EXPECT_NEAR(DialReading(*gauge, r->Console(), 0.0, none.fuel_l), 0.0, 1e-12);
    // The cabin refuses a fuel gauge on a console with no fuel block.
    ConsoleData bare = console;
    bare.has_fuel = false;
    CabinLayout refused;
    std::string why;
    SC_EXPECT(!LoadCabinFile(kData + "/cabins/osa_ak.json", bare, refused, &why));
    SC_EXPECT(why.find("fuel needs the console's fuel block") != std::string::npos);
}
