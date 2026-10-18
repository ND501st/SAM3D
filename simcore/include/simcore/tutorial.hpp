// simcore/tutorial.hpp — the tutorial guide (data/tutorials/*.json; the first playable test, docs/PLAYING.md).
//
// A guide is a list of steps, each a few lines telling the player what to do next and the conditions that finish it.
// The guide is a training aid laid over the game, like the training overlay (docs/crew/08_TRAINING_AND_ASSISTANCE.md):
// it never tells the crew anything and never acts. It reads the scenario (the crew's tasks, the orders spoken, the
// debrief, the console's inputs) and what the game's front end reports (the camera view, the cabin seat, the keys
// tried: TutorialUi), and moves to the next step when any of the current step's conditions holds.
//
// Conditions (strings in "done_when"; any one finishes the step):
//   view:F6          the camera is in that view now (F1-F12)
//   views:3          at least that many different views tried since the guide began
//   cabin            the camera is in the cabin
//   seat:commander   the camera is at that seat of the cabin
//   holding:commander  the player holds that seat
//   task:captured    a task of the crew data holds
//   order:order_k_boyu  that order (a crew data order's callout) has been spoken since the step began
//   debrief:HIT      a debrief line of that kind since the step began (LAUNCH, HIT, MISS, FAULT, DEVIATION)
//   input:target_in_zone  a console input is on
//   ended            the scenario has ended
//   time:300         sim time is at least that (s)
//   after:20         that many real seconds since the step began (a step that is only read)
//   ui:keys_card     the front end reports it happened (TutorialUi::events; kUiEvents lists them)
// A step without conditions is the last: it stays until the player leaves the mission.
#pragma once

#include <cstddef>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"
#include "simcore/scenario_runner.hpp"

namespace simcore {

struct TutorialCondition {
    enum class Kind { View, Views, Cabin, Seat, Holding, Task, Order, Debrief, Input, Ended, Time, After, Ui };
    Kind kind = Kind::Ended;
    std::string arg;
    double number = 0.0;
    std::string text;  // as written in the data
};

struct TutorialStep {
    std::string id;
    std::string title;
    std::vector<std::string> lines;  // what to do, a sentence or two each
    std::string hint;                // the keys for it, one line
    std::vector<TutorialCondition> done_when;
};

struct TutorialDef {
    std::string id;
    std::string title;
    std::string scenario;  // the scenario it is written for, relative to the data folder
    std::vector<TutorialStep> steps;
};

// What the game's front end tells the guide each update.
struct TutorialUi {
    std::string view;                  // the camera view now: "F1".."F12"
    std::set<std::string> views_used;  // every view tried since the guide began
    bool in_cabin = false;
    std::string seat;                  // the cabin seat the camera is at ("" outside)
    std::string holding;               // the seat the player holds ("" none)
    std::set<std::string> events;      // kUiEvents that have happened since the guide began
    double real_time_s = 0.0;          // real seconds since the guide began (paused time not counted)
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
    };
    return events;
}

namespace detail {

inline bool ParseTutorialCondition(const std::string& text, TutorialCondition& out, std::string& why) {
    out = TutorialCondition();
    out.text = text;
    const std::size_t colon = text.find(':');
    const std::string head = colon == std::string::npos ? text : text.substr(0, colon);
    const std::string arg = colon == std::string::npos ? std::string() : text.substr(colon + 1);
    auto number = [&](double& value) {
        char* end = nullptr;
        value = std::strtod(arg.c_str(), &end);
        if (arg.empty() || end == nullptr || *end != '\0' || value < 0.0) {
            why = "'" + text + "': expected a number >= 0 after the colon";
            return false;
        }
        return true;
    };
    auto needs_arg = [&]() {
        if (arg.empty()) why = "'" + text + "': needs a value after the colon";
        return !arg.empty();
    };
    out.arg = arg;
    if (head == "cabin" || head == "ended") {
        if (!arg.empty()) {
            why = "'" + text + "': takes no value";
            return false;
        }
        out.kind = head == "cabin" ? TutorialCondition::Kind::Cabin : TutorialCondition::Kind::Ended;
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
    if (head == "views") {
        out.kind = TutorialCondition::Kind::Views;
        return number(out.number);
    }
    if (head == "time" || head == "after") {
        out.kind = head == "time" ? TutorialCondition::Kind::Time : TutorialCondition::Kind::After;
        return number(out.number);
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
    if (head == "seat") out.kind = TutorialCondition::Kind::Seat;
    else if (head == "holding") out.kind = TutorialCondition::Kind::Holding;
    else if (head == "task") out.kind = TutorialCondition::Kind::Task;
    else if (head == "order") out.kind = TutorialCondition::Kind::Order;
    else if (head == "input") out.kind = TutorialCondition::Kind::Input;
    else {
        why = "'" + text + "': unknown condition (view, views, cabin, seat, holding, task, order, debrief, input, ended, time, after, ui)";
        return false;
    }
    return needs_arg();
}

class TutorialLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, TutorialDef& out) {
        if (!Keys(root, "", {"tutorial_id", "title", "scenario", "status", "source", "notes", "steps"}) ||
            !RequireString(root, "", "tutorial_id", out.id) || !RequireString(root, "", "title", out.title) ||
            !RequireString(root, "", "scenario", out.scenario)) {
            return false;
        }
        const json::Value* steps = root.Find("steps");
        if (steps == nullptr || !steps->Is(json::Type::Array) || steps->items.empty()) return Fail("steps", "required, a non-empty list");
        for (std::size_t i = 0; i < steps->items.size(); ++i) {
            const json::Value& s = steps->items[i];
            const std::string p = Index("steps", i);
            TutorialStep step;
            if (!Keys(s, p, {"id", "title", "lines", "hint", "done_when", "notes"}) || !RequireString(s, p, "id", step.id) ||
                !RequireString(s, p, "title", step.title) || !OptionalString(s, p, "hint", step.hint)) {
                return false;
            }
            for (const TutorialStep& other : out.steps) {
                if (other.id == step.id) return Fail(Join(p, "id"), "duplicate step '" + step.id + "'");
            }
            if (!Strings(s, p, "lines", step.lines)) return false;
            if (step.lines.empty()) return Fail(Join(p, "lines"), "required, at least one line");
            std::vector<std::string> conditions;
            if (!Strings(s, p, "done_when", conditions)) return false;
            for (std::size_t c = 0; c < conditions.size(); ++c) {
                TutorialCondition cond;
                std::string why;
                if (!ParseTutorialCondition(conditions[c], cond, why)) return Fail(Index(Join(p, "done_when"), c), why);
                step.done_when.push_back(std::move(cond));
            }
            if (step.done_when.empty() && i + 1 != steps->items.size()) {
                return Fail(Join(p, "done_when"), "only the last step may have no conditions");
            }
            out.steps.push_back(std::move(step));
        }
        return true;
    }

private:
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

// A guide from its file, relative to the data folder (e.g. "tutorials/first_shoot.json").
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

// Everything a guide names exists in the scenario it runs on: the seats in its console, the tasks and orders in its crew
// data, the inputs in its equipment.
inline bool CheckTutorialAgainst(const TutorialDef& def, const ScenarioRunner& r, std::string* error = nullptr) {
    auto fail = [&](const TutorialStep& s, const TutorialCondition& c, const std::string& what) {
        if (error != nullptr) *error = def.id + ", step '" + s.id + "', '" + c.text + "': " + what;
        return false;
    };
    const CrewData* crew = r.Def().has_crew_data ? &r.Def().crew_data : nullptr;
    for (const TutorialStep& s : def.steps) {
        for (const TutorialCondition& c : s.done_when) {
            switch (c.kind) {
                case TutorialCondition::Kind::Seat:
                case TutorialCondition::Kind::Holding:
                    if (r.Console().FindStation(c.arg) == nullptr) return fail(s, c, "no such seat in the scenario's console");
                    break;
                case TutorialCondition::Kind::Task:
                    if (crew == nullptr || crew->FindTask(c.arg) == nullptr) return fail(s, c, "no such task in the crew data");
                    break;
                case TutorialCondition::Kind::Order:
                    if (crew == nullptr || crew->FindOrder(c.arg) == nullptr) return fail(s, c, "no such order in the crew data");
                    break;
                case TutorialCondition::Kind::Input:
                    if (!r.Console().equipment.HasInput(c.arg)) return fail(s, c, "no such input in the console");
                    break;
                default:
                    break;
            }
        }
    }
    return true;
}

// The player's way through a guide.
class TutorialGuide {
public:
    TutorialGuide() = default;
    explicit TutorialGuide(TutorialDef def) : def_(std::move(def)) {}

    const TutorialDef& Def() const { return def_; }
    std::size_t Count() const { return def_.steps.size(); }
    // The step shown now (its index; Count() once past the last, which only Skip does).
    std::size_t Index() const { return at_; }
    const TutorialStep* Current() const { return at_ < def_.steps.size() ? &def_.steps[at_] : nullptr; }
    // On the last step (or past it).
    bool AtEnd() const { return at_ + 1 >= def_.steps.size(); }
    // Sim time each finished step was finished at.
    const std::vector<double>& FinishedAt() const { return finished_at_; }

    // Whether one condition holds now for the current step.
    bool Holds(const TutorialCondition& c, const ScenarioRunner& r, const TutorialUi& ui) const {
        switch (c.kind) {
            case TutorialCondition::Kind::View: return ui.view == c.arg;
            case TutorialCondition::Kind::Views: return static_cast<double>(ui.views_used.size()) >= c.number;
            case TutorialCondition::Kind::Cabin: return ui.in_cabin;
            case TutorialCondition::Kind::Seat: return ui.in_cabin && ui.seat == c.arg;
            case TutorialCondition::Kind::Holding: return ui.holding == c.arg || r.SeatHolder(c.arg) == ui_player_;
            case TutorialCondition::Kind::Task: {
                if (!r.Def().has_crew_data) return false;
                const CrewTask* t = r.Def().crew_data.FindTask(c.arg);
                return t != nullptr && r.Console().equipment.AllHold(t->conditions, r.Console().panel);
            }
            case TutorialCondition::Kind::Order:
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
            case TutorialCondition::Kind::Ended: return r.Finished();
            case TutorialCondition::Kind::Time: return r.Time() >= c.number;
            case TutorialCondition::Kind::After: return ui.real_time_s - step_real_s_ >= c.number;
            case TutorialCondition::Kind::Ui: return ui.events.count(c.arg) != 0;
        }
        return false;
    }

    // Each update: the current step is done when any of its conditions holds; then the next is shown (one step per
    // update, so each is seen). True if the step changed.
    bool Update(const ScenarioRunner& r, const TutorialUi& ui) {
        if (!started_) {
            started_ = true;
            step_sim_s_ = r.Time();
            step_real_s_ = ui.real_time_s;
        }
        const TutorialStep* s = Current();
        if (s == nullptr || s->done_when.empty()) return false;
        for (const TutorialCondition& c : s->done_when) {
            if (Holds(c, r, ui)) {
                Next(r.Time(), ui.real_time_s);
                return true;
            }
        }
        return false;
    }

    // The player moves on without doing the step (the guide's skip key).
    void Skip(double sim_time_s, double real_time_s) {
        if (at_ < def_.steps.size()) Next(sim_time_s, real_time_s);
    }

    // The name a player holds seats under (ScenarioRunner::TakeSeat), for holding:<seat>.
    void SetPlayerName(std::string name) { ui_player_ = std::move(name); }

private:
    void Next(double sim_time_s, double real_time_s) {
        finished_at_.push_back(sim_time_s);
        ++at_;
        step_sim_s_ = sim_time_s;
        step_real_s_ = real_time_s;
    }

    TutorialDef def_;
    std::size_t at_ = 0;
    bool started_ = false;
    double step_sim_s_ = 0.0;
    double step_real_s_ = 0.0;
    std::vector<double> finished_at_;
    std::string ui_player_ = "Player";
};

}  // namespace simcore
