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
#include <map>
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
    // Range rings on the scope, in metres, for each position of range_control (empty = the display's own).
    std::vector<std::vector<double>> range_rings_m;
    // Elevation beams (optional; beam_control empty = none). Each position of `beam_control` selects a programme: the
    // beams (indices into the radar's elevation beams) the antenna uses, one per turn, in turn. `beam_inputs` are
    // inputs set while that beam is the one radiating, for the beam lamps. `rotation_period_s` is one turn.
    std::string beam_control;
    std::vector<std::vector<int>> beam_programs;
    std::vector<std::string> beam_inputs;
    double rotation_period_s = 0.0;
    // IFF interrogator (optional; query empty = none): each press of `query` (or `control_query`) interrogates for
    // query_time_s; friendly aircraft within max_range_m reply. With `gate_control` at 1 the replies are gated by the
    // search radar's echoes (only targets it sees reply on the scope). `interrogating_input` is set while it runs.
    std::string iff_query;
    std::string iff_control_query;
    std::string iff_gate_control;
    std::string iff_ready;           // subsystem the interrogator needs
    std::string iff_interrogating_input;
    double iff_query_time_s = 1.0;
    double iff_max_range_m = 0.0;
};

// The vehicle's heading reference (optional): a gyro half-compass that holds the hull's heading once it has been
// oriented. While `mode_control` is at `set_position` and `relay_control` at `relay_position`, the gyro is slaved
// to the course setter (a wheel, degrees) and turns towards it; a press of `sync_button` makes it turn fast until
// they agree. The gyro runs while `gyro` is ready and loses its orientation when it stops.
struct OrientationDef {
    std::string gyro;
    std::string setter;
    std::string mode_control;
    int set_position = 1;
    std::string relay_control;
    int relay_position = 1;
    std::string sync_button;
    double slew_rate_deg_s = 1.0;
    double sync_rate_deg_s = 10.0;
    std::string synced_input;  // set while slaved and agreeing with the setter
    // Drift (roadmap 6.5): a gyro half-compass keeps its direction in space, so the Earth turning under it makes its
    // heading run off at earth_rate * sin(latitude). The latitude correction (with `relay_control` back at its other
    // position) cancels the part set on `latitude_control` (a wheel, degrees); what is left, plus the gyro's own drift
    // (degrees an hour, its sign drawn per run), moves the crew's frame away from north until they orient again.
    std::string latitude_control;
    double earth_rate_deg_h = 15.041;
    double own_drift_deg_h = 0.0;
    // The crew chief's compass work (orientation.hpp): seconds at a halt to find Qc with the artillery compass, and the
    // error of the result (standard deviation, degrees). `qc_known_input` is set while he has a Qc for this halt.
    double survey_s = 0.0;
    double survey_sigma_deg = 0.0;
    std::string qc_known_input;
    // Set while the gyro holds a heading that was set from the crew chief's Qc at this halt (cleared when the gyro
    // stops or the vehicle moves off).
    std::string oriented_input;
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
    std::string mti_control;     // tracking radar СДЦ / ШТ.: 1 = moving-target indication (may be empty)
    // Counter-jamming (roadmap 9.4): the switch of the circuit against a range-gate pull-off that walks the gate out
    // (УВ. ПОМЕХА, 1 = on; with the radar's EM model); the input set while a repeater holds the range gate (the
    // operator sees the target's mark split from the jamming pulse on the range indicator). Both may be empty.
    std::string rgpo_control;
    std::string range_pulled_input;
    double fov_wide_deg = 0.0;
    double fov_narrow_deg = 0.0;
    double fov_height_ratio = 1.0;  // the lens field's height over its width (a 4:3 raster: 0.75)
    double max_range_m = 0.0;
    double az_limit_deg = 360.0;  // wheel limits already enforce this; used to pick the reachable angle
    std::string captured_input;   // inputs the model sets (may be empty)
    std::string tv_track_input;   // the TV channel holds the target (manual TV tracking)
    std::string too_low_input;
    std::string near_zone_input;
    std::string in_zone_input;
    std::string crosshair_input;  // the TV-tracked target fills the narrow-lens crosshair (its range judged by its size)
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

// The missile load and the guidance channels (optional; roadmap 6.3). Containers belong to a channel (1 = I, 2 = II);
// at ВНИМАНИЕ (`attention_input`) the launcher picks one missile per usable channel, lowest `order` first, and the
// channel's command transmitter comes on. After a launch the channel's lamps follow the missile: СХОД at once,
// ЗАХВАТ after `capture_s`, СР.Л after `medium_s`, УЗКИЙ ЛУЧ after `narrow_s`, К3 when it is `fuse_range_m` from the
// target (with `jam_control` on, together with УЗКИЙ ЛУЧ). Launches are at least `min_interval_s` apart:
// `hold_input` is set while the next one must wait (the console's armed subsystem should require it false, so
// ПУСК ГОТОВ goes out and the crew waits; with no launcher running it stays false). When the selected missiles are spent, `pair_preparation` is
// restarted for the next ones (instead of the console's launch "restarts" after every launch).
// The R-123M sets and the R-124 intercom (roadmap 7.8, optional). Each radio names its controls and the two
// subsystems that say whether it can receive (warm) and transmit; `ranges_km` are the published ranges that become
// losses in the receiving chain; the intercom names the three boxes that matter and the shouting times.
struct CommsRadioDef {
    int set = 0;  // 0 left, 1 right (the C2 network's order)
    std::string name;
    std::string power, mode, squelch, volume, antenna;  // controls
    std::string warm, transmit;                         // subsystems
    std::string tone;                                   // ТОН. ВЫЗОВ button (optional): a call tone while transmitting
    // The preset set-up (roadmap 7.8, radio_setup.hpp; optional, all or none): the preset selector (positions 0-3 the
    // fixed frequencies, then ПЛАВНЫЙ ПОДДИАПАЗОН I and II), the fixators' cover and the four fixators (0 locked,
    // 1 unlocked), УСТАНОВКА ЧАСТОТЫ (a wheel, MHz), ПОДДИАПАЗОН (0 I, 1 II), the antenna-tuning knob's lock (0 locked)
    // and the knob (a wheel, divisions); the inputs set while the set is on its net's frequency and while it is keyed
    // with its antenna tuned (the tuning lamp at its brightest).
    std::string preset, fixator_cover, frequency, subband, antenna_lock, antenna_tune;
    std::vector<std::string> fixators;
    std::string on_frequency_input, tuned_input;
    bool HasSetup() const { return !frequency.empty(); }
};

struct CommsDef {
    std::vector<CommsRadioDef> radios;
    double range_whip_4m_km = 0.0;           // squelch off: the reference
    double range_whip_4m_squelch_km = 0.0;   // squelch set
    double range_whip_1m_km = 0.0;
    double range_wire_km = 0.0;
    std::string commander, deputy, driver;   // R-124 boxes A-1, A-2, PV
    int commander_radio_position = 0;
    int commander_off_position = 0;
    int deputy_radio_position = 0;
    int driver_radio_position = 0;
    std::string intercom_subsystem;
    std::string chest;                       // the commander's chest switch (optional): position 1 = ПРД, transmitting
    std::string driver_chest;                // the driver's (optional): with his ПВ on РС it keys the left set
    // The R-124's subscriber boxes and the seat each is at (optional): a seat without a box is not on the intercom, so an
    // order to it is shouted. Empty: every seat is on it.
    struct Subscriber {
        std::string box;
        std::string station;
    };
    std::vector<Subscriber> subscribers;
    // Tuning the sets (radio_setup.hpp): the motors' time to drive the knobs to a fixed frequency, the antenna circuit's
    // half-width in knob divisions, how fast the vibration walks a smooth-tuned frequency off on the move, the reading
    // at which the tuning lamp is at its brightest.
    double motor_s = 3.0;
    double antenna_width = 5.0;
    double smooth_drift_khz_s = 0.0;
    double tuned_reading = 0.9;
    double shout_quiet_s = 0.0;
    double shout_noisy_s = 0.0;
    std::vector<std::string> noisy;          // subsystems whose running makes the cabin too loud to talk
};

// The crew's voice paths ("voice", simcore/voice_comms.hpp, docs/crew/12_COMMUNICATIONS.md): each seat's headset and
// intercom box, and the radio sets the boxes reach. A seat hears the intercom, a set or a line only through its headset,
// worn and plugged into its box, with the box turned to it.
struct VoiceSetDef {
    std::string id;                    // "fug1", "right": what a box position names
    std::string name;                  // "R-123M FuG-1"
    std::string kind;                  // "r123m" (worked here) or "r111" (radio_sets.hpp works it)
    std::string power, mode, preset, volume, squelch;  // the R-123M's controls (mode: Д. ПРИЁМ / СИМПЛЕКС; preset: ФИКС. 1-4)
    int simplex_position = 1;          // the mode switch's СИМПЛЕКС
    std::string warm, transmit;        // subsystems: warmed up (10 min); able to transmit (3 min after Д. ПРИЁМ)
    double band_lo_mhz = 0.0, band_hi_mhz = 0.0;  // the frequencies it may work (a diplexer's share of the band); 0: any
    std::string net;                   // the net it works when the scenario's radio data names none ("battery", ...)
};

struct VoiceBoxDef {
    std::string id;                    // "a1", "rtk1", "pvs1"
    std::string name;                  // "А-1", "РТК-1", "ПВС-1"
    std::string station;
    std::string headset;               // control: СНЯТ / НАДЕТ
    std::string selector;              // control ("" : a box with no switch, always on the intercom)
    std::vector<std::string> positions;  // per selector position: "off", "intercom", "radio" or a set's id
    std::string radio;                 // the set "radio" names, if fixed
    std::string radio_select;          // or the control that picks it (the Osa's headset switch)
    std::vector<std::string> radio_select_sets;  // per position of radio_select
    std::string monitor;               // a second set heard (the RTK-1: it receives on two sets)
    std::vector<std::string> monitor_positions;  // "off" or a set's id
    std::vector<std::string> lines;    // line levers (the RTK-1's five lines): down / up
    std::string circular;              // the circular call button (ЦИРКУЛЯР)
    std::string power;                 // a subsystem this box needs besides the network's ("" : none)
    bool keeps_intercom = false;       // on a set it still hears and talks on the intercom (the commander's A-1, the RTK-1)
};

struct VoicePhoneDef {
    std::string id;                    // "ta57"
    std::string handset;               // control: on the hook / taken
    std::string station;               // the seat that answers it
};

struct VoiceDef {
    std::string system;                // "R-124" or "ПВС / РТК-1"
    std::string power;                 // subsystem: the boxes powered
    bool intercom_on_radio = false;    // a box on a set still hears the intercom (the PVS family) or leaves it (the R-124)
    std::vector<VoiceSetDef> sets;
    std::vector<VoiceBoxDef> boxes;
    std::vector<std::string> unboxed;  // seats with no box (the PU-12M's plotter)
    std::vector<VoicePhoneDef> phones;
    double shout_quiet_s = 1.5;        // a word across the cabin without the intercom (SAM3D estimates, as the Osa's comms)
    double shout_noisy_s = 4.0;
    std::vector<std::string> noisy;    // subsystems whose running makes the cabin too loud to talk
};

struct LauncherContainerDef {
    std::string id;           // e.g. "1-I"
    int channel = 1;          // 1 = channel I, 2 = channel II
    int order = 0;            // selection order within the channel, lowest first
    std::string loaded_input; // input set while a missile is in it (its ammunition lamp)
    std::string hot_input;    // input set while its missile's gyro is overheated (the lamp flashes; may be empty)
};

struct LauncherChannelDef {
    std::string transmitter_input;  // ВКЛ ВЫС
    std::string away_input;         // СХОД
    std::string captured_input;     // ЗАХВАТ
    std::string medium_input;       // СР.Л
    std::string narrow_input;       // УЗКИЙ ЛУЧ
    std::string fuse_input;         // К3
};

// The command transmitter's frequency letter (roadmap 6.3, optional): moving `control` to `change_position` changes the
// letter. Both channels' ЗАХВ. АПЧ lamps (`lock_inputs`, [0] = I, [1] = II) go out for `relock_s` and the next launch
// waits until they are back.
struct LauncherLetterDef {
    std::string control;
    int change_position = 0;
    std::vector<std::string> lock_inputs;
    double relock_s = 0.0;
};

// The emergency launch (roadmap 6.3, optional): `select_control` (ИЗДЕЛИЯ) picks a container per position (an empty id
// is ОТКЛ.); `fire_control` throws that missile off unguided, and it climbs for `flight_s` before it self-destructs.
// `ready_input` is set while the selector is on a loaded container (АВАР. ПУСК ГОТОВ).
struct LauncherEmergencyDef {
    std::string select_control;
    std::vector<std::string> select_containers;  // per position of select_control
    std::string fire_control;
    std::string ready_input;
    double flight_s = 0.0;
};

// How long a prepared missile may be held (roadmap 6.3, optional): after `hold_s` ready with a missile selected its gyro
// overheats, the pair is prepared again with the next missiles, and the overheated one rests for `rest_s` before it can
// be prepared again; after `cycles` such holds it rests `long_rest_s` instead.
struct LauncherPrepHoldDef {
    double hold_s = 0.0;
    double rest_s = 0.0;
    int cycles = 0;
    double long_rest_s = 0.0;
};

struct LauncherDef {
    bool has_prep_hold = false;
    LauncherPrepHoldDef prep_hold;
    // What the tracking radar's range indicator shows (Р I — ЦЕЛЬ — Р II, roadmap 6.3): per position of
    // `range_sweep_control`, the channel whose missile it follows (1, 2), or 0 for the target.
    std::string range_sweep_control;
    std::vector<int> range_sweep_channels;
    std::vector<LauncherContainerDef> containers;
    std::vector<LauncherChannelDef> channels;  // [0] = channel I, [1] = channel II
    std::string channel_control;               // КАНАЛЫ
    std::vector<std::vector<int>> channel_positions;  // per position: the channels in use, in firing order
    std::string attention_input;
    std::string jam_control;
    std::string pair_preparation;
    double capture_s = 0.0;
    double medium_s = 0.0;
    double narrow_s = 0.0;
    double fuse_range_m = 0.0;
    double nominal_flight_s = 0.0;  // how long a channel is busy when no outcome model times the flight
    double min_interval_s = 0.0;    // between two launches
    std::string hold_input;         // set while the next launch must wait (too soon, or no missile on a free channel)
    // Reloading from the transport-loader vehicle (TZM; roadmap 7.3): while `reload_input` is set and the vehicle is
    // stopped (`stopped_input`), the crane puts one missile into an empty container every `reload_s_per_missile`.
    std::string reload_input;
    std::string stopped_input;
    double reload_s_per_missile = 0.0;
    bool has_letter = false;
    LauncherLetterDef letter;
    bool has_emergency = false;
    LauncherEmergencyDef emergency;
};

// The vehicle's fuel (optional; roadmap 7.4, simcore/fuel.hpp): one tank, the power sources burning it, a reserve kept
// for driving (the power sources are cut at it, through `power_cut_input`), and refuelling from the TZM.
struct FuelSource {
    std::string subsystem;  // running (not off) = burning
    double stationary_lph = 0.0;
    double moving_lph = 0.0;
};

struct FuelDef {
    double tank_l = 0.0;
    double reserve_fraction = 0.0;
    std::vector<FuelSource> sources;
    std::string stopped_input;
    std::string refuel_input;  // the TZM alongside
    std::string refuel_input_alt;  // or a fuel truck alongside
    double refuel_lpm = 0.0;
    std::string power_cut_input;
    double refuel_hysteresis_l = 5.0;  // the crew asks again once refuelled this far above the reserve
};

// A state a session can start in instead of cold (roadmap 6.2): controls already set, inputs, subsystems already
// warmed up (in dependency order), and whether the vehicle is already oriented.
struct ReadinessState {
    std::string id;  // e.g. "powered", "deployed"
    std::string description;
    std::vector<std::pair<std::string, int>> controls;
    std::vector<std::pair<std::string, bool>> inputs;
    std::vector<std::string> ready;
    bool oriented = false;
};

// A crew seat that owns controls (e.g. "senior_operator"). Crew roles name these.
struct ConsoleStation {
    std::string id;
    std::string label;
};

// One radio set on the console as a set (simcore/radio_sets.hpp; "radios" in the console file): which control, subsystem and
// input plays each role. The roles each set needs are fixed (radio_sets.hpp: kR111Roles, kR407Roles).
struct RadioSetBinding {
    bool present = false;
    std::map<std::string, std::string> controls;    // role -> control id
    std::map<std::string, std::string> subsystems;  // role -> subsystem id
    std::map<std::string, std::string> inputs;      // role -> input id
    const std::string& Control(const std::string& role) const { return Get(controls, role); }
    const std::string& Subsystem(const std::string& role) const { return Get(subsystems, role); }
    const std::string& Input(const std::string& role) const { return Get(inputs, role); }

private:
    static const std::string& Get(const std::map<std::string, std::string>& m, const std::string& role) {
        static const std::string none;
        const auto it = m.find(role);
        return it == m.end() ? none : it->second;
    }
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
    bool has_orientation = false;
    OrientationDef orientation;
    bool has_launcher = false;
    LauncherDef launcher;
    bool has_comms = false;
    CommsDef comms;
    bool has_voice = false;
    VoiceDef voice;
    bool has_fuel = false;
    FuelDef fuel;
    // A command post's own information chain ("post", simcore/command_post.hpp): the subsystems that show the regiment's
    // telecode picture and a cabled radar's primary picture, the input the sim sets while the regiment's telecode is
    // within reach, the plotter's station, and the subsystem that says the post is oriented.
    bool has_post = false;
    std::string post_telecode;        // subsystem: the telecode picture on the IT-45
    std::string post_primary;         // subsystem: a cabled radar's picture on the IT-45
    std::string post_telecode_input;  // input: the regiment's telecode arriving
    std::string post_plotter;         // station: writes the board and tracks the primary picture
    std::string post_oriented;        // subsystem: the post oriented (its navigation error small)
    std::string post_scale;           // control: the IT-45's scale switch, one position per scale of the ASPD-U ("" none)
    std::string post_battery_net;     // subsystem: the battery net up, for designations and reports ("" always up)
    std::string post_air_net;         // subsystem: the air-picture voice net heard (FuG-1; "" none: the post never hears it)
    std::string post_imitation;       // subsystem: the ФР block at КОНТРОЛЬ, the imitated echo on the tube ("" none)
    std::string post_board_open;      // subsystem: the plotting board swung open ("" always open)
    std::string post_regiment_net;    // subsystem: the regiment's command net heard (the R-111; "" none)
    std::string post_radar_switch;    // control: the primary radar's source switch, positions 0-2 = the library's 1-3 ("" none)
    std::string post_radar_input;     // input: set while a cabled radar's picture comes through (the switch on its type)
    // The ДХ characteristics keypad (optional "keypad"): its ten digit keys, the designation key (ЦУ) and the reset; the
    // station that works it. And the DT-2 coordinate pickup (optional "marker"): two wheels that move the marker on the
    // IT-45 in azimuth (deg) and range (a fraction of the scale).
    std::vector<std::string> post_keypad_digits;
    std::string post_keypad_designate;
    std::string post_keypad_reset;
    std::string post_keypad_station;
    std::string post_keypad_friend, post_keypad_foe, post_keypad_group;  // СВ, Ч, ГР (optional)
    std::string post_keypad_zone[3];                                     // Н1, Н2, Н3 (optional)
    std::string post_marker_azimuth;
    std::string post_marker_range;
    // The post's radio sets worked as sets ("radios"): the R-111 (the regiment's command net) and the R-407 (the
    // regiment's telecode), and the data file with their numbers (relative to the data directory).
    std::string radios_file;
    RadioSetBinding r111;
    RadioSetBinding r407;
    std::vector<ReadinessState> readiness;

    const ReadinessState* FindReadiness(const std::string& id) const {
        for (const ReadinessState& r : readiness) {
            if (r.id == id) return &r;
        }
        return nullptr;
    }

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
                             "controls", "subsystems", "procedures", "launch", "launcher", "comms", "fuel", "readiness", "sensors", "post",
                             "radios", "voice"})) {
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
        if (!LoadLauncher(root, out) || !LoadComms(root, out)) return false;
        if (!LoadFuel(root, out)) return false;
        if (!LoadPost(root, out)) return false;
        if (!LoadRadios(root, out)) return false;
        if (!LoadVoice(root, out)) return false;
        if (!LoadReadiness(root, out)) return false;
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

    // A number the file must give (loaders in this class otherwise treat numbers as optional).
    bool NumberOf(const json::Value& obj, const std::string& path, const char* key, double& out) {
        if (obj.Find(key) == nullptr) return Fail(Join(path, key), "required");
        return OptionalNumber(obj, path, key, out);
    }

    bool LoadPost(const json::Value& root, ConsoleData& out) {
        const json::Value* p = root.Find("post");
        if (p == nullptr) return true;
        if (!Keys(*p, "post", {"telecode", "primary", "telecode_input", "plotter", "oriented", "scale", "battery_net", "air_net", "imitation",
                               "board_open", "regiment_net", "radar_switch", "radar_input", "keypad", "marker", "source", "confidence",
                               "notes"}) ||
            !OptionalString(*p, "post", "regiment_net", out.post_regiment_net) ||
            !OptionalString(*p, "post", "board_open", out.post_board_open) ||
            !OptionalString(*p, "post", "scale", out.post_scale) || !OptionalString(*p, "post", "battery_net", out.post_battery_net) ||
            !OptionalString(*p, "post", "air_net", out.post_air_net) || !OptionalString(*p, "post", "imitation", out.post_imitation) ||
            !OptionalString(*p, "post", "radar_switch", out.post_radar_switch) ||
            !OptionalString(*p, "post", "radar_input", out.post_radar_input) ||
            !Provenance(*p, "post") || !RequireString(*p, "post", "telecode", out.post_telecode) ||
            !RequireString(*p, "post", "primary", out.post_primary) || !RequireString(*p, "post", "telecode_input", out.post_telecode_input) ||
            !RequireString(*p, "post", "plotter", out.post_plotter) || !RequireString(*p, "post", "oriented", out.post_oriented)) {
            return false;
        }
        for (const auto& sub : {std::make_pair("telecode", &out.post_telecode), std::make_pair("primary", &out.post_primary),
                                std::make_pair("oriented", &out.post_oriented)}) {
            if (out.equipment.Def(*sub.second) == nullptr) return Fail(Join("post", sub.first), "unknown subsystem '" + *sub.second + "'");
        }
        if (!out.equipment.HasInput(out.post_telecode_input)) return Fail("post.telecode_input", "unknown input '" + out.post_telecode_input + "'");
        if (!out.post_scale.empty() && out.panel.Find(out.post_scale) == nullptr) return Fail("post.scale", "unknown control '" + out.post_scale + "'");
        if (!out.post_battery_net.empty() && out.equipment.Def(out.post_battery_net) == nullptr) {
            return Fail("post.battery_net", "unknown subsystem '" + out.post_battery_net + "'");
        }
        if (!out.post_air_net.empty() && out.equipment.Def(out.post_air_net) == nullptr) {
            return Fail("post.air_net", "unknown subsystem '" + out.post_air_net + "'");
        }
        if (!out.post_regiment_net.empty() && out.equipment.Def(out.post_regiment_net) == nullptr) {
            return Fail("post.regiment_net", "unknown subsystem '" + out.post_regiment_net + "'");
        }
        if (!out.post_board_open.empty() && out.equipment.Def(out.post_board_open) == nullptr) {
            return Fail("post.board_open", "unknown subsystem '" + out.post_board_open + "'");
        }
        if (!out.post_imitation.empty() && out.equipment.Def(out.post_imitation) == nullptr) {
            return Fail("post.imitation", "unknown subsystem '" + out.post_imitation + "'");
        }
        if (!out.post_radar_switch.empty() && out.panel.Find(out.post_radar_switch) == nullptr) {
            return Fail("post.radar_switch", "unknown control '" + out.post_radar_switch + "'");
        }
        if (!out.post_radar_input.empty() && !out.equipment.HasInput(out.post_radar_input)) {
            return Fail("post.radar_input", "unknown input '" + out.post_radar_input + "'");
        }
        if (!out.stations.empty() && out.FindStation(out.post_plotter) == nullptr) {
            return Fail("post.plotter", "'" + out.post_plotter + "' is not a station");
        }
        if (const json::Value* k = p->Find("keypad")) {
            const std::string kp = "post.keypad";
            if (!Keys(*k, kp, {"digits", "designate", "reset", "friend", "foe", "group", "zone1", "zone2", "zone3", "station"}) ||
                !RefControl(*k, kp, "designate", out.panel, ControlKind::Momentary, true, out.post_keypad_designate) ||
                !RefControl(*k, kp, "reset", out.panel, ControlKind::Momentary, true, out.post_keypad_reset) ||
                !RefControl(*k, kp, "friend", out.panel, ControlKind::Momentary, false, out.post_keypad_friend) ||
                !RefControl(*k, kp, "foe", out.panel, ControlKind::Momentary, false, out.post_keypad_foe) ||
                !RefControl(*k, kp, "group", out.panel, ControlKind::Momentary, false, out.post_keypad_group) ||
                !RefControl(*k, kp, "zone1", out.panel, ControlKind::Momentary, false, out.post_keypad_zone[0]) ||
                !RefControl(*k, kp, "zone2", out.panel, ControlKind::Momentary, false, out.post_keypad_zone[1]) ||
                !RefControl(*k, kp, "zone3", out.panel, ControlKind::Momentary, false, out.post_keypad_zone[2]) ||
                !RequireString(*k, kp, "station", out.post_keypad_station)) {
                return false;
            }
            if (!out.stations.empty() && out.FindStation(out.post_keypad_station) == nullptr) {
                return Fail(Join(kp, "station"), "'" + out.post_keypad_station + "' is not a station");
            }
            const json::Value* d = k->Find("digits");
            if (d == nullptr || !d->Is(json::Type::Array) || d->items.size() != 10) return Fail(Join(kp, "digits"), "needs the ten digit keys, 0 to 9");
            for (std::size_t i = 0; i < d->items.size(); ++i) {
                const json::Value& v = d->items[i];
                const ControlDef* c = v.Is(json::Type::String) ? out.panel.Find(v.string) : nullptr;
                if (c == nullptr || c->kind != ControlKind::Momentary) return Fail(Index(Join(kp, "digits"), i), "must be a button control");
                out.post_keypad_digits.push_back(v.string);
            }
        }
        if (const json::Value* m = p->Find("marker")) {
            const std::string mp = "post.marker";
            if (!Keys(*m, mp, {"azimuth", "range"}) ||
                !RefControl(*m, mp, "azimuth", out.panel, ControlKind::Continuous, true, out.post_marker_azimuth) ||
                !RefControl(*m, mp, "range", out.panel, ControlKind::Continuous, true, out.post_marker_range)) {
                return false;
            }
        }
        out.has_post = true;
        return true;
    }

    bool LoadFuel(const json::Value& root, ConsoleData& out) {
        const json::Value* f = root.Find("fuel");
        if (f == nullptr) return true;
        const std::string fp = "fuel";
        FuelDef& d = out.fuel;
        const Equipment& eq = out.equipment;
        if (!Keys(*f, fp, {"tank_l", "reserve_fraction", "sources", "stopped_input", "refuel_input", "refuel_input_alt", "refuel_lpm",
                           "power_cut_input", "source", "confidence", "notes"}) ||
            !Provenance(*f, fp) || !RefInput(*f, fp, "stopped_input", eq, d.stopped_input) ||
            !RefInput(*f, fp, "refuel_input", eq, d.refuel_input) || !RefInput(*f, fp, "refuel_input_alt", eq, d.refuel_input_alt) || !RefInput(*f, fp, "power_cut_input", eq, d.power_cut_input) ||
            !OptionalNumber(*f, fp, "refuel_lpm", d.refuel_lpm)) {
            return false;
        }
        if (d.stopped_input.empty()) return Fail(Join(fp, "stopped_input"), "required");
        if (!NumberOf(*f, fp, "tank_l", d.tank_l) || !NumberOf(*f, fp, "reserve_fraction", d.reserve_fraction)) {
            return false;
        }
        if (!(d.tank_l > 0.0)) return Fail(Join(fp, "tank_l"), "must be > 0");
        if (d.reserve_fraction < 0.0 || d.reserve_fraction >= 1.0) return Fail(Join(fp, "reserve_fraction"), "from 0 up to (not including) 1");
        if (d.refuel_lpm < 0.0) return Fail(Join(fp, "refuel_lpm"), "must be >= 0");
        if (d.refuel_lpm > 0.0 && d.refuel_input.empty()) return Fail(Join(fp, "refuel_lpm"), "needs refuel_input");
        const json::Value* srcs = OptionalArray(*f, fp, "sources");
        if (srcs == nullptr || srcs->items.empty()) {
            return message_.empty() ? Fail(Join(fp, "sources"), "expected a list of power sources") : false;
        }
        for (std::size_t i = 0; i < srcs->items.size(); ++i) {
            const std::string sp = Index(Join(fp, "sources"), i);
            const json::Value& v = srcs->items[i];
            FuelSource s;
            if (!Keys(v, sp, {"subsystem", "stationary_lph", "moving_lph"}) ||
                !RefSubsystem(v, sp, "subsystem", eq, true, s.subsystem) || !NumberOf(v, sp, "stationary_lph", s.stationary_lph) ||
                !NumberOf(v, sp, "moving_lph", s.moving_lph)) {
                return false;
            }
            if (s.stationary_lph < 0.0 || s.moving_lph < 0.0) return Fail(sp, "burn rates must be >= 0");
            d.sources.push_back(s);
        }
        out.has_fuel = true;
        return true;
    }

    // The post's radio sets ("radios": { "file", "r111": { "controls", "subsystems", "inputs" }, "r407": {...} }): each role
    // named once, to a control, subsystem or input of this console (the roles: radio_sets.hpp).
    bool LoadRadios(const json::Value& root, ConsoleData& out) {
        const json::Value* r = root.Find("radios");
        if (r == nullptr) return true;
        if (!r->Is(json::Type::Object) || !Keys(*r, "radios", {"file", "r111", "r407", "source", "confidence", "notes"}) ||
            !Provenance(*r, "radios") || !RequireString(*r, "radios", "file", out.radios_file)) {
            return message_.empty() ? Fail("radios", "expected an object") : false;
        }
        for (const auto& set : {std::make_pair("r111", &out.r111), std::make_pair("r407", &out.r407)}) {
            const json::Value* b = r->Find(set.first);
            if (b == nullptr) continue;
            const std::string bp = Join("radios", set.first);
            if (!b->Is(json::Type::Object) || !Keys(*b, bp, {"controls", "subsystems", "inputs"})) {
                return message_.empty() ? Fail(bp, "expected an object") : false;
            }
            RadioSetBinding& binding = *set.second;
            for (const char* group : {"controls", "subsystems", "inputs"}) {
                const json::Value* g = b->Find(group);
                if (g == nullptr || !g->Is(json::Type::Object)) return Fail(Join(bp, group), "required, an object");
                for (std::size_t i = 0; i < g->keys.size(); ++i) {
                    const std::string& role = g->keys[i];
                    const std::string rp = Join(Join(bp, group), role);
                    if (!g->items[i].Is(json::Type::String)) return Fail(rp, "expected an id");
                    const std::string& id = g->items[i].string;
                    if (std::string(group) == "controls") {
                        if (out.panel.Find(id) == nullptr) return Fail(rp, "unknown control '" + id + "'");
                        binding.controls[role] = id;
                    } else if (std::string(group) == "subsystems") {
                        if (out.equipment.Def(id) == nullptr) return Fail(rp, "unknown subsystem '" + id + "'");
                        binding.subsystems[role] = id;
                    } else {
                        if (!out.equipment.HasInput(id)) return Fail(rp, "unknown input '" + id + "'");
                        binding.inputs[role] = id;
                    }
                }
            }
            binding.present = true;
        }
        return true;
    }

    bool LoadReadiness(const json::Value& root, ConsoleData& out) {
        const json::Value* r = root.Find("readiness");
        if (r == nullptr) return true;
        if (!r->Is(json::Type::Object)) return Fail("readiness", "expected an object of state: {...}");
        for (std::size_t i = 0; i < r->keys.size(); ++i) {
            const std::string p = Join("readiness", r->keys[i]);
            const json::Value& v = r->items[i];
            ReadinessState st;
            st.id = r->keys[i];
            if (st.id == "cold") return Fail(p, "\"cold\" is the default and needs no entry");
            if (!Keys(v, p, {"description", "controls", "inputs", "ready", "oriented", "source", "confidence", "notes"}) ||
                !Provenance(v, p) || !OptionalString(v, p, "description", st.description)) {
                return false;
            }
            if (const json::Value* o = v.Find("oriented")) {
                if (!o->Is(json::Type::Bool)) return Fail(Join(p, "oriented"), "expected true or false");
                st.oriented = o->boolean;
            }
            if (const json::Value* cs = v.Find("controls")) {
                if (!cs->Is(json::Type::Object)) return Fail(Join(p, "controls"), "expected an object of control: position");
                for (std::size_t k = 0; k < cs->keys.size(); ++k) {
                    const std::string cp = Join(Join(p, "controls"), cs->keys[k]);
                    const ControlDef* c = out.panel.Find(cs->keys[k]);
                    if (c == nullptr) return Fail(cp, "unknown control");
                    const json::Value& pos = cs->items[k];
                    if (!pos.Is(json::Type::Number) || pos.number < 0.0 || pos.number >= c->positions ||
                        pos.number != static_cast<double>(static_cast<int>(pos.number))) {
                        return Fail(cp, "not a position of the control");
                    }
                    st.controls.emplace_back(cs->keys[k], static_cast<int>(pos.number));
                }
            }
            if (const json::Value* is = v.Find("inputs")) {
                if (!is->Is(json::Type::Object)) return Fail(Join(p, "inputs"), "expected an object of input: true/false");
                for (std::size_t k = 0; k < is->keys.size(); ++k) {
                    const std::string ip = Join(Join(p, "inputs"), is->keys[k]);
                    if (!out.equipment.HasInput(is->keys[k])) return Fail(ip, "unknown input");
                    if (!is->items[k].Is(json::Type::Bool)) return Fail(ip, "expected true or false");
                    st.inputs.emplace_back(is->keys[k], is->items[k].boolean);
                }
            }
            if (const json::Value* rs = OptionalArray(v, p, "ready")) {
                for (std::size_t k = 0; k < rs->items.size(); ++k) {
                    const std::string rp = Index(Join(p, "ready"), k);
                    if (!rs->items[k].Is(json::Type::String) || !out.equipment.Has(rs->items[k].string)) {
                        return Fail(rp, "expected the id of a subsystem");
                    }
                    st.ready.push_back(rs->items[k].string);
                }
            } else if (!message_.empty()) {
                return false;
            }
            out.readiness.push_back(std::move(st));
        }
        return true;
    }

    bool LoadSubscribers(const json::Value& ic, const std::string& ip, const ConsoleData& out, CommsDef& d) {
        const json::Value* arr = OptionalArray(ic, ip, "subscribers");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& e = arr->items[i];
            const std::string sp = Index(Join(ip, "subscribers"), i);
            CommsDef::Subscriber sub;
            if (!Keys(e, sp, {"box", "station", "source", "confidence", "notes"}) || !RequireString(e, sp, "box", sub.box) ||
                !RequireString(e, sp, "station", sub.station) || !Provenance(e, sp)) {
                return false;
            }
            if (out.FindStation(sub.station) == nullptr) return Fail(Join(sp, "station"), "unknown station '" + sub.station + "'");
            for (const CommsDef::Subscriber& o : d.subscribers) {
                if (o.box == sub.box || o.station == sub.station) return Fail(sp, "each box and each seat once");
            }
            d.subscribers.push_back(sub);
        }
        return true;
    }

    bool LoadComms(const json::Value& root, ConsoleData& out) {
        const json::Value* v = root.Find("comms");
        if (v == nullptr) return true;
        const std::string p = "comms";
        if (!v->Is(json::Type::Object)) return Fail(p, "expected an object");
        if (!Keys(*v, p, {"radios", "ranges_km", "intercom", "tuning", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
        CommsDef& d = out.comms;
        const Equipment& eq = out.equipment;
        const json::Value* radios = OptionalArray(*v, p, "radios");
        if (radios == nullptr || radios->items.size() != 2) {
            return message_.empty() ? Fail(Join(p, "radios"), "expected two radios (left, right)") : false;
        }
        for (std::size_t i = 0; i < 2; ++i) {
            const std::string rp = Index(Join(p, "radios"), i);
            const json::Value& e = radios->items[i];
            CommsRadioDef r;
            if (!Keys(e, rp, {"set", "name", "power", "mode", "squelch", "volume", "antenna", "warm", "transmit", "tone", "preset",
                              "fixator_cover", "fixators", "frequency", "subband", "antenna_lock", "antenna_tune",
                              "on_frequency_input", "tuned_input"}) ||
                !OptionalInt(e, rp, "set", r.set) || !RequireString(e, rp, "name", r.name) ||
                !RefControl(e, rp, "power", out.panel, ControlKind::Discrete, true, r.power) ||
                !RefControl(e, rp, "mode", out.panel, ControlKind::Discrete, true, r.mode) ||
                !RefControl(e, rp, "squelch", out.panel, ControlKind::Discrete, true, r.squelch) ||
                !RefControl(e, rp, "volume", out.panel, ControlKind::Discrete, true, r.volume) ||
                !RefControl(e, rp, "antenna", out.panel, ControlKind::Discrete, true, r.antenna) ||
                !RefSubsystem(e, rp, "warm", eq, true, r.warm) || !RefSubsystem(e, rp, "transmit", eq, true, r.transmit) ||
                !RefControl(e, rp, "tone", out.panel, ControlKind::Momentary, false, r.tone) ||
                !RefControl(e, rp, "preset", out.panel, ControlKind::Discrete, false, r.preset) ||
                !RefControl(e, rp, "fixator_cover", out.panel, ControlKind::Discrete, false, r.fixator_cover) ||
                !RefControl(e, rp, "frequency", out.panel, ControlKind::Continuous, false, r.frequency) ||
                !RefControl(e, rp, "subband", out.panel, ControlKind::Discrete, false, r.subband) ||
                !RefControl(e, rp, "antenna_lock", out.panel, ControlKind::Discrete, false, r.antenna_lock) ||
                !RefControl(e, rp, "antenna_tune", out.panel, ControlKind::Continuous, false, r.antenna_tune) ||
                !RefInput(e, rp, "on_frequency_input", eq, r.on_frequency_input) || !RefInput(e, rp, "tuned_input", eq, r.tuned_input)) {
                return false;
            }
            if (const json::Value* fx = e.Find("fixators")) {
                if (!fx->Is(json::Type::Array) || fx->items.size() != 4) return Fail(Join(rp, "fixators"), "expected the four fixators' controls");
                for (std::size_t k = 0; k < 4; ++k) {
                    const json::Value& c = fx->items[k];
                    const std::string fp = Index(Join(rp, "fixators"), k);
                    if (!c.Is(json::Type::String) || out.panel.Find(c.string) == nullptr) return Fail(fp, "expected a control id");
                    if (out.panel.Find(c.string)->kind != ControlKind::Discrete) return Fail(fp, "control '" + c.string + "' is the wrong kind");
                    r.fixators.push_back(c.string);
                }
            }
            const bool any = !r.preset.empty() || !r.fixator_cover.empty() || !r.frequency.empty() || !r.subband.empty() ||
                             !r.antenna_lock.empty() || !r.antenna_tune.empty() || !r.fixators.empty();
            if (any && (r.preset.empty() || r.fixator_cover.empty() || r.frequency.empty() || r.subband.empty() ||
                        r.antenna_lock.empty() || r.antenna_tune.empty() || r.fixators.size() != 4)) {
                return Fail(rp, "the preset set-up needs preset, fixator_cover, fixators, frequency, subband, antenna_lock and antenna_tune together");
            }
            if (!r.preset.empty() && out.panel.Find(r.preset)->positions != 6) {
                return Fail(Join(rp, "preset"), "needs six positions (fixed frequencies 1-4, smooth sub-band I and II)");
            }
            if (r.set != static_cast<int>(i)) return Fail(Join(rp, "set"), "radios are in the order of the C2 network's sets: 0, 1");
            if (out.panel.Find(r.antenna)->positions != 3) return Fail(Join(rp, "antenna"), "needs three positions (4 m whip, 1 m whip, emergency wire)");
            d.radios.push_back(std::move(r));
        }
        const json::Value* rg = v->Find("ranges_km");
        const std::string gp = Join(p, "ranges_km");
        if (rg == nullptr || !rg->Is(json::Type::Object) ||
            !Keys(*rg, gp, {"whip_4m_squelch_off", "whip_4m_squelch_on", "whip_1m", "emergency_wire"})) {
            return message_.empty() ? Fail(gp, "required, an object") : false;
        }
        struct Range {
            const char* key;
            double* value;
        };
        for (const Range& r : {Range{"whip_4m_squelch_off", &d.range_whip_4m_km}, Range{"whip_4m_squelch_on", &d.range_whip_4m_squelch_km},
                               Range{"whip_1m", &d.range_whip_1m_km}, Range{"emergency_wire", &d.range_wire_km}}) {
            if (rg->Find(r.key) == nullptr) return Fail(Join(gp, r.key), "required");
            if (!OptionalNumber(*rg, gp, r.key, *r.value)) return false;
            if (*r.value <= 0.0) return Fail(Join(gp, r.key), "must be > 0");
        }
        if (d.range_whip_4m_squelch_km > d.range_whip_4m_km) return Fail(Join(gp, "whip_4m_squelch_on"), "the suppressor can't lengthen the range");
        const json::Value* ic = v->Find("intercom");
        const std::string ip = Join(p, "intercom");
        if (ic == nullptr || !ic->Is(json::Type::Object) ||
            !Keys(*ic, ip, {"commander", "commander_radio_position", "commander_off_position", "deputy", "deputy_radio_position", "driver",
                            "driver_radio_position", "subsystem", "chest", "driver_chest", "subscribers", "shout_quiet_s", "shout_noisy_s",
                            "noisy"})) {
            return message_.empty() ? Fail(ip, "required, an object") : false;
        }
        if (!RefControl(*ic, ip, "commander", out.panel, ControlKind::Discrete, true, d.commander) ||
            !RefControl(*ic, ip, "deputy", out.panel, ControlKind::Discrete, true, d.deputy) ||
            !RefControl(*ic, ip, "driver", out.panel, ControlKind::Discrete, true, d.driver) ||
            !OptionalInt(*ic, ip, "commander_radio_position", d.commander_radio_position) ||
            !OptionalInt(*ic, ip, "commander_off_position", d.commander_off_position) ||
            !OptionalInt(*ic, ip, "deputy_radio_position", d.deputy_radio_position) ||
            !OptionalInt(*ic, ip, "driver_radio_position", d.driver_radio_position) ||
            !RefSubsystem(*ic, ip, "subsystem", eq, true, d.intercom_subsystem) ||
            !RefControl(*ic, ip, "chest", out.panel, ControlKind::Discrete, false, d.chest) ||
            !RefControl(*ic, ip, "driver_chest", out.panel, ControlKind::Discrete, false, d.driver_chest) ||
            !LoadSubscribers(*ic, ip, out, d) ||
            !OptionalNumber(*ic, ip, "shout_quiet_s", d.shout_quiet_s) || !OptionalNumber(*ic, ip, "shout_noisy_s", d.shout_noisy_s)) {
            return false;
        }
        struct Pos {
            const char* key;
            const std::string* control;
            int value;
        };
        for (const Pos& pos : {Pos{"commander_radio_position", &d.commander, d.commander_radio_position},
                               Pos{"commander_off_position", &d.commander, d.commander_off_position},
                               Pos{"deputy_radio_position", &d.deputy, d.deputy_radio_position},
                               Pos{"driver_radio_position", &d.driver, d.driver_radio_position}}) {
            if (pos.value < 0 || pos.value >= out.panel.Find(*pos.control)->positions) {
                return Fail(Join(ip, pos.key), "not a position of " + *pos.control);
            }
        }
        if (d.shout_quiet_s < 0.0 || d.shout_noisy_s < d.shout_quiet_s) {
            return Fail(Join(ip, "shout_noisy_s"), "shouting times: >= 0, and louder is not quicker");
        }
        if (const json::Value* noisy = OptionalArray(*ic, ip, "noisy")) {
            for (std::size_t i = 0; i < noisy->items.size(); ++i) {
                const json::Value& e = noisy->items[i];
                if (!e.Is(json::Type::String) || !eq.Has(e.string)) return Fail(Index(Join(ip, "noisy"), i), "unknown subsystem");
                d.noisy.push_back(e.string);
            }
        } else if (!message_.empty()) {
            return false;
        }
        out.has_comms = true;
        if (const json::Value* tn = v->Find("tuning")) {
            const std::string tp = Join(p, "tuning");
            if (!tn->Is(json::Type::Object) ||
                !Keys(*tn, tp, {"motor_s", "antenna_width", "smooth_drift_khz_s", "tuned_reading", "source", "confidence", "notes"}) ||
                !Provenance(*tn, tp) || !OptionalNumber(*tn, tp, "motor_s", d.motor_s) ||
                !OptionalNumber(*tn, tp, "antenna_width", d.antenna_width) ||
                !OptionalNumber(*tn, tp, "smooth_drift_khz_s", d.smooth_drift_khz_s) ||
                !OptionalNumber(*tn, tp, "tuned_reading", d.tuned_reading)) {
                return message_.empty() ? Fail(tp, "expected an object") : false;
            }
            if (d.motor_s < 0.0 || !(d.antenna_width > 0.0) || d.smooth_drift_khz_s < 0.0 || !(d.tuned_reading > 0.0 && d.tuned_reading <= 1.0)) {
                return Fail(tp, "motor_s and smooth_drift_khz_s >= 0, antenna_width > 0, tuned_reading in (0, 1]");
            }
        }
        return true;
    }

    // A list of strings (empty when absent); `vet` checks each one ("" : fine, else the complaint).
    template <typename Vet>
    bool StringList(const json::Value& obj, const std::string& path, const char* key, std::vector<std::string>& out, Vet vet) {
        const json::Value* a = OptionalArray(obj, path, key);
        if (a == nullptr) return message_.empty();
        for (std::size_t i = 0; i < a->items.size(); ++i) {
            const json::Value& e = a->items[i];
            if (!e.Is(json::Type::String)) return Fail(Index(Join(path, key), i), "expected a string");
            const std::string why = vet(e.string);
            if (!why.empty()) return Fail(Index(Join(path, key), i), why);
            out.push_back(e.string);
        }
        return true;
    }

    bool LoadVoice(const json::Value& root, ConsoleData& out) {
        const json::Value* v = root.Find("voice");
        if (v == nullptr) return true;
        const std::string p = "voice";
        if (!Keys(*v, p, {"system", "power", "intercom_on_radio", "sets", "boxes", "unboxed", "phones", "shout_quiet_s", "shout_noisy_s", "noisy",
                          "source", "confidence", "notes"}) ||
            !Provenance(*v, p)) {
            return false;
        }
        VoiceDef& d = out.voice;
        const Equipment& eq = out.equipment;
        if (!RequireString(*v, p, "system", d.system) || !RefSubsystem(*v, p, "power", eq, true, d.power) ||
            !OptionalBool(*v, p, "intercom_on_radio", d.intercom_on_radio) || !OptionalNumber(*v, p, "shout_quiet_s", d.shout_quiet_s) ||
            !OptionalNumber(*v, p, "shout_noisy_s", d.shout_noisy_s) ||
            !StringList(*v, p, "noisy", d.noisy, [&eq](const std::string& id) { return eq.Has(id) ? std::string() : "unknown subsystem '" + id + "'"; })) {
            return false;
        }
        const json::Value* sets = OptionalArray(*v, p, "sets");
        if (sets == nullptr && !message_.empty()) return false;
        for (std::size_t i = 0; sets != nullptr && i < sets->items.size(); ++i) {
            const std::string sp = Index(Join(p, "sets"), i);
            const json::Value& e = sets->items[i];
            VoiceSetDef s;
            if (!Keys(e, sp, {"id", "name", "kind", "power", "mode", "preset", "volume", "squelch", "simplex_position", "warm", "transmit",
                              "band_mhz", "net", "notes"}) ||
                !RequireString(e, sp, "id", s.id) || !RequireString(e, sp, "name", s.name) || !RequireString(e, sp, "kind", s.kind) ||
                !OptionalString(e, sp, "net", s.net) ||
                !OptionalInt(e, sp, "simplex_position", s.simplex_position) || !RefSubsystem(e, sp, "warm", eq, false, s.warm) ||
                !RefSubsystem(e, sp, "transmit", eq, false, s.transmit)) {
                return false;
            }
            if (s.kind != "r123m" && s.kind != "r111") return Fail(Join(sp, "kind"), "r123m or r111");
            const bool r123 = s.kind == "r123m";
            if (!RefControl(e, sp, "power", out.panel, ControlKind::Discrete, r123, s.power) ||
                !RefControl(e, sp, "mode", out.panel, ControlKind::Discrete, r123, s.mode) ||
                !RefControl(e, sp, "preset", out.panel, ControlKind::Discrete, r123, s.preset) ||
                !RefControl(e, sp, "volume", out.panel, ControlKind::Discrete, false, s.volume) ||
                !RefControl(e, sp, "squelch", out.panel, ControlKind::Discrete, false, s.squelch)) {
                return false;
            }
            if (r123 && (s.warm.empty() || s.transmit.empty())) return Fail(sp, "an R-123M needs warm and transmit subsystems");
            if (const json::Value* band = OptionalArray(e, sp, "band_mhz")) {
                if (band->items.size() != 2 || !band->items[0].Is(json::Type::Number) || !band->items[1].Is(json::Type::Number) ||
                    !(band->items[0].number < band->items[1].number)) {
                    return Fail(Join(sp, "band_mhz"), "[lowest, highest] MHz");
                }
                s.band_lo_mhz = band->items[0].number;
                s.band_hi_mhz = band->items[1].number;
            } else if (!message_.empty()) {
                return false;
            }
            // ФИКС. 1-4, and with the presets' set-up drill ПЛАВН. I-II (smooth tuning: on no numbered net's frequency).
            if (r123 && out.panel.Find(s.preset)->positions != 4 && out.panel.Find(s.preset)->positions != 6) {
                return Fail(Join(sp, "preset"), "four positions (ФИКС. 1-4), or six with ПЛАВН. I-II");
            }
            for (const VoiceSetDef& o : d.sets) {
                if (o.id == s.id) return Fail(Join(sp, "id"), "duplicate set '" + s.id + "'");
            }
            d.sets.push_back(std::move(s));
        }
        auto set_known = [&d](const std::string& id) {
            for (const VoiceSetDef& s : d.sets) {
                if (s.id == id) return true;
            }
            return false;
        };
        auto station_known = [&out](const std::string& st) {
            if (out.stations.empty()) return true;
            for (const ConsoleStation& s : out.stations) {
                if (s.id == st) return true;
            }
            return false;
        };
        const json::Value* boxes = OptionalArray(*v, p, "boxes");
        if (boxes == nullptr) return message_.empty() ? Fail(Join(p, "boxes"), "required, a list") : false;
        for (std::size_t i = 0; i < boxes->items.size(); ++i) {
            const std::string bp = Index(Join(p, "boxes"), i);
            const json::Value& e = boxes->items[i];
            VoiceBoxDef b;
            if (!Keys(e, bp, {"id", "name", "station", "headset", "selector", "positions", "radio", "radio_select", "radio_select_sets",
                              "monitor", "monitor_positions", "lines", "circular", "power", "keeps_intercom", "notes"}) ||
                !RequireString(e, bp, "id", b.id) || !RequireString(e, bp, "name", b.name) || !RequireString(e, bp, "station", b.station) ||
                !RefControl(e, bp, "headset", out.panel, ControlKind::Discrete, true, b.headset) ||
                !RefControl(e, bp, "selector", out.panel, ControlKind::Discrete, false, b.selector) ||
                !OptionalString(e, bp, "radio", b.radio) ||
                !RefControl(e, bp, "radio_select", out.panel, ControlKind::Discrete, false, b.radio_select) ||
                !RefControl(e, bp, "monitor", out.panel, ControlKind::Discrete, false, b.monitor) ||
                !RefControl(e, bp, "circular", out.panel, ControlKind::Momentary, false, b.circular) ||
                !RefSubsystem(e, bp, "power", eq, false, b.power) || !OptionalBool(e, bp, "keeps_intercom", b.keeps_intercom)) {
                return false;
            }
            if (!station_known(b.station)) return Fail(Join(bp, "station"), "unknown station '" + b.station + "'");
            const auto channel = [&](const std::string& c) -> std::string {
                if (c == "off" || c == "intercom" || c == "radio" || set_known(c)) return std::string();
                return "unknown channel '" + c + "' (off, intercom, radio or a set's id)";
            };
            const auto a_set = [&](const std::string& c) -> std::string {
                return c == "off" || set_known(c) ? std::string() : "unknown set '" + c + "'";
            };
            if (!StringList(e, bp, "positions", b.positions, channel) || !StringList(e, bp, "radio_select_sets", b.radio_select_sets, a_set) ||
                !StringList(e, bp, "monitor_positions", b.monitor_positions, a_set)) {
                return false;
            }
            const auto lever = [&](const std::string& c) -> std::string {
                const ControlDef* cd = out.panel.Find(c);
                return cd != nullptr && cd->kind == ControlKind::Discrete ? std::string() : "unknown line lever '" + c + "'";
            };
            if (!StringList(e, bp, "lines", b.lines, lever)) return false;
            if (!b.selector.empty() && static_cast<int>(b.positions.size()) != out.panel.Find(b.selector)->positions) {
                return Fail(Join(bp, "positions"), "one per position of " + b.selector);
            }
            if (b.selector.empty() && !b.positions.empty()) return Fail(Join(bp, "positions"), "a box without a selector has none");
            if (!b.monitor.empty() && static_cast<int>(b.monitor_positions.size()) != out.panel.Find(b.monitor)->positions) {
                return Fail(Join(bp, "monitor_positions"), "one per position of " + b.monitor);
            }
            if (!b.radio.empty() && !set_known(b.radio)) return Fail(Join(bp, "radio"), "unknown set '" + b.radio + "'");
            if (!b.radio_select.empty() && static_cast<int>(b.radio_select_sets.size()) != out.panel.Find(b.radio_select)->positions) {
                return Fail(Join(bp, "radio_select_sets"), "one per position of " + b.radio_select);
            }
            bool names_radio = false;
            for (const std::string& c : b.positions) names_radio = names_radio || c == "radio";
            if (names_radio && b.radio.empty() && b.radio_select.empty()) return Fail(bp, "\"radio\" needs radio or radio_select");
            for (const VoiceBoxDef& o : d.boxes) {
                if (o.id == b.id) return Fail(Join(bp, "id"), "duplicate box '" + b.id + "'");
                if (o.station == b.station) return Fail(Join(bp, "station"), "one box a seat");
            }
            d.boxes.push_back(std::move(b));
        }
        if (!StringList(*v, p, "unboxed", d.unboxed, [&](const std::string& s) {
                return station_known(s) ? std::string() : "unknown station '" + s + "'";
            })) {
            return false;
        }
        const json::Value* phones = OptionalArray(*v, p, "phones");
        if (phones == nullptr && !message_.empty()) return false;
        for (std::size_t i = 0; phones != nullptr && i < phones->items.size(); ++i) {
            const std::string pp = Index(Join(p, "phones"), i);
            const json::Value& e = phones->items[i];
            VoicePhoneDef ph;
            if (!Keys(e, pp, {"id", "handset", "station", "notes"}) || !RequireString(e, pp, "id", ph.id) ||
                !RefControl(e, pp, "handset", out.panel, ControlKind::Discrete, true, ph.handset) ||
                !RequireString(e, pp, "station", ph.station)) {
                return false;
            }
            if (!station_known(ph.station)) return Fail(Join(pp, "station"), "unknown station '" + ph.station + "'");
            d.phones.push_back(std::move(ph));
        }
        out.has_voice = true;
        return true;
    }

    bool LoadLauncher(const json::Value& root, ConsoleData& out) {
        const json::Value* l = root.Find("launcher");
        if (l == nullptr) return true;
        const std::string lp = "launcher";
        LauncherDef& d = out.launcher;
        const Equipment& eq = out.equipment;
        if (!Keys(*l, lp, {"containers", "channels", "channel_control", "channel_positions", "attention_input",
                           "jam_control", "pair_preparation", "capture_s", "medium_s", "narrow_s", "fuse_range_m",
                           "nominal_flight_s", "min_interval_s", "hold_input", "reload_input", "stopped_input",
                           "reload_s_per_missile", "letter", "emergency", "range_sweep", "prep_hold", "source", "confidence", "notes"}) ||
            !Provenance(*l, lp) ||
            !RefControl(*l, lp, "channel_control", out.panel, ControlKind::Discrete, true, d.channel_control) ||
            !RefControl(*l, lp, "jam_control", out.panel, ControlKind::Discrete, false, d.jam_control) ||
            !RefInput(*l, lp, "attention_input", eq, d.attention_input) ||
            !RefInput(*l, lp, "hold_input", eq, d.hold_input) || !RefInput(*l, lp, "reload_input", eq, d.reload_input) ||
            !RefInput(*l, lp, "stopped_input", eq, d.stopped_input) ||
            !OptionalNumber(*l, lp, "reload_s_per_missile", d.reload_s_per_missile) ||
            !RefSubsystem(*l, lp, "pair_preparation", eq, true, d.pair_preparation)) {
            return false;
        }
        if (d.attention_input.empty()) return Fail(Join(lp, "attention_input"), "required");
        for (const auto& t : {std::make_pair("capture_s", &d.capture_s), std::make_pair("medium_s", &d.medium_s),
                              std::make_pair("narrow_s", &d.narrow_s), std::make_pair("fuse_range_m", &d.fuse_range_m),
                              std::make_pair("nominal_flight_s", &d.nominal_flight_s),
                              std::make_pair("min_interval_s", &d.min_interval_s)}) {
            if (l->Find(t.first) == nullptr) return Fail(Join(lp, t.first), "required");
            if (!OptionalNumber(*l, lp, t.first, *t.second)) return false;
            if (*t.second < 0.0) return Fail(Join(lp, t.first), "must be >= 0");
        }
        if (d.reload_s_per_missile < 0.0) return Fail(Join(lp, "reload_s_per_missile"), "must be >= 0");
        if (d.reload_s_per_missile > 0.0 && (d.reload_input.empty() || d.stopped_input.empty())) {
            return Fail(Join(lp, "reload_s_per_missile"), "needs reload_input and stopped_input");
        }
        if (!(d.capture_s <= d.medium_s && d.medium_s <= d.narrow_s)) {
            return Fail(lp, "capture_s <= medium_s <= narrow_s: the beams follow one another");
        }
        const json::Value* chans = OptionalArray(*l, lp, "channels");
        if (chans == nullptr || chans->items.size() != 2) {
            return message_.empty() ? Fail(Join(lp, "channels"), "expected two channels (I, II)") : false;
        }
        for (std::size_t i = 0; i < chans->items.size(); ++i) {
            const std::string cp = Index(Join(lp, "channels"), i);
            const json::Value& c = chans->items[i];
            LauncherChannelDef ch;
            if (!Keys(c, cp, {"transmitter", "away", "captured", "medium", "narrow", "fuse"}) ||
                !RefInput(c, cp, "transmitter", eq, ch.transmitter_input) || !RefInput(c, cp, "away", eq, ch.away_input) ||
                !RefInput(c, cp, "captured", eq, ch.captured_input) || !RefInput(c, cp, "medium", eq, ch.medium_input) ||
                !RefInput(c, cp, "narrow", eq, ch.narrow_input) || !RefInput(c, cp, "fuse", eq, ch.fuse_input)) {
                return false;
            }
            d.channels.push_back(ch);
        }
        const json::Value* cons = OptionalArray(*l, lp, "containers");
        if (cons == nullptr || cons->items.empty()) {
            return message_.empty() ? Fail(Join(lp, "containers"), "expected a list of containers") : false;
        }
        for (std::size_t i = 0; i < cons->items.size(); ++i) {
            const std::string cp = Index(Join(lp, "containers"), i);
            const json::Value& c = cons->items[i];
            LauncherContainerDef con;
            if (!Keys(c, cp, {"id", "channel", "order", "input", "hot_input"}) || !RequireString(c, cp, "id", con.id) ||
                !OptionalInt(c, cp, "channel", con.channel) || !OptionalInt(c, cp, "order", con.order) ||
                !RefInput(c, cp, "input", eq, con.loaded_input) || !RefInput(c, cp, "hot_input", eq, con.hot_input)) {
                return false;
            }
            if (con.channel != 1 && con.channel != 2) return Fail(Join(cp, "channel"), "must be 1 (I) or 2 (II)");
            for (const LauncherContainerDef& other : d.containers) {
                if (other.id == con.id) return Fail(Join(cp, "id"), "duplicate container '" + con.id + "'");
            }
            d.containers.push_back(con);
        }
        const json::Value* pos = OptionalArray(*l, lp, "channel_positions");
        if (pos == nullptr) return message_.empty() ? Fail(Join(lp, "channel_positions"), "required") : false;
        for (std::size_t i = 0; i < pos->items.size(); ++i) {
            const std::string pp = Index(Join(lp, "channel_positions"), i);
            const json::Value& list = pos->items[i];
            if (!list.Is(json::Type::Array) || list.items.empty()) return Fail(pp, "expected a non-empty list of channels");
            std::vector<int> chs;
            for (const json::Value& c : list.items) {
                if (!c.Is(json::Type::Number) || (c.number != 1.0 && c.number != 2.0)) return Fail(pp, "channels are 1 or 2");
                chs.push_back(static_cast<int>(c.number));
            }
            d.channel_positions.push_back(std::move(chs));
        }
        if (static_cast<int>(d.channel_positions.size()) != out.panel.Find(d.channel_control)->positions) {
            return Fail(Join(lp, "channel_positions"), "needs one entry per position of " + d.channel_control);
        }
        if (!LoadLauncherLetter(*l, lp, out, d) || !LoadLauncherEmergency(*l, lp, out, d) ||
            !LoadLauncherRangeSweep(*l, lp, out, d) || !LoadLauncherPrepHold(*l, lp, d)) {
            return false;
        }
        out.has_launcher = true;
        return true;
    }

    bool LoadLauncherLetter(const json::Value& l, const std::string& lp, const ConsoleData& out, LauncherDef& d) {
        const json::Value* v = l.Find("letter");
        if (v == nullptr) return true;
        const std::string p = Join(lp, "letter");
        if (!v->Is(json::Type::Object)) return Fail(p, "expected an object");
        if (!Keys(*v, p, {"control", "change_position", "lock_inputs", "relock_s", "source", "confidence", "notes"}) ||
            !Provenance(*v, p) || !RefControl(*v, p, "control", out.panel, ControlKind::Discrete, true, d.letter.control) ||
            !OptionalInt(*v, p, "change_position", d.letter.change_position) ||
            !OptionalNumber(*v, p, "relock_s", d.letter.relock_s)) {
            return false;
        }
        if (d.letter.change_position < 0 || d.letter.change_position >= out.panel.Find(d.letter.control)->positions) {
            return Fail(Join(p, "change_position"), "not a position of " + d.letter.control);
        }
        if (d.letter.relock_s < 0.0) return Fail(Join(p, "relock_s"), "must be >= 0");
        const json::Value* locks = OptionalArray(*v, p, "lock_inputs");
        if (locks == nullptr || locks->items.size() != 2) {
            return message_.empty() ? Fail(Join(p, "lock_inputs"), "expected two inputs (channel I, II)") : false;
        }
        for (std::size_t i = 0; i < 2; ++i) {
            const json::Value& e = locks->items[i];
            if (!e.Is(json::Type::String) || !out.equipment.HasInput(e.string)) {
                return Fail(Index(Join(p, "lock_inputs"), i), "unknown input");
            }
            d.letter.lock_inputs.push_back(e.string);
        }
        d.has_letter = true;
        return true;
    }

    bool LoadLauncherPrepHold(const json::Value& l, const std::string& lp, LauncherDef& d) {
        const json::Value* v = l.Find("prep_hold");
        if (v == nullptr) return true;
        const std::string p = Join(lp, "prep_hold");
        if (!v->Is(json::Type::Object)) return Fail(p, "expected an object");
        LauncherPrepHoldDef& h = d.prep_hold;
        if (!Keys(*v, p, {"hold_s", "rest_s", "cycles", "long_rest_s", "source", "confidence", "notes"}) || !Provenance(*v, p) ||
            !OptionalNumber(*v, p, "hold_s", h.hold_s) || !OptionalNumber(*v, p, "rest_s", h.rest_s) ||
            !OptionalInt(*v, p, "cycles", h.cycles) || !OptionalNumber(*v, p, "long_rest_s", h.long_rest_s)) {
            return false;
        }
        if (h.hold_s <= 0.0) return Fail(Join(p, "hold_s"), "must be > 0");
        if (h.rest_s < 0.0 || h.long_rest_s < h.rest_s) return Fail(Join(p, "long_rest_s"), "rests must be >= 0 and the long one the longer");
        if (h.cycles < 1) return Fail(Join(p, "cycles"), "must be >= 1");
        d.has_prep_hold = true;
        return true;
    }

    bool LoadLauncherRangeSweep(const json::Value& l, const std::string& lp, const ConsoleData& out, LauncherDef& d) {
        const json::Value* v = l.Find("range_sweep");
        if (v == nullptr) return true;
        const std::string p = Join(lp, "range_sweep");
        if (!v->Is(json::Type::Object)) return Fail(p, "expected an object");
        if (!Keys(*v, p, {"control", "channels", "source", "confidence", "notes"}) || !Provenance(*v, p) ||
            !RefControl(*v, p, "control", out.panel, ControlKind::Discrete, true, d.range_sweep_control)) {
            return false;
        }
        const json::Value* list = OptionalArray(*v, p, "channels");
        if (list == nullptr) return message_.empty() ? Fail(Join(p, "channels"), "required") : false;
        if (static_cast<int>(list->items.size()) != out.panel.Find(d.range_sweep_control)->positions) {
            return Fail(Join(p, "channels"), "needs one entry per position of " + d.range_sweep_control);
        }
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const json::Value& c = list->items[i];
            if (!c.Is(json::Type::Number) || c.number < 0.0 || c.number > 2.0) {
                return Fail(Index(Join(p, "channels"), i), "0 (the target), 1 or 2");
            }
            d.range_sweep_channels.push_back(static_cast<int>(c.number));
        }
        return true;
    }

    bool LoadLauncherEmergency(const json::Value& l, const std::string& lp, const ConsoleData& out, LauncherDef& d) {
        const json::Value* v = l.Find("emergency");
        if (v == nullptr) return true;
        const std::string p = Join(lp, "emergency");
        if (!v->Is(json::Type::Object)) return Fail(p, "expected an object");
        LauncherEmergencyDef& e = d.emergency;
        if (!Keys(*v, p, {"select_control", "select_containers", "fire_control", "ready_input", "flight_s", "source",
                          "confidence", "notes"}) ||
            !Provenance(*v, p) ||
            !RefControl(*v, p, "select_control", out.panel, ControlKind::Discrete, true, e.select_control) ||
            !RefControl(*v, p, "fire_control", out.panel, ControlKind::Momentary, true, e.fire_control) ||
            !RefInput(*v, p, "ready_input", out.equipment, e.ready_input) || !OptionalNumber(*v, p, "flight_s", e.flight_s)) {
            return false;
        }
        if (e.flight_s <= 0.0) return Fail(Join(p, "flight_s"), "must be > 0");
        const json::Value* list = OptionalArray(*v, p, "select_containers");
        if (list == nullptr) return message_.empty() ? Fail(Join(p, "select_containers"), "required") : false;
        if (static_cast<int>(list->items.size()) != out.panel.Find(e.select_control)->positions) {
            return Fail(Join(p, "select_containers"), "needs one entry per position of " + e.select_control);
        }
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const json::Value& c = list->items[i];
            if (!c.Is(json::Type::String)) return Fail(Index(Join(p, "select_containers"), i), "expected a container id or \"\"");
            bool known = c.string.empty();
            for (const LauncherContainerDef& con : d.containers) known = known || con.id == c.string;
            if (!known) return Fail(Index(Join(p, "select_containers"), i), "unknown container '" + c.string + "'");
            e.select_containers.push_back(c.string);
        }
        d.has_emergency = true;
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
        if (!Keys(*s, "sensors", {"search", "track", "orientation", "source", "confidence", "notes"}) ||
            !Provenance(*s, "sensors")) {
            return false;
        }
        const ControlPanel& panel = out.panel;
        const Equipment& eq = out.equipment;
        const json::Value* se = s->Find("search");
        const json::Value* tr = s->Find("track");
        if (se == nullptr || tr == nullptr) return Fail("sensors", "needs search and track");
        const std::string sp = "sensors.search";
        SearchSensorDef& search = out.search;
        if (!Keys(*se, sp, {"antenna", "transmitter", "range_control", "range_scales_m", "range_rings_m", "mti_control",
                            "mti_max_range_m", "beams", "iff", "notes"}) ||
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
        if (const json::Value* rings = OptionalArray(*se, sp, "range_rings_m")) {
            for (std::size_t i = 0; i < rings->items.size(); ++i) {
                const json::Value& list = rings->items[i];
                const std::string rp = Index(Join(sp, "range_rings_m"), i);
                if (!list.Is(json::Type::Array)) return Fail(rp, "expected a list of distances in metres");
                std::vector<double> distances;
                for (const json::Value& d : list.items) {
                    if (!d.Is(json::Type::Number) || !(d.number > 0.0)) return Fail(rp, "distances must be numbers > 0");
                    distances.push_back(d.number);
                }
                search.range_rings_m.push_back(std::move(distances));
            }
            if (search.range_rings_m.size() != search.range_scales_m.size()) {
                return Fail(Join(sp, "range_rings_m"), "needs one list per range scale");
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (const json::Value* beams = se->Find("beams")) {
            const std::string bp = Join(sp, "beams");
            if (!Keys(*beams, bp, {"control", "programs", "inputs", "rotation_period_s", "source", "confidence", "notes"}) ||
                !Provenance(*beams, bp) ||
                !RefControl(*beams, bp, "control", panel, ControlKind::Discrete, true, search.beam_control) ||
                !OptionalNumber(*beams, bp, "rotation_period_s", search.rotation_period_s)) {
                return false;
            }
            if (!(search.rotation_period_s > 0.0)) return Fail(Join(bp, "rotation_period_s"), "required, > 0");
            const json::Value* programs = OptionalArray(*beams, bp, "programs");
            if (programs == nullptr) return message_.empty() ? Fail(Join(bp, "programs"), "required") : false;
            for (std::size_t i = 0; i < programs->items.size(); ++i) {
                const json::Value& list = programs->items[i];
                const std::string pp = Index(Join(bp, "programs"), i);
                if (!list.Is(json::Type::Array) || list.items.empty()) return Fail(pp, "expected a non-empty list of beam indices");
                std::vector<int> beams_used;
                for (const json::Value& b : list.items) {
                    if (!b.Is(json::Type::Number) || b.number < 0.0 || b.number != std::floor(b.number)) {
                        return Fail(pp, "beam indices must be whole numbers >= 0");
                    }
                    beams_used.push_back(static_cast<int>(b.number));
                }
                search.beam_programs.push_back(std::move(beams_used));
            }
            if (static_cast<int>(search.beam_programs.size()) != panel.Find(search.beam_control)->positions) {
                return Fail(Join(bp, "programs"), "needs one programme per position of " + search.beam_control);
            }
            if (const json::Value* ins = OptionalArray(*beams, bp, "inputs")) {
                for (std::size_t i = 0; i < ins->items.size(); ++i) {
                    const std::string ip = Index(Join(bp, "inputs"), i);
                    if (!ins->items[i].Is(json::Type::String) || !eq.HasInput(ins->items[i].string)) {
                        return Fail(ip, "expected the id of an input");
                    }
                    search.beam_inputs.push_back(ins->items[i].string);
                }
            } else if (!message_.empty()) {
                return false;
            }
            for (const std::vector<int>& prog : search.beam_programs) {
                for (int b : prog) {
                    if (!search.beam_inputs.empty() && b >= static_cast<int>(search.beam_inputs.size())) {
                        return Fail(Join(bp, "programs"), "names a beam that has no input");
                    }
                }
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (const json::Value* iff = se->Find("iff")) {
            const std::string ip = Join(sp, "iff");
            if (!Keys(*iff, ip, {"query", "control_query", "gate_control", "ready", "interrogating_input",
                                 "query_time_s", "max_range_m", "source", "confidence", "notes"}) ||
                !Provenance(*iff, ip) ||
                !RefControl(*iff, ip, "query", panel, ControlKind::Momentary, true, search.iff_query) ||
                !RefControl(*iff, ip, "control_query", panel, ControlKind::Momentary, false, search.iff_control_query) ||
                !RefControl(*iff, ip, "gate_control", panel, ControlKind::Discrete, false, search.iff_gate_control) ||
                !RefSubsystem(*iff, ip, "ready", eq, true, search.iff_ready) ||
                !RefInput(*iff, ip, "interrogating_input", eq, search.iff_interrogating_input) ||
                !OptionalNumber(*iff, ip, "query_time_s", search.iff_query_time_s) ||
                !OptionalNumber(*iff, ip, "max_range_m", search.iff_max_range_m)) {
                return false;
            }
            if (!(search.iff_query_time_s > 0.0)) return Fail(Join(ip, "query_time_s"), "must be > 0");
            if (!(search.iff_max_range_m > 0.0)) return Fail(Join(ip, "max_range_m"), "required, > 0");
        }

        const std::string tp = "sensors.track";
        TrackSensorDef& track = out.track;
        if (!Keys(*tr, tp, {"transmitter", "slew", "radar_mode", "tv_camera", "tv_mode", "range_only_mode", "az_wheel",
                            "el_wheel", "range_wheel", "lock_control", "fov_control", "mti_control", "rgpo_control", "fov_deg", "fov_height_ratio",
                            "max_range_m", "az_limit_deg", "captured_input", "range_pulled_input", "tv_track_input", "too_low_input", "near_zone_input",
                            "in_zone_input", "crosshair_input", "elevation_search", "notes"}) ||
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
            !RefControl(*tr, tp, "mti_control", panel, ControlKind::Discrete, false, track.mti_control) ||
            !RefControl(*tr, tp, "rgpo_control", panel, ControlKind::Discrete, false, track.rgpo_control) ||
            !RefInput(*tr, tp, "range_pulled_input", eq, track.range_pulled_input) ||
            !OptionalNumber(*tr, tp, "max_range_m", track.max_range_m) ||
            !OptionalNumber(*tr, tp, "az_limit_deg", track.az_limit_deg) ||
            !RefInput(*tr, tp, "captured_input", eq, track.captured_input) ||
            !RefInput(*tr, tp, "tv_track_input", eq, track.tv_track_input) ||
            !RefInput(*tr, tp, "too_low_input", eq, track.too_low_input) ||
            !RefInput(*tr, tp, "near_zone_input", eq, track.near_zone_input) ||
            !RefInput(*tr, tp, "in_zone_input", eq, track.in_zone_input) ||
            !RefInput(*tr, tp, "crosshair_input", eq, track.crosshair_input)) {
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
        if (!OptionalNumber(*tr, tp, "fov_height_ratio", track.fov_height_ratio)) return false;
        if (!(track.fov_height_ratio > 0.0 && track.fov_height_ratio <= 1.0)) {
            return Fail(Join(tp, "fov_height_ratio"), "must be > 0 and <= 1");
        }
        if (!track.tv_mode.empty() && (track.tv_camera.empty() || track.fov_wide_deg <= 0.0)) {
            return Fail(tp, "tv_mode needs tv_camera and fov_deg");
        }
        if (const json::Value* es = tr->Find("elevation_search")) {
            if (!LoadElevationSearch(*es, Join(tp, "elevation_search"), panel, eq, track)) return false;
        }
        if (const json::Value* o = s->Find("orientation")) {
            if (!LoadOrientation(*o, "sensors.orientation", panel, eq, out.orientation)) return false;
            out.has_orientation = true;
        }
        out.has_sensors = true;
        return true;
    }

    bool LoadOrientation(const json::Value& o, const std::string& op, const ControlPanel& panel, const Equipment& eq,
                         OrientationDef& def) {
        if (!Keys(o, op, {"gyro", "setter", "mode_control", "set_position", "relay_control", "relay_position",
                          "sync_button", "slew_rate_deg_s", "sync_rate_deg_s", "synced_input", "latitude_control",
                          "earth_rate_deg_h", "own_drift_deg_h", "survey_s", "survey_sigma_deg", "qc_known_input",
                          "oriented_input", "source", "confidence", "notes"}) ||
            !Provenance(o, op) || !RefSubsystem(o, op, "gyro", eq, true, def.gyro) ||
            !RefControl(o, op, "setter", panel, ControlKind::Continuous, true, def.setter) ||
            !RefControl(o, op, "mode_control", panel, ControlKind::Discrete, true, def.mode_control) ||
            !RefControl(o, op, "relay_control", panel, ControlKind::Discrete, true, def.relay_control) ||
            !RefControl(o, op, "sync_button", panel, ControlKind::Momentary, true, def.sync_button) ||
            !OptionalInt(o, op, "set_position", def.set_position) ||
            !OptionalInt(o, op, "relay_position", def.relay_position) ||
            !OptionalNumber(o, op, "slew_rate_deg_s", def.slew_rate_deg_s) ||
            !OptionalNumber(o, op, "sync_rate_deg_s", def.sync_rate_deg_s) ||
            !RefInput(o, op, "synced_input", eq, def.synced_input) ||
            !RefControl(o, op, "latitude_control", panel, ControlKind::Continuous, false, def.latitude_control) ||
            !OptionalNumber(o, op, "earth_rate_deg_h", def.earth_rate_deg_h) ||
            !OptionalNumber(o, op, "own_drift_deg_h", def.own_drift_deg_h) ||
            !OptionalNumber(o, op, "survey_s", def.survey_s) ||
            !OptionalNumber(o, op, "survey_sigma_deg", def.survey_sigma_deg) ||
            !RefInput(o, op, "qc_known_input", eq, def.qc_known_input) ||
            !RefInput(o, op, "oriented_input", eq, def.oriented_input)) {
            return false;
        }
        if (def.earth_rate_deg_h < 0.0 || def.own_drift_deg_h < 0.0 || def.survey_s < 0.0 || def.survey_sigma_deg < 0.0) {
            return Fail(op, "earth_rate_deg_h, own_drift_deg_h, survey_s and survey_sigma_deg must be >= 0");
        }
        if (def.set_position < 0 || def.set_position >= panel.Find(def.mode_control)->positions) {
            return Fail(Join(op, "set_position"), "not a position of " + def.mode_control);
        }
        if (def.relay_position < 0 || def.relay_position >= panel.Find(def.relay_control)->positions) {
            return Fail(Join(op, "relay_position"), "not a position of " + def.relay_control);
        }
        if (!(def.slew_rate_deg_s > 0.0) || !(def.sync_rate_deg_s > 0.0)) {
            return Fail(op, "slew_rate_deg_s and sync_rate_deg_s must be > 0");
        }
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
            if (!Keys(p, path, {"id", "name", "steps", "situational", "source", "confidence", "notes"})) return false;
            Procedure proc;
            if (!RequireString(p, path, "id", proc.id) || !RequireString(p, path, "name", proc.name) ||
                !OptionalBool(p, path, "situational", proc.situational) || !Provenance(p, path)) {
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
