// Routes as flown (roadmap track B, B2; simcore/air_war/route_planner.hpp, docs/air_battle/01_ROUTES.md).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/route_planner.hpp"
#include "simcore/air_war/save.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

AirWarData LoadOn(const std::string& theatre, const std::string& faction) {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

std::vector<Package> PlanDays(const AirWarData& d, unsigned seed, double days, AirCampaign* out = nullptr) {
    AirCampaign c;
    c.Init(d, seed);
    while (c.Now() < days * 24.0) {
        if (c.Advance(days * 24.0)) c.SkipPlayerMission();
    }
    if (out != nullptr) *out = c;
    return c.Packages();
}

std::uint64_t JournalFnv(const AirCampaign& c) {
    std::uint64_t h = 1469598103934665603ull;
    for (const auto& j : c.Journal()) {
        for (char ch : j.text) {
            h ^= static_cast<unsigned char>(ch);
            h *= 1099511628211ull;
        }
        h ^= 0x0a;
        h *= 1099511628211ull;
    }
    return h;
}

// A test country 200 km square, flat but for a ridge running north-south along east 0 (200-400 m high, 3 km each
// side), with two columns of villages 6 km either side of it.
std::shared_ptr<const CountryMap> RidgeCountry() {
    CountryDef def;
    def.map_id = "ridge_test";
    def.ground.west_m = -100000.0;
    def.ground.east_m = 100000.0;
    def.ground.south_m = -100000.0;
    def.ground.north_m = 100000.0;
    def.grid_spacing_m = 1000.0;
    MapRidgeDef r;
    r.id = "ridge";
    r.points = {MapPoint{0.0, -100000.0}, MapPoint{0.0, 100000.0}};
    r.half_width_m = 3000.0;
    r.height_m = 300.0;
    def.ground.ridges.push_back(r);
    for (int k = -3; k <= 3; ++k) {
        for (int side : {-1, 1}) {
            CountrySettlementDef s;
            s.id = std::string(side < 0 ? "w" : "e") + std::to_string(k + 3);
            s.name = s.id;
            s.kind = "village";
            s.centre = MapPoint{side * 6000.0, k * 20000.0};
            def.settlements.push_back(s);
        }
    }
    return CountryMap::Build(def);
}

AircraftType TestJet() {
    AircraftType a;
    a.id = "test_jet";
    a.low_agl_m = 60.0;
    a.medium_alt_m = 4000.0;
    a.cruise_mps = 230.0;
    a.attack_mps = 250.0;
    return a;
}

bool Has(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }

}  // namespace

SC_TEST("routes (B2): a route is timed leg by leg at each leg's own speed, over its release point at the time on target") {
    std::vector<RoutePoint> r(4);
    r[0].pos = Vec3{0.0, 0.0, 0.0};
    r[0].leg_mps = 200.0;
    r[1].pos = Vec3{0.0, 36000.0, 0.0};
    r[1].leg_mps = 100.0;
    r[2].pos = Vec3{0.0, 72000.0, 0.0};
    r[2].leg_mps = 300.0;
    r[2].tag = "target";
    r[3].pos = Vec3{108000.0, 72000.0, 0.0};
    TimeRoute(r, 10.0);
    // 36 km at 200 m/s is 0.05 h, 36 km at 100 m/s 0.1 h, 108 km at 300 m/s 0.1 h.
    SC_EXPECT_NEAR(r[2].t_h, 10.0, 1e-12);
    SC_EXPECT_NEAR(r[1].t_h, 9.9, 1e-12);
    SC_EXPECT_NEAR(r[0].t_h, 9.85, 1e-12);
    SC_EXPECT_NEAR(r[3].t_h, 10.1, 1e-12);
}

SC_TEST("routes (B2): a low route goes behind a ridge the believed site cannot see over; at height it cannot hide") {
    TuningTable tune;
    RouteGround g;
    g.BuildFrom(RidgeCountry(), tune);
    SC_EXPECT(g.HasGround());
    SC_EXPECT(g.IndexOf("settlement:w3") >= 0);
    // A site 10 km east of the ridge, ring 20 km: the straight way north along east 6 km passes 4 km from it.
    const std::vector<RouteThreat> threats = {RouteThreat{"osa", Vec3{10000.0, 0.0, 0.0}, 20000.0, 1.0, true}};
    // Behind the ridge at 60 m it sees nothing; on the near side it sees all.
    SC_EXPECT(g.Seen("osa", threats[0].pos, 20000.0, Vec3{-6000.0, -15000.0, 0.0}, Vec3{-6000.0, 15000.0, 0.0}, 60.0, 60.0, true) < 1.0);
    SC_EXPECT(g.Seen("osa", threats[0].pos, 20000.0, Vec3{6000.0, -15000.0, 0.0}, Vec3{6000.0, 15000.0, 0.0}, 60.0, 60.0, true) > 25000.0);
    const AircraftType a = TestJet();
    RouteRules rules;
    const RoutePlanner planner(g, tune, rules);
    EntryPoint gate{"G1", Vec3{6000.0, -75000.0, 0.0}};
    RouteRequest rq;
    rq.bases = Vec3{6000.0, -160000.0, 0.0};
    rq.gate_in = &gate;
    rq.target = Vec3{6000.0, 75000.0, 0.0};
    rq.type = &a;
    rq.profile = Profile::Low;
    LegCache cache;
    const PlannedRoute low = planner.Plan(rq, threats, &cache);
    SC_EXPECT(low.ok);
    std::string turns;
    for (const std::string& t : low.turns) turns += t + " ";
    std::printf("    low route: %s(exposure %.1f s, corridor %s)\n", turns.c_str(), low.exposure_s, low.corridor.c_str());
    // Behind the ridge abeam the site (by the village w3, on a straight, so no turn point), not by e3 4 km from it.
    // Features on the line are left out of the points, so the route's line is checked where it passes the site.
    std::vector<std::string> over;
    for (const RoutePoint& rp : low.points) over.push_back(rp.feature);
    SC_EXPECT(!Has(over, "settlement:e3"));
    bool behind = false;
    for (std::size_t i = 0; i + 1 < low.points.size(); ++i) {
        const Vec3& p0 = low.points[i].pos;
        const Vec3& p1 = low.points[i + 1].pos;
        if (p0.y <= 0.0 && p1.y > 0.0) behind = p0.x + (p1.x - p0.x) * (0.0 - p0.y) / (p1.y - p0.y) < -3000.0;
    }
    SC_EXPECT(behind);
    // The straight way's believed exposure against the route's.
    const double straight = g.Seen("osa", threats[0].pos, 20000.0, gate.pos, rq.target, 60.0, 60.0, true) / RouteLowMps(a);
    SC_EXPECT(low.exposure_s < 0.25 * straight);
    SC_EXPECT(low.longest_s <= tune.Get("route.continuous_s"));
    // The same flight at medium height is seen everywhere in the ring: the ridge buys nothing.
    std::vector<RouteThreat> high = threats;
    high[0].masked = false;
    const double high_w = g.Seen("osa", high[0].pos, 20000.0, Vec3{-6000.0, -15000.0, 0.0}, Vec3{-6000.0, 15000.0, 0.0}, 4000.0, 4000.0, false);
    SC_EXPECT(high_w > 20000.0);
    // Caches change nothing: the same plan without the leg cache, and again with the warm one.
    const PlannedRoute cold = planner.Plan(rq, threats, nullptr);
    const PlannedRoute warm = planner.Plan(rq, threats, &cache);
    SC_EXPECT(cold.turns == low.turns && warm.turns == low.turns);
    SC_EXPECT_NEAR(cold.cost, low.cost, 1e-9);
    SC_EXPECT_NEAR(warm.cost, low.cost, 1e-9);
}

SC_TEST("routes (B2): the route's structure: let-down before the gate, turn points on features, an IP, the egress turn and home") {
    TuningTable tune;
    RouteGround g;
    g.BuildFrom(RidgeCountry(), tune);
    const AircraftType a = TestJet();
    RouteRules rules;
    rules.crossing = "checkpoint_gate";
    rules.letdown_m = 40000.0;
    const RoutePlanner planner(g, tune, rules);
    EntryPoint g1{"G1", Vec3{-6000.0, -75000.0, 0.0}}, g2{"G2", Vec3{30000.0, -75000.0, 0.0}};
    RouteRequest rq;
    rq.bases = Vec3{6000.0, -160000.0, 0.0};
    rq.gate_in = &g1;
    rq.exits = {&g1, &g2};
    rq.target = Vec3{6000.0, 60000.0, 0.0};
    rq.type = &a;
    rq.delivery = Delivery::PopUp;
    // No threats: the way is the short one.
    const PlannedRoute pr = planner.Plan(rq, {}, nullptr);
    SC_EXPECT(pr.ok);
    std::vector<std::string> tags;
    for (const RoutePoint& p : pr.points) tags.push_back(p.tag);
    std::string all;
    for (const std::string& t : tags) all += t + " ";
    std::printf("    %s\n", all.c_str());
    SC_EXPECT(tags.front() == "entry" && tags[1] == "letdown" && tags[2] == "checkpoint" && tags[3] == "gate");
    SC_EXPECT(tags.back() == "exit" && tags[tags.size() - 2] == "climb" && tags[tags.size() - 3] == "gate");
    const int ip = RouteIndexOf(pr.points, "ip"), pull = RouteIndexOf(pr.points, "pull_up"), tg = RouteIndexOf(pr.points, "target"),
              eg = RouteIndexOf(pr.points, "egress");
    SC_EXPECT(ip > 3 && pull == ip + 1 && tg > pull && eg == tg + 1);
    for (std::size_t i = 4; i < static_cast<std::size_t>(ip); ++i) {
        SC_EXPECT(pr.points[i].tag == "turn" && g.IndexOf(pr.points[i].feature) >= 0);
    }
    // Every leg has its speed; transit high, the rest low; the times run forward once timed.
    std::vector<RoutePoint> r = pr.points;
    TimeRoute(r, 5.0);
    for (std::size_t i = 0; i + 1 < r.size(); ++i) {
        SC_EXPECT(r[i].leg_mps > 0.0);
        SC_EXPECT(r[i + 1].t_h > r[i].t_h);
    }
    SC_EXPECT_NEAR(r[static_cast<std::size_t>(tg)].t_h, 5.0, 1e-12);
    SC_EXPECT(r[0].agl_m >= a.medium_alt_m && r[3].agl_m == 60.0);
    // With no threats the way in is near the straight line (route.direct_slack).
    double in_m = 0.0;
    for (int i = 3; i < tg; ++i) in_m += Dist2d(r[static_cast<std::size_t>(i)].pos, r[static_cast<std::size_t>(i) + 1].pos);
    SC_EXPECT(in_m <= tune.Get("route.direct_slack") * Dist2d(g1.pos, rq.target) + 5000.0);
    // Home another way: none of the way in's turn points.
    for (const std::string& h : pr.home) SC_EXPECT(!Has(pr.turns, h));
}

SC_TEST("routes (B2): a target inside a believed ring is attacked from inside it, straight in from the IP") {
    TuningTable tune;
    RouteGround g;
    g.BuildFrom(RidgeCountry(), tune);
    const AircraftType a = TestJet();
    RouteRules rules;
    const RoutePlanner planner(g, tune, rules);
    EntryPoint gate{"G1", Vec3{14000.0, -75000.0, 0.0}};
    RouteRequest rq;
    rq.bases = Vec3{14000.0, -160000.0, 0.0};
    rq.gate_in = &gate;
    rq.exits = {&gate};
    rq.target = Vec3{14000.0, 20000.0, 0.0};
    rq.type = &a;
    rq.delivery = Delivery::Loft;  // its pull-up on the run-in line (a pop-up's is offset to the side by design)
    // The target's own guard: a site on it, ring 15 km.
    const std::vector<RouteThreat> threats = {RouteThreat{"guard", rq.target, 15000.0, 1.0, true}};
    const PlannedRoute pr = planner.Plan(rq, threats, nullptr);
    SC_EXPECT(pr.ok);
    const int ip = RouteIndexOf(pr.points, "ip"), tg = RouteIndexOf(pr.points, "target");
    SC_EXPECT(ip > 0 && tg > ip);
    if (ip <= 0 || tg <= ip) return;
    // No turn point between the IP and the release; the attack's points lie on the IP's run-in line.
    const Vec3 from = pr.points[static_cast<std::size_t>(ip)].pos, to = pr.points[static_cast<std::size_t>(tg)].pos;
    const double len = Dist2d(from, to);
    for (int i = ip + 1; i < tg; ++i) {
        const Vec3 p = pr.points[static_cast<std::size_t>(i)].pos;
        SC_EXPECT(pr.points[static_cast<std::size_t>(i)].tag != "turn");
        const double cross = ((to.x - from.x) * (p.y - from.y) - (to.y - from.y) * (p.x - from.x)) / std::max(1.0, len);
        SC_EXPECT(std::fabs(cross) < 1.0);
    }
    // The release is inside the ring: the run is not bent out of it to save exposure.
    SC_EXPECT(Dist2d(to, threats[0].pos) < threats[0].ring_m);
}

SC_TEST("routes (B2): over dense features a far target is reached; features flown straight over are no turn points") {
    // Villages every 8 km on a 160 x 160 km square: each feature's nearest neighbours are a few km off, so a route of
    // 150 km is many hops; with two turn points allowed it is still planned, and only its real turns are listed.
    CountryDef def;
    def.map_id = "dense_test";
    def.ground.west_m = -100000.0;
    def.ground.east_m = 100000.0;
    def.ground.south_m = -100000.0;
    def.ground.north_m = 100000.0;
    def.grid_spacing_m = 2000.0;
    for (int i = -10; i <= 10; ++i) {
        for (int j = -10; j <= 10; ++j) {
            CountrySettlementDef s;
            s.id = "v" + std::to_string(i + 10) + "_" + std::to_string(j + 10);
            s.name = s.id;
            s.kind = "village";
            s.centre = MapPoint{i * 8000.0, j * 8000.0};
            def.settlements.push_back(s);
        }
    }
    TuningTable tune;
    RouteGround g;
    g.BuildFrom(CountryMap::Build(def), tune);
    const AircraftType a = TestJet();
    RouteRules rules;
    rules.max_turn_points = 2;
    const RoutePlanner planner(g, tune, rules);
    EntryPoint gate{"G1", Vec3{0.0, -80000.0, 0.0}};
    RouteRequest rq;
    rq.bases = Vec3{0.0, -160000.0, 0.0};
    rq.gate_in = &gate;
    rq.exits = {&gate};
    rq.target = Vec3{28000.0, 76000.0, 0.0};  // off the grid's lines: the way flies over villages a little off straight
    rq.type = &a;
    const PlannedRoute pr = planner.Plan(rq, {}, nullptr);
    SC_EXPECT(pr.ok);
    const int ip = RouteIndexOf(pr.points, "ip");
    // The turn points listed (the IP last) against the features flown over on the way in.
    const int turns = static_cast<int>(pr.turns.size()) - 1;
    int over = 0;
    for (int i = 0; i < ip; ++i) over += pr.points[static_cast<std::size_t>(i)].tag == "turn" ? 1 : 0;
    std::printf("    %d turn points and %d features flown over on the way in of %.0f km\n", turns, over, Dist2d(gate.pos, rq.target) / 1000.0);
    SC_EXPECT(ip > 0 && turns <= 2 && over >= turns);
    // Every listed turn point turns more than route.straight_deg; the route's shape is what was searched.
    for (std::size_t k = 0; k + 1 < pr.turns.size(); ++k) {
        int at = -1;
        for (int i = 1; i < ip; ++i) {
            if (pr.points[static_cast<std::size_t>(i)].feature == pr.turns[k]) at = i;
        }
        SC_EXPECT(at > 0);
        if (at <= 0) continue;
        const Vec3 d0 = pr.points[static_cast<std::size_t>(at)].pos - pr.points[static_cast<std::size_t>(at - 1)].pos;
        const Vec3 d1 = pr.points[static_cast<std::size_t>(at + 1)].pos - pr.points[static_cast<std::size_t>(at)].pos;
        const double turn_deg = std::fabs(std::atan2(d0.x * d1.y - d0.y * d1.x, d0.x * d1.x + d0.y * d1.y)) * 57.29577951308232;
        SC_EXPECT(turn_deg > tune.Get("route.straight_deg"));
    }
}

SC_TEST("routes (B2): a helicopter's battle position is hidden from the believed site and sees its target") {
    TuningTable tune;
    RouteGround g;
    g.BuildFrom(RidgeCountry(), tune);
    AircraftType h;
    h.id = "test_helo";
    h.helicopter = true;
    h.low_agl_m = 15.0;
    h.medium_alt_m = 1000.0;
    h.cruise_mps = 60.0;
    h.attack_mps = 60.0;
    RouteRules rules;
    const RoutePlanner planner(g, tune, rules);
    EntryPoint gate{"G1", Vec3{-30000.0, -40000.0, 0.0}};
    RouteRequest rq;
    rq.bases = Vec3{-30000.0, -80000.0, 0.0};
    rq.gate_in = &gate;
    rq.target = Vec3{-9000.0, 0.0, 0.0};  // just west of the ridge; the site 10 km east of it
    rq.type = &h;
    rq.ip_dist_m = 6000.0;
    rq.helicopter_hide = true;
    rq.delivery = Delivery::HoverFire;
    const std::vector<RouteThreat> threats = {RouteThreat{"osa", Vec3{10000.0, 0.0, 0.0}, 20000.0, 1.0, true}};
    const PlannedRoute pr = planner.Plan(rq, threats, nullptr);
    SC_EXPECT(pr.ok);
    SC_EXPECT(pr.hide_unseen && pr.hide_sees);
    const int hide = RouteIndexOf(pr.points, "hide");
    SC_EXPECT(hide > 0 && pr.points[static_cast<std::size_t>(hide)].pos.x < -3000.0);
    SC_EXPECT(RouteIndexOf(pr.points, "letdown") < 0 && RouteIndexOf(pr.points, "climb") < 0);
}

SC_TEST("routes (B2): the Pact's counter-SAM programme names each believed zone its route crosses, and a way out of it") {
    std::vector<RoutePoint> r(4);
    r[0].pos = Vec3{0.0, -40000.0, 0.0};
    r[1].pos = Vec3{0.0, -5000.0, 0.0};
    r[2].pos = Vec3{0.0, 5000.0, 0.0};
    r[3].pos = Vec3{0.0, 40000.0, 0.0};
    for (RoutePoint& p : r) p.agl_m = 60.0;
    const std::vector<RouteThreat> threats = {RouteThreat{"a", Vec3{3000.0, 0.0, 0.0}, 10000.0, 1.0, false},
                                              RouteThreat{"b", Vec3{50000.0, 0.0, 0.0}, 10000.0, 1.0, false},
                                              RouteThreat{"c", Vec3{-2000.0, 0.0, 0.0}, 10000.0, 0.0, false}};
    const std::vector<ZoneEscape> z = RoutePlanner::Escapes(r, threats);
    SC_EXPECT(z.size() == 1);  // b is off the route; c cannot reach its height
    if (z.empty()) return;
    SC_EXPECT(z[0].threat == "a" && z[0].first == 0 && z[0].last == 2 && z[0].rejoin == 3);
    SC_EXPECT(z[0].dz_m < 0.0 && z[0].full_speed && z[0].jam);
}

SC_TEST("routes (B2): the resolver engages once a pass through a zone however many legs the pass takes; the old shapes per leg") {
    AirWarData d = LoadOn("theatre_pannonia.json", "nato");
    AirCampaign base;
    base.Init(d, 1);
    int site = -1;
    for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
        const TheatreObject& o = d.theatre.objects[i];
        if (o.defence.present && !o.player && !o.decoy && o.defence.lethality > 0.0 && o.defence.missiles != 0 && o.defence.radius_m > 3000.0 &&
            o.defence.medium_cover && (o.defence.max_target_mps <= 0.0 || o.defence.max_target_mps > 300.0)) {
            site = static_cast<int>(i);
            break;
        }
    }
    SC_EXPECT(site >= 0);
    if (site < 0) return;
    const TheatreObject& def = d.theatre.objects[static_cast<std::size_t>(site)];
    const auto fired = [&](double mode) {
        AirWarData dm = d;
        dm.tuning.values["route.mode"] = mode;
        std::vector<ObjectState> truth = base.Truth();
        const Vec3 c = truth[static_cast<std::size_t>(site)].pos;
        const double rr = 0.5 * def.defence.radius_m;
        Package p;
        p.id = 1;
        p.kind = MissionKind::Strike;
        Flight fl;
        fl.callsign = "Test 1";
        fl.role = FlightRole::Recce;
        fl.aircraft = "rf4c";
        fl.count = 4;
        fl.profile = Profile::Medium;
        // Three legs inside the ring: a zig-zag past the site.
        fl.route = {{c + Vec3{-3.0 * rr, -rr, 0.0}, 4000.0, 0.0, "entry"}, {c + Vec3{-rr, rr, 0.0}, 4000.0, 0.1, "turn"},
                    {c + Vec3{rr, -rr, 0.0}, 4000.0, 0.2, "turn"}, {c + Vec3{rr, rr, 0.0}, 4000.0, 0.3, "turn"},
                    {c + Vec3{3.0 * rr, 3.0 * rr, 0.0}, 4000.0, 0.4, "exit"}};
        p.flights = {fl};
        BatteryPosture posture;
        AirStaff staff;
        Rng rs(9);
        staff.Build(dm, rs, 0.0);
        Rng rng(3);
        ResolveContext rc;
        rc.data = &dm;
        rc.truth = &truth;
        rc.posture = &posture;
        rc.staff = &staff;
        rc.rng = &rng;
        Resolver r;
        const PackageOutcome out = r.Resolve(p, rc);
        int n = 0;
        for (const std::string& f : out.fired) n += f == def.id ? 1 : 0;
        return n;
    };
    const int old_shapes = fired(0.0), routed = fired(1.0);
    std::printf("    %s fired %d times on the old rule, %d on the pass rule\n", def.id.c_str(), old_shapes, routed);
    SC_EXPECT(old_shapes >= 3);
    SC_EXPECT(routed == 1);
}

SC_TEST("routes (B2): a chord the ground masked carries no second cut for a long-range site at low level") {
    TuningTable tune;
    const double unmasked = PassKillChance(0.2, 10000.0, 30000.0, Profile::Low, false, true, true, false, 0.0, 0.0, 1.0, 0.0, tune);
    const double masked = PassKillChance(0.2, 10000.0, 30000.0, Profile::Low, false, true, true, false, 0.0, 0.0, 1.0, 0.0, tune, true);
    SC_EXPECT(masked >= unmasked);
    if (tune.Get("attrition.low_vs_long_range_sam") < 1.0) SC_EXPECT(masked > unmasked);
    // A short-range site: the same either way.
    SC_EXPECT_NEAR(PassKillChance(0.2, 5000.0, 6000.0, Profile::Low, false, true, true, false, 0.0, 0.0, 1.0, 0.0, tune),
                   PassKillChance(0.2, 5000.0, 6000.0, Profile::Low, false, true, true, false, 0.0, 0.0, 1.0, 0.0, tune, true), 1e-12);
}

SC_TEST("routes (B2): the factions' low-level heights and routing rules load with their sources") {
    const AirWarData nato = LoadOn("theatre_pannonia.json", "nato");
    const AirWarData pact = LoadOn("theatre_pannonia.json", "warsaw_pact");
    const AirWarData neutral = LoadOn("theatre_pannonia.json", "neutral");
    const AirWarData rebels = LoadOn("theatre_pannonia.json", "rebels");
    const AircraftType* f111 = nato.faction.Aircraft("f111f");
    const AircraftType* tornado = nato.faction.Aircraft("tornado_ids");
    const AircraftType* su24 = pact.faction.Aircraft("su24");
    SC_EXPECT(f111 != nullptr && tornado != nullptr && su24 != nullptr);
    if (f111 == nullptr || tornado == nullptr || su24 == nullptr) return;
    SC_EXPECT_NEAR(f111->low_agl_night_m, 61.0, 0.5);           // crandall_twz: 200 ft on TFR
    SC_EXPECT_NEAR(tornado->low_agl_night_m, 152.0, 0.5);       // napier_hushkit_2022: 500 ft
    SC_EXPECT(tornado->low_level_agl_m >= 30.0 && tornado->low_level_agl_m <= 75.0);
    SC_EXPECT(su24->low_level_agl_m >= 50.0 && su24->low_level_agl_m <= 100.0);  // fm_100_2_1_1984 p. 12-8
    SC_EXPECT(!f111->low_level_source.empty() && !su24->low_level_source.empty());
    SC_EXPECT(RouteLowAgl(*f111, DayPart::Night, nato.tuning) < RouteLowAgl(*tornado, DayPart::Night, nato.tuning));
    const RouteRules& rn = nato.faction.doctrine.routing;
    const RouteRules& rp = pact.faction.doctrine.routing;
    SC_EXPECT(rn.present && rn.crossing == "gate" && rn.cas_free && rn.ret == "other_gate" && rn.corridor_suppression);
    SC_EXPECT(rp.present && rp.crossing == "checkpoint_gate" && rp.zone_escapes && rp.helo_umbrella && rp.blast_lead_s > 0.0);
    SC_EXPECT(neutral.faction.doctrine.routing.crossing == "entry" && neutral.faction.doctrine.routing.max_turn_points <= 2);
    SC_EXPECT(rebels.faction.doctrine.routing.crossing == "none" && rebels.faction.doctrine.routing.lines_bonus > 0.0);
    SC_EXPECT(rn.variants.size() == 3 && rebels.faction.doctrine.routing.variants.size() == 1);
}

SC_TEST("routes (B2): a NATO campaign on the nation plans by gates and corridors: other gate home, gates kept apart, keys by corridor") {
    const AirWarData d = LoadOn("theatre_pannonia_nation.json", "nato");
    SC_EXPECT(d.country != nullptr);
    AirCampaign c;
    const std::vector<Package> ps = PlanDays(d, 3, 1.0, &c);
    int routed = 0, strikes = 0, other_gate = 0, keyed = 0;
    std::map<std::string, std::vector<double>> by_gate;
    for (const Package& p : ps) {
        if (p.kind == MissionKind::Missile) continue;
        ++routed;
        if (p.kind == MissionKind::Strike || p.kind == MissionKind::Dead) {
            ++strikes;
            if (p.exit != p.entry) ++other_gate;
        }
        if (p.tactic.find(" via ") != std::string::npos || p.tactic.find(" direct") != std::string::npos) ++keyed;
        SC_EXPECT(!p.corridor.empty());
        if (p.gate_h >= 0.0) by_gate[p.entry].push_back(p.gate_h);
        for (const Flight& fl : p.flights) {
            if (fl.role != FlightRole::Strike && fl.role != FlightRole::Dead) continue;
            SC_EXPECT(RouteIndexOf(fl.route, "gate") >= 0 && RouteIndexOf(fl.route, "letdown") > 0);
            for (std::size_t i = 0; i + 1 < fl.route.size(); ++i) SC_EXPECT(fl.route[i + 1].t_h >= fl.route[i].t_h);
            const int tg = RouteIndexOf(fl.route, "target");
            SC_EXPECT(tg > 0 && fl.release_index == tg);
            // Its turn points are on the map's features.
            for (const RoutePoint& rp : fl.route) {
                if (rp.tag == "turn") SC_EXPECT(rp.feature.find(':') != std::string::npos);
            }
        }
    }
    std::printf("    %d packages: %d strikes, %d of them home by another gate; %d keyed by corridor\n", routed, strikes, other_gate, keyed);
    SC_EXPECT(routed > 0 && keyed == routed);
    SC_EXPECT(other_gate == strikes);  // NATO comes home by another gate (the theatre has four)
    // Packages of one cycle through one gate: at least the separation apart (the book is per cycle; a day has a few).
    const double sep_h = d.faction.doctrine.routing.gate_separation_s / 3600.0;
    int close = 0;
    for (const auto& g : by_gate) {
        for (std::size_t i = 0; i < g.second.size(); ++i) {
            for (std::size_t j = i + 1; j < g.second.size(); ++j) close += std::fabs(g.second[i] - g.second[j]) < sep_h - 1e-6 ? 1 : 0;
        }
    }
    SC_EXPECT(close == 0);
}

SC_TEST("routes (B2): a Pact campaign on the nation flies by a checkpoint, carries its counter-SAM programme, sends suppression ahead") {
    const AirWarData d = LoadOn("theatre_pannonia_nation.json", "warsaw_pact");
    const std::vector<Package> ps = PlanDays(d, 5, 2.0);
    int checkpoint = 0, escapes = 0, mains = 0, ahead = 0, sead = 0;
    for (const Package& p : ps) {
        double main_tot = -1.0;
        for (const Flight& fl : p.flights) {
            if (fl.role != FlightRole::Strike && fl.role != FlightRole::Dead) continue;
            ++mains;
            if (RouteIndexOf(fl.route, "checkpoint") >= 0) ++checkpoint;
            if (!fl.escapes.empty()) ++escapes;
            const int tg = RouteIndexOf(fl.route, "target");
            if (tg >= 0) main_tot = fl.route[static_cast<std::size_t>(tg)].t_h;
        }
        for (const Flight& fl : p.flights) {
            if (fl.role != FlightRole::Sead || main_tot < 0.0) continue;
            ++sead;
            const int tg = RouteIndexOf(fl.route, "target");
            if (tg >= 0 && fl.route[static_cast<std::size_t>(tg)].t_h < main_tot - 0.5 * d.faction.doctrine.routing.blast_lead_s / 3600.0) ++ahead;
        }
    }
    std::printf("    %d main flights: %d by a checkpoint, %d with zone escapes; %d SEAD flights, %d ahead of the strike\n", mains, checkpoint,
                escapes, sead, ahead);
    SC_EXPECT(mains > 0 && checkpoint == mains);
    SC_EXPECT(escapes > 0);
    SC_EXPECT(sead == 0 || ahead > 0);
}

SC_TEST("routes (B2): the same seed plans the same routes; an option's noise is its own stream") {
    const AirWarData d = LoadOn("theatre_pannonia.json", "warsaw_pact");
    AirCampaign a, b;
    PlanDays(d, 11, 2.0, &a);
    PlanDays(d, 11, 2.0, &b);
    SC_EXPECT(a.Packages().size() == b.Packages().size());
    SC_EXPECT(JournalFnv(a) == JournalFnv(b));
    bool same = a.Packages().size() == b.Packages().size();
    for (std::size_t i = 0; same && i < a.Packages().size(); ++i) {
        const Package& x = a.Packages()[i];
        const Package& y = b.Packages()[i];
        same = x.corridor == y.corridor && x.flights.size() == y.flights.size();
        for (std::size_t f = 0; same && f < x.flights.size(); ++f) {
            same = x.flights[f].route.size() == y.flights[f].route.size();
            for (std::size_t k = 0; same && k < x.flights[f].route.size(); ++k) {
                same = x.flights[f].route[k].pos.x == y.flights[f].route[k].pos.x && x.flights[f].route[k].t_h == y.flights[f].route[k].t_h;
            }
        }
    }
    SC_EXPECT(same);
    Rng r(42);
    const std::uint64_t before = r.State();
    Rng child = r.Fork(Fnv1a("strike|key|N1|low|day|1.0|0"));
    (void)child.Normal();
    SC_EXPECT(r.State() == before);
    SC_EXPECT(Fnv1a("abc") == Fnv1a("abc") && Fnv1a("abc") != Fnv1a("abd"));
}

SC_TEST("routes (B2): a save made before B2 replays to the bit with the old shapes; a new save keeps its route mode") {
    std::ifstream in(kData + "/../simcore/tests/fixtures/air_war_save_v1.json", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    SC_EXPECT(!text.empty());
    AirCampaign c;
    std::string e;
    SC_EXPECT(LoadCampaignJson(text, kData, c, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    // The fingerprint (pact, the district theatre, seed 21, three days). The code before B2 wrote 170 entries, FNV
    // 17720618632243727934, 57 packages, and B2 and B5 replayed it to the bit. B8 (18 Nov 2026) changed the Pact's 1985
    // order of battle (the MiG-23ML regiments beside the MLD's) and replaced the flat fighter roll with the air battle, so
    // the same actions now make another war; this is B8's fingerprint, and a reload replays it to the bit.
    std::printf("    replay: journal %zu, fnv %llu, packages %zu\n", c.Journal().size(), static_cast<unsigned long long>(JournalFnv(c)), c.Packages().size());
    SC_EXPECT(c.Journal().size() == 107);
    SC_EXPECT(JournalFnv(c) == 1375944258174313379ull);
    SC_EXPECT(c.Packages().size() == 41);
    {
        AirCampaign again;
        SC_EXPECT(LoadCampaignJson(text, kData, again, &e));
        SC_EXPECT(JournalFnv(again) == JournalFnv(c));
    }
    SC_EXPECT_NEAR(c.Data().tuning.Get("route.mode"), 0.0, 1e-12);
    // A save now is version 2 with the mode it was planned in, and replays to the same journal.
    const AirWarData d = LoadOn("theatre_pannonia.json", "warsaw_pact");
    AirCampaign n;
    PlanDays(d, 21, 1.0, &n);
    const std::string saved = SaveCampaignJson(n, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json");
    SC_EXPECT(saved.find("\"route_mode\"") != std::string::npos);
    AirCampaign back;
    SC_EXPECT(LoadCampaignJson(saved, kData, back, &e));
    SC_EXPECT(JournalFnv(back) == JournalFnv(n));
    SC_EXPECT_NEAR(back.Data().tuning.Get("route.mode"), 1.0, 1e-12);
}

SC_TEST("routes (B2): the fighters meet a package on the leg from its IP, found by its tag, whatever comes before") {
    std::vector<RoutePoint> old_shape(4), routed(7);
    old_shape[1].tag = "ip";
    for (std::size_t i = 0; i < routed.size(); ++i) routed[i].tag = i == 5 ? "ip" : "turn";
    SC_EXPECT(FighterLeg(old_shape) == 1);
    SC_EXPECT(FighterLeg(routed) == 5);
    routed[5].tag = "hide";
    SC_EXPECT(FighterLeg(routed) == 5);
    std::vector<RoutePoint> loiter(5);
    SC_EXPECT(FighterLeg(loiter) == 1);
}

SC_TEST("routes (B2): a tactic is remembered by its corridor and way home") {
    const std::string k = TacticKey("N2", "N2 via settlement:kovar", "N3", Profile::Low, DayPart::Night, MissionKind::Strike);
    SC_EXPECT(k.find("N2 via settlement:kovar") != std::string::npos && k.find("|N3|") != std::string::npos);
    SC_EXPECT(k != TacticKey("N2", "N2 direct", "N3", Profile::Low, DayPart::Night, MissionKind::Strike));
}
