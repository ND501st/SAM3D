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
//     the azimuth line and range gate on a target; TV tracking locks when its mode is
//     on and the target is inside the lens's field of view. Once locked it follows the
//     target (drives the wheels) until line of sight, altitude, crossing rate or the
//     equipment breaks the lock. It sets the console's capture / too-low / zone inputs.
//   * Crew aiming: each wheel has its own seat (azimuth, elevation, range); the crew
//     member in that seat turns it onto the nearest search-radar contact (or the
//     plotting-board track when the radars are off).
//   * Outcome: a launch starts a shot with a flight time from the game's pacing speed.
//     Guidance is by command from the ground, so losing the track loses the missile.
//     At the end, the chance of a hit is the product of plain factors — target class,
//     tracking method, in the zone at launch, target manoeuvre, crew mistakes — and one
//     draw from the scenario's random stream decides it. The debrief shows every factor.
//
// The vehicle's hull is taken to face north (azimuth 0) in this version.
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
    double azimuth_deg = 0.0;  // -180..180, 0 = north (hull forward)
    double elevation_deg = 0.0;
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
        Measure(dt, world, targets);
        ResolveShots(t, world, targets, log, rng);
        UpdateSearch(t, console, log);
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

    // The unit moved (driven by the player or an AI driver).
    void SetUnitPosition(const Vec3& unit_position) { unit_ = unit_position; }
    const Vec3& UnitPosition() const { return unit_; }

    const std::vector<Contact>& SearchPicture() const { return picture_; }
    // Echoes on the tracking radar's range indicator: targets inside its beam (the azimuth line, within the
    // tracking tolerance) up to its displayed range, with line of sight, while it radiates. Low targets still show:
    // the radar measures their range even when it can't hold them in angle.
    const std::vector<Contact>& TrackReturns() const { return track_returns_; }
    TrackMethod Method() const { return method_; }
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
            const Aircraft* a = tg.world_id >= 0 ? world.FindAircraft(tg.world_id) : nullptr;
            g.alive = a != nullptr && a->alive;
            if (!g.alive) continue;
            const Vec3 los = a->position - unit_;
            const double alt = frame.AltitudeOf(a->position);
            g.range_m = los.Length();
            g.az_deg = Wrap180(RadToDeg(AzimuthOf(los)));
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

    // The aiming crew turn their wheels toward the nearest contact. A locked wheel doesn't turn.
    void Aim(double dt, ConsoleData& console, const AimCrew& crew) {
        if (!crew.azimuth && !crew.elevation && !crew.range) return;
        const TrackSensorDef& d = console.track;
        const Geometry* best = nullptr;
        bool range_known = false;
        for (const Contact& c : picture_) {
            const Geometry& g = geo_.at(c.target_id);
            if (best == nullptr || g.range_m < best->range_m) best = &g;
            range_known = true;
        }
        const bool tv_search = !d.tv_camera.empty() && console.equipment.IsReady(d.tv_camera);
        if (best == nullptr && tv_search) {  // radars silent: azimuth from the plotting board
            for (const auto& kv : geo_) {
                const Geometry& g = kv.second;
                if (g.alive && g.range_m <= tuning_.plotting_board_range_m &&
                    (best == nullptr || g.range_m < best->range_m)) {
                    best = &g;
                }
            }
        }
        if (best == nullptr) return;
        const bool az_locked = !d.lock_control.empty() && console.panel.Get(d.lock_control) < 1;
        if (crew.azimuth && !az_locked) {
            const double wheel_az = console.panel.GetValue(d.az_wheel);
            const double want_az = WheelAzimuth(best->az_deg, wheel_az, d.az_limit_deg);
            console.panel.SetValue(d.az_wheel, Toward(wheel_az, want_az, tuning_.aim_az_rate_deg_s * dt));
        }
        if (crew.range && range_known) {
            const double wheel_r = console.panel.GetValue(d.range_wheel);
            console.panel.SetValue(d.range_wheel, Toward(wheel_r, best->range_m, tuning_.aim_range_rate_mps * dt));
        }
        if (crew.elevation && tv_search) {
            const double wheel_el = console.panel.GetValue(d.el_wheel);
            console.panel.SetValue(d.el_wheel, Toward(wheel_el, best->el_deg, tuning_.aim_el_rate_deg_s * dt));
        }
    }

    static double TvHalfFov(const ConsoleData& console) { return 0.5 * TvFieldOfViewDeg(console); }

    void UpdateTrackReturns(const ConsoleData& console) {
        track_returns_.clear();
        const TrackSensorDef& d = console.track;
        if (!console.equipment.IsReady(d.transmitter)) return;
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
            if (problem == nullptr) {  // the tracking servo follows the target
                console.panel.SetValue(d.az_wheel,
                                       WheelAzimuth(g.az_deg, console.panel.GetValue(d.az_wheel), d.az_limit_deg));
                console.panel.SetValue(d.el_wheel, g.el_deg);
                if (RangeKnown(console)) console.panel.SetValue(d.range_wheel, g.range_m);
                return;
            }
            log.Record({t, DebriefEntry::Kind::Scenario, "SSC", tracked_, std::string("track lost: ") + problem});
            method_ = TrackMethod::None;
            tracked_.clear();
        }
        // Looking for a lock: radar first, then TV. Targets are checked in id order (deterministic).
        const double wheel_az = console.panel.GetValue(d.az_wheel);
        const double wheel_el = console.panel.GetValue(d.el_wheel);
        const double wheel_r = console.panel.GetValue(d.range_wheel);
        const bool radar_mode = console.equipment.IsReady(d.radar_mode);
        const double half_fov = d.tv_mode.empty() ? 0.0 : TvHalfFov(console);
        std::string found;
        TrackMethod found_method = TrackMethod::None;
        for (const auto& kv : geo_) {
            const Geometry& g = kv.second;
            if (!g.alive) continue;
            const double az_err = std::fabs(WheelAzimuth(g.az_deg, wheel_az, d.az_limit_deg) - wheel_az);
            if (radar_mode && az_err <= tuning_.az_tolerance_deg && std::fabs(g.range_m - wheel_r) <= tuning_.range_gate_m &&
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
    Vec3 unit_;
    std::map<std::string, Geometry> geo_;  // ordered by id: deterministic iteration
    std::vector<Contact> picture_;
    std::vector<Contact> track_returns_;
    std::set<std::string> seen_by_search_;
    TrackMethod method_ = TrackMethod::None;
    std::string tracked_;
    std::string candidate_;
    TrackMethod candidate_method_ = TrackMethod::None;
    double candidate_time_s_ = 0.0;
    std::vector<Shot> shots_;
    int hits_ = 0;
    int misses_ = 0;
};

}  // namespace simcore
