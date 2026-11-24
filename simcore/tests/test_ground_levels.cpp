// Track F, F3.6: U1 in the player's sector and the march on the roads (simcore/ground_war/ground_war.hpp; docs/fronts/
// 03_GROUND_WAR.md sections 3.3 and 7). A hand war on a straight 100 km line: a division's regiments built with their
// shares and echelons, up at once in the player's sector, its strength always the sum of theirs, the regiment the axis
// meets losing more, asking, relieved by the second echelon's, and down 6 h after the need ends with strength, posture and
// place kept. The route service on the nation: asked once per pair of anchors and the overlay's version, a bridge down in
// the air war cutting its crossing and asked again. And U1 against U0 over 20 seeds on every faction's front: the same war.
// F5.2 (F8.3): U2, the regiments at U1 near the battery as the session's battalions and companies, ours by our models and
// the enemy's never; the enemy's listed for drawing only within sight, by day and by night; nothing out of a session; the
// front's distant fire with its retarded time and level, and its glow only at night.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/ground_war.hpp"
#include "simcore/ground_war/session_units.hpp"
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

// ------------------------------------------------------------------------------------------------------------ U2 (F5.2)

namespace {

// The hand war with the red division's three regiments under it (U1 needs two with tanks).
std::shared_ptr<GroundWarData> HandWarWithRedRegiments() {
    std::shared_ptr<GroundWarData> d = HandWar();
    d->enemy.oob.units.front().equipment.clear();
    d->enemy.oob.units.front().equipment.push_back({"t55", 40});
    for (const char* id : {"red_r1", "red_r2", "red_r3"}) d->enemy.oob.units.push_back(Unit(id, "regiment", 95, 2500, "red_div"));
    return d;
}

}  // namespace

SC_TEST("ground levels (F5.2): U2 near the battery: the regiments at U1 as battalions at their places, ours by our models, the enemy's never; none at U0") {
    std::shared_ptr<GroundWarData> d = HandWarWithRedRegiments();
    LineOfContact line;
    line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;
    GroundEnv env;
    env.theatre = &theatre;
    env.player_sector = 1;
    GroundWar g;
    std::string e;
    SC_EXPECT(g.Build(d, line, theatre, nullptr, 7, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    g.Step(6.0, env, line);
    const std::vector<std::string> ours = OurTypes(*d);
    SC_EXPECT(std::find(ours.begin(), ours.end(), "t55") != ours.end());
    const Vec3 battery = Km(65, 8);
    const double radius_m = 20000.0;
    const std::vector<U2Unit> u2 = U2Near(g, line, battery, radius_m, ours);
    int n_ours = 0, n_enemy = 0, vehicles = 0;
    for (const U2Unit& u : u2) {
        (u.side == Side::Ours ? n_ours : n_enemy) += 1;
        vehicles += u.vehicles;
        SC_EXPECT(Dist2d(u.place, battery) <= radius_m + 1e-6);
        SC_EXPECT(u.vehicles >= 1 && (u.kind == "battalion" || u.kind == "company"));
        // Ours by our equipment's models; the enemy's never one of ours (its T-55s are its faction's variant).
        if (u.side == Side::Ours) {
            SC_EXPECT(u.model == u.type && std::binary_search(ours.begin(), ours.end(), u.model));
            SC_EXPECT(line.DistanceTo(u.place) > 0.0);  // behind the line of contact, our side
        } else {
            SC_EXPECT(u.model == "t55_test" && !std::binary_search(ours.begin(), ours.end(), u.model));
            SC_EXPECT(line.DistanceTo(u.place) < 0.0);  // its side
        }
        SC_EXPECT(std::fabs(std::hypot(u.along.x, u.along.y) - 1.0) < 1e-9);
    }
    std::printf("    at h 6: %zu units near the battery (%d ours, %d the enemy's), %d vehicles\n", u2.size(), n_ours, n_enemy, vehicles);
    SC_EXPECT(n_ours >= 2 && n_enemy >= 1);
    // Their vehicles follow the regiment's strength.
    for (const U2Unit& u : u2) {
        if (u.side != Side::Ours) continue;
        SC_EXPECT(u.vehicles <= 50 && static_cast<double>(u.vehicles) >= std::floor(50.0 * g.RegimentFraction(static_cast<std::size_t>(u.reg))));
    }
    // A battery far behind the line has none; the same war out of the player's area comes down to U0 and has none either.
    SC_EXPECT(U2Near(g, line, Km(65, 80), radius_m, ours).empty());
    env.player_sector = -1;
    g.Step(6.0 + Tables().levels_down_after_h + 2.0, env, line);
    SC_EXPECT(U2Near(g, line, battery, radius_m, ours).empty());
    // The enemy's model for a type of ours is its faction's; for one not ours, its own.
    SC_EXPECT(U2Model("bmp1", Side::Enemy, "warsaw_pact", {"bmp1", "t55"}) == "bmp1_warsaw_pact");
    SC_EXPECT(U2Model("t72", Side::Enemy, "warsaw_pact", {"bmp1", "t55"}) == "t72");
    SC_EXPECT(U2Model("bmp1", Side::Ours, "warsaw_pact", {"bmp1", "t55"}) == "bmp1");
}

SC_TEST("ground levels (F5.2): U2's movers: deployed in line, the enemy's listed only within sight by day and by night, none once out of need, a moved place marched") {
    std::shared_ptr<GroundWarData> d = HandWarWithRedRegiments();
    nav::SessionNav nav;  // no roads: a march goes straight across country
    SessionUnits su;
    su.Start(*d);
    std::vector<U2Unit> units(2);
    units[0].id = "red_r1/t72/1";
    units[0].side = Side::Enemy;
    units[0].type = "t72";
    units[0].model = "t72";
    units[0].kind = "battalion";
    units[0].vehicles = 31;
    units[0].place = Vec3{5000.0, 0.0, 0.0};
    units[0].along = Vec3{1.0, 0.0, 0.0};
    units[1].id = "blue_r1/t55/1";
    units[1].side = Side::Ours;
    units[1].type = units[1].model = "t55";
    units[1].kind = "company";
    units[1].vehicles = 10;
    units[1].place = Vec3{0.0, 3000.0, 0.0};
    units[1].along = Vec3{1.0, 0.0, 0.0};
    su.Apply(units, nav, 0.0);
    SC_EXPECT(su.Made() == 2 && su.Near() == 2);
    const nav::Mover* red = nav.GetMovers().Find("red_r1/t72/1");
    SC_EXPECT(red != nullptr && red->ground_war && red->enemy && red->state == nav::MoverState::Arrived);
    if (red == nullptr) return;
    // Deployed in line across its place, its vehicles 50 m apart.
    SC_EXPECT_NEAR(red->route.length_m, 30 * SessionUnits::kSpacingM, 1e-9);
    const MapPoint mid = nav::PointAlong(red->route, 0.5 * red->route.length_m);
    SC_EXPECT(std::hypot(mid.east_m - 5000.0, mid.north_m) < 1e-6);
    const auto listed = [&nav](const std::string& id, bool* enemy = nullptr, std::string* model = nullptr) {
        int n = 0;
        for (const nav::SessionNav::Seen& v : nav.Visible()) {
            if (v.mover != id) continue;
            ++n;
            if (enemy != nullptr) *enemy = v.enemy;
            if (model != nullptr) *model = v.model;
        }
        return n;
    };
    const double day = nav::VisualRangeM(40000.0, 30.0, nav.Range());
    const double night = nav::VisualRangeM(40000.0, -20.0, nav.Range());
    std::printf("    visual range: %.0f m by day, %.0f m by night\n", day, night);
    const std::vector<MapPoint> eye = {MapPoint{0.0, 0.0}};
    // By day both, the enemy's as the enemy's with its own model, ours as ours.
    nav.Step(1.0, 1.0, eye, day);
    bool enemy = false;
    std::string model;
    SC_EXPECT(listed("red_r1/t72/1", &enemy, &model) == 31 && enemy && model == "t72");
    SC_EXPECT(listed("blue_r1/t55/1", &enemy, &model) == 10 && !enemy && model == "t55");
    // By night neither, 5 and 3 km off; the enemy's close enough again only within the night's range.
    nav.Step(2.0, 1.0, eye, night);
    SC_EXPECT(listed("red_r1/t72/1") == 0 && listed("blue_r1/t55/1") == 0);
    nav.Step(3.0, 1.0, {MapPoint{5000.0, 1000.0}}, night);
    SC_EXPECT(listed("red_r1/t72/1") == 31);
    // Out of need: nothing listed of it, however close, until it is near again.
    su.Apply({units[1]}, nav, 60.0);
    SC_EXPECT(su.Near() == 1);
    nav.Step(4.0, 1.0, {MapPoint{5000.0, 0.0}}, day);
    SC_EXPECT(listed("red_r1/t72/1") == 0);
    // Near again with its place moved 4 km: back in sight and marching there, straight (no roads in this session).
    units[0].place = Vec3{5000.0, -4000.0, 0.0};
    units[0].vehicles = 25;  // its losses
    su.Apply(units, nav, 120.0);
    nav.Step(121.0, 1.0, {MapPoint{5000.0, 0.0}}, day);
    red = nav.GetMovers().Find("red_r1/t72/1");
    SC_EXPECT(red != nullptr && su.Marches() == 1 && su.RoadSearches() == 0);
    if (red == nullptr) return;
    SC_EXPECT(red->state == nav::MoverState::Moving && red->vehicles == 25 && listed("red_r1/t72/1") == 25);
    nav.Step(6000.0, 6000.0 - 121.0, {MapPoint{5000.0, 0.0}}, day);  // the march straight across country, an hour or so
    SC_EXPECT(red->state == nav::MoverState::Arrived);
    const MapPoint head = nav::PointAlong(red->route, red->arc_m);
    SC_EXPECT(std::hypot(head.east_m - 5000.0, head.north_m + 4000.0) < 1.0);
}

SC_TEST("ground levels (F5.2): the front from afar: each salvo's retarded time and level at the battery, none too far; fires glow only at night") {
    FrontContact c;
    c.axis = "east";
    c.at = Vec3{0.0, 20000.0, 0.0};
    c.along = Vec3{1.0, 0.0, 0.0};
    c.half_width_m = 0.0;  // the salvo where the tip is
    FrontContact c2 = c;
    c2.axis = "west";
    c2.at = Vec3{0.0, 60000.0, 0.0};  // beyond hearing
    FrontAir air;
    air.temperature_k = 283.15;
    air.rh_pct = 70.0;
    const FrontFxValues v;
    const Vec3 battery{0.0, 0.0, 0.0};
    const std::vector<fx::Event> heard = DistantFire({c, c2}, battery, 100.0, 0, air, v, 1);
    SC_EXPECT(heard.size() == 1);
    if (heard.size() != 1) return;
    const fx::Event& e = heard[0];
    const double c_mps = fx::SpeedOfSound(air.temperature_k);
    std::printf("    a salvo 20 km off: fired at %.1f s, heard at %.1f s (%.1f m/s), %.1f dB peak, %d rounds over %.0f s\n", e.t_s, e.end_t_s, c_mps,
                e.level_db, e.number, e.span_s);
    SC_EXPECT(e.kind == fx::EventKind::DistantFire && std::string(fx::ToString(e.kind)) == "distant_fire");
    SC_EXPECT_NEAR(e.end_t_s - e.t_s, 20000.0 / c_mps, 1e-9);  // its retarded time
    SC_EXPECT(e.end_t_s - e.t_s > 55.0 && e.end_t_s - e.t_s < 62.0);
    const double expect_db = fx::PeakSplDb(v.shell_kg, 20000.0, air.pressure_pa, fx::kSurfaceBurstFactor) -
                             fx::AirAbsorptionDbPerM(v.thud_hz, air.temperature_k, air.rh_pct, air.pressure_pa) * 20000.0;
    SC_EXPECT_NEAR(e.level_db, expect_db, 1e-9);
    SC_EXPECT(e.level_db >= v.heard_db && e.number == v.rounds && e.span_s == v.every_s);
    // Farther is quieter and later; the air's absorption takes more than spreading alone.
    c.at = Vec3{0.0, 40000.0, 0.0};
    const std::vector<fx::Event> farther = DistantFire({c}, battery, 100.0, 0, air, v, 1);
    SC_EXPECT(farther.size() == 1 && farther[0].level_db < e.level_db - 6.0 && farther[0].end_t_s > e.end_t_s);
    // Too faint to hear over the battery: none.
    FrontFxValues deaf = v;
    deaf.heard_db = e.level_db + 1.0;
    c.at = Vec3{0.0, 20000.0, 0.0};
    SC_EXPECT(DistantFire({c}, battery, 100.0, 0, air, deaf, 1).empty());
    // Along a frontage the salvo moves from step to step, the same for the same seed.
    c.half_width_m = 4000.0;
    const std::vector<fx::Event> a1 = DistantFire({c}, battery, 0.0, 3, air, v, 9), a2 = DistantFire({c}, battery, 0.0, 3, air, v, 9),
                                 b1 = DistantFire({c}, battery, 0.0, 4, air, v, 9);
    SC_EXPECT(a1.size() == 1 && a2.size() == 1 && b1.size() == 1);
    if (a1.size() == 1 && a2.size() == 1 && b1.size() == 1) {
        SC_EXPECT(a1[0].position.x == a2[0].position.x && a1[0].position.x != b1[0].position.x && std::fabs(a1[0].position.x) <= 4000.0);
    }
    // Fires: none by day; at night within the weather's visibility only.
    SC_EXPECT(FireGlows({c}, battery, 0.0, 0, false, 40000.0, v, 1).empty());
    const std::vector<fx::Event> glow = FireGlows({c}, battery, 0.0, 0, true, 40000.0, v, 1);
    SC_EXPECT(glow.size() == 1 && glow[0].kind == fx::EventKind::FireGlow && glow[0].end_t_s == v.glow_lasts_s && glow[0].fire);
    SC_EXPECT(FireGlows({c}, battery, 0.0, 0, true, 10000.0, v, 1).empty());
}

SC_TEST("ground levels (F5.2): a played mission with the fronts has the war's units near the battery as its movers; none without the fronts") {
    for (int fronts = 0; fronts < 2; ++fronts) {
        campaign::MissionSessionDef md;
        md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
        md.faction = "warsaw_pact";
        md.seed = 1;
        md.day = 3;
        md.theatre = "air_war/theatre_pannonia_nation.json";
        md.hours = 0.1;
        md.fronts = fronts == 1;
        md.u2_radius_m = 150000.0;  // battery 21 stands about 100 km from the Pact's nearest regiment on day 3
        campaign::MissionSession s;
        std::string e;
        SC_EXPECT(s.Start(md, kData, &e));
        if (!e.empty()) std::printf("    %s\n", e.c_str());
        ScenarioRunner& r = s.Runner();
        for (int k = 0; k < 600 && !r.Finished(); ++k) s.Step(0.5);
        int gw = 0, enemy = 0, ours_seen = 0, enemy_seen = 0;
        const nav::SessionNav* nav = r.Nav();
        SC_EXPECT(nav != nullptr);
        if (nav == nullptr) return;
        const std::vector<std::string> our_types = s.GroundUnits().OurTypeList();
        for (const nav::Mover& m : nav->GetMovers().All()) {
            if (!m.ground_war) continue;
            ++gw;
            enemy += m.enemy ? 1 : 0;
        }
        for (const ScenarioRunner::WorldMover& v : r.MoversInView()) {
            bool u2 = false;
            for (const nav::Mover& m : nav->GetMovers().All()) u2 = u2 || (m.id == v.mover && m.ground_war);
            if (!u2) continue;
            (v.enemy ? enemy_seen : ours_seen) += 1;
            // Ours by our models, the enemy's never.
            SC_EXPECT(v.enemy != std::binary_search(our_types.begin(), our_types.end(), v.model));
        }
        std::printf("    fronts %s: %d U2 movers (%d the enemy's), %zu near now, %d of ours and %d of the enemy's vehicles in sight; %d salvos heard, %d fires seen\n",
                    fronts == 1 ? "on " : "off", gw, enemy, s.GroundUnits().Near(), ours_seen, enemy_seen, s.FrontSalvosHeard(), s.FrontFiresSeen());
        if (fronts == 0) {
            SC_EXPECT(!s.GroundUnits().Started() && gw == 0 && s.FrontSalvosHeard() == 0);
        } else {
            SC_EXPECT(s.GroundUnits().Started() && gw > 0 && static_cast<std::size_t>(gw) == s.GroundUnits().Made());
        }
    }
}
