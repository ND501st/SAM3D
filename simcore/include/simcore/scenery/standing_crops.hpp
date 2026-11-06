// simcore/scenery/standing_crops.hpp — the crops that stand on the session's day and block sight (roadmap L5, D4).
//
// A field's crop and its state on a day come from the map's dressing and the crop calendar (map_dressing.hpp CropOf,
// ground_layers.hpp StandOnDay): standing maize 2.4 m, sunflower 1.8 m, wheat 0.9 m in their seasons, nothing on stubble or
// ploughland; each field a share taller or shorter, and patches over it taller or shorter again (GroundLayerRules
// StandAtPoint, roadmap A.3.10). Where a field stands is where the land cover is field (the 25 m cover cells: woods, towns,
// roads and water cut it) inside the field's own rectangle (farm_fields.hpp FieldAt, straight at every distance). This is
// what a sight line meets (MapTerrain merges it with the map's woods and buildings) and what Unreal draws, from the same
// cells.
//
// Baked once for a day: one byte a cover cell (the most the stand in it can reach in decimetres, and whether a field's
// edge crosses it) and the tallest top in every 500 m block. A path's chords are walked cell by cell and, in a cell a field's edge
// crosses, field by field, so a field's edge is where FieldAt puts it, not at the cell's. The day is the session's start:
// a field harvested during a long session stands until it ends.
//
// Standard library only.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "simcore/farm_fields.hpp"
#include "simcore/scenery/ground_layers.hpp"
#include "simcore/scenery/map_dressing.hpp"
#include "simcore/terrain.hpp"
#include "simcore/terrain_map.hpp"

namespace simcore {
namespace scenery {

class StandingCrops final : public MapStandingObstacles {
public:
    static constexpr double kBlockM = 500.0;
    static constexpr std::uint8_t kMixed = 0x80;  // a field's edge crosses the cell

    // The crops of `map` standing on day `day` (1..366), from its cover (BakeCover), the dressing's crops and the calendar.
    static std::shared_ptr<const StandingCrops> Bake(std::shared_ptr<const MapData> map, std::shared_ptr<const CoverRaster> cover,
                                                     const DressingRules& dressing, const GroundLayerRules& layers, int day) {
        std::shared_ptr<StandingCrops> s(new StandingCrops());
        s->map_ = std::move(map);
        s->cover_ = std::move(cover);
        s->dressing_ = dressing;
        s->layers_ = layers;
        s->day_ = day;
        s->BakeCells();
        return s;
    }

    int Day() const { return day_; }
    const CoverRaster& CoverCells() const { return *cover_; }
    bool Any() const { return tallest_stand_m_ > 0.0; }
    // The tallest any crop stands today, and the highest top of any (above sea level; kNoObstacleTop: none).
    double TallestStand() const override { return tallest_stand_m_; }
    double TallestTop() const override { return tallest_top_m_; }
    // The cover cells' size: what a path is looked at at where no chord walk is asked for.
    double CellM() const override { return cover_->grids.empty() ? 25.0 : cover_->grids.front().cell_m; }

    // How high the crop stands at (east, north) of the map frame: 0 where nothing does.
    double StandAt(double e, double n) const override {
        const int gi = Any() ? GridIndex(e, n) : -1;
        if (gi < 0) return 0.0;
        const CoverGrid& g = cover_->grids[static_cast<std::size_t>(gi)];
        if (cells_[static_cast<std::size_t>(gi)][g.IndexAt(e, n)] == 0) return 0.0;
        return StandOf(FieldAt(g.fields, e, n), e, n);
    }
    // The top of the crop at (east, north), above sea level (kNoObstacleTop: nothing stands).
    double TopAt(double e, double n) const override {
        const double h = StandAt(e, n);
        return h > 0.0 ? map_->GroundHeight(e, n) + h : Terrain::kNoObstacleTop;
    }
    // The tallest stand within `radius_m` of (east, north), by the cells (a cell a field's edge crosses counts whole).
    double StandWithin(double e, double n, double radius_m) const override {
        double most = 0.0;
        if (!Any()) return most;
        for (std::size_t gi = 0; gi < cover_->grids.size(); ++gi) {
            const CoverGrid& g = cover_->grids[gi];
            const int c0 = std::max(0, static_cast<int>(std::floor((e - radius_m - g.west_m) / g.cell_m)));
            const int c1 = std::min(g.cols - 1, static_cast<int>(std::floor((e + radius_m - g.west_m) / g.cell_m)));
            const int r0 = std::max(0, static_cast<int>(std::floor((n - radius_m - g.south_m) / g.cell_m)));
            const int r1 = std::min(g.rows - 1, static_cast<int>(std::floor((n + radius_m - g.south_m) / g.cell_m)));
            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    const std::uint8_t b = cells_[gi][g.Index(c, r)];
                    if (b == 0) continue;
                    const double de = std::max(0.0, std::fabs(g.CentreEast(c) - e) - 0.5 * g.cell_m);
                    const double dn = std::max(0.0, std::fabs(g.CentreNorth(r) - n) - 0.5 * g.cell_m);
                    if (std::hypot(de, dn) <= radius_m) most = std::max(most, static_cast<double>(b & 0x7F) / 10.0);
                }
            }
        }
        return most;
    }

    // No crop over the ground segment from (e0, n0) to (e1, n1) stands higher than this (above sea level; a bound by the
    // 500 m blocks its bounds reach; kNoObstacleTop: none).
    double TopBound(double e0, double n0, double e1, double n1) const override {
        if (blocks_.empty()) return Terrain::kNoObstacleTop;
        const int c0 = BlockCol(std::min(e0, e1)), c1 = BlockCol(std::max(e0, e1));
        const int r0 = BlockRow(std::min(n0, n1)), r1 = BlockRow(std::max(n0, n1));
        double top = Terrain::kNoObstacleTop;
        for (int r = std::max(0, r0); r <= std::min(block_rows_ - 1, r1); ++r) {
            for (int c = std::max(0, c0); c <= std::min(block_cols_ - 1, c1); ++c) {
                top = std::max(top, blocks_[static_cast<std::size_t>(r) * static_cast<std::size_t>(block_cols_) + static_cast<std::size_t>(c)]);
            }
        }
        return top;
    }

    // Every stretch of the ground segment from (e0, n0) to (e1, n1) where a crop stands, at most a cover cell long, whose
    // top is no lower than `below_m`, told to `sink` with where the segment goes in and comes out of it and its top (the
    // ground's highest at its ends and middle, plus the field's highest stand there). Exact at a field's edge.
    void Chords(double e0, double n0, double e1, double n1, double below_m, ObstacleChordSink& sink) const override {
        if (!Any() || below_m > tallest_top_m_) return;
        const double de = e1 - e0, dn = n1 - n0;
        bool going = true;
        const auto emit = [&](double ta, double tb, const FieldCell& f) {
            if (tb <= ta || FieldStand(f) <= 0.0) return;
            const double tm = 0.5 * (ta + tb);
            double ground = -1e300, stand = 0.0;
            for (const double t : {ta, tm, tb}) {
                ground = std::max(ground, map_->GroundHeight(e0 + de * t, n0 + dn * t));
                stand = std::max(stand, StandOf(f, e0 + de * t, n0 + dn * t));
            }
            if (ground + stand >= below_m) going = sink.Chord(ta, tb, ground + stand);
        };
        // The segment grid by grid: a detail area's grid where it covers, else the map's (CoverRaster::GridAt's order).
        double t = 0.0;
        while (t < 1.0 && going) {
            const double probe = std::min(1.0, t + 1e-9);
            const int gi = GridIndex(e0 + de * probe, n0 + dn * probe);
            double t_end = 1.0;
            if (gi < 0) {
                // Beyond every grid: on to where the segment first comes into one.
                for (const CoverGrid& g : cover_->grids) {
                    double a = 0.0, b = 0.0;
                    if (ClipBox(e0, n0, de, dn, g, a, b) && a > t) t_end = std::min(t_end, a);
                }
                if (t_end <= t) break;
                t = t_end;
                continue;
            }
            const CoverGrid& g = cover_->grids[static_cast<std::size_t>(gi)];
            double a = 0.0, b = 1.0;
            ClipBox(e0, n0, de, dn, g, a, b);
            t_end = std::min(1.0, b);
            for (int k = 0; k < gi; ++k) {  // a grid before it in the order takes over where the segment comes into it
                double ka = 0.0, kb = 0.0;
                if (ClipBox(e0, n0, de, dn, cover_->grids[static_cast<std::size_t>(k)], ka, kb) && ka > t) t_end = std::min(t_end, ka);
            }
            if (t_end <= t) t_end = std::min(1.0, t + 1e-9);
            WalkGrid(static_cast<std::size_t>(gi), e0, n0, de, dn, t, t_end, [&](std::size_t i, double ta, double tb) {
                const std::uint8_t cell = cells_[static_cast<std::size_t>(gi)][i];
                if (cell == 0) return going;
                if ((cell & kMixed) == 0) {
                    const double tm = 0.5 * (ta + tb);
                    emit(ta, tb, FieldAt(g.fields, e0 + de * tm, n0 + dn * tm));
                    return going;
                }
                // A field's edge crosses the cell: field by field, each to where the segment leaves its rectangle or estate.
                double s = ta;
                while (s < tb && going) {
                    const double sp = std::min(tb, s + 1e-9);
                    const double pe = e0 + de * sp, pn = n0 + dn * sp;
                    const FieldCell f = FieldAt(g.fields, pe, pn);
                    double out = std::min(tb, FieldExit(g.fields, f, pe, pn, e0, n0, de, dn));
                    if (out <= s) out = std::min(tb, s + 1e-9);
                    emit(s, out, f);
                    s = out;
                }
                return going;
            });
            t = t_end;
        }
    }

    // The standing crops over the map's square [w, e) x [s, n) as a block for Unreal (roadmap L5's far form): each standing
    // cell whose middle lies in the square, a polygon (the cell, or where a field's edge crosses it, each standing field's
    // part of it, its edges FieldAt's), a top at the crop's height over the ground at its corners, and a wall down to
    // 0.3 m under the ground on every edge beyond which the crop stands lower. Map metres, heights above sea level; the
    // winding faces out by (b - a) x (c - a) in east, north, up. The colours: the crops' tops and their sides, a field's
    // shade varied by its number.
    //
    // With `uvs`, one pair per vertex for a material to draw the plants by (Unreal's M_SimMapCrop): on a wall, metres along
    // its line and the height over the ground as a share of the stand; on a top, metres along the field's rows and across
    // them. The walls rise to kFringe of the stand and the top lies at kTopShare of it, so a material can cut the walls' top
    // ragged (the plants' tops) over the top. The sight lines' obstacle stays the stand.
    static constexpr double kFringe = 1.06, kTopShare = 0.88;
    TriMesh BlockMesh(double w, double s, double e, double n, const std::array<float, 3>& top_colour, const std::array<float, 3>& side_colour,
                      std::vector<std::array<float, 2>>* uvs = nullptr) const {
        TriMesh m;
        if (!Any()) return m;
        for (std::size_t gi = 0; gi < cover_->grids.size(); ++gi) {
            const CoverGrid& g = cover_->grids[gi];
            const int c0 = std::max(0, static_cast<int>(std::ceil((w - g.west_m) / g.cell_m - 0.5)));
            const int c1 = std::min(g.cols - 1, static_cast<int>(std::ceil((e - g.west_m) / g.cell_m - 0.5)) - 1);
            const int r0 = std::max(0, static_cast<int>(std::ceil((s - g.south_m) / g.cell_m - 0.5)));
            const int r1 = std::min(g.rows - 1, static_cast<int>(std::ceil((n - g.south_m) / g.cell_m - 0.5)) - 1);
            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    const std::uint8_t b = cells_[gi][g.Index(c, r)];
                    if (b == 0) continue;
                    const double cw = g.west_m + g.cell_m * c, cs = g.south_m + g.cell_m * r;
                    // The cell less any grid before it in the order (that grid draws its own cells there).
                    std::vector<std::array<double, 4>> rects{{cw, cs, cw + g.cell_m, cs + g.cell_m}};
                    for (std::size_t k = 0; k < gi; ++k) {
                        const CoverGrid& o = cover_->grids[k];
                        rects = Subtract(rects, {o.west_m, o.south_m, o.west_m + o.cell_m * o.cols, o.south_m + o.cell_m * o.rows});
                    }
                    for (const std::array<double, 4>& rc : rects) AddCellBlock(m, g, rc, (b & kMixed) != 0, top_colour, side_colour, uvs);
                }
            }
        }
        return m;
    }

private:
    StandingCrops() = default;

    // Rectangles {w, s, e, n} less one, as up to four rectangles each.
    static std::vector<std::array<double, 4>> Subtract(const std::vector<std::array<double, 4>>& in, const std::array<double, 4>& hole) {
        std::vector<std::array<double, 4>> out;
        for (const std::array<double, 4>& r : in) {
            if (hole[0] >= r[2] || hole[2] <= r[0] || hole[1] >= r[3] || hole[3] <= r[1]) {
                out.push_back(r);
                continue;
            }
            if (hole[0] > r[0]) out.push_back({r[0], r[1], hole[0], r[3]});
            if (hole[2] < r[2]) out.push_back({hole[2], r[1], r[2], r[3]});
            const double lo = std::max(r[0], hole[0]), hi = std::min(r[2], hole[2]);
            if (hole[1] > r[1]) out.push_back({lo, r[1], hi, hole[1]});
            if (hole[3] < r[3]) out.push_back({lo, hole[3], hi, r[3]});
        }
        return out;
    }

    // A convex polygon (counter-clockwise in east, north) cut to the side of a line where a*e + b*n <= d.
    static std::vector<std::array<double, 2>> Clip(const std::vector<std::array<double, 2>>& poly, double a, double b, double d) {
        std::vector<std::array<double, 2>> out;
        for (std::size_t i = 0; i < poly.size(); ++i) {
            const std::array<double, 2>& p = poly[i];
            const std::array<double, 2>& q = poly[(i + 1) % poly.size()];
            const double fp = a * p[0] + b * p[1] - d, fq = a * q[0] + b * q[1] - d;
            if (fp <= 0.0) out.push_back(p);
            if ((fp < 0.0 && fq > 0.0) || (fp > 0.0 && fq < 0.0)) {
                const double t = fp / (fp - fq);
                out.push_back({p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t});
            }
        }
        return out;
    }

    void AddCellBlock(TriMesh& m, const CoverGrid& g, const std::array<double, 4>& rc, bool mixed, const std::array<float, 3>& top_colour,
                      const std::array<float, 3>& side_colour, std::vector<std::array<float, 2>>* uvs) const {
        const std::vector<std::array<double, 2>> square{{rc[0], rc[1]}, {rc[2], rc[1]}, {rc[2], rc[3]}, {rc[0], rc[3]}};
        if (!mixed) {
            const FieldCell f = FieldAt(g.fields, 0.5 * (rc[0] + rc[2]), 0.5 * (rc[1] + rc[3]));
            AddPrism(m, square, FieldStand(f), f, top_colour, side_colour, uvs);
            return;
        }
        // Each field a sample finds in the cell, cut to its rectangle and estate.
        std::vector<std::uint64_t> seen;
        for (int j = 0; j <= 6; ++j) {
            for (int i = 0; i <= 6; ++i) {
                const FieldCell f = FieldAt(g.fields, rc[0] + (rc[2] - rc[0]) * (0.02 + 0.96 * i / 6.0), rc[1] + (rc[3] - rc[1]) * (0.02 + 0.96 * j / 6.0));
                if (std::find(seen.begin(), seen.end(), f.id) != seen.end()) continue;
                seen.push_back(f.id);
                const double stand = FieldStand(f);
                if (stand <= 0.0) continue;
                const double sh = std::sin(f.heading_rad), ch = std::cos(f.heading_rad);
                std::vector<std::array<double, 2>> poly = square;
                // |u - u_c| <= L/2 and |v - v_c| <= W/2, with u = e sh + n ch, v = e ch - n sh.
                const double uc = f.centre_e * sh + f.centre_n * ch, vc = f.centre_e * ch - f.centre_n * sh;
                poly = Clip(poly, sh, ch, uc + 0.5 * f.length_m);
                poly = Clip(poly, -sh, -ch, -(uc - 0.5 * f.length_m));
                poly = Clip(poly, ch, -sh, vc + 0.5 * f.width_m);
                poly = Clip(poly, -ch, sh, -(vc - 0.5 * f.width_m));
                // The estate: the sample's own square (a field's centre may lie in the next estate).
                const double px = rc[0] + (rc[2] - rc[0]) * (0.02 + 0.96 * i / 6.0), py = rc[1] + (rc[3] - rc[1]) * (0.02 + 0.96 * j / 6.0);
                const double ew = std::floor(px / g.fields.estate_m) * g.fields.estate_m, es = std::floor(py / g.fields.estate_m) * g.fields.estate_m;
                poly = Clip(poly, 1.0, 0.0, ew + g.fields.estate_m);
                poly = Clip(poly, -1.0, 0.0, -ew);
                poly = Clip(poly, 0.0, 1.0, es + g.fields.estate_m);
                poly = Clip(poly, 0.0, -1.0, -es);
                if (poly.size() >= 3) AddPrism(m, poly, stand, f, top_colour, side_colour, uvs);
            }
        }
    }

    void AddPrism(TriMesh& m, const std::vector<std::array<double, 2>>& poly, double stand, const FieldCell& field, const std::array<float, 3>& top_colour,
                  const std::array<float, 3>& side_colour, std::vector<std::array<float, 2>>* uvs) const {
        if (stand <= 0.0 || poly.size() < 3) return;
        // The field's own shade, and its crop's tone (GroundLayerRules::BlockTone: dry sunflower darker than dry maize).
        const float shade = static_cast<float>(0.9 + 0.2 * Unit24(Hash32(static_cast<std::uint32_t>(field.id >> 32) ^ 0x5EEDu)));
        const std::array<float, 3> tone = layers_.BlockTone(CropOf(dressing_, field.id));
        const std::array<float, 3> top{top_colour[0] * shade * tone[0], top_colour[1] * shade * tone[1], top_colour[2] * shade * tone[2]};
        const std::array<float, 3> side{side_colour[0] * shade * tone[0], side_colour[1] * shade * tone[1], side_colour[2] * shade * tone[2]};
        const auto vertex = [&](double e, double n, double z, const Vec3& normal, const std::array<float, 3>& colour, double u, double v) {
            // The crop's tone mottled over tens of metres (thinner, drier, lodged patches), the same at a point for every cell.
            const float mottle = static_cast<float>(0.82 + 0.36 * noise::Fbm(0xC0A2Eull, e, n, 60.0, 3));
            m.positions.push_back(Vec3{e, n, z});
            m.normals.push_back(normal);
            m.colours.push_back({colour[0] * mottle, colour[1] * mottle, colour[2] * mottle});
            if (uvs != nullptr) uvs->push_back({static_cast<float>(u), static_cast<float>(v)});
            return static_cast<std::uint32_t>(m.positions.size() - 1);
        };
        const bool fringe = uvs != nullptr;  // the drawn form for a material: walls to the fringe, the top under it
        // Shares of the stand at each corner (StandOf: the field's height varies over it).
        const double top_share = fringe ? kTopShare : 1.0, wall_share = fringe ? kFringe : 1.0;
        // The top, a fan, at the crop's height over the ground at each corner; its UV along the rows and across them.
        const double sh = std::sin(field.heading_rad), ch = std::cos(field.heading_rad);
        // UVs kept small for a GPU's floats: wrapped by whole periods of the material's patterns (225 m on a top: rows
        // 0.75 m, plants 0.5 m; 230 m along a wall: plants 0.23 m), the same for every vertex of a face.
        const double u0 = 225.0 * std::floor((poly[0][0] * sh + poly[0][1] * ch) / 225.0), v0 = 225.0 * std::floor((poly[0][0] * ch - poly[0][1] * sh) / 225.0);
        const std::uint32_t first = static_cast<std::uint32_t>(m.positions.size());
        for (const std::array<double, 2>& p : poly) {
            vertex(p[0], p[1], map_->GroundHeight(p[0], p[1]) + top_share * StandOf(field, p[0], p[1]), Vec3{0.0, 0.0, 1.0}, top,
                   p[0] * sh + p[1] * ch - u0, p[0] * ch - p[1] * sh - v0);
        }
        for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
            m.indices.push_back(first);
            m.indices.push_back(first + static_cast<std::uint32_t>(i));
            m.indices.push_back(first + static_cast<std::uint32_t>(i + 1));
        }
        // A wall on each edge beyond which the crop stands lower (a neighbour as tall, within 2 cm, covers it: inside a
        // field the height varies smoothly, so only a field's edge or a lower field's has one).
        for (std::size_t i = 0; i < poly.size(); ++i) {
            const std::array<double, 2>& a = poly[i];
            const std::array<double, 2>& b = poly[(i + 1) % poly.size()];
            const double de = b[0] - a[0], dn = b[1] - a[1], len = std::hypot(de, dn);
            if (len < 1e-6) continue;
            const Vec3 out{dn / len, -de / len, 0.0};  // right of a -> b: outward for a counter-clockwise polygon
            const double me = 0.5 * (a[0] + b[0]), mn = 0.5 * (a[1] + b[1]);
            if (StandAt(me + 0.05 * out.x, mn + 0.05 * out.y) >= StandOf(field, me, mn) - 0.02) continue;
            const double ga = map_->GroundHeight(a[0], a[1]), gb = map_->GroundHeight(b[0], b[1]);
            const double sa = StandOf(field, a[0], a[1]), sb = StandOf(field, b[0], b[1]);
            // Along the wall's line (the same along a straight edge cut into cells), the height as a share of the stand.
            const double along = a[0] * de / len + a[1] * dn / len;
            const double ua = along - 230.0 * std::floor(along / 230.0), ub = ua + len;
            const std::uint32_t k = vertex(a[0], a[1], ga - 0.3, out, side, ua, -0.3 / sa);
            vertex(b[0], b[1], gb - 0.3, out, side, ub, -0.3 / sb);
            vertex(b[0], b[1], gb + wall_share * sb, out, side, ub, wall_share);
            vertex(a[0], a[1], ga + wall_share * sa, out, side, ua, wall_share);
            for (const std::uint32_t t : {k, k + 1, k + 2, k, k + 2, k + 3}) m.indices.push_back(t);
        }
    }

    // The field's calendar height on the day, and its height at a point of it (the variation over it added).
    double FieldStand(const FieldCell& f) const { return layers_.StandOnDay(CropOf(dressing_, f.id), f.id, day_); }
    double StandOf(const FieldCell& f, double e, double n) const { return layers_.StandAtPoint(CropOf(dressing_, f.id), f.id, day_, e, n); }

    int GridIndex(double e, double n) const {
        for (std::size_t i = 0; i < cover_->grids.size(); ++i) {
            if (cover_->grids[i].Covers(e, n)) return static_cast<int>(i);
        }
        return -1;
    }
    int BlockCol(double e) const { return static_cast<int>(std::floor((e - block_west_m_) / kBlockM)); }
    int BlockRow(double n) const { return static_cast<int>(std::floor((n - block_south_m_) / kBlockM)); }

    // The segment's fractions inside a grid's square (false: it misses it).
    static bool ClipBox(double e0, double n0, double de, double dn, const CoverGrid& g, double& t_in, double& t_out) {
        return ClipRect(e0, n0, de, dn, g.west_m, g.south_m, g.west_m + g.cell_m * g.cols, g.south_m + g.cell_m * g.rows, t_in, t_out);
    }
    static bool ClipRect(double e0, double n0, double de, double dn, double w, double s, double ea, double nn, double& t_in, double& t_out) {
        t_in = -std::numeric_limits<double>::infinity();
        t_out = std::numeric_limits<double>::infinity();
        const auto slab = [&](double p, double d, double lo, double hi) {
            if (std::fabs(d) < 1e-15) return p >= lo && p <= hi;
            double a = (lo - p) / d, b = (hi - p) / d;
            if (a > b) std::swap(a, b);
            t_in = std::max(t_in, a);
            t_out = std::min(t_out, b);
            return t_in <= t_out;
        };
        return slab(e0, de, w, ea) && slab(n0, dn, s, nn) && t_out >= 0.0 && t_in <= 1.0;
    }
    // Where the segment, at (pe, pn) inside field `f`, leaves it: its rectangle (farm_fields.hpp: u along the furrows, v
    // across) or its estate's square, whichever first.
    static double FieldExit(const FieldGrid& fg, const FieldCell& f, double pe, double pn, double e0, double n0, double de, double dn) {
        const double sh = std::sin(f.heading_rad), ch = std::cos(f.heading_rad);
        // In the field's frame about its centre: u(t) = u0 + du t, v(t) = v0 + dv t.
        const double u0 = (e0 - f.centre_e) * sh + (n0 - f.centre_n) * ch, du = de * sh + dn * ch;
        const double v0 = (e0 - f.centre_e) * ch - (n0 - f.centre_n) * sh, dv = de * ch - dn * sh;
        double t_out = std::numeric_limits<double>::infinity();
        const auto leave = [&](double p, double d, double half) {
            if (std::fabs(d) < 1e-15) return;
            t_out = std::min(t_out, ((d > 0.0 ? half : -half) - p) / d);
        };
        leave(u0, du, 0.5 * f.length_m);
        leave(v0, dv, 0.5 * f.width_m);
        const double ix = std::floor(pe / fg.estate_m), iy = std::floor(pn / fg.estate_m);
        const auto leave_box = [&](double p0, double d, double lo, double hi) {
            if (std::fabs(d) < 1e-15) return;
            t_out = std::min(t_out, ((d > 0.0 ? hi : lo) - p0) / d);
        };
        leave_box(e0, de, ix * fg.estate_m, (ix + 1.0) * fg.estate_m);
        leave_box(n0, dn, iy * fg.estate_m, (iy + 1.0) * fg.estate_m);
        return t_out;
    }

    // The cells of grid `gi` the segment passes through between fractions ta and tb, in order: visit(cell index, t in,
    // t out), false to stop.
    template <class Visit>
    void WalkGrid(std::size_t gi, double e0, double n0, double de, double dn, double ta, double tb, Visit visit) const {
        const CoverGrid& g = cover_->grids[gi];
        const double tm = std::min(tb, ta + 1e-9);
        int c = std::min(g.cols - 1, std::max(0, static_cast<int>(std::floor((e0 + de * tm - g.west_m) / g.cell_m))));
        int r = std::min(g.rows - 1, std::max(0, static_cast<int>(std::floor((n0 + dn * tm - g.south_m) / g.cell_m))));
        const int step_c = de > 0.0 ? 1 : -1, step_r = dn > 0.0 ? 1 : -1;
        const auto next_t = [](double p0, double d, double edge) { return std::fabs(d) < 1e-15 ? std::numeric_limits<double>::infinity() : (edge - p0) / d; };
        double t = ta;
        while (t < tb) {
            const double edge_e = g.west_m + g.cell_m * (c + (step_c > 0 ? 1 : 0));
            const double edge_n = g.south_m + g.cell_m * (r + (step_r > 0 ? 1 : 0));
            const double te = next_t(e0, de, edge_e), tn = next_t(n0, dn, edge_n);
            const double t_next = std::min({tb, te, tn});
            if (t_next > t && !visit(g.Index(c, r), t, t_next)) return;
            if (t_next >= tb) return;
            if (te <= tn) c += step_c;
            if (tn <= te) r += step_r;
            if (c < 0 || r < 0 || c >= g.cols || r >= g.rows) return;
            t = std::max(t, t_next);
        }
    }

    // A cell's byte: the tallest stand in it (decimetres, rounded up) and kMixed where a field's edge crosses it.
    std::uint8_t CellByte(const CoverGrid& g, int c, int r) const {
        if (static_cast<scenery::Cover>(g.cover[g.Index(c, r)]) != scenery::Cover::Field) return 0;
        const double w = g.west_m + g.cell_m * c, s = g.south_m + g.cell_m * r, ea = w + g.cell_m, nn = s + g.cell_m;
        const FieldCell f = FieldAt(g.fields, 0.5 * (w + ea), 0.5 * (s + nn));
        // The fields' lines are straight: the cell is one field's when its four corners are.
        bool mixed = false;
        const double ce[4] = {w, ea, w, ea}, cn[4] = {s, s, nn, nn};
        for (int k = 0; k < 4 && !mixed; ++k) mixed = FieldAt(g.fields, ce[k], cn[k]).id != f.id;
        // A corner on the line belongs to the next field: a cell flush with an edge reads as crossed (a few cells more to walk).
        double stand = FieldStand(f);
        if (mixed) {
            for (int j = 0; j <= 6; ++j) {
                for (int i = 0; i <= 6; ++i) {
                    stand = std::max(stand, FieldStand(FieldAt(g.fields, w + (ea - w) * i / 6.0, s + (nn - s) * j / 6.0)));
                }
            }
        }
        if (stand <= 0.0 && !mixed) return 0;
        // The most it can reach anywhere in the cell (the variation's bound): what the blocks' tops and StandWithin read.
        const int dm = std::min(127, static_cast<int>(std::ceil(stand * layers_.MostVary() * 10.0 - 1e-9)));
        if (dm == 0 && mixed) {
            // Nothing stands at the samples, but a field's edge crosses it: a sliver of a standing field may (walked anyway
            // when its neighbours stand; 0 keeps open ground cheap).
            return 0;
        }
        return static_cast<std::uint8_t>(dm | (mixed ? kMixed : 0));
    }

    void BakeCells() {
        tallest_stand_m_ = 0.0;
        tallest_top_m_ = Terrain::kNoObstacleTop;
        cells_.assign(cover_->grids.size(), {});
        if (cover_->grids.empty()) return;
        double w = 1e300, s = 1e300, ea = -1e300, nn = -1e300;
        for (const CoverGrid& g : cover_->grids) {
            w = std::min(w, g.west_m);
            s = std::min(s, g.south_m);
            ea = std::max(ea, g.west_m + g.cell_m * g.cols);
            nn = std::max(nn, g.south_m + g.cell_m * g.rows);
        }
        block_west_m_ = w;
        block_south_m_ = s;
        block_cols_ = std::max(1, static_cast<int>(std::ceil((ea - w) / kBlockM)));
        block_rows_ = std::max(1, static_cast<int>(std::ceil((nn - s) / kBlockM)));
        blocks_.assign(static_cast<std::size_t>(block_cols_) * static_cast<std::size_t>(block_rows_), Terrain::kNoObstacleTop);
        if (layers_.TallestStand() <= 0.0) return;
        for (std::size_t gi = 0; gi < cover_->grids.size(); ++gi) {
            const CoverGrid& g = cover_->grids[gi];
            std::vector<std::uint8_t>& cells = cells_[gi];
            cells.assign(static_cast<std::size_t>(g.cols) * static_cast<std::size_t>(g.rows), 0);
            for (int r = 0; r < g.rows; ++r) {
                for (int c = 0; c < g.cols; ++c) {
                    const std::uint8_t b = CellByte(g, c, r);
                    if (b == 0) continue;
                    cells[g.Index(c, r)] = b;
                    const double stand = static_cast<double>(b & 0x7F) / 10.0;
                    // The ground's highest over the cell, a 3 x 3 look with a metre to spare (the lattice's nodes between).
                    double ground = -1e300;
                    for (int j = 0; j <= 2; ++j) {
                        for (int i = 0; i <= 2; ++i) {
                            ground = std::max(ground, map_->GroundHeight(g.west_m + g.cell_m * (c + 0.5 * i), g.south_m + g.cell_m * (r + 0.5 * j)));
                        }
                    }
                    const double top = ground + 1.0 + stand;
                    tallest_stand_m_ = std::max(tallest_stand_m_, stand);
                    tallest_top_m_ = std::max(tallest_top_m_, top);
                    // The cell's own block and any its square reaches into.
                    const double cw = g.west_m + g.cell_m * c, cs = g.south_m + g.cell_m * r;
                    for (int br = BlockRow(cs); br <= BlockRow(cs + g.cell_m); ++br) {
                        for (int bc = BlockCol(cw); bc <= BlockCol(cw + g.cell_m); ++bc) {
                            if (bc < 0 || br < 0 || bc >= block_cols_ || br >= block_rows_) continue;
                            double& bt = blocks_[static_cast<std::size_t>(br) * static_cast<std::size_t>(block_cols_) + static_cast<std::size_t>(bc)];
                            bt = std::max(bt, top);
                        }
                    }
                }
            }
        }
    }

    std::shared_ptr<const MapData> map_;
    std::shared_ptr<const CoverRaster> cover_;
    DressingRules dressing_;
    GroundLayerRules layers_;
    int day_ = 288;
    std::vector<std::vector<std::uint8_t>> cells_;  // per cover grid, a byte a cell (CellByte)
    std::vector<double> blocks_;                    // the tallest top in each 500 m block
    double block_west_m_ = 0.0, block_south_m_ = 0.0;
    int block_cols_ = 0, block_rows_ = 0;
    double tallest_stand_m_ = 0.0, tallest_top_m_ = Terrain::kNoObstacleTop;
};

// The crops of the map `map_file` in `data_dir` on day `day`, from data/scenery/map_dressing.json and ground_layers.json.
// The last few are kept, so sessions on one map and day share them and a run of scenarios bakes the map's cover once.
// Null, with no error, when the data folder has no dressing (a bare test folder); null with the reason in `error` when
// the files are there but wrong.
inline std::shared_ptr<const StandingCrops> LoadStandingCropsCached(const std::string& data_dir, const std::string& map_file,
                                                                    std::shared_ptr<const MapData> map, int day, std::string* error = nullptr) {
    static std::mutex lock;
    static std::vector<std::pair<std::string, std::shared_ptr<const StandingCrops>>> kept;  // the newest last
    static std::map<std::string, std::shared_ptr<const CoverRaster>> covers;              // by map and dressing
    constexpr std::size_t kKept = 4;
    const std::string dressing_path = data_dir + "/scenery/map_dressing.json", layers_path = data_dir + "/scenery/ground_layers.json";
    if (!map || !std::ifstream(dressing_path).good() || !std::ifstream(layers_path).good()) return nullptr;
    const std::string map_key = data_dir + "/" + map_file, key = map_key + "#" + std::to_string(day);
    std::lock_guard<std::mutex> guard(lock);
    for (std::size_t i = 0; i < kept.size(); ++i) {
        if (kept[i].first != key) continue;
        std::pair<std::string, std::shared_ptr<const StandingCrops>> hit = kept[i];
        kept.erase(kept.begin() + static_cast<std::ptrdiff_t>(i));
        kept.push_back(hit);
        return hit.second;
    }
    DressingRules dressing;
    GroundLayerRules layers;
    if (!LoadDressingRules(dressing_path, dressing, error) || !LoadGroundLayers(layers_path, dressing, layers, error)) return nullptr;
    std::shared_ptr<const CoverRaster>& cover = covers[map_key];
    if (!cover) cover = std::make_shared<const CoverRaster>(BakeCover(*map, dressing));
    std::shared_ptr<const StandingCrops> built = StandingCrops::Bake(std::move(map), cover, dressing, layers, day);
    kept.emplace_back(key, built);
    if (kept.size() > kKept) kept.erase(kept.begin());
    return built;
}

}  // namespace scenery
}  // namespace simcore
