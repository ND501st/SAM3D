// simcore/air_war/state.hpp — the enemy air command's working state: packages and flights, what happened to them, the
// squadrons, the player's side as it really is, politics, and the journal (docs/air_war/04_PLANNING.md,
// 05_EXECUTION.md).
#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/types.hpp"

namespace simcore {
namespace airwar {

struct RoutePoint {
    Vec3 pos;          // theatre ENU [m]; z unused
    double agl_m = 0;  // height above the ground there
    double t_h = 0;    // campaign hour at this point
    std::string tag;   // "entry", "ip", "target", "egress", "exit", "orbit", "loiter"
};

struct Flight {
    std::string callsign;
    FlightRole role = FlightRole::Strike;
    std::string squadron;
    std::string aircraft;
    int count = 2;
    Profile profile = Profile::Low;
    std::vector<RoutePoint> route;
    int release_index = -1;   // route index where the weapons go
    std::string aim_key;      // the intel entry aimed at
    Vec3 aim_pos;             // where the weapons are aimed (the believed position)
    int officer = -1;         // a staff officer flying in it (the lead aircraft)
    int arms = 0;             // ARMs carried by the flight
    // What happened (filled by the resolution).
    int lost = 0;
    int damaged = 0;
    bool aborted = false;
    bool released = false;
};

struct Package {
    int id = 0;
    std::string name;
    MissionKind kind = MissionKind::Strike;
    std::string target_key;     // the intel entry it is planned against
    std::string target_object;  // the real object behind it ("" for a ghost)
    std::string target_label;
    Vec3 target_pos;            // believed
    double tot_h = 0.0;
    DayPart part = DayPart::Day;
    Profile profile = Profile::Low;
    std::string entry;
    std::string exit;
    std::vector<Flight> flights;
    int planner = -1;
    int commander = -1;
    int leader = -1;            // the officer who leads it in the air (-1: none of the staff)
    double expected_losses = 0.0;
    double expected_value = 0.0;
    double utility = 0.0;
    double coordination = 1.0;  // how well the SEAD, jamming and strike are timed together
    double load_factor = 1.0;   // < 1: wrong or partial loads (a rushed plan)
    bool interference = false;  // higher authority set the time on target
    std::string tactic;         // TacticKey
    std::vector<std::string> rationale;
    bool resolved = false;
    bool played = false;        // flown as the player's mission

    int Aircraft() const {
        int n = 0;
        for (const Flight& f : flights) n += f.count;
        return n;
    }
    int Main() const {
        int n = 0;
        for (const Flight& f : flights) {
            if (f.role == FlightRole::Strike || f.role == FlightRole::Dead || f.role == FlightRole::Helicopter) n += f.count;
        }
        return n;
    }
    double FirstH() const {
        double t = 1e18;
        for (const Flight& f : flights) {
            if (!f.route.empty() && f.route.front().t_h < t) t = f.route.front().t_h;
        }
        return t;
    }
    double LastH() const {
        double t = -1e18;
        for (const Flight& f : flights) {
            if (!f.route.empty() && f.route.back().t_h > t) t = f.route.back().t_h;
        }
        return t;
    }
    bool Has(FlightRole r) const {
        for (const Flight& f : flights) {
            if (f.role == r && f.count > 0) return true;
        }
        return false;
    }
    int Count(FlightRole r) const {
        int n = 0;
        for (const Flight& f : flights) {
            if (f.role == r) n += f.count;
        }
        return n;
    }
};

// What a package did and suffered, as it truly happened (from the abstract resolver or from a played mission).
struct LossRecord {
    int flight = -1;
    std::string by;   // the truth object id of the defence that shot it down ("" : fighters or unknown)
    Vec3 where;
    bool officer = false;  // a staff officer was in it
    bool captured = false; // the crew came down alive in the player's country
};

struct PackageOutcome {
    int package_id = 0;
    std::vector<LossRecord> losses;
    int damaged = 0;
    std::vector<std::pair<std::string, double>> damage;   // truth object id -> working share removed
    std::vector<std::string> destroyed;                   // truth object ids wrecked by it
    std::vector<std::string> decoys_hit;
    bool hit_nothing = false;                             // the aim point was empty (stale or a ghost)
    std::vector<std::string> radars_killed;
    std::vector<std::string> radars_suppressed;           // switched off under ARM attack
    std::vector<std::pair<std::string, double>> emissions;  // truth object id -> seconds it radiated during the raid
    std::vector<std::string> fired;                       // defences seen to launch
    std::vector<std::string> recce_seen;                  // objects the reconnaissance photographed
    int missiles_spent = 0;                               // the player's side's missiles, all defences
    std::vector<std::pair<std::string, int>> missiles_by;  // (a played mission) rounds our other defences spent: the neighbours
    int arms_fired = 0;
    bool aborted = false;
    bool delivered = false;
    double collateral = 0.0;
    std::vector<std::string> notes;
    std::vector<int> released_flights;  // flights that released their weapons (a played mission)
    std::vector<int> aborted_flights;   // flights that broke away before their release point (a played mission)
    bool truth_applied = false;  // the damage is already in the truth (the abstract resolver); else the campaign applies it
    bool played = false;         // from a played mission (the mission bridge)

    void AddEmission(const std::string& id, double s) {
        for (auto& e : emissions) {
            if (e.first == id) {
                e.second += s;
                return;
            }
        }
        emissions.push_back({id, s});
    }
    void AddDamage(const std::string& id, double d) {
        for (auto& e : damage) {
            if (e.first == id) {
                e.second += d;
                return;
            }
        }
        damage.push_back({id, d});
    }
    int Lost() const { return static_cast<int>(losses.size()); }
};

struct SquadronState {
    std::string id;
    std::string aircraft;
    int total = 0;
    int lost = 0;
    std::vector<double> damaged_until;  // campaign hour each damaged aircraft returns
    double sorties_left = 0.0;          // this cycle

    int Damaged(double now_h) const {
        int n = 0;
        for (double t : damaged_until) {
            if (t > now_h) ++n;
        }
        return n;
    }
    int Serviceable(double now_h, double serviceability) const {
        const int whole = total - lost - Damaged(now_h);
        return whole <= 0 ? 0 : static_cast<int>(whole * serviceability + 0.5);
    }
};

// The player's side as it really is.
struct ObjectState {
    std::string id;
    Vec3 pos;
    double function = 1.0;
    int missiles = -1;
    double last_moved_h = 0.0;
    double wrecked_h = -1.0;  // when it was wrecked (function 0)
};

// How the player's battery behaves between and during fights: the commander's choices the enemy reacts to.
struct BatteryPosture {
    double emission = 0.3;           // share of each hour its radars radiate when not fighting (0: strict EMCON)
    double emcon_discipline = 0.5;   // switching off when ARMs come, brief bursts: 0..1
    double radio_discipline = 0.5;   // little voice traffic: 0..1
    bool dummy_traffic = false;      // empty messages on the net (em/deception.hpp)
    int dummies = 2;                 // dummy positions
    std::string decoy_mode = "when_radar_off";  // their decoy emitters: always, with_radar, when_radar_off, "" none
    bool nets = true;
    bool emplacement = true;
    double relocate_every_h = 0.0;   // 0: stays where it is
    bool fight_silent = false;       // the battery's radars stay silent; fights by the TV on the PU-12's designations
};

struct PoliticsState {
    double support = 0.7;
    double pressure = 0.3;
    std::vector<ObjectKind> banned;
    double city_radius_m = 0.0;
    double max_depth_m = 0.0;  // 0: no limit
    double paused_until_h = -1e18;
    double sortie_factor = 1.0;
    int min_profile = 0;  // 0 any, 1 medium or higher (loss avoidance ordered from above)
    std::set<std::string> fired_events;
    double losses_today = 0.0;
    double losses_total = 0.0;
    double damage_today = 0.0;
    int pows = 0;

    bool Banned(ObjectKind k) const {
        for (ObjectKind b : banned) {
            if (b == k) return true;
        }
        return false;
    }
};

// The journal: the enemy's mind (hidden from the player until the debrief) and what the player's side learns.
struct JournalEntry {
    double h = 0.0;
    std::string kind;   // "plan", "decision", "result", "intel", "career", "politics", "own_intel", "player_mission"
    std::string who;    // an officer's title, "intelligence", "politics" ...
    std::string text;
    bool visible = false;  // the player's side knows this (an intelligence summary, a POW's interrogation)
};

}  // namespace airwar
}  // namespace simcore
