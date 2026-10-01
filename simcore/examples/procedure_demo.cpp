// procedure_demo: an Osa-AK console session from the march to the debrief.
//
// Loads data/procedures/osa_ak.json and plays a short scripted session:
//   0 s    commander starts the GTA and searches on the march
//   90 s   the vehicle stops; commander deploys; firing officer runs the TV-only engagement,
//          but a scripted "player" does the key and launch, turning the key too early
//   stow   crew stows for the march and the vehicle drives off
// then prints the debrief timeline. Target geometry isn't simulated here: the sim inputs
// are set by the script.
//
// Usage: procedure_demo [path/to/osa_ak.json]   (default: data/procedures/osa_ak.json, run from the repo root)
#include <cstdio>
#include <string>

#include "simcore/console_data.hpp"
#include "simcore/debrief.hpp"

using namespace simcore;

namespace {

struct Session {
    ConsoleData console;
    CrewMember commander{"Commander", "commander", 1.0};
    CrewMember firing{"Firing officer", "firing_officer", 1.0};
    SessionLog log;
    double t = 0.0;

    // One 0.1 s step in the documented order.
    void Step() {
        t += 0.1;
        console.equipment.Update(0.1, console.panel);
        console.panel.ReleaseMomentaries();
        commander.Update(0.1, console.panel, &console.equipment);
        firing.Update(0.1, console.panel, &console.equipment);
        log.CollectEquipment(console.equipment);
        log.CollectCrew(commander, t);
        log.CollectCrew(firing, t);
    }

    void RunFor(double seconds) {
        for (const double end = t + seconds; t < end - 1e-9;) Step();
    }

    template <typename Pred>
    bool RunUntil(Pred done, double limit_s) {
        for (const double end = t + limit_s; !done() && t < end;) Step();
        return done();
    }

    const Procedure& Proc(const char* id) const { return *console.FindProcedure(id); }
};

}  // namespace

int main(int argc, char** argv) {
    const std::string path = argc > 1 ? argv[1] : "data/procedures/osa_ak.json";
    Session s;
    std::string error;
    if (!LoadConsoleFile(path, s.console, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::printf("%s: %zu controls, %zu procedures\n\n", s.console.display_name.c_str(),
                s.console.panel.Controls().size(), s.console.procedures.size());

    // On the march: power up and search while moving.
    s.commander.Order(s.Proc("power_up"));
    s.commander.Queue(s.Proc("search_on_march"));
    s.RunFor(90.0);

    // Halt and come into action.
    s.console.equipment.SetInput("vehicle_stopped", true);
    s.commander.Order(s.Proc("deploy_at_halt"));
    // The firing officer does everything up to the key; the "player" takes over from there.
    Procedure crew_part = s.Proc("emcon_tv_engagement");
    crew_part.id = "emcon_tv_engagement (crew part)";
    crew_part.steps.resize(crew_part.steps.size() - 2);
    s.firing.Order(crew_part);
    if (!s.RunUntil([&] { return !s.firing.IsBusy() && !s.commander.IsBusy(); }, 60.0)) {
        std::fprintf(stderr, "crew did not finish deploying\n");
        return 1;
    }

    // The player turns the key and presses ПУСК using the same tracker a cabin would use.
    Procedure player_part = s.Proc("emcon_tv_engagement");
    player_part.id = "emcon_tv_engagement (player part)";
    player_part.steps.erase(player_part.steps.begin(), player_part.steps.end() - 2);
    ProcedureTracker player(player_part);
    s.RunFor(3.0);
    StepResult r = player.Attempt(s.console.panel, "fo.key", 2, &s.console.equipment);  // too early: ПУСК ГОТОВ not lit
    s.log.RecordAttempt(s.t, "Player", player, r);
    if (!s.RunUntil([&] { return s.console.equipment.IsReady("launch_armed"); }, 30.0)) {
        std::fprintf(stderr, "launch circuit never armed\n");
        return 1;
    }
    s.RunFor(1.0);
    r = player.Attempt(s.console.panel, "fo.launch", 1, &s.console.equipment);
    s.log.RecordAttempt(s.t, "Player", player, r);
    s.log.Record({s.t, DebriefEntry::Kind::Completed, "Player", player_part.id, "ПУСК"});
    s.RunFor(10.0);

    // Take the key out, stow and drive off.
    s.console.panel.Set("fo.key", 1);
    s.console.panel.Set("fo.key", 0);
    s.firing.Order(s.Proc("stow_for_march"));
    s.RunUntil([&] { return !s.firing.IsBusy(); }, 60.0);
    s.RunFor(1.0);
    s.console.equipment.SetInput("vehicle_stopped", false);
    s.RunFor(5.0);

    std::printf("%s", s.log.Format().c_str());
    const DebriefSummary sum = s.log.Summary();
    return sum.equipment_faults == 0 && sum.early_actions == 1 ? 0 : 1;
}
