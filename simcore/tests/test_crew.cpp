// Phase 8, crew and teamwork (docs/crew/): the crew data, skill in behaviour, and the drill master (the reference
// performer that knows each seat's best move).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_callouts.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/drill_master.hpp"
#include "simcore/procedures.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string ReplaceAll(std::string s, const std::string& from, const std::string& to) {
    std::size_t at = 0;
    while ((at = s.find(from, at)) != std::string::npos) {
        s.replace(at, from.size(), to);
        at += to.size();
    }
    return s;
}

struct Osa {
    ConsoleData console;
    CrewData crew;
    SkillTable skills;
    bool ok = false;
    Osa() {
        std::string e;
        ok = LoadConsoleFile(kData + "/procedures/osa_ak.json", console, &e) &&
             LoadCrewDataFile(kData + "/crew/osa_ak_crew.json", console, crew, &e) &&
             LoadSkillTableFile(kData + "/crew/skill_levels.json", skills, &e);
        if (!ok) std::printf("    %s\n", e.c_str());
    }
};

bool BadCrew(const std::string& from, const std::string& to, const std::string& expect) {
    Osa o;
    const std::string text = ReplaceAll(ReadFile(kData + "/crew/osa_ak_crew.json"), from, to);
    CrewData d;
    std::string e;
    const bool ok = LoadCrewData(text, o.console, d, &e);
    if (ok || e.find(expect) == std::string::npos) std::printf("    expected \"%s\", got ok=%d \"%s\"\n", expect.c_str(), ok ? 1 : 0, e.c_str());
    return !ok && e.find(expect) != std::string::npos;
}

bool BadSkill(const std::string& from, const std::string& to, const std::string& expect) {
    const std::string text = ReplaceAll(ReadFile(kData + "/crew/skill_levels.json"), from, to);
    SkillTable t;
    std::string e;
    const bool ok = LoadSkillTable(text, t, &e);
    if (ok || e.find(expect) == std::string::npos) std::printf("    expected \"%s\", got ok=%d \"%s\"\n", expect.c_str(), ok ? 1 : 0, e.c_str());
    return !ok && e.find(expect) != std::string::npos;
}

// Run one crew member through a procedure on the console until it is done; the time it took.
double TimeToDo(Osa& o, CrewMember& m, const std::string& procedure, double max_s = 120.0) {
    m.Order(*o.console.FindProcedure(procedure));
    double t = 0.0;
    while (m.IsBusy() && t < max_s) {
        t += 0.1;
        o.console.equipment.Update(0.1, o.console.panel);
        o.console.panel.ReleaseMomentaries();
        m.Update(0.1, o.console.panel, &o.console.equipment);
    }
    return t;
}

const CrewTask& TaskOf(const Osa& o, const std::string& id) { return *o.crew.FindTask(id); }

bool HasAction(const IdealTimeline& tl, const std::string& station, const std::string& control) {
    for (const DrillAction& a : tl.actions) {
        if (a.station == station && a.control == control) return true;
    }
    return false;
}

}  // namespace

// ------------------------------------------------------------------------------------------ 8.1 crew data

SC_TEST("crew data: the Osa's crew file and skill tiers load and agree with the console") {
    Osa o;
    SC_EXPECT(o.ok);
    SC_EXPECT(o.crew.seats.size() == 5);
    SC_EXPECT(o.crew.FindSeat("senior_operator")->number == 2 && o.crew.FindSeat("operator")->number == 1);
    SC_EXPECT(o.crew.callouts.size() > 80);
    const OrderDef* k = o.crew.FindOrder("order_k_boyu");
    SC_EXPECT(k != nullptr && k->task == "ready_halted" && k->procedures.size() == 5);
    SC_EXPECT(o.crew.FindCallout("power_on")->ru == "Питание подано!");  // voenizdat_osa_1991 p246
    SC_EXPECT(o.crew.FindFact("vehicle_stopped")->by_action);
    SC_EXPECT(o.crew.FindTask("ready_halted")->conditions.size() == 7);
    SC_EXPECT(!o.crew.reports.empty());
    SC_EXPECT(o.skills.tiers.size() == 5 && o.skills.Reference()->id == "master");
    SC_EXPECT(o.skills.Find("recruit")->reaction_s > o.skills.Find("master")->reaction_s);
}

SC_TEST("crew data: bad references and values are refused with their paths") {
    SC_EXPECT(BadCrew("\"procedure\": \"power_up\" }", "\"procedure\": \"fly\" }", "orders[0].procedures[0].procedure: unknown procedure 'fly'"));
    SC_EXPECT(BadCrew("{ \"station\": \"driver\", \"procedure\": \"power_up\" }, { \"station\": \"operator\", \"procedure\": \"equipment_on\" },",
                      "{ \"station\": \"operator\", \"procedure\": \"power_up\" }, { \"station\": \"operator\", \"procedure\": \"equipment_on\" },",
                      "is the driver's, not the operator's"));
    SC_EXPECT(BadCrew("{ \"input\": \"launch_hold\"", "{ \"input\": \"no_such_input\"", "unknown input 'no_such_input'"));
    SC_EXPECT(BadCrew("\"task\": \"ready_halted\"", "\"task\": \"nap\"", "orders[0].task: unknown task 'nap'"));
    SC_EXPECT(BadCrew("\"id\": \"ack\", \"kind\": \"ack\"", "\"id\": \"ack\", \"kind\": \"shout\"", "must be order, ack, report"));
    SC_EXPECT(BadCrew("\"system_id\": \"osa_ak\"", "\"system_id\": \"s75\"", "is not the console's 'osa_ak'"));
    SC_EXPECT(BadSkill("\"slip_chance\": 0.06", "\"slip_chance\": 1.5", "tiers[0].slip_chance: must be from 0 to 1"));
    SC_EXPECT(BadSkill("\"reference_tier\": \"master\"", "\"reference_tier\": \"god\"", "reference_tier: unknown tier 'god'"));
    SC_EXPECT(BadSkill("\"knows\": \"basic\"", "\"knows\": \"some\"", "must be basic or all"));
}

// ------------------------------------------------------------------------------------------ 8.2 skill

SC_TEST("skill: a crew member's pace comes from its tier: a master works a drill several times faster than a recruit") {
    Osa a, b;
    CrewMember master("M", "deputy_commander", 1.0), recruit("R", "deputy_commander", 1.0);
    CrewSkill sm = ToCrewSkill(*a.skills.Find("master"));
    CrewSkill sr = ToCrewSkill(*a.skills.Find("recruit"));
    sr.slip_chance = sr.omission_chance = 0.0;  // pace only
    master.SetSkill(sm, 1u);
    recruit.SetSkill(sr, 1u);
    const double tm = TimeToDo(a, master, "stow_launch_console");
    const double tr = TimeToDo(b, recruit, "stow_launch_console");
    std::printf("    three steps: master %.1f s, recruit %.1f s\n", tm, tr);
    SC_EXPECT(tm < 2.6 && tr > 4.5);
    SC_EXPECT(tr > 2.5 * tm);
}

SC_TEST("skill: a slip is an out-of-order action in the debrief; a forgotten step costs the crewmate's patience") {
    Osa o;
    CrewMember m("Deputy", "deputy_commander", 1.0);
    CrewSkill s = ToCrewSkill(*o.skills.Find("class2"));
    s.slip_chance = 1.0;
    s.omission_chance = 0.0;
    m.SetSkill(s, 3u);
    SessionLog log;
    m.Order(*o.console.FindProcedure("stow_launch_console"));
    for (int i = 0; i < 200 && m.IsBusy(); ++i) {
        o.console.equipment.Update(0.1, o.console.panel);
        m.Update(0.1, o.console.panel, &o.console.equipment);
        log.CollectCrew(m, i * 0.1);
    }
    SC_EXPECT(!m.IsBusy() && m.Slips() >= 1);
    SC_EXPECT(log.Summary().out_of_order_actions >= 1);  // a crew mistake, like a player's
    Osa p;
    CrewMember f("Deputy", "deputy_commander", 1.0);
    CrewSkill sf = ToCrewSkill(*p.skills.Find("class2"));
    sf.slip_chance = 0.0;
    sf.omission_chance = 1.0;
    sf.jitter_s = 0.0;
    f.SetSkill(sf, 3u);
    const double t = TimeToDo(p, f, "stow_launch_console");
    SC_EXPECT(f.Omissions() == 3);
    SC_EXPECT_NEAR(t, 3.0 * (sf.reaction_s + sf.patience_s), 0.5);
}

SC_TEST("skill: stress slows a crewmate by its tier's stress factor") {
    Osa a, b;
    CrewMember calm("C", "deputy_commander", 1.0), stressed("S", "deputy_commander", 1.0);
    CrewSkill s = ToCrewSkill(*a.skills.Find("class3"));
    s.jitter_s = s.slip_chance = s.omission_chance = 0.0;
    calm.SetSkill(s, 1u);
    stressed.SetSkill(s, 1u);
    stressed.SetStressed(true);
    const double tc = TimeToDo(a, calm, "stow_launch_console");
    const double ts = TimeToDo(b, stressed, "stow_launch_console");
    SC_EXPECT_NEAR(ts / tc, s.stress_factor, 0.15);
}

SC_TEST("skill: in a scenario a master crew engages sooner than a recruit crew") {
    auto scenario = [](const std::string& tier) {
        std::string t = R"({ "scenario_id": "s", "name": "S", "source": "sam3d", "confidence": "estimate", "seed": 2,
          "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 }, "terrain": { "type": "flat", "height_m": 0 },
          "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json",
                        "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
          "crew": [ { "name": "Commander", "role": "commander", "skill": "TIER" }, { "name": "Deputy", "role": "deputy_commander", "skill": "TIER" },
                    { "name": "Senior operator", "role": "senior_operator", "skill": "TIER" }, { "name": "Operator", "role": "operator", "skill": "TIER" },
                    { "name": "Driver", "role": "driver", "skill": "TIER" } ],
          "targets": [ { "id": "t1", "name": "T", "class": "drone", "start": { "east_m": 1500, "north_m": 24000, "up_m": 1000 },
                         "waypoints": [ { "east_m": 1500, "north_m": -20000, "up_m": 1000 } ], "speed_mps": 150, "appear_at_s": 260 } ],
          "triggers": [
            { "id": "up", "when": {}, "do": [ { "order": "power_up", "crew": "Driver" }, { "queue": "pto_on_line", "crew": "Driver" },
                                               { "order": "equipment_on", "crew": "Operator" }, { "queue": "deploy_at_halt", "crew": "Operator" },
                                               { "order": "search_on_march", "crew": "Senior operator" } ] },
            { "id": "acq", "when": { "after_s": 260, "conditions": [ { "subsystem": "soc_transmitter", "is": "ready" } ] },
              "do": [ { "order": "acquire_with_soc", "crew": "Senior operator" }, { "order": "ready_capture", "crew": "Deputy" },
                      { "order": "capture_with_ssc", "crew": "Operator" },
                      { "queue": "prepare_firing", "crew": "Deputy" }, { "queue": "launch", "crew": "Deputy" } ] } ],
          "end": { "time_limit_s": 600 } })";
        return ReplaceAll(t, "TIER", tier);
    };
    auto first = [&](const std::string& tier, const std::string& what) {
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        if (!LoadScenario(scenario(tier), kData, def, console, &e)) {
            std::printf("    %s\n", e.c_str());
            return -1.0;
        }
        ScenarioRunner r(std::move(def), std::move(console));
        while (!r.Finished() && r.Time() < 600.0) {
            r.Step(0.1);
            if (what == "track" && r.Engagement().Method() == TrackMethod::Radar) return r.Time();
            if (what == "launch" && r.Launches() > 0) return r.Time();
        }
        return -1.0;
    };
    const double master_track = first("master", "track"), recruit_track = first("recruit", "track");
    const double master_launch = first("master", "launch"), recruit_launch = first("recruit", "launch");
    std::printf("    master: track %.1f s, launch %.1f s; recruit: track %.1f s, launch %.1f s\n", master_track, master_launch,
                recruit_track, recruit_launch);
    SC_EXPECT(master_track > 0.0 && recruit_track > 0.0 && master_track < recruit_track);
    SC_EXPECT(master_launch > 0.0 && (recruit_launch < 0.0 || master_launch < recruit_launch));
    // Unknown tiers are refused with the path.
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(!LoadScenario(scenario("genius"), kData, def, console, &e) && e.find("crew[0].skill: unknown skill tier 'genius'") != std::string::npos);
}

// ------------------------------------------------------------------------------------------ 8.3 the drill master

SC_TEST("drill master: from cold, the ideal К бою is the warm-up chain plus a few reactions, with every seat's part") {
    Osa o;
    SC_EXPECT(o.ok);
    o.console.equipment.SetInput("vehicle_stopped", true);
    const DrillMaster dm(o.console, &o.crew, o.skills.Reference()->reaction_s);
    const IdealTimeline tl = dm.Ideal(TaskOf(o, "ready_halted"), o.console);
    std::printf("    ready, halted from cold: %.1f s ideal, %zu actions\n", tl.time_s, tl.actions.size());
    SC_EXPECT(tl.reached);
    // The longest warm-up chain in the data: the turbine (60 s), then stabilisation (180 s), then the mast (10 s).
    SC_EXPECT(tl.time_s > 250.0 && tl.time_s < 256.0);
    SC_EXPECT(HasAction(tl, "driver", "pwr.gta") && HasAction(tl, "operator", "pwr.equipment") &&
              HasAction(tl, "operator", "apu.lock") && HasAction(tl, "senior_operator", "soc.hv") &&
              HasAction(tl, "deputy_commander", "tov.power"));
    // The driver's turbine comes at once.
    bool gta_at_once = false;
    for (const DrillAction& a : tl.actions) gta_at_once = gta_at_once || (a.control == "pwr.gta" && a.time_s < 1.0);
    SC_EXPECT(gta_at_once);
}

SC_TEST("drill master: the plan follows the sourced drills and never pulls in a drill that would do much else") {
    Osa o;
    o.console.equipment.SetInput("vehicle_stopped", true);
    const DrillMaster dm(o.console, &o.crew);
    const DrillPlan plan = dm.Plan(TaskOf(o, "ready_halted"), o.console.panel, o.console.equipment);
    auto drills = [&](const std::string& seat) {
        const DrillPlan::Seat* s = plan.FindSeat(seat);
        return s != nullptr ? s->procedures : std::vector<std::string>();
    };
    SC_EXPECT(drills("driver") == std::vector<std::string>{"power_up"});
    SC_EXPECT(drills("operator") == (std::vector<std::string>{"equipment_on", "deploy_at_halt"}));
    SC_EXPECT(drills("senior_operator") == std::vector<std::string>{"search_on_march"});
    SC_EXPECT(drills("deputy_commander").empty());  // the TV camera is one switch: not the EMCON engagement drill
    const DrillPlan::Seat* dep = plan.FindSeat("deputy_commander");
    SC_EXPECT(dep != nullptr && dep->loose.size() == 1 && dep->loose[0].control == "tov.power");
    SC_EXPECT(plan.world.empty());
}

SC_TEST("drill master: the best next move for every seat, with why and when") {
    Osa o;
    o.console.equipment.SetInput("vehicle_stopped", true);
    const DrillMaster dm(o.console, &o.crew);
    const std::vector<DrillAdvice> best = dm.NextBest(TaskOf(o, "ready_halted"), o.console);
    const DrillAdvice drv = dm.NextFor("driver", TaskOf(o, "ready_halted"), o.console);
    SC_EXPECT(drv.control == "pwr.gta" && drv.position == 1 && drv.due_in_s < 1.0);
    SC_EXPECT(!drv.why.empty() && drv.why.back().find("the goal") == 0);
    const DrillAdvice op = dm.NextFor("operator", TaskOf(o, "ready_halted"), o.console);
    SC_EXPECT(op.control == "pwr.equipment");
    SC_EXPECT(op.due_in_s > 59.0 && op.due_in_s < 63.0);  // after the turbine's 60 s: the drill waits for power
    std::string chain;
    for (const std::string& w : op.why) chain += w + " < ";
    std::printf("    operator: %s in %.1f s, because %s\n", op.description.c_str(), op.due_in_s, chain.c_str());
    SC_EXPECT(chain.find("ready") != std::string::npos);
    SC_EXPECT(best.size() >= 4);
}

SC_TEST("drill master: from a state halfway through, it plans only what is left") {
    Osa o;
    ConsoleData& c = o.console;
    c.equipment.SetInput("vehicle_stopped", true);
    // The driver and the operator have done their parts and everything has warmed up.
    for (const auto& m : std::vector<std::pair<std::string, int>>{{"pwr.gta", 1}, {"gyro.arrest", 1}, {"pwr.equipment", 1}, {"stab.power", 1},
                                                                  {"soc.rotation_lock", 1}, {"tov.supply", 1}, {"apu.lock", 1}, {"drv.az", 1}, {"drv.el", 1}}) {
        c.panel.Set(m.first, m.second);
    }
    for (const std::string& s : c.equipment.UnitIds()) {
        c.equipment.Warm(s, c.panel);
        c.equipment.Update(0.0, c.panel);
    }
    const DrillMaster dm(c, &o.crew);
    const IdealTimeline tl = dm.Ideal(TaskOf(o, "ready_halted"), c);
    std::printf("    from halfway: %.1f s\n", tl.time_s);
    SC_EXPECT(tl.reached && tl.time_s < 15.0);
    SC_EXPECT(!HasAction(tl, "driver", "pwr.gta") && HasAction(tl, "senior_operator", "soc.hv"));
}

SC_TEST("drill master: a crew's part of a capture is planned; the run then waits for the world (the target)") {
    Osa o;
    o.console.equipment.SetInput("vehicle_stopped", true);
    const DrillMaster dm(o.console, &o.crew);
    const DrillPlan plan = dm.Plan(TaskOf(o, "captured"), o.console.panel, o.console.equipment);
    SC_EXPECT(plan.world.size() == 1 && plan.world[0].id == "target_captured");
    const IdealTimeline tl = dm.Ideal(TaskOf(o, "captured"), o.console, 400.0);
    SC_EXPECT(!tl.reached && tl.blocked_on.size() >= 1 && tl.blocked_on[0].id == "target_captured");
    // The seats' parts of a capture: the operator's transmitter and drives, the deputy's capture mode.
    SC_EXPECT(HasAction(tl, "operator", "ssc.tx") && HasAction(tl, "operator", "drv.az"));
    SC_EXPECT(HasAction(tl, "deputy_commander", "fo.cu_az") || HasAction(tl, "deputy_commander", "fo.az") ||
              HasAction(tl, "deputy_commander", "fo.tsc"));
}

SC_TEST("drill master: moving to the march, the driver halts first when the vehicle is moving") {
    Osa o;  // moving: vehicle_stopped false
    const DrillMaster dm(o.console, &o.crew);
    const DrillPlan plan = dm.Plan(TaskOf(o, "ready_halted"), o.console.panel, o.console.equipment);
    const DrillPlan::Seat* drv = plan.FindSeat("driver");
    SC_EXPECT(drv != nullptr && drv->world_actions.size() == 1 && drv->world_actions[0] == "vehicle_stopped");
    const IdealTimeline tl = dm.Ideal(TaskOf(o, "ready_halted"), o.console);
    SC_EXPECT(tl.reached);
}

SC_TEST("drill master: the cost of a move: none for the best one, the goal lost for one that faults equipment") {
    Osa o;
    o.console.equipment.SetInput("vehicle_stopped", true);
    const DrillMaster dm(o.console, &o.crew);
    const DrillGoal goal = TaskOf(o, "ready_halted");
    SC_EXPECT_NEAR(dm.CostOf(goal, o.console, "pwr.gta", 1), 0.0, 1e-9);
    // Unlocking the АПУ while the vehicle is still moving faults it (its prerequisite is a halted vehicle): the cost is
    // the recovery (lock it again, halt, unlock), not the goal lost.
    Osa m;
    const DrillMaster dm2(m.console, &m.crew);
    const double cost = dm2.CostOf(goal, m.console, "apu.lock", 1, 600.0);
    std::printf("    unlocking on the move costs %.1f s\n", cost);
    SC_EXPECT(cost > 0.5 && cost < 600.0);
}

SC_TEST("drill master: from a faulted subsystem it plans the recovery: off, the prerequisite, on again") {
    Osa o;  // moving
    o.console.panel.Set("apu.lock", 1);
    o.console.equipment.Update(0.0, o.console.panel);
    SC_EXPECT(o.console.equipment.State("apu_free") == SubsystemState::Fault);
    const DrillMaster dm(o.console, &o.crew);
    const IdealTimeline tl = dm.Ideal(TaskOf(o, "ready_halted"), o.console);
    SC_EXPECT(tl.reached);
    double off_s = -1.0, on_s = -1.0;
    for (const DrillAction& a : tl.actions) {
        if (a.control == "apu.lock" && a.position == 0 && off_s < 0.0) off_s = a.time_s;
        if (a.control == "apu.lock" && a.position == 1 && off_s >= 0.0 && on_s < 0.0) on_s = a.time_s;
    }
    SC_EXPECT(off_s >= 0.0 && on_s > off_s);
}

// ------------------------------------------------------------------------------------------ 8.4 callouts

SC_TEST("callouts: one voice at a time per channel, urgent first, speaking takes time, fields filled in") {
    Osa o;
    CalloutBus bus(&o.crew);
    SC_EXPECT(bus.Say(0.0, "done", "operator"));
    SC_EXPECT(bus.Say(0.0, "contact", "senior_operator", {{"azimuth", "05-40"}, {"range_km", "24"}}));
    SC_EXPECT(bus.Say(0.0, "captured", "operator"));  // urgent: spoken first
    SC_EXPECT(!bus.Say(0.0, "no_such_callout", "operator"));
    std::vector<SpokenCallout> heard;
    for (int i = 1; i <= 100; ++i) {
        bus.Step(i * 0.1);
        for (const SpokenCallout& s : bus.TakeHeard()) heard.push_back(s);
    }
    SC_EXPECT(heard.size() == 3);
    SC_EXPECT(heard[0].id == "captured" && heard[1].id == "contact" && heard[2].id == "done");
    for (std::size_t i = 1; i < heard.size(); ++i) SC_EXPECT(heard[i].start_s >= heard[i - 1].end_s - 1e-9);  // never two at once
    SC_EXPECT(heard[1].ru == "Цель, азимут 05-40, дальность 24!");
    SC_EXPECT_NEAR(SpeakingTimeS("Цель, азимут 05-40, дальность 24!"), 0.4 + 0.35 * 6, 1e-9);
    SC_EXPECT(MilsText(90.0) == "15-00" && MilsText(-6.0) == "59-00");
}

SC_TEST("callouts: in the practice shoot the crew speaks its drill: orders, acknowledgements, reports, contact, capture, result") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice_em.json", kData, def, console, &e));
    SC_EXPECT(def.has_crew_data);
    ScenarioRunner r(std::move(def), std::move(console));
    r.RunToEnd(0.1, 700.0);
    std::vector<std::string> said;
    for (const SpokenCallout& s : r.Callouts()->Log()) said.push_back(s.id);
    auto has = [&](const std::string& id) {
        const bool f = std::find(said.begin(), said.end(), id) != said.end();
        if (!f) std::printf("    no '%s'\n", id.c_str());
        return f;
    };
    SC_EXPECT(has("order_gta") && has("ack") && has("power_on") && has("soc_ready") && has("contact") && has("captured") &&
              has("target_in_zone") && has("missile_away"));
    SC_EXPECT(has("hit") || has("miss"));
    // They reach the radio log (the HUD's lines) and the debrief.
    bool on_radio = false;
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) on_radio = on_radio || m.net == "intercom";
    SC_EXPECT(on_radio);
    SC_EXPECT(r.Debrief().find("Питание подано!") != std::string::npos);
}

SC_TEST("callouts: a crewmate waiting on a player's seat reminds him, then the commander orders it by name") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice_em.json", kData, def, console));
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.TakeSeat("operator", "Player"));  // the player holds the operator's seat and does nothing
    r.RunToEnd(0.1, 300.0);
    SC_EXPECT(r.RemindersTo("operator") >= 1);
    bool asked = false, ordered = false;
    for (const SpokenCallout& s : r.Callouts()->Log()) {
        asked = asked || (s.id == "remind" && s.ru.find("Оператор") == 0);
        ordered = ordered || (s.id == "remind_order" && s.ru.find("Оператор") == 0);
    }
    SC_EXPECT(asked && ordered);
    SC_EXPECT(r.Debrief().find("waits for Operator") != std::string::npos);
}

// ------------------------------------------------------------------------------------------ 8.6 the NPC commander

namespace {
std::unique_ptr<ScenarioRunner> CrewPractice() {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenarioFile(kData + "/scenarios/osa_crew_practice.json", kData, def, console, &e)) {
        std::printf("    %s\n", e.c_str());
        return nullptr;
    }
    return std::unique_ptr<ScenarioRunner>(new ScenarioRunner(std::move(def), std::move(console)));
}
}  // namespace

SC_TEST("npc commander: with no triggers the crew comes to readiness from cold, takes the target and fires") {
    std::unique_ptr<ScenarioRunner> r = CrewPractice();
    SC_EXPECT(r != nullptr && r->NpcCommanderActive());
    r->RunToEnd(0.1, 800.0);
    const std::string d = r->Debrief();
    SC_EXPECT(r->Launches() >= 1);
    SC_EXPECT(d.find("HIT") != std::string::npos || d.find("MISS") != std::string::npos);
    for (const char* drill : {"power_up", "equipment_on", "deploy_at_halt", "acquire_with_soc", "ready_capture", "capture_with_ssc",
                              "prepare_firing", "launch"}) {
        if (d.find(std::string("started    ") + drill) == std::string::npos) std::printf("    no %s\n", drill);
        SC_EXPECT(d.find(std::string("started    ") + drill) != std::string::npos);
    }
    SC_EXPECT(d.find("Пуск разрешаю!") != std::string::npos);
}

SC_TEST("npc commander: the player in the operator's seat gets his orders; the crew waits for him, reminds him, and fires only with him") {
    std::unique_ptr<ScenarioRunner> r = CrewPractice();
    SC_EXPECT(r->TakeSeat("operator", "Player"));
    r->RunToEnd(0.1, 520.0);
    SC_EXPECT(r->Launches() == 0);  // the chain stops at the seat that never acted
    SC_EXPECT(r->RemindersTo("operator") >= 1);
    SC_EXPECT(r->Debrief().find("order for the player") != std::string::npos);
}

SC_TEST("npc commander: with the player in command the crew waits for his orders; К бою from the order menu starts every seat") {
    std::unique_ptr<ScenarioRunner> r = CrewPractice();
    SC_EXPECT(r->TakeSeat("commander", "Player"));
    SC_EXPECT(!r->NpcCommanderActive());
    for (int i = 0; i < 300; ++i) r->Step(0.1);
    SC_EXPECT(r->Debrief().find("started    power_up") == std::string::npos);  // nobody acts unordered
    SC_EXPECT(r->GiveCrewOrder("order_k_boyu"));
    SC_EXPECT(!r->GiveCrewOrder("order_no_such_thing"));
    for (int i = 0; i < 50; ++i) r->Step(0.1);
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("started    power_up") != std::string::npos && d.find("started    equipment_on") != std::string::npos &&
              d.find("started    acquire_with_soc") != std::string::npos);
    SC_EXPECT(d.find("К бою!") != std::string::npos);
}

SC_TEST("drill master: a task's order lends its drills (К бою's autonomous search at a halt, not the search on the move)") {
    Osa o;
    o.console.equipment.SetInput("vehicle_stopped", true);
    const DrillMaster dm(o.console, &o.crew);
    const DrillPlan plan = dm.Plan(dm.GoalFor(*o.crew.FindTask("ready_halted")), o.console.panel, o.console.equipment);
    const DrillPlan::Seat* so = plan.FindSeat("senior_operator");
    SC_EXPECT(so != nullptr && so->procedures == std::vector<std::string>{"acquire_with_soc"});
}

// ------------------------------------------------------------------------------------------ 8.5 knowledge

SC_TEST("knowledge: the operator puts the range on what the senior operator told him, not on the truth") {
    std::unique_ptr<ScenarioRunner> r = CrewPractice();
    const double start = r->Console().panel.GetValue("ssc.range_wheel");
    double told_s = -1.0, moved_s = -1.0;
    while (!r->Finished() && r->Time() < 460.0 && moved_s < 0.0) {
        r->Step(0.1);
        const CrewPicture* p = r->PictureOf("operator");
        const KnownTarget* k = p != nullptr ? p->Find("t1") : nullptr;
        if (told_s < 0.0 && k != nullptr) {
            told_s = r->Time();
            SC_EXPECT(k->source == "told by Senior operator");
        }
        if (std::fabs(r->Console().panel.GetValue("ssc.range_wheel") - start) > 1.0) moved_s = r->Time();
    }
    std::printf("    told at %.1f s, range wheel moved at %.1f s\n", told_s, moved_s);
    SC_EXPECT(told_s > 0.0 && moved_s >= told_s);
}

SC_TEST("knowledge: a crewmate who misses a report says Повторите and learns it from the repeat") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_crew_practice.json", kData, def, console));
    for (SkillTier& t : def.skills.tiers) {
        if (t.id == "class3") t.miss_chance = 1.0;  // the operator's tier: he never takes a report in the first time
    }
    ScenarioRunner r(std::move(def), std::move(console));
    while (!r.Finished() && r.Time() < 360.0) r.Step(0.1);
    SC_EXPECT(r.SayAgains() >= 1);
    const KnownTarget* k = r.PictureOf("operator") != nullptr ? r.PictureOf("operator")->Find("t1") : nullptr;
    SC_EXPECT(k != nullptr && k->has_range);
    bool repeated = false;
    for (const SpokenCallout& s : r.Callouts()->Log()) repeated = repeated || (s.id == "contact" && s.repeats > 0);
    SC_EXPECT(repeated);
}

SC_TEST("knowledge: with a silent player at the search scope the commander never hears of the target; his report starts the crew") {
    std::unique_ptr<ScenarioRunner> r = CrewPractice();
    SC_EXPECT(r->TakeSeat("senior_operator", "Player"));
    // The player switches his search radar on, but says nothing.
    for (const auto& s : std::vector<std::pair<std::string, int>>{{"soc.search", 1}, {"soc.hv_cover", 1}, {"soc.hv", 1}, {"soc.range", 2}}) {
        r->PlayerSet(s.first, s.second);
    }
    while (r->Time() < 420.0) r->Step(0.1);
    SC_EXPECT(!r->Engagement().SearchPicture().empty());  // the target is on his scope
    SC_EXPECT(r->PictureOf("commander") == nullptr || r->PictureOf("commander")->Find("t1") == nullptr);
    SC_EXPECT(r->Debrief().find("started    ready_capture") == std::string::npos);
    SC_EXPECT(r->PlayerSay("senior_operator", "contact"));
    for (int i = 0; i < 100; ++i) r->Step(0.1);
    SC_EXPECT(r->PictureOf("commander")->Find("t1") != nullptr);
    SC_EXPECT(r->Debrief().find("started    ready_capture") != std::string::npos);
}

SC_TEST("stress: jamming on the crew's own radars stresses the crew") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_jamming_em.json", kData, def, console));
    ScenarioRunner r(std::move(def), std::move(console));
    bool stressed = false;
    while (!r.Finished() && r.Time() < 700.0) {
        r.Step(0.1);
        stressed = stressed || r.Crew("Operator")->Stressed();
    }
    SC_EXPECT(stressed);
}

// ------------------------------------------------------------------------------------------ 8.2 calibration

namespace {
// A ready crew, all at one tier, run by its NPC commander, against a fighter that appears inside the zone: the time
// from the first echo on the search scope to the launch (the published "detection to launch").
double DetectionToLaunch(const std::string& tier, unsigned seed) {
    std::string t = R"JSON({ "scenario_id": "cal", "name": "Calibration", "source": "sam3d", "confidence": "estimate", "seed": SEED,
      "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 }, "terrain": { "type": "flat", "height_m": 110 },
      "crew_commander": "npc",
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "em": { "library": "em/osa_ak.json", "search_radar": "osa_ak_soc", "track_radar": "osa_ak_ssc" },
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 } },
      "crew": [ { "name": "Commander", "role": "commander", "skill": "TIER" }, { "name": "Deputy commander", "role": "deputy_commander", "skill": "TIER" },
                { "name": "Senior operator", "role": "senior_operator", "skill": "TIER" }, { "name": "Operator", "role": "operator", "skill": "TIER" },
                { "name": "Driver", "role": "driver", "skill": "TIER" } ],
      "targets": [ { "id": "t1", "name": "Fighter (placeholder)", "class": "fighter", "start": { "east_m": 800, "north_m": 9500, "up_m": 1000 },
                     "waypoints": [ { "east_m": 800, "north_m": -20000, "up_m": 1000 } ], "speed_mps": 150, "rcs_m2": 3, "appear_at_s": 40 } ],
      "triggers": [], "end": { "time_limit_s": 200 } })JSON";
    t = ReplaceAll(ReplaceAll(t, "TIER", tier), "SEED", std::to_string(seed));
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenario(t, kData, def, console, &e)) {
        std::printf("    %s\n", e.c_str());
        return -1.0;
    }
    ScenarioRunner r(std::move(def), std::move(console));
    double seen = -1.0;
    while (!r.Finished() && r.Time() < 200.0) {
        r.Step(0.1);
        if (seen < 0.0 && !r.Engagement().SearchPicture().empty()) seen = r.Time();
        if (r.Launches() > 0) return seen >= 0.0 ? r.Time() - seen : -1.0;
    }
    return -1.0;
}

double MeanDetectionToLaunch(const std::string& tier) {
    double sum = 0.0;
    int n = 0;
    for (unsigned seed = 1; seed <= 20; ++seed) {  // enough seeds for a mean that doesn't hang on one bad run
        const double t = DetectionToLaunch(tier, seed);
        if (t > 0.0) {
            sum += t;
            ++n;
        }
    }
    return n > 0 ? sum / n : -1.0;
}
}  // namespace

SC_TEST("calibration: detection to launch falls in the published 26-40 s for trained crews, longer for recruits") {
    const double master = MeanDetectionToLaunch("master"), c1 = MeanDetectionToLaunch("class1"), c2 = MeanDetectionToLaunch("class2"),
                 c3 = MeanDetectionToLaunch("class3"), rec = MeanDetectionToLaunch("recruit");
    std::printf("    detection to launch (mean of 20): master %.1f s, 1st class %.1f s, 2nd %.1f s, 3rd %.1f s, recruit %.1f s\n", master,
                c1, c2, c3, rec);
    SC_EXPECT(master > 0.0 && c1 > 0.0 && c2 > 0.0 && c3 > 0.0 && rec > 0.0);
    SC_EXPECT(master < c2 && c2 < rec);
    SC_EXPECT(c1 >= 24.0 && c1 <= 40.0);  // nvo_2015, korovin_tiv_2010: 26-40 s (27-39 s for the Osa-AK)
    SC_EXPECT(c3 >= 26.0 && c3 <= 42.0);
    SC_EXPECT(rec > 40.0);
}

// ------------------------------------------------------------------------------------------ 8.6 march and logistics

SC_TEST("npc commander: on the march he searches on the move and stops the vehicle at once for a hostile inside 20 km") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    const std::string text = ReplaceAll(ReadFile(kData + "/scenarios/osa_crew_practice.json"), "\"inputs\": { \"vehicle_stopped\": true }",
                                        "\"inputs\": { \"vehicle_stopped\": false }");
    SC_EXPECT(LoadScenario(text, kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    double halted_s = -1.0, halt_range_m = -1.0;
    while (!r.Finished() && r.Time() < 800.0) {
        r.Step(0.1);
        if (halted_s < 0.0 && r.Console().equipment.GetInput("vehicle_stopped")) {
            halted_s = r.Time();
            halt_range_m = r.TargetRange("t1");
        }
    }
    std::printf("    halted at %.1f s with the target at %.1f km; %d launch(es)\n", halted_s, halt_range_m / 1000.0, r.Launches());
    SC_EXPECT(halted_s > 0.0 && halt_range_m > 0.0 && halt_range_m < 20000.0);
    SC_EXPECT(r.Debrief().find("Стой!") != std::string::npos);
    SC_EXPECT(r.Debrief().find("started    deploy_at_halt") != std::string::npos);
    SC_EXPECT(r.Launches() >= 1);
}

SC_TEST("npc commander: an empty launcher is reported and the TZM asked for; the driver reports the fuel at half") {
    std::unique_ptr<ScenarioRunner> r = CrewPractice();
    for (int i = 0; i < 20; ++i) r->Step(0.1);
    for (const char* c : {"1-I", "2-I", "3-I", "1-II", "2-II", "3-II"}) r->GetLauncher()->Unload(c);
    for (int i = 0; i < 100; ++i) r->Step(0.1);
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("Ракет нет!") != std::string::npos);
    SC_EXPECT(d.find("прошу ТЗМ!") != std::string::npos);
}

// ------------------------------------------------------------------------------------------ 8.7 grading

SC_TEST("grading: the debrief grades К бою against the drill master's ideal and lists every seat's reminders and slips") {
    std::unique_ptr<ScenarioRunner> r = CrewPractice();
    SC_EXPECT(r->IdealReadyS() > 240.0 && r->IdealReadyS() < 260.0);
    r->RunToEnd(0.1, 800.0);
    SC_EXPECT(r->ActualReadyS() >= r->IdealReadyS() - 1.0);
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("К бою (ready, halted):") != std::string::npos);
    SC_EXPECT(d.find("excellent") != std::string::npos || d.find("good") != std::string::npos || d.find("satisfactory") != std::string::npos);
    SC_EXPECT(d.find("First echo to launch") != std::string::npos);
    SC_EXPECT(d.find("operator           Operator         tier class3") != std::string::npos);
}

// ------------------------------------------------------------------------------------------ the player in the crew

SC_TEST("in the crew: a player in the operator's seat who does what he's ordered, with what he's told, fights with the crew") {
    std::unique_ptr<ScenarioRunner> r = CrewPractice();
    SC_EXPECT(r->TakeSeat("operator", "Player"));
    while (!r->Finished() && r->Time() < 800.0) {
        r->Step(0.1);
        // The player works his orders step by step when each step's lamps allow ...
        const ProcedureTracker* t = r->SeatTracker("operator");
        const ProcedureStep* next = t != nullptr && !t->GetProcedure().id.empty() ? t->NextStep() : nullptr;
        if (next != nullptr && r->Console().equipment.AllHold(next->wait_for, r->Console().panel)) {
            r->SeatSet("operator", next->control_id, next->value);
        }
        // ... and turns his range wheel onto the range he was told or sees on his indicator (never the truth).
        const CrewPicture* p = r->PictureOf("operator");
        const KnownTarget* k = p != nullptr && !r->AssignedTarget().empty() ? p->Find(r->AssignedTarget()) : nullptr;
        if (k != nullptr && k->has_range && r->Engagement().Method() == TrackMethod::None) r->SeatTurn("operator", "ssc.range_wheel", k->range_m);
    }
    const std::string d = r->Debrief();
    SC_EXPECT(r->Launches() >= 1);
    SC_EXPECT(d.find("HIT") != std::string::npos || d.find("MISS") != std::string::npos);
    SC_EXPECT(d.find("operator           Player (player)") != std::string::npos);
}

// ------------------------------------------------------------------------------------------ 8.9 the battery

SC_TEST("battery: the NPC battery commander gives the PU-12's targets to the launchers on the battery net, and they fight") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_battery_fight.json", kData, def, console));
    ScenarioRunner r(std::move(def), std::move(console));
    r.RunToEnd(0.1, 900.0);
    int assigned = 0, reports = 0;
    for (const SpokenCallout& s : r.Callouts()->Log()) {
        assigned += s.id == "order_assign" ? 1 : 0;
        reports += s.id == "report_result" ? 1 : 0;
    }
    int shooter_launches = 0, shooter_hits = 0;
    for (const BatteryFire::Shooter& s : r.GetBatteryFire()->Shooters()) {
        shooter_launches += s.launches;
        shooter_hits += s.hits;
    }
    std::printf("    %d assignments, %d launches and %d hits by the other launchers, %d by ours\n", assigned, shooter_launches,
                shooter_hits, r.Launches());
    SC_EXPECT(assigned >= 2 && shooter_launches >= 2 && shooter_hits >= 1 && reports == shooter_hits);
    SC_EXPECT(r.Debrief().find("цель 620") != std::string::npos);  // targets named by their board numbers
}

SC_TEST("battery: with the player as battery commander nothing is assigned until he does it") {
    ScenarioDef def;
    ConsoleData console;
    const std::string text = ReplaceAll(ReadFile(kData + "/scenarios/osa_battery_fight.json"), "\"battery_commander\": \"npc\"",
                                        "\"battery_commander\": \"player\"");
    SC_EXPECT(LoadScenario(text, kData, def, console));
    ScenarioRunner r(std::move(def), std::move(console));
    while (r.Time() < 420.0) r.Step(0.1);
    int assigned = 0;
    for (const SpokenCallout& s : r.Callouts()->Log()) assigned += s.id == "order_assign" ? 1 : 0;
    SC_EXPECT(assigned == 0);
    SC_EXPECT(r.AssignBatteryTarget("24", "t2"));
    SC_EXPECT(!r.AssignBatteryTarget("99", "t2"));
    for (int i = 0; i < 30; ++i) r.Step(0.1);
    bool said = false;
    for (const SpokenCallout& s : r.Callouts()->Log()) said = said || (s.id == "order_assign" && s.ru.find("24, цель") == 0);
    SC_EXPECT(said);
    SC_EXPECT(r.GetBatteryFire()->Engaged("t2"));
}

// ------------------------------------------------------------------------------------------ 8.11 one mission

SC_TEST("mission: on the march, a raid: halt, deploy, fight, stow and march on, run by the NPC commanders without a fault") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_mission_march.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    r.RunToEnd(0.1, 1500.0);
    const std::string d = r.Debrief();
    for (const char* step : {"march: form the column", "started    search_on_march", "Стой!", "short halt", "started    deploy_at_halt",
                             "К маршу!", "the crew is stowed and marches on", "К маршу готов!"}) {
        if (d.find(step) == std::string::npos) std::printf("    no '%s'\n", step);
        SC_EXPECT(d.find(step) != std::string::npos);
    }
    SC_EXPECT(r.Launches() >= 1);
    SC_EXPECT(r.Log().Summary().equipment_faults == 0);
}
