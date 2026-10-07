// simcore/em/radar_system.hpp — radars looking: calibration, looks, plots, tracks, strobes, lock.
//
// The radar equation is used in *calibrated* form. Instead of trusting a full set of transmitter,
// antenna and receiver numbers (rarely all published for a real system), each radar has a loop
// gain K fitted so that the complete model gives the published detection range against the
// published target:
//   SNR = K * sigma / R^4 * G_pattern^2 * F^4 / L_one_way^2
// The Czechoslovak army taught the same scaling, R = R_ref (sigma / sigma_ref)^(1/4)
// (Vojenské rozhledy 1989/3; docs/research/EM_RESEARCH_2026-10.md §2). The absolute numbers
// (power, gain, noise figure) are still used where they matter on their own: what an ESM receiver
// hears, and how a jammer's noise compares with the receiver's.
//
// Per tick, a rotating search radar only looks at what its beam swept this tick (InSweep), in the
// elevation beam of this revolution. Each look: path (terrain diffraction, gases, ground
// reflection), aspect-dependent RCS, MTI and clutter, noise jamming, chaff in the same cell, then
// one Swerling I draw keyed on (radar, target, look), so evaluation order never changes results.
#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/em/detection.hpp"
#include "simcore/em/model.hpp"
#include "simcore/em/scene_ops.hpp"

namespace simcore {
namespace em {

// The elevation beam a search radar uses for a target at `el` (the one containing it, else the nearest).
inline ElevationBeam BeamFor(const EmitterDef& d, double el_rad, double pointing_el_rad = 0.0) {
    if (d.beams.empty()) return {pointing_el_rad, d.antenna.beamwidth_el_rad};
    const ElevationBeam* best = &d.beams.front();
    double best_d = 1e9;
    for (const ElevationBeam& b : d.beams) {
        const double off = std::fabs(el_rad - b.centre_rad);
        if (off <= 0.5 * b.height_rad) return b;
        if (off < best_d) {
            best_d = off;
            best = &b;
        }
    }
    return *best;
}

// Antenna of a search radar in one elevation beam: same peak-gain law, beam height swapped in.
// A taller beam spreads the same power: gain falls with height (G ~ 4 pi / (theta_az theta_el)).
inline AntennaDef BeamAntenna(const EmitterDef& d, const ElevationBeam& b) {
    AntennaDef a = d.antenna;
    if (b.height_rad > 0.0 && d.antenna.beamwidth_el_rad > 0.0) {
        a.gain_db = d.antenna.gain_db - 10.0 * std::log10(b.height_rad / d.antenna.beamwidth_el_rad);
        a.beamwidth_el_rad = b.height_rad;
    }
    return a;
}

struct RadarLookPhysics {
    double range_m = 0.0;
    double rcs_m2 = 0.0;
    double pattern_db = 0.0;        // two-way pattern loss relative to the reference beam peak (<= 0)
    double one_way_loss_db = 0.0;   // diffraction + gases
    double multipath_factor = 1.0;  // one-way F
};

inline double RadarSnr(const RadarDef& d, const RadarLookPhysics& l) {
    if (l.range_m <= 0.0 || l.rcs_m2 <= 0.0) return 0.0;
    const double r2 = l.range_m * l.range_m;
    const double f2 = l.multipath_factor * l.multipath_factor;
    return d.loop_gain * l.rcs_m2 / (r2 * r2) * DbToLinear(l.pattern_db - 2.0 * l.one_way_loss_db) * f2 * f2;
}

// Physics of a look at a target at slant range R and altitude h over flat ground at sea level,
// with the radar's antenna at its deployed height (used for calibration and published-range checks).
inline RadarLookPhysics AnchorLook(const RadarDef& d, const Environment& env, double range_m, double target_alt_m,
                                   double rcs_m2) {
    RadarLookPhysics l;
    const double hr = d.antenna_height_m;
    const double dh = target_alt_m - hr;
    const double ground = range_m > std::fabs(dh) ? std::sqrt(range_m * range_m - dh * dh) : 1.0;
    const double el = std::atan(dh / ground - ground / (2.0 * env.k_factor * wgs84::kMeanRadius));
    const double f = d.emitter.waveform.freq_hz;
    const ElevationBeam beam = d.role == RadarRole::Search
                                   ? BeamFor(d.emitter, el)
                                   : ElevationBeam{MaxOf(el, d.low_target_elevation_rad), 0.0};
    const AntennaDef ant = d.role == RadarRole::Search ? BeamAntenna(d.emitter, beam) : d.emitter.antenna;
    const FlatTerrain flat(0.0);
    const PathGeometry g =
        AnalysePath(flat, Vec3{0.0, 0.0, hr}, hr, Vec3{0.0, ground, target_alt_m}, target_alt_m, env.profile_spacing_m,
                    600, env.k_factor);
    const ReflectionGeometry rg = ComputeReflection(hr, target_alt_m > 0.5 ? target_alt_m : 0.5, ground, env.k_factor);
    const double lambda = WavelengthOf(f);
    const double rho = EffectiveReflection(env, rg.grazing_rad, lambda);
    const double ratio = ReflectionGainRatio(ant, beam.centre_rad, el, rg.grazing_rad);
    l.range_m = range_m;
    l.rcs_m2 = rcs_m2;
    // Pattern relative to the reference beam's peak: a taller beam has lower gain (two-way).
    const double peak_db = ant.gain_db - d.emitter.antenna.gain_db;
    l.pattern_db = 2.0 * (AntennaGainDb(ant, 0.0, el - beam.centre_rad) - ant.gain_db + peak_db);
    l.one_way_loss_db = DiffractionLossDb(g, f) + GaseousLossDb(range_m, hr, target_alt_m, f);
    l.multipath_factor = TwoRayFactor(rg, lambda, rho, ratio);
    return l;
}

// Fit the loop gain to the radar's primary anchor. Returns false if the anchor is missing.
inline bool Calibrate(RadarDef& d, const Environment& env) {
    if (!d.calibration.valid || d.calibration.range_m <= 0.0 || d.calibration.rcs_m2 <= 0.0) return false;
    d.loop_gain = 1.0;
    const RadarLookPhysics l =
        AnchorLook(d, env, d.calibration.range_m, d.calibration.target_altitude_m, d.calibration.rcs_m2);
    const double unit = RadarSnr(d, l);
    if (unit <= 0.0) return false;
    d.loop_gain = SwerlingIRequiredSnr(d.calibration.pd, d.pfa) / unit;
    return true;
}

inline double AnchorPd(const RadarDef& d, const Environment& env, double range_m, double target_alt_m, double rcs_m2) {
    return SwerlingIPd(RadarSnr(d, AnchorLook(d, env, range_m, target_alt_m, rcs_m2)), d.pfa);
}

// First-detection range: the largest range (within max_range) at which Pd reaches `pd` for a
// target at `target_alt_m` (lobing makes Pd non-monotonic; an inbound target meets this one first).
inline double DetectionRangeAt(const RadarDef& d, const Environment& env, double target_alt_m, double rcs_m2,
                               double pd, double step_m = 50.0) {
    const double top = d.max_range_m > 0.0 ? d.max_range_m : 200000.0;
    for (double r = top; r > step_m; r -= step_m) {
        if (AnchorPd(d, env, r, target_alt_m, rcs_m2) >= pd) return r;
    }
    return 0.0;
}

// Sustained detection range: the largest range R from which an inbound target stays at Pd >= pd
// for at least `window_m` of its approach. Low fliers are seen in and out as they cross multipath
// lobes; a test range records the range from which the target was *held*, not the first glint.
inline double SustainedDetectionRange(const RadarDef& d, const Environment& env, double target_alt_m, double rcs_m2,
                                      double pd, double window_m = 2000.0, double step_m = 50.0) {
    const double top = d.max_range_m > 0.0 ? d.max_range_m : 200000.0;
    double run = 0.0;
    double start = 0.0;
    for (double r = top; r > step_m; r -= step_m) {
        if (AnchorPd(d, env, r, target_alt_m, rcs_m2) >= pd) {
            if (run == 0.0) start = r;
            run += step_m;
            if (run >= window_m) return start;
        } else {
            run = 0.0;
        }
    }
    return 0.0;
}

// ------------------------------------------------------------------------------------------ looks

struct LookResult {
    LookRecord record;
    bool detected = false;
    RadarPlot plot;
};

// One look at a scatterer. `rcs_m2` is already aspect-resolved; `radial_mps` is its radial speed.
inline LookResult EvaluateLook(EmScene& s, const RadarState& r, const RadarDef& d, const EmitterState& e,
                               const Platform& p, const Platform& t, double rcs_m2, double radial_mps,
                               const ElevationBeam& beam, double look_az, double look_el, std::uint64_t draw_key,
                               bool is_chaff = false) {
    LookResult out;
    LookRecord& rec = out.record;
    rec.platform_id = t.id;
    const Vec3 los = t.position - p.position;
    const double range = los.Length();
    const double az = AzimuthOf(los);
    const double el = s.ElevationAngle(p, t);
    rec.range_m = range;
    const double f = e.freq_hz > 0.0 ? e.freq_hz : d.emitter.waveform.freq_hz;
    const AntennaDef ant = d.role == RadarRole::Search ? BeamAntenna(d.emitter, beam) : d.emitter.antenna;
    const double grazing = PairGrazing(s, p, t);
    const LinkLossResult loss = LinkLoss(s, p, t, f, ReflectionGainRatio(ant, look_el, el, grazing));
    rec.line_of_sight = loss.line_of_sight;
    rec.diffraction_db = loss.diffraction_db;

    RadarLookPhysics phys;
    phys.range_m = range;
    phys.rcs_m2 = rcs_m2;
    const double peak_db = ant.gain_db - d.emitter.antenna.gain_db;
    phys.pattern_db = 2.0 * (AntennaGainDb(ant, WrapPi(az - look_az), el - look_el) - ant.gain_db + peak_db);
    phys.one_way_loss_db = loss.diffraction_db + loss.gas_db;
    phys.multipath_factor = loss.multipath_factor;
    double snr = RadarSnr(d, phys);

    // MTI and ground clutter.
    double cn = 0.0;
    const double ground_horizon = 4120.0 * std::sqrt(p.height_agl_m > 0.5 ? p.height_agl_m : 0.5);
    const bool in_clutter = el < d.clutter_max_elevation_rad && range < 1.3 * ground_horizon;
    if (in_clutter) cn = DbToLinear(d.clutter_cnr_db_at_10km - 30.0 * std::log10(range / 10000.0));
    if (d.has_mti && r.mti != MtiMode::Off) {
        cn *= DbToLinear(-d.mti_improvement_db);
        if (std::fabs(radial_mps) < d.mti_notch_mps) snr *= DbToLinear(-d.mti_notch_attenuation_db);
        // External coherence references the chaff's own phase: drifting chaff cancels like clutter.
        if (is_chaff && r.mti == MtiMode::External) snr *= DbToLinear(-d.mti_improvement_db);
    }
    // Chaff sharing the resolution cell.
    for (const ChaffCloud& c : s.chaff) {
        const double sc = ChaffRcs(c, s.time_s);
        if (sc <= 0.0) continue;
        const Vec3 cl = c.position - p.position;
        const double cr = cl.Length();
        if (std::fabs(cr - range) > d.range_resolution_m) continue;
        if (std::fabs(WrapPi(AzimuthOf(cl) - az)) > d.emitter.antenna.beamwidth_az_rad) continue;
        RadarLookPhysics cp = phys;
        cp.rcs_m2 = sc;
        double chaff_snr = RadarSnr(d, cp);
        if (d.has_mti && r.mti != MtiMode::Off) {
            const double vr_chaff = Dot(c.velocity, cl) / (cr > 1.0 ? cr : 1.0);
            // Internal coherence cancels only echoes that barely move; external coherence takes its
            // phase reference from the chaff itself, so wind-blown chaff cancels too, and the
            // target survives if it moves relative to the chaff (mon_2112_80 p15).
            const bool cancelled = r.mti == MtiMode::External || std::fabs(vr_chaff) < d.mti_notch_mps;
            if (cancelled) chaff_snr *= DbToLinear(-d.mti_improvement_db);
            if (r.mti == MtiMode::External && std::fabs(radial_mps - vr_chaff) < d.mti_notch_mps)
                snr *= DbToLinear(-d.mti_notch_attenuation_db);
        }
        cn += chaff_snr;
    }
    const double jn = NoiseJammingToNoise(s, p, ant, look_az, look_el, f, d.RxBandwidthHz(), d.noise_figure_db);
    const double sinr = snr / (1.0 + jn + cn);
    rec.snr_db = snr > 0.0 ? LinearToDb(snr) : -300.0;
    rec.sinr_db = sinr > 0.0 ? LinearToDb(sinr) : -300.0;
    rec.jn_db = jn > 0.0 ? LinearToDb(jn) : -300.0;
    rec.cn_db = cn > 0.0 ? LinearToDb(cn) : -300.0;
    rec.pd = SwerlingIPd(sinr, d.pfa);
    out.detected = s.rng.Uniform01Of(Stream::Detection, draw_key) < rec.pd;
    rec.detected = out.detected;
    if (!out.detected) return out;

    RadarPlot& pl = out.plot;
    pl.time_s = s.time_s;
    pl.sensor_position = p.position;
    pl.snr_db = rec.sinr_db;
    pl.sigma_range_m = RangeSigmaM(d.emitter.waveform.pulse_width_s, sinr, d.range_sigma_floor_m);
    pl.sigma_azimuth_rad = AngleSigmaRad(d.emitter.antenna.beamwidth_az_rad, sinr, d.monopulse_slope,
                                         d.angle_sigma_floor_rad);
    pl.sigma_elevation_rad = d.role == RadarRole::Search
                                 ? MaxOf(beam.height_rad / std::sqrt(12.0), d.angle_sigma_floor_rad)
                                 : AngleSigmaRad(d.emitter.antenna.beamwidth_el_rad, sinr, d.monopulse_slope,
                                                 d.angle_sigma_floor_rad);
    pl.range_m = range + pl.sigma_range_m * s.rng.NormalOf(Stream::RangeNoise, draw_key);
    pl.azimuth_rad = WrapTwoPi(az + pl.sigma_azimuth_rad * s.rng.NormalOf(Stream::AzimuthNoise, draw_key));
    // A search radar knows only which beam the echo came in: report the beam centre.
    pl.elevation_rad = d.role == RadarRole::Search
                           ? Clamp(el + pl.sigma_elevation_rad * s.rng.NormalOf(Stream::ElevationNoise, draw_key),
                                   beam.centre_rad - 0.5 * beam.height_rad, beam.centre_rad + 0.5 * beam.height_rad)
                           : el + pl.sigma_elevation_rad * s.rng.NormalOf(Stream::ElevationNoise, draw_key);
    // The radar measures the refracted (4/3-Earth) elevation; the tracker works in ENU, so correct
    // for standard refraction as height finders do: el_enu = el_app + d/(2kR) - d/(2R).
    pl.elevation_rad += range / (2.0 * s.env.k_factor * wgs84::kMeanRadius) - range / (2.0 * wgs84::kMeanRadius);
    pl.debug_truth_platform = t.id;
    return out;
}

// ------------------------------------------------------------------------------------------ system

inline std::uint64_t LookKey(int radar_id, int target_id, long long look) {
    return DrawHash(0, Stream::Detection, Key(radar_id), Key(target_id), Key(look));
}

inline void UpdateStrobeTracks(EmScene& s, RadarState& r, const Platform& p, const std::vector<JamStrobe>& fresh) {
    for (const JamStrobe& st : fresh) {
        StrobeTrack* best = nullptr;
        double best_d = 0.06;  // ~3.4 deg
        for (StrobeTrack& t : r.strobe_tracks) {
            const double d = std::fabs(WrapPi(t.azimuth_rad - st.azimuth_rad));
            if (d < best_d) {
                best_d = d;
                best = &t;
            }
        }
        if (best == nullptr) {
            StrobeTrack t;
            t.id = r.next_strobe_id++;
            t.azimuth_rad = st.azimuth_rad;
            r.strobe_tracks.push_back(t);
            best = &r.strobe_tracks.back();
            s.Log(EmEventKind::Strobe, r.id, t.id,
                  "jamming strobe at " + std::to_string(static_cast<int>(RadToDeg(st.azimuth_rad) + 0.5)) + " deg");
        } else {
            best->azimuth_rad = WrapTwoPi(best->azimuth_rad + 0.5 * WrapPi(st.azimuth_rad - best->azimuth_rad));
        }
        best->sigma_rad = st.sigma_rad;
        best->jn_db = st.jn_db;
        best->last_s = st.time_s;
        if (best->lines.empty() || st.time_s - best->lines.back().time_s >= 2.0) {
            best->lines.push_back({p.position, st.azimuth_rad, st.sigma_rad, st.time_s});
            if (best->lines.size() > 30) best->lines.erase(best->lines.begin());
        }
    }
    std::vector<StrobeTrack> kept;
    for (const StrobeTrack& t : r.strobe_tracks)
        if (s.time_s - t.last_s < 10.0) kept.push_back(t);
    r.strobe_tracks.swap(kept);
}

inline void RunSearchRadar(EmScene& s, RadarState& r, const RadarDef& d, EmitterState& e, const Platform& p) {
    if (e.revolutions != r.last_revolution) {
        r.last_revolution = e.revolutions;
        if (!r.beam_program.empty()) {
            r.program_pos = (r.program_pos + 1) % r.beam_program.size();
            e.beam_index = r.beam_program[r.program_pos];
        }
    }
    const ElevationBeam beam = d.emitter.beams.empty()
                                   ? ElevationBeam{e.pointing_el_rad, d.emitter.antenna.beamwidth_el_rad}
                                   : d.emitter.beams[static_cast<std::size_t>(e.beam_index)];
    const double half_bw = 0.5 * d.emitter.antenna.beamwidth_az_rad;
    std::vector<int> cand;
    s.grid.Query(p.position, d.max_range_m, cand);
    for (int idx : cand) {
        const Platform& t = s.platforms[static_cast<std::size_t>(idx)];
        if (t.id == p.id || !t.alive) continue;
        const double range = (t.position - p.position).Length();
        if (range > d.max_range_m || range < d.min_range_m) {
            ++s.stats.radar_candidates_culled;
            continue;
        }
        const double az = AzimuthFromTo(p.position, t.position);
        if (!InSweep(e.sweep_start_az_rad, e.swept_rad, az, half_bw)) continue;
        const double el = s.ElevationAngle(p, t);
        if (el < d.min_elevation_rad || el > d.max_elevation_rad) continue;
        if (el < beam.centre_rad - beam.height_rad || el > beam.centre_rad + beam.height_rad) continue;
        ++s.stats.radar_looks;
        const Vec3 to_radar = (p.position - t.position).Normalized();
        const double rcs = RcsToward(t.rcs, t.velocity, to_radar);
        const double vr = RadialSpeed(p.position, p.velocity, t.position, t.velocity);
        const LookResult lr = EvaluateLook(s, r, d, e, p, t, rcs, vr, beam, az, beam.centre_rad,
                                           LookKey(r.id, t.id, e.revolutions));
        r.last_looks.push_back(lr.record);
        if (lr.detected) {
            RadarPlot pl = lr.plot;
            // IFF: the interrogator shares the antenna; a working transponder of our side replies.
            if (d.has_iff && r.iff_interrogating && t.iff_reply && t.side == p.side)
                pl.iff_reply = s.rng.Uniform01Of(Stream::Iff, LookKey(r.id, t.id, e.revolutions)) < d.iff_reply_probability;
            r.last_plots.push_back(pl);
        }
    }
    // Chaff echoes are plots too (false ones).
    for (const ChaffCloud& c : s.chaff) {
        const double sc = ChaffRcs(c, s.time_s);
        if (sc <= 0.0) continue;
        const Vec3 cl = c.position - p.position;
        const double range = cl.Length();
        if (range > d.max_range_m) continue;
        const double az = AzimuthOf(cl);
        if (!InSweep(e.sweep_start_az_rad, e.swept_rad, az, half_bw)) continue;
        Platform ghost;
        ghost.id = -c.id;
        ghost.position = c.position;
        ghost.velocity = c.velocity;
        ghost.altitude_m = s.frame.AltitudeOf(c.position);
        ghost.height_agl_m = ghost.altitude_m - s.GetTerrain().HeightAt(c.position.x, c.position.y);
        const double el = s.ElevationAngle(p, ghost);
        if (el < beam.centre_rad - beam.height_rad || el > beam.centre_rad + beam.height_rad) continue;
        const double vr = RadialSpeed(p.position, p.velocity, c.position, c.velocity);
        // Evaluate the cloud without itself in the cell.
        LookResult lr;
        {
            std::vector<ChaffCloud> saved;
            saved.swap(s.chaff);
            lr = EvaluateLook(s, r, d, e, p, ghost, sc, vr, beam, az, beam.centre_rad,
                              LookKey(r.id, -c.id, e.revolutions), true);
            saved.swap(s.chaff);
        }
        if (lr.detected) {
            lr.plot.false_target = true;
            r.last_plots.push_back(lr.plot);
        }
    }
    // Noise jammers swept by the main beam show as strobes.
    std::vector<JamStrobe> fresh;
    for (const JammerState& j : s.jammers) {
        if (!j.on || j.silent_until_s > s.time_s) continue;
        const JammerDef& jd = s.lib.jammers[static_cast<std::size_t>(j.def_index)];
        if (jd.technique == JamTechnique::RangeGatePullOff) continue;
        const Platform* jp = s.FindPlatform(j.platform_id);
        if (jp == nullptr || !jp->alive) continue;
        const double az_j = AzimuthFromTo(p.position, jp->position);
        if (!InSweep(e.sweep_start_az_rad, e.swept_rad, az_j, half_bw)) continue;
        const AntennaDef ant = BeamAntenna(d.emitter, beam);
        const double jn = NoiseJammingToNoise(s, p, ant, az_j, beam.centre_rad, e.freq_hz, d.RxBandwidthHz(),
                                              d.noise_figure_db);
        if (jn <= 0.0 || LinearToDb(jn) < d.strobe_jn_threshold_db) continue;
        JamStrobe st;
        st.time_s = s.time_s;
        st.sigma_rad = AngleSigmaRad(d.emitter.antenna.beamwidth_az_rad, jn, d.monopulse_slope, d.angle_sigma_floor_rad);
        st.azimuth_rad =
            WrapTwoPi(az_j + st.sigma_rad * s.rng.NormalOf(Stream::AzimuthNoise, LookKey(r.id, -1000 - j.id, e.revolutions)));
        st.jn_db = LinearToDb(jn);
        fresh.push_back(st);
        r.strobes.push_back(st);
    }
    std::vector<JamStrobe> recent;
    for (const JamStrobe& st : r.strobes)
        if (s.time_s - st.time_s < 4.0) recent.push_back(st);
    r.strobes.swap(recent);
    UpdateStrobeTracks(s, r, p, fresh);

    // Track-while-scan: only tracks predicted inside this tick's sweep and beam can miss.
    const double sweep_start = e.sweep_start_az_rad, swept = e.swept_rad;
    const double max_range = d.max_range_m;
    // Only a track inside the active beam's 3 dB height was really looked at.
    const double el_lo = beam.centre_rad - 0.5 * beam.height_rad, el_hi = beam.centre_rad + 0.5 * beam.height_rad;
    const Vec3 pos = p.position;
    const double k_re = s.env.k_factor * wgs84::kMeanRadius;
    const std::vector<int> changed =
        r.tracker.ProcessLook(s.time_s, r.last_plots, [&](const SensorTrack& t) {
            const Vec3 rel = t.Position() - pos;
            const double ground = rel.HorizontalLength();
            const double el = std::atan2(rel.z, ground > 1.0 ? ground : 1.0) + ground / (2.0 * wgs84::kMeanRadius) -
                              ground / (2.0 * k_re);
            return rel.Length() <= max_range && InSweep(sweep_start, swept, AzimuthOf(rel), half_bw) && el >= el_lo &&
                   el <= el_hi;
        });
    s.stats.plots += r.last_plots.size();
    for (int id : changed) {
        const SensorTrack* t = r.tracker.Find(id);
        if (t != nullptr && t->status == TrackStatus::Confirmed) {
            s.Log(EmEventKind::TrackConfirmed, r.id, id, "track " + std::to_string(id) + " confirmed");
        } else {
            s.Log(EmEventKind::TrackDropped, r.id, id, "track " + std::to_string(id) + " dropped");
        }
    }
}

// A single-target tracking radar: points at its designation (or its locked track), looks only in
// its beam and range gate, and keeps the lock while plots keep coming. A repeater jammer on the
// target can capture the range gate and walk it off (range-gate pull-off), then fall silent so
// the gate sits on empty space and the lock breaks.
inline void RunTrackRadar(EmScene& s, RadarState& r, const RadarDef& d, EmitterState& e, const Platform& p, double dt) {
    const SensorTrack* lt = r.locked_track != 0 ? r.tracker.Find(r.locked_track) : nullptr;
    if (lt == nullptr && r.locked_track != 0) r.locked_track = 0;
    Vec3 aim;
    if (lt != nullptr) {
        SensorTrack pred = *lt;
        PredictTrack(pred, s.time_s, d.tracker.process_noise_accel);
        aim = pred.Position();
    } else if (r.has_designation) {
        aim = r.designation;
    } else {
        return;  // nothing to point at
    }
    // Slew the antenna towards the aim point (apparent elevation over a 4/3 Earth).
    const Vec3 rel = aim - p.position;
    const double want_az = AzimuthOf(rel);
    const double ground = rel.HorizontalLength();
    const double want_el = std::atan2(rel.z, ground > 1.0 ? ground : 1.0) + ground / (2.0 * wgs84::kMeanRadius) -
                           ground / (2.0 * s.env.k_factor * wgs84::kMeanRadius);
    const double max_step = d.max_slew_rad_s * dt;
    // Low-target mode: hold the antenna above the ground reflection (mon_2112_80 p17).
    const double held_el = MaxOf(want_el, d.low_target_elevation_rad);
    e.pointing_az_rad = WrapTwoPi(e.pointing_az_rad + Clamp(WrapPi(want_az - e.pointing_az_rad), -max_step, max_step));
    e.pointing_el_rad += Clamp(held_el - e.pointing_el_rad, -2.0 * max_step, 2.0 * max_step);
    const double aim_range = rel.Length();
    const double gate = d.track_gate_m;
    const ElevationBeam beam{e.pointing_el_rad, d.emitter.antenna.beamwidth_el_rad};
    const Vec3 boresight = DirectionFromAzEl(e.pointing_az_rad, e.pointing_el_rad);
    ++r.looks;

    // A repeater answers when the beam illuminates the platform it sits on.
    const JammerState* rgpo = nullptr;
    for (const JammerState& j : s.jammers) {
        if (!j.on || j.silent_until_s > s.time_s) continue;
        if (s.lib.jammers[static_cast<std::size_t>(j.def_index)].technique != JamTechnique::RangeGatePullOff) continue;
        if (j.victim_radar != 0 && j.victim_radar != r.id) continue;
        const Platform* jp = s.FindPlatform(j.platform_id);
        if (jp == nullptr || !jp->alive) continue;
        if (AngleBetween(jp->position - p.position, boresight) <= 1.5 * d.emitter.antenna.beamwidth_az_rad) rgpo = &j;
    }

    std::vector<int> cand;
    s.grid.Query(p.position, d.max_range_m, cand);
    LookResult best;
    bool have = false;
    for (int idx : cand) {
        const Platform& t = s.platforms[static_cast<std::size_t>(idx)];
        if (t.id == p.id || !t.alive) continue;
        const Vec3 tl = t.position - p.position;
        const double range = tl.Length();
        if (range > d.max_range_m || range < d.min_range_m) continue;
        if (AngleBetween(tl, boresight) > 1.5 * d.emitter.antenna.beamwidth_az_rad) continue;
        // While a repeater holds the gate, the echo in the gate is its false one, `offset` behind
        // the skin echo; once it falls silent only the skin echo is left.
        const bool false_echo = r.rgpo_captured && rgpo != nullptr && t.id == rgpo->platform_id;
        if (std::fabs(range + (false_echo ? r.rgpo_offset_m : 0.0) - aim_range) > gate) continue;
        ++s.stats.radar_looks;
        const double rcs = RcsToward(t.rcs, t.velocity, (p.position - t.position).Normalized());
        const double vr = RadialSpeed(p.position, p.velocity, t.position, t.velocity);
        const LookResult lr = EvaluateLook(s, r, d, e, p, t, rcs, vr, beam, e.pointing_az_rad, e.pointing_el_rad,
                                           LookKey(r.id, t.id, r.looks));
        r.last_looks.push_back(lr.record);
        if (lr.detected && (!have || lr.record.sinr_db > best.record.sinr_db)) {
            best = lr;
            have = true;
        }
    }

    if (rgpo != nullptr && have && best.record.platform_id == rgpo->platform_id) {
        const JammerDef& jd = s.lib.jammers[static_cast<std::size_t>(rgpo->def_index)];
        const Platform* jp = s.FindPlatform(rgpo->platform_id);
        const EmitterState* je = s.FindEmitter(rgpo->emitter_id);
        if (jp != nullptr && je != nullptr) {
            // Repeater-to-skin ratio: one-way repeated power vs the two-way echo, both relative to noise.
            const double back_az = AzimuthFromTo(jp->position, p.position);
            const double eirp = EirpW(jd.emitter, WrapPi(back_az - je->pointing_az_rad), 0.0);
            const LinkLossResult loss = LinkLoss(s, *jp, p, e.freq_hz);
            const double pj = OneWayReceivedPowerW(eirp, d.emitter.antenna.gain_db, e.freq_hz, loss.range_m,
                                                   loss.TotalOneWayDb());
            const double jn = JammingToNoise(pj, d.noise_figure_db, d.RxBandwidthHz());
            const double sn = DbToLinear(best.record.snr_db);
            const double js_db = LinearToDb((jn > 1e-30 ? jn : 1e-30) / (sn > 1e-12 ? sn : 1e-12));
            if (!r.rgpo_captured && js_db >= jd.rgpo_js_needed_db &&
                s.rng.Uniform01Of(Stream::Detection, LookKey(r.id, -2000 - rgpo->id, r.looks)) < d.rgpo_susceptibility) {
                r.rgpo_captured = true;
                r.rgpo_offset_m = 0.0;
                s.Log(EmEventKind::Info, r.id, rgpo->id, "range gate captured by a repeater");
            }
            if (r.rgpo_captured) {
                r.rgpo_offset_m += jd.rgpo_walk_mps * dt;
                best.plot.range_m += r.rgpo_offset_m;
                best.plot.false_target = true;
                if (r.rgpo_offset_m >= jd.rgpo_max_m) {
                    for (JammerState& j : s.jammers)
                        if (j.id == rgpo->id) j.silent_until_s = s.time_s + jd.rgpo_off_s;
                    s.Log(EmEventKind::Info, r.id, rgpo->id, "repeater falls silent, leaving the gate off target");
                }
            }
        }
    } else if (r.rgpo_captured && rgpo == nullptr && have) {
        // The repeater fell silent before the gate left the skin echo: the radar keeps the target.
        r.rgpo_captured = false;
        r.rgpo_offset_m = 0.0;
    }

    if (have) {
        r.last_plots.push_back(best.plot);
        const int before = r.locked_track;
        r.locked_track = r.tracker.UpdateSingle(r.locked_track, best.plot);
        if (before == 0) s.Log(EmEventKind::LockOn, r.id, r.locked_track, "lock on");
        r.has_designation = false;
    } else if (r.locked_track != 0) {
        if (!r.tracker.MissSingle(r.locked_track, s.time_s)) {
            s.Log(EmEventKind::LockLost, r.id, r.locked_track, "lock lost");
            r.locked_track = 0;
            r.rgpo_captured = false;
            r.rgpo_offset_m = 0.0;
        }
    }
}

inline void RunRadars(EmScene& s, double dt) {
    for (RadarState& r : s.radars) {
        r.last_looks.clear();
        r.last_plots.clear();
        const Platform* p = s.FindPlatform(r.platform_id);
        EmitterState* e = s.FindEmitter(r.emitter_id);
        if (p == nullptr || e == nullptr) continue;
        if (!p->alive) r.on = false;
        e->radiating = r.on;
        if (!r.on || !e->InAir()) {
            r.tracker.PredictAll(s.time_s);
            continue;
        }
        const RadarDef& d = s.lib.radars[static_cast<std::size_t>(r.def_index)];
        if (d.role == RadarRole::Search) {
            RunSearchRadar(s, r, d, *e, *p);
        } else {
            RunTrackRadar(s, r, d, *e, *p, dt);
        }
    }
}

}  // namespace em
}  // namespace simcore
