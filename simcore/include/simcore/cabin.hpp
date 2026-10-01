// simcore/cabin.hpp — where a console's controls and lamps sit inside the vehicle (the 3D cabin).
//
// A cabin layout (data/cabins/<system>.json, format in data/cabins/README.md) places
// operator stations, panels, controls and lamps in the vehicle's body frame. It is
// presentation data: the console file (console_data.hpp) still decides what every
// control does. The layout only says where things are, how they look, and when a
// lamp is lit, so the engine can build the cabin and the player can reach the
// controls the crew uses.
//
// Body frame: metres, x = right, y = forward, z = up, origin on the ground under the
// vehicle's reference point. A panel is a flat rectangle: `facing` is the heading its
// front looks towards (clockwise from forward, so an operator looking forward faces a
// panel with facing 180 deg), `tilt` leans it back from vertical (90 deg = a desk).
// Items sit at (u, v) on the panel from its centre: u to the operator's right, v up
// the face.
//
// Displays are the instruments the crew reads: the search radar's scope ("ppi"), the
// tracking radar's range indicator ("range_scope"), the TV sight's monitor ("tv"), and
// needle instruments showing a control's value ("dial" turns, "meter" swings). Each
// has conditions for when it is powered and (dials) when its backlight is on.
//
// Loading checks every reference against the console: each control placed at most
// once, on a panel of its own station, inside the panel. A guard (cover or lock) that
// isn't placed itself is placed over the control it guards.
#pragma once

#include <cmath>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// How a control looks and how a click moves it.
enum class CabinStyle { Toggle, Rotary, Key, Button, Wheel, Cover };

inline const char* ToString(CabinStyle s) {
    switch (s) {
        case CabinStyle::Toggle: return "toggle";
        case CabinStyle::Rotary: return "rotary";
        case CabinStyle::Key: return "key";
        case CabinStyle::Button: return "button";
        case CabinStyle::Wheel: return "wheel";
        case CabinStyle::Cover: return "cover";
    }
    return "?";
}

// Default look for a control: buttons and wheels by kind, a control that guards another is a cover,
// stepwise controls are keys, more than two positions is a rotary, anything else a toggle.
inline CabinStyle DefaultCabinStyle(const ControlDef& def, const ControlPanel& panel) {
    if (def.kind == ControlKind::Momentary) return CabinStyle::Button;
    if (def.kind == ControlKind::Continuous) return CabinStyle::Wheel;
    for (const ControlDef& other : panel.Controls()) {
        if (other.guard_id == def.id && other.kind != ControlKind::Continuous) return CabinStyle::Cover;
    }
    if (def.stepwise) return CabinStyle::Key;
    return def.positions > 2 ? CabinStyle::Rotary : CabinStyle::Toggle;
}

// The position a click asks for: two-position controls flip, others step by `direction` (+1 or -1) and stop
// at the ends, buttons press. -1 for wheels (they turn, they aren't clicked).
inline int CabinClickTarget(const ControlDef& def, int current, int direction) {
    if (def.kind == ControlKind::Continuous) return -1;
    if (def.kind == ControlKind::Momentary) return 1;
    if (def.positions == 2) return 1 - current;
    const int target = current + (direction >= 0 ? 1 : -1);
    if (target < 0) return 0;
    return target >= def.positions ? def.positions - 1 : target;
}

struct CabinStation {
    std::string id;           // a station id used by the console's controls, e.g. "range_officer"
    std::string label;
    Vec3 eye_m;               // body frame
    double yaw_rad = 0.0;     // view heading, clockwise from forward
    double pitch_rad = 0.0;   // view pitch, up positive
};

struct CabinPanel {
    std::string id;
    std::string label;
    std::string station;
    Vec3 centre_m;            // body frame
    double facing_rad = 0.0;  // heading the front looks towards, clockwise from forward
    double tilt_rad = 0.0;    // leaned back from vertical
    double width_m = 0.0;
    double height_m = 0.0;

    // Unit vectors in the body frame: the face's normal, the operator's right along the face, and up the face.
    Vec3 Normal() const {
        const Vec3 h{std::sin(facing_rad), std::cos(facing_rad), 0.0};
        return h * std::cos(tilt_rad) + Vec3{0.0, 0.0, 1.0} * std::sin(tilt_rad);
    }
    Vec3 Right() const { return Vec3{-std::cos(facing_rad), std::sin(facing_rad), 0.0}; }
    Vec3 Up() const { return Cross(Normal(), Right()); }

    Vec3 PointAt(double u_m, double v_m) const { return centre_m + Right() * u_m + Up() * v_m; }
    bool Contains(double u_m, double v_m) const {
        return std::fabs(u_m) <= 0.5 * width_m + 1e-9 && std::fabs(v_m) <= 0.5 * height_m + 1e-9;
    }
};

struct CabinControl {
    std::string control_id;
    std::string panel_id;
    double u_m = 0.0;
    double v_m = 0.0;
    CabinStyle style = CabinStyle::Toggle;
    double units_per_turn = 360.0;  // wheels: value change for one full turn of the hand wheel
    bool auto_placed = false;       // a guard placed over the control it guards
};

struct CabinLamp {
    std::string id;
    std::string label;
    std::string panel_id;
    double u_m = 0.0;
    double v_m = 0.0;
    std::string color;                   // "green", "red", "amber" or "white"
    std::vector<Condition> lit_when;     // all must hold
    std::vector<Condition> lit_when_any; // if not empty, at least one must also hold
};

enum class CabinDisplayKind { Ppi, RangeScope, Tv, Dial, Meter };

inline const char* ToString(CabinDisplayKind k) {
    switch (k) {
        case CabinDisplayKind::Ppi: return "ppi";
        case CabinDisplayKind::RangeScope: return "range_scope";
        case CabinDisplayKind::Tv: return "tv";
        case CabinDisplayKind::Dial: return "dial";
        case CabinDisplayKind::Meter: return "meter";
    }
    return "?";
}

struct CabinDisplay {
    std::string id;
    std::string label;
    std::string panel_id;
    CabinDisplayKind kind = CabinDisplayKind::Dial;
    double u_m = 0.0;  // centre on the panel
    double v_m = 0.0;
    double width_m = 0.0;
    double height_m = 0.0;
    std::vector<Condition> on_when;      // powered: all must hold (empty = always)
    std::vector<Condition> on_when_any;  // if not empty, at least one must also hold
    std::vector<Condition> lit_when;     // dial backlight: all must hold (empty = lit whenever powered)
    // dial and meter
    std::string control;                 // the control whose value the needle shows
    int marks = 12;                      // labelled divisions on the scale
    // dial: one needle revolution = value_per_turn; the scale reads 0..labels_per_turn around it
    double value_per_turn = 360.0;
    double labels_per_turn = 360.0;
    // meter: reading = value * scale, full deflection at full_scale, in `unit`
    double scale = 1.0;
    double full_scale = 1.0;
    std::string unit;
    // ppi
    double sweep_period_s = 2.0;
    double afterglow_s = 3.0;
    // range_scope: span of the magnified lower sweep, centred on the range gate
    double magnified_window_m = 1500.0;
};

struct CabinLayout {
    std::string system_id;
    std::vector<CabinStation> stations;
    std::vector<CabinPanel> panels;
    std::vector<CabinControl> controls;
    std::vector<CabinLamp> lamps;
    std::vector<CabinDisplay> displays;

    const CabinStation* FindStation(const std::string& id) const {
        for (const CabinStation& s : stations) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }
    const CabinPanel* FindPanel(const std::string& id) const {
        for (const CabinPanel& p : panels) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
    const CabinControl* FindControl(const std::string& control_id) const {
        for (const CabinControl& c : controls) {
            if (c.control_id == control_id) return &c;
        }
        return nullptr;
    }

    // Body-frame position of a placed control or lamp.
    Vec3 PositionOf(const CabinControl& c) const { return FindPanel(c.panel_id)->PointAt(c.u_m, c.v_m); }
    Vec3 PositionOf(const CabinLamp& l) const { return FindPanel(l.panel_id)->PointAt(l.u_m, l.v_m); }

    static bool LampLit(const CabinLamp& lamp, const ControlPanel& panel, const Equipment& equipment) {
        if (!equipment.AllHold(lamp.lit_when, panel)) return false;
        return lamp.lit_when_any.empty() || equipment.AnyHolds(lamp.lit_when_any, panel);
    }

    static bool DisplayOn(const CabinDisplay& d, const ControlPanel& panel, const Equipment& equipment) {
        if (!equipment.AllHold(d.on_when, panel)) return false;
        return d.on_when_any.empty() || equipment.AnyHolds(d.on_when_any, panel);
    }
    static bool DisplayLit(const CabinDisplay& d, const ControlPanel& panel, const Equipment& equipment) {
        return DisplayOn(d, panel, equipment) && equipment.AllHold(d.lit_when, panel);
    }

    // Console controls the layout doesn't place (a cabin should usually place them all).
    std::vector<std::string> UnplacedControls(const ControlPanel& panel) const {
        std::vector<std::string> out;
        for (const ControlDef& def : panel.Controls()) {
            if (FindControl(def.id) == nullptr) out.push_back(def.id);
        }
        return out;
    }
};

namespace detail {

class CabinLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, const ConsoleData& console, CabinLayout& out) {
        if (!Keys(root, "", {"system_id", "status", "notes", "stations", "panels", "controls", "lamps", "displays"})) {
            return false;
        }
        if (!RequireString(root, "", "system_id", out.system_id)) return false;
        if (out.system_id != console.system_id) {
            return Fail("system_id", "'" + out.system_id + "' doesn't match the console's '" + console.system_id + "'");
        }
        return LoadStations(root, out) && LoadPanels(root, out) && LoadControlItems(root, console.panel, out) &&
               PlaceGuards(console.panel, out) && LoadLamps(root, console, out) && LoadDisplays(root, console, out);
    }

private:
    bool Number3(const json::Value& obj, const std::string& path, const char* key, Vec3& out) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        if (!v->Is(json::Type::Array) || v->items.size() != 3) return Fail(Join(path, key), "expected [x, y, z]");
        double xyz[3] = {0.0, 0.0, 0.0};
        for (std::size_t i = 0; i < 3; ++i) {
            if (!v->items[i].Is(json::Type::Number)) return Fail(Index(Join(path, key), i), "expected a number");
            xyz[i] = v->items[i].number;
        }
        out = Vec3{xyz[0], xyz[1], xyz[2]};
        return true;
    }

    bool Number2(const json::Value& obj, const std::string& path, const char* key, double& a, double& b) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(Join(path, key), "required");
        if (!v->Is(json::Type::Array) || v->items.size() != 2 || !v->items[0].Is(json::Type::Number) ||
            !v->items[1].Is(json::Type::Number)) {
            return Fail(Join(path, key), "expected two numbers");
        }
        a = v->items[0].number;
        b = v->items[1].number;
        return true;
    }

    bool Degrees(const json::Value& obj, const std::string& path, const char* key, double& out_rad) {
        double deg = 0.0;
        if (!OptionalNumber(obj, path, key, deg)) return false;
        out_rad = DegToRad(deg);
        return true;
    }

    bool LoadStations(const json::Value& root, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "stations");
        if (arr == nullptr) return message_.empty() ? Fail("stations", "required") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("stations", i);
            const json::Value& s = arr->items[i];
            if (!Keys(s, path, {"id", "label", "eye_m", "yaw_deg", "pitch_deg", "notes"})) return false;
            CabinStation st;
            if (!RequireString(s, path, "id", st.id) || !RequireString(s, path, "label", st.label) ||
                !Number3(s, path, "eye_m", st.eye_m) || !Degrees(s, path, "yaw_deg", st.yaw_rad) ||
                !Degrees(s, path, "pitch_deg", st.pitch_rad)) {
                return false;
            }
            if (out.FindStation(st.id) != nullptr) return Fail(Join(path, "id"), "duplicate station '" + st.id + "'");
            out.stations.push_back(std::move(st));
        }
        return true;
    }

    bool LoadPanels(const json::Value& root, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "panels");
        if (arr == nullptr) return message_.empty() ? Fail("panels", "required") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("panels", i);
            const json::Value& p = arr->items[i];
            if (!Keys(p, path, {"id", "label", "station", "centre_m", "facing_deg", "tilt_deg", "size_m", "notes"})) {
                return false;
            }
            CabinPanel panel;
            if (!RequireString(p, path, "id", panel.id) || !RequireString(p, path, "label", panel.label) ||
                !RequireString(p, path, "station", panel.station) || !Number3(p, path, "centre_m", panel.centre_m) ||
                !Degrees(p, path, "facing_deg", panel.facing_rad) || !Degrees(p, path, "tilt_deg", panel.tilt_rad) ||
                !Number2(p, path, "size_m", panel.width_m, panel.height_m)) {
                return false;
            }
            if (out.FindStation(panel.station) == nullptr) {
                return Fail(Join(path, "station"), "unknown station '" + panel.station + "'");
            }
            if (!(panel.width_m > 0.0) || !(panel.height_m > 0.0)) return Fail(Join(path, "size_m"), "must be positive");
            if (out.FindPanel(panel.id) != nullptr) return Fail(Join(path, "id"), "duplicate panel '" + panel.id + "'");
            out.panels.push_back(std::move(panel));
        }
        return true;
    }

    // Shared by controls and lamps: a panel that exists, and a spot inside it.
    bool Placement(const json::Value& item, const std::string& path, const CabinLayout& out, std::string& panel_id,
                   double& u, double& v) {
        if (!RequireString(item, path, "panel", panel_id) || !Number2(item, path, "at_m", u, v)) return false;
        const CabinPanel* panel = out.FindPanel(panel_id);
        if (panel == nullptr) return Fail(Join(path, "panel"), "unknown panel '" + panel_id + "'");
        if (!panel->Contains(u, v)) return Fail(Join(path, "at_m"), "outside panel '" + panel_id + "'");
        return true;
    }

    bool StyleFits(CabinStyle style, const ControlDef& def) {
        switch (style) {
            case CabinStyle::Button: return def.kind == ControlKind::Momentary;
            case CabinStyle::Wheel: return def.kind == ControlKind::Continuous;
            case CabinStyle::Toggle:
            case CabinStyle::Cover: return def.kind == ControlKind::Discrete && def.positions == 2;
            case CabinStyle::Rotary:
            case CabinStyle::Key: return def.kind == ControlKind::Discrete;
        }
        return false;
    }

    bool ParseStyle(const std::string& text, CabinStyle& out) {
        for (CabinStyle s : {CabinStyle::Toggle, CabinStyle::Rotary, CabinStyle::Key, CabinStyle::Button,
                             CabinStyle::Wheel, CabinStyle::Cover}) {
            if (text == ToString(s)) {
                out = s;
                return true;
            }
        }
        return false;
    }

    bool LoadControlItems(const json::Value& root, const ControlPanel& console_panel, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "controls");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("controls", i);
            const json::Value& c = arr->items[i];
            if (!Keys(c, path, {"control", "panel", "at_m", "style", "units_per_turn", "notes"})) return false;
            CabinControl item;
            if (!RequireString(c, path, "control", item.control_id)) return false;
            const ControlDef* def = console_panel.Find(item.control_id);
            if (def == nullptr) return Fail(Join(path, "control"), "unknown control '" + item.control_id + "'");
            if (out.FindControl(item.control_id) != nullptr) {
                return Fail(Join(path, "control"), "'" + item.control_id + "' is placed twice");
            }
            if (!Placement(c, path, out, item.panel_id, item.u_m, item.v_m)) return false;
            const CabinPanel* panel = out.FindPanel(item.panel_id);
            if (!def->station.empty() && def->station != panel->station) {
                return Fail(Join(path, "panel"), "'" + item.control_id + "' belongs to station '" + def->station +
                                                     "', panel '" + panel->id + "' to '" + panel->station + "'");
            }
            item.style = DefaultCabinStyle(*def, console_panel);
            std::string style;
            if (!OptionalString(c, path, "style", style)) return false;
            if (!style.empty() && !ParseStyle(style, item.style)) {
                return Fail(Join(path, "style"), "must be toggle, rotary, key, button, wheel or cover");
            }
            if (!StyleFits(item.style, *def)) {
                return Fail(Join(path, "style"), std::string("'") + ToString(item.style) + "' doesn't fit control '" +
                                                     item.control_id + "'");
            }
            if (c.Find("units_per_turn") != nullptr) {
                if (def->kind != ControlKind::Continuous) return Fail(Join(path, "units_per_turn"), "for wheels only");
                if (!OptionalNumber(c, path, "units_per_turn", item.units_per_turn)) return false;
                if (!(item.units_per_turn > 0.0)) return Fail(Join(path, "units_per_turn"), "must be positive");
            }
            out.controls.push_back(std::move(item));
        }
        return true;
    }

    // A guard nobody placed goes over the control it guards.
    bool PlaceGuards(const ControlPanel& console_panel, CabinLayout& out) {
        const std::size_t placed = out.controls.size();
        for (std::size_t i = 0; i < placed; ++i) {
            const ControlDef* def = console_panel.Find(out.controls[i].control_id);
            if (def->guard_id.empty() || out.FindControl(def->guard_id) != nullptr) continue;
            CabinControl cover = out.controls[i];
            cover.control_id = def->guard_id;
            cover.style = DefaultCabinStyle(*console_panel.Find(def->guard_id), console_panel);
            cover.units_per_turn = 360.0;
            cover.auto_placed = true;
            out.controls.push_back(std::move(cover));
        }
        return true;
    }

    bool LoadLamps(const json::Value& root, const ConsoleData& console, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "lamps");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("lamps", i);
            const json::Value& l = arr->items[i];
            if (!Keys(l, path, {"id", "label", "panel", "at_m", "color", "lit_when", "lit_when_any", "source",
                                "confidence", "notes"})) {
                return false;
            }
            CabinLamp lamp;
            if (!RequireString(l, path, "id", lamp.id) || !RequireString(l, path, "label", lamp.label) ||
                !RequireString(l, path, "color", lamp.color) || !Provenance(l, path) ||
                !Placement(l, path, out, lamp.panel_id, lamp.u_m, lamp.v_m)) {
                return false;
            }
            if (lamp.color != "green" && lamp.color != "red" && lamp.color != "amber" && lamp.color != "white") {
                return Fail(Join(path, "color"), "must be green, red, amber or white");
            }
            for (const CabinLamp& other : out.lamps) {
                if (other.id == lamp.id) return Fail(Join(path, "id"), "duplicate lamp '" + lamp.id + "'");
            }
            if (!LoadConditions(l, path, "lit_when", console.panel, console.equipment, lamp.lit_when) ||
                !LoadConditions(l, path, "lit_when_any", console.panel, console.equipment, lamp.lit_when_any)) {
                return false;
            }
            if (lamp.lit_when.empty() && lamp.lit_when_any.empty()) {
                return Fail(path, "needs lit_when or lit_when_any");
            }
            out.lamps.push_back(std::move(lamp));
        }
        return true;
    }

    bool LoadDisplays(const json::Value& root, const ConsoleData& console, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "displays");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("displays", i);
            const json::Value& d = arr->items[i];
            if (!Keys(d, path, {"id", "label", "kind", "panel", "at_m", "size_m", "on_when", "on_when_any", "lit_when",
                                "control", "marks", "value_per_turn", "labels_per_turn", "scale", "full_scale", "unit",
                                "sweep_period_s", "afterglow_s", "magnified_window_m", "source", "confidence",
                                "notes"})) {
                return false;
            }
            CabinDisplay disp;
            std::string kind;
            if (!RequireString(d, path, "id", disp.id) || !RequireString(d, path, "label", disp.label) ||
                !RequireString(d, path, "kind", kind) || !Provenance(d, path) ||
                !RequireString(d, path, "panel", disp.panel_id) || !Number2(d, path, "at_m", disp.u_m, disp.v_m) ||
                !Number2(d, path, "size_m", disp.width_m, disp.height_m)) {
                return false;
            }
            const CabinPanel* panel = out.FindPanel(disp.panel_id);
            if (panel == nullptr) return Fail(Join(path, "panel"), "unknown panel '" + disp.panel_id + "'");
            if (!(disp.width_m > 0.0) || !(disp.height_m > 0.0)) return Fail(Join(path, "size_m"), "must be positive");
            if (!panel->Contains(disp.u_m - 0.5 * disp.width_m, disp.v_m - 0.5 * disp.height_m) ||
                !panel->Contains(disp.u_m + 0.5 * disp.width_m, disp.v_m + 0.5 * disp.height_m)) {
                return Fail(Join(path, "at_m"), "display doesn't fit inside panel '" + disp.panel_id + "'");
            }
            for (const CabinDisplay& other : out.displays) {
                if (other.id == disp.id) return Fail(Join(path, "id"), "duplicate display '" + disp.id + "'");
            }
            if (!LoadConditions(d, path, "on_when", console.panel, console.equipment, disp.on_when) ||
                !LoadConditions(d, path, "on_when_any", console.panel, console.equipment, disp.on_when_any) ||
                !LoadConditions(d, path, "lit_when", console.panel, console.equipment, disp.lit_when)) {
                return false;
            }
            if (!LoadDisplayKind(d, path, kind, console, disp)) return false;
            out.displays.push_back(std::move(disp));
        }
        return true;
    }

    // The keys each kind takes beyond the common ones, and what it needs from the console.
    bool LoadDisplayKind(const json::Value& d, const std::string& path, const std::string& kind,
                         const ConsoleData& console, CabinDisplay& disp) {
        std::vector<std::string> allowed;
        if (kind == "ppi") {
            disp.kind = CabinDisplayKind::Ppi;
            allowed = {"sweep_period_s", "afterglow_s"};
            if (!console.has_sensors) return Fail(Join(path, "kind"), "a ppi needs the console's sensors block");
        } else if (kind == "range_scope") {
            disp.kind = CabinDisplayKind::RangeScope;
            allowed = {"magnified_window_m"};
            if (!console.has_sensors) return Fail(Join(path, "kind"), "a range_scope needs the console's sensors block");
        } else if (kind == "tv") {
            disp.kind = CabinDisplayKind::Tv;
            if (!console.has_sensors || console.track.tv_camera.empty()) {
                return Fail(Join(path, "kind"), "a tv needs a TV camera in the console's sensors block");
            }
        } else if (kind == "dial") {
            disp.kind = CabinDisplayKind::Dial;
            allowed = {"control", "marks", "value_per_turn", "labels_per_turn"};
        } else if (kind == "meter") {
            disp.kind = CabinDisplayKind::Meter;
            allowed = {"control", "marks", "scale", "full_scale", "unit"};
        } else {
            return Fail(Join(path, "kind"), "must be ppi, range_scope, tv, dial or meter");
        }
        for (const char* key : {"control", "marks", "value_per_turn", "labels_per_turn", "scale", "full_scale", "unit",
                                "sweep_period_s", "afterglow_s", "magnified_window_m"}) {
            bool ok = d.Find(key) == nullptr;
            for (const std::string& a : allowed) ok = ok || a == key;
            if (!ok) return Fail(Join(path, key), "not used by a " + kind);
        }
        if (disp.kind == CabinDisplayKind::Dial || disp.kind == CabinDisplayKind::Meter) {
            if (!RequireString(d, path, "control", disp.control)) return false;
            if (!console.panel.Has(disp.control)) {
                return Fail(Join(path, "control"), "unknown control '" + disp.control + "'");
            }
        }
        if (!OptionalInt(d, path, "marks", disp.marks) || !OptionalNumber(d, path, "value_per_turn", disp.value_per_turn) ||
            !OptionalNumber(d, path, "labels_per_turn", disp.labels_per_turn) ||
            !OptionalNumber(d, path, "scale", disp.scale) || !OptionalNumber(d, path, "full_scale", disp.full_scale) ||
            !OptionalString(d, path, "unit", disp.unit) ||
            !OptionalNumber(d, path, "sweep_period_s", disp.sweep_period_s) ||
            !OptionalNumber(d, path, "afterglow_s", disp.afterglow_s) ||
            !OptionalNumber(d, path, "magnified_window_m", disp.magnified_window_m)) {
            return false;
        }
        if (disp.marks < 1) return Fail(Join(path, "marks"), "must be at least 1");
        for (double v : {disp.value_per_turn, disp.full_scale, disp.sweep_period_s, disp.afterglow_s,
                         disp.magnified_window_m}) {
            if (!(v > 0.0)) {
                return Fail(path, "value_per_turn, full_scale, sweep_period_s, afterglow_s and magnified_window_m "
                                  "must be positive");
            }
        }
        return true;
    }
};

}  // namespace detail

// Parse and validate a cabin layout against the console it belongs to. On failure `out` is left unchanged.
inline bool LoadCabin(const std::string& json_text, const ConsoleData& console, CabinLayout& out,
                      std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    CabinLayout loaded;
    detail::CabinLoader loader;
    if (!loader.Load(root, console, loaded)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(loaded);
    return true;
}

inline bool LoadCabinFile(const std::string& path, const ConsoleData& console, CabinLayout& out,
                          std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    if (!LoadCabin(text.str(), console, out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

}  // namespace simcore
