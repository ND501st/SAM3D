// simcore/air_war/opportunity_def.hpp — targets of opportunity as data (roadmap B6, docs/air_battle/06_OPPORTUNITY.md §9).
//
// Three pieces of data the opportunity chain (opportunity.hpp) and the attack passes (attack_pass.hpp) read:
//   * a faction's "opportunity_rules": what its flights may do with what they find (§2-3.3), each value with its source;
//   * an aircraft type's "opportunity" block: how easily its crews act on what they find (`ease`, §4) and the share of
//     fuel they turn home at (`bingo_share`);
//   * a gun's or rockets' "pass" in data/air_war/weapons.json: the pass as it is flown and what one pass does (§5.2).
// The game values are the "opportunity" group of data/gameplay/air_war.json (OpportunityTuningDefaults).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "simcore/json.hpp"

namespace simcore {
namespace airwar {

// A faction's rules (docs/air_battle/06_OPPORTUNITY.md §2 and the table of §3.3). Absent from a faction's file: the
// defaults below (report what is seen, attack nothing unbriefed).
struct OpportunityRulesDef {
    bool present = false;
    // Its base tendency to act on a target of opportunity rather than report it: the faction's hot_phase.opportunity
    // (10_FACTIONS.md §7.2), written here by the faction's loader (roadmap B11 moved it out of this block).
    double tendency = 0.2;
    bool report_in_flight = true;             // its crews report what they see at once by radio; else after landing
    bool fired_on_may_attack = false;         // a flight fired on may hit back at that gun or site, and nothing else
    std::string night_hunt_clearance = "none";  // "ground_post": a helicopter's night hunt strikes once the ground clears it
    double own_troops_keepout_m = 0.0;        // no attack this near where the flight believes its own troops are
    double road_corridor_m = 0.0;             // > 0: armed reconnaissance only this near a road (off by default)
    double village_keepout_m = 0.0;           // > 0: no strike this near a village unless fired on (off by default)
    double reattack = 0.5;                    // 0..1: how readily its pilots come round again
    std::string armed_recce = "attack";       // where an armed reconnaissance strikes: "attack" (its area), "free_hunt_zone"
                                              // (the Pact: only in the plan's zone once the air is held), "near_line"
    std::string strike_on_task = "report";    // a flight on another task: "report" (in flight or at the debrief), "attack"
    std::string cas = "fac";                  // close support: "fac" (only what a FAC or control post designates), "attack"
    double fscl_km = 0.0;                     // > 0: beyond this depth past the front any target may be attacked (NATO)
    double squadron_area_km = 0.0;            // > 0: a close-support squadron's area this deep (NATO's A-10s, unverified)
    double near_line_km = 0.0;                // > 0: "near the line" for armed_recce "near_line" (the neutral)
    // Each value's source, as the file gives it (key, source, confidence).
    struct Cite {
        std::string key;
        std::string source;
        std::string confidence;
    };
    std::vector<Cite> cites;
};

// A gun's or rockets' pass (weapons.json "pass", docs/air_battle/06_OPPORTUNITY.md §5.2). For a gun, one burst on one
// aim point; for rockets, one salvo. `per_pass` is what one pass does to one found, stationary, undefended aim point of
// each target class (the gun: immobilised, of which `catastrophic_share` burnt out).
struct GunPassDef {
    bool present = false;
    double open_fire_m = 1115.0;   // slant range at which it opens fire (or fires the salvo)
    double dive_deg = 3.3;         // the dive it is fired in
    double burst_s = 2.0;          // a gun's burst (rockets: the salvo's ripple)
    int rounds = 135;              // rounds a burst, rockets a salvo
    static constexpr int kClasses = 6;  // types.hpp's TargetClass: soft, light, armour, hard, very_hard, runway
    double per_pass[kClasses] = {};
    double catastrophic_share = 0.0;
    double moving_factor = 0.5;    // a moving target lowers the hit share by this
    std::string source;
    std::string confidence;
    std::string notes;
};

// The "opportunity" group of data/gameplay/air_war.json: every key the chain and the passes read (game values unless the
// file's notes give a source).
inline std::vector<std::pair<std::string, double>> OpportunityTuningDefaults() {
    return {
        {"opportunity.mode", 1.0},                 // 1: roadmap B6's chain and flown passes; 0: before it (a save made before B6)
        {"opportunity.tick_s", 1.0},               // a flight leader runs his chain this often (1-5 Hz), staggered by a fixed key
        {"opportunity.merge_m", 600.0},            // two sightings of a flight this near are the same thing
        {"opportunity.min_confidence", 0.25},      // a sighting less sure than this is not acted on yet
        {"opportunity.identify_s", 4.5},           // identification after detection, centred on 4.5 s (fm_100_2_1_1984: 3-6 s)
        {"opportunity.identify_spread", 0.35},     // its log-normal spread
        {"opportunity.decision_s", 1.0},           // the decision, the roll-in and the roll-out (nwc_tp6005_1978's sample)
        {"opportunity.roll_in_s", 0.5},
        {"opportunity.roll_out_s", 1.0},
        {"opportunity.min_release_m", 914.4},      // 3,000 ft minimum release range (nwc_tp6005_1978)
        {"opportunity.misidentify_day", 0.05},     // the chance of taking a dummy, a civilian lorry or own troops for the target
        {"opportunity.misidentify_night", 0.15},
        {"opportunity.slot_slack_s", 120.0},       // a flight on a time on target or an escort's slot has this much to spare
        {"opportunity.gun_burst_under_fire", 0.6}, // under fire the burst is shorter and opens further out: its effect times this
        {"opportunity.reattack_separation_m", 6000.0},
        {"opportunity.reattack_heading_deg", 60.0},
        {"opportunity.reattack_risk", 1.5},        // each re-attack multiplies the defences' chance on the aircraft by this
        {"opportunity.max_passes", 3.0},
        {"opportunity.trait_bound", 0.2},          // the officer's traits move a share by at most this (factions_cases.md §1)
        {"opportunity.pilot_bound", 0.15},         // the pilots' experience and nerve by at most this
        {"opportunity.threat_radius_m", 5000.0},   // guns or SAMs seen firing this near the target are its threat
        {"opportunity.threat_age_s", 90.0},        // ... for this long
        {"opportunity.claim_on_miss_day", 0.25},   // a pass that hit nothing is claimed as a kill
        {"opportunity.claim_on_miss_night", 0.6},
        {"opportunity.claim_extra_day", 0.3},      // more claimed for each true kill
        {"opportunity.claim_extra_night", 1.0},
        {"opportunity.report_sigma_m", 500.0},     // a pilot's report of a ground target's place
        {"opportunity.run_in_m", 6000.0},          // a pass set up this far out
        {"opportunity.find_m", 300.0},           // on his run he lines up on the real thing this near where he believed it
        {"opportunity.recover_m", 2500.0},         // and recovered this far past the aim
        {"opportunity.trail_m", 1500.0},           // each wingman this much further back on the run in
        {"opportunity.a0_find_scale", 1.0},        // the abstract level's find chance per mover passed, times the type's ease
        {"opportunity.a0_column_hit", 0.12},       // the share of a column's function one attacking aircraft removes in a pass
        {"opportunity.a0_column_guns", 0.02},      // a column's own air defence brings an attacker down
    };
}

namespace detail {

// One rule of a faction's "opportunity_rules": {"value": ..., "source": ..., "confidence": ..., "notes": ...}.
inline bool OpportunityRule(const json::Value& v, const std::string& path, json::Type want, const json::Value*& value,
                            OpportunityRulesDef::Cite& cite, std::string* error) {
    const auto fail = [&](const std::string& what) {
        if (error != nullptr) *error = path + ": " + what;
        return false;
    };
    if (!v.Is(json::Type::Object)) return fail("expected {\"value\", \"source\", \"confidence\"}");
    for (const std::string& k : v.keys) {
        if (k != "value" && k != "source" && k != "confidence" && k != "notes") return fail("unknown key '" + k + "'");
    }
    value = v.Find("value");
    const json::Value* src = v.Find("source");
    const json::Value* conf = v.Find("confidence");
    if (value == nullptr || !value->Is(want)) return fail("a value of the right kind is required");
    if (src == nullptr || !src->Is(json::Type::String) || src->string.empty()) return fail("source required");
    if (conf == nullptr || !conf->Is(json::Type::String) || conf->string.empty()) return fail("confidence required");
    cite.source = src->string;
    cite.confidence = conf->string;
    return true;
}

}  // namespace detail

// A faction's "opportunity_rules" block (strict keys; every value with its source). False with `error` on a fault.
inline bool LoadOpportunityRules(const json::Value& b, OpportunityRulesDef& out, std::string* error) {
    const std::string p = "opportunity_rules";
    if (!b.Is(json::Type::Object)) {
        if (error != nullptr) *error = p + ": expected an object";
        return false;
    }
    struct Num {
        const char* key;
        double* to;
        double lo, hi;
    };
    struct Flag {
        const char* key;
        bool* to;
    };
    struct Word {
        const char* key;
        std::string* to;
        std::vector<std::string> allowed;
    };
    const Num nums[] = {{"own_troops_keepout_m", &out.own_troops_keepout_m, 0.0, 10000.0},
                        {"road_corridor_m", &out.road_corridor_m, 0.0, 10000.0},
                        {"village_keepout_m", &out.village_keepout_m, 0.0, 10000.0},
                        {"reattack", &out.reattack, 0.0, 1.0},
                        {"fscl_km", &out.fscl_km, 0.0, 300.0},
                        {"squadron_area_km", &out.squadron_area_km, 0.0, 100.0},
                        {"near_line_km", &out.near_line_km, 0.0, 300.0}};
    const Flag flags[] = {{"report_in_flight", &out.report_in_flight}, {"fired_on_may_attack", &out.fired_on_may_attack}};
    const Word words[] = {{"night_hunt_clearance", &out.night_hunt_clearance, {"none", "ground_post"}},
                          {"armed_recce", &out.armed_recce, {"attack", "free_hunt_zone", "near_line"}},
                          {"strike_on_task", &out.strike_on_task, {"report", "attack"}},
                          {"cas", &out.cas, {"fac", "attack"}}};
    for (std::size_t i = 0; i < b.keys.size(); ++i) {
        const std::string& key = b.keys[i];
        const json::Value& v = b.items[i];
        const std::string kp = p + "." + key;
        OpportunityRulesDef::Cite cite;
        cite.key = key;
        const json::Value* value = nullptr;
        bool known = false;
        for (const Num& n : nums) {
            if (key != n.key) continue;
            known = true;
            if (!detail::OpportunityRule(v, kp, json::Type::Number, value, cite, error)) return false;
            if (value->number < n.lo || value->number > n.hi) {
                if (error != nullptr) *error = kp + ": out of range";
                return false;
            }
            *n.to = value->number;
        }
        for (const Flag& f : flags) {
            if (key != f.key) continue;
            known = true;
            if (!detail::OpportunityRule(v, kp, json::Type::Bool, value, cite, error)) return false;
            *f.to = value->boolean;
        }
        for (const Word& w : words) {
            if (key != w.key) continue;
            known = true;
            if (!detail::OpportunityRule(v, kp, json::Type::String, value, cite, error)) return false;
            bool ok = false;
            for (const std::string& a : w.allowed) ok = ok || a == value->string;
            if (!ok) {
                if (error != nullptr) *error = kp + ": unknown value '" + value->string + "'";
                return false;
            }
            *w.to = value->string;
        }
        if (!known) {
            if (error != nullptr) *error = kp + ": unknown rule";
            return false;
        }
        out.cites.push_back(cite);
    }
    out.present = true;
    return true;
}

// A weapon's "pass" (weapons.json; a gun's or rockets'). `class_index` maps a target class name to its index (types.hpp
// ParseTargetClass, passed in to keep this header free of the air war's types).
template <typename ClassIndex>
inline bool LoadGunPass(const json::Value& v, const std::string& path, GunPassDef& out, const ClassIndex& class_index, std::string* error) {
    const auto fail = [&](const std::string& what) {
        if (error != nullptr) *error = path + ": " + what;
        return false;
    };
    if (!v.Is(json::Type::Object)) return fail("expected an object");
    for (const std::string& k : v.keys) {
        if (k != "open_fire_m" && k != "dive_deg" && k != "burst_s" && k != "rounds" && k != "per_pass" && k != "catastrophic_share" &&
            k != "moving_factor" && k != "source" && k != "confidence" && k != "notes") {
            return fail("unknown key '" + k + "'");
        }
    }
    const auto num = [&](const char* key, double& to, double lo, double hi) {
        const json::Value* x = v.Find(key);
        if (x == nullptr) return true;
        if (!x->Is(json::Type::Number) || x->number < lo || x->number > hi) return fail(std::string(key) + ": a number in range");
        to = x->number;
        return true;
    };
    double rounds = out.rounds;
    if (!num("open_fire_m", out.open_fire_m, 50.0, 10000.0) || !num("dive_deg", out.dive_deg, 0.0, 60.0) ||
        !num("burst_s", out.burst_s, 0.1, 30.0) || !num("rounds", rounds, 1.0, 5000.0) ||
        !num("catastrophic_share", out.catastrophic_share, 0.0, 1.0) || !num("moving_factor", out.moving_factor, 0.0, 1.0)) {
        return false;
    }
    out.rounds = static_cast<int>(rounds + 0.5);
    const json::Value* pp = v.Find("per_pass");
    if (pp == nullptr || !pp->Is(json::Type::Object)) return fail("per_pass required: {target class: 0..1}");
    for (std::size_t k = 0; k < pp->keys.size(); ++k) {
        const int ci = class_index(pp->keys[k]);
        if (ci < 0 || ci >= GunPassDef::kClasses) return fail("per_pass: unknown target class '" + pp->keys[k] + "'");
        if (!pp->items[k].Is(json::Type::Number) || pp->items[k].number < 0.0 || pp->items[k].number > 1.0) return fail("per_pass: 0..1");
        out.per_pass[ci] = pp->items[k].number;
    }
    const json::Value* src = v.Find("source");
    const json::Value* conf = v.Find("confidence");
    if (src == nullptr || !src->Is(json::Type::String) || src->string.empty()) return fail("source required");
    if (conf == nullptr || !conf->Is(json::Type::String) || conf->string.empty()) return fail("confidence required");
    out.source = src->string;
    out.confidence = conf->string;
    if (const json::Value* n = v.Find("notes")) out.notes = n->Is(json::Type::String) ? n->string : std::string();
    out.present = true;
    return true;
}

}  // namespace airwar
}  // namespace simcore
