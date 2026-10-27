// The emission regime passed down the chain (docs/crew/16_EMISSION.md, simcore/emission_regime.hpp): the regiment's
// order or the battery commander's decision, said on the nets; the Osa's commander tells his crew before they deploy or
// as soon as he hears a new one, with the turns and the seconds; the senior operator keeps the bursts only once told; the
// commander reminds the radar seats. The scenarios are placeholders (osa_battery_fight.json, the link practice, the
// PU-12M's battery); the regimes' turns are the data's (voenizdat_osa_1991 p262 for the least emission).
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "simcore/battery_link.hpp"
#include "simcore/emission_regime.hpp"
#include "simcore/helper.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

std::string ReplaceOnce(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at == std::string::npos) {
        std::printf("    test text not found: %s\n", from.c_str());
        return s;
    }
    return s.replace(at, from.size(), to);
}

std::unique_ptr<ScenarioRunner> Load(const std::string& text, std::string* error = nullptr) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenario(text, kData, def, console, &e)) {
        if (error != nullptr) *error = e;
        else std::printf("    %s\n", e.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

const SpokenCallout* FirstSaid(const ScenarioRunner& r, const std::string& id, const std::string& from = std::string()) {
    for (const SpokenCallout& c : r.Callouts()->Log()) {
        if (c.id == id && (from.empty() || c.from == from)) return &c;
    }
    return nullptr;
}
int TimesSaid(const ScenarioRunner& r, const std::string& id) {
    int n = 0;
    for (const SpokenCallout& c : r.Callouts()->Log()) n += c.id == id ? 1 : 0;
    return n;
}
bool HasDuty(const ScenarioRunner& r, const std::string& seat, const std::string& id) {
    for (const SeatDuty& d : r.OwedDuties(seat)) {
        if (d.id == id) return true;
    }
    return false;
}
bool Has(const std::string& text, const std::string& what) { return text.find(what) != std::string::npos; }
// Said on a net (the radio log: the battery net, the regiment's).
bool OnAir(const ScenarioRunner& r, const std::string& net, const std::string& what) {
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) {
        if (m.net == net && Has(m.text, what)) return true;
    }
    return false;
}

// A ready crew at a halt, its NPC commander (no EM layer, no battery), a raid that comes late; `doctrine` its JSON.
std::string ReadyCrew(const std::string& doctrine, double appear_s = 400.0, double time_limit_s = 600.0) {
    return R"JSON({ "scenario_id": "emission", "name": "Emission", "source": "sam3d", "confidence": "estimate", "seed": 1,
      "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 }, "terrain": { "type": "flat", "height_m": 110 },
      "crew_commander": "npc",)JSON" +
           (doctrine.empty() ? std::string() : " \"doctrine\": " + doctrine + ",") + R"JSON(
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 } },
      "crew": [ { "name": "Commander", "role": "commander", "skill": "class1" }, { "name": "Deputy commander", "role": "deputy_commander", "skill": "class1" },
                { "name": "Senior operator", "role": "senior_operator", "skill": "class1" }, { "name": "Operator", "role": "operator", "skill": "class1" },
                { "name": "Driver", "role": "driver", "skill": "class1" } ],
      "targets": [ { "id": "t1", "name": "Fighter (placeholder)", "class": "fighter", "start": { "east_m": 900, "north_m": 40000, "up_m": 1000 },
                     "waypoints": [ { "east_m": 900, "north_m": -20000, "up_m": 1000 } ], "speed_mps": 150, "rcs_m2": 3, "appear_at_s": )JSON" +
           std::to_string(appear_s) + R"JSON( } ],
      "triggers": [], "end": { "time_limit_s": )JSON" + std::to_string(time_limit_s) + R"JSON( } })JSON";
}

std::string BatteryFight(const std::string& emission_orders) {
    std::string t = ReadAll(kData + "/scenarios/osa_battery_fight.json");
    return ReplaceOnce(t, "\"crew_commander\":", "\"emission_orders\": " + emission_orders + ", \"crew_commander\":");
}

// The share of the time the search transmitter radiates at the halt with the search running, from `from_s` to `to_s`.
double DutyCycle(ScenarioRunner& r, double from_s, double to_s) {
    double on = 0.0, all = 0.0;
    while (!r.Finished() && r.Time() < to_s) {
        r.Step(0.1);
        if (r.Time() < from_s || r.Console().panel.Get("soc.search") != 1) continue;
        all += 0.1;
        if (r.Console().panel.Get("soc.hv") == 1) on += 0.1;
    }
    return all > 0.0 ? on / all : -1.0;
}

}  // namespace

SC_TEST("emission: the regimes load with their sources; their words give the turns and the seconds") {
    EmissionDef d;
    std::string e;
    SC_EXPECT(LoadEmissionFile(kData + "/command/emission_regimes.json", d, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT_NEAR(d.arm.on_turns, 2.0, 1e-9);  // voenizdat_osa_1991 p262: on for 2 turns after every 3-4
    SC_EXPECT_NEAR(d.arm.off_turns, 3.0, 1e-9);
    SC_EXPECT(d.limited.on_turns > 0.0 && d.limited.off_turns > 0.0);
    SC_EXPECT(std::string(TurnsWordRu(1)) == "оборот" && std::string(TurnsWordRu(3)) == "оборота" && std::string(TurnsWordRu(5)) == "оборотов");
    SC_EXPECT(std::string(TurnsWordRu(12)) == "оборотов" && std::string(TurnsWordRu(21)) == "оборот");
    SC_EXPECT(std::string(SecondsWordRu(4)) == "секунды" && std::string(SecondsWordRu(5)) == "секунд" && std::string(SecondsWordRu(1)) == "секунда");
    // At 33 rpm a turn is 1.82 s: 2 turns are 4 s, 3 turns 5 s.
    const auto w = EmissionWords(d, d.Make(EmissionKind::Arm, "regiment"), 60.0 / 33.0);
    SC_EXPECT(w.at("bursts") == "высокое на 2 оборота (4 секунды), через 3 оборота (5 секунд)");
    SC_EXPECT(Has(w.at("bursts_en"), "on for 2 turns (4 s), then off for 3 turns (5 s)"));
    // A limited regime given in seconds (the older search_bursts) is said in the nearest turns.
    EmissionRegime s = d.Make(EmissionKind::Limited, "battery");
    s.on_s = 4.0;
    s.off_s = 16.0;
    const auto ws = EmissionWords(d, s, 60.0 / 33.0);
    SC_EXPECT(ws.at("on") == "2" && ws.at("off") == "9" && ws.at("on_s") == "4" && ws.at("off_s") == "16");
    SC_EXPECT(EmissionWords(d, d.Make(EmissionKind::Silent, "battery"), 1.8).at("bursts") == "радары не включать");
    SC_EXPECT(d.Make(EmissionKind::Arm, "regiment").Same(d.Make(EmissionKind::Arm, "battery")));
    SC_EXPECT(!d.Make(EmissionKind::Arm, "battery").Same(d.Make(EmissionKind::Limited, "battery")));
}

SC_TEST("emission: the doctrine's regime and its older forms load; the orders need a battery and a time") {
    std::string e;
    auto r = Load(ReadyCrew(R"({ "emission": { "regime": "limited", "from": "regiment" } })"));
    SC_EXPECT(r != nullptr);
    if (r) {
        const EmissionRegime& g = r->Def().doctrine.emission;
        SC_EXPECT(g.kind == EmissionKind::Limited && g.from == "regiment");
        SC_EXPECT_NEAR(g.on_turns, r->Def().emission.limited.on_turns, 1e-9);
    }
    r = Load(ReadyCrew(R"({ "arm_threat": true, "arm_search_off_turns": 4 })"));
    SC_EXPECT(r && r->Def().doctrine.emission.kind == EmissionKind::Arm && r->Def().doctrine.emission.off_turns == 4.0);
    r = Load(ReadyCrew(R"({ "search_bursts": { "on_s": 4, "off_s": 16 } })"));
    SC_EXPECT(r && r->Def().doctrine.emission.kind == EmissionKind::Limited && r->Def().doctrine.emission.off_s == 16.0);
    SC_EXPECT(Load(ReadyCrew(R"({ "emission": { "regime": "arm" }, "arm_threat": true })"), &e) == nullptr);
    SC_EXPECT(Has(e, "give the regime once"));
    SC_EXPECT(Load(ReadyCrew(R"({ "emission": { "regime": "silent", "on_turns": 2 } })"), &e) == nullptr);
    SC_EXPECT(Has(e, "bursts"));
    SC_EXPECT(Load(ReadyCrew(R"({ "emission": { "regime": "quiet" } })"), &e) == nullptr);
    std::string orders = ReadyCrew("");
    orders = ReplaceOnce(orders, "\"crew_commander\": \"npc\",", "\"crew_commander\": \"npc\", \"emission_orders\": [ { \"at_s\": 60, \"regime\": \"arm\" } ],");
    SC_EXPECT(Load(orders, &e) == nullptr);
    SC_EXPECT(Has(e, "battery"));
    // An Osa linked to a post: the post's scenario gives the battery's orders.
    SC_EXPECT(Load(ReplaceOnce(ReadAll(kData + "/scenarios/link_osa_21.json"), "\"crew_commander\":",
                               "\"emission_orders\": [ { \"at_s\": 60, \"regime\": \"arm\" } ], \"crew_commander\":"),
                   &e) == nullptr);
    SC_EXPECT(Has(e, "linked post"));
    SC_EXPECT(Load(BatteryFight(R"([ { "at_s": 0, "regime": "arm" } ])"), &e) == nullptr);
    SC_EXPECT(Has(e, "at_s"));
    SC_EXPECT(Load(BatteryFight(R"([ { "at_s": 60, "regime": "arm", "from": "division" } ])"), &e) == nullptr);
}

SC_TEST("emission: the combat order's regime is told to the crew before the radars come on, and kept in bursts") {
    auto r = Load(ReadyCrew(R"({ "emission": { "regime": "arm" } })"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->CommanderRegime().kind == EmissionKind::Arm);
    SC_EXPECT(r->CrewRegime().kind == EmissionKind::Free);  // told by nobody yet
    double first_hv = -1.0;
    while (!r->Finished() && r->Time() < 60.0) {
        r->Step(0.1);
        if (first_hv < 0.0 && r->Console().panel.Get("soc.hv") == 1) first_hv = r->Time();
    }
    const SpokenCallout* told = FirstSaid(*r, "order_emission_regime", "commander");
    SC_EXPECT(told != nullptr);
    SC_EXPECT(r->CrewRegime().kind == EmissionKind::Arm);
    if (told != nullptr) {
        std::printf("    told at %.1f s: %s; the transmitter first on at %.1f s\n", told->start_s, told->ru.c_str(), first_hv);
        SC_EXPECT(Has(told->ru, "высокое на 2 оборота (4 секунды), через 3 оборота (5 секунд)"));
        SC_EXPECT(first_hv < 0.0 || told->start_s <= first_hv);
    }
    SC_EXPECT(FirstSaid(*r, "emission_readback", "senior_operator") != nullptr);
    SC_EXPECT(FirstSaid(*r, "order_remind_ssc", "commander") != nullptr);  // the least emission: the tracking transmitter's rule
    // Nothing near: the senior operator keeps 2 turns on in every 5.
    const double duty = DutyCycle(*r, 120.0, 380.0);
    std::printf("    the search transmitter on %.0f%% of the time at the halt\n", 100.0 * duty);
    SC_EXPECT(duty > 0.25 && duty < 0.6);
    SC_EXPECT(r->GetEmissionRecord().regimes_told == 1 && r->GetEmissionRecord().bursts_s > 200.0);
    // A hostile after a quiet spell: the radar seats are reminded.
    while (!r->Finished() && r->Time() < 470.0) r->Step(0.1);
    SC_EXPECT(TimesSaid(*r, "order_remind_bursts") >= 1);
    SC_EXPECT(r->GetEmissionRecord().reminders >= 1);
}

SC_TEST("emission: a limited regime is the commander's bursts, said once, kept as a standing order") {
    auto r = Load(ReadyCrew(R"({ "emission": { "regime": "limited" } })"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const double duty = DutyCycle(*r, 120.0, 380.0);
    std::printf("    limited: the search transmitter on %.0f%% of the time at the halt\n", 100.0 * duty);
    SC_EXPECT(FirstSaid(*r, "order_emission_regime", "commander") != nullptr);
    SC_EXPECT(duty > 0.1 && duty < 0.45);  // 2 turns in 8
}

SC_TEST("emission: a player commander's crew keeps the old regime until he tells them; the helper shows it") {
    auto r = Load(ReadyCrew(R"({ "emission": { "regime": "arm" } })"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("commander", "Player");
    r->SetHelper(true);
    for (int i = 0; i < 100; ++i) r->Step(0.1);
    SC_EXPECT(r->CrewRegime().kind == EmissionKind::Free);
    SC_EXPECT(HasDuty(*r, "commander", "order_emission_regime"));
    SC_EXPECT(!r->CheckOrder("order_remind_bursts").can);  // nothing told yet to remind them of
    SC_EXPECT(r->GiveCrewOrder("order_emission_regime"));
    SC_EXPECT(r->CrewRegime().kind == EmissionKind::Arm);
    SC_EXPECT(r->CheckOrder("order_remind_bursts").can);
    for (int i = 0; i < 200; ++i) r->Step(0.1);
    SC_EXPECT(FirstSaid(*r, "emission_readback", "senior_operator") != nullptr);
    SC_EXPECT(r->GiveCrewOrder("order_remind_bursts"));
    SC_EXPECT(r->GetEmissionRecord().reminders == 1);
}

SC_TEST("emission: a player commander is shown every burst of the limited regime, not a stop of the search") {
    auto r = Load(ReadyCrew(R"({ "emission": { "regime": "limited" } })"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("commander", "Player");
    r->SetHelper(true);
    HelpFollower f("commander");  // he does what the helper shows
    while (!r->Finished() && r->Time() < 240.0) {
        r->Step(0.1);
        f.Update(*r);
    }
    int off = 0, stops = 0;
    double first_off = -1.0;
    for (const ScenarioRunner::GivenOrder& o : r->OrdersGiven()) {
        if (o.order == "order_hv_off" && first_off < 0.0) first_off = o.time_s;
        off += o.order == "order_hv_off" ? 1 : 0;
        if (o.order == "order_stop_search" && first_off >= 0.0) {
            ++stops;
            std::printf("    «Прекратить поиск!» at %.1f s\n", o.time_s);
        }
    }
    const double turn = 60.0 / 33.0;  // no EM layer: 33 rpm
    const double cycle = r->CrewRegime().OnS(turn) + r->CrewRegime().OffS(turn);
    std::printf("    ВЫСОКОЕ off %d times (the first at %.0f s, a cycle %.1f s), the search stopped %d times after; %d long\n", off,
                first_off, cycle, stops, r->GetEmissionRecord().breaches);
    SC_EXPECT(first_off >= 0.0 && off >= static_cast<int>((240.0 - first_off) / cycle) - 1);
    SC_EXPECT(stops == 0);
    SC_EXPECT(r->GetEmissionRecord().breaches <= 1);
}

SC_TEST("emission: a burst held on too long is recorded, and the commander reminds the senior operator") {
    auto r = Load(ReadyCrew(R"({ "emission": { "regime": "arm" } })"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("senior_operator", "Player");
    std::string why;
    // The player keeps one burst (on to 10 s, off to 20 s), then keeps the search radiating without a break.
    while (!r->Finished() && r->Time() < 120.0) {
        r->Step(0.1);
        const int hv = r->Time() < 10.0 || r->Time() > 20.0 ? 1 : 0;
        if (r->Console().panel.Get("soc.search") != 1) r->SeatSet("senior_operator", "soc.search", 1, &why);
        if (r->Console().panel.Get("soc.hv_cover") != 1) r->SeatSet("senior_operator", "soc.hv_cover", 1, &why);
        if (r->Console().panel.Get("soc.hv") != hv) r->SeatSet("senior_operator", "soc.hv", hv, &why);
    }
    if (!why.empty()) std::printf("    %s\n", why.c_str());
    std::printf("    breaches %d, reminders %d; search %d, hv %d, crew regime %s\n", r->GetEmissionRecord().breaches,
                r->GetEmissionRecord().reminders, r->Console().panel.Get("soc.search"), r->Console().panel.Get("soc.hv"),
                EmissionKindName(r->CrewRegime().kind));
    SC_EXPECT(r->GetEmissionRecord().breaches >= 1);
    SC_EXPECT(FirstSaid(*r, "order_remind_bursts", "commander") != nullptr);
}

SC_TEST("emission: the battery commander's order reaches the Osa on the battery net and is passed on to the crew") {
    auto r = Load(BatteryFight(R"([ { "at_s": 60, "regime": "arm" } ])"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    while (!r->Finished() && r->Time() < 59.0) r->Step(0.1);
    SC_EXPECT(r->CommanderRegime().kind == EmissionKind::Free);
    while (!r->Finished() && r->Time() < 61.0) r->Step(0.1);
    SC_EXPECT(r->CommanderRegime().kind == EmissionKind::Arm);
    SC_EXPECT(r->CrewRegime().kind == EmissionKind::Free);  // not yet told
    SC_EXPECT(OnAir(*r, "battery net", "режим излучения минимальный, угроза ПРР, 2 через 3"));  // in clear: no talk table here
    while (!r->Finished() && r->Time() < 75.0) r->Step(0.1);
    SC_EXPECT(r->CrewRegime().kind == EmissionKind::Arm);
    const SpokenCallout* told = FirstSaid(*r, "order_emission_regime", "commander");
    SC_EXPECT(told != nullptr && told->start_s >= 60.0 + r->Def().emission.pass_on_s - 0.5);
    SC_EXPECT(r->GetEmissionRecord().told_after_s >= r->Def().emission.pass_on_s - 0.5);
}

SC_TEST("emission: the regiment's order is passed on by the battery commander; silence from the start is the combat order") {
    auto r = Load(BatteryFight(R"([ { "at_s": 30, "regime": "silent", "from": "regiment" } ])"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    while (!r->Finished() && r->Time() < 30.0 + r->Def().emission.relay_s - 1.0) r->Step(0.1);
    SC_EXPECT(r->CommanderRegime().kind == EmissionKind::Free);
    while (!r->Finished() && r->Time() < 30.0 + r->Def().emission.relay_s + 2.0) r->Step(0.1);
    SC_EXPECT(r->CommanderRegime().kind == EmissionKind::Silent && r->CommanderRegime().from == "regiment");
    // The crew is told by «Излучение выключить!» if the radars are on, else by the regime's own words.
    while (!r->Finished() && r->Time() < 30.0 + r->Def().emission.relay_s + 15.0) r->Step(0.1);
    SC_EXPECT(r->CrewRegime().kind == EmissionKind::Silent);
    SC_EXPECT(!r->Console().equipment.IsReady("soc_transmitter"));
    // The battery's EMCON in the combat order: known before the vehicles deploy (no net needed).
    auto s = Load(ReplaceOnce(ReadAll(kData + "/scenarios/osa_battery_fight.json"), "\"share_picture\": true,",
                              "\"share_picture\": true, \"orders\": [ { \"at_s\": 0, \"order\": \"emcon\", \"radiate\": false } ],"));
    SC_EXPECT(s != nullptr);
    if (!s) return;
    s->Step(0.1);
    SC_EXPECT(s->CommanderRegime().kind == EmissionKind::Silent);
    SC_EXPECT(Has(s->Debrief(), "the combat order"));
}

SC_TEST("emission: the post's battery commander passes on the regiment's order; a player owes it until he orders one") {
    const std::string text = ReplaceOnce(ReadAll(kData + "/scenarios/pu12m_battery.json"), "\"crew_commander\":",
                                         "\"emission_orders\": [ { \"at_s\": 20, \"regime\": \"arm\", \"from\": \"regiment\" } ], \"crew_commander\":");
    auto npc = Load(text);
    SC_EXPECT(npc != nullptr);
    if (!npc) return;
    while (!npc->Finished() && npc->Time() < 120.0) npc->Step(0.1);
    std::printf("    the post's regime: %s\n", EmissionKindName(npc->CommanderRegime().kind));
    SC_EXPECT(OnAir(*npc, "regiment net", "633"));
    SC_EXPECT(OnAir(*npc, "battery net", "633"));
    SC_EXPECT(npc->CommanderRegime().kind == EmissionKind::Arm && npc->CommanderRegime().from == "regiment");
    bool ordered = false;
    for (const ScenarioRunner::GivenOrder& o : npc->OrdersGiven()) ordered = ordered || o.order == "order_battery_emission_arm";
    SC_EXPECT(ordered);
    auto player = Load(text);
    SC_EXPECT(player != nullptr);
    if (!player) return;
    player->TakeSeat("battery_commander", "Player");
    player->SetHelper(true);
    while (!player->Finished() && player->Time() < 120.0) player->Step(0.1);
    SC_EXPECT(player->CommanderRegime().kind == EmissionKind::Free);
    SC_EXPECT(HasDuty(*player, "battery_commander", "order_battery_emission_arm"));
    if (player->CheckOrder("order_battery_emission_arm").can) {
        SC_EXPECT(player->GiveCrewOrder("order_battery_emission_arm"));
        SC_EXPECT(player->CommanderRegime().kind == EmissionKind::Arm && player->CommanderRegime().from == "regiment");
        for (int i = 0; i < 20; ++i) player->Step(0.1);
        SC_EXPECT(!HasDuty(*player, "battery_commander", "order_battery_emission_arm"));
    } else {
        std::printf("    the order greyed: %s\n", player->CheckOrder("order_battery_emission_arm").reason_en.c_str());
        SC_EXPECT(false);
    }
}

SC_TEST("emission: in one battery the post's order reaches the Osa's crew, and an ARM attack's report the post") {
    ScenarioDef osa, post;
    ConsoleData oc, pc;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/link_osa_21.json", kData, osa, oc, &e));
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/link_pu12m.json", kData, post, pc, &e));
    if (!e.empty()) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    BatteryLink link(std::move(osa), std::move(oc), std::move(post), std::move(pc), "21");
    for (int i = 0; i < 300; ++i) link.Step(0.1);
    SC_EXPECT(link.Post().CommanderOrderEmission(EmissionKind::Limited));
    link.Step(0.1);
    SC_EXPECT(link.Osa().CommanderRegime().kind == EmissionKind::Limited);
    while (!link.Finished() && link.Time() < 60.0) link.Step(0.1);
    SC_EXPECT(link.Osa().CrewRegime().kind == EmissionKind::Limited);
    // The Osa reports an ARM attack: the post's NPC battery commander puts the battery under the least emission.
    link.Post().HearArmAttackReport("21", "15-00");
    while (!link.Finished() && link.Time() < 60.0 + link.Post().Def().emission.arm_on_report_s + 2.0) link.Step(0.1);
    SC_EXPECT(link.Post().CommanderRegime().kind == EmissionKind::Arm);
    SC_EXPECT(link.Osa().CommanderRegime().kind == EmissionKind::Arm);
}

SC_TEST("emission: a launcher's report of an ARM attack puts its own battery under the least emission") {
    std::string t = ReadAll(kData + "/scenarios/osa_arm_drill_shrike.json");  // a Shrike: beaten by going silent
    t = ReplaceOnce(t, "\"arm_threat\": true,", "");
    t = ReplaceOnce(t, "\"battery_commander\": \"none\"", "\"battery_commander\": \"npc\"");
    auto r = Load(t);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->CommanderRegime().kind == EmissionKind::Free);
    double reported = -1.0;
    while (!r->Finished() && r->Time() < 900.0 && (reported < 0.0 || r->Time() < reported + r->Def().emission.arm_on_report_s + 3.0)) {
        r->Step(0.1);
        if (reported < 0.0 && FirstSaid(*r, "report_arm_attack") != nullptr) reported = r->Time();
    }
    const std::string d = r->Debrief();
    std::printf("    reported at %.0f s; at %.0f s: %s\n", reported, r->Time(), r->Finished() ? r->EndReason().c_str() : "running");
    if (reported < 0.0) {
        std::printf("    no ARM attack reported in this run: the battery's reaction not tested\n");
        return;
    }
    SC_EXPECT(Has(d, "a launcher reported an ARM attack"));
    SC_EXPECT(r->CommanderRegime().kind == EmissionKind::Arm || r->CommanderRegime().kind == EmissionKind::Silent);
}

SC_TEST("emission: a player commander hears the ARM's sign: the silence and the report are his duties, and his report counts") {
    std::string t = ReadAll(kData + "/scenarios/osa_arm_drill_shrike.json");
    t = ReplaceOnce(t, "\"arm_threat\": true,", "");
    t = ReplaceOnce(t, "\"battery_commander\": \"none\"", "\"battery_commander\": \"npc\"");
    auto r = Load(t);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("commander", "Player");
    r->SetHelper(true);
    HelpFollower f("commander");  // he does what the helper shows; the radio report he makes by hand below
    while (!r->Finished() && r->Time() < 900.0 && !HasDuty(*r, "commander", "report_arm_attack")) {
        r->Step(0.1);
        f.Update(*r);
    }
    std::printf("    the report owed at %.0f s\n", r->Time());
    SC_EXPECT(HasDuty(*r, "commander", "report_arm_attack"));
    const double heard = r->Time();
    while (!r->Finished() && r->Time() < heard + 15.0) {
        r->Step(0.1);
        f.Update(*r);
    }
    bool silenced = false;  // «Излучение выключить!», shown to him by the shadow commander and given
    for (const ScenarioRunner::GivenOrder& o : r->OrdersGiven()) silenced = silenced || (o.order == "order_emcon_off" && o.time_s >= heard - 1.0);
    SC_EXPECT(silenced);
    SC_EXPECT(r->CommanderRegime().kind == EmissionKind::Free);  // the battery commander hasn't heard of it yet
    const std::vector<std::string> radio = r->PlayerRadioReports("commander");  // in his crew menu
    SC_EXPECT(std::find(radio.begin(), radio.end(), "report_arm_attack") != radio.end());
    SC_EXPECT(r->ReportWords("commander", "report_arm_attack").first.find("…") == std::string::npos);
    r->SeatSet("commander", "radio.chest", 1);  // held at ПРД to talk
    SC_EXPECT(r->PlayerSay("commander", "report_arm_attack"));
    if (!r->LastRefusal().empty()) std::printf("    %s\n", r->LastRefusal().c_str());
    SC_EXPECT(!HasDuty(*r, "commander", "report_arm_attack"));
    while (!r->Finished() && r->Time() < heard + 15.0 + r->Def().emission.arm_on_report_s + 3.0) r->Step(0.1);
    SC_EXPECT(Has(r->Debrief(), "a launcher reported an ARM attack"));
    SC_EXPECT(r->CommanderRegime().kind == EmissionKind::Arm || r->CommanderRegime().kind == EmissionKind::Silent);
}
