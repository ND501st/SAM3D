// simcore/helper.hpp — the helper (docs/crew/14_HELPER.md): for the seat a player holds, the move that seat's NPC would
// make now, in the lessons' terms (a switch to set, a wheel to bring onto a mark, an order to give, a report to make, a
// target to give a launcher or to plot), so the front end highlights it as it highlights a lesson's step, without the
// lesson's card. It works in any scenario, because it asks the same rules the NPC crew act on.
//
// Where the moves come from, best first:
//   duty    what the seat's NPC would have done and the player hasn't yet (ScenarioRunner::OwedDuties, recorded by the
//           NPC rules themselves through ScenarioRunner::NpcDoes): reports, own-initiative drills (ЗАПРОС on a new echo),
//           the battery commander's designations, the plotter's reports to write, a ringing line, the halt; and in the
//           commanding seat, the shadow commander's orders (the NPC commander thinking in the player's place)
//   drill   the next step of the drill ordered to the seat: exactly what its NPC would do next
//   wheel   a hand wheel of the seat that its NPC would be turning onto the target now (ScenarioRunner::WheelWant)
//   wait    the ordered drill's next step, waiting for a lamp or another seat: what it waits for
//   ahead   when nobody will order the seat (no NPC commander), the drill master's best move toward the crew's task
//
// The level is the runner's AssistLevel (the missions menu's Helper setting, H in the cabin): guided shows the moves at
// once; hint only once the player has hesitated longer than his seat's NPC's patience; demonstrate shows what the seat's
// NPC is about to do (the seat not taken); unassisted and an exam show nothing.
//
// HelpDo makes a move through the seat's own API, as a player's hand would: the coverage map (examples/helper_coverage)
// plays every mission from every seat by following the helper, and the tests do the same.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/crew_callouts.hpp"
#include "simcore/crew_training.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/seat_duty.hpp"

namespace simcore {

struct HelpStep {
    enum class Kind { Control, Wheel, Order, Report, Designate, Plot, Halt, Wait };
    Kind kind = Kind::Wait;
    std::string source;  // duty, drill, wheel, wait, ahead, demonstrate
    std::string rule;    // why, in one line: the NPC's rule, the drill's name, the drill master's reason
    std::string seat;
    // Control, Wheel
    std::string control;
    std::string control_label;
    int position = -1;
    std::string position_label;
    int current_position = -1;
    double target = 0.0;     // a wheel's mark
    double value = 0.0;      // the wheel now
    double tolerance = 0.0;
    bool on_mark = false;
    // Order, Report: the callout and its words
    std::string callout;
    std::string words_ru;
    std::string words_en;
    // Designate, Plot: the target's board number, the launcher, the target
    int number = 0;
    std::string callsign;
    std::string target_id;
    // Wait: what the next move waits for
    std::string waiting_for;
    // An order the menu would refuse now, and why (the net it goes on can't be reached, a seat busy): shown, tried last
    std::string blocked;
    double due_in_s = 0.0;
    std::string text;  // the move in a few words, for the hint line
};

inline const char* ToString(HelpStep::Kind k) {
    switch (k) {
        case HelpStep::Kind::Control: return "control";
        case HelpStep::Kind::Wheel: return "wheel";
        case HelpStep::Kind::Order: return "order";
        case HelpStep::Kind::Report: return "report";
        case HelpStep::Kind::Designate: return "designate";
        case HelpStep::Kind::Plot: return "plot";
        case HelpStep::Kind::Halt: return "halt";
        case HelpStep::Kind::Wait: return "wait";
    }
    return "?";
}

// The help for one seat at the level set.
struct SeatHelp {
    std::string seat;
    AssistLevel level = AssistLevel::Guided;
    bool shown = false;       // the level shows the moves now
    bool hesitating = false;  // hint: the player has waited longer than his seat's NPC would
    std::vector<HelpStep> steps;  // best first
    const HelpStep* Top() const { return steps.empty() ? nullptr : &steps.front(); }
};

namespace detail {

inline std::string HelpBlanks(std::string t) {
    for (std::size_t a = t.find('{'); a != std::string::npos; a = t.find('{', a)) {
        const std::size_t b = t.find('}', a);
        if (b == std::string::npos) break;
        t.replace(a, b - a + 1, "…");
    }
    return t;
}

// A control as the crew names it: its label up to " — " (labels give both positions).
inline std::string HelpLabel(const ControlDef* d, const std::string& id) {
    if (d == nullptr || d->label.empty()) return id;
    std::string l = d->label;
    const std::size_t dash = l.find(" — ");
    if (dash != std::string::npos) l = l.substr(0, dash);
    return l;
}

inline void HelpControl(const ScenarioRunner& r, const std::string& control, int position, HelpStep& s) {
    const ControlDef* d = r.Console().panel.Find(control);
    s.kind = HelpStep::Kind::Control;
    s.control = control;
    s.control_label = HelpLabel(d, control);
    s.position = position;
    s.current_position = r.Console().panel.Get(control);
    if (d != nullptr && position >= 0 && static_cast<std::size_t>(position) < d->position_labels.size()) {
        s.position_label = d->position_labels[static_cast<std::size_t>(position)];
    } else if (d != nullptr && d->kind == ControlKind::Momentary) {
        s.position_label = "press";
    }
    s.text = s.control_label + (s.position_label.empty() ? std::string() : ": " + s.position_label);
}

// The words of a callout: the fields the NPC would have said them with, else what the seat's own menu fills in.
inline void HelpWords(const ScenarioRunner& r, const std::string& seat, const std::string& callout,
                      const std::map<std::string, std::string>& fields, HelpStep& s) {
    s.callout = callout;
    const CalloutDef* c = r.Def().has_crew_data ? r.Def().crew_data.FindCallout(callout) : nullptr;
    if (c == nullptr) return;
    if (fields.empty()) {
        const std::pair<std::string, std::string> w = r.ReportWords(seat, callout);
        s.words_ru = w.first;
        s.words_en = w.second;
    } else {
        s.words_ru = HelpBlanks(FillCallout(c->ru, fields));
        s.words_en = HelpBlanks(FillCallout(c->en, fields));
    }
}

// What a condition a step waits for is, in words.
inline std::string HelpDescribe(const ScenarioRunner& r, const Condition& c) {
    const ConsoleData& con = r.Console();
    switch (c.kind) {
        case Condition::Kind::SubsystemIs: {
            const SubsystemDef* d = con.equipment.Def(c.id);
            const std::string name = d != nullptr && !d->name.empty() ? d->name : c.id;
            switch (c.state) {
                case SubsystemState::Ready: return name + " ready";
                case SubsystemState::Off: return name + " off";
                case SubsystemState::Starting: return name + " starting";
                case SubsystemState::Stopping: return name + " stopping";
                case SubsystemState::Fault: return name + " in fault";
            }
            return name;
        }
        case Condition::Kind::ControlEquals:
        case Condition::Kind::ControlAtLeast: {
            const ControlDef* d = con.panel.Find(c.id);
            std::string t = HelpLabel(d, c.id);
            if (d != nullptr && c.position >= 0 && static_cast<std::size_t>(c.position) < d->position_labels.size()) {
                t += ": " + d->position_labels[static_cast<std::size_t>(c.position)];
            }
            if (d != nullptr && !d->station.empty()) t += " (the " + d->station + "'s)";
            return t;
        }
        case Condition::Kind::InputIs: {
            const CrewFact* f = r.Def().has_crew_data ? r.Def().crew_data.FindFact(c.id) : nullptr;
            if (f != nullptr && !f->unmet_en.empty() && c.input_value) return f->unmet_en;
            return c.id + (c.input_value ? std::string() : std::string(" off"));
        }
    }
    return c.id;
}

inline bool HelpWheelAngle(const ScenarioRunner& r, const std::string& wheel) {
    const TrackSensorDef& t = r.Console().track;
    return wheel == t.az_wheel || wheel == t.el_wheel || (r.Console().has_orientation && wheel == r.Console().orientation.setter);
}

inline double HelpWheelTolerance(const ScenarioRunner& r, const ControlDef& d) {
    // The tracking wheels: on the mark when the target is on the narrow TV's screen (its field is ±0.5°), and the range
    // gate within half a kilometre of the echo (sam3d: what an eye on the indicator holds, not the mark to a decimal).
    const TrackSensorDef& t = r.Console().track;
    if (d.id == t.az_wheel || d.id == t.el_wheel) return 0.5;
    if (HelpWheelAngle(r, d.id)) return 0.1;  // the gyro's setter: onto the heading the crew chief gave, to a tenth
    if (d.id == t.range_wheel) return 500.0;
    return 0.01 * (d.max_value - d.min_value);
}

// What a tracking wheel is for, in words (the player watches a display, not a number); "" for a wheel set to a number.
inline std::string HelpWheelWords(const ScenarioRunner& r, const std::string& id) {
    const TrackSensorDef& t = r.Console().track;
    if (id == t.az_wheel) return "turn it to bring the tracking radar's azimuth line onto the target's mark on the search scope, and keep it there";
    if (id == t.el_wheel) return "turn it to bring the target into the TV's crosshair (or onto its mark on the elevation indicator), and keep it there";
    if (id == t.range_wheel) return "turn it to put the range gate on the target's echo on the range indicator, and keep it there";
    return std::string();
}

inline void HelpWheel(const ScenarioRunner& r, const ControlDef& d, double target, double tolerance, HelpStep& s) {
    s.kind = HelpStep::Kind::Wheel;
    s.control = d.id;
    s.control_label = HelpLabel(&d, d.id);
    s.target = target;
    s.value = r.Console().panel.GetValue(d.id);
    s.tolerance = tolerance;
    double diff = s.value - target;
    if (d.wraps) diff = std::remainder(diff, d.max_value - d.min_value);
    s.on_mark = std::fabs(diff) <= tolerance;
    const std::string words = HelpWheelWords(r, d.id);
    if (!words.empty()) {
        s.text = s.control_label + ": " + words;
        return;
    }
    char t[96];
    if (d.id == r.Console().track.range_wheel) {
        std::snprintf(t, sizeof(t), ": onto %.1f km", target / 1000.0);
    } else if (HelpWheelAngle(r, d.id)) {
        std::snprintf(t, sizeof(t), ": onto %.1f°", target);
    } else {
        std::snprintf(t, sizeof(t), ": onto %.2f", target);
    }
    s.text = s.control_label + t;
}

// A duty as a move. False when it can't be shown as one (a drill already done).
inline bool HelpFromDuty(const ScenarioRunner& r, const SeatDuty& d, HelpStep& s) {
    s.source = "duty";
    s.rule = d.rule;
    s.seat = d.seat;
    s.target_id = d.target;
    switch (d.kind) {
        case SeatDuty::Kind::Report:
            s.kind = HelpStep::Kind::Report;
            HelpWords(r, d.seat, d.id, d.fields, s);
            s.text = "Report: " + (s.words_ru.empty() ? d.id : s.words_ru);
            return true;
        case SeatDuty::Kind::Order:
            s.kind = HelpStep::Kind::Order;
            HelpWords(r, d.seat, d.id, d.fields, s);
            s.text = "Order: " + (s.words_ru.empty() ? d.id : s.words_ru);
            if (d.seat == r.Cmdr()) {
                const ScenarioRunner::OrderCheck c = r.CheckOrder(d.id);
                if (!c.can) s.blocked = c.reason_en;
            }
            return true;
        case SeatDuty::Kind::Drill: {
            const ProcedureStep* st = r.DrillNextStep(d.id, d.seat, d.since_s);
            if (st == nullptr) return false;
            HelpControl(r, st->control_id, st->value, s);
            if (!st->description.empty()) s.rule = d.rule + " (" + st->description + ")";
            return true;
        }
        case SeatDuty::Kind::Control:
            HelpControl(r, d.id, d.number, s);
            return true;
        case SeatDuty::Kind::Wheel: {
            const ControlDef* c = r.Console().panel.Find(d.id);
            if (c == nullptr) return false;
            HelpWheel(r, *c, d.value, d.tolerance, s);
            return true;
        }
        case SeatDuty::Kind::Designate: {
            s.kind = HelpStep::Kind::Designate;
            s.number = d.number;
            s.callsign = d.id;
            char t[96];
            std::snprintf(t, sizeof(t), "Target %02d to launcher %s", d.number, d.id.c_str());
            s.text = t;
            return true;
        }
        case SeatDuty::Kind::Plot: {
            s.kind = HelpStep::Kind::Plot;
            s.number = d.number;
            char t[64];
            std::snprintf(t, sizeof(t), "Plot report %02d on the board", d.number);
            s.text = t;
            return true;
        }
        case SeatDuty::Kind::Answer: {  // the line's lever on the seat's box, else just the line
            const VoiceComms* v = r.Voice();
            const VoiceBoxDef* b = v != nullptr ? v->BoxOf(d.seat) : nullptr;
            if (b == nullptr || d.number < 1 || static_cast<std::size_t>(d.number) > b->lines.size()) return false;
            HelpControl(r, b->lines[static_cast<std::size_t>(d.number - 1)], 1, s);
            s.text = "Answer line " + std::to_string(d.number) + ": " + s.control_label;
            return true;
        }
        case SeatDuty::Kind::Halt:
            s.kind = HelpStep::Kind::Halt;
            s.text = "Halt the vehicle";
            return true;
    }
    return false;
}

}  // namespace detail

// The drill master's advice is a ghost run of the whole drill (tens of milliseconds): kept, and worked out again only when
// the console has changed (a switch, a subsystem's state, an input) or the task has. One per caller (the front end, a
// follower).
struct HelpMemo {
    std::string key;
    DrillAdvice advice;
};

namespace detail {

// The console's state as a key: every switch's position, every subsystem's state, every input; and the seat and task.
inline std::string HelpStateKey(const ScenarioRunner& r, const std::string& seat, const std::string& task) {
    const ConsoleData& con = r.Console();
    std::string k = seat + "|" + task + "|";
    for (const ControlDef& d : con.panel.Controls()) {
        if (d.kind != ControlKind::Continuous) k += static_cast<char>('0' + con.panel.Get(d.id) % 64);
    }
    k += '|';
    for (const Equipment::UnitStateCopy& u : con.equipment.UnitStates()) k += static_cast<char>('a' + static_cast<int>(u.state) * 2 + (u.latched ? 1 : 0));
    k += '|';
    for (const std::pair<std::string, bool>& in : con.equipment.Inputs()) k += in.second ? '1' : '0';
    return k;
}

}  // namespace detail

// Every move the helper has for a seat a player holds now, best first, whatever the level. `memo` keeps the drill
// master's advice between calls (null: worked out each time).
inline std::vector<HelpStep> HelpMoves(const ScenarioRunner& r, const std::string& seat, HelpMemo* memo = nullptr) {
    std::vector<HelpStep> out;
    if (seat.empty() || r.SeatHolder(seat).empty()) return out;
    const ConsoleData& con = r.Console();
    // What the seat's NPC would have done already.
    for (const SeatDuty& d : r.OwedDuties(seat)) {
        HelpStep s;
        if (detail::HelpFromDuty(r, d, s)) out.push_back(std::move(s));
    }
    // The ordered drill's next step (a check whose switch is already there is passed by the next move).
    HelpStep wait;
    bool waiting = false;
    if (const ProcedureTracker* t = r.SeatTracker(seat)) {
        const Procedure& p = t->GetProcedure();
        for (std::size_t i = t->NextIndex(); !p.id.empty() && i < p.steps.size(); ++i) {
            const ProcedureStep& st = p.steps[i];
            const ControlDef* d = con.panel.Find(st.control_id);
            const bool there = d != nullptr && d->kind != ControlKind::Momentary && con.panel.Get(st.control_id) == st.value;
            const bool ready = con.equipment.AllHold(st.wait_for, con.panel);
            if (there && ready) continue;
            HelpStep s;
            s.seat = seat;
            s.rule = p.name.empty() ? p.id : p.name;
            if (!st.description.empty()) s.rule += ": " + st.description;
            detail::HelpControl(r, st.control_id, st.value, s);
            const double left = i == t->NextIndex() ? st.min_wait_s - t->ElapsedSinceStep() : 0.0;
            if (ready && !there) {
                s.source = "drill";
                s.due_in_s = left > 0.0 ? left : 0.0;
                out.push_back(std::move(s));
            } else {
                s.source = "wait";
                s.kind = HelpStep::Kind::Wait;
                for (const Condition& c : st.wait_for) {
                    if (!con.equipment.Holds(c, con.panel)) {
                        s.waiting_for = detail::HelpDescribe(r, c);
                        break;
                    }
                }
                s.text = "Wait for " + s.waiting_for + ", then " + s.text;
                wait = std::move(s);
                waiting = true;
            }
            break;
        }
    }
    // The wheels its NPC would be turning onto the target.
    for (const ControlDef& d : con.panel.Controls()) {
        if (d.kind != ControlKind::Continuous || d.station != seat) continue;
        if (!d.guard_id.empty() && con.panel.Get(d.guard_id) < 1) continue;  // locked: its NPC can't turn it either
        if (!r.WheelDriven(d.id).empty()) continue;                         // the equipment turns it: hands off
        double want = 0.0;
        if (!r.WheelWant(d.id, want)) continue;
        if (!d.wraps) want = std::clamp(want, d.min_value, d.max_value);  // a target past its stop: as far as it goes
        HelpStep s;
        s.seat = seat;
        s.source = "wheel";
        s.rule = "its NPC turns it onto the target the crew is on";
        detail::HelpWheel(r, d, want, detail::HelpWheelTolerance(r, d), s);
        if (!s.on_mark) out.push_back(std::move(s));
    }
    if (waiting) out.push_back(std::move(wait));
    // Nobody will order this seat: what the drill master would do toward the crew's task.
    if (out.empty() && !r.NpcCommanderActive() && r.Drill() != nullptr && r.Def().has_crew_data) {
        const CrewTask* task = r.Def().crew_data.FindTask(r.TrainingTask());
        DrillAdvice a;
        if (task != nullptr) {
            const std::string key = memo != nullptr ? detail::HelpStateKey(r, seat, task->id) : std::string();
            if (memo != nullptr && memo->key == key) {
                a = memo->advice;
            } else {
                a = r.Drill()->NextFor(seat, r.Drill()->GoalFor(*task), con);
                if (memo != nullptr) {
                    memo->key = key;
                    memo->advice = a;
                }
            }
        }
        if (!a.control.empty()) {
            HelpStep s;
            s.seat = seat;
            s.source = "ahead";
            s.rule = a.why.empty() ? task->id : a.why.front();
            detail::HelpControl(r, a.control, a.position, s);
            s.due_in_s = a.due_in_s;
            out.push_back(std::move(s));
        }
    }
    // A move that can't be made yet (an order the menu would refuse) after those that can.
    std::stable_sort(out.begin(), out.end(), [](const HelpStep& a, const HelpStep& b) { return a.blocked.empty() && !b.blocked.empty(); });
    for (HelpStep& s : out) s.seat = seat;
    return out;
}

// The help for a seat at the runner's level.
inline SeatHelp HelpFor(const ScenarioRunner& r, const std::string& seat, HelpMemo* memo = nullptr) {
    SeatHelp h;
    h.seat = seat;
    h.level = r.GetAssistLevel();
    switch (h.level) {
        case AssistLevel::Unassisted:
        case AssistLevel::Exam:
            return h;
        case AssistLevel::Demonstrate: {  // the seat's NPC works it: what he moves next
            const AssistView v = r.Assistance(seat);
            if (!v.show || v.control.empty()) return h;
            HelpStep s;
            s.seat = seat;
            s.source = "demonstrate";
            s.rule = v.why.empty() ? v.description : v.why.front();
            detail::HelpControl(r, v.control, v.position, s);
            s.due_in_s = v.due_in_s;
            h.steps.push_back(std::move(s));
            h.shown = true;
            return h;
        }
        case AssistLevel::Hint:
        case AssistLevel::Guided:
            break;
    }
    h.steps = HelpMoves(r, seat, memo);
    h.hesitating = r.SeatHesitation(seat) >= r.SeatPatience(seat);
    h.shown = !h.steps.empty() && (h.level == AssistLevel::Guided || h.hesitating);
    return h;
}

// Makes a move through the seat's own API, as the player's hand would (the coverage map, the tests, "do it for me"). A
// switch behind a closed cover opens the cover first; a stepwise selector goes one position on. False when nothing
// could be done (a wait, a refusal: `why` says).
inline bool HelpDo(ScenarioRunner& r, const HelpStep& s, std::string* why = nullptr) {
    switch (s.kind) {
        case HelpStep::Kind::Control: {
            const ControlPanel& panel = r.Console().panel;
            const ControlDef* d = panel.Find(s.control);
            int to = s.position;
            if (d != nullptr && !panel.CanMove(s.control, to)) {
                if (!d->guard_id.empty() && panel.Get(d->guard_id) < 1) {
                    const StepResult g = r.SeatSet(s.seat, d->guard_id, 1, why);
                    return g != StepResult::Blocked && g != StepResult::UnknownControl && g != StepResult::InvalidPosition &&
                           g != StepResult::WaitRequired;
                }
                const int now = panel.Get(s.control);
                if (d->stepwise && now != to) to = now + (to > now ? 1 : -1);
            }
            const StepResult res = r.SeatSet(s.seat, s.control, to, why);
            return res != StepResult::Blocked && res != StepResult::UnknownControl && res != StepResult::InvalidPosition &&
                   res != StepResult::WaitRequired;
        }
        case HelpStep::Kind::Wheel: return r.SeatTurn(s.seat, s.control, s.target, why);
        case HelpStep::Kind::Order: {
            ScenarioRunner::OrderCheck c;
            const bool ok = r.TryCrewOrder(s.callout, &c);
            if (!ok && why != nullptr) *why = c.reason_en;
            return ok;
        }
        case HelpStep::Kind::Report: {
            const bool ok = r.PlayerSay(s.seat, s.callout);
            if (!ok && why != nullptr) *why = r.LastRefusal().empty() ? "the report could not be made" : r.LastRefusal();
            return ok;
        }
        case HelpStep::Kind::Designate: return r.SeatDesignate(s.seat, s.callsign, s.number, why);
        case HelpStep::Kind::Plot: return r.SeatPlot(s.seat, s.number, why);
        case HelpStep::Kind::Halt: return r.Console().equipment.SetInput("vehicle_stopped", true);  // the front end brakes
        case HelpStep::Kind::Wait:
            if (why != nullptr) *why = "waiting for " + s.waiting_for;
            return false;
    }
    return false;
}

// A player who does what the helper shows at his seat (the coverage map, the tests): one move per reaction time, the
// first the helper lists that he can make now. A move that was refused is not tried again for a few seconds.
class HelpFollower {
public:
    explicit HelpFollower(std::string seat, double reaction_s = 0.6) : seat_(std::move(seat)), reaction_s_(reaction_s) {}

    // After each runner step. The move made, or null.
    const HelpStep* Update(ScenarioRunner& r) {
        if (r.Time() + 1e-9 < next_s_) return nullptr;
        next_s_ = r.Time() + kLookS;  // nothing to do: he looks again in a moment (the drill master's advice costs a ghost run)
        for (const HelpStep& s : HelpMoves(r, seat_, &memo_)) {
            if (s.kind == HelpStep::Kind::Wait || s.due_in_s > 0.05) continue;
            const std::string key = std::string(ToString(s.kind)) + "|" + s.control + "|" + s.callout + "|" + s.callsign + "|" +
                                    std::to_string(s.number) + "|" + std::to_string(s.position);
            const auto refused = refused_until_.find(key);
            if (refused != refused_until_.end() && r.Time() < refused->second) continue;
            std::string why;
            HelpStep move = s;
            if (s.kind == HelpStep::Kind::Wheel) move.target = Turned(r, s);  // his hand turns it, it doesn't jump
            if (!HelpDo(r, move, &why)) {
                refused_until_[key] = r.Time() + kRetryS;
                ++refusals_;
                last_refusal_ = s.text + ": " + why;
                continue;
            }
            last_ = s;
            ++moves_;
            ++by_source_[s.source];
            ++by_kind_[ToString(s.kind)];
            // A hand on a wheel keeps following its mark; a switch, an order or a report takes a reaction each.
            next_s_ = r.Time() + (s.kind == HelpStep::Kind::Wheel ? kWheelS : reaction_s_);
            if (s.kind == HelpStep::Kind::Wheel) wheel_s_[s.control] = r.Time();
            return &last_;
        }
        return nullptr;
    }

    // Where his hand brings a wheel by now: toward the mark at the rate the NPCs turn the tracking radar's wheels (the
    // scenario's aim tuning), from where it is; another wheel at once. (A wheel that jumped onto each mark would miss what
    // a turning one sweeps across: the TV search finds the target as the elevation wheel passes it.)
    double Turned(const ScenarioRunner& r, const HelpStep& s) const {
        const TrackSensorDef& t = r.Console().track;
        const EngagementTuning& tune = r.Def().tuning;
        const double rate = s.control == t.az_wheel ? tune.aim_az_rate_deg_s : s.control == t.el_wheel ? tune.aim_el_rate_deg_s
                            : s.control == t.range_wheel                     ? tune.aim_range_rate_mps
                                                                             : 0.0;
        if (!(rate > 0.0)) return s.target;
        const auto last = wheel_s_.find(s.control);
        const double dt = std::min(reaction_s_, last == wheel_s_.end() ? reaction_s_ : r.Time() - last->second);
        const double step = rate * std::max(dt, kWheelS);
        const double diff = s.target - s.value;
        return std::fabs(diff) <= step ? s.target : s.value + (diff > 0.0 ? step : -step);
    }

    const std::string& Seat() const { return seat_; }
    int Moves() const { return moves_; }
    int Refusals() const { return refusals_; }
    const std::string& LastRefusal() const { return last_refusal_; }
    const std::map<std::string, int>& BySource() const { return by_source_; }
    const std::map<std::string, int>& ByKind() const { return by_kind_; }

private:
    static constexpr double kRetryS = 3.0;
    static constexpr double kLookS = 0.5;
    static constexpr double kWheelS = 0.1;
    std::string seat_;
    double reaction_s_ = 0.6;
    double next_s_ = 0.0;
    HelpStep last_;
    int moves_ = 0;
    int refusals_ = 0;
    std::string last_refusal_;
    std::map<std::string, double> refused_until_;
    std::map<std::string, int> by_source_;
    std::map<std::string, int> by_kind_;
    std::map<std::string, double> wheel_s_;  // when he last turned each wheel
    HelpMemo memo_;
};

}  // namespace simcore
