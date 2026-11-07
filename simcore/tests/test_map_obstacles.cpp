// Roadmap E9 (in R6): woods and buildings on the map stand in the way of a line of sight and a radio path (radar.hpp
// HasLineOfSight, em/propagation.hpp AnalysePath, through Terrain::ObstacleHeightAt), except within 60 m of either end;
// thin and flat parts do not; the far ground's woods and towns count too; a low pilot holds his height over the canopy.
// Synthetic terrains use placeholder numbers, not a real system's.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>

#include "simcore/aircraft.hpp"
#include "simcore/em/propagation.hpp"
#include "simcore/geo.hpp"
#include "simcore/radar.hpp"
#include "simcore/target_behaviour.hpp"
#include "simcore/terrain.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::shared_ptr<const MapData> KesmarkMap() {
    std::string e;
    std::shared_ptr<const MapData> m = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    if (!m) std::printf("    %s\n", e.c_str());
    return m;
}

// Flat ground with obstacles where a function says (a wood, a strip, a forest everywhere).
class ObstacleTerrain final : public Terrain {
public:
    using Fn = double (*)(double, double);
    ObstacleTerrain(double ground_m, Fn fn) : ground_m_(ground_m), fn_(fn) {}
    double HeightAt(double, double) const override { return ground_m_; }
    double ObstacleHeightAt(double e, double n) const override { return fn_(e, n); }

private:
    double ground_m_;
    Fn fn_;
};

double Strip20(double e, double) { return std::fabs(e - 5000.0) < 100.0 ? 20.0 : 0.0; }
double Around20(double e, double n) { return std::hypot(e, n) < 50.0 ? 20.0 : 0.0; }
double Forest20(double, double) { return 20.0; }

}  // namespace

SC_TEST("obstacles: a ray through the Lazne woods 8 m up is blocked, 40 m up it is clear; on bare ground it is clear") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    // The canopy block nearest the woods' middle (found among the boxes, not a fixed point).
    const MapBox* wood = nullptr;
    double best = 1e18;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::Canopy) continue;
        const double d = std::hypot(b.east_m + 11000.0, b.north_m + 3000.0);
        if (d < best) {
            best = d;
            wood = &b;
        }
    }
    SC_EXPECT(wood != nullptr && best < 300.0);
    if (wood == nullptr) return;
    const MapTerrain ground(m, m->Def().origin);  // the map's own frame: the scenario's east and north are the map's
    const Terrain& t = ground;
    SC_EXPECT_NEAR(t.ObstacleHeightAt(wood->east_m, wood->north_m), m->ObstacleHeight(wood->east_m, wood->north_m), 1e-6);
    SC_EXPECT(t.ObstacleHeightAt(wood->east_m, wood->north_m) >= m->Def().canopy_min_m - 0.5);
    const Vec3 a{wood->east_m - 300.0, wood->north_m, 0.0}, b{wood->east_m + 300.0, wood->north_m, 0.0};
    const double ga = t.HeightAt(a.x, a.y), gb = t.HeightAt(b.x, b.y);
    double top = std::max(ga, gb);
    for (double s = 0.0; s <= 600.0; s += 10.0) top = std::max(top, t.HeightAt(a.x + s, a.y));
    std::printf("    the block at (%.0f, %.0f): canopy %.1f m over the ground\n", wood->east_m, wood->north_m,
                t.ObstacleHeightAt(wood->east_m, wood->north_m));
    SC_EXPECT(!HasLineOfSight(t, a, ga + 8.0, b, gb + 8.0));
    SC_EXPECT(HasLineOfSight(t, a, top + 40.0, b, top + 40.0));
    // The same geometry over the ground alone (a terrain with no obstacles): clear.
    const FlatTerrain bare(std::max(ga, gb));
    SC_EXPECT(HasLineOfSight(bare, a, bare.HeightAt(0, 0) + 8.0, b, bare.HeightAt(0, 0) + 8.0));
    // The radio path's profile sees the same wood (AnalysePath), and not without it.
    const em::PathGeometry g = em::AnalysePath(t, a, ga + 8.0, b, gb + 8.0, 25.0);
    SC_EXPECT(!g.line_of_sight);
}

SC_TEST("obstacles: woods within 60 m of an end do not hide it; with no skip they would") {
    const ObstacleTerrain around(0.0, Around20);
    const Vec3 a{0.0, 0.0, 2.0}, b{2000.0, 0.0, 300.0};
    SC_EXPECT(HasLineOfSight(around, a, 2.0, b, 300.0, 10.0));
    SC_EXPECT(!HasLineOfSight(around, a, 2.0, b, 300.0, 10.0, kStandardRefractionK, 0.0, 0.0));
    SC_EXPECT(em::AnalysePath(around, a, 2.0, b, 300.0, 10.0).line_of_sight);
    SC_EXPECT(!em::AnalysePath(around, a, 2.0, b, 300.0, 10.0, 600, kStandardRefractionK, 0.0).line_of_sight);
}

SC_TEST("obstacles: a 20 m strip of woods across a flat path blocks it and makes a diffraction edge (P.1812 clutter)") {
    const ObstacleTerrain strip(0.0, Strip20);
    const FlatTerrain flat(0.0);
    const Vec3 a{0.0, 0.0, 5.0}, b{10000.0, 0.0, 5.0};
    const em::PathGeometry bare = em::AnalysePath(flat, a, 5.0, b, 5.0, 50.0);
    const em::PathGeometry wooded = em::AnalysePath(strip, a, 5.0, b, 5.0, 50.0);
    SC_EXPECT(bare.line_of_sight && bare.worst_v_geometry < 0.0);
    SC_EXPECT(!wooded.line_of_sight && wooded.worst_v_geometry > 0.0);
    // Hand calculation: the edge 20 m up at 5 km on the Earth's bulge (1.47 m at k 4/3), the ray at 5 m:
    // h = 16.5 m, h sqrt(1/5000 + 1/5000) = 0.33.
    SC_EXPECT_NEAR(wooded.worst_v_geometry, (20.0 - 5.0 + 5000.0 * 5000.0 / (2.0 * kStandardRefractionK * wgs84::kMeanRadius)) *
                                                std::sqrt(2.0 / 5000.0),
                   0.01);
    SC_EXPECT(!HasLineOfSight(strip, a, 5.0, b, 5.0));
}

SC_TEST("obstacles: masts, chimneys, towers, launchers and tents are not; the far ground's woods and towns are") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    const MapDef& d = m->Def();
    // An Osa position's radio mast (16 m, 0.6 m across) stands off its pad: the cell it stands in is open.
    int masts = 0;
    for (const MapSiteDef& s : d.sites) {
        if (s.kind != "osa_position" || !d.InCore(s.centre.east_m, s.centre.north_m)) continue;
        const double e = s.centre.east_m + 45.0, n = s.centre.north_m + 30.0;
        std::printf("    %s's mast: obstacle %.1f m\n", s.id.c_str(), m->ObstacleHeight(e, n));
        SC_EXPECT(m->ObstacleHeight(e, n) < 3.0);
        ++masts;
    }
    SC_EXPECT(masts >= 1);
    // A wood block of the far ground (beyond the map proper and the detail areas): its canopy over the ground.
    const MapBox* far_wood = nullptr;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part == MapPart::Canopy && !d.InCore(b.east_m, b.north_m) && !d.InDetail(b.east_m, b.north_m) && b.length_m == d.far_block_m) {
            far_wood = &b;
            break;
        }
    }
    SC_EXPECT(far_wood != nullptr);
    if (far_wood != nullptr) {
        const double h = m->ObstacleHeight(far_wood->east_m, far_wood->north_m);
        std::printf("    a far wood at (%.0f, %.0f): %.1f m\n", far_wood->east_m, far_wood->north_m, h);
        SC_EXPECT(h >= d.canopy_min_m - 0.5 && h <= d.canopy_max_m + 0.5);
    }
    // Over the far ground on a 5 km lattice: mostly open, some woods, and nothing standing where no wood or town is.
    std::unordered_set<long long> woods;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part == MapPart::Canopy && !d.InCore(b.east_m, b.north_m)) {
            woods.insert(static_cast<long long>(std::floor(b.east_m / d.far_block_m)) * 1000003LL + static_cast<long long>(std::floor(b.north_m / d.far_block_m)));
        }
    }
    int open = 0, wooded = 0, towns = 0, stray = 0;
    for (double n = -145000.0; n <= 145000.0; n += 5000.0) {
        for (double e = -145000.0; e <= 145000.0; e += 5000.0) {
            if (d.InCore(e, n) || d.InDetail(e, n)) continue;
            const double h = m->ObstacleHeight(e, n);
            const long long key = static_cast<long long>(std::floor(e / d.far_block_m)) * 1000003LL + static_cast<long long>(std::floor(n / d.far_block_m));
            bool in_town = false;
            for (const MapSettlementDef& s : d.far_ground->settlements) in_town = in_town || std::hypot(e - s.centre.east_m, n - s.centre.north_m) < s.radius_m;
            if (h <= 0.0) {
                ++open;
            } else if (woods.count(key) != 0) {
                ++wooded;
            } else if (in_town) {
                ++towns;
            } else {
                // Something else the map lays out beyond the map proper (a site, a farm) standing in the 50 m cell.
                bool laid = false;
                for (const MapBox& b : m->Layout().boxes) {
                    if (std::fabs(b.east_m - e) < 50.0 + 0.5 * std::max(b.length_m, b.width_m) &&
                        std::fabs(b.north_m - n) < 50.0 + 0.5 * std::max(b.length_m, b.width_m) && h <= b.TopAt(e, n) - m->GroundHeight(e, n) + 1e-6) {
                        laid = true;
                        break;
                    }
                }
                if (laid) ++towns;
                else ++stray;
            }
        }
    }
    std::printf("    the far ground every 5 km: %d open, %d in woods, %d in towns, %d other\n", open, wooded, towns, stray);
    SC_EXPECT(open > 2000 && wooded > 10 && stray == 0);
    // A town of the far ground the map does not lay out (Dravec): its middle dense urban, its edge suburban
    // (data/gameplay/clutter.json), unless a wood stands there.
    for (const MapSettlementDef& s : d.far_ground->settlements) {
        if (s.id != "dravec") continue;
        SC_EXPECT(d.clutter.dense_urban_m == 20.0 && d.clutter.suburban_m == 10.0);
        const double mid = m->ObstacleHeight(s.centre.east_m, s.centre.north_m);
        const double edge = m->ObstacleHeight(s.centre.east_m + 0.8 * s.radius_m, s.centre.north_m);
        const double out = m->ObstacleHeight(s.centre.east_m + 1.2 * s.radius_m, s.centre.north_m);
        std::printf("    Dravec: %.1f m in the middle, %.1f m at 0.8 of its radius, %.1f m beyond\n", mid, edge, out);
        SC_EXPECT(mid == 20.0 || mid >= d.canopy_min_m - 0.5);
        SC_EXPECT(edge == 10.0 || edge >= d.canopy_min_m - 0.5);
    }
}

SC_TEST("obstacles: what is laid beyond the map proper (woods running out of it, the towns laid there) stands no higher than its obstacle") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    const MapDef& d = m->Def();
    // Every opaque box beyond the map proper and the detail areas, at its centre and at its corners 1 m in: the ground
    // plus the obstacle there reaches its top.
    int boxes = 0, over = 0, houses = 0, small_woods = 0, far_town = 0;
    double worst = 0.0;
    for (const MapBox& b : m->Layout().boxes) {
        if (d.InCore(b.east_m, b.north_m) || d.InDetail(b.east_m, b.north_m)) continue;
        if (b.part == MapPart::Marsh || b.part == MapPart::Pad || b.part == MapPart::Runway || b.part == MapPart::Mast ||
            b.part == MapPart::Chimney || b.part == MapPart::Tower || b.part == MapPart::Launcher || b.part == MapPart::Tent) {
            continue;
        }
        ++boxes;
        houses += b.part == MapPart::House ? 1 : 0;
        small_woods += b.part == MapPart::Canopy && b.length_m < d.far_block_m ? 1 : 0;
        // A wood on a slope has a sloping top (A.3.20): its obstacle follows the plane, so the plane is what it reaches.
        const double sh = std::sin(b.heading_rad), ch = std::cos(b.heading_rad);
        const double hl = std::max(0.0, 0.5 * b.length_m - 1.0), hw = std::max(0.0, 0.5 * b.width_m - 1.0);
        const double at[5][2] = {{0.0, 0.0}, {hl, hw}, {hl, -hw}, {-hl, hw}, {-hl, -hw}};
        bool short_of = false;
        for (const auto& p : at) {
            const double e = b.east_m + p[0] * sh + p[1] * ch, n = b.north_m + p[0] * ch - p[1] * sh;
            if (d.InCore(e, n) || d.InDetail(e, n)) continue;
            const double gap = b.TopAt(e, n) - (m->GroundHeight(e, n) + m->ObstacleHeight(e, n));
            worst = std::max(worst, gap);
            short_of = short_of || gap > 1e-6;
        }
        over += short_of ? 1 : 0;
    }
    // A town the district lays out beyond the map proper (Novo Varosd): its houses block sight.
    for (const MapSettlementDef& s : d.settlements) {
        if (s.id != "novo_varosd") continue;
        for (const MapBox& b : m->Layout().boxes) {
            if (b.part != MapPart::House || std::hypot(b.east_m - s.centre.east_m, b.north_m - s.centre.north_m) > s.radius_m) continue;
            ++far_town;
            SC_EXPECT(m->ObstacleHeight(b.east_m, b.north_m) >= b.base_m + b.height_m - m->GroundHeight(b.east_m, b.north_m) - 1e-6);
        }
    }
    std::printf("    beyond the map proper: %d boxes (%d houses, %d small wood blocks), %d short of their obstacle (worst %.2f m); "
                "%d houses in Novo Varosd\n",
                boxes, houses, small_woods, over, worst, far_town);
    SC_EXPECT(boxes > 1000 && houses > 1000 && small_woods > 1000 && far_town > 100);
    SC_EXPECT(over == 0);
}

SC_TEST("obstacles: no house or block of flats stands within a site's clearance, however far from the site's centre") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    int inside = 0, checked = 0;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::House && b.part != MapPart::Flats) continue;
        ++checked;
        for (const MapSiteDef& s : m->Def().sites) {
            if (s.kind == "tower") continue;  // a church tower's own nave stands 17 m off it
            const double r = MapData::ClearanceOf(s);
            if (std::hypot(b.east_m - s.centre.east_m, b.north_m - s.centre.north_m) >= r) continue;
            if (inside < 5) std::printf("    a %s %.0f m from %s (clearance %.0f m)\n", MapPartName(b.part),
                                        std::hypot(b.east_m - s.centre.east_m, b.north_m - s.centre.north_m), s.id.c_str(), r);
            ++inside;
        }
    }
    std::printf("    %d houses and flats, %d inside a site's clearance\n", checked, inside);
    SC_EXPECT(checked > 10000);
    SC_EXPECT(inside == 0);
}

SC_TEST("obstacles: a terrain-following pilot holds his height over the forest's canopy, not over the ground under it") {
    const LocalFrame frame(GeoPoint::FromDegrees(47.0, 20.0, 0.0));
    const ObstacleTerrain forest(100.0, Forest20);
    const FlatTerrain bare(100.0);
    for (const Terrain* t : {static_cast<const Terrain*>(&forest), static_cast<const Terrain*>(&bare)}) {
        TargetBehaviourDef b;
        b.kind = TargetBehaviourDef::Kind::TerrainFollowing;
        b.agl_m = 30.0;
        b.max_climb_mps = 20.0;
        TargetPilot pilot(b, EvadeDef());
        Aircraft a;
        a.type = "Air+FixedWing";
        a.bottom_m = 1.5;
        a.speed_mps = 200.0;
        a.position = GroundPointInFrame(frame, 0.0, 0.0, 130.0);
        a.waypoints = {GroundPointInFrame(frame, 30000.0, 0.0, 130.0)};
        for (double s = 0.0; s < 60.0; s += 0.1) {
            pilot.BeforeStep(a);
            StepAircraft(a, 0.1);
            pilot.AfterStep(a, *t, frame, 0.1, s);
        }
        const double over = frame.AltitudeOf(a.position) - t->HeightAt(a.position.x, a.position.y) - t->ObstacleHeightAt(a.position.x, a.position.y);
        std::printf("    %s: %.1f m over the %s\n", t == &forest ? "forest" : "bare ground", over, t == &forest ? "canopy" : "ground");
        SC_EXPECT_NEAR(over, 30.0, 1.0);
    }
}

namespace {

// Flat ground with one box standing on it as a footprint (terrain.hpp's footprint interface, as MapTerrain gives the
// map's boxes): 12 m wide across east, 30 m long along north, 20 m tall, centred at (east, north) = (0, 0). Placeholder
// numbers.
class OneBoxTerrain final : public Terrain {
public:
    static constexpr double kHalfE = 6.0, kHalfN = 15.0, kTop = 20.0;
    double HeightAt(double, double) const override { return 0.0; }
    double ObstacleHeightAt(double e, double n) const override { return Holds(e, n) ? kTop : 0.0; }
    double ObstacleSpacingM() const override { return 2.0 * kHalfE; }
    double ObstacleTopAt(double e, double n) const override { return Holds(e, n) ? kTop : kNoObstacleTop; }
    double ObstacleTopBoundM(double e0, double n0, double e1, double n1, double, double& spacing_m) const override {
        spacing_m = 2.0 * kHalfE;
        const bool apart = std::max(e0, e1) < -kHalfE || std::min(e0, e1) > kHalfE || std::max(n0, n1) < -kHalfN || std::min(n0, n1) > kHalfN;
        return apart ? kNoObstacleTop : kTop;
    }

private:
    static bool Holds(double e, double n) { return std::fabs(e) <= kHalfE && std::fabs(n) <= kHalfN; }
};

// The ground alone of a terrain (its woods and buildings left out).
class GroundOnly final : public Terrain {
public:
    explicit GroundOnly(const Terrain& t) : t_(t) {}
    double HeightAt(double e, double n) const override { return t_.HeightAt(e, n); }

private:
    const Terrain& t_;
};

}  // namespace

// Roadmap R6's review: a box masks no wider than it stands. Before, the obstacle grid stamped every node within half a
// cell of a footprint and was sampled between its nodes, so a building masked about 12 m wider on each side; and a path
// sampled every 50-100 m could step over a narrow one. Now a path looks the footprints up between its samples at their
// own spacing (terrain.hpp ObstaclesAlong).
SC_TEST("obstacles: a 12 m-wide, 20 m-tall box blocks a ray exactly where it stands, at any sampling; past its corner is clear") {
    const OneBoxTerrain box;
    for (const double spacing : {50.0, 100.0, 400.0}) {
        // Across it from west to east, 10 m up, 0.5 m inside and outside its north end; no sample of the path's own
        // spacing falls on it (the ends 1013 m west and 987 m east of it).
        const auto los = [&](double n, double up) {
            return HasLineOfSight(box, Vec3{-1013.0, n, 0.0}, up, Vec3{987.0, n, 0.0}, up, spacing);
        };
        const auto path = [&](double n, double up) {
            return em::AnalysePath(box, Vec3{-1013.0, n, 0.0}, up, Vec3{987.0, n, 0.0}, up, spacing).line_of_sight;
        };
        SC_EXPECT(!los(0.0, 10.0) && !los(14.5, 10.0) && los(15.5, 10.0) && los(-15.5, 10.0));
        SC_EXPECT(!path(14.5, 10.0) && path(15.5, 10.0));
        // Over its top: the bulge 1 km in is 0.06 m at k 4/3, so a ray 0.5 m over the roof is clear and 0.5 m under is not.
        SC_EXPECT(!los(0.0, 19.5) && los(0.0, 20.5));
        // Diagonally past its north-east corner, 1 m clear of it (the ray's nearest point to the corner); and cutting the
        // corner 8 m deep (16 m through it: a cut shorter than the footprint's spacing may be stepped over).
        const double c = std::sqrt(0.5);
        const auto diagonal = [&](double off) {
            const double pe = 6.0 + off * c, pn = 15.0 + off * c;  // the ray's nearest point to the corner
            return HasLineOfSight(box, Vec3{pe - 700.0 * c, pn + 700.0 * c, 0.0}, 10.0, Vec3{pe + 700.0 * c, pn - 700.0 * c, 0.0}, 10.0, spacing);
        };
        SC_EXPECT(diagonal(1.0) && !diagonal(-8.0));
    }
}

SC_TEST("obstacles: the map's buildings mask exactly their footprints, in the map proper and in a detail area") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    const MapDef& d = m->Def();
    const MapTerrain ground(m, d.origin);
    const Terrain& t = ground;
    const GroundOnly bare(t);
    // In each of the map proper and the capital's detail area, the house or block of flats furthest from any other opaque
    // box, among those standing over level ground.
    for (const bool detail : {false, true}) {
        const MapBox* pick = nullptr;
        double pick_clear = 0.0;
        std::vector<const MapBox*> opaque;
        for (const MapBox& b : m->Layout().boxes) {
            if (b.part != MapPart::Marsh && b.part != MapPart::Pad && b.part != MapPart::Runway && b.part != MapPart::Mast &&
                b.part != MapPart::Chimney && b.part != MapPart::Tower && b.part != MapPart::Launcher && b.part != MapPart::Tent) {
                opaque.push_back(&b);
            }
        }
        for (const MapBox* b : opaque) {
            if (b->part != MapPart::House && b->part != MapPart::Flats) continue;
            if (detail ? !d.InDetail(b->east_m, b->north_m) || d.InCore(b->east_m, b->north_m) : !d.InCore(b->east_m, b->north_m)) continue;
            double clear = 1e18;
            for (const MapBox* o : opaque) {
                if (o == b) continue;
                const double r = std::hypot(o->east_m - b->east_m, o->north_m - b->north_m) - 0.5 * std::hypot(o->length_m, o->width_m);
                clear = std::min(clear, r);
                if (clear < pick_clear) break;
            }
            if (clear > pick_clear) {
                pick_clear = clear;
                pick = b;
            }
        }
        SC_EXPECT(pick != nullptr && pick_clear > 150.0);
        if (pick == nullptr || pick_clear <= 150.0) continue;
        const MapBox& b = *pick;
        const double top = b.base_m + b.height_m;
        const double sh = std::sin(b.heading_rad), ch = std::cos(b.heading_rad);
        // A ray across its width (along its "across" axis), 3 m under its roof, `along` metres from its middle along its length.
        const double reach = std::min(140.0, pick_clear - 5.0);
        const auto ray = [&](double along, double up_from_top, double spacing) {
            const double pe = b.east_m + along * sh, pn = b.north_m + along * ch;  // on the box's length axis
            const Vec3 a{pe - reach * ch, pn + reach * sh, 0.0}, z{pe + reach * ch, pn - reach * sh, 0.0};
            SC_EXPECT(HasLineOfSight(bare, a, top + up_from_top, z, top + up_from_top, spacing));  // the ground is under it
            return HasLineOfSight(t, a, top + up_from_top, z, top + up_from_top, spacing);
        };
        const double half = 0.5 * b.length_m;
        std::printf("    %s: a %s %.1f x %.1f m, %.1f m tall, %.0f m clear of the rest\n", detail ? "the capital" : "the map proper",
                    MapPartName(b.part), b.length_m, b.width_m, b.height_m, pick_clear);
        for (const double spacing : {25.0, 100.0}) {
            SC_EXPECT(!ray(0.0, -3.0, spacing));
            SC_EXPECT(!ray(half - 0.5, -3.0, spacing) && !ray(-(half - 0.5), -3.0, spacing));
            SC_EXPECT(ray(half + 0.5, -3.0, spacing) && ray(-(half + 0.5), -3.0, spacing));  // 0.5 m past its end: clear
            SC_EXPECT(ray(0.0, 0.5, spacing));                                                // 0.5 m over its roof: clear
        }
        // The obstacle as a point: the box's own height inside it, none 0.5 m outside its side.
        const double out_e = b.east_m + (0.5 * b.width_m + 0.5) * ch, out_n = b.north_m - (0.5 * b.width_m + 0.5) * sh;
        SC_EXPECT_NEAR(m->ObstacleHeight(b.east_m, b.north_m), top - m->GroundHeight(b.east_m, b.north_m), 1e-6);
        SC_EXPECT(m->ObstacleHeight(out_e, out_n) == 0.0);
    }
}

namespace {

// OneBoxTerrain that knows its footprint's cuts exactly (Terrain::ObstacleChords, as MapTerrain does).
class OneBoxExact final : public Terrain {
public:
    double HeightAt(double, double) const override { return 0.0; }
    double ObstacleHeightAt(double e, double n) const override { return box_.ObstacleHeightAt(e, n); }
    double ObstacleSpacingM() const override { return box_.ObstacleSpacingM(); }
    double ObstacleTopAt(double e, double n) const override { return box_.ObstacleTopAt(e, n); }
    double ObstacleTopBoundM(double e0, double n0, double e1, double n1, double below, double& spacing_m) const override {
        return box_.ObstacleTopBoundM(e0, n0, e1, n1, below, spacing_m);
    }
    bool ObstacleChords(double e0, double n0, double e1, double n1, double below_m, ObstacleChordSink& sink) const override {
        if (OneBoxTerrain::kTop < below_m) return true;
        const double p0[2] = {e0, n0}, dp[2] = {e1 - e0, n1 - n0}, half[2] = {OneBoxTerrain::kHalfE, OneBoxTerrain::kHalfN};
        double t_in = 0.0, t_out = 1.0;
        for (int k = 0; k < 2; ++k) {
            if (std::fabs(dp[k]) < 1e-12) {
                if (std::fabs(p0[k]) > half[k]) return true;
                continue;
            }
            const double ta = (-half[k] - p0[k]) / dp[k], tb = (half[k] - p0[k]) / dp[k];
            t_in = std::max(t_in, std::min(ta, tb));
            t_out = std::min(t_out, std::max(ta, tb));
        }
        if (t_in <= t_out) sink.Chord(t_in, t_out, OneBoxTerrain::kTop, OneBoxTerrain::kTop);
        return true;
    }

private:
    OneBoxTerrain box_;
};

bool OpaqueForSight(MapPart p) {
    return p != MapPart::Marsh && p != MapPart::Pad && p != MapPart::Runway && p != MapPart::Mast && p != MapPart::Chimney &&
           p != MapPart::Tower && p != MapPart::Launcher && p != MapPart::Tent;
}

}  // namespace

// R6's second review: a ray cutting a building's corner was stepped over when its cut was shorter than the footprint's
// spacing (a quarter of the corner rays on the Kesmark map). A terrain that knows its footprints' cuts exactly is looked
// at at them, however short.
SC_TEST("obstacles: a ray cutting a box's corner 0.1 m deep is blocked, at any sampling; 0.1 m clear of it is not") {
    const OneBoxExact box;
    const double c = std::sqrt(0.5);
    for (const double spacing : {50.0, 100.0, 400.0}) {
        const auto diagonal = [&](double off, bool path) {
            const double pe = 6.0 + off * c, pn = 15.0 + off * c;  // the ray's nearest point to the north-east corner
            const Vec3 a{pe - 700.0 * c, pn + 700.0 * c, 0.0}, z{pe + 700.0 * c, pn - 700.0 * c, 0.0};
            return path ? em::AnalysePath(box, a, 10.0, z, 10.0, spacing).line_of_sight : HasLineOfSight(box, a, 10.0, z, 10.0, spacing);
        };
        for (const bool path : {false, true}) {
            SC_EXPECT(!diagonal(-0.1, path) && !diagonal(-2.0, path) && !diagonal(-8.0, path));
            SC_EXPECT(diagonal(0.1, path) && diagonal(1.0, path));
        }
        // Across it, as before: 0.5 m inside its end blocked, 0.5 m outside clear; under and over its roof.
        SC_EXPECT(!HasLineOfSight(box, Vec3{-1013.0, 14.5, 0.0}, 10.0, Vec3{987.0, 14.5, 0.0}, 10.0, spacing));
        SC_EXPECT(HasLineOfSight(box, Vec3{-1013.0, 15.5, 0.0}, 10.0, Vec3{987.0, 15.5, 0.0}, 10.0, spacing));
        SC_EXPECT(!HasLineOfSight(box, Vec3{-1013.0, 0.0, 0.0}, 19.5, Vec3{987.0, 0.0, 0.0}, 19.5, spacing));
        SC_EXPECT(HasLineOfSight(box, Vec3{-1013.0, 0.0, 0.0}, 20.5, Vec3{987.0, 0.0, 0.0}, 20.5, spacing));
    }
}

SC_TEST("obstacles: the Kesmark map's buildings and woods block a ray cutting a corner 0.3 m deep, 2 m under the roof") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    const MapTerrain ground(m, m->Def().origin);
    const Terrain& t = ground;
    const GroundOnly bare(t);
    const double c = std::sqrt(0.5);
    int tried = 0, stepped_los = 0, stepped_path = 0;
    std::size_t index = 0;
    for (const MapBox& b : m->Layout().boxes) {
        if (!OpaqueForSight(b.part) || (index++ % 5) != 0) continue;  // every fifth, for the time
        const double sh = std::sin(b.heading_rad), ch = std::cos(b.heading_rad);
        double pe = 0.0, pn = 0.0, de = 0.0, dn = 0.0;  // the cut's middle, inside it, and the ray's direction
        if (MapPartIsRound(b.part)) {
            const double r = 0.5 * b.length_m;
            pe = b.east_m + std::sqrt(std::max(0.0, r * r - 0.09));  // a 0.6 m chord at its side
            pn = b.north_m;
            dn = 1.0;
        } else {
            // The corner (+along, +across), the ray at 45 degrees to its sides cutting 0.3 m into it.
            const double hl = 0.5 * b.length_m, hw = 0.5 * b.width_m;
            const double ce = b.east_m + hl * sh + hw * ch, cn = b.north_m + hl * ch - hw * sh;
            pe = ce - (sh + ch) * c * 0.3;
            pn = cn - (ch - sh) * c * 0.3;
            de = (sh - ch) * c;
            dn = (ch + sh) * c;
        }
        const double up = b.TopAt(pe, pn) - 2.0;  // a wood on a slope: its top there (A.3.20)
        const double reach = 300.0;
        const Vec3 a{pe - reach * de, pn - reach * dn, 0.0}, z{pe + reach * de, pn + reach * dn, 0.0};
        if (!HasLineOfSight(bare, a, up, z, up, 100.0)) continue;  // the ground in the way: nothing to show
        if (t.ObstacleTopAt(pe, pn) < up) continue;
        ++tried;
        if (HasLineOfSight(t, a, up, z, up, 100.0)) ++stepped_los;
        if (em::AnalysePath(t, a, up, z, up, 100.0).line_of_sight) ++stepped_path;
    }
    std::printf("    %d corner and side cuts: %d stepped over by HasLineOfSight, %d by AnalysePath\n", tried, stepped_los, stepped_path);
    SC_EXPECT(tried > 1000);
    SC_EXPECT(stepped_los == 0 && stepped_path == 0);
}

// A wood on a hillside (terrain roadmap A.3.20): its top follows the ground's plane across the block, so it stands about
// its height over the ground at its uphill and its downhill edge alike (a level top was buried uphill, twice as tall
// downhill), and a ray along the slope just under that top is blocked across the block, just over it is not.
SC_TEST("obstacles: a wood on a slope stands its height over the ground across it, and a ray along its top meets it") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    const MapTerrain ground(m, m->Def().origin);
    const Terrain& t = ground;
    int sloped = 0, checked = 0, blocked = 0, cleared = 0;
    double worst_spread = 0.0, level_spread = 0.0;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::Canopy || !m->Def().InCore(b.east_m, b.north_m)) continue;
        const double g = std::hypot(b.rise_e, b.rise_n);
        if (g == 0.0) continue;
        ++sloped;
        if (g < 0.08 || checked >= 200) continue;
        // Uphill and downhill, 40 m from its centre along the slope.
        const double ue = b.rise_e / g, un = b.rise_n / g, d = 0.4 * b.length_m;
        const double hu = m->ObstacleHeight(b.east_m + ue * d, b.north_m + un * d), hd = m->ObstacleHeight(b.east_m - ue * d, b.north_m - un * d);
        if (hu <= 0.0 || hd <= 0.0) continue;  // another wood's corner, a clearing
        ++checked;
        worst_spread = std::max(worst_spread, std::fabs(hu - hd));
        level_spread = std::max(level_spread, 2.0 * d * g);
        // A ray along the slope, 1 m under the top and 1 m over it, from 300 m before the block to 300 m past it.
        const double reach = 0.5 * b.length_m + 300.0;
        const Vec3 a{b.east_m - ue * reach, b.north_m - un * reach, 0.0}, z{b.east_m + ue * reach, b.north_m + un * reach, 0.0};
        const double ta = b.TopAt(b.east_m - ue * 0.45 * b.length_m, b.north_m - un * 0.45 * b.length_m);
        const double tz = b.TopAt(b.east_m + ue * 0.45 * b.length_m, b.north_m + un * 0.45 * b.length_m);
        // The ray through those two points of the top, 1 m under and over, carried out to the ends.
        const double slope = (tz - ta) / (0.9 * b.length_m);
        const double za = ta - slope * (reach - 0.45 * b.length_m), zz = tz + slope * (reach - 0.45 * b.length_m);
        const GroundOnly bare(t);
        if (!HasLineOfSight(bare, a, za - 1.0, z, zz - 1.0, 100.0)) continue;  // the ground itself in the way
        blocked += HasLineOfSight(t, a, za - 1.0, z, zz - 1.0, 100.0) ? 0 : 1;
        cleared += HasLineOfSight(t, a, za + 1.0, z, zz + 1.0, 100.0) ? 1 : 0;
    }
    std::printf("    %d sloping wood blocks; %d checked: their height uphill and downhill differs by at most %.1f m (a level top: %.1f m); "
                "rays along their tops: %d blocked 1 m under, %d clear 1 m over\n",
                sloped, checked, worst_spread, level_spread, blocked, cleared);
    SC_EXPECT(sloped > 1000);
    SC_EXPECT(checked > 50);
    SC_EXPECT(worst_spread < 0.5 * level_spread);
    SC_EXPECT(blocked > 0);
}

// R6's second review: a raid's sites were judged open by looking at nine points 20 m round the spot, between which a
// house can stand. The clearance is looked at whole (MapData::ObstacleHeightWithin).
SC_TEST("obstacles: a house between the points round a spot is within its clearance") {
    const std::shared_ptr<const MapData> m = KesmarkMap();
    if (!m) return;
    const MapTerrain ground(m, m->Def().origin);
    int tried = 0;
    for (const MapBox& b : m->Layout().boxes) {
        if (b.part != MapPart::House || b.length_m > 12.0 || b.width_m > 12.0 || b.height_m <= 3.5) continue;
        // A spot 17 m from the house's middle, half-way between two of the ring's points (22.5 degrees off north): the
        // nearest ring points are about 7.8 m from the house's middle.
        const double bearing = 22.5 * 3.14159265358979 / 180.0;
        const double se = b.east_m - 17.0 * std::sin(bearing), sn = b.north_m - 17.0 * std::cos(bearing);
        const double k = 20.0, d = 0.7071 * 20.0;
        const double ring[9][2] = {{0.0, 0.0}, {k, 0.0}, {-k, 0.0}, {0.0, k}, {0.0, -k}, {d, d}, {d, -d}, {-d, d}, {-d, -d}};
        bool ring_sees = false;
        for (const auto& o : ring) ring_sees = ring_sees || m->ObstacleHeight(se + o[0], sn + o[1]) > 3.0;
        if (ring_sees) continue;  // the ring catches this one (or another nearby): not the case shown
        ++tried;
        SC_EXPECT(ground.ObstacleHeightWithin(se, sn, 20.0) > 3.0);
        if (tried >= 50) break;
    }
    SC_EXPECT(tried > 10);
}
