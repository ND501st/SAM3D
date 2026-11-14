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
#include "simcore/radio_sets.hpp"
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
    Gauge,        // an instrument the console doesn't model: dark rim, light face, needle (round, or square for [w, h])
    BarKnob,      // bakelite T-bar knob: a round boss with a bar across it, one end longer (the pointer): the R-123M's
    Dome,         // hand wheel: a domed cap in place of the dish, its crank on the dome's flank (SŁ-2M's range wheel)
    InMesh,       // a control whose part is the shell mesh's own (the 0096's pairs of ВКЛ. and ОТКЛ. buttons): nothing
                  // is drawn for it, only its hit area is kept (controls only)
    Text,         // engraved text standing on its own (a group's heading such as ПЕРЕДАТЧИК, "— I —"): `label`, letters
                  // `size_m` high (or [width, height] of its box), turned `angle_deg` anticlockwise (fixtures only)
    Scale,        // a printed scale round a knob: an arc of `size_m` across with a tick and a figure at each of
                  // `angles_deg` (clockwise from up), the figures `marks` (fixtures only)
    Frame,        // a ruled rectangle printed on the plate (a block's chart of its units, filled with Text fixtures)
    Screw,        // a slotted screw head holding a block's plate, `size_m` across (fixtures only)
    Signal,       // signal lamp armature (lamps only): a copper-red ring, a fluted cap standing out with the lens at its
                  // end; `ring` the cap's finish (chrome, the default, or black); drawn 1.4 x `size_m` across, as a lamp
    HexButton     // hex-nut push-button: a steel nut `size_m` across its flats and a domed pusher in `color` standing out
                  // of it (SŁ-2M's ВЫСОКОЕ, ПОИСК, ЛУЧ, ЗАПРОС, ПЕЛЕНГ); on a lamp, the pusher is its lens (ЗАХ. АПЧ I)
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
        case CabinLookKind::BarKnob: return "bar_knob";
        case CabinLookKind::Dome: return "dome";
        case CabinLookKind::InMesh: return "in_mesh";
        case CabinLookKind::Text: return "text";
        case CabinLookKind::Scale: return "scale";
        case CabinLookKind::Frame: return "frame";
        case CabinLookKind::Screw: return "screw";
        case CabinLookKind::Signal: return "signal";
        case CabinLookKind::HexButton: return "hex_button";
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
    std::string scale;      // gauge: the printed dial, "linear" (default, 0-10), "compass" (a 0-35 card), "counters" (a row of lit digit windows) or "hours" (an hour meter: a grey face with a digit window)
    std::vector<std::string> marks;  // gauge, linear: the figures printed at its major marks, first to last (as read off a
                                     // photograph); empty: the generic 0-10; scale: a figure per tick ("" for a bare tick)
    std::string unit;                // gauge: the unit printed on its face
    std::string label_at;            // where the engraved label stands: "above" (empty: the default), "below", "left", "right"
    std::vector<double> angles_deg;  // scale: where each of `marks` stands, degrees clockwise from up
    double angle_deg = 0.0;          // text and latches: turned anticlockwise (90: read bottom to top; a lock on its side)
    std::string arrow;               // scale: an arrowhead at its "first" angle's end, its "last", or "both"
    std::string ink;                 // legend: the letters' colour when unlit (ПУСК ГОТОВ's red); empty: the usual
    std::string shape;               // legend: "stadium" rounds a wide cap's ends (ПУСК ГОТОВ); empty: a rectangle
    int up = 1;                      // toggle: the position that stands up (SAM's photo engraves the upper legend first;
                                     // УГОЛ ЗРЕНИЯ's ШИРОКИЙ, position 0, is up)
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
    std::string mesh;  // a shell mesh that draws this fixture (SŁ-2M's mounts in SM_RackSL2M): the game draws nothing
};

// A piece of the cabin's shell in the body frame: floor, walls, ceiling, bench, racks ("box"), a window ("glass"), an
// interior lamp ("light", its size the light's reach), or a modelled part ("mesh": a mesh of the cabin's art,
// art/cabin_interior, with its origin at the centre of its bounding box, placed at centre_m and turned by yaw).
enum class CabinShellKind { Box, Glass, Light, Mesh };

struct CabinShellPart {
    std::string id;
    std::string label;
    CabinShellKind kind = CabinShellKind::Box;
    Vec3 centre_m;
    Vec3 size_m;          // x (right), y (forward), z (up); a mesh's bounding box before it is turned
    std::string color;    // a look colour (CabinLook), or "paint" (the cabin's grey-green), "floor"; lights: also blue
    std::string mesh;     // mesh parts: the mesh's name, e.g. "SM_OsaHull"
    double yaw_rad = 0.0; // mesh parts: turned clockwise from forward (seen from above)
    double lumens = 0.0;  // lights: brightness; 0 = the cabin's default
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
    bool bare = false;        // no plate of its own: the panel is the front of a mesh that has its own (an R-123M's), and only
                              // carries the controls and lamps at their places on it
    std::string color;        // the plate's paint (a look colour; "" the console's cream)

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

// A hinged part drawn in place of a control's own (a cover over a button, a lock's lever): meshes modelled full size in
// metres with the hinge's axis through their origin; shut (0 degrees) the part's +Y runs across it from the hinge,
// `axis_deg` clockwise from up the face, its +Z out of the panel and its +X along the hinge. A lid swings out of the face
// about its hinge line (+X: 180 degrees lays it over on the hinge's far side, inside out); a lever in the plane of the
// face, about the panel's normal (anticlockwise as the operator sees it).
struct CabinFlapPart {
    std::string part;   // the parts library's mesh (art/cabin_parts)
    std::string color;  // a look colour ("" the guards' paint)
};

struct CabinFlap {
    std::vector<CabinFlapPart> parts;  // swung with the control
    std::vector<CabinFlapPart> fixed;  // on the panel at the hinge, never moving (brackets, lugs)
    double hinge_u_m = 0.0;            // the hinge's axis from the control's place (m, right and up)
    double hinge_v_m = 0.0;
    double lift_m = 0.0;               // and out of the face
    double axis_deg = 0.0;
    bool in_plane = false;             // a lever turned in the face, not a lid lifted out of it
    std::vector<double> angles_deg;    // per position of the control

    double AngleDeg(int position) const {
        if (angles_deg.empty()) return 0.0;
        const std::size_t k = position < 0 ? 0 : static_cast<std::size_t>(position);
        return angles_deg[k < angles_deg.size() ? k : angles_deg.size() - 1];
    }
};

struct CabinControl {
    std::string control_id;
    std::string panel_id;
    double u_m = 0.0;
    double v_m = 0.0;
    CabinStyle style = CabinStyle::Toggle;
    double units_per_turn = 360.0;  // wheels: value change for one full turn of the hand wheel
    bool potentiometer = false;     // a knob turned between two stops (a wheel the layout gives no units_per_turn and
                                    // whose range doesn't wrap): its whole range is kPotTravelDeg of turn
    bool auto_placed = false;       // a guard placed over the control it guards
    int red_dot = -1;               // toggles and rotaries: the position a red dot marks on the panel, or -1
    CabinLook look;                 // optional appearance (kind Default = by style)
    std::string watch;              // wheels: the display its crewman watches while he turns it ("" none): the front
                                    // end turns his head onto it while the wheel is held
    std::vector<double> angles_deg; // rotaries: where its pointer stands in each position, degrees clockwise from up (empty:
                                    // spread evenly, PositionAngleDeg)
    // A locking bar over it (СМЕНА ЛИТ.'s, SAM Simulator's osa050-052): hinged on an axis up the face at `bar_hinge_m` from
    // the control (m, right and up), `bar_length_m` long; it lies over the knob below position `bar_open_from` and swings
    // out to the left from it (SM_LetterBar).
    bool has_bar = false;
    double bar_hinge_u_m = 0.0;
    double bar_hinge_v_m = 0.0;
    double bar_length_m = 0.0;
    int bar_open_from = 1;

    bool BarOpen(int position) const { return has_bar && position >= bar_open_from; }

    // A hinged part drawn in its place (CabinFlap): a guard's real cover, a wheel lock's lever.
    bool has_flap = false;
    CabinFlap flap;
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
    std::vector<Condition> flash_when;   // optional: while all hold, the lamp flashes instead of burning steadily
    CabinLook look;                      // optional appearance (kind Default = a plain lamp cap)
};

// The readouts a "counters" display's digit counters can show (their values: readouts.hpp).
inline bool KnownReadout(const std::string& name) {
    for (const char* known : {"none", "range_error", "elevation_error", "azimuth_coarse", "azimuth_fine",
                              "elevation_coarse", "elevation_fine"}) {
        if (name == known) return true;
    }
    return false;
}

// aspd_scope: the command post's IT-45 (command_post.hpp: the ASPD-U's tracks on the scale the console's post block names,
// the DT-2 marker), with the grid glass over it (`glass`); status_board: the IT-1M / IT-2M boards (the launchers' reports);
// plot_board: the plotter's air-situation map (the board's marks in grease pencil); keypad_window: the ДХ keypad's window
// (the number it is on, ЦУ). All need the console's post block (keypad_window its keypad).
enum class CabinDisplayKind { Ppi, RangeScope, ElevationScope, Tv, Dial, Meter, Counters, AspdScope, StatusBoard, PlotBoard,
                              KeypadWindow, Document };

inline const char* ToString(CabinDisplayKind k) {
    switch (k) {
        case CabinDisplayKind::Ppi: return "ppi";
        case CabinDisplayKind::RangeScope: return "range_scope";
        case CabinDisplayKind::ElevationScope: return "elevation_scope";
        case CabinDisplayKind::Tv: return "tv";
        case CabinDisplayKind::Dial: return "dial";
        case CabinDisplayKind::Meter: return "meter";
        case CabinDisplayKind::Counters: return "counters";
        case CabinDisplayKind::AspdScope: return "aspd_scope";
        case CabinDisplayKind::StatusBoard: return "status_board";
        case CabinDisplayKind::PlotBoard: return "plot_board";
        case CabinDisplayKind::KeypadWindow: return "keypad_window";
        case CabinDisplayKind::Document: return "document";
    }
    return "?";
}

// Where one search beam's raster sits on an elevation_scope's screen (0 = bottom, 1 = top). The beam's elevations
// are the console's (TrackSensorDef::beams_deg).
struct ElevationRaster {
    double screen_low = 0.0;
    double screen_high = 1.0;
};

// One reading of a gauge worked out from the console's state (a meter's or a dial's `levels`): the first level whose
// conditions hold sets the needle, to `value` in the display's units, times the progress (0..1) of the subsystem `ramp`
// when one is named (a turbine running up and down). No level holding: the needle rests at 0.
struct CabinNeedleLevel {
    std::vector<Condition> when;      // all must hold
    std::vector<Condition> when_any;  // if not empty, at least one must also hold
    double value = 0.0;
    std::string ramp;
};

// The potentiometers that set a scope's picture (voenizdat_osa_1991 p50, p52, p98-99, p103: ЯРКОСТЬ, ФОКУС, КОНТРАСТНОСТЬ,
// ЯРКОСТЬ МЕТОК, ЯРКОСТЬ ВИЗИРА, ОСВЕЩЕНИЕ ШКАЛЫ and РРУ on the search scope, КОНТРАСТ., ЯРКОСТЬ, ФОКУС on the elevation
// scope and the TV, a brightness and a focus per sweep on the range indicator): each a wheel, empty where none is fitted.
struct CabinPictureControls {
    std::string brightness;    // the trace and the echoes (the second sweep's: brightness_2)
    std::string focus;         // the spot's focus (focus_2)
    std::string contrast;      // the echoes against the background
    std::string marks;         // the range marks
    std::string cursor;        // the cursor (ВИЗИР), the TV's cross, the range gate's strobe (`gate`)
    std::string gain;          // the receiver's gain (РРУ): the noise, the echoes, a jammer's sector
    std::string scale_light;   // the azimuth scale's lighting
    std::string brightness_2;
    std::string focus_2;
    std::string gate;
    std::string astig;         // the spot's astigmatism (astig_2): smeared as by focus
    std::string astig_2;
    std::string hshift;        // the trace moved across the screen (ГОРИЗ. СМЕЩ.; the second sweep's hshift_2)
    std::string vshift;        // and up and down (ВЕРТ. СМЕЩ.; vshift_2)
    std::string hshift_2;
    std::string vshift_2;
    std::vector<Condition> gain_when;  // the gain knob works only while these hold (controls only: РРУ, not АРУ);
                                       // otherwise the automatic gain control holds the set-up picture
    std::string gain_boost;    // a switch: off its first position the gain is raised (ДОП. УСИЛ.)
    bool Any() const {
        return !(brightness.empty() && focus.empty() && contrast.empty() && marks.empty() && cursor.empty() && gain.empty() &&
                 scale_light.empty() && brightness_2.empty() && focus_2.empty() && gate.empty() && astig.empty() &&
                 astig_2.empty() && hshift.empty() && vshift.empty() && hshift_2.empty() && vshift_2.empty() &&
                 gain_boost.empty());
    }
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
    std::string reading;                 // meter: or a reading worked out from the console (radio_sets.hpp), 0..1 of the swing;
                                         // "supply_voltage" reads volts (the 220 V 400 Hz supply, SupplyVoltage)
    std::string readout;                 // dial: or a value the simulation gives (simcore/readouts.hpp, DialReading)
    std::vector<CabinNeedleLevel> levels;  // meter or dial: or a reading from the console's state (LevelReading)
    int marks = 12;                      // labelled divisions on the scale
    // dial: one needle revolution = value_per_turn; the scale reads 0..labels_per_turn around it
    double value_per_turn = 360.0;
    double labels_per_turn = 360.0;
    // meter: reading = value * scale, full deflection at full_scale, in `unit`; the scale's left end at scale_from (a
    // centre-zero meter has scale_from = -full_scale)
    double scale = 1.0;
    double full_scale = 1.0;
    double scale_from = 0.0;
    std::string unit;
    std::string face;                    // meter: "arched", a window with its top corners rounded and its scale on an arc
                                         // over a black pointer hump (ДЦ on 0004-11M, samsim_panels); empty a plain face
    // ppi
    double sweep_period_s = 2.0;
    double afterglow_s = 3.0;
    // ppi (optional): a ruled disc over the screen that a control turns, its parallel lines `course_spacing_m` apart on
    // the scale `course_span_m` long (the Osa's course-parameter scale); the lines run along the control's azimuth
    std::string course_control;
    int course_lines = 0;
    double course_spacing_m = 2000.0;
    double course_span_m = 35000.0;
    // range_scope: span of the magnified lower sweep, centred on the range gate
    double magnified_window_m = 1500.0;
    // elevation_scope (also uses afterglow_s): one raster per search beam of the console's elevation search, and
    // the range window across the screen, centred on the range gate
    std::vector<ElevationRaster> rasters;
    double range_window_m = 2000.0;
    // counters: what each lit digit counter reads (simcore/readouts.hpp), left to right
    std::vector<std::string> readouts;
    // aspd_scope: the grid glass over the tube, with the plotting board's marks on it
    bool glass = false;
    // document: a paper kept at the seat (the talk tables, the radio data: docs/crew/12_COMMUNICATIONS.md), drawn from
    // the scenario's own (`talk_tables`, `radio_data`)
    std::string document;
    // a scope's picture knobs (ScopePictureOf)
    CabinPictureControls picture;
};

// The voltmeter on a vehicle's distribution cabinet (the Osa's 0096-9M1: voenizdat_osa_1991 p163, p247, where the operator
// checks 220 V 400 Hz before АППАРАТУРА ВКЛ.): the supply's volts while the console's "power" subsystem (a source on line,
// GTA, engine generator or an external supply) is ready, else none. The supply's own wander is not modelled.
inline constexpr double kSupplyVolts = 220.0;
inline double SupplyVoltage(const ConsoleData& console) {
    return console.equipment.Has("power") && console.equipment.State("power") == SubsystemState::Ready ? kSupplyVolts : 0.0;
}

// A gauge's reading from its levels (CabinNeedleLevel): the first level that holds, else 0.
inline double LevelReading(const CabinDisplay& d, const ConsoleData& console) {
    for (const CabinNeedleLevel& l : d.levels) {
        if (!console.equipment.AllHold(l.when, console.panel)) continue;
        if (!l.when_any.empty() && !console.equipment.AnyHolds(l.when_any, console.panel)) continue;
        return l.ramp.empty() ? l.value : l.value * console.equipment.Progress(l.ramp);
    }
    return 0.0;
}

// What a meter's needle reads now, in its display units: its levels, its control's value times `scale`, or its reading
// (0..1 of the swing, radio_sets.hpp) times `full_scale`. Worked out from the console alone, so a mirrored console reads
// the same.
inline double MeterValue(const CabinDisplay& d, const ConsoleData& console) {
    if (!d.levels.empty()) return LevelReading(d, console);
    if (d.reading == "supply_voltage") return SupplyVoltage(console);
    if (!d.reading.empty()) return RadioMeterReading(d.reading, console) * d.full_scale;
    return console.panel.GetValue(d.control) * d.scale;
}

// One engraved line of a PPI's course-parameter scale, in units of the tube's radius: the centre at (0, 0), x to the
// right, y up.
struct CabinScaleLine {
    double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
};

// The course-parameter scale's lines as the console has the disc turned: parallel to the azimuth its control reads
// (degrees clockwise from the top of the scale), evenly about the centre, each a chord of the tube. Empty without one.
inline std::vector<CabinScaleLine> CourseScaleLines(const CabinDisplay& d, const ConsoleData& console) {
    std::vector<CabinScaleLine> out;
    if (d.course_control.empty() || d.course_lines < 1) return out;
    const double a = console.panel.GetValue(d.course_control) * 3.14159265358979323846 / 180.0;
    const double dx = std::sin(a), dy = std::cos(a);  // along the course
    const double px = std::cos(a), py = -std::sin(a);  // across it, to the right
    const double step = d.course_spacing_m / d.course_span_m;
    for (int k = 0; k < d.course_lines; ++k) {
        const double o = (k - 0.5 * (d.course_lines - 1)) * step;
        if (std::fabs(o) >= 1.0) continue;
        const double h = std::sqrt(1.0 - o * o);
        out.push_back({o * px - h * dx, o * py - h * dy, o * px + h * dx, o * py + h * dy});
    }
    return out;
}

// A scope's picture as its potentiometers set it: what the display draws, from the console alone (a mirrored console
// draws the same). 1 is a picture set up as the drill asks (each knob at its middle; the gain full, voenizdat_osa_1991
// p52: full gain in combat); a knob with no control leaves its quantity at 1 (or 0 blur).
struct ScopePicture {
    double trace = 1.0;        // the sweep, echoes and noise: 0 below the tube's cut-off, up to 1.8 overdriven
    double trace_2 = 1.0;      // the range indicator's second sweep
    double marks = 1.0;
    double cursor = 1.0;
    double gate = 1.0;
    double contrast = 1.0;     // echoes against the noise: 0.3 flat .. 1.7 hard
    double blur = 0.0;         // the spot smeared: 0 sharp .. 1 a blur several spots wide (focus off, or overdriven)
    double blur_2 = 0.0;
    double gain = 1.0;         // the receiver's gain, 0..1 of full (more with the extra gain switched in)
    double scale_light = 1.0;  // the scale's lamps, 0 dark .. 1 full
    double shift_x = 0.0;      // the trace moved right, a fraction of the screen's width (shift_x_2: the second sweep's)
    double shift_y = 0.0;      // and up, a fraction of its height
    double shift_x_2 = 0.0;
    double shift_y_2 = 0.0;
};

// The curves (SAM3D's choice, not from a source; the sources say only what each knob is for): a brightness knob is dark
// below the tube's cut-off (0.2 of its turn), reaches the set-up picture at its middle and overdrives towards 1.8,
// blooming past three quarters; focus is sharp at its middle and smeared 0.3 of a turn either way; contrast from 0.3 to
// 1.7; the gain and the scale's light follow the knob.
inline double KnobFraction(const ControlPanel& panel, const std::string& id) {
    const ControlDef* def = panel.Find(id);
    if (def == nullptr || def->kind != ControlKind::Continuous || !(def->max_value > def->min_value)) return 0.5;
    const double f = (panel.GetValue(id) - def->min_value) / (def->max_value - def->min_value);
    return f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
}
inline double PictureLevel(double k) {
    constexpr double kCutOff = 0.2;
    if (k <= kCutOff) return 0.0;
    if (k <= 0.5) return (k - kCutOff) / (0.5 - kCutOff);
    return 1.0 + 0.8 * (k - 0.5) / 0.5;
}
inline double PictureBloom(double k) { return k > 0.75 ? 0.5 * (k - 0.75) / 0.25 : 0.0; }
inline double PictureBlur(double k) {
    const double b = std::fabs(k - 0.5) / 0.3;
    return b > 1.0 ? 1.0 : b;
}
// A shift knob moves the trace a tenth of the screen either way from its middle; the extra gain raises the gain by half
// (SAM3D's choices: the sources say only what each control is for).
inline constexpr double kPictureShift = 0.1;
inline constexpr double kPictureGainBoost = 1.5;
inline double PictureShift(double k) { return (k - 0.5) * 2.0 * kPictureShift; }

inline ScopePicture ScopePictureOf(const CabinDisplay& d, const ControlPanel& panel) {
    ScopePicture p;
    const CabinPictureControls& c = d.picture;
    auto knob = [&](const std::string& id) { return KnobFraction(panel, id); };
    double bloom = 0.0;
    double bloom_2 = 0.0;
    if (!c.brightness.empty()) {
        p.trace = PictureLevel(knob(c.brightness));
        bloom = PictureBloom(knob(c.brightness));
    }
    if (!c.brightness_2.empty()) {
        p.trace_2 = PictureLevel(knob(c.brightness_2));
        bloom_2 = PictureBloom(knob(c.brightness_2));
    }
    if (!c.marks.empty()) p.marks = PictureLevel(knob(c.marks));
    if (!c.cursor.empty()) p.cursor = PictureLevel(knob(c.cursor));
    if (!c.gate.empty()) p.gate = PictureLevel(knob(c.gate));
    if (!c.contrast.empty()) p.contrast = 0.3 + 1.4 * knob(c.contrast);
    const double focus = c.focus.empty() ? 0.0 : PictureBlur(knob(c.focus));
    const double focus_2 = c.focus_2.empty() ? 0.0 : PictureBlur(knob(c.focus_2));
    const double astig = c.astig.empty() ? 0.0 : PictureBlur(knob(c.astig));
    const double astig_2 = c.astig_2.empty() ? 0.0 : PictureBlur(knob(c.astig_2));
    p.blur = std::fmin(1.0, focus + astig + bloom);
    p.blur_2 = std::fmin(1.0, focus_2 + astig_2 + bloom_2);
    bool manual_gain = true;
    for (const Condition& w : c.gain_when) manual_gain = manual_gain && ControlConditionHolds(w, panel);
    if (!c.gain.empty() && manual_gain) p.gain = knob(c.gain);
    if (!c.gain_boost.empty() && panel.Get(c.gain_boost) != 0) p.gain *= kPictureGainBoost;
    if (!c.scale_light.empty()) p.scale_light = knob(c.scale_light);
    if (!c.hshift.empty()) p.shift_x = PictureShift(knob(c.hshift));
    if (!c.vshift.empty()) p.shift_y = PictureShift(knob(c.vshift));
    if (!c.hshift_2.empty()) p.shift_x_2 = PictureShift(knob(c.hshift_2));
    if (!c.vshift_2.empty()) p.shift_y_2 = PictureShift(knob(c.vshift_2));
    return p;
}

// How wide a noise jammer's sector shows on the search scope, as a share of its width at full gain and the set-up
// brightness: turning the gain and the brightness down narrows it, so the cursor can be laid on its middle
// (voenizdat_osa_1991 p266, firing at a noise jammer). Never below a fifth: the jammer's own beam stays.
inline double JamSectorScale(const ScopePicture& p) {
    const double s = p.gain * std::fmin(p.trace, 1.5);
    return s < 0.2 ? 0.2 : (s > 1.5 ? 1.5 : s);
}

// Where a meter's needle stands for a value: 0 at the scale's left end (scale_from), 1 at full_scale.
inline double MeterFraction(const CabinDisplay& d, double value) {
    return std::clamp((value - d.scale_from) / (d.full_scale - d.scale_from), 0.0, 1.0);
}

// The value at a meter's mark i (0..marks), left to right.
inline double MeterMark(const CabinDisplay& d, int i) {
    return d.scale_from + (d.full_scale - d.scale_from) * static_cast<double>(i) / static_cast<double>(d.marks);
}

// A rectangle of floor a crewman can walk on, in plan (body frame x and y, metres).
// A part that swings on a hinge (the PU-12M's plotting board, the 0096-9M2's cover): its panels (with everything on them)
// and shell meshes turn about `axis` (a unit vector, body frame; upright unless given) through `pivot_m`, by `folded_rad`
// (clockwise seen from the axis's tip: from above for an upright one) when folded and not at all when as built, as the
// console's `subsystem` goes from off to ready (its progress in between).
struct CabinHinge {
    std::string id;
    std::string label;
    std::vector<std::string> panels;
    std::vector<std::string> shell;
    Vec3 pivot_m;
    Vec3 axis{0.0, 0.0, 1.0};
    double folded_rad = 0.0;
    std::string subsystem;

    // A body point of the hinged parts, as built, when they have turned by `turn_rad` (clockwise seen from the axis's tip).
    Vec3 Turned(const Vec3& p, double turn_rad) const { return pivot_m + RotateAboutAxis(p - pivot_m, axis, -turn_rad); }
};

struct CabinWalkArea {
    double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
    bool Contains(double x, double y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};

struct CabinLayout {
    std::string system_id;
    double ceiling_m = 0.0;  // the roof's inside in the body frame, where a crewman getting up must keep his head under (0: none)
    std::vector<CabinWalkArea> walk;  // the floor he can walk on, up from his seat ("walk"); none: he stays seated
    std::vector<CabinHinge> hinges;   // parts that swing (CabinHinge)
    double stand_eye_m = 0.0;         // his eye's height on his feet (stooping under a low roof; "stand_eye_m")
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

    // A potentiometer's travel between its stops: the SP-series resistors behind the SŁ-2M's knobs turn through about
    // 270 degrees (SAM3D's figure for all of them; no source gives the Osa's).
    static constexpr double kPotTravelDeg = 270.0;

    // Which way a wheel's knob points at a value: degrees clockwise from up the face. A potentiometer from -135 at its
    // lower stop to +135 at its upper; a hand wheel a full turn per units_per_turn.
    static double WheelAngleDeg(const CabinControl& c, const ControlDef& def, double value) {
        if (c.potentiometer && def.max_value > def.min_value) {
            const double f = (value - def.min_value) / (def.max_value - def.min_value);
            return kPotTravelDeg * (std::fmin(1.0, std::fmax(0.0, f)) - 0.5);
        }
        return 360.0 * value / c.units_per_turn;
    }

    // Where a placed control's pointer stands in a position: its own angles where the layout gives them (a rotary whose
    // stops are not evenly spread: СМЕНА ЛИТ. holds НОРМ. in two of its steps), else PositionAngleDeg.
    static double ControlAngleDeg(const CabinControl& c, int positions, int position) {
        if (c.style == CabinStyle::Rotary && position >= 0 && static_cast<std::size_t>(position) < c.angles_deg.size()) {
            return c.angles_deg[static_cast<std::size_t>(position)];
        }
        return PositionAngleDeg(c.style, positions, position, c.look.up);
    }
    Vec3 PositionOf(const CabinLamp& l) const { return FindPanel(l.panel_id)->PointAt(l.u_m, l.v_m); }

    // Which way a toggle's lever, a rotary's pointer or a key points in a position: degrees on the panel face
    // from "up the face", clockwise (towards the operator's right). Toggles: up for on (>= 1), down for off.
    // Rotaries: positions spread symmetrically about up, at most 30 degrees apart. Keys: upright when
    // inserted, a quarter turn right when turned. 0 for other styles.
    static double PositionAngleDeg(CabinStyle style, int positions, int position, int up_position = 1) {
        switch (style) {
            case CabinStyle::Toggle:
                return (up_position == 0 ? position == 0 : position >= 1) ? 0.0 : 180.0;
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
        const double a = DegToRad(ControlAngleDeg(c, def.positions, c.red_dot));
        du_m = radius_m * std::sin(a);
        dv_m = radius_m * std::cos(a);
        return true;
    }

    static bool LampLit(const CabinLamp& lamp, const ControlPanel& panel, const Equipment& equipment) {
        if (!equipment.AllHold(lamp.lit_when, panel)) return false;
        return lamp.lit_when_any.empty() || equipment.AnyHolds(lamp.lit_when_any, panel);
    }

    // A lamp that is lit and flashes (a missile whose gyro has overheated): the renderer alternates it.
    static bool LampFlashing(const CabinLamp& lamp, const ControlPanel& panel, const Equipment& equipment) {
        return !lamp.flash_when.empty() && equipment.AllHold(lamp.flash_when, panel) && LampLit(lamp, panel, equipment);
    }

    static bool DisplayOn(const CabinDisplay& d, const ControlPanel& panel, const Equipment& equipment) {
        if (!equipment.AllHold(d.on_when, panel)) return false;
        return d.on_when_any.empty() || equipment.AnyHolds(d.on_when_any, panel);
    }
    static bool DisplayLit(const CabinDisplay& d, const ControlPanel& panel, const Equipment& equipment) {
        return DisplayOn(d, panel, equipment) && equipment.AllHold(d.lit_when, panel);
    }

    // Walking (a player up from his seat, roadmap 8 mobility): the plan point is on the floor he can walk on.
    bool Walkable(double x, double y) const {
        for (const CabinWalkArea& a : walk) {
            if (a.Contains(x, y)) return true;
        }
        return false;
    }

    // The nearest walkable plan point to (x, y) (the point itself if walkable); false without walking floor.
    bool NearestWalkable(double x, double y, double& out_x, double& out_y) const {
        double best = 1e18;
        for (const CabinWalkArea& a : walk) {
            const double cx = x < a.x0 ? a.x0 : (x > a.x1 ? a.x1 : x);
            const double cy = y < a.y0 ? a.y0 : (y > a.y1 ? a.y1 : y);
            const double d2 = (cx - x) * (cx - x) + (cy - y) * (cy - y);
            if (d2 < best) {
                best = d2;
                out_x = cx;
                out_y = cy;
            }
        }
        return !walk.empty();
    }

    // One step of walking from `from` towards `to` (the eye; z kept): all of it if the floor allows, else along x or y
    // alone (sliding along an edge), else none.
    Vec3 WalkStep(const Vec3& from, const Vec3& to) const {
        if (Walkable(to.x, to.y)) return Vec3{to.x, to.y, from.z};
        if (Walkable(to.x, from.y)) return Vec3{to.x, from.y, from.z};
        if (Walkable(from.x, to.y)) return Vec3{from.x, to.y, from.z};
        return from;
    }

    // Where a crewman stands when he gets up from a seat: the walkable point nearest his eye, at standing eye height.
    bool StandUpFrom(const CabinStation& seat, Vec3& out) const {
        double x = 0.0, y = 0.0;
        if (!NearestWalkable(seat.eye_m.x, seat.eye_m.y, x, y)) return false;
        out = Vec3{x, y, stand_eye_m};
        return true;
    }

    // The seat a standing crewman can sit down in from where he is: the nearest whose standing-up point is within
    // `radius_m` (plan distance); null if none.
    const CabinStation* SeatNear(const Vec3& eye, double radius_m) const {
        const CabinStation* best = nullptr;
        double best_d = radius_m;
        for (const CabinStation& st : stations) {
            Vec3 up;
            if (!StandUpFrom(st, up)) continue;
            const double d = std::sqrt((up.x - eye.x) * (up.x - eye.x) + (up.y - eye.y) * (up.y - eye.y));
            if (d <= best_d) {
                best_d = d;
                best = &st;
            }
        }
        return best;
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
        if (!Keys(root, "", {"system_id", "status", "notes", "ceiling_m", "walk", "stand_eye_m", "stations", "panels",
                             "controls", "lamps", "displays", "blocks", "fixtures", "shell", "hinges"})) {
            return false;
        }
        if (!RequireString(root, "", "system_id", out.system_id)) return false;
        if (!OptionalNumber(root, "", "ceiling_m", out.ceiling_m)) return false;
        if (!LoadWalk(root, out)) return false;
        if (out.system_id != console.system_id) {
            return Fail("system_id", "'" + out.system_id + "' doesn't match the console's '" + console.system_id + "'");
        }
        return LoadSeats(root, console, out) && LoadPanels(root, out) && LoadControlItems(root, console.panel, out) &&
               PlaceGuards(console.panel, out) && LoadLamps(root, console, out) && LoadDisplays(root, console, out) &&
               LoadBlocks(root, out) && LoadFixtures(root, out) && LoadShell(root, out) && LoadHinges(root, console, out) && CheckWatches(out);
    }

private:
    // The walking floor: rectangles from_m -> to_m in plan, and the standing eye's height (under the roof).
    bool LoadWalk(const json::Value& root, CabinLayout& out) {
        if (!OptionalNumber(root, "", "stand_eye_m", out.stand_eye_m)) return false;
        const json::Value* arr = OptionalArray(root, "", "walk");
        if (arr == nullptr) return message_.empty();
        if (!(out.stand_eye_m > 0.0)) return Fail("stand_eye_m", "required with walk");
        if (out.ceiling_m > 0.0 && out.stand_eye_m > out.ceiling_m - 0.08) {
            return Fail("stand_eye_m", "must leave the head under the ceiling (8 cm from the eye to the roof at least)");
        }
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("walk", i);
            const json::Value& w = arr->items[i];
            if (!Keys(w, path, {"from_m", "to_m", "notes"})) return false;
            double ax = 0.0, ay = 0.0, bx = 0.0, by = 0.0;
            if (!Number2(w, path, "from_m", ax, ay) || !Number2(w, path, "to_m", bx, by)) return false;
            CabinWalkArea area;
            area.x0 = ax < bx ? ax : bx;
            area.x1 = ax < bx ? bx : ax;
            area.y0 = ay < by ? ay : by;
            area.y1 = ay < by ? by : ay;
            if (area.x1 - area.x0 < 0.05 || area.y1 - area.y0 < 0.05) return Fail(path, "too narrow to walk in");
            out.walk.push_back(area);
        }
        return true;
    }

    // A wheel's "watch" names a display of the same seat.
    bool CheckWatches(const CabinLayout& out) {
        for (std::size_t i = 0; i < out.controls.size(); ++i) {
            const CabinControl& c = out.controls[i];
            if (c.watch.empty()) continue;
            const CabinDisplay* d = nullptr;
            for (const CabinDisplay& x : out.displays) {
                if (x.id == c.watch) d = &x;
            }
            const std::string path = Index("controls", i) + ".watch";
            if (d == nullptr) return Fail(path, "no display '" + c.watch + "'");
            const CabinPanel* mine = out.FindPanel(c.panel_id);
            const CabinPanel* theirs = out.FindPanel(d->panel_id);
            if (mine != nullptr && theirs != nullptr && mine->station != theirs->station) {
                return Fail(path, "'" + c.watch + "' is another seat's display");
            }
        }
        return true;
    }

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
            if (!Keys(p, path, {"id", "label", "station", "centre_m", "facing_deg", "tilt_deg", "size_m", "bare", "color", "notes"})) {
                return false;
            }
            CabinPanel panel;
            if (!OptionalBool(p, path, "bare", panel.bare) || !OptionalString(p, path, "color", panel.color)) return false;
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
            if (!panel.color.empty() && !KnownColour(panel.color)) return Fail(Join(path, "color"), "unknown colour '" + panel.color + "'");
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
            if (!Keys(c, path, {"control", "panel", "at_m", "style", "units_per_turn", "red_dot", "look", "watch", "angles_deg", "bar",
                                "flap", "notes"})) {
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
            } else if (def->kind == ControlKind::Continuous && !def->wraps && def->max_value > def->min_value) {
                // a potentiometer: its range is its travel between the stops, turned a little at a time
                item.potentiometer = true;
                item.units_per_turn = (def->max_value - def->min_value) * 360.0 / CabinLayout::kPotTravelDeg;
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
                if (item.look.kind == CabinLookKind::Signal) return Fail(Join(Join(path, "look"), "kind"), "signal is for lamps only");
            }
            if (!OptionalString(c, path, "watch", item.watch)) return false;
            if (!item.watch.empty() && item.style != CabinStyle::Wheel) return Fail(Join(path, "watch"), "for wheels only");
            if (const json::Value* angles = OptionalArray(c, path, "angles_deg")) {
                if (item.style != CabinStyle::Rotary) return Fail(Join(path, "angles_deg"), "for rotaries only");
                for (std::size_t k = 0; k < angles->items.size(); ++k) {
                    if (!angles->items[k].Is(json::Type::Number)) return Fail(Index(Join(path, "angles_deg"), k), "expected a number");
                    item.angles_deg.push_back(angles->items[k].number);
                }
                if (item.angles_deg.size() != static_cast<std::size_t>(def->positions)) {
                    return Fail(Join(path, "angles_deg"), "one angle per position of '" + item.control_id + "'");
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (const json::Value* bar = c.Find("bar")) {
                const std::string bp = Join(path, "bar");
                double open_from = 1.0;
                if (!Keys(*bar, bp, {"hinge_m", "length_m", "open_from", "notes"}) ||
                    !Number2(*bar, bp, "hinge_m", item.bar_hinge_u_m, item.bar_hinge_v_m) ||
                    !OptionalNumber(*bar, bp, "length_m", item.bar_length_m) || !OptionalNumber(*bar, bp, "open_from", open_from)) {
                    return false;
                }
                if (!(item.bar_length_m > 0.0) || item.bar_length_m > 0.2) return Fail(Join(bp, "length_m"), "from 0 to 0.2 m");
                item.bar_open_from = static_cast<int>(open_from);
                if (item.bar_open_from < 1 || item.bar_open_from >= def->positions) {
                    return Fail(Join(bp, "open_from"), "a position of '" + item.control_id + "' after its first");
                }
                item.has_bar = true;
            }
            if (const json::Value* flap = c.Find("flap")) {
                if (!LoadFlap(*flap, Join(path, "flap"), *def, item.flap)) return false;
                item.has_flap = true;
            }
            out.controls.push_back(std::move(item));
        }
        return true;
    }

    bool LoadFlapParts(const json::Value& flap, const std::string& path, const char* key, std::vector<CabinFlapPart>& out) {
        const json::Value* arr = OptionalArray(flap, path, key);
        if (arr == nullptr) return message_.empty();
        for (std::size_t k = 0; k < arr->items.size(); ++k) {
            const std::string pp = Index(Join(path, key), k);
            const json::Value& p = arr->items[k];
            CabinFlapPart part;
            if (!Keys(p, pp, {"part", "color"}) || !RequireString(p, pp, "part", part.part) ||
                !OptionalString(p, pp, "color", part.color)) {
                return false;
            }
            if (part.part.rfind("SM_", 0) != 0) return Fail(Join(pp, "part"), "a parts library mesh, SM_...");
            if (!part.color.empty() && !KnownColour(part.color)) return Fail(Join(pp, "color"), "unknown colour '" + part.color + "'");
            out.push_back(std::move(part));
        }
        return true;
    }

    bool LoadFlap(const json::Value& flap, const std::string& path, const ControlDef& def, CabinFlap& out) {
        std::string swing = "out";
        if (!Keys(flap, path, {"parts", "fixed", "hinge_m", "lift_m", "axis_deg", "swing", "angles_deg", "notes"}) ||
            !LoadFlapParts(flap, path, "parts", out.parts) || !LoadFlapParts(flap, path, "fixed", out.fixed) ||
            !Number2(flap, path, "hinge_m", out.hinge_u_m, out.hinge_v_m) || !OptionalNumber(flap, path, "lift_m", out.lift_m) ||
            !OptionalNumber(flap, path, "axis_deg", out.axis_deg) || !OptionalString(flap, path, "swing", swing)) {
            return false;
        }
        if (out.parts.empty()) return Fail(Join(path, "parts"), "at least one part");
        if (swing != "out" && swing != "plane") return Fail(Join(path, "swing"), "out or plane");
        out.in_plane = swing == "plane";
        if (std::fabs(out.hinge_u_m) > 0.2 || std::fabs(out.hinge_v_m) > 0.2 || out.lift_m < 0.0 || out.lift_m > 0.1) {
            return Fail(Join(path, "hinge_m"), "within 0.2 m of the control, lift_m from 0 to 0.1 m");
        }
        const json::Value* angles = OptionalArray(flap, path, "angles_deg");
        if (angles == nullptr) return Fail(Join(path, "angles_deg"), "one angle per position");
        for (std::size_t k = 0; k < angles->items.size(); ++k) {
            if (!angles->items[k].Is(json::Type::Number)) return Fail(Index(Join(path, "angles_deg"), k), "expected a number");
            out.angles_deg.push_back(angles->items[k].number);
        }
        if (out.angles_deg.size() != static_cast<std::size_t>(def.positions)) {
            return Fail(Join(path, "angles_deg"), "one angle per position of '" + def.id + "'");
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
            cover.has_bar = false;
            cover.has_flap = false;
            cover.flap = CabinFlap{};
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
            if (!Keys(l, path, {"id", "label", "panel", "at_m", "color", "lit_when", "lit_when_any", "flash_when", "look",
                                "source", "confidence", "notes"})) {
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
                !LoadConditions(l, path, "lit_when_any", console.panel, console.equipment, lamp.lit_when_any) ||
                !LoadConditions(l, path, "flash_when", console.panel, console.equipment, lamp.flash_when)) {
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
        for (const char* known : {"black", "white", "grey", "chrome", "brass", "red", "green", "amber", "yellow", "orange", "bluegrey",
                                  "lightgrey", "ivory", "blue", "cream", "copper", "steel", "nickel", "screw", "satin",
                                  "silver", "gunmetal"}) {
            if (c == known) return true;
        }
        return false;
    }

    // A look: "kind", "size_m" (a diameter, or [width, height]), "color", "ring", "label". Fixtures carry the same keys
    // inline and need a kind; a control's or lamp's "look" may leave it out (kind default).
    bool LoadLook(const json::Value& v, const std::string& path, bool inline_keys, CabinLook& out) {
        if (!inline_keys && !Keys(v, path, {"kind", "size_m", "color", "ring", "label", "scale", "label_at", "ink", "shape", "up"})) {
            return false;
        }
        std::string kind;
        if (!OptionalString(v, path, "kind", kind)) return false;
        if (inline_keys && kind.empty()) return Fail(Join(path, "kind"), "required");
        if (!kind.empty()) {
            bool found = false;
            for (int k = 0; k <= static_cast<int>(CabinLookKind::HexButton); ++k) {
                if (kind == ToString(static_cast<CabinLookKind>(k))) {
                    out.kind = static_cast<CabinLookKind>(k);
                    found = true;
                }
            }
            if (!found || (inline_keys && out.kind == CabinLookKind::Default)) {
                return Fail(Join(path, "kind"), "unknown look '" + kind + "'");
            }
            if (inline_keys && out.kind == CabinLookKind::InMesh) return Fail(Join(path, "kind"), "in_mesh is for controls only");
            if (!inline_keys && (out.kind == CabinLookKind::Text || out.kind == CabinLookKind::Scale || out.kind == CabinLookKind::Frame ||
                                 out.kind == CabinLookKind::Screw)) {
                return Fail(Join(path, "kind"), kind + " is for fixtures only");
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
            !OptionalString(v, path, "label", out.label) || !OptionalString(v, path, "scale", out.scale) ||
            !OptionalString(v, path, "label_at", out.label_at) || !OptionalString(v, path, "ink", out.ink) ||
            !OptionalString(v, path, "shape", out.shape)) {
            return false;
        }
        if (!out.ink.empty() && out.kind != CabinLookKind::Legend) return Fail(Join(path, "ink"), "for legends only");
        if (!out.ink.empty() && !KnownColour(out.ink)) return Fail(Join(path, "ink"), "unknown colour '" + out.ink + "'");
        if (!out.shape.empty() && (out.kind != CabinLookKind::Legend || out.shape != "stadium")) {
            return Fail(Join(path, "shape"), "\"stadium\", for legends only");
        }
        if (const json::Value* up = v.Find("up")) {
            if (!up->Is(json::Type::Number) || (up->number != 0.0 && up->number != 1.0)) {
                return Fail(Join(path, "up"), "the position that stands up: 0 or 1");
            }
            out.up = static_cast<int>(up->number);
        }
        if (!out.label_at.empty() && out.label_at != "above" && out.label_at != "below" && out.label_at != "left" &&
            out.label_at != "right") {
            return Fail(Join(path, "label_at"), "must be above, below, left or right");
        }
        if (!out.scale.empty()) {
            if (out.kind != CabinLookKind::Gauge) return Fail(Join(path, "scale"), "for gauges only");
            if (out.scale != "linear" && out.scale != "compass" && out.scale != "counters" &&
                out.scale != "hours") {
                return Fail(Join(path, "scale"), "unknown scale '" + out.scale + "' (linear, compass, counters or hours)");
            }
        }
        if (inline_keys) {
            if (const json::Value* marks = OptionalArray(v, path, "marks")) {
                for (std::size_t i = 0; i < marks->items.size(); ++i) {
                    const json::Value& m = marks->items[i];
                    // a scale's tick may stand without a figure (СКОРОСТЬ П А's middle, a lock's unlettered end)
                    if (!m.Is(json::Type::String) || (m.string.empty() && out.kind != CabinLookKind::Scale)) {
                        return Fail(Index(Join(path, "marks"), i), "expected a non-empty string");
                    }
                    out.marks.push_back(m.string);
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (!OptionalString(v, path, "unit", out.unit)) return false;
            if (!out.marks.empty() && out.kind != CabinLookKind::Gauge && out.kind != CabinLookKind::Scale) {
                return Fail(Join(path, "marks"), "for gauges and scales only");
            }
            if (!out.unit.empty() && out.kind != CabinLookKind::Gauge) return Fail(Join(path, "unit"), "for gauges only");
            if (out.kind == CabinLookKind::Gauge && !out.marks.empty() &&
                (out.marks.size() < 2 || (!out.scale.empty() && out.scale != "linear"))) {
                return Fail(Join(path, "marks"), "a linear dial's figures, two or more");
            }
            if (const json::Value* angles = OptionalArray(v, path, "angles_deg")) {
                for (std::size_t i = 0; i < angles->items.size(); ++i) {
                    if (!angles->items[i].Is(json::Type::Number)) return Fail(Index(Join(path, "angles_deg"), i), "expected a number");
                    out.angles_deg.push_back(angles->items[i].number);
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (out.kind == CabinLookKind::Scale && (out.marks.empty() || out.marks.size() != out.angles_deg.size())) {
                return Fail(Join(path, "angles_deg"), "a scale needs as many angles as marks");
            }
            if (!out.angles_deg.empty() && out.kind != CabinLookKind::Scale) return Fail(Join(path, "angles_deg"), "for scales only");
            if (!OptionalString(v, path, "arrow", out.arrow)) return false;
            if (!out.arrow.empty() && (out.kind != CabinLookKind::Scale ||
                                       (out.arrow != "first" && out.arrow != "last" && out.arrow != "both"))) {
                return Fail(Join(path, "arrow"), "a scale's arrowhead: first, last or both");
            }
            if (const json::Value* a = v.Find("angle_deg")) {
                if (!a->Is(json::Type::Number)) return Fail(Join(path, "angle_deg"), "expected a number");
                if (out.kind != CabinLookKind::Text && out.kind != CabinLookKind::Latch) {
                    return Fail(Join(path, "angle_deg"), "for text and latches only");
                }
                out.angle_deg = a->number;
            }
        }
        for (const std::string* c : {&out.color, &out.ring}) {
            if (!c->empty() && !KnownColour(*c)) {
                return Fail(Join(path, c == &out.color ? "color" : "ring"), "unknown colour '" + *c + "'");
            }
        }
        if ((out.kind == CabinLookKind::Nameplate || out.kind == CabinLookKind::Legend || out.kind == CabinLookKind::Text) &&
            out.label.empty()) {
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

    bool LoadHinges(const json::Value& root, const ConsoleData& console, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "hinges");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("hinges", i);
            const json::Value& v = arr->items[i];
            if (!Keys(v, path, {"id", "label", "panels", "shell", "pivot_m", "axis", "folded_deg", "subsystem", "source", "confidence", "notes"})) {
                return false;
            }
            CabinHinge h;
            if (!RequireString(v, path, "id", h.id) || !OptionalString(v, path, "label", h.label) || !Number3(v, path, "pivot_m", h.pivot_m) ||
                !Degrees(v, path, "folded_deg", h.folded_rad) || !RequireString(v, path, "subsystem", h.subsystem) || !Provenance(v, path)) {
                return false;
            }
            if (v.Find("axis") != nullptr) {
                if (!Number3(v, path, "axis", h.axis)) return false;
                if (h.axis.Length() < 1e-6) return Fail(Join(path, "axis"), "must not be zero");
                h.axis = h.axis.Normalized();
            }
            for (const char* key : {"panels", "shell"}) {
                const json::Value* list = OptionalArray(v, path, key);
                if (list == nullptr) {
                    if (!message_.empty()) return false;
                    continue;
                }
                for (std::size_t k = 0; k < list->items.size(); ++k) {
                    if (!list->items[k].Is(json::Type::String)) return Fail(Index(Join(path, key), k), "expected an id");
                    const std::string id = list->items[k].string;
                    if (std::string(key) == "panels") {
                        if (out.FindPanel(id) == nullptr) return Fail(Index(Join(path, key), k), "unknown panel '" + id + "'");
                        h.panels.push_back(id);
                    } else {
                        bool found = false;
                        for (const CabinShellPart& part : out.shell) found = found || part.id == id;
                        if (!found) return Fail(Index(Join(path, key), k), "unknown shell part '" + id + "'");
                        h.shell.push_back(id);
                    }
                }
            }
            if (h.panels.empty() && h.shell.empty()) return Fail(path, "swings nothing: give panels or shell parts");
            if (console.equipment.Def(h.subsystem) == nullptr) return Fail(Join(path, "subsystem"), "unknown subsystem '" + h.subsystem + "'");
            out.hinges.push_back(std::move(h));
        }
        return true;
    }

    bool LoadShell(const json::Value& root, CabinLayout& out) {
        const json::Value* arr = OptionalArray(root, "", "shell");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string path = Index("shell", i);
            const json::Value& v = arr->items[i];
            if (!Keys(v, path, {"id", "label", "kind", "centre_m", "size_m", "color", "mesh", "yaw_deg", "lumens", "source",
                                "confidence", "notes"})) {
                return false;
            }
            CabinShellPart part;
            std::string kind;
            if (!RequireString(v, path, "id", part.id) || !RequireString(v, path, "label", part.label) ||
                !RequireString(v, path, "kind", kind) || !Number3(v, path, "centre_m", part.centre_m) ||
                !Number3(v, path, "size_m", part.size_m) || !OptionalString(v, path, "color", part.color) ||
                !OptionalString(v, path, "mesh", part.mesh) || !Degrees(v, path, "yaw_deg", part.yaw_rad) ||
                !OptionalNumber(v, path, "lumens", part.lumens) || !Provenance(v, path)) {
                return false;
            }
            if (kind == "box") {
                part.kind = CabinShellKind::Box;
            } else if (kind == "glass") {
                part.kind = CabinShellKind::Glass;
            } else if (kind == "light") {
                part.kind = CabinShellKind::Light;
            } else if (kind == "mesh") {
                part.kind = CabinShellKind::Mesh;
            } else {
                return Fail(Join(path, "kind"), "must be box, glass, light or mesh");
            }
            if (!(part.size_m.x > 0.0) || !(part.size_m.y > 0.0) || !(part.size_m.z > 0.0)) {
                return Fail(Join(path, "size_m"), "must be positive");
            }
            const bool mesh = part.kind == CabinShellKind::Mesh;
            if (mesh && part.mesh.empty()) return Fail(Join(path, "mesh"), "required for a mesh part");
            if (!mesh && !part.mesh.empty()) return Fail(Join(path, "mesh"), "for mesh parts only");
            if (!mesh && v.Find("yaw_deg") != nullptr) return Fail(Join(path, "yaw_deg"), "for mesh parts only");
            if (mesh && !part.color.empty()) return Fail(Join(path, "color"), "a mesh part has the mesh's own colours");
            if (part.kind != CabinShellKind::Light && v.Find("lumens") != nullptr) {
                return Fail(Join(path, "lumens"), "for lights only");
            }
            if (part.lumens < 0.0) return Fail(Join(path, "lumens"), "must not be negative");
            bool known = part.color.empty() || part.color == "paint" || part.color == "floor" ||
                         (part.kind == CabinShellKind::Light && part.color == "blue");
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
            if (!Keys(f, path, {"panel", "at_m", "kind", "size_m", "color", "ring", "label", "scale", "marks", "unit", "mesh",
                                "label_at", "angles_deg", "angle_deg", "arrow", "ink", "shape", "notes"})) {
                return false;
            }
            CabinFixture fixture;
            if (!Placement(f, path, out, fixture.panel_id, fixture.u_m, fixture.v_m) ||
                !LoadLook(f, path, true, fixture.look) || !OptionalString(f, path, "mesh", fixture.mesh)) {
                return false;
            }
            if (fixture.look.kind == CabinLookKind::Signal) return Fail(Join(path, "kind"), "signal is for lamps only");
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
                                "control", "readout", "marks", "value_per_turn", "labels_per_turn", "scale", "full_scale",
                                "unit", "sweep_period_s", "afterglow_s", "magnified_window_m", "rasters", "range_window_m",
                                "readouts", "glass", "reading", "document", "levels", "scale_from", "course_scale", "picture", "face",
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
            allowed = {"sweep_period_s", "afterglow_s", "course_scale", "picture"};
            if (!console.has_sensors) return Fail(Join(path, "kind"), "a ppi needs the console's sensors block");
        } else if (kind == "range_scope") {
            disp.kind = CabinDisplayKind::RangeScope;
            allowed = {"magnified_window_m", "picture"};
            if (!console.has_sensors) return Fail(Join(path, "kind"), "a range_scope needs the console's sensors block");
        } else if (kind == "elevation_scope") {
            disp.kind = CabinDisplayKind::ElevationScope;
            allowed = {"afterglow_s", "rasters", "range_window_m", "picture"};
            if (!console.has_sensors || console.track.beams_deg.empty()) {
                return Fail(Join(path, "kind"), "an elevation_scope needs the console's track elevation_search");
            }
        } else if (kind == "tv") {
            disp.kind = CabinDisplayKind::Tv;
            allowed = {"picture"};
            if (!console.has_sensors || console.track.tv_camera.empty()) {
                return Fail(Join(path, "kind"), "a tv needs a TV camera in the console's sensors block");
            }
        } else if (kind == "dial") {
            disp.kind = CabinDisplayKind::Dial;
            allowed = {"control", "readout", "levels", "marks", "value_per_turn", "labels_per_turn"};
        } else if (kind == "meter") {
            disp.kind = CabinDisplayKind::Meter;
            allowed = {"control", "reading", "levels", "marks", "scale", "full_scale", "scale_from", "unit", "face"};
        } else if (kind == "counters") {
            disp.kind = CabinDisplayKind::Counters;
            allowed = {"readouts"};
            if (!console.has_sensors) return Fail(Join(path, "kind"), "counters need the console's sensors block");
        } else if (kind == "aspd_scope") {
            disp.kind = CabinDisplayKind::AspdScope;
            allowed = {"glass", "sweep_period_s", "afterglow_s"};
            if (!console.has_post) return Fail(Join(path, "kind"), "an aspd_scope needs the console's post block");
        } else if (kind == "status_board") {
            disp.kind = CabinDisplayKind::StatusBoard;
            if (!console.has_post) return Fail(Join(path, "kind"), "a status_board needs the console's post block");
        } else if (kind == "plot_board") {
            disp.kind = CabinDisplayKind::PlotBoard;
            if (!console.has_post) return Fail(Join(path, "kind"), "a plot_board needs the console's post block");
        } else if (kind == "keypad_window") {
            disp.kind = CabinDisplayKind::KeypadWindow;
            if (console.post_keypad_digits.empty()) return Fail(Join(path, "kind"), "a keypad_window needs the console's post keypad");
        } else if (kind == "document") {
            disp.kind = CabinDisplayKind::Document;
            allowed = {"document"};
            if (!OptionalString(d, path, "document", disp.document)) return false;
            if (disp.document != "talk_tables" && disp.document != "radio_data") return Fail(Join(path, "document"), "talk_tables or radio_data");
        } else {
            return Fail(Join(path, "kind"), "must be ppi, range_scope, elevation_scope, tv, dial, meter, counters, aspd_scope, "
                                            "status_board, plot_board, keypad_window or document");
        }
        for (const char* key : {"control", "reading", "readout", "levels", "marks", "value_per_turn", "labels_per_turn", "scale",
                                "full_scale", "scale_from", "unit", "face",
                                "sweep_period_s", "afterglow_s", "magnified_window_m", "rasters",
                                "range_window_m", "readouts", "glass", "document", "course_scale", "picture"}) {
            bool ok = d.Find(key) == nullptr;
            for (const std::string& a : allowed) ok = ok || a == key;
            if (!ok) return Fail(Join(path, key), "not used by a " + kind);
        }
        if (d.Find("levels") != nullptr) {
            // A gauge read from the console's state by its levels, not a control, reading or readout.
            for (const char* key : {"control", "reading", "readout"}) {
                if (d.Find(key) != nullptr) return Fail(Join(path, key), "a gauge with levels reads nothing else");
            }
            if (!LoadLevels(d, path, console, disp)) return false;
        } else if (disp.kind == CabinDisplayKind::Meter && d.Find("reading") != nullptr) {
            // A meter that reads the set rather than a control: one of the radio sets' readings, and that set on the console.
            if (d.Find("control") != nullptr) return Fail(path, "a meter shows a control or a reading, not both");
            if (!RequireString(d, path, "reading", disp.reading)) return false;
            if (disp.reading == "supply_voltage") {
                if (!console.equipment.Has("power")) return Fail(Join(path, "reading"), "supply_voltage needs the console's power subsystem");
            } else if (!KnownRadioReading(disp.reading)) {
                return Fail(Join(path, "reading"), "unknown reading '" + disp.reading + "'");
            }
            if ((disp.reading == "r111_indicator" && !console.r111.present) || (disp.reading == "r407_meter" && !console.r407.present)) {
                return Fail(Join(path, "reading"), "the console has no such radio set");
            }
        } else if (disp.kind == CabinDisplayKind::Dial && d.Find("readout") != nullptr) {
            if (d.Find("control") != nullptr) return Fail(Join(path, "readout"), "a dial reads a control or a readout, not both");
            if (!RequireString(d, path, "readout", disp.readout)) return false;
            if (disp.readout != "gyro_heading" && disp.readout != "fuel") return Fail(Join(path, "readout"), "must be gyro_heading or fuel");
            if (disp.readout == "gyro_heading" && !console.has_orientation) {
                return Fail(Join(path, "readout"), "gyro_heading needs the console's orientation block");
            }
            if (disp.readout == "fuel" && !console.has_fuel) return Fail(Join(path, "readout"), "fuel needs the console's fuel block");
        } else if (disp.kind == CabinDisplayKind::Dial || disp.kind == CabinDisplayKind::Meter) {
            if (!RequireString(d, path, "control", disp.control)) return false;
            if (!console.panel.Has(disp.control)) {
                return Fail(Join(path, "control"), "unknown control '" + disp.control + "'");
            }
        }
        if (!OptionalInt(d, path, "marks", disp.marks) || !OptionalNumber(d, path, "value_per_turn", disp.value_per_turn) ||
            !OptionalNumber(d, path, "labels_per_turn", disp.labels_per_turn) ||
            !OptionalNumber(d, path, "scale", disp.scale) || !OptionalNumber(d, path, "full_scale", disp.full_scale) ||
            !OptionalNumber(d, path, "scale_from", disp.scale_from) ||
            !OptionalString(d, path, "unit", disp.unit) || !OptionalString(d, path, "face", disp.face) ||
            !OptionalNumber(d, path, "sweep_period_s", disp.sweep_period_s) ||
            !OptionalNumber(d, path, "afterglow_s", disp.afterglow_s) ||
            !OptionalNumber(d, path, "magnified_window_m", disp.magnified_window_m) ||
            !OptionalNumber(d, path, "range_window_m", disp.range_window_m) || !OptionalBool(d, path, "glass", disp.glass)) {
            return false;
        }
        if (disp.marks < 1) return Fail(Join(path, "marks"), "must be at least 1");
        if (!disp.face.empty() && disp.face != "arched") return Fail(Join(path, "face"), "arched or none");
        if (!(disp.scale_from < disp.full_scale)) return Fail(Join(path, "scale_from"), "must be below full_scale");
        for (double v : {disp.value_per_turn, disp.full_scale, disp.sweep_period_s, disp.afterglow_s,
                         disp.magnified_window_m, disp.range_window_m}) {
            if (!(v > 0.0)) {
                return Fail(path, "value_per_turn, full_scale, sweep_period_s, afterglow_s, magnified_window_m and "
                                  "range_window_m must be positive");
            }
        }
        if (const json::Value* cs = d.Find("course_scale")) {
            const std::string p = Join(path, "course_scale");
            if (!Keys(*cs, p, {"control", "lines", "spacing_m", "span_m", "notes"}) ||
                !RequireString(*cs, p, "control", disp.course_control) || !OptionalInt(*cs, p, "lines", disp.course_lines) ||
                !OptionalNumber(*cs, p, "spacing_m", disp.course_spacing_m) || !OptionalNumber(*cs, p, "span_m", disp.course_span_m)) {
                return false;
            }
            if (!console.panel.Has(disp.course_control)) return Fail(Join(p, "control"), "unknown control '" + disp.course_control + "'");
            if (disp.course_lines < 1 || !(disp.course_spacing_m > 0.0) || !(disp.course_span_m > 0.0)) {
                return Fail(p, "lines, spacing_m and span_m must be positive");
            }
        }
        if (const json::Value* pic = d.Find("picture")) {
            const std::string p = Join(path, "picture");
            CabinPictureControls& c = disp.picture;
            if (!Keys(*pic, p, {"brightness", "focus", "contrast", "marks", "cursor", "gain", "scale_light", "brightness_2",
                                "focus_2", "gate", "astig", "astig_2", "hshift", "vshift", "hshift_2", "vshift_2",
                                "gain_when", "gain_boost", "notes"}) ||
                !OptionalString(*pic, p, "brightness", c.brightness) || !OptionalString(*pic, p, "focus", c.focus) ||
                !OptionalString(*pic, p, "contrast", c.contrast) || !OptionalString(*pic, p, "marks", c.marks) ||
                !OptionalString(*pic, p, "cursor", c.cursor) || !OptionalString(*pic, p, "gain", c.gain) ||
                !OptionalString(*pic, p, "scale_light", c.scale_light) || !OptionalString(*pic, p, "brightness_2", c.brightness_2) ||
                !OptionalString(*pic, p, "focus_2", c.focus_2) || !OptionalString(*pic, p, "gate", c.gate) ||
                !OptionalString(*pic, p, "astig", c.astig) || !OptionalString(*pic, p, "astig_2", c.astig_2) ||
                !OptionalString(*pic, p, "hshift", c.hshift) || !OptionalString(*pic, p, "vshift", c.vshift) ||
                !OptionalString(*pic, p, "hshift_2", c.hshift_2) || !OptionalString(*pic, p, "vshift_2", c.vshift_2) ||
                !OptionalString(*pic, p, "gain_boost", c.gain_boost) ||
                !LoadConditions(*pic, p, "gain_when", console.panel, console.equipment, c.gain_when)) {
                return false;
            }
            for (const Condition& w : c.gain_when) {
                if (!IsControlCondition(w)) return Fail(Join(p, "gain_when"), "only controls' positions");
            }
            if (!c.gain_when.empty() && c.gain.empty()) return Fail(Join(p, "gain_when"), "needs a gain knob");
            if (!c.gain_boost.empty()) {
                const ControlDef* def = console.panel.Find(c.gain_boost);
                if (def == nullptr) return Fail(Join(p, "gain_boost"), "unknown control '" + c.gain_boost + "'");
                if (def->kind != ControlKind::Discrete) return Fail(Join(p, "gain_boost"), "'" + c.gain_boost + "' is not a switch");
            }
            const std::pair<const char*, const std::string*> knobs[] = {
                {"brightness", &c.brightness}, {"focus", &c.focus}, {"contrast", &c.contrast}, {"marks", &c.marks},
                {"cursor", &c.cursor}, {"gain", &c.gain}, {"scale_light", &c.scale_light}, {"brightness_2", &c.brightness_2},
                {"focus_2", &c.focus_2}, {"gate", &c.gate}, {"astig", &c.astig}, {"astig_2", &c.astig_2},
                {"hshift", &c.hshift}, {"vshift", &c.vshift}, {"hshift_2", &c.hshift_2}, {"vshift_2", &c.vshift_2}};
            for (const auto& k : knobs) {
                if (k.second->empty()) continue;
                const ControlDef* def = console.panel.Find(*k.second);
                if (def == nullptr) return Fail(Join(p, k.first), "unknown control '" + *k.second + "'");
                if (def->kind != ControlKind::Continuous) return Fail(Join(p, k.first), "'" + *k.second + "' is not a knob that turns");
            }
            if (!c.Any()) return Fail(p, "names no control");
        }
        if (disp.kind == CabinDisplayKind::ElevationScope && !LoadElevationScope(d, path, console, disp)) return false;
        if (disp.kind == CabinDisplayKind::Counters && !LoadCounters(d, path, disp)) return false;
        return true;
    }

    bool LoadLevels(const json::Value& d, const std::string& path, const ConsoleData& console, CabinDisplay& disp) {
        const json::Value* arr = OptionalArray(d, path, "levels");
        if (arr == nullptr) return message_.empty() ? Fail(Join(path, "levels"), "required") : false;
        if (arr->items.empty()) return Fail(Join(path, "levels"), "needs at least one level");
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index(Join(path, "levels"), i);
            const json::Value& v = arr->items[i];
            if (!Keys(v, p, {"when", "when_any", "value", "ramp", "notes"})) return false;
            CabinNeedleLevel level;
            if (!LoadConditions(v, p, "when", console.panel, console.equipment, level.when) ||
                !LoadConditions(v, p, "when_any", console.panel, console.equipment, level.when_any)) {
                return false;
            }
            if (v.Find("value") == nullptr) return Fail(Join(p, "value"), "required");
            if (!OptionalNumber(v, p, "value", level.value) || !OptionalString(v, p, "ramp", level.ramp)) return false;
            if (!level.ramp.empty() && !console.equipment.Has(level.ramp)) {
                return Fail(Join(p, "ramp"), "unknown subsystem '" + level.ramp + "'");
            }
            disp.levels.push_back(std::move(level));
        }
        return true;
    }

    bool LoadCounters(const json::Value& d, const std::string& path, CabinDisplay& disp) {
        const json::Value* arr = OptionalArray(d, path, "readouts");
        if (arr == nullptr) return message_.empty() ? Fail(Join(path, "readouts"), "required") : false;
        if (arr->items.size() != 8) return Fail(Join(path, "readouts"), "needs eight counters");
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& r = arr->items[i];
            if (!r.Is(json::Type::String) || !KnownReadout(r.string)) {
                return Fail(Index(Join(path, "readouts"), i), "unknown readout");
            }
            disp.readouts.push_back(r.string);
        }
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
