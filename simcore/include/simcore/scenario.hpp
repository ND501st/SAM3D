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
#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/crew_training.hpp"
#include "simcore/em_link.hpp"
#include "simcore/engagement.hpp"
#include "simcore/position_prep.hpp"
#include "simcore/pu12.hpp"
#include "simcore/target_behaviour.hpp"
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
    std::string target_class = "default";  // picks the hit chance in the gameplay tuning
    bool evade_on_launch = false;          // breaks away onto a beam heading when a missile is launched at it
    double evade_turn_rate_rad_s = DegToRad(12.0);
    bool friendly = false;                 // "iff": "friend": replies to IFF interrogation
    int board_type = -1;                   // its type on the PU-12 board (0-9; -1: 1 if a friend, else 9, no IFF)
    int formation = 1;                     // aircraft in its formation, as the board notes it
    TargetBehaviourDef behaviour;          // terrain following, pop-up (target_behaviour.hpp)
    EvadeDef evade;                        // evasion on launch or lock (takes the place of evade_on_launch)
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
    std::string console_file;      // relative to the data folder, e.g. "procedures/osa_ak.json"
    std::string crew_file;         // the crew data (own_unit.crew_data, or data/crew/<system>_crew.json when it exists)
    bool has_crew_data = false;
    CrewData crew_data;
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
    Vec3 unit_position;            // ENU [m]
    double unit_heading_deg = 0.0; // the hull's true heading, clockwise from north
    std::vector<std::pair<std::string, bool>> initial_inputs;
    std::vector<ScenarioCrew> crew;
    std::vector<ScenarioTarget> targets;
    std::vector<ScenarioTrigger> triggers;
    std::string command_post_file;  // the battery command post's plotting board, e.g. "command/pu12.json"; "" = none
    PlottingBoardDef board;
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
    double time_limit_s = 600.0;
    bool end_when_targets_gone = true;  // end once every target is destroyed or past its last waypoint
    // "crew_commander": "npc": the vehicle commander is an NPC who runs the crew by doctrine (roadmap 8.6,
    // docs/crew/07_COMMANDER_MODE.md §4) instead of the scenario's triggers giving every order. "script" (default): the
    // triggers give the orders.
    bool npc_commander = false;
    // "battery_commander": "npc" (default) distributes the PU-12's targets to the battery's other launchers; "player": the
    // player does (ScenarioRunner::AssignBatteryTarget); "none": they fire freely at what is in their reach.
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
                             "terrain", "command_post", "c2", "battery", "own_unit", "crew", "crew_commander", "battery_commander", "targets",
                             "triggers", "end", "assistance"})) {
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
        if (!LoadOrigin(root, out) || !LoadTerrain(root, out)) return false;
        if (!LoadOwnUnit(root, data_dir, out, console)) return false;
        if (!OptionalString(root, "", "command_post", out.command_post_file)) return false;
        if (!out.command_post_file.empty()) {
            std::string error;
            if (!LoadPlottingBoardFile(data_dir + "/" + out.command_post_file, out.board, &error)) {
                return Fail("command_post", error);
            }
        }
        if (!LoadC2(root, data_dir, out)) return false;
        if (!LoadBattery(root, data_dir, out)) return false;
        if (!LoadCrew(root, data_dir, out, console)) return false;
        if (const json::Value* cc = root.Find("crew_commander")) {
            if (!cc->Is(json::Type::String) || (cc->string != "npc" && cc->string != "script")) {
                return Fail("crew_commander", "must be \"npc\" or \"script\"");
            }
            out.npc_commander = cc->string == "npc";
        }
        if (const json::Value* bc = root.Find("battery_commander")) {
            if (!bc->Is(json::Type::String) || (bc->string != "npc" && bc->string != "player" && bc->string != "none")) {
                return Fail("battery_commander", "must be \"npc\", \"player\" or \"none\"");
            }
            out.battery_commander = bc->string;
        }
        if (const json::Value* as = root.Find("assistance")) {
            if (!as->Is(json::Type::String) || !ParseAssistLevel(as->string, out.assistance)) {
                return Fail("assistance", "must be \"demonstrate\", \"guided\", \"hint\", \"unassisted\" or \"exam\"");
            }
        }
        if (!LoadTargets(root, out)) return false;
        if (!LoadTriggers(root, out, console)) return false;
        return LoadEnd(root, out);
    }

private:
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

    bool LoadTerrain(const json::Value& root, ScenarioDef& out) {
        const json::Value* t = nullptr;
        if (!RequireObject(root, "", "terrain", t) || !Keys(*t, "terrain", {"type", "height_m", "ridges", "notes"})) return false;
        std::string type;
        if (!RequireString(*t, "terrain", "type", type)) return false;
        if (type != "flat") return Fail("terrain.type", "only \"flat\" is supported so far");
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
                                   "position", "heading_deg", "inputs"})) {
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
        if (const json::Value* in = u->Find("inputs")) {
            if (!in->Is(json::Type::Object)) return Fail("own_unit.inputs", "expected an object of input: true/false");
            for (std::size_t i = 0; i < in->keys.size(); ++i) {
                const std::string p = Join("own_unit.inputs", in->keys[i]);
                if (!console.equipment.HasInput(in->keys[i])) return Fail(p, "unknown input");
                if (!in->items[i].Is(json::Type::Bool)) return Fail(p, "expected true or false");
                out.initial_inputs.emplace_back(in->keys[i], in->items[i].boolean);
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

    bool LoadTargets(const json::Value& root, ScenarioDef& out) {
        const json::Value* arr = OptionalArray(root, "", "targets");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("targets", i);
            const json::Value& t = arr->items[i];
            if (!Keys(t, p, {"id", "name", "type", "class", "start", "waypoints", "speed_mps", "max_turn_rate_deg_s",
                             "rcs_m2", "appear_at_s", "evade_on_launch", "evade_turn_rate_deg_s", "iff", "board_type",
                             "formation", "behaviour", "evade", "notes"})) {
                return false;
            }
            ScenarioTarget tg;
            double turn_deg_s = RadToDeg(tg.max_turn_rate_rad_s);
            double evade_deg_s = RadToDeg(tg.evade_turn_rate_rad_s);
            if (!OptionalString(t, p, "class", tg.target_class) ||
                !OptionalBool(t, p, "evade_on_launch", tg.evade_on_launch) || !LoadIff(t, p, tg.friendly) ||
                !OptionalNumber(t, p, "evade_turn_rate_deg_s", evade_deg_s)) {
                return false;
            }
            if (evade_deg_s <= 0.0) return Fail(Join(p, "evade_turn_rate_deg_s"), "must be > 0");
            if (!OptionalInt(t, p, "board_type", tg.board_type) || !OptionalInt(t, p, "formation", tg.formation)) return false;
            if (t.Find("board_type") != nullptr && (tg.board_type < 0 || tg.board_type > 9)) {
                return Fail(Join(p, "board_type"), "a digit 0-9 (0 jamming, 1 friend, 7 practice, 8 enemy, 9 no IFF...)");
            }
            if (tg.formation < 1) return Fail(Join(p, "formation"), "must be >= 1");
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
                         "crossing", "width_m", "vehicle"})) {
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
    // "river_crossing" with "crossing", "axis_deg" and "width_m", "far_bank_held", "reload" / "refuel" with "vehicle".
    bool LoadBatteryOrder(const json::Value& o, const std::string& p, const std::string& kind_text, BatteryOrder& out) {
        if (!ParseBatteryOrder(kind_text, out.kind)) {
            return Fail(p, "unknown battery order '" + kind_text +
                               "' (march, short_halt, combat_formation, halt, resume, ambush, river_crossing, far_bank_held, reload, refuel)");
        }
        if (!OptionalBool(o, p, "bounding", out.bounding) || !OptionalBool(o, p, "roving", out.roving) ||
            !OptionalNumber(o, p, "width_m", out.width_m) || !OptionalString(o, p, "vehicle", out.vehicle)) {
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
            !Keys(*b, bp, {"tactics", "drive_own", "night", "share_picture", "own", "vehicles", "orders", "notes"})) {
            return message_.empty() ? Fail(bp, "expected an object") : false;
        }
        std::string tactics = "command/battery_tactics.json";
        bool share = false;
        if (!OptionalString(*b, bp, "tactics", tactics) || !OptionalBool(*b, bp, "drive_own", out.battery_drive_own) ||
            !OptionalBool(*b, bp, "night", out.battery_night) || !OptionalBool(*b, bp, "share_picture", share)) {
            return false;
        }
        std::string error;
        if (!LoadBatteryTacticsFile(data_dir + "/" + tactics, out.battery_tactics, &error)) return Fail(Join(bp, "tactics"), error);
        BatteryVehicleDef own;
        own.id = "own";
        own.callsign = "own";
        own.own = true;
        own.start = out.unit_position;
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
                if (!Keys(v, p, {"id", "callsign", "kind", "position", "notes"}) || !RequireString(v, p, "id", d.id) ||
                    !OptionalString(v, p, "callsign", d.callsign) || !OptionalString(v, p, "kind", kind)) {
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
                                 "vehicle", "notes"})) {
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
