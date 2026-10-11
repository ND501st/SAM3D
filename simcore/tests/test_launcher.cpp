// The missile load and the two guidance channels (roadmap 6.3): selection, the launch interval, the per-channel lamp
// sequence and the console's "launcher" block.
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/launcher.hpp"
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

ConsoleData Osa() {
    ConsoleData c;
    std::string error;
    if (!LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error)) std::printf("    console: %s\n", error.c_str());
    return c;
}

// The lamps of a channel (1 = I, 2 = II) as one string, in the NVA's order: ВКЛ ВЫС, СХОД, ЗАХВАТ, СР.Л, УЗКИЙ ЛУЧ, К3.
std::string Lamps(const ConsoleData& c, int ch) {
    const std::string p = "ch" + std::to_string(ch) + "_";
    std::string out;
    for (const char* l : {"vkl_vys", "skhod", "zakhvat", "sr_l", "uzkiy_luch", "k3"}) {
        out += c.equipment.GetInput(p + l) ? "1" : "0";
    }
    return out;
}

}  // namespace

SC_TEST("launcher: the Osa's launcher block loads; bad ones are refused with paths") {
    const ConsoleData c = Osa();
    SC_EXPECT(c.has_launcher && c.launcher.containers.size() == 6 && c.launcher.channels.size() == 2);
    SC_EXPECT(c.launcher.channel_positions.size() == 3 && c.launcher.channel_positions[2] == std::vector<int>({1, 2}));
    SC_EXPECT_NEAR(c.launcher.capture_s, 0.8, 1e-12);
    SC_EXPECT_NEAR(c.launcher.fuse_range_m, 120.0, 1e-12);
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
    SC_EXPECT(bad("\"channel_positions\": [ [2], [1], [1, 2] ]", "\"channel_positions\": [ [2], [1] ]",
                  "launcher.channel_positions: needs one entry per position of msl.channels"));
    SC_EXPECT(bad("{ \"id\": \"2-I\", \"channel\": 1,", "{ \"id\": \"2-I\", \"channel\": 3,", "launcher.containers[1].channel"));
    SC_EXPECT(bad("{ \"id\": \"2-I\",", "{ \"id\": \"3-I\",", "duplicate container '3-I'"));
    SC_EXPECT(bad("\"medium_s\": 2.0", "\"medium_s\": 0.5", "capture_s <= medium_s <= narrow_s"));
    SC_EXPECT(bad("\"captured\": \"ch1_zakhvat\"", "\"captured\": \"ch1_nothing\"", "launcher.channels[0].captured"));
}

SC_TEST("launcher: at ВНИМАНИЕ the outer missiles are picked, one per channel, and the transmitters come on") {
    ConsoleData c = Osa();
    Launcher l(c.launcher);
    l.Step(0.0, c);
    SC_EXPECT(l.Selected(1).empty() && l.Selected(2).empty() && Lamps(c, 1) == "000000");
    for (const char* a : {"ammo_1_i", "ammo_2_i", "ammo_3_i", "ammo_1_ii", "ammo_2_ii", "ammo_3_ii"}) SC_EXPECT(c.equipment.GetInput(a));
    c.equipment.SetInput("target_near_zone", true);
    l.Step(0.1, c);
    SC_EXPECT(l.Selected(1) == "3-I" && l.Selected(2) == "3-II");
    SC_EXPECT(Lamps(c, 1) == "100000" && Lamps(c, 2) == "100000");  // ВКЛ ВЫС on both channels
    SC_EXPECT(!c.equipment.GetInput("launch_hold"));                // channel I could take a launch now
}

SC_TEST("launcher: КАНАЛЫ decides which channels take missiles, and channel I fires first in СОВМ.") {
    for (int pos : {0, 1, 2}) {
        ConsoleData c = Osa();
        c.panel.Set("msl.channels", pos);  // КАН. II, КАН. I, СОВМ.
        Launcher l(c.launcher);
        c.equipment.SetInput("target_near_zone", true);
        l.Step(0.0, c);
        SC_EXPECT(l.Selected(1).empty() == (pos == 0));
        SC_EXPECT(l.Selected(2).empty() == (pos == 1));
        SC_EXPECT(l.NextChannel(10.0, c) == (pos == 0 ? 2 : 1));
    }
}

SC_TEST("launcher: a missile's lamps follow it through the beams in the NVA's order, and К3 lights near the target") {
    ConsoleData c = Osa();
    Launcher l(c.launcher);
    c.equipment.SetInput("target_near_zone", true);
    l.Step(0.0, c);
    // 10 s of flight at 450 m/s: К3 at 120 m to go, 0.27 s before the end.
    const Launcher::Shot s = l.Launch(1.0, c, 1, 10.0, 450.0);
    SC_EXPECT(s.channel == 1 && s.container == "3-I");
    std::vector<std::string> seen;
    for (double t = 1.0; t < 11.5; t += 0.05) {
        l.Step(t, c);
        if (seen.empty() || seen.back() != Lamps(c, 1)) seen.push_back(Lamps(c, 1));
    }
    // ВКЛ ВЫС stays lit while the channel guides; then СХОД, ЗАХВАТ (0.8 s), СР.Л (2 s), УЗКИЙ ЛУЧ (4 s), К3; all out
    // when the shot is over; then, ВНИМАНИЕ still lit, the channel picks its next missile and the transmitter is on.
    const std::vector<std::string> want = {"110000", "111000", "111100", "111110", "111111", "000000", "100000"};
    SC_EXPECT(seen == want);
    if (seen != want) {
        for (const std::string& x : seen) std::printf("    %s\n", x.c_str());
    }
    SC_EXPECT(!c.equipment.GetInput("ammo_3_i") && c.equipment.GetInput("ammo_2_i"));
    SC_EXPECT(l.MissilesLeft() == 5);
}

SC_TEST("launcher: against a jamming target (ПОМЕХА) К3 comes with the medium beam, method Н (1991 textbook p23-24)") {
    ConsoleData c = Osa();
    c.panel.Set("fo.pomekha", 1);
    Launcher l(c.launcher);
    c.equipment.SetInput("target_near_zone", true);
    l.Step(0.0, c);
    l.Launch(0.0, c, 1, 20.0, 450.0);
    l.Step(1.9, c);
    SC_EXPECT(Lamps(c, 1) == "111000");  // captured, not yet the medium beam
    l.Step(2.1, c);
    SC_EXPECT(Lamps(c, 1) == "111101");  // СР.Л and К3 together: 18 s and 8 km to go
}

SC_TEST("launcher: launches are 4 s apart, a busy channel takes no missile, and six missiles is all there is") {
    ConsoleData c = Osa();
    Launcher l(c.launcher);
    c.equipment.SetInput("target_near_zone", true);
    l.Step(0.0, c);
    SC_EXPECT(l.Launch(0.0, c, 1, 30.0, 450.0).channel == 1);
    SC_EXPECT(l.WhyNot(2.0, c) == "too soon after the last launch");
    SC_EXPECT(!l.PairSpent());
    SC_EXPECT(l.Launch(4.0, c, 2, 30.0, 450.0).channel == 2);
    SC_EXPECT(l.PairSpent());
    l.Step(5.0, c);  // both channels guide: the next pair waits for a free channel
    SC_EXPECT(l.Selected(1).empty() && l.Selected(2).empty());
    SC_EXPECT(l.WhyNot(10.0, c) == "no missile selected on a free channel");
    l.ShotDone(1);  // the outcome model resolved shot 1
    l.Step(6.0, c);
    SC_EXPECT(l.Selected(1) == "2-I" && l.Selected(2).empty());
    int n = 2;
    for (double t = 10.0; t < 200.0 && l.MissilesLeft() > 0; t += 1.0) {
        l.Step(t, c);
        if (l.NextChannel(t, c) != 0) {
            l.Launch(t, c, ++n, 2.0, 450.0);
        }
        for (int done = 1; done <= n; ++done) l.ShotDone(done);
    }
    SC_EXPECT(n == 6 && l.MissilesLeft() == 0);
    l.Step(300.0, c);
    SC_EXPECT(l.WhyNot(300.0, c) == "no missile left");
    for (const char* a : {"ammo_1_i", "ammo_2_i", "ammo_3_i", "ammo_1_ii", "ammo_2_ii", "ammo_3_ii"}) SC_EXPECT(!c.equipment.GetInput(a));
}

SC_TEST("launcher: a unit with empty containers takes the next loaded one") {
    // (Preparation started by hand with БОРТ СПК, with no radar range, also picks missiles: the TV-only scenarios in
    // test_engagement_model.cpp launch only because of it.)
    ConsoleData c = Osa();
    Launcher partial(c.launcher);
    partial.Unload("3-I");
    partial.Unload("2-I");
    c.equipment.SetInput("target_near_zone", true);
    partial.Step(0.0, c);
    SC_EXPECT(partial.Selected(1) == "1-I" && partial.Selected(2) == "3-II" && partial.MissilesLeft() == 4);
    SC_EXPECT(!c.equipment.GetInput("ammo_3_i") && c.equipment.GetInput("ammo_1_i"));
}

SC_TEST("launcher: the TZM reloads six missiles in 6 min, one container per minute, only at a halt with the pair spent") {
    ConsoleData c = Osa();
    SC_EXPECT_NEAR(c.launcher.reload_s_per_missile, 60.0, 1e-12);
    Launcher l(c.launcher);
    for (const char* id : {"3-I", "2-I", "1-I", "3-II", "2-II", "1-II"}) l.Unload(id);
    SC_EXPECT(l.MissilesLeft() == 0);
    double t = 0.0;
    auto run = [&](double seconds) {
        for (double end = t + seconds; t < end - 1e-9; t += 0.1) l.Step(t, c);
    };
    // The TZM isn't there: nothing loads.
    c.equipment.SetInput("vehicle_stopped", true);
    run(120.0);
    SC_EXPECT(l.MissilesLeft() == 0 && l.Reloading() < 0);
    // It arrives, but the vehicle is moving: nothing loads.
    c.equipment.SetInput("tzm_alongside", true);
    c.equipment.SetInput("vehicle_stopped", false);
    run(120.0);
    SC_EXPECT(l.MissilesLeft() == 0);
    // At a halt: one a minute, outer containers first (firing order).
    c.equipment.SetInput("vehicle_stopped", true);
    run(30.0);
    SC_EXPECT(l.MissilesLeft() == 0 && l.Reloading() >= 0);
    run(31.0);
    SC_EXPECT(l.MissilesLeft() == 1 && l.Loaded("3-I"));
    run(60.0);
    SC_EXPECT(l.MissilesLeft() == 2 && l.Loaded("3-II"));
    run(240.0 + 1.0);
    SC_EXPECT(l.MissilesLeft() == 6);
    SC_EXPECT(l.Reloading() < 0);
    // Six missiles, 6 min from the first minute at the halt: flak11.
    SC_EXPECT(t > 120.0 + 120.0 + 360.0 - 5.0);
}

SC_TEST("launcher: the crane waits while a pair is selected or a channel is guiding") {
    ConsoleData c = Osa();
    Launcher l(c.launcher);
    l.Unload("1-I");
    l.Unload("1-II");
    c.equipment.SetInput("tzm_alongside", true);
    c.equipment.SetInput("vehicle_stopped", true);
    c.equipment.SetInput("target_near_zone", true);  // ВНИМАНИЕ: a pair is selected
    double t = 0.0;
    for (; t < 200.0; t += 0.1) l.Step(t, c);
    SC_EXPECT(l.MissilesLeft() == 4 && !l.Loaded("1-I"));
    l.Launch(t, c, 1, 20.0, 450.0);
    l.Launch(t + 4.0, c, 2, 20.0, 450.0);
    c.equipment.SetInput("target_near_zone", false);
    for (; t < 200.0 + 24.0; t += 0.1) l.Step(t, c);  // both flying
    SC_EXPECT(l.MissilesLeft() == 2 && !l.Loaded("1-I"));
    for (double end = t + 70.0; t < end; t += 0.1) l.Step(t, c);  // flights over, pair spent: the crane gets to work
    SC_EXPECT(l.MissilesLeft() == 3);
}

SC_TEST("launcher: СМЕНА ЛИТ. blanks ЗАХВ. АПЧ for 1 s on both channels and holds the launch until they relock") {
    ConsoleData c = Osa();
    Launcher l(c.launcher);
    c.equipment.SetInput("target_near_zone", true);
    l.Step(0.0, c);
    SC_EXPECT(c.equipment.GetInput("ch1_apch_lock") && c.equipment.GetInput("ch2_apch_lock"));
    SC_EXPECT(!c.equipment.GetInput("launch_hold"));
    c.panel.Set("spk.letter", 1);  // the latch opens: НОРМА, no change yet
    l.Step(1.0, c);
    SC_EXPECT(l.LetterChanges() == 0 && c.equipment.GetInput("ch1_apch_lock"));
    c.panel.Set("spk.letter", 2);  // СМЕНА ЛИТ.
    l.Step(2.0, c);
    SC_EXPECT(l.LetterChanges() == 1 && l.Relocking(2.5));
    SC_EXPECT(!c.equipment.GetInput("ch1_apch_lock") && !c.equipment.GetInput("ch2_apch_lock"));
    SC_EXPECT(c.equipment.GetInput("launch_hold") && l.NextChannel(2.5, c) == 0);
    SC_EXPECT(l.WhyNot(2.5, c).find("relocking") != std::string::npos);
    l.Step(3.1, c);  // back within 1 s
    SC_EXPECT(c.equipment.GetInput("ch1_apch_lock") && c.equipment.GetInput("ch2_apch_lock") && !c.equipment.GetInput("launch_hold"));
    l.Step(4.0, c);  // staying on СМЕНА ЛИТ. is not a second change
    SC_EXPECT(l.LetterChanges() == 1);
    c.panel.Set("spk.letter", 1);
    l.Step(5.0, c);
    c.panel.Set("spk.letter", 2);
    l.Step(6.0, c);
    SC_EXPECT(l.LetterChanges() == 2);
}

SC_TEST("launcher: the lock lamp only lights for a channel whose transmitter is on") {
    ConsoleData c = Osa();
    Launcher l(c.launcher);
    l.Step(0.0, c);
    SC_EXPECT(!c.equipment.GetInput("ch1_vkl_vys") && !c.equipment.GetInput("ch1_apch_lock"));
    c.equipment.SetInput("target_near_zone", true);
    l.Step(1.0, c);
    SC_EXPECT(c.equipment.GetInput("ch1_vkl_vys") && c.equipment.GetInput("ch1_apch_lock"));
}

SC_TEST("launcher: the emergency launch throws the ИЗДЕЛИЯ missile off unguided, for 25.5 s, using no channel") {
    ConsoleData c = Osa();
    Launcher l(c.launcher);
    l.Step(0.0, c);
    SC_EXPECT(l.EmergencySelected(c).empty() && !c.equipment.GetInput("emerg_ready"));  // ОТКЛ.
    SC_EXPECT(l.EmergencyLaunch(1.0, c).empty() && l.MissilesLeft() == 6);
    c.panel.Set("msl.emergency_select", 6);  // 3-I
    l.Step(2.0, c);
    SC_EXPECT(l.EmergencySelected(c) == "3-I" && c.equipment.GetInput("emerg_ready"));
    SC_EXPECT(l.EmergencyLaunch(3.0, c) == "3-I");
    l.Step(3.0, c);
    SC_EXPECT(l.MissilesLeft() == 5 && !l.Loaded("3-I") && !c.equipment.GetInput("ammo_3_i"));
    SC_EXPECT(l.EmergencyThrown() == 1 && l.EmergencyInFlight(20.0) == 1 && l.EmergencyInFlight(28.6) == 0);
    SC_EXPECT(!c.equipment.GetInput("ch1_skhod") && !c.equipment.GetInput("ch2_skhod"));  // no channel is busy
    SC_EXPECT(!c.equipment.GetInput("emerg_ready"));  // the container is empty now
    SC_EXPECT(l.EmergencyLaunch(4.0, c).empty());     // nothing left to throw from it
}

SC_TEST("launcher: an emergency launch of a selected missile leaves the channel to pick another at the next ВНИМАНИЕ") {
    ConsoleData c = Osa();
    Launcher l(c.launcher);
    c.equipment.SetInput("target_near_zone", true);
    l.Step(0.0, c);
    SC_EXPECT(l.Selected(1) == "3-I");
    c.panel.Set("msl.emergency_select", 6);
    SC_EXPECT(l.EmergencyLaunch(1.0, c) == "3-I");
    SC_EXPECT(l.Selected(1).empty() && !l.PairSpent());  // channel II still has its missile
    l.Step(1.1, c);
    SC_EXPECT(l.Selected(1) == "2-I");                   // ВНИМАНИЕ still on: the next outer missile
}

SC_TEST("launcher: the letter and emergency blocks are refused with paths when they are wrong") {
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
    SC_EXPECT(bad("\"change_position\": 2", "\"change_position\": 5", "launcher.letter.change_position"));
    SC_EXPECT(bad("\"lock_inputs\": [ \"ch1_apch_lock\", \"ch2_apch_lock\" ]", "\"lock_inputs\": [ \"ch1_apch_lock\" ]", "launcher.letter.lock_inputs"));
    SC_EXPECT(bad("\"3-II\", \"2-II\", \"1-II\", \"\"", "\"3-II\", \"2-II\", \"9-II\", \"\"", "launcher.emergency.select_containers[2]"));
    SC_EXPECT(bad("\"fire_control\": \"msl.emergency\"", "\"fire_control\": \"msl.channels\"", "launcher.emergency.fire_control"));
    SC_EXPECT(bad("\"flight_s\": 25.5", "\"flight_s\": 0", "launcher.emergency.flight_s"));
}

SC_TEST("launcher: Р I — ЦЕЛЬ — Р II picks the channel whose missile the range indicator follows") {
    ConsoleData c = Osa();
    Launcher l(c.launcher);
    c.equipment.SetInput("target_near_zone", true);
    l.Step(0.0, c);
    SC_EXPECT(c.panel.Get("ssc.target_select") == 1 && l.RangeSweepChannel(c) == 0);  // ЦЕЛЬ
    c.panel.Set("ssc.target_select", 0);
    SC_EXPECT(l.RangeSweepChannel(c) == 1);
    c.panel.Set("ssc.target_select", 2);
    SC_EXPECT(l.RangeSweepChannel(c) == 2);
    SC_EXPECT(l.MissileRangeM(1, 0.0) < 0.0);  // nothing in flight yet
    l.Launch(10.0, c, 1, 20.0, 450.0);         // channel I, a 20 s flight at 450 m/s
    SC_EXPECT_NEAR(l.MissileRangeM(1, 10.0), 0.0, 1e-9);
    SC_EXPECT_NEAR(l.MissileRangeM(1, 14.0), 1800.0, 1e-9);
    SC_EXPECT_NEAR(l.MissileRangeM(1, 100.0), 9000.0, 1e-9);  // the flight ends at the target
    SC_EXPECT(l.MissileRangeM(2, 14.0) < 0.0);
    l.ShotDone(1);
    SC_EXPECT(l.MissileRangeM(1, 14.0) < 0.0);
    l.Launch(20.0, c, 2, 0.0, 0.0);  // no outcome model: the flight has no speed
    SC_EXPECT(l.MissileRangeM(1, 21.0) < 0.0 && l.MissileRangeM(2, 21.0) < 0.0);
}

// ---- Prepared missiles are held at most 10 min (voenizdat_osa_1991 p248): the gyro overheats, the pair is prepared again.

#include <memory>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

namespace {

std::unique_ptr<ScenarioRunner> HeldPair() {
    const std::string text = R"({
      "scenario_id": "h", "name": "H", "source": "sam3d", "confidence": "estimate", "seed": 1,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 }, "terrain": { "type": "flat", "height_m": 0 },
      "own_unit": { "console": "procedures/osa_ak.json", "readiness": "deployed",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "triggers": [ { "id": "near", "when": {}, "do": [ { "set_input": "target_near_zone", "value": true }, { "set_input": "target_in_zone", "value": true } ] } ],
      "end": { "time_limit_s": 100000, "when_targets_gone": false } })";
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

void RunFor(ScenarioRunner& r, double seconds) {
    for (double end = r.Time() + seconds; r.Time() < end - 1e-9;) r.Step(1.0);
}

}  // namespace

SC_TEST("launcher: a pair held prepared for 10 min overheats, the next pair is prepared, the hot ones flash and rest") {
    auto r = HeldPair();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Launcher& l = *r->GetLauncher();
    const Equipment& eq = r->Console().equipment;
    RunFor(*r, 30.0);
    SC_EXPECT(eq.IsReady("missile_prep") && l.Selected(1) == "3-I" && l.Selected(2) == "3-II");
    SC_EXPECT(!eq.GetInput("hot_3_i") && l.Overheats() == 0);
    RunFor(*r, 560.0);  // 590 s: still holding
    SC_EXPECT(l.Selected(1) == "3-I" && l.Overheats() == 0);
    RunFor(*r, 40.0);   // past 10 min of being prepared (it was ready at about 13 s)
    SC_EXPECT(l.Overheats() == 2 && l.Overheated("3-I") && l.Overheated("3-II"));
    SC_EXPECT(eq.GetInput("hot_3_i") && eq.GetInput("hot_3_ii") && eq.GetInput("ammo_3_i"));  // flashes, still in its container
    SC_EXPECT(l.Selected(1) == "2-I" && l.Selected(2) == "2-II");                              // the next pair
    SC_EXPECT(l.MissilesLeft() == 6);
    RunFor(*r, 20.0);
    SC_EXPECT(eq.IsReady("missile_prep"));  // the second pair is prepared
    // After the first rest (10 min from the overheat) the hot missiles are usable again.
    RunFor(*r, 600.0);
    SC_EXPECT(!l.Overheated("3-I") && !eq.GetInput("hot_3_i"));
}

SC_TEST("launcher: after three holds a missile rests for an hour") {
    auto r = HeldPair();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    Launcher& l = *r->GetLauncher();
    // Keep the pair in the outer containers: take the other missiles away, so 3-I and 3-II are the only choice.
    for (const char* c : {"2-I", "1-I", "2-II", "1-II"}) l.Unload(c);
    RunFor(*r, 30.0);
    SC_EXPECT(l.Selected(1) == "3-I");
    for (int hold = 1; hold <= 3; ++hold) {
        RunFor(*r, 640.0);  // 600 s prepared (plus the 13 s preparation) and over
        SC_EXPECT(l.Overheated("3-I"));
        if (hold < 3) {
            RunFor(*r, 620.0);  // the 10 min rest
            SC_EXPECT(!l.Overheated("3-I"));
            RunFor(*r, 30.0);
            SC_EXPECT(l.Selected(1) == "3-I");  // prepared again
        }
    }
    RunFor(*r, 3000.0);  // the long rest: still hot after 50 min
    SC_EXPECT(l.Overheated("3-I"));
    RunFor(*r, 700.0);
    SC_EXPECT(!l.Overheated("3-I"));
}

SC_TEST("launcher: the prep_hold block and the container hot inputs are checked") {
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
    SC_EXPECT(bad("\"hold_s\": 600", "\"hold_s\": 0", "launcher.prep_hold.hold_s"));
    SC_EXPECT(bad("\"cycles\": 3", "\"cycles\": 0", "launcher.prep_hold.cycles"));
    SC_EXPECT(bad("\"long_rest_s\": 3600", "\"long_rest_s\": 60", "launcher.prep_hold.long_rest_s"));
    SC_EXPECT(bad("\"hot_input\": \"hot_3_i\"", "\"hot_input\": \"hot_nothing\"", "launcher.containers[0].hot_input"));
}
