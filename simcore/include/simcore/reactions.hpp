// simcore/reactions.hpp — how a pilot reacts under fire (roadmap B4, docs/air_battle/04_REACTIONS.md): the tables
// and the pure functions. The chain that runs them in a session is reaction_chain.hpp.
//
// A reaction starts from a cue in the pilot's own picture (air_war/perception.hpp: what his eyes and his receiver gave
// him, or a call he heard), never from the truth. Its class is what he believes (a receiver names it from its library,
// the eye by size and smoke, a call as the caller said it). He notices it after a delay set by his skill, the cue's
// source, his workload and his stress, or not at all (the share of no reaction by skill). Then he flies the
// manoeuvre his faction's profile gives for that cue and class, dispenses what his type carries in the war's year, and
// his flight's leader decides once for the flight: jettison (tanks only, or everything), press on, go round the zone,
// or go home.
//
// Every number is in data/gameplay/reactions.json (the tiers, the values, the profiles' rows) and
// data/air_war/countermeasures.json (the fits by type and year), each with its source or labelled a game value.
// Every draw is keyed (em::DrawHash on Stream::Reaction): the order the runner visits aircraft in changes nothing.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/damage.hpp"
#include "simcore/em/random.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// What the pilot believes threatens him (04_REACTIONS.md 3.2). Loss: a wingman hit or going down, the threat unknown.
enum class ThreatClass { Unknown, SamHigh, SamLow, SamIr, FlakLight, FlakHeavy, Fighter, Loss };
// How it reached him. Any: a row for every source.
enum class CueSource { Any, Eye, Rwr, Radio, Damage, Order };
// What he flies (6.1, 6.3): a break onto the beam; a break held until the missile is close (break_late); a break with a
// dive to the ground; a jink; the Pact's preplanned counter-SAM programme (6.2); a break into a fighter; a run away at
// full speed (extend).
enum class Manoeuvre { None, Break, BreakLate, Dive, Jink, Programme, BreakInto, Extend };
enum class Countermeasure { None, Chaff, Flares, Both };
// The flight's decision (9): press on, go round the zone, or go home.
enum class FlightChoice { PressOn, GoRound, Abort };
// When a dispense comes (7, misuse by skill).
enum class Dispense { OnTime, Early, Late, Never };

inline const char* ToString(ThreatClass c) {
    switch (c) {
        case ThreatClass::Unknown: return "unknown";
        case ThreatClass::SamHigh: return "sam_high";
        case ThreatClass::SamLow: return "sam_low";
        case ThreatClass::SamIr: return "sam_ir";
        case ThreatClass::FlakLight: return "flak_light";
        case ThreatClass::FlakHeavy: return "flak_heavy";
        case ThreatClass::Fighter: return "fighter";
        case ThreatClass::Loss: return "loss";
    }
    return "?";
}
inline const char* ToString(CueSource s) {
    switch (s) {
        case CueSource::Any: return "any";
        case CueSource::Eye: return "eye";
        case CueSource::Rwr: return "rwr";
        case CueSource::Radio: return "radio";
        case CueSource::Damage: return "damage";
        case CueSource::Order: return "order";
    }
    return "?";
}
inline const char* ToString(Manoeuvre m) {
    switch (m) {
        case Manoeuvre::None: return "none";
        case Manoeuvre::Break: return "break";
        case Manoeuvre::BreakLate: return "break_late";
        case Manoeuvre::Dive: return "dive";
        case Manoeuvre::Jink: return "jink";
        case Manoeuvre::Programme: return "programme";
        case Manoeuvre::BreakInto: return "break_into";
        case Manoeuvre::Extend: return "extend";
    }
    return "?";
}
inline const char* ToString(Countermeasure c) {
    switch (c) {
        case Countermeasure::None: return "none";
        case Countermeasure::Chaff: return "chaff";
        case Countermeasure::Flares: return "flares";
        case Countermeasure::Both: return "both";
    }
    return "?";
}
inline const char* ToString(FlightChoice c) {
    switch (c) {
        case FlightChoice::PressOn: return "press_on";
        case FlightChoice::GoRound: return "go_round";
        case FlightChoice::Abort: return "abort";
    }
    return "?";
}
inline const char* ToString(Dispense d) {
    switch (d) {
        case Dispense::OnTime: return "on_time";
        case Dispense::Early: return "early";
        case Dispense::Late: return "late";
        case Dispense::Never: return "never";
    }
    return "?";
}

inline bool ParseThreatClass(const std::string& s, ThreatClass& out) {
    for (ThreatClass c : {ThreatClass::Unknown, ThreatClass::SamHigh, ThreatClass::SamLow, ThreatClass::SamIr, ThreatClass::FlakLight,
                          ThreatClass::FlakHeavy, ThreatClass::Fighter, ThreatClass::Loss}) {
        if (s == ToString(c)) {
            out = c;
            return true;
        }
    }
    return false;
}
inline bool ParseCueSource(const std::string& s, CueSource& out) {
    for (CueSource c : {CueSource::Any, CueSource::Eye, CueSource::Rwr, CueSource::Radio, CueSource::Damage, CueSource::Order}) {
        if (s == ToString(c)) {
            out = c;
            return true;
        }
    }
    return false;
}
inline bool ParseManoeuvre(const std::string& s, Manoeuvre& out) {
    for (Manoeuvre m : {Manoeuvre::None, Manoeuvre::Break, Manoeuvre::BreakLate, Manoeuvre::Dive, Manoeuvre::Jink, Manoeuvre::Programme,
                        Manoeuvre::BreakInto, Manoeuvre::Extend}) {
        if (s == ToString(m)) {
            out = m;
            return true;
        }
    }
    return false;
}
inline bool ParseCountermeasure(const std::string& s, Countermeasure& out) {
    for (Countermeasure c : {Countermeasure::None, Countermeasure::Chaff, Countermeasure::Flares, Countermeasure::Both}) {
        if (s == ToString(c)) {
            out = c;
            return true;
        }
    }
    return false;
}

// The SAM classes, which a flight decides on together (one decision per flight for all of them).
inline bool IsSam(ThreatClass c) { return c == ThreatClass::SamHigh || c == ThreatClass::SamLow || c == ThreatClass::SamIr || c == ThreatClass::Unknown; }
inline bool IsFlak(ThreatClass c) { return c == ThreatClass::FlakLight || c == ThreatClass::FlakHeavy; }
// The situation a flight decides once for: SAMs (0), flak (1), fighters (2), a loss (3).
inline int SituationOf(ThreatClass c) { return IsSam(c) ? 0 : IsFlak(c) ? 1 : c == ThreatClass::Fighter ? 2 : 3; }

// ------------------------------------------------------------------------------------------------ the data

// One row of a profile (04_REACTIONS.md 12.1): for a cue's source and class, the manoeuvre, the countermeasure, and the
// flight's shares: jettison anything, of those tanks only, press on, abort (the rest goes round).
struct ReactionRow {
    CueSource cue = CueSource::Any;
    bool any_threat = true;
    ThreatClass threat = ThreatClass::Unknown;
    Manoeuvre manoeuvre = Manoeuvre::Break;
    Countermeasure cm = Countermeasure::None;
    double jettison = 0.0;
    double tanks_only = 0.0;
    double press_on = 1.0;
    double abort = 0.0;
};

// A faction's way of reacting (04_REACTIONS.md 9, 11).
struct ReactionProfile {
    std::string id;
    bool programme = false;    // the Pact's preplanned counter-SAM programme (6.2)
    bool calls = true;         // its pilots call threats on the flight's net (the rebels have few radios)
    bool package_net = false;  // a call is heard by the package's other flights (one strike frequency)
    double skill = 0.6;        // its pilots' skill when the target names none (0 green .. 1 veteran)
    // (The leader's initiative is the faction's hot_phase.initiative since roadmap B11: data/air_war/faction_*.json.)
    std::vector<ReactionRow> rows;
    ReactionRow fallback;
};

// A skill tier (04_REACTIONS.md 4, 7): its place on the skill scale, the delay, the share of no reaction, and the
// dispense misuse.
struct ReactionTier {
    std::string id;
    double skill = 0.0;
    double delay_min_s = 0.0;
    double delay_max_s = 0.0;
    double no_reaction = 0.0;
    double dispense_early = 0.0;
    double dispense_late = 0.0;
    double dispense_never = 0.0;
};

// A countermeasure fit or a drop tank load of a type, from a year (data/air_war/countermeasures.json).
struct CountermeasureFit {
    std::string type;  // the aircraft's model id (data/air_war/faction_*.json)
    std::string kind;  // "chaff", "flares", "drop_tanks"
    int count = 0;
    int in_service_year = 0;
    bool automatic = false;  // started by the warning receiver itself (not used in the 1985 war: undated)
};

// What one aircraft carries in a war of a year.
struct Loadout {
    int chaff = 0;
    int flares = 0;
    int tanks = 0;
};

struct ReactionTable {
    bool loaded = false;
    std::vector<ReactionTier> tiers;  // ascending skill
    // How the cue's source moves the delay (factors on it).
    double rwr_factor = 0.7;
    double eye_factor = 1.0;
    double radio_factor = 1.0;
    double damage_factor = 0.5;
    // Workload: low and fast, or in the attack run (agard_cp267_1979); a second seat shares the lookout.
    double workload_factor = 1.5;
    double workload_agl_m = 150.0;
    double workload_mps = 200.0;
    double two_seat_factor = 0.8;
    // Stress: what raises it, how fast it decays, what it does.
    double stress_launch = 0.4;
    double stress_lock = 0.2;
    double stress_search = 0.05;
    double stress_flak = 0.15;
    double stress_fighter = 0.4;
    double stress_loss = 0.5;
    double stress_damage = 0.6;
    double stress_call = 0.15;
    double stress_half_life_s = 120.0;
    double stress_delay_gain = 0.5;
    double stress_no_reaction_gain = 1.0;
    double stress_jettison_gain = 0.3;
    double stress_abort_gain = 0.3;
    double nerve_shift = 0.3;  // a pilot's nerve moves press-on and abort by +-nerve_shift/2 (10_FACTIONS.md 8: about 0.15)
    // The call (5).
    double call_bearing_sigma_deg = 15.0;
    double package_call_range_m = 50000.0;
    // The manoeuvres (6).
    double refractory_s = 20.0;
    double break_turn_deg_s = 12.0;
    double break_s = 15.0;
    double break_into_s = 10.0;
    double dive_agl_m = 60.0;
    double dive_s = 25.0;
    double jink_s = 20.0;
    double jink_min_s = 2.0;
    double jink_max_s = 8.0;
    double late_missile_mps = 500.0;
    double late_lead_s = 4.0;
    double late_max_wait_s = 12.0;
    double programme_margin_m = 2000.0;
    double programme_max_s = 60.0;
    double extend_s = 30.0;
    double extend_m = 30000.0;
    // Going round: the believed zone's radius by class, and the margin kept from it.
    double go_round_sam_high_m = 25000.0;
    double go_round_sam_low_m = 12000.0;
    double go_round_sam_ir_m = 5000.0;
    double go_round_flak_light_m = 3000.0;
    double go_round_flak_heavy_m = 10000.0;
    double go_round_margin_m = 3000.0;
    // Countermeasures (7).
    double chaff_salvo = 4.0;
    double flare_salvo = 2.0;
    double chaff_rcs_m2 = 30.0;
    double chaff_life_s = 60.0;
    double chaff_factor = 0.75;
    double chaff_window_s = 10.0;
    double flare_decoy = 0.5;
    double flare_window_s = 6.0;
    double dispense_late_s = 3.0;
    // The flight's decision (9).
    double decision_wait_s = 8.0;
    double loss_seen_m = 5000.0;
    // A receiver's names to classes (substring of the library name), and our side's fighters by model id.
    std::vector<std::pair<std::string, ThreatClass>> rwr_classes;
    std::vector<std::string> fighter_models;
    std::map<std::string, ReactionProfile> profiles;
    std::vector<CountermeasureFit> fits;

    const ReactionProfile* Profile(const std::string& id) const {
        const auto it = profiles.find(id);
        return it != profiles.end() ? &it->second : nullptr;
    }
    // The class a receiver's name gives (Unknown: no name, or none the table knows).
    ThreatClass RwrClass(const std::string& name) const {
        for (const auto& c : rwr_classes) {
            if (name.find(c.first) != std::string::npos) return c.second;
        }
        return ThreatClass::Unknown;
    }
    bool IsFighterModel(const std::string& model) const {
        return std::find(fighter_models.begin(), fighter_models.end(), model) != fighter_models.end();
    }
    double GoRoundRadiusM(ThreatClass c) const {
        switch (c) {
            case ThreatClass::SamHigh: return go_round_sam_high_m;
            case ThreatClass::SamLow:
            case ThreatClass::Unknown: return go_round_sam_low_m;
            case ThreatClass::SamIr: return go_round_sam_ir_m;
            case ThreatClass::FlakLight: return go_round_flak_light_m;
            case ThreatClass::FlakHeavy: return go_round_flak_heavy_m;
            case ThreatClass::Fighter:
            case ThreatClass::Loss: return 0.0;
        }
        return 0.0;
    }
    // What a type carries in a war of `year`: per kind, the latest fit in service by then.
    Loadout LoadoutOf(const std::string& type, int year) const {
        Loadout l;
        int chaff_y = -1, flares_y = -1, tanks_y = -1;
        for (const CountermeasureFit& f : fits) {
            if (f.type != type || f.in_service_year > year || f.automatic) continue;
            if (f.kind == "chaff" && f.in_service_year > chaff_y) {
                chaff_y = f.in_service_year;
                l.chaff = f.count;
            } else if (f.kind == "flares" && f.in_service_year > flares_y) {
                flares_y = f.in_service_year;
                l.flares = f.count;
            } else if (f.kind == "drop_tanks" && f.in_service_year > tanks_y) {
                tanks_y = f.in_service_year;
                l.tanks = f.count;
            }
        }
        return l;
    }
};

// A believed SAM zone the flight's counter-SAM programme was planned against (6.2): from the enemy's intelligence when
// the raid was planned, never the truth.
struct ProgrammeZone {
    Vec3 centre;  // ENU [m], the scenario's
    double radius_m = 0.0;
};

// A target's "reactions" block (a scenario; generated raids and the testbed write it in place of "evade").
struct ReactionPilotDef {
    bool on = false;
    std::string profile;              // a profile of gameplay/reactions.json (its faction's)
    std::string aircraft;             // its type's model id: its countermeasures and tanks by the war's year
    int chaff = -1, flares = -1, tanks = -1;  // what it carries (< 0: by its type and the year)
    std::string stores;               // the load it carries, as jettisoned ("" none: an unarmed type)
    double nerve = 0.5;               // the pilot's nerve, 0..1 (0.5 his faction's own)
    // His officer's shifts of the faction's press-on and jettison readiness (roadmap B11, air_war/hot_phase.hpp
    // HotOfficerShift), handed over with the flight: they move the row's shares by the same amount (10_FACTIONS.md §7.1).
    double press_on_shift = 0.0;
    double jettison_shift = 0.0;
    std::vector<ProgrammeZone> zones;  // the Pact's programme: the zones briefed
    // His faction's hot-phase values as his flight reads them (roadmap S5, 10_FACTIONS.md §7.1; air_war/hot_phase.hpp,
    // moved by his officer and his squadron's pilots), handed over with the flight. Absent (`hot` false): none of it is
    // read, and he reacts as before S5.
    struct Hot {
        bool on = false;
        double initiative = 0.0;      // his leader acts on his own on a cue the plan does not cover (goes round a site)
        double plan_adherence = 1.0;  // the share of a disruption the plan absorbs; the time a detour cost made up
        double turn_back = 0.0;       // he turns back when he knows he is spotted (of what the plan does not absorb)
        bool may_route = true;        // his leader may change the route himself (§7.3 leader_may: route, everything)
        bool may_turn_back = true;    // ... and turn back on his own call (abort, break_off, everything)
        bool may_programme = true;    // ... and fly the preset counter-SAM programme (programme)
        double delay_lo_s = -1.0;     // his faction's trained band, cue to manoeuvre, moved (< 0: none)
        double delay_hi_s = -1.0;
        double improvised_extra_s = 0.0;  // what a manoeuvre off the programme adds
        double husbanding_shift = 0.0;    // press-on fallen with the day's losses (<= 0), beside the officer's
    } hot;
};

// ------------------------------------------------------------------------------------------------ the pure functions

// A keyed draw, 0..1: the aircraft's key, the cue's serial, what it decides. Initiative and TurnBack are roadmap S5's,
// drawn only for a pilot with "hot" values.
enum class ReactionDraw : std::uint64_t { NoReaction = 1, Delay, Dispense, Jettison, TanksOnly, Choice, Bearing, Flare, Row, Programme, Initiative, TurnBack };
inline double ReactionU(std::uint64_t seed, std::uint64_t aircraft, std::uint64_t serial, ReactionDraw what) {
    return em::Uniform01(em::DrawHash(seed, em::Stream::Reaction, aircraft, serial, static_cast<std::uint64_t>(what)));
}
inline double ReactionN(std::uint64_t seed, std::uint64_t aircraft, std::uint64_t serial, ReactionDraw what) {
    return em::StandardNormal(em::DrawHash(seed, em::Stream::Reaction, aircraft, serial, static_cast<std::uint64_t>(what)));
}

// The tier values at a skill, linear between the tiers' skills, flat beyond.
inline ReactionTier TierAt(const ReactionTable& t, double skill) {
    if (t.tiers.empty()) return ReactionTier{};
    if (skill <= t.tiers.front().skill) return t.tiers.front();
    if (skill >= t.tiers.back().skill) return t.tiers.back();
    for (std::size_t i = 0; i + 1 < t.tiers.size(); ++i) {
        const ReactionTier& a = t.tiers[i];
        const ReactionTier& b = t.tiers[i + 1];
        if (skill > b.skill) continue;
        const double f = (skill - a.skill) / std::max(1e-9, b.skill - a.skill);
        const auto lerp = [f](double x, double y) { return x + (y - x) * f; };
        ReactionTier r;
        r.id = f < 0.5 ? a.id : b.id;
        r.skill = skill;
        r.delay_min_s = lerp(a.delay_min_s, b.delay_min_s);
        r.delay_max_s = lerp(a.delay_max_s, b.delay_max_s);
        r.no_reaction = lerp(a.no_reaction, b.no_reaction);
        r.dispense_early = lerp(a.dispense_early, b.dispense_early);
        r.dispense_late = lerp(a.dispense_late, b.dispense_late);
        r.dispense_never = lerp(a.dispense_never, b.dispense_never);
        return r;
    }
    return t.tiers.back();
}

// What sets the delay besides the skill.
struct NoticeContext {
    CueSource source = CueSource::Eye;
    bool workload = false;   // low and fast, or in the attack run
    bool two_seat = false;
    double stress = 0.0;     // 0..1
    bool programmed = false; // a cue his flight's programme covers: flown at once, the veteran's delay whatever his skill
    // His faction's (roadmap S5, hot_phase.hpp ReactionDelayS): its trained pilot's band over the tables' trained tier, and
    // what a manoeuvre off the programme adds. 1 and 0 without "hot" values.
    double faction_scale = 1.0;
    double improvised_extra_s = 0.0;
};

// A faction's trained band (10_FACTIONS.md §7.2, moved by the officer and the pilot's experience) as a factor on the
// tables' delay: its middle over the trained tier's middle. 1 without a band or a trained tier.
inline double FactionDelayScale(const ReactionTable& t, double lo_s, double hi_s) {
    if (lo_s < 0.0 || hi_s < lo_s) return 1.0;
    for (const ReactionTier& r : t.tiers) {
        if (r.id != "trained") continue;
        const double mid = 0.5 * (r.delay_min_s + r.delay_max_s);
        return mid > 0.0 ? 0.5 * (lo_s + hi_s) / mid : 1.0;
    }
    return 1.0;
}

// Seconds from the cue to the manoeuvre (04_REACTIONS.md 4): the skill's range drawn by `u`, then the source's, the
// workload's, the second seat's and the stress's factors. A programmed cue: the most skilled tier's range, no factor.
// Roadmap S5: the faction's scale on the skill's range, and an improvised manoeuvre's extra (not on damage or an order).
inline double NoticeDelayS(const ReactionTable& t, double skill, const NoticeContext& c, double u) {
    if (c.programmed && !t.tiers.empty()) {
        const ReactionTier& v = t.tiers.back();
        return v.delay_min_s + (v.delay_max_s - v.delay_min_s) * u;
    }
    const ReactionTier tier = TierAt(t, skill);
    double d = tier.delay_min_s + (tier.delay_max_s - tier.delay_min_s) * u;
    if (c.faction_scale != 1.0) d *= c.faction_scale;
    if (c.improvised_extra_s > 0.0 && c.source != CueSource::Damage && c.source != CueSource::Order) d += c.improvised_extra_s;
    switch (c.source) {
        case CueSource::Rwr: d *= t.rwr_factor; break;
        case CueSource::Radio: d *= t.radio_factor; break;
        case CueSource::Damage: d *= t.damage_factor; break;
        case CueSource::Order: d = 0.0; break;
        case CueSource::Eye:
        case CueSource::Any: d *= t.eye_factor; break;
    }
    if (c.workload) d *= t.workload_factor;
    if (c.two_seat) d *= t.two_seat_factor;
    d *= 1.0 + t.stress_delay_gain * std::clamp(c.stress, 0.0, 1.0);
    return d;
}

// The chance he does not react to a warning at all: the skill's share, raised by stress the more the greener he is.
// Damage, an order and a programmed cue always get a reaction.
inline double NoReactionChance(const ReactionTable& t, double skill, const NoticeContext& c) {
    if (c.source == CueSource::Damage || c.source == CueSource::Order || c.programmed) return 0.0;
    const ReactionTier tier = TierAt(t, skill);
    const double s = std::clamp(c.stress, 0.0, 1.0);
    return std::clamp(tier.no_reaction * (1.0 + t.stress_no_reaction_gain * s * (1.0 - std::clamp(skill, 0.0, 1.0))), 0.0, 1.0);
}

// The profile's row for a cue (12.1): its row for the source and the class, else for the class with any source, else for
// any class from the source, else its fallback.
inline const ReactionRow& ChooseRow(const ReactionProfile& p, CueSource source, ThreatClass threat) {
    for (const ReactionRow& r : p.rows) {
        if (r.cue == source && !r.any_threat && r.threat == threat) return r;
    }
    for (const ReactionRow& r : p.rows) {
        if (r.cue == CueSource::Any && !r.any_threat && r.threat == threat) return r;
    }
    for (const ReactionRow& r : p.rows) {
        if (r.cue == source && r.any_threat) return r;
    }
    return p.fallback;
}

// The flight's decision (8, 9), drawn once by its leader: jettison (and of it tanks only), then press on, abort or go
// round. Stress raises the jettison and abort shares; the leader's nerve (0..1, 0.5 the faction's own) moves press on
// up and abort down by up to nerve_shift / 2.
struct FlightDecision {
    bool jettison = false;
    bool tanks_only = false;
    FlightChoice choice = FlightChoice::PressOn;
};
// A row moved by the flight's officer (10_FACTIONS.md §7.1: the officer's shift of press_on and jettison_readiness moves the
// situation's shares by the same amount; what he takes from abort goes to press on).
// The chance a flight that knows it is spotted (its leader's receiver shows him searched for, before anyone fires) turns
// back (roadmap S5, 10_FACTIONS.md §7.1): the plan absorbs plan_adherence of the disruption; of the rest, turn_back. 0
// when its leader may not turn back on his own call (the Pact's: ground control decides), or without "hot" values.
inline double SpottedTurnBackChance(const ReactionPilotDef::Hot& h) {
    if (!h.on || !h.may_turn_back) return 0.0;
    return std::clamp(h.turn_back, 0.0, 1.0) * (1.0 - std::clamp(h.plan_adherence, 0.0, 1.0));
}

inline ReactionRow ShiftedRow(const ReactionRow& r, double press_on_shift, double jettison_shift) {
    ReactionRow out = r;
    if (press_on_shift == 0.0 && jettison_shift == 0.0) return out;
    out.press_on = std::clamp(r.press_on + press_on_shift, 0.0, 1.0);
    out.abort = std::clamp(r.abort - press_on_shift, 0.0, 1.0);
    out.jettison = std::clamp(r.jettison + jettison_shift, 0.0, 1.0);
    return out;
}

inline FlightDecision DecideFlight(const ReactionTable& t, const ReactionRow& r, double stress, double nerve, double u_jettison,
                                   double u_tanks, double u_choice) {
    FlightDecision d;
    const double s = std::clamp(stress, 0.0, 1.0);
    const double jet = std::clamp(r.jettison * (1.0 + t.stress_jettison_gain * s), 0.0, 1.0);
    d.jettison = u_jettison < jet;
    d.tanks_only = d.jettison && u_tanks < r.tanks_only;
    const double shift = t.nerve_shift * (std::clamp(nerve, 0.0, 1.0) - 0.5);
    double press = std::clamp(r.press_on + shift, 0.0, 1.0);
    double abort = std::clamp(r.abort * (1.0 + t.stress_abort_gain * s) - shift, 0.0, 1.0);
    if (press + abort > 1.0) {
        const double k = 1.0 / (press + abort);
        press *= k;
        abort *= k;
    }
    d.choice = u_choice < press ? FlightChoice::PressOn : u_choice < press + abort ? FlightChoice::Abort : FlightChoice::GoRound;
    return d;
}

// Stress after an event adds `add` (saturating toward 1), and after `dt_s` of decay.
inline double StressAdd(double stress, double add) {
    const double s = std::clamp(stress, 0.0, 1.0);
    return s + (1.0 - s) * std::clamp(add, 0.0, 1.0);
}
inline double StressDecay(double stress, double dt_s, double half_life_s) {
    if (dt_s <= 0.0 || half_life_s <= 0.0) return stress;
    return stress * std::pow(0.5, dt_s / half_life_s);
}

// When a dispense comes, by the skill's misuse shares (7): never, early, late, else on time.
inline Dispense DrawDispense(const ReactionTable& t, double skill, double u) {
    const ReactionTier tier = TierAt(t, skill);
    if (u < tier.dispense_never) return Dispense::Never;
    if (u < tier.dispense_never + tier.dispense_early) return Dispense::Early;
    if (u < tier.dispense_never + tier.dispense_early + tier.dispense_late) return Dispense::Late;
    return Dispense::OnTime;
}

// A jink's period against flak (checo_1970_tactics p. 21): every 2/3 x the height in thousands of feet seconds, held
// to the table's limits.
inline double JinkPeriodS(const ReactionTable& t, double agl_m) {
    const double kft = std::max(0.0, agl_m) / 304.8;
    return std::clamp(2.0 / 3.0 * kft, t.jink_min_s, t.jink_max_s);
}

// How long a pilot waits before his late pull (6.1, werrell_2005 p. 120): the missile's believed time to him less the
// lead he pulls at, held to 0..late_max_wait_s. `range_m` is his estimate of the range to the launch.
inline double LateBreakWaitS(const ReactionTable& t, double range_m) {
    if (range_m <= 0.0 || t.late_missile_mps <= 0.0) return 0.0;
    return std::clamp(range_m / t.late_missile_mps - t.late_lead_s, 0.0, t.late_max_wait_s);
}

// The clock code of a bearing seen from a heading (1..12).
inline int ClockOf(double bearing_rad, double heading_rad) {
    double rel = std::fmod(bearing_rad - heading_rad, 2.0 * kPi);
    if (rel < 0.0) rel += 2.0 * kPi;
    int h = static_cast<int>(std::lround(rel / (kPi / 6.0)));
    if (h <= 0) h = 12;
    return h > 12 ? h - 12 : h;
}

// ------------------------------------------------------------------------------------------------ the loader

namespace detail {

class ReactionLoader : public SourcedValuesLoader {
public:
    bool Load(const json::Value& root, ReactionTable& out) {
        if (!Keys(root, "", {"reactions_id", "status", "notes", "source", "confidence", "tiers", "values", "rwr_classes", "fighter_models",
                             "profiles"}) ||
            !Provenance(root, "")) {
            return false;
        }
        if (!LoadTiers(root, out) || !LoadValues(root, out)) return false;
        const json::Value* rc = OptionalArray(root, "", "rwr_classes");
        if (rc == nullptr) return message_empty() ? Fail("rwr_classes", "required") : false;
        for (std::size_t i = 0; i < rc->items.size(); ++i) {
            const std::string p = Index("rwr_classes", i);
            const json::Value& r = rc->items[i];
            std::string match, cls;
            if (!Keys(r, p, {"match", "class", "notes"}) || !RequireString(r, p, "match", match) || !RequireString(r, p, "class", cls)) return false;
            ThreatClass c;
            if (!ParseThreatClass(cls, c)) return Fail(Join(p, "class"), "unknown class '" + cls + "'");
            out.rwr_classes.push_back({match, c});
        }
        const json::Value* fm = OptionalArray(root, "", "fighter_models");
        if (fm == nullptr) return message_empty() ? Fail("fighter_models", "required") : false;
        for (std::size_t i = 0; i < fm->items.size(); ++i) {
            if (!fm->items[i].Is(json::Type::String)) return Fail(Index("fighter_models", i), "a model id");
            out.fighter_models.push_back(fm->items[i].string);
        }
        const json::Value* ps = root.Find("profiles");
        if (ps == nullptr || !ps->Is(json::Type::Object) || ps->keys.empty()) return Fail("profiles", "required, an object of profiles");
        for (std::size_t i = 0; i < ps->keys.size(); ++i) {
            ReactionProfile pr;
            pr.id = ps->keys[i];
            if (!LoadProfile(ps->items[i], Join("profiles", pr.id), pr)) return false;
            out.profiles[pr.id] = std::move(pr);
        }
        out.loaded = true;
        return true;
    }

    bool LoadFits(const json::Value& root, ReactionTable& out) {
        if (!Keys(root, "", {"countermeasures_id", "status", "notes", "source", "confidence", "fits"}) || !Provenance(root, "")) return false;
        const json::Value* fs = OptionalArray(root, "", "fits");
        if (fs == nullptr) return message_empty() ? Fail("fits", "required") : false;
        for (std::size_t i = 0; i < fs->items.size(); ++i) {
            const std::string p = Index("fits", i);
            const json::Value& f = fs->items[i];
            CountermeasureFit fit;
            double count = 0.0, year = 0.0;
            if (!Keys(f, p, {"type", "kind", "count", "in_service_year", "automatic", "source", "confidence", "notes"}) || !Provenance(f, p) ||
                !RequireString(f, p, "type", fit.type) || !RequireString(f, p, "kind", fit.kind) || !RequireNumber(f, p, "count", count) ||
                !RequireNumber(f, p, "in_service_year", year) || !OptionalBool(f, p, "automatic", fit.automatic)) {
                return false;
            }
            if (fit.kind != "chaff" && fit.kind != "flares" && fit.kind != "drop_tanks") return Fail(Join(p, "kind"), "chaff, flares or drop_tanks");
            if (count < 0.0 || count != std::floor(count)) return Fail(Join(p, "count"), "a whole number >= 0");
            if (year < 1900.0 || year > 2100.0 || year != std::floor(year)) return Fail(Join(p, "in_service_year"), "a year");
            fit.count = static_cast<int>(count);
            fit.in_service_year = static_cast<int>(year);
            out.fits.push_back(fit);
        }
        return true;
    }

private:
    bool message_empty() const { return Message().empty(); }

    bool LoadTiers(const json::Value& root, ReactionTable& out) {
        const json::Value* ts = root.Find("tiers");
        if (ts == nullptr || !ts->Is(json::Type::Object) || ts->keys.empty()) return Fail("tiers", "required, an object of skill tiers");
        for (std::size_t i = 0; i < ts->keys.size(); ++i) {
            const std::string p = Join("tiers", ts->keys[i]);
            const json::Value& v = ts->items[i];
            if (!Keys(v, p, {"skill", "delay_min_s", "delay_max_s", "no_reaction", "dispense_early", "dispense_late", "dispense_never", "notes"})) {
                return false;
            }
            ReactionTier t;
            t.id = ts->keys[i];
            if (!Values(v, p, {{"skill", &t.skill}, {"delay_min_s", &t.delay_min_s}, {"delay_max_s", &t.delay_max_s}, {"no_reaction", &t.no_reaction},
                               {"dispense_early", &t.dispense_early}, {"dispense_late", &t.dispense_late}, {"dispense_never", &t.dispense_never}})) {
                return false;
            }
            if (t.skill > 1.0) return Fail(Join(p, "skill"), "0 to 1");
            if (t.delay_max_s < t.delay_min_s) return Fail(Join(p, "delay_max_s"), "at least delay_min_s");
            if (t.no_reaction > 1.0 || t.dispense_early + t.dispense_late + t.dispense_never > 1.0) return Fail(p, "chances, the dispense shares at most 1 together");
            out.tiers.push_back(t);
        }
        std::stable_sort(out.tiers.begin(), out.tiers.end(), [](const ReactionTier& a, const ReactionTier& b) { return a.skill < b.skill; });
        for (std::size_t i = 1; i < out.tiers.size(); ++i) {
            if (out.tiers[i].skill == out.tiers[i - 1].skill) return Fail("tiers", "two tiers at one skill");
        }
        return true;
    }

    bool LoadValues(const json::Value& root, ReactionTable& t) {
        const json::Value* v = root.Find("values");
        if (v == nullptr) return Fail("values", "required");
        const std::initializer_list<std::pair<const char*, double*>> each = {
            {"rwr_factor", &t.rwr_factor},
            {"eye_factor", &t.eye_factor},
            {"radio_factor", &t.radio_factor},
            {"damage_factor", &t.damage_factor},
            {"workload_factor", &t.workload_factor},
            {"workload_agl_m", &t.workload_agl_m},
            {"workload_mps", &t.workload_mps},
            {"two_seat_factor", &t.two_seat_factor},
            {"stress_launch", &t.stress_launch},
            {"stress_lock", &t.stress_lock},
            {"stress_search", &t.stress_search},
            {"stress_flak", &t.stress_flak},
            {"stress_fighter", &t.stress_fighter},
            {"stress_loss", &t.stress_loss},
            {"stress_damage", &t.stress_damage},
            {"stress_call", &t.stress_call},
            {"stress_half_life_s", &t.stress_half_life_s},
            {"stress_delay_gain", &t.stress_delay_gain},
            {"stress_no_reaction_gain", &t.stress_no_reaction_gain},
            {"stress_jettison_gain", &t.stress_jettison_gain},
            {"stress_abort_gain", &t.stress_abort_gain},
            {"nerve_shift", &t.nerve_shift},
            {"call_bearing_sigma_deg", &t.call_bearing_sigma_deg},
            {"package_call_range_m", &t.package_call_range_m},
            {"refractory_s", &t.refractory_s},
            {"break_turn_deg_s", &t.break_turn_deg_s},
            {"break_s", &t.break_s},
            {"break_into_s", &t.break_into_s},
            {"dive_agl_m", &t.dive_agl_m},
            {"dive_s", &t.dive_s},
            {"jink_s", &t.jink_s},
            {"jink_min_s", &t.jink_min_s},
            {"jink_max_s", &t.jink_max_s},
            {"late_missile_mps", &t.late_missile_mps},
            {"late_lead_s", &t.late_lead_s},
            {"late_max_wait_s", &t.late_max_wait_s},
            {"programme_margin_m", &t.programme_margin_m},
            {"programme_max_s", &t.programme_max_s},
            {"extend_s", &t.extend_s},
            {"extend_m", &t.extend_m},
            {"go_round_sam_high_m", &t.go_round_sam_high_m},
            {"go_round_sam_low_m", &t.go_round_sam_low_m},
            {"go_round_sam_ir_m", &t.go_round_sam_ir_m},
            {"go_round_flak_light_m", &t.go_round_flak_light_m},
            {"go_round_flak_heavy_m", &t.go_round_flak_heavy_m},
            {"go_round_margin_m", &t.go_round_margin_m},
            {"chaff_salvo", &t.chaff_salvo},
            {"flare_salvo", &t.flare_salvo},
            {"chaff_rcs_m2", &t.chaff_rcs_m2},
            {"chaff_life_s", &t.chaff_life_s},
            {"chaff_factor", &t.chaff_factor},
            {"chaff_window_s", &t.chaff_window_s},
            {"flare_decoy", &t.flare_decoy},
            {"flare_window_s", &t.flare_window_s},
            {"dispense_late_s", &t.dispense_late_s},
            {"decision_wait_s", &t.decision_wait_s},
            {"loss_seen_m", &t.loss_seen_m}};
        for (const std::string& k : v->keys) {
            bool known = false;
            for (const auto& e : each) known = known || k == e.first;
            if (!known) return Fail(Join("values", k), "unknown key");
        }
        if (!Values(*v, "values", each)) return false;
        for (const std::pair<const char*, double> c : {std::make_pair("stress_launch", t.stress_launch), std::make_pair("stress_lock", t.stress_lock),
                                                       std::make_pair("stress_search", t.stress_search), std::make_pair("stress_flak", t.stress_flak),
                                                       std::make_pair("stress_fighter", t.stress_fighter), std::make_pair("stress_loss", t.stress_loss),
                                                       std::make_pair("stress_damage", t.stress_damage), std::make_pair("stress_call", t.stress_call),
                                                       std::make_pair("chaff_factor", t.chaff_factor), std::make_pair("flare_decoy", t.flare_decoy)}) {
            if (c.second > 1.0) return Fail(Join("values", c.first), "0 to 1");
        }
        if (t.jink_max_s < t.jink_min_s) return Fail("values.jink_max_s", "at least jink_min_s");
        if (t.stress_half_life_s <= 0.0 || t.break_turn_deg_s <= 0.0) return Fail("values", "stress_half_life_s and break_turn_deg_s must be > 0");
        return true;
    }

    bool LoadRow(const json::Value& r, const std::string& p, ReactionRow& row, bool fallback) {
        if (!Keys(r, p, {"cue", "threat", "manoeuvre", "countermeasure", "jettison", "tanks_only", "press_on", "abort", "source", "confidence", "notes"}) ||
            !Provenance(r, p)) {
            return false;
        }
        std::string cue = "any", threat = "any", man, cm = "none";
        if (!OptionalString(r, p, "cue", cue) || !OptionalString(r, p, "threat", threat) || !RequireString(r, p, "manoeuvre", man) ||
            !OptionalString(r, p, "countermeasure", cm) || !RequireNumber(r, p, "jettison", row.jettison) ||
            !OptionalNumber(r, p, "tanks_only", row.tanks_only) || !RequireNumber(r, p, "press_on", row.press_on) ||
            !RequireNumber(r, p, "abort", row.abort)) {
            return false;
        }
        if (fallback && (cue != "any" || threat != "any")) return Fail(p, "the fallback is for any cue and any threat");
        if (!ParseCueSource(cue, row.cue)) return Fail(Join(p, "cue"), "eye, rwr, radio, damage, order or any");
        row.any_threat = threat == "any";
        if (!row.any_threat && !ParseThreatClass(threat, row.threat)) return Fail(Join(p, "threat"), "unknown class '" + threat + "'");
        if (!ParseManoeuvre(man, row.manoeuvre)) return Fail(Join(p, "manoeuvre"), "unknown manoeuvre '" + man + "'");
        if (!ParseCountermeasure(cm, row.cm)) return Fail(Join(p, "countermeasure"), "none, chaff, flares or both");
        for (double x : {row.jettison, row.tanks_only, row.press_on, row.abort}) {
            if (x < 0.0 || x > 1.0) return Fail(p, "the shares are chances, 0 to 1");
        }
        if (row.press_on + row.abort > 1.0 + 1e-9) return Fail(p, "press_on and abort at most 1 together (the rest goes round)");
        return true;
    }

    bool LoadProfile(const json::Value& v, const std::string& p, ReactionProfile& pr) {
        if (!Keys(v, p, {"programme", "calls", "package_net", "skill", "rows", "fallback", "source", "confidence", "notes"}) ||
            !Provenance(v, p) || !OptionalBool(v, p, "programme", pr.programme) || !OptionalBool(v, p, "calls", pr.calls) ||
            !OptionalBool(v, p, "package_net", pr.package_net) || !OptionalNumber(v, p, "skill", pr.skill)) {
            return false;
        }
        if (pr.skill < 0.0 || pr.skill > 1.0) return Fail(p, "skill 0 to 1");
        const json::Value* rows = OptionalArray(v, p, "rows");
        if (rows == nullptr) return Message().empty() ? Fail(Join(p, "rows"), "required") : false;
        for (std::size_t i = 0; i < rows->items.size(); ++i) {
            ReactionRow row;
            if (!LoadRow(rows->items[i], Index(Join(p, "rows"), i), row, false)) return false;
            pr.rows.push_back(row);
        }
        const json::Value* fb = v.Find("fallback");
        if (fb == nullptr) return Fail(Join(p, "fallback"), "required");
        return LoadRow(*fb, Join(p, "fallback"), pr.fallback, true);
    }

    bool RequireNumber(const json::Value& obj, const std::string& path, const char* key, double& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        if (!v->Is(json::Type::Number)) return Fail(Join(path, key), "expected a number");
        out = v->number;
        return true;
    }
};

inline bool ParseDataFile(const std::string& path, json::Value& root, std::string* error, const char* what) {
    std::string text;
    if (!ReadDataFile(path, text)) {
        if (error != nullptr) *error = std::string(what) + ": cannot read " + path;
        return false;
    }
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = std::string(what) + ": " + e;
        return false;
    }
    return true;
}

}  // namespace detail

// data/gameplay/reactions.json and data/air_war/countermeasures.json from `data_dir`; false with the reason in `error`.
inline bool LoadReactionTable(const std::string& data_dir, ReactionTable& out, std::string* error = nullptr) {
    json::Value root;
    if (!detail::ParseDataFile(data_dir + "/gameplay/reactions.json", root, error, "reactions")) return false;
    ReactionTable t;
    detail::ReactionLoader loader;
    if (!loader.Load(root, t)) {
        if (error != nullptr) *error = "gameplay/reactions.json: " + loader.Message();
        return false;
    }
    json::Value fits;
    if (!detail::ParseDataFile(data_dir + "/air_war/countermeasures.json", fits, error, "countermeasures")) return false;
    detail::ReactionLoader fit_loader;
    if (!fit_loader.LoadFits(fits, t)) {
        if (error != nullptr) *error = "air_war/countermeasures.json: " + fit_loader.Message();
        return false;
    }
    out = std::move(t);
    return true;
}

}  // namespace simcore
