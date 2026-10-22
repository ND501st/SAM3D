// The tutorial lessons (simcore/tutorial.hpp, data/tutorials/*.json): they load, everything they name is in their
// scenario, and a student who does exactly what each step asks, at a human pace (a click a second, the wheels turned by
// hand), is taken through every lesson to its last step: no step stalls, nothing he is asked to do is refused, and he
// never deviates from the drill.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#include "simcore/cabin.hpp"
#include "simcore/missions.hpp"
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

bool StepRefused(const std::string& step, const std::string& expect_in_error) {
    const std::string text = "{\"tutorial_id\": \"t\", \"title\": \"T\", \"scenario\": \"s.json\", \"steps\": [" + step + "]}";
    TutorialDef def;
    std::string e;
    const bool ok = LoadTutorialJson(text, def, &e);
    if (ok || e.find(expect_in_error) == std::string::npos) std::printf("    error: %s\n", e.c_str());
    return !ok && e.find(expect_in_error) != std::string::npos;
}

// What a student playing a lesson ran into.
struct LessonRun {
    bool loaded = false;
    bool finished = false;      // reached the last step
    std::size_t step = 0;       // where he got to
    std::string stuck_on;       // the step and what it was waiting for, if he didn't finish
    int deviations = 0;         // his moves off the drill (the debrief's DEVIATION lines under his name)
    int refusals = 0;           // moves the lesson asked for and then refused
    int launches = 0;
    int designations = 0;
    int decoded = 0;            // talk-table groups a crewman decoded aloud (none, where the learner decodes them himself)
    double sim_s = 0.0;
    double real_s = 0.0;
    std::string timeline;
};

// The student: does what the step asks, a move every so often like a person (a click about once a second, the wheels at
// a hand's speed), and nothing else. Front-end conditions (views, the cabin, the keys) he satisfies as asked; he drives
// when a step wants the vehicle moved, and stops when it wants it halted.
LessonRun PlayLesson(const std::string& file, double limit_s = 3000.0) {
    LessonRun run;
    TutorialDef def;
    std::string e;
    if (!LoadTutorialFile(kData, file, def, &e)) {
        std::printf("    %s\n", e.c_str());
        return run;
    }
    std::unique_ptr<ScenarioRunner> r = Load(def.scenario);
    if (!r) return run;
    TutorialGuide g(def);
    if (!g.Bind(*r, &e)) {
        std::printf("    %s\n", e.c_str());
        return run;
    }
    run.loaded = true;
    TutorialUi ui;
    ui.view = "F2";
    ui.views_used.insert("F2");
    if (!def.seat.empty()) {
        r->TakeSeat(def.seat, "Player");  // the missions menu starts him in the lesson's seat
        ui.view = "F1";
        ui.views_used.insert("F1");
        ui.in_cabin = true;
        ui.seat = def.seat;
        ui.holding = def.seat;
    }
    // The PU-12M's lessons: a half-second step (the post's deployment takes some 17 minutes, nothing in it is fast).
    const double dt = r->Console().system_id == "pu12m" ? 0.5 : 0.1;
    double next_move_s = 0.0;
    Vec3 at = r->UnitPosition();
    bool driving = false;
    double next_replot_s = 0.0;
    for (int i = 0; i < static_cast<int>(limit_s / dt) && !g.AtEnd(); ++i) {
        r->Step(dt);
        ui.real_time_s += dt;
        if (driving) {
            at.y += 8.0 * dt;  // 8 m/s north
            r->SetUnitPosition(at);
        }
        const std::size_t before = g.Index();
        if (g.Update(*r, ui)) {
            char line[96];
            std::snprintf(line, sizeof(line), " %s@%.0f", g.Def().steps[before].id.c_str(), r->Time());
            run.timeline += line;
            next_move_s = r->Time() + 1.0;  // he reads the new step first
        }
        if (g.AtEnd()) break;
        const TutorialStep* s = g.Current();
        const TutorialStatus st = g.Status(*r, ui);
        // The front end's conditions: he does what they ask.
        for (const TutorialCondition& c : s->done_when) {
            switch (c.kind) {
                case TutorialCondition::Kind::View:
                    ui.view = c.arg;
                    ui.views_used.insert(c.arg);
                    ui.in_cabin = c.arg == "F1";
                    break;
                case TutorialCondition::Kind::Views:
                    for (const char* v : {"F2", "F4", "F8", "F10", "F11"}) {
                        if (static_cast<double>(ui.views_used.size()) < c.number) ui.views_used.insert(v);
                    }
                    break;
                case TutorialCondition::Kind::Cabin: ui.in_cabin = true; break;
                case TutorialCondition::Kind::Seat:
                    ui.in_cabin = true;
                    ui.seat = c.arg;
                    break;
                case TutorialCondition::Kind::Holding:
                    if (r->TakeSeat(c.arg, "Player")) ui.holding = c.arg;
                    break;
                case TutorialCondition::Kind::Ui: ui.events.insert(c.arg); break;
                case TutorialCondition::Kind::Plotted:
                    // The plotter: a report he heard clicked on the board where it said, a try a second (each takes
                    // him a while).
                    if (r->Time() >= next_move_s) {
                        if (r->Post() != nullptr && !r->Post()->Heard().empty()) {
                            const CommandPost::AirReport& h = r->Post()->Heard().front();
                            r->PlayerPlotAt(h.number, h.contact.position.x, h.contact.position.y, 200000.0 / 0.17);
                        }
                        next_move_s = r->Time() + 1.0;
                    }
                    break;
                case TutorialCondition::Kind::Designated:
                    // At the plotter's seat, waiting for the commander: he keeps the board fresh, each report as it
                    // comes (PlayerPlotAt refuses at any other seat).
                    if (r->Time() >= next_replot_s) {
                        if (r->Post() != nullptr && !r->Post()->Heard().empty()) {
                            const CommandPost::AirReport& h = r->Post()->Heard().front();
                            r->PlayerPlotAt(h.number, h.contact.position.x, h.contact.position.y, 200000.0 / 0.17);
                        }
                        next_replot_s = r->Time() + 1.0;
                    }
                    break;
                case TutorialCondition::Kind::Moved:
                    if (!driving) {
                        driving = true;
                        r->Console().equipment.SetInput("vehicle_stopped", false);
                    }
                    break;
                case TutorialCondition::Kind::Input:
                    if (c.arg == "vehicle_stopped" && driving) {
                        driving = false;
                        r->Console().equipment.SetInput("vehicle_stopped", true);
                    }
                    // The radiotelephonist: the page before his eyes, the report taken down as he heard it.
                    if (c.arg == "journal_written_by_hand" && r->Time() >= next_move_s && r->JournalBegin(def.seat)) {
                        const JournalDraft d = *r->Journal().Draft();
                        for (const std::string& group : d.said) {
                            for (char digit : group) r->JournalType(def.seat, digit);
                        }
                        r->JournalFinish(def.seat, false);
                        next_move_s = r->Time() + 1.0;
                    }
                    break;
                default:
                    break;
            }
        }
        if (st.phase == TutorialStatus::Phase::Act && st.wheel) {
            // A wheel by hand: at most 15 degrees or 1.5 km a second, onto the mark.
            const double err = st.wraps ? std::remainder(st.target - st.value, 360.0) : st.target - st.value;
            const double rate = st.wraps ? 15.0 : 1500.0;
            const double move = std::fabs(err) <= rate * dt ? err : (err > 0.0 ? rate * dt : -rate * dt);
            if (!g.Refusal(*r, st.control, ui).empty()) ++run.refusals;
            const std::string seat = def.seat.empty() ? st.station : def.seat;
            if (r->SeatHolder(seat).empty()) {
                r->Console().panel.SetValue(st.control, st.value + move);  // a seat nobody holds: straight on the panel
            } else {
                r->SeatTurn(seat, st.control, st.value + move);
            }
            continue;
        }
        if (r->Time() < next_move_s) continue;
        switch (st.phase) {
            case TutorialStatus::Phase::Read:
                ++ui.continues;
                next_move_s = r->Time() + 1.0;
                break;
            case TutorialStatus::Phase::Act:
                if (!st.control.empty()) {
                    if (!g.Refusal(*r, st.control, ui).empty()) ++run.refusals;
                    const ControlDef* d = r->Console().panel.Find(st.control);
                    const int now = r->Console().panel.Get(st.control);
                    // A click: a toggle flips, a selector or key steps one position, a button presses.
                    const int to = d->kind == ControlKind::Momentary ? 1 : CabinClickTarget(*d, now, st.position > now ? 1 : -1);
                    const std::string seat = def.seat.empty() ? d->station : def.seat;
                    if (r->SeatHolder(seat).empty()) {
                        r->PlayerSet(st.control, to);
                    } else {
                        r->SeatSet(seat, st.control, to);
                    }
                } else if (!st.order.empty() || !st.decode.empty()) {
                    // A group to decode: the card doesn't name the answer; he reads the group against the table himself.
                    const std::string want = !st.order.empty() ? st.order : (s->order.empty() ? s->report : s->order);
                    if (!g.MenuRefusal(*r, want, ui).empty()) ++run.refusals;
                    if (st.report) {
                        if (!r->PlayerSay(def.seat, want)) std::printf("    can't say %s at %.0f s\n", want.c_str(), r->Time());
                    } else {
                        ScenarioRunner::OrderCheck why;
                        if (!r->TryCrewOrder(want, &why)) {
                            std::printf("    order %s refused at %.0f s: %s\n", want.c_str(), r->Time(), why.reason_en.c_str());
                        }
                    }
                }
                next_move_s = r->Time() + 1.0;
                break;
            default:
                break;
        }
    }
    run.finished = g.AtEnd();
    run.step = g.Index();
    if (!run.finished && g.Current() != nullptr) {
        const TutorialStatus st = g.Status(*r, ui);
        run.stuck_on = g.Current()->id + " (" + st.wait_text + ")";
        for (const ScenarioRunner::SeatAction& a : r->SeatActions()) {
            if (!a.wheel) std::printf("    his move at %.1f s: %s to %d (%s)\n", a.time_s, a.control.c_str(), a.position, a.moved ? "moved" : "refused");
        }
    }
    for (const DebriefEntry& d : r->Log().Entries()) {
        if (d.kind == DebriefEntry::Kind::Deviation && d.who == "Player") {
            ++run.deviations;
            std::printf("    deviation: %s %s\n", d.what.c_str(), d.detail.c_str());
        }
    }
    run.launches = r->Launches();
    run.designations = r->DesignationsMade();
    if (r->Callouts() != nullptr) {
        for (const SpokenCallout& c : r->Callouts()->Log()) run.decoded += c.id == "decoded" ? 1 : 0;
    }
    run.sim_s = r->Time();
    run.real_s = ui.real_time_s;
    return run;
}

void ExpectFinished(const LessonRun& run, const char* name) {
    std::printf("    %s: %s at %.0f s sim%s%s\n", name, run.finished ? "finished" : "STUCK", run.sim_s,
                run.stuck_on.empty() ? "" : " on ", run.stuck_on.c_str());
    std::printf("    steps:%s\n", run.timeline.c_str());
    SC_EXPECT(run.loaded);
    SC_EXPECT(run.finished);
    SC_EXPECT(run.deviations == 0);
    SC_EXPECT(run.refusals == 0);
}

}  // namespace

SC_TEST("tutorial: every lesson loads, and every seat, control, procedure, order, report and target it names is in its scenario") {
    MissionCatalog c;
    std::string e;
    SC_EXPECT(LoadMissionCatalog(kData, c, &e));  // binds each mission's lesson to its scenario
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    int lessons = 0;
    for (const MissionGroup& gr : c.groups) {
        for (const MissionDef& m : gr.missions) {
            if (m.tutorial.empty()) continue;
            ++lessons;
            TutorialDef def;
            SC_EXPECT(LoadTutorialFile(kData, m.tutorial, def, &e));
            SC_EXPECT(def.scenario == m.file);
            // A seat lesson starts the student in its seat; every step says something.
            if (!def.seat.empty()) SC_EXPECT(m.start == def.seat);
            for (const TutorialStep& s : def.steps) SC_EXPECT(!s.title.empty() && !s.lines.empty());
            SC_EXPECT(!def.steps.back().HasAction() && def.steps.back().done_when.empty());  // the last step stays
        }
    }
    SC_EXPECT(lessons == 11);  // the basics, the Osa's five seats and the PU-12M's five
}

SC_TEST("tutorial: every highlight a lesson shows is a panel, display, lamp or control of the cabin") {
    for (const char* file : {"tutorials/basics.json", "tutorials/school_driver.json", "tutorials/school_operator.json",
                             "tutorials/school_senior_operator.json", "tutorials/school_deputy_commander.json",
                             "tutorials/school_commander.json", "tutorials/school_pu12m_driver.json",
                             "tutorials/school_pu12m_radio_operator.json", "tutorials/school_pu12m_squad_leader.json",
                             "tutorials/school_pu12m_operator.json", "tutorials/school_pu12m_battery_commander.json"}) {
        TutorialDef def;
        std::string e;
        SC_EXPECT(LoadTutorialFile(kData, file, def, &e));
        std::unique_ptr<ScenarioRunner> r = Load(def.scenario);
        SC_EXPECT(r != nullptr);
        if (!r) continue;
        CabinLayout cabin;
        SC_EXPECT(LoadCabinFile(kData + "/cabins/" + r->Console().system_id + ".json", r->Console(), cabin, &e));
        for (const TutorialStep& s : def.steps) {
            if (s.look.empty()) continue;
            bool known = r->Console().panel.Find(s.look) != nullptr;
            for (const CabinPanel& p : cabin.panels) known = known || p.id == s.look;
            for (const CabinDisplay& d : cabin.displays) known = known || d.id == s.look;
            for (const CabinLamp& l : cabin.lamps) known = known || l.id == s.look;
            if (!known) std::printf("    %s, step %s: no '%s' in the cabin\n", file, s.id.c_str(), s.look.c_str());
            SC_EXPECT(known);
        }
    }
}

SC_TEST("tutorial: conditions and steps are checked as a lesson loads; a wrong one is refused with the reason") {
    SC_EXPECT(Refused("view:F13", "F1 to F12"));
    SC_EXPECT(Refused("views:many", "a number"));
    SC_EXPECT(Refused("cabin:yes", "takes no value"));
    SC_EXPECT(Refused("debrief:BOOM", "LAUNCH, HIT, MISS"));
    SC_EXPECT(Refused("ui:dance", "unknown front-end event"));
    SC_EXPECT(Refused("marked:yes", "takes no value"));
    SC_EXPECT(Refused("task:", "needs a value"));
    SC_EXPECT(Refused("subsystem:power", "subsystem:<id>=<value>"));
    SC_EXPECT(Refused("subsystem:power=hot", "a state is ready"));
    SC_EXPECT(Refused("control:soc.hv=on", "a number"));
    SC_EXPECT(Refused("smell:smoke", "unknown condition"));
    SC_EXPECT(Refused("given_target:21", "takes no value"));
    SC_EXPECT(Refused("group:", "needs a value"));
    SC_EXPECT(StepRefused("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"continue\": true, \"decode\": \"707\"}", "for an order or report step"));
    SC_EXPECT(StepRefused("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"control\": \"soc.hv\", \"position\": 1, \"continue\": true}",
                          "one action a step"));
    SC_EXPECT(StepRefused("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"control\": \"soc.hv\"}", "needs the position"));
    SC_EXPECT(StepRefused("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"wheel\": \"ssc.az_wheel\"}", "needs it"));
    SC_EXPECT(StepRefused("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"wheel\": \"ssc.az_wheel\", \"aim\": \"contact_azimuth\", \"hold\": true}",
                          "needs done_when"));
    SC_EXPECT(StepRefused("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"]}, {\"id\": \"b\", \"title\": \"B\", \"lines\": [\"y\"]}",
                          "only the last step"));
    // Against the scenario: a control of another seat in a strict lesson, a move its procedure hasn't got, a target not
    // held on cue.
    auto bind_fails = [](const std::string& steps, const std::string& expect) {
        TutorialDef def;
        std::string e;
        if (!LoadTutorialJson("{\"tutorial_id\": \"t\", \"title\": \"T\", \"scenario\": \"s.json\", \"seat\": \"operator\", "
                              "\"strict\": true, \"steps\": [" + steps + "]}", def, &e)) {
            std::printf("    load: %s\n", e.c_str());
            return false;
        }
        std::unique_ptr<ScenarioRunner> r = Load("scenarios/school_operator.json");
        TutorialGuide g(def);
        const bool bound = r != nullptr && g.Bind(*r, &e);
        if (bound || e.find(expect) == std::string::npos) std::printf("    error: %s\n", e.c_str());
        return !bound && e.find(expect) != std::string::npos;
    };
    SC_EXPECT(bind_fails("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"control\": \"soc.hv\", \"position\": 1}, "
                         "{\"id\": \"z\", \"title\": \"Z\", \"lines\": [\"y\"]}", "is the senior_operator's"));
    SC_EXPECT(bind_fails("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"control\": \"ssc.tx\", \"position\": 1, \"procedure\": \"equipment_on\"}, "
                         "{\"id\": \"z\", \"title\": \"Z\", \"lines\": [\"y\"]}", "has no step ssc.tx"));
    SC_EXPECT(bind_fails("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"continue\": true, \"on_start\": [\"release:zz\"]}, "
                         "{\"id\": \"z\", \"title\": \"Z\", \"lines\": [\"y\"]}", "held on cue"));
    SC_EXPECT(bind_fails("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"report\": \"contact\"}, "
                         "{\"id\": \"z\", \"title\": \"Z\", \"lines\": [\"y\"]}", "senior_operator's report"));
    // The voice paths: a regiment's command the scenario doesn't hold on cue, a group not in its talk tables, a target
    // given on a battery net it hasn't got.
    SC_EXPECT(bind_fails("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"continue\": true, \"on_start\": [\"regiment:707\"]}, "
                         "{\"id\": \"z\", \"title\": \"Z\", \"lines\": [\"y\"]}", "no regiment's command '707'"));
    SC_EXPECT(bind_fails("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"done_when\": [\"group:707\"]}, "
                         "{\"id\": \"z\", \"title\": \"Z\", \"lines\": [\"y\"]}", "no such group"));
    SC_EXPECT(bind_fails("{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"done_when\": [\"given_target\"]}, "
                         "{\"id\": \"z\", \"title\": \"Z\", \"lines\": [\"y\"]}", "no battery net"));
}

SC_TEST("tutorial: a group the learner decodes himself: nothing names the answer, another order is refused with the group; named after its time") {
    // The deployed battery (pu12m_battery.json) with the regiment's ПТ-1 707 (report readiness) held on cue.
    std::ifstream f(kData + "/scenarios/pu12m_battery.json", std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    const std::string from = "\"commands\": [ { \"at_s\": 5, \"code\": 10 } ]";
    SC_EXPECT(text.find(from) != std::string::npos);
    text.replace(text.find(from), from.size(), "\"commands\": [ { \"at_s\": 5, \"code\": 10 }, { \"group\": \"707\", \"on_cue\": true } ]");
    ScenarioDef sdef;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(text, kData, sdef, console, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    ScenarioRunner r(std::move(sdef), std::move(console));
    TutorialDef def;
    SC_EXPECT(LoadTutorialJson("{\"tutorial_id\": \"t\", \"title\": \"T\", \"scenario\": \"s.json\", \"seat\": \"battery_commander\", \"strict\": true, \"steps\": ["
                               "{\"id\": \"wait\", \"title\": \"W\", \"lines\": [\"x\"], \"done_when\": [\"time:240\"]},"
                               "{\"id\": \"call\", \"title\": \"C\", \"lines\": [\"x\"], \"on_start\": [\"regiment:707\"], \"done_when\": [\"group:707\"]},"
                               "{\"id\": \"decode\", \"title\": \"D\", \"lines\": [\"x\"], \"order\": \"order_report_regiment\", \"decode\": \"707\"},"
                               "{\"id\": \"end\", \"title\": \"E\", \"lines\": [\"x\"]}]}", def, &e));
    TutorialGuide g(def);
    SC_EXPECT(g.Bind(r, &e));
    SC_EXPECT(r.TakeSeat("battery_commander", "Player"));
    TutorialUi ui;
    ui.in_cabin = true;
    ui.seat = ui.holding = "battery_commander";
    for (int i = 0; i < 4000 && g.Index() < 2; ++i) {
        r.Step(0.1);
        ui.real_time_s += 0.1;
        g.Update(r, ui);
    }
    SC_EXPECT(g.Index() == 2);  // the group heard in his second receiver (the post deployed: the RTK-1 monitors РСТ-3)
    bool decoded_for_him = false;
    for (const SpokenCallout& s : r.Callouts()->Log()) decoded_for_him = decoded_for_him || s.id == "decoded";
    SC_EXPECT(!decoded_for_him);  // nobody else in the post is on the regiment's net
    g.Update(r, ui);
    TutorialStatus st = g.Status(r, ui);
    SC_EXPECT(st.phase == TutorialStatus::Phase::Act && st.decode == "707" && st.order.empty() && st.order_ru.empty());
    const std::string no = g.MenuRefusal(r, "order_battery_spare", ui);
    SC_EXPECT(no.find("707") != std::string::npos && no.find("ПТ-1") != std::string::npos && no.find("Доложить") == std::string::npos);
    SC_EXPECT(g.MenuRefusal(r, "order_report_regiment", ui).empty());
    ui.real_time_s += TutorialGuide::kDecodeRevealS + 1.0;  // he hasn't found it: the card names it
    st = g.Status(r, ui);
    SC_EXPECT(st.order == "order_report_regiment" && !st.order_ru.empty() && st.decode.empty());
    SC_EXPECT(r.TryCrewOrder("order_report_regiment"));
    g.Update(r, ui);
    SC_EXPECT(g.AtEnd());
}

SC_TEST("tutorial: a strict lesson works only the step's control, after its wait; a button counts when pressed; the guide stays on the last step") {
    TutorialDef def;
    std::string e;
    SC_EXPECT(LoadTutorialFile(kData, "tutorials/school_operator.json", def, &e));
    std::unique_ptr<ScenarioRunner> r = Load(def.scenario);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    TutorialGuide g(def);
    SC_EXPECT(g.Bind(*r, &e));
    SC_EXPECT(r->TakeSeat("operator", "Player"));
    TutorialUi ui;
    ui.in_cabin = true;
    ui.seat = ui.holding = "operator";
    g.Update(*r, ui);
    // A reading step: nothing works, Enter moves on.
    SC_EXPECT(g.Status(*r, ui).phase == TutorialStatus::Phase::Read);
    SC_EXPECT(!g.Refusal(*r, "pwr.equipment", ui).empty());
    for (int i = 0; i < 2000 && g.Current()->id != "equipment"; ++i) {
        // The headset on, and «Третий готов!» on the intercom: the steps before the drill.
        if (g.Current()->id == "headset" && r->Console().panel.Get("headset.operator") == 0) r->SeatSet("operator", "headset.operator", 1);
        if (g.Current()->id == "seat_ready") r->PlayerSay("operator", "seat_ready");
        ++ui.continues;
        r->Step(0.1);
        g.Update(*r, ui);
    }
    SC_EXPECT(g.Current()->id == "equipment");
    // The equipment switch waits for the turbine's power: it says so, and refuses until then.
    TutorialStatus st = g.Status(*r, ui);
    SC_EXPECT(st.phase == TutorialStatus::Phase::Wait && st.control == "pwr.equipment" && st.position == 1 && st.fast_forward);
    SC_EXPECT(st.position_label == "ВКЛ." && st.control_label == "АППАРАТУРА ВКЛ. — ОТКЛ.");
    SC_EXPECT(g.Refusal(*r, "pwr.equipment", ui).find("Not yet") == 0);
    SC_EXPECT(g.Refusal(*r, "stab.power", ui).find("Not that one") == 0);
    for (int i = 0; i < 1200 && g.Status(*r, ui).phase == TutorialStatus::Phase::Wait; ++i) {
        r->Step(0.1);
        g.Update(*r, ui);
    }
    SC_EXPECT(g.Status(*r, ui).phase == TutorialStatus::Phase::Act);
    SC_EXPECT(g.Refusal(*r, "pwr.equipment", ui).empty());
    SC_EXPECT(r->SeatSet("operator", "pwr.equipment", 1) == StepResult::Accepted);
    r->Step(0.1);
    SC_EXPECT(g.Update(*r, ui) && g.Current()->id == "stabilisation");
    // A move off the step (possible in a free lesson) is answered in words.
    r->SeatSet("operator", "tov.supply", 1);
    g.Update(*r, ui);
    SC_EXPECT(g.Status(*r, ui).feedback.find("ТОВ") != std::string::npos);
}

SC_TEST("tutorial: a step the learner finishes himself is his move; one the crew or the world finishes he watches") {
    TutorialDef def;
    std::string e;
    SC_EXPECT(LoadTutorialFile(kData, "tutorials/school_driver.json", def, &e));
    std::unique_ptr<ScenarioRunner> r = Load(def.scenario);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    TutorialGuide g(def);
    SC_EXPECT(g.Bind(*r, &e));
    SC_EXPECT(r->TakeSeat("driver", "Player"));
    TutorialUi ui;
    ui.in_cabin = true;
    ui.seat = ui.holding = "driver";
    g.Update(*r, ui);
    auto PhaseOf = [&](const std::string& title) {
        while (g.Current() != nullptr && g.Current()->title != title) g.Pass(*r, ui);
        return g.Current() != nullptr ? g.Status(*r, ui).phase : TutorialStatus::Phase::None;
    };
    SC_EXPECT(PhaseOf("Drive forward 40 m") == TutorialStatus::Phase::Act);       // moved: his keys
    SC_EXPECT(PhaseOf("Stop") == TutorialStatus::Phase::Act);                     // the halt: his brake
    SC_EXPECT(PhaseOf("The turbine spins up") == TutorialStatus::Phase::Watch);   // the turbine's own time
    SC_EXPECT(PhaseOf("Missile away") == TutorialStatus::Phase::Watch);           // the crew's shot
    SC_EXPECT(PhaseOf("The driver's part: done") == TutorialStatus::Phase::End);
    TutorialCondition view;
    view.kind = TutorialCondition::Kind::View;
    TutorialCondition task;
    task.kind = TutorialCondition::Kind::Task;
    SC_EXPECT(TutorialLearnerCondition(view) && !TutorialLearnerCondition(task));
}

SC_TEST("tutorial: a selector clicked through its positions toward the step's isn't a deviation; past it, it is") {
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/school_senior_operator.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("senior_operator", "Player"));
    r->SeatSet("senior_operator", "headset.senior_operator", 1);  // the lesson starts with it off: the order comes on the intercom
    r->Step(0.1);
    SC_EXPECT(r->GiveCrewOrder("order_k_boyu"));
    r->Step(0.1);
    // acquire_with_soc: ПОИСК, the guard, ВЫСОКОЕ, then ЛУЧ to АВТ. I–III (position 3) from I (0).
    SC_EXPECT(r->SeatSet("senior_operator", "soc.search", 1) == StepResult::Accepted);
    SC_EXPECT(r->SeatSet("senior_operator", "soc.hv_cover", 1) == StepResult::Accepted);
    const StepResult hv = r->SeatSet("senior_operator", "soc.hv", 1);  // early or not: the turbine is still starting
    SC_EXPECT(hv == StepResult::Accepted || hv == StepResult::AcceptedEarly);
    SC_EXPECT(r->SeatSet("senior_operator", "soc.beam", 1) == StepResult::Toward);
    SC_EXPECT(r->SeatSet("senior_operator", "soc.beam", 2) == StepResult::Toward);
    SC_EXPECT(r->SeatSet("senior_operator", "soc.beam", 3) == StepResult::Accepted);
    SC_EXPECT(r->SeatSet("senior_operator", "soc.range", 0) == StepResult::NotNextStep);  // the wrong way: judged
}

SC_TEST("tutorial: a report counts when the learner makes it, not when the busy intercom gets round to saying it") {
    TutorialDef def;
    std::string e;
    SC_EXPECT(LoadTutorialFile(kData, "tutorials/school_driver.json", def, &e));
    std::unique_ptr<ScenarioRunner> r = Load(def.scenario);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    TutorialGuide g(def);
    SC_EXPECT(g.Bind(*r, &e));
    SC_EXPECT(r->TakeSeat("driver", "Player"));
    TutorialUi ui;
    ui.in_cabin = true;
    ui.seat = ui.holding = "driver";
    g.Update(*r, ui);
    while (g.Current() != nullptr && g.Current()->id != "report_halt") g.Pass(*r, ui);
    SC_EXPECT(g.Current() != nullptr);
    if (g.Current() == nullptr) return;
    // The commander is talking: the report waits for the intercom, the lesson doesn't.
    SC_EXPECT(r->GiveCrewOrder("order_k_boyu"));
    r->Step(0.1);
    SC_EXPECT(r->Callouts()->Speaking("intercom"));
    SC_EXPECT(r->PlayerSay("driver", "halted"));
    r->Step(0.1);
    SC_EXPECT(g.Update(*r, ui) && g.Current()->id != "report_halt");
    bool said = false;
    for (const SpokenCallout& c : r->Callouts()->Log()) said = said || c.id == "halted";
    SC_EXPECT(!said);
    SC_EXPECT(r->ReportsMade().size() == 1 && r->ReportsMade()[0].callout == "halted" && r->ReportsMade()[0].station == "driver");
}

SC_TEST("tutorial: back shows a step again to read; its orders aren't given twice; Enter moves on") {
    TutorialDef def;
    std::string e;
    SC_EXPECT(LoadTutorialFile(kData, "tutorials/school_operator.json", def, &e));
    std::unique_ptr<ScenarioRunner> r = Load(def.scenario);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    TutorialGuide g(def);
    SC_EXPECT(g.Bind(*r, &e));
    SC_EXPECT(r->TakeSeat("operator", "Player"));
    TutorialUi ui;
    ui.in_cabin = true;
    ui.seat = ui.holding = "operator";
    g.Update(*r, ui);
    SC_EXPECT(!g.Back(*r, ui));  // nothing before the first
    for (int i = 0; i < 2000 && g.Current() != nullptr && g.Current()->id != "equipment"; ++i) {
        if (g.Current()->id == "headset" && r->Console().panel.Get("headset.operator") == 0) r->SeatSet("operator", "headset.operator", 1);
        if (g.Current()->id == "seat_ready") r->PlayerSay("operator", "seat_ready");
        ++ui.continues;
        r->Step(0.1);
        g.Update(*r, ui);
    }
    SC_EXPECT(g.Current() != nullptr && g.Current()->id == "equipment");
    if (g.Current() == nullptr || g.Current()->id != "equipment") return;
    const std::size_t orders = r->OrdersGiven().size();
    SC_EXPECT(orders == 1);  // «К бою!», at the step before
    SC_EXPECT(g.Back(*r, ui) && g.Current()->id != "equipment");
    const TutorialStatus st = g.Status(*r, ui);
    SC_EXPECT(st.phase == TutorialStatus::Phase::Read && st.review);
    for (int i = 0; i < 20; ++i) {
        r->Step(0.1);
        g.Update(*r, ui);
    }
    SC_EXPECT(g.Current()->id != "equipment");  // stays until Enter
    ++ui.continues;
    r->Step(0.1);
    SC_EXPECT(g.Update(*r, ui) && g.Current()->id == "equipment" && !g.Status(*r, ui).review);
    SC_EXPECT(r->OrdersGiven().size() == orders);
}

SC_TEST("tutorial: a student who only presses skip is taken through every lesson by the instructor") {
    const char* lessons[] = {"tutorials/basics.json", "tutorials/school_driver.json", "tutorials/school_operator.json",
                             "tutorials/school_senior_operator.json", "tutorials/school_deputy_commander.json",
                             "tutorials/school_commander.json", "tutorials/school_pu12m_driver.json",
                             "tutorials/school_pu12m_radio_operator.json", "tutorials/school_pu12m_squad_leader.json",
                             "tutorials/school_pu12m_operator.json", "tutorials/school_pu12m_battery_commander.json"};
    for (const char* file : lessons) {
        TutorialDef def;
        std::string e;
        SC_EXPECT(LoadTutorialFile(kData, file, def, &e));
        std::unique_ptr<ScenarioRunner> r = Load(def.scenario);
        SC_EXPECT(r != nullptr);
        if (!r) continue;
        TutorialGuide g(def);
        SC_EXPECT(g.Bind(*r, &e));
        TutorialUi ui;
        if (!def.seat.empty()) {
            SC_EXPECT(r->TakeSeat(def.seat, "Player"));
            ui.in_cabin = true;
            ui.seat = ui.holding = def.seat;
        }
        double next_skip_s = 1.0;
        std::size_t skipped_at = 1000;
        const double dt = r->Console().system_id == "pu12m" ? 0.5 : 0.1;
        for (int tick = 0; tick < static_cast<int>(5000.0 / dt) && !g.AtEnd(); ++tick) {
            r->Step(dt);
            ui.real_time_s += dt;
            g.Update(*r, ui);
            if (r->Time() >= next_skip_s && g.Index() != skipped_at) {
                skipped_at = g.Index();
                g.Skip(*r, ui);
                next_skip_s = r->Time() + 1.0;
            }
        }
        std::printf("    %s: %s at %.0f s sim, %d launches\n", file, g.AtEnd() ? "finished" : "STUCK", r->Time(), r->Launches());
        if (!g.AtEnd() && g.Current() != nullptr) std::printf("      on %s (%s)\n", g.Current()->id.c_str(), g.Status(*r, ui).wait_text.c_str());
        SC_EXPECT(g.AtEnd());
        // (Skipped, the watching steps pass at once: the lesson can end before the crew has fired.)
    }
}

SC_TEST("tutorial: a held target appears only when released") {
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/school_operator.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    for (int i = 0; i < 600; ++i) r->Step(0.1);
    SC_EXPECT(r->TargetRange("d1") < 0.0);
    SC_EXPECT(r->ReleaseTarget("d1"));
    SC_EXPECT(!r->ReleaseTarget("d1"));
    r->Step(0.1);
    SC_EXPECT(r->TargetRange("d1") > 25000.0);
}

SC_TEST("tutorial: a student who does what each step asks finishes the basics") {
    ExpectFinished(PlayLesson("tutorials/basics.json"), "basics");
}

SC_TEST("tutorial: a student who does what each step asks finishes the driver's lesson") {
    ExpectFinished(PlayLesson("tutorials/school_driver.json"), "driver");
}

SC_TEST("tutorial: a student who does what each step asks finishes the operator's lesson, and the missile flies") {
    const LessonRun run = PlayLesson("tutorials/school_operator.json");
    ExpectFinished(run, "operator");
    SC_EXPECT(run.launches >= 1);
}

SC_TEST("tutorial: a student who does what each step asks finishes the senior operator's lesson, and the missile flies") {
    const LessonRun run = PlayLesson("tutorials/school_senior_operator.json");
    ExpectFinished(run, "senior operator");
    SC_EXPECT(run.launches >= 1);
}

SC_TEST("tutorial: a student who does what each step asks finishes the deputy commander's lesson and launches") {
    const LessonRun run = PlayLesson("tutorials/school_deputy_commander.json");
    ExpectFinished(run, "deputy commander");
    SC_EXPECT(run.launches >= 1);
}

SC_TEST("tutorial: a student who does what each step asks finishes the commander's lesson, giving every order") {
    const LessonRun run = PlayLesson("tutorials/school_commander.json");
    ExpectFinished(run, "commander");
    SC_EXPECT(run.launches >= 1);
}

SC_TEST("tutorial: a student who does what each step asks finishes each of the PU-12M's lessons; the commander gives a launcher a target") {
    for (const char* seat : {"driver", "radio_operator", "squad_leader", "operator", "battery_commander"}) {
        const LessonRun run = PlayLesson(std::string("tutorials/school_pu12m_") + seat + ".json", 5000.0);
        ExpectFinished(run, seat);
        if (std::string(seat) == "battery_commander") SC_EXPECT(run.designations >= 1 && run.decoded == 0);  // he read 707 himself
        SC_EXPECT(run.deviations == 0 && run.refusals == 0);
    }
}

SC_TEST("tutorial: a push-button's step says to press it") {
    const std::string text = "{\"tutorial_id\": \"t\", \"title\": \"T\", \"scenario\": \"scenarios/school_deputy_commander.json\", \"steps\": ["
                             "{\"id\": \"a\", \"title\": \"A\", \"lines\": [\"x\"], \"control\": \"fo.launch\", \"position\": 1},"
                             "{\"id\": \"b\", \"title\": \"B\", \"lines\": [\"y\"]}]}";
    TutorialDef def;
    std::string e;
    SC_EXPECT(LoadTutorialJson(text, def, &e));
    std::unique_ptr<ScenarioRunner> r = Load("scenarios/school_deputy_commander.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    TutorialGuide g(def);
    SC_EXPECT(g.Bind(*r, &e));
    TutorialUi ui;
    g.Update(*r, ui);
    SC_EXPECT(g.Status(*r, ui).position_label == "press");
}
