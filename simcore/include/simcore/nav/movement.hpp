// simcore/nav/movement.hpp — coordination on the roads (roadmap G3; docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.3).
//
// The movement table: a crossing or a narrow stretch is reserved by time, unit and priority. A march plan asks for its
// slot at each crossing on its route; the higher priority holds the place, and of equal priority the earlier asker; each
// slot keeps the table's gap from the next. A later or lower column's slot moves, and its plan with it: its head is held at
// the crossing until its slot (`HoldAt`, a regulator's point in the plan, which battery.hpp's column keeps as a point's
// time). The military commandant's regulators at a crossing pass a column inside its slot, hold one that comes early and
// send one that comes after its slot to the next free one (`PassAt`). On the table: the battery's own moves, the TZM's
// runs, the regiment's batteries, the rear's convoys, the covered troops and the rebuild teams, as units by name.
//
// Exposure (G3.4): the enemy's armed reconnaissance searching a stretch of road sees a mover on the move there at the
// air war's chance for the part of the day (air_war/logistics.hpp's ColumnFindChance: by eye by day, by moonlight and
// headlights at night unless it has a night attack system); a mover halted or waiting off the road is not seen.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "simcore/air_war/logistics.hpp"
#include "simcore/nav/march.hpp"
#include "simcore/nav/movers.hpp"

namespace simcore {
namespace nav {

struct Slot {
    std::string unit;
    std::string place;  // a crossing's or a stretch's id
    int priority = 0;   // higher first
    int asked = 0;      // the order the units asked in
    double want_s = 0.0;
    double need_s = 0.0;  // the column's time to pass the place
    double from_s = 0.0;  // its slot
    double until_s = 0.0;
};

// A change to a unit's slot made by another's asking: the unit's plan holds until it is told (a battery order).
struct SlotChange {
    std::string unit;
    std::string place;
    double was_s = 0.0;
    double now_s = 0.0;
    std::string because;  // the unit that took it
};

class MovementTable {
public:
    explicit MovementTable(double gap_s = 300.0) : gap_s_(gap_s) {}

    double Gap() const { return gap_s_; }

    // A unit asks for a place from `want_s` for `need_s`: the table gives every place's slots again in order of priority
    // and asking, each at the earliest time from its wish that keeps the gap from those already given. Returns its slot.
    const Slot& Ask(const std::string& unit, const std::string& place, double want_s, double need_s, int priority) {
        Slot* s = Find(unit, place);
        if (s == nullptr) {
            slots_.push_back(Slot{});
            s = &slots_.back();
            s->unit = unit;
            s->place = place;
            s->asked = next_++;
        }
        s->priority = priority;
        s->want_s = want_s;
        s->need_s = std::max(0.0, need_s);
        Give(place, unit);
        return *Find(unit, place);
    }
    const Slot* SlotOf(const std::string& unit, const std::string& place) const {
        for (const Slot& s : slots_) {
            if (s.unit == unit && s.place == place) return &s;
        }
        return nullptr;
    }
    // The place's slots in time order.
    std::vector<Slot> At(const std::string& place) const {
        std::vector<Slot> out;
        for (const Slot& s : slots_) {
            if (s.place == place) out.push_back(s);
        }
        std::stable_sort(out.begin(), out.end(), [](const Slot& a, const Slot& b) { return a.from_s < b.from_s; });
        return out;
    }
    const std::vector<SlotChange>& Changes() const { return changes_; }

    // The regulator at the place: when the unit's head arriving at `arrive_s` may pass. Inside its slot at once; early,
    // at its slot; late (after its slot) or with no slot, the next free one, which the table gives it now.
    double PassAt(const std::string& unit, const std::string& place, double arrive_s, double need_s, int priority) {
        const Slot* s = SlotOf(unit, place);
        if (s != nullptr && arrive_s <= s->from_s) return s->from_s;
        if (s != nullptr && arrive_s < s->until_s) return arrive_s;
        return Ask(unit, place, arrive_s, need_s, priority).from_s;
    }

private:
    Slot* Find(const std::string& unit, const std::string& place) {
        for (Slot& s : slots_) {
            if (s.unit == unit && s.place == place) return &s;
        }
        return nullptr;
    }
    void Give(const std::string& place, const std::string& asker) {
        std::vector<Slot*> order;
        for (Slot& s : slots_) {
            if (s.place == place) order.push_back(&s);
        }
        std::stable_sort(order.begin(), order.end(), [](const Slot* a, const Slot* b) {
            if (a->priority != b->priority) return a->priority > b->priority;
            return a->asked < b->asked;
        });
        std::vector<const Slot*> given;
        for (Slot* s : order) {
            const double was = s->from_s;
            double t = s->want_s;
            for (bool moved = true; moved;) {
                moved = false;
                for (const Slot* g : given) {
                    if (t < g->until_s + gap_s_ && t + s->need_s + gap_s_ > g->from_s) {
                        t = g->until_s + gap_s_;
                        moved = true;
                    }
                }
            }
            s->from_s = t;
            s->until_s = t + s->need_s;
            if (s->unit != asker && std::fabs(was - t) > 1e-6) changes_.push_back({s->unit, place, was, t, asker});
            given.push_back(s);
        }
    }

    double gap_s_ = 300.0;
    int next_ = 0;
    std::vector<Slot> slots_;
    std::vector<SlotChange> changes_;
};

// ---- The march plan and the table

// The column's movement time between two places along its plan's route, at the plan's speed on each stretch.
inline double PlanMoveS(const MarchPlan& p, double from_m, double to_m) {
    double t = 0.0;
    for (std::size_t i = 0; i + 1 < p.arcs_m.size() && i < p.stretch_mps.size(); ++i) {
        const double a = std::max(from_m, p.arcs_m[i]), b = std::min(to_m, p.arcs_m[i + 1]);
        if (b > a && p.stretch_mps[i] > 0.0) t += (b - a) / p.stretch_mps[i];
    }
    return t;
}
// When the head passes a place along the route by the plan: the start point's time, the movement since, and every
// halt and hold before it.
inline double PlanHeadAt(const MarchPlan& p, double arc_m) {
    const MarchPoint* sp = p.Point("start");
    if (sp == nullptr) return 0.0;
    double t = sp->head_s + PlanMoveS(p, sp->arc_m, arc_m);
    for (const MarchPoint& pt : p.points) {
        if (pt.arc_m < arc_m - 1e-6 && pt.arc_m >= sp->arc_m - 1e-6) t += pt.halt_s + pt.hold_s;
    }
    return t;
}
// Every point's and every route card's times again from the start point's (after a hold was added).
inline void RetimeMarch(MarchPlan& p) {
    const double total = p.arcs_m.empty() ? 0.0 : p.arcs_m.back();
    for (MarchPoint& pt : p.points) {
        if (pt.kind == "start") continue;
        pt.head_s = PlanHeadAt(p, pt.arc_m) + pt.hold_s;
        pt.tail_s = pt.kind == "end" ? pt.head_s : PlanHeadAt(p, std::min(total, pt.arc_m + p.column_m));
    }
    if (!p.points.empty()) p.end_s = p.points.back().head_s;
    for (RouteCard& c : p.cards) {
        const double behind = p.spacing_m * static_cast<double>(c.place - 1);
        c.at_s.clear();
        for (const MarchPoint& pt : p.points) {
            c.at_s.push_back(pt.kind == "end" ? pt.head_s : PlanHeadAt(p, std::min(total, pt.arc_m + behind)) + (behind <= 0.0 ? pt.hold_s : 0.0));
        }
    }
}
// The head is held at a place along the route until `until_s` (its slot at a crossing): a regulator's point there, and
// every time after it later by the wait. Returns the wait (0: the plan reaches it in its slot anyway).
inline double HoldAt(MarchPlan& p, double arc_m, double until_s, const std::string& name) {
    const double head = PlanHeadAt(p, arc_m);
    const double wait = until_s - head;
    MarchPoint reg;
    reg.kind = "regulator";
    reg.name = name;
    reg.arc_m = arc_m;
    for (std::size_t i = 0; i + 1 < p.route.path.size() && i + 1 < p.arcs_m.size(); ++i) {
        if (arc_m <= p.arcs_m[i + 1] + 1e-6) {
            const double len = p.arcs_m[i + 1] - p.arcs_m[i];
            const double u = len > 0.0 ? (arc_m - p.arcs_m[i]) / len : 0.0;
            reg.at = {p.route.path[i].east_m + (p.route.path[i + 1].east_m - p.route.path[i].east_m) * u,
                      p.route.path[i].north_m + (p.route.path[i + 1].north_m - p.route.path[i].north_m) * u};
            break;
        }
    }
    reg.hold_s = std::max(0.0, wait);
    // Into the points in order along the route, and into each card at the same place.
    std::size_t at = 0;
    while (at < p.points.size() && p.points[at].arc_m <= arc_m) ++at;
    p.points.insert(p.points.begin() + static_cast<std::ptrdiff_t>(at), reg);
    RetimeMarch(p);
    return reg.hold_s;
}

// The crossings on the plan's route and where along it each is (a bridge's node on the path).
struct RouteCrossing {
    std::string id;
    double arc_m = 0.0;
};
inline std::vector<RouteCrossing> CrossingsOn(const RoadGraph& g, const MarchPlan& p) {
    std::vector<RouteCrossing> out;
    for (std::size_t i = 0; i < p.route.nodes.size() && i < p.arcs_m.size(); ++i) {
        const int node = p.route.nodes[i];
        if (node < 0) continue;
        const int c = g.Nodes()[static_cast<std::size_t>(node)].crossing;
        if (c < 0) continue;
        const std::string& id = g.Crossings()[static_cast<std::size_t>(c)].id;
        if (out.empty() || out.back().id != id) out.push_back({id, p.arcs_m[i]});
    }
    return out;
}
// The plan asks the table for a slot at each crossing on its route, from its head's time there for the time the column
// takes to pass; where the slot is later, the head is held at the crossing for it. Returns the total wait.
inline double BookCrossings(const RoadGraph& g, MovementTable& table, MarchPlan& p, const std::string& unit, int priority) {
    double waited = 0.0;
    for (const RouteCrossing& c : CrossingsOn(g, p)) {
        const double head = PlanHeadAt(p, c.arc_m);
        const Slot& s = table.Ask(unit, c.id, head, p.pass_s + 1.0, priority);
        if (s.from_s > head + 1e-6) waited += HoldAt(p, c.arc_m, s.from_s, "the regulator at " + c.id);
    }
    return waited;
}

// ---- Exposure (G3.4)

// One search of a stretch of road by the enemy's armed reconnaissance: each mover on the move within `search_m` of the
// point (its head or its tail) is seen at the part of the day's chance. The rolls are the air war's portable generator.
// At night the chance is for headlights: a mover with its lights off (blackout, roadmap C2) is seen at `blackout_factor`
// of it (data/vehicles/<system>_systems.json's lights) unless the aircraft has a night attack system.
inline std::vector<std::string> ArmedRecceSees(const Movers& movers, const MapPoint& at, double search_m, airwar::DayPart part,
                                               airwar::Weather weather, bool night_capable, const airwar::TuningTable& tune,
                                               airwar::Rng& rng, double blackout_factor = 1.0) {
    std::vector<std::string> seen;
    const double p_lit = airwar::ColumnFindChance(part, weather, night_capable, tune);
    for (const Mover& m : movers.All()) {
        if (m.state != MoverState::Moving) continue;
        const double p = !m.lit && part == airwar::DayPart::Night && !night_capable ? p_lit * blackout_factor : p_lit;
        const MapPoint h = movers.Head(m), t = movers.Tail(m);
        const double d = std::min(std::hypot(h.east_m - at.east_m, h.north_m - at.north_m), std::hypot(t.east_m - at.east_m, t.north_m - at.north_m));
        if (d > search_m) continue;
        if (rng.Chance(p)) seen.push_back(m.id);
    }
    return seen;
}

}  // namespace nav
}  // namespace simcore
