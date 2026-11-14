// simcore/nav/movers.hpp — movers on routes and their levels of detail (roadmap G2.1; docs/campaign/11_BATTERY_LIFE_PROPOSAL.md
// §5.2).
//
// A mover is a unit or a person with a mobility class and a route from the route service: one vehicle, a column (the
// head on the route, the tail its length behind), a person on foot, a carrier (the pipeline's courier or despatch
// rider), a convoy. Its place is a distance along the route, and it goes stretch by stretch at each stretch's time, so a
// delivery takes its route's time. Its state: waiting to depart, moving, halted, blocked, broken down, arrived.
//
// Levels of detail: M0 in the campaign (its place by its plan at the campaign's step), M1 in a session out of every
// player's visual range (head and tail along the path, no ground contact), M2 within it and always the battery's own
// elements (the session's battery.hpp drives each vehicle on the ground). A hand-over between levels keeps the mover's
// place along its route: the column seen from far and the column driven past are the same column. The level follows
// the viewers' visual range, with a margin so that a mover at its edge does not flicker between levels.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "simcore/nav/routes.hpp"

namespace simcore {
namespace nav {

enum class MoverLevel { M0, M1, M2 };
enum class MoverState { Waiting, Moving, Halted, Blocked, BrokenDown, Arrived };

inline const char* MoverStateName(MoverState s) {
    switch (s) {
        case MoverState::Waiting: return "waiting";
        case MoverState::Moving: return "moving";
        case MoverState::Halted: return "halted";
        case MoverState::Blocked: return "blocked";
        case MoverState::BrokenDown: return "broken down";
        case MoverState::Arrived: return "arrived";
    }
    return "?";
}

struct Mover {
    std::string id;
    std::string kind = "vehicle";  // "vehicle", "column", "person", "carrier", "convoy"
    std::string mobility;          // its class (data/vehicles/mobility.json)
    Route route;
    double depart_s = 0.0;
    int vehicles = 1;              // a column or a convoy: how many, at `spacing_m`
    double spacing_m = 0.0;
    MoverLevel level = MoverLevel::M1;
    MoverState state = MoverState::Waiting;
    double arc_m = 0.0;            // the head's place along the route
    double moved_s = 0.0;          // time spent moving (the route's time along it)
    double arrived_s = -1.0;
    double halt_until_s = -1.0;
    bool lit = true;               // its lights on at night (false: a column under blackout, roadmap C2)

    double Length() const { return std::max(0, vehicles - 1) * spacing_m; }
};

// A point along a route's path at `arc_m` from its start (clamped to the ends), and the direction there.
inline MapPoint PointAlong(const Route& r, double arc_m, double* heading_rad = nullptr) {
    if (r.path.empty()) return MapPoint{};
    double left = std::max(0.0, arc_m);
    for (std::size_t i = 0; i + 1 < r.path.size(); ++i) {
        const MapPoint& a = r.path[i];
        const MapPoint& b = r.path[i + 1];
        const double len = std::hypot(b.east_m - a.east_m, b.north_m - a.north_m);
        if (heading_rad != nullptr && len > 0.0) *heading_rad = std::atan2(b.east_m - a.east_m, b.north_m - a.north_m);
        if (left <= len || i + 2 == r.path.size()) {
            const double f = len > 0.0 ? std::min(1.0, left / len) : 1.0;
            return {a.east_m + (b.east_m - a.east_m) * f, a.north_m + (b.north_m - a.north_m) * f};
        }
        left -= len;
    }
    return r.path.back();
}
inline double PathLength(const Route& r) {
    double s = 0.0;
    for (std::size_t i = 0; i + 1 < r.path.size(); ++i) s += std::hypot(r.path[i + 1].east_m - r.path[i].east_m, r.path[i + 1].north_m - r.path[i].north_m);
    return s;
}
// Where a mover is along its route after `moving_s` of moving: the stretches' times in order (a swim is a stretch
// whose two ends are the same point, so it takes its time where it is).
inline double ArcAfter(const Route& r, double moving_s) {
    double t = std::max(0.0, moving_s), arc = 0.0;
    for (std::size_t i = 0; i < r.stretches.size() && i + 1 < r.path.size(); ++i) {
        const RouteStretch& st = r.stretches[i];
        const double len = std::hypot(r.path[i + 1].east_m - r.path[i].east_m, r.path[i + 1].north_m - r.path[i].north_m);
        if (st.time_s <= 0.0) {
            arc += len;
            continue;
        }
        if (t < st.time_s) return arc + len * t / st.time_s;
        t -= st.time_s;
        arc += len;
    }
    return arc;
}

class Movers {
public:
    // Within `enter_m` of a viewer a mover comes to M2; it goes back to M1 only beyond `enter_m * (1 + margin)`.
    explicit Movers(double margin = 0.1) : margin_(margin) {}

    Mover& Add(const Mover& m) {
        movers_.push_back(m);
        return movers_.back();
    }
    Mover* Find(const std::string& id) {
        for (Mover& m : movers_)
            if (m.id == id) return &m;
        return nullptr;
    }
    const Mover* Find(const std::string& id) const {
        for (const Mover& m : movers_)
            if (m.id == id) return &m;
        return nullptr;
    }
    const std::vector<Mover>& All() const { return movers_; }

    // Each M0 and M1 mover to time t: it departs at its time, goes at its route's times, waits out a halt; a blocked or
    // broken-down mover stays where it is. M2 movers belong to the session, which hands their place back (SetArc).
    void Step(double t, double dt) {
        for (Mover& m : movers_) {
            if (m.level != MoverLevel::M2) Advance(m, t, dt);
        }
    }
    // One mover to time t at its route's times (a session moving an M2 mover with nothing finer to drive it uses this too,
    // so its place is the one M1 would have).
    static void Advance(Mover& m, double t, double dt) {
        if (m.state == MoverState::Waiting && t + 1e-9 >= m.depart_s) m.state = MoverState::Moving;
        if (m.state == MoverState::Halted && t + 1e-9 >= m.halt_until_s) m.state = MoverState::Moving;
        if (m.state != MoverState::Moving) return;
        const double since = std::max(std::max(m.depart_s, m.halt_until_s), t - dt);  // moving since then in this step
        m.moved_s += std::max(0.0, t - since);
        m.arc_m = ArcAfter(m.route, m.moved_s);
        if (m.moved_s + 1e-9 >= m.route.time_s) {
            m.arc_m = PathLength(m.route);
            m.state = MoverState::Arrived;
            if (m.arrived_s < 0.0) m.arrived_s = std::max(m.depart_s, t - (m.moved_s - m.route.time_s));
        }
    }
    void Halt(const std::string& id, double until_s) {
        if (Mover* m = Find(id)) {
            if (m->state == MoverState::Moving) {
                m->state = MoverState::Halted;
                m->halt_until_s = until_s;
            }
        }
    }
    void SetState(const std::string& id, MoverState s) {
        if (Mover* m = Find(id)) m->state = s;
    }
    // The session hands an M2 mover's place back (its head's distance along the route).
    void SetArc(const std::string& id, double arc_m) {
        if (Mover* m = Find(id)) m->arc_m = arc_m;
    }

    // The levels by the viewers' points (the player's eye or optic) and their visual range: M2 within it, M1 beyond its
    // margin, unchanged between; `always_m2` (the battery's own elements) are M2 whatever. Returns the ids handed over.
    // `unlit_range_m` (>= 0): the range for a mover with its lights off (at night, nav::VisualRangeUnlitM).
    std::vector<std::string> UpdateLevels(const std::vector<MapPoint>& viewers, double range_m, const std::vector<std::string>& always_m2 = {},
                                          double unlit_range_m = -1.0) {
        std::vector<std::string> changed;
        for (Mover& m : movers_) {
            if (m.level == MoverLevel::M0) continue;
            const bool own = std::find(always_m2.begin(), always_m2.end(), m.id) != always_m2.end();
            const double d = Nearest(m, viewers);
            const double r = !m.lit && unlit_range_m >= 0.0 ? std::min(range_m, unlit_range_m) : range_m;
            MoverLevel next = m.level;
            if (own || d <= r) next = MoverLevel::M2;
            else if (d > r * (1.0 + margin_)) next = MoverLevel::M1;
            if (next != m.level) {
                // The place along the route goes with it; at M1 the time moved is the time to that place.
                if (next == MoverLevel::M1) m.moved_s = TimeTo(m.route, m.arc_m);
                m.level = next;
                changed.push_back(m.id);
            }
        }
        return changed;
    }
    // The head's and the tail's points (a column's tail its length behind).
    MapPoint Head(const Mover& m) const { return PointAlong(m.route, m.arc_m); }
    MapPoint Tail(const Mover& m) const { return PointAlong(m.route, m.arc_m - m.Length()); }

    // The route's time to `arc_m` along it (the inverse of ArcAfter).
    static double TimeTo(const Route& r, double arc_m) {
        double arc = 0.0, t = 0.0;
        for (std::size_t i = 0; i < r.stretches.size() && i + 1 < r.path.size(); ++i) {
            const double len = std::hypot(r.path[i + 1].east_m - r.path[i].east_m, r.path[i + 1].north_m - r.path[i].north_m);
            if (arc + len >= arc_m && len > 0.0) return t + r.stretches[i].time_s * (arc_m - arc) / len;
            arc += len;
            t += r.stretches[i].time_s;
        }
        return t;
    }

private:
    double Nearest(const Mover& m, const std::vector<MapPoint>& viewers) const {
        double best = 1e300;
        const MapPoint h = Head(m), tl = Tail(m);
        for (const MapPoint& v : viewers) {
            best = std::min(best, std::hypot(h.east_m - v.east_m, h.north_m - v.north_m));
            best = std::min(best, std::hypot(tl.east_m - v.east_m, tl.north_m - v.north_m));
        }
        return best;
    }

    double margin_ = 0.1;
    std::vector<Mover> movers_;
};

}  // namespace nav
}  // namespace simcore
