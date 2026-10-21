// The helper (docs/crew/13_HELPER.md): for the seat a player holds, the move its NPC would make now. The NPC's own rules
// record what he would do as the player's duty (ScenarioRunner::NpcDoes); the helper shows it, and a player who follows
// it does what the NPC crew does.
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "simcore/helper.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
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

struct Played {
    double ready_s = -1.0;
    int launches = 0;
    int hits = 0;
    int moves = 0;
    int refusals = 0;
    std::string last_refusal;
};

// The crew practice (one target, the NPC commander) played to its end, all NPC (seat "") or with a player who follows
// the helper in that seat.
Played Play(const std::string& file, const std::string& seat, double max_s = 900.0) {
    Played p;
    std::unique_ptr<ScenarioRunner> r = Load(file);
    if (!r) return p;
    std::unique_ptr<HelpFollower> f;
    if (!seat.empty()) {
        r->TakeSeat(seat, "Player");
        r->SetHelper(true);
        f = std::make_unique<HelpFollower>(seat);
    }
    const CrewTask* ready = r->Def().crew_data.FindTask("ready_halted");
    while (!r->Finished() && r->Time() < max_s) {
        r->Step(0.1);
        if (f) f->Update(*r);
        if (p.ready_s < 0.0 && ready != nullptr && r->Console().equipment.AllHold(ready->conditions, r->Console().panel)) p.ready_s = r->Time();
    }
    p.launches = r->Launches();
    p.hits = r->Log().Summary().hits;
    if (f) {
        p.moves = f->Moves();
        p.refusals = f->Refusals();
        p.last_refusal = f->LastRefusal();
    }
    return p;
}

bool HasDuty(const ScenarioRunner& r, const std::string& seat, SeatDuty::Kind kind, const std::string& id) {
    for (const SeatDuty& d : r.OwedDuties(seat)) {
        if (d.kind == kind && d.id == id) return true;
    }
    return false;
}

std::string ReadAll(const std::string& path) {
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

}  // namespace

SC_TEST("helper: a seat a player holds owes what its NPC would do; an NPC's seat owes nothing") {
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/osa_crew_practice.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("driver", "Player");
    r->SetHelper(true);
    for (int i = 0; i < 100; ++i) r->Step(0.1);
    // The NPC commander has ordered К бою: the driver's part is the player's next move, a step of that drill.
    const std::vector<HelpStep> moves = HelpMoves(*r, "driver");
    SC_EXPECT(!moves.empty());
    if (!moves.empty()) {
        SC_EXPECT(moves.front().source == "drill");
        SC_EXPECT(moves.front().kind == HelpStep::Kind::Control);
        const ProcedureTracker* t = r->SeatTracker("driver");
        SC_EXPECT(t != nullptr && t->NextStep() != nullptr && t->NextStep()->control_id == moves.front().control);
    }
    SC_EXPECT(HelpMoves(*r, "operator").empty());
    SC_EXPECT(r->OwedDuties("operator").empty());
}

SC_TEST("helper: a player who follows it in any crew seat does what the NPC crew does") {
    const Played npc = Play("scenarios/osa_crew_practice.json", "");
    SC_EXPECT(npc.ready_s > 0.0 && npc.launches >= 1 && npc.hits >= 1);
    for (const char* seat : {"driver", "operator", "senior_operator", "deputy_commander"}) {
        const Played p = Play("scenarios/osa_crew_practice.json", seat);
        std::printf("    %s: ready %.0f s (NPC %.0f), %d launches, %d hits, %d moves\n", seat, p.ready_s, npc.ready_s, p.launches, p.hits, p.moves);
        SC_EXPECT(p.moves > 0);
        SC_EXPECT(p.ready_s > 0.0 && p.ready_s <= npc.ready_s + 10.0);
        SC_EXPECT(p.launches >= npc.launches);
        SC_EXPECT(p.hits >= npc.hits);
    }
}

SC_TEST("helper: in command, the shadow commander's orders are the player's, К бою first") {
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/osa_crew_practice.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("commander", "Player");
    r->SetHelper(true);
    for (int i = 0; i < 20; ++i) r->Step(0.1);
    SC_EXPECT(r->ShadowCommanding());
    SC_EXPECT(HasDuty(*r, "commander", SeatDuty::Kind::Order, "order_k_boyu"));
    const std::vector<HelpStep> moves = HelpMoves(*r, "commander");
    SC_EXPECT(!moves.empty() && moves.front().kind == HelpStep::Kind::Order && moves.front().callout == "order_k_boyu");
    // Nobody in the crew has been ordered anything: the shadow commander only thinks it.
    for (const CrewMember& m : r->CrewMembers()) SC_EXPECT(!m.IsBusy());
    SC_EXPECT(r->OrdersGiven().empty());
    // The player gives it: the duty is done and the crew is at work.
    SC_EXPECT(HelpDo(*r, moves.front()));
    r->Step(0.1);
    SC_EXPECT(!HasDuty(*r, "commander", SeatDuty::Kind::Order, "order_k_boyu"));
    bool busy = false;
    for (const CrewMember& m : r->CrewMembers()) busy = busy || m.IsBusy();
    SC_EXPECT(busy);
    // Followed to the end, the commander's orders take the target and fire at it as the NPC commander would.
    const Played p = Play("scenarios/osa_crew_practice.json", "commander");
    std::printf("    commander: %d launches, %d hits, %d moves\n", p.launches, p.hits, p.moves);
    SC_EXPECT(p.launches >= 1 && p.hits >= 1);
}

SC_TEST("helper: the shadow commander and the wheels' advice change nothing in the world") {
    for (const char* seat : {"commander", "operator"}) {
        std::unique_ptr<ScenarioRunner> off = Load("scenarios/osa_crew_practice.json");
        std::unique_ptr<ScenarioRunner> on = Load("scenarios/osa_crew_practice.json");
        SC_EXPECT(off != nullptr && on != nullptr);
        if (!off || !on) return;
        off->TakeSeat(seat, "Player");
        on->TakeSeat(seat, "Player");
        on->SetHelper(true);
        for (int i = 0; i < 4000; ++i) {
            off->Step(0.1);
            on->Step(0.1);
        }
        SC_EXPECT(off->Debrief() == on->Debrief());
        SC_EXPECT(!on->OwedDuties(seat).empty() || std::string(seat) == "operator");
    }
}

SC_TEST("helper: levels: off and an exam show nothing, a hint waits for the player to hesitate") {
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/osa_crew_practice.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("driver", "Player");
    r->SetHelper(true);
    for (int i = 0; i < 20; ++i) r->Step(0.1);
    r->SetAssistLevel(AssistLevel::Guided);
    SC_EXPECT(HelpFor(*r, "driver").shown);
    r->SetAssistLevel(AssistLevel::Unassisted);
    SC_EXPECT(!HelpFor(*r, "driver").shown);
    r->SetAssistLevel(AssistLevel::Exam);
    SC_EXPECT(!HelpFor(*r, "driver").shown && HelpFor(*r, "driver").steps.empty());
    r->SetAssistLevel(AssistLevel::Hint);
    const SeatHelp early = HelpFor(*r, "driver");
    SC_EXPECT(!early.steps.empty());
    SC_EXPECT(early.shown == early.hesitating);
    const double wait = r->SeatPatience("driver") - r->SeatHesitation("driver");
    for (double t = 0.0; t <= wait + 0.2; t += 0.1) r->Step(0.1);
    SC_EXPECT(HelpFor(*r, "driver").hesitating);
}

SC_TEST("helper: the player's report says and tells what his seat's NPC would have (the IFF's answer)") {
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/osa_crew_practice.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("senior_operator", "Player");
    r->SetHelper(true);
    HelpFollower f("senior_operator");
    bool told = false;
    while (!r->Finished() && r->Time() < 700.0 && !told) {
        r->Step(0.1);
        f.Update(*r);
        for (const SpokenCallout& c : r->Callouts()->Log()) told = told || (c.id == "iff_no_reply" && c.from == "senior_operator");
    }
    SC_EXPECT(told);
    // The commander learnt the target is interrogated from the player's words, as from his NPC's: he gave the launch.
    while (!r->Finished() && r->Time() < 700.0 && r->Launches() == 0) {
        r->Step(0.1);
        f.Update(*r);
    }
    SC_EXPECT(r->Launches() >= 1);
}

SC_TEST("helper: a step of a player's drill whose switch is already set passes by itself, as an NPC's does") {
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/osa_crew_practice.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("senior_operator", "Player");
    const Procedure* p = r->Console().FindProcedure("azimuth_onto_target");
    SC_EXPECT(p != nullptr && !p->steps.empty());
    if (p == nullptr) return;
    for (const ProcedureStep& s : p->steps) r->Console().panel.Set(s.control_id, s.value);
    r->SetSeatProcedure("senior_operator", "azimuth_onto_target");
    r->Step(0.1);
    // Done at once (the NPC commander may hand the seat its next drill in the same step).
    bool completed = false;
    for (const DebriefEntry& e : r->Log().Entries()) {
        completed = completed || (e.kind == DebriefEntry::Kind::Completed && e.what == "azimuth_onto_target");
    }
    SC_EXPECT(completed);
}

SC_TEST("helper: the post's seats: the battery commander deploys the post, the plotter writes what he hears") {
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/pu12m_deploy.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->TakeSeat("battery_commander", "Player");
    r->SetHelper(true);
    for (int i = 0; i < 10; ++i) r->Step(0.1);
    SC_EXPECT(HasDuty(*r, "battery_commander", SeatDuty::Kind::Order, "order_deploy"));
    std::unique_ptr<ScenarioRunner> post = Load("scenarios/pu12m_deploy.json");
    SC_EXPECT(post != nullptr);
    if (!post) return;
    post->TakeSeat("operator", "Player");
    post->SetHelper(true);
    HelpFollower f("operator");
    while (!post->Finished() && post->Time() < 900.0) {
        post->Step(0.1);
        f.Update(*post);
    }
    std::printf("    the plotter: %d plots, %d moves\n", post->PlotsMade(), f.Moves());
    SC_EXPECT(post->PlotsMade() > 0);
}

SC_TEST("helper: every NPC act at a seat goes through NpcDoes, so the helper knows it") {
    // A tripwire for the rule (docs/crew/13_HELPER.md, CLAUDE.md): NpcAt(seat) is only for choosing who speaks and how fast.
    // An NPC act behind a bare NpcAt check is invisible to the helper: a player in that seat is never told to do it.
    // If this count changes, look at the new or removed NpcAt use: an act goes through NpcDoes(seat, SeatDuty::...)
    // instead; a who-speaks choice may stay, and then the count here is raised with a word on what it is.
    const std::string src = ReadAll(kData + "/../simcore/include/simcore/scenario_runner.hpp");
    SC_EXPECT(!src.empty());
    int uses = 0;
    for (std::size_t at = src.find("NpcAt("); at != std::string::npos; at = src.find("NpcAt(", at + 1)) ++uses;
    // 21: the definition, NpcDoes itself, and the who-speaks, who-reads and how-fast choices; the gates that pass a
    // player's seat on to NpcDoes (the periodic contact, the battery's distribution, a ringing line); and the acts with no
    // move in a player's menu yet (docs/crew/13_HELPER.md, "Not yet").
    if (uses != 21) std::printf("    NpcAt( appears %d times in scenario_runner.hpp, not 21: see the comment above\n", uses);
    SC_EXPECT(uses == 21);
}
