// The battery's mission (roadmap 14.1, 14.3, 14.5): the regiment's task as a document, the battery commander's
// planner by the textbook's sequence, and a cover task run headless from the regiment's order to «к бою готов».
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/campaign/battery_plan.hpp"
#include "simcore/campaign/battery_task.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string Clock19(double s) {  // the session's second 0 at 19:00
    const long long m = (static_cast<long long>(std::llround(19.0 * 3600.0 + s)) / 60) % (24 * 60);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld", m / 60, m % 60);
    return buf;
}

std::string Same(const std::string& id) { return id; }

// A task in memory: the file's with changes.
campaign::BatteryTask BridgesTask() {
    campaign::BatteryTask t;
    std::string error;
    campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", t, &error);
    return t;
}

campaign::PlanInputs Inputs(double march_s) {
    campaign::PlanInputs in;
    in.given_s = 0.0;
    in.received_s = 0.0;
    in.march_s = march_s;
    in.where = [](const std::string& id, Vec3& out) {
        if (id == "bty21_bridges") out = Vec3{9600.0, -7000.0, 0.0};
        else if (id == "bty21_bridges_reserve") out = Vec3{8700.0, -8000.0, 0.0};
        else if (id == "kesmark_bridges") out = Vec3{12000.0, -6000.0, 0.0};
        else if (id == "vrbice_crossroads") out = Vec3{4000.0, -7000.0, 0.0};
        else return false;
        return true;
    };
    return in;
}

}  // namespace

SC_TEST("battery mission (14.1): the regiment's task loads; its paper has the order's parts in the textbook's order; its radio form") {
    campaign::BatteryTask t;
    std::string error;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", t, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(t.kind == campaign::TaskKind::CoverObject && t.covered == "kesmark_bridges" && t.main == "bty21_bridges");
    SC_EXPECT(t.reasons.size() == 3 && t.sectors.size() == 4 && t.reserve.size() == 1 && t.emission == "limited");
    double attack = 0.0;
    SC_EXPECT(t.MainAttackDeg(attack));
    SC_EXPECT_NEAR(attack, 255.0, 1e-9);  // the middle of 225-285
    const std::string paper = campaign::TaskDocument(t, 60.0, Clock19, Same);
    std::printf("%s\n", paper.c_str());
    // The parts in the order of pp. 240-245.
    std::size_t last = 0;
    for (const char* part : {"1. Task:", "Why:", "2. The situation:", "3. Position area:", "4. Sectors:", "5. Readiness", "6. Time:",
                             "7. Coordination:", "8. Reports:", "9. Support:"}) {
        const std::size_t at = paper.find(part);
        SC_EXPECT(at != std::string::npos && at >= last);
        if (at != std::string::npos) last = at;
    }
    SC_EXPECT(paper.find("Combat order to battery 21, 19:01") != std::string::npos);
    SC_EXPECT(paper.find("ready by 20:31; until relieved") != std::string::npos);
    SC_EXPECT(paper.find("attack 225-285") != std::string::npos && paper.find("21 225-270 (duty)") != std::string::npos);
    const std::string radio = campaign::TaskVoiceSummary(t, 60.0, Clock19, Same);
    std::printf("    %s\n", radio.c_str());
    SC_EXPECT(radio.find(t.short_ru) != std::string::npos && radio.find(t.area_ru) != std::string::npos && radio.find("20:31") != std::string::npos);
}

SC_TEST("battery mission (14.1): a task's faults are named") {
    std::string text;
    SC_EXPECT(simcore::detail::ReadTextFile(kData + "/campaign/tasks/bty21_cover_bridges.json", text));
    const auto fails = [&](const std::string& from, const std::string& to, const std::string& why) {
        std::string t = text;
        const std::size_t at = t.find(from);
        SC_EXPECT(at != std::string::npos);
        if (at == std::string::npos) return;
        t.replace(at, from.size(), to);
        campaign::BatteryTask task;
        std::string error;
        SC_EXPECT(!campaign::LoadBatteryTaskJson(t, task, &error));
        std::printf("    %s\n", error.c_str());
        SC_EXPECT(error.find(why) != std::string::npos);
    };
    fails("\"kind\": \"cover_object\"", "\"kind\": \"hold_the_line\"", "kind");
    fails("\"ready_in_s\": 5400", "\"ready_in_s\": 0", "ready_in_s");
    fails("\"emission\": \"limited\"", "\"emission\": \"quiet\"", "emission");
    fails("\"vehicle\": \"22\", \"from_deg\": 225", "\"vehicle\": \"21\", \"from_deg\": 225", "a second sector");
    fails("\"kind\": \"attack\"", "\"kind\": \"bombers\"", "situation[0].kind");
    fails("\"main\": \"bty21_bridges\",", "", "area.main");
}

SC_TEST("battery mission (14.3): the commander's sequence timed to the ready-by: in full, shortened alike, and late") {
    campaign::PlanSequence seq;
    std::string error;
    SC_EXPECT(campaign::LoadPlanSequence(kData, "campaign/battery_plan.json", seq, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    seq.occupy_s = 600.0;  // a test figure, not the data's
    BatteryTactics tactics;
    campaign::BatteryTask t = BridgesTask();
    // Time enough: every step its full time (reconnaissance only for a new position), the march at the combat order's end.
    // Serial: 300 + 180 + 60 + 600 + 600 + 300 = 2040 s; a 20 min march, 600 s to occupy: ready at 3840 s, by 5400.
    campaign::BatteryPlan p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    std::printf("%s\n", campaign::PlanText(p, Clock19).c_str());
    SC_EXPECT(p.planned && p.Step("reconnaissance") == nullptr);
    SC_EXPECT_NEAR(p.scale, 1.0, 1e-12);
    SC_EXPECT_NEAR(p.order_s, 2040.0, 1e-9);
    SC_EXPECT_NEAR(p.march_at_s, 2040.0, 1e-9);
    SC_EXPECT_NEAR(p.arrive_s, 3240.0, 1e-9);
    SC_EXPECT_NEAR(p.ready_s, 3840.0, 1e-9);
    SC_EXPECT(p.InTime() && p.to_place == "bty21_bridges");
    const campaign::PlannedStep* pre = p.Step("preliminary");
    SC_EXPECT(pre != nullptr && pre->order == "order_battery_preliminary");
    if (pre != nullptr) SC_EXPECT_NEAR(pre->end_s, 300.0 + 180.0 + 60.0, 1e-9);  // before the situation and the decision
    const campaign::PlannedStep* support = p.Step("support");
    SC_EXPECT(support != nullptr);
    if (support != nullptr) SC_EXPECT_NEAR(support->start_s, p.order_s, 1e-9);  // alongside the march
    // Short of time (ready in 50 min): the steps between their short (690 s in all) and full times by one proportion, so
    // that the battery is ready just in time: (3000 - 1200 - 600 - 690) / (2040 - 690).
    t.ready_in_s = 3000.0;
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    SC_EXPECT_NEAR(p.scale, 510.0 / 1350.0, 1e-12);
    SC_EXPECT_NEAR(p.ready_s, 3000.0, 1e-9);
    SC_EXPECT(p.InTime());
    // Too short even for the short times: late by what's left over.
    t.ready_in_s = 2000.0;
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    SC_EXPECT_NEAR(p.scale, 0.0, 1e-12);
    SC_EXPECT_NEAR(p.order_s, 690.0, 1e-9);
    SC_EXPECT_NEAR(p.late_s, 690.0 + 1200.0 + 600.0 - 2000.0, 1e-9);
    SC_EXPECT(!p.InTime() && campaign::PlanText(p, Clock19).find("min late") != std::string::npos);
    // A position new to the battery: he reconnoitres it before he gives the order.
    t.ready_in_s = 5400.0;
    t.reconnoitre = true;
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    // 4440 s in full is more than the 3600 s left, so every step is shortened alike to fit: the order at 3600 s.
    SC_EXPECT(p.Step("reconnaissance") != nullptr);
    SC_EXPECT_NEAR(p.scale, (3600.0 - 1590.0) / (4440.0 - 1590.0), 1e-12);
    SC_EXPECT_NEAR(p.order_s, 3600.0, 1e-9);
    SC_EXPECT(p.InTime());
    // Already in the area: no march, the formation at the order.
    t.reconnoitre = false;
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(-1.0));
    SC_EXPECT(p.planned && p.march_at_s < 0.0 && p.has_formation);
    SC_EXPECT_NEAR(p.ready_s, 2040.0 + 600.0, 1e-9);
}

SC_TEST("battery mission (14.5): what the order ends in, by the mission's kind") {
    campaign::PlanSequence seq;
    SC_EXPECT(campaign::LoadPlanSequence(kData, "campaign/battery_plan.json", seq));
    BatteryTactics tactics;
    tactics.behind_covered_m = 2000.0;
    campaign::BatteryTask t = BridgesTask();
    // An object: the pairs across the main attack direction, centred on the main position (FormationSlots puts them
    // behind_covered_m behind `covered` along the axis).
    campaign::BatteryPlan p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    SC_EXPECT(p.has_formation && p.formation.kind == BatteryOrder::Kind::CombatFormation);
    SC_EXPECT_NEAR(p.formation.axis_deg, 255.0, 1e-9);
    const double a = DegToRad(255.0);
    SC_EXPECT_NEAR(p.formation.covered.x - std::sin(a) * 2000.0, 9600.0, 1e-6);
    SC_EXPECT_NEAR(p.formation.covered.y - std::cos(a) * 2000.0, -7000.0, 1e-6);
    // Troops: behind the covered companies, on their axis.
    t.kind = campaign::TaskKind::CoverTroops;
    t.axis_deg = 300.0;
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    SC_EXPECT(p.formation.kind == BatteryOrder::Kind::CombatFormation && p.formation.axis_deg == 300.0);
    SC_EXPECT_NEAR(p.formation.covered.x, 12000.0, 1e-9);
    // A crossing: the river approach at the crossing, across the river.
    t.kind = campaign::TaskKind::CoverCrossing;
    t.axis_deg = 90.0;
    t.width_m = 90.0;
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    SC_EXPECT(p.formation.kind == BatteryOrder::Kind::RiverCrossing && p.formation.width_m == 90.0 && p.formation.axis_deg == 90.0);
    // An ambush: the main position and the reserve as its prepared points, roving between them if the order says.
    t.kind = campaign::TaskKind::Ambush;
    t.roving = true;
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    SC_EXPECT(p.formation.kind == BatteryOrder::Kind::Ambush && p.formation.route.size() == 2 && p.formation.roving);
    // A relocation with no attack direction given: «К бою!» along the march's last leg, as for any march.
    t.kind = campaign::TaskKind::Relocate;
    t.situation.clear();
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    SC_EXPECT(p.planned && !p.has_formation && p.march_at_s >= 0.0);
    // Covering a march is not planned in this form, and says so; a place off the map stops the plan.
    t.kind = campaign::TaskKind::CoverMarch;
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    SC_EXPECT(!p.planned && p.why.find("march") != std::string::npos);
    t = BridgesTask();
    t.main = "atlantis";
    p = campaign::PlanBatteryTask(t, seq, tactics, Inputs(1200.0));
    SC_EXPECT(!p.planned && p.why.find("atlantis") != std::string::npos);
}

SC_TEST("battery mission (14.1-14.5): the regiment's order by radio to «к бою готов» west of the bridges by its ready-by") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_mission_cover.json", kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(def.has_task && def.clock_start_s == 19.0 * 3600.0);
    if (!def.has_task || !def.nav) return;
    const nav::Anchor* main = def.nav->GetAnchors().Find("bty21_bridges");
    SC_EXPECT(main != nullptr);
    if (main == nullptr) return;
    const auto at = main->at;
    ScenarioRunner r(std::move(def), std::move(console));
    while (!r.Finished() && r.Time() < 6600.0 && r.TaskReadyS() < 0.0) r.Step(0.05);
    for (int i = 0; i < 20 * 30; ++i) r.Step(0.05);
    const campaign::BatteryPlan& p = r.TaskPlan();
    std::printf("    received %s, the plan: order %s, «К маршу!» %s, at the position %s, ready %s by %s; reported ready %s\n",
                r.TaskClock(r.TaskReceivedS()).c_str(), r.TaskClock(p.order_s).c_str(), r.TaskClock(p.march_at_s).c_str(),
                r.TaskClock(p.arrive_s).c_str(), r.TaskClock(p.ready_s).c_str(), r.TaskClock(p.ready_by_s).c_str(),
                r.TaskClock(r.TaskReadyS()).c_str());
    // By radio on the regiment's net: the battery commander has it once he has read it back.
    SC_EXPECT(r.TaskReceivedS() > 60.0 && r.TaskReceivedS() < 120.0);
    SC_EXPECT(p.planned && p.march_at_s > r.TaskReceivedS() && p.InTime());
    const command::BatteryOrderNet* reg = r.GetRegimentOrders();
    SC_EXPECT(reg != nullptr && !reg->Book().Orders().empty() && reg->Book().Orders().front().kind == "task");
    // His papers: the order as taken down, and his plan.
    int papers = 0;
    for (const HeldPaper& h : r.MapCase("battery_commander")) {
        if (h.title.find("Combat order") != std::string::npos || h.title.find("Plan for the task") != std::string::npos) {
            ++papers;
            std::printf("    [%s]\n%s\n", h.title.c_str(), h.text.c_str());
        }
    }
    SC_EXPECT(papers == 2);
    // On the battery net: the preliminary order, then the combat order; each vehicle commander read both back and nobody
    // moved on them.
    const command::BatteryOrderNet* net = r.GetBatteryOrders();
    SC_EXPECT(net != nullptr);
    if (net == nullptr) return;
    const command::BatteryOrderRecord* pre = nullptr;
    const command::BatteryOrderRecord* combat = nullptr;
    for (const command::BatteryOrderRecord& o : net->Book().Orders()) {
        if (o.kind == "preliminary" && pre == nullptr) pre = &o;
        if (o.kind == "combat_order" && combat == nullptr) combat = &o;
    }
    SC_EXPECT(pre != nullptr && combat != nullptr);
    if (pre == nullptr || combat == nullptr) return;
    SC_EXPECT(pre->words.find("Предварительное распоряжение") == 0 && combat->words.find("Боевой приказ") == 0);
    // Said when his plan has them: the combat order as long after the preliminary as the plan's steps between them.
    const campaign::PlannedStep* pre_step = p.Step("preliminary");
    SC_EXPECT(pre_step != nullptr);
    if (pre_step != nullptr) SC_EXPECT(std::fabs((combat->given_h - pre->given_h) * 3600.0 - (p.order_s - pre_step->end_s)) < 1.0);
    for (const command::BatteryOrderRecord* o : {pre, combat}) {
        SC_EXPECT(o->to.size() >= 4);
        for (const command::OrderLeg& l : o->to) SC_EXPECT(l.Has(command::OrderStep::RepeatedBack));
    }
    // The combat order set the regime and every launcher's sector, then the march to the main position.
    SC_EXPECT(r.TaskSectorsSet() == 4);
    const Battery* b = r.GetBattery();
    SC_EXPECT(b != nullptr && b->Regime().kind == EmissionKind::Limited);
    const nav::MarchPlan* march = r.MarchPlanned();
    SC_EXPECT(march != nullptr && march->found);
    // The battery stands deployed round the main position, its pairs across the attack direction (255 deg): the
    // launchers' offsets from the position lie along 345/165 deg.
    if (b == nullptr) return;
    int launchers = 0;
    for (const Battery::Vehicle& v : b->Vehicles()) {
        if (v.def.kind != BatteryVehicleKind::Launcher) continue;
        ++launchers;
        const double dx = v.position.x - at.east_m, dy = v.position.y - at.north_m;
        const double along = dx * std::sin(DegToRad(255.0)) + dy * std::cos(DegToRad(255.0));
        std::printf("    %s at %.0f m from the position, %.0f m toward the attack, deployed %d\n", v.def.id.c_str(), std::hypot(dx, dy), along,
                    v.deployed ? 1 : 0);
        SC_EXPECT(v.deployed && std::hypot(dx, dy) < 2500.0 && std::fabs(along) < 400.0);
    }
    SC_EXPECT(launchers == 4);
    // «к бою готов» up to the regiment by the order's ready-by: the task is done in its book.
    SC_EXPECT(r.TaskReadyS() > p.arrive_s && r.TaskReadyS() <= p.ready_by_s);
    SC_EXPECT(reg != nullptr && reg->Book().Orders().front().For("bty21_commander") != nullptr &&
              reg->Book().Orders().front().For("bty21_commander")->Has(command::OrderStep::Done));
    const std::string debrief = r.Debrief();
    SC_EXPECT(debrief.find("The task (bty21_cover_bridges)") != std::string::npos && debrief.find("in time, to spare") != std::string::npos);
    // Each read back the order it heard, not the move in force.
    SC_EXPECT(debrief.find("я 21: предварительное распоряжение, понял") != std::string::npos &&
              debrief.find("я 21: боевой приказ, понял") != std::string::npos);
    // At the new position the commander had the R-111 back on the regiment's net (its antenna to the mast, tuned) to report.
    SC_EXPECT(r.Console().panel.Get("radio.fug3.antenna") == 1 && r.Console().panel.Get("radio.fug3.mode") == 3);
}

SC_TEST("battery mission (14.3): in the battery commander's seat the plan's orders are the player's, owed when due and given from the menu") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_mission_cover.json", kData, def, console, &error));
    if (!def.has_task) return;
    ScenarioRunner r(std::move(def), std::move(console));
    r.TakeSeat("battery_commander", "Player");
    r.SetHelper(true);
    const auto owes = [&](const std::string& id) {
        for (const SeatDuty& d : r.OwedDuties("battery_commander")) {
            if (d.kind == SeatDuty::Kind::Order && d.id == id) return true;
        }
        return false;
    };
    const auto said = [&](const char* words) {
        for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) {
            if (m.net == "battery net" && m.text.find(words) != std::string::npos) return true;
        }
        return false;
    };
    // Before the task there is nothing to give.
    SC_EXPECT(!r.GiveCrewOrder("order_battery_preliminary") && r.LastRefusal() == "no task from the regiment to give it for");
    while (r.Time() < 300.0) r.Step(0.05);
    SC_EXPECT(r.TaskReceivedS() > 0.0);
    const campaign::BatteryPlan p = r.TaskPlan();
    while (r.Time() < p.Step("preliminary")->end_s + 5.0) r.Step(0.05);
    // Due: owed, not said for him.
    SC_EXPECT(owes("order_battery_preliminary") && !said("Предварительное распоряжение"));
    SC_EXPECT(r.GiveCrewOrder("order_battery_preliminary"));
    r.Step(0.05);
    SC_EXPECT(said("Предварительное распоряжение") && !owes("order_battery_preliminary"));
    SC_EXPECT(!r.GiveCrewOrder("order_battery_preliminary") && r.LastRefusal() == "given already");
    // The combat order: owed at its time; given, the march follows it as his to give.
    while (r.Time() < p.order_s + 5.0) r.Step(0.05);
    SC_EXPECT(owes("order_battery_combat_order") && !said("Боевой приказ"));
    SC_EXPECT(r.GiveCrewOrder("order_battery_combat_order"));
    for (int i = 0; i < 20 * 3; ++i) r.Step(0.05);
    SC_EXPECT(said("Боевой приказ") && owes("order_battery_march"));
}
