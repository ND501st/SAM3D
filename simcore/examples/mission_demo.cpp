// mission_demo: play a battery mission headless (roadmap 14): the regiment's combat task to the battery commander, his
// plan timed to its ready-by, his orders on the battery net, the march and the formation, and «к бою готова» up to the
// regiment. Prints the order and the plan as the papers in his map case, then what happened against the plan. With
// --war, the enemy's air campaign runs beside it on the mission's clock and its packages come in live (roadmap 15.2,
// 15.3: campaign/mission_session.hpp), and the regiment tasks the battery on what our intelligence estimates of it
// (roadmap 14.2: data/campaign/tasking.json). The director (roadmap 15.4) counts what woke the battery commander and how
// long the quiet between was.
//
// Usage: mission_demo [scenario.json] [data_dir] [--war faction [seed [day]]] [--hours h] [--theatre file] [--fronts]
//   defaults: data/scenarios/pu12m_mission_cover.json, data (run from the repo root); seed 1, the war's first day;
//   --hours: the session held that long from its start (the task through the day), not to the scenario's end;
//   --theatre: the war's theatre in data/air_war/ (theatre_pannonia.json); --fronts: track F's modes at 1 (the ground war,
//   the air front, our air defence commanders) and the regiment following them on what reached it (F4.4; needs a theatre
//   with a country map: --theatre theatre_pannonia_nation.json)
// Exits non-zero if the scenario fails to load, has no task, or the battery was not reported ready by the ready-by;
// with --war also if no package came in.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "simcore/campaign/director.hpp"
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
        if (a == "--theatre" && i + 1 < argc) {
            md.theatre = std::string("air_war/") + argv[++i];
            continue;
        }
        if (a == "--fronts") {
            md.fronts = true;
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
    campaign::Director director(r.Cmdr());
    while (!r.Finished() && (s.Campaign() != nullptr || md.hours > 0.0 || ready_seen_s < 0.0 || r.Time() < ready_seen_s + 60.0)) {
        s.Step(0.1);
        director.Step(r);
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
    // Track F, F4.4: what reached the regiment of the fronts, the tasks it gave on it and how long each took to the battery.
    if (s.Watch().Bound() && s.Campaign() != nullptr) {
        const double h0 = s.CampaignHourAtStart();
        const auto clock = [&](double h) { return h < h0 ? std::string("before the session") : r.TaskClock((h - h0) * 3600.0); };
        const campaign::RegimentPicture held = s.RegimentHolds();
        const std::string priority_from = held.priority_h < 0.0 ? std::string("his pre-war plan") : "his order at the regiment " + clock(held.priority_h);
        const std::string sharing_from = held.sharing_h < 0.0 ? std::string("the pre-war plan") : "the order at the regiment " + clock(held.sharing_h);
        std::printf("==== The regiment and the fronts (F4.4)\n");
        std::printf("  it holds: the division's sector %s, %s by the army's chief of air defence (%s); the sky shared %s (%s); %d withdrawal(s) of the division\n",
                    held.sector.c_str(), held.priority.c_str(), priority_from.c_str(), command::CooperationMethodName(held.sharing), sharing_from.c_str(),
                    held.withdrawals);
        int in_session = 0;
        for (const campaign::RegimentArrival& a : s.Watch().Arrivals()) {
            if (a.reached_h < h0) continue;
            ++in_session;
            std::printf("  reached the regiment %s: %s %s (its hour %s)\n", clock(a.reached_h).c_str(), a.what.c_str(), a.key.c_str(), clock(a.compiled_h).c_str());
        }
        if (in_session == 0) std::printf("  nothing of the fronts reached the regiment in the session\n");
        for (const campaign::MissionSession::GivenTask& g : s.TasksGiven()) {
            if (g.rule == "cover_priority_object" || g.rule == "relocate_when_known") continue;
            const std::string fact = g.fact_s >= 0.0 ? r.TaskClock(g.fact_s) : std::string("before the session");
            const std::string heard = g.heard_s >= 0.0 ? r.TaskClock(g.heard_s) : std::string("never");
            std::printf("  %s (rule %s): the fact at the regiment %s, the order given %s, heard at the battery %s", g.task.c_str(), g.rule.c_str(), fact.c_str(),
                        r.TaskClock(g.at_s).c_str(), heard.c_str());
            if (g.fact_s >= 0.0 && g.heard_s >= 0.0) {
                std::printf(": %.1f min from the fact at the regiment (%.1f of it to the order)", (g.heard_s - g.fact_s) / 60.0, (g.at_s - g.fact_s) / 60.0);
            }
            std::printf("\n");
        }
        const std::string told = r.SharingHeardS() >= 0.0 ? "heard " + r.TaskClock(r.SharingHeardS()) : std::string("no");
        std::printf("  the battery marched to a new position on the regiment's tasks: %s; the sky's sharing told: %s (the plan of cooperation now %s, %d change(s) heard)\n",
                    s.BatteryMoves() > 0 ? "yes" : "no", told.c_str(), command::CooperationMethodName(r.Task().own_air.method), r.SharingChanges());
        std::printf("  the pipeline: %zu facts, %zu still looked for at the regiment\n", s.Net() != nullptr ? s.Net()->Facts().size() : std::size_t{0},
                    s.Watch().Waiting());
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
    std::printf("==== The director\n  %s", director.Text(r.Time()).c_str());
    return r.TaskReadyS() >= 0.0 && r.TaskReadyS() <= p.ready_by_s && war_ok ? 0 : 1;
}
