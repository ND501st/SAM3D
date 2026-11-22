// simcore/command/situation_map.hpp — the situation map a post holds: the front as its facts give it (track F, F5.1;
// docs/fronts/05_WHAT_THE_PLAYER_SEES.md sections 1, 5 and 6).
//
// First person (docs/command/01_PRINCIPLES.md): the player's character knows where the front is only through reports that
// reached his post. A post's situation map is built from the facts it holds and from nothing else, each mark with who
// compiled it, the hour it was compiled, the hour it reached the post and its age:
//   * the believed line by axis: the depth and pace of each axis in the latest report that reached the post (a unit's
//     return, or the army's summary), the enemy in contact as the unit believed him, our counterattack on it; the line
//     itself drawn from the layout's base line with those depths (BuildSituationMap), never carried on at the reported pace;
//   * our units as they reported themselves (strength, supplies, out of the fight, in which sector), the enemy's
//     formations not in contact as the army's intelligence summary gave them;
//   * the orders the post holds (our commanders' on the ground, the air defence's sector priorities and the sky's sharing),
//     the sector priorities by the pre-war plan until an order reached the post;
//   * the army's air defence staff's conclusions on the enemy's air front;
//   * our SAM units' places: the plan the post holds, then the last move ordered or reported that reached it.
// A mark is the latest that reached the post. It keeps its hour and is shown aged; it never moves on its own ("stale is
// stale"), and a post that holds no report of an axis has no mark for it.
//
// Nothing is read from the ground war, the air campaign or any truth. The builder takes the pipeline (InfoNet), the
// campaign's feed (the records it made of the facts it put into the net: GroundFeed, AdStrategy, the air front watch) and
// the layout, and a record is used only where the post holds its fact (InfoNet::Known). The layout is the map's own data:
// the names, the base line, the axes' places on it, the order of battle's SAM units' plan positions; SituationLayoutOf is
// the one function that reads a campaign for it, and reads only what the war does not change.
//
// Cheap: a map is built on demand (a post's view asked for, a debrief) and kept per post by the net's revision
// (SituationMaps); nothing is built per step.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/campaign/ad_strategy.hpp"
#include "simcore/command/air_war_feed.hpp"
#include "simcore/command/ground_feed.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/command/own_air.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace command {

// ------------------------------------------------------------------------------------------------------------- the layout

// What a map is drawn on: names and places that do not change with the war.
struct SituationLayout {
    struct Sector {
        std::string id, name;
    };
    struct Axis {
        std::string name;
        int sector = -1;
        double along_m = 0.0, half_width_m = 0.0, taper_m = 0.0;  // its place on the base line and the shape of a penetration
    };
    struct Station {  // a point of the base line every station spacing
        double along_m = 0.0;
        Vec3 base;
        Vec3 inward;  // the unit normal toward our side
        int sector = 0;
    };
    struct Unit {  // one of our SAM units that moves on orders: the plan's place
        std::string id, name;
        Vec3 plan;
        int sector = 0;
        int commander = -1;  // the commander (an index of AdStrategy's) who orders it
        int post = -1;       // its own commander's position
    };
    std::vector<Sector> sectors;
    std::vector<Axis> axes;                  // the ground war's, by its index (empty without it)
    std::vector<std::string> formation_name;  // by the ground war's formation index
    std::vector<char> formation_ours;
    std::vector<Station> stations;
    std::vector<Unit> units;
    SkySharingDef sky;       // the sky's sharing by priority (own_air.json), present or not
    double stale_h = 14.0;   // a mark older than this is shown as stale: the units' returns come twice a day, 12 h apart, and a few hours late (game value)
    std::size_t max_orders = 12;  // the newest orders held that are listed (game value)
    double line_step_m = 5000.0;
};

// The layout of a campaign's front, from the war's static data only (the sectors' names, the base line, the axes' places
// and the formations' names, the theatre's SAM units). `ad` gives which post holds each unit's plan: build it after the
// feed's first pump, when the strategy is on. `own_air` gives the sky's sharing (null: none).
inline SituationLayout SituationLayoutOf(const airwar::AirCampaign& c, const campaign::AdStrategy* ad = nullptr, const OwnAirDef* own_air = nullptr) {
    SituationLayout L;
    const airwar::LineOfContact& line = c.FrontLine();
    for (std::size_t i = 0; i < line.SectorCount(); ++i) L.sectors.push_back({line.Sector(i).id, line.Sector(i).name.empty() ? line.Sector(i).id : line.Sector(i).name});
    if (!line.Empty()) {
        for (std::size_t i = 0; i < line.StationCount(); ++i) {
            SituationLayout::Station s;
            s.along_m = line.StationAlong(i);
            s.base = line.PointAt(s.along_m);
            s.inward = line.InwardNormal(s.along_m);
            s.sector = line.SectorAt(s.along_m);
            L.stations.push_back(s);
        }
        L.line_step_m = line.StationSpacing();
    }
    if (const groundwar::GroundWar* g = c.Ground()) {
        for (const groundwar::Axis& a : g->Axes()) L.axes.push_back({a.name.empty() ? a.id : a.name, a.sector, a.along_m, a.half_width_m, a.taper_m});
        for (const groundwar::Formation& f : g->Formations()) {
            L.formation_name.push_back(f.name);
            L.formation_ours.push_back(f.unit.side == groundwar::Side::Ours ? 1 : 0);
        }
    }
    if (own_air != nullptr) L.sky = own_air->sky;
    if (ad != nullptr && ad->On()) {
        for (std::size_t i = 0; i < ad->MoveUnits(); ++i) {
            const int obj = ad->MoveUnitObject(i);
            if (obj < 0 || static_cast<std::size_t>(obj) >= c.Data().theatre.objects.size()) continue;
            const airwar::TheatreObject& o = c.Data().theatre.objects[static_cast<std::size_t>(obj)];
            SituationLayout::Unit u;
            u.id = o.id;
            u.name = o.name;
            u.plan = o.pos;
            u.sector = line.Empty() ? 0 : line.SectorOf(o.pos);
            u.commander = ad->MoveUnitCommander(i);
            u.post = ad->MoveUnitPost(i);
            L.units.push_back(u);
        }
    }
    return L;
}

// ------------------------------------------------------------------------------------------------------------- the map

// Where a mark comes from and how old it is.
struct SituationMark {
    std::string source;     // the post that compiled, ordered or stated it ("plan": the plan the post holds from before the war)
    std::string key;        // the fact it rests on ("" for the plan)
    double h = -1.0;        // compiled, ordered or stated (campaign hours; -1 the plan)
    double got_h = -1.0;    // when it reached this post (-1 the plan)
    double age_h = 0.0;     // the post's hour less h (the plan: the hours of the war so far)
    bool stale = false;     // older than the layout's stale_h (never the plan)
};

struct MapAxis {
    int index = -1;
    std::string name;
    int sector = -1;
    double depth_m = 0.0;       // how far in the unit that reported it said the enemy was
    double pace_km_day = 0.0;
    bool counterattack = false;
    int enemy_n = 0;            // the enemy's formations in contact as the unit believed them
    double enemy_fraction = 0.0;
    bool by_summary = false;    // from the army's summary, not a unit's report
    SituationMark mark;
};
struct MapUnit {
    std::string unit;    // the order of battle's id
    std::string name;
    int formation = -1;
    int sector = -1;
    double fraction = 0.0;
    double supply = 1.0;
    bool out = false;
    std::string cause;   // why it reported: "scheduled", "asked", "withdrawn"...
    bool by_summary = false;
    SituationMark mark;
};
struct MapEnemy {  // the army's intelligence summary's line
    std::string name;
    int sector = -1;
    double fraction = 0.0;
    bool out = false;
    SituationMark mark;
};
struct MapPriority {
    std::string commander;  // who gave it (his id)
    int sector = -1;
    std::string sector_name;
    std::string priority;   // "economy", "defend", "offence"
    std::string sharing;    // the sky's sharing it implies ("" none given)
    SituationMark mark;
};
struct MapAirFront {
    int sector = -1;
    std::string sector_name;
    double depth_m = 0.0;
    int signs = 0;
    SituationMark mark;
};
struct MapSam {
    std::string id;
    std::string name;
    double x = 0.0, y = 0.0;   // the theatre's metres (east, north)
    int sector = 0;
    std::string basis;         // "plan", "ordered" (the post holds the move's order) or "reported" (ready there)
    SituationMark mark;
};
struct MapOrder {
    std::string kind;          // "withdraw_ordered", "relief_ordered"...
    std::string unit;
    int axis = -1;
    std::string text;
    SituationMark mark;
};
struct MapLinePoint {
    double along_m = 0.0;
    double depth_m = 0.0;      // believed in beyond the base line
    double bx = 0.0, by = 0.0; // the base line there
    double x = 0.0, y = 0.0;   // the line as believed
    int sector = 0;
};

struct SituationMap {
    std::string post;
    double now_h = 0.0;
    double stale_h = 14.0;
    std::vector<MapLinePoint> line;
    std::vector<MapAxis> axes;
    std::vector<MapUnit> ours;
    std::vector<MapEnemy> enemy;
    std::vector<MapPriority> priorities;
    std::vector<MapAirFront> air_front;
    std::vector<MapSam> sams;
    std::vector<MapOrder> orders;

    bool Empty() const { return axes.empty() && ours.empty() && enemy.empty() && priorities.empty() && air_front.empty() && orders.empty(); }
    const MapAxis* FindAxis(const std::string& name) const {
        for (const MapAxis& a : axes) {
            if (a.name == name) return &a;
        }
        return nullptr;
    }
    // The oldest mark's age, or 0 with none (the post's whole picture is as old as its oldest mark; the plan's marks are not counted).
    double OldestAgeH() const {
        double oldest = 0.0;
        const auto look = [&](const SituationMark& m) {
            if (m.h >= 0.0) oldest = std::max(oldest, m.age_h);
        };
        for (const MapAxis& a : axes) look(a.mark);
        for (const MapUnit& a : ours) look(a.mark);
        for (const MapEnemy& a : enemy) look(a.mark);
        return oldest;
    }
};

// ------------------------------------------------------------------------------------------------------------- building it

namespace situation_detail {

inline SituationMark MarkOf(const InfoNet& net, const SituationLayout& L, int fact, double compiled_h, double got_h) {
    SituationMark m;
    const Fact& f = net.Facts()[static_cast<std::size_t>(fact)];
    m.source = f.origin;
    m.key = f.key;
    m.h = compiled_h;
    m.got_h = got_h;
    m.age_h = std::max(0.0, net.Now() - compiled_h);
    m.stale = m.age_h > L.stale_h;
    return m;
}

inline SituationMark PlanMark(const InfoNet& net) {
    SituationMark m;
    m.source = "plan";
    m.age_h = std::max(0.0, net.Now());
    return m;
}

inline const char* CauseName(groundwar::SitCause why) {
    switch (why) {
        case groundwar::SitCause::Scheduled: return "scheduled";
        case groundwar::SitCause::Reported: return "reported";
        case groundwar::SitCause::Asked: return "asked";
        case groundwar::SitCause::AttackStopped: return "attack_stopped";
        case groundwar::SitCause::Withdrawn: return "withdrawn";
        case groundwar::SitCause::Dispersed: return "dispersed";
        case groundwar::SitCause::Reformed: return "reformed";
        case groundwar::SitCause::AxisNews: return "axis_news";
    }
    return "scheduled";
}

inline std::string SectorNameOf(const SituationLayout& L, int s) {
    return s >= 0 && static_cast<std::size_t>(s) < L.sectors.size() ? L.sectors[static_cast<std::size_t>(s)].name : std::string();
}

}  // namespace situation_detail

// The situation map of `post` now: what its facts hold. `net` and `feed` are read only; the layout carries the names.
inline void BuildSituationMap(const InfoNet& net, const CampaignFeed& feed, const SituationLayout& L, int post, SituationMap& out) {
    using namespace situation_detail;
    out = SituationMap();
    const ChainData& data = net.Data();
    if (post < 0 || static_cast<std::size_t>(post) >= data.positions.size()) return;
    const std::string& post_id = data.positions[static_cast<std::size_t>(post)].id;
    out.post = post_id;
    out.now_h = net.Now();
    out.stale_h = L.stale_h;
    const auto fname = [&](int formation) {
        return formation >= 0 && static_cast<std::size_t>(formation) < L.formation_name.size() ? L.formation_name[static_cast<std::size_t>(formation)] : std::string();
    };

    // ---- what our units and the army reported
    const GroundFeed& gf = feed.Ground();
    if (gf.On()) {
        struct AxisPick {
            bool has = false;
            double compiled_h = -1e18;
            double got_h = 0.0;
            int fact = -1;
            groundwar::SitAxis axis;
            bool summary = false;
        };
        std::vector<AxisPick> picks(L.axes.size());
        const auto offer = [&](const groundwar::SitAxis& a, double compiled_h, int fact, double got_h, bool summary) {
            if (a.axis < 0 || static_cast<std::size_t>(a.axis) >= picks.size()) return;
            AxisPick& p = picks[static_cast<std::size_t>(a.axis)];
            if (p.has && compiled_h <= p.compiled_h) return;  // on a tie the first stays: a unit's own report before a summary of it
            p.has = true;
            p.compiled_h = compiled_h;
            p.got_h = got_h;
            p.fact = fact;
            p.axis = a;
            p.summary = summary;
        };
        struct UnitPick {
            const SituationReport* r = nullptr;
            double got_h = 0.0;
            int fact = -1;  // the fact the post holds it by (the report's own, or the summary's)
            bool summary = false;
        };
        std::map<int, UnitPick> units;
        const auto offer_unit = [&](const SituationReport& r, int fact, double got_h, bool summary) {
            const auto it = units.find(r.formation);
            if (it != units.end() && r.h <= it->second.r->h) return;
            units[r.formation] = {&r, got_h, fact, summary};
        };
        for (const SituationReport& r : gf.Reports()) {
            const Delivery* d = net.Known(post, r.fact);
            if (d == nullptr) continue;
            for (const groundwar::SitAxis& a : r.axes) offer(a, r.h, r.fact, d->h, false);
            if (r.own) offer_unit(r, r.fact, d->h, false);
        }
        for (const ArmySummary& s : gf.Summaries()) {
            const Delivery* d = net.Known(post, s.fact);
            if (d == nullptr) continue;
            for (std::size_t k = 0; k < s.axes.size() && k < s.axes_h.size(); ++k) offer(s.axes[k], s.axes_h[k], s.fact, d->h, true);
            for (int ri : s.reports) {
                if (ri >= 0 && static_cast<std::size_t>(ri) < gf.Reports().size()) offer_unit(gf.Reports()[static_cast<std::size_t>(ri)], s.fact, d->h, true);
            }
        }
        for (std::size_t i = 0; i < picks.size(); ++i) {
            const AxisPick& p = picks[i];
            if (!p.has) continue;
            MapAxis a;
            a.index = static_cast<int>(i);
            a.name = L.axes[i].name;
            a.sector = p.axis.sector >= 0 ? p.axis.sector : L.axes[i].sector;
            a.depth_m = p.axis.depth_m;
            a.pace_km_day = p.axis.pace_km_day;
            a.counterattack = p.axis.counterattacked;
            a.enemy_n = p.axis.enemy_n;
            a.enemy_fraction = p.axis.enemy_fraction;
            a.by_summary = p.summary;
            a.mark = MarkOf(net, L, p.fact, p.compiled_h, p.got_h);
            out.axes.push_back(std::move(a));
        }
        for (const auto& kv : units) {
            const UnitPick& u = kv.second;
            MapUnit m;
            m.unit = u.r->unit;
            m.name = fname(u.r->formation);
            m.formation = u.r->formation;
            m.sector = u.r->sector;
            m.fraction = u.r->fraction;
            m.supply = u.r->supply;
            m.out = u.r->out;
            m.cause = CauseName(u.r->cause);
            m.by_summary = u.summary;
            m.mark = MarkOf(net, L, u.fact, u.r->h, u.got_h);
            out.ours.push_back(std::move(m));
        }
        // the enemy's formations not in contact: the latest intelligence summary that reached the post
        const IntelRecord* intel = nullptr;
        double intel_got = 0.0;
        for (const IntelRecord& r : gf.Intels()) {
            const Delivery* d = net.Known(post, r.fact);
            if (d == nullptr || (intel != nullptr && r.compiled_h <= intel->compiled_h)) continue;
            intel = &r;
            intel_got = d->h;
        }
        if (intel != nullptr) {
            for (const IntelRecord::Line& l : intel->lines) {
                MapEnemy e;
                e.name = fname(l.formation);
                e.sector = l.sector;
                e.fraction = l.fraction;
                e.out = l.out;
                e.mark = MarkOf(net, L, intel->fact, intel->compiled_h, intel_got);
                out.enemy.push_back(std::move(e));
            }
        }
        // the orders held, the newest first
        std::vector<std::pair<double, std::size_t>> held;
        for (std::size_t i = 0; i < gf.Orders().size(); ++i) {
            if (net.Known(post, gf.Orders()[i].fact) != nullptr) held.push_back({gf.Orders()[i].h, i});
        }
        std::stable_sort(held.begin(), held.end(), [](const std::pair<double, std::size_t>& a, const std::pair<double, std::size_t>& b) { return a.first > b.first; });
        if (held.size() > L.max_orders) held.resize(L.max_orders);
        for (const auto& hk : held) {
            const OrderRecord& o = gf.Orders()[hk.second];
            const Delivery* d = net.Known(post, o.fact);
            const Fact& f = net.Facts()[static_cast<std::size_t>(o.fact)];
            MapOrder m;
            m.kind = groundwar::GroundOrderName(o.order);
            m.unit = o.unit;
            m.axis = o.axis;
            m.text = f.summary.empty() ? f.text : f.summary;
            m.mark = MarkOf(net, L, o.fact, o.h, d != nullptr ? d->h : 0.0);
            out.orders.push_back(std::move(m));
        }
    }

    // ---- the air defence commanders' orders and the staff's conclusions on the enemy's air front
    const campaign::AdStrategy& ad = feed.Ad();
    if (ad.On()) {
        for (std::size_t k = 0; k < ad.Commanders(); ++k) {
            const campaign::AdCommanderDef& def = ad.Def(static_cast<int>(k));
            bool mine = def.post == post_id;
            for (const std::string& to : def.to) mine = mine || to == post_id;
            for (std::size_t s = 0; s < L.sectors.size(); ++s) {
                double h = -1.0;
                const campaign::SectorPriority p = ad.PriorityKnownBy(static_cast<int>(k), post, static_cast<int>(s), &h);
                if (h < 0.0 && !mine) continue;  // the plan is held by those it is for; an order, by whoever it reached
                MapPriority m;
                m.commander = def.id;
                m.sector = static_cast<int>(s);
                m.sector_name = L.sectors[s].name;
                m.priority = campaign::SectorPriorityName(p);
                if (L.sky.present) m.sharing = CooperationMethodName(SharingFor(L.sky, m.priority));
                if (h >= 0.0) {
                    // the order that gave it: the latest decision of his on this sector whose fact the post holds
                    int fact = -1;
                    for (std::size_t i = ad.Decisions().size(); i-- > 0 && fact < 0;) {
                        const campaign::AdDecision& d = ad.Decisions()[i];
                        if (d.commander == static_cast<int>(k) && d.sector == static_cast<int>(s) && d.fact >= 0 && net.Known(post, d.fact) != nullptr) fact = d.fact;
                    }
                    if (fact >= 0) {
                        m.mark = MarkOf(net, L, fact, net.Facts()[static_cast<std::size_t>(fact)].h, h);
                    } else {
                        m.mark = PlanMark(net);
                    }
                } else {
                    m.mark = PlanMark(net);
                }
                out.priorities.push_back(std::move(m));
            }
        }
    }
    const std::vector<AirFrontConclusion>& concl = feed.Watch().Conclusions();
    for (std::size_t s = 0; s < L.sectors.size(); ++s) {
        for (std::size_t i = concl.size(); i-- > 0;) {
            const AirFrontConclusion& c = concl[i];
            if (c.sector != static_cast<int>(s) || c.key.empty()) continue;
            const int fact = net.FactIndex(c.key);
            const Delivery* d = net.Known(post, fact);
            if (d == nullptr) continue;
            MapAirFront a;
            a.sector = static_cast<int>(s);
            a.sector_name = L.sectors[s].name;
            a.depth_m = c.depth_m;
            a.signs = c.signs;
            a.mark = MarkOf(net, L, fact, c.h, d->h);
            out.air_front.push_back(std::move(a));
            break;
        }
    }

    // ---- our SAM units: the plan held by the post that commands the unit or is the unit's own, else only what reached it
    if (ad.On()) {
        for (const SituationLayout::Unit& u : L.units) {
            MapSam m;
            m.id = u.id;
            m.name = u.name;
            m.x = u.plan.x;
            m.y = u.plan.y;
            m.sector = u.sector;
            m.basis = "plan";
            const bool holds_plan = u.post == post || (u.commander >= 0 && static_cast<std::size_t>(u.commander) < ad.Commanders() &&
                                                       ad.Def(u.commander).post == post_id);
            const campaign::AdStrategy::MoveOrder* latest = nullptr;
            double latest_h = -1e18;
            for (const campaign::AdStrategy::MoveOrder& o : ad.MoveOrders()) {
                if (o.unit != u.id || o.fact < 0 || net.Known(post, o.fact) == nullptr) continue;
                const double oh = net.Facts()[static_cast<std::size_t>(o.fact)].h;
                if (oh >= latest_h) {
                    latest = &o;
                    latest_h = oh;
                }
            }
            if (latest != nullptr) {
                m.x = latest->to.x;
                m.y = latest->to.y;
                m.basis = "ordered";
                m.mark = MarkOf(net, L, latest->fact, latest_h, net.Known(post, latest->fact)->h);
                if (latest->ready_fact >= 0) {
                    if (const Delivery* r = net.Known(post, latest->ready_fact)) {
                        m.basis = "reported";
                        m.mark = MarkOf(net, L, latest->ready_fact, net.Facts()[static_cast<std::size_t>(latest->ready_fact)].h, r->h);
                    }
                }
            } else if (holds_plan) {
                m.mark = PlanMark(net);
            } else {
                continue;
            }
            out.sams.push_back(std::move(m));
        }
    }

    // ---- the line as believed: the base line with each axis's reported depth in its shape
    if (!out.axes.empty()) {
        for (const SituationLayout::Station& st : L.stations) {
            double depth = 0.0;
            for (const MapAxis& a : out.axes) {
                const SituationLayout::Axis& g = L.axes[static_cast<std::size_t>(a.index)];
                if (!(a.depth_m > 0.0)) continue;
                const double off = std::fabs(st.along_m - g.along_m);
                double d = 0.0;
                if (off <= g.half_width_m) d = a.depth_m;
                else if (g.taper_m > 0.0 && off < g.half_width_m + g.taper_m) d = a.depth_m * (1.0 - (off - g.half_width_m) / g.taper_m);
                depth = std::max(depth, d);
            }
            MapLinePoint p;
            p.along_m = st.along_m;
            p.depth_m = depth;
            p.bx = st.base.x;
            p.by = st.base.y;
            p.x = st.base.x + st.inward.x * depth;
            p.y = st.base.y + st.inward.y * depth;
            p.sector = st.sector;
            out.line.push_back(p);
        }
    }
}

// A post's maps kept between asks: rebuilt only when the net has changed (InfoNet::Revision) or the feed's records have.
// A reference returned stays valid (maps of other posts do not move it) until the same post's map is asked for after a change.
class SituationMaps {
public:
    const SituationMap& For(const InfoNet& net, const CampaignFeed& feed, const SituationLayout& layout, int post) {
        const std::uint64_t stamp = Stamp(net, feed);
        const auto it = entries_.find(post);
        Entry& e = it != entries_.end() ? it->second : entries_[post];
        if (it == entries_.end() || e.stamp != stamp) {
            BuildSituationMap(net, feed, layout, post, e.map);
            e.stamp = stamp;
            ++builds_;
        }
        return e.map;
    }
    void Clear() { entries_.clear(); }
    // Maps built so far (a test's: asking twice builds once).
    int Builds() const { return builds_; }

private:
    struct Entry {
        std::uint64_t stamp = 0;
        SituationMap map;
    };
    std::map<int, Entry> entries_;
    int builds_ = 0;
    static std::uint64_t Stamp(const InfoNet& net, const CampaignFeed& feed) {
        std::uint64_t s = net.Revision();
        s = s * 1000003u + feed.Ground().Reports().size();
        s = s * 1000003u + feed.Ground().Orders().size() + feed.Ground().Summaries().size() + feed.Ground().Intels().size();
        s = s * 1000003u + feed.Ad().Decisions().size() + feed.Ad().MoveOrders().size() + feed.Watch().Conclusions().size();
        return s;
    }
};

// ------------------------------------------------------------------------------------------------------------- JSON and text

namespace situation_detail {

inline void PutMark(json::Value& o, const SituationMark& m) {
    using namespace airwar::jsonb;
    Put(o, "source", Str(m.source));
    if (!m.key.empty()) Put(o, "key", Str(m.key));
    Put(o, "h", Num(m.h));
    Put(o, "got_h", Num(m.got_h));
    Put(o, "age_h", Num(m.age_h));
    if (m.stale) Put(o, "stale", Bool(true));
}

inline SituationMark ReadMark(const json::Value& o) {
    SituationMark m;
    const auto str = [&](const char* k) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->Is(json::Type::String) ? v->string : std::string();
    };
    const auto num = [&](const char* k, double dflt) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->Is(json::Type::Number) ? v->number : dflt;
    };
    m.source = str("source");
    m.key = str("key");
    m.h = num("h", -1.0);
    m.got_h = num("got_h", -1.0);
    m.age_h = num("age_h", 0.0);
    const json::Value* st = o.Find("stale");
    m.stale = st != nullptr && st->boolean;
    return m;
}

}  // namespace situation_detail

// The map as the post's view sends it (PostViewJson), as a JSON value.
inline json::Value SituationMapValue(const SituationMap& m) {
    using namespace airwar::jsonb;
    using namespace situation_detail;
    json::Value root = Obj();
    Put(root, "post", Str(m.post));
    Put(root, "now_h", Num(m.now_h));
    Put(root, "stale_h", Num(m.stale_h));
    json::Value line = Arr();
    for (const MapLinePoint& p : m.line) {
        json::Value j = Obj();
        Put(j, "along_m", Num(p.along_m));
        Put(j, "depth_m", Num(p.depth_m));
        Put(j, "bx", Num(p.bx));
        Put(j, "by", Num(p.by));
        Put(j, "x", Num(p.x));
        Put(j, "y", Num(p.y));
        Put(j, "sector", Num(p.sector));
        Push(line, j);
    }
    Put(root, "line", line);
    json::Value axes = Arr();
    for (const MapAxis& a : m.axes) {
        json::Value j = Obj();
        Put(j, "axis", Num(a.index));
        Put(j, "name", Str(a.name));
        Put(j, "sector", Num(a.sector));
        Put(j, "depth_m", Num(a.depth_m));
        Put(j, "pace_km_day", Num(a.pace_km_day));
        Put(j, "counterattack", Bool(a.counterattack));
        Put(j, "enemy_n", Num(a.enemy_n));
        Put(j, "enemy_fraction", Num(a.enemy_fraction));
        if (a.by_summary) Put(j, "by_summary", Bool(true));
        PutMark(j, a.mark);
        Push(axes, j);
    }
    Put(root, "axes", axes);
    json::Value ours = Arr();
    for (const MapUnit& u : m.ours) {
        json::Value j = Obj();
        Put(j, "unit", Str(u.unit));
        Put(j, "name", Str(u.name));
        Put(j, "formation", Num(u.formation));
        Put(j, "sector", Num(u.sector));
        Put(j, "fraction", Num(u.fraction));
        Put(j, "supply", Num(u.supply));
        Put(j, "out", Bool(u.out));
        Put(j, "cause", Str(u.cause));
        if (u.by_summary) Put(j, "by_summary", Bool(true));
        PutMark(j, u.mark);
        Push(ours, j);
    }
    Put(root, "ours", ours);
    json::Value enemy = Arr();
    for (const MapEnemy& e : m.enemy) {
        json::Value j = Obj();
        Put(j, "name", Str(e.name));
        Put(j, "sector", Num(e.sector));
        Put(j, "fraction", Num(e.fraction));
        Put(j, "out", Bool(e.out));
        PutMark(j, e.mark);
        Push(enemy, j);
    }
    Put(root, "enemy", enemy);
    json::Value pri = Arr();
    for (const MapPriority& p : m.priorities) {
        json::Value j = Obj();
        Put(j, "commander", Str(p.commander));
        Put(j, "sector", Num(p.sector));
        Put(j, "sector_name", Str(p.sector_name));
        Put(j, "priority", Str(p.priority));
        if (!p.sharing.empty()) Put(j, "sharing", Str(p.sharing));
        PutMark(j, p.mark);
        Push(pri, j);
    }
    Put(root, "priorities", pri);
    json::Value af = Arr();
    for (const MapAirFront& a : m.air_front) {
        json::Value j = Obj();
        Put(j, "sector", Num(a.sector));
        Put(j, "sector_name", Str(a.sector_name));
        Put(j, "depth_m", Num(a.depth_m));
        Put(j, "signs", Num(a.signs));
        PutMark(j, a.mark);
        Push(af, j);
    }
    Put(root, "air_front", af);
    json::Value sams = Arr();
    for (const MapSam& s : m.sams) {
        json::Value j = Obj();
        Put(j, "id", Str(s.id));
        Put(j, "name", Str(s.name));
        Put(j, "x", Num(s.x));
        Put(j, "y", Num(s.y));
        Put(j, "sector", Num(s.sector));
        Put(j, "basis", Str(s.basis));
        PutMark(j, s.mark);
        Push(sams, j);
    }
    Put(root, "sams", sams);
    json::Value orders = Arr();
    for (const MapOrder& o : m.orders) {
        json::Value j = Obj();
        Put(j, "kind", Str(o.kind));
        Put(j, "unit", Str(o.unit));
        Put(j, "axis", Num(o.axis));
        Put(j, "text", Str(o.text));
        PutMark(j, o.mark);
        Push(orders, j);
    }
    Put(root, "orders", orders);
    return root;
}

inline std::string SituationMapJson(const SituationMap& m) { return airwar::jsonb::Dump(SituationMapValue(m), 0, 17); }

// A map read back from its JSON (a client holding its post's view).
inline bool ReadSituationMap(const json::Value& root, SituationMap& out) {
    using namespace situation_detail;
    if (!root.Is(json::Type::Object)) return false;
    out = SituationMap();
    const auto str = [](const json::Value& o, const char* k) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->Is(json::Type::String) ? v->string : std::string();
    };
    const auto num = [](const json::Value& o, const char* k, double dflt) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->Is(json::Type::Number) ? v->number : dflt;
    };
    const auto inum = [&](const json::Value& o, const char* k, int dflt) { return static_cast<int>(std::lround(num(o, k, dflt))); };
    const auto flag = [](const json::Value& o, const char* k) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->boolean;
    };
    out.post = str(root, "post");
    out.now_h = num(root, "now_h", 0.0);
    out.stale_h = num(root, "stale_h", 14.0);
    if (const json::Value* a = root.Find("line")) {
        for (const json::Value& j : a->items) {
            MapLinePoint p;
            p.along_m = num(j, "along_m", 0.0);
            p.depth_m = num(j, "depth_m", 0.0);
            p.bx = num(j, "bx", 0.0);
            p.by = num(j, "by", 0.0);
            p.x = num(j, "x", 0.0);
            p.y = num(j, "y", 0.0);
            p.sector = inum(j, "sector", 0);
            out.line.push_back(p);
        }
    }
    if (const json::Value* a = root.Find("axes")) {
        for (const json::Value& j : a->items) {
            MapAxis x;
            x.index = inum(j, "axis", -1);
            x.name = str(j, "name");
            x.sector = inum(j, "sector", -1);
            x.depth_m = num(j, "depth_m", 0.0);
            x.pace_km_day = num(j, "pace_km_day", 0.0);
            x.counterattack = flag(j, "counterattack");
            x.enemy_n = inum(j, "enemy_n", 0);
            x.enemy_fraction = num(j, "enemy_fraction", 0.0);
            x.by_summary = flag(j, "by_summary");
            x.mark = ReadMark(j);
            out.axes.push_back(x);
        }
    }
    if (const json::Value* a = root.Find("ours")) {
        for (const json::Value& j : a->items) {
            MapUnit x;
            x.unit = str(j, "unit");
            x.name = str(j, "name");
            x.formation = inum(j, "formation", -1);
            x.sector = inum(j, "sector", -1);
            x.fraction = num(j, "fraction", 0.0);
            x.supply = num(j, "supply", 1.0);
            x.out = flag(j, "out");
            x.cause = str(j, "cause");
            x.by_summary = flag(j, "by_summary");
            x.mark = ReadMark(j);
            out.ours.push_back(x);
        }
    }
    if (const json::Value* a = root.Find("enemy")) {
        for (const json::Value& j : a->items) {
            MapEnemy x;
            x.name = str(j, "name");
            x.sector = inum(j, "sector", -1);
            x.fraction = num(j, "fraction", 0.0);
            x.out = flag(j, "out");
            x.mark = ReadMark(j);
            out.enemy.push_back(x);
        }
    }
    if (const json::Value* a = root.Find("priorities")) {
        for (const json::Value& j : a->items) {
            MapPriority x;
            x.commander = str(j, "commander");
            x.sector = inum(j, "sector", -1);
            x.sector_name = str(j, "sector_name");
            x.priority = str(j, "priority");
            x.sharing = str(j, "sharing");
            x.mark = ReadMark(j);
            out.priorities.push_back(x);
        }
    }
    if (const json::Value* a = root.Find("air_front")) {
        for (const json::Value& j : a->items) {
            MapAirFront x;
            x.sector = inum(j, "sector", -1);
            x.sector_name = str(j, "sector_name");
            x.depth_m = num(j, "depth_m", 0.0);
            x.signs = inum(j, "signs", 0);
            x.mark = ReadMark(j);
            out.air_front.push_back(x);
        }
    }
    if (const json::Value* a = root.Find("sams")) {
        for (const json::Value& j : a->items) {
            MapSam x;
            x.id = str(j, "id");
            x.name = str(j, "name");
            x.x = num(j, "x", 0.0);
            x.y = num(j, "y", 0.0);
            x.sector = inum(j, "sector", 0);
            x.basis = str(j, "basis");
            x.mark = ReadMark(j);
            out.sams.push_back(x);
        }
    }
    if (const json::Value* a = root.Find("orders")) {
        for (const json::Value& j : a->items) {
            MapOrder x;
            x.kind = str(j, "kind");
            x.unit = str(j, "unit");
            x.axis = inum(j, "axis", -1);
            x.text = str(j, "text");
            x.mark = ReadMark(j);
            out.orders.push_back(x);
        }
    }
    return true;
}

// The map as a page of plain text, for the debrief and the benches: each mark with its source and age, stale ones starred.
inline std::string SituationMapText(const SituationMap& m, double start_hour = 0.0) {
    std::string s;
    char buf[320];
    const auto age = [&](const SituationMark& k) {
        if (k.h < 0.0) return std::string("the plan");
        char t[192];
        std::snprintf(t, sizeof(t), "%s, %.1f h old%s, from %s", FormatClock(k.h, start_hour).c_str(), k.age_h, k.stale ? " STALE" : "", k.source.c_str());
        return std::string(t);
    };
    std::snprintf(buf, sizeof(buf), "Situation map of %s at %s\n", m.post.c_str(), FormatClock(m.now_h, start_hour).c_str());
    s += buf;
    if (m.Empty() && m.sams.empty()) return s + "  nothing has reached the post.\n";
    if (!m.axes.empty()) s += "  The line as reported, by axis:\n";
    for (const MapAxis& a : m.axes) {
        std::snprintf(buf, sizeof(buf), "    %-26s %5.1f km in, %4.1f km/day%s, enemy %d at %.0f%%  [%s%s]\n", a.name.c_str(), a.depth_m / 1000.0, a.pace_km_day,
                      a.counterattack ? ", counterattack" : "", a.enemy_n, 100.0 * a.enemy_fraction, a.by_summary ? "army's summary, " : "", age(a.mark).c_str());
        s += buf;
    }
    if (!m.ours.empty()) s += "  Our units as they reported:\n";
    for (const MapUnit& u : m.ours) {
        std::snprintf(buf, sizeof(buf), "    %-30s %3.0f%% strength, supplies %3.0f%%%s  [%s%s]\n", u.name.c_str(), 100.0 * u.fraction, 100.0 * u.supply,
                      u.out ? ", out of the fight" : "", u.by_summary ? "army's summary, " : "", age(u.mark).c_str());
        s += buf;
    }
    if (!m.enemy.empty()) s += "  The enemy not in contact, by the army's intelligence summary:\n";
    for (const MapEnemy& e : m.enemy) {
        std::snprintf(buf, sizeof(buf), "    %-30s about %3.0f%%%s  [%s]\n", e.name.c_str(), 100.0 * e.fraction, e.out ? ", out of the fight" : "", age(e.mark).c_str());
        s += buf;
    }
    if (!m.priorities.empty()) s += "  Sector priorities held:\n";
    for (const MapPriority& p : m.priorities) {
        std::snprintf(buf, sizeof(buf), "    %-20s %-8s %-8s sharing %-8s [%s]\n", p.sector_name.c_str(), p.commander.c_str(), p.priority.c_str(),
                      p.sharing.empty() ? "-" : p.sharing.c_str(), age(p.mark).c_str());
        s += buf;
    }
    if (!m.air_front.empty()) s += "  The enemy's air front, as the army's air defence staff concluded:\n";
    for (const MapAirFront& a : m.air_front) {
        std::snprintf(buf, sizeof(buf), "    %-20s %5.1f km beyond the line on %d signs  [%s]\n", a.sector_name.c_str(), a.depth_m / 1000.0, a.signs, age(a.mark).c_str());
        s += buf;
    }
    if (!m.sams.empty()) s += "  Our SAM units:\n";
    for (const MapSam& a : m.sams) {
        std::snprintf(buf, sizeof(buf), "    %-26s at %7.0f E %7.0f N, %-8s [%s]\n", a.name.c_str(), a.x, a.y, a.basis.c_str(), age(a.mark).c_str());
        s += buf;
    }
    if (!m.orders.empty()) s += "  Orders held:\n";
    for (const MapOrder& o : m.orders) {
        std::snprintf(buf, sizeof(buf), "    %s  [%s]\n", o.text.c_str(), age(o.mark).c_str());
        s += buf;
    }
    return s;
}

}  // namespace command
}  // namespace simcore
