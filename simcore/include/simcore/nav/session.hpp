// simcore/nav/session.hpp — the route service in a game session and what the player could see (roadmap G2.3, G2.6, G11;
// docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.2, §5.12).
//
// A session on a map of the country (the Kesmark range) has the country's roads and the district's tracks as one road
// graph, the chain's places and the country's towns as anchors, and the mobility classes. With them:
// - a battery order may name where to go by an anchor ("to": "bty21_v2") instead of a polyline: the march is planned on
//   the roads for the column's slowest, as the battery commander's march plan (G2.3, nav/march.hpp);
// - the world's movers (another unit's column, a supply convoy, the TZM's run) go on the same roads (G2.6, G11.1);
// - the visual range from the viewers' eyes (G11.2): inside it the movers run at full detail (M2) and every vehicle of
//   them is listed for drawing where the simulation has it; beyond it by its margin they go back to M1, keeping their
//   place along their route, so nothing jumps at the edge.
// Unreal draws what is listed (`Visible`); no navigation mesh decides where anything goes (the owner's question 7).
#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/logistics.hpp"
#include "simcore/command/data.hpp"
#include "simcore/console_data.hpp"
#include "simcore/country_map.hpp"
#include "simcore/json.hpp"
#include "simcore/nav/anchors.hpp"
#include "simcore/nav/march.hpp"
#include "simcore/nav/movers.hpp"
#include "simcore/nav/routes.hpp"

namespace simcore {
namespace nav {

// ---------------------------------------------------------------------------------------------- the visual range (G11.2)

struct VisualRangeValues {
    double day_m = 8000.0;
    double twilight_m = 3000.0;
    double night_m = 1500.0;
    double twilight_deg = 6.0;
    double margin = 0.1;
};

namespace detail {

class VisualRangeLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, VisualRangeValues& out) {
        if (!Keys(root, "", {"visual_range_id", "status", "notes", "source", "confidence", "values"}) || !Provenance(root, "")) return false;
        const json::Value* values = root.Find("values");
        if (values == nullptr) return Fail("values", "required");
        const std::pair<const char*, double*> each[] = {{"day_m", &out.day_m}, {"twilight_m", &out.twilight_m}, {"night_m", &out.night_m},
                                                        {"twilight_deg", &out.twilight_deg}, {"margin", &out.margin}};
        if (!Keys(*values, "values", {"day_m", "twilight_m", "night_m", "twilight_deg", "margin"})) return false;
        for (const auto& e : each) {
            const std::string p = Join("values", e.first);
            const json::Value* v = values->Find(e.first);
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

inline bool LoadVisualRange(const std::string& data_dir, const std::string& rel, VisualRangeValues& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "visual range: cannot read " + rel;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "visual range: " + e;
        return false;
    }
    detail::VisualRangeLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "visual range: " + loader.Message();
        return false;
    }
    return true;
}

// How far a vehicle on the ground is seen: the light's range by the sun's elevation, capped by the weather's visibility.
inline double VisualRangeM(double visibility_m, double sun_elevation_deg, const VisualRangeValues& v) {
    const double light = sun_elevation_deg >= 0.0 ? v.day_m : sun_elevation_deg >= -v.twilight_deg ? v.twilight_m : v.night_m;
    return std::max(0.0, std::min(light, visibility_m));
}

// ---------------------------------------------------------------------------------------------- the supply lines (G2.6)

// The air war's supply columns as M0 movers on their lines' roads: each where its line's state has it, going out (leg 1)
// or coming back (leg 3) along its route, loading or unloading at an end. Their places are the air war's
// (logistics.hpp); as movers they are drawn, searched by armed reconnaissance (ArmedRecceSees) and put on a movement
// table like any other.
inline std::vector<Mover> SupplyColumnMovers(const airwar::AirWarData& d, const airwar::SupplyNetwork& net, const std::vector<airwar::ObjectState>& truth,
                                             double spacing_m = 50.0) {
    std::vector<Mover> out;
    for (std::size_t i = 0; i < truth.size() && i < d.theatre.objects.size(); ++i) {
        const airwar::TheatreObject& o = d.theatre.objects[i];
        if (o.supply_line < 0 || static_cast<std::size_t>(o.supply_line) >= net.Lines()) continue;
        const airwar::ObjectState& st = truth[i];
        if (st.leg < 0) continue;  // destroyed, waiting to be replaced
        const airwar::SupplyLineDef& l = d.theatre.supply_lines[static_cast<std::size_t>(o.supply_line)];
        const airwar::SupplyPath& p = net.Path(static_cast<std::size_t>(o.supply_line));
        if (p.pts.size() < 2) continue;
        Mover m;
        m.id = o.id;
        m.kind = l.net == "rail" ? "train" : "convoy";
        m.mobility = "truck";
        m.level = MoverLevel::M0;
        m.vehicles = std::max(1, l.vehicles);
        m.spacing_m = spacing_m;
        const bool back = st.leg >= 2;
        Route& r = m.route;
        r.found = true;
        for (std::size_t k = 0; k < p.pts.size(); ++k) {
            const Vec3& q = back ? p.pts[p.pts.size() - 1 - k] : p.pts[k];
            r.path.push_back({q.x, q.y});
        }
        for (std::size_t k = 0; k + 1 < r.path.size(); ++k) {
            const double len = std::hypot(r.path[k + 1].east_m - r.path[k].east_m, r.path[k + 1].north_m - r.path[k].north_m);
            RouteStretch s;
            s.kind = l.net;
            s.length_m = len;
            s.time_s = l.speed_mps > 0.0 ? len / l.speed_mps : 0.0;
            r.stretches.push_back(s);
            r.length_m += len;
            r.time_s += s.time_s;
        }
        m.arc_m = back ? p.Length() - st.along_m : st.along_m;
        m.moved_s = Movers::TimeTo(r, m.arc_m);
        m.state = st.leg == 1 || st.leg == 3 ? (p.cut ? MoverState::Blocked : MoverState::Moving) : MoverState::Halted;
        out.push_back(m);
    }
    return out;
}

// ---------------------------------------------------------------------------------------------- the session's roads (G2.3, G11)

// The basic model a mobility class is drawn as (data/scenery/basic_models.json's "models").
inline std::string ModelForMobility(const std::string& mobility) {
    if (mobility == "truck") return "ural_truck";
    if (mobility == "foot") return "soldier";
    if (mobility == "motorcycle") return "motorcycle";
    return mobility;  // osa_9a33, tzm_9t217, pu12m: the class is the model
}

// A mover the scenario puts on the roads: a column or a convoy from one anchor to another, departing at its time.
struct SessionMoverDef {
    std::string id;
    std::string kind = "convoy";   // "convoy", "column", "person", "carrier"
    std::string mobility = "truck";
    std::string from;              // anchors
    std::string to;
    double depart_s = 0.0;
    int vehicles = 1;
    double spacing_m = 50.0;
    std::string model;             // its basic model (data/scenery/basic_models.json); "" by its mobility
};

class SessionNav {
public:
    // The country's roads with the district's tracks (`district`: the session's map, null for the country's own), the
    // chain's places and the country's towns and landmarks as anchors, the mobility classes.
    bool Init(const std::string& data_dir, const std::string& country_file, const MapDef* district, const std::string& chain_file,
              std::string* error = nullptr) {
        std::string text, e;
        if (!simcore::detail::ReadTextFile(data_dir + "/" + country_file, text) || !LoadCountryDefJson(text, country_, &e)) {
            if (error != nullptr) *error = "nav: " + country_file + ": " + e;
            return false;
        }
        if (!LoadMobility(data_dir, "vehicles/mobility.json", mobility_, &e)) {
            if (error != nullptr) *error = "nav: " + e;
            return false;
        }
        if (!LoadVisualRange(data_dir, "gameplay/visual_range.json", range_, &e)) {
            if (error != nullptr) *error = "nav: " + e;
            return false;
        }
        graph_ = RoadGraph::ForCountry(country_, district);
        // The district's ground off the road (G1.3): a column's last kilometres and a man's walk go round its woods and water.
        if (district != nullptr) graph_.SetGround(std::make_shared<GroundGrid>(GroundGrid::FromMap(*district, kGroundStepM)));
        if (!chain_file.empty()) {
            command::ChainData chain;
            if (!command::LoadChainData(data_dir, chain_file, chain, &e)) {
                if (error != nullptr) *error = "nav: " + e;
                return false;
            }
            anchors_.AddChain(chain);
        }
        anchors_.AddCountry(country_);
        movers_ = Movers(range_.margin);
        ready_ = true;
        return true;
    }
    bool Ready() const { return ready_; }
    const RoadGraph& Graph() const { return graph_; }
    const Anchors& GetAnchors() const { return anchors_; }
    Anchors& MutableAnchors() { return anchors_; }
    const MobilityLibrary& MobilityClasses() const { return mobility_; }
    const VisualRangeValues& Range() const { return range_; }
    // The country's origin: the roads' frame, which a session's ENU must share.
    const GeoPoint& Origin() const { return country_.ground.origin; }

    // The battery commander's march to an anchor (G2.3): from where the column stands, on the roads for its slowest, the
    // order to move off given at `move_off_s` (the start point's time follows from it).
    MarchPlan PlanTo(const MapPoint& from, const std::string& to, const std::vector<MarchVehicle>& column, const BatteryTactics& tactics,
                     double move_off_s, std::string* why = nullptr) const {
        const Anchor* a = anchors_.Find(to);
        if (ready_ && a == nullptr) {
            if (why != nullptr) *why = "no place called '" + to + "'";
            return MarchPlan{};
        }
        return PlanBetween(from, a != nullptr ? a->at : MapPoint{}, a != nullptr ? a->name : to, column, tactics, move_off_s, why);
    }
    MarchPlan PlanBetween(const MapPoint& from, const MapPoint& to, const std::string& to_name, const std::vector<MarchVehicle>& column,
                          const BatteryTactics& tactics, double move_off_s, std::string* why = nullptr) const {
        if (!ready_) {
            if (why != nullptr) *why = "no roads in this session";
            return MarchPlan{};
        }
        MarchRequest q;
        q.from = from;
        q.to = to;
        q.from_name = "the battery's position";
        q.to_name = to_name;
        q.column = column;
        q.start_s = move_off_s;
        q.move_off_s = move_off_s;
        MarchPlan p = PlanMarch(graph_, mobility_, tactics, q, Overlay(), &anchors_);
        if (!p.found && why != nullptr) *why = p.why.empty() ? "no way there" : p.why;
        return p;
    }

    // A man's way on foot between two points (G2.4: the crew's walks, CrewMover::SetFootRoute): by the tracks and round
    // the district's water and woods; its points from start to end. Empty: no way (he goes straight).
    std::vector<MapPoint> FootWay(const MapPoint& a, const MapPoint& b) const {
        RouteRequest q;
        q.from = a;
        q.to = b;
        q.mobility = mobility_.Find("foot");
        q.swim = false;
        if (!ready_ || q.mobility == nullptr) return {};
        const Route r = graph_.Find(q);
        return r.found ? r.path : std::vector<MapPoint>{};
    }

    // Battery 21's column in its order of march (the PU-12M, four launchers, two TZMs, the maintenance vehicle at the
    // tail), standing at `at`: the campaign's battery when it moves (G9.5).
    static std::vector<MarchVehicle> BatteryColumn(const MapPoint& at) {
        const struct {
            const char* id;
            BatteryVehicleKind kind;
        } order[] = {{"20", BatteryVehicleKind::CommandPost}, {"21", BatteryVehicleKind::Launcher}, {"22", BatteryVehicleKind::Launcher},
                     {"23", BatteryVehicleKind::Launcher},    {"24", BatteryVehicleKind::Launcher}, {"TZM-1", BatteryVehicleKind::Reload},
                     {"TZM-2", BatteryVehicleKind::Reload},   {"MTO", BatteryVehicleKind::Maintenance}};
        std::vector<MarchVehicle> out;
        for (const auto& v : order) {
            MarchVehicle m;
            m.def.id = v.id;
            m.def.callsign = v.id;
            m.def.kind = v.kind;
            m.def.start = Vec3{at.east_m, at.north_m, 0.0};
            out.push_back(m);
        }
        return out;
    }
    // The battery's march between two points on the roads, in hours from the order to move off to its head at the end
    // (G9.5: the campaign screen's relocation). < 0, with why, when there is no way.
    double MarchHours(const MapPoint& from, const MapPoint& to, const BatteryTactics& tactics, std::string* why = nullptr) const {
        MarchPlan p = PlanBetween(from, to, "the new position", BatteryColumn(from), tactics, 0.0, why);
        return p.found ? (p.end_s - p.move_off_s) / 3600.0 : -1.0;
    }

    // A mover between two anchors on the roads for its class. False, with why, if either is unknown or there is no way.
    bool AddMover(const SessionMoverDef& d, std::string* why = nullptr) {
        const Anchor* a = anchors_.Find(d.from);
        const Anchor* b = anchors_.Find(d.to);
        const Mobility* m = mobility_.Find(d.mobility);
        auto fail = [&](const std::string& w) {
            if (why != nullptr) *why = d.id + ": " + w;
            return false;
        };
        if (a == nullptr) return fail("no place called '" + d.from + "'");
        if (b == nullptr) return fail("no place called '" + d.to + "'");
        if (m == nullptr) return fail("no mobility class '" + d.mobility + "'");
        RouteRequest q;
        q.from = a->at;
        q.to = b->at;
        q.mobility = m;
        q.swim = false;
        Mover mv;
        mv.id = d.id;
        mv.kind = d.kind;
        mv.mobility = d.mobility;
        mv.route = graph_.Find(q);
        if (!mv.route.found) return fail("no way from " + d.from + " to " + d.to);
        mv.depart_s = d.depart_s;
        mv.vehicles = std::max(1, d.vehicles);
        mv.spacing_m = d.spacing_m;
        movers_.Add(mv);
        models_.push_back({d.id, d.model.empty() ? ModelForMobility(d.mobility) : d.model});
        return true;
    }
    void AddMover(const Mover& m, const std::string& model = "") {
        movers_.Add(m);
        models_.push_back({m.id, model.empty() ? ModelForMobility(m.mobility) : model});
    }
    Movers& GetMovers() { return movers_; }
    const Movers& GetMovers() const { return movers_; }

    // The roads as they are (roadmap 17.4): the crossings down in truth, from the session's hour `h`. A unit plans on what
    // it believes (its own overlay); a mover finds the truth only when it comes to the crossing. False for a crossing the
    // graph does not have, or one already so.
    bool SetCrossingDown(const std::string& crossing, bool down, double h, const std::string& entry, const std::string& fact = "") {
        if (graph_.Crossing(crossing) == nullptr) return false;
        if (down) {
            if (truth_.Down(crossing, h) != nullptr) return false;
            truth_.Cut(entry, crossing, h, 1e18, fact);
            return true;
        }
        bool opened = false;
        for (OverlayEntry& e : truth_.entries) {
            if (e.crossing == crossing && h >= e.from_h && h < e.until_h) {
                e.until_h = h;
                opened = true;
            }
        }
        return opened;
    }
    const Overlay& Truth() const { return truth_; }

    // To time t: the levels by the viewers and the range, then every mover on (the M2 ones by the session, at their route's
    // times stretch by stretch as the M1 ones go, so a hand-over keeps the place).
    void Step(double t, double dt, const std::vector<MapPoint>& viewers, double range_m) {
        range_now_m_ = range_m;
        movers_.UpdateLevels(viewers, range_m);
        movers_.Step(t, dt);
        for (const Mover& m : movers_.All()) {
            if (m.level != MoverLevel::M2) continue;
            Mover* w = movers_.Find(m.id);
            Movers::Advance(*w, t, dt);
        }
    }
    double RangeNow() const { return range_now_m_; }

    // One vehicle or person of a mover, for drawing.
    struct Seen {
        std::string mover;
        int ordinal = 0;  // the mover's place among the session's (fixed: with `index`, a drawn vehicle's id)
        int index = 0;    // 0: the head
        MapPoint at;
        double heading_rad = 0.0;
        std::string kind;
        std::string model;
        bool moving = false;
    };
    // Every vehicle of every mover at full detail (inside the range): the head on its place, each after it at the
    // spacing behind along the route (before the start, queued back along its first stretch).
    std::vector<Seen> Visible() const {
        std::vector<Seen> out;
        const std::vector<Mover>& all = movers_.All();
        for (std::size_t k = 0; k < all.size(); ++k) {
            const Mover& m = all[k];
            if (m.level != MoverLevel::M2 || m.route.path.size() < 2) continue;
            for (int i = 0; i < m.vehicles; ++i) {
                Seen s;
                s.mover = m.id;
                s.ordinal = static_cast<int>(k);
                s.index = i;
                s.kind = m.kind;
                s.model = ModelOf(m.id);
                s.moving = m.state == MoverState::Moving;
                const double arc = m.arc_m - i * m.spacing_m;
                if (arc >= 0.0) {
                    s.at = PointAlong(m.route, arc, &s.heading_rad);
                } else {
                    const MapPoint& p0 = m.route.path[0];
                    const MapPoint& p1 = m.route.path[1];
                    const double len = std::max(1e-6, std::hypot(p1.east_m - p0.east_m, p1.north_m - p0.north_m));
                    s.at = {p0.east_m + (p1.east_m - p0.east_m) / len * arc, p0.north_m + (p1.north_m - p0.north_m) / len * arc};
                    s.heading_rad = std::atan2(p1.east_m - p0.east_m, p1.north_m - p0.north_m);
                }
                out.push_back(s);
            }
        }
        return out;
    }

private:
    std::string ModelOf(const std::string& id) const {
        for (const auto& m : models_) {
            if (m.first == id) return m.second;
        }
        return std::string();
    }

    static constexpr double kGroundStepM = 100.0;  // the district's ground cells (the Kesmark range's at 100 m: test_nav.cpp)
    bool ready_ = false;
    CountryDef country_;
    MobilityLibrary mobility_;
    VisualRangeValues range_;
    RoadGraph graph_;
    Anchors anchors_;
    Movers movers_;
    Overlay truth_;
    std::vector<std::pair<std::string, std::string>> models_;
    double range_now_m_ = 0.0;
};

}  // namespace nav
}  // namespace simcore
