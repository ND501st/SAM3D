// The battery around the player's vehicle (roadmap 7.5): march spacing, the air-threat spacing, the short halt, the
// combat formation in pairs, scheduled halts, and the scenario hooks (orders, triggers, shared pictures).
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/battery.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

BatteryTactics Tactics() {
    BatteryTactics t;
    std::string error;
    if (!LoadBatteryTacticsFile(kData + "/command/battery_tactics.json", t, &error)) std::printf("    %s\n", error.c_str());
    return t;
}

std::vector<BatteryVehicleDef> Vehicles() {
    std::vector<BatteryVehicleDef> v;
    auto add = [&v](const char* id, BatteryVehicleKind k, double e, double n) {
        BatteryVehicleDef d;
        d.id = id;
        d.callsign = id;
        d.kind = k;
        d.start = Vec3{e, n, 0.0};
        v.push_back(d);
    };
    add("21", BatteryVehicleKind::Launcher, 0.0, 0.0);
    add("22", BatteryVehicleKind::Launcher, 30.0, -60.0);
    add("23", BatteryVehicleKind::Launcher, -20.0, -120.0);
    add("24", BatteryVehicleKind::Launcher, 10.0, -180.0);
    add("TZM", BatteryVehicleKind::Reload, 0.0, -240.0);
    add("PU-12", BatteryVehicleKind::CommandPost, 0.0, -300.0);
    return v;
}

BatteryOrder MarchNorth(double km = 40.0) {
    BatteryOrder o;
    o.kind = BatteryOrder::Kind::March;
    o.route = {Vec3{0.0, 0.0, 0.0}, Vec3{0.0, km * 1000.0, 0.0}};
    return o;
}

void Run(Battery& b, double& t, double seconds, double dt = 0.5) {
    for (double end = t + seconds; t < end - 1e-9; t += dt) b.Step(t + dt, dt);
}

double Gap(const Battery& b, std::size_t i) {
    const Vec3 d = b.Vehicles()[i].position - b.Vehicles()[i + 1].position;
    return std::hypot(d.x, d.y);
}

}  // namespace

SC_TEST("battery: the tactics load with every value cited") {
    const BatteryTactics t = Tactics();
    SC_EXPECT_NEAR(t.march_spacing_m, 40.0, 1e-9);
    SC_EXPECT_NEAR(t.pair_separation_m, 3000.0, 1e-9);
    SC_EXPECT_NEAR(t.halt_interval_s, 3.5 * 3600.0, 1e-9);
}

SC_TEST("battery: the column forms on the road, keeps 40 m, and opens to 400 m when an attack is expected") {
    Battery b(Tactics(), Vehicles());
    b.SetDriveOwn(true);
    double t = 0.0;
    b.Order(MarchNorth(), t);
    Run(b, t, 900.0);
    SC_EXPECT(b.CurrentMode() == Battery::Mode::Marching);
    for (std::size_t i = 0; i + 1 < b.Vehicles().size(); ++i) SC_EXPECT(std::fabs(Gap(b, i) - 40.0) < 12.0);
    const double head = b.Vehicles()[0].position.y;
    std::printf("    head of the column %.1f km up the road after 15 min; gaps %.0f..%.0f m\n", head / 1000.0, Gap(b, 0), Gap(b, 4));
    SC_EXPECT(head > 4000.0);
    b.SetAirThreat(true, t);
    Run(b, t, 600.0);
    for (std::size_t i = 0; i + 1 < b.Vehicles().size(); ++i) SC_EXPECT(std::fabs(Gap(b, i) - 400.0) < 60.0);
    std::printf("    with an air threat: gaps %.0f..%.0f m\n", Gap(b, 0), Gap(b, 4));
}

SC_TEST("battery: on a short halt each vehicle pulls off the road, alternating sides, and deploys in 4 min") {
    Battery b(Tactics(), Vehicles());
    b.SetDriveOwn(true);
    double t = 0.0;
    b.Order(MarchNorth(), t);
    Run(b, t, 600.0);
    BatteryOrder halt;
    halt.kind = BatteryOrder::Kind::ShortHalt;
    b.Order(halt, t);
    Run(b, t, 60.0);
    for (std::size_t i = 0; i < b.Vehicles().size(); ++i) {
        const double x = b.Vehicles()[i].position.x;  // the road runs along x = 0
        SC_EXPECT(std::fabs(std::fabs(x) - 40.0) < 2.0);
        if (i > 0) SC_EXPECT(x * b.Vehicles()[i - 1].position.x < 0.0);  // the other side from the one ahead
        SC_EXPECT(!b.Vehicles()[i].deployed);
    }
    Run(b, t, 240.0);
    for (const Battery::Vehicle& v : b.Vehicles()) SC_EXPECT(v.deployed);
}

SC_TEST("battery: the combat formation puts launchers in pairs 3 km apart, 200 m within, 2 km behind; the TZM 500 m back") {
    Battery b(Tactics(), Vehicles());
    b.SetDriveOwn(true);
    BatteryOrder f;
    f.kind = BatteryOrder::Kind::CombatFormation;
    f.covered = Vec3{0.0, 5000.0, 0.0};
    f.axis_deg = 0.0;  // advancing north
    double t = 0.0;
    b.Order(f, t);
    Run(b, t, 2400.0);
    const auto& v = b.Vehicles();
    for (const Battery::Vehicle& x : v) SC_EXPECT(x.deployed);
    // pair 1: 21 and 22; pair 2: 23 and 24
    SC_EXPECT_NEAR(std::fabs(v[0].position.x - v[1].position.x), 200.0, 1.0);
    SC_EXPECT_NEAR(std::fabs(v[2].position.x - v[3].position.x), 200.0, 1.0);
    const double c1 = 0.5 * (v[0].position.x + v[1].position.x), c2 = 0.5 * (v[2].position.x + v[3].position.x);
    SC_EXPECT_NEAR(std::fabs(c2 - c1), 3000.0, 1.0);
    for (int i = 0; i < 4; ++i) SC_EXPECT_NEAR(v[static_cast<std::size_t>(i)].position.y, 3000.0, 1.0);  // 2 km behind
    SC_EXPECT_NEAR(v[4].position.y, 2500.0, 1.0);   // TZM 500 m behind its pair
    SC_EXPECT(v[5].position.y < 2500.0);             // the PU-12 further back
}

SC_TEST("battery: a halt every 3.5 h on the march, 1.5 h long, 1 h at night") {
    for (int night = 0; night < 2; ++night) {
        BatteryTactics tac = Tactics();
        Battery b(tac, Vehicles());
        b.SetDriveOwn(true);
        b.SetNight(night == 1);
        double t = 0.0;
        b.Order(MarchNorth(400.0), t);
        Run(b, t, 3.5 * 3600.0 + 600.0, 2.0);
        SC_EXPECT(b.CurrentMode() == Battery::Mode::Halted);
        const double y0 = b.Vehicles()[0].position.y;
        Run(b, t, (night == 1 ? 3600.0 : 5400.0) - 900.0, 2.0);
        SC_EXPECT(b.CurrentMode() == Battery::Mode::Halted && std::fabs(b.Vehicles()[0].position.y - y0) < 1.0);
        Run(b, t, 1200.0, 2.0);
        SC_EXPECT(b.CurrentMode() != Battery::Mode::Halted);
    }
}

namespace {

std::string BatteryScenario(const std::string& extra_trigger) {
    return R"({
      "scenario_id": "bat", "name": "Battery", "source": "sam3d", "confidence": "estimate", "seed": 2,
      "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 }, "terrain": { "type": "flat", "height_m": 110 },
      "command_post": "command/pu12.json",
      "battery": { "drive_own": true, "share_picture": true, "own": { "id": "21", "callsign": "21" },
        "vehicles": [ { "id": "22", "kind": "launcher", "position": { "east_m": 0, "north_m": -60, "up_m": 3 } },
                      { "id": "23", "kind": "launcher", "position": { "east_m": 0, "north_m": -120, "up_m": 3 } },
                      { "id": "TZM", "kind": "reload", "position": { "east_m": 0, "north_m": -180, "up_m": 3 } } ],
        "orders": [ { "at_s": 0, "order": "march", "route": [ { "east_m": 0, "north_m": 0, "up_m": 3 }, { "east_m": 0, "north_m": 30000, "up_m": 3 } ] } ] },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": false } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Driver", "role": "driver" } ],
      "targets": [ { "id": "t1", "name": "T", "class": "drone", "start": { "east_m": 30000, "north_m": 60000, "up_m": 2000 },
                     "waypoints": [ { "east_m": 30000, "north_m": -30000, "up_m": 2000 } ], "speed_mps": 150 } ],
      "triggers": [ )" + extra_trigger + R"( ],
      "end": { "time_limit_s": 900, "when_targets_gone": false } })";
}

}  // namespace

SC_TEST("battery: a scenario's battery marches the player's vehicle with it, shares pictures, and takes trigger orders") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(BatteryScenario(R"({ "id": "halt", "when": { "after_s": 300 }, "do": [ { "battery_order": "short_halt" } ] })"),
                           kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(def.has_battery && def.battery_vehicles.size() == 4 && def.battery_vehicles[0].own);
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.GetBattery() != nullptr && r.C2() != nullptr);
    SC_EXPECT(r.C2()->Has("22") && r.C2()->Has("23") && !r.C2()->Has("TZM"));  // the launchers' radars feed the crew
    for (int i = 0; i < 2900; ++i) r.Step(0.1);  // 290 s
    const double north = r.UnitPosition().y;
    std::printf("    the player's vehicle is %.1f km up the road at 290 s\n", north / 1000.0);
    SC_EXPECT(north > 1000.0);
    // the neighbour sources moved with their vehicles
    const PlottingBoard* nb = r.C2()->BoardOf("22");
    SC_EXPECT(nb != nullptr);
    for (int i = 0; i < 3000; ++i) r.Step(0.1);  // past the short halt at 300 s
    SC_EXPECT(r.GetBattery()->CurrentMode() == Battery::Mode::ShortHalt);
    SC_EXPECT(r.Debrief().find("short halt") != std::string::npos);
    SC_EXPECT(r.Debrief().find("column formed") != std::string::npos);
    SC_EXPECT(r.Acmi().Text().find("Name=22") != std::string::npos);
}

SC_TEST("battery: the scenario's battery block refuses mistakes with a path") {
    auto bad = [](const std::string& text, const std::string& part) {
        ScenarioDef d;
        ConsoleData c;
        std::string e;
        const bool ok = LoadScenario(text, kData, d, c, &e);
        if (ok || e.find(part) == std::string::npos) std::printf("    expected '%s', got '%s'\n", part.c_str(), e.c_str());
        return !ok && e.find(part) != std::string::npos;
    };
    std::string t = BatteryScenario("");
    std::string k = t;
    k.replace(k.find("\"kind\": \"reload\""), 16, "\"kind\": \"tank\"");
    SC_EXPECT(bad(k, "launcher, reload"));
    std::string o = t;
    o.replace(o.find("\"order\": \"march\""), 16, "\"order\": \"dance\"");
    SC_EXPECT(bad(o, "unknown battery order"));
    SC_EXPECT(bad(BatteryScenario(R"({ "id": "x", "when": {}, "do": [ { "battery_order": "combat_formation" } ] })"), "covered"));
}

SC_TEST("prepared position: emplacement and nets shrink the vehicle's echo; dummies are echoes and decoys; leaving takes time") {
    const std::string text = R"({
      "scenario_id": "prep", "name": "Prepared", "source": "sam3d", "confidence": "estimate", "seed": 2,
      "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 }, "terrain": { "type": "flat", "height_m": 110 },
      "battery": { "drive_own": true, "vehicles": [ { "id": "22", "kind": "launcher", "position": { "east_m": 0, "north_m": -60, "up_m": 3 } } ],
        "orders": [ { "at_s": 10, "order": "march", "route": [ { "east_m": 0, "north_m": 0, "up_m": 3 }, { "east_m": 0, "north_m": 30000, "up_m": 3 } ] } ] },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "em": { "library": "em/osa_ak.json", "search_radar": "osa_ak_soc", "track_radar": "osa_ak_ssc" },
                    "prepared_position": { "emplacement": true, "nets": true, "wire": true,
                      "dummies": [ { "position": { "east_m": 800, "north_m": 300, "up_m": 0 }, "decoy": "when_radar_off" },
                                   { "position": { "east_m": -900, "north_m": 200, "up_m": 0 } } ] },
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" } ],
      "targets": [], "triggers": [], "end": { "time_limit_s": 900, "when_targets_gone": false } })";
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(text, kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(def.prepared.present && def.prepared.dummies.size() == 2 && def.prepared.dummies[0].decoy && !def.prepared.dummies[1].decoy);
    // 20 m2 x 0.2 (emplacement) x 0.5 (nets) = 2 m2
    SC_EXPECT_NEAR(def.prepared.VehicleRcsM2(), 2.0, 1e-9);
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.Em() != nullptr);
    if (r.Em() == nullptr) return;
    const em::EmScene& s = r.Em()->World().Scene();
    SC_EXPECT_NEAR(s.FindPlatform(r.Em()->UnitPlatformId())->rcs.beam_m2, 2.0, 1e-9);
    SC_EXPECT(r.Em()->DummyPlatforms().size() == 2);
    SC_EXPECT_NEAR(s.FindPlatform(r.Em()->DummyPlatforms()[0])->rcs.beam_m2, 20.0, 1e-9);
    SC_EXPECT(r.Em()->DecoyEmitters().size() == 1);
    // the march is ordered at 10 s; climbing out takes 60 s: nobody has moved by 60 s
    for (int i = 0; i < 600; ++i) r.Step(0.1);
    SC_EXPECT(r.UnitPosition().y < 1.0);
    for (int i = 0; i < 1200; ++i) r.Step(0.1);  // 180 s
    SC_EXPECT(r.UnitPosition().y > 100.0);
    SC_EXPECT(r.Debrief().find("climb out") != std::string::npos);
    SC_EXPECT(r.Acmi().Text().find("Dummy position") != std::string::npos);
    // The works (roadmap 7.6): an emplacement at each starting place (ours, the launcher's, the two dummies'); the nets
    // came down where a vehicle drove off, the dummies keep theirs; and they reach a player through the stream.
    const std::vector<em::ViewWork> works = r.Works();
    SC_EXPECT(works.size() == 4);
    if (works.size() == 4) {
        for (const em::ViewWork& k : works) SC_EXPECT(k.emplacement);
        SC_EXPECT(!works[0].nets && std::fabs(works[0].position.y) < 1.0);  // ours, left behind
        SC_EXPECT(works[2].nets && works[3].nets);                        // the dummy positions
    }
    em::KnowledgeView v;
    v.extras = r.ViewExtrasNow();
    em::KnowledgeView back;
    SC_EXPECT(em::DecodeView(em::EncodeView(v), back));
    SC_EXPECT(back.extras.works.size() == works.size());
    if (back.extras.works.size() == 4) {
        SC_EXPECT(back.extras.works[2].emplacement && back.extras.works[2].nets && !back.extras.works[0].nets);
        SC_EXPECT(std::fabs(back.extras.works[2].position.x - 800.0) < 1.0);
    }
}

SC_TEST("battery view: the board's marks, the radio traffic and the battery's vehicles reach a player through the stream") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(BatteryScenario(""), kData, def, console, &error));
    ScenarioRunner r(std::move(def), std::move(console));
    for (int i = 0; i < 1300; ++i) r.Step(0.1);  // 130 s: three plots, the column formed and marching
    const em::ViewExtras x = r.ViewExtrasNow();
    std::printf("    %zu board marks, %zu radio messages, %zu battery vehicles\n", x.board.size(), x.radio.size(), x.battery.size());
    SC_EXPECT(!x.board.empty());
    SC_EXPECT(!x.radio.empty());
    SC_EXPECT(x.battery.size() == 3);
    bool column = false;
    for (const em::ViewRadio& m : x.radio) column = column || m.text.find("column formed") != std::string::npos;
    SC_EXPECT(column);
    em::KnowledgeView v;
    v.extras = x;
    em::KnowledgeView back;
    SC_EXPECT(em::DecodeView(em::EncodeView(v), back));
    SC_EXPECT(back.extras.valid && back.extras.board.size() == x.board.size() && back.extras.radio.size() == x.radio.size());
    SC_EXPECT(back.extras.battery[0].kind == "launcher" && std::fabs(back.extras.battery[0].position.y - x.battery[0].position.y) < 1.0);
    SC_EXPECT(back.extras.board[0].number == x.board[0].number && back.extras.board[0].code == x.board[0].code);
}

SC_TEST("battery practice: the shipped scenario loads and runs with its battery, board, radio and prepared position") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_battery_practice.json", kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner r(std::move(def), std::move(console));
    for (int i = 0; i < 20 * 330; ++i) r.Step(0.05);
    const em::ViewExtras x = r.ViewExtrasNow(16);
    std::printf("    at %.0f s: %zu board marks, %zu radio messages, %zu vehicles and dummies\n", r.Time(), x.board.size(), x.radio.size(), x.battery.size());
    SC_EXPECT(x.battery.size() == 8);
    SC_EXPECT(!x.board.empty() && !x.radio.empty());
    SC_EXPECT(r.Em() != nullptr && r.Em()->DummyPlatforms().size() == 2);
}

SC_TEST("battery drills: an ambush puts pairs on the prepared points and, roving, moves them on") {
    BatteryTactics tac = Tactics();
    tac.ambush_dwell_s = 3000.0;  // long enough to get there first at 10 km/h
    Battery b(tac, Vehicles());
    b.SetDriveOwn(true);
    BatteryOrder a;
    a.kind = BatteryOrder::Kind::Ambush;
    a.route = {Vec3{0.0, 2000.0, 0.0}, Vec3{3000.0, 2000.0, 0.0}};
    a.roving = true;
    double t = 0.0;
    b.Order(a, t);
    Run(b, t, 2000.0, 1.0);
    const auto& v = b.Vehicles();
    SC_EXPECT(std::hypot(0.5 * (v[0].position.x + v[1].position.x) - 0.0, v[0].position.y - 2000.0) < 20.0);
    SC_EXPECT(std::hypot(0.5 * (v[2].position.x + v[3].position.x) - 3000.0, v[2].position.y - 2000.0) < 20.0);
    SC_EXPECT_NEAR(std::fabs(v[0].position.x - v[1].position.x), 200.0, 1.0);
    Run(b, t, 1000.0 + 1500.0, 1.0);  // roved at 3000 s: the pairs swap points
    SC_EXPECT(std::fabs(0.5 * (v[0].position.x + v[1].position.x) - 3000.0) < 20.0);
}

SC_TEST("battery drills: bounding forward, one pair moves while the other stays deployed") {
    Battery b(Tactics(), Vehicles());
    b.SetDriveOwn(true);
    BatteryOrder f;
    f.kind = BatteryOrder::Kind::CombatFormation;
    f.covered = Vec3{0.0, 5000.0, 0.0};
    f.bounding = true;
    double t = 0.0;
    b.Order(f, t);
    Run(b, t, 300.0, 1.0);
    const auto& v = b.Vehicles();
    SC_EXPECT(v[0].speed_mps > 0.0 || v[1].speed_mps > 0.0);   // the first pair is moving
    SC_EXPECT(v[2].speed_mps == 0.0 && v[3].speed_mps == 0.0);  // the second waits
    Run(b, t, 3600.0, 1.0);
    for (std::size_t i = 0; i < 4; ++i) SC_EXPECT(v[i].deployed);
    bool told = false;
    for (const Battery::Event& e : b.TakeEvents()) told = told || e.text.find("bounds forward") != std::string::npos;
    SC_EXPECT(told);
}

SC_TEST("battery drills: a river crossing: positions 2 km short, then across in pairs once the far bank is held") {
    Battery b(Tactics(), Vehicles());
    b.SetDriveOwn(true);
    BatteryOrder r;
    r.kind = BatteryOrder::Kind::RiverCrossing;
    r.covered = Vec3{0.0, 3000.0, 0.0};  // the crossing on the near bank; the river runs east-west, 120 m wide
    r.axis_deg = 0.0;
    r.width_m = 120.0;
    double t = 0.0;
    b.Order(r, t);
    Run(b, t, 2000.0, 1.0);
    const auto& v = b.Vehicles();
    for (std::size_t i = 0; i < 4; ++i) SC_EXPECT(std::fabs(v[i].position.y - 1000.0) < 20.0);  // 2 km short of the bank
    BatteryOrder go;
    go.kind = BatteryOrder::Kind::FarBankHeld;
    b.Order(go, t);
    bool pair2_waited = false, afloat_seen = false;
    for (int i = 0; i < 4000; ++i) {
        b.Step(t + 1.0, 1.0);
        t += 1.0;
        if (v[0].afloat) {
            afloat_seen = true;
            pair2_waited = pair2_waited || (v[2].crossing_stage == 0 && v[3].crossing_stage == 0);
            SC_EXPECT(v[0].speed_mps <= 2.2 + 1e-9);
        }
    }
    SC_EXPECT(afloat_seen && pair2_waited);
    for (const Battery::Vehicle& x : v) SC_EXPECT(x.position.y > 3120.0);  // everyone across
    bool across = false;
    for (const Battery::Event& e : b.TakeEvents()) across = across || e.text.find("the battery is across") != std::string::npos;
    SC_EXPECT(across);
}

SC_TEST("battery drills: the TZM drives up when the launcher is empty, reloads six, and goes back; the fuel truck refuels") {
    const std::string text = R"({
      "scenario_id": "svc", "name": "Service", "source": "sam3d", "confidence": "estimate", "seed": 2,
      "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 }, "terrain": { "type": "flat", "height_m": 110 },
      "battery": { "own": { "id": "21", "callsign": "21" },
        "vehicles": [ { "id": "TZM", "kind": "reload", "position": { "east_m": 300, "north_m": -400, "up_m": 3 } },
                      { "id": "ATZ", "kind": "fuel", "position": { "east_m": -300, "north_m": -500, "up_m": 3 } } ] },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed", "fuel_fraction": 0.4,
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" } ],
      "targets": [], "triggers": [], "end": { "time_limit_s": 1200, "when_targets_gone": false } })";
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(text, kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.GetLauncher() != nullptr && r.Fuel() != nullptr);
    if (r.GetLauncher() == nullptr || r.Fuel() == nullptr) return;
    for (const char* c : {"1-I", "2-I", "3-I", "1-II", "2-II", "3-II"}) r.GetLauncher()->Unload(c);
    for (int i = 0; i < 20 * 900; ++i) r.Step(0.05);  // 15 min
    SC_EXPECT(r.GetLauncher()->LoadedCount() == 6);
    SC_EXPECT(r.Fuel()->LevelL() > 0.9 * r.Fuel()->TankL());  // filled (and burning again since)
    SC_EXPECT(r.Debrief().find("tank full") != std::string::npos);
    const Battery::Vehicle* t = r.GetBattery()->Find("TZM");
    const Battery::Vehicle* f = r.GetBattery()->Find("ATZ");
    SC_EXPECT(t != nullptr && f != nullptr);
    if (t == nullptr || f == nullptr) return;
    std::printf("    TZM %.0f missiles left, fuel truck %.0f l left\n", t->stock, f->stock);
    SC_EXPECT_NEAR(t->stock, 6.0, 1e-9);
    SC_EXPECT(f->stock < 3800.0 - 100.0);
    SC_EXPECT(t->serving.empty() && f->serving.empty());  // both back in place
    const std::string d = r.Debrief();
    SC_EXPECT(d.find("to reload 21") != std::string::npos && d.find("alongside 21") != std::string::npos &&
              d.find("reload done") != std::string::npos && d.find("to refuel 21") != std::string::npos);
}

SC_TEST("battery fire: in free fire each launcher takes the targets in its sector of fire, not just the nearest") {
    std::vector<BatteryVehicleDef> v;
    auto add = [&v](const char* id, double from, double to) {
        BatteryVehicleDef d;
        d.id = d.callsign = id;
        d.kind = BatteryVehicleKind::Launcher;
        d.start = Vec3{0.0, 0.0, 0.0};
        d.has_sector = true;
        d.sector_from_deg = from;
        d.sector_to_deg = to;
        v.push_back(d);
    };
    BatteryVehicleDef own;
    own.id = own.callsign = "21";
    own.own = true;
    v.push_back(own);
    add("22", 0.0, 90.0);     // north-east
    add("23", 180.0, 270.0);  // south-west
    Battery b(Tactics(), v);
    b.SetInEmplacement(0.0);
    EngagementTuning t;
    t.zone_min_range_m = 1500.0;
    t.zone_max_range_m = 12000.0;
    t.zone_min_alt_m = 10.0;
    t.zone_max_alt_m = 5000.0;
    t.missile_average_speed_mps = 500.0;
    t.missile_max_flight_time_s = 30.0;
    t.hit_chance["default"] = 0.6;
    BatteryFire fire(t, 20000.0);
    fire.SetAutonomous(true);
    fire.Sync(b);
    // The near target is south-west, the far one north-east: without sectors 22 would take the near one.
    BatteryFireTarget ne, sw;
    ne.id = "ne";
    ne.position = Vec3{10000.0, 10000.0, 1000.0};
    ne.velocity = Vec3{-150.0, -150.0, 0.0};
    ne.agl_m = 1000.0;
    ne.alive = true;
    sw.id = "sw";
    sw.position = Vec3{-5000.0, -5000.0, 1000.0};
    sw.velocity = Vec3{150.0, 150.0, 0.0};
    sw.agl_m = 1000.0;
    sw.alive = true;
    std::mt19937 rng(3);
    fire.Step(0.0, {ne, sw}, rng, &b, [](const std::string&) {});
    std::string t22, t23;
    for (const BatteryFire::Shooter& s : fire.Shooters()) {
        if (s.callsign == "22") t22 = s.target;
        if (s.callsign == "23") t23 = s.target;
    }
    SC_EXPECT(t22 == "ne");
    SC_EXPECT(t23 == "sw");
    // Cleared, a sector no longer limits it.
    SC_EXPECT(fire.SetSector("22", 0.0, 0.0));
    SC_EXPECT(BatteryFire::InSector(fire.Shooters()[0], Vec3{-100.0, -100.0, 0.0}));
    SC_EXPECT(!fire.SetSector("99", 0.0, 90.0));
}
