// The vehicle's fuel (roadmap 7.4): burn rates by power source and movement, the half-tank rule, and refuelling.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <utility>

#include "simcore/fuel.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
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
