// simcore/command/held_situation.hpp — a holder's situation map on his held map (track O, O2;
// docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md section 5).
//
// F5's SituationMap is built only from the facts a post holds; this copies it onto the post holder's HeldMap, each mark
// with who told him, by what means and when from its fact's Delivery to him (InfoNet::Known). A mark whose fact he does
// not hold is not copied (the situation map should have none; this is the rule kept a second time where the map is drawn).
// The plan's marks (a post's pre-war plan of our SAM units) are copied as the plan. Shapes go from the theatre's frame to
// the session's by `to_session`; hours from the campaign's to the holder's clock by `to_map_h` and `clock`.
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

#include "simcore/command/held_map.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/command/situation_map.hpp"

namespace simcore {
namespace command {

struct SituationFrame {
    std::function<Vec3(const Vec3&)> to_session;     // a theatre point in the session's frame
    std::function<double(double)> to_map_h;          // a campaign hour on the holder's clock (HeldMark::as_of_h)
    std::function<std::string(double)> clock;        // a campaign hour as the clock reads
};

namespace held_detail {

// Who told the holder and by what, from his delivery of the fact (nullptr: he does not hold it).
inline bool StampFromFact(const InfoNet& net, int holder, const SituationMark& sm, const SituationFrame& f, HeldMark& m) {
    if (sm.key.empty()) {  // the plan held from before the war
        m.source = "the plan";
        m.means = "the plan";
        m.as_of_h = -1.0;
        m.as_of = "the plan";
        m.certainty = MarkCertainty::Estimated;
        return true;
    }
    const int fact = net.FactIndex(sm.key);
    const Delivery* d = net.Known(holder, fact);
    if (d == nullptr) return false;
    const ChainData& data = net.Data();
    if (d->from >= 0 && static_cast<std::size_t>(d->from) < data.positions.size()) m.source = data.positions[static_cast<std::size_t>(d->from)].Label();
    else m.source = d->from == -1 ? "seen yourself" : "the state radio";
    if (d->mean >= 0 && static_cast<std::size_t>(d->mean) < data.means.size()) m.means = MeanKindName(data.means[static_cast<std::size_t>(d->mean)].kind);
    else m.means = d->from == -1 ? "seen" : "broadcast";
    m.fact = sm.key;
    m.as_of_h = f.to_map_h ? f.to_map_h(sm.h) : sm.h;
    m.as_of = f.clock ? f.clock(sm.h) : std::string();
    m.stale = sm.stale;
    m.certainty = d->from == -1 ? MarkCertainty::Confirmed : MarkCertainty::Reported;
    return true;
}

}  // namespace held_detail

// The holder's situation map onto `out`: the line as believed (our forward edge, red), each axis's report at its place on
// it (the enemy's, blue), our SAM units' places, and the rest (our units' returns, the enemy's formations, the air front's
// conclusions, the orders) as notes in the margin.
inline void AddSituationMarks(HeldMap& out, const SituationMap& map, const InfoNet& net, int holder, const SituationFrame& f,
                              const SituationLayout* layout = nullptr) {
    using held_detail::StampFromFact;
    const auto at = [&](double x, double y) {
        const Vec3 p{x, y, 0.0};
        return f.to_session ? f.to_session(p) : p;
    };
    // The line: stamped by the newest axis report the holder has.
    const MapAxis* newest = nullptr;
    for (const MapAxis& a : map.axes) {
        HeldMark probe;
        if (!StampFromFact(net, holder, a.mark, f, probe)) continue;
        if (newest == nullptr || a.mark.h > newest->mark.h) newest = &a;
    }
    if (newest != nullptr && map.line.size() >= 2) {
        HeldMark m;
        m.kind = "forward_edge";
        m.id = "line";
        m.side = MarkSide::Ours;
        StampFromFact(net, holder, newest->mark, f, m);
        for (const MapLinePoint& p : map.line) m.points.push_back(at(p.x, p.y));
        m.label = "forward edge";
        out.marks.push_back(m);
    }
    for (const MapAxis& a : map.axes) {
        HeldMark m;
        m.kind = "axis";
        m.id = a.name;
        m.side = MarkSide::Enemy;
        if (!StampFromFact(net, holder, a.mark, f, m)) continue;
        if (a.by_summary) m.certainty = MarkCertainty::Estimated;
        // Its place: the line's point nearest the axis's place on the base line.
        if (layout != nullptr && a.index >= 0 && static_cast<std::size_t>(a.index) < layout->axes.size() && !map.line.empty()) {
            const double along = layout->axes[static_cast<std::size_t>(a.index)].along_m;
            const MapLinePoint* best = &map.line.front();
            for (const MapLinePoint& p : map.line) {
                if (std::fabs(p.along_m - along) < std::fabs(best->along_m - along)) best = &p;
            }
            m.points = {at(best->x, best->y)};
        }
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%s: %.0f km in, %.1f km/day%s", a.name.c_str(), a.depth_m / 1000.0, a.pace_km_day, a.counterattack ? ", our counterattack" : "");
        m.label = buf;
        if (m.points.empty()) m.kind = "note";
        out.marks.push_back(m);
    }
    for (const MapSam& s : map.sams) {
        HeldMark m;
        m.kind = "sam_unit";
        m.id = s.id;
        m.side = MarkSide::Ours;
        if (!StampFromFact(net, holder, s.mark, f, m)) continue;
        if (s.basis == "ordered") m.certainty = MarkCertainty::Estimated;  // ordered there, not yet reported
        m.points = {at(s.x, s.y)};
        m.label = s.name.empty() ? s.id : s.name;
        out.marks.push_back(m);
    }
    for (const MapUnit& u : map.ours) {
        HeldMark m;
        m.kind = "note";
        m.id = u.unit;
        m.side = MarkSide::Ours;
        if (!StampFromFact(net, holder, u.mark, f, m)) continue;
        char buf[200];
        std::snprintf(buf, sizeof(buf), "%s: %.0f%%, supplies %.0f%%%s", u.name.c_str(), u.fraction * 100.0, u.supply * 100.0, u.out ? ", out of the fight" : "");
        m.label = buf;
        out.marks.push_back(m);
    }
    for (const MapEnemy& e : map.enemy) {
        HeldMark m;
        m.kind = "note";
        m.id = e.name;
        m.side = MarkSide::Enemy;
        if (!StampFromFact(net, holder, e.mark, f, m)) continue;
        m.certainty = MarkCertainty::Estimated;  // the intelligence summary's
        char buf[200];
        std::snprintf(buf, sizeof(buf), "%s: %.0f%%%s", e.name.c_str(), e.fraction * 100.0, e.out ? ", out of the fight" : "");
        m.label = buf;
        out.marks.push_back(m);
    }
    for (const MapAirFront& a : map.air_front) {
        HeldMark m;
        m.kind = "note";
        m.id = "air_front:" + a.sector_name;
        m.side = MarkSide::Enemy;
        if (!StampFromFact(net, holder, a.mark, f, m)) continue;
        m.certainty = MarkCertainty::Estimated;  // the air defence staff's conclusion
        char buf[200];
        std::snprintf(buf, sizeof(buf), "the enemy's air front in %s: %.0f km deep", a.sector_name.c_str(), a.depth_m / 1000.0);
        m.label = buf;
        out.marks.push_back(m);
    }
    for (const MapOrder& o : map.orders) {
        HeldMark m;
        m.kind = "note";
        m.id = "order:" + o.kind + ":" + o.unit;
        m.side = MarkSide::Ours;
        if (!StampFromFact(net, holder, o.mark, f, m)) continue;
        m.certainty = MarkCertainty::Confirmed;
        m.label = o.text;
        out.marks.push_back(m);
    }
}

}  // namespace command
}  // namespace simcore
