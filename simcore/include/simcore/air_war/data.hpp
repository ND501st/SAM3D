// simcore/air_war/data.hpp — the enemy air command's data: the theatre, the factions and their doctrine, the officers'
// traits and quirks, and the game-balance numbers (data/air_war/, data/gameplay/air_war.json; docs/air_war/).
//
// Loaders are strict like the console's: unknown keys fail, every block names its source and confidence, numbers are
// range-checked. Doctrine comes from the research (docs/research/ENEMY_AIR_RESEARCH_2026-10.md) where a source exists
// and is a game value where none does; the files say which.
#pragma once

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/types.hpp"
#include "simcore/console_data.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

// ------------------------------------------------------------------------------------------------ the theatre

// The air defence systems a theatre object may have: the id the data uses, and how the enemy's intelligence labels it
// (NATO names, since every adversary's analysts used them or their own equivalent). The S-75M2 and S-125M1 are the
// national air defence's variants (docs/campaign/10_BRIEFING_FORCES_AND_NATION.md decision 1); "s75" and "s125" stay
// for the theatre's existing sites.
struct AirDefenceSystem {
    const char* id;
    const char* label;
};

inline const std::vector<AirDefenceSystem>& AirDefenceSystems() {
    static const std::vector<AirDefenceSystem> systems = {
        {"osa", "SA-8 battery"},       {"kub", "SA-6 battery"},           {"s75", "SA-2 site"},
        {"s75m2", "SA-2 site"},        {"s125", "SA-3 battery"},          {"s125m1", "SA-3 battery"},
        {"strela10", "SA-13 platoon"}, {"strela1", "SA-9 platoon"},       {"zsu234", "ZSU-23-4 platoon"},
        {"s60", "57 mm AAA battery"},  {"ks30", "130 mm AAA battery"},    {"zu23", "AAA"},
        {"aaa", "AAA"},                {"manpads", "MANPADS"},
    };
    return systems;
}

inline const AirDefenceSystem* FindAirDefenceSystem(const std::string& id) {
    for (const AirDefenceSystem& s : AirDefenceSystems()) {
        if (id == s.id) return &s;
    }
    return nullptr;
}

// An object's air defence, as the abstract resolver and the enemy's threat picture see it (game values).
struct DefenceDef {
    bool present = false;
    std::string system;        // an id of AirDefenceSystems(): display and notes
    double radius_m = 0.0;     // where it can engage
    double lethality = 0.0;    // chance per aircraft to be shot down on a pass through the heart of the zone
    bool low_cover = true;     // effective against low fliers
    bool medium_cover = true;  // effective against fliers at medium altitude
    bool radar = true;         // radiates: ELINT can locate it, ARMs can home on it (guns on optical sights: false)
    int missiles = -1;         // ready rounds before a reload (-1: guns, no limit)
    int radars = 1;            // fire-control radars that can each fight alone (an Osa battery's four combat vehicles: 4);
                               // an anti-radiation missile's hit takes out one of them
};

struct TheatreObject {
    std::string id;
    std::string name;
    ObjectKind kind = ObjectKind::Depot;
    Vec3 pos;                    // theatre ENU [m]
    double value = 0.5;          // its worth to an attacker, 0..1, before the faction's weights
    double hardness = 0.5;       // 0 soft .. 1 very hard
    double repair_h = 24.0;      // hours from wrecked to working (0: not repaired in the campaign)
    double relocate_h = 0.0;     // a mobile unit moves this often (0: stays); the player's battery: the player decides
    double relocate_m = 0.0;     // how far (a unit without prepared positions)
    std::vector<Vec3> prepared;  // its prepared positions, `pos` first: it moves between these (empty: anywhere relocate_m away)
    bool mobile = false;
    bool emitter = false;        // radiates when it works (its share of the hour: `emission`)
    double emission = 0.5;       // share of each hour a radar radiates when not fighting
    bool player = false;         // the player's battery
    bool decoy = false;          // a dummy that looks like `looks_like`
    ObjectKind looks_like = ObjectKind::SamSite;
    double prior_known = 1.0;    // chance the enemy knew of it before the war (pre-war survey)
    DefenceDef defence;
    std::string c2_source;       // the battery's C2 source this object is (command/c2_sources.json id, or "pu12")
    std::string regiment_radar;  // "p19" / "p40": it feeds the PU-12's picture (scenario c2.regiment)
    double radius_m = 0.0;       // a city's built-up area (collateral damage)
    std::string notes;
};

struct EntryPoint {
    std::string id;
    Vec3 pos;  // on the enemy's side of the line
};

// One enemy's border with the player's country, and where its raids come from.
struct FrontDef {
    std::string faction;
    std::string name;
    std::vector<Vec3> line;           // the front line or border
    std::vector<EntryPoint> entries;  // where raids cross (and leave)
    Vec3 bases;                       // the enemy's airfields, roughly (routes start there)
    double friendly_fighters = 0.3;   // the player's side's fighter threat over this front, 0..1 (escort need)
    bool has_battery_position = false;  // in a war on this front the player's battery deploys here (its dummies with it)
    Vec3 battery_position;
};

struct TheatreDef {
    std::string id;
    std::string name;
    std::string description;
    std::string country_name;   // the player's country
    std::string country_short;
    GeoPoint origin;
    double ground_height_m = 0.0;
    double start_hour = 6.0;    // local time at campaign hour 0
    std::vector<FrontDef> fronts;
    std::vector<TheatreObject> objects;

    const TheatreObject* Find(const std::string& id_) const {
        for (const TheatreObject& o : objects) {
            if (o.id == id_) return &o;
        }
        return nullptr;
    }
    int IndexOf(const std::string& id_) const {
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].id == id_) return static_cast<int>(i);
        }
        return -1;
    }
    const FrontDef* FrontFor(const std::string& faction) const {
        for (const FrontDef& f : fronts) {
            if (f.faction == faction) return &f;
        }
        return nullptr;
    }
    const TheatreObject* PlayerBattery() const {
        for (const TheatreObject& o : objects) {
            if (o.player) return &o;
        }
        return nullptr;
    }
};

// ------------------------------------------------------------------------------------------------ factions

struct AircraftType {
    std::string id;
    std::string name;
    std::vector<FlightRole> roles;
    std::string target_class = "fighter";  // the class in data/gameplay/osa_ak.json's hit chances
    bool helicopter = false;
    bool drone = false;
    double rcs_m2 = 3.0;
    double cruise_mps = 220.0;
    double attack_mps = 250.0;
    double low_agl_m = 60.0;       // its low-level height
    double medium_alt_m = 4000.0;  // its medium-altitude height above the ground
    double max_turn_deg_s = 8.0;
    bool night = false;
    bool all_weather = false;
    std::string rwr_em;            // EM library RWR, when it has one
    std::string self_jammer_em;    // EM library self-protection jammer
    std::string standoff_jammer_em;
    int arms = 0;                  // anti-radiation missiles per sortie
    double arm_range_m = 0.0;
    bool arm_memory = false;       // the seeker remembers the radar's position after it stops radiating
    // The EM library's anti-radiation missile it carries (em/arm.hpp; roadmap E6, A11). With the EM layer and a warning
    // receiver the raid's scenario fires it as an EM missile; otherwise the mission bridge's gameplay model does. Default:
    // the HARM class with a memory, else the Shrike class.
    std::string arm_em;
    double strike_power = 0.0;     // effect of one aircraft's load on a target (game value, 0..1 of a "standard" target)
    double recce_quality = 0.0;    // camera or sensor, 0..1
    double air_to_air = 0.0;       // value as an escort, 0..1
    double evasion = 0.3;          // how well its crews survive a pass (with the pilot's skill), 0..1
    std::string source;
    std::string confidence;
    std::string notes;

    bool Has(FlightRole r) const {
        for (FlightRole x : roles) {
            if (x == r) return true;
        }
        return false;
    }
};

struct SquadronDef {
    std::string id;
    std::string name;
    std::string aircraft;  // AircraftType id
    int count = 12;
    std::string leader;    // officer id ("" : one is generated)
};

// How a faction builds a package of one mission kind (the tactical groups of mai_tactics_vvs §3.3.3; the package
// roles of afmag_1989_slam).
struct PackageTemplate {
    bool allowed = false;
    int main_flights = 1;          // flights of the main role
    int flight_size = 2;           // aircraft in each
    double sead_per_threat = 0.0;  // SEAD aircraft per radar threat believed on the route
    int sead_max = 0;
    double escort_ratio = 0.0;     // escort aircraft per main aircraft, scaled by the front's fighter threat
    int jammers = 0;               // stand-off jammers
    bool escort_jamming = false;   // the jammers fly with the package instead of standing off
    int decoys = 0;                // decoy drones or demonstration aircraft
    bool chaff = false;            // a chaff corridor ahead of the strike
    int recce_lead = 0;            // additional reconnaissance ahead of the strike
    std::vector<Profile> profiles;
    std::vector<DayPart> day_parts;
    std::vector<std::string> aircraft;  // preferred types for the main flights (the doctrine's choice), in order
    std::string notes;
};

struct PhaseDef {
    std::string id;
    std::string name;
    double apportion[kMissionKinds] = {};  // share of the cycle's sorties per mission kind (normalised on load)
    int until_day = 0;                     // > 0: the phase ends after this campaign day
    double until_iads = -1.0;              // >= 0: ends when the believed working air defences fall below this share
    double tempo = 1.0;                    // the sortie rate in this phase, times the doctrine's (> 1: a surge)
    std::string notes;
};

struct DoctrineDef {
    double planning_cycle_h = 24.0;
    double lead_time_h = 6.0;           // from the plan to the first time on target
    double on_call_fraction = 0.0;      // sorties held back for on-call strikes (mai_tactics_vvs: "по вызову")
    double centralisation = 0.5;        // 0 decentralised .. 1 every time set from above (rigid timing)
    double simultaneous_bias = 0.5;     // 0 successive small raids .. 1 massed simultaneous blows
    double sortie_rate_per_day = 2.0;   // sorties per serviceable aircraft per day
    double serviceability = 0.7;
    double max_packages_per_cycle = 12.0;
    std::vector<PhaseDef> phases;
    PackageTemplate packages[kMissionKinds];
};

struct IntelDef {
    bool satellite = false;
    std::vector<double> satellite_pass_hours;  // local hours of the passes
    double satellite_latency_h = 3.0;
    double satellite_detect = 0.5;             // chance to find a mobile unit in the open on a clear pass
    double recce_latency_h = 4.0;              // film processing
    bool elint = false;
    double elint_reach_m = 150000.0;           // stand-off collectors' reach beyond the line
    double elint_delay_h = 0.5;
    double elint_best_m = 150.0;               // fix accuracy with long emissions
    double elint_k_m = 3000.0;                 // accuracy = best + k / sqrt(seconds radiated)
    bool comint = false;
    double comint_sigma_m = 3000.0;
    double comint_delay_h = 2.0;
    double humint_per_day = 1.0;
    double humint_delay_h = 24.0;
    double humint_sigma_m = 4000.0;
    double humint_false = 0.2;                 // share of agents' reports that are wrong or planted
    double humint_reach_m = 60000.0;           // how far behind the line the agents see (rebels: everywhere)
    double prior_mobile_sigma_m = 8000.0;      // pre-war knowledge of mobile units
    double bda_overclaim = 0.5;                // mean inflation of pilots' claims
    double pirep_delay_h = 1.0;
    double analyst_skill = 0.5;
};

struct LogisticsDef {
    double replacements_per_day = 0.0;  // aircraft
    double arm_stock = 0.0;
    double arm_resupply_per_day = 0.0;
    double damaged_return_h = 48.0;     // a damaged aircraft back in service after
};

struct PoliticalEventDef {
    std::string id;
    std::string name;
    std::string text;
    int day_at_least = 0;
    double losses_today_at_least = -1.0;
    double pressure_at_least = -1.0;
    double chance = 1.0;                 // per day once the conditions hold
    bool once = true;
    // effects
    std::vector<ObjectKind> unban;
    std::vector<ObjectKind> ban;
    double pause_h = 0.0;                // no new packages for this long
    double sortie_factor = 1.0;          // multiplies the tempo from now on
    double min_altitude_profile = -1.0;  // >= 1: medium altitude or higher only (loss avoidance)
    std::string reinforce_squadron;
    int reinforce_count = 0;
    double arm_resupply = 0.0;
    double city_radius_m = -1.0;         // >= 0: new no-strike radius around cities
    double max_depth_m = -1.0;           // >= 0: new depth limit (0: none)
};

struct PoliticsDef {
    double support = 0.7;
    double pressure = 0.3;
    double loss_tolerance_per_day = 3.0;
    double pow_sensitivity = 0.5;
    double collateral_sensitivity = 0.5;
    double no_strike_city_radius_m = 0.0;
    double max_depth_m = 0.0;  // > 0: no raids on targets deeper than this behind the front (a limited war)
    std::vector<ObjectKind> banned;
    double interference_per_cycle = 0.05;  // higher authority sets the time on target (hgram_080)
    double target_weight[static_cast<int>(ObjectKind::Count)] = {};  // the faction's worth of each kind (default 1)
    std::vector<PoliticalEventDef> events;
};

// The eight character traits and six skills, all 0..1 (docs/air_war/02_AIR_STAFF.md).
struct Traits {
    double boldness = 0.5;        // accepts risk for results
    double methodical = 0.5;      // thorough: SEAD and recce first, good coordination
    double creativity = 0.5;      // tries new things, deceives, varies routes
    double rigidity = 0.5;        // holds to doctrine and to the plan, learns slowly
    double ambition = 0.5;        // wants visible results for his superiors
    double skepticism = 0.5;      // doubts pilots' claims and agents
    double vindictiveness = 0.5;  // remembers who shot his men down
    double composure = 0.5;       // keeps his head under losses
};

struct Skills {
    double planning = 0.5;
    double intelligence = 0.5;
    double sead = 0.5;
    double airmanship = 0.5;
    double leadership = 0.5;
    double politics = 0.5;
};

enum class StaffRole { Commander, Planner, Intelligence, Sead, Political, SquadronLeader };

inline const char* StaffRoleName(StaffRole r) {
    switch (r) {
        case StaffRole::Commander: return "commander";
        case StaffRole::Planner: return "planner";
        case StaffRole::Intelligence: return "intelligence";
        case StaffRole::Sead: return "sead";
        case StaffRole::Political: return "political";
        case StaffRole::SquadronLeader: return "squadron_leader";
    }
    return "?";
}

inline bool ParseStaffRole(const std::string& s, StaffRole& out) {
    for (StaffRole r : {StaffRole::Commander, StaffRole::Planner, StaffRole::Intelligence, StaffRole::Sead,
                        StaffRole::Political, StaffRole::SquadronLeader}) {
        if (s == StaffRoleName(r)) {
            out = r;
            return true;
        }
    }
    return false;
}

struct OfficerDef {
    std::string id;
    std::string name;
    std::string rank;
    StaffRole role = StaffRole::Planner;
    std::string squadron;  // a squadron leader's squadron
    int age = 40;
    std::string background;  // "fighter pilot", "bomber navigator", "staff college" ...
    std::string bio;         // a line or two: who he is (what our intelligence may learn)
    Traits traits;
    Skills skills;
    std::vector<std::string> quirks;
    bool flies = false;      // leads missions himself (squadron leaders always do)
    bool authored = false;   // written by hand (vs generated)
};

struct OfficerPoolDef {
    std::map<std::string, std::string> ranks;  // role name -> rank title
    std::vector<std::string> first_names;
    std::vector<std::string> last_names;
    std::vector<std::string> backgrounds;
    Traits trait_mean;
    double trait_sd = 0.18;
    Skills skill_mean;
    double skill_sd = 0.15;
    int quirks_min = 1;
    int quirks_max = 3;
};

struct FactionDef {
    std::string id;
    std::string name;
    std::string short_name;
    Tier tier = Tier::Peer;
    std::string description;
    std::string front;  // the theatre front it attacks across (faction id in theatre.fronts)
    DoctrineDef doctrine;
    IntelDef intel;
    LogisticsDef logistics;
    PoliticsDef politics;
    std::vector<AircraftType> aircraft;
    std::vector<SquadronDef> squadrons;
    OfficerPoolDef pool;
    std::vector<OfficerDef> officers;  // the authored staff
    std::vector<std::string> em_libraries;  // EM libraries its jammers and RWRs come from (for scenarios)
    std::vector<std::string> callsigns;     // flight call signs its planners use

    const AircraftType* Aircraft(const std::string& id_) const {
        for (const AircraftType& a : aircraft) {
            if (a.id == id_) return &a;
        }
        return nullptr;
    }
    const SquadronDef* Squadron(const std::string& id_) const {
        for (const SquadronDef& s : squadrons) {
            if (s.id == id_) return &s;
        }
        return nullptr;
    }
};

// ------------------------------------------------------------------------------------------------ traits and quirks

// What a quirk does to its officer's choices. The planner reads these by name (kQuirkEffects lists them all).
struct QuirkDef {
    std::string id;
    std::string name;
    std::string description;
    std::string signature;   // what the player can see in his raids
    std::string intel_hint;  // how our analysts phrase it once they have seen enough
    bool weakness = false;
    std::map<std::string, double> effects;
    double tier_weight[4] = {1.0, 1.0, 1.0, 1.0};
    std::vector<StaffRole> roles;  // who can have it (empty: anyone)

    double Effect(const std::string& key) const {
        const auto it = effects.find(key);
        return it == effects.end() ? 0.0 : it->second;
    }
};

// Every effect a quirk may name, and what reads it.
inline const std::vector<std::string>& KnownQuirkEffects() {
    static const std::vector<std::string> k = {
        "pref_night", "pref_dawn", "pref_day", "pref_dusk",  // time-of-day bonus (planner)
        "axis_repeat",      // bonus for the entry point he used last (predictable)
        "axis_vary",        // penalty for the entry point he used last
        "feint",            // adds a demonstration flight to his strike packages
        "decoys",           // more decoys
        "low_level",        // bonus for low profiles
        "medium_altitude",  // bonus for medium profiles
        "massing",          // simultaneous blows (more aircraft per package, fewer packages)
        "drip_feed",        // successive small raids
        "weather_tolerance",// flies when the weather is poor
        "arm_reserve",      // keeps ARMs back
        "restrike",         // re-strikes his earlier targets
        "flies",            // leads in person
        "doctrine_weight",  // sticks to the doctrine's apportionment
        "report_inflation", // inflates results to his superiors
        "grudge",           // more vindictive
        "comsec_leak",      // his raids are announced on the radio (our intelligence warns earlier)
        "punctuality",      // + exact times, - late and ragged
        "double_tap",       // a second strike on the same target soon after the first
        "elint_trust",      // believes ELINT fixes (chases ghosts)
        "imagery_wait",     // wants fresh imagery before a strike on a mobile target
        "abort_threshold",  // + presses on, - turns back at the first loss
        "helicopter_use",   // uses helicopters more (+) or less (-)
        "sead_ratio",       // more SEAD per package
        "recce_first",      // recce before strikes
        "risk",             // accepts higher expected losses
    };
    return k;
}

// ------------------------------------------------------------------------------------------------ game balance

// The game-balance table (data/gameplay/air_war.json): grouped numbers, flattened to "group.key". Every key the code
// reads is listed here with its default; the file must not name others.
struct TuningTable {
    std::map<std::string, double> values;

    double Get(const std::string& key) const {
        const auto it = values.find(key);
        return it == values.end() ? Defaults().at(key) : it->second;
    }

    static const std::map<std::string, double>& Defaults() {
        static const std::map<std::string, double> d = {
            // utility (planner)
            {"utility.value_weight", 10.0},
            {"utility.loss_cost", 4.0},          // per aircraft expected lost, before the faction's tolerance
            {"utility.noise_at_zero_skill", 3.0},
            {"utility.quirk_scale", 2.0},
            {"utility.grudge_weight", 0.4},
            {"utility.stale_penalty", 4.0},
            {"utility.exploration", 1.0},
            {"utility.min_value", 0.5},
            {"utility.min_effect", 0.05},
            // attrition (abstract resolution)
            {"attrition.low_vs_long_range_sam", 0.35},
            {"attrition.medium_vs_guns", 0.0},
            {"attrition.high_vs_short_range", 0.0},
            {"attrition.sead_suppression", 0.45},
            {"attrition.jamming", 0.25},
            {"attrition.night", 0.7},
            {"attrition.poor_weather", 0.8},
            {"attrition.emcon_lethality", 0.6},
            {"attrition.arm_kill", 0.35},
            {"attrition.arm_kill_no_memory_off", 0.05},
            {"attrition.escort_fighter_kill", 0.03},
            {"attrition.max_per_pass", 0.6},
            // effect of strikes
            {"strike.hit_chance", 0.6},
            {"strike.night_accuracy", 0.6},
            {"strike.poor_weather_accuracy", 0.5},
            {"strike.toss_accuracy", 0.6},
            {"strike.stale_miss_m", 1500.0},
            {"strike.decoy_area_m", 1500.0},
            {"strike.collateral_m", 3000.0},
            // intelligence
            {"intel.mobile_sigma_growth_mps", 0.15},
            {"intel.mobile_confidence_half_life_h", 18.0},
            {"intel.fixed_function_recovery", 0.5},
            {"intel.decoy_spot_skill", 0.6},
            {"intel.ghost_fix_chance", 0.35},
            {"intel.claim_destroy_when_off", 0.5},
            {"intel.launch_sighting_m", 20000.0},
            {"intel.recce_corridor_m", 12000.0},
            {"intel.suspect_after_loss", 0.5},
            {"repair.restored_function", 0.5},
            {"attrition.target_guns_troops", 0.04},
            {"attrition.target_guns_site", 0.015},
            {"adapt.reopen_margin", 0.2},
            // adaptation
            {"adapt.learning_rate", 0.5},
            {"adapt.stress_per_loss", 0.08},
            {"adapt.stress_recovery_per_day", 0.15},
            {"adapt.reputation_per_success", 0.05},
            {"adapt.reputation_per_loss", 0.04},
            {"adapt.relieve_below", 0.2},
            {"adapt.promote_above", 0.9},
            {"adapt.exposure_lesson_m", 40000.0},
            // politics
            {"politics.pressure_per_quiet_day", 0.08},
            {"politics.pressure_relief_per_success", 0.04},
            {"politics.support_per_excess_loss", 0.03},
            {"politics.pow_pressure", 0.05},
            {"politics.collateral_restriction_m", 2000.0},
            // weather (a day's weather, drawn at the start)
            {"weather.clear_share", 0.5},
            {"weather.poor_share", 0.15},
            // pacing (which raids the player plays)
            {"pacing.min_gap_h", 3.0},
            {"pacing.max_per_day", 3.0},
            {"pacing.sector_m", 10000.0},
            // raid scenarios
            {"scenario.clip_m", 70000.0},
            {"scenario.prep_s", 150.0},
            {"scenario.margin_s", 240.0},
            {"scenario.flight_spacing_m", 800.0},
            {"scenario.max_targets", 24.0},
            {"scenario.release_capture_m", 2000.0},
            {"scenario.weapon_fall_s", 15.0},
            {"scenario.arm_speed_mps", 600.0},
            {"scenario.arm_pk", 0.45},
            {"scenario.arm_memory_s", 20.0},
            {"scenario.bomb_lethal_m", 150.0},
            {"scenario.bomb_pk_vehicle", 0.5},
            {"scenario.rocket_pk_vehicle", 0.3},
            {"scenario.revealed_by_launch_m", 15000.0},
            // our own intelligence (what the player learns about the enemy)
            {"own_intel.reveal_per_observation", 0.12},
            {"own_intel.pow_chance", 0.4},
            {"own_intel.warning_s", 300.0},
            {"own_intel.leak_warning_s", 900.0},
        };
        return d;
    }
};

// ------------------------------------------------------------------------------------------------ loaders

namespace detail {

class AirWarLoader : public simcore::detail::ConsoleLoader {
public:
    bool Num(const json::Value& obj, const std::string& path, const char* key, double& out, double lo, double hi) {
        if (!OptionalNumber(obj, path, key, out)) return false;
        if (obj.Find(key) != nullptr && (out < lo || out > hi)) {
            return Fail(Join(path, key), "must be in " + Fmt(lo) + " .. " + Fmt(hi));
        }
        return true;
    }
    bool Int(const json::Value& obj, const std::string& path, const char* key, int& out, int lo, int hi) {
        if (!OptionalInt(obj, path, key, out)) return false;
        if (obj.Find(key) != nullptr && (out < lo || out > hi)) {
            return Fail(Join(path, key), "must be in " + std::to_string(lo) + " .. " + std::to_string(hi));
        }
        return true;
    }
    bool Km(const json::Value& obj, const std::string& path, Vec3& out) {
        double e = 0.0, n = 0.0;
        if (!Keys(obj, path, {"east_km", "north_km", "notes"})) return false;
        if (obj.Find("east_km") == nullptr || obj.Find("north_km") == nullptr) return Fail(path, "needs east_km and north_km");
        if (!OptionalNumber(obj, path, "east_km", e) || !OptionalNumber(obj, path, "north_km", n)) return false;
        out = Vec3{e * 1000.0, n * 1000.0, 0.0};
        return true;
    }
    bool KindList(const json::Value& obj, const std::string& path, const char* key, std::vector<ObjectKind>& out) {
        const json::Value* arr = OptionalArray(obj, path, key);
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            ObjectKind k;
            if (!arr->items[i].Is(json::Type::String) || !ParseObjectKind(arr->items[i].string, k)) {
                return Fail(Index(Join(path, key), i), "not an object kind");
            }
            out.push_back(k);
        }
        return true;
    }
    bool StringList(const json::Value& obj, const std::string& path, const char* key, std::vector<std::string>& out) {
        const json::Value* arr = OptionalArray(obj, path, key);
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            if (!arr->items[i].Is(json::Type::String)) return Fail(Index(Join(path, key), i), "expected a string");
            out.push_back(arr->items[i].string);
        }
        return true;
    }

    // ---- theatre
    // `base`: the theatre this one extends ("extends", LoadTheatreFile), or null. An extending theatre starts from its
    // base: its fronts replace the base's front of the same faction, "remove" drops base objects, and its objects are
    // added, one with a base object's id replacing it where it stands.
    bool LoadTheatre(const json::Value& root, TheatreDef& out, const TheatreDef* base = nullptr) {
        if (!Keys(root, "", {"theatre_id", "name", "description", "source", "confidence", "notes", "extends", "remove", "country",
                             "origin", "ground_height_m", "start_hour", "fronts", "objects"})) {
            return false;
        }
        if (root.Find("extends") != nullptr && base == nullptr) {
            return Fail("extends", "only a theatre file can extend another (LoadTheatreFile)");
        }
        if (root.Find("remove") != nullptr && base == nullptr) return Fail("remove", "only a theatre that extends another removes objects");
        if (base != nullptr) out = *base;
        if (!RequireString(root, "", "theatre_id", out.id) || !RequireString(root, "", "name", out.name) ||
            !OptionalString(root, "", "description", out.description) || !Provenance(root, "")) {
            return false;
        }
        if (base != nullptr) return ExtendTheatre(root, out);
        if (const json::Value* c = root.Find("country")) {
            if (!Keys(*c, "country", {"name", "short", "notes"}) || !RequireString(*c, "country", "name", out.country_name) ||
                !OptionalString(*c, "country", "short", out.country_short)) {
                return false;
            }
        }
        const json::Value* o = root.Find("origin");
        double lat = 0.0, lon = 0.0, alt = 0.0;
        if (o == nullptr || !Keys(*o, "origin", {"lat_deg", "lon_deg", "alt_m"}) ||
            !OptionalNumber(*o, "origin", "lat_deg", lat) || !OptionalNumber(*o, "origin", "lon_deg", lon) ||
            !OptionalNumber(*o, "origin", "alt_m", alt)) {
            return message_.empty() ? Fail("origin", "required: lat_deg, lon_deg, alt_m") : false;
        }
        out.origin = GeoPoint::FromDegrees(lat, lon, alt);
        if (!Num(root, "", "ground_height_m", out.ground_height_m, -500.0, 5000.0) ||
            !Num(root, "", "start_hour", out.start_hour, 0.0, 24.0)) {
            return false;
        }
        const json::Value* fronts = OptionalArray(root, "", "fronts");
        if (fronts == nullptr || fronts->items.empty()) return message_.empty() ? Fail("fronts", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < fronts->items.size(); ++i) {
            if (!LoadFront(fronts->items[i], Index("fronts", i), out)) return false;
        }
        const json::Value* objs = OptionalArray(root, "", "objects");
        if (objs == nullptr || objs->items.empty()) return message_.empty() ? Fail("objects", "required, a non-empty list") : false;
        int players = 0;
        for (std::size_t i = 0; i < objs->items.size(); ++i) {
            TheatreObject ob;
            if (!LoadObject(objs->items[i], Index("objects", i), ob)) return false;
            if (out.Find(ob.id) != nullptr) return Fail(Index("objects", i), "duplicate id '" + ob.id + "'");
            if (ob.player) ++players;
            out.objects.push_back(std::move(ob));
        }
        if (players != 1) return Fail("objects", "exactly one object must be the player's battery (\"player\": true)");
        return true;
    }

    bool ExtendTheatre(const json::Value& root, TheatreDef& out) {
        if (root.Find("country") != nullptr || root.Find("origin") != nullptr) {
            return Fail(root.Find("country") != nullptr ? "country" : "origin", "an extending theatre keeps its base's");
        }
        if (!Num(root, "", "ground_height_m", out.ground_height_m, -500.0, 5000.0) ||
            !Num(root, "", "start_hour", out.start_hour, 0.0, 24.0)) {
            return false;
        }
        if (const json::Value* fronts = OptionalArray(root, "", "fronts")) {
            for (std::size_t i = 0; i < fronts->items.size(); ++i) {
                TheatreDef one;
                if (!LoadFront(fronts->items[i], Index("fronts", i), one)) return false;
                bool replaced = false;
                for (FrontDef& f : out.fronts) {
                    if (f.faction != one.fronts[0].faction) continue;
                    f = one.fronts[0];
                    replaced = true;
                }
                if (!replaced) out.fronts.push_back(one.fronts[0]);
            }
        } else if (!message_.empty()) {
            return false;
        }
        std::vector<std::string> removed;
        if (!StringList(root, "", "remove", removed)) return false;
        for (std::size_t i = 0; i < removed.size(); ++i) {
            const int at = out.IndexOf(removed[i]);
            if (at < 0) return Fail(Index("remove", i), "the base has no object '" + removed[i] + "'");
            out.objects.erase(out.objects.begin() + at);
        }
        std::vector<std::string> own;
        if (const json::Value* objs = OptionalArray(root, "", "objects")) {
            for (std::size_t i = 0; i < objs->items.size(); ++i) {
                TheatreObject ob;
                if (!LoadObject(objs->items[i], Index("objects", i), ob)) return false;
                if (std::find(own.begin(), own.end(), ob.id) != own.end()) return Fail(Index("objects", i), "duplicate id '" + ob.id + "'");
                own.push_back(ob.id);
                const int at = out.IndexOf(ob.id);
                if (at >= 0) {
                    out.objects[static_cast<std::size_t>(at)] = std::move(ob);
                } else {
                    out.objects.push_back(std::move(ob));
                }
            }
        } else if (!message_.empty()) {
            return false;
        }
        int players = 0;
        for (const TheatreObject& o : out.objects) players += o.player ? 1 : 0;
        if (players != 1) return Fail("objects", "exactly one object must be the player's battery (\"player\": true)");
        return true;
    }

    bool LoadFront(const json::Value& f, const std::string& p, TheatreDef& out) {
        FrontDef fr;
        if (!Keys(f, p, {"faction", "name", "line", "entries", "bases", "friendly_fighters", "battery_position", "notes"}) ||
            !RequireString(f, p, "faction", fr.faction) || !RequireString(f, p, "name", fr.name) ||
            !Num(f, p, "friendly_fighters", fr.friendly_fighters, 0.0, 1.0)) {
            return false;
        }
        const json::Value* line = OptionalArray(f, p, "line");
        if (line == nullptr || line->items.size() < 2) return message_.empty() ? Fail(Join(p, "line"), "at least two points") : false;
        for (std::size_t i = 0; i < line->items.size(); ++i) {
            Vec3 v;
            if (!Km(line->items[i], Index(Join(p, "line"), i), v)) return false;
            fr.line.push_back(v);
        }
        const json::Value* entries = OptionalArray(f, p, "entries");
        if (entries == nullptr || entries->items.empty()) return message_.empty() ? Fail(Join(p, "entries"), "at least one") : false;
        for (std::size_t i = 0; i < entries->items.size(); ++i) {
            const json::Value& e = entries->items[i];
            const std::string ep = Index(Join(p, "entries"), i);
            EntryPoint en;
            double ekm = 0.0, nkm = 0.0;
            if (!Keys(e, ep, {"id", "east_km", "north_km", "notes"}) || !RequireString(e, ep, "id", en.id) ||
                !OptionalNumber(e, ep, "east_km", ekm) || !OptionalNumber(e, ep, "north_km", nkm)) {
                return false;
            }
            en.pos = Vec3{ekm * 1000.0, nkm * 1000.0, 0.0};
            fr.entries.push_back(en);
        }
        const json::Value* b = f.Find("bases");
        if (b == nullptr) return Fail(Join(p, "bases"), "required");
        if (!Km(*b, Join(p, "bases"), fr.bases)) return false;
        if (const json::Value* bp = f.Find("battery_position")) {
            if (!Km(*bp, Join(p, "battery_position"), fr.battery_position)) return false;
            fr.has_battery_position = true;
        }
        out.fronts.push_back(std::move(fr));
        return true;
    }

    bool LoadObject(const json::Value& v, const std::string& p, TheatreObject& ob) {
        std::string kind, looks;
        if (!Keys(v, p, {"id", "name", "kind", "east_km", "north_km", "value", "hardness", "repair_h", "relocate_h",
                         "relocate_km", "positions", "mobile", "emitter", "emission", "player", "decoy", "looks_like",
                         "prior_known", "defence", "c2_source", "regiment_radar", "radius_km", "notes"}) ||
            !RequireString(v, p, "id", ob.id) || !RequireString(v, p, "name", ob.name) ||
            !RequireString(v, p, "kind", kind)) {
            return false;
        }
        if (!ParseObjectKind(kind, ob.kind)) return Fail(Join(p, "kind"), "unknown object kind '" + kind + "'");
        double ekm = 0.0, nkm = 0.0, rkm = 0.0, relkm = 0.0;
        if (v.Find("east_km") == nullptr || v.Find("north_km") == nullptr) return Fail(p, "needs east_km and north_km");
        if (!OptionalNumber(v, p, "east_km", ekm) || !OptionalNumber(v, p, "north_km", nkm) ||
            !Num(v, p, "value", ob.value, 0.0, 1.0) || !Num(v, p, "hardness", ob.hardness, 0.0, 1.0) ||
            !Num(v, p, "repair_h", ob.repair_h, 0.0, 10000.0) || !Num(v, p, "relocate_h", ob.relocate_h, 0.0, 10000.0) ||
            !Num(v, p, "relocate_km", relkm, 0.0, 100.0) || !OptionalBool(v, p, "mobile", ob.mobile) ||
            !OptionalBool(v, p, "emitter", ob.emitter) || !Num(v, p, "emission", ob.emission, 0.0, 1.0) ||
            !OptionalBool(v, p, "player", ob.player) || !OptionalBool(v, p, "decoy", ob.decoy) ||
            !OptionalString(v, p, "looks_like", looks) || !Num(v, p, "prior_known", ob.prior_known, 0.0, 1.0) ||
            !OptionalString(v, p, "c2_source", ob.c2_source) || !OptionalString(v, p, "regiment_radar", ob.regiment_radar) ||
            !Num(v, p, "radius_km", rkm, 0.0, 50.0) || !OptionalString(v, p, "notes", ob.notes)) {
            return false;
        }
        ob.pos = Vec3{ekm * 1000.0, nkm * 1000.0, 0.0};
        ob.radius_m = rkm * 1000.0;
        ob.relocate_m = relkm * 1000.0;
        // Its prepared positions besides the one it starts at: a mobile unit that moves on its own.
        if (const json::Value* pos = OptionalArray(v, p, "positions")) {
            const std::string pp = Join(p, "positions");
            if (ob.player) return Fail(pp, "the player chooses the battery's positions");
            if (!ob.mobile || ob.relocate_h <= 0.0) return Fail(pp, "prepared positions are for a mobile unit that moves (relocate_h)");
            if (pos->items.empty()) return Fail(pp, "at least one");
            ob.prepared.push_back(ob.pos);
            for (std::size_t i = 0; i < pos->items.size(); ++i) {
                Vec3 at;
                if (!Km(pos->items[i], Index(pp, i), at)) return false;
                for (const Vec3& q : ob.prepared) {
                    if (Dist2d(q, at) < 300.0) return Fail(Index(pp, i), "within 300 m of another of its positions");
                }
                ob.prepared.push_back(at);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (!looks.empty() && !ParseObjectKind(looks, ob.looks_like)) return Fail(Join(p, "looks_like"), "unknown object kind");
        if (ob.decoy && looks.empty()) return Fail(Join(p, "looks_like"), "a decoy says what it looks like");
        if (ob.player && ob.kind != ObjectKind::SamSite) return Fail(Join(p, "player"), "the player's battery is a sam_site");
        if (!ob.regiment_radar.empty() && ob.regiment_radar != "p19" && ob.regiment_radar != "p40") {
            return Fail(Join(p, "regiment_radar"), "p19 or p40 (data/command/regiment_radars.json)");
        }
        if (const json::Value* d = v.Find("defence")) {
            const std::string dp = Join(p, "defence");
            double radius_km = 0.0;
            if (!Keys(*d, dp, {"system", "radius_km", "lethality", "low_cover", "medium_cover", "radar", "missiles", "radars", "notes"}) ||
                !RequireString(*d, dp, "system", ob.defence.system) || !Num(*d, dp, "radius_km", radius_km, 0.1, 100.0) ||
                !Num(*d, dp, "lethality", ob.defence.lethality, 0.0, 1.0) ||
                !OptionalBool(*d, dp, "low_cover", ob.defence.low_cover) ||
                !OptionalBool(*d, dp, "medium_cover", ob.defence.medium_cover) ||
                !OptionalBool(*d, dp, "radar", ob.defence.radar) || !Int(*d, dp, "missiles", ob.defence.missiles, -1, 1000) ||
                !Int(*d, dp, "radars", ob.defence.radars, 1, 16)) {
                return false;
            }
            if (FindAirDefenceSystem(ob.defence.system) == nullptr) {
                return Fail(Join(dp, "system"), "unknown air defence system '" + ob.defence.system + "' (AirDefenceSystems())");
            }
            ob.defence.present = true;
            ob.defence.radius_m = radius_km * 1000.0;
        }
        if (ob.player && !ob.defence.present) return Fail(p, "the player's battery needs its defence block");
        return true;
    }

    // ---- faction
    bool LoadFaction(const json::Value& root, FactionDef& out) {
        std::string tier;
        if (!Keys(root, "", {"faction_id", "name", "short_name", "tier", "description", "front", "source", "confidence",
                             "notes", "em_libraries", "callsigns", "doctrine", "intelligence", "logistics", "politics",
                             "aircraft", "squadrons", "officer_pool", "officers"}) ||
            !RequireString(root, "", "faction_id", out.id) || !RequireString(root, "", "name", out.name) ||
            !OptionalString(root, "", "short_name", out.short_name) || !RequireString(root, "", "tier", tier) ||
            !OptionalString(root, "", "description", out.description) || !RequireString(root, "", "front", out.front) ||
            !Provenance(root, "") || !StringList(root, "", "em_libraries", out.em_libraries) ||
            !StringList(root, "", "callsigns", out.callsigns)) {
            return false;
        }
        if (out.callsigns.empty()) out.callsigns.push_back("Flight");
        if (!ParseTier(tier, out.tier)) return Fail("tier", "inferior, near_peer, peer or superior");
        if (out.short_name.empty()) out.short_name = out.name;
        if (!LoadAircraft(root, out) || !LoadSquadrons(root, out) || !LoadDoctrine(root, out) || !LoadIntel(root, out) ||
            !LoadLogistics(root, out) || !LoadPolitics(root, out) || !LoadPool(root, out) || !LoadOfficers(root, out)) {
            return false;
        }
        return true;
    }

    bool LoadAircraft(const json::Value& root, FactionDef& out) {
        const json::Value* arr = OptionalArray(root, "", "aircraft");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("aircraft", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& a = arr->items[i];
            const std::string p = Index("aircraft", i);
            AircraftType t;
            std::vector<std::string> roles;
            if (!Keys(a, p, {"id", "name", "roles", "class", "helicopter", "drone", "rcs_m2", "cruise_mps", "attack_mps",
                             "low_agl_m", "medium_alt_m", "max_turn_deg_s", "night", "all_weather", "rwr_em",
                             "self_jammer_em", "standoff_jammer_em", "arms", "arm_range_km", "arm_memory", "arm_em", "strike_power",
                             "recce_quality", "air_to_air", "evasion", "source", "confidence", "notes"}) ||
                !RequireString(a, p, "id", t.id) || !RequireString(a, p, "name", t.name) ||
                !StringList(a, p, "roles", roles) || !OptionalString(a, p, "class", t.target_class) ||
                !OptionalBool(a, p, "helicopter", t.helicopter) || !OptionalBool(a, p, "drone", t.drone) ||
                !Num(a, p, "rcs_m2", t.rcs_m2, 0.001, 1000.0) || !Num(a, p, "cruise_mps", t.cruise_mps, 10.0, 1000.0) ||
                !Num(a, p, "attack_mps", t.attack_mps, 10.0, 1000.0) || !Num(a, p, "low_agl_m", t.low_agl_m, 2.0, 1000.0) ||
                !Num(a, p, "medium_alt_m", t.medium_alt_m, 100.0, 20000.0) ||
                !Num(a, p, "max_turn_deg_s", t.max_turn_deg_s, 0.5, 40.0) || !OptionalBool(a, p, "night", t.night) ||
                !OptionalBool(a, p, "all_weather", t.all_weather) || !OptionalString(a, p, "rwr_em", t.rwr_em) ||
                !OptionalString(a, p, "self_jammer_em", t.self_jammer_em) ||
                !OptionalString(a, p, "standoff_jammer_em", t.standoff_jammer_em) || !Int(a, p, "arms", t.arms, 0, 8) ||
                !OptionalBool(a, p, "arm_memory", t.arm_memory) || !OptionalString(a, p, "arm_em", t.arm_em) ||
                !Num(a, p, "strike_power", t.strike_power, 0.0, 2.0) ||
                !Num(a, p, "recce_quality", t.recce_quality, 0.0, 1.0) || !Num(a, p, "air_to_air", t.air_to_air, 0.0, 1.0) ||
                !Num(a, p, "evasion", t.evasion, 0.0, 1.0) || !Provenance(a, p) ||
                !OptionalString(a, p, "source", t.source) || !OptionalString(a, p, "confidence", t.confidence) ||
                !OptionalString(a, p, "notes", t.notes)) {
                return false;
            }
            double arm_km = 0.0;
            if (!Num(a, p, "arm_range_km", arm_km, 0.0, 300.0)) return false;
            t.arm_range_m = arm_km * 1000.0;
            if (t.arms > 0 && t.arm_em.empty()) t.arm_em = t.arm_memory ? "agm88a_class_harm" : "agm45_class_shrike";
            if (roles.empty()) return Fail(Join(p, "roles"), "at least one role");
            for (const std::string& r : roles) {
                FlightRole fr;
                if (!ParseFlightRole(r, fr)) return Fail(Join(p, "roles"), "unknown role '" + r + "'");
                t.roles.push_back(fr);
            }
            if (t.target_class != "default" && t.target_class != "drone" && t.target_class != "helicopter" &&
                t.target_class != "fighter" && t.target_class != "bomber") {
                return Fail(Join(p, "class"), "a class of the gameplay tuning: default, drone, helicopter, fighter, bomber");
            }
            if (t.arms > 0 && t.arm_range_m <= 0.0) return Fail(Join(p, "arm_range_km"), "an ARM carrier needs its range");
            if (out.Aircraft(t.id) != nullptr) return Fail(p, "duplicate aircraft id '" + t.id + "'");
            out.aircraft.push_back(std::move(t));
        }
        return true;
    }

    bool LoadSquadrons(const json::Value& root, FactionDef& out) {
        const json::Value* arr = OptionalArray(root, "", "squadrons");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("squadrons", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& s = arr->items[i];
            const std::string p = Index("squadrons", i);
            SquadronDef sq;
            if (!Keys(s, p, {"id", "name", "aircraft", "count", "leader", "notes"}) || !RequireString(s, p, "id", sq.id) ||
                !RequireString(s, p, "name", sq.name) || !RequireString(s, p, "aircraft", sq.aircraft) ||
                !Int(s, p, "count", sq.count, 1, 200) || !OptionalString(s, p, "leader", sq.leader)) {
                return false;
            }
            if (out.Aircraft(sq.aircraft) == nullptr) return Fail(Join(p, "aircraft"), "no aircraft '" + sq.aircraft + "'");
            if (out.Squadron(sq.id) != nullptr) return Fail(p, "duplicate squadron id '" + sq.id + "'");
            out.squadrons.push_back(std::move(sq));
        }
        return true;
    }

    bool LoadProfiles(const json::Value& obj, const std::string& p, std::vector<Profile>& out) {
        std::vector<std::string> names;
        if (!StringList(obj, p, "profiles", names)) return false;
        for (const std::string& n : names) {
            if (n == "low") out.push_back(Profile::Low);
            else if (n == "medium") out.push_back(Profile::Medium);
            else if (n == "high") out.push_back(Profile::High);
            else return Fail(Join(p, "profiles"), "low, medium or high");
        }
        return true;
    }
    bool LoadDayParts(const json::Value& obj, const std::string& p, std::vector<DayPart>& out) {
        std::vector<std::string> names;
        if (!StringList(obj, p, "day_parts", names)) return false;
        for (const std::string& n : names) {
            DayPart d;
            if (!ParseDayPart(n, d)) return Fail(Join(p, "day_parts"), "night, dawn, day or dusk");
            out.push_back(d);
        }
        return true;
    }

    bool LoadDoctrine(const json::Value& root, FactionDef& out) {
        const json::Value* d = root.Find("doctrine");
        if (d == nullptr) return Fail("doctrine", "required");
        DoctrineDef& doc = out.doctrine;
        const std::string p = "doctrine";
        if (!Keys(*d, p, {"planning_cycle_h", "lead_time_h", "on_call_fraction", "centralisation", "simultaneous_bias",
                          "sortie_rate_per_day", "serviceability", "max_packages_per_cycle", "phases", "packages",
                          "source", "confidence", "notes"}) ||
            !Provenance(*d, p) || !Num(*d, p, "planning_cycle_h", doc.planning_cycle_h, 1.0, 96.0) ||
            !Num(*d, p, "lead_time_h", doc.lead_time_h, 0.0, 72.0) ||
            !Num(*d, p, "on_call_fraction", doc.on_call_fraction, 0.0, 0.9) ||
            !Num(*d, p, "centralisation", doc.centralisation, 0.0, 1.0) ||
            !Num(*d, p, "simultaneous_bias", doc.simultaneous_bias, 0.0, 1.0) ||
            !Num(*d, p, "sortie_rate_per_day", doc.sortie_rate_per_day, 0.1, 8.0) ||
            !Num(*d, p, "serviceability", doc.serviceability, 0.1, 1.0) ||
            !Num(*d, p, "max_packages_per_cycle", doc.max_packages_per_cycle, 1.0, 100.0)) {
            return false;
        }
        const json::Value* phases = OptionalArray(*d, p, "phases");
        if (phases == nullptr || phases->items.empty()) return message_.empty() ? Fail(Join(p, "phases"), "at least one") : false;
        for (std::size_t i = 0; i < phases->items.size(); ++i) {
            const json::Value& ph = phases->items[i];
            const std::string pp = Index(Join(p, "phases"), i);
            PhaseDef phase;
            if (!Keys(ph, pp, {"id", "name", "apportion", "until_day", "until_iads", "tempo", "notes"}) ||
                !RequireString(ph, pp, "id", phase.id) || !RequireString(ph, pp, "name", phase.name) ||
                !Int(ph, pp, "until_day", phase.until_day, 0, 1000) || !Num(ph, pp, "until_iads", phase.until_iads, -1.0, 1.0) ||
                !Num(ph, pp, "tempo", phase.tempo, 0.1, 3.0) || !OptionalString(ph, pp, "notes", phase.notes)) {
                return false;
            }
            const json::Value* ap = ph.Find("apportion");
            if (ap == nullptr || !ap->Is(json::Type::Object)) return Fail(Join(pp, "apportion"), "required: {mission kind: share}");
            double total = 0.0;
            for (std::size_t k = 0; k < ap->keys.size(); ++k) {
                MissionKind mk;
                if (!ParseMissionKind(ap->keys[k], mk)) return Fail(Join(Join(pp, "apportion"), ap->keys[k]), "unknown mission kind");
                if (!ap->items[k].Is(json::Type::Number) || ap->items[k].number < 0.0) {
                    return Fail(Join(Join(pp, "apportion"), ap->keys[k]), "a share >= 0");
                }
                phase.apportion[static_cast<int>(mk)] = ap->items[k].number;
                total += ap->items[k].number;
            }
            if (total <= 0.0) return Fail(Join(pp, "apportion"), "the shares add up to nothing");
            for (double& x : phase.apportion) x /= total;
            doc.phases.push_back(std::move(phase));
        }
        const json::Value* pk = d->Find("packages");
        if (pk == nullptr || !pk->Is(json::Type::Object)) return Fail(Join(p, "packages"), "required: {mission kind: template}");
        for (std::size_t k = 0; k < pk->keys.size(); ++k) {
            MissionKind mk;
            const std::string tp = Join(Join(p, "packages"), pk->keys[k]);
            if (!ParseMissionKind(pk->keys[k], mk)) return Fail(tp, "unknown mission kind");
            PackageTemplate& t = doc.packages[static_cast<int>(mk)];
            const json::Value& v = pk->items[k];
            if (!Keys(v, tp, {"main_flights", "flight_size", "sead_per_threat", "sead_max", "escort_ratio", "jammers",
                              "escort_jamming", "decoys", "chaff", "recce_lead", "profiles", "day_parts", "aircraft", "notes"}) ||
                !Int(v, tp, "main_flights", t.main_flights, 1, 12) || !Int(v, tp, "flight_size", t.flight_size, 1, 8) ||
                !Num(v, tp, "sead_per_threat", t.sead_per_threat, 0.0, 8.0) || !Int(v, tp, "sead_max", t.sead_max, 0, 16) ||
                !Num(v, tp, "escort_ratio", t.escort_ratio, 0.0, 4.0) || !Int(v, tp, "jammers", t.jammers, 0, 8) ||
                !OptionalBool(v, tp, "escort_jamming", t.escort_jamming) || !Int(v, tp, "decoys", t.decoys, 0, 16) ||
                !OptionalBool(v, tp, "chaff", t.chaff) || !Int(v, tp, "recce_lead", t.recce_lead, 0, 4) ||
                !LoadProfiles(v, tp, t.profiles) || !LoadDayParts(v, tp, t.day_parts) || !StringList(v, tp, "aircraft", t.aircraft) ||
                !OptionalString(v, tp, "notes", t.notes)) {
                return false;
            }
            if (t.profiles.empty()) return Fail(Join(tp, "profiles"), "at least one");
            for (const std::string& ac : t.aircraft) {
                if (out.Aircraft(ac) == nullptr) return Fail(Join(tp, "aircraft"), "no aircraft '" + ac + "'");
            }
            if (t.day_parts.empty()) return Fail(Join(tp, "day_parts"), "at least one");
            t.allowed = true;
        }
        for (const PhaseDef& ph : doc.phases) {
            for (int k = 0; k < kMissionKinds; ++k) {
                if (ph.apportion[k] > 0.0 && !doc.packages[k].allowed) {
                    return Fail(Join(p, "phases"), "phase '" + ph.id + "' apportions " +
                                                       MissionKindName(static_cast<MissionKind>(k)) + " but has no package for it");
                }
            }
        }
        return true;
    }

    bool LoadIntel(const json::Value& root, FactionDef& out) {
        const json::Value* d = root.Find("intelligence");
        if (d == nullptr) return Fail("intelligence", "required");
        IntelDef& in = out.intel;
        const std::string p = "intelligence";
        double elint_reach_km = in.elint_reach_m / 1000.0, humint_reach_km = in.humint_reach_m / 1000.0;
        if (!Keys(*d, p, {"satellite", "satellite_pass_hours", "satellite_latency_h", "satellite_detect", "recce_latency_h",
                          "elint", "elint_reach_km", "elint_delay_h", "elint_best_m", "elint_k_m", "comint",
                          "comint_sigma_m", "comint_delay_h", "humint_per_day", "humint_delay_h", "humint_sigma_m",
                          "humint_false", "humint_reach_km", "prior_mobile_sigma_m", "bda_overclaim", "pirep_delay_h",
                          "analyst_skill", "source", "confidence", "notes"}) ||
            !Provenance(*d, p) || !OptionalBool(*d, p, "satellite", in.satellite) ||
            !Num(*d, p, "satellite_latency_h", in.satellite_latency_h, 0.0, 200.0) ||
            !Num(*d, p, "satellite_detect", in.satellite_detect, 0.0, 1.0) ||
            !Num(*d, p, "recce_latency_h", in.recce_latency_h, 0.0, 48.0) || !OptionalBool(*d, p, "elint", in.elint) ||
            !Num(*d, p, "elint_reach_km", elint_reach_km, 0.0, 600.0) ||
            !Num(*d, p, "elint_delay_h", in.elint_delay_h, 0.0, 48.0) ||
            !Num(*d, p, "elint_best_m", in.elint_best_m, 1.0, 50000.0) ||
            !Num(*d, p, "elint_k_m", in.elint_k_m, 0.0, 200000.0) || !OptionalBool(*d, p, "comint", in.comint) ||
            !Num(*d, p, "comint_sigma_m", in.comint_sigma_m, 10.0, 100000.0) ||
            !Num(*d, p, "comint_delay_h", in.comint_delay_h, 0.0, 72.0) ||
            !Num(*d, p, "humint_per_day", in.humint_per_day, 0.0, 50.0) ||
            !Num(*d, p, "humint_delay_h", in.humint_delay_h, 0.0, 200.0) ||
            !Num(*d, p, "humint_sigma_m", in.humint_sigma_m, 10.0, 100000.0) ||
            !Num(*d, p, "humint_false", in.humint_false, 0.0, 1.0) ||
            !Num(*d, p, "humint_reach_km", humint_reach_km, 0.0, 1000.0) ||
            !Num(*d, p, "prior_mobile_sigma_m", in.prior_mobile_sigma_m, 10.0, 100000.0) ||
            !Num(*d, p, "bda_overclaim", in.bda_overclaim, 0.0, 5.0) || !Num(*d, p, "pirep_delay_h", in.pirep_delay_h, 0.0, 48.0) ||
            !Num(*d, p, "analyst_skill", in.analyst_skill, 0.0, 1.0)) {
            return false;
        }
        in.elint_reach_m = elint_reach_km * 1000.0;
        in.humint_reach_m = humint_reach_km * 1000.0;
        if (const json::Value* arr = OptionalArray(*d, p, "satellite_pass_hours")) {
            for (std::size_t i = 0; i < arr->items.size(); ++i) {
                if (!arr->items[i].Is(json::Type::Number) || arr->items[i].number < 0.0 || arr->items[i].number >= 24.0) {
                    return Fail(Index(Join(p, "satellite_pass_hours"), i), "a local hour 0 .. 24");
                }
                in.satellite_pass_hours.push_back(arr->items[i].number);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (in.satellite && in.satellite_pass_hours.empty()) return Fail(Join(p, "satellite_pass_hours"), "a satellite needs its passes");
        return true;
    }

    bool LoadLogistics(const json::Value& root, FactionDef& out) {
        const json::Value* d = root.Find("logistics");
        if (d == nullptr) return Fail("logistics", "required");
        const std::string p = "logistics";
        LogisticsDef& l = out.logistics;
        return Keys(*d, p, {"replacements_per_day", "arm_stock", "arm_resupply_per_day", "damaged_return_h", "source",
                            "confidence", "notes"}) &&
               Provenance(*d, p) && Num(*d, p, "replacements_per_day", l.replacements_per_day, 0.0, 100.0) &&
               Num(*d, p, "arm_stock", l.arm_stock, 0.0, 10000.0) &&
               Num(*d, p, "arm_resupply_per_day", l.arm_resupply_per_day, 0.0, 1000.0) &&
               Num(*d, p, "damaged_return_h", l.damaged_return_h, 0.0, 1000.0);
    }

    bool LoadPolitics(const json::Value& root, FactionDef& out) {
        const json::Value* d = root.Find("politics");
        if (d == nullptr) return Fail("politics", "required");
        const std::string p = "politics";
        PoliticsDef& pol = out.politics;
        double city_km = 0.0, depth_km = 0.0;
        if (!Keys(*d, p, {"support", "pressure", "loss_tolerance_per_day", "pow_sensitivity", "collateral_sensitivity",
                          "no_strike_city_radius_km", "max_depth_km", "banned", "interference_per_cycle", "target_weights",
                          "events", "source", "confidence", "notes"}) ||
            !Num(*d, p, "max_depth_km", depth_km, 0.0, 2000.0) ||
            !Provenance(*d, p) || !Num(*d, p, "support", pol.support, 0.0, 1.0) ||
            !Num(*d, p, "pressure", pol.pressure, 0.0, 1.0) ||
            !Num(*d, p, "loss_tolerance_per_day", pol.loss_tolerance_per_day, 0.0, 100.0) ||
            !Num(*d, p, "pow_sensitivity", pol.pow_sensitivity, 0.0, 1.0) ||
            !Num(*d, p, "collateral_sensitivity", pol.collateral_sensitivity, 0.0, 1.0) ||
            !Num(*d, p, "no_strike_city_radius_km", city_km, 0.0, 100.0) || !KindList(*d, p, "banned", pol.banned) ||
            !Num(*d, p, "interference_per_cycle", pol.interference_per_cycle, 0.0, 1.0)) {
            return false;
        }
        pol.no_strike_city_radius_m = city_km * 1000.0;
        pol.max_depth_m = depth_km * 1000.0;
        for (double& w : pol.target_weight) w = 1.0;
        if (const json::Value* tw = d->Find("target_weights")) {
            if (!tw->Is(json::Type::Object)) return Fail(Join(p, "target_weights"), "expected {kind: weight}");
            for (std::size_t k = 0; k < tw->keys.size(); ++k) {
                ObjectKind ok;
                if (!ParseObjectKind(tw->keys[k], ok)) return Fail(Join(Join(p, "target_weights"), tw->keys[k]), "unknown object kind");
                if (!tw->items[k].Is(json::Type::Number) || tw->items[k].number < 0.0) {
                    return Fail(Join(Join(p, "target_weights"), tw->keys[k]), "a weight >= 0");
                }
                pol.target_weight[static_cast<int>(ok)] = tw->items[k].number;
            }
        }
        if (const json::Value* ev = OptionalArray(*d, p, "events")) {
            for (std::size_t i = 0; i < ev->items.size(); ++i) {
                const json::Value& e = ev->items[i];
                const std::string ep = Index(Join(p, "events"), i);
                PoliticalEventDef pe;
                double rad_km = -1.0, ev_depth_km = -1.0;
                if (!Keys(e, ep, {"id", "name", "text", "day_at_least", "losses_today_at_least", "pressure_at_least", "chance",
                                  "once", "unban", "ban", "pause_h", "sortie_factor", "min_altitude", "reinforce_squadron",
                                  "reinforce_count", "arm_resupply", "city_radius_km", "max_depth_km", "notes"}) ||
                    !Num(e, ep, "max_depth_km", ev_depth_km, -1.0, 2000.0) ||
                    !RequireString(e, ep, "id", pe.id) || !RequireString(e, ep, "name", pe.name) ||
                    !RequireString(e, ep, "text", pe.text) || !Int(e, ep, "day_at_least", pe.day_at_least, 0, 1000) ||
                    !Num(e, ep, "losses_today_at_least", pe.losses_today_at_least, -1.0, 1000.0) ||
                    !Num(e, ep, "pressure_at_least", pe.pressure_at_least, -1.0, 1.0) ||
                    !Num(e, ep, "chance", pe.chance, 0.0, 1.0) || !OptionalBool(e, ep, "once", pe.once) ||
                    !KindList(e, ep, "unban", pe.unban) || !KindList(e, ep, "ban", pe.ban) ||
                    !Num(e, ep, "pause_h", pe.pause_h, 0.0, 1000.0) || !Num(e, ep, "sortie_factor", pe.sortie_factor, 0.0, 5.0) ||
                    !Num(e, ep, "min_altitude", pe.min_altitude_profile, -1.0, 2.0) ||
                    !OptionalString(e, ep, "reinforce_squadron", pe.reinforce_squadron) ||
                    !Int(e, ep, "reinforce_count", pe.reinforce_count, 0, 100) ||
                    !Num(e, ep, "arm_resupply", pe.arm_resupply, 0.0, 1000.0) ||
                    !Num(e, ep, "city_radius_km", rad_km, -1.0, 100.0)) {
                    return false;
                }
                pe.city_radius_m = rad_km < 0.0 ? -1.0 : rad_km * 1000.0;
                pe.max_depth_m = ev_depth_km < 0.0 ? -1.0 : ev_depth_km * 1000.0;
                if (!pe.reinforce_squadron.empty() && out.Squadron(pe.reinforce_squadron) == nullptr) {
                    return Fail(Join(ep, "reinforce_squadron"), "no squadron '" + pe.reinforce_squadron + "'");
                }
                pol.events.push_back(std::move(pe));
            }
        } else if (!message_.empty()) {
            return false;
        }
        return true;
    }

    bool LoadTraits(const json::Value& v, const std::string& p, Traits& t) {
        return Keys(v, p, {"boldness", "methodical", "creativity", "rigidity", "ambition", "skepticism", "vindictiveness",
                           "composure"}) &&
               Num(v, p, "boldness", t.boldness, 0.0, 1.0) && Num(v, p, "methodical", t.methodical, 0.0, 1.0) &&
               Num(v, p, "creativity", t.creativity, 0.0, 1.0) && Num(v, p, "rigidity", t.rigidity, 0.0, 1.0) &&
               Num(v, p, "ambition", t.ambition, 0.0, 1.0) && Num(v, p, "skepticism", t.skepticism, 0.0, 1.0) &&
               Num(v, p, "vindictiveness", t.vindictiveness, 0.0, 1.0) && Num(v, p, "composure", t.composure, 0.0, 1.0);
    }
    bool LoadSkills(const json::Value& v, const std::string& p, Skills& s) {
        return Keys(v, p, {"planning", "intelligence", "sead", "airmanship", "leadership", "politics"}) &&
               Num(v, p, "planning", s.planning, 0.0, 1.0) && Num(v, p, "intelligence", s.intelligence, 0.0, 1.0) &&
               Num(v, p, "sead", s.sead, 0.0, 1.0) && Num(v, p, "airmanship", s.airmanship, 0.0, 1.0) &&
               Num(v, p, "leadership", s.leadership, 0.0, 1.0) && Num(v, p, "politics", s.politics, 0.0, 1.0);
    }

    bool LoadPool(const json::Value& root, FactionDef& out) {
        const json::Value* d = root.Find("officer_pool");
        if (d == nullptr) return Fail("officer_pool", "required");
        const std::string p = "officer_pool";
        OfficerPoolDef& pool = out.pool;
        if (!Keys(*d, p, {"ranks", "first_names", "last_names", "backgrounds", "trait_mean", "trait_sd", "skill_mean",
                          "skill_sd", "quirks_min", "quirks_max", "notes"}) ||
            !StringList(*d, p, "first_names", pool.first_names) || !StringList(*d, p, "last_names", pool.last_names) ||
            !StringList(*d, p, "backgrounds", pool.backgrounds) || !Num(*d, p, "trait_sd", pool.trait_sd, 0.0, 0.5) ||
            !Num(*d, p, "skill_sd", pool.skill_sd, 0.0, 0.5) || !Int(*d, p, "quirks_min", pool.quirks_min, 0, 5) ||
            !Int(*d, p, "quirks_max", pool.quirks_max, 0, 6)) {
            return false;
        }
        if (pool.first_names.empty() || pool.last_names.empty()) return Fail(p, "needs first_names and last_names");
        if (pool.backgrounds.empty()) pool.backgrounds.push_back("staff officer");
        if (pool.quirks_max < pool.quirks_min) return Fail(Join(p, "quirks_max"), "below quirks_min");
        const json::Value* r = d->Find("ranks");
        if (r == nullptr || !r->Is(json::Type::Object)) return Fail(Join(p, "ranks"), "required: {role: rank title}");
        for (std::size_t k = 0; k < r->keys.size(); ++k) {
            StaffRole sr;
            if (!ParseStaffRole(r->keys[k], sr)) return Fail(Join(Join(p, "ranks"), r->keys[k]), "unknown staff role");
            if (!r->items[k].Is(json::Type::String)) return Fail(Join(Join(p, "ranks"), r->keys[k]), "expected a string");
            pool.ranks[r->keys[k]] = r->items[k].string;
        }
        if (const json::Value* tm = d->Find("trait_mean")) {
            if (!LoadTraits(*tm, Join(p, "trait_mean"), pool.trait_mean)) return false;
        }
        if (const json::Value* sm = d->Find("skill_mean")) {
            if (!LoadSkills(*sm, Join(p, "skill_mean"), pool.skill_mean)) return false;
        }
        return true;
    }

    bool LoadOfficers(const json::Value& root, FactionDef& out) {
        const json::Value* arr = OptionalArray(root, "", "officers");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& o = arr->items[i];
            const std::string p = Index("officers", i);
            OfficerDef od;
            std::string role;
            if (!Keys(o, p, {"id", "name", "rank", "role", "squadron", "age", "background", "bio", "traits", "skills",
                             "quirks", "flies", "notes"}) ||
                !RequireString(o, p, "id", od.id) || !RequireString(o, p, "name", od.name) ||
                !OptionalString(o, p, "rank", od.rank) || !RequireString(o, p, "role", role) ||
                !OptionalString(o, p, "squadron", od.squadron) || !Int(o, p, "age", od.age, 18, 80) ||
                !OptionalString(o, p, "background", od.background) || !OptionalString(o, p, "bio", od.bio) ||
                !StringList(o, p, "quirks", od.quirks) || !OptionalBool(o, p, "flies", od.flies)) {
                return false;
            }
            if (!ParseStaffRole(role, od.role)) return Fail(Join(p, "role"), "unknown staff role '" + role + "'");
            if (const json::Value* t = o.Find("traits")) {
                if (!LoadTraits(*t, Join(p, "traits"), od.traits)) return false;
            }
            if (const json::Value* s = o.Find("skills")) {
                if (!LoadSkills(*s, Join(p, "skills"), od.skills)) return false;
            }
            if (od.role == StaffRole::SquadronLeader) {
                if (out.Squadron(od.squadron) == nullptr) return Fail(Join(p, "squadron"), "a squadron leader names the squadron");
                od.flies = true;
            }
            if (od.rank.empty()) {
                const auto it = out.pool.ranks.find(StaffRoleName(od.role));
                od.rank = it == out.pool.ranks.end() ? std::string() : it->second;
            }
            od.authored = true;
            out.officers.push_back(std::move(od));
        }
        return true;
    }

    // ---- traits and quirks
    bool LoadQuirks(const json::Value& root, std::vector<QuirkDef>& out) {
        if (!Keys(root, "", {"catalogue_id", "status", "source", "confidence", "notes", "traits", "quirks"}) ||
            !Provenance(root, "")) {
            return false;
        }
        const json::Value* arr = OptionalArray(root, "", "quirks");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("quirks", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& q = arr->items[i];
            const std::string p = Index("quirks", i);
            QuirkDef qd;
            std::vector<std::string> roles;
            if (!Keys(q, p, {"id", "name", "description", "signature", "intel_hint", "weakness", "effects", "tier_weight",
                             "roles", "source", "notes"}) ||
                !RequireString(q, p, "id", qd.id) || !RequireString(q, p, "name", qd.name) ||
                !RequireString(q, p, "description", qd.description) || !RequireString(q, p, "signature", qd.signature) ||
                !RequireString(q, p, "intel_hint", qd.intel_hint) || !OptionalBool(q, p, "weakness", qd.weakness) ||
                !StringList(q, p, "roles", roles)) {
                return false;
            }
            for (const std::string& r : roles) {
                StaffRole sr;
                if (!ParseStaffRole(r, sr)) return Fail(Join(p, "roles"), "unknown staff role '" + r + "'");
                qd.roles.push_back(sr);
            }
            const json::Value* e = q.Find("effects");
            if (e == nullptr || !e->Is(json::Type::Object) || e->keys.empty()) return Fail(Join(p, "effects"), "required: {effect: amount}");
            for (std::size_t k = 0; k < e->keys.size(); ++k) {
                bool known = false;
                for (const std::string& kn : KnownQuirkEffects()) known = known || kn == e->keys[k];
                if (!known) return Fail(Join(Join(p, "effects"), e->keys[k]), "unknown effect (KnownQuirkEffects)");
                if (!e->items[k].Is(json::Type::Number)) return Fail(Join(Join(p, "effects"), e->keys[k]), "expected a number");
                qd.effects[e->keys[k]] = e->items[k].number;
            }
            if (const json::Value* tw = q.Find("tier_weight")) {
                if (!tw->Is(json::Type::Object)) return Fail(Join(p, "tier_weight"), "expected {tier: weight}");
                for (std::size_t k = 0; k < tw->keys.size(); ++k) {
                    Tier t;
                    if (!ParseTier(tw->keys[k], t)) return Fail(Join(Join(p, "tier_weight"), tw->keys[k]), "unknown tier");
                    if (!tw->items[k].Is(json::Type::Number) || tw->items[k].number < 0.0) {
                        return Fail(Join(Join(p, "tier_weight"), tw->keys[k]), "a weight >= 0");
                    }
                    qd.tier_weight[static_cast<int>(t)] = tw->items[k].number;
                }
            }
            for (const QuirkDef& other : out) {
                if (other.id == qd.id) return Fail(p, "duplicate quirk '" + qd.id + "'");
            }
            out.push_back(std::move(qd));
        }
        return true;
    }

    // ---- tuning
    bool LoadTuning(const json::Value& root, TuningTable& out) {
        if (!root.Is(json::Type::Object)) return Fail("", "expected an object");
        for (std::size_t g = 0; g < root.keys.size(); ++g) {
            const std::string& group = root.keys[g];
            if (group == "tuning_id" || group == "status" || group == "source" || group == "confidence" || group == "notes") continue;
            const json::Value& v = root.items[g];
            if (!v.Is(json::Type::Object)) return Fail(group, "expected a group of numbers");
            for (std::size_t k = 0; k < v.keys.size(); ++k) {
                const std::string& key = v.keys[k];
                if (key == "source" || key == "notes" || key == "confidence") continue;
                const std::string flat = group + "." + key;
                if (TuningTable::Defaults().count(flat) == 0) return Fail(flat, "unknown tuning value");
                if (!v.items[k].Is(json::Type::Number)) return Fail(flat, "expected a number");
                out.values[flat] = v.items[k].number;
            }
        }
        return Provenance(root, "");
    }

private:
    static std::string Fmt(double x) {
        std::ostringstream s;
        s << x;
        return s.str();
    }
};

inline bool ReadFile(const std::string& path, std::string& out, std::string* error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream s;
    s << f.rdbuf();
    out = s.str();
    return true;
}

template <typename Out, typename Fn>
bool LoadJsonWith(const std::string& path, Out& out, std::string* error, Fn&& fn) {
    std::string text;
    if (!ReadFile(path, text, error)) return false;
    json::Value root;
    std::string perr;
    if (!json::Parse(text, root, &perr)) {
        if (error != nullptr) *error = path + ": " + perr;
        return false;
    }
    AirWarLoader loader;
    Out tmp;
    if (!fn(loader, root, tmp)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = std::move(tmp);
    return true;
}

inline bool LoadTheatreFileAt(const std::string& path, TheatreDef& out, std::string* error, int depth) {
    std::string text;
    if (!ReadFile(path, text, error)) return false;
    json::Value root;
    std::string perr;
    if (!json::Parse(text, root, &perr)) {
        if (error != nullptr) *error = path + ": " + perr;
        return false;
    }
    TheatreDef base;
    const json::Value* ext = root.Find("extends");
    if (ext != nullptr) {
        if (!ext->Is(json::Type::String) || ext->string.empty()) {
            if (error != nullptr) *error = path + ": extends: the name of a theatre file in the same folder";
            return false;
        }
        if (depth >= 4) {
            if (error != nullptr) *error = path + ": extends: more than four deep (a loop?)";
            return false;
        }
        const std::size_t slash = path.find_last_of("/\\");
        const std::string dir = slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
        if (!LoadTheatreFileAt(dir + ext->string, base, error, depth + 1)) return false;
    }
    AirWarLoader loader;
    TheatreDef tmp;
    if (!loader.LoadTheatre(root, tmp, ext != nullptr ? &base : nullptr)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = std::move(tmp);
    return true;
}

}  // namespace detail

// A theatre file may extend another in its folder ("extends"; AirWarLoader::LoadTheatre says how).
inline bool LoadTheatreFile(const std::string& path, TheatreDef& out, std::string* error = nullptr) {
    return detail::LoadTheatreFileAt(path, out, error, 0);
}

inline bool LoadFactionFile(const std::string& path, FactionDef& out, std::string* error = nullptr) {
    return detail::LoadJsonWith(path, out, error, [](detail::AirWarLoader& l, const json::Value& r, FactionDef& o) {
        return l.LoadFaction(r, o);
    });
}

inline bool LoadQuirksFile(const std::string& path, std::vector<QuirkDef>& out, std::string* error = nullptr) {
    return detail::LoadJsonWith(path, out, error, [](detail::AirWarLoader& l, const json::Value& r, std::vector<QuirkDef>& o) {
        return l.LoadQuirks(r, o);
    });
}

inline bool LoadTuningFile(const std::string& path, TuningTable& out, std::string* error = nullptr) {
    return detail::LoadJsonWith(path, out, error, [](detail::AirWarLoader& l, const json::Value& r, TuningTable& o) {
        return l.LoadTuning(r, o);
    });
}

// Text loaders (tests).
template <typename Out, typename Fn>
bool LoadAirWarJson(const std::string& text, Out& out, std::string* error, Fn&& fn) {
    json::Value root;
    std::string perr;
    if (!json::Parse(text, root, &perr)) {
        if (error != nullptr) *error = perr;
        return false;
    }
    detail::AirWarLoader loader;
    Out tmp;
    if (!fn(loader, root, tmp)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(tmp);
    return true;
}

inline bool LoadFactionJson(const std::string& text, FactionDef& out, std::string* error = nullptr) {
    return LoadAirWarJson(text, out, error, [](detail::AirWarLoader& l, const json::Value& r, FactionDef& o) {
        return l.LoadFaction(r, o);
    });
}

inline bool LoadTheatreJson(const std::string& text, TheatreDef& out, std::string* error = nullptr) {
    return LoadAirWarJson(text, out, error, [](detail::AirWarLoader& l, const json::Value& r, TheatreDef& o) {
        return l.LoadTheatre(r, o);
    });
}

// Everything a campaign needs, loaded together and cross-checked (the faction's front exists in the theatre, every
// quirk the authored officers name exists).
struct AirWarData {
    TheatreDef theatre;
    FactionDef faction;
    std::vector<QuirkDef> quirks;
    TuningTable tuning;

    const QuirkDef* Quirk(const std::string& id) const {
        for (const QuirkDef& q : quirks) {
            if (q.id == id) return &q;
        }
        return nullptr;
    }
};

// data_dir: the repo's data folder. faction_file and theatre_file relative to it ("air_war/faction_nato.json").
inline bool LoadAirWarData(const std::string& data_dir, const std::string& theatre_file, const std::string& faction_file,
                           AirWarData& out, std::string* error = nullptr) {
    AirWarData d;
    if (!LoadTheatreFile(data_dir + "/" + theatre_file, d.theatre, error)) return false;
    if (!LoadFactionFile(data_dir + "/" + faction_file, d.faction, error)) return false;
    if (!LoadQuirksFile(data_dir + "/air_war/traits.json", d.quirks, error)) return false;
    if (!LoadTuningFile(data_dir + "/gameplay/air_war.json", d.tuning, error)) return false;
    if (d.theatre.FrontFor(d.faction.front) == nullptr) {
        if (error != nullptr) *error = faction_file + ": front '" + d.faction.front + "' is not a front of " + theatre_file;
        return false;
    }
    for (const OfficerDef& o : d.faction.officers) {
        for (const std::string& q : o.quirks) {
            if (d.Quirk(q) == nullptr) {
                if (error != nullptr) *error = faction_file + ": officer " + o.id + " has unknown quirk '" + q + "'";
                return false;
            }
        }
    }
    for (const SquadronDef& s : d.faction.squadrons) {
        if (s.leader.empty()) continue;
        bool found = false;
        for (const OfficerDef& o : d.faction.officers) found = found || o.id == s.leader;
        if (!found) {
            if (error != nullptr) *error = faction_file + ": squadron " + s.id + " names unknown leader '" + s.leader + "'";
            return false;
        }
    }
    out = std::move(d);
    return true;
}

}  // namespace airwar
}  // namespace simcore
