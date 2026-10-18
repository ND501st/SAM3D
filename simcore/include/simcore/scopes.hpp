// simcore/scopes.hpp — what a radar's scopes show over time (search scope, elevation-search indicator).
//
// The search radar's picture (EngagementModel::SearchPicture) says which targets the
// radar can see right now. A plan-position indicator only shows an echo when the
// rotating antenna's beam passes over it, and the phosphor keeps it glowing while it
// fades. SearchScope keeps the sweep angle and those fading echoes so a display can
// draw them; it doesn't decide what the radar can see.
//
// Azimuths are clockwise from the hull's forward direction (north in this version of
// the engagement model), in radians, 0..2*pi.
#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/engagement.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// The range rings the search scope draws on range scale `scale_index` (metres from the radar): the console's own list
// for that scale (sensors.search.range_rings_m, as the operators describe the Osa's scope), else every 5 km (10 km on a
// scale more than 20 km deep) inside the scale's window.
inline std::vector<double> ScopeRingsM(const SearchSensorDef& s, int scale_index) {
    if (scale_index >= 0 && scale_index < static_cast<int>(s.range_rings_m.size())) {
        return s.range_rings_m[static_cast<std::size_t>(scale_index)];
    }
    double lo = 0.0, hi = 45000.0;
    if (scale_index >= 0 && scale_index < static_cast<int>(s.range_scales_m.size())) {
        lo = s.range_scales_m[static_cast<std::size_t>(scale_index)].first;
        hi = s.range_scales_m[static_cast<std::size_t>(scale_index)].second;
    }
    const double step = hi - lo > 20000.0 ? 10000.0 : 5000.0;
    std::vector<double> out;
    for (double k = std::ceil((lo + 1.0) / step) * step; k <= hi + 1.0; k += step) out.push_back(k);
    return out;
}

class SearchScope {
public:
    struct Blip {
        std::string target_id;
        double azimuth_rad = 0.0;
        double range_m = 0.0;
        double brightness = 0.0;  // its strength when just painted, fading linearly to 0 over the afterglow
        bool iff_reply = false;   // an IFF reply rather than an echo
        bool control_reply = false;  // the reply to a control interrogation (ЗАПРОС К)
        bool strobe = false;      // a noise jammer's strobe: a bright spoke along its azimuth, no range
    };

    // How bright an echo paints for its signal to noise and jamming: 0.35 at the detection threshold (about 13 dB),
    // full from 38 dB (a display choice; an unknown SNR, the gameplay rules', paints full).
    static double EchoStrength(double snr_db) {
        if (snr_db <= Contact::kUnknownSnrDb + 1.0) return 1.0;
        const double s = 0.35 + 0.65 * (snr_db - 13.0) / 25.0;
        return s < 0.35 ? 0.35 : (s > 1.0 ? 1.0 : s);
    }
    // A strobe's brightness for its J/N: 0.25 when just heard (3 dB), full from 33 dB.
    static double StrobeStrength(double jn_db) {
        const double s = 0.25 + 0.75 * (jn_db - 3.0) / 30.0;
        return s < 0.25 ? 0.25 : (s > 1.0 ? 1.0 : s);
    }

    // `sweep_period_s`: one antenna turn. `afterglow_s`: how long an echo stays visible.
    SearchScope(double sweep_period_s, double afterglow_s)
        : period_s_(sweep_period_s > 0.0 ? sweep_period_s : 1.0), afterglow_s_(afterglow_s > 0.0 ? afterglow_s : 1.0) {}

    // Advance by dt. The sweep turns while `rotating`; each echo in `picture` and IFF reply in `replies` it passes
    // over is painted (the interrogator's antenna turns with the search antenna).
    // `strobes`: jamming strobes, painted as the sweep passes their azimuth. `antenna_az_rad` >= 0: the search
    // antenna's azimuth now (the EM layer's), which the sweep follows, so a contact paints when the beam passes it.
    void Update(double dt, bool rotating, const std::vector<Contact>& picture, const std::vector<Contact>& replies = {},
                const std::vector<ScopeStrobe>& strobes = {}, double antenna_az_rad = -1.0) {
        if (dt <= 0.0) return;
        for (Paint& p : paints_) p.age_s += dt;
        std::size_t kept = 0;
        for (std::size_t i = 0; i < paints_.size(); ++i) {
            if (paints_[i].age_s < afterglow_s_) paints_[kept++] = paints_[i];
        }
        paints_.resize(kept);
        if (!rotating) return;
        const double turn = kTwoPi * dt / period_s_;
        if (antenna_az_rad >= 0.0) sweep_rad_ = Wrap(antenna_az_rad - turn);  // what the antenna swept this step
        // Painted part-way through the step: aged by the time since the sweep passed.
        auto passed = [&](double az, double& since_s) {
            if (turn < kTwoPi && Wrap(az - sweep_rad_) > turn) return false;
            since_s = turn >= kTwoPi ? 0.0 : (turn - Wrap(az - sweep_rad_)) / kTwoPi * period_s_;
            return true;
        };
        for (int list = 0; list < 2; ++list) {
            for (const Contact& c : list == 0 ? picture : replies) {
                const double az = Wrap(DegToRad(c.azimuth_deg));
                double since_s = 0.0;
                if (passed(az, since_s)) {
                    paints_.push_back({c.target_id, az, c.range_m, since_s, list == 1, list == 1 && c.control_reply,
                                       list == 0 ? EchoStrength(c.snr_db) : 1.0, false});
                }
            }
        }
        for (const ScopeStrobe& st : strobes) {
            const double az = Wrap(DegToRad(st.azimuth_deg));
            double since_s = 0.0;
            if (passed(az, since_s)) paints_.push_back({std::string(), az, -1.0, since_s, false, false, StrobeStrength(st.jn_db), true});
        }
        sweep_rad_ = Wrap(sweep_rad_ + turn);
    }

    double SweepAzimuthRad() const { return sweep_rad_; }
    double SweepPeriod() const { return period_s_; }
    double Afterglow() const { return afterglow_s_; }

    std::vector<Blip> Blips() const {
        std::vector<Blip> out;
        out.reserve(paints_.size());
        for (const Paint& p : paints_) {
            out.push_back({p.target_id, p.azimuth_rad, p.range_m, p.strength * (1.0 - p.age_s / afterglow_s_), p.iff_reply,
                           p.control_reply, p.strobe});
        }
        return out;
    }

    void Clear() { paints_.clear(); }

private:
    struct Paint {
        std::string target_id;
        double azimuth_rad;
        double range_m;
        double age_s;
        bool iff_reply;
        bool control_reply;
        double strength;
        bool strobe;
    };

    static double Wrap(double rad) {
        double r = std::fmod(rad, kTwoPi);
        if (r < 0.0) r += kTwoPi;
        return r;
    }

    double period_s_;
    double afterglow_s_;
    double sweep_rad_ = 0.0;
    std::vector<Paint> paints_;
};

// What a tracking radar's elevation-search indicator shows (the Osa's OS81-10M, voenizdat_osa_1991 p95, Fig. 2.37):
// a rectangular raster, elevation up and range across, covering the search-radar beam being searched and a window of
// range centred on the range gate. A target inside the antenna's beam lights a bright mark, which fades with the
// phosphor's afterglow. Where the antenna points (and how it scans during target designation) is the engagement
// model's (EngagementModel::SweptElevationLowDeg / HighDeg); this keeps the fading marks.
class ElevationScope {
public:
    struct Mark {
        std::string target_id;
        double range_offset_m = 0.0;  // from the range gate, positive farther
        double elevation_deg = 0.0;
        double brightness = 0.0;      // 1 when just painted, fading linearly to 0 over the afterglow
    };

    // `beam_width_deg`: the antenna's full beam width.
    ElevationScope(double afterglow_s, double beam_width_deg)
        : afterglow_s_(afterglow_s > 0.0 ? afterglow_s : 1.0), half_beam_deg_(beam_width_deg > 0.0 ? 0.5 * beam_width_deg : 0.5) {}

    // Advance by dt. The antenna's beam centre passed over [swept_low_deg, swept_high_deg] during the step. While
    // `radiating`, every return inside the range window that the beam passed over is painted.
    void Update(double dt, bool radiating, double swept_low_deg, double swept_high_deg, double gate_m, double window_m,
                const std::vector<Contact>& returns) {
        if (dt <= 0.0) return;
        std::size_t kept = 0;
        for (std::size_t i = 0; i < paints_.size(); ++i) {
            paints_[i].age_s += dt;
            if (paints_[i].age_s < afterglow_s_) paints_[kept++] = paints_[i];
        }
        paints_.resize(kept);
        if (!radiating) return;
        for (const Contact& c : returns) {
            const double offset = c.range_m - gate_m;
            if (std::fabs(offset) > 0.5 * window_m) continue;
            if (c.elevation_deg < swept_low_deg - half_beam_deg_ || c.elevation_deg > swept_high_deg + half_beam_deg_) continue;
            Paint* existing = nullptr;
            for (Paint& p : paints_) {
                if (p.target_id == c.target_id) existing = &p;
            }
            if (existing == nullptr) {
                paints_.push_back({c.target_id, offset, c.elevation_deg, 0.0});
            } else {
                *existing = {c.target_id, offset, c.elevation_deg, 0.0};
            }
        }
    }

    std::vector<Mark> Marks() const {
        std::vector<Mark> out;
        out.reserve(paints_.size());
        for (const Paint& p : paints_) out.push_back({p.target_id, p.offset_m, p.el_deg, 1.0 - p.age_s / afterglow_s_});
        return out;
    }

    void Clear() { paints_.clear(); }

private:
    struct Paint {
        std::string target_id;
        double offset_m;
        double el_deg;
        double age_s;
    };

    double afterglow_s_;
    double half_beam_deg_;
    std::vector<Paint> paints_;
};

}  // namespace simcore
