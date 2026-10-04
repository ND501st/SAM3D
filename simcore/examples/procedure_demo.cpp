// procedure_demo: an Osa-AK console session from the march to the debrief.
//
// Loads data/procedures/osa_ak.json and plays a short scripted session:
//   0 s    commander starts the GTA; the senior operator searches on the march
//   90 s   the vehicle stops; commander deploys, the driver puts the generator on line; the
//          senior operator and deputy commander run the TV-only engagement, but a scripted
//          "player" in the deputy commander's seat does the key and launch, turning the key too early
//   stow   crew stows for the march and the vehicle drives off
// then prints the debrief timeline. Target geometry isn't simulated here: the sim inputs
// are set by the script.
//
// Usage: procedure_demo [path/to/osa_ak.json]   (default: data/procedures/osa_ak.json, run from the repo root)
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/debrief.hpp"

using namespace simcore;

namespace {

struct Session {
    ConsoleData console;
    std::vector<CrewMember> crew;  // one per console station
    SessionLog log;
    double t = 0.0;

    // One 0.1 s step in the documented order.
    void Step() {
        t += 0.1;
        console.equipment.Update(0.1, console.panel);
        console.panel.ReleaseMomentaries();
        for (CrewMember& m : crew) m.Update(0.1, console.panel, &console.equipment);
        log.CollectEquipment(console.equipment);
        for (CrewMember& m : crew) log.CollectCrew(m, t);
    }

    bool Busy() const {
        for (const CrewMember& m : crew) {
            if (m.IsBusy()) return true;
        }
        return false;
    }

    // The crew member in the seat that does this procedure.
    CrewMember& At(const Procedure& p) {
        for (CrewMember& m : crew) {
            if (m.Role() == p.station) return m;
        }
        return crew.front();
    }

    void Order(const Procedure& p) { At(p).Order(p); }

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
    std::printf("%s: %zu stations, %zu controls, %zu procedures\n\n", s.console.display_name.c_str(),
                s.console.stations.size(), s.console.panel.Controls().size(), s.console.procedures.size());
    for (const ConsoleStation& st : s.console.stations) s.crew.emplace_back(st.label, st.id, 1.0);

    // On the march: power up and search while moving.
    s.Order(s.Proc("power_up"));
    s.Order(s.Proc("search_on_march"));
    s.RunFor(90.0);

    // Halt and come into action. The radars stay silent: the senior operator turns to the
    // plotting-board azimuth instead of searching.
    s.console.equipment.SetInput("vehicle_stopped", true);
    s.Order(s.Proc("deploy_at_halt"));
    s.Order(s.Proc("pto_on_line"));
    s.Order(s.Proc("stop_search"));
    s.RunUntil([&] { return !s.Busy(); }, 30.0);
    s.Order(s.Proc("emcon_tv_azimuth"));
    // The deputy commander's NPC does everything up to the key; the "player" takes over from there.
    Procedure crew_part = s.Proc("emcon_tv_engagement");
    crew_part.id = "emcon_tv_engagement (crew part)";
    crew_part.steps.resize(crew_part.steps.size() - 2);
    s.Order(crew_part);
    if (!s.RunUntil([&] { return !s.Busy(); }, 60.0)) {
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
    s.Order(s.Proc("stow_launch_console"));
    s.Order(s.Proc("pto_off_line"));
    s.Order(s.Proc("stow_for_march"));
    s.RunUntil([&] { return !s.Busy(); }, 60.0);
    s.RunFor(1.0);
    s.console.equipment.SetInput("vehicle_stopped", false);
    s.RunFor(5.0);

    std::printf("%s", s.log.Format().c_str());
    const DebriefSummary sum = s.log.Summary();
    return sum.equipment_faults == 0 && sum.early_actions == 1 ? 0 : 1;
}
