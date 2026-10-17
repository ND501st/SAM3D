// The battery commander in the PU-12M (docs/PU12_DEVELOPMENT.md P4): targets given out by voice on the battery net from
// the post's board and tube, the launchers' reports onto the IT-1 and IT-2 boards, the NPC commander and the player.
#include <memory>
#include <string>

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
