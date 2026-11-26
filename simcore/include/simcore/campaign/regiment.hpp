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
// the other triggers of 03 §1 (a corridor named, ARM attacks, low stocks, the enemy's phase, a battery lost) and the
// regiment's five batteries.
//
// Track F, F4.4 (docs/fronts/04_AIR_DEFENCE_STRATEGY.md section 4): the regiment follows the ground war and our air
// defence strategy on what reached its posts through the pipeline (command/info_net.hpp) and nothing else
// (RegimentWatch): the covered division's withdrawal or relief (the ground feed's orders and reports, F3.8), the army's
// chief of air defence's priority for the division's sector and the national sector's sharing of the sky there (their
// orders, campaign/ad_strategy.hpp, F4.1 and F4.3). Four rules read it: the division withdraws, so the battery marches
// back with it to the nearest of its prepared positions a line further back (march_back); the sector raised to defend or
// fallen to economy, the task held with the readiness the rule gives (posture); the sky's sharing there changed, the
// battery's plan of cooperation told on the regiment's net (tell_sharing). Each acts once per fact, after its staff's
// work on it, and only on facts that reached the regiment after the session began (Since). Without the pipeline (the
// fronts' modes at 0) the picture's F4.4 fields stay empty and the tasking is 14.2's.
#pragma once

#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/campaign/ad_strategy.hpp"
#include "simcore/campaign/battery_task.hpp"
#include "simcore/command/air_war_feed.hpp"
#include "simcore/command/own_air.hpp"
#include "simcore/console_data.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace campaign {

struct TaskingRule {
    std::string id;
    // "no_task", "enemy_knows_battery"; F4.4: "division_withdraws", "sector_defend", "sector_economy", "sharing_changed"
    std::string when;
    // "task_file" (the file's task), "relocate" (to the reserve position); F4.4: "march_back" (with the division to a
    // prepared position further back), "posture" (the task held, its readiness and emission as the rule says),
    // "tell_sharing" (the sky's sharing told on the net: no new task)
    std::string task;
    std::string task_file;   // with task_file: relative to the data folder
    bool urgent = false;     // at once; else at the evening briefing
    double ready_in_s = 0.0; // relocate, march_back, posture: ready this long after the order
    std::string reason;      // into the order's reasons, first
    // F4.4
    std::vector<std::string> positions;  // march_back: the battery's prepared positions in depth (places), in no order
    double back_m = 0.0;                 // march_back: at least this much further behind the front's line than the main
    int readiness = 0;                   // posture: no. 1 or 2 (0: as held)
    std::string readiness_text;          // posture: its words ("": as held)
    std::string emission;                // posture: the emission regime by name ("": as held)
    double work_s = 0.0;                 // the regiment's staff's work from the fact reaching it to the order
    // Track O, O5: the battery's standing with the regiment (its day's mark at the summing up and its trust in the
    // battery's reports, grading.hpp). "trusted": the mark at least grade_at_least and the trust at least trust_at_least;
    // "distrusted": the mark at most grade_at_most or the trust below trust_below (each 0: not read). Task "fire_mode": the
    // task held with its launchers' fire mode set to `fire`.
    int grade_at_least = 0;
    int grade_at_most = 0;
    double trust_at_least = 0.0;
    double trust_below = 0.0;
    std::string fire;                    // fire_mode: "designation" or "independent"
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
            if (!r.Is(json::Type::Object) ||
                !Keys(r, p, {"id", "when", "task", "task_file", "urgent", "ready_in_s", "reason", "positions", "back_m", "readiness", "readiness_text",
                             "emission", "work_s", "grade_at_least", "grade_at_most", "trust_at_least", "trust_below", "fire", "source", "confidence",
                             "notes"}) ||
                !Provenance(r, p) || !RequireString(r, p, "id", t.id) || !RequireString(r, p, "when", t.when) ||
                !RequireString(r, p, "task", t.task) || !OptionalString(r, p, "task_file", t.task_file) ||
                !OptionalBool(r, p, "urgent", t.urgent) || !OptionalNumber(r, p, "ready_in_s", t.ready_in_s) ||
                !RequireString(r, p, "reason", t.reason) || !OptionalNumber(r, p, "back_m", t.back_m) || !OptionalInt(r, p, "readiness", t.readiness) ||
                !OptionalString(r, p, "readiness_text", t.readiness_text) || !OptionalString(r, p, "emission", t.emission) ||
                !OptionalNumber(r, p, "work_s", t.work_s) || !OptionalInt(r, p, "grade_at_least", t.grade_at_least) ||
                !OptionalInt(r, p, "grade_at_most", t.grade_at_most) || !OptionalNumber(r, p, "trust_at_least", t.trust_at_least) ||
                !OptionalNumber(r, p, "trust_below", t.trust_below) || !OptionalString(r, p, "fire", t.fire)) {
                return false;
            }
            const auto any = [](const std::string&) { return std::string(); };
            if (!StringList(r, p, "positions", t.positions, any)) return false;
            if (t.when != "no_task" && t.when != "enemy_knows_battery" && t.when != "division_withdraws" && t.when != "sector_defend" &&
                t.when != "sector_economy" && t.when != "sharing_changed" && t.when != "trusted" && t.when != "distrusted") {
                return Fail(p + ".when", "no_task, enemy_knows_battery, division_withdraws, sector_defend, sector_economy, sharing_changed, trusted or distrusted");
            }
            // O5: the standing's triggers take the fire mode, and the fire mode only them.
            if ((t.when == "trusted" || t.when == "distrusted") != (t.task == "fire_mode")) {
                return Fail(p + ".task", "\"fire_mode\" goes with \"when\": \"trusted\" or \"distrusted\", and only with them");
            }
            if (t.task == "fire_mode") {
                if (t.fire != "designation" && t.fire != "independent") return Fail(p + ".fire", "designation or independent");
                if (!(t.ready_in_s > 0.0)) return Fail(p + ".ready_in_s", "required with \"task\": \"fire_mode\", > 0");
                if (t.when == "trusted" && t.grade_at_least <= 0 && !(t.trust_at_least > 0.0)) {
                    return Fail(p + ".grade_at_least", "\"trusted\" reads grade_at_least or trust_at_least");
                }
                if (t.when == "distrusted" && t.grade_at_most <= 0 && !(t.trust_below > 0.0)) {
                    return Fail(p + ".grade_at_most", "\"distrusted\" reads grade_at_most or trust_below");
                }
            }
            if (t.task == "task_file") {
                if (t.task_file.empty()) return Fail(p + ".task_file", "required with \"task\": \"task_file\"");
            } else if (t.task == "relocate") {
                if (!(t.ready_in_s > 0.0)) return Fail(p + ".ready_in_s", "required with \"task\": \"relocate\", > 0");
            } else if (t.task == "march_back") {
                if (t.positions.empty()) return Fail(p + ".positions", "required with \"task\": \"march_back\": the prepared positions in depth");
                if (!(t.back_m > 0.0)) return Fail(p + ".back_m", "required with \"task\": \"march_back\", > 0");
                if (!(t.ready_in_s > 0.0)) return Fail(p + ".ready_in_s", "required with \"task\": \"march_back\", > 0");
            } else if (t.task == "posture") {
                if (!(t.ready_in_s > 0.0)) return Fail(p + ".ready_in_s", "required with \"task\": \"posture\", > 0");
                if (t.readiness < 0 || t.readiness > 2) return Fail(p + ".readiness", "1 or 2 (0: as held)");
                if (!t.emission.empty() && t.emission != "free" && t.emission != "limited" && t.emission != "arm" && t.emission != "silent") {
                    return Fail(p + ".emission", "free, limited, arm or silent");
                }
                if (t.readiness == 0 && t.emission.empty()) return Fail(p + ".readiness", "a posture changes the readiness or the emission");
            } else if (t.task != "tell_sharing" && t.task != "fire_mode") {
                return Fail(p + ".task", "task_file, relocate, march_back, posture, tell_sharing or fire_mode");
            }
            // The F4.4 triggers each take their own task.
            const std::string pair = t.when == "division_withdraws" ? "march_back"
                                     : t.when == "sector_defend" || t.when == "sector_economy" ? "posture"
                                     : t.when == "sharing_changed" ? "tell_sharing"
                                                                   : "";
            if (!pair.empty() && t.task != pair) return Fail(p + ".task", "\"" + pair + "\" with \"when\": \"" + t.when + "\"");
            if (pair.empty() && (t.task == "march_back" || t.task == "posture" || t.task == "tell_sharing")) {
                return Fail(p + ".when", "\"" + t.task + "\" is for the fronts' triggers (division_withdraws, sector_defend, sector_economy, sharing_changed)");
            }
            if (t.work_s < 0.0) return Fail(p + ".work_s", ">= 0");
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
// as it stands in the campaign's journal; and (F4.4) what reached its posts of the ground war and our air defence's
// orders through the pipeline (RegimentWatch::Fill), empty without it.
struct RegimentPicture {
    double estimate_h = -1.0;         // the campaign hour of the latest estimate (< 0: none yet)
    bool enemy_knows_battery = false; // it says the enemy probably knows the battery's position
    std::string estimate;             // its words

    // F4.4, campaign hours throughout.
    bool fronts = false;              // the pipeline's facts are in it
    double now_h = 0.0;               // the campaign's hour of the picture
    int withdrawals = 0;              // the covered division's withdrawals (or reliefs) held, each once
    double withdrawal_h = -1.0;       // when the newest reached the regiment
    std::string withdrawal;           // its words
    std::string sector;               // the division's sector as the regiment holds it (its latest report's, else the battery's)
    std::string priority;             // the sector's priority by the army's chief of air defence ("defend", "offence", "economy")
    double priority_h = -1.0;         // when his order on it reached the regiment (< 0: his pre-war plan)
    std::string priority_why;         // the order's words
    bool sharing_known = false;       // the sky's sharing there, by the national sector's priority the regiment holds
    command::CooperationMethod sharing = command::CooperationMethod::ByZones;
    double sharing_h = -1.0;          // when that order reached the regiment (< 0: the pre-war plan)

    // Track O, O5: the battery's standing as the regiment holds it from its records (grading.hpp's RegimentStanding).
    double standing_h = -1.0;         // the campaign hour of its latest summing up (< 0: none yet)
    int standing_grade = 0;           // the day's mark then (0: nothing to sum up)
    double standing_trust = 1.0;      // its trust in the battery's reports
    std::string standing_why;         // the summing up's words for the mark
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

// ------------------------------------------------------------------------------------------ F4.4: the fronts' tasks

// A place on the regiment's map: how far it lies behind the front's line into our country (m) and its name. False if
// the regiment cannot place it (not on the roads' anchors, or no front).
using PlaceDepth = std::function<bool(const std::string& place, double& depth_m, std::string& name)>;

namespace detail {

// A task's id without the fronts' suffixes, so that tasks made from tasks keep one name and one suffix.
inline std::string TaskBaseId(std::string id) {
    for (bool cut = true; cut;) {
        cut = false;
        for (const char* s : {"_withdraw", "_defend", "_economy", "_independent", "_designation"}) {
            const std::string suffix = s;
            if (id.size() > suffix.size() && id.compare(id.size() - suffix.size(), suffix.size(), suffix) == 0) {
                id.erase(id.size() - suffix.size());
                cut = true;
            }
        }
    }
    return id;
}

inline std::string Because(const std::string& reason, const std::string& fact) { return fact.empty() ? reason : reason + " (" + fact + ")"; }

}  // namespace detail

// The march back with the division (F4.4): the task the battery holds, to the nearest of the rule's prepared positions
// that lies at least back_m further behind the front's line than its main position; an object's cover (or a crossing's,
// an ambush's) becomes a relocation from there, troops' cover stays theirs. The plan of cooperation is the one the
// regiment holds for the sector (`split_m` its default split height). False, with why, if no position is far enough back.
inline bool MarchBackTask(const BatteryTask& now, const TaskingRule& rule, const RegimentPicture& pic, const PlaceDepth& depth, double split_m,
                          BatteryTask& out, std::string* why = nullptr) {
    double from_m = 0.0;
    std::string from_name;
    if (!depth || !depth(now.main, from_m, from_name)) {
        if (why != nullptr) *why = "the regiment cannot place the main position '" + now.main + "' against the front's line";
        return false;
    }
    std::string best, best_name;
    double best_m = 1e18;
    for (const std::string& p : rule.positions) {
        double d = 0.0;
        std::string name;
        if (p == now.main || !depth(p, d, name) || d < from_m + rule.back_m - 1e-6 || d >= best_m) continue;
        best = p;
        best_m = d;
        best_name = name;
    }
    if (best.empty()) {
        if (why != nullptr) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%.1f km", rule.back_m / 1000.0);
            *why = "none of the battery's prepared positions lies " + std::string(buf) + " further back than " + from_name;
        }
        return false;
    }
    out = now;
    out.id = detail::TaskBaseId(now.id) + "_withdraw";
    if (now.kind != TaskKind::CoverTroops) {
        out.kind = TaskKind::Relocate;
        out.covered.clear();
    }
    out.main = best;
    out.reserve.clear();
    out.dummies.clear();
    out.tzm_place.clear();
    out.roving = false;
    out.task_text = "withdraw with the division to the prepared position at " + best_name + " and cover its troops from there";
    out.short_ru = "отойти с дивизией на новую позицию";
    out.area_ru.clear();
    out.reasons.clear();
    out.reasons.push_back(detail::Because(rule.reason, pic.withdrawal));
    out.ready_in_s = rule.ready_in_s;
    out.reconnoitre = false;  // the division's prepared positions (vr_poltikovic_1984 p. 96)
    out.lasts_s = -1.0;
    if (pic.sharing_known) out.own_air = command::TellSharing(out.own_air, pic.sharing, split_m);
    return true;
}

// The task held with the readiness (and emission) the rule gives for the sector's new priority (F4.4).
inline void PostureTask(const BatteryTask& now, const TaskingRule& rule, const RegimentPicture& pic, double split_m, BatteryTask& out) {
    out = now;
    out.id = detail::TaskBaseId(now.id) + "_" + pic.priority;
    if (rule.readiness > 0) out.readiness = rule.readiness;
    if (!rule.readiness_text.empty()) out.readiness_text = rule.readiness_text;
    if (!rule.emission.empty() && rule.emission != out.emission) {
        out.emission = rule.emission;
        out.on_turns = out.off_turns = 0.0;  // the regime's own bursts
    }
    const std::string go_on = " and go on with the task: ";
    std::string text = now.task_text;
    const std::size_t at = text.find(go_on);
    if (text.compare(0, 14, "hold readiness") == 0 && at != std::string::npos) text = text.substr(at + go_on.size());
    out.task_text = "hold readiness no. " + std::to_string(out.readiness) + go_on + text;
    out.short_ru = out.readiness == 1 ? "задача прежняя, готовность номер один" : "задача прежняя, готовность номер два";
    out.reasons.clear();
    out.reasons.push_back(detail::Because(rule.reason, pic.priority_why));
    out.ready_in_s = rule.ready_in_s;
    out.reconnoitre = false;
    out.lasts_s = -1.0;
    if (pic.sharing_known) out.own_air = command::TellSharing(out.own_air, pic.sharing, split_m);
}

// O5: the task held with its launchers' fire mode as the rule gives it for the battery's standing.
inline void FireModeTask(const BatteryTask& now, const TaskingRule& rule, const RegimentPicture& pic, BatteryTask& out) {
    out = now;
    out.id = detail::TaskBaseId(now.id) + "_" + rule.fire;
    out.limits.fire = rule.fire;
    const std::string head = "the launchers ";
    const std::string go_on = ", and go on with the task: ";
    std::string text = now.task_text;
    const std::size_t at = text.find(go_on);
    if (text.compare(0, head.size(), head) == 0 && at != std::string::npos) text = text.substr(at + go_on.size());
    out.task_text = head + (rule.fire == "designation" ? "on the battery commander's designation only" : "to search and fire on their own in their sectors") +
                    go_on + text;
    out.reasons.clear();
    out.reasons.push_back(detail::Because(rule.reason, pic.standing_why));
    out.ready_in_s = rule.ready_in_s;
    out.reconnoitre = false;
    out.lasts_s = -1.0;
}

struct TaskingDecision {
    bool give = false;
    std::string rule;
    BatteryTask task;
    std::string why;  // when nothing is given: why not (for a debrief)
    // F4.4
    bool tell_sharing = false;  // no task: the sky's sharing told on the net
    command::CooperationMethod sharing = command::CooperationMethod::ByZones;
    double fact_h = -1.0;       // when the fact it acts on reached the regiment (campaign hours; < 0: before the war)
};

// The regiment's tasking for one battery: given what the regiment knows and the task the battery holds, the next task
// (if any). Each estimate moves the battery once; `arrived_h` is when the battery reached the position it holds (an
// estimate made before that is about where it was).
class RegimentTasking {
public:
    explicit RegimentTasking(TaskingRules rules, std::string data_dir) : rules_(std::move(rules)), data_dir_(std::move(data_dir)) {}

    // F4.4: how the regiment places its battery's positions against the front's line (march_back), and the cooperation's
    // default split height (one zone by height).
    void SetPlaceDepth(PlaceDepth f) { depth_ = std::move(f); }
    void SetSplitHeight(double m) { split_m_ = m; }
    // F4.4: what the regiment held before now was acted on before the session (its withdrawals and priority orders); the
    // sky's sharing is told when it differs from the plan the battery holds.
    void Since(const RegimentPicture& pic) {
        withdrawals_acted_ = pic.withdrawals;
        priority_acted_h_ = pic.priority_h;
    }

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
                sharing_told_ = -1;  // the file's plan of cooperation is the one the battery holds now
            } else if (r.when == "enemy_knows_battery") {
                if (holds == nullptr || !pic.enemy_knows_battery || pic.estimate_h < arrived_h || pic.estimate_h <= acted_on_h_) continue;
                if (!RelocateTask(*holds, r, pic.estimate, d.task, &d.why)) continue;
                acted_on_h_ = pic.estimate_h;
            } else if (r.when == "division_withdraws") {
                if (holds == nullptr || pic.withdrawals <= withdrawals_acted_ || !Due(pic, pic.withdrawal_h, r)) continue;
                withdrawals_acted_ = pic.withdrawals;  // one march for the withdrawals held, given or not
                if (!MarchBackTask(*holds, r, pic, depth_, split_m_, d.task, &d.why)) continue;
                d.fact_h = pic.withdrawal_h;
            } else if (r.when == "sector_defend" || r.when == "sector_economy") {
                const char* want = r.when == "sector_defend" ? "defend" : "economy";
                if (holds == nullptr || pic.priority_h < 0.0 || pic.priority_h <= priority_acted_h_ || pic.priority != want || !Due(pic, pic.priority_h, r)) {
                    continue;
                }
                priority_acted_h_ = pic.priority_h;
                PostureTask(*holds, r, pic, split_m_, d.task);
                d.fact_h = pic.priority_h;
            } else if (r.when == "trusted" || r.when == "distrusted") {
                // O5: once per summing up, on the standing the regiment holds from the battery's records.
                if (holds == nullptr || pic.standing_h < 0.0 || pic.standing_h <= standing_acted_h_ || holds->limits.fire == r.fire) continue;
                const bool trusted = (r.grade_at_least <= 0 || pic.standing_grade >= r.grade_at_least) &&
                                     (!(r.trust_at_least > 0.0) || pic.standing_trust + 1e-9 >= r.trust_at_least) && pic.standing_grade > 0;
                const bool distrusted = (r.grade_at_most > 0 && pic.standing_grade > 0 && pic.standing_grade <= r.grade_at_most) ||
                                        (r.trust_below > 0.0 && pic.standing_trust < r.trust_below);
                if (r.when == "trusted" ? !trusted : !distrusted) continue;
                standing_acted_h_ = pic.standing_h;
                FireModeTask(*holds, r, pic, d.task);
            } else if (r.when == "sharing_changed") {
                if (holds == nullptr || !pic.sharing_known) continue;
                const int now = static_cast<int>(pic.sharing);
                const int told = sharing_told_ >= 0 ? sharing_told_ : holds->own_air.present ? static_cast<int>(holds->own_air.method) : -1;
                if (told == now || !Due(pic, std::max(0.0, pic.sharing_h), r)) continue;
                sharing_told_ = now;
                d.tell_sharing = true;
                d.sharing = pic.sharing;
                d.fact_h = pic.sharing_h;
            } else {
                continue;
            }
            d.give = true;
            d.rule = r.id;
            if (!d.tell_sharing && pic.sharing_known && d.task.own_air.present) sharing_told_ = static_cast<int>(d.task.own_air.method);
            return d;
        }
        return d;
    }
    const TaskingRules& Rules() const { return rules_; }

private:
    TaskingRules rules_;
    std::string data_dir_;
    double acted_on_h_ = -1.0;  // the estimate the regiment moved the battery on
    double standing_acted_h_ = -1.0;  // O5: the summing up the regiment last changed the battery's fire mode on
    // F4.4
    PlaceDepth depth_;
    double split_m_ = 5000.0;        // own_air.json's cooperation split_height_m, as its loader's default
    int withdrawals_acted_ = 0;
    double priority_acted_h_ = -1.0;
    int sharing_told_ = -1;          // the method last told (or given in a task), -1 none: the held task's plan

    // The staff's work on the fact done by the picture's hour.
    static bool Due(const RegimentPicture& pic, double reached_h, const TaskingRule& r) { return pic.now_h + 1e-9 >= reached_h + r.work_s / 3600.0; }
};

// ------------------------------------------------------------------- F4.4: what reached the regiment through the pipeline

// One fact the watch saw reach the regiment, for the debrief and the bench.
struct RegimentArrival {
    std::string key;
    std::string what;        // "withdrawal", "report", "priority", "sharing"
    double compiled_h = 0.0; // the fact's own hour
    double reached_h = 0.0;  // when it reached the regiment's commander or its staff
};

// The regiment's posts on the campaign's pipeline (a CampaignFeed bound to an InfoNet): the facts that may be for it
// are listed as the feed adds them (the ground feed's orders and reports of the covered division, the air defence
// commanders' orders), each looked for at the regiment's commander's and its staff's posts until it arrives; the
// picture changes only when one does. Cheap: called after each pump, it reads what is new by cursors and a short list
// of what has not arrived yet; nothing per step, nothing of the truth but where its own battery stands.
class RegimentWatch {
public:
    // False, with why, without the regiment's posts in the chain.
    bool Bind(const command::InfoNet* net, const command::CampaignFeed* feed, const command::OwnAirDef& own_air, std::string* why = nullptr) {
        net_ = net;
        feed_ = feed;
        own_air_ = own_air;
        if (net_ == nullptr || feed_ == nullptr) return false;
        const command::ChainData& d = net_->Data();
        commander_ = d.PositionIndex(d.unit_regiment);
        staff_ = d.PositionIndex(feed_->Ground().Roles().regiment);
        if (commander_ < 0 || staff_ < 0) {
            if (why != nullptr) *why = "the chain has no regiment's commander or staff (" + d.unit_regiment + ", " + feed_->Ground().Roles().regiment + ")";
            net_ = nullptr;
            return false;
        }
        return true;
    }
    bool Bound() const { return net_ != nullptr; }

    // After each pump of the feed: what is new, and what of it reached the regiment by the campaign's hour. True when
    // its picture changed.
    bool Update(const airwar::AirCampaign& c) {
        if (net_ == nullptr) return false;
        const command::GroundFeed& g = feed_->Ground();
        if (g.On()) {
            for (; orders_seen_ < g.Orders().size(); ++orders_seen_) {
                const command::OrderRecord& o = g.Orders()[orders_seen_];
                if (o.fact < 0) continue;
                if (o.order == groundwar::GroundOrder::WithdrawOrdered && g.WithRegiment(o.formation)) {
                    pending_.push_back({o.fact, What::Withdrawal, o.formation, o.h, -1});
                } else if (o.order == groundwar::GroundOrder::ReliefOrdered && g.WithRegiment(o.other)) {
                    pending_.push_back({o.fact, What::Withdrawal, o.other, o.h, -1});  // relieved, it goes back
                }
            }
            for (; reports_seen_ < g.Reports().size(); ++reports_seen_) {
                const command::SituationReport& r = g.Reports()[reports_seen_];
                if (r.fact < 0 || !g.WithRegiment(r.formation)) continue;
                if (r.own) pending_.push_back({r.fact, What::Report, r.formation, r.h, r.sector});
                if (r.cause == groundwar::SitCause::Withdrawn) pending_.push_back({r.fact, What::Withdrawal, r.formation, r.h, -1});
            }
        }
        const AdStrategy& ad = feed_->Ad();
        if (ad.On()) {
            if (troops_ < 0 && national_ < 0) {
                for (std::size_t k = 0; k < ad.Commanders(); ++k) {
                    const int ki = static_cast<int>(k);
                    if (ad.Def(ki).role == AdRole::Troops && troops_ < 0) troops_ = ki;
                    if (ad.Def(ki).role == AdRole::National && national_ < 0) national_ = ki;
                }
            }
            for (; decisions_seen_ < ad.Decisions().size(); ++decisions_seen_) {
                const AdDecision& d = ad.Decisions()[decisions_seen_];
                if (d.fact < 0) continue;
                if (d.commander == troops_) pending_.push_back({d.fact, What::Priority, -1, d.h, d.sector});
                if (d.commander == national_) pending_.push_back({d.fact, What::Sharing, -1, d.h, d.sector});
            }
        }
        bool news = !read_;
        read_ = true;
        std::size_t keep = 0;
        for (std::size_t i = 0; i < pending_.size(); ++i) {
            const Pending p = pending_[i];
            const double at = ReachedH(p.fact);
            if (at < 0.0) {
                if (c.Now() - p.h < kStaleH) pending_[keep++] = p;
                continue;
            }
            Take(p, at);
            news = true;
        }
        pending_.resize(keep);
        if (news) Recompute(c);
        return news;
    }

    // The F4.4 fields of the regiment's picture at campaign hour now_h.
    void Fill(RegimentPicture& p, double now_h) const {
        if (net_ == nullptr) return;
        p.fronts = true;
        p.now_h = now_h;
        p.withdrawals = withdrawals_;
        p.withdrawal_h = withdrawal_h_;
        p.withdrawal = withdrawal_;
        p.sector = sector_id_;
        p.priority = priority_;
        p.priority_h = priority_h_;
        p.priority_why = priority_why_;
        p.sharing_known = sharing_known_;
        p.sharing = sharing_;
        p.sharing_h = sharing_h_;
    }
    const std::vector<RegimentArrival>& Arrivals() const { return arrivals_; }
    int Sector() const { return sector_; }
    std::size_t Waiting() const { return pending_.size(); }

private:
    enum class What { Withdrawal, Report, Priority, Sharing };
    struct Pending {
        int fact = -1;
        What what = What::Report;
        int formation = -1;
        double h = 0.0;   // the fact's own hour
        int sector = -1;
    };
    static constexpr double kStaleH = 72.0;            // as the air defence commanders' stale_h: not looked for after this
    static constexpr double kSameWithdrawalH = 24.0;   // sam3d: a division's order and its report of the same withdrawal

    const command::InfoNet* net_ = nullptr;
    const command::CampaignFeed* feed_ = nullptr;
    command::OwnAirDef own_air_;
    int commander_ = -1, staff_ = -1;
    int troops_ = -1, national_ = -1;
    bool read_ = false;
    std::size_t orders_seen_ = 0, reports_seen_ = 0, decisions_seen_ = 0;
    std::vector<Pending> pending_;
    std::vector<RegimentArrival> arrivals_;
    std::map<int, double> withdrawn_h_;  // by formation: the last withdrawal's own hour counted
    int withdrawals_ = 0;
    double withdrawal_h_ = -1.0;
    std::string withdrawal_;
    int sector_ = -1;                    // the division's latest report's sector held
    double report_h_ = -1.0;
    std::string sector_id_;
    std::string priority_;
    double priority_h_ = -1.0;
    std::string priority_why_;
    bool sharing_known_ = false;
    command::CooperationMethod sharing_ = command::CooperationMethod::ByZones;
    double sharing_h_ = -1.0;

    // The hour the fact reached the regiment's commander or its staff, whichever first (-1 not yet).
    double ReachedH(int fact) const {
        double at = -1.0;
        for (int post : {commander_, staff_}) {
            const command::Delivery* got = net_->Known(post, fact);
            if (got != nullptr && (at < 0.0 || got->h < at)) at = got->h;
        }
        return at;
    }

    void Take(const Pending& p, double at) {
        const command::Fact& f = net_->Facts()[static_cast<std::size_t>(p.fact)];
        static const char* const kWhat[] = {"withdrawal", "report", "priority", "sharing"};
        arrivals_.push_back({f.key, kWhat[static_cast<int>(p.what)], p.h, at});
        switch (p.what) {
            case What::Withdrawal: {
                const auto it = withdrawn_h_.find(p.formation);
                if (it != withdrawn_h_.end() && p.h - it->second < kSameWithdrawalH && p.h >= it->second) break;  // the same withdrawal
                withdrawn_h_[p.formation] = p.h;
                ++withdrawals_;
                if (at >= withdrawal_h_) {
                    withdrawal_h_ = at;
                    withdrawal_ = f.Told();
                }
                break;
            }
            case What::Report:
                if (p.h >= report_h_ && p.sector >= 0) {
                    report_h_ = p.h;
                    sector_ = p.sector;
                }
                break;
            case What::Priority:
            case What::Sharing:
                break;  // read again below from the orders that reached the regiment
        }
    }

    // The division's sector (its latest report's, else where the battery stands), and its priority and the sky's
    // sharing there as the orders that reached the regiment's commander give them.
    void Recompute(const airwar::AirCampaign& c) {
        const airwar::LineOfContact& line = c.FrontLine();
        if (line.Empty()) return;
        int s = sector_;
        if (s < 0 && c.PlayerIndex() >= 0) s = line.SectorOf(c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos);
        if (s < 0 || static_cast<std::size_t>(s) >= line.SectorCount()) return;
        sector_id_ = line.Sector(static_cast<std::size_t>(s)).id;
        const AdStrategy& ad = feed_->Ad();
        if (!ad.On()) return;
        if (troops_ >= 0) {
            priority_ = SectorPriorityName(ad.PriorityKnownBy(troops_, commander_, s, &priority_h_));
            priority_why_.clear();
            for (std::size_t i = ad.Decisions().size(); i-- > 0;) {
                const AdDecision& d = ad.Decisions()[i];
                if (d.commander != troops_ || d.sector != s || d.fact < 0 || net_->Known(commander_, d.fact) == nullptr) continue;
                priority_why_ = net_->Facts()[static_cast<std::size_t>(d.fact)].Told();
                break;
            }
        }
        if (national_ >= 0) {
            sharing_ = ad.SharingKnownBy(national_, commander_, s, own_air_, &sharing_h_);
            sharing_known_ = true;
        }
    }
};

}  // namespace campaign
}  // namespace simcore
