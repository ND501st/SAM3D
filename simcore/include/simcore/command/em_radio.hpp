// simcore/command/em_radio.hpp — the pipeline's radio hops worked out by the EM layer (roadmap P5,
// docs/command/06_INTEGRATION.md section 4).
//
// Without it a radio call between two posts gets through inside the means' range and is garbled at its loss rate. With it
// each hop is a link budget, as the EM layer's nets work (em/comms_system.hpp): each post's set (the vehicles' R-123M,
// the command posts' R-111) with its power and sensitivity calibrated to its published range (CalibrateRadio), the path's
// loss over the country's ground (diffraction over a ridge, the plane-earth loss below 1 GHz), and noise jamming raising
// the receiver's floor (J/N). The margin decides: below -3 dB nothing gets through, above it a call is lost and said again
// with chance 1 - 1/(1 + exp(-margin/2)), as the EM layer delivers. The enemy's listeners (a ground intercept station, a
// listening aircraft) hear a call by their own margin; with none placed the intercept service is the campaign's abstract
// one and hears everything (P3).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/command/info_net.hpp"
#include "simcore/country_map.hpp"
#include "simcore/em/comms_system.hpp"
#include "simcore/terrain.hpp"

namespace simcore {
namespace command {

// The country's ground as the EM layer's terrain (theatre ENU, the chain's places' frame), with what stands on it as
// ITU-R P.1812 carries it (roadmap S9, simcore/clutter.hpp): a clutter class per `cell_m` cell from the country's land
// cover (CountryMap::BakeClutter), its representative height (data/gameplay/clutter.json, LoadClutterHeights) on the
// profile, so a forest or a town between two posts lies on their path as a ridge would. Without heights (the default),
// the bare ground.
class CountryGround final : public Terrain {
public:
    explicit CountryGround(std::shared_ptr<const CountryMap> map, const ClutterHeights& heights = ClutterHeights(), double cell_m = 250.0)
        : map_(std::move(map)), heights_(heights) {
        if (map_ != nullptr && heights_.Tallest() > 0.0) clutter_ = map_->BakeClutter(cell_m, heights_.dense_share);
        for (std::size_t k = 0; k < height_of_.size(); ++k) height_of_[k] = heights_.HeightOf(static_cast<ClutterClass>(k));
    }
    double HeightAt(double east_m, double north_m) const override { return map_ != nullptr ? map_->GroundHeight(east_m, north_m) : 0.0; }
    double ObstacleHeightAt(double east_m, double north_m) const override {
        return clutter_.cols > 0 ? height_of_[static_cast<std::size_t>(clutter_.ClassAt(east_m, north_m))] : 0.0;
    }
    double ObstacleTallestM() const override { return heights_.Tallest(); }
    const ClutterRaster& Clutter() const { return clutter_; }

private:
    std::shared_ptr<const CountryMap> map_;
    ClutterHeights heights_;
    ClutterRaster clutter_;
    std::array<double, static_cast<std::size_t>(ClutterClass::Count)> height_of_{};
};

// A transmitter that drowns the net in noise: a ground or airborne communications jammer, from and until campaign hours.
struct RadioJammer {
    std::string id;
    Vec3 pos;                   // theatre ENU [m]
    double height_m = 10.0;     // its antenna above the ground under it (an aircraft: its height)
    double erp_w = 0.0;         // radiated towards the net, over its whole bandwidth
    double centre_hz = 0.0;     // 0: on the net's frequency
    double bandwidth_hz = 1e6;  // its noise's width (a barrage across part of the band)
    double from_h = -1e18, until_h = 1e18;
};

// An enemy receiver listening to the net, from and until campaign hours.
struct RadioListener {
    std::string id;
    Vec3 pos;                         // theatre ENU [m]
    double height_m = 10.0;           // its antenna above the ground
    double gain_db = 0.0;             // its antenna towards the net
    double sensitivity_dbw = -130.0;  // for a usable intercept
    double from_h = -1e18, until_h = 1e18;
};

class EmRadio final : public RadioModel {
public:
    // The net's sets: `standard` for every post unless Equip gives it another, on freq_hz (0: the standard set's band
    // middle), over `ground` (null: flat at sea level). Each set is calibrated to its published range.
    EmRadio(em::RadioDef standard, std::shared_ptr<const Terrain> ground, double freq_hz = 0.0, const em::Environment& env = em::Environment())
        : ground_(std::move(ground)), env_(env) {
        if (ground_ == nullptr) ground_ = std::make_shared<FlatTerrain>(0.0);
        freq_hz_ = freq_hz > 0.0 ? freq_hz : 0.5 * (standard.freq_min_hz + standard.freq_max_hz);
        AddSet(std::move(standard));
    }

    // Another set; returns its number for Equip.
    int AddSet(em::RadioDef set) {
        em::CalibrateRadio(set, env_);
        sets_.push_back(std::move(set));
        return static_cast<int>(sets_.size()) - 1;
    }
    void Equip(int position, int set) {
        if (position < 0 || set < 0 || set >= static_cast<int>(sets_.size())) return;
        if (static_cast<std::size_t>(position) >= set_of_.size()) set_of_.resize(static_cast<std::size_t>(position) + 1, 0);
        set_of_[static_cast<std::size_t>(position)] = set;
    }
    // Every post of the chain at or above `level` (the battery's command post and up: 3) works the set: the command nets.
    void EquipCommandPosts(const ChainData& data, int level, int set) {
        for (std::size_t i = 0; i < data.positions.size(); ++i) {
            if (data.positions[i].level <= level) Equip(static_cast<int>(i), set);
        }
    }
    void AddJammer(const RadioJammer& j) { jammers_.push_back(j); }
    void AddListener(const RadioListener& l) { listeners_.push_back(l); }
    const em::RadioDef& SetOf(int position) const {
        const int s = position >= 0 && static_cast<std::size_t>(position) < set_of_.size() ? set_of_[static_cast<std::size_t>(position)] : 0;
        return sets_[static_cast<std::size_t>(s)];
    }
    double FrequencyHz() const { return freq_hz_; }

    RadioPath Hop(int from, int to, const Vec3& xa, const Vec3& xb, double h) const override {
        RadioPath p;
        const em::RadioDef& tx = SetOf(from);
        const em::RadioDef& rx = SetOf(to);
        // Each set's system loss is fitted for two of its kind: a mixed pair takes half of each.
        const double system_loss_db = 0.5 * (tx.system_loss_db + rx.system_loss_db);
        const double eirp_w = tx.power_w * DbToLinear(tx.antenna_gain_db);
        const double rx_dbw = ReceivedDbw(xa, tx.antenna_height_m, xb, rx.antenna_height_m, rx.antenna_gain_db, eirp_w) - system_loss_db;
        // Noise jamming at the receiver: the jammers' power inside its channel against its own noise.
        const double noise_dbw = rx.sensitivity_dbw - rx.required_snr_db;
        double jam_w = 0.0;
        for (const RadioJammer& j : jammers_) {
            if (h < j.from_h || h >= j.until_h || j.erp_w <= 0.0) continue;
            const double share = em::JammerBandOverlap(j.centre_hz > 0.0 ? j.centre_hz : freq_hz_, j.bandwidth_hz, freq_hz_, rx.bandwidth_hz);
            if (share <= 0.0) continue;
            jam_w += share * DbToLinear(ReceivedDbw(j.pos, j.height_m, xb, rx.antenna_height_m, rx.antenna_gain_db, j.erp_w) - rx.system_loss_db);
        }
        const double jn = jam_w / DbToLinear(noise_dbw);
        p.margin_db = rx_dbw - (rx.sensitivity_dbw + LinearToDb(1.0 + jn));
        p.up = p.margin_db >= -3.0;
        p.loss = 1.0 - Delivered(p.margin_db);
        if (!p.up) {
            // Jammed if it would get through without the jamming; else the ground, if it would get through as far over
            // open ground; else simply too far.
            const double range_m = std::hypot(xb.x - xa.x, xb.y - xa.y);
            const double h_m = 0.5 * (tx.antenna_height_m + rx.antenna_height_m);
            const double open_dbw = em::FlatGroundRxDbw(tx, env_, std::max(range_m, 1.0), h_m, freq_hz_) + rx.antenna_gain_db - tx.antenna_gain_db -
                                    system_loss_db;
            if (rx_dbw - rx.sensitivity_dbw >= -3.0) p.why = "the net is jammed";
            else if (open_dbw - rx.sensitivity_dbw >= -3.0) p.why = "the ground is in the way";
            else p.why = "out of range";
        }
        // The enemy's listeners: each hears the call by its own margin.
        if (!listeners_.empty()) {
            double missed = 1.0;
            for (const RadioListener& l : listeners_) {
                if (h < l.from_h || h >= l.until_h) continue;
                const double at_dbw = ReceivedDbw(xa, tx.antenna_height_m, l.pos, l.height_m, l.gain_db, eirp_w);
                missed *= 1.0 - Delivered(at_dbw - l.sensitivity_dbw);
            }
            p.heard = 1.0 - missed;
        }
        return p;
    }

private:
    // The EM layer's delivery chance at a margin (em/comms_system.hpp).
    static double Delivered(double margin_db) { return 1.0 / (1.0 + std::exp(-margin_db / 2.0)); }

    // Power (dBW) received at b from a transmitter at a radiating eirp_w, over the ground, before system loss; the path's
    // loss is kept per pair of points to the metre.
    double ReceivedDbw(const Vec3& a, double ha, const Vec3& b, double hb, double rx_gain_db, double eirp_w) const {
        const std::array<long long, 6> key = {std::llround(a.x), std::llround(a.y), std::llround(ha * 10.0),
                                              std::llround(b.x), std::llround(b.y), std::llround(hb * 10.0)};
        auto it = paths_.find(key);
        if (it == paths_.end()) {
            const double alt_a = ground_->HeightAt(a.x, a.y) + ha;
            const double alt_b = ground_->HeightAt(b.x, b.y) + hb;
            const Vec3 pa{a.x, a.y, alt_a}, pb{b.x, b.y, alt_b};
            const em::PathGeometry g = em::AnalysePath(*ground_, pa, alt_a, pb, alt_b, env_.profile_spacing_m, 600, env_.k_factor);
            const double range = (pb - pa).Length();
            const em::LinkLossResult loss = em::LinkLossCore(env_, g, ha, hb, alt_a, alt_b, range, freq_hz_);
            it = paths_.emplace(key, Path{range, loss.TotalOneWayDb()}).first;
        }
        const double w = em::OneWayReceivedPowerW(eirp_w, rx_gain_db, freq_hz_, std::max(it->second.range_m, 1.0), it->second.loss_db);
        return w > 0.0 ? LinearToDb(w) : -300.0;
    }

    struct Path {
        double range_m = 0.0;
        double loss_db = 0.0;
    };
    std::vector<em::RadioDef> sets_;
    std::vector<int> set_of_;  // by position; 0 the standard set
    std::shared_ptr<const Terrain> ground_;
    em::Environment env_;
    double freq_hz_ = 0.0;
    std::vector<RadioJammer> jammers_;
    std::vector<RadioListener> listeners_;
    mutable std::map<std::array<long long, 6>, Path> paths_;
};

}  // namespace command
}  // namespace simcore
