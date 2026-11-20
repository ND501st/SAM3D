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
    SC_EXPECT_NEAR(back.Data().tuning.Get("route.mode"), 2.0, 1e-12);  // roadmap S7's mode since S7
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

// ------------------------------------------------------------------------------------------------ roadmap S7

namespace {

// The packages' corridors in order, and how many follow one of the same corridor (the planner's same way in).
int SameCorridorRuns(const std::vector<Package>& ps, int* routed = nullptr) {
    int same = 0, n = 0;
    std::string last;
    for (const Package& p : ps) {
        if (p.corridor.empty()) continue;
        ++n;
        if (p.corridor == last) ++same;
        last = p.corridor;
    }
    if (routed != nullptr) *routed = n;
    return same;
}

AirWarData WithPlannerQuirk(AirWarData d, const std::string& effect) {
    QuirkDef q;
    q.id = "test_" + effect;
    q.name = q.id;
    q.effects[effect] = 3.0;
    d.quirks.push_back(q);
    for (OfficerDef& o : d.faction.officers) {
        if (o.role == StaffRole::Planner) o.quirks = {q.id};
    }
    return d;
}

}  // namespace

SC_TEST("routes (S7): a gate with no feature within a leg is joined to its nearest features; mode 1 leaves it unplanned") {
    // The ridge country's villages run from 60 km south to 60 km north; the gate stands 35 km south of the last, legs at
    // most 25 km: no edge reaches it.
    TuningTable tune;
    tune.values["route.max_leg_m"] = 25000.0;
    RouteGround g;
    g.BuildFrom(RidgeCountry(), tune);
    const AircraftType a = TestJet();
    RouteRules rules;
    EntryPoint gate{"G1", Vec3{6000.0, -95000.0, 0.0}};
    RouteRequest rq;
    rq.bases = Vec3{6000.0, -160000.0, 0.0};
    rq.gate_in = &gate;
    rq.exits = {&gate};
    rq.target = Vec3{6000.0, 40000.0, 0.0};
    rq.type = &a;
    tune.values["route.mode"] = 1.0;
    const RoutePlanner old_planner(g, tune, rules);
    const PlannedRoute before = old_planner.Plan(rq, {}, nullptr);
    tune.values["route.mode"] = 2.0;
    const RoutePlanner planner(g, tune, rules);
    const PlannedRoute now = planner.Plan(rq, {}, nullptr);
    std::printf("    mode 1 %s, mode 2 %s (%zu points, first turn %s)\n", before.ok ? "planned" : "unplanned", now.ok ? "planned" : "unplanned",
                now.points.size(), now.turns.empty() ? "-" : now.turns.front().c_str());
    SC_EXPECT(!before.ok || before.turns.empty());
    SC_EXPECT(now.ok);
    SC_EXPECT(!now.turns.empty());
}

SC_TEST("routes (S7): our believed air bases cost a leg; a low flight minds them less; no zone, escape or exposure") {
    TuningTable tune;
    std::vector<IntelEntry> pic(3);
    pic[0].key = "af_b";
    pic[0].kind = ObjectKind::Airfield;
    pic[0].pos = Vec3{0.0, 20000.0, 0.0};
    pic[0].sigma_m = 4000.0;
    pic[1] = pic[0];
    pic[1].key = "af_a";
    pic[1].exists = 0.1;  // hardly believed: no ring
    pic[2] = pic[0];
    pic[2].key = "sam";
    pic[2].kind = ObjectKind::SamSite;
    const AircraftType a = TestJet();
    const std::vector<RouteThreat> high = BelievedFighterThreats(pic, 0.6, a, Profile::Medium, tune);
    const std::vector<RouteThreat> low = BelievedFighterThreats(pic, 0.6, a, Profile::Low, tune);
    SC_EXPECT(high.size() == 1 && low.size() == 1);
    if (high.size() != 1 || low.size() != 1) return;
    SC_EXPECT(high[0].fighter && high[0].key == "fighter:af_b" && !high[0].masked);
    SC_EXPECT_NEAR(high[0].ring_m, tune.Get("route.fighter_reach_m") + 2000.0, 1e-9);
    SC_EXPECT_NEAR(high[0].weight, tune.Get("route.fighter_weight") * 0.6, 1e-12);
    SC_EXPECT_NEAR(low[0].weight, high[0].weight * tune.Get("route.fighter_low_share"), 1e-12);
    SC_EXPECT(BelievedFighterThreats(pic, 0.0, a, Profile::Medium, tune).empty());
    // A route past the base costs more with it than without; it is never an exposure to the believed SAMs.
    RouteGround g;
    g.BuildFrom(RidgeCountry(), tune);
    RouteRules rules;
    const RoutePlanner planner(g, tune, rules);
    EntryPoint gate{"G1", Vec3{6000.0, -75000.0, 0.0}};
    RouteRequest rq;
    rq.bases = Vec3{6000.0, -160000.0, 0.0};
    rq.gate_in = &gate;
    rq.exits = {&gate};
    rq.target = Vec3{6000.0, 60000.0, 0.0};
    rq.type = &a;
    rq.profile = Profile::Medium;
    const PlannedRoute clear = planner.Plan(rq, {}, nullptr);
    const PlannedRoute minded = planner.Plan(rq, high, nullptr);
    std::printf("    cost %.0f clear, %.0f past the believed base; exposure %.1f s\n", clear.cost, minded.cost, minded.exposure_s);
    SC_EXPECT(clear.ok && minded.ok);
    SC_EXPECT(minded.cost > clear.cost);
    SC_EXPECT_NEAR(minded.exposure_s, 0.0, 1e-9);
}

SC_TEST("routes (S7): without a map the lattice is coarser and fewer run-in headings are tried") {
    TuningTable tune;
    SC_EXPECT(tune.Get("route.mode") >= 2.0);
    SC_EXPECT(tune.Get("route.fallback_grid2_m") > tune.Get("route.fallback_grid_m"));
    SC_EXPECT(tune.Get("route.run_in_headings_nomap") < tune.Get("route.run_in_headings"));
    // A theatre without a map plans on its objects and the lattice: the same in both modes, fewer features in mode 2.
    AirWarData d1 = LoadOn("theatre_pannonia.json", "nato");
    d1.country = nullptr;
    AirWarData d2 = d1;
    d1.tuning.values["route.mode"] = 1.0;
    d2.tuning.values["route.mode"] = 2.0;
    RouteGround g1, g2;
    g1.Build(d1);
    g2.Build(d2);
    std::printf("    features without a map: %zu in mode 1, %zu in mode 2\n", g1.Features().size(), g2.Features().size());
    SC_EXPECT(!g2.HasGround());
    SC_EXPECT(g2.Features().size() < g1.Features().size());
}

SC_TEST("routes (S7): a flight drifts between its fixes, never more than the cap, the same every time; a fix ends it") {
    std::vector<RoutePoint> r(6);
    for (std::size_t i = 0; i < r.size(); ++i) r[i].pos = Vec3{0.0, 20000.0 * static_cast<double>(i), 0.0};
    r[2].feature = "settlement:x";  // a fix
    r[4].tag = "ip";
    const std::vector<Vec3> a = NavigationDrift(r, 0.01, 3000.0, 77);
    const std::vector<Vec3> b = NavigationDrift(r, 0.01, 3000.0, 77);
    const std::vector<Vec3> c = NavigationDrift(r, 0.01, 3000.0, 78);
    const std::vector<Vec3> off = NavigationDrift(r, 0.0, 3000.0, 77);
    SC_EXPECT(a.size() == r.size());
    const auto len = [](const Vec3& v) { return std::hypot(v.x, v.y); };
    SC_EXPECT_NEAR(len(a[0]), 0.0, 1e-12);
    SC_EXPECT_NEAR(len(a[1]), 200.0, 1e-9);   // 1 % of 20 km
    SC_EXPECT_NEAR(len(a[2]), 0.0, 1e-12);    // over its feature
    SC_EXPECT_NEAR(len(a[3]), 200.0, 1e-9);
    SC_EXPECT_NEAR(len(a[4]), 0.0, 1e-12);    // the IP, found by eye
    SC_EXPECT_NEAR(len(a[5]), 200.0, 1e-9);
    const std::vector<Vec3> capped = NavigationDrift(r, 0.2, 3000.0, 77);
    SC_EXPECT_NEAR(len(capped[1]), 3000.0, 1e-9);
    for (std::size_t i = 0; i < r.size(); ++i) {
        SC_EXPECT(a[i].x == b[i].x && a[i].y == b[i].y);
        SC_EXPECT(len(off[i]) == 0.0);
    }
    SC_EXPECT(std::fabs(a[1].x - c[1].x) + std::fabs(a[1].y - c[1].y) > 1e-6);
    // The factions' drift loads with its source.
    for (const char* f : {"nato", "warsaw_pact", "neutral", "rebels"}) {
        const AirWarData d = LoadOn("theatre_pannonia.json", f);
        std::printf("    %s drifts %.3f of the way between fixes\n", f, d.faction.doctrine.routing.nav_drift_share);
        SC_EXPECT(d.faction.doctrine.routing.nav_drift_share > 0.0 && d.faction.doctrine.routing.nav_drift_share < 0.05);
    }
}

SC_TEST("routes (S7): the route as flown: the place it left the session goes in, the rest moves on in time, indices follow") {
    Flight fl;
    fl.route.resize(4);
    for (std::size_t i = 0; i < 4; ++i) {
        fl.route[i].pos = Vec3{0.0, 36000.0 * static_cast<double>(i), 0.0};
        fl.route[i].t_h = 10.0 + 0.05 * static_cast<double>(i);  // 200 m/s
        fl.route[i].leg_mps = 200.0;
    }
    fl.route[2].tag = "target";
    fl.release_index = 2;
    ZoneEscape z;
    z.first = 2;
    z.last = 3;
    z.rejoin = 3;
    fl.escapes.push_back(z);
    // Seen last 10 km east of the line, half way along its second leg, a minute late.
    const double t = 10.075 + 60.0 / 3600.0;
    SC_EXPECT(FlownInto(fl, Vec3{10000.0, 54000.0, 0.0}, t));
    SC_EXPECT(fl.route.size() == 5 && fl.route[2].tag == "flown");
    SC_EXPECT_NEAR(fl.route[2].t_h, t, 1e-12);
    const double to_next = std::hypot(10000.0, 72000.0 - 54000.0) / 200.0 / 3600.0;
    SC_EXPECT_NEAR(fl.route[3].t_h, t + to_next, 1e-12);
    SC_EXPECT_NEAR(fl.route[4].t_h - fl.route[3].t_h, 0.05, 1e-12);  // the legs after it keep their own times
    SC_EXPECT(fl.release_index == 3 && fl.route[3].tag == "target");
    SC_EXPECT(fl.escapes[0].first == 3 && fl.escapes[0].last == 4 && fl.escapes[0].rejoin == 4);
    SC_EXPECT_NEAR(fl.route[0].t_h, 10.0, 1e-12);  // before it, as planned
    // After its last point: nothing to change.
    SC_EXPECT(!FlownInto(fl, Vec3{}, 99.0));
}

SC_TEST("routes (S7): the Pact's helicopters beyond their umbrella are not sent once the umbrella's worth is nothing") {
    AirWarData d = LoadOn("theatre_pannonia.json", "warsaw_pact");
    const double umbrella = d.tuning.Get("route.helo_umbrella_m");
    const auto beyond = [&](const std::vector<Package>& ps, int* helos) {
        int n = 0;
        *helos = 0;
        for (const Package& p : ps) {
            if (p.kind != MissionKind::HeloRaid) continue;
            ++*helos;
            for (const FrontDef& f : d.theatre.fronts) {
                for (const EntryPoint& e : f.entries) {
                    if (e.id == p.entry && Dist2d(e.pos, p.target_pos) > umbrella + 2000.0) ++n;
                }
            }
        }
        return n;
    };
    AirWarData none = d;
    none.tuning.values["route.helo_beyond_umbrella"] = 0.0;
    AirWarData open_sky = d;
    open_sky.tuning.values["route.helo_beyond_umbrella"] = 1.0;
    int h0 = 0, h1 = 0;
    const int b0 = beyond(PlanDays(none, 5, 3.0), &h0);
    const int b1 = beyond(PlanDays(open_sky, 5, 3.0), &h1);
    std::printf("    helicopter raids beyond the umbrella: %d of %d with it worth nothing, %d of %d with it free\n", b0, h0, b1, h1);
    SC_EXPECT(b0 == 0);
    SC_EXPECT(b1 >= b0);
}

SC_TEST("routes (S7): a planner who varies his axis comes the same way in less often than one who repeats it") {
    const AirWarData d = LoadOn("theatre_pannonia.json", "nato");
    int n_vary = 0, n_repeat = 0, same_vary = 0, same_repeat = 0;
    for (unsigned seed : {3u, 9u}) {
        int n = 0;
        same_vary += SameCorridorRuns(PlanDays(WithPlannerQuirk(d, "axis_vary"), seed, 3.0), &n);
        n_vary += n;
        same_repeat += SameCorridorRuns(PlanDays(WithPlannerQuirk(d, "axis_repeat"), seed, 3.0), &n);
        n_repeat += n;
    }
    std::printf("    the same way in as the last: %d of %d varying, %d of %d repeating\n", same_vary, n_vary, same_repeat, n_repeat);
    SC_EXPECT(n_vary > 5 && n_repeat > 5);
    SC_EXPECT(static_cast<double>(same_vary) / std::max(1, n_vary) < static_cast<double>(same_repeat) / std::max(1, n_repeat));
}

// ------------------------------------------------------------------------------------------------ learning (S7)

SC_TEST("learning (S7): a flight lost with nobody home to tell makes the staff suspect a defence where it was lost") {
    for (double mode : {0.0, 1.0}) {
        AirWarData d = LoadOn("theatre_pannonia.json", "warsaw_pact");
        d.tuning.values["learning.mode"] = mode;
        AirCampaign c;
        c.Init(d, 4);
        int played = 0;
        Vec3 lost_at;
        while (played == 0 && c.Now() < 96.0) {
            if (!c.Advance(96.0)) continue;
            const Package* p = c.PlayerMission();
            PackageOutcome o;
            o.package_id = p->id;
            o.played = true;
            const std::string own = d.theatre.objects[static_cast<std::size_t>(c.PlayerIndex())].id;
            lost_at = c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos + Vec3{3000.0, 2000.0, 0.0};
            for (std::size_t fi = 0; fi < p->flights.size(); ++fi) {
                for (int k = 0; k < p->flights[fi].count; ++k) {
                    LossRecord l;
                    l.flight = static_cast<int>(fi);
                    l.by = own;
                    l.where = lost_at;
                    o.losses.push_back(l);
                }
            }
            o.fired.push_back(own);  // it fired, but nobody came home to say so
            c.CompletePlayerMission(o, c.Now() + 0.2);
            ++played;
        }
        SC_EXPECT(played == 1);
        const double until = c.Now() + 12.0;
        while (c.Now() < until) {
            if (c.Advance(until)) c.SkipPlayerMission();
        }
        int suspected = 0;
        double nearest = 1e18;
        for (const IntelEntry& e : c.Intel().entries) {
            if (!e.ghost || e.label.find("a loss nobody saw") == std::string::npos) continue;
            ++suspected;
            nearest = std::min(nearest, Dist2d(e.pos, lost_at));
            SC_EXPECT(e.IsThreat());
        }
        std::printf("    learning.mode %.0f: %d suspected defences, the nearest %.1f km from the loss\n", mode, suspected,
                    suspected > 0 ? nearest / 1000.0 : -1.0);
        if (mode < 1.0) SC_EXPECT(suspected == 0);
        else SC_EXPECT(suspected > 0 && nearest < 4.0 * d.tuning.Get("learning.unexplained_sigma_m"));
    }
}

SC_TEST("learning (S7): the rebels' sympathisers bring word of our mobile batteries; not before S7, and not other factions") {
    int with = 0, without = 0, nato = 0;
    for (double mode : {0.0, 1.0}) {
        AirWarData d = LoadOn("theatre_pannonia.json", "rebels");
        d.tuning.values["learning.mode"] = mode;
        AirCampaign c;
        PlanDays(d, 6, 3.0, &c);
        (mode > 0.0 ? with : without) = c.SympathiserReports();
    }
    {
        AirCampaign c;
        PlanDays(LoadOn("theatre_pannonia.json", "nato"), 6, 3.0, &c);
        nato = c.SympathiserReports();
    }
    std::printf("    sympathisers' reports in three days: %d with S7, %d before it, %d for NATO\n", with, without, nato);
    SC_EXPECT(with > 0 && without == 0 && nato == 0);
}

SC_TEST("learning (S7): the weather by month: a winter war is mostly not clear; the same day's draw, a save's mode kept") {
    SC_EXPECT_NEAR(NotClearShare(15), 0.85, 1e-12);
    SC_EXPECT_NEAR(NotClearShare(200), 0.5, 1e-12);
    SC_EXPECT(NotClearShare(100) < 0.85 && NotClearShare(100) > 0.5);
    SC_EXPECT(NotClearShare(300) < 0.85 && NotClearShare(300) > 0.5);
    SC_EXPECT_NEAR(NotClearShare(15 + 365), NotClearShare(15), 1e-12);
    int clear[2] = {0, 0};
    for (int m = 0; m < 2; ++m) {
        AirWarData d = LoadOn("theatre_pannonia.json", "nato");
        d.theatre.day_of_year = 15;  // a war from the middle of January
        d.tuning.values["weather.mode"] = m;
        AirCampaign c;
        c.Init(d, 12);
        for (int day = 0; day < 90; ++day) clear[m] += c.WeatherOn(day + 1) == airwar::Weather::Clear ? 1 : 0;
    }
    std::printf("    clear days of 90 from mid-January: %d by the year's share, %d by the month's\n", clear[0], clear[1]);
    SC_EXPECT(clear[1] < clear[0]);
    SC_EXPECT(clear[1] <= 30);
    // A save keeps the modes; one made before S7 has neither and loads with both 0, the routes' mode as saved.
    const AirWarData d = LoadOn("theatre_pannonia.json", "rebels");
    AirCampaign n;
    PlanDays(d, 21, 1.0, &n);
    std::string saved = SaveCampaignJson(n, "air_war/theatre_pannonia.json", "air_war/faction_rebels.json");
    SC_EXPECT(saved.find("\"learning_mode\"") != std::string::npos && saved.find("\"weather_mode\"") != std::string::npos);
    AirCampaign back;
    std::string e;
    SC_EXPECT(LoadCampaignJson(saved, kData, back, &e));
    SC_EXPECT(JournalFnv(back) == JournalFnv(n));
    SC_EXPECT_NEAR(back.Data().tuning.Get("learning.mode"), 1.0, 1e-12);
    SC_EXPECT_NEAR(back.Data().tuning.Get("route.mode"), 2.0, 1e-12);
    for (const char* key : {"\"learning_mode\"", "\"weather_mode\""}) {
        const std::size_t at = saved.find(key);
        const std::size_t end = saved.find(',', at);
        if (at != std::string::npos && end != std::string::npos) saved.erase(at, end - at + 1);
    }
    AirCampaign old;
    SC_EXPECT(LoadCampaignJson(saved, kData, old, &e));
    SC_EXPECT_NEAR(old.Data().tuning.Get("learning.mode"), 0.0, 1e-12);
    SC_EXPECT_NEAR(old.Data().tuning.Get("weather.mode"), 0.0, 1e-12);
}

// ---------------------------------------------------------------- track F, F2.4: the routes read the air front

namespace {

// A straight front along north -75 km, the enemy's bases to the south, cut into two sectors at 150 km along it.
LineOfContact StraightFront() {
    FrontDef f;
    f.name = "test front";
    f.line = {Vec3{-100000.0, -75000.0, 0.0}, Vec3{100000.0, -75000.0, 0.0}};
    f.bases = Vec3{0.0, -160000.0, 0.0};
    SectorDef a, b;
    a.id = "a";
    a.from_m = 0.0;
    a.to_m = 150000.0;
    b.id = "b";
    b.from_m = 150000.0;
    b.to_m = 200000.0;
    f.sectors = {a, b};
    LineOfContact l;
    l.Build(f, 5000.0);
    return l;
}

bool SamePoints(const std::vector<RoutePoint>& x, const std::vector<RoutePoint>& y) {
    if (x.size() != y.size()) return false;
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (x[i].tag != y[i].tag || x[i].pos.x != y[i].pos.x || x[i].pos.y != y[i].pos.y || x[i].agl_m != y[i].agl_m || x[i].leg_mps != y[i].leg_mps) {
            return false;
        }
    }
    return true;
}

}  // namespace

SC_TEST("routes (F2.4): inside its sector's air front a low flight crosses high, lets down where it leaves it and climbs coming back") {
    TuningTable tune;
    RouteGround g;
    g.BuildFrom(RidgeCountry(), tune);
    const AircraftType a = TestJet();
    RouteRules rules;
    rules.crossing = "checkpoint_gate";
    rules.letdown_m = 40000.0;
    const RoutePlanner planner(g, tune, rules);
    const LineOfContact line = StraightFront();
    EntryPoint g1{"G1", Vec3{-6000.0, -75000.0, 0.0}}, g2{"G2", Vec3{30000.0, -75000.0, 0.0}};
    RouteRequest rq;
    rq.bases = Vec3{6000.0, -160000.0, 0.0};
    rq.gate_in = &g1;
    rq.exits = {&g1, &g2};
    rq.target = Vec3{6000.0, 60000.0, 0.0};
    rq.type = &a;
    rq.delivery = Delivery::PopUp;
    const PlannedRoute base = planner.Plan(rq, {}, nullptr);
    SC_EXPECT(base.ok);
    // The view off (mode 0), on with every sector at 0, and on with only the other sector forward: the route as built.
    const std::vector<double> none, zero = {0.0, 0.0}, other = {0.0, 20000.0}, here = {20000.0, 0.0};
    rq.air_front.line = &line;
    const std::vector<double>* views[] = {&none, &zero, &other};
    for (const std::vector<double>* v : views) {
        rq.air_front.depth_m = v;
        rq.air_front.helo_depth_m = v;
        const PlannedRoute pr = planner.Plan(rq, {}, nullptr);
        SC_EXPECT(SamePoints(pr.points, base.points) && pr.exposure_s == base.exposure_s && pr.length_m == base.length_m);
    }
    // The gates' sector forward by 20 km.
    rq.air_front.depth_m = &here;
    rq.air_front.helo_depth_m = &zero;
    const PlannedRoute pr = planner.Plan(rq, {}, nullptr);
    SC_EXPECT(pr.ok);
    std::string all;
    for (const RoutePoint& p : pr.points) all += p.tag + " ";
    std::printf("    %s\n", all.c_str());
    const std::vector<RoutePoint>& r = pr.points;
    const int gi = RouteIndexOf(r, "gate"), ld = RouteIndexOf(r, "letdown"), ip = RouteIndexOf(r, "ip"), cl = RouteIndexOf(r, "climb");
    int go = -1;
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (r[i].tag == "gate") go = static_cast<int>(i);
    }
    SC_EXPECT(r.size() > 3);
    if (r.size() <= 3) return;
    SC_EXPECT(r.front().tag == "entry" && r[1].tag == "checkpoint" && gi == 2);  // no let-down short of the gate
    SC_EXPECT(ld > gi && ld < ip);
    SC_EXPECT(cl > RouteIndexOf(r, "egress") && cl < go && r.back().tag == "exit" && r[r.size() - 2].tag == "gate");
    if (ld < 0 || cl < 0 || ip < 0) return;
    // Where it lets down and climbs: on the air front, 20 km beyond the line of contact.
    SC_EXPECT_NEAR(line.DistanceTo(r[static_cast<std::size_t>(ld)].pos), 20000.0, 5.0);
    SC_EXPECT_NEAR(line.DistanceTo(r[static_cast<std::size_t>(cl)].pos), 20000.0, 5.0);
    for (int i = 0; i < ld; ++i) {
        SC_EXPECT(r[static_cast<std::size_t>(i)].agl_m >= a.medium_alt_m && r[static_cast<std::size_t>(i)].leg_mps == a.cruise_mps);
    }
    for (int i = ld; i < cl; ++i) {
        const std::string& t = r[static_cast<std::size_t>(i)].tag;
        if (t == "pull_up" || t == "apex" || t == "target") continue;  // the attack's own heights
        SC_EXPECT(r[static_cast<std::size_t>(i)].agl_m == 60.0);
    }
    for (std::size_t i = static_cast<std::size_t>(cl); i < r.size(); ++i) SC_EXPECT(r[i].agl_m >= a.medium_alt_m);
    // The same way: the let-down and the climb lie on the legs of the route as built, which is no longer than it was.
    SC_EXPECT(pr.length_m <= base.length_m + 1.0);
    SC_EXPECT(pr.turns == base.turns && pr.home == base.home);
    // A target inside the air front: the let-down a leg short of the IP, the climb as far past the egress.
    std::vector<double> deep = {150000.0, 0.0};
    rq.air_front.depth_m = &deep;
    const PlannedRoute in = planner.Plan(rq, {}, nullptr);
    SC_EXPECT(in.ok);
    const int ld2 = RouteIndexOf(in.points, "letdown"), ip2 = RouteIndexOf(in.points, "ip"), eg2 = RouteIndexOf(in.points, "egress"),
              cl2 = RouteIndexOf(in.points, "climb");
    std::string all2;
    for (const RoutePoint& p : in.points) all2 += p.tag + " ";
    std::printf("    %s\n", all2.c_str());
    SC_EXPECT(ld2 == ip2 - 1 && cl2 == eg2 + 1);
    if (ld2 < 0 || ip2 < 0) return;
    SC_EXPECT(Dist2d(in.points[static_cast<std::size_t>(ld2)].pos, in.points[static_cast<std::size_t>(ip2)].pos) <= rules.letdown_m + 1.0);
    // A helicopter is untouched by the fixed-wing front; it holds behind its own (route.helo_holding_m behind it, never
    // nearer the line than route.helo_holding_floor_m).
    AircraftType h;
    h.id = "test_helo";
    h.helicopter = true;
    h.low_agl_m = 15.0;
    h.medium_alt_m = 1000.0;
    h.cruise_mps = 60.0;
    h.attack_mps = 60.0;
    TuningTable tune2 = tune;
    tune2.values["route.helo_holding_m"] = 30000.0;
    const RoutePlanner hp(g, tune2, rules);
    EntryPoint gh{"GH", Vec3{-30000.0, -75000.0, 0.0}};  // the B2 helicopter test's way in, on this front
    RouteRequest hq;
    hq.bases = Vec3{-30000.0, -160000.0, 0.0};
    hq.gate_in = &gh;
    hq.target = Vec3{-9000.0, -35000.0, 0.0};
    hq.type = &h;
    hq.ip_dist_m = 6000.0;
    hq.helicopter_hide = true;
    hq.delivery = Delivery::HoverFire;
    hq.air_front.line = &line;
    const auto holding = [&](double helo_front_m) {
        const std::vector<double> fw = {helo_front_m, 0.0}, hf = {helo_front_m, 0.0};
        hq.air_front.depth_m = &fw;
        hq.air_front.helo_depth_m = &hf;
        const PlannedRoute p = hp.Plan(hq, {}, nullptr);
        SC_EXPECT(p.ok && p.points.front().tag == "entry" && RouteIndexOf(p.points, "letdown") < 0);
        return p.ok ? Dist2d(p.points.front().pos, gh.pos) : -1.0;
    };
    SC_EXPECT_NEAR(holding(0.0), 30000.0, 1.0);
    SC_EXPECT_NEAR(holding(10000.0), 20000.0, 1.0);
    SC_EXPECT_NEAR(holding(20000.0), tune.Get("route.helo_holding_floor_m"), 1.0);
    SC_EXPECT_NEAR(holding(40000.0), tune.Get("route.helo_holding_floor_m"), 1.0);
    // With the data's values (15 km behind, the floor 15 km) the holding area stays where it was.
    const RoutePlanner hp0(g, tune, rules);
    const std::vector<double> hf = {20000.0, 0.0};
    hq.air_front.depth_m = &hf;
    hq.air_front.helo_depth_m = &hf;
    const PlannedRoute p0 = hp0.Plan(hq, {}, nullptr);
    SC_EXPECT(p0.ok);
    if (p0.ok) SC_EXPECT_NEAR(Dist2d(p0.points.front().pos, gh.pos), tune.Get("route.helo_holding_m"), 1.0);
}
