// mission_demo: play a battery mission headless (roadmap 14): the regiment's combat task to the battery commander, his
// plan timed to its ready-by, his orders on the battery net, the march and the formation, and «к бою готова» up to the
// regiment. Prints the order and the plan as the papers in his map case, then what happened against the plan.
//
// Usage: mission_demo [scenario.json] [data_dir]
//   defaults: data/scenarios/pu12m_mission_cover.json, data (run from the repo root)
// Exits non-zero if the scenario fails to load, has no task, or the battery was not reported ready by the ready-by.
#include <cstdio>
#include <string>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;

int main(int argc, char** argv) {
    const std::string path = argc > 1 ? argv[1] : "data/scenarios/pu12m_mission_cover.json";
    const std::string data_dir = argc > 2 ? argv[2] : "data";

    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenarioFile(path, data_dir, def, console, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (!def.has_task) {
        std::fprintf(stderr, "%s: no \"task\" (a battery mission starts with the regiment's task)\n", path.c_str());
        return 1;
    }
    ScenarioRunner r(std::move(def), std::move(console));
    // To «к бою готова» up to the regiment, and a minute after it; or to the scenario's end.
    double ready_seen_s = -1.0;
    while (!r.Finished() && (ready_seen_s < 0.0 || r.Time() < ready_seen_s + 60.0)) {
        r.Step(0.1);
        if (ready_seen_s < 0.0 && r.TaskReadyS() >= 0.0) ready_seen_s = r.Time();
    }
    for (const HeldPaper& h : r.MapCase("battery_commander")) {
        if (h.title.find("Combat order") == std::string::npos && h.title.find("Plan for the task") == std::string::npos) continue;
        std::printf("==== %s\n%s\n", h.title.c_str(), h.text.c_str());
    }
    const campaign::BatteryPlan& p = r.TaskPlan();
    std::printf("==== Against the plan\n");
    std::printf("  received      %s\n", r.TaskReceivedS() >= 0.0 ? r.TaskClock(r.TaskReceivedS()).c_str() : "never");
    std::printf("  the order     %s (planned)\n", r.TaskClock(p.order_s).c_str());
    if (p.march_at_s >= 0.0) std::printf("  at the place  %s (planned)\n", r.TaskClock(p.arrive_s).c_str());
    std::printf("  ready         %s (planned), by %s\n", r.TaskClock(p.ready_s).c_str(), r.TaskClock(p.ready_by_s).c_str());
    if (r.TaskReadyS() >= 0.0) {
        const double spare_min = (p.ready_by_s - r.TaskReadyS()) / 60.0;
        std::printf("  reported      %s: %s %.0f min\n", r.TaskClock(r.TaskReadyS()).c_str(), spare_min >= 0.0 ? "in time," : "late by",
                    spare_min >= 0.0 ? spare_min : -spare_min);
    } else {
        std::printf("  reported      never\n");
    }
    // The debrief's account of the task.
    const std::string debrief = r.Debrief();
    const std::size_t at = debrief.find("The task (");
    if (at != std::string::npos) {
        const std::size_t end = debrief.find("\n\n", at);
        std::printf("==== The debrief\n%s\n", debrief.substr(at, end == std::string::npos ? std::string::npos : end - at).c_str());
    }
    return r.TaskReadyS() >= 0.0 && r.TaskReadyS() <= p.ready_by_s ? 0 : 1;
}
