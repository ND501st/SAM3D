// simcore/terrain.hpp — terrain height interface.
//
// The sim core never owns terrain data. The game (Unreal/Cesium) or a test
// supplies an implementation of `Terrain`. Heights are metres above the
// WGS-84 ellipsoid, looked up by local ENU east/north coordinates.
#pragma once

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace simcore {

// What a terrain tells of each footprint a ground segment crosses (Terrain::ObstacleChords): the fractions of the
// segment where it goes in and comes out (0..1, in <= out) and its top above sea level. Return false to stop.
class ObstacleChordSink {
public:
    virtual bool Chord(double t_in, double t_out, double top_m) = 0;

protected:
    ObstacleChordSink() = default;
    ObstacleChordSink(const ObstacleChordSink&) = default;
    ObstacleChordSink& operator=(const ObstacleChordSink&) = default;
    ~ObstacleChordSink() = default;
};

class Terrain {
public:
    Terrain() = default;
    Terrain(const Terrain&) = default;
    Terrain& operator=(const Terrain&) = default;
    virtual ~Terrain() = default;
    virtual double HeightAt(double east_m, double north_m) const = 0;
    // How high woods and buildings stand above the ground there (roadmap E9): what a line of sight and a radio path
    // must clear besides the ground, and what a low pilot climbs over. None on a bare terrain.
    virtual double ObstacleHeightAt(double /*east_m*/, double /*north_m*/) const { return 0.0; }
    // Where woods and buildings stand as footprints of their own (a map's boxes, roadmap R6), the narrowest of them: a
    // path looks at them no coarser (ObstaclesAlong) so it steps over none. 0: none (a bare terrain, or obstacles as a
    // representative clutter height): the path's own samples alone.
    virtual double ObstacleSpacingM() const { return 0.0; }
    // The top of what stands at (east, north), above sea level (the ground plus ObstacleHeightAt); kNoObstacleTop where
    // nothing does. A terrain that has its tops at hand saves the ground's lookup.
    virtual double ObstacleTopAt(double east_m, double north_m) const {
        const double o = ObstacleHeightAt(east_m, north_m);
        return o > 0.0 ? HeightAt(east_m, north_m) + o : kNoObstacleTop;
    }
    // No footprint over the ground segment from (e0, n0) to (e1, n1) stands higher than this (above sea level; a bound,
    // not the top itself; kNoObstacleTop: none there), and `spacing_m` is the narrowest of those footprints: the spacing
    // the segment is looked at at. A terrain may bound more loosely when the bound is already under `below_m` (nothing
    // there reaches the ray). Only asked where ObstacleSpacingM() > 0.
    virtual double ObstacleTopBoundM(double /*e0*/, double /*n0*/, double /*e1*/, double /*n1*/, double /*below_m*/,
                                     double& spacing_m) const {
        spacing_m = ObstacleSpacingM();
        return spacing_m > 0.0 ? 1e300 : kNoObstacleTop;
    }

    // Every footprint the ground segment from (e0, n0) to (e1, n1) crosses whose top is no lower than `below_m`, told to
    // `sink` with where the segment goes in and comes out of it: exactly, so a ray cutting a building's corner is not
    // stepped over however short its cut (roadmap R6's review). A footprint may be told more than once. False: this
    // terrain does not know them so; a path looks at ObstacleTopAt at the spacing instead (ObstaclesAlong).
    virtual bool ObstacleChords(double /*e0*/, double /*n0*/, double /*e1*/, double /*n1*/, double /*below_m*/,
                                ObstacleChordSink& /*sink*/) const {
        return false;
    }

    // No footprint anywhere stands higher than this (above sea level): a path whose ray runs higher asks nothing more.
    virtual double ObstacleTopCeilingM() const { return 1e300; }
    // No woods or building anywhere stands higher than this over the ground under it (ObstacleHeightAt never more): a
    // path's sample whose ray runs higher over the ground there does not look the obstacle up.
    virtual double ObstacleTallestM() const { return 1e300; }

    static constexpr double kNoObstacleTop = -1e300;
};

// A path's profile (radar.hpp HasLineOfSight, em/propagation.hpp AnalysePath) carries the woods and buildings as
// ITU-R P.1812 carries its representative clutter heights, except within this distance of either end: a radar or a
// vehicle at a wood's edge, in a clearing or in a street is not hidden by the cell it stands in (a game value,
// docs/EM_ARCHITECTURE.md section 27).
constexpr double kObstacleSkipM = 60.0;

// The woods and buildings over one stretch of a straight path, between two of its samples: each footprint the stretch
// crosses, at the ends and the middle of its cut (Terrain::ObstacleChords), or, on a terrain that does not know its
// footprints so, at the terrain's own spacing (Terrain::ObstacleSpacingM, roadmap R6): a narrow building between two
// samples, or a corner cut short, is not stepped over. The path starts at
// (ax, ay) and runs along the unit direction (ux, uy) for total_m; the stretch is [s0, s1] of it, less skip_m at either
// end of the path (kObstacleSkipM). Looked at only where something there may stand higher than `below_m` (above sea
// level: Terrain::ObstacleTopBoundM, which gives the spacing too), so open ground, and a ray high over the roofs, cost
// one bound. `visit(s, top_m)` is called for each point where something stands (s along the path, its top above sea
// level, without the Earth's bulge) and returns false to stop. Nothing is done on a terrain with no footprints (spacing 0).
template <class Visit>
inline void ObstaclesAlong(const Terrain& t, double ax, double ay, double ux, double uy, double total_m, double s0, double s1,
                           double below_m, double skip_m, Visit visit) {
    if (t.ObstacleSpacingM() <= 0.0) return;
    const double lo = s0 > skip_m ? s0 : skip_m;
    const double hi = s1 < total_m - skip_m ? s1 : total_m - skip_m;
    if (hi < lo || below_m > t.ObstacleTopCeilingM()) return;
    double spacing = 0.0;
    if (t.ObstacleTopBoundM(ax + ux * lo, ay + uy * lo, ax + ux * hi, ay + uy * hi, below_m, spacing) < below_m || spacing <= 0.0) return;
    // The cuts: the ray over a flat roof is lowest at one end of its cut (the Earth's bulge across a cut is under a
    // millimetre), so its ends and its middle are what the visitor needs.
    struct Cuts final : ObstacleChordSink {
        Visit& v;
        double lo_m, hi_m;
        Cuts(Visit& visit_fn, double l, double h) : v(visit_fn), lo_m(l), hi_m(h) {}
        bool Chord(double t_in, double t_out, double top_m) override {
            const double a = lo_m + (hi_m - lo_m) * (t_in < 0.0 ? 0.0 : t_in > 1.0 ? 1.0 : t_in);
            const double b = lo_m + (hi_m - lo_m) * (t_out < 0.0 ? 0.0 : t_out > 1.0 ? 1.0 : t_out);
            return v(a, top_m) && v(0.5 * (a + b), top_m) && v(b, top_m);
        }
    } cuts(visit, lo, hi);
    if (t.ObstacleChords(ax + ux * lo, ay + uy * lo, ax + ux * hi, ay + uy * hi, below_m, cuts)) return;
    const int steps = static_cast<int>(std::ceil((hi - lo) / spacing));
    for (int k = 0; k <= steps; ++k) {
        const double s = steps > 0 ? lo + (hi - lo) * static_cast<double>(k) / steps : lo;
        const double top = t.ObstacleTopAt(ax + ux * s, ay + uy * s);
        if (top > Terrain::kNoObstacleTop && !visit(s, top)) return;
    }
}

// Flat terrain at a constant height.
class FlatTerrain final : public Terrain {
public:
    explicit FlatTerrain(double height_m = 0.0) : height_m_(height_m) {}
    double HeightAt(double, double) const override { return height_m_; }

private:
    double height_m_;
};

// Regular grid heightmap with bilinear interpolation. Outside the grid the edge value is used.
class GridTerrain final : public Terrain {
public:
    // `heights` is row-major: heights[row * cols + col]; row 0 = southmost, col 0 = westmost.
    GridTerrain(double origin_east_m, double origin_north_m, double spacing_m, int cols, int rows,
                std::vector<double> heights)
        : origin_east_(origin_east_m), origin_north_(origin_north_m), spacing_(spacing_m),
          cols_(cols), rows_(rows), heights_(std::move(heights)) {}

    double HeightAt(double east_m, double north_m) const override {
        if (cols_ < 1 || rows_ < 1 || heights_.empty()) return 0.0;
        const double fx = ClampIndex((east_m - origin_east_) / spacing_, cols_);
        const double fy = ClampIndex((north_m - origin_north_) / spacing_, rows_);
        const int x0 = static_cast<int>(std::floor(fx));
        const int y0 = static_cast<int>(std::floor(fy));
        const int x1 = x0 + 1 < cols_ ? x0 + 1 : x0;
        const int y1 = y0 + 1 < rows_ ? y0 + 1 : y0;
        const double tx = fx - x0;
        const double ty = fy - y0;
        const double h00 = At(x0, y0), h10 = At(x1, y0), h01 = At(x0, y1), h11 = At(x1, y1);
        return (h00 * (1 - tx) + h10 * tx) * (1 - ty) + (h01 * (1 - tx) + h11 * tx) * ty;
    }

private:
    static double ClampIndex(double f, int count) {
        if (f < 0.0) return 0.0;
        const double hi = static_cast<double>(count - 1);
        return f > hi ? hi : f;
    }
    double At(int x, int y) const { return heights_[static_cast<std::size_t>(y) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(x)]; }

    double origin_east_, origin_north_, spacing_;
    int cols_, rows_;
    std::vector<double> heights_;
};

// A straight ridge on flat ground: a bank `height_m` high along the segment from (a_east, a_north) to (b_east, b_north),
// `width_m` wide at its foot, with cosine shoulders. Cover for scenarios played without a terrain grid (in the game,
// Cesium supplies the real ground instead).
struct TerrainRidge {
    double a_east_m = 0.0;
    double a_north_m = 0.0;
    double b_east_m = 0.0;
    double b_north_m = 0.0;
    double height_m = 0.0;
    double width_m = 500.0;
};

class RidgeTerrain final : public Terrain {
public:
    explicit RidgeTerrain(double base_m = 0.0, std::vector<TerrainRidge> ridges = {}) : base_m_(base_m), ridges_(std::move(ridges)) {}

    double HeightAt(double east_m, double north_m) const override {
        double top = 0.0;
        for (const TerrainRidge& r : ridges_) {
            const double dx = r.b_east_m - r.a_east_m, dy = r.b_north_m - r.a_north_m;
            const double len2 = dx * dx + dy * dy;
            double f = len2 > 0.0 ? ((east_m - r.a_east_m) * dx + (north_m - r.a_north_m) * dy) / len2 : 0.0;
            f = f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
            const double d = std::hypot(east_m - (r.a_east_m + f * dx), north_m - (r.a_north_m + f * dy));
            const double half = 0.5 * r.width_m;
            if (d >= half || half <= 0.0) continue;
            const double h = r.height_m * 0.5 * (1.0 + std::cos(3.14159265358979323846 * d / half));
            if (h > top) top = h;
        }
        return base_m_ + top;
    }
    const std::vector<TerrainRidge>& Ridges() const { return ridges_; }

private:
    double base_m_;
    std::vector<TerrainRidge> ridges_;
};

}  // namespace simcore
