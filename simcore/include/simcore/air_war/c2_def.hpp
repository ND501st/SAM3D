// simcore/air_war/c2_def.hpp — the enemy's command in the air as data (roadmap B5, docs/air_battle/05_C2_AND_RETASKING.md
// §4.1-4.4 and §4.9).
//
// A faction file's "c2" block: its command posts (what kind, where on its front, how far its radio reaches), who may do
// what to a flight in the air (the authority table, read by C2Authority::May in c2.hpp), how long each hop of a message
// takes, its alert pools, which missions report in flight, and the slow paths (silence as consent, the age a report may
// have, the nomination cycle, the debrief after landing). Every entry carries its source and confidence; the values no
// source gives are game values and say so. Loaded by data.hpp (LoadC2Block); the game values are TuningTable's "c2.*".
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

// A band of values [lo, hi] (a time, a hold); one is drawn per use from a keyed stream.
struct C2Band {
    double lo = 0.0;
    double hi = 0.0;
    bool Set() const { return hi > 0.0; }
};

// A command post (§4.1). Its place is given on the faction's front, so one faction file serves every theatre: at its
// bases (one post), behind each entry (one post per entry: a corps' or a division's sector), or behind the line's middle.
struct C2PostDef {
    std::string id;
    std::string name;
    std::string kind;          // tasking, support_centre, radar_control, airborne_warning, fac, squadron
    std::string at = "bases";  // "bases", "entries" (one per front entry), "line" (its middle) or "line_points" (one per point)
    double back_km = 0.0;      // behind the entry or the line, toward the bases
    double reach_km = 100.0;   // its radio cover, and its own area (own_corps, own_area, within_allocation)
    double sight_km = 0.0;     // a FAC: how far across the line it sees our troops
    std::string net;           // the net it works on (09_COMMS_AND_INTERCEPT.md)
    int from_year = 0;         // in service from (a later war's posts stay out of the 1985 war)
    std::string source;
    std::string confidence;
    std::string notes;
};

// One line of the authority table (§4.2): a post kind may do an act to the missions listed, where it applies, with
// whose agreement, from a year.
struct C2AuthorityDef {
    std::string post;                   // a post kind
    std::string act;                    // warn, designate, approve, divert_airborne, redirect_before_ip, divert_recovery, scramble, recall
    std::vector<std::string> missions;  // mission names (MissionKindName, "bai", "fighters"), or "any"
    std::string where = "any";          // own_corps, within_allocation, own_area, free_hunt_zone, any
    std::string needs;                  // "army": the corps' agreement (or silence for silence_consent_s)
    double chance = 1.0;                // how often the post does it when it may ("occasionally")
    int from_year = 0;
    std::string source;
    std::string confidence;
    std::string notes;
};

// One hop of a message (§4.3): relay, decide and encode, each a band in seconds.
struct C2HopDef {
    C2Band relay_s;
    C2Band decide_s;
    C2Band encode_s;
    std::string source;
    std::string confidence;
    std::string notes;
};

// An alert pool's kind (§4.4): its share of the held-back sorties, the time to take off and how long it is held.
struct C2AlertDef {
    std::string id;           // airborne, ground_5, ground_15; readiness_1, readiness_2, readiness_3; ground
    double share = 0.0;       // of the sorties held back for on-call work
    C2Band takeoff_s;         // from the order to the wheels up (0 for an airborne pool: in the air)
    C2Band hold_h;            // how long one shift is held
    bool airborne = false;    // airborne alert: no take-off
    std::vector<double> phase_scale;  // per campaign phase (the Pact's on-call share small in the opening operation); 1 past the list
    std::string source;
    std::string confidence;
    std::string notes;
};

// Where free hunting is allowed (the Pact, once its phase holds the air: fm_100_2_1_1984 p. 12-6).
struct C2FreeHuntDef {
    int from_phase = -1;    // -1: never
    double depth_km = 0.0;  // behind our side's line
    std::string source;
    std::string confidence;
};

// What losing a post does (roadmap S2, 05 §9): who takes its traffic and when, and how long the post takes to be set up
// again. "alternate": the nearest live post of the same kind takes its messages after a takeover; "none": messages to it
// are lost, and the flights fly the plan (the Pact's: fm_100_2_1_1984 p. 12-4).
struct C2PostLossDef {
    bool set = false;
    std::string takeover = "none";               // "alternate" or "none"
    C2Band takeover_s;                           // the alternate's time to take the traffic over
    std::map<std::string, C2Band> reconstitute_h;  // post kind -> hours until it works again (unlisted: not in this war)
    std::string source;
    std::string confidence;
    std::string notes;
};

// A strike that needs a controller's clearance or mark (roadmap S2, 05 §9): its missions, and what a flight does when the
// clearance does not reach it (the net jammed or its post gone): "attack" on its plan, "abort", or "hold_then_abort"
// (it holds for hold_s, asking again, then goes home).
struct C2ClearanceDef {
    bool set = false;
    std::vector<std::string> missions;
    std::string on_jammed = "attack";
    C2Band hold_s;
    std::string source;
    std::string confidence;
    std::string notes;
    bool Covers(const std::string& mission) const {
        for (const std::string& m : missions) {
            if (m == mission || m == "any") return true;
        }
        return false;
    }
};

struct FactionC2 {
    bool present = false;
    std::vector<C2PostDef> posts;
    std::vector<C2AuthorityDef> authority;
    std::map<std::string, C2HopDef> timing;    // "report", "request", "army", "order", "scramble"
    std::vector<C2AlertDef> alert;
    std::vector<std::string> report_missions;  // the missions whose flights report in flight when in a post's cover ("any")
    std::string hunter_leave = "none";         // a hunter flight's own leave to attack a reported site: any, free_hunt_zone, none
    C2FreeHuntDef free_hunt;
    double silence_consent_s = 0.0;            // > 0: a request stands unless refused within this (NATO's CAS)
    double max_report_age_h = 6.0;             // a mobile target reported longer ago is not struck without a fresh report
    double nomination_h = 0.0;                 // a nomination into the tasking cycle is flown no sooner
    C2Band debrief_s;                          // the report after landing (unset: the faction's pirep_delay_h)
    C2PostLossDef post_loss;                   // a post destroyed (roadmap S2)
    C2ClearanceDef clearance;                  // a strike's clearance on a jammed net (roadmap S2)
    std::string source;
    std::string confidence;
    std::string notes;

    const C2PostDef* Post(const std::string& id) const {
        for (const C2PostDef& p : posts) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
    const C2HopDef* Hop(const std::string& id) const {
        const auto it = timing.find(id);
        return it == timing.end() ? nullptr : &it->second;
    }
};

inline const std::vector<std::string>& C2PostKinds() {
    static const std::vector<std::string> k = {"tasking", "support_centre", "radar_control", "airborne_warning", "fac", "squadron"};
    return k;
}
inline const std::vector<std::string>& C2Acts() {
    static const std::vector<std::string> k = {"warn", "designate", "approve", "divert_airborne", "redirect_before_ip", "divert_recovery",
                                               "scramble", "recall"};
    return k;
}
inline const std::vector<std::string>& C2Wheres() {
    static const std::vector<std::string> k = {"own_corps", "within_allocation", "own_area", "free_hunt_zone", "any"};
    return k;
}
inline const std::vector<std::string>& C2Hops() {
    static const std::vector<std::string> k = {"report", "request", "army", "order", "scramble"};
    return k;
}

// The game values of the command in the air (data/gameplay/air_war.json "c2"), every one in TuningTable::Defaults.
inline std::vector<std::pair<std::string, double>> C2TuningDefaults() {
    return {
        {"c2.mode", 1.0},                  // 1: B5's command in the air and the packages flown in time; 0: before it (a save of version 1)
        {"c2.step_h", 1.0 / 12.0},          // the campaign's step while a package is in the air or a message on its way
        {"c2.retry_s", 60.0},               // a jammed hop is said again after this
        {"c2.max_retries", 3.0},            // and given up after this many tries
        {"c2.bai_depth_m", 30000.0},        // a strike this near the front is battlefield interdiction (BAI), beyond it AI
        {"c2.trait_bound", 0.2},            // a post officer's traits move its decisions by at most this share
        {"c2.commit_risk", 0.15},           // an alert pool is committed when the on-call package's expected loss share is below this
        {"c2.low_cover", 0.4},              // a post's radio cover of a flight low down (50-100 m), as a share of its reach
        {"c2.fac_sigma_m", 300.0},          // a FAC's sighting of our troops
        {"c2.report_sigma_m", 700.0},       // a pilot's in-flight report of a launch (as the debrief's launch sighting)
        {"c2.hunter_reach_m", 30000.0},     // a hunter flight this near a reported site is told
        {"c2.divert_reach_m", 40000.0},     // an airborne flight this near a reported place may be diverted to it
        {"c2.intercept", 0.3},              // our listeners hear a call near our battery (times the faction's 1 - opsec)
        {"c2.intercept_reach_m", 60000.0},  // how far from our battery a call can be heard
        {"c2.second_report_h", 0.5},        // a methodical officer's wait for a second report of the same place
        {"c2.second_report_m", 3000.0},     // and how near it must be
        {"c2.army_refusal", 0.1},           // the corps refuses a CAS divert
        {"c2.fac_request_per_h", 0.1},      // a FAC with our troops in sight asks for immediate support on them, per hour
        // Roadmap S2, the command live in the session: 1 the posts' orders into a session's flights, recalls, posts struck
        // by our fires, the nomination cycle, our jamming of the enemy's nets and the strike's clearance; 0 before it (a
        // save without the key).
        {"c2.live_mode", 1.0},
        {"c2.recall_live", 0.3},            // a package is recalled when its target, claimed destroyed since it took off, is believed this alive
        {"c2.jam_strong_share", 0.9},       // the share of a hop's tries our jamming takes at a strong J/S (FM capture)
        {"c2.jam_weak_share", 0.35},        // ... and at a weak one (words lost)
        {"c2.post_fix_calls", 3.0},         // calls of a post our listeners must have heard before our fires can be laid on it
        {"c2.post_strike_per_h", 0.04},     // our fires' chance per hour on a located post marked for destruction
        {"c2.post_footprint_m", 600.0},     // a fix this near the post puts the fires on it
        {"c2.fac_contact_m", 25000.0},      // a close support flight in a session asks its FAC for the mark this near him
    };
}

}  // namespace airwar
}  // namespace simcore
