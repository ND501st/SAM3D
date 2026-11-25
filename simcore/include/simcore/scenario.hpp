// simcore/scenario.hpp — scenario files: where, who, what flies, and what happens when.
//
// A scenario (data/scenarios/<id>.json, format in data/scenarios/README.md) names the
// console file of the player's system, places the unit, lists its crew, the air
// targets with their routes, and triggers. A trigger fires once when all of its
// "when" conditions hold (time, a target's range, console conditions, number of
// launches) and then sets sim inputs, gives crew orders, or ends the scenario.
//
// LoadScenarioFile also loads the console the scenario names, so every reference
// (inputs, procedures, subsystems, controls) is checked when the scenario loads.
#pragma once

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_levels.hpp"
#include "simcore/air_radio_def.hpp"
#include "simcore/air_war/air_combat_data.hpp"
#include "simcore/air_war/air_fight_def.hpp"
#include "simcore/air_war/perception.hpp"
#include "simcore/air_war/test_waves_data.hpp"
#include "simcore/battery.hpp"
#include "simcore/c2_sources.hpp"
#include "simcore/campaign/battery_plan.hpp"
#include "simcore/campaign/battery_task.hpp"
#include "simcore/campaign/report.hpp"
#include "simcore/campaign/stocks.hpp"
#include "simcore/command/battery_orders.hpp"
#include "simcore/command/own_flights.hpp"

#include "simcore/command_post.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/crew_mobility.hpp"
#include "simcore/crew_training.hpp"
#include "simcore/em_link.hpp"
#include "simcore/em/optical.hpp"
#include "simcore/reactions.hpp"
#include "simcore/emission_regime.hpp"
#include "simcore/engagement.hpp"
#include "simcore/flights.hpp"
#include "simcore/instructor.hpp"
#include "simcore/light.hpp"
#include "simcore/nav/session.hpp"
#include "simcore/position_prep.hpp"
#include "simcore/pu12.hpp"
#include "simcore/radio_nets.hpp"
#include "simcore/radio_sets.hpp"
#include "simcore/session_deliveries.hpp"
#include "simcore/talk_tables.hpp"
#include "simcore/target_behaviour.hpp"
#include "simcore/terrain_map.hpp"
#include "simcore/weather_data.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct ScenarioCrew {
    std::string name;
    std::string role;
    double reaction_time_s = 1.0;
    double reaction_jitter_s = 0.0;  // each run draws reaction_time_s ± this from the scenario seed
    std::string skill;               // a tier of data/crew/skill_levels.json (docs/crew/05_SKILL.md); empty: none
};

// A target's anti-radiation missiles (roadmap E6): "arms": {"weapon", "count", "launch_range_km", "min_range_km",
// "interval_s", "aim", "turn_away"}. It fires only on what its own warning receiver hears: a hostile radar in the
// weapon's band (search, track or fire control) heard in the last second, with the place it believes the radar is
// (its intelligence: "aim", by default the unit's position, a pre-briefed launch) within the launch range. Each ARM is
// set to the signal heard. After its last one it turns away from the radar (the ARM sign: voenizdat_osa_1991 p262).
struct ArmCarrierDef {
    std::string weapon;           // the EM library's ARM id
    int count = 0;
    double launch_range_m = 0.0;  // 0: the weapon's max_range_m
    double min_range_m = -1.0;    // < 0: the weapon's
    double interval_s = 4.0;      // between two of its ARMs
    bool has_aim = false;
    Vec3 aim;                     // ENU; without one, the unit's position
    bool turn_away = true;
};

struct ScenarioTarget {
    std::string id;
    std::string name;
    std::string type = "Air+FixedWing";  // Tacview type tags
    Vec3 start;                          // ENU [m] at the scenario origin
    std::vector<Vec3> waypoints;         // ENU [m]
    double speed_mps = 200.0;
    double max_turn_rate_rad_s = DegToRad(6.0);
    // Its type's flight limits (roadmap R3, aircraft.hpp): "max_load_g", "sustained_turn_deg_s", "roll_rate_deg_s",
    // "max_mps", "accel_mps2", "climb_mps" (the air war's types give them). 0: not given; with none it flies the old
    // way (constant speed, its velocity turned at max_turn_rate_rad_s), with any it flies by bank-to-turn.
    double max_load_g = 0.0;
    double sustained_turn_rate_rad_s = 0.0;
    double roll_rate_rad_s = 0.0;
    double max_speed_mps = 0.0;
    double accel_mps2 = 0.0;
    double max_climb_mps = 0.0;
    double rcs_m2 = 5.0;
    double appear_at_s = 0.0;
    bool on_cue = false;                   // held until something releases it (a lesson: ScenarioRunner::ReleaseTarget)
    std::string target_class = "default";  // picks the hit chance in the gameplay tuning
    bool evade_on_launch = false;          // breaks away onto a beam heading when a missile is launched at it
    double evade_turn_rate_rad_s = DegToRad(12.0);
    bool friendly = false;                 // "iff": "friend": replies to IFF interrogation
    int iff_code = 0;                      // "iff_code": its transponder's code, fixed (0 the default every interrogator asks
                                           // on; -1 off: ours, but it gives no reply) (roadmap B10)
    bool iff_table = false;                // "iff_table": our pilot sets his code by the day's table ("own_air"), and may miss a change

    int board_type = -1;                   // its type on the PU-12 board (0-9; -1: 1 if a friend, else 9, no IFF)
    int formation = 1;                     // aircraft in its formation, as the board notes it
    double span_m = 0.0;                   // its span as the TV sight sees it (0: the gameplay tuning's for its class)
    TargetBehaviourDef behaviour;          // terrain following, pop-up (target_behaviour.hpp)
    EvadeDef evade;                        // evasion on launch or lock (takes the place of evade_on_launch)
    // Its pilot's reactions under fire (roadmap B4, reactions.hpp; "reactions"): his faction's profile, his type's
    // countermeasures and tanks, his load, the programme's zones. Takes the place of "evade" (one or the other).
    ReactionPilotDef reactions;
    double size_m = 4.5;                   // critical dimension for the TV sight (em/optical.hpp; estimate for a fighter)
    // Its radar warning receiver: an ESM receiver of the EM library (kind rwr). With one, the pilot evades when it warns
    // of the lock or the launch (the command link heard: roadmap E5), not on the truth.
    std::string rwr;
    // Its crew (roadmap B3, docs/air_battle/03_PERCEPTION.md 2.2): a second seat adds his glimpses to the lookout. And the
    // pilot's skill at seeing, 0 untrained to 1 trained (< 0: perception.json's default).
    int seats = 1;
    double pilot_skill = -1.0;
    // The model it is drawn with (data/scenery/basic_models.json; "" its class's): presentation only.
    std::string model;
    // A decoy (roadmap E6): a drone whose echo is made to look like an aircraft's (radar reflectors). The radar can't
    // tell; the TV can, close enough to make out its type (em/optical.hpp), when the crew looks.
    bool decoy = false;
    // Anti-radiation missiles it carries (roadmap E6, em/arm.hpp): "arms". Count 0: none.
    ArmCarrierDef arms;
    // A weapon in flight to the ground ("weapon": true): a bomb, an air-to-ground missile, a cruise missile. It ends at
    // its impact (the ground, or the end of its route), not by flying on (the testbed's waves, air_war/test_waves.hpp).
    bool weapon = false;
    // How far its lowest point is below its centre ("bottom_m"; < 0: by its kind, BottomM): the pilot keeps that much
    // more than a metre over the ground (target_behaviour.hpp, roadmap R5).
    double bottom_m = -1.0;
    // Game values: a helicopter's skids or wheels 2.5 m under its centre, an aircraft's belly 1.5 m, a weapon's 0.3 m.
    double BottomM() const {
        if (bottom_m >= 0.0) return bottom_m;
        if (weapon) return 0.3;
        return type.find("Rotorcraft") != std::string::npos ? 2.5 : 1.5;
    }
    // Its flight (roadmap B1, flights.hpp): one of the scenario's "flights" by id, and its number in it ("slot": 1 the
    // leader, 3 the element lead; 0: by its order among the flight's targets). No flight: a flight of one.
    std::string flight;
    int slot = 0;
    // Its route's legs ("legs", one per waypoint: the leg flown to it): the speed on it ("speed_mps"; 0: speed_mps), its
    // height ("agl_m" above the ground, held by the behaviour; or "alt_m" above sea level, the waypoint's height) and its
    // "tag" (the phase: transit, ingress, attack, egress, defensive, orbit; or the planner's point tag). Empty: speed_mps
    // the whole way. `to` is the waypoint.
    std::vector<RouteLeg> legs;
};

// A flight of the scenario's targets ("flights", roadmap B1): who leads is its targets' slot 1.
struct ScenarioFlight {
    std::string id;
    std::string callsign;
    std::string package;          // a "packages" entry's id ("" none)
    std::string side = "enemy";   // "enemy" or "ours"
    bool keep_slots = true;       // false: each flies its own route (a stream of cruise missiles)
    bool crewed = true;           // false: drones or weapons: no nerve, never turned back, no loss to its package
    double call_s = flights::kCallS;  // a call on its net reaching a member
    // The formation by phase ("formations": {"transit": [[right_m, back_m, up_m], ...], ...}): the leader's place first.
    // None: each keeps where it started from the leader.
    std::map<std::string, std::vector<Vec3>> formations;
    FlightRadioInfo radio;        // "radio": its unit, role and discipline for the enemy's radio (roadmap B9)
};
// A package ("packages"): its commander's flight and his nerve, the share of its aircraft lost (of those he knows of)
// at which he turns it back (the air war's PackageNerve; default 0.25 + 0.3 x 0.5).
struct ScenarioPackage {
    std::string id;
    std::string commander;   // a flight id ("" the first of its flights)
    int size = 0;            // its aircraft (0: those of its flights in the scenario)
    int lost_before = 0;     // lost before it came into the scenario, known to him
    double turn_back_share = 0.4;
};

// How the crew fights, as its orders from above put it (roadmap E6): "doctrine".
struct CrewDoctrineDef {
    // Warned of decoys: the commander has the deputy look at what the radar holds on the TV (tv_watch) and launches
    // only at what he has made out; at night or in a haze where the TV sees nothing he goes by the radar.
    bool tv_check = false;
    // The emission regime the crew goes into the mission under (emission_regime.hpp): the battery commander's decision,
    // or the regiment's order he passed on, given in the combat order before the vehicles deploy; the vehicle commander
    // tells his crew before the radars come on. "emission": {"regime": "free" | "limited" | "arm" | "silent", "on_turns",
    // "off_turns", "from": "battery" | "regiment"}. The older forms still load: "search_bursts" {on_s, off_s} is the
    // limited regime in seconds (against direction finding: on for on_s, then off for off_s, while no hostile is within
    // the tracking radar's reach); "arm_threat" is the least emission against ARMs.
    EmissionRegime emission;
    // Missiles per target at once: 2 is the two-missile salvo, the second launched as soon as the launcher allows
    // (3-5 s), before the first's result (korovin_tiv_2010; flak11: two missiles selected at ВНИМАНИЕ). 1: one, and
    // the second only after a miss.
    int salvo = 1;
    // Under the threat of anti-radiation missiles ("arm_threat", voenizdat_osa_1991 pp262-264): the least emission. The
    // search transmitter's high voltage on for 2 antenna turns after every 3-4 («через 3—4 оборота антенны на 2
    // оборота»); the tracking transmitter's only from the range of Table 7.3 for the target's speed; the search
    // transmitter off while the tracking radar holds a stable track and nothing else is about; the tracking
    // transmitter off after the missile's burst. The commander's reaction to the signs of an ARM launch is part of
    // every crew's drill, threat or not: «Излучение выключить!» once the missile in the air has burst, for
    // arm_silence_s after the last sign (a SAM3D estimate). The least emission is the regime "arm"; "arm_threat": true
    // with "arm_search_off_turns" (the textbook's 3-4) is its older form.
    double arm_silence_s = 60.0;
    // A standing order from the regiment ("relocate_after_arm": {"to": position}): attacked by ARMs, the battery moves to
    // its reserve position when the fight is over (only where the battery drives this vehicle, battery_drive_own). The
    // battery commander changes position only on the regiment commander's order (voenizdat_osa_1991 p238).
    bool relocate_after_arm = false;
    Vec3 relocate_to;
};

struct TriggerAction {
    enum class Kind { SetInput, Order, Queue, End, C2Source, C2Fail, C2Restore, BatteryOrder, AirThreat, Instructor, Breakdown };
    Kind kind = Kind::SetInput;
    std::string input;      // SetInput
    bool value = true;      // SetInput
    std::string crew;       // Order, Queue
    std::string procedure;  // Order, Queue
    std::string reason;     // End, C2Fail, Breakdown
    std::string vehicle;    // Breakdown: a battery vehicle that breaks down where it stands
    std::string c2_id;      // C2Source (a source id or "none"), C2Fail, C2Restore
    simcore::BatteryOrder battery_order;  // BatteryOrder
    bool air_threat = false;              // AirThreat
    InstructorOrder instructor;           // Instructor: the trainer's scripted drill (roadmap 10.3)
};

struct ScenarioTrigger {
    std::string id;
    double after_s = 0.0;                 // scenario time at or after
    std::string target;                   // with target_within_m: that target is spawned, alive and this close
    double target_within_m = -1.0;        // < 0: not used
    std::vector<Condition> conditions;    // console conditions (controls, subsystems, inputs)
    int launches_at_least = 0;
    std::vector<TriggerAction> actions;
};

// A prepared position's power (roadmap 7.6, own_unit.position_power): the civil grid feeds the vehicle's external
// supply; when it is cut a diesel set takes over after `diesel_start_s` and runs for `diesel_hours` (no diesel: 0).
// The scenario sets the console input "external_power" from this every step.
struct PositionPowerDef {
    bool present = false;
    bool grid = true;
    double grid_cut_s = -1.0;     // < 0: the grid is never cut
    double diesel_start_s = 0.0;
    double diesel_hours = 0.0;
};

// One of our other air defences near the scenario (a raid's neighbours: air_war/raid_scenario.hpp). Drawn where it
// stands (the model library's site layout for its system) and plotted on the command post's board, as the regiment's
// order gives them. Its fire at a raid's aircraft is the mission bridge's abstract chance (air_war/mission_bridge.hpp),
// except an Osa battery that `fights`: its launchers are shooters of the runner's battery fire model
// (ScenarioRunner::NeighbourLaunchers), firing on their own.
struct ScenarioNeighbour {
    std::string id;
    std::string name;
    std::string system;     // an air defence system id (air_war/data.hpp AirDefenceSystems)
    Vec3 position;          // ENU
    double radius_m = 0.0;  // its reach
    bool fights = false;    // an Osa battery fought in the battery fire model
    int launchers = 4;      // its launchers that work (fights)
    int missiles = -1;      // its missiles ready, all launchers together (-1: six each)
};

// A model standing in the scenario ("scenery": a radar, a vehicle, a building of data/scenery/basic_models.json):
// presentation only, drawn where it stands and visited by the views (the dynamic campaign testbed's radars).
struct ScenarioScenery {
    std::string id;
    std::string name;
    std::string model;
    Vec3 position;              // ENU
    double heading_deg = 0.0;   // true, clockwise from north
    bool friendly = true;       // ours (the default) or the enemy's
};

struct ScenarioDef {
    std::string id;
    std::string name;
    std::string description;
    // "exercise" (a range day, a drill, a lesson: the default) or "war" (a battle, the air war's raids): a war sets the
    // crew's stress a step higher from the start (docs/crew/15_TALK.md section 4).
    std::string setting = "exercise";
    unsigned seed = 1;
    GeoPoint origin;
    double ground_height_m = 0.0;  // flat terrain (MSL); terrain grids come with Cesium
    std::vector<TerrainRidge> ridges;     // straight ridges on it, for cover (terrain.ridges)
    // "terrain": {"type": "map", "map": "maps/kesmark_range.json"}: the ground of a generated map (terrain_map.hpp)
    // under the scenario, whatever its origin. ground_height_m is then the map's ground under the origin.
    std::string map_file;
    std::shared_ptr<const MapData> map;
    // "weather": a preset of data/weather/presets.json (by id, or the air war's clear/cloudy/poor): the winds, the
    // cloud and the air (weather.hpp). Without one, still standard air and no cloud.
    bool has_weather = false;
    WeatherPreset weather;
    std::string console_file;      // relative to the data folder, e.g. "procedures/osa_ak.json"
    std::string crew_file;         // the crew data (own_unit.crew_data, or data/crew/<system>_crew.json when it exists)
    bool has_crew_data = false;
    CrewData crew_data;
    // The vehicle as a body to climb (data/vehicles/<system>.json, crew_mobility.hpp) and the crew's pace and voice
    // (data/crew/walking.json), loaded when the system has a vehicle file: its crew then gets in and out by its hatches.
    bool has_vehicle = false;
    VehicleDef vehicle;
    WalkingDef walking;
    bool has_skills = false;       // a crew member has a "skill": the tiers are loaded
    SkillTable skills;
    std::string tuning_file;       // gameplay tuning, e.g. "gameplay/osa_ak.json"; empty = no engagement model
    EngagementTuning tuning;
    double fuel_fraction = 1.0;    // the tank's starting level, 0 to 1 (own_unit.fuel_fraction)
    bool radio_presets_set = true;  // own_unit.radio_presets: "set" (the frequency plan is on the fixators) or "unset"
    PositionPowerDef position_power;
    PreparedPositionDef prepared;  // own_unit.prepared_position (roadmap 7.6)
    std::string readiness;         // the state the unit starts in: "" or "cold", or a console readiness state
    bool has_em = false;           // the unit's radars come from the electromagnetic layer (own_unit.em)
    EmLinkDef em;
    em::OpticalConditions optics;  // "environment": the visibility and the daylight the TV sight works in
    CrewDoctrineDef doctrine;      // "doctrine": the TV check against decoys, the emission regime, the salvo
    // The emission regimes' words and the chain's times (data/command/emission_regimes.json; the code's defaults if the
    // data folder has none).
    EmissionDef emission;
    // Orders of the emission regime during the mission ("emission_orders": [{"at_s", "regime", "on_turns", "off_turns",
    // "from"}]): from "regiment", said on the regiment's net and passed on by the battery commander (in an Osa's scenario
    // the battery commander is the battery's own NPC; in the post's, the seat); from "battery", the battery commander's
    // own decision at that time, said on the battery net.
    struct EmissionOrderDef {
        double at_s = 0.0;
        EmissionRegime regime;
    };
    std::vector<EmissionOrderDef> emission_orders;
    Vec3 unit_position;            // ENU [m]
    double unit_heading_deg = 0.0; // the hull's true heading, clockwise from north
    // "empty_containers": the launcher's containers that start without a missile (e.g. ["1-I", "1-II"]): a mission that
    // starts part-spent, so that the TZM comes within it (roadmap 8.11).
    std::vector<std::string> empty_containers;
    std::vector<std::pair<std::string, bool>> initial_inputs;
    std::vector<std::pair<std::string, int>> initial_controls;  // set after the readiness (own_unit.controls)
    std::vector<ScenarioCrew> crew;
    std::vector<ScenarioTarget> targets;
    std::vector<ScenarioFlight> flights;    // "flights": the targets' flights (a target with none is a flight of one)
    std::vector<ScenarioPackage> packages;  // "packages": the flights' packages
    AirRadioDef air_radio;                  // "air_radio": the enemy's flights' calls as transmissions, our intercept posts (B9)
    airwar::AirFightDef air_fight;          // "air_fight": the air battle's fighters and controllers (B8, air_fight_session.hpp)
    command::OwnAirScenarioDef own_air;     // "own_air": our aviation's codes, notices, plan of cooperation and duty (B10)

    std::vector<ScenarioNeighbour> neighbours;
    std::vector<ScenarioScenery> scenery;
    // "waves": "gameplay/test_waves.json": random waves of every adversary's and our own aircraft for as long as the
    // session runs, the strike aircraft dropping their loads (the dynamic campaign testbed, air_war/test_waves.hpp).
    std::string waves_file;
    std::shared_ptr<const airwar::WavesDef> waves;
    std::vector<ScenarioTrigger> triggers;
    std::vector<DeliveryDef> deliveries;  // the chain of command's messages played in the session (roadmap P1)
    std::string command_post_file;  // the battery command post's plotting board, e.g. "command/pu12.json"; "" = none
    PlottingBoardDef board;
    // The player's vehicle is the command post (the console has "post"): its own board, its ASPD-U, the regiment that
    // sends it the telecode, and a radar cabled to it (simcore/command_post.hpp, docs/PU12_DEVELOPMENT.md P3).
    bool has_post = false;
    PlottingBoardDef post_board;
    AspdDef post_aspd;
    PostRegiment post_regiment;
    RegimentRadarLibrary post_radar_types;  // the post's library of radar types ("post.library"): a radar joining in play
    AspdCodeTable post_codes;        // the regiment's two-digit commands' meanings ("post.codes"; empty: none)
    PlottingTuning post_plotting;    // the player's hand on the board ("post.plotting"; the defaults without it)
    // The after-action report from the post's records and the regiment's check of them (roadmap P9,
    // data/gameplay/after_action.json, read with any post).
    campaign::AfterActionValues after_action;
    bool has_post_radar = false;
    RegimentRadar post_radar;
    // The cabled radar in the electromagnetic layer ("post.em"): its echoes, IFF replies and jamming strobes from the EM
    // world instead of the coverage model (a search radar alone, standing at post_radar.position).
    bool has_post_em = false;
    EmLinkDef post_em;
    // The post's R-111 and R-407 (the console's "radios"): their numbers, and the scenario's frequency plan for them.
    bool has_radio_sets = false;
    RadioSetsDef radio_sets;
    RadioPlan radio_plan;
    // The battery's radio data: its nets, frequencies by number, call signs, scheduled changes, jamming, lines; the talk
    // tables and the communications' game values ("radio_data", docs/crew/12_COMMUNICATIONS.md).
    RadioDataDef radio_data;
    TalkTables talk_tables;
    CommsTuning comms_tuning;
    // The battery's early-warning and command sources (roadmap E4): the PU-12 from "command_post" first, then the
    // scenario's "c2" block. Empty: no command network.
    C2Policy c2_policy;
    std::vector<C2SourceDef> c2_sources;
    // The battery around the player's vehicle (roadmap 7.5): "battery" block. The own vehicle is battery_vehicles[0].
    bool has_battery = false;
    BatteryTactics battery_tactics;
    std::vector<BatteryVehicleDef> battery_vehicles;
    std::vector<BatteryOrder> battery_orders;  // scheduled by at_s
    std::shared_ptr<const command::ChainData> battery_chain;  // by_means's chain (the staff list and the diagram: G8, G9.4)
    // The route service in the session (roadmap G2.3, G11; nav/session.hpp): "nav": {"country": ..., "chain": ...,
    // "movers": [...]}. A march may then name where it goes ("to_place"), and the world's movers go on the roads.
    std::shared_ptr<const nav::SessionNav> nav;
    // The battery's missiles from the rear in the session (roadmap 17.4; campaign/stocks.hpp): "supply": {"logistics": ...,
    // "siding": <a place on the roads>, "tested": n, "untested": n}. The TZM's run to the regiment's technical battery is
    // then a battery order on the net and a mover on the session's roads.
    bool has_supply = false;
    campaign::LogisticsValues logistics;
    std::string supply_siding = "tech_battery";
    int siding_tested = 0;    // at the technical battery at the start (the day's situation)
    int siding_untested = 0;
    // The regiment's task to the battery (roadmap 14.1, 14.3; campaign/battery_task.hpp, battery_plan.hpp): "task":
    // {"file": ..., "given_s": ..., "means": "radio" | "in_person"}. The battery commander plans it by the textbook's
    // sequence (data/campaign/battery_plan.json) and the battery's orders follow from his plan.
    bool has_task = false;
    campaign::BatteryTask task;
    double task_given_s = 0.0;
    std::string task_means = "radio";
    // "held": true: the battery holds the task from before the session, in its area and reported ready (a campaign's
    // session resumed from a save: track O, O1). Nobody gives it again: the battery commander has its paper, the
    // launchers their sectors, and the regime is said once on the battery net as the watch begins.
    bool task_held = false;
    campaign::PlanSequence plan_sequence;
    // "clock_start": "19:00": the time of day at the session's second 0, for the order's paper, the order books and the
    // debrief (< 0: none, the session's own clock).
    double clock_start_s = -1.0;
    // The sky clock (roadmap R7, light.hpp): the origin's place, the "date" (15 October without one), "utc_offset_h" (the
    // longitude's hour without one) and the local hour of second 0: "clock_start" when given, else 23:00 for a scenario
    // whose legacy keys say night ("environment.daylight": false or "battery.night": true), else 10:30. The runner's
    // light is the sun of this clock; the legacy keys only choose the start hour of a scenario without a clock.
    SkyClock clock;
    bool clock_given = false;  // "clock_start" was in the file
    int year = 1985;           // the "date"'s year (the default's)
    LightValues light_values;  // data/gameplay/light.json
    airwar::PerceptionData perception;  // data/gameplay/perception.json and the receivers' displays (roadmap B3)
    AirLevelValues air_levels;          // data/gameplay/air_levels.json: the flights' levels of detail (roadmap B12)
    // The reactions under fire (roadmap B4): data/gameplay/reactions.json and data/air_war/countermeasures.json, loaded
    // whenever they are in the data folder (a package entering the session may bring pilots who react).
    ReactionTable reactions;
    // Damage states for every aircraft of the session ("damage": the damage table's path, data/gameplay/damage.json;
    // roadmap R1 in played raids, B4): a hit, a burst or a crash by damage.hpp, not only in the testbed's waves.
    bool has_damage = false;
    DamageTable damage;
    bool battery_drive_own = false;
    bool battery_night = false;
    bool battery_deployed = false;  // "deployed": every vehicle starts set up where it stands
    // Battery orders carried by a means (roadmap G10.5; "battery.by_means"): a moving order goes out on the battery net
    // (flags or lamps under the march's radio ban) and the battery's other vehicles act on it only once their commanders
    // have heard it (command/battery_orders.hpp). Off: every vehicle takes it at once.
    bool battery_by_means = false;
    command::BatteryOrdersDef battery_orders_def;
    std::vector<command::OrderVehicle> battery_crews;  // every vehicle but the own and the giver's: its commander
    std::string battery_giver;                         // the vehicle the battery commander rides in (the command post)
    std::string battery_giver_post = "battery_commander";
    // The battery's launchers when the player's vehicle is not one of them (the PU-12M): their gameplay tuning and their
    // tracking radar's reach, from "battery.launchers": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json" }.
    bool has_launcher_tuning = false;
    EngagementTuning launcher_tuning;
    double launcher_reach_m = 0.0;
    double time_limit_s = 600.0;
    bool end_when_targets_gone = true;  // end once every target is destroyed or past its last waypoint
    // "end": {"on_unit_hit": false}: the vehicle put out of action (an ARM's burst) is logged and the session runs on (a
    // testbed that must not stop, docs/DYNAMIC_CAMPAIGN_TEST.md).
    bool end_on_unit_hit = true;
    // "crew_commander": "npc": the vehicle commander is an NPC who runs the crew by doctrine (roadmap 8.6,
    // docs/crew/07_COMMANDER_MODE.md §4) instead of the scenario's triggers giving every order. "script" (default): the
    // triggers give the orders.
    bool npc_commander = false;
    // "battery_commander": "npc" (default) distributes the PU-12's targets to the battery's other launchers; "player": the
    // player does (ScenarioRunner::AssignBatteryTarget); "none": they fire freely at what is in their reach; "post": the
    // battery commander sits in a linked PU-12M (battery_link.hpp), whose launchers the other vehicles are and whose
    // designations reach this vehicle as the PU-12's.
    std::string battery_commander = "npc";
    // "assistance": the training overlay's level for the player's seat (docs/crew/08_TRAINING_AND_ASSISTANCE.md §2):
    // "demonstrate", "guided", "hint", "unassisted" or "exam". Default: guided.
    AssistLevel assistance = AssistLevel::Guided;
};

namespace detail {

class ScenarioLoader : public ConsoleLoader {
public:
    bool LoadPositionPower(const json::Value& unit, const ConsoleData& console, PositionPowerDef& out) {
        const json::Value* v = unit.Find("position_power");
        if (v == nullptr) return true;
        const std::string p = "own_unit.position_power";
        if (!v->Is(json::Type::Object)) return Fail(p, "expected an object");
        if (!Keys(*v, p, {"grid", "grid_cut_s", "diesel_start_s", "diesel_hours", "source", "confidence", "notes"}) ||
            !Provenance(*v, p) || !OptionalBool(*v, p, "grid", out.grid) || !OptionalNumber(*v, p, "grid_cut_s", out.grid_cut_s) ||
            !OptionalNumber(*v, p, "diesel_start_s", out.diesel_start_s) || !OptionalNumber(*v, p, "diesel_hours", out.diesel_hours)) {
            return false;
        }
        if (out.diesel_start_s < 0.0) return Fail(Join(p, "diesel_start_s"), "must be >= 0");
        if (out.diesel_hours < 0.0) return Fail(Join(p, "diesel_hours"), "must be >= 0");
        if (!console.equipment.HasInput("external_power")) return Fail(p, "the console has no external_power input");
        out.present = true;
        return true;
    }

    bool Load(const json::Value& root, const std::string& data_dir, ScenarioDef& out, ConsoleData& console) {
        if (!Keys(root, "", {"scenario_id", "name", "description", "source", "confidence", "notes", "seed", "origin",
                             "terrain", "command_post", "c2", "post", "battery", "own_unit", "crew", "crew_commander", "battery_commander",
                             "targets", "neighbours", "triggers", "end", "assistance", "environment", "doctrine", "weather", "radio_data",
                             "setting", "emission_orders", "deliveries", "nav", "task", "clock_start", "date", "utc_offset_h", "supply",
                             "scenery", "waves", "flights", "packages", "air_radio", "damage", "air_fight", "own_air"})) {

            return false;
        }
        if (!RequireString(root, "", "scenario_id", out.id) || !RequireString(root, "", "name", out.name) ||
            !OptionalString(root, "", "description", out.description) || !Provenance(root, "")) {
            return false;
        }
        if (!OptionalString(root, "", "setting", out.setting)) return false;
        if (out.setting != "exercise" && out.setting != "war") return Fail("setting", "must be \"exercise\" or \"war\"");
        int seed = 1;
        if (!OptionalInt(root, "", "seed", seed)) return false;
        if (seed < 0) return Fail("seed", "must be >= 0");
        out.seed = static_cast<unsigned>(seed);
        if (!LoadOrigin(root, out) || !LoadTerrain(root, data_dir, out)) return false;
        if (!LoadEmissionData(data_dir, out)) return false;
        if (!LoadOwnUnit(root, data_dir, out, console)) return false;
        if (!OptionalString(root, "", "command_post", out.command_post_file)) return false;
        if (!out.command_post_file.empty()) {
            std::string error;
            if (!LoadPlottingBoardFile(data_dir + "/" + out.command_post_file, out.board, &error)) {
                return Fail("command_post", error);
            }
        }
        if (!LoadC2(root, data_dir, out)) return false;
        if (!LoadPost(root, data_dir, out, console)) return false;
        if (!LoadRadioData(root, data_dir, out, console)) return false;
        if (!CheckRegimentGroups(out)) return false;
        if (!LoadNav(root, data_dir, out)) return false;
        if (!LoadBattery(root, data_dir, out)) return false;
        if (!LoadSupply(root, data_dir, out)) return false;
        if (!LoadCrew(root, data_dir, out, console)) return false;
        if (const json::Value* cc = root.Find("crew_commander")) {
            if (!cc->Is(json::Type::String) || (cc->string != "npc" && cc->string != "script")) {
                return Fail("crew_commander", "must be \"npc\" or \"script\"");
            }
            out.npc_commander = cc->string == "npc";
        }
        if (const json::Value* bc = root.Find("battery_commander")) {
            if (!bc->Is(json::Type::String) || (bc->string != "npc" && bc->string != "player" && bc->string != "none" &&
                                                 bc->string != "post")) {
                return Fail("battery_commander", "must be \"npc\", \"player\", \"none\" or \"post\"");
            }
            out.battery_commander = bc->string;
        }
        if (const json::Value* as = root.Find("assistance")) {
            if (!as->Is(json::Type::String) || !ParseAssistLevel(as->string, out.assistance)) {
                return Fail("assistance", "must be \"demonstrate\", \"guided\", \"hint\", \"unassisted\" or \"exam\"");
            }
        }
        if (!LoadEnvironment(root, out) || !LoadWeather(root, data_dir, out) || !LoadDoctrine(root, out)) return false;
        if (!LoadEmissionOrders(root, out)) return false;
        if (!LoadClockStart(root, out) || !LoadTask(root, data_dir, out)) return false;
        if (!LoadFlights(root, out) || !LoadTargets(root, out) || !CheckFlights(out) || !LoadAirRadio(root, data_dir, out)) return false;
        if (!LoadAirFight(root, data_dir, out) || !LoadOwnAir(root, data_dir, out)) return false;

        if (!LoadScenery(root, out) || !LoadWaves(root, data_dir, out)) return false;
        if (!LoadTriggers(root, out, console)) return false;
        if (!LoadDeliveries(root, out, console)) return false;
        if (!LoadEnd(root, out)) return false;
        if (!LoadReactionsAndDamage(root, data_dir, out)) return false;
        return LoadSky(root, data_dir, out);
    }

private:
    // The regiment's commands said as talk-table groups go on its command net: the scenario's radio data must have that net
    // and the group must be in its talk tables. Radio silence and the spare frequency are ordered by the radio data
    // (silence_orders, the net's changes), and the warnings are the battery's own signals, so those groups are refused here.
    bool CheckRegimentGroups(const ScenarioDef& out) {
        for (std::size_t j = 0; j < out.post_regiment.commands.size(); ++j) {
            const RegimentCommand& c = out.post_regiment.commands[j];
            if (c.group.empty()) continue;
            const std::string cp = Index("post.regiment.commands", j);
            if (out.radio_data.Find("regiment") == nullptr) return Fail(cp, "a group goes on the regiment's net: radio_data has no net \"regiment\"");
            const TalkEntry* e = out.talk_tables.Decode(c.group);
            if (e == nullptr) return Fail(Join(cp, "group"), "group " + c.group + " is not in the radio data's talk tables");
            for (const char* other : {"silence_on", "silence_off", "spare_frequency", "air_warning", "all_clear"}) {
                if (e->effect == other) return Fail(Join(cp, "group"), "group " + c.group + " (" + e->effect + ") is ordered by the radio data, not as a command");
            }
        }
        return true;
    }

    // "radio_data": the battery's nets and lines (radio_nets.hpp). A net's set must be one of the console's voice sets,
    // its frequencies inside that set's band.
    bool LoadRadioData(const json::Value& root, const std::string& data_dir, ScenarioDef& out, const ConsoleData& console) {
        const json::Value* r = root.Find("radio_data");
        if (r == nullptr) return true;
        const std::string p = "radio_data";
        if (!Keys(*r, p, {"talk_tables", "comms", "silence", "silence_orders", "jamming_follows_df", "df_jam_s", "nets", "jamming", "lines",
                          "source", "confidence", "notes"}) ||
            !Provenance(*r, p)) {
            return false;
        }
        RadioDataDef& d = out.radio_data;
        std::string tables_file, comms_file;
        if (!OptionalString(*r, p, "talk_tables", tables_file) || !OptionalString(*r, p, "comms", comms_file) ||
            !OptionalBool(*r, p, "silence", d.silence) || !OptionalBool(*r, p, "jamming_follows_df", d.jamming_follows_df) ||
            !OptionalNumber(*r, p, "df_jam_s", d.df_jam_s)) {
            return false;
        }
        std::string error;
        if (!tables_file.empty() && !LoadTalkTablesFile(data_dir + "/" + tables_file, out.talk_tables, &error)) return Fail(Join(p, "talk_tables"), error);
        if (!comms_file.empty() && !LoadCommsTuningFile(data_dir + "/" + comms_file, out.comms_tuning, &error)) return Fail(Join(p, "comms"), error);
        const json::Value* nets = OptionalArray(*r, p, "nets");
        if (nets == nullptr) return message_.empty() ? Fail(Join(p, "nets"), "required, a list") : false;
        for (std::size_t i = 0; i < nets->items.size(); ++i) {
            const std::string np = Index(Join(p, "nets"), i);
            const json::Value& e = nets->items[i];
            RadioNetDef n;
            if (!Keys(e, np, {"id", "name_ru", "name_en", "set", "main", "own_call_sign", "main_call_sign", "frequencies_mhz", "changes",
                              "orders", "exempt", "notes"}) ||
                !RequireString(e, np, "id", n.id) || !RequireString(e, np, "name_ru", n.name_ru) || !RequireString(e, np, "name_en", n.name_en) ||
                !OptionalString(e, np, "set", n.set) || !OptionalString(e, np, "main", n.main) ||
                !OptionalString(e, np, "own_call_sign", n.own_call_sign) || !OptionalString(e, np, "main_call_sign", n.main_call_sign) ||
                !OptionalBool(e, np, "exempt", n.exempt)) {
                return false;
            }
            if (d.Find(n.id) != nullptr) return Fail(Join(np, "id"), "duplicate net '" + n.id + "'");
            const json::Value* f = OptionalArray(e, np, "frequencies_mhz");
            if (f == nullptr || f->items.empty()) return message_.empty() ? Fail(Join(np, "frequencies_mhz"), "required: working, spare, ...") : false;
            const VoiceSetDef* set = nullptr;
            if (!n.set.empty()) {
                for (const VoiceSetDef& vs : console.voice.sets) {
                    if (vs.id == n.set) set = &vs;
                }
                if (console.has_voice && set == nullptr) return Fail(Join(np, "set"), "not one of the console's voice sets");
            }
            for (std::size_t k = 0; k < f->items.size(); ++k) {
                const json::Value& x = f->items[k];
                if (!x.Is(json::Type::Number) || x.number <= 0.0) return Fail(Index(Join(np, "frequencies_mhz"), k), "a frequency, MHz");
                if (set != nullptr && set->band_hi_mhz > 0.0 && (x.number < set->band_lo_mhz || x.number > set->band_hi_mhz)) {
                    return Fail(Index(Join(np, "frequencies_mhz"), k), "outside " + set->name + "'s band");
                }
                n.frequencies_mhz.push_back(x.number);
            }
            for (const char* key : {"changes", "orders"}) {
                const json::Value* list = OptionalArray(e, np, key);
                if (list == nullptr) {
                    if (!message_.empty()) return false;
                    continue;
                }
                for (std::size_t k = 0; k < list->items.size(); ++k) {
                    const std::string cp = Index(Join(np, key), k);
                    RadioNetDef::Change c;
                    double to = 1.0;
                    if (!Keys(list->items[k], cp, {"at_s", "to", "notes"}) || !RequireNumber(list->items[k], cp, "at_s", c.at_s) ||
                        !RequireNumber(list->items[k], cp, "to", to)) {
                        return false;
                    }
                    c.to = static_cast<int>(to);
                    if (c.to < 1 || static_cast<std::size_t>(c.to) > n.frequencies_mhz.size()) return Fail(Join(cp, "to"), "a frequency number of the net");
                    (std::string(key) == "changes" ? n.changes : n.orders).push_back(c);
                }
            }
            if (!n.orders.empty() && n.main == "self") return Fail(Join(np, "orders"), "a net this vehicle runs takes its own orders");
            d.nets.push_back(std::move(n));
        }
        const json::Value* jam = OptionalArray(*r, p, "jamming");
        if (jam == nullptr && !message_.empty()) return false;
        for (std::size_t i = 0; jam != nullptr && i < jam->items.size(); ++i) {
            const std::string jp = Index(Join(p, "jamming"), i);
            const json::Value& e = jam->items[i];
            JammingDef j;
            std::string strength = "strong", kind = "barrage";
            if (!Keys(e, jp, {"nets", "band_mhz", "from_s", "to_s", "strength", "kind", "follow_s", "notes"}) ||
                !OptionalNumber(e, jp, "from_s", j.from_s) || !OptionalNumber(e, jp, "to_s", j.to_s) ||
                !OptionalString(e, jp, "strength", strength) || !OptionalString(e, jp, "kind", kind) ||
                !OptionalNumber(e, jp, "follow_s", j.follow_s)) {
                return false;
            }
            if (strength != "strong" && strength != "weak") return Fail(Join(jp, "strength"), "strong or weak");
            if (kind != "barrage" && kind != "spot") return Fail(Join(jp, "kind"), "barrage or spot");
            j.strong = strength == "strong";
            j.spot = kind == "spot";
            if (const json::Value* list = OptionalArray(e, jp, "nets")) {
                for (std::size_t k = 0; k < list->items.size(); ++k) {
                    if (!list->items[k].Is(json::Type::String) || d.Find(list->items[k].string) == nullptr) {
                        return Fail(Index(Join(jp, "nets"), k), "unknown net");
                    }
                    j.nets.push_back(list->items[k].string);
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (const json::Value* band = OptionalArray(e, jp, "band_mhz")) {
                if (band->items.size() != 2 || !band->items[0].Is(json::Type::Number) || !band->items[1].Is(json::Type::Number)) {
                    return Fail(Join(jp, "band_mhz"), "[lowest, highest] MHz");
                }
                j.band_lo_mhz = band->items[0].number;
                j.band_hi_mhz = band->items[1].number;
            } else if (!message_.empty()) {
                return false;
            }
            if (j.nets.empty() && !(j.band_hi_mhz > j.band_lo_mhz)) return Fail(jp, "nets or a band");
            if (j.spot && j.nets.size() != 1) return Fail(Join(jp, "nets"), "a spot jammer works one net");
            if (j.to_s <= j.from_s) return Fail(Join(jp, "to_s"), "after from_s");
            d.jamming.push_back(std::move(j));
        }
        if (const json::Value* so = OptionalArray(*r, p, "silence_orders")) {
            for (std::size_t i = 0; i < so->items.size(); ++i) {
                const std::string sp = Index(Join(p, "silence_orders"), i);
                RadioDataDef::SilenceOrder o;
                if (!Keys(so->items[i], sp, {"at_s", "on", "notes"}) || !RequireNumber(so->items[i], sp, "at_s", o.at_s) ||
                    !OptionalBool(so->items[i], sp, "on", o.on)) {
                    return false;
                }
                d.silence_orders.push_back(o);
            }
        } else if (!message_.empty()) {
            return false;
        }
        const json::Value* lines = OptionalArray(*r, p, "lines");
        if (lines == nullptr && !message_.empty()) return false;
        for (std::size_t i = 0; lines != nullptr && i < lines->items.size(); ++i) {
            const std::string lp = Index(Join(p, "lines"), i);
            const json::Value& e = lines->items[i];
            LineDef l;
            double n = 0.0;
            if (!Keys(e, lp, {"line", "to", "to_name", "laid", "far_end", "calls", "notes"}) || !RequireNumber(e, lp, "line", n) ||
                !RequireString(e, lp, "to", l.to) || !OptionalString(e, lp, "to_name", l.to_name) || !OptionalBool(e, lp, "laid", l.laid)) {
                return false;
            }
            if (const json::Value* fe = e.Find("far_end")) {
                if (!LoadPosition(*fe, Join(lp, "far_end"), frame_, l.far_end)) return false;
                l.has_far_end = true;
            }
            if (const json::Value* calls = OptionalArray(e, lp, "calls")) {
                for (std::size_t k = 0; k < calls->items.size(); ++k) {
                    const std::string cp = Index(Join(lp, "calls"), k);
                    LineDef::Call c;
                    if (!Keys(calls->items[k], cp, {"at_s", "text", "notes"}) || !RequireNumber(calls->items[k], cp, "at_s", c.at_s) ||
                        !RequireString(calls->items[k], cp, "text", c.text)) {
                        return false;
                    }
                    l.calls.push_back(std::move(c));
                }
            } else if (!message_.empty()) {
                return false;
            }
            l.line = static_cast<int>(n);
            if (l.line < 1 || l.line > 5) return Fail(Join(lp, "line"), "1-5 (the RTK-1's levers)");
            if (d.Line(l.line) != nullptr) return Fail(Join(lp, "line"), "two subscribers on one line");
            if (l.to_name.empty()) l.to_name = l.to;
            d.lines.push_back(std::move(l));
        }
        d.present = true;
        return true;
    }

    bool RequireObject(const json::Value& obj, const std::string& path, const char* key, const json::Value*& out) {
        out = obj.Find(key);
        if (out == nullptr) return Fail(Join(path, key), "required");
        if (!out->Is(json::Type::Object)) return Fail(Join(path, key), "expected an object");
        return true;
    }

    bool RequireNumber(const json::Value& obj, const std::string& path, const char* key, double& out) {
        if (obj.Find(key) == nullptr) return Fail(Join(path, key), "required");
        return OptionalNumber(obj, path, key, out);
    }

    // A position is either {"east_m", "north_m", "up_m"} (ENU at the origin) or {"lat_deg", "lon_deg", "alt_m"}.
    bool LoadPosition(const json::Value& v, const std::string& path, const LocalFrame& frame, Vec3& out) {
        if (!Keys(v, path, {"east_m", "north_m", "up_m", "lat_deg", "lon_deg", "alt_m"})) return false;
        if (v.Find("east_m") != nullptr || v.Find("north_m") != nullptr || v.Find("up_m") != nullptr) {
            if (v.Find("lat_deg") != nullptr || v.Find("lon_deg") != nullptr || v.Find("alt_m") != nullptr) {
                return Fail(path, "use either east/north/up or lat/lon/alt, not both");
            }
            return RequireNumber(v, path, "east_m", out.x) && RequireNumber(v, path, "north_m", out.y) &&
                   RequireNumber(v, path, "up_m", out.z);
        }
        double lat = 0.0;
        double lon = 0.0;
        double alt = 0.0;
        if (!RequireNumber(v, path, "lat_deg", lat) || !RequireNumber(v, path, "lon_deg", lon) ||
            !RequireNumber(v, path, "alt_m", alt)) {
            return false;
        }
        out = frame.GeodeticToEnu(GeoPoint::FromDegrees(lat, lon, alt));
        return true;
    }

    bool LoadOrigin(const json::Value& root, ScenarioDef& out) {
        const json::Value* o = nullptr;
        if (!RequireObject(root, "", "origin", o) || !Keys(*o, "origin", {"lat_deg", "lon_deg", "alt_m"})) return false;
        double lat = 0.0;
        double lon = 0.0;
        double alt = 0.0;
        if (!RequireNumber(*o, "origin", "lat_deg", lat) || !RequireNumber(*o, "origin", "lon_deg", lon) ||
            !RequireNumber(*o, "origin", "alt_m", alt)) {
            return false;
        }
        if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return Fail("origin", "latitude/longitude out of range");
        out.origin = GeoPoint::FromDegrees(lat, lon, alt);
        frame_ = LocalFrame(out.origin);
        return true;
    }

    bool LoadTerrain(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* t = nullptr;
        if (!RequireObject(root, "", "terrain", t) || !Keys(*t, "terrain", {"type", "height_m", "map", "ridges", "notes"})) return false;
        std::string type;
        if (!RequireString(*t, "terrain", "type", type)) return false;
        if (type == "map") {  // a generated map's ground (terrain_map.hpp); no ridges: the map has its own hills
            if (!RequireString(*t, "terrain", "map", out.map_file)) return false;
            if (t->Find("ridges") != nullptr || t->Find("height_m") != nullptr) return Fail("terrain", "a map has its own ground: no height_m or ridges");
            std::string error;
            out.map = LoadMapCached(data_dir, out.map_file, &error);
            if (!out.map) return Fail("terrain.map", error);
            out.ground_height_m = MapTerrain(out.map, out.origin).HeightAt(0.0, 0.0);
            return true;
        }
        if (type != "flat") return Fail("terrain.type", "must be \"flat\" or \"map\"");
        if (!RequireNumber(*t, "terrain", "height_m", out.ground_height_m)) return false;
        // "ridges": [ { "from": { "east_m", "north_m" }, "to": {...}, "height_m": 120, "width_m": 1000 } ]
        const json::Value* rs = OptionalArray(*t, "terrain", "ridges");
        if (rs == nullptr) return message_.empty();
        for (std::size_t i = 0; i < rs->items.size(); ++i) {
            const json::Value& r = rs->items[i];
            const std::string p = Index("terrain.ridges", i);
            const json::Value* a = nullptr;
            const json::Value* b = nullptr;
            TerrainRidge ridge;
            if (!Keys(r, p, {"from", "to", "height_m", "width_m", "notes"}) || !RequireObject(r, p, "from", a) ||
                !RequireObject(r, p, "to", b) || !Keys(*a, Join(p, "from"), {"east_m", "north_m"}) ||
                !Keys(*b, Join(p, "to"), {"east_m", "north_m"}) || !RequireNumber(*a, Join(p, "from"), "east_m", ridge.a_east_m) ||
                !RequireNumber(*a, Join(p, "from"), "north_m", ridge.a_north_m) || !RequireNumber(*b, Join(p, "to"), "east_m", ridge.b_east_m) ||
                !RequireNumber(*b, Join(p, "to"), "north_m", ridge.b_north_m) || !RequireNumber(r, p, "height_m", ridge.height_m) ||
                !RequireNumber(r, p, "width_m", ridge.width_m)) {
                return false;
            }
            if (ridge.height_m < 0.0) return Fail(Join(p, "height_m"), "must be >= 0");
            if (ridge.width_m <= 0.0) return Fail(Join(p, "width_m"), "must be > 0");
            out.ridges.push_back(ridge);
        }
        return true;
    }

    // A target's "iff": "friend" (replies to interrogation) or "none" (the default).
    bool LoadIff(const json::Value& t, const std::string& p, bool& friendly) {
        std::string iff;
        if (!OptionalString(t, p, "iff", iff)) return false;
        if (!iff.empty() && iff != "friend" && iff != "none") return Fail(Join(p, "iff"), "must be friend or none");
        friendly = iff == "friend";
        return true;
    }

    bool LoadOwnUnit(const json::Value& root, const std::string& data_dir, ScenarioDef& out, ConsoleData& console) {
        const json::Value* u = nullptr;
        if (!RequireObject(root, "", "own_unit", u) ||
            !Keys(*u, "own_unit", {"console", "tuning", "crew_data", "em", "readiness", "fuel_fraction", "radio_presets", "position_power",
                                   "prepared_position", "position", "heading_deg", "inputs", "controls", "empty_containers"})) {
            return false;
        }
        if (!RequireString(*u, "own_unit", "console", out.console_file)) return false;
        std::string error;
        if (!LoadConsoleFile(data_dir + "/" + out.console_file, console, &error)) {
            return Fail("own_unit.console", error);
        }
        // The crew's data (docs/crew/): named, or the system's own file when there is one.
        if (!OptionalString(*u, "own_unit", "crew_data", out.crew_file)) return false;
        const bool named_crew = !out.crew_file.empty();
        if (!named_crew) out.crew_file = "crew/" + console.system_id + "_crew.json";
        if (named_crew || std::ifstream(data_dir + "/" + out.crew_file).good()) {
            if (!LoadCrewDataFile(data_dir + "/" + out.crew_file, console, out.crew_data, &error)) return Fail("own_unit.crew_data", error);
            out.has_crew_data = true;
        } else {
            out.crew_file.clear();
        }
        // The vehicle's hatches, hull and the places round it (docs/crew/13_OUT_OF_THE_VEHICLE.md), when it has a file.
        const std::string vehicle_file = data_dir + "/vehicles/" + console.system_id + ".json";
        if (std::ifstream(vehicle_file).good()) {
            if (!LoadVehicleFile(vehicle_file, console, out.vehicle, &error)) return Fail("own_unit", error);
            if (!LoadWalkingFile(data_dir + "/crew/walking.json", out.walking, &error)) return Fail("own_unit", error);
            out.has_vehicle = true;
        }
        if (!OptionalString(*u, "own_unit", "tuning", out.tuning_file)) return false;
        if (!out.tuning_file.empty()) {
            if (!console.has_sensors) return Fail("own_unit.tuning", "the console has no \"sensors\" block");
            if (!LoadEngagementTuningFile(data_dir + "/" + out.tuning_file, out.tuning, &error)) {
                return Fail("own_unit.tuning", error);
            }
            if (out.tuning.system_id != console.system_id) {
                return Fail("own_unit.tuning", "is for '" + out.tuning.system_id + "', not '" + console.system_id + "'");
            }
            for (const std::string* role : {&out.tuning.aim_az_role, &out.tuning.aim_el_role, &out.tuning.aim_range_role}) {
                if (!console.stations.empty() && console.FindStation(*role) == nullptr) {
                    return Fail("own_unit.tuning", "aim role '" + *role + "' is not a station of the console");
                }
            }
        }
        if (!OptionalNumber(*u, "own_unit", "fuel_fraction", out.fuel_fraction)) return false;
        if (const json::Value* rp = u->Find("radio_presets")) {
            if (!rp->Is(json::Type::String) || (rp->string != "set" && rp->string != "unset")) {
                return Fail("own_unit.radio_presets", "\"set\" (the frequency plan on the fixators) or \"unset\"");
            }
            out.radio_presets_set = rp->string == "set";
        }
        if (out.fuel_fraction < 0.0 || out.fuel_fraction > 1.0) return Fail("own_unit.fuel_fraction", "from 0 to 1");
        if (!LoadPositionPower(*u, console, out.position_power)) return false;
        if (!OptionalString(*u, "own_unit", "readiness", out.readiness)) return false;
        if (out.readiness == "cold") out.readiness.clear();
        if (!out.readiness.empty() && console.FindReadiness(out.readiness) == nullptr) {
            std::string known;
            for (const ReadinessState& r : console.readiness) known += (known.empty() ? "" : ", ") + r.id;
            return Fail("own_unit.readiness", "unknown state '" + out.readiness + "' (cold" + (known.empty() ? "" : ", ") + known + ")");
        }
        if (const json::Value* em = u->Find("em")) {
            if (out.tuning_file.empty()) return Fail("own_unit.em", "needs own_unit.tuning: the EM layer replaces its sensors");
            std::string library, search, track;
            if (!em->Is(json::Type::Object) ||
                !Keys(*em, "own_unit.em", {"library", "search_radar", "track_radar", "extra_libraries", "wind_mps", "rain_mmh",
                                           "jammers", "chaff", "decoys", "notes"}) ||
                !RequireString(*em, "own_unit.em", "library", library) ||
                !RequireString(*em, "own_unit.em", "search_radar", search) ||
                !RequireString(*em, "own_unit.em", "track_radar", track)) {
                return message_.empty() ? Fail("own_unit.em", "expected an object") : false;
            }
            std::vector<std::string> extras;
            if (const json::Value* ex = OptionalArray(*em, "own_unit.em", "extra_libraries")) {
                for (std::size_t i = 0; i < ex->items.size(); ++i) {
                    if (!ex->items[i].Is(json::Type::String)) return Fail(Index("own_unit.em.extra_libraries", i), "expected a file name");
                    extras.push_back(ex->items[i].string);
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (!LoadEmLinkDef(data_dir, library, search, track, out.em, &error, extras)) return Fail("own_unit.em", error);
            if (!OptionalNumber(*em, "own_unit.em", "rain_mmh", out.em.rain_mmh)) return false;
            if (out.em.rain_mmh < 0.0) return Fail("own_unit.em.rain_mmh", "must be >= 0");
            if (const json::Value* w = em->Find("wind_mps")) {
                if (!Keys(*w, "own_unit.em.wind_mps", {"east", "north", "notes"}) ||
                    !OptionalNumber(*w, "own_unit.em.wind_mps", "east", out.em.wind_mps.x) ||
                    !OptionalNumber(*w, "own_unit.em.wind_mps", "north", out.em.wind_mps.y)) {
                    return false;
                }
            }
            if (!LoadEmJammers(*em, "own_unit.em", out.em)) return false;
            if (const json::Value* ch = OptionalArray(*em, "own_unit.em", "chaff")) {
                for (std::size_t i = 0; i < ch->items.size(); ++i) {
                    const std::string cp = Index("own_unit.em.chaff", i);
                    const json::Value& c = ch->items[i];
                    if (!Keys(c, cp, {"target", "at_s", "rcs_m2", "lifetime_s", "notes"})) return false;
                    EmChaffDef cd;
                    if (!RequireString(c, cp, "target", cd.target_id) || !RequireNumber(c, cp, "at_s", cd.at_s) ||
                        !OptionalNumber(c, cp, "rcs_m2", cd.rcs_m2) || !OptionalNumber(c, cp, "lifetime_s", cd.lifetime_s)) {
                        return false;
                    }
                    if (cd.rcs_m2 <= 0.0) return Fail(Join(cp, "rcs_m2"), "must be > 0");
                    if (cd.lifetime_s <= 0.0) return Fail(Join(cp, "lifetime_s"), "must be > 0");
                    out.em.chaff.push_back(cd);
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (const json::Value* dl = OptionalArray(*em, "own_unit.em", "decoys")) {
                for (std::size_t i = 0; i < dl->items.size(); ++i) {
                    const std::string dpth = Index("own_unit.em.decoys", i);
                    const json::Value& dv = dl->items[i];
                    if (!Keys(dv, dpth, {"position", "mode", "notes"})) return false;
                    EmDecoyDef dd;
                    std::string mode = "with_radar";
                    if (!OptionalString(dv, dpth, "mode", mode)) return false;
                    if (!em::ParseDecoyMode(mode, dd.mode)) return Fail(Join(dpth, "mode"), "with_radar, when_radar_off or always");
                    const json::Value* dpos = dv.Find("position");
                    if (dpos == nullptr) return Fail(Join(dpth, "position"), "required");
                    if (!LoadPosition(*dpos, Join(dpth, "position"), frame_, dd.position)) return false;
                    out.em.decoys.push_back(dd);
                }
            } else if (!message_.empty()) {
                return false;
            }
            out.has_em = true;
        }
        if (const json::Value* pp = u->Find("prepared_position")) {
            if (!LoadPrepared(*pp, data_dir, out)) return false;
        }
        const json::Value* pos = nullptr;
        if (!RequireObject(*u, "own_unit", "position", pos) ||
            !LoadPosition(*pos, "own_unit.position", frame_, out.unit_position) ||
            !OptionalNumber(*u, "own_unit", "heading_deg", out.unit_heading_deg)) {
            return false;
        }
        if (const json::Value* ec = OptionalArray(*u, "own_unit", "empty_containers")) {
            for (std::size_t i = 0; i < ec->items.size(); ++i) {
                const std::string p = Index("own_unit.empty_containers", i);
                if (!ec->items[i].Is(json::Type::String)) return Fail(p, "expected a container id");
                bool known = false;
                for (const auto& c : console.launcher.containers) known = known || c.id == ec->items[i].string;
                if (!console.has_launcher || !known) return Fail(p, "the console's launcher has no container '" + ec->items[i].string + "'");
                out.empty_containers.push_back(ec->items[i].string);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (const json::Value* in = u->Find("inputs")) {
            if (!in->Is(json::Type::Object)) return Fail("own_unit.inputs", "expected an object of input: true/false");
            for (std::size_t i = 0; i < in->keys.size(); ++i) {
                const std::string p = Join("own_unit.inputs", in->keys[i]);
                if (!console.equipment.HasInput(in->keys[i])) return Fail(p, "unknown input");
                if (!in->items[i].Is(json::Type::Bool)) return Fail(p, "expected true or false");
                out.initial_inputs.emplace_back(in->keys[i], in->items[i].boolean);
            }
        }
        // Controls set at the start after the readiness (a lesson starting with the headset off).
        if (const json::Value* cs = u->Find("controls")) {
            if (!cs->Is(json::Type::Object)) return Fail("own_unit.controls", "expected an object of control: position");
            for (std::size_t i = 0; i < cs->keys.size(); ++i) {
                const std::string p = Join("own_unit.controls", cs->keys[i]);
                const ControlDef* c = console.panel.Find(cs->keys[i]);
                if (c == nullptr) return Fail(p, "unknown control");
                if (!cs->items[i].Is(json::Type::Number)) return Fail(p, "expected a position");
                const int at = static_cast<int>(cs->items[i].number);
                if (at < 0 || at >= c->positions) return Fail(p, "not a position of the control");
                out.initial_controls.emplace_back(cs->keys[i], at);
            }
        }
        return true;
    }

    bool LoadCrew(const json::Value& root, const std::string& data_dir, ScenarioDef& out, const ConsoleData& console) {
        const json::Value* arr = OptionalArray(root, "", "crew");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("crew", i);
            const json::Value& c = arr->items[i];
            ScenarioCrew crew;
            if (!Keys(c, p, {"name", "role", "reaction_time_s", "reaction_jitter_s", "skill"}) ||
                !RequireString(c, p, "name", crew.name) || !RequireString(c, p, "role", crew.role) ||
                !OptionalNumber(c, p, "reaction_time_s", crew.reaction_time_s) ||
                !OptionalNumber(c, p, "reaction_jitter_s", crew.reaction_jitter_s) || !OptionalString(c, p, "skill", crew.skill)) {
                return false;
            }
            if (!crew.skill.empty()) {
                std::string error;
                if (!out.has_skills && !LoadSkillTableFile(data_dir + "/crew/skill_levels.json", out.skills, &error)) {
                    return Fail(Join(p, "skill"), error);
                }
                out.has_skills = true;
                if (out.skills.Find(crew.skill) == nullptr) return Fail(Join(p, "skill"), "unknown skill tier '" + crew.skill + "'");
            }
            if (crew.reaction_time_s <= 0.0) return Fail(Join(p, "reaction_time_s"), "must be > 0");
            if (crew.reaction_jitter_s < 0.0 || crew.reaction_jitter_s >= crew.reaction_time_s) {
                return Fail(Join(p, "reaction_jitter_s"), "must be >= 0 and less than reaction_time_s");
            }
            if (!console.stations.empty() && console.FindStation(crew.role) == nullptr) {
                return Fail(Join(p, "role"), "'" + crew.role + "' is not a station of the console");
            }
            for (const ScenarioCrew& other : out.crew) {
                if (other.name == crew.name) return Fail(Join(p, "name"), "duplicate crew name '" + crew.name + "'");
                if (other.role == crew.role) return Fail(Join(p, "role"), "'" + crew.role + "' already has a crew member");
            }
            out.crew.push_back(std::move(crew));
        }
        return true;
    }

    // A target's "behaviour" (terrain_following, pop_up) and "evade" blocks (target_behaviour.hpp).
    bool LoadTargetBehaviour(const json::Value& t, const std::string& p, ScenarioTarget& tg) {
        if (const json::Value* b = t.Find("behaviour")) {
            const std::string bp = Join(p, "behaviour");
            if (!b->Is(json::Type::Object)) return Fail(bp, "expected an object");
            std::string kind;
            if (!RequireString(*b, bp, "kind", kind)) return false;
            TargetBehaviourDef& d = tg.behaviour;
            if (kind == "terrain_following") {
                d.kind = TargetBehaviourDef::Kind::TerrainFollowing;
                if (!Keys(*b, bp, {"kind", "agl_m", "max_climb_mps", "lookahead_s", "notes"}) ||
                    !OptionalNumber(*b, bp, "agl_m", d.agl_m)) {
                    return false;
                }
            } else if (kind == "pop_up") {
                d.kind = TargetBehaviourDef::Kind::PopUp;
                if (!Keys(*b, bp, {"kind", "hide_agl_m", "popup_agl_m", "first_hold_s", "expose_s", "hidden_s", "cycles",
                                   "max_climb_mps", "lookahead_s", "notes"}) ||
                    !OptionalNumber(*b, bp, "hide_agl_m", d.hide_agl_m) || !OptionalNumber(*b, bp, "popup_agl_m", d.popup_agl_m) ||
                    !OptionalNumber(*b, bp, "first_hold_s", d.first_hold_s) || !OptionalNumber(*b, bp, "expose_s", d.expose_s) ||
                    !OptionalNumber(*b, bp, "hidden_s", d.hidden_s) || !OptionalInt(*b, bp, "cycles", d.cycles)) {
                    return false;
                }
                if (d.popup_agl_m <= d.hide_agl_m) return Fail(Join(bp, "popup_agl_m"), "must be above hide_agl_m");
                if (d.cycles < 1) return Fail(Join(bp, "cycles"), "must be >= 1");
                if (tg.waypoints.size() < 2) return Fail(Join(p, "waypoints"), "a pop-up needs the hide and at least one waypoint after it");
            } else {
                return Fail(Join(bp, "kind"), "unknown behaviour '" + kind + "' (terrain_following, pop_up)");
            }
            if (!OptionalNumber(*b, bp, "max_climb_mps", d.max_climb_mps) || !OptionalNumber(*b, bp, "lookahead_s", d.lookahead_s)) return false;
            if (d.agl_m < 0.0 || d.hide_agl_m < 0.0) return Fail(bp, "heights above the ground must be >= 0");
            if (d.max_climb_mps <= 0.0) return Fail(Join(bp, "max_climb_mps"), "must be > 0");
            if (d.lookahead_s < 0.0) return Fail(Join(bp, "lookahead_s"), "must be >= 0");
        }
        if (const json::Value* e = t.Find("evade")) {
            const std::string ep = Join(p, "evade");
            if (t.Find("evade_on_launch") != nullptr) return Fail(ep, "use either evade or evade_on_launch, not both");
            if (!e->Is(json::Type::Object)) return Fail(ep, "expected an object");
            if (!Keys(*e, ep, {"on", "kind", "turn_rate_deg_s", "dive_agl_m", "jink_period_s", "evade_s", "notes"})) return false;
            EvadeDef& d = tg.evade;
            std::string on = "launch", kind = "break";
            double turn_deg_s = RadToDeg(d.turn_rate_rad_s);
            if (!OptionalString(*e, ep, "on", on) || !OptionalString(*e, ep, "kind", kind) ||
                !OptionalNumber(*e, ep, "turn_rate_deg_s", turn_deg_s) || !OptionalNumber(*e, ep, "dive_agl_m", d.dive_agl_m) ||
                !OptionalNumber(*e, ep, "jink_period_s", d.jink_period_s) || !OptionalNumber(*e, ep, "evade_s", d.evade_s)) {
                return false;
            }
            if (on == "launch") {
                d.on = EvadeDef::On::Launch;
            } else if (on == "lock") {
                d.on = EvadeDef::On::Lock;
            } else {
                return Fail(Join(ep, "on"), "unknown cue '" + on + "' (launch, lock)");
            }
            if (kind == "break") {
                d.kind = EvadeDef::Kind::Break;
            } else if (kind == "dive") {
                d.kind = EvadeDef::Kind::Dive;
            } else if (kind == "jink") {
                d.kind = EvadeDef::Kind::Jink;
            } else {
                return Fail(Join(ep, "kind"), "unknown evasion '" + kind + "' (break, dive, jink)");
            }
            if (turn_deg_s <= 0.0) return Fail(Join(ep, "turn_rate_deg_s"), "must be > 0");
            if (d.dive_agl_m < 0.0) return Fail(Join(ep, "dive_agl_m"), "must be >= 0");
            if (d.jink_period_s <= 0.0 || d.evade_s <= 0.0) return Fail(ep, "jink_period_s and evade_s must be > 0");
            d.turn_rate_rad_s = DegToRad(turn_deg_s);
            d.present = true;
        }
        return true;
    }

    // "environment": {"visibility_km": 40, "daylight": true}: the air and the light (the TV sight, em/optical.hpp).
    bool LoadEnvironment(const json::Value& root, ScenarioDef& out) {
        const json::Value* env = root.Find("environment");
        if (env == nullptr) return true;
        double visibility_km = out.optics.visibility_m / 1000.0;
        if (!Keys(*env, "environment", {"visibility_km", "daylight", "notes"}) ||
            !OptionalNumber(*env, "environment", "visibility_km", visibility_km) ||
            !OptionalBool(*env, "environment", "daylight", out.optics.daylight)) {
            return false;
        }
        if (visibility_km <= 0.0) return Fail("environment.visibility_km", "must be > 0");
        out.optics.visibility_m = visibility_km * 1000.0;
        return true;
    }

    // "weather": "cloudy" (a preset id, or the air war's clear/cloudy/poor). Without an "environment" the preset's
    // surface visibility is the TV's; without own_unit.em.rain_mmh its rain falls on the radars.
    bool LoadWeather(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* w = root.Find("weather");
        if (w == nullptr) return true;
        if (!w->Is(json::Type::String) || w->string.empty()) return Fail("weather", "expected a preset id (data/weather/presets.json)");
        std::string error;
        if (!FindWeatherPreset(data_dir, w->string, out.weather, &error)) return Fail("weather", error);
        out.has_weather = true;
        const json::Value* env = root.Find("environment");
        if (env == nullptr || env->Find("visibility_km") == nullptr) out.optics.visibility_m = out.weather.visibility_km * 1000.0;
        if (out.has_em && out.em.rain_mmh <= 0.0) out.em.rain_mmh = out.weather.rain_mmh;
        return true;
    }

    // data/command/emission_regimes.json, if the data folder has it (else the code's defaults, the same numbers).
    bool LoadEmissionData(const std::string& data_dir, ScenarioDef& out) {
        const std::string path = data_dir + "/command/emission_regimes.json";
        if (!std::ifstream(path, std::ios::binary)) return true;
        std::string error;
        if (!LoadEmissionFile(path, out.emission, &error)) return Fail("emission_regimes", error);
        return true;
    }
    // {"regime": "limited", "on_turns": 2, "off_turns": 6, "from": "regiment"}: the turns default to the data's.
    bool LoadEmissionRegime(const json::Value& v, const std::string& p, const ScenarioDef& def, EmissionRegime& out) {
        std::string kind, from = "battery";
        if (!RequireString(v, p, "regime", kind) || !OptionalString(v, p, "from", from)) return false;
        EmissionKind k = EmissionKind::Free;
        if (!ParseEmissionKind(kind, k)) return Fail(Join(p, "regime"), "must be \"free\", \"limited\", \"arm\" or \"silent\"");
        if (from != "battery" && from != "regiment") return Fail(Join(p, "from"), "must be \"battery\" or \"regiment\"");
        out = def.emission.Make(k, from);
        if (!OptionalNumber(v, p, "on_turns", out.on_turns) || !OptionalNumber(v, p, "off_turns", out.off_turns)) return false;
        if (!out.Bursts() && (v.Find("on_turns") != nullptr || v.Find("off_turns") != nullptr)) {
            return Fail(p, "only the limited and arm regimes have bursts (on_turns, off_turns)");
        }
        if (out.Bursts() && (out.on_turns <= 0.0 || out.off_turns <= 0.0)) return Fail(p, "on_turns and off_turns must be > 0");
        return true;
    }
    // "emission_orders": the regime changed during the mission, from the regiment or the battery commander.
    bool LoadEmissionOrders(const json::Value& root, ScenarioDef& out) {
        const json::Value* list = OptionalArray(root, "", "emission_orders");
        if (list == nullptr) return message_.empty();
        if (!out.has_battery && !out.has_post) return Fail("emission_orders", "an emission order goes to a battery: the scenario has none");
        if (out.battery_commander == "post") return Fail("emission_orders", "the battery commander is at the linked post: its scenario gives them");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index("emission_orders", i);
            const json::Value& o = list->items[i];
            ScenarioDef::EmissionOrderDef e;
            if (!Keys(o, p, {"at_s", "regime", "on_turns", "off_turns", "from", "notes"}) || !RequireNumber(o, p, "at_s", e.at_s) ||
                !LoadEmissionRegime(o, p, out, e.regime)) {
                return false;
            }
            if (e.at_s <= 0.0) return Fail(Join(p, "at_s"), "> 0: the regime the mission starts under is the doctrine's \"emission\"");
            if (e.regime.from == "regiment" && out.has_post && out.radio_data.Find("regiment") == nullptr) {
                return Fail(Join(p, "from"), "the regiment's order goes on its net: radio_data has no net \"regiment\"");
            }
            out.emission_orders.push_back(e);
        }
        return true;
    }

    // "doctrine": {"tv_check": true, "emission": {"regime": "limited"}, "salvo": 2}.
    bool LoadDoctrine(const json::Value& root, ScenarioDef& out) {
        const json::Value* d = root.Find("doctrine");
        if (d == nullptr) return true;
        bool arm_threat = false;
        double arm_off_turns = out.emission.arm.off_turns;
        if (!Keys(*d, "doctrine", {"tv_check", "emission", "search_bursts", "salvo", "arm_threat", "arm_silence_s", "arm_search_off_turns",
                                   "relocate_after_arm", "notes"}) ||
            !OptionalBool(*d, "doctrine", "tv_check", out.doctrine.tv_check) ||
            !OptionalInt(*d, "doctrine", "salvo", out.doctrine.salvo) || !OptionalBool(*d, "doctrine", "arm_threat", arm_threat) ||
            !OptionalNumber(*d, "doctrine", "arm_silence_s", out.doctrine.arm_silence_s) ||
            !OptionalNumber(*d, "doctrine", "arm_search_off_turns", arm_off_turns)) {
            return false;
        }
        if (out.doctrine.arm_silence_s < 0.0) return Fail("doctrine.arm_silence_s", "must be >= 0");
        if (arm_off_turns <= 0.0) return Fail("doctrine.arm_search_off_turns", "must be > 0");
        const json::Value* em = d->Find("emission");
        if (em != nullptr && (arm_threat || d->Find("search_bursts") != nullptr || d->Find("arm_search_off_turns") != nullptr)) {
            return Fail("doctrine.emission", "give the regime once: \"emission\", or the older \"arm_threat\" / \"search_bursts\"");
        }
        if (em != nullptr) {
            if (!em->Is(json::Type::Object) || !Keys(*em, "doctrine.emission", {"regime", "on_turns", "off_turns", "from", "notes"}) ||
                !LoadEmissionRegime(*em, "doctrine.emission", out, out.doctrine.emission)) {
                return message_.empty() ? Fail("doctrine.emission", "expected an object") : false;
            }
        } else if (arm_threat) {
            out.doctrine.emission = out.emission.Make(EmissionKind::Arm, "battery");
            out.doctrine.emission.off_turns = arm_off_turns;
        }
        if (const json::Value* r = d->Find("relocate_after_arm")) {
            const json::Value* to = nullptr;
            if (!Keys(*r, "doctrine.relocate_after_arm", {"to", "notes"}) || !RequireObject(*r, "doctrine.relocate_after_arm", "to", to) ||
                !LoadPosition(*to, "doctrine.relocate_after_arm.to", frame_, out.doctrine.relocate_to)) {
                return false;
            }
            if (!out.has_battery || !out.battery_drive_own) {
                return Fail("doctrine.relocate_after_arm", "needs a battery that drives this vehicle (battery.drive_own)");
            }
            out.doctrine.relocate_after_arm = true;
        }
        if (out.doctrine.salvo < 1 || out.doctrine.salvo > 2) return Fail("doctrine.salvo", "must be 1 or 2");
        if (const json::Value* b = d->Find("search_bursts")) {
            double on_s = 0.0, off_s = 0.0;
            if (!Keys(*b, "doctrine.search_bursts", {"on_s", "off_s", "notes"}) || !RequireNumber(*b, "doctrine.search_bursts", "on_s", on_s) ||
                !RequireNumber(*b, "doctrine.search_bursts", "off_s", off_s)) {
                return false;
            }
            if (on_s <= 0.0 || off_s <= 0.0) return Fail("doctrine.search_bursts", "on_s and off_s must be > 0");
            if (!arm_threat) {  // the least emission covers the bursts
                out.doctrine.emission = out.emission.Make(EmissionKind::Limited, "battery");
                out.doctrine.emission.on_s = on_s;
                out.doctrine.emission.off_s = off_s;
            }
        }
        return true;
    }

    // A "reactions" block's "hot" (roadmap S5, reactions.hpp ReactionPilotDef::Hot; air_war/raid_scenario.hpp writes it
    // under hot_phase.mode 2): {"initiative", "plan_adherence", "turn_back" (0..1), "leader_may" (§7.3's acts; absent: all),
    // "delay_s": [lo, hi], "improvised_extra_s", "husbanding_shift" (-1..0)}.
    bool LoadReactionHot(const json::Value& r, const std::string& rp, ReactionPilotDef::Hot& h) {
        const json::Value* v = r.Find("hot");
        if (v == nullptr) return true;
        const std::string p = Join(rp, "hot");
        std::vector<std::string> may;
        bool has_may = v->Find("leader_may") != nullptr;
        if (!Keys(*v, p, {"initiative", "plan_adherence", "turn_back", "leader_may", "delay_s", "improvised_extra_s", "husbanding_shift", "notes"}) ||
            !OptionalNumber(*v, p, "initiative", h.initiative) || !OptionalNumber(*v, p, "plan_adherence", h.plan_adherence) ||
            !OptionalNumber(*v, p, "turn_back", h.turn_back) || !OptionalNumber(*v, p, "improvised_extra_s", h.improvised_extra_s) ||
            !OptionalNumber(*v, p, "husbanding_shift", h.husbanding_shift) ||
            !StringList(*v, p, "leader_may", may, [](const std::string& s) {
                const std::vector<std::string>& acts = airwar::HotLeaderActs();
                return std::find(acts.begin(), acts.end(), s) != acts.end() ? std::string() : std::string("not a leader's act");
            })) {
            return false;
        }
        for (const double x : {h.initiative, h.plan_adherence, h.turn_back}) {
            if (x < 0.0 || x > 1.0) return Fail(p, "initiative, plan_adherence and turn_back are 0 to 1");
        }
        if (h.husbanding_shift > 0.0 || h.husbanding_shift < -1.0) return Fail(Join(p, "husbanding_shift"), "-1 to 0");
        if (h.improvised_extra_s < 0.0 || h.improvised_extra_s > 60.0) return Fail(Join(p, "improvised_extra_s"), "0 to 60 s");
        if (const json::Value* ds = v->Find("delay_s")) {
            if (!ds->Is(json::Type::Array) || ds->items.size() != 2 || !ds->items[0].Is(json::Type::Number) || !ds->items[1].Is(json::Type::Number) ||
                ds->items[0].number < 0.0 || ds->items[1].number < ds->items[0].number || ds->items[1].number > 60.0) {
                return Fail(Join(p, "delay_s"), "[lo, hi] seconds, 0 <= lo <= hi <= 60");
            }
            h.delay_lo_s = ds->items[0].number;
            h.delay_hi_s = ds->items[1].number;
        }
        if (has_may) {
            const auto has = [&may](const char* a) { return std::find(may.begin(), may.end(), a) != may.end(); };
            h.may_route = has("route") || has("everything");
            h.may_turn_back = has("abort") || has("break_off") || has("everything");
            h.may_programme = has("programme") || has("everything");
        }
        h.on = true;
        return true;
    }

    // A target's "arms" (ArmCarrierDef): its warning receiver tells it what to fire at, so it needs one.
    // A target's "reactions" (roadmap B4, reactions.hpp): {"profile", "aircraft", "chaff", "flares", "tanks", "stores",
    // "nerve", "press_on_shift", "jettison_shift", "zones": [{"east_m", "north_m", "radius_m"}]}. Not with "evade" or
    // "evade_on_launch". The shifts: his officer's (roadmap B11, air_war/hot_phase.hpp), at most 0.2 either way.
    bool LoadReactionPilot(const json::Value& t, const std::string& tp, ScenarioTarget& tg) {
        const json::Value* r = t.Find("reactions");
        if (r == nullptr) return true;
        const std::string p = Join(tp, "reactions");
        if (t.Find("evade") != nullptr || t.Find("evade_on_launch") != nullptr) return Fail(p, "a target has \"reactions\" or \"evade\", not both");
        ReactionPilotDef& d = tg.reactions;
        if (!Keys(*r, p, {"profile", "aircraft", "chaff", "flares", "tanks", "stores", "nerve", "press_on_shift", "jettison_shift", "zones", "hot", "notes"}) ||
            !RequireString(*r, p, "profile", d.profile) || !OptionalString(*r, p, "aircraft", d.aircraft) ||
            !OptionalInt(*r, p, "chaff", d.chaff) || !OptionalInt(*r, p, "flares", d.flares) || !OptionalInt(*r, p, "tanks", d.tanks) ||
            !OptionalString(*r, p, "stores", d.stores) || !OptionalNumber(*r, p, "nerve", d.nerve) ||
            !OptionalNumber(*r, p, "press_on_shift", d.press_on_shift) || !OptionalNumber(*r, p, "jettison_shift", d.jettison_shift)) {
            return false;
        }
        if (d.nerve < 0.0 || d.nerve > 1.0) return Fail(Join(p, "nerve"), "0 to 1");
        if (std::fabs(d.press_on_shift) > 0.2 || std::fabs(d.jettison_shift) > 0.2) return Fail(p, "an officer's shift is at most 0.2 either way");
        if (!LoadReactionHot(*r, p, d.hot)) return false;
        if (const json::Value* zs = OptionalArray(*r, p, "zones")) {
            for (std::size_t i = 0; i < zs->items.size(); ++i) {
                const std::string zp = Index(Join(p, "zones"), i);
                const json::Value& z = zs->items[i];
                ProgrammeZone zn;
                if (!Keys(z, zp, {"east_m", "north_m", "radius_m"}) || !RequireNumber(z, zp, "east_m", zn.centre.x) ||
                    !RequireNumber(z, zp, "north_m", zn.centre.y) || !RequireNumber(z, zp, "radius_m", zn.radius_m)) {
                    return false;
                }
                if (zn.radius_m <= 0.0) return Fail(Join(zp, "radius_m"), "must be > 0");
                d.zones.push_back(zn);
            }
        } else if (!message_.empty()) {
            return false;
        }
        d.on = true;
        return true;
    }

    // The reaction tables (always, when the data folder has them; required when a target reacts) and the damage table
    // ("damage", the testbed's own aside).
    bool LoadReactionsAndDamage(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        bool needed = false;
        for (const ScenarioTarget& t : out.targets) needed = needed || t.reactions.on;
        std::string error;
        if (std::ifstream(data_dir + "/gameplay/reactions.json").good() || needed) {
            if (!LoadReactionTable(data_dir, out.reactions, &error)) return Fail("", error);
        }
        for (std::size_t i = 0; i < out.targets.size(); ++i) {
            const ReactionPilotDef& d = out.targets[i].reactions;
            if (d.on && out.reactions.Profile(d.profile) == nullptr) {
                return Fail(Join(Index("targets", i), "reactions.profile"), "no profile '" + d.profile + "' in gameplay/reactions.json");
            }
        }
        std::string damage;
        if (!OptionalString(root, "", "damage", damage)) return false;
        if (!damage.empty()) {
            if (!LoadDamageFile(data_dir + "/" + damage, out.damage, &error)) return Fail("damage", error);
            out.has_damage = true;
        }
        return true;
    }

    bool LoadArmCarrier(const json::Value& t, const std::string& tp, const ScenarioDef& out, ScenarioTarget& tg) {
        const json::Value* a = t.Find("arms");
        if (a == nullptr) return true;
        const std::string p = Join(tp, "arms");
        ArmCarrierDef& d = tg.arms;
        double launch_km = 0.0, min_km = -1.0;
        d.count = 1;
        if (!Keys(*a, p, {"weapon", "count", "launch_range_km", "min_range_km", "interval_s", "aim", "turn_away", "notes"}) ||
            !RequireString(*a, p, "weapon", d.weapon) || !OptionalInt(*a, p, "count", d.count) ||
            !OptionalNumber(*a, p, "launch_range_km", launch_km) || !OptionalNumber(*a, p, "min_range_km", min_km) ||
            !OptionalNumber(*a, p, "interval_s", d.interval_s) || !OptionalBool(*a, p, "turn_away", d.turn_away)) {
            return false;
        }
        if (d.count < 1) return Fail(Join(p, "count"), "must be >= 1");
        if (d.interval_s < 0.0) return Fail(Join(p, "interval_s"), "must be >= 0");
        if (!out.has_em) return Fail(p, "needs own_unit.em: an anti-radiation missile is an EM platform");
        if (tg.rwr.empty()) return Fail(p, "needs the target's rwr: it fires at what its warning receiver hears");
        const int w = out.em.library.Arm(d.weapon);
        if (w < 0) return Fail(Join(p, "weapon"), "'" + d.weapon + "' is not an anti-radiation missile of the EM libraries");
        const em::ArmDef& def = out.em.library.arms[static_cast<std::size_t>(w)];
        d.launch_range_m = launch_km > 0.0 ? launch_km * 1000.0 : def.max_range_m;
        d.min_range_m = min_km >= 0.0 ? min_km * 1000.0 : def.min_range_m;
        if (d.min_range_m >= d.launch_range_m) return Fail(Join(p, "min_range_km"), "must be below the launch range");
        if (const json::Value* aim = a->Find("aim")) {
            if (!LoadPosition(*aim, Join(p, "aim"), frame_, d.aim)) return false;
            d.has_aim = true;
        }
        return true;
    }

    // "scenery": [{"id", "name", "model", "position", "heading_deg", "side": "own" | "enemy"}].
    bool LoadScenery(const json::Value& root, ScenarioDef& out) {
        const json::Value* arr = OptionalArray(root, "", "scenery");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& v = arr->items[i];
            const std::string p = Index("scenery", i);
            ScenarioScenery s;
            std::string side = "own";
            const json::Value* at = nullptr;
            if (!Keys(v, p, {"id", "name", "model", "position", "heading_deg", "side", "notes"}) || !RequireString(v, p, "id", s.id) ||
                !RequireString(v, p, "name", s.name) || !RequireString(v, p, "model", s.model) || !RequireObject(v, p, "position", at) ||
                !LoadPosition(*at, Join(p, "position"), frame_, s.position) || !OptionalNumber(v, p, "heading_deg", s.heading_deg) ||
                !OptionalString(v, p, "side", side)) {
                return false;
            }
            if (side != "own" && side != "enemy") return Fail(Join(p, "side"), "\"own\" or \"enemy\"");
            s.friendly = side == "own";
            for (const ScenarioScenery& o : out.scenery) {
                if (o.id == s.id) return Fail(Join(p, "id"), "duplicate '" + s.id + "'");
            }
            out.scenery.push_back(std::move(s));
        }
        return true;
    }

    bool LoadWaves(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        if (!OptionalString(root, "", "waves", out.waves_file) || out.waves_file.empty()) return message_.empty();
        auto waves = std::make_shared<airwar::WavesDef>();
        std::string e;
        if (!airwar::LoadWavesFile(data_dir, out.waves_file, *waves, &e)) return Fail("waves", e);
        out.waves = std::move(waves);
        return true;
    }

    // "flights" and "packages" (roadmap B1, flights.hpp).
    bool LoadFlights(const json::Value& root, ScenarioDef& out) {
        if (const json::Value* arr = OptionalArray(root, "", "flights")) {
            for (std::size_t i = 0; i < arr->items.size(); ++i) {
                const json::Value& v = arr->items[i];
                const std::string p = Index("flights", i);
                ScenarioFlight f;
                if (!Keys(v, p, {"id", "callsign", "package", "side", "keep_slots", "crewed", "call_s", "formations", "radio", "source", "confidence", "notes"}) ||
                    !RequireString(v, p, "id", f.id) || !OptionalString(v, p, "callsign", f.callsign) ||
                    !OptionalString(v, p, "package", f.package) || !OptionalString(v, p, "side", f.side) ||
                    !OptionalBool(v, p, "keep_slots", f.keep_slots) || !OptionalBool(v, p, "crewed", f.crewed) ||
                    !OptionalNumber(v, p, "call_s", f.call_s)) {
                    return false;
                }
                if (f.side != "enemy" && f.side != "ours") return Fail(Join(p, "side"), "\"enemy\" or \"ours\"");
                if (f.call_s < 0.0) return Fail(Join(p, "call_s"), "must be >= 0");
                if (f.callsign.empty()) f.callsign = f.id;
                if (const json::Value* rd = v.Find("radio")) {
                    const std::string rp = Join(p, "radio");
                    if (!rd->Is(json::Type::Object)) return Fail(rp, "expected an object");
                    if (!Keys(*rd, rp, {"unit", "role", "discipline_adjust", "have_quick", "secure_voice", "comm_out", "codeword", "notes"}) ||
                        !OptionalString(*rd, rp, "unit", f.radio.unit) || !OptionalString(*rd, rp, "role", f.radio.role) ||
                        !OptionalNumber(*rd, rp, "discipline_adjust", f.radio.discipline_adjust) ||
                        !OptionalBool(*rd, rp, "have_quick", f.radio.have_quick) || !OptionalBool(*rd, rp, "secure_voice", f.radio.secure_voice) ||
                        !OptionalBool(*rd, rp, "comm_out", f.radio.comm_out) || !OptionalString(*rd, rp, "codeword", f.radio.codeword)) {
                        return false;
                    }
                }
                if (const json::Value* fm = v.Find("formations")) {
                    const std::string fp = Join(p, "formations");
                    if (!fm->Is(json::Type::Object)) return Fail(fp, "expected an object of phases");
                    if (!Keys(*fm, fp, {"transit", "ingress", "attack", "egress", "defensive", "orbit"})) return false;
                    for (std::size_t k = 0; k < fm->keys.size() && k < fm->items.size(); ++k) {
                        std::vector<Vec3> slots;
                        if (!LoadSlots(fm->items[k], Join(fp, fm->keys[k]), slots)) return false;
                        f.formations[fm->keys[k]] = std::move(slots);
                    }
                }
                for (const ScenarioFlight& o : out.flights) {
                    if (o.id == f.id) return Fail(Join(p, "id"), "duplicate flight '" + f.id + "'");
                }
                out.flights.push_back(std::move(f));
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (const json::Value* arr = OptionalArray(root, "", "packages")) {
            for (std::size_t i = 0; i < arr->items.size(); ++i) {
                const json::Value& v = arr->items[i];
                const std::string p = Index("packages", i);
                ScenarioPackage pk;
                if (!Keys(v, p, {"id", "commander", "size", "lost_before", "turn_back_share", "source", "confidence", "notes"}) ||
                    !RequireString(v, p, "id", pk.id) || !OptionalString(v, p, "commander", pk.commander) ||
                    !OptionalInt(v, p, "size", pk.size) || !OptionalInt(v, p, "lost_before", pk.lost_before) ||
                    !OptionalNumber(v, p, "turn_back_share", pk.turn_back_share)) {
                    return false;
                }
                if (pk.size < 0 || pk.lost_before < 0) return Fail(p, "size and lost_before must be >= 0");
                if (pk.turn_back_share <= 0.0 || pk.turn_back_share > 1.0) return Fail(Join(p, "turn_back_share"), "between 0 and 1");
                for (const ScenarioPackage& o : out.packages) {
                    if (o.id == pk.id) return Fail(Join(p, "id"), "duplicate package '" + pk.id + "'");
                }
                out.packages.push_back(std::move(pk));
            }
        } else if (!message_.empty()) {
            return false;
        }
        for (std::size_t i = 0; i < out.flights.size(); ++i) {
            const ScenarioFlight& f = out.flights[i];
            if (f.package.empty()) continue;
            bool known = false;
            for (const ScenarioPackage& pk : out.packages) known = known || pk.id == f.package;
            if (!known) return Fail(Join(Index("flights", i), "package"), "no package '" + f.package + "' in \"packages\"");
        }
        for (std::size_t i = 0; i < out.packages.size(); ++i) {
            const ScenarioPackage& pk = out.packages[i];
            if (pk.commander.empty()) continue;
            bool ok = false;
            for (const ScenarioFlight& f : out.flights) ok = ok || (f.id == pk.commander && f.package == pk.id);
            if (!ok) return Fail(Join(Index("packages", i), "commander"), "'" + pk.commander + "' is not a flight of this package");
        }
        return true;
    }
    // "air_radio" (roadmap B9): the enemy's flights' calls on the air and our intercept posts, from a library of sets.
    bool LoadAirRadio(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* v = root.Find("air_radio");
        if (v == nullptr) return true;
        const std::string p = "air_radio";
        if (!v->Is(json::Type::Object)) return Fail(p, "expected an object");
        AirRadioDef& a = out.air_radio;
        std::string library;
        double freq_mhz = a.freq_hz / 1e6, channel_khz = a.channel_hz / 1e3, our_mhz = a.our_freq_hz / 1e6;
        if (!Keys(*v, p, {"faction", "library", "radio", "ground_radio", "freq_mhz", "channel_khz", "language", "discipline", "callsign_rotation_h",
                          "have_quick_share", "secure_voice_share", "opsec", "tune", "posts", "our_radio", "our_freq_mhz", "source", "confidence",
                          "notes"}) ||
            !OptionalString(*v, p, "faction", a.faction) || !RequireString(*v, p, "library", library) || !RequireString(*v, p, "radio", a.air_radio) ||
            !OptionalString(*v, p, "our_radio", a.our_radio) || !OptionalNumber(*v, p, "our_freq_mhz", our_mhz) ||
            !OptionalString(*v, p, "ground_radio", a.ground_radio) || !OptionalNumber(*v, p, "freq_mhz", freq_mhz) ||
            !OptionalNumber(*v, p, "channel_khz", channel_khz) || !OptionalString(*v, p, "language", a.language) ||
            !OptionalNumber(*v, p, "discipline", a.discipline) || !OptionalNumber(*v, p, "callsign_rotation_h", a.callsign_rotation_h) ||
            !OptionalNumber(*v, p, "have_quick_share", a.have_quick_share) || !OptionalNumber(*v, p, "secure_voice_share", a.secure_voice_share) ||
            !OptionalNumber(*v, p, "opsec", a.opsec) || !Provenance(*v, p)) {
            return false;
        }
        a.freq_hz = freq_mhz * 1e6;
        a.channel_hz = channel_khz * 1e3;
        a.our_freq_hz = our_mhz * 1e6;
        if (a.discipline < 0.0 || a.discipline > 1.0 || a.opsec < 0.0 || a.opsec > 1.0) return Fail(p, "discipline and opsec are between 0 and 1");
        if (const json::Value* t = v->Find("tune")) {
            const std::string tp = Join(p, "tune");
            if (!t->Is(json::Type::Object)) return Fail(tp, "expected an object of numbers");
            for (std::size_t k = 0; k < t->keys.size() && k < t->items.size(); ++k) {
                const std::string key = "radio." + t->keys[k];
                if (AirRadioTuningDefaults().count(key) == 0) return Fail(Join(tp, t->keys[k]), "unknown tuning value");
                if (!t->items[k].Is(json::Type::Number)) return Fail(Join(tp, t->keys[k]), "expected a number");
                a.tune[key] = t->items[k].number;
            }
        }
        if (const json::Value* arr = OptionalArray(*v, p, "posts")) {
            for (std::size_t i = 0; i < arr->items.size(); ++i) {
                const json::Value& pv = arr->items[i];
                const std::string pp = Index(Join(p, "posts"), i);
                AirRadioPostDef d;
                double e = 0.0, n = 0.0, freq = 0.0;
                if (!Keys(pv, pp, {"id", "name", "kind", "east_m", "north_m", "mast_m", "esm", "radio", "freq_mhz", "channels", "chain_post", "corridor",
                                   "from_s", "until_s", "period_s", "notes"}) ||
                    !RequireString(pv, pp, "id", d.id) || !OptionalString(pv, pp, "name", d.name) || !OptionalString(pv, pp, "kind", d.kind) ||
                    !OptionalNumber(pv, pp, "east_m", e) || !OptionalNumber(pv, pp, "north_m", n) || !OptionalNumber(pv, pp, "mast_m", d.mast_m) ||
                    !OptionalString(pv, pp, "esm", d.esm) || !OptionalString(pv, pp, "radio", d.radio) || !OptionalNumber(pv, pp, "freq_mhz", freq) ||
                    !OptionalInt(pv, pp, "channels", d.channels) || !OptionalString(pv, pp, "chain_post", d.chain_post) ||
                    !OptionalString(pv, pp, "corridor", d.corridor) || !OptionalNumber(pv, pp, "from_s", d.from_s) ||
                    !OptionalNumber(pv, pp, "until_s", d.until_s) || !OptionalNumber(pv, pp, "period_s", d.period_s)) {
                    return false;
                }
                if (d.kind != "intercept" && d.kind != "gci" && d.kind != "crc" && d.kind != "fac" && d.kind != "awacs" && d.kind != "our_gci" &&
                    d.kind != "our_dummy" && d.kind != "enemy_intercept") {
                    return Fail(Join(pp, "kind"), "intercept, gci, crc, fac, awacs, our_gci, our_dummy or enemy_intercept");
                }
                if ((d.kind == "our_gci" || d.kind == "our_dummy") && d.radio.empty() && a.our_radio.empty()) return Fail(pp, "our side speaks on our_radio: give it");
                if ((d.kind == "intercept" || d.kind == "enemy_intercept") && d.esm.empty()) return Fail(pp, "a listening post needs its receiver (the esm key)");
                if (d.kind != "our_dummy" && (!d.corridor.empty() || pv.Find("from_s") != nullptr || pv.Find("until_s") != nullptr || pv.Find("period_s") != nullptr)) {
                    return Fail(pp, "corridor, from_s, until_s and period_s are a dummy transmitter's (kind our_dummy)");
                }
                if (d.kind == "our_dummy" && (d.until_s < d.from_s || d.period_s < 0.0)) return Fail(pp, "a dummy transmitter talks from from_s until until_s, every period_s >= 0");
                d.at = Vec3{e, n, 0.0};
                d.freq_hz = freq * 1e6;
                a.posts.push_back(std::move(d));
            }
        }
        const em::EmLoadResult res = em::LoadEmLibraryFile(data_dir + "/" + library, a.library);
        if (!res.ok) return Fail(Join(p, "library"), res.error);
        a.library_file = library;
        a.on = true;
        return true;
    }
    // "air_fight" (roadmap B8): the targets that fight in the air, for which side, under which controller, and the data
    // they fight by (air_war/air_fight_def.hpp).
    bool LoadAirFight(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* v = root.Find("air_fight");
        if (v == nullptr) return true;
        const std::string p = "air_fight";
        if (!v->Is(json::Type::Object)) return Fail(p, "expected an object");
        airwar::AirFightDef& a = out.air_fight;
        std::string combat = "air_war/air_combat.json", sensors = "air_war/sensors.json";
        if (!Keys(*v, p, {"year", "combat", "sensors", "fighters", "controllers", "mode", "far_fight", "hot_mode", "teaball", "source", "confidence", "notes"}) ||
            !OptionalInt(*v, p, "year", a.year) || !OptionalString(*v, p, "combat", combat) || !OptionalString(*v, p, "sensors", sensors) ||
            !OptionalInt(*v, p, "mode", a.mode) || !OptionalInt(*v, p, "far_fight", a.far_fight) ||
            !OptionalInt(*v, p, "hot_mode", a.hot_mode) || !Provenance(*v, p)) {
            return false;
        }
        if (a.mode < 0 || a.mode > 1) return Fail(Join(p, "mode"), "0 (B8's fight) or 1 (roadmap S3's)");
        if (a.far_fight < -1 || a.far_fight > 1) return Fail(Join(p, "far_fight"), "-1 (with the mode), 0 or 1");
        std::string e;
        if (!airwar::LoadAirCombatFile(data_dir + "/" + combat, a.combat, &e)) return Fail(Join(p, "combat"), e);
        if (!airwar::LoadSensorsFile(data_dir + "/" + sensors, a.sensors, &e)) return Fail(Join(p, "sensors"), e);
        if (!airwar::FillLookDown(a.sensors, data_dir, &e)) return Fail(Join(p, "sensors"), e);
        {  // the radars as emitters (S3): every library a sensor names, once
            std::vector<std::string> libs;
            for (const airwar::SensorDef& sd : a.sensors.sensors) {
                if (sd.library.empty() || std::find(libs.begin(), libs.end(), sd.library) != libs.end()) continue;
                libs.push_back(sd.library);
                const em::EmLoadResult lr = em::LoadEmLibraryFile(data_dir + "/" + sd.library, a.radar_library);
                if (!lr.ok) return Fail(Join(p, "sensors"), sd.library + ": " + lr.error);
            }
        }
        if (const json::Value* arr = OptionalArray(*v, p, "controllers")) {
            for (std::size_t i = 0; i < arr->items.size(); ++i) {
                const json::Value& cv = arr->items[i];
                const std::string cp = Index(Join(p, "controllers"), i);
                airwar::AirFightControllerDef c;
                double east = 0.0, north = 0.0, alt = 0.0;
                if (!Keys(cv, cp, {"id", "side", "kind", "east_m", "north_m", "alt_m", "range_m", "post", "notes"}) || !RequireString(cv, cp, "id", c.id) ||
                    !OptionalString(cv, cp, "side", c.side) || !OptionalString(cv, cp, "kind", c.kind) || !OptionalNumber(cv, cp, "east_m", east) ||
                    !OptionalNumber(cv, cp, "north_m", north) || !OptionalNumber(cv, cp, "alt_m", alt) || !OptionalNumber(cv, cp, "range_m", c.range_m) ||
                    !OptionalString(cv, cp, "post", c.post)) {
                    return false;
                }
                if (c.side != "ours" && c.side != "enemy") return Fail(Join(cp, "side"), "ours or enemy");
                if (c.kind != "radar_control" && c.kind != "airborne_warning") return Fail(Join(cp, "kind"), "radar_control or airborne_warning");
                if (!c.post.empty()) {
                    bool post = false;
                    for (const AirRadioPostDef& rp : out.air_radio.posts) post = post || rp.id == c.post;
                    if (!post) return Fail(Join(cp, "post"), "no air_radio post '" + c.post + "'");
                }
                c.pos = Vec3{east, north, alt};
                a.controllers.push_back(std::move(c));
            }
        }
        if (const json::Value* tb = v->Find("teaball")) {  // roadmap S6: the enemy's relay of our GCI's vectors
            const std::string tp = Join(p, "teaball");
            airwar::AirFightDef::Teaball& t = a.teaball;
            if (!tb->Is(json::Type::Object) || !Keys(*tb, tp, {"relay", "delay_s", "fail", "error_m", "our_mast_m", "source", "confidence", "notes"}) ||
                !RequireString(*tb, tp, "relay", t.relay) || !OptionalNumber(*tb, tp, "delay_s", t.delay_s) || !OptionalNumber(*tb, tp, "fail", t.fail) ||
                !OptionalNumber(*tb, tp, "error_m", t.error_m) || !OptionalNumber(*tb, tp, "our_mast_m", t.our_mast_m)) {
                return tb->Is(json::Type::Object) ? false : Fail(tp, "expected an object");
            }
            const airwar::AirFightControllerDef* rc = a.Controller(t.relay);
            if (rc == nullptr || rc->side != "enemy") return Fail(Join(tp, "relay"), "no enemy controller '" + t.relay + "'");
            if (t.fail < 0.0 || t.fail > 1.0) return Fail(Join(tp, "fail"), "a chance 0 to 1");
            if (t.delay_s < 0.0 || t.error_m < 0.0 || t.our_mast_m < 0.0) return Fail(tp, "delay_s, error_m and our_mast_m are not negative");
        }
        const json::Value* arr = OptionalArray(*v, p, "fighters");
        bool our_gci = false;  // our GCI with our regiments' pairs to come (B10: scrambled by "own_air")
        for (const airwar::AirFightControllerDef& c : a.controllers) our_gci = our_gci || c.side == "ours";
        if ((arr == nullptr || arr->items.empty()) && !(our_gci && root.Find("own_air") != nullptr)) {
            return Fail(Join(p, "fighters"), "required: the targets that fight (or our GCI with \"own_air\"'s regiments)");
        }
        if (arr == nullptr) {
            a.present = true;
            return true;
        }

        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& fv = arr->items[i];
            const std::string fp = Index(Join(p, "fighters"), i);
            airwar::AirFightFighterDef f;
            if (!Keys(fv, fp, {"target", "type", "faction", "role", "regiment", "controller", "skill", "escorts", "turn_back_lost", "unwarned_break", "notes"}) ||
                !RequireString(fv, fp, "target", f.target) || !RequireString(fv, fp, "type", f.type) || !RequireString(fv, fp, "faction", f.faction) ||
                !OptionalString(fv, fp, "role", f.role) || !OptionalString(fv, fp, "regiment", f.regiment) ||
                !OptionalString(fv, fp, "controller", f.controller) || !OptionalNumber(fv, fp, "skill", f.skill) ||
                !OptionalString(fv, fp, "escorts", f.escorts) ||
                !OptionalNumber(fv, fp, "turn_back_lost", f.turn_back_lost) || !OptionalNumber(fv, fp, "unwarned_break", f.unwarned_break)) {
                return false;
            }
            if (f.turn_back_lost > 1.0) return Fail(Join(fp, "turn_back_lost"), "a chance 0 to 1 (absent: none)");
            if (f.unwarned_break < 0.0 || f.unwarned_break > 1.0) return Fail(Join(fp, "unwarned_break"), "0 to 1");
            if (!f.escorts.empty()) {
                bool named = false;
                for (const ScenarioTarget& t : out.targets) named = named || t.id == f.escorts;
                if (!named) return Fail(Join(fp, "escorts"), "no target '" + f.escorts + "'");
            }
            bool known = false;
            for (const ScenarioTarget& t : out.targets) known = known || t.id == f.target;
            if (!known) return Fail(Join(fp, "target"), "no target '" + f.target + "'");
            if (a.combat.Doctrine(f.faction) == nullptr) return Fail(Join(fp, "faction"), "no doctrine for '" + f.faction + "' in " + combat);
            if (a.combat.Fit(f.type) == nullptr) return Fail(Join(fp, "type"), "no fit for '" + f.type + "' in " + combat);
            if (!f.controller.empty() && a.Controller(f.controller) == nullptr) return Fail(Join(fp, "controller"), "no controller '" + f.controller + "'");
            if (f.role != "escort" && f.role != "sweep" && f.role != "cap" && f.role != "intercept" && f.role != "strike") {
                return Fail(Join(fp, "role"), "escort, sweep, cap, intercept or strike");
            }
            // Its faction's vector lines (roadmap S3), once: who may vector which of its fighters.
            if (f.faction != "ours" && a.vector_lines.find(f.faction) == a.vector_lines.end()) {
                std::vector<airwar::C2AuthorityDef>& lines = a.vector_lines[f.faction];
                std::ifstream in(data_dir + "/air_war/faction_" + f.faction + ".json", std::ios::binary);
                if (in) {
                    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    if (!airwar::VectorLinesFromJson(text, lines)) return Fail(Join(fp, "faction"), "air_war/faction_" + f.faction + ".json does not parse");
                }
            }
            a.fighters.push_back(std::move(f));
        }
        a.present = true;
        return true;
    }
    // "own_air" (roadmap B10): our aviation's side of the session. The codes and rules files, the day's codes and when
    // the table reached the battery and the post, the plan of cooperation, the notices our fighter regiment sends, its
    // regiments and the GCI's radar. Positions in the scenario's east and north metres; times in session seconds.
    bool LoadOwnAir(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* v = root.Find("own_air");
        if (v == nullptr) return true;
        const std::string p = "own_air";
        if (!v->Is(json::Type::Object)) return Fail(p, "expected an object");
        command::OwnAirScenarioDef& o = out.own_air;
        if (!Keys(*v, p, {"data", "codes", "war", "day", "fixed_code", "battery_table_s", "battery_last_code", "post_table_s", "post_last_code",
                          "chain", "origin", "notice_post", "plan", "notices", "regiments", "gci", "raid_warning_s", "front", "enemy_allies", "source",
                          "confidence", "notes"}) ||
            !OptionalString(*v, p, "data", o.data_file) || !OptionalString(*v, p, "codes", o.codes_file) || !OptionalBool(*v, p, "war", o.war) ||
            !OptionalInt(*v, p, "day", o.day) || !OptionalInt(*v, p, "fixed_code", o.fixed_code) ||
            !OptionalNumber(*v, p, "battery_table_s", o.battery_table_s) || !OptionalInt(*v, p, "battery_last_code", o.battery_last_code) ||
            !OptionalNumber(*v, p, "post_table_s", o.post_table_s) || !OptionalInt(*v, p, "post_last_code", o.post_last_code) ||
            !OptionalString(*v, p, "chain", o.chain_file) || !OptionalString(*v, p, "origin", o.origin) ||
            !OptionalString(*v, p, "notice_post", o.notice_post) || !OptionalNumber(*v, p, "raid_warning_s", o.raid_warning_s) ||
            !OptionalBool(*v, p, "enemy_allies", o.enemy_allies) || !Provenance(*v, p)) {
            return false;
        }
        std::string e;
        if (!command::LoadOwnAirFile(data_dir + "/" + o.data_file, o.def, &e)) return Fail(Join(p, "data"), e);
        if (!command::LoadIffCodesFile(data_dir + "/" + o.codes_file, o.codes, &e)) return Fail(Join(p, "codes"), e);
        if (o.fixed_code < 0 || o.fixed_code > o.codes.filters) return Fail(Join(p, "fixed_code"), "0 (the day's table) or a code 1-" + std::to_string(o.codes.filters));
        if (!o.chain_file.empty()) {
            if (!command::LoadChainData(data_dir, o.chain_file, o.chain, &e)) return Fail(Join(p, "chain"), e);
            if (o.chain.PositionIndex(o.origin) < 0) return Fail(Join(p, "origin"), "no position '" + o.origin + "' in the chain");
            if (o.chain.PositionIndex(o.notice_post) < 0) return Fail(Join(p, "notice_post"), "no position '" + o.notice_post + "' in the chain");
            o.has_chain = true;
        }
        if (const json::Value* pl = v->Find("plan")) {
            if (!command::LoadCooperationPlan(*pl, Join(p, "plan"), o.plan, &e)) return Fail(Join(p, "plan"), e);
        } else if (out.has_task && out.task.own_air.present) {
            o.plan = out.task.own_air;  // the plan of cooperation the regiment's task gave the battery (S1)
        }
        if (const json::Value* fr = v->Find("front")) {  // S1: the front line (airborne alert, the pursuit limit)
            const std::string fp = Join(p, "front");
            double fe = 0.0, fn = 0.0, bearing = 0.0;
            if (!Keys(*fr, fp, {"east_m", "north_m", "enemy_bearing_deg", "notes"}) || !RequireNumber(*fr, fp, "east_m", fe) ||
                !RequireNumber(*fr, fp, "north_m", fn) || !RequireNumber(*fr, fp, "enemy_bearing_deg", bearing)) {
                return false;
            }
            o.has_front = true;
            o.front = Vec3{fe, fn, 0.0};
            o.to_enemy = Vec3{std::sin(DegToRad(bearing)), std::cos(DegToRad(bearing)), 0.0};
        }
        const double clock0_h = out.clock_start_s >= 0.0 ? out.clock_start_s / 3600.0 : 0.0;
        const auto read_path = [&](const json::Value& n, const std::string& np, std::vector<Vec3>& path) {
            const json::Value* r = n.Find("route_m");
            if (r == nullptr || !r->Is(json::Type::Array) || r->items.empty()) return Fail(Join(np, "route_m"), "a list of [east_m, north_m]");
            for (std::size_t j = 0; j < r->items.size(); ++j) {
                const json::Value& q = r->items[j];
                if (!q.Is(json::Type::Array) || q.items.size() != 2 || !q.items[0].Is(json::Type::Number) || !q.items[1].Is(json::Type::Number)) {
                    return Fail(Index(Join(np, "route_m"), j), "expected [east_m, north_m]");
                }
                path.push_back(Vec3{q.items[0].number, q.items[1].number, 0.0});
            }
            return true;
        };
        if (const json::Value* arr = OptionalArray(*v, p, "notices")) {
            for (std::size_t i = 0; i < arr->items.size(); ++i) {
                const json::Value& n = arr->items[i];
                const std::string np = Index(Join(p, "notices"), i);
                command::OwnAirScenarioNotice sn;
                command::OwnFlightNotice& f = sn.notice;
                double from_s = 0.0, to_s = 0.0, back_s = -1.0;
                f.number = static_cast<int>(i) + 1;
                if (!Keys(n, np, {"number", "amendment", "actual", "flight", "type", "count", "route_m", "half_width_m", "floor_m", "ceiling_m",
                                  "cross_from_s", "cross_to_s", "back_s", "sent_s", "lost", "aircraft", "notes"}) ||
                    !OptionalInt(n, np, "number", f.number) || !OptionalInt(n, np, "amendment", f.amendment) || !OptionalBool(n, np, "actual", f.actual) ||
                    !RequireString(n, np, "flight", f.flight) || !OptionalString(n, np, "type", f.type) || !OptionalInt(n, np, "count", f.count) ||
                    !OptionalNumber(n, np, "half_width_m", f.half_width_m) || !RequireNumber(n, np, "floor_m", f.floor_m) ||
                    !RequireNumber(n, np, "ceiling_m", f.ceiling_m) || !RequireNumber(n, np, "cross_from_s", from_s) ||
                    !RequireNumber(n, np, "cross_to_s", to_s) || !OptionalNumber(n, np, "back_s", back_s) || !OptionalNumber(n, np, "sent_s", sn.sent_s) ||
                    !OptionalBool(n, np, "lost", sn.lost) || !read_path(n, np, f.route)) {
                    return false;
                }
                if (f.ceiling_m <= f.floor_m) return Fail(Join(np, "ceiling_m"), "must be above floor_m");
                if (to_s < from_s) return Fail(Join(np, "cross_to_s"), "must be after cross_from_s");
                f.cross_from_h = clock0_h + from_s / 3600.0;
                f.cross_to_h = clock0_h + to_s / 3600.0;
                f.back_h = back_s >= 0.0 ? clock0_h + back_s / 3600.0 : -1.0;
                if (const json::Value* ac = OptionalArray(n, np, "aircraft")) {
                    for (std::size_t k = 0; k < ac->items.size(); ++k) {
                        if (!ac->items[k].Is(json::Type::String)) return Fail(Index(Join(np, "aircraft"), k), "a target's id");
                        bool known = false;
                        for (const ScenarioTarget& t : out.targets) known = known || t.id == ac->items[k].string;
                        if (!known) return Fail(Index(Join(np, "aircraft"), k), "no target '" + ac->items[k].string + "'");
                        sn.aircraft.push_back(ac->items[k].string);
                    }
                }
                o.notices.push_back(std::move(sn));
            }
        }
        if (const json::Value* arr = OptionalArray(*v, p, "regiments")) {
            for (std::size_t i = 0; i < arr->items.size(); ++i) {
                const json::Value& r = arr->items[i];
                const std::string rp = Index(Join(p, "regiments"), i);
                command::FighterRegimentDef d;
                double be = 0.0, bn = 0.0, ae = 0.0, an = 0.0;
                if (!Keys(r, rp, {"id", "base", "base_east_m", "base_north_m", "type", "name", "skill", "aircraft", "function", "area_east_m",
                                  "area_north_m", "area_radius_m", "gci", "notes"}) ||
                    !RequireString(r, rp, "id", d.id) || !OptionalString(r, rp, "base", d.base) || !RequireNumber(r, rp, "base_east_m", be) ||
                    !RequireNumber(r, rp, "base_north_m", bn) || !OptionalString(r, rp, "type", d.type) || !OptionalString(r, rp, "name", d.name) ||
                    !OptionalNumber(r, rp, "skill", d.skill) || !OptionalInt(r, rp, "aircraft", d.aircraft) || !OptionalNumber(r, rp, "function", d.function) ||
                    !RequireNumber(r, rp, "area_east_m", ae) || !RequireNumber(r, rp, "area_north_m", an) ||
                    !OptionalNumber(r, rp, "area_radius_m", d.area_radius_m) || !RequireString(r, rp, "gci", d.gci)) {
                    return false;
                }
                if (!out.air_fight.present || out.air_fight.Controller(d.gci) == nullptr || out.air_fight.Controller(d.gci)->side != "ours") {
                    return Fail(Join(rp, "gci"), "one of our controllers in \"air_fight\"");
                }
                if (out.air_fight.combat.Fit(d.type) == nullptr) return Fail(Join(rp, "type"), "no fit for '" + d.type + "' in the air fight's combat data");
                d.base_pos = Vec3{be, bn, 0.0};
                d.area_centre = Vec3{ae, an, 0.0};
                o.regiments.push_back(std::move(d));
            }
        }
        if (const json::Value* g = v->Find("gci")) {
            double ge = 0.0, gn = 0.0;
            if (!Keys(*g, Join(p, "gci"), {"east_m", "north_m", "mast_m", "notes"}) || !RequireNumber(*g, Join(p, "gci"), "east_m", ge) ||
                !RequireNumber(*g, Join(p, "gci"), "north_m", gn) || !OptionalNumber(*g, Join(p, "gci"), "mast_m", o.gci_mast_m)) {
                return false;
            }
            o.gci_radar = Vec3{ge, gn, 0.0};
        } else if (!o.regiments.empty()) {
            return Fail(Join(p, "gci"), "required with regiments: our GCI's radar");
        }
        o.present = true;
        return true;
    }
    // A formation's slots: [[right_m, back_m, up_m], ...], the leader's place first.

    bool LoadSlots(const json::Value& v, const std::string& p, std::vector<Vec3>& out) {
        if (!v.Is(json::Type::Array) || v.items.empty()) return Fail(p, "a list of [right_m, back_m, up_m], the leader's first");
        for (std::size_t j = 0; j < v.items.size(); ++j) {
            const json::Value& s = v.items[j];
            if (!s.Is(json::Type::Array) || s.items.size() != 3 || !s.items[0].Is(json::Type::Number) ||
                !s.items[1].Is(json::Type::Number) || !s.items[2].Is(json::Type::Number)) {
                return Fail(Index(p, j), "expected [right_m, back_m, up_m]");
            }
            out.push_back(Vec3{s.items[0].number, s.items[1].number, s.items[2].number});
        }
        return true;
    }
    // A target's "flight", "slot" and "legs".
    bool LoadTargetLegs(const json::Value& t, const std::string& p, ScenarioTarget& tg, bool& any_agl) {
        if (!OptionalString(t, p, "flight", tg.flight) || !OptionalInt(t, p, "slot", tg.slot)) return false;
        if (t.Find("slot") != nullptr && (tg.slot < 1 || tg.slot > 16)) return Fail(Join(p, "slot"), "its number in the flight, 1 to 16");
        if (t.Find("slot") != nullptr && tg.flight.empty()) return Fail(Join(p, "slot"), "needs a flight");
        const json::Value* legs = OptionalArray(t, p, "legs");
        if (legs == nullptr) return message_.empty();
        if (legs->items.size() != tg.waypoints.size()) return Fail(Join(p, "legs"), "one per waypoint (the leg flown to it)");
        for (std::size_t j = 0; j < legs->items.size(); ++j) {
            const json::Value& l = legs->items[j];
            const std::string lp = Index(Join(p, "legs"), j);
            RouteLeg leg;
            leg.to = tg.waypoints[j];
            if (!Keys(l, lp, {"speed_mps", "agl_m", "alt_m", "tag", "notes"}) || !OptionalNumber(l, lp, "speed_mps", leg.speed_mps) ||
                !OptionalNumber(l, lp, "agl_m", leg.agl_m) || !OptionalNumber(l, lp, "alt_m", leg.alt_m) ||
                !OptionalString(l, lp, "tag", leg.tag)) {
                return false;
            }
            if (leg.speed_mps < 0.0) return Fail(Join(lp, "speed_mps"), "must be > 0 (or left out)");
            if (l.Find("agl_m") != nullptr && l.Find("alt_m") != nullptr) return Fail(lp, "agl_m or alt_m, not both");
            if (l.Find("agl_m") != nullptr) {
                if (leg.agl_m < 0.0) return Fail(Join(lp, "agl_m"), "must be >= 0");
                tg.behaviour.waypoint_agl_m[j] = leg.agl_m;
                any_agl = true;
            }
            if (l.Find("alt_m") != nullptr) {
                leg.has_alt = true;
                tg.waypoints[j].z = GroundPointInFrame(frame_, tg.waypoints[j].x, tg.waypoints[j].y, leg.alt_m).z;
                leg.to = tg.waypoints[j];
            }
            tg.legs.push_back(leg);
        }
        return true;
    }
    // Every flight a target names is in "flights", and its slots are its own.
    bool CheckFlights(ScenarioDef& out) {
        for (std::size_t i = 0; i < out.targets.size(); ++i) {
            const ScenarioTarget& t = out.targets[i];
            if (t.flight.empty()) continue;
            bool known = false;
            for (const ScenarioFlight& f : out.flights) known = known || f.id == t.flight;
            if (!known) return Fail(Join(Index("targets", i), "flight"), "no flight '" + t.flight + "' in \"flights\"");
            for (std::size_t j = 0; j < i; ++j) {
                if (t.slot > 0 && out.targets[j].flight == t.flight && out.targets[j].slot == t.slot) {
                    return Fail(Join(Index("targets", i), "slot"), "slot " + std::to_string(t.slot) + " of " + t.flight + " is taken");
                }
            }
        }
        return true;
    }

    bool LoadTargets(const json::Value& root, ScenarioDef& out) {
        if (const json::Value* nbs = OptionalArray(root, "", "neighbours")) {
            for (std::size_t i = 0; i < nbs->items.size(); ++i) {
                const json::Value& v = nbs->items[i];
                const std::string p = Index("neighbours", i);
                ScenarioNeighbour nb;
                const json::Value* at = nullptr;
                double radius_km = 0.0;
                double launchers = 4.0, missiles = -1.0;
                if (!Keys(v, p, {"id", "name", "system", "position", "radius_km", "fights", "launchers", "missiles", "notes"}) ||
                    !RequireString(v, p, "id", nb.id) || !RequireString(v, p, "name", nb.name) ||
                    !RequireString(v, p, "system", nb.system) || !RequireObject(v, p, "position", at) ||
                    !LoadPosition(*at, Join(p, "position"), frame_, nb.position) || !RequireNumber(v, p, "radius_km", radius_km) ||
                    !OptionalBool(v, p, "fights", nb.fights) || !OptionalNumber(v, p, "launchers", launchers) ||
                    !OptionalNumber(v, p, "missiles", missiles)) {
                    return false;
                }
                if (radius_km <= 0.0) return Fail(Join(p, "radius_km"), "must be > 0");
                nb.radius_m = radius_km * 1000.0;
                if (nb.fights && nb.system != "osa") return Fail(Join(p, "fights"), "only an Osa battery fights in the battery fire model");
                if (launchers < 1.0 || launchers > 4.0) return Fail(Join(p, "launchers"), "between 1 and 4");
                if (missiles < -1.0 || missiles > 24.0) return Fail(Join(p, "missiles"), "between 0 and 24 (-1: six a launcher)");
                nb.launchers = static_cast<int>(launchers);
                nb.missiles = static_cast<int>(missiles);
                out.neighbours.push_back(nb);
            }
        } else if (!message_.empty()) {
            return false;
        }
        const json::Value* arr = OptionalArray(root, "", "targets");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("targets", i);
            const json::Value& t = arr->items[i];
            if (!Keys(t, p, {"id", "name", "type", "class", "start", "waypoints", "speed_mps", "max_turn_rate_deg_s",
                             "rcs_m2", "appear_at_s", "evade_on_launch", "evade_turn_rate_deg_s", "iff", "board_type",
                             "formation", "behaviour", "evade", "size_m", "rwr", "decoy", "model", "on_cue", "arms", "span_m", "weapon",
                             "bottom_m", "max_load_g", "sustained_turn_deg_s", "roll_rate_deg_s", "max_mps", "accel_mps2",
                             "climb_mps", "flight", "slot", "legs", "seats", "pilot_skill", "reactions", "iff_code", "iff_table", "notes"})) {

                return false;
            }
            ScenarioTarget tg;
            double turn_deg_s = RadToDeg(tg.max_turn_rate_rad_s);
            double evade_deg_s = RadToDeg(tg.evade_turn_rate_rad_s);
            if (!OptionalBool(t, p, "on_cue", tg.on_cue) || !OptionalString(t, p, "class", tg.target_class) ||
                !OptionalBool(t, p, "evade_on_launch", tg.evade_on_launch) || !LoadIff(t, p, tg.friendly) ||
                !OptionalNumber(t, p, "evade_turn_rate_deg_s", evade_deg_s)) {
                return false;
            }
            if (evade_deg_s <= 0.0) return Fail(Join(p, "evade_turn_rate_deg_s"), "must be > 0");
            if (!OptionalInt(t, p, "board_type", tg.board_type) || !OptionalInt(t, p, "formation", tg.formation) ||
                !OptionalNumber(t, p, "span_m", tg.span_m) || !OptionalInt(t, p, "iff_code", tg.iff_code) ||
                !OptionalBool(t, p, "iff_table", tg.iff_table)) {
                return false;
            }
            if (tg.iff_code < -1) return Fail(Join(p, "iff_code"), "-1 (off), 0 (the default) or a code 1-12");
            if ((tg.iff_code != 0 || tg.iff_table) && !tg.friendly) return Fail(Join(p, "iff_code"), "only for one of ours (\"iff\": \"friend\")");

            if (tg.span_m < 0.0) return Fail(Join(p, "span_m"), "must be >= 0");
            if (t.Find("board_type") != nullptr && (tg.board_type < 0 || tg.board_type > 9)) {
                return Fail(Join(p, "board_type"), "a digit 0-9 (0 jamming, 1 friend, 2 training target, 8 enemy, 9 no IFF...)");
            }
            if (tg.formation < 1) return Fail(Join(p, "formation"), "must be >= 1");
            if (!OptionalNumber(t, p, "size_m", tg.size_m) || !OptionalString(t, p, "rwr", tg.rwr) ||
                !OptionalBool(t, p, "decoy", tg.decoy) || !OptionalString(t, p, "model", tg.model) ||
                !OptionalBool(t, p, "weapon", tg.weapon) || !OptionalNumber(t, p, "bottom_m", tg.bottom_m)) {
                return false;
            }
            if (tg.size_m <= 0.0) return Fail(Join(p, "size_m"), "must be > 0");
            if (!OptionalInt(t, p, "seats", tg.seats) || !OptionalNumber(t, p, "pilot_skill", tg.pilot_skill)) return false;
            if (tg.seats < 1 || tg.seats > 2) return Fail(Join(p, "seats"), "1 or 2");
            if (t.Find("pilot_skill") != nullptr && (tg.pilot_skill < 0.0 || tg.pilot_skill > 1.0)) return Fail(Join(p, "pilot_skill"), "0 to 1");
            double sustained_deg_s = 0.0, roll_deg_s = 0.0;
            if (!OptionalNumber(t, p, "max_load_g", tg.max_load_g) || !OptionalNumber(t, p, "sustained_turn_deg_s", sustained_deg_s) ||
                !OptionalNumber(t, p, "roll_rate_deg_s", roll_deg_s) || !OptionalNumber(t, p, "max_mps", tg.max_speed_mps) ||
                !OptionalNumber(t, p, "accel_mps2", tg.accel_mps2) || !OptionalNumber(t, p, "climb_mps", tg.max_climb_mps)) {
                return false;
            }
            if (tg.max_load_g != 0.0 && tg.max_load_g <= 1.0) return Fail(Join(p, "max_load_g"), "must be > 1 (or left out)");
            if (sustained_deg_s < 0.0 || roll_deg_s < 0.0 || tg.max_speed_mps < 0.0 || tg.accel_mps2 < 0.0 || tg.max_climb_mps < 0.0) {
                return Fail(p, "the flight limits (sustained_turn_deg_s, roll_rate_deg_s, max_mps, accel_mps2, climb_mps) must be >= 0");
            }
            tg.sustained_turn_rate_rad_s = DegToRad(sustained_deg_s);
            tg.roll_rate_rad_s = DegToRad(roll_deg_s);
            if (!tg.rwr.empty()) {
                if (!out.has_em) return Fail(Join(p, "rwr"), "needs own_unit.em: the warning receiver is an EM receiver");
                const int e = out.em.library.Esm(tg.rwr);
                if (e < 0) return Fail(Join(p, "rwr"), "'" + tg.rwr + "' is not an ESM receiver of the EM libraries");
                if (out.em.library.esm[static_cast<std::size_t>(e)].kind != em::EsmKind::Rwr) {
                    return Fail(Join(p, "rwr"), "'" + tg.rwr + "' is not a radar warning receiver");
                }
            }
            if (!LoadArmCarrier(t, p, out, tg)) return false;
            if (!LoadReactionPilot(t, p, tg)) return false;
            tg.evade_turn_rate_rad_s = DegToRad(evade_deg_s);
            const json::Value* start = nullptr;
            if (!RequireString(t, p, "id", tg.id) || !RequireString(t, p, "name", tg.name) ||
                !OptionalString(t, p, "type", tg.type) || !RequireObject(t, p, "start", start) ||
                !LoadPosition(*start, Join(p, "start"), frame_, tg.start) ||
                !RequireNumber(t, p, "speed_mps", tg.speed_mps) ||
                !OptionalNumber(t, p, "max_turn_rate_deg_s", turn_deg_s) || !OptionalNumber(t, p, "rcs_m2", tg.rcs_m2) ||
                !OptionalNumber(t, p, "appear_at_s", tg.appear_at_s)) {
                return false;
            }
            if (tg.speed_mps <= 0.0) return Fail(Join(p, "speed_mps"), "must be > 0");
            if (turn_deg_s <= 0.0) return Fail(Join(p, "max_turn_rate_deg_s"), "must be > 0");
            if (tg.rcs_m2 <= 0.0) return Fail(Join(p, "rcs_m2"), "must be > 0");
            if (tg.appear_at_s < 0.0) return Fail(Join(p, "appear_at_s"), "must be >= 0");
            tg.max_turn_rate_rad_s = DegToRad(turn_deg_s);
            const json::Value* wps = OptionalArray(t, p, "waypoints");
            if (wps == nullptr || wps->items.empty()) {
                return message_.empty() ? Fail(Join(p, "waypoints"), "needs at least one waypoint") : false;
            }
            bool any_agl = false;
            for (std::size_t j = 0; j < wps->items.size(); ++j) {
                const json::Value& w = wps->items[j];
                const std::string wp_path = Index(Join(p, "waypoints"), j);
                Vec3 wp;
                double agl = -1.0;
                if (w.Find("agl_m") != nullptr) {  // a height above the ground, held by the target's behaviour
                    if (!Keys(w, wp_path, {"east_m", "north_m", "agl_m"}) || !RequireNumber(w, wp_path, "east_m", wp.x) ||
                        !RequireNumber(w, wp_path, "north_m", wp.y) || !RequireNumber(w, wp_path, "agl_m", agl)) {
                        return false;
                    }
                    if (agl < 0.0) return Fail(Join(wp_path, "agl_m"), "must be >= 0");
                    wp.z = tg.start.z;
                    any_agl = true;
                } else if (!LoadPosition(w, wp_path, frame_, wp)) {
                    return false;
                }
                tg.waypoints.push_back(wp);
                tg.behaviour.waypoint_agl_m.push_back(agl);
            }
            if (!LoadTargetBehaviour(t, p, tg)) return false;
            if (!LoadTargetLegs(t, p, tg, any_agl)) return false;
            if (any_agl && tg.behaviour.kind == TargetBehaviourDef::Kind::None) {
                return Fail(Join(p, "waypoints"), "agl_m needs a behaviour that holds the height (terrain_following, pop_up)");
            }
            for (const ScenarioTarget& other : out.targets) {
                if (other.id == tg.id) return Fail(Join(p, "id"), "duplicate target '" + tg.id + "'");
            }
            out.targets.push_back(std::move(tg));
        }
        return true;
    }

    bool LoadAction(const json::Value& a, const std::string& p, const ScenarioDef& def, const ConsoleData& console,
                    TriggerAction& out) {
        if (!Keys(a, p, {"set_input", "value", "order", "queue", "crew", "end", "c2_source", "c2_fail", "c2_restore", "reason",
                         "battery_order", "route", "covered", "axis_deg", "air_threat", "bounding", "roving", "points",
                         "crossing", "width_m", "vehicle", "radiate", "regime", "on_turns", "off_turns", "from", "instructor", "target",
                         "to", "breakdown", "to_place"})) {
            return false;
        }
        const int kinds = (a.Find("set_input") != nullptr) + (a.Find("order") != nullptr) + (a.Find("queue") != nullptr) +
                          (a.Find("end") != nullptr) + (a.Find("c2_source") != nullptr) + (a.Find("c2_fail") != nullptr) +
                          (a.Find("c2_restore") != nullptr) + (a.Find("battery_order") != nullptr) +
                          (a.Find("air_threat") != nullptr) + (a.Find("instructor") != nullptr) + (a.Find("breakdown") != nullptr);
        if (kinds != 1) {
            return Fail(p, "needs exactly one of set_input, order, queue, end, c2_source, c2_fail, c2_restore, battery_order, "
                           "air_threat, instructor or breakdown");
        }
        // {"breakdown": "23", "reason": "..."}: a battery vehicle breaks down where it stands (its commander reports that he
        // cannot carry out the battery's order: command/battery_orders.hpp).
        if (a.Find("breakdown") != nullptr) {
            out.kind = TriggerAction::Kind::Breakdown;
            if (!def.has_battery) return Fail(p, "the scenario has no battery block");
            if (!RequireString(a, p, "breakdown", out.vehicle) || !OptionalString(a, p, "reason", out.reason)) return false;
            bool known = false;
            for (const BatteryVehicleDef& v : def.battery_vehicles) known = known || v.id == out.vehicle;
            if (!known) return Fail(Join(p, "breakdown"), "unknown battery vehicle '" + out.vehicle + "'");
            if (out.reason.empty()) out.reason = "broken down";
            return true;
        }
        // {"instructor": "turn", "target": "t1", "to": 90}: the trainer's instructor gives an order (instructor.hpp).
        if (a.Find("instructor") != nullptr) {
            out.kind = TriggerAction::Kind::Instructor;
            std::string kind;
            if (!RequireString(a, p, "instructor", kind) || !RequireString(a, p, "target", out.instructor.target)) return false;
            if (!InstructorKindFrom(kind, out.instructor.kind)) {
                return Fail(Join(p, "instructor"), "unknown order '" + kind + "' (take, release, turn, height, speed, evade, jam)");
            }
            bool known = false;
            for (const ScenarioTarget& t : def.targets) known = known || t.id == out.instructor.target;
            if (!known) return Fail(Join(p, "target"), "unknown target '" + out.instructor.target + "'");
            if (InstructorOrderHasValue(out.instructor.kind)) return RequireNumber(a, p, "to", out.instructor.value);
            if (a.Find("to") != nullptr) return Fail(Join(p, "to"), "'" + kind + "' takes no value");
            return true;
        }
        if (a.Find("battery_order") != nullptr || a.Find("air_threat") != nullptr) {
            if (!def.has_battery) return Fail(p, "the scenario has no battery block");
            if (a.Find("air_threat") != nullptr) {
                out.kind = TriggerAction::Kind::AirThreat;
                return OptionalBool(a, p, "air_threat", out.air_threat);
            }
            out.kind = TriggerAction::Kind::BatteryOrder;
            std::string kind;
            return RequireString(a, p, "battery_order", kind) && LoadBatteryOrder(a, p, kind, def, out.battery_order);
        }
        for (const char* key : {"c2_source", "c2_fail", "c2_restore"}) {
            if (a.Find(key) == nullptr) continue;
            out.kind = key[3] == 's' ? TriggerAction::Kind::C2Source
                       : key[3] == 'f' ? TriggerAction::Kind::C2Fail
                                       : TriggerAction::Kind::C2Restore;
            if (!RequireString(a, p, key, out.c2_id) || !OptionalString(a, p, "reason", out.reason)) return false;
            if (def.c2_sources.empty()) return Fail(Join(p, key), "the scenario has no command network (command_post or c2)");
            bool found = out.c2_id == "none" && out.kind == TriggerAction::Kind::C2Source;
            for (const C2SourceDef& d : def.c2_sources) found = found || d.id == out.c2_id;
            if (!found) return Fail(Join(p, key), "unknown source '" + out.c2_id + "'");
            return true;
        }
        if (a.Find("set_input") != nullptr) {
            out.kind = TriggerAction::Kind::SetInput;
            if (!RequireString(a, p, "set_input", out.input) || !OptionalBool(a, p, "value", out.value)) return false;
            if (!console.equipment.HasInput(out.input)) return Fail(Join(p, "set_input"), "unknown input '" + out.input + "'");
            return true;
        }
        if (a.Find("end") != nullptr) {
            out.kind = TriggerAction::Kind::End;
            return RequireString(a, p, "end", out.reason);
        }
        const bool queue = a.Find("queue") != nullptr;
        out.kind = queue ? TriggerAction::Kind::Queue : TriggerAction::Kind::Order;
        if (!RequireString(a, p, queue ? "queue" : "order", out.procedure) || !RequireString(a, p, "crew", out.crew)) {
            return false;
        }
        const Procedure* proc = console.FindProcedure(out.procedure);
        if (proc == nullptr) {
            return Fail(Join(p, queue ? "queue" : "order"), "unknown procedure '" + out.procedure + "'");
        }
        for (const ScenarioCrew& c : def.crew) {
            if (c.name != out.crew) continue;
            if (!proc->station.empty() && proc->station != c.role) {
                return Fail(Join(p, "crew"), "'" + out.procedure + "' is done at station '" + proc->station + "', " +
                                                 out.crew + " sits at '" + c.role + "'");
            }
            return true;
        }
        return Fail(Join(p, "crew"), "unknown crew member '" + out.crew + "'");
    }

    // The command network: the PU-12 (when there is a command post) and the other sources the crew can switch to.
    //   "c2": { "library": "command/c2_sources.json", "primary": "pu12", "active": "pu12", "auto_fallback": true,
    //           "fallback_order": ["ewr_north"], "presets": ["pu12","ewr_north"], "select_s": 3, "retune_s": 30, "primary_stable_s": 60, "heard_life_s": 180,
    //           "sources": [ { "id": "ewr_north", "type": "ewr_voice", "position": {...}, "net": "battery_net",
    //                          "link_range_m": 30000, "outages": [ { "from_s": 300, "to_s": 420, "reason": "jammed" } ] },
    //                        { "id": "pu12", "outages": [...] } ] }
    bool LoadC2(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        if (!out.command_post_file.empty()) {
            C2SourceDef pu;
            pu.id = "pu12";
            pu.display_name = out.board.display_name.empty() ? std::string("PU-12M") : out.board.display_name;
            pu.short_name = "PU-12";
            pu.kind = C2Kind::BatteryPost;
            pu.medium = "voice";
            pu.sense = C2Sense::Relayed;
            pu.board = out.board;
            out.c2_sources.push_back(pu);
        }
        const json::Value* c = root.Find("c2");
        if (c == nullptr) return true;
        if (!c->Is(json::Type::Object) ||
            !Keys(*c, "c2", {"library", "primary", "active", "auto_fallback", "fallback_order", "presets", "select_s", "retune_s", "primary_stable_s",
                             "heard_life_s", "sources", "regiment", "notes"})) {
            return message_.empty() ? Fail("c2", "expected an object") : false;
        }
        out.c2_policy.heard_life_s = 180.0;  // with a network the crew forgets stale designations
        std::string library_file;
        if (!OptionalString(*c, "c2", "library", library_file) || !OptionalString(*c, "c2", "primary", out.c2_policy.primary) ||
            !OptionalString(*c, "c2", "active", out.c2_policy.active) ||
            !OptionalBool(*c, "c2", "auto_fallback", out.c2_policy.auto_fallback) ||
            !OptionalNumber(*c, "c2", "select_s", out.c2_policy.select_s) ||
            !OptionalNumber(*c, "c2", "retune_s", out.c2_policy.retune_s) ||
            !OptionalNumber(*c, "c2", "primary_stable_s", out.c2_policy.primary_stable_s) ||
            !OptionalNumber(*c, "c2", "heard_life_s", out.c2_policy.heard_life_s)) {
            return false;
        }
        if (out.c2_policy.select_s < 0.0) return Fail("c2.select_s", "must be >= 0");
        if (out.c2_policy.retune_s < 0.0) return Fail("c2.retune_s", "must be >= 0");
        if (out.c2_policy.primary_stable_s < 0.0) return Fail("c2.primary_stable_s", "must be >= 0");
        if (out.c2_policy.heard_life_s < 0.0) return Fail("c2.heard_life_s", "must be >= 0");
        C2Library lib;
        if (!library_file.empty()) {
            std::string error;
            if (!LoadC2LibraryFile(data_dir + "/" + library_file, lib, &error)) return Fail("c2.library", error);
        }
        if (const json::Value* order = OptionalArray(*c, "c2", "fallback_order")) {
            for (std::size_t i = 0; i < order->items.size(); ++i) {
                if (!order->items[i].Is(json::Type::String)) return Fail(Index("c2.fallback_order", i), "expected a source id");
                out.c2_policy.fallback_order.push_back(order->items[i].string);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (const json::Value* pr = OptionalArray(*c, "c2", "presets")) {
            if (pr->items.size() > 4) return Fail("c2.presets", "the R-123M has four presets");
            for (std::size_t i = 0; i < pr->items.size(); ++i) {
                if (!pr->items[i].Is(json::Type::String)) return Fail(Index("c2.presets", i), "expected a source id");
                out.c2_policy.presets.push_back(pr->items[i].string);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (const json::Value* list = OptionalArray(*c, "c2", "sources")) {
            for (std::size_t i = 0; i < list->items.size(); ++i) {
                const std::string p = Index("c2.sources", i);
                const json::Value& e = list->items[i];
                std::string id;
                std::string type;
                if (!Keys(e, p, {"id", "type", "name", "position", "net", "radio", "frequency_mhz", "link_range_m", "outages", "notes"}) ||
                    !RequireString(e, p, "id", id) || !OptionalString(e, p, "type", type)) {
                    return false;
                }
                C2SourceDef* def = nullptr;
                for (C2SourceDef& d : out.c2_sources) {
                    if (d.id == id) def = &d;
                }
                if (type.empty()) {
                    if (def == nullptr || id != "pu12") {
                        return Fail(Join(p, "type"), "a source other than the PU-12 needs a library \"type\" (and \"c2.library\")");
                    }
                } else {
                    if (def != nullptr) return Fail(Join(p, "id"), "duplicate source '" + id + "'");
                    const C2SourceDef* t = lib.Find(type);
                    if (t == nullptr) return Fail(Join(p, "type"), "unknown source type '" + type + "' (c2.library names the file)");
                    out.c2_sources.push_back(*t);
                    def = &out.c2_sources.back();
                    def->id = id;
                    def->type = type;
                    def->board.id = id;
                }
                std::string name;
                if (!OptionalString(e, p, "name", name) || !OptionalString(e, p, "net", def->net) || !OptionalString(e, p, "radio", def->radio) ||
                    !OptionalNumber(e, p, "link_range_m", def->link_range_m)) {
                    return false;
                }
                if (!name.empty()) def->display_name = name;
                if (def->link_range_m < 0.0) return Fail(Join(p, "link_range_m"), "must be >= 0");
                double mhz = 0.0;
                if (!OptionalNumber(e, p, "frequency_mhz", mhz)) return false;
                if (e.Find("frequency_mhz") != nullptr) {
                    // the R-123M covers 20.00 to 51.50 MHz in 25 kHz steps (r123_refs)
                    const double steps = (mhz - 20.0) / 0.025;
                    if (mhz < 20.0 || mhz > 51.5 || std::fabs(steps - std::round(steps)) > 1e-6) {
                        return Fail(Join(p, "frequency_mhz"), "an R-123M frequency: 20.000 to 51.500 MHz in 25 kHz steps");
                    }
                    def->frequency_hz = mhz * 1e6;
                }
                if (const json::Value* pos = e.Find("position")) {
                    if (!pos->Is(json::Type::Object) || !LoadPosition(*pos, Join(p, "position"), frame_, def->position)) {
                        return message_.empty() ? Fail(Join(p, "position"), "expected an object") : false;
                    }
                    def->has_position = true;
                }
                if (def->sense != C2Sense::Relayed && !def->has_position) {
                    return Fail(Join(p, "position"), "a radar or observation source needs the place of its site");
                }
                if (const json::Value* outs = OptionalArray(e, p, "outages")) {
                    for (std::size_t j = 0; j < outs->items.size(); ++j) {
                        const std::string op = Index(Join(p, "outages"), j);
                        C2Outage o;
                        if (!Keys(outs->items[j], op, {"from_s", "to_s", "reason"}) || !RequireNumber(outs->items[j], op, "from_s", o.from_s) ||
                            !OptionalNumber(outs->items[j], op, "to_s", o.to_s) || !OptionalString(outs->items[j], op, "reason", o.reason)) {
                            return false;
                        }
                        if (o.to_s <= o.from_s) return Fail(Join(op, "to_s"), "must be after from_s");
                        def->outages.push_back(o);
                    }
                } else if (!message_.empty()) {
                    return false;
                }
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (!LoadRegiment(*c, data_dir, out)) return false;
        if (const json::Value* presets = OptionalArray(*c, "c2", "presets")) {
            if (presets->items.size() > 4) return Fail("c2.presets", "the R-123M has four presets");
            for (std::size_t i = 0; i < presets->items.size(); ++i) {
                if (!presets->items[i].Is(json::Type::String)) return Fail(Index("c2.presets", i), "expected a source id or \"\"");
                out.c2_policy.presets.push_back(presets->items[i].string);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (out.c2_sources.empty()) return Fail("c2", "needs sources (or a command_post for the PU-12)");
        auto known = [&](const std::string& id) {
            for (const C2SourceDef& d : out.c2_sources) {
                if (d.id == id) return true;
            }
            return false;
        };
        if (!known(out.c2_policy.primary)) {
            if (out.c2_policy.primary != "pu12") return Fail("c2.primary", "unknown source '" + out.c2_policy.primary + "'");
            out.c2_policy.primary = out.c2_sources.front().id;  // no PU-12: the first source is the main one
        }
        if (!out.c2_policy.active.empty() && !known(out.c2_policy.active)) {
            return Fail("c2.active", "unknown source '" + out.c2_policy.active + "'");
        }
        for (const std::string& id : out.c2_policy.fallback_order) {
            if (!known(id)) return Fail("c2.fallback_order", "unknown source '" + id + "'");
        }
        for (const std::string& id : out.c2_policy.presets) {
            if (!known(id)) return Fail("c2.presets", "unknown source '" + id + "'");
        }
        return true;
    }

    // The player's vehicle is the command post: "post": { "board": "command/pu12.json", "aspd": "command/aspd_u.json",
    //   "library": "command/regiment_radars.json",
    //   "codes": "command/aspd_codes.json",
    //   "regiment": { "position": {...}, "radars": [ { "id", "type", "position" } ], "outages": [ { "from_s", "to_s", "reason" } ],
    //                 "telecode": true, "designate_within_m": 40000,
    //                 "commands": [ { "at_s", "code" } | { "at_s", "group" } | { "code" or "group", "on_cue": true } ] },
    //   "radar": { "type": "p19", "position": {...} } }  (a radar cabled to the post; optional)
    // Required when the console has "post", refused otherwise.
    bool LoadPost(const json::Value& root, const std::string& data_dir, ScenarioDef& out, const ConsoleData& console) {
        const json::Value* p = root.Find("post");
        if (p == nullptr) {
            if (console.has_post) return Fail("post", "the console is a command post: the scenario needs a \"post\" block");
            return true;
        }
        if (!console.has_post) return Fail("post", "the console is not a command post (no \"post\" block)");
        if (!p->Is(json::Type::Object) ||
            !Keys(*p, "post", {"board", "aspd", "codes", "plotting", "library", "regiment", "radar", "em", "radios", "notes"})) {
            return message_.empty() ? Fail("post", "expected an object") : false;
        }
        std::string board_file, aspd_file, codes_file, plotting_file, library_file, error;
        if (!RequireString(*p, "post", "board", board_file) || !RequireString(*p, "post", "aspd", aspd_file) ||
            !OptionalString(*p, "post", "codes", codes_file) || !OptionalString(*p, "post", "plotting", plotting_file) ||
            !OptionalString(*p, "post", "library", library_file)) {
            return false;
        }
        if (!plotting_file.empty() && !LoadPlottingTuningFile(data_dir + "/" + plotting_file, out.post_plotting, &error)) {
            return Fail("post.plotting", error);
        }
        if (!LoadPlottingBoardFile(data_dir + "/" + board_file, out.post_board, &error)) return Fail("post.board", error);
        if (!campaign::LoadAfterActionValues(data_dir, "gameplay/after_action.json", out.after_action, &error)) return Fail("post", error);
        if (!LoadAspdFile(data_dir + "/" + aspd_file, out.post_aspd, &error)) return Fail("post.aspd", error);
        if (!codes_file.empty() && !LoadAspdCodesFile(data_dir + "/" + codes_file, out.post_codes, &error)) return Fail("post.codes", error);
        RegimentRadarLibrary lib;
        if (!library_file.empty() && !LoadRegimentRadarsFile(data_dir + "/" + library_file, lib, &error)) return Fail("post.library", error);
        out.post_radar_types = lib;
        auto radar_of = [&](const json::Value& e, const std::string& rp, bool with_id, RegimentRadar& radar) {
            std::string id, type;
            // A cabled radar (no id) may also go off the air (its outages); a regiment's radar may not.
            const bool keys = with_id ? Keys(e, rp, {"id", "type", "position", "notes"}) : Keys(e, rp, {"type", "position", "outages", "notes"});
            if (!keys || (with_id && !RequireString(e, rp, "id", id)) ||
                !RequireString(e, rp, "type", type)) {
                return false;
            }
            if (library_file.empty()) return Fail(Join("post", "library"), "a radar needs the library of radar types");
            const RegimentRadar* t = lib.Find(type);
            if (t == nullptr) return Fail(Join(rp, "type"), "unknown radar type '" + type + "'");
            radar = *t;
            radar.id = with_id ? id : type;
            radar.type = type;
            const json::Value* pos = e.Find("position");
            if (pos == nullptr || !pos->Is(json::Type::Object) || !LoadPosition(*pos, Join(rp, "position"), frame_, radar.position)) {
                return message_.empty() ? Fail(Join(rp, "position"), "required, an object") : false;
            }
            return true;
        };
        if (const json::Value* r = p->Find("regiment")) {
            const std::string rp = "post.regiment";
            if (!r->Is(json::Type::Object) ||
                !Keys(*r, rp, {"position", "radars", "outages", "telecode", "designate_within_m", "commands", "command_net_mhz",
                               "telecode_mhz", "telecode_return_mhz", "mast", "messages", "telecode_wire", "notes"}) ||
                !OptionalBool(*r, rp, "telecode", out.post_regiment.telecode) ||
                !OptionalBool(*r, rp, "telecode_wire", out.post_regiment.telecode_wire) ||
                !OptionalNumber(*r, rp, "designate_within_m", out.post_regiment.designate_within_m)) {
                return message_.empty() ? Fail(rp, "expected an object") : false;
            }
            if (const json::Value* cmds = OptionalArray(*r, rp, "commands")) {
                double last_s = -1e18;
                for (std::size_t j = 0; j < cmds->items.size(); ++j) {
                    const std::string cp = Index(Join(rp, "commands"), j);
                    const json::Value& x = cmds->items[j];
                    RegimentCommand c;
                    double code = -1.0;
                    if (!Keys(x, cp, {"at_s", "code", "group", "on_cue", "notes"}) || !OptionalNumber(x, cp, "code", code) ||
                        !OptionalString(x, cp, "group", c.group) || !OptionalBool(x, cp, "on_cue", c.on_cue)) {
                        return false;
                    }
                    if ((x.Find("code") != nullptr) == !c.group.empty()) return Fail(cp, "a telecode \"code\" or a talk-table \"group\", one of them");
                    if (x.Find("code") != nullptr) {
                        if (code < 0.0 || code > 99.0 || code != std::floor(code)) return Fail(Join(cp, "code"), "a two-digit number, 00-99");
                        c.code = static_cast<int>(code);
                    }
                    if (c.on_cue) {
                        if (x.Find("at_s") != nullptr) return Fail(Join(cp, "at_s"), "a command held on cue has no time: the lesson sends it");
                    } else {
                        if (!RequireNumber(x, cp, "at_s", c.at_s)) return false;
                        if (c.at_s < last_s) return Fail(Join(cp, "at_s"), "the commands in time order");
                        last_s = c.at_s;
                    }
                    out.post_regiment.commands.push_back(c);
                }
            } else if (!message_.empty()) {
                return false;
            }
            RadioPlan& plan = out.radio_plan;
            double net = 0.0, tc = 0.0, tc_back = 0.0;
            if (!OptionalNumber(*r, rp, "command_net_mhz", net) || !OptionalNumber(*r, rp, "telecode_mhz", tc) ||
                !OptionalNumber(*r, rp, "telecode_return_mhz", tc_back) || !OptionalBool(*r, rp, "mast", plan.regiment_mast)) {
                return false;
            }
            plan.command_net_hz = net * 1.0e6;
            plan.telecode_hz = tc * 1.0e6;
            plan.telecode_return_hz = tc_back * 1.0e6;
            if (const json::Value* msgs = OptionalArray(*r, rp, "messages")) {
                for (std::size_t j = 0; j < msgs->items.size(); ++j) {
                    const std::string mp = Index(Join(rp, "messages"), j);
                    RegimentMessage m;
                    if (!Keys(msgs->items[j], mp, {"at_s", "text"}) || !RequireNumber(msgs->items[j], mp, "at_s", m.at_s) ||
                        !RequireString(msgs->items[j], mp, "text", m.text)) {
                        return false;
                    }
                    plan.messages.push_back(std::move(m));
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (const json::Value* pos = r->Find("position")) {
                if (!pos->Is(json::Type::Object) || !LoadPosition(*pos, Join(rp, "position"), frame_, out.post_regiment.position)) {
                    return message_.empty() ? Fail(Join(rp, "position"), "expected an object") : false;
                }
                out.post_regiment.has_position = true;
            }
            if (const json::Value* list = OptionalArray(*r, rp, "radars")) {
                for (std::size_t i = 0; i < list->items.size(); ++i) {
                    RegimentRadar radar;
                    if (!radar_of(list->items[i], Index(Join(rp, "radars"), i), true, radar)) return false;
                    out.post_regiment.radars.push_back(std::move(radar));
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (const json::Value* outs = OptionalArray(*r, rp, "outages")) {
                for (std::size_t j = 0; j < outs->items.size(); ++j) {
                    const std::string op = Index(Join(rp, "outages"), j);
                    C2Outage o;
                    std::string kind;
                    if (!Keys(outs->items[j], op, {"from_s", "to_s", "reason", "kind"}) || !RequireNumber(outs->items[j], op, "from_s", o.from_s) ||
                        !OptionalNumber(outs->items[j], op, "to_s", o.to_s) || !OptionalString(outs->items[j], op, "reason", o.reason) ||
                        !OptionalString(outs->items[j], op, "kind", kind)) {
                        return false;
                    }
                    if (!kind.empty() && kind != "off_air" && kind != "interference") {
                        return Fail(Join(op, "kind"), "must be off_air or interference");
                    }
                    o.interference = kind == "interference";
                    if (o.to_s <= o.from_s) return Fail(Join(op, "to_s"), "must be after from_s");
                    out.post_regiment.outages.push_back(o);
                }
            } else if (!message_.empty()) {
                return false;
            }
        }
        if (const json::Value* r = p->Find("radar")) {
            if (!r->Is(json::Type::Object)) return Fail("post.radar", "expected an object");
            if (!radar_of(*r, "post.radar", false, out.post_radar)) return false;
            // The radar's cable reaches only so far (flak11 PU12-0: up to 300 m).
            const Vec3 d = out.post_radar.position - out.unit_position;
            if (Vec3{d.x, d.y, 0.0}.Length() > out.post_aspd.radar_cable_m) {
                char why[96];
                std::snprintf(why, sizeof(why), "%.0f m from the post: its cable reaches %.0f m", Vec3{d.x, d.y, 0.0}.Length(),
                              out.post_aspd.radar_cable_m);
                return Fail("post.radar.position", why);
            }
            if (const json::Value* outs = OptionalArray(*r, "post.radar", "outages")) {
                for (std::size_t j = 0; j < outs->items.size(); ++j) {
                    const std::string op = Index("post.radar.outages", j);
                    C2Outage o;
                    if (!Keys(outs->items[j], op, {"from_s", "to_s", "reason"}) || !RequireNumber(outs->items[j], op, "from_s", o.from_s) ||
                        !OptionalNumber(outs->items[j], op, "to_s", o.to_s) || !OptionalString(outs->items[j], op, "reason", o.reason)) {
                        return false;
                    }
                    if (o.to_s <= o.from_s) return Fail(Join(op, "to_s"), "must be after from_s");
                    out.post_radar.outages.push_back(o);
                }
            } else if (!message_.empty()) {
                return false;
            }
            out.has_post_radar = true;
        }
        if (const json::Value* em = p->Find("em")) {
            // "em": { "library": "em/p19.json", "radar": "p19", "extra_libraries": [...], "jammers": [...] }
            const std::string ep = "post.em";
            if (!out.has_post_radar) return Fail(ep, "needs post.radar: the EM layer runs the radar cabled to the post");
            std::string library, radar;
            if (!em->Is(json::Type::Object) || !Keys(*em, ep, {"library", "radar", "extra_libraries", "rain_mmh", "jammers", "notes"}) ||
                !RequireString(*em, ep, "library", library) || !RequireString(*em, ep, "radar", radar)) {
                return message_.empty() ? Fail(ep, "expected an object") : false;
            }
            std::vector<std::string> extras;
            if (const json::Value* ex = OptionalArray(*em, ep, "extra_libraries")) {
                for (std::size_t i = 0; i < ex->items.size(); ++i) {
                    if (!ex->items[i].Is(json::Type::String)) return Fail(Index(Join(ep, "extra_libraries"), i), "expected a file name");
                    extras.push_back(ex->items[i].string);
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (!LoadEmLinkDef(data_dir, library, radar, std::string(), out.post_em, &error, extras)) return Fail(ep, error);
            if (!OptionalNumber(*em, ep, "rain_mmh", out.post_em.rain_mmh)) return false;
            if (out.post_em.rain_mmh < 0.0) return Fail(Join(ep, "rain_mmh"), "must be >= 0");
            if (!LoadEmJammers(*em, ep, out.post_em)) return false;
            out.has_post_em = true;
        }
        if (!LoadPostRadios(*p, data_dir, out, console)) return false;
        out.has_post = true;
        return true;
    }

    // An "em" block's jammers: each a library jammer on a target (self-screening) or at a position (stand-off), on and
    // off at times; "until_painted" (roadmap S5, a self-screening one): held off until its carrier is locked on or fired at.
    bool LoadEmJammers(const json::Value& em, const std::string& path, EmLinkDef& out) {
        const json::Value* jm = OptionalArray(em, path, "jammers");
        if (jm == nullptr) return message_.empty();
        for (std::size_t i = 0; i < jm->items.size(); ++i) {
            const std::string jp = Index(Join(path, "jammers"), i);
            const json::Value& j = jm->items[i];
            if (!Keys(j, jp, {"jammer", "target", "position", "on_at_s", "off_at_s", "until_painted", "notes"})) return false;
            EmJammerDef jd;
            if (!RequireString(j, jp, "jammer", jd.jammer_id) || !OptionalString(j, jp, "target", jd.target_id) ||
                !OptionalNumber(j, jp, "on_at_s", jd.on_at_s) || !OptionalNumber(j, jp, "off_at_s", jd.off_at_s) ||
                !OptionalBool(j, jp, "until_painted", jd.until_painted)) {
                return false;
            }
            if (jd.until_painted && jd.target_id.empty()) return Fail(Join(jp, "until_painted"), "a self-screening jammer's (with a target)");
            if (out.library.Jammer(jd.jammer_id) < 0) return Fail(Join(jp, "jammer"), "unknown jammer '" + jd.jammer_id + "' (add its file to extra_libraries)");
            const json::Value* jpos = j.Find("position");
            if (jd.target_id.empty() == (jpos == nullptr)) return Fail(jp, "give either a target (self-screening) or a position (stand-off)");
            if (jpos != nullptr && !LoadPosition(*jpos, Join(jp, "position"), frame_, jd.position)) return false;
            out.jammers.push_back(jd);
        }
        return true;
    }

    // The post's radio sets (the console's "radios"): their data file, every role bound, and the plan the scenario gives
    // them: "radios": { "r111_presets_mhz": [4 frequencies], "own_nets_mhz": { "fug1": 24.4, ... }, "prepared": true }.
    bool LoadPostRadios(const json::Value& p, const std::string& data_dir, ScenarioDef& out, const ConsoleData& console) {
        const json::Value* r = p.Find("radios");
        if (!console.r111.present && !console.r407.present) {
            return r == nullptr ? true : Fail("post.radios", "the console has no radio sets (" + std::string("radios") + ")");
        }
        std::string error;
        if (!LoadRadioSetsFile(data_dir + "/" + console.radios_file, out.radio_sets, &error)) return Fail("post", "the console's radios: " + error);
        for (const auto& set : {std::make_pair(true, &console.r111), std::make_pair(false, &console.r407)}) {
            if (!set.second->present) continue;
            const std::string missing = MissingRadioRole(*set.second, set.first);
            if (!missing.empty()) return Fail("post", std::string("the console's radios.") + (set.first ? "r111." : "r407.") + missing + " is not bound");
        }
        out.has_radio_sets = true;
        RadioPlan& plan = out.radio_plan;
        if (r == nullptr) return true;
        const std::string rp = "post.radios";
        if (!r->Is(json::Type::Object) || !Keys(*r, rp, {"r111_presets_mhz", "own_nets_mhz", "prepared", "repeat_s", "repeats", "notes"}) ||
            !OptionalBool(*r, rp, "prepared", plan.prepared) || !OptionalNumber(*r, rp, "repeat_s", plan.repeat_s) ||
            !OptionalInt(*r, rp, "repeats", plan.repeats)) {
            return message_.empty() ? Fail(rp, "expected an object") : false;
        }
        if (const json::Value* list = OptionalArray(*r, rp, "r111_presets_mhz")) {
            if (list->items.size() > 4) return Fail(Join(rp, "r111_presets_mhz"), "the R-111 has four presets");
            for (std::size_t i = 0; i < list->items.size(); ++i) {
                const json::Value& v = list->items[i];
                const double mhz = v.Is(json::Type::Number) ? v.number : -1.0;
                if (mhz != 0.0 && (mhz * 1.0e6 < out.radio_sets.r111.freq_min_hz || mhz * 1.0e6 > out.radio_sets.r111.freq_max_hz)) {
                    return Fail(Index(Join(rp, "r111_presets_mhz"), i), "outside the R-111's band (0: not prepared)");
                }
                plan.presets_hz[i] = mhz * 1.0e6;
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (const json::Value* nets = r->Find("own_nets_mhz")) {
            if (!nets->Is(json::Type::Object)) return Fail(Join(rp, "own_nets_mhz"), "expected an object of set: MHz");
            for (std::size_t i = 0; i < nets->keys.size(); ++i) {
                if (!nets->items[i].Is(json::Type::Number) || !(nets->items[i].number > 0.0)) {
                    return Fail(Join(Join(rp, "own_nets_mhz"), nets->keys[i]), "expected a frequency in MHz");
                }
                plan.own_nets_hz.emplace_back(nets->keys[i], nets->items[i].number * 1.0e6);
            }
        }
        if (!(plan.repeat_s > 0.0) || plan.repeats < 0) return Fail(rp, "repeat_s must be > 0, repeats >= 0");
        plan.present = true;
        return true;
    }

    // The regiment's radars that feed the PU-12M: "regiment": { "library": "command/regiment_radars.json",
    //   "radars": [ { "id": "p19_a", "type": "p19", "position": { "east_m": ..., "north_m": ... } } ] }.
    // The PU-12's board then holds only the targets some radar covers; without the block it sees them all.
    bool LoadRegiment(const json::Value& c2, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* r = c2.Find("regiment");
        if (r == nullptr) return true;
        const std::string rp = "c2.regiment";
        if (!r->Is(json::Type::Object) || !Keys(*r, rp, {"library", "radars", "notes"})) {
            return message_.empty() ? Fail(rp, "expected an object") : false;
        }
        C2SourceDef* pu = nullptr;
        for (C2SourceDef& d : out.c2_sources) {
            if (d.id == "pu12") pu = &d;
        }
        if (pu == nullptr) return Fail(rp, "the regiment feeds the PU-12M: the scenario needs a command_post");
        std::string library_file;
        if (!RequireString(*r, rp, "library", library_file)) return false;
        RegimentRadarLibrary lib;
        std::string error;
        if (!LoadRegimentRadarsFile(data_dir + "/" + library_file, lib, &error)) return Fail(Join(rp, "library"), error);
        const json::Value* list = OptionalArray(*r, rp, "radars");
        if (list == nullptr || list->items.empty()) {
            return message_.empty() ? Fail(Join(rp, "radars"), "needs at least one radar") : false;
        }
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index(Join(rp, "radars"), i);
            const json::Value& e = list->items[i];
            std::string id;
            std::string type;
            if (!Keys(e, p, {"id", "type", "position", "notes"}) || !RequireString(e, p, "id", id) ||
                !RequireString(e, p, "type", type)) {
                return false;
            }
            const RegimentRadar* t = lib.Find(type);
            if (t == nullptr) return Fail(Join(p, "type"), "unknown radar type '" + type + "'");
            for (const RegimentRadar& other : pu->feed) {
                if (other.id == id) return Fail(Join(p, "id"), "duplicate radar '" + id + "'");
            }
            RegimentRadar radar = *t;
            radar.id = id;
            radar.type = type;
            const json::Value* pos = e.Find("position");
            if (pos == nullptr || !pos->Is(json::Type::Object) || !LoadPosition(*pos, Join(p, "position"), frame_, radar.position)) {
                return message_.empty() ? Fail(Join(p, "position"), "required, an object") : false;
            }
            pu->feed.push_back(std::move(radar));
        }
        return true;
    }

    // An order to the battery: "march" with a "route" (positions), "short_halt", "combat_formation" with "covered"
    // (a position), "axis_deg" and "bounding", "halt", "resume", "ambush" with "points" (and "roving"),
    // "river_crossing" with "crossing", "axis_deg" and "width_m", "far_bank_held", "reload" / "refuel" with "vehicle",
    // "emcon" with "radiate" (false: the battery's radars silent; true: they may radiate again), "emission" with
    // "regime" (and "on_turns", "off_turns", "from": emission_regime.hpp). Said on the battery net: the launchers know
    // it once they hear it (at 0 s, it is part of the combat order given before the mission).
    bool LoadBatteryOrder(const json::Value& o, const std::string& p, const std::string& kind_text, const ScenarioDef& def, BatteryOrder& out) {
        if (!ParseBatteryOrder(kind_text, out.kind)) {
            return Fail(p, "unknown battery order '" + kind_text +
                               "' (march, short_halt, combat_formation, halt, resume, ambush, river_crossing, far_bank_held, reload, refuel, emcon, "
                               "relocate, emission)");
        }
        if (out.kind == BatteryOrder::Kind::Emission) {
            if (!LoadEmissionRegime(o, p, def, out.regime)) return false;
            out.from = out.regime.from;
        } else {
            for (const char* k : {"regime", "on_turns", "off_turns"}) {
                if (o.Find(k) != nullptr) return Fail(Join(p, k), "only an \"emission\" order has it");
            }
            // "from": "regiment": the regiment's order, which reaches the battery commander and he passes on (G10.7).
            if (!OptionalString(o, p, "from", out.from)) return false;
            if (!out.from.empty() && out.from != "regiment" && out.from != "battery") return Fail(Join(p, "from"), "\"regiment\" or \"battery\"");
        }
        if (!OptionalBool(o, p, "bounding", out.bounding) || !OptionalBool(o, p, "roving", out.roving) ||
            !OptionalNumber(o, p, "width_m", out.width_m) || !OptionalString(o, p, "vehicle", out.vehicle) ||
            !OptionalBool(o, p, "radiate", out.radiate)) {
            return false;
        }
        if (out.kind == BatteryOrder::Kind::Ambush) {
            const json::Value* pts = OptionalArray(o, p, "points");
            if (pts == nullptr || pts->items.empty()) return message_.empty() ? Fail(Join(p, "points"), "an ambush needs its prepared points") : false;
            for (std::size_t i = 0; i < pts->items.size(); ++i) {
                Vec3 v;
                if (!LoadPosition(pts->items[i], Index(Join(p, "points"), i), frame_, v)) return false;
                out.route.push_back(v);
            }
        }
        if (out.kind == BatteryOrder::Kind::RiverCrossing) {
            const json::Value* c = o.Find("crossing");
            if (c == nullptr) return Fail(Join(p, "crossing"), "a river crossing needs the crossing point on the near bank");
            if (!LoadPosition(*c, Join(p, "crossing"), frame_, out.covered) || !OptionalNumber(o, p, "axis_deg", out.axis_deg)) return false;
            if (out.width_m <= 0.0) return Fail(Join(p, "width_m"), "must be > 0");
        }
        if (out.kind == BatteryOrder::Kind::March) {
            // Where to by a place (G2.3): the route is the battery commander's, planned on the roads when he gives it.
            if (!OptionalString(o, p, "to_place", out.to_place)) return false;
            if (!out.to_place.empty()) {
                if (!def.nav) return Fail(Join(p, "to_place"), "a march to a place needs the scenario's \"nav\" (the roads)");
                if (def.nav->GetAnchors().Find(out.to_place) == nullptr) return Fail(Join(p, "to_place"), "no place called '" + out.to_place + "'");
                if (o.Find("route") != nullptr) return Fail(Join(p, "route"), "a march to a place has its route planned on the roads");
                return true;
            }
            const json::Value* route = OptionalArray(o, p, "route");
            if (route == nullptr || route->items.size() < 2) return message_.empty() ? Fail(Join(p, "route"), "a march needs a route of 2 or more positions") : false;
            for (std::size_t i = 0; i < route->items.size(); ++i) {
                Vec3 v;
                if (!LoadPosition(route->items[i], Index(Join(p, "route"), i), frame_, v)) return false;
                out.route.push_back(v);
            }
        } else if (o.Find("to_place") != nullptr && out.kind != BatteryOrder::Kind::Relocate) {
            return Fail(Join(p, "to_place"), "only a march or a change of position goes to a place");
        }
        if (out.kind == BatteryOrder::Kind::Relocate) {
            // To a place (G2): a march on the roads to it, planned when the order is given, the battery deploying there.
            if (!OptionalString(o, p, "to_place", out.to_place)) return false;
            if (!out.to_place.empty()) {
                if (!def.nav) return Fail(Join(p, "to_place"), "a change of position to a place needs the scenario's \"nav\" (the roads)");
                if (def.nav->GetAnchors().Find(out.to_place) == nullptr) return Fail(Join(p, "to_place"), "no place called '" + out.to_place + "'");
                return true;
            }
            const json::Value* c = o.Find("covered");
            if (c == nullptr) return Fail(Join(p, "covered"), "a change of position needs the new position (of the own vehicle)");
            if (!LoadPosition(*c, Join(p, "covered"), frame_, out.covered)) return false;
        }
        if (out.kind == BatteryOrder::Kind::CombatFormation) {
            const json::Value* c = o.Find("covered");
            if (c == nullptr) return Fail(Join(p, "covered"), "a combat formation needs the covered unit's position");
            if (!LoadPosition(*c, Join(p, "covered"), frame_, out.covered) || !OptionalNumber(o, p, "axis_deg", out.axis_deg)) return false;
        }
        return true;
    }

    //   "battery": { "tactics": "command/battery_tactics.json", "drive_own": false, "night": false, "share_picture": true,
    //                "own": { "id": "21", "callsign": "21" },
    //                "vehicles": [ { "id": "22", "callsign": "22", "kind": "launcher", "position": {...} }, ... ],
    //                "orders": [ { "at_s": 0, "order": "march", "route": [ {...}, {...} ] }, ... ] }
    // "by_means": { "chain": "command/chain_pannonia.json", "giver_post": "bty21_commander",
    //               "crews": [ { "vehicle": "23", "post": "veh21_3_commander", "set_on": false, "line": false } ] }
    // The battery commander rides in the command post (the own vehicle when it is the post, else the own vehicle). Every
    // other vehicle has a commander: his post's holder and skill tier from the chain, or class 2's times. The own vehicle,
    // when the battery drives it, is on the net too, seat by seat: its commander is the crew's (NPC or player).
    bool LoadBatteryByMeans(const json::Value& b, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* m = b.Find("by_means");
        if (m == nullptr) return true;
        const std::string mp = Join("battery", "by_means");
        std::string chain_file, error;
        if (!m->Is(json::Type::Object) || !Keys(*m, mp, {"chain", "giver_post", "crews", "notes"}) ||
            !OptionalString(*m, mp, "chain", chain_file) || !OptionalString(*m, mp, "giver_post", out.battery_giver_post)) {
            return message_.empty() ? Fail(mp, "expected an object") : false;
        }
        if (!command::LoadBatteryOrders(data_dir, out.battery_orders_def, &error)) return Fail(mp, error);
        command::ChainData chain;
        if (!chain_file.empty() && !command::LoadChainData(data_dir, chain_file, chain, &error)) return Fail(Join(mp, "chain"), error);
        if (!chain_file.empty()) out.battery_chain = std::make_shared<const command::ChainData>(chain);
        if (!out.has_skills) {
            if (!LoadSkillTableFile(data_dir + "/crew/skill_levels.json", out.skills, &error)) return Fail(mp, error);
            out.has_skills = true;
        }
        out.battery_giver = out.battery_vehicles.front().id;
        for (const BatteryVehicleDef& v : out.battery_vehicles) {
            if (v.kind == BatteryVehicleKind::CommandPost) {
                out.battery_giver = v.id;
                break;
            }
        }
        out.battery_crews.clear();
        for (const BatteryVehicleDef& v : out.battery_vehicles) {
            if (v.id == out.battery_giver || (v.own && !out.battery_drive_own)) continue;
            out.battery_crews.push_back(command::CommanderFromChain(chain, out.skills, v.id, v.callsign, v.callsign + "_commander"));
            out.battery_crews.back().seats = v.own;
        }
        if (const json::Value* cs = OptionalArray(*m, mp, "crews")) {
            for (std::size_t i = 0; i < cs->items.size(); ++i) {
                const std::string p = Index(Join(mp, "crews"), i);
                const json::Value& c = cs->items[i];
                std::string vehicle, post;
                bool set_on = true, line = false;
                if (!Keys(c, p, {"vehicle", "post", "set_on", "line", "notes"}) || !RequireString(c, p, "vehicle", vehicle) ||
                    !OptionalString(c, p, "post", post) || !OptionalBool(c, p, "set_on", set_on) || !OptionalBool(c, p, "line", line)) {
                    return false;
                }
                command::OrderVehicle* crew = nullptr;
                for (command::OrderVehicle& o : out.battery_crews) {
                    if (o.vehicle == vehicle) crew = &o;
                }
                if (crew == nullptr) return Fail(Join(p, "vehicle"), "'" + vehicle + "' is not one of the battery's other vehicles");
                if (!post.empty()) {
                    if (!chain_file.empty() && chain.FindPosition(post) == nullptr) return Fail(Join(p, "post"), "no post '" + post + "' in the chain");
                    const bool seats = crew->seats;
                    *crew = command::CommanderFromChain(chain, out.skills, crew->vehicle, crew->callsign, post);
                    crew->seats = seats;
                }
                crew->set_on = set_on;
                crew->line = line;
            }
        } else if (!message_.empty()) {
            return false;
        }
        out.battery_by_means = true;
        return true;
    }

    // The sky clock (roadmap R7, light.hpp): "date": "1985-10-15", "utc_offset_h": 1, with the origin and "clock_start".
    bool LoadSky(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        SkyClock& c = out.clock;
        c.lat_deg = RadToDeg(out.origin.lat_rad);
        c.lon_deg = RadToDeg(out.origin.lon_rad);
        std::string date;
        if (!OptionalString(root, "", "date", date)) return false;
        if (!date.empty() && !ParseIsoDate(date, out.year, c.day_of_year)) return Fail("date", "a date that exists, \"YYYY-MM-DD\"");
        c.year = out.year;  // the moon's (roadmap S8)
        c.utc_offset_h = std::round(c.lon_deg / 15.0);
        if (!OptionalNumber(root, "", "utc_offset_h", c.utc_offset_h)) return false;
        if (c.utc_offset_h < -12.0 || c.utc_offset_h > 14.0) return Fail("utc_offset_h", "hours ahead of UTC, -12 to 14");
        std::string error;
        if (std::ifstream(data_dir + "/gameplay/light.json").good() &&
            !LoadLightValues(data_dir, "gameplay/light.json", out.light_values, &error)) {
            return Fail("", error);  // there but wrong; a data folder without it keeps the code's thresholds
        }
        if (!airwar::LoadPerceptionData(data_dir, out.perception, &error)) return Fail("", error);
        // The flights' levels of detail (roadmap B12): a data folder without the file keeps every aircraft at A2.
        if (!LoadAirLevelValues(data_dir, "gameplay/air_levels.json", out.air_levels, &error)) return Fail("", error);
        const json::Value* env = root.Find("environment");
        const json::Value* bat = root.Find("battery");
        const bool daylight_given = env != nullptr && env->Find("daylight") != nullptr;
        const bool night_given = bat != nullptr && bat->Find("night") != nullptr;
        out.clock_given = out.clock_start_s >= 0.0;
        if (out.clock_given) {
            c.start_local_h = out.clock_start_s / 3600.0;
            const SunLight l = LightAt(c, 0.0, out.light_values);
            if (daylight_given && out.optics.daylight != l.daylight) {
                return Fail("environment.daylight", std::string("the sun at clock_start says ") + (l.daylight ? "daylight" : "no daylight") +
                                                        " (" + LightPhaseName(l.phase) + "): leave the key out, the clock decides");
            }
            if (night_given && out.battery_night != l.dark) {
                return Fail("battery.night", std::string("the sun at clock_start says ") + (l.dark ? "night" : "not night") + " (" +
                                                 LightPhaseName(l.phase) + "): leave the key out, the clock decides");
            }
        } else {
            c.start_local_h = !out.optics.daylight || out.battery_night ? 23.0 : 10.5;
        }
        return true;
    }

    // "clock_start": "19:00": the session's second 0 as a time of day.
    bool LoadClockStart(const json::Value& root, ScenarioDef& out) {
        std::string c;
        if (!OptionalString(root, "", "clock_start", c)) return false;
        if (c.empty()) return true;
        const auto digit = [&c](std::size_t i) { return c[i] >= '0' && c[i] <= '9' ? c[i] - '0' : -100; };
        const int h = c.size() == 5 ? digit(0) * 10 + digit(1) : -1;
        const int m = c.size() == 5 ? digit(3) * 10 + digit(4) : -1;
        if (c.size() != 5 || c[2] != ':' || h < 0 || h > 23 || m < 0 || m > 59) {
            return Fail("clock_start", "a time of day, \"HH:MM\"");
        }
        out.clock_start_s = 3600.0 * h + 60.0 * m;
        return true;
    }

    //   "task": { "file": "campaign/tasks/bty21_cover_bridges.json", "given_s": 60, "means": "radio" }
    // The regiment's combat order to the battery (roadmap 14.1): by radio, its short form on the regiment's command net to
    // the battery commander, who reads it back; in person, the paper in his hands at given_s. Its places must be places of
    // the session's roads, its sectors the battery's vehicles.
    bool LoadTask(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* t = root.Find("task");
        if (t == nullptr) return true;
        const std::string p = "task";
        std::string file, error;
        if (!Keys(*t, p, {"file", "given_s", "means", "held", "notes"}) || !RequireString(*t, p, "file", file) ||
            !OptionalNumber(*t, p, "given_s", out.task_given_s) || !OptionalString(*t, p, "means", out.task_means) ||
            !OptionalBool(*t, p, "held", out.task_held)) {
            return false;
        }
        if (out.task_given_s < 0.0) return Fail(Join(p, "given_s"), "must be >= 0");
        if (out.task_means != "radio" && out.task_means != "in_person") return Fail(Join(p, "means"), "\"radio\" or \"in_person\"");
        if (!out.has_battery || !out.battery_by_means) return Fail(p, "a task goes to a battery whose orders go by a means (battery.by_means)");
        if (!out.nav) return Fail(p, "a task's places are places of the session's roads: the scenario needs \"nav\"");
        if (out.task_means == "radio" && out.radio_data.Find("regiment") == nullptr) {
            return Fail(Join(p, "means"), "by radio on the regiment's command net: radio_data has no net \"regiment\"");
        }
        if (!campaign::LoadBatteryTask(data_dir, file, out.task, &error)) return Fail(Join(p, "file"), error);
        for (const std::string& place : out.task.Places()) {
            if (out.nav->GetAnchors().Find(place) == nullptr) return Fail(Join(p, "file"), file + ": no place called '" + place + "' on the roads");
        }
        for (const campaign::TaskSector& sec : out.task.sectors) {
            bool found = false;
            for (const BatteryVehicleDef& v : out.battery_vehicles) found = found || (v.id == sec.vehicle && v.kind == BatteryVehicleKind::Launcher);
            if (!found) return Fail(Join(p, "file"), file + ": a sector for '" + sec.vehicle + "', not a launcher of the battery");
        }
        if (!campaign::LoadPlanSequence(data_dir, "campaign/battery_plan.json", out.plan_sequence, &error)) return Fail(p, error);
        out.has_task = true;
        return true;
    }

    //   "nav": { "country": "maps/pannonia.json", "chain": "command/chain_pannonia.json",
    //            "movers": [{"id": "rgt_convoy", "kind": "convoy", "mobility": "truck", "from": "rgt_cp", "to": "tech_battery",
    //                        "depart_s": 60, "vehicles": 8, "spacing_m": 50}] }
    // The route service in the session (roadmap G2.3, G11): the country's roads with the scenario's map's tracks, the
    // chain's places and the country's towns as places, and the world's movers on the roads between them.
    bool LoadNav(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* n = root.Find("nav");
        if (n == nullptr) return true;
        const std::string p = "nav";
        std::string country = "maps/pannonia.json", chain, error;
        if (!n->Is(json::Type::Object) || !Keys(*n, p, {"country", "chain", "movers", "notes"}) || !OptionalString(*n, p, "country", country) ||
            !OptionalString(*n, p, "chain", chain)) {
            return message_.empty() ? Fail(p, "expected an object") : false;
        }
        auto s = std::make_shared<nav::SessionNav>();
        if (!s->Init(data_dir, country, out.map ? &out.map->Def() : nullptr, chain, &error)) return Fail(p, error);
        // The roads lie in the country's frame: the scenario's origin must be the country's (within a few metres).
        if (std::fabs(s->Origin().lat_rad - out.origin.lat_rad) > 1e-6 || std::fabs(s->Origin().lon_rad - out.origin.lon_rad) > 1e-6) {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "the scenario's origin must be the country's (%.4f, %.4f) for its roads to lie under it",
                          s->Origin().LatDeg(), s->Origin().LonDeg());
            return Fail(Join(p, "country"), buf);
        }
        if (const json::Value* ms = OptionalArray(*n, p, "movers")) {
            for (std::size_t i = 0; i < ms->items.size(); ++i) {
                const std::string mp = Index(Join(p, "movers"), i);
                const json::Value& m = ms->items[i];
                nav::SessionMoverDef d;
                double vehicles = 1.0;
                if (!Keys(m, mp, {"id", "kind", "mobility", "from", "to", "depart_s", "vehicles", "spacing_m", "model", "notes"}) ||
                    !RequireString(m, mp, "id", d.id) || !RequireString(m, mp, "from", d.from) || !RequireString(m, mp, "to", d.to) ||
                    !OptionalString(m, mp, "kind", d.kind) || !OptionalString(m, mp, "mobility", d.mobility) ||
                    !OptionalNumber(m, mp, "depart_s", d.depart_s) || !OptionalNumber(m, mp, "vehicles", vehicles) ||
                    !OptionalNumber(m, mp, "spacing_m", d.spacing_m) || !OptionalString(m, mp, "model", d.model)) {
                    return false;
                }
                if (vehicles < 1.0 || vehicles > 200.0) return Fail(Join(mp, "vehicles"), "1 to 200");
                if (d.spacing_m < 0.0) return Fail(Join(mp, "spacing_m"), "must be >= 0");
                d.vehicles = static_cast<int>(vehicles);
                std::string why;
                if (!s->AddMover(d, &why)) return Fail(mp, why);
            }
        } else if (!message_.empty()) {
            return false;
        }
        out.nav = s;
        return true;
    }

    // "supply": the TZM's runs to the regiment's technical battery on the session's roads (roadmap 17.4): the logistics'
    // values, the siding's place and what is there at the start. Needs the roads ("nav"), the place on them, and a TZM
    // whose commander hears the battery's orders by a means (battery.by_means).
    bool LoadSupply(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* s = root.Find("supply");
        if (s == nullptr) return true;
        const std::string p = "supply";
        std::string file = "campaign/logistics.json", error;
        double tested = 0.0, untested = 0.0;
        if (!s->Is(json::Type::Object) || !Keys(*s, p, {"logistics", "siding", "tested", "untested", "notes"}) ||
            !OptionalString(*s, p, "logistics", file) || !OptionalString(*s, p, "siding", out.supply_siding) ||
            !OptionalNumber(*s, p, "tested", tested) || !OptionalNumber(*s, p, "untested", untested)) {
            return message_.empty() ? Fail(p, "expected an object") : false;
        }
        if (tested < 0.0 || untested < 0.0) return Fail(p, "tested and untested must be >= 0");
        if (!campaign::LoadLogisticsValues(data_dir, file, out.logistics, &error)) return Fail(Join(p, "logistics"), error);
        if (!out.nav) return Fail(p, "the TZM's run needs the session's roads (\"nav\")");
        if (out.nav->GetAnchors().Find(out.supply_siding) == nullptr) return Fail(Join(p, "siding"), "no place called '" + out.supply_siding + "' on the roads");
        bool tzm = false;
        for (const command::OrderVehicle& c : out.battery_crews) {
            for (const BatteryVehicleDef& v : out.battery_vehicles) tzm = tzm || (v.id == c.vehicle && v.kind == BatteryVehicleKind::Reload);
        }
        if (!tzm) return Fail(p, "the battery needs a TZM whose commander hears its orders (battery.by_means)");
        out.siding_tested = static_cast<int>(tested);
        out.siding_untested = static_cast<int>(untested);
        out.has_supply = true;
        return true;
    }

    bool LoadBattery(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* b = root.Find("battery");
        if (b == nullptr) return true;
        const std::string bp = "battery";
        if (!b->Is(json::Type::Object) ||
            !Keys(*b, bp, {"tactics", "drive_own", "night", "share_picture", "deployed", "own", "launchers", "vehicles", "orders", "by_means",
                           "notes"})) {
            return message_.empty() ? Fail(bp, "expected an object") : false;
        }
        std::string tactics = "command/battery_tactics.json";
        bool share = false;
        if (!OptionalString(*b, bp, "tactics", tactics) || !OptionalBool(*b, bp, "drive_own", out.battery_drive_own) ||
            !OptionalBool(*b, bp, "night", out.battery_night) || !OptionalBool(*b, bp, "share_picture", share) ||
            !OptionalBool(*b, bp, "deployed", out.battery_deployed)) {
            return false;
        }
        std::string error;
        if (!LoadBatteryTacticsFile(data_dir + "/" + tactics, out.battery_tactics, &error)) return Fail(Join(bp, "tactics"), error);
        if (const json::Value* l = b->Find("launchers")) {
            const std::string lp = Join(bp, "launchers");
            std::string console_file, tuning_file;
            if (!l->Is(json::Type::Object) || !Keys(*l, lp, {"console", "tuning", "notes"}) || !RequireString(*l, lp, "console", console_file) ||
                !RequireString(*l, lp, "tuning", tuning_file)) {
                return message_.empty() ? Fail(lp, "expected an object") : false;
            }
            ConsoleData launcher_console;
            if (!LoadConsoleFile(data_dir + "/" + console_file, launcher_console, &error)) return Fail(Join(lp, "console"), error);
            if (!LoadEngagementTuningFile(data_dir + "/" + tuning_file, out.launcher_tuning, &error)) return Fail(Join(lp, "tuning"), error);
            if (out.launcher_tuning.system_id != launcher_console.system_id) {
                return Fail(Join(lp, "tuning"), "is for '" + out.launcher_tuning.system_id + "', not '" + launcher_console.system_id + "'");
            }
            out.launcher_reach_m = launcher_console.track.max_range_m;
            out.has_launcher_tuning = true;
        } else if (out.has_post) {
            return Fail(Join(bp, "launchers"), "the player's vehicle is the command post: the battery needs its launchers' console and tuning");
        }
        BatteryVehicleDef own;
        own.id = "own";
        own.callsign = "own";
        own.own = true;
        own.start = out.unit_position;
        if (out.has_post) own.kind = BatteryVehicleKind::CommandPost;
        if (const json::Value* o = b->Find("own")) {
            if (!Keys(*o, Join(bp, "own"), {"id", "callsign"}) || !OptionalString(*o, Join(bp, "own"), "id", own.id) ||
                !OptionalString(*o, Join(bp, "own"), "callsign", own.callsign)) {
                return false;
            }
        }
        out.battery_vehicles.push_back(own);
        if (const json::Value* vs = OptionalArray(*b, bp, "vehicles")) {
            for (std::size_t i = 0; i < vs->items.size(); ++i) {
                const std::string p = Index(Join(bp, "vehicles"), i);
                const json::Value& v = vs->items[i];
                BatteryVehicleDef d;
                std::string kind = "launcher";
                if (!Keys(v, p, {"id", "callsign", "kind", "position", "sector_deg", "stock", "notes"}) || !RequireString(v, p, "id", d.id) ||
                    !OptionalString(v, p, "callsign", d.callsign) || !OptionalString(v, p, "kind", kind) || !OptionalNumber(v, p, "stock", d.stock)) {
                    return false;
                }
                if (v.Find("stock") != nullptr && (d.stock < 0.0 || (kind != "reload" && kind != "fuel"))) {
                    return Fail(Join(p, "stock"), "a TZM's missiles or a fuel truck's litres, >= 0");
                }
                if (const json::Value* sec = OptionalArray(v, p, "sector_deg")) {
                    if (sec->items.size() != 2 || !sec->items[0].Is(json::Type::Number) || !sec->items[1].Is(json::Type::Number)) {
                        return Fail(Join(p, "sector_deg"), "expected [from, to]: true azimuths, clockwise");
                    }
                    d.has_sector = true;
                    d.sector_from_deg = sec->items[0].number;
                    d.sector_to_deg = sec->items[1].number;
                } else if (!message_.empty()) {
                    return false;
                }
                if (!ParseBatteryVehicleKind(kind, d.kind)) return Fail(Join(p, "kind"), "launcher, reload, command_post, maintenance or fuel");
                const json::Value* pos = v.Find("position");
                if (pos == nullptr) return Fail(Join(p, "position"), "required");
                if (!LoadPosition(*pos, Join(p, "position"), frame_, d.start)) return false;
                if (d.callsign.empty()) d.callsign = d.id;
                for (const BatteryVehicleDef& other : out.battery_vehicles) {
                    if (other.id == d.id) return Fail(Join(p, "id"), "duplicate vehicle '" + d.id + "'");
                }
                out.battery_vehicles.push_back(d);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (!LoadBatteryByMeans(*b, data_dir, out)) return false;
        if (const json::Value* os = OptionalArray(*b, bp, "orders")) {
            for (std::size_t i = 0; i < os->items.size(); ++i) {
                const std::string p = Index(Join(bp, "orders"), i);
                const json::Value& o = os->items[i];
                if (!Keys(o, p, {"at_s", "order", "route", "covered", "axis_deg", "bounding", "roving", "points", "crossing", "width_m",
                                 "vehicle", "radiate", "regime", "on_turns", "off_turns", "from", "to_place", "notes"})) {
                    return false;
                }
                std::string kind;
                BatteryOrder ord;
                if (!RequireString(o, p, "order", kind) || !OptionalNumber(o, p, "at_s", ord.at_s) ||
                    !LoadBatteryOrder(o, p, kind, out, ord)) {
                    return false;
                }
                out.battery_orders.push_back(ord);
            }
        } else if (!message_.empty()) {
            return false;
        }
        out.has_battery = true;
        // The other launchers' radar pictures reach the crew as neighbour sources (batteries feeding each other).
        if (share) {
            std::string library = "command/c2_sources.json";
            C2Library lib;
            if (!LoadC2LibraryFile(data_dir + "/" + library, lib, &error)) return Fail(Join(bp, "share_picture"), error);
            const C2SourceDef* t = lib.Find("neighbour_radio");
            if (t == nullptr) return Fail(Join(bp, "share_picture"), "the C2 library has no neighbour_radio type");
            for (const BatteryVehicleDef& v : out.battery_vehicles) {
                if (v.own || v.kind != BatteryVehicleKind::Launcher) continue;
                C2SourceDef d = *t;
                d.id = v.id;
                d.type = "neighbour_radio";
                d.display_name = "Battery vehicle " + v.callsign;
                d.short_name = ("Veh " + v.callsign).substr(0, 14);
                d.board.id = v.id;
                d.has_position = true;
                d.position = v.start;
                out.c2_sources.push_back(d);
                if (std::find(out.c2_policy.fallback_order.begin(), out.c2_policy.fallback_order.end(), v.id) ==
                    out.c2_policy.fallback_order.end()) {
                    out.c2_policy.fallback_order.push_back(v.id);
                }
            }
            bool primary_known = false;
            for (const C2SourceDef& d : out.c2_sources) primary_known = primary_known || d.id == out.c2_policy.primary;
            if (!primary_known && !out.c2_sources.empty()) out.c2_policy.primary = out.c2_sources.front().id;
        }
        return true;
    }

    //   "prepared_position": { "file": "command/prepared_positions.json", "emplacement": true, "nets": true, "wire": true,
    //                          "dummies": [ { "position": {...}, "decoy": "when_radar_off" } ] }
    bool LoadPrepared(const json::Value& v, const std::string& data_dir, ScenarioDef& out) {
        const std::string p = "own_unit.prepared_position";
        std::string file = "command/prepared_positions.json";
        if (!v.Is(json::Type::Object) || !Keys(v, p, {"file", "emplacement", "nets", "wire", "dummies", "notes"}) ||
            !OptionalString(v, p, "file", file) || !OptionalBool(v, p, "emplacement", out.prepared.emplacement) ||
            !OptionalBool(v, p, "nets", out.prepared.nets) || !OptionalBool(v, p, "wire", out.prepared.wire)) {
            return message_.empty() ? Fail(p, "expected an object") : false;
        }
        std::string error;
        if (!LoadPreparedPositionFile(data_dir + "/" + file, out.prepared.values, &error)) return Fail(Join(p, "file"), error);
        if (const json::Value* ds = OptionalArray(v, p, "dummies")) {
            for (std::size_t i = 0; i < ds->items.size(); ++i) {
                const std::string dp = Index(Join(p, "dummies"), i);
                const json::Value& d = ds->items[i];
                if (!Keys(d, dp, {"position", "decoy", "notes"})) return false;
                DummyPositionDef dd;
                std::string decoy;
                if (!OptionalString(d, dp, "decoy", decoy)) return false;
                if (!decoy.empty() && decoy != "none") {
                    if (!em::ParseDecoyMode(decoy, dd.mode)) return Fail(Join(dp, "decoy"), "none, with_radar, when_radar_off or always");
                    dd.decoy = true;
                }
                const json::Value* pos = d.Find("position");
                if (pos == nullptr) return Fail(Join(dp, "position"), "required");
                if (!LoadPosition(*pos, Join(dp, "position"), frame_, dd.position)) return false;
                out.prepared.dummies.push_back(dd);
            }
        } else if (!message_.empty()) {
            return false;
        }
        out.prepared.present = true;
        // With the EM layer: the vehicle's echo and the dummies' echoes and decoys.
        out.em.unit_rcs_m2 = out.prepared.VehicleRcsM2();
        for (const DummyPositionDef& d : out.prepared.dummies) {
            EmDummyDef e;
            e.position = d.position;
            e.rcs_m2 = out.prepared.values.dummy_rcs_m2;
            e.decoy = d.decoy;
            e.mode = d.mode;
            out.em.dummies.push_back(e);
        }
        return true;
    }

    bool LoadTriggers(const json::Value& root, ScenarioDef& out, const ConsoleData& console) {
        const json::Value* arr = OptionalArray(root, "", "triggers");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("triggers", i);
            const json::Value& t = arr->items[i];
            ScenarioTrigger tr;
            const json::Value* when = nullptr;
            if (!Keys(t, p, {"id", "when", "do", "notes"}) || !RequireString(t, p, "id", tr.id) ||
                !RequireObject(t, p, "when", when)) {
                return false;
            }
            const std::string wp = Join(p, "when");
            if (!Keys(*when, wp, {"after_s", "target", "target_within_m", "conditions", "launches_at_least"}) ||
                !OptionalNumber(*when, wp, "after_s", tr.after_s) || !OptionalString(*when, wp, "target", tr.target) ||
                !OptionalNumber(*when, wp, "target_within_m", tr.target_within_m) ||
                !OptionalInt(*when, wp, "launches_at_least", tr.launches_at_least) ||
                !LoadConditions(*when, wp, "conditions", console.panel, console.equipment, tr.conditions)) {
                return false;
            }
            if (tr.target.empty() != (when->Find("target_within_m") == nullptr)) {
                return Fail(wp, "target and target_within_m go together");
            }
            if (!tr.target.empty()) {
                bool found = false;
                for (const ScenarioTarget& tg : out.targets) found = found || tg.id == tr.target;
                if (!found) return Fail(Join(wp, "target"), "unknown target '" + tr.target + "'");
                if (tr.target_within_m <= 0.0) return Fail(Join(wp, "target_within_m"), "must be > 0");
            }
            const json::Value* actions = OptionalArray(t, p, "do");
            if (actions == nullptr || actions->items.empty()) {
                return message_.empty() ? Fail(Join(p, "do"), "needs at least one action") : false;
            }
            for (std::size_t j = 0; j < actions->items.size(); ++j) {
                TriggerAction a;
                if (!LoadAction(actions->items[j], Index(Join(p, "do"), j), out, console, a)) return false;
                tr.actions.push_back(std::move(a));
            }
            for (const ScenarioTrigger& other : out.triggers) {
                if (other.id == tr.id) return Fail(Join(p, "id"), "duplicate trigger '" + tr.id + "'");
            }
            out.triggers.push_back(std::move(tr));
        }
        return true;
    }

    // The chain of command's messages in the session (session_deliveries.hpp, roadmap P1).
    bool LoadDeliveries(const json::Value& root, ScenarioDef& out, const ConsoleData& console) {
        const json::Value* arr = OptionalArray(root, "", "deliveries");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("deliveries", i);
            const json::Value& v = arr->items[i];
            DeliveryDef d;
            std::string means;
            if (!Keys(v, p, {"id", "after_s", "to", "means", "net", "from", "words", "paper", "air_warning", "from_bearing_deg", "azimuth_deg", "notes"}) ||
                !RequireString(v, p, "id", d.id) || !RequireString(v, p, "means", means) || !RequireString(v, p, "from", d.from) ||
                !RequireString(v, p, "words", d.words) || !OptionalNumber(v, p, "after_s", d.after_s) ||
                !OptionalString(v, p, "to", d.to) || !OptionalString(v, p, "net", d.net) ||
                !OptionalBool(v, p, "air_warning", d.air_warning) || !OptionalNumber(v, p, "from_bearing_deg", d.from_bearing_deg)) {
                return false;
            }
            if (!ParseDeliveryMeans(means, d.means)) return Fail(Join(p, "means"), "unknown means '" + means + "'");
            if (d.after_s < 0.0) return Fail(Join(p, "after_s"), "must be >= 0");
            if (v.Find("azimuth_deg") != nullptr) {
                if (!OptionalNumber(v, p, "azimuth_deg", d.azimuth_deg)) return false;
                if (d.azimuth_deg < 0.0 || d.azimuth_deg >= 360.0) return Fail(Join(p, "azimuth_deg"), "must be in [0, 360)");
                d.has_azimuth = true;
            }
            if (d.means == DeliveryMeans::Radio && d.net.empty()) return Fail(Join(p, "net"), "a radio delivery names its net");
            if (!d.to.empty() && console.FindStation(d.to) == nullptr) return Fail(Join(p, "to"), "unknown station '" + d.to + "'");
            if (const json::Value* paper = v.Find("paper")) {
                const std::string pp = Join(p, "paper");
                if (!Keys(*paper, pp, {"title", "text"}) || !RequireString(*paper, pp, "title", d.paper_title) ||
                    !OptionalString(*paper, pp, "text", d.paper_text)) {
                    return false;
                }
            }
            for (const DeliveryDef& other : out.deliveries) {
                if (other.id == d.id) return Fail(Join(p, "id"), "duplicate delivery '" + d.id + "'");
            }
            out.deliveries.push_back(d);
        }
        return true;
    }

    bool LoadEnd(const json::Value& root, ScenarioDef& out) {
        const json::Value* e = root.Find("end");
        if (e == nullptr) return true;
        if (!Keys(*e, "end", {"time_limit_s", "when_targets_gone", "on_unit_hit"}) ||
            !OptionalNumber(*e, "end", "time_limit_s", out.time_limit_s) ||
            !OptionalBool(*e, "end", "when_targets_gone", out.end_when_targets_gone) ||
            !OptionalBool(*e, "end", "on_unit_hit", out.end_on_unit_hit)) {
            return false;
        }
        if (out.time_limit_s <= 0.0) return Fail("end.time_limit_s", "must be > 0");
        return true;
    }

    LocalFrame frame_;
};

}  // namespace detail

// Parse a scenario and load the console it names from `data_dir` (the repo's data/ folder).
// On failure the outputs are left unchanged.
inline bool LoadScenario(const std::string& json_text, const std::string& data_dir, ScenarioDef& out,
                         ConsoleData& console, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    ScenarioDef def;
    ConsoleData loaded;
    detail::ScenarioLoader loader;
    if (!loader.Load(root, data_dir, def, loaded)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(def);
    console = std::move(loaded);
    return true;
}

// The time of day chosen in the missions menu (roadmap R7): the session's second 0 at a local hour of the scenario's own
// day. The paper clock ("clock_start") moves with it only when the scenario has one; the legacy light keys then follow
// the sun so they no longer choose anything.
inline void SetStartHour(ScenarioDef& def, double local_h) {
    local_h -= 24.0 * std::floor(local_h / 24.0);
    def.clock.start_local_h = local_h;
    if (def.clock_given) def.clock_start_s = local_h * 3600.0;
    const SunLight l = LightAt(def.clock, 0.0, def.light_values);
    def.optics.daylight = l.daylight;
    def.battery_night = l.dark;
}

// A preset of the menu's: the mission's own (nothing changes), an hour, or dawn, day, dusk or night by the scenario's sun.
inline void ApplyTimeChoice(ScenarioDef& def, TimePreset p, double hour = 0.0) {
    if (p == TimePreset::Mission) return;
    SetStartHour(def, p == TimePreset::Hour ? hour : PresetHour(def.clock, p));
}

inline bool LoadScenarioFile(const std::string& path, const std::string& data_dir, ScenarioDef& out,
                             ConsoleData& console, std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    if (!LoadScenario(text.str(), data_dir, out, console, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

}  // namespace simcore
