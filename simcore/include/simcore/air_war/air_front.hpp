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
// F2.3's decision reads them (DecideAirFront, below): each sector's air front, a depth for the fixed-wing low-level work
// and one for the helicopters, moved by the faction's rules (AirFrontDef) and its air operations chief's traits, a step at
// a time, journalled by the campaign. At air_front.mode 0 nothing here is built, measured or decided.
#pragma once

#include <algorithm>
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
        placed_version_ = 0;
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
        if (placed_version_ != line.Version()) PlacePrewar(line);
        out.resize(n);
        for (std::size_t s = 0; s < n; ++s) {
            SectorCover& c = out[s];
            c = SectorCover();
            c.near_m = s < front_depth_m.size() ? front_depth_m[s] : 0.0;
            c.far_m = c.near_m + band_m;
            State& st = state_[s];
            if (!st.set || st.near_m != c.near_m || st.far_m != c.far_m || st.line_version != line.Version()) {
                Rebase(line, s, c.near_m, c.far_m, st);
            }
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
        double along_m = 0.0;  // its place against the line the book last measured on (PlacePrewar)
        double depth_m = 0.0;
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
    unsigned placed_version_ = 0;  // the version of the line the pre-war units' places were taken on (0: none)

    // The pre-war units' places against the line: taken once a line (or its offsets) and kept for every band's baseline.
    void PlacePrewar(const LineOfContact& line) {
        for (Unit& u : prewar_) {
            const FrontPlace pl = line.Place(u.pos);
            u.along_m = pl.along_m;
            u.depth_m = pl.side * pl.distance_m - line.Offset(pl.along_m);
        }
        placed_version_ = line.Version();
    }

    // A new band (the air front moved, or the line): its baseline is the pre-war picture's count over it.
    void Rebase(const LineOfContact& line, std::size_t s, double near_m, double far_m, State& st) const {
        st = State();
        st.set = true;
        st.near_m = near_m;
        st.far_m = far_m;
        st.line_version = line.Version();
        for (const Unit& u : prewar_) {
            if (!RingReachesBand(line, s, u.along_m, u.depth_m, u.radius_m, near_m, far_m)) continue;
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

// ------------------------------------------------------------------------------------------------ the decision (F2.3)

// The believed worth of a step to each sector's low-level work (§3, the reward): our troops, columns, convoys and trains,
// bridges, command posts, depots and rail junctions the enemy believes are in the band the step brings into its reach,
// [front + reach_m, front + reach_m + band_m] beyond the line of contact (reach_m: air_front.reward_reach_m, the close
// support's depth beyond the air front), each by Live() x its value x the faction's weight for its kind (none for a
// banned kind). One pass over the picture; `out` sized here.
inline bool IsAirFrontReward(ObjectKind k) {
    switch (k) {
        case ObjectKind::Troops:
        case ObjectKind::Column:
        case ObjectKind::Convoy:
        case ObjectKind::Train:
        case ObjectKind::Bridge:
        case ObjectKind::CommandPost:
        case ObjectKind::Depot:
        case ObjectKind::RailJunction: return true;
        default: return false;
    }
}
inline void SectorRewards(const IntelPicture& pic, const LineOfContact& line, const std::vector<double>& front_depth_m, double reach_m,
                          double band_m, const double* weight, const std::vector<ObjectKind>* banned, std::vector<double>& out) {
    const std::size_t n = line.SectorCount();
    out.assign(n, 0.0);
    for (const IntelEntry& e : pic.entries) {
        if (!IsAirFrontReward(e.kind)) continue;
        if (banned != nullptr && std::find(banned->begin(), banned->end(), e.kind) != banned->end()) continue;
        const FrontPlace pl = line.Place(e.pos);
        const int s = line.SectorAt(pl.along_m);
        if (s < 0 || static_cast<std::size_t>(s) >= n) continue;
        const std::size_t si = static_cast<std::size_t>(s);
        const double near_m = (si < front_depth_m.size() ? front_depth_m[si] : 0.0) + reach_m;
        const double depth = pl.side * pl.distance_m - line.Offset(pl.along_m);
        if (depth < near_m || depth > near_m + band_m) continue;
        const double w = weight != nullptr ? weight[static_cast<int>(e.kind)] : 1.0;
        out[si] += e.Live() * e.value * w;
    }
}

// One sector's air front and what its chief is waiting on.
struct SectorFront {
    static constexpr double kNever = -1e18;
    double depth_m = 0.0;            // the fixed-wing low-level front beyond the line of contact
    double helo_depth_m = 0.0;       // the helicopters' (never past depth_m but in a crisis's one exception)
    double depth_since_h = 0.0;      // when the fixed-wing front reached its depth
    double helo_since_h = 0.0;       // when the helicopters' did
    double pending_h = kNever;       // the cycle that first saw the gates hold (kNever: they do not)
    double pending_delay_h = 0.0;    // and the chief's delay from it to the step
    int over_cycles = 0;             // cycles in a row the loss rate has been over the line
    bool contested = false;          // a band of it was once believed covered by at least air_front.min_baseline
    double surge_until_h = kNever;   // a surge's end (the rebels)
    double rest_until_h = kNever;    // and no new surge before this
    double crisis_until_h = kNever;  // a crisis's override lasts until this (F3.5)
    bool crisis_helo_used = false;   // the helicopters' one exception spent
    int advances = 0;
    int pullbacks = 0;
};

// Every sector's air front of a faction.
struct AirFrontState {
    std::vector<SectorFront> sectors;
    void Reset(std::size_t n, double now_h) {
        sectors.assign(n, SectorFront());
        for (SectorFront& f : sectors) f.depth_since_h = f.helo_since_h = now_h;
    }
    void Depths(std::vector<double>& out) const {
        out.resize(sectors.size());
        for (std::size_t i = 0; i < sectors.size(); ++i) out[i] = sectors[i].depth_m;
    }
};

// The game values the decision reads (TuningTable's "air_front.*").
struct AirFrontRules {
    double step_m = 20000.0;
    double min_baseline = 1.0;
    double min_sorties = 6.0;
    double helo_lag_h = 24.0;
    double trait_shift = 0.2;
    double reward_reach_m = 40000.0;
};

// The officer who decides (the air operations chief: the planner's post) as the decision reads him.
struct AirFrontChief {
    Traits traits;
    double stress = 0.1;
};

// What else it reads, all of it the enemy's own: its picture and its measures of it, its losses, its politics.
struct AirFrontInputs {
    const IntelPicture* picture = nullptr;
    const LineOfContact* line = nullptr;
    const std::vector<SectorCover>* cover = nullptr;  // this cycle's measure (SectorCoverBook::Measure at the current depths)
    const SectorLossWindow* losses = nullptr;
    const double* target_weight = nullptr;            // the faction's weight by ObjectKind (null: 1)
    const std::vector<ObjectKind>* banned = nullptr;  // kinds its politics bans now
    double politics_max_depth_m = 0.0;                // > 0: no target deeper than this from the border (the politics' now)
    bool event_fired = false;                         // a border's after_event has fired
    const std::vector<char>* crisis = nullptr;        // its own ground front collapsing, by sector (F3.5; null: none yet)
    double now_h = 0.0;
};

// One move of a sector's air front, for the journal.
struct AirFrontMove {
    enum class What { Seen, Advance, PullBack, SurgeEnd, Politics, Helicopters, HeloBack, Crisis };
    What what = What::Advance;
    int sector = 0;
    double depth_from_m = 0.0;
    double depth_to_m = 0.0;
    double helo_from_m = 0.0;
    double helo_to_m = 0.0;
    double cover = 0.0;
    double mobile_destroyed = 0.0;
    double reward = 0.0;
    double loss_rate = 0.0;
    double after_h = 0.0;  // Seen: the delay to the step
    int sorties = 0;
};

// A gate moved by a trait in [-1, 1], by at most `shift` of its value.
inline double TraitMoved(double gate, double t, double shift) { return gate * (1.0 + shift * t); }

// The air staff's decision at a planning cycle (§3), sector by sector, on its picture: never the truth. Pure: the state
// in, the state out, the moves for the journal. At most one move a sector a cycle. `reward_scratch`: a kept vector, so a
// cycle allocates nothing once sized (null: a local one).
inline void DecideAirFront(const AirFrontDef& def, const AirFrontRules& rules, const AirFrontInputs& in, const AirFrontChief& chief,
                           AirFrontState& st, std::vector<AirFrontMove>& moves, std::vector<double>* reward_scratch = nullptr,
                           std::vector<double>* depth_scratch = nullptr) {
    moves.clear();
    if (!def.present || in.line == nullptr || in.cover == nullptr || in.picture == nullptr) return;
    const LineOfContact& line = *in.line;
    const std::size_t n = line.SectorCount();
    if (st.sectors.size() != n) st.Reset(n, in.now_h);
    std::vector<double> local_depth, local_reward;
    std::vector<double>& depth_now = depth_scratch != nullptr ? *depth_scratch : local_depth;
    std::vector<double>& reward = reward_scratch != nullptr ? *reward_scratch : local_reward;
    st.Depths(depth_now);
    SectorRewards(*in.picture, line, depth_now, rules.reward_reach_m, rules.step_m, in.target_weight, in.banned, reward);

    const Traits& tr = chief.traits;
    const double bold = 2.0 * (Clamp01(tr.boldness) - 0.5);
    const double ambition = 2.0 * (Clamp01(tr.ambition) - 0.5);
    const double gate_cover = Clamp01(TraitMoved(def.cover_at_most, bold, rules.trait_shift));
    const double gate_mobile = Clamp01(TraitMoved(def.mobile_destroyed_at_least, -bold, rules.trait_shift));
    const double loss_line = TraitMoved(def.loss_line, bold, rules.trait_shift);
    const double min_reward = TraitMoved(def.min_reward, -ambition, rules.trait_shift);
    // A methodical chief takes the long end of the delay, a bold one the short.
    const double delay_h = def.delay_lo_h + (def.delay_hi_h - def.delay_lo_h) * Clamp01(0.5 + 0.5 * (tr.methodical - tr.boldness));
    const double surge_h = def.surge_lo_h + (def.surge_hi_h - def.surge_lo_h) * Clamp01(tr.boldness);
    const double crisis_h = def.crisis_lo_h + (def.crisis_hi_h - def.crisis_lo_h) * Clamp01(tr.boldness);
    // A composed chief waits a cycle longer before he pulls back; one under stress pulls back at once.
    const int pull_cycles = (tr.composure >= 0.7 && chief.stress < 0.6) ? 2 : 1;
    const int day = SectorLossWindow::DayOf(in.now_h);
    const bool may_move = def.kind != AirFrontKind::Border || in.event_fired;

    for (std::size_t s = 0; s < n && s < in.cover->size(); ++s) {
        SectorFront& f = st.sectors[s];
        const SectorCover& c = (*in.cover)[s];
        const int si = static_cast<int>(s);
        AirFrontMove m;
        m.sector = si;
        m.depth_from_m = f.depth_m;
        m.helo_from_m = f.helo_depth_m;
        m.cover = c.cover;
        m.mobile_destroyed = c.mobile_destroyed;
        m.reward = reward[s];
        const int sorties = in.losses != nullptr ? in.losses->Sorties(si, day, def.loss_days) : 0;
        const double rate = in.losses != nullptr ? in.losses->Rate(si, day, def.loss_days) : 0.0;
        m.sorties = sorties;
        m.loss_rate = rate;
        auto emit = [&](AirFrontMove::What what) {
            m.what = what;
            m.depth_to_m = f.depth_m;
            m.helo_to_m = f.helo_depth_m;
            moves.push_back(m);
        };
        if (c.sam_baseline >= rules.min_baseline || c.mobile_baseline >= rules.min_baseline) f.contested = true;
        // The deepest it may go here: its own greatest depth and the politics', which is measured from the border.
        double cap = def.max_depth_m;
        if (in.politics_max_depth_m > 0.0) {
            const double mid = 0.5 * (std::max(0.0, line.Sector(s).from_m) + std::min(line.Length(), line.Sector(s).to_m));
            cap = std::min(cap, in.politics_max_depth_m - line.Offset(mid));
        }
        if (!may_move) cap = 0.0;
        cap = std::max(0.0, cap);
        const bool over = sorties >= rules.min_sorties && rate > loss_line;
        const bool crisis_now = in.crisis != nullptr && s < in.crisis->size() && (*in.crisis)[s] != 0;
        const bool in_crisis = in.now_h < f.crisis_until_h;

        // The politics (or the event's absence) holds it shallower than it is: back to the deepest whole step allowed.
        if (f.depth_m > cap + 1e-6) {
            f.depth_m = std::floor(cap / rules.step_m + 1e-9) * rules.step_m;
            f.helo_depth_m = std::min(f.helo_depth_m, f.depth_m);
            f.depth_since_h = f.helo_since_h = in.now_h;
            f.pending_h = SectorFront::kNever;
            emit(AirFrontMove::What::Politics);
            continue;
        }
        // A crisis on its own ground front (F3.5): the override, once a crisis.
        if (crisis_now && !in_crisis && def.crisis != AirFrontCrisis::None) {
            f.crisis_until_h = in.now_h + crisis_h;
            if (def.crisis == AirFrontCrisis::Advance && f.depth_m + rules.step_m <= cap + 1e-6) {
                f.depth_m += rules.step_m;
                f.depth_since_h = in.now_h;
                f.pending_h = SectorFront::kNever;
                ++f.advances;
                emit(AirFrontMove::What::Crisis);
                continue;
            }
            if (def.crisis == AirFrontCrisis::Helicopters && !f.crisis_helo_used) {
                f.crisis_helo_used = true;
                f.helo_depth_m = f.depth_m + rules.step_m;  // the helicopter rule broken: a step ahead of the fixed-wing front
                f.helo_since_h = in.now_h;
                emit(AirFrontMove::What::Crisis);
                continue;
            }
        }
        // The crisis over: the helicopters back behind the fixed-wing front.
        if (!(in.now_h < f.crisis_until_h) && f.helo_depth_m > f.depth_m + 1e-6) {
            f.helo_depth_m = f.depth_m;
            f.helo_since_h = in.now_h;
            emit(AirFrontMove::What::HeloBack);
            continue;
        }
        // The surge's days are up: back to the border, and a rest before the next.
        if (def.kind == AirFrontKind::Surge && f.depth_m > 0.0 && in.now_h >= f.surge_until_h) {
            f.depth_m = 0.0;
            f.helo_depth_m = 0.0;
            f.depth_since_h = f.helo_since_h = in.now_h;
            f.rest_until_h = in.now_h + def.surge_rest_h;
            f.pending_h = SectorFront::kNever;
            ++f.pullbacks;
            emit(AirFrontMove::What::SurgeEnd);
            continue;
        }
        // Losses over the line pull it back a step (not while a crisis's override lasts).
        const bool override_losses = in_crisis && def.crisis == AirFrontCrisis::Advance;
        if (f.depth_m > 0.0 && over && !override_losses) {
            if (++f.over_cycles >= pull_cycles) {
                f.depth_m = std::max(0.0, f.depth_m - rules.step_m);
                f.helo_depth_m = std::min(f.helo_depth_m, f.depth_m);
                f.depth_since_h = f.helo_since_h = in.now_h;
                f.pending_h = SectorFront::kNever;
                f.over_cycles = 0;
                ++f.pullbacks;
                emit(AirFrontMove::What::PullBack);
                continue;
            }
        } else {
            f.over_cycles = 0;
        }
        // The gates: a band once believed covered, its cover now broken, our mobile air defence there believed destroyed,
        // something worth flying for, no losses over the line, and room for a step.
        const bool open = may_move && f.contested && c.cover <= gate_cover + 1e-9 && c.mobile_destroyed >= gate_mobile - 1e-9 &&
                          reward[s] >= min_reward && (!over || override_losses) && f.depth_m + rules.step_m <= cap + 1e-6 &&
                          (def.kind != AirFrontKind::Surge || in.now_h >= f.rest_until_h);
        if (!open) {
            f.pending_h = SectorFront::kNever;
        } else if (f.pending_h == SectorFront::kNever) {
            f.pending_h = in.now_h;
            f.pending_delay_h = delay_h;
            m.after_h = delay_h;
            if (delay_h > 1e-9) {
                emit(AirFrontMove::What::Seen);
                continue;
            }
        }
        if (open && in.now_h >= f.pending_h + f.pending_delay_h - 1e-9) {
            f.depth_m += rules.step_m;
            f.depth_since_h = in.now_h;
            f.pending_h = SectorFront::kNever;
            if (def.kind == AirFrontKind::Surge) f.surge_until_h = in.now_h + surge_h;
            ++f.advances;
            emit(AirFrontMove::What::Advance);
            continue;
        }
        // The helicopters follow a step at a time, each after a day of clean fixed-wing low-level work in the sector.
        if (f.helo_depth_m + 1e-6 < f.depth_m && in.now_h - std::max(f.depth_since_h, f.helo_since_h) >= rules.helo_lag_h - 1e-9 &&
            in.losses != nullptr) {
            const int flown = in.losses->Sorties(si, day, 2);
            const double recent = in.losses->Rate(si, day, 2);
            if (flown >= 1 && recent <= loss_line) {
                f.helo_depth_m = std::min(f.depth_m, f.helo_depth_m + rules.step_m);
                f.helo_since_h = in.now_h;
                m.sorties = flown;
                m.loss_rate = recent;
                emit(AirFrontMove::What::Helicopters);
            }
        }
    }
}

}  // namespace airwar
}  // namespace simcore
