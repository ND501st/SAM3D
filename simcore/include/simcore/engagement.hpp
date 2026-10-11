// simcore/engagement.hpp — gameplay-level sensors and engagement outcome.
//
// This is deliberately NOT a physics model of radars or missiles. It decides, from
// the console's state and simple geometry, what the operators would see and whether
// an engagement succeeds, using transparent, tunable game-balance numbers kept in
// data/gameplay/<system>.json:
//
//   * Search radar picture: contacts inside the selected range scale, with line of
//     sight, and either above the clutter height or with MTI on (within its range).
//   * Tracking radar: locks when its acquisition mode is on and the operator has put
//     the azimuth line and range gate on a target, and (with an elevation search) the
//     antenna's beam is on it in elevation: during target designation the antenna scans
//     the search beam the target was found in, otherwise it points where the elevation
//     wheel is. TV tracking locks when its mode is
//     on and the target is inside the lens's field of view. Once locked it follows the
//     target (drives the wheels) until line of sight, altitude, crossing rate or the
//     equipment breaks the lock. It sets the console's capture / too-low / zone inputs.
//   * Crew aiming: each wheel has its own seat (azimuth, elevation, range); the crew
//     member in that seat turns it onto the nearest search-radar contact (or the
//     plotting-board track when the radars are off); elevation by the TV, or by the
//     elevation-search indicator once the target shows on it.
//   * Outcome: a launch starts a shot with a flight time from the game's pacing speed.
//     Guidance is by command from the ground, so losing the track loses the missile.
//     At the end, the chance of a hit is the product of plain factors — target class,
//     tracking method, in the zone at launch, target manoeuvre, crew mistakes — and one
//     draw from the scenario's random stream decides it. The debrief shows every factor.
//
//   * Heading: azimuths the crew sees and sets are in the vehicle's heading-reference frame
//     (a gyro half-compass, ConsoleData::orientation). It follows the hull's turns but only
//     knows north once the crew has oriented it; until then the picture is rotated by its
//     error, and designations given in true azimuth (the plotting board) point wrong.
//     Without an orientation block the frame is true north.
//   * IFF: an interrogation makes friendly aircraft reply on the search scope.
//
// With an EmSensorLink (em_link.hpp; the scenario's "em" block) the search picture and the radar lock come from
// the electromagnetic layer instead of the rules above: the search scope shows the search radar's tracks, and
// the tracking radar is pointed at the wheels' designation and locks on a detected echo in its beam and gate.
// TV tracking, IFF, the zone lamps and the outcome stay as described.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/acmi.hpp"
#include "simcore/console_data.hpp"
#include "simcore/debrief.hpp"
#include "simcore/em_link.hpp"
#include "simcore/radar.hpp"
#include "simcore/world.hpp"

namespace simcore {

// Game-balance numbers. Every field is loaded from data/gameplay/<system>.json.
struct EngagementTuning {
    std::string system_id;
    // Firing zone, for the ЦЕЛЬ В ЗОНЕ / ВНИМАНИЕ lamps and the outcome's zone factor.
    double zone_min_range_m = 0.0;
    double zone_max_range_m = 0.0;
    double zone_min_alt_m = 0.0;
    double zone_max_alt_m = 0.0;
    double zone_warning_s = 0.0;
    // Search radar.
    double clutter_altitude_m = 0.0;
    // Tracking.
    double az_tolerance_deg = 0.0;
    double range_gate_m = 0.0;
    double lock_time_s = 0.0;
    double track_min_altitude_m = 0.0;
    double tv_max_range_m = 0.0;
    double max_angular_rate_deg_s = 0.0;
    // Crew aiming: the crew role (console station) that turns each wheel.
    std::string aim_az_role;
    std::string aim_el_role;
    std::string aim_range_role;
    double aim_az_rate_deg_s = 0.0;
    double aim_el_rate_deg_s = 0.0;
    double aim_range_rate_mps = 0.0;
    double plotting_board_range_m = 0.0;
    // Missile pacing (not a flight model).
    double missile_average_speed_mps = 0.0;
    double missile_max_flight_time_s = 0.0;
    // Outcome.
    std::map<std::string, double> hit_chance;  // by target class; "default" required
    double tv_factor = 1.0;
    double outside_zone_factor = 1.0;
    double manoeuvre_penalty_per_deg_s = 0.0;
    double manoeuvre_factor_min = 1.0;
    double deviation_penalty = 0.0;
    double deviation_factor_min = 1.0;

    double HitChanceFor(const std::string& target_class) const {
        const auto it = hit_chance.find(target_class);
        return it != hit_chance.end() ? it->second : hit_chance.at("default");
    }
};

namespace detail {

class TuningLoader : public ConsoleLoader {
public:
    bool LoadTuning(const json::Value& root, EngagementTuning& t) {
        if (!Keys(root, "", {"system_id", "status", "notes", "zone", "search", "track", "aim", "missile", "outcome"}) ||
            !RequireString(root, "", "system_id", t.system_id)) {
            return false;
        }
        std::string status;
        if (!RequireString(root, "", "status", status)) return false;
        const json::Value* g = nullptr;
        if (!Group(root, "zone", {"min_range_m", "max_range_m", "min_altitude_m", "max_altitude_m", "warning_s"}, g) ||
            !Num(*g, "zone", "min_range_m", t.zone_min_range_m, 0.0) ||
            !Num(*g, "zone", "max_range_m", t.zone_max_range_m, t.zone_min_range_m + 1.0) ||
            !Num(*g, "zone", "min_altitude_m", t.zone_min_alt_m, 0.0) ||
            !Num(*g, "zone", "max_altitude_m", t.zone_max_alt_m, t.zone_min_alt_m + 1.0) ||
            !Num(*g, "zone", "warning_s", t.zone_warning_s, 0.0)) {
            return false;
        }
        if (!Group(root, "search", {"clutter_altitude_m"}, g) ||
            !Num(*g, "search", "clutter_altitude_m", t.clutter_altitude_m, 0.0)) {
            return false;
        }
        if (!Group(root, "track",
                   {"az_tolerance_deg", "range_gate_m", "lock_time_s", "min_altitude_m", "tv_max_range_m",
                    "max_angular_rate_deg_s"},
                   g) ||
            !Num(*g, "track", "az_tolerance_deg", t.az_tolerance_deg, 0.01) ||
            !Num(*g, "track", "range_gate_m", t.range_gate_m, 1.0) ||
            !Num(*g, "track", "lock_time_s", t.lock_time_s, 0.0) ||
            !Num(*g, "track", "min_altitude_m", t.track_min_altitude_m, 0.0) ||
            !Num(*g, "track", "tv_max_range_m", t.tv_max_range_m, 1.0) ||
            !Num(*g, "track", "max_angular_rate_deg_s", t.max_angular_rate_deg_s, 0.1)) {
            return false;
        }
        if (!Group(root, "aim", {"azimuth_role", "elevation_role", "range_role", "az_rate_deg_s", "el_rate_deg_s",
                                 "range_rate_mps", "plotting_board_range_m"},
                   g) ||
            !RequireString(*g, "aim", "azimuth_role", t.aim_az_role) ||
            !RequireString(*g, "aim", "elevation_role", t.aim_el_role) ||
            !RequireString(*g, "aim", "range_role", t.aim_range_role) ||
            !Num(*g, "aim", "az_rate_deg_s", t.aim_az_rate_deg_s, 0.1) ||
            !Num(*g, "aim", "el_rate_deg_s", t.aim_el_rate_deg_s, 0.1) ||
            !Num(*g, "aim", "range_rate_mps", t.aim_range_rate_mps, 1.0) ||
            !Num(*g, "aim", "plotting_board_range_m", t.plotting_board_range_m, 0.0)) {
            return false;
        }
        if (!Group(root, "missile", {"average_speed_mps", "max_flight_time_s"}, g) ||
            !Num(*g, "missile", "average_speed_mps", t.missile_average_speed_mps, 1.0) ||
            !Num(*g, "missile", "max_flight_time_s", t.missile_max_flight_time_s, 0.1)) {
            return false;
        }
        if (!Group(root, "outcome",
                   {"hit_chance", "tv_factor", "outside_zone_factor", "manoeuvre_penalty_per_deg_s",
                    "manoeuvre_factor_min", "deviation_penalty", "deviation_factor_min"},
                   g) ||
            !Factor(*g, "tv_factor", t.tv_factor) || !Factor(*g, "outside_zone_factor", t.outside_zone_factor) ||
            !Num(*g, "outcome", "manoeuvre_penalty_per_deg_s", t.manoeuvre_penalty_per_deg_s, 0.0) ||
            !Factor(*g, "manoeuvre_factor_min", t.manoeuvre_factor_min) ||
            !Num(*g, "outcome", "deviation_penalty", t.deviation_penalty, 0.0) ||
            !Factor(*g, "deviation_factor_min", t.deviation_factor_min)) {
            return false;
        }
        const json::Value* hc = g->Find("hit_chance");
        if (hc == nullptr || !hc->Is(json::Type::Object)) {
            return Fail("outcome.hit_chance", "expected an object of class: chance");
        }
        for (std::size_t i = 0; i < hc->keys.size(); ++i) {
            const json::Value& v = hc->items[i];
            if (!v.Is(json::Type::Number) || v.number < 0.0 || v.number > 1.0) {
                return Fail(Join("outcome.hit_chance", hc->keys[i]), "expected a number from 0 to 1");
            }
            t.hit_chance[hc->keys[i]] = v.number;
        }
        if (t.hit_chance.count("default") == 0) return Fail("outcome.hit_chance", "needs a \"default\" entry");
        return true;
    }

private:
    bool Group(const json::Value& root, const char* key, std::initializer_list<const char*> fields,
               const json::Value*& out) {
        out = root.Find(key);
        if (out == nullptr) return Fail(key, "required");
        if (!out->Is(json::Type::Object)) return Fail(key, "expected an object");
        for (const std::string& k : out->keys) {
            bool ok = k == "source" || k == "notes";
            for (const char* f : fields) ok = ok || k == f;
            if (!ok) return Fail(Join(key, k), "unknown key");
        }
        return true;
    }

    bool Num(const json::Value& g, const char* group, const char* key, double& out, double minimum) {
        if (g.Find(key) == nullptr) return Fail(Join(group, key), "required");
        if (!OptionalNumber(g, group, key, out)) return false;
        if (out < minimum) return Fail(Join(group, key), "must be >= " + std::to_string(minimum));
        return true;
    }

    bool Factor(const json::Value& g, const char* key, double& out) {
        if (!Num(g, "outcome", key, out, 0.0)) return false;
        if (out > 1.0) return Fail(Join("outcome", key), "must be from 0 to 1");
        return true;
    }
};

}  // namespace detail

inline bool LoadEngagementTuning(const std::string& json_text, EngagementTuning& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    EngagementTuning t;
    detail::TuningLoader loader;
    if (!loader.LoadTuning(root, t)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(t);
    return true;
}

inline bool LoadEngagementTuningFile(const std::string& path, EngagementTuning& out, std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    if (!LoadEngagementTuning(text.str(), out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

// A target as the engagement model sees it.
struct EngagementTarget {
    std::string id;
    std::string target_class = "default";
    int world_id = -1;  // -1 until it appears
    bool evade_on_launch = false;
    double evade_turn_rate_rad_s = 0.0;
    bool friendly = false;  // replies to IFF interrogation
};

enum class TrackMethod { None, Radar, Tv };

inline const char* ToString(TrackMethod m) {
    switch (m) {
        case TrackMethod::None: return "none";
        case TrackMethod::Radar: return "radar";
        case TrackMethod::Tv: return "TV";
    }
    return "?";
}

struct Contact {
    std::string target_id;
    double range_m = 0.0;
    double azimuth_deg = 0.0;  // -180..180 in the vehicle's heading-reference frame (true north once oriented)
    double elevation_deg = 0.0;
    bool control_reply = false;  // IFF replies only: to a control interrogation (ЗАПРОС К)
};

// Full width of the TV sight's field of view for the lens selected now (0 if the console has no TV).
inline double TvFieldOfViewDeg(const ConsoleData& console) {
    const TrackSensorDef& d = console.track;
    if (d.tv_camera.empty()) return 0.0;
    const bool narrow = !d.fov_control.empty() && console.panel.Get(d.fov_control) == 1;
    return narrow ? d.fov_narrow_deg : d.fov_wide_deg;
}

// One number from [0, 1) drawn from a std::mt19937, the same on every platform.
inline double Uniform01(std::mt19937& rng) { return static_cast<double>(rng()) / 4294967296.0; }

// Which aiming seats have an NPC in them. A seat the player sits in (or nobody) is left alone.
struct AimCrew {
    bool azimuth = false;
    bool elevation = false;
    bool range = false;
};

class EngagementModel {
public:
    EngagementModel(EngagementTuning tuning, Vec3 unit_position)
        : tuning_(std::move(tuning)), unit_(unit_position) {}

    // One step, after the World has moved the targets and before triggers and equipment run.
    void Step(double t, double dt, World& world, ConsoleData& console, std::vector<EngagementTarget>& targets,
              SessionLog& log, std::mt19937& rng, const AimCrew& aim_crew) {
        if (!console.has_sensors) return;
        UpdateOrientation(t, dt, console, log, aim_crew.elevation);
        Measure(dt, world, targets);
        ResolveShots(t, world, targets, log, rng);
        if (em_ != nullptr) StepEm(dt, world, console, targets);
        UpdateSearch(t, console, log);
        UpdateIff(t, console, log);
        if (method_ == TrackMethod::None) Aim(dt, console, aim_crew);
        UpdateTrack(t, dt, console, log);
        UpdateTrackReturns(console);
        SetInputs(console);
    }

    // Call when the launch button fires with the circuit armed.
    void OnLaunch(double t, int number, World& world, std::vector<EngagementTarget>& targets, SessionLog& log) {
        Shot s;
        s.number = number;
        s.launch_t = t;
        s.launch_pos = unit_;
        const std::string who = "Missile " + std::to_string(number);
        if (method_ == TrackMethod::None) {
            s.done = true;
            ++misses_;
            log.Record({t, DebriefEntry::Kind::Miss, who, "-", "launched with no target track: unguided"});
            shots_.push_back(s);
            return;
        }
        const Geometry& g = geo_.at(tracked_);
        if (g.friendly) log.Record({t, DebriefEntry::Kind::Deviation, who, tracked_, "fired at a friendly aircraft"});
        s.target_id = tracked_;
        s.method = method_;
        s.in_zone = InZone(g);
        s.flight_time_s = g.range_m / tuning_.missile_average_speed_mps;
        s.out_of_reach = s.flight_time_s > tuning_.missile_max_flight_time_s;
        if (s.out_of_reach) s.flight_time_s = tuning_.missile_max_flight_time_s;
        char detail[160];
        std::snprintf(detail, sizeof(detail), "guided at %s, %.1f km, %s track%s", tracked_.c_str(), g.range_m / 1000.0,
                      ToString(method_), s.in_zone ? ", in the zone" : ", OUTSIDE the zone");
        log.Record({t, DebriefEntry::Kind::Scenario, who, tracked_, detail});
        shots_.push_back(s);
        for (EngagementTarget& tg : targets) {
            if (tg.id == tracked_ && tg.evade_on_launch) Evade(world, tg, t, log);
        }
    }

    // Missiles in flight for display: an illustrative straight path from the launch point to the target,
    // advanced by the fraction of the flight time elapsed. Not a flight path.
    struct MissileView {
        int number = 0;
        Vec3 position;
        Vec3 velocity;
    };

    std::vector<MissileView> MissilesInFlight(double t, World& world, const std::vector<EngagementTarget>& targets) const {
        std::vector<MissileView> out;
        for (const Shot& s : shots_) {
            if (s.done) continue;
            const Aircraft* a = TargetAircraft(world, targets, s.target_id);
            if (a == nullptr) continue;
            const double f = Clamp((t - s.launch_t) / s.flight_time_s, 0.0, 1.0);
            const Vec3 path = a->position - s.launch_pos;
            out.push_back({s.number, s.launch_pos + path * f, path / s.flight_time_s});
        }
        return out;
    }

    // Tacview: an illustrative missile path (straight line from the unit to the target), not a flight path.
    void WriteAcmi(AcmiWriter& acmi, double t, World& world, const std::vector<EngagementTarget>& targets) {
        for (Shot& s : shots_) {
            const int id = kMissileAcmiBase + s.number;
            if (s.done) {
                if (s.acmi_shown) acmi.RemoveObject(id);
                s.acmi_shown = false;
                continue;
            }
            const Aircraft* a = TargetAircraft(world, targets, s.target_id);
            if (a == nullptr) continue;
            const double f = Clamp((t - s.launch_t) / s.flight_time_s, 0.0, 1.0);
            const Vec3 pos = s.launch_pos + (a->position - s.launch_pos) * f;
            acmi.WriteObject(id, pos, a->position - s.launch_pos,
                             "Type=Weapon+Missile,Name=Missile " + std::to_string(s.number) +
                                 " (illustrative path),Color=Red");
            s.acmi_shown = true;
        }
    }

    // Use the electromagnetic layer for the search picture and the radar lock (null = the gameplay rules). The link
    // must outlive the model.
    void SetEmLink(EmSensorLink* link) { em_ = link; }
    bool EmActive() const { return em_ != nullptr; }

    // The unit moved (driven by the player or an AI driver).
    void SetUnitPosition(const Vec3& unit_position) { unit_ = unit_position; }
    // The hull's true heading, degrees clockwise from north (driven by the player or the scenario).
    void SetUnitHeading(double heading_deg) { unit_heading_deg_ = heading_deg; }
    double UnitHeadingDeg() const { return unit_heading_deg_; }
    // The heading the gyro holds (its idea of the hull's heading), and the frame conversion: an azimuth in the
    // crew's frame (wheels, scope) to true azimuth from north.
    double GyroHeadingDeg() const { return gyro_heading_deg_; }
    double IndicatedToTrueDeg(double indicated_deg) const {
        return Wrap180(indicated_deg + unit_heading_deg_ - gyro_heading_deg_);
    }
    // IFF replies painted on the search scope this step.
    const std::vector<Contact>& IffReplies() const { return iff_replies_; }
    const Vec3& UnitPosition() const { return unit_; }

    const std::vector<Contact>& SearchPicture() const { return picture_; }
    // Echoes on the tracking radar's range indicator: targets inside its beam (the azimuth line, within the
    // tracking tolerance) up to its displayed range, with line of sight, while it radiates. Low targets still show:
    // the radar measures their range even when it can't hold them in angle.
    const std::vector<Contact>& TrackReturns() const { return track_returns_; }
    TrackMethod Method() const { return method_; }
    // The tracking antenna's elevation now, the elevations its beam centre passed over in the last step (a span
    // while it scans during target designation), and the search beam it searches (-1 if the console has no
    // elevation search).
    double AntennaElevationDeg() const { return antenna_el_deg_; }
    double SweptElevationLowDeg() const { return swept_low_deg_; }
    double SweptElevationHighDeg() const { return swept_high_deg_; }
    int ElevationBeam() const { return el_beam_; }
    const std::string& TrackedTarget() const { return tracked_; }
    const EngagementTuning& Tuning() const { return tuning_; }
    int Hits() const { return hits_; }
    int Misses() const { return misses_; }
    bool ShotsInFlight() const {
        for (const Shot& s : shots_) {
            if (!s.done) return true;
        }
        return false;
    }

private:
    static constexpr int kMissileAcmiBase = 0x7F000;

    struct Geometry {
        bool alive = false;
        double range_m = 0.0;
        double az_deg = 0.0;
        double el_deg = 0.0;
        double agl_m = 0.0;
        double true_az_deg = 0.0;  // from north; az_deg is in the crew's frame
        bool friendly = false;
        double closing_mps = 0.0;
        double angular_rate_deg_s = 0.0;
        double turn_rate_deg_s = 0.0;
        bool los = false;
        Vec3 prev_velocity;
        Vec3 prev_los_dir;
        bool has_prev = false;
        std::string target_class;
    };

    struct Shot {
        int number = 0;
        std::string target_id;
        double launch_t = 0.0;
        double flight_time_s = 0.0;
        Vec3 launch_pos;
        TrackMethod method = TrackMethod::None;
        bool in_zone = false;
        bool out_of_reach = false;
        bool done = false;
        bool acmi_shown = false;
        double max_turn_deg_s = 0.0;
    };

    static double Wrap180(double deg) {
        double d = std::fmod(deg + 180.0, 360.0);
        if (d < 0.0) d += 360.0;
        return d - 180.0;
    }

    // The target azimuth expressed within the wheel's limits, nearest to where the wheel is now.
    static double WheelAzimuth(double az_deg, double wheel_deg, double limit_deg) {
        const double base = Wrap180(az_deg);
        double best = base;
        double best_err = 1e9;
        for (double c : {base - 360.0, base, base + 360.0}) {
            if (std::fabs(c) > limit_deg) continue;
            const double err = std::fabs(c - wheel_deg);
            if (err < best_err) {
                best = c;
                best_err = err;
            }
        }
        return best;
    }

    static Aircraft* TargetAircraft(World& world, const std::vector<EngagementTarget>& targets, const std::string& id) {
        for (const EngagementTarget& t : targets) {
            if (t.id == id && t.world_id >= 0) return world.FindAircraft(t.world_id);
        }
        return nullptr;
    }

    void Measure(double dt, World& world, const std::vector<EngagementTarget>& targets) {
        const LocalFrame& frame = world.Frame();
        const Terrain& terrain = world.GetTerrain();
        const double unit_alt = frame.AltitudeOf(unit_);
        for (const EngagementTarget& tg : targets) {
            Geometry& g = geo_[tg.id];
            g.target_class = tg.target_class;
            g.friendly = tg.friendly;
            const Aircraft* a = tg.world_id >= 0 ? world.FindAircraft(tg.world_id) : nullptr;
            g.alive = a != nullptr && a->alive;
            if (!g.alive) continue;
            const Vec3 los = a->position - unit_;
            const double alt = frame.AltitudeOf(a->position);
            g.range_m = los.Length();
            g.true_az_deg = Wrap180(RadToDeg(AzimuthOf(los)));
            g.az_deg = Wrap180(g.true_az_deg - unit_heading_deg_ + gyro_heading_deg_);
            g.el_deg = RadToDeg(ElevationOf(los));
            g.agl_m = alt - terrain.HeightAt(a->position.x, a->position.y);
            g.los = HasLineOfSight(terrain, unit_, unit_alt, a->position, alt, 100.0);
            const Vec3 dir = los.Normalized();
            g.closing_mps = -Dot(a->velocity, dir);
            if (g.has_prev && dt > 0.0) {
                g.angular_rate_deg_s = RadToDeg(AngleBetween(g.prev_los_dir, dir)) / dt;
                g.turn_rate_deg_s = RadToDeg(AngleBetween(g.prev_velocity, a->velocity)) / dt;
            }
            g.prev_los_dir = dir;
            g.prev_velocity = a->velocity;
            g.has_prev = true;
        }
    }

    // Hand the EM layer this step's world: where the unit and the targets are, which radars radiate and whether the
    // search radar's MTI is selected; then let it run to now.
    void StepEm(double dt, World& world, const ConsoleData& console, const std::vector<EngagementTarget>& targets) {
        em_->SetUnit(unit_);
        for (const EngagementTarget& tg : targets) {
            if (tg.world_id < 0) continue;
            const Aircraft* a = world.FindAircraft(tg.world_id);
            if (a != nullptr) em_->SyncTarget(tg.id, a->alive, a->position, a->velocity, a->rcs_m2, tg.friendly);
        }
        const SearchSensorDef& s = console.search;
        const Equipment& eq = console.equipment;
        const bool mti = !s.mti_control.empty() && console.panel.Get(s.mti_control) == 1;
        em_->SetSearch(eq.IsReady(s.antenna) && eq.IsReady(s.transmitter), mti);
        em_->SetTrack(eq.IsReady(console.track.transmitter));
        em_->Advance(dt);
    }

    bool InZone(const Geometry& g) const {
        return g.range_m >= tuning_.zone_min_range_m && g.range_m <= tuning_.zone_max_range_m &&
               g.agl_m >= tuning_.zone_min_alt_m && g.agl_m <= tuning_.zone_max_alt_m;
    }

    static int Mistakes(const SessionLog& log) {
        const DebriefSummary s = log.Summary();
        return s.early_actions + s.out_of_order_actions;
    }

    void ResolveShots(double t, World& world, const std::vector<EngagementTarget>& targets, SessionLog& log,
                      std::mt19937& rng) {
        for (Shot& s : shots_) {
            if (s.done) continue;
            const std::string who = "Missile " + std::to_string(s.number);
            const auto it = geo_.find(s.target_id);
            if (it == geo_.end() || !it->second.alive) {
                Finish(s, false);
                log.Record({t, DebriefEntry::Kind::Miss, who, s.target_id, "target already down"});
                continue;
            }
            const Geometry& g = it->second;
            if (method_ == TrackMethod::None || tracked_ != s.target_id) {
                Finish(s, false);
                log.Record({t, DebriefEntry::Kind::Miss, who, s.target_id, "track lost: no guidance commands"});
                continue;
            }
            if (g.turn_rate_deg_s > s.max_turn_deg_s) s.max_turn_deg_s = g.turn_rate_deg_s;
            if (t + 1e-9 < s.launch_t + s.flight_time_s) continue;
            if (s.out_of_reach) {
                Finish(s, false);
                log.Record({t, DebriefEntry::Kind::Miss, who, s.target_id, "out of reach: flight time ran out"});
                continue;
            }
            const double base = tuning_.HitChanceFor(g.target_class);
            const double method = s.method == TrackMethod::Tv ? tuning_.tv_factor : 1.0;
            const double zone = s.in_zone ? 1.0 : tuning_.outside_zone_factor;
            const double manoeuvre =
                std::fmax(tuning_.manoeuvre_factor_min, 1.0 - tuning_.manoeuvre_penalty_per_deg_s * s.max_turn_deg_s);
            const int mistakes = Mistakes(log);
            const double crew = std::fmax(tuning_.deviation_factor_min, 1.0 - tuning_.deviation_penalty * mistakes);
            const double p = base * method * zone * manoeuvre * crew;
            const double draw = Uniform01(rng);
            const bool hit = draw < p;
            char detail[256];
            std::snprintf(detail, sizeof(detail),
                          "%s: chance %.2f = %s %.2f x %s track %.2f x zone %.2f x manoeuvre %.2f (%.0f deg/s) x "
                          "crew %.2f (%d mistakes); draw %.2f",
                          hit ? "HIT" : "MISS", p, g.target_class.c_str(), base, ToString(s.method), method, zone,
                          manoeuvre, s.max_turn_deg_s, crew, mistakes, draw);
            log.Record({t, hit ? DebriefEntry::Kind::Hit : DebriefEntry::Kind::Miss, who, s.target_id, detail});
            Finish(s, hit);
            if (hit) {
                Aircraft* a = TargetAircraft(world, targets, s.target_id);
                if (a != nullptr) a->alive = false;
            }
        }
    }

    void Finish(Shot& s, bool hit) {
        s.done = true;
        if (hit) {
            ++hits_;
        } else {
            ++misses_;
        }
    }

    void UpdateSearch(double t, const ConsoleData& console, SessionLog& log) {
        picture_.clear();
        const SearchSensorDef& d = console.search;
        if (!console.equipment.IsReady(d.antenna) || !console.equipment.IsReady(d.transmitter)) return;
        const int scale = console.panel.Get(d.range_control);
        if (scale < 0 || scale >= static_cast<int>(d.range_scales_m.size())) return;
        const std::pair<double, double>& window = d.range_scales_m[static_cast<std::size_t>(scale)];
        const bool mti = !d.mti_control.empty() && console.panel.Get(d.mti_control) == 1;
        if (em_ != nullptr) {  // the search radar's own tracks, in the crew's heading frame
            for (const EmSensorLink::Contact& c : em_->SearchPicture()) {
                if (c.range_m < window.first || c.range_m > window.second) continue;
                const double az = Wrap180(c.true_az_deg - unit_heading_deg_ + gyro_heading_deg_);
                picture_.push_back({c.target_id, c.range_m, az, c.el_deg});
                if (seen_by_search_.insert(c.target_id).second) {
                    char detail[96];
                    std::snprintf(detail, sizeof(detail), "contact %.1f km, azimuth %.0f deg", c.range_m / 1000.0, az);
                    log.Record({t, DebriefEntry::Kind::Scenario, "SOC", c.target_id, detail});
                }
            }
            return;
        }
        for (const auto& kv : geo_) {
            const Geometry& g = kv.second;
            if (!g.alive || !g.los || g.range_m < window.first || g.range_m > window.second) continue;
            if (mti ? g.range_m > d.mti_max_range_m : g.agl_m < tuning_.clutter_altitude_m) continue;
            picture_.push_back({kv.first, g.range_m, g.az_deg, g.el_deg});
            if (seen_by_search_.insert(kv.first).second) {
                char detail[96];
                std::snprintf(detail, sizeof(detail), "contact %.1f km, azimuth %.0f deg", g.range_m / 1000.0, g.az_deg);
                log.Record({t, DebriefEntry::Kind::Scenario, "SOC", kv.first, detail});
            }
        }
    }

    static double Toward(double from, double to, double max_step) {
        const double d = to - from;
        return std::fabs(d) <= max_step ? to : from + (d > 0.0 ? max_step : -max_step);
    }

    // The aiming crew turn their wheels toward the nearest contact, as the scope shows it. A locked wheel doesn't turn.
    void Aim(double dt, ConsoleData& console, const AimCrew& crew) {
        if (!crew.azimuth && !crew.elevation && !crew.range) return;
        const TrackSensorDef& d = console.track;
        struct Aimpoint {
            double range_m = 0.0;
            double az_deg = 0.0;  // the crew's frame
            double el_deg = 0.0;
        };
        Aimpoint best;
        bool have = false;
        std::string best_id;
        bool range_known = false;
        for (const Contact& c : picture_) {
            if (!have || c.range_m < best.range_m) {
                best = {c.range_m, c.azimuth_deg, c.elevation_deg};
                best_id = c.target_id;
                have = true;
            }
            range_known = true;
        }
        const bool tv_search = !d.tv_camera.empty() && console.equipment.IsReady(d.tv_camera);
        bool from_board = false;
        double board_true_az = 0.0;
        if (!have && tv_search) {  // radars silent: azimuth from the plotting board
            const Geometry* near_g = nullptr;
            for (const auto& kv : geo_) {
                const Geometry& g = kv.second;
                if (g.alive && g.range_m <= tuning_.plotting_board_range_m &&
                    (near_g == nullptr || g.range_m < near_g->range_m)) {
                    near_g = &g;
                }
            }
            if (near_g != nullptr) {
                best = {near_g->range_m, near_g->az_deg, near_g->el_deg};
                board_true_az = near_g->true_az_deg;
                have = true;
                from_board = true;
            }
        }
        if (!have) return;
        const bool az_locked = !d.lock_control.empty() && console.panel.Get(d.lock_control) < 1;
        if (crew.azimuth && !az_locked) {
            const double wheel_az = console.panel.GetValue(d.az_wheel);
            // The board gives true azimuth, set on the wheel as it is: right only if the vehicle is oriented.
            const double want_az = WheelAzimuth(from_board ? board_true_az : best.az_deg, wheel_az, d.az_limit_deg);
            console.panel.SetValue(d.az_wheel, Toward(wheel_az, want_az, tuning_.aim_az_rate_deg_s * dt));
        }
        if (crew.range && range_known) {
            const double wheel_r = console.panel.GetValue(d.range_wheel);
            console.panel.SetValue(d.range_wheel, Toward(wheel_r, best.range_m, tuning_.aim_range_rate_mps * dt));
        }
        // The elevation seat aims by the TV, or by the elevation-search indicator once the target shows on it
        // (under the azimuth line while the tracking radar radiates).
        bool on_indicator = false;
        if (d.beam_width_deg > 0.0) {
            for (const Contact& c : track_returns_) on_indicator = on_indicator || c.target_id == best_id;
        }
        if (crew.elevation && (tv_search || on_indicator)) {
            const double wheel_el = console.panel.GetValue(d.el_wheel);
            console.panel.SetValue(d.el_wheel, Toward(wheel_el, best.el_deg, tuning_.aim_el_rate_deg_s * dt));
        }
    }

    static double TvHalfFov(const ConsoleData& console) { return 0.5 * TvFieldOfViewDeg(console); }

    void UpdateTrackReturns(const ConsoleData& console) {
        track_returns_.clear();
        const TrackSensorDef& d = console.track;
        if (!console.equipment.IsReady(d.transmitter)) return;
        if (em_ != nullptr) {  // echoes the antenna's beam really picks up, wherever the range gate is
            std::string locked;
            const bool is_locked = method_ == TrackMethod::Radar && em_->Locked(&locked);
            for (const auto& kv : geo_) {
                const Geometry& g = kv.second;
                if (!g.alive || g.range_m > d.max_range_m) continue;
                if ((is_locked && kv.first == locked) || em_->EchoVisible(kv.first)) {
                    track_returns_.push_back({kv.first, g.range_m, g.az_deg, g.el_deg});
                }
            }
            return;
        }
        const double wheel_az = console.panel.GetValue(d.az_wheel);
        for (const auto& kv : geo_) {
            const Geometry& g = kv.second;
            if (!g.alive || !g.los || g.range_m > d.max_range_m) continue;
            const bool in_beam =
                std::fabs(WheelAzimuth(g.az_deg, wheel_az, d.az_limit_deg) - wheel_az) <= tuning_.az_tolerance_deg;
            if (in_beam || (kv.first == tracked_ && method_ == TrackMethod::Radar)) {
                track_returns_.push_back({kv.first, g.range_m, g.az_deg, g.el_deg});
            }
        }
    }

    // Why a lock on target g by `method` can't be held right now; nullptr if it can.
    const char* LockProblem(const ConsoleData& console, const Geometry& g, TrackMethod method) const {
        const TrackSensorDef& d = console.track;
        const Equipment& eq = console.equipment;
        if (!g.alive) return "target down";
        if (!eq.IsReady(d.slew)) return "antenna not free to turn";
        if (method == TrackMethod::Radar && em_ != nullptr) {  // the radar equation decides, not these rules
            if (!eq.IsReady(d.transmitter)) return "transmitter off";
            return nullptr;
        }
        if (!g.los) return "no line of sight";
        if (g.angular_rate_deg_s > tuning_.max_angular_rate_deg_s) return "target crossing too fast";
        if (method == TrackMethod::Radar) {
            if (!eq.IsReady(d.transmitter)) return "transmitter off";
            if (g.range_m > d.max_range_m) return "beyond tracking range";
            if (g.agl_m < tuning_.track_min_altitude_m) return "target too low for the radar";
            return nullptr;
        }
        if (d.tv_mode.empty() || !eq.IsReady(d.tv_mode) || !eq.IsReady(d.tv_camera)) return "TV tracking off";
        if (g.range_m > tuning_.tv_max_range_m) return "beyond TV range";
        return nullptr;
    }

    bool RangeKnown(const ConsoleData& console) const {
        if (method_ == TrackMethod::Radar) return true;
        const TrackSensorDef& d = console.track;
        return method_ == TrackMethod::Tv && !d.range_only_mode.empty() && console.equipment.IsReady(d.range_only_mode);
    }

    void UpdateTrack(double t, double dt, ConsoleData& console, SessionLog& log) {
        const TrackSensorDef& d = console.track;
        if (method_ != TrackMethod::None) {
            const Geometry& g = geo_.at(tracked_);
            const char* problem = LockProblem(console, g, method_);
            // The radar's own lock: the filter's estimate drives the servo, and a lock that has gone is lost.
            const bool radar_em = method_ == TrackMethod::Radar && em_ != nullptr;
            EmSensorLink::Contact estimate;
            if (problem == nullptr && radar_em) {
                std::string id;
                if (!em_->Locked(&id, &estimate) || id != tracked_) problem = "radar lost the echo";
            }
            if (problem == nullptr) {  // the tracking servo follows the target
                const double az_deg = radar_em ? Wrap180(estimate.true_az_deg - unit_heading_deg_ + gyro_heading_deg_) : g.az_deg;
                const double el_deg = radar_em ? estimate.el_deg : g.el_deg;
                const double range_m = radar_em ? estimate.range_m : g.range_m;
                console.panel.SetValue(d.az_wheel, WheelAzimuth(az_deg, console.panel.GetValue(d.az_wheel), d.az_limit_deg));
                console.panel.SetValue(d.el_wheel, el_deg);
                if (RangeKnown(console)) console.panel.SetValue(d.range_wheel, range_m);
                antenna_el_deg_ = swept_low_deg_ = swept_high_deg_ = el_deg;
                if (em_ != nullptr && method_ == TrackMethod::Tv) em_->ClearDesignation();
                return;
            }
            log.Record({t, DebriefEntry::Kind::Scenario, "SSC", tracked_, std::string("track lost: ") + problem});
            if (radar_em) em_->ClearLock();
            method_ = TrackMethod::None;
            tracked_.clear();
        }
        // Looking for a lock: radar first, then TV. Targets are checked in id order (deterministic).
        UpdateElevationSearch(dt, console);
        const double wheel_az = console.panel.GetValue(d.az_wheel);
        const double wheel_el = console.panel.GetValue(d.el_wheel);
        const double wheel_r = console.panel.GetValue(d.range_wheel);
        const bool radar_mode = console.equipment.IsReady(d.radar_mode);
        const double half_fov = d.tv_mode.empty() ? 0.0 : TvHalfFov(console);
        std::string found;
        TrackMethod found_method = TrackMethod::None;
        if (em_ != nullptr) {
            // Point the tracking radar where the wheels say (the acquisition mode must be engaged and the antenna free
            // to turn); it finds the target itself. The beam's elevation is the scan's, or the elevation wheel's.
            const Equipment& eq = console.equipment;
            const bool aiming = radar_mode && eq.IsReady(d.transmitter) && eq.IsReady(d.slew);
            std::string locked;
            if (aiming) {
                em_->Designate(IndicatedToTrueDeg(wheel_az), antenna_el_deg_, wheel_r);
                const auto it = em_->Captured(&locked) ? geo_.find(locked) : geo_.end();
                if (it != geo_.end() && it->second.alive) {
                    found = locked;
                    found_method = TrackMethod::Radar;
                }
            } else {
                em_->ClearLock();
            }
        }
        for (const auto& kv : geo_) {
            const Geometry& g = kv.second;
            if (!g.alive) continue;
            if (!found.empty() && em_ != nullptr) break;  // the radar has it; TV only looks when the radar has nothing
            const double az_err = std::fabs(WheelAzimuth(g.az_deg, wheel_az, d.az_limit_deg) - wheel_az);
            if (em_ == nullptr && radar_mode && az_err <= tuning_.az_tolerance_deg && std::fabs(g.range_m - wheel_r) <= tuning_.range_gate_m &&
                LockProblem(console, g, TrackMethod::Radar) == nullptr) {
                found = kv.first;
                found_method = TrackMethod::Radar;
                break;
            }
            if (found.empty() && half_fov > 0.0 && az_err <= half_fov && std::fabs(g.el_deg - wheel_el) <= half_fov &&
                LockProblem(console, g, TrackMethod::Tv) == nullptr) {
                found = kv.first;
                found_method = TrackMethod::Tv;
            }
        }
        if (found != candidate_ || found_method != candidate_method_) {
            candidate_ = found;
            candidate_method_ = found_method;
            candidate_time_s_ = 0.0;
        } else if (!found.empty()) {
            candidate_time_s_ += dt;
        }
        if (found.empty() || candidate_time_s_ + 1e-9 < tuning_.lock_time_s) return;
        // The radar captures when the antenna's beam is on the target in elevation as well (the OS81-10M
        // auto-capture circuit fires as the scan crosses it, voenizdat_osa_1991 p95).
        if (em_ == nullptr && candidate_method_ == TrackMethod::Radar && !BeamOnTarget(d, geo_.at(candidate_))) return;
        method_ = candidate_method_;
        tracked_ = candidate_;
        candidate_.clear();
        candidate_method_ = TrackMethod::None;
        candidate_time_s_ = 0.0;
        const Geometry& g = geo_.at(tracked_);
        char detail[96];
        std::snprintf(detail, sizeof(detail), "%s track, %.1f km, azimuth %.0f deg", ToString(method_),
                      g.range_m / 1000.0, g.az_deg);
        log.Record({t, DebriefEntry::Kind::Scenario, "SSC", tracked_, detail});
    }

    // The gyro half-compass: it turns with the hull; slaved to the course setter it turns towards the setter's
    // value (fast after СОГЛАСОВАНИЕ, until they agree). Without power it loses its orientation and starts again
    // taking the hull to face north. When the setter is connected the crew chief gives the measured heading
    // (logged), and an NPC in the elevation seat (the deputy commander) turns the setter to it.
    void UpdateOrientation(double t, double dt, ConsoleData& console, SessionLog& log, bool npc_sets_heading) {
        const double turned = Wrap180(unit_heading_deg_ - last_unit_heading_deg_);
        last_unit_heading_deg_ = unit_heading_deg_;
        if (!console.has_orientation) {
            gyro_heading_deg_ = unit_heading_deg_;  // no heading reference modelled: the frame is true north
            return;
        }
        const OrientationDef& o = console.orientation;
        if (!console.equipment.IsReady(o.gyro)) {
            gyro_heading_deg_ = 0.0;
            fast_sync_ = false;
            if (!o.synced_input.empty()) console.equipment.SetInput(o.synced_input, false);
            return;
        }
        gyro_heading_deg_ = Wrap360(gyro_heading_deg_ + turned);
        const bool setting = console.panel.Get(o.mode_control) == o.set_position;
        const double true_heading = Wrap360(unit_heading_deg_);
        if (setting && !was_setting_) {
            char detail[64];
            std::snprintf(detail, sizeof(detail), "Qc = %.1f deg (by compass)", true_heading);
            log.Record({t, DebriefEntry::Kind::Scenario, "Commander", "orientation", detail});
        }
        was_setting_ = setting;
        if (setting && npc_sets_heading) {
            const double now = Wrap360(console.panel.GetValue(o.setter));
            const double err = Wrap180(true_heading - now);
            const double step = tuning_.aim_az_rate_deg_s * dt;
            console.panel.SetValue(o.setter, std::fabs(err) <= step ? true_heading : Wrap360(now + (err > 0.0 ? step : -step)));
        }
        const bool slaved = setting && console.panel.Get(o.relay_control) == o.relay_position;
        bool synced = false;
        if (slaved) {
            if (console.panel.Get(o.sync_button) == 1) fast_sync_ = true;
            const double want = Wrap360(console.panel.GetValue(o.setter));
            const double err = Wrap180(want - gyro_heading_deg_);
            const double step = (fast_sync_ ? o.sync_rate_deg_s : o.slew_rate_deg_s) * dt;
            gyro_heading_deg_ = std::fabs(err) <= step ? want : Wrap360(gyro_heading_deg_ + (err > 0.0 ? step : -step));
            synced = std::fabs(Wrap180(want - gyro_heading_deg_)) < 0.05;
            if (synced) fast_sync_ = false;  // the scale stops
        } else {
            fast_sync_ = false;
        }
        if (!o.synced_input.empty()) console.equipment.SetInput(o.synced_input, synced);
    }

    static double Wrap360(double deg) {
        double d = std::fmod(deg, 360.0);
        if (d < 0.0) d += 360.0;
        return d;
    }

    // IFF: a press interrogates for a while; friendly aircraft in range reply (gated: only those on the scope).
    void UpdateIff(double t, ConsoleData& console, SessionLog& log) {
        iff_replies_.clear();
        const SearchSensorDef& d = console.search;
        if (d.iff_query.empty()) return;
        const bool ready = console.equipment.IsReady(d.iff_ready);
        if (ready && console.panel.Get(d.iff_query) == 1) {
            iff_until_s_ = t + d.iff_query_time_s;
            iff_control_ = false;
        }
        if (ready && !d.iff_control_query.empty() && console.panel.Get(d.iff_control_query) == 1) {
            iff_until_s_ = t + d.iff_query_time_s;
            iff_control_ = true;
        }
        const bool active = ready && t < iff_until_s_ + 1e-9;
        if (!d.iff_interrogating_input.empty()) console.equipment.SetInput(d.iff_interrogating_input, active);
        if (!active) return;
        const bool gated = !d.iff_gate_control.empty() && console.panel.Get(d.iff_gate_control) == 1;
        for (const auto& kv : geo_) {
            const Geometry& g = kv.second;
            if (!g.alive || !g.friendly || !g.los || g.range_m > d.iff_max_range_m) continue;
            if (gated) {
                bool on_scope = false;
                for (const Contact& c : picture_) on_scope = on_scope || c.target_id == kv.first;
                if (!on_scope) continue;
            }
            iff_replies_.push_back({kv.first, g.range_m, g.az_deg, g.el_deg, iff_control_});
            if (replied_.insert(kv.first).second) {
                char detail[96];
                std::snprintf(detail, sizeof(detail), "IFF reply: friend, %.1f km, azimuth %.0f deg", g.range_m / 1000.0,
                              g.az_deg);
                log.Record({t, DebriefEntry::Kind::Scenario, "IFF", kv.first, detail});
            }
        }
    }

    // A triangle wave over [low, high]: low at phase 0, high at 0.5, low again at 1.
    static double ScanAt(double phase, double low, double high) {
        double p = std::fmod(phase, 1.0);
        if (p < 0.0) p += 1.0;
        return low + (high - low) * (p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p);
    }

    // The search beam the elevation search uses: the beam selector's, or in the automatic programmes the lowest
    // beam holding the target the search radar sees nearest the azimuth line (else the first beam).
    int SelectBeam(const ConsoleData& console) const {
        const TrackSensorDef& d = console.track;
        if (d.beams_deg.empty()) return -1;
        const int pos = console.panel.Get(d.beam_control);
        if (pos >= 0 && pos < static_cast<int>(d.beam_by_position.size()) &&
            d.beam_by_position[static_cast<std::size_t>(pos)] >= 0) {
            return d.beam_by_position[static_cast<std::size_t>(pos)];
        }
        const double wheel_az = console.panel.GetValue(d.az_wheel);
        double best_err = 1e9;
        double el_deg = 0.0;
        for (const Contact& c : picture_) {
            const double err = std::fabs(WheelAzimuth(c.azimuth_deg, wheel_az, d.az_limit_deg) - wheel_az);
            if (err <= 5.0 && err < best_err) {
                best_err = err;
                el_deg = c.elevation_deg;
            }
        }
        // No echo near the azimuth line: the programme's first beam (not the scan's own angle, which would chase itself).
        if (best_err > 5.0) return 0;
        int best = 0;
        double best_gap = 1e9;
        for (std::size_t i = 0; i < d.beams_deg.size(); ++i) {
            const double low = d.beams_deg[i].first;
            const double high = d.beams_deg[i].second;
            const double gap = el_deg < low ? low - el_deg : el_deg > high ? el_deg - high : 0.0;
            if (gap < best_gap) {
                best = static_cast<int>(i);
                best_gap = gap;
            }
        }
        return best;
    }

    // Where the tracking antenna points in elevation while no target is tracked: scanning the search beam during
    // target designation (it drives the elevation wheel then), otherwise wherever the wheel is.
    void UpdateElevationSearch(double dt, ConsoleData& console) {
        const TrackSensorDef& d = console.track;
        const double wheel_el = console.panel.GetValue(d.el_wheel);
        el_beam_ = SelectBeam(console);
        const bool scanning = el_beam_ >= 0 && console.equipment.IsReady(d.el_search_mode);
        if (!scanning || dt <= 0.0) {
            antenna_el_deg_ = swept_low_deg_ = swept_high_deg_ = wheel_el;
            return;
        }
        const double low = d.beams_deg[static_cast<std::size_t>(el_beam_)].first;
        const double high = d.beams_deg[static_cast<std::size_t>(el_beam_)].second;
        // Sample the sweep finely enough that it can't jump over a beam width.
        const int samples = 1 + static_cast<int>(std::ceil(dt / (d.scan_period_s / 64.0)));
        swept_low_deg_ = swept_high_deg_ = ScanAt(scan_phase_, low, high);
        for (int i = 1; i <= samples; ++i) {
            const double e = ScanAt(scan_phase_ + dt / d.scan_period_s * i / samples, low, high);
            swept_low_deg_ = std::fmin(swept_low_deg_, e);
            swept_high_deg_ = std::fmax(swept_high_deg_, e);
        }
        scan_phase_ = std::fmod(scan_phase_ + dt / d.scan_period_s, 1.0);
        antenna_el_deg_ = ScanAt(scan_phase_, low, high);
        console.panel.SetValue(d.el_wheel, antenna_el_deg_);
    }

    // Whether the antenna's beam passed over the target's elevation in the last step (always, without an
    // elevation search).
    bool BeamOnTarget(const TrackSensorDef& d, const Geometry& g) const {
        if (!(d.beam_width_deg > 0.0)) return true;
        const double half = 0.5 * d.beam_width_deg;
        return g.el_deg >= swept_low_deg_ - half && g.el_deg <= swept_high_deg_ + half;
    }

    void SetInputs(ConsoleData& console) {
        const TrackSensorDef& d = console.track;
        Equipment& eq = console.equipment;
        const Geometry* g = tracked_.empty() ? nullptr : &geo_.at(tracked_);
        const bool range_known = g != nullptr && RangeKnown(console);
        const bool in_zone = range_known && InZone(*g);
        if (!d.captured_input.empty()) eq.SetInput(d.captured_input, method_ == TrackMethod::Radar);
        if (!d.in_zone_input.empty()) eq.SetInput(d.in_zone_input, in_zone);
        if (!d.near_zone_input.empty()) {
            bool near_zone = in_zone;
            if (range_known && !in_zone && g->closing_mps > 0.0 && g->range_m > tuning_.zone_max_range_m) {
                near_zone = (g->range_m - tuning_.zone_max_range_m) / g->closing_mps <= tuning_.zone_warning_s;
            }
            eq.SetInput(d.near_zone_input, near_zone);
        }
        if (!d.too_low_input.empty()) {
            // НЛЦ: the target under the azimuth line (or being tracked) is too low for the radar.
            bool low = false;
            const double wheel_az = console.panel.GetValue(d.az_wheel);
            for (const auto& kv : geo_) {
                const Geometry& tg = kv.second;
                if (!tg.alive || tg.range_m > d.max_range_m) continue;
                const bool aimed = std::fabs(WheelAzimuth(tg.az_deg, wheel_az, d.az_limit_deg) - wheel_az) <= 5.0;
                if ((aimed || kv.first == tracked_) && tg.agl_m < tuning_.track_min_altitude_m) low = true;
            }
            eq.SetInput(d.too_low_input, low);
        }
    }

    // Break away: turn hard onto a beam heading, away from the missile's line of sight.
    void Evade(World& world, const EngagementTarget& tg, double t, SessionLog& log) {
        Aircraft* a = world.FindAircraft(tg.world_id);
        if (a == nullptr || !a->alive) return;
        const Vec3 los = (a->position - unit_).Normalized();
        Vec3 beam{-los.y, los.x, 0.0};
        if (Dot(beam, a->velocity) < 0.0) beam = -beam;
        a->max_turn_rate_rad_s = tg.evade_turn_rate_rad_s;
        a->waypoints = {a->position + beam.Normalized() * 30000.0};
        a->next_waypoint = 0;
        log.Record({t, DebriefEntry::Kind::Scenario, tg.id, "evading", "breaks away onto a beam heading"});
    }

    EngagementTuning tuning_;
    EmSensorLink* em_ = nullptr;  // not owned
    Vec3 unit_;
    std::map<std::string, Geometry> geo_;  // ordered by id: deterministic iteration
    std::vector<Contact> picture_;
    std::vector<Contact> track_returns_;
    std::set<std::string> seen_by_search_;
    std::set<std::string> replied_;
    std::vector<Contact> iff_replies_;
    double iff_until_s_ = -1.0;
    bool iff_control_ = false;
    double unit_heading_deg_ = 0.0;
    double last_unit_heading_deg_ = 0.0;
    double gyro_heading_deg_ = 0.0;
    bool fast_sync_ = false;
    bool was_setting_ = false;
    TrackMethod method_ = TrackMethod::None;
    std::string tracked_;
    std::string candidate_;
    TrackMethod candidate_method_ = TrackMethod::None;
    double candidate_time_s_ = 0.0;
    double antenna_el_deg_ = 0.0;
    double swept_low_deg_ = 0.0;
    double swept_high_deg_ = 0.0;
    double scan_phase_ = 0.0;
    int el_beam_ = -1;
    std::vector<Shot> shots_;
    int hits_ = 0;
    int misses_ = 0;
};

}  // namespace simcore
