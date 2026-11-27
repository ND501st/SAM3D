// Track F, F3.6: U1 in the player's sector and the march on the roads (simcore/ground_war/ground_war.hpp; docs/fronts/
// 03_GROUND_WAR.md sections 3.3 and 7). A hand war on a straight 100 km line: a division's regiments built with their
// shares and echelons, up at once in the player's sector, its strength always the sum of theirs, the regiment the axis
// meets losing more, asking, relieved by the second echelon's, and down 6 h after the need ends with strength, posture and
// place kept. The route service on the nation: asked once per pair of anchors and the overlay's version, a bridge down in
// the air war cutting its crossing and asked again. And U1 against U0 over 20 seeds on every faction's front: the same war.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/ground_war.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;
using namespace simcore::groundwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

const GroundTables& Tables() {
    static GroundTables t;
    static bool loaded = false;
    if (!loaded) {
        std::string e;
        if (!LoadGroundTables(kData, t, &e)) std::printf("    %s\n", e.c_str());
        loaded = true;
    }
    return t;
}

Vec3 Km(double e, double n) { return Vec3{e * 1000.0, n * 1000.0, 0.0}; }

command::OobUnit Unit(const std::string& id, const std::string& kind, int tanks, int men, const std::string& parent = "") {
    command::OobUnit u;
    u.id = id;
    u.name = id;
    u.kind = kind;
    u.parent = parent;
    if (tanks > 0) u.equipment.push_back({"t55", tanks});
    u.personnel.conscripts = men;
    u.personnel.basis = "test";
    return u;
}

GroundPlace At(const std::string& id, double e_km, double n_km) {
    GroundPlace p;
    p.at = id;
    p.pos = Km(e_km, n_km);
    return p;
}

// A straight 100 km line along the east axis from (0, 0), the enemy south: sectors west (0-50 km) and east (50-100 km).
FrontDef StraightFront() {
    FrontDef f;
    f.faction = "test";
    f.name = "test front";
    f.line = {Km(0, 0), Km(100, 0)};
    f.bases = Km(50, -50);
    SectorDef w;
    w.id = "west";
    w.name = "west";
    w.from_m = 0.0;
    w.to_m = 50000.0;
    SectorDef e;
    e.id = "east";
    e.name = "east";
    e.from_m = 50000.0;
    e.to_m = 100000.0;
    f.sectors = {w, e};
    return f;
}

// Test lines a regiment reaches in hours (not the doctrine's 2/3 and 1/2).
EffectivenessDef TestLines() {
    EffectivenessDef e;
    e.kind = EffectivenessKind::Lines;
    e.degraded_below = 0.95;
    e.ineffective_below = 0.9;
    e.hq_hit_ineffective_below = 0.6;
    return e;
}

// A red division attacking north at 62 km (into the east sector's western half); our division holding the east sector
// with three tank regiments and an artillery regiment under it.
std::shared_ptr<GroundWarData> HandWar() {
    auto d = std::make_shared<GroundWarData>();
    d->faction = "test";
    d->tables = Tables();
    d->tables.draw_sigma = 0.0;
    d->tables.picture.truth = true;
    d->enemy.faction = "test";
    d->enemy.name = "the red army";
    d->enemy.doctrine.effectiveness = TestLines();
    d->enemy.doctrine.caps.breakthrough_km_per_day = 30.0;
    d->enemy.doctrine.caps.depth_km_per_day = 50.0;
    d->enemy.doctrine.caps.broken_km_per_day = 70.0;
    d->enemy.plan.kind = PlanKind::Offensive;
    d->enemy.plan.start_after_h = 2.0;
    d->ours.name = "our army";
    d->ours.doctrine.effectiveness = TestLines();
    d->ours.doctrine.caps = d->enemy.doctrine.caps;
    DeploymentDef dep;
    dep.adversary = "test";
    d->ours.deployments.push_back(dep);
    d->enemy.oob.units.push_back(Unit("red_div", "division", 700, 10000));
    ObjectiveDef goal;
    goal.id = "east_obj";
    goal.name = "the goal";
    goal.place = At("east_goal", 62, 200);
    d->enemy.plan.objectives.push_back(goal);
    AxisDef ax;
    ax.id = "east";
    ax.name = "the east axis";
    ax.main = true;
    ax.route = {At("east_start", 62, -20), At("east_crossing", 62, 0), At("east_goal", 62, 200)};
    ax.first_echelon = {"red_div"};
    ax.objective = goal.id;
    ax.sector = "east";
    d->enemy.plan.axes.push_back(ax);
    d->ours.oob.units.push_back(Unit("blue_army", "army", 0, 500));
    d->ours.oob.units.push_back(Unit("blue_div", "division", 10, 1000, "blue_army"));
    d->ours.oob.units.push_back(Unit("blue_r1", "regiment", 50, 3000, "blue_div"));
    d->ours.oob.units.push_back(Unit("blue_r2", "regiment", 50, 3000, "blue_div"));
    d->ours.oob.units.push_back(Unit("blue_r3", "regiment", 50, 3000, "blue_div"));
    d->ours.oob.units.push_back(Unit("blue_art", "regiment", 0, 1500, "blue_div"));
    DeployedUnitDef u;
    u.unit = "blue_div";
    u.role = DeployRole::FirstEchelon;
    u.sectors = {"east"};
    u.place = At("blue_div_place", 75, 40);
    d->ours.deployments.front().units.push_back(u);
    return d;
}

int FormationOf(const GroundWar& g, const std::string& id) {
    for (std::size_t i = 0; i < g.Formations().size(); ++i) {
        if (g.Formations()[i].unit.id == id) return static_cast<int>(i);
    }
    return -1;
}
int RegimentOf(const GroundWar& g, const std::string& id) {
    for (std::size_t i = 0; i < g.Regiments().size(); ++i) {
        if (g.Regiments()[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

struct Logged {
    std::vector<GroundLogEntry> all;
    void Take(GroundWar& g) {
        all.insert(all.end(), g.Log().begin(), g.Log().end());
        g.ClearLog();
    }
    const GroundLogEntry* First(GroundOrder o, int formation = -1) const {
        for (const GroundLogEntry& e : all) {
            if (e.what == GroundNote::Order && e.order == o && (formation < 0 || e.formation == formation)) return &e;
        }
        return nullptr;
    }
    int Count(GroundOrder o) const {
        int n = 0;
        for (const GroundLogEntry& e : all) n += e.what == GroundNote::Order && e.order == o ? 1 : 0;
        return n;
    }
};

double SumOfRegiments(const GroundWar& g, const Formation& f) {
    double s = 0.0;
    for (int k = 0; k < f.regiment_count; ++k) s += g.RegimentStrength(static_cast<std::size_t>(f.first_regiment + k));
    return s;
}

}  // namespace

SC_TEST("ground levels (F3.6): a division's regiments: shares, echelons, up in the player's sector, the one hit asks and is relieved, down after 6 h") {
    std::shared_ptr<GroundWarData> d = HandWar();
    LineOfContact line;
    line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;
    GroundEnv env;
    env.theatre = &theatre;
    env.player_sector = 1;  // the east sector
    GroundWar g;
    std::string e;
    SC_EXPECT(g.Build(d, line, theatre, nullptr, 7, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const int blue = FormationOf(g, "blue_div");
    SC_EXPECT(blue >= 0);
    if (blue < 0) return;
    const Formation& f = g.Formations()[static_cast<std::size_t>(blue)];
    // Four regiments, the artillery the support; their shares by their strength, summing to 1; two in the first echelon.
    SC_EXPECT(f.regiment_count == 4);
    if (f.regiment_count != 4) return;
    const int r1 = RegimentOf(g, "blue_r1"), r2 = RegimentOf(g, "blue_r2"), r3 = RegimentOf(g, "blue_r3"), art = RegimentOf(g, "blue_art");
    SC_EXPECT(r1 >= 0 && r2 >= 0 && r3 >= 0 && art >= 0);
    if (r1 < 0 || r2 < 0 || r3 < 0 || art < 0) return;
    const std::vector<Regiment>& regs = g.Regiments();
    double parts = 0.0;
    for (int k = 0; k < f.regiment_count; ++k) parts += regs[static_cast<std::size_t>(f.first_regiment + k)].part;
    SC_EXPECT_NEAR(parts, 1.0, 1e-12);
    SC_EXPECT(regs[static_cast<std::size_t>(art)].support && !regs[static_cast<std::size_t>(r1)].support);
    SC_EXPECT(regs[static_cast<std::size_t>(r1)].first && regs[static_cast<std::size_t>(r2)].first && !regs[static_cast<std::size_t>(r3)].first);
    SC_EXPECT(regs[static_cast<std::size_t>(r1)].part > regs[static_cast<std::size_t>(art)].part);
    SC_EXPECT(g.RegimentCommanders().size() == regs.size());
    SC_EXPECT(g.RegimentCommanders()[static_cast<std::size_t>(regs[static_cast<std::size_t>(r1)].cmdr)].senior == f.cmdr);
    const std::size_t cap = g.QueueCapacity();
    Logged log;
    bool conserved = true, r1_harder = true;
    double worst_gap = 0.0;
    int hours = 0;
    for (int h = 1; h <= 24 * 2; ++h) {
        g.Step(static_cast<double>(h), env, line);
        log.Take(g);
        hours = h;
        const double gap = std::fabs(SumOfRegiments(g, f) - f.unit.strength) / std::max(1e-9, f.unit.strength);
        worst_gap = std::max(worst_gap, gap);
        conserved = conserved && gap < 1e-9;
        // Until the relief: the regiment the axis meets loses more than its neighbour in the first echelon.
        if (g.Regiments()[static_cast<std::size_t>(r1)].first && h > 6) {
            r1_harder = r1_harder && g.RegimentFraction(static_cast<std::size_t>(r1)) <= g.RegimentFraction(static_cast<std::size_t>(r2)) + 1e-12;
        }
        if (log.First(GroundOrder::RegimentRelieved) != nullptr) break;
    }
    std::printf("    %d h: the division at %.3f; r1 %.3f r2 %.3f r3 %.3f art %.3f (worst sum gap %.1e)\n", hours, f.unit.Fraction(),
                g.RegimentFraction(static_cast<std::size_t>(r1)), g.RegimentFraction(static_cast<std::size_t>(r2)),
                g.RegimentFraction(static_cast<std::size_t>(r3)), g.RegimentFraction(static_cast<std::size_t>(art)), worst_gap);
    SC_EXPECT(conserved);
    SC_EXPECT(r1_harder);
    const GroundLogEntry* up = log.First(GroundOrder::LevelUp, blue);
    SC_EXPECT(up != nullptr && up->h <= 1.0);  // at once
    SC_EXPECT(f.level == UnitLevel::U1);
    const GroundLogEntry* asked = log.First(GroundOrder::RegimentAsked, blue);
    const GroundLogEntry* sent = log.First(GroundOrder::RegimentReliefOrdered, blue);
    const GroundLogEntry* done = log.First(GroundOrder::RegimentRelieved, blue);
    SC_EXPECT(asked != nullptr && sent != nullptr && done != nullptr);
    if (asked == nullptr || sent == nullptr || done == nullptr) {
        for (const GroundLogEntry& l : log.all) {
            if (l.what == GroundNote::Order) std::printf("      h %5.0f %-24s %3d %3d %3d %8.3f\n", l.h, GroundOrderName(l.order), l.formation, l.regiment, l.other_regiment, l.value);
        }
        return;
    }
    std::printf("    r1 asked at h %.0f (%.3f), r3 sent at h %.0f (due in %.1f h), relieved at h %.0f\n", asked->h, asked->value, sent->h, sent->value, done->h);
    SC_EXPECT(asked->regiment == r1);
    SC_EXPECT(sent->regiment == r1 && sent->other_regiment == r3 && sent->h >= asked->h);
    SC_EXPECT(done->regiment == r1 && done->other_regiment == r3 && done->h >= sent->h + sent->value - 1.0);
    SC_EXPECT(sent->value > 0.0);  // a march at F3.4's rule (no country map here)
    SC_EXPECT(g.Regiments()[static_cast<std::size_t>(r3)].first && !g.Regiments()[static_cast<std::size_t>(r1)].first);
    SC_EXPECT(g.Regiments()[static_cast<std::size_t>(r1)].rotations == 1);
    // Out of the player's area: down 6 h later, nothing of the formation changed by it (strength, posture, place); its
    // regiments' shares stand still at U0 while it fights on.
    env.player_sector = -1;
    const int out_h = hours;
    int down_h = -1;
    std::vector<double> at_down;
    for (int h = out_h + 1; h <= out_h + 10 && down_h < 0; ++h) {
        g.Step(static_cast<double>(h), env, line);
        log.Take(g);
        if (f.level == UnitLevel::U0) {
            down_h = h;
            for (int k = 0; k < f.regiment_count; ++k) at_down.push_back(g.Regiments()[static_cast<std::size_t>(f.first_regiment + k)].part);
        }
    }
    const GroundLogEntry* down = log.First(GroundOrder::LevelDown, blue);
    SC_EXPECT(down != nullptr && down_h > 0);
    if (down == nullptr || down_h < 0) return;
    std::printf("    out of need from h %d, down at h %.0f\n", out_h, down->h);
    SC_EXPECT(down->h >= out_h + Tables().levels_down_after_h - 1e-9 && down->h <= out_h + Tables().levels_down_after_h + 1.0 + 1e-9);
    const double strength = f.unit.strength;
    const Posture posture = f.unit.posture;
    const Vec3 pos = f.pos;
    SC_EXPECT_NEAR(SumOfRegiments(g, f), f.unit.strength, 1e-9 * std::max(1.0, f.unit.strength));
    g.Step(static_cast<double>(down_h + 1), env, line);
    log.Take(g);
    SC_EXPECT(f.unit.strength <= strength + 1e-9);
    SC_EXPECT(f.unit.posture == posture || f.spent);
    SC_EXPECT(airwar::Dist2d(f.pos, pos) < 1.0 || f.spent);
    for (int k = 0; k < f.regiment_count; ++k) {
        SC_EXPECT(g.Regiments()[static_cast<std::size_t>(f.first_regiment + k)].part == at_down[static_cast<std::size_t>(k)]);
    }
    // And up again at once when it is needed (unless it is out of the fight by now), with the shares it had.
    env.player_sector = 1;
    g.Step(static_cast<double>(down_h + 2), env, line);
    log.Take(g);
    SC_EXPECT(f.level == UnitLevel::U1 || f.spent);
    if (f.level == UnitLevel::U1) SC_EXPECT(log.Count(GroundOrder::LevelUp) == 2);
    for (int k = 0; k < f.regiment_count && f.level == UnitLevel::U1; ++k) {
        SC_EXPECT_NEAR(g.Regiments()[static_cast<std::size_t>(f.first_regiment + k)].part, at_down[static_cast<std::size_t>(k)], 0.02);
    }
    SC_EXPECT(g.QueueCapacity() == cap);
}

SC_TEST("ground levels (F3.6): the march on the roads: one search a pair of anchors and overlay, a bridge down asked again and longer") {
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json", d, &e));
    auto gd = std::make_shared<GroundWarData>();
    SC_EXPECT(LoadGroundWarData(kData, "warsaw_pact", *gd, &e));
    const FrontDef* front = d.theatre.FrontFor(d.faction.front);
    SC_EXPECT(front != nullptr && d.country != nullptr);
    if (front == nullptr || d.country == nullptr) return;
    LineOfContact line;
    line.Build(*front, d.tuning.Get("air_front.station_m"));
    GroundWar g;
    SC_EXPECT(g.Build(gd, line, d.theatre, d.country.get(), 7, &e));
    SC_EXPECT(g.Roads() != nullptr);
    if (g.Roads() == nullptr) return;
    nav::Anchors anchors;
    anchors.AddCountry(d.country->Def());
    anchors.AddTheatre(d.theatre);
    std::vector<airwar::Weather> weather(30, airwar::Weather::Clear);
    std::vector<ObjectState> truth(d.theatre.objects.size());
    for (ObjectState& s : truth) s.function = 1.0;
    GroundEnv env;
    env.theatre = &d.theatre;
    env.weather = &weather;
    env.truth = &truth;
    // A pair of our towns whose road between them crosses a theatre bridge.
    nav::Mobility col;
    col.id = "probe";
    col.road_mps = {6.0, 6.0, 6.0};
    col.cross_country_mps = 3.0;
    const char* const pairs[][2] = {{"dravec", "mezovar"}, {"capital", "mezovar"}, {"kisvar", "novo_varosd"}, {"capital", "dravec"},
                                    {"mezovar", "kisvar"},  {"capital", "kisvar"},  {"saldanj", "kisvar"}};
    const nav::Anchor* a = nullptr;
    const nav::Anchor* b = nullptr;
    int bridge = -1;
    for (const auto& p : pairs) {
        const nav::Anchor* x = anchors.Find(p[0]);
        const nav::Anchor* y = anchors.Find(p[1]);
        if (x == nullptr || y == nullptr) continue;
        nav::RouteRequest q;
        q.from = x->at;
        q.to = y->at;
        q.mobility = &col;
        const nav::Route r = g.Roads()->Find(q, nav::Overlay());
        for (std::size_t o = 0; o < d.theatre.objects.size() && bridge < 0; ++o) {
            if (d.theatre.objects[o].kind != ObjectKind::Bridge) continue;
            double dist = 0.0;
            const nav::NavCrossing* c = g.Roads()->NearestCrossing({d.theatre.objects[o].pos.x, d.theatre.objects[o].pos.y}, &dist);
            if (c == nullptr || dist > 100.0) continue;
            for (const std::string& used : r.crossings) {
                if (used == c->id) bridge = static_cast<int>(o);
            }
        }
        if (bridge >= 0) {
            a = x;
            b = y;
            break;
        }
    }
    SC_EXPECT(a != nullptr && b != nullptr && bridge >= 0);
    if (a == nullptr || b == nullptr || bridge < 0) return;
    const Vec3 pa{a->at.east_m, a->at.north_m, 0.0}, pb{b->at.east_m, b->at.north_m, 0.0};
    // A day's hour and a night's, by the theatre's clock.
    double night_h = -1.0, day_h = -1.0;
    for (int k = 0; k < 48; ++k) {
        const airwar::DayPart part = airwar::TheatrePartAt(d.theatre, d.theatre.start_hour + k + 0.5);
        if (part == airwar::DayPart::Night && night_h < 0.0) night_h = k;
        if (part == airwar::DayPart::Day && day_h < 0.0) day_h = k;
    }
    SC_EXPECT(night_h >= 0.0 && day_h >= 0.0);
    const double h1 = g.TestRoadHours(a->id, pa, b->id, pb, day_h, env);
    const double h2 = g.TestRoadHours(a->id, pa, b->id, pb, day_h, env);
    SC_EXPECT(h1 > 0.0 && std::fabs(h1 - h2) < 1e-12);
    SC_EXPECT(g.RouteStats().asks == 2 && g.RouteStats().finds == 1 && g.RouteStats().hits == 1);
    // Its time: the road at the column's day speed with its halts, no faster than the straight line at the day's top speed.
    const double straight_km = airwar::Dist2d(pa, pb) / 1000.0;
    SC_EXPECT(h1 >= straight_km / Tables().road_day_kmh.hi);
    // By night slower, on the same route.
    const double hn = g.TestRoadHours(a->id, pa, b->id, pb, night_h, env);
    SC_EXPECT(hn > h1 && g.RouteStats().finds == 1);
    const std::uint64_t version = g.RoadOverlay().version;
    truth[static_cast<std::size_t>(bridge)].function = 0.0;  // the air war drops the bridge
    const double h3 = g.TestRoadHours(a->id, pa, b->id, pb, day_h, env);
    SC_EXPECT(g.RoadOverlay().version > version);
    SC_EXPECT(g.RouteStats().finds == 2);  // asked again on the new overlay
    SC_EXPECT(h3 > h1);                    // a detour, or the river crossed where the bridge was (crossing_cut_h)
    std::printf("    %s -> %s over %s: %.2f h by day, %.2f h by night, %.2f h with the bridge down (straight %.1f km, x1.3 at the day's speed %.2f h)\n",
                a->id.c_str(), b->id.c_str(), d.theatre.objects[static_cast<std::size_t>(bridge)].id.c_str(), h1, hn, h3, straight_km,
                straight_km * Tables().march_road_factor / Tables().road_day_kmh.Mid());
    // Unchanged truth: no new search.
    const double h4 = g.TestRoadHours(a->id, pa, b->id, pb, day_h, env);
    SC_EXPECT(std::fabs(h4 - h3) < 1e-12 && g.RouteStats().finds == 2);
    // The bridge back up: the old overlay's route again, searched afresh on the new version.
    truth[static_cast<std::size_t>(bridge)].function = 1.0;
    const double h5 = g.TestRoadHours(a->id, pa, b->id, pb, day_h, env);
    SC_EXPECT(std::fabs(h5 - h1) < 1e-9);
}

SC_TEST("ground levels (F3.6): U1 against U0 over 20 seeds on every faction's front: the same expected war") {
    // The band: each side's loss share and the axes' summed depth at the day-10 end, U1's mean over 20 seeds within 10% of
    // U0's (or 0.02 of loss, 3 km of depth where small). U1 conserves the formation's strength, power and losses by
    // construction; it parts from U0 only through the regiments' reports and asks waking their commanders earlier, the
    // clamped cohesion of a regiment untouched, and the draws those decisions consume.
    std::printf("    %-12s %-6s %10s %10s %10s %10s %10s %10s %6s %6s %6s\n", "faction", "sector", "depth_u0", "depth_u1", "ours_u0", "ours_u1", "enemy_u0",
                "enemy_u1", "ups", "rdec", "rot");
    for (const char* faction : {"warsaw_pact", "nato", "neutral", "rebels"}) {
        std::string e;
        AirWarData d;
        SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", std::string("air_war/faction_") + faction + ".json", d, &e));
        auto gd = std::make_shared<GroundWarData>();
        SC_EXPECT(LoadGroundWarData(kData, faction, *gd, &e));
        const FrontDef* front = d.theatre.FrontFor(d.faction.front);
        SC_EXPECT(front != nullptr && d.country != nullptr);
        if (front == nullptr || d.country == nullptr) return;
        LineOfContact line0;
        line0.Build(*front, d.tuning.Get("air_front.station_m"));
        // The player's battery's sector, where the campaign deploys it on this front.
        const TheatreObject* pb = d.theatre.PlayerBattery();
        SC_EXPECT(pb != nullptr);
        if (pb == nullptr) return;
        const int sector = line0.SectorOf(front->has_battery_position ? front->battery_position : pb->pos);
        SC_EXPECT(sector >= 0);
        std::vector<airwar::Weather> weather(30, airwar::Weather::Clear);
        std::vector<ObjectState> truth(d.theatre.objects.size());
        for (ObjectState& s : truth) s.function = 1.0;
        double depth[2] = {0, 0}, ours[2] = {0, 0}, enemy[2] = {0, 0};
        int ups = 0, rdec = 0, rot = 0;
        const int seeds = 20;
        for (int s = 0; s < seeds; ++s) {
            for (int lv = 0; lv < 2; ++lv) {
                LineOfContact line;
                line.Build(*front, d.tuning.Get("air_front.station_m"));
                GroundWar g;
                SC_EXPECT(g.Build(gd, line, d.theatre, d.country.get(), static_cast<unsigned>(100 + s), &e));
                GroundEnv env;
                env.theatre = &d.theatre;
                env.weather = &weather;
                env.truth = &truth;
                env.player_sector = lv == 1 ? sector : -1;
                g.Step(24.0 * 10, env, line);
                for (const Axis& x : g.Axes()) depth[lv] += x.depth_m / 1000.0;
                double str[2] = {0, 0}, base[2] = {0, 0};
                for (const Formation& f : g.Formations()) {
                    const int side = f.unit.side == Side::Ours ? 0 : 1;
                    str[side] += f.unit.strength;
                    base[side] += f.base_establishment;
                }
                ours[lv] += base[0] > 0.0 ? 1.0 - str[0] / base[0] : 0.0;
                enemy[lv] += base[1] > 0.0 ? 1.0 - str[1] / base[1] : 0.0;
                if (lv == 1) {
                    for (const Formation& f : g.Formations()) ups += f.ups;
                    rdec += static_cast<int>(g.Profile().regiment_decisions);
                    for (const Regiment& r : g.Regiments()) rot += r.rotations;
                }
            }
        }
        for (int lv = 0; lv < 2; ++lv) {
            depth[lv] /= seeds;
            ours[lv] /= seeds;
            enemy[lv] /= seeds;
        }
        std::printf("    %-12s %-6d %10.2f %10.2f %10.4f %10.4f %10.4f %10.4f %6d %6d %6d\n", faction, sector, depth[0], depth[1], ours[0], ours[1], enemy[0],
                    enemy[1], ups, rdec, rot);
        SC_EXPECT(std::fabs(depth[1] - depth[0]) <= 0.10 * std::max(depth[0], 30.0));
        SC_EXPECT(std::fabs(ours[1] - ours[0]) <= 0.10 * std::max(ours[0], 0.2));
        SC_EXPECT(std::fabs(enemy[1] - enemy[0]) <= 0.10 * std::max(enemy[0], 0.2));
    }
}
