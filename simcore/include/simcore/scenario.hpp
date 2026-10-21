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
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/battery.hpp"
#include "simcore/c2_sources.hpp"
#include "simcore/command_post.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/crew_mobility.hpp"
#include "simcore/crew_training.hpp"
#include "simcore/em_link.hpp"
#include "simcore/em/optical.hpp"
#include "simcore/engagement.hpp"
#include "simcore/position_prep.hpp"
#include "simcore/pu12.hpp"
#include "simcore/radio_nets.hpp"
#include "simcore/radio_sets.hpp"
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
    double rcs_m2 = 5.0;
    double appear_at_s = 0.0;
    bool on_cue = false;                   // held until something releases it (a lesson: ScenarioRunner::ReleaseTarget)
    std::string target_class = "default";  // picks the hit chance in the gameplay tuning
    bool evade_on_launch = false;          // breaks away onto a beam heading when a missile is launched at it
    double evade_turn_rate_rad_s = DegToRad(12.0);
    bool friendly = false;                 // "iff": "friend": replies to IFF interrogation
    int board_type = -1;                   // its type on the PU-12 board (0-9; -1: 1 if a friend, else 9, no IFF)
    int formation = 1;                     // aircraft in its formation, as the board notes it
    TargetBehaviourDef behaviour;          // terrain following, pop-up (target_behaviour.hpp)
    EvadeDef evade;                        // evasion on launch or lock (takes the place of evade_on_launch)
    double size_m = 4.5;                   // critical dimension for the TV sight (em/optical.hpp; estimate for a fighter)
    // Its radar warning receiver: an ESM receiver of the EM library (kind rwr). With one, the pilot evades when it warns
    // of the lock or the launch (the command link heard: roadmap E5), not on the truth.
    std::string rwr;
    // The model it is drawn with (data/scenery/basic_models.json; "" its class's): presentation only.
    std::string model;
    // A decoy (roadmap E6): a drone whose echo is made to look like an aircraft's (radar reflectors). The radar can't
    // tell; the TV can, close enough to make out its type (em/optical.hpp), when the crew looks.
    bool decoy = false;
    // Anti-radiation missiles it carries (roadmap E6, em/arm.hpp): "arms". Count 0: none.
    ArmCarrierDef arms;
};

// How the crew fights, as its orders from above put it (roadmap E6): "doctrine".
struct CrewDoctrineDef {
    // Warned of decoys: the commander has the deputy look at what the radar holds on the TV (tv_watch) and launches
    // only at what he has made out; at night or in a haze where the TV sees nothing he goes by the radar.
    bool tv_check = false;
    // Emission discipline against direction-finding: while nothing is tracked, the search radar radiates in bursts,
    // on for on_s then off for off_s (video_osa: the search radar on for two turns). 0: always on.
    double search_on_s = 0.0;
    double search_off_s = 0.0;
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
    // arm_silence_s after the last sign (a SAM3D estimate).
    bool arm_threat = false;
    double arm_silence_s = 60.0;
    double arm_search_off_turns = 3.0;  // the low end of the textbook's 3-4
    // A standing order from the regiment ("relocate_after_arm": {"to": position}): attacked by ARMs, the battery moves to
    // its reserve position when the fight is over (only where the battery drives this vehicle, battery_drive_own). The
    // battery commander changes position only on the regiment commander's order (voenizdat_osa_1991 p238).
    bool relocate_after_arm = false;
    Vec3 relocate_to;
};

struct TriggerAction {
    enum class Kind { SetInput, Order, Queue, End, C2Source, C2Fail, C2Restore, BatteryOrder, AirThreat };
    Kind kind = Kind::SetInput;
    std::string input;      // SetInput
    bool value = true;      // SetInput
    std::string crew;       // Order, Queue
    std::string procedure;  // Order, Queue
    std::string reason;     // End, C2Fail
    std::string c2_id;      // C2Source (a source id or "none"), C2Fail, C2Restore
    simcore::BatteryOrder battery_order;  // BatteryOrder
    bool air_threat = false;              // AirThreat
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

struct ScenarioDef {
    std::string id;
    std::string name;
    std::string description;
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
    PositionPowerDef position_power;
    PreparedPositionDef prepared;  // own_unit.prepared_position (roadmap 7.6)
    std::string readiness;         // the state the unit starts in: "" or "cold", or a console readiness state
    bool has_em = false;           // the unit's radars come from the electromagnetic layer (own_unit.em)
    EmLinkDef em;
    em::OpticalConditions optics;  // "environment": the visibility and the daylight the TV sight works in
    CrewDoctrineDef doctrine;      // "doctrine": the TV check against decoys, search bursts
    Vec3 unit_position;            // ENU [m]
    double unit_heading_deg = 0.0; // the hull's true heading, clockwise from north
    // "empty_containers": the launcher's containers that start without a missile (e.g. ["1-I", "1-II"]): a mission that
    // starts part-spent, so that the TZM comes within it (roadmap 8.11).
    std::vector<std::string> empty_containers;
    std::vector<std::pair<std::string, bool>> initial_inputs;
    std::vector<std::pair<std::string, int>> initial_controls;  // set after the readiness (own_unit.controls)
    std::vector<ScenarioCrew> crew;
    std::vector<ScenarioTarget> targets;
    std::vector<ScenarioTrigger> triggers;
    std::string command_post_file;  // the battery command post's plotting board, e.g. "command/pu12.json"; "" = none
    PlottingBoardDef board;
    // The player's vehicle is the command post (the console has "post"): its own board, its ASPD-U, the regiment that
    // sends it the telecode, and a radar cabled to it (simcore/command_post.hpp, docs/PU12_DEVELOPMENT.md P3).
    bool has_post = false;
    PlottingBoardDef post_board;
    AspdDef post_aspd;
    PostRegiment post_regiment;
    AspdCodeTable post_codes;        // the regiment's two-digit commands' meanings ("post.codes"; empty: none)
    PlottingTuning post_plotting;    // the player's hand on the board ("post.plotting"; the defaults without it)
    bool has_post_radar = false;
    RegimentRadar post_radar;
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
    bool battery_drive_own = false;
    bool battery_night = false;
    bool battery_deployed = false;  // "deployed": every vehicle starts set up where it stands
    // The battery's launchers when the player's vehicle is not one of them (the PU-12M): their gameplay tuning and their
    // tracking radar's reach, from "battery.launchers": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json" }.
    bool has_launcher_tuning = false;
    EngagementTuning launcher_tuning;
    double launcher_reach_m = 0.0;
    double time_limit_s = 600.0;
    bool end_when_targets_gone = true;  // end once every target is destroyed or past its last waypoint
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
                             "targets", "triggers", "end", "assistance", "environment", "doctrine", "weather", "radio_data"})) {
            return false;
        }
        if (!RequireString(root, "", "scenario_id", out.id) || !RequireString(root, "", "name", out.name) ||
            !OptionalString(root, "", "description", out.description) || !Provenance(root, "")) {
            return false;
        }
        int seed = 1;
        if (!OptionalInt(root, "", "seed", seed)) return false;
        if (seed < 0) return Fail("seed", "must be >= 0");
        out.seed = static_cast<unsigned>(seed);
        if (!LoadOrigin(root, out) || !LoadTerrain(root, data_dir, out)) return false;
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
        if (!LoadBattery(root, data_dir, out)) return false;
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
        if (!LoadTargets(root, out)) return false;
        if (!LoadTriggers(root, out, console)) return false;
        return LoadEnd(root, out);
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
            if (!Keys(e, lp, {"line", "to", "to_name", "laid", "calls", "notes"}) || !RequireNumber(e, lp, "line", n) ||
                !RequireString(e, lp, "to", l.to) || !OptionalString(e, lp, "to_name", l.to_name) || !OptionalBool(e, lp, "laid", l.laid)) {
                return false;
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
            !Keys(*u, "own_unit", {"console", "tuning", "crew_data", "em", "readiness", "fuel_fraction", "position_power", "prepared_position",
                                   "position", "heading_deg", "inputs", "controls", "empty_containers"})) {
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
            if (const json::Value* jm = OptionalArray(*em, "own_unit.em", "jammers")) {
                for (std::size_t i = 0; i < jm->items.size(); ++i) {
                    const std::string jp = Index("own_unit.em.jammers", i);
                    const json::Value& j = jm->items[i];
                    if (!Keys(j, jp, {"jammer", "target", "position", "on_at_s", "off_at_s", "notes"})) return false;
                    EmJammerDef jd;
                    if (!RequireString(j, jp, "jammer", jd.jammer_id) || !OptionalString(j, jp, "target", jd.target_id) ||
                        !OptionalNumber(j, jp, "on_at_s", jd.on_at_s) || !OptionalNumber(j, jp, "off_at_s", jd.off_at_s)) {
                        return false;
                    }
                    if (out.em.library.Jammer(jd.jammer_id) < 0) return Fail(Join(jp, "jammer"), "unknown jammer '" + jd.jammer_id + "' (add its file to extra_libraries)");
                    const json::Value* jpos = j.Find("position");
                    if (jd.target_id.empty() == (jpos == nullptr)) return Fail(jp, "give either a target (self-screening) or a position (stand-off)");
                    if (jpos != nullptr && !LoadPosition(*jpos, Join(jp, "position"), frame_, jd.position)) return false;
                    out.em.jammers.push_back(jd);
                }
            } else if (!message_.empty()) {
                return false;
            }
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

    // "doctrine": {"tv_check": true, "search_bursts": {"on_s": 4, "off_s": 20}, "salvo": 2}.
    bool LoadDoctrine(const json::Value& root, ScenarioDef& out) {
        const json::Value* d = root.Find("doctrine");
        if (d == nullptr) return true;
        if (!Keys(*d, "doctrine", {"tv_check", "search_bursts", "salvo", "arm_threat", "arm_silence_s", "arm_search_off_turns",
                                   "relocate_after_arm", "notes"}) ||
            !OptionalBool(*d, "doctrine", "tv_check", out.doctrine.tv_check) ||
            !OptionalInt(*d, "doctrine", "salvo", out.doctrine.salvo) || !OptionalBool(*d, "doctrine", "arm_threat", out.doctrine.arm_threat) ||
            !OptionalNumber(*d, "doctrine", "arm_silence_s", out.doctrine.arm_silence_s) ||
            !OptionalNumber(*d, "doctrine", "arm_search_off_turns", out.doctrine.arm_search_off_turns)) {
            return false;
        }
        if (out.doctrine.arm_silence_s < 0.0) return Fail("doctrine.arm_silence_s", "must be >= 0");
        if (out.doctrine.arm_search_off_turns < 0.0) return Fail("doctrine.arm_search_off_turns", "must be >= 0");
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
            if (!Keys(*b, "doctrine.search_bursts", {"on_s", "off_s", "notes"}) ||
                !RequireNumber(*b, "doctrine.search_bursts", "on_s", out.doctrine.search_on_s) ||
                !RequireNumber(*b, "doctrine.search_bursts", "off_s", out.doctrine.search_off_s)) {
                return false;
            }
            if (out.doctrine.search_on_s <= 0.0 || out.doctrine.search_off_s <= 0.0) {
                return Fail("doctrine.search_bursts", "on_s and off_s must be > 0");
            }
        }
        return true;
    }

    // A target's "arms" (ArmCarrierDef): its warning receiver tells it what to fire at, so it needs one.
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

    bool LoadTargets(const json::Value& root, ScenarioDef& out) {
        const json::Value* arr = OptionalArray(root, "", "targets");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("targets", i);
            const json::Value& t = arr->items[i];
            if (!Keys(t, p, {"id", "name", "type", "class", "start", "waypoints", "speed_mps", "max_turn_rate_deg_s",
                             "rcs_m2", "appear_at_s", "evade_on_launch", "evade_turn_rate_deg_s", "iff", "board_type",
                             "formation", "behaviour", "evade", "size_m", "rwr", "decoy", "model", "on_cue", "arms", "notes"})) {
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
            if (!OptionalInt(t, p, "board_type", tg.board_type) || !OptionalInt(t, p, "formation", tg.formation)) return false;
            if (t.Find("board_type") != nullptr && (tg.board_type < 0 || tg.board_type > 9)) {
                return Fail(Join(p, "board_type"), "a digit 0-9 (0 jamming, 1 friend, 2 training target, 8 enemy, 9 no IFF...)");
            }
            if (tg.formation < 1) return Fail(Join(p, "formation"), "must be >= 1");
            if (!OptionalNumber(t, p, "size_m", tg.size_m) || !OptionalString(t, p, "rwr", tg.rwr) ||
                !OptionalBool(t, p, "decoy", tg.decoy) || !OptionalString(t, p, "model", tg.model)) {
                return false;
            }
            if (tg.size_m <= 0.0) return Fail(Join(p, "size_m"), "must be > 0");
            if (!tg.rwr.empty()) {
                if (!out.has_em) return Fail(Join(p, "rwr"), "needs own_unit.em: the warning receiver is an EM receiver");
                const int e = out.em.library.Esm(tg.rwr);
                if (e < 0) return Fail(Join(p, "rwr"), "'" + tg.rwr + "' is not an ESM receiver of the EM libraries");
                if (out.em.library.esm[static_cast<std::size_t>(e)].kind != em::EsmKind::Rwr) {
                    return Fail(Join(p, "rwr"), "'" + tg.rwr + "' is not a radar warning receiver");
                }
            }
            if (!LoadArmCarrier(t, p, out, tg)) return false;
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
                         "crossing", "width_m", "vehicle", "radiate"})) {
            return false;
        }
        const int kinds = (a.Find("set_input") != nullptr) + (a.Find("order") != nullptr) + (a.Find("queue") != nullptr) +
                          (a.Find("end") != nullptr) + (a.Find("c2_source") != nullptr) + (a.Find("c2_fail") != nullptr) +
                          (a.Find("c2_restore") != nullptr) + (a.Find("battery_order") != nullptr) +
                          (a.Find("air_threat") != nullptr);
        if (kinds != 1) {
            return Fail(p, "needs exactly one of set_input, order, queue, end, c2_source, c2_fail, c2_restore, battery_order or air_threat");
        }
        if (a.Find("battery_order") != nullptr || a.Find("air_threat") != nullptr) {
            if (!def.has_battery) return Fail(p, "the scenario has no battery block");
            if (a.Find("air_threat") != nullptr) {
                out.kind = TriggerAction::Kind::AirThreat;
                return OptionalBool(a, p, "air_threat", out.air_threat);
            }
            out.kind = TriggerAction::Kind::BatteryOrder;
            std::string kind;
            return RequireString(a, p, "battery_order", kind) && LoadBatteryOrder(a, p, kind, out.battery_order);
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
            !Keys(*p, "post", {"board", "aspd", "codes", "plotting", "library", "regiment", "radar", "radios", "notes"})) {
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
        if (!LoadAspdFile(data_dir + "/" + aspd_file, out.post_aspd, &error)) return Fail("post.aspd", error);
        if (!codes_file.empty() && !LoadAspdCodesFile(data_dir + "/" + codes_file, out.post_codes, &error)) return Fail("post.codes", error);
        RegimentRadarLibrary lib;
        if (!library_file.empty() && !LoadRegimentRadarsFile(data_dir + "/" + library_file, lib, &error)) return Fail("post.library", error);
        auto radar_of = [&](const json::Value& e, const std::string& rp, bool with_id, RegimentRadar& radar) {
            std::string id, type;
            if (!Keys(e, rp, {"id", "type", "position", "notes"}) || (with_id && !RequireString(e, rp, "id", id)) ||
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
                               "telecode_mhz", "telecode_return_mhz", "mast", "messages", "notes"}) ||
                !OptionalBool(*r, rp, "telecode", out.post_regiment.telecode) ||
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
            out.has_post_radar = true;
        }
        if (!LoadPostRadios(*p, data_dir, out, console)) return false;
        out.has_post = true;
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
    // "emcon" with "radiate" (false: the battery's radars silent; true: they may radiate again).
    bool LoadBatteryOrder(const json::Value& o, const std::string& p, const std::string& kind_text, BatteryOrder& out) {
        if (!ParseBatteryOrder(kind_text, out.kind)) {
            return Fail(p, "unknown battery order '" + kind_text +
                               "' (march, short_halt, combat_formation, halt, resume, ambush, river_crossing, far_bank_held, reload, refuel, emcon, relocate)");
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
            const json::Value* route = OptionalArray(o, p, "route");
            if (route == nullptr || route->items.size() < 2) return message_.empty() ? Fail(Join(p, "route"), "a march needs a route of 2 or more positions") : false;
            for (std::size_t i = 0; i < route->items.size(); ++i) {
                Vec3 v;
                if (!LoadPosition(route->items[i], Index(Join(p, "route"), i), frame_, v)) return false;
                out.route.push_back(v);
            }
        }
        if (out.kind == BatteryOrder::Kind::Relocate) {
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
    bool LoadBattery(const json::Value& root, const std::string& data_dir, ScenarioDef& out) {
        const json::Value* b = root.Find("battery");
        if (b == nullptr) return true;
        const std::string bp = "battery";
        if (!b->Is(json::Type::Object) ||
            !Keys(*b, bp, {"tactics", "drive_own", "night", "share_picture", "deployed", "own", "launchers", "vehicles", "orders", "notes"})) {
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
                if (!Keys(v, p, {"id", "callsign", "kind", "position", "sector_deg", "notes"}) || !RequireString(v, p, "id", d.id) ||
                    !OptionalString(v, p, "callsign", d.callsign) || !OptionalString(v, p, "kind", kind)) {
                    return false;
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
        if (const json::Value* os = OptionalArray(*b, bp, "orders")) {
            for (std::size_t i = 0; i < os->items.size(); ++i) {
                const std::string p = Index(Join(bp, "orders"), i);
                const json::Value& o = os->items[i];
                if (!Keys(o, p, {"at_s", "order", "route", "covered", "axis_deg", "bounding", "roving", "points", "crossing", "width_m",
                                 "vehicle", "radiate", "notes"})) {
                    return false;
                }
                std::string kind;
                BatteryOrder ord;
                if (!RequireString(o, p, "order", kind) || !OptionalNumber(o, p, "at_s", ord.at_s) ||
                    !LoadBatteryOrder(o, p, kind, ord)) {
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

    bool LoadEnd(const json::Value& root, ScenarioDef& out) {
        const json::Value* e = root.Find("end");
        if (e == nullptr) return true;
        if (!Keys(*e, "end", {"time_limit_s", "when_targets_gone"}) ||
            !OptionalNumber(*e, "end", "time_limit_s", out.time_limit_s) ||
            !OptionalBool(*e, "end", "when_targets_gone", out.end_when_targets_gone)) {
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
