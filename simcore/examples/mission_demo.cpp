// mission_demo: play a battery mission headless (roadmap 14): the regiment's combat task to the battery commander, his
// plan timed to its ready-by, his orders on the battery net, the march and the formation, and «к бою готова» up to the
// regiment. Prints the order and the plan as the papers in his map case, then what happened against the plan. With
// --war, the enemy's air campaign runs beside it on the mission's clock and its packages come in live (roadmap 15.2,
// 15.3: campaign/mission_session.hpp), and the regiment tasks the battery on what our intelligence estimates of it
// (roadmap 14.2: data/campaign/tasking.json).
//
// Usage: mission_demo [scenario.json] [data_dir] [--war faction [seed [day]]] [--hours h]
//   defaults: data/scenarios/pu12m_mission_cover.json, data (run from the repo root); seed 1, the war's first day;
//   --hours: the session held that long from its start (the task through the day), not to the scenario's end
// Exits non-zero if the scenario fails to load, has no task, or the battery was not reported ready by the ready-by;
// with --war also if no package came in.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "simcore/campaign/mission_session.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;

int main(int argc, char** argv) {
    std::vector<std::string> args;
    campaign::MissionSessionDef md;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--war" && i + 1 < argc) {
            md.faction = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] != '-') md.seed = static_cast<unsigned>(std::atoi(argv[++i]));
            if (i + 1 < argc && argv[i + 1][0] != '-') md.day = std::atoi(argv[++i]);
            md.tasking = "campaign/tasking.json";
            continue;
        }
        if (a == "--hours" && i + 1 < argc) {
            md.hours = std::atof(argv[++i]);
            continue;
        }
        args.push_back(a);
    }
    md.scenario = args.size() > 0 ? args[0] : "data/scenarios/pu12m_mission_cover.json";
    const std::string data_dir = args.size() > 1 ? args[1] : "data";

    campaign::MissionSession s;
    std::string error;
    if (!s.Start(md, data_dir, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    ScenarioRunner& r = s.Runner();
    if (!r.Def().has_task) {
        std::fprintf(stderr, "%s: no \"task\" (a battery mission starts with the regiment's task)\n", md.scenario.c_str());
        return 1;
    }
    // Without the war: to «к бою готова» up to the regiment, and a minute after it. With it: to the scenario's end.
    double ready_seen_s = -1.0;
    while (!r.Finished() && (s.Campaign() != nullptr || md.hours > 0.0 || ready_seen_s < 0.0 || r.Time() < ready_seen_s + 60.0)) {
        s.Step(0.1);
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
    bool war_ok = true;
    if (const airwar::LivePackages* live = s.Packages()) {
        std::printf("==== The enemy's packages (%s, seed %u, day %d)\n", md.faction.c_str(), md.seed, md.day);
        for (const airwar::LivePackages::Played& pk : live->PlayedPackages()) {
            std::printf("  package %d '%s': %zu aircraft, in at %s, back at %s, %d shot down\n", pk.package_id, pk.name.c_str(), pk.aircraft,
                        r.TaskClock(pk.in_s).c_str(), pk.out_s >= 0.0 ? r.TaskClock(pk.out_s).c_str() : "still in", pk.lost);
        }
        for (const std::string& n : live->Notes()) std::printf("  (%s)\n", n.c_str());
        std::printf("  the battery's new position is the campaign's: %s\n", s.BatteryMoved() ? "yes" : "not yet");
        war_ok = !live->PlayedPackages().empty();
    }
    if (s.Regiment() != nullptr) {
        std::printf("==== The regiment's tasking\n");
        const campaign::RegimentPicture pic = s.Campaign() != nullptr ? campaign::PictureFromJournal(*s.Campaign()) : campaign::RegimentPicture();
        std::printf("  its latest estimate: %s\n", pic.estimate_h >= 0.0 ? pic.estimate.c_str() : "none yet");
        for (const campaign::MissionSession::GivenTask& g : s.TasksGiven()) {
            std::printf("  %s: %s (rule %s)\n", r.TaskClock(g.at_s).c_str(), g.task.c_str(), g.rule.c_str());
        }
        if (s.TasksGiven().empty()) std::printf("  no new task\n");
        for (const std::string& n : s.Notes()) std::printf("  (%s)\n", n.c_str());
    }
    // The debrief's account of the task.
    const std::string debrief = r.Debrief();
    const std::size_t at = debrief.find("The task (");
    if (at != std::string::npos) {
        const std::size_t end = debrief.find("\n\n", at);
        std::printf("==== The debrief\n%s\n", debrief.substr(at, end == std::string::npos ? std::string::npos : end - at).c_str());
    }
    // The player's two outcomes (roadmap 14.6), from the truth.
    const std::string with_outcomes = s.Debrief();
    const std::size_t outcomes_at = with_outcomes.find("The battery's two outcomes");
    if (outcomes_at != std::string::npos) std::printf("==== %s", with_outcomes.substr(outcomes_at).c_str());
    return r.TaskReadyS() >= 0.0 && r.TaskReadyS() <= p.ready_by_s && war_ok ? 0 : 1;
}
