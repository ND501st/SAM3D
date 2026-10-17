// The R-123M radio sets and the R-124 intercom (roadmap 7.8): the console's comms block, a set that is off or warming up,
// the squelch and antenna as losses on the link, the commander's box on the radio or off, and shouted orders.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/c2_sources.hpp"
#include "simcore/crew_comms.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// A deployed Osa with a command network: the PU-12 on preset 1 and an EWR on preset 2, the commander falling back.
std::string Text(const std::string& c2_extra = std::string(), const std::string& unit_extra = std::string()) {
    return R"({
      "scenario_id": "comms", "name": "Comms", "source": "sam3d", "confidence": "estimate", "seed": 3,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 }, "terrain": { "type": "flat", "height_m": 0 },
      "command_post": "command/pu12.json",
      "c2": { "library": "command/c2_sources.json", "primary": "pu12", "fallback_order": ["ewr_north"], "presets": ["pu12", "ewr_north"],
              "sources": [ { "id": "ewr_north", "type": "ewr_voice", "position": { "east_m": 0, "north_m": 8000, "up_m": 0 }, "link_range_m": 200000 })" +
           c2_extra + R"( ] },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed", )" + unit_extra +
           R"("position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [ { "id": "t1", "name": "T", "class": "drone", "board_type": 7,
                     "start": { "east_m": 2000, "north_m": 45000, "up_m": 1000 },
                     "waypoints": [ { "east_m": 2000, "north_m": -30000, "up_m": 1000 } ], "speed_mps": 150 } ],
      "triggers": [],
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

void Steps(ScenarioRunner& r, double seconds, double dt = 0.5) {
    for (double end = r.Time() + seconds; r.Time() < end - 1e-9;) r.Step(dt);
}

bool Has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

ConsoleData Osa() {
    ConsoleData c;
    std::string error;
    if (!LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error)) std::printf("    console: %s\n", error.c_str());
    return c;
}

}  // namespace

SC_TEST("comms: the Osa's comms block loads; bad ones are refused with paths") {
    const ConsoleData c = Osa();
    SC_EXPECT(c.has_comms && c.comms.radios.size() == 2 && c.comms.radios[1].name == "right");
    SC_EXPECT_NEAR(c.comms.range_whip_4m_km, 20.0, 1e-12);
    SC_EXPECT_NEAR(c.comms.range_whip_4m_squelch_km, 13.0, 1e-12);
    SC_EXPECT(c.comms.commander == "intercom.a1" && c.comms.commander_radio_position == 2 && c.comms.noisy.size() == 2);
    const std::string text = ReadFile(kData + "/procedures/osa_ak.json");
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
    SC_EXPECT(bad("\"whip_4m_squelch_on\": 13", "\"whip_4m_squelch_on\": 25", "comms.ranges_km.whip_4m_squelch_on"));
    SC_EXPECT(bad("\"whip_1m\": 8", "\"whip_1m\": 0", "comms.ranges_km.whip_1m"));
    SC_EXPECT(bad("\"commander_radio_position\": 2", "\"commander_radio_position\": 5", "comms.intercom.commander_radio_position"));
    SC_EXPECT(bad("\"warm\": \"r123m_left\"", "\"warm\": \"nothing\"", "comms.radios[0].warm"));
    SC_EXPECT(bad("\"shout_noisy_s\": 4.0", "\"shout_noisy_s\": 1.0", "comms.intercom.shout_noisy_s"));
    SC_EXPECT(bad("\"noisy\": [ \"gta\", \"pto_generator\" ]", "\"noisy\": [ \"gta\", \"ghost\" ]", "comms.intercom.noisy[1]"));
    SC_EXPECT(bad("\"antenna\": \"radio.left.antenna\"", "\"antenna\": \"radio.left.power\"", "comms.radios[0].antenna"));
}

SC_TEST("comms: the squelch and the antenna become losses from the published ranges (40 log10 of the ratio)") {
    ConsoleData c = Osa();
    CrewComms comms(c.comms);
    SC_EXPECT_NEAR(CrewComms::RangeLossDb(20.0, 13.0), 40.0 * std::log10(20.0 / 13.0), 1e-9);
    SC_EXPECT_NEAR(comms.ReceiveLossDb(0, c), 0.0, 1e-12);  // 4 m whip, suppressor off: the reference
    c.panel.Set("radio.left.squelch", 1);
    SC_EXPECT_NEAR(comms.ReceiveLossDb(0, c), 7.5, 0.05);
    c.panel.Set("radio.left.antenna", 1);  // 1 m whip: 8 km
    SC_EXPECT_NEAR(comms.ReceiveLossDb(0, c), 7.5 + 15.9, 0.1);
    c.panel.Set("radio.left.squelch", 0);
    SC_EXPECT_NEAR(comms.ReceiveLossDb(0, c), 15.9, 0.1);
    c.panel.Set("radio.left.antenna", 2);  // the emergency wire: 4 km
    SC_EXPECT_NEAR(comms.ReceiveLossDb(0, c), 28.0, 0.1);
    SC_EXPECT_NEAR(comms.ReceiveLossDb(1, c), 0.0, 1e-12);  // the other set is untouched
}

SC_TEST("comms: a set hears only when switched on, warmed up (10 min) and turned up") {
    auto r = Load(Text());
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    const CrewComms& comms = *r->Comms();
    Steps(*r, 2.0);
    std::string why;
    SC_EXPECT(comms.Receiving(0, c) && comms.Receiving(1, c) && comms.Transmitting(0, c));  // the vehicle starts with the sets warm
    c.panel.Set("radio.left.power", 0);
    Steps(*r, 1.0);
    SC_EXPECT(!comms.Receiving(0, c, &why) && Has(why, "switched off"));
    c.panel.Set("radio.left.power", 1);
    Steps(*r, 1.0);
    SC_EXPECT(!comms.Receiving(0, c, &why) && Has(why, "warming up"));
    Steps(*r, 590.0);  // t about 595 s since switching on
    SC_EXPECT(!comms.Receiving(0, c));
    Steps(*r, 20.0);
    SC_EXPECT(comms.Receiving(0, c));
    c.panel.Set("radio.left.volume", 0);
    SC_EXPECT(!comms.Receiving(0, c, &why) && Has(why, "volume"));
    c.panel.Set("radio.left.volume", 1);
    // Duty reception, then back: the transmitter needs 3 min.
    c.panel.Set("radio.right.mode", 0);
    Steps(*r, 1.0);
    SC_EXPECT(comms.Receiving(1, c) && !comms.Transmitting(1, c));
    c.panel.Set("radio.right.mode", 1);
    Steps(*r, 120.0);
    SC_EXPECT(!comms.Transmitting(1, c));
    Steps(*r, 70.0);
    SC_EXPECT(comms.Transmitting(1, c));
}

SC_TEST("comms: with the listening set off the commander falls back to the other set's source, and says so") {
    auto r = Load(Text());
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    Steps(*r, 100.0);
    SC_EXPECT(r->C2()->ActiveId() == "pu12" && r->C2()->ListeningSet() == 0 && r->C2()->Hearing());
    c.panel.Set("radio.left.power", 0);  // the set he listens on goes off
    Steps(*r, 2.0);
    SC_EXPECT(!r->C2()->Hearing());
    Steps(*r, 400.0);  // the PU-12 goes silent for him; the right set carries the EWR
    SC_EXPECT(r->C2()->ActiveId() == "ewr_north" && r->C2()->ListeningSet() == 1 && r->C2()->Hearing());
    const std::string d = r->Debrief();
    SC_EXPECT(Has(d, "cannot hear the net: R-123M left is switched off"));
    SC_EXPECT(Has(d, "hears the net again"));
}

SC_TEST("comms: the commander's box on the intercom only (ВС) hears no radio; back on Р-123 he hears it again") {
    auto r = Load(Text());
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    Steps(*r, 100.0);
    SC_EXPECT(r->C2()->Hearing() && !r->C2()->Known(r->Time()).empty());
    c.panel.Set("intercom.a1", 1);  // ВС
    Steps(*r, 2.0);
    SC_EXPECT(!r->C2()->CommanderOnRadio() && !r->C2()->Hearing());
    c.panel.Set("intercom.a1", 2);  // Р-123
    Steps(*r, 2.0);
    SC_EXPECT(r->C2()->CommanderOnRadio() && r->C2()->Hearing());
    const std::string d = r->Debrief();
    SC_EXPECT(Has(d, "cannot hear the net: his intercom box is not on the radio (A-1)") && Has(d, "hears the net again"));
}

SC_TEST("comms: an order goes by the intercom at once, and is shouted when the commander's box is off or the member's is on the radio") {
    auto r = Load(Text());
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    // each crew member gets an order of his own seat (a member refuses another seat's procedure)
    const Procedure* p_op = c.FindProcedure("equipment_on");
    const Procedure* p_driver = c.FindProcedure("power_up");
    const Procedure* p_deputy = c.FindProcedure("orient_vehicle");
    const Procedure* p_cmd = c.FindProcedure("radio_right_on");
    SC_EXPECT(p_op != nullptr && p_driver != nullptr && p_deputy != nullptr && p_cmd != nullptr);
    if (p_op == nullptr || p_driver == nullptr || p_deputy == nullptr || p_cmd == nullptr) return;
    Steps(*r, 2.0);
    CrewMember* op = r->Crew("Operator");
    CrewMember* driver = r->Crew("Driver");
    CrewMember* cmd = r->Crew("Commander");
    SC_EXPECT(op != nullptr && driver != nullptr && cmd != nullptr);
    r->GiveOrder(*op, *p_op, false);
    SC_EXPECT(r->PendingOrders() == 0 && op->IsBusy());  // the intercom: at once
    op->CancelOrder();
    c.panel.Set("intercom.a1", 0);  // ВЫКЛ
    Steps(*r, 1.0);
    r->GiveOrder(*cmd, *p_cmd, false);
    SC_EXPECT(r->PendingOrders() == 0);  // his own order is not shouted to himself
    cmd->CancelOrder();
    r->GiveOrder(*op, *p_op, false);
    SC_EXPECT(r->PendingOrders() == 1 && !op->IsBusy());
    Steps(*r, 3.0);  // the turbine runs: 4 s
    SC_EXPECT(r->PendingOrders() == 1 && !op->IsBusy());
    Steps(*r, 1.5);
    SC_EXPECT(r->PendingOrders() == 0 && op->IsBusy());
    SC_EXPECT(Has(r->Debrief(), "is shouted, it arrives in 4.0 s"));
    op->CancelOrder();
    c.panel.Set("intercom.a1", 2);
    Steps(*r, 1.0);
    c.panel.Set("intercom.pv", 1);  // the driver is on the radio: he does not hear the intercom
    r->GiveOrder(*driver, *p_driver, false);
    SC_EXPECT(r->PendingOrders() == 1);
    Steps(*r, 5.0);
    SC_EXPECT(r->PendingOrders() == 0 && driver->IsBusy());
    driver->CancelOrder();
    c.panel.Set("intercom.pv", 0);
    c.panel.Set("intercom.a2", 1);  // the deputy on the radio
    r->GiveOrder(*r->Crew("Deputy"), *p_deputy, true);
    SC_EXPECT(r->PendingOrders() == 1);
}

SC_TEST("comms: without the turbine and the take-off generator a shouted order takes only the quiet time") {
    auto r = Load(Text());
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    c.panel.Set("pwr.gta", 0);
    c.panel.Set("pwr.pto", 0);
    c.panel.Set("intercom.a1", 0);
    Steps(*r, 3.0);
    SC_EXPECT(!r->Comms()->Noisy(c));
    SC_EXPECT_NEAR(r->Comms()->OrderDelayS("operator", c), 1.5, 1e-12);
    SC_EXPECT_NEAR(r->Comms()->OrderDelayS("commander", c), 0.0, 1e-12);
    SC_EXPECT_NEAR(r->Comms()->PassOnExtraS(c), 1.5, 1e-12);
}

SC_TEST("comms: with the commander's box off a designation takes the shouting time longer to reach the crew") {
    // The PU-12 alone, as in the c2 tests: the crew hears the board's plot 14 s later; shouting adds the extra time.
    auto known_at = [&](double extra) {
        C2Policy policy;
        C2Network n(policy, 1);
        C2SourceDef d;
        d.id = "pu12";
        d.display_name = "PU-12M";
        d.short_name = "PU-12";
        d.kind = C2Kind::BatteryPost;
        d.sense = C2Sense::Relayed;
        std::string error;
        if (!LoadPlottingBoardFile(kData + "/command/pu12.json", d.board, &error)) std::printf("    board: %s\n", error.c_str());
        d.board.navigation_error_deg = 0.0;
        d.board.navigation_error_m = 0.0;
        n.AddSource(d, Vec3{});
        n.SetPassOnExtraS(extra);
        BoardContact t;
        t.target_id = "a";
        t.position = Vec3{0.0, 40000.0, 1000.0};
        t.altitude_m = 1000.0;
        for (double now = 0.0; now < 60.0; now += 0.5) {
            n.Update(now, {t}, Vec3{});
            if (!n.Known(now).empty()) return now;
        }
        return -1.0;
    };
    const double normal = known_at(0.0);
    const double shouted = known_at(4.0);
    SC_EXPECT_NEAR(normal, 14.0, 0.6);
    SC_EXPECT_NEAR(shouted - normal, 4.0, 0.6);
}

SC_TEST("comms: with the EM layer a shorter antenna or the squelch shortens the net: near carries, the wire does not") {
    std::string text = Text();
    const std::string from = "\"readiness\": \"deployed\",";
    text.replace(text.find(from), from.size(),
                 from + " \"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" },");
    auto r = Load(text);
    SC_EXPECT(r != nullptr && r->Em() != nullptr);
    if (!r || r->Em() == nullptr) return;
    ConsoleData& c = r->Console();
    Steps(*r, 5.0);
    const double p_whip = r->Em()->RadioDeliveryProbability("ewr_north");  // 8 km, 4 m whip
    SC_EXPECT(p_whip > 0.95);
    c.panel.Set("radio.left.squelch", 1);  // the listening set: 13 km class
    Steps(*r, 1.0);
    const double p_squelch = r->Em()->RadioDeliveryProbability("ewr_north");
    SC_EXPECT(p_squelch < p_whip && p_squelch > 0.5);
    c.panel.Set("radio.left.squelch", 0);
    c.panel.Set("radio.left.antenna", 1);  // the 1 m whip: 8 km class
    Steps(*r, 1.0);
    SC_EXPECT(r->Em()->RadioDeliveryProbability("ewr_north") < 0.5);
    c.panel.Set("radio.left.antenna", 2);  // the emergency wire
    Steps(*r, 1.0);
    SC_EXPECT(r->Em()->RadioDeliveryProbability("ewr_north") < 0.01);
    c.panel.Set("radio.left.antenna", 0);
    Steps(*r, 1.0);
    SC_EXPECT(r->Em()->RadioDeliveryProbability("ewr_north") > 0.95);
    SC_EXPECT_NEAR(r->Em()->UnitRxLossDb(), 0.0, 1e-12);
}

SC_TEST("comms: a source's frequency is on the R-123M's 25 kHz grid, 20.000 to 51.500 MHz; others are refused") {
    auto with = [&](const char* mhz) {
        return Text(std::string(R"(, { "id": "ewr_b", "type": "ewr_voice", "position": { "east_m": 0, "north_m": 9000, "up_m": 0 }, "frequency_mhz": )") + mhz + " }");
    };
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(with("38.55"), kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    double hz = 0.0;
    for (const C2SourceDef& s : def.c2_sources) {
        if (s.id == "ewr_b") hz = s.frequency_hz;
    }
    SC_EXPECT_NEAR(hz, 38.55e6, 1.0);
    for (const char* bad : {"38.51", "19.975", "51.525"}) {
        ScenarioDef d;
        ConsoleData c;
        std::string e;
        const bool ok = LoadScenario(with(bad), kData, d, c, &e);
        if (ok || e.find("frequency_mhz") == std::string::npos) std::printf("    %s: '%s'\n", bad, e.c_str());
        SC_EXPECT(!ok && e.find("frequency_mhz") != std::string::npos);
    }
}

SC_TEST("comms: the radio-on procedures bring the sets and the commander's box to their working state") {
    auto r = Load(Text());
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    c.panel.Set("radio.right.power", 0);
    c.panel.Set("radio.right.volume", 0);
    c.panel.Set("intercom.a1", 1);
    Steps(*r, 2.0);
    SC_EXPECT(!r->Comms()->Receiving(1, c));
    CrewMember* cmd = r->Crew("Commander");
    const Procedure* p = c.FindProcedure("radio_right_on");
    SC_EXPECT(cmd != nullptr && p != nullptr);
    if (cmd == nullptr || p == nullptr) return;
    r->GiveOrder(*cmd, *p, false);
    Steps(*r, 30.0);
    SC_EXPECT(c.panel.Get("radio.right.power") == 1 && c.panel.Get("radio.right.volume") == 1 && c.panel.Get("intercom.a1") == 2);
    SC_EXPECT(!cmd->IsBusy());  // the drill is done: the set warms up on its own
    SC_EXPECT(!r->Comms()->Receiving(1, c));  // still warming (10 min)
    Steps(*r, 700.0);
    SC_EXPECT(r->Comms()->Receiving(1, c) && r->C2()->CommanderOnRadio());
}

// ---- Transmitting from the vehicle (roadmap 7.8)

SC_TEST("comms 7.8: the commander can transmit only on a set that is on, warm, in СИМПЛЕКС, with A-1 on Р-123") {
    auto r = Load(Text());
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ConsoleData& c = r->Console();
    r->Step(0.5);
    std::string why;
    SC_EXPECT(r->CommanderCanTransmit(&why));
    const int set = r->C2()->ListeningSet();
    const std::string side = set == 0 ? "left" : "right";
    c.panel.Set("intercom.a1", 1);  // ВС: on the intercom only
    r->Step(0.5);
    SC_EXPECT(!r->CommanderCanTransmit(&why) && Has(why, "A-1 is not on Р-123"));
    c.panel.Set("intercom.a1", 2);
    c.panel.Set("radio." + side + ".mode", 0);  // duty reception: the transmitter valves are cold
    r->Step(0.5);
    SC_EXPECT(!r->CommanderCanTransmit(&why) && Has(why, "not ready to transmit"));
    c.panel.Set("radio." + side + ".mode", 1);
    Steps(*r, 60.0);
    SC_EXPECT(!r->CommanderCanTransmit());  // 3 min after duty reception
    Steps(*r, 125.0);
    SC_EXPECT(r->CommanderCanTransmit());
    c.panel.Set("radio." + side + ".power", 0);
    r->Step(0.5);
    SC_EXPECT(!r->CommanderCanTransmit(&why) && Has(why, "switched off"));
}

SC_TEST("comms 7.8: a player commander's radio report goes out only with his chest switch at ПРД, and is on the air") {
    auto r = Load(Text());
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("commander", "player"));
    r->Step(0.5);
    SC_EXPECT(!r->PlayerSay("commander", "report_ready"));
    SC_EXPECT(Has(r->Debrief(), "cannot transmit: the chest switch is at receive"));
    const int before = r->Transmissions();
    r->Console().panel.Set("radio.chest", 1);
    SC_EXPECT(r->PlayerSay("commander", "report_ready"));
    Steps(*r, 10.0);
    SC_EXPECT(r->Transmissions() > before);
    SC_EXPECT(Has(r->Debrief(), "к бою готов"));
    SC_EXPECT(Has(r->Debrief(), "the commander keys R-123M"));
}

SC_TEST("comms 7.8: ТОН. ВЫЗОВ sends a call tone only while the commander transmits on that set") {
    auto r = Load(Text());
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("commander", "player"));
    r->Step(0.5);
    const std::string side = r->C2()->ListeningSet() == 0 ? "left" : "right";
    const std::string other = side == "left" ? "right" : "left";
    r->Console().panel.Set("radio." + side + ".tone", 1);
    r->Step(0.5);
    SC_EXPECT(Has(r->Debrief(), "nothing goes out: not transmitting"));
    r->Console().panel.Set("radio.chest", 1);
    r->Console().panel.Set("radio." + other + ".tone", 1);
    r->Step(0.5);
    SC_EXPECT(Has(r->Debrief(), "is not the set the commander's headset is on"));
    r->Console().panel.Set("radio." + side + ".tone", 1);
    r->Step(0.5);
    SC_EXPECT(Has(r->Debrief(), "ТОН. ВЫЗОВ: a call tone on R-123M " + side));
    bool heard = false;
    for (const ScenarioRunner::RadioMessage& m : r->RadioLog()) heard = heard || Has(m.text, "ТОН. ВЫЗОВ");
    SC_EXPECT(heard);
}

SC_TEST("comms 7.8: the request for the TZM reaches the battery only when the commander's set can transmit") {
    const std::string text = R"({
      "scenario_id": "svc", "name": "Service", "source": "sam3d", "confidence": "estimate", "seed": 2,
      "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 }, "terrain": { "type": "flat", "height_m": 110 },
      "battery": { "own": { "id": "21", "callsign": "21" },
        "vehicles": [ { "id": "TZM", "kind": "reload", "position": { "east_m": 300, "north_m": -400, "up_m": 3 } } ] },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" } ],
      "targets": [], "triggers": [], "end": { "time_limit_s": 1200, "when_targets_gone": false } })";
    auto r = Load(text);
    SC_EXPECT(r != nullptr);
    if (!r || r->GetLauncher() == nullptr) return;
    r->Console().panel.Set("radio.right.power", 0);  // the commander's set (no C2 network: the right one) is off
    for (const char* c : {"1-I", "2-I", "3-I", "1-II", "2-II", "3-II"}) r->GetLauncher()->Unload(c);
    Steps(*r, 120.0, 0.1);
    SC_EXPECT(Has(r->Debrief(), "the request for the TZM can't go out: R-123M right is switched off"));
    SC_EXPECT(!Has(r->Debrief(), "to reload 21"));
    r->Console().panel.Set("radio.right.power", 1);  // on again: 10 min to warm up, then 3 min for the transmitter
    Steps(*r, 820.0, 0.1);
    SC_EXPECT(Has(r->Debrief(), "to reload 21"));
}

SC_TEST("comms 7.8: with the EM layer what the commander says goes on the air on the net of the source on his set") {
    std::string text = Text();
    const std::string from = "\"readiness\": \"deployed\",";
    text.replace(text.find(from), from.size(),
                 from + " \"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" },");
    auto r = Load(text);
    SC_EXPECT(r != nullptr && r->Em() != nullptr);
    if (!r || r->Em() == nullptr) return;
    SC_EXPECT(r->TakeSeat("commander", "player"));
    SC_EXPECT(r->RequestC2Source("ewr_north"));  // a source with a place: an EM radio link
    Steps(*r, 30.0);
    SC_EXPECT(r->C2()->ActiveId() == "ewr_north");
    const std::size_t before = r->Em()->UnitTransmissions();
    r->Console().panel.Set("radio.chest", 1);
    SC_EXPECT(r->PlayerSay("commander", "report_ready"));
    Steps(*r, 3.0);
    SC_EXPECT(r->Em()->UnitTransmissions() > before);
}
