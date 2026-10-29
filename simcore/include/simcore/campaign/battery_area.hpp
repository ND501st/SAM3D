// simcore/campaign/battery_area.hpp — the battery area and its places (roadmap G4; docs/campaign/11_BATTERY_LIFE_PROPOSAL.md
// §5.4).
//
// At a position, facing the threat's direction, the battery lays out by the textbook's rules (data/campaign/
// battery_area.json): the launchers in two pairs across the threat, each pair's two 200-500 m apart and the pairs up to
// 3 km apart; each TZM within 500 m of its pair; the PU-12M behind the pairs with its aiming circle 30-40 m ahead of it,
// a launcher's 20-50 m ahead; the maintenance vehicle and the camp behind the post; a reserve position within 2 km.
// Each place goes where its rule puts it when the ground there is open (and, for a launcher, open all round it within
// `clear_m`); else to the nearest open point that still keeps every rule, searched ring by ring, so the same task and
// ground give the same area every time. Every place is an anchor ("<area>/<place>"); the area's check lists every rule
// broken and every place a vehicle or a man on foot cannot reach from the post.
//
// The position work and the wire are jobs at places with their hours (G4.2): an emplacement dug, nets put up, a line
// laid from the post to each launcher. A job dropped keeps what was done: a half-dug emplacement is half dug.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
#include "simcore/nav/anchors.hpp"
#include "simcore/nav/ground.hpp"
#include "simcore/nav/routes.hpp"

namespace simcore {
namespace campaign {

using nav::Ground;
using nav::GroundGrid;

struct AreaRule {
    double value = 0.0;
    double min_v = 0.0;
    double max_v = 1e300;
    bool Keeps(double d) const { return d >= min_v - 1e-6 && d <= max_v + 1e-6; }
};

struct BatteryAreaRules {
    AreaRule pair_internal_m, pair_separation_m, tzm_from_pair_m, command_post_behind_m, aiming_circle_post_m, aiming_circle_launcher_m,
        reserve_within_m, maintenance_behind_m, camp_behind_m, clear_m;
    double dig_emplacement_h = 3.0;
    double nets_h = 0.5;
    double wire_lay_mps = 1.2;
};

namespace detail {

class BatteryAreaLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, BatteryAreaRules& out) {
        if (!Keys(root, "", {"area_id", "status", "notes", "source", "confidence", "rules", "jobs"}) || !Provenance(root, "")) return false;
        const json::Value* rules = root.Find("rules");
        if (rules == nullptr || !Keys(*rules, "rules", {"pair_internal_m", "pair_separation_m", "tzm_from_pair_m", "command_post_behind_m",
                                                         "aiming_circle_post_m", "aiming_circle_launcher_m", "reserve_within_m",
                                                         "maintenance_behind_m", "camp_behind_m", "clear_m"})) {
            return rules == nullptr ? Fail("rules", "required") : false;
        }
        const std::pair<const char*, AreaRule*> each[] = {
            {"pair_internal_m", &out.pair_internal_m},           {"pair_separation_m", &out.pair_separation_m},
            {"tzm_from_pair_m", &out.tzm_from_pair_m},           {"command_post_behind_m", &out.command_post_behind_m},
            {"aiming_circle_post_m", &out.aiming_circle_post_m}, {"aiming_circle_launcher_m", &out.aiming_circle_launcher_m},
            {"reserve_within_m", &out.reserve_within_m},         {"maintenance_behind_m", &out.maintenance_behind_m},
            {"camp_behind_m", &out.camp_behind_m},               {"clear_m", &out.clear_m}};
        for (const auto& e : each) {
            const std::string p = Join("rules", e.first);
            const json::Value* v = rules->Find(e.first);
            if (v == nullptr) return Fail(p, "required");
            if (!Keys(*v, p, {"value", "min", "max", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
            AreaRule& r = *e.second;
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number > 0.0)) return Fail(Join(p, "value"), "a number > 0");
            r.value = n->number;
            r.min_v = 0.0;
            r.max_v = 1e300;
            if (!OptionalNumber(*v, p, "min", r.min_v) || !OptionalNumber(*v, p, "max", r.max_v)) return false;
            if (!r.Keeps(r.value)) return Fail(Join(p, "value"), "outside its min and max");
        }
        const json::Value* jobs = root.Find("jobs");
        if (jobs == nullptr) return Fail("jobs", "required");
        if (!Keys(*jobs, "jobs", {"dig_emplacement_h", "nets_h", "wire_lay_mps"})) return false;
        const std::pair<const char*, double*> job_values[] = {
            {"dig_emplacement_h", &out.dig_emplacement_h}, {"nets_h", &out.nets_h}, {"wire_lay_mps", &out.wire_lay_mps}};
        for (const auto& e : job_values) {
            const std::string p = Join("jobs", e.first);
            const json::Value* v = jobs->Find(e.first);
            if (v == nullptr) return Fail(p, "required");
            if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number > 0.0)) return Fail(Join(p, "value"), "a number > 0");
            *e.second = n->number;
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadBatteryAreaRulesJson(const std::string& text, BatteryAreaRules& out, std::string* error = nullptr) {
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "battery area: " + e;
        return false;
    }
    detail::BatteryAreaLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "battery area: " + loader.Message();
        return false;
    }
    return true;
}
inline bool LoadBatteryAreaRules(const std::string& data_dir, const std::string& rel, BatteryAreaRules& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "battery area: cannot read " + rel;
        return false;
    }
    return LoadBatteryAreaRulesJson(text, out, error);
}

// ---------------------------------------------------------------------------------------------- the area

struct AreaPlace {
    std::string id;        // "launcher_1", "tzm_a", "command_post", "aiming_circle_post", "aiming_circle_1", ...
    std::string kind;      // "launcher", "tzm", "command_post", "aiming_circle", "maintenance", "camp", "reserve"
    std::string mobility;  // the class that drives there ("" : on foot only)
    MapPoint at;
    MapPoint ideal;        // where the rule put it on open ground
    Ground ground = Ground::Open;
};

struct WireRun {
    std::string from;
    std::string to;
    double length_m = 0.0;
};

struct BatteryArea {
    std::string id;
    MapPoint centre;
    double threat_rad = 0.0;  // the threat's direction, clockwise from north
    bool ok = false;
    std::string why;          // not ok: the first place with nowhere to go
    std::vector<AreaPlace> places;
    std::vector<WireRun> wires;

    const AreaPlace* Place(const std::string& id_) const {
        for (const AreaPlace& p : places) {
            if (p.id == id_) return &p;
        }
        return nullptr;
    }
    // Each place an anchor: "<area>/<place>".
    void AddAnchors(nav::Anchors& anchors) const {
        for (const AreaPlace& p : places) anchors.Add(id + "/" + p.id, "area_place", p.at, p.id);
    }
};

namespace detail {

inline double Dist(const MapPoint& a, const MapPoint& b) { return std::hypot(b.east_m - a.east_m, b.north_m - a.north_m); }

// Open ground at the point, and (clear_m > 0) all round it within that distance.
inline bool OpenAt(const GroundGrid* g, const MapPoint& p, double clear_m) {
    if (g == nullptr) return true;
    if (!g->Covers(p) || g->At(p) != Ground::Open) return false;
    if (clear_m <= 0.0) return true;
    for (int k = 0; k < 8; ++k) {
        const double a = 0.7853981633974483 * k;
        if (g->At({p.east_m + clear_m * std::sin(a), p.north_m + clear_m * std::cos(a)}) != Ground::Open) return false;
    }
    return true;
}

}  // namespace detail

// Lays out the battery area at `centre` facing `threat_rad` on the ground (none: all open).
inline BatteryArea LayOutBatteryArea(const std::string& id, const MapPoint& centre, double threat_rad, const BatteryAreaRules& r,
                                     const GroundGrid* ground) {
    BatteryArea a;
    a.id = id;
    a.centre = centre;
    a.threat_rad = threat_rad;
    const double fe = std::sin(threat_rad), fn = std::cos(threat_rad);  // forward, toward the threat
    const double re = fn, rn = -fe;                                        // right of it
    auto at = [&](const MapPoint& base, double fwd, double right) {
        return MapPoint{base.east_m + fe * fwd + re * right, base.north_m + fn * fwd + rn * right};
    };
    // A place's rule against the places already laid: a distance from another place within bounds.
    struct Bound {
        std::string other;
        const AreaRule* rule;
    };
    auto place = [&](const std::string& pid, const std::string& kind, const std::string& mobility, const MapPoint& ideal,
                     std::vector<Bound> bounds, double clear, double reach_m = 800.0) -> bool {
        auto fits = [&](const MapPoint& p) {
            if (!detail::OpenAt(ground, p, clear)) return false;
            for (const Bound& b : bounds) {
                const AreaPlace* o = a.Place(b.other);
                if (o != nullptr && !b.rule->Keeps(detail::Dist(p, o->at))) return false;
            }
            for (const AreaPlace& o : a.places) {
                if (detail::Dist(p, o.at) < 10.0) return false;  // no two places on one spot
            }
            return true;
        };
        const double step = ground != nullptr ? std::max(10.0, 0.5 * ground->Step()) : 10.0;
        // Out from the ideal spot, nearest first, as far as its rules allow it to be from the places it is bound to (2 km
        // at most).
        double reach = reach_m;
        for (const Bound& b : bounds) reach = std::max(reach, std::min(2000.0, b.rule->max_v));
        for (int ring = 0; ring * step <= reach; ++ring) {
            const int n = ring == 0 ? 1 : std::max(16, static_cast<int>(6.283185307179586 * ring * step / 100.0));
            for (int k = 0; k < n; ++k) {
                const double ang = 6.283185307179586 * k / n;
                const MapPoint p{ideal.east_m + ring * step * std::sin(ang), ideal.north_m + ring * step * std::cos(ang)};
                if (!fits(p)) continue;
                AreaPlace pl;
                pl.id = pid;
                pl.kind = kind;
                pl.mobility = mobility;
                pl.at = p;
                pl.ideal = ideal;
                pl.ground = ground != nullptr ? ground->At(p) : Ground::Open;
                a.places.push_back(pl);
                return true;
            }
        }
        a.why = pid + ": no open ground that keeps its rules within " + std::to_string(static_cast<int>(reach)) + " m";
        return false;
    };
    const double sep = r.pair_separation_m.value, in = r.pair_internal_m.value;
    const MapPoint pair_a = at(centre, 0.0, -0.5 * sep), pair_b = at(centre, 0.0, 0.5 * sep);
    const double clear = r.clear_m.value;
    bool ok = place("launcher_1", "launcher", "osa_9a33", at(pair_a, 0.0, -0.5 * in), {}, clear) &&
              place("launcher_2", "launcher", "osa_9a33", at(pair_a, 0.0, 0.5 * in), {{"launcher_1", &r.pair_internal_m}}, clear) &&
              place("launcher_3", "launcher", "osa_9a33", at(pair_b, 0.0, -0.5 * in), {{"launcher_2", &r.pair_separation_m}}, clear) &&
              place("launcher_4", "launcher", "osa_9a33", at(pair_b, 0.0, 0.5 * in), {{"launcher_3", &r.pair_internal_m}}, clear) &&
              place("tzm_a", "tzm", "tzm_9t217", at(pair_a, -r.tzm_from_pair_m.value, 0.0),
                    {{"launcher_1", &r.tzm_from_pair_m}, {"launcher_2", &r.tzm_from_pair_m}}, 0.0) &&
              place("tzm_b", "tzm", "tzm_9t217", at(pair_b, -r.tzm_from_pair_m.value, 0.0),
                    {{"launcher_3", &r.tzm_from_pair_m}, {"launcher_4", &r.tzm_from_pair_m}}, 0.0);
    if (ok) {
        // The post behind the pairs, at its distance from the area's centre (anywhere its rule allows: the search reaches
        // as far as the rule's span).
        const AreaRule& cp = r.command_post_behind_m;
        AreaPlace c;
        c.id = "centre";
        c.at = centre;
        a.places.push_back(c);
        ok = place("command_post", "command_post", "pu12m", at(centre, -cp.value, 0.0), {{"centre", &cp}}, 0.0);
        a.places.erase(std::remove_if(a.places.begin(), a.places.end(), [](const AreaPlace& p) { return p.id == "centre"; }), a.places.end());
    }
    if (ok) {
        const AreaPlace cp = *a.Place("command_post");
        ok = place("aiming_circle_post", "aiming_circle", "", at(cp.at, r.aiming_circle_post_m.value, 0.0), {{"command_post", &r.aiming_circle_post_m}}, 0.0);
        for (int k = 1; ok && k <= 4; ++k) {
            const std::string l = "launcher_" + std::to_string(k);
            ok = place("aiming_circle_" + std::to_string(k), "aiming_circle", "", at(a.Place(l)->at, r.aiming_circle_launcher_m.value, 0.0),
                       {{l, &r.aiming_circle_launcher_m}}, 0.0);
        }
        ok = ok && place("maintenance", "maintenance", "truck", at(cp.at, -r.maintenance_behind_m.value, 0.0), {{"command_post", &r.maintenance_behind_m}}, 0.0);
        ok = ok && place("camp", "camp", "truck", at(cp.at, -r.camp_behind_m.value, 0.25 * r.camp_behind_m.value), {{"command_post", &r.camp_behind_m}}, 0.0);
    }
    if (ok) {
        // The reserve position: off to a flank and back, within its distance of the centre.
        const double d = r.reserve_within_m.value;
        AreaPlace c;
        c.id = "centre";
        c.at = centre;
        a.places.push_back(c);
        ok = place("reserve", "reserve", "osa_9a33", at(centre, -0.6 * d, 0.8 * d), {{"centre", &r.reserve_within_m}}, clear);
        a.places.erase(std::remove_if(a.places.begin(), a.places.end(), [](const AreaPlace& p) { return p.id == "centre"; }), a.places.end());
    }
    if (ok) {
        // The wire: a line from the post to each launcher (the P-274's run along the ground, a tenth over the straight line).
        for (int k = 1; k <= 4; ++k) {
            const std::string l = "launcher_" + std::to_string(k);
            a.wires.push_back({"command_post", l, 1.1 * detail::Dist(a.Place("command_post")->at, a.Place(l)->at)});
        }
    }
    a.ok = ok;
    return a;
}

// What breaks the area's rules or cannot be reached from the post: "" for each that keeps them.
inline std::vector<std::string> CheckBatteryArea(const BatteryArea& a, const BatteryAreaRules& r, const GroundGrid* ground,
                                                 const nav::MobilityLibrary* lib) {
    std::vector<std::string> out;
    if (!a.ok) {
        out.push_back(a.why);
        return out;
    }
    auto d = [&](const std::string& x, const std::string& y) { return detail::Dist(a.Place(x)->at, a.Place(y)->at); };
    auto need = [&](bool keeps, const std::string& what) {
        if (!keeps) out.push_back(what);
    };
    need(r.pair_internal_m.Keeps(d("launcher_1", "launcher_2")) && r.pair_internal_m.Keeps(d("launcher_3", "launcher_4")), "a pair's launchers too near or too far");
    need(r.pair_separation_m.Keeps(d("launcher_2", "launcher_3")), "the pairs too near or too far apart");
    need(r.tzm_from_pair_m.Keeps(d("tzm_a", "launcher_1")) && r.tzm_from_pair_m.Keeps(d("tzm_b", "launcher_3")), "a TZM away from its pair");
    need(r.aiming_circle_post_m.Keeps(d("aiming_circle_post", "command_post")), "the post's aiming circle");
    for (int k = 1; k <= 4; ++k) {
        need(r.aiming_circle_launcher_m.Keeps(d("aiming_circle_" + std::to_string(k), "launcher_" + std::to_string(k))), "a launcher's aiming circle");
    }
    need(r.reserve_within_m.Keeps(detail::Dist(a.Place("reserve")->at, a.centre)), "the reserve position");
    for (const AreaPlace& p : a.places) {
        need(detail::OpenAt(ground, p.at, p.kind == "launcher" ? r.clear_m.value : 0.0), p.id + " not on open ground");
    }
    // From the post: every place on foot, and each vehicle's place by its class, across the ground.
    if (ground != nullptr && lib != nullptr) {
        const MapPoint post = a.Place("command_post")->at;
        const nav::Mobility* foot = lib->Find("foot");
        for (const AreaPlace& p : a.places) {
            if (foot != nullptr && !ground->Across(post, p.at, foot->GroundMps()).found) out.push_back(p.id + " out of reach on foot");
            const nav::Mobility* m = p.mobility.empty() ? nullptr : lib->Find(p.mobility);
            if (m != nullptr && !ground->Across(post, p.at, m->GroundMps()).found) out.push_back(p.id + " out of reach for its vehicle");
        }
    }
    return out;
}

// ---------------------------------------------------------------------------------------------- jobs at places (G4.2)

struct AreaJob {
    std::string id;     // "dig_launcher_1", "nets_tzm_a", "wire_launcher_2"
    std::string kind;   // "dig", "nets", "wire"
    std::string place;  // where it is worked
    double work_h = 0.0;
    double done_h = 0.0;

    bool Done() const { return done_h >= work_h - 1e-9; }
    double Left() const { return std::max(0.0, work_h - done_h); }
};

class JobBoard {
public:
    // The area's position work: an emplacement for each launcher, nets over each vehicle, a line to each launcher.
    void ForArea(const BatteryArea& a, const BatteryAreaRules& r) {
        jobs_.clear();
        for (const AreaPlace& p : a.places) {
            if (p.kind == "launcher") jobs_.push_back({"dig_" + p.id, "dig", p.id, r.dig_emplacement_h, 0.0});
            if (p.kind == "launcher" || p.kind == "tzm" || p.kind == "command_post") jobs_.push_back({"nets_" + p.id, "nets", p.id, r.nets_h, 0.0});
        }
        for (const WireRun& w : a.wires) jobs_.push_back({"wire_" + w.to, "wire", w.from, w.length_m / r.wire_lay_mps / 3600.0, 0.0});
    }
    // Work on a job for `hours` (a crew at it); it keeps what was done when dropped. Returns the hours used.
    double Work(const std::string& id, double hours) {
        AreaJob* j = Find(id);
        if (j == nullptr || hours <= 0.0) return 0.0;
        const double used = std::min(hours, j->Left());
        j->done_h += used;
        return used;
    }
    AreaJob* Find(const std::string& id) {
        for (AreaJob& j : jobs_) {
            if (j.id == id) return &j;
        }
        return nullptr;
    }
    const std::vector<AreaJob>& Jobs() const { return jobs_; }

private:
    std::vector<AreaJob> jobs_;
};

}  // namespace campaign
}  // namespace simcore
