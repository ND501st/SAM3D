// simcore/em/tracking.hpp — turning plots into tracks: association, filtering, confirmation, loss.
//
// A sensor's tracker sees only its own plots (range, azimuth, elevation with their errors) and
// never the truth. Standard track-while-scan logic (Blackman & Popoli, "Design and Analysis of
// Modern Tracking Systems", 1999; Bar-Shalom, Li & Kirubarajan, "Estimation with Applications to
// Tracking and Navigation", 2001):
//
//   * Filter: nearly-constant-velocity Kalman filter in ENU; plots converted to Cartesian with
//     their covariance through the Jacobian of (R, az, el) -> (x, y, z). Joseph-form update.
//   * Association: global nearest neighbour inside a chi-square gate on the Mahalanobis distance
//     (greedy by distance; adequate for sparse air pictures; JPDA/MHT are future work).
//   * Life cycle: a new plot opens a tentative track; M of N looks confirm it; a looked-at track
//     with no plot is a miss; tentatives die after a few misses, confirmed tracks coast (their
//     uncertainty grows with process noise) and are dropped after K misses or once too uncertain.
//     A miss counts only when the sensor actually looked where the track should be (a rotating
//     radar does not "miss" a track behind its beam).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "simcore/em/common.hpp"
#include "simcore/em/linalg.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace em {

struct TrackerParams {
    int confirm_hits = 2;              // M ...
    int confirm_window = 3;            // ... of N looks
    int drop_tentative_misses = 2;
    int drop_confirmed_misses = 4;     // consecutive looks without a plot
    double drop_position_sigma_m = 4000.0;
    double process_noise_accel = 15.0;  // white-acceleration spectral density q [m^2/s^3]
    double gate_chi2 = 16.27;           // 3 dof, 99.9 %
    double initial_velocity_sigma_mps = 300.0;
    double good_sigma_m = 50.0;         // quality 1 at or below this position sigma
    double bad_sigma_m = 3000.0;        // quality 0 at or above this
};

enum class TrackStatus { Tentative, Confirmed, Dropped };

inline const char* TrackStatusName(TrackStatus s) {
    switch (s) {
        case TrackStatus::Tentative: return "tentative";
        case TrackStatus::Confirmed: return "confirmed";
        case TrackStatus::Dropped: return "dropped";
    }
    return "?";
}

// One plot: what the sensor measured, in its own polar coordinates, and how well.
struct RadarPlot {
    double time_s = 0.0;
    Vec3 sensor_position;
    double range_m = 0.0;
    double azimuth_rad = 0.0;
    double elevation_rad = 0.0;
    double sigma_range_m = 0.0;
    double sigma_azimuth_rad = 0.0;
    double sigma_elevation_rad = 0.0;
    double snr_db = 0.0;
    bool iff_reply = false;      // an IFF reply came back from this direction and range
    bool false_target = false;   // debug only: a deception or clutter false plot
    int debug_truth_platform = 0;  // debug and tests only; never read by trackers or knowledge
};

// Plot to Cartesian position and covariance (converted measurement).
inline void PlotToCartesian(const RadarPlot& p, Vec3& pos, Mat3& cov) {
    const double ce = std::cos(p.elevation_rad), se = std::sin(p.elevation_rad);
    const double ca = std::cos(p.azimuth_rad), sa = std::sin(p.azimuth_rad);
    pos = p.sensor_position + Vec3{ce * sa, ce * ca, se} * p.range_m;
    Mat3 j;  // columns: d/dR, d/daz, d/del
    j(0, 0) = ce * sa;
    j(1, 0) = ce * ca;
    j(2, 0) = se;
    j(0, 1) = p.range_m * ce * ca;
    j(1, 1) = -p.range_m * ce * sa;
    j(2, 1) = 0.0;
    j(0, 2) = -p.range_m * se * sa;
    j(1, 2) = -p.range_m * se * ca;
    j(2, 2) = p.range_m * ce;
    Mat3 d;
    d(0, 0) = p.sigma_range_m * p.sigma_range_m;
    d(1, 1) = p.sigma_azimuth_rad * p.sigma_azimuth_rad;
    d(2, 2) = p.sigma_elevation_rad * p.sigma_elevation_rad;
    cov = j * d * j.Transposed();
    cov.Symmetrize();
}

struct SensorTrack {
    int id = 0;
    TrackStatus status = TrackStatus::Tentative;
    Col6 x;          // px py pz vx vy vz (ENU)
    Mat6 p;          // covariance
    double time_s = 0.0;       // time of x
    double created_s = 0.0;
    double last_update_s = 0.0;
    int updates = 0;
    int consecutive_misses = 0;
    unsigned history = 0;      // last looks, bit 0 = newest, 1 = hit
    int looks = 0;
    double last_snr_db = 0.0;
    int iff_replies = 0;
    double last_iff_s = -1e9;
    int debug_truth_platform = 0;  // debug and tests only

    Vec3 Position() const { return {x(0, 0), x(1, 0), x(2, 0)}; }
    Vec3 Velocity() const { return {x(3, 0), x(4, 0), x(5, 0)}; }
    Mat3 PositionCov() const {
        Mat3 c;
        for (std::size_t r = 0; r < 3; ++r)
            for (std::size_t k = 0; k < 3; ++k) c(r, k) = p(r, k);
        return c;
    }
    double PositionSigmaM() const { return std::sqrt((p(0, 0) + p(1, 1) + p(2, 2)) / 3.0); }
};

// Quality 0..1 from position uncertainty (log scale between "good" and "bad").
inline double TrackQuality(const SensorTrack& t, const TrackerParams& prm) {
    const double s = t.PositionSigmaM();
    if (s <= prm.good_sigma_m) return 1.0;
    if (s >= prm.bad_sigma_m) return 0.0;
    return (std::log(prm.bad_sigma_m) - std::log(s)) / (std::log(prm.bad_sigma_m) - std::log(prm.good_sigma_m));
}

// Constant-velocity prediction with white-acceleration process noise.
inline void PredictTrack(SensorTrack& t, double to_time_s, double q) {
    const double dt = to_time_s - t.time_s;
    if (dt <= 0.0) return;
    Mat6 f = Mat6::Identity();
    for (std::size_t i = 0; i < 3; ++i) f(i, i + 3) = dt;
    Mat6 qm;
    const double dt2 = dt * dt, dt3 = dt2 * dt;
    for (std::size_t i = 0; i < 3; ++i) {
        qm(i, i) = q * dt3 / 3.0;
        qm(i, i + 3) = q * dt2 / 2.0;
        qm(i + 3, i) = q * dt2 / 2.0;
        qm(i + 3, i + 3) = q * dt;
    }
    t.x = f * t.x;
    t.p = f * t.p * f.Transposed() + qm;
    t.p.Symmetrize();
    t.time_s = to_time_s;
}

// Squared Mahalanobis distance of a Cartesian measurement from a (predicted) track.
inline double MahalanobisSq(const SensorTrack& t, const Vec3& z, const Mat3& r) {
    Mat3 s = t.PositionCov() + r;
    Mat3 si;
    if (!Inverse(s, si)) return 1e300;
    const Col3 y = ToCol(z - t.Position());
    return QuadraticForm(y, si);
}

inline void UpdateTrack(SensorTrack& t, const Vec3& z, const Mat3& r) {
    Mat<3, 6> h;
    for (std::size_t i = 0; i < 3; ++i) h(i, i) = 1.0;
    const Mat<6, 3> ht = h.Transposed();
    Mat3 s = h * t.p * ht + r;
    Mat3 si;
    if (!Inverse(s, si)) return;
    const Mat<6, 3> k = t.p * ht * si;
    const Col3 y = ToCol(z - t.Position());
    t.x = t.x + k * y;
    const Mat6 ikh = Mat6::Identity() - k * h;
    t.p = ikh * t.p * ikh.Transposed() + k * r * k.Transposed();
    t.p.Symmetrize();
}

class Tracker {
public:
    explicit Tracker(TrackerParams p = {}) : prm_(p) {}

    const TrackerParams& Params() const { return prm_; }
    TrackerParams& MutableParams() { return prm_; }
    const std::vector<SensorTrack>& Tracks() const { return tracks_; }

    // Bring every track to `time_s` (for display and reporting between looks).
    void PredictAll(double time_s) {
        for (SensorTrack& t : tracks_) PredictTrack(t, time_s, prm_.process_noise_accel);
    }

    // Process one look. `looked_at(track)` says whether the sensor looked where the track is
    // predicted (inside the swept arc, beam and coverage); only those can collect a miss.
    // Returns ids of tracks that changed status this call (confirmed or dropped), for logging.
    template <typename LookedAtFn>
    std::vector<int> ProcessLook(double time_s, const std::vector<RadarPlot>& plots, LookedAtFn looked_at) {
        std::vector<int> changed;
        PredictAll(time_s);
        std::vector<Vec3> zs(plots.size());
        std::vector<Mat3> rs(plots.size());
        for (std::size_t i = 0; i < plots.size(); ++i) PlotToCartesian(plots[i], zs[i], rs[i]);

        struct Cand {
            double d2;
            std::size_t plot;
            std::size_t track;
        };
        std::vector<Cand> cands;
        for (std::size_t pi = 0; pi < plots.size(); ++pi)
            for (std::size_t ti = 0; ti < tracks_.size(); ++ti) {
                const double d2 = MahalanobisSq(tracks_[ti], zs[pi], rs[pi]);
                if (d2 <= prm_.gate_chi2) cands.push_back({d2, pi, ti});
            }
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
            if (a.d2 != b.d2) return a.d2 < b.d2;
            if (a.plot != b.plot) return a.plot < b.plot;
            return a.track < b.track;
        });
        std::vector<int> plot_used(plots.size(), 0), track_used(tracks_.size(), 0);
        for (const Cand& c : cands) {
            if (plot_used[c.plot] || track_used[c.track]) continue;
            plot_used[c.plot] = 1;
            track_used[c.track] = 1;
            SensorTrack& t = tracks_[c.track];
            UpdateTrack(t, zs[c.plot], rs[c.plot]);
            t.last_update_s = time_s;
            t.last_snr_db = plots[c.plot].snr_db;
            if (plots[c.plot].iff_reply) {
                ++t.iff_replies;
                t.last_iff_s = time_s;
            }
            ++t.updates;
            t.consecutive_misses = 0;
            RecordLook(t, true);
            if (plots[c.plot].debug_truth_platform != 0) t.debug_truth_platform = plots[c.plot].debug_truth_platform;
            if (t.status == TrackStatus::Tentative && HitsInWindow(t) >= prm_.confirm_hits) {
                t.status = TrackStatus::Confirmed;
                changed.push_back(t.id);
            }
        }
        for (std::size_t ti = 0; ti < tracks_.size(); ++ti) {
            if (track_used[ti]) continue;
            SensorTrack& t = tracks_[ti];
            if (!looked_at(t)) continue;
            ++t.consecutive_misses;
            RecordLook(t, false);
            const bool tentative_dead = t.status == TrackStatus::Tentative &&
                                        (t.consecutive_misses >= prm_.drop_tentative_misses ||
                                         (t.looks >= prm_.confirm_window && HitsInWindow(t) < prm_.confirm_hits));
            const bool confirmed_dead = t.status == TrackStatus::Confirmed &&
                                        (t.consecutive_misses >= prm_.drop_confirmed_misses ||
                                         t.PositionSigmaM() > prm_.drop_position_sigma_m);
            if (tentative_dead || confirmed_dead) {
                t.status = TrackStatus::Dropped;
                changed.push_back(t.id);
            }
        }
        for (std::size_t pi = 0; pi < plots.size(); ++pi) {
            if (!plot_used[pi]) Spawn(plots[pi], zs[pi], rs[pi]);
        }
        // Coasting confirmed tracks that are no longer looked at can still become too uncertain.
        for (SensorTrack& t : tracks_) {
            if (t.status == TrackStatus::Confirmed && t.PositionSigmaM() > prm_.drop_position_sigma_m) {
                t.status = TrackStatus::Dropped;
                changed.push_back(t.id);
            }
        }
        tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                     [](const SensorTrack& t) { return t.status == TrackStatus::Dropped; }),
                      tracks_.end());
        return changed;
    }

    // Single-target tracking (a tracking radar locked on): update one track, no association.
    // Creates the track if `track_id` is 0 or unknown. Returns the track id.
    int UpdateSingle(int track_id, const RadarPlot& plot) {
        Vec3 z;
        Mat3 r;
        PlotToCartesian(plot, z, r);
        for (SensorTrack& t : tracks_) {
            if (t.id != track_id) continue;
            PredictTrack(t, plot.time_s, prm_.process_noise_accel);
            UpdateTrack(t, z, r);
            t.last_update_s = plot.time_s;
            t.last_snr_db = plot.snr_db;
            ++t.updates;
            t.consecutive_misses = 0;
            RecordLook(t, true);
            if (t.status == TrackStatus::Tentative && HitsInWindow(t) >= prm_.confirm_hits)
                t.status = TrackStatus::Confirmed;
            if (plot.debug_truth_platform != 0) t.debug_truth_platform = plot.debug_truth_platform;
            return t.id;
        }
        return Spawn(plot, z, r);
    }

    // Coast a single track one look without a plot. Returns false if the track was dropped.
    bool MissSingle(int track_id, double time_s) {
        for (SensorTrack& t : tracks_) {
            if (t.id != track_id) continue;
            PredictTrack(t, time_s, prm_.process_noise_accel);
            ++t.consecutive_misses;
            RecordLook(t, false);
            if (t.consecutive_misses >= prm_.drop_confirmed_misses ||
                t.PositionSigmaM() > prm_.drop_position_sigma_m) {
                t.status = TrackStatus::Dropped;
                tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                             [](const SensorTrack& x) { return x.status == TrackStatus::Dropped; }),
                              tracks_.end());
                return false;
            }
            return true;
        }
        return false;
    }

    const SensorTrack* Find(int id) const {
        for (const SensorTrack& t : tracks_)
            if (t.id == id) return &t;
        return nullptr;
    }

    void Clear() { tracks_.clear(); }

private:
    int Spawn(const RadarPlot& plot, const Vec3& z, const Mat3& r) {
        SensorTrack t;
        t.id = next_id_++;
        t.x(0, 0) = z.x;
        t.x(1, 0) = z.y;
        t.x(2, 0) = z.z;
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t k = 0; k < 3; ++k) t.p(i, k) = r(i, k);
        const double v2 = prm_.initial_velocity_sigma_mps * prm_.initial_velocity_sigma_mps;
        for (std::size_t i = 3; i < 6; ++i) t.p(i, i) = v2;
        t.time_s = plot.time_s;
        t.created_s = plot.time_s;
        t.last_update_s = plot.time_s;
        t.updates = 1;
        t.last_snr_db = plot.snr_db;
        if (plot.iff_reply) {
            t.iff_replies = 1;
            t.last_iff_s = plot.time_s;
        }
        t.debug_truth_platform = plot.debug_truth_platform;
        RecordLook(t, true);
        tracks_.push_back(t);
        return t.id;
    }

    void RecordLook(SensorTrack& t, bool hit) {
        t.history = (t.history << 1) | (hit ? 1u : 0u);
        ++t.looks;
    }
    int HitsInWindow(const SensorTrack& t) const {
        int n = 0;
        const int w = prm_.confirm_window < 31 ? prm_.confirm_window : 31;
        for (int i = 0; i < w; ++i)
            if ((t.history >> i) & 1u) ++n;
        return n;
    }

    TrackerParams prm_;
    std::vector<SensorTrack> tracks_;
    int next_id_ = 1;
};

}  // namespace em
}  // namespace simcore
