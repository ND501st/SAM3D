// simcore/console_data.hpp — load a console (controls, equipment, procedures) from JSON.
//
// One file per system, in data/procedures/<system>.json (see data/procedures/README.md for
// the format). The loader is strict: unknown keys, missing sources, bad references and
// out-of-range values are errors, reported with the path to the offending item
// (e.g. "controls[4].guard: unknown control 'x'"). Nothing is partially loaded on failure.
#pragma once

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

struct ConsoleData {
    std::string system_id;
    std::string display_name;
    ControlPanel panel;
    Equipment equipment;
    std::vector<Procedure> procedures;
    std::string launch_button;  // push-button that commands a launch (empty if the file has no "launch")
    std::string launch_armed;   // subsystem that must be ready for the button to launch

    const Procedure* FindProcedure(const std::string& id) const {
        for (const Procedure& p : procedures) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
};

namespace detail {

class ConsoleLoader {
public:
    bool Load(const json::Value& root, ConsoleData& out) {
        if (!Keys(root, "", {"system_id", "display_name", "status", "notes", "sources", "inputs", "controls",
                             "subsystems", "procedures", "launch"})) {
            return false;
        }
        if (!RequireString(root, "", "system_id", out.system_id)) return false;
        if (!RequireString(root, "", "display_name", out.display_name)) return false;
        if (!LoadSources(root)) return false;
        if (!LoadInputs(root, out.equipment)) return false;
        if (!LoadControls(root, out.panel)) return false;
        if (!LoadSubsystems(root, out.panel, out.equipment)) return false;
        if (!LoadProcedures(root, out.panel, out.equipment, out.procedures)) return false;
        return LoadLaunch(root, out);
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

    bool LoadControls(const json::Value& root, ControlPanel& panel) {
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
        if (!Keys(*l, "launch", {"button", "armed", "source", "confidence", "notes"}) ||
            !RequireString(*l, "launch", "button", out.launch_button) ||
            !RequireString(*l, "launch", "armed", out.launch_armed) || !Provenance(*l, "launch")) {
            return false;
        }
        const ControlDef* b = out.panel.Find(out.launch_button);
        if (b == nullptr || b->kind != ControlKind::Momentary) return Fail("launch.button", "must be a button control");
        if (!out.equipment.Has(out.launch_armed)) return Fail("launch.armed", "unknown subsystem '" + out.launch_armed + "'");
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
                                "start_time_s", "stop_time_s", "source", "confidence", "notes"})) {
                return false;
            }
            SubsystemDef def;
            if (!RequireString(s, path, "id", def.id) || !RequireString(s, path, "name", def.name) ||
                !Provenance(s, path) || !OptionalBool(s, path, "latching", def.latching) ||
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
                proc.steps.push_back(std::move(step));
            }
            out.push_back(std::move(proc));
        }
        return true;
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
