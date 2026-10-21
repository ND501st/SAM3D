// The PU-12M's information chain when the player's vehicle is the post (simcore/command_post.hpp, docs/PU12_DEVELOPMENT.md
// P3): the regiment's telecode onto the IT-45, a cabled radar's semi-automatic tracks, the plotter's marks on the board,
// and the post's navigation error. Behaviour tests use generic placeholder numbers; one test loads the real data file.
#include <cmath>
#include <string>
#include <vector>

#include "simcore/command_post.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

// Placeholder numbers, round for hand calculation.
AspdDef PlaceholderAspd() {
    AspdDef d;
    d.max_targets = 100;
    d.semi_auto_tracks = 3;
    d.take_off_s = 2.0;
    d.height_step_m = 200;
    d.speed_step_mps = 10;
    d.max_speed_mps = 500;
    d.zone1_top_m = 1000;
    d.zone2_top_m = 3000;
    d.telecode_period_s = 10;
    d.telecode_range_m = 15000;
    d.radar_period_s = 10;
    d.plot_mark_s = 5;
    d.oriented_az_error_deg = 0.5;
    d.oriented_position_error_m = 100;
    d.drift_fraction = 0.01;
    d.unoriented_az_error_deg = 5;
    d.unoriented_position_error_m = 3000;
    return d;
}

PlottingBoardDef PlaceholderBoard() {
    PlottingBoardDef b;
    b.radar_battery_number = 62;
    b.max_range_m = 200000;
    return b;
}

BoardContact Target(const std::string& id, double east, double north, double alt, int type = 9) {
    BoardContact c;
    c.target_id = id;
    c.position = Vec3{east, north, alt};
    c.altitude_m = alt;
    c.height_agl_m = alt;
    c.type = type;
    return c;
}

// The post, oriented with no error at all (so the positions are the truth's).
CommandPost Exact(const AspdDef& d = PlaceholderAspd()) {
    AspdDef e = d;
    e.oriented_az_error_deg = 0.0;
    e.oriented_position_error_m = 0.0;
    e.drift_fraction = 0.0;
    CommandPost p(e, PlaceholderBoard(), Vec3{}, 1);
    p.Oriented();
    return p;
}

}  // namespace

SC_TEST("command post: the ASPD-U data file loads, every value cited") {
    AspdDef d;
    std::string error;
    SC_EXPECT(LoadAspdFile(std::string(SIMCORE_DATA_DIR) + "/command/aspd_u.json", d, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(d.id == "aspd_u" && d.scales_m.size() == 3);
    SC_EXPECT_NEAR(d.telecode_range_m, 15000.0, 1e-9);  // the R-407's reach (flak11)
    SC_EXPECT_NEAR(d.drift_fraction, 0.01, 1e-12);      // the KP-4's 1 % (flak11)
}

SC_TEST("command post: heights in steps and zones, speeds in steps, as the ASPD-U carries them") {
    const CommandPost p = Exact();
    SC_EXPECT_NEAR(p.QuantiseHeight(1290.0), 1200.0, 1e-9);
    SC_EXPECT_NEAR(p.QuantiseHeight(1310.0), 1400.0, 1e-9);
    SC_EXPECT(p.ZoneOf(900.0) == 1 && p.ZoneOf(1000.0) == 1 && p.ZoneOf(1200.0) == 2 && p.ZoneOf(3000.0) == 2 && p.ZoneOf(3200.0) == 3);
    SC_EXPECT_NEAR(p.QuantiseSpeed(254.0), 250.0, 1e-9);
    SC_EXPECT_NEAR(p.QuantiseSpeed(900.0), 500.0, 1e-9);  // the scale's end
}

SC_TEST("command post: the regiment's telecode arrives once a cycle; tracks carry their numbers, zones and a speed vector") {
    CommandPost p = Exact();
    CommandPost::Inputs in;
    in.telecode = true;
    std::vector<BoardContact> regiment = {Target("a", 0, 20000, 2500), Target("b", 5000, 30000, 800, 1)};
    p.Update(0.0, regiment, {}, in);
    SC_EXPECT(p.Tracks().size() == 2);
    SC_EXPECT(p.Tracks()[0].number == 0 && p.Tracks()[1].number == 1);  // the regiment's numbers, 00-99
    SC_EXPECT(p.Tracks()[0].zone == 2 && p.Tracks()[1].zone == 1 && p.Tracks()[1].friendly);
    SC_EXPECT_NEAR(p.Tracks()[0].velocity.Length(), 0.0, 1e-9);  // one fix: no speed yet
    // Halfway through the cycle the targets have moved but the tube shows the last fix.
    regiment[0].position.y -= 1000.0;  // 200 m/s
    p.Update(5.0, regiment, {}, in);
    SC_EXPECT_NEAR(p.Tracks()[0].position.y, 20000.0, 1e-9);
    regiment[0].position.y -= 1000.0;
    p.Update(10.0, regiment, {}, in);
    SC_EXPECT_NEAR(p.Tracks()[0].position.y, 18000.0, 1e-9);
    SC_EXPECT_NEAR(p.Tracks()[0].velocity.y, -200.0, 1e-9);  // 2000 m in 10 s, in 10 m/s steps
    SC_EXPECT_NEAR(p.LastTelecodeS(), 10.0, 1e-9);
    // The telecode stops: after three cycles without a fix the tracks go off the tube.
    in.telecode = false;
    p.Update(39.0, regiment, {}, in);
    SC_EXPECT(p.Tracks().size() == 2);  // still within three cycles of the last fix (10 s)
    p.Update(41.0, regiment, {}, in);
    SC_EXPECT(p.Tracks().empty());
}

SC_TEST("command post: the plotter writes one mark at a time, new targets first and nearest first, a target again only after a new fix") {
    CommandPost p = Exact();
    CommandPost::Inputs in;
    in.telecode = true;
    in.operator_free = true;
    const std::vector<BoardContact> regiment = {Target("far", 0, 40000, 2000), Target("near", 0, 20000, 2000), Target("mid", 0, 30000, 2000)};
    std::vector<double> times;
    std::vector<std::string> order;
    for (double t = 0.0; t < 30.0 - 1e-9; t += 0.5) {
        const std::size_t before = p.Board().Marks().size();
        p.Update(t, regiment, {}, in);
        if (p.Board().Marks().size() > before) {
            times.push_back(t);
            order.push_back(p.Board().Marks().back().target_id);
        }
    }
    // One mark every 5 s (the placeholder's time per mark): the three in order of range, then each again once the next
    // cycle's fix (at 10 s, 20 s) is in.
    SC_EXPECT(order.size() >= 5);
    if (order.size() >= 3) SC_EXPECT(order[0] == "near" && order[1] == "mid" && order[2] == "far");
    if (times.size() >= 3) SC_EXPECT_NEAR(times[1] - times[0], 5.0, 1e-9);
    SC_EXPECT(p.Board().Marks()[0].number == 6201);  // the board's own numbering
    // A slower plotter (half the speed) writes half as many.
    CommandPost slow = Exact();
    in.operator_speed = 0.5;
    for (double t = 0.0; t < 30.0 - 1e-9; t += 0.5) slow.Update(t, regiment, {}, in);
    SC_EXPECT(slow.Board().Marks().size() * 2 <= p.Board().Marks().size() + 1);
    // An operator busy elsewhere writes nothing.
    CommandPost busy = Exact();
    in.operator_free = false;
    for (double t = 0.0; t < 30.0 - 1e-9; t += 0.5) busy.Update(t, regiment, {}, in);
    SC_EXPECT(busy.Board().Marks().empty() && !busy.Tracks().empty());
}

SC_TEST("command post: a cabled radar's targets are taken off by hand, a few at a time, before anything is plotted") {
    CommandPost p = Exact();  // placeholder: 3 semi-automatic tracks, 2 s to take each off
    CommandPost::Inputs in;
    in.primary = true;
    in.operator_free = true;
    const std::vector<BoardContact> radar = {Target("a", 0, 10000, 1000), Target("b", 0, 12000, 1000), Target("c", 0, 14000, 1000),
                                             Target("d", 0, 16000, 1000)};
    p.Update(0.0, {}, radar, in);
    SC_EXPECT(p.Tracks().size() == 1 && p.Tracks()[0].target_id == "a" && p.Tracks()[0].primary);
    SC_EXPECT(p.Scope(50000.0).size() == 1 && p.Scope(50000.0)[0].primary);  // its echo on the IT-45 under the symbol
    p.Update(1.0, {}, radar, in);  // still taking the first off
    SC_EXPECT(p.Tracks().size() == 1);
    p.Update(2.0, {}, radar, in);
    p.Update(4.0, {}, radar, in);
    SC_EXPECT(p.Tracks().size() == 3);  // the third in, the fourth never (3 held)
    // With the three held and none due again, his hands go to the board.
    p.Update(6.0, {}, radar, in);
    SC_EXPECT(p.Board().Marks().size() == 1);
    SC_EXPECT(p.Tracks().size() == 3);
}

SC_TEST("command post: never oriented the post's picture is off by up to 5 deg and 3 km; oriented, little; driving adds 1 % at most") {
    double worst_unoriented = 0.0, worst_oriented = 0.0;
    for (unsigned seed = 1; seed <= 30; ++seed) {
        CommandPost p(PlaceholderAspd(), PlaceholderBoard(), Vec3{}, seed);
        SC_EXPECT(!p.IsOriented());
        SC_EXPECT(std::fabs(RadToDeg(p.AzErrorRad())) <= 5.0 + 1e-9 && p.OffsetM().HorizontalLength() <= 3000.0 + 1e-6);
        worst_unoriented = std::fmax(worst_unoriented, (p.Shown(Vec3{0, 50000, 0}) - Vec3{0, 50000, 0}).HorizontalLength());
        p.Oriented();
        SC_EXPECT(p.IsOriented() && std::fabs(RadToDeg(p.AzErrorRad())) <= 0.5 + 1e-9 && p.OffsetM().HorizontalLength() <= 100.0 + 1e-6);
        const double before = p.OffsetM().HorizontalLength();
        p.MoveTo(Vec3{20000, 0, 0});  // 20 km driven
        SC_EXPECT(p.OffsetM().HorizontalLength() <= before + 200.0 + 1e-6);
        worst_oriented = std::fmax(worst_oriented, (p.Shown(Vec3{20000, 50000, 0}) - Vec3{20000, 50000, 0}).HorizontalLength());
    }
    std::printf("    worst error at 50 km: %.0f m not oriented, %.0f m oriented and driven 20 km\n", worst_unoriented, worst_oriented);
    SC_EXPECT(worst_unoriented > 2000.0);  // the large error does show
    SC_EXPECT(worst_oriented < 800.0);     // 0.5 deg at 50 km is 436 m, + 100 m + 200 m
}

SC_TEST("command post: the regiment's telecode reaches the R-407's range and stops in an outage") {
    const CommandPost p = Exact();
    PostRegiment r;
    r.has_position = true;
    r.position = Vec3{0, -14000, 0};
    SC_EXPECT(p.TelecodeInReach(r, 0.0));
    r.position = Vec3{0, -16000, 0};
    SC_EXPECT(!p.TelecodeInReach(r, 0.0));
    r.position = Vec3{0, -5000, 0};
    r.outages.push_back({100.0, 200.0, "jammed"});
    SC_EXPECT(p.TelecodeInReach(r, 99.0) && !p.TelecodeInReach(r, 150.0) && p.TelecodeInReach(r, 200.0));
    // Only the targets the regiment's radars cover are sent.
    RegimentRadar radar;
    radar.position = Vec3{0, 0, 0};
    radar.max_range_m = 100000;
    radar.antenna_height_m = 9;  // horizon to a target at 100 m: 4.12 (3 + 10) km = 53.6 km
    r.radars.push_back(radar);
    const std::vector<BoardContact> seen = CommandPost::RegimentSees(r, {Target("high", 0, 90000, 5000), Target("low", 0, 60000, 100)});
    SC_EXPECT(seen.size() == 1 && seen[0].target_id == "high");
}

#include <fstream>
#include <sstream>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

namespace {

std::string ReadFile(const std::string& path) {
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

std::string ReplaceOnce(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at != std::string::npos) s.replace(at, from.size(), to);
    return s;
}

}  // namespace

SC_TEST("command post scenario: the board fills from the regiment's telecode, stops while the net is off the air, and resumes") {
    std::string t = ReadFile(std::string(SIMCORE_DATA_DIR) + "/scenarios/pu12m_deploy.json");
    t = ReplaceOnce(t, "\"inputs\": { \"vehicle_stopped\": true }", "\"inputs\": { \"vehicle_stopped\": true }, \"readiness\": \"deployed\"");
    t = ReplaceOnce(t, "\"notes\": \"The regiment's command post",
                    "\"outages\": [ { \"from_s\": 200, \"to_s\": 400, \"reason\": \"jammed\" } ], \"notes\": \"The regiment's command post");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(t, SIMCORE_DATA_DIR, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.Post() != nullptr && r.Board() == &r.Post()->Board());
    std::size_t marks_at_235 = 0, marks_at_400 = 0;
    bool reception_lost = false;
    while (!r.Finished() && r.Time() < 600.0) {
        r.Step(0.1);
        if (std::fabs(r.Time() - 235.0) < 0.05) marks_at_235 = r.Board()->Marks().size();
        if (std::fabs(r.Time() - 400.0) < 0.05) marks_at_400 = r.Board()->Marks().size();
        if (r.Time() > 210.0 && r.Time() < 390.0) reception_lost = reception_lost || !r.Console().equipment.IsReady("telecode_reception");
    }
    std::printf("    marks: %zu by 235 s, %zu by 400 s, %zu by 600 s\n", marks_at_235, marks_at_400, r.Board()->Marks().size());
    SC_EXPECT(marks_at_235 >= 10);                         // a deployed post plots from the start
    SC_EXPECT(reception_lost);                             // the R-407 net jammed: no telecode
    SC_EXPECT(marks_at_400 == marks_at_235);               // and nothing new to write
    SC_EXPECT(r.Board()->Marks().size() > marks_at_400 + 5);  // back on the air
    // The deployed post is oriented: its error is the aiming circle's, not the 5 degrees of a post never oriented.
    SC_EXPECT(r.Post()->IsOriented() && std::fabs(RadToDeg(r.Post()->AzErrorRad())) < 1.5 && r.Post()->OffsetM().HorizontalLength() < 300.0);
    SC_EXPECT(r.Debrief().find("navigation") != std::string::npos);
    // The IT-45 at 50 km doesn't show the target 60-90 km out; at 100 km it does.
    SC_EXPECT(r.Console().panel.Get("fr.scale") == 1 && std::fabs(r.PostScaleM() - 50000.0) < 1e-6);
}

SC_TEST("command post scenario: the tube's scale switch picks the scale; the player at the plotter's seat writes the marks himself") {
    std::string t = ReadFile(std::string(SIMCORE_DATA_DIR) + "/scenarios/pu12m_deploy.json");
    t = ReplaceOnce(t, "\"inputs\": { \"vehicle_stopped\": true }", "\"inputs\": { \"vehicle_stopped\": true }, \"readiness\": \"deployed\"");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(t, SIMCORE_DATA_DIR, def, console, &error));
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.TakeSeat("operator", "Player"));
    for (int i = 0; i < 300; ++i) r.Step(0.1);  // 30 s: the picture is in, nobody writes
    SC_EXPECT(!r.Post()->Tracks().empty() && r.Board()->Marks().empty());
    // The target is 80-90 km out: off the tube at 50 km, on it at 100 km.
    SC_EXPECT(r.PostScope().empty());
    r.PlayerSet("fr.scale", 2);
    r.Step(0.1);
    SC_EXPECT(r.PostScope().size() == 1 && std::fabs(r.PostScaleM() - 100000.0) < 1e-6);
    const int number = r.Post()->Tracks().front().number;
    SC_EXPECT(r.PlayerPlot(number));
    SC_EXPECT(!r.PlayerPlot(number));  // still writing the last one
    SC_EXPECT(r.Board()->Marks().size() == 1);
    SC_EXPECT(!r.PlayerPlot(99));  // no such track on the tube
}

SC_TEST("command post: the IT boards' lamps for a launcher's report") {
    LauncherStatus s;
    SC_EXPECT(BoardLampsOf(s).load == 0 && !BoardLampsOf(s).ready);  // never heard from: all dark
    s.state = LauncherStatus::State::Ready;
    s.missiles = 6;
    BoardLamps b = BoardLampsOf(s);
    SC_EXPECT(b.load == 1 && b.ready && !b.sees && !b.fired && !b.fault);
    s.missiles = 3;
    s.state = LauncherStatus::State::Engaging;
    b = BoardLampsOf(s);
    SC_EXPECT(b.load == 2 && !b.ready && b.sees);
    s.missiles = 1;
    s.state = LauncherStatus::State::Launched;
    b = BoardLampsOf(s);
    SC_EXPECT(b.load == 3 && b.fired);
    s.missiles = 0;
    s.state = LauncherStatus::State::Empty;
    SC_EXPECT(BoardLampsOf(s).load == 0 && !BoardLampsOf(s).ready);  // out of missiles: КОМПЛЕКТ dark
}
