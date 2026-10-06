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
// tracking radar's range indicator ("range_scope"), its elevation-search indicator
// ("elevation_scope"), the TV sight's monitor ("tv"), and needle instruments showing a
// control's value ("dial" turns, "meter" swings). Each has conditions for when it is
// powered and (dials) when its backlight is on.
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

// What an item looks like on the panel, so a cabin can resemble the real one: its shape, size and colours. Fixtures
// (items the console doesn't model: plates, nameplates, knobs, jacks...) always have one; controls and lamps may.
enum class CabinLookKind {
    Default,      // the control's or lamp's own style
    Knob,         // round knob (color: black, white, brass, chrome)
    PointerKnob,  // knob with a pointer over a scale
    Trim,         // slotted screwdriver trim
    Toggle,       // toggle lever
    Lever,        // flat lever
    Lamp,         // indicator lamp cap
    LampButton,   // illuminated push-button: cap `color` in a `ring`
    Button,       // plain push-button
    Jack,         // test jack
    Socket,       // round connector or coaxial socket
    Latch,        // block lock (ОТКР. / ЗАКР.)
    Nameplate,    // block type plate with `label`
    Bezel,        // a screen's surround
    Window,       // a counter or dial window
    Diagram,      // printed outline diagram
    Oval,         // raised oval mount
    Legend,       // legend lamp: black glass with its `label` on it, the letters lit in `color` (lamps only)
    Gauge         // an instrument the console doesn't model: dark rim, light face, needle (round, or square for [w, h])
};

inline const char* ToString(CabinLookKind k) {
    switch (k) {
        case CabinLookKind::Default: return "default";
        case CabinLookKind::Knob: return "knob";
        case CabinLookKind::PointerKnob: return "pointer_knob";
        case CabinLookKind::Trim: return "trim";
        case CabinLookKind::Toggle: return "toggle";
        case CabinLookKind::Lever: return "lever";
        case CabinLookKind::Lamp: return "lamp";
        case CabinLookKind::LampButton: return "lamp_button";
        case CabinLookKind::Button: return "button";
        case CabinLookKind::Jack: return "jack";
        case CabinLookKind::Socket: return "socket";
        case CabinLookKind::Latch: return "latch";
        case CabinLookKind::Nameplate: return "nameplate";
        case CabinLookKind::Bezel: return "bezel";
        case CabinLookKind::Window: return "window";
        case CabinLookKind::Diagram: return "diagram";
        case CabinLookKind::Oval: return "oval";
        case CabinLookKind::Legend: return "legend";
        case CabinLookKind::Gauge: return "gauge";
    }
    return "?";
}

struct CabinLook {
    CabinLookKind kind = CabinLookKind::Default;
    double width_m = 0.0;   // round items: diameter (height = width)
    double height_m = 0.0;
    std::string color;      // black, white, grey, chrome, brass, red, green, amber, yellow, orange
    std::string ring;       // lamp_button: the ring's colour
    std::string label;      // nameplate text, or the item's engraved label
};

// A block (one instrument unit of a console column): its plate on the panel and its type plate.
struct CabinBlock {
    std::string id;
    std::string panel_id;
    std::string label;      // e.g. "ОС81-9М"
    double u_m = 0.0;       // centre
    double v_m = 0.0;
    double width_m = 0.0;
    double height_m = 0.0;
    std::string color;      // the plate's paint, a look colour; empty = the console's cream
};

// An item on a panel the console doesn't model; drawn, not operated.
struct CabinFixture {
    std::string panel_id;
    double u_m = 0.0;
    double v_m = 0.0;
    CabinLook look;
};

// A piece of the cabin's shell in the body frame: floor, walls, ceiling, bench, racks ("box"), a window ("glass"), or
// an interior lamp ("light", its size the light's reach).
enum class CabinShellKind { Box, Glass, Light };

struct CabinShellPart {
    std::string id;
    std::string label;
    CabinShellKind kind = CabinShellKind::Box;
    Vec3 centre_m;
    Vec3 size_m;       // x (right), y (forward), z (up)
    std::string color; // a look colour (CabinLook), or "paint" (the cabin's grey-green), "floor"
};

struct CabinStation {
    std::string id;           // a station id used by the console's controls, e.g. "senior_operator"
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
    int red_dot = -1;               // toggles and rotaries: the position a red dot marks on the panel, or -1
    CabinLook look;                 // optional appearance (kind Default = by style)
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
    CabinLook look;                      // optional appearance (kind Default = a plain lamp cap)
};

enum class CabinDisplayKind { Ppi, RangeScope, ElevationScope, Tv, Dial, Meter };

inline const char* ToString(CabinDisplayKind k) {
    switch (k) {
        case CabinDisplayKind::Ppi: return "ppi";
        case CabinDisplayKind::RangeScope: return "range_scope";
        case CabinDisplayKind::ElevationScope: return "elevation_scope";
        case CabinDisplayKind::Tv: return "tv";
        case CabinDisplayKind::Dial: return "dial";
        case CabinDisplayKind::Meter: return "meter";
    }
    return "?";
}

// Where one search beam's raster sits on an elevation_scope's screen (0 = bottom, 1 = top). The beam's elevations
// are the console's (TrackSensorDef::beams_deg).
struct ElevationRaster {
    double screen_low = 0.0;
    double screen_high = 1.0;
};

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
    // elevation_scope (also uses afterglow_s): one raster per search beam of the console's elevation search, and
    // the range window across the screen, centred on the range gate
    std::vector<ElevationRaster> rasters;
    double range_window_m = 2000.0;
};

struct CabinLayout {
    std::string system_id;
    std::vector<CabinStation> stations;
    std::vector<CabinPanel> panels;
    std::vector<CabinControl> controls;
    std::vector<CabinLamp> lamps;
    std::vector<CabinDisplay> displays;
    std::vector<CabinBlock> blocks;
    std::vector<CabinFixture> fixtures;
    std::vector<CabinShellPart> shell;

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

    // Which way a toggle's lever, a rotary's pointer or a key points in a position: degrees on the panel face
    // from "up the face", clockwise (towards the operator's right). Toggles: up for on (>= 1), down for off.
    // Rotaries: positions spread symmetrically about up, at most 30 degrees apart. Keys: upright when
    // inserted, a quarter turn right when turned. 0 for other styles.
    static double PositionAngleDeg(CabinStyle style, int positions, int position) {
        switch (style) {
            case CabinStyle::Toggle:
                return position >= 1 ? 0.0 : 180.0;
            case CabinStyle::Rotary: {
                const int count = positions < 2 ? 2 : positions;
                const double step = std::fmin(30.0, 240.0 / (count - 1));
                return (position - 0.5 * (count - 1)) * step;
            }
            case CabinStyle::Key:
                return position >= 2 ? 90.0 : 0.0;
            default:
                return 0.0;
        }
    }

    // The red dot printed beside a toggle or rotary (korovin_tiv_2010: red dots by the knobs and switches for quick
    // preparation before firing): its offset on the panel from the control's centre (u right, v up the face,
    // metres). The dot marks the control's working position, which is often not where it starts (РОД РАБОТЫ's dot
    // is at БР; docs/research/OSA_CONSOLE_MAP_2026-10.md §12), so the layout file names it. False for controls
    // without a dot.
    static bool RedDot(const CabinControl& c, const ControlDef& def, double& du_m, double& dv_m) {
        double radius_m = 0.0;
        if (c.red_dot < 0) return false;
        if (c.style == CabinStyle::Toggle) {
            radius_m = 0.03;
        } else if (c.style == CabinStyle::Rotary) {
            radius_m = 0.032;
        } else {
            return false;
        }
        const double a = DegToRad(PositionAngleDeg(c.style, def.positions, c.red_dot));
        du_m = radius_m * std::sin(a);
        dv_m = radius_m * std::cos(a);
        return true;
    }

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
        if (!Keys(root, "", {"system_id", "status", "notes", "stations", "panels", "controls", "lamps", "displays",
                             "blocks", "fixtures", "shell"})) {
            return false;
        }
        if (!RequireString(root, "", "system_id", out.system_id)) return false;
        if (out.system_id != console.system_id) {
            return Fail("system_id", "'" + out.system_id + "' doesn't match the console's '" + console.system_id + "'");
        }
        return LoadSeats(root, console, out) && LoadPanels(root, out) && LoadControlItems(root, console.panel, out) &&
               PlaceGuards(console.panel, out) && LoadLamps(root, console, out) && LoadDisplays(root, console, out) &&
               LoadBlocks(root, out) && LoadFixtures(root, out) && LoadShell(root, out);
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

    // With a station list in the console, the cabin has exactly one seat for each of its stations.
    bool LoadSeats(const json::Value& root, const ConsoleData& console, CabinLayout& out) {
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
            if (!console.stations.empty() && console.FindStation(st.id) == nullptr) {
                return Fail(Join(path, "id"), "'" + st.id + "' is not a station of the console");
            }
            out.stations.push_back(std::move(st));
        }
        for (const ConsoleStation& cs : console.stations) {
            if (out.FindStation(cs.id) == nullptr) return Fail("stations", "no seat for the console's '" + cs.id + "'");
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
            if (!Keys(c, path, {"control", "panel", "at_m", "style", "units_per_turn", "red_dot", "look", "notes"})) {
                return false;
            }
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
            if (c.Find("red_dot") != nullptr) {
                if (item.style != CabinStyle::Toggle && item.style != CabinStyle::Rotary) {
                    return Fail(Join(path, "red_dot"), "for toggles and rotaries only");
                }
                if (!OptionalInt(c, path, "red_dot", item.red_dot)) return false;
                if (item.red_dot < 0 || item.red_dot >= def->positions) {
                    return Fail(Join(path, "red_dot"), "not a position of '" + item.control_id + "'");
                }
            }
            if (const json::Value* look = c.Find("look")) {
                if (!LoadLook(*look, Join(path, "look"), false, item.look)) return false;
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
            cover.red_dot = -1;
            cover.look = CabinLook{};  // a cover keeps its own look, not the guarded control's
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
            if (!Keys(l, path, {"id", "label", "panel", "at_m", "color", "lit_when", "lit_when_any", "look", "source",
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
            if (const json::Value* look = l.Find("look")) {
                if (!LoadLook(*look, Join(path, "look"), false, lamp.look)) return false;
            }
            out.lamps.push_back(std::move(lamp));
        }
        return true;
    }

    static bool KnownColour(const std::string& c) {
        for (const char* known : {"black", "white", "grey", "chrome", "brass", "red", "green", "amber", "yellow", "orange"}) {
            if (c == known) return true;
        }
        return false;
    }

    // A look: "kind", "size_m" (a diameter, or [width, height]), "color", "ring", "label". Fixtures carry the same keys
    // inline and need a kind; a control's or lamp's "look" may leave it out (kind default).
    bool LoadLook(const json::Value& v, const std::string& path, bool inline_keys, CabinLook& out) {
        if (!inline_keys && !Keys(v, path, {"kind", "size_m", "color", "ring", "label"})) return false;
        std::string kind;
        if (!OptionalString(v, path, "kind", kind)) return false;
        if (inline_keys && kind.empty()) return Fail(Join(path, "kind"), "required");
        if (!kind.empty()) {
            bool found = false;
            for (int k = 0; k <= static_cast<int>(CabinLookKind::Gauge); ++k) {
                if (kind == ToString(static_cast<CabinLookKind>(k))) {
                    out.kind = static_cast<CabinLookKind>(k);
                    found = true;
                }
            }
            if (!found || (inline_keys && out.kind == CabinLookKind::Default)) {
                return Fail(Join(path, "kind"), "unknown look '" + kind + "'");
            }
        }
        if (const json::Value* size = v.Find("size_m")) {
            if (size->Is(json::Type::Number)) {
                out.width_m = out.height_m = size->number;
            } else if (!Number2(v, path, "size_m", out.width_m, out.height_m)) {
                return false;
            }
            if (!(out.width_m > 0.0) || !(out.height_m > 0.0)) return Fail(Join(path, "size_m"), "must be positive");
        } else if (inline_keys) {
            return Fail(Join(path, "size_m"), "required");
        }
        if (!OptionalString(v, path, "color", out.color) || !OptionalString(v, path, "ring", out.ring) ||
            !OptionalString(v, path, "label", out.label)) {
            return false;
        }
        for (const std::string* c : {&out.color, &out.ring}) {
            if (!c->empty() && !KnownColour(*c)) {
                return Fail(Join(path, c == &out.color ? "color" : "ring"), "unknown colour '" + *c + "'");
            }
        }
        if ((out.kind == CabinLookKind::Nameplate || out.kind == CabinLookKind::Legend) && out.label.empty()) {
            return Fail(Join(path, "label"), "required");
        }
        return true;
    }

    bool LoadBlocks(const json::Value& root, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "blocks");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("blocks", i);
            const json::Value& b = arr->items[i];
            if (!Keys(b, path, {"id", "panel", "label", "at_m", "size_m", "color", "notes"})) return false;
            CabinBlock block;
            if (!RequireString(b, path, "id", block.id) || !RequireString(b, path, "label", block.label) ||
                !Placement(b, path, out, block.panel_id, block.u_m, block.v_m) ||
                !Number2(b, path, "size_m", block.width_m, block.height_m)) {
                return false;
            }
            if (!(block.width_m > 0.0) || !(block.height_m > 0.0)) return Fail(Join(path, "size_m"), "must be positive");
            if (!OptionalString(b, path, "color", block.color)) return false;
            if (!block.color.empty() && !KnownColour(block.color)) {
                return Fail(Join(path, "color"), "unknown colour '" + block.color + "'");
            }
            const CabinPanel* panel = out.FindPanel(block.panel_id);
            if (!panel->Contains(block.u_m - 0.5 * block.width_m, block.v_m - 0.5 * block.height_m) ||
                !panel->Contains(block.u_m + 0.5 * block.width_m, block.v_m + 0.5 * block.height_m)) {
                return Fail(Join(path, "at_m"), "block doesn't fit inside panel '" + block.panel_id + "'");
            }
            for (const CabinBlock& other : out.blocks) {
                if (other.id == block.id) return Fail(Join(path, "id"), "duplicate block '" + block.id + "'");
            }
            out.blocks.push_back(std::move(block));
        }
        return true;
    }

    bool LoadShell(const json::Value& root, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "shell");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("shell", i);
            const json::Value& v = arr->items[i];
            if (!Keys(v, path, {"id", "label", "kind", "centre_m", "size_m", "color", "source", "confidence", "notes"})) {
                return false;
            }
            CabinShellPart part;
            std::string kind;
            if (!RequireString(v, path, "id", part.id) || !RequireString(v, path, "label", part.label) ||
                !RequireString(v, path, "kind", kind) || !Number3(v, path, "centre_m", part.centre_m) ||
                !Number3(v, path, "size_m", part.size_m) || !OptionalString(v, path, "color", part.color) ||
                !Provenance(v, path)) {
                return false;
            }
            if (kind == "box") {
                part.kind = CabinShellKind::Box;
            } else if (kind == "glass") {
                part.kind = CabinShellKind::Glass;
            } else if (kind == "light") {
                part.kind = CabinShellKind::Light;
            } else {
                return Fail(Join(path, "kind"), "must be box, glass or light");
            }
            if (!(part.size_m.x > 0.0) || !(part.size_m.y > 0.0) || !(part.size_m.z > 0.0)) {
                return Fail(Join(path, "size_m"), "must be positive");
            }
            bool known = part.color.empty() || part.color == "paint" || part.color == "floor";
            for (const char* c : {"black", "white", "grey", "chrome", "brass", "red", "green", "amber", "yellow", "orange"}) {
                known = known || part.color == c;
            }
            if (!known) return Fail(Join(path, "color"), "unknown colour '" + part.color + "'");
            for (const CabinShellPart& other : out.shell) {
                if (other.id == part.id) return Fail(Join(path, "id"), "duplicate shell part '" + part.id + "'");
            }
            out.shell.push_back(std::move(part));
        }
        return true;
    }

    bool LoadFixtures(const json::Value& root, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "fixtures");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("fixtures", i);
            const json::Value& f = arr->items[i];
            if (!Keys(f, path, {"panel", "at_m", "kind", "size_m", "color", "ring", "label", "notes"})) return false;
            CabinFixture fixture;
            if (!Placement(f, path, out, fixture.panel_id, fixture.u_m, fixture.v_m) ||
                !LoadLook(f, path, true, fixture.look)) {
                return false;
            }
            out.fixtures.push_back(std::move(fixture));
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
                                "sweep_period_s", "afterglow_s", "magnified_window_m", "rasters", "range_window_m",
                                "source", "confidence", "notes"})) {
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
        } else if (kind == "elevation_scope") {
            disp.kind = CabinDisplayKind::ElevationScope;
            allowed = {"afterglow_s", "rasters", "range_window_m"};
            if (!console.has_sensors || console.track.beams_deg.empty()) {
                return Fail(Join(path, "kind"), "an elevation_scope needs the console's track elevation_search");
            }
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
            return Fail(Join(path, "kind"), "must be ppi, range_scope, elevation_scope, tv, dial or meter");
        }
        for (const char* key : {"control", "marks", "value_per_turn", "labels_per_turn", "scale", "full_scale", "unit",
                                "sweep_period_s", "afterglow_s", "magnified_window_m", "rasters",
                                "range_window_m"}) {
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
            !OptionalNumber(d, path, "magnified_window_m", disp.magnified_window_m) ||
            !OptionalNumber(d, path, "range_window_m", disp.range_window_m)) {
            return false;
        }
        if (disp.marks < 1) return Fail(Join(path, "marks"), "must be at least 1");
        for (double v : {disp.value_per_turn, disp.full_scale, disp.sweep_period_s, disp.afterglow_s,
                         disp.magnified_window_m, disp.range_window_m}) {
            if (!(v > 0.0)) {
                return Fail(path, "value_per_turn, full_scale, sweep_period_s, afterglow_s, magnified_window_m and "
                                  "range_window_m must be positive");
            }
        }
        if (disp.kind == CabinDisplayKind::ElevationScope && !LoadElevationScope(d, path, console, disp)) return false;
        return true;
    }

    bool LoadElevationScope(const json::Value& d, const std::string& path, const ConsoleData& console,
                            CabinDisplay& disp) {
        const json::Value* arr = OptionalArray(d, path, "rasters");
        if (arr == nullptr) return message_.empty() ? Fail(Join(path, "rasters"), "required") : false;
        if (arr->items.size() != console.track.beams_deg.size()) {
            return Fail(Join(path, "rasters"), "needs one raster per beam of the console's elevation_search");
        }
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string rpath = Index(Join(path, "rasters"), i);
            const json::Value& r = arr->items[i];
            if (!Keys(r, rpath, {"screen"})) return false;
            ElevationRaster raster;
            if (!Number2(r, rpath, "screen", raster.screen_low, raster.screen_high)) return false;
            if (!(raster.screen_low >= 0.0 && raster.screen_low < raster.screen_high && raster.screen_high <= 1.0)) {
                return Fail(Join(rpath, "screen"), "must be [low, high] within 0..1");
            }
            disp.rasters.push_back(raster);
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
