// The crews' voice paths and the battery's nets (docs/crew/12_COMMUNICATIONS.md): headsets and boxes (voice_comms.hpp),
// the radio data, jamming, the spare frequency, radio silence and lines (radio_nets.hpp), the talk tables
// (talk_tables.hpp), and what the PU-12M and the Osa do with them (scenario_runner.hpp).
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/battery_link.hpp"
#include "simcore/radio_nets.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/talk_tables.hpp"
#include "simcore/voice_comms.hpp"
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

std::string ReplaceOnce(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at != std::string::npos) s.replace(at, from.size(), to);
    return s;
}

std::unique_ptr<ScenarioRunner> LoadText(const std::string& text) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

// The deployed battery (data/scenarios/pu12m_battery.json), with something put into its radio data.
std::unique_ptr<ScenarioRunner> PostBattery(const std::string& radio_data_extra = std::string(), const std::string& own_unit_extra = std::string()) {
    std::string t = ReadFile(kData + "/scenarios/pu12m_battery.json");
    if (!radio_data_extra.empty()) t = ReplaceOnce(t, "\"comms\": \"gameplay/comms.json\",", "\"comms\": \"gameplay/comms.json\", " + radio_data_extra + ",");
    if (!own_unit_extra.empty()) t = ReplaceOnce(t, "\"own_unit\": {", "\"own_unit\": { " + own_unit_extra + ",");
    return LoadText(t);
}

void Steps(ScenarioRunner& r, double seconds) {
    for (double t = 0.0; t < seconds - 1e-9 && !r.Finished(); t += 0.1) r.Step(0.1);
}

bool LogHas(const ScenarioRunner& r, const std::string& net, const std::string& text, const std::string& heard_at = std::string()) {
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) {
        if (m.net == net && m.text.find(text) != std::string::npos && (heard_at.empty() || m.HeardAt(heard_at))) return true;
    }
    return false;
}

}  // namespace

SC_TEST("voice: the PU-12M's boxes: a seat hears a set only through its headset and box; the plotter has none") {
    std::unique_ptr<ScenarioRunner> r = PostBattery();
    SC_EXPECT(r != nullptr && r->Voice() != nullptr);
    if (!r || r->Voice() == nullptr) return;
    Steps(*r, 1.0);
    const VoiceComms& v = *r->Voice();
    // The watch on deployment: the radiotelephonist on РСТ-1 (the air picture), the commander's RTK-1 talking on РСТ-4
    // (the battery) and hearing РСТ-3 (the regiment) besides, the squad leader on РСТ-2, the driver on ВС.
    SC_EXPECT(v.HearsSet("radio_operator", "fug1") && !v.HearsSet("radio_operator", "fug4"));
    SC_EXPECT(v.HearsSet("battery_commander", "fug4") && v.HearsSet("battery_commander", "fug3") && v.TalksOnSet("battery_commander", "fug4"));
    SC_EXPECT(v.HearsSet("squad_leader", "fug2") && v.HearsIntercom("driver") && !v.HearsSet("driver", "fug1"));
    // A PVS box on a set still hears the intercom (one shared intercom net); the plotter has no box at all.
    SC_EXPECT(v.HearsIntercom("radio_operator") && v.Unboxed("operator") && !v.HearsIntercom("operator"));
    SC_EXPECT(r->SeatHearsNet("radio_operator", "air_picture") && !r->SeatHearsNet("operator", "air_picture"));
    // The headset off: nothing but the cabin.
    r->Console().panel.Set("headset.radio_operator", 0);
    Steps(*r, 0.2);
    SC_EXPECT(!r->Voice()->HearsSet("radio_operator", "fug1") && !r->Voice()->HearsIntercom("radio_operator"));
    SC_EXPECT(r->Voice()->WhyNot("radio_operator", "fug1") == "the headset is not on");
    // The RTK-1 switched off: the commander's box is dead.
    r->Console().panel.Set("rtk1.power", 0);
    Steps(*r, 0.2);
    SC_EXPECT(!r->Voice()->OnBox("battery_commander"));
}

SC_TEST("voice: the Osa's R-124: A-1 on the radio keeps the intercom; A-2 on the radio leaves it but hears the call; A-1 off, no intercom") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    Steps(r, 1.0);
    SC_EXPECT(r.Voice() != nullptr);
    if (r.Voice() == nullptr) return;
    SC_EXPECT(r.Voice()->HearsIntercom("commander") && r.Voice()->HearsSet("commander", "left"));  // A-1 on Р-123, the left set
    r.Console().panel.Set("intercom.a2", 1);  // the deputy on the radio
    Steps(r, 0.2);
    SC_EXPECT(!r.Voice()->HearsIntercom("deputy_commander") && r.Voice()->HearsCall("deputy_commander") &&
              r.Voice()->HearsSet("deputy_commander", "left"));
    SC_EXPECT(r.Voice()->HearsIntercom("senior_operator") && !r.Voice()->HearsSet("senior_operator", "left"));
    r.Console().panel.Set("intercom.a1", 0);  // ВЫКЛ: the intercom is off
    Steps(r, 1.0);
    SC_EXPECT(!r.Voice()->Powered() && !r.Voice()->HearsCall("deputy_commander"));
}

SC_TEST("radio nets: scheduled changes, the main station's order, jamming that a spot jammer follows, direction finding, silence") {
    RadioDataDef d;
    RadioNetDef battery;
    battery.id = "battery";
    battery.set = "fug4";
    battery.frequencies_mhz = {44.0, 42.2};
    battery.exempt = true;
    battery.changes = {{600.0, 2}};
    RadioNetDef regiment;
    regiment.id = "regiment";
    regiment.main = "regiment";
    regiment.frequencies_mhz = {31.5, 34.2};
    regiment.orders = {{100.0, 2}};
    d.nets = {battery, regiment};
    JammingDef spot;
    spot.nets = {"battery"};
    spot.spot = true;
    spot.strong = true;
    spot.from_s = 50.0;
    spot.to_s = 2000.0;
    spot.follow_s = 120.0;
    JammingDef weak;
    weak.band_lo_mhz = 30.0;
    weak.band_hi_mhz = 32.0;
    weak.strong = false;
    weak.from_s = 0.0;
    weak.to_s = 90.0;
    d.jamming = {spot, weak};
    d.jamming_follows_df = true;
    d.df_jam_s = 300.0;
    d.present = true;
    CommsTuning tu;
    RadioNets n(d, tu, 7u);
    // The weak barrage on 30-32 MHz: the regiment's working frequency is in it; about weak_jam_loss of messages lost.
    n.Step(1.0);
    SC_EXPECT(n.Jam("regiment", 1.0) == 1 && n.Jam("battery", 1.0) == 0);
    int got = 0;
    for (int i = 0; i < 1000; ++i) got += n.GetsThrough("regiment", 1.0) ? 1 : 0;
    SC_EXPECT(got > 500 && got < 700);
    // The spot jammer finds the battery net on no. 1 at 50 s.
    n.Step(60.0);
    SC_EXPECT(n.Jam("battery", 60.0) == 2 && !n.GetsThrough("battery", 60.0, true));
    // The regiment orders its spare at 100 s: due as an order (said on air).
    const std::vector<RadioNets::Due> due = n.Step(100.0);
    SC_EXPECT(due.size() == 1 && due[0].net == "regiment" && due[0].to == 2 && due[0].ordered);
    SC_EXPECT(n.Current("regiment") == 2 && n.Jam("regiment", 100.0) == 0);  // off the barrage's band
    // The battery net to its spare: free of the spot jammer until it follows (120 s).
    SC_EXPECT(n.MoveTo("battery", 2, 200.0, "test") && n.Jam("battery", 200.0) == 0);
    n.Step(201.0);
    SC_EXPECT(n.Jam("battery", 201.0) == 0);
    n.Step(330.0);
    SC_EXPECT(n.Jam("battery", 330.0) == 2);  // it has found no. 2
    // A member that missed the order is off the net; its clock finds the net after 5 min.
    RadioNets::Member& m = n.MemberOf("battery", "22");
    m.on = 1;
    m.lost_s = 200.0;
    SC_EXPECT(!n.MemberOnNet("battery", "22"));
    SC_EXPECT(n.StepMember("battery", "22", 300.0).empty());
    SC_EXPECT(n.StepMember("battery", "22", 501.0).find("finds the net") != std::string::npos && n.MemberOnNet("battery", "22"));
    // Direction finding: 30 s on air within the window fixes the regiment's net and draws a jammer onto it.
    for (int i = 0; i < 8; ++i) n.Transmitted("regiment", 400.0 + i * 10.0, 4.0);
    n.Step(480.0);
    SC_EXPECT(n.Fixed("regiment"));
    n.Step(480.0 + tu.df_reaction_s + 1.0);
    SC_EXPECT(n.Jam("regiment", 480.0 + tu.df_reaction_s + 1.0) == 2);
    // Silence: a transmission on a net that is not exempt breaks it; the battery net (exempt) does not.
    n.SetSilence(true, 700.0, "test");
    SC_EXPECT(!n.Open("regiment") && n.Open("battery"));
    SC_EXPECT(n.Transmitted("regiment", 701.0, 2.0) && !n.Transmitted("battery", 702.0, 2.0) && n.Violations() == 1);
}

SC_TEST("talk tables: ПТ-1 and ПТ-2 load; a group decodes; an ASPD-U command has its group; the game values load") {
    TalkTables t;
    std::string e;
    SC_EXPECT(LoadTalkTablesFile(kData + "/command/talk_tables.json", t, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const TalkTable* in = nullptr;
    const TalkEntry* x = t.Decode("143", &in);
    SC_EXPECT(x != nullptr && in != nullptr && in->name_ru == "ПТ-1" && x->effect == "readiness");
    SC_EXPECT(t.ByAspd(20) != nullptr && t.ByAspd(20)->code == "214");
    SC_EXPECT(t.ByEffect("air_warning") != nullptr && t.ByEffect("air_warning")->code == "333");
    CommsTuning tu;
    SC_EXPECT(LoadCommsTuningFile(kData + "/gameplay/comms.json", tu, &e));
    SC_EXPECT_NEAR(tu.both_frequencies_s, 300.0, 1e-9);
    SC_EXPECT_NEAR(tu.warn_covered_m, 60000.0, 1e-9);
}

SC_TEST("post: the air picture reaches the board only through the radiotelephonist's box on FuG-1, read out to the plotter") {
    std::string t = ReadFile(kData + "/scenarios/pu12m_deploy.json");
    t = ReplaceOnce(t, "\"inputs\": { \"vehicle_stopped\": true }", "\"inputs\": { \"vehicle_stopped\": true }, \"readiness\": \"deployed\"");
    std::unique_ptr<ScenarioRunner> r = LoadText(t);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Steps(*r, 90.0);
    SC_EXPECT(r->Board()->Marks().size() >= 1);
    // Heard on the radio by the radiotelephonist alone; read out in the cabin to the plotter (pvo_bu_x_1984 art. 23).
    SC_EXPECT(LogHas(*r, "air picture", "square", "radio_operator") && !LogHas(*r, "air picture", "square", "operator"));
    SC_EXPECT(LogHas(*r, "cabin", "square", "operator"));
    // PVS-1 left on ВС: nobody hears the net, nothing reaches the board.
    std::string t2 = ReplaceOnce(t, "\"readiness\": \"deployed\"", "\"readiness\": \"deployed\", \"controls\": { \"intercom.pvs1\": 0 }");
    std::unique_ptr<ScenarioRunner> q = LoadText(t2);
    SC_EXPECT(q != nullptr);
    if (!q) return;
    Steps(*q, 90.0);
    SC_EXPECT(q->Board()->Marks().empty() && q->Post()->Held().empty() && q->Post()->Heard().empty());
}

SC_TEST("post: a player radiotelephonist's reports wait for him; his menu shows the one he would read out") {
    std::string t = ReadFile(kData + "/scenarios/pu12m_deploy.json");
    t = ReplaceOnce(t, "\"inputs\": { \"vehicle_stopped\": true }", "\"inputs\": { \"vehicle_stopped\": true }, \"readiness\": \"deployed\"");
    std::unique_ptr<ScenarioRunner> r = LoadText(t);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("radio_operator", "Player"));
    Steps(*r, 90.0);
    SC_EXPECT(!r->Post()->Held().empty() && r->Board()->Marks().empty());  // heard by him alone: nobody else reads it out
    SC_EXPECT(r->Console().equipment.GetInput("air_report_waiting"));
    const std::string held = r->Post()->Held().front().text;
    SC_EXPECT(r->ReportWords("radio_operator", "air_report").first == held);
    SC_EXPECT(r->PlayerSay("radio_operator", "air_report"));
    Steps(*r, 30.0);
    SC_EXPECT(r->Board()->Marks().size() >= 1);  // read out: the plotter wrote it
    // The Osa: a seat's check-in names its number; without the headset he can't say it on the intercom.
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/school_operator.json", kData, def, console, &e));
    ScenarioRunner o(std::move(def), std::move(console));
    SC_EXPECT(o.TakeSeat("operator", "Player"));
    const std::pair<std::string, std::string> w = o.ReportWords("operator", "seat_ready");
    SC_EXPECT(w.first == "Третий готов!" && w.second == "Seat 3 ready!");
    SC_EXPECT(!o.PlayerSay("operator", "seat_ready"));  // the lesson's scenario starts with the headset off
    o.SeatSet("operator", "headset.operator", 1);
    o.Step(0.1);
    SC_EXPECT(o.PlayerSay("operator", "seat_ready"));
}

SC_TEST("post: a player commander designates only with the RTK-1 talking on РСТ-4, and is told why not") {
    std::unique_ptr<ScenarioRunner> r = PostBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("battery_commander", "Player"));
    Steps(*r, 140.0);
    int number = 0;
    for (const BoardMark& m : r->Board()->Marks()) {
        if (m.target_id == "t1") number = m.number;
    }
    SC_EXPECT(number > 0);
    r->PlayerSet("rtk1.channel", 0);  // the intercom
    Steps(*r, 0.2);
    SC_EXPECT(!r->DesignateBatteryTarget("22", number));
    SC_EXPECT(r->LastRefusal().find("РТК-1") != std::string::npos);
    r->PlayerSet("rtk1.channel", 4);  // РСТ-4: the battery net
    Steps(*r, 0.2);
    SC_EXPECT(r->DesignateBatteryTarget("22", number));
}

SC_TEST("post: jammed out, the NPC commander takes the battery net to its spare; the radiotelephonist turns FuG-4; targets go on") {
    std::unique_ptr<ScenarioRunner> r = PostBattery(
        "\"jamming\": [ { \"nets\": [\"battery\"], \"from_s\": 60, \"to_s\": 3000, \"strength\": \"strong\", \"kind\": \"spot\", \"follow_s\": -1 } ]");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Steps(*r, 240.0);
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("jamming recognised") != std::string::npos);
    SC_EXPECT(r->Nets() != nullptr && r->Nets()->Current("battery") == 2);
    SC_EXPECT(r->Console().panel.Get("radio.fug4.preset") == 1);  // ФИКСИР. 2
    SC_EXPECT(LogHas(*r, "battery net", "620. 620"));
    Steps(*r, 200.0);
    SC_EXPECT(r->DesignationsMade() >= 1 && r->ReadbacksHeard() >= 1);
}

SC_TEST("post: the regiment's radio silence, heard on the R-111: the NPC radiotelephonist puts it on duty reception") {
    std::unique_ptr<ScenarioRunner> r = PostBattery("\"silence_orders\": [ { \"at_s\": 30, \"on\": true } ]");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Steps(*r, 90.0);
    SC_EXPECT(r->Nets()->Silence());
    SC_EXPECT(LogHas(*r, "regiment net", "611. 611"));
    SC_EXPECT(r->Console().panel.Get("radio.fug3.mode") == 1);  // ДЕЖ. ПР.
}

SC_TEST("post: the battery net barrage-jammed, a line laid to a launcher: the NPC commander designates by line") {
    std::unique_ptr<ScenarioRunner> r = PostBattery(
        "\"jamming\": [ { \"band_mhz\": [37, 52], \"from_s\": 0, \"to_s\": 3000, \"strength\": \"strong\" } ], "
        "\"lines\": [ { \"line\": 1, \"to\": \"21\" }, { \"line\": 2, \"to\": \"22\" }, { \"line\": 3, \"to\": \"23\" }, { \"line\": 4, \"to\": \"24\" } ]");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Steps(*r, 400.0);
    SC_EXPECT(r->Debrief().find("by line") != std::string::npos);
    SC_EXPECT(r->DesignationsMade() >= 1);
}

SC_TEST("post: the covered unit warned («Воздух», ПТ-2 333) by the squad leader once a hostile is within the warning range") {
    std::unique_ptr<ScenarioRunner> r = PostBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Steps(*r, 300.0);
    SC_EXPECT(r->CoveredWarnedS() >= 0.0);
    SC_EXPECT(LogHas(*r, "covered net", "333"));
}

SC_TEST("post: a line rings; the NPC commander raises its lever and hears the call") {
    std::unique_ptr<ScenarioRunner> r =
        PostBattery("\"lines\": [ { \"line\": 2, \"to\": \"regiment\", \"to_name\": \"the regiment's post\", \"calls\": [ { \"at_s\": 20, \"text\": \"Рябина, я Тополь: проверка связи.\" } ] } ]");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Steps(*r, 21.0);
    SC_EXPECT(r->Console().equipment.GetInput("line2_call"));
    Steps(*r, 10.0);
    SC_EXPECT(!r->Console().equipment.GetInput("line2_call") && LogHas(*r, "line 2", "проверка связи", "battery_commander"));
}

SC_TEST("osa: at К бою the deputy and the operators report «Первый (второй, третий) готов» as each takes up his drill") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_crew_practice.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    Steps(r, 240.0);
    const std::string d = r.Debrief();
    SC_EXPECT(d.find("Первый готов") != std::string::npos && d.find("Второй готов") != std::string::npos &&
              d.find("Третий готов") != std::string::npos);
}

SC_TEST("post: the battery reported ready to the regiment on the R-111 once its communications and its launchers are ready") {
    std::unique_ptr<ScenarioRunner> r = PostBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Steps(*r, 300.0);
    SC_EXPECT(LogHas(*r, "regiment net", "к бою готова"));
}

SC_TEST("post: the orders menu's orders on the nets are greyed with the reason when the commander cannot talk on the net") {
    std::unique_ptr<ScenarioRunner> r = PostBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("battery_commander", "Player"));
    Steps(*r, 2.0);
    SC_EXPECT(r->CheckOrder("order_battery_spare").can);
    r->PlayerSet("rtk1.channel", 0);
    Steps(*r, 0.2);
    const ScenarioRunner::OrderCheck c = r->CheckOrder("order_battery_spare");
    SC_EXPECT(!c.can && c.reason_en.find("РТК-1") != std::string::npos);
    SC_EXPECT(!r->GiveCrewOrder("order_battery_spare"));
    r->PlayerSet("rtk1.channel", 4);
    Steps(*r, 0.2);
    SC_EXPECT(r->GiveCrewOrder("order_battery_spare") && r->Nets()->Current("battery") == 2);
}

SC_TEST("link: the post's order to the spare reaches the Osa on its net; its NPC commander turns his set; read-backs reach the post") {
    std::string e;
    std::unique_ptr<BatteryLink> link = LoadBatteryLinkFile(kData + "/scenarios/battery_link_practice.json", kData, &e);
    SC_EXPECT(link != nullptr);
    if (!link) return;
    for (int i = 0; i < 600; ++i) link->Step(0.1);
    SC_EXPECT(link->Post().GiveCrewOrder("order_battery_spare"));
    for (int i = 0; i < 300; ++i) link->Step(0.1);
    SC_EXPECT(link->Post().Nets()->Current("battery") == 2);
    SC_EXPECT(link->Osa().Console().panel.Get("radio.left.preset") == 1 || link->Osa().Console().panel.Get("radio.right.preset") == 1);
    link->RunToEnd(0.1, 900.0);
    SC_EXPECT(link->Post().ReadbacksHeard() >= 1);
}
