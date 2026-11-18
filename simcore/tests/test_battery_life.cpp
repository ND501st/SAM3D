// The battery's life on the ground (roadmap track G, docs/campaign/11_BATTERY_LIFE_PROPOSAL.md): the pipeline's carriers
// on the roads (G2.5), the movement table and exposure on the march (G3), breakdowns (G3.5), the battery area (G4), the
// missiles from the rear (G5) and one TZM's run on the roads round a bridge down (17.4), the people and papers of the chain
// (G8), the march to a place in a session with its papers (G2.3, G9.4), the air war's supply columns as movers (G2.6),
// and the world's movers in sight (G11).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/battery.hpp"
#include "simcore/campaign.hpp"
#include "simcore/command.hpp"
#include "simcore/country_map.hpp"
#include "simcore/nav.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::nav;
using namespace simcore::campaign;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct NavWorld {
    CountryDef country;
    MapDef district;
    MobilityLibrary mobility;
    Anchors anchors;
    RoadGraph graph;
    command::ChainData chain;
    airwar::TheatreDef theatre;
};

const NavWorld& TheWorld() {
    static const NavWorld w = [] {
        NavWorld out;
        std::string text, e;
        if (!simcore::detail::ReadTextFile(kData + "/maps/pannonia.json", text) || !LoadCountryDefJson(text, out.country, &e)) {
            std::printf("    country: %s\n", e.c_str());
        }
        if (!simcore::detail::ReadTextFile(kData + "/maps/kesmark_range.json", text) || !LoadMapDefJson(text, out.district, &e)) {
            std::printf("    district: %s\n", e.c_str());
        }
        if (!LoadMobility(kData, "vehicles/mobility.json", out.mobility, &e)) std::printf("    %s\n", e.c_str());
        if (!command::LoadChainData(kData, "command/chain_pannonia.json", out.chain, &e)) std::printf("    %s\n", e.c_str());
        if (!airwar::LoadTheatreFile(kData + "/air_war/theatre_pannonia_nation.json", out.theatre, &e)) std::printf("    %s\n", e.c_str());
        out.anchors.AddChain(out.chain);
        out.anchors.AddTheatre(out.theatre);
        out.anchors.AddCountry(out.country);
        out.graph = RoadGraph::ForCountry(out.country, &out.district);
        return out;
    }();
    return w;
}

Route Between(const std::string& from, const std::string& to, const std::string& mobility, bool night = false) {
    const NavWorld& w = TheWorld();
    RouteRequest q;
    q.from = w.anchors.Find(from)->at;
    q.to = w.anchors.Find(to)->at;
    q.mobility = w.mobility.Find(mobility);
    q.night = night;
    q.swim = false;
    return w.graph.Find(q);
}

BatteryTactics Tactics() {
    BatteryTactics t;
    std::string e;
    if (!LoadBatteryTacticsFile(kData + "/command/battery_tactics.json", t, &e)) std::printf("    %s\n", e.c_str());
    return t;
}

// Battery 21's column as the nav tests have it: the PU-12M, the launchers and TZMs, the maintenance vehicle last.
MarchRequest MarchTo(const std::string& to, double start_s) {
    const NavWorld& w = TheWorld();
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
    MarchRequest q;
    q.from = w.anchors.Find("bty21_cp")->at;
    q.to = w.anchors.Find(to)->at;
    q.from_name = "battery 21's position";
    q.to_name = w.anchors.Find(to)->name;
    q.column = {v("21", BatteryVehicleKind::CommandPost, at("bty21_cp", 0.0, 0.0)), v("21-1", BatteryVehicleKind::Launcher, at("bty21_v1", 0.0, 0.0)),
                v("21-2", BatteryVehicleKind::Launcher, at("bty21_v2", 0.0, 0.0)), v("21-5", BatteryVehicleKind::Reload, at("bty21_cp", -150.0, -100.0)),
                v("21-3", BatteryVehicleKind::Launcher, at("bty21_v3", 0.0, 0.0)), v("21-4", BatteryVehicleKind::Launcher, at("bty21_v4", 0.0, 0.0)),
                v("21-6", BatteryVehicleKind::Reload, at("bty21_cp", -150.0, -200.0)), v("21-7", BatteryVehicleKind::Maintenance, at("bty21_cp", 100.0, -250.0))};
    q.start_s = start_s;
    return q;
}

}  // namespace

// ---------------------------------------------------------------------------------------------- G2.5

SC_TEST("battery life (G2.5): a runner, a despatch rider and the sergeant major in his truck take their routes' times on the roads") {
    const NavWorld& w = TheWorld();
    const double start = 6.0, h = 6.0;  // local noon: no night factor
    command::InfoNet net;
    SC_EXPECT(net.Init(w.chain, 1u, start));
    const Vec3 a = net.PlacePos(w.chain.PlaceIndex("bty21_cp"), h);
    const Vec3 b = net.PlacePos(w.chain.PlaceIndex("rgt_cp"), h);
    const double straight = net.CarrierTravelH("messenger", a, b, h);
    net.SetRouteTime(CarrierRoutes(std::make_shared<RoadGraph>(w.graph), std::make_shared<MobilityLibrary>(w.mobility)));
    for (const auto& m : std::vector<std::pair<std::string, std::string>>{{"messenger", "foot"}, {"despatch_rider", "motorcycle"}, {"sent_by_vehicle", "truck"}}) {
        RouteRequest q;
        q.from = {a.x, a.y};
        q.to = {b.x, b.y};
        q.mobility = w.mobility.Find(m.second);
        q.swim = false;
        const Route r = w.graph.Find(q);
        const double got = net.CarrierTravelH(m.first, a, b, h);
        std::printf("    %s (%s): %.0f min on the roads\n", m.first.c_str(), m.second.c_str(), got * 60.0);
        SC_EXPECT(r.found && std::fabs(got * 3600.0 - r.time_s) < 1.0);
    }
    std::printf("    the runner by the old straight-line rule: %.0f min\n", straight * 60.0);
    // A means that carries nothing on the roads is left to the net.
    SC_EXPECT(CarrierClass(command::MeanKind::FieldTelephone).empty());
}

// ---------------------------------------------------------------------------------------------- G3

SC_TEST("battery life (G3): two batteries ordered over the Kesmark road bridge cross it in priority order with the gap; the TZM run waits") {
    const NavWorld& w = TheWorld();
    const BatteryTactics t = Tactics();
    const MarchPlan plan = PlanMarch(w.graph, w.mobility, t, MarchTo("tech_battery", 1200.0), Overlay(), &w.anchors);
    SC_EXPECT(plan.found);
    double d = 0.0;
    const NavCrossing* bridge = w.graph.NearestCrossing(w.anchors.Find("kesmark_road_bridge")->at, &d);
    SC_EXPECT(bridge != nullptr);
    if (!plan.found || bridge == nullptr) return;
    const std::vector<RouteCrossing> on = CrossingsOn(w.graph, plan);
    bool uses = false;
    double bridge_arc = 0.0;
    for (const RouteCrossing& c : on) {
        if (c.id == bridge->id) {
            uses = true;
            bridge_arc = c.arc_m;
        }
    }
    SC_EXPECT(uses);
    const double head = PlanHeadAt(plan, bridge_arc);

    // Battery 22 asks first; battery 21 asks later for the same hour at a higher priority and is given it; 22 goes after
    // it, the gap between them.
    MovementTable table(300.0);
    table.Ask("bty22", bridge->id, head, 900.0, 1);
    MarchPlan ours = plan;
    const double waited = BookCrossings(w.graph, table, ours, "bty21", 2);
    const Slot* s21 = table.SlotOf("bty21", bridge->id);
    const Slot* s22 = table.SlotOf("bty22", bridge->id);
    SC_EXPECT(s21 != nullptr && s22 != nullptr);
    if (s21 == nullptr || s22 == nullptr) return;
    std::printf("    the bridge: 21 from %.0f to %.0f s, 22 moved to %.0f s; 21 waited %.0f s\n", s21->from_s, s21->until_s, s22->from_s, waited);
    SC_EXPECT(std::fabs(s21->from_s - head) < 1e-6 && waited < 1e-6);
    SC_EXPECT(std::fabs(s22->from_s - (s21->until_s + 300.0)) < 1e-6);
    SC_EXPECT(table.Changes().size() == 1 && table.Changes()[0].unit == "bty22" && table.Changes()[0].because == "bty21");

    // The TZM run, lowest priority, asks for the bridge in battery 21's hour: it goes after both.
    const double tzm = table.PassAt("tzm21_run", bridge->id, head + 60.0, 120.0, 0);
    SC_EXPECT(std::fabs(tzm - (table.SlotOf("bty22", bridge->id)->until_s + 300.0)) < 1e-6);

    // Battery 21 at a lower priority than 22 and asking after it: held at a regulator before the bridge, its times after
    // it later by the wait.
    MovementTable second(300.0);
    second.Ask("bty22", bridge->id, head - 100.0, 900.0, 2);
    MarchPlan held = plan;
    const double wait = BookCrossings(w.graph, second, held, "bty21", 1);
    const MarchPoint* reg = held.Point("regulator");
    std::printf("    behind 22: held %.0f s at the regulator; at the end %.0f s later\n", wait, held.end_s - plan.end_s);
    SC_EXPECT(reg != nullptr && std::fabs(wait - (800.0 + 300.0)) < 1.0);
    SC_EXPECT(reg != nullptr && std::fabs(reg->hold_s - wait) < 1e-6 && std::fabs(held.end_s - plan.end_s - wait) < 1.0);
}

SC_TEST("battery life (G3.4): the enemy's armed reconnaissance finds a column on the road more often by day than by night") {
    const NavWorld& w = TheWorld();
    const MarchPlan plan = PlanMarch(w.graph, w.mobility, Tactics(), MarchTo("tech_battery", 0.0), Overlay(), &w.anchors);
    SC_EXPECT(plan.found);
    if (!plan.found) return;
    airwar::TuningTable tune;
    std::string e;
    SC_EXPECT(airwar::LoadTuningFile(kData + "/gameplay/air_war.json", tune, &e));
    Movers movers;
    Mover col;
    col.id = "bty21";
    col.kind = "column";
    col.mobility = "osa_9a33";
    col.route = plan.route;
    col.vehicles = 8;
    col.spacing_m = plan.spacing_m;
    col.level = MoverLevel::M0;
    movers.Add(col);
    for (double t = 1.0; t <= 600.0; t += 1.0) movers.Step(t, 1.0);
    const MapPoint at = movers.Head(*movers.Find("bty21"));
    auto finds = [&](airwar::DayPart part, bool capable) {
        airwar::Rng rng(7);
        int n = 0;
        for (int k = 0; k < 400; ++k) n += ArmedRecceSees(movers, at, 3000.0, part, airwar::Weather::Clear, capable, tune, rng).empty() ? 0 : 1;
        return n;
    };
    const int day = finds(airwar::DayPart::Day, false), night = finds(airwar::DayPart::Night, false), capable = finds(airwar::DayPart::Night, true);
    std::printf("    found in 400 searches: by day %d, by night %d, by night with a night attack system %d\n", day, night, capable);
    SC_EXPECT(day > capable && capable > night && night > 0);
    // Far from the search, nothing.
    airwar::Rng rng(1);
    SC_EXPECT(ArmedRecceSees(movers, {at.east_m + 50000.0, at.north_m}, 3000.0, airwar::DayPart::Day, airwar::Weather::Clear, false, tune, rng).empty());
}

// ---------------------------------------------------------------------------------------------- G3.5

SC_TEST("battery life (G3.5): a vehicle breaks down on the march; the maintenance vehicle stays and repairs it; both catch the column up") {
    BatteryTactics tactics = Tactics();
    tactics.breakdown_per_100km = 1.0;
    tactics.repair_s = 600.0;
    std::vector<BatteryVehicleDef> defs;
    auto add = [&defs](const char* id, BatteryVehicleKind k, double n) {
        BatteryVehicleDef d;
        d.id = id;
        d.callsign = id;
        d.kind = k;
        d.start = Vec3{0.0, n, 0.0};
        defs.push_back(d);
    };
    add("21", BatteryVehicleKind::Launcher, 0.0);
    add("22", BatteryVehicleKind::Launcher, -60.0);
    add("23", BatteryVehicleKind::Launcher, -120.0);
    add("24", BatteryVehicleKind::Launcher, -180.0);
    add("TZM", BatteryVehicleKind::Reload, -240.0);
    add("MTO", BatteryVehicleKind::Maintenance, -300.0);
    BatteryOrder o;
    o.kind = BatteryOrder::Kind::March;
    o.route = {Vec3{0.0, 0.0, 0.0}, Vec3{0.0, 120000.0, 0.0}};
    bool shown = false;
    int seeds_with_fixer = 0;
    for (std::uint64_t seed = 1; seed <= 12; ++seed) {
        Battery b(tactics, defs);
        b.SetDriveOwn(true);
        b.SetBreakdownSeed(seed);
        b.Order(o, 0.0);
        std::vector<Battery::Event> ev;
        std::string fixed;
        double came = -1.0, repaired = -1.0, north_then = 0.0;
        for (double t = 0.0; t < 7200.0; t += 0.5) {
            b.Step(t + 0.5, 0.5);
            for (const Battery::Event& x : b.TakeEvents()) {
                ev.push_back(x);
                if (x.who == "MTO" && x.text.rfind("stops with ", 0) == 0 && fixed.empty()) {
                    fixed = x.text.substr(11, x.text.find(' ', 11) - 11);
                    came = x.t;
                }
                if (!fixed.empty() && x.who == fixed && x.text.rfind("repaired", 0) == 0 && repaired < 0.0) {
                    repaired = x.t;
                    north_then = b.Find(fixed)->position.y;
                }
            }
        }
        if (fixed.empty() || repaired < 0.0) continue;
        ++seeds_with_fixer;
        SC_EXPECT(std::fabs(repaired - came - tactics.repair_s) < 1.0);
        if (!shown) {
            shown = true;
            for (const Battery::Event& x : ev) {
                if (x.text.find("fault") != std::string::npos || x.text.find("repair") != std::string::npos) {
                    std::printf("    seed %d, %5.0f s %s: %s\n", static_cast<int>(seed), x.t, x.who.c_str(), x.text.c_str());
                }
            }
        }
        // Repaired, it drives on after the column (unless it broke down again).
        const Battery::Vehicle* v = b.Find(fixed);
        SC_EXPECT(v != nullptr && (v->broken || v->position.y > north_then + 1000.0));
    }
    std::printf("    the maintenance vehicle stayed and repaired with %d of 12 seeds\n", seeds_with_fixer);
    SC_EXPECT(seeds_with_fixer >= 3);
    // No chance, no breakdowns.
    Battery calm(Tactics(), defs);
    calm.SetDriveOwn(true);
    calm.Order(o, 0.0);
    for (double t = 0.0; t < 3600.0; t += 0.5) calm.Step(t + 0.5, 0.5);
    for (const Battery::Vehicle& v : calm.Vehicles()) SC_EXPECT(!v.broken);
}

// ---------------------------------------------------------------------------------------------- G4

SC_TEST("battery life (G4): a battery area laid at every Osa battery's prepared position keeps the textbook's rules on the ground") {
    const NavWorld& w = TheWorld();
    BatteryAreaRules rules;
    std::string e;
    SC_EXPECT(LoadBatteryAreaRules(kData, "campaign/battery_area.json", rules, &e));
    std::vector<GroundGrid::Disc> towns;
    for (const CountrySettlementDef& s : w.country.settlements) towns.push_back({s.centre, s.radius_m});
    int laid = 0, failed = 0;
    for (const airwar::TheatreObject& o : w.theatre.objects) {
        if (o.kind != airwar::ObjectKind::SamSite || o.id.rfind("osa_", 0) != 0) continue;
        std::vector<Vec3> at = o.prepared;
        if (at.empty()) at.push_back(o.pos);
        for (std::size_t k = 0; k < at.size(); ++k) {
            const MapPoint c{at[k].x, at[k].y};
            const GroundGrid grid = GroundGrid::FromBox(w.country.ground, c.east_m - 4000.0, c.north_m - 4000.0, c.east_m + 4000.0, c.north_m + 4000.0, 50.0, towns);
            const double threat = std::atan2(-c.east_m, -c.north_m);  // toward the capital: a placeholder threat axis
            const BatteryArea area = LayOutBatteryArea(o.id + "#" + std::to_string(k), c, threat, rules, &grid);
            const std::vector<std::string> wrong = CheckBatteryArea(area, rules, &grid, &w.mobility);
            ++laid;
            if (!wrong.empty()) {
                ++failed;
                std::printf("    %s: %s\n", area.id.c_str(), wrong.front().c_str());
                for (const AreaPlace& p : area.places) {
                    std::printf("      %s at %.0f %.0f (%s; ideal %.0f %.0f)\n", p.id.c_str(), p.at.east_m - c.east_m, p.at.north_m - c.north_m,
                                GroundName(p.ground), p.ideal.east_m - c.east_m, p.ideal.north_m - c.north_m);
                }
            }
            SC_EXPECT(area.ok && wrong.empty());
        }
    }
    std::printf("    %d battery areas laid, %d against the rules\n", laid, failed);
    SC_EXPECT(laid >= 9);

    // The work at its places: an emplacement dug half keeps what was done when the crew is called away.
    const MapPoint c{0.0, 0.0};
    const BatteryArea area = LayOutBatteryArea("open", c, 0.0, rules, nullptr);
    SC_EXPECT(area.ok && area.places.size() >= 14);
    JobBoard jobs;
    jobs.ForArea(area, rules);
    SC_EXPECT(jobs.Find("dig_launcher_1") != nullptr && jobs.Find("nets_tzm_a") != nullptr);
    SC_EXPECT(std::fabs(jobs.Work("dig_launcher_1", 1.5) - 1.5) < 1e-9);
    SC_EXPECT(!jobs.Find("dig_launcher_1")->Done() && std::fabs(jobs.Find("dig_launcher_1")->Left() - (rules.dig_emplacement_h - 1.5)) < 1e-9);
    SC_EXPECT(std::fabs(jobs.Work("dig_launcher_1", 5.0) - (rules.dig_emplacement_h - 1.5)) < 1e-9 && jobs.Find("dig_launcher_1")->Done());
    // Each place an anchor the route service finds by id.
    Anchors anchors;
    area.AddAnchors(anchors);
    SC_EXPECT(anchors.Find("open/launcher_3") != nullptr);
}

// ---------------------------------------------------------------------------------------------- G5

SC_TEST("battery life (G5): twelve fired at dawn; the TZMs bring twelve back in the test rate's and the road's time; a ledger wrong asks wrong") {
    LogisticsValues v;
    std::string e;
    SC_EXPECT(LoadLogisticsValues(kData, "campaign/logistics.json", v, &e));
    const Route road = Between("bty21_cp", "tech_battery", "tzm_9t217");
    SC_EXPECT(road.found);
    const std::vector<std::pair<std::string, std::string>> tzms = {{"tzm_a", "tzm21_chief"}, {"tzm_b", "tzm21_2_chief"}};
    BatteryStocks s;
    s.Init(v, 4, tzms, 0, 24, road);
    s.Fire(0.0, 0, 6);
    s.Fire(0.0, 1, 6);
    s.Logged(30.0, 0, 6);
    s.Logged(30.0, 1, 6);
    for (double t = 0.0; t <= 6.0 * 3600.0; t += 10.0) s.Step(t);
    SC_EXPECT(s.Requests().size() == 1 && s.Requests()[0].missiles == 12);
    const BatteryStocks::Tzm& b = s.Tzms()[1];
    const double expect = 12.0 * 3600.0 / v.tests_per_h + v.load_tzm_s_per_missile + road.time_s;
    std::printf("    the road %.0f min; tzm_b back at %.0f s (the test rate and the road: %.0f s); the battery holds %d\n", road.time_s / 60.0, b.back_s, expect,
                s.TrueTotal());
    SC_EXPECT(std::fabs(b.back_s - expect) < 40.0);
    SC_EXPECT(s.TrueTotal() == 48 && s.GetLedger().Total() == 48);
    // The run is in the order book, carried out and reported by each TZM's commander.
    SC_EXPECT(s.Orders().Orders().size() == 1);
    // On the road, the TZMs are movers.
    BatteryStocks mid;
    mid.Init(v, 4, tzms, 0, 24, road);
    mid.Fire(0.0, 0, 6);
    mid.Logged(0.0, 0, 6);
    mid.Fire(0.0, 1, 6);
    mid.Logged(0.0, 1, 6);
    for (double t = 0.0; t <= 1800.0 + road.time_s * 0.5; t += 10.0) mid.Step(t);
    SC_EXPECT(mid.Movers(1800.0 + road.time_s * 0.5).size() == 2);

    // The rear's train by rail to the Horka siding: with the line's bridge down it goes round, or not at all.
    std::string ce;
    const std::shared_ptr<const CountryMap> country = LoadCountryMap(kData, "maps/pannonia.json", &ce);
    SC_EXPECT(country != nullptr);
    if (country != nullptr) {
        const CountryNetwork& rail = country->Network(CountryNet::Rail);
        const MapPoint cegled{-35000.0, 15000.0}, horka{19400.0, -14200.0};
        const CountryRoute whole = rail.Route(cegled, horka);
        SC_EXPECT(whole.found && !whole.crossings.empty());
        if (whole.found && !whole.crossings.empty()) {
            const CountryRoute cut = rail.Route(cegled, horka, {whole.crossings.front()});
            const double kmh = v.train_kmh / 3.6;
            BatteryStocks trains;
            trains.Init(v, 4, tzms, 0, 0, road);
            trains.TrainDue(0.0, whole.length_m / kmh);
            trains.TrainDue(0.0, cut.found ? cut.length_m / kmh : -1.0);
            for (double t = 0.0; t <= 24.0 * 3600.0; t += 60.0) trains.Step(t);
            std::printf("    the train: %.1f h by the line; with %s down %s\n", whole.length_m / kmh / 3600.0, whole.crossings.front().c_str(),
                        cut.found ? (std::to_string(cut.length_m / kmh / 3600.0) + " h round").c_str() : "no way through");
            SC_EXPECT(!cut.found || cut.length_m > whole.length_m);
            SC_EXPECT(trains.TrainsIn().size() == (cut.found && cut.length_m / kmh < 24.0 * 3600.0 ? 2u : 1u));
        }
    }

    // A ledger wrong: a launch logged twice. The battery commander asks for what his ledger says, not what is so.
    BatteryStocks wrong;
    wrong.Init(v, 4, tzms, 0, 24, road);
    wrong.Fire(0.0, 0, 6);
    wrong.Fire(0.0, 1, 6);
    wrong.Logged(0.0, 0, 6);
    wrong.Logged(0.0, 1, 6);
    wrong.Logged(0.0, 2, 1);
    wrong.Step(0.0);
    SC_EXPECT(wrong.Requests().size() == 1 && wrong.Requests()[0].missiles == 13 && wrong.TrueTotal() == 36);
}

// ---------------------------------------------------------------------------------------------- 17.4

namespace {

// Where a crossing is along a route (< 0: not on it).
double ArcOfCrossing(const RoadGraph& g, const Route& r, const std::string& crossing) {
    double arc = 0.0;
    for (std::size_t i = 0; i < r.path.size() && i < r.nodes.size(); ++i) {
        if (i > 0) arc += std::hypot(r.path[i].east_m - r.path[i - 1].east_m, r.path[i].north_m - r.path[i - 1].north_m);
        const int n = r.nodes[i];
        if (n < 0) continue;
        const int c = g.Nodes()[static_cast<std::size_t>(n)].crossing;
        if (c >= 0 && g.Crossings()[static_cast<std::size_t>(c)].id == crossing) return arc;
    }
    return -1.0;
}

// One TZM's run headless, every second: the mover at its route's times, then the run. What it saw on the way.
struct TzmTrace {
    Mover m;
    double stop_s = -1.0;  // first stopped short of a bridge
    double stop_arc_m = 0.0;
    Route stopped_on;      // the route he stopped on
    Route back;            // the route he took back
    bool moved_again = false;
};

TzmTrace RunTzm(TzmRun& run, TechnicalBattery& siding, const Overlay& told, const Overlay& truth) {
    const NavWorld& w = TheWorld();
    LogisticsValues v;
    std::string e;
    if (!LoadLogisticsValues(kData, "campaign/logistics.json", v, &e)) std::printf("    %s\n", e.c_str());
    TzmTrace tr;
    if (!run.Start(w.graph, *w.mobility.Find("tzm_9t217"), v, "TZM", w.anchors.Find("bty21_cp")->at, w.anchors.Find("tech_battery")->at, 0, told,
                   false, 0.0, tr.m, &e)) {
        std::printf("    cannot start: %s\n", e.c_str());
        return tr;
    }
    for (double t = 1.0; t <= 12.0 * 3600.0 && !run.Finished(); t += 1.0) {
        siding.Step(t);
        Movers::Advance(tr.m, t, 1.0);
        const TzmRun::Leg before = run.GetLeg();
        run.Step(t, w.graph, truth, siding, tr.m);
        if (run.GetLeg() == TzmRun::Leg::Stopped && tr.stop_s < 0.0) {
            tr.stop_s = t;
            tr.stop_arc_m = tr.m.arc_m;
            tr.stopped_on = tr.m.route;
        }
        if (before == TzmRun::Leg::Stopped && run.GetLeg() != TzmRun::Leg::Stopped) {
            tr.moved_again = t == tr.stop_s + 600.0 && tr.m.state == MoverState::Moving;
        }
        if (before != TzmRun::Leg::Back && run.GetLeg() == TzmRun::Leg::Back) tr.back = tr.m.route;
    }
    return tr;
}

}  // namespace

SC_TEST("battery life (17.4): the TZM's run on the roads: out, twelve at the crane's rate, back; told of the bridge down, it plans round from the start") {
    const Route out = Between("bty21_cp", "tech_battery", "tzm_9t217");
    const Route back = Between("tech_battery", "bty21_cp", "tzm_9t217");
    SC_EXPECT(out.found && back.found && out.Uses("b_ralje@rava"));
    LogisticsValues v;
    std::string e;
    SC_EXPECT(LoadLogisticsValues(kData, "campaign/logistics.json", v, &e));
    TechnicalBattery siding;
    siding.Init(v, 24, 0);
    TzmRun run;
    const TzmTrace tr = RunTzm(run, siding, Overlay(), Overlay());
    const double load_s = v.tzm_missiles * v.load_tzm_s_per_missile;
    std::printf("    the road %.1f min each way; at the siding at %.0f s, back at %.0f s (the roads and the crane: %.0f s)\n", out.time_s / 60.0,
                run.AtSidingS(), run.BackS(), out.time_s + load_s + back.time_s);
    SC_EXPECT(run.Home() && run.CannotWhy().empty() && run.Aboard() == 12 && siding.Tested() == 12);
    SC_EXPECT(std::fabs(run.AtSidingS() - run.LeftS() - out.time_s) < 2.0);
    SC_EXPECT(std::fabs(run.BackS() - run.LeftS() - (out.time_s + load_s + back.time_s)) < 2.0);
    SC_EXPECT(tr.stop_s < 0.0 && run.Found().empty());

    // Told the Kesmark road bridge is down, he plans round it from the start, both ways.
    Overlay told;
    told.Cut("wreck:kesmark_road_bridge:0", "b_ralje@rava", 0.0);
    const NavWorld& w = TheWorld();
    RouteRequest q;
    q.from = w.anchors.Find("bty21_cp")->at;
    q.to = w.anchors.Find("tech_battery")->at;
    q.mobility = w.mobility.Find("tzm_9t217");
    q.swim = false;
    const Route round_out = w.graph.Find(q, told);
    std::swap(q.from, q.to);
    const Route round_back = w.graph.Find(q, told);
    SC_EXPECT(round_out.found && !round_out.Uses("b_ralje@rava") && round_out.time_s > 4.0 * out.time_s);
    TechnicalBattery siding2;
    siding2.Init(v, 24, 0);
    TzmRun round;
    const TzmTrace tr2 = RunTzm(round, siding2, told, told);
    std::printf("    told of the bridge: round it %.0f min each way; back at %.0f s\n", round_out.time_s / 60.0, round.BackS());
    SC_EXPECT(!tr2.back.Uses("b_ralje@rava") && tr2.stop_s < 0.0);
    SC_EXPECT(round.Home() && round.Aboard() == 12);
    SC_EXPECT(std::fabs(round.BackS() - round.LeftS() - (round_out.time_s + load_s + round_back.time_s)) < 2.0);
    // Waiting for the station: with none tested, the crane loads as the station tests them.
    TechnicalBattery slow;
    slow.Init(v, 0, 24);
    TzmRun waits;
    RunTzm(waits, slow, Overlay(), Overlay());
    const double tested_by = 12.0 * 3600.0 / v.tests_per_h;
    std::printf("    none tested at the siding: back at %.0f s (twelve tested by %.0f s)\n", waits.BackS(), tested_by);
    SC_EXPECT(waits.Home() && waits.Aboard() == 12);
    SC_EXPECT(waits.BackS() > tested_by + back.time_s && waits.BackS() < tested_by + v.load_tzm_s_per_missile + back.time_s + 2.0);
}

SC_TEST("battery life (17.4): a bridge he was not told of: he stops short of it, plans again after ten minutes, and plans the way back round it") {
    const NavWorld& w = TheWorld();
    LogisticsValues v;
    std::string e;
    SC_EXPECT(LoadLogisticsValues(kData, "campaign/logistics.json", v, &e));
    Overlay truth;
    truth.Cut("wreck:kesmark_road_bridge:0", "b_ralje@rava", 0.0);
    TechnicalBattery siding;
    siding.Init(v, 24, 0);
    TzmRun run;
    const TzmTrace tr = RunTzm(run, siding, Overlay(), truth);
    const double bridge = ArcOfCrossing(w.graph, tr.stopped_on, "b_ralje@rava");
    const ColumnMarchSettings s;
    std::printf("    stopped at %.0f s, %.0f m short of the bridge; back at %.0f s\n", tr.stop_s, bridge - tr.stop_arc_m, run.BackS());
    SC_EXPECT(tr.stop_s > 0.0 && bridge > 0.0);
    SC_EXPECT(tr.stop_arc_m >= bridge - s.see_m - 1.0 && tr.stop_arc_m <= bridge - s.see_m + 25.0);
    SC_EXPECT(tr.moved_again);
    // His new route from where he stood, on what he now believes; the way back from the siding round it too.
    RouteRequest q;
    q.from = PointAlong(tr.stopped_on, tr.stop_arc_m);
    q.to = w.anchors.Find("tech_battery")->at;
    q.mobility = w.mobility.Find("tzm_9t217");
    q.h = (tr.stop_s + s.replan_s) / 3600.0;
    q.swim = false;
    const Route replan = w.graph.Find(q, truth);
    q.from = q.to;
    q.to = w.anchors.Find("bty21_cp")->at;
    const Route back = w.graph.Find(q, truth);
    SC_EXPECT(replan.found && !replan.Uses("b_ralje@rava") && back.found && !tr.back.Uses("b_ralje@rava"));
    SC_EXPECT(run.Believed().Down("b_ralje@rava", 1.0) != nullptr && run.Believed().Down("b_ralje@rava", 1.0)->id == "wreck:kesmark_road_bridge:0");
    SC_EXPECT(run.Found().size() == 1 && run.Home() && run.Aboard() == 12);
    const double expect = tr.stop_s + s.replan_s + replan.time_s + v.tzm_missiles * v.load_tzm_s_per_missile + back.time_s;
    SC_EXPECT(std::fabs(run.BackS() - run.LeftS() - expect) < 2.0);
}

SC_TEST("battery life (17.4): no way on: the bridge he finds is the last way over the river; he turns back and cannot") {
    const NavWorld& w = TheWorld();
    LogisticsValues v;
    std::string e;
    SC_EXPECT(LoadLogisticsValues(kData, "campaign/logistics.json", v, &e));
    // Told of every other way over: each crossing of each way round the Kesmark road bridge is down, until there is none.
    RouteRequest q;
    q.from = w.anchors.Find("bty21_cp")->at;
    q.to = w.anchors.Find("tech_battery")->at;
    q.mobility = w.mobility.Find("tzm_9t217");
    q.swim = false;
    Overlay told;
    Overlay truth;
    truth.Cut("wreck:kesmark_road_bridge:0", "b_ralje@rava", 0.0);
    int rounds = 0;
    for (Route r = w.graph.Find(q, truth); r.found && rounds < 20; r = w.graph.Find(q, truth), ++rounds) {
        for (const std::string& c : r.crossings) {
            told.Cut("told:" + c, c, 0.0);
            truth.Cut("told:" + c, c, 0.0);
        }
    }
    SC_EXPECT(!w.graph.Find(q, truth).found && w.graph.Find(q, told).Uses("b_ralje@rava"));
    TechnicalBattery siding;
    siding.Init(v, 24, 0);
    TzmRun run;
    const TzmTrace tr = RunTzm(run, siding, told, truth);
    std::printf("    %zu other crossings told down; stopped at %.0f s, back at %.0f s: %s\n", told.entries.size(), tr.stop_s, run.BackS(),
                run.CannotWhy().c_str());
    SC_EXPECT(tr.stop_s > 0.0 && run.Home() && run.Aboard() == 0 && siding.Tested() == 24);
    SC_EXPECT(run.CannotWhy() == "every way is cut" && run.AtSidingS() < 0.0);
    SC_EXPECT(run.Believed().Down("b_ralje@rava", 1.0) != nullptr);
    SC_EXPECT(!tr.back.Uses("b_ralje@rava") && tr.back.found);
}

SC_TEST("battery life (17.4): no way back: he waits short of the bridge he found and goes home once he sees it open again; an empty siding sends him back with what it had") {
    const NavWorld& w = TheWorld();
    LogisticsValues v;
    std::string e;
    SC_EXPECT(LoadLogisticsValues(kData, "campaign/logistics.json", v, &e));
    const Route out = Between("bty21_cp", "tech_battery", "tzm_9t217");
    const Route back = Between("tech_battery", "bty21_cp", "tzm_9t217");
    // Every way over the river but the Kesmark road bridge is down, and he was told so; the bridge falls once he is over it,
    // and stands open again at 03:00.
    RouteRequest q;
    q.from = w.anchors.Find("bty21_cp")->at;
    q.to = w.anchors.Find("tech_battery")->at;
    q.mobility = w.mobility.Find("tzm_9t217");
    q.swim = false;
    Overlay told;
    Overlay cut;
    cut.Cut("kesmark", "b_ralje@rava", 0.0);
    int rounds = 0;
    for (Route r = w.graph.Find(q, cut); r.found && rounds < 20; r = w.graph.Find(q, cut), ++rounds) {
        for (const std::string& c : r.crossings) {
            told.Cut("told:" + c, c, 0.0);
            cut.Cut("told:" + c, c, 0.0);
        }
    }
    Overlay truth = told;
    truth.Cut("wreck:kesmark_road_bridge:1", "b_ralje@rava", 1.0);
    const double open_s = 3.0 * 3600.0;
    TechnicalBattery siding;
    siding.Init(v, 24, 0);
    TzmRun run;
    Mover m;
    SC_EXPECT(run.Start(w.graph, *w.mobility.Find("tzm_9t217"), v, "TZM", q.from, q.to, 0, told, false, 0.0, m, &e));
    double stuck_s = -1.0;
    for (double t = 1.0; t <= 12.0 * 3600.0 && !run.Finished(); t += 1.0) {
        if (t >= open_s) truth.entries.back().until_h = open_s / 3600.0;
        siding.Step(t);
        Movers::Advance(m, t, 1.0);
        run.Step(t, w.graph, truth, siding, m);
        if (run.Stuck() && stuck_s < 0.0) stuck_s = t;
    }
    bool saw_open = false;
    for (const StockEvent& x : run.Events()) saw_open = saw_open || x.text.find("stands open again") != std::string::npos;
    const ColumnMarchSettings s;
    std::printf("    over the river by %zu crossings told down: the bridge falls behind him; no way back at %.0f s, back at %.0f s\n",
                told.entries.size(), stuck_s, run.BackS());
    SC_EXPECT(stuck_s > 3600.0 && stuck_s < open_s && saw_open);
    SC_EXPECT(run.Home() && !run.Stuck() && run.Aboard() == 12 && run.CannotWhy().empty());
    SC_EXPECT(run.BackS() > open_s && run.BackS() < open_s + s.replan_s + back.time_s);
    SC_EXPECT(run.Found().size() == 1 && run.Found()[0].until_h <= run.BackS() / 3600.0);

    // Five tested and none to test, no train due: he comes back with the five.
    TechnicalBattery few;
    few.Init(v, 5, 0);
    TzmRun few_run;
    RunTzm(few_run, few, Overlay(), Overlay());
    SC_EXPECT(few_run.Home() && few_run.Aboard() == 5 && few_run.SidingEmpty() && few.Tested() == 0);
    SC_EXPECT(std::fabs(few_run.BackS() - few_run.LeftS() - (out.time_s + 5.0 * v.load_tzm_s_per_missile + back.time_s)) < 2.0);
    // A train due: he waits for it and the station's tests.
    TechnicalBattery train;
    train.Init(v, 5, 0);
    train.TrainDue(0.0, 2.0 * 3600.0);
    TzmRun waits;
    RunTzm(waits, train, Overlay(), Overlay());
    std::printf("    five at the siding: back with %d at %.0f s; a train due at 7200 s: back with %d at %.0f s\n", few_run.Aboard(), few_run.BackS(),
                waits.Aboard(), waits.BackS());
    SC_EXPECT(waits.Home() && waits.Aboard() == 12 && !waits.SidingEmpty() && waits.BackS() > 7200.0 + back.time_s);
}

// ---------------------------------------------------------------------------------------------- G8

SC_TEST("battery life (G8): the battery commander finds the duty officer on his diagram and rings him; the wire cut, he sends the sergeant major") {
    const NavWorld& w = TheWorld();
    const command::ChainData& d = w.chain;
    const double start = 6.0, h = 6.0;
    auto report = [&](command::InfoNet& n) {
        command::Fact f;
        f.key = "report:test";
        f.kind = command::FactKind::BatteryNews;
        f.h = h;
        f.text = "Battery 21: two launchers short of missiles.";
        f.audience = "rgt_commander";
        f.need_level = 2;
        f.document = true;
        return n.Report(f);
    };
    const int me = d.PositionIndex("bty21_commander"), duty = d.PositionIndex("rgt_cp_duty");
    // The diagram: the people he deals with and the means he has to each.
    command::InfoNet net;
    SC_EXPECT(net.Init(d, 1u, start));
    net.SetPlayer("bty21_commander");
    bool on_diagram = false;
    for (const command::DiagramLine& l : command::CommsDiagram(net, "bty21_commander")) {
        if (l.post != "rgt_cp_duty") continue;
        on_diagram = true;
        SC_EXPECT(std::find(l.means.begin(), l.means.end(), "field_telephone") != l.means.end());
        SC_EXPECT(std::find(l.means.begin(), l.means.end(), "sent_by_vehicle") != l.means.end());
    }
    SC_EXPECT(on_diagram);
    const int fi = report(net);
    SC_EXPECT(fi >= 0);
    double phone = -1.0;
    for (const command::TellOption& o : net.TellOptions(me, duty, fi, h)) {
        if (o.mean == "field_telephone") phone = o.arrive_h;
    }
    SC_EXPECT(phone >= h && phone < h + 0.5);
    SC_EXPECT(net.Tell({h, me, duty, fi, "field_telephone"}));
    net.AdvanceTo(h + 1.0);
    SC_EXPECT(net.Known(duty, fi) != nullptr);

    // The wire cut and the radio jammed: the telephone is refused; the sergeant major goes in the truck with the paper,
    // and arrives in his route's time on the roads.
    command::InfoNet cut;
    cut.Init(d, 1u, start);
    cut.SetPlayer("bty21_commander");
    cut.CutWire("bty21_cp", h - 1.0, h + 6.0);
    cut.JamRadio(h - 1.0, h + 6.0);
    cut.SetRouteTime(CarrierRoutes(std::make_shared<RoadGraph>(w.graph), std::make_shared<MobilityLibrary>(w.mobility)));
    const int fc = report(cut);
    std::string why_phone;
    double by_truck = -1.0;
    for (const command::TellOption& o : cut.TellOptions(me, duty, fc, h)) {
        if (o.mean == "field_telephone") why_phone = o.why_not;
        if (o.mean == "sent_by_vehicle") by_truck = o.arrive_h;
    }
    SC_EXPECT(why_phone == "the line is cut");
    const Vec3 a = cut.PlacePos(d.PlaceIndex("bty21_cp"), h), b = cut.PlacePos(d.PlaceIndex("rgt_cp"), h);
    RouteRequest q;
    q.from = {a.x, a.y};
    q.to = {b.x, b.y};
    q.mobility = w.mobility.Find("truck");
    q.swim = false;
    const Route r = w.graph.Find(q);
    const double handling_h = d.FindMean("sent_by_vehicle")->handling_s / 3600.0;
    std::printf("    the telephone: at %.2f h; the wire cut: the truck at %.2f h (the road %.0f min)\n", phone, by_truck, r.time_s / 60.0);
    SC_EXPECT(r.found && std::fabs(by_truck - (h + handling_h + r.time_s / 3600.0)) < 1.0 / 3600.0);
    SC_EXPECT(cut.Tell({h, me, duty, fc, "sent_by_vehicle"}));
    cut.AdvanceTo(h + 3.0);
    const command::Delivery* got = cut.Known(duty, fc);
    SC_EXPECT(got != nullptr && got->full && std::fabs(got->h - by_truck) < 1.0 / 3600.0);

    // The staff list: 21-2's commander replaced after a raid is a new name on it only once the battery commander is told.
    command::InfoNet staff;
    staff.Init(d, 1u, start);
    staff.SetPlayer("bty21_commander");
    command::StaffList list = command::IssueStaffList(staff, "Battery 21", "bty21_commander", 0.0);
    const command::StaffLine* line = list.Line("veh21_2_commander");
    SC_EXPECT(line != nullptr && !line->name.empty());
    if (line == nullptr) return;
    const std::string was = line->name;
    command::PostChange c;
    c.position = "veh21_2_commander";
    c.h = h;
    c.holder.name = "Kovacs";
    c.holder.rank = "Junior Sergeant";
    c.why = "replaced";
    SC_EXPECT(staff.ChangeHands(c));
    command::CorrectStaffList(list, staff, h + 4.0);
    SC_EXPECT(list.Line("veh21_2_commander")->name == was);  // nobody has told him
    const int news = command::AddStaffNews(staff, c, "rgt_chief_of_staff", "bty21_commander");
    staff.AdvanceTo(h + 12.0);
    const command::Delivery* told = staff.Known(me, news);
    SC_EXPECT(told != nullptr);
    command::CorrectStaffList(list, staff, h + 12.0);
    SC_EXPECT(list.Line("veh21_2_commander")->name == "Kovacs");
    if (told != nullptr) {
        std::printf("    told at %.2f h:\n", told->h);
        for (const std::string& s : list.Text(start)) std::printf("    %s\n", s.c_str());
    }
    // The registry: one man per post at a time, the new one from the change.
    const command::PeopleRegistry people = command::PeopleRegistry::FromNet(staff);
    SC_EXPECT(people.HolderOf("veh21_2_commander", h - 1.0) != nullptr && people.HolderOf("veh21_2_commander", h - 1.0)->name == was);
    SC_EXPECT(people.HolderOf("veh21_2_commander", h + 1.0) != nullptr && people.HolderOf("veh21_2_commander", h + 1.0)->name == "Kovacs");
    SC_EXPECT(!people.At(staff, "bty21_cp", h).empty());
}

SC_TEST("battery life (G11): the visual range by the light and the weather; a convoy comes into sight at the range, moves on without a jump, keeps its ids") {
    const NavWorld& w = TheWorld();
    SessionNav s;
    std::string e;
    SC_EXPECT(s.Init(kData, "maps/pannonia.json", &w.district, "command/chain_pannonia.json", &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const VisualRangeValues& v = s.Range();
    SC_EXPECT_NEAR(VisualRangeM(40000.0, 30.0, v), 8000.0, 1e-9);
    SC_EXPECT_NEAR(VisualRangeM(40000.0, -3.0, v), 3000.0, 1e-9);
    SC_EXPECT_NEAR(VisualRangeM(40000.0, -12.0, v), 1500.0, 1e-9);
    SC_EXPECT_NEAR(VisualRangeM(500.0, 30.0, v), 500.0, 1e-9);  // fog: the weather's visibility the cap

    SessionMoverDef d;
    d.id = "rgt_supply";
    d.from = "rgt_rear";
    d.to = "bty21_cp";
    d.vehicles = 4;
    std::string why;
    SC_EXPECT(s.AddMover(d, &why));
    if (!why.empty()) std::printf("    %s\n", why.c_str());
    SessionMoverDef lost = d;
    lost.id = "lost";
    lost.to = "nowhere";
    SC_EXPECT(!s.AddMover(lost, &why) && why.find("nowhere") != std::string::npos);
    const MapPoint eye = s.GetAnchors().Find("bty21_cp")->at;
    const double range = VisualRangeM(40000.0, 30.0, v);
    double first_seen = -1.0, first_d = 0.0, worst_jump = 0.0;
    std::map<int, MapPoint> last;
    bool ids_kept = true, all_seen = false;
    const double dt = 1.0;
    for (double t = dt; t < 7200.0; t += dt) {
        s.Step(t, dt, {eye}, range);
        const std::vector<SessionNav::Seen> seen = s.Visible();
        if (!seen.empty() && first_seen < 0.0) {
            first_seen = t;
            first_d = 1e300;
            for (const SessionNav::Seen& x : seen) first_d = std::min(first_d, std::hypot(x.at.east_m - eye.east_m, x.at.north_m - eye.north_m));
        }
        all_seen = all_seen || seen.size() == 4;
        std::map<int, MapPoint> now;
        for (const SessionNav::Seen& x : seen) {
            const int id = x.ordinal * 1000 + x.index;
            ids_kept = ids_kept && x.mover == "rgt_supply" && x.model == "ural_truck" && x.index < 4;
            auto it = last.find(id);
            if (it != last.end()) worst_jump = std::max(worst_jump, std::hypot(x.at.east_m - it->second.east_m, x.at.north_m - it->second.north_m));
            now[id] = x.at;
        }
        last = now;
        if (s.GetMovers().Find("rgt_supply")->state == MoverState::Arrived) break;
    }
    const Mover* m = s.GetMovers().Find("rgt_supply");
    std::printf("    the convoy %.1f km by the roads in %.0f min; first seen at %.0f s, %.0f m off (the range %.0f m); worst step %.1f m\n",
                m->route.length_m / 1000.0, m->route.time_s / 60.0, first_seen, first_d, range, worst_jump);
    SC_EXPECT(m->state == MoverState::Arrived && m->level == MoverLevel::M2);
    SC_EXPECT(first_seen > 0.0 && first_d <= range + 1.0 && first_d > range - 200.0);  // at the edge of sight, not nearer
    SC_EXPECT(all_seen && ids_kept);
    SC_EXPECT(worst_jump < 40.0);  // a truck's metres in a second: no jump at the hand-over
}

SC_TEST("battery life (G2.6): the air war's supply columns as movers on their lines' roads, where its state has them") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_rebels.json", d, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    std::vector<airwar::ObjectState> truth;
    for (const airwar::TheatreObject& o : d.theatre.objects) {
        airwar::ObjectState st;
        st.id = o.id;
        st.pos = o.pos;
        st.missiles = o.defence.present ? o.defence.missiles : -1;
        truth.push_back(st);
    }
    airwar::SupplyNetwork net;
    net.Init(d, truth);
    for (double h = 0.0; h < 30.0; h += 1.0) {
        net.Step(d, truth, 1.0, airwar::DayPartAt(d.theatre.start_hour + h) != airwar::DayPart::Day, nullptr);
    }
    const std::vector<Mover> columns = SupplyColumnMovers(d, net, truth);
    int moving = 0, matched = 0;
    for (const Mover& m : columns) {
        const int i = d.theatre.IndexOf(m.id);
        SC_EXPECT(i >= 0);
        if (i < 0) continue;
        const airwar::ObjectState& st = truth[static_cast<std::size_t>(i)];
        const airwar::SupplyPath& p = net.Path(static_cast<std::size_t>(d.theatre.objects[static_cast<std::size_t>(i)].supply_line));
        SC_EXPECT_NEAR(m.route.length_m, p.Length(), 1.0);
        SC_EXPECT(m.level == MoverLevel::M0);
        if (m.state != MoverState::Moving) continue;
        ++moving;
        // The mover's head is where the air war has the column (going back, along the path from its far end).
        const MapPoint at = PointAlong(m.route, m.arc_m);
        const Vec3 truth_at = p.At(st.along_m);
        matched += std::hypot(at.east_m - truth_at.x, at.north_m - truth_at.y) < 5.0 ? 1 : 0;
        // Its time on the route follows the place.
        SC_EXPECT_NEAR(ArcAfter(m.route, m.moved_s), m.arc_m, 1.0);
    }
    std::printf("    %zu columns as movers, %d on the road, %d where the air war has them\n", columns.size(), moving, matched);
    SC_EXPECT(columns.size() >= 4 && moving >= 1 && matched == moving);
}

SC_TEST("battery life (G2.3, G9.4): the march to a place planned on the roads at the order, its papers in the map cases, the column there") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_battery_march.json", kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(def.nav != nullptr && def.nav->Ready() && def.nav->GetMovers().All().size() == 1);
    if (!def.nav) return;
    const MapPoint woods = def.nav->GetAnchors().Find("lazne_woods")->at;
    ScenarioRunner r(std::move(def), std::move(console));
    auto papers = [&](const std::string& station, const std::string& title) {
        int n = 0;
        for (const HeldPaper& p : r.MapCase(station)) n += p.title.find(title) != std::string::npos ? 1 : 0;
        return n;
    };
    // The battery commander's own papers from the start (G8): his staff list and his communications diagram.
    SC_EXPECT(papers("battery_commander", "Staff list") == 1 && papers("battery_commander", "Communications diagram") == 1);
    for (const HeldPaper& p : r.MapCase("battery_commander")) std::printf("    [%s]\n    %s\n", p.title.c_str(), p.text.c_str());
    double planned_at = -1.0, end_s = 0.0;
    bool convoy_seen = false;
    while (!r.Finished() && r.Time() < 4800.0) {
        r.Step(0.05);
        if (planned_at < 0.0 && r.MarchPlanned() != nullptr) {
            planned_at = r.Time();
            end_s = r.MarchPlanned()->end_s;
        }
        if (!convoy_seen) {
            for (const ScenarioRunner::WorldMover& v : r.MoversInView()) convoy_seen = convoy_seen || v.mover == "rgt_supply";
        }
        if (planned_at >= 0.0 && r.Time() > end_s + 900.0) break;
    }
    const nav::MarchPlan* plan = r.MarchPlanned();
    SC_EXPECT(plan != nullptr && plan->found);
    if (plan == nullptr) return;
    // Planned when the battery commander gave it (due at the combat order's minute), moving off then.
    std::printf("    given and planned at %.1f s\n", planned_at);
    SC_EXPECT(planned_at >= 60.0 - 0.1 && planned_at < 600.0);  // the session's clock in steps of 0.05 s
    SC_EXPECT_NEAR(plan->move_off_s, planned_at, 1.0);
    // The route card to the vehicle commander and the driver, the order of march to the battery commander.
    SC_EXPECT(papers("battery_commander", "Route card") == 1 && papers("driver", "Route card") == 1);
    SC_EXPECT(papers("battery_commander", "Order of march") == 1);
    for (const HeldPaper& p : r.MapCase("driver")) std::printf("    [%s]\n    %s\n", p.title.c_str(), p.text.c_str());
    // The column went by the plan's road and stands at its end.
    const Battery* b = r.GetBattery();
    double worst = 0.0;
    for (const Battery::Vehicle& v : b->Vehicles()) worst = std::max(worst, std::hypot(v.position.x - woods.east_m, v.position.y - woods.north_m));
    std::printf("    %.1f km by the roads, the head due at %.0f s; at %.0f s the farthest vehicle %.0f m from the woods\n",
                plan->route.length_m / 1000.0, plan->end_s, r.Time(), worst);
    SC_EXPECT(worst < 2500.0);
    SC_EXPECT(convoy_seen);  // the regiment's convoy came into sight on the way
}

SC_TEST("battery life (G2, G10.6): the regiment's change of position to a place; the march's points in the post's journal; arrived reported up") {
    std::string text;
    SC_EXPECT(simcore::detail::ReadTextFile(kData + "/scenarios/pu12m_battery_march.json", text));
    const std::string march = "\"order\": \"march\",";
    const std::size_t at = text.find(march);
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    text.replace(at, march.size(), "\"order\": \"relocate\", \"from\": \"regiment\",");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(text, kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    const std::string giver = def.battery_giver;
    ScenarioRunner r(std::move(def), std::move(console));
    auto said_up = [&](const std::string& words) {
        for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) {
            if (m.net == "regiment net" && m.text.find(words) != std::string::npos) return m.time_s;
        }
        return -1.0;
    };
    double planned_at = -1.0, end_s = 0.0;
    while (!r.Finished() && r.Time() < 4800.0) {
        r.Step(0.05);
        if (planned_at < 0.0 && r.MarchPlanned() != nullptr) {
            planned_at = r.Time();
            end_s = r.MarchPlanned()->end_s;
        }
        if (said_up("в назначенный район прибыла") >= 0.0 && r.Time() > said_up("в назначенный район прибыла") + 5.0) break;
    }
    // The regiment's order reached the battery commander on its net, who planned the march on the roads when he passed it on.
    const command::BatteryOrderNet* rgt = r.GetRegimentOrders();
    SC_EXPECT(rgt != nullptr && !rgt->Book().Orders().empty());
    if (rgt == nullptr || rgt->Book().Orders().empty()) return;
    SC_EXPECT(planned_at > 60.0 && r.MarchPlanned() != nullptr && r.MarchPlanned()->found);
    // The march's points in the post's journal as the head passed them, the arrival among them.
    const RadioJournal* j = r.GetBatteryJournal();
    SC_EXPECT(j != nullptr);
    if (j == nullptr) return;
    int start = 0, arrived = 0;
    for (const RadioJournalEntry& e : j->Legible(r.Time())) {
        if (e.net != "march") continue;
        std::printf("    journal %6.0f s  %s\n", e.heard_s, e.text.c_str());
        start += e.text.find("the head at the start point") == 0 ? 1 : 0;
        arrived += e.text.find("arrived: ") == 0 ? 1 : 0;
    }
    SC_EXPECT(start == 1 && arrived == 1);
    // Nothing up on the march (radio forbidden in the column); at the new place, ready, the battery commander reports it,
    // and only then is the regiment's order done.
    const double up_s = said_up("в назначенный район прибыла");
    std::printf("    the head due at %.0f s; reported up at %.0f s; the regiment's order done: %s\n", end_s, up_s,
                rgt->IsDone(0, giver) ? "yes" : "no");
    SC_EXPECT(up_s > end_s);
    SC_EXPECT(rgt->IsDone(0, giver));
}

SC_TEST("battery life: a scenario's \"nav\" refuses an unknown place, a bad mover, another origin, and a march to a place with a route") {
    auto refused = [](const std::string& edit_from, const std::string& edit_to, const std::string& want) {
        std::string text;
        SC_EXPECT(simcore::detail::ReadTextFile(kData + "/scenarios/pu12m_battery_march.json", text));
        const std::size_t at = text.find(edit_from);
        SC_EXPECT(at != std::string::npos);
        if (at == std::string::npos) return false;
        text.replace(at, edit_from.size(), edit_to);
        ScenarioDef def;
        ConsoleData console;
        std::string error;
        const bool ok = LoadScenario(text, kData, def, console, &error);
        if (ok || error.find(want) == std::string::npos) std::printf("    expected '%s', got '%s'\n", want.c_str(), error.c_str());
        return !ok && error.find(want) != std::string::npos;
    };
    SC_EXPECT(refused("\"to_place\": \"lazne_woods\"", "\"to_place\": \"atlantis\"", "atlantis"));
    SC_EXPECT(refused("\"to\": \"bty21_cp\"", "\"to\": \"atlantis\"", "atlantis"));
    SC_EXPECT(refused("\"vehicles\": 4", "\"vehicles\": 0", "vehicles"));
    SC_EXPECT(refused("\"lat_deg\": 46.9", "\"lat_deg\": 47.2", "origin"));
    SC_EXPECT(refused("\"to_place\": \"lazne_woods\"",
                      "\"to_place\": \"lazne_woods\", \"route\": [{\"east_m\": 0, \"north_m\": 0, \"up_m\": 0}, {\"east_m\": 0, \"north_m\": 100, \"up_m\": 0}]",
                      "planned on the roads"));
}

SC_TEST("battery life (G9.5): the campaign's relocation is a march with the roads' time; out of the fight until it arrives; saved and replayed") {
    using namespace simcore::airwar;
    const NavWorld& w = TheWorld();
    SessionNav roads;
    std::string e;
    SC_EXPECT(roads.Init(kData, "maps/pannonia.json", &w.district, "command/chain_pannonia.json", &e));
    AirWarData d;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_rebels.json", d, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    AirCampaign c;
    SC_EXPECT(c.Init(d, 7u, &e));
    auto run_to = [](AirCampaign& k, double h) {
        while (k.Now() < h - 1e-9) {
            if (k.Advance(h)) k.SkipPlayerMission();
        }
    };
    run_to(c, 6.0);
    const std::size_t me = static_cast<std::size_t>(c.PlayerIndex());
    const Vec3 from = c.Truth()[me].pos;
    const Vec3 to = from + Vec3{-6000.0, 4000.0, 0.0};
    SC_EXPECT(c.CanRelocateBattery(to));
    std::string why;
    const double march_h = roads.MarchHours({from.x, from.y}, {to.x, to.y}, Tactics(), &why);
    if (!why.empty()) std::printf("    %s\n", why.c_str());
    // On the roads, slower than the straight line at a column's speed.
    SC_EXPECT(march_h > Dist2d(from, to) / 1000.0 / 40.0 && march_h < 4.0);
    const double start_h = c.Now();
    c.RelocateBattery(to, march_h);
    SC_EXPECT(c.Marching() && c.MarchArriveH() > start_h);
    SC_EXPECT(!c.CanRelocateBattery(from, &why) && why.find("march") != std::string::npos);
    SC_EXPECT_NEAR(Dist2d(c.Truth()[me].pos, from), 0.0, 1e-9);  // not there yet
    run_to(c, start_h + march_h * 0.5);
    SC_EXPECT(c.Marching() && !c.HasPlayerMission());
    run_to(c, start_h + march_h + 2.0);
    SC_EXPECT(!c.Marching());
    SC_EXPECT_NEAR(Dist2d(c.Truth()[me].pos, to), 0.0, 1e-9);
    SC_EXPECT_NEAR(c.Truth()[me].last_moved_h, start_h + march_h, 1e-6);
    std::printf("    %.1f km apart: the march %.0f min by the roads, from %.2f h to %.2f h\n", Dist2d(from, to) / 1000.0, march_h * 60.0, start_h,
                c.Truth()[me].last_moved_h);
    // Saved and loaded, the march replays to the same hour.
    const std::string text = SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_rebels.json");
    SC_EXPECT(text.find("\"march_h\"") != std::string::npos);
    AirCampaign loaded;
    SC_EXPECT(LoadCampaignJson(text, kData, loaded, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT_NEAR(loaded.Truth()[me].last_moved_h, c.Truth()[me].last_moved_h, 0.0);
    SC_EXPECT(loaded.Journal().size() == c.Journal().size());
    // No march time: there at once, as before.
    AirCampaign at_once;
    SC_EXPECT(at_once.Init(d, 7u, &e));
    run_to(at_once, 6.0);
    at_once.RelocateBattery(to);
    SC_EXPECT(!at_once.Marching());
    SC_EXPECT_NEAR(Dist2d(at_once.Truth()[me].pos, to), 0.0, 1e-9);
}
