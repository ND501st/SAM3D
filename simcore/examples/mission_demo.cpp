// mission_demo: play a battery mission headless (roadmap 14): the regiment's combat task to the battery commander, his
// plan timed to its ready-by, his orders on the battery net, the march and the formation, and «к бою готова» up to the
// regiment. Prints the order and the plan as the papers in his map case, then what happened against the plan. With
// --war, the enemy's air campaign runs beside it on the mission's clock and its packages come in live (roadmap 15.2,
// 15.3: campaign/mission_session.hpp), and the regiment tasks the battery on what our intelligence estimates of it
// (roadmap 14.2: data/campaign/tasking.json). The director (roadmap 15.4) counts what woke the battery commander and how
// long the quiet between was.
//
// Usage: mission_demo [scenario.json] [data_dir] [--war faction [seed [day]]] [--hours h] [--theatre file] [--fronts [--u2-km km]] [--reveal]
//        mission_demo --campaign <adversary> [data_dir] [--hours h] [--reveal]: the game's campaign as the menu starts it
//   (data/campaign/campaigns.json: its mission, war, theatre, tasking and fronts; track O, O6), at the battery commander's seat
//   defaults: data/scenarios/pu12m_mission_cover.json, data (run from the repo root); seed 1, the war's first day;
//   --hours: the session held that long from its start (the task through the day), not to the scenario's end;
//   --theatre: the war's theatre in data/air_war/ (theatre_pannonia.json); --fronts: track F's modes on (the ground war
//   and our air defence commanders at 1, the air front at 2, following its own ground offensive: F7.3) and the regiment following them on what reached it (F4.4; needs a theatre
//   with a country map: --theatre theatre_pannonia_nation.json); with the fronts, the ground war's units near the battery
//   at U2 and the front heard from afar (F5.2); --u2-km: how near is near (20 km; battery 21 stands about 100 km from
//   the nearest regiment in the Pact's war, so nothing comes down there unless widened); --reveal: at the end, what our
//   side believed beside the truth (track O, O5: campaign/reveal.hpp)
// Exits non-zero if the scenario fails to load, has no task, or the battery was not reported ready by the ready-by;
// with --war also if no package came in.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "simcore/campaign/campaign_save.hpp"
#include "simcore/campaign/director.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/campaign/reveal.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;

int main(int argc, char** argv) {
    std::vector<std::string> args;
    bool reveal = false;
    std::string campaign_id;
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
        if (a == "--reveal") {
            reveal = true;
            continue;
        }
        if (a == "--fronts") {
            md.fronts = true;
            continue;
        }
        if (a == "--u2-km" && i + 1 < argc) {
            md.u2_radius_m = std::atof(argv[++i]) * 1000.0;
            continue;
        }
        if (a == "--campaign" && i + 1 < argc) {
            campaign_id = argv[++i];
            continue;
        }
        args.push_back(a);
    }
    md.scenario = args.size() > 0 ? args[0] : "data/scenarios/pu12m_mission_cover.json";
    // With --campaign the mission is the campaign's: the one argument is the data folder.
    const std::string data_dir = campaign_id.empty() ? (args.size() > 1 ? args[1] : "data") : (args.empty() ? "data" : args[0]);
    if (!campaign_id.empty()) {
        // Track O, O6: the game's campaign as the menu starts it (data/campaign/campaigns.json), headless.
        campaign::CampaignsData cd;
        campaign::CampaignSave header;
        std::string e;
        const double hours = md.hours;
        if (!campaign::LoadCampaignsData(data_dir, "campaign/campaigns.json", cd, &e) ||
            !campaign::NewCampaign(cd, campaign_id, data_dir, "battery_commander", md, header, &e)) {
            std::fprintf(stderr, "%s\n", e.c_str());
            return 1;
        }
        md.hours = hours > 0.0 ? hours : 24.0;  // the war's first day unless asked for more (the campaign's is its whole length)
        md.seams = false;
        std::printf("==== The campaign against %s: %s (war seed %u)\n", campaign_id.c_str(), header.mission.c_str(), md.seed);
    }

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
    // Track O, O6: the march with the division: where the battery and the regiment's command post stand at the end.
    if (const nav::SessionNav* nav = r.Nav()) {
        const nav::Anchor* main = nav->GetAnchors().Find(r.Task().main);
        const Vec3 at = r.UnitPosition();
        std::printf("==== The march (O6)\n");
        if (main != nullptr) {
            std::printf("  the post at the end: %.1f km from the task's main position (%s)\n",
                        std::hypot(at.x - main->at.east_m, at.y - main->at.north_m) / 1000.0, r.PlaceNameOf(r.Task().main).c_str());
        }
        std::printf("  the regiment's command post: %s%s\n", r.PlaceNameOf(s.RegimentCp()).c_str(),
                    s.RegimentGoing().empty() ? "" : (", its column on the way to " + r.PlaceNameOf(s.RegimentGoing())).c_str());
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
    // Track F, F5.2 (F8.3): the ground war's units at U2 in the session and the front heard and seen from the battery.
    if (s.GroundUnits().Started()) {
        const groundwar::SessionUnits& u2 = s.GroundUnits();
        int mine = 0, theirs = 0, mine_seen = 0, theirs_seen = 0;
        for (const groundwar::SessionUnits::Held& h : u2.All()) (h.side == groundwar::Side::Ours ? mine : theirs) += 1;
        for (const ScenarioRunner::WorldMover& v : r.MoversInView()) {
            const nav::Mover* m = r.Nav() != nullptr ? r.Nav()->GetMovers().Find(v.mover) : nullptr;
            if (m != nullptr && m->ground_war) (v.enemy ? theirs_seen : mine_seen) += 1;
        }
        std::printf("==== The ground war in sight (F5.2)\n");
        std::printf("  U2: %zu units made (%d ours, %d the enemy's), %zu near the battery at the end; %zu marches, %zu on the roads\n", u2.Made(), mine,
                    theirs, u2.Near(), u2.Marches(), u2.RoadSearches());
        std::printf("  in sight at the end: %d of our vehicles, %d of the enemy's\n", mine_seen, theirs_seen);
        std::printf("  the front from afar: %d salvos heard, %d fires seen\n", s.FrontSalvosHeard(), s.FrontFiresSeen());
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
    // Track O, O5: the regiment's summings up at its briefings, from the battery's records; the crews' marks.
    if (!s.Summings().empty() || !s.CrewGradesAll().empty()) {
        std::printf("==== The summing up (O5)\n");
        for (const campaign::SummingUp& u : s.Summings()) {
            std::printf("  %s:\n", r.TaskClock(u.at_s).c_str());
            for (const std::string& l : u.lines) std::printf("    %s\n", l.c_str());
        }
        for (const campaign::CrewGrade& c : s.CrewGradesAll()) std::printf("  %s\n", campaign::CrewGradeText(c).c_str());
    }
    if (reveal) std::printf("==== %s", campaign::MakeReveal(s).Text().c_str());
    return r.TaskReadyS() >= 0.0 && r.TaskReadyS() <= p.ready_by_s && war_ok ? 0 : 1;
}
