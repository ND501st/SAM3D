// simcore/nav/ground.hpp — the ground off the road, cell by cell (roadmap G1.3; docs/campaign/11_BATTERY_LIFE_PROPOSAL.md
// §5.1, owner's answer 3: the obstacle grid's spacing in districts, networks only in the country).
//
// A district's map data (its woods, marshes, lakes, rivers, settlements and roads) laid on a grid of cells, each with the
// ground that covers its centre: water first, then a road's corridor (open: the way through the woods is the track), then
// marsh, built-up ground and woods. A mobility class crosses a cell at its speed on that ground (routes.hpp's
// `Mobility::GroundMps`: its cross-country speed times its factor there, water only swimming, 0 not at all). The quickest
// way off the road between two points of the district is an A* search over the cells (eight neighbours, integer tenths of
// a second, ties by the cell's index), so a column goes round the woods and a runner round the pond; derived from the
// map's data at load, never authored. A river or a road is laid on every cell its line passes through, so a river is
// never crossed between two cells and a diagonal step is taken only where both cells beside it are passable. Outside the
// grid the way off the road stays a straight line (the country's).
#pragma once

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

#include "simcore/terrain_map.hpp"

namespace simcore {
namespace nav {

enum class Ground : std::uint8_t { Open, Woods, Marsh, Water, BuiltUp, Count };

inline const char* GroundName(Ground g) {
    switch (g) {
        case Ground::Open: return "open";
        case Ground::Woods: return "woods";
        case Ground::Marsh: return "marsh";
        case Ground::Water: return "water";
        case Ground::BuiltUp: return "built_up";
        case Ground::Count: break;
    }
    return "?";
}

// A class's speed on each ground in m/s (0: it does not cross that ground).
using GroundSpeeds = std::array<double, static_cast<std::size_t>(Ground::Count)>;

class GroundGrid {
public:
    // The map's own ground (its core) at `step_m`. Each shape is painted over its own cells only, lowest first, so a
    // 40 km district at 100 m takes a moment.
    static GroundGrid FromMap(const MapDef& map, double step_m) {
        return FromBox(map, map.west_m, map.south_m, map.east_m, map.north_m, step_m);
    }
    // A box of a map's ground (a battery area in the country: G4), with the country's towns as discs of built-up ground.
    struct Disc {
        MapPoint centre;
        double radius_m = 0.0;
    };
    static GroundGrid FromBox(const MapDef& map, double west_m, double south_m, double east_m, double north_m, double step_m,
                              const std::vector<Disc>& towns = {}) {
        GroundGrid g;
        g.west_ = west_m;
        g.south_ = south_m;
        g.step_ = std::max(10.0, step_m);
        g.cols_ = std::max(1, static_cast<int>(std::ceil((east_m - west_m) / g.step_)));
        g.rows_ = std::max(1, static_cast<int>(std::ceil((north_m - south_m) / g.step_)));
        g.cells_.assign(static_cast<std::size_t>(g.cols_) * static_cast<std::size_t>(g.rows_), static_cast<std::uint8_t>(Ground::Open));
        const double line_band = g.step_ * std::sqrt(0.5);  // every cell a line passes through
        for (const MapPolygonDef& p : map.forests) g.PaintPolygon(p.points, Ground::Woods);
        for (const MapSettlementDef& s : map.settlements) g.PaintDisc(s.centre, s.radius_m, Ground::BuiltUp);
        for (const Disc& d : towns) g.PaintDisc(d.centre, d.radius_m, Ground::BuiltUp);
        for (const MapPolygonDef& p : map.marshes) g.PaintPolygon(p.points, Ground::Marsh);
        for (const MapLineDef& l : map.roads) g.PaintLine(l.points, line_band, Ground::Open);
        for (const MapLineDef& r : map.rivers) g.PaintLine(r.points, std::max(0.5 * r.width_m, line_band), Ground::Water);
        for (const MapLakeDef& l : map.lakes) g.PaintPolygon(l.points, Ground::Water);
        return g;
    }

    bool Covers(const MapPoint& p) const {
        return p.east_m >= west_ && p.north_m >= south_ && p.east_m < west_ + step_ * cols_ && p.north_m < south_ + step_ * rows_;
    }
    Ground At(const MapPoint& p) const {
        if (!Covers(p)) return Ground::Open;
        return static_cast<Ground>(cells_[Index(Col(p.east_m), Row(p.north_m))]);
    }
    double Step() const { return step_; }
    int Cols() const { return cols_; }
    int Rows() const { return rows_; }
    // How many cells of each ground (a check of the layout).
    std::array<int, static_cast<std::size_t>(Ground::Count)> Census() const {
        std::array<int, static_cast<std::size_t>(Ground::Count)> n{};
        for (std::uint8_t c : cells_) ++n[c];
        return n;
    }

    // The quickest way across the ground from a to b at `mps` (each ground's speed, `f` its night factor or 1): the time
    // in tenths of a second, the corners of the way and each corner's time from a. The cells a and b stand in are
    // crossed at the open ground's speed whatever they are (one is where it is). Not found: no way (water all round,
    // for one that does not swim).
    struct Way {
        bool found = false;
        std::int64_t ds = 0;
        double length_m = 0.0;
        std::vector<MapPoint> points;     // the corners after a, the last b
        std::vector<std::int64_t> at_ds;  // each corner's time from a
    };
    Way Across(const MapPoint& a, const MapPoint& b, const GroundSpeeds& mps, double f = 1.0) const {
        Way w;
        if (!Covers(a) || !Covers(b)) return w;
        const int ca = Col(a.east_m), ra = Row(a.north_m), cb = Col(b.east_m), rb = Row(b.north_m);
        const std::size_t start = Index(ca, ra), goal = Index(cb, rb);
        std::array<std::int64_t, static_cast<std::size_t>(Ground::Count)> v{};  // cm/s on each ground (0: cannot)
        for (std::size_t k = 0; k < v.size(); ++k) v[k] = mps[k] > 0.0 ? std::max<std::int64_t>(1, std::llround(mps[k] * f * 100.0)) : 0;
        const std::int64_t v_open = std::max<std::int64_t>(1, v[static_cast<std::size_t>(Ground::Open)]);
        std::int64_t v_max = v_open;
        for (std::int64_t s : v) v_max = std::max(v_max, s);
        auto ds = [](std::int64_t cm, std::int64_t speed) { return (cm * 10 + speed / 2) / speed; };
        auto speed_of = [&](std::size_t cell) {
            const std::int64_t s = v[cells_[cell]];
            return s == 0 && (cell == start || cell == goal) ? v_open : s;
        };
        const std::int64_t straight_cm = std::llround(step_ * 100.0), diagonal_cm = std::llround(step_ * std::sqrt(2.0) * 100.0);
        const std::size_t n = cells_.size();
        std::vector<std::int64_t> g(n, LLONG_MAX);
        std::vector<std::int64_t> from(n, -1);
        // Never more than the true time: the straight line at the best speed, less the rounding of its steps.
        auto h = [&](std::size_t cell) {
            const int c = static_cast<int>(cell % static_cast<std::size_t>(cols_)), r = static_cast<int>(cell / static_cast<std::size_t>(cols_));
            const std::int64_t steps = std::max(std::abs(c - cb), std::abs(r - rb));
            const std::int64_t d_cm = std::llround(std::hypot(static_cast<double>(c - cb), static_cast<double>(r - rb)) * step_ * 100.0);
            return std::max<std::int64_t>(0, ds(d_cm, v_max) - 2 * steps);
        };
        using Item = std::pair<std::int64_t, std::size_t>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
        g[start] = 0;
        open.push({h(start), start});
        while (!open.empty()) {
            const Item top = open.top();
            open.pop();
            const std::size_t u = top.second;
            if (u == goal) break;
            if (top.first - h(u) > g[u]) continue;
            const int c = static_cast<int>(u % static_cast<std::size_t>(cols_)), r = static_cast<int>(u / static_cast<std::size_t>(cols_));
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr == 0 && dc == 0) continue;
                    const int nc = c + dc, nr = r + dr;
                    if (nc < 0 || nr < 0 || nc >= cols_ || nr >= rows_) continue;
                    const std::size_t nb = Index(nc, nr);
                    // Half a step on each cell's ground; a diagonal only where both cells beside it are passable too.
                    const std::int64_t s_from = speed_of(u), s_to = speed_of(nb);
                    if (s_to == 0) continue;
                    if (dr != 0 && dc != 0 && (speed_of(Index(c + dc, r)) == 0 || speed_of(Index(c, r + dr)) == 0)) continue;
                    const std::int64_t len = dr != 0 && dc != 0 ? diagonal_cm : straight_cm;
                    const std::int64_t cost = ds(len / 2, s_from) + ds(len - len / 2, s_to);
                    if (g[u] + cost < g[nb]) {
                        g[nb] = g[u] + cost;
                        from[nb] = static_cast<std::int64_t>(u);
                        open.push({g[nb] + h(nb), nb});
                    }
                }
            }
        }
        if (g[goal] == LLONG_MAX) return w;
        // The cells back from the goal; the way keeps only the cells where it turns (its corners), the ends being a and b.
        std::vector<std::size_t> cells;
        for (std::int64_t k = static_cast<std::int64_t>(goal); k >= 0; k = from[static_cast<std::size_t>(k)]) cells.push_back(static_cast<std::size_t>(k));
        std::reverse(cells.begin(), cells.end());
        auto part = [&](const MapPoint& p, const MapPoint& q, std::size_t cell) {
            return ds(std::llround(std::hypot(q.east_m - p.east_m, q.north_m - p.north_m) * 100.0), speed_of(cell));
        };
        MapPoint last = a;
        auto corner = [&](const MapPoint& p, std::int64_t at) {
            w.length_m += std::hypot(p.east_m - last.east_m, p.north_m - last.north_m);
            w.points.push_back(p);
            w.at_ds.push_back(at);
            last = p;
        };
        // a to its cell's centre, cell to cell, and the goal's centre to b, each at its cells' speeds.
        std::int64_t t = part(a, Centre(ca, ra), start);
        if (cells.size() > 1) corner(Centre(ca, ra), t);
        for (std::size_t i = 1; i < cells.size(); ++i) {
            t += g[cells[i]] - g[cells[i - 1]];
            if (i + 1 == cells.size()) break;
            const int c0 = CellCol(cells[i - 1]), r0 = CellRow(cells[i - 1]), c1 = CellCol(cells[i]), r1 = CellRow(cells[i]);
            const int c2 = CellCol(cells[i + 1]), r2 = CellRow(cells[i + 1]);
            if (c1 - c0 != c2 - c1 || r1 - r0 != r2 - r1) corner(Centre(c1, r1), t);
        }
        if (cells.size() > 1) {
            corner(Centre(cb, rb), t);
            t += part(Centre(cb, rb), b, goal);
        } else {
            t = part(a, b, start);  // within one cell: straight across it
        }
        corner(b, t);
        w.found = true;
        w.ds = t;
        return w;
    }

private:
    int Col(double e) const { return std::min(cols_ - 1, std::max(0, static_cast<int>(std::floor((e - west_) / step_)))); }
    int Row(double n) const { return std::min(rows_ - 1, std::max(0, static_cast<int>(std::floor((n - south_) / step_)))); }
    int CellCol(std::size_t cell) const { return static_cast<int>(cell % static_cast<std::size_t>(cols_)); }
    int CellRow(std::size_t cell) const { return static_cast<int>(cell / static_cast<std::size_t>(cols_)); }
    std::size_t Index(int c, int r) const { return static_cast<std::size_t>(r) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(c); }
    MapPoint Centre(int c, int r) const { return {west_ + (c + 0.5) * step_, south_ + (r + 0.5) * step_}; }

    // Every cell whose centre is in the box (west, south)-(east, north), for `fn(c, r, centre)`.
    template <typename Fn>
    void InBox(double w, double s, double e, double n, Fn fn) const {
        const int c0 = Col(w), c1 = Col(e), r0 = Row(s), r1 = Row(n);
        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) fn(c, r, Centre(c, r));
        }
    }
    bool Outside(double w, double s, double e, double n) const {
        return e < west_ || n < south_ || w > west_ + step_ * cols_ || s > south_ + step_ * rows_;
    }
    void PaintPolygon(const std::vector<MapPoint>& pts, Ground ground) {
        if (pts.size() < 3) return;
        double w = 1e300, s = 1e300, e = -1e300, n = -1e300;
        for (const MapPoint& p : pts) {
            w = std::min(w, p.east_m), e = std::max(e, p.east_m), s = std::min(s, p.north_m), n = std::max(n, p.north_m);
        }
        if (Outside(w, s, e, n)) return;
        InBox(w, s, e, n, [&](int c, int r, const MapPoint& m) {
            if (mapgeo::InPolygon(pts, m.east_m, m.north_m)) cells_[Index(c, r)] = static_cast<std::uint8_t>(ground);
        });
    }
    void PaintDisc(const MapPoint& centre, double radius_m, Ground ground) {
        if (Outside(centre.east_m - radius_m, centre.north_m - radius_m, centre.east_m + radius_m, centre.north_m + radius_m)) return;
        InBox(centre.east_m - radius_m, centre.north_m - radius_m, centre.east_m + radius_m, centre.north_m + radius_m, [&](int c, int r, const MapPoint& m) {
            if (std::hypot(m.east_m - centre.east_m, m.north_m - centre.north_m) < radius_m) cells_[Index(c, r)] = static_cast<std::uint8_t>(ground);
        });
    }
    void PaintLine(const std::vector<MapPoint>& pts, double band_m, Ground ground) {
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            const MapPoint& a = pts[i];
            const MapPoint& b = pts[i + 1];
            if (Outside(std::min(a.east_m, b.east_m) - band_m, std::min(a.north_m, b.north_m) - band_m, std::max(a.east_m, b.east_m) + band_m,
                        std::max(a.north_m, b.north_m) + band_m)) {
                continue;
            }
            InBox(std::min(a.east_m, b.east_m) - band_m, std::min(a.north_m, b.north_m) - band_m, std::max(a.east_m, b.east_m) + band_m,
                  std::max(a.north_m, b.north_m) + band_m, [&](int c, int r, const MapPoint& m) {
                      if (mapgeo::DistToSegment(m.east_m, m.north_m, a.east_m, a.north_m, b.east_m, b.north_m) < band_m) {
                          cells_[Index(c, r)] = static_cast<std::uint8_t>(ground);
                      }
                  });
        }
    }

    double west_ = 0.0, south_ = 0.0, step_ = 100.0;
    int cols_ = 0, rows_ = 0;
    std::vector<std::uint8_t> cells_;
};

}  // namespace nav
}  // namespace simcore
