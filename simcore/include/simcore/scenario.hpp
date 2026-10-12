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

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/c2_sources.hpp"
#include "simcore/console_data.hpp"
#include "simcore/em_link.hpp"
#include "simcore/engagement.hpp"
#include "simcore/pu12.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct ScenarioCrew {
    std::string name;
    std::string role;
    double reaction_time_s = 1.0;
    double reaction_jitter_s = 0.0;  // each run draws reaction_time_s ± this from the scenario seed
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
};

struct TriggerAction {
    enum class Kind { SetInput, Order, Queue, End, C2Source, C2Fail, C2Restore };
    Kind kind = Kind::SetInput;
    std::string input;      // SetInput
    bool value = true;      // SetInput
    std::string crew;       // Order, Queue
    std::string procedure;  // Order, Queue
    std::string reason;     // End, C2Fail
    std::string c2_id;      // C2Source (a source id or "none"), C2Fail, C2Restore
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

struct ScenarioDef {
    std::string id;
    std::string name;
    std::string description;
    unsigned seed = 1;
    GeoPoint origin;
    double ground_height_m = 0.0;  // flat terrain (MSL); terrain grids come with Cesium
    std::string console_file;      // relative to the data folder, e.g. "procedures/osa_ak.json"
    std::string tuning_file;       // gameplay tuning, e.g. "gameplay/osa_ak.json"; empty = no engagement model
    EngagementTuning tuning;
    double fuel_fraction = 1.0;    // the tank's starting level, 0 to 1 (own_unit.fuel_fraction)
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
    double time_limit_s = 600.0;
    bool end_when_targets_gone = true;  // end once every target is destroyed or past its last waypoint
};

namespace detail {

class ScenarioLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, const std::string& data_dir, ScenarioDef& out, ConsoleData& console) {
        if (!Keys(root, "", {"scenario_id", "name", "description", "source", "confidence", "notes", "seed", "origin",
                             "terrain", "command_post", "c2", "own_unit", "crew", "targets", "triggers", "end"})) {
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
        if (!LoadCrew(root, out, console)) return false;
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
        if (!RequireObject(root, "", "terrain", t) || !Keys(*t, "terrain", {"type", "height_m"})) return false;
        std::string type;
        if (!RequireString(*t, "terrain", "type", type)) return false;
        if (type != "flat") return Fail("terrain.type", "only \"flat\" is supported so far");
        return RequireNumber(*t, "terrain", "height_m", out.ground_height_m);
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
            !Keys(*u, "own_unit", {"console", "tuning", "em", "readiness", "fuel_fraction", "position", "heading_deg", "inputs"})) {
            return false;
        }
        if (!RequireString(*u, "own_unit", "console", out.console_file)) return false;
        std::string error;
        if (!LoadConsoleFile(data_dir + "/" + out.console_file, console, &error)) {
            return Fail("own_unit.console", error);
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
                !Keys(*em, "own_unit.em", {"library", "search_radar", "track_radar", "extra_libraries", "wind_mps",
                                           "jammers", "chaff", "notes"}) ||
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
            out.has_em = true;
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

    bool LoadCrew(const json::Value& root, ScenarioDef& out, const ConsoleData& console) {
        const json::Value* arr = OptionalArray(root, "", "crew");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("crew", i);
            const json::Value& c = arr->items[i];
            ScenarioCrew crew;
            if (!Keys(c, p, {"name", "role", "reaction_time_s", "reaction_jitter_s"}) ||
                !RequireString(c, p, "name", crew.name) || !RequireString(c, p, "role", crew.role) ||
                !OptionalNumber(c, p, "reaction_time_s", crew.reaction_time_s) ||
                !OptionalNumber(c, p, "reaction_jitter_s", crew.reaction_jitter_s)) {
                return false;
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

    bool LoadTargets(const json::Value& root, ScenarioDef& out) {
        const json::Value* arr = OptionalArray(root, "", "targets");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("targets", i);
            const json::Value& t = arr->items[i];
            if (!Keys(t, p, {"id", "name", "type", "class", "start", "waypoints", "speed_mps", "max_turn_rate_deg_s",
                             "rcs_m2", "appear_at_s", "evade_on_launch", "evade_turn_rate_deg_s", "iff", "board_type",
                             "formation", "notes"})) {
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
            for (std::size_t j = 0; j < wps->items.size(); ++j) {
                Vec3 wp;
                if (!LoadPosition(wps->items[j], Index(Join(p, "waypoints"), j), frame_, wp)) return false;
                tg.waypoints.push_back(wp);
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
        if (!Keys(a, p, {"set_input", "value", "order", "queue", "crew", "end", "c2_source", "c2_fail", "c2_restore", "reason"})) {
            return false;
        }
        const int kinds = (a.Find("set_input") != nullptr) + (a.Find("order") != nullptr) + (a.Find("queue") != nullptr) +
                          (a.Find("end") != nullptr) + (a.Find("c2_source") != nullptr) + (a.Find("c2_fail") != nullptr) +
                          (a.Find("c2_restore") != nullptr);
        if (kinds != 1) return Fail(p, "needs exactly one of set_input, order, queue, end, c2_source, c2_fail or c2_restore");
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
                             "heard_life_s", "sources", "notes"})) {
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
                if (!Keys(e, p, {"id", "type", "name", "position", "net", "radio", "link_range_m", "outages", "notes"}) ||
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
