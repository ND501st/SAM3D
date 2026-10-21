// simcore/em/model.hpp — the data of the electromagnetic layer: definitions and runtime state.
//
// Organised like an entity-component system: plain arrays of components (platforms, emitters,
// radars, ESM receivers, jammers, chaff, comms nodes and nets) in one EmScene, and "systems"
// (radar_system.hpp, esm_system.hpp, comms_system.hpp, ...) that run over them at their own
// rates. Definitions (RadarDef, EsmDef, ...) are data loaded from data/em/*.json; the runtime
// structs refer to them by index. Nothing here is specific to one weapon system.
//
// Truth lives only in Platform. Sensors read it inside their system functions and emit
// observations; everything downstream (trackers, knowledge, AI, the player's displays) works from
// observations. Fields named debug_truth_* exist for overlays and tests and must not be read by
// trackers, fusion, doctrine or UI logic.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "simcore/em/common.hpp"
#include "simcore/em/emitter.hpp"
#include "simcore/em/knowledge.hpp"
#include "simcore/em/propagation.hpp"
#include "simcore/em/random.hpp"
#include "simcore/em/tracking.hpp"
#include "simcore/geo.hpp"
#include "simcore/terrain.hpp"

namespace simcore {
namespace em {

enum class Side { Blue, Red, Neutral };
enum class PlatformKind { FixedWing, RotaryWing, Missile, Ground, Decoy };

inline const char* SideName(Side s) {
    switch (s) {
        case Side::Blue: return "blue";
        case Side::Red: return "red";
        case Side::Neutral: return "neutral";
    }
    return "?";
}

// ----------------------------------------------------------------------------------- truth

// Radar cross-section by aspect: nose-on, beam-on and tail-on values, interpolated in dB.
struct RcsModel {
    double nose_m2 = 5.0;
    double beam_m2 = 10.0;
    double tail_m2 = 5.0;
};

// RCS seen from a direction (`to_observer`, unit vector from the platform towards the radar).
inline double RcsToward(const RcsModel& m, const Vec3& velocity, const Vec3& to_observer) {
    const double hv = velocity.HorizontalLength();
    const double ho = to_observer.HorizontalLength();
    if (hv < 1e-6 || ho < 1e-6) return std::sqrt(m.nose_m2 * m.tail_m2);  // hovering or overhead
    double c = (velocity.x * to_observer.x + velocity.y * to_observer.y) / (hv * ho);
    c = Clamp(c, -1.0, 1.0);
    const double aspect = std::acos(c);  // 0 = nose-on
    const double half = 0.5 * kPi;
    const double lo = aspect <= half ? m.nose_m2 : m.beam_m2;
    const double hi = aspect <= half ? m.beam_m2 : m.tail_m2;
    const double w = aspect <= half ? aspect / half : (aspect - half) / half;
    return std::exp((1.0 - w) * std::log(lo) + w * std::log(hi));
}

struct Platform {
    int id = 0;
    std::string name;
    Side side = Side::Neutral;
    PlatformKind kind = PlatformKind::FixedWing;
    Vec3 position;  // ENU m (ground platforms: the antenna's height above the ground included)
    Vec3 velocity;
    RcsModel rcs;
    bool alive = true;
    bool iff_reply = false;  // answers its own side's interrogations (codes and modes not modelled yet)
    // Derived each tick by the scene:
    double altitude_m = 0.0;    // above the ellipsoid
    double height_agl_m = 0.0;  // above the terrain under it
};

// ------------------------------------------------------------------------------ definitions

enum class MtiMode { Off, Internal, External };

inline const char* MtiModeName(MtiMode m) {
    switch (m) {
        case MtiMode::Off: return "off";
        case MtiMode::Internal: return "internal coherence";
        case MtiMode::External: return "external coherence";
    }
    return "?";
}

// A published performance point: "detects a target of rcs at range when it flies at altitude".
struct RadarAnchor {
    bool valid = false;
    double range_m = 0.0;
    double rcs_m2 = 0.0;
    double target_altitude_m = 0.0;
    double pd = 0.5;
    std::string source;
};

enum class RadarRole { Search, Track };

struct RadarDef {
    std::string id;
    std::string name;
    RadarRole role = RadarRole::Search;
    EmitterDef emitter;  // its transmission (what ESM receivers see)
    double noise_figure_db = 5.0;
    double rx_bandwidth_hz = 0.0;  // 0: 1 / pulse width
    double pfa = 1e-6;
    RadarAnchor calibration;         // sets the loop gain (Calibrate)
    RadarAnchor low_altitude_check;  // reported by CheckCalibration, not fitted
    double antenna_height_m = 4.0;   // above the ground when deployed (the platform position includes it)
    double min_range_m = 0.0;
    double max_range_m = 0.0;
    double min_elevation_rad = -0.05;
    double max_elevation_rad = 1.4;
    double range_sigma_floor_m = 20.0;
    double angle_sigma_floor_rad = 0.001;
    double monopulse_slope = 1.6;  // k_m; the search radar's elevation accuracy is the beam height / sqrt(12)
    double range_resolution_m = 150.0;
    // Moving-target indication and clutter.
    bool has_mti = false;
    double mti_improvement_db = 25.0;
    double mti_notch_mps = 15.0;            // radial speeds below this are cancelled with the clutter
    double mti_notch_attenuation_db = 30.0;
    double clutter_cnr_db_at_10km = 40.0;   // ground clutter-to-noise in the target's cell at 10 km
    double clutter_max_elevation_rad = 0.026;  // targets below ~1.5 deg share cells with the ground
    // Tracking.
    TrackerParams tracker;
    double track_gate_m = 750.0;          // track radar: range gate half-width
    // Low-target mode: below this elevation the antenna is held here instead of following the
    // target down into its ground reflection (0 = no such mode). The target then sits low in the beam.
    double low_target_elevation_rad = 0.0;
    double max_slew_rad_s = 0.26;         // track radar antenna slew limit
    // Counter-countermeasures.
    bool has_iff = false;                 // integrated interrogator: own-side transponders reply on the scope
    double iff_reply_probability = 0.9;   // per look, for a working transponder in the beam
    bool angle_track_on_jam = false;      // can follow a noise jammer in angle (home-on-jam)
    double rgpo_susceptibility = 1.0;     // chance per look that a strong repeater captures the range gate
    double strobe_jn_threshold_db = 6.0;  // J/N at which a jamming strobe shows on the scope
    std::string source;
    std::string confidence;
    // Derived by Calibrate(): SNR = loop_gain * rcs / R^4 * (pattern and propagation factors).
    double loop_gain = 0.0;

    double RxBandwidthHz() const {
        if (rx_bandwidth_hz > 0.0) return rx_bandwidth_hz;
        return emitter.waveform.pulse_width_s > 0.0 ? 1.0 / emitter.waveform.pulse_width_s : 1e6;
    }
};

enum class EsmKind { Rwr, Esm, Elint, TdoaSite, CommsIntercept };

inline const char* EsmKindName(EsmKind k) {
    switch (k) {
        case EsmKind::Rwr: return "rwr";
        case EsmKind::Esm: return "esm";
        case EsmKind::Elint: return "elint";
        case EsmKind::TdoaSite: return "tdoa_site";
        case EsmKind::CommsIntercept: return "comms_intercept";
    }
    return "?";
}

struct EsmDef {
    std::string id;
    std::string name;
    EsmKind kind = EsmKind::Esm;
    double freq_min_hz = 0.0;
    double freq_max_hz = 0.0;
    double sensitivity_dbw = -100.0;  // weakest signal it reports (peak power at the antenna terminals)
    double antenna_gain_db = 0.0;
    double bearing_sigma_rad = 0.05;
    double freq_sigma_fraction = 0.002;
    double pri_sigma_fraction = 0.01;
    double pw_sigma_fraction = 0.05;
    double time_sigma_s = 50e-9;       // TDOA sites: arrival-time error
    double line_interval_s = 2.0;      // how often a contact stores a line of bearing
    double contact_timeout_s = 12.0;
    std::string threat_library;
    std::string source;
    std::string confidence;
};

// A threat-library entry: what a receiver's library says an emitter looks like. Written from the
// *owning* side's intelligence, which need not match the truth (that difference is gameplay).
struct ThreatEntry {
    std::string id;
    std::string name;
    std::string kind;  // "search", "track", "fire_control", "comms", "jammer", ...
    Side owner = Side::Neutral;
    bool cw = false;              // continuous wave or noise: a pulsed signal can't be this
    double max_speed_mps = 0.0;   // how fast its platform can move (air ~300, ground ~20; 0 = unknown)
    double freq_min_hz = 0.0, freq_max_hz = 0.0;
    double prf_min_hz = 0.0, prf_max_hz = 0.0;  // 0/0: CW or not known
    double pw_min_s = 0.0, pw_max_s = 0.0;
    std::string source;
    std::string confidence;
};

struct ThreatLibrary {
    std::string id;
    std::vector<ThreatEntry> entries;
};

// FalseTargets: a repeater that answers the radar's pulses with copies delayed behind its own echo, so the radar sees
// a string of echoes on the jammer's bearing (an abstraction: count, spacing, and the jam-to-noise it needs).
enum class JamTechnique { Barrage, Spot, RangeGatePullOff, FalseTargets };

struct JammerDef {
    std::string id;
    std::string name;
    JamTechnique technique = JamTechnique::Barrage;
    EmitterDef emitter;  // waveform.freq_hz/bandwidth_hz = the jammed band; power and antenna
    double rgpo_walk_mps = 150.0;
    double rgpo_max_m = 2500.0;
    double rgpo_off_s = 3.0;         // jammer silent after a walk-off, dragging the gate onto nothing
    double rgpo_js_needed_db = 3.0;  // repeater must beat the skin echo by this to capture the gate
    double retune_s = 0.0;           // spot: time to follow its victim radar onto a new frequency (0: never follows)
    int false_targets = 0;           // false-target repeater: echoes it adds behind itself
    double false_spacing_m = 1000.0; // ... this far apart in range
    double false_jn_needed_db = 6.0; // ... once its repeated power is this far above the radar's noise
    std::string source;
    std::string confidence;
};

struct RadioDef {
    std::string id;
    std::string name;
    double freq_min_hz = 0.0;
    double freq_max_hz = 0.0;
    double power_w = 0.0;
    double antenna_gain_db = 0.0;
    double antenna_height_m = 3.0;
    double sensitivity_dbw = -130.0;  // input power for usable voice/data
    double required_snr_db = 10.0;    // used to place the noise floor for jamming
    double bandwidth_hz = 25e3;       // channel bandwidth (VHF FM: 25 kHz spacing)
    double system_loss_db = 0.0;      // cables, body shadowing, man-made noise (fitted to the range anchor)
    double cosite_desense_db = 0.0;   // extra noise when the platform's own radars radiate
    // Published range anchor: two stations at this antenna height over flat open ground just reach
    // each other at this range. With a cosite range, the desense is fitted too (radars on).
    double anchor_range_m = 0.0;
    double anchor_height_m = 0.0;
    double anchor_freq_hz = 0.0;      // 0: middle of the band
    double cosite_range_m = 0.0;
    bool voice = true;
    double message_s = 8.0;           // to pass one report (voice: read out by an operator)
    double operator_delay_s = 4.0;    // human reaction before a voice transmission starts
    int tracks_per_message = 1;
    std::string source;
    std::string confidence;
};

struct Environment {
    double surface_reflection = 0.9;    // smooth-ground reflection coefficient at grazing incidence
    double surface_roughness_m = 1.0;   // rms terrain height for the Rayleigh roughness factor
    double k_factor = kStandardRefractionK;
    double profile_spacing_m = 100.0;
    Vec3 wind_mps;                      // chaff drift
    double rain_rate_mmh = 0.0;         // rain over the whole scene (ITU-R P.838); never part of a radar's calibration
};

// Effective reflection coefficient: rho0 * Rayleigh roughness factor
// exp(-8 (pi sigma_h sin(psi) / lambda)^2) (Beckmann & Spizzichino; Skolnik, Radar Handbook ch. 2).
// Rough ground scatters microwaves but still reflects VHF.
inline double EffectiveReflection(const Environment& env, double grazing_rad, double wavelength_m) {
    if (wavelength_m <= 0.0) return 0.0;
    const double g = kPi * env.surface_roughness_m * std::sin(grazing_rad) / wavelength_m;
    return env.surface_reflection * std::exp(-8.0 * g * g);
}

// ------------------------------------------------------------------------------- runtime

struct LookRecord {  // one radar look at one candidate, kept for overlays and tests
    int platform_id = 0;
    double range_m = 0.0;
    double snr_db = -300.0;
    double sinr_db = -300.0;
    double jn_db = -300.0;
    double cn_db = -300.0;
    double pd = 0.0;
    bool detected = false;
    bool line_of_sight = false;
    double diffraction_db = 0.0;
};

struct JamStrobe {
    double time_s = 0.0;
    double azimuth_rad = 0.0;
    double sigma_rad = 0.0;
    double jn_db = 0.0;
};

// Angle-only track of a jammer's strobe (a radar sees direction, not range).
struct StrobeTrack {
    int id = 0;
    double azimuth_rad = 0.0;
    double sigma_rad = 0.0;
    double jn_db = 0.0;
    double last_s = 0.0;
    std::vector<BearingLine> lines;
};

struct RadarState {
    int id = 0;
    int def_index = -1;
    int platform_id = 0;
    int emitter_id = 0;
    bool on = false;
    bool iff_interrogating = true;
    MtiMode mti = MtiMode::Off;
    std::vector<int> beam_program;  // indices into the emitter's beams, one per revolution
    std::size_t program_pos = 0;
    long long last_revolution = 0;
    long long looks = 0;
    Tracker tracker;
    // Tracking radar (single target): designation and lock.
    bool has_designation = false;
    Vec3 designation;
    int locked_track = 0;
    double rgpo_offset_m = 0.0;  // how far a repeater has walked the range gate
    bool rgpo_captured = false;
    // Outputs of the last tick (overlays, tests, the scope).
    std::vector<LookRecord> last_looks;
    std::vector<RadarPlot> last_plots;
    std::vector<JamStrobe> strobes;  // recent (kept a few seconds)
    std::vector<StrobeTrack> strobe_tracks;
    int next_strobe_id = 1;
    mutable PathCache paths{25.0};  // this radar's own profiles (so radars can run on different threads with identical results)
};

struct EsmIntercept {
    double time_s = 0.0;
    double bearing_rad = 0.0;
    double bearing_sigma_rad = 0.0;
    double power_dbw = -300.0;
    double freq_hz = 0.0;
    double prf_hz = 0.0;
    double pulse_width_s = 0.0;
    bool main_beam = false;
    double arrival_time_s = 0.0;  // TDOA sites
    int debug_truth_emitter = 0;
};

struct EsmContact {
    int id = 0;
    EmitterSignature sig;
    double bearing_rad = 0.0;
    double bearing_sigma_rad = 0.0;
    double power_dbw = -300.0;
    double first_seen_s = 0.0;
    double last_seen_s = 0.0;
    int intercepts = 0;
    double last_main_beam_s = -1.0;
    double scan_period_est_s = 0.0;
    double steady_since_s = -1.0;
    double last_line_s = -1e9;
    std::vector<BearingLine> lines;  // stored every line_interval_s (a moving receiver builds a baseline)
    int debug_truth_emitter = 0;
};

struct EsmState {
    int id = 0;
    int def_index = -1;
    int platform_id = 0;
    bool on = true;
    std::vector<EsmIntercept> intercepts;  // this tick
    std::vector<EsmContact> contacts;
    int next_contact_id = 1;
    mutable PathCache paths{25.0};  // this site's own profiles: sites can then run on different threads with identical results
};

// Several TDOA sites working as one system (Ramona/Tamara style): a pulse seen by enough sites
// in the same tick is multilaterated at the group's node.
struct TdoaGroup {
    int id = 0;
    std::string name;
    std::vector<int> esm_ids;
    int node_id = 0;
    bool solve_3d = true;
    double assumed_altitude_m = 0.0;  // used when !solve_3d
    double max_residual_m = 500.0;    // reject solutions whose range differences don't fit
    std::vector<EmitterSignature> items;  // signature of each emitter it has fixed (item id = index + 1)
};

struct JammerState {
    int id = 0;
    int def_index = -1;
    int platform_id = 0;
    int emitter_id = 0;
    bool on = false;
    Vec3 aim_point;        // where the antenna points (a briefed target area, not a sensed position)
    bool aim_at_point = false;
    int victim_radar = 0;  // RGPO: the radar whose range gate it works on
    double started_s = 0.0;
    double silent_until_s = -1.0;
    double retune_since_s = -1.0;  // spot: when it noticed its victim left the band (-1: on frequency)
};

// A decoy emitter that copies a radar's transmission (Czechoslovak practice: decoys on the same parameters as the real
// radar, to draw strikes away; Vojenské rozhledy 1985/6). It radiates always, with the radar, or while the radar is off
// (blinking: the enemy hears the same signal from somewhere else when the real radar falls silent).
enum class DecoyMode { Always, WithRadar, WhenRadarOff };

struct DecoyLink {
    int emitter_id = 0;
    int radar_id = 0;
    DecoyMode mode = DecoyMode::WithRadar;
};

// Dummy radio traffic: a node that transmits empty voice messages on a net on a schedule, so radio intercept and DF
// hear a busy station where there is none to protect (or to keep the net's pattern the same when the real one is quiet).
struct DummyTraffic {
    int node_id = 0;
    int net_id = 0;
    double period_s = 30.0;
    double duration_s = 8.0;
    double next_s = 0.0;
    double until_s = 1e18;
};

struct ChaffCloud {
    int id = 0;
    Vec3 position;
    Vec3 velocity;
    double rcs_m2 = 0.0;
    double created_s = 0.0;
    double lifetime_s = 60.0;
    double bloom_s = 1.5;
};

inline double ChaffRcs(const ChaffCloud& c, double now) {
    const double age = now - c.created_s;
    if (age < 0.0 || age > c.lifetime_s) return 0.0;
    const double bloom = age < c.bloom_s ? age / c.bloom_s : 1.0;
    return c.rcs_m2 * bloom * std::exp(-age / (0.5 * c.lifetime_s));
}

// ------------------------------------------------------------------------------- comms

enum class C2Status { Connected, Degraded, Autonomous };

inline const char* C2StatusName(C2Status s) {
    switch (s) {
        case C2Status::Connected: return "connected";
        case C2Status::Degraded: return "degraded";
        case C2Status::Autonomous: return "autonomous";
    }
    return "?";
}

// Voice: one spoken message (a designation read out) with no track payload; its timing is set by the sender.
enum class MessageKind { TrackReport, Picture, Voice };

struct NetState {
    int id = 0;
    std::string name;
    int radio_def = -1;
    double freq_hz = 0.0;
    std::vector<int> members;
    double busy_until_s = 0.0;
};

struct NodeState {
    int id = 0;
    std::string name;
    int platform_id = 0;
    Side side = Side::Neutral;
    bool alive = true;
    bool c2_center = false;
    bool relay = false;
    int parent = 0;  // superior node (0 = none)
    std::vector<int> nets;
    std::vector<int> sensors;  // radar and ESM ids that feed this node directly (cable)
    KnowledgeBase kb;
    double report_period_s = 10.0;
    double next_report_s = 0.0;
    double picture_period_s = 20.0;
    double next_picture_s = 0.0;
    double last_parent_contact_s = -1e9;
    C2Status status = C2Status::Autonomous;
    int radio_emitter_id = 0;
    // Loss in this node's receiving chain that the link budget does not know (roadmap 7.8: the R-123M's squelch and a
    // shorter antenna), subtracted from every transmission it receives.
    double rx_extra_loss_db = 0.0;
    std::vector<long long> forwarded;  // relay: messages already passed on (recent ids)
};

struct Message {
    long long id = 0;
    MessageKind kind = MessageKind::TrackReport;
    int net = 0;
    int from_node = 0;
    int origin_node = 0;
    int to_node = 0;  // 0: everyone on the net
    double queued_s = 0.0;
    double start_s = 0.0;
    double end_s = 0.0;
    int hops = 0;
    std::vector<KnowledgeTrack> tracks;
    bool done = false;
};

// The fate of a voice message at its addressee, for whoever sent it.
struct VoiceResult {
    long long message_id = 0;
    int node = 0;
    bool delivered = false;
};

struct LinkState {
    int net = 0;
    int a = 0;  // transmitting node
    int b = 0;  // receiving node
    double rx_dbw = -300.0;
    double margin_db = -300.0;
    double jn_db = -300.0;
    double diffraction_db = 0.0;
    bool line_of_sight = false;
    bool up = false;
};

// ------------------------------------------------------------------------------- library

struct EmLibrary {
    std::vector<RadarDef> radars;
    std::vector<EsmDef> esm;
    std::vector<JammerDef> jammers;
    std::vector<RadioDef> radios;
    std::vector<EmitterDef> emitters;  // stand-alone emitters (navaids, decoy emitters)
    std::vector<ThreatLibrary> threat_libraries;

    template <typename T>
    static int IndexOf(const std::vector<T>& v, const std::string& id) {
        for (std::size_t i = 0; i < v.size(); ++i)
            if (v[i].id == id) return static_cast<int>(i);
        return -1;
    }
    int Radar(const std::string& id) const { return IndexOf(radars, id); }
    int Esm(const std::string& id) const { return IndexOf(esm, id); }
    int Jammer(const std::string& id) const { return IndexOf(jammers, id); }
    int Radio(const std::string& id) const { return IndexOf(radios, id); }
    int Emitter(const std::string& id) const { return IndexOf(emitters, id); }
    const ThreatLibrary* Library(const std::string& id) const {
        for (const ThreatLibrary& l : threat_libraries)
            if (l.id == id) return &l;
        return nullptr;
    }
};

// ------------------------------------------------------------------------------- events

enum class EmEventKind {
    RadarOn, RadarOff, Detection, TrackConfirmed, TrackDropped, LockOn, LockLost, EsmNewContact, EsmContactLost,
    EmitterFix, JammerOn, JammerOff, Strobe, MessageSent, MessageDelivered, MessageLost, C2StatusChange,
    NodeDestroyed, DoctrineDecision, Info
};

inline const char* EmEventKindName(EmEventKind k) {
    switch (k) {
        case EmEventKind::RadarOn: return "radar on";
        case EmEventKind::RadarOff: return "radar off";
        case EmEventKind::Detection: return "detection";
        case EmEventKind::TrackConfirmed: return "track confirmed";
        case EmEventKind::TrackDropped: return "track dropped";
        case EmEventKind::LockOn: return "lock on";
        case EmEventKind::LockLost: return "lock lost";
        case EmEventKind::EsmNewContact: return "ESM contact";
        case EmEventKind::EsmContactLost: return "ESM contact lost";
        case EmEventKind::EmitterFix: return "emitter fix";
        case EmEventKind::JammerOn: return "jammer on";
        case EmEventKind::JammerOff: return "jammer off";
        case EmEventKind::Strobe: return "strobe";
        case EmEventKind::MessageSent: return "message sent";
        case EmEventKind::MessageDelivered: return "message delivered";
        case EmEventKind::MessageLost: return "message lost";
        case EmEventKind::C2StatusChange: return "C2 status";
        case EmEventKind::NodeDestroyed: return "node destroyed";
        case EmEventKind::DoctrineDecision: return "decision";
        case EmEventKind::Info: return "info";
    }
    return "?";
}

struct EmEvent {
    double time_s = 0.0;
    EmEventKind kind = EmEventKind::Info;
    int subject = 0;  // radar, ESM, node or jammer id
    int object = 0;   // track, contact or platform id
    std::string text;
};

// Work counters for profiling (docs/EM_ARCHITECTURE.md, "Performance").
struct EmStats {
    std::uint64_t radar_looks = 0;
    std::uint64_t radar_candidates_culled = 0;
    std::uint64_t esm_evaluations = 0;
    std::uint64_t esm_culled = 0;
    std::uint64_t link_evaluations = 0;
    std::uint64_t plots = 0;
    std::uint64_t intercepts = 0;
    // Wall-clock nanoseconds per phase of EmWorld::Step (only counted when EmRates::profile is set).
    std::uint64_t ns_platforms = 0, ns_emitters = 0, ns_radars = 0, ns_esm = 0, ns_tdoa = 0, ns_knowledge = 0, ns_comms = 0;
};

// ------------------------------------------------------------------------------- spatial grid

// Uniform 2-D hash of platform positions. Air pictures are wide and flat, so a grid on east/north
// beats an octree here: rebuild is O(n) each tick and a range query touches (2r/cell)^2 cells.
class SpatialGrid {
public:
    explicit SpatialGrid(double cell_m = 10000.0) : cell_m_(cell_m) {}

    void Rebuild(const std::vector<Platform>& platforms) {
        cells_.clear();
        for (std::size_t i = 0; i < platforms.size(); ++i) {
            if (!platforms[i].alive) continue;
            cells_[KeyOf(CellOf(platforms[i].position.x), CellOf(platforms[i].position.y))].push_back(
                static_cast<int>(i));
        }
    }

    // Indices of platforms in cells overlapping the circle (callers still check the exact range).
    void Query(const Vec3& centre, double radius_m, std::vector<int>& out) const {
        out.clear();
        const long long x0 = CellOf(centre.x - radius_m), x1 = CellOf(centre.x + radius_m);
        const long long y0 = CellOf(centre.y - radius_m), y1 = CellOf(centre.y + radius_m);
        for (long long x = x0; x <= x1; ++x)
            for (long long y = y0; y <= y1; ++y) {
                const auto it = cells_.find(KeyOf(x, y));
                if (it == cells_.end()) continue;
                out.insert(out.end(), it->second.begin(), it->second.end());
            }
    }

private:
    long long CellOf(double v) const { return static_cast<long long>(std::floor(v / cell_m_)); }
    static long long KeyOf(long long x, long long y) { return (x << 32) ^ (y & 0xFFFFFFFFll); }
    double cell_m_;
    std::unordered_map<long long, std::vector<int>> cells_;
};

// ------------------------------------------------------------------------------- scene

struct EmScene {
    LocalFrame frame;
    const Terrain* terrain = nullptr;
    FlatTerrain flat{0.0};
    Environment env;
    Random rng{1};
    double time_s = 0.0;
    long long tick = 0;
    EmLibrary lib;

    std::vector<Platform> platforms;
    std::vector<EmitterState> emitters;
    std::vector<RadarState> radars;
    std::vector<EsmState> esm;
    std::vector<TdoaGroup> tdoa_groups;
    std::vector<JammerState> jammers;
    std::vector<ChaffCloud> chaff;
    std::vector<NodeState> nodes;
    std::vector<NetState> nets;
    std::vector<Message> messages;  // queued or on the air
    std::vector<VoiceResult> voice_results;
    std::vector<DecoyLink> decoys;
    std::vector<DummyTraffic> dummy_traffic;  // outcomes of voice messages (the sender collects them)
    std::vector<LinkState> links;
    std::vector<EmEvent> events;
    EmStats stats;

    mutable PathCache paths{25.0};  // shared profiles (links, comms); the per-site passes use their own
    unsigned threads = 1;  // worker threads for the per-site passes (ESM); results do not depend on it

    void ClearPaths() {
        paths.Clear();
        for (EsmState& e : esm) e.paths.Clear();
        for (RadarState& r : radars) r.paths.Clear();
    }
    SpatialGrid grid{10000.0};
    int next_id = 1;
    long long next_message_id = 1;

    const Terrain& GetTerrain() const { return terrain != nullptr ? *terrain : flat; }

    void Log(EmEventKind kind, int subject, int object, const std::string& text) {
        events.push_back({time_s, kind, subject, object, text});
    }

    // Entries are added with increasing ids and never reordered, so a binary search finds them; a miss falls back to a
    // scan in case something was inserted out of order.
    template <typename T>
    static T* FindIn(std::vector<T>& v, int id) {
        std::size_t lo = 0, hi = v.size();
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (v[mid].id < id) lo = mid + 1; else hi = mid;
        }
        if (lo < v.size() && v[lo].id == id) return &v[lo];
        for (T& x : v)
            if (x.id == id) return &x;
        return nullptr;
    }
    template <typename T>
    static const T* FindIn(const std::vector<T>& v, int id) {
        std::size_t lo = 0, hi = v.size();
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (v[mid].id < id) lo = mid + 1; else hi = mid;
        }
        if (lo < v.size() && v[lo].id == id) return &v[lo];
        for (const T& x : v)
            if (x.id == id) return &x;
        return nullptr;
    }
    Platform* FindPlatform(int id) { return FindIn(platforms, id); }
    const Platform* FindPlatform(int id) const { return FindIn(platforms, id); }
    EmitterState* FindEmitter(int id) { return FindIn(emitters, id); }
    const EmitterState* FindEmitter(int id) const { return FindIn(emitters, id); }
    RadarState* FindRadar(int id) { return FindIn(radars, id); }
    const RadarState* FindRadar(int id) const { return FindIn(radars, id); }
    EsmState* FindEsm(int id) { return FindIn(esm, id); }
    const EsmState* FindEsm(int id) const { return FindIn(esm, id); }
    JammerState* FindJammer(int id) { return FindIn(jammers, id); }
    NodeState* FindNode(int id) { return FindIn(nodes, id); }
    const NodeState* FindNode(int id) const { return FindIn(nodes, id); }
    NetState* FindNet(int id) { return FindIn(nets, id); }
    const NetState* FindNet(int id) const { return FindIn(nets, id); }

    // Elevation angle (rad) of `to` seen from `from`, over a 4/3 Earth (independent of where the
    // ENU origin is): atan((h_to - h_from) / d - d / (2 k R)).
    double ElevationAngle(const Platform& from, const Platform& to) const {
        const double d = GroundRange(from.position, to.position);
        if (d < 1.0) return to.altitude_m >= from.altitude_m ? 0.5 * kPi : -0.5 * kPi;
        return std::atan((to.altitude_m - from.altitude_m) / d - d / (2.0 * env.k_factor * wgs84::kMeanRadius));
    }
};

}  // namespace em
}  // namespace simcore
