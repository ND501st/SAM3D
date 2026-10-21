// simcore/tutorial.hpp — the tutorial lessons (data/tutorials/*.json; docs/TUTORIALS.md, docs/PLAYING.md).
//
// A lesson is a list of steps. Each step shows a few lines (what to do, why) and finishes when its one action is done
// (docs/research/TUTORIAL_DESIGN_2026-10.md: one action per step, gated on the real control):
//
//   "continue": true            a reading step: done when the learner presses the continue key (TutorialUi::continues)
//   "control" + "position"      a switch, selector, key, cover or button of the console set to that position (a button:
//                               pressed during the step)
//   "wheel" + "aim"             a hand wheel brought onto a value: "value" (with "target"), "contact_azimuth" or
//                               "contact_range" (the nearest contact the lesson's seat knows of, from its own instruments or
//                               the crew's reports), "qc" (the crew chief's heading for the orientation, from the log),
//                               "marker_azimuth" or "marker_range" (the PU-12M's DT-2 marker onto the target nearest the
//                               post on the IT-45: its bearing in degrees, its range in fractions of the tube's scale);
//                               "hold": true keeps it there until the step's "done_when" (the capture: the target moves)
//   "order"                     an order of the crew data given (its words spoken) during the step: the commander's
//   "report"                    a report (a callout of the crew data) made from the lesson's seat during the step: a seat
//                               a player holds makes its own reports (the crew menu, O), its NPC no longer does; it
//                               counts when made, not when the intercom is free to say it
//   "done_when"                 conditions (below): any one finishes the step; with an action, either finishes it
//
// An action can't be done before its wait: the console procedure's wait_for for that move ("procedure"; found by
// itself when only one procedure of the seat has it) and the lesson's own "wait_until" conditions; until then the step
// says what it waits for. "on_start" actions run when the step comes up: "order:<id>" (the commander, an NPC in a seat
// lesson, gives that order: the lesson keeps the drill at the learner's pace) and "release:<target>" (a target held
// with "on_cue" appears); for the voice paths (docs/crew/12_COMMUNICATIONS.md) "net_order:<net>" (the net's main station
// orders it to its other frequency now, on air), "jam:<net>" (a strong spot jammer comes on the net now),
// "line_call:<n>" (the far end of the RTK-1's line n rings) and "regiment:<group or code>" (the regiment's command held
// "on_cue" in the scenario goes out now: a ПТ-1 group on its command net, or a telecode). "fast_forward": the front end
// may run time faster while the step only waits.
// "conditions_only": the step's action is shown and asked for, but only its "done_when" finishes it (the ДХ's launcher
// key: done when the designation has gone out, not when the key is pressed).
// "decode": "<group>": the step's order or report answers a talk-table group the learner reads against the table himself
// (docs/crew/12_COMMUNICATIONS.md §2.9). Until he has done it the card doesn't name it and the crew menu doesn't mark it;
// another line is refused with the group, not the answer. kDecodeRevealS after its wait is over (or once skipped) it is
// named like any other.
//
// A "strict" lesson lets the learner work only the step's control (Refusal says why not), at the lesson's "seat".
//
// The learner can go back a step (Back): it comes up again to read, and Enter (or doing it again) moves on; what was done
// stays done, and its start actions don't run twice. He can skip a step (Skip): the instructor does it for him, the
// switch set, the wheel turned (and held on a moving mark until the step is done), the order given, the report made, as
// soon as its wait allows; so the drill goes on as if he had done it. A step with nothing to do is just passed.
// The guide never tells the crew anything itself: orders go through the runner as the commander's, moves through the
// learner's seat like any player's (ScenarioRunner::SeatSet), so the debrief judges them.
//
// Conditions (strings in "done_when" and "wait_until"):
//   view:F6          the camera is in that view now (F1-F12)
//   views:3          at least that many different views tried since the guide began
//   cabin            the camera is in the cabin
//   seat:commander   the camera is at that seat of the cabin
//   holding:commander  the player holds that seat
//   task:captured    a task of the crew data holds
//   order:order_k_boyu  that order (a crew data order's callout) has been spoken since the step began
//   heard:report_soc_ready  that callout has been spoken since the step began (anyone's)
//   debrief:HIT      a debrief line of that kind since the step began (LAUNCH, HIT, MISS, FAULT, DEVIATION)
//   input:target_in_zone  a console input is on
//   subsystem:power=ready  a subsystem in that state (ready, off, starting, stopping, fault)
//   control:soc.hv=1 a control at that position
//   contact:senior_operator  that seat knows of a contact that isn't a friend (azimuth or range, seen in the last 30 s)
//   contact_in:senior_operator=35000  ... and it is that close (m)
//   moved:100        the vehicle has moved that far (m) since the step began
//   ended            the scenario has ended
//   time:300         sim time is at least that (s)
//   after:20         that many real seconds since the step began
//   ui:keys_card     the front end reports it happened (TutorialUi::events; TutorialUiEvents lists them)
//   marked           the PU-12M's DT-2 marker is on a target of the IT-45
//   designated       the battery commander has given a launcher a target since the step began
//   plotted          the plotter has written a target on the board since the step began
//   given_target     the Osa's commander has heard a target given his vehicle on the battery net since the step began
//   group:707        a talk-table group has been said on a net the lesson's seat hears (any seat in a free lesson) since
//                    the step began
// A step without an action or conditions is the last: it stays until the player leaves the mission. A step finished by
// conditions alone is the learner's move when one of them is his own doing (a view, the cabin, a seat, the keys, driving
// or halting: the vehicle moves only under his keys); otherwise he watches the crew or the world finish it.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

namespace simcore {

struct TutorialCondition {
    enum class Kind {
        View, Views, Cabin, Seat, Holding, Task, Order, Heard, Debrief, Input, Subsystem, Control, Contact, ContactIn,
        Moved, Ended, Time, After, Ui, Marked, Designated, Plotted, GivenTarget, Group
    };
    Kind kind = Kind::Ended;
    std::string arg;
    std::string value;   // after '=' (subsystem state, control position, contact distance)
    double number = 0.0;
    std::string text;    // as written in the data
};

struct TutorialAction {
    enum class Kind { Order, Release, NetOrder, Jam, LineCall, Regiment };
    Kind kind = Kind::Order;
    std::string arg;
    std::string text;
};

// How a wheel step finds the value to bring the wheel onto.
enum class TutorialAim { None, Value, ContactAzimuth, ContactRange, Qc, MarkerAzimuth, MarkerRange };

// A wheel aim whose differences don't wrap at 360 degrees (a range, a value).
inline bool TutorialAimLinear(TutorialAim a) {
    return a == TutorialAim::ContactRange || a == TutorialAim::Value || a == TutorialAim::MarkerRange;
}

struct TutorialStep {
    std::string id;
    std::string title;
    std::string chapter;             // set on a chapter's first step; the following steps belong to it
    std::vector<std::string> lines;  // what to do, a sentence or two each
    std::string why;                 // one sentence: what the action is for
    std::string hint;                // the keys for it, one line
    std::string look;                // a panel, display, lamp or control the front end highlights (a tour step)
    // The step's action (at most one).
    bool continue_key = false;
    std::string control;
    int position = -1;
    std::string wheel;
    TutorialAim aim = TutorialAim::None;
    double target = 0.0;
    double tolerance = 0.0;
    bool hold = false;               // a wheel kept on its mark until done_when, not just brought onto it
    std::string order;
    std::string report;
    std::vector<TutorialCondition> done_when;
    // Before the action can be done.
    std::string procedure;               // the console procedure the move belongs to (its wait_for and min_wait_s)
    std::vector<Condition> wait_for;     // filled from the procedure when the lesson is bound to its console
    double min_wait_s = 0.0;
    std::string drill;                   // the procedure step's own description
    std::vector<TutorialCondition> wait_until;
    std::string wait_text;               // what it waits for, in words (else made from the conditions)
    std::vector<TutorialAction> on_start;
    bool fast_forward = false;
    bool conditions_only = false;        // the action is shown and expected, but only "done_when" finishes the step
    std::string decode;                  // a talk-table group the order or report answers: the learner reads it himself

    bool HasAction() const { return continue_key || !control.empty() || !wheel.empty() || !order.empty() || !report.empty(); }
};

struct TutorialDef {
    std::string id;
    std::string title;
    std::string scenario;  // the scenario it is written for, relative to the data folder
    std::string seat;      // the learner's seat ("" any)
    bool strict = false;   // only the step's control works
    std::vector<TutorialStep> steps;
};

// What the game's front end tells the guide each update.
struct TutorialUi {
    std::string view;                  // the camera view now: "F1".."F12"
    std::set<std::string> views_used;  // every view tried since the guide began
    bool in_cabin = false;
    std::string seat;                  // the cabin seat the camera is at ("" outside)
    std::string holding;               // the seat the player holds ("" none)
    std::set<std::string> events;      // TutorialUiEvents that have happened since the guide began
    double real_time_s = 0.0;          // real seconds since the guide began (paused time not counted)
    int continues = 0;                 // presses of the continue key since the guide began
};

// The front-end events a condition may wait for.
inline const std::vector<std::string>& TutorialUiEvents() {
    static const std::vector<std::string> events = {
        "keys_card",    // the keys card was opened (G)
        "time_faster",  // time was sped up (Ctrl+Z)
        "time_normal",  // time back to normal (Shift+Z)
        "crew_menu",    // the crew menu was opened (O)
        "orbit",        // the external camera was turned with the mouse
        "zoom",         // the external camera was zoomed with the wheel
        "look",         // the head was turned onto the highlighted control (L)
        "pick",         // a target was picked on the post's tube or board ([ or ])
    };
    return events;
}

// The step as the front end shows it now.
struct TutorialStatus {
    enum class Phase {
        None,   // no lesson, or past its end
        Read,   // read, then the continue key
        Wait,   // the action can't be done yet: wait_text says what for
        Act,    // do the action, or meet the step's own conditions (a view, the cabin, driving)
        Watch,  // nothing to do: the crew or the world finishes the step (wait_text)
        End     // the last step
    };
    Phase phase = Phase::None;
    std::size_t index = 0;
    std::size_t count = 0;
    std::string chapter;
    std::string wait_text;
    // The action.
    std::string control;          // a control or wheel to work (Act and Wait)
    std::string control_label;
    std::string station;          // whose control it is
    int position = -1;
    std::string position_label;
    int current_position = -1;
    bool wheel = false;
    bool target_known = false;    // a wheel's mark is known (a contact on the scope)
    double target = 0.0;
    double tolerance = 0.0;
    double value = 0.0;           // the wheel now
    bool wraps = false;           // an angle: differences wrap at 360
    std::string order;            // an order to give, or a report to make: its callout id
    std::string order_ru;
    std::string order_en;
    bool report = false;          // the menu line is a report of the seat's, not an order
    bool on_mark = false;         // a wheel within its tolerance of the mark
    bool fast_forward = false;
    std::string feedback;         // the learner's last move off the step, in words ("" none)
    bool review = false;          // gone back to: shown again to read (Phase::Read); Enter moves on
    bool instructor = false;      // skipped: the instructor is doing it
    std::string decode;           // the group the learner reads against his table; while set, order and its words are
                                  // left empty (the menu marks no line)
};

// A condition the learner meets himself (TutorialStatus::Phase::Act), not one the crew or the world meets.
inline bool TutorialLearnerCondition(const TutorialCondition& c) {
    switch (c.kind) {
        case TutorialCondition::Kind::View:
        case TutorialCondition::Kind::Views:
        case TutorialCondition::Kind::Cabin:
        case TutorialCondition::Kind::Seat:
        case TutorialCondition::Kind::Holding:
        case TutorialCondition::Kind::Moved:
        case TutorialCondition::Kind::Ui:
        case TutorialCondition::Kind::Plotted:
            return true;
        case TutorialCondition::Kind::Input:
            return c.arg == "vehicle_stopped";  // the halt: his brake
        default:
            return false;
    }
}

namespace detail {

inline double TutorialWrap180(double deg) {
    double d = std::fmod(deg + 180.0, 360.0);
    if (d < 0.0) d += 360.0;
    return d - 180.0;
}

inline bool ParseSubsystemState(const std::string& s, SubsystemState& out) {
    if (s == "ready") out = SubsystemState::Ready;
    else if (s == "off") out = SubsystemState::Off;
    else if (s == "starting") out = SubsystemState::Starting;
    else if (s == "stopping") out = SubsystemState::Stopping;
    else if (s == "fault") out = SubsystemState::Fault;
    else return false;
    return true;
}

inline bool ParseTutorialCondition(const std::string& text, TutorialCondition& out, std::string& why) {
    out = TutorialCondition();
    out.text = text;
    const std::size_t colon = text.find(':');
    const std::string head = colon == std::string::npos ? text : text.substr(0, colon);
    std::string arg = colon == std::string::npos ? std::string() : text.substr(colon + 1);
    auto number_of = [&](const std::string& s, double& value) {
        char* end = nullptr;
        value = std::strtod(s.c_str(), &end);
        if (s.empty() || end == nullptr || *end != '\0' || value < 0.0) {
            why = "'" + text + "': expected a number >= 0";
            return false;
        }
        return true;
    };
    auto needs_arg = [&]() {
        if (arg.empty()) why = "'" + text + "': needs a value after the colon";
        return !arg.empty();
    };
    auto split_value = [&]() {
        const std::size_t eq = arg.find('=');
        if (eq == std::string::npos || eq == 0 || eq + 1 >= arg.size()) {
            why = "'" + text + "': expected " + head + ":<id>=<value>";
            return false;
        }
        out.value = arg.substr(eq + 1);
        arg = arg.substr(0, eq);
        out.arg = arg;
        return true;
    };
    out.arg = arg;
    if (head == "cabin" || head == "ended" || head == "marked" || head == "designated" || head == "plotted" || head == "given_target") {
        if (!arg.empty()) {
            why = "'" + text + "': takes no value";
            return false;
        }
        out.kind = head == "cabin"        ? TutorialCondition::Kind::Cabin
                 : head == "ended"        ? TutorialCondition::Kind::Ended
                 : head == "marked"       ? TutorialCondition::Kind::Marked
                 : head == "designated"   ? TutorialCondition::Kind::Designated
                 : head == "given_target" ? TutorialCondition::Kind::GivenTarget
                                          : TutorialCondition::Kind::Plotted;
        return true;
    }
    if (head == "view") {
        out.kind = TutorialCondition::Kind::View;
        const int n = arg.size() >= 2 && arg[0] == 'F' ? std::atoi(arg.c_str() + 1) : 0;
        if (n < 1 || n > 12 || arg != "F" + std::to_string(n)) {
            why = "'" + text + "': a view is F1 to F12";
            return false;
        }
        return true;
    }
    if (head == "views" || head == "time" || head == "after" || head == "moved") {
        out.kind = head == "views" ? TutorialCondition::Kind::Views
                 : head == "time"  ? TutorialCondition::Kind::Time
                 : head == "after" ? TutorialCondition::Kind::After
                                   : TutorialCondition::Kind::Moved;
        return number_of(arg, out.number);
    }
    if (head == "debrief") {
        out.kind = TutorialCondition::Kind::Debrief;
        for (const char* k : {"LAUNCH", "HIT", "MISS", "FAULT", "DEVIATION"}) {
            if (arg == k) return true;
        }
        why = "'" + text + "': a debrief kind is LAUNCH, HIT, MISS, FAULT or DEVIATION";
        return false;
    }
    if (head == "ui") {
        out.kind = TutorialCondition::Kind::Ui;
        for (const std::string& e : TutorialUiEvents()) {
            if (arg == e) return true;
        }
        why = "'" + text + "': unknown front-end event";
        return false;
    }
    if (head == "subsystem") {
        out.kind = TutorialCondition::Kind::Subsystem;
        if (!split_value()) return false;
        SubsystemState state;
        if (!ParseSubsystemState(out.value, state)) {
            why = "'" + text + "': a state is ready, off, starting, stopping or fault";
            return false;
        }
        return true;
    }
    if (head == "control" || head == "contact_in") {
        out.kind = head == "control" ? TutorialCondition::Kind::Control : TutorialCondition::Kind::ContactIn;
        return split_value() && number_of(out.value, out.number);
    }
    if (head == "seat") out.kind = TutorialCondition::Kind::Seat;
    else if (head == "holding") out.kind = TutorialCondition::Kind::Holding;
    else if (head == "task") out.kind = TutorialCondition::Kind::Task;
    else if (head == "order") out.kind = TutorialCondition::Kind::Order;
    else if (head == "heard") out.kind = TutorialCondition::Kind::Heard;
    else if (head == "input") out.kind = TutorialCondition::Kind::Input;
    else if (head == "contact") out.kind = TutorialCondition::Kind::Contact;
    else if (head == "group") out.kind = TutorialCondition::Kind::Group;
    else {
        why = "'" + text + "': unknown condition (view, views, cabin, seat, holding, task, order, heard, debrief, input, "
              "subsystem, control, contact, contact_in, moved, ended, time, after, ui, marked, designated, plotted, "
              "given_target, group)";
        return false;
    }
    return needs_arg();
}

inline bool ParseTutorialAction(const std::string& text, TutorialAction& out, std::string& why) {
    out = TutorialAction();
    out.text = text;
    const std::size_t colon = text.find(':');
    const std::string head = colon == std::string::npos ? text : text.substr(0, colon);
    out.arg = colon == std::string::npos ? std::string() : text.substr(colon + 1);
    if (head == "order") out.kind = TutorialAction::Kind::Order;
    else if (head == "release") out.kind = TutorialAction::Kind::Release;
    else if (head == "net_order") out.kind = TutorialAction::Kind::NetOrder;
    else if (head == "jam") out.kind = TutorialAction::Kind::Jam;
    else if (head == "line_call") out.kind = TutorialAction::Kind::LineCall;
    else if (head == "regiment") out.kind = TutorialAction::Kind::Regiment;
    else {
        why = "'" + text + "': unknown action (order, release, net_order, jam, line_call, regiment)";
        return false;
    }
    if (out.arg.empty()) {
        why = "'" + text + "': needs a value after the colon";
        return false;
    }
    return true;
}

class TutorialLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, TutorialDef& out) {
        if (!Keys(root, "", {"tutorial_id", "title", "scenario", "seat", "strict", "status", "source", "notes", "steps"}) ||
            !RequireString(root, "", "tutorial_id", out.id) || !RequireString(root, "", "title", out.title) ||
            !RequireString(root, "", "scenario", out.scenario) || !OptionalString(root, "", "seat", out.seat) ||
            !OptionalBool(root, "", "strict", out.strict)) {
            return false;
        }
        const json::Value* steps = root.Find("steps");
        if (steps == nullptr || !steps->Is(json::Type::Array) || steps->items.empty()) return Fail("steps", "required, a non-empty list");
        for (std::size_t i = 0; i < steps->items.size(); ++i) {
            TutorialStep step;
            if (!LoadStep(steps->items[i], Index("steps", i), out, step)) return false;
            if (!step.HasAction() && step.done_when.empty() && i + 1 != steps->items.size()) {
                return Fail(Index("steps", i), "only the last step may have no action and no conditions");
            }
            out.steps.push_back(std::move(step));
        }
        return true;
    }

private:
    bool LoadStep(const json::Value& s, const std::string& p, const TutorialDef& def, TutorialStep& step) {
        if (!Keys(s, p, {"id", "title", "chapter", "lines", "why", "hint", "look", "continue", "control", "position", "wheel",
                         "aim", "target", "tolerance", "hold", "order", "report", "done_when", "procedure", "wait_until", "wait_text",
                         "on_start", "fast_forward", "conditions_only", "decode", "notes"}) ||
            !RequireString(s, p, "id", step.id) || !RequireString(s, p, "title", step.title) ||
            !OptionalString(s, p, "chapter", step.chapter) || !OptionalString(s, p, "why", step.why) ||
            !OptionalString(s, p, "hint", step.hint) || !OptionalString(s, p, "look", step.look) ||
            !OptionalBool(s, p, "continue", step.continue_key) || !OptionalString(s, p, "control", step.control) ||
            !OptionalInt(s, p, "position", step.position) || !OptionalString(s, p, "wheel", step.wheel) ||
            !OptionalNumber(s, p, "target", step.target) || !OptionalNumber(s, p, "tolerance", step.tolerance) ||
            !OptionalBool(s, p, "hold", step.hold) ||
            !OptionalString(s, p, "order", step.order) || !OptionalString(s, p, "report", step.report) ||
            !OptionalString(s, p, "procedure", step.procedure) ||
            !OptionalString(s, p, "wait_text", step.wait_text) || !OptionalBool(s, p, "fast_forward", step.fast_forward) ||
            !OptionalBool(s, p, "conditions_only", step.conditions_only) || !OptionalString(s, p, "decode", step.decode)) {
            return false;
        }
        if (!step.decode.empty() && step.order.empty() && step.report.empty()) {
            return Fail(Join(p, "decode"), "for an order or report step: what the group asks of the learner");
        }
        for (const TutorialStep& other : def.steps) {
            if (other.id == step.id) return Fail(Join(p, "id"), "duplicate step '" + step.id + "'");
        }
        if (!Strings(s, p, "lines", step.lines)) return false;
        if (step.lines.empty()) return Fail(Join(p, "lines"), "required, at least one line");
        const int actions = (step.continue_key ? 1 : 0) + (step.control.empty() ? 0 : 1) + (step.wheel.empty() ? 0 : 1) +
                            (step.order.empty() ? 0 : 1) + (step.report.empty() ? 0 : 1);
        if (actions > 1) return Fail(p, "one action a step: continue, control, wheel, order or report");
        if (!step.control.empty() && step.position < 0) return Fail(Join(p, "position"), "a control step needs the position to set (>= 0)");
        if (step.control.empty() && s.Find("position") != nullptr) return Fail(Join(p, "position"), "only with a control");
        std::string aim;
        if (!OptionalString(s, p, "aim", aim)) return false;
        if (!step.wheel.empty()) {
            if (aim.empty() || aim == "value") step.aim = TutorialAim::Value;
            else if (aim == "contact_azimuth") step.aim = TutorialAim::ContactAzimuth;
            else if (aim == "contact_range") step.aim = TutorialAim::ContactRange;
            else if (aim == "qc") step.aim = TutorialAim::Qc;
            else if (aim == "marker_azimuth") step.aim = TutorialAim::MarkerAzimuth;
            else if (aim == "marker_range") step.aim = TutorialAim::MarkerRange;
            else return Fail(Join(p, "aim"), "value, contact_azimuth, contact_range, qc, marker_azimuth or marker_range");
            if (step.aim == TutorialAim::Value && s.Find("target") == nullptr) return Fail(Join(p, "target"), "a wheel aimed at a value needs it");
            if (step.tolerance <= 0.0) {
                step.tolerance = step.aim == TutorialAim::ContactRange ? 600.0
                               : step.aim == TutorialAim::Qc          ? 0.5
                               : step.aim == TutorialAim::MarkerAzimuth ? 2.0
                               : step.aim == TutorialAim::MarkerRange ? 0.02
                                                                      : 1.0;
            }
        } else if (!aim.empty() || s.Find("target") != nullptr || s.Find("tolerance") != nullptr) {
            return Fail(p, "aim, target and tolerance are for a wheel step");
        }
        if (!step.procedure.empty() && step.control.empty()) return Fail(Join(p, "procedure"), "only with a control");
        if (step.hold && step.wheel.empty()) return Fail(Join(p, "hold"), "only with a wheel");
        std::vector<std::string> list;
        if (!Strings(s, p, "done_when", list)) return false;
        for (std::size_t c = 0; c < list.size(); ++c) {
            TutorialCondition cond;
            std::string why;
            if (!ParseTutorialCondition(list[c], cond, why)) return Fail(Index(Join(p, "done_when"), c), why);
            step.done_when.push_back(std::move(cond));
        }
        if (step.hold && step.done_when.empty()) return Fail(Join(p, "hold"), "a held wheel needs done_when: what ends the hold");
        if (step.conditions_only && (step.done_when.empty() || !step.HasAction())) {
            return Fail(Join(p, "conditions_only"), "for an action step with done_when: what finishes it");
        }
        list.clear();
        if (!Strings(s, p, "wait_until", list)) return false;
        for (std::size_t c = 0; c < list.size(); ++c) {
            TutorialCondition cond;
            std::string why;
            if (!ParseTutorialCondition(list[c], cond, why)) return Fail(Index(Join(p, "wait_until"), c), why);
            step.wait_until.push_back(std::move(cond));
        }
        list.clear();
        if (!Strings(s, p, "on_start", list)) return false;
        for (std::size_t c = 0; c < list.size(); ++c) {
            TutorialAction action;
            std::string why;
            if (!ParseTutorialAction(list[c], action, why)) return Fail(Index(Join(p, "on_start"), c), why);
            step.on_start.push_back(std::move(action));
        }
        return true;
    }

    bool Strings(const json::Value& obj, const std::string& path, const char* key, std::vector<std::string>& out) {
        const json::Value* arr = OptionalArray(obj, path, key);
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& v = arr->items[i];
            if (!v.Is(json::Type::String) || v.string.empty()) return Fail(Index(Join(path, key), i), "expected a non-empty string");
            out.push_back(v.string);
        }
        return true;
    }
};

// The console procedure's step for a move (control to position): the named procedure's, or the only one among the
// seat's procedures. Null if none (or, unnamed, more than one).
inline const ProcedureStep* FindProcedureStep(const ConsoleData& console, const std::string& procedure, const std::string& station,
                                              const std::string& control, int position, std::string* found_in = nullptr) {
    const ProcedureStep* match = nullptr;
    int matches = 0;
    for (const Procedure& p : console.procedures) {
        if (!procedure.empty() && p.id != procedure) continue;
        if (procedure.empty() && !station.empty() && p.station != station) continue;
        for (const ProcedureStep& s : p.steps) {
            if (s.control_id != control || s.value != position) continue;
            if (match == nullptr && found_in != nullptr) *found_in = p.id;
            if (match == nullptr) match = &s;
            ++matches;
            break;
        }
    }
    return procedure.empty() && matches > 1 ? nullptr : match;
}

}  // namespace detail

inline bool LoadTutorialJson(const std::string& json_text, TutorialDef& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    TutorialDef def;
    detail::TutorialLoader loader;
    if (!loader.Load(root, def)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(def);
    return true;
}

// A lesson from its file, relative to the data folder (e.g. "tutorials/basics.json").
inline bool LoadTutorialFile(const std::string& data_dir, const std::string& file, TutorialDef& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(data_dir + "/" + file, text)) {
        if (error != nullptr) *error = "cannot open " + file;
        return false;
    }
    std::string e;
    if (!LoadTutorialJson(text, out, &e)) {
        if (error != nullptr) *error = file + ": " + e;
        return false;
    }
    return true;
}

// Everything a lesson names is in the scenario it runs on: the seats in its console, the controls, positions and wheels
// (a strict lesson's at its seat), the procedures with those moves, the tasks, orders and callouts in its crew data, the
// inputs and subsystems in its equipment, the targets it releases. Fills each move's wait from its procedure.
inline bool BindTutorial(TutorialDef& def, const ScenarioDef& scenario, const ConsoleData& console, std::string* error = nullptr) {
    const CrewData* crew = scenario.has_crew_data ? &scenario.crew_data : nullptr;
    std::string where;
    auto fail = [&](const std::string& what) {
        if (error != nullptr) *error = def.id + where + ": " + what;
        return false;
    };
    if (!def.seat.empty() && console.FindStation(def.seat) == nullptr) return fail(": no seat '" + def.seat + "' in the console");
    auto check_condition = [&](const TutorialCondition& c) {
        switch (c.kind) {
            case TutorialCondition::Kind::Seat:
            case TutorialCondition::Kind::Holding:
            case TutorialCondition::Kind::Contact:
            case TutorialCondition::Kind::ContactIn:
                if (console.FindStation(c.arg) == nullptr) return fail(", '" + c.text + "': no such seat in the console");
                break;
            case TutorialCondition::Kind::Task:
                if (crew == nullptr || crew->FindTask(c.arg) == nullptr) return fail(", '" + c.text + "': no such task in the crew data");
                break;
            case TutorialCondition::Kind::Order:
                if (crew == nullptr || crew->FindOrder(c.arg) == nullptr) return fail(", '" + c.text + "': no such order in the crew data");
                break;
            case TutorialCondition::Kind::Heard:
                if (crew == nullptr || crew->FindCallout(c.arg) == nullptr) return fail(", '" + c.text + "': no such callout in the crew data");
                break;
            case TutorialCondition::Kind::Input:
                if (!console.equipment.HasInput(c.arg)) return fail(", '" + c.text + "': no such input in the console");
                break;
            case TutorialCondition::Kind::Subsystem:
                if (!console.equipment.Has(c.arg)) return fail(", '" + c.text + "': no such subsystem in the console");
                break;
            case TutorialCondition::Kind::Control: {
                const ControlDef* d = console.panel.Find(c.arg);
                if (d == nullptr) return fail(", '" + c.text + "': no such control in the console");
                if (d->kind == ControlKind::Continuous || c.number >= d->positions) return fail(", '" + c.text + "': no such position");
                break;
            }
            case TutorialCondition::Kind::Group:
                if (scenario.talk_tables.Decode(c.arg) == nullptr) return fail(", '" + c.text + "': no such group in the scenario's talk tables");
                break;
            case TutorialCondition::Kind::GivenTarget:
                if (scenario.c2_sources.empty()) return fail(", '" + c.text + "': the scenario has no battery net to give targets on (\"c2\")");
                break;
            default:
                break;
        }
        return true;
    };
    for (TutorialStep& s : def.steps) {
        where = ", step '" + s.id + "'";
        for (const TutorialCondition& c : s.done_when) {
            if (!check_condition(c)) return false;
        }
        for (const TutorialCondition& c : s.wait_until) {
            if (!check_condition(c)) return false;
        }
        for (const TutorialAction& a : s.on_start) {
            if (a.kind == TutorialAction::Kind::Order && (crew == nullptr || crew->FindOrder(a.arg) == nullptr)) {
                return fail(", '" + a.text + "': no such order in the crew data");
            }
            if ((a.kind == TutorialAction::Kind::NetOrder || a.kind == TutorialAction::Kind::Jam) && scenario.radio_data.Find(a.arg) == nullptr) {
                return fail(", '" + a.text + "': no net '" + a.arg + "' in the scenario's radio data");
            }
            if (a.kind == TutorialAction::Kind::LineCall && scenario.radio_data.Line(std::atoi(a.arg.c_str())) == nullptr) {
                return fail(", '" + a.text + "': no line " + a.arg + " in the scenario's radio data");
            }
            if (a.kind == TutorialAction::Kind::Release) {
                bool held = false;
                for (const ScenarioTarget& t : scenario.targets) held = held || (t.id == a.arg && t.on_cue);
                if (!held) return fail(", '" + a.text + "': no target '" + a.arg + "' held on cue in the scenario");
            }
            if (a.kind == TutorialAction::Kind::Regiment) {
                bool held = false;
                for (const RegimentCommand& c : scenario.post_regiment.commands) {
                    held = held || (c.on_cue && ScenarioRunner::RegimentCommandIs(c, a.arg));
                }
                if (!held) return fail(", '" + a.text + "': no regiment's command '" + a.arg + "' held on cue in the scenario");
            }
        }
        if (!s.decode.empty() && scenario.talk_tables.Decode(s.decode) == nullptr) {
            return fail(": decode '" + s.decode + "': no such group in the scenario's talk tables");
        }
        if (!s.order.empty() && (crew == nullptr || crew->FindOrder(s.order) == nullptr)) return fail(": no order '" + s.order + "' in the crew data");
        if (!s.report.empty()) {
            const CalloutDef* c = crew != nullptr ? crew->FindCallout(s.report) : nullptr;
            if (c == nullptr) return fail(": no callout '" + s.report + "' in the crew data");
            if (!def.seat.empty() && c->from != def.seat && c->from != "any") return fail(": '" + s.report + "' is the " + c->from + "'s report");
        }
        if (!s.control.empty()) {
            const ControlDef* d = console.panel.Find(s.control);
            if (d == nullptr) return fail(": no control '" + s.control + "' in the console");
            if (d->kind == ControlKind::Continuous) return fail(": '" + s.control + "' is a wheel: use \"wheel\"");
            if (s.position >= d->positions) return fail(": '" + s.control + "' has no position " + std::to_string(s.position));
            if (def.strict && !def.seat.empty() && !d->station.empty() && d->station != def.seat) {
                return fail(": '" + s.control + "' is the " + d->station + "'s, not at the lesson's seat");
            }
            if (!s.procedure.empty() && console.FindProcedure(s.procedure) == nullptr) return fail(": no procedure '" + s.procedure + "'");
            std::string found;
            const ProcedureStep* ps = detail::FindProcedureStep(console, s.procedure, d->station, s.control, s.position, &found);
            if (!s.procedure.empty() && ps == nullptr) {
                return fail(": procedure '" + s.procedure + "' has no step " + s.control + " to " + std::to_string(s.position));
            }
            if (ps != nullptr) {
                if (s.procedure.empty()) s.procedure = found;
                s.wait_for = ps->wait_for;
                s.min_wait_s = ps->min_wait_s;
                s.drill = ps->description;
            }
        }
        if (!s.wheel.empty()) {
            const ControlDef* d = console.panel.Find(s.wheel);
            if (d == nullptr || d->kind != ControlKind::Continuous) return fail(": no wheel '" + s.wheel + "' in the console");
            if (def.strict && !def.seat.empty() && !d->station.empty() && d->station != def.seat) {
                return fail(": '" + s.wheel + "' is the " + d->station + "'s, not at the lesson's seat");
            }
            if (s.aim == TutorialAim::Qc && !console.has_orientation) return fail(": aim qc needs the console's orientation");
        }
    }
    return true;
}

// The same checks against a running scenario (the game's own copy of the lesson is bound when it starts).
inline bool CheckTutorialAgainst(const TutorialDef& def, const ScenarioRunner& r, std::string* error = nullptr) {
    TutorialDef copy = def;
    return BindTutorial(copy, r.Def(), r.Console(), error);
}

// The learner's way through a lesson.
class TutorialGuide {
public:
    TutorialGuide() = default;
    explicit TutorialGuide(TutorialDef def) : def_(std::move(def)) {}

    // Binds the lesson to the scenario it runs on (each move's wait from its procedure). False with the reason if it
    // names what the scenario hasn't got.
    bool Bind(const ScenarioRunner& r, std::string* error = nullptr) { return BindTutorial(def_, r.Def(), r.Console(), error); }

    const TutorialDef& Def() const { return def_; }
    std::size_t Count() const { return def_.steps.size(); }
    // The step shown now (its index; Count() once past the last, which only Skip does).
    std::size_t Index() const { return at_; }
    const TutorialStep* Current() const { return at_ < def_.steps.size() ? &def_.steps[at_] : nullptr; }
    // On the last step (or past it).
    bool AtEnd() const { return at_ + 1 >= def_.steps.size(); }
    // Sim time each finished step was finished at.
    const std::vector<double>& FinishedAt() const { return finished_at_; }
    // The chapter the current step belongs to.
    std::string Chapter() const {
        for (std::size_t i = at_ + 1; i-- > 0;) {
            if (i < def_.steps.size() && !def_.steps[i].chapter.empty()) return def_.steps[i].chapter;
        }
        return std::string();
    }

    // Whether one condition holds now for the current step.
    bool Holds(const TutorialCondition& c, const ScenarioRunner& r, const TutorialUi& ui) const {
        switch (c.kind) {
            case TutorialCondition::Kind::View: return ui.view == c.arg;
            case TutorialCondition::Kind::Views: return static_cast<double>(ui.views_used.size()) >= c.number;
            case TutorialCondition::Kind::Cabin: return ui.in_cabin;
            case TutorialCondition::Kind::Seat: return ui.in_cabin && ui.seat == c.arg;
            case TutorialCondition::Kind::Holding: return ui.holding == c.arg || r.SeatHolder(c.arg) == player_;
            case TutorialCondition::Kind::Task: {
                if (!r.Def().has_crew_data) return false;
                const CrewTask* t = r.Def().crew_data.FindTask(c.arg);
                return t != nullptr && r.Console().equipment.AllHold(t->conditions, r.Console().panel);
            }
            case TutorialCondition::Kind::Order:
                for (const ScenarioRunner::GivenOrder& o : r.OrdersGiven()) {
                    if (o.order == c.arg && o.time_s >= step_sim_s_) return true;
                }
                return false;
            case TutorialCondition::Kind::Heard:
                if (r.Callouts() == nullptr) return false;
                for (const SpokenCallout& s : r.Callouts()->Log()) {
                    if (s.id == c.arg && s.queued_s >= step_sim_s_) return true;
                }
                return false;
            case TutorialCondition::Kind::Debrief:
                for (const DebriefEntry& e : r.Log().Entries()) {
                    if (e.time_s >= step_sim_s_ && c.arg == ToString(e.kind)) return true;
                }
                return false;
            case TutorialCondition::Kind::Input: return r.Console().equipment.GetInput(c.arg);
            case TutorialCondition::Kind::Subsystem: {
                SubsystemState want = SubsystemState::Ready;
                detail::ParseSubsystemState(c.value, want);
                return r.Console().equipment.State(c.arg) == want;
            }
            case TutorialCondition::Kind::Control: return r.Console().panel.Get(c.arg) == static_cast<int>(c.number);
            case TutorialCondition::Kind::Contact:
            case TutorialCondition::Kind::ContactIn: {
                const KnownTarget* k = NearestContact(r, c.arg, false, false);
                if (k == nullptr) return false;
                return c.kind == TutorialCondition::Kind::Contact || (k->has_range && k->range_m <= c.number);
            }
            case TutorialCondition::Kind::Moved: {
                const Vec3 d = r.UnitPosition() - step_unit_;
                return std::sqrt(d.x * d.x + d.y * d.y) >= c.number;
            }
            case TutorialCondition::Kind::Ended: return r.Finished();
            case TutorialCondition::Kind::Time: return r.Time() >= c.number;
            case TutorialCondition::Kind::After: return ui.real_time_s - step_real_s_ >= c.number;
            case TutorialCondition::Kind::Ui: return ui.events.count(c.arg) != 0;
            case TutorialCondition::Kind::Marked: return r.MarkedTarget() >= 0;
            case TutorialCondition::Kind::Designated: return r.DesignationsMade() > step_designations_;
            case TutorialCondition::Kind::Plotted: return r.PlotsMade() > step_plots_;
            case TutorialCondition::Kind::GivenTarget: return r.DesignationsHeard() > step_given_;
            case TutorialCondition::Kind::Group:
                for (const ScenarioRunner::GroupHeard& g : r.GroupsHeard()) {
                    if (g.code != c.arg || g.time_s < step_sim_s_) continue;
                    if (def_.seat.empty()) return true;
                    for (const std::string& e : g.ears) {
                        if (e == def_.seat) return true;
                    }
                }
                return false;
        }
        return false;
    }

    // How long a "decode" step leaves the learner to read the group himself, once its wait is over, before the card names
    // the answer (sam3d: time to find the table and the group in it; the hints that grow, docs/TUTORIALS.md rule 9).
    static constexpr double kDecodeRevealS = 45.0;
    // Is the current step's answer shown (no group to decode, the instructor doing it, gone back to, or the time up)?
    bool Revealed(const TutorialUi& ui) const {
        const TutorialStep* s = Current();
        return s == nullptr || s->decode.empty() || auto_ || Reviewing() ||
               (act_real_s_ >= 0.0 && ui.real_time_s - act_real_s_ >= kDecodeRevealS);
    }

    // The step as the front end shows it now.
    TutorialStatus Status(const ScenarioRunner& r, const TutorialUi& ui) const {
        TutorialStatus st;
        const TutorialStep* s = Current();
        if (s == nullptr) return st;
        st.index = at_;
        st.count = def_.steps.size();
        st.chapter = Chapter();
        st.fast_forward = s->fast_forward;
        st.feedback = feedback_;
        if (!s->control.empty()) {
            const ControlDef* d = r.Console().panel.Find(s->control);
            st.control = s->control;
            st.position = s->position;
            st.current_position = r.Console().panel.Get(s->control);
            if (d != nullptr) {
                st.control_label = d->label;
                st.station = d->station;
                if (s->position >= 0 && s->position < static_cast<int>(d->position_labels.size())) {
                    st.position_label = d->position_labels[static_cast<std::size_t>(s->position)];
                } else if (d->kind == ControlKind::Momentary) {
                    st.position_label = "press";  // a push-button has no positions to name
                }
            }
        }
        if (!s->wheel.empty()) {
            const ControlDef* d = r.Console().panel.Find(s->wheel);
            st.control = s->wheel;
            st.wheel = true;
            st.value = r.Console().panel.GetValue(s->wheel);
            st.tolerance = s->tolerance;
            st.wraps = !TutorialAimLinear(s->aim);
            st.target_known = AimTarget(*s, r, st.target);
            if (st.target_known) {
                const double diff = st.wraps ? detail::TutorialWrap180(st.value - st.target) : st.value - st.target;
                st.on_mark = std::fabs(diff) <= st.tolerance;
            }
            if (d != nullptr) {
                st.control_label = d->label;
                st.station = d->station;
            }
        }
        if (!s->order.empty() || !s->report.empty()) {
            st.order = s->order.empty() ? s->report : s->order;
            st.report = !s->report.empty();
            const CalloutDef* words = r.Def().has_crew_data ? r.Def().crew_data.FindCallout(st.order) : nullptr;
            if (words != nullptr) {
                st.order_ru = words->ru;
                st.order_en = words->en;
            }
            if (words != nullptr && st.report && !def_.seat.empty()) {  // his words: the report he reads out, his seat's number
                const std::pair<std::string, std::string> w = r.ReportWords(def_.seat, st.order);
                st.order_ru = w.first;
                st.order_en = w.second;
            }
            if (!Revealed(ui)) {  // he reads the group himself: nothing on the card or in the menu gives it away
                st.decode = s->decode;
                st.order.clear();
                st.order_ru.clear();
                st.order_en.clear();
            }
        }
        st.instructor = auto_;
        if (s->continue_key || Reviewing()) {
            st.phase = TutorialStatus::Phase::Read;
            st.review = Reviewing();
            return st;
        }
        if (!s->HasAction()) {
            bool yours = false;
            for (const TutorialCondition& c : s->done_when) yours = yours || TutorialLearnerCondition(c);
            st.phase = s->done_when.empty() ? TutorialStatus::Phase::End
                       : yours             ? TutorialStatus::Phase::Act
                                           : TutorialStatus::Phase::Watch;
            st.wait_text = s->wait_text;
            return st;
        }
        const std::string waiting = WaitingFor(*s, r, ui);
        if (!waiting.empty()) {
            st.phase = TutorialStatus::Phase::Wait;
            st.wait_text = s->wait_text.empty() ? waiting : s->wait_text;
            return st;
        }
        if (!st.wheel || st.target_known) {
            st.phase = TutorialStatus::Phase::Act;
        } else {
            st.phase = TutorialStatus::Phase::Wait;
            st.wait_text = s->wait_text.empty() ? std::string("a mark to bring the wheel onto") : s->wait_text;
        }
        return st;
    }

    // In a strict lesson, why the learner may not work this control now ("" allowed). Any control is allowed in a free
    // lesson.
    std::string Refusal(const ScenarioRunner& r, const std::string& control_id, const TutorialUi& ui) const {
        if (!def_.strict) return std::string();
        const TutorialStep* s = Current();
        const ControlDef* d = r.Console().panel.Find(control_id);
        const std::string name = d != nullptr ? d->label : control_id;
        if (s == nullptr) return std::string();
        const std::string& want = !s->control.empty() ? s->control : s->wheel;
        if (want.empty()) {
            return s->continue_key ? "Not yet: read the step, then Enter" : "Not now: this step needs nothing from the panels";
        }
        if (want != control_id) {
            const ControlDef* w = r.Console().panel.Find(want);
            return "Not that one (" + name + "): the highlighted one is " + (w != nullptr ? w->label : want);
        }
        const std::string waiting = WaitingFor(*s, r, ui);
        if (!waiting.empty()) return "Not yet: waiting for " + (s->wait_text.empty() ? waiting : s->wait_text);
        return std::string();
    }

    // In a strict lesson, why the learner may not give this order or make this report from the crew menu now ("" allowed).
    std::string MenuRefusal(const ScenarioRunner& r, const std::string& callout_id, const TutorialUi& ui) const {
        if (!def_.strict) return std::string();
        const TutorialStep* s = Current();
        if (s == nullptr) return std::string();
        const std::string& want = s->order.empty() ? s->report : s->order;
        if (want.empty()) return "Not now: the lesson isn't asking for that yet";
        if (want != callout_id && !Revealed(ui)) {
            const TalkTable* t = nullptr;
            r.GetTalkTables().Decode(s->decode, &t);
            return "Not what " + s->decode + " means: read it in " + (t != nullptr ? t->name_ru : std::string("the talk table")) +
                   " on your desk (L)";
        }
        if (want != callout_id) {
            const CalloutDef* words = r.Def().has_crew_data ? r.Def().crew_data.FindCallout(want) : nullptr;
            return std::string("Not that one: the lesson asks for ") + (words != nullptr ? words->ru + " (" + words->en + ")" : want);
        }
        const std::string waiting = WaitingFor(*s, r, ui);
        if (!waiting.empty()) return "Not yet: waiting for " + (s->wait_text.empty() ? waiting : s->wait_text);
        return std::string();
    }

    // Each update: runs the step's start actions once, and the current step is done when its action is done or any of
    // its conditions holds; then the next comes up (one step per update, so each is seen). True if the step changed.
    bool Update(ScenarioRunner& r, const TutorialUi& ui) {
        if (!started_) {
            started_ = true;
            BeginStep(r, ui);
        }
        const TutorialStep* s = Current();
        if (s == nullptr) return false;
        UpdateFeedback(*s, r);
        if (act_real_s_ < 0.0 && WaitingFor(*s, r, ui).empty()) act_real_s_ = ui.real_time_s;  // his move from now
        if (auto_) Perform(*s, r, ui);
        if (!Done(*s, r, ui)) return false;
        Next(r, ui);
        return true;
    }

    // The learner skips the step: one with nothing to do (reading, watching, a front-end step) is passed; otherwise the
    // instructor does it, now or as soon as its wait allows (Update goes on doing it: a held wheel follows its mark).
    void Skip(ScenarioRunner& r, const TutorialUi& ui) {
        const TutorialStep* s = Current();
        if (s == nullptr || AtEnd()) return;
        const bool action = !s->control.empty() || !s->wheel.empty() || !s->order.empty() || !s->report.empty() || PlotStep(*s);
        if (!action || Reviewing()) {
            Next(r, ui);
            return;
        }
        auto_ = true;
        Perform(*s, r, ui);
    }

    // On to the next step without doing this one, nor having it done (tests, and checking a lesson as it is written).
    void Pass(ScenarioRunner& r, const TutorialUi& ui) {
        if (at_ < def_.steps.size()) Next(r, ui);
    }

    // Back to the step before, to read again (false at the first).
    bool Back(ScenarioRunner& r, const TutorialUi& ui) {
        if (at_ == 0) return false;
        --at_;
        if (!finished_at_.empty()) finished_at_.pop_back();
        BeginStep(r, ui);
        return true;
    }

    // The step came up again by Back (not the furthest the learner has got).
    bool Reviewing() const { return at_ + 1 < begun_; }

    // The name a player holds seats under (ScenarioRunner::TakeSeat), for holding:<seat>.
    void SetPlayerName(std::string name) { player_ = std::move(name); }

    // The value a wheel step's wheel should be brought onto; false while there is none (no contact yet).
    bool AimTarget(const TutorialStep& s, const ScenarioRunner& r, double& out) const {
        switch (s.aim) {
            case TutorialAim::None: return false;
            case TutorialAim::Value: out = s.target; return true;
            case TutorialAim::ContactAzimuth: {
                const KnownTarget* k = NearestContact(r, Seat(s, r), true, false);
                if (k == nullptr) return false;
                out = k->azimuth_deg;
                return true;
            }
            case TutorialAim::ContactRange: {
                const KnownTarget* k = NearestContact(r, Seat(s, r), false, true);
                if (k == nullptr) return false;
                out = k->range_m;
                return true;
            }
            case TutorialAim::MarkerAzimuth:
            case TutorialAim::MarkerRange: {
                // The target nearest the post on the IT-45 (x east, y north, fractions of the scale).
                const std::vector<CommandPost::ScopeMark> scope = r.PostScope();
                const CommandPost::ScopeMark* best = nullptr;
                for (const CommandPost::ScopeMark& m : scope) {
                    if (best == nullptr || m.x * m.x + m.y * m.y < best->x * best->x + best->y * best->y) best = &m;
                }
                if (best == nullptr) return false;
                if (s.aim == TutorialAim::MarkerRange) {
                    out = std::sqrt(best->x * best->x + best->y * best->y);
                } else {
                    out = std::atan2(best->x, best->y) * 180.0 / kPi;
                    if (out < 0.0) out += 360.0;
                }
                return true;
            }
            case TutorialAim::Qc: {
                // The crew chief's measured heading, given when the setter was connected (the log's orientation line).
                const std::vector<DebriefEntry>& log = r.Log().Entries();
                for (auto it = log.rbegin(); it != log.rend(); ++it) {
                    if (it->what != "orientation") continue;
                    const std::size_t at = it->detail.find("Qc = ");
                    if (at == std::string::npos) continue;
                    out = std::strtod(it->detail.c_str() + at + 5, nullptr);
                    return true;
                }
                return false;
            }
        }
        return false;
    }

private:
    std::string Seat(const TutorialStep& s, const ScenarioRunner& r) const {
        if (!def_.seat.empty()) return def_.seat;
        const ControlDef* d = r.Console().panel.Find(s.wheel);
        return d != nullptr ? d->station : std::string();
    }

    // The nearest contact a seat knows of that isn't a friend, seen or heard in the last 30 s (with an azimuth in the
    // crew's frame, or a range, when asked).
    const KnownTarget* NearestContact(const ScenarioRunner& r, const std::string& station, bool need_azimuth, bool need_range) const {
        const CrewPicture* p = r.PictureOf(station);
        if (p == nullptr) return nullptr;
        nearest_ = p->Recent(r.Time(), 30.0);
        const KnownTarget* best = nullptr;
        for (const KnownTarget& k : nearest_) {
            if (k.friendly) continue;
            if (need_azimuth && (!k.has_azimuth || k.true_azimuth)) continue;
            if (need_range && !k.has_range) continue;
            if (!k.has_azimuth && !k.has_range) continue;
            if (best == nullptr || (k.has_range && (!best->has_range || k.range_m < best->range_m))) best = &k;
        }
        return best;
    }

    // What the action still waits for, in words ("" ready).
    std::string WaitingFor(const TutorialStep& s, const ScenarioRunner& r, const TutorialUi& ui) const {
        std::string out;
        auto add = [&out](const std::string& what) {
            if (!what.empty()) out += (out.empty() ? "" : "; ") + what;
        };
        const ConsoleData& c = r.Console();
        for (const Condition& w : s.wait_for) {
            if (c.equipment.Holds(w, c.panel)) continue;
            switch (w.kind) {
                case Condition::Kind::SubsystemIs: {
                    const SubsystemDef* d = c.equipment.Def(w.id);
                    const std::string name = d != nullptr && !d->name.empty() ? d->name : w.id;
                    add(w.state == SubsystemState::Ready ? name : name + (w.state == SubsystemState::Off ? " off" : ""));
                    break;
                }
                case Condition::Kind::InputIs: {
                    const CrewFact* f = r.Def().has_crew_data ? r.Def().crew_data.FindFact(w.id) : nullptr;
                    add(f != nullptr && !f->unmet_en.empty() ? f->unmet_en : w.id + (w.input_value ? "" : " off"));
                    break;
                }
                case Condition::Kind::ControlEquals:
                case Condition::Kind::ControlAtLeast: {
                    const ControlDef* d = c.panel.Find(w.id);
                    const std::string label = d != nullptr ? d->label : w.id;
                    const std::string pos = d != nullptr && w.position >= 0 && w.position < static_cast<int>(d->position_labels.size())
                                                ? d->position_labels[static_cast<std::size_t>(w.position)]
                                                : std::to_string(w.position);
                    add(label + " at " + pos);
                    break;
                }
            }
        }
        for (const TutorialCondition& w : s.wait_until) {
            if (!Holds(w, r, ui)) add(Describe(w, r));
        }
        if (out.empty() && s.min_wait_s > 0.0 && r.Time() - step_sim_s_ < s.min_wait_s) {
            char line[64];
            std::snprintf(line, sizeof(line), "%.0f s more", std::ceil(s.min_wait_s - (r.Time() - step_sim_s_)));
            add(line);
        }
        return out;
    }

    std::string Describe(const TutorialCondition& w, const ScenarioRunner& r) const {
        auto seat = [&r](const std::string& id) {
            const ConsoleStation* st = r.Console().FindStation(id);
            return st != nullptr ? st->label : id;
        };
        switch (w.kind) {
            case TutorialCondition::Kind::Contact: return "a target to show on the " + seat(w.arg) + "'s picture";
            case TutorialCondition::Kind::ContactIn: {
                char line[96];
                std::snprintf(line, sizeof(line), "the target to close to %.0f km", w.number / 1000.0);
                return line;
            }
            case TutorialCondition::Kind::Task: {
                const CrewTask* t = r.Def().has_crew_data ? r.Def().crew_data.FindTask(w.arg) : nullptr;
                return t != nullptr && !t->name.empty() ? t->name : w.arg;
            }
            case TutorialCondition::Kind::Subsystem: {
                const SubsystemDef* d = r.Console().equipment.Def(w.arg);
                return (d != nullptr && !d->name.empty() ? d->name : w.arg) + (w.value == "ready" ? "" : " (" + w.value + ")");
            }
            case TutorialCondition::Kind::Input: {
                const CrewFact* f = r.Def().has_crew_data ? r.Def().crew_data.FindFact(w.arg) : nullptr;
                return f != nullptr && !f->unmet_en.empty() ? f->unmet_en : w.arg;
            }
            case TutorialCondition::Kind::Group: return "group " + w.arg + " on the net";
            case TutorialCondition::Kind::GivenTarget: return "a target from the battery commander on the battery net";
            default: return w.text;
        }
    }

    bool ActionDone(const TutorialStep& s, const ScenarioRunner& r, const TutorialUi& ui) const {
        if (s.continue_key) return ui.continues > step_continues_;
        if (!s.order.empty()) {
            for (const ScenarioRunner::GivenOrder& o : r.OrdersGiven()) {
                if (o.order == s.order && o.time_s >= step_sim_s_) return true;
            }
            return false;
        }
        if (!s.report.empty()) {
            // When he makes it, as an order when it is given: a busy intercom says it later.
            for (const ScenarioRunner::MadeReport& m : r.ReportsMade()) {
                if (m.time_s >= step_sim_s_ && m.callout == s.report && (def_.seat.empty() || m.station == def_.seat)) return true;
            }
            return false;
        }
        if (!s.control.empty()) {
            // The learner moved it there during the step (a button springs back at once; a move can end its own wait:
            // the gyro unslaved by the very switch the step asked for).
            for (std::size_t i = actions_seen_; i < r.SeatActions().size(); ++i) {
                const ScenarioRunner::SeatAction& a = r.SeatActions()[i];
                if (a.control == s.control && !a.wheel && a.position == s.position && a.moved) return true;
            }
            // Or it is there (by any hand) with its wait met.
            const ControlDef* d = r.Console().panel.Find(s.control);
            if (d != nullptr && d->kind == ControlKind::Momentary) return false;
            return WaitingFor(s, r, ui).empty() && r.Console().panel.Get(s.control) == s.position;
        }
        if (!WaitingFor(s, r, ui).empty()) return false;
        if (!s.wheel.empty()) {
            if (s.hold) return false;  // held until done_when
            double target = 0.0;
            if (!AimTarget(s, r, target)) return false;
            const double v = r.Console().panel.GetValue(s.wheel);
            const double diff = TutorialAimLinear(s.aim) ? v - target
                                                                                                  : detail::TutorialWrap180(v - target);
            return std::fabs(diff) <= s.tolerance;
        }
        return false;
    }

    bool Done(const TutorialStep& s, const ScenarioRunner& r, const TutorialUi& ui) const {
        // Gone back to: Enter, or the move made again (its state and conditions likely hold already).
        if (Reviewing()) {
            if (ui.continues > step_continues_) return true;
            for (std::size_t i = actions_seen_; i < r.SeatActions().size(); ++i) {
                const ScenarioRunner::SeatAction& a = r.SeatActions()[i];
                if (!s.control.empty() && a.control == s.control && !a.wheel && a.position == s.position && a.moved) return true;
            }
            return false;
        }
        if (s.HasAction() && !s.conditions_only && ActionDone(s, r, ui)) return true;
        for (const TutorialCondition& c : s.done_when) {
            if (Holds(c, r, ui)) return true;
        }
        return false;
    }

    // The learner's moves off the step since it began, in words.
    void UpdateFeedback(const TutorialStep& s, const ScenarioRunner& r) {
        const auto& actions = r.SeatActions();
        for (; feedback_seen_ < actions.size(); ++feedback_seen_) {
            const ScenarioRunner::SeatAction& a = actions[feedback_seen_];
            if (a.station != def_.seat && !def_.seat.empty()) continue;
            const bool on_step = (a.control == s.control && (a.wheel || a.position == s.position)) || a.control == s.wheel;
            if (on_step || !a.moved) continue;
            const ControlDef* d = r.Console().panel.Find(a.control);
            const std::string label = d != nullptr ? d->label : a.control;
            std::string pos;
            if (d != nullptr && !a.wheel && a.position >= 0 && a.position < static_cast<int>(d->position_labels.size())) {
                pos = " to " + d->position_labels[static_cast<std::size_t>(a.position)];
            }
            feedback_ = "That was " + label + pos + ": not this step's move";
        }
    }

    // The instructor's hand on a skipped step: one click (a selector a position at a time), the wheel onto its mark, the
    // order or the report; nothing while its wait lasts.
    // A step done when the learner writes a mark on the plotting board.
    static bool PlotStep(const TutorialStep& s) {
        for (const TutorialCondition& c : s.done_when) {
            if (c.kind == TutorialCondition::Kind::Plotted) return true;
        }
        return false;
    }
    void Perform(const TutorialStep& s, ScenarioRunner& r, const TutorialUi& ui) {
        if (!WaitingFor(s, r, ui).empty() || r.Time() < next_perform_s_) return;
        next_perform_s_ = r.Time() + 0.5;
        if (PlotStep(s)) {  // the oldest report heard, written where it puts the target (exact at any scale: no miss)
            if (r.Post() != nullptr && !r.Post()->Heard().empty()) {
                const CommandPost::AirReport& h = r.Post()->Heard().front();
                r.PlayerPlotAt(h.number, h.contact.position.x, h.contact.position.y, 200000.0 / 0.17);
            }
            next_perform_s_ = r.Time() + 1.0;
            return;
        }
        if (!s.control.empty()) {
            const ControlDef* d = r.Console().panel.Find(s.control);
            if (d == nullptr) return;
            const int now = r.Console().panel.Get(s.control);
            int to = s.position;
            if (d->kind == ControlKind::Momentary) {
                to = 1;
            } else if (now == s.position) {
                return;
            } else if (d->kind == ControlKind::Discrete && d->position_labels.size() > 2) {
                to = now + (s.position > now ? 1 : -1);
            }
            const std::string seat = def_.seat.empty() ? d->station : def_.seat;
            if (r.SeatHolder(seat).empty()) {
                r.PlayerSet(s.control, to);
            } else {
                r.SeatSet(seat, s.control, to);
            }
            return;
        }
        if (!s.wheel.empty()) {
            double target = 0.0;
            if (!AimTarget(s, r, target)) return;
            next_perform_s_ = r.Time();  // a moving mark: kept on it every update
            const std::string seat = Seat(s, r);
            if (r.SeatHolder(seat).empty()) {
                r.Console().panel.SetValue(s.wheel, target);
            } else {
                r.SeatTurn(seat, s.wheel, target);
            }
            return;
        }
        if (!s.order.empty()) {
            r.TryCrewOrder(s.order);
            next_perform_s_ = r.Time() + 2.0;
            return;
        }
        if (!s.report.empty()) {
            // Once; a report refused (nothing to read back yet, the set not on the net) again a little later.
            next_perform_s_ = r.PlayerSay(def_.seat, s.report) ? r.Time() + 1e9 : r.Time() + 2.0;
        }
    }

    void BeginStep(ScenarioRunner& r, const TutorialUi& ui) {
        auto_ = false;
        next_perform_s_ = 0.0;
        step_sim_s_ = r.Time();
        step_real_s_ = ui.real_time_s;
        act_real_s_ = -1.0;
        step_continues_ = ui.continues;
        step_unit_ = r.UnitPosition();
        step_designations_ = r.DesignationsMade();
        step_plots_ = r.PlotsMade();
        step_given_ = r.DesignationsHeard();
        actions_seen_ = r.SeatActions().size();
        feedback_seen_ = actions_seen_;
        feedback_.clear();
        const TutorialStep* s = Current();
        if (s == nullptr) return;
        // Start actions once: a step gone back to doesn't give its orders again.
        if (at_ < begun_) return;
        begun_ = at_ + 1;
        for (const TutorialAction& a : s->on_start) {
            if (a.kind == TutorialAction::Kind::Order) r.GiveCrewOrder(a.arg);
            if (a.kind == TutorialAction::Kind::Release) r.ReleaseTarget(a.arg);
            if (a.kind == TutorialAction::Kind::NetOrder) r.ExternalNetOrder(a.arg);
            if (a.kind == TutorialAction::Kind::Jam) r.StartJamming(a.arg);
            if (a.kind == TutorialAction::Kind::LineCall) r.RingLine(std::atoi(a.arg.c_str()));
            if (a.kind == TutorialAction::Kind::Regiment) r.ReleaseRegimentCommand(a.arg);
        }
    }

    void Next(ScenarioRunner& r, const TutorialUi& ui) {
        finished_at_.push_back(r.Time());
        ++at_;
        BeginStep(r, ui);
    }

    TutorialDef def_;
    std::size_t at_ = 0;
    std::size_t begun_ = 0;      // steps begun so far (the furthest + 1)
    bool auto_ = false;          // the instructor does this step (Skip)
    double next_perform_s_ = 0.0;
    bool started_ = false;
    double step_sim_s_ = 0.0;
    double step_real_s_ = 0.0;
    double act_real_s_ = -1.0;   // when the step's action could first be done (its wait over); -1 not yet
    int step_continues_ = 0;
    Vec3 step_unit_;
    int step_designations_ = 0;
    int step_plots_ = 0;
    int step_given_ = 0;
    std::size_t actions_seen_ = 0;
    std::size_t feedback_seen_ = 0;
    std::string feedback_;
    std::vector<double> finished_at_;
    std::string player_ = "Player";
    mutable std::vector<KnownTarget> nearest_;  // NearestContact's copy of the picture it points into
};

}  // namespace simcore
