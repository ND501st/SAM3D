// simcore/console_data.hpp — load a console (controls, equipment, procedures) from JSON.
//
// One file per system, in data/procedures/<system>.json (see data/procedures/README.md for
// the format). The loader is strict: unknown keys, missing sources, bad references and
// out-of-range values are errors, reported with the path to the offending item
// (e.g. "controls[4].guard: unknown control 'x'"). Nothing is partially loaded on failure.
#pragma once

#include <cmath>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/equipment.hpp"
#include "simcore/json.hpp"
#include "simcore/panel.hpp"
#include "simcore/procedures.hpp"

namespace simcore {

// How a console's radars and sight map onto its controls and equipment (the "sensors" block).
// Used by the gameplay engagement model (engagement.hpp). Ids refer to this console.
struct SearchSensorDef {
    std::string antenna;       // subsystem: rotating
    std::string transmitter;   // subsystem: radiating
    std::string range_control; // selector: one displayed range scale per position
    std::vector<std::pair<double, double>> range_scales_m;  // [min, max] per position
    std::string mti_control;   // switch: moving-target indicator (1 = on); may be empty
    double mti_max_range_m = 0.0;
};

struct TrackSensorDef {
    std::string transmitter;     // subsystem: tracking radar radiating
    std::string slew;            // subsystem: antenna free to turn
    std::string radar_mode;      // subsystem: radar acquisition mode engaged (e.g. ЦУ АЗ)
    std::string tv_camera;       // subsystem: TV camera on (may be empty)
    std::string tv_mode;         // subsystem: TV angle tracking engaged (may be empty)
    std::string range_only_mode; // subsystem: radar range-only tracking engaged (may be empty)
    std::string az_wheel;        // wheels, in degrees and metres
    std::string el_wheel;
    std::string range_wheel;
    std::string lock_control;    // lock that must be open before the aiming crew can turn the azimuth wheel
    std::string fov_control;     // TV lens selector: 0 wide, 1 narrow (may be empty)
    double fov_wide_deg = 0.0;
    double fov_narrow_deg = 0.0;
    double max_range_m = 0.0;
    double az_limit_deg = 360.0;  // wheel limits already enforce this; used to pick the reachable angle
    std::string captured_input;   // inputs the model sets (may be empty)
    std::string too_low_input;
    std::string near_zone_input;
    std::string in_zone_input;
    // Elevation search (optional; beam_width_deg 0 = absent): while `el_search_mode` is ready the antenna scans in
    // elevation through one search-radar beam, and the radar only captures when the antenna's beam is on the
    // target in elevation. `beam_by_position` gives the beam for each position of `beam_control`; -1 = the beam
    // the search radar sees the target under the azimuth line in.
    std::string el_search_mode;
    std::string beam_control;
    std::vector<std::pair<double, double>> beams_deg;
    std::vector<int> beam_by_position;
    double scan_period_s = 2.0;
    double beam_width_deg = 0.0;
};

// A crew seat that owns controls (e.g. "senior_operator"). Crew roles name these.
struct ConsoleStation {
    std::string id;
    std::string label;
};

struct ConsoleData {
    std::string system_id;
    std::string display_name;
    std::vector<ConsoleStation> stations;  // empty if the file lists none (then control stations are free text)
    ControlPanel panel;
    Equipment equipment;
    std::vector<Procedure> procedures;
    std::string launch_button;  // push-button that commands a launch (empty if the file has no "launch")
    std::string launch_armed;   // subsystem that must be ready for the button to launch
    std::string launch_restarts;  // subsystem restarted by each launch (the next missile's preparation); may be empty
    bool has_sensors = false;
    SearchSensorDef search;
    TrackSensorDef track;

    const Procedure* FindProcedure(const std::string& id) const {
        for (const Procedure& p : procedures) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }

    const ConsoleStation* FindStation(const std::string& id) const {
        for (const ConsoleStation& s : stations) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }
};

namespace detail {

class ConsoleLoader {
public:
    bool Load(const json::Value& root, ConsoleData& out) {
        if (!Keys(root, "", {"system_id", "display_name", "status", "notes", "sources", "stations", "inputs",
                             "controls", "subsystems", "procedures", "launch", "sensors"})) {
            return false;
        }
        if (!RequireString(root, "", "system_id", out.system_id)) return false;
        if (!RequireString(root, "", "display_name", out.display_name)) return false;
        if (!LoadSources(root)) return false;
        if (!LoadStations(root, out.stations)) return false;
        if (!LoadInputs(root, out.equipment)) return false;
        if (!LoadControls(root, out.stations, out.panel)) return false;
        if (!LoadSubsystems(root, out.panel, out.equipment)) return false;
        if (!LoadProcedures(root, out.panel, out.equipment, out.procedures)) return false;
        if (!LoadLaunch(root, out)) return false;
        return LoadSensors(root, out);
    }

    const std::string& Message() const { return message_; }

protected:  // the helpers are shared with the scenario loader (scenario.hpp)
    bool Fail(const std::string& path, const std::string& msg) {
        message_ = (path.empty() ? std::string() : path + ": ") + msg;
        return false;
    }

    static std::string Join(const std::string& path, const std::string& key) {
        return path.empty() ? key : path + "." + key;
    }

    static std::string Index(const std::string& path, std::size_t i) { return path + "[" + std::to_string(i) + "]"; }

    bool Keys(const json::Value& v, const std::string& path, std::initializer_list<const char*> allowed) {
        if (!v.Is(json::Type::Object)) return Fail(path, "expected an object");
        for (const std::string& k : v.keys) {
            bool ok = false;
            for (const char* a : allowed) ok = ok || k == a;
            if (!ok) return Fail(Join(path, k), "unknown key");
        }
        return true;
    }

    bool RequireString(const json::Value& obj, const std::string& path, const char* key, std::string& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        if (!v->Is(json::Type::String) || v->string.empty()) return Fail(Join(path, key), "expected a non-empty string");
        out = v->string;
        return true;
    }

    bool OptionalString(const json::Value& obj, const std::string& path, const char* key, std::string& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return true;
        if (!v->Is(json::Type::String)) return Fail(Join(path, key), "expected a string");
        out = v->string;
        return true;
    }

    bool OptionalBool(const json::Value& obj, const std::string& path, const char* key, bool& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return true;
        if (!v->Is(json::Type::Bool)) return Fail(Join(path, key), "expected true or false");
        out = v->boolean;
        return true;
    }

    bool OptionalNumber(const json::Value& obj, const std::string& path, const char* key, double& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return true;
        if (!v->Is(json::Type::Number)) return Fail(Join(path, key), "expected a number");
        out = v->number;
        return true;
    }

    bool OptionalInt(const json::Value& obj, const std::string& path, const char* key, int& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return true;
        if (!v->Is(json::Type::Number) || v->number != static_cast<double>(static_cast<int>(v->number))) {
            return Fail(Join(path, key), "expected a whole number");
        }
        out = static_cast<int>(v->number);
        return true;
    }

    const json::Value* OptionalArray(const json::Value& obj, const std::string& path, const char* key) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return nullptr;
        if (!v->Is(json::Type::Array)) {
            Fail(Join(path, key), "expected an array");
            return nullptr;
        }
        return v;
    }

    // Every data item says where it came from (data/systems/README.md).
    bool Provenance(const json::Value& obj, const std::string& path) {
        std::string source;
        std::string confidence;
        if (!RequireString(obj, path, "source", source)) return false;
        if (!RequireString(obj, path, "confidence", confidence)) return false;
        if (confidence != "high" && confidence != "medium" && confidence != "low" && confidence != "estimate") {
            return Fail(Join(path, "confidence"), "must be high, medium, low or estimate");
        }
        return true;
    }

    bool LoadSources(const json::Value& root) {
        const json::Value* arr = OptionalArray(root, "", "sources");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("sources", i);
            const json::Value& s = arr->items[i];
            if (!Keys(s, path, {"id", "title", "url", "notes"})) return false;
            std::string text;
            if (!RequireString(s, path, "id", text) || !RequireString(s, path, "title", text)) return false;
        }
        return true;
    }

    bool LoadInputs(const json::Value& root, Equipment& eq) {
        const json::Value* arr = OptionalArray(root, "", "inputs");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("inputs", i);
            const json::Value& in = arr->items[i];
            if (!Keys(in, path, {"id", "description", "source", "confidence", "notes"})) return false;
            std::string id;
            std::string text;
            if (!RequireString(in, path, "id", id) || !RequireString(in, path, "description", text)) return false;
            if (!Provenance(in, path)) return false;
            if (!eq.AddInput(id)) return Fail(Join(path, "id"), "duplicate input '" + id + "'");
        }
        return true;
    }

    bool LoadStations(const json::Value& root, std::vector<ConsoleStation>& out) {
        const json::Value* arr = OptionalArray(root, "", "stations");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("stations", i);
            const json::Value& st = arr->items[i];
            if (!Keys(st, path, {"id", "label", "source", "confidence", "notes"})) return false;
            ConsoleStation station;
            if (!RequireString(st, path, "id", station.id) || !RequireString(st, path, "label", station.label) ||
                !Provenance(st, path)) {
                return false;
            }
            for (const ConsoleStation& existing : out) {
                if (existing.id == station.id) return Fail(Join(path, "id"), "duplicate station '" + station.id + "'");
            }
            out.push_back(std::move(station));
        }
        return true;
    }

    bool LoadControls(const json::Value& root, const std::vector<ConsoleStation>& stations, ControlPanel& panel) {
        const json::Value* arr = OptionalArray(root, "", "controls");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("controls", i);
            const json::Value& c = arr->items[i];
            if (!Keys(c, path, {"id", "label", "station", "kind", "positions", "position_labels", "initial", "guard",
                                "min", "max", "initial_value", "wraps", "unit", "source", "confidence", "notes"})) {
                return false;
            }
            ControlDef def;
            std::string kind;
            if (!RequireString(c, path, "id", def.id) || !RequireString(c, path, "label", def.label) ||
                !RequireString(c, path, "kind", kind) || !OptionalString(c, path, "station", def.station) ||
                !OptionalString(c, path, "guard", def.guard_id) || !Provenance(c, path)) {
                return false;
            }
            if (!stations.empty()) {  // with a station list, every control belongs to one of its seats
                bool known = false;
                for (const ConsoleStation& st : stations) known = known || st.id == def.station;
                if (!known) {
                    return Fail(Join(path, "station"), def.station.empty() ? std::string("required")
                                                                           : "unknown station '" + def.station + "'");
                }
            }
            if (const json::Value* labels = OptionalArray(c, path, "position_labels")) {
                for (std::size_t j = 0; j < labels->items.size(); ++j) {
                    if (!labels->items[j].Is(json::Type::String)) {
                        return Fail(Index(Join(path, "position_labels"), j), "expected a string");
                    }
                    def.position_labels.push_back(labels->items[j].string);
                }
                def.positions = static_cast<int>(def.position_labels.size());
            } else if (!message_.empty()) {
                return false;
            }
            if (!OptionalInt(c, path, "positions", def.positions) || !OptionalInt(c, path, "initial", def.initial)) {
                return false;
            }
            if (kind == "switch" || kind == "key") {
                def.kind = ControlKind::Discrete;
                def.stepwise = kind == "key";
            } else if (kind == "button") {
                def.kind = ControlKind::Momentary;
            } else if (kind == "wheel") {
                def.kind = ControlKind::Continuous;
                if (c.Find("min") == nullptr || c.Find("max") == nullptr) return Fail(path, "a wheel needs min and max");
                if (!OptionalNumber(c, path, "min", def.min_value) || !OptionalNumber(c, path, "max", def.max_value) ||
                    !OptionalBool(c, path, "wraps", def.wraps)) {
                    return false;
                }
                def.initial_value = def.min_value;
                if (!OptionalNumber(c, path, "initial_value", def.initial_value)) return false;
            } else {
                return Fail(Join(path, "kind"), "must be switch, key, button or wheel");
            }
            if (def.kind != ControlKind::Continuous &&
                (c.Find("min") != nullptr || c.Find("max") != nullptr || c.Find("initial_value") != nullptr ||
                 c.Find("wraps") != nullptr)) {
                return Fail(path, "min, max, initial_value and wraps are for wheels only");
            }
            if (!def.guard_id.empty() && !panel.Has(def.guard_id)) {
                return Fail(Join(path, "guard"), "unknown control '" + def.guard_id + "' (define the guard first)");
            }
            if (panel.Has(def.id)) return Fail(Join(path, "id"), "duplicate control '" + def.id + "'");
            if (!panel.AddControl(def)) return Fail(path, "invalid control (check positions, initial, range and guard)");
        }
        return true;
    }

    bool LoadCondition(const json::Value& v, const std::string& path, Condition& out) {
        if (!Keys(v, path, {"control", "subsystem", "input", "is", "at_least"})) return false;
        const json::Value* is = v.Find("is");
        const json::Value* at_least = v.Find("at_least");
        const int kinds = (v.Find("control") != nullptr) + (v.Find("subsystem") != nullptr) + (v.Find("input") != nullptr);
        if (kinds != 1) return Fail(path, "needs exactly one of control, subsystem or input");
        if ((is != nullptr) == (at_least != nullptr)) return Fail(path, "needs exactly one of is or at_least");
        if (const json::Value* c = v.Find("control")) {
            if (!c->Is(json::Type::String)) return Fail(Join(path, "control"), "expected a string");
            const json::Value* n = is != nullptr ? is : at_least;
            if (!n->Is(json::Type::Number) || n->number != static_cast<double>(static_cast<int>(n->number))) {
                return Fail(Join(path, is != nullptr ? "is" : "at_least"), "expected a position number");
            }
            out = is != nullptr ? Condition::Control(c->string, static_cast<int>(n->number))
                                : Condition::AtLeast(c->string, static_cast<int>(n->number));
            return true;
        }
        if (at_least != nullptr) return Fail(Join(path, "at_least"), "only for controls");
        if (const json::Value* s = v.Find("subsystem")) {
            if (!s->Is(json::Type::String)) return Fail(Join(path, "subsystem"), "expected a string");
            if (!is->Is(json::Type::String)) return Fail(Join(path, "is"), "expected a state name");
            const std::string& st = is->string;
            SubsystemState state = SubsystemState::Off;
            if (st == "off") state = SubsystemState::Off;
            else if (st == "starting") state = SubsystemState::Starting;
            else if (st == "ready") state = SubsystemState::Ready;
            else if (st == "stopping") state = SubsystemState::Stopping;
            else if (st == "fault") state = SubsystemState::Fault;
            else return Fail(Join(path, "is"), "must be off, starting, ready, stopping or fault");
            out = Condition::Subsystem(s->string, state);
            return true;
        }
        const json::Value* in = v.Find("input");
        if (!in->Is(json::Type::String)) return Fail(Join(path, "input"), "expected a string");
        if (!is->Is(json::Type::Bool)) return Fail(Join(path, "is"), "expected true or false");
        out = Condition::Input(in->string, is->boolean);
        return true;
    }

    // Loads a condition list and checks that every reference exists (subsystems must be defined earlier).
    bool LoadConditions(const json::Value& obj, const std::string& path, const char* key, const ControlPanel& panel,
                        const Equipment& eq, std::vector<Condition>& out) {
        const json::Value* arr = OptionalArray(obj, path, key);
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index(Join(path, key), i);
            Condition c;
            if (!LoadCondition(arr->items[i], p, c)) return false;
            switch (c.kind) {
                case Condition::Kind::ControlEquals:
                case Condition::Kind::ControlAtLeast:
                    if (!panel.Has(c.id)) return Fail(p, "unknown control '" + c.id + "'");
                    if (!panel.IsValidPosition(c.id, c.position)) {
                        return Fail(p, "control '" + c.id + "' has no position " + std::to_string(c.position));
                    }
                    break;
                case Condition::Kind::SubsystemIs:
                    if (!eq.Has(c.id)) return Fail(p, "unknown subsystem '" + c.id + "' (define it earlier)");
                    break;
                case Condition::Kind::InputIs:
                    if (!eq.HasInput(c.id)) return Fail(p, "unknown input '" + c.id + "'");
                    break;
            }
            out.push_back(std::move(c));
        }
        return true;
    }

    bool LoadLaunch(const json::Value& root, ConsoleData& out) {
        const json::Value* l = root.Find("launch");
        if (l == nullptr) return true;
        if (!Keys(*l, "launch", {"button", "armed", "restarts", "source", "confidence", "notes"}) ||
            !OptionalString(*l, "launch", "restarts", out.launch_restarts) ||
            !RequireString(*l, "launch", "button", out.launch_button) ||
            !RequireString(*l, "launch", "armed", out.launch_armed) || !Provenance(*l, "launch")) {
            return false;
        }
        const ControlDef* b = out.panel.Find(out.launch_button);
        if (b == nullptr || b->kind != ControlKind::Momentary) return Fail("launch.button", "must be a button control");
        if (!out.equipment.Has(out.launch_armed)) return Fail("launch.armed", "unknown subsystem '" + out.launch_armed + "'");
        if (!out.launch_restarts.empty() && !out.equipment.Has(out.launch_restarts)) {
            return Fail("launch.restarts", "unknown subsystem '" + out.launch_restarts + "'");
        }
        return true;
    }

    bool RefSubsystem(const json::Value& obj, const std::string& path, const char* key, const Equipment& eq,
                      bool required, std::string& out) {
        if (required ? !RequireString(obj, path, key, out) : !OptionalString(obj, path, key, out)) return false;
        if (!out.empty() && !eq.Has(out)) return Fail(Join(path, key), "unknown subsystem '" + out + "'");
        return true;
    }

    bool RefControl(const json::Value& obj, const std::string& path, const char* key, const ControlPanel& panel,
                    ControlKind kind, bool required, std::string& out) {
        if (required ? !RequireString(obj, path, key, out) : !OptionalString(obj, path, key, out)) return false;
        if (out.empty()) return true;
        const ControlDef* c = panel.Find(out);
        if (c == nullptr) return Fail(Join(path, key), "unknown control '" + out + "'");
        if (c->kind != kind) return Fail(Join(path, key), "control '" + out + "' is the wrong kind");
        return true;
    }

    bool RefInput(const json::Value& obj, const std::string& path, const char* key, const Equipment& eq,
                  std::string& out) {
        if (!OptionalString(obj, path, key, out)) return false;
        if (!out.empty() && !eq.HasInput(out)) return Fail(Join(path, key), "unknown input '" + out + "'");
        return true;
    }

    bool LoadSensors(const json::Value& root, ConsoleData& out) {
        const json::Value* s = root.Find("sensors");
        if (s == nullptr) return true;
        if (!Keys(*s, "sensors", {"search", "track", "source", "confidence", "notes"}) || !Provenance(*s, "sensors")) {
            return false;
        }
        const ControlPanel& panel = out.panel;
        const Equipment& eq = out.equipment;
        const json::Value* se = s->Find("search");
        const json::Value* tr = s->Find("track");
        if (se == nullptr || tr == nullptr) return Fail("sensors", "needs search and track");
        const std::string sp = "sensors.search";
        SearchSensorDef& search = out.search;
        if (!Keys(*se, sp, {"antenna", "transmitter", "range_control", "range_scales_m", "mti_control",
                            "mti_max_range_m", "notes"}) ||
            !RefSubsystem(*se, sp, "antenna", eq, true, search.antenna) ||
            !RefSubsystem(*se, sp, "transmitter", eq, true, search.transmitter) ||
            !RefControl(*se, sp, "range_control", panel, ControlKind::Discrete, true, search.range_control) ||
            !RefControl(*se, sp, "mti_control", panel, ControlKind::Discrete, false, search.mti_control) ||
            !OptionalNumber(*se, sp, "mti_max_range_m", search.mti_max_range_m)) {
            return false;
        }
        const json::Value* scales = OptionalArray(*se, sp, "range_scales_m");
        if (scales == nullptr) return message_.empty() ? Fail(Join(sp, "range_scales_m"), "required") : false;
        for (std::size_t i = 0; i < scales->items.size(); ++i) {
            const json::Value& pair = scales->items[i];
            if (!pair.Is(json::Type::Array) || pair.items.size() != 2 || !pair.items[0].Is(json::Type::Number) ||
                !pair.items[1].Is(json::Type::Number) || !(pair.items[1].number > pair.items[0].number)) {
                return Fail(Index(Join(sp, "range_scales_m"), i), "expected [min, max] with max > min");
            }
            search.range_scales_m.emplace_back(pair.items[0].number, pair.items[1].number);
        }
        if (static_cast<int>(search.range_scales_m.size()) != panel.Find(search.range_control)->positions) {
            return Fail(Join(sp, "range_scales_m"), "needs one scale per position of " + search.range_control);
        }

        const std::string tp = "sensors.track";
        TrackSensorDef& track = out.track;
        if (!Keys(*tr, tp, {"transmitter", "slew", "radar_mode", "tv_camera", "tv_mode", "range_only_mode", "az_wheel",
                            "el_wheel", "range_wheel", "lock_control", "fov_control", "fov_deg", "max_range_m",
                            "az_limit_deg", "captured_input", "too_low_input", "near_zone_input", "in_zone_input",
                            "elevation_search", "notes"}) ||
            !RefSubsystem(*tr, tp, "transmitter", eq, true, track.transmitter) ||
            !RefSubsystem(*tr, tp, "slew", eq, true, track.slew) ||
            !RefSubsystem(*tr, tp, "radar_mode", eq, true, track.radar_mode) ||
            !RefSubsystem(*tr, tp, "tv_camera", eq, false, track.tv_camera) ||
            !RefSubsystem(*tr, tp, "tv_mode", eq, false, track.tv_mode) ||
            !RefSubsystem(*tr, tp, "range_only_mode", eq, false, track.range_only_mode) ||
            !RefControl(*tr, tp, "az_wheel", panel, ControlKind::Continuous, true, track.az_wheel) ||
            !RefControl(*tr, tp, "el_wheel", panel, ControlKind::Continuous, true, track.el_wheel) ||
            !RefControl(*tr, tp, "range_wheel", panel, ControlKind::Continuous, true, track.range_wheel) ||
            !RefControl(*tr, tp, "lock_control", panel, ControlKind::Discrete, false, track.lock_control) ||
            !RefControl(*tr, tp, "fov_control", panel, ControlKind::Discrete, false, track.fov_control) ||
            !OptionalNumber(*tr, tp, "max_range_m", track.max_range_m) ||
            !OptionalNumber(*tr, tp, "az_limit_deg", track.az_limit_deg) ||
            !RefInput(*tr, tp, "captured_input", eq, track.captured_input) ||
            !RefInput(*tr, tp, "too_low_input", eq, track.too_low_input) ||
            !RefInput(*tr, tp, "near_zone_input", eq, track.near_zone_input) ||
            !RefInput(*tr, tp, "in_zone_input", eq, track.in_zone_input)) {
            return false;
        }
        if (track.max_range_m <= 0.0) return Fail(Join(tp, "max_range_m"), "must be > 0");
        if (const json::Value* fov = tr->Find("fov_deg")) {
            if (!fov->Is(json::Type::Array) || fov->items.size() != 2 || !fov->items[0].Is(json::Type::Number) ||
                !fov->items[1].Is(json::Type::Number) || fov->items[0].number <= 0.0 || fov->items[1].number <= 0.0) {
                return Fail(Join(tp, "fov_deg"), "expected [wide, narrow] in degrees");
            }
            track.fov_wide_deg = fov->items[0].number;
            track.fov_narrow_deg = fov->items[1].number;
        }
        if (!track.tv_mode.empty() && (track.tv_camera.empty() || track.fov_wide_deg <= 0.0)) {
            return Fail(tp, "tv_mode needs tv_camera and fov_deg");
        }
        if (const json::Value* es = tr->Find("elevation_search")) {
            if (!LoadElevationSearch(*es, Join(tp, "elevation_search"), panel, eq, track)) return false;
        }
        out.has_sensors = true;
        return true;
    }

    bool LoadElevationSearch(const json::Value& es, const std::string& ep, const ControlPanel& panel,
                             const Equipment& eq, TrackSensorDef& track) {
        if (!Keys(es, ep, {"mode", "beam_control", "beams_deg", "beam_by_position", "scan_period_s", "beam_width_deg",
                           "source", "confidence", "notes"}) ||
            !Provenance(es, ep) || !RefSubsystem(es, ep, "mode", eq, true, track.el_search_mode) ||
            !RefControl(es, ep, "beam_control", panel, ControlKind::Discrete, true, track.beam_control) ||
            !OptionalNumber(es, ep, "scan_period_s", track.scan_period_s) ||
            !OptionalNumber(es, ep, "beam_width_deg", track.beam_width_deg)) {
            return false;
        }
        if (!(track.scan_period_s > 0.0)) return Fail(Join(ep, "scan_period_s"), "must be > 0");
        if (!(track.beam_width_deg > 0.0)) return Fail(Join(ep, "beam_width_deg"), "required, > 0");
        const json::Value* beams = OptionalArray(es, ep, "beams_deg");
        if (beams == nullptr) return message_.empty() ? Fail(Join(ep, "beams_deg"), "required") : false;
        for (std::size_t i = 0; i < beams->items.size(); ++i) {
            const json::Value& pair = beams->items[i];
            if (!pair.Is(json::Type::Array) || pair.items.size() != 2 || !pair.items[0].Is(json::Type::Number) ||
                !pair.items[1].Is(json::Type::Number) || !(pair.items[1].number > pair.items[0].number)) {
                return Fail(Index(Join(ep, "beams_deg"), i), "expected [low, high] in degrees");
            }
            track.beams_deg.emplace_back(pair.items[0].number, pair.items[1].number);
        }
        if (track.beams_deg.empty()) return Fail(Join(ep, "beams_deg"), "needs at least one beam");
        const json::Value* map = OptionalArray(es, ep, "beam_by_position");
        if (map == nullptr) return message_.empty() ? Fail(Join(ep, "beam_by_position"), "required") : false;
        if (static_cast<int>(map->items.size()) != panel.Find(track.beam_control)->positions) {
            return Fail(Join(ep, "beam_by_position"), "needs one entry per position of " + track.beam_control);
        }
        const double beam_count = static_cast<double>(track.beams_deg.size());
        for (std::size_t i = 0; i < map->items.size(); ++i) {
            const json::Value& v = map->items[i];
            if (!v.Is(json::Type::Number) || v.number != std::floor(v.number) || v.number < -1.0 ||
                v.number >= beam_count) {
                return Fail(Index(Join(ep, "beam_by_position"), i), "must be -1 or a beam index");
            }
            track.beam_by_position.push_back(static_cast<int>(v.number));
        }
        return true;
    }

    bool LoadTime(const json::Value& obj, const std::string& path, const char* key, double& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return true;
        const std::string p = Join(path, key);
        if (!Keys(*v, p, {"value", "source", "confidence", "notes"})) return false;
        const json::Value* n = v->Find("value");
        if (n == nullptr || !n->Is(json::Type::Number) || n->number < 0.0) return Fail(Join(p, "value"), "expected a number >= 0");
        if (!Provenance(*v, p)) return false;
        out = n->number;
        return true;
    }

    bool LoadSubsystems(const json::Value& root, const ControlPanel& panel, Equipment& eq) {
        const json::Value* arr = OptionalArray(root, "", "subsystems");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("subsystems", i);
            const json::Value& s = arr->items[i];
            if (!Keys(s, path, {"id", "name", "enable", "enable_any", "prerequisites", "latching", "release",
                                "start_ready", "start_time_s", "stop_time_s", "source", "confidence", "notes"})) {
                return false;
            }
            SubsystemDef def;
            if (!RequireString(s, path, "id", def.id) || !RequireString(s, path, "name", def.name) ||
                !Provenance(s, path) || !OptionalBool(s, path, "latching", def.latching) ||
                !OptionalBool(s, path, "start_ready", def.start_ready) ||
                !LoadConditions(s, path, "enable", panel, eq, def.enable) ||
                !LoadConditions(s, path, "enable_any", panel, eq, def.enable_any) ||
                !LoadConditions(s, path, "prerequisites", panel, eq, def.prerequisites) ||
                !LoadConditions(s, path, "release", panel, eq, def.release) ||
                !LoadTime(s, path, "start_time_s", def.start_time_s) || !LoadTime(s, path, "stop_time_s", def.stop_time_s)) {
                return false;
            }
            if (eq.Has(def.id)) return Fail(Join(path, "id"), "duplicate subsystem '" + def.id + "'");
            if (def.latching && def.release.empty()) return Fail(path, "a latching subsystem needs release conditions");
            if (!def.latching && !def.release.empty()) return Fail(Join(path, "release"), "only for latching subsystems");
            if (!eq.AddSubsystem(def, panel)) return Fail(path, "invalid subsystem");
        }
        return true;
    }

    bool LoadProcedures(const json::Value& root, const ControlPanel& panel, const Equipment& eq,
                        std::vector<Procedure>& out) {
        const json::Value* arr = OptionalArray(root, "", "procedures");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("procedures", i);
            const json::Value& p = arr->items[i];
            if (!Keys(p, path, {"id", "name", "steps", "source", "confidence", "notes"})) return false;
            Procedure proc;
            if (!RequireString(p, path, "id", proc.id) || !RequireString(p, path, "name", proc.name) ||
                !Provenance(p, path)) {
                return false;
            }
            for (const Procedure& existing : out) {
                if (existing.id == proc.id) return Fail(Join(path, "id"), "duplicate procedure '" + proc.id + "'");
            }
            const json::Value* steps = OptionalArray(p, path, "steps");
            if (steps == nullptr || steps->items.empty()) {
                return message_.empty() ? Fail(Join(path, "steps"), "needs at least one step") : false;
            }
            for (std::size_t j = 0; j < steps->items.size(); ++j) {
                const std::string sp = Index(Join(path, "steps"), j);
                const json::Value& st = steps->items[j];
                if (!Keys(st, sp, {"control", "position", "description", "wait_for", "min_wait_s", "notes"})) return false;
                ProcedureStep step;
                if (!RequireString(st, sp, "control", step.control_id) ||
                    !RequireString(st, sp, "description", step.description)) {
                    return false;
                }
                if (st.Find("position") == nullptr) return Fail(Join(sp, "position"), "required");
                if (!OptionalInt(st, sp, "position", step.value) ||
                    !OptionalNumber(st, sp, "min_wait_s", step.min_wait_s) ||
                    !LoadConditions(st, sp, "wait_for", panel, eq, step.wait_for)) {
                    return false;
                }
                if (!panel.IsValidPosition(step.control_id, step.value)) {
                    return Fail(sp, "control '" + step.control_id + "' is unknown, a wheel, or has no position " +
                                        std::to_string(step.value));
                }
                if (step.min_wait_s < 0.0) return Fail(Join(sp, "min_wait_s"), "must be >= 0");
                // One seat per procedure: a crew member can only reach the controls of their own station.
                const std::string& station = StationOf(panel, step.control_id);
                if (!station.empty()) {
                    if (proc.station.empty()) {
                        proc.station = station;
                    } else if (station != proc.station) {
                        return Fail(sp, "control '" + step.control_id + "' is at station '" + station +
                                            "', the procedure's earlier steps at '" + proc.station +
                                            "'; split it into one procedure per station");
                    }
                }
                proc.steps.push_back(std::move(step));
            }
            out.push_back(std::move(proc));
        }
        return true;
    }

    static const std::string& StationOf(const ControlPanel& panel, const std::string& control_id) {
        static const std::string none;
        for (const ControlDef& c : panel.Controls()) {
            if (c.id == control_id) return c.station;
        }
        return none;
    }

    std::string message_;
};

}  // namespace detail

// Parse and validate a console description. On failure `out` is left unchanged.
inline bool LoadConsole(const std::string& json_text, ConsoleData& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    ConsoleData loaded;
    detail::ConsoleLoader loader;
    if (!loader.Load(root, loaded)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(loaded);
    return true;
}

inline bool LoadConsoleFile(const std::string& path, ConsoleData& out, std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    if (!LoadConsole(text.str(), out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

}  // namespace simcore
