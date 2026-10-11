// simcore/em/scene_ops.hpp — per-tick housekeeping and the propagation of one link in the scene.
//
//   * UpdatePlatforms: altitude and height above ground, spatial grid.
//   * UpdateEmitters: rotate scanning antennas, point jammers.
//   * LinkLoss: everything between two platforms at a frequency (terrain diffraction, gases,
//     ground reflection), from the cached path geometry. Radar, ESM, jamming and radio all use it.
//   * NoiseJammingToNoise: summed J/N of the active noise jammers in one receiver look.
#pragma once

#include <cmath>
#include <vector>

#include "simcore/em/detection.hpp"
#include "simcore/em/model.hpp"

namespace simcore {
namespace em {

inline void UpdatePlatforms(EmScene& s) {
    const Terrain& terrain = s.GetTerrain();
    for (Platform& p : s.platforms) {
        p.altitude_m = s.frame.AltitudeOf(p.position);
        p.height_agl_m = p.altitude_m - terrain.HeightAt(p.position.x, p.position.y);
    }
    s.grid.Rebuild(s.platforms);
}

inline void UpdateEmitters(EmScene& s, double dt) {
    for (EmitterState& e : s.emitters) {
        const Platform* p = s.FindPlatform(e.platform_id);
        if (p == nullptr || !p->alive) {
            e.radiating = false;
            continue;
        }
        if (e.def.scan == ScanKind::Circular && e.def.scan_period_s > 0.0) {
            e.sweep_start_az_rad = e.pointing_az_rad;
            e.swept_rad = kTwoPi * dt / e.def.scan_period_s;
            const double next = e.pointing_az_rad + e.swept_rad;
            if (next >= kTwoPi) ++e.revolutions;
            e.pointing_az_rad = WrapTwoPi(next);
        } else if (e.def.scan == ScanKind::Sector && e.def.scan_period_s > 0.0) {
            // Back-and-forth over a sector centred on the platform's heading (an airborne radar).
            const double heading = p->velocity.HorizontalLength() > 1.0 ? AzimuthOf(p->velocity) : e.pointing_az_rad;
            auto offset_at = [&](double t) {
                double ph = std::fmod(t, e.def.scan_period_s) / e.def.scan_period_s;
                if (ph < 0.0) ph += 1.0;
                const double tri = ph < 0.5 ? 2.0 * ph : 2.0 - 2.0 * ph;
                return (tri - 0.5) * e.def.sector_width_rad;
            };
            const double a0 = offset_at(s.time_s - dt), a1 = offset_at(s.time_s);
            e.sweep_start_az_rad = WrapTwoPi(heading + (a1 >= a0 ? a0 : a1));
            e.swept_rad = std::fabs(a1 - a0);
            e.pointing_az_rad = WrapTwoPi(heading + a1);
            if (a1 < a0 && offset_at(s.time_s - 2.0 * dt) <= a0) ++e.revolutions;
        } else {
            e.sweep_start_az_rad = e.pointing_az_rad;
            e.swept_rad = 0.0;
        }
    }
    for (JammerState& j : s.jammers) {
        EmitterState* e = s.FindEmitter(j.emitter_id);
        const Platform* p = s.FindPlatform(j.platform_id);
        if (e == nullptr || p == nullptr) continue;
        const bool silent = j.silent_until_s > s.time_s;
        e->radiating = j.on && p->alive && !silent;
        if (j.aim_at_point) {
            const Vec3 d = j.aim_point - p->position;
            e->pointing_az_rad = AzimuthOf(d);
            e->pointing_el_rad = ElevationOf(d);
        }
    }
}

struct LinkLossResult {
    bool line_of_sight = false;
    double range_m = 0.0;
    double diffraction_db = 0.0;
    double gas_db = 0.0;
    double multipath_factor = 1.0;  // one-way amplitude factor F
    double TotalOneWayDb() const { return diffraction_db + gas_db - 20.0 * std::log10(multipath_factor); }
};

// One-way propagation for a path whose profile is known: diffraction, gases and the ground.
//   h1, h2: antenna heights above the local ground; alt1, alt2: altitudes (for the gas loss).
// Below 1 GHz (radios near the ground) the excess loss over free space is the larger of the
// plane-earth loss and the knife-edge loss, not their sum: a ridge that lifts the path off the
// ground also removes most of the grazing ground loss (the 'obstacle gain' of VHF over hills is
// not modelled, so this stays on the conservative side). Bullington's rule of thumb.
inline LinkLossResult LinkLossCore(const Environment& env, const PathGeometry& g, double h1, double h2, double alt1,
                                   double alt2, double range_m, double freq_hz, double gain_ratio = 1.0) {
    LinkLossResult r;
    r.line_of_sight = g.line_of_sight;
    r.range_m = range_m;
    r.diffraction_db = DiffractionLossDb(g, freq_hz);
    r.gas_db = GaseousLossDb(range_m, alt1, alt2, freq_hz);
    const double lambda = WavelengthOf(freq_hz);
    if (freq_hz < 1e9) {
        const double pe = PlaneEarthFactor(h1, h2, g.ground_range_m, lambda);
        const double pe_db = -20.0 * std::log10(pe);
        if (pe_db >= r.diffraction_db) {
            r.multipath_factor = pe;
            r.diffraction_db = 0.0;
        } else {
            r.multipath_factor = 1.0;
        }
    } else {
        const ReflectionGeometry rg =
            ComputeReflection(h1 > 0.5 ? h1 : 0.5, h2 > 0.5 ? h2 : 0.5, g.ground_range_m, env.k_factor);
        const double rho = EffectiveReflection(env, rg.grazing_rad, lambda);
        r.multipath_factor = TwoRayFactor(rg, lambda, rho, gain_ratio);
    }
    return r;
}

// One-way propagation between platforms a and b at a frequency. `gain_ratio` is the amplitude
// ratio of a's antenna gain towards the ground reflection to its gain towards b (1 = broad beam).
inline LinkLossResult LinkLoss(EmScene& s, const Platform& a, const Platform& b, double freq_hz,
                               double gain_ratio = 1.0) {
    const bool ab = a.id < b.id;
    const Platform& lo = ab ? a : b;
    const Platform& hi = ab ? b : a;
    const PathGeometry& g = s.paths.Get(PathCache::PairKey(lo.id, hi.id), s.GetTerrain(), lo.position, lo.altitude_m,
                                        hi.position, hi.altitude_m, s.env.profile_spacing_m);
    return LinkLossCore(s.env, g, a.height_agl_m, b.height_agl_m, a.altitude_m, b.altitude_m,
                        (b.position - a.position).Length(), freq_hz, gain_ratio);
}

// Amplitude ratio of an antenna's elevation pattern: towards the ground reflection vs the target.
inline double ReflectionGainRatio(const AntennaDef& ant, double boresight_el_rad, double target_el_rad,
                                  double grazing_rad) {
    if (ant.beamwidth_el_rad <= 0.0) return 1.0;
    const double g_direct = AntennaGainDb(ant, 0.0, target_el_rad - boresight_el_rad);
    const double g_refl = AntennaGainDb(ant, 0.0, -grazing_rad - boresight_el_rad);
    return std::pow(10.0, (g_refl - g_direct) / 20.0);
}

// Grazing angle for a pair (for the reflection gain ratio above).
inline double PairGrazing(const EmScene& s, const Platform& a, const Platform& b) {
    const double h1 = a.height_agl_m > 0.5 ? a.height_agl_m : 0.5;
    const double h2 = b.height_agl_m > 0.5 ? b.height_agl_m : 0.5;
    return ComputeReflection(h1, h2, GroundRange(a.position, b.position), s.env.k_factor).grazing_rad;
}

// Sum of J/N from active noise jammers in one look of a receiver on platform `rx` whose antenna
// points at (look_az, look_el), at frequency/bandwidth (freq_hz, bw_hz). Repeater (deception)
// jammers are not noise and are handled by the radar's range gate.
inline double NoiseJammingToNoise(EmScene& s, const Platform& rx, const AntennaDef& rx_ant, double look_az,
                                  double look_el, double freq_hz, double bw_hz, double noise_figure_db,
                                  double extra_noise_db = 0.0) {
    double sum = 0.0;
    for (const JammerState& j : s.jammers) {
        if (!j.on || j.silent_until_s > s.time_s) continue;
        const JammerDef& jd = s.lib.jammers[static_cast<std::size_t>(j.def_index)];
        if (jd.technique == JamTechnique::RangeGatePullOff) continue;
        const Platform* jp = s.FindPlatform(j.platform_id);
        const EmitterState* je = s.FindEmitter(j.emitter_id);
        if (jp == nullptr || je == nullptr || !jp->alive || jp->id == rx.id) continue;
        const double overlap =
            JammerBandOverlap(jd.emitter.waveform.freq_hz, jd.emitter.waveform.bandwidth_hz, freq_hz, bw_hz);
        if (overlap <= 0.0) continue;
        const double az_j = AzimuthFromTo(rx.position, jp->position);
        const double el_j = s.ElevationAngle(rx, *jp);
        const double g_rx = AntennaGainDb(rx_ant, WrapPi(az_j - look_az), el_j - look_el);
        const double az_back = AzimuthFromTo(jp->position, rx.position);
        const double el_back = s.ElevationAngle(*jp, rx);
        const double eirp = EirpW(jd.emitter, WrapPi(az_back - je->pointing_az_rad), el_back - je->pointing_el_rad);
        const LinkLossResult loss = LinkLoss(s, *jp, rx, freq_hz);
        const double pj = OneWayReceivedPowerW(eirp, g_rx, freq_hz, loss.range_m, loss.TotalOneWayDb()) * overlap;
        sum += JammingToNoise(pj, noise_figure_db + extra_noise_db, bw_hz);
    }
    return sum;
}

}  // namespace em
}  // namespace simcore
