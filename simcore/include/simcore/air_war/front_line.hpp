// simcore/air_war/front_line.hpp — a front's sectors and its line of contact (track F, F2.1; docs/fronts/02_AIR_FRONT.md
// §2, docs/fronts/README.md decision 1).
//
// The front is one line, kept as offsets along its base line: the theatre front's own line (FrontDef::line, the owner's
// open decision 1 taken as proposed; anchored to the map by F1). Stations every air_front.station_m along it each hold
// how far the line of contact has moved into our country there, along the base line's normal toward our side (the side
// away from the enemy's bases). Sectors are runs of the base line by distance along it (FrontDef::sectors; none: one
// sector the line's length). At zero offsets the line of contact is the base line exactly, and every distance read from
// it is DistanceToLine's to the bit: the campaign and the planner read it in place of the base line (F2.1) and nothing
// changes until the ground war (F3.3) moves a station.
//
// Lookups are a projection on the base line (its nearest point, as DistanceToLine finds it) and an interpolation of the
// offsets at that point: O(the line's segments), a handful. With offsets the signed distance is the projection's less
// the offset there, an approximation that is exact on a straight line and close on the gentle lines of a front.
#pragma once

#include <atomic>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/air_war/air_front_def.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

// A version no line of contact has had before (never 0): what a cache keyed on a line's shape compares, so two lines,
// or a line and its copy once either changes, never share one.
inline unsigned NextLineVersion() {
    static std::atomic<unsigned> last{0};
    unsigned v = ++last;
    if (v == 0) v = ++last;
    return v;
}

// Where a point lies against a base line: its nearest point on the line and which side it is.
struct FrontPlace {
    double along_m = 0.0;        // along the line from its first point to the nearest point (0 .. the line's length)
    double distance_m = 1e18;    // to the line, horizontal and unsigned: DistanceToLine's value
    double side = 1.0;           // +1 on our side of the line (or on it), -1 on the enemy's
    std::size_t segment = 0;     // the segment the nearest point is on
};

class LineOfContact {
public:
    // From a front: its line, its sectors, stations every `station_m` (at least 100 m), every offset zero.
    void Build(const FrontDef& f, double station_m) {
        base_ = f.line;
        cum_.assign(base_.size(), 0.0);
        for (std::size_t i = 1; i < base_.size(); ++i) cum_[i] = cum_[i - 1] + Dist2d(base_[i - 1], base_[i]);
        const double length = Length();
        sectors_ = f.sectors;
        if (sectors_.empty()) {
            SectorDef all;
            all.id = "front";
            all.name = f.name;
            all.from_m = 0.0;
            all.to_m = length;
            sectors_.push_back(all);
        }
        station_m_ = station_m < 100.0 ? 100.0 : station_m;
        std::size_t n = 1;
        if (length > 0.0) n = static_cast<std::size_t>(std::ceil(length / station_m_ - 1e-9)) + 1;
        station_along_.assign(n, 0.0);
        for (std::size_t i = 0; i < n; ++i) station_along_[i] = std::fmin(static_cast<double>(i) * station_m_, length);
        offsets_.assign(n, 0.0);
        moved_ = 0;
        version_ = NextLineVersion();
        // Our side: away from the enemy's airfields.
        our_side_ = 1.0;
        if (!Empty()) {
            const FrontPlace b = PlaceRaw(f.bases);
            const double c = Cross(b.segment, f.bases);
            our_side_ = c > 0.0 ? -1.0 : 1.0;
        }
    }

    bool Empty() const { return base_.size() < 2; }
    double Length() const { return cum_.empty() ? 0.0 : cum_.back(); }
    const std::vector<Vec3>& BaseLine() const { return base_; }

    // ---- the sectors

    std::size_t SectorCount() const { return sectors_.size(); }
    const std::vector<SectorDef>& Sectors() const { return sectors_; }
    const SectorDef& Sector(std::size_t i) const { return sectors_[i]; }
    int SectorIndex(const std::string& id) const {
        for (std::size_t i = 0; i < sectors_.size(); ++i) {
            if (sectors_[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }
    // The sector at a distance along the line: the first whose run ends beyond it; past the last sector's end, the last
    // (the line's ends belong to the end sectors, whatever their written runs).
    int SectorAt(double along_m) const {
        for (std::size_t i = 0; i + 1 < sectors_.size(); ++i) {
            if (along_m < sectors_[i].to_m) return static_cast<int>(i);
        }
        return static_cast<int>(sectors_.size()) - 1;
    }
    // The sector a point is in, by its projection on the base line.
    int SectorOf(const Vec3& p) const { return Empty() ? 0 : SectorAt(PlaceRaw(p).along_m); }
    // A sector's run along the line as the lookups treat it: the first open before the line's start, the last after its end.
    double SectorFrom(std::size_t i) const { return i == 0 ? -1e18 : sectors_[i].from_m; }
    double SectorTo(std::size_t i) const { return i + 1 >= sectors_.size() ? 1e18 : sectors_[i].to_m; }

    // ---- the stations and their offsets

    std::size_t StationCount() const { return station_along_.size(); }
    double StationSpacing() const { return station_m_; }
    double StationAlong(std::size_t i) const { return station_along_[i]; }
    double StationOffset(std::size_t i) const { return offsets_[i]; }
    // How far the line of contact has moved into our country at station i (negative: into the enemy's). The ground war's
    // (F3.3); each change bumps Version().
    void SetStationOffset(std::size_t i, double offset_m) {
        if (i >= offsets_.size() || offsets_[i] == offset_m) return;
        if (offsets_[i] != 0.0) --moved_;
        offsets_[i] = offset_m;
        if (offset_m != 0.0) ++moved_;
        version_ = NextLineVersion();
    }
    // Whether any station has moved off the base line.
    bool Moved() const { return moved_ > 0; }
    // New with every Build and every offset changed, unique among lines (NextLineVersion): what a cache keyed on the
    // line's shape compares.
    unsigned Version() const { return version_; }
    // The offset at a distance along the line, linear between its stations; 0 everywhere while no station has moved.
    double Offset(double along_m) const {
        if (moved_ == 0) return 0.0;
        const std::size_t n = station_along_.size();
        if (n == 1) return offsets_[0];
        double k = std::floor(along_m / station_m_);
        if (k < 0.0) k = 0.0;
        std::size_t i = static_cast<std::size_t>(k);
        if (i > n - 2) i = n - 2;
        const double span = station_along_[i + 1] - station_along_[i];
        const double t = span > 0.0 ? Clamp01((along_m - station_along_[i]) / span) : 0.0;
        return Lerp(offsets_[i], offsets_[i + 1], t);
    }

    // ---- where a point lies

    // Its projection on the base line: the nearest point (the first segment's on a tie, as DistanceToLine's min) and the side.
    FrontPlace Place(const Vec3& p) const { return Empty() ? FrontPlace() : PlaceRaw(p); }
    // How far p is beyond the line of contact into our country (negative on the enemy's side of it).
    double DistanceTo(const Vec3& p) const {
        if (Empty()) return 1e18;
        const FrontPlace pl = PlaceRaw(p);
        return pl.side * pl.distance_m - Offset(pl.along_m);
    }
    // How far p is from the line of contact, either side: at zero offsets DistanceToLine(the base line, p) to the bit.
    double AbsDistance(const Vec3& p) const {
        if (Empty()) return 1e18;
        if (moved_ == 0) return DistanceToLine(base_, p);
        return std::fabs(DistanceTo(p));
    }

    // ---- the line's geometry

    // The base line's point at a distance along it (clamped to the line).
    Vec3 PointAt(double along_m) const {
        if (Empty()) return base_.empty() ? Vec3{} : base_[0];
        const std::size_t i = SegmentAt(along_m);
        const double len = cum_[i + 1] - cum_[i];
        const double t = len > 0.0 ? Clamp01((along_m - cum_[i]) / len) : 0.0;
        return base_[i] + (base_[i + 1] - base_[i]) * t;
    }
    // The unit normal of the base line toward our side at a distance along it (the segment's there).
    Vec3 InwardNormal(double along_m) const {
        if (Empty()) return Vec3{};
        const std::size_t i = SegmentAt(along_m);
        const double dx = base_[i + 1].x - base_[i].x, dy = base_[i + 1].y - base_[i].y;
        const double len = std::hypot(dx, dy);
        if (len <= 0.0) return Vec3{};
        return Vec3{-dy / len * our_side_, dx / len * our_side_, 0.0};
    }
    // Where the line of contact is at station i: the base line's point moved by its offset toward our side.
    Vec3 ContactPoint(std::size_t i) const {
        const double a = station_along_[i];
        return PointAt(a) + InwardNormal(a) * offsets_[i];
    }

private:
    std::vector<Vec3> base_;
    std::vector<double> cum_;            // distance along the line at each of its points
    std::vector<SectorDef> sectors_;
    std::vector<double> station_along_;
    std::vector<double> offsets_;
    double station_m_ = 5000.0;
    double our_side_ = 1.0;              // the sign of Cross on our side of the line
    int moved_ = 0;                      // stations with a non-zero offset
    unsigned version_ = 0;

    // The cross product of segment i's direction and p's place from its start: > 0 on its left.
    double Cross(std::size_t i, const Vec3& p) const {
        const Vec3& a = base_[i];
        const Vec3& b = base_[i + 1];
        return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    }
    std::size_t SegmentAt(double along_m) const {
        std::size_t i = 0;
        while (i + 2 < base_.size() && along_m > cum_[i + 1]) ++i;
        return i;
    }
    FrontPlace PlaceRaw(const Vec3& p) const {
        FrontPlace out;
        double best_t = 0.0;
        for (std::size_t i = 0; i + 1 < base_.size(); ++i) {
            double t = 0.0;
            const double d = SegmentDistance2d(p, base_[i], base_[i + 1], &t);
            if (d < out.distance_m) {
                out.distance_m = d;
                out.segment = i;
                best_t = t;
            }
        }
        out.along_m = cum_[out.segment] + best_t * (cum_[out.segment + 1] - cum_[out.segment]);
        out.side = Cross(out.segment, p) * our_side_ >= 0.0 ? 1.0 : -1.0;
        return out;
    }
};

}  // namespace airwar
}  // namespace simcore
