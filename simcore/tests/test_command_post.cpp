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

SC_TEST("command post: the plotter does not copy the tube; he writes what he hears, at his own speed") {
    // The telecode's tracks are on the IT-45, the commander's tube (rwd_mb3): nothing of them goes on the board.
    CommandPost p = Exact();
    CommandPost::Inputs in;
    in.telecode = true;
    in.operator_free = true;
    const std::vector<BoardContact> regiment = {Target("far", 0, 40000, 2000), Target("near", 0, 20000, 2000), Target("mid", 0, 30000, 2000)};
    for (double t = 0.0; t < 90.0 - 1e-9; t += 0.5) p.Update(t, regiment, {}, in);
    SC_EXPECT(p.Tracks().size() == 3 && p.Board().Marks().empty());
    // On the air-picture net he writes each report as it ends (5 s a report, 5 s a mark); a slower plotter (half the
    // speed) takes twice as long over each mark, so the second report waits for him.
    AspdDef d = PlaceholderAspd();
    d.air_net_report_s = 5;
    d.air_net_period_s = 60;
    d.air_net_range_m = 20000;
    in.telecode = false;
    in.air_net = true;
    const std::vector<BoardContact> two = {Target("near", 0, 20000, 2000), Target("far", 0, 60000, 2000)};
    auto gap = [&](double speed) {
        CommandPost q = Exact(d);
        in.operator_speed = speed;
        std::vector<double> times;
        for (double t = 0.0; t < 40.0 - 1e-9; t += 0.5) {
            const std::size_t before = q.Board().Marks().size();
            q.Update(t, two, {}, in);
            if (q.Board().Marks().size() > before) times.push_back(t);
        }
        return times.size() == 2 ? times[1] - times[0] : -1.0;
    };
    const double normal = gap(1.0), slow = gap(0.5);
    SC_EXPECT_NEAR(normal, 5.0, 1e-9);
    SC_EXPECT_NEAR(slow, 10.0, 1e-9);
    // A plotter busy elsewhere writes nothing.
    CommandPost busy = Exact(d);
    in.operator_speed = 1.0;
    in.operator_free = false;
    for (double t = 0.0; t < 30.0 - 1e-9; t += 0.5) busy.Update(t, two, {}, in);
    SC_EXPECT(busy.Board().Marks().empty() && !busy.Heard().empty());
}

SC_TEST("command post: a cabled radar's targets are taken off by the commander's hand at the tube, a few at a time") {
    CommandPost p = Exact();  // placeholder: 3 semi-automatic tracks, 2 s to take each off
    CommandPost::Inputs in;
    in.primary = true;
    in.tube_free = true;
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
    // The plotter does not copy them onto the board.
    p.Update(6.0, {}, radar, in);
    SC_EXPECT(p.Board().Marks().empty());
    SC_EXPECT(p.Tracks().size() == 3);
    // A commander busy elsewhere takes nothing off.
    CommandPost busy = Exact();
    in.tube_free = false;
    for (double t = 0.0; t < 10.0; t += 1.0) busy.Update(t, {}, radar, in);
    SC_EXPECT(busy.Tracks().empty());
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

SC_TEST("command post: the air-picture net reads one report at a time, each target once a minute; the plotter writes what he hears first") {
    AspdDef d = PlaceholderAspd();
    d.air_net_report_s = 5;  // placeholder: 5 s a report, 5 s a mark
    d.air_net_period_s = 60;
    d.air_net_range_m = 20000;
    d.air_net_az_step_deg = 1;
    d.air_net_range_step_m = 1000;
    CommandPost p = Exact(d);
    CommandPost::Inputs in;
    in.air_net = true;
    in.operator_free = true;
    // One far beyond the tube's 100 km, one near; the reports are measured from the post (no radar set).
    const std::vector<BoardContact> regiment = {Target("far", 0, 150000, 3000), Target("near", 300, 20400, 2000)};
    std::vector<std::string> order;
    std::vector<CommandPost::Spoken> spoken;
    for (double t = 0.0; t < 50.0 - 1e-9; t += 0.5) {
        const std::size_t before = p.Board().Marks().size();
        p.Update(t, regiment, {}, in);
        for (const CommandPost::Spoken& s : p.TakeSpoken()) spoken.push_back(s);
        if (p.Board().Marks().size() > before) order.push_back(p.Board().Marks().back().target_id);
    }
    // The nearest first: read out 0-5 s, written 5-10 s; the far one read out 5-10 s, written 10-15 s. Then nothing:
    // each target once a minute.
    SC_EXPECT(spoken.size() == 2 && order.size() == 2);
    if (order.size() == 2) SC_EXPECT(order[0] == "near" && order[1] == "far");
    if (spoken.size() == 2) {
        SC_EXPECT(spoken[0].heard && spoken[0].report.text.find("azimuth 001, 20 km") != std::string::npos);
        SC_EXPECT(spoken[1].report.text.find("azimuth 000, 150 km") != std::string::npos);
        SC_EXPECT(spoken[1].report.text.find("height 30") != std::string::npos);
    }
    // The board's mark is where the report put it: the radar's steps (1 degree, 1 km), not the truth; beyond the tube.
    for (const BoardMark& m : p.Board().Marks()) {
        const double range = m.position.HorizontalLength();
        SC_EXPECT_NEAR(range, std::round(range / 1000.0) * 1000.0, 1.0);
        if (m.target_id == "far") SC_EXPECT(range > 100000.0);
    }
    SC_EXPECT(p.Heard().empty());  // all written
    // A minute on, the near one again.
    p.Update(60.0, regiment, {}, in);
    p.Update(65.0, regiment, {}, in);
    SC_EXPECT(p.TakeSpoken().size() == 1);
    // A post not on the net: the reports go out all the same, but nothing is heard or written.
    CommandPost deaf = Exact(d);
    in.air_net = false;
    for (double t = 0.0; t < 30.0 - 1e-9; t += 0.5) deaf.Update(t, regiment, {}, in);
    const std::vector<CommandPost::Spoken> missed = deaf.TakeSpoken();
    SC_EXPECT(missed.size() == 2 && !missed[0].heard && deaf.Board().Marks().empty() && deaf.Heard().empty());
    // A plotter busy elsewhere: the heard reports wait, and are forgotten after a period.
    CommandPost busy = Exact(d);
    in.air_net = true;
    in.operator_free = false;
    for (double t = 0.0; t < 20.0 - 1e-9; t += 0.5) busy.Update(t, regiment, {}, in);
    SC_EXPECT(busy.Heard().size() == 2);
    // The player at the plotter's seat writes a heard report by its board number.
    const int number = busy.Heard().front().number;
    SC_EXPECT(number >= 6201 && busy.PlotHeard(20.0, number, 1.0));
    SC_EXPECT(!busy.PlotHeard(20.0, busy.Heard().front().number, 1.0));  // still writing
    SC_EXPECT(busy.Board().Marks().size() == 1 && busy.Heard().size() == 1);
    busy.Update(80.0, {}, {}, in);
    SC_EXPECT(busy.Heard().empty());  // the other, never written, forgotten after a minute
    // The net's reach is the R-123M's from the regiment's post.
    PostRegiment r;
    r.has_position = true;
    r.position = Vec3{0, -19000, 0};
    SC_EXPECT(p.AirNetInReach(r, 0.0));
    r.position = Vec3{0, -21000, 0};
    SC_EXPECT(!p.AirNetInReach(r, 0.0));
}

SC_TEST("command post: the telecode's sending post takes its targets off by hand: with many each comes less often, and in steps") {
    AspdDef d = PlaceholderAspd();
    d.telecode_take_off_s = 2.5;  // four in a 10 s cycle
    d.telecode_step_m = 1000;
    CommandPost p = Exact(d);
    CommandPost::Inputs in;
    in.telecode = true;
    std::vector<BoardContact> regiment;
    for (int i = 1; i <= 8; ++i) regiment.push_back(Target("t" + std::to_string(i), 0, 10400.0 * i, 2000));
    p.Update(0.0, regiment, {}, in);
    SC_EXPECT(p.Tracks().size() == 4);  // the nearest four
    bool nearest = true;
    for (const AspdTrack& k : p.Tracks()) nearest = nearest && k.position.y < 45000.0;
    SC_EXPECT(nearest);
    SC_EXPECT_NEAR(p.Tracks()[0].position.y, 10000.0, 1e-9);  // 10.4 km sent as 10 km
    p.Update(10.0, regiment, {}, in);
    SC_EXPECT(p.Tracks().size() == 8);  // the other four
    for (const AspdTrack& k : p.Tracks()) {
        if (k.target_id == "t1") SC_EXPECT_NEAR(k.time_s, 0.0, 1e-9);  // not refreshed this cycle
    }
    p.Update(20.0, regiment, {}, in);
    for (const AspdTrack& k : p.Tracks()) {
        if (k.target_id == "t1") SC_EXPECT_NEAR(k.time_s, 20.0, 1e-9);  // its turn again: every 20 s
    }
    // The tracks stay on the tube for three of the sender's rounds (20 s each), not three cycles.
    in.telecode = false;
    p.Update(55.0, regiment, {}, in);
    SC_EXPECT(p.Tracks().size() == 8);
    p.Update(81.0, regiment, {}, in);
    SC_EXPECT(p.Tracks().empty());
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
    SC_EXPECT(marks_at_235 >= 4);                          // a deployed post plots from the start, the target once a minute
    SC_EXPECT(reception_lost);                             // the R-407 net jammed: no telecode
    SC_EXPECT(marks_at_400 == marks_at_235);               // and nothing new to write
    SC_EXPECT(r.Board()->Marks().size() >= marks_at_400 + 3);  // back on the air
    // The deployed post is oriented: its error is the aiming circle's, not the 5 degrees of a post never oriented.
    SC_EXPECT(r.Post()->IsOriented() && std::fabs(RadToDeg(r.Post()->AzErrorRad())) < 1.5 && r.Post()->OffsetM().HorizontalLength() < 300.0);
    SC_EXPECT(r.Debrief().find("navigation") != std::string::npos);
    // The IT-45 at 50 km doesn't show the target 60-90 km out; at 100 km it does.
    SC_EXPECT(r.Console().panel.Get("fr.scale") == 1 && std::fabs(r.PostScaleM() - 50000.0) < 1e-6);
}

SC_TEST("command post scenario: the tube's scale switch picks the scale; the player at the plotter's seat cannot copy the tube") {
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
    // The tube is the commander's: its tracks are not the plotter's to write.
    SC_EXPECT(!r.PlayerPlot(r.Post()->Tracks().front().number));
    SC_EXPECT(r.Board()->Marks().empty());
}

SC_TEST("command post scenario: the regiment's air-picture net is heard in the post, waits for the plotter, and reaches a client") {
    std::string t = ReadFile(std::string(SIMCORE_DATA_DIR) + "/scenarios/pu12m_deploy.json");
    t = ReplaceOnce(t, "\"inputs\": { \"vehicle_stopped\": true }", "\"inputs\": { \"vehicle_stopped\": true }, \"readiness\": \"deployed\"");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(t, SIMCORE_DATA_DIR, def, console, &error));
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.TakeSeat("operator", "Player"));
    for (int i = 0; i < 300; ++i) r.Step(0.1);  // 30 s: reports heard, nobody writes
    SC_EXPECT(r.Console().equipment.IsReady("air_net"));
    bool logged = false;
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) logged = logged || (m.net == "air picture" && m.text.find("square") != std::string::npos);
    SC_EXPECT(logged);  // in the post's radio log: the plotter's headset
    const PostFeed f = r.PostFeedNow();
    SC_EXPECT(!f.heard.empty() && r.Board()->Marks().empty());
    // Through the knowledge stream, so a client's plotter can pick the report.
    em::KnowledgeView v;
    v.extras = r.ViewExtrasNow();
    em::KnowledgeView back;
    SC_EXPECT(em::DecodeView(em::EncodeView(v), back) && FromViewExtras(back.extras).heard == f.heard);
    if (f.heard.empty()) return;
    SC_EXPECT(r.PlayerPlot(f.heard.front()));  // a heard report, by its board number
    SC_EXPECT(r.Board()->Marks().size() == 1 && r.Board()->Marks()[0].number == f.heard.front());
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

SC_TEST("command post: the regiment's designations to the battery ride with its telecode and are marked on the tube") {
    CommandPost p = Exact();
    p.SetRegimentDesignationRange(25000);  // placeholder
    CommandPost::Inputs in;
    in.telecode = true;
    p.Update(0.0, {Target("near", 0, 20000, 2000), Target("far", 0, 40000, 2000)}, {}, in);
    SC_EXPECT(p.Tracks().size() == 2);
    for (const AspdTrack& k : p.Tracks()) SC_EXPECT(k.designated == (k.target_id == "near"));
    int designated = 0;
    for (const CommandPost::ScopeMark& m : p.Scope(50000)) designated += m.designated ? 1 : 0;
    SC_EXPECT(designated == 1);
}

SC_TEST("command post: the ДХ's characteristics stay with the target through later fixes; the board is not the tube's") {
    CommandPost p = Exact();
    CommandPost::Inputs in;
    in.telecode = true;
    const std::vector<BoardContact> regiment = {Target("a", 0, 20000, 2500)};
    p.Update(0.0, regiment, {}, in);
    const int number = p.Tracks().front().number;
    CommandPost::Attributes a;
    a.friendly = 1;
    a.group = 1;
    a.zone = 3;
    SC_EXPECT(p.SetAttributes(number, a));
    SC_EXPECT(!p.SetAttributes(77, a));  // no such track
    p.Update(10.0, regiment, {}, in);    // a new fix keeps them
    const AspdTrack& k = p.Tracks().front();
    SC_EXPECT(k.friendly && k.type == 1 && k.group && k.zone == 3);
    in.operator_free = true;
    p.Update(11.0, regiment, {}, in);
    SC_EXPECT(p.Board().Marks().empty());  // the plotter writes what he hears, not the commander's tube
}

SC_TEST("command post: the ФР block at КОНТРОЛЬ shows an imitated echo, taken off like a radar's, never written on the board") {
    CommandPost p = Exact();
    CommandPost::Inputs in;
    in.imitation = true;
    in.operator_free = true;
    in.tube_free = true;
    p.Update(0.0, {}, {}, in);
    bool echo = false, track = false;
    for (const CommandPost::ScopeMark& m : p.Scope(35000)) {
        echo = echo || (m.imitated && m.number < 0);
        track = track || (m.imitated && m.number >= 0);
    }
    SC_EXPECT(echo && track);  // the echo, and taken off at once by the commander at the tube
    for (double t = 0.5; t < 90.0; t += 0.5) p.Update(t, {}, {}, in);
    SC_EXPECT(p.Board().Marks().empty());  // nothing to write: there is no aircraft
    in.imitation = false;
    p.Update(90.5, {}, {}, in);
    SC_EXPECT(p.Tracks().empty() && p.Scope(35000).empty());
}

SC_TEST("command post: the code table loads; every effect is one the battery knows") {
    AspdCodeTable t;
    std::string error;
    SC_EXPECT(LoadAspdCodesFile(std::string(SIMCORE_DATA_DIR) + "/command/aspd_codes.json", t, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(t.Find(10) != nullptr && t.Find(10)->effect == "readiness");
    SC_EXPECT(t.Find(20) != nullptr && t.Find(20)->effect == "fire_ban");
    SC_EXPECT(t.Find(99) == nullptr);
}

SC_TEST("command post scenario: the primary radar's switch must be on the cabled radar's type for its picture") {
    std::string t = ReadFile(std::string(SIMCORE_DATA_DIR) + "/scenarios/pu12m_deploy.json");
    t = ReplaceOnce(t, "\"inputs\": { \"vehicle_stopped\": true }", "\"inputs\": { \"vehicle_stopped\": true }, \"readiness\": \"deployed\"");
    t = ReplaceOnce(t, "\"regiment\": {",
                    "\"radar\": { \"type\": \"p19\", \"position\": { \"east_m\": 150, \"north_m\": -200, \"up_m\": 2 } }, \"regiment\": {");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(t, SIMCORE_DATA_DIR, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(def.has_post_radar && def.post_radar.switch_position == 3);
    ScenarioRunner r(std::move(def), std::move(console));
    for (int i = 0; i < 20; ++i) r.Step(0.1);
    SC_EXPECT(r.Console().equipment.GetInput("radar_coupled"));  // the switch starts at 1RL13 / 1RL134: the P-19's
    r.PlayerSet("aspd.radar_source", 0);                           // 1RL14 / 1RL131: a P-18's
    for (int i = 0; i < 20; ++i) r.Step(0.1);
    SC_EXPECT(!r.Console().equipment.GetInput("radar_coupled") && !r.Console().equipment.IsReady("primary_picture"));
    r.PlayerSet("aspd.radar_source", 2);
    for (int i = 0; i < 20; ++i) r.Step(0.1);
    SC_EXPECT(r.Console().equipment.GetInput("radar_coupled"));
}

#include "simcore/pvo_grid.hpp"

SC_TEST("PVO grid: the medium and small squares numbered 1-9 clockwise from the top left, 9 in the middle; plotted at the centre") {
    // A big square from 47 N, 18 E (1 degree by 1 degree 30'); the place 47.95 N, 18.10 E: the top row, left column (1),
    // then within it the bottom row, left column of its ninth (7).
    const std::string sq = pvo_grid::Encode(47.95, 18.10, 2);
    SC_EXPECT(sq.size() == 4 && sq[2] == '1');
    double lat = 0.0, lon = 0.0;
    SC_EXPECT(pvo_grid::Centre(sq, 47.0, 18.0, lat, lon));
    SC_EXPECT(std::fabs(lat - 47.95) <= 1.0 / 18.0 && std::fabs(lon - 18.10) <= 1.5 / 18.0);  // within half a small square
    SC_EXPECT(pvo_grid::Encode(47.5, 18.75, 1).back() == '9');  // the middle
    SC_EXPECT(pvo_grid::Encode(47.9, 19.45, 1).back() == '3');  // top right
    SC_EXPECT(pvo_grid::Encode(47.1, 19.45, 1).back() == '5');  // bottom right
    SC_EXPECT(pvo_grid::Encode(47.1, 18.05, 1).back() == '7');  // bottom left
    // The additional square narrows it to a ninth again.
    const std::string fine = pvo_grid::Encode(47.95, 18.10, 3);
    SC_EXPECT(fine.size() == 5 && pvo_grid::Centre(fine, 47.0, 18.0, lat, lon));
    SC_EXPECT(std::fabs(lat - 47.95) <= 1.0 / 54.0 && std::fabs(lon - 18.10) <= 1.5 / 54.0);
}

SC_TEST("command post: on the grid the air-picture net reads the target's square, and the mark goes at its centre") {
    AspdDef d = PlaceholderAspd();
    d.air_net_report_s = 5;
    d.air_net_period_s = 60;
    d.air_net_grid_levels = 2;
    CommandPost p = Exact(d);
    p.SetFrame(LocalFrame(GeoPoint::FromDegrees(47.3, 18.6, 100.0)));
    CommandPost::Inputs in;
    in.air_net = true;
    in.operator_free = true;
    for (double t = 0.0; t < 15.0; t += 0.5) p.Update(t, {Target("a", 20000, 90000, 3000)}, {}, in);
    const std::vector<CommandPost::Spoken> spoken = p.TakeSpoken();
    SC_EXPECT(spoken.size() == 1 && spoken[0].report.text.find("square ") != std::string::npos);
    SC_EXPECT(p.Board().Marks().size() == 1);
    if (!p.Board().Marks().empty()) {
        const double off = (p.Board().Marks()[0].position - Vec3{20000, 90000, 0}).HorizontalLength();
        std::printf("    %s: the mark %.1f km from the target\n", spoken.empty() ? "?" : spoken[0].report.text.c_str(), off / 1000.0);
        SC_EXPECT(off < 9000.0);  // inside its small square (about 12 x 11 km)
    }
}

SC_TEST("command post: the player's mark goes exactly where the report is within 5 cm, off by up to his miss to 20 cm, further beyond") {
    AspdDef d = PlaceholderAspd();
    d.air_net_report_s = 1;
    CommandPost::Inputs in;
    in.air_net = true;
    in.operator_free = false;  // the player holds the plotter's seat
    PlottingTuning tu;          // 5 cm, 20 cm, 1.0-1.6 (data/gameplay/plotting.json's)
    const double mpb = 200000.0 / 0.17;  // a board 0.34 m across for 200 km each side
    auto plot = [&](double miss_board_m, unsigned seed) {
        CommandPost p = Exact(d);
        (void)seed;
        p.Update(0.0, {Target("a", 0, 100000, 3000)}, {}, in);
        p.Update(2.0, {Target("a", 0, 100000, 3000)}, {}, in);
        const int number = p.Heard().empty() ? -1 : p.Heard().front().number;
        const Vec3 right = p.Heard().empty() ? Vec3{} : p.Heard().front().contact.position;
        return p.PlotAt(3.0, number, right + Vec3{miss_board_m * mpb, 0.0, 0.0}, mpb, tu, 1.0);
    };
    const CommandPost::PlotResult exact = plot(0.04, 1);
    SC_EXPECT(exact.ok && exact.error_board_m == 0.0);
    for (int i = 0; i < 20; ++i) {
        const CommandPost::PlotResult mid = plot(0.15, static_cast<unsigned>(i));
        SC_EXPECT(mid.ok && mid.error_board_m >= 0.05 - 1e-9 && mid.error_board_m <= 0.15 + 1e-9);
        const CommandPost::PlotResult far = plot(0.30, static_cast<unsigned>(i));
        SC_EXPECT(far.ok && far.error_board_m >= 0.30 - 1e-9 && far.error_board_m <= 0.48 + 1e-9);
    }
    // The mark is where the result says, on the board.
    CommandPost p = Exact(d);
    p.Update(0.0, {Target("a", 0, 100000, 3000)}, {}, in);
    p.Update(2.0, {Target("a", 0, 100000, 3000)}, {}, in);
    const CommandPost::PlotResult r = p.PlotAt(3.0, p.Heard().front().number, Vec3{0, 100000, 0}, mpb, tu, 1.0);
    SC_EXPECT(r.ok && p.Board().Marks().size() == 1);
    SC_EXPECT_NEAR((p.Board().Marks()[0].position - r.placed).HorizontalLength(), 0.0, 1e-6);
    // Nothing waiting under that number, or a hand still writing: no mark.
    SC_EXPECT(!p.PlotAt(3.5, 9999, Vec3{}, mpb, tu, 1.0).ok);
}

SC_TEST("command post: a folded board takes no marks; the reports wait and are forgotten") {
    AspdDef d = PlaceholderAspd();
    d.air_net_report_s = 1;
    CommandPost p = Exact(d);
    CommandPost::Inputs in;
    in.air_net = true;
    in.operator_free = true;
    in.board_open = false;
    for (double t = 0.0; t < 20.0; t += 0.5) p.Update(t, {Target("a", 0, 50000, 3000)}, {}, in);
    SC_EXPECT(p.Board().Marks().empty() && !p.Heard().empty() && !p.BoardOpen());
    SC_EXPECT(!p.PlotHeard(20.0, p.Heard().front().number, 1.0));
    in.board_open = true;
    p.Update(20.5, {Target("a", 0, 50000, 3000)}, {}, in);
    SC_EXPECT(p.Board().Marks().size() == 1);  // opened: the plotter writes what he heard
}

SC_TEST("command post: the plotting tuning loads") {
    PlottingTuning t;
    std::string error;
    SC_EXPECT(LoadPlottingTuningFile(std::string(SIMCORE_DATA_DIR) + "/gameplay/plotting.json", t, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT_NEAR(t.exact_board_m, 0.05, 1e-12);
    SC_EXPECT_NEAR(t.scatter_board_m, 0.20, 1e-12);
}

SC_TEST("command post scenario: the regiment's commands come by voice on the R-111 when the telecode is off") {
    std::string t = ReadFile(std::string(SIMCORE_DATA_DIR) + "/scenarios/pu12m_range_day.json");
    t = ReplaceOnce(t, "\"telecode\": false", "\"telecode\": false, \"commands\": [ { \"at_s\": 20, \"code\": 20 } ]");
    t = ReplaceOnce(t, "\"plotting\": \"gameplay/plotting.json\",", "\"plotting\": \"gameplay/plotting.json\", \"codes\": \"command/aspd_codes.json\",");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(t, SIMCORE_DATA_DIR, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner r(std::move(def), std::move(console));
    for (int i = 0; i < 300; ++i) r.Step(0.1);
    bool heard = false;
    // By voice on the R-111, as a group of the talk table ПТ-1 (214: fire ban), heard by the commander's RTK-1 on РСТ-3.
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) {
        heard = heard || (m.net == "regiment net" && m.text.find("Тополь: 214") != std::string::npos && m.HeardAt("battery_commander"));
    }
    SC_EXPECT(heard);
    SC_EXPECT(r.PostFeedNow().command < 0);    // not on the tube: no telecode
}
