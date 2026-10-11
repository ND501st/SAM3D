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

SC_TEST("launcher: against a jamming target (ПОМЕХА) К3 comes with the narrow beam") {
    ConsoleData c = Osa();
    c.panel.Set("fo.pomekha", 1);
    Launcher l(c.launcher);
    c.equipment.SetInput("target_near_zone", true);
    l.Step(0.0, c);
    l.Launch(0.0, c, 1, 20.0, 450.0);
    l.Step(3.9, c);
    SC_EXPECT(Lamps(c, 1) == "111100");
    l.Step(4.1, c);
    SC_EXPECT(Lamps(c, 1) == "111111");  // 16 s and 7 km to go: К3 already
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
