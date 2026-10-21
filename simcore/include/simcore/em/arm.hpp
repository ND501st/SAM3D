// simcore/em/arm.hpp — anti-radiation missiles (roadmap E6): a missile whose passive seeker homes on a radar.
//
// An ARM is a platform of the EM world carrying a receiver. Its carrier sets it at launch to the signal its own
// warning receiver heard (frequency and repetition rate) and fires it, lofted, toward where it believes the radar is.
// Each tick its seeker listens like any passive receiver (esm_system.hpp: the one-way radar equation, the emitter's
// antenna pattern toward it, terrain and ground reflection on the path) to the emitters in its band, on the signal it
// was set to, inside its field of view. Two such emitters closer together than its resolution are one direction to it:
// it homes on their power-weighted middle, which is how a decoy on the radar's own parameters, beside it or blinking
// with it, draws the missile off (Czechoslovak and Soviet doctrine, docs/EM_ARCHITECTURE.md §29). Its line of sight is
// a smoothed, noisy measurement, never the truth.
//
// Guidance is pursuit of that line of sight at the missile's average speed, within its steering limit (a radar stands
// still, so pursuit and proportional navigation meet at the same point). When the emission stops:
//   * with a memory (HARM class: "an internal inertial system keeps the missile heading toward the target after it has
//     locked on once, though CEP is larger", designation-systems.net), it flies on to where its last line of sight met
//     the ground; the error of that point is the line of sight's angle error times the range then, so a radar that
//     goes silent early is missed by hundreds of metres and one that goes silent in the last seconds is not;
//   * without one (Shrike class: "if the SAM radar switched off, missiles already fired lost lock and went ballistic"),
//     it keeps its last heading and falls under gravity.
// It hears again whatever comes back on the air (a search radar in bursts, a decoy) and homes on that.
//
// The fuze bursts it within its lethal radius of a ground platform (the closest point of this tick's path) or where
// it meets the ground; a platform within the lethal radius is put out of action (its radars and nodes go off the air,
// DestroyPlatformIn). The ARM is an echo too: the defenders' radars see a small, fast mark (voenizdat_osa_1991 p262).
#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/em/detection.hpp"
#include "simcore/em/esm_system.hpp"
#include "simcore/em/model.hpp"
#include "simcore/em/scene_ops.hpp"

namespace simcore {
namespace em {

constexpr double kArmGravity = 9.80665;
// Until its seeker hears something a lofted ARM pitches over toward the believed point no harder than this (an
// estimate: about a gravity turn's rate with the autopilot's help, so that the loft still buys height and range).
constexpr double kArmPitchOverMps2 = 2.0 * kArmGravity;

// The direction a carrier fires along: at the place it believes the radar is, lofted `loft_rad` above that line.
inline Vec3 ArmLaunchDirection(const Vec3& from, const Vec3& believed, double loft_rad) {
    const Vec3 to = believed - from;
    return DirectionFromAzEl(AzimuthOf(to), ElevationOf(to) + loft_rad);
}

// Fire an ARM of library entry `def_index` from `shooter_platform_id` at `aim_point` (where its carrier believes the
// radar is), set to the signal `freq_hz` / `prf_hz` its receiver heard. It leaves along `direction` when one is given,
// else lofted along ArmLaunchDirection. Returns the ARM's id (0 if it can't).
inline int LaunchArm(EmScene& s, int shooter_platform_id, int def_index, const Vec3& aim_point, double freq_hz, double prf_hz,
                     const Vec3* direction = nullptr) {
    const Platform* shooter = s.FindPlatform(shooter_platform_id);
    if (shooter == nullptr || !shooter->alive || def_index < 0 || static_cast<std::size_t>(def_index) >= s.lib.arms.size()) return 0;
    const ArmDef& d = s.lib.arms[static_cast<std::size_t>(def_index)];
    const Vec3 dir = direction != nullptr ? direction->Normalized() : ArmLaunchDirection(shooter->position, aim_point, d.loft_rad);
    Platform p;
    p.id = s.next_id++;
    p.name = d.name;
    p.side = shooter->side;
    p.kind = PlatformKind::Missile;
    p.rcs = RcsModel{d.rcs_m2, d.rcs_m2, d.rcs_m2};
    p.position = shooter->position;
    p.velocity = dir * d.speed_mps;
    p.altitude_m = s.frame.AltitudeOf(p.position);
    p.height_agl_m = p.altitude_m - s.GetTerrain().HeightAt(p.position.x, p.position.y);
    s.platforms.push_back(p);
    ArmState a;
    a.id = s.next_id++;
    a.def_index = def_index;
    a.platform_id = p.id;
    a.shooter_platform_id = shooter_platform_id;
    a.launched_s = s.time_s;
    a.tuned_freq_hz = freq_hz;
    a.tuned_prf_hz = prf_hz;
    a.aim_point = aim_point;
    a.loft_s = direction != nullptr ? 0.0 : 0.5 * (aim_point - shooter->position).Length() / d.speed_mps;
    s.arms.push_back(a);
    s.Log(EmEventKind::Info, a.id, p.id, shooter->name + " fires " + d.name);
    return a.id;
}

// Is the emitter on the signal the ARM was set to, in its band?
inline bool ArmTunedTo(const ArmDef& d, const ArmState& a, const EmitterState& e) {
    const double f = e.freq_hz > 0.0 ? e.freq_hz : e.def.waveform.freq_hz;
    if (f < d.freq_min_hz || f > d.freq_max_hz) return false;
    if (d.tune_fraction > 0.0 && a.tuned_freq_hz > 0.0 && std::fabs(f - a.tuned_freq_hz) > d.tune_fraction * a.tuned_freq_hz) return false;
    const double prf = e.def.waveform.prf_hz;
    if (d.prf_fraction > 0.0 && a.tuned_prf_hz > 0.0 && std::fabs(prf - a.tuned_prf_hz) > d.prf_fraction * a.tuned_prf_hz) return false;
    return true;
}

// Where a ray from `from` along unit `dir` meets the ground (false if it doesn't within `max_m`).
inline bool ArmGroundPoint(const EmScene& s, const Vec3& from, const Vec3& dir, double max_m, Vec3& out) {
    const Terrain& terrain = s.GetTerrain();
    auto above = [&](const Vec3& q) { return s.frame.AltitudeOf(q) - terrain.HeightAt(q.x, q.y); };
    if (above(from) <= 0.0) {
        out = from;
        return true;
    }
    const double step = 50.0;
    double lo = 0.0;
    for (double t = step; t <= max_m; t += step) {
        if (above(from + dir * t) > 0.0) {
            lo = t;
            continue;
        }
        double hi = t;
        for (int i = 0; i < 20; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (above(from + dir * mid) > 0.0) lo = mid; else hi = mid;
        }
        out = from + dir * hi;
        return true;
    }
    return false;
}

// One seeker look: the direction it measures to what it hears now (false: nothing).
inline bool ArmSeekerLook(EmScene& s, ArmState& a, const ArmDef& d, const Platform& rx, double dt, Vec3& measured) {
    const double sens_w = DbToLinear(d.sensitivity_dbw);
    const Vec3 nose = rx.velocity.Normalized();
    struct Heard {
        Vec3 dir;
        double power_w;
        double margin;
        int emitter;
    };
    std::vector<Heard> heard;
    for (const EmitterState& e : s.emitters) {
        if (!e.InAir() || e.platform_id == rx.id) continue;
        const Platform* tx = s.FindPlatform(e.platform_id);
        if (tx == nullptr || !tx->alive || tx->side == rx.side) continue;
        if (!ArmTunedTo(d, a, e)) continue;
        const Vec3 to = tx->position - rx.position;
        const double range = to.Length();
        if (range < 1.0 || AngleBetween(nose, to) > d.fov_rad) continue;
        const double f = e.freq_hz > 0.0 ? e.freq_hz : e.def.waveform.freq_hz;
        const IlluminationGeometry ill = Illumination(s, e, *tx, rx, dt);
        const double eirp = e.def.peak_power_w * DbToLinear(AntennaGainDb(ill.antenna, ill.off_az_rad, ill.off_el_rad) - e.def.tx_loss_db);
        const LinkLossResult loss = LinkLossCached(s, a.paths, *tx, rx, f);
        const double pr = OneWayReceivedPowerW(eirp, d.antenna_gain_db, f, range, loss.TotalOneWayDb());
        if (pr < sens_w) continue;
        heard.push_back({to / range, pr, pr / sens_w, e.id});
    }
    if (heard.empty()) return false;
    // The one it was homing on (nearest its line of sight), else the strongest; with it, everything inside its resolution.
    std::size_t pick = 0;
    for (std::size_t i = 1; i < heard.size(); ++i) {
        const bool better = a.has_los ? AngleBetween(heard[i].dir, a.los) < AngleBetween(heard[pick].dir, a.los)
                                      : heard[i].power_w > heard[pick].power_w;
        if (better) pick = i;
    }
    Vec3 sum;
    double margin = 0.0;
    for (const Heard& h : heard) {
        if (AngleBetween(h.dir, heard[pick].dir) > d.resolution_rad) continue;
        sum += h.dir * h.power_w;
        margin += h.margin;
    }
    const Vec3 centre = sum.Normalized();
    const double sigma = d.angle_sigma_rad * std::sqrt(1.0 + 1.0 / margin);
    const double el = ElevationOf(centre);
    const double ce = std::cos(el) > 0.2 ? std::cos(el) : 0.2;
    const double n_az = s.rng.Normal(Stream::ArmSeeker, Key(a.id), Key(static_cast<long long>(s.tick)), 1);
    const double n_el = s.rng.Normal(Stream::ArmSeeker, Key(a.id), Key(static_cast<long long>(s.tick)), 2);
    measured = DirectionFromAzEl(AzimuthOf(centre) + sigma * n_az / ce, el + sigma * n_el);
    a.debug_truth_emitter = heard[pick].emitter;
    return true;
}

// Burst: what it puts out of action (the nearest ground platform within the lethal radius), and how near it came.
inline void ArmBurst(EmScene& s, ArmState& a, const ArmDef& d, const Vec3& at) {
    a.burst_point = at;
    a.burst_s = s.time_s;
    a.miss_m = -1.0;
    for (const Platform& g : s.platforms) {
        if (!g.alive || g.id == a.platform_id || g.id == a.shooter_platform_id) continue;
        if (g.kind != PlatformKind::Ground && g.kind != PlatformKind::Decoy) continue;
        const double m = (g.position - at).Length();
        if (a.miss_m < 0.0 || m < a.miss_m) {
            a.miss_m = m;
            a.nearest_platform = g.id;
        }
    }
    if (Platform* p = s.FindPlatform(a.platform_id)) p->alive = false;
    if (a.nearest_platform != 0 && a.miss_m <= d.lethal_radius_m) {
        a.outcome = ArmOutcome::Hit;
        a.hit_platform = a.nearest_platform;
        const Platform* g = s.FindPlatform(a.hit_platform);
        s.Log(EmEventKind::Info, a.id, a.hit_platform, d.name + " bursts " + std::to_string(static_cast<int>(a.miss_m + 0.5)) +
                                                        " m from " + (g != nullptr ? g->name : std::string("?")) + ": out of action");
        DestroyPlatformIn(s, a.hit_platform);
    } else {
        a.outcome = ArmOutcome::Missed;
        const Platform* g = s.FindPlatform(a.nearest_platform);
        s.Log(EmEventKind::Info, a.id, a.nearest_platform,
              d.name + " misses" + (g != nullptr ? ": bursts " + std::to_string(static_cast<int>(a.miss_m + 0.5)) + " m from " + g->name
                                                 : std::string()));
    }
}

// Every ARM in flight: seeker, guidance, flight, fuze.
inline void RunArms(EmScene& s, double dt) {
    const Terrain& terrain = s.GetTerrain();
    for (ArmState& a : s.arms) {
        if (a.outcome != ArmOutcome::Flying) continue;
        Platform* p = s.FindPlatform(a.platform_id);
        if (p == nullptr || !p->alive) {
            a.outcome = ArmOutcome::Missed;
            continue;
        }
        const ArmDef& d = s.lib.arms[static_cast<std::size_t>(a.def_index)];
        const double flown_s = s.time_s - a.launched_s;
        const bool powered = flown_s <= d.max_flight_s;
        // The seeker.
        Vec3 measured;
        if (powered && ArmSeekerLook(s, a, d, *p, dt, measured)) {
            a.los = a.has_los ? (a.los + (measured - a.los) * d.smoothing).Normalized() : measured;
            a.has_los = true;
            a.last_heard_s = s.time_s;
            a.remembering = false;
        } else if (a.has_los && !a.remembering && d.memory && s.time_s - a.last_heard_s > 0.3) {
            a.remembering = ArmGroundPoint(s, p->position, a.los, 100000.0, a.memory_point);
        }
        // Guidance: pursue the line of sight, or fly to the remembered point; with nothing, ballistic. A lofted launch
        // is flown as one: over the first half of the expected flight the autopilot holds the flight path above the line
        // of sight, by the loft angle falling away to nothing, so the missile climbs and comes down on the radar.
        const double loft = a.loft_s > 0.0 && flown_s < a.loft_s ? d.loft_rad * (1.0 - flown_s / a.loft_s) : 0.0;
        auto lofted = [&](const Vec3& los) { return DirectionFromAzEl(AzimuthOf(los), ElevationOf(los) + loft); };
        const bool hearing = a.has_los && s.time_s - a.last_heard_s <= 0.3;
        Vec3 want;
        bool guided = false;
        if (powered && hearing) {
            want = lofted(a.los);
            guided = true;
        } else if (powered && a.remembering) {
            want = (a.memory_point - p->position).Normalized();
            guided = true;
        }
        const Vec3 before = p->position;
        if (guided) {
            const Vec3 nose = p->velocity.Normalized();
            const Vec3 dir = TurnToward(nose, want, d.max_accel_mps2 / d.speed_mps * dt);
            p->velocity = dir * d.speed_mps;
        } else if (powered && !a.has_los) {
            // Out from the launch toward the believed point, pitching over from the loft, until it hears something.
            const Vec3 nose = p->velocity.Normalized();
            const Vec3 dir = TurnToward(nose, lofted((a.aim_point - p->position).Normalized()), kArmPitchOverMps2 / d.speed_mps * dt);
            p->velocity = dir * d.speed_mps;
        } else {
            p->velocity.z -= kArmGravity * dt;  // lost or spent: it falls
        }
        p->position += p->velocity * dt;
        p->altitude_m = s.frame.AltitudeOf(p->position);
        p->height_agl_m = p->altitude_m - terrain.HeightAt(p->position.x, p->position.y);
        // The fuze: the nearest ground platform passed within the lethal radius on this tick's path, or the ground.
        const Vec3 seg = p->position - before;
        const double len2 = seg.LengthSquared();
        double best = d.lethal_radius_m;
        bool passed_close = false;
        Vec3 burst;
        for (const Platform& g : s.platforms) {
            if (!g.alive || g.id == p->id || g.id == a.shooter_platform_id) continue;
            if (g.kind != PlatformKind::Ground && g.kind != PlatformKind::Decoy) continue;
            const double u = len2 > 1e-9 ? Clamp(Dot(g.position - before, seg) / len2, 0.0, 1.0) : 0.0;
            const Vec3 q = before + seg * u;
            const double m = (g.position - q).Length();
            if (m <= best) {
                best = m;
                passed_close = true;
                burst = q;
            }
        }
        if (passed_close) {
            ArmBurst(s, a, d, burst);
        } else if (p->height_agl_m <= 0.0) {
            ArmBurst(s, a, d, p->position);
        } else if (flown_s > d.max_flight_s + 120.0) {
            ArmBurst(s, a, d, p->position);  // never came down near anything (it would have hit the ground by now)
        }
    }
}

}  // namespace em
}  // namespace simcore
