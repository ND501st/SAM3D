// simcore/fx/trajectories.hpp — where the things the simulation fires and destroys are drawn between their ends
// (docs/fx/README.md 3.3): our missile leaving the launcher and gathering onto the simulation's line, a bomb's fall from
// its release to its impact, a killed aircraft's wreck falling to the ground, and a gun's rounds with their tracers.
//
// The simulation decides where things start and end (a release and an impact, a launch and a burst); these paths only
// fill the time between so the drawing looks physical. Every one is a pure function of its inputs (a wreck of its seed
// too), so every client draws the same. Presentation only: nothing here feeds back into the simulation.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {
namespace fx {

namespace trajectory_detail {

inline double SmoothStep01(double x) {
    const double u = Clamp(x, 0.0, 1.0);
    return u * u * (3.0 - 2.0 * u);
}
inline double SmoothStep01Slope(double x) { return x <= 0.0 || x >= 1.0 ? 0.0 : 6.0 * x * (1.0 - x); }

// The cubic Hermite through (p0, v0) at u = 0 and (p1, v1) at u = 1 over a span of `h` seconds, and its rate (per second).
inline Vec3 HermitePoint(const Vec3& p0, const Vec3& v0, const Vec3& p1, const Vec3& v1, double h, double u) {
    const double u2 = u * u, u3 = u2 * u;
    return p0 * (2.0 * u3 - 3.0 * u2 + 1.0) + v0 * (h * (u3 - 2.0 * u2 + u)) + p1 * (3.0 * u2 - 2.0 * u3) +
           v1 * (h * (u3 - u2));
}
inline Vec3 HermiteRate(const Vec3& p0, const Vec3& v0, const Vec3& p1, const Vec3& v1, double h, double u) {
    const double u2 = u * u;
    return (p1 - p0) * ((6.0 * u - 6.0 * u2) / h) + v0 * (3.0 * u2 - 4.0 * u + 1.0) + v1 * (3.0 * u2 - 2.0 * u);
}

inline Vec3 Horizontal(const Vec3& v) { return {v.x, v.y, 0.0}; }

// Uniform draws from the raw 32-bit output of std::mt19937, which the standard fixes on every platform (the standard
// library's distributions are not fixed, so they are not used).
inline double Unit(std::mt19937& rng) { return static_cast<double>(rng()) / 4294967296.0; }
inline double Between(std::mt19937& rng, double lo, double hi) { return lo + (hi - lo) * Unit(rng); }
inline double PlusMinus(std::mt19937& rng) { return Unit(rng) < 0.5 ? -1.0 : 1.0; }

}  // namespace trajectory_detail

// ------------------------------------------------------------------ the motor

// What a rocket motor is doing `t_since_launch_s` after launch, from its boost and sustain times (munitions.json's
// "motor"): None before launch, Boost, Sustain, then Coast once burnt out (at once for a munition without a motor).
enum class MotorPhase { None, Boost, Sustain, Coast };

inline MotorPhase MotorPhaseAt(double t_since_launch_s, double boost_s, double sustain_s) {
    const double boost = std::max(boost_s, 0.0), sustain = std::max(sustain_s, 0.0);
    if (t_since_launch_s < 0.0) return MotorPhase::None;
    if (t_since_launch_s < boost) return MotorPhase::Boost;
    if (t_since_launch_s < boost + sustain) return MotorPhase::Sustain;
    return MotorPhase::Coast;
}

// ------------------------------------------------------------------ our missile leaving the launcher

// The engagement model's missile flies a line to its target from the moment of launch (docs/fx/README.md 7); the drawn
// missile instead leaves along the launcher's line (27° up for the 9M33) and gathers onto the simulation's position over
// its first `gather_s`. Its own run: from rest, evenly faster to 500 m/s in 2.5 s, then 500 m/s (an estimate, a
// short-range SAM's boost). The blend's weight rises smoothly from kDisplayGatherFromS (clear of the launcher) to
// `gather_s`, when the drawn position is exactly the simulation's.
constexpr double kDisplayBoostMps = 500.0;
constexpr double kDisplayBoostS = 2.5;
constexpr double kDisplayGatherFromS = 0.3;

// How far the drawn missile has run along its launch line `t_s` after launch, and how fast it is going.
inline double MissileDisplayRunM(double t_s) {
    const double a = kDisplayBoostMps / kDisplayBoostS;
    if (t_s <= 0.0) return 0.0;
    if (t_s <= kDisplayBoostS) return 0.5 * a * t_s * t_s;
    return 0.5 * kDisplayBoostMps * kDisplayBoostS + kDisplayBoostMps * (t_s - kDisplayBoostS);
}
inline double MissileDisplaySpeedMps(double t_s) {
    if (t_s <= 0.0) return 0.0;
    return t_s >= kDisplayBoostS ? kDisplayBoostMps : kDisplayBoostMps * t_s / kDisplayBoostS;
}

// The blend's weight on the simulation's position (0: the launch line only; 1: the simulation's) and its rate per
// second. A `gather_s` shorter than twice kDisplayGatherFromS blends from half of it instead (still smooth).
inline double MissileGatherWeight(double t_s, double gather_s) {
    if (t_s >= gather_s) return 1.0;
    const double from = std::min(kDisplayGatherFromS, 0.5 * gather_s);
    if (gather_s - from <= 1e-9) return 0.0;
    return trajectory_detail::SmoothStep01((t_s - from) / (gather_s - from));
}
inline double MissileGatherRate(double t_s, double gather_s) {
    if (t_s >= gather_s) return 0.0;
    const double from = std::min(kDisplayGatherFromS, 0.5 * gather_s);
    if (gather_s - from <= 1e-9) return 0.0;
    return trajectory_detail::SmoothStep01Slope((t_s - from) / (gather_s - from)) / (gather_s - from);
}

// Where to draw our missile `t_since_launch_s` after launch: `launch_pos` the launcher's rail, `launch_dir` the launch
// line (unit), `sim_pos` the simulation's missile position at the same moment. Continuous in time when `sim_pos` is;
// exactly `sim_pos` from `gather_s` on.
inline Vec3 MissileDisplayPosition(const Vec3& launch_pos, const Vec3& launch_dir, double t_since_launch_s,
                                   const Vec3& sim_pos, double gather_s = 2.0) {
    if (t_since_launch_s >= gather_s) return sim_pos;
    const Vec3 own = launch_pos + launch_dir.Normalized() * MissileDisplayRunM(t_since_launch_s);
    return own + (sim_pos - own) * MissileGatherWeight(t_since_launch_s, gather_s);
}

// Its velocity (the derivative of the above, for the drawn heading, the plume and the smoke): `sim_vel` is the
// simulation's missile velocity at that moment.
inline Vec3 MissileDisplayVelocity(const Vec3& launch_pos, const Vec3& launch_dir, double t_since_launch_s,
                                   const Vec3& sim_pos, const Vec3& sim_vel, double gather_s = 2.0) {
    if (t_since_launch_s >= gather_s) return sim_vel;
    const Vec3 dir = launch_dir.Normalized();
    const Vec3 own = launch_pos + dir * MissileDisplayRunM(t_since_launch_s);
    const Vec3 own_vel = dir * MissileDisplaySpeedMps(t_since_launch_s);
    const double w = MissileGatherWeight(t_since_launch_s, gather_s);
    return own_vel * (1.0 - w) + sim_vel * w + (sim_pos - own) * MissileGatherRate(t_since_launch_s, gather_s);
}

// ------------------------------------------------------------------ a bomb's fall

// The shape of a dropped weapon's path near its end (estimates from the physics of each kind, sam3d):
constexpr double kFreeFallEndSpeedShare = 0.8;  // a free-fall bomb's horizontal speed at impact over its release speed
constexpr double kFreeFallEndFallShare = 0.9;   // its vertical speed at impact over the drag-free sqrt(vz0^2 + 2 g h)
constexpr double kMinImpactAngleRad = DegToRad(30.0);  // a falling bomb arrives at least this far below the horizon
                                                       // (unless gravity cannot give it that fall: a low release)
constexpr double kRetardedEndSpeedShare = 0.3;  // a retarded (Snakeye) bomb's horizontal speed at impact
constexpr double kRetardedEndFallMps = 60.0;    // and its fall
constexpr double kChuteSettleS = 2.0;           // a chute or ballute takes this long to bring the bomb to its descent
constexpr double kChuteMaxDriftRad = DegToRad(12.0);  // under the chute the path is at most this far off the vertical
constexpr double kGlideEndMinRad = DegToRad(25.0);    // a glide bomb's dive at the end
constexpr double kGlideEndMaxRad = DegToRad(35.0);

struct BombPathParams {
    enum Kind {
        FreeFall,   // a slick bomb: the aircraft's speed at release, a steep fall at the end
        Retarded,   // a high-drag bomb (Snakeye blades): slows quickly, falls steeply
        Parachute,  // a chute or ballute opens `chute_open_s` after release, then a nearly vertical descent
        Glide,      // a winged glide bomb: a shallow dive at the end
        Rocket,     // an unguided rocket: nearly straight, at its mean speed
        Missile,    // an air-to-ground missile: the same
    };
    Vec3 release_pos, release_vel, impact_pos;  // ENU m, m/s
    double release_t_s = 0.0, impact_t_s = 0.0;  // simulation times
    Kind kind = FreeFall;
    double chute_open_s = 1.2;        // Parachute: after release (munitions.json's chute "open_s")
    // Parachute: the steady descent under the canopy. The default is the FAB-500ShN's 47 m/s (airwar_ru), the rate
    // air_war/weapon_flight.hpp's kChuteDescentMps times every chute bomb's fall by; munitions.json's retarder
    // "descent_mps" gives a munition's own (scenery::MunitionRetarder::descent_mps, 0 when not given: keep this). The path
    // still lands at impact_t_s: when the times leave too little height it descends slower, and a rate slower than the
    // simulation timed is made up in the settling after the chute opens.
    double chute_descent_mps = 47.0;
};

// A dropped weapon's drawn path: it leaves `release_pos` at `release_t_s` with `release_vel` and arrives at
// `impact_pos` at `impact_t_s`, both exactly, as a cubic Hermite (piecewise for a parachute: a drag-free fall until the
// chute opens, the settling, the steady descent) whose end velocity is the one the kind really has. Before release it
// waits at the release point; after impact it rests at the impact point with its end velocity.
//
// It never goes below the impact point's height before impact: with the end velocity falling, the cubic's height over
// the impact is h00 h + T (h10 vz0 + h11 vz1) >= 0 whenever the release's own fall rate is at most h / T. A release
// diving faster than that (the inputs disagree: it would have hit the ground sooner) is drawn with h / T.
class BombPath {
public:
    BombPath() = default;
    explicit BombPath(const BombPathParams& params) : p_(params) { Build(); }

    const BombPathParams& Params() const { return p_; }
    bool Done(double t_s) const { return t_s >= p_.impact_t_s; }

    Vec3 Position(double t_s) const {
        if (knots_.size() < 2) return t_s < p_.impact_t_s ? p_.release_pos : p_.impact_pos;
        if (t_s <= knots_.front().t_s) return p_.release_pos;
        if (t_s >= knots_.back().t_s) return p_.impact_pos;
        const std::size_t k = SegmentAt(t_s);
        const Knot& a = knots_[k];
        const Knot& b = knots_[k + 1];
        const double h = b.t_s - a.t_s;
        return trajectory_detail::HermitePoint(a.p, a.v, b.p, b.v, h, (t_s - a.t_s) / h);
    }

    Vec3 Velocity(double t_s) const {
        if (knots_.size() < 2) return t_s < p_.impact_t_s ? p_.release_vel : Vec3{};
        if (t_s <= knots_.front().t_s) return knots_.front().v;
        if (t_s >= knots_.back().t_s) return knots_.back().v;
        const std::size_t k = SegmentAt(t_s);
        const Knot& a = knots_[k];
        const Knot& b = knots_[k + 1];
        const double h = b.t_s - a.t_s;
        return trajectory_detail::HermiteRate(a.p, a.v, b.p, b.v, h, (t_s - a.t_s) / h);
    }

    // The way the weapon points: along its velocity (straight down when it has none).
    Vec3 Forward(double t_s) const {
        const Vec3 v = Velocity(t_s);
        return v.LengthSquared() > 1e-12 ? v.Normalized() : Vec3{0.0, 0.0, -1.0};
    }

    // The velocity it arrives with.
    Vec3 ImpactVelocity() const { return knots_.empty() ? Vec3{} : knots_.back().v; }

private:
    struct Knot {
        double t_s = 0.0;
        Vec3 p, v;
    };

    // The knot k with knots_[k].t_s <= t_s < knots_[k + 1].t_s (at most four knots).
    std::size_t SegmentAt(double t_s) const {
        std::size_t k = 0;
        while (k + 2 < knots_.size() && t_s >= knots_[k + 1].t_s) ++k;
        return k;
    }

    // Steepen a falling end velocity to at least `angle_rad` below the horizon, but never to a fall faster than
    // `free_fall_mps`, the drag-free sqrt(vz0^2 + 2 g h): no bomb falls faster than that (drag only slows its fall, and
    // while it climbs it lowers its apex), so a low release arrives as flat as gravity leaves it.
    static Vec3 AtLeastBelow(Vec3 v, double angle_rad, double free_fall_mps) {
        const double fall_min = std::min(std::tan(angle_rad) * trajectory_detail::Horizontal(v).Length(), free_fall_mps);
        if (-v.z < fall_min) v.z = -fall_min;
        return v;
    }

    void Build() {
        using trajectory_detail::Horizontal;
        knots_.clear();
        const double t0 = p_.release_t_s, t1 = p_.impact_t_s, dur = t1 - t0;
        if (!(dur > 1e-6)) return;  // no time between: drawn at the release, then at the impact
        const Vec3 chord = p_.impact_pos - p_.release_pos;
        const double h = -chord.z;  // the release's height above the impact
        Vec3 hdir = Horizontal(chord).Normalized();
        if (hdir.LengthSquared() < 0.5) hdir = Horizontal(p_.release_vel).Normalized();
        Vec3 v0 = p_.release_vel;
        if (h > 0.0 && v0.z < -h / dur) v0.z = -h / dur;
        const double vh0 = Horizontal(p_.release_vel).Length();
        const double vz0 = p_.release_vel.z;
        const double g = kStandardGravity;
        const double free_fall = std::sqrt(std::max(vz0 * vz0 + 2.0 * g * h, 0.0));
        knots_.push_back({t0, p_.release_pos, v0});
        Vec3 v1;
        switch (p_.kind) {
            case BombPathParams::FreeFall:
                v1 = AtLeastBelow(hdir * (kFreeFallEndSpeedShare * vh0) - Vec3{0.0, 0.0, kFreeFallEndFallShare * free_fall},
                                  kMinImpactAngleRad, free_fall);
                break;
            case BombPathParams::Retarded:
                v1 = AtLeastBelow(hdir * (kRetardedEndSpeedShare * vh0) - Vec3{0.0, 0.0, std::min(kRetardedEndFallMps, free_fall)},
                                  kMinImpactAngleRad, free_fall);
                break;
            case BombPathParams::Glide: {
                const double speed = chord.Length() / dur;
                const double angle = Clamp(std::atan2(h, Horizontal(chord).Length()), kGlideEndMinRad, kGlideEndMaxRad);
                v1 = hdir * (speed * std::cos(angle)) - Vec3{0.0, 0.0, speed * std::sin(angle)};
                break;
            }
            case BombPathParams::Rocket:
            case BombPathParams::Missile:
                v1 = chord / dur;
                break;
            case BombPathParams::Parachute:
                BuildChute(v0, hdir);
                return;
        }
        knots_.push_back({t1, p_.impact_pos, v1});
    }

    // A drag-free fall from the release until the chute opens (a quadratic, so its Hermite is exact), the settling onto
    // the descent over kChuteSettleS, then a straight, steady descent to the impact. When the times leave too little
    // height for the full descent rate, it descends slower.
    void BuildChute(const Vec3& v0, const Vec3& hdir) {
        using trajectory_detail::Horizontal;
        const double g = kStandardGravity;
        const double t0 = p_.release_t_s, t1 = p_.impact_t_s, dur = t1 - t0;
        const double tau = Clamp(p_.chute_open_s, 0.0, 0.5 * dur);
        const Vec3 p_open = p_.release_pos + v0 * tau - Vec3{0.0, 0.0, 0.5 * g * tau * tau};
        const Vec3 v_open = v0 - Vec3{0.0, 0.0, g * tau};
        const double t_open = t0 + tau;
        if (tau > 1e-6) knots_.push_back({t_open, p_open, v_open});
        const double rest = t1 - t_open;
        const double settle = std::min(kChuteSettleS, 0.5 * rest);
        const double steady = rest - settle;
        // The settling falls about half the descent rate on average: the steady descent starts no higher than that leaves.
        const double above = std::max(p_open.z - p_.impact_pos.z, 0.0);
        const double descent = std::min(std::max(p_.chute_descent_mps, 0.0), above / std::max(steady + 0.5 * settle, 1e-6));
        // The horizontal way left after the settling (about half the opening speed over it) is drifted under the chute.
        const Vec3 to_impact = Horizontal(p_.impact_pos - p_open);
        Vec3 dir = to_impact.Normalized();
        if (dir.LengthSquared() < 0.5) dir = hdir;
        const double drift_way = std::max(to_impact.Length() - 0.5 * Horizontal(v_open).Length() * settle, 0.0);
        const double drift = std::min(drift_way / std::max(steady, 1e-6), std::tan(kChuteMaxDriftRad) * descent);
        const Vec3 v_steady = dir * drift - Vec3{0.0, 0.0, descent};
        knots_.push_back({t_open + settle, p_.impact_pos - v_steady * steady, v_steady});
        knots_.push_back({t1, p_.impact_pos, v_steady});
    }

    BombPathParams p_{};
    std::vector<Knot> knots_;
};

// When a cluster dispenser on `path` opens: the last time before impact it is `opens_agl_m` above the impact point (on
// its way down, after a loft's climb). Released below that height, it opens at release; at or under 0 m, at impact.
inline double ClusterOpenTime(const BombPath& path, double opens_agl_m) {
    const BombPathParams& p = path.Params();
    const double t0 = p.release_t_s, t1 = p.impact_t_s;
    if (opens_agl_m <= 0.0 || t1 <= t0) return std::max(t0, t1);
    const double z_open = p.impact_pos.z + opens_agl_m;
    constexpr int kScan = 512;
    const double step = (t1 - t0) / kScan;
    for (int i = kScan - 1; i >= 0; --i) {  // back from the impact to the last moment above the height
        double above = t0 + step * i;
        if (path.Position(above).z < z_open) continue;
        double below = above + step;
        for (int k = 0; k < 60; ++k) {
            const double mid = 0.5 * (above + below);
            (path.Position(mid).z >= z_open ? above : below) = mid;
        }
        return 0.5 * (above + below);
    }
    return t0;
}

// ------------------------------------------------------------------ a wreck's fall

struct WreckFallParams {
    Vec3 pos, vel;             // where and how fast the aircraft was going when it was destroyed (ENU m, m/s)
    double ground_z_m = 0.0;   // the ground's ENU z under it: a flat ground, used when `ground_z_at` is not given
    std::uint32_t seed = 0;    // the same seed draws the same fall on every client
    bool falls = false;        // true: it drops out of the sky at once, tumbling; false: it spirals down
    // The ground's ENU z under any point (x, y in ENU m). A wreck drifts 1-2.5 km as it falls, and over that the ground
    // under it changes with the terrain's relief and with the Earth's curve (ENU z sinks by about d^2 / 2R away from the
    // frame's origin: 8-11 m over a 1.6-2.2 km drift outward 30 km out), so it lands where its path meets this ground,
    // not on the plane under the kill. Called only while the fall is built (the WreckFall keeps no copy, so it may
    // capture what the caller owns); empty: the flat `ground_z_m`.
    std::function<double(double x_m, double y_m)> ground_z_at;
};

// The wreck's attitude: yaw clockwise from north (as AzimuthOf), pitch up from the horizontal (as ElevationOf), roll
// right wing down; radians, unwrapped (a tumbling wreck's angles run on past 2 pi).
struct WreckRotation {
    double yaw_rad = 0.0;
    double pitch_rad = 0.0;
    double roll_rad = 0.0;
};

// A destroyed aircraft's fall: a point mass with quadratic drag, dv/dt = -g z - k |v| v, k = g / v_t^2 with the terminal
// speed v_t drawn from the seed (95-120 m/s for a tumbling wreck, 80-100 m/s for one spiralling down). One that does
// not simply fall keeps some lift for a while: banked 55-70° to one side, 1.0-1.4 g fading over 30-50 s, so it turns as
// it goes down and steepens into a dive (an estimate of how burning aircraft are seen to go down). Integrated once at
// construction (RK4, 0.02 s steps, a sample every 0.1 s) to the ground or 900 s; Position is a Hermite between samples.
// The ground is the params' `ground_z_at` under each sample (the flat `ground_z_m` without it), and the landing is where
// the Hermite meets the ground under it.
class WreckFall {
public:
    static constexpr double kSampleS = 0.1;
    static constexpr int kStepsPerSample = 5;
    static constexpr double kMaxS = 900.0;

    WreckFall() = default;
    explicit WreckFall(const WreckFallParams& params) : p_(params) {
        Draw();
        Integrate();
        p_.ground_z_at = nullptr;  // used only while building: a capture of the caller's must not outlive the call
    }

    // The parameters it was built from (without the ground function, which it does not keep).
    const WreckFallParams& Params() const { return p_; }
    double GroundTime() const { return ground_t_; }
    double TerminalMps() const { return terminal_mps_; }

    // Where the wreck is `t_since_s` after it was destroyed (at its start before, at rest on the ground after GroundTime).
    Vec3 Position(double t_since_s) const {
        if (samples_.empty()) return p_.pos;
        if (t_since_s <= 0.0) return samples_.front().p;
        if (t_since_s >= ground_t_) return ground_p_;
        return Interpolate(t_since_s, false);
    }

    Vec3 Velocity(double t_since_s) const {
        if (samples_.empty() || t_since_s >= ground_t_) return {};
        if (t_since_s <= 0.0) return samples_.front().v;
        return Interpolate(t_since_s, true);
    }

    WreckRotation Rotation(double t_since_s) const {
        const double t = Clamp(t_since_s, 0.0, ground_t_);
        WreckRotation r;
        if (p_.falls) {
            r.yaw_rad = yaw0_ + yaw_rate_ * t;
            r.pitch_rad = pitch0_ + pitch_rate_ * t;
            r.roll_rad = roll_rate_ * t;
            return r;
        }
        if (samples_.empty()) return r;
        const std::size_t i = std::min(static_cast<std::size_t>(t / kSampleS), samples_.size() - 1);
        const std::size_t j = std::min(i + 1, samples_.size() - 1);
        const double u = Clamp(t / kSampleS - static_cast<double>(i), 0.0, 1.0);
        r.yaw_rad = samples_[i].yaw_rad + (samples_[j].yaw_rad - samples_[i].yaw_rad) * u;
        const Vec3 v = i + 1 < samples_.size() ? Interpolate(t, true) : samples_.back().v;
        r.pitch_rad = ElevationOf(v);
        r.roll_rad = side_ * bank_rad_ + wobble_rad_ * std::sin(kTwoPi * wobble_hz_ * t + wobble_phase_);
        return r;
    }

private:
    struct Sample {
        Vec3 p, v;
        double yaw_rad = 0.0;  // the heading of the velocity, unwrapped from sample to sample
    };

    // Every draw in a fixed order, whatever the kind, so a seed means the same numbers everywhere: one draw a statement
    // (C++ leaves the order of two draws in one expression, such as both operands of a product, to the compiler).
    void Draw() {
        using namespace trajectory_detail;
        std::mt19937 rng(p_.seed);
        const double vt_fall = Between(rng, 95.0, 120.0), vt_glide = Between(rng, 80.0, 100.0);
        lift_g_ = Between(rng, 1.0, 1.4);
        lift_fade_s_ = Between(rng, 30.0, 50.0);
        bank_rad_ = DegToRad(Between(rng, 55.0, 70.0));
        side_ = PlusMinus(rng);
        const double yaw_sign = PlusMinus(rng);
        yaw_rate_ = yaw_sign * Between(rng, 0.3, 1.2);
        const double pitch_sign = PlusMinus(rng);
        pitch_rate_ = pitch_sign * Between(rng, 0.6, 2.0);
        const double roll_sign = PlusMinus(rng);
        roll_rate_ = roll_sign * Between(rng, 1.5, 4.0);
        wobble_rad_ = Between(rng, 0.05, 0.15);
        wobble_hz_ = Between(rng, 0.2, 0.5);
        wobble_phase_ = Between(rng, 0.0, kTwoPi);
        terminal_mps_ = p_.falls ? vt_fall : vt_glide;
        if (p_.falls) lift_g_ = 0.0;
        drag_k_ = kStandardGravity / (terminal_mps_ * terminal_mps_);
        yaw0_ = AzimuthOf(p_.vel);
        pitch0_ = ElevationOf(p_.vel);
    }

    Vec3 Accel(const Vec3& v, double t) const {
        const double speed = v.Length();
        Vec3 a = Vec3{0.0, 0.0, -kStandardGravity} - v * (drag_k_ * speed);
        if (lift_g_ <= 0.0 || speed < 1e-3) return a;
        const Vec3 f = v / speed;
        Vec3 up = Vec3{0.0, 0.0, 1.0} - f * f.z;  // the lift's plane: square to the path, up
        const double up_len = up.Length();
        if (up_len < 1e-6) return a;  // straight down: no lift to bank
        up = up / up_len;
        const Vec3 right = Cross(f, up);
        const double slow = std::min(1.0, (speed / 30.0) * (speed / 30.0));  // no lift left at a stall
        const double n = lift_g_ * std::exp(-t / lift_fade_s_) * slow;
        return a + (up * std::cos(bank_rad_) + right * (side_ * std::sin(bank_rad_))) * (n * kStandardGravity);
    }

    double Heading(const Vec3& v, double previous) const {
        if (v.HorizontalLength() < 0.5) return previous;
        return previous + std::remainder(AzimuthOf(v) - previous, kTwoPi);
    }

    // The ground's ENU z under (x, y).
    double GroundZ(double x_m, double y_m) const { return p_.ground_z_at ? p_.ground_z_at(x_m, y_m) : p_.ground_z_m; }

    // How high a point is over the ground under it.
    double AboveGround(const Vec3& p) const { return p.z - GroundZ(p.x, p.y); }

    void Integrate() {
        samples_.clear();
        Vec3 p = p_.pos, v = p_.vel;
        samples_.push_back({p, v, Heading(v, yaw0_)});
        if (AboveGround(p) <= 0.0) {
            ground_t_ = 0.0;
            ground_p_ = p;
            return;
        }
        const double dt = kSampleS / kStepsPerSample;
        const auto max_samples = static_cast<std::size_t>(kMaxS / kSampleS);
        for (std::size_t n = 1; n <= max_samples; ++n) {
            for (int s = 0; s < kStepsPerSample; ++s) {
                const double t = (static_cast<double>(n - 1) + static_cast<double>(s) / kStepsPerSample) * kSampleS;
                const Vec3 a1 = Accel(v, t), v1 = v;
                const Vec3 a2 = Accel(v + a1 * (0.5 * dt), t + 0.5 * dt), v2 = v + a1 * (0.5 * dt);
                const Vec3 a3 = Accel(v + a2 * (0.5 * dt), t + 0.5 * dt), v3 = v + a2 * (0.5 * dt);
                const Vec3 a4 = Accel(v + a3 * dt, t + dt), v4 = v + a3 * dt;
                p += (v1 + v2 * 2.0 + v3 * 2.0 + v4) * (dt / 6.0);
                v += (a1 + a2 * 2.0 + a3 * 2.0 + a4) * (dt / 6.0);
            }
            samples_.push_back({p, v, Heading(v, samples_.back().yaw_rad)});
            if (AboveGround(p) <= 0.0) {
                // The ground is crossed in the last interval (the sample before was above the ground under it): find
                // where on its Hermite, against the ground under each point tried.
                double lo = static_cast<double>(n - 1) * kSampleS, hi = static_cast<double>(n) * kSampleS;
                for (int k = 0; k < 50; ++k) {
                    const double mid = 0.5 * (lo + hi);
                    (AboveGround(Interpolate(mid, false)) > 0.0 ? lo : hi) = mid;
                }
                ground_t_ = 0.5 * (lo + hi);
                ground_p_ = Interpolate(ground_t_, false);
                ground_p_.z = GroundZ(ground_p_.x, ground_p_.y);
                return;
            }
        }
        ground_t_ = kMaxS;  // still in the air: held where the table ends
        ground_p_ = samples_.back().p;
    }

    // Position (or velocity) between the samples by their cubic Hermite.
    Vec3 Interpolate(double t, bool rate) const {
        const double x = t / kSampleS;
        std::size_t i = static_cast<std::size_t>(std::max(x, 0.0));
        if (i + 1 >= samples_.size()) i = samples_.size() - 2;
        const Sample& a = samples_[i];
        const Sample& b = samples_[i + 1];
        const double u = x - static_cast<double>(i);
        return rate ? trajectory_detail::HermiteRate(a.p, a.v, b.p, b.v, kSampleS, u)
                    : trajectory_detail::HermitePoint(a.p, a.v, b.p, b.v, kSampleS, u);
    }

    WreckFallParams p_{};
    double terminal_mps_ = 100.0;
    double drag_k_ = kStandardGravity / 1.0e4;
    double lift_g_ = 0.0, lift_fade_s_ = 30.0, bank_rad_ = 0.0, side_ = 1.0;
    double yaw0_ = 0.0, pitch0_ = 0.0, yaw_rate_ = 0.0, pitch_rate_ = 0.0, roll_rate_ = 0.0;
    double wobble_rad_ = 0.0, wobble_hz_ = 0.0, wobble_phase_ = 0.0;
    std::vector<Sample> samples_;
    double ground_t_ = 0.0;
    Vec3 ground_p_;
};

// ------------------------------------------------------------------ a gun's rounds

// A round's flight, from the muzzle: quadratic drag v' = -k |v| v - g. `drag_k` is the drag over the mass (1/m); along
// its line a round's speed falls as v0 exp(-k s), so k = ln 2 / (the distance over which it halves).
struct RoundParams {
    double muzzle_mps = 0.0;  // muzzle velocity (relative to the gun: add a moving gun's velocity to the aim yourself)
    double drag_k = 0.0;      // 1/m
    double tracer_s = 0.0;    // how long the tracer burns
    double sd_s = 0.0;        // the self-destruct's time (0: none)
};

inline double DragKForHalving(double halving_m) { return halving_m > 0.0 ? 0.69314718055994531 / halving_m : 0.0; }

// A 23 mm round's speed halving in about 2 km: an estimate (sam3d), not taken from a range table; at 980 m/s it puts
// 2 km 2.9 s away. k = 3.47e-4 1/m. munitions.json carries each round's own.
constexpr double kDragKTypical23mm = 0.69314718055994531 / 2000.0;

struct RoundState {
    Vec3 position, velocity;  // ENU m, m/s
    double t_s = 0.0;         // since it left the muzzle
    bool alive = false;       // in flight: fired and not yet self-destroyed
    bool tracer = false;      // its tracer is burning
};

// A round's flight in closed form, split along its line of fire and straight down: v = u d + w z. With the drag's speed
// taken as a(t), the speed along the line under drag and gravity's pull along it (a' = -k a^2 - g sin(elevation)), the
// equation v' = -k a v - g z is linear and solves exactly: u = v0 E, w = -g E(t) (the integral of 1 / E from 0 to t),
// with E = exp(-k (the integral of a)). The round's true speed is a sqrt(1 + (w cos(elevation) / a)^2), so the error is of the second order
// in the fall's speed against the round's: 0.4 m at 5 s for a round of 1000 m/s whose speed halves in 2 km, at any
// elevation (against a numerical integration of v' = -k |v| v - g z). The forms by gravity's pull along the line:
//   none (level fire):  a = v0 / (1 + k v0 t), E = 1 / (1 + k v0 t)
//   against (climbing): c^2 = g sin / k, a = c tan(p0 - k c t), p0 = atan(v0 / c), E = cos p0 / cos(p0 - k c t)
//   with (diving):      c^2 = -g sin / k, a = c coth(k c t + q0), q0 = atanh(c / v0) (v0 > c; tanh when slower)
// A climbing round is held where its speed along the line runs out (long after any gun's self-destruct).
struct RoundFlight {
    double run_m = 0.0;     // along the line of fire
    double along_mps = 0.0; // u: the velocity's part along the line
    double drop_m = 0.0;    // straight down from the point on the line
    double drop_mps = 0.0;  // -w: falling
};

inline RoundFlight RoundFlightAt(double t_s, double muzzle_mps, double drag_k, double sin_elevation) {
    const double g = kStandardGravity;
    const double v0 = std::max(muzzle_mps, 0.0), k = std::max(drag_k, 0.0), s = Clamp(sin_elevation, -1.0, 1.0);
    double t = std::max(t_s, 0.0);
    RoundFlight f;
    if (k * v0 < 1e-12) {  // no drag
        f.run_m = v0 * t;
        f.along_mps = v0;
        f.drop_m = 0.5 * g * t * t;
        f.drop_mps = g * t;
        return f;
    }
    const double q = g * s / k;  // c^2, signed: > 0 climbing
    if (std::fabs(q) < 1e-6 * v0 * v0) {  // level (gravity's pull along the line is negligible)
        const double a = k * v0, x = a * t;
        f.run_m = std::log1p(x) / k;
        f.along_mps = v0 / (1.0 + x);
        f.drop_m = x < 1e-3 ? g * t * t * (0.5 - x / 6.0 + x * x / 8.0)
                            : g * (0.25 * t * t + 0.5 * t / a - std::log1p(x) / (2.0 * a * a));
        f.drop_mps = g * (t + 0.5 * a * t * t) / (1.0 + x);
        return f;
    }
    const double c = std::sqrt(std::fabs(q)), kc = k * c, kc2 = kc * kc;
    if (q > 0.0) {
        const auto sec_tan = [](double x) { return std::log(1.0 / std::cos(x) + std::tan(x)); };
        const double p0 = std::atan(v0 / c);
        t = std::min(t, p0 / kc);
        const double p = std::max(p0 - kc * t, 0.0);
        const double span = sec_tan(p0) - sec_tan(p);
        f.run_m = v0 * std::cos(p0) * span / kc;
        f.along_mps = v0 * std::cos(p0) / std::cos(p);
        f.drop_m = g / kc2 * (std::sin(p0) * span + std::log(std::cos(p0) / std::cos(p)));
        f.drop_mps = g * (std::sin(p0) - std::sin(p)) / (kc * std::cos(p));
        return f;
    }
    if (v0 > c) {
        const double q0 = std::atanh(std::min(c / v0, 1.0 - 1e-12));
        const double p = std::min(q0 + kc * t, 300.0);
        const double span = std::log(std::tanh(0.5 * p)) - std::log(std::tanh(0.5 * q0));
        f.run_m = v0 * std::sinh(q0) * span / kc;
        f.along_mps = v0 * std::sinh(q0) / std::sinh(p);
        f.drop_m = g / kc2 * (std::log(std::sinh(p) / std::sinh(q0)) - std::cosh(q0) * span);
        f.drop_mps = g * (std::cosh(p) - std::cosh(q0)) / (kc * std::sinh(p));
        return f;
    }
    const double q0 = std::atanh(std::min(v0 / c, 1.0 - 1e-12));
    const double p = std::min(q0 + kc * t, 300.0);
    const double span = std::atan(std::sinh(p)) - std::atan(std::sinh(q0));
    f.run_m = v0 * std::cosh(q0) * span / kc;
    f.along_mps = v0 * std::cosh(q0) / std::cosh(p);
    f.drop_m = g / kc2 * (std::log(std::cosh(p) / std::cosh(q0)) - std::sinh(q0) * span);
    f.drop_mps = g * (std::sinh(p) - std::sinh(q0)) / (kc * std::cosh(p));
    return f;
}

// The time a round fired at `sin_elevation` takes to run `run_m` along its line (Newton's method: the run is concave in
// time, so from any start it closes in from below after the first step). A climbing round that cannot run so far gives
// the time its speed runs out, p0 / (k c), and the search starts no later than that: RoundFlightAt holds the round there,
// so past it Newton's steps would not shrink (and LeadAim would chase the target out to an infinite time of flight).
inline double RoundTimeToRunS(double run_m, double muzzle_mps, double drag_k, double sin_elevation) {
    const double x = std::max(run_m, 0.0), v0 = std::max(muzzle_mps, 1e-9), kx = std::max(drag_k, 0.0) * x;
    double t = kx > 1e-12 ? x / v0 * std::expm1(kx) / kx : x / v0;  // level fire's own inverse, a close start
    const double k = std::max(drag_k, 0.0), s = Clamp(sin_elevation, -1.0, 1.0), q = kStandardGravity * s / std::max(k, 1e-300);
    if (k * v0 >= 1e-12 && q > 0.0 && q >= 1e-6 * v0 * v0) {  // RoundFlightAt's climbing case: its reach is finite
        const double c = std::sqrt(q);
        const double t_out = std::atan(v0 / c) / (k * c);
        if (x >= RoundFlightAt(t_out, v0, drag_k, sin_elevation).run_m) return t_out;
        t = std::min(t, t_out);  // the level start can lie past it (from about 6.4 km for the 23 mm straight up)
    }
    for (int i = 0; i < 40; ++i) {
        const RoundFlight f = RoundFlightAt(t, v0, drag_k, sin_elevation);
        if (f.along_mps < 1e-9) break;
        const double step = (f.run_m - x) / f.along_mps;
        t = std::max(t - step, 0.0);
        if (std::fabs(step) < 1e-12 * std::max(t, 1.0)) break;
    }
    return t;
}

// The round fired from `muzzle_pos` along `dir_unit`, `t_s` after it left.
inline RoundState RoundAt(const Vec3& muzzle_pos, const Vec3& dir_unit, const RoundParams& round, double t_s) {
    RoundState s;
    s.t_s = t_s;
    const Vec3 dir = dir_unit.Normalized();
    const RoundFlight f = RoundFlightAt(t_s, round.muzzle_mps, round.drag_k, dir.z);
    s.position = muzzle_pos + dir * f.run_m - Vec3{0.0, 0.0, f.drop_m};
    s.velocity = dir * f.along_mps - Vec3{0.0, 0.0, f.drop_mps};
    s.alive = t_s >= 0.0 && (round.sd_s <= 0.0 || t_s < round.sd_s);
    s.tracer = s.alive && t_s < round.tracer_s;
    return s;
}

// The direction to fire from `gun_pos` so a round meets a target now at `target_pos` flying a straight line at
// `target_vel`: where the target will be after the round's time of flight, raised by the round's drop, found by
// iterating the time of flight (it converges by a factor of about the target's speed over the round's each time).
// `time_of_flight_s`, if given, gets the meeting's time.
inline Vec3 LeadAim(const Vec3& gun_pos, const Vec3& target_pos, const Vec3& target_vel, double muzzle_mps, double drag_k,
                    double* time_of_flight_s = nullptr) {
    Vec3 aim = target_pos - gun_pos;
    const auto sin_el = [](const Vec3& v) { return v.LengthSquared() > 0.0 ? v.z / v.Length() : 0.0; };
    double t = RoundTimeToRunS(aim.Length(), muzzle_mps, drag_k, sin_el(aim));
    for (int i = 0; i < 40; ++i) {
        // The drop depends on the elevation, which depends on the drop: both settle with the time of flight.
        const double drop = RoundFlightAt(t, muzzle_mps, drag_k, sin_el(aim)).drop_m;
        aim = target_pos + target_vel * t - gun_pos + Vec3{0.0, 0.0, drop};
        const double next = RoundTimeToRunS(aim.Length(), muzzle_mps, drag_k, sin_el(aim));
        const bool settled = std::fabs(next - t) < 1e-10;
        t = next;
        if (settled) break;
    }
    aim = target_pos + target_vel * t - gun_pos + Vec3{0.0, 0.0, RoundFlightAt(t, muzzle_mps, drag_k, sin_el(aim)).drop_m};
    if (time_of_flight_s != nullptr) *time_of_flight_s = t;
    return aim.Normalized();
}

}  // namespace fx
}  // namespace simcore
