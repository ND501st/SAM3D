// simcore/air_war/opportunity.hpp — a flight leader's chain on a target of opportunity (roadmap B6,
// docs/air_battle/06_OPPORTUNITY.md §2-7).
//
// A flight that sees something on the ground it was not sent for runs this chain on what its pilots believe they see
// (their pictures, perception.hpp), never on the truth:
//   seen        the sighting is sure enough (opportunity.min_confidence) and of a kind a pilot can name;
//   identified  he has watched it long enough to say what it is: a log-normal time about opportunity.identify_s
//               (fm_100_2_1_1984: 3-6 s), shorter for an easy type and a calm, skilled pilot; with a small chance he
//               takes it for what it is not (misidentify_day, misidentify_night);
//   authorised  his faction's rules allow it (data/air_war/faction_*.json "opportunity_rules", §2-3.3): his mission's
//               rule (armed reconnaissance, close support by the FAC's designation, a strike reports), the protective
//               reaction (fired on), the Pact's free hunt in its zone, NATO's FSCL, the keep-outs near his own troops,
//               roads and villages, a helicopter's night hunt cleared by the ground;
//   able        weapons for it (a gun, rockets, or bombs no longer owed to a primary target), the fuel above his bingo,
//               no damage, time to spare before a slot, and room: too close and there is no pass before it is behind
//               him (the decision, roll-in and roll-out at his speed, and the release range: nwc_tp6005_1978);
//   worth it    the target's value as he believes it against the threat he has seen around it, times his delivery's
//               exposure;
//   act         ignore, report, a recce pass, an attack or a gun run: the faction's tendency moved by the officer's
//               traits (at most opportunity.trait_bound) and his pilots (opportunity.pilot_bound), times the type's ease.
// After a pass the leader may come round again (Reattack): the faction's readiness, the threat seen, and the passes flown.
// What his pilots claim (Claim) is drawn from what the pass did and the light: claims are the enemy's records, never the
// truth.
//
// Every draw is keyed (em::Stream::Opportunity, the session's seed, the flight, the contact and the step): the chain's
// draws move nothing else.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/em/random.hpp"

namespace simcore {
namespace airwar {

// What the leader believes he sees, from the sighting's class (data/gameplay/perception.json "classes").
enum class OppTarget { Battery, Column, Vehicles, Fixed, Unknown };

inline const char* OppTargetName(OppTarget t) {
    switch (t) {
        case OppTarget::Battery: return "battery";
        case OppTarget::Column: return "column";
        case OppTarget::Vehicles: return "vehicles";
        case OppTarget::Fixed: return "fixed";
        case OppTarget::Unknown: return "unknown";
    }
    return "?";
}
inline OppTarget OppTargetFromName(const std::string& s) {
    if (s == "battery") return OppTarget::Battery;
    if (s == "column") return OppTarget::Column;
    if (s == "vehicles") return OppTarget::Vehicles;
    if (s == "fixed") return OppTarget::Fixed;
    return OppTarget::Unknown;
}
// A ground sighting's `what` (its class while unrecognised) as a target.
inline OppTarget OppTargetOf(const std::string& what) {
    if (what == "osa_battery_silent" || what == "launch_sign") return OppTarget::Battery;
    if (what == "column_moving") return OppTarget::Column;
    if (what == "vehicles_parked") return OppTarget::Vehicles;
    if (what == "bridge_or_works") return OppTarget::Fixed;
    return OppTarget::Unknown;
}
// The weapons' target class of what he believes it is (types.hpp): a battery's launchers and radars light, a column's
// trucks soft, parked vehicles light, a bridge or works hard.
inline TargetClass OppClassOf(OppTarget t) {
    switch (t) {
        case OppTarget::Battery: return TargetClass::Light;
        case OppTarget::Column: return TargetClass::Soft;
        case OppTarget::Vehicles: return TargetClass::Light;
        case OppTarget::Fixed: return TargetClass::Hard;
        case OppTarget::Unknown: break;
    }
    return TargetClass::Soft;
}

enum class OppAct { Ignore, Report, ReccePass, Attack, GunRun, Reattack };

inline const char* OppActName(OppAct a) {
    switch (a) {
        case OppAct::Ignore: return "ignore";
        case OppAct::Report: return "report";
        case OppAct::ReccePass: return "recce_pass";
        case OppAct::Attack: return "attack";
        case OppAct::GunRun: return "gun_run";
        case OppAct::Reattack: return "reattack";
    }
    return "?";
}
inline OppAct OppActFromName(const std::string& s) {
    if (s == "report") return OppAct::Report;
    if (s == "recce_pass") return OppAct::ReccePass;
    if (s == "attack") return OppAct::Attack;
    if (s == "gun_run") return OppAct::GunRun;
    if (s == "reattack") return OppAct::Reattack;
    return OppAct::Ignore;
}

// Where the chain stopped: the last step it did not pass (Act: it passed them all and acted).
enum class OppStep { Seen, Identified, Authorised, Able, Worth, Act };

inline const char* OppStepName(OppStep s) {
    switch (s) {
        case OppStep::Seen: return "seen";
        case OppStep::Identified: return "identified";
        case OppStep::Authorised: return "authorised";
        case OppStep::Able: return "able";
        case OppStep::Worth: return "worth";
        case OppStep::Act: return "act";
    }
    return "?";
}
inline OppStep OppStepFromName(const std::string& s) {
    if (s == "identified") return OppStep::Identified;
    if (s == "authorised") return OppStep::Authorised;
    if (s == "able") return OppStep::Able;
    if (s == "worth") return OppStep::Worth;
    if (s == "act") return OppStep::Act;
    return OppStep::Seen;
}

// The flight's leader as the chain reads him: the officer's traits and quirks (the flight's own officer, else the
// package's leader, else its planner: what the package was briefed to do), and his pilots.
struct OppLeader {
    Traits traits;
    bool press_on = false;      // quirk: presses the attack
    bool by_the_book = false;   // quirk: does what he was briefed and nothing else
    bool loss_shy = false;      // quirk: spares his men
    bool chases_kills = false;  // quirk: wants the kill confirmed, comes round again
    double skill = 0.5;         // the pilots' (0..1)
    double stress = 0.0;        // 0..1
};

// Everything the chain reads: what the flight is and has, and what it believes it sees.
struct OppSituation {
    MissionKind mission = MissionKind::Strike;
    FlightRole role = FlightRole::Strike;
    bool helicopter = false;
    double ease = 0.3;          // the type's (data/air_war/faction_*.json "opportunity")
    double bingo_share = 0.3;
    // What he believes he sees.
    OppTarget target = OppTarget::Unknown;
    double confidence = 0.0;
    double watched_s = 0.0;     // how long he has had it in sight
    double range_m = 0.0;       // from the leader, over the ground
    bool moving = false;
    bool fired_on = false;      // it fired at the flight: a launch's sign or trail began at it
    bool designated = false;    // a FAC or a control post designated it (roadmap B5)
    // Where it is, as he believes it.
    double depth_km = 0.0;          // past the front into our side (his own troops are behind the line)
    bool free_hunt_zone = false;    // the Pact's free hunt is allowed there now (c2.free_hunt: the zone and the phase)
    double road_m = -1.0;           // to the nearest road (< 0: not known)
    double village_m = -1.0;        // to the nearest village (< 0: not known)
    bool night = false;
    // What he has.
    bool primary_due = false;   // his stores are owed to a target still ahead
    bool has_gun = false;
    bool has_rockets = false;
    bool has_bombs = false;
    double fuel_share = 1.0;    // fuel left, of a full load
    bool damaged = false;
    double slot_slack_s = 1e9;  // time he can spare before a slot (a time on target, an escort's place)
    double speed_mps = 200.0;
    double release_m = 1115.0;  // his pass's own release (open-fire) range
    // What it would cost.
    double threat = 0.0;        // 0..1: guns or missiles he has seen firing near it
    double exposure = 0.8;      // his delivery's exposure to the target's own guns (data/gameplay/air_war.json delivery.*)
    // Coming round again.
    int passes = 0;
    bool believed_hit = false;
};

struct OppDecision {
    OppStep stopped = OppStep::Seen;
    OppAct act = OppAct::Ignore;
    bool may = false;
    bool able = false;
    double worth = 0.0;
    double share = 0.0;         // the chance he acts, once allowed, able and it is worth it
    double identify_s = 0.0;
    bool misidentified = false;
    bool too_close = false;     // able but for the room: a later look may find it
    std::string why;
};

// A keyed draw of the chain: the session's seed, the flight, the contact, the step and the draw's own number.
inline double OppDraw(std::uint64_t seed, std::uint64_t flight, std::uint64_t contact, std::uint64_t step, std::uint64_t n) {
    return em::Uniform01(em::DrawHash(seed, em::Stream::Opportunity, flight, contact, step, n));
}
inline double OppNormal(std::uint64_t seed, std::uint64_t flight, std::uint64_t contact, std::uint64_t step, std::uint64_t n) {
    return em::StandardNormal(em::DrawHash(seed, em::Stream::Opportunity, flight, contact, step, n));
}

// Identification after detection: log-normal about opportunity.identify_s; a skilled pilot (x 0.75 at skill 1, 1.25 at
// 0), an easy type (the close-support specialist's crews know what they look at) and a calm one sooner. `z`: a standard
// normal draw.
inline double IdentifyTimeS(const OppSituation& s, const OppLeader& l, double z, const TuningTable& tune) {
    const double median = tune.Get("opportunity.identify_s") * (1.25 - 0.5 * Clamp01(l.skill)) * (1.15 - 0.3 * Clamp01(s.ease)) *
                          (1.0 + 0.5 * Clamp01(l.stress));
    return median * std::exp(tune.Get("opportunity.identify_spread") * z);
}

// The chance he takes it for what it is not, as he identifies it (a dummy for a battery, a civilian lorry for a column).
inline double MisidentifyChance(const OppSituation& s, const OppLeader& l, const TuningTable& tune) {
    const double base = s.night ? tune.Get("opportunity.misidentify_night") : tune.Get("opportunity.misidentify_day");
    return Clamp01(base * (1.3 - 0.6 * Clamp01(l.skill)));
}

// The ground he needs between him and it for a pass from where he is: the decision, roll-in and roll-out at his speed,
// and his release range (never under the 3,000 ft minimum release: nwc_tp6005_1978).
inline double MinimumRunM(double speed_mps, double release_m, const TuningTable& tune) {
    const double turn_s = tune.Get("opportunity.decision_s") + tune.Get("opportunity.roll_in_s") + tune.Get("opportunity.roll_out_s");
    return turn_s * std::max(0.0, speed_mps) + std::max(tune.Get("opportunity.min_release_m"), release_m);
}

// Authorised: his faction's rules (§2-3.3) for this flight on this target. `why`: the rule that decided.
inline bool MayAttack(const OpportunityRulesDef& rules, const OppSituation& s, std::string* why = nullptr) {
    const auto say = [why](bool v, const char* w) {
        if (why != nullptr) *why = w;
        return v;
    };
    if (rules.own_troops_keepout_m > 0.0 && s.depth_km * 1000.0 < rules.own_troops_keepout_m) return say(false, "too near his own troops");
    // The protective reaction: the gun or site that fired on the flight, and nothing else (checo_roe_1973).
    if (s.fired_on && rules.fired_on_may_attack) return say(true, "it fired on the flight");
    if (rules.village_keepout_m > 0.0 && s.village_m >= 0.0 && s.village_m < rules.village_keepout_m) return say(false, "too near a village");
    if (s.helicopter && s.night && rules.night_hunt_clearance == "ground_post" && !s.designated)
        return say(false, "a night hunt waits for the ground post's clearance");
    switch (s.mission) {
        case MissionKind::ArmedRecce:
            if (rules.road_corridor_m > 0.0 && (s.road_m < 0.0 || s.road_m > rules.road_corridor_m)) return say(false, "off the road corridor");
            if (rules.armed_recce == "free_hunt_zone") return s.free_hunt_zone ? say(true, "free hunt in its zone") : say(false, "outside the free hunt's zone");
            if (rules.armed_recce == "near_line") {
                return s.depth_km <= rules.near_line_km ? say(true, "near the line") : say(false, "too far past the line");
            }
            return say(true, "armed reconnaissance of its area");
        case MissionKind::Cas:
        case MissionKind::HeloRaid:
            if (rules.cas == "attack") return say(true, "close support on its own eyes");
            if (s.designated) return say(true, "designated by the controller");
            if (rules.fscl_km > 0.0 && s.depth_km > rules.fscl_km) return say(true, "beyond the fire support coordination line");
            // NATO's A-10 squadrons' own areas at the border (bush_a10_simhq, unverified): the close-support specialist
            // (its type's ease that of the A-10's crews) only.
            if (rules.squadron_area_km > 0.0 && s.ease >= 0.75 && s.depth_km <= rules.squadron_area_km)
                return say(true, "in its squadron's area");
            return say(false, "close support strikes what the controller designates");
        default:
            break;
    }
    if (rules.strike_on_task == "attack") return say(true, "attacks what it finds");
    if (s.designated) return say(true, "designated by a post");
    return say(false, "a flight on another task reports what it sees");
}

// Able: weapons, fuel, damage, time and room. `too_close`: everything but the room (a later look may find it).
inline bool AbleToAttack(const OppSituation& s, const TuningTable& tune, bool* too_close = nullptr, std::string* why = nullptr) {
    const auto say = [why](bool v, const char* w) {
        if (why != nullptr) *why = w;
        return v;
    };
    if (too_close != nullptr) *too_close = false;
    if (s.damaged) return say(false, "damaged");
    const bool own_mission = s.mission == MissionKind::ArmedRecce || s.mission == MissionKind::Cas || s.mission == MissionKind::HeloRaid;
    const bool stores = s.has_gun || s.has_rockets || (s.has_bombs && (!s.primary_due || own_mission));
    if (!stores) return say(false, s.primary_due && s.has_bombs ? "his stores are for the target" : "nothing left to attack with");
    if (s.primary_due && !own_mission && !s.has_gun) return say(false, "his stores are for the target");
    if (s.fuel_share - s.bingo_share < 0.1) return say(false, "near his bingo fuel");
    const double pass_s = (tune.Get("opportunity.run_in_m") + tune.Get("opportunity.recover_m")) / std::max(50.0, s.speed_mps);
    if (s.slot_slack_s < pass_s) return say(false, "no time to spare before his slot");
    if (s.range_m < MinimumRunM(s.speed_mps, s.release_m, tune)) {
        if (too_close != nullptr) *too_close = true;
        return say(false, "too close for a pass");
    }
    return say(true, "able");
}

// Worth it: the value of what he believes it is, by how sure he is, against the threat he has seen around it.
inline double OppValue(OppTarget t) {
    switch (t) {
        case OppTarget::Battery: return 1.0;   // the threat to the whole air campaign (06 §4)
        case OppTarget::Column: return 0.8;    // the follow-on forces (fm_101_5_1_1980)
        case OppTarget::Vehicles: return 0.5;
        case OppTarget::Fixed: return 0.2;     // a bridge needs bombs, not a gun
        case OppTarget::Unknown: break;
    }
    return 0.0;
}
inline double OppWorth(const OppSituation& s, const OppLeader& l, const TuningTable& tune) {
    const double value = OppValue(s.target) * (0.5 + 0.5 * Clamp01(s.confidence));
    double nerve = 1.0 + 0.5 * (0.5 - l.traits.boldness);
    if (l.loss_shy) nerve += 0.3;
    if (l.press_on) nerve -= 0.3;
    const double risk = Clamp01(s.threat) * Clamp01(s.exposure) * std::pow(tune.Get("opportunity.reattack_risk"), std::max(0, s.passes)) *
                        std::max(0.2, nerve);
    return value - risk;
}

// The share of his acting on it, once allowed, able and worth it: the faction's tendency moved by the officer's traits
// (bold and ambitious more, rigid less, at most trait_bound) and his pilots (at most pilot_bound), times the type's ease
// (0.4 + 1.2 x ease: the F-111's crews about half the faction's tendency, the A-10's 1.4 times). On a mission whose task
// is the hunting, kOwnMissionShare moved the same way: the tendency is for targets a flight was not sent for.
constexpr double kOwnMissionShare = 0.85;  // sam3d
inline double ActShare(const OpportunityRulesDef& rules, const OppSituation& s, const OppLeader& l, const TuningTable& tune) {
    const Traits& t = l.traits;
    double traits = ((t.boldness - 0.5) + (t.ambition - 0.5) - (t.rigidity - 0.5)) / 1.5;
    if (l.press_on) traits += 0.5;
    if (l.by_the_book) traits -= 0.5;
    const double trait_move = Clamp(traits, -1.0, 1.0) * tune.Get("opportunity.trait_bound");
    const double pilot_move = Clamp(2.0 * (Clamp01(l.skill) - 0.5) - Clamp01(l.stress), -1.0, 1.0) * tune.Get("opportunity.pilot_bound");
    // Hunting is its task (armed reconnaissance, close support, a helicopter raid): it attacks what it may, as briefed.
    const bool own_mission = s.mission == MissionKind::ArmedRecce || s.mission == MissionKind::Cas || s.mission == MissionKind::HeloRaid;
    if (own_mission) return Clamp01(kOwnMissionShare + trait_move + pilot_move);
    const double moved = Clamp01(rules.tendency + trait_move + pilot_move);
    return Clamp01(moved * (0.4 + 1.2 * Clamp01(s.ease)));
}

// The chain. `seed`, `flight`, `contact`: the keyed draws' (the step is the chain's own). A decision to attack is a gun
// run when the gun is what he has for it, an attack (rockets, bombs) otherwise.
inline OppDecision RunChain(const OpportunityRulesDef& rules, const OppSituation& s, const OppLeader& l, const TuningTable& tune,
                            std::uint64_t seed, std::uint64_t flight, std::uint64_t contact) {
    OppDecision d;
    if (s.target == OppTarget::Unknown || s.confidence < tune.Get("opportunity.min_confidence")) {
        d.why = "not sure what it saw";
        return d;
    }
    d.stopped = OppStep::Identified;
    d.identify_s = IdentifyTimeS(s, l, OppNormal(seed, flight, contact, 1, 0), tune);
    if (s.watched_s < d.identify_s) {
        d.why = "still identifying it";
        return d;
    }
    d.misidentified = OppDraw(seed, flight, contact, 2, 0) < MisidentifyChance(s, l, tune);
    d.stopped = OppStep::Authorised;
    std::string rule;
    d.may = MayAttack(rules, s, &rule);
    if (!d.may) {
        d.act = OppAct::Report;
        d.why = rule;
        return d;
    }
    d.stopped = OppStep::Able;
    d.able = AbleToAttack(s, tune, &d.too_close, &d.why);
    if (!d.able) {
        d.act = OppAct::Report;
        return d;
    }
    d.stopped = OppStep::Worth;
    d.worth = OppWorth(s, l, tune);
    if (d.worth <= 0.0) {
        d.act = OppAct::Report;
        d.why = "not worth the guns around it";
        return d;
    }
    d.stopped = OppStep::Act;
    d.share = ActShare(rules, s, l, tune);
    if (OppDraw(seed, flight, contact, 3, 0) < d.share) {
        d.act = s.has_gun && !s.has_rockets ? OppAct::GunRun : OppAct::Attack;
        d.why = rule;
        return d;
    }
    // Not this time: a pilot unsure what he saw, on an easy type, looks again on a recce pass; the rest report it.
    const bool look = s.confidence < 0.6 && OppDraw(seed, flight, contact, 4, 0) < Clamp01(s.ease);
    d.act = look ? OppAct::ReccePass : OppAct::Report;
    d.why = look ? "goes round to look again" : "reports it and flies on";
    return d;
}

// After a pass: does he come round again? Not past opportunity.max_passes, not without weapons, not once he believes it
// hit unless he chases kills; else his faction's readiness, less for the threat seen, moved by his traits.
inline bool Reattack(const OpportunityRulesDef& rules, const OppSituation& s, const OppLeader& l, const TuningTable& tune, double u) {
    if (s.passes >= static_cast<int>(tune.Get("opportunity.max_passes"))) return false;
    if (!s.has_gun && !s.has_rockets && !s.has_bombs) return false;
    if (s.damaged || s.fuel_share - s.bingo_share < 0.1) return false;
    if (s.believed_hit && !l.chases_kills) return false;
    double p = rules.reattack * (1.0 - 0.7 * Clamp01(s.threat)) + 0.5 * tune.Get("opportunity.trait_bound") * (l.traits.boldness - 0.5);
    if (l.loss_shy) p -= 0.15;
    if (l.chases_kills || l.press_on) p += 0.15;
    return u < Clamp01(p);
}

// What his pilots claim for a pass (afmag_2000_kosovo's overclaims, design reference; 06 §7): a pass that did nothing
// may be claimed a kill (claim_on_miss_*), one that killed may be claimed more (claim_extra_*), more so at night. The
// officer's skepticism weighs the claim later (campaign.hpp BeliefsFrom). `u1`, `u2`: draws.
inline int Claim(int kills, bool night, double u1, double u2, const TuningTable& tune) {
    if (kills <= 0) return u1 < tune.Get(night ? "opportunity.claim_on_miss_night" : "opportunity.claim_on_miss_day") ? 1 : 0;
    const double extra = tune.Get(night ? "opportunity.claim_extra_night" : "opportunity.claim_extra_day");
    const int whole = static_cast<int>(std::floor(extra));
    return kills + whole + (u2 < extra - whole ? 1 : 0);
}

}  // namespace airwar
}  // namespace simcore
