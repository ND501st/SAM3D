// The route service (simcore/nav/, docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.1, roadmap G1).
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/nav.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::nav;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct World {
    CountryDef country;
    MapDef district;
    MobilityLibrary mobility;
    Anchors anchors;
    RoadGraph graph;
};

const World& TheWorld() {
    static const World w = [] {
        World out;
        std::string text, e;
        if (!simcore::detail::ReadTextFile(kData + "/maps/pannonia.json", text) || !LoadCountryDefJson(text, out.country, &e)) {
            std::printf("    country: %s\n", e.c_str());
        }
        if (!simcore::detail::ReadTextFile(kData + "/maps/kesmark_range.json", text) || !LoadMapDefJson(text, out.district, &e)) {
            std::printf("    district: %s\n", e.c_str());
        }
        if (!LoadMobility(kData, "vehicles/mobility.json", out.mobility, &e)) std::printf("    %s\n", e.c_str());
        command::ChainData chain;
        if (!command::LoadChainData(kData, "command/chain_pannonia.json", chain, &e)) std::printf("    %s\n", e.c_str());
        airwar::TheatreDef theatre;
        if (!airwar::LoadTheatreFile(kData + "/air_war/theatre_pannonia_nation.json", theatre, &e)) std::printf("    %s\n", e.c_str());
        out.anchors.AddChain(chain);
        out.anchors.AddTheatre(theatre);
        out.anchors.AddCountry(out.country);
        out.graph = RoadGraph::ForCountry(out.country, &out.district);
        return out;
    }();
    return w;
}

Route Between(const std::string& from, const std::string& to, const std::string& mobility, const Overlay& overlay = Overlay(), bool swim = true) {
    const World& w = TheWorld();
    RouteRequest q;
    q.from = w.anchors.Find(from)->at;
    q.to = w.anchors.Find(to)->at;
    q.mobility = w.mobility.Find(mobility);
    q.swim = swim;
    return w.graph.Find(q, overlay);
}

std::string Lines(const Route& r) {
    std::string out;
    for (const RouteLeg& l : r.legs) out += (out.empty() ? "" : " > ") + (l.kind == "road" ? l.line_id : l.kind);
    return out;
}

}  // namespace

SC_TEST("nav: the mobility classes load with their sources; the loader refuses a class without a source or with no speed") {
    const World& w = TheWorld();
    const Mobility* osa = w.mobility.Find("osa_9a33");
    SC_EXPECT(osa != nullptr && w.mobility.classes.size() >= 6);
    if (osa != nullptr) {
        SC_EXPECT_NEAR(osa->RoadMps(Road::Main), 60.0 / 3.6, 1e-9);
        SC_EXPECT_NEAR(osa->RoadMps(Road::Track), 30.0 / 3.6, 1e-9);
        SC_EXPECT_NEAR(osa->cross_country_mps, 10.0 / 3.6, 1e-9);
        SC_EXPECT(osa->Swims() && !w.mobility.Find("truck")->Swims());
    }
    SC_EXPECT_NEAR(w.mobility.Find("foot")->cross_country_mps, 1.44, 0.01);
    auto one = [](const std::string& cls) {
        return R"({"mobility_id": "m", "source": "sam3d", "confidence": "estimate", "classes": [)" + cls + "]}";
    };
    MobilityLibrary out;
    std::string e;
    SC_EXPECT(LoadMobilityJson(one(R"({"id": "a", "name": "A", "main_kmh": 1, "secondary_kmh": 1, "track_kmh": 1, "cross_country_kmh": 1, "source": "sam3d", "confidence": "estimate"})"), out, &e));
    out = MobilityLibrary();
    SC_EXPECT(!LoadMobilityJson(one(R"({"id": "a", "name": "A", "main_kmh": 1, "secondary_kmh": 1, "track_kmh": 1, "cross_country_kmh": 1})"), out, &e) &&
              e.find("source") != std::string::npos);
    out = MobilityLibrary();
    SC_EXPECT(!LoadMobilityJson(one(R"({"id": "a", "name": "A", "main_kmh": 1, "secondary_kmh": 1, "cross_country_kmh": 1, "source": "sam3d", "confidence": "estimate"})"), out, &e) &&
              e.find("track_kmh") != std::string::npos);
}

SC_TEST("nav: the road graph is the map's: the country's crossings by their own names, the district's tracks joined to its roads") {
    const World& w = TheWorld();
    std::printf("    %d nodes, %d edges, %d crossings\n", static_cast<int>(w.graph.Nodes().size()), static_cast<int>(w.graph.Edges().size()),
                static_cast<int>(w.graph.Crossings().size()));
    std::string e;
    const std::shared_ptr<const CountryMap> country = LoadCountryMap(kData, "maps/pannonia.json", &e);
    SC_EXPECT(country != nullptr);
    if (country == nullptr) return;
    int roads = 0;
    for (const CountryCrossing& c : country->Crossings()) {
        if (c.net != CountryNet::Road) continue;
        ++roads;
        const NavCrossing* mine = w.graph.Crossing(c.id);
        SC_EXPECT(mine != nullptr);
        if (mine != nullptr) SC_EXPECT(std::hypot(mine->at.east_m - c.at.east_m, mine->at.north_m - c.at.north_m) < 1.0);
    }
    SC_EXPECT(roads > 0 && static_cast<int>(w.graph.Crossings().size()) >= roads);
    // The battery's track meets the road it leaves from.
    bool track = false;
    for (const NavEdge& ed : w.graph.Edges()) track = track || ed.line_id == "t_bty21";
    SC_EXPECT(track);
    const Route r = Between("bty21_cp", "rgt_cp", "osa_9a33");
    std::printf("    battery 21 to the regiment's CP: %s\n", Lines(r).c_str());
    SC_EXPECT(r.found && r.legs.front().line_id == "t_bty21" && r.legs.back().line_id == "t_rgt_cp");
}

SC_TEST("nav: battery 21's routes (G1): to the regiment's CP, the Horka siding, the division and the Rava dam, for each vehicle, with times") {
    const std::vector<std::string> classes = {"osa_9a33", "tzm_9t217", "pu12m", "truck", "motorcycle", "foot"};
    for (const std::string& to : {"rgt_cp", "tech_battery", "div_hq", "rava_dam"}) {
        std::string line = "    to " + to + ":";
        double osa = 0.0, foot = 0.0;
        for (const std::string& c : classes) {
            const Route r = Between("bty21_cp", to, c);
            SC_EXPECT(r.found);
            char buf[64];
            std::snprintf(buf, sizeof(buf), " %s %.0f min", c.c_str(), r.time_s / 60.0);
            line += buf;
            if (c == "osa_9a33") osa = r.time_s;
            if (c == "foot") foot = r.time_s;
            if (c == "osa_9a33" && to == "rava_dam") std::printf("    the dam by %s, %.1f km\n", Lines(r).c_str(), r.length_m / 1000.0);
        }
        std::printf("%s\n", line.c_str());
        SC_EXPECT(osa > 0.0 && foot > 5.0 * osa);
    }
    // A short way between the battery's own places goes straight across the field.
    const Route near = Between("bty21_cp", "bty21_v2", "osa_9a33");
    SC_EXPECT(near.found && near.length_m < 2000.0);
    // By night every speed by the class's factor.
    RouteRequest q;
    q.from = TheWorld().anchors.Find("bty21_cp")->at;
    q.to = TheWorld().anchors.Find("tech_battery")->at;
    q.mobility = TheWorld().mobility.Find("truck");
    const Route day = TheWorld().graph.Find(q);
    q.night = true;
    const Route night = TheWorld().graph.Find(q);
    SC_EXPECT(day.found && night.found && night.time_s > day.time_s * 1.3);
}

SC_TEST("nav: a bridge down (G1.5): the truck goes round, a unit not told plans over it, the Osa swims the river where the road met it") {
    const World& w = TheWorld();
    double d = 0.0;
    const NavCrossing* bridge = w.graph.NearestCrossing(w.anchors.Find("kesmark_road_bridge")->at, &d);
    SC_EXPECT(bridge != nullptr && d < 500.0);
    if (bridge == nullptr) return;
    std::printf("    the Kesmark road bridge is %s\n", bridge->id.c_str());
    const Route over = Between("bty21_cp", "tech_battery", "truck");
    SC_EXPECT(over.found && over.Uses(bridge->id));
    Overlay truth;
    truth.Cut("strike:1", bridge->id, 0.0, 1e18, "raid:7");
    const Route round = Between("bty21_cp", "tech_battery", "truck", truth);
    std::printf("    the truck over the bridge %.0f min, round it %.0f min (%s)\n", over.time_s / 60.0, round.time_s / 60.0, Lines(round).c_str());
    SC_EXPECT(round.found && !round.Uses(bridge->id) && round.time_s > over.time_s);
    SC_EXPECT(round.depends.size() == 1 && round.depends[0] == "strike:1");
    // Planned on what it believes: not told, it still plans over the bridge.
    const Route believed = Between("bty21_cp", "tech_battery", "truck", Overlay());
    SC_EXPECT(believed.Uses(bridge->id) && believed.Hash() == over.Hash());
    // Before it went down, the bridge stands.
    RouteRequest q;
    q.from = w.anchors.Find("bty21_cp")->at;
    q.to = w.anchors.Find("tech_battery")->at;
    q.mobility = w.mobility.Find("truck");
    Overlay later;
    later.Cut("strike:2", bridge->id, 10.0);
    q.h = 5.0;
    SC_EXPECT(w.graph.Find(q, later).Uses(bridge->id));
    // The launcher swims where the road met the river, if that is quicker than going round; told not to, it goes round.
    const Route swim = Between("bty21_cp", "tech_battery", "osa_9a33", truth);
    const Route dry = Between("bty21_cp", "tech_battery", "osa_9a33", truth, false);
    bool swam = false;
    for (const RouteLeg& l : swim.legs) swam = swam || l.kind == "swim";
    std::printf("    the launcher: %s %.0f min; kept dry %.0f min\n", swam ? "swims," : "goes round,", swim.time_s / 60.0, dry.time_s / 60.0);
    SC_EXPECT(swim.found && dry.found && !dry.Uses(bridge->id) && swim.time_s <= dry.time_s);
    SC_EXPECT(swam == swim.Uses(bridge->id));
}

SC_TEST("nav: a road moved in a fixture map changes the route with no other edit; the route's digest is the same on every compiler") {
    // Two roads from (0, 0) to (10 km, 0): one straight over a river, one bowed north round its source.
    auto line = [](const std::string& id, const std::string& cls, std::vector<MapPoint> pts) {
        MapLineDef l;
        l.id = id;
        l.line_class = cls;
        l.points = std::move(pts);
        return l;
    };
    MapLineDef river = line("river", "river", {{5000.0, -10000.0}, {5000.0, 2000.0}});
    river.width_m = 60.0;
    const MapLineDef straight = line("straight", "secondary", {{0.0, 0.0}, {10000.0, 0.0}});
    const MapLineDef bowed = line("bowed", "main", {{0.0, 0.0}, {5000.0, 4000.0}, {10000.0, 0.0}});
    const RoadGraph g = RoadGraph::Build({&straight, &bowed}, {river});
    SC_EXPECT(g.Crossings().size() == 1 && g.Crossings()[0].id == "straight@river");
    MobilityLibrary lib;
    std::string e;
    SC_EXPECT(LoadMobility(kData, "vehicles/mobility.json", lib, &e));
    RouteRequest q;
    q.from = {0.0, -200.0};
    q.to = {10000.0, -200.0};
    q.mobility = lib.Find("truck");
    const Route r = g.Find(q);
    // Hand-worked: 200 m off the road at 10 km/h each end (72 s each), 10 km at 50 km/h (720 s); speeds are whole
    // centimetres a second, so a few tenths either way.
    SC_EXPECT(r.found && r.Uses("straight@river"));
    SC_EXPECT_NEAR(r.time_s, 72.0 + 720.0 + 72.0, 0.5);
    std::printf("    the fixture's route: %.1f s, digest %llu\n", r.time_s, static_cast<unsigned long long>(r.Hash()));
    SC_EXPECT(r.Hash() == 6913594298189549214ull);  // MSVC's; GCC must agree
    // The straight road bent far south: the bowed main road is quicker now. Nothing else edited.
    const MapLineDef bent = line("straight", "secondary", {{0.0, 0.0}, {5000.0, -8000.0}, {10000.0, 0.0}});
    const RoadGraph g2 = RoadGraph::Build({&bent, &bowed}, {river});
    const Route r2 = g2.Find(q);
    bool round_north = false;
    for (const RouteLeg& l : r2.legs) round_north = round_north || l.line_id == "bowed";
    SC_EXPECT(r2.found && !r2.Uses("straight@river") && round_north);
    // The bridge down and no other way: a man on foot cannot swim it, but crosses by the bowed road round the source.
    Overlay cut;
    cut.Cut("x", "straight@river", 0.0);
    q.mobility = lib.Find("foot");
    const Route r3 = g.Find(q, cut);
    SC_EXPECT(r3.found && !r3.Uses("straight@river"));
}

SC_TEST("nav: anchors (G1.6's check): the battery's and the regiment's places stand within reach of a road; the rest counted and listed") {
    const World& w = TheWorld();
    int places = 0, far_places = 0, others = 0, far_others = 0;
    std::string far_list;
    for (const Anchor& a : w.anchors.All()) {
        const double d = w.graph.DistanceToRoad(a.at);
        const bool far = d > 3000.0;
        if (a.kind == "place") {
            ++places;
            far_places += far ? 1 : 0;
            if (far) far_list += " " + a.id;
        } else {
            ++others;
            far_others += far ? 1 : 0;
        }
    }
    std::printf("    %d places of the chain, %d beyond 3 km of a road%s; %d other anchors, %d beyond\n", places, far_places, far_list.c_str(),
                others, far_others);
    SC_EXPECT(places > 30);
    for (const std::string& id : {"bty21_cp", "bty21_v1", "bty21_v2", "bty21_v3", "bty21_v4", "rgt_cp", "rgt_rear", "tech_battery"}) {
        const Anchor* a = w.anchors.Find(id);
        SC_EXPECT(a != nullptr && w.graph.DistanceToRoad(a->at) < 3000.0);
    }
}

// ---------------------------------------------------------------------------------------------- G2: the march

namespace {

BatteryTactics Tactics() {
    BatteryTactics t;
    std::string e;
    if (!LoadBatteryTacticsFile(kData + "/command/battery_tactics.json", t, &e)) std::printf("    %s\n", e.c_str());
    return t;
}

// Battery 21's column in the textbook's order of march: the PU-12M, two launchers, a TZM, two launchers, a TZM, the
// maintenance vehicle; the launchers at their positions, the rest by the command post.
std::vector<MarchVehicle> Battery21(bool with_maintenance = true) {
    const World& w = TheWorld();
    auto at = [&](const std::string& id, double de, double dn) {
        const MapPoint p = w.anchors.Find(id)->at;
        return Vec3{p.east_m + de, p.north_m + dn, 0.0};
    };
    auto v = [](const std::string& id, BatteryVehicleKind kind, Vec3 start) {
        MarchVehicle m;
        m.def.id = id;
        m.def.callsign = id;
        m.def.kind = kind;
        m.def.start = start;
        return m;
    };
    std::vector<MarchVehicle> out = {
        v("21", BatteryVehicleKind::CommandPost, at("bty21_cp", 0.0, 0.0)),
        v("21-1", BatteryVehicleKind::Launcher, at("bty21_v1", 0.0, 0.0)),
        v("21-2", BatteryVehicleKind::Launcher, at("bty21_v2", 0.0, 0.0)),
        v("21-5", BatteryVehicleKind::Reload, at("bty21_cp", -150.0, -100.0)),
        v("21-3", BatteryVehicleKind::Launcher, at("bty21_v3", 0.0, 0.0)),
        v("21-4", BatteryVehicleKind::Launcher, at("bty21_v4", 0.0, 0.0)),
        v("21-6", BatteryVehicleKind::Reload, at("bty21_cp", -150.0, -200.0)),
    };
    if (with_maintenance) out.push_back(v("21-7", BatteryVehicleKind::Maintenance, at("bty21_cp", 100.0, -250.0)));
    return out;
}

MarchRequest MarchTo(const std::string& to, double start_s) {
    const World& w = TheWorld();
    MarchRequest q;
    q.from = w.anchors.Find("bty21_cp")->at;
    q.to = w.anchors.Find(to)->at;
    q.from_name = "battery 21's position";
    q.to_name = w.anchors.Find(to)->name;
    q.column = Battery21();
    q.start_s = start_s;
    return q;
}

const MarchPoint* Nth(const MarchPlan& p, const std::string& kind, int n = 0) {
    for (const MarchPoint& m : p.points) {
        if (m.kind == kind && n-- == 0) return &m;
    }
    return nullptr;
}

}  // namespace

SC_TEST("nav: battery 21's march plan to the Rava dam (G2.2): the start point, a halt after 3.5 h with a regulation point after it, the route cards") {
    const World& w = TheWorld();
    const BatteryTactics t = Tactics();
    const double kStartHour = 20.0;  // the session starts at 20:00; the head at the start point at 23:00
    const MarchPlan p = PlanMarch(w.graph, w.mobility, t, MarchTo("rava_dam", 3.0 * 3600.0), Overlay(), &w.anchors);
    SC_EXPECT(p.found);
    if (!p.found) return;
    for (const std::string& line : p.Document(kStartHour)) std::printf("    %s\n", line.c_str());
    for (const std::string& line : p.Card(4, kStartHour)) std::printf("    %s\n", line.c_str());
    // The column: eight vehicles 40 m apart, 280 m; it passes a point in 34 s at 30 km/h.
    SC_EXPECT_NEAR(p.column_m, 280.0, 1e-9);
    SC_EXPECT_NEAR(p.pass_s, 280.0 / t.march_speed_mps, 1e-6);
    // The start point: a landmark beyond the column's length, at its time.
    const MarchPoint* sp = Nth(p, "start");
    SC_EXPECT(sp != nullptr && sp->arc_m >= p.column_m && sp->name.find("crossroads") != std::string::npos);
    if (sp != nullptr) SC_EXPECT_NEAR(sp->head_s, 3.0 * 3600.0, 1e-6);
    SC_EXPECT(p.move_off_s < 3.0 * 3600.0);
    // Over 4 h of movement: one halt of 1 h after 3.5 h of it, and a regulation point after the halt.
    const MarchPoint* halt = Nth(p, "halt");
    const MarchPoint* reg = Nth(p, "regulation");
    SC_EXPECT(halt != nullptr && Nth(p, "halt", 1) == nullptr && reg != nullptr);
    if (halt == nullptr || reg == nullptr || sp == nullptr) return;
    SC_EXPECT_NEAR(halt->head_s - sp->head_s, t.halt_interval_s, 1.0);
    SC_EXPECT_NEAR(halt->halt_s, 3600.0, 1e-9);
    SC_EXPECT(reg->arc_m > halt->arc_m && reg->head_s > halt->head_s + halt->halt_s);
    // The end: the movement at the plan's speeds and the halt; on the roads at the march speed, the last way off the road
    // at the class's cross-country speed.
    const MarchPoint& end = p.points.back();
    SC_EXPECT(end.kind == "end" && end.head_s > sp->head_s + 4.0 * 3600.0);
    double moving = 0.0;
    for (std::size_t i = 0; i < p.stretch_mps.size(); ++i) {
        const double len = p.arcs_m[i + 1] - p.arcs_m[i];
        if (p.arcs_m[i] >= sp->arc_m) moving += len / p.stretch_mps[i];
        SC_EXPECT(p.stretch_mps[i] <= t.march_speed_mps + 1e-9);
    }
    SC_EXPECT_NEAR(end.head_s - sp->head_s, moving + 3600.0, 2.0);
    // The route cards: the head's times are the plan's, each vehicle behind by its distance at the speed there.
    SC_EXPECT(p.cards.size() == 8 && p.cards[0].place == 1 && p.cards[7].callsign == "21-7");
    SC_EXPECT_NEAR(p.cards[0].at_s[0], sp->head_s, 1e-6);
    SC_EXPECT(p.cards[7].at_s[0] > p.cards[0].at_s[0] && p.cards[7].at_s[0] <= sp->tail_s + 1e-6);
    // A march of half an hour has no halt: the start point and the end only.
    const MarchPlan near = PlanMarch(w.graph, w.mobility, t, MarchTo("rgt_cp", 3600.0), Overlay(), &w.anchors);
    SC_EXPECT(near.found && near.points.size() == 2 && near.points.front().kind == "start" && near.points.back().kind == "end");
    // By night the column is slower by the class's night factor, and the plan longer.
    MarchRequest q = MarchTo("rava_dam", 3.0 * 3600.0);
    q.night = true;
    const MarchPlan night = PlanMarch(w.graph, w.mobility, t, q, Overlay(), &w.anchors);
    SC_EXPECT(night.found && night.speed_mps < p.speed_mps && night.end_s > p.end_s);
    std::printf("    by day the head arrives %s, by night %s\n", ClockHhMm(p.end_s, kStartHour).c_str(), ClockHhMm(night.end_s, kStartHour).c_str());
}

SC_TEST("nav: battery 21 marches to the Rava dam on the roads in its plan's times, the radiotelephonist writing each point (G2's done-when)") {
    const World& w = TheWorld();
    const double kStartHour = 20.0;
    ColumnMarch march(w.graph, w.mobility, Tactics(), &w.anchors);
    MarchRequest q = MarchTo("rava_dam", 3.0 * 3600.0);
    q.leave_s = 300.0;  // out of the emplacements first
    SC_EXPECT(march.Start(q));
    const MarchPlan plan = march.Plan();
    march.Run(0.0, plan.end_s + 900.0);
    SC_EXPECT(march.Arrived());
    for (const std::string& line : march.Journal().Page(plan.end_s + 900.0, 12)) std::printf("    %s\n", line.c_str());
    for (const std::string& line : march.Record(plan.end_s + 900.0, kStartHour)) std::printf("    %s\n", line.c_str());
    // Each point of the plan passed, and written, within two minutes of its time.
    const std::vector<Battery::Passing>& passed = march.Column().Passings();
    SC_EXPECT(passed.size() == plan.points.size());
    const std::vector<RadioJournalEntry> page = march.Journal().Legible(plan.end_s + 900.0);
    SC_EXPECT(page.size() == plan.points.size());
    for (std::size_t i = 0; i < passed.size() && i < plan.points.size(); ++i) {
        std::printf("    %s planned %.1f passed %.1f, tail %.1f planned %.1f\n", plan.points[i].kind.c_str(), plan.points[i].head_s, passed[i].head_s, passed[i].tail_s, plan.points[i].tail_s);
        SC_EXPECT(passed[i].head_s >= plan.points[i].head_s - 1e-6);  // never early: each point holds the head to its time
        SC_EXPECT_NEAR(passed[i].head_s, plan.points[i].head_s, 120.0);
        SC_EXPECT(passed[i].tail_s >= passed[i].head_s);
        if (i < page.size()) {
            SC_EXPECT_NEAR(page[i].heard_s, passed[i].head_s, 1e-6);
            SC_EXPECT(page[i].text.find(plan.points[i].name) != std::string::npos);
        }
    }
    // Every vehicle in its place behind the head at the end.
    const Battery& b = march.Column();
    for (std::size_t k = 0; k < b.Vehicles().size(); ++k) {
        const Vec3 at = b.Vehicles()[k].position;
        const Vec3 goal = b.Vehicles()[k].goal;
        SC_EXPECT(std::hypot(at.x - goal.x, at.y - goal.y) < 20.0);
    }
}

SC_TEST("nav: a column meets a bridge down that its commander was not told of (G2.7): it stops, he reports, a new route turns it about") {
    const World& w = TheWorld();
    const double kStartHour = 20.0;
    double d = 0.0;
    const NavCrossing* bridge = w.graph.NearestCrossing(w.anchors.Find("kesmark_road_bridge")->at, &d);
    SC_EXPECT(bridge != nullptr);
    if (bridge == nullptr) return;
    // The maintenance vehicle does not swim, so neither does the column.
    ColumnMarch march(w.graph, w.mobility, Tactics(), &w.anchors);
    Overlay truth;
    truth.Cut("strike:1", bridge->id, 0.0, 1e18, "raid:7");
    march.SetTruth(truth);
    SC_EXPECT(march.Start(MarchTo("tech_battery", 1200.0)));
    const MarchPlan first = march.Plan();
    SC_EXPECT(first.found && first.route.Uses(bridge->id) && !first.column_class.Swims());
    march.Run(0.0, first.end_s + 1800.0);
    march.Run(first.end_s + 1800.0, march.Plan().end_s + 900.0);
    for (const std::string& line : march.Journal().Page(march.Plan().end_s + 900.0, 14)) std::printf("    %s\n", line.c_str());
    SC_EXPECT(march.Plans().size() == 2);
    SC_EXPECT(march.Reports().size() == 1 && march.Reports()[0].text.find(bridge->id) != std::string::npos);
    SC_EXPECT(march.Believed().Down(bridge->id, 1.0) != nullptr);
    if (march.Plans().size() < 2 || march.Reports().empty()) return;
    const MarchPlan& second = march.Plan();
    SC_EXPECT(second.found && !second.route.Uses(bridge->id));
    // Stopped short of the bridge: the report's time is before the first plan had the head at the end.
    SC_EXPECT(march.Reports()[0].t < first.end_s);
    // Turned about: the new order of march is the old one reversed, the maintenance vehicle leading.
    SC_EXPECT(second.request.column.front().def.id == "21-7" && second.request.column.back().def.id == "21");
    SC_EXPECT(march.Arrived() && second.end_s > first.end_s + 1800.0);
    std::printf("    planned over the bridge to arrive %s; stopped %s; round it, arrived %s\n", ClockHhMm(first.end_s, kStartHour).c_str(),
                ClockHhMm(march.Reports()[0].t, kStartHour).c_str(), ClockHhMm(second.end_s, kStartHour).c_str());
    // A column of swimmers told of the bridge before it plans swims the river where the road met it.
    ColumnMarch swimmers(w.graph, w.mobility, Tactics(), &w.anchors);
    swimmers.Tell(truth.entries[0]);
    MarchRequest q = MarchTo("tech_battery", 1200.0);
    q.column = Battery21(false);
    SC_EXPECT(swimmers.Start(q));
    const MarchPoint* swim = Nth(swimmers.Plan(), "swim");
    SC_EXPECT(swim != nullptr && swim->halt_s > 0.0);
    swimmers.Run(0.0, swimmers.Plan().end_s + 900.0);
    SC_EXPECT(swimmers.Arrived() && swimmers.Plans().size() == 1);
}

// ---------------------------------------------------------------------------------------------- G10: orders by a means

SC_TEST("nav: battery 21 marches on orders carried by a means (G10.2-G10.5's done-when): 21-3's set is off, a runner reaches it, it catches the column up; every report in the journal") {
    const World& w = TheWorld();
    const double kStartHour = 20.0;
    std::string e;
    command::BatteryOrdersDef def;
    SC_EXPECT(command::LoadBatteryOrders(kData, def, &e));
    command::ChainData chain;
    SC_EXPECT(command::LoadChainData(kData, "command/chain_pannonia.json", chain, &e));
    SkillTable skills;
    SC_EXPECT(LoadSkillTableFile(kData + "/crew/skill_levels.json", skills, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    // The vehicle commanders of the chain (21-6 and 21-7 have no post of their own there: class 2's times).
    const std::vector<std::pair<std::string, std::string>> posts = {
        {"21-1", "veh21_1_commander"}, {"21-2", "veh21_2_commander"}, {"21-5", "tzm21_chief"}, {"21-3", "veh21_3_commander"},
        {"21-4", "veh21_4_commander"}, {"21-6", "veh21_6_commander"}, {"21-7", "veh21_7_commander"}};
    std::vector<command::OrderVehicle> crews;
    for (const auto& vp : posts) crews.push_back(command::CommanderFromChain(chain, skills, vp.first, vp.first, vp.second));
    SC_EXPECT(crews[3].commander == "Radek Cerny" && crews[3].reaction_s > crews[1].reaction_s);  // class 3 against class 1
    crews[3].set_on = false;  // 21-3's commander has his set off: he misses everything said on the net

    ColumnMarch march(w.graph, w.mobility, Tactics(), &w.anchors);
    march.UseOrders(def, "21", "bty21_commander", crews);
    MarchRequest q = MarchTo("rava_dam", 3.0 * 3600.0);
    q.leave_s = 300.0;
    SC_EXPECT(march.Start(q));
    const MarchPlan plan = march.Plan();
    const Vec3 start3 = march.Column().Find("21-3")->position;
    const double until = plan.end_s + 1800.0;
    double heard3 = -1.0, fell_in3 = -1.0;
    bool moved_early = false, column_went_on = false;
    for (double t = 0.0; t < until; t += 1.0) {
        march.Step(t, 1.0);
        const command::BatteryOrderNet* net = march.Orders();
        const Battery::Vehicle* v3 = march.Column().Find("21-3");
        if (net == nullptr || v3 == nullptr) break;
        const int mo = march.MarchOrder();
        const bool heard = mo >= 0 && net->Heard(mo, "21-3");
        if (heard && heard3 < 0.0) heard3 = t;
        // Until its commander has heard, 21-3 stays in its emplacement.
        if (!heard && std::hypot(v3->position.x - start3.x, v3->position.y - start3.y) > 1e-6) moved_early = true;
        // The column does not wait for it.
        if (!heard && march.Column().CurrentMode() == Battery::Mode::Marching) column_went_on = true;
        if (heard3 >= 0.0 && fell_in3 < 0.0 && !v3->catching && !v3->absent && t > heard3 + q.leave_s &&
            march.Column().CurrentMode() == Battery::Mode::Marching) {
            fell_in3 = t;
        }
    }
    const command::BatteryOrderNet* net = march.Orders();
    SC_EXPECT(net != nullptr && march.Arrived());
    if (net == nullptr) return;
    for (const command::OrderEvent& ev : march.OrderEvents()) {
        if (ev.vehicle == "21-3") std::printf("    %s %s: %s (%s) %s\n", ClockHhMm(ev.t, kStartHour).c_str(), ev.callsign.c_str(), ev.what.c_str(), ev.mean.c_str(), ev.text.c_str());
    }
    for (const std::string& line : net->Debrief(kStartHour)) std::printf("    %s\n", line.c_str());
    for (const std::string& line : march.Journal().Page(until, 40)) std::printf("    %s\n", line.c_str());
    std::printf("    21-3 heard %s, in its place in the column %s\n", ClockHhMm(heard3, kStartHour).c_str(), ClockHhMm(fell_in3, kStartHour).c_str());

    // «К маршу!» on the battery net at the move-off: every commander with his set on heard it then.
    const command::OrderBook& book = net->Book();
    SC_EXPECT(march.MarchOrder() == 0 && march.BattleOrder() == 1 && book.Orders().size() == 2);
    if (book.Orders().size() != 2) return;
    const command::BatteryOrderRecord& mo = book.Orders()[0];
    SC_EXPECT(mo.mean == "radio_voice" && mo.words == "К маршу!");
    SC_EXPECT_NEAR(mo.given_h, plan.move_off_s / 3600.0, 1.0 / 3600.0);
    for (const command::OrderLeg& l : mo.to) {
        SC_EXPECT(l.Has(command::OrderStep::Logged));  // every report on the page
        if (l.post == "veh21_3_commander") continue;
        SC_EXPECT(l.mean == "radio_voice" && l.sent.size() == 1);
        SC_EXPECT_NEAR(l.At(command::OrderStep::Heard), mo.given_h, 10.0 / 3600.0);
    }
    // 21-3: said twice more on the net, no line to ring, then a runner; he heard it from the runner, switched his set on,
    // and repeated back on the net once it could transmit.
    const command::OrderLeg* l3 = mo.For("veh21_3_commander");
    SC_EXPECT(l3 != nullptr);
    if (l3 == nullptr) return;
    SC_EXPECT(l3->sent.size() == 4 && l3->sent[1].second == "radio_voice" && l3->sent[2].second == "radio_voice" && l3->sent[3].second == "messenger");
    SC_EXPECT(l3->mean == "messenger");
    // The runner walks the route service's way on foot from the PU-12M to 21-3's position.
    RouteRequest walk;
    walk.from = w.anchors.Find("bty21_cp")->at;
    walk.to = w.anchors.Find("bty21_v3")->at;
    walk.mobility = w.mobility.Find("foot");
    const Route run = w.graph.Find(walk);
    SC_EXPECT(run.found);
    const double runner_s = def.runner_handling_s + run.time_s;
    SC_EXPECT_NEAR(l3->At(command::OrderStep::Heard) - mo.given_h, (3.0 * def.chase.wait_s + runner_s) / 3600.0, 30.0 / 3600.0);
    SC_EXPECT(l3->At(command::OrderStep::CarryingOut) > l3->At(command::OrderStep::Heard));
    const double warm_h = (def.set_warm_s + def.set_tx_s) / 3600.0;
    SC_EXPECT(l3->At(command::OrderStep::RepeatedBack) >= l3->At(command::OrderStep::Heard) + warm_h);
    SC_EXPECT(!moved_early && column_went_on);
    // It left its emplacement only once he had heard, and caught the column up long before the halt.
    SC_EXPECT(heard3 > plan.move_off_s && fell_in3 > heard3 + q.leave_s);
    const MarchPoint* halt = Nth(plan, "halt");
    SC_EXPECT(halt != nullptr && fell_in3 > 0.0 && fell_in3 < halt->head_s);

    // The journal: every vehicle's «К маршу готов», each point of the plan, «К бою!», every vehicle's «К бою готов».
    const std::vector<RadioJournalEntry> page = march.Journal().Legible(until);
    auto written = [&](const std::string& from, const std::string& text) {
        for (const RadioJournalEntry& en : page) {
            if (en.from == from && en.text.find(text) != std::string::npos) return true;
        }
        return false;
    };
    for (const auto& vp : posts) {
        SC_EXPECT(written(vp.first, "К маршу готов"));
        SC_EXPECT(written(vp.first, "К бою готов"));
    }
    for (const MarchPoint& p : plan.points) SC_EXPECT(written("Battery commander", p.name));
    SC_EXPECT(written("Battery commander", "К бою!"));
    // «К бою!» by flags down the column: every commander read it within a few passes, deployed, reported on the net.
    const command::BatteryOrderRecord& bo = book.Orders()[1];
    SC_EXPECT(bo.mean == "flags");
    for (const command::OrderLeg& l : bo.to) {
        SC_EXPECT(l.mean == "flags" && l.Has(command::OrderStep::Logged));
        SC_EXPECT(l.At(command::OrderStep::Heard) - bo.given_h < (def.say_s + 8.0 * def.pass_s + 1.0) / 3600.0);
    }
    for (const Battery::Vehicle& v : march.Column().Vehicles()) SC_EXPECT(v.deployed);
}

namespace {

// Whether a path touches `ground` anywhere: every 10 m along each stretch.
bool Crosses(const GroundGrid& grid, const Route& r, Ground ground) {
    for (std::size_t i = 0; i + 1 < r.path.size(); ++i) {
        const MapPoint& a = r.path[i];
        const MapPoint& b = r.path[i + 1];
        const double len = std::hypot(b.east_m - a.east_m, b.north_m - a.north_m);
        for (double d = 0.0; d <= len; d += 10.0) {
            const double f = len > 0.0 ? d / len : 0.0;
            if (grid.At({a.east_m + (b.east_m - a.east_m) * f, a.north_m + (b.north_m - a.north_m) * f}) == ground) return true;
        }
    }
    return false;
}

}  // namespace

SC_TEST("nav: the ground off the road (G1.3): a man on foot goes round the pond, a truck round the woods; without a grid, straight") {
    MobilityLibrary lib;
    std::string e;
    SC_EXPECT(LoadMobility(kData, "vehicles/mobility.json", lib, &e));
    const Mobility* foot = lib.Find("foot");
    const Mobility* truck = lib.Find("truck");
    SC_EXPECT(foot != nullptr && truck != nullptr);
    if (foot == nullptr || truck == nullptr) return;
    // The data's factors: a truck does not go into the marsh; a man is slower in the woods than in the open.
    SC_EXPECT(truck->GroundMps()[static_cast<std::size_t>(Ground::Marsh)] == 0.0);
    SC_EXPECT(foot->GroundMps()[static_cast<std::size_t>(Ground::Woods)] < foot->cross_country_mps);
    SC_EXPECT(foot->GroundMps()[static_cast<std::size_t>(Ground::Water)] == 0.0);
    // A 4 km square: a pond 1 km across in the middle, a wood north of it, a road along its south edge.
    MapDef map;
    map.west_m = -2000.0;
    map.east_m = 2000.0;
    map.south_m = -2000.0;
    map.north_m = 2000.0;
    MapLakeDef pond;
    pond.id = "pond";
    pond.points = {{-500.0, -500.0}, {500.0, -500.0}, {500.0, 500.0}, {-500.0, 500.0}};
    map.lakes.push_back(pond);
    MapPolygonDef wood;
    wood.id = "wood";
    wood.points = {{-300.0, 700.0}, {300.0, 700.0}, {300.0, 1300.0}, {-300.0, 1300.0}};
    map.forests.push_back(wood);
    MapLineDef road;
    road.id = "road";
    road.line_class = "secondary";
    road.points = {{-2000.0, -1800.0}, {2000.0, -1800.0}};
    map.roads.push_back(road);
    const GroundGrid grid = GroundGrid::FromMap(map, 50.0);
    SC_EXPECT(grid.Cols() == 80 && grid.Rows() == 80);
    SC_EXPECT(grid.At({0.0, 0.0}) == Ground::Water && grid.At({0.0, 1000.0}) == Ground::Woods && grid.At({-1000.0, 0.0}) == Ground::Open);
    SC_EXPECT(grid.Census()[static_cast<std::size_t>(Ground::Water)] == 400);      // the pond: 20 by 20 cells of 50 m
    SC_EXPECT(grid.Census()[static_cast<std::size_t>(Ground::Woods)] == 12 * 12);  // the wood: 600 m square
    RoadGraph g = RoadGraph::Build({&road}, {});
    RouteRequest q;
    q.from = {-1000.0, 0.0};
    q.to = {1000.0, 0.0};
    q.mobility = foot;
    // No grid (the country): straight across, through the pond as if it were not there.
    const Route straight = g.Find(q);
    SC_EXPECT(straight.found && straight.path.size() == 2);
    SC_EXPECT_NEAR(straight.time_s, 2000.0 / (std::round(foot->cross_country_mps * 100.0) / 100.0), 0.5);  // whole cm/s
    // With the district's ground: round the pond, a little longer, never in the water.
    g.SetGround(std::make_shared<GroundGrid>(grid));
    const Route round_pond = g.Find(q);
    std::printf("    on foot: straight %.0f s, round the pond %.0f s, %.0f m, %zu points\n", straight.time_s, round_pond.time_s, round_pond.length_m,
                round_pond.path.size());
    SC_EXPECT(round_pond.found && !Crosses(grid, round_pond, Ground::Water));
    SC_EXPECT(round_pond.time_s > straight.time_s && round_pond.time_s < 1.5 * straight.time_s);
    SC_EXPECT(round_pond.length_m > 1000.0 + 1000.0 * std::sqrt(2.0) - 30.0);  // at least round the pond's corners
    double sum_s = 0.0;
    for (const RouteStretch& s : round_pond.stretches) sum_s += s.time_s;
    SC_EXPECT_NEAR(sum_s, round_pond.time_s, 0.05);  // the stretches' times are the route's
    // A truck across the wood's line: round it (the woods at 0.3 of its speed), quicker than through.
    q.from = {-1000.0, 1000.0};
    q.to = {1000.0, 1000.0};
    q.mobility = truck;
    const Route round_wood = g.Find(q);
    const double v = truck->cross_country_mps;
    const double through_s = 1400.0 / v + 600.0 / (v * 0.3);
    std::printf("    the truck: through the wood %.0f s, round it %.0f s\n", through_s, round_wood.time_s);
    SC_EXPECT(round_wood.found && !Crosses(grid, round_wood, Ground::Woods));
    SC_EXPECT(round_wood.time_s < through_s && round_wood.time_s > 2000.0 / v);
}

SC_TEST("nav: the Kesmark range's ground (G1.3): its woods, marshes, water and towns laid at 100 m; battery 21's routes over them") {
    const World& w = TheWorld();
    const GroundGrid grid = GroundGrid::FromMap(w.district, 100.0);
    const auto n = grid.Census();
    std::printf("    %d by %d cells: open %d, woods %d, marsh %d, water %d, built-up %d\n", grid.Cols(), grid.Rows(), n[0], n[1], n[2], n[3], n[4]);
    for (int k : n) SC_EXPECT(k > 0);
    RoadGraph g = w.graph;
    g.SetGround(std::make_shared<GroundGrid>(grid));
    for (const std::string& to : {"bty21_v2", "rgt_cp", "tech_battery"}) {
        RouteRequest q;
        q.from = w.anchors.Find("bty21_cp")->at;
        q.to = w.anchors.Find(to)->at;
        q.mobility = w.mobility.Find("osa_9a33");
        const Route flat = w.graph.Find(q);
        const Route ground = g.Find(q);
        std::printf("    to %s: %.0f s straight off the road, %.0f s over the ground\n", to.c_str(), flat.time_s, ground.time_s);
        SC_EXPECT(flat.found && ground.found);
        SC_EXPECT(ground.time_s >= flat.time_s * 0.98);  // never much quicker than the open field (the grid's steps apart)
    }
}

SC_TEST("nav: movers on routes (G2.1): a column goes stretch by stretch at its route's times, halts, and keeps its place handed between M1 and M2") {
    Route r;
    r.found = true;
    r.path = {{0.0, 0.0}, {1000.0, 0.0}, {1000.0, 1000.0}};
    r.stretches = {{"road", Road::Main, "a", 1000.0, 100.0}, {"road", Road::Track, "b", 1000.0, 200.0}};
    r.time_s = 300.0;
    r.length_m = 2000.0;
    Movers movers;
    Mover col;
    col.id = "column";
    col.kind = "column";
    col.mobility = "osa_9a33";
    col.route = r;
    col.depart_s = 10.0;
    col.vehicles = 5;
    col.spacing_m = 50.0;
    movers.Add(col);
    double t = 0.0;
    auto run_to = [&](double until) {
        while (t < until - 1e-9) {
            t += 1.0;
            movers.Step(t, 1.0);
        }
    };
    const Mover* m = movers.Find("column");
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    run_to(60.0);
    SC_EXPECT(m->state == MoverState::Moving);
    SC_EXPECT_NEAR(m->arc_m, 500.0, 1e-6);  // 50 s at 10 m/s on the first stretch
    movers.Halt("column", 90.0);
    run_to(90.0);
    SC_EXPECT_NEAR(m->arc_m, 500.0, 1e-6);  // halted 30 s where it was
    run_to(210.0);
    SC_EXPECT_NEAR(m->arc_m, 1350.0, 1e-6);  // 170 s moving: the first stretch, then 70 s at 5 m/s
    const MapPoint head = movers.Head(*m), tail = movers.Tail(*m);
    SC_EXPECT_NEAR(head.north_m, 350.0, 1e-6);
    SC_EXPECT_NEAR(tail.north_m, 150.0, 1e-6);  // its 200 m behind
    // Seen: M2, and the session drives it; the place it hands back is the place it goes on from.
    std::vector<std::string> changed = movers.UpdateLevels({{1000.0, 600.0}}, 500.0);
    SC_EXPECT(m->level == MoverLevel::M2 && changed.size() == 1);
    movers.SetArc("column", 1600.0);
    movers.Step(t, 1.0);
    SC_EXPECT_NEAR(m->arc_m, 1600.0, 1e-6);  // M2: the session's
    // 520 m from the viewer: inside the margin, still M2; 1.5 km away: M1, at the route's time to its place.
    changed = movers.UpdateLevels({{1000.0, 1120.0}}, 500.0);
    SC_EXPECT(m->level == MoverLevel::M2 && changed.empty());
    changed = movers.UpdateLevels({{1000.0, 2130.0}}, 500.0);
    SC_EXPECT(m->level == MoverLevel::M1 && changed.size() == 1);
    SC_EXPECT_NEAR(m->moved_s, 220.0, 1e-6);
    run_to(300.0);
    SC_EXPECT(m->state == MoverState::Arrived);
    SC_EXPECT_NEAR(m->arrived_s, 290.0, 1e-6);  // 80 s on from where it was handed back
    SC_EXPECT_NEAR(m->arc_m, 2000.0, 1e-6);
    // The battery's own elements are M2 wherever the viewer is.
    movers.UpdateLevels({{50000.0, 50000.0}}, 500.0, {"column"});
    SC_EXPECT(m->level == MoverLevel::M2);
}
