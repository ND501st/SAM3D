// The vehicle's own systems (roadmap controls C1-C3, C7, C8; simcore/vehicle_systems.hpp): the engine and its cold start,
// the drive's speed cap, the 24 V network and the supply's voltage and frequency, the lights and the light discipline,
// the cabin's climate, fire; and the C9 controls wired from sources (МЦ, ОТКАЗ).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/cabin.hpp"
#include "simcore/em/replication.hpp"
#include "simcore/launcher.hpp"
#include "simcore/nav.hpp"
#include "simcore/readouts.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/scope_feed.hpp"
#include "simcore/vehicle_systems.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string Osa(const std::string& readiness, const std::string& crew = R"({ "name": "Commander", "role": "commander" })") {
    return R"({
      "scenario_id": "vs", "name": "VS", "source": "sam3d", "confidence": "estimate", "seed": 3,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 }, "terrain": { "type": "flat", "height_m": 0 },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", )" +
           (readiness.empty() ? std::string() : "\"readiness\": \"" + readiness + "\", ") +
           R"("position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ )" + crew + R"( ],
      "triggers": [ ],
      "end": { "time_limit_s": 100000, "when_targets_gone": false } })";
}

// The scenario with the air at `outside_c` (dewpoint `dew_c`).
std::unique_ptr<ScenarioRunner> Load(const std::string& text, double outside_c = 15.0, double dew_c = 5.0) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    def.weather.surface_temperature_c = outside_c;
    def.weather.surface_dewpoint_c = dew_c;
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

void Run(ScenarioRunner& r, double seconds, double dt = 0.5) {
    for (double end = r.Time() + seconds; r.Time() < end - 1e-9;) r.Step(dt);
}

void Press(ScenarioRunner& r, const std::string& id) {
    r.Console().panel.Set(id, 1);
    r.Step(0.5);
}

}  // namespace

SC_TEST("vehicle systems: the Osa's file loads with every part, its speeds converted; a bad file is refused") {
    VehicleSystemsDef d;
    std::string error;
    SC_EXPECT(LoadVehicleSystemsFile(kData + "/vehicles/osa_ak_systems.json", d, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(d.has_engine && d.has_drive && d.has_electrics && d.has_lights && d.has_climate && d.has_fire);
    SC_EXPECT_NEAR(d.drive.road_mps, 60.0 / 3.6, 1e-9);    // voenizdat_osa_1991 p16
    SC_EXPECT_NEAR(d.drive.afloat_mps, 8.0 / 3.6, 1e-9);
    SC_EXPECT_NEAR(d.electrics.shed_hz, 360.0, 1e-9);      // p163: the УКЧ at 360 Hz
    SC_EXPECT(d.fire.zones.size() == 2 && d.fire.bottles == 3);
    SC_EXPECT_NEAR(d.climate.winter_below_c, -10.0, 1e-9);  // mon_2112_80 p68
    VehicleSystemsDef bad;
    SC_EXPECT(!LoadVehicleSystems("{ \"engine\": 3 }", bad, &error));
    SC_EXPECT(!LoadVehicleSystems("not json", bad, &error));
    // Every value carries its source.
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(Osa("deployed"), kData, def, console, &error));
    SC_EXPECT(def.has_vehicle_systems);
}

SC_TEST("vehicle systems (C1): a cold engine starts only after the pre-heater; warm, the starter in neutral runs it, the stop stops it") {
    auto r = Load(Osa(""), -25.0, -30.0);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ScenarioRunner& s = *r;
    const VehicleSystems* v = s.OwnSystems();
    SC_EXPECT(v != nullptr);
    if (v == nullptr) return;
    Run(s, 1.0);
    SC_EXPECT_NEAR(v->CoolantC(), -25.0, 0.5);
    SC_EXPECT(s.Console().equipment.GetInput("engine_cold"));
    s.Console().panel.Set("veh.well_switch", 1);
    s.Console().panel.Set("veh.gear", 0);
    Press(s, "veh.starter");
    Run(s, 10.0);
    SC_EXPECT(s.Console().equipment.State("engine") == SubsystemState::Off);  // too cold: it will not take
    // The pre-heater: its glow plug, its button, then it warms the coolant.
    s.Console().panel.Set("heat.plug", 1);
    Press(s, "heat.start");
    Run(s, 800.0);
    std::printf("    pre-heated: coolant %.1f C\n", v->CoolantC());
    SC_EXPECT(!s.Console().equipment.GetInput("engine_cold"));
    Press(s, "veh.starter");
    Run(s, 10.0);
    SC_EXPECT(s.Console().equipment.IsReady("engine") && v->EngineRunning());
    SC_EXPECT(v->Rpm() > 600.0 && v->OilPressureKgfCm2() > 1.5);
    // In gear it will not start: stop it, put it in first and try.
    Press(s, "veh.engine_stop");
    Run(s, 2.0);
    SC_EXPECT(s.Console().equipment.State("engine") == SubsystemState::Off && !v->EngineRunning());
    s.Console().panel.Set("veh.gear", 1);
    Press(s, "veh.starter");
    Run(s, 5.0);
    SC_EXPECT(s.Console().equipment.State("engine") == SubsystemState::Off);
}

SC_TEST("vehicle systems (C1, C2): the gear, handbrake, flaps, wave shield and lights cap the speed; the gear for a speed") {
    auto r = Load(Osa("deployed"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ScenarioRunner& s = *r;
    Run(s, 1.0);
    const VehicleSystems* v = s.OwnSystems();
    SC_EXPECT(v != nullptr && v->EngineRunning());
    if (v == nullptr) return;
    ControlPanel& p = s.Console().panel;
    VehicleSystems::World w;
    p.Set("veh.handbrake", 1);
    p.Set("veh.gear", 5);
    SC_EXPECT_NEAR(v->SpeedCapMps(s.Console(), w), 0.0, 1e-12);  // the brake on
    p.Set("veh.handbrake", 0);
    SC_EXPECT_NEAR(v->SpeedCapMps(s.Console(), w), 60.0 / 3.6, 1e-9);
    p.Set("veh.gear", 1);
    SC_EXPECT_NEAR(v->SpeedCapMps(s.Console(), w), 8.0 / 3.6, 1e-9);
    p.Set("veh.gear", 0);
    SC_EXPECT_NEAR(v->SpeedCapMps(s.Console(), w), 0.0, 1e-12);  // neutral
    SC_EXPECT(v->GearFor(36.0 / 3.6) == 4 && v->GearFor(0.0) == 0 && v->GearFor(100.0) == 5);
    // At night with no headlights: the blackout's 25 km/h; with them, the road's.
    p.Set("veh.gear", 5);
    w.dark = true;
    p.Set("veh.outside_lights", 0);
    Run(s, 0.5);
    SC_EXPECT_NEAR(v->SpeedCapMps(s.Console(), w), 25.0 / 3.6, 1e-9);
    SC_EXPECT(!v->OutsideLit() && v->SeenAtNightM() == 150.0 && v->NightFindFactor() < 0.5);
    p.Set("veh.outside_lights", 1);
    p.Set("veh.light_switch", 2);
    Run(s, 0.5);
    SC_EXPECT(v->Headlights() && v->OutsideLit() && v->NightFindFactor() == 1.0);
    {
        // A client's mirror of the console draws the same lamps without stepping the systems.
        const ConsoleData mirror = s.Console();
        const OutsideLamps o = OutsideLampsOf(s.Def().vehicle_systems, mirror);
        SC_EXPECT(o.headlights == v->Headlights() && o.sidelights == v->Sidelights() && o.rear == v->RearLamps() && o.Any());
        ConsoleData dark = mirror;
        dark.panel.Set("veh.outside_lights", 0);
        SC_EXPECT(!OutsideLampsOf(s.Def().vehicle_systems, dark).Any());
        dark.panel.Set("veh.outside_lights", 1);
        dark.panel.Set("veh.well_switch", 0);  // the battery switch off: no power, no lamps
        SC_EXPECT(!OutsideLampsOf(s.Def().vehicle_systems, dark).Any());
    }
    SC_EXPECT_NEAR(v->SpeedCapMps(s.Console(), w), 60.0 / 3.6, 1e-9);
    // Afloat: both flaps open and the shield up, or no way at all.
    w.dark = false;
    w.afloat = true;
    p.Set("veh.waterjet_left", 0);
    p.Set("veh.waterjet_right", 1);
    p.Set("veh.bilge_pump", 1);
    SC_EXPECT_NEAR(v->SpeedCapMps(s.Console(), w), 0.0, 1e-12);
    p.Set("veh.waterjet_left", 1);
    SC_EXPECT_NEAR(v->SpeedCapMps(s.Console(), w), 8.0 / 3.6, 1e-9);
    p.Set("veh.bilge_pump", 0);
    SC_EXPECT_NEAR(v->SpeedCapMps(s.Console(), w), 0.0, 1e-12);
}

SC_TEST("vehicle systems (C7): the batteries run down with nothing charging; НАПР. sets the voltmeter; the УКЧ sheds a slow ГОМ") {
    // The batteries alone: 15 A for an hour from 70 Ah.
    {
        auto r = Load(Osa(""));
        SC_EXPECT(r != nullptr);
        if (!r) return;
        r->Console().panel.Set("veh.well_switch", 1);
        Run(*r, 3600.0, 2.0);
        const VehicleSystems* v = r->OwnSystems();
        std::printf("    an hour on the batteries: charge %.3f, %.2f V, %.1f A\n", v->BatteryCharge(), v->NetworkV(), v->AmmeterA());
        SC_EXPECT_NEAR(v->BatteryCharge(), 1.0 - 15.0 / 70.0, 0.01);
        SC_EXPECT(v->NetworkV() > 23.0 && v->NetworkV() < 26.0 && v->AmmeterA() < 0.0 && v->RadioSupplyInNorm());
    }
    auto r = Load(Osa("deployed"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ScenarioRunner& s = *r;
    Run(s, 2.0);
    const VehicleSystems* v = s.OwnSystems();
    SC_EXPECT_NEAR(v->SupplyV(), 220.0, 1e-6);
    SC_EXPECT_NEAR(v->NetworkV(), 27.5, 1e-6);  // charged by ВУ-1 while a generator runs
    // The cabinet's voltmeter reads the live supply through the dash.
    CabinLayout cab;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", s.Console(), cab, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    const CabinDisplay* volt = nullptr;
    for (const CabinDisplay& d : cab.displays) {
        if (d.id == "pwr_voltmeter") volt = &d;
    }
    SC_EXPECT(volt != nullptr);
    if (volt == nullptr) return;
    s.Console().panel.SetValue("gta.volt_gta", 0.75);
    Run(s, 1.0);
    const VehicleDash dash = s.OwnDash();
    SC_EXPECT(dash.valid);
    SC_EXPECT_NEAR(MeterValue(*volt, s.Console(), dash.supply_v), 226.0, 1e-6);
    SC_EXPECT_NEAR(MeterValue(*volt, s.Console()), 220.0, 1e-6);  // without the vehicle's model, the nominal
    // The turbine off: the take-off generator alone; at idle it turns too slow for 400 Hz and the УКЧ sheds the load.
    s.Console().panel.Set("pwr.gta", 0);
    s.Console().panel.SetValue("veh.hand_throttle", 0.0);
    Run(s, 20.0);
    std::printf("    the ГОМ at %.0f rpm: %.0f Hz, shed %d, power %d\n", v->Rpm(), v->SupplyHz(), v->Shedding() ? 1 : 0,
                s.Console().equipment.IsReady("power") ? 1 : 0);
    SC_EXPECT(v->SupplyHz() < 360.0 && v->Shedding() && !s.Console().equipment.IsReady("power"));
    s.Console().panel.SetValue("veh.hand_throttle", 1.0);
    Run(s, 20.0);
    SC_EXPECT(v->SupplyHz() > 395.0 && !v->Shedding());
    // With the УКЧ out nothing sheds, however slow.
    s.Console().panel.Set("pwr.ukch", 0);
    s.Console().panel.SetValue("veh.hand_throttle", 0.0);
    Run(s, 20.0);
    SC_EXPECT(v->SupplyHz() < 360.0 && !v->Shedding());
}

SC_TEST("vehicle systems (C3): the season's ventilation, the gyro's heater, racks and transmitters overheating, the frost drill, the crew's comfort") {
    // Deep winter with the equipment running: ЗИМА, the cabin warmed by the racks, the gyro's heater on; the warm-up slower.
    {
        auto r = Load(Osa("deployed"), -25.0, -30.0);
        SC_EXPECT(r != nullptr);
        if (!r) return;
        const VehicleSystems* v = r->OwnSystems();
        Run(*r, 1.0);
        SC_EXPECT(r->Console().panel.Get("vent.season") == 0 && r->Console().panel.Get("vent.cover_left") == 0);
        std::printf("    -25 C outside: cabin %.1f C, racks %.1f C, gyro heater %d, work %.2f\n", v->CabinC(), v->RackC(),
                    r->Console().panel.Get("gyro.heat"), v->WorkSlowFactor(r->Console()));
        SC_EXPECT(v->CabinC() < 5.0 && r->Console().panel.Get("gyro.heat") == 1);
        SC_EXPECT(v->WorkSlowFactor(r->Console()) > 1.1);
        // The gyro will not erect cold without its heater.
        r->Console().panel.Set("gyro.heat", 0);
        Run(*r, 1.0);
        SC_EXPECT(r->Console().equipment.State("gyro_warm") == SubsystemState::Off);
    }
    // A hot day with the intakes shut: the racks overheat and a block fails; with them open, not.
    for (int open = 0; open < 2; ++open) {
        auto r = Load(Osa("deployed"), 35.0, 10.0);
        SC_EXPECT(r != nullptr);
        if (!r) return;
        r->Console().panel.Set("vent.hatch_left", open);
        r->Console().panel.Set("vent.hatch_right", open);
        Run(*r, 3600.0, 2.0);
        const VehicleSystems* v = r->OwnSystems();
        std::printf("    +35 C, intakes %s: racks %.1f C, СРП fault %d\n", open ? "open" : "shut", v->RackC(),
                    r->Console().equipment.GetInput("srp_fault") ? 1 : 0);
        SC_EXPECT(r->Console().equipment.GetInput("srp_fault") == (open == 0));
    }
    // A very hot day radiating: ПЕРЕГРЕВ, then the transmitter fails; the magnetron drifts with its heat.
    {
        auto r = Load(Osa("deployed"), 55.0, 10.0);
        SC_EXPECT(r != nullptr);
        if (!r) return;
        r->Console().panel.Set("soc.training", 1);
        r->Console().panel.Set("soc.hv_cover", 1);  // ВЫСОКОЕ is under its cover
        r->Console().panel.Set("soc.hv", 1);
        Run(*r, 600.0, 2.0);
        SC_EXPECT(r->Console().equipment.IsReady("soc_transmitter"));
        const VehicleSystems* v = r->OwnSystems();
        const double drift0 = v->MagnetronDriftHz();
        // Radiating on, it heats past ПЕРЕГРЕВ and fails; failed, it stops radiating and cools (the lamp goes out).
        bool overheat_seen = false;
        double hottest_c = 0.0, drift_max = drift0;
        for (int k = 0; k < 100 && !r->Console().equipment.GetInput("transmitter_failed"); ++k) {
            Run(*r, 60.0, 2.0);
            overheat_seen = overheat_seen || (v->Overheat() && r->Console().equipment.GetInput("transmitter_overheat"));
            hottest_c = std::max(hottest_c, v->TransmitterC());
            drift_max = std::max(drift_max, v->MagnetronDriftHz());
        }
        std::printf("    +55 C radiating: transmitter up to %.1f C, ПЕРЕГРЕВ %d, failed %d, drift %.0f -> %.0f kHz\n", hottest_c,
                    overheat_seen ? 1 : 0, r->Console().equipment.GetInput("transmitter_failed") ? 1 : 0, drift0 / 1000.0,
                    drift_max / 1000.0);
        SC_EXPECT(overheat_seen);
        SC_EXPECT(r->Console().equipment.GetInput("transmitter_failed") && !r->Console().equipment.IsReady("soc_transmitter"));
        SC_EXPECT(drift_max > drift0);
    }
    // Damp air and a frost: switched on, the equipment stays on its filaments 15 min before it warms up.
    {
        auto r = Load(Osa("powered"), -5.0, -5.5);
        SC_EXPECT(r != nullptr);
        if (!r) return;
        r->Console().panel.Set("pwr.equipment", 0);  // "powered" starts with it on: off, then switched on
        Run(*r, 1.0);
        r->Console().panel.Set("pwr.equipment", 1);
        Run(*r, 600.0);
        SC_EXPECT(r->OwnSystems()->FrostDamp() && r->Console().equipment.State("equipment_warm") == SubsystemState::Off);
        Run(*r, 1200.0);
        SC_EXPECT(!r->OwnSystems()->FrostDamp() && r->Console().equipment.IsReady("equipment_warm"));
    }
    // The same switched on dry: at once.
    {
        auto r = Load(Osa("powered"), -5.0, -20.0);
        SC_EXPECT(r != nullptr);
        if (!r) return;
        r->Console().panel.Set("pwr.equipment", 0);
        Run(*r, 1.0);
        r->Console().panel.Set("pwr.equipment", 1);
        Run(*r, 2.0);
        SC_EXPECT(!r->OwnSystems()->FrostDamp() && r->Console().equipment.State("equipment_warm") != SubsystemState::Off);
    }
}

SC_TEST("vehicle systems (C8): a fire lights its lamp and stops the engine; the automat and the driver fire bottles; left burning, she burns out") {
    auto r = Load(Osa("deployed", R"({ "name": "Commander", "role": "commander" }, { "name": "Driver", "role": "driver" })"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ScenarioRunner& s = *r;
    Run(s, 1.0);
    VehicleSystems* v = s.MutableOwnSystems();
    SC_EXPECT(v != nullptr && v->BottlesLeft() == 3);
    if (v == nullptr) return;
    SC_EXPECT(v->Ignite("engine", s.Time()) && !v->Ignite("engine", s.Time()) && !v->Ignite("galley", s.Time()));
    Run(s, 1.0);
    SC_EXPECT(s.Console().equipment.GetInput("fire_engine") && !s.Console().equipment.GetInput("fire_gta"));
    SC_EXPECT(s.Console().equipment.State("engine") == SubsystemState::Off);  // the «Роса» stops it
    Run(s, 60.0);
    std::printf("    after a minute: burning %d, bottles left %d, the disc on a full one %d\n", v->AnyBurning() ? 1 : 0, v->BottlesLeft(),
                v->DiscOnFull(s.Console()) ? 1 : 0);
    SC_EXPECT(v->BottlesLeft() < 3);
    SC_EXPECT(!v->AnyBurning() || v->BottlesLeft() == 0);
    if (v->BottlesLeft() > 0) SC_EXPECT(v->DiscOnFull(s.Console()));  // the driver turned the disc on to a full bottle
    SC_EXPECT(!v->Destroyed());
    // A model on its own with no agent that works and nobody at the box: it burns the vehicle out in two minutes.
    VehicleSystemsDef def;
    std::string error;
    SC_EXPECT(LoadVehicleSystemsFile(kData + "/vehicles/osa_ak_systems.json", def, &error));
    def.fire.put_out_chance = 0.0;
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error));
    VehicleSystems alone;
    alone.Bind(def, c, 5u);
    alone.Start(c, VehicleSystems::World(), true, false);
    SC_EXPECT(alone.Ignite("gta", 0.0));
    double t = 0.0;
    for (; t < 200.0 && !alone.Destroyed(); t += 1.0) alone.Step(1.0, c, VehicleSystems::World(), t + 1.0);
    SC_EXPECT(alone.Destroyed() && c.equipment.GetInput("vehicle_burnt") && c.equipment.GetInput("fire_gta"));
    SC_EXPECT_NEAR(t, def.fire.burn_to_destroyed_s, 1.5);
    SC_EXPECT(alone.BottlesLeft() == 2);  // the automat fired one, to no effect
}

SC_TEST("vehicle systems: the dash reaches a network client through the scope feed and reads on its dials") {
    auto r = Load(Osa("deployed"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Run(*r, 5.0);
    const ScopeFeed f = ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time(), r->Fuel(), r->OwnDash());
    SC_EXPECT(f.dash.valid && f.dash.engine_rpm > 600.0);
    EchoKeys keys;
    const em::ViewScope view = ToViewScope(f, keys);
    em::ViewScope back;
    const std::vector<std::uint8_t> bytes = em::wire::ScopeBytes(view);
    em::wire::Reader rd(bytes);
    SC_EXPECT(em::wire::GetScope(rd, back));
    const ScopeFeed g = FromViewScope(back);
    SC_EXPECT(g.dash.valid);
    SC_EXPECT_NEAR(g.dash.engine_rpm, f.dash.engine_rpm, 0.01);
    SC_EXPECT_NEAR(g.dash.coolant_c, f.dash.coolant_c, 0.01);
    SC_EXPECT_NEAR(g.dash.supply_v, 220.0, 0.01);
    CabinLayout cab;
    std::string error;
    SC_EXPECT(LoadCabinFile(kData + "/cabins/osa_ak.json", r->Console(), cab, &error));
    int dials = 0;
    for (const CabinDisplay& d : cab.displays) {
        if (d.id == "tachometer") {
            SC_EXPECT_NEAR(DialReading(d, r->Console(), 0.0, -1.0, g.dash), g.dash.engine_rpm / 100.0, 1e-9);
            ++dials;
        }
        if (d.id == "coolant_gauge") {
            SC_EXPECT_NEAR(DialReading(d, r->Console(), 0.0, -1.0, g.dash), 90.0, 1.0);
            SC_EXPECT_NEAR(DialReading(d, r->Console(), 0.0, -1.0), 0.0, 1e-12);  // no model: at rest
            ++dials;
        }
    }
    SC_EXPECT(dials == 2);
}

SC_TEST("vehicle systems (C2): a blacked-out vehicle is seen at night only from 150 m, and found less by armed reconnaissance") {
    nav::VisualRangeValues vr;
    std::string error;
    SC_EXPECT(nav::LoadVisualRange(kData, "gameplay/visual_range.json", vr, &error));
    SC_EXPECT_NEAR(vr.night_blackout_m, 150.0, 1e-9);
    SC_EXPECT_NEAR(nav::VisualRangeUnlitM(20000.0, -20.0, vr), 150.0, 1e-9);
    SC_EXPECT_NEAR(nav::VisualRangeUnlitM(20000.0, 20.0, vr), nav::VisualRangeM(20000.0, 20.0, vr), 1e-9);  // by day as any
    nav::Route rt;
    rt.found = true;
    rt.path = {{0.0, 0.0}, {2000.0, 0.0}};
    rt.stretches = {{"road", nav::Road::Main, "a", 2000.0, 200.0}};
    rt.time_s = 200.0;
    rt.length_m = 2000.0;
    auto two_columns = [&](nav::Movers& movers) {
        for (int lit = 0; lit < 2; ++lit) {
            nav::Mover m;
            m.id = lit ? "lit" : "dark";
            m.kind = "column";
            m.mobility = "osa_9a33";
            m.route = rt;
            m.lit = lit != 0;
            movers.Add(m);
        }
    };
    nav::Movers movers;
    two_columns(movers);
    movers.UpdateLevels({{1000.0, 400.0}}, 1500.0, {}, 150.0);
    SC_EXPECT(movers.Find("lit")->level == nav::MoverLevel::M2);
    SC_EXPECT(movers.Find("dark")->level != nav::MoverLevel::M2);
    // Armed reconnaissance by night, on two columns far from the player (abstract movers): the dark one a quarter as often.
    airwar::TuningTable tune;
    SC_EXPECT(airwar::LoadTuningFile(kData + "/gameplay/air_war.json", tune, &error));
    nav::Movers far_columns;
    two_columns(far_columns);
    for (double t = 1.0; t <= 20.0; t += 1.0) far_columns.Step(t, 1.0);
    int lit_seen = 0, dark_seen = 0;
    airwar::Rng rng(11);
    for (int k = 0; k < 2000; ++k) {
        for (const std::string& id : nav::ArmedRecceSees(far_columns, {200.0, 0.0}, 5000.0, airwar::DayPart::Night, airwar::Weather::Clear, false,
                                                         tune, rng, 0.25)) {
            (id == "lit" ? lit_seen : dark_seen) += 1;
        }
    }
    std::printf("    by night in 2000 searches: lit %d, blacked out %d\n", lit_seen, dark_seen);
    SC_EXPECT(lit_seen > 0 && dark_seen > 0 && dark_seen < lit_seen / 2);
}

SC_TEST("C9: ОТКАЗ with the selector on the prepared missile's container drops it, and the next is prepared") {
    auto r = Load(Osa("deployed"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ScenarioRunner& s = *r;
    const Launcher* l = s.GetLauncher();
    SC_EXPECT(l != nullptr && s.Console().launcher.has_refuse);
    if (l == nullptr) return;
    // Preparation started by hand (БОРТ СПК), as with no target.
    const std::string& prep = s.Console().launcher.pair_preparation;
    Run(s, 1.0);
    // БОРТ СПК on: the preparation runs and a missile is picked for each channel (the attention input is the runner's).
    s.Console().panel.Set("fo.bort_spk_cover", 1);
    s.Console().panel.Set("fo.bort_spk", 1);
    Run(s, 2.0);
    const std::string first = l->Selected(1);
    std::printf("    channel I prepared %s (preparation %s)\n", first.c_str(), prep.c_str());
    SC_EXPECT(!first.empty());
    if (first.empty()) return;
    int pos = -1;
    for (std::size_t i = 0; i < s.Console().launcher.refuse.select_containers.size(); ++i) {
        if (s.Console().launcher.refuse.select_containers[i] == first) pos = static_cast<int>(i);
    }
    SC_EXPECT(pos >= 0);
    s.Console().panel.Set("ind.select", pos);
    Press(s, "ind.refuse");
    Run(s, 1.0);
    std::printf("    after ОТКАЗ: channel I %s, refusals %d\n", l->Selected(1).c_str(), l->Refusals());
    SC_EXPECT(l->Refusals() == 1 && l->Selected(1) != first);
    SC_EXPECT(s.Console().equipment.GetInput("missile_refused"));
    // On ОТКЛ. nothing is refused.
    s.Console().panel.Set("ind.select", 3);
    Press(s, "ind.refuse");
    SC_EXPECT(l->Refusals() == 1);
}

SC_TEST("C9: МЦ is the small-target mode: the console names it, the tuning reads its factors") {
    ConsoleData c;
    std::string error;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error));
    SC_EXPECT(c.track.small_target_control == "fo.mc" && c.track.small_target_on_position == 1);
    SC_EXPECT(c.panel.Find("fo.mc") != nullptr && c.panel.Find("fo.mc")->label == "МЦ — ОТКЛ.");
    EngagementTuning t;
    SC_EXPECT(LoadEngagementTuningFile(kData + "/gameplay/osa_ak.json", t, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT_NEAR(t.small_target_below_mps, 100.0, 1e-9);
    SC_EXPECT(t.small_target_off_factor < 1.0 && t.small_target_fast_on_factor < 1.0);
}
