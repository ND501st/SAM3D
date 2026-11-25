// simcore/command/air_front_watch.hpp — how our side learns that the enemy's air front has moved (track F, F2.5;
// docs/fronts/02_AIR_FRONT.md section 5).
//
// The enemy moves its air front in his own mind (air_war/air_front.hpp); we see only its effects. The army's air
// defence staff keeps, sector by sector and day by day, the signs that reached it: low-level attacks on our troops and
// helicopter attacks seen deeper than the enemy's close support usually works (the warning net's reports), his close
// support request nets heard naming squares that deep (our intercept service), our troops' reports of air attack (F3.8).
// Once enough signs fall in a sector within a few days, the staff states, after its working delay, that the enemy's air
// front there now reaches further: a fact for the pipeline (command/air_war_feed.hpp carries it). The staff knows only
// what its signs say: it never reads the enemy's depths, so a front moved where nobody saw anything stays unknown, and
// signs where nothing moved can mislead it.
//
// Pure: signs in (each with the hour it reached the staff), conclusions out. Per sector a few numbers and the signs of the
// last days; no per-step work.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/data.hpp"

namespace simcore {
namespace command {

// What a sign is.
enum class FrontSign {
    Attack,       // fixed-wing aircraft at low level attacked our troops by day (the warning net's report)
    Helicopters,  // helicopters attacked (the warning net's report)
    Intercept,    // a close support request heard on the enemy's request net naming a square (our intercept service)
    Troops        // our troops reported an air attack (F3.8)
};

inline const char* FrontSignName(FrontSign s) {
    switch (s) {
        case FrontSign::Attack: return "attack";
        case FrontSign::Helicopters: return "helicopters";
        case FrontSign::Intercept: return "intercept";
        case FrontSign::Troops: return "troops";
    }
    return "?";
}

// The staff's rules (data/gameplay/air_war.json "air_front", the watch_ keys; game values, docs/fronts/02 section 5).
struct AirFrontWatchRules {
    double cas_reach_m = 40000.0;   // how deep the enemy's close support works beyond the line of contact, his air front at it
    double helo_reach_m = 25000.0;  // his attack helicopters
    double margin_m = 5000.0;       // a sign counts only this much deeper than the staff expects
    double band_m = 40000.0;        // and no more than this: deeper is interdiction, not the close support's front (two steps)
    int count = 3;                  // signs in a sector within the window before the staff concludes
    int days = 2;                   // the window: this day and the days before it (campaign days)
    double delay_h = 6.0;           // the staff's work from the last sign to the stated conclusion
    double round_m = 5000.0;        // the estimate is stated in steps of this

    static AirFrontWatchRules From(const airwar::TuningTable& t) {
        AirFrontWatchRules r;
        r.cas_reach_m = t.Get("air_front.watch_cas_reach_m");
        r.helo_reach_m = t.Get("air_front.watch_helo_reach_m");
        r.margin_m = t.Get("air_front.watch_margin_m");
        r.band_m = t.Get("air_front.watch_band_m");
        r.count = std::max(1, static_cast<int>(std::lround(t.Get("air_front.watch_count"))));
        r.days = std::max(1, static_cast<int>(std::lround(t.Get("air_front.watch_days"))));
        r.delay_h = std::max(0.0, t.Get("air_front.watch_delay_h"));
        r.round_m = std::max(1.0, t.Get("air_front.watch_round_m"));
        return r;
    }
};

// One sign as it reached the staff.
struct FrontSignReport {
    FrontSign sign = FrontSign::Attack;
    int sector = 0;          // by our line of contact's sectors
    double depth_m = 0.0;    // how far beyond the line of contact, as reported
    double seen_h = 0.0;     // when our post saw or heard it
    double known_h = -1.0;   // when it reached the staff (-1: not yet)
    std::string key;         // its fact's key in the pipeline
};

// The staff's conclusion on a sector.
struct AirFrontConclusion {
    int sector = 0;
    double depth_m = 0.0;    // the staff's estimate of the enemy's air front beyond the line of contact
    double from_m = 0.0;     // its estimate before
    double decided_h = 0.0;  // when the last sign reached it
    double h = 0.0;          // when it states it (decided_h + the working delay)
    int signs = 0;           // the signs behind it
    int first_day = 0;       // the campaign day of the earliest of them
    std::string key;         // its fact's key in the pipeline ("" until the feed gives it one)
};

class AirFrontWatch {
public:
    void Init(std::size_t sectors, const AirFrontWatchRules& rules) {
        rules_ = rules;
        believed_.assign(sectors, 0.0);
        window_.assign(sectors, std::vector<std::pair<int, double>>());
        conclusions_.clear();
        taken_ = 0;
    }

    const AirFrontWatchRules& Rules() const { return rules_; }
    std::size_t Sectors() const { return believed_.size(); }
    // How deep the staff expects a sign of this kind with the enemy's air front at the line of contact.
    double Reach(FrontSign s) const { return s == FrontSign::Helicopters ? rules_.helo_reach_m : rules_.cas_reach_m; }
    // How far a reported sign lies beyond what the staff now expects in its sector.
    double Excess(const FrontSignReport& s) const { return s.depth_m - Reach(s.sign) - Believed(s.sector); }
    // The staff's estimate of a sector's air front (0: at the line of contact).
    double Believed(int sector) const {
        return sector >= 0 && static_cast<std::size_t>(sector) < believed_.size() ? believed_[static_cast<std::size_t>(sector)] : 0.0;
    }
    // The signs counted in a sector's window as it stood at the last sign.
    int Counted(int sector) const {
        return sector >= 0 && static_cast<std::size_t>(sector) < window_.size() ? static_cast<int>(window_[static_cast<std::size_t>(sector)].size()) : 0;
    }
    int Taken() const { return taken_; }
    const std::vector<AirFrontConclusion>& Conclusions() const { return conclusions_; }
    void SetKey(std::size_t conclusion, const std::string& key) {
        if (conclusion < conclusions_.size()) conclusions_[conclusion].key = key;
    }

    // A sign that reached the staff, in the order they reached it. Counted in its sector's day when it lies deeper than
    // the staff expects by more than the margin and no more than the band; true (and `out`) when it brings the window's count to the rule's: the estimate is
    // the count-th deepest of the window's signs, rounded down to the rule's step (the depth at least that many signs
    // reached), and the window starts again from it.
    bool Take(const FrontSignReport& s, AirFrontConclusion& out) {
        if (s.sector < 0 || static_cast<std::size_t>(s.sector) >= believed_.size() || s.known_h < 0.0) return false;
        ++taken_;
        const std::size_t si = static_cast<std::size_t>(s.sector);
        const int day = static_cast<int>(std::floor(s.known_h / 24.0));
        auto& w = window_[si];
        w.erase(std::remove_if(w.begin(), w.end(), [&](const std::pair<int, double>& e) { return e.first <= day - rules_.days; }), w.end());
        const double excess = Excess(s);
        if (excess <= rules_.margin_m || excess > rules_.band_m) return false;
        w.push_back({day, excess});
        if (static_cast<int>(w.size()) < rules_.count) return false;
        std::vector<double> deep;
        int first_day = day;
        for (const auto& e : w) {
            deep.push_back(e.second);
            first_day = std::min(first_day, e.first);
        }
        std::stable_sort(deep.begin(), deep.end(), [](double a, double b) { return a > b; });
        const double at_least = deep[static_cast<std::size_t>(rules_.count - 1)];
        const double step = std::max(rules_.round_m, std::floor(at_least / rules_.round_m) * rules_.round_m);
        AirFrontConclusion c;
        c.sector = s.sector;
        c.from_m = believed_[si];
        c.depth_m = believed_[si] + step;
        c.decided_h = s.known_h;
        c.h = s.known_h + rules_.delay_h;
        c.signs = static_cast<int>(w.size());
        c.first_day = first_day;
        believed_[si] = c.depth_m;
        w.clear();
        conclusions_.push_back(c);
        out = c;
        return true;
    }

private:
    AirFrontWatchRules rules_;
    std::vector<double> believed_;
    std::vector<std::vector<std::pair<int, double>>> window_;  // per sector: (day, excess) of the signs counted
    std::vector<AirFrontConclusion> conclusions_;
    int taken_ = 0;
};

}  // namespace command
}  // namespace simcore
