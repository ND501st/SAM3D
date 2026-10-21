// The battery commander in the PU-12M (docs/PU12_DEVELOPMENT.md P4): targets given out by voice on the battery net from
// the post's board and tube, the launchers' reports onto the IT-1 and IT-2 boards, the NPC commander and the player.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <memory>
#include <string>

#include "simcore/em/replication.hpp"
#include "simcore/post_feed.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

std::unique_ptr<ScenarioRunner> LoadBattery() {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenarioFile(std::string(SIMCORE_DATA_DIR) + "/scenarios/pu12m_battery.json", SIMCORE_DATA_DIR, def, console, &error)) {
        std::printf("    %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

int Count(const std::string& debrief, const std::string& what) {
    int n = 0;
    for (std::size_t at = debrief.find(what); at != std::string::npos; at = debrief.find(what, at + what.size())) ++n;
    return n;
}

}  // namespace

SC_TEST("pu12m battery: the NPC battery commander gives each hostile to a launcher by voice; the launchers report back") {
    std::unique_ptr<ScenarioRunner> r = LoadBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->RunToEnd(0.1, 900.0);
    const std::string d = r->Debrief();
    // Every launcher reported ready on the battery net; each hostile was designated once, by azimuth and range; the
    // friend never was.
    SC_EXPECT(Count(d, "к бою готов, ракет 6!") == 4);
    SC_EXPECT(Count(d, "by voice: azimuth") == 4);
    SC_EXPECT(d.find("t5  designat") == std::string::npos && d.find("launches at t5") == std::string::npos);
    for (const char* t : {"t1", "t2", "t3", "t4"}) SC_EXPECT(d.find(std::string("destroys ") + t) != std::string::npos);
    SC_EXPECT(d.find("no target where it was designated") == std::string::npos);
    // The IT-1 and IT-2 boards: the four launchers with their missiles as reported.
    SC_EXPECT(r->StatusBoards().size() == 4);
    // In the battery's order, whoever reported first (the ДХ's launcher places count down the boards).
    for (std::size_t i = 0; i < r->StatusBoards().size(); ++i) SC_EXPECT(r->StatusBoards()[i].callsign == std::to_string(21 + i));
    int missiles = 0;
    for (const LauncherStatus& s : r->StatusBoards()) {
        SC_EXPECT(s.missiles >= 0 && s.reported_s > 0.0);
        missiles += s.missiles;
    }
    std::printf("    missiles left on the boards: %d of 24\n", missiles);
    SC_EXPECT(missiles >= 24 - 6 && missiles <= 24 - 4);  // one each, and maybe a second at a miss
}

SC_TEST("pu12m battery: the player in the commander's seat gives the targets himself; the NPC leaves it to him") {
    std::unique_ptr<ScenarioRunner> r = LoadBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("battery_commander", "Player"));
    while (r->Time() < 140.0) r->Step(0.1);
    SC_EXPECT(Count(r->Debrief(), "by voice: azimuth") == 0);  // nobody gives targets for him
    // Strike aircraft 1 is the second target on the board (the friend came first): give it to launcher 22.
    int number = 0;
    for (const BoardMark& m : r->Board()->Marks()) {
        if (m.target_id == "t1") number = m.number;
    }
    SC_EXPECT(number > 0);
    SC_EXPECT(!r->DesignateBatteryTarget("22", 9999));  // no such number on the board
    SC_EXPECT(!r->DesignateBatteryTarget("99", number));  // no such launcher
    SC_EXPECT(r->DesignateBatteryTarget("22", number));
    // The words take their time on the net: the launcher has it some seconds later and captures it in its own time.
    bool captured = false;
    while (r->Time() < 300.0 && !captured) {
        r->Step(0.1);
        for (const BatteryFire::Shooter& s : r->GetBatteryFire()->Shooters()) captured = captured || (s.callsign == "22" && s.target == "t1");
    }
    SC_EXPECT(captured);
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("22, я 10: цель") != std::string::npos);  // the call (rrs_1986), then the designation
}

SC_TEST("pu12m battery: with the battery net's set off the commander can give no targets and hears no reports") {
    std::unique_ptr<ScenarioRunner> r = LoadBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("battery_commander", "Player"));
    r->Step(0.1);
    r->PlayerSet("radio.fug4.power", 0);  // the R-123M of the battery net off
    while (r->Time() < 140.0) r->Step(0.1);
    SC_EXPECT(!r->Console().equipment.IsReady("battery_net"));
    int number = 0;
    for (const BoardMark& m : r->Board()->Marks()) {
        if (m.target_id == "t1") number = m.number;
    }
    SC_EXPECT(number > 0 && !r->DesignateBatteryTarget("22", number));
    // The launchers' first reports came in before the set went off; nothing since.
    for (const LauncherStatus& s : r->StatusBoards()) SC_EXPECT(s.reported_s < 10.0);
}

SC_TEST("pu12m battery: the post's IT-45, board and IT boards ride in the knowledge stream (multiplayer, P7)") {
    std::unique_ptr<ScenarioRunner> r = LoadBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    while (r->Time() < 200.0) r->Step(0.1);
    const PostFeed here = r->PostFeedNow();
    SC_EXPECT(here.post && here.scale_m > 0.0);
    SC_EXPECT(!here.scope.empty() && !here.glass.empty() && here.launchers.size() == 4);
    // What a client gets: the extras through their bytes.
    const std::vector<std::uint8_t> bytes = em::wire::ExtrasBytes(r->ViewExtrasNow(16));
    em::wire::Reader rd(bytes);
    em::ViewExtras got;
    SC_EXPECT(em::wire::GetExtras(rd, got));
    const PostFeed there = FromViewExtras(got);
    SC_EXPECT(there.post && there.scale_m == here.scale_m);
    SC_EXPECT((there.position - here.position).Length() < 1.0);
    SC_EXPECT(there.scope.size() == here.scope.size());
    for (std::size_t i = 0; i < there.scope.size() && i < here.scope.size(); ++i) {
        SC_EXPECT(there.scope[i].number == here.scope[i].number && there.scope[i].zone == here.scope[i].zone);
        SC_EXPECT(std::fabs(there.scope[i].x - here.scope[i].x) < 1e-4 && std::fabs(there.scope[i].vy - here.scope[i].vy) < 1e-4);
        SC_EXPECT(there.scope[i].friendly == here.scope[i].friendly && there.scope[i].primary == here.scope[i].primary);
    }
    SC_EXPECT(there.glass.size() == here.glass.size() && LatestMarks(there).size() == LatestMarks(here).size());
    // The board's numbers a client sees are the ones the commander can give out (PostBoard).
    SC_EXPECT(LatestMarks(here).size() == r->PostBoard().size());
    for (const BoardMark& m : r->PostBoard()) {
        bool found = false;
        for (const em::ViewBoardMark& v : LatestMarks(there)) found = found || v.number == m.number;
        SC_EXPECT(found);
    }
    SC_EXPECT(there.launchers.size() == 4);
    for (std::size_t i = 0; i < there.launchers.size(); ++i) {
        SC_EXPECT(there.launchers[i].callsign == here.launchers[i].callsign && there.launchers[i].state == here.launchers[i].state &&
                  there.launchers[i].missiles == here.launchers[i].missiles);
    }
    // An Osa scenario's extras carry no post.
    em::ViewExtras plain;
    plain.valid = true;
    const std::vector<std::uint8_t> b2 = em::wire::ExtrasBytes(plain);
    em::wire::Reader rd2(b2);
    em::ViewExtras got2;
    SC_EXPECT(em::wire::GetExtras(rd2, got2) && !FromViewExtras(got2).post);
}

SC_TEST("pu12m battery: through the seats only the plotter plots and only the commander designates") {
    std::unique_ptr<ScenarioRunner> r = LoadBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("battery_commander", "Player 1"));
    SC_EXPECT(r->TakeSeat("operator", "Player 2"));
    while (r->Time() < 140.0) r->Step(0.1);
    // With a player at the plotter's seat nobody else writes the board: he writes a report he heard himself.
    SC_EXPECT(r->PostBoard().empty());
    while (r->Time() < 400.0 && r->Post()->Heard().empty()) r->Step(0.1);
    SC_EXPECT(!r->Post()->Heard().empty());
    if (r->Post()->Heard().empty()) return;
    const int heard = r->Post()->Heard().front().number;
    std::string why;
    SC_EXPECT(!r->SeatPlot("battery_commander", heard, &why));
    SC_EXPECT(why.find("operator") != std::string::npos);
    SC_EXPECT(!r->SeatPlot("operator", r->PostScope().empty() ? 99 : r->PostScope()[0].number, &why));  // the tube: not his
    SC_EXPECT(r->SeatPlot("operator", heard, &why));
    while (r->Time() < 420.0 && r->PostBoard().empty()) r->Step(0.1);
    const std::vector<BoardMark> board = r->PostBoard();
    SC_EXPECT(board.size() == 1);
    if (board.empty()) return;
    SC_EXPECT(!r->SeatDesignate("operator", "22", board[0].number, &why));
    SC_EXPECT(why.find("battery_commander") != std::string::npos);
    SC_EXPECT(r->SeatDesignate("battery_commander", "22", board[0].number, &why));
}

SC_TEST("pu12m battery: the commander puts the DT-2 marker on a target, presses ЦУ and a launcher's place on the ДХ") {
    std::unique_ptr<ScenarioRunner> r = LoadBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("battery_commander", "Player"));
    while (r->Time() < 140.0) r->Step(0.1);
    const std::vector<CommandPost::ScopeMark> tube = r->PostScope();
    SC_EXPECT(!tube.empty());
    if (tube.empty()) return;
    const CommandPost::ScopeMark& m = tube[0];
    // The marker onto it (azimuth from north, range a fraction of the scale).
    double az = RadToDeg(std::atan2(m.x, m.y));
    if (az < 0.0) az += 360.0;
    std::string why;
    SC_EXPECT(r->SeatTurn("battery_commander", "dt2.azimuth", az, &why));
    SC_EXPECT(r->SeatTurn("battery_commander", "dt2.range", std::hypot(m.x, m.y), &why));
    r->Step(0.1);
    SC_EXPECT(r->MarkedTarget() == m.number && r->KeypadTarget() == m.number);
    SC_EXPECT(r->PostFeedNow().keypad.size() == 2);
    // Only the commander's hands: the operator can't press his keys.
    SC_EXPECT(r->SeatSet("operator", "dch.key.cu", 1, &why) != StepResult::Accepted || !why.empty());
    // ЦУ, then 2: the second launcher on the IT boards.
    r->SeatSet("battery_commander", "dch.key.cu", 1, &why);
    r->Step(0.1);
    SC_EXPECT(r->Keypad().designating);
    r->SeatSet("battery_commander", "dch.key.2", 1, &why);
    r->Step(0.1);
    SC_EXPECT(!r->Keypad().designating);
    const std::string second = r->StatusBoards().size() > 1 ? r->StatusBoards()[1].callsign : std::string();
    std::printf("    the ДХ gave: %s\n", r->Keypad().last.c_str());
    SC_EXPECT(!second.empty() && r->Keypad().last.find(">" + second) != std::string::npos);
    SC_EXPECT(Count(r->Debrief(), "by voice: azimuth") == 1);
    // Typed digits win over the marker; СБР clears them.
    r->SeatSet("battery_commander", "dch.key.4", 1, &why);
    r->Step(0.1);
    r->SeatSet("battery_commander", "dch.key.2", 1, &why);
    r->Step(0.1);
    SC_EXPECT(r->KeypadTarget() == 42);
    r->SeatSet("battery_commander", "dch.key.reset", 1, &why);
    r->Step(0.1);
    SC_EXPECT(r->Keypad().entry.empty());
}

SC_TEST("pu12m battery: the regiment's commands come by telecode as two digits; a fire ban stops the NPC's designations") {
    // The shipped battery: code 10 (readiness no. 1) at 5 s; here a fire ban (20) at 30 s, lifted (21) at 400 s.
    std::ifstream f(std::string(SIMCORE_DATA_DIR) + "/scenarios/pu12m_battery.json");
    std::stringstream text;
    text << f.rdbuf();
    std::string s = text.str();
    const std::string from = "\"commands\": [ { \"at_s\": 5, \"code\": 10 } ]";
    const std::size_t at = s.find(from);
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    s.replace(at, from.size(), "\"commands\": [ { \"at_s\": 5, \"code\": 10 }, { \"at_s\": 30, \"code\": 20 }, { \"at_s\": 400, \"code\": 21 } ]");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(s, SIMCORE_DATA_DIR, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner r(std::move(def), std::move(console));
    int before_ban = 0, during_ban = 0;
    // Counted at the ban's start and end only (the debrief is rebuilt each time it is read).
    while (!r.Finished() && r.Time() < 30.5) r.Step(0.1);
    before_ban = Count(r.Debrief(), "by voice: azimuth");
    while (!r.Finished() && r.Time() < 100.0) r.Step(0.1);
    SC_EXPECT(r.PostFeedNow().command == 20 && r.PostFeedNow().command_text.find("fire ban") != std::string::npos);
    while (!r.Finished() && r.Time() < 399.0) r.Step(0.1);
    during_ban = Count(r.Debrief(), "by voice: azimuth") - before_ban;
    while (!r.Finished() && r.Time() < 600.0) r.Step(0.1);
    const std::string d = r.Debrief();
    std::printf("    designations before the ban %d, during it %d, by 600 s %d\n", before_ban, during_ban, Count(d, "by voice: azimuth"));
    SC_EXPECT(d.find("10 on the IT-45: readiness no. 1") != std::string::npos);
    SC_EXPECT(during_ban == 0);
    SC_EXPECT(Count(d, "by voice: azimuth") > before_ban);  // lifted at 400 s
    // The codes ride in the knowledge stream.
    em::KnowledgeView v;
    v.extras = r.ViewExtrasNow();
    em::KnowledgeView back;
    SC_EXPECT(em::DecodeView(em::EncodeView(v), back) && back.extras.post_command == 21);
}

SC_TEST("pu12m battery: the ДХ's СВ and Н3 keys set the target in the window a friend in the third height zone") {
    std::unique_ptr<ScenarioRunner> r = LoadBattery();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("battery_commander", "Player"));
    while (r->Time() < 140.0) r->Step(0.1);
    const std::vector<CommandPost::ScopeMark> tube = r->PostScope();
    SC_EXPECT(!tube.empty());
    if (tube.empty()) return;
    const int number = tube[0].number;
    std::string why;
    char digits[4];
    std::snprintf(digits, sizeof(digits), "%02d", number);
    for (int i = 0; i < 2; ++i) {
        r->SeatSet("battery_commander", std::string("dch.key.") + digits[i], 1, &why);
        r->Step(0.1);
    }
    SC_EXPECT(r->KeypadTarget() == number);
    r->SeatSet("battery_commander", "dch.key.friend", 1, &why);
    r->Step(0.1);
    r->SeatSet("battery_commander", "dch.key.h3", 1, &why);
    r->Step(0.1);
    bool set = false;
    for (const AspdTrack& k : r->Post()->Tracks()) {
        if (k.number == number) set = k.friendly && k.zone == 3;
    }
    SC_EXPECT(set);
    SC_EXPECT(r->Keypad().last.find("Н3") != std::string::npos);
}

SC_TEST("pu12m range day: the ASPD-U off, the tube dark; the board from the air-picture net warns early and the battery shoots") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(std::string(SIMCORE_DATA_DIR) + "/scenarios/pu12m_range_day.json", SIMCORE_DATA_DIR, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(!def.post_regiment.telecode);
    ScenarioRunner r(std::move(def), std::move(console));
    bool telecode = false, tube = false;
    double far_mark_km = 0.0;
    while (!r.Finished() && r.Time() < 1500.0) {
        r.Step(0.1);
        telecode = telecode || r.Console().equipment.IsReady("telecode_reception");
        tube = tube || !r.PostScope().empty();
        for (const BoardMark& m : r.Board()->Marks()) far_mark_km = std::max(far_mark_km, m.position.HorizontalLength() / 1000.0);
    }
    const std::string d = r.Debrief();
    std::printf("    the farthest mark %.0f km out; %d targets destroyed\n", far_mark_km, Count(d, "destroys "));
    SC_EXPECT(!telecode && !tube);        // no telecode, nothing on the IT-45
    SC_EXPECT(far_mark_km > 120.0);       // the board reaches far beyond the tube's 100 km
    SC_EXPECT(d.find("from the board:") != std::string::npos);  // readiness from the board's warning
    // The battery shoots at two of the strikers and brings one down at least (the draws decide the rest; a receding
    // target is out of the zone once the meeting point is past 8700 - 8 V m, voenizdat_osa_1991 eq. 2.19).
    SC_EXPECT(Count(d, "launches at ") >= 2);
    SC_EXPECT(Count(d, "destroys ") >= 1);
}
