// simcore/fx/acoustics.hpp — how a sound gets from where it was made to the listener (docs/fx/README.md, "Sound"):
// the speed of sound in the air between them, the delay (the sound left the source when the source was somewhere else),
// the Doppler shift, spherical spreading, the air's absorption of the high frequencies (ISO 9613-1), and a supersonic
// source's Mach cone and the boom it carries (Carlson's simplified method, NASA TP-1122).
//
// Presentation only: nothing here changes the simulation. The game turns a Reception into each voice's position (where
// the sound left from), pitch (the Doppler factor), volume (the level at the ear through the mix) and low-pass cut-off.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "simcore/atmosphere.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace fx {

// ------------------------------------------------------------------ the air

// The speed of sound in dry air at `temperature_k`: c = sqrt(gamma R T) (343.2 m/s at 20 °C).
inline double SpeedOfSound(double temperature_k) {
    return std::sqrt(isa::kGamma * isa::kGasConstantAir * std::max(temperature_k, 150.0));
}

// Relative humidity (%) from the air temperature and the dew point (°C), by the Magnus formula over water.
inline double RelativeHumidityPct(double temperature_c, double dewpoint_c) {
    const auto es = [](double t) { return std::exp(17.625 * t / (t + 243.04)); };
    return Clamp(100.0 * es(dewpoint_c) / es(temperature_c), 0.0, 100.0);
}

// ISO 9613-1:1993 pure-tone atmospheric absorption: dB per metre at frequency `f_hz` in air at `temperature_k`,
// relative humidity `rh_pct` and pressure `pressure_pa` (oxygen and nitrogen relaxation plus classical absorption).
// At 20 °C, 70 % and 101.325 kPa: 4.98 dB/km at 1 kHz, 77.6 dB/km at 8 kHz (ISO 9613-2 table 2: 5.0 and 76.6).
inline double AirAbsorptionDbPerM(double f_hz, double temperature_k, double rh_pct, double pressure_pa) {
    constexpr double kT0 = 293.15, kT01 = 273.16, kPr = 101325.0;
    const double pa = pressure_pa / kPr;
    const double tr = temperature_k / kT0;
    const double c_sat = -6.8346 * std::pow(kT01 / temperature_k, 1.261) + 4.6151;
    const double h = std::max(rh_pct, 0.0) * std::pow(10.0, c_sat) / pa;  // molar concentration of water vapour, %
    const double fr_o = pa * (24.0 + 4.04e4 * h * (0.02 + h) / (0.391 + h));
    const double fr_n = pa / std::sqrt(tr) * (9.0 + 280.0 * h * std::exp(-4.170 * (std::pow(tr, -1.0 / 3.0) - 1.0)));
    const double f2 = f_hz * f_hz;
    return 8.686 * f2 *
           (1.84e-11 / pa * std::sqrt(tr) +
            std::pow(tr, -2.5) * (0.01275 * std::exp(-2239.1 / temperature_k) / (fr_o + f2 / fr_o) +
                                  0.1068 * std::exp(-3352.0 / temperature_k) / (fr_n + f2 / fr_n)));
}

// The frequency above which the air has taken `budget_db` or more off a sound that came `distance_m`: the cut-off of a
// low-pass that stands for the absorption (a far explosion's thud, a far jet's rumble). 20 kHz when nothing is lost.
inline double AbsorptionCutoffHz(double distance_m, double temperature_k, double rh_pct, double pressure_pa,
                                 double budget_db = 6.0) {
    constexpr double kTop = 20000.0, kBottom = 60.0;
    const auto loss = [&](double f) { return AirAbsorptionDbPerM(f, temperature_k, rh_pct, pressure_pa) * distance_m; };
    if (distance_m <= 0.0 || loss(kTop) <= budget_db) return kTop;
    if (loss(kBottom) >= budget_db) return kBottom;
    double lo = std::log(kBottom), hi = std::log(kTop);
    for (int i = 0; i < 40; ++i) {  // absorption rises with frequency: bisect in log frequency
        const double mid = 0.5 * (lo + hi);
        (loss(std::exp(mid)) > budget_db ? hi : lo) = mid;
    }
    return std::exp(0.5 * (lo + hi));
}

// Spherical spreading from `ref_m` (where a source's level is given) out to `distance_m`: 6 dB per doubling.
inline double SpreadingLossDb(double distance_m, double ref_m) {
    return 20.0 * std::log10(std::max(distance_m, ref_m) / std::max(ref_m, 1e-3));
}

// ------------------------------------------------------------------ the delay and the Doppler shift

// Where a source has been: its positions at the times they were sampled (ENU m, s), oldest first, long enough to cover
// the longest delay heard (a sound from 30 km takes 90 s). Samples older than `span_s` behind the newest are dropped.
class SourceTrack {
public:
    struct Sample {
        double t_s = 0.0;
        Vec3 position;
    };

    explicit SourceTrack(double span_s = 120.0) : span_s_(span_s) {}

    // Add where the source is at `t_s` (not earlier than the newest sample; a repeated time replaces it).
    void Push(double t_s, const Vec3& position) {
        if (!samples_.empty() && t_s <= samples_.back().t_s) {
            samples_.back().position = position;
            return;
        }
        samples_.push_back({t_s, position});
        std::size_t drop = 0;
        while (drop + 2 < samples_.size() && samples_[drop + 1].t_s < t_s - span_s_) ++drop;
        if (drop > 64 || (drop > 0 && drop * 4 > samples_.size())) {
            samples_.erase(samples_.begin(), samples_.begin() + static_cast<std::ptrdiff_t>(drop));
        }
    }

    bool Empty() const { return samples_.empty(); }
    std::size_t Size() const { return samples_.size(); }
    const Sample& Newest() const { return samples_.back(); }
    const Sample& Oldest() const { return samples_.front(); }
    void Clear() { samples_.clear(); }

    // The source's position and velocity at `t_s` (between samples: on the straight line between them; before the
    // first or after the last: held at the end, still).
    Vec3 PositionAt(double t_s) const {
        if (samples_.empty()) return {};
        if (t_s <= samples_.front().t_s) return samples_.front().position;
        if (t_s >= samples_.back().t_s) return samples_.back().position;
        const std::size_t k = Segment(t_s);
        const Sample& a = samples_[k];
        const Sample& b = samples_[k + 1];
        return a.position + (b.position - a.position) * ((t_s - a.t_s) / (b.t_s - a.t_s));
    }
    Vec3 VelocityAt(double t_s) const {
        if (samples_.size() < 2 || t_s < samples_.front().t_s || t_s > samples_.back().t_s) return {};
        const std::size_t k = Segment(t_s);
        return (samples_[k + 1].position - samples_[k].position) / (samples_[k + 1].t_s - samples_[k].t_s);
    }

    const std::vector<Sample>& Samples() const { return samples_; }

private:
    // The segment [k, k+1] holding t_s (samples_.size() >= 2, front().t_s <= t_s <= back().t_s).
    std::size_t Segment(double t_s) const {
        std::size_t lo = 0, hi = samples_.size() - 1;
        while (hi - lo > 1) {
            const std::size_t mid = (lo + hi) / 2;
            (samples_[mid].t_s <= t_s ? lo : hi) = mid;
        }
        return lo;
    }

    double span_s_;
    std::vector<Sample> samples_;
};

// What the listener hears of a source at one moment.
struct Reception {
    bool heard = false;        // false: no sound from the source has reached the listener yet (too far for the time it
                               // has existed, or ahead of a supersonic source's Mach cone)
    double emit_time_s = 0.0;  // when the sound now arriving left the source
    Vec3 emit_position;        // where the source was then: the direction the sound comes from
    double distance_m = 0.0;   // how far it came
    double delay_s = 0.0;      // how long it took
    double doppler = 1.0;      // the received frequency over the emitted one, 1 / (1 - v_r / c); the playback rate
    double source_mach = 0.0;  // the source's speed then, over the speed of sound
};

// The sound now reaching a listener at `listener` at time `t_s` from a source whose track is `track`, in air with sound
// speed `c_mps` (the mean along the path): the latest emission time tau with |listener - S(tau)| = c (t - tau). A subsonic
// source has exactly one; a supersonic source none ahead of its Mach cone and two inside it, of which the later is the one
// played (its sounds arrive in their own order, pitched down as it goes away; the earlier branch arrives reversed).
inline Reception Hear(const SourceTrack& track, const Vec3& listener, double t_s, double c_mps) {
    Reception r;
    const auto& s = track.Samples();
    if (s.empty() || c_mps <= 0.0) return r;
    // h(tau) = c (t - tau) - |listener - S(tau)|: >= 0 once the sound sent at tau has arrived.
    const auto h = [&](double tau, const Vec3& p) { return c_mps * (t_s - tau) - (listener - p).Length(); };
    std::size_t k = s.size() - 1;
    bool found = false;
    double tau = 0.0;
    Vec3 velocity;
    if (s.size() >= 2) velocity = (s[k].position - s[k - 1].position) / std::max(s[k].t_s - s[k - 1].t_s, 1e-9);
    if (s[k].t_s <= t_s && h(s[k].t_s, s[k].position) >= 0.0) {
        // The newest sample's sound has arrived: the source is held still after it, so the sound now arriving left
        // from there (in practice the listener is at the source). Its last segment's velocity gives the shift.
        tau = t_s - (listener - s[k].position).Length() / c_mps;
        r.emit_position = s[k].position;
        found = true;
    } else {
        // Walk back to the newest sample whose sound has already arrived; the root is in the segment after it.
        while (k > 0 && !found) {
            --k;
            if (s[k].t_s > t_s) continue;
            if (h(s[k].t_s, s[k].position) < 0.0) continue;
            using Sample = SourceTrack::Sample;
            const Sample& a = s[k];
            const Sample& b = s[k + 1];
            const double seg = b.t_s - a.t_s;
            const Vec3 v = (b.position - a.position) / std::max(seg, 1e-9);
            const Vec3 d = listener - a.position;
            const double big_t = t_s - a.t_s;
            // |d - v u| = c (T - u): (|v|^2 - c^2) u^2 + 2 (c^2 T - d.v) u + (|d|^2 - c^2 T^2) = 0
            const double qa = v.LengthSquared() - c_mps * c_mps;
            const double qb = 2.0 * (c_mps * c_mps * big_t - Dot(d, v));
            const double qc = d.LengthSquared() - c_mps * c_mps * big_t * big_t;
            const double u_hi = std::min(seg, big_t);
            double best = -1.0;
            const auto take = [&](double u) {
                if (u >= -1e-9 && u <= u_hi + 1e-9) best = std::max(best, Clamp(u, 0.0, u_hi));
            };
            if (std::fabs(qa) < 1e-9 * c_mps * c_mps) {
                if (std::fabs(qb) > 1e-12) take(-qc / qb);
            } else {
                const double disc = qb * qb - 4.0 * qa * qc;
                if (disc >= 0.0) {
                    const double sq = std::sqrt(disc);
                    take((-qb + sq) / (2.0 * qa));
                    take((-qb - sq) / (2.0 * qa));
                }
            }
            if (best < 0.0) best = 0.0;  // rounding at the sample itself
            tau = a.t_s + best;
            r.emit_position = a.position + v * best;
            velocity = v;
            found = true;
        }
    }
    if (!found) return r;  // nothing the track holds has been heard yet
    r.heard = true;
    r.emit_time_s = tau;
    r.distance_m = (listener - r.emit_position).Length();
    r.delay_s = t_s - tau;
    const double speed = velocity.Length();
    r.source_mach = speed / c_mps;
    const Vec3 to_listener = r.distance_m > 1e-6 ? (listener - r.emit_position) / r.distance_m : Vec3{};
    const double closing = Dot(velocity, to_listener);  // the source's speed towards the listener
    r.doppler = 1.0 / std::max(1.0 - closing / c_mps, 1e-3);
    return r;
}

// The mean speed of sound along the path between two altitudes, for an atmosphere `air(alt_m)` (returns an
// AtmosphereState: the weather's Air or StandardAtmosphere): the harmonic mean of `n` samples (the travel time is the sum
// of each stretch's length over its own speed).
template <typename AirFn>
double MeanSoundSpeed(AirFn air, double alt_a_m, double alt_b_m, int n = 6) {
    double inv = 0.0;
    for (int i = 0; i < n; ++i) {
        const double f = (i + 0.5) / n;
        inv += 1.0 / SpeedOfSound(air(alt_a_m + (alt_b_m - alt_a_m) * f).temperature_k);
    }
    return n / inv;
}

// ------------------------------------------------------------------ the boom

// Carlson's simplified sonic-boom method (H. W. Carlson, NASA TP-1122, 1978) for steady level flight: the peak
// overpressure of the N-wave on the ground (Pa) and its duration (s). `altitude_m` is the height above the ground,
// `pressure_aircraft_pa` and `pressure_ground_pa` the ambient pressures there, `sound_speed_aircraft_mps` the speed of
// sound at the aircraft, `length_m` its length, `shape_factor` K_s (the weight-and-shape factor, 0.08-0.12 for fighters at
// cruise weight). The reflection factor is 2 (hard ground). Carlson's K_p and K_t, the pressure-amplification and
// signature-duration factors, are taken as 1: in TP-1122 they are read from his charts by altitude and Mach for the
// flight-path angle and carry the real atmosphere's gradients into the propagation, so leaving them at 1 omits that
// correction (it grows towards the cut-off Mach; a dB or two on a rare boom). With that, an F-15-sized fighter (19.4 m) at
// Mach 1.5 and 10 km gives 98.4 Pa (about 2 psf) for 0.146 s.
struct BoomSignature {
    double overpressure_pa = 0.0;
    double duration_s = 0.0;
};

inline BoomSignature CarlsonBoom(double mach, double altitude_m, double pressure_aircraft_pa, double pressure_ground_pa,
                                 double sound_speed_aircraft_mps, double length_m, double shape_factor = 0.1) {
    BoomSignature b;
    if (mach <= 1.0 || altitude_m <= 0.0 || length_m <= 0.0) return b;
    constexpr double kReflection = 2.0, kPressureAmp = 1.0, kTimeAmp = 1.0;  // K_r; K_p and K_t omitted (above)
    const double m21 = mach * mach - 1.0;
    const double l34 = std::pow(length_m, 0.75);
    b.overpressure_pa = kPressureAmp * kReflection * std::sqrt(pressure_aircraft_pa * pressure_ground_pa) *
                        std::pow(m21, 0.125) * std::pow(altitude_m, -0.75) * l34 * shape_factor;
    b.duration_s = kTimeAmp * (3.42 / sound_speed_aircraft_mps) * (mach / std::pow(m21, 0.375)) *
                   std::pow(altitude_m, 0.25) * l34 * shape_factor;
    return b;
}

// A boom reaches the ground only if the aircraft goes faster over it than sound does at the ground (the cut-off Mach:
// below it the cone's rays bend back up before they reach the ground, and only a rumble is heard).
inline bool BoomReachesGround(double ground_speed_mps, double sound_speed_ground_mps) {
    return ground_speed_mps > sound_speed_ground_mps;
}

// The peak sound pressure level of an overpressure (dB re 20 µPa).
inline double PressureToDb(double pressure_pa) { return 20.0 * std::log10(std::max(pressure_pa, 2e-5) / 2e-5); }
inline double DbToPressure(double level_db) { return 2e-5 * std::pow(10.0, level_db / 20.0); }

// Watches one source's reception from frame to frame and says when a boom arrives: the first sound of a supersonic
// source to reach the listener (the cone's edge passing over), or a forward jump in the emission time onto a stretch the
// source flew supersonic (it went through the sound barrier and its cone has caught up with its older, slower sound).
// `track_start_s` is when the track began: a first sound sent right then is the source appearing, not its cone.
//
// Just after a cone passes, the later root runs forward as sqrt(2 (t - t_cone) / t''), t'' = c (M^2 - 1)^1.5 / (h M) at
// the cone: for a jet near Mach 1 or high up (Mach 1.13 at 10 km: t'' = 0.004 /s) one frame moves it seconds, more than
// the Doppler-capped advance allows, which is the same cone, not a new one. So a boom holds off another for
// kBoomHoldOffS: two passages of one source's cone over the listener are further apart than that (a supersonic turn
// back for a second pass takes tens of seconds at the least).
constexpr double kBoomHoldOffS = 1.0;

class BoomWatch {
public:
    // Call once a frame with this frame's reception and the frame's step. True on the frame the boom arrives.
    bool Update(const Reception& r, double dt_s, double track_start_s = -1e300) {
        bool boom = false;
        if (r.heard && r.source_mach > 1.0) {
            const double expected = last_heard_ ? last_emit_s_ + std::max(last_doppler_, 1.0) * dt_s : r.emit_time_s;
            boom = last_heard_ ? r.emit_time_s - expected > 0.5 : r.emit_time_s > track_start_s + 0.5;
        }
        last_heard_ = r.heard;
        if (r.heard) {
            last_emit_s_ = r.emit_time_s;
            last_doppler_ = std::min(r.doppler, 50.0);
        }
        since_boom_s_ += dt_s;
        if (boom && since_boom_s_ < kBoomHoldOffS) boom = false;
        if (boom) since_boom_s_ = 0.0;
        return boom;
    }

private:
    bool last_heard_ = false;
    double last_emit_s_ = 0.0;
    double last_doppler_ = 1.0;
    double since_boom_s_ = 1e9;  // since the last boom
};

// ------------------------------------------------------------------ the mix

// The levels a game can play, from the levels the ear gets (a jet at 300 m: 110 dB; a 500 kg bomb at 200 m: 160 dB peak;
// a quiet field: 30 dB). Like a high-dynamic-range mix (Frostbite's "HDR audio"): a window hangs below the loudest sound
// now heard, with a fast attack and a slow release, so a near explosion pushes everything else down and the distant war
// comes back up after it; sounds below the window are not played at all (fewer voices). Within the window levels are
// compressed by `slope` (1: as heard; 0.6: 10 dB at the ear is 6 dB in the speakers).
class LoudnessWindow {
public:
    struct Params {
        double floor_db = 105.0;     // the top of the window never sinks below this (a quiet scene is not turned up)
        double depth_db = 60.0;      // the window's depth: quieter than top - depth is not played
        double slope = 0.7;          // compression inside the window
        double attack_db_per_s = 400.0;
        double release_db_per_s = 12.0;
    };

    LoudnessWindow() = default;
    explicit LoudnessWindow(Params p) : p_(p), top_db_(p.floor_db) {}

    // Once a frame, with the loudest level heard this frame (dB).
    void Update(double loudest_db, double dt_s) {
        const double target = std::max(loudest_db, p_.floor_db);
        if (target > top_db_) top_db_ = std::min(target, top_db_ + p_.attack_db_per_s * dt_s);
        else top_db_ = std::max(target, top_db_ - p_.release_db_per_s * dt_s);
    }

    double TopDb() const { return top_db_; }

    // A sound's volume in the speakers (linear, 0..1; 0 = don't play it) from its level at the ear.
    double Gain(double level_db) const {
        const double below = top_db_ - level_db;
        if (below > p_.depth_db) return 0.0;
        return std::pow(10.0, -std::max(below, 0.0) * p_.slope / 20.0);
    }

    bool Audible(double level_db) const { return top_db_ - level_db <= p_.depth_db; }

private:
    Params p_{};
    double top_db_ = 105.0;
};

// A jet's noise is not the same all round it: the roar of the exhaust's mixing peaks some 40° off the tail (about 140°
// from the nose), the fan's whine forward. Level changes (dB) by the angle between the nose and the listener (rad).
inline double JetRoarDirectivityDb(double angle_from_nose_rad) {
    const double a = angle_from_nose_rad * 180.0 / 3.14159265358979323846;
    return 6.0 * std::exp(-std::pow((a - 140.0) / 35.0, 2.0)) - 5.0 * std::exp(-std::pow(a / 55.0, 2.0));
}
inline double JetWhineDirectivityDb(double angle_from_nose_rad) {
    const double a = angle_from_nose_rad * 180.0 / 3.14159265358979323846;
    return 4.0 * std::exp(-std::pow(a / 50.0, 2.0)) - 8.0 * std::exp(-std::pow((a - 180.0) / 50.0, 2.0));
}

}  // namespace fx
}  // namespace simcore
