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
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/campaign/ad_strategy.hpp"
#include "simcore/campaign/battery_state.hpp"
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
    std::string fire;                    // fire_mode: "designation" or "independent"; O7: the task's fire mode ("" as held)
    // Track O, O7: the officers' redeployments (docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md section 8). The doctrine
    // profile: the rule's weight against each adversary (by the faction's id; "other" for any not named; none at all: 1).
    // How far the rule leaves the textbook (0 the book's own, 1 well off it): the officer's traits make him readier or
    // slower to take it. It fires when weight x (1 - departure x (1 - his willingness)) reaches `threshold`.
    std::map<std::string, double> weights;
    double departure = 0.0;
    double threshold = 0.5;
    // The facts it reads: raids that reached the regiment in the last `window_h`, at least `min_raids` of them, and
    // `ratio` times the others they are weighed against; `near_m` from the battery is near. The battery worn: in line
    // `days_in_line` days by the regiment's record of its ready report, or its last report's missiles under
    // `missiles_below`.
    int min_raids = 0;
    double window_h = 24.0;
    double ratio = 0.0;
    double near_m = 15000.0;
    double days_in_line = 0.0;
    int missiles_below = 0;
    double forward_m = 0.0;              // bounds: at least this much nearer the front's line than the main
    double lasts_h = 0.0;                // the task's length (> 0: a temporary task; the one held before it comes back)
    std::string vehicle;                 // detach_one: the launcher detached ("": the last in the task's sectors)
    std::string task_text;               // the new task's words ({place} its place's name, {entry} the raids' entry)
    std::string short_ru;                // and in the radio's short form
    // relief: the reserve battery (its name and where it comes from) and the rest at the rear (positions[0])
    std::string reserve_unit;
    std::string reserve_from;
    double rest_h = 0.0;
};

// The regiment commander as an officer (O7): his traits (0-1, as our air defence commanders' in ad_strategy.json) and
// how far they move his readiness to leave the textbook.
struct TaskingOfficer {
    std::string post;  // his position in the chain
    double boldness = 0.5;
    double creativity = 0.5;
    double rigidity = 0.5;
    double methodical = 0.5;
    double trait_shift = 0.2;
    // His willingness to leave the textbook, 0-1 (0.5 the middle).
    double Willingness() const {
        const double w = 0.5 + trait_shift * (boldness + creativity - rigidity - methodical);
        return w < 0.0 ? 0.0 : w > 1.0 ? 1.0 : w;
    }
};

struct TaskingRules {
    double briefing_hour = 19.0;  // the regiment's evening briefing, local time
    std::vector<TaskingRule> rules;
    TaskingOfficer officer;
};

// O7: how strongly a rule speaks to the officer against this adversary (see TaskingRule::weights).
inline double RuleScore(const TaskingRule& r, const TaskingOfficer& o, const std::string& adversary) {
    double w = 1.0;
    if (!r.weights.empty()) {
        auto it = r.weights.find(adversary);
        if (it == r.weights.end()) it = r.weights.find("other");
        w = it != r.weights.end() ? it->second : 0.0;
    }
    return w * (1.0 - r.departure * (1.0 - o.Willingness()));
}

namespace detail {

class TaskingLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, TaskingRules& out) {
        if (!Keys(root, "", {"tasking_id", "status", "briefing_hour", "rules", "officer", "source", "confidence", "notes"}) || !Provenance(root, "") ||
            !OptionalNumber(root, "", "briefing_hour", out.briefing_hour)) {
            return false;
        }
        if (const json::Value* o = root.Find("officer")) {  // O7: the regiment commander's traits
            TaskingOfficer& of = out.officer;
            if (!Keys(*o, "officer", {"post", "boldness", "creativity", "rigidity", "methodical", "trait_shift", "source", "confidence", "notes"}) ||
                !Provenance(*o, "officer") || !OptionalString(*o, "officer", "post", of.post) || !OptionalNumber(*o, "officer", "boldness", of.boldness) ||
                !OptionalNumber(*o, "officer", "creativity", of.creativity) || !OptionalNumber(*o, "officer", "rigidity", of.rigidity) ||
                !OptionalNumber(*o, "officer", "methodical", of.methodical) || !OptionalNumber(*o, "officer", "trait_shift", of.trait_shift)) {
                return false;
            }
            for (double v : {of.boldness, of.creativity, of.rigidity, of.methodical, of.trait_shift}) {
                if (v < 0.0 || v > 1.0) return Fail("officer", "traits and trait_shift are 0-1");
            }
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
                             "emission", "work_s", "grade_at_least", "grade_at_most", "trust_at_least", "trust_below", "fire", "weights", "departure",
                             "threshold", "min_raids", "window_h", "ratio", "near_m", "days_in_line", "missiles_below", "forward_m", "lasts_h",
                             "vehicle", "task_text", "short_ru", "reserve_unit", "reserve_from", "rest_h", "source", "confidence", "notes"}) ||
                !Provenance(r, p) || !RequireString(r, p, "id", t.id) || !RequireString(r, p, "when", t.when) ||
                !RequireString(r, p, "task", t.task) || !OptionalString(r, p, "task_file", t.task_file) ||
                !OptionalBool(r, p, "urgent", t.urgent) || !OptionalNumber(r, p, "ready_in_s", t.ready_in_s) ||
                !RequireString(r, p, "reason", t.reason) || !OptionalNumber(r, p, "back_m", t.back_m) || !OptionalInt(r, p, "readiness", t.readiness) ||
                !OptionalString(r, p, "readiness_text", t.readiness_text) || !OptionalString(r, p, "emission", t.emission) ||
                !OptionalNumber(r, p, "work_s", t.work_s) || !OptionalInt(r, p, "grade_at_least", t.grade_at_least) ||
                !OptionalInt(r, p, "grade_at_most", t.grade_at_most) || !OptionalNumber(r, p, "trust_at_least", t.trust_at_least) ||
                !OptionalNumber(r, p, "trust_below", t.trust_below) || !OptionalString(r, p, "fire", t.fire) ||
                !OptionalNumber(r, p, "departure", t.departure) || !OptionalNumber(r, p, "threshold", t.threshold) ||
                !OptionalInt(r, p, "min_raids", t.min_raids) || !OptionalNumber(r, p, "window_h", t.window_h) || !OptionalNumber(r, p, "ratio", t.ratio) ||
                !OptionalNumber(r, p, "near_m", t.near_m) || !OptionalNumber(r, p, "days_in_line", t.days_in_line) ||
                !OptionalInt(r, p, "missiles_below", t.missiles_below) || !OptionalNumber(r, p, "forward_m", t.forward_m) ||
                !OptionalNumber(r, p, "lasts_h", t.lasts_h) || !OptionalString(r, p, "vehicle", t.vehicle) || !OptionalString(r, p, "task_text", t.task_text) ||
                !OptionalString(r, p, "short_ru", t.short_ru) || !OptionalString(r, p, "reserve_unit", t.reserve_unit) ||
                !OptionalString(r, p, "reserve_from", t.reserve_from) || !OptionalNumber(r, p, "rest_h", t.rest_h)) {
                return false;
            }
            const auto any = [](const std::string&) { return std::string(); };
            if (!StringList(r, p, "positions", t.positions, any)) return false;
            if (const json::Value* w = r.Find("weights")) {  // O7: the doctrine profile, by adversary
                if (!w->Is(json::Type::Object)) return Fail(p + ".weights", "an object: adversary -> weight");
                for (std::size_t k = 0; k < w->keys.size(); ++k) {
                    if (!w->items[k].Is(json::Type::Number) || w->items[k].number < 0.0 || w->items[k].number > 1.0) {
                        return Fail(p + ".weights." + w->keys[k], "a weight 0-1");
                    }
                    t.weights[w->keys[k]] = w->items[k].number;
                }
            }
            if (t.departure < 0.0 || t.departure > 1.0 || t.threshold < 0.0 || t.threshold > 1.0) return Fail(p + ".departure", "departure and threshold 0-1");
            static const char* const kWhen[] = {"no_task", "enemy_knows_battery", "division_withdraws", "sector_defend", "sector_economy",
                                                "sharing_changed", "trusted", "distrusted",
                                                // O7: the officers' redeployments, and O6's moves with the division
                                                "heavier_elsewhere", "asset_struck", "battery_worn", "contact_lost", "cut_off", "deep_strike",
                                                "special_mission", "rebel_helicopters", "sector_threatened", "first_echelon_attacks",
                                                "division_advances", "task_over"};
            bool known = false;
            for (const char* w : kWhen) known = known || t.when == w;
            if (!known) {
                std::string all;
                for (const char* w : kWhen) all += (all.empty() ? "" : ", ") + std::string(w);
                return Fail(p + ".when", "one of " + all);
            }
            // O7: each redeployment's trigger takes its own task.
            static const std::pair<const char*, const char*> kO7[] = {
                {"heavier_elsewhere", "redeploy"}, {"asset_struck", "detach_one"},  {"battery_worn", "relief"},       {"contact_lost", "roving"},
                {"cut_off", "march_back"},         {"deep_strike", "redeploy"},     {"special_mission", "ambush"},    {"rebel_helicopters", "singles"},
                {"sector_threatened", "redeploy"}, {"first_echelon_attacks", "cover_regiment"}, {"division_advances", "bounds"}, {"task_over", "return"}};
            bool o7 = false;
            for (const auto& wt : kO7) {
                if (t.when != wt.first) continue;
                o7 = true;
                if (t.task != wt.second) return Fail(p + ".task", "\"" + std::string(wt.second) + "\" with \"when\": \"" + t.when + "\"");
            }
            if (o7) {
                if (!(t.ready_in_s > 0.0)) return Fail(p + ".ready_in_s", "required with \"when\": \"" + t.when + "\", > 0");
                const bool needs_places = t.task == "redeploy" || t.task == "detach_one" || t.task == "relief" || t.task == "ambush" || t.task == "singles" ||
                                          t.task == "cover_regiment" || t.task == "bounds" || t.task == "march_back";
                if (needs_places && t.positions.empty()) return Fail(p + ".positions", "required with \"task\": \"" + t.task + "\": the places it may go to");
                if (t.task == "ambush" && t.positions.size() < 2) return Fail(p + ".positions", "an ambush needs its main and at least one more prepared point");
                if (t.task == "relief" && (t.reserve_unit.empty() || t.reserve_from.empty())) return Fail(p + ".reserve_unit", "a relief names the reserve battery and where it comes from");
                if (t.task == "bounds" && !(t.forward_m > 0.0)) return Fail(p + ".forward_m", "required with \"task\": \"bounds\", > 0");
                if (!t.fire.empty() && t.fire != "designation" && t.fire != "independent") return Fail(p + ".fire", "designation or independent");
                if (!t.emission.empty() && t.emission != "free" && t.emission != "limited" && t.emission != "arm" && t.emission != "silent") {
                    return Fail(p + ".emission", "free, limited, arm or silent");
                }
                if (t.work_s < 0.0) return Fail(p + ".work_s", ">= 0");
                out.rules.push_back(t);
                continue;
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

    // Track O, O7: the facts the officers' redeployments read, each as it reached the regiment (campaign hours; places in
    // the session's frame).
    std::string adversary;            // the enemy's faction id (the doctrine profile's key)
    bool has_battery_at = false;
    Vec3 battery_at;                  // where the regiment has the battery (the main position it holds)
    struct Raid {
        double h = 0.0;               // over its target
        double reached_h = 0.0;       // when the warning net's report of it reached the regiment
        Vec3 at;                      // what it went for
        std::string target;           // its name as our side knows it
        std::string object;           // the theatre object's id ("" none of ours)
        std::string entry;            // where it came in
        bool helicopters = false;
        bool strategic = false;       // a national object: power, fuel, a dam, the ministries, the radio
    };
    std::vector<Raid> raids;
    double ready_report_h = -1.0;     // the battery's last ready report at its position (< 0: none)
    int missiles_reported = -1;       // its missiles in that report (< 0: not said)
    int radars_total = 0;             // the regiment's radars the battery follows on its net
    int radars_out = 0;               // of them off the air as the regiment knows it (RadarWord)
    double radars_out_h = -1.0;       // since when the regiment has known all were (< 0: not all)
    int dispersals = 0;               // the covered division broken and dispersed (cut off), each once
    double dispersal_h = -1.0;
    std::string dispersal;
    int advances = 0;                 // the covered division's attack resumed or counterattack started (bounds)
    double advance_h = -1.0;
    std::string advance;
    int counterattacks = 0;           // our counterattack ordered with the covered division's first echelon
    double counterattack_h = -1.0;
    std::string counterattack;
    bool has_counterattack_at = false;
    Vec3 counterattack_at;            // the axis's point on the line
    std::string threatened_sector;    // another sector raised to defend by the army's chief of air defence
    double threatened_h = -1.0;
    std::string threatened_why;
    bool has_threatened_at = false;
    Vec3 threatened_at;               // that sector's middle on the line
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
        const std::size_t o7 = id.find("_o7_");  // O7: a redeployment's task, named after the rule that gave it
        if (o7 != std::string::npos && o7 > 0) {
            id.erase(o7);
            cut = true;
        }
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

// ------------------------------------------------------------------- Track O, O7: the officers' redeployments

// A place on the regiment's map in the session's frame (false: unknown), and its name.
using PlaceAt = std::function<bool(const std::string& place, Vec3& at)>;
using PlaceWords = std::function<std::string(const std::string& place)>;

namespace detail {

inline std::string FillTaskText(std::string text, const std::string& place, const std::string& entry) {
    for (const auto& kv : {std::make_pair(std::string("{place}"), place), std::make_pair(std::string("{entry}"), entry)}) {
        for (std::size_t at = text.find(kv.first); at != std::string::npos; at = text.find(kv.first, at + kv.second.size())) {
            text.replace(at, kv.first.size(), kv.second);
        }
    }
    return text;
}
// The rule's place nearest `to` (not `skip`), "" when none is on the map.
inline std::string NearestPlace(const std::vector<std::string>& places, const Vec3& to, const PlaceAt& at, const std::string& skip) {
    std::string best;
    double best_m = 1e18;
    for (const std::string& p : places) {
        Vec3 v;
        if (p == skip || !at || !at(p, v)) continue;
        const double d = std::hypot(v.x - to.x, v.y - to.y);
        if (d < best_m) {
            best_m = d;
            best = p;
        }
    }
    return best;
}
inline std::string O7Id(const BatteryTask& now, const TaskingRule& rule) { return TaskBaseId(now.id) + "_o7_" + rule.id; }
// What every redeployment's task takes from its rule: its reason (with the fact), its time, its length, its fire mode and
// emission when the rule gives them.
inline void FromRule(BatteryTask& out, const TaskingRule& rule, const std::string& fact) {
    out.reasons.clear();
    out.reasons.push_back(Because(rule.reason, fact));
    out.ready_in_s = rule.ready_in_s;
    out.lasts_s = rule.lasts_h > 0.0 ? std::max(rule.lasts_h * 3600.0, rule.ready_in_s) : -1.0;
    if (!rule.fire.empty()) out.limits.fire = rule.fire;
    if (!rule.emission.empty() && rule.emission != out.emission) {
        out.emission = rule.emission;
        out.on_turns = out.off_turns = 0.0;
    }
    if (!rule.short_ru.empty()) out.short_ru = rule.short_ru;
}

}  // namespace detail

// The battery sent to another place to cover it (another division's sector under heavier attack, a strategic object lent
// to the national air defence, a threatened sector): the place its main position and the covered object, a new position
// to reconnoitre, nothing detached.
inline BatteryTask RedeployTask(const BatteryTask& now, const TaskingRule& rule, const std::string& place, const std::string& place_name,
                                const std::string& fact) {
    BatteryTask out = now;
    out.id = detail::O7Id(now, rule);
    out.kind = TaskKind::CoverObject;
    out.covered = place;
    out.main = place;
    out.reserve.clear();
    out.dummies.clear();
    out.tzm_place.clear();
    out.detached.clear();
    out.roving = false;
    out.limits.cover_unit.clear();
    out.limits.leash_m = -1.0;
    out.area_ru.clear();
    out.task_text = detail::FillTaskText(rule.task_text.empty() ? "march to {place} and cover it from low-flying attack" : rule.task_text, place_name, "");
    out.short_ru = "сменить район, прикрыть объект";
    detail::FromRule(out, rule, fact);
    out.reconnoitre = true;  // a position new to the battery
    return out;
}

// One launcher detached to cover an asset the enemy is striking (the rest go on with the task held).
inline BatteryTask DetachOneTask(const BatteryTask& now, const TaskingRule& rule, const std::string& place, const std::string& place_name,
                                 const std::string& fact) {
    BatteryTask out = now;
    std::string vehicle = rule.vehicle;
    if (vehicle.empty() && !now.sectors.empty()) vehicle = now.sectors.back().vehicle;
    out.id = detail::O7Id(now, rule);
    out.detached.clear();
    out.detached.push_back({vehicle, place});
    out.task_text = detail::FillTaskText(rule.task_text.empty() ? "launcher " + vehicle + " detached to cover {place}; the battery goes on with its task" : rule.task_text,
                                         place_name, "") + ": " + now.task_text;
    out.short_ru = "пусковую " + vehicle + " выделить на прикрытие объекта, задача батареи прежняя";
    detail::FromRule(out, rule, fact);
    out.reconnoitre = false;
    return out;
}

// The launchers split singly over the rule's places (the rebels' helicopters): each in turn to the next place, the post
// staying at the main position; each launcher on its own.
inline BatteryTask SinglesTask(const BatteryTask& now, const TaskingRule& rule, const PlaceWords& name, const std::string& fact) {
    BatteryTask out = now;
    out.id = detail::O7Id(now, rule);
    out.detached.clear();
    std::string list;
    for (std::size_t i = 0; i < now.sectors.size() && i < rule.positions.size(); ++i) {
        out.detached.push_back({now.sectors[i].vehicle, rule.positions[i]});
        list += (list.empty() ? "" : ", ") + now.sectors[i].vehicle + " to " + (name ? name(rule.positions[i]) : rule.positions[i]);
    }
    out.task_text = detail::FillTaskText(rule.task_text.empty() ? "split the launchers singly over the villages ({place}), each covering its village and the road"
                                                                : rule.task_text,
                                         list, "");
    out.short_ru = "пусковые — по одной на прикрытие сёл и дорог";
    detail::FromRule(out, rule, fact);
    if (rule.fire.empty()) out.limits.fire = "independent";  // each on its own: nobody to designate for it
    out.reconnoitre = false;
    return out;
}

// An ambush on the enemy's route (the rule's prepared points, the first the main), the radars silent until the moment.
inline BatteryTask AmbushTask(const BatteryTask& now, const TaskingRule& rule, const std::string& entry, const PlaceWords& name, const std::string& fact) {
    BatteryTask out = now;
    out.id = detail::O7Id(now, rule);
    out.kind = TaskKind::Ambush;
    out.main = rule.positions.front();
    out.reserve.assign(rule.positions.begin() + 1, rule.positions.end());
    out.dummies.clear();
    out.tzm_place.clear();
    out.detached.clear();
    out.roving = false;
    out.task_text = detail::FillTaskText(rule.task_text.empty() ? "lie in ambush at {place} on the route from {entry}, the radars silent until the moment"
                                                                : rule.task_text,
                                         name ? name(out.main) : out.main, entry);
    out.short_ru = "засада, РЛС не включать до команды";
    detail::FromRule(out, rule, fact);
    if (rule.emission.empty()) out.emission = "silent";
    out.reconnoitre = true;
    return out;
}

// The battery handing over to the reserve and marching to rest and repair at the rear (positions[0]).
inline BatteryTask RestTask(const BatteryTask& now, const TaskingRule& rule, const PlaceWords& name, const std::string& fact) {
    BatteryTask out = now;
    out.id = detail::O7Id(now, rule);
    out.kind = TaskKind::Relocate;
    out.main = rule.positions.front();
    out.covered.clear();
    out.reserve.clear();
    out.dummies.clear();
    out.tzm_place.clear();
    out.detached.clear();
    out.roving = false;
    out.limits = OrderLimits();
    out.task_text = detail::FillTaskText(rule.task_text.empty() ? "hand over the position to " + rule.reserve_unit + " and march to {place} to rest and repair"
                                                                : rule.task_text,
                                         name ? name(out.main) : out.main, "");
    out.short_ru = "сдать позицию резервной батарее, отдых и ремонт";
    out.readiness = 2;
    out.readiness_text = "no. 2: at rest, refitting";
    detail::FromRule(out, rule, fact);
    out.reconnoitre = false;
    return out;
}

// Contact with the regiment lost (its post and radars gone): the battery fights alone, roving between its own positions,
// its launchers on their own.
inline BatteryTask RovingTask(const BatteryTask& now, const TaskingRule& rule, const std::string& fact) {
    BatteryTask out = now;
    out.id = detail::O7Id(now, rule);
    out.kind = TaskKind::Ambush;
    out.roving = true;
    out.detached.clear();
    if (out.reserve.empty()) out.reserve.push_back(now.main);
    out.task_text = rule.task_text.empty() ? "fight on alone, roving between the battery's positions, the launchers searching on their own" : rule.task_text;
    out.short_ru = "связи с полком нет: действовать самостоятельно, кочующими";
    detail::FromRule(out, rule, fact);
    if (rule.fire.empty()) out.limits.fire = "independent";
    out.reconnoitre = false;
    return out;
}

// A battery sent to cover a first-echelon regiment going into the attack: behind it, on its axis.
inline BatteryTask CoverRegimentTask(const BatteryTask& now, const TaskingRule& rule, const std::string& place, const std::string& place_name,
                                     double axis_deg, const std::string& fact) {
    BatteryTask out = now;
    out.id = detail::O7Id(now, rule);
    out.kind = TaskKind::CoverTroops;
    out.covered = place;
    out.main = place;
    out.axis_deg = axis_deg;
    out.reserve.clear();
    out.dummies.clear();
    out.tzm_place.clear();
    out.detached.clear();
    out.limits.cover_unit.clear();
    out.limits.leash_m = -1.0;
    out.task_text = detail::FillTaskText(rule.task_text.empty() ? "cover the first-echelon regiment's attack from {place}, behind it on its axis" : rule.task_text,
                                         place_name, "");
    out.short_ru = "прикрыть полк первого эшелона в наступлении";
    detail::FromRule(out, rule, fact);
    out.reconnoitre = true;
    return out;
}

// The battery moved forward by a bound (the division advancing): the nearest of the rule's prepared positions at least
// forward_m nearer the front's line than its main position. False, with why, with none.
inline bool BoundsTask(const BatteryTask& now, const TaskingRule& rule, const PlaceDepth& depth, const std::string& fact, BatteryTask& out,
                       std::string* why = nullptr) {
    double from_m = 0.0;
    std::string from_name;
    if (!depth || !depth(now.main, from_m, from_name)) {
        if (why != nullptr) *why = "the regiment cannot place the main position '" + now.main + "' against the front's line";
        return false;
    }
    std::string best, best_name;
    double best_m = -1e18;
    for (const std::string& p : rule.positions) {
        double d = 0.0;
        std::string name;
        if (p == now.main || !depth(p, d, name) || d > from_m - rule.forward_m + 1e-6 || d <= best_m) continue;
        best = p;
        best_m = d;
        best_name = name;
    }
    if (best.empty()) {
        if (why != nullptr) *why = "none of the battery's prepared positions lies a bound further forward than " + from_name;
        return false;
    }
    out = now;
    out.id = detail::O7Id(now, rule);
    if (now.kind != TaskKind::CoverTroops) {
        out.kind = TaskKind::Relocate;
        out.covered.clear();
    }
    out.main = best;
    out.reserve.clear();
    out.dummies.clear();
    out.tzm_place.clear();
    out.detached.clear();
    out.task_text = detail::FillTaskText(rule.task_text.empty() ? "move forward by a bound to {place}; the regiment's other batteries stand until you report ready there"
                                                                : rule.task_text,
                                         best_name, "");
    out.short_ru = "перейти вперёд скачком на новую позицию";
    detail::FromRule(out, rule, fact);
    out.reconnoitre = false;  // the division's prepared positions
    return true;
}

struct TaskingDecision {
    bool give = false;
    std::string rule;
    BatteryTask task;
    std::string why;  // when nothing is given: why not (for a debrief)
    // O7: the relief by the reserve battery: the task (the march to rest) is given once the reserve has arrived; the reserve
    // battery's name and where it comes from. The rule's score with the officer and the adversary.
    bool relief = false;
    std::string reserve_unit;
    std::string reserve_from;
    double rest_h = 0.0;
    double score = 1.0;
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
        dispersals_acted_ = pic.dispersals;  // O7
        advances_acted_ = pic.advances;
        counterattacks_acted_ = pic.counterattacks;
        for (const TaskingRule& r : rules_.rules) {
            if (r.when == "sector_threatened") last_h_[r.id] = pic.threatened_h;
        }
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
            } else if (!Officer(r, pic, holds, d)) {  // O7
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

    // O7: the raids reported before `now_h` were weighed before the session (a session resumed, or the war's hours run
    // before it): the raid rules count only those reaching the regiment from then.
    void RaidsSince(double now_h) {
        for (const TaskingRule& r : rules_.rules) {
            if (r.when == "heavier_elsewhere" || r.when == "deep_strike" || r.when == "asset_struck" || r.when == "special_mission" ||
                r.when == "rebel_helicopters") {
                last_h_[r.id] = now_h;
            }
        }
    }
    // O7: where the regiment's places are and what they are called (the session's anchors).
    void SetPlaces(PlaceAt at, PlaceWords name) {
        place_at_ = std::move(at);
        place_name_ = std::move(name);
    }
    // A temporary task the regiment gave (a loan, a detachment for some days): its id, until when (campaign hours), and the
    // task held before it, which the regiment gives back then (task_over). Empty: none.
    const std::string& TemporaryTask() const { return temp_id_; }
    double TemporaryUntilH() const { return temp_until_h_; }
    // A relief under way: the reserve battery on its way (the session gives the march to rest on its arrival).
    void ReliefDone() { relief_pending_ = false; }
    bool ReliefPending() const { return relief_pending_; }
    // What of the regiment's redeployments a save carries (battery_state.hpp's RegimentMemory, its tasking part), and
    // taken back on a resume, before Since and RaidsSince (which then mark what reached it before the session as acted on).
    void FillMemory(RegimentMemory& m) const {
        m.present = true;
        m.last_h = last_h_;
        m.temp_id = temp_id_;
        m.temp_until_h = temp_until_h_;
        m.has_before = !temp_id_.empty();
        if (m.has_before) m.before = before_;
        m.relief_pending = relief_pending_;
    }
    void Remember(const RegimentMemory& m) {
        if (!m.present) return;
        last_h_ = m.last_h;
        temp_id_ = m.has_before ? m.temp_id : std::string();
        temp_until_h_ = m.temp_until_h;
        if (m.has_before) before_ = m.before;
        relief_pending_ = m.relief_pending;
    }

private:
    // ---- O7: the officers' redeployments. True when the rule gives a task now (d filled).
    bool Officer(const TaskingRule& r, const RegimentPicture& pic, const BatteryTask* holds, TaskingDecision& d) {
        if (holds == nullptr) return false;
        const bool temp_held = !temp_id_.empty() && holds->id == temp_id_;
        if (r.when == "task_over") {
            if (!temp_held || pic.now_h + 1e-9 < temp_until_h_) return false;
            d.task = before_;
            d.task.reasons.clear();
            d.task.reasons.push_back(r.reason);
            d.task.ready_in_s = r.ready_in_s;
            d.task.reconnoitre = false;
            d.task.lasts_s = -1.0;
            temp_id_.clear();
            return true;
        }
        const double score = RuleScore(r, rules_.officer, pic.adversary);
        d.score = score;
        if (score + 1e-9 < r.threshold) return false;
        const bool from_this = holds->id.find("_o7_" + r.id) != std::string::npos;
        const bool urgent_kind = r.when == "contact_lost" || r.when == "cut_off";
        if ((temp_held || relief_pending_) && !urgent_kind) return false;  // the loan or the relief first
        if (from_this && r.when != "division_advances") return false;
        auto found = last_h_.find(r.id);
        if (found == last_h_.end()) found = last_h_.emplace(r.id, -1e18).first;
        double& last = found->second;
        const auto recent = [&](const RegimentPicture::Raid& x) { return x.reached_h > last + 1e-9 && x.reached_h >= pic.now_h - r.window_h - 1e-9; };
        const auto near_battery = [&](const RegimentPicture::Raid& x) {
            return pic.has_battery_at && std::hypot(x.at.x - pic.battery_at.x, x.at.y - pic.battery_at.y) <= r.near_m;
        };
        const auto name = [this](const std::string& p) { return place_name_ ? place_name_(p) : p; };
        const auto due = [&](double fact_h) { return fact_h >= 0.0 && Due(pic, fact_h, r); };
        if (r.when == "heavier_elsewhere" || r.when == "deep_strike") {
            int counted = 0, others = 0;
            Vec3 sum;
            double newest = -1.0;
            for (const RegimentPicture::Raid& x : pic.raids) {
                if (!recent(x)) continue;
                const bool takes = r.when == "deep_strike" ? x.strategic && !near_battery(x) : !near_battery(x);
                if (takes) {
                    ++counted;
                    sum += x.at;
                    newest = std::max(newest, x.reached_h);
                } else if (near_battery(x)) {
                    ++others;
                }
            }
            if (counted < std::max(1, r.min_raids) || counted + 1e-9 < r.ratio * std::max(1, others) || !due(newest)) return false;
            const std::string place = detail::NearestPlace(r.positions, sum / static_cast<double>(counted), place_at_, holds->main);
            if (place.empty()) return false;
            char fact[160];
            std::snprintf(fact, sizeof(fact), "%d raids reported %s in the last %.0f h, %d near you", counted,
                          r.when == "deep_strike" ? "on the country's strategic objects" : "on the other division's sector", r.window_h, others);
            d.task = RedeployTask(*holds, r, place, name(place), fact);
            d.fact_h = newest;
        } else if (r.when == "asset_struck") {
            std::map<std::string, int> hits;
            std::map<std::string, Vec3> at;
            std::map<std::string, std::string> called;
            double newest = -1.0;
            for (const RegimentPicture::Raid& x : pic.raids) {
                if (!recent(x) || near_battery(x)) continue;
                const std::string key = x.object.empty() ? x.target : x.object;
                ++hits[key];
                at[key] = x.at;
                called[key] = x.target;
                newest = std::max(newest, x.reached_h);
            }
            std::string best;
            for (const auto& kv : hits) {
                if (kv.second >= std::max(1, r.min_raids) && (best.empty() || kv.second > hits[best])) best = kv.first;
            }
            if (best.empty() || !due(newest)) return false;
            const std::string place = detail::NearestPlace(r.positions, at[best], place_at_, std::string());
            if (place.empty()) return false;
            d.task = DetachOneTask(*holds, r, place, name(place), "our intelligence expects " + called[best] + " to be struck again: " +
                                                                    std::to_string(hits[best]) + " raids on it reported");
            d.fact_h = newest;
        } else if (r.when == "special_mission" || r.when == "rebel_helicopters") {
            std::map<std::string, int> by_entry;
            int helicopters = 0, fixed = 0;
            double newest = -1.0;
            for (const RegimentPicture::Raid& x : pic.raids) {
                if (!recent(x)) continue;
                if (x.helicopters) {
                    ++helicopters;
                    ++by_entry[x.entry];
                    newest = std::max(newest, x.reached_h);
                } else {
                    ++fixed;
                }
            }
            if (!due(newest)) return false;
            if (r.when == "rebel_helicopters") {
                if (!holds->detached.empty() || helicopters < std::max(1, r.min_raids) || helicopters + 1e-9 < r.ratio * std::max(1, fixed)) return false;
                char fact[120];
                std::snprintf(fact, sizeof(fact), "%d helicopter raids and %d by aircraft reported in the last %.0f h", helicopters, fixed, r.window_h);
                d.task = SinglesTask(*holds, r, name, fact);
                if (d.task.detached.empty()) return false;
            } else {
                std::string entry;
                for (const auto& kv : by_entry) {
                    if (kv.second >= std::max(1, r.min_raids) && (entry.empty() || kv.second > by_entry[entry])) entry = kv.first;
                }
                if (entry.empty()) return false;
                d.task = AmbushTask(*holds, r, entry, name, std::to_string(by_entry[entry]) + " helicopter raids reported from " + entry);
            }
            d.fact_h = newest;
        } else if (r.when == "battery_worn") {
            if (pic.ready_report_h < 0.0 || pic.ready_report_h <= last + 1e-9) return false;
            const double in_line_h = pic.now_h - pic.ready_report_h;
            const bool tired = r.days_in_line > 0.0 && in_line_h + 1e-9 >= r.days_in_line * 24.0;
            const bool short_of = r.missiles_below > 0 && pic.missiles_reported >= 0 && pic.missiles_reported < r.missiles_below;
            if (!tired && !short_of) return false;
            char fact[120];
            if (tired) {
                std::snprintf(fact, sizeof(fact), "in line %.0f h since its ready report", in_line_h);
            } else {
                std::snprintf(fact, sizeof(fact), "%d missiles in its last report", pic.missiles_reported);
            }
            d.task = RestTask(*holds, r, name, fact);
            d.relief = true;
            d.reserve_unit = r.reserve_unit;
            d.reserve_from = r.reserve_from;
            d.rest_h = r.rest_h;
            d.fact_h = pic.ready_report_h;
            relief_pending_ = true;
            last = pic.ready_report_h;
            return true;
        } else if (r.when == "contact_lost") {
            if (pic.radars_total <= 0 || pic.radars_out_h < 0.0 || pic.radars_out_h <= last + 1e-9 || !due(pic.radars_out_h)) return false;
            d.task = RovingTask(*holds, r, "all " + std::to_string(pic.radars_total) + " of the regiment's radars off the air");
            d.fact_h = pic.radars_out_h;
            last = pic.radars_out_h;
            temp_id_.clear();  // no loan stands with the regiment gone
            return true;
        } else if (r.when == "cut_off") {
            if (pic.dispersals <= dispersals_acted_ || !due(pic.dispersal_h)) return false;
            dispersals_acted_ = pic.dispersals;
            if (!MarchBackTask(*holds, r, pic, depth_, split_m_, d.task, &d.why)) return false;
            d.task.id = detail::O7Id(*holds, r);
            d.task.task_text = (r.task_text.empty() ? std::string("withdraw alone to the next line") : r.task_text) + ": " + d.task.task_text;
            d.task.reasons.clear();
            d.task.reasons.push_back(detail::Because(r.reason, pic.dispersal));
            if (!r.fire.empty()) d.task.limits.fire = r.fire;
            if (!r.short_ru.empty()) d.task.short_ru = r.short_ru;
            d.fact_h = pic.dispersal_h;
            temp_id_.clear();
            return true;
        } else if (r.when == "sector_threatened") {
            if (pic.threatened_h < 0.0 || pic.threatened_h <= last + 1e-9 || !pic.has_threatened_at || !due(pic.threatened_h)) return false;
            const std::string place = detail::NearestPlace(r.positions, pic.threatened_at, place_at_, holds->main);
            last = pic.threatened_h;
            if (place.empty()) return false;
            d.task = RedeployTask(*holds, r, place, name(place), pic.threatened_why.empty() ? "sector " + pic.threatened_sector : pic.threatened_why);
            d.fact_h = pic.threatened_h;
        } else if (r.when == "first_echelon_attacks") {
            if (pic.counterattacks <= counterattacks_acted_ || !pic.has_counterattack_at || !due(pic.counterattack_h)) return false;
            counterattacks_acted_ = pic.counterattacks;
            const std::string place = detail::NearestPlace(r.positions, pic.counterattack_at, place_at_, std::string());
            Vec3 p;
            if (place.empty() || !place_at_(place, p)) return false;
            double axis = std::atan2(pic.counterattack_at.x - p.x, pic.counterattack_at.y - p.y) * 180.0 / 3.14159265358979323846;
            if (axis < 0.0) axis += 360.0;
            d.task = CoverRegimentTask(*holds, r, place, name(place), axis, pic.counterattack);
            d.fact_h = pic.counterattack_h;
        } else if (r.when == "division_advances") {
            if (pic.advances <= advances_acted_ || !due(pic.advance_h)) return false;
            advances_acted_ = pic.advances;
            if (!BoundsTask(*holds, r, depth_, pic.advance, d.task, &d.why)) return false;
            d.fact_h = pic.advance_h;
        } else {
            return false;
        }
        last = pic.now_h;
        if (r.lasts_h > 0.0) {  // a loan: the task held before it comes back at its end
            if (!temp_held) before_ = *holds;
            temp_id_ = d.task.id;
            temp_until_h_ = pic.now_h + r.lasts_h;
        }
        return true;
    }

    TaskingRules rules_;
    std::string data_dir_;
    // O7
    PlaceAt place_at_;
    PlaceWords place_name_;
    std::map<std::string, double> last_h_;  // by rule: the picture's hour it last gave a task on
    std::string temp_id_;
    double temp_until_h_ = 0.0;
    BatteryTask before_;
    bool relief_pending_ = false;
    int dispersals_acted_ = 0, advances_acted_ = 0, counterattacks_acted_ = 0;
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

// O7: what the regiment knows of its own radars being on the air. Each radar's reports (its plots and its routine reports
// to the regiment's post) are heard while it is on the air; a report of it wrecked reaches the regiment through the
// pipeline (the feed's OwnDamage fact, RegimentWatch::RadarWrecks). A radar is off the air for the regiment from the
// arrival of a wreck report while nothing has been heard from it since the wreck, or once nothing has been heard from it
// for kSilentH: the net gone silent. Never from the radar's state itself.
class RadarWord {
public:
    static constexpr double kSilentH = 0.25;  // sam3d: a game value (the post calls a radar it has not heard for 15 min)

    void Add(const std::string& id, double heard_h) {
        for (const Radar& r : radars_) {
            if (r.id == id) return;
        }
        radars_.push_back({id, heard_h, -1.0, -1.0});
    }
    void Clear() { radars_.clear(); }
    // A report from the radar reached the regiment at h.
    void Heard(const std::string& id, double h) {
        for (Radar& r : radars_) {
            if (r.id == id && h > r.heard_h) r.heard_h = h;
        }
    }
    // A report of the radar wrecked at wrecked_h reached the regiment at reached_h.
    void Reported(const std::string& id, double wrecked_h, double reached_h) {
        for (Radar& r : radars_) {
            if (r.id != id || wrecked_h < r.wrecked_h) continue;
            r.wrecked_h = wrecked_h;
            r.reached_h = reached_h;
        }
    }
    // When the regiment has known the radar off the air, at now_h (< 0: it has not).
    double OutSinceH(const std::string& id, double now_h) const {
        for (const Radar& r : radars_) {
            if (r.id == id) return OutSince(r, now_h);
        }
        return -1.0;
    }
    std::size_t Count() const { return radars_.size(); }
    void Fill(RegimentPicture& p, double now_h) const {
        p.radars_total = static_cast<int>(radars_.size());
        p.radars_out = 0;
        double all_h = -1.0;
        for (const Radar& r : radars_) {
            const double h = OutSince(r, now_h);
            if (h < 0.0) continue;
            ++p.radars_out;
            all_h = std::max(all_h, h);
        }
        p.radars_out_h = p.radars_total > 0 && p.radars_out == p.radars_total ? all_h : -1.0;
    }

private:
    struct Radar {
        std::string id;
        double heard_h = -1.0;    // its last word
        double wrecked_h = -1.0;  // the last wreck reported: when it was wrecked, and when the report came
        double reached_h = -1.0;
    };
    static double OutSince(const Radar& r, double now_h) {
        double h = -1.0;
        const bool unheard_since = r.heard_h <= r.wrecked_h + kSilentH + 1e-9;  // repaired and heard again: the report is old
        if (r.reached_h >= 0.0 && unheard_since && r.reached_h <= now_h + 1e-9) h = r.reached_h;
        const double silent = r.heard_h + kSilentH;
        if (silent <= now_h + 1e-9 && (h < 0.0 || silent < h)) h = silent;
        return h;
    }
    std::vector<Radar> radars_;
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
    // O7: a wreck of one of the regiment's radars as its report reached the regiment (the theatre object's id).
    struct RadarWreck {
        std::string object;
        double wrecked_h = 0.0;
        double reached_h = 0.0;
    };
    const std::vector<RadarWreck>& RadarWrecks() const { return radar_wrecks_; }

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
                } else if (o.order == groundwar::GroundOrder::Dispersed && g.WithRegiment(o.formation)) {
                    pending_.push_back({o.fact, What::Dispersal, o.formation, o.h, -1});  // O7: cut off, broken
                } else if ((o.order == groundwar::GroundOrder::AttackResumed || o.order == groundwar::GroundOrder::CounterattackStarts) &&
                           g.WithRegiment(o.formation)) {
                    pending_.push_back({o.fact, What::Advance, o.formation, o.h, o.axis});  // O7: it goes forward
                } else if (o.order == groundwar::GroundOrder::CounterattackOrdered && g.WithRegiment(o.formation)) {
                    pending_.push_back({o.fact, What::Counterattack, o.formation, o.h, o.axis});  // O7: its first echelon attacks
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
                // O7: a sector raised to defend: threatened, if it is not the division's own (read when it arrives).
                if (d.commander == troops_ && d.to == SectorPriority::Defend && d.from != SectorPriority::Defend) {
                    pending_.push_back({d.fact, What::Threatened, -1, d.h, d.sector});
                }
            }
        }
        // O7: the warning net's raids, as each reached the regiment (the feed's "raid:N" facts).
        const auto& raids = c.ObservedRaids();
        for (; raids_seen_ < raids.size(); ++raids_seen_) {
            const int fact = net_->FactIndex("raid:" + std::to_string(raids_seen_));
            if (fact < 0) break;  // not in the pipeline yet: looked for after the next pump
            pending_.push_back({fact, What::Raid, -1, raids[raids_seen_].h, static_cast<int>(raids_seen_)});
        }
        // O7: the wrecks of the regiment's radars reported (the feed's OwnDamage facts), as each reached the regiment.
        const std::vector<command::Fact>& facts = net_->Facts();
        for (; facts_seen_ < facts.size(); ++facts_seen_) {
            const command::Fact& f = facts[facts_seen_];
            if (f.kind != command::FactKind::OwnDamage) continue;
            const int oi = c.Data().theatre.IndexOf(f.subject);
            if (oi < 0 || c.Data().theatre.objects[static_cast<std::size_t>(oi)].regiment_radar.empty()) continue;
            pending_.push_back({static_cast<int>(facts_seen_), What::Wreck, -1, f.h, oi});
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
            Take(p, at, c);
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
        // O7, in the theatre's frame (the session puts its places in its own).
        p.raids = raids_;
        p.dispersals = dispersals_;
        p.dispersal_h = dispersal_h_;
        p.dispersal = dispersal_;
        p.advances = advances_;
        p.advance_h = advance_h_;
        p.advance = advance_;
        p.counterattacks = counterattacks_;
        p.counterattack_h = counterattack_h_;
        p.counterattack = counterattack_;
        p.has_counterattack_at = has_counterattack_at_;
        p.counterattack_at = counterattack_at_;
        if (threatened_ >= 0 && threatened_ != sector_) {
            p.threatened_sector = threatened_id_;
            p.threatened_h = threatened_h_;
            p.threatened_why = threatened_why_;
            p.has_threatened_at = has_threatened_at_;
            p.threatened_at = threatened_at_;
        }
    }
    const std::vector<RegimentArrival>& Arrivals() const { return arrivals_; }
    int Sector() const { return sector_; }
    std::size_t Waiting() const { return pending_.size(); }

private:
    enum class What { Withdrawal, Report, Priority, Sharing, Dispersal, Advance, Counterattack, Threatened, Raid, Wreck };
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
    // O7: the facts the redeployments read.
    std::size_t raids_seen_ = 0;
    std::size_t facts_seen_ = 0;  // O7: the net's facts looked at for our radars' wrecks
    std::vector<RadarWreck> radar_wrecks_;
    std::vector<RegimentPicture::Raid> raids_;
    int dispersals_ = 0, advances_ = 0, counterattacks_ = 0;
    double dispersal_h_ = -1.0, advance_h_ = -1.0, counterattack_h_ = -1.0;
    std::string dispersal_, advance_, counterattack_;
    bool has_counterattack_at_ = false;
    Vec3 counterattack_at_;
    int threatened_ = -1;
    std::string threatened_id_, threatened_why_;
    double threatened_h_ = -1.0;
    bool has_threatened_at_ = false;
    Vec3 threatened_at_;

    static bool Strategic(airwar::ObjectKind k) {
        switch (k) {
            case airwar::ObjectKind::PowerPlant:
            case airwar::ObjectKind::Refinery:
            case airwar::ObjectKind::PipelineStation:
            case airwar::ObjectKind::Substation:
            case airwar::ObjectKind::Dam:
            case airwar::ObjectKind::Port:
            case airwar::ObjectKind::Government:
            case airwar::ObjectKind::Broadcast:
                return true;
            default:
                return false;
        }
    }
    // A point of the line of contact (theatre frame): the middle of sector s, or where axis a pushed in.
    static bool SectorMiddle(const airwar::LineOfContact& line, int s, Vec3& out) {
        if (line.Empty() || s < 0 || static_cast<std::size_t>(s) >= line.SectorCount()) return false;
        const airwar::SectorDef& d = line.Sector(static_cast<std::size_t>(s));
        const double from = std::max(0.0, d.from_m), to = std::min(line.Length(), d.to_m);
        out = line.PointAt(0.5 * (from + std::max(from, to)));
        return true;
    }

    // The hour the fact reached the regiment's commander or its staff, whichever first (-1 not yet).
    double ReachedH(int fact) const {
        double at = -1.0;
        for (int post : {commander_, staff_}) {
            const command::Delivery* got = net_->Known(post, fact);
            if (got != nullptr && (at < 0.0 || got->h < at)) at = got->h;
        }
        return at;
    }

    void Take(const Pending& p, double at, const airwar::AirCampaign& c) {
        const command::Fact& f = net_->Facts()[static_cast<std::size_t>(p.fact)];
        static const char* const kWhat[] = {"withdrawal", "report", "priority", "sharing", "dispersal", "advance", "counterattack", "threatened", "raid", "wreck"};
        if (p.what != What::Raid && p.what != What::Wreck) arrivals_.push_back({f.key, kWhat[static_cast<int>(p.what)], p.h, at});
        const airwar::LineOfContact& line = c.FrontLine();
        switch (p.what) {
            case What::Dispersal:
                ++dispersals_;
                if (at >= dispersal_h_) {
                    dispersal_h_ = at;
                    dispersal_ = f.Told();
                }
                break;
            case What::Advance:
                ++advances_;
                if (at >= advance_h_) {
                    advance_h_ = at;
                    advance_ = f.Told();
                }
                break;
            case What::Counterattack: {
                ++counterattacks_;
                if (at < counterattack_h_) break;
                counterattack_h_ = at;
                counterattack_ = f.Told();
                const groundwar::GroundWar* g = c.Ground();
                has_counterattack_at_ = false;
                if (g != nullptr && p.sector >= 0 && static_cast<std::size_t>(p.sector) < g->Axes().size() && !line.Empty()) {
                    counterattack_at_ = line.PointAt(g->Axes()[static_cast<std::size_t>(p.sector)].along_m);
                    has_counterattack_at_ = true;
                }
                break;
            }
            case What::Threatened:
                if (at < threatened_h_) break;
                threatened_ = p.sector;
                threatened_h_ = at;
                threatened_why_ = f.Told();
                threatened_id_ = p.sector >= 0 && static_cast<std::size_t>(p.sector) < line.SectorCount() ? line.Sector(static_cast<std::size_t>(p.sector)).id : std::string();
                has_threatened_at_ = SectorMiddle(line, p.sector, threatened_at_);
                break;
            case What::Raid: {
                const auto& raids = c.ObservedRaids();
                if (p.sector < 0 || static_cast<std::size_t>(p.sector) >= raids.size()) break;
                const airwar::ObservedRaid& r = raids[static_cast<std::size_t>(p.sector)];
                RegimentPicture::Raid out;
                out.h = r.h;
                out.reached_h = at;
                out.at = r.target_pos;
                out.target = r.target;
                out.entry = r.entry;
                out.helicopters = r.helicopters;
                const auto& objs = c.Data().theatre.objects;
                if (r.target_object >= 0 && static_cast<std::size_t>(r.target_object) < objs.size()) {
                    out.object = objs[static_cast<std::size_t>(r.target_object)].id;
                    out.strategic = Strategic(objs[static_cast<std::size_t>(r.target_object)].kind);
                }
                raids_.push_back(out);
                break;
            }
            case What::Wreck:
                radar_wrecks_.push_back({f.subject, p.h, at});
                break;
            default:
                break;
        }
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
            default:
                break;  // O7's, above
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
