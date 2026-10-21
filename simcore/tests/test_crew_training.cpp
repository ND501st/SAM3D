// In the crew and in command (roadmap 8.7, 8.8; docs/crew/07_COMMANDER_MODE.md, 08_TRAINING_AND_ASSISTANCE.md): the
// order menu's honest greying and refusals, the training overlay's levels, and the verdict on a player's moves.
#include <fstream>
#include <sstream>
#include <string>

#include "simcore/crew_decisions.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadAll(const std::string& path) {
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

std::string Replace(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at != std::string::npos) s.replace(at, from.size(), to);
    return s;
}

// The crew practice scenario (NPC commander, from cold), with an assistance level and the hull's heading.
std::unique_ptr<ScenarioRunner> Practice(const std::string& assistance = "guided", const std::string& heading = "0") {
    std::string text = ReadAll(kData + "/scenarios/osa_crew_practice.json");
    text = Replace(text, "\"heading_deg\": 0", "\"heading_deg\": " + heading);
    text = Replace(text, "\"crew_commander\"", "\"assistance\": \"" + assistance + "\", \"crew_commander\"");
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenario(text, kData, def, console, &e)) {
        std::printf("    %s\n", e.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

bool Said(const ScenarioRunner& r, const std::string& id, const std::string& from, const std::string& words) {
    for (const SpokenCallout& c : r.Callouts()->Log()) {
        if (c.id == id && c.from == from && c.ru.find(words) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

SC_TEST("order menu: orders that can't be carried out are greyed, with the reason") {
    std::unique_ptr<ScenarioRunner> r = Practice();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->Def().assistance == AssistLevel::Guided);
    // From cold, nothing in the air: К бою can be given; taking a target, permitting a launch, the jammer can't.
    SC_EXPECT(r->CheckOrder("order_k_boyu").can);
    ScenarioRunner::OrderCheck c = r->CheckOrder("order_take_target");
    SC_EXPECT(!c.can && c.reason_en == "no target reported" && c.reason_ru == "цели нет");
    c = r->CheckOrder("fire_permitted");
    SC_EXPECT(!c.can && c.reason_en == "nothing tracked");
    SC_EXPECT(!r->CheckOrder("order_jammer").can);
    SC_EXPECT(!r->CheckOrder("order_no_such").can);
    // Through the fight: К бою is "done already" once ready; the launch permission waits for the target in the zone
    // (greyed with the crew data's words), then opens.
    bool ready_done = false, zone_reason = false, permit_open = false;
    for (int i = 0; i < 9000 && !r->Finished(); ++i) {
        r->Step(0.1);
        const ScenarioRunner::OrderCheck k = r->CheckOrder("order_k_boyu");
        ready_done = ready_done || (!k.can && k.reason_en == "done already");
        const ScenarioRunner::OrderCheck p = r->CheckOrder("fire_permitted");
        zone_reason = zone_reason || (!p.can && p.reason_ru == "Цель не в зоне");
        permit_open = permit_open || p.can;
        if (r->Launches() > 0) break;
    }
    SC_EXPECT(ready_done);
    SC_EXPECT(zone_reason);
    SC_EXPECT(permit_open);
}

SC_TEST("order menu: an order that can't be carried out is answered \"Не могу\" with the reason, and nothing starts") {
    std::unique_ptr<ScenarioRunner> r = Practice();
    SC_EXPECT(r != nullptr);
    if (!r) return;
    ScenarioRunner::OrderCheck why;
    SC_EXPECT(!r->TryCrewOrder("fire_permitted", &why));
    SC_EXPECT(why.reason_ru == "сопровождения нет");
    for (int i = 0; i < 80; ++i) r->Step(0.1);
    SC_EXPECT(Said(*r, "cannot", "deputy_commander", "сопровождения нет"));
    SC_EXPECT(r->Launches() == 0);
    // An order that can be carried out goes through.
    SC_EXPECT(r->TryCrewOrder("order_radio_on"));
}

SC_TEST("training: guided shows the best move, unassisted and exam nothing, hint once the player hesitates") {
    for (const char* level : {"guided", "unassisted", "exam", "hint"}) {
        std::unique_ptr<ScenarioRunner> r = Practice(level);
        SC_EXPECT(r != nullptr);
        if (!r) continue;
        SC_EXPECT(r->TakeSeat("operator", "Player"));
        for (int i = 0; i < 20; ++i) r->Step(0.1);  // 2 s in
        const AssistView early = r->Assistance("operator");
        SC_EXPECT(early.task == "ready_halted");
        const std::string l = level;
        if (l == "guided") SC_EXPECT(early.show && !early.control.empty() && !early.why.empty());
        if (l == "unassisted" || l == "exam" || l == "hint") SC_EXPECT(!early.show);
        for (int i = 0; i < 150; ++i) r->Step(0.1);  // 17 s without a move: past any tier's patience
        const AssistView later = r->Assistance("operator");
        if (l == "hint") SC_EXPECT(later.show && !later.control.empty());
        if (l == "unassisted" || l == "exam") SC_EXPECT(!later.show);
    }
}

SC_TEST("training: demonstrate names the control the seat's NPC is about to move") {
    std::unique_ptr<ScenarioRunner> r = Practice("demonstrate");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    bool shown = false, matched = false;
    for (int i = 0; i < 1200 && !matched; ++i) {
        r->Step(0.1);
        const AssistView v = r->Assistance("operator");
        if (!v.show) continue;
        shown = true;
        SC_EXPECT(v.demonstrating);
        for (const SeatActivity& a : r->CrewActivity()) {
            if (a.station == "operator" && a.control == v.control) matched = true;
        }
    }
    SC_EXPECT(shown);
    SC_EXPECT(matched);
}

SC_TEST("training: a player's best move costs nothing; undoing the drill is judged with what it costs and reaches the debrief") {
    std::unique_ptr<ScenarioRunner> r = Practice("guided");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("operator", "Player"));
    // The player follows the overlay until the equipment is on.
    for (int i = 0; i < 3000 && r->Console().panel.Get("pwr.equipment") != 1; ++i) {
        r->Step(0.1);
        if (i % 10 != 0) continue;
        const AssistView v = r->Assistance("operator");
        if (v.show && v.due_in_s < 1.0) r->SeatSet("operator", v.control, v.position);
    }
    SC_EXPECT(r->Console().panel.Get("pwr.equipment") == 1);
    SC_EXPECT(!r->MoveVerdicts().empty());
    bool all_best = true;
    for (const MoveVerdict& v : r->MoveVerdicts()) all_best = all_best && v.best;
    SC_EXPECT(all_best);
    // Now he switches the equipment off again: off the drill, and it costs the warm-up.
    for (int i = 0; i < 30; ++i) r->Step(0.1);
    r->SeatSet("operator", "pwr.equipment", 0);
    const MoveVerdict* v = r->LastVerdict("operator");
    SC_EXPECT(v != nullptr && v->control == "pwr.equipment" && v->position == 0);
    if (v != nullptr) {
        std::printf("    off the drill: %.1f s lost; the best was %s\n", v->cost_s, v->best_control.c_str());
        SC_EXPECT(!v->best && v->cost_s > 0.3);  // more than a step: what it costs depends on the critical path
    }
    SC_EXPECT(r->Debrief().find("off the drill") != std::string::npos);
}

// ------------------------------------------------------------------------------------ decisions (06 §3.6)

namespace {
// A plain firing zone (generic placeholder numbers): 1.5-10 km, 10-5000 m up; missiles at 500 m/s on average.
EngagementTuning BoxTuning() {
    EngagementTuning t;
    t.zone_min_range_m = 1500.0;
    t.zone_max_range_m = 10000.0;
    t.zone_min_alt_m = 10.0;
    t.zone_max_alt_m = 5000.0;
    t.missile_average_speed_mps = 500.0;
    t.missile_max_flight_time_s = 30.0;
    t.hit_chance["default"] = 0.6;
    t.outside_zone_factor = 0.3;
    return t;
}
DecisionTrack Inbound(const std::string& id, double north_m, double speed_mps) {
    DecisionTrack d;
    d.target = id;
    d.position = Vec3{0.0, north_m, 1000.0};
    d.velocity = Vec3{0.0, -speed_mps, 0.0};
    d.agl_m = 1000.0;
    return d;
}
}  // namespace

SC_TEST("decisions: the best launch is at the zone's far edge; the window allows a second missile") {
    const EngagementTuning t = BoxTuning();
    // 20 km out, 1 km up, at 300 m/s. The launch zone is where a missile launched now meets it inside the zone: the
    // meeting at the far edge, 10 km slant (9950 m along the ground), takes 10000 / 500 = 20 s, so the launch is from
    // 9950 + 300 x 20 = 15950 m (13.5 s from now); the last meeting at 1.5 km slant (1118 m) takes 3 s, from 2018 m
    // (59.9 s). The next missile can go 20 + 11.2 s after the first (44.7 s, still in the zone): two shots.
    const DecisionTrack d = Inbound("t1", 20000.0, 300.0);
    const LaunchTiming lt = BestLaunch(t, d, 11.2);
    SC_EXPECT(lt.reachable);
    SC_EXPECT_NEAR(lt.window.enter_s, 13.5, 0.3);
    SC_EXPECT_NEAR(lt.window.leave_s, 59.94, 0.3);
    SC_EXPECT_NEAR(lt.best.flight_s, 20.0, 0.2);
    SC_EXPECT_NEAR(lt.best.intercept_range_m, 10000.0, 100.0);
    SC_EXPECT(lt.best.in_zone && lt.best.shots == 2);
    SC_EXPECT_NEAR(lt.best.kill, 1.0 - 0.4 * 0.4, 1e-9);
    // Waiting costs the second missile; launching early is outside the zone (it would meet the target 11.6 km out).
    const LaunchOption late = EvaluateLaunch(t, d, 50.0, 11.2, lt.window);
    SC_EXPECT(late.shots == 1 && late.kill < lt.best.kill);
    const LaunchOption early = EvaluateLaunch(t, d, 5.0, 11.2, lt.window);
    SC_EXPECT(!early.in_zone && early.chance < lt.best.chance);
}

SC_TEST("decisions: the nearer threat first; a friend is never chosen") {
    const EngagementTuning t = BoxTuning();
    DecisionTrack friendly = Inbound("friend", 9000.0, 250.0);
    friendly.friendly = true;
    const std::vector<TargetScore> s = ScoreTargets(t, {Inbound("far", 30000.0, 250.0), Inbound("near", 12000.0, 250.0), friendly}, 11.2);
    SC_EXPECT(s.size() == 3u);
    SC_EXPECT(s[0].target == "near" && s[1].target == "far");
    SC_EXPECT(s[2].target == "friend" && s[2].score < 0.0);
    // A target that never reaches the zone scores nothing.
    const std::vector<TargetScore> away = ScoreTargets(t, {Inbound("away", 30000.0, -250.0)}, 11.2);
    SC_EXPECT(away[0].zone_in_s < 0.0 && away[0].score == 0.0);
}

SC_TEST("decisions: the commander's choice of target and his launch are judged in the debrief") {
    std::unique_ptr<ScenarioRunner> r = Practice("guided");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    for (int i = 0; i < 9000 && !r->Finished() && r->Launches() == 0; ++i) r->Step(0.1);
    for (int i = 0; i < 20; ++i) r->Step(0.1);
    bool target = false, launch = false;
    for (const ScenarioRunner::DecisionRecord& d : r->Decisions()) {
        if (d.kind == "target") target = target || (d.target == "t1" && d.best == "t1" && d.good);
        if (d.kind == "launch") {
            launch = launch || (d.target == "t1" && d.ideal_s > 0.0);
            std::printf("    launch at %.1f s, ideal %.1f s, %.1f s late\n", d.time_s, d.ideal_s, d.late_s);
        }
    }
    SC_EXPECT(target);
    SC_EXPECT(launch);
    SC_EXPECT(r->Debrief().find("Decisions") != std::string::npos);
}

SC_TEST("npc commander: ready at a halt with nothing about, he has the vehicle oriented before the fight") {
    std::unique_ptr<ScenarioRunner> r = Practice("guided", "75");  // the hull 75 degrees off north: the gyro must be set
    SC_EXPECT(r != nullptr);
    if (!r) return;
    double oriented_s = -1.0;
    for (int i = 0; i < 9000 && !r->Finished() && r->Launches() == 0; ++i) {
        r->Step(0.1);
        if (oriented_s < 0.0 && r->Time() > 60.0 && r->Oriented()) oriented_s = r->Time();
    }
    std::printf("    oriented at %.0f s; first launch at %.0f s\n", oriented_s, r->Time());
    SC_EXPECT(oriented_s > 0.0);
    SC_EXPECT(r->Oriented());  // and it stays so: the gyro half-compass keeps the heading
    SC_EXPECT(r->Launches() > 0);
    SC_EXPECT(Said(*r, "order_orient", "commander", "Ориентировать"));
}
