// simcore/campaign/regiment.hpp — the regiment's tasking (roadmap 14.2, first form; docs/campaign/03_THE_BATTERY_MISSION.md
// §1, 05_THE_SYSTEMS.md §3).
//
// A battery changes position only on the regiment commander's order (voenizdat_osa_1991 pp. 232-239). The tasking is a
// rule table, not a strategy game: each rule has a trigger the regiment can know, a task, and the reason written into
// the order (data/campaign/tasking.json). The regiment decides on what reached it: our intelligence's estimates as
// they stand in the campaign's journal (its visible "own_intel" entries), never the enemy's mind. Routine rules are
// read at the evening briefing, urgent ones at once.
//
// First form: two rules. A battery without a task is given the division's priority object to cover (a task file); a
// battery the regiment's estimate says the enemy probably knows, made after it reached its position, is moved to its
// reserve position (a relocate task made from the task it holds). Not yet: the regiment's officers bending the rules,
// the other triggers of 03 §1 (the covered troops moving, a corridor named, ARM attacks, low stocks, the enemy's phase,
// a battery lost) and the regiment's five batteries.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/campaign/battery_task.hpp"
#include "simcore/console_data.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace campaign {

struct TaskingRule {
    std::string id;
    std::string when;        // "no_task", "enemy_knows_battery"
    std::string task;        // "task_file" (the file's task), "relocate" (to the reserve position)
    std::string task_file;   // with task_file: relative to the data folder
    bool urgent = false;     // at once; else at the evening briefing
    double ready_in_s = 0.0; // relocate: ready this long after the order
    std::string reason;      // into the order's reasons, first
};

struct TaskingRules {
    double briefing_hour = 19.0;  // the regiment's evening briefing, local time
    std::vector<TaskingRule> rules;
};

namespace detail {

class TaskingLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, TaskingRules& out) {
        if (!Keys(root, "", {"tasking_id", "status", "briefing_hour", "rules", "source", "confidence", "notes"}) || !Provenance(root, "") ||
            !OptionalNumber(root, "", "briefing_hour", out.briefing_hour)) {
            return false;
        }
        if (out.briefing_hour < 0.0 || out.briefing_hour >= 24.0) return Fail("briefing_hour", "a local hour, 0 to 24");
        const json::Value* rules = OptionalArray(root, "", "rules");
        if (rules == nullptr || rules->items.empty()) return Fail("rules", "required: at least one rule");
        for (std::size_t i = 0; i < rules->items.size(); ++i) {
            const json::Value& r = rules->items[i];
            const std::string p = Index("rules", i);
            TaskingRule t;
            if (!r.Is(json::Type::Object) || !Keys(r, p, {"id", "when", "task", "task_file", "urgent", "ready_in_s", "reason", "source",
                                                         "confidence", "notes"}) ||
                !Provenance(r, p) || !RequireString(r, p, "id", t.id) || !RequireString(r, p, "when", t.when) ||
                !RequireString(r, p, "task", t.task) || !OptionalString(r, p, "task_file", t.task_file) ||
                !OptionalBool(r, p, "urgent", t.urgent) || !OptionalNumber(r, p, "ready_in_s", t.ready_in_s) ||
                !RequireString(r, p, "reason", t.reason)) {
                return false;
            }
            if (t.when != "no_task" && t.when != "enemy_knows_battery") return Fail(p + ".when", "no_task or enemy_knows_battery");
            if (t.task == "task_file") {
                if (t.task_file.empty()) return Fail(p + ".task_file", "required with \"task\": \"task_file\"");
            } else if (t.task == "relocate") {
                if (!(t.ready_in_s > 0.0)) return Fail(p + ".ready_in_s", "required with \"task\": \"relocate\", > 0");
            } else {
                return Fail(p + ".task", "task_file or relocate");
            }
            out.rules.push_back(t);
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadTaskingRules(const std::string& data_dir, const std::string& rel, TaskingRules& out, std::string* error = nullptr) {
    std::string text;
    json::Value root;
    std::string e;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text) || !json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "tasking: cannot read " + rel + (e.empty() ? "" : ": " + e);
        return false;
    }
    detail::TaskingLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "tasking: " + loader.Message();
        return false;
    }
    return true;
}

// What the regiment knows when it decides: our intelligence's latest estimate of what the enemy knows of the battery,
// as it stands in the campaign's journal.
struct RegimentPicture {
    double estimate_h = -1.0;         // the campaign hour of the latest estimate (< 0: none yet)
    bool enemy_knows_battery = false; // it says the enemy probably knows the battery's position
    std::string estimate;             // its words
};

inline RegimentPicture PictureFromJournal(const airwar::AirCampaign& c) {
    RegimentPicture p;
    for (const airwar::JournalEntry& j : c.Journal()) {
        if (!j.visible || j.kind != "own_intel" || j.text.find("the battery's position") == std::string::npos) continue;
        p.estimate_h = j.h;
        p.estimate = j.text;
        p.enemy_knows_battery = j.text.find("probably knows") != std::string::npos;
    }
    return p;
}

// The relocate task made from the task the battery holds: to its first reserve position, the main position its
// reserve now, the covered object and the sectors as they were. False, with why, without a reserve position.
inline bool RelocateTask(const BatteryTask& now, const TaskingRule& rule, const std::string& estimate, BatteryTask& out, std::string* why = nullptr) {
    if (now.reserve.empty()) {
        if (why != nullptr) *why = "the task gives no reserve position to move to";
        return false;
    }
    const std::string suffix = "_relocate", prefix = "move to the reserve position and go on with the task: ";
    out = now;
    const bool again = now.id.size() >= suffix.size() && now.id.compare(now.id.size() - suffix.size(), suffix.size(), suffix) == 0;
    out.id = again ? now.id : now.id + suffix;
    // An object's cover moves as a relocation (planned alike); troops', a crossing's or an ambush's keeps its own
    // formation round what it covers.
    if (now.kind == TaskKind::CoverObject || now.kind == TaskKind::CoverRebuild) out.kind = TaskKind::Relocate;
    out.main = now.reserve.front();
    out.reserve.erase(out.reserve.begin());
    out.reserve.push_back(now.main);
    if (now.tzm_place == out.main) out.tzm_place = now.main;  // the TZM waited at the reserve position: it still does
    out.task_text = now.task_text.compare(0, prefix.size(), prefix) == 0 ? now.task_text : prefix + now.task_text;
    out.short_ru = "сменить позицию, задача прежняя";
    out.area_ru.clear();  // its place's name
    out.reasons.clear();
    out.reasons.push_back(rule.reason + (estimate.empty() ? std::string() : " (" + estimate + ")"));
    out.ready_in_s = rule.ready_in_s;
    out.reconnoitre = false;  // the battery's own reserve position, prepared with its main one (pp. 232-239)
    out.lasts_s = -1.0;
    return true;
}

struct TaskingDecision {
    bool give = false;
    std::string rule;
    BatteryTask task;
    std::string why;  // when nothing is given: why not (for a debrief)
};

// The regiment's tasking for one battery: given what the regiment knows and the task the battery holds, the next task
// (if any). Each estimate moves the battery once; `arrived_h` is when the battery reached the position it holds (an
// estimate made before that is about where it was).
class RegimentTasking {
public:
    explicit RegimentTasking(TaskingRules rules, std::string data_dir) : rules_(std::move(rules)), data_dir_(std::move(data_dir)) {}

    TaskingDecision Decide(const RegimentPicture& pic, const BatteryTask* holds, double arrived_h, bool briefing) {
        TaskingDecision d;
        for (const TaskingRule& r : rules_.rules) {
            if (!r.urgent && !briefing) continue;
            if (r.when == "no_task") {
                if (holds != nullptr) continue;
                std::string e;
                if (!LoadBatteryTask(data_dir_, r.task_file, d.task, &e)) {
                    d.why = e;
                    continue;
                }
                d.task.reasons.insert(d.task.reasons.begin(), r.reason);
            } else if (r.when == "enemy_knows_battery") {
                if (holds == nullptr || !pic.enemy_knows_battery || pic.estimate_h < arrived_h || pic.estimate_h <= acted_on_h_) continue;
                if (!RelocateTask(*holds, r, pic.estimate, d.task, &d.why)) continue;
                acted_on_h_ = pic.estimate_h;
            }
            d.give = true;
            d.rule = r.id;
            return d;
        }
        return d;
    }
    const TaskingRules& Rules() const { return rules_; }

private:
    TaskingRules rules_;
    std::string data_dir_;
    double acted_on_h_ = -1.0;  // the estimate the regiment moved the battery on
};

}  // namespace campaign
}  // namespace simcore
