// simcore/air_war/route_planner.hpp — routes as they were flown (docs/air_battle/01_ROUTES.md, roadmap track B, B2).
//
// A route of 1985 was a line of turn points planned on paper maps, over features a crew could recognise, low over the
// ground the planners chose, through the front at a gate and a time, to an IP on a feature, the attack, an egress turn
// and home another way (afm_51_40_1973, fm_100_42_1976, fm_100_2_1_1984). Neither side expected to fly round every
// defence (mt_1984_babich): the planner keeps the believed exposure low, weighted by the time in each believed site's
// sight (crawford_1977_rand, dorsett_1980), prefers the low ground, and accepts the target's own defence on the last leg.
//
//   RouteGround   the map's features (settlements, bridges, junctions, river bends, lake ends, landmarks; on a theatre
//                 without a map its fixed objects and a lattice) and the ground: viewsheds of believed (or true) sites,
//                 cached, and a line of sight. Caches only: what is computed never depends on what was asked before.
//   RoutePlanner  one flight's route from the believed threats and the ground: a pure function, no random draw.
//
// Everything is iterated in id order; no unordered container decides an order.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/front_line.hpp"
#include "simcore/air_war/intel.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"

namespace simcore {
namespace airwar {

// ---------------------------------------------------------------------------------------------- heights and speeds

// The type's low-level height on this part of the day (01_ROUTES.md §2): the sourced "low_level" values, at night the
// terrain-following height of a type that has one; a helicopter's contour at night (route.helo_night_agl_m).
inline double RouteLowAgl(const AircraftType& a, DayPart part, const TuningTable& tune) {
    const bool night = part == DayPart::Night;
    if (a.helicopter) return night ? std::max(a.low_agl_m, tune.Get("route.helo_night_agl_m")) : a.low_agl_m;
    if (night && a.low_agl_night_m > 0.0) return a.low_agl_night_m;
    return a.low_level_agl_m > 0.0 ? a.low_level_agl_m : a.low_agl_m;
}
// The height a flight of this profile flies its legs at (Low: RouteLowAgl).
inline double RouteProfileAgl(const AircraftType& a, Profile p, DayPart part, const TuningTable& tune) {
    switch (p) {
        case Profile::Low: return RouteLowAgl(a, part, tune);
        case Profile::Medium: return a.medium_alt_m;
        case Profile::High: return std::min(10000.0, a.medium_alt_m * 2.0);
    }
    return a.low_agl_m;
}
// The speed low over enemy ground (the type's low_mps, else its attack speed).
inline double RouteLowMps(const AircraftType& a) { return a.low_mps > 0.0 ? a.low_mps : a.attack_mps; }

// Times along a route by each leg's own speed (RoutePoint::leg_mps, the leg from the point to the next), the flight over
// its release point (the "target" tag; else its first orbit point; else its last point) at `anchor_h` (01_ROUTES.md §6.6).
inline void TimeRoute(std::vector<RoutePoint>& r, double anchor_h) {
    if (r.empty()) return;
    std::size_t anchor = r.size() - 1;
    bool found = false;
    for (std::size_t i = 0; i < r.size() && !found; ++i) {
        if (r[i].tag == "target") {
            anchor = i;
            found = true;
        }
    }
    for (std::size_t i = 0; i < r.size() && !found; ++i) {
        if (r[i].tag == "orbit") {
            anchor = i;
            found = true;
        }
    }
    const auto speed = [&](std::size_t i) {
        for (std::size_t k = i + 1; k-- > 0;) {
            if (r[k].leg_mps > 0.0) return r[k].leg_mps;
        }
        return 200.0;
    };
    r[anchor].t_h = anchor_h;
    for (std::size_t i = anchor + 1; i < r.size(); ++i) r[i].t_h = r[i - 1].t_h + Dist2d(r[i - 1].pos, r[i].pos) / speed(i - 1) / 3600.0;
    for (std::size_t i = anchor; i-- > 0;) r[i].t_h = r[i + 1].t_h - Dist2d(r[i].pos, r[i + 1].pos) / speed(i) / 3600.0;
}

// The index of the first point with this tag, -1 none.
inline int RouteIndexOf(const std::vector<RoutePoint>& r, const char* tag) {
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (r[i].tag == tag) return static_cast<int>(i);
    }
    return -1;
}

// A flight's navigation error between its fixes (roadmap S7, route.mode 2): each point's offset from where it was planned,
// in the theatre frame. A fix is a point on a map feature the crew sees (or the route's first point, the IP and the
// target, which it finds by eye): there the error is gone. Between fixes the flight drifts along one direction, drawn
// from `key` and the fix's count (no campaign or session stream), by `share` of the distance flown since the fix, never
// more than `cap_m`. Every aircraft of the flight flies the same error.
inline std::vector<Vec3> NavigationDrift(const std::vector<RoutePoint>& r, double share, double cap_m, std::uint64_t key) {
    std::vector<Vec3> off(r.size());
    if (share <= 0.0 || cap_m <= 0.0) return off;
    double since_m = 0.0;
    std::uint64_t fixes = 0;
    double dir = 0.0;
    for (std::size_t i = 0; i < r.size(); ++i) {
        const bool fix = i == 0 || !r[i].feature.empty() || r[i].tag == "ip" || r[i].tag == "target";
        if (fix) {
            since_m = 0.0;
            Rng g(key * 0x9E3779B97F4A7C15ull + (++fixes) * 0xC2B2AE3D27D4EB4Full);
            dir = 6.283185307179586 * g.Uniform();
            continue;
        }
        since_m += Dist2d(r[i - 1].pos, r[i].pos);
        const double d = std::min(share * since_m, cap_m);
        off[i] = Vec3{d * std::sin(dir), d * std::cos(dir), 0.0};
    }
    return off;
}

// The route as flown back from a session (roadmap S7, route.mode 2): the flight was last seen at `at` (the theatre's
// frame) at `t_h`. That place goes into its route after the points it had passed by then (tag "flown"), and the points
// after it move in time by when it can reach the next of them at the leg's planned speed. Every index into the route
// after the new point moves on by one. False: nothing to change (no route, or the session ended after its last point).
inline bool FlownInto(Flight& fl, const Vec3& at, double t_h) {
    std::vector<RoutePoint>& r = fl.route;
    if (r.size() < 2) return false;
    std::size_t j = 0;
    while (j < r.size() && r[j].t_h <= t_h) ++j;
    if (j == 0 || j >= r.size()) return false;
    const RoutePoint& a = r[j - 1];
    const RoutePoint& b = r[j];
    const double planned_s = (b.t_h - a.t_h) * 3600.0;
    const double v = a.leg_mps > 0.0 ? a.leg_mps : (planned_s > 1.0 ? Dist2d(a.pos, b.pos) / planned_s : 200.0);
    const double next_h = t_h + Dist2d(at, b.pos) / std::max(1.0, v) / 3600.0;
    const double shift_h = next_h - b.t_h;
    RoutePoint f;
    f.pos = Vec3{at.x, at.y, 0.0};
    f.t_h = t_h;
    f.agl_m = a.agl_m;
    f.leg_mps = a.leg_mps;
    f.tag = "flown";
    for (std::size_t k = j; k < r.size(); ++k) r[k].t_h += shift_h;
    r.insert(r.begin() + static_cast<std::ptrdiff_t>(j), f);
    const int ji = static_cast<int>(j);
    const auto moved = [ji](int& i) {
        if (i >= ji) ++i;
    };
    moved(fl.release_index);
    for (ZoneEscape& z : fl.escapes) {
        moved(z.first);
        moved(z.last);
        moved(z.rejoin);
    }
    return true;
}

inline double RouteLength(const std::vector<RoutePoint>& r) {
    double s = 0.0;
    for (std::size_t i = 0; i + 1 < r.size(); ++i) s += Dist2d(r[i].pos, r[i + 1].pos);
    return s;
}

// FNV-1a over text (stable on every standard library, unlike std::hash).
inline std::uint64_t Fnv1a(const std::string& s, std::uint64_t h = 1469598103934665603ull) {
    for (char c : s) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;
    }
    return h;
}

// ---------------------------------------------------------------------------------------------- the ground

struct RouteFeature {
    std::string id;  // "settlement:<id>", "crossing:<id>", "junction:<net>:<settlement>:<n>", "river:<id>:<k>",
                     // "lake:<id>:<a|b>", "landmark:<id>"; without a map "object:<id>", "grid:<i>:<j>"
    Vec3 pos;
    bool on_line = false;  // on a road, railway or river (the rebels' navigation by lines)
};

// What a site sees around it within its ring: for each ray and range step, the least height above the ground a target
// there must fly at to be seen (the ground's horizon from the antenna, the earth's curvature at 4/3 radius).
struct Viewshed {
    Vec3 site;
    double radius_m = 0.0;
    double step_m = 1000.0;
    int rays = 0;
    int steps = 0;
    std::vector<float> need;  // [ray * steps + k - 1]

    // The least height seen at p (+inf beyond the ring).
    double NeedAt(const Vec3& p) const {
        const double dx = p.x - site.x, dy = p.y - site.y;
        const double d = std::hypot(dx, dy);
        if (d >= radius_m) return std::numeric_limits<double>::infinity();
        if (d < 0.5 * step_m || rays <= 0 || steps <= 0) return 0.0;
        double ang = std::atan2(dx, dy);
        if (ang < 0.0) ang += 6.283185307179586;
        const int ray = static_cast<int>(std::lround(ang / 6.283185307179586 * rays)) % rays;
        const int k = std::min(steps, std::max(1, static_cast<int>(std::lround(d / step_m))));
        return need[static_cast<std::size_t>(ray) * static_cast<std::size_t>(steps) + static_cast<std::size_t>(k - 1)];
    }
};

class RouteGround {
public:
    // The features and the ground of the theatre: the country map's when the theatre names one, else its fixed objects
    // and a lattice over flat ground (01_ROUTES.md §6.2).
    void Build(const AirWarData& d) {
        country_ = d.country;
        const TuningTable& tune = d.tuning;
        step_m_ = tune.Get("route.view_step_m");
        antenna_m_ = tune.Get("route.mask_antenna_m");
        features_.clear();
        views_.clear();
        near_.clear();
        if (country_) {
            BuildMapFeatures(tune);
        } else {
            BuildTheatreFeatures(d, tune);
        }
        std::stable_sort(features_.begin(), features_.end(), [](const RouteFeature& a, const RouteFeature& b) { return a.id < b.id; });
        BuildBuckets();
    }
    // A ground and features of the caller's (tests: a synthetic map).
    void BuildFrom(std::shared_ptr<const CountryMap> country, const TuningTable& tune) {
        AirWarData d;
        d.tuning = tune;
        d.country = std::move(country);
        Build(d);
    }

    bool HasGround() const { return country_ != nullptr; }
    double Height(double e, double n) const { return country_ ? country_->GroundHeight(e, n) : 0.0; }
    const std::vector<RouteFeature>& Features() const { return features_; }
    int IndexOf(const std::string& id) const {
        const auto it = std::lower_bound(features_.begin(), features_.end(), id, [](const RouteFeature& f, const std::string& s) { return f.id < s; });
        return it != features_.end() && it->id == id ? static_cast<int>(it - features_.begin()) : -1;
    }
    double StepM() const { return step_m_; }

    // Features within `half_m` of any of the segments, at most `max_n` (the nearest), in id order.
    std::vector<int> Near(const std::vector<std::pair<Vec3, Vec3>>& segs, double half_m, int max_n) const {
        if (features_.empty()) return {};
        // Remembered by the question (a cache only: the same segments always give the same features).
        std::uint64_t key = 1469598103934665603ull;
        const auto mix = [&key](double v) {
            key ^= static_cast<std::uint64_t>(std::llround(v));
            key *= 1099511628211ull;
        };
        for (const auto& s : segs) {
            mix(s.first.x);
            mix(s.first.y);
            mix(s.second.x);
            mix(s.second.y);
        }
        mix(half_m);
        mix(max_n);
        const auto hit = near_.find(key);
        if (hit != near_.end()) return hit->second;
        if (near_.size() > 8192) near_.clear();
        std::vector<int>& out = near_[key];
        std::vector<std::pair<double, int>> found;
        std::set<int> cells;
        for (const auto& s : segs) {
            const int x0 = Cell(std::min(s.first.x, s.second.x) - half_m, bx0_), x1 = Cell(std::max(s.first.x, s.second.x) + half_m, bx0_);
            const int y0 = Cell(std::min(s.first.y, s.second.y) - half_m, by0_), y1 = Cell(std::max(s.first.y, s.second.y) + half_m, by0_);
            for (int y = std::max(0, y0); y <= std::min(ny_ - 1, y1); ++y) {
                for (int x = std::max(0, x0); x <= std::min(nx_ - 1, x1); ++x) cells.insert(y * nx_ + x);
            }
        }
        for (int c : cells) {
            for (int i : buckets_[static_cast<std::size_t>(c)]) {
                double best = 1e18;
                for (const auto& s : segs) best = std::min(best, SegmentDistance2d(features_[static_cast<std::size_t>(i)].pos, s.first, s.second));
                if (best <= half_m) found.push_back({best, i});
            }
        }
        std::sort(found.begin(), found.end());
        if (max_n > 0 && static_cast<int>(found.size()) > max_n) found.resize(static_cast<std::size_t>(max_n));
        for (const auto& f : found) out.push_back(f.second);
        std::sort(out.begin(), out.end());
        return out;
    }
    // The feature nearest p within max_m (-1 none; ties by id).
    int Nearest(const Vec3& p, double max_m) const {
        const std::vector<int> n = Near({{p, p}}, max_m, 0);
        int best = -1;
        double best_d = 1e18;
        for (int i : n) {
            const double d = Dist2d(features_[static_cast<std::size_t>(i)].pos, p);
            if (d < best_d - 1e-9) {
                best_d = d;
                best = i;
            }
        }
        return best;
    }

    // What a site sees within its ring (cached by its key, place and ring).
    const Viewshed& View(const std::string& key, const Vec3& site, double radius_m) const {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "|%.0f|%.0f|%.0f", std::round(site.x / 10.0), std::round(site.y / 10.0), std::round(radius_m / 100.0));
        const std::string k = key + buf;
        const auto it = views_.find(k);
        if (it != views_.end()) return it->second;
        if (views_.size() > 4096) views_.clear();  // a cache only
        Viewshed& v = views_[k];
        MakeView(v, site, radius_m);
        return v;
    }

    // How much of a-b a site sees within its ring at heights above the ground from agl_a to agl_b (masked by the ground
    // when the map has one and `masked`), and the longest stretch seen in one piece.
    double Seen(const std::string& key, const Vec3& site, double ring_m, const Vec3& a, const Vec3& b, double agl_a, double agl_b,
                bool masked, double* longest = nullptr) const {
        if (longest != nullptr) *longest = 0.0;
        const double len = Dist2d(a, b);
        if (len <= 0.0 || SegmentDistance2d(site, a, b) >= ring_m) return 0.0;
        if (!masked || !country_) {
            const double c = ChordInCircle2d(a, b, site, ring_m);
            if (longest != nullptr) *longest = c;
            return c;
        }
        const Viewshed& v = View(key, site, ring_m);
        // Only the part of the leg inside the ring is sampled (beyond it the site sees nothing).
        const double dx = b.x - a.x, dy = b.y - a.y, fx = a.x - site.x, fy = a.y - site.y;
        const double qa = dx * dx + dy * dy, qb = 2.0 * (fx * dx + fy * dy), qc = fx * fx + fy * fy - ring_m * ring_m;
        const double disc = qb * qb - 4.0 * qa * qc;
        if (disc <= 0.0) return 0.0;
        const double sq = std::sqrt(disc);
        const double t0 = std::max(0.0, (-qb - sq) / (2.0 * qa)), t1 = std::min(1.0, (-qb + sq) / (2.0 * qa));
        if (t1 <= t0) return 0.0;
        const double piece = (t1 - t0) * len;
        const double ds_target = std::max(100.0, 0.5 * step_m_);
        const int n = std::max(1, static_cast<int>(std::ceil(piece / ds_target)));
        const double ds = piece / n;
        double seen = 0.0, run = 0.0, best = 0.0;
        for (int i = 0; i < n; ++i) {
            const double t = t0 + (t1 - t0) * (i + 0.5) / n;
            const Vec3 p = a + (b - a) * t;
            const double agl = agl_a + (agl_b - agl_a) * t;
            if (agl >= v.NeedAt(p)) {
                seen += ds;
                run += ds;
                best = std::max(best, run);
            } else {
                run = 0.0;
            }
        }
        if (longest != nullptr) *longest = best;
        return seen;
    }

    // Whether a point at height h_a above the ground sees a point at h_b (the ground between, the earth's curvature).
    bool LineOfSight(const Vec3& a, double h_a, const Vec3& b, double h_b) const {
        if (!country_) return true;
        const double len = Dist2d(a, b);
        const double za = Height(a.x, a.y) + h_a, zb = Height(b.x, b.y) + h_b;
        const int n = std::max(2, static_cast<int>(std::ceil(len / std::max(100.0, 0.5 * step_m_))));
        for (int i = 1; i < n; ++i) {
            const double t = static_cast<double>(i) / n;
            const Vec3 p = a + (b - a) * t;
            const double x = t * len;
            const double bulge = x * (len - x) / (2.0 * kEffectiveEarthM);
            if (Height(p.x, p.y) + bulge > za + (zb - za) * t) return false;
        }
        return true;
    }

private:
    static constexpr double kEffectiveEarthM = 6371000.0 * 4.0 / 3.0;
    static constexpr double kCellM = 10000.0;
    std::shared_ptr<const CountryMap> country_;
    std::vector<RouteFeature> features_;
    double step_m_ = 1000.0;
    double antenna_m_ = 6.0;
    double bx0_ = 0.0, by0_ = 0.0;
    int nx_ = 0, ny_ = 0;
    std::vector<std::vector<int>> buckets_;
    mutable std::unordered_map<std::uint64_t, std::vector<int>> near_;
    mutable std::map<std::string, Viewshed> views_;

    static int Cell(double v, double origin) { return static_cast<int>(std::floor((v - origin) / kCellM)); }

    void BuildBuckets() {
        buckets_.clear();
        nx_ = ny_ = 0;
        if (features_.empty()) return;
        double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
        for (const RouteFeature& f : features_) {
            x0 = std::min(x0, f.pos.x);
            y0 = std::min(y0, f.pos.y);
            x1 = std::max(x1, f.pos.x);
            y1 = std::max(y1, f.pos.y);
        }
        bx0_ = x0;
        by0_ = y0;
        nx_ = Cell(x1, x0) + 1;
        ny_ = Cell(y1, y0) + 1;
        buckets_.assign(static_cast<std::size_t>(nx_) * static_cast<std::size_t>(ny_), {});
        for (std::size_t i = 0; i < features_.size(); ++i) {
            const int cx = Cell(features_[i].pos.x, bx0_), cy = Cell(features_[i].pos.y, by0_);
            buckets_[static_cast<std::size_t>(cy * nx_ + cx)].push_back(static_cast<int>(i));
        }
    }

    void Add(const std::string& id, double e, double n, bool on_line = false) { features_.push_back({id, Vec3{e, n, 0.0}, on_line}); }

    void BuildMapFeatures(const TuningTable& tune) {
        const CountryDef& def = country_->Def();
        for (const CountrySettlementDef& s : def.settlements) Add("settlement:" + s.id, s.centre.east_m, s.centre.north_m);
        for (const CountryCrossing& c : country_->Crossings()) Add("crossing:" + c.id, c.at.east_m, c.at.north_m, true);
        for (const CountryLandmarkDef& l : def.landmarks) Add("landmark:" + l.id, l.at.east_m, l.at.north_m);
        // Junctions of three or more ways, named by the nearest settlement and numbered in order of distance from it.
        for (CountryNet net : {CountryNet::Road, CountryNet::Rail}) {
            struct J {
                std::string town;
                double d;
                double e, n;
            };
            std::vector<J> js;
            for (const CountryNode& node : country_->Network(net).Nodes()) {
                if (node.edges.size() < 3 || node.crossing >= 0) continue;
                double d = 0.0;
                const CountrySettlementDef* s = country_->NearestSettlement(node.at.east_m, node.at.north_m, &d);
                js.push_back({s != nullptr ? s->id : std::string("none"), d, node.at.east_m, node.at.north_m});
            }
            std::stable_sort(js.begin(), js.end(), [](const J& a, const J& b) {
                if (a.town != b.town) return a.town < b.town;
                if (a.d != b.d) return a.d < b.d;
                if (a.e != b.e) return a.e < b.e;
                return a.n < b.n;
            });
            std::map<std::string, int> count;
            for (const J& j : js) {
                // Two nodes within a few hundred metres are one junction to a navigator.
                bool dup = false;
                for (const RouteFeature& f : features_) {
                    if (std::hypot(f.pos.x - j.e, f.pos.y - j.n) < 1500.0) dup = true;
                }
                if (dup) continue;
                Add(std::string("junction:") + CountryNetName(net) + ":" + j.town + ":" + std::to_string(count[j.town]++), j.e, j.n, true);
            }
        }
        // River bends: where the line turns more than route.bend_deg.
        const double bend = DegToRad(tune.Get("route.bend_deg"));
        for (const MapLineDef& r : def.ground.rivers) {
            for (std::size_t k = 1; k + 1 < r.points.size(); ++k) {
                const MapPoint& a = r.points[k - 1];
                const MapPoint& b = r.points[k];
                const MapPoint& c = r.points[k + 1];
                const double h1 = std::atan2(b.east_m - a.east_m, b.north_m - a.north_m);
                const double h2 = std::atan2(c.east_m - b.east_m, c.north_m - b.north_m);
                double dh = std::fabs(h2 - h1);
                if (dh > 3.141592653589793) dh = 6.283185307179586 - dh;
                if (dh > bend) Add("river:" + r.id + ":" + std::to_string(k), b.east_m, b.north_m, true);
            }
        }
        // A lake's two ends: the shore points farthest apart.
        for (const MapLakeDef& l : def.ground.lakes) {
            std::size_t ia = 0, ib = 0;
            double best = -1.0;
            for (std::size_t i = 0; i < l.points.size(); ++i) {
                for (std::size_t j = i + 1; j < l.points.size(); ++j) {
                    const double d = std::hypot(l.points[i].east_m - l.points[j].east_m, l.points[i].north_m - l.points[j].north_m);
                    if (d > best) {
                        best = d;
                        ia = i;
                        ib = j;
                    }
                }
            }
            if (best > 0.0) {
                Add("lake:" + l.id + ":a", l.points[ia].east_m, l.points[ia].north_m);
                Add("lake:" + l.id + ":b", l.points[ib].east_m, l.points[ib].north_m);
            }
        }
    }

    // Without a map: the theatre's fixed objects that are no threat (towns, bridges, depots, works) and a lattice.
    void BuildTheatreFeatures(const AirWarData& d, const TuningTable& tune) {
        double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
        const auto grow = [&](const Vec3& p) {
            x0 = std::min(x0, p.x);
            y0 = std::min(y0, p.y);
            x1 = std::max(x1, p.x);
            y1 = std::max(y1, p.y);
        };
        for (const TheatreObject& o : d.theatre.objects) {
            grow(o.pos);
            if (o.mobile || IsAirDefence(o.kind) || o.kind == ObjectKind::EwRadar || o.kind == ObjectKind::Troops || IsColumn(o.kind)) continue;
            Add("object:" + o.id, o.pos.x, o.pos.y, o.kind == ObjectKind::Bridge);
        }
        for (const FrontDef& f : d.theatre.fronts) {
            for (const EntryPoint& e : f.entries) grow(e.pos);
        }
        // Roadmap S7 (route.mode 2): a coarser lattice, so a search on a theatre without a map costs less.
        const double g = tune.Get("route.mode") >= 2.0 ? tune.Get("route.fallback_grid2_m") : tune.Get("route.fallback_grid_m");
        if (g <= 0.0 || x1 < x0) return;
        for (int j = static_cast<int>(std::floor(y0 / g)); j <= static_cast<int>(std::ceil(y1 / g)); ++j) {
            for (int i = static_cast<int>(std::floor(x0 / g)); i <= static_cast<int>(std::ceil(x1 / g)); ++i) {
                Add("grid:" + std::to_string(i) + ":" + std::to_string(j), i * g, j * g);
            }
        }
    }

    void MakeView(Viewshed& v, const Vec3& site, double radius_m) const {
        v.site = site;
        v.radius_m = radius_m;
        v.step_m = step_m_;
        v.rays = std::max(32, static_cast<int>(std::ceil(6.283185307179586 * radius_m / step_m_)));
        v.steps = std::max(1, static_cast<int>(std::ceil(radius_m / step_m_)));
        v.need.assign(static_cast<std::size_t>(v.rays) * static_cast<std::size_t>(v.steps), 0.0f);
        const double z0 = Height(site.x, site.y) + antenna_m_;
        for (int r = 0; r < v.rays; ++r) {
            const double ang = 6.283185307179586 * r / v.rays;
            const double sx = std::sin(ang), sy = std::cos(ang);
            double max_tan = -1e18;
            for (int k = 1; k <= v.steps; ++k) {
                const double dd = k * step_m_;
                const double g = Height(site.x + sx * dd, site.y + sy * dd);
                const double curv = dd * dd / (2.0 * kEffectiveEarthM);
                const double need = k == 1 ? 0.0 : std::max(0.0, max_tan * dd + z0 + curv - g);
                v.need[static_cast<std::size_t>(r) * static_cast<std::size_t>(v.steps) + static_cast<std::size_t>(k - 1)] = static_cast<float>(need);
                max_tan = std::max(max_tan, (g - curv - z0) / dd);
            }
        }
    }
};

// ---------------------------------------------------------------------------------------------- the planner

// A believed threat as the route planner prices it: its ring (the radius and half the position's sigma), and its weight
// (lethality, the belief it is there and working, the spread over the wider ring, and its reach at the flight's height).
struct RouteThreat {
    std::string key;
    Vec3 pos;
    double ring_m = 0.0;
    double weight = 0.0;
    bool masked = false;  // seen only where the ground lets it see (a Low flight on a map with ground)
    // Our fighters as the enemy believes them (roadmap S7, route.mode 2): a ring about a believed air base our
    // interceptors reach from. It costs a leg like a site's ring, but it is no zone: no escape, no believed SAM exposure,
    // no battle position's hiding.
    bool fighter = false;
};

struct RouteRequest {
    Vec3 bases;                                // the front's airfields (the route starts behind its gate)
    const EntryPoint* gate_in = nullptr;       // the front's entry it crosses by
    std::vector<const EntryPoint*> exits;      // where it may come back (the first the cheapest is chosen of)
    Vec3 target;                               // the believed aim
    double release_short_m = 0.0;
    Delivery delivery = Delivery::Count;
    const AircraftType* type = nullptr;
    Profile profile = Profile::Low;
    DayPart part = DayPart::Day;
    int variant = 0;                           // 0 covered, 1 other flank, 2 direct (01_ROUTES.md §6.4)
    double ip_dist_m = 20000.0;
    bool helicopter_hide = false;              // a battle position (hide) instead of an IP
    bool loiter = false;                       // a hunter: a circle round the area instead of an attack
    std::vector<std::string> avoid;            // features charged route.reuse_penalty (variant 0's for the other flank)
    // The enemy's air front by sector (track F, F2.4): a fixed-wing flight lets down where it crosses it and climbs where
    // it comes back inside it; helicopters hold behind their sector's helicopter front. Off (air_front.mode 0): as before.
    AirFrontView air_front;
};

struct PlannedRoute {
    bool ok = false;
    std::vector<RoutePoint> points;  // tagged; leg_mps set; untimed (TimeRoute times them)
    std::string corridor;            // "S2 via settlement:kovar", for the officers' memory
    std::string exit;                // the gate it comes home by
    double exposure_s = 0.0;         // believed lethality-weighted seconds in sight, the target's own zone from the IP on excluded
    double longest_s = 0.0;          // the longest continuous believed exposure to one threat on the way in
    double length_m = 0.0;           // from the bases to the release and back to the bases
    double cost = 0.0;               // the search's cost (time and exposure), for pruning
    std::vector<std::string> turns;  // the features of the way in (the IP's included)
    std::vector<std::string> home;   // and of the way home
    bool hide_unseen = true;         // a helicopter's battle position is hidden from every believed threat at hover height
    bool hide_sees = true;           // and sees the target at its pop-up height
};

// The cached facts of a leg between two features at a height, for one cycle's threats (the cache's owner keys them).
struct LegStats {
    double len_m = 0.0;
    double seen_w_m = 0.0;                          // Σ weight × length seen
    std::vector<std::pair<double, double>> runs;    // (weight, longest seen in one piece) per threat that sees it
    double ground_m = 0.0;                          // mean ground under it
    bool on_line = false;
};
// Keyed by (the threat set's hash, the leg and its height): one cache serves every flight of a cycle.
// A cache keyed by two hashes (a cache only: lookups never decide an order).
struct RouteKeyHash {
    std::size_t operator()(const std::pair<std::uint64_t, std::uint64_t>& k) const {
        // Mixed to the last bit (splitmix64's finaliser): the standard library may take the bucket from the low bits,
        // and a leg's key keeps its height and profile there.
        std::uint64_t h = k.first ^ (k.second + 0x9e3779b97f4a7c15ull + (k.first << 6) + (k.first >> 2));
        h ^= h >> 30;
        h *= 0xbf58476d1ce4e5b9ull;
        h ^= h >> 27;
        h *= 0x94d049bb133111ebull;
        h ^= h >> 31;
        return static_cast<std::size_t>(h);
    }
};
using LegCache = std::unordered_map<std::pair<std::uint64_t, std::uint64_t>, LegStats, RouteKeyHash>;

// A hash of a threat set as the planner prices it (FNV-1a over keys, places, rings, weights and masking).
inline std::uint64_t RouteThreatHash(const std::vector<RouteThreat>& threats) {
    std::uint64_t h = 1469598103934665603ull;
    char buf[160];
    for (const RouteThreat& t : threats) {
        std::snprintf(buf, sizeof(buf), "|%.0f|%.0f|%.0f|%.6g|%d", std::round(t.pos.x), std::round(t.pos.y), std::round(t.ring_m), t.weight, t.masked ? 1 : 0);
        h = Fnv1a(t.key + buf, h);
    }
    return h;
}

class RoutePlanner {
public:
    RoutePlanner(const RouteGround& ground, const TuningTable& tune, const RouteRules& rules)
        : g_(ground), tune_(tune), rules_(rules), final_cone_m_(tune.Get("route.final_cone_m")),
          continuous_s_(tune.Get("route.continuous_s")), mode_(tune.Get("route.mode")) {}

    // Plans one route (a pure function of its inputs; `cache` holds leg facts for this cycle's `threats` only).
    PlannedRoute Plan(const RouteRequest& rq, const std::vector<RouteThreat>& threats, LegCache* cache = nullptr) const {
        PlannedRoute out;
        if (rq.type == nullptr || rq.gate_in == nullptr) return out;
        const AircraftType& a = *rq.type;
        const bool heli = a.helicopter;
        threats_ = &threats;
        threat_hash_ = cache != nullptr ? RouteThreatHash(threats) : 0;
        cache_ = cache;
        agl_ = RouteProfileAgl(a, rq.profile, rq.part, tune_);
        transit_agl_ = heli ? agl_ : std::max(agl_, a.medium_alt_m);
        in_mps_ = heli ? a.cruise_mps : (rq.profile == Profile::Low ? RouteLowMps(a) : a.cruise_mps);
        attack_mps_ = heli ? a.cruise_mps : a.attack_mps;
        cruise_mps_ = a.cruise_mps;
        home_mps_ = heli ? cruise_mps_ : (rq.profile == Profile::Low ? attack_mps_ : cruise_mps_);
        w_exp_ = rules_.w_exp * (rq.variant == 2 ? tune_.Get("route.direct_exposure_share") : 1.0);
        prof_key_ = static_cast<std::uint64_t>(rq.profile);
        // The graph: features in the corridors from the gate to the target and from the target to the exits.
        const Vec3 gate = rq.gate_in->pos;
        std::vector<const EntryPoint*> exits = rq.exits.empty() ? std::vector<const EntryPoint*>{rq.gate_in} : rq.exits;
        std::vector<std::pair<Vec3, Vec3>> segs = {{gate, rq.target}};
        for (const EntryPoint* x : exits) segs.push_back({rq.target, x->pos});
        double half = 0.0;
        for (const auto& s : segs) half = std::max(half, tune_.Get("route.corridor_share") * Dist2d(s.first, s.second));
        half = std::max(half, tune_.Get("route.corridor_min_m"));
        const std::vector<int> feats = g_.Near(segs, half, static_cast<int>(tune_.Get("route.max_features")));
        nodes_.clear();
        for (int fi : feats) nodes_.push_back({g_.Features()[static_cast<std::size_t>(fi)].pos, fi, g_.Features()[static_cast<std::size_t>(fi)].id});
        const std::size_t n_feat = nodes_.size();
        const int gate_node = AddNode(gate, "gate:" + rq.gate_in->id);
        std::vector<int> exit_nodes;
        for (const EntryPoint* x : exits) exit_nodes.push_back(x == rq.gate_in ? gate_node : AddNode(x->pos, "gate:" + x->id));
        avoid_.assign(nodes_.size(), false);
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            if (std::find(rq.avoid.begin(), rq.avoid.end(), nodes_[i].id) != rq.avoid.end()) avoid_[i] = true;
        }

        // The run-in: IP candidates round the target, each on a feature where one is near (else the computed point).
        const double ipd = std::min(rq.ip_dist_m, 0.6 * std::max(1.0, Dist2d(gate, rq.target)));
        // Roadmap S7 (route.mode 2): fewer headings on a theatre without a map (its lattice is cost, not features).
        const int headings =
            std::max(1, static_cast<int>(tune_.Get(mode_ >= 2.0 && !g_.HasGround() ? "route.run_in_headings_nomap" : "route.run_in_headings")));
        std::vector<int> ip_nodes;
        for (int h = 0; h < headings; ++h) {
            const double ang = 6.283185307179586 * h / headings;
            const Vec3 want = rq.target - Vec3{std::sin(ang), std::cos(ang), 0.0} * ipd;
            int node = -1;
            if (!rq.helicopter_hide) {
                double best = tune_.Get("route.ip_snap_m");
                for (std::size_t i = 0; i < n_feat; ++i) {
                    const double d = Dist2d(nodes_[i].pos, want);
                    const double dt = Dist2d(nodes_[i].pos, rq.target);
                    if (d < best - 1e-9 && dt > 0.5 * ipd && dt < 1.5 * ipd) {
                        best = d;
                        node = static_cast<int>(i);
                    }
                }
            }
            if (node < 0) node = AddNode(want, "");
            if (std::find(ip_nodes.begin(), ip_nodes.end(), node) == ip_nodes.end()) ip_nodes.push_back(node);
        }
        avoid_.resize(nodes_.size(), false);
        BuildEdges();
        // A gate with no feature within a leg of it (roadmap S7, route.mode 2): its first leg runs to the nearest features
        // however far, as a crew flies a heading from the gate to its first fix. Before S7 such a route was not planned.
        if (mode_ >= 2.0) {
            LinkFar(gate_node);
            for (int x : exit_nodes) LinkFar(x);
        }

        // Ingress: from the gate, the way the flight came (the bearing from its bases).
        Vec3 u_in = gate - rq.bases;
        u_in.z = 0.0;
        u_in = u_in.LengthSquared() > 1.0 ? u_in.Normalized() : Vec3{0.0, 1.0, 0.0};
        std::vector<double> in_cost;
        std::vector<int> in_prev;
        Search(gate_node, u_in, rq.target, ip_nodes, in_cost, in_prev, /*home=*/false);
        int best_ip = -1;
        double best_total = 1e300;
        for (int ip : ip_nodes) {
            if (in_cost[static_cast<std::size_t>(ip)] >= 1e299) continue;
            const Vec3 ipp = nodes_[static_cast<std::size_t>(ip)].pos;
            const int pv = in_prev[static_cast<std::size_t>(ip)];
            const Vec3 from = pv >= 0 ? nodes_[static_cast<std::size_t>(pv)].pos : ipp - u_in;
            if (pv >= 0 && TurnDeg(from, ipp, rq.target) > tune_.Get("route.max_turn_deg")) continue;
            double total = in_cost[static_cast<std::size_t>(ip)] + RunInCost(ipp, rq.target);
            if (rq.helicopter_hide) total += HideCost(rq, ipp);
            if (total < best_total - 1e-9) {
                best_total = total;
                best_ip = ip;
            }
        }
        if (best_ip < 0) return out;
        std::vector<int> in_path = Path(in_prev, best_ip);  // gate .. ip

        // The attack along the IP's run-in, the release, and the egress turn to the cheaper side.
        const Vec3 ip_pos = nodes_[static_cast<std::size_t>(best_ip)].pos;
        Vec3 dir = rq.target - ip_pos;
        dir.z = 0.0;
        const double run = std::max(1.0, dir.HorizontalLength());
        dir = Vec3{dir.x / run, dir.y / run, 0.0};
        const double rs = std::min(rq.release_short_m, 0.8 * run);
        const Vec3 release = rq.target - dir * rs;
        std::vector<int> in_feats;
        for (int i : in_path) {
            if (nodes_[static_cast<std::size_t>(i)].feature >= 0) in_feats.push_back(i);
        }
        for (int i : in_feats) avoid_[static_cast<std::size_t>(i)] = true;  // home another way when one is cheaper
        int best_side = 0, best_exit = -1;
        std::vector<int> best_home;
        double best_home_cost = 1e300;
        Vec3 best_egress;
        const double clear = tune_.Get("route.egress_clear_s") * attack_mps_;
        const double turn = DegToRad(rs > 1500.0 ? 180.0 - tune_.Get("route.egress_turn_deg") : tune_.Get("route.egress_turn_deg"));
        for (int side : {1, -1}) {
            if (rq.loiter) break;
            const Vec3 out_dir = Rotate(dir, side * turn);
            const Vec3 eg = release + dir * (rs > 1500.0 ? 0.0 : clear) + out_dir * tune_.Get("route.egress_m");
            const std::size_t mark = nodes_.size();
            const int eg_node = AddNode(eg, "");
            avoid_.resize(nodes_.size(), false);
            AddEdgesOf(eg_node);
            std::vector<double> hc;
            std::vector<int> hp;
            Search(eg_node, out_dir, MeanOf(exits), exit_nodes, hc, hp, /*home=*/true);
            for (std::size_t xi = 0; xi < exit_nodes.size(); ++xi) {
                const int xn = exit_nodes[xi];
                const double c = hc[static_cast<std::size_t>(xn)];
                if (c >= 1e299) continue;
                const double total = c + LegCost(release, eg, attack_mps_, true, rq.target);
                if (total < best_home_cost - 1e-9) {
                    best_home_cost = total;
                    best_side = side;
                    best_exit = static_cast<int>(xi);
                    best_home = Path(hp, xn);
                    best_egress = eg;
                }
            }
            RemoveNodesFrom(mark);
        }
        if (!rq.loiter && best_exit < 0) return out;

        // The points.
        const EntryPoint* gx = rq.loiter ? rq.gate_in : exits[static_cast<std::size_t>(best_exit)];
        const Vec3 gate_out = gx->pos;
        Vec3 u_out = rq.bases - gate_out;
        u_out.z = 0.0;
        u_out = u_out.LengthSquared() > 1.0 ? u_out.Normalized() : u_in * -1.0;
        const double behind_in = std::max(1.0, Dist2d(rq.bases, gate)), behind_out = std::max(1.0, Dist2d(rq.bases, gate_out));
        std::vector<RoutePoint>& r = out.points;
        const auto pt = [&](const Vec3& p, double agl, double mps, const char* tag, const std::string& feature) {
            RoutePoint q;
            q.pos = Vec3{p.x, p.y, 0.0};
            q.agl_m = agl;
            q.leg_mps = mps;
            q.tag = tag;
            q.feature = feature;
            r.push_back(q);
        };
        if (heli) {
            const double hold = std::min(HoldingBehind(rq, gate), 0.9 * behind_in);
            pt(gate - u_in * hold, agl_, cruise_mps_, "entry", "");
        } else {
            const double start = std::min(tune_.Get("route.start_before_gate_m"), behind_in);
            const double letdown = std::min(rules_.letdown_m, 0.9 * start);
            pt(gate - u_in * start, transit_agl_, cruise_mps_, "entry", "");
            pt(gate - u_in * letdown, agl_, in_mps_, "letdown", "");
            if (rules_.crossing == "checkpoint_gate") {
                const double cp = std::min(tune_.Get("route.checkpoint_before_gate_m"), 0.6 * letdown);
                pt(gate - u_in * cp, agl_, in_mps_, "checkpoint", "");
            }
        }
        pt(gate, agl_, in_mps_, "gate", "gate:" + rq.gate_in->id);
        for (std::size_t k = 1; k + 1 < in_path.size(); ++k) {
            const Node& nd = nodes_[static_cast<std::size_t>(in_path[k])];
            pt(nd.pos, agl_, in_mps_, "turn", nd.id);
            if (RealTurn(in_path, k)) out.turns.push_back(nd.id);
        }
        const Node& ipn = nodes_[static_cast<std::size_t>(best_ip)];
        if (!ipn.id.empty()) out.turns.push_back(ipn.id);
        if (rq.loiter) {
            // A hunter: in to its area, round its circle, and home the way it planned in.
            const double rad = tune_.Get("route.loiter_radius_m");
            for (int i = 0; i < 4; ++i) {
                const double ang = 1.5707963 * i;
                pt(rq.target + Vec3{std::cos(ang), std::sin(ang), 0.0} * rad, agl_, attack_mps_, i == 0 ? "target" : "loiter", "");
            }
            for (std::size_t k = in_path.size() - 1; k-- > 1;) {
                const Node& nd = nodes_[static_cast<std::size_t>(in_path[k])];
                pt(nd.pos, agl_, in_mps_, "turn", nd.id);
            }
        } else {
            pt(ip_pos, rq.helicopter_hide ? tune_.Get("route.helo_hide_agl_m") : agl_, attack_mps_, rq.helicopter_hide ? "hide" : "ip", ipn.id);
            AttackPoints(rq, dir, rs, best_side, r);
            pt(release, r.back().tag == "target" ? r.back().agl_m : agl_, attack_mps_, "target", "");
            if (r.size() >= 2 && r[r.size() - 2].tag == "target") r.erase(r.end() - 2);
            const double home_mps = home_mps_;
            pt(best_egress, agl_, home_mps, "egress", "");
            for (std::size_t k = 1; k + 1 < best_home.size(); ++k) {
                const Node& nd = nodes_[static_cast<std::size_t>(best_home[k])];
                pt(nd.pos, agl_, home_mps, "turn", nd.id);
                // The home path starts at the egress point, whose node was removed with its side's search.
                if (RealTurn(best_home, k, &best_egress)) out.home.push_back(nd.id);
            }
        }
        pt(gate_out, agl_, home_mps_, "gate", "gate:" + gx->id);
        if (heli) {
            pt(gate_out + u_out * std::min(HoldingBehind(rq, gate_out), 0.9 * behind_out), agl_, cruise_mps_, "exit", "");
        } else {
            const double start = std::min(tune_.Get("route.start_before_gate_m"), behind_out);
            pt(gate_out + u_out * std::min(rules_.letdown_m, 0.9 * start), transit_agl_, cruise_mps_, "climb", "");
            pt(gate_out + u_out * start, transit_agl_, cruise_mps_, "exit", "");
            LetDownAtAirFront(rq, r);
        }
        out.exit = gx->id;
        out.cost = rq.loiter ? best_total : best_total + best_home_cost;
        // The corridor: the gate and the feature farthest from the straight line in.
        std::string farthest;
        double far_d = -1.0;
        for (const std::string& id : out.turns) {
            const int fi = g_.IndexOf(id);
            if (fi < 0) continue;
            const double d = SegmentDistance2d(g_.Features()[static_cast<std::size_t>(fi)].pos, gate, rq.target);
            if (d > far_d + 1e-9) {
                far_d = d;
                farthest = id;
            }
        }
        out.corridor = rq.gate_in->id + (farthest.empty() ? std::string(" direct") : " via " + farthest);
        // What it costs in believed exposure, and its length from the bases and back.
        Exposure(out, rq);
        out.length_m = RouteLength(r) + Dist2d(rq.bases, r.front().pos) + Dist2d(r.back().pos, rq.bases);
        if (rq.helicopter_hide) {
            out.hide_unseen = HiddenFromAll(ip_pos);
            out.hide_sees = g_.LineOfSight(ip_pos, tune_.Get("route.helo_popup_agl_m"), rq.target, 5.0);
        }
        out.ok = true;
        return out;
    }

    // The Pact's counter-SAM programme for each believed zone a route crosses (01_ROUTES.md §9).
    static std::vector<ZoneEscape> Escapes(const std::vector<RoutePoint>& r, const std::vector<RouteThreat>& threats) {
        std::vector<ZoneEscape> out;
        for (const RouteThreat& th : threats) {
            if (th.weight <= 0.0 || th.fighter) continue;  // a zone that cannot reach the flight's height needs no escape
            int first = -1, last = -1;
            for (std::size_t i = 0; i + 1 <= r.size(); ++i) {
                const bool in = i + 1 < r.size() && ChordInCircle2d(r[i].pos, r[i + 1].pos, th.pos, th.ring_m) > 0.0;
                if (in) {
                    if (first < 0) first = static_cast<int>(i);
                    last = static_cast<int>(i);
                } else if (first >= 0) {
                    ZoneEscape z;
                    z.threat = th.key;
                    z.first = first;
                    z.last = last;
                    // Where the route enters the ring, the way out of it nearest from there: straight away from the site.
                    const Vec3 a = r[static_cast<std::size_t>(first)].pos, b = r[static_cast<std::size_t>(first) + 1].pos;
                    Vec3 at = a;
                    if (Dist2d(a, th.pos) >= th.ring_m) {
                        double lo = 0.0, hi = 1.0;
                        for (int k = 0; k < 30; ++k) {
                            const double mid = 0.5 * (lo + hi);
                            (Dist2d(a + (b - a) * mid, th.pos) < th.ring_m ? hi : lo) = mid;
                        }
                        at = a + (b - a) * hi;
                    }
                    z.heading_rad = std::atan2(at.x - th.pos.x, at.y - th.pos.y);
                    const double agl = r[static_cast<std::size_t>(first)].agl_m;
                    z.dz_m = agl > 300.0 ? -(agl - 100.0) : -0.5 * agl;
                    z.rejoin = std::min(static_cast<int>(r.size()) - 1, last + 1);
                    out.push_back(z);
                    first = last = -1;
                }
            }
        }
        return out;
    }

private:
    struct Node {
        Vec3 pos;
        int feature = -1;  // the ground's feature index, -1 a computed point
        std::string id;
    };
    const RouteGround& g_;
    const TuningTable& tune_;
    const RouteRules& rules_;
    const double final_cone_m_;
    const double continuous_s_;
    const double mode_;  // route.mode
    mutable const std::vector<RouteThreat>* threats_ = nullptr;
    mutable LegCache* cache_ = nullptr;
    mutable std::vector<Node> nodes_;
    // An edge of the graph: the node it goes to, its heading (as TurnDeg reads it) and, once looked up, its leg's facts in
    // the cycle's cache (roadmap B12: the three searches of a plan share their edges; a reference into an unordered_map
    // stays valid as it grows).
    struct Edge {
        int to = 0;
        double heading_rad = 0.0;
        const LegStats* stats = nullptr;
    };
    mutable std::vector<std::vector<Edge>> adj_;
    mutable std::vector<bool> avoid_;
    mutable double agl_ = 60.0, transit_agl_ = 4000.0, in_mps_ = 220.0, attack_mps_ = 250.0, cruise_mps_ = 220.0, home_mps_ = 220.0, w_exp_ = 1.0;
    mutable std::uint64_t prof_key_ = 0, threat_hash_ = 0;

    int AddNode(const Vec3& p, const std::string& id) const {
        nodes_.push_back({Vec3{p.x, p.y, 0.0}, -1, id});
        return static_cast<int>(nodes_.size()) - 1;
    }
    void RemoveNodesFrom(std::size_t mark) const {
        nodes_.resize(mark);
        avoid_.resize(mark);
        adj_.resize(mark);
        for (auto& a : adj_) a.erase(std::remove_if(a.begin(), a.end(), [&](const Edge& e) { return e.to >= static_cast<int>(mark); }), a.end());
    }

    static Vec3 Rotate(const Vec3& v, double ang) {  // counter-clockwise (to the left) by ang
        const double c = std::cos(ang), s = std::sin(ang);
        return Vec3{v.x * c - v.y * s, v.x * s + v.y * c, 0.0};
    }
    static Vec3 MeanOf(const std::vector<const EntryPoint*>& xs) {
        Vec3 m;
        for (const EntryPoint* x : xs) m = m + x->pos;
        return xs.empty() ? m : m * (1.0 / static_cast<double>(xs.size()));
    }
    static double TurnDeg(const Vec3& a, const Vec3& b, const Vec3& c) {
        return TurnDegOf(std::atan2(b.x - a.x, b.y - a.y), std::atan2(c.x - b.x, c.y - b.y));
    }
    static double TurnDegOf(double h1, double h2) {
        double d = std::fabs(h2 - h1);
        if (d > 3.141592653589793) d = 6.283185307179586 - d;
        return RadToDeg(d);
    }

    double MinLegM() const {
        // A leg at least this many turn radii (480 kt at 60 deg of bank: about 3.6 km, routing §3).
        const double v = std::max(in_mps_, 50.0);
        const double radius = v * v / (9.80665 * std::tan(DegToRad(tune_.Get("route.turn_bank_deg"))));
        return tune_.Get("route.min_leg_turns") * radius;
    }

    // Each node joined to its nearest within the legs' limits (both ways), in order of distance then id.
    mutable double edge_lo_m_ = 0.0, edge_hi_m_ = 0.0;
    mutable int edge_k_ = 8;
    mutable std::vector<std::pair<double, int>> edge_scratch_;
    void BuildEdges() const {
        edge_lo_m_ = MinLegM();
        edge_hi_m_ = tune_.Get("route.max_leg_m");
        edge_k_ = static_cast<int>(tune_.Get("route.neighbours"));
        adj_.assign(nodes_.size(), {});
        for (std::size_t i = 0; i < nodes_.size(); ++i) AddEdgesOf(static_cast<int>(i));
    }
    void AddEdgesOf(int i) const {
        adj_.resize(nodes_.size());
        const double lo2 = edge_lo_m_ * edge_lo_m_, hi2 = edge_hi_m_ * edge_hi_m_;
        const std::size_t k = static_cast<std::size_t>(edge_k_);
        std::vector<std::pair<double, int>>& c = edge_scratch_;
        c.clear();
        const Vec3 pi = nodes_[static_cast<std::size_t>(i)].pos;
        for (std::size_t j = 0; j < nodes_.size(); ++j) {
            if (static_cast<int>(j) == i) continue;
            const double ex = nodes_[j].pos.x - pi.x, ey = nodes_[j].pos.y - pi.y;
            const double d2 = ex * ex + ey * ey;
            if (d2 < lo2 || d2 > hi2) continue;
            c.push_back({d2, static_cast<int>(j)});
        }
        if (c.size() > k) {
            std::partial_sort(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(k), c.end());
            c.resize(k);
        } else {
            std::sort(c.begin(), c.end());
        }
        const auto to = [](int j) { return [j](const Edge& x) { return x.to == j; }; };
        for (const auto& e : c) {
            auto& a = adj_[static_cast<std::size_t>(i)];
            auto& b = adj_[static_cast<std::size_t>(e.second)];
            const Vec3& pj = nodes_[static_cast<std::size_t>(e.second)].pos;
            if (std::find_if(a.begin(), a.end(), to(e.second)) == a.end()) a.push_back({e.second, std::atan2(pj.x - pi.x, pj.y - pi.y), nullptr});
            if (std::find_if(b.begin(), b.end(), to(i)) == b.end()) b.push_back({i, std::atan2(pi.x - pj.x, pi.y - pj.y), nullptr});
        }
    }

    // A node with no edge joined to its route.neighbours nearest features at least the least leg away, however far
    // (roadmap S7: the gate's fallback). In order of distance, then index.
    void LinkFar(int i) const {
        if (i < 0 || static_cast<std::size_t>(i) >= adj_.size() || !adj_[static_cast<std::size_t>(i)].empty()) return;
        const Vec3 pi = nodes_[static_cast<std::size_t>(i)].pos;
        std::vector<std::pair<double, int>> c;
        for (std::size_t j = 0; j < nodes_.size(); ++j) {
            if (static_cast<int>(j) == i || nodes_[j].feature < 0) continue;
            const double d = Dist2d(nodes_[j].pos, pi);
            if (d < edge_lo_m_) continue;
            c.push_back({d, static_cast<int>(j)});
        }
        std::sort(c.begin(), c.end());
        if (c.size() > static_cast<std::size_t>(edge_k_)) c.resize(static_cast<std::size_t>(edge_k_));
        for (const auto& e : c) {
            const Vec3& pj = nodes_[static_cast<std::size_t>(e.second)].pos;
            adj_[static_cast<std::size_t>(i)].push_back({e.second, std::atan2(pj.x - pi.x, pj.y - pi.y), nullptr});
            adj_[static_cast<std::size_t>(e.second)].push_back({i, std::atan2(pi.x - pj.x, pi.y - pj.y), nullptr});
        }
    }

    // The leg's facts for this cycle's threats at this height (cached between features).
    // The leg's stats: a reference into the cache, or to the last computed (valid until the next call).
    const LegStats& Stats(int ia, int ib) const {
        const Node& a = nodes_[static_cast<std::size_t>(ia)];
        const Node& b = nodes_[static_cast<std::size_t>(ib)];
        std::uint64_t key = 0;
        const bool cacheable = cache_ != nullptr && a.feature >= 0 && b.feature >= 0;
        if (cacheable) {
            const std::uint64_t lo = static_cast<std::uint64_t>(std::min(a.feature, b.feature));
            const std::uint64_t hi = static_cast<std::uint64_t>(std::max(a.feature, b.feature));
            const std::uint64_t agl = static_cast<std::uint64_t>(std::min(16383.0, std::max(0.0, std::round(agl_))));
            key = (lo << 40) | (hi << 16) | (agl << 2) | prof_key_;
            const auto it = cache_->find({threat_hash_, key});
            if (it != cache_->end()) return it->second;
        }
        LegStats s = Compute(a.pos, b.pos, agl_, agl_, nullptr);
        if (a.feature >= 0 && b.feature >= 0) {
            s.on_line = g_.Features()[static_cast<std::size_t>(a.feature)].on_line && g_.Features()[static_cast<std::size_t>(b.feature)].on_line;
        }
        if (cacheable) return (*cache_)[{threat_hash_, key}] = std::move(s);
        last_stats_ = std::move(s);
        return last_stats_;
    }
    mutable LegStats last_stats_;
    // Whether Stats keeps this leg's facts in the cycle's cache (else in last_stats_, good until the next call).
    bool StatsCached(int ia, int ib) const {
        return cache_ != nullptr && nodes_[static_cast<std::size_t>(ia)].feature >= 0 && nodes_[static_cast<std::size_t>(ib)].feature >= 0;
    }
    LegStats Compute(const Vec3& a, const Vec3& b, double agl_a, double agl_b, const Vec3* skip_ring_of) const {
        LegStats s;
        s.len_m = Dist2d(a, b);
        for (const RouteThreat& th : *threats_) {
            if (th.weight <= 0.0) continue;
            if (skip_ring_of != nullptr && Dist2d(*skip_ring_of, th.pos) < th.ring_m) continue;  // the target's own: accepted
            double longest = 0.0;
            const double seen = g_.Seen(th.key, th.pos, th.ring_m, a, b, agl_a, agl_b, th.masked, &longest);
            if (seen <= 0.0) continue;
            s.seen_w_m += th.weight * seen;
            if (!th.fighter) s.runs.push_back({th.weight, longest});  // a fighter ring is no site's continuous sight (S7)
        }
        if (g_.HasGround() && rules_.w_low > 0.0) {
            const int n = std::max(1, static_cast<int>(std::ceil(s.len_m / 2000.0)));
            double sum = 0.0;
            for (int i = 0; i < n; ++i) {
                const Vec3 p = a + (b - a) * ((i + 0.5) / n);
                sum += g_.Height(p.x, p.y);
            }
            s.ground_m = sum / n;
        }
        return s;
    }

    // cost = t (1 + w_heading) + w_exp Σ weight seen_s + w_cont Σ max(0, longest_s − 20) + w_low t (ground − lowest) / 100 m
    double Cost(const LegStats& s, const Vec3& a, const Vec3& b, double mps, const Vec3& goal) const {
        const double t = s.len_m / mps;
        double w_heading = 0.0;
        const double to_goal = Dist2d(a, goal);
        if (to_goal > 1.0 && s.len_m > 1.0) {
            const double cosang = ((b.x - a.x) * (goal.x - a.x) + (b.y - a.y) * (goal.y - a.y)) / (s.len_m * to_goal);
            // More than 45 degrees off the goal inside the final cone, more than 90 outside it.
            static const double cos45 = std::cos(DegToRad(45.0)), cos90 = std::cos(DegToRad(90.0));
            if (cosang < (to_goal < final_cone_m_ ? cos45 : cos90)) w_heading = 1.0;
        }
        double c = t * (1.0 + w_heading) * (s.on_line ? 1.0 - rules_.lines_bonus : 1.0);
        c += w_exp_ * s.seen_w_m / mps;
        for (const auto& run : s.runs) c += rules_.w_cont * std::max(0.0, run.second / mps - continuous_s_);
        if (rules_.w_low > 0.0 && g_.HasGround()) c += rules_.w_low * t * std::max(0.0, s.ground_m - lowest_) / 100.0;
        return c;
    }
    mutable double lowest_ = 0.0;

    double LegCost(const Vec3& a, const Vec3& b, double mps, bool own_zone_free, const Vec3& target) const {
        const LegStats s = Compute(a, b, agl_, agl_, own_zone_free ? &target : nullptr);
        return Cost(s, a, b, mps, b);
    }
    double RunInCost(const Vec3& ip, const Vec3& target) const { return LegCost(ip, target, attack_mps_, true, target); }

    // A battle position: hidden from every believed threat at hover height, seeing the target at pop-up height.
    bool HiddenFromAll(const Vec3& p) const {
        const double hover = tune_.Get("route.helo_hide_agl_m");
        for (const RouteThreat& th : *threats_) {
            if (th.weight <= 0.0 || th.fighter || Dist2d(p, th.pos) >= th.ring_m) continue;
            if (!th.masked || !g_.HasGround()) return false;
            if (hover >= g_.View(th.key, th.pos, th.ring_m).NeedAt(p)) return false;
        }
        return true;
    }
    double HideCost(const RouteRequest& rq, const Vec3& p) const {
        double c = 0.0;
        if (!HiddenFromAll(p)) c += tune_.Get("route.hide_penalty_s");
        if (!g_.LineOfSight(p, tune_.Get("route.helo_popup_agl_m"), rq.target, 5.0)) c += tune_.Get("route.hide_penalty_s");
        return c;
    }

    // Dijkstra from `start` (entered along `in_dir`) with the turn and turn-point limits, ties by node index.
    void Search(int start, const Vec3& in_dir, const Vec3& goal, const std::vector<int>& goals, std::vector<double>& cost,
                std::vector<int>& prev, bool home) const {
        const std::size_t n = nodes_.size();
        cost.assign(n, 1e300);
        prev.assign(n, -1);
        std::vector<int> hops(n, 0);
        std::vector<bool> done(n, false);
        lowest_ = 1e18;
        for (const Node& nd : nodes_) lowest_ = std::min(lowest_, g_.Height(nd.pos.x, nd.pos.y));
        const double max_turn = tune_.Get("route.max_turn_deg");
        const double straight = tune_.Get("route.straight_deg");
        const double mps = home ? home_mps_ : in_mps_;
        const double penalty = tune_.Get("route.reuse_penalty_s");
        using Item = std::pair<double, int>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> q;
        cost[static_cast<std::size_t>(start)] = 0.0;
        q.push({0.0, start});
        while (!q.empty()) {
            const Item it = q.top();
            q.pop();
            const std::size_t u = static_cast<std::size_t>(it.second);
            if (done[u]) continue;
            done[u] = true;
            const bool is_goal = std::find(goals.begin(), goals.end(), static_cast<int>(u)) != goals.end();
            if (is_goal && static_cast<int>(u) != start) continue;  // a route ends at its goal
            const Vec3 from = prev[u] >= 0 ? nodes_[static_cast<std::size_t>(prev[u])].pos : nodes_[u].pos - in_dir * 1000.0;
            const double h_in = std::atan2(nodes_[u].pos.x - from.x, nodes_[u].pos.y - from.y);  // TurnDeg's first heading
            for (Edge& e : adj_[u]) {
                const int v = e.to;
                const std::size_t vv = static_cast<std::size_t>(v);
                if (done[vv]) continue;
                const bool v_goal = std::find(goals.begin(), goals.end(), v) != goals.end();
                // Only goals and features are turn points; computed points (other goals) are not a way through.
                if (!v_goal && nodes_[vv].feature < 0) continue;
                // A feature flown over straight on is a check on the way, not a turn point: only real turns count
                // against the faction's turn points (the legs' limits keep the graph's own hops short).
                const double turn = TurnDegOf(h_in, e.heading_rad);
                if (turn > max_turn) continue;
                const int hv = hops[u] + (static_cast<int>(u) != start && turn > straight ? 1 : 0);
                if (hv > rules_.max_turn_points) continue;
                const LegStats* sp = e.stats;
                if (sp == nullptr) {
                    sp = &Stats(static_cast<int>(u), v);
                    if (StatsCached(static_cast<int>(u), v)) e.stats = sp;  // a reference into the cycle's cache, not the scratch
                }
                const LegStats& s = *sp;
                double c = cost[u] + Cost(s, nodes_[u].pos, nodes_[vv].pos, mps, goal);
                if (vv < avoid_.size() && avoid_[vv]) c += penalty;
                if (c < cost[vv] - 1e-9) {
                    cost[vv] = c;
                    prev[vv] = static_cast<int>(u);
                    hops[vv] = hv;
                    q.push({c, v});
                }
            }
        }
    }
    // The searched path, the features within route.simplify_m of a straight line between those kept left out: the
    // shape flown is the searched one to that much, with fewer points.
    std::vector<int> Path(const std::vector<int>& prev, int end) const {
        std::vector<int> p;
        for (int v = end; v >= 0; v = prev[static_cast<std::size_t>(v)]) p.push_back(v);
        std::reverse(p.begin(), p.end());
        const double tol = tune_.Get("route.simplify_m");
        const auto at = [&](std::size_t i) -> const Vec3& { return nodes_[static_cast<std::size_t>(p[i])].pos; };
        std::vector<int> kept;
        std::size_t a = 0;
        while (a < p.size()) {
            kept.push_back(p[a]);
            if (a + 1 >= p.size()) break;
            // The farthest point the line from a reaches with every point between within the tolerance.
            std::size_t b = a + 1;
            for (std::size_t j = a + 2; j < p.size(); ++j) {
                bool fits = true;
                for (std::size_t m = a + 1; m < j && fits; ++m) fits = SegmentDistance2d(at(m), at(a), at(j)) <= tol;
                if (!fits) break;
                b = j;
            }
            a = b;
        }
        return kept;
    }
    // Whether the path turns at its k-th point by more than route.straight_deg: a turn point, not a feature flown over.
    // `first`: the path's first point's place, when its node is gone.
    bool RealTurn(const std::vector<int>& p, std::size_t k, const Vec3* first = nullptr) const {
        const Vec3& before = k == 1 && first != nullptr ? *first : nodes_[static_cast<std::size_t>(p[k - 1])].pos;
        return TurnDeg(before, nodes_[static_cast<std::size_t>(p[k])].pos,
                       nodes_[static_cast<std::size_t>(p[k + 1])].pos) > tune_.Get("route.straight_deg");
    }

    // The attack's points between the IP and the release, as the delivery flies it (docs/air_war/10_WEAPONS.md), on the
    // IP's run-in; the pull-up's offset to the side away from the egress.
    void AttackPoints(const RouteRequest& rq, const Vec3& dir, double rs, int egress_side, std::vector<RoutePoint>& r) const {
        const Vec3 side = Rotate(dir, 1.5707963267948966) * (egress_side > 0 ? -1.0 : 1.0);
        const double ipd = Dist2d(r.back().pos, rq.target);
        const auto room = [&](double m) { return std::min(m, 0.9 * ipd); };
        const auto add = [&](const Vec3& p, double agl, const char* tag) {
            RoutePoint q;
            q.pos = Vec3{p.x, p.y, 0.0};
            q.agl_m = agl;
            q.leg_mps = attack_mps_;
            q.tag = tag;
            r.push_back(q);
        };
        double release_agl = agl_;
        if (!rq.type->helicopter) {
            switch (rq.delivery) {
                case Delivery::PopUp: {
                    const double pull = room(tune_.Get("delivery.popup_pull_m"));
                    add(rq.target - dir * pull + side * 1500.0, agl_, "pull_up");
                    add(rq.target - dir * (0.5 * (pull + rs)) + side * 800.0, tune_.Get("delivery.popup_apex_m"), "apex");
                    release_agl = tune_.Get("delivery.popup_release_m");
                    break;
                }
                case Delivery::Loft:
                    add(rq.target - dir * room(tune_.Get("delivery.loft_pull_m")), agl_, "pull_up");
                    release_agl = tune_.Get("delivery.loft_release_m");
                    break;
                case Delivery::Dive:
                    add(rq.target - dir * room(tune_.Get("delivery.dive_roll_in_m")), agl_, "roll_in");
                    release_agl = std::min(agl_, tune_.Get("delivery.dive_release_m"));
                    break;
                case Delivery::GentleDive: {  // roadmap B6: a shallow climb to a low apex and a 10-25 deg dive (mt_1970_bukhantsev)
                    const double pull = room(tune_.Get("delivery.gentle_dive_pull_m"));
                    add(rq.target - dir * pull + side * 800.0, agl_, "pull_up");
                    add(rq.target - dir * (0.5 * (pull + rs)) + side * 400.0, tune_.Get("delivery.gentle_dive_apex_m"), "apex");
                    release_agl = tune_.Get("delivery.gentle_dive_release_m");
                    break;
                }
                default: break;
            }
        } else if (rq.delivery == Delivery::HoverFire) {
            release_agl = tune_.Get("delivery.hover_agl_m");
        }
        add(rq.target - dir * rs, release_agl, "target");
    }

    // The route's believed exposure (the target's own zone from the IP on excluded) and its longest continuous exposure
    // to one threat on the way in.
    // A helicopter's holding area behind a gate (track F, F2.4; 02_AIR_FRONT.md §3-4): route.helo_holding_m behind its
    // sector's helicopter front, never nearer the line than route.helo_holding_floor_m (our artillery's reach). With the
    // helicopters' front at 0 (air_front.mode 0) route.helo_holding_m, as before.
    double HoldingBehind(const RouteRequest& rq, const Vec3& gate) const {
        const double hold = tune_.Get("route.helo_holding_m");
        const double front = rq.air_front.HeloAt(gate);
        if (front <= 0.0) return hold;
        return std::max(std::min(hold, tune_.Get("route.helo_holding_floor_m")), hold - front);
    }

    // The let-down and the climb at the air front (track F, F2.4; 02_AIR_FRONT.md §4). Where the gate is inside its
    // sector's fixed-wing air front, a low flight crosses the line of contact at its transit height and lets down where
    // its way in leaves the air front (the first leg whose end is past it, the crossing found by halving the leg); on the
    // way home it climbs where it comes back inside it. A target inside the air front: the let-down a leg short of the IP
    // (route.letdown_m at most), the climb as far past the egress. The gate's sector at 0 (and air_front.mode 0): the
    // route as built.
    void LetDownAtAirFront(const RouteRequest& rq, std::vector<RoutePoint>& r) const {
        if (!rq.air_front.On() || agl_ >= transit_agl_) return;
        const int gi = RouteIndexOf(r, "gate");
        int go = -1;
        for (std::size_t i = r.size(); i-- > 0;) {
            if (r[i].tag == "gate") {
                go = static_cast<int>(i);
                break;
            }
        }
        if (gi < 0 || go <= gi) return;
        const AirFrontView& af = rq.air_front;
        const auto make = [](const Vec3& p, double agl, double mps, const char* tag) {
            RoutePoint q;
            q.pos = Vec3{p.x, p.y, 0.0};
            q.agl_m = agl;
            q.leg_mps = mps;
            q.tag = tag;
            return q;
        };
        // The crossing on a leg from a point inside the air front to one outside it (or back).
        const auto crossing = [&](const Vec3& a, const Vec3& b) {
            const bool a_in = af.Inside(a);
            double lo = 0.0, hi = 1.0;
            for (int k = 0; k < 24; ++k) {
                const double mid = 0.5 * (lo + hi);
                if (af.Inside(a + (b - a) * mid) == a_in) {
                    lo = mid;
                } else {
                    hi = mid;
                }
            }
            return a + (b - a) * hi;
        };
        const auto short_of = [&](const Vec3& a, const Vec3& b) {  // on the leg a-b, route.letdown_m short of b at most
            const double len = Dist2d(a, b);
            const double back = std::min(rules_.letdown_m, 0.9 * len);
            return len > 0.0 ? b + (a - b) * (back / len) : b;
        };
        // The way home first (its indices are after the way in's): from the attack's last point to the gate out.
        int home_from = -1;
        for (int i = go - 1; i > gi; --i) {
            const std::string& t = r[static_cast<std::size_t>(i)].tag;
            if (t == "egress" || t == "loiter" || t == "target") {
                home_from = i;
                break;
            }
        }
        if (home_from > 0 && af.Inside(r[static_cast<std::size_t>(go)].pos)) {
            int k = go - 1;  // the last point of the way home outside the air front
            while (k > home_from && af.Inside(r[static_cast<std::size_t>(k)].pos)) --k;
            const Vec3 a = r[static_cast<std::size_t>(k)].pos, b = r[static_cast<std::size_t>(k + 1)].pos;
            const Vec3 c = af.Inside(a) ? short_of(b, a) : crossing(a, b);
            const std::size_t old_climb = static_cast<std::size_t>(go) + 1;
            if (old_climb < r.size() && r[old_climb].tag == "climb") r.erase(r.begin() + static_cast<std::ptrdiff_t>(old_climb));
            for (int i = k + 1; i <= go; ++i) {
                r[static_cast<std::size_t>(i)].agl_m = transit_agl_;
                r[static_cast<std::size_t>(i)].leg_mps = cruise_mps_;
            }
            r.insert(r.begin() + (k + 1), make(c, transit_agl_, cruise_mps_, "climb"));
        }
        // The way in: from the gate to the IP (a hunter's: to its first circle point).
        int in_to = -1;
        for (std::size_t i = static_cast<std::size_t>(gi) + 1; i < r.size(); ++i) {
            if (r[i].tag == "ip" || r[i].tag == "hide" || r[i].tag == "target") {
                in_to = static_cast<int>(i);
                break;
            }
        }
        if (in_to < 0 || !af.Inside(r[static_cast<std::size_t>(gi)].pos)) return;
        int k = gi + 1;  // the first point of the way in outside the air front
        while (k < in_to && af.Inside(r[static_cast<std::size_t>(k)].pos)) ++k;
        const Vec3 a = r[static_cast<std::size_t>(k - 1)].pos, b = r[static_cast<std::size_t>(k)].pos;
        const Vec3 c = af.Inside(b) ? short_of(a, b) : crossing(a, b);
        for (int i = 0; i < k; ++i) {  // the entry, the checkpoint, the gate and the turns inside: high
            r[static_cast<std::size_t>(i)].agl_m = transit_agl_;
            r[static_cast<std::size_t>(i)].leg_mps = cruise_mps_;
        }
        r.insert(r.begin() + k, make(c, agl_, in_mps_, "letdown"));
        const int old_letdown = RouteIndexOf(r, "letdown");
        if (old_letdown >= 0 && old_letdown < gi) r.erase(r.begin() + old_letdown);
    }

    void Exposure(PlannedRoute& out, const RouteRequest& rq) const {
        const std::vector<RoutePoint>& r = out.points;
        const int ip = std::max(RouteIndexOf(r, "ip"), RouteIndexOf(r, "hide"));
        const int gate = RouteIndexOf(r, "gate");
        double exposure = 0.0, longest = 0.0;
        for (const RouteThreat& th : *threats_) {
            if (th.weight <= 0.0 || th.fighter) continue;  // the believed SAM and gun exposure only
            const bool own = Dist2d(rq.target, th.pos) < th.ring_m;
            double run_s = 0.0;
            for (std::size_t i = 0; i + 1 < r.size(); ++i) {
                const double mps = r[i].leg_mps > 0.0 ? r[i].leg_mps : 200.0;
                if (own && ip >= 0 && static_cast<int>(i) >= ip) break;
                double lg = 0.0;
                const double seen = g_.Seen(th.key, th.pos, th.ring_m, r[i].pos, r[i + 1].pos, r[i].agl_m, r[i + 1].agl_m, th.masked, &lg);
                exposure += th.weight * seen / mps;
                if (gate >= 0 && static_cast<int>(i) >= gate && (ip < 0 || static_cast<int>(i) < ip)) {
                    // A run carried over from the last leg when this one is seen from its start.
                    const double len = Dist2d(r[i].pos, r[i + 1].pos);
                    if (seen > 0.0 && lg >= len - 1.0) {
                        run_s += lg / mps;
                    } else {
                        run_s = lg / mps;
                    }
                    longest = std::max(longest, run_s);
                    if (seen <= 0.0) run_s = 0.0;
                }
            }
        }
        out.exposure_s = exposure;
        out.longest_s = longest;
    }
};

// The believed threats as the route planner prices them for a flight of this type and profile: those too slow for it
// left out, the ring widened by half the position's sigma and the lethality spread over it (as ExpectedLosses does),
// the reach at the flight's height; a Low flight on a map with ground is seen only where the ground lets the site see
// it. In key order.
inline std::vector<RouteThreat> BelievedRouteThreats(const std::vector<const IntelEntry*>& intel, const AircraftType& a, Profile prof,
                                                     bool ground, const TuningTable& tune) {
    std::vector<RouteThreat> out;
    for (const IntelEntry* th : intel) {
        if (th->threat_max_target_mps > 0.0 && a.cruise_mps > th->threat_max_target_mps) continue;
        const double ring = th->threat_radius_m + 0.5 * th->sigma_m;
        const double spread = (th->threat_radius_m / ring) * (th->threat_radius_m / ring);
        const Profile p = a.helicopter ? Profile::Low : prof;
        const bool long_range = th->threat_radius_m > 15000.0;
        double alt = 1.0;
        if (p == Profile::Low) {
            alt = th->threat_low ? (ground ? 1.0 : (long_range ? tune.Get("attrition.low_vs_long_range_sam") : 1.0)) : 0.0;
        } else if (p == Profile::Medium) {
            alt = th->threat_medium ? 1.0 : tune.Get("attrition.medium_vs_guns");
        } else {
            alt = (th->threat_medium && long_range) ? 1.0 : tune.Get("attrition.high_vs_short_range");
        }
        RouteThreat r;
        r.key = th->key;
        r.pos = th->pos;
        r.ring_m = ring;
        r.weight = th->threat_lethality * th->Live() * spread * alt;
        r.masked = ground && p == Profile::Low;
        out.push_back(r);
    }
    std::stable_sort(out.begin(), out.end(), [](const RouteThreat& x, const RouteThreat& y) { return x.key < y.key; });
    return out;
}

// Our fighters as the enemy believes them (roadmap S7, route.mode 2; 01_ROUTES.md "As built (S7)"): each air base of
// ours it believes there and working is a ring of route.fighter_reach_m (with half its position's sigma) our interceptors
// reach from under ground control, weighed by the front's fighter threat (friendly_fighters), route.fighter_weight and its
// belief in the base; for a Low flight or a helicopter by route.fighter_low_share (the period's radars saw little below
// them: 07_AIR_TO_AIR.md 5.2). Never masked (a fighter is not a site). Keys "fighter:<entry>", in key order.
inline std::vector<RouteThreat> BelievedFighterThreats(const std::vector<IntelEntry>& entries, double friendly_fighters, const AircraftType& a,
                                                       Profile prof, const TuningTable& tune) {
    std::vector<RouteThreat> out;
    const double reach = tune.Get("route.fighter_reach_m");
    const double w0 = tune.Get("route.fighter_weight") * friendly_fighters;
    if (reach <= 0.0 || w0 <= 0.0) return out;
    const bool low = a.helicopter || prof == Profile::Low;
    for (const IntelEntry& e : entries) {
        if (e.kind != ObjectKind::Airfield || e.ghost) continue;
        const double live = e.Live();
        if (live < 0.15) continue;
        RouteThreat r;
        r.key = "fighter:" + e.key;
        r.pos = e.pos;
        r.ring_m = reach + 0.5 * e.sigma_m;
        r.weight = w0 * live * (low ? tune.Get("route.fighter_low_share") : 1.0);
        r.fighter = true;
        out.push_back(r);
    }
    std::stable_sort(out.begin(), out.end(), [](const RouteThreat& x, const RouteThreat& y) { return x.key < y.key; });
    return out;
}

}  // namespace airwar
}  // namespace simcore
