// Multiplayer crew: players in the crew's seats replace the NPCs there, work only their own station's controls, get the
// orders for their seat as procedures; every player's machine mirrors the server's console (console_sync.hpp).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct Loaded {
    ScenarioDef def;
    ConsoleData console;
    bool ok = false;
    Loaded() {
        std::string error;
        ok = LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &error);
        if (!ok) std::printf("    %s\n", error.c_str());
    }
};

void Run(ScenarioRunner& r, double seconds) {
    for (int i = 0; i < static_cast<int>(seconds / 0.05 + 0.5); ++i) r.Step(0.05);
}

}  // namespace

SC_TEST("crew seats: a player in the driver's seat replaces the NPC and gets the driver's orders") {
    Loaded l;
    SC_EXPECT(l.ok);
    ScenarioRunner r(std::move(l.def), std::move(l.console));
    SC_EXPECT(r.TakeSeat("driver", "Ann"));
    SC_EXPECT(r.SeatHolder("driver") == "Ann" && r.SeatHolder("commander").empty());
    SC_EXPECT(!r.TakeSeat("driver", "Bob"));          // taken
    SC_EXPECT(r.TakeSeat("driver", "Ann"));           // the same player again: fine
    SC_EXPECT(!r.TakeSeat("navigator", "Bob"));       // no such station
    const int gta0 = r.Console().panel.Get("pwr.gta");
    Run(r, 10.0);  // the scenario orders power_up to the Driver at once
    SC_EXPECT(r.Console().panel.Get("pwr.gta") == gta0);  // the NPC did not do it
    const ProcedureTracker* t = r.SeatTracker("driver");
    SC_EXPECT(t != nullptr && t->GetProcedure().id == "power_up");
    SC_EXPECT(r.Debrief().find("order for the player") != std::string::npos);
    // Ann does the first step herself
    const StepResult res = r.SeatSet("driver", "pwr.gta", 1);
    SC_EXPECT(res == StepResult::Accepted);
    SC_EXPECT(r.Console().panel.Get("pwr.gta") == 1);
    SC_EXPECT(r.Debrief().find("Ann") != std::string::npos);
}

SC_TEST("crew seats: a player works only the controls of their own station") {
    Loaded l;
    ScenarioRunner r(std::move(l.def), std::move(l.console));
    SC_EXPECT(r.TakeSeat("commander", "Cid"));
    // a control at the driver's station
    std::string why;
    const std::string other = "pwr.gta";
    const int before = r.Console().panel.Get(other);
    SC_EXPECT(r.SeatSet("commander", other, 1, &why) == StepResult::Blocked);
    SC_EXPECT(r.Console().panel.Get(other) == before);
    SC_EXPECT(why.find("driver") != std::string::npos);
    std::printf("    %s\n", why.c_str());
    // nobody in the operator's seat: refused
    SC_EXPECT(r.SeatSet("operator", other, 1, &why) == StepResult::Blocked && why.find("nobody") != std::string::npos);
    // a wheel at another station is refused too
    SC_EXPECT(!r.SeatTurn("commander", "ssc.az_wheel", 10.0, &why));
}

SC_TEST("crew seats: a wheel that won't turn says why: its lock, or its stop") {
    Loaded l;
    ScenarioRunner r(std::move(l.def), std::move(l.console));
    SC_EXPECT(r.TakeSeat("senior_operator", "Ann"));
    std::string why;
    r.Console().panel.Set("ssc.az_lock", 0);
    SC_EXPECT(!r.SeatTurn("senior_operator", "ssc.az_wheel", 10.0, &why));
    SC_EXPECT(why.find("Locked") == 0 && why.find("azimuth wheel lock") != std::string::npos);
    r.Console().panel.Set("ssc.az_lock", 1);
    SC_EXPECT(r.SeatTurn("senior_operator", "ssc.az_wheel", 330.0, &why));
    // On past the antenna's +330 degrees: nothing moves, and the reason is the stop, not a lock.
    why.clear();
    SC_EXPECT(!r.SeatTurn("senior_operator", "ssc.az_wheel", 335.0, &why));
    SC_EXPECT(why.find("At its stop (330)") == 0);
    SC_EXPECT_NEAR(r.Console().panel.GetValue("ssc.az_wheel"), 330.0, 1e-9);
    SC_EXPECT(r.SeatTurn("senior_operator", "ssc.az_wheel", 320.0, &why));  // back off the stop
    // Pushed past the stop from inside the travel, it stops there.
    SC_EXPECT(r.SeatTurn("senior_operator", "ssc.az_wheel", 340.0, &why));
    SC_EXPECT_NEAR(r.Console().panel.GetValue("ssc.az_wheel"), 330.0, 1e-9);
}

SC_TEST("crew seats: a player at an aiming seat stops the NPC's aiming; leaving hands it back") {
    Loaded l;
    ScenarioRunner r(std::move(l.def), std::move(l.console));
    const AimCrew all = r.AimFlags();
    SC_EXPECT(all.azimuth || all.elevation || all.range);
    // whoever aims the azimuth wheel in this console
    const std::vector<std::string> roles = {"senior_operator", "deputy_commander", "operator"};
    for (const std::string& role : roles) {
        SC_EXPECT(r.TakeSeat(role, "P-" + role));
    }
    const AimCrew none = r.AimFlags();
    SC_EXPECT(!none.azimuth && !none.elevation && !none.range);
    for (const std::string& role : roles) r.LeaveSeat(role);
    const AimCrew back = r.AimFlags();
    SC_EXPECT(back.azimuth == all.azimuth && back.elevation == all.elevation && back.range == all.range);
    SC_EXPECT(r.SeatHolder("operator").empty());
    SC_EXPECT(r.Debrief().find("leaves the seat") != std::string::npos);
}

SC_TEST("crew seats: a player's machine mirrors the server's console from snapshots") {
    Loaded server_l;
    Loaded client_l;
    SC_EXPECT(server_l.ok && client_l.ok);
    ScenarioRunner server(std::move(server_l.def), std::move(server_l.console));
    ScenarioRunner client(std::move(client_l.def), std::move(client_l.console));  // never stepped
    Run(server, 60.0);  // the crew powers up: switches move, subsystems start
    std::uint32_t seq = 0;
    double t = 0.0;
    const std::vector<std::uint8_t> snap = server.ConsoleSnapshot(1);
    std::printf("    console snapshot: %zu bytes\n", snap.size());
    SC_EXPECT(snap.size() < 1024);
    SC_EXPECT(ApplyConsoleState(snap, client.Console(), &seq, &t));
    SC_EXPECT(seq == 1 && std::fabs(t - server.Time()) < 0.002);
    SC_EXPECT(client.Console().panel.Positions() == server.Console().panel.Positions());
    const auto& sv = server.Console().panel.Values();
    const auto& cv = client.Console().panel.Values();
    bool values_close = sv.size() == cv.size();
    for (std::size_t i = 0; values_close && i < sv.size(); ++i) values_close = std::fabs(sv[i] - cv[i]) < 1e-4;
    SC_EXPECT(values_close);
    const auto su = server.Console().equipment.UnitStates();
    const auto cu = client.Console().equipment.UnitStates();
    bool units_same = su.size() == cu.size();
    for (std::size_t i = 0; units_same && i < su.size(); ++i)
        units_same = su[i].state == cu[i].state && std::fabs(su[i].progress - cu[i].progress) < 2e-5;
    SC_EXPECT(units_same);
    SC_EXPECT(client.Console().equipment.Inputs() == server.Console().equipment.Inputs());
    int ready = 0;
    for (const auto& u : cu) ready += u.state == SubsystemState::Ready ? 1 : 0;
    std::printf("    %d subsystems ready on the mirror\n", ready);
    SC_EXPECT(ready > 0);
    // the same or an older snapshot changes nothing; a damaged or foreign one is refused
    SC_EXPECT(!ApplyConsoleState(snap, client.Console(), &seq));
    std::vector<std::uint8_t> bad = snap;
    bad[5] ^= 0x5a;  // the fingerprint
    std::uint32_t fresh = 0;
    SC_EXPECT(!ApplyConsoleState(bad, client.Console(), &fresh));
    std::vector<std::uint8_t> cut(snap.begin(), snap.end() - 3);
    SC_EXPECT(!ApplyConsoleState(cut, client.Console(), &fresh));
}
