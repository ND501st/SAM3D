// Roadmap E6, anti-radiation missiles and the drills against them: hunters fire ARMs at what radiates (a gameplay rule,
// not a seeker model); the crew calls an echo closing faster than any aircraft, the post's observers call launches on
// the battery net, the commander has the radars off and keeps them off; decoy emitters, the ARM's memory and shoot and
// scoot decide what the ARM comes to. Numbers are placeholders (gameplay/osa_ak.json "arm", the scenarios' "arms").
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
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

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string Scenario(const std::string& name) { return ReadFile(kData + "/scenarios/" + name + ".json"); }

std::string ReplaceOnce(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at == std::string::npos) {
        std::printf("    test text not found: %s\n", from.c_str());
        return s;
    }
    return s.replace(at, from.size(), to);
}

std::string ReplaceAll(std::string s, const std::string& from, const std::string& to) {
    for (std::size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
    return s;
}

// A runner for the text; `adjust` may change the loaded definition first (the tuning's game values).
std::unique_ptr<ScenarioRunner> Runner(const std::string& text, const std::function<void(ScenarioDef&)>& adjust = nullptr,
                                       std::string* error = nullptr) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenario(text, kData, def, console, &e)) {
        if (error != nullptr) *error = e;
        else std::printf("    load: %s\n", e.c_str());
        return nullptr;
    }
    if (adjust) adjust(def);
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

bool Has(const std::string& d, const std::string& text) { return d.find(text) != std::string::npos; }

int Count(const std::string& d, const std::string& text) {
    int n = 0;
    for (std::size_t at = d.find(text); at != std::string::npos; at = d.find(text, at + text.size())) ++n;
    return n;
}

// The hunters' scenario without its emission discipline: the radars radiate whenever the drills want them.
std::string HuntersRadiating() {
    return ReplaceOnce(Scenario("osa_sead_hunters"), "\"doctrine\": { \"search_bursts\": { \"on_s\": 4, \"off_s\": 16 } },", "");
}

}  // namespace

SC_TEST("arm: hunters fire at what radiates within their reach; out of reach they don't; an ARM is no target and isn't plotted") {
    auto r = Runner(HuntersRadiating());
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    r->RunToEnd(0.1, 900.0);
    const std::string d = r->Debrief();
    std::printf("    fired %d: lost %d, decoyed %d, moved %d, missed %d, hit %d\n", r->Arms().fired, r->Arms().lost, r->Arms().decoyed,
                r->Arms().moved, r->Arms().missed, r->Arms().hit);
    SC_EXPECT(r->Arms().fired == 4);  // two hunters, two each
    SC_EXPECT(Count(d, "fires an anti-radiation missile (arm-") == 4);
    SC_EXPECT(r->Arms().lost + r->Arms().decoyed + r->Arms().moved + r->Arms().missed + r->Arms().hit == 4);
    // The crew never takes an ARM as a target, nor does the label of its launcher's echo pass to it.
    SC_EXPECT(!Has(d, "took arm-") && !Has(d, "SSC              event      arm-") && !Has(d, "guided at arm-"));
    // Out of their reach: nothing fired.
    auto far = Runner(ReplaceAll(HuntersRadiating(), "\"range_m\": 24000", "\"range_m\": 5000"));
    SC_EXPECT(far != nullptr);
    if (far == nullptr) return;
    far->RunToEnd(0.1, 900.0);
    SC_EXPECT(far->Arms().fired == 0);
}

SC_TEST("arm: the senior operator calls an echo closing faster than any aircraft; both transmitters go off and stay off a while") {
    auto r = Runner(HuntersRadiating());
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    double called_s = -1.0, silent_s = -1.0, back_s = -1.0;
    while (!r->Finished() && r->Time() < 300.0) {
        r->Step(0.1);
        const std::string d = r->Debrief();
        if (called_s < 0.0 && Has(d, "Пуск ПРР!")) called_s = r->Time();
        const bool radiating = r->Console().equipment.IsReady("soc_transmitter") || r->Console().equipment.IsReady("ssc_transmitter");
        if (called_s >= 0.0 && silent_s < 0.0 && !radiating) silent_s = r->Time();
        if (silent_s >= 0.0 && back_s < 0.0 && r->Console().equipment.IsReady("soc_transmitter")) back_s = r->Time();
    }
    std::printf("    called at %.0f s, silent at %.0f s, the search radar back at %.0f s\n", called_s, silent_s, back_s);
    SC_EXPECT(called_s > 0.0 && silent_s > 0.0 && silent_s - called_s < 8.0);  // his words, the commander's, two switches
    SC_EXPECT(back_s - silent_s > 35.0);                                        // arm.quiet_s 40 from the order
    const std::string d = r->Debrief();
    SC_EXPECT(Has(d, "ПРР! Излучение выключить!") && Has(d, "Излучение включить!"));
    SC_EXPECT(Count(d, "Излучение выключить!") == 1);  // the ARM order only, no second EMCON order on top
}

SC_TEST("arm: silent at its arrival an ARM with no memory loses the radar, or a decoy emitter that came on draws it") {
    auto r = Runner(HuntersRadiating());
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    r->RunToEnd(0.1, 300.0);
    const std::string d = r->Debrief();
    SC_EXPECT(Has(d, "arm-1  ARM: decoyed - it dived onto a decoy emitter, which is destroyed"));
    SC_EXPECT(Has(d, "arm-2  ARM: lost it - the radar was silent when it arrived (no memory)"));
}

SC_TEST("arm: one that remembers the radar comes on past a short silence and is lost only past its memory") {
    auto r = Runner(Scenario("osa_sead_hunters"));  // with the search radar in bursts
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    r->RunToEnd(0.1, 450.0);
    SC_EXPECT(Has(r->Debrief(), "ARM: lost it - the radar went silent 9 s before it arrived, past its 8 s memory"));
    // The same ARM with a longer memory still comes, and with a sure hit it gets the vehicle: the scenario ends.
    auto longer = Runner(ReplaceAll(Scenario("osa_sead_hunters"), "\"memory_s\": 8", "\"memory_s\": 30"),
                         [](ScenarioDef& def) { def.tuning.arm_hit_chance = 1.0; });
    SC_EXPECT(longer != nullptr);
    if (longer == nullptr) return;
    longer->RunToEnd(0.1, 900.0);
    std::printf("    with 30 s of memory: %s\n", longer->EndReason().c_str());
    SC_EXPECT(longer->UnitHitByArm() && longer->Finished() && Has(longer->EndReason(), "anti-radiation missile"));
}

SC_TEST("arm: a crew that doesn't know the drill keeps radiating, and a sure ARM gets its vehicle") {
    auto r = Runner(ReplaceOnce(HuntersRadiating(), "\"crew_commander\": \"npc\",", "\"doctrine\": { \"arm_drill\": false },\n  \"crew_commander\": \"npc\","),
                    [](ScenarioDef& def) { def.tuning.arm_hit_chance = 1.0; });
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    r->RunToEnd(0.1, 900.0);
    const std::string d = r->Debrief();
    SC_EXPECT(!Has(d, "ПРР! Излучение выключить!"));
    SC_EXPECT(r->UnitHitByArm() && Has(d, "arm-1  ARM: hit"));
}

SC_TEST("arm: the post's observers call launches on the battery net; the barraged net loses the calls") {
    auto r = Runner(Scenario("osa_sead_scoot"));
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    r->RunToEnd(0.1, 620.0);
    const std::string d = r->Debrief();
    SC_EXPECT(Has(d, "Всем! Пуск ПРР"));  // before the barrage (420 s)
    SC_EXPECT(Has(d, "arm-1  ARM: decoyed") && Has(d, "arm-2  ARM: decoyed"));  // warned early, the radar off long enough
    SC_EXPECT(Has(d, "ARM: lost it") || Has(d, "the post's ARM warning didn't get through"));
    SC_EXPECT(Has(d, "the post's ARM warning didn't get through on the battery net"));  // under the barrage
}

SC_TEST("shoot and scoot: after the fight the crew stows, the vehicle drives to its alternate position and comes to readiness") {
    auto r = Runner(Scenario("osa_sead_scoot"));
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    const Vec3 start = r->Def().unit_position;
    bool stowed_seen = false, moving_seen = false;
    double ready_again_s = -1.0;
    while (!r->Finished() && r->Time() < 620.0) {
        r->Step(0.1);
        if (r->Relocating() && !r->Console().equipment.IsReady("soc_transmitter") && !r->Console().equipment.IsReady("apu_free")) stowed_seen = true;
        if (!r->Console().equipment.GetInput("vehicle_stopped")) moving_seen = true;
        if (moving_seen && !r->Relocating() && ready_again_s < 0.0 && r->Console().equipment.IsReady("soc_transmitter")) ready_again_s = r->Time();
    }
    const double moved = (r->Def().unit_position - start).HorizontalLength();
    std::printf("    moved %.0f m; the search radar on again at %.0f s\n", moved, ready_again_s);
    SC_EXPECT(stowed_seen && moving_seen);
    SC_EXPECT(std::fabs(moved - 1140.0) < 30.0);  // to the first alternate position
    SC_EXPECT(ready_again_s > 0.0);
    const std::string d = r->Debrief();
    SC_EXPECT(Has(d, "Смена позиции!") && Has(d, "На запасной позиции!"));
}

SC_TEST("shoot and scoot: an ARM flying at where the vehicle radiated, silent but within its memory, misses a vehicle that moved") {
    // A slow ARM with a long memory, a sure hit had the vehicle stayed: the crew goes silent and moves at once.
    std::string text = Scenario("osa_sead_scoot");
    text = ReplaceOnce(text, "\"relocate\": \"after_launch\"", "\"relocate\": \"after_arm\"");
    text = ReplaceOnce(text, "\"arms\": { \"count\": 2, \"range_m\": 24000, \"speed_mps\": 600, \"memory_s\": 20,",
                       "\"arms\": { \"count\": 1, \"range_m\": 24000, \"speed_mps\": 120, \"memory_s\": 400,");
    auto r = Runner(text, [](ScenarioDef& def) {
        def.tuning.arm_hit_chance = 1.0;
        def.em.dummies.clear();  // no decoy to draw it
    });
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    r->RunToEnd(0.1, 570.0);  // before the second hunter's ARMs (still sure hits here) arrive
    const std::string d = r->Debrief();
    if (!Has(d, "the vehicle had moved")) std::printf("%s\n", d.c_str());
    SC_EXPECT(Has(d, "arm-1  ARM: missed - the vehicle had moved"));
    SC_EXPECT(!r->UnitHitByArm());
}

SC_TEST("battery: a relocation order sends one vehicle along its route to its new position; it sets up there") {
    BatteryTactics tactics;
    std::string error;
    SC_EXPECT(LoadBatteryTacticsFile(kData + "/command/battery_tactics.json", tactics, &error));
    std::vector<BatteryVehicleDef> vehicles(2);
    vehicles[0].id = vehicles[0].callsign = "21";
    vehicles[0].own = true;
    vehicles[1].id = vehicles[1].callsign = "22";
    vehicles[1].start = Vec3{200.0, 0.0, 0.0};
    Battery b(tactics, vehicles);
    b.SetDriveOwn(true);
    BatteryOrder o;
    o.kind = BatteryOrder::Kind::Relocate;
    o.vehicle = "22";
    o.route = {Vec3{200.0, -830.0, 0.0}};
    double t = 0.0;
    b.Order(o, t);
    SC_EXPECT(b.Relocating("22") && !b.Relocating(""));
    for (; t < 100.0; t += 0.5) b.Step(t, 0.5);  // 830 m at the march speed (8.3 m/s): 100 s
    SC_EXPECT(std::fabs(b.Find("22")->position.y + 830.0) < 20.0);
    SC_EXPECT(b.Relocating("22") && !b.Find("22")->deployed);  // setting up: the battery's deploy time
    for (; t < 100.0 + tactics.deploy_s + 1.0; t += 0.5) b.Step(t, 0.5);
    SC_EXPECT(!b.Relocating("22") && b.Find("22")->deployed);
    SC_EXPECT(b.Find("21")->position.Length() < 1e-6);  // the other one stayed
}

SC_TEST("arm: the loader checks the arms, the alternate positions, the doctrine and the relocation order") {
    const std::string text = Scenario("osa_sead_scoot");
    std::string error;
    SC_EXPECT(Runner(ReplaceOnce(text, "\"count\": 2, \"range_m\": 24000, \"speed_mps\": 600", "\"count\": 0, \"range_m\": 24000, \"speed_mps\": 600"), nullptr, &error) == nullptr);
    SC_EXPECT(Has(error, "arms.count"));
    SC_EXPECT(Runner(ReplaceOnce(text, "\"memory_s\": 20, \"notes\": \"Remembers", "\"memory_s\": -1, \"notes\": \"Remembers"), nullptr, &error) == nullptr);
    SC_EXPECT(Has(error, "memory_s"));
    SC_EXPECT(Runner(ReplaceOnce(text, "\"relocate\": \"after_launch\"", "\"relocate\": \"sometimes\""), nullptr, &error) == nullptr);
    SC_EXPECT(Has(error, "doctrine.relocate"));
    SC_EXPECT(Runner(ReplaceOnce(text, "\"drive_own\": true", "\"drive_own\": false"), nullptr, &error) == nullptr);
    SC_EXPECT(Has(error, "own_unit.alternate_positions") && Has(error, "drive_own"));
    SC_EXPECT(Runner(ReplaceOnce(ReplaceOnce(text, "\"relocate\": \"after_launch\"", "\"relocate\": \"after_arm\""),
                                 "\"alternate_positions\": [ { \"east_m\": -900, \"north_m\": -700, \"up_m\": 3 }, { \"east_m\": 400, \"north_m\": -1300, \"up_m\": 3 } ],", ""),
                     nullptr, &error) == nullptr);
    SC_EXPECT(Has(error, "doctrine.relocate") && Has(error, "alternate_positions"));
    SC_EXPECT(Runner(ReplaceOnce(text, "\"triggers\": []", "\"triggers\": [ { \"id\": \"go\", \"when\": { \"after_s\": 10 }, \"do\": [ { \"battery_order\": \"relocate\" } ] } ]"),
                     nullptr, &error) == nullptr);
    SC_EXPECT(Has(error, "points"));
    SC_EXPECT(Runner(ReplaceOnce(text, "\"triggers\": []", "\"triggers\": [ { \"id\": \"go\", \"when\": { \"after_s\": 10 }, \"do\": [ { \"battery_order\": \"relocate\", \"vehicle\": \"22\", \"points\": [ { \"east_m\": 0, \"north_m\": 900, \"up_m\": 3 } ] } ] } ]")) != nullptr);
}
