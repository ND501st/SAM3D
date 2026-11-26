// The regiment's tasking (roadmap 14.2, first form; track F's F4.4, the fronts' triggers): simcore/campaign/regiment.hpp, data/campaign/tasking.json, and the
// regiment's next task given into a running session (ScenarioRunner::GiveTask, campaign/mission_session.hpp).
#include <cmath>
#include <cstdio>
#include <map>
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
    SC_EXPECT(rules.rules.size() == 20);  // and F4.4's four: test_regiment's F4.4 tests; O5's two: test_summing_up; O7's twelve: test_redeploy
    if (rules.rules.size() < 2) return;
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

// ------------------------------------------------------------------------------- F4.4: the regiment and the battery follow

namespace {

// The regiment's map by hand: how far behind the front's line each place lies (m); against the Warsaw Pact from the
// north, the depth is how far south.
campaign::PlaceDepth HandDepth(const std::map<std::string, double>& m) {
    return [m](const std::string& place, double& depth_m, std::string& name) {
        const auto it = m.find(place);
        if (it == m.end()) return false;
        depth_m = it->second;
        name = "the place " + place;
        return true;
    };
}

const campaign::TaskingRule* RuleOf(const campaign::TaskingRules& rules, const std::string& id) {
    for (const campaign::TaskingRule& r : rules.rules) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

campaign::RegimentPicture Fronts(double now_h) {
    campaign::RegimentPicture p;
    p.fronts = true;
    p.now_h = now_h;
    p.sector = "rava_reservoir";
    p.priority = "defend";
    return p;
}

}  // namespace

SC_TEST("regiment (F4.4): the fronts' rules load, each trigger with its own task, sourced") {
    campaign::TaskingRules rules;
    std::string e;
    SC_EXPECT(campaign::LoadTaskingRules(kData, "campaign/tasking.json", rules, &e));
    const campaign::TaskingRule* back = RuleOf(rules, "withdraw_with_division");
    const campaign::TaskingRule* defend = RuleOf(rules, "sector_raised_to_defend");
    const campaign::TaskingRule* economy = RuleOf(rules, "sector_to_economy");
    const campaign::TaskingRule* sharing = RuleOf(rules, "tell_sharing");
    SC_EXPECT(back != nullptr && defend != nullptr && economy != nullptr && sharing != nullptr);
    if (back == nullptr || defend == nullptr || economy == nullptr || sharing == nullptr) return;
    SC_EXPECT(back->when == "division_withdraws" && back->task == "march_back" && back->urgent && back->positions.size() == 2 && back->back_m > 0.0 &&
              back->ready_in_s > 0.0 && back->work_s > 0.0);
    SC_EXPECT(defend->when == "sector_defend" && defend->task == "posture" && defend->urgent && defend->readiness == 1);
    SC_EXPECT(economy->when == "sector_economy" && economy->task == "posture" && !economy->urgent && economy->readiness == 2);
    SC_EXPECT(sharing->when == "sharing_changed" && sharing->task == "tell_sharing" && sharing->urgent);
    const auto bad = [](const std::string& rule) {
        json::Value root;
        std::string pe;
        const std::string text = R"({"source": "sam3d", "confidence": "estimate", "rules": [)" + rule + "]}";
        if (!json::Parse(text, root, &pe)) return std::string("parse: ") + pe;
        campaign::TaskingRules r;
        campaign::detail::TaskingLoader l;
        return l.Load(root, r) ? std::string() : l.Message();
    };
    const std::string prov = R"("reason": "r", "source": "sam3d", "confidence": "estimate")";
    SC_EXPECT(bad(R"({"id": "a", "when": "division_withdraws", "task": "march_back", "back_m": 6000, "ready_in_s": 1, )" + prov + "}").find("positions") !=
              std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "division_withdraws", "task": "march_back", "positions": ["p"], "ready_in_s": 1, )" + prov + "}").find("back_m") !=
              std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "sector_defend", "task": "posture", "ready_in_s": 1, )" + prov + "}").find("readiness") != std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "sector_defend", "task": "posture", "ready_in_s": 1, "emission": "loud", )" + prov + "}").find("emission") !=
              std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "division_withdraws", "task": "relocate", "ready_in_s": 1, )" + prov + "}").find("march_back") != std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "no_task", "task": "tell_sharing", )" + prov + "}").find("fronts") != std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "sharing_changed", "task": "tell_sharing", "work_s": -1, )" + prov + "}").find("work_s") != std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "sharing_changed", "task": "tell_sharing", "work_s": 600, )" + prov + "}").empty());
}

SC_TEST("regiment (F4.4): the march back goes to the nearest prepared position far enough behind the front's line") {
    campaign::BatteryTask now;
    std::string e;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", now, &e));
    campaign::TaskingRule rule;
    rule.reason = "The division withdraws.";
    rule.positions = {"bty21_depth_south", "bty21_depth_east", "near"};
    rule.back_m = 6000.0;
    rule.ready_in_s = 7200.0;
    campaign::RegimentPicture pic = Fronts(10.0);
    pic.withdrawals = 1;
    pic.withdrawal_h = 9.5;
    pic.withdrawal = "the 7th Motor Rifle Division is to withdraw from the line";
    // Against a war from the north: the south position is 10.5 km further back; the near one only 2 km, the east one 0.5.
    const campaign::PlaceDepth north = HandDepth({{"bty21_bridges", 7000.0}, {"bty21_depth_south", 17500.0}, {"bty21_depth_east", 7500.0}, {"near", 9000.0}});
    campaign::BatteryTask t;
    SC_EXPECT(campaign::MarchBackTask(now, rule, pic, north, 5000.0, t, &e));
    SC_EXPECT(t.id == "bty21_cover_bridges_withdraw" && t.main == "bty21_depth_south" && t.kind == campaign::TaskKind::Relocate && t.covered.empty());
    SC_EXPECT(t.reserve.empty() && t.dummies.empty() && t.tzm_place.empty() && !t.reconnoitre);
    SC_EXPECT(t.task_text.find("the place bty21_depth_south") != std::string::npos && !t.short_ru.empty());
    SC_EXPECT(t.reasons.size() == 1 && t.reasons.front().find(rule.reason) == 0 && t.reasons.front().find("7th Motor Rifle") != std::string::npos);
    SC_EXPECT_NEAR(t.ready_in_s, 7200.0, 1e-9);
    SC_EXPECT(t.own_air.method == now.own_air.method && t.sectors.size() == now.sectors.size());  // no sharing held: as it was
    // From the west, the east position; and with the sky's sharing held, the plan the regiment holds goes with it.
    pic.sharing_known = true;
    pic.sharing = command::CooperationMethod::OneZone;
    SC_EXPECT(campaign::MarchBackTask(now, rule, pic, HandDepth({{"bty21_bridges", 9600.0}, {"bty21_depth_south", 2000.0}, {"bty21_depth_east", 19000.0}}),
                                      5000.0, t, &e));
    SC_EXPECT(t.main == "bty21_depth_east" && t.own_air.method == command::CooperationMethod::OneZone);
    SC_EXPECT_NEAR(t.own_air.split_height_m, 5000.0, 1e-9);
    // Withdrawn again from there: one name, one suffix.
    campaign::BatteryTask again;
    SC_EXPECT(campaign::MarchBackTask(t, rule, pic, HandDepth({{"bty21_depth_east", 0.0}, {"bty21_depth_south", 9000.0}}), 5000.0, again, &e));
    SC_EXPECT(again.id == "bty21_cover_bridges_withdraw" && again.main == "bty21_depth_south");
    // Troops' cover stays theirs; no position far enough back, or a main position the regiment cannot place: no march.
    now.kind = campaign::TaskKind::CoverTroops;
    SC_EXPECT(campaign::MarchBackTask(now, rule, pic, north, 5000.0, t, &e) && t.kind == campaign::TaskKind::CoverTroops && t.covered == now.covered);
    rule.back_m = 20000.0;
    SC_EXPECT(!campaign::MarchBackTask(now, rule, pic, north, 5000.0, t, &e) && e.find("further back") != std::string::npos);
    SC_EXPECT(!campaign::MarchBackTask(now, rule, pic, HandDepth({}), 5000.0, t, &e) && e.find("bty21_bridges") != std::string::npos);
    SC_EXPECT(!campaign::MarchBackTask(now, rule, pic, campaign::PlaceDepth(), 5000.0, t, &e));
}

SC_TEST("regiment (F4.4): a sector's new priority changes the readiness held, not the task") {
    campaign::TaskingRules rules;
    campaign::BatteryTask now;
    std::string e;
    SC_EXPECT(campaign::LoadTaskingRules(kData, "campaign/tasking.json", rules, &e));
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", now, &e));
    const campaign::TaskingRule* defend = RuleOf(rules, "sector_raised_to_defend");
    const campaign::TaskingRule* economy = RuleOf(rules, "sector_to_economy");
    if (defend == nullptr || economy == nullptr) return;
    campaign::RegimentPicture pic = Fronts(20.0);
    pic.priority_h = 19.5;
    pic.priority_why = "the army's chief of air defence: rava_reservoir to defend";
    campaign::BatteryTask t;
    campaign::PostureTask(now, *defend, pic, 5000.0, t);
    SC_EXPECT(t.id == "bty21_cover_bridges_defend" && t.main == now.main && t.kind == now.kind && t.reserve == now.reserve && t.covered == now.covered);
    SC_EXPECT(t.readiness == 1 && t.readiness_text == defend->readiness_text && t.emission == now.emission);
    SC_EXPECT(t.task_text == "hold readiness no. 1 and go on with the task: " + now.task_text);
    SC_EXPECT(t.reasons.size() == 1 && t.reasons.front().find(defend->reason) == 0 && t.reasons.front().find("to defend") != std::string::npos);
    // Then economy: the words do not stack.
    pic.priority = "economy";
    campaign::BatteryTask quiet;
    campaign::PostureTask(t, *economy, pic, 5000.0, quiet);
    SC_EXPECT(quiet.id == "bty21_cover_bridges_economy" && quiet.readiness == 2);
    SC_EXPECT(quiet.task_text == "hold readiness no. 2 and go on with the task: " + now.task_text);
    // A rule's emission regime, with the regime's own bursts.
    campaign::TaskingRule silent = *defend;
    silent.emission = "silent";
    campaign::PostureTask(now, silent, pic, 5000.0, t);
    SC_EXPECT(t.emission == "silent" && t.on_turns == 0.0 && t.off_turns == 0.0);
}

SC_TEST("regiment (F4.4): it acts on each fact once, after its staff's work; it tells the sky's sharing when it differs from the plan held") {
    campaign::TaskingRules rules;
    campaign::BatteryTask holds;
    std::string e;
    SC_EXPECT(campaign::LoadTaskingRules(kData, "campaign/tasking.json", rules, &e));
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", holds, &e));
    SC_EXPECT(holds.own_air.present && holds.own_air.method == command::CooperationMethod::ByZones);
    campaign::RegimentTasking rgt(rules, kData);
    rgt.SetPlaceDepth(HandDepth({{"bty21_bridges", 7000.0}, {"bty21_depth_south", 17500.0}, {"bty21_depth_east", 7500.0}}));
    // Its pre-war picture: nothing to do.
    SC_EXPECT(!rgt.Decide(Fronts(1.0), &holds, 0.0, true).give);
    // The division withdraws: the order reached the regiment at 10:00; its staff's quarter hour, then the march.
    campaign::RegimentPicture w = Fronts(10.1);
    w.withdrawals = 1;
    w.withdrawal_h = 10.0;
    w.withdrawal = "withdraw";
    SC_EXPECT(!rgt.Decide(w, &holds, 0.0, false).give);
    w.now_h = 10.25;
    const campaign::TaskingDecision d = rgt.Decide(w, &holds, 0.0, false);
    SC_EXPECT(d.give && d.rule == "withdraw_with_division" && d.task.main == "bty21_depth_south" && !d.tell_sharing);
    SC_EXPECT_NEAR(d.fact_h, 10.0, 1e-9);
    w.now_h = 11.0;
    SC_EXPECT(!rgt.Decide(w, &d.task, 10.25, false).give);  // once
    w.withdrawals = 2;
    w.withdrawal_h = 30.0;
    w.now_h = 30.5;
    SC_EXPECT(!rgt.Decide(w, &d.task, 10.25, false).give);  // nothing further back from there: told why
    // What reached it before the session was acted on before it.
    campaign::RegimentTasking since(rules, kData);
    since.SetPlaceDepth(HandDepth({{"bty21_bridges", 7000.0}, {"bty21_depth_south", 17500.0}}));
    since.Since(w);
    SC_EXPECT(!since.Decide(w, &holds, 0.0, false).give);
    // The sector raised to defend: at once; to economy: at the evening briefing.
    campaign::RegimentPicture p = Fronts(20.5);
    p.priority_h = 20.0;
    const campaign::TaskingDecision up = rgt.Decide(p, &holds, 0.0, false);
    SC_EXPECT(up.give && up.rule == "sector_raised_to_defend" && up.task.readiness == 1);
    SC_EXPECT(!rgt.Decide(p, &up.task, 0.0, false).give);
    p.priority = "economy";
    p.priority_h = 40.0;
    p.now_h = 41.0;
    SC_EXPECT(!rgt.Decide(p, &up.task, 0.0, false).give);
    const campaign::TaskingDecision down = rgt.Decide(p, &up.task, 0.0, true);
    SC_EXPECT(down.give && down.rule == "sector_to_economy" && down.task.readiness == 2);
    // The sky's sharing: the plan held is by zones; one zone reached the regiment.
    campaign::RegimentPicture s = Fronts(50.0);
    s.sharing_known = true;
    s.sharing = command::CooperationMethod::ByZones;
    SC_EXPECT(!rgt.Decide(s, &holds, 0.0, false).give);  // as held
    s.sharing = command::CooperationMethod::OneZone;
    s.sharing_h = 50.0;
    SC_EXPECT(!rgt.Decide(s, &holds, 0.0, false).give);  // the duty officer's ten minutes
    s.now_h = 50.2;
    const campaign::TaskingDecision told = rgt.Decide(s, &holds, 0.0, false);
    SC_EXPECT(told.give && told.rule == "tell_sharing" && told.tell_sharing && told.sharing == command::CooperationMethod::OneZone);
    SC_EXPECT_NEAR(told.fact_h, 50.0, 1e-9);
    SC_EXPECT(!rgt.Decide(s, &holds, 0.0, false).give);  // told once
    s.sharing = command::CooperationMethod::ByZones;
    s.sharing_h = 60.0;
    s.now_h = 60.2;
    const campaign::TaskingDecision back = rgt.Decide(s, &holds, 0.0, false);
    SC_EXPECT(back.give && back.tell_sharing && back.sharing == command::CooperationMethod::ByZones);
}

SC_TEST("regiment (F4.4): the division's withdrawal reaches the regiment, its order is heard on the net and the battery marches back") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.tasking = "campaign/tasking.json";
    md.hours = 5.0;
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Regiment() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    ScenarioRunner& r = s.Runner();
    while (!r.Finished() && r.TaskReadyS() < 0.0) s.Step(0.1);
    SC_EXPECT(r.TaskReadyS() > 0.0);
    // The regiment's map: a war from the north (the session's own roads' anchors; the front far to the north).
    s.Regiment()->SetPlaceDepth([&r](const std::string& place, double& depth_m, std::string& name) {
        const nav::Anchor* a = r.Nav() != nullptr ? r.Nav()->GetAnchors().Find(place) : nullptr;
        if (a == nullptr) return false;
        depth_m = -a->at.north_m;
        name = a->name;
        return true;
    });
    // The withdrawal order reached the regiment now; a quarter of an hour later it decides.
    const double now_h = r.Time() / 3600.0;
    campaign::RegimentPicture w = Fronts(now_h);
    w.withdrawals = 1;
    w.withdrawal_h = now_h;
    w.withdrawal = "the 7th Motor Rifle Division is to withdraw from the line";
    SC_EXPECT(!s.RegimentDecides(w, false));
    w.now_h = now_h + 0.26;
    SC_EXPECT(s.RegimentDecides(w, false));
    SC_EXPECT(!s.RegimentDecides(w, false));
    SC_EXPECT(s.TasksGiven().size() == 1 && r.Task().main == "bty21_depth_south" && r.Task().id == "bty21_cover_bridges_withdraw");
    const double given = r.Time();
    while (!r.Finished() && (r.TaskReadyS() < given || r.Time() < given + 1.0)) s.Step(0.1);
    for (const std::string& n : s.Notes()) std::printf("    %s\n", n.c_str());
    const campaign::MissionSession::GivenTask& g = s.TasksGiven().front();
    SC_EXPECT(g.rule == "withdraw_with_division" && g.heard_s >= given && g.fact_s >= 0.0 && g.fact_s <= given);
    SC_EXPECT(r.TaskPlan().planned && r.TaskPlan().march_at_s > given && r.TaskReadyS() > given && r.TaskReadyS() <= r.TaskPlan().ready_by_s);
    const nav::Anchor* south = r.Nav()->GetAnchors().Find("bty21_depth_south");
    const Battery::Vehicle* head = r.GetBattery()->Find(r.Def().battery_giver);
    SC_EXPECT(south != nullptr && head != nullptr);
    if (south != nullptr && head != nullptr) {
        SC_EXPECT(std::hypot(head->position.x - south->at.east_m, head->position.y - south->at.north_m) < 1000.0);
    }
    std::printf("    the order heard %.1f min after the fact reached the regiment; ready at the new position %.0f min after\n", (g.heard_s - g.fact_s) / 60.0,
                (r.TaskReadyS() - given) / 60.0);
    // The sky's sharing changes in the sector: told on the net, heard at the post, the plan of cooperation so.
    campaign::RegimentPicture sky = Fronts(r.Time() / 3600.0 + 0.2);
    sky.sharing_known = true;
    sky.sharing = command::CooperationMethod::OneZone;
    sky.sharing_h = r.Time() / 3600.0;
    SC_EXPECT(r.SharingHeardS() < 0.0 && r.Task().own_air.method == command::CooperationMethod::ByZones);
    SC_EXPECT(s.RegimentDecides(sky, false));
    const double told = r.Time();
    while (!r.Finished() && r.SharingHeardS() < 0.0 && r.Time() < told + 1800.0) s.Step(0.1);
    SC_EXPECT(r.SharingHeardS() >= told && r.SharingChanges() == 1 && r.Task().own_air.method == command::CooperationMethod::OneZone);
    SC_EXPECT(s.TasksGiven().size() == 2 && s.TasksGiven().back().heard_s >= told);
}
