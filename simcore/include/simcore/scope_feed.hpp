// simcore/scope_feed.hpp — what the cabin's scopes are drawn from.
//
// The search indicator, the range and elevation indicators, their counters and the TV's pointing all draw from one
// ScopeFeed. On the machine that simulates it comes straight from the engagement model (ScopeFeedOf); on a network
// client it comes from the knowledge stream (the server puts the same feed into each player's view, em::ViewScope, with
// the echoes' target ids replaced by opaque keys: EchoKeys), so the client's scopes show the server's picture.
#pragma once

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "simcore/em/replication.hpp"
#include "simcore/engagement.hpp"
#include "simcore/launcher.hpp"

namespace simcore {

struct ScopeFeed {
    bool valid = false;
    std::vector<Contact> search;
    std::vector<Contact> iff;
    std::vector<Contact> track_returns;
    int elevation_beam = 0;
    double swept_low_deg = 0.0;
    double swept_high_deg = 0.0;
    double antenna_el_deg = 0.0;
    double indicated_to_true_deg = 0.0;
    double gyro_heading_deg = 0.0;  // the gyro half-compass's heading (the OR-31M Qc scale)
    double missile_range_m[2] = {-1.0, -1.0};
    Vec3 unit_position;

    double IndicatedToTrueDeg(double indicated_deg) const {
        double d = std::fmod(indicated_deg + indicated_to_true_deg + 180.0, 360.0);
        if (d < 0.0) d += 360.0;
        return d - 180.0;
    }
    // The missile range of guidance channel 1 or 2 (-1: none).
    double MissileRangeM(int channel) const { return channel == 1 || channel == 2 ? missile_range_m[channel - 1] : -1.0; }
};

inline ScopeFeed ScopeFeedOf(const EngagementModel& e, const Launcher* launcher, double now_s) {
    ScopeFeed f;
    f.valid = true;
    f.search = e.SearchPicture();
    f.iff = e.IffReplies();
    f.track_returns = e.TrackReturns();
    f.elevation_beam = e.ElevationBeam();
    f.swept_low_deg = e.SweptElevationLowDeg();
    f.swept_high_deg = e.SweptElevationHighDeg();
    f.antenna_el_deg = e.AntennaElevationDeg();
    f.indicated_to_true_deg = e.IndicatedToTrueDeg(0.0);
    f.gyro_heading_deg = e.GyroHeadingDeg();
    if (launcher != nullptr) {
        f.missile_range_m[0] = launcher->MissileRangeM(1, now_s);
        f.missile_range_m[1] = launcher->MissileRangeM(2, now_s);
    }
    f.unit_position = e.UnitPosition();
    return f;
}

// Opaque, stable keys for the echoes' target ids, one table per player (a key says "the same echo as before" and
// nothing else).
class EchoKeys {
public:
    int Key(const std::string& id) {
        const auto it = keys_.find(id);
        if (it != keys_.end()) return it->second;
        const int k = static_cast<int>(keys_.size()) + 1;
        keys_[id] = k;
        return k;
    }

private:
    std::map<std::string, int> keys_;
};

inline em::ViewScope ToViewScope(const ScopeFeed& f, EchoKeys& keys) {
    em::ViewScope v;
    v.valid = f.valid;
    auto conv = [&keys](const std::vector<Contact>& in, std::vector<em::ViewEcho>& out) {
        for (const Contact& c : in) {
            em::ViewEcho e;
            e.key = keys.Key(c.target_id);
            e.range_m = c.range_m;
            e.azimuth_deg = c.azimuth_deg;
            e.elevation_deg = c.elevation_deg;
            e.beam = c.beam;
            e.control_reply = c.control_reply;
            out.push_back(e);
        }
    };
    conv(f.search, v.search);
    conv(f.iff, v.iff);
    conv(f.track_returns, v.track_returns);
    v.elevation_beam = f.elevation_beam;
    v.swept_low_deg = f.swept_low_deg;
    v.swept_high_deg = f.swept_high_deg;
    v.antenna_el_deg = f.antenna_el_deg;
    v.indicated_to_true_deg = f.indicated_to_true_deg;
    v.gyro_heading_deg = f.gyro_heading_deg;
    v.missile_range_m[0] = f.missile_range_m[0];
    v.missile_range_m[1] = f.missile_range_m[1];
    v.unit_position = f.unit_position;
    return v;
}

inline ScopeFeed FromViewScope(const em::ViewScope& v) {
    ScopeFeed f;
    f.valid = v.valid;
    auto conv = [](const std::vector<em::ViewEcho>& in, std::vector<Contact>& out) {
        for (const em::ViewEcho& e : in) {
            Contact c;
            c.target_id = "e" + std::to_string(e.key);
            c.range_m = e.range_m;
            c.azimuth_deg = e.azimuth_deg;
            c.elevation_deg = e.elevation_deg;
            c.beam = e.beam;
            c.control_reply = e.control_reply;
            out.push_back(c);
        }
    };
    conv(v.search, f.search);
    conv(v.iff, f.iff);
    conv(v.track_returns, f.track_returns);
    f.elevation_beam = v.elevation_beam;
    f.swept_low_deg = v.swept_low_deg;
    f.swept_high_deg = v.swept_high_deg;
    f.antenna_el_deg = v.antenna_el_deg;
    f.indicated_to_true_deg = v.indicated_to_true_deg;
    f.gyro_heading_deg = v.gyro_heading_deg;
    f.missile_range_m[0] = v.missile_range_m[0];
    f.missile_range_m[1] = v.missile_range_m[1];
    f.unit_position = v.unit_position;
    return f;
}

}  // namespace simcore
