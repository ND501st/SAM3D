// simcore/air_war/perception.hpp — what a pilot sees and hears (roadmap track B, B3; docs/air_battle/03_PERCEPTION.md).
//
// A pilot knows four things: what he sees, what his warning receiver tells him, what his own sensors show him and what
// he is told. This header is the first two. Nothing here reads the simulation's truth about where our vehicle is: an
// object reaches a pilot's picture only through a law of seeing (or a receiver's display), and the picture holds what
// he believes, with errors, never the truth. `Sighting::debug_truth_key` is for overlays and tests; no decision reads it.
//
//   * The eye (nwc_tp5709_1974): the threshold contrast at a range for an object of an area, with Koschmieder's haze and
//     a cloud's transmission, the field factors, the empty-field halving, and the glimpse as a draw. An object is seen
//     once the cumulative chance of the glimpses so far passes its own keyed draw.
//   * The ground law (nwc_tp6005_1978): P_max by pattern and contrastiness, and the range curve, one keyed draw per
//     pass. Speed does not change the chance.
//   * Visible events from the simulation's own sources (the runner records them beside the places that push fx events;
//     nothing reads the fx feed): a launch's dust and flash, its smoke trail, flak bursts, tracer, a fire.
//   * The warning receiver as it was: type and bearing with error (or a quadrant), a range class, never our position.
//   * Deterministic draws: every draw is keyed on the seed, the stream, the observer, the object and a tick or pass
//     number, so a saved campaign replays to the bit and no draw depends on the order of a vector.
//
// Every number is in data/gameplay/perception.json (with the receivers' displays in data/air_war/sensors.json).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/em/optical.hpp"
#include "simcore/em/random.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/light.hpp"
#include "simcore/radar.hpp"
#include "simcore/terrain.hpp"
#include "simcore/vec3.hpp"
#include "simcore/weather.hpp"

namespace simcore {
namespace airwar {

enum class SightKind { Aircraft, Trail, LaunchSign, Burst, Tracer, GroundObject, Fire, Contrail, Emitter };
enum class SightBy { Eye, Rwr, Radar, Irst, Pod, Voice };
enum class RwrState { None, Search, Lock, Launch };
enum class Pattern { Small, Medium, Large };
enum class GroundContrast { Low, Medium, High };
enum class LookPhase { Transit, LowLevel, Attack, Defensive };

inline const char* ToString(SightKind k) {
    switch (k) {
        case SightKind::Aircraft: return "aircraft";
        case SightKind::Trail: return "trail";
        case SightKind::LaunchSign: return "launch sign";
        case SightKind::Burst: return "burst";
        case SightKind::Tracer: return "tracer";
        case SightKind::GroundObject: return "ground object";
        case SightKind::Fire: return "fire";
        case SightKind::Contrail: return "contrail";
        case SightKind::Emitter: return "emitter";
    }
    return "?";
}
inline const char* ToString(SightBy b) {
    switch (b) {
        case SightBy::Eye: return "eye";
        case SightBy::Rwr: return "rwr";
        case SightBy::Radar: return "radar";
        case SightBy::Irst: return "irst";
        case SightBy::Pod: return "pod";
        case SightBy::Voice: return "voice";
    }
    return "?";
}

// ------------------------------------------------------------------------------------------------ the numbers

struct EyeData {
    double rm_constant_m = 1006.0;
    double threshold_offset_pct = 1.565;
    double factor_confidence = 1.64;
    double factor_unknown_location = 1.31;
    double factor_vigilance = 1.19;
    double factor_untrained = 2.0;
    double factor_ordinary = 1.2;
    double fatigue_vigilance_gain = 0.5;
    double empty_field_range_factor = 0.5;
    double glimpse_s = 1.65;
    double search_solid_angle_sr = 3.14159;
    double cue_sector_deg = 15.0;
    double lobe_max_sr = 0.35;
    double resolution_rad = 2.9e-4;
    double recognise_lines = 2.8;
    double aircraft_contrast_pct = 30.0;
    double aircraft_ground_contrast_pct = 15.0;
    double afterburner_contrast_pct = 60.0;
    double afterburner_area_m2 = 6.0;
    double twilight_contrast_loss = 0.7;
    double contrail_cap_m = 37000.0;
    double contrail_contrast_pct = 25.0;
    double contrail_width_m = 60.0;
    double sun_glare_deg = 15.0;
    double sun_glare_contrast_factor = 0.3;
    double max_range_m = 60000.0;
    double los_sample_m = 150.0;
    double los_cache_near_s = 0.5;
    double los_cache_far_s = 2.0;
    double default_skill = 0.7;
    double area_nose_per_size2 = 0.5;
    double area_beam_per_size2 = 2.4;
    double area_plan_per_size2 = 3.6;
};

struct LookoutData {
    double transit = 0.9;
    double low_level = 0.5;
    double attack = 0.2;
    double defensive = 0.7;
    double second_seat_extra = 0.3;
    double low_level_agl_m = 300.0;
    double check_six_share = 0.1;
    double rear_half_deg = 25.0;  // blind: the cone about the tail of a single seat
};

struct GroundData {
    double pmax[3][3] = {{0.25, 0.37, 0.50}, {0.37, 0.56, 0.75}, {0.50, 0.75, 1.00}};  // [pattern][contrast], small..large, low..high
    double zero_at_rstar = 0.75;
    double at_0625 = 0.2;
    double at_05 = 0.5;
    double at_0375 = 0.8;
    double full_at_rstar = 0.25;
    double off_angle_loss_per_deg = 0.0033;
    double off_track_limit_deg = 30.0;
    double unbriefed_scale = 0.7;
    double pass_gap_factor = 1.5;
    double pass_gap_m = 2000.0;
    double recognise_good_m = 3000.0;
    double recognise_poor_m = 1000.0;
    double max_range_m = 40000.0;
};

struct GroundClassDef {
    Pattern pattern = Pattern::Small;
    GroundContrast contrast = GroundContrast::Low;
    double area_m2 = 60.0;
};

struct TrailData {
    double contrast_pct = 20.0;
    double width_m = 1.5;
    double max_length_m = 500.0;
    double point_gap_m = 150.0;
    double max_points = 48.0;
    double tracer_night_range_m = 8000.0;
    double flame_night_range_m = 6000.0;
    double night_area_m2 = 2.0;
    double night_contrast_pct = 100.0;
};

struct BurstData {
    double life_s = 8.0;
    double area_m2 = 40.0;
    double contrast_pct = 35.0;
    double flash_life_s = 0.3;
    double flash_area_m2 = 40.0;
    double flash_contrast_pct = 100.0;
    double night_range_m = 6000.0;
};

struct TracerData {
    double life_s = 1.5;
    double area_m2 = 25.0;
    double contrast_pct = 100.0;
    double night_range_m = 8000.0;
};

struct FireData {
    double life_s = 120.0;
    double area_m2 = 8.0;
    double contrast_pct = 50.0;
    double night_area_m2 = 6.0;
};

struct ErrorData {
    double bearing_sigma_deg = 2.0;
    double elevation_sigma_deg = 2.0;
    double range_sigma_frac = 0.3;
    double ground_bearing_sigma_deg = 2.0;
    double ground_range_sigma_frac = 0.15;
};

struct MemoryData {
    double aircraft_s = 20.0, trail_s = 60.0, launch_sign_s = 600.0, burst_s = 30.0, tracer_s = 20.0;
    double ground_object_s = 600.0, fire_s = 120.0, contrail_s = 60.0, rwr_s = 6.0, event_keep_s = 30.0;
};

struct RateData {
    double transit_hz = 1.0, threat_hz = 2.0, defensive_hz = 5.0;
};

struct ConfidenceData {
    double seen = 0.4, watched = 0.6, recognised = 0.8, two_senses = 0.95;
};

// A warning receiver's display (data/air_war/sensors.json "rwr_sets"): what the set shows the pilot of what its EM
// receiver heard (docs/air_battle/03_PERCEPTION.md 3.2). The receiver's physics stays in data/em/.
struct RwrSetDisplay {
    std::string em_esm;                 // the EM receiver it is the display of
    std::string name;
    bool shows_name = true;             // the threat library's name (else search or lock only)
    bool quadrant = false;              // the bearing quantised to the quadrant of four antenna units
    bool range_class = true;            // near, middle or far from the received power
    bool launch_indication = true;      // a missile-guidance contact is shown as a launch (else it reads as a lock)
    double bearing_sigma_deg = 10.0;    // used when the contact carries none
    double false_alarms_per_h = 1.0;
    int max_display = 8;
    double near_dbw = -65.0;            // received power above this reads near, above mid_dbw middle, else far
    double mid_dbw = -80.0;
    double near_m = 6000.0, mid_m = 16000.0, far_m = 35000.0;  // what a class means as a distance
    std::string source;
    std::string confidence;
};

struct PerceptionData {
    EyeData eye;
    LookoutData lookout;
    GroundData ground;
    std::map<std::string, GroundClassDef> classes;
    TrailData trail;
    double launch_sign_life_s = 25.0;
    double launch_flash_night_range_m = 15000.0;
    BurstData burst;
    TracerData tracer;
    FireData fire;
    ErrorData errors;
    MemoryData memory;
    RateData rate;
    ConfidenceData confidence;
    std::vector<RwrSetDisplay> rwr_sets;

    PerceptionData() {
        classes["osa_battery_silent"] = {Pattern::Small, GroundContrast::Low, 60.0};
        classes["launch_sign"] = {Pattern::Large, GroundContrast::High, 400.0};
        classes["column_moving"] = {Pattern::Large, GroundContrast::Medium, 300.0};
        classes["vehicles_parked"] = {Pattern::Small, GroundContrast::Low, 40.0};
        classes["bridge_or_works"] = {Pattern::Large, GroundContrast::High, 500.0};
    }
    const RwrSetDisplay* RwrSet(const std::string& em_esm) const {
        for (const RwrSetDisplay& s : rwr_sets) {
            if (s.em_esm == em_esm) return &s;
        }
        return nullptr;
    }
    const GroundClassDef* Class(const std::string& id) const {
        const auto it = classes.find(id);
        return it == classes.end() ? nullptr : &it->second;
    }
};

// ------------------------------------------------------------------------------------------------ the laws (pure)

// The product of the field factors (TP 5709, Taylor). `cued`: the pilot knows where to look (a wingman's call, a
// receiver's bearing, a briefed place), so the unknown-location factor falls away. `skill` 0..1: 1 is trained (the
// untrained factor 1), 0 untrained (2.0). `stress` 0..1 raises the vigilance factor by up to the data's gain.
inline double EyeFactor(const EyeData& d, bool cued, double skill, double stress = 0.0) {
    const double s = skill < 0.0 ? 0.0 : (skill > 1.0 ? 1.0 : skill);
    const double st = stress < 0.0 ? 0.0 : (stress > 1.0 ? 1.0 : stress);
    const double untrained = 1.0 + (d.factor_untrained - 1.0) * (1.0 - s);
    const double vigilance = d.factor_vigilance * (1.0 + d.fatigue_vigilance_gain * st);
    return d.factor_confidence * (cued ? 1.0 : d.factor_unknown_location) * vigilance * untrained * d.factor_ordinary;
}

// The contrast (per cent) the eye needs at `range_m` for an object of `area_m2`, in the field factor `factor`.
inline double EyeThresholdPct(const EyeData& d, double factor, double range_m, double area_m2) {
    const double k = d.rm_constant_m;
    return factor * (d.threshold_offset_pct + range_m * range_m / (k * k * std::max(area_m2, 1e-9)));
}

// The apparent contrast (per cent): Koschmieder's law and the cloud between.
inline double EyeApparentPct(double inherent_pct, double range_m, double visibility_m, double cloud_transmission = 1.0) {
    return em::ApparentContrast(inherent_pct, range_m, visibility_m) * cloud_transmission;
}

// How far above its threshold an object is at `range_m` (>= 1: seen). Against empty sky the range counts as
// 1 / empty_field_range_factor times as far (the foveal range halves).
inline double EyeMargin(const EyeData& d, double factor, double inherent_pct, double area_m2, double range_m,
                        double visibility_m, double cloud_transmission, bool empty_field) {
    const double eff = empty_field ? range_m / d.empty_field_range_factor : range_m;
    const double th = EyeThresholdPct(d, factor, eff, area_m2);
    if (th <= 0.0) return 1e9;
    return EyeApparentPct(inherent_pct, eff, visibility_m, cloud_transmission) / th;
}

// The range at which the margin falls to 1 (bisection; for tests and calibration, not for the tick).
inline double EyeRangeM(const EyeData& d, double factor, double inherent_pct, double area_m2, double visibility_m,
                        double cloud_transmission, bool empty_field) {
    if (EyeMargin(d, factor, inherent_pct, area_m2, 1.0, visibility_m, cloud_transmission, empty_field) < 1.0) return 0.0;
    double lo = 1.0, hi = 1.0e6;
    for (int i = 0; i < 40; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (EyeMargin(d, factor, inherent_pct, area_m2, mid, visibility_m, cloud_transmission, empty_field) >= 1.0) lo = mid;
        else hi = mid;
    }
    return lo;
}

// A glimpse's solid angle against the margin over threshold: zero at the threshold, the lobe's maximum at a wide margin.
inline double GlimpseLobeSr(const EyeData& d, double margin) {
    if (margin <= 1.0) return 0.0;
    return d.lobe_max_sr * (1.0 - 1.0 / margin);
}

// The chance one glimpse finds the object: the lobe over the solid angle searched (the cued sector when cued).
inline double GlimpseChance(const EyeData& d, double margin, bool cued) {
    double search = d.search_solid_angle_sr;
    if (cued) {
        const double w = 2.0 * d.cue_sector_deg * kPi / 180.0;
        search = std::min(search, w * w);
    }
    if (search <= 0.0) return 1.0;
    return std::min(1.0, GlimpseLobeSr(d, margin) / search);
}

// Range at which a type is recognised (not just seen): the object's critical size across `recognise_lines` of the eye's
// resolution.
inline double RecognitionRangeM(const EyeData& d, double size_m) {
    return size_m / (d.recognise_lines * d.resolution_rad);
}

// The ground law's P_max (NWC TP 6005).
inline double GroundPmax(const GroundData& g, Pattern p, GroundContrast c) { return g.pmax[static_cast<int>(p)][static_cast<int>(c)]; }

// The cumulative probability of having acquired a ground object at `range_m`, given R* (the smaller of the unmask range
// and the visibility): zero beyond 0.75 R*, 0.2 / 0.5 / 0.8 of P_max at 0.625 / 0.5 / 0.375 R*, P_max by 0.25 R*,
// linear between (TP 6005 gives the middle three points; the ends are interpolations).
inline double GroundAcquireP(const GroundData& g, double pmax, double range_m, double rstar_m) {
    if (rstar_m <= 0.0) return 0.0;
    const double f = range_m / rstar_m;
    struct Pt {
        double f, p;
    };
    const Pt pts[] = {{g.zero_at_rstar, 0.0}, {0.625, g.at_0625}, {0.5, g.at_05}, {0.375, g.at_0375}, {g.full_at_rstar, 1.0}};
    if (f >= pts[0].f) return 0.0;
    if (f <= pts[4].f) return pmax;
    for (int i = 0; i < 4; ++i) {
        if (f <= pts[i].f && f >= pts[i + 1].f) {
            const double t = (pts[i].f - f) / (pts[i].f - pts[i + 1].f);
            return pmax * (pts[i].p + (pts[i + 1].p - pts[i].p) * t);
        }
    }
    return pmax;
}

// ------------------------------------------------------------------------------------------------ the picture

struct Sighting {
    SightKind kind = SightKind::Aircraft;
    SightBy by = SightBy::Eye;
    RwrState rwr = RwrState::None;  // an Emitter sighting: what the receiver says it is
    std::string what;               // the class, or the type once recognised ("aircraft", "MiG-21", "SAM launch", "lock")
    double bearing_rad = 0.0;       // true azimuth, as estimated
    double elevation_rad = 0.0;
    double range_est_m = 0.0;
    double range_sigma_m = 0.0;
    Vec3 believed_enu;              // the pilot's estimate, never the truth
    Vec3 origin_enu;                // a trail: where he believes it began (the launch site)
    bool has_origin = false;
    double confidence = 0.0;        // 0..1: seen once, watched, recognised, confirmed by a second sense
    double first_s = 0.0;
    double time_s = 0.0;            // when last seen: age = now - time_s
    int seen_count = 0;
    bool recognised = false;
    std::string seen_by;            // the aircraft
    std::uint64_t debug_truth_key = 0;  // the object's key: the hub's merge key, overlays and tests only; no decision reads it
    bool borrowed = false;          // a receiver's contact heard on his leader's receiver (a wingman at A1, roadmap B12)
};

// A thing the simulation made that can be seen: recorded by the runner beside the place that makes it.
struct VisibleEvent {
    std::uint64_t serial = 0;
    SightKind kind = SightKind::LaunchSign;
    std::string what;
    std::string owner;             // the id of what made it (debug only)
    Vec3 pos;                      // its place; a trail's head
    double start_s = 0.0;
    double until_s = 0.0;          // seen until
    double area_m2 = 0.0;
    double contrast_pct = 0.0;
    bool shows_at_night = false;
    double night_range_m = 0.0;    // by night the eye sees it only inside this
    bool ground = false;           // searched for by the ground law (a launch sign, a site)
    Pattern pattern = Pattern::Large;
    GroundContrast ground_contrast = GroundContrast::High;
    // A trail: the points the missile has flown, each with its time; it hangs until `hang_s` after a point was laid.
    std::vector<Vec3> trail_pts;
    std::vector<double> trail_t;
    double hang_s = 40.0;
    double width_m = 1.5;
    double trail_len_m = 0.0;
    int launch_id = 0;             // the launch that made it (the runner's count; debug and the bridge)
};

// A ground object the pilot may find on a pass (the battery, a column, a bridge).
struct GroundObject {
    std::uint64_t id = 0;
    std::string cls;
    std::string owner;
    Vec3 pos;
    bool briefed = false;          // the briefing put it here: searched with the unknown-location factor off, no scale
};

// An aircraft another may see.
struct AirObject {
    std::string id;
    std::uint64_t key = 0;
    int side = 0;
    Vec3 pos;
    Vec3 vel;
    double alt_m = 0.0;
    double size_m = 4.5;
    std::string type;
    bool afterburner = false;
    bool burning = false;          // on fire: seen at night
    double contrail_area_m2 = 0.0; // its contrail now (0: none)
    bool weapon = false;
};

struct RwrHeard {
    std::uint64_t key = 0;
    RwrState state = RwrState::Search;
    std::string name;              // the library's name; "" if it matched no entry
    bool known = false;
    double bearing_rad = 0.0;      // true azimuth, measured
    double sigma_rad = 0.0;
    double power_dbw = -300.0;
    // Heard on his leader's receiver, not his own (a wingman at A1 keeping his slot, roadmap B12): a search or a missile's
    // guidance the flight hears together, never a lock (a lock is a radar's beam held on one aircraft). `key` is then the
    // leader's receiver's contact; his own receiver's contact of the same emitter takes its place in his picture when his
    // receiver is back on, and the other way about (Adopt).
    bool borrowed = false;
};

struct ObserverIn {
    std::string id;
    std::uint64_t key = 0;
    int side = 0;
    Vec3 pos;
    Vec3 vel;
    double alt_m = 0.0;
    double agl_m = 1000.0;
    bool two_seat = false;
    double skill = -1.0;           // < 0: the data's default
    double stress = 0.0;
    bool defensive = false;
    bool attack = false;
    const RwrSetDisplay* rwr_set = nullptr;
};

struct EnvIn {
    const Terrain* terrain = nullptr;
    const LocalFrame* frame = nullptr;
    const simcore::Weather* weather = nullptr;  // (airwar::Weather is the campaign's enum)
    SunLight light;
    double visibility_m = 40000.0;
};

namespace detail {

inline double Wrap2Pi(double a) {
    a = std::fmod(a, kTwoPi);
    return a < 0.0 ? a + kTwoPi : a;
}
inline double WrapPi(double a) {
    a = Wrap2Pi(a);
    return a > kPi ? a - kTwoPi : a;
}
inline std::uint64_t HashString(const std::string& s) {
    std::uint64_t h = 1469598103934665603ull;
    for (char c : s) h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
    return em::Mix64(h);
}

}  // namespace detail

// A key for an aircraft, from its id: stable across runs and orders.
// The top four bits stay clear: the hub tags an event's key and a ground object's with bits 62 and 61.
inline std::uint64_t ObjectKeyOf(const std::string& id) { return detail::HashString(id) & 0x0FFFFFFFFFFFFFFFull; }

// What a pilot can know of his surroundings, kept per observer. See the file's header.
class PerceptionHub {
public:
    PerceptionHub(const PerceptionData& d, std::uint64_t seed) : d_(d), seed_(seed) {}

    const PerceptionData& Data() const { return d_; }

    // ---- what the simulation made (recorded by the runner)
    std::uint64_t AddEvent(VisibleEvent e) {
        e.serial = ++serial_;
        events_.push_back(std::move(e));
        return events_.back().serial;
    }
    VisibleEvent* FindEvent(std::uint64_t serial) {
        for (VisibleEvent& e : events_) {
            if (e.serial == serial) return &e;
        }
        return nullptr;
    }
    const std::vector<VisibleEvent>& Events() const { return events_; }
    // A trail's head moved: a point is laid every point_gap_m. `burning`: the motor still burns (the tracer and the flame
    // show by night).
    void ExtendTrail(std::uint64_t serial, const Vec3& head, double now_s, bool burning) {
        VisibleEvent* e = FindEvent(serial);
        if (e == nullptr) return;
        e->pos = head;
        e->shows_at_night = burning;
        if (e->trail_pts.empty() || (head - e->trail_pts.back()).Length() >= d_.trail.point_gap_m) {
            if (!e->trail_pts.empty()) e->trail_len_m += (head - e->trail_pts.back()).Length();
            e->trail_pts.push_back(head);
            e->trail_t.push_back(now_s);
            if (static_cast<double>(e->trail_pts.size()) > d_.trail.max_points) {
                e->trail_len_m -= (e->trail_pts[1] - e->trail_pts[0]).Length();
                e->trail_pts.erase(e->trail_pts.begin());
                e->trail_t.erase(e->trail_t.begin());
            }
        }
        e->until_s = now_s + e->hang_s;
    }
    std::uint64_t AddGround(const std::string& cls, const Vec3& pos, const std::string& owner = std::string(), bool briefed = false) {
        GroundObject g;
        g.id = ++ground_serial_;
        g.cls = cls;
        g.owner = owner;
        g.pos = pos;
        g.briefed = briefed;
        ground_.push_back(g);
        return g.id;
    }
    void MoveGround(std::uint64_t id, const Vec3& pos) {
        if (GroundObject* g = FindGround(id)) g->pos = pos;
    }
    GroundObject* FindGround(std::uint64_t id) {
        for (GroundObject& g : ground_) {
            if (g.id == id) return &g;
        }
        return nullptr;
    }
    const std::vector<GroundObject>& Grounds() const { return ground_; }

    // ---- the observers
    // A cue to look toward `bearing_rad` (a call, a receiver, a briefing) until `until_s`.
    void AddCue(std::uint64_t observer_key, double bearing_rad, double until_s) {
        Obs& o = obs_[observer_key];
        o.cues.push_back({detail::Wrap2Pi(bearing_rad), until_s});
    }

    // Is this observer's tick due? Its first tick is offset by a keyed draw within its period, so ticks spread over steps.
    bool Due(std::uint64_t observer_key, double now_s) {
        Obs& o = obs_[observer_key];
        if (!o.started) {
            o.started = true;
            const double period = 1.0 / d_.rate.transit_hz;
            o.next_tick_s = now_s + period * em::Uniform01(em::DrawHash(seed_, em::Stream::Sight, observer_key, 0xFFFFull));
            o.last_tick_s = now_s;
            o.last_event_check_s = now_s;  // what ended before he was there is not for him to see
        }
        return now_s + 1e-9 >= o.next_tick_s;
    }

    // One perception tick for one observer: looks at what is in view and updates his picture. `fresh` receives what is new
    // or has grown more serious (a receiver's search become a lock) in this tick.
    void Tick(const ObserverIn& ob, const EnvIn& env, double now_s, const std::vector<AirObject>& air,
              const std::vector<RwrHeard>& rwr, std::vector<Sighting>* fresh) {
        Obs& o = obs_[ob.key];
        if (!o.started) {
            Due(ob.key, now_s);
        }
        ++ticks_;
        const double dt = std::max(0.05, now_s - o.last_tick_s);
        o.last_tick_s = now_s;
        // The next tick: faster while threatened.
        const bool threatened = ob.defensive || !rwr.empty() || o.threat_until_s > now_s;
        const double hz = ob.defensive || ob.attack ? d_.rate.defensive_hz : (threatened ? d_.rate.threat_hz : d_.rate.transit_hz);
        o.tick_index = static_cast<std::uint64_t>(std::floor(now_s * hz + 1e-9));
        o.next_tick_s = now_s + 1.0 / hz;
        // Cues expire.
        o.cues.erase(std::remove_if(o.cues.begin(), o.cues.end(), [&](const Cue& c) { return c.until_s < now_s; }), o.cues.end());

        const double skill = ob.skill >= 0.0 ? ob.skill : d_.eye.default_skill;
        LookPhase phase = LookPhase::Transit;
        if (ob.defensive) phase = LookPhase::Defensive;
        else if (ob.attack) phase = LookPhase::Attack;
        else if (ob.agl_m < d_.lookout.low_level_agl_m) phase = LookPhase::LowLevel;
        double lookout = 0.0;
        switch (phase) {
            case LookPhase::Transit: lookout = d_.lookout.transit; break;
            case LookPhase::LowLevel: lookout = d_.lookout.low_level; break;
            case LookPhase::Attack: lookout = d_.lookout.attack; break;
            case LookPhase::Defensive: lookout = d_.lookout.defensive; break;
        }
        if (ob.two_seat) lookout = std::min(1.0, lookout + d_.lookout.second_seat_extra);
        const double glimpses = dt / d_.eye.glimpse_s * lookout;

        Vec3 heading = ob.vel;
        heading.z = 0.0;
        heading = heading.LengthSquared() > 1.0 ? heading.Normalized() : Vec3{0.0, 1.0, 0.0};
        const double heading_az = AzimuthOf(heading);

        View v;
        v.ob = &ob;
        v.env = &env;
        v.now = now_s;
        v.glimpses = glimpses;
        v.skill = skill;
        v.heading = heading;
        v.heading_az = heading_az;
        v.night = env.light.night_share >= 0.9;
        v.sun = DirectionFromAzEl(env.light.sun_azimuth_deg * kPi / 180.0, env.light.sun_elevation_deg * kPi / 180.0);
        v.sun_up = env.light.sun_elevation_deg > 0.0;

        // Aircraft of the other side, and their contrails.
        const double max_r2 = d_.eye.max_range_m * d_.eye.max_range_m;
        const double best_factor = EyeFactor(d_.eye, true, 1.0, 0.0);
        const double c_hi = std::max(std::max(d_.eye.aircraft_contrast_pct, d_.eye.aircraft_ground_contrast_pct),
                                     std::max(d_.eye.afterburner_contrast_pct, d_.eye.contrail_contrast_pct));
        for (const AirObject& a : air) {
            if (a.side == ob.side || a.key == ob.key) continue;
            const Vec3 dv = a.pos - ob.pos;
            const double r2 = dv.LengthSquared();
            if (r2 > max_r2) continue;
            // The farthest any eye could see it: the best field factor, the best contrast and its largest area (cost, 5.7).
            const double s2 = a.size_m * a.size_m;
            const double area_hi = std::max(std::max(d_.eye.area_plan_per_size2, d_.eye.area_beam_per_size2) * s2,
                                            std::max(a.contrail_area_m2, std::max(d_.eye.afterburner_area_m2, d_.fire.night_area_m2)));
            const double far_m = d_.eye.rm_constant_m * std::sqrt(std::max(0.0, (c_hi / best_factor - d_.eye.threshold_offset_pct) * area_hi));
            if (r2 > far_m * far_m) continue;
            LookAtAircraft(o, v, a, std::sqrt(r2), fresh);
        }
        // Events live now or since the last tick.
        for (const VisibleEvent& e : events_) {
            if (e.until_s + 1e-9 < o.last_event_check_s) continue;
            if (e.start_s > now_s + 1e-9) continue;
            LookAtEvent(o, v, e, fresh);
        }
        o.last_event_check_s = now_s;
        // Ground objects (a silent site, a column).
        for (const GroundObject& g : ground_) LookAtGround(o, v, g, fresh);
        // The receiver.
        IngestRwr(o, v, rwr, fresh);
        // Age the picture out.
        Age(o, now_s);
    }

    const std::vector<Sighting>& Picture(std::uint64_t observer_key) const {
        static const std::vector<Sighting> kNone;
        const auto it = obs_.find(observer_key);
        return it == obs_.end() ? kNone : it->second.picture;
    }
    // Does the observer's picture hold a sighting of `kind` seen within `age_s` of `now_s`?
    bool Holds(std::uint64_t observer_key, SightKind kind, double now_s, double age_s) const {
        for (const Sighting& s : Picture(observer_key)) {
            if (s.kind == kind && now_s - s.time_s <= age_s) return true;
        }
        return false;
    }
    // Is some observer of `side` holding a sighting of a launch (its sign or its trail) of `launch_id`? Truth ids belong to
    // the runner's own bookkeeping (the mission bridge's "the enemy saw the launch"), never to a pilot's decision.
    bool LaunchSeen(int launch_id) const { return launch_seen_.count(launch_id) != 0; }
    const std::map<int, double>& LaunchesSeen() const { return launch_seen_; }
    // The sightings of a launch (the first time each observer saw it): for the report that goes up.
    const std::vector<std::pair<std::string, Sighting>>& LaunchSightings() const { return launch_sightings_; }

    void Prune(double now_s) {
        events_.erase(std::remove_if(events_.begin(), events_.end(),
                                     [&](const VisibleEvent& e) { return now_s > e.until_s + d_.memory.event_keep_s; }),
                      events_.end());
    }

    // Counters for the cost bench.
    std::uint64_t Ticks() const { return ticks_; }
    std::uint64_t Looks() const { return looks_; }
    std::uint64_t SightLines() const { return los_calls_; }

private:
    struct Cue {
        double bearing_rad;
        double until_s;
    };
    struct ObjState {
        double miss = 1.0;       // the chance no glimpse so far found it
        bool tracked = false;
        int seen_count = 0;
        // The ground law's pass.
        int pass = 0;
        double unmask_m = -1.0;
        double min_r = 1e18;
        bool acquired = false;
        double los_time = -1e9;
        bool los = false;
    };
    struct Obs {
        bool started = false;
        double next_tick_s = 0.0;
        double last_tick_s = 0.0;
        double last_event_check_s = -1e9;
        double threat_until_s = 0.0;
        std::uint64_t tick_index = 0;
        std::vector<Cue> cues;
        std::vector<Sighting> picture;
        std::map<std::uint64_t, ObjState> objs;
    };
    struct View {
        const ObserverIn* ob = nullptr;
        const EnvIn* env = nullptr;
        double now = 0.0;
        double glimpses = 0.0;
        double skill = 0.7;
        Vec3 heading;
        double heading_az = 0.0;
        bool night = false;
        Vec3 sun;
        bool sun_up = false;
    };

    static constexpr std::uint64_t kEventBit = 1ull << 62;
    static constexpr std::uint64_t kGroundBit = 1ull << 61;
    static constexpr std::uint64_t kBorrowedBit = 1ull << 60;  // a contact of his leader's receiver (Adopt)

    PerceptionData d_;
    std::uint64_t seed_;
    std::uint64_t serial_ = 0;
    std::uint64_t ground_serial_ = 0;
    std::vector<VisibleEvent> events_;
    std::vector<GroundObject> ground_;
    std::map<std::uint64_t, Obs> obs_;
    std::map<int, double> launch_seen_;
    std::vector<std::pair<std::string, Sighting>> launch_sightings_;
    std::uint64_t ticks_ = 0;
    std::uint64_t looks_ = 0;
    std::uint64_t los_calls_ = 0;
    std::vector<std::uint64_t> held_keys_;  // Age's scratch: the picture's keys, sorted

    double Normal(const View& v, std::uint64_t obj_key, std::uint64_t tag) const {
        return em::StandardNormal(em::DrawHash(seed_, em::Stream::SightError, v.ob->key, obj_key, obs_.at(v.ob->key).tick_index, tag));
    }

    bool InCue(const Obs& o, double bearing_rad, double slack_deg) const {
        const double w = (d_.eye.cue_sector_deg + slack_deg) * kPi / 180.0;
        for (const Cue& c : o.cues) {
            if (std::fabs(detail::WrapPi(bearing_rad - c.bearing_rad)) <= w) return true;
        }
        return false;
    }

    // A sight line over the terrain from the observer to a point, cached per pair.
    bool SightLine(ObjState& st, const View& v, const Vec3& to, double to_alt_m, double range_m) {
        const double ttl = range_m < 8000.0 ? d_.eye.los_cache_near_s : d_.eye.los_cache_far_s;
        if (v.now - st.los_time < ttl) return st.los;
        st.los_time = v.now;
        if (v.env->terrain == nullptr || v.env->frame == nullptr) {
            st.los = true;
            return true;
        }
        ++los_calls_;
        const double spacing = range_m > 20000.0 ? d_.eye.los_sample_m * 2.0 : d_.eye.los_sample_m;
        st.los = HasLineOfSight(*v.env->terrain, v.ob->pos, v.ob->alt_m, to, to_alt_m, spacing);
        return st.los;
    }

    double CloudT(const View& v, const Vec3& to, double to_alt_m) const {
        if (v.env->weather == nullptr) return 1.0;
        return v.env->weather->CloudTransmission(v.ob->pos, v.ob->alt_m, to, to_alt_m);
    }

    // Inside the blind cone about a single seat's tail?
    bool Blind(const View& v, const Vec3& dir) const {
        if (v.ob->two_seat) return false;
        const double c = Dot(dir.Normalized(), v.heading);
        return c < -std::cos(d_.lookout.rear_half_deg * kPi / 180.0);
    }

    double SunFactor(const View& v, const Vec3& dir) const {
        if (!v.sun_up || v.night) return 1.0;
        const double c = Dot(dir, v.sun);
        return c > std::cos(d_.eye.sun_glare_deg * kPi / 180.0) ? d_.eye.sun_glare_contrast_factor : 1.0;
    }

    double DayContrast(const View& v, double pct) const {
        // Dawn and dusk: the contrast falls by the night share.
        return pct * (1.0 - d_.eye.twilight_contrast_loss * v.env->light.night_share);
    }

    // One draw's merge into the picture; returns the sighting and whether it is new.
    Sighting* Upsert(Obs& o, const View& v, Sighting s, bool* is_new) {
        for (Sighting& e : o.picture) {
            if (e.debug_truth_key == s.debug_truth_key && e.kind == s.kind && e.by == s.by) {
                const int seen = e.seen_count + 1;
                const double first = e.first_s;
                const bool was_recognised = e.recognised;
                const double conf = e.confidence;
                e = std::move(s);
                e.seen_count = seen;
                e.first_s = first;
                e.recognised = e.recognised || was_recognised;
                e.confidence = std::max(conf, e.confidence);
                if (e.seen_count >= 3) e.confidence = std::max(e.confidence, d_.confidence.watched);
                if (e.recognised) e.confidence = std::max(e.confidence, d_.confidence.recognised);
                *is_new = false;
                return &e;
            }
        }
        s.first_s = v.now;
        s.seen_count = 1;
        o.picture.push_back(std::move(s));
        *is_new = true;
        return &o.picture.back();
    }

    void Note(Obs& o, const View& v, const Sighting& s, bool is_new, std::vector<Sighting>* fresh, int launch_id) {
        if (is_new) {
            if (fresh != nullptr) fresh->push_back(s);
            // Only an enemy pilot's sight of the launch is the enemy's knowledge of it (side 0: the raid's; ours is 1).
            if (launch_id > 0 && v.ob->side == 0 && (s.kind == SightKind::LaunchSign || s.kind == SightKind::Trail) && s.by == SightBy::Eye) {
                if (launch_seen_.count(launch_id) == 0) launch_seen_[launch_id] = v.now;
                launch_sightings_.emplace_back(v.ob->id, s);
            }
            if (s.kind == SightKind::LaunchSign || s.kind == SightKind::Trail) o.threat_until_s = std::max(o.threat_until_s, v.now + 20.0);
        }
        (void)o;
    }

    // ---- aircraft (and their contrails): the eye law and the glimpse draws.
    void LookAtAircraft(Obs& o, const View& v, const AirObject& a, double range_m, std::vector<Sighting>* fresh) {
        const Vec3 d = a.pos - v.ob->pos;
        const Vec3 dir = d / std::max(range_m, 1.0);
        const double bearing = AzimuthOf(d);
        const double elev = ElevationOf(d);
        const bool contrail = a.contrail_area_m2 > 0.0;
        // Cheap culls first: the blind cone, the light.
        const bool dark = v.night;
        if (dark && !a.afterburner && !a.burning) return;  // by night only a flame shows; a contrail not at all (no moon)
        if (Blind(v, dir)) return;
        ++looks_;
        const bool cued = InCue(o, bearing, 0.0);
        const double factor = EyeFactor(d_.eye, cued, v.skill, v.ob->stress);
        // Presented area by aspect: the nose and the beam and the plan, blended by the angle between the sight line and its axis
        // and by the elevation.
        const Vec3 axis = a.vel.LengthSquared() > 1.0 ? a.vel.Normalized() : Vec3{0.0, 1.0, 0.0};
        const double ca = std::fabs(Dot(dir, axis));
        const double size2 = a.size_m * a.size_m;
        const double nose = d_.eye.area_nose_per_size2 * size2, beam = d_.eye.area_beam_per_size2 * size2, plan = d_.eye.area_plan_per_size2 * size2;
        const double side = nose * ca * ca + beam * (1.0 - ca * ca);
        const double s_el = std::fabs(std::sin(elev));
        const double area = a.weapon ? nose : side * (1.0 - s_el) + plan * s_el;
        const bool against_sky = elev > -0.09;  // above 5 degrees below the horizon: a clear sky behind it
        double c0 = against_sky ? d_.eye.aircraft_contrast_pct : d_.eye.aircraft_ground_contrast_pct;
        double use_area = area;
        double cap_range = 1e18;
        SightKind kind = SightKind::Aircraft;
        if (contrail && !dark) {
            kind = SightKind::Contrail;
            c0 = d_.eye.contrail_contrast_pct;
            use_area = a.contrail_area_m2;
            cap_range = d_.eye.contrail_cap_m;
        }
        if (dark) {
            c0 = d_.eye.afterburner_contrast_pct;
            use_area = a.burning && !a.afterburner ? d_.fire.night_area_m2 : d_.eye.afterburner_area_m2;
        } else {
            c0 = DayContrast(v, c0) * SunFactor(v, dir);
        }
        if (range_m > cap_range) return;
        // An upper bound on the range at which any haze-free eye could see it: a cull before the line of sight.
        const double bound = d_.eye.rm_constant_m * std::sqrt(std::max(0.0, (c0 / factor - d_.eye.threshold_offset_pct) * use_area));
        if (range_m > bound) return;
        const double alt = a.alt_m;
        ObjState& st = o.objs[a.key];
        // The margin is arithmetic; the sight line walks the terrain. The margin goes first, so a line is walked only for
        // an aircraft the eye could see if the way were open (roadmap B3 cost, 03_PERCEPTION.md 5.7).
        const double cloud = CloudT(v, a.pos, alt);
        // A contrail is extended, not a point against the sky: no empty-field myopia.
        const bool empty = against_sky && kind == SightKind::Aircraft && !dark;  // a light is not a point of low contrast
        const double margin = EyeMargin(d_.eye, factor, c0, use_area, range_m, v.env->visibility_m, cloud, empty);
        if (margin < 1.0) {
            st.tracked = false;
            return;
        }
        if (!SightLine(st, v, a.pos, alt, range_m)) return;
        if (!st.tracked) {
            const double g = GlimpseChance(d_.eye, margin, cued);
            st.miss *= std::pow(1.0 - g, v.glimpses);
            const double u = em::Uniform01(em::DrawHash(seed_, em::Stream::Sight, v.ob->key, a.key, 1));
            if (1.0 - st.miss < u) return;
            st.tracked = true;
        }
        // Seen: bearing and range as the eye gives them.
        const double nb = Normal(v, a.key, 1), ne = Normal(v, a.key, 2), nr = Normal(v, a.key, 3);
        Sighting s;
        s.kind = kind;
        s.by = SightBy::Eye;
        s.seen_by = v.ob->id;
        s.debug_truth_key = a.key;
        s.time_s = v.now;
        s.bearing_rad = bearing + nb * d_.errors.bearing_sigma_deg * kPi / 180.0;
        s.elevation_rad = elev + ne * d_.errors.elevation_sigma_deg * kPi / 180.0;
        s.range_sigma_m = d_.errors.range_sigma_frac * range_m;
        s.range_est_m = std::max(50.0, range_m + nr * s.range_sigma_m);
        s.believed_enu = v.ob->pos + DirectionFromAzEl(s.bearing_rad, s.elevation_rad) * s.range_est_m;
        s.recognised = kind == SightKind::Aircraft && range_m <= RecognitionRangeM(d_.eye, a.size_m) &&
                       margin >= 1.0;
        s.what = kind == SightKind::Contrail ? "contrail" : (s.recognised && !a.type.empty() ? a.type : (a.weapon ? "missile" : "aircraft"));
        s.confidence = s.recognised ? d_.confidence.recognised : d_.confidence.seen;
        bool is_new = false;
        const Sighting* ref = Upsert(o, v, std::move(s), &is_new);
        Note(o, v, *ref, is_new, fresh, 0);
    }

    // ---- events: trails, bursts, tracer and fires are objects in the air under the eye law; a launch sign and a site under
    // the ground law.
    void LookAtEvent(Obs& o, const View& v, const VisibleEvent& e, std::vector<Sighting>* fresh) {
        if (e.ground) {
            LookAtGroundThing(o, v, kEventBit | e.serial, e.kind, e.what, e.pos, e.pattern, e.ground_contrast, e.area_m2, false,
                              e.shows_at_night, e.night_range_m, e.launch_id, fresh, e.start_s, e.until_s);
            return;
        }
        // The nearest live point of a trail, or the event's place.
        Vec3 place = e.pos;
        double area = e.area_m2;
        if (!e.trail_pts.empty()) {
            double best = 1e18;
            double live_len = 0.0;
            for (std::size_t i = 0; i < e.trail_pts.size(); ++i) {
                if (v.now - e.trail_t[i] > e.hang_s + 1e-9) continue;
                if (i > 0 && v.now - e.trail_t[i - 1] <= e.hang_s + 1e-9) live_len += (e.trail_pts[i] - e.trail_pts[i - 1]).Length();
                const double r = (e.trail_pts[i] - v.ob->pos).Length();
                if (r < best) {
                    best = r;
                    place = e.trail_pts[i];
                }
            }
            if (best > 1e17) return;
            area = e.width_m * std::min(std::max(live_len, e.width_m), d_.trail.max_length_m);
        }
        const Vec3 d = place - v.ob->pos;
        double range_m = d.Length();
        if (range_m > d_.eye.max_range_m || range_m < 1.0) return;
        const Vec3 dir = d / range_m;
        double bearing = AzimuthOf(d);
        double elev = ElevationOf(d);
        if (Blind(v, dir)) return;
        double c0 = e.contrast_pct;
        double use_area = area;
        double cap_range = 1e18;
        if (v.night) {
            if (!e.shows_at_night) return;
            c0 = d_.trail.night_contrast_pct;
            if (e.night_range_m > 0.0) cap_range = e.night_range_m;
            if (e.kind == SightKind::Trail) use_area = d_.trail.night_area_m2;
        } else {
            c0 = DayContrast(v, c0) * SunFactor(v, dir);
        }
        if (range_m > cap_range) return;
        ++looks_;
        const bool cued = InCue(o, bearing, 0.0);
        const double factor = EyeFactor(d_.eye, cued, v.skill, v.ob->stress);
        double alt = v.env->frame != nullptr ? v.env->frame->AltitudeOf(place) : place.z;
        ObjState& st = o.objs[kEventBit | e.serial];
        {  // The arithmetic first: the nearest point is the best the eye has, so if it fails no line is walked (cost, 5.7).
            const bool sky0 = elev > -0.09;
            const bool pt0 = sky0 && (e.kind == SightKind::Burst || e.kind == SightKind::Tracer);
            if (EyeMargin(d_.eye, factor, c0, use_area, range_m, v.env->visibility_m, CloudT(v, place, alt), pt0) < 1.0) return;
        }
        if (!SightLine(st, v, place, alt, range_m)) {
            // Behind a ridge: the trail's head may stand clear of it.
            if (e.trail_pts.empty() || (place - e.pos).LengthSquared() < 1e-6) return;
            const Vec3 head = e.pos;
            const double hr = (head - v.ob->pos).Length();
            const double halt = v.env->frame != nullptr ? v.env->frame->AltitudeOf(head) : head.z;
            if (v.env->terrain != nullptr && !HasLineOfSight(*v.env->terrain, v.ob->pos, v.ob->alt_m, head, halt, d_.eye.los_sample_m)) return;
            place = head;
            range_m = hr;
            elev = ElevationOf(head - v.ob->pos);
            bearing = AzimuthOf(head - v.ob->pos);
            alt = halt;
        }
        const double cloud = CloudT(v, place, alt);
        const bool against_sky = elev > -0.09;
        // A smoke trail and a fire are extended: the empty-field myopia is for a point against the sky, a puff or a tracer.
        const bool empty = against_sky && (e.kind == SightKind::Burst || e.kind == SightKind::Tracer);
        const double margin = EyeMargin(d_.eye, factor, c0, use_area, range_m, v.env->visibility_m, cloud, empty);
        if (margin < 1.0) return;
        if (!st.tracked) {
            const double g = GlimpseChance(d_.eye, margin, cued);
            // The glimpses since this observer last looked, or the whole life of a flash shorter than his tick.
            st.miss *= std::pow(1.0 - g, std::max(v.glimpses, 1.0));
            const double u = em::Uniform01(em::DrawHash(seed_, em::Stream::Sight, v.ob->key, kEventBit | e.serial, 1));
            if (1.0 - st.miss < u) return;
            st.tracked = true;
        }
        const double nb = Normal(v, kEventBit | e.serial, 1), ne = Normal(v, kEventBit | e.serial, 2), nr = Normal(v, kEventBit | e.serial, 3);
        Sighting s;
        s.kind = e.kind;
        s.by = SightBy::Eye;
        s.what = e.what;
        s.seen_by = v.ob->id;
        s.debug_truth_key = kEventBit | e.serial;
        s.time_s = v.now;
        s.bearing_rad = bearing + nb * d_.errors.bearing_sigma_deg * kPi / 180.0;
        s.elevation_rad = elev + ne * d_.errors.elevation_sigma_deg * kPi / 180.0;
        s.range_sigma_m = d_.errors.range_sigma_frac * range_m;
        s.range_est_m = std::max(50.0, range_m + nr * s.range_sigma_m);
        s.believed_enu = v.ob->pos + DirectionFromAzEl(s.bearing_rad, s.elevation_rad) * s.range_est_m;
        s.confidence = d_.confidence.seen;
        if (!e.trail_pts.empty()) {
            // Where he believes it began: the trail's first point with the same error as its nearest.
            const Vec3 off = s.believed_enu - place;
            s.origin_enu = e.trail_pts.front() + off;
            s.has_origin = true;
        }
        bool is_new = false;
        const Sighting* ref = Upsert(o, v, std::move(s), &is_new);
        Note(o, v, *ref, is_new, fresh, e.launch_id);
    }

    void LookAtGround(Obs& o, const View& v, const GroundObject& g, std::vector<Sighting>* fresh) {
        const GroundClassDef* c = d_.Class(g.cls);
        if (c == nullptr) return;
        LookAtGroundThing(o, v, kGroundBit | g.id, SightKind::GroundObject, g.cls, g.pos, c->pattern, c->contrast, c->area_m2, g.briefed,
                          false, 0.0, 0, fresh, -1e18, 1e18);
    }

    // The ground law: one keyed draw per pass; acquired at the first range where the cumulative chance reaches it.
    void LookAtGroundThing(Obs& o, const View& v, std::uint64_t key, SightKind kind, const std::string& what, const Vec3& pos,
                           Pattern pattern, GroundContrast gc, double area_m2, bool briefed, bool shows_at_night, double night_range_m,
                           int launch_id, std::vector<Sighting>* fresh, double start_s, double until_s) {
        (void)start_s;
        const Vec3 d = pos - v.ob->pos;
        const double range_m = d.Length();
        if (range_m > d_.ground.max_range_m || range_m < 1.0) return;
        // By night only what shows: a flash.
        double cap = 1e18;
        bool lit = false;
        if (v.night) {
            if (!shows_at_night) return;
            lit = true;
            if (night_range_m > 0.0) cap = night_range_m;
        }
        if (range_m > cap) return;
        // Within the field of search: ahead of the track, inside the limit (about 1 % lost per 3 degrees off).
        const Vec3 hd = Vec3{d.x, d.y, 0.0};
        const double off_deg = hd.LengthSquared() > 1.0 && v.heading.LengthSquared() > 0.0
                                   ? std::acos(std::max(-1.0, std::min(1.0, Dot(hd.Normalized(), v.heading)))) * 180.0 / kPi
                                   : 0.0;
        const bool cued = InCue(o, AzimuthOf(d), 0.0) || briefed;
        if (off_deg > d_.ground.off_track_limit_deg && !cued) return;
        ++looks_;
        ObjState& st = o.objs[key];
        // A new pass once the range has opened well beyond the closest approach.
        if (st.min_r < 1e17 && range_m > st.min_r * d_.ground.pass_gap_factor + d_.ground.pass_gap_m) {
            ++st.pass;
            st.min_r = 1e18;
            st.unmask_m = -1.0;
            st.acquired = false;
        }
        st.min_r = std::min(st.min_r, range_m);
        if (st.acquired) {
            // Held: refreshed while it is in view, so it does not age out.
            Refresh(o, v, key, kind, what, pos, range_m, launch_id, until_s, fresh);
            return;
        }
        // The pass's draw first: an object whose P_max is below it is not acquired on this pass, whatever the geometry.
        double pmax = GroundPmax(d_.ground, pattern, gc);
        pmax *= std::max(0.0, 1.0 - d_.ground.off_angle_loss_per_deg * off_deg);
        if (!briefed) pmax *= d_.ground.unbriefed_scale;
        // How much of the pilot's eyes are outside, relative to a transit's.
        double phase_scale = 1.0;
        if (v.ob->defensive) phase_scale = d_.lookout.defensive / d_.lookout.transit;
        else if (v.ob->attack) phase_scale = d_.lookout.attack / d_.lookout.transit;
        // (A low-level search is TP 6005's own case: its curve already holds the pilot's effort.)
        if (v.ob->two_seat) phase_scale = std::min(1.0, phase_scale + d_.lookout.second_seat_extra);
        pmax *= phase_scale;
        const double u = em::Uniform01(em::DrawHash(seed_, em::Stream::GroundAcquire, v.ob->key, key, static_cast<std::uint64_t>(st.pass)));
        const double alt = v.env->frame != nullptr ? v.env->frame->AltitudeOf(pos) : pos.z;
        if (u >= pmax) {
            // Not this pass; but the unmask range is still where the sight line first opens (the next pass may need it).
            return;
        }
        if (!SightLine(st, v, pos, alt + 2.0, range_m)) {
            st.unmask_m = -1.0;  // masked: the unmask range is where the sight line first opens
            return;
        }
        if (st.unmask_m < 0.0) st.unmask_m = range_m;
        // R*: the unmask range and the visibility, and what the eye could see of an object of this size and contrast.
        const double factor = EyeFactor(d_.eye, cued, v.skill, v.ob->stress);
        const double contrast_pct = lit ? 100.0 : DayContrast(v, 20.0 + 20.0 * static_cast<double>(static_cast<int>(gc)));
        const double cloud = CloudT(v, pos, alt);
        const double eye_r = EyeRangeM(d_.eye, factor, contrast_pct, area_m2, v.env->visibility_m, cloud, false);
        const double rstar = std::min(std::min(st.unmask_m, v.env->visibility_m), eye_r);
        if (GroundAcquireP(d_.ground, pmax, range_m, rstar) < u) return;
        st.acquired = true;
        Refresh(o, v, key, kind, what, pos, range_m, launch_id, until_s, fresh);
    }

    void Refresh(Obs& o, const View& v, std::uint64_t key, SightKind kind, const std::string& what, const Vec3& pos, double range_m,
                 int launch_id, double until_s, std::vector<Sighting>* fresh) {
        (void)until_s;
        const Vec3 d = pos - v.ob->pos;
        const double bearing = AzimuthOf(d);
        const double elev = ElevationOf(d);
        // The error is drawn once per pass (the same believed place while he holds it), not every tick.
        const ObjState& st = o.objs[key];
        const std::uint64_t tag = static_cast<std::uint64_t>(st.pass) * 4;
        const double nb = em::StandardNormal(em::DrawHash(seed_, em::Stream::SightError, v.ob->key, key, 0x1000 + tag + 1));
        const double nr = em::StandardNormal(em::DrawHash(seed_, em::Stream::SightError, v.ob->key, key, 0x1000 + tag + 2));
        const double r_sigma = d_.errors.ground_range_sigma_frac * std::max(st.min_r < 1e17 ? st.min_r : range_m, 500.0);
        Sighting s;
        s.kind = kind;
        s.by = SightBy::Eye;
        s.what = what;
        s.seen_by = v.ob->id;
        s.debug_truth_key = key;
        s.time_s = v.now;
        s.bearing_rad = bearing + nb * d_.errors.ground_bearing_sigma_deg * kPi / 180.0;
        s.elevation_rad = elev;
        s.range_sigma_m = r_sigma;
        s.range_est_m = std::max(50.0, range_m + nr * r_sigma);
        s.believed_enu = v.ob->pos + DirectionFromAzEl(s.bearing_rad, s.elevation_rad) * s.range_est_m;
        s.recognised = range_m <= d_.ground.recognise_good_m;
        s.confidence = s.recognised ? d_.confidence.recognised : d_.confidence.seen;
        s.origin_enu = s.believed_enu;
        s.has_origin = true;
        bool is_new = false;
        const Sighting* ref = Upsert(o, v, std::move(s), &is_new);
        Note(o, v, *ref, is_new, fresh, launch_id);
    }

    // ---- the warning receiver
    // A receiver's contact `key` heard for the first time, on his own receiver or (`borrowed`) on his leader's: if his
    // picture holds the same emitter as heard on the other (its name or class, its bearing within the display's error),
    // that sighting becomes this contact's. A wingman whose receiver is switched off at A1, or back on at A2, so goes on
    // holding what he held, and the change is no news to him (roadmap B12).
    static void Adopt(Obs& o, std::uint64_t key, bool borrowed, const std::string& what, double bearing_rad, double sigma_rad) {
        Sighting* best = nullptr;
        double best_d = std::max(3.0 * sigma_rad, 10.0 * kPi / 180.0);
        for (Sighting& e : o.picture) {
            if (e.by != SightBy::Rwr) continue;
            if (e.debug_truth_key == key) return;
            if (e.borrowed == borrowed || e.what != what) continue;
            const double d = std::fabs(detail::WrapPi(e.bearing_rad - bearing_rad));
            if (d <= best_d) {
                best_d = d;
                best = &e;
            }
        }
        if (best == nullptr) return;
        best->debug_truth_key = key;
        best->borrowed = borrowed;
    }

    void IngestRwr(Obs& o, const View& v, const std::vector<RwrHeard>& heard, std::vector<Sighting>* fresh) {
        const RwrSetDisplay* set = v.ob->rwr_set;
        if (set == nullptr) return;
        // The strongest contacts only: the display's room.
        std::vector<const RwrHeard*> order;
        for (const RwrHeard& h : heard) order.push_back(&h);
        std::stable_sort(order.begin(), order.end(), [](const RwrHeard* a, const RwrHeard* b) {
            if (a->power_dbw != b->power_dbw) return a->power_dbw > b->power_dbw;
            return a->key < b->key;
        });
        if (static_cast<int>(order.size()) > set->max_display) order.resize(static_cast<std::size_t>(set->max_display));
        for (const RwrHeard* h : order) {
            RwrState st = h->state;
            if (st == RwrState::Launch && !set->launch_indication) st = RwrState::Lock;  // the command link reads as a lock
            double bearing = h->bearing_rad;
            double sigma = h->sigma_rad > 0.0 ? h->sigma_rad : set->bearing_sigma_deg * kPi / 180.0;
            if (set->quadrant) {
                const double rel = detail::WrapPi(h->bearing_rad - v.heading_az);
                const double centre = std::round(rel / (kPi / 2.0)) * (kPi / 2.0);
                bearing = v.heading_az + centre;
                sigma = kPi / 4.0 / std::sqrt(3.0);
            }
            double range = 20000.0, range_sigma = 15000.0;
            if (set->range_class) {
                if (h->power_dbw >= set->near_dbw) range = set->near_m;
                else if (h->power_dbw >= set->mid_dbw) range = set->mid_m;
                else range = set->far_m;
                range_sigma = 0.5 * range;
            }
            Sighting s;
            s.kind = SightKind::Emitter;
            s.by = SightBy::Rwr;
            s.rwr = st;
            s.seen_by = v.ob->id;
            s.debug_truth_key = h->key;
            s.time_s = v.now;
            s.bearing_rad = detail::Wrap2Pi(bearing);
            s.elevation_rad = 0.0;
            s.range_est_m = range;
            s.range_sigma_m = range_sigma;
            // A bearing and a class of range: a believed point on the line, never the emitter's place.
            s.believed_enu = v.ob->pos + DirectionFromAzEl(s.bearing_rad, 0.0) * range;
            s.what = set->shows_name && h->known && !h->name.empty() ? h->name
                     : st == RwrState::Launch               ? "missile guidance"
                     : st == RwrState::Lock                 ? "lock"
                                                            : "search";
            s.confidence = d_.confidence.seen;
            // A contact heard on his leader's receiver is the leader's receiver's contact; when he changes from his own
            // receiver to his leader's or back, the contact takes the place of the same emitter's in his picture (Adopt).
            const std::uint64_t key = h->borrowed ? (kBorrowedBit | h->key) : h->key;
            s.debug_truth_key = key;
            s.borrowed = h->borrowed;
            Adopt(o, key, h->borrowed, s.what, s.bearing_rad, sigma);
            // A contact that has grown more serious is new news.
            bool escalated = false;
            for (const Sighting& e : o.picture) {
                if (e.by == SightBy::Rwr && e.debug_truth_key == key && static_cast<int>(st) > static_cast<int>(e.rwr)) escalated = true;
            }
            bool is_new = false;
            const Sighting* ref = Upsert(o, v, std::move(s), &is_new);
            if (is_new || escalated) {
                if (fresh != nullptr) fresh->push_back(*ref);
                o.threat_until_s = std::max(o.threat_until_s, v.now + 10.0);
            }
            // A receiver's bearing is a cue to look: the sector shrinks about it.
            bool merged = false;
            for (Cue& c : o.cues) {
                if (std::fabs(detail::WrapPi(c.bearing_rad - bearing)) < 0.1) {
                    c.until_s = v.now + d_.memory.rwr_s;
                    merged = true;
                }
            }
            if (!merged) o.cues.push_back({detail::Wrap2Pi(bearing), v.now + d_.memory.rwr_s});
        }
        // A false alarm: a search contact from nowhere, now and then.
        if (set->false_alarms_per_h > 0.0) {
            const double p = set->false_alarms_per_h / 3600.0 * (1.0 / std::max(d_.rate.transit_hz, 0.1));
            const std::uint64_t tick = o.tick_index;
            if (em::Uniform01(em::DrawHash(seed_, em::Stream::FalseAlarm, v.ob->key, tick, 0x5A)) < p) {
                Sighting s;
                s.kind = SightKind::Emitter;
                s.by = SightBy::Rwr;
                s.rwr = RwrState::Search;
                s.what = "search";
                s.seen_by = v.ob->id;
                s.debug_truth_key = 0xFA15E000ull + tick;
                s.time_s = v.now;
                s.bearing_rad = em::Uniform01(em::DrawHash(seed_, em::Stream::FalseAlarm, v.ob->key, tick, 0xB0)) * kTwoPi;
                s.range_est_m = set->mid_m;
                s.range_sigma_m = set->mid_m;
                s.believed_enu = v.ob->pos + DirectionFromAzEl(s.bearing_rad, 0.0) * set->mid_m;
                s.confidence = 0.2;
                bool is_new = false;
                const Sighting* ref = Upsert(o, v, std::move(s), &is_new);
                if (is_new && fresh != nullptr) fresh->push_back(*ref);
            }
        }
    }

    double MemoryOf(const Sighting& s) const {
        if (s.by == SightBy::Rwr) return d_.memory.rwr_s;
        switch (s.kind) {
            case SightKind::Aircraft: return d_.memory.aircraft_s;
            case SightKind::Trail: return d_.memory.trail_s;
            case SightKind::LaunchSign: return d_.memory.launch_sign_s;
            case SightKind::Burst: return d_.memory.burst_s;
            case SightKind::Tracer: return d_.memory.tracer_s;
            case SightKind::GroundObject: return d_.memory.ground_object_s;
            case SightKind::Fire: return d_.memory.fire_s;
            case SightKind::Contrail: return d_.memory.contrail_s;
            case SightKind::Emitter: return d_.memory.rwr_s;
        }
        return 20.0;
    }

    void Age(Obs& o, double now_s) {
        o.picture.erase(std::remove_if(o.picture.begin(), o.picture.end(),
                                       [&](const Sighting& s) {
                                           const bool gone = now_s - s.time_s > MemoryOf(s);
                                           return gone;
                                       }),
                        o.picture.end());
        // A forgotten object may be found again as new: its state goes with its sighting; an event that has been pruned
        // takes its state with it (the events' serials run up in the order they were made). The picture's keys are sorted
        // once, not searched again for every object held (a defensive pilot ticks five times a second: cost, 5.7).
        held_keys_.clear();
        for (const Sighting& s : o.picture) held_keys_.push_back(s.debug_truth_key);
        std::sort(held_keys_.begin(), held_keys_.end());
        for (auto it = o.objs.begin(); it != o.objs.end();) {
            const std::uint64_t key = it->first;
            if ((key & kEventBit) != 0) {
                const std::uint64_t serial = key & ~kEventBit;
                const auto at = std::lower_bound(events_.begin(), events_.end(), serial,
                                                 [](const VisibleEvent& e, std::uint64_t v) { return e.serial < v; });
                if (at == events_.end() || at->serial != serial) {
                    it = o.objs.erase(it);
                    continue;
                }
            }
            const bool held = std::binary_search(held_keys_.begin(), held_keys_.end(), key);
            if (!held && it->second.tracked) {
                it->second.tracked = false;
                it->second.miss = 1.0;
            }
            if (!held && it->second.acquired && (key & kGroundBit) == 0 && (key & kEventBit) == 0) it->second.acquired = false;
            ++it;
        }
    }
};

// ------------------------------------------------------------------------------------------------ data

namespace detail {

class PerceptionLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, PerceptionData& out) {
        if (!Keys(root, "", {"perception_id", "status", "notes", "source", "confidence", "eye", "lookout", "blind", "ground", "classes", "trail",
                             "launch", "burst", "tracer", "fire", "errors", "memory", "hz", "confidence_levels"})) {
            return false;
        }
        EyeData& e = out.eye;
        if (!Group(root, "eye",
                   {{"rm_constant_m", &e.rm_constant_m}, {"threshold_offset_pct", &e.threshold_offset_pct},
                    {"factor_confidence", &e.factor_confidence}, {"factor_unknown_location", &e.factor_unknown_location},
                    {"factor_vigilance", &e.factor_vigilance}, {"factor_untrained", &e.factor_untrained},
                    {"factor_ordinary", &e.factor_ordinary}, {"fatigue_vigilance_gain", &e.fatigue_vigilance_gain},
                    {"empty_field_range_factor", &e.empty_field_range_factor}, {"glimpse_s", &e.glimpse_s},
                    {"search_solid_angle_sr", &e.search_solid_angle_sr}, {"cue_sector_deg", &e.cue_sector_deg},
                    {"lobe_max_sr", &e.lobe_max_sr}, {"resolution_rad", &e.resolution_rad}, {"recognise_lines", &e.recognise_lines},
                    {"aircraft_contrast_pct", &e.aircraft_contrast_pct}, {"aircraft_ground_contrast_pct", &e.aircraft_ground_contrast_pct},
                    {"afterburner_contrast_pct", &e.afterburner_contrast_pct}, {"afterburner_area_m2", &e.afterburner_area_m2}, {"twilight_contrast_loss", &e.twilight_contrast_loss},
                    {"contrail_cap_m", &e.contrail_cap_m}, {"contrail_contrast_pct", &e.contrail_contrast_pct},
                    {"contrail_width_m", &e.contrail_width_m}, {"sun_glare_deg", &e.sun_glare_deg},
                    {"sun_glare_contrast_factor", &e.sun_glare_contrast_factor}, {"max_range_m", &e.max_range_m},
                    {"los_sample_m", &e.los_sample_m}, {"los_cache_near_s", &e.los_cache_near_s}, {"los_cache_far_s", &e.los_cache_far_s},
                    {"default_skill", &e.default_skill}, {"area_nose_per_size2", &e.area_nose_per_size2},
                    {"area_beam_per_size2", &e.area_beam_per_size2}, {"area_plan_per_size2", &e.area_plan_per_size2}})) {
            return false;
        }
        LookoutData& l = out.lookout;
        if (!Group(root, "lookout",
                   {{"transit", &l.transit}, {"low_level", &l.low_level}, {"attack", &l.attack}, {"defensive", &l.defensive},
                    {"second_seat_extra", &l.second_seat_extra}, {"low_level_agl_m", &l.low_level_agl_m},
                    {"check_six_share", &l.check_six_share}})) {
            return false;
        }
        if (!Group(root, "blind", {{"rear_half_deg", &l.rear_half_deg}})) return false;
        GroundData& g = out.ground;
        if (!Group(root, "ground",
                   {{"pmax_small_low", &g.pmax[0][0]}, {"pmax_small_medium", &g.pmax[0][1]}, {"pmax_small_high", &g.pmax[0][2]},
                    {"pmax_medium_low", &g.pmax[1][0]}, {"pmax_medium_medium", &g.pmax[1][1]}, {"pmax_medium_high", &g.pmax[1][2]},
                    {"pmax_large_low", &g.pmax[2][0]}, {"pmax_large_medium", &g.pmax[2][1]}, {"pmax_large_high", &g.pmax[2][2]},
                    {"zero_at_rstar", &g.zero_at_rstar}, {"at_0625", &g.at_0625}, {"at_05", &g.at_05}, {"at_0375", &g.at_0375},
                    {"full_at_rstar", &g.full_at_rstar}, {"off_angle_loss_per_deg", &g.off_angle_loss_per_deg},
                    {"off_track_limit_deg", &g.off_track_limit_deg}, {"unbriefed_scale", &g.unbriefed_scale},
                    {"pass_gap_factor", &g.pass_gap_factor}, {"pass_gap_m", &g.pass_gap_m}, {"recognise_good_m", &g.recognise_good_m},
                    {"recognise_poor_m", &g.recognise_poor_m}, {"max_range_m", &g.max_range_m}})) {
            return false;
        }
        if (!Classes(root, out)) return false;
        TrailData& t = out.trail;
        if (!Group(root, "trail",
                   {{"contrast_pct", &t.contrast_pct}, {"width_m", &t.width_m}, {"max_length_m", &t.max_length_m},
                    {"point_gap_m", &t.point_gap_m}, {"max_points", &t.max_points}, {"tracer_night_range_m", &t.tracer_night_range_m},
                    {"flame_night_range_m", &t.flame_night_range_m}, {"night_area_m2", &t.night_area_m2},
                    {"night_contrast_pct", &t.night_contrast_pct}})) {
            return false;
        }
        if (!Group(root, "launch", {{"sign_life_s", &out.launch_sign_life_s}, {"flash_night_range_m", &out.launch_flash_night_range_m}})) return false;
        BurstData& b = out.burst;
        if (!Group(root, "burst",
                   {{"life_s", &b.life_s}, {"area_m2", &b.area_m2}, {"contrast_pct", &b.contrast_pct}, {"flash_life_s", &b.flash_life_s},
                    {"flash_area_m2", &b.flash_area_m2}, {"flash_contrast_pct", &b.flash_contrast_pct}, {"night_range_m", &b.night_range_m}})) {
            return false;
        }
        TracerData& tr = out.tracer;
        if (!Group(root, "tracer", {{"life_s", &tr.life_s}, {"area_m2", &tr.area_m2}, {"contrast_pct", &tr.contrast_pct}, {"night_range_m", &tr.night_range_m}})) {
            return false;
        }
        FireData& f = out.fire;
        if (!Group(root, "fire", {{"life_s", &f.life_s}, {"area_m2", &f.area_m2}, {"contrast_pct", &f.contrast_pct}, {"night_area_m2", &f.night_area_m2}})) {
            return false;
        }
        ErrorData& er = out.errors;
        if (!Group(root, "errors",
                   {{"bearing_sigma_deg", &er.bearing_sigma_deg}, {"elevation_sigma_deg", &er.elevation_sigma_deg},
                    {"range_sigma_frac", &er.range_sigma_frac}, {"ground_bearing_sigma_deg", &er.ground_bearing_sigma_deg},
                    {"ground_range_sigma_frac", &er.ground_range_sigma_frac}})) {
            return false;
        }
        MemoryData& m = out.memory;
        if (!Group(root, "memory",
                   {{"aircraft_s", &m.aircraft_s}, {"trail_s", &m.trail_s}, {"launch_sign_s", &m.launch_sign_s}, {"burst_s", &m.burst_s},
                    {"tracer_s", &m.tracer_s}, {"ground_object_s", &m.ground_object_s}, {"fire_s", &m.fire_s}, {"contrail_s", &m.contrail_s},
                    {"rwr_s", &m.rwr_s}, {"event_keep_s", &m.event_keep_s}})) {
            return false;
        }
        if (!Group(root, "hz", {{"transit", &out.rate.transit_hz}, {"threat", &out.rate.threat_hz}, {"defensive", &out.rate.defensive_hz}})) return false;
        ConfidenceData& c = out.confidence;
        if (!Group(root, "confidence_levels", {{"seen", &c.seen}, {"watched", &c.watched}, {"recognised", &c.recognised}, {"two_senses", &c.two_senses}})) {
            return false;
        }
        if (out.rate.transit_hz <= 0.0 || out.rate.threat_hz <= 0.0 || out.rate.defensive_hz <= 0.0) return Fail("hz", "must be > 0");
        if (out.eye.glimpse_s <= 0.0) return Fail("eye.glimpse_s", "must be > 0");
        return true;
    }

    // data/air_war/sensors.json's "rwr_sets".
    bool LoadRwrSets(const json::Value& root, PerceptionData& out) {
        const json::Value* arr = root.Find("rwr_sets");
        if (arr == nullptr) return true;
        if (!arr->Is(json::Type::Array)) return Fail("rwr_sets", "expected a list");
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& v = arr->items[i];
            const std::string p = Index("rwr_sets", i);
            if (!Keys(v, p, {"em_esm", "name", "shows_name", "bearing", "range_class", "launch_indication", "bearing_sigma_deg",
                             "false_alarms_per_h", "max_display", "near_dbw", "mid_dbw", "near_m", "mid_m", "far_m", "source", "confidence", "notes"})) {
                return false;
            }
            RwrSetDisplay s;
            if (!RequireString(v, p, "em_esm", s.em_esm) || !OptionalString(v, p, "name", s.name) || !OptionalString(v, p, "source", s.source) ||
                !OptionalString(v, p, "confidence", s.confidence) || !OptionalBool(v, p, "shows_name", s.shows_name) ||
                !OptionalBool(v, p, "range_class", s.range_class) || !OptionalBool(v, p, "launch_indication", s.launch_indication) ||
                !OptionalNumber(v, p, "bearing_sigma_deg", s.bearing_sigma_deg) || !OptionalNumber(v, p, "false_alarms_per_h", s.false_alarms_per_h) ||
                !OptionalNumber(v, p, "near_dbw", s.near_dbw) || !OptionalNumber(v, p, "mid_dbw", s.mid_dbw) ||
                !OptionalNumber(v, p, "near_m", s.near_m) || !OptionalNumber(v, p, "mid_m", s.mid_m) || !OptionalNumber(v, p, "far_m", s.far_m)) {
                return false;
            }
            double md = static_cast<double>(s.max_display);
            if (!OptionalNumber(v, p, "max_display", md)) return false;
            s.max_display = std::max(1, static_cast<int>(md));
            std::string bearing = "measured";
            if (!OptionalString(v, p, "bearing", bearing)) return false;
            if (bearing != "measured" && bearing != "quadrant") return Fail(Join(p, "bearing"), "\"measured\" or \"quadrant\"");
            s.quadrant = bearing == "quadrant";
            if (s.source.empty() || s.confidence.empty()) return Fail(p, "needs a source and a confidence");
            out.rwr_sets.push_back(s);
        }
        return true;
    }

private:
    bool Group(const json::Value& root, const char* name, std::initializer_list<std::pair<const char*, double*>> each) {
        const json::Value* g = root.Find(name);
        if (g == nullptr) return Fail(name, "required");
        if (!g->Is(json::Type::Object)) return Fail(name, "expected an object");
        for (const std::string& k : g->keys) {
            bool known = false;
            for (const auto& e : each) known = known || k == e.first;
            if (!known) return Fail(Join(name, k), "unknown key");
        }
        for (const auto& e : each) {
            const std::string p = Join(name, e.first);
            const json::Value* v = g->Find(e.first);
            if (v == nullptr) return Fail(p, "required");
            if (!Value(*v, p, *e.second)) return false;
        }
        return true;
    }
    bool Value(const json::Value& v, const std::string& p, double& out) {
        if (!Keys(v, p, {"value", "source", "confidence", "notes"})) return false;
        const json::Value* n = v.Find("value");
        if (n == nullptr || !n->Is(json::Type::Number)) return Fail(Join(p, "value"), "a number");
        std::string src, conf;
        if (!RequireString(v, p, "source", src) || !RequireString(v, p, "confidence", conf)) return false;
        out = n->number;
        return true;
    }
    bool Classes(const json::Value& root, PerceptionData& out) {
        const json::Value* g = root.Find("classes");
        if (g == nullptr || !g->Is(json::Type::Object)) return Fail("classes", "required, an object");
        out.classes.clear();
        for (const std::string& k : g->keys) {
            const json::Value* v = g->Find(k);
            const std::string p = Join("classes", k);
            if (!Keys(*v, p, {"pattern", "contrast", "area_m2", "source", "confidence"})) return false;
            std::string pat, con, src, conf;
            if (!RequireString(*v, p, "pattern", pat) || !RequireString(*v, p, "contrast", con) || !RequireString(*v, p, "source", src) ||
                !RequireString(*v, p, "confidence", conf)) {
                return false;
            }
            GroundClassDef c;
            if (pat == "small") c.pattern = Pattern::Small;
            else if (pat == "medium") c.pattern = Pattern::Medium;
            else if (pat == "large") c.pattern = Pattern::Large;
            else return Fail(Join(p, "pattern"), "small, medium or large");
            if (con == "low") c.contrast = GroundContrast::Low;
            else if (con == "medium") c.contrast = GroundContrast::Medium;
            else if (con == "high") c.contrast = GroundContrast::High;
            else return Fail(Join(p, "contrast"), "low, medium or high");
            const json::Value* a = v->Find("area_m2");
            if (a == nullptr || !Value(*a, Join(p, "area_m2"), c.area_m2)) return false;
            out.classes[k] = c;
        }
        return true;
    }
};

inline bool ReadAll(const std::string& path, std::string& text) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    text = ss.str();
    return true;
}

}  // namespace detail

// data/gameplay/perception.json and the receivers' displays in data/air_war/sensors.json. False with the reason in `error`
// when a file is there but wrong; a missing file leaves the code's defaults.
inline bool LoadPerceptionData(const std::string& data_dir, PerceptionData& out, std::string* error = nullptr) {
    PerceptionData d;
    std::string text;
    if (detail::ReadAll(data_dir + "/gameplay/perception.json", text)) {
        json::Value root;
        std::string e;
        if (!json::Parse(text, root, &e)) {
            if (error != nullptr) *error = "perception: " + e;
            return false;
        }
        detail::PerceptionLoader loader;
        if (!loader.Load(root, d)) {
            if (error != nullptr) *error = "perception: " + loader.Message();
            return false;
        }
    }
    if (detail::ReadAll(data_dir + "/air_war/sensors.json", text)) {
        json::Value root;
        std::string e;
        if (!json::Parse(text, root, &e)) {
            if (error != nullptr) *error = "perception sensors: " + e;
            return false;
        }
        detail::PerceptionLoader loader;
        if (!loader.LoadRwrSets(root, d)) {
            if (error != nullptr) *error = "perception rwr_sets: " + loader.Message();
            return false;
        }
    }
    out = std::move(d);
    return true;
}

}  // namespace airwar
}  // namespace simcore
