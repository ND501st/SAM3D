// simcore/air_war/logistics.hpp — the player's side's supply lines (docs/air_war/14_LOGISTICS.md).
//
// Each supply line's columns (TheatreDef::supply_lines; the loader makes their theatre objects) load at a depot, drive or
// run by road or rail to the troops, unload and come back, hour by hour. What a convoy or a train brings keeps the
// troops' stocks up; a march column brings strength (tanks and men moving up). The troops a line feeds use their stocks
// up, and once they are out lose strength and mend nothing (campaign.hpp's Repair). The roads and railways are the
// country map's networks (simcore/country_map.hpp) when the theatre names one: a bridge down sends a line round by
// another crossing, or cuts it until the bridge carries traffic again; without a map a line runs straight through its
// waypoints and a bridge down on it cuts it. The enemy finds the columns on the move by armed reconnaissance
// (MissionKind::ArmedRecce: the planner's estimate here, the search in resolve.hpp).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/country_map.hpp"

namespace simcore {
namespace airwar {

// A line's way from its depot to its troops (theatre ENU).
struct SupplyPath {
    std::vector<Vec3> pts;
    std::vector<double> at_m;            // the distance along it at each point
    std::vector<std::string> crossings;  // the bridges it uses (the map's crossing ids; without a map, bridge objects' ids)
    bool cut = false;                    // no way through: the columns on it wait

    double Length() const { return at_m.empty() ? 0.0 : at_m.back(); }

    void Measure() {
        at_m.assign(pts.size(), 0.0);
        for (std::size_t i = 1; i < pts.size(); ++i) at_m[i] = at_m[i - 1] + Dist2d(pts[i - 1], pts[i]);
    }

    Vec3 At(double s) const {
        if (pts.empty()) return Vec3{};
        if (s <= 0.0) return pts.front();
        for (std::size_t i = 1; i < pts.size(); ++i) {
            if (s <= at_m[i]) {
                const double seg = at_m[i] - at_m[i - 1];
                const double f = seg > 0.0 ? (s - at_m[i - 1]) / seg : 0.0;
                return pts[i - 1] + (pts[i] - pts[i - 1]) * f;
            }
        }
        return pts.back();
    }
};

// What the lines did, for the campaign's totals and the balance pass.
struct SupplyLedger {
    double delivered = 0.0;       // stocks brought to the troops (1: a formation's full stocks)
    double strength = 0.0;        // strength brought up by march columns
    double cargo_lost = 0.0;      // stocks lost with columns struck on their way
    double vehicles_lost = 0.0;   // trucks, wagons, tanks and carriers
    int columns_destroyed = 0;
    double line_hours_cut = 0.0;  // hours a line had no way through, summed over the lines
    int reroutes = 0;             // times a line was sent round another way
};

// The chance an aircraft searching a stretch of road sees a column on it (the period's armed reconnaissance: by eye by
// day, by moonlight and headlights at night unless it has a night attack system).
inline double ColumnFindChance(DayPart part, Weather w, bool night_capable, const TuningTable& tune) {
    double f = tune.Get("logistics.find_twilight");
    if (part == DayPart::Day) f = tune.Get("logistics.find_day");
    if (part == DayPart::Night) f = tune.Get(night_capable ? "logistics.find_night_capable" : "logistics.find_night");
    if (w == Weather::Poor) f *= tune.Get("logistics.find_poor_weather");
    return f;
}

class SupplyNetwork {
public:
    // The lines' ways at the start, and their columns spread round their rounds so they don't all leave at once.
    void Init(const AirWarData& d, std::vector<ObjectState>& truth) {
        const std::vector<SupplyLineDef>& lines = d.theatre.supply_lines;
        paths_.assign(lines.size(), SupplyPath());
        last_function_.assign(truth.size(), 1.0);
        bridge_crossings_.assign(truth.size(), std::vector<std::string>());
        served_.clear();
        ledger_ = SupplyLedger();
        if (d.country) {
            for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
                const TheatreObject& o = d.theatre.objects[i];
                if (o.kind != ObjectKind::Bridge) continue;
                for (CountryNet net : {CountryNet::Road, CountryNet::Rail}) {
                    double dist = 0.0;
                    const CountryCrossing* c = d.country->NearestCrossing(o.pos.x, o.pos.y, net, &dist);
                    if (c != nullptr && dist < 800.0) bridge_crossings_[i].push_back(c->id);
                }
            }
        }
        for (const SupplyLineDef& l : lines) {
            const int to = d.theatre.IndexOf(l.to);
            if (to >= 0 && l.column != ObjectKind::Column && std::find(served_.begin(), served_.end(), to) == served_.end()) served_.push_back(to);
        }
        cut_ = CutNow(d, truth);
        for (std::size_t li = 0; li < lines.size(); ++li) paths_[li] = Route(d, li, cut_);
        std::vector<int> seen(lines.size(), 0);
        for (std::size_t i = 0; i < truth.size(); ++i) {
            const TheatreObject& o = d.theatre.objects[i];
            if (o.supply_line < 0) continue;
            const std::size_t li = static_cast<std::size_t>(o.supply_line);
            ObjectState& st = truth[i];
            st.leg = 0;
            st.along_m = 0.0;
            st.wait_h = -Cycle(d, li) * seen[li] / std::max(1, lines[li].columns);
            st.pos = paths_[li].At(0.0);
            ++seen[li];
        }
    }

    // One step of dt hours: the bridges, the columns' rounds, the troops' stocks. `dark`: night or twilight. What the
    // lines' people would report goes to `events` (a line cut, sent round, open again; a column destroyed).
    void Step(const AirWarData& d, std::vector<ObjectState>& truth, double dt, bool dark, std::vector<std::string>* events) {
        const std::vector<SupplyLineDef>& lines = d.theatre.supply_lines;
        const TuningTable& tune = d.tuning;
        const std::set<std::string> cut = CutNow(d, truth);
        if (cut != cut_) {
            cut_ = cut;
            for (std::size_t li = 0; li < lines.size(); ++li) Reroute(d, li, truth, events);
        }
        for (const SupplyPath& p : paths_) ledger_.line_hours_cut += p.cut ? dt : 0.0;
        for (std::size_t i = 0; i < truth.size(); ++i) {
            const TheatreObject& o = d.theatre.objects[i];
            if (o.supply_line < 0) continue;
            const SupplyLineDef& l = lines[static_cast<std::size_t>(o.supply_line)];
            const SupplyPath& path = paths_[static_cast<std::size_t>(o.supply_line)];
            ObjectState& st = truth[i];
            const double before = last_function_[i];
            if (st.function < before) {
                const double lost = before - st.function;
                ledger_.vehicles_lost += lost * l.vehicles;
                if (st.leg == 1) ledger_.cargo_lost += lost * l.delivery;
            }
            if (st.function <= 0.0) {
                if (before > 0.0) {
                    ++ledger_.columns_destroyed;
                    if (events != nullptr) events->push_back(o.name + " was destroyed on its way (" + l.carries + ")");
                }
                st.leg = -1;
                last_function_[i] = 0.0;
                continue;
            }
            if (st.leg < 0) {
                // Its replacement, formed at the depot from the reserve.
                st.leg = 0;
                st.wait_h = 0.0;
                st.along_m = 0.0;
            }
            last_function_[i] = st.function;
            const bool may_move = !path.cut && (!l.night_only || dark);
            const double step_m = may_move ? l.speed_mps * 3600.0 * dt : 0.0;
            switch (st.leg) {
                case 0:
                    st.wait_h += dt;
                    if (st.wait_h >= l.load_h && may_move) {
                        st.leg = 1;
                        st.wait_h = 0.0;
                    }
                    break;
                case 1:
                    st.along_m += step_m;
                    if (st.along_m >= path.Length()) {
                        st.along_m = path.Length();
                        Arrive(d, truth, l, st.function);
                        st.leg = 2;
                        st.wait_h = 0.0;
                    }
                    break;
                case 2:
                    st.wait_h += dt;
                    if (st.wait_h >= 0.5 * l.load_h) {
                        st.leg = 3;
                        st.wait_h = 0.0;
                    }
                    break;
                case 3:
                    st.along_m -= step_m;
                    if (st.along_m <= 0.0) {
                        st.along_m = 0.0;
                        st.leg = 0;
                        st.wait_h = 0.0;
                    }
                    break;
                default: break;
            }
            st.pos = path.At(st.along_m);
        }
        // The troops' stocks: used up in the field; out of them, the formation fights at a fraction.
        const double use = tune.Get("logistics.consumption_per_day") * dt / 24.0;
        const double decay = tune.Get("logistics.unsupplied_decay_per_day") * dt / 24.0;
        const double least = tune.Get("logistics.unsupplied_floor");
        for (int j : served_) {
            ObjectState& t = truth[static_cast<std::size_t>(j)];
            if (t.function <= 0.0) continue;
            t.supply = std::max(0.0, t.supply - use);
            if (t.supply <= 0.0 && t.function > least) t.function = std::max(least, t.function - decay);
        }
    }

    // The targeteers' rule of thumb for an armed reconnaissance on line li in this part of the day: how much of its round
    // a column spends on the road, how much of the road the stretch searched covers, how many columns run. Times the
    // chance to see one there (ColumnFindChance) for the chance of a find.
    double OnStretchChance(const AirWarData& d, std::size_t li, DayPart part) const {
        if (li >= paths_.size()) return 0.0;
        const SupplyLineDef& l = d.theatre.supply_lines[li];
        const SupplyPath& p = paths_[li];
        const double len = p.Length();
        if (p.cut || len <= 0.0) return 0.0;
        if (l.night_only && part == DayPart::Day) return 0.0;  // nothing moves on it by day
        const double drive_h = 2.0 * len / (l.speed_mps * 3600.0);
        const double moving = drive_h / (drive_h + 1.5 * l.load_h);
        const double stretch = std::min(1.0, 2.0 * d.tuning.Get("logistics.search_m") / len);
        return 1.0 - std::pow(1.0 - moving * stretch, l.columns);
    }

    // Where an armed reconnaissance of line li searches: the point of its way nearest the enemy's front.
    Vec3 PatrolPoint(std::size_t li, const std::vector<Vec3>& front_line) const {
        if (li >= paths_.size() || paths_[li].pts.empty()) return Vec3{};
        const SupplyPath& p = paths_[li];
        Vec3 best = p.pts.front();
        double best_d = 1e18;
        for (double s = 0.0; s <= p.Length(); s += 2000.0) {
            const Vec3 at = p.At(s);
            const double dist = DistanceToLine(front_line, at);
            if (dist < best_d) {
                best_d = dist;
                best = at;
            }
        }
        return best;
    }

    const SupplyPath& Path(std::size_t li) const { return paths_[li]; }
    std::size_t Lines() const { return paths_.size(); }
    const SupplyLedger& Ledger() const { return ledger_; }
    // The troops the convoys and trains feed (theatre object indices).
    const std::vector<int>& Served() const { return served_; }

    // A round of line li: loading, the way there, unloading and back (hours).
    static double Cycle(const AirWarData& d, std::size_t li, double length_m) {
        const SupplyLineDef& l = d.theatre.supply_lines[li];
        return 1.5 * l.load_h + 2.0 * length_m / (l.speed_mps * 3600.0);
    }
    double Cycle(const AirWarData& d, std::size_t li) const { return Cycle(d, li, paths_[li].Length()); }

private:
    // The bridges down: a bridge carries no traffic below logistics.bridge_open (a span down, the deck holed).
    std::set<std::string> CutNow(const AirWarData& d, const std::vector<ObjectState>& truth) const {
        std::set<std::string> s;
        const double open = d.tuning.Get("logistics.bridge_open");
        for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
            const TheatreObject& o = d.theatre.objects[i];
            if (o.kind != ObjectKind::Bridge || truth[i].function >= open) continue;
            if (d.country) {
                for (const std::string& c : bridge_crossings_[i]) s.insert(c);
            } else {
                s.insert(o.id);
            }
        }
        return s;
    }

    SupplyPath Route(const AirWarData& d, std::size_t li, const std::set<std::string>& cut) const {
        const SupplyLineDef& l = d.theatre.supply_lines[li];
        std::vector<Vec3> way{d.theatre.Find(l.from)->pos};
        for (const std::string& v : l.via) way.push_back(d.theatre.Find(v)->pos);
        way.push_back(d.theatre.Find(l.to)->pos);
        SupplyPath p;
        if (d.country) {
            // The network's shortest way leg by leg, the last stretches at each end on local roads.
            const CountryNetwork& net = d.country->Network(l.net == "rail" ? CountryNet::Rail : CountryNet::Road);
            const double snap = d.tuning.Get("logistics.snap_m");
            p.pts.push_back(way.front());
            for (std::size_t k = 0; k + 1 < way.size(); ++k) {
                const MapPoint a{way[k].x, way[k].y}, b{way[k + 1].x, way[k + 1].y};
                if (net.NearestNode(a, snap) < 0 || net.NearestNode(b, snap) < 0) {
                    p.pts.push_back(way[k + 1]);  // off the network: local roads and tracks all the way
                    continue;
                }
                const CountryRoute r = net.Route(a, b, cut, snap);
                if (!r.found) {
                    p.cut = true;
                    break;
                }
                for (const MapPoint& m : r.path) p.pts.push_back(Vec3{m.east_m, m.north_m, 0.0});
                p.pts.push_back(way[k + 1]);
                p.crossings.insert(p.crossings.end(), r.crossings.begin(), r.crossings.end());
            }
            if (p.cut) p.pts = way;
        } else {
            // Straight through its waypoints; a bridge within a kilometre of the way is on it.
            p.pts = way;
            for (const TheatreObject& o : d.theatre.objects) {
                if (o.kind != ObjectKind::Bridge) continue;
                double closest = 1e18;
                for (std::size_t k = 0; k + 1 < way.size(); ++k) closest = std::min(closest, SegmentDistance2d(o.pos, way[k], way[k + 1]));
                if (closest > 1000.0) continue;
                p.crossings.push_back(o.id);
                if (cut.count(o.id) > 0) p.cut = true;
            }
        }
        p.Measure();
        return p;
    }

    // A bridge went down or carries traffic again: line li takes its way again; its columns on the way keep their share
    // of it (a cut line's columns stop where they are).
    void Reroute(const AirWarData& d, std::size_t li, std::vector<ObjectState>& truth, std::vector<std::string>* events) {
        const SupplyLineDef& l = d.theatre.supply_lines[li];
        SupplyPath np = Route(d, li, cut_);
        SupplyPath& op = paths_[li];
        const bool same = np.cut == op.cut && np.crossings == op.crossings;
        if (same) return;
        if (events != nullptr) {
            if (np.cut && !op.cut) {
                events->push_back(l.name + " is cut: no way round a bridge that is down");
            } else if (!np.cut && op.cut) {
                events->push_back(l.name + " is open again");
            } else if (!np.cut) {
                events->push_back(l.name + " goes round by another crossing (" + std::to_string(static_cast<int>(np.Length() / 1000.0 + 0.5)) +
                                  " km instead of " + std::to_string(static_cast<int>(op.Length() / 1000.0 + 0.5)) + ")");
            }
        }
        if (np.cut) {
            op.cut = true;  // the columns hold where they are on the old way
            return;
        }
        if (!op.cut) ++ledger_.reroutes;
        const double scale = op.Length() > 0.0 ? np.Length() / op.Length() : 0.0;
        for (std::size_t i = 0; i < truth.size(); ++i) {
            if (d.theatre.objects[i].supply_line != static_cast<int>(li)) continue;
            truth[i].along_m *= scale;
            truth[i].pos = np.At(truth[i].along_m);
        }
        op = np;
    }

    void Arrive(const AirWarData& d, std::vector<ObjectState>& truth, const SupplyLineDef& l, double function) {
        const int to = d.theatre.IndexOf(l.to);
        if (to < 0) return;
        ObjectState& t = truth[static_cast<std::size_t>(to)];
        if (t.function <= 0.0) return;  // nobody left to take it
        const double brought = l.delivery * function;
        if (l.column == ObjectKind::Column) {
            const double room = std::max(0.0, 1.0 - t.function);
            t.function += std::min(room, brought);
            ledger_.strength += std::min(room, brought);
        } else {
            const double room = std::max(0.0, 1.0 - t.supply);
            t.supply += std::min(room, brought);
            ledger_.delivered += std::min(room, brought);
        }
    }

    std::vector<SupplyPath> paths_;
    std::vector<double> last_function_;                      // per theatre object
    std::vector<std::vector<std::string>> bridge_crossings_;  // per theatre object: the map's crossings a bridge stands on
    std::vector<int> served_;
    std::set<std::string> cut_;
    SupplyLedger ledger_;
};

}  // namespace airwar
}  // namespace simcore
