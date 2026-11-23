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
//
// One ground (roadmap R5). The ground is a few lattices, finest first (MapData::DrawnLevels: the map proper, the far
// ground, the country's 1 km grid, the 4 km land beyond it), and each is sampled on the triangles the game draws (each
// cell split along its (c+1, r)-(c, r+1) diagonal: MapGrid::SampleTri, Unreal's SimMapActor), so a height the sim
// takes is a height on the drawn mesh. The game places every vertex by MapFrameLink::GroundPoint (the point of the
// scenario's frame straight under the map's east and north, at the height), the vertical the sim uses (geo.hpp
// GroundPointInFrame). DrawnGroundAgrees checks the two.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "simcore/clutter.hpp"
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

// A detail area (roadmap R6): a square of finer ground where play happens close up, one more drawn level of the map's
// ground (MapData::DrawnLevels). Its edges fall on the nodes of the level it lies in (the map proper's or the far
// ground's), so a coarse cell is wholly inside it (not drawn) or wholly outside. Its ground is the coarse level's
// triangles at its own spacing plus a little micro relief, faded to nothing at its edge, along lines, on sites and in
// towns; its obstacle heights at a finer spacing.
struct MapDetailDef {
    std::string id;
    MapPoint centre;
    double half_m = 5000.0;   // half its side
    double spacing_m = 10.0;  // its ground's
    double micro_m = 0.0;     // the micro relief's amplitude
    bool Contains(double e, double n) const { return std::fabs(e - centre.east_m) <= half_m && std::fabs(n - centre.north_m) <= half_m; }
};

// Representative clutter heights for the far ground's towns the map does not lay out (ITU-R P.1812 section 3.2 Table 2,
// simcore/clutter.hpp; data/gameplay/clutter.json, read by LoadMap): dense urban within `dense_share` of a town's radius,
// suburban beyond (TownClutterClass). 0: none.
using MapClutterDef = ClutterHeights;

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
    // Towns laid out in the detail areas (roadmap R6, "detail_settlements"): on their own random stream, after the rest,
    // so the map proper's layout is the same with or without them; their streets along the country's roads there.
    std::vector<MapSettlementDef> detail_settlements;
    std::vector<MapDetailDef> detail;  // the detail areas (terrain.detail)
    MapClutterDef clutter;
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
    // The country's grid spacing (its "grid_km"), as the country is drawn beyond the far ground (with far_ground: its
    // own extent is far_ground's west_m .. north_m).
    double far_grid_m = 1000.0;

    bool InCore(double e, double n) const { return e >= west_m && e <= east_m && n >= south_m && n <= north_m; }
    // The detail area holding (east, north), or null.
    const MapDetailDef* DetailAt(double e, double n) const {
        for (const MapDetailDef& d : detail) {
            if (d.Contains(e, n)) return &d;
        }
        return nullptr;
    }
    bool InDetail(double e, double n) const { return DetailAt(e, n) != nullptr; }
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

// A square grid of heights (row 0 southmost, column 0 westmost): bilinear between samples (Sample), or on the two
// triangles each cell is drawn as (SampleTri: the ground).
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
    // The cell (c, r) holding (east, north) (clamped onto the grid; its corners c..c+1, r..r+1).
    void CellOf(double e, double n, int& c, int& r) const {
        const double fx = Clamp((e - west_m) / spacing_m, 0.0, static_cast<double>(cols - 1));
        const double fy = Clamp((n - south_m) / spacing_m, 0.0, static_cast<double>(rows - 1));
        c = std::min(static_cast<int>(fx), cols - 2);
        r = std::min(static_cast<int>(fy), rows - 2);
    }
    // The height on the triangle of its cell, as the game draws it. The contract with Unreal's SimMapActor: a cell's
    // corners A (c, r), B (c+1, r), C (c, r+1), D (c+1, r+1) make the triangles {A, B, C} and {B, D, C}, split along the
    // diagonal B-C (from (c+1, r) to (c, r+1)).
    double SampleTri(double e, double n) const {
        int x0 = 0, y0 = 0;
        CellOf(e, n, x0, y0);
        const double tx = Clamp((e - west_m) / spacing_m - x0, 0.0, 1.0), ty = Clamp((n - south_m) / spacing_m - y0, 0.0, 1.0);
        if (tx + ty <= 1.0) {
            const double h00 = At(x0, y0);
            return h00 + (At(x0 + 1, y0) - h00) * tx + (At(x0, y0 + 1) - h00) * ty;
        }
        const double h11 = At(x0 + 1, y0 + 1);
        return h11 + (At(x0, y0 + 1) - h11) * (1.0 - tx) + (At(x0 + 1, y0) - h11) * (1.0 - ty);
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

    // How far (east, north) lies from the nearest segment's edge (0 on it), up to `cap_m` (under half a cell).
    double EdgeDistance(double e, double n, double cap_m) const {
        double best = cap_m;
        const long long cx = Cell(e), cy = Cell(n);
        for (long long y = cy - 1; y <= cy + 1; ++y) {
            for (long long x = cx - 1; x <= cx + 1; ++x) {
                const auto it = buckets_.find(Key(x, y));
                if (it == buckets_.end()) continue;
                for (std::size_t i : it->second) {
                    const Seg& s = segs_[i];
                    const double d = DistToSegment(e, n, s.a.east_m, s.a.north_m, s.b.east_m, s.b.north_m) - s.half_width;
                    best = std::min(best, std::max(0.0, d));
                }
            }
        }
        return best;
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
    // A circle up to a cell across is filed by its centre (Free's reach finds it); a wider one (a site's clearance) in
    // every bucket its square covers, so a query far from its centre but inside it still meets it.
    void Take(double e, double n, double radius_m) {
        if (radius_m <= cell_) {
            buckets_[Cell(e) * 1000003LL + Cell(n)].push_back({e, n, radius_m});
            return;
        }
        for (long long y = Cell(n - radius_m); y <= Cell(n + radius_m); ++y) {
            for (long long x = Cell(e - radius_m); x <= Cell(e + radius_m); ++x) buckets_[x * 1000003LL + y].push_back({e, n, radius_m});
        }
    }

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
// The bed reaches this far beyond the water's edge, shallowing to nothing (a game value).
constexpr double kRiverBedBankM = 15.0;

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
        const double bed = 0.5 * r.width_m + kRiverBedBankM;
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
    // Bakes the ground and lays the map out. Takes a second or two for a 40 km map with its detail areas. The order
    // matters: the detail areas' ground fades its micro relief off the lines (the indices, with the country's lines
    // imported into the areas), and the map proper is laid before the detail areas' towns, which draw on their own
    // random streams, so the map proper's layout does not depend on them.
    static std::shared_ptr<const MapData> Build(MapDef def) {
        std::shared_ptr<MapData> m(new MapData(std::move(def)));
        m->Bake();
        m->SetLevels(false);
        m->ImportDetailLines();
        m->BuildIndices();
        m->BuildWaterGrid();
        m->BakeDetail();
        m->Lay();
        m->LayDetailSettlements();
        m->BakeObstacles();
        return m;
    }
    MapData(const MapData&) = delete;             // DrawnLevels points into it
    MapData& operator=(const MapData&) = delete;

    const MapDef& Def() const { return def_; }
    const MapLayout& Layout() const { return layout_; }
    const MapGrid& CoreGrid() const { return core_; }
    const MapGrid& FarGrid() const { return far_; }
    // With a country's far ground (far_ground): its ground at its own grid spacing over its extent, and the land beyond
    // its borders at 4 km, 100 km out (the lattices the game draws there; empty without far_ground).
    const MapGrid& CountryGrid() const { return country_; }
    const MapGrid& BeyondGrid() const { return beyond_; }
    const MapGrid& ObstacleGrid() const { return obstacles_; }
    // The detail areas' ground and obstacle grids, in the order of Def().detail (roadmap R6).
    const std::vector<MapGrid>& DetailGrids() const { return detail_ground_; }
    const std::vector<MapGrid>& DetailObstacleGrids() const { return detail_obstacles_; }
    bool InDetail(double e, double n) const { return def_.InDetail(e, n); }
    // The country's roads, railways and rivers through the detail areas beyond the map proper, clipped to each area and
    // a kilometre round it (the country's ids; laid as strips and bridges, and kept clear of by the areas' towns). Not
    // in Def(): the route service and NearestRoad keep the country's own lines.
    struct DetailLines {
        std::vector<MapLineDef> roads, railways, rivers;
    };
    const DetailLines& ImportedLines() const { return detail_lines_; }
    // Whether the map lays the country's lines at (east, north) as its own (roadmap S9): in the map proper, in a detail
    // area, or in the kilometre round a detail area beyond the map proper that the country's lines are imported into
    // (ImportDetailLines). A drawing of the country's own lines leaves these places out, so no line is drawn twice.
    bool LaysCountryLinesAt(double e, double n) const {
        if (def_.InCore(e, n) || def_.InDetail(e, n)) return true;
        for (const MapDetailDef& a : def_.detail) {
            double w = 0.0, s = 0.0, ea = 0.0, nn = 0.0;
            if (ImportBox(a, w, s, ea, nn) && e >= w && e <= ea && n >= s && n <= nn) return true;
        }
        return false;
    }
    // The levelled sites (a centre, a radius, the height levelled to): what the ground is flattened for.
    struct Flat {
        double e, n, r, h;
    };
    const std::vector<Flat>& Flats() const { return flats_; }
    // Whether (east, north) lies within `clear_m` (under 125 m) of the edge of a road, railway or river (the map's and the
    // detail areas' imported ones), or of a river's: what the map's dressing keeps its trees off (scenery/map_dressing.hpp).
    bool NearLine(double e, double n, double clear_m) const { return lines_.Near(e, n, clear_m); }
    bool NearWater(double e, double n, double clear_m) const { return water_.Near(e, n, clear_m); }
    // Whether (east, north) is water (roadmap E9's sea areas; Terrain::WaterAt): on a river of the map or its detail
    // areas within half its width, in one of the map's lakes, or beyond the map proper on its country's lakes and rivers.
    bool WaterAt(double e, double n) const { return has_water_ && water_grid_.Maybe(e, n) && WaterAtExact(e, n); }
    // The same without the grid's quick no (what the grid is tested against).
    bool WaterAtExact(double e, double n) const {
        if (!has_water_) return false;
        if (water_.Near(e, n, 0.0)) return true;
        const bool beyond = def_.far_ground != nullptr && !def_.InCore(e, n);
        for (const LakeBox& b : lake_boxes_) {
            if ((b.country && !beyond) || e < b.w || e > b.e || n < b.s || n > b.n) continue;
            const MapLakeDef& l = b.country ? def_.far_ground->lakes[b.index] : def_.lakes[b.index];
            if (mapgeo::InPolygon(l.points, e, n)) return true;
        }
        return beyond && far_water_.Near(e, n, 0.0);
    }
    bool HasWater() const { return has_water_; }

    // The ground's lattices as they are drawn, finest first: the detail areas (roadmap R6), the map proper, the far
    // ground, the country's, the land beyond (those it has). A coarser level leaves out only the cells lying wholly
    // inside a finer level (DrawsCell), so no point is left without ground; the ground at a point is the finest level
    // covering it (LevelAt), on that cell's triangles. Every level's edges fall on the next coarser level's nodes (a
    // detail area's on the map proper's or the far ground's, the country's lattice grown to the 4 km beyond's), so a
    // coarse cell is wholly inside a finer level or wholly outside it, and every point of the ground is drawn by
    // exactly one cell (NoGroundDrawnTwice).
    // A finer level of detail anywhere is a level added here, never in the game alone.
    std::vector<const MapGrid*> DrawnLevels() const { return levels_; }
    // Whether level `level` of DrawnLevels draws its cell (c, r) (corners c..c+1, r..r+1): not wholly inside a finer one.
    bool DrawsCell(std::size_t level, int c, int r) const {
        if (level >= levels_.size()) return false;
        const MapGrid& g = *levels_[level];
        const double w = g.EastOf(c), s = g.NorthOf(r), e = g.EastOf(c + 1), n = g.NorthOf(r + 1);
        for (std::size_t j = 0; j < level; ++j) {
            const MapGrid& f = *levels_[j];
            if (f.Covers(w, s) && f.Covers(e, s) && f.Covers(w, n) && f.Covers(e, n)) return false;
        }
        return true;
    }
    // The level of DrawnLevels the ground at (east, north) is drawn by (-1: none, beyond every lattice: Analytic).
    int LevelAt(double e, double n) const {
        int index = -1;
        LevelGridAt(e, n, &index);
        return index;
    }

    // The ground's height above sea level at (east, north) of the map frame: on the triangles of the lattice drawn
    // there (LevelAt), the analytic ground beyond every lattice.
    double GroundHeight(double e, double n) const {
        const MapGrid* g = LevelGridAt(e, n, nullptr);
        return g != nullptr ? g->SampleTri(e, n) : Analytic(e, n);
    }

    // How high woods and buildings stand above the ground (roadmap E9): the tallest box whose footprint holds (east,
    // north), as it is laid (roadmap R6: no wider; a building's edge is its edge), anywhere on the map; beyond the map
    // proper and the detail areas where no box stands, in the far ground's towns the map does not lay out, a
    // representative clutter height (MapClutterDef).
    double ObstacleHeight(double e, double n) const {
        const double top = ObstacleTop(e, n);
        return top > Terrain::kNoObstacleTop ? std::max(0.0, top - GroundHeight(e, n)) : 0.0;
    }
    // The top of what stands at (east, north), above sea level (Terrain::kNoObstacleTop: nothing).
    double ObstacleTop(double e, double n) const {
        double top = Terrain::kNoObstacleTop;
        const FootCells* cells = CellsAt(e, n);
        if (cells != nullptr) {
            const std::size_t i = cells->Cell(e, n);
            for (std::uint32_t k = cells->first[i]; k < cells->first[i + 1]; ++k) {
                const Foot& f = feet_[cells->feet[k]];
                if (f.top > top && f.Holds(e, n)) top = f.top;
            }
        } else {
            const auto it = loose_.find(CellKey(e, n, kLooseCellM));
            if (it != loose_.end()) {
                for (std::uint32_t k : it->second) {
                    const Foot& f = feet_[k];
                    if (f.top > top && f.Holds(e, n)) top = f.top;
                }
            }
        }
        if (top > Terrain::kNoObstacleTop) return top;
        const double clutter = cells == nullptr ? Clutter(e, n) : 0.0;
        return clutter > 0.0 ? GroundHeight(e, n) + clutter : Terrain::kNoObstacleTop;
    }
    // No box over the ground segment from (e0, n0) to (e1, n1) stands higher than this (above sea level;
    // Terrain::kNoObstacleTop: none there), and `spacing_m` is the narrowest of those boxes' sides (no less than
    // kFootSpacingFloorM): what a path looks at them at (Terrain::ObstacleTopBoundM). The bound is the tallest top in the
    // 500 m blocks the segment's bounds reach and, when that is not already under `below_m` and the segment is short,
    // in its finer cells. The clutter of the towns not laid out is not a footprint: a path's own samples carry it.
    double ObstacleTopBound(double e0, double n0, double e1, double n1, double below_m, double& spacing_m) const {
        spacing_m = 0.0;
        const double w = std::min(e0, e1), ea = std::max(e0, e1), s = std::min(n0, n1), nn = std::max(n0, n1);
        double top = Terrain::kNoObstacleTop, side = 1e300;
        if (blocks_.Overlaps(w, s, ea, nn)) blocks_.Bound(w, s, ea, nn, top, side);
        if (top <= Terrain::kNoObstacleTop) return top;
        if (top >= below_m && (ea - w) * (nn - s) <= kFineBoundAreaM2) {  // short: the finer cells' bound
            double fine_top = Terrain::kNoObstacleTop, fine_side = 1e300;
            for (const FootCells& c : rects_) {
                if (c.Overlaps(w, s, ea, nn)) c.Bound(w, s, ea, nn, fine_top, fine_side);
            }
            // Some of it beyond every dense grid: the loose cells too.
            if (CellsAt(w, s) == nullptr || CellsAt(w, nn) == nullptr || CellsAt(ea, s) == nullptr || CellsAt(ea, nn) == nullptr) {
                for (long long y = CellIndex(s, kLooseCellM); y <= CellIndex(nn, kLooseCellM); ++y) {
                    for (long long x = CellIndex(w, kLooseCellM); x <= CellIndex(ea, kLooseCellM); ++x) {
                        const auto it = loose_.find(PairKey(x, y));
                        if (it == loose_.end()) continue;
                        for (std::uint32_t k : it->second) {
                            fine_top = std::max(fine_top, feet_[k].top);
                            fine_side = std::min(fine_side, feet_[k].side);
                        }
                    }
                }
            }
            top = fine_top;
            side = fine_side;
        }
        if (top > Terrain::kNoObstacleTop) spacing_m = side;
        return top;
    }
    // Every box over the ground segment from (e0, n0) to (e1, n1) whose top is no lower than `below_m`, told to `sink`
    // with where the segment goes in and comes out of its footprint (Terrain::ObstacleChords; roadmap R6's review: a ray
    // cutting a corner is not stepped over). The segment walks every cell it passes through (a corner clipped too) in
    // each dense grid it reaches and, unless it lies in one, the loose cells; a box is told once per grid, in the cell
    // where the walk first meets it. The clutter of the towns not laid out is not a footprint.
    void ObstacleChords(double e0, double n0, double e1, double n1, double below_m, ObstacleChordSink& sink) const {
        const double de = e1 - e0, dn = n1 - n0;
        bool going = true;
        const auto tell = [&](std::uint32_t k, double cell_in, double cell_out, double walk_in) {
            const Foot& f = feet_[k];
            if (f.top < below_m) return;
            double t_in = 0.0, t_out = 1.0;
            if (!f.Chord(e0, n0, de, dn, t_in, t_out)) return;
            const double first = std::max(t_in, walk_in);  // where the walk first meets it
            if (first < cell_in - kWalkEps || first > cell_out + kWalkEps) return;
            going = sink.Chord(t_in, t_out, f.top);
        };
        const double w = std::min(e0, e1), s = std::min(n0, n1), ea = std::max(e0, e1), nn = std::max(n0, n1);
        bool inside_one = false;
        for (const FootCells& c : rects_) {
            if (!going) return;
            if (!c.Overlaps(w, s, ea, nn)) continue;
            inside_one = inside_one || (c.Covers(e0, n0) && c.Covers(e1, n1));
            double walk_in = 0.0;
            WalkCells(e0, n0, e1, n1, c.west_m, c.south_m, c.cell_m, c.cols, c.rows, walk_in,
                      [&](long long col, long long row, double t0, double t1) {
                          const std::size_t i = static_cast<std::size_t>(row) * static_cast<std::size_t>(c.cols) + static_cast<std::size_t>(col);
                          if (c.top[i] < below_m) return true;
                          for (std::uint32_t k = c.first[i]; k < c.first[i + 1] && going; ++k) tell(c.feet[k], t0, t1, walk_in);
                          return going;
                      });
        }
        if (inside_one || !going || loose_.empty()) return;
        double walk_in = 0.0;
        WalkCells(e0, n0, e1, n1, 0.0, 0.0, kLooseCellM, -1, -1, walk_in, [&](long long col, long long row, double t0, double t1) {
            const auto it = loose_.find(PairKey(col, row));
            if (it == loose_.end()) return true;
            for (std::size_t j = 0; j < it->second.size() && going; ++j) tell(it->second[j], t0, t1, walk_in);
            return going;
        });
    }
    // The tallest box (its top over the ground at its centre) whose footprint comes within `radius_m` of (east, north);
    // 0: none. A spot's clearance looked at whole (roadmap R6's review: a house between a few points is not missed).
    double ObstacleHeightWithin(double e, double n, double radius_m) const {
        double most = 0.0;
        const auto near_enough = [&](std::uint32_t k) {
            const Foot& f = feet_[k];
            const double de = e - f.e, dn = n - f.n;
            double d = 0.0;
            if (f.round) {
                d = std::max(0.0, std::hypot(de, dn) - f.half_l);
            } else {
                const double u = std::max(0.0, std::fabs(de * f.sin_h + dn * f.cos_h) - f.half_l);
                const double v = std::max(0.0, std::fabs(de * f.cos_h - dn * f.sin_h) - f.half_w);
                d = std::hypot(u, v);
            }
            if (d <= radius_m) most = std::max(most, f.top - GroundHeight(f.e, f.n));
        };
        const double w = e - radius_m, ea = e + radius_m, s = n - radius_m, nn = n + radius_m;
        for (const FootCells& c : rects_) {
            if (!c.Overlaps(w, s, ea, nn)) continue;
            for (int r = c.Row(s); r <= c.Row(nn); ++r) {
                for (int col = c.Col(w); col <= c.Col(ea); ++col) {
                    const std::size_t i = static_cast<std::size_t>(r) * static_cast<std::size_t>(c.cols) + static_cast<std::size_t>(col);
                    for (std::uint32_t k = c.first[i]; k < c.first[i + 1]; ++k) near_enough(c.feet[k]);
                }
            }
        }
        for (long long y = CellIndex(s, kLooseCellM); y <= CellIndex(nn, kLooseCellM); ++y) {
            for (long long x = CellIndex(w, kLooseCellM); x <= CellIndex(ea, kLooseCellM); ++x) {
                const auto it = loose_.find(PairKey(x, y));
                if (it == loose_.end()) continue;
                for (std::uint32_t k : it->second) near_enough(k);
            }
        }
        return most;
    }
    // The narrowest side of any box that blocks sight (no less than kFootSpacingFloorM; 0: none), and the tallest top
    // of any (above sea level; Terrain::kNoObstacleTop: none).
    double NarrowestObstacle() const { return narrowest_m_; }
    double TallestObstacleTop() const { return tallest_top_m_; }
    // How high anything stands over the ground under it at most: a box's top over the lowest of its corners and its
    // middle with 5 m to spare, or the far ground's towns' clutter.
    double TallestObstacle() const { return tallest_m_; }
    // Below this a wall is not looked at finer (a revetment's 2.5 m bank): a game value.
    static constexpr double kFootSpacingFloorM = 4.0;

    // How far a site keeps the map's buildings and woods off its centre (its levelled ground reaches as far, but for an
    // airfield's and a town's sites).
    static double ClearanceOf(const MapSiteDef& s) { return SiteClearance(s); }

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

    explicit MapData(MapDef def) : def_(std::move(def)) {}

    // The lattice drawing the ground at (east, north) and its place in DrawnLevels (null, -1: none).
    const MapGrid* LevelGridAt(double e, double n, int* index) const {
        for (std::size_t i = 0; i < levels_.size(); ++i) {
            if (levels_[i]->Covers(e, n)) {
                if (index != nullptr) *index = static_cast<int>(i);
                return levels_[i];
            }
        }
        if (index != nullptr) *index = -1;
        return nullptr;
    }

    // The drawn levels, finest first: the detail areas (once baked, by spacing), then the map proper, the far ground,
    // the country's lattice, the land beyond.
    void SetLevels(bool with_detail) {
        levels_.clear();
        if (with_detail) {
            std::vector<std::size_t> order(detail_ground_.size());
            for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(),
                             [this](std::size_t a, std::size_t b) { return detail_ground_[a].spacing_m < detail_ground_[b].spacing_m; });
            for (std::size_t i : order) {
                if (detail_ground_[i].cols > 1 && detail_ground_[i].rows > 1) levels_.push_back(&detail_ground_[i]);
            }
        }
        for (const MapGrid* g : {&core_, &far_, &country_, &beyond_}) {
            if (g->cols > 1 && g->rows > 1) levels_.push_back(g);
        }
    }

    static std::uint64_t HashId(const std::string& id) {  // FNV-1a
        std::uint64_t h = 0xCBF29CE484222325ull;
        for (char ch : id) {
            h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(ch));
            h *= 0x100000001B3ull;
        }
        return h;
    }

    // A box that blocks sight as the paths look it up (roadmap R6): its footprint (the turned rectangle, or the round
    // part's circle), its top above sea level, its narrowest side.
    struct Foot {
        double e = 0.0, n = 0.0, sin_h = 0.0, cos_h = 1.0, half_l = 0.0, half_w = 0.0, top = 0.0, side = 0.0;
        bool round = false;
        bool Holds(double pe, double pn) const {
            const double de = pe - e, dn = pn - n;
            if (round) return de * de + dn * dn <= half_l * half_l;
            return std::fabs(de * sin_h + dn * cos_h) <= half_l && std::fabs(de * cos_h - dn * sin_h) <= half_w;
        }
        // Where the ground segment from (e0, n0) along (de, dn) (t from 0 to 1) is inside it: [t_in, t_out]. False:
        // never.
        bool Chord(double e0, double n0, double de, double dn, double& t_in, double& t_out) const {
            const double we = e0 - e, wn = n0 - n;
            t_in = 0.0;
            t_out = 1.0;
            if (round) {
                const double a = de * de + dn * dn, b = 2.0 * (we * de + wn * dn), c = we * we + wn * wn - half_l * half_l;
                if (a < 1e-18) return c <= 0.0;
                const double disc = b * b - 4.0 * a * c;
                if (disc < 0.0) return false;
                const double root = std::sqrt(disc);
                t_in = std::max(0.0, (-b - root) / (2.0 * a));
                t_out = std::min(1.0, (-b + root) / (2.0 * a));
                return t_in <= t_out;
            }
            // The slabs along its length and across its width.
            const double p0[2] = {we * sin_h + wn * cos_h, we * cos_h - wn * sin_h};
            const double dp[2] = {de * sin_h + dn * cos_h, de * cos_h - dn * sin_h};
            const double half[2] = {half_l, half_w};
            for (int k = 0; k < 2; ++k) {
                if (std::fabs(dp[k]) < 1e-12) {
                    if (std::fabs(p0[k]) > half[k]) return false;
                    continue;
                }
                double ta = (-half[k] - p0[k]) / dp[k], tb = (half[k] - p0[k]) / dp[k];
                if (ta > tb) std::swap(ta, tb);
                t_in = std::max(t_in, ta);
                t_out = std::min(t_out, tb);
                if (t_in > t_out) return false;
            }
            return true;
        }
        // Its bounds' half extents east and north.
        double HalfE() const { return round ? half_l : half_l * std::fabs(sin_h) + half_w * std::fabs(cos_h); }
        double HalfN() const { return round ? half_l : half_l * std::fabs(cos_h) + half_w * std::fabs(sin_h); }
    };
    // Which footprints reach each cell of a rectangle (the map proper, a detail area), and per cell the tallest top and
    // the narrowest side among them.
    struct FootCells {
        double west_m = 0.0, south_m = 0.0, cell_m = 50.0;
        int cols = 0, rows = 0;
        std::vector<std::uint32_t> first;  // cols * rows + 1: the cell's footprints are feet[first[i] .. first[i + 1])
        std::vector<std::uint32_t> feet;
        std::vector<double> top, side;
        bool Covers(double e, double n) const {
            return e >= west_m && n >= south_m && e < west_m + cell_m * cols && n < south_m + cell_m * rows;
        }
        bool Overlaps(double w, double s, double e, double n) const {
            return e >= west_m && n >= south_m && w < west_m + cell_m * cols && s < south_m + cell_m * rows;
        }
        int Col(double e) const { return std::max(0, std::min(cols - 1, static_cast<int>(std::floor((e - west_m) / cell_m)))); }
        int Row(double n) const { return std::max(0, std::min(rows - 1, static_cast<int>(std::floor((n - south_m) / cell_m)))); }
        std::size_t Cell(double e, double n) const {
            return static_cast<std::size_t>(Row(n)) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(Col(e));
        }
        void Bound(double w, double s, double e, double n, double& top_m, double& side_m) const {
            for (int r = Row(s); r <= Row(n); ++r) {
                for (int c = Col(w); c <= Col(e); ++c) {
                    const std::size_t i = static_cast<std::size_t>(r) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(c);
                    if (top[i] <= Terrain::kNoObstacleTop) continue;
                    top_m = std::max(top_m, top[i]);
                    side_m = std::min(side_m, side[i]);
                }
            }
        }
    };
    // The dense cells holding (east, north): a detail area's, else the map proper's; null beyond them (the loose cells).
    const FootCells* CellsAt(double e, double n) const {
        for (const FootCells& c : rects_) {
            if (c.Covers(e, n)) return &c;
        }
        return nullptr;
    }
    static long long CellIndex(double v, double cell_m) { return static_cast<long long>(std::floor(v / cell_m)); }
    static long long PairKey(long long x, long long y) { return x * 2000003LL + y; }
    static long long CellKey(double e, double n, double cell_m) { return PairKey(CellIndex(e, cell_m), CellIndex(n, cell_m)); }
    // The cells of a lattice (west, south, cell; cols and rows, or -1: unbounded) the ground segment from (e0, n0) to
    // (e1, n1) passes through, in order, every one: `visit(col, row, t_in, t_out)`, the segment's fractions in it;
    // false stops. `walk_in`: where the segment comes into a bounded lattice (0 when it starts inside).
    template <class Visit>
    static void WalkCells(double e0, double n0, double e1, double n1, double west, double south, double cell, long long cols,
                          long long rows, double& walk_in, Visit visit) {
        const double de = e1 - e0, dn = n1 - n0;
        double ta = 0.0, tb = 1.0;
        if (cols > 0 && rows > 0) {  // clipped to the lattice
            const double lo[2] = {west, south};
            const double hi[2] = {west + cell * static_cast<double>(cols), south + cell * static_cast<double>(rows)};
            const double p0[2] = {e0, n0}, dp[2] = {de, dn};
            for (int k = 0; k < 2; ++k) {
                if (std::fabs(dp[k]) < 1e-12) {
                    if (p0[k] < lo[k] || p0[k] > hi[k]) return;
                    continue;
                }
                double t0 = (lo[k] - p0[k]) / dp[k], t1 = (hi[k] - p0[k]) / dp[k];
                if (t0 > t1) std::swap(t0, t1);
                ta = std::max(ta, t0);
                tb = std::min(tb, t1);
            }
            if (ta > tb) return;
        }
        walk_in = ta;
        const auto index = [&](double v, double origin, long long count) {
            long long i = static_cast<long long>(std::floor((v - origin) / cell));
            if (count > 0) i = std::max(0LL, std::min(count - 1, i));
            return i;
        };
        const double start = ta + std::min(1e-9, 0.5 * (tb - ta));  // the start cell from a point just inside, off an edge
        long long cx = index(e0 + de * start, west, cols), cy = index(n0 + dn * start, south, rows);
        const long long step_x = de > 0.0 ? 1 : -1, step_y = dn > 0.0 ? 1 : -1;
        constexpr double kNever = 1e300;
        const auto next_edge = [&](long long ci, long long step, double origin, double p, double d) {
            if (std::fabs(d) < 1e-12) return kNever;
            const double edge = origin + cell * static_cast<double>(step > 0 ? ci + 1 : ci);
            return (edge - p) / d;
        };
        double tx = next_edge(cx, step_x, west, e0, de), ty = next_edge(cy, step_y, south, n0, dn);
        const double dtx = std::fabs(de) < 1e-12 ? kNever : cell / std::fabs(de);
        const double dty = std::fabs(dn) < 1e-12 ? kNever : cell / std::fabs(dn);
        double t = ta;
        const long long most = 4 + static_cast<long long>(std::fabs(de) / cell) + static_cast<long long>(std::fabs(dn) / cell);
        for (long long guard = 0; guard <= most; ++guard) {
            const double t_out = std::min({tx, ty, tb});
            if (!visit(cx, cy, t, t_out)) return;
            if (t_out >= tb) return;
            t = t_out;
            if (tx <= ty) {
                cx += step_x;
                tx += dtx;
            } else {
                cy += step_y;
                ty += dty;
            }
            if (cols > 0 && (cx < 0 || cx >= cols || cy < 0 || cy >= rows)) return;
        }
    }
    static constexpr double kWalkEps = 1e-9;              // of a segment: a cut met on a cell's edge is told in both cells
    static constexpr double kLooseCellM = 50.0;           // beyond the map proper and the detail areas
    static constexpr double kPitWaterClearM = 25.0;       // a gun pit no nearer the water (a game value)
    static constexpr double kBlockM = 500.0;              // the coarse bound's blocks
    static constexpr double kFineBoundAreaM2 = 250000.0;  // a segment whose bounds are no larger is bounded by the finer cells

    // Beyond the map proper and the detail areas where no box stands: the clutter of the far ground's towns the map does
    // not lay out.
    double Clutter(double e, double n) const {
        for (const ClutterTown& t : clutter_towns_) {
            if (std::fabs(e - t.e) >= t.r || std::fabs(n - t.n) >= t.r) continue;
            const double d = std::hypot(e - t.e, n - t.n);
            if (d < t.r) return def_.clutter.HeightOf(TownClutterClass(t.village ? "village" : "town", d, t.r, def_.clutter.dense_share));
        }
        return 0.0;
    }
    static long long CanopyKey(double e, double n) {
        return static_cast<long long>(std::floor(e / 10.0)) * 2000003LL + static_cast<long long>(std::floor(n / 10.0));
    }

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

    // How far (east, north) is from the nearest river's bed (NaturalGround's: half its width and 15 m), the map's own and
    // its far ground's; negative inside it.
    double FromRiverBed(double e, double n) const {
        double best = 1e300;
        for (const std::vector<MapLineDef>* list : {&def_.rivers, def_.far_ground ? &def_.far_ground->rivers : nullptr}) {
            if (list == nullptr) continue;
            for (const MapLineDef& r : *list) best = std::min(best, mapgeo::DistToLine(r.points, e, n) - (0.5 * r.width_m + kRiverBedBankM));
        }
        return best;
    }

    void Bake() {
        // A site's levelled ground stops short of a river's bed (roadmap S9: the guns on the Rava's bank): its radius no
        // more than its centre's distance from the bed, none for a site standing in it, so the bed is never filled.
        for (const MapSiteDef& s : def_.sites) {
            const double r = std::min(SiteFlatRadius(s), FromRiverBed(s.centre.east_m, s.centre.north_m));
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
        if (!def_.far_ground || def_.far_grid_m <= 0.0) return;
        // The country's ground as it is drawn beyond the far ground (country_map.hpp's BakeGround: its natural ground at
        // its grid's nodes over its extent), and the land beyond its borders at 4 km, 100 km out (the same frame: the
        // far ground's origin is the map's). Nodes wholly under the far ground are never drawn or sampled: left 0.
        const MapDef& cg = *def_.far_ground;
        const auto lattice = [this, &cg](MapGrid& g, double west, double south, int cols, int rows, double spacing) {
            g.spacing_m = spacing;
            g.west_m = west;
            g.south_m = south;
            g.cols = cols;
            g.rows = rows;
            g.values.assign(static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows), 0.0);
            const double inside = def_.far_m - 2.0 * spacing;  // under the far ground, two cells in from its edge
            for (int r = 0; r < g.rows; ++r) {
                for (int c = 0; c < g.cols; ++c) {
                    const double e = g.EastOf(c), n = g.NorthOf(r);
                    if (std::fabs(e) < inside && std::fabs(n) < inside) continue;
                    g.At(c, r) = NaturalGround(cg, e, n);
                }
            }
        };
        // The country's lattice covers the country and the far ground both (pannonia ends 140 km north, the far ground
        // 150 km), grown in whole cells of the 4 km lattice beyond (pannonia's 330 by 320 km becomes 332 by 332), so
        // every level's edge falls on the next coarser level's nodes and a coarse cell lies wholly inside a finer level
        // or wholly outside it: no strip drawn twice, no coarse triangle over the finer ground (roadmap R5's review,
        // NoGroundDrawnTwice). The country's ground there is its natural ground, as everywhere.
        const double sp = def_.far_grid_m;
        const double per = kBeyondSpacingM / sp;  // country cells to a beyond cell (4 at 1 km)
        const double snap = std::round(per) == per ? kBeyondSpacingM : sp;
        const auto whole = [snap](double span) { return std::ceil(span / snap - 1e-9) * snap; };
        const double west = cg.west_m - whole(std::max(0.0, cg.west_m + def_.far_m));
        const double south = cg.south_m - whole(std::max(0.0, cg.south_m + def_.far_m));
        const double east = west + whole(std::max(cg.east_m, def_.far_m) - west);
        const double north = south + whole(std::max(cg.north_m, def_.far_m) - south);
        lattice(country_, west, south, static_cast<int>(std::lround((east - west) / sp)) + 1,
                static_cast<int>(std::lround((north - south) / sp)) + 1, sp);
        lattice(beyond_, west - kBeyondM, south - kBeyondM, static_cast<int>(std::lround((east - west + 2.0 * kBeyondM) / kBeyondSpacingM)) + 1,
                static_cast<int>(std::lround((north - south + 2.0 * kBeyondM) / kBeyondSpacingM)) + 1, kBeyondSpacingM);
    }
    static constexpr double kBeyondM = 100000.0;        // the land drawn beyond the country's borders (a game value)
    static constexpr double kBeyondSpacingM = 4000.0;

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

    // ------------------------------------------------------------------ detail areas (roadmap R6)

    // The country's roads, railways and rivers through each detail area beyond the map proper (an area inside it has
    // the district's own lines), clipped to the area and a kilometre round it.
    // The box a detail area's imported lines are clipped to (the area and a kilometre round it); false for an area whose
    // box reaches the map proper (the map proper's own lines serve it) or when there is no far ground to import from.
    bool ImportBox(const MapDetailDef& a, double& w, double& s, double& e, double& n) const {
        constexpr double kMarginM = 1000.0;
        w = a.centre.east_m - a.half_m - kMarginM;
        e = a.centre.east_m + a.half_m + kMarginM;
        s = a.centre.north_m - a.half_m - kMarginM;
        n = a.centre.north_m + a.half_m + kMarginM;
        return def_.far_ground && !(w < def_.east_m && e > def_.west_m && s < def_.north_m && n > def_.south_m);
    }

    void ImportDetailLines() {
        if (!def_.far_ground) return;
        for (const MapDetailDef& a : def_.detail) {
            double w = 0.0, s = 0.0, e = 0.0, n = 0.0;
            if (!ImportBox(a, w, s, e, n)) continue;
            const auto clip = [&](const std::vector<MapLineDef>& from, std::vector<MapLineDef>& to) {
                for (const MapLineDef& line : from) {
                    std::vector<std::vector<MapPoint>> runs;
                    bool open = false;
                    for (std::size_t i = 1; i < line.points.size(); ++i) {
                        MapPoint p = line.points[i - 1], q = line.points[i];
                        if (!ClipSegment(w, s, e, n, p, q)) {
                            open = false;
                            continue;
                        }
                        if (!open) runs.push_back({p});
                        else if (std::hypot(runs.back().back().east_m - p.east_m, runs.back().back().north_m - p.north_m) > 0.01) runs.push_back({p});
                        runs.back().push_back(q);
                        open = true;  // the next segment carries on this run if it starts where this one ended
                    }
                    for (std::size_t k = 0; k < runs.size(); ++k) {
                        if (runs[k].size() < 2) continue;
                        MapLineDef c = line;
                        if (runs.size() > 1) c.id += "#" + std::to_string(k + 1);
                        c.points = runs[k];
                        to.push_back(std::move(c));
                    }
                }
            };
            clip(def_.far_ground->roads, detail_lines_.roads);
            clip(def_.far_ground->railways, detail_lines_.railways);
            clip(def_.far_ground->rivers, detail_lines_.rivers);
        }
    }

    // Liang-Barsky: clips p-q to the box; false if none of it is inside.
    static bool ClipSegment(double w, double s, double e, double n, MapPoint& p, MapPoint& q) {
        const double dx = q.east_m - p.east_m, dy = q.north_m - p.north_m;
        double t0 = 0.0, t1 = 1.0;
        const double pp[4] = {-dx, dx, -dy, dy};
        const double qq[4] = {p.east_m - w, e - p.east_m, p.north_m - s, n - p.north_m};
        for (int k = 0; k < 4; ++k) {
            if (pp[k] == 0.0) {
                if (qq[k] < 0.0) return false;
                continue;
            }
            const double r = qq[k] / pp[k];
            if (pp[k] < 0.0) t0 = std::max(t0, r);
            else t1 = std::min(t1, r);
        }
        if (t0 > t1) return false;
        const MapPoint a = p;
        p = {a.east_m + t0 * dx, a.north_m + t0 * dy};
        q = {a.east_m + t1 * dx, a.north_m + t1 * dy};
        return true;
    }

    // What buildings, woods and the detail ground's micro relief keep clear of: the lines (the map's and the detail
    // areas' imported ones), the water, the sites. And the far ground's towns the map does not lay out (their clutter).
    void BuildIndices() {
        for (const MapLineDef& r : def_.roads) lines_.Add(r.points, 0.5 * r.width_m);
        for (const MapLineDef& r : def_.railways) lines_.Add(r.points, 0.5 * r.width_m);
        for (const MapLineDef& r : def_.rivers) {
            water_.Add(r.points, 0.5 * r.width_m);
            lines_.Add(r.points, 0.5 * r.width_m);
        }
        for (const MapLineDef& r : detail_lines_.roads) lines_.Add(r.points, 0.5 * r.width_m);
        for (const MapLineDef& r : detail_lines_.railways) lines_.Add(r.points, 0.5 * r.width_m);
        for (const MapLineDef& r : detail_lines_.rivers) {
            water_.Add(r.points, 0.5 * r.width_m);
            lines_.Add(r.points, 0.5 * r.width_m);
        }
        for (const MapSiteDef& s : def_.sites) {
            occupied_.Take(s.centre.east_m, s.centre.north_m, SiteClearance(s));
        }
        // The water a path's reflection and smooth earth look for (WaterAt): the rivers above, the map's lakes, and beyond
        // the map proper its country's lakes and rivers.
        has_water_ = !def_.rivers.empty() || !detail_lines_.rivers.empty();
        const auto add_lake = [&](const MapLakeDef& l, bool country, std::size_t index) {
            if (l.points.size() < 3) return;
            LakeBox b{1e300, 1e300, -1e300, -1e300, country, index};
            for (const MapPoint& p : l.points) {
                b.w = std::min(b.w, p.east_m);
                b.s = std::min(b.s, p.north_m);
                b.e = std::max(b.e, p.east_m);
                b.n = std::max(b.n, p.north_m);
            }
            lake_boxes_.push_back(b);
            has_water_ = true;
        };
        for (std::size_t i = 0; i < def_.lakes.size(); ++i) add_lake(def_.lakes[i], false, i);
        if (!def_.far_ground) return;
        for (std::size_t i = 0; i < def_.far_ground->lakes.size(); ++i) add_lake(def_.far_ground->lakes[i], true, i);
        for (const MapLineDef& r : def_.far_ground->rivers) {
            far_water_.Add(r.points, 0.5 * r.width_m);
            has_water_ = true;
        }
        for (const MapSettlementDef& t : def_.far_ground->settlements) {
            bool laid = def_.InCore(t.centre.east_m, t.centre.north_m);
            for (const MapSettlementDef& s : def_.settlements) laid = laid || s.id == t.id;
            for (const MapSettlementDef& s : def_.detail_settlements) laid = laid || s.id == t.id;
            if (laid) continue;
            clutter_towns_.push_back({t.centre.east_m, t.centre.north_m, t.radius_m, t.kind == "village"});
        }
    }

    // How much of the micro relief a detail area's node keeps (0 to 1): none on and near the lines (15 m from their
    // edge, then over 45 m), on levelled sites and in towns (50 m beyond their edge, then over 50 m).
    double MicroWeight(double e, double n, const std::vector<const Flat*>& flats, const std::vector<const MapSettlementDef*>& towns) const {
        const double from_line = lines_.EdgeDistance(e, n, 60.0);
        double m = noise::Smooth(Clamp((from_line - 15.0) / 45.0, 0.0, 1.0));
        for (const Flat* f : flats) {
            if (m <= 0.0) return 0.0;
            m *= noise::Smooth(Clamp((std::hypot(e - f->e, n - f->n) - f->r) / 50.0, 0.0, 1.0));
        }
        for (const MapSettlementDef* t : towns) {
            if (m <= 0.0) return 0.0;
            m *= noise::Smooth(Clamp((std::hypot(e - t->centre.east_m, n - t->centre.north_m) - t->radius_m - 50.0) / 50.0, 0.0, 1.0));
        }
        return m;
    }

    // Each detail area's ground: the coarse level's triangles at the area's spacing (so its edge is the coarse edge
    // exactly), plus micro relief faded to nothing over the last 300 m to the edge and by MicroWeight. Then the areas
    // join DrawnLevels, finest first.
    void BakeDetail() {
        detail_ground_.assign(def_.detail.size(), MapGrid());
        for (std::size_t i = 0; i < def_.detail.size(); ++i) {
            const MapDetailDef& a = def_.detail[i];
            MapGrid& g = detail_ground_[i];
            g.spacing_m = a.spacing_m;
            g.west_m = a.centre.east_m - a.half_m;
            g.south_m = a.centre.north_m - a.half_m;
            g.cols = static_cast<int>(std::lround(2.0 * a.half_m / a.spacing_m)) + 1;
            g.rows = g.cols;
            g.values.assign(static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows), 0.0);
            // The sites and towns that reach into it.
            std::vector<const Flat*> flats;
            for (const Flat& f : flats_) {
                if (std::fabs(f.e - a.centre.east_m) < a.half_m + f.r + 100.0 && std::fabs(f.n - a.centre.north_m) < a.half_m + f.r + 100.0) flats.push_back(&f);
            }
            std::vector<const MapSettlementDef*> towns;
            for (const std::vector<MapSettlementDef>* list : {&def_.settlements, &def_.detail_settlements}) {
                for (const MapSettlementDef& t : *list) {
                    const double reach = a.half_m + t.radius_m + 150.0;
                    if (std::fabs(t.centre.east_m - a.centre.east_m) < reach && std::fabs(t.centre.north_m - a.centre.north_m) < reach) towns.push_back(&t);
                }
            }
            const std::uint64_t seed = noise::Mix64(def_.seed ^ 0xDE7A11ull ^ HashId(a.id));
            for (int r = 0; r < g.rows; ++r) {
                for (int c = 0; c < g.cols; ++c) {
                    const double e = g.EastOf(c), n = g.NorthOf(r);
                    double v = GroundHeight(e, n);  // the coarse level's triangles (the detail levels are not in yet)
                    if (a.micro_m > 0.0) {
                        const double to_edge = a.half_m - std::max(std::fabs(e - a.centre.east_m), std::fabs(n - a.centre.north_m));
                        const double w = noise::Smooth(Clamp(to_edge / 300.0, 0.0, 1.0));
                        if (w > 0.0) {
                            const double m = MicroWeight(e, n, flats, towns);
                            if (m > 0.0) v += w * m * a.micro_m * (2.0 * noise::Fbm(seed, e, n, 400.0, 3) - 1.0);
                        }
                    }
                    g.At(c, r) = v;
                }
            }
        }
        SetLevels(true);
    }

    // The detail areas' towns, each on its own random stream (so the map proper's layout is the same with or without
    // them), after the country's lines through the areas are laid as strips and bridges. The far ground's woods give way.
    void LayDetailSettlements() {
        LayLinesOf(detail_lines_.roads, detail_lines_.railways, detail_lines_.rivers);
        for (const MapSettlementDef& s : def_.detail_settlements) {
            const double clear = s.radius_m + 100.0;
            std::vector<MapBox>& boxes = layout_.boxes;
            boxes.erase(std::remove_if(boxes.begin(), boxes.end(),
                                       [&](const MapBox& b) {
                                           if (b.part != MapPart::Canopy || def_.InCore(b.east_m, b.north_m)) return false;
                                           if (std::hypot(b.east_m - s.centre.east_m, b.north_m - s.centre.north_m) >= clear) return false;
                                           canopy_cells_.erase(CanopyKey(b.east_m, b.north_m));
                                           return true;
                                       }),
                        boxes.end());
            noise::Stream rng(noise::Mix64(def_.seed ^ HashId(s.id)));
            LaySettlement(s, rng, detail_lines_.roads);
            layout_.labels.push_back({s.id, s.name, s.kind, s.centre});
        }
    }

    // ------------------------------------------------------------------ the map proper

    void Lay() {
        noise::Stream rng(noise::Mix64(def_.seed ^ 0x1A70Full));
        LayLinesOf(def_.roads, def_.railways, def_.rivers);
        for (const MapSiteDef& s : def_.sites) LaySite(s, rng);
        for (const MapSettlementDef& s : def_.settlements) LaySettlement(s, rng, def_.roads);
        LayFarmsteads(rng);
        LayWoods(rng);
        LayMarshes();
        for (const MapSettlementDef& s : def_.settlements) layout_.labels.push_back({s.id, s.name, s.kind, s.centre});
        for (const MapSiteDef& s : def_.sites) layout_.labels.push_back({s.id, s.name, s.kind, s.centre});
    }

    // Roads, railways and rivers as strips; a bridge wherever a road or rail crosses a river.
    void LayLinesOf(const std::vector<MapLineDef>& roads, const std::vector<MapLineDef>& railways, const std::vector<MapLineDef>& rivers) {
        const auto crossings_of = [this, &rivers](const MapLineDef& line, bool rail) {
            for (const MapLineDef& river : rivers) {
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
        for (const MapLineDef& r : roads) crossings_of(r, false);
        for (const MapLineDef& r : railways) crossings_of(r, true);
        for (const MapLineDef& r : roads) {
            const MapSurface s = r.line_class == "main" ? MapSurface::MainRoad : (r.line_class == "track" ? MapSurface::Track : MapSurface::Road);
            Strip(r, s, 0.25);
        }
        for (const MapLineDef& r : railways) Strip(r, MapSurface::Rail, 0.45);
        for (const MapLineDef& r : rivers) Strip(r, MapSurface::Water, 0.0);
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
            const bool core = def_.InCore(a.east_m, a.north_m) || def_.InCore(b.east_m, b.north_m) || def_.InDetail(a.east_m, a.north_m) ||
                              def_.InDetail(b.east_m, b.north_m);
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
            // The pits on a ring 55 m out; one that would stand within 25 m of the water (a site on a river's bank, roadmap
            // S9) goes round the ring, then nearer in or further out, to the first place clear of the water and of the
            // pits already dug (none where every place is wet).
            const int count = s.count > 0 ? s.count : 4;
            std::vector<MapPoint> dug;
            const auto free_at = [&](double pe, double pn) {
                if (water_.Near(pe, pn, kPitWaterClearM)) return false;
                for (const MapPoint& d : dug) {
                    if (std::hypot(pe - d.east_m, pn - d.north_m) < 20.0) return false;
                }
                return true;
            };
            for (int i = 0; i < count; ++i) {
                const double a = kTwoPi * i / count + 0.3;
                bool placed = false;
                for (const double ring : {55.0, 35.0, 75.0}) {
                    for (int k = 0; k < 36 && !placed; ++k) {
                        const double ak = a + kTwoPi * (k % 2 == 0 ? k / 2 : -(k + 1) / 2) / 36.0;
                        const double pe = e + ring * std::sin(ak), pn = n + ring * std::cos(ak);
                        if (!free_at(pe, pn)) continue;
                        GunPit(pe, pn, 9.0, 1.3);
                        dug.push_back({pe, pn});
                        placed = true;
                    }
                    if (placed) break;
                }
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

    // A town or village: houses along the roads through it (`roads`), then the blocks behind.
    void LaySettlement(const MapSettlementDef& s, noise::Stream& rng, const std::vector<MapLineDef>& roads) {
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
        for (const MapLineDef& road : roads) {
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
        const long long key = CanopyKey(e, n);
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
    // Thin or porous things (masts, chimneys, towers, launchers, tents) and what lies flat do not hide what is behind
    // them; a 50 m cell made opaque by a 0.6 m mast would.
    static bool Opaque(MapPart p) {
        return !(p == MapPart::Marsh || p == MapPart::Pad || p == MapPart::Runway || p == MapPart::Mast || p == MapPart::Chimney ||
                 p == MapPart::Tower || p == MapPart::Launcher || p == MapPart::Tent);
    }

    // The tallest thing standing over each node, above the ground there: every node (the centre of its cell) inside the
    // box's footprint (roadmap R6: no wider than the box). A picture of the obstacles (ObstacleGrid); the paths look the
    // footprints up themselves (ObstacleTop).
    void Stamp(MapGrid& g, const Foot& f) const {
        const int c0 = std::max(0, static_cast<int>(std::ceil((f.e - f.HalfE() - g.west_m) / g.spacing_m)));
        const int c1 = std::min(g.cols - 1, static_cast<int>(std::floor((f.e + f.HalfE() - g.west_m) / g.spacing_m)));
        const int r0 = std::max(0, static_cast<int>(std::ceil((f.n - f.HalfN() - g.south_m) / g.spacing_m)));
        const int r1 = std::min(g.rows - 1, static_cast<int>(std::floor((f.n + f.HalfN() - g.south_m) / g.spacing_m)));
        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) {
                if (!f.Holds(g.EastOf(c), g.NorthOf(r))) continue;
                double& v = g.At(c, r);
                v = std::max(v, f.top - GroundHeight(g.EastOf(c), g.NorthOf(r)));
            }
        }
    }

    // The footprints of what blocks sight, indexed by cell (the map proper's at 50 m, each detail area's at 25 m, the
    // rest by 50 m cells as they come, and every one in 500 m blocks for the coarse bound), and the obstacle grids drawn
    // from them: the tallest thing over each node of the map proper (50 m) and of each detail area (25 m).
    void BakeObstacles() {
        feet_.clear();
        narrowest_m_ = 0.0;
        for (const MapBox& bx : layout_.boxes) {
            if (!Opaque(bx.part)) continue;
            Foot f;
            f.e = bx.east_m;
            f.n = bx.north_m;
            f.sin_h = std::sin(bx.heading_rad);
            f.cos_h = std::cos(bx.heading_rad);
            f.round = MapPartIsRound(bx.part);
            f.half_l = 0.5 * bx.length_m;
            f.half_w = f.round ? f.half_l : 0.5 * bx.width_m;
            f.top = bx.base_m + bx.height_m;
            f.side = std::max(kFootSpacingFloorM, f.round ? bx.length_m : std::min(bx.length_m, bx.width_m));
            narrowest_m_ = narrowest_m_ > 0.0 ? std::min(narrowest_m_, f.side) : f.side;
            feet_.push_back(f);
        }
        rects_.clear();
        for (const MapDetailDef& a : def_.detail) {
            FootCells c;
            c.cell_m = kDetailObstacleSpacingM;
            c.west_m = a.centre.east_m - a.half_m;
            c.south_m = a.centre.north_m - a.half_m;
            c.cols = c.rows = static_cast<int>(std::ceil(2.0 * a.half_m / c.cell_m)) + 1;
            rects_.push_back(c);
        }
        {
            FootCells c;
            c.cell_m = def_.core_spacing_m;
            c.west_m = def_.west_m;
            c.south_m = def_.south_m;
            c.cols = static_cast<int>(std::ceil((def_.east_m - def_.west_m) / c.cell_m)) + 1;
            c.rows = static_cast<int>(std::ceil((def_.north_m - def_.south_m) / c.cell_m)) + 1;
            rects_.push_back(c);
        }
        for (FootCells& c : rects_) {  // count, then fill
            const std::size_t cells = static_cast<std::size_t>(c.cols) * static_cast<std::size_t>(c.rows);
            std::vector<std::uint32_t> count(cells + 1, 0);
            c.top.assign(cells, Terrain::kNoObstacleTop);
            c.side.assign(cells, 1e300);
            for (int pass = 0; pass < 2; ++pass) {
                for (std::size_t k = 0; k < feet_.size(); ++k) {
                    const Foot& f = feet_[k];
                    const double w = f.e - f.HalfE(), e = f.e + f.HalfE(), s = f.n - f.HalfN(), n = f.n + f.HalfN();
                    if (!c.Overlaps(w, s, e, n)) continue;
                    for (int r = c.Row(s); r <= c.Row(n); ++r) {
                        for (int col = c.Col(w); col <= c.Col(e); ++col) {
                            const std::size_t i = static_cast<std::size_t>(r) * static_cast<std::size_t>(c.cols) + static_cast<std::size_t>(col);
                            if (pass == 0) {
                                ++count[i + 1];
                                c.top[i] = std::max(c.top[i], f.top);
                                c.side[i] = std::min(c.side[i], f.side);
                            } else {
                                c.feet[count[i]++] = static_cast<std::uint32_t>(k);
                            }
                        }
                    }
                }
                if (pass == 0) {
                    for (std::size_t i = 0; i < cells; ++i) count[i + 1] += count[i];
                    c.first = count;
                    c.feet.assign(count[cells], 0);
                }
            }
        }
        loose_.clear();
        // The 500 m blocks over everything that stands: each block's tallest top and narrowest side (no footprint list).
        blocks_ = FootCells();
        blocks_.cell_m = kBlockM;
        tallest_top_m_ = Terrain::kNoObstacleTop;
        tallest_m_ = std::max(0.0, def_.clutter.Tallest());
        for (const Foot& f : feet_) {
            double low = GroundHeight(f.e, f.n);
            for (const double se : {-1.0, 1.0}) {
                for (const double sn : {-1.0, 1.0}) low = std::min(low, GroundHeight(f.e + se * f.HalfE(), f.n + sn * f.HalfN()));
            }
            tallest_m_ = std::max(tallest_m_, f.top - low + 5.0);  // 5 m for the ground between those points
        }
        if (!feet_.empty()) {
            double w = 1e300, e = -1e300, s = 1e300, n = -1e300;
            for (const Foot& f : feet_) {
                w = std::min(w, f.e - f.HalfE());
                e = std::max(e, f.e + f.HalfE());
                s = std::min(s, f.n - f.HalfN());
                n = std::max(n, f.n + f.HalfN());
                tallest_top_m_ = std::max(tallest_top_m_, f.top);
            }
            blocks_.west_m = std::floor(w / kBlockM) * kBlockM;
            blocks_.south_m = std::floor(s / kBlockM) * kBlockM;
            blocks_.cols = static_cast<int>(std::floor((e - blocks_.west_m) / kBlockM)) + 1;
            blocks_.rows = static_cast<int>(std::floor((n - blocks_.south_m) / kBlockM)) + 1;
            const std::size_t cells = static_cast<std::size_t>(blocks_.cols) * static_cast<std::size_t>(blocks_.rows);
            blocks_.top.assign(cells, Terrain::kNoObstacleTop);
            blocks_.side.assign(cells, 1e300);
        }
        for (std::size_t k = 0; k < feet_.size(); ++k) {
            const Foot& f = feet_[k];
            const double w = f.e - f.HalfE(), e = f.e + f.HalfE(), s = f.n - f.HalfN(), n = f.n + f.HalfN();
            for (int r = blocks_.Row(s); r <= blocks_.Row(n); ++r) {
                for (int c = blocks_.Col(w); c <= blocks_.Col(e); ++c) {
                    const std::size_t i = static_cast<std::size_t>(r) * static_cast<std::size_t>(blocks_.cols) + static_cast<std::size_t>(c);
                    blocks_.top[i] = std::max(blocks_.top[i], f.top);
                    blocks_.side[i] = std::min(blocks_.side[i], f.side);
                }
            }
            bool dense = false;  // wholly inside one dense grid: no loose cells needed
            for (const FootCells& c : rects_) dense = dense || (c.Covers(w, s) && c.Covers(e, n));
            if (dense) continue;
            for (long long y = CellIndex(s, kLooseCellM); y <= CellIndex(n, kLooseCellM); ++y) {
                for (long long x = CellIndex(w, kLooseCellM); x <= CellIndex(e, kLooseCellM); ++x) {
                    loose_[PairKey(x, y)].push_back(static_cast<std::uint32_t>(k));
                }
            }
        }
        obstacles_.west_m = def_.west_m;
        obstacles_.south_m = def_.south_m;
        obstacles_.spacing_m = def_.core_spacing_m;
        obstacles_.cols = core_.cols;
        obstacles_.rows = core_.rows;
        obstacles_.values.assign(core_.values.size(), 0.0);
        detail_obstacles_.assign(def_.detail.size(), MapGrid());
        for (std::size_t i = 0; i < def_.detail.size(); ++i) {
            const MapDetailDef& a = def_.detail[i];
            MapGrid& g = detail_obstacles_[i];
            g.spacing_m = kDetailObstacleSpacingM;
            g.west_m = a.centre.east_m - a.half_m;
            g.south_m = a.centre.north_m - a.half_m;
            g.cols = static_cast<int>(std::lround(2.0 * a.half_m / g.spacing_m)) + 1;
            g.rows = g.cols;
            g.values.assign(static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows), 0.0);
        }
        for (const Foot& f : feet_) {
            if (def_.InCore(f.e, f.n)) Stamp(obstacles_, f);
            for (std::size_t i = 0; i < def_.detail.size(); ++i) {
                const MapDetailDef& a = def_.detail[i];
                if (std::fabs(f.e - a.centre.east_m) < a.half_m + f.HalfE() && std::fabs(f.n - a.centre.north_m) < a.half_m + f.HalfN()) {
                    Stamp(detail_obstacles_[i], f);
                }
            }
        }
    }
    static constexpr double kDetailObstacleSpacingM = 25.0;

    MapDef def_;
    MapGrid core_, far_, country_, beyond_, obstacles_;
    std::vector<MapGrid> detail_ground_, detail_obstacles_;  // per detail area, in Def().detail's order
    std::vector<const MapGrid*> levels_;                    // DrawnLevels
    DetailLines detail_lines_;
    std::vector<Flat> flats_;
    MapLayout layout_;
    mapgeo::SegmentIndex lines_{250.0};
    mapgeo::SegmentIndex water_{250.0};
    mapgeo::SegmentIndex far_water_{250.0};  // the country's rivers beyond the map proper (WaterAt)
    struct LakeBox {
        double w, s, e, n;  // the shore's bounds
        bool country;       // the country's (beyond the map proper), else the map's
        std::size_t index;
    };
    std::vector<LakeBox> lake_boxes_;
    bool has_water_ = false;
    // Where water may be (WaterAt's quick no): 100 m cells over the water's bounds, set wherever a river's or a lake's
    // bounds, widened by a cell, reach; the exact test follows only there.
    struct WaterGrid {
        double west = 0.0, south = 0.0;
        long long nx = 0, ny = 0;
        std::vector<std::uint64_t> bits;
        static constexpr double kCellM = 100.0;
        long long Cell(double v, double origin) const { return static_cast<long long>(std::floor((v - origin) / kCellM)); }
        bool Maybe(double e, double n) const {
            const long long x = Cell(e, west), y = Cell(n, south);
            if (x < 0 || y < 0 || x >= nx || y >= ny) return false;
            const long long i = y * nx + x;
            return ((bits[static_cast<std::size_t>(i >> 6)] >> (i & 63)) & 1u) != 0;
        }
        void Mark(double w, double s, double e, double n) {
            const long long x0 = std::max(0LL, Cell(w, west) - 1), x1 = std::min(nx - 1, Cell(e, west) + 1);
            const long long y0 = std::max(0LL, Cell(s, south) - 1), y1 = std::min(ny - 1, Cell(n, south) + 1);
            for (long long y = y0; y <= y1; ++y) {
                for (long long x = x0; x <= x1; ++x) {
                    const long long i = y * nx + x;
                    bits[static_cast<std::size_t>(i >> 6)] |= std::uint64_t{1} << (i & 63);
                }
            }
        }
    };
    WaterGrid water_grid_;

    // WaterAt's grid: every river segment's bounds widened by its half width, and every lake's (BuildIndices's lakes).
    void BuildWaterGrid() {
        if (!has_water_) return;
        struct Box {
            double w, s, e, n;
        };
        std::vector<Box> boxes;
        const auto add_river = [&](const MapLineDef& r) {
            const double hw = 0.5 * r.width_m;
            for (std::size_t i = 1; i < r.points.size(); ++i) {
                const MapPoint& a = r.points[i - 1];
                const MapPoint& b = r.points[i];
                boxes.push_back({std::min(a.east_m, b.east_m) - hw, std::min(a.north_m, b.north_m) - hw,
                                 std::max(a.east_m, b.east_m) + hw, std::max(a.north_m, b.north_m) + hw});
            }
        };
        for (const MapLineDef& r : def_.rivers) add_river(r);
        for (const MapLineDef& r : detail_lines_.rivers) add_river(r);
        if (def_.far_ground) {
            for (const MapLineDef& r : def_.far_ground->rivers) add_river(r);
        }
        for (const LakeBox& l : lake_boxes_) boxes.push_back({l.w, l.s, l.e, l.n});
        if (boxes.empty()) return;
        Box all = boxes.front();
        for (const Box& b : boxes) {
            all.w = std::min(all.w, b.w);
            all.s = std::min(all.s, b.s);
            all.e = std::max(all.e, b.e);
            all.n = std::max(all.n, b.n);
        }
        WaterGrid& g = water_grid_;
        g.west = all.w - 2.0 * WaterGrid::kCellM;
        g.south = all.s - 2.0 * WaterGrid::kCellM;
        g.nx = static_cast<long long>(std::ceil((all.e - g.west) / WaterGrid::kCellM)) + 3;
        g.ny = static_cast<long long>(std::ceil((all.n - g.south) / WaterGrid::kCellM)) + 3;
        g.bits.assign(static_cast<std::size_t>((g.nx * g.ny + 63) / 64), 0);
        for (const Box& b : boxes) g.Mark(b.w, b.s, b.e, b.n);
    }
    mapgeo::Occupancy occupied_{40.0};
    std::unordered_set<long long> canopy_cells_;
    // What blocks sight (BakeObstacles): the footprints, their dense cells (the detail areas', then the map proper's),
    // the loose cells beyond them, the 500 m blocks' tallest top and narrowest side.
    std::vector<Foot> feet_;
    std::vector<FootCells> rects_;
    std::unordered_map<long long, std::vector<std::uint32_t>> loose_;
    FootCells blocks_;  // the 500 m blocks: top and side only
    double narrowest_m_ = 0.0;
    double tallest_top_m_ = Terrain::kNoObstacleTop;
    double tallest_m_ = 0.0;
    struct ClutterTown {
        double e, n, r;
        bool village;
    };
    std::vector<ClutterTown> clutter_towns_;
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

    // The scenario's (east, north) over the map's (east, north): the inverse of FrameToMap (exactly: FrameToMap of it
    // is the map's point again).
    void MapToFrameEN(double map_e, double map_n, double& east_m, double& north_m) const {
        const double de = map_e - off_e_, dn = map_n - off_n_;
        const double det = a_ * d_ - b_ * c_;
        east_m = (d_ * de - b_ * dn) / det;
        north_m = (a_ * dn - c_ * de) / det;
    }
    // The point of the ground, or anything standing on it, in the scenario's ENU: `alt_m` above sea level straight under
    // the scenario's frame (its z) at the map's (east, north) (roadmap R5: the vertical the sim's GroundEnu uses, so what
    // the game draws there is where the sim has the ground). Not MapToFrame, whose vertical is the map's local up.
    Vec3 GroundPoint(double map_e, double map_n, double alt_m) const {
        double x = 0.0, y = 0.0;
        MapToFrameEN(map_e, map_n, x, y);
        return GroundPointInFrame(frame_, x, y, alt_m);
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

// Roadmap R5's contract, checked: at `samples` random points of every drawn level (where it is the ground), the sim's
// height (MapData::GroundHeight) is the height on the drawn triangle of the cell's corners to a millimetre, and the
// sim's ground point there in the scenario's frame (GroundPointInFrame) lies on the triangle the game draws through
// the corners' GroundPoints, within the chord's sag under the Earth's curve (a triangle is flat, the ground's frame z
// bends: s^2 / 4R over a cell of side s, 1 cm at 500 m, 0.6 m on the 4 km land beyond). False, and why, at the first
// point that does not.
inline bool DrawnGroundAgrees(const MapData& map, const MapFrameLink& link, int samples, std::string* why = nullptr) {
    const std::vector<const MapGrid*> levels = map.DrawnLevels();
    std::uint64_t seed = 0x52355EEDULL;
    const auto uniform = [&seed]() {
        seed = noise::Mix64(seed);
        return static_cast<double>(seed >> 11) * (1.0 / 9007199254740992.0);
    };
    char text[256];
    for (std::size_t li = 0; li < levels.size(); ++li) {
        const MapGrid& g = *levels[li];
        const double sag_m = 1.1 * g.spacing_m * g.spacing_m / (4.0 * wgs84::kMeanRadius) + 2e-3;
        const double width = g.spacing_m * (g.cols - 1), height = g.spacing_m * (g.rows - 1);
        int done = 0;
        for (int tried = 0; done < samples && tried < 100 * samples; ++tried) {
            const double e = g.west_m + uniform() * width, n = g.south_m + uniform() * height;
            if (map.LevelAt(e, n) != static_cast<int>(li)) continue;
            int c = 0, r = 0;
            g.CellOf(e, n, c, r);
            if (!map.DrawsCell(li, c, r)) {
                std::snprintf(text, sizeof(text), "level %zu: the ground at (%.0f, %.0f) is a cell it does not draw", li, e, n);
                if (why != nullptr) *why = text;
                return false;
            }
            const double tx = (e - g.EastOf(c)) / g.spacing_m, ty = (n - g.NorthOf(r)) / g.spacing_m;
            // The triangle's corners (map east, north) and weights at the point.
            int cc[3], rr[3];
            double wt[3];
            if (tx + ty <= 1.0) {
                cc[0] = c; rr[0] = r; cc[1] = c + 1; rr[1] = r; cc[2] = c; rr[2] = r + 1;
                wt[0] = 1.0 - tx - ty; wt[1] = tx; wt[2] = ty;
            } else {
                cc[0] = c + 1; rr[0] = r + 1; cc[1] = c; rr[1] = r + 1; cc[2] = c + 1; rr[2] = r;
                wt[0] = tx + ty - 1.0; wt[1] = 1.0 - tx; wt[2] = 1.0 - ty;
            }
            double h_tri = 0.0;
            Vec3 corner[3];
            for (int k = 0; k < 3; ++k) {
                const double hk = g.At(cc[k], rr[k]);
                h_tri += wt[k] * hk;
                corner[k] = link.GroundPoint(g.EastOf(cc[k]), g.NorthOf(rr[k]), hk);
            }
            const double h = map.GroundHeight(e, n);
            if (std::fabs(h - h_tri) > 1e-3) {
                std::snprintf(text, sizeof(text), "level %zu at (%.0f, %.0f): the sim's ground %.3f m, the drawn triangle's %.3f m", li, e, n, h, h_tri);
                if (why != nullptr) *why = text;
                return false;
            }
            // The drawn triangle in the scenario's frame, at the point's (x, y): weights from the corners' x, y.
            double x = 0.0, y = 0.0;
            link.MapToFrameEN(e, n, x, y);
            const Vec3 a = corner[0], b = corner[1], d = corner[2];
            const double det = (b.x - a.x) * (d.y - a.y) - (d.x - a.x) * (b.y - a.y);
            const double u = ((x - a.x) * (d.y - a.y) - (d.x - a.x) * (y - a.y)) / det;
            const double v = ((b.x - a.x) * (y - a.y) - (x - a.x) * (b.y - a.y)) / det;
            const double z_drawn = a.z + u * (b.z - a.z) + v * (d.z - a.z);
            const double z_sim = GroundPointInFrame(link.Frame(), x, y, h).z;
            if (std::fabs(z_drawn - z_sim) > sag_m) {
                std::snprintf(text, sizeof(text), "level %zu at (%.0f, %.0f): the sim's ground point is %.3f m off the drawn triangle (allowed %.3f)", li,
                              e, n, z_sim - z_drawn, sag_m);
                if (why != nullptr) *why = text;
                return false;
            }
            ++done;
        }
        if (done < samples) {
            std::snprintf(text, sizeof(text), "level %zu: only %d of %d points where it is the ground", li, done, samples);
            if (why != nullptr) *why = text;
            return false;
        }
    }
    return true;
}

// Roadmap R5's other half: no ground drawn twice. Every cell a level draws (DrawsCell) shares no area with any finer
// level, so no coarse triangle can lie over (and hide) the finer ground the sim stands things on, or under it. False,
// and why, at the first cell that does.
inline bool NoGroundDrawnTwice(const MapData& map, std::string* why = nullptr) {
    const std::vector<const MapGrid*> levels = map.DrawnLevels();
    char text[256];
    for (std::size_t li = 1; li < levels.size(); ++li) {
        const MapGrid& g = *levels[li];
        for (int r = 0; r + 1 < g.rows; ++r) {
            for (int c = 0; c + 1 < g.cols; ++c) {
                const double w = g.EastOf(c), e = g.EastOf(c + 1), s = g.NorthOf(r), n = g.NorthOf(r + 1);
                for (std::size_t j = 0; j < li; ++j) {
                    const MapGrid& f = *levels[j];
                    const double fe = f.EastOf(f.cols - 1), fn = f.NorthOf(f.rows - 1);
                    const double across = std::min(e, fe) - std::max(w, f.west_m), up = std::min(n, fn) - std::max(s, f.south_m);
                    if (across <= 1e-6 || up <= 1e-6) continue;  // no area in common
                    if (!map.DrawsCell(li, c, r)) break;           // wholly inside a finer level: not drawn
                    std::snprintf(text, sizeof(text), "level %zu's cell (%.0f..%.0f, %.0f..%.0f) is drawn and overlaps level %zu by %.0f x %.0f m",
                                  li, w, e, s, n, j, across, up);
                    if (why != nullptr) *why = text;
                    return false;
                }
            }
        }
    }
    return true;
}

// The map's ground as a scenario's terrain: heights above sea level under the scenario's (east, north).
class MapTerrain final : public Terrain {
public:
    MapTerrain(std::shared_ptr<const MapData> map, const GeoPoint& frame_origin)
        : map_(std::move(map)), link_(map_->Def().origin, frame_origin) {}

    double HeightAt(double east_m, double north_m) const override {
        const MapPoint p = link_.FrameToMap(east_m, north_m);
        return map_->GroundHeight(p.east_m, p.north_m);
    }
    double ObstacleHeightAt(double east_m, double north_m) const override {
        const MapPoint p = link_.FrameToMap(east_m, north_m);
        return map_->ObstacleHeight(p.east_m, p.north_m);
    }
    // The map's boxes as footprints (roadmap R6): a path looks at them at their own spacing (terrain.hpp ObstaclesAlong).
    // The scenario's frame and the map's are an affine pair, near enough a rotation, so a segment stays a segment and
    // a footprint's sides keep their lengths.
    double ObstacleSpacingM() const override { return map_->NarrowestObstacle(); }
    double ObstacleTopCeilingM() const override { return map_->TallestObstacleTop(); }
    double ObstacleTallestM() const override { return map_->TallestObstacle(); }
    bool WaterAt(double east_m, double north_m) const override {
        const MapPoint p = link_.FrameToMap(east_m, north_m);
        return map_->WaterAt(p.east_m, p.north_m);
    }
    bool HasWater() const override { return map_->HasWater(); }
    double ObstacleTopAt(double east_m, double north_m) const override {
        const MapPoint p = link_.FrameToMap(east_m, north_m);
        return map_->ObstacleTop(p.east_m, p.north_m);
    }
    double ObstacleTopBoundM(double e0, double n0, double e1, double n1, double below_m, double& spacing_m) const override {
        const MapPoint a = link_.FrameToMap(e0, n0), b = link_.FrameToMap(e1, n1);
        return map_->ObstacleTopBound(a.east_m, a.north_m, b.east_m, b.north_m, below_m, spacing_m);
    }
    bool ObstacleChords(double e0, double n0, double e1, double n1, double below_m, ObstacleChordSink& sink) const override {
        const MapPoint a = link_.FrameToMap(e0, n0), b = link_.FrameToMap(e1, n1);  // affine: a fraction stays a fraction
        map_->ObstacleChords(a.east_m, a.north_m, b.east_m, b.north_m, below_m, sink);
        return true;
    }
    // The tallest box whose footprint comes within `radius_m` of the scenario's (east, north) (MapData::ObstacleHeightWithin).
    double ObstacleHeightWithin(double east_m, double north_m, double radius_m) const {
        const MapPoint p = link_.FrameToMap(east_m, north_m);
        return map_->ObstacleHeightWithin(p.east_m, p.north_m, radius_m);
    }
    // The nearest point of a road's centre line to the scenario's (east, north), within `within_m` of it (ENU; its z is
    // not the ground's), and that road's width. False if no road is that near.
    bool NearestRoad(double east_m, double north_m, double within_m, Vec3& centre, double& width_m) const {
        const MapPoint p = link_.FrameToMap(east_m, north_m);
        double best = within_m;
        bool found = false;
        MapPoint at;
        for (const MapLineDef& r : map_->Def().roads) {
            for (std::size_t i = 1; i < r.points.size(); ++i) {
                const MapPoint& a = r.points[i - 1];
                const double de = r.points[i].east_m - a.east_m, dn = r.points[i].north_m - a.north_m;
                const double len2 = de * de + dn * dn;
                const double t = len2 > 0.0 ? std::clamp(((p.east_m - a.east_m) * de + (p.north_m - a.north_m) * dn) / len2, 0.0, 1.0) : 0.0;
                const double qe = a.east_m + t * de, qn = a.north_m + t * dn;
                const double d = std::hypot(p.east_m - qe, p.north_m - qn);
                if (d < best) {
                    best = d;
                    at = MapPoint{qe, qn};
                    width_m = r.width_m;
                    found = true;
                }
            }
        }
        if (found) centre = link_.MapToFrame(at.east_m, at.north_m, 0.0);
        return found;
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
                             "forests", "woods", "marshes", "lakes", "sites", "detail_settlements"}) ||
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
               LoadSites(root, out) && LoadDetail(root, out) && LoadSettlements(root, "detail_settlements", out.detail_settlements) &&
               DetailTownsInAreas(out);
    }

    // The ground another file shapes (a country's, named as a district's "far_ground"): its origin, seed, terrain, rivers
    // and lakes; and its roads and railways (imported into the district's detail areas) and its towns (their places,
    // radii and kinds: the clutter of those the district does not lay out). The rest of that file is its own loader's
    // business.
    bool LoadGround(const json::Value& root, MapDef& out) {
        if (!OriginAndSeed(root, out) || !LoadTerrain(root, out) || !LoadLines(root, "rivers", "river", out.rivers) ||
            !LoadLakes(root, out.lakes) || !LoadLines(root, "roads", "", out.roads) || !LoadLines(root, "railways", "rail", out.railways)) {
            return false;
        }
        if (const json::Value* list = OptionalArray(root, "", "settlements")) {
            for (std::size_t i = 0; i < list->items.size(); ++i) {
                const json::Value& v = list->items[i];
                const std::string p = Index("settlements", i);
                MapSettlementDef s;
                double r = 0.5;
                if (!RequireString(v, p, "id", s.id) || !RequireString(v, p, "kind", s.kind) || !Centre(v, p, s.centre) ||
                    !Number(v, p, "radius_km", r) || !OptionalString(v, p, "name", s.name)) {
                    return false;
                }
                s.radius_m = r * 1000.0;
                out.settlements.push_back(std::move(s));
            }
        } else if (!Message().empty()) {
            return false;
        }
        return true;
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
                                  "detail", "notes"}) ||
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

    bool LoadSettlements(const json::Value& root, MapDef& out) { return LoadSettlements(root, "settlements", out.settlements); }

    bool LoadSettlements(const json::Value& root, const char* key, std::vector<MapSettlementDef>& into) {
        const json::Value* list = OptionalArray(root, "", key);
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index(key, i);
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
            into.push_back(std::move(s));
        }
        return true;
    }

    // terrain.detail: the detail areas (roadmap R6). Each is a level of the drawn ground, so its edges must fall on the
    // nodes of the level it lies in (the map proper's, or the far ground's beyond it) and it lies wholly in one of them;
    // its spacing divides that level's; the areas do not overlap.
    bool LoadDetail(const json::Value& root, MapDef& out) {
        const json::Value* t = root.Find("terrain");
        if (t == nullptr) return true;
        const json::Value* list = OptionalArray(*t, "terrain", "detail");
        if (list == nullptr) return Message().empty();
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = Index("terrain.detail", i);
            const json::Value& v = list->items[i];
            MapDetailDef d;
            double half_km = 5.0;
            if (!Keys(v, p, {"id", "east_km", "north_km", "half_km", "spacing_m", "micro_m", "source", "confidence", "notes"}) ||
                !RequireString(v, p, "id", d.id) || !Centre(v, p, d.centre) || !Number(v, p, "half_km", half_km) ||
                !Number(v, p, "spacing_m", d.spacing_m) || !OptionalNumber(v, p, "micro_m", d.micro_m)) {
                return false;
            }
            d.half_m = half_km * 1000.0;
            if (d.spacing_m < 5.0 || d.spacing_m > out.core_spacing_m) return Fail(Join(p, "spacing_m"), "needs 5 m <= spacing_m <= core_spacing_m");
            if (d.micro_m < 0.0 || d.micro_m > 1.5) return Fail(Join(p, "micro_m"), "needs 0 <= micro_m <= 1.5");
            const double w = d.centre.east_m - d.half_m, e = d.centre.east_m + d.half_m;
            const double s = d.centre.north_m - d.half_m, n = d.centre.north_m + d.half_m;
            const bool in_core = out.InCore(w, s) && out.InCore(e, n);
            const bool outside_core = w >= out.east_m || e <= out.west_m || s >= out.north_m || n <= out.south_m;
            if (!in_core && !outside_core) return Fail(p, "must lie wholly inside the map proper or wholly outside it");
            if (std::max({std::fabs(w), std::fabs(e), std::fabs(s), std::fabs(n)}) > out.far_m) return Fail(p, "must lie inside the far ground");
            const double coarse = in_core ? out.core_spacing_m : out.far_spacing_m;
            const double west0 = in_core ? out.west_m : -out.far_m, south0 = in_core ? out.south_m : -out.far_m;
            const auto on_node = [](double v0, double step) { return std::fabs(v0 / step - std::round(v0 / step)) < 1e-9; };
            if (!on_node(w - west0, coarse) || !on_node(e - west0, coarse) || !on_node(s - south0, coarse) || !on_node(n - south0, coarse)) {
                return Fail(p, "its edges must fall on the nodes of the ground it lies in (every " + std::to_string(static_cast<int>(coarse)) + " m)");
            }
            if (!on_node(coarse, d.spacing_m)) return Fail(Join(p, "spacing_m"), "must divide the spacing of the ground it lies in");
            for (const MapDetailDef& o : out.detail) {
                if (o.id == d.id) return Fail(Join(p, "id"), "'" + d.id + "' is listed twice");
                if (std::fabs(o.centre.east_m - d.centre.east_m) < o.half_m + d.half_m && std::fabs(o.centre.north_m - d.centre.north_m) < o.half_m + d.half_m) {
                    return Fail(p, "overlaps the detail area '" + o.id + "'");
                }
            }
            out.detail.push_back(std::move(d));
        }
        return true;
    }

    // A detail area's town lies inside one (its streets are the area's lines, its ground the area's).
    bool DetailTownsInAreas(const MapDef& out) {
        for (std::size_t i = 0; i < out.detail_settlements.size(); ++i) {
            if (!out.InDetail(out.detail_settlements[i].centre.east_m, out.detail_settlements[i].centre.north_m)) {
                return Fail(Index("detail_settlements", i), "must lie in a detail area (terrain.detail)");
            }
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
    // The representative clutter heights of the far ground's towns (data/gameplay/clutter.json; none without it).
    if (!LoadClutterHeights(data_dir, def.clutter, error)) return nullptr;
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
        // Its extent and grid, as the country is drawn beyond the far ground (country_map.hpp reads them the same way).
        // Without an extent no country is drawn around the map: no lattices beyond the far ground (far_grid_m 0).
        bool extent = false;
        if (const json::Value* ext = root.Find("extent_km")) {
            const json::Value* w = ext->Find("west");
            const json::Value* e = ext->Find("east");
            const json::Value* s = ext->Find("south");
            const json::Value* n = ext->Find("north");
            if (w != nullptr && e != nullptr && s != nullptr && n != nullptr && e->number > w->number && n->number > s->number) {
                ground->west_m = w->number * 1000.0;
                ground->east_m = e->number * 1000.0;
                ground->south_m = s->number * 1000.0;
                ground->north_m = n->number * 1000.0;
                extent = true;
            }
        }
        if (const json::Value* g = root.Find("grid_km")) {
            if (g->Is(json::Type::Number) && g->number >= 0.1 && g->number <= 10.0) def.far_grid_m = g->number * 1000.0;
        }
        if (!extent) def.far_grid_m = 0.0;
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
