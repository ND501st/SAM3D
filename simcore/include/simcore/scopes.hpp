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

class SearchScope {
public:
    struct Blip {
        std::string target_id;
        double azimuth_rad = 0.0;
        double range_m = 0.0;
        double brightness = 0.0;  // 1 when just painted, fading linearly to 0 over the afterglow
    };

    // `sweep_period_s`: one antenna turn. `afterglow_s`: how long an echo stays visible.
    SearchScope(double sweep_period_s, double afterglow_s)
        : period_s_(sweep_period_s > 0.0 ? sweep_period_s : 1.0), afterglow_s_(afterglow_s > 0.0 ? afterglow_s : 1.0) {}

    // Advance by dt. The sweep turns while `rotating`; each contact it passes over is painted.
    void Update(double dt, bool rotating, const std::vector<Contact>& picture) {
        if (dt <= 0.0) return;
        for (Paint& p : paints_) p.age_s += dt;
        std::size_t kept = 0;
        for (std::size_t i = 0; i < paints_.size(); ++i) {
            if (paints_[i].age_s < afterglow_s_) paints_[kept++] = paints_[i];
        }
        paints_.resize(kept);
        if (!rotating) return;
        const double turn = kTwoPi * dt / period_s_;
        for (const Contact& c : picture) {
            const double az = Wrap(DegToRad(c.azimuth_deg));
            if (turn >= kTwoPi || Wrap(az - sweep_rad_) <= turn) {
                // Painted part-way through the step: age it by the time since the sweep passed.
                const double since_s = turn >= kTwoPi ? 0.0 : (turn - Wrap(az - sweep_rad_)) / kTwoPi * period_s_;
                paints_.push_back({c.target_id, az, c.range_m, since_s});
            }
        }
        sweep_rad_ = Wrap(sweep_rad_ + turn);
    }

    double SweepAzimuthRad() const { return sweep_rad_; }
    double SweepPeriod() const { return period_s_; }
    double Afterglow() const { return afterglow_s_; }

    std::vector<Blip> Blips() const {
        std::vector<Blip> out;
        out.reserve(paints_.size());
        for (const Paint& p : paints_) out.push_back({p.target_id, p.azimuth_rad, p.range_m, 1.0 - p.age_s / afterglow_s_});
        return out;
    }

    void Clear() { paints_.clear(); }

private:
    struct Paint {
        std::string target_id;
        double azimuth_rad;
        double range_m;
        double age_s;
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
// a rectangular raster, elevation up and range across, covering the search-radar beam the target was found in and a
// window of range centred on the range gate. During target designation the antenna scans up and down through that
// beam; a target inside the antenna's beam lights a bright mark, which fades with the phosphor's afterglow. Outside
// the scan the antenna simply points where the elevation wheel (or the tracking servo) puts it.
class ElevationScope {
public:
    struct Mark {
        std::string target_id;
        double range_offset_m = 0.0;  // from the range gate, positive farther
        double elevation_deg = 0.0;
        double brightness = 0.0;      // 1 when just painted, fading linearly to 0 over the afterglow
    };

    // `scan_period_s`: one sweep up and back down. `beam_width_deg`: the antenna's full beam width.
    ElevationScope(double scan_period_s, double afterglow_s, double beam_width_deg)
        : period_s_(scan_period_s > 0.0 ? scan_period_s : 1.0),
          afterglow_s_(afterglow_s > 0.0 ? afterglow_s : 1.0),
          half_beam_deg_(beam_width_deg > 0.0 ? 0.5 * beam_width_deg : 0.5) {}

    // Advance by dt. While `scanning`, the antenna sweeps [el_low_deg, el_high_deg]; otherwise it points at
    // `antenna_el_deg`. While `radiating`, every return inside the range window that the antenna's beam passes over
    // during the step is painted.
    void Update(double dt, bool radiating, bool scanning, double antenna_el_deg, double el_low_deg, double el_high_deg,
                double gate_m, double window_m, const std::vector<Contact>& returns) {
        if (dt <= 0.0) return;
        std::size_t kept = 0;
        for (std::size_t i = 0; i < paints_.size(); ++i) {
            paints_[i].age_s += dt;
            if (paints_[i].age_s < afterglow_s_) paints_[kept++] = paints_[i];
        }
        paints_.resize(kept);
        double low = antenna_el_deg;
        double high = antenna_el_deg;
        if (scanning && el_high_deg > el_low_deg) {
            // Sample the sweep finely enough that it can't jump over a beam width.
            const int samples = 1 + static_cast<int>(std::ceil(dt / (period_s_ / 64.0)));
            low = high = ScanAt(phase_, el_low_deg, el_high_deg);
            for (int i = 1; i <= samples; ++i) {
                const double e = ScanAt(phase_ + dt / period_s_ * i / samples, el_low_deg, el_high_deg);
                low = std::fmin(low, e);
                high = std::fmax(high, e);
            }
            phase_ = std::fmod(phase_ + dt / period_s_, 1.0);
            antenna_el_deg_ = ScanAt(phase_, el_low_deg, el_high_deg);
        } else {
            antenna_el_deg_ = antenna_el_deg;
        }
        if (!radiating) return;
        for (const Contact& c : returns) {
            const double offset = c.range_m - gate_m;
            if (std::fabs(offset) > 0.5 * window_m) continue;
            if (c.elevation_deg < low - half_beam_deg_ || c.elevation_deg > high + half_beam_deg_) continue;
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

    double AntennaElevationDeg() const { return antenna_el_deg_; }

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

    // A triangle wave: low at phase 0, high at 0.5, low again at 1.
    static double ScanAt(double phase, double low, double high) {
        double p = std::fmod(phase, 1.0);
        if (p < 0.0) p += 1.0;
        const double up = p < 0.5 ? 2.0 * p : 2.0 - 2.0 * p;
        return low + (high - low) * up;
    }

    double period_s_;
    double afterglow_s_;
    double half_beam_deg_;
    double phase_ = 0.0;
    double antenna_el_deg_ = 0.0;
    std::vector<Paint> paints_;
};

}  // namespace simcore
