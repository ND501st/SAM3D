// simcore/country_map.hpp — the whole country as a strategic map (data/maps/<id>.json with "kind": "country"): its
// ground at about 1 km, its borders and regions, its rivers, and its roads, railways and oil pipelines as networks that
// can be routed over, with every river crossing a named bridge (or pipeline crossing) that can be cut.
//
// It is the map the war is simulated on without a player (roadmap: the nation on the map, N1): supply runs, marches,
// the rebuild's logistics and the enemy's interdiction all ask it which way goes and what a cut bridge costs. The
// detailed districts the player fights in (the Kesmark range, simcore/terrain_map.hpp) lie inside it and share its
// ground: both use NaturalGround, and the country's file carries the district's relief, hills and rivers unchanged
// (tests/test_country_map.cpp proves the two agree).
//
// Nothing here draws houses or wood blocks; that is the district maps' work.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "simcore/terrain_map.hpp"

namespace simcore {

// ---------------------------------------------------------------------------------------------- the country file

struct CountrySettlementDef {
    std::string id;
    std::string name;
    std::string kind;  // capital, city, town, village
    MapPoint centre;
    double radius_m = 1000.0;
    double population = 0.0;  // game value
};

inline bool IsCountrySettlementKind(const std::string& k) { return k == "capital" || k == "city" || k == "town" || k == "village"; }

// A named point the map itself needs: the ends of the pipelines, where the roads and railways cross the borders, the
// refinery and the dam the oil and the Rava run through. The theatre's objects stand at them (N2).
struct CountryLandmarkDef {
    std::string id;
    std::string name;
    std::string kind;
    MapPoint at;
};

inline const std::vector<std::string>& CountryLandmarkKinds() {
    static const std::vector<std::string> kinds = {"refinery", "dam", "pipeline_station", "border_crossing", "port"};
    return kinds;
}

// A stretch of the country's border and who is across it.
struct CountryBorderDef {
    std::string id;
    std::string name;
    std::string neighbour;  // a faction id of data/air_war/ (nato, warsaw_pact, neutral) or "none"
    std::vector<MapPoint> points;
};

struct CountryDef {
    std::string map_id;
    std::string name;
    std::string description;
    // The ground and the lines, read the way a district map reads them (simcore/terrain_map.hpp): origin, seed, the
    // grid's extent (west_m .. north_m), terrain (base, relief, hills, valley), rivers, roads, railways, forests.
    MapDef ground;
    double grid_spacing_m = 1000.0;
    std::vector<MapLineDef> pipelines;  // class "oil"
    std::vector<CountrySettlementDef> settlements;
    std::vector<MapPolygonDef> regions;
    std::vector<MapPoint> outline;  // the country's border, closed
    std::vector<CountryBorderDef> borders;
    std::vector<CountryLandmarkDef> landmarks;
    std::string district;  // a district map lying inside it that must share its ground ("maps/kesmark_range.json")
};

// ---------------------------------------------------------------------------------------------- the networks

enum class CountryNet { Road, Rail, Oil, Count };

inline const char* CountryNetName(CountryNet n) {
    switch (n) {
        case CountryNet::Road: return "road";
        case CountryNet::Rail: return "rail";
        case CountryNet::Oil: return "oil";
        case CountryNet::Count: break;
    }
    return "?";
}

// Where a road, a railway or a pipeline crosses a river: a bridge (or the pipeline's crossing), a thing that can be
// struck and rebuilt. Lines of one network meeting on the river share it.
struct CountryCrossing {
    std::string id;       // "<line>@<river>", a second crossing of the same pair "<line>@<river>#2"
    std::string line_id;  // the first line that made it
    std::string river_id;
    CountryNet net = CountryNet::Road;
    MapPoint at;
    double heading_rad = 0.0;        // the line's, true
    std::vector<std::string> lines;  // every line of the network that uses it
};

struct CountryNode {
    MapPoint at;
    int crossing = -1;  // an index into CountryMap::Crossings(), or -1
    std::vector<int> edges;
};

struct CountryEdge {
    int a = 0;
    int b = 0;
    double length_m = 0.0;
    std::string line_id;
};

struct CountryRoute {
    bool found = false;
    double length_m = 0.0;
    std::vector<MapPoint> path;
    std::vector<std::string> crossings;  // the bridges used, in order
    std::vector<std::string> lines;      // the lines followed, in order
};

class CountryNetwork {
public:
    CountryNet Kind() const { return kind_; }
    const std::vector<CountryNode>& Nodes() const { return nodes_; }
    const std::vector<CountryEdge>& Edges() const { return edges_; }

    // The node nearest `p`, if one lies within `max_m`; -1 otherwise.
    int NearestNode(const MapPoint& p, double max_m) const {
        int best = -1;
        double best_d = max_m;
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            const double d = std::hypot(nodes_[i].at.east_m - p.east_m, nodes_[i].at.north_m - p.north_m);
            if (d <= best_d) {
                best_d = d;
                best = static_cast<int>(i);
            }
        }
        return best;
    }

    // The shortest way from the node nearest `from` to the node nearest `to` (each within `snap_m`), never over a
    // crossing in `cut` (a bridge down). Not found when either end is off the network or every way is cut.
    CountryRoute Route(const MapPoint& from, const MapPoint& to, const std::set<std::string>& cut = {}, double snap_m = 3000.0) const {
        CountryRoute r;
        const int a = NearestNode(from, snap_m);
        const int b = NearestNode(to, snap_m);
        if (a < 0 || b < 0 || Blocked(a, cut) || Blocked(b, cut)) return r;
        const double inf = std::numeric_limits<double>::infinity();
        std::vector<double> dist(nodes_.size(), inf);
        std::vector<int> via(nodes_.size(), -1);  // the edge that reached the node
        using Item = std::pair<double, int>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
        dist[static_cast<std::size_t>(a)] = 0.0;
        open.push({0.0, a});
        while (!open.empty()) {
            const Item top = open.top();
            open.pop();
            const int u = top.second;
            if (top.first > dist[static_cast<std::size_t>(u)]) continue;
            if (u == b) break;
            for (int ei : nodes_[static_cast<std::size_t>(u)].edges) {
                const CountryEdge& e = edges_[static_cast<std::size_t>(ei)];
                const int v = e.a == u ? e.b : e.a;
                if (Blocked(v, cut)) continue;
                const double nd = top.first + e.length_m;
                if (nd < dist[static_cast<std::size_t>(v)]) {
                    dist[static_cast<std::size_t>(v)] = nd;
                    via[static_cast<std::size_t>(v)] = ei;
                    open.push({nd, v});
                }
            }
        }
        if (dist[static_cast<std::size_t>(b)] == inf) return r;
        r.found = true;
        r.length_m = dist[static_cast<std::size_t>(b)];
        std::vector<int> nodes_on;
        std::vector<int> edges_on;
        for (int n = b; n != a;) {
            nodes_on.push_back(n);
            const int ei = via[static_cast<std::size_t>(n)];
            edges_on.push_back(ei);
            const CountryEdge& e = edges_[static_cast<std::size_t>(ei)];
            n = e.a == n ? e.b : e.a;
        }
        nodes_on.push_back(a);
        std::reverse(nodes_on.begin(), nodes_on.end());
        std::reverse(edges_on.begin(), edges_on.end());
        for (int n : nodes_on) {
            r.path.push_back(nodes_[static_cast<std::size_t>(n)].at);
            const int c = nodes_[static_cast<std::size_t>(n)].crossing;
            if (c >= 0) r.crossings.push_back((*crossings_)[static_cast<std::size_t>(c)].id);
        }
        for (int ei : edges_on) {
            const std::string& l = edges_[static_cast<std::size_t>(ei)].line_id;
            if (r.lines.empty() || r.lines.back() != l) r.lines.push_back(l);
        }
        return r;
    }

private:
    friend class CountryMap;

    bool Blocked(int node, const std::set<std::string>& cut) const {
        const int c = nodes_[static_cast<std::size_t>(node)].crossing;
        return c >= 0 && cut.count((*crossings_)[static_cast<std::size_t>(c)].id) > 0;
    }

    CountryNet kind_ = CountryNet::Road;
    std::vector<CountryNode> nodes_;
    std::vector<CountryEdge> edges_;
    const std::vector<CountryCrossing>* crossings_ = nullptr;
};

// ---------------------------------------------------------------------------------------------- the built country

class CountryMap {
public:
    // Bakes the ground grid and builds the networks. A fraction of a second for a 300 km country at 1 km.
    static std::shared_ptr<const CountryMap> Build(CountryDef def) {
        std::shared_ptr<CountryMap> m(new CountryMap(std::move(def)));
        m->BakeGround();
        m->BuildNet(CountryNet::Road, m->def_.ground.roads);
        m->BuildNet(CountryNet::Rail, m->def_.ground.railways);
        m->BuildNet(CountryNet::Oil, m->def_.pipelines);
        return m;
    }

    const CountryDef& Def() const { return def_; }
    const MapGrid& Grid() const { return grid_; }

    // The ground as the terrain shapes it (the same function a district map uses).
    double Natural(double e, double n) const { return NaturalGround(def_.ground, e, n); }

    // The ground's height above sea level: the baked grid, the analytic ground beyond it.
    double GroundHeight(double e, double n) const { return grid_.Covers(e, n) ? grid_.Sample(e, n) : Natural(e, n); }

    bool Inside(double e, double n) const { return def_.outline.size() >= 3 && mapgeo::InPolygon(def_.outline, e, n); }

    // How far (e, n) is from the nearest stretch of border, and which one.
    double DistanceToBorder(double e, double n, const CountryBorderDef** border = nullptr) const {
        double best = std::numeric_limits<double>::infinity();
        for (const CountryBorderDef& b : def_.borders) {
            const double d = mapgeo::DistToLine(b.points, e, n);
            if (d < best) {
                best = d;
                if (border != nullptr) *border = &b;
            }
        }
        return best;
    }

    // The first region containing (e, n), or null.
    const MapPolygonDef* RegionAt(double e, double n) const {
        for (const MapPolygonDef& r : def_.regions) {
            if (mapgeo::InPolygon(r.points, e, n)) return &r;
        }
        return nullptr;
    }

    const CountrySettlementDef* Settlement(const std::string& id) const {
        for (const CountrySettlementDef& s : def_.settlements) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }

    const CountrySettlementDef* NearestSettlement(double e, double n, double* dist = nullptr) const {
        const CountrySettlementDef* best = nullptr;
        double best_d = std::numeric_limits<double>::infinity();
        for (const CountrySettlementDef& s : def_.settlements) {
            const double d = std::hypot(s.centre.east_m - e, s.centre.north_m - n);
            if (d < best_d) {
                best_d = d;
                best = &s;
            }
        }
        if (dist != nullptr) *dist = best_d;
        return best;
    }

    const CountrySettlementDef* Capital() const {
        for (const CountrySettlementDef& s : def_.settlements) {
            if (s.kind == "capital") return &s;
        }
        return nullptr;
    }

    const CountryLandmarkDef* Landmark(const std::string& id) const {
        for (const CountryLandmarkDef& l : def_.landmarks) {
            if (l.id == id) return &l;
        }
        return nullptr;
    }

    double DistanceToRiver(double e, double n, std::string* river = nullptr) const {
        double best = std::numeric_limits<double>::infinity();
        for (const MapLineDef& r : def_.ground.rivers) {
            const double d = mapgeo::DistToLine(r.points, e, n);
            if (d < best) {
                best = d;
                if (river != nullptr) *river = r.id;
            }
        }
        return best;
    }

    bool InForest(double e, double n) const {
        for (const MapPolygonDef& f : def_.ground.forests) {
            if (mapgeo::InPolygon(f.points, e, n)) return true;
        }
        return false;
    }

    const std::vector<MapLineDef>& Lines(CountryNet net) const {
        return net == CountryNet::Road ? def_.ground.roads : (net == CountryNet::Rail ? def_.ground.railways : def_.pipelines);
    }

    const MapLineDef* Line(CountryNet net, const std::string& id) const {
        for (const MapLineDef& l : Lines(net)) {
            if (l.id == id) return &l;
        }
        return nullptr;
    }

    const CountryNetwork& Network(CountryNet net) const { return nets_[static_cast<std::size_t>(net)]; }
    const std::vector<CountryCrossing>& Crossings() const { return crossings_; }

    const CountryCrossing* Crossing(const std::string& id) const {
        for (const CountryCrossing& c : crossings_) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }

    const CountryCrossing* NearestCrossing(double e, double n, CountryNet net, double* dist = nullptr) const {
        const CountryCrossing* best = nullptr;
        double best_d = std::numeric_limits<double>::infinity();
        for (const CountryCrossing& c : crossings_) {
            if (c.net != net) continue;
            const double d = std::hypot(c.at.east_m - e, c.at.north_m - n);
            if (d < best_d) {
                best_d = d;
                best = &c;
            }
        }
        if (dist != nullptr) *dist = best_d;
        return best;
    }

private:
    static constexpr double kJoinM = 50.0;        // line vertices this close are one junction
    static constexpr double kShareBridgeM = 30.0;  // lines of one network meeting on the river share its bridge

    explicit CountryMap(CountryDef def) : def_(std::move(def)) {
        for (std::size_t i = 0; i < nets_.size(); ++i) {
            nets_[i].kind_ = static_cast<CountryNet>(i);
            nets_[i].crossings_ = &crossings_;
        }
    }

    void BakeGround() {
        const MapDef& g = def_.ground;
        grid_.spacing_m = def_.grid_spacing_m;
        grid_.west_m = g.west_m;
        grid_.south_m = g.south_m;
        grid_.cols = static_cast<int>(std::lround((g.east_m - g.west_m) / grid_.spacing_m)) + 1;
        grid_.rows = static_cast<int>(std::lround((g.north_m - g.south_m) / grid_.spacing_m)) + 1;
        grid_.values.assign(static_cast<std::size_t>(grid_.cols) * static_cast<std::size_t>(grid_.rows), 0.0);
        for (int r = 0; r < grid_.rows; ++r) {
            for (int c = 0; c < grid_.cols; ++c) grid_.At(c, r) = Natural(grid_.EastOf(c), grid_.NorthOf(r));
        }
    }

    // The crossing of this network within kShareBridgeM of `p`, or a new one.
    int CrossingAt(CountryNet net, const MapLineDef& line, const MapLineDef& river, const MapPoint& p, const MapPoint& a, const MapPoint& b) {
        for (std::size_t i = 0; i < crossings_.size(); ++i) {
            CountryCrossing& c = crossings_[i];
            if (c.net != net || std::hypot(c.at.east_m - p.east_m, c.at.north_m - p.north_m) > kShareBridgeM) continue;
            if (std::find(c.lines.begin(), c.lines.end(), line.id) == c.lines.end()) c.lines.push_back(line.id);
            return static_cast<int>(i);
        }
        CountryCrossing c;
        const std::string base = line.id + "@" + river.id;
        int same = 0;
        for (const CountryCrossing& o : crossings_) same += o.line_id == line.id && o.river_id == river.id ? 1 : 0;
        c.id = same == 0 ? base : base + "#" + std::to_string(same + 1);
        c.line_id = line.id;
        c.river_id = river.id;
        c.net = net;
        c.at = p;
        c.heading_rad = std::atan2(b.east_m - a.east_m, b.north_m - a.north_m);
        c.lines.push_back(line.id);
        crossings_.push_back(c);
        return static_cast<int>(crossings_.size() - 1);
    }

    int NodeAt(CountryNetwork& net, const MapPoint& p, int crossing) {
        const long long cx = static_cast<long long>(std::floor(p.east_m / 100.0));
        const long long cy = static_cast<long long>(std::floor(p.north_m / 100.0));
        std::unordered_map<long long, std::vector<int>>& index = node_index_[static_cast<std::size_t>(net.kind_)];
        for (long long y = cy - 1; y <= cy + 1; ++y) {
            for (long long x = cx - 1; x <= cx + 1; ++x) {
                const auto it = index.find(x * 1000003LL + y);
                if (it == index.end()) continue;
                for (int i : it->second) {
                    CountryNode& n = net.nodes_[static_cast<std::size_t>(i)];
                    if (std::hypot(n.at.east_m - p.east_m, n.at.north_m - p.north_m) > kJoinM) continue;
                    if (crossing >= 0 && n.crossing < 0) n.crossing = crossing;
                    return i;
                }
            }
        }
        CountryNode n;
        n.at = p;
        n.crossing = crossing;
        net.nodes_.push_back(n);
        const int i = static_cast<int>(net.nodes_.size() - 1);
        index[cx * 1000003LL + cy].push_back(i);
        return i;
    }

    void Link(CountryNetwork& net, int a, int b, const std::string& line_id) {
        if (a == b) return;
        const CountryNode& na = net.nodes_[static_cast<std::size_t>(a)];
        const CountryNode& nb = net.nodes_[static_cast<std::size_t>(b)];
        CountryEdge e;
        e.a = a;
        e.b = b;
        e.length_m = std::hypot(nb.at.east_m - na.at.east_m, nb.at.north_m - na.at.north_m);
        e.line_id = line_id;
        net.edges_.push_back(e);
        const int ei = static_cast<int>(net.edges_.size() - 1);
        net.nodes_[static_cast<std::size_t>(a)].edges.push_back(ei);
        net.nodes_[static_cast<std::size_t>(b)].edges.push_back(ei);
    }

    // Every line as a chain of nodes: its vertices, and its river crossings between them. Vertices shared by lines
    // (within kJoinM) are one junction, so the lines join into a network.
    void BuildNet(CountryNet kind, const std::vector<MapLineDef>& lines) {
        CountryNetwork& net = nets_[static_cast<std::size_t>(kind)];
        constexpr double kEnd = 1e-6;
        for (const MapLineDef& line : lines) {
            std::vector<int> vertex_crossing(line.points.size(), -1);
            std::vector<std::vector<std::pair<double, int>>> inner(line.points.size());  // per segment: (t, crossing)
            for (std::size_t i = 1; i < line.points.size(); ++i) {
                const MapPoint& a = line.points[i - 1];
                const MapPoint& b = line.points[i];
                for (const MapLineDef& river : def_.ground.rivers) {
                    for (std::size_t j = 1; j < river.points.size(); ++j) {
                        double t = 0.0, u = 0.0;
                        if (!mapgeo::Intersect(a, b, river.points[j - 1], river.points[j], t, u)) continue;
                        const MapPoint p{a.east_m + (b.east_m - a.east_m) * t, a.north_m + (b.north_m - a.north_m) * t};
                        const int c = CrossingAt(kind, line, river, p, a, b);
                        if (t <= kEnd) vertex_crossing[i - 1] = c;
                        else if (t >= 1.0 - kEnd) vertex_crossing[i] = c;
                        else inner[i].push_back({t, c});
                    }
                }
            }
            int prev = NodeAt(net, line.points[0], vertex_crossing[0]);
            for (std::size_t i = 1; i < line.points.size(); ++i) {
                const MapPoint& a = line.points[i - 1];
                const MapPoint& b = line.points[i];
                std::sort(inner[i].begin(), inner[i].end());
                for (const std::pair<double, int>& hit : inner[i]) {
                    const MapPoint p{a.east_m + (b.east_m - a.east_m) * hit.first, a.north_m + (b.north_m - a.north_m) * hit.first};
                    const int n = NodeAt(net, p, hit.second);
                    Link(net, prev, n, line.id);
                    prev = n;
                }
                const int n = NodeAt(net, b, vertex_crossing[i]);
                Link(net, prev, n, line.id);
                prev = n;
            }
        }
    }

    CountryDef def_;
    MapGrid grid_;
    std::vector<CountryCrossing> crossings_;
    std::array<CountryNetwork, static_cast<std::size_t>(CountryNet::Count)> nets_;
    std::array<std::unordered_map<long long, std::vector<int>>, static_cast<std::size_t>(CountryNet::Count)> node_index_;
};

// ---------------------------------------------------------------------------------------------- loading

namespace detail {

class CountryLoader : public MapLoader {
public:
    bool Load(const json::Value& root, CountryDef& out) {
        std::string kind;
        if (!Keys(root, "", {"map_id", "kind", "name", "description", "status", "source", "confidence", "notes", "origin", "seed",
                             "extent_km", "grid_km", "district", "terrain", "outline", "borders", "regions", "rivers", "roads",
                             "railways", "pipelines", "settlements", "forests", "landmarks"}) ||
            !RequireString(root, "", "map_id", out.map_id) || !RequireString(root, "", "kind", kind) ||
            !RequireString(root, "", "name", out.name) || !OptionalString(root, "", "description", out.description) ||
            !OptionalString(root, "", "district", out.district) || !Provenance(root, "")) {
            return false;
        }
        if (kind != "country") return Fail("kind", "must be \"country\" (a district map is read by simcore/terrain_map.hpp)");
        MapDef& g = out.ground;
        g.map_id = out.map_id;
        g.name = out.name;
        const json::Value* o = root.Find("origin");
        if (o == nullptr) return Fail("origin", "required");
        double lat = 0.0, lon = 0.0, alt = 0.0;
        if (!Keys(*o, "origin", {"lat_deg", "lon_deg", "alt_m"}) || !Number(*o, "origin", "lat_deg", lat) ||
            !Number(*o, "origin", "lon_deg", lon) || !Number(*o, "origin", "alt_m", alt)) {
            return false;
        }
        g.origin = GeoPoint::FromDegrees(lat, lon, alt);
        double seed = 1.0;
        if (!OptionalNumber(root, "", "seed", seed)) return false;
        g.seed = static_cast<std::uint64_t>(std::max(0.0, seed));
        const json::Value* ext = root.Find("extent_km");
        if (ext == nullptr) return Fail("extent_km", "required");
        double w = 0.0, e = 0.0, s = 0.0, n = 0.0;
        if (!Keys(*ext, "extent_km", {"west", "east", "south", "north"}) || !Number(*ext, "extent_km", "west", w) ||
            !Number(*ext, "extent_km", "east", e) || !Number(*ext, "extent_km", "south", s) || !Number(*ext, "extent_km", "north", n)) {
            return false;
        }
        if (e <= w || n <= s) return Fail("extent_km", "east must be beyond west and north beyond south");
        g.west_m = w * 1000.0;
        g.east_m = e * 1000.0;
        g.south_m = s * 1000.0;
        g.north_m = n * 1000.0;
        g.far_m = std::max({std::fabs(g.west_m), g.east_m, std::fabs(g.south_m), g.north_m});
        double grid_km = 1.0;
        if (!OptionalNumber(root, "", "grid_km", grid_km)) return false;
        if (grid_km < 0.1 || grid_km > 10.0) return Fail("grid_km", "between 0.1 and 10");
        out.grid_spacing_m = grid_km * 1000.0;
        if (!LoadTerrain(root, g) || !LoadLines(root, "rivers", "river", g.rivers) || !LoadLines(root, "roads", "", g.roads) ||
            !LoadLines(root, "railways", "rail", g.railways) || !LoadLines(root, "pipelines", "oil", out.pipelines) ||
            !LoadPolygons(root, "forests", g.forests) || !LoadPolygons(root, "regions", out.regions)) {
            return false;
        }
        return LoadOutline(root, out) && LoadBorders(root, out) && LoadSettlementsOf(root, out) && LoadLandmarks(root, out);
    }

private:
    bool LoadOutline(const json::Value& root, CountryDef& out) {
        const json::Value* o = root.Find("outline");
        if (o == nullptr) return Fail("outline", "required");
        return Keys(*o, "outline", {"points", "notes"}) && Points(*o, "outline", 3, out.outline);
    }

    bool LoadBorders(const json::Value& root, CountryDef& out) {
        const json::Value* list = OptionalArray(root, "", "borders");
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index("borders", i);
            const json::Value& v = list->items[i];
            CountryBorderDef b;
            if (!Keys(v, p, {"id", "name", "neighbour", "points", "notes"}) || !RequireString(v, p, "id", b.id) ||
                !OptionalString(v, p, "name", b.name) || !RequireString(v, p, "neighbour", b.neighbour) || !Points(v, p, 2, b.points)) {
                return false;
            }
            for (const CountryBorderDef& o : out.borders) {
                if (o.id == b.id) return Fail(Join(p, "id"), "'" + b.id + "' is listed twice");
            }
            out.borders.push_back(std::move(b));
        }
        return true;
    }

    bool LoadSettlementsOf(const json::Value& root, CountryDef& out) {
        const json::Value* list = OptionalArray(root, "", "settlements");
        if (list == nullptr) return Message().empty();
        int capitals = 0;
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index("settlements", i);
            const json::Value& v = list->items[i];
            CountrySettlementDef s;
            double r = 1.0;
            if (!Keys(v, p, {"id", "name", "kind", "east_km", "north_km", "radius_km", "population", "notes"}) ||
                !RequireString(v, p, "id", s.id) || !RequireString(v, p, "name", s.name) || !RequireString(v, p, "kind", s.kind) ||
                !Centre(v, p, s.centre) || !Number(v, p, "radius_km", r) || !Number(v, p, "population", s.population)) {
                return false;
            }
            if (!IsCountrySettlementKind(s.kind)) return Fail(Join(p, "kind"), "must be capital, city, town or village");
            if (r <= 0.0 || s.population < 0.0) return Fail(p, "needs a radius > 0 and a population >= 0");
            s.radius_m = r * 1000.0;
            capitals += s.kind == "capital" ? 1 : 0;
            for (const CountrySettlementDef& o : out.settlements) {
                if (o.id == s.id) return Fail(Join(p, "id"), "'" + s.id + "' is listed twice");
            }
            out.settlements.push_back(std::move(s));
        }
        if (capitals != 1) return Fail("settlements", "needs exactly one capital");
        return true;
    }

    bool LoadLandmarks(const json::Value& root, CountryDef& out) {
        const json::Value* list = OptionalArray(root, "", "landmarks");
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index("landmarks", i);
            const json::Value& v = list->items[i];
            CountryLandmarkDef l;
            if (!Keys(v, p, {"id", "name", "kind", "east_km", "north_km", "notes"}) || !RequireString(v, p, "id", l.id) ||
                !RequireString(v, p, "name", l.name) || !RequireString(v, p, "kind", l.kind) || !Centre(v, p, l.at)) {
                return false;
            }
            const std::vector<std::string>& kinds = CountryLandmarkKinds();
            if (std::find(kinds.begin(), kinds.end(), l.kind) == kinds.end()) return Fail(Join(p, "kind"), "unknown landmark kind '" + l.kind + "'");
            for (const CountryLandmarkDef& o : out.landmarks) {
                if (o.id == l.id) return Fail(Join(p, "id"), "'" + l.id + "' is listed twice");
            }
            out.landmarks.push_back(std::move(l));
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadCountryDefJson(const std::string& text, CountryDef& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) {
        if (error) *error = "country map: " + parse_error;
        return false;
    }
    detail::CountryLoader loader;
    if (!loader.Load(root, out)) {
        if (error) *error = "country map: " + loader.Message();
        return false;
    }
    return true;
}

// Loads and builds data/<file> (e.g. "maps/pannonia.json").
inline std::shared_ptr<const CountryMap> LoadCountryMap(const std::string& data_dir, const std::string& file, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(data_dir + "/" + file, text)) {
        if (error) *error = "cannot open " + file;
        return nullptr;
    }
    CountryDef def;
    if (!LoadCountryDefJson(text, def, error)) return nullptr;
    return CountryMap::Build(std::move(def));
}

}  // namespace simcore
