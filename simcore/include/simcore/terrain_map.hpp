// simcore/terrain_map.hpp — a generated map: the ground, its towns, woods, roads and military sites, built from a map
// file (data/maps/<id>.json) the same way on every machine.
//
// The sim and the picture share it: the sim's line of sight runs over the same baked heights the game draws, and the
// woods' and buildings' heights are kept as an obstacle grid for the propagation work still to come (roadmap E9). The
// detail is deliberately the least that reads from a vehicle or an aircraft: every house, wood block, revetment and
// runway is one box.
//
// Frames. The map is laid out in its own ENU frame (its "origin", normally the theatre's), east and north in metres
// on the tangent plane, heights above sea level. A scenario plays in its own frame; MapTerrain looks the map up from
// the scenario's east and north through the Earth-centred frame, and MapFrameLink turns a point of the map (its east,
// north and height) into the scenario's ENU, which bends the ground away with the Earth's curvature: 150 km out it
// lies 1.8 km below the scenario's horizontal plane, which is where the sim's aircraft fly over it.
//
// Ground beyond the map ("far_km") is drawn coarser and carries only what the file lists there by name (towns, sites,
// roads) and scattered woods, so aircraft coming in from the borders fly over land, not off an edge. A district of a
// country names the country's file as its "far_ground": beyond the map proper the ground is then the country's (its
// ranges, gorges, lakes and rivers), so what stands there, what the sim sees and what the game draws are one ground.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/noise.hpp"
#include "simcore/terrain.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// ---------------------------------------------------------------------------------------------- the map file

struct MapPoint {
    double east_m = 0.0;
    double north_m = 0.0;
};

struct MapLineDef {
    std::string id;
    std::string name;
    std::string line_class;  // road: main, secondary, track; "rail"; "river"
    double width_m = 6.0;
    std::vector<MapPoint> points;
};

struct MapHillDef {
    std::string id;
    MapPoint centre;
    double radius_m = 1000.0;
    double height_m = 0.0;
};

struct MapReliefDef {
    double wavelength_m = 1000.0;
    double amplitude_m = 0.0;
};

// A range of hills along a line, its crest rising and falling into peaks and saddles; or, as a valley, the trough a
// river has cut through the hills along it (docs/COUNTRY_MAP.md).
struct MapRidgeDef {
    std::string id;
    std::vector<MapPoint> points;
    double half_width_m = 5000.0;
    double height_m = 0.0;  // a ridge's crest above its foot; a valley's floor below its shoulders
};

// A lake or reservoir: the ground under it lies below its level, deepening from the shore.
struct MapLakeDef {
    std::string id;
    std::string name;
    std::vector<MapPoint> points;  // its shore, closed
    double level_m = 0.0;          // its surface above sea level
    double depth_m = 5.0;          // at a kilometre from the shore and beyond
};

struct MapSettlementDef {
    std::string id;
    std::string name;
    std::string kind;  // village, town, city
    MapPoint centre;
    double radius_m = 500.0;
    int buildings = 50;
    double heading_rad = 0.0;  // the town's street grid
};

struct MapPolygonDef {
    std::string id;
    std::string name;
    std::vector<MapPoint> points;
};

struct MapSiteDef {
    std::string id;
    std::string name;
    std::string kind;  // see MapSiteKinds()
    MapPoint centre;
    double heading_rad = 0.0;
    int count = 0;  // launchers, guns, tanks, shelters, tents, sheds: what the kind repeats
    double runway_length_m = 2500.0;
    double runway_width_m = 60.0;
    std::vector<MapPoint> vehicles;  // an Osa position's launcher revetments
};

inline const std::vector<std::string>& MapSiteKinds() {
    static const std::vector<std::string> kinds = {"osa_position", "dummy_position", "gun_site", "fuel_depot", "airfield",
                                                   "sam_site", "field_post", "technical_battery", "radar_post", "barracks",
                                                   "bunker", "tower", "factory", "station", "power_station"};
    return kinds;
}

struct MapDef {
    std::string map_id;
    std::string name;
    std::string description;
    GeoPoint origin;
    std::uint64_t seed = 1;
    double west_m = -20000.0, east_m = 20000.0, south_m = -20000.0, north_m = 20000.0;  // the map proper
    double far_m = 150000.0;  // ground drawn out to this far from the origin (a square)
    double airspace_radius_m = 250000.0;
    double airspace_ceiling_m = 25000.0;
    // Ground.
    double base_m = 0.0;
    double core_spacing_m = 50.0;
    double far_spacing_m = 500.0;
    double roughness_m = 0.1;
    std::vector<MapReliefDef> relief;
    std::vector<MapHillDef> hills;
    std::vector<MapRidgeDef> ridges;
    std::vector<MapRidgeDef> valleys;
    double valley_depth_m = 0.0;
    double valley_half_width_m = 500.0;
    std::vector<MapLakeDef> lakes;
    // Lines and places.
    std::vector<MapLineDef> rivers;
    std::vector<MapLineDef> roads;
    std::vector<MapLineDef> railways;
    std::vector<MapSettlementDef> settlements;
    int farmsteads = 0;
    int farm_buildings_min = 2, farm_buildings_max = 4;
    std::vector<MapPolygonDef> forests;
    std::vector<MapPolygonDef> marshes;
    std::vector<MapSiteDef> sites;
    // Woods.
    double block_m = 100.0;
    double far_block_m = 400.0;
    double canopy_min_m = 14.0, canopy_max_m = 24.0;
    int scatter_count = 0;
    double scatter_min_m = 150.0, scatter_max_m = 700.0;
    int far_scatter_count = 0;
    double far_scatter_min_m = 600.0, far_scatter_max_m = 3000.0;
    double riverside_min_m = 0.0, riverside_max_m = 0.0, riverside_share = 0.0;
    // The ground beyond the map proper when it is a country's: the file ("far_ground", e.g. maps/pannonia.json) and its
    // terrain, rivers and lakes as LoadMap read them (in the same frame: the file's origin is the map's). None: the map's
    // own terrain carries on out to far_m.
    std::string far_ground_file;
    std::shared_ptr<const MapDef> far_ground;

    bool InCore(double e, double n) const { return e >= west_m && e <= east_m && n >= south_m && n <= north_m; }
};

// ---------------------------------------------------------------------------------------------- what is built

// Every kind of box the map is made of; the game gives each kind its colour and shape.
enum class MapPart {
    House, Flats, Industrial, Farm, Military, Hangar, Shelter, Tower, Station, Tank, Chimney, CoolingTower, Bridge,
    Canopy, Marsh, Revetment, Pad, Runway, Mast, Tent, Bunker, Launcher, Count
};

inline const char* MapPartName(MapPart p) {
    switch (p) {
    case MapPart::House: return "house";
    case MapPart::Flats: return "flats";
    case MapPart::Industrial: return "industrial";
    case MapPart::Farm: return "farm";
    case MapPart::Military: return "military";
    case MapPart::Hangar: return "hangar";
    case MapPart::Shelter: return "shelter";
    case MapPart::Tower: return "tower";
    case MapPart::Station: return "station";
    case MapPart::Tank: return "tank";
    case MapPart::Chimney: return "chimney";
    case MapPart::CoolingTower: return "cooling_tower";
    case MapPart::Bridge: return "bridge";
    case MapPart::Canopy: return "canopy";
    case MapPart::Marsh: return "marsh";
    case MapPart::Revetment: return "revetment";
    case MapPart::Pad: return "pad";
    case MapPart::Runway: return "runway";
    case MapPart::Mast: return "mast";
    case MapPart::Tent: return "tent";
    case MapPart::Bunker: return "bunker";
    case MapPart::Launcher: return "launcher";
    case MapPart::Count: break;
    }
    return "?";
}

// Round parts (tanks, chimneys, cooling towers) are cylinders `length_m` across; the rest are boxes.
inline bool MapPartIsRound(MapPart p) { return p == MapPart::Tank || p == MapPart::Chimney || p == MapPart::CoolingTower; }

// One box: its footprint centre in the map frame, its bottom above sea level, `length_m` along `heading_rad` (true),
// `width_m` across it.
struct MapBox {
    MapPart part = MapPart::House;
    double east_m = 0.0;
    double north_m = 0.0;
    double base_m = 0.0;
    double length_m = 10.0;
    double width_m = 10.0;
    double height_m = 5.0;
    double heading_rad = 0.0;
};

enum class MapSurface { Water, MainRoad, Road, Track, Rail };

inline const char* MapSurfaceName(MapSurface s) {
    switch (s) {
    case MapSurface::Water: return "water";
    case MapSurface::MainRoad: return "main_road";
    case MapSurface::Road: return "road";
    case MapSurface::Track: return "track";
    case MapSurface::Rail: return "rail";
    }
    return "?";
}

// A ribbon laid on the ground: its centre line sampled every few tens of metres (east, north, height above sea level).
struct MapStrip {
    std::string id;
    MapSurface surface = MapSurface::Road;
    double width_m = 6.0;
    std::vector<Vec3> points;
};

// Where a road or the railway crosses a river: the bridge.
struct MapCrossing {
    std::string line_id;
    std::string river_id;
    MapPoint at;
    double heading_rad = 0.0;  // the line's
    double deck_m = 0.0;       // the deck's height above sea level
    double half_length_m = 0.0;
};

struct MapLabel {
    std::string id;
    std::string name;
    std::string kind;
    MapPoint at;
};

struct MapLayout {
    std::vector<MapBox> boxes;
    std::vector<MapStrip> strips;
    std::vector<MapCrossing> crossings;
    std::vector<MapLabel> labels;

    std::size_t CountOf(MapPart p) const {
        std::size_t n = 0;
        for (const MapBox& b : boxes) n += b.part == p ? 1 : 0;
        return n;
    }
};

// A square grid of heights (row 0 southmost, column 0 westmost), bilinear between samples.
struct MapGrid {
    double west_m = 0.0;
    double south_m = 0.0;
    double spacing_m = 100.0;
    int cols = 0;
    int rows = 0;
    std::vector<double> values;

    bool Covers(double e, double n) const {
        return cols > 1 && rows > 1 && e >= west_m && n >= south_m && e <= west_m + spacing_m * (cols - 1) &&
               n <= south_m + spacing_m * (rows - 1);
    }
    double At(int c, int r) const { return values[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(c)]; }
    double& At(int c, int r) { return values[static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(c)]; }
    double Sample(double e, double n) const {
        const double fx = Clamp((e - west_m) / spacing_m, 0.0, static_cast<double>(cols - 1));
        const double fy = Clamp((n - south_m) / spacing_m, 0.0, static_cast<double>(rows - 1));
        const int x0 = std::min(static_cast<int>(fx), cols - 2), y0 = std::min(static_cast<int>(fy), rows - 2);
        const double tx = fx - x0, ty = fy - y0;
        return (At(x0, y0) * (1 - tx) + At(x0 + 1, y0) * tx) * (1 - ty) + (At(x0, y0 + 1) * (1 - tx) + At(x0 + 1, y0 + 1) * tx) * ty;
    }
    double EastOf(int c) const { return west_m + spacing_m * c; }
    double NorthOf(int r) const { return south_m + spacing_m * r; }
};

// ---------------------------------------------------------------------------------------------- geometry helpers

namespace mapgeo {

inline double DistToSegment(double px, double py, double ax, double ay, double bx, double by, double* along = nullptr) {
    const double dx = bx - ax, dy = by - ay;
    const double len2 = dx * dx + dy * dy;
    double f = len2 > 0.0 ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0.0;
    f = Clamp(f, 0.0, 1.0);
    if (along != nullptr) *along = f;
    return std::hypot(px - (ax + f * dx), py - (ay + f * dy));
}

inline double DistToLine(const std::vector<MapPoint>& pts, double e, double n) {
    double best = 1e18;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        best = std::min(best, DistToSegment(e, n, pts[i - 1].east_m, pts[i - 1].north_m, pts[i].east_m, pts[i].north_m));
    }
    return best;
}

inline bool InPolygon(const std::vector<MapPoint>& poly, double e, double n) {
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const MapPoint& a = poly[i];
        const MapPoint& b = poly[j];
        if ((a.north_m > n) != (b.north_m > n) &&
            e < (b.east_m - a.east_m) * (n - a.north_m) / (b.north_m - a.north_m) + a.east_m) {
            in = !in;
        }
    }
    return in;
}

// Where segments a-b and c-d cross: the fraction along a-b, or false.
inline bool Intersect(const MapPoint& a, const MapPoint& b, const MapPoint& c, const MapPoint& d, double& t_ab, double& t_cd) {
    const double rx = b.east_m - a.east_m, ry = b.north_m - a.north_m;
    const double sx = d.east_m - c.east_m, sy = d.north_m - c.north_m;
    const double den = rx * sy - ry * sx;
    if (std::fabs(den) < 1e-9) return false;
    const double qx = c.east_m - a.east_m, qy = c.north_m - a.north_m;
    t_ab = (qx * sy - qy * sx) / den;
    t_cd = (qx * ry - qy * rx) / den;
    return t_ab >= 0.0 && t_ab <= 1.0 && t_cd >= 0.0 && t_cd <= 1.0;
}

// Segments in buckets, to ask quickly whether a point is close to any road, rail or river.
class SegmentIndex {
public:
    explicit SegmentIndex(double cell_m = 250.0) : cell_(cell_m) {}

    void Add(const std::vector<MapPoint>& pts, double half_width_m) {
        for (std::size_t i = 1; i < pts.size(); ++i) {
            const Seg s{pts[i - 1], pts[i], half_width_m};
            const std::size_t index = segs_.size();
            segs_.push_back(s);
            const long long x0 = Cell(std::min(s.a.east_m, s.b.east_m)), x1 = Cell(std::max(s.a.east_m, s.b.east_m));
            const long long y0 = Cell(std::min(s.a.north_m, s.b.north_m)), y1 = Cell(std::max(s.a.north_m, s.b.north_m));
            for (long long y = y0; y <= y1; ++y) {
                for (long long x = x0; x <= x1; ++x) {
                    const double cx = (static_cast<double>(x) + 0.5) * cell_, cy = (static_cast<double>(y) + 0.5) * cell_;
                    if (DistToSegment(cx, cy, s.a.east_m, s.a.north_m, s.b.east_m, s.b.north_m) <= cell_ * 0.75) {
                        buckets_[Key(x, y)].push_back(index);
                    }
                }
            }
        }
    }

    // Is (e, n) within `clear_m` of any segment's edge? `clear_m` must stay under half a cell.
    bool Near(double e, double n, double clear_m) const {
        const long long cx = Cell(e), cy = Cell(n);
        for (long long y = cy - 1; y <= cy + 1; ++y) {
            for (long long x = cx - 1; x <= cx + 1; ++x) {
                const auto it = buckets_.find(Key(x, y));
                if (it == buckets_.end()) continue;
                for (std::size_t i : it->second) {
                    const Seg& s = segs_[i];
                    if (DistToSegment(e, n, s.a.east_m, s.a.north_m, s.b.east_m, s.b.north_m) < s.half_width + clear_m) return true;
                }
            }
        }
        return false;
    }

private:
    struct Seg {
        MapPoint a, b;
        double half_width;
    };
    long long Cell(double v) const { return static_cast<long long>(std::floor(v / cell_)); }
    static long long Key(long long x, long long y) { return x * 1000003LL + y; }

    double cell_;
    std::vector<Seg> segs_;
    std::unordered_map<long long, std::vector<std::size_t>> buckets_;
};

// Footprints already taken (circles), so a new house does not stand in another.
class Occupancy {
public:
    explicit Occupancy(double cell_m = 40.0) : cell_(cell_m) {}

    bool Free(double e, double n, double radius_m) const {
        const long long cx = Cell(e), cy = Cell(n);
        const long long reach = 1 + static_cast<long long>(std::ceil(radius_m / cell_));
        for (long long y = cy - reach; y <= cy + reach; ++y) {
            for (long long x = cx - reach; x <= cx + reach; ++x) {
                const auto it = buckets_.find(x * 1000003LL + y);
                if (it == buckets_.end()) continue;
                for (const Circle& c : it->second) {
                    if (std::hypot(c.e - e, c.n - n) < c.r + radius_m) return false;
                }
            }
        }
        return true;
    }
    void Take(double e, double n, double radius_m) { buckets_[Cell(e) * 1000003LL + Cell(n)].push_back({e, n, radius_m}); }

private:
    struct Circle {
        double e, n, r;
    };
    long long Cell(double v) const { return static_cast<long long>(std::floor(v / cell_)); }
    double cell_;
    std::unordered_map<long long, std::vector<Circle>> buckets_;
};

}  // namespace mapgeo

// How deep a river's bed is cut below the ground around it.
constexpr double kRiverBedDepthM = 2.0;

// The ground as a map file's terrain shapes it, before any site is levelled: the relief, the hills, the river valleys
// and beds. One function for every map, so a district (the Kesmark range) and the country it lies in (simcore/
// country_map.hpp) share the same ground wherever their files share the same terrain.
// A valley's floor stays this far above the plain's base (the river runs out of the hills onto the plain).
constexpr double kValleyFloorAbovePlainM = 15.0;

inline double NaturalGround(const MapDef& d, double e, double n) {
    if (d.far_ground && !d.InCore(e, n)) return NaturalGround(*d.far_ground, e, n);  // beyond a district: its country's
    double h = d.base_m;
    std::uint64_t s = d.seed;
    for (const MapReliefDef& r : d.relief) {
        s = noise::Mix64(s);
        h += r.amplitude_m * (2.0 * noise::Value(s, e, n, r.wavelength_m) - 1.0);
    }
    for (const MapHillDef& hill : d.hills) {
        const double dist = std::hypot(e - hill.centre.east_m, n - hill.centre.north_m);
        if (dist < hill.radius_m) h += hill.height_m * 0.5 * (1.0 + std::cos(kPi * dist / hill.radius_m));
    }
    // Ranges: across, a smooth rise to the crest; along it, peaks and saddles (two thirds to four thirds of its height).
    for (std::size_t i = 0; i < d.ridges.size(); ++i) {
        const MapRidgeDef& r = d.ridges[i];
        const double dist = mapgeo::DistToLine(r.points, e, n);
        if (dist >= r.half_width_m) continue;
        const double across = 0.5 * (1.0 + std::cos(kPi * dist / r.half_width_m));
        const std::uint64_t rs = noise::Mix64(d.seed ^ (0x9E3779B97F4A7C15ULL * (i + 1)));
        const double along = 2.0 / 3.0 + (2.0 / 3.0) * noise::Value(rs, e, n, 1.5 * r.half_width_m);
        h += r.height_m * across * along;
    }
    // Valleys the rivers cut through the hills: a smooth trough along their line, never below the plain they run out to.
    for (const MapRidgeDef& v : d.valleys) {
        const double dist = mapgeo::DistToLine(v.points, e, n);
        if (dist >= v.half_width_m) continue;
        const double above_plain = std::max(0.0, h - (d.base_m + kValleyFloorAbovePlainM));
        h -= std::min(v.height_m, above_plain) * 0.5 * (1.0 + std::cos(kPi * dist / v.half_width_m));
    }
    for (const MapLineDef& r : d.rivers) {
        const double dist = mapgeo::DistToLine(r.points, e, n);
        if (dist < d.valley_half_width_m && d.valley_depth_m > 0.0) {
            h -= d.valley_depth_m * 0.5 * (1.0 + std::cos(kPi * dist / d.valley_half_width_m));
        }
        const double bed = 0.5 * r.width_m + 15.0;
        if (dist < bed) h -= kRiverBedDepthM * noise::Smooth(1.0 - dist / bed);
    }
    // Under a lake the ground lies below its level, deepening from the shore over a kilometre.
    for (const MapLakeDef& l : d.lakes) {
        if (l.points.size() < 3 || !mapgeo::InPolygon(l.points, e, n)) continue;
        std::vector<MapPoint> shore = l.points;
        shore.push_back(l.points.front());
        const double from_shore = mapgeo::DistToLine(shore, e, n);
        h = std::min(h, l.level_m - 0.5 - l.depth_m * noise::Smooth(std::min(1.0, from_shore / 1000.0)));
    }
    return h;
}

// ---------------------------------------------------------------------------------------------- the built map

class MapData {
public:
    // Bakes the ground and lays the map out. Takes a second or so for a 40 km map.
    static std::shared_ptr<const MapData> Build(MapDef def) {
        std::shared_ptr<MapData> m(new MapData(std::move(def)));
        m->Bake();
        m->Lay();
        m->BakeObstacles();
        return m;
    }

    const MapDef& Def() const { return def_; }
    const MapLayout& Layout() const { return layout_; }
    const MapGrid& CoreGrid() const { return core_; }
    const MapGrid& FarGrid() const { return far_; }
    const MapGrid& ObstacleGrid() const { return obstacles_; }

    // The ground's height above sea level at (east, north) of the map frame: the baked grids, the analytic ground
    // beyond them.
    double GroundHeight(double e, double n) const {
        if (core_.Covers(e, n)) return core_.Sample(e, n);
        if (far_.Covers(e, n)) return far_.Sample(e, n);
        return Analytic(e, n);
    }

    // How high woods and buildings stand above the ground (inside the map; 0 beyond it). For E9's obstacle heights.
    double ObstacleHeight(double e, double n) const { return obstacles_.Covers(e, n) ? obstacles_.Sample(e, n) : 0.0; }

    // The river's surface where it passes (east, north).
    double WaterHeight(double e, double n) const { return std::max(Analytic(e, n) + kBedDepthM - 0.3, GroundHeight(e, n) + 0.3); }

    // The ground as the file shapes it (relief, hills, the valley, the flattened sites), before baking.
    double Analytic(double e, double n) const {
        double h = Natural(e, n);
        for (const Flat& f : flats_) {
            const double d = std::hypot(e - f.e, n - f.n);
            if (d >= f.r) continue;
            const double blend = std::min(0.4 * f.r, 500.0);  // level out to r - blend, then back to the ground
            const double w = d <= f.r - blend ? 1.0 : 1.0 - noise::Smooth((d - (f.r - blend)) / blend);
            h += (f.h - h) * w;
        }
        return h;
    }

private:
    static constexpr double kBedDepthM = kRiverBedDepthM;

    struct Flat {
        double e, n, r, h;
    };

    explicit MapData(MapDef def) : def_(std::move(def)) {}

    // Relief, hills and the river valley: the ground before the sites are levelled.
    double Natural(double e, double n) const { return NaturalGround(def_, e, n); }

    static double SiteClearance(const MapSiteDef& s) {
        if (s.kind == "airfield") return 0.5 * s.runway_length_m + 400.0;
        if (s.kind == "power_station" ) return 450.0;
        if (s.kind == "osa_position" || s.kind == "factory") return 260.0;
        if (s.kind == "fuel_depot" || s.kind == "sam_site" || s.kind == "technical_battery" || s.kind == "barracks") return 220.0;
        if (s.kind == "dummy_position" || s.kind == "field_post" || s.kind == "radar_post" || s.kind == "station") return 140.0;
        if (s.kind == "gun_site") return 100.0;
        if (s.kind == "bunker") return 80.0;
        return 35.0;  // a tower
    }

    static double SiteFlatRadius(const MapSiteDef& s) {
        if (s.kind == "airfield") return 0.5 * s.runway_length_m + 600.0;
        if (s.kind == "tower" || s.kind == "station" || s.kind == "factory" || s.kind == "barracks") return 0.0;  // in a town
        return SiteClearance(s);
    }

    void Bake() {
        for (const MapSiteDef& s : def_.sites) {
            const double r = SiteFlatRadius(s);
            if (r > 0.0) flats_.push_back({s.centre.east_m, s.centre.north_m, r, Natural(s.centre.east_m, s.centre.north_m)});
        }
        const auto fill = [this](MapGrid& g, double west, double south, double east, double north, double spacing) {
            g.spacing_m = spacing;
            g.west_m = west;
            g.south_m = south;
            g.cols = static_cast<int>(std::lround((east - west) / spacing)) + 1;
            g.rows = static_cast<int>(std::lround((north - south) / spacing)) + 1;
            g.values.assign(static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows), 0.0);
            for (int r = 0; r < g.rows; ++r) {
                for (int c = 0; c < g.cols; ++c) g.At(c, r) = Analytic(g.EastOf(c), g.NorthOf(r));
            }
        };
        fill(core_, def_.west_m, def_.south_m, def_.east_m, def_.north_m, def_.core_spacing_m);
        fill(far_, -def_.far_m, -def_.far_m, def_.far_m, def_.far_m, def_.far_spacing_m);
    }

    // ------------------------------------------------------------------ laying the map out

    void Box(MapPart part, double e, double n, double length, double width, double height, double heading, double sink = 0.5) {
        MapBox b;
        b.part = part;
        b.east_m = e;
        b.north_m = n;
        b.base_m = GroundHeight(e, n) - sink;
        b.length_m = length;
        b.width_m = width;
        b.height_m = height + sink;
        b.heading_rad = heading;
        layout_.boxes.push_back(b);
    }

    void Lay() {
        noise::Stream rng(noise::Mix64(def_.seed ^ 0x1A70Full));
        // Indices: what buildings and woods keep clear of.
        for (const MapLineDef& r : def_.roads) lines_.Add(r.points, 0.5 * r.width_m);
        for (const MapLineDef& r : def_.railways) lines_.Add(r.points, 0.5 * r.width_m);
        for (const MapLineDef& r : def_.rivers) {
            water_.Add(r.points, 0.5 * r.width_m);
            lines_.Add(r.points, 0.5 * r.width_m);
        }
        for (const MapSiteDef& s : def_.sites) {
            occupied_.Take(s.centre.east_m, s.centre.north_m, SiteClearance(s));
        }
        LayLines();
        for (const MapSiteDef& s : def_.sites) LaySite(s, rng);
        for (const MapSettlementDef& s : def_.settlements) LaySettlement(s, rng);
        LayFarmsteads(rng);
        LayWoods(rng);
        LayMarshes();
        for (const MapSettlementDef& s : def_.settlements) layout_.labels.push_back({s.id, s.name, s.kind, s.centre});
        for (const MapSiteDef& s : def_.sites) layout_.labels.push_back({s.id, s.name, s.kind, s.centre});
    }

    // Roads, railways and rivers as strips; a bridge wherever a road or rail crosses a river.
    void LayLines() {
        const auto crossings_of = [this](const MapLineDef& line, bool rail) {
            for (const MapLineDef& river : def_.rivers) {
                for (std::size_t i = 1; i < line.points.size(); ++i) {
                    for (std::size_t j = 1; j < river.points.size(); ++j) {
                        double t = 0.0, u = 0.0;
                        if (!mapgeo::Intersect(line.points[i - 1], line.points[i], river.points[j - 1], river.points[j], t, u)) continue;
                        const MapPoint& a = line.points[i - 1];
                        const MapPoint& b = line.points[i];
                        MapCrossing c;
                        c.line_id = line.id;
                        c.river_id = river.id;
                        c.at = {a.east_m + (b.east_m - a.east_m) * t, a.north_m + (b.north_m - a.north_m) * t};
                        // Two lines meeting on the river share its bridge.
                        bool shared = false;
                        for (const MapCrossing& o : layout_.crossings) {
                            shared = shared || std::hypot(o.at.east_m - c.at.east_m, o.at.north_m - c.at.north_m) < 30.0;
                        }
                        if (shared) continue;
                        c.heading_rad = std::atan2(b.east_m - a.east_m, b.north_m - a.north_m);
                        c.deck_m = WaterHeight(c.at.east_m, c.at.north_m) + (rail ? 5.0 : 4.0);
                        c.half_length_m = 0.5 * river.width_m + 30.0;
                        layout_.crossings.push_back(c);
                        {
                            const double w = rail ? 7.0 : line.width_m + 2.0;
                            MapBox deck;
                            deck.part = MapPart::Bridge;
                            deck.east_m = c.at.east_m;
                            deck.north_m = c.at.north_m;
                            deck.base_m = c.deck_m - 1.4;
                            deck.length_m = 2.0 * c.half_length_m;
                            deck.width_m = w;
                            deck.height_m = 1.4;
                            deck.heading_rad = c.heading_rad;
                            layout_.boxes.push_back(deck);
                        }
                    }
                }
            }
        };
        for (const MapLineDef& r : def_.roads) crossings_of(r, false);
        for (const MapLineDef& r : def_.railways) crossings_of(r, true);
        for (const MapLineDef& r : def_.roads) {
            const MapSurface s = r.line_class == "main" ? MapSurface::MainRoad : (r.line_class == "track" ? MapSurface::Track : MapSurface::Road);
            Strip(r, s, 0.25);
        }
        for (const MapLineDef& r : def_.railways) Strip(r, MapSurface::Rail, 0.45);
        for (const MapLineDef& r : def_.rivers) Strip(r, MapSurface::Water, 0.0);
    }

    void Strip(const MapLineDef& line, MapSurface surface, double lift_m) {
        MapStrip s;
        s.id = line.id;
        s.surface = surface;
        s.width_m = line.width_m;
        for (std::size_t i = 1; i < line.points.size(); ++i) {
            const MapPoint& a = line.points[i - 1];
            const MapPoint& b = line.points[i];
            const double len = std::hypot(b.east_m - a.east_m, b.north_m - a.north_m);
            const bool core = def_.InCore(a.east_m, a.north_m) || def_.InCore(b.east_m, b.north_m);
            const double step = core ? 0.5 * def_.core_spacing_m : 0.5 * def_.far_spacing_m;
            const int n = std::max(1, static_cast<int>(std::ceil(len / step)));
            for (int k = (i == 1 ? 0 : 1); k <= n; ++k) {
                const double f = static_cast<double>(k) / n;
                const double e = a.east_m + (b.east_m - a.east_m) * f, nn = a.north_m + (b.north_m - a.north_m) * f;
                double h = 0.0;
                if (surface == MapSurface::Water) {
                    h = WaterHeight(e, nn);
                } else {
                    h = GroundHeight(e, nn) + lift_m;
                    for (const MapCrossing& c : layout_.crossings) {  // up onto the bridge and down again
                        const double d = std::hypot(e - c.at.east_m, nn - c.at.north_m);
                        constexpr double ramp = 120.0;
                        if (d <= c.half_length_m) h = std::max(h, c.deck_m + 0.05);
                        else if (d < c.half_length_m + ramp) h = std::max(h, c.deck_m + 0.05 - (c.deck_m - h) * (d - c.half_length_m) / ramp);
                    }
                }
                s.points.push_back({e, nn, h});
            }
        }
        layout_.strips.push_back(std::move(s));
    }

    // A point in the site's frame: `fwd` along its heading, `right` to the right of it.
    static MapPoint At(const MapSiteDef& s, double fwd, double right) {
        const double h = s.heading_rad;
        return {s.centre.east_m + fwd * std::sin(h) + right * std::cos(h), s.centre.north_m + fwd * std::cos(h) - right * std::sin(h)};
    }

    void SiteBox(const MapSiteDef& s, MapPart part, double fwd, double right, double length, double width, double height,
                 double turn = 0.0, double sink = 0.5) {
        const MapPoint p = At(s, fwd, right);
        Box(part, p.east_m, p.north_m, length, width, height, s.heading_rad + turn, sink);
    }

    // An earth berm round three sides of a pit `l` long and `w` wide, open to the rear.
    void Revetment(double e, double n, double heading, double l, double w, double height) {
        constexpr double t = 3.0;
        const double sh = std::sin(heading), ch = std::cos(heading);
        const auto put = [&](double fwd, double right, double bl, double bw) {
            Box(MapPart::Revetment, e + fwd * sh + right * ch, n + fwd * ch - right * sh, bl, bw, height, heading);
        };
        put(0.5 * l + 0.5 * t, 0.0, t, w + 2 * t);
        put(0.0, 0.5 * w + 0.5 * t, l, t);
        put(0.0, -0.5 * w - 0.5 * t, l, t);
    }

    void GunPit(double e, double n, double size, double height) {
        constexpr double t = 2.5;
        Box(MapPart::Revetment, e, n + 0.5 * size, size + t, t, height, 0.0);
        Box(MapPart::Revetment, e, n - 0.5 * size, size + t, t, height, 0.0);
        Box(MapPart::Revetment, e + 0.5 * size, n, t, size - t, height, 0.0);
        Box(MapPart::Revetment, e - 0.5 * size, n, t, size - t, height, 0.0);
    }

    void LaySite(const MapSiteDef& s, noise::Stream& rng) {
        const double e = s.centre.east_m, n = s.centre.north_m, h = s.heading_rad;
        if (s.kind == "osa_position") {
            for (const MapPoint& v : s.vehicles) Revetment(v.east_m, v.north_m, h, 11.0, 6.0, 2.2);
            SiteBox(s, MapPart::Pad, 0.0, 0.0, 16.0, 9.0, 0.25);
            Box(MapPart::Mast, e + 45.0, n + 30.0, 0.6, 0.6, 16.0, 0.0);  // the post's radio mast, off the pad
        } else if (s.kind == "dummy_position") {
            const MapPoint a = At(s, 0.0, -50.0), b = At(s, 0.0, 50.0);
            Revetment(a.east_m, a.north_m, h, 11.0, 6.0, 2.2);
            Revetment(b.east_m, b.north_m, h, 11.0, 6.0, 2.2);
        } else if (s.kind == "gun_site") {
            const int count = s.count > 0 ? s.count : 4;
            for (int i = 0; i < count; ++i) {
                const double a = kTwoPi * i / count + 0.3;
                GunPit(e + 55.0 * std::sin(a), n + 55.0 * std::cos(a), 9.0, 1.3);
            }
        } else if (s.kind == "fuel_depot") {
            const int count = s.count > 0 ? s.count : 6;
            const int per_row = (count + 1) / 2;
            for (int i = 0; i < count; ++i) {
                const double fwd = (i % per_row - 0.5 * (per_row - 1)) * 34.0, right = (i < per_row ? -20.0 : 20.0);
                SiteBox(s, MapPart::Tank, fwd, right, rng.Range(12.0, 18.0), 0.0, rng.Range(8.0, 12.0));
            }
            const double half = 0.5 * per_row * 34.0 + 20.0;
            SiteBox(s, MapPart::Revetment, half, 0.0, 3.0, 90.0, 1.8);
            SiteBox(s, MapPart::Revetment, -half, 0.0, 3.0, 90.0, 1.8);
            SiteBox(s, MapPart::Revetment, 0.0, 45.0, 2.0 * half, 3.0, 1.8);
            SiteBox(s, MapPart::Revetment, 0.0, -45.0, 2.0 * half, 3.0, 1.8);
            SiteBox(s, MapPart::Industrial, -half - 30.0, 0.0, 20.0, 12.0, 6.0);
        } else if (s.kind == "airfield") {
            const double len = s.runway_length_m, w = s.runway_width_m;
            const int shelters = s.count > 0 ? s.count : 10;
            SiteBox(s, MapPart::Runway, 0.0, 0.0, len, w, 0.3, 0.0, 0.2);
            SiteBox(s, MapPart::Runway, 0.0, 180.0, 0.9 * len, 20.0, 0.25, 0.0, 0.2);  // the parallel taxiway
            for (double f : {-0.45, 0.0, 0.45}) SiteBox(s, MapPart::Runway, f * len, 90.0, 20.0, 180.0, 0.25, 0.0, 0.2);
            for (int i = 0; i < shelters; ++i) {
                const double fwd = (i - 0.5 * (shelters - 1)) * (0.8 * len / std::max(shelters - 1, 1));
                SiteBox(s, MapPart::Runway, fwd, 215.0, 20.0, 50.0, 0.25, 0.0, 0.2);
                SiteBox(s, MapPart::Shelter, fwd, 255.0, 26.0, 24.0, 8.0, kPi / 2);
            }
            SiteBox(s, MapPart::Pad, 0.0, 330.0, 320.0, 140.0, 0.25, 0.0, 0.2);  // the apron
            SiteBox(s, MapPart::Hangar, -100.0, 450.0, 60.0, 45.0, 14.0);
            SiteBox(s, MapPart::Hangar, 0.0, 450.0, 60.0, 45.0, 14.0);
            SiteBox(s, MapPart::Tower, 120.0, 430.0, 8.0, 8.0, 18.0);
            for (int i = 0; i < 4; ++i) SiteBox(s, MapPart::Military, 220.0 + 70.0 * i, 470.0, 50.0, 14.0, 8.0);
        } else if (s.kind == "sam_site") {
            const int count = s.count > 0 ? s.count : 4;
            for (int i = 0; i < count; ++i) {
                const double a = h + kTwoPi * i / count;
                const double pe = e + 75.0 * std::sin(a), pn = n + 75.0 * std::cos(a);
                GunPit(pe, pn, 15.0, 2.0);
                Box(MapPart::Launcher, pe, pn, 7.0, 3.0, 2.5, a);
            }
            SiteBox(s, MapPart::Pad, 0.0, 0.0, 24.0, 18.0, 0.25);
            SiteBox(s, MapPart::Military, 0.0, 0.0, 10.0, 3.0, 3.2);  // the guidance radar's cabins
            SiteBox(s, MapPart::Mast, 0.0, 0.0, 6.0, 1.0, 6.5);
            SiteBox(s, MapPart::Bunker, -120.0, 0.0, 24.0, 12.0, 3.5);
        } else if (s.kind == "field_post") {
            const int count = s.count > 0 ? s.count : 5;
            for (int i = 0; i < count; ++i) {
                const double a = rng.Range(0.0, kTwoPi), d = rng.Range(10.0, 80.0);
                Box(i % 3 == 0 ? MapPart::Military : MapPart::Tent, e + d * std::sin(a), n + d * std::cos(a),
                    i % 3 == 0 ? 7.0 : 6.0, i % 3 == 0 ? 2.5 : 4.5, i % 3 == 0 ? 3.0 : 2.4, rng.Range(0.0, kPi));
            }
            Box(MapPart::Mast, e + 20.0, n - 15.0, 0.5, 0.5, 14.0, 0.0);
        } else if (s.kind == "technical_battery") {
            const int count = s.count > 0 ? s.count : 4;
            for (int i = 0; i < count; ++i) SiteBox(s, MapPart::Industrial, (i - 0.5 * (count - 1)) * 45.0, 40.0, 32.0, 12.0, 6.5);
            SiteBox(s, MapPart::Pad, 0.0, -10.0, 45.0 * count, 50.0, 0.25);
            SiteBox(s, MapPart::Mast, 0.0, 90.0, 0.5, 0.5, 14.0);
        } else if (s.kind == "radar_post") {
            SiteBox(s, MapPart::Pad, 0.0, 0.0, 45.0, 45.0, 0.25);
            SiteBox(s, MapPart::Military, 0.0, 0.0, 9.0, 3.4, 3.6);
            SiteBox(s, MapPart::Mast, 0.0, 0.0, 7.0, 1.0, 7.5);
            SiteBox(s, MapPart::Tent, 25.0, 20.0, 6.0, 4.5, 2.4);
            SiteBox(s, MapPart::Military, -22.0, 18.0, 6.0, 2.5, 2.8);
        } else if (s.kind == "barracks") {
            const int count = s.count > 0 ? s.count : 6;
            const int per_row = (count + 1) / 2;
            for (int i = 0; i < count; ++i) {
                SiteBox(s, MapPart::Military, (i % per_row - 0.5 * (per_row - 1)) * 75.0, i < per_row ? -60.0 : 60.0, 60.0, 12.0, 9.0);
            }
            SiteBox(s, MapPart::Pad, 0.0, 0.0, 75.0 * per_row, 70.0, 0.2);
        } else if (s.kind == "bunker") {
            SiteBox(s, MapPart::Bunker, 0.0, 0.0, 42.0, 26.0, 4.5);
            SiteBox(s, MapPart::Mast, 30.0, 0.0, 0.6, 0.6, 24.0);
            SiteBox(s, MapPart::Mast, -30.0, 10.0, 0.6, 0.6, 24.0);
        } else if (s.kind == "tower") {
            Box(MapPart::Tower, e, n, 6.0, 6.0, 36.0, h);
            SiteBox(s, MapPart::House, -17.0, 0.0, 26.0, 11.0, 11.0);
        } else if (s.kind == "factory") {
            const int count = s.count > 0 ? s.count : 4;
            for (int i = 0; i < count; ++i) {
                SiteBox(s, MapPart::Industrial, (i % 3 - 1) * 90.0, (i / 3) * 70.0 - 35.0, rng.Range(60.0, 100.0), rng.Range(28.0, 40.0),
                        rng.Range(9.0, 15.0));
            }
            SiteBox(s, MapPart::Chimney, 60.0, 80.0, 5.0, 0.0, 48.0);
        } else if (s.kind == "station") {
            SiteBox(s, MapPart::Station, 0.0, -18.0, 60.0, 14.0, 10.0);
            SiteBox(s, MapPart::Pad, 0.0, 6.0, 220.0, 6.0, 0.7);
        } else if (s.kind == "power_station") {
            SiteBox(s, MapPart::Industrial, 0.0, 0.0, 130.0, 60.0, 36.0);
            for (int i = 0; i < 4; ++i) SiteBox(s, MapPart::Chimney, -45.0 + 30.0 * i, -60.0, 7.0, 0.0, 150.0);
            SiteBox(s, MapPart::CoolingTower, -100.0, 160.0, 90.0, 0.0, 100.0);
            SiteBox(s, MapPart::CoolingTower, 30.0, 160.0, 90.0, 0.0, 100.0);
        }
    }

    bool Clear(double e, double n, double radius_m, double road_clear_m) const {
        return occupied_.Free(e, n, radius_m) && !lines_.Near(e, n, road_clear_m);
    }

    void House(MapPart part, double e, double n, double length, double width, double height, double heading) {
        Box(part, e, n, length, width, height, heading);
        occupied_.Take(e, n, 0.5 * std::hypot(length, width));
    }

    void LaySettlement(const MapSettlementDef& s, noise::Stream& rng) {
        const bool village = s.kind == "village";
        const bool city = s.kind == "city";
        const double lot = village ? 30.0 : (city ? 36.0 : 32.0);
        int placed = 0;
        const auto house_size = [&](double& l, double& w, double& ht) {
            l = rng.Range(10.0, 16.0);
            w = rng.Range(7.0, 10.0);
            ht = rng.Range(4.5, 7.0);
        };
        // Houses along every road through the settlement, end on to the street.
        for (const MapLineDef& road : def_.roads) {
            if (road.line_class == "track") continue;
            for (std::size_t i = 1; i < road.points.size() && placed < s.buildings; ++i) {
                const MapPoint& a = road.points[i - 1];
                const MapPoint& b = road.points[i];
                const double len = std::hypot(b.east_m - a.east_m, b.north_m - a.north_m);
                if (mapgeo::DistToSegment(s.centre.east_m, s.centre.north_m, a.east_m, a.north_m, b.east_m, b.north_m) > s.radius_m) continue;
                const double heading = std::atan2(b.east_m - a.east_m, b.north_m - a.north_m);
                const double ue = std::sin(heading), un = std::cos(heading), re = un, rn = -ue;
                for (double d = 0.5 * lot; d < len && placed < s.buildings; d += lot) {
                    const double pe = a.east_m + ue * d, pn = a.north_m + un * d;
                    const double r = std::hypot(pe - s.centre.east_m, pn - s.centre.north_m);
                    if (r > s.radius_m) continue;
                    for (double side : {1.0, -1.0}) {
                        if (!rng.Chance(0.9 - 0.4 * r / s.radius_m)) continue;
                        double l = 0.0, w = 0.0, ht = 0.0;
                        house_size(l, w, ht);
                        const double off = 0.5 * road.width_m + 7.0 + 0.5 * l;
                        const double he = pe + side * re * off, hn = pn + side * rn * off;
                        if (!Clear(he, hn, 0.5 * std::hypot(l, w), 0.5 * l + 4.0) || water_.Near(he, hn, 30.0)) continue;
                        House(MapPart::House, he, hn, l, w, ht, heading + kPi / 2);
                        if (++placed >= s.buildings) break;
                    }
                }
            }
        }
        // Then the blocks behind: flats in a town's centre, houses round it; a village's second row.
        const double grid = village ? 42.0 : (city ? 52.0 : 46.0);
        const double sh = std::sin(s.heading_rad), ch = std::cos(s.heading_rad);
        const int span = static_cast<int>(std::ceil(s.radius_m / grid));
        std::vector<std::pair<int, int>> cells;
        for (int j = -span; j <= span; ++j) {
            for (int i = -span; i <= span; ++i) cells.push_back({i, j});
        }
        for (std::size_t k = cells.size(); k > 1; --k) {  // in a random order, so a short count spreads over the whole town
            std::swap(cells[k - 1], cells[static_cast<std::size_t>(rng.Next() % k)]);
        }
        for (const auto& cell : cells) {
            if (placed >= s.buildings) break;
            const double x = cell.first * grid, y = cell.second * grid;
            const double r = std::hypot(x, y);
            if (r > s.radius_m) continue;
            const double share = village ? 0.3 * (1.0 - r / s.radius_m) : 0.95 - 0.5 * r / s.radius_m;
            if (!rng.Chance(share)) continue;
            const double pe = s.centre.east_m + x * ch + y * sh, pn = s.centre.north_m - x * sh + y * ch;
            const bool flats = !village && r < (city ? 0.45 : 0.3) * s.radius_m;
            double l = 0.0, w = 0.0, ht = 0.0;
            if (flats) {
                l = rng.Range(40.0, 60.0);
                w = rng.Range(11.0, 14.0);
                ht = rng.Range(12.0, city ? 30.0 : 16.0);
            } else {
                house_size(l, w, ht);
            }
            if (!Clear(pe, pn, 0.5 * std::hypot(l, w), 0.5 * l + 4.0) || water_.Near(pe, pn, 30.0)) continue;
            House(flats ? MapPart::Flats : MapPart::House, pe, pn, l, w, ht, s.heading_rad + (rng.Chance(0.5) ? 0.0 : kPi / 2));
            ++placed;
        }
    }

    bool NearSettlement(double e, double n, double margin_m) const {
        for (const MapSettlementDef& s : def_.settlements) {
            if (std::hypot(e - s.centre.east_m, n - s.centre.north_m) < s.radius_m + margin_m) return true;
        }
        return false;
    }

    bool InForest(double e, double n) const {
        for (const MapPolygonDef& f : def_.forests) {
            if (mapgeo::InPolygon(f.points, e, n)) return true;
        }
        return false;
    }

    // Within margin_m of the far ground's water (its country's lakes and rivers): no wood of the far ground stands there.
    bool OnFarWater(double e, double n, double margin_m) const {
        if (!def_.far_ground) return false;
        for (const MapLakeDef& l : def_.far_ground->lakes) {
            std::vector<MapPoint> shore = l.points;
            shore.push_back(l.points.front());
            if (mapgeo::InPolygon(l.points, e, n) || mapgeo::DistToLine(shore, e, n) < margin_m) return true;
        }
        for (const MapLineDef& r : def_.far_ground->rivers) {
            if (mapgeo::DistToLine(r.points, e, n) < margin_m + 0.5 * r.width_m) return true;
        }
        return false;
    }

    void LayFarmsteads(noise::Stream& rng) {
        int placed = 0;
        for (int tries = 0; placed < def_.farmsteads && tries < def_.farmsteads * 50; ++tries) {
            const double e = rng.Range(def_.west_m + 300.0, def_.east_m - 300.0), n = rng.Range(def_.south_m + 300.0, def_.north_m - 300.0);
            if (NearSettlement(e, n, 300.0) || InForest(e, n) || water_.Near(e, n, 120.0) || !Clear(e, n, 80.0, 50.0)) continue;
            const double heading = rng.Range(0.0, kPi);
            const int count = def_.farm_buildings_min + static_cast<int>(rng.Next() % static_cast<std::uint64_t>(
                                                                    std::max(1, def_.farm_buildings_max - def_.farm_buildings_min + 1)));
            const double sh = std::sin(heading), ch = std::cos(heading);
            for (int i = 0; i < count; ++i) {
                const double fwd = i == 0 ? 0.0 : rng.Range(-35.0, 35.0), right = i == 0 ? 0.0 : (i % 2 ? 22.0 : -22.0);
                const bool barn = i > 0;
                Box(MapPart::Farm, e + fwd * sh + right * ch, n + fwd * ch - right * sh, barn ? rng.Range(16.0, 26.0) : 14.0,
                    barn ? rng.Range(8.0, 12.0) : 8.0, barn ? rng.Range(5.0, 8.0) : 5.0, heading + (barn && rng.Chance(0.5) ? kPi / 2 : 0.0));
            }
            occupied_.Take(e, n, 60.0);
            ++placed;
        }
    }

    // A canopy block, unless something stands there.
    void Canopy(double e, double n, double size, std::uint64_t seed) {
        const long long key = static_cast<long long>(std::floor(e / 10.0)) * 2000003LL + static_cast<long long>(std::floor(n / 10.0));
        if (canopy_cells_.count(key) != 0) return;  // a named forest, a copse and the river's trees may claim one block
        if (water_.Near(e, n, 10.0) || lines_.Near(e, n, 0.5 * size + 4.0) || !occupied_.Free(e, n, 0.5 * size)) return;
        canopy_cells_.insert(key);
        const double u = noise::Value(seed, e, n, 300.0);
        Box(MapPart::Canopy, e, n, size, size, def_.canopy_min_m + (def_.canopy_max_m - def_.canopy_min_m) * u, 0.0, 1.0);
    }

    void LayWoods(noise::Stream& rng) {
        const std::uint64_t seed = noise::Mix64(def_.seed ^ 0xF0E57ull);
        const double b = def_.block_m;
        const auto snap = [](double v, double size) { return (std::floor(v / size) + 0.5) * size; };
        // The named forests.
        for (const MapPolygonDef& f : def_.forests) {
            double w = 1e18, e = -1e18, s = 1e18, nn = -1e18;
            for (const MapPoint& p : f.points) {
                w = std::min(w, p.east_m);
                e = std::max(e, p.east_m);
                s = std::min(s, p.north_m);
                nn = std::max(nn, p.north_m);
            }
            for (double y = snap(s, b); y < nn; y += b) {
                for (double x = snap(w, b); x < e; x += b) {
                    if (mapgeo::InPolygon(f.points, x, y)) Canopy(x, y, b, seed);
                }
            }
        }
        // Copses scattered over the map, and big woods beyond it.
        const auto copse = [&](double ce, double cn, double radius, double size) {
            for (double y = snap(cn - radius, size); y < cn + radius; y += size) {
                for (double x = snap(ce - radius, size); x < ce + radius; x += size) {
                    const double d = std::hypot(x - ce, y - cn);
                    if (d < radius * (0.65 + 0.6 * noise::Value(seed ^ 0xC0Full, x, y, 0.5 * radius))) Canopy(x, y, size, seed);
                }
            }
        };
        for (int i = 0, tries = 0; i < def_.scatter_count && tries < def_.scatter_count * 40; ++tries) {
            const double r = rng.Range(def_.scatter_min_m, def_.scatter_max_m);
            const double ce = rng.Range(def_.west_m + r, def_.east_m - r), cn = rng.Range(def_.south_m + r, def_.north_m - r);
            if (NearSettlement(ce, cn, r + 150.0) || InForest(ce, cn) || water_.Near(ce, cn, 150.0) || !occupied_.Free(ce, cn, r)) continue;
            copse(ce, cn, r, b);
            ++i;
        }
        const double fb = def_.far_block_m;
        for (int i = 0, tries = 0; i < def_.far_scatter_count && tries < def_.far_scatter_count * 40; ++tries) {
            const double r = rng.Range(def_.far_scatter_min_m, def_.far_scatter_max_m);
            const double ce = rng.Range(-def_.far_m + r, def_.far_m - r), cn = rng.Range(-def_.far_m + r, def_.far_m - r);
            if (def_.InCore(ce, cn) || def_.InCore(ce - r, cn - r) || def_.InCore(ce + r, cn + r) || def_.InCore(ce - r, cn + r) ||
                def_.InCore(ce + r, cn - r) || NearSettlement(ce, cn, r + 300.0) || OnFarWater(ce, cn, r) || !occupied_.Free(ce, cn, r)) {
                continue;
            }
            copse(ce, cn, r, fb);
            ++i;
        }
        // Trees along the river banks, inside the map.
        if (def_.riverside_share > 0.0) {
            for (const MapLineDef& river : def_.rivers) {
                for (double y = snap(def_.south_m, b); y < def_.north_m; y += b) {
                    for (double x = snap(def_.west_m, b); x < def_.east_m; x += b) {
                        const double d = mapgeo::DistToLine(river.points, x, y);
                        if (d < def_.riverside_min_m || d > def_.riverside_max_m) continue;
                        if (noise::Value(seed ^ 0xB4Aull, x, y, 250.0) > def_.riverside_share || NearSettlement(x, y, 100.0)) continue;
                        Canopy(x, y, b, seed);
                    }
                }
            }
        }
    }

    void LayMarshes() {
        const double b = def_.block_m;
        for (const MapPolygonDef& m : def_.marshes) {
            double w = 1e18, e = -1e18, s = 1e18, nn = -1e18;
            for (const MapPoint& p : m.points) {
                w = std::min(w, p.east_m);
                e = std::max(e, p.east_m);
                s = std::min(s, p.north_m);
                nn = std::max(nn, p.north_m);
            }
            for (double y = (std::floor(s / b) + 0.5) * b; y < nn; y += b) {
                for (double x = (std::floor(w / b) + 0.5) * b; x < e; x += b) {
                    if (!mapgeo::InPolygon(m.points, x, y) || water_.Near(x, y, 5.0) || lines_.Near(x, y, 0.5 * b)) continue;
                    Box(MapPart::Marsh, x, y, b, b, 0.25, 0.0, 0.3);
                }
            }
        }
    }

    // The tallest thing standing over each cell of the map, above the ground (woods and buildings).
    void BakeObstacles() {
        obstacles_.west_m = def_.west_m;
        obstacles_.south_m = def_.south_m;
        obstacles_.spacing_m = def_.core_spacing_m;
        obstacles_.cols = core_.cols;
        obstacles_.rows = core_.rows;
        obstacles_.values.assign(core_.values.size(), 0.0);
        for (const MapBox& bx : layout_.boxes) {
            if (bx.part == MapPart::Marsh || bx.part == MapPart::Pad || bx.part == MapPart::Runway) continue;
            if (!def_.InCore(bx.east_m, bx.north_m)) continue;
            const double reach = 0.5 * std::hypot(bx.length_m, std::max(bx.width_m, MapPartIsRound(bx.part) ? bx.length_m : 0.0));
            const double top = bx.base_m + bx.height_m;
            const int c0 = std::max(0, static_cast<int>(std::floor((bx.east_m - reach - obstacles_.west_m) / obstacles_.spacing_m)));
            const int c1 = std::min(obstacles_.cols - 1, static_cast<int>(std::ceil((bx.east_m + reach - obstacles_.west_m) / obstacles_.spacing_m)));
            const int r0 = std::max(0, static_cast<int>(std::floor((bx.north_m - reach - obstacles_.south_m) / obstacles_.spacing_m)));
            const int r1 = std::min(obstacles_.rows - 1, static_cast<int>(std::ceil((bx.north_m + reach - obstacles_.south_m) / obstacles_.spacing_m)));
            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    const double de = obstacles_.EastOf(c) - bx.east_m, dn = obstacles_.NorthOf(r) - bx.north_m;
                    const double along = de * std::sin(bx.heading_rad) + dn * std::cos(bx.heading_rad);
                    const double across = de * std::cos(bx.heading_rad) - dn * std::sin(bx.heading_rad);
                    const double half_cell = 0.5 * obstacles_.spacing_m;
                    if (std::fabs(along) > 0.5 * bx.length_m + half_cell || std::fabs(across) > 0.5 * bx.width_m + half_cell) continue;
                    double& v = obstacles_.At(c, r);
                    v = std::max(v, top - core_.At(c, r));
                }
            }
        }
    }

    MapDef def_;
    MapGrid core_, far_, obstacles_;
    std::vector<Flat> flats_;
    MapLayout layout_;
    mapgeo::SegmentIndex lines_{250.0};
    mapgeo::SegmentIndex water_{250.0};
    mapgeo::Occupancy occupied_{40.0};
    std::unordered_set<long long> canopy_cells_;
};

// ---------------------------------------------------------------------------------------------- frames

// A point of the map in a scenario's frame, and back. Exact through the Earth-centred frame (Bowring's closed form for
// the geodetic latitude, millimetres at these heights).
class MapFrameLink {
public:
    MapFrameLink(const GeoPoint& map_origin, const GeoPoint& frame_origin) : map_(map_origin), frame_(frame_origin) {
        // The horizontal part of frame -> map is affine: a 2x2 and an offset.
        const Vec3 o = map_.EcefToEnu(frame_.EnuToEcef({0.0, 0.0, 0.0}));
        const Vec3 ex = map_.EcefToEnu(frame_.EnuToEcef({1000.0, 0.0, 0.0}));
        const Vec3 ny = map_.EcefToEnu(frame_.EnuToEcef({0.0, 1000.0, 0.0}));
        off_e_ = o.x;
        off_n_ = o.y;
        a_ = (ex.x - o.x) / 1000.0;
        b_ = (ny.x - o.x) / 1000.0;
        c_ = (ex.y - o.y) / 1000.0;
        d_ = (ny.y - o.y) / 1000.0;
        frame_ecef_ = frame_.EnuToEcef({0.0, 0.0, 0.0});
    }

    // The map's east and north under the scenario's (east, north).
    MapPoint FrameToMap(double east_m, double north_m) const {
        return {off_e_ + a_ * east_m + b_ * north_m, off_n_ + c_ * east_m + d_ * north_m};
    }

    // The point `alt_m` above sea level over the map's (east, north), in the scenario's ENU.
    Vec3 MapToFrame(double east_m, double north_m, double alt_m) const {
        GeoPoint g = Geodetic(map_.EnuToEcef({east_m, north_m, 0.0}));
        g.alt_m = alt_m;
        return frame_.EcefToEnu(GeodeticToEcef(g));
    }

    // The local east, north and up at the map's (east, north), as directions in the scenario's ENU: what stands
    // upright there leans this way in the scenario's frame.
    void AxesAt(double east_m, double north_m, Vec3& east, Vec3& north, Vec3& up) const {
        const GeoPoint g = Geodetic(map_.EnuToEcef({east_m, north_m, 0.0}));
        const double sl = std::sin(g.lat_rad), cl = std::cos(g.lat_rad), so = std::sin(g.lon_rad), co = std::cos(g.lon_rad);
        east = Rotate({-so, co, 0.0});
        north = Rotate({-sl * co, -sl * so, cl});
        up = Rotate({cl * co, cl * so, sl});
    }

    const LocalFrame& MapFrame() const { return map_; }
    const LocalFrame& Frame() const { return frame_; }

private:
    Vec3 Rotate(const Vec3& ecef_dir) const { return frame_.EcefToEnu(frame_ecef_ + ecef_dir); }

    static GeoPoint Geodetic(const Vec3& p) {
        const double a = wgs84::kSemiMajorAxis, b = wgs84::kSemiMinorAxis, e2 = wgs84::kEccentricitySq;
        const double ep2 = (a * a - b * b) / (b * b);
        const double q = std::sqrt(p.x * p.x + p.y * p.y);
        const double th = std::atan2(p.z * a, q * b);
        const double st = std::sin(th), ct = std::cos(th);
        GeoPoint g;
        g.lat_rad = std::atan2(p.z + ep2 * b * st * st * st, q - e2 * a * ct * ct * ct);
        g.lon_rad = std::atan2(p.y, p.x);
        const double sl = std::sin(g.lat_rad);
        const double n = a / std::sqrt(1.0 - e2 * sl * sl);
        g.alt_m = q / std::cos(g.lat_rad) - n;
        return g;
    }

    LocalFrame map_, frame_;
    Vec3 frame_ecef_;
    double off_e_ = 0.0, off_n_ = 0.0, a_ = 1.0, b_ = 0.0, c_ = 0.0, d_ = 1.0;
};

// The map's ground as a scenario's terrain: heights above sea level under the scenario's (east, north).
class MapTerrain final : public Terrain {
public:
    MapTerrain(std::shared_ptr<const MapData> map, const GeoPoint& frame_origin)
        : map_(std::move(map)), link_(map_->Def().origin, frame_origin) {}

    double HeightAt(double east_m, double north_m) const override {
        const MapPoint p = link_.FrameToMap(east_m, north_m);
        return map_->GroundHeight(p.east_m, p.north_m);
    }
    double ObstacleHeightAt(double east_m, double north_m) const {
        const MapPoint p = link_.FrameToMap(east_m, north_m);
        return map_->ObstacleHeight(p.east_m, p.north_m);
    }

    const MapData& Map() const { return *map_; }
    const std::shared_ptr<const MapData>& MapPtr() const { return map_; }
    const MapFrameLink& Link() const { return link_; }

private:
    std::shared_ptr<const MapData> map_;
    MapFrameLink link_;
};

// ---------------------------------------------------------------------------------------------- loading

namespace detail {

class MapLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, MapDef& out) {
        if (!Keys(root, "", {"map_id", "name", "description", "status", "source", "confidence", "notes", "origin", "seed", "extent_km",
                             "far_km", "far_ground", "airspace", "terrain", "rivers", "roads", "railways", "settlements", "farmsteads",
                             "forests", "woods", "marshes", "lakes", "sites"}) ||
            !RequireString(root, "", "map_id", out.map_id) || !RequireString(root, "", "name", out.name) ||
            !OptionalString(root, "", "description", out.description) || !OptionalString(root, "", "far_ground", out.far_ground_file) ||
            !Provenance(root, "") || !OriginAndSeed(root, out)) {
            return false;
        }
        const json::Value* ext = root.Find("extent_km");
        if (ext == nullptr) return Fail("extent_km", "required");
        double w = 0.0, e = 0.0, s = 0.0, n = 0.0;
        if (!Keys(*ext, "extent_km", {"west", "east", "south", "north"}) || !Number(*ext, "extent_km", "west", w) ||
            !Number(*ext, "extent_km", "east", e) || !Number(*ext, "extent_km", "south", s) || !Number(*ext, "extent_km", "north", n)) {
            return false;
        }
        if (e <= w || n <= s) return Fail("extent_km", "east must be beyond west and north beyond south");
        out.west_m = w * 1000.0;
        out.east_m = e * 1000.0;
        out.south_m = s * 1000.0;
        out.north_m = n * 1000.0;
        double far_km = 150.0;
        if (!OptionalNumber(root, "", "far_km", far_km)) return false;
        out.far_m = far_km * 1000.0;
        if (out.far_m < std::max({std::fabs(out.west_m), out.east_m, std::fabs(out.south_m), out.north_m})) {
            return Fail("far_km", "the far ground must reach beyond the map");
        }
        if (const json::Value* a = root.Find("airspace")) {
            double r = out.airspace_radius_m / 1000.0;
            if (!Keys(*a, "airspace", {"radius_km", "ceiling_m", "notes"}) || !OptionalNumber(*a, "airspace", "radius_km", r) ||
                !OptionalNumber(*a, "airspace", "ceiling_m", out.airspace_ceiling_m)) {
                return false;
            }
            out.airspace_radius_m = r * 1000.0;
        }
        return LoadTerrain(root, out) && LoadLines(root, "rivers", "river", out.rivers) && LoadLines(root, "roads", "", out.roads) &&
               LoadLines(root, "railways", "rail", out.railways) && LoadSettlements(root, out) && LoadFarmsteads(root, out) &&
               LoadPolygons(root, "forests", out.forests) && LoadPolygons(root, "marshes", out.marshes) && LoadLakes(root, out.lakes) &&
               LoadWoods(root, out) &&
               LoadSites(root, out);
    }

    // The ground another file shapes (a country's, named as a district's "far_ground"): its origin, seed, terrain, rivers
    // and lakes. The rest of that file is its own loader's business.
    bool LoadGround(const json::Value& root, MapDef& out) {
        return OriginAndSeed(root, out) && LoadTerrain(root, out) && LoadLines(root, "rivers", "river", out.rivers) &&
               LoadLakes(root, out.lakes);
    }

protected:  // the country map's loader (simcore/country_map.hpp) reads its terrain, lines and polygons the same way
    bool OriginAndSeed(const json::Value& root, MapDef& out) {
        const json::Value* o = root.Find("origin");
        if (o == nullptr) return Fail("origin", "required");
        double lat = 0.0, lon = 0.0, alt = 0.0;
        if (!Keys(*o, "origin", {"lat_deg", "lon_deg", "alt_m"}) || !Number(*o, "origin", "lat_deg", lat) ||
            !Number(*o, "origin", "lon_deg", lon) || !Number(*o, "origin", "alt_m", alt)) {
            return false;
        }
        out.origin = GeoPoint::FromDegrees(lat, lon, alt);
        double seed = 1.0;
        if (!OptionalNumber(root, "", "seed", seed)) return false;
        out.seed = static_cast<std::uint64_t>(std::max(0.0, seed));
        return true;
    }

    bool Number(const json::Value& o, const std::string& path, const char* key, double& v) {
        if (o.Find(key) == nullptr) return Fail(Join(path, key), "required");
        return OptionalNumber(o, path, key, v);
    }

    bool Point(const json::Value& v, const std::string& path, MapPoint& out) {
        if (!v.Is(json::Type::Array) || v.items.size() != 2 || !v.items[0].Is(json::Type::Number) || !v.items[1].Is(json::Type::Number)) {
            return Fail(path, "expected [east_km, north_km]");
        }
        out = {v.items[0].number * 1000.0, v.items[1].number * 1000.0};
        return true;
    }

    bool Points(const json::Value& obj, const std::string& path, std::size_t min_count, std::vector<MapPoint>& out) {
        const json::Value* pts = obj.Find("points");
        if (pts == nullptr || !pts->Is(json::Type::Array) || pts->items.size() < min_count) {
            return Fail(Join(path, "points"), "needs at least " + std::to_string(min_count) + " points");
        }
        for (std::size_t i = 0; i < pts->items.size(); ++i) {
            MapPoint p;
            if (!Point(pts->items[i], Index(Join(path, "points"), i), p)) return false;
            out.push_back(p);
        }
        return true;
    }

    bool Centre(const json::Value& obj, const std::string& path, MapPoint& out) {
        double e = 0.0, n = 0.0;
        if (!Number(obj, path, "east_km", e) || !Number(obj, path, "north_km", n)) return false;
        out = {e * 1000.0, n * 1000.0};
        return true;
    }

    bool LoadTerrain(const json::Value& root, MapDef& out) {
        const json::Value* t = root.Find("terrain");
        if (t == nullptr) return Fail("terrain", "required");
        if (!Keys(*t, "terrain", {"base_m", "core_spacing_m", "far_spacing_m", "roughness_m", "relief", "hills", "ridges", "valleys", "valley",
                                  "notes"}) ||
            !Number(*t, "terrain", "base_m", out.base_m) || !OptionalNumber(*t, "terrain", "core_spacing_m", out.core_spacing_m) ||
            !OptionalNumber(*t, "terrain", "far_spacing_m", out.far_spacing_m) || !OptionalNumber(*t, "terrain", "roughness_m", out.roughness_m)) {
            return false;
        }
        if (out.core_spacing_m < 10.0 || out.far_spacing_m < out.core_spacing_m) {
            return Fail("terrain", "needs 10 m <= core_spacing_m <= far_spacing_m");
        }
        if (const json::Value* rel = OptionalArray(*t, "terrain", "relief")) {
            for (std::size_t i = 0; i < rel->items.size(); ++i) {
                const std::string p = Index("terrain.relief", i);
                MapReliefDef r;
                double wl = 1.0;
                if (!Keys(rel->items[i], p, {"wavelength_km", "amplitude_m"}) || !Number(rel->items[i], p, "wavelength_km", wl) ||
                    !Number(rel->items[i], p, "amplitude_m", r.amplitude_m)) {
                    return false;
                }
                if (wl <= 0.0) return Fail(Join(p, "wavelength_km"), "must be > 0");
                r.wavelength_m = wl * 1000.0;
                out.relief.push_back(r);
            }
        } else if (!Message().empty()) {
            return false;
        }
        if (const json::Value* hs = OptionalArray(*t, "terrain", "hills")) {
            for (std::size_t i = 0; i < hs->items.size(); ++i) {
                const std::string p = Index("terrain.hills", i);
                const json::Value& h = hs->items[i];
                MapHillDef hill;
                double r = 1.0;
                if (!Keys(h, p, {"id", "east_km", "north_km", "radius_km", "height_m", "notes"}) || !RequireString(h, p, "id", hill.id) ||
                    !Centre(h, p, hill.centre) || !Number(h, p, "radius_km", r) || !Number(h, p, "height_m", hill.height_m)) {
                    return false;
                }
                if (r <= 0.0) return Fail(Join(p, "radius_km"), "must be > 0");
                hill.radius_m = r * 1000.0;
                out.hills.push_back(hill);
            }
        } else if (!Message().empty()) {
            return false;
        }
        if (!Ridges(*t, "ridges", "height_m", out.ridges) || !Ridges(*t, "valleys", "depth_m", out.valleys)) return false;
        if (const json::Value* v = t->Find("valley")) {
            if (!Keys(*v, "terrain.valley", {"depth_m", "half_width_m", "notes"}) || !Number(*v, "terrain.valley", "depth_m", out.valley_depth_m) ||
                !Number(*v, "terrain.valley", "half_width_m", out.valley_half_width_m)) {
                return false;
            }
            if (out.valley_half_width_m <= 0.0) return Fail("terrain.valley.half_width_m", "must be > 0");
        }
        return true;
    }

    bool LoadLines(const json::Value& root, const char* key, const char* fixed_class, std::vector<MapLineDef>& out) {
        const json::Value* list = OptionalArray(root, "", key);
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index(key, i);
            const json::Value& l = list->items[i];
            MapLineDef line;
            if (!Keys(l, p, {"id", "name", "class", "width_m", "points", "notes"}) || !RequireString(l, p, "id", line.id) ||
                !OptionalString(l, p, "name", line.name) || !Points(l, p, 2, line.points)) {
                return false;
            }
            if (fixed_class[0] != '\0') {
                line.line_class = fixed_class;
                line.width_m = line.line_class == "rail" ? 5.0 : (line.line_class == "oil" ? 1.0 : 60.0);
            } else {
                if (!RequireString(l, p, "class", line.line_class)) return false;
                if (line.line_class == "main") line.width_m = 9.0;
                else if (line.line_class == "secondary") line.width_m = 6.5;
                else if (line.line_class == "track") line.width_m = 4.0;
                else return Fail(Join(p, "class"), "must be main, secondary or track");
            }
            if (!OptionalNumber(l, p, "width_m", line.width_m)) return false;
            if (line.width_m <= 0.0 || line.width_m > 400.0) return Fail(Join(p, "width_m"), "must be between 0 and 400 m");
            for (const MapLineDef& o : out) {
                if (o.id == line.id) return Fail(Join(p, "id"), "'" + line.id + "' is listed twice");
            }
            out.push_back(std::move(line));
        }
        return true;
    }

    bool LoadSettlements(const json::Value& root, MapDef& out) {
        const json::Value* list = OptionalArray(root, "", "settlements");
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index("settlements", i);
            const json::Value& v = list->items[i];
            MapSettlementDef s;
            double r = 0.5, heading = 0.0;
            if (!Keys(v, p, {"id", "name", "kind", "east_km", "north_km", "radius_km", "buildings", "heading_deg", "notes"}) ||
                !RequireString(v, p, "id", s.id) || !RequireString(v, p, "name", s.name) || !RequireString(v, p, "kind", s.kind) ||
                !Centre(v, p, s.centre) || !Number(v, p, "radius_km", r) || !OptionalInt(v, p, "buildings", s.buildings) ||
                !OptionalNumber(v, p, "heading_deg", heading)) {
                return false;
            }
            if (s.kind != "village" && s.kind != "town" && s.kind != "city") return Fail(Join(p, "kind"), "must be village, town or city");
            if (r <= 0.0 || s.buildings < 0) return Fail(p, "needs a radius > 0 and buildings >= 0");
            s.radius_m = r * 1000.0;
            s.heading_rad = DegToRad(heading);
            out.settlements.push_back(std::move(s));
        }
        return true;
    }

    bool LoadFarmsteads(const json::Value& root, MapDef& out) {
        const json::Value* f = root.Find("farmsteads");
        if (f == nullptr) return true;
        if (!Keys(*f, "farmsteads", {"count", "buildings", "notes"}) || !OptionalInt(*f, "farmsteads", "count", out.farmsteads)) return false;
        if (const json::Value* b = f->Find("buildings")) {
            if (!b->Is(json::Type::Array) || b->items.size() != 2 || !b->items[0].Is(json::Type::Number) || !b->items[1].Is(json::Type::Number)) {
                return Fail("farmsteads.buildings", "expected [min, max]");
            }
            out.farm_buildings_min = static_cast<int>(b->items[0].number);
            out.farm_buildings_max = static_cast<int>(b->items[1].number);
            if (out.farm_buildings_min < 1 || out.farm_buildings_max < out.farm_buildings_min) return Fail("farmsteads.buildings", "needs 1 <= min <= max");
        }
        return true;
    }

    // terrain.ridges or terrain.valleys: lines with a half width and a height (a valley's depth), both > 0.
    bool Ridges(const json::Value& t, const char* key, const char* height_key, std::vector<MapRidgeDef>& out) {
        const json::Value* list = OptionalArray(t, "terrain", key);
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index(std::string("terrain.") + key, i);
            const json::Value& v = list->items[i];
            MapRidgeDef r;
            double hw = 0.0;
            if (!Keys(v, p, {"id", "points", "half_width_km", height_key, "notes"}) || !RequireString(v, p, "id", r.id) ||
                !Points(v, p, 2, r.points) || !Number(v, p, "half_width_km", hw) || !Number(v, p, height_key, r.height_m)) {
                return false;
            }
            if (hw <= 0.0) return Fail(Join(p, "half_width_km"), "must be > 0");
            if (r.height_m <= 0.0) return Fail(Join(p, height_key), "must be > 0");
            r.half_width_m = hw * 1000.0;
            out.push_back(std::move(r));
        }
        return true;
    }

    bool LoadLakes(const json::Value& root, std::vector<MapLakeDef>& out) {
        const json::Value* list = OptionalArray(root, "", "lakes");
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index("lakes", i);
            const json::Value& v = list->items[i];
            MapLakeDef l;
            if (!Keys(v, p, {"id", "name", "points", "level_m", "depth_m", "notes"}) || !RequireString(v, p, "id", l.id) ||
                !OptionalString(v, p, "name", l.name) || !Points(v, p, 3, l.points) || !Number(v, p, "level_m", l.level_m) ||
                !OptionalNumber(v, p, "depth_m", l.depth_m)) {
                return false;
            }
            if (l.depth_m <= 0.0) return Fail(Join(p, "depth_m"), "must be > 0");
            out.push_back(std::move(l));
        }
        return true;
    }

    bool LoadPolygons(const json::Value& root, const char* key, std::vector<MapPolygonDef>& out) {
        const json::Value* list = OptionalArray(root, "", key);
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index(key, i);
            MapPolygonDef poly;
            if (!Keys(list->items[i], p, {"id", "name", "points", "notes"}) || !RequireString(list->items[i], p, "id", poly.id) ||
                !OptionalString(list->items[i], p, "name", poly.name) || !Points(list->items[i], p, 3, poly.points)) {
                return false;
            }
            out.push_back(std::move(poly));
        }
        return true;
    }

    bool Pair(const json::Value& obj, const std::string& path, const char* key, double& lo, double& hi) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return true;
        if (!v->Is(json::Type::Array) || v->items.size() != 2 || !v->items[0].Is(json::Type::Number) || !v->items[1].Is(json::Type::Number) ||
            v->items[1].number < v->items[0].number) {
            return Fail(Join(path, key), "expected [low, high]");
        }
        lo = v->items[0].number;
        hi = v->items[1].number;
        return true;
    }

    bool LoadWoods(const json::Value& root, MapDef& out) {
        const json::Value* w = root.Find("woods");
        if (w == nullptr) return true;
        if (!Keys(*w, "woods", {"block_m", "far_block_m", "canopy_m", "scatter", "far_scatter", "riverside", "notes"}) ||
            !OptionalNumber(*w, "woods", "block_m", out.block_m) || !OptionalNumber(*w, "woods", "far_block_m", out.far_block_m) ||
            !Pair(*w, "woods", "canopy_m", out.canopy_min_m, out.canopy_max_m)) {
            return false;
        }
        if (out.block_m < 20.0 || out.far_block_m < out.block_m) return Fail("woods", "needs 20 m <= block_m <= far_block_m");
        if (const json::Value* s = w->Find("scatter")) {
            if (!Keys(*s, "woods.scatter", {"count", "radius_m"}) || !OptionalInt(*s, "woods.scatter", "count", out.scatter_count) ||
                !Pair(*s, "woods.scatter", "radius_m", out.scatter_min_m, out.scatter_max_m)) {
                return false;
            }
        }
        if (const json::Value* s = w->Find("far_scatter")) {
            if (!Keys(*s, "woods.far_scatter", {"count", "radius_m"}) || !OptionalInt(*s, "woods.far_scatter", "count", out.far_scatter_count) ||
                !Pair(*s, "woods.far_scatter", "radius_m", out.far_scatter_min_m, out.far_scatter_max_m)) {
                return false;
            }
        }
        if (const json::Value* s = w->Find("riverside")) {
            if (!Keys(*s, "woods.riverside", {"band_m", "share"}) || !Pair(*s, "woods.riverside", "band_m", out.riverside_min_m, out.riverside_max_m) ||
                !OptionalNumber(*s, "woods.riverside", "share", out.riverside_share)) {
                return false;
            }
        }
        return true;
    }

    bool Runway(const json::Value& obj, const std::string& path, MapSiteDef& s) {
        const json::Value* v = obj.Find("runway_m");
        if (v == nullptr) return true;
        if (!v->Is(json::Type::Array) || v->items.size() != 2 || !v->items[0].Is(json::Type::Number) || !v->items[1].Is(json::Type::Number) ||
            v->items[0].number < 500.0 || v->items[1].number < 10.0 || v->items[1].number > v->items[0].number) {
            return Fail(Join(path, "runway_m"), "expected [length >= 500, width >= 10]");
        }
        s.runway_length_m = v->items[0].number;
        s.runway_width_m = v->items[1].number;
        return true;
    }

    bool LoadSites(const json::Value& root, MapDef& out) {
        const json::Value* list = OptionalArray(root, "", "sites");
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index("sites", i);
            const json::Value& v = list->items[i];
            MapSiteDef s;
            double heading = 0.0;
            if (!Keys(v, p, {"id", "name", "kind", "east_km", "north_km", "heading_deg", "count", "runway_m", "vehicles_km", "notes"}) ||
                !RequireString(v, p, "id", s.id) || !RequireString(v, p, "name", s.name) || !RequireString(v, p, "kind", s.kind) ||
                !Centre(v, p, s.centre) || !OptionalNumber(v, p, "heading_deg", heading) || !OptionalInt(v, p, "count", s.count) ||
                !Runway(v, p, s)) {
                return false;
            }
            s.heading_rad = DegToRad(heading);
            bool known = false;
            for (const std::string& k : MapSiteKinds()) known = known || k == s.kind;
            if (!known) return Fail(Join(p, "kind"), "unknown site kind '" + s.kind + "'");
            if (const json::Value* vs = OptionalArray(v, p, "vehicles_km")) {
                for (std::size_t k = 0; k < vs->items.size(); ++k) {
                    MapPoint pt;
                    if (!Point(vs->items[k], Index(Join(p, "vehicles_km"), k), pt)) return false;
                    s.vehicles.push_back(pt);
                }
            } else if (!Message().empty()) {
                return false;
            }
            for (const MapSiteDef& o : out.sites) {
                if (o.id == s.id) return Fail(Join(p, "id"), "'" + s.id + "' is listed twice");
            }
            out.sites.push_back(std::move(s));
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadMapDefJson(const std::string& text, MapDef& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) {
        if (error) *error = "map: " + parse_error;
        return false;
    }
    detail::MapLoader loader;
    if (!loader.Load(root, out)) {
        if (error) *error = "map: " + loader.Message();
        return false;
    }
    return true;
}

// Loads and builds data/<map_file> (e.g. "maps/kesmark_range.json").
inline std::shared_ptr<const MapData> LoadMap(const std::string& data_dir, const std::string& map_file, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(data_dir + "/" + map_file, text)) {
        if (error) *error = "cannot open " + map_file;
        return nullptr;
    }
    MapDef def;
    if (!LoadMapDefJson(text, def, error)) return nullptr;
    if (!def.far_ground_file.empty()) {
        // A district: its far ground is its country's (the same frame, so one ground meets the other at the map's edge).
        std::string far_text;
        json::Value root;
        std::string parse_error;
        if (!detail::ReadTextFile(data_dir + "/" + def.far_ground_file, far_text) || !json::Parse(far_text, root, &parse_error)) {
            if (error) *error = "map: far_ground: cannot read " + def.far_ground_file + (parse_error.empty() ? "" : " (" + parse_error + ")");
            return nullptr;
        }
        auto ground = std::make_shared<MapDef>();
        detail::MapLoader loader;
        if (!loader.LoadGround(root, *ground)) {
            if (error) *error = "map: far_ground " + def.far_ground_file + ": " + loader.Message();
            return nullptr;
        }
        if (std::fabs(ground->origin.LatDeg() - def.origin.LatDeg()) > 1e-9 || std::fabs(ground->origin.LonDeg() - def.origin.LonDeg()) > 1e-9) {
            if (error) *error = "map: far_ground " + def.far_ground_file + ": its origin must be the map's";
            return nullptr;
        }
        def.far_ground = std::move(ground);
    }
    return MapData::Build(std::move(def));
}

// LoadMap, built once while anyone holds it: every scenario on the map (the missions menu checks them all) shares one.
inline std::shared_ptr<const MapData> LoadMapCached(const std::string& data_dir, const std::string& map_file, std::string* error = nullptr) {
    static std::mutex lock;
    static std::map<std::string, std::weak_ptr<const MapData>> cache;
    const std::string key = data_dir + "/" + map_file;
    std::lock_guard<std::mutex> guard(lock);
    if (std::shared_ptr<const MapData> held = cache[key].lock()) return held;
    std::shared_ptr<const MapData> built = LoadMap(data_dir, map_file, error);
    if (built) cache[key] = built;
    return built;
}

}  // namespace simcore
