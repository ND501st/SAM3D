// simcore/em/esm_system.hpp — passive receivers: RWR, ESM, ELINT and TDOA sites.
//
// A receiver hears an emitter when the one-way signal at its antenna beats its sensitivity:
//   Pr = EIRP(towards the receiver) * G_rx * lambda^2 / (4 pi R)^2 * path factors
// One-way beats two-way: a radar is heard far beyond the range at which it can see. Whether the
// main beam or only a sidelobe points at the receiver matters: a rotating search radar paints an
// RWR once per turn with its main beam (scan-on-scan), while a sensitive ELINT receiver also hears
// its sidelobes continuously; a tracking radar locked on illuminates its target steadily.
//
// What a receiver gets is an intercept: bearing (with error), power, and measured carrier, PRF and
// pulse width — never the emitter's identity. Intercepts are associated into contacts by those
// parameters, and a contact is identified (or not) against the receiver's threat library, which
// holds what *its side* believes about the other side's emitters (Wiley, "ELINT: The Interception
// and Analysis of Radar Signals", 2006; Adamy, "EW 101", ch. 5–8).
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

#include "simcore/em/detection.hpp"
#include "simcore/em/geolocation.hpp"
#include "simcore/em/model.hpp"
#include "simcore/em/radar_system.hpp"
#include "simcore/em/scene_ops.hpp"

namespace simcore {
namespace em {

// How well a measured value fits a library window [lo, hi]: 1 inside, Gaussian fall-off outside
// with a tolerance of `tol_frac` of the window edge. A window of 0/0 means "not specified".
inline double ParamMatch(double v, double lo, double hi, double tol_frac) {
    if (lo <= 0.0 && hi <= 0.0) return 1.0;
    if (v >= lo && v <= hi) return 1.0;
    const double edge = v < lo ? lo : hi;
    const double tol = tol_frac * (edge > 0.0 ? edge : 1.0);
    const double z = (v - edge) / (tol > 1e-12 ? tol : 1e-12);
    return std::exp(-0.5 * z * z);
}

struct LibraryMatch {
    const ThreatEntry* entry = nullptr;
    double confidence = 0.0;
};

// Score every entry and normalise against an "unknown emitter" prior, so a signal that fits
// nothing well stays unknown instead of being forced onto the nearest entry.
inline LibraryMatch MatchLibrary(const ThreatLibrary& lib, double freq_hz, double prf_hz, double pw_s) {
    LibraryMatch m;
    constexpr double kUnknownPrior = 0.05;
    double sum = 0.0, best = 0.0;
    for (const ThreatEntry& e : lib.entries) {
        double sc = ParamMatch(freq_hz, e.freq_min_hz, e.freq_max_hz, 0.02);
        if (e.cw && prf_hz > 0.0) sc *= 0.01;                               // pulses heard, CW/noise expected
        else if (!e.cw && prf_hz <= 0.0 && e.prf_max_hz > 0.0) sc *= 0.01;  // CW heard, pulses expected
        else sc *= ParamMatch(prf_hz, e.prf_min_hz, e.prf_max_hz, 0.05);
        sc *= ParamMatch(pw_s, e.pw_min_s, e.pw_max_s, 0.25);
        sum += sc;
        if (sc > best) {
            best = sc;
            m.entry = &e;
        }
    }
    if (m.entry == nullptr || best < 0.3) {
        m.entry = nullptr;
        m.confidence = 0.0;
        return m;
    }
    m.confidence = best / (sum + kUnknownPrior);
    return m;
}

inline void Identify(const EmScene& s, const EsmDef& def, Side my_side, EsmContact& c) {
    const ThreatLibrary* lib = s.lib.Library(def.threat_library);
    if (lib == nullptr) return;
    const LibraryMatch m = MatchLibrary(*lib, c.sig.freq_hz, c.sig.prf_hz, c.sig.pulse_width_s);
    if (m.entry == nullptr) {
        c.sig.library_id.clear();
        c.sig.library_name.clear();
        c.sig.library_confidence = 0.0;
        c.sig.hostile = false;
        c.sig.max_speed_mps = 0.0;
        return;
    }
    c.sig.library_id = m.entry->id;
    c.sig.library_name = m.entry->name;
    c.sig.library_confidence = m.confidence;
    c.sig.hostile = m.entry->owner != Side::Neutral && m.entry->owner != my_side;
    c.sig.max_speed_mps = m.entry->max_speed_mps;
}

// Where the emitter's antenna points relative to the receiver, and whether that is its main beam.
struct IlluminationGeometry {
    double off_az_rad = 0.0;
    double off_el_rad = 0.0;
    bool main_beam = false;
    AntennaDef antenna;
};

// `window_s`: how long since this receiver last listened. A rotating beam that painted the
// receiver at any moment in that window counts as a main-beam intercept.
inline IlluminationGeometry Illumination(const EmScene& s, const EmitterState& e, const Platform& tx,
                                         const Platform& rx, double window_s = 0.0) {
    IlluminationGeometry g;
    const EmitterDef& d = e.def;
    const double az = AzimuthFromTo(tx.position, rx.position);
    const double el = s.ElevationAngle(tx, rx);
    g.antenna = d.antenna;
    double centre_el = e.pointing_el_rad;
    if (!d.beams.empty()) {
        const ElevationBeam& b = d.beams[static_cast<std::size_t>(e.beam_index) < d.beams.size()
                                             ? static_cast<std::size_t>(e.beam_index)
                                             : 0];
        g.antenna = BeamAntenna(d, b);
        centre_el = b.centre_rad;
    }
    switch (d.scan) {
        case ScanKind::Omni:
            g.main_beam = true;
            return g;
        case ScanKind::Circular: {
            double start = e.sweep_start_az_rad, covered = e.swept_rad;
            if (window_s > 0.0 && d.scan_period_s > 0.0) {
                covered = kTwoPi * window_s / d.scan_period_s;
                if (covered > kTwoPi) covered = kTwoPi;
                start = WrapTwoPi(e.pointing_az_rad - covered);
            }
            const bool swept = InSweep(start, covered, az, 0.5 * d.antenna.beamwidth_az_rad);
            g.off_az_rad = swept ? 0.0 : WrapPi(az - e.pointing_az_rad);
            g.off_el_rad = el - centre_el;
            break;
        }
        default:
            g.off_az_rad = WrapPi(az - e.pointing_az_rad);
            g.off_el_rad = el - centre_el;
            break;
    }
    const double hw_az = d.antenna.beamwidth_az_rad > 0.0 ? 0.5 * d.antenna.beamwidth_az_rad : kPi;
    const double hw_el = g.antenna.beamwidth_el_rad > 0.0 ? 0.5 * g.antenna.beamwidth_el_rad : kPi;
    g.main_beam = std::fabs(g.off_az_rad) <= hw_az && std::fabs(g.off_el_rad) <= hw_el;
    return g;
}

inline void UpdateContact(const EmScene& s, std::vector<EmEvent>& log, const EsmDef& def, const EsmState& esm, Side side, const Platform& rx,
                          EsmContact& c, const EsmIntercept& i, double dt) {
    const bool first = c.intercepts == 0;
    if (first) {
        c.bearing_rad = i.bearing_rad;
        c.bearing_sigma_rad = i.bearing_sigma_rad;
        c.sig.freq_hz = i.freq_hz;
        c.sig.prf_hz = i.prf_hz;
        c.sig.pulse_width_s = i.pulse_width_s;
        c.first_seen_s = i.time_s;
    } else {
        // Smooth the measurements; the bearing keeps up with a moving emitter or receiver.
        c.bearing_rad = WrapTwoPi(c.bearing_rad + 0.5 * WrapPi(i.bearing_rad - c.bearing_rad));
        c.bearing_sigma_rad = std::sqrt(0.5) * i.bearing_sigma_rad;
        c.sig.freq_hz += 0.2 * (i.freq_hz - c.sig.freq_hz);
        c.sig.prf_hz += 0.2 * (i.prf_hz - c.sig.prf_hz);
        c.sig.pulse_width_s += 0.2 * (i.pulse_width_s - c.sig.pulse_width_s);
    }
    c.power_dbw = i.power_dbw;
    c.last_seen_s = i.time_s;
    ++c.intercepts;
    if (i.main_beam) {
        const double gap = c.last_main_beam_s >= 0.0 ? i.time_s - c.last_main_beam_s : -1.0;
        if (gap > 0.0 && gap <= 1.5 * dt + 1e-9) {
            if (c.steady_since_s < 0.0) c.steady_since_s = c.last_main_beam_s;
        } else {
            c.steady_since_s = -1.0;
            if (gap > 0.35) c.scan_period_est_s = c.scan_period_est_s > 0.0 ? 0.7 * c.scan_period_est_s + 0.3 * gap : gap;
        }
        c.last_main_beam_s = i.time_s;
    } else if (i.time_s - c.last_main_beam_s > 1.5 * dt) {
        c.steady_since_s = -1.0;
    }
    c.sig.steady = c.steady_since_s >= 0.0 && i.time_s - c.steady_since_s >= 1.0;
    if (i.time_s - c.last_line_s >= def.line_interval_s) {
        c.lines.push_back({rx.position, i.bearing_rad, i.bearing_sigma_rad, i.time_s});
        if (c.lines.size() > 40) c.lines.erase(c.lines.begin());
        c.last_line_s = i.time_s;
    }
    c.debug_truth_emitter = i.debug_truth_emitter;
    Identify(s, def, side, c);
    if (first) {
        log.push_back({s.time_s, EmEventKind::EsmNewContact, esm.id, c.id,
              std::string(EsmKindName(def.kind)) + " contact " + std::to_string(c.id) + " at " +
                  std::to_string(static_cast<int>(RadToDeg(c.bearing_rad) + 0.5)) + " deg: " +
                  (c.sig.library_name.empty() ? std::string("unknown emitter") : c.sig.library_name)});
    }
}

// What one site's pass produces besides its own state: counters and events, merged in site order afterwards.
struct EsmWork {
    std::vector<EmEvent> events;
    std::uint64_t culled = 0, evaluations = 0, intercepts = 0;
};

inline void RunEsmSite(const EmScene& s, EsmState& esm, double dt, EsmWork& work) {
    {
        esm.intercepts.clear();
        const Platform* rx = s.FindPlatform(esm.platform_id);
        if (rx == nullptr || !rx->alive || !esm.on) return;
        const EsmDef& def = s.lib.esm[static_cast<std::size_t>(esm.def_index)];
        const double sens_w = DbToLinear(def.sensitivity_dbw);
        for (const EmitterState& e : s.emitters) {
            if (!e.InAir() || e.platform_id == rx->id) continue;
            const Platform* tx = s.FindPlatform(e.platform_id);
            if (tx == nullptr || !tx->alive) continue;
            const double f = e.freq_hz > 0.0 ? e.freq_hz : e.def.waveform.freq_hz;
            if (f < def.freq_min_hz || f > def.freq_max_hz) continue;
            const double range = (rx->position - tx->position).Length();
            // Cheap bound first: even the main beam in free space can't reach the sensitivity.
            const double peak_eirp = e.def.peak_power_w * DbToLinear(e.def.antenna.gain_db - e.def.tx_loss_db);
            if (OneWayReceivedPowerW(peak_eirp, def.antenna_gain_db, f, range, 0.0) < sens_w) {
                ++work.culled;
                continue;
            }
            ++work.evaluations;
            const IlluminationGeometry ill = Illumination(s, e, *tx, *rx, dt);
            const double eirp =
                e.def.peak_power_w * DbToLinear(AntennaGainDb(ill.antenna, ill.off_az_rad, ill.off_el_rad) - e.def.tx_loss_db);
            const LinkLossResult loss = LinkLossCached(s, esm.paths, *tx, *rx, f);
            const double pr = OneWayReceivedPowerW(eirp, def.antenna_gain_db, f, range, loss.TotalOneWayDb());
            if (pr < sens_w) continue;
            const std::uint64_t key = DrawHash(0, Stream::EsmBearing, Key(esm.id), Key(e.id), Key(s.tick));
            const double margin = pr / sens_w;
            EsmIntercept in;
            in.time_s = s.time_s;
            in.power_dbw = LinearToDb(pr);
            in.main_beam = ill.main_beam;
            in.bearing_sigma_rad = def.bearing_sigma_rad * std::sqrt(1.0 + 1.0 / margin);
            in.bearing_rad =
                WrapTwoPi(AzimuthFromTo(rx->position, tx->position) + in.bearing_sigma_rad * s.rng.NormalOf(Stream::EsmBearing, key));
            in.freq_hz = f * (1.0 + def.freq_sigma_fraction * s.rng.NormalOf(Stream::EsmFrequency, key));
            in.prf_hz = e.def.waveform.prf_hz > 0.0
                            ? e.def.waveform.prf_hz * (1.0 + def.pri_sigma_fraction * s.rng.NormalOf(Stream::EsmPri, key))
                            : 0.0;
            in.pulse_width_s =
                e.def.waveform.pulse_width_s > 0.0
                    ? e.def.waveform.pulse_width_s * (1.0 + def.pw_sigma_fraction * s.rng.NormalOf(Stream::EsmPulseWidth, key))
                    : 0.0;
            in.arrival_time_s = range / kSpeedOfLight + def.time_sigma_s * s.rng.NormalOf(Stream::TdoaNoise, key);
            in.debug_truth_emitter = e.id;
            esm.intercepts.push_back(in);
            ++work.intercepts;
        }
        // Parametric association of this tick's intercepts with the contacts. Gates are 4 sigma of
        // the difference between a new measurement and a contact's smoothed value.
        const Side side = rx->side;
        const double ftol = MaxOf(4.0 * std::sqrt(2.0) * def.freq_sigma_fraction, 0.005);
        const double ptol = MaxOf(4.0 * std::sqrt(2.0) * def.pri_sigma_fraction, 0.01);
        for (const EsmIntercept& in : esm.intercepts) {
            EmitterSignature sig;
            sig.freq_hz = in.freq_hz;
            sig.prf_hz = in.prf_hz;
            sig.pulse_width_s = in.pulse_width_s;
            EsmContact* best = nullptr;
            double best_d = 1e9;
            for (EsmContact& c : esm.contacts) {
                EmitterSignature raw = c.sig;
                raw.library_id.clear();  // compare measurements, not the identification
                if (!SameEmitter(raw, sig, ftol, ptol)) continue;
                const double gate = MaxOf(4.0 * MaxOf(c.bearing_sigma_rad, in.bearing_sigma_rad), 0.12);
                const double d = std::fabs(WrapPi(c.bearing_rad - in.bearing_rad));
                if (d < gate && d < best_d) {
                    best_d = d;
                    best = &c;
                }
            }
            if (best == nullptr) {
                EsmContact c;
                c.id = esm.next_contact_id++;
                esm.contacts.push_back(c);
                best = &esm.contacts.back();
            }
            UpdateContact(s, work.events, def, esm, side, *rx, *best, in, dt);
        }
        // Contacts that drifted together are one emitter: merge into the older one.
        for (std::size_t i = 0; i < esm.contacts.size(); ++i)
            for (std::size_t j = i + 1; j < esm.contacts.size();) {
                EsmContact& a = esm.contacts[i];
                const EsmContact& b = esm.contacts[j];
                EmitterSignature sa = a.sig, sb = b.sig;
                sa.library_id.clear();
                sb.library_id.clear();
                const double gate = MaxOf(3.0 * MaxOf(a.bearing_sigma_rad, b.bearing_sigma_rad), 0.08);
                if (SameEmitter(sa, sb, ftol, ptol) && std::fabs(WrapPi(a.bearing_rad - b.bearing_rad)) < gate) {
                    std::vector<BearingLine> lines = a.lines;
                    lines.insert(lines.end(), b.lines.begin(), b.lines.end());
                    std::sort(lines.begin(), lines.end(),
                              [](const BearingLine& x, const BearingLine& y) { return x.time_s < y.time_s; });
                    if (lines.size() > 40) lines.erase(lines.begin(), lines.end() - 40);
                    a.lines = lines;
                    a.intercepts += b.intercepts;
                    if (b.last_seen_s > a.last_seen_s) {
                        a.last_seen_s = b.last_seen_s;
                        a.bearing_rad = b.bearing_rad;
                    }
                    a.first_seen_s = MinOf(a.first_seen_s, b.first_seen_s);
                    esm.contacts.erase(esm.contacts.begin() + static_cast<std::ptrdiff_t>(j));
                } else {
                    ++j;
                }
            }
        std::vector<EsmContact> kept;
        for (const EsmContact& c : esm.contacts) {
            if (s.time_s - c.last_seen_s <= def.contact_timeout_s) {
                kept.push_back(c);
            } else {
                work.events.push_back({s.time_s, EmEventKind::EsmContactLost, esm.id, c.id, "ESM contact " + std::to_string(c.id) + " lost"});
            }
        }
        esm.contacts.swap(kept);
    }
}

// Every ESM site listens. Sites are independent (each has its own contacts, path profiles, counters and event list), so
// they can run on `s.threads` threads; counters and events are merged in site order, and the result is the same at any
// thread count.
inline void RunEsm(EmScene& s, double dt) {
    const std::size_t n = s.esm.size();
    std::vector<EsmWork> work(n);
    const EmScene& cs = s;
    const unsigned threads = s.threads < 1 ? 1u : s.threads;
    if (threads <= 1 || n < 2) {
        for (std::size_t i = 0; i < n; ++i) RunEsmSite(cs, s.esm[i], dt, work[i]);
    } else {
        std::atomic<std::size_t> next{0};
        std::vector<std::thread> pool;
        const unsigned count = threads < n ? threads : static_cast<unsigned>(n);
        for (unsigned t = 0; t < count; ++t) {
            pool.emplace_back([&]() {
                for (;;) {
                    const std::size_t i = next.fetch_add(1);
                    if (i >= n) break;
                    RunEsmSite(cs, s.esm[i], dt, work[i]);
                }
            });
        }
        for (std::thread& th : pool) th.join();
    }
    for (std::size_t i = 0; i < n; ++i) {
        s.stats.esm_culled += work[i].culled;
        s.stats.esm_evaluations += work[i].evaluations;
        s.stats.intercepts += work[i].intercepts;
        s.events.insert(s.events.end(), work[i].events.begin(), work[i].events.end());
    }
}

// TDOA groups: a pulse heard by enough sites in the same tick is multilaterated at the group's node.
inline void RunTdoa(EmScene& s) {
    for (TdoaGroup& g : s.tdoa_groups) {
        NodeState* node = s.FindNode(g.node_id);
        if (node == nullptr || !node->alive) continue;
        struct Cluster {
            EmitterSignature sig;
            std::vector<TdoaSite> sites;
            std::vector<int> site_ids;
            std::vector<double> bearings;
            int truth = 0;
        };
        std::vector<Cluster> clusters;
        for (int esm_id : g.esm_ids) {
            const EsmState* esm = s.FindEsm(esm_id);
            if (esm == nullptr || !esm->on) continue;
            const Platform* site = s.FindPlatform(esm->platform_id);
            if (site == nullptr || !site->alive) continue;
            for (const EsmIntercept& in : esm->intercepts) {
                EmitterSignature sig;
                sig.freq_hz = in.freq_hz;
                sig.prf_hz = in.prf_hz;
                Cluster* home = nullptr;
                for (Cluster& c : clusters) {
                    bool has_site = false;
                    for (int id : c.site_ids) has_site = has_site || id == esm_id;
                    if (!has_site && SameEmitter(c.sig, sig, 0.01, 0.05)) {
                        home = &c;
                        break;
                    }
                }
                if (home == nullptr) {
                    clusters.push_back(Cluster{});
                    home = &clusters.back();
                    home->sig = sig;
                    home->truth = in.debug_truth_emitter;
                }
                home->sites.push_back({site->position, in.arrival_time_s});
                home->site_ids.push_back(esm_id);
                home->bearings.push_back(in.bearing_rad);
            }
        }
        const std::size_t need = g.solve_3d ? 4u : 3u;
        for (const Cluster& c : clusters) {
            if (c.sites.size() < need) continue;
            // Start from the centroid pushed 50 km out along the mean bearing.
            Vec3 centroid;
            double sx = 0.0, cy = 0.0;
            for (std::size_t i = 0; i < c.sites.size(); ++i) {
                centroid += c.sites[i].position;
                sx += std::sin(c.bearings[i]);
                cy += std::cos(c.bearings[i]);
            }
            centroid = centroid / static_cast<double>(c.sites.size());
            const double mean_az = std::atan2(sx, cy);
            Vec3 initial = centroid + Vec3{std::sin(mean_az), std::cos(mean_az), 0.0} * 50000.0;
            initial.z = g.solve_3d ? 5000.0 : g.assumed_altitude_m;
            const EsmDef& def0 = s.lib.esm[static_cast<std::size_t>(s.FindEsm(c.site_ids[0])->def_index)];
            const TdoaResult r = SolveTdoa(c.sites, def0.time_sigma_s, initial, !g.solve_3d);
            if (!r.valid || r.residual_rms_m > g.max_residual_m) continue;
            int item = 0;
            for (std::size_t i = 0; i < g.items.size(); ++i)
                if (SameEmitter(g.items[i], c.sig, 0.01, 0.05)) item = static_cast<int>(i) + 1;
            if (item == 0) {
                g.items.push_back(c.sig);
                item = static_cast<int>(g.items.size());
                s.Log(EmEventKind::EmitterFix, g.id, item,
                      g.name + ": TDOA fix on a new emitter (" + std::to_string(c.sites.size()) + " sites)");
            }
            KnowledgeTrack k;
            k.origin = {node->id, g.id, item};
            k.source = InfoSource::OwnEsm;
            k.category = Category::Emitter;
            k.position_valid = true;
            k.position = r.position;
            k.cov = r.covariance;
            if (!g.solve_3d) k.cov(2, 2) = 1000.0 * 1000.0;
            k.time_s = s.time_s;
            k.received_s = s.time_s;
            k.has_emitter = true;
            k.emitter = c.sig;
            const EsmDef& d = def0;
            const ThreatLibrary* lib = s.lib.Library(d.threat_library);
            if (lib != nullptr) {
                const LibraryMatch m = MatchLibrary(*lib, c.sig.freq_hz, c.sig.prf_hz, 0.0);
                if (m.entry != nullptr) {
                    k.emitter.library_id = m.entry->id;
                    k.emitter.library_name = m.entry->name;
                    k.emitter.library_confidence = m.confidence;
                    k.emitter.hostile = m.entry->owner != Side::Neutral && m.entry->owner != node->side;
                    k.emitter.max_speed_mps = m.entry->max_speed_mps;
                    if (k.emitter.hostile) {
                        k.identity = Identity::Hostile;
                        k.identity_confidence = 0.8 * m.confidence;
                    }
                }
            }
            k.debug_truth_platform = 0;
            const EmitterState* te = s.FindEmitter(c.truth);
            if (te != nullptr) k.debug_truth_platform = te->platform_id;
            node->kb.Upsert(k);
        }
    }
}

}  // namespace em
}  // namespace simcore
