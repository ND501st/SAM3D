// simcore/em/knowledge.hpp — what one unit believes, kept apart from what is true.
//
// The layers (docs/EM_ARCHITECTURE.md, "Information model"):
//   truth (Platform)  ->  observation (RadarPlot, EsmIntercept)  ->  sensor track (SensorTrack,
//   EsmContact)  ->  knowledge (KnowledgeTrack: this node's belief, with uncertainty, age, source)
//   ->  picture (FusedTrack: the node's correlated, fused view that AI and the player read).
//
// A KnowledgeBase stores the latest report from each *origin* (node, sensor, item) separately and
// never merges them destructively. The picture is recomputed from those entries on demand:
// entries are predicted to "now" (uncertainty grows with age), correlated by Mahalanobis distance
// and fused with covariance intersection (Julier & Uhlmann, "A non-divergent estimation algorithm
// in the presence of unknown correlations", ACC 1997). CI is the conservative choice when the same
// detection may reach a node by several routes: it never becomes over-confident by counting a
// report twice ("rumour propagation"). Losing a source just lets its entries age out.
//
// Bearing-only entries (ESM, jam strobes) hold their lines of bearing; the picture triangulates
// lines of the same emitter signature (BearingFix), whether they come from several sites or from
// one platform moving over time.
#pragma once

#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "simcore/em/common.hpp"
#include "simcore/em/geolocation.hpp"
#include "simcore/em/linalg.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace em {

enum class Identity { Unknown, AssumedFriend, Friend, Suspect, Hostile };
enum class Category { Unknown, Air, Ground, Emitter };
enum class InfoSource { OwnRadar, OwnEsm, OwnStrobe, Network };

inline const char* IdentityName(Identity i) {
    switch (i) {
        case Identity::Unknown: return "unknown";
        case Identity::AssumedFriend: return "assumed friend";
        case Identity::Friend: return "friend";
        case Identity::Suspect: return "suspect";
        case Identity::Hostile: return "hostile";
    }
    return "?";
}
inline const char* CategoryName(Category c) {
    switch (c) {
        case Category::Unknown: return "unknown";
        case Category::Air: return "air";
        case Category::Ground: return "ground";
        case Category::Emitter: return "emitter";
    }
    return "?";
}
inline const char* InfoSourceName(InfoSource s) {
    switch (s) {
        case InfoSource::OwnRadar: return "own radar";
        case InfoSource::OwnEsm: return "own ESM";
        case InfoSource::OwnStrobe: return "own jam strobe";
        case InfoSource::Network: return "network";
    }
    return "?";
}

// Who first produced an item: the node, its sensor and the sensor's own track/contact number.
struct Origin {
    int node = 0;
    int sensor = 0;
    int item = 0;
    long long Key() const {
        return (static_cast<long long>(node) << 40) ^ (static_cast<long long>(sensor) << 20) ^
               static_cast<long long>(item);
    }
};

// What an ESM receiver measured about an emitter (never its true identity).
struct EmitterSignature {
    double freq_hz = 0.0;
    double prf_hz = 0.0;
    double pulse_width_s = 0.0;
    std::string library_id;       // best threat-library match, "" = unknown emitter
    std::string library_name;
    std::string library_kind;     // the entry's kind: "search", "track", "missile_guidance", ... (what an RWR warns of)
    double library_confidence = 0.0;
    bool hostile = false;         // the library says this emitter type belongs to the other side
    bool steady = false;          // continuous illumination seen (a tracking radar locked on)
    double max_speed_mps = 0.0;   // from the library: how fast such an emitter can move (0 = unknown)
};

// Two signatures plausibly the same emitter (parametric association, as ESM systems do).
inline bool SameEmitter(const EmitterSignature& a, const EmitterSignature& b, double freq_tol = 0.03,
                        double prf_tol = 0.05) {
    if (!a.library_id.empty() && !b.library_id.empty() && a.library_id != b.library_id) return false;
    const double fm = 0.5 * (a.freq_hz + b.freq_hz);
    if (fm > 0.0 && std::fabs(a.freq_hz - b.freq_hz) > freq_tol * fm) return false;
    const double pm = 0.5 * (a.prf_hz + b.prf_hz);
    if (pm > 0.0 && std::fabs(a.prf_hz - b.prf_hz) > prf_tol * pm) return false;
    return true;
}

struct KnowledgeTrack {
    Origin origin;
    InfoSource source = InfoSource::OwnRadar;
    int via_node = 0;  // last node it came through (0 = produced here)
    int hops = 0;
    Category category = Category::Unknown;

    bool position_valid = false;
    Vec3 position;
    Mat3 cov;  // m^2
    bool velocity_valid = false;
    Vec3 velocity;
    double velocity_sigma_mps = 0.0;
    double time_s = 0.0;      // time the position/velocity refer to
    double received_s = 0.0;  // when this node got it

    std::vector<BearingLine> bearings;  // bearing-only items: lines collected so far (newest last)
    bool has_emitter = false;
    EmitterSignature emitter;

    Identity identity = Identity::Unknown;
    double identity_confidence = 0.0;
    std::string type_guess;  // classification from kinematics or emitter type
    double type_confidence = 0.0;

    double quality = 0.0;          // 0..1 at the source
    int debug_truth_platform = 0;  // debug overlays and tests only; AI and fusion never read it

    double PositionSigmaM() const { return std::sqrt((cov(0, 0) + cov(1, 1) + cov(2, 2)) / 3.0); }
    double HorizontalSigmaM() const { return std::sqrt(0.5 * (cov(0, 0) + cov(1, 1))); }
};

struct KnowledgeParams {
    double max_age_s = 120.0;              // entries older than this are forgotten
    double drop_sigma_m = 15000.0;         // ... or once this uncertain
    double unknown_speed_mps = 250.0;      // growth rate for air tracks without a velocity
    double velocity_growth_mps2 = 3.0;     // velocity uncertainty growth (manoeuvre allowance)
    double gate_chi2 = 11.34;              // 3 dof, 99 %
    int max_bearings = 40;                 // lines kept per bearing-only entry
    double emitter_max_speed_mps = 100.0;  // assumed speed of an emitter the library can't place
    double bearing_line_max_age_s = 180.0;
    double min_fix_crossing_rad = 0.06;    // ~3.4 deg: below this the lines are too parallel
};

struct FusedTrack {
    int id = 0;  // stable display number at this node
    KnowledgeTrack fused;
    int contributors = 0;
    std::vector<long long> origins;
    bool from_fix = false;         // position from triangulated bearings
    double fix_chi2 = 0.0;
    std::vector<BearingLine> fix_lines;
    bool bearing_only = false;     // direction known, range unknown
};

// Predict a knowledge item to `t`: move with its velocity and grow its uncertainty.
inline KnowledgeTrack PredictKnowledge(const KnowledgeTrack& k, double t, const KnowledgeParams& prm) {
    KnowledgeTrack out = k;
    const double dt = t - k.time_s;
    if (dt <= 0.0 || !k.position_valid) return out;
    double growth_var;
    if (k.velocity_valid) {
        out.position += k.velocity * dt;
        const double sv = k.velocity_sigma_mps + prm.velocity_growth_mps2 * dt;
        growth_var = (sv * dt) * (sv * dt);
        out.velocity_sigma_mps = sv;
    } else {
        double speed = prm.unknown_speed_mps;
        if (k.category == Category::Ground || k.category == Category::Emitter)
            speed = k.has_emitter && k.emitter.max_speed_mps > 0.0 ? k.emitter.max_speed_mps : prm.emitter_max_speed_mps;
        growth_var = (speed * dt) * (speed * dt);
    }
    out.cov(0, 0) += growth_var;
    out.cov(1, 1) += growth_var;
    out.cov(2, 2) += k.category == Category::Air ? 0.25 * growth_var : 0.01 * growth_var;
    out.time_s = t;
    return out;
}

// Covariance intersection of two position estimates; omega by the fast determinant rule
// (Niehsen, "Information fusion based on fast covariance intersection filtering", FUSION 2002).
inline bool CovarianceIntersect(const Vec3& xa, const Mat3& pa, const Vec3& xb, const Mat3& pb, Vec3& x, Mat3& p) {
    Mat3 ia, ib;
    if (!Inverse(pa, ia) || !Inverse(pb, ib)) return false;
    const double da = Determinant(pa), db = Determinant(pb);
    const double w = (da + db) > 0.0 ? db / (da + db) : 0.5;  // weight on a: higher when a is tighter
    const Mat3 info = ia * w + ib * (1.0 - w);
    Mat3 pi;
    if (!Inverse(info, pi)) return false;
    const Col3 v = ia * ToCol(xa) * w + ib * ToCol(xb) * (1.0 - w);
    x = ToVec(pi * v);
    p = pi;
    p.Symmetrize();
    return true;
}

inline double KnowledgeMahalanobisSq(const KnowledgeTrack& a, const KnowledgeTrack& b) {
    Mat3 s = a.cov + b.cov;
    Mat3 si;
    if (!Inverse(s, si)) return 1e300;
    const Col3 d = ToCol(a.position - b.position);
    return QuadraticForm(d, si);
}

// Horizontal-only Mahalanobis (for a 2-D emitter fix against 3-D tracks).
inline double HorizontalMahalanobisSq(const KnowledgeTrack& a, const KnowledgeTrack& b) {
    Mat2 s = Horizontal(a.cov) + Horizontal(b.cov);
    Mat2 si;
    if (!Inverse(s, si)) return 1e300;
    Col2 d;
    d(0, 0) = a.position.x - b.position.x;
    d(1, 0) = a.position.y - b.position.y;
    return QuadraticForm(d, si);
}

class KnowledgeBase {
public:
    explicit KnowledgeBase(KnowledgeParams p = {}) : prm_(p) {}

    const KnowledgeParams& Params() const { return prm_; }
    KnowledgeParams& MutableParams() { return prm_; }
    const std::vector<KnowledgeTrack>& Entries() const { return entries_; }

    // Store a report. Same origin: the newer report replaces the old one (bearing-only reports
    // add their new lines instead). Returns true if the entry is new.
    bool Upsert(const KnowledgeTrack& k) {
        const long long key = k.origin.Key();
        for (KnowledgeTrack& e : entries_) {
            if (e.origin.Key() != key) continue;
            if (!k.bearings.empty() && !k.position_valid) {
                std::vector<BearingLine> lines = e.bearings;
                for (const BearingLine& l : k.bearings) {
                    bool dup = false;
                    for (const BearingLine& m : lines) dup = dup || std::fabs(m.time_s - l.time_s) < 1e-6;
                    if (!dup) lines.push_back(l);
                }
                const std::size_t keep = static_cast<std::size_t>(prm_.max_bearings > 1 ? prm_.max_bearings : 1);
                if (lines.size() > keep) lines.erase(lines.begin(), lines.end() - static_cast<long>(keep));
                e = k;
                e.bearings = lines;
            } else if (k.time_s >= e.time_s) {
                e = k;
            }
            return false;
        }
        entries_.push_back(k);
        return true;
    }

    // Forget what is too old or too uncertain.
    void Age(double now) {
        std::vector<KnowledgeTrack> kept;
        kept.reserve(entries_.size());
        for (const KnowledgeTrack& e : entries_) {
            const double newest = e.bearings.empty() ? e.time_s : e.bearings.back().time_s;
            if (now - newest > prm_.max_age_s) continue;
            if (e.position_valid && PredictKnowledge(e, now, prm_).PositionSigmaM() > prm_.drop_sigma_m) continue;
            kept.push_back(e);
        }
        entries_.swap(kept);
    }

    template <typename Pred>
    void RemoveIf(Pred pred) {
        std::vector<KnowledgeTrack> kept;
        kept.reserve(entries_.size());
        for (const KnowledgeTrack& e : entries_)
            if (!pred(e)) kept.push_back(e);
        entries_.swap(kept);
    }

    void RemoveFromSource(InfoSource s) {
        std::vector<KnowledgeTrack> kept;
        for (const KnowledgeTrack& e : entries_)
            if (e.source != s) kept.push_back(e);
        entries_.swap(kept);
    }

    std::size_t Size() const { return entries_.size(); }

    // The node's fused picture at `now`.
    std::vector<FusedTrack> Picture(double now) const {
        std::vector<FusedTrack> out;
        // 1. Position entries, best first, clustered and fused.
        std::vector<KnowledgeTrack> pos;
        for (const KnowledgeTrack& e : entries_)
            if (e.position_valid) pos.push_back(PredictKnowledge(e, now, prm_));
        std::vector<std::size_t> order(pos.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        for (std::size_t i = 1; i < order.size(); ++i)  // insertion sort by sigma (small lists)
            for (std::size_t j = i; j > 0 && pos[order[j]].PositionSigmaM() < pos[order[j - 1]].PositionSigmaM(); --j) {
                const std::size_t tmp = order[j];
                order[j] = order[j - 1];
                order[j - 1] = tmp;
            }
        for (std::size_t oi : order) {
            const KnowledgeTrack& e = pos[oi];
            FusedTrack* best = nullptr;
            double best_d2 = prm_.gate_chi2;
            for (FusedTrack& f : out) {
                if (!Compatible(f.fused, e)) continue;
                bool same_origin_node = false;  // two tracks from one sensor are two objects
                for (long long o : f.origins) same_origin_node = same_origin_node || SameSensor(o, e.origin.Key());
                if (same_origin_node) continue;
                const double d2 = KnowledgeMahalanobisSq(f.fused, e);
                if (d2 < best_d2) {
                    best_d2 = d2;
                    best = &f;
                }
            }
            if (best == nullptr) {
                FusedTrack f;
                f.fused = e;
                f.contributors = 1;
                f.origins.push_back(e.origin.Key());
                out.push_back(f);
            } else {
                Merge(*best, e);
            }
        }
        // 2. Bearing-only entries grouped by emitter signature and triangulated.
        std::vector<int> used(entries_.size(), 0);
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const KnowledgeTrack& e = entries_[i];
            if (e.position_valid || e.bearings.empty() || used[i]) continue;
            std::vector<BearingLine> lines;
            std::vector<long long> origins;
            KnowledgeTrack head = e;
            for (std::size_t j = i; j < entries_.size(); ++j) {
                const KnowledgeTrack& o = entries_[j];
                if (used[j] || o.position_valid || o.bearings.empty()) continue;
                if (j != i && !(e.has_emitter && o.has_emitter && SameEmitter(e.emitter, o.emitter))) continue;
                used[j] = 1;
                origins.push_back(o.origin.Key());
                for (const BearingLine& l : o.bearings)
                    if (now - l.time_s <= prm_.bearing_line_max_age_s) lines.push_back(l);
                if (o.emitter.library_confidence > head.emitter.library_confidence) head.emitter = o.emitter;
                if (o.identity_confidence > head.identity_confidence) {
                    head.identity = o.identity;
                    head.identity_confidence = o.identity_confidence;
                }
                if (o.debug_truth_platform != 0) head.debug_truth_platform = o.debug_truth_platform;
            }
            if (lines.empty()) continue;
            FusedTrack f;
            f.origins = origins;
            f.contributors = static_cast<int>(origins.size());
            f.fused = head;
            f.fused.category = Category::Emitter;
            f.fused.bearings = lines;
            // Old lines count for less the faster this kind of emitter can move (an aircraft's
            // radar vs a SAM site's): bearings of a moving emitter are only good together.
            const double vmax = head.emitter.max_speed_mps > 0.0 ? head.emitter.max_speed_mps : prm_.emitter_max_speed_mps;
            const FixResult fix = BearingFix(lines, now, vmax, prm_.min_fix_crossing_rad);
            if (fix.valid) {
                f.from_fix = true;
                f.fix_chi2 = fix.chi2;
                f.fix_lines = lines;
                f.fused.position_valid = true;
                f.fused.position = fix.position;
                f.fused.cov = Mat3{};
                f.fused.cov(0, 0) = fix.covariance(0, 0);
                f.fused.cov(0, 1) = fix.covariance(0, 1);
                f.fused.cov(1, 0) = fix.covariance(1, 0);
                f.fused.cov(1, 1) = fix.covariance(1, 1);
                f.fused.cov(2, 2) = 1000.0 * 1000.0;  // a 2-D fix says nothing about height
                f.fused.time_s = now;
                // An emitter fix that matches a position track tags that track with the emitter.
                FusedTrack* match = nullptr;
                double best_d2 = 9.21;  // 2 dof, 99 %
                for (FusedTrack& p : out) {
                    if (p.from_fix || p.bearing_only) continue;
                    const double d2 = HorizontalMahalanobisSq(p.fused, f.fused);
                    if (d2 < best_d2) {
                        best_d2 = d2;
                        match = &p;
                    }
                }
                if (match != nullptr) {
                    match->fused.has_emitter = true;
                    match->fused.emitter = f.fused.emitter;
                    if (f.fused.identity_confidence > match->fused.identity_confidence) {
                        match->fused.identity = f.fused.identity;
                        match->fused.identity_confidence = f.fused.identity_confidence;
                    }
                    for (long long o : f.origins) match->origins.push_back(o);
                    continue;
                }
            } else {
                f.bearing_only = true;
                f.fused.position_valid = false;
            }
            out.push_back(f);
        }
        for (FusedTrack& f : out) f.id = DisplayId(f.origins);
        return out;
    }

private:
    static bool Compatible(const KnowledgeTrack& a, const KnowledgeTrack& b) {
        if (a.category == Category::Unknown || b.category == Category::Unknown) return true;
        if (a.category == b.category) return true;
        return (a.category == Category::Emitter) != (b.category == Category::Emitter);
    }
    static bool SameSensor(long long a, long long b) { return (a >> 20) == (b >> 20); }

    void Merge(FusedTrack& f, const KnowledgeTrack& e) const {
        Vec3 x;
        Mat3 p;
        if (CovarianceIntersect(f.fused.position, f.fused.cov, e.position, e.cov, x, p)) {
            f.fused.position = x;
            f.fused.cov = p;
        }
        if (e.velocity_valid && (!f.fused.velocity_valid || e.velocity_sigma_mps < f.fused.velocity_sigma_mps)) {
            f.fused.velocity_valid = true;
            f.fused.velocity = e.velocity;
            f.fused.velocity_sigma_mps = e.velocity_sigma_mps;
        }
        if (e.identity_confidence > f.fused.identity_confidence) {
            f.fused.identity = e.identity;
            f.fused.identity_confidence = e.identity_confidence;
        }
        if (e.has_emitter && (!f.fused.has_emitter || e.emitter.library_confidence > f.fused.emitter.library_confidence)) {
            f.fused.has_emitter = true;
            f.fused.emitter = e.emitter;
        }
        if (e.type_confidence > f.fused.type_confidence) {
            f.fused.type_guess = e.type_guess;
            f.fused.type_confidence = e.type_confidence;
        }
        if (e.quality > f.fused.quality) f.fused.quality = e.quality;
        if (f.fused.debug_truth_platform == 0) f.fused.debug_truth_platform = e.debug_truth_platform;
        ++f.contributors;
        f.origins.push_back(e.origin.Key());
    }

    // Display numbers stay with an object while any of its origins persists.
    int DisplayId(const std::vector<long long>& origins) const {
        for (long long o : origins) {
            const auto it = display_ids_.find(o);
            if (it != display_ids_.end()) {
                for (long long other : origins) display_ids_[other] = it->second;
                return it->second;
            }
        }
        const int id = next_display_id_++;
        for (long long o : origins) display_ids_[o] = id;
        return id;
    }

    KnowledgeParams prm_;
    std::vector<KnowledgeTrack> entries_;
    mutable std::map<long long, int> display_ids_;
    mutable int next_display_id_ = 1;
};

}  // namespace em
}  // namespace simcore
