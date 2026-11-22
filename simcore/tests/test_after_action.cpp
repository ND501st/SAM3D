// The after-action loop's first link (roadmap P9; docs/command/01_PRINCIPLES.md 9, docs/campaign/05_THE_SYSTEMS.md §11):
// the battery's combat report compiled from its records, never the truth; the regiment's check of the records against the
// radar company's log, the crew's mark for them and the regiment's trust in the unit; the report as a fact of the chain.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/campaign.hpp"
#include "simcore/command.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::campaign;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

JournalLine Air(double t, const std::string& text) { return {t, "air picture", "Regiment", text}; }
BoardRecord MarkAt(int number, double t, double east_m, int code) {
    BoardRecord m;
    m.number = number;
    m.time_s = t;
    m.position = Vec3{east_m, 20000.0, 1000.0};
    m.height_hm = 10;
    m.type = code / 10;
    m.formation = code % 10;
    return m;
}
std::string Clock(double s) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d:%02d", static_cast<int>(s) / 60, static_cast<int>(s) % 60);
    return buf;
}

// Two raids twenty minutes apart, as the post wrote them.
std::vector<JournalLine> Journal(bool second_raid) {
    std::vector<JournalLine> j = {Air(60.0, "0012: square 4215, 82, height 30, minute 02"),
                                  Air(65.0, "0013: square 4315, 81, height 10, minute 02"),
                                  Air(70.0, "0020: square 4400, 11, height 40, minute 02"),  // our own transport
                                  {100.0, "battery net", "21", "21: к бою готов"},
                                  Air(120.0, "0012: square 4216, 82, height 30, minute 03"),
                                  Air(130.0, "00-4: square 42-5, 8-, height 1-, minute 03")};  // boxes left empty
    if (second_raid) j.push_back(Air(1500.0, "0031: square 4100, 84, height 50, minute 26"));
    return j;
}
std::vector<BoardRecord> Board(bool second_raid) {
    std::vector<BoardRecord> b = {MarkAt(12, 62.0, 3000.0, 82), MarkAt(13, 70.0, 5000.0, 81), MarkAt(20, 75.0, 9000.0, 11),
                                  MarkAt(12, 125.0, 3200.0, 82), MarkAt(14, 140.0, -4000.0, 81)};  // 0014 only on the board
    if (second_raid) b.push_back(MarkAt(31, 1505.0, -9000.0, 84));
    return b;
}
const std::vector<LogEntry> kLog = {{200.0, "21", "launch", 12}, {215.0, "21", "destroyed", 12}, {230.0, "22", "launch", 13},
                                    {245.0, "22", "missed", 13}};

}  // namespace

SC_TEST("after action (P9): the combat report from the records: raids by time, friends apart, a lost line loses its raid") {
    AfterActionValues v;
    std::string e;
    SC_EXPECT(LoadAfterActionValues(kData, "gameplay/after_action.json", v, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT_NEAR(v.raid_gap_s, 600.0, 1e-9);
    const PlaceOf place = [](const Vec3& p) { return p.x < 0.0 ? std::string("square 4105") : std::string("square 4200"); };
    const CombatReport r = CompileReport("Battery 21", 0.0, 1800.0, Journal(true), Board(true), kLog, v, place);
    const std::string text = ReportText(r, Clock);
    std::printf("    %s\n    %s\n", ReportSummary(r).c_str(), text.c_str());
    SC_EXPECT(r.raids.size() == 2);
    if (r.raids.size() != 2) return;
    // The first raid: 0012 (two aircraft written), 0013 and 0014 (the board's), four aircraft; 0020 written as ours.
    SC_EXPECT(r.raids[0].numbers == std::vector<int>({12, 13, 14}) && r.raids[0].aircraft == 4);
    SC_EXPECT(r.raids[1].numbers == std::vector<int>({31}) && r.raids[1].aircraft == 4);
    SC_EXPECT(r.friends == std::vector<int>({20}));
    const ReportTarget* t12 = r.Target(12);
    SC_EXPECT(t12 != nullptr && t12->places == std::vector<std::string>({"square 4215", "square 4216"}) && t12->lines == 2 && t12->marks == 2);
    const ReportTarget* t14 = r.Target(14);
    SC_EXPECT(t14 != nullptr && t14->from_board && t14->places == std::vector<std::string>({"square 4105"}));
    SC_EXPECT(r.unreadable == 1 && r.missiles == 2 && r.destroyed == 1);
    SC_EXPECT(text.find("0012 hostile x2 from square 4215 to square 4216 at height 30") != std::string::npos);
    SC_EXPECT(text.find("0014 hostile in square 4105 at height 10 (the board only)") != std::string::npos);
    SC_EXPECT(text.find("Our own: 0020") != std::string::npos && text.find("(0012 by 21)") != std::string::npos);
    // The radiotelephonist lost the second raid's line unwritten and nobody plotted it: the report has one raid.
    const CombatReport lost = CompileReport("Battery 21", 0.0, 1800.0, Journal(false), Board(false), kLog, v, place);
    SC_EXPECT(lost.raids.size() == 1 && lost.Target(31) == nullptr && ReportText(lost, Clock).find("Raid 2") == std::string::npos);
    // A digit written wrong in a number: a second target where there was one.
    std::vector<JournalLine> slipped = Journal(false);
    slipped[4].text = "0072: square 4216, 82, height 30, minute 03";
    const CombatReport ghost = CompileReport("Battery 21", 0.0, 1800.0, slipped, {}, {}, v);
    SC_EXPECT(ghost.Target(72) != nullptr && ghost.Target(12) != nullptr && ghost.Target(12)->places.size() == 1);
    // A kill taken back is no kill; the short form gives a number and a time only.
    const std::vector<LogEntry> back = {{200.0, "21", "launch", 12}, {215.0, "21", "destroyed", 12}, {260.0, "21", "not destroyed", 12}};
    const CombatReport taken = CompileReport("Battery 21", 0.0, 600.0, {Air(60.0, "0012 — кратко")}, {}, back, v);
    SC_EXPECT(taken.destroyed == 0 && taken.missiles == 1 && taken.Target(12) != nullptr && taken.Target(12)->places.empty() &&
              taken.Target(12)->type == -1);
    // Roadmap S4: taken back as a hit (seen burning on the TV, flying on): no kill, one hit, and the report says so.
    const std::vector<LogEntry> hit = {{200.0, "21", "launch", 12}, {215.0, "21", "destroyed", 12}, {260.0, "21", "hit", 12}};
    const CombatReport burning = CompileReport("Battery 21", 0.0, 600.0, {Air(60.0, "0012 — кратко")}, {}, hit, v);
    const std::string bt = ReportText(burning, Clock);
    std::printf("    %s\n", bt.c_str());
    SC_EXPECT(burning.destroyed == 0 && burning.damaged == 1 && burning.missiles == 1);
    SC_EXPECT(bt.find("Targets hit and flying on: 1 (0012 by 21)") != std::string::npos);
    SC_EXPECT(taken.damaged == 0 && ReportText(taken, Clock).find("hit and flying on") == std::string::npos);
}

SC_TEST("after action (P9): the regiment checks the records against the radar company's log; the mark; the trust") {
    AfterActionValues v;
    SC_EXPECT(LoadAfterActionValues(kData, "gameplay/after_action.json", v));
    const std::vector<SaidReport> said = {{60.0, 12, "0012: square 4215, 82, height 30, minute 02"},
                                          {65.0, 13, "0013: square 4315, 81, height 10, minute 02"},
                                          {120.0, 12, "0012: square 4216, 82, height 30, minute 03"},
                                          {1500.0, 31, "0031: square 4100, 84, height 50, minute 26"}};
    // Word for word: excellent.
    const std::vector<JournalLine> good = {Air(60.0, said[0].text), Air(65.0, said[1].text), Air(120.0, said[2].text), Air(1500.0, said[3].text)};
    const RecordCheck ok = CheckJournal(good, said, v);
    SC_EXPECT(ok.as_said == 4 && ok.wrong == 0 && ok.missing == 0 && ok.ghosts == 0 && RecordGrade(ok, v) == 5);
    // 0013 written 0018, the second raid lost unwritten, a line for nothing said.
    const std::vector<JournalLine> bad = {Air(60.0, said[0].text), Air(65.0, "0018: square 4315, 81, height 10, minute 02"), Air(120.0, said[2].text),
                                          Air(900.0, "0044: square 4000, 82, height 20, minute 15")};
    const RecordCheck c = CheckJournal(bad, said, v);
    std::printf("    journal: %d of %d as said, %d a digit wrong, %d missing, %d ghosts: agreement %.2f, %s\n", c.as_said, c.said, c.wrong, c.missing,
                c.ghosts, c.Agreement(), GradeWord(RecordGrade(c, v)));
    SC_EXPECT(c.said == 4 && c.as_said == 2 && c.wrong == 1 && c.missing == 1 && c.ghosts == 1);
    SC_EXPECT_NEAR(c.Agreement(), 0.4, 1e-9);
    SC_EXPECT(RecordGrade(c, v) == 2);
    // The board: each report's mark under its number within a period and a half; one under another number in its time.
    const std::vector<BoardRecord> board = {MarkAt(12, 62.0, 0.0, 82), MarkAt(18, 70.0, 0.0, 81), MarkAt(12, 125.0, 0.0, 82),
                                            MarkAt(31, 1505.0, 0.0, 84)};
    const RecordCheck b = CheckBoard(board, said, v);
    SC_EXPECT(b.as_said == 3 && b.wrong == 1 && b.missing == 0 && b.ghosts == 0 && RecordGrade(b, v) == 3);
    // Nothing said and nothing written: not graded.
    SC_EXPECT(RecordGrade(CheckJournal({}, {}, v), v) == 0);
    // The regiment's trust: 0.8 before a check; battery 21's bad journal brings it to 0.6, battery 22's good one to 8/9.
    ReportTrust trust(v);
    SC_EXPECT_NEAR(trust.Of("bty21"), 0.8, 1e-9);
    trust.Checked("bty21", c);
    trust.Checked("bty22", ok);
    SC_EXPECT_NEAR(trust.Of("bty21"), 0.6, 1e-9);
    SC_EXPECT_NEAR(trust.Of("bty22"), 8.0 / 9.0, 1e-9);
    SC_EXPECT(trust.Checks("bty21") == 1 && trust.Checks("bty23") == 0);
    trust.Checked("bty21", ok);
    SC_EXPECT(trust.Of("bty21") > 0.6 && trust.Of("bty21") < trust.Of("bty22"));
}

SC_TEST("after action (P9): the post's report from its own records; a radiotelephonist who writes nothing grades it and the regiment learns less") {
    const auto play = [](bool player_keeper, std::string& debrief, CombatReport& report, RecordCheck& journal, RecordCheck& board, int& launches) {
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        if (!LoadScenarioFile(kData + "/scenarios/pu12m_battery.json", kData, def, console, &e)) {
            std::printf("    %s\n", e.c_str());
            return false;
        }
        ScenarioRunner r(std::move(def), std::move(console));
        if (player_keeper && !r.TakeSeat("radio_operator", "Player")) return false;  // he never takes a line down
        while (!r.Finished() && r.Time() < 900.0) r.Step(0.1);
        debrief = r.Debrief();
        if (!r.ReportFromRecords(report, "Battery 21") || !r.CheckRecords(journal, board)) return false;
        launches = 0;
        for (const LogEntry& l : r.CommanderLog()) launches += l.kind == "launch" ? 1 : 0;
        // Nothing in the report names a target as the simulation knows it: only board numbers.
        for (const ReportTarget& t : report.targets) {
            if (t.number <= 0) return false;
        }
        return journal.said == static_cast<int>(r.RadarLog().size()) && board.said == journal.said;
    };
    std::string debrief, debrief2;
    CombatReport npc, player;
    RecordCheck nj, nb, pj, pb;
    int launches = 0, launches2 = 0;
    SC_EXPECT(play(false, debrief, npc, nj, nb, launches));
    SC_EXPECT(play(true, debrief2, player, pj, pb, launches2));
    AfterActionValues v;
    std::printf("    the NPC radiotelephonist: %s\n", ReportSummary(npc).c_str());
    std::printf("      journal %d of %d as said, %d wrong, %d missing, %d ghosts (%s); board %d of %d (%s)\n", nj.as_said, nj.said, nj.wrong, nj.missing,
                nj.ghosts, GradeWord(RecordGrade(nj, v)), nb.as_said, nb.said, GradeWord(RecordGrade(nb, v)));
    std::printf("    a player who writes nothing: %s\n      journal %d of %d as said, %d missing (%s); board %d of %d (%s)\n", ReportSummary(player).c_str(),
                pj.as_said, pj.said, pj.missing, GradeWord(RecordGrade(pj, v)), pb.as_said, pb.said, GradeWord(RecordGrade(pb, v)));
    // The battery fought one raid and wrote it down: its targets, its own transport apart, its launches from the log.
    SC_EXPECT(nj.said > 0 && npc.raids.size() == 1 && npc.targets.size() >= 3 && !npc.friends.empty());
    SC_EXPECT(npc.missiles == launches && launches > 0);
    SC_EXPECT(nj.as_said + nj.wrong + nj.missing == nj.said && RecordGrade(nj, v) >= 4);
    SC_EXPECT(debrief.find("The combat report from the records: Battery") != std::string::npos ||
              debrief.find("The combat report from the records: The battery") != std::string::npos);
    SC_EXPECT(debrief.find("The journal against the radar company's log") != std::string::npos);
    // The page stayed empty: every report missing from the journal, graded unsatisfactory; the board's marks (the plotter
    // heard the net himself) still carry the targets, so the report has them from the board only.
    SC_EXPECT(pj.said > 0 && pj.missing == pj.said && pj.as_said == 0 && RecordGrade(pj, v) == 2);
    bool board_only = !player.targets.empty();
    for (const ReportTarget& t : player.targets) board_only = board_only && t.from_board && t.lines == 0;
    SC_EXPECT(board_only);
    // The regiment believes the battery less after it.
    ReportTrust trust(v);
    ReportTrust trust2(v);
    trust.Checked("bty21", nj);
    trust2.Checked("bty21", pj);
    SC_EXPECT(trust2.Of("bty21") < trust.Of("bty21"));
}

SC_TEST("after action (P9): the report goes up from the battery commander's post as a written report at priority") {
    using namespace simcore::command;
    ChainData d;
    std::string e;
    SC_EXPECT(LoadChainData(kData, "command/chain_pannonia.json", d, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    AfterActionValues v;
    const CombatReport r = CompileReport("Battery 21", 0.0, 1800.0, Journal(true), Board(true), kLog, v);
    const double start = 6.0;
    InfoNet net;
    net.Init(d, 5u, start);
    net.SetPlayer("veh21_1_commander");  // the battery commander is an NPC: he sends it himself
    const double h0 = 2.0;
    net.AdvanceTo(h0);
    Fact f;
    SC_EXPECT(CombatReportFact(net, r, h0, "report:1", f));
    std::printf("    %s\n", f.Told().c_str());
    SC_EXPECT(f.origin == "bty21_commander" && f.kind == FactKind::RaidResult && f.document && f.precedence == 1 && !f.held);
    SC_EXPECT(std::fabs(f.h - (h0 + 0.5)) < 1e-9);
    SC_EXPECT(f.text.find("Raid 2, D1 08:25") != std::string::npos);
    const int fi = net.AddFact(f);
    net.AdvanceTo(h0 + 3.0);
    SC_EXPECT(net.Known(d.PositionIndex("rgt_commander"), fi) != nullptr || net.Known(d.PositionIndex("rgt_chief_of_staff"), fi) != nullptr);
    // The player is the battery commander: the report is his own and goes where he tells it.
    InfoNet mine;
    mine.Init(d, 5u, start);
    mine.AdvanceTo(h0);
    SC_EXPECT(CombatReportFact(mine, r, h0, "report:1", f) && f.held);
}
