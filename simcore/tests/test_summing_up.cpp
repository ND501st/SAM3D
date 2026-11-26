// Track O, O5: the summing up (campaign/grading.hpp; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md sections 3 and 7).
// The regiment's officer grades the battery commander at the evening briefing from the written record only: readiness
// against the ready-by as reported, the records against the radar company's log, the order's limits as his log shows
// them, the kills claimed against what that log can bear out. The marks and the trust move the regiment's next task
// (data/campaign/tasking.json's trusted and distrusted rules). The battery commander grades his crews from his own log,
// the NPC through NpcDoes; the marks travel in the campaign's save. The reveal (campaign/reveal.hpp) is the only place the
// truth is shown. And the late battery's two planning bugs (the cover's march end, the in-person order's gathering).
#include <cmath>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/campaign/campaign_save.hpp"
#include "simcore/campaign/grading.hpp"
#include "simcore/campaign/regiment.hpp"
#include "simcore/campaign/reveal.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string Clock(double s) { return std::to_string(static_cast<int>(s)); }

campaign::LogEntry Entry(double t, const std::string& who, const std::string& kind, int number = 0) {
    campaign::LogEntry e;
    e.time_s = t;
    e.callsign = who;
    e.kind = kind;
    e.number = number;
    return e;
}

campaign::SaidReport Said(double t, int number) {
    campaign::SaidReport s;
    s.time_s = t;
    s.number = number;
    return s;
}

campaign::GradingRules Rules() {
    campaign::GradingRules g;
    std::string e;
    if (!campaign::LoadGradingRules(kData, "campaign/grading.json", g, &e)) std::printf("    %s\n", e.c_str());
    return g;
}

std::string LoadMessage(const std::string& text) {
    json::Value root;
    std::string pe;
    if (!json::Parse(text, root, &pe)) return "parse: " + pe;
    campaign::GradingRules g;
    campaign::detail::GradingLoader l;
    return l.Load(root, g) ? std::string() : l.Message();
}

std::string Replace(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at != std::string::npos) s.replace(at, from.size(), to);
    return s;
}

std::unique_ptr<ScenarioRunner> LoadPost(const std::string& rel, double clock_h, bool briefings) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenarioFile(kData + "/" + rel, kData, def, console, &e)) {
        std::printf("    %s\n", e.c_str());
        return nullptr;
    }
    if (briefings && !campaign::LoadBriefings(kData, "campaign/briefings.json", def.briefings, &e)) {
        std::printf("    %s\n", e.c_str());
        return nullptr;
    }
    if (clock_h >= 0.0) def.clock_start_s = clock_h * 3600.0;
    return std::unique_ptr<ScenarioRunner>(new ScenarioRunner(std::move(def), std::move(console)));
}

int Grades(const ScenarioRunner& r) {
    int n = 0;
    for (const campaign::LogEntry& e : r.CommanderLog()) n += e.kind == "grade" ? 1 : 0;
    return n;
}

}  // namespace

SC_TEST("summing up (O5): the grading rules load, each value sourced; bands out of order or an unknown key are refused") {
    const campaign::GradingRules g = Rules();
    SC_EXPECT(g.speaker == "rgt_commander" && g.late_4_s > g.late_5_s && g.share_3 < g.share_5 && g.above_lowest == 1);
    SC_EXPECT(!g.Line("ready_late").empty() && !g.CrewLine("breach").empty() && g.crew_lead_h > 0.0);
    std::string text;
    SC_EXPECT(simcore::detail::ReadTextFile(kData + "/campaign/grading.json", text));
    SC_EXPECT(LoadMessage(text).empty());
    SC_EXPECT(LoadMessage(Replace(text, "\"breaches_4\": { \"value\": 1", "\"breaches_4\": { \"value\": 9")).find("limits") != std::string::npos);
    SC_EXPECT(!LoadMessage(Replace(text, "\"value\": 180, \"source\"", "\"value\": 180, \"sourc\"")).empty());
    SC_EXPECT(LoadMessage(Replace(text, "\"ready_never\":", "\"ready_nver\":")).find("ready_never") != std::string::npos);
}

SC_TEST("summing up (O5): the kills claimed checked against the radar company's log: confirmed, pending, contradicted, unknown") {
    const campaign::GradingRules g = Rules();
    const std::vector<campaign::LogEntry> log = {Entry(100.0, "22", "destroyed", 3401), Entry(120.0, "23", "destroyed", 9999),
                                                 Entry(130.0, "22", "destroyed", 3402), Entry(140.0, "22", "not destroyed", 3402)};
    std::vector<campaign::SaidReport> radar = {Said(50.0, 3401), Said(60.0, 3402)};
    campaign::ClaimCheck c = campaign::CheckClaims(log, radar, 100.0 + g.confirm_s + 10.0, g);
    SC_EXPECT(c.claims.size() == 2);  // the kill taken back is no claim
    SC_EXPECT(c.Confirmed() == 1 && c.Count("unknown") == 1);
    SC_EXPECT(campaign::CheckClaims(log, radar, 150.0, g).Count("pending") == 1);
    radar.push_back(Said(100.0 + g.confirm_s + 60.0, 3401));  // it went on reporting it
    c = campaign::CheckClaims(log, radar, 1000.0, g);
    SC_EXPECT(c.Contradicted() == 1 && c.Confirmed() == 0);
    // Into the trust: a contradicted claim is a checked entry that did not agree.
    campaign::ReportTrust t;
    campaign::TrustClaims(t, "Battery 21", c);
    SC_EXPECT(t.CountOf("Battery 21").checked == 1 && t.CountOf("Battery 21").agreed == 0);
}

SC_TEST("summing up (O5): each objective's mark by its band, its reason in the officer's words; the day held to one above the worst") {
    const campaign::GradingRules g = Rules();
    const campaign::AfterActionValues av;
    campaign::DayRecord d;
    d.to_s = 9000.0;
    d.readiness.push_back({"bty21_cover_bridges", 0.0, 5000.0, 5000.0 + 600.0, false});  // ten minutes late
    d.board.record = "board";
    d.board.said = 10;
    d.board.as_said = 10;
    d.breaches.push_back({300.0, "22", "spot", "a launcher's spot reported 900 m off"});
    d.claims.claims = {{100.0, "22", 1, "confirmed"}, {200.0, "23", 2, "unknown"}};
    const campaign::SummingUp s = campaign::GradeDay(d, g, av, Clock);
    SC_EXPECT(s.objectives.size() == 4);
    const campaign::ObjectiveGrade* ready = s.Find("ready");
    const campaign::ObjectiveGrade* records = s.Find("records");
    const campaign::ObjectiveGrade* limits = s.Find("limits");
    const campaign::ObjectiveGrade* claims = s.Find("claims");
    SC_EXPECT(ready != nullptr && ready->grade == 4 && ready->line.find("10 min after") != std::string::npos && ready->line.find("Good.") != std::string::npos);
    SC_EXPECT(records != nullptr && records->grade == campaign::RecordGrade(d.board, av) && records->line.find("10 of 10") != std::string::npos);
    SC_EXPECT(limits != nullptr && limits->grade == 4 && limits->line.find("900 m off") != std::string::npos);
    SC_EXPECT(claims != nullptr && claims->grade == 3 && claims->line.find("bears out 1") != std::string::npos);
    SC_EXPECT(s.grade == 4 && s.lines.size() == 5 && s.lines.back().find("for the day: good") != std::string::npos);
    for (const std::string& l : s.lines) std::printf("    %s\n", l.c_str());
    // Never reported ready: unsatisfactory, and the day no better than satisfactory however good the rest.
    campaign::DayRecord never = d;
    never.readiness = {{"bty21_cover_bridges", 0.0, 5000.0, -1.0, false}};
    never.breaches.clear();
    never.claims.claims = {{100.0, "22", 1, "confirmed"}};
    const campaign::SummingUp n = campaign::GradeDay(never, g, av, Clock);
    SC_EXPECT(n.Find("ready") != nullptr && n.Find("ready")->grade == 2 && n.grade == 3);
    // A task held from before the save is not judged again; nothing else written: nothing to sum up.
    campaign::DayRecord quiet;
    quiet.readiness = {{"bty21_cover_bridges", 0.0, 5000.0, 4000.0, true}};
    const campaign::SummingUp q = campaign::GradeDay(quiet, g, av, Clock);
    SC_EXPECT(q.grade == 0 && q.objectives.empty() && q.lines.size() == 1 && q.lines.front().find("nothing") != std::string::npos);
    // Into the standing: the mark kept, the checks in the trust.
    campaign::RegimentStanding st;
    campaign::SummingUp marked = s;
    marked.campaign_h = 19.0;
    st.Add(marked, d);
    SC_EXPECT(st.LastGrade() == 4 && st.checked == 11 && st.agreed == 11 && st.summed_h == 19.0);
}

SC_TEST("summing up (O5): trusted, the regiment puts the battery's launchers on their own; once per summing up, at the briefing") {
    campaign::TaskingRules rules;
    std::string e;
    SC_EXPECT(campaign::LoadTaskingRules(kData, "campaign/tasking.json", rules, &e));
    campaign::RegimentTasking rgt(rules, kData);
    campaign::BatteryTask holds;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", holds, &e));
    campaign::RegimentPicture pic;
    pic.standing_h = 19.0;
    pic.standing_grade = 5;
    pic.standing_trust = 0.9;
    pic.standing_why = "Battery 21 for the day: excellent.";
    SC_EXPECT(!rgt.Decide(pic, &holds, 0.0, false).give);  // routine: at the briefing only
    const campaign::TaskingDecision d = rgt.Decide(pic, &holds, 0.0, true);
    SC_EXPECT(d.give && d.rule == "trusted_on_its_own" && d.task.limits.fire == "independent");
    SC_EXPECT(d.task.id == "bty21_cover_bridges_independent" && d.task.task_text.compare(0, 14, "the launchers ") == 0);
    SC_EXPECT(d.task.main == holds.main && !d.task.reasons.empty() && d.task.reasons.front().find("excellent") != std::string::npos);
    std::printf("    %s\n", d.task.task_text.c_str());
    // Not again on the same summing up; a good day is not enough.
    SC_EXPECT(!rgt.Decide(pic, &holds, 0.0, true).give);
    campaign::RegimentPicture good = pic;
    good.standing_h = 43.0;
    good.standing_grade = 4;
    SC_EXPECT(!rgt.Decide(good, &holds, 0.0, true).give);
    // Already on their own: nothing to change.
    campaign::RegimentPicture next = pic;
    next.standing_h = 67.0;
    SC_EXPECT(!rgt.Decide(next, &d.task, 0.0, true).give);
}

SC_TEST("summing up (O5): distrusted, the regiment holds the launchers on the battery commander's designation, the task's words kept") {
    campaign::TaskingRules rules;
    std::string e;
    SC_EXPECT(campaign::LoadTaskingRules(kData, "campaign/tasking.json", rules, &e));
    campaign::RegimentTasking rgt(rules, kData);
    campaign::BatteryTask holds;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", holds, &e));
    holds.limits.fire = "independent";
    holds.id = "bty21_cover_bridges_independent";
    campaign::RegimentPicture pic;
    pic.standing_h = 43.0;
    pic.standing_grade = 4;
    pic.standing_trust = 0.6;  // a good day, but its reports have not held up
    const campaign::TaskingDecision d = rgt.Decide(pic, &holds, 0.0, true);
    SC_EXPECT(d.give && d.rule == "distrusted_on_designation" && d.task.limits.fire == "designation");
    SC_EXPECT(d.task.id == "bty21_cover_bridges_designation");
    SC_EXPECT(d.task.task_text.find(holds.task_text) != std::string::npos);
    // An unsatisfactory day alone is enough.
    campaign::RegimentPicture bad;
    bad.standing_h = 67.0;
    bad.standing_grade = 2;
    bad.standing_trust = 0.95;
    holds.limits.fire.clear();
    SC_EXPECT(rgt.Decide(bad, &holds, 0.0, true).rule == "distrusted_on_designation");
    // Refused: a fire mode with no standing to read, or a standing trigger with another task.
    const auto bad_rules = [](const std::string& rule) {
        json::Value root;
        std::string pe;
        if (!json::Parse(R"({"source": "sam3d", "confidence": "estimate", "rules": [)" + rule + "]}", root, &pe)) return "parse: " + pe;
        campaign::TaskingRules r;
        campaign::detail::TaskingLoader l;
        return l.Load(root, r) ? std::string() : l.Message();
    };
    const std::string prov = R"("reason": "r", "source": "sam3d", "confidence": "estimate")";
    SC_EXPECT(bad_rules(R"({"id": "a", "when": "trusted", "task": "fire_mode", "fire": "independent", "ready_in_s": 60, )" + prov + "}")
                  .find("grade_at_least") != std::string::npos);
    SC_EXPECT(bad_rules(R"({"id": "a", "when": "distrusted", "task": "relocate", "ready_in_s": 60, "trust_below": 0.5, )" + prov + "}")
                  .find("fire_mode") != std::string::npos);
    SC_EXPECT(bad_rules(R"({"id": "a", "when": "trusted", "task": "fire_mode", "fire": "sometimes", "ready_in_s": 60, "grade_at_least": 5, )" + prov + "}")
                  .find("fire") != std::string::npos);
}

SC_TEST("summing up (O5): the battery commander grades each crew from his log: a breach or a kill taken back costs a mark") {
    const campaign::GradingRules g = Rules();
    const std::vector<campaign::LogEntry> log = {Entry(50.0, "22", "launch", 1), Entry(10.0, "23", "launch", 3),  // before: not this day's
                                                 Entry(100.0, "22", "launch", 1), Entry(110.0, "22", "destroyed", 1),
                                                 Entry(300.0, "22", "not destroyed", 1)};
    const std::vector<campaign::LimitFinding> breaches = {{200.0, "23", "spot", "its spot 800 m off"}, {5.0, "24", "spot", "before"}};
    const std::vector<campaign::CrewGrade> c = campaign::GradeCrews({"22", "23", "24"}, log, breaches, 40.0, 400.0, g);
    SC_EXPECT(c.size() == 3);
    if (c.size() < 3) return;
    SC_EXPECT(c[0].crew == "22" && c[0].grade == 4 && c[0].reasons.size() == 2);  // its launches, the kill taken back
    SC_EXPECT(c[1].crew == "23" && c[1].grade == 4 && c[1].reasons.size() == 1 && c[1].reasons.front().find("800 m off") != std::string::npos);
    SC_EXPECT(c[2].crew == "24" && c[2].grade == 5 && c[2].reasons.size() == 1);
    for (const campaign::CrewGrade& x : c) std::printf("    %s\n", campaign::CrewGradeText(x).c_str());
}

SC_TEST("summing up (O5): the NPC battery commander grades his crews before the briefing through his duty; a player is owed it and may set his own") {
    std::unique_ptr<ScenarioRunner> r = LoadPost("scenarios/pu12m_mission_cover.json", 17.6, true);  // PLACEHOLDER: 17:36, within the lead
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const std::vector<std::string> crews = r->LauncherCrews();
    SC_EXPECT(!crews.empty());
    for (int i = 0; i < 240 && Grades(*r) == 0; ++i) r->Step(0.5);
    SC_EXPECT(Grades(*r) == static_cast<int>(crews.size()) && r->CrewsGradedS() > 0.0);
    SC_EXPECT(r->Debrief().find("crew grades") != std::string::npos);
    for (int i = 0; i < 240; ++i) r->Step(0.5);
    SC_EXPECT(Grades(*r) == static_cast<int>(crews.size()));  // once a day
    // Too early: nothing.
    std::unique_ptr<ScenarioRunner> early = LoadPost("scenarios/pu12m_mission_cover.json", 12.0, true);
    for (int i = 0; i < 240; ++i) early->Step(0.5);
    SC_EXPECT(Grades(*early) == 0);
    // The player in his seat: the duty is owed, nothing is done for him; his own mark replaces the rule's.
    std::unique_ptr<ScenarioRunner> p = LoadPost("scenarios/pu12m_mission_cover.json", 17.6, true);
    SC_EXPECT(p->TakeSeat("battery_commander", "player"));
    bool owed = false;
    for (int i = 0; i < 240 && !owed; ++i) {
        p->Step(0.5);
        for (const SeatDuty& d : p->AllOwedDuties()) owed = owed || d.id == "order_grade_crews";
    }
    SC_EXPECT(owed && Grades(*p) == 0);
    SC_EXPECT(p->SetCrewMark(crews.front(), 3) && !p->SetCrewMark("nobody", 3) && !p->SetCrewMark(crews.front(), 6));
    SC_EXPECT(p->GiveCrewOrder("order_grade_crews"));
    SC_EXPECT(!p->CrewMarks().empty() && p->CrewMarks().front().grade == 3 && p->CrewMarks().front().by_player);
}

SC_TEST("summing up (O5): the standing, the crews' marks and the task's fire mode travel in the campaign's save") {
    campaign::CampaignSave s;
    s.version = 1;
    s.kind = "briefing";
    s.adversary = "rebels";
    s.mission = "scenarios/pu12m_mission_cover.json";
    s.air_war = "{}";
    s.battery.present = true;
    s.battery.campaign_h = 19.0;
    std::string e;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", s.battery.task, &e));
    s.battery.has_task = true;
    s.battery.task.limits.fire = "independent";
    campaign::CrewGrade c;
    c.crew = "22";
    c.grade = 3;
    c.by_player = true;
    c.campaign_h = 17.6;
    c.reasons = {"its spot 800 m off"};
    s.battery.crew_grades.push_back(c);
    s.standing.checked = 12;
    s.standing.agreed = 9;
    s.standing.checks = 3;
    s.standing.summed_h = 19.0;
    s.standing.days.push_back({19.0, 4, {{"ready", 4}, {"records", 5}}, {"Readiness: ...", "Battery 21 for the day: good."}});
    campaign::CampaignSave back;
    SC_EXPECT(campaign::LoadCampaignSaveJson(campaign::CampaignSaveJson(s), back, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(back.standing.checked == 12 && back.standing.agreed == 9 && back.standing.checks == 3 && back.standing.summed_h == 19.0);
    SC_EXPECT(back.standing.days.size() == 1 && back.standing.days[0].grade == 4 && back.standing.days[0].objectives.size() == 2 &&
              back.standing.days[0].lines.back() == "Battery 21 for the day: good.");
    SC_EXPECT(back.battery.crew_grades.size() == 1 && back.battery.crew_grades[0].grade == 3 && back.battery.crew_grades[0].by_player &&
              back.battery.crew_grades[0].reasons.size() == 1);
    SC_EXPECT(back.battery.has_task && back.battery.task.limits.fire == "independent");
}

SC_TEST("summing up (O5): the cover task's battery is ready by its ready-by (the march's end behind the covered unit planned)") {
    std::unique_ptr<ScenarioRunner> r = LoadPost("scenarios/pu12m_cover_company.json", -1.0, false);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    while (r->TaskReadyS() < 0.0 && r->Time() < 6000.0) r->Step(0.1);
    std::printf("    ready at %.0f s, by %.0f s\n", r->TaskReadyS(), r->TaskPlan().ready_by_s);
    SC_EXPECT(r->TaskReadyS() > 0.0 && r->TaskReadyS() <= r->TaskPlan().ready_by_s);
    SC_EXPECT(!r->TaskPlan().to_place.empty() && r->TaskPlan().to_place.compare(0, 12, "task:behind:") == 0);
}

SC_TEST("summing up (O5): the reveal puts the records beside the truth, and the regiment's marks with them") {
    campaign::MissionSessionDef d;
    d.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    d.hours = 0.5;
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(d, kData, &e));
    for (int i = 0; i < 600; ++i) s.Step(0.5);
    const campaign::Reveal v = campaign::MakeReveal(s);
    SC_EXPECT(v.pairs.size() >= 3 && v.pairs.front().topic == "Kills");
    const std::string t = v.Text();
    SC_EXPECT(t.find("believed:") != std::string::npos && t.find("truth:") != std::string::npos);
    SC_EXPECT(t.find("The battery's two outcomes") != std::string::npos);  // the ledgers, from the truth: here only
}
