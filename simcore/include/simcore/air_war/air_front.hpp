// simcore/air_war/air_front.hpp — what the enemy's air staff weighs, sector by sector, before it moves its air front
// (track F, F2.2; docs/fronts/02_AIR_FRONT.md §3).
//
// Two measures, both on the enemy's own picture (IntelPicture), never the truth:
//
//   * The believed cover over a sector's next band: the sector's run of the line by air_front.step_m (20 km) beyond its
//     air front. The radar SAM fire units whose believed engagement rings reach the band, each weighed as
//     BelievedIadsShare weighs a site (BelievedThere: believed there and real), the working ones by their believed
//     working share too:
//
//         cover = sum of BelievedThere x function  /  max(sum of BelievedThere, the sector's pre-war baseline)
//
//     The baseline is the same sum over the band from the picture at the war's start, raised whenever more are found
//     there; so a unit that pulls out of the sector lowers the cover as a destroyed one does. Nothing believed there,
//     ever: no cover (0).
//   * The believed share of our mobile air defence in the band destroyed or out of action: the same sums over the army's
//     air defence (the Osa, Kub and Strela batteries and the guns: IsMobileAirDefence), one less the working share.
//     Nothing believed there, ever: all of it gone (1).
//
// The band is measured in the line of contact's own coordinates (front_line.hpp): a ring reaches it when its centre,
// placed by its distance along the line and its depth beyond the line of contact, is within its radius of the sector's
// run [from, to] by [near, far]. The end sectors run on past the line's ends.
//
// And the losses the staff counts against a sector: aircraft lost to ground fire per low-level sortie flown there, by
// day, over the last one to three days (SectorLossWindow), kept by the campaign under air_front.mode 1.
//
// Nothing reads them yet: F2.3's decision does (DecideAirFront). At air_front.mode 0 nothing here is built or measured.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/air_war/front_line.hpp"
#include "simcore/air_war/intel.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

// The army's air defence the brief calls our mobile ground assets: its SAM batteries (by the system the enemy believes
// it is) and every gun. A SAM of no known system (an unexplained launch, a ghost) counts as one of them: what pops up
// unannounced is the mobile kind.
inline bool IsMobileAirDefenceSystem(const std::string& system) {
    return system == "osa" || system == "kub" || system == "strela1" || system == "strela10";
}
inline bool IsMobileAirDefence(const IntelEntry& e) {
    if (e.kind == ObjectKind::Aaa) return true;
    return e.kind == ObjectKind::SamSite && (e.system.empty() || IsMobileAirDefenceSystem(e.system));
}

// One sector's measure.
struct SectorCover {
    double near_m = 0.0;            // the band: from this far beyond the line of contact
    double far_m = 0.0;             // to this far
    double cover = 0.0;             // the believed working share of the radar SAM fire units over the band (0..1)
    double sam_working = 0.0;       // sum of BelievedThere x function over them
    double sam_there = 0.0;         // sum of BelievedThere over them now
    double sam_baseline = 0.0;      // the most ever believed there: the pre-war count, raised when more are found
    int sam_units = 0;              // entries counted
    double mobile_destroyed = 1.0;  // the believed share of our mobile air defence there destroyed or out of action (0..1)
    double mobile_working = 0.0;
    double mobile_there = 0.0;
    double mobile_baseline = 0.0;
    int mobile_units = 0;
};

// Whether a ring (its centre's place against the line, its radius) reaches sector s's band [near_m, far_m].
inline bool RingReachesBand(const LineOfContact& line, std::size_t s, double along_m, double depth_m, double radius_m, double near_m,
                            double far_m) {
    const double from = line.SectorFrom(s), to = line.SectorTo(s);
    const double da = along_m < from ? from - along_m : (along_m > to ? along_m - to : 0.0);
    const double dd = depth_m < near_m ? near_m - depth_m : (depth_m > far_m ? depth_m - far_m : 0.0);
    const double r = radius_m > 0.0 ? radius_m : 0.0;
    return da * da + dd * dd <= r * r;
}

// The cover by sector over a front: the pre-war picture kept at the war's start, each sector's band and its baseline
// cached until the band or the line moves. One pass over the picture a measure: an entry that counts is placed on the
// line once and tested against every sector's band.
class SectorCoverBook {
public:
    // The picture at the war's start (after the prior intelligence): every entry that counts, as believed then.
    void Start(const IntelPicture& pic, const LineOfContact& line) {
        prewar_.clear();
        for (const IntelEntry& e : pic.entries) {
            const bool sam = e.IsSamRadar(), mobile = IsMobileAirDefence(e);
            if (!sam && !mobile) continue;
            Unit u;
            u.pos = e.pos;
            u.radius_m = e.threat_radius_m;
            u.weight = e.BelievedThere();
            u.sam = sam;
            u.mobile = mobile;
            prewar_.push_back(u);
        }
        state_.assign(line.SectorCount(), State());
        started_ = true;
    }
    bool Started() const { return started_; }
    std::size_t PrewarUnits() const { return prewar_.size(); }

    // Every sector's cover over the band from its air front `front_depth_m[s]` (beyond the line of contact; a missing
    // entry 0) to band_m further, into `out` (one per sector; sized here, so a kept vector allocates once).
    void Measure(const IntelPicture& pic, const LineOfContact& line, const std::vector<double>& front_depth_m, double band_m,
                 std::vector<SectorCover>& out) {
        const std::size_t n = line.SectorCount();
        if (state_.size() != n) state_.assign(n, State());
        out.resize(n);
        for (std::size_t s = 0; s < n; ++s) {
            SectorCover& c = out[s];
            c = SectorCover();
            c.near_m = s < front_depth_m.size() ? front_depth_m[s] : 0.0;
            c.far_m = c.near_m + band_m;
            State& st = state_[s];
            if (!st.set || st.near_m != c.near_m || st.far_m != c.far_m || st.line_version != line.Version()) Rebase(line, s, c.near_m, c.far_m, st);
        }
        for (const IntelEntry& e : pic.entries) {
            const bool sam = e.IsSamRadar(), mobile = IsMobileAirDefence(e);
            if (!sam && !mobile) continue;
            const FrontPlace pl = line.Place(e.pos);
            const double depth = pl.side * pl.distance_m - line.Offset(pl.along_m);
            const double there = e.BelievedThere();
            const double working = there * Clamp01(e.function);
            for (std::size_t s = 0; s < n; ++s) {
                SectorCover& c = out[s];
                if (!RingReachesBand(line, s, pl.along_m, depth, e.threat_radius_m, c.near_m, c.far_m)) continue;
                if (sam) {
                    c.sam_working += working;
                    c.sam_there += there;
                    ++c.sam_units;
                }
                if (mobile) {
                    c.mobile_working += working;
                    c.mobile_there += there;
                    ++c.mobile_units;
                }
            }
        }
        for (std::size_t s = 0; s < n; ++s) {
            SectorCover& c = out[s];
            State& st = state_[s];
            if (c.sam_there > st.sam_max) st.sam_max = c.sam_there;
            if (c.mobile_there > st.mobile_max) st.mobile_max = c.mobile_there;
            c.sam_baseline = st.sam_max;
            c.mobile_baseline = st.mobile_max;
            c.cover = st.sam_max > 0.0 ? Clamp01(c.sam_working / st.sam_max) : 0.0;
            c.mobile_destroyed = st.mobile_max > 0.0 ? Clamp01(1.0 - c.mobile_working / st.mobile_max) : 1.0;
        }
    }

private:
    struct Unit {
        Vec3 pos;
        double radius_m = 0.0;
        double weight = 0.0;
        bool sam = false;
        bool mobile = false;
    };
    struct State {
        bool set = false;
        double near_m = 0.0;
        double far_m = 0.0;
        unsigned line_version = 0;
        double sam_max = 0.0;     // the band's pre-war count, raised when more are found
        double mobile_max = 0.0;
    };
    std::vector<Unit> prewar_;
    std::vector<State> state_;
    bool started_ = false;

    // A new band (the air front moved, or the line): its baseline is the pre-war picture's count over it.
    void Rebase(const LineOfContact& line, std::size_t s, double near_m, double far_m, State& st) const {
        st = State();
        st.set = true;
        st.near_m = near_m;
        st.far_m = far_m;
        st.line_version = line.Version();
        for (const Unit& u : prewar_) {
            const FrontPlace pl = line.Place(u.pos);
            const double depth = pl.side * pl.distance_m - line.Offset(pl.along_m);
            if (!RingReachesBand(line, s, pl.along_m, depth, u.radius_m, near_m, far_m)) continue;
            if (u.sam) st.sam_max += u.weight;
            if (u.mobile) st.mobile_max += u.weight;
        }
    }
};

// The aircraft a faction loses to ground fire per low-level sortie in each sector, by day (the campaign's day: its hour
// over 24, from 0), kept for the last kDays days. A sortie is counted on the day of its package's time on target in the
// sector of its target; a loss on the same package's.
class SectorLossWindow {
public:
    static constexpr int kDays = 4;  // the window's longest, three days, and the day before it began

    void Reset(std::size_t sectors) { ring_.assign(sectors, Ring()); }
    std::size_t Sectors() const { return ring_.size(); }
    void NoteSorties(int sector, int day, int sorties) {
        if (Day* d = At(sector, day)) d->sorties += sorties;
    }
    void NoteLosses(int sector, int day, int lost) {
        if (Day* d = At(sector, day)) d->lost += lost;
    }
    // Over the `days` days (1..kDays) ending with `day`.
    int Sorties(int sector, int day, int days) const { return Sum(sector, day, days, true); }
    int Losses(int sector, int day, int days) const { return Sum(sector, day, days, false); }
    // Aircraft lost per sortie over them; 0 with no sorties flown.
    double Rate(int sector, int day, int days) const {
        const int s = Sorties(sector, day, days);
        return s > 0 ? static_cast<double>(Losses(sector, day, days)) / static_cast<double>(s) : 0.0;
    }
    static int DayOf(double hour) { return static_cast<int>(std::floor(hour / 24.0)); }

private:
    struct Day {
        int day = -1000000;
        int sorties = 0;
        int lost = 0;
    };
    using Ring = std::array<Day, kDays>;
    std::vector<Ring> ring_;

    static std::size_t Slot(int day) { return static_cast<std::size_t>(((day % kDays) + kDays) % kDays); }
    Day* At(int sector, int day) {
        if (sector < 0 || static_cast<std::size_t>(sector) >= ring_.size()) return nullptr;
        Day& d = ring_[static_cast<std::size_t>(sector)][Slot(day)];
        if (d.day != day) {
            if (d.day > day) return nullptr;  // older than the window
            d = Day();
            d.day = day;
        }
        return &d;
    }
    int Sum(int sector, int day, int days, bool sorties) const {
        if (sector < 0 || static_cast<std::size_t>(sector) >= ring_.size()) return 0;
        if (days < 1) days = 1;
        if (days > kDays) days = kDays;
        int total = 0;
        for (int k = 0; k < days; ++k) {
            const Day& d = ring_[static_cast<std::size_t>(sector)][Slot(day - k)];
            if (d.day == day - k) total += sorties ? d.sorties : d.lost;
        }
        return total;
    }
};

}  // namespace airwar
}  // namespace simcore
