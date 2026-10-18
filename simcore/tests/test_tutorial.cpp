// The tutorial guide (simcore/tutorial.hpp, data/tutorials/first_shoot.json): it loads, everything it names is in its
// scenario, and a player who does what each step says is taken through the whole engagement to the last step.
#include <cstdio>
#include <memory>
#include <string>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/tutorial.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::unique_ptr<ScenarioRunner> Load(const std::string& file) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenarioFile(kData + "/" + file, kData, def, console, &e)) {
        std::printf("    %s\n", e.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

bool Refused(const std::string& condition, const std::string& expect_in_error) {
    const std::string text = "{\"tutorial_id\": \"t\", \"title\": \"T\", \"scenario\": \"s.json\", \"steps\": ["
                             "{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"done_when\": [\"" + condition + "\"]}]}";
    TutorialDef def;
    std::string e;
    const bool ok = LoadTutorialJson(text, def, &e);
    if (ok || e.find(expect_in_error) == std::string::npos) std::printf("    error: %s\n", e.c_str());
    return !ok && e.find(expect_in_error) != std::string::npos;
}

}  // namespace

SC_TEST("tutorial: the first shoot's guide loads, and every seat, task, order and input it names is in its scenario") {
    TutorialDef def;
    std::string e;
    SC_EXPECT(LoadTutorialFile(kData, "tutorials/first_shoot.json", def, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(def.id == "first_shoot" && def.scenario == "scenarios/tutorial_first_shoot.json");
    SC_EXPECT(def.steps.size() >= 10);
    for (const TutorialStep& s : def.steps) SC_EXPECT(!s.title.empty() && !s.lines.empty());
    SC_EXPECT(def.steps.back().done_when.empty());  // the last step stays
    std::unique_ptr<ScenarioRunner> r = Load(def.scenario);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(CheckTutorialAgainst(def, *r, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    // Nobody commands the tutorial's crew but the player.
    SC_EXPECT(!r->Def().npc_commander);
}

SC_TEST("tutorial: conditions are checked as the guide loads; a wrong one is refused with the reason") {
    SC_EXPECT(Refused("view:F13", "F1 to F12"));
    SC_EXPECT(Refused("views:many", "a number"));
    SC_EXPECT(Refused("cabin:yes", "takes no value"));
    SC_EXPECT(Refused("debrief:BOOM", "LAUNCH, HIT, MISS"));
    SC_EXPECT(Refused("ui:dance", "unknown front-end event"));
    SC_EXPECT(Refused("task:", "needs a value"));
    SC_EXPECT(Refused("smell:smoke", "unknown condition"));
    // Only the last step may be without conditions.
    TutorialDef def;
    std::string e;
    SC_EXPECT(!LoadTutorialJson("{\"tutorial_id\": \"t\", \"title\": \"T\", \"scenario\": \"s.json\", \"steps\": ["
                                "{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"]}, {\"id\": \"b\", \"title\": \"B\", \"lines\": [\"y\"]}]}",
                                def, &e));
    SC_EXPECT(e.find("only the last step") != std::string::npos);
    // A seat the scenario's console doesn't have is caught against the scenario.
    SC_EXPECT(LoadTutorialJson("{\"tutorial_id\": \"t\", \"title\": \"T\", \"scenario\": \"s.json\", \"steps\": ["
                               "{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"done_when\": [\"seat:gunner\"]}]}",
                               def, &e));
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/tutorial_first_shoot.json");
    SC_EXPECT(r != nullptr && !CheckTutorialAgainst(def, *r, &e) && e.find("no such seat") != std::string::npos);
}

SC_TEST("tutorial: the guide waits for its step, Skip moves on, and the last step stays") {
    TutorialDef def;
    SC_EXPECT(LoadTutorialJson("{\"tutorial_id\": \"t\", \"title\": \"T\", \"scenario\": \"s.json\", \"steps\": ["
                               "{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"done_when\": [\"view:F6\", \"after:10\"]},"
                               "{\"id\": \"b\", \"title\": \"B\", \"lines\": [\"y\"], \"done_when\": [\"ui:keys_card\"]},"
                               "{\"id\": \"c\", \"title\": \"C\", \"lines\": [\"z\"]}]}",
                               def));
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/tutorial_first_shoot.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    TutorialGuide g(def);
    TutorialUi ui;
    ui.view = "F2";
    SC_EXPECT(!g.Update(*r, ui) && g.Current()->id == "a");
    ui.real_time_s = 9.0;
    SC_EXPECT(!g.Update(*r, ui));
    ui.view = "F6";  // either condition finishes it
    SC_EXPECT(g.Update(*r, ui) && g.Current()->id == "b");
    SC_EXPECT(!g.Update(*r, ui));
    g.Skip(r->Time(), ui.real_time_s);
    SC_EXPECT(g.Current()->id == "c" && g.AtEnd());
    ui.events.insert("keys_card");
    SC_EXPECT(!g.Update(*r, ui) && g.Current()->id == "c");
    SC_EXPECT(g.FinishedAt().size() == 2);
}

SC_TEST("tutorial: a player who does what each step says goes from the views to the shot and the last step") {
    TutorialDef def;
    std::string e;
    SC_EXPECT(LoadTutorialFile(kData, "tutorials/first_shoot.json", def, &e));
    std::unique_ptr<ScenarioRunner> r = Load(def.scenario);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    TutorialGuide g(def);
    TutorialUi ui;
    ui.view = "F2";
    ui.views_used.insert("F2");
    std::string log;
    double next_order_s = 0.0;
    auto order = [&](const std::string& id) {
        if (r->Time() < next_order_s || !r->CheckOrder(id).can) return;
        SC_EXPECT(r->TryCrewOrder(id));
        next_order_s = r->Time() + 2.0;  // a player reads the menu, then picks the line
    };
    for (int i = 0; i < 18000 && !g.AtEnd() && !r->Finished(); ++i) {
        r->Step(0.1);
        ui.real_time_s += 0.1;
        const std::string step = g.Current()->id;
        if (step == "welcome") ui.events.insert("orbit");
        if (step == "views") {
            for (const char* v : {"F4", "F8", "F10"}) ui.views_used.insert(v);
        }
        if (step == "cabin") {
            ui.view = "F1";
            ui.views_used.insert("F1");
            ui.in_cabin = true;
            ui.seat = "commander";
        }
        if (step == "take_seat" && r->TakeSeat("commander", "Player")) ui.holding = "commander";
        if (step == "k_boyu") order("order_k_boyu");
        if (step == "readiness") ui.events.insert("time_faster");
        if (step == "search") order("order_take_target");
        if (step == "prepare") order("order_prepare");
        if (step == "permit") order("fire_permitted");
        if (g.Update(*r, ui)) {
            char line[96];
            std::snprintf(line, sizeof(line), " %s %.0fs", step.c_str(), r->Time());
            log += line;
        }
    }
    std::printf("    steps done at:%s\n", log.c_str());
    SC_EXPECT(g.AtEnd() && g.Current()->id == "after");
    SC_EXPECT(r->Launches() >= 1);
    SC_EXPECT(r->SeatHolder("commander") == "Player");
    // The order of the fight: ready before the capture, the capture before the launch.
    SC_EXPECT(g.FinishedAt().size() == def.steps.size() - 1);
}
