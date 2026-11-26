// simcore/nav/routes.hpp — the route service, first form (roadmap G1; docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.1).
//
// Everything that travels asks one service the way between two anchors (places, positions, sites: by their points) for
// its mobility class (data/vehicles/mobility.json). The navigation data is derived from the map's data, never authored:
// one road graph built at load from the country's roads and a district's own (its tracks to the sites), joined where
// their vertices meet, with the rivers' crossings as the country map names them. Changes in play are overlays (a bridge
// down from an hour): a unit plans on the overlay it believes, which may not be the truth.
//
// Basic, not clever (G-4): roads first, a straight off-road link from each end to the nearest road, a straight
// cross-country move between ends near each other; Dijkstra on integer times (tenths of a second, lengths to the
// centimetre, ties by node index), so every compiler gives the same route. A swimmer may swim a river where a road
// reaches it and its bridge is down. Left: trafficability tiles (G1.3), slopes, the foot layer round the pond.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/country_map.hpp"
#include "simcore/nav/ground.hpp"
#include "simcore/terrain_map.hpp"

namespace simcore {
namespace nav {

// ---------------------------------------------------------------------------------------------- mobility classes

enum class Road { Main, Secondary, Track, Count };

inline const char* RoadName(Road r) {
    switch (r) {
        case Road::Main: return "main";
        case Road::Secondary: return "secondary";
        case Road::Track: return "track";
        case Road::Count: break;
    }
    return "?";
}

// A map line's class as a road: "main", "secondary", anything else a track.
inline Road RoadOf(const std::string& line_class) {
    if (line_class == "main") return Road::Main;
    if (line_class == "secondary") return Road::Secondary;
    return Road::Track;
}

struct Mobility {
    std::string id;
    std::string name;
    std::array<double, static_cast<std::size_t>(Road::Count)> road_mps{};  // by Road
    double cross_country_mps = 0.0;
    double water_mps = 0.0;  // 0: does not swim
    double night_factor = 1.0;
    double swim_prep_s = 0.0;  // into the water and out again
    GroundSpeeds ground_factor = {1.0, 1.0, 1.0, 1.0, 1.0};  // its cross-country speed's factor on each ground (by Ground)
    std::string source;
    std::string confidence;
    std::string notes;

    bool Swims() const { return water_mps > 0.0; }
    double RoadMps(Road r) const { return road_mps[static_cast<std::size_t>(r)]; }
    // Its speed on each ground off the road (ground.hpp): cross-country times its factor there; water only swimming.
    GroundSpeeds GroundMps() const {
        GroundSpeeds s{};
        for (std::size_t k = 0; k < s.size(); ++k) s[k] = static_cast<Ground>(k) == Ground::Water ? water_mps : cross_country_mps * ground_factor[k];
        return s;
    }
};

struct MobilityLibrary {
    std::vector<Mobility> classes;

    const Mobility* Find(const std::string& id) const {
        for (const Mobility& m : classes) {
            if (m.id == id) return &m;
        }
        return nullptr;
    }
};

namespace detail {

class MobilityLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, MobilityLibrary& out) {
        std::string id;
        if (!Keys(root, "", {"mobility_id", "status", "notes", "source", "confidence", "classes"}) ||
            !RequireString(root, "", "mobility_id", id) || !Provenance(root, "")) {
            return false;
        }
        const json::Value* list = OptionalArray(root, "", "classes");
        if (list == nullptr) return Fail("classes", "required");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const json::Value& v = list->items[i];
            const std::string p = Index("classes", i);
            Mobility m;
            double main = 0.0, secondary = 0.0, track = 0.0, cross = 0.0, water = 0.0;
            if (!Keys(v, p, {"id", "name", "main_kmh", "secondary_kmh", "track_kmh", "cross_country_kmh", "water_kmh", "night_factor",
                             "swim_prep_s", "ground", "source", "confidence", "notes"}) ||
                !RequireString(v, p, "id", m.id) || !RequireString(v, p, "name", m.name) || !Provenance(v, p) ||
                !RequireString(v, p, "source", m.source) || !RequireString(v, p, "confidence", m.confidence) ||
                !OptionalString(v, p, "notes", m.notes) || !Speed(v, p, "main_kmh", main) || !Speed(v, p, "secondary_kmh", secondary) ||
                !Speed(v, p, "track_kmh", track) || !Speed(v, p, "cross_country_kmh", cross) ||
                !OptionalNumber(v, p, "water_kmh", water) || !OptionalNumber(v, p, "night_factor", m.night_factor) ||
                !OptionalNumber(v, p, "swim_prep_s", m.swim_prep_s)) {
                return false;
            }
            if (water < 0.0) return Fail(Join(p, "water_kmh"), "must not be negative");
            if (m.night_factor <= 0.0 || m.night_factor > 1.0) return Fail(Join(p, "night_factor"), "must be in (0, 1]");
            if (out.Find(m.id) != nullptr) return Fail(Join(p, "id"), "'" + m.id + "' is listed twice");
            // Off the road on each ground: woods, marsh and built-up ground as a factor of the cross-country speed (0: not at
            // all); the open is the cross-country speed itself, water the swimming speed.
            if (const json::Value* g = v.Find("ground")) {
                const std::string gp = Join(p, "ground");
                if (!Keys(*g, gp, {"woods", "marsh", "built_up", "source", "confidence"}) || !Provenance(*g, gp)) return false;
                for (Ground k : {Ground::Woods, Ground::Marsh, Ground::BuiltUp}) {
                    double& factor = m.ground_factor[static_cast<std::size_t>(k)];
                    if (!OptionalNumber(*g, gp, GroundName(k), factor)) return false;
                    if (factor < 0.0 || factor > 1.0) return Fail(Join(gp, GroundName(k)), "must be in [0, 1]");
                }
            }
            m.road_mps = {main / 3.6, secondary / 3.6, track / 3.6};
            m.cross_country_mps = cross / 3.6;
            m.water_mps = water / 3.6;
            out.classes.push_back(m);
        }
        return true;
    }

private:
    bool Speed(const json::Value& v, const std::string& p, const char* key, double& out) {
        if (v.Find(key) == nullptr) return Fail(Join(p, key), "required");
        if (!OptionalNumber(v, p, key, out)) return false;
        if (out <= 0.0) return Fail(Join(p, key), "must be positive");
        return true;
    }
};

}  // namespace detail

inline bool LoadMobilityJson(const std::string& text, MobilityLibrary& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) {
        if (error != nullptr) *error = "mobility: " + parse_error;
        return false;
    }
    detail::MobilityLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "mobility: " + loader.Message();
        return false;
    }
    return true;
}

// data/<file> ("vehicles/mobility.json").
inline bool LoadMobility(const std::string& data_dir, const std::string& file, MobilityLibrary& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + file, text)) {
        if (error != nullptr) *error = "cannot open " + file;
        return false;
    }
    return LoadMobilityJson(text, out, error);
}

// ---------------------------------------------------------------------------------------------- overlays

// A change in play (G-3): a crossing down from one hour until another (a bridge struck, until it is rebuilt), and the
// fact it came from. The truth has every entry; a unit's believed overlay the ones that reached it.
struct OverlayEntry {
    std::string id;
    std::string crossing;  // a crossing's id ("b_ralje@rava")
    double from_h = -1e18;
    double until_h = 1e18;
    std::string fact;  // the pipeline's fact that tells of it ("" none)
};

struct Overlay {
    std::vector<OverlayEntry> entries;
    // Bumped by every change made through Cut and Clear (a route cache's key, RouteCache below; F3.6). Code that edits
    // `entries` directly and caches routes bumps it itself.
    std::uint64_t version = 0;

    void Cut(const std::string& id, const std::string& crossing, double from_h, double until_h = 1e18, const std::string& fact = "") {
        entries.push_back({id, crossing, from_h, until_h, fact});
        ++version;
    }
    void Clear() {
        entries.clear();
        ++version;
    }
    // The entry that has the crossing down at hour h, or null.
    const OverlayEntry* Down(const std::string& crossing, double h) const {
        for (const OverlayEntry& e : entries) {
            if (e.crossing == crossing && h >= e.from_h && h < e.until_h) return &e;
        }
        return nullptr;
    }
};

// ---------------------------------------------------------------------------------------------- routes

struct RouteRequest {
    MapPoint from;
    MapPoint to;
    const Mobility* mobility = nullptr;
    double h = 0.0;            // the hour it is planned for: the overlay's entries then
    bool night = false;        // every speed by the class's night factor
    bool swim = true;          // a swimmer may swim a river whose bridge is down
    double link_max_m = 3000.0;  // ends this near each other may go straight across country
};

struct RouteLeg {
    std::string kind;     // "road", "off_road", "swim"
    Road road = Road::Track;
    std::string line_id;  // the road's line, or the river swum
    double length_m = 0.0;
    double time_s = 0.0;
};

// One stretch of the path, from path[i] to path[i + 1], at the class's speed there. A swim is a stretch at a crossing:
// its point twice, as long as the river is wide. A mover goes along the path stretch by stretch (nav/march.hpp).
using RouteStretch = RouteLeg;

struct Route {
    bool found = false;
    std::string why;  // when not found
    double length_m = 0.0;
    double time_s = 0.0;
    std::vector<MapPoint> path;
    std::vector<RouteStretch> stretches;  // one for each step of the path
    std::vector<int> nodes;               // the graph's node at each point of the path (-1: an end, or a point on an edge)
    std::vector<RouteLeg> legs;           // the stretches of one road joined
    std::vector<std::string> crossings;  // the crossings used, in order (bridges, or rivers swum where a bridge is down)
    std::vector<std::string> depends;    // the overlay entries it was planned round: when one changes, plan again
    double max_link_m = 0.0;             // the longer way off the road at an end (beyond the request's bound: no road near)

    // A digest of the route, to compare compilers (G-4).
    std::uint64_t Hash() const {
        std::uint64_t h = 1469598103934665603ull;
        auto mix = [&h](std::int64_t v) {
            for (int i = 0; i < 8; ++i) {
                h ^= static_cast<std::uint64_t>((v >> (8 * i)) & 0xff);
                h *= 1099511628211ull;
            }
        };
        mix(std::llround(time_s * 10.0));
        for (const MapPoint& p : path) {
            mix(std::llround(p.east_m * 100.0));
            mix(std::llround(p.north_m * 100.0));
        }
        return h;
    }
    bool Uses(const std::string& crossing) const { return std::find(crossings.begin(), crossings.end(), crossing) != crossings.end(); }
};

struct NavCrossing {
    std::string id;  // the country map's ("<line>@<river>", a second of the pair "#2")
    std::string river_id;
    double river_width_m = 0.0;
    MapPoint at;
};

struct NavNode {
    MapPoint at;
    int crossing = -1;
    std::vector<int> edges;
};

struct NavEdge {
    int a = 0;
    int b = 0;
    std::int64_t length_cm = 0;
    Road road = Road::Track;
    std::string line_id;
};

class RoadGraph {
public:
    // The graph of `roads`, crossing `rivers`: line vertices within 50 m are one junction, a crossing where a road meets
    // a river (shared by roads meeting on it within 30 m), named as the country map names them (country_map.hpp).
    static RoadGraph Build(const std::vector<const MapLineDef*>& roads, const std::vector<MapLineDef>& rivers) {
        RoadGraph g;
        g.rivers_ = rivers;
        for (const MapLineDef* l : roads) g.AddLine(*l);
        return g;
    }
    // The country's roads, and a district's roads the country does not have (its tracks to the sites).
    static RoadGraph ForCountry(const CountryDef& country, const MapDef* district = nullptr) {
        std::vector<const MapLineDef*> roads;
        for (const MapLineDef& l : country.ground.roads) roads.push_back(&l);
        RoadGraph g = Build(roads, country.ground.rivers);
        if (district != nullptr) {
            for (const MapLineDef& l : district->roads) {
                bool known = false;
                for (const MapLineDef& c : country.ground.roads) known = known || c.id == l.id;
                if (known || l.points.size() < 2) continue;
                // A district's track that ends on a road between its bends joins it there (track O, O6: a district's
                // tracks off the country's roads); the country's own graph (the ground war's) is never split.
                g.JoinAt(l.points.front());
                g.JoinAt(l.points.back());
                g.AddLine(l);
            }
        }
        return g;
    }

    const std::vector<NavNode>& Nodes() const { return nodes_; }
    const std::vector<NavEdge>& Edges() const { return edges_; }
    const std::vector<NavCrossing>& Crossings() const { return crossings_; }
    const NavCrossing* Crossing(const std::string& id) const {
        for (const NavCrossing& c : crossings_) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }
    const NavCrossing* NearestCrossing(const MapPoint& p, double* dist = nullptr) const {
        const NavCrossing* best = nullptr;
        double best_d = 1e300;
        for (const NavCrossing& c : crossings_) {
            const double d = std::hypot(c.at.east_m - p.east_m, c.at.north_m - p.north_m);
            if (d < best_d) {
                best_d = d;
                best = &c;
            }
        }
        if (dist != nullptr) *dist = best_d;
        return best;
    }

    // The district's ground cell by cell (ground.hpp): the ways off the road go round its woods, marshes and water. None
    // (the country): straight lines.
    void SetGround(std::shared_ptr<const GroundGrid> grid) { ground_ = std::move(grid); }
    const GroundGrid* GetGround() const { return ground_.get(); }

    // How far a point is from the nearest road (a place out of reach of the road net is for nav_check to list).
    double DistanceToRoad(const MapPoint& p) const {
        const Snap s = SnapTo(p);
        return s.edge < 0 ? 1e300 : static_cast<double>(s.off_cm) / 100.0;
    }

    // The quickest way for the request's class from one point to the other, on `overlay` (what the planner believes).
    Route Find(const RouteRequest& q, const Overlay& overlay = Overlay()) const {
        Route r;
        if (q.mobility == nullptr) {
            r.why = "no mobility class";
            return r;
        }
        const Mobility& m = *q.mobility;
        const double f = q.night ? m.night_factor : 1.0;
        // Speeds in centimetres a second, so every time is an integer of tenths of a second.
        auto cmps = [f](double mps) { return std::max<std::int64_t>(1, std::llround(mps * f * 100.0)); };
        std::array<std::int64_t, static_cast<std::size_t>(Road::Count)> road_v{};
        for (std::size_t i = 0; i < road_v.size(); ++i) road_v[i] = cmps(m.road_mps[i]);
        const std::int64_t off_v = cmps(m.cross_country_mps);
        auto ds = [](std::int64_t length_cm, std::int64_t v) { return (length_cm * 10 + v / 2) / v; };
        auto cm = [](double a_e, double a_n, double b_e, double b_n) { return std::llround(std::hypot(b_e - a_e, b_n - a_n) * 100.0); };
        // A way off the road between two points: across the district's ground cell by cell where a grid is laid (round
        // the woods and the water), else straight at the class's cross-country speed. Not found on the grid (an end shut
        // in by water): straight, as without one.
        const GroundSpeeds ground_mps = m.GroundMps();
        struct Off {
            bool grid = false;
            std::int64_t time_ds = 0;
            std::vector<MapPoint> points;
            std::vector<std::int64_t> at_ds;
        };
        auto off_way = [&](const MapPoint& a, const MapPoint& b, std::int64_t straight_cm) {
            Off o;
            if (ground_ && ground_->Covers(a) && ground_->Covers(b)) {
                GroundGrid::Way w = ground_->Across(a, b, ground_mps, f);
                if (w.found) {
                    o.grid = true;
                    o.time_ds = w.ds;
                    o.points = std::move(w.points);
                    o.at_ds = std::move(w.at_ds);
                    return o;
                }
            }
            o.time_ds = ds(straight_cm, off_v);
            return o;
        };

        // Each end's way onto the road: the nearest point of the road net, however far (a point beyond the bound is
        // reached across country all the same, and the route says how far: max_link_m).
        const Snap s = SnapTo(q.from);
        const Snap t = SnapTo(q.to);
        const Off s_off = s.edge >= 0 ? off_way(q.from, s.on, s.off_cm) : Off();
        const Off t_off = t.edge >= 0 ? off_way(t.on, q.to, t.off_cm) : Off();
        const std::int64_t direct_cm = cm(q.from.east_m, q.from.north_m, q.to.east_m, q.to.north_m);
        // Straight across country between ends near each other, or when there is no road at all.
        const bool direct_ok = direct_cm <= std::llround(2.0 * q.link_max_m * 100.0) || s.edge < 0 || t.edge < 0;
        const Off d_off = direct_ok ? off_way(q.from, q.to, direct_cm) : Off();
        const std::int64_t direct_ds = d_off.time_ds;
        r.max_link_m = static_cast<double>(std::max(s.off_cm, t.off_cm)) / 100.0;

        const std::size_t n = nodes_.size();
        const int S = static_cast<int>(n);
        const int T = static_cast<int>(n) + 1;
        std::vector<std::int64_t> dist(n + 2, INT64_MAX);
        struct Via {
            int from = -1;
            int edge = -1;  // an edge of the graph; -2 off road; -3 along part of an edge (its index in `part`)
            int part = -1;
            bool swim = false;
        };
        std::vector<Via> via(n + 2);
        std::vector<std::string> depends;
        // A node a class cannot pass: its crossing down, for one that does not swim (or may not here).
        auto down = [&](int node) -> const OverlayEntry* {
            const int c = nodes_[static_cast<std::size_t>(node)].crossing;
            return c >= 0 ? overlay.Down(crossings_[static_cast<std::size_t>(c)].id, q.h) : nullptr;
        };
        auto swim_ds = [&](int node) {
            const NavCrossing& c = crossings_[static_cast<std::size_t>(nodes_[static_cast<std::size_t>(node)].crossing)];
            const std::int64_t width = std::max<std::int64_t>(100, std::llround(c.river_width_m * 100.0));
            return ds(width, cmps(m.water_mps)) + std::llround(m.swim_prep_s * 10.0);
        };
        auto passable = [&](int node, bool& swim) {
            swim = false;
            const OverlayEntry* e = down(node);
            if (e == nullptr) return true;
            if (std::find(depends.begin(), depends.end(), e->id) == depends.end()) depends.push_back(e->id);
            if (m.Swims() && q.swim) {
                swim = true;
                return true;
            }
            return false;
        };
        struct Part {
            int edge;
            std::int64_t cost;
        };
        std::vector<Part> parts;
        using Item = std::pair<std::int64_t, int>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
        auto relax = [&](int u, int v, std::int64_t c, Via w) {
            if (dist[static_cast<std::size_t>(u)] == INT64_MAX) return;
            const std::int64_t nd = dist[static_cast<std::size_t>(u)] + c;
            if (nd < dist[static_cast<std::size_t>(v)]) {
                dist[static_cast<std::size_t>(v)] = nd;
                w.from = u;
                via[static_cast<std::size_t>(v)] = w;
                open.push({nd, v});
            }
        };
        dist[static_cast<std::size_t>(S)] = 0;
        open.push({0, S});
        while (!open.empty()) {
            const Item top = open.top();
            open.pop();
            const int u = top.second;
            if (top.first > dist[static_cast<std::size_t>(u)]) continue;
            if (u == T) break;
            if (u == S) {
                if (direct_ok) relax(S, T, direct_ds, {-1, -2, -1, false});
                if (s.edge >= 0) {
                    const NavEdge& e = edges_[static_cast<std::size_t>(s.edge)];
                    const std::int64_t link = s_off.time_ds;
                    const std::int64_t v = road_v[static_cast<std::size_t>(e.road)];
                    for (int end : {e.a, e.b}) {
                        bool swim = false;
                        if (!passable(end, swim)) continue;
                        const std::int64_t along = end == e.a ? s.along_cm : e.length_cm - s.along_cm;
                        parts.push_back({s.edge, link + ds(along, v)});
                        relax(S, end, parts.back().cost + (swim ? swim_ds(end) : 0), {-1, -3, static_cast<int>(parts.size()) - 1, swim});
                    }
                    if (t.edge == s.edge) {
                        const std::int64_t along = s.along_cm > t.along_cm ? s.along_cm - t.along_cm : t.along_cm - s.along_cm;
                        parts.push_back({s.edge, link + ds(along, v) + t_off.time_ds});
                        relax(S, T, parts.back().cost, {-1, -3, static_cast<int>(parts.size()) - 1, false});
                    }
                }
                continue;
            }
            for (int ei : nodes_[static_cast<std::size_t>(u)].edges) {
                const NavEdge& e = edges_[static_cast<std::size_t>(ei)];
                const int v = e.a == u ? e.b : e.a;
                bool swim = false;
                if (!passable(v, swim)) continue;
                relax(u, v, ds(e.length_cm, road_v[static_cast<std::size_t>(e.road)]) + (swim ? swim_ds(v) : 0), {-1, ei, -1, swim});
            }
            if (t.edge >= 0) {
                const NavEdge& e = edges_[static_cast<std::size_t>(t.edge)];
                if (u == e.a || u == e.b) {
                    const std::int64_t along = u == e.a ? t.along_cm : e.length_cm - t.along_cm;
                    parts.push_back({t.edge, ds(along, road_v[static_cast<std::size_t>(e.road)]) + t_off.time_ds});
                    relax(u, T, parts.back().cost, {-1, -3, static_cast<int>(parts.size()) - 1, false});
                }
            }
        }
        r.depends = depends;
        if (dist[static_cast<std::size_t>(T)] == INT64_MAX) {
            r.why = s.edge < 0 && t.edge < 0 ? "no road within reach" : "every way is cut";
            return r;
        }
        r.found = true;
        r.time_s = static_cast<double>(dist[static_cast<std::size_t>(T)]) / 10.0;
        // Back from the end: the nodes and how each was reached.
        std::vector<int> chain;
        for (int v = T; v != S; v = via[static_cast<std::size_t>(v)].from) chain.push_back(v);
        std::reverse(chain.begin(), chain.end());
        auto point = [&](int v) {
            if (v == S) return q.from;
            if (v == T) return q.to;
            return nodes_[static_cast<std::size_t>(v)].at;
        };
        // Stretch by stretch: each adds the point it ends at; the legs join the stretches of one line.
        auto go = [&r](const std::string& kind, Road road, const std::string& line, double length_m, double time_s, const MapPoint& to, int node) {
            r.stretches.push_back({kind, road, line, length_m, time_s});
            r.path.push_back(to);
            r.nodes.push_back(node);
            if (length_m <= 0.0 && time_s <= 0.0) return;
            if (!r.legs.empty() && r.legs.back().kind == kind && r.legs.back().line_id == line) {
                r.legs.back().length_m += length_m;
                r.legs.back().time_s += time_s;
            } else {
                r.legs.push_back({kind, road, line, length_m, time_s});
            }
        };
        auto off_s = [&](std::int64_t length_cm) { return static_cast<double>(ds(length_cm, off_v)) / 10.0; };
        // A way off the road: one stretch straight, or one for each corner of its way across the grid.
        auto off_go = [&](const Off& o, const MapPoint& from, const MapPoint& to, double straight_m, double straight_s) {
            if (!o.grid) {
                go("off_road", Road::Track, "", straight_m, straight_s, to, -1);
                return;
            }
            MapPoint last = from;
            std::int64_t at = 0;
            for (std::size_t i = 0; i < o.points.size(); ++i) {
                go("off_road", Road::Track, "", std::hypot(o.points[i].east_m - last.east_m, o.points[i].north_m - last.north_m),
                   static_cast<double>(o.at_ds[i] - at) / 10.0, o.points[i], -1);
                last = o.points[i];
                at = o.at_ds[i];
            }
        };
        r.path.push_back(q.from);
        r.nodes.push_back(-1);
        int prev = S;
        for (int v : chain) {
            const Via& w = via[static_cast<std::size_t>(v)];
            const MapPoint a = point(prev);
            const MapPoint b = point(v);
            const int node = v == S || v == T ? -1 : v;
            if (w.edge == -2) {
                off_go(d_off, a, b, std::hypot(b.east_m - a.east_m, b.north_m - a.north_m), static_cast<double>(direct_ds) / 10.0);
            } else if (w.edge == -3) {
                // Off the road to an edge, along it, and (at the end) off it again.
                const NavEdge& e = edges_[static_cast<std::size_t>(parts[static_cast<std::size_t>(w.part)].edge)];
                const std::int64_t v_road = road_v[static_cast<std::size_t>(e.road)];
                auto road_s = [&](double along_m) { return static_cast<double>(ds(std::llround(along_m * 100.0), v_road)) / 10.0; };
                if (prev == S) {
                    off_go(s_off, q.from, s.on, static_cast<double>(s.off_cm) / 100.0, off_s(s.off_cm));
                    if (v == T && t.edge == s.edge) {
                        const double along_m = std::hypot(t.on.east_m - s.on.east_m, t.on.north_m - s.on.north_m);
                        go("road", e.road, e.line_id, along_m, road_s(along_m), t.on, -1);
                        off_go(t_off, t.on, b, static_cast<double>(t.off_cm) / 100.0, off_s(t.off_cm));
                    } else {
                        const double along_m = std::hypot(b.east_m - s.on.east_m, b.north_m - s.on.north_m);
                        go("road", e.road, e.line_id, along_m, road_s(along_m), b, node);
                    }
                } else {
                    const double along_m = std::hypot(t.on.east_m - a.east_m, t.on.north_m - a.north_m);
                    go("road", e.road, e.line_id, along_m, road_s(along_m), t.on, -1);
                    off_go(t_off, t.on, b, static_cast<double>(t.off_cm) / 100.0, off_s(t.off_cm));
                }
            } else {
                const NavEdge& e = edges_[static_cast<std::size_t>(w.edge)];
                go("road", e.road, e.line_id, static_cast<double>(e.length_cm) / 100.0,
                   static_cast<double>(ds(e.length_cm, road_v[static_cast<std::size_t>(e.road)])) / 10.0, b, node);
            }
            if (node >= 0 && nodes_[static_cast<std::size_t>(node)].crossing >= 0) {
                const NavCrossing& x = crossings_[static_cast<std::size_t>(nodes_[static_cast<std::size_t>(node)].crossing)];
                r.crossings.push_back(x.id);
                if (w.swim) go("swim", Road::Track, x.river_id, x.river_width_m, static_cast<double>(swim_ds(v)) / 10.0, b, node);
            }
            prev = v;
        }
        for (const RouteLeg& l : r.legs) r.length_m += l.length_m;
        return r;
    }

private:
    static constexpr double kJoinM = 50.0;
    static constexpr double kShareBridgeM = 30.0;

    struct Snap {
        int edge = -1;
        std::int64_t along_cm = 0;  // from the edge's a end
        std::int64_t off_cm = 0;    // from the point to the edge
        MapPoint on;
    };

    // The nearest point of the road net (ties: the first edge).
    Snap SnapTo(const MapPoint& p) const {
        Snap best;
        double best_d = 1e300;
        for (std::size_t i = 0; i < edges_.size(); ++i) {
            const NavEdge& e = edges_[i];
            const MapPoint& a = nodes_[static_cast<std::size_t>(e.a)].at;
            const MapPoint& b = nodes_[static_cast<std::size_t>(e.b)].at;
            double f = 0.0;  // the fraction of the way from a to b
            const double d = mapgeo::DistToSegment(p.east_m, p.north_m, a.east_m, a.north_m, b.east_m, b.north_m, &f);
            if (d < best_d - 1e-9) {
                best_d = d;
                best.edge = static_cast<int>(i);
                best.on = {a.east_m + (b.east_m - a.east_m) * f, a.north_m + (b.north_m - a.north_m) * f};
                best.along_cm = std::llround(f * static_cast<double>(e.length_cm));
                best.off_cm = std::llround(d * 100.0);
            }
        }
        return best;
    }

    int CrossingAt(const MapLineDef& line, const MapLineDef& river, const MapPoint& p) {
        for (std::size_t i = 0; i < crossings_.size(); ++i) {
            if (std::hypot(crossings_[i].at.east_m - p.east_m, crossings_[i].at.north_m - p.north_m) <= kShareBridgeM) return static_cast<int>(i);
        }
        NavCrossing c;
        const std::string base = line.id + "@" + river.id;
        int same = 0;
        for (std::size_t i = 0; i < crossings_.size(); ++i) same += first_line_[i] == line.id && crossings_[i].river_id == river.id ? 1 : 0;
        c.id = same == 0 ? base : base + "#" + std::to_string(same + 1);
        c.river_id = river.id;
        c.river_width_m = river.width_m;
        c.at = p;
        crossings_.push_back(c);
        first_line_.push_back(line.id);
        return static_cast<int>(crossings_.size() - 1);
    }

    int NodeAt(const MapPoint& p, int crossing) {
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            NavNode& nd = nodes_[i];
            if (std::hypot(nd.at.east_m - p.east_m, nd.at.north_m - p.north_m) > kJoinM) continue;
            if (crossing >= 0 && nd.crossing < 0) nd.crossing = crossing;
            return static_cast<int>(i);
        }
        NavNode nd;
        nd.at = p;
        nd.crossing = crossing;
        nodes_.push_back(nd);
        return static_cast<int>(nodes_.size() - 1);
    }

    // A point on a road between two of its nodes (within kJoinM of the edge, none of its nodes that near) becomes a node
    // there: the edge split in two, its line and class kept.
    void JoinAt(const MapPoint& p) {
        for (const NavNode& nd : nodes_) {
            if (std::hypot(nd.at.east_m - p.east_m, nd.at.north_m - p.north_m) <= kJoinM) return;
        }
        const Snap s = SnapTo(p);
        if (s.edge < 0 || static_cast<double>(s.off_cm) / 100.0 > kJoinM) return;
        const int ei = s.edge;
        const int b = edges_[static_cast<std::size_t>(ei)].b;
        NavNode mid;
        mid.at = s.on;
        nodes_.push_back(mid);
        const int m = static_cast<int>(nodes_.size() - 1);
        NavEdge& e = edges_[static_cast<std::size_t>(ei)];
        NavEdge rest = e;
        e.b = m;
        e.length_cm = std::llround(std::hypot(s.on.east_m - nodes_[static_cast<std::size_t>(e.a)].at.east_m,
                                              s.on.north_m - nodes_[static_cast<std::size_t>(e.a)].at.north_m) * 100.0);
        rest.a = m;
        rest.length_cm = std::llround(std::hypot(nodes_[static_cast<std::size_t>(b)].at.east_m - s.on.east_m,
                                                 nodes_[static_cast<std::size_t>(b)].at.north_m - s.on.north_m) * 100.0);
        edges_.push_back(rest);
        const int ri = static_cast<int>(edges_.size() - 1);
        nodes_[static_cast<std::size_t>(m)].edges = {ei, ri};
        for (int& x : nodes_[static_cast<std::size_t>(b)].edges) {
            if (x == ei) x = ri;
        }
    }

    void Link(int a, int b, const MapLineDef& line) {
        if (a == b) return;
        NavEdge e;
        e.a = a;
        e.b = b;
        const MapPoint& pa = nodes_[static_cast<std::size_t>(a)].at;
        const MapPoint& pb = nodes_[static_cast<std::size_t>(b)].at;
        e.length_cm = std::llround(std::hypot(pb.east_m - pa.east_m, pb.north_m - pa.north_m) * 100.0);
        e.road = RoadOf(line.line_class);
        e.line_id = line.id;
        edges_.push_back(e);
        const int ei = static_cast<int>(edges_.size() - 1);
        nodes_[static_cast<std::size_t>(a)].edges.push_back(ei);
        nodes_[static_cast<std::size_t>(b)].edges.push_back(ei);
    }

    // A line as a chain of nodes, its river crossings between its vertices (as CountryMap::BuildNet does).
    void AddLine(const MapLineDef& line) {
        if (line.points.size() < 2) return;
        constexpr double kEnd = 1e-6;
        std::vector<int> vertex_crossing(line.points.size(), -1);
        std::vector<std::vector<std::pair<double, int>>> inner(line.points.size());
        for (std::size_t i = 1; i < line.points.size(); ++i) {
            const MapPoint& a = line.points[i - 1];
            const MapPoint& b = line.points[i];
            for (const MapLineDef& river : rivers_) {
                for (std::size_t j = 1; j < river.points.size(); ++j) {
                    double t = 0.0, u = 0.0;
                    if (!mapgeo::Intersect(a, b, river.points[j - 1], river.points[j], t, u)) continue;
                    const MapPoint p{a.east_m + (b.east_m - a.east_m) * t, a.north_m + (b.north_m - a.north_m) * t};
                    const int c = CrossingAt(line, river, p);
                    if (t <= kEnd) vertex_crossing[i - 1] = c;
                    else if (t >= 1.0 - kEnd) vertex_crossing[i] = c;
                    else inner[i].push_back({t, c});
                }
            }
        }
        int prev = NodeAt(line.points[0], vertex_crossing[0]);
        for (std::size_t i = 1; i < line.points.size(); ++i) {
            const MapPoint& a = line.points[i - 1];
            const MapPoint& b = line.points[i];
            std::sort(inner[i].begin(), inner[i].end());
            for (const std::pair<double, int>& hit : inner[i]) {
                const MapPoint p{a.east_m + (b.east_m - a.east_m) * hit.first, a.north_m + (b.north_m - a.north_m) * hit.first};
                const int nd = NodeAt(p, hit.second);
                Link(prev, nd, line);
                prev = nd;
            }
            const int nd = NodeAt(b, vertex_crossing[i]);
            Link(prev, nd, line);
            prev = nd;
        }
    }

    std::vector<MapLineDef> rivers_;
    std::shared_ptr<const GroundGrid> ground_;
    std::vector<NavNode> nodes_;
    std::vector<NavEdge> edges_;
    std::vector<NavCrossing> crossings_;
    std::vector<std::string> first_line_;  // the line that made each crossing (its id)
};

// ---------------------------------------------------------------------------------------------- the cache

// Routes asked between anchors by their ids (never node numbers), kept by the two anchors, the mobility class and the
// overlay's version (track F, F3.6; docs/fronts/03_GROUND_WAR.md section 7): an order asks once, a second order between
// the same anchors on the same overlay takes the route found, and a bridge cut (a new overlay version) asks again. The
// points are the anchors' own; the hour is not part of the key, so the overlay it is used with must hold only entries
// that are down for good or not at all (the ground war rebuilds its own when a bridge's state changes). The oldest entry
// leaves when it is full.
class RouteCache {
public:
    struct Stats {
        std::uint64_t asks = 0;   // routes asked
        std::uint64_t finds = 0;  // of those, searched on the graph
        std::uint64_t hits = 0;   // and taken from the cache
    };

    explicit RouteCache(std::size_t capacity = 128) : capacity_(capacity) { entries_.reserve(capacity); }

    const Route& Get(const RoadGraph& g, const std::string& from_id, const MapPoint& from, const std::string& to_id, const MapPoint& to,
                     const Mobility& m, const Overlay& overlay) {
        ++stats_.asks;
        for (const Entry& e : entries_) {
            if (e.version == overlay.version && e.from == from_id && e.to == to_id && e.mobility == m.id) {
                ++stats_.hits;
                return e.route;
            }
        }
        ++stats_.finds;
        RouteRequest q;
        q.from = from;
        q.to = to;
        q.mobility = &m;
        if (capacity_ > 0 && entries_.size() >= capacity_) entries_.erase(entries_.begin());
        Entry e;
        e.from = from_id;
        e.to = to_id;
        e.mobility = m.id;
        e.version = overlay.version;
        e.route = g.Find(q, overlay);
        entries_.push_back(std::move(e));
        return entries_.back().route;
    }

    const Stats& GetStats() const { return stats_; }
    std::size_t Size() const { return entries_.size(); }

private:
    struct Entry {
        std::string from;
        std::string to;
        std::string mobility;
        std::uint64_t version = 0;
        Route route;
    };
    std::size_t capacity_;
    std::vector<Entry> entries_;
    Stats stats_;
};

}  // namespace nav
}  // namespace simcore
