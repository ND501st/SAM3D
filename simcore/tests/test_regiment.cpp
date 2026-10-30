// The regiment's tasking (roadmap 14.2, first form): simcore/campaign/regiment.hpp, data/campaign/tasking.json, and the
// regiment's next task given into a running session (ScenarioRunner::GiveTask, campaign/mission_session.hpp).
#include <cmath>
#include <cstdio>
#include <string>

#include "simcore/air_war.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/campaign/regiment.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

campaign::RegimentPicture Knows(double h) {
    campaign::RegimentPicture p;
    p.estimate_h = h;
    p.enemy_knows_battery = true;
    p.estimate = "Day 2: the enemy probably knows the battery's position: consider moving it.";
    return p;
}

}  // namespace

SC_TEST("regiment (14.2): the tasking rules load, each with its reason, and the routine one's task file with them") {
    campaign::TaskingRules rules;
    std::string e;
    SC_EXPECT(campaign::LoadTaskingRules(kData, "campaign/tasking.json", rules, &e));
    SC_EXPECT_NEAR(rules.briefing_hour, 19.0, 1e-9);
    SC_EXPECT(rules.rules.size() == 2);
    if (rules.rules.size() != 2) return;
    const campaign::TaskingRule& cover = rules.rules[0];
    const campaign::TaskingRule& move = rules.rules[1];
    SC_EXPECT(cover.when == "no_task" && cover.task == "task_file" && !cover.urgent && !cover.reason.empty());
    campaign::BatteryTask t;
    SC_EXPECT(campaign::LoadBatteryTask(kData, cover.task_file, t, &e));
    SC_EXPECT(move.when == "enemy_knows_battery" && move.task == "relocate" && move.urgent && move.ready_in_s > 0.0);
    // A rule must say what it does.
    const auto bad = [](const std::string& text) {
        json::Value root;
        std::string pe;
        if (!json::Parse(text, root, &pe)) return std::string("parse: ") + pe;
        campaign::TaskingRules r;
        campaign::detail::TaskingLoader l;
        return l.Load(root, r) ? std::string() : l.Message();
    };
    const std::string head = R"({"source": "sam3d", "confidence": "estimate", "rules": [)";
    const std::string prov = R"("source": "sam3d", "confidence": "estimate")";
    SC_EXPECT(bad(head + R"({"id": "a", "when": "no_task", "task": "relocate", "reason": "r", )" + prov + "}]}").find("ready_in_s") != std::string::npos);
    SC_EXPECT(bad(head + R"({"id": "a", "when": "no_task", "task": "task_file", "reason": "r", )" + prov + "}]}").find("task_file") != std::string::npos);
    SC_EXPECT(bad(head + R"({"id": "a", "when": "sometimes", "task": "relocate", "ready_in_s": 1, "reason": "r", )" + prov + "}]}").find("when") !=
              std::string::npos);
    SC_EXPECT(bad(R"({"source": "sam3d", "confidence": "estimate", "rules": []})").find("rules") != std::string::npos);
}

SC_TEST("regiment (14.2): the relocate task is the task held, to its reserve position, the old main one its reserve now") {
    campaign::BatteryTask now;
    std::string e;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", now, &e));
    campaign::TaskingRule rule;
    rule.reason = "Our intelligence estimates the enemy has found your position.";
    rule.ready_in_s = 2700.0;
    campaign::BatteryTask t;
    SC_EXPECT(campaign::RelocateTask(now, rule, "the enemy probably knows", t, &e));
    SC_EXPECT(t.kind == campaign::TaskKind::Relocate && t.id == "bty21_cover_bridges_relocate");
    SC_EXPECT(t.main == "bty21_bridges_reserve" && t.reserve.size() == 1 && t.reserve.front() == "bty21_bridges");
    SC_EXPECT(t.tzm_place == "bty21_bridges");  // the TZM still waits at a reserve position
    SC_EXPECT(t.covered == now.covered && t.sectors.size() == now.sectors.size() && t.emission == now.emission);
    SC_EXPECT(t.reasons.size() == 1 && t.reasons.front().find(rule.reason) == 0 && t.reasons.front().find("probably knows") != std::string::npos);
    SC_EXPECT_NEAR(t.ready_in_s, 2700.0, 1e-9);
    SC_EXPECT(!t.reconnoitre && t.task_text.find(now.task_text) != std::string::npos);
    // Moved again, it goes back, with the same name and words.
    campaign::BatteryTask back;
    SC_EXPECT(campaign::RelocateTask(t, rule, "", back, &e));
    SC_EXPECT(back.main == "bty21_bridges" && back.reserve.front() == "bty21_bridges_reserve" && back.id == t.id && back.task_text == t.task_text);
    // Troops' cover keeps its own formation; a task without a reserve position cannot be moved by this rule.
    now.kind = campaign::TaskKind::CoverTroops;
    SC_EXPECT(campaign::RelocateTask(now, rule, "", t, &e) && t.kind == campaign::TaskKind::CoverTroops);
    now.reserve.clear();
    SC_EXPECT(!campaign::RelocateTask(now, rule, "", t, &e) && e.find("reserve") != std::string::npos);
}

SC_TEST("regiment (14.2): it decides on its picture: once per estimate, only one made after the battery arrived, routine tasks at the briefing") {
    campaign::TaskingRules rules;
    std::string e;
    SC_EXPECT(campaign::LoadTaskingRules(kData, "campaign/tasking.json", rules, &e));
    campaign::RegimentTasking rgt(rules, kData);
    campaign::BatteryTask holds;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", holds, &e));
    // Nothing known: nothing given.
    SC_EXPECT(!rgt.Decide(campaign::RegimentPicture(), &holds, 10.0, true).give);
    // "May know" is not enough.
    campaign::RegimentPicture may = Knows(20.0);
    may.enemy_knows_battery = false;
    SC_EXPECT(!rgt.Decide(may, &holds, 10.0, false).give);
    // An estimate from before the battery came to this position is about the old one.
    SC_EXPECT(!rgt.Decide(Knows(9.0), &holds, 10.0, false).give);
    // Urgent: at once, not waiting for the briefing.
    const campaign::TaskingDecision d = rgt.Decide(Knows(20.0), &holds, 10.0, false);
    SC_EXPECT(d.give && d.rule == "relocate_when_known" && d.task.main == "bty21_bridges_reserve");
    // The same estimate does not move it again; the next one does.
    SC_EXPECT(!rgt.Decide(Knows(20.0), &d.task, 10.0, false).give);
    SC_EXPECT(rgt.Decide(Knows(44.0), &d.task, 10.0, false).give);
    // Without a task: the cover task, but only at the evening briefing.
    SC_EXPECT(!rgt.Decide(campaign::RegimentPicture(), nullptr, 0.0, false).give);
    const campaign::TaskingDecision c = rgt.Decide(campaign::RegimentPicture(), nullptr, 0.0, true);
    SC_EXPECT(c.give && c.rule == "cover_priority_object" && c.task.id == "bty21_cover_bridges");
    SC_EXPECT(!c.task.reasons.empty() && c.task.reasons.front() == rules.rules[0].reason);
}

SC_TEST("regiment (14.2): its picture is our intelligence's latest estimate in the campaign's journal") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    SC_EXPECT(campaign::PictureFromJournal(c).estimate_h < 0.0);  // none yet
    while (c.Advance(3.0 * 24.0)) c.SkipPlayerMission();
    const campaign::RegimentPicture p = campaign::PictureFromJournal(c);
    SC_EXPECT(p.estimate_h > 0.0 && p.estimate.find("the battery's position") != std::string::npos);
    SC_EXPECT(p.enemy_knows_battery == (p.estimate.find("probably knows") != std::string::npos));
    // The latest of the visible estimates.
    double last = -1.0;
    for (const airwar::JournalEntry& j : c.Journal()) {
        if (j.visible && j.kind == "own_intel" && j.text.find("the battery's position") != std::string::npos) last = j.h;
    }
    SC_EXPECT_NEAR(p.estimate_h, last, 1e-9);
}

SC_TEST("regiment (14.2): told the enemy knows the battery at the bridges, the regiment moves it to the reserve; it reports ready there") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.tasking = "campaign/tasking.json";
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Regiment() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    ScenarioRunner& r = s.Runner();
    while (!r.Finished() && r.TaskReadyS() < 0.0) s.Step(0.1);
    SC_EXPECT(r.TaskReadyS() > 0.0 && s.TasksGiven().empty());  // nothing reached the regiment to move it on
    const double first_ready = r.TaskReadyS();
    // Our intelligence's estimate, made now that the battery stands at the bridges.
    SC_EXPECT(s.RegimentDecides(Knows(r.Time() / 3600.0 + 0.01), false));
    SC_EXPECT(!s.RegimentDecides(Knows(r.Time() / 3600.0 + 0.01), false));  // once per estimate
    SC_EXPECT(s.TasksGiven().size() == 1 && r.TasksGiven() == 2 && r.Task().main == "bty21_bridges_reserve");
    const double given = r.Time();
    while (!r.Finished()) s.Step(0.1);
    for (const std::string& n : s.Notes()) std::printf("    %s\n", n.c_str());
    // He had it on the regiment's net, planned it from the bridges, marched the battery to the reserve position and
    // reported it ready there by the order's time.
    SC_EXPECT(r.TaskReceivedS() >= given && r.TaskPlan().planned && r.TaskPlan().march_at_s > given);
    SC_EXPECT(r.TaskReadyS() > first_ready && r.TaskReadyS() <= r.TaskPlan().ready_by_s);
    const nav::Anchor* reserve = r.Nav()->GetAnchors().Find("bty21_bridges_reserve");
    const Battery::Vehicle* head = r.GetBattery()->Find(r.Def().battery_giver);
    SC_EXPECT(reserve != nullptr && head != nullptr);
    if (reserve != nullptr && head != nullptr) {
        SC_EXPECT(std::hypot(head->position.x - reserve->at.east_m, head->position.y - reserve->at.north_m) < 1000.0);
    }
    // Both tasks in the debrief, the first as it went.
    const std::string d = r.Debrief();
    const std::size_t from = d.find("  The task (");
    if (from != std::string::npos) std::printf("%s", d.substr(from, d.find("  Battery orders", from) - from).c_str());
    SC_EXPECT(d.find("The task (bty21_cover_bridges)") != std::string::npos && d.find("The task (bty21_cover_bridges_relocate)") != std::string::npos);
    SC_EXPECT(d.find("The task (bty21_cover_bridges)") < d.find("The task (bty21_cover_bridges_relocate)"));
}

SC_TEST("regiment (14.2): a task is given only where the session can carry it out") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    campaign::BatteryTask t;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", t, &e));
    std::string why;
    campaign::BatteryTask nowhere = t;
    nowhere.main = "no_such_place";
    SC_EXPECT(!s.Runner().GiveTask(nowhere, "radio", &why) && why.find("no_such_place") != std::string::npos);
    campaign::BatteryTask wrong = t;
    wrong.sectors.front().vehicle = "TZM";
    SC_EXPECT(!s.Runner().GiveTask(wrong, "radio", &why) && why.find("launcher") != std::string::npos);
    SC_EXPECT(!s.Runner().GiveTask(t, "pigeon", &why));
    SC_EXPECT(s.Runner().TasksGiven() == 1);
    // A session without the battery's roads has nothing to plan a task on.
    md.scenario = kData + "/scenarios/osa_practice.json";
    campaign::MissionSession o;
    SC_EXPECT(o.Start(md, kData, &e));
    SC_EXPECT(!o.Runner().GiveTask(t, "radio", &why) && !why.empty());
}
