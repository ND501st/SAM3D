// simcore/scopes.hpp — what a search radar's scope shows over time.
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

}  // namespace simcore
