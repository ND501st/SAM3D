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
    std::string tag;   // "entry", "letdown", "checkpoint", "gate", "turn", "ip", "target", "egress", "climb", "exit",
                       // "orbit", "loiter" (docs/air_battle/01_ROUTES.md §6.1)
    double leg_mps = 0;   // the speed on the leg from this point to the next (0: the route's one speed, TimeRoute's old form)
    std::string feature;  // the map feature it is on (route_planner.hpp: "settlement:<id>", "crossing:<id>"...), "" computed
};

// The Pact's planned counter-SAM programme for one believed zone its route crosses (01_ROUTES.md §9; flown by B4): full
// speed to the zone's nearest boundary, down, the jammers on, back on the route at `rejoin`.
struct ZoneEscape {
    std::string threat;        // the intel entry's key
    int first = -1;            // the first route index whose leg is inside the believed ring
    int last = -1;             // the last such index
    double heading_rad = 0.0;  // to the nearest boundary from where it enters, clockwise from North
    double dz_m = 0.0;         // the height change (down)
    bool full_speed = true;
    bool jam = true;
    int rejoin = -1;           // the route point it is back on
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
    // Its weapons and how it puts them on the target (the planner's weaponeering, docs/air_war/10_WEAPONS.md); no
    // weapon: the flight strikes by its type's strike_power (Delivery::Count: no attack profile).
    std::string weapon;
    int weapons = 0;          // on each aircraft
    Delivery delivery = Delivery::Count;
    std::vector<ZoneEscape> escapes;  // the Pact's counter-SAM programme, one per believed zone crossed (route_planner.hpp)
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
    int refuelled = 0;          // aircraft that need the tankers to reach the target and back (docs/air_war/11_SUPPORT.md)
    bool interference = false;  // higher authority set the time on target
    std::string tactic;         // TacticKey
    // The route planner's (docs/air_battle/01_ROUTES.md, route.mode 1): the main flights' corridor ("S2 via
    // settlement:kovar"), the variant flown (0 covered, 1 other flank, 2 direct), the gate's crossing as planned (before
    // the punctuality error) and its window (NATO), and a request for corridor suppression there (jsead_1982).
    std::string corridor;
    int variant = 0;
    double gate_h = -1.0;
    double gate_window_s = 0.0;
    bool corridor_suppression = false;
    // Timed into our fighters' corridor its staff believes in (S1, docs/air_battle/08_OUR_FIGHTERS.md section 9): the
    // corridor's id, and whether the belief found the real one (its defences then hold fire for ours at its hours).
    std::string own_corridor;
    bool own_corridor_found = false;
    std::vector<std::string> rationale;
    bool resolved = false;
    bool played = false;        // flown as the player's mission
    std::string found;          // an armed reconnaissance: the column it found on its stretch ("" nothing)

    // Aircraft in it (a cruise missile salvo has none: Missiles()).
    int Aircraft() const {
        int n = 0;
        for (const Flight& f : flights) n += f.role == FlightRole::Missile ? 0 : f.count;
        return n;
    }
    int Missiles() const {
        int n = 0;
        for (const Flight& f : flights) n += f.role == FlightRole::Missile ? f.count : 0;
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
    std::string by;   // the truth object id of the defence that shot it down; our fighter regiment's id when `fighter`; "": unknown
    Vec3 where;
    bool officer = false;  // a staff officer was in it
    bool captured = false; // the crew came down alive in the player's country
    bool fighter = false;  // shot down by our fighters (roadmap B8, air_fight.hpp): `by` is the regiment
    bool own_fire = false; // shot down by its own side's ground air defence coming home (S1): `by` is its belt
};

// An order a played package's flight was given from outside it (roadmap B1, flights.hpp): retasked, diverted, held.
struct RetaskRecord {
    int flight = -1;     // the package's flight
    std::string kind;    // the order (flights.hpp OrderKindId)
    std::string by;      // who gave it (a ground node's id, "instructor", "session")
    double t_s = 0.0;    // when (the session's seconds)
};

// One call our intercept posts logged of a played package's flights (roadmap B9, docs/air_battle/09 section 5.4): as the
// post wrote it, right or wrong, never the truth. The save keeps it so a loaded campaign rebuilds the intercept facts
// without replaying the session.
struct HeardRecord {
    std::string post;        // the intercept post that logged it
    double t_s = 0.0;        // when it was heard (the session's seconds)
    std::string kind;        // the call's kind (air_radio.hpp CallKindId) when it was read, "traffic" when not
    std::string text;        // what the post wrote
    bool understood = false;
    std::string fact;        // the key of the fact made of it in our side's pipeline ("" none)
};

struct PackageOutcome {
    int package_id = 0;
    std::vector<LossRecord> losses;
    int damaged = 0;
    // A played session's damaged aircraft that got away (roadmap S4): its flight and whether heavily damaged (else
    // moderately). The campaign repairs each by its level (damage.mode 1); the rest of `damaged` (the abstract resolver's)
    // go to a flight drawn as before.
    struct DamagedRecord {
        int flight = -1;
        bool heavy = false;
    };
    std::vector<DamagedRecord> damaged_list;
    std::vector<std::pair<std::string, double>> damage;   // truth object id -> working share removed
    std::vector<std::string> destroyed;                   // truth object ids wrecked by it
    std::vector<std::string> decoys_hit;
    bool hit_nothing = false;                             // the aim point was empty (stale or a ghost)
    std::vector<std::string> radars_killed;
    std::vector<std::string> radars_suppressed;           // switched off under ARM attack
    std::vector<std::pair<std::string, double>> emissions;  // truth object id -> seconds it radiated during the raid
    std::vector<std::string> fired;                       // defences seen to launch
    // What the raid's pilots made of our launches (roadmap B3): each is one pilot's first sight of one (the dust and flash,
    // or the smoke trail), where he believed it to be and how far off that may be, never the truth. A launch no pilot saw
    // (night, cloud, behind the beam, a ridge) leaves none, and `fired` does not hold the battery.
    struct LaunchSighting {
        double time_s = 0.0;
        double east_m = 0.0;     // where he believed the launch was (ENU, the scenario's)
        double north_m = 0.0;
        double sigma_m = 0.0;    // his error
        double confidence = 0.0;
        std::string by;          // the aircraft
    };
    std::vector<LaunchSighting> sightings;
    std::vector<std::string> recce_seen;                  // objects the reconnaissance photographed
    int missiles_spent = 0;                               // the player's side's missiles, all defences
    std::vector<std::string> missiles_downed;             // the enemy's cruise missiles shot down: by which defence, one each
    std::vector<std::pair<std::string, int>> missiles_by;  // (a played mission) rounds our other defences spent: the neighbours
    int arms_fired = 0;
    // The air battle (roadmap B8, air_fight.hpp): our fighters that met it, one entry per our fighter it shot down (the
    // regiment's id), and the fights it had with them.
    std::vector<std::string> our_fighters_lost;
    int air_fights = 0;
    // The strikers' answer in those fights (bookkeeping for the balance tool, roadmap B14; not saved): strike flights our
    // fighters reached, of them those that drew a jettison (whether they then aborted or not) and those that turned home.
    int fight_attacked_flights = 0;
    int fight_jettison_drawn = 0;
    int fight_aborted = 0;
    bool aborted = false;
    bool delivered = false;
    double collateral = 0.0;
    std::vector<std::string> notes;
    // Each flight that left a played session, where it was last seen (the theatre's frame) and when (roadmap S7: the
    // rest of the raid flies on from there under route.mode 2; empty before, and the rest flew the plan).
    struct FlownRecord {
        int flight = -1;
        Vec3 at;
        double t_h = 0.0;
    };
    std::vector<FlownRecord> flown;
    std::vector<int> released_flights;  // flights that released their weapons (a played mission)
    std::vector<int> aborted_flights;   // flights that broke away before their release point (a played mission)
    // Under fire (roadmap B4, reaction_chain.hpp): flights that threw their stores off before their release point, and
    // the drop tanks and loads that fell (a played mission).
    std::vector<int> jettisoned_flights;
    int tanks_dropped = 0;
    int stores_dropped = 0;
    std::vector<RetaskRecord> retasks;  // orders its flights were given from outside them (a played mission, roadmap B1)
    std::vector<HeardRecord> heard;     // what our intercept posts logged of its calls (a played mission, roadmap B9)
    // Command in the air (roadmap B5, c2.hpp): each message sent about it (its kind, the hour, the net, from and to; lost
    // to jamming; heard by our listeners) and each in-flight report its aircraft made (the place, its sigma, the hour it
    // was seen: every position has an age).
    struct CallRecord {
        std::string kind;
        std::string from;
        std::string to;
        std::string net;
        double t_h = 0.0;
        bool lost = false;
        bool heard = false;
    };
    std::vector<CallRecord> calls;
    struct ReportRecord {
        int flight = -1;
        std::string what;  // "launch", "column"
        double east_m = 0.0;
        double north_m = 0.0;
        double sigma_m = 0.0;
        double observed_h = 0.0;
    };
    std::vector<ReportRecord> reports;
    // Targets of opportunity (roadmap B6, opportunity.hpp): each thing a flight leader acted on or stopped at, as his
    // flight knew it (what he believed it was and where, what he did, where the chain stopped, his passes and his pilots'
    // claims), and beside it what was really there and what the passes did (the truth: the debrief's and the balance's,
    // never the enemy's records, which hold the claims).
    struct OpportunityRecord {
        int flight = -1;
        double t_h = 0.0;
        std::string believed;     // OppTargetName: what he believed it was
        double east_m = 0.0;      // where he believed it (the scenario's ENU, or the theatre's for the abstract level)
        double north_m = 0.0;
        std::string act;          // OppActName
        std::string stopped;      // OppStepName: where the chain stopped ("act": it acted)
        std::string why;
        bool reported_in_flight = false;
        int passes = 0;
        int claimed = 0;          // kills his pilots claim
        std::string object;       // the theatre object his report or claim is taken to be about ("" none: the debrief only)
        std::string truth;        // what was really there ("" nothing; a theatre or scenario id, "mover:<id>", "dummy:<n>")
        int kills = 0;            // what the passes really did
        bool misidentified = false;
    };
    std::vector<OpportunityRecord> opportunities;
    bool truth_applied = false;  // the damage is already in the truth (the abstract resolver); else the campaign applies it
    bool played = false;         // from a played mission (the mission bridge)
    // What the battery reported of it from its records (roadmap 18.1, campaign/report.hpp): our side's journal states
    // these for a played mission, never the truth. Not `reported`: no records kept (the Osa's crew writes no journal),
    // and our side's journal takes the truth.
    bool reported = false;
    int reported_destroyed = 0;  // kills its launchers reported and did not take back
    int reported_missiles = 0;   // launches reported
    int reported_targets = 0;    // the hostile and unidentified targets the post wrote down, by number

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
    // Combat sorties its pilots flew and came back from (roadmap S5, 10_FACTIONS.md §8.3: a pilot's experience is his
    // sorties survived; hot_phase.hpp HotPilotOf reads them per pilot). Rebuilt by the replay; read only under
    // hot_phase.mode 2.
    int combat_sorties = 0;

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
    // Logistics (docs/air_war/14_LOGISTICS.md, logistics.hpp). Troops: their stocks (1 full). A supply line's column:
    // where it is on its round (0 loading at the depot, 1 on its way, 2 unloading, 3 coming back, -1 destroyed), how
    // far along the line, and how long it has waited.
    double supply = 1.0;
    int leg = 0;
    double along_m = 0.0;
    double wait_h = 0.0;
};

// How the player's battery behaves between and during fights: the commander's choices the enemy reacts to.
// What the enemy's support squadrons keep up (docs/air_war/11_SUPPORT.md).
struct SupportLevel {
    double awacs = 0.0;             // share of the day an AWACS is on station
    double elint = 0.0;             // share of the day a stand-off ELINT aircraft is
    double tanker_receivers = 0.0;  // aircraft the tankers can refuel a day
    bool has_elint_aircraft = false;
    bool has_tankers = false;
    int sorties = 0;                // support sorties a day
};

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
