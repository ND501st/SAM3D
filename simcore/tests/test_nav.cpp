// The route service (simcore/nav/, docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.1, roadmap G1).
#include <cmath>
#include <cstdio>
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
