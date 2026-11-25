// simcore/command/held_map.hpp — the maps people hold (track O, O2; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md
// sections 3 (D2, D3) and 5).
//
// A map is a paper a person holds, and it shows only what reached him (D2). A HeldMap belongs to a holder (a position of
// the chain, or a vehicle's commander); each HeldMark on it records what it is (a kind, ours red or the enemy blue), its
// shape in the session's frame (points, or a point with an arc for a sector or a direction), a short label in the map's
// hand, who told the holder and by what means, as of when, how sure (reported, confirmed, estimated), and whether it was
// copied from what he was told or is his own decision. Marks come from three places:
//   * the holder's situation map (F5's SituationMap, already built only from facts the post holds): each mark's source and
//     means from its fact's Delivery to him (command/held_situation.hpp, which needs the pipeline);
//   * the regiment's task as it reached him (AddTaskMarks: the order's means and hour);
//   * his own decisions (BatteryDecision, AddDecisionMarks).
// Nothing here reads a truth: a caller hands in what the holder was told.
//
// Decisions (D3): the battery commander decides on his map the main, reserve and dummy positions and each launcher's sector.
// CheckDecision lists what breaks the order (a position beyond decision_leeway_m of the order's point, a launcher without a
// sector, the battery's sector not covered) and refuses nothing (D4: limits are the order's rules, not walls). The runner's
// SetBatteryDecision makes the combat order use the decision; each launcher commander gets his sector card (SectorCard).
//
// Contours (ContourSegments) are derived from a height grid by marching squares, so the sheet's brown lines are the
// ground's own and testable here.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "simcore/campaign/battery_task.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace command {

// ------------------------------------------------------------------------------------------------------------- values

// data/gameplay/held_map.json: game values.
struct HeldMapValues {
    double decision_leeway_m = 1000.0;  // D3: a decided position within this of the order's point
    double coverage_step_deg = 1.0;     // the sectors checked against the battery's at this step
    double line_range_m = 20000.0;      // a direction drawn this far when the order gives no range
    double sector_drawn_m = 10000.0;    // a launcher's sector drawn to this radius
    double false_easting_km = 50.0;     // the sheet's grid numbers: km from the origin plus these
    double false_northing_km = 50.0;
};

namespace detail {

class HeldMapValuesLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, HeldMapValues& out) {
        if (!Keys(root, "", {"held_map_id", "status", "notes", "source", "confidence", "values"}) || !Provenance(root, "")) return false;
        const json::Value* values = root.Find("values");
        if (values == nullptr) return Fail("values", "required");
        const std::pair<const char*, double*> each[] = {{"decision_leeway_m", &out.decision_leeway_m}, {"coverage_step_deg", &out.coverage_step_deg},
                                                        {"line_range_m", &out.line_range_m},           {"sector_drawn_m", &out.sector_drawn_m},
                                                        {"false_easting_km", &out.false_easting_km},   {"false_northing_km", &out.false_northing_km}};
        if (!Keys(*values, "values", {"decision_leeway_m", "coverage_step_deg", "line_range_m", "sector_drawn_m", "false_easting_km", "false_northing_km"})) {
            return false;
        }
        for (const auto& e : each) {
            const std::string p = Join("values", e.first);
            const json::Value* v = values->Find(e.first);
            if (v == nullptr) return Fail(p, "required");
            if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number >= 0.0)) return Fail(Join(p, "value"), "a number >= 0");
            *e.second = n->number;
        }
        if (!(out.coverage_step_deg > 0.0)) return Fail("values.coverage_step_deg.value", "a number > 0");
        return true;
    }
};

}  // namespace detail

inline bool LoadHeldMapValues(const std::string& data_dir, const std::string& rel, HeldMapValues& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "held map: cannot read " + rel;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "held map: " + e;
        return false;
    }
    detail::HeldMapValuesLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "held map: " + loader.Message();
        return false;
    }
    return true;
}

// ------------------------------------------------------------------------------------------------------------- the marks

enum class MarkSide { Ours, Enemy };
enum class MarkCertainty { Reported, Confirmed, Estimated };
enum class MarkOrigin { Copied, Decision };

inline const char* MarkCertaintyName(MarkCertainty c) {
    switch (c) {
        case MarkCertainty::Reported: return "reported";
        case MarkCertainty::Confirmed: return "confirmed";
        case MarkCertainty::Estimated: return "estimated";
    }
    return "?";
}

struct HeldMark {
    // "covered", "main", "reserve", "dummy", "tzm", "attack", "helicopter", "jammer", "arm", "corridor", "duty_zone",
    // "sector", "forward_edge", "axis", "sam_unit", "note" (no shape: the margin's list)
    std::string kind;
    std::string id;
    MarkSide side = MarkSide::Ours;
    std::vector<Vec3> points;    // the session's frame (empty: a note, written in the margin)
    bool has_arc = false;        // a sector or a direction: from points[0], clockwise from..to (true bearings)
    double from_deg = 0.0;
    double to_deg = 0.0;
    double radius_m = 0.0;
    double half_width_m = 0.0;   // a corridor's or zone's
    std::string label;           // in the map's hand
    std::string source;          // who told the holder
    std::string means;           // by what (a MeanKindName, "own decision", "seen", "the plan")
    double as_of_h = -1.0;       // the holder's clock hours (the session's: its second 0 at 0 h; -1 the plan from before)
    std::string as_of;           // the same as the clock reads ("19:42", "D2 06:00")
    MarkCertainty certainty = MarkCertainty::Reported;
    MarkOrigin origin = MarkOrigin::Copied;
    std::string fact;            // the pipeline's fact it rests on ("" none)
    bool stale = false;
};

struct HeldMap {
    std::string holder;          // the position's id ("bty21_commander") or a vehicle's commander ("21:commander")
    std::string holder_label;
    double now_h = 0.0;
    std::string now;
    double leeway_m = 1000.0;    // D3's, drawn round the order's points
    bool has_centre = false;     // where the sheet opens
    Vec3 centre;
    std::vector<HeldMark> marks;

    const HeldMark* Find(const std::string& kind, const std::string& id = std::string()) const {
        for (const HeldMark& m : marks) {
            if (m.kind == kind && (id.empty() || m.id == id)) return &m;
        }
        return nullptr;
    }
    const HeldMark* FindDecision(const std::string& kind, const std::string& id = std::string()) const {
        for (const HeldMark& m : marks) {
            if (m.kind == kind && m.origin == MarkOrigin::Decision && (id.empty() || m.id == id)) return &m;
        }
        return nullptr;
    }
    int Count(const std::string& kind) const {
        int n = 0;
        for (const HeldMark& m : marks) n += m.kind == kind ? 1 : 0;
        return n;
    }
};

// The sheet's 1 km grid square of a point ("5943": easting 59, northing 43, the last two figures as printed).
inline int GridNumber(double metres, double false_km) {
    const int km = static_cast<int>(std::floor(metres / 1000.0 + false_km));
    return ((km % 100) + 100) % 100;
}
inline std::string GridSquare(const Vec3& p, const HeldMapValues& v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d%02d", GridNumber(p.x, v.false_easting_km), GridNumber(p.y, v.false_northing_km));
    return buf;
}

// ------------------------------------------------------------------------------------------------------------- bearings

namespace held_detail {

inline double Norm360(double d) {
    while (d < 0.0) d += 360.0;
    while (d >= 360.0) d -= 360.0;
    return d;
}
// Is `deg` within the span clockwise from..to (a full circle when from == to is not a span: false)?
inline bool InSpan(double from_deg, double to_deg, double deg) {
    double span = Norm360(to_deg - from_deg);
    if (span == 0.0 && to_deg != from_deg) span = 360.0;
    return Norm360(deg - from_deg) <= span + 1e-9;
}
inline double BearingDeg(const Vec3& from, const Vec3& to) { return Norm360(RadToDeg(AzimuthOf(to - from))); }
inline std::string Deg(double d) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%03.0f", Norm360(d));
    return buf;
}
inline std::string Km(double m) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%.1f km", m / 1000.0);
    return buf;
}

}  // namespace held_detail

// ------------------------------------------------------------------------------------------------------------- decisions

// A launcher's sector as the battery commander decided it.
struct DecidedSector {
    std::string vehicle;
    double from_deg = 0.0;
    double to_deg = 0.0;
    bool duty = false;
};

// The battery commander's decision on his map (D3). A part left empty keeps the order's.
struct BatteryDecision {
    bool has_main = false;
    Vec3 main;
    std::vector<Vec3> reserve;
    std::vector<Vec3> dummies;
    std::vector<DecidedSector> sectors;
    bool Empty() const { return !has_main && reserve.empty() && dummies.empty() && sectors.empty(); }
    const DecidedSector* SectorOf(const std::string& vehicle) const {
        for (const DecidedSector& s : sectors) {
            if (s.vehicle == vehicle) return &s;
        }
        return nullptr;
    }
};

// What a decision breaks of the order: listed, never refused (D4).
struct DecisionBreach {
    std::string kind;    // "main_off", "reserve_off", "dummy_off", "sector_missing", "sector_unknown", "sector_gap"
    std::string what;    // the position's or launcher's id
    double off_m = 0.0;  // a position: how far from the order's point
    double from_deg = 0.0, to_deg = 0.0;  // a gap in the battery's sector
    std::string text;
};

using PlaceFn = std::function<bool(const std::string&, Vec3&)>;

// The breaches of `d` against `task`: each decided position against the nearest point of its kind the order gives (the
// main against the order's main; a reserve or dummy against the order's reserves or dummies, unchecked when it gives
// none), within the leeway; each launcher the order gives a sector a decided one, no sector for a vehicle the order does
// not name, and the battery's sector (the union of the order's) covered by the decided ones at the values' step.
inline std::vector<DecisionBreach> CheckDecision(const campaign::BatteryTask& task, const BatteryDecision& d, const PlaceFn& where,
                                                 const HeldMapValues& v) {
    using namespace held_detail;
    std::vector<DecisionBreach> out;
    const auto near_check = [&](const char* kind, const char* what_name, const std::string& what, const Vec3& p, const std::vector<std::string>& given) {
        double best = std::numeric_limits<double>::infinity();
        for (const std::string& id : given) {
            Vec3 q;
            if (where && where(id, q)) best = std::min(best, std::hypot(p.x - q.x, p.y - q.y));
        }
        if (!std::isfinite(best) || best <= v.decision_leeway_m) return;
        DecisionBreach b;
        b.kind = kind;
        b.what = what;
        b.off_m = best;
        b.text = std::string(what_name) + " " + Km(best) + " from the order's point (the leeway " + Km(v.decision_leeway_m) + ")";
        out.push_back(b);
    };
    if (d.has_main) near_check("main_off", "the main position", "main", d.main, {task.main});
    for (std::size_t i = 0; i < d.reserve.size(); ++i) near_check("reserve_off", "a reserve position", "reserve:" + std::to_string(i), d.reserve[i], task.reserve);
    for (std::size_t i = 0; i < d.dummies.size(); ++i) near_check("dummy_off", "a dummy position", "dummy:" + std::to_string(i), d.dummies[i], task.dummies);
    if (d.sectors.empty()) return out;
    for (const campaign::TaskSector& s : task.sectors) {
        if (d.SectorOf(s.vehicle) != nullptr) continue;
        DecisionBreach b;
        b.kind = "sector_missing";
        b.what = s.vehicle;
        b.text = "launcher " + s.vehicle + " has no sector";
        out.push_back(b);
    }
    for (const DecidedSector& s : d.sectors) {
        bool named = false;
        for (const campaign::TaskSector& t : task.sectors) named = named || t.vehicle == s.vehicle;
        if (named) continue;
        DecisionBreach b;
        b.kind = "sector_unknown";
        b.what = s.vehicle;
        b.text = "a sector for " + s.vehicle + ", not a launcher the order names";
        out.push_back(b);
    }
    // The battery's sector, bearing by bearing: a run of bearings in the order's union and in no decided sector is a gap.
    const int steps = static_cast<int>(std::ceil(360.0 / v.coverage_step_deg));
    bool in_gap = false;
    double gap_from = 0.0, last = 0.0;
    const auto close_gap = [&]() {
        if (!in_gap) return;
        in_gap = false;
        DecisionBreach b;
        b.kind = "sector_gap";
        b.from_deg = gap_from;
        b.to_deg = last;
        b.text = "the battery's sector uncovered from " + Deg(gap_from) + " to " + Deg(last);
        out.push_back(b);
    };
    // Start the sweep at a bearing outside the order's union (if any) so that no gap is split at 0.
    double start = 0.0;
    for (int i = 0; i < steps; ++i) {
        const double deg = i * v.coverage_step_deg;
        bool any = false;
        for (const campaign::TaskSector& t : task.sectors) any = any || InSpan(t.from_deg, t.to_deg, deg);
        if (!any) {
            start = deg;
            break;
        }
    }
    for (int i = 0; i <= steps; ++i) {
        const double deg = Norm360(start + i * v.coverage_step_deg);
        bool wanted = false;
        for (const campaign::TaskSector& t : task.sectors) wanted = wanted || InSpan(t.from_deg, t.to_deg, deg);
        bool covered = false;
        for (const DecidedSector& s : d.sectors) covered = covered || InSpan(s.from_deg, s.to_deg, deg);
        if (wanted && !covered && i < steps) {
            if (!in_gap) gap_from = deg;
            in_gap = true;
            last = deg;
        } else {
            close_gap();
        }
    }
    close_gap();
    return out;
}

// ------------------------------------------------------------------------------------------------------------- the sector card

// A launcher commander's paper with the combat order (O2): his point, his sector and the landmarks' bearings from it, his
// neighbours' sectors, the reserve position and the code change times.
struct SectorCard {
    std::string vehicle;
    std::string point_name;
    Vec3 point;
    double from_deg = 0.0;
    double to_deg = 0.0;
    bool duty = false;
    struct Landmark {
        std::string name;
        double bearing_deg = 0.0;
        double range_m = 0.0;
    };
    std::vector<Landmark> landmarks;
    std::vector<DecidedSector> neighbours;
    bool has_reserve = false;
    std::string reserve_name;
    Vec3 reserve;
    std::vector<std::string> code_changes;  // the clock times the identification code changes
    double given_s = 0.0;
    std::string given;                      // the clock
};

inline std::string SectorCardText(const SectorCard& c, const HeldMapValues& v) {
    using namespace held_detail;
    std::string s = "Launcher " + c.vehicle + (c.duty ? " (duty launcher)" : "") + ", with the combat order of " + c.given + ".\n";
    s += "Your point: " + c.point_name + ", square " + GridSquare(c.point, v) + "; your spot within " + Km(v.decision_leeway_m) + " of it.\n";
    s += "Your sector: " + Deg(c.from_deg) + " to " + Deg(c.to_deg) + ".\n";
    if (!c.landmarks.empty()) {
        s += "Landmarks from the point:";
        for (const SectorCard::Landmark& l : c.landmarks) s += "\n  " + l.name + ": " + Deg(l.bearing_deg) + ", " + Km(l.range_m);
        s += "\n";
    }
    if (!c.neighbours.empty()) {
        s += "Neighbours:";
        for (const DecidedSector& n : c.neighbours) s += "\n  " + n.vehicle + ": " + Deg(n.from_deg) + " to " + Deg(n.to_deg) + (n.duty ? " (duty)" : "");
        s += "\n";
    }
    if (c.has_reserve) s += "Reserve position: " + c.reserve_name + ", square " + GridSquare(c.reserve, v) + ".\n";
    if (!c.code_changes.empty()) {
        s += "Code changes:";
        for (const std::string& t : c.code_changes) s += " " + t;
        s += " (the codes by the day's table).\n";
    }
    return s;
}

// ------------------------------------------------------------------------------------------------------------- building

// Where a set of marks came from, as the holder was told.
struct MarkSource {
    std::string source;
    std::string means;
    double as_of_h = -1.0;
    std::string as_of;
    MarkCertainty certainty = MarkCertainty::Confirmed;
    MarkOrigin origin = MarkOrigin::Copied;
};

namespace held_detail {

inline HeldMark Stamped(const MarkSource& src, const std::string& kind, const std::string& id, MarkSide side) {
    HeldMark m;
    m.kind = kind;
    m.id = id;
    m.side = side;
    m.source = src.source;
    m.means = src.means;
    m.as_of_h = src.as_of_h;
    m.as_of = src.as_of;
    m.certainty = src.certainty;
    m.origin = src.origin;
    return m;
}
inline std::string HourText(double h) {  // a zone's local clock hours
    char buf[16];
    const int hh = static_cast<int>(std::floor(h));
    std::snprintf(buf, sizeof(buf), "%02d:%02d", ((hh % 24) + 24) % 24, static_cast<int>(std::round((h - hh) * 60.0)) % 60);
    return buf;
}

}  // namespace held_detail

// The regiment's task on the holder's map as it reached him: the covered object, the order's positions, the situation's
// lines (the regiment's estimate) from the covered object, our aviation's corridors and zones, and the launchers' sectors
// the order gives (left off when `with_sectors` is false: a decision's replace them). `name` gives a place's name.
inline void AddTaskMarks(HeldMap& out, const campaign::BatteryTask& t, const PlaceFn& where, const MarkSource& src, const HeldMapValues& v,
                         const std::function<std::string(const std::string&)>& name, bool with_sectors = true) {
    using namespace held_detail;
    const auto place_name = [&](const std::string& id) { return name ? name(id) : id; };
    Vec3 covered, main;
    const bool has_covered = !t.covered.empty() && where && where(t.covered, covered);
    const bool has_main = !t.main.empty() && where && where(t.main, main);
    const auto place = [&](const std::string& kind, const std::string& id, const std::string& label) {
        Vec3 p;
        if (id.empty() || !where || !where(id, p)) return;
        HeldMark m = Stamped(src, kind, id, MarkSide::Ours);
        m.points = {p};
        m.label = label;
        out.marks.push_back(m);
    };
    place("covered", t.covered, place_name(t.covered));
    place("main", t.main, "main: " + place_name(t.main));
    for (const std::string& r : t.reserve) place("reserve", r, "reserve");
    for (const std::string& d : t.dummies) place("dummy", d, "dummy");
    place("tzm", t.tzm_place, "TZM");
    for (std::size_t i = 0; i < t.situation.size(); ++i) {
        const campaign::TaskLine& l = t.situation[i];
        if (!has_covered && !has_main) continue;
        MarkSource est = src;
        est.certainty = MarkCertainty::Estimated;  // the regiment's estimate of the enemy
        HeldMark m = Stamped(est, l.kind, l.kind + ":" + std::to_string(i), MarkSide::Enemy);
        m.points = {has_covered ? covered : main};
        m.has_arc = true;
        m.from_deg = l.from_deg;
        m.to_deg = l.to_deg;
        m.radius_m = l.range_km > 0.0 ? l.range_km * 1000.0 : v.line_range_m;
        m.label = l.text.empty() ? l.kind : l.text;
        out.marks.push_back(m);
    }
    for (const OwnAirZone& z : t.own_air.zones) {
        HeldMark m = Stamped(src, z.kind, z.id, MarkSide::Ours);
        m.points = z.path;
        for (Vec3& p : m.points) p.z = 0.0;
        m.half_width_m = z.half_width_m;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s %s, %.0f-%.0f m", z.kind == "corridor" ? "corridor" : "zone", z.id.c_str(), z.floor_m, z.ceiling_m);
        m.label = buf;
        if (z.from_h > -1e17 && z.to_h < 1e17) m.label += ", " + HourText(z.from_h) + "-" + HourText(z.to_h);
        out.marks.push_back(m);
    }
    if (with_sectors && has_main) {
        for (const campaign::TaskSector& s : t.sectors) {
            HeldMark m = Stamped(src, "sector", s.vehicle, MarkSide::Ours);
            m.points = {main};
            m.has_arc = true;
            m.from_deg = s.from_deg;
            m.to_deg = s.to_deg;
            m.radius_m = v.sector_drawn_m;
            m.label = s.vehicle + ": " + Deg(s.from_deg) + "-" + Deg(s.to_deg) + (s.duty ? " duty" : "");
            out.marks.push_back(m);
        }
    }
    if (!out.has_centre && (has_main || has_covered)) {
        out.has_centre = true;
        out.centre = has_main ? main : covered;
    }
}

// The battery commander's own decisions on his map: the positions he chose and each launcher's sector from the main
// position (his decided one, else the order's `order_main`).
inline void AddDecisionMarks(HeldMap& out, const BatteryDecision& d, const Vec3* order_main, const MarkSource& src, const HeldMapValues& v) {
    using namespace held_detail;
    MarkSource own = src;
    own.origin = MarkOrigin::Decision;
    own.certainty = MarkCertainty::Confirmed;
    const auto add = [&](const std::string& kind, const std::string& id, const Vec3& p, const std::string& label) {
        HeldMark m = Stamped(own, kind, id, MarkSide::Ours);
        m.points = {Vec3{p.x, p.y, 0.0}};
        m.label = label;
        out.marks.push_back(m);
    };
    if (d.has_main) add("main", "decided", d.main, "main (decided)");
    for (std::size_t i = 0; i < d.reserve.size(); ++i) add("reserve", "decided:" + std::to_string(i), d.reserve[i], "reserve");
    for (std::size_t i = 0; i < d.dummies.size(); ++i) add("dummy", "decided:" + std::to_string(i), d.dummies[i], "dummy");
    const Vec3* centre = d.has_main ? &d.main : order_main;
    if (centre == nullptr) return;
    for (const DecidedSector& s : d.sectors) {
        HeldMark m = Stamped(own, "sector", s.vehicle, MarkSide::Ours);
        m.points = {Vec3{centre->x, centre->y, 0.0}};
        m.has_arc = true;
        m.from_deg = s.from_deg;
        m.to_deg = s.to_deg;
        m.radius_m = v.sector_drawn_m;
        m.label = s.vehicle + ": " + Deg(s.from_deg) + "-" + Deg(s.to_deg) + (s.duty ? " duty" : "");
        out.marks.push_back(m);
    }
    if (d.has_main) {
        out.has_centre = true;
        out.centre = d.main;
    }
}

// A launcher commander's map from his sector card: his point, his sector and his neighbours', the reserve.
inline void AddCardMarks(HeldMap& out, const SectorCard& c, const MarkSource& src, const HeldMapValues& v) {
    using namespace held_detail;
    HeldMark p = Stamped(src, "main", "point", MarkSide::Ours);
    p.points = {c.point};
    p.label = "point: " + c.point_name;
    out.marks.push_back(p);
    const auto sector = [&](const std::string& vehicle, double from, double to, bool duty) {
        HeldMark m = Stamped(src, "sector", vehicle, MarkSide::Ours);
        m.points = {c.point};
        m.has_arc = true;
        m.from_deg = from;
        m.to_deg = to;
        m.radius_m = v.sector_drawn_m;
        m.label = vehicle + ": " + Deg(from) + "-" + Deg(to) + (duty ? " duty" : "");
        out.marks.push_back(m);
    };
    sector(c.vehicle, c.from_deg, c.to_deg, c.duty);
    for (const DecidedSector& n : c.neighbours) sector(n.vehicle, n.from_deg, n.to_deg, n.duty);
    if (c.has_reserve) {
        HeldMark r = Stamped(src, "reserve", "card", MarkSide::Ours);
        r.points = {c.reserve};
        r.label = "reserve";
        out.marks.push_back(r);
    }
    if (!out.has_centre) {
        out.has_centre = true;
        out.centre = c.point;
    }
}

// ------------------------------------------------------------------------------------------------------------- contours

struct ContourSegment {
    double level_m = 0.0;
    bool index = false;  // every fifth level: drawn heavier and numbered
    Vec3 a, b;
};

// The contours of a height grid (anything with west_m, south_m, spacing_m, cols, rows and At(c, r), as terrain_map.hpp's
// MapGrid) every `interval_m`, by marching squares over the cells inside west..east, south..north, every `stride`-th node
// (a coarser sheet). A cell's saddle is resolved by its centre's mean. Index contours every fifth level.
template <class Grid>
std::vector<ContourSegment> ContourSegments(const Grid& g, double interval_m, double west, double south, double east, double north, int stride = 1) {
    std::vector<ContourSegment> out;
    if (!(interval_m > 0.0) || g.cols < 2 || g.rows < 2 || stride < 1) return out;
    const double sp = g.spacing_m;
    const int c0 = std::max(0, static_cast<int>(std::floor((west - g.west_m) / sp)));
    const int r0 = std::max(0, static_cast<int>(std::floor((south - g.south_m) / sp)));
    const int c1 = std::min(g.cols - 1, static_cast<int>(std::ceil((east - g.west_m) / sp)));
    const int r1 = std::min(g.rows - 1, static_cast<int>(std::ceil((north - g.south_m) / sp)));
    for (int r = r0; r + stride <= r1; r += stride) {
        for (int c = c0; c + stride <= c1; c += stride) {
            // Corners anticlockwise from the south-west: 0 (c, r), 1 (c+s, r), 2 (c+s, r+s), 3 (c, r+s).
            const double h[4] = {g.At(c, r), g.At(c + stride, r), g.At(c + stride, r + stride), g.At(c, r + stride)};
            const double x[4] = {g.west_m + c * sp, g.west_m + (c + stride) * sp, g.west_m + (c + stride) * sp, g.west_m + c * sp};
            const double y[4] = {g.south_m + r * sp, g.south_m + r * sp, g.south_m + (r + stride) * sp, g.south_m + (r + stride) * sp};
            const double lo = std::min(std::min(h[0], h[1]), std::min(h[2], h[3]));
            const double hi = std::max(std::max(h[0], h[1]), std::max(h[2], h[3]));
            for (double level = std::ceil(lo / interval_m) * interval_m; level <= hi; level += interval_m) {
                // A level exactly on a corner's height is nudged up, so that no edge is crossed twice.
                const double L = level + 1e-6;
                int mask = 0;
                for (int k = 0; k < 4; ++k) mask |= (h[k] > L ? 1 : 0) << k;
                if (mask == 0 || mask == 15) continue;
                const auto edge = [&](int e) {  // edge e from corner e to corner (e+1)%4
                    const int a = e, b = (e + 1) % 4;
                    const double t = (L - h[a]) / (h[b] - h[a]);
                    return Vec3{x[a] + (x[b] - x[a]) * t, y[a] + (y[b] - y[a]) * t, level};
                };
                const auto seg = [&](int e1, int e2) {
                    ContourSegment s;
                    s.level_m = level;
                    const long long n = static_cast<long long>(std::llround(level / interval_m));
                    s.index = n % 5 == 0;
                    s.a = edge(e1);
                    s.b = edge(e2);
                    out.push_back(s);
                };
                // Each edge is crossed where its two corners lie on either side of the level.
                int crossed[4];
                int n = 0;
                for (int e = 0; e < 4; ++e) {
                    if (((mask >> e) & 1) != ((mask >> ((e + 1) % 4)) & 1)) crossed[n++] = e;
                }
                if (n == 2) {
                    seg(crossed[0], crossed[1]);
                } else if (n == 4) {  // a saddle: the centre's side decides which corners join
                    const bool centre_up = (h[0] + h[1] + h[2] + h[3]) * 0.25 > L;
                    const bool c0_up = (mask & 1) != 0;
                    if (centre_up == c0_up) {  // corner 0's side joins across the middle: cut off corners 1 and 3
                        seg(0, 1);
                        seg(2, 3);
                    } else {                   // cut off corners 0 and 2
                        seg(3, 0);
                        seg(1, 2);
                    }
                }
            }
        }
    }
    return out;
}

}  // namespace command
}  // namespace simcore
