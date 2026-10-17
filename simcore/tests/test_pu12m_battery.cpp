// The battery commander in the PU-12M (docs/PU12_DEVELOPMENT.md P4): targets given out by voice on the battery net from
// the post's board and tube, the launchers' reports onto the IT-1 and IT-2 boards, the NPC commander and the player.
#include <cmath>
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
    SC_EXPECT(d.find("22, цель") != std::string::npos);
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
        SC_EXPECT(there.scope[i].friendly == here.scope[i].friendly);
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
    // With a player at the plotter's seat nobody else writes the board: he plots a target off the tube himself.
    SC_EXPECT(r->PostBoard().empty());
    const std::vector<CommandPost::ScopeMark> tube = r->PostScope();
    SC_EXPECT(!tube.empty());
    if (tube.empty()) return;
    std::string why;
    SC_EXPECT(!r->SeatPlot("battery_commander", tube[0].number, &why));
    SC_EXPECT(why.find("operator") != std::string::npos);
    SC_EXPECT(r->SeatPlot("operator", tube[0].number, &why));
    while (r->Time() < 160.0 && r->PostBoard().empty()) r->Step(0.1);
    const std::vector<BoardMark> board = r->PostBoard();
    SC_EXPECT(board.size() == 1);
    if (board.empty()) return;
    SC_EXPECT(!r->SeatDesignate("operator", "22", board[0].number, &why));
    SC_EXPECT(why.find("battery_commander") != std::string::npos);
    SC_EXPECT(r->SeatDesignate("battery_commander", "22", board[0].number, &why));
}
