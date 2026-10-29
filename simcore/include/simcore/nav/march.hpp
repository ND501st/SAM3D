// simcore/nav/march.hpp — the march plan and the column on it (roadmap G2.1-G2.3, G2.7; docs/campaign/11 §5.2).
//
// The battery commander's march plan, as the 1991 textbook has him organise the march (voenizdat_osa_1991 p239, 6.3.3):
// the route; a start point far enough out for the column to stretch out of its area, and the time the head passes it;
// regulation points at well-seen landmarks every 3-4 h of movement, as a rule just after a halt, with the times the
// column passes them, to keep its speed to the plan; halts of up to 1 h every 3-4 h (battery_tactics.json); the order
// of march (the PU-12M, two launchers, a TZM, two launchers, a TZM, the maintenance vehicle: pp. 232-239) and the
// spacing; the column's length and the time it takes to pass a point. Each vehicle commander has his route card: the
// plan's points with his own vehicle's times. Not yet: the meal halt of up to 2 h in the second half of a day's march,
// the day's (night's) rest beyond one day's march, a point and time for joining a covered unit's column.
//
// The column marches in battery.hpp on the plan's route at the plan's speeds and halts where the plan halts. The battery
// commander rides at the head in the PU-12M and signals by flags and lamps; radio is forbidden on the march except for
// fire control (p240), so the points passed reach the journal by his word in the cabin, not over a net.
//
// A column plans on what its commander believes (an overlay, routes.hpp). A crossing down in the truth stops the column
// when its head comes to it: the commander is the fact's first knower, says so, has it to report, and after his time at
// the map orders the march on by a new route, the column turned about where the new way leads back (G2.7).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/battery.hpp"
#include "simcore/nav/anchors.hpp"
#include "simcore/nav/routes.hpp"
#include "simcore/radio_journal.hpp"

namespace simcore {
namespace nav {

// The class each kind of battery vehicle moves as (data/vehicles/mobility.json). The maintenance vehicle's chassis is
// G0's to find: a truck until then.
inline const char* MobilityOf(BatteryVehicleKind k) {
    switch (k) {
        case BatteryVehicleKind::Launcher: return "osa_9a33";
        case BatteryVehicleKind::Reload: return "tzm_9t217";
        case BatteryVehicleKind::CommandPost: return "pu12m";
        case BatteryVehicleKind::Maintenance: return "truck";
        case BatteryVehicleKind::Fuel: return "truck";
    }
    return "truck";
}

// A vehicle of the column: the battery's (its id, call sign, kind and where it stands: start, east and north), and its
// mobility class ("" : by its kind).
struct MarchVehicle {
    BatteryVehicleDef def;
    std::string mobility;
};

struct MarchRequest {
    MapPoint from;
    MapPoint to;
    std::string from_name;  // for the plan's papers
    std::string to_name;
    std::vector<MarchVehicle> column;  // in the order of march
    double start_s = 0.0;     // the head passes the start point (session seconds)
    double move_off_s = -1.0; // >= 0: the order to move off is given then, and the start point's time follows from it
    double h = 0.0;           // the hour of the start point's time: the overlay's entries then
    bool night = false;
    bool air_threat = false;  // the column opened up for an air attack expected
    double leave_s = 0.0;     // out of the emplacements before moving off
};

struct MarchPoint {
    std::string kind;     // "start", "halt", "regulation", "swim", "end"
    std::string name;
    double arc_m = 0.0;   // along the route
    MapPoint at;
    double head_s = 0.0;  // the head passes it (at a halt: stops there)
    double tail_s = 0.0;  // the tail passes it (at a halt: after the column moves off again)
    double halt_s = 0.0;
};

// A vehicle commander's route card: his place in the column and his vehicle's time at each of the plan's points.
struct RouteCard {
    std::string vehicle;
    std::string callsign;
    BatteryVehicleKind kind = BatteryVehicleKind::Launcher;
    int place = 0;  // 1: the head
    std::vector<double> at_s;
};

// "23:00" for a session second, the session starting at `start_hour` o'clock.
inline std::string ClockHhMm(double s, double start_hour) {
    long long m = std::llround(start_hour * 60.0 + s / 60.0);
    m = ((m % 1440) + 1440) % 1440;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld", m / 60, m % 60);
    return buf;
}

struct MarchPlan {
    bool found = false;
    std::string why;
    MarchRequest request;
    Mobility column_class;            // the column's slowest in every respect
    Route route;
    std::vector<double> arcs_m;       // along the route at each of its points
    std::vector<double> stretch_mps;  // the column's speed on each stretch
    double speed_mps = 0.0;           // the march speed
    double spacing_m = 0.0;
    double column_m = 0.0;            // head to tail
    double pass_s = 0.0;              // the time the column takes to pass a point at the march speed
    double move_off_s = 0.0;          // the order to move off: the column forms and its head reaches the start point in time
    double end_s = 0.0;               // the head at the end
    std::vector<MarchPoint> points;
    std::vector<RouteCard> cards;

    const MarchPoint* Point(const std::string& kind) const {
        for (const MarchPoint& p : points) {
            if (p.kind == kind) return &p;
        }
        return nullptr;
    }

    // The order for battery.hpp: the route's points, the speeds, the points with their times and halts, the order of march.
    BatteryOrder Order() const {
        BatteryOrder o;
        o.kind = BatteryOrder::Kind::March;
        for (const MapPoint& p : route.path) o.route.push_back(Vec3{p.east_m, p.north_m, 0.0});
        o.route_mps = stretch_mps;
        for (const MarchPoint& p : points) {
            BatteryMarchPoint b;
            b.kind = p.kind;
            b.name = p.name;
            b.arc_m = p.arc_m;
            b.not_before_s = p.head_s;
            b.halt_s = p.halt_s;
            o.points.push_back(b);
        }
        for (const MarchVehicle& v : request.column) o.order.push_back(v.def.id);
        return o;
    }

    // The plan as the battery commander writes it.
    std::vector<std::string> Document(double start_hour) const {
        std::vector<std::string> out;
        char buf[400];
        out.push_back("MARCH PLAN: " + request.from_name + " to " + request.to_name + (request.night ? ", by night" : ", by day"));
        if (!found) {
            out.push_back("  no route: " + why);
            return out;
        }
        std::string lines;
        for (const RouteLeg& l : route.legs) lines += (lines.empty() ? "" : " > ") + (l.kind == "road" ? l.line_id : l.kind);
        std::snprintf(buf, sizeof(buf), "  Route: %s (%.1f km)", lines.c_str(), route.length_m / 1000.0);
        out.push_back(buf);
        std::string order;
        for (const MarchVehicle& v : request.column) {
            order += (order.empty() ? "" : ", ") + v.def.callsign + " (" + BatteryVehicleKindName(v.def.kind) + ")";
        }
        out.push_back("  Order of march: " + order);
        std::snprintf(buf, sizeof(buf), "  Spacing %.0f m; the column %.0f m long, %.0f s to pass a point; march speed %.0f km/h",
                      spacing_m, column_m, pass_s, speed_mps * 3.6);
        out.push_back(buf);
        out.push_back("  Move off: " + ClockHhMm(move_off_s, start_hour));
        for (const MarchPoint& p : points) {
            const std::string what = p.kind == "start"        ? "Start point"
                                     : p.kind == "regulation" ? "Regulation point"
                                     : p.kind == "halt"       ? "Halt"
                                     : p.kind == "swim"       ? "Swim"
                                                              : "End";
            if (p.kind == "halt" || p.kind == "swim") {
                std::snprintf(buf, sizeof(buf), "  %s: %s (km %.1f): %s-%s", what.c_str(), p.name.c_str(), p.arc_m / 1000.0,
                              ClockHhMm(p.head_s, start_hour).c_str(), ClockHhMm(p.head_s + p.halt_s, start_hour).c_str());
            } else {
                std::snprintf(buf, sizeof(buf), "  %s: %s (km %.1f): head %s, tail %s", what.c_str(), p.name.c_str(), p.arc_m / 1000.0,
                              ClockHhMm(p.head_s, start_hour).c_str(), ClockHhMm(p.tail_s, start_hour).c_str());
            }
            out.push_back(buf);
        }
        return out;
    }

    // Vehicle i's route card.
    std::vector<std::string> Card(std::size_t i, double start_hour) const {
        std::vector<std::string> out;
        if (i >= cards.size()) return out;
        const RouteCard& c = cards[i];
        out.push_back("ROUTE CARD " + c.callsign + " (" + BatteryVehicleKindName(c.kind) + "), place " + std::to_string(c.place) +
                      " in the column, " + std::to_string(static_cast<long long>(std::llround(spacing_m))) + " m behind the one ahead");
        for (std::size_t k = 0; k < points.size() && k < c.at_s.size(); ++k) {
            out.push_back("  " + points[k].kind + " " + points[k].name + ": " + ClockHhMm(c.at_s[k], start_hour));
        }
        return out;
    }
};

namespace detail {

// The column's class: its slowest vehicle's speed on each kind of ground, its least night factor, its longest time into
// the water; it swims only if every vehicle swims.
inline Mobility ColumnClass(const std::vector<const Mobility*>& classes) {
    Mobility c;
    c.id = "column";
    c.name = "the column's slowest";
    for (std::size_t r = 0; r < c.road_mps.size(); ++r) c.road_mps[r] = 1e9;
    c.cross_country_mps = 1e9;
    c.water_mps = 1e9;
    c.night_factor = 1.0;
    for (const Mobility* m : classes) {
        for (std::size_t r = 0; r < c.road_mps.size(); ++r) c.road_mps[r] = std::min(c.road_mps[r], m->road_mps[r]);
        c.cross_country_mps = std::min(c.cross_country_mps, m->cross_country_mps);
        c.water_mps = std::min(c.water_mps, m->water_mps);
        c.night_factor = std::min(c.night_factor, m->night_factor);
        c.swim_prep_s = std::max(c.swim_prep_s, m->swim_prep_s);
    }
    return c;
}

// The well-seen landmark at a point of the route: a bridge, a crossroads, or a village or town the road runs through
// (within 1 km); named by the nearest town or village within 3 km. "" where there is none.
inline std::string LandmarkAt(const RoadGraph& g, const Route& r, std::size_t i, const Anchors* names) {
    const int node = i < r.nodes.size() ? r.nodes[i] : -1;
    if (node < 0) return "";
    const NavNode& nd = g.Nodes()[static_cast<std::size_t>(node)];
    const Anchor* town = nullptr;
    double town_d = 3000.0;
    if (names != nullptr) {
        for (const Anchor& a : names->All()) {
            if (a.kind != "settlement") continue;
            const double d = std::hypot(a.at.east_m - nd.at.east_m, a.at.north_m - nd.at.north_m);
            if (d < town_d) {
                town_d = d;
                town = &a;
            }
        }
    }
    std::string what;
    if (nd.crossing >= 0) {
        what = "the bridge " + g.Crossings()[static_cast<std::size_t>(nd.crossing)].id;
    } else if (nd.edges.size() >= 3) {
        std::vector<std::string> lines;
        for (int e : nd.edges) {
            const std::string& id = g.Edges()[static_cast<std::size_t>(e)].line_id;
            if (std::find(lines.begin(), lines.end(), id) == lines.end()) lines.push_back(id);
        }
        what = "the crossroads of";
        for (std::size_t k = 0; k < lines.size(); ++k) what += (k == 0 ? " " : k + 1 == lines.size() ? " and " : ", ") + lines[k];
        if (lines.size() < 2) what = "the fork of " + lines.front();
    } else if (town != nullptr && town_d <= 1000.0) {
        return town->name;
    } else if (nd.edges.size() == 1) {
        what = "the end of " + g.Edges()[static_cast<std::size_t>(nd.edges.front())].line_id;
    } else {
        return "";
    }
    if (town != nullptr) what += " by " + town->name;
    return what;
}

}  // namespace detail

// The battery commander's march plan: the route for the column's slowest on what he believes, the start point, the
// halts and the regulation points after them, the times, and each vehicle's route card. `landmarks` names the points by
// the towns and villages near them.
inline MarchPlan PlanMarch(const RoadGraph& g, const MobilityLibrary& lib, const BatteryTactics& tactics, const MarchRequest& q,
                           const Overlay& believed = Overlay(), const Anchors* landmarks = nullptr) {
    MarchPlan p;
    p.request = q;
    std::vector<const Mobility*> classes;
    for (const MarchVehicle& v : q.column) {
        const std::string id = v.mobility.empty() ? MobilityOf(v.def.kind) : v.mobility;
        const Mobility* m = lib.Find(id);
        if (m == nullptr) {
            p.why = "no mobility class " + id + " for " + v.def.id;
            return p;
        }
        classes.push_back(m);
    }
    if (classes.empty()) {
        p.why = "no vehicles";
        return p;
    }
    p.column_class = detail::ColumnClass(classes);
    RouteRequest rq;
    rq.from = q.from;
    rq.to = q.to;
    rq.mobility = &p.column_class;
    rq.h = q.h;
    rq.night = q.night;
    rq.swim = p.column_class.Swims();
    p.route = g.Find(rq, believed);
    if (!p.route.found) {
        p.why = p.route.why;
        return p;
    }
    const Route& r = p.route;
    const double f = q.night ? p.column_class.night_factor : 1.0;
    p.speed_mps = tactics.march_speed_mps * f;
    p.spacing_m = q.air_threat ? tactics.threat_spacing_m : tactics.march_spacing_m;
    p.column_m = p.spacing_m * static_cast<double>(q.column.size() - 1);
    p.pass_s = p.column_m / p.speed_mps;

    // Along the route: each point's arc, each stretch's speed for the column, and the head's movement time to each point
    // (a river swum is a halt at its bank, so it takes no movement time).
    const std::size_t n = r.path.size();
    p.arcs_m.assign(n, 0.0);
    std::vector<double> move(n, 0.0);
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const RouteStretch& st = r.stretches[i];
        const double len = std::hypot(r.path[i + 1].east_m - r.path[i].east_m, r.path[i + 1].north_m - r.path[i].north_m);
        double v = p.speed_mps;
        if (st.kind == "road") v = std::min(v, p.column_class.RoadMps(st.road) * f);
        if (st.kind == "off_road") v = std::min(v, p.column_class.cross_country_mps * f);
        p.stretch_mps.push_back(v);
        p.arcs_m[i + 1] = p.arcs_m[i] + len;
        move[i + 1] = move[i] + len / v;
    }
    const double total_m = p.arcs_m.back();
    // The movement time to a place along the route, and back.
    auto move_at = [&](double arc) {
        if (arc <= 0.0) return 0.0;
        for (std::size_t i = 0; i + 1 < n; ++i) {
            if (arc <= p.arcs_m[i + 1] && p.arcs_m[i + 1] > p.arcs_m[i]) return move[i] + (arc - p.arcs_m[i]) / p.stretch_mps[i];
        }
        return move.back();
    };
    auto arc_at = [&](double m) {
        for (std::size_t i = 0; i + 1 < n; ++i) {
            if (m <= move[i + 1] && move[i + 1] > move[i]) return p.arcs_m[i] + (m - move[i]) * p.stretch_mps[i];
        }
        return total_m;
    };
    auto at_arc = [&](double arc) {
        for (std::size_t i = 0; i + 1 < n; ++i) {
            if (arc <= p.arcs_m[i + 1] && p.arcs_m[i + 1] > p.arcs_m[i]) {
                const double u = (arc - p.arcs_m[i]) / (p.arcs_m[i + 1] - p.arcs_m[i]);
                return MapPoint{r.path[i].east_m + (r.path[i + 1].east_m - r.path[i].east_m) * u,
                                r.path[i].north_m + (r.path[i + 1].north_m - r.path[i].north_m) * u};
            }
        }
        return r.path.back();
    };

    // The start point: the first landmark the whole column has stretched out of its area by (its head a column's length
    // along the route), else that place itself.
    MarchPoint sp;
    sp.kind = "start";
    sp.arc_m = std::min(p.column_m, total_m);
    for (std::size_t i = 1; i + 1 < n; ++i) {
        if (p.arcs_m[i] < p.column_m) continue;
        const std::string name = detail::LandmarkAt(g, r, i, landmarks);
        if (name.empty()) continue;
        sp.arc_m = p.arcs_m[i];
        sp.name = name;
        break;
    }
    if (sp.name.empty()) sp.name = "km " + std::to_string(static_cast<long long>(std::llround(sp.arc_m / 100.0)) / 10);
    sp.at = at_arc(sp.arc_m);

    std::vector<MarchPoint> pts{sp};
    // The halts: every halt_interval_s of movement while more than the longest movement without one is left; each with a
    // regulation point at the next landmark after it (within the next hour's movement).
    double since = move_at(sp.arc_m);
    const double halt_s = q.night ? tactics.night_halt_s : tactics.halt_duration_s;
    while (move.back() - since > tactics.halt_interval_max_s) {
        since += tactics.halt_interval_s;
        MarchPoint h;
        h.kind = "halt";
        h.arc_m = arc_at(since);
        h.at = at_arc(h.arc_m);
        h.halt_s = halt_s;
        h.name = "km " + std::to_string(static_cast<long long>(std::llround(h.arc_m / 1000.0)));
        if (landmarks != nullptr) {
            double best_d = 5000.0;
            for (const Anchor& a : landmarks->All()) {
                if (a.kind != "settlement") continue;
                const double d = std::hypot(a.at.east_m - h.at.east_m, a.at.north_m - h.at.north_m);
                if (d < best_d) {
                    best_d = d;
                    h.name = "near " + a.name;
                }
            }
        }
        pts.push_back(h);
        for (std::size_t i = 1; i + 1 < n; ++i) {
            if (p.arcs_m[i] <= h.arc_m || move[i] > since + 3600.0) continue;
            std::string name = detail::LandmarkAt(g, r, i, landmarks);
            // Where no crossroads, bridge or village serves, the turn off the road to the end.
            if (name.empty() && i < r.stretches.size() && r.stretches[i].kind == "off_road" && r.stretches[i - 1].kind == "road") {
                name = "the turn off " + r.stretches[i - 1].line_id;
            }
            if (name.empty()) continue;
            MarchPoint reg;
            reg.kind = "regulation";
            reg.arc_m = p.arcs_m[i];
            reg.at = r.path[i];
            reg.name = name;
            pts.push_back(reg);
            break;
        }
    }
    // A river swum: the column stops at the bank while it crosses.
    for (std::size_t i = 0; i + 1 < n; ++i) {
        if (r.stretches[i].kind != "swim") continue;
        MarchPoint s;
        s.kind = "swim";
        s.arc_m = p.arcs_m[i];
        s.at = r.path[i];
        s.halt_s = r.stretches[i].time_s;
        s.name = "the " + r.stretches[i].line_id;
        pts.push_back(s);
    }
    MarchPoint end;
    end.kind = "end";
    end.arc_m = total_m;
    end.at = r.path.back();
    end.name = q.to_name;
    pts.push_back(end);
    std::stable_sort(pts.begin(), pts.end(), [](const MarchPoint& a, const MarchPoint& b) { return a.arc_m < b.arc_m; });

    // The times: the head at a place is the start point's time, the movement from there and every halt before it.
    double start_s = q.start_s;
    double form_s = 0.0;
    {
        // To form the column: the farthest vehicle's way to its place behind the head, which stands a column's length
        // along the route (battery.hpp drives them at the march speed to their places), and a minute to close up.
        for (std::size_t k = 0; k < q.column.size(); ++k) {
            const MapPoint slot = at_arc(std::max(0.0, p.column_m - p.spacing_m * static_cast<double>(k)));
            const double d = std::hypot(slot.east_m - q.column[k].def.start.x, slot.north_m - q.column[k].def.start.y);
            form_s = std::max(form_s, d / tactics.march_speed_mps);
        }
        form_s += 60.0;
    }
    const double to_sp_s = move_at(sp.arc_m) - move_at(p.column_m);
    if (q.move_off_s >= 0.0) start_s = q.move_off_s + q.leave_s + form_s + to_sp_s;
    p.move_off_s = start_s - to_sp_s - form_s - q.leave_s;
    auto head_at = [&](double arc) {
        double t = start_s + move_at(std::min(arc, total_m)) - move_at(sp.arc_m);
        for (const MarchPoint& h : pts) {
            if (h.halt_s > 0.0 && h.arc_m < arc - 1e-6) t += h.halt_s;
        }
        return t;
    };
    for (MarchPoint& pt : pts) {
        pt.head_s = head_at(pt.arc_m);
        pt.tail_s = pt.kind == "end" ? pt.head_s : head_at(std::min(total_m, pt.arc_m + p.column_m));
    }
    p.points = pts;
    p.end_s = p.points.back().head_s;
    for (std::size_t k = 0; k < q.column.size(); ++k) {
        RouteCard c;
        c.vehicle = q.column[k].def.id;
        c.callsign = q.column[k].def.callsign;
        c.kind = q.column[k].def.kind;
        c.place = static_cast<int>(k) + 1;
        const double behind = p.spacing_m * static_cast<double>(k);
        for (const MarchPoint& pt : p.points) c.at_s.push_back(pt.kind == "end" ? pt.head_s : head_at(std::min(total_m, pt.arc_m + behind)));
        p.cards.push_back(c);
    }
    p.found = true;
    return p;
}

struct ColumnMarchSettings {
    double write_s = 20.0;    // the radiotelephonist writes a line (game value; a session takes the keeper's skill)
    double replan_s = 600.0;  // the commander at his map for a new route (game value)
    double see_m = 200.0;     // the head sees a bridge down this far ahead (game value)
};

// The column marching on its plan, headless (G2's done-when; a session will do the same): the battery's vehicles on the
// route, the battery commander at the head noting each point and saying it in the cabin, the radiotelephonist writing it
// in the journal; a crossing down in the truth stops the column at it, and the commander plans again on what he now
// knows and orders the march on.
class ColumnMarch {
public:
    using Settings = ColumnMarchSettings;

    ColumnMarch(const RoadGraph& g, const MobilityLibrary& lib, const BatteryTactics& tactics, const Anchors* landmarks = nullptr,
                Settings s = Settings())
        : g_(g), lib_(lib), tactics_(tactics), landmarks_(landmarks), s_(s) {}

    // What is (a crossing struck), and what the battery commander has been told (he plans on it).
    void SetTruth(const Overlay& truth) { truth_ = truth; }
    void Tell(const OverlayEntry& e) {
        for (const OverlayEntry& b : believed_.entries) {
            if (b.id == e.id) return;
        }
        believed_.entries.push_back(e);
    }
    const Overlay& Believed() const { return believed_; }

    // The plan on what he believes; the battery stands at its vehicles' starts, dug in for `leave_s`. The march order is
    // given at the plan's move-off time.
    bool Start(const MarchRequest& q) {
        plans_.clear();
        reports_.clear();
        journal_ = RadioJournal();
        hour_at_zero_ = q.h - q.start_s / 3600.0;
        MarchPlan p = PlanMarch(g_, lib_, tactics_, q, believed_, landmarks_);
        if (!p.found) return false;
        std::vector<BatteryVehicleDef> defs;
        for (const MarchVehicle& v : q.column) defs.push_back(v.def);
        battery_ = std::make_unique<Battery>(tactics_, defs);
        battery_->SetNight(q.night);
        battery_->SetAirThreat(q.air_threat, 0.0);
        if (q.leave_s > 0.0) battery_->SetInEmplacement(q.leave_s);
        plans_.push_back(p);
        ordered_ = false;
        logged_ = 0;
        replan_at_ = -1.0;
        return true;
    }

    void Step(double t, double dt) {
        if (battery_ == nullptr || plans_.empty()) return;
        if (replan_at_ >= 0.0 && t >= replan_at_) Replan(t);
        if (!ordered_ && t >= plans_.back().move_off_s) {
            battery_->Order(plans_.back().Order(), t);
            ordered_ = true;
        }
        battery_->Step(t, dt);
        battery_->TakeEvents();
        // Each point the head passes: the commander says it in the cabin, the radiotelephonist hears it.
        const std::vector<Battery::Passing>& passed = battery_->Passings();
        while (ordered_ && logged_ < passed.size() && passed[logged_].head_s >= 0.0) {
            const Battery::Passing& ps = passed[logged_];
            const std::string text = ps.kind == "start"        ? "the head at the start point, " + ps.name
                                     : ps.kind == "regulation" ? "the head at the regulation point, " + ps.name
                                     : ps.kind == "halt"       ? "halt, " + ps.name
                                     : ps.kind == "swim"       ? "swimming " + ps.name
                                                               : "arrived: " + ps.name;
            journal_.Heard(ps.head_s, "march", "Battery commander", text);
            ++logged_;
        }
        if (ordered_ && replan_at_ < 0.0 && battery_->CurrentMode() == Battery::Mode::Marching) Lookout(t);
        if (!journal_.Busy(t)) journal_.WriteNext(t, s_.write_s);
    }
    void Run(double from_s, double until_s, double dt = 1.0) {
        for (double t = from_s; t < until_s; t += dt) Step(t, dt);
    }

    const MarchPlan& Plan() const { return plans_.back(); }
    const std::vector<MarchPlan>& Plans() const { return plans_; }
    const Battery& Column() const { return *battery_; }
    const RadioJournal& Journal() const { return journal_; }
    // What the commander has to report up (the means are G10's): the hour and the words.
    struct Report {
        double t = 0.0;
        std::string text;
    };
    const std::vector<Report>& Reports() const { return reports_; }
    bool Arrived() const {
        if (battery_ == nullptr || !ordered_) return false;
        const std::vector<Battery::Passing>& passed = battery_->Passings();
        return !passed.empty() && passed.back().kind == "end" && passed.back().head_s >= 0.0;
    }

    // The march as the journal has it beside the plan in force: each point's planned time, and the time written against
    // it ("not written" where the page has nothing).
    std::vector<std::string> Record(double t, double start_hour) const {
        std::vector<std::string> out;
        if (plans_.empty()) return out;
        const std::vector<RadioJournalEntry> page = journal_.Legible(t);
        for (const MarchPoint& p : plans_.back().points) {
            std::string written = "not written";
            for (const RadioJournalEntry& e : page) {
                if (e.text.find(p.name) != std::string::npos && e.heard_s + 1e-6 >= plans_.back().move_off_s) written = ClockHhMm(e.heard_s, start_hour);
            }
            out.push_back(p.kind + " " + p.name + ": planned " + ClockHhMm(p.head_s, start_hour) + ", written " + written);
        }
        return out;
    }

private:
    double Hour(double t) const { return hour_at_zero_ + t / 3600.0; }

    // The head comes to a crossing down in the truth that the commander did not know of: the column stops.
    void Lookout(double t) {
        const MarchPlan& p = plans_.back();
        const double head = battery_->HeadArc();
        for (std::size_t i = 0; i < p.route.path.size(); ++i) {
            const int node = p.route.nodes[i];
            if (node < 0 || p.arcs_m[i] < head || p.arcs_m[i] > head + s_.see_m) continue;
            const int c = g_.Nodes()[static_cast<std::size_t>(node)].crossing;
            if (c < 0) continue;
            const std::string& id = g_.Crossings()[static_cast<std::size_t>(c)].id;
            const OverlayEntry* down = truth_.Down(id, Hour(t));
            if (down == nullptr || believed_.Down(id, Hour(t)) != nullptr) continue;
            // Swum by plan: the bridge was not needed.
            if (i < p.route.stretches.size() && p.route.stretches[i].kind == "swim") continue;
            battery_->Block(t, "the bridge " + id + " is down");
            Tell(*down);
            journal_.Heard(t, "march", "Battery commander", "the bridge " + id + " is down: the column stopped");
            reports_.push_back({t, "the bridge " + id + " is down; battery 21's column stopped before it, a new route to follow"});
            replan_at_ = t + s_.replan_s;
            return;
        }
    }

    // A new plan from where the column stands, on what he now believes; the column turned about if the new way leads
    // back past its tail.
    void Replan(double t) {
        replan_at_ = -1.0;
        const MarchPlan& old = plans_.back();
        MarchRequest q = old.request;
        q.leave_s = 0.0;
        q.start_s = 0.0;
        q.move_off_s = t;
        q.h = Hour(t);
        std::vector<std::pair<std::size_t, std::size_t>> by_place;
        for (std::size_t k = 0; k < battery_->Vehicles().size(); ++k) by_place.push_back({battery_->PlaceOf(k), k});
        std::sort(by_place.begin(), by_place.end());
        q.column.clear();
        for (const auto& pk : by_place) {
            MarchVehicle v;
            v.def = battery_->Vehicles()[pk.second].def;
            v.def.start = battery_->Vehicles()[pk.second].position;
            for (const MarchVehicle& o : old.request.column) {
                if (o.def.id == v.def.id) v.mobility = o.mobility;
            }
            q.column.push_back(v);
        }
        const Vec3 head = q.column.front().def.start;
        const Vec3 tail = q.column.back().def.start;
        q.from = {head.x, head.y};
        MarchPlan p = PlanMarch(g_, lib_, tactics_, q, believed_, landmarks_);
        if (p.found && p.route.path.size() >= 2) {
            // Which way the new route leaves: back toward the tail means the column turns about, the tail leading.
            const MapPoint ahead = p.route.path[std::min<std::size_t>(p.route.path.size() - 1, 2)];
            const double dot = (ahead.east_m - head.x) * (head.x - tail.x) + (ahead.north_m - head.y) * (head.y - tail.y);
            if (dot < 0.0) {
                std::reverse(q.column.begin(), q.column.end());
                q.from = {tail.x, tail.y};
                p = PlanMarch(g_, lib_, tactics_, q, believed_, landmarks_);
            }
        }
        if (!p.found) {
            journal_.Heard(t, "march", "Battery commander", "no way on: " + p.why);
            return;
        }
        std::string lines;
        for (const RouteLeg& l : p.route.legs) {
            if (l.kind == "road") lines += (lines.empty() ? "" : " > ") + l.line_id;
        }
        journal_.Heard(t, "march", "Battery commander", "a new route: " + lines);
        plans_.push_back(p);
        ordered_ = false;
        logged_ = 0;
    }

    const RoadGraph& g_;
    const MobilityLibrary& lib_;
    BatteryTactics tactics_;
    const Anchors* landmarks_;
    Settings s_;
    Overlay truth_;
    Overlay believed_;
    std::vector<MarchPlan> plans_;
    std::unique_ptr<Battery> battery_;
    RadioJournal journal_;
    std::vector<Report> reports_;
    bool ordered_ = false;
    std::size_t logged_ = 0;
    double replan_at_ = -1.0;
    double hour_at_zero_ = 0.0;
};

}  // namespace nav
}  // namespace simcore
