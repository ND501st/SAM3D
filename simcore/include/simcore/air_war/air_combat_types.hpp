// simcore/air_war/air_combat_types.hpp — the air battle's air-to-air weapons and sensors as data (roadmap track B, B7;
// docs/air_battle/07_AIR_TO_AIR.md section 5.1, 03_PERCEPTION.md section 4). Plain structs and the lookups on them;
// the loaders are in air_combat_data.hpp, the shot as a chain of chances in air_combat.hpp.
//
// Nothing here knows a war. Every missile, gun and sensor carries the year it entered service (and the year its allies
// got it), and the war's year (TheatreDef::year) decides what is offered: AamDef::InService(year, allied),
// AirCombatDef::LoadOutFor(aircraft, year, allied), SensorsDef::InService.
#pragma once

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace simcore {
namespace airwar {

// How a missile finds its target. The class sets the aspect it can be fired from and what defeats it.
enum class Guidance {
    IrRear,       // an uncooled infrared seeker: the rear hemisphere only (K-13, R-13M, R-60, AIM-9B/J)
    IrAllAspect,  // a cooled seeker that sees the target from any side (AIM-9L/M, R-24T, R-60M close in, R-73)
    Sarh,         // semi-active radar: the launcher illuminates until impact (AIM-7, R-23R)
    SarhInertial  // semi-active with a stretch of inertial flight if the lock is lost (R-24R: 10 s)
};

inline const char* GuidanceName(Guidance g) {
    switch (g) {
        case Guidance::IrRear: return "ir_rear";
        case Guidance::IrAllAspect: return "ir_all_aspect";
        case Guidance::Sarh: return "sarh";
        case Guidance::SarhInertial: return "sarh_inertial";
    }
    return "?";
}

inline bool ParseGuidance(const std::string& s, Guidance& out) {
    for (Guidance g : {Guidance::IrRear, Guidance::IrAllAspect, Guidance::Sarh, Guidance::SarhInertial}) {
        if (s == GuidanceName(g)) {
            out = g;
            return true;
        }
    }
    return false;
}

inline bool IsInfrared(Guidance g) { return g == Guidance::IrRear || g == Guidance::IrAllAspect; }
inline bool IsSemiActive(Guidance g) { return g == Guidance::Sarh || g == Guidance::SarhInertial; }

// An air-to-air missile (data/air_war/air_combat.json "aams"). Ranges are at the reference height of the chain
// (ChainParams::alt_ref_m); low_factor is the share of them left at the ground.
struct AamDef {
    std::string id;
    std::string name;
    Guidance guidance = Guidance::IrRear;
    int in_service_year = 0;       // 0: always
    int allies_year = 0;           // the year the Pact's allies and our side got it; 0: the same as in_service_year
    double max_head_m = 0.0;       // from head-on (aspect 180 deg); 0 with a rear-aspect seeker
    double max_rear_m = 0.0;       // from astern (aspect 0)
    double low_factor = 1.0;       // of the maximum range near the ground
    double min_m = 0.0;
    double launch_g = 4.0;         // the most the launcher may pull at launch
    double seeker_cone_rad = 0.2;  // the target's angle off the nose (or the radar's boresight) the seeker accepts
    double target_g = 8.0;         // the target's load the missile can follow
    double sun_exclusion_rad = 0.0;  // an infrared seeker finds nothing nearer than this to the sun (0: no such limit)
    double clutter_below = 1.0;    // share of guidance left when the target is below the launcher against the ground
    double autonomous_s = 0.0;     // flight on its own after the launcher's lock is lost (R-24R: 10 s)
    double flare_rejection = 0.0;  // 0..1: the share of a countermeasure's effect it sees through (flares, chaff, jamming)
    double reliability = 0.8;      // the round works after carriage (a fraction that guides)
    double design_pk = 0.5;        // the designers' kill chance for a perfect launch: the ceiling of the last link
    double lethal_m = 5.0;         // the warhead's lethal radius
    double mean_mps = 600.0;       // mean speed in flight
    std::string munition;          // the shape in data/scenery/munitions.json (B13 adds the air-to-air shapes)
    std::vector<std::string> game_values;  // the keys of this entry that are SAM3D's own, not sourced
    std::string source;
    std::string confidence;
    std::string notes;

    // In a war of `year`; `allied`: the shooter is a Pact ally or our side (the Soviet equipment's second year).
    bool InService(int year, bool allied = false) const {
        const int y = allied && allies_year > 0 ? allies_year : in_service_year;
        return y <= 0 || y <= year;
    }
};

// A gun (data/air_war/air_combat.json "guns"). A gun pass is a firing window, not rounds (07 section 3.3).
struct AirGunDef {
    std::string id;
    std::string name;
    double firing_s = 5.0;       // seconds of fire in the magazine
    double effective_m = 800.0;  // beyond this a burst does nothing
    double kill_per_s = 0.1;     // game value: kills per second of fire inside range, at a perfect aim
    int in_service_year = 0;
    std::vector<std::string> game_values;
    std::string source;
    std::string confidence;
    std::string notes;

    bool InService(int year) const { return in_service_year <= 0 || in_service_year <= year; }
};

// What a type carries from a year on: the latest era at or before the war's year is the fit.
struct LoadEra {
    int from_year = 0;
    std::vector<std::pair<std::string, int>> aams;  // missile id, count
    std::string gun;                                // gun id, or none
};

// An aircraft's air-to-air fit (the radar and infrared set are the type's entries in sensors.json).
struct FitDef {
    std::string id;                    // the aircraft type's id
    std::vector<std::string> aliases;  // other ids that mean the same fit (the order of battle's "mig21")
    std::string iff = "none";          // mk12, sro2m, parol, kremniy2m, none
    bool command_link = false;         // Lazur or its kind: the ground's commands to the autopilot
    double vid_aid_m = 0.0;            // identification range beyond the eye's (TISEO, rifle scopes); 0: none
    std::vector<LoadEra> loads_by_year;  // in rising order of from_year
    std::string export_fit;            // the id of the stripped client variant's fit, or none
    std::string source;
    std::string confidence;
    std::string notes;

    bool Is(const std::string& type_id) const {
        return id == type_id || std::find(aliases.begin(), aliases.end(), type_id) != aliases.end();
    }
};

// The missiles and the gun a type carries in a war of a given year.
struct LoadOut {
    std::vector<std::pair<const AamDef*, int>> aams;
    const AirGunDef* gun = nullptr;

    bool Empty() const { return aams.empty() && gun == nullptr; }
    int Count(Guidance g) const {
        int n = 0;
        for (const auto& a : aams) n += a.first->guidance == g ? a.second : 0;
        return n;
    }
    int SemiActive() const { return Count(Guidance::Sarh) + Count(Guidance::SarhInertial); }
    bool Carries(const std::string& id) const {
        for (const auto& a : aams) {
            if (a.first->id == id) return true;
        }
        return false;
    }
};

// A faction's way of fighting in the air (07 section 4); the values are strings and shares the air combat layer reads.
struct AirDoctrineDef {
    std::string faction;               // nato, warsaw_pact, neutral, rebels, ours
    std::string roe;                   // vid, cleared_bvr, controller
    std::string commit_by;             // leader, controller
    std::string initiative_at_contact; // flight_leader, controller, by_skill
    std::string formation;             // line_abreast_pair, sword_shield, pair, single
    std::string sort;                  // left_low_lead, controller, nearest
    std::vector<std::string> shot_order;  // sarh, ir, gun, in the order a shooter tries them
    std::string tactic;                // turn, slash, beam
    bool cooperative_id = false;       // Combat Tree style interrogation of the enemy's IFF
    double strikers_jettison = 0.0;    // a striker's answer to an interceptor, trained crews (04_REACTIONS.md section 8)
    double strikers_press_on = 0.0;
    double strikers_abort = 0.0;
    // The fight (roadmap B8; 07 sections 4.2, 4.5, 5.5). skill: the faction's air_combat_skill (10_FACTIONS.md 7.2).
    double skill = 0.5;
    // What a flight does when it loses its controller, a green pilot's and a skilled one's: abort, pairs, continue.
    std::string control_lost_green = "abort";
    std::string control_lost_skilled = "pairs";
    // The believed ratio of own to enemy fighters a leader who commits himself needs (0: the controller commits).
    double commit_odds = 0.0;
    // How its planner shares the fighters it sends with a package: close escort, a sweep ahead, a detached CAP; the sweep's
    // lead on the strike's time over target.
    double plan_close = 1.0;
    double plan_sweep = 0.0;
    double plan_cap = 0.0;
    double sweep_lead_s = 0.0;
    std::string source;
    std::string confidence;
    std::string notes;

    // A radar shot needs the target identified by eye (vid), else the controller's word clears it.
    bool NeedsVid() const { return roe == "vid"; }
    bool ControllerBound() const { return commit_by == "controller"; }
    // What a pilot of this skill does when he loses his controller (green below 0.5).
    const std::string& OnControlLost(double pilot_skill) const { return pilot_skill < 0.5 ? control_lost_green : control_lost_skilled; }
};

// The fight's game values (data/air_war/air_combat.json "fight"; 07 sections 5.3, 5.6): the live fight's and the abstract
// one's. Every one is a game value; the data's notes give the period anchor where there is one.
struct FightParams {
    // Live.
    double think_s = 0.5;
    double radar_scan_s = 2.0;
    double radar_az_rad = 1.0472;
    double contact_hold_s = 10.0;
    double vid_m = 5500.0;
    double friendly_clear_m = 20000.0;
    double gci_vector_s = 20.0;
    double gci_update_s = 10.0;
    double gci_error_m = 1000.0;
    double control_lost_s = 60.0;
    double joker_s = 600.0;
    double bingo_s = 900.0;
    double break_g = 6.0;
    double gun_cone_rad = 0.1745;
    double gun_aspect_rad = 1.0472;
    double extend_s = 20.0;
    double claim_on_miss = 0.3;
    double slow_target_mps = 120.0;
    // Abstract (A0).
    double a0_intercept = 0.5;
    double a0_detect_day = 0.85;
    double a0_detect_night = 0.45;
    double a0_detect_poor = 0.6;
    double a0_detect_low = 0.6;
    double a0_rounds = 2.0;
    double a0_meet_sweep = 0.6;
    double a0_meet_cap = 0.4;
    double a0_meet_close = 0.5;
    double a0_awacs_meet = 1.5;
    double a0_awacs_meet_mode0 = 1.5;  // the factor before roadmap B14 (balance.mode 0: a save made before it)
    double a0_surprise_gci = 0.6;
    double a0_surprise_jammed = 0.2;
    double a0_seen_awacs = 0.9;
    double a0_seen_eye = 0.2;
    double a0_gci_jammed = 0.5;
    double a0_green_home = 0.3;
    double a0_sarh_bvr_m = 15000.0;
    double a0_sarh_vid_m = 5000.0;
    double a0_ir_m = 3000.0;
    double a0_gun_m = 600.0;
    double a0_range_sigma = 0.4;
};

// Our fighter regiments as the abstract fight sees them (data/air_war/air_combat.json "ours"), until B10 makes them actors.
struct OurRegimentDef {
    std::string id;                                  // the order of battle's regiment (data/command/oob_pannonia.json)
    std::vector<std::pair<std::string, int>> types;  // fit id, count
};
struct OursDef {
    std::vector<OurRegimentDef> regiments;
    int Total() const {
        int n = 0;
        for (const OurRegimentDef& r : regiments) {
            for (const auto& t : r.types) n += t.second;
        }
        return n;
    }
};

// The numbers of the chain of chances (air_combat.hpp). Game values calibrated until the period's rates come out
// (the `calibration` section's `chain` and `mix`): every one is labelled so in the data.
struct ChainParams {
    double alt_ref_m = 12000.0;            // the height the missiles' maximum ranges hold at
    double rear_aspect_limit_rad = 1.5708; // an uncooled seeker needs the target's tail within this of its line of sight
    // The pilot's judgement of the shot (the error shrinks with skill, grows with stress).
    double judge_range_sigma = 0.35;       // of the range, at zero skill and no stress
    double judge_aspect_sigma_rad = 0.45;
    double judge_cone_sigma_rad = 0.20;
    double judge_g_sigma = 0.35;           // of the launch load
    double judge_skill_gain = 0.8;         // sigma x (1 - gain x skill)
    double judge_stress_gain = 0.5;        // sigma x (1 + gain x stress)
    double judge_sarh_scale = 1.0;         // a radar round asks more of a pilot's judgement
    // The round works.
    double switch_error_base = 0.12;       // after a change of mode, at zero skill and no stress
    double switch_idle_share = 0.2;        // of it with no change of mode
    double switch_skill_gain = 0.7;
    double switch_stress_gain = 1.0;
    // The seeker guides: a base by class, then multipliers.
    double guides_ir_rear = 0.5;
    double guides_ir_all_aspect = 0.8;
    double guides_sarh = 0.5;
    double guides_sarh_inertial = 0.5;
    double aspect_ir_rear_edge = 0.5;      // an uncooled seeker's share at the edge of its hemisphere
    double aspect_ir_all_head = 0.85;      // an all-aspect seeker's share head-on
    double sun_soft_factor = 1.667;        // the sun degrades the seeker out to this many exclusion angles (30 to 50 deg)
    double sun_soft_floor = 0.3;           // ... leaving this share at the exclusion's edge
    double beam_notch = 0.5;               // a radar round's share in the beam against the ground (AFIT 1982)
    double beam_notch_half_rad = 0.35;     // the beam: 90 deg either way by this
    double break_max = 0.6;                // the most a perfectly timed break at the missile's limit takes off guidance
    double break_miss_m = 6.0;             // the miss it adds, rms
    double nominal_miss_m = 1.5;           // the miss of a launch with no break, rms
    // Guns.
    double gun_skill_floor = 0.4;
    double gun_range_edge = 0.5;           // the share of aim left at the effective range
    double gun_g_loss = 0.4;               // the most a manoeuvring target takes off a burst's aim
    double gun_g_full = 8.0;               // ... at this load
};

// One 1967 engagement's mix: where shots are taken from and what the target does (the calibration's reference).
struct CalibrationMix {
    double skill = 0.3;                // the 1967 crews' (SAM3D's estimate)
    double stress = 0.4;
    double mode_change_share = 0.5;    // of launches after a change of mode or hard manoeuvring
    double range_median_m = 2800.0;    // shots are taken from a lognormal range with this median ...
    double range_sigma_ln = 0.75;      // ... and this spread
    double aspect_rear_share = 0.7;    // of attacks from the rear hemisphere (the rest from the front)
    double off_boresight_sigma_rad = 0.2;  // the target's angle off the nose: the magnitude of a normal with this spread
    double altitude_m = 5000.0;        // the shooter's height
    double launch_g_mean = 2.0;        // the launcher's load: 1 g plus an exponential of this mean minus one
    double break_share = 0.5;          // of targets that see the shot and break
    double break_g = 6.0;
    double look_down_share = 0.3;      // of shots at a target below the launcher against the ground
    double sun_share = 0.05;           // of shots with the sun in the seeker's view
    double countermeasure_share = 0.0; // of shots with flares or chaff in the picture
};

// A published rate or ratio the chain and the benches are held to.
struct CalibrationTarget {
    double value = 0.0;
    double tolerance = 0.0;  // absolute, game value
    std::string source;
    std::string confidence;
    std::string notes;
};

struct CalibrationDef {
    ChainParams chain;
    CalibrationMix mix;
    std::map<std::string, CalibrationTarget> targets;

    const CalibrationTarget* Target(const std::string& key) const {
        const auto it = targets.find(key);
        return it == targets.end() ? nullptr : &it->second;
    }
};

// data/air_war/air_combat.json.
struct AirCombatDef {
    std::vector<AamDef> aams;
    std::vector<AirGunDef> guns;
    std::vector<FitDef> fits;
    std::vector<AirDoctrineDef> doctrines;
    CalibrationDef calibration;
    FightParams fight;  // "fight" (B8)
    OursDef ours;       // "ours" (B8)

    const AamDef* Aam(const std::string& id) const {
        for (const AamDef& a : aams) {
            if (a.id == id) return &a;
        }
        return nullptr;
    }
    const AirGunDef* Gun(const std::string& id) const {
        for (const AirGunDef& g : guns) {
            if (g.id == id) return &g;
        }
        return nullptr;
    }
    const FitDef* Fit(const std::string& type_id) const {
        for (const FitDef& f : fits) {
            if (f.Is(type_id)) return &f;
        }
        return nullptr;
    }
    const AirDoctrineDef* Doctrine(const std::string& faction) const {
        for (const AirDoctrineDef& d : doctrines) {
            if (d.faction == faction) return &d;
        }
        return nullptr;
    }

    // What a type carries in a war of `year`: its latest era at or before it, each missile and the gun checked against
    // the year (a missile not yet in service for this side is not offered). A type with no fit carries nothing.
    LoadOut LoadOutFor(const std::string& type_id, int year, bool allied) const {
        LoadOut out;
        const FitDef* f = Fit(type_id);
        if (f == nullptr) return out;
        const LoadEra* era = nullptr;
        for (const LoadEra& e : f->loads_by_year) {
            if (e.from_year <= year) era = &e;
        }
        if (era == nullptr) return out;
        for (const auto& a : era->aams) {
            const AamDef* m = Aam(a.first);
            if (m != nullptr && m->InService(year, allied)) out.aams.push_back({m, a.second});
        }
        const AirGunDef* g = era->gun.empty() ? nullptr : Gun(era->gun);
        if (g != nullptr && g->InService(year)) out.gun = g;
        return out;
    }
};

// ------------------------------------------------------------------------------------------------ sensors

// What an airborne sensor is (03_PERCEPTION.md section 4).
enum class SensorKind { Radar, Irst, OpticalAid, Rwr, Countermeasure, CommandLink };

inline const char* SensorKindName(SensorKind k) {
    switch (k) {
        case SensorKind::Radar: return "radar";
        case SensorKind::Irst: return "irst";
        case SensorKind::OpticalAid: return "optical_aid";
        case SensorKind::Rwr: return "rwr";
        case SensorKind::Countermeasure: return "countermeasure";
        case SensorKind::CommandLink: return "command_link";
    }
    return "?";
}

inline bool ParseSensorKind(const std::string& s, SensorKind& out) {
    for (SensorKind k : {SensorKind::Radar, SensorKind::Irst, SensorKind::OpticalAid, SensorKind::Rwr, SensorKind::Countermeasure,
                         SensorKind::CommandLink}) {
        if (s == SensorKindName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

struct SensorDef {
    std::string id;
    std::string name;
    SensorKind kind = SensorKind::Radar;
    std::vector<std::string> carriers;  // aircraft type ids
    int in_service_year = 0;
    int allies_year = 0;
    bool placeholder = false;           // the figures are SAM3D's estimates until a source is found
    // A radar: its EM RadarDef (in `library`) and the published ranges, in metres, 0 where none is given.
    std::string radar_def;
    std::string library;                // the EM library file, relative to data/ ("em/fighter_radars_1985.json")
    double rcs_m2 = 0.0;                // the target the ranges are for
    double by_hand_m = 0.0;             // the pilot searching on his own
    double cued_m = 0.0;                // under ground guidance
    double lock_m = 0.0;                // lock head-on
    double design_m = 0.0;              // on paper
    double clutter_m = 0.0;             // looking down against the ground
    // An infrared search set or an optical aid.
    double rear_m = 0.0;                // from the rear against a clear sky (an F-4 from the rear for the TP-23)
    double vid_m = 0.0;                 // recognition range it gives beyond the eye's
    std::string source;
    std::string confidence;
    std::string notes;

    bool InService(int year, bool allied = false) const {
        const int y = allied && allies_year > 0 ? allies_year : in_service_year;
        return y <= 0 || y <= year;
    }
    bool CarriedBy(const std::string& type_id) const {
        return std::find(carriers.begin(), carriers.end(), type_id) != carriers.end();
    }
};

// A sensor the research has no year for: it stays out of the war; the slot is here so it can be filled.
struct SensorSlotDef {
    std::string id;
    std::string name;
    SensorKind kind = SensorKind::Radar;
    std::vector<std::string> carriers;
    std::string needs;  // what source would fill it
};

// data/air_war/sensors.json.
struct SensorsDef {
    std::vector<SensorDef> sensors;
    std::vector<SensorSlotDef> slots;

    const SensorDef* Find(const std::string& id) const {
        for (const SensorDef& s : sensors) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }
    // The sensors of a kind a type carries in a war of `year`.
    std::vector<const SensorDef*> Of(const std::string& type_id, SensorKind kind, int year, bool allied) const {
        std::vector<const SensorDef*> out;
        for (const SensorDef& s : sensors) {
            if (s.kind == kind && s.CarriedBy(type_id) && s.InService(year, allied)) out.push_back(&s);
        }
        return out;
    }
};

// Declared here, defined in air_combat_data.hpp (which data.hpp includes last), so AirWarData can load them.
inline bool LoadAirCombatFile(const std::string& path, AirCombatDef& out, std::string* error = nullptr);
inline bool LoadSensorsFile(const std::string& path, SensorsDef& out, std::string* error = nullptr);

}  // namespace airwar
}  // namespace simcore
