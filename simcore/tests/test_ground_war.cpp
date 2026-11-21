// Track F, F3.3: the ground war's clock and its line (simcore/ground_war/ground_war.hpp; docs/fronts/03_GROUND_WAR.md
// sections 2, 3.3 and 12). A hand case on a straight 100 km line: the first hour's advance and losses from F3.2's
// sector model with the same inputs, the line's penetration drawn over its stations and the distances that follow it; a
// river that holds the attack at its bank until crossed; the order the axes are resolved in changing nothing; the
// campaign at ground.mode 0 and on the district theatre unchanged; the save's key; the nation's war moving its line.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/save.hpp"
#include "simcore/command.hpp"
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

command::OobUnit Division(const std::string& id, int tanks, int men) {
    command::OobUnit u;
    u.id = id;
    u.name = id;
    u.kind = "division";
    u.equipment.push_back({"t55", tanks});
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

EffectivenessDef Lines() {
    EffectivenessDef e;
    e.kind = EffectivenessKind::Lines;
    e.degraded_below = 0.75;
    e.ineffective_below = 0.5;
    e.hq_hit_ineffective_below = 0.6;
    return e;
}

// A straight 100 km line along the east axis from (0, 0); the enemy's bases 50 km south, so our country is north.
// Sectors: west (0-50 km), east (50-100 km).
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

// One axis: red_div (`red_tanks` T-55s) attacks north through the east sector at 75 km towards a goal 60 km in; our
// blue_div (`blue_tanks`) holds the sector in its first echelon, no security zone. With `two_axes`, a second axis
// (green_div) attacks the west sector at 25 km and blue_div holds both sectors. No draw (sigma 0) unless `draws`.
std::shared_ptr<GroundWarData> HandData(int red_tanks, int blue_tanks, bool two_axes, bool draws) {
    auto d = std::make_shared<GroundWarData>();
    d->faction = "test";
    d->tables = Tables();
    if (!draws) d->tables.draw_sigma = 0.0;
    d->enemy.faction = "test";
    d->enemy.name = "the red army";
    d->enemy.oob.units.push_back(Division("red_div", red_tanks, 10000));
    d->enemy.doctrine.effectiveness = Lines();
    d->enemy.doctrine.caps.breakthrough_km_per_day = 30.0;
    d->enemy.doctrine.caps.depth_km_per_day = 50.0;
    d->enemy.doctrine.caps.broken_km_per_day = 70.0;
    GroundPlanDef& p = d->enemy.plan;
    p.kind = PlanKind::Offensive;
    p.start_after_h = 2.0;
    ObjectiveDef goal;
    goal.id = "goal_obj";
    goal.name = "the goal";
    goal.place = At("goal", 75, 60);
    p.objectives.push_back(goal);
    AxisDef ax;
    ax.id = "east_axis";
    ax.name = "the east axis";
    ax.main = true;
    ax.route = {At("start", 75, -20), At("crossing", 75, 0), At("goal", 75, 60)};
    ax.first_echelon = {"red_div"};
    ax.objective = "goal_obj";
    ax.sector = "east";
    p.axes.push_back(ax);
    if (two_axes) {
        d->enemy.oob.units.push_back(Division("green_div", red_tanks, 10000));
        ObjectiveDef g2;
        g2.id = "west_obj";
        g2.name = "the west goal";
        g2.place = At("west_goal", 25, 60);
        p.objectives.push_back(g2);
        AxisDef w;
        w.id = "west_axis";
        w.name = "the west axis";
        w.route = {At("w_start", 25, -20), At("w_crossing", 25, 0), At("west_goal", 25, 60)};
        w.first_echelon = {"green_div"};
        w.objective = "west_obj";
        w.sector = "west";
        p.axes.push_back(w);
    }
    d->ours.name = "our army";
    d->ours.oob.units.push_back(Division("blue_div", blue_tanks, 10000));
    d->ours.doctrine.effectiveness = Lines();
    DeploymentDef dep;
    dep.adversary = "test";
    dep.security_zone_km = 0.0;
    DeployedUnitDef u;
    u.unit = "blue_div";
    u.role = DeployRole::FirstEchelon;
    u.sectors = two_axes ? std::vector<std::string>{"west", "east"} : std::vector<std::string>{"east"};
    dep.units.push_back(u);
    d->ours.deployments.push_back(dep);
    return d;
}

std::size_t FormationIndex(const GroundWar& g, const std::string& id) {
    for (std::size_t i = 0; i < g.Formations().size(); ++i) {
        if (g.Formations()[i].unit.id == id) return i;
    }
    return 0;
}

bool Logged(const GroundWar& g, GroundNote what) {
    for (const GroundLogEntry& e : g.Log()) {
        if (e.what == what) return true;
    }
    return false;
}

std::uint64_t JournalHash(const AirCampaign& c) {
    std::uint64_t h = 1469598103934665603ull;
    for (const JournalEntry& j : c.Journal()) {
        for (char ch : j.text) {
            h ^= static_cast<unsigned char>(ch);
            h *= 1099511628211ull;
        }
        h ^= 0x0a;
        h *= 1099511628211ull;
    }
    return h;
}

void RunTo(AirCampaign& c, double h) {
    while (c.Now() < h) {
        if (c.Advance(h)) c.SkipPlayerMission();
    }
}

}  // namespace

SC_TEST("ground war (F3.3): the first hour by hand: F3.2's advance and losses, the penetration drawn on the line") {
    const GroundTables& t = Tables();
    std::shared_ptr<GroundWarData> d = HandData(900, 100, false, false);
    LineOfContact line;
    line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;  // start 06:00: the first hours are day
    GroundWar g;
    std::string e;
    SC_EXPECT(g.Build(d, line, theatre, nullptr, 7, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(g.Axes().size() == 1 && g.Formations().size() == 2);
    const Axis& x = g.Axes()[0];
    SC_EXPECT(x.sector == 1);
    SC_EXPECT_NEAR(x.along_m, 75000.0, 1e-6);
    SC_EXPECT_NEAR(x.objective_m, 60000.0, 1e-6);
    GroundEnv env;
    env.theatre = &theatre;
    // Before the plan's start nothing moves.
    g.Step(2.0, env, line);
    SC_EXPECT(!line.Moved() && x.status == AxisStatus::Waiting);
    // The hand case: both have waited 2 h at the reserve's rate; the attack is FM 101-10-1's on a prepared defence.
    const std::size_t red = FormationIndex(g, "red_div"), blue = FormationIndex(g, "blue_div");
    const double est_red = g.Formations()[red].unit.establishment, est_blue = g.Formations()[blue].unit.establishment;
    SC_EXPECT_NEAR(est_red, t.personnel_per_man * 10000.0 + t.WeightOf("t55") * 900.0, 1e-6);
    const double waited = LossOverHours(DailyLoss(t, LossAction::Reserve, 1), 2.0);
    const double s_red = est_red * (1.0 - waited), s_blue = est_blue * (1.0 - waited);
    SectorSide att;
    att.power = UnitPower(t, s_red, 1.0, 1.0);
    att.posture = Posture::Attack;
    SectorSide def;
    def.power = UnitPower(t, s_blue, 1.0, 1.0);
    def.posture = Posture::PreparedDefence;
    SectorConditions c;
    const SectorResult r = ResolveSector(t, att, def, c, d->enemy.doctrine.caps, 1.0, 1.0);
    std::printf("    hour 1: ratio %.2f (%s), %.0f m in, losses %.4f / %.4f\n", r.ratio, OppositionName(r.opposition), r.advance_km * 1000.0,
                r.attacker_loss, r.defender_loss);
    g.Step(3.0, env, line);
    SC_EXPECT(x.status == AxisStatus::Attacking);
    SC_EXPECT_NEAR(x.last_ratio, r.ratio, 1e-9);
    SC_EXPECT_NEAR(x.depth_m, r.advance_km * 1000.0, 1e-6);
    SC_EXPECT(x.depth_m > 1.0);
    SC_EXPECT_NEAR(g.Formations()[red].unit.strength, s_red * (1.0 - r.attacker_loss), 1e-6);
    // Ours lose tanks by FM 101-10-1 table 7-7's first day over 24 hours, the rest by the sector's rate.
    const TankCommitment cm = r.ratio >= 4.0 ? TankCommitment::DefendHeavy : TankCommitment::DefendModerate;
    const double tanks0 = 100.0 * (1.0 - waited);
    SC_EXPECT_NEAR(g.Formations()[blue].Tanks(), tanks0 * (1.0 - TankLossShare(t, cm, 1) / 24.0), 1e-9);
    SC_EXPECT(g.Formations()[blue].commitment == static_cast<int>(cm));
    // The line: the depth over the breakthrough's half width (75 km, its station 15), tapering linearly to 0.
    const double half = 500.0 * t.breakthrough_width_km.Mid(), taper = 1000.0 * t.taper_km.Mid();
    const auto expect_at = [&](double along_m) {
        const double off = std::fabs(along_m - 75000.0);
        if (off <= half) return x.depth_m;
        if (off >= half + taper) return 0.0;
        return x.depth_m * (1.0 - (off - half) / taper);
    };
    SC_EXPECT(line.Moved());
    for (std::size_t i = 0; i < line.StationCount(); ++i) SC_EXPECT_NEAR(line.StationOffset(i), expect_at(line.StationAlong(i)), 1e-9);
    SC_EXPECT_NEAR(line.StationOffset(15), x.depth_m, 1e-9);
    SC_EXPECT_NEAR(line.StationOffset(11), 0.0, 1e-12);
    SC_EXPECT_NEAR(line.StationOffset(4), 0.0, 1e-12);  // the west sector is untouched
    // The distances follow it: a point 30 km behind the border at 75 km is that less the depth from the line.
    SC_EXPECT_NEAR(line.DistanceTo(Km(75, 30)), 30000.0 - x.depth_m, 1e-6);
    SC_EXPECT_NEAR(line.DistanceTo(Km(20, 30)), 30000.0, 1e-6);
    SC_EXPECT_NEAR(GroundWar::SectorDepth(line, 1), x.depth_m, 1e-9);
    // Days on: it goes on, never back, never faster than the faction's cap; a day's end writes the line.
    double last = x.depth_m;
    bool monotone = true;
    for (int h = 4; h <= 3 + 24 * 5; ++h) {
        g.Step(static_cast<double>(h), env, line);
        monotone = monotone && x.depth_m >= last - 1e-9;
        last = x.depth_m;
    }
    SC_EXPECT(monotone);
    SC_EXPECT(x.depth_m <= 5.1 * 30000.0);
    SC_EXPECT(Logged(g, GroundNote::DayLine) && Logged(g, GroundNote::AxisStarts));
    for (std::size_t i = 0; i < line.StationCount(); ++i) SC_EXPECT(line.StationOffset(i) >= 0.0);
    std::printf("    after 5 days: %.1f km in, red %.0f%%, blue %.0f%% (%.1f tanks lost, %.1f reparable)\n", x.depth_m / 1000.0,
                100.0 * g.Formations()[red].unit.Fraction(), 100.0 * g.Formations()[blue].unit.Fraction(), g.Formations()[blue].tanks_lost,
                g.Formations()[blue].tanks_reparable);
    // To its end: the objective, its attackers spent, or stalled; the queue stays small (no growth hour by hour).
    g.ClearLog();
    for (int h = 4 + 24 * 5; h <= 24 * 60; h += 24) g.Step(static_cast<double>(h), env, line);
    SC_EXPECT(x.status == AxisStatus::Halted && x.halted != HaltReason::None);
    SC_EXPECT(x.depth_m <= x.objective_m + 1e-6);
    SC_EXPECT(g.QueueSize() <= 4);
    std::printf("    halted %.1f km in: %s\n", x.depth_m / 1000.0, HaltReasonName(x.halted));
}

SC_TEST("ground war (F3.3): a river holds the attack at its bank until crossed, longer with its bridge down") {
    const GroundTables& t = Tables();
    CountryDef cd;
    cd.ground.west_m = -10000.0;
    cd.ground.east_m = 110000.0;
    cd.ground.south_m = -30000.0;
    cd.ground.north_m = 70000.0;
    cd.grid_spacing_m = 1000.0;
    MapLineDef river;
    river.id = "test_river";
    river.line_class = "river";
    river.width_m = 50.0;
    river.points = {MapPoint{50000.0, 10000.0}, MapPoint{100000.0, 10000.0}};
    cd.ground.rivers.push_back(river);
    std::shared_ptr<const CountryMap> map = CountryMap::Build(cd);
    TheatreDef theatre;
    TheatreObject bridge;
    bridge.id = "test_bridge";
    bridge.kind = ObjectKind::Bridge;
    bridge.pos = Km(75, 10);
    theatre.objects.push_back(bridge);
    double held_h[2] = {0.0, 0.0};
    for (int down = 0; down < 2; ++down) {
        std::shared_ptr<GroundWarData> d = HandData(600, 100, false, false);
        LineOfContact line;
        line.Build(StraightFront(), 5000.0);
        GroundWar g;
        std::string e;
        SC_EXPECT(g.Build(d, line, theatre, map.get(), 7, &e));
        const Axis& x = g.Axes()[0];
        SC_EXPECT(x.rivers.size() == 1);
        if (x.rivers.empty()) return;
        SC_EXPECT_NEAR(x.rivers[0].depth_m, 10000.0, 1e-6);
        SC_EXPECT(x.rivers[0].bridge_object == 0 && x.rivers[0].river == "test_river");
        std::vector<ObjectState> truth(1);
        truth[0].id = "test_bridge";
        truth[0].function = down == 1 ? 0.0 : 1.0;
        GroundEnv env;
        env.theatre = &theatre;
        env.truth = &truth;
        double reached = -1.0;
        int h = 1;
        for (; h <= 24 * 30 && reached < 0.0; ++h) {
            g.Step(static_cast<double>(h), env, line);
            for (const GroundLogEntry& l : g.Log()) {
                if (l.what == GroundNote::RiverReached) {
                    reached = l.h;
                    SC_EXPECT(l.intact == (down == 0));
                }
            }
            g.ClearLog();
        }
        SC_EXPECT(reached > 0.0);
        SC_EXPECT_NEAR(x.depth_m, 10000.0, 1e-6);
        const double crossing = CrossingHours(t, down == 0, false);
        // Held at the bank for the crossing's hours, then on across.
        g.Step(reached + crossing - 1.0, env, line);
        SC_EXPECT_NEAR(x.depth_m, 10000.0, 1e-6);
        SC_EXPECT(!Logged(g, GroundNote::RiverCrossed));
        g.Step(reached + crossing + 1.0, env, line);
        SC_EXPECT(Logged(g, GroundNote::RiverCrossed));
        g.Step(reached + crossing + 24.0, env, line);
        SC_EXPECT(x.depth_m > 10000.0 || x.status == AxisStatus::Halted);
        held_h[down] = crossing;
        std::printf("    bridge %s: at the bank at %.0f h, across after %.0f h, %.1f km in a day later (%s, ratio %.2f)\n", down == 1 ? "down" : "standing",
                    reached, crossing, x.depth_m / 1000.0, x.status == AxisStatus::Halted ? HaltReasonName(x.halted) : "attacking", x.last_ratio);
    }
    SC_EXPECT(held_h[1] > held_h[0]);
}

SC_TEST("ground war (F3.3): order-free: two axes sharing a defender, resolved either way round, give the same war") {
    LineOfContact a_line, b_line;
    a_line.Build(StraightFront(), 5000.0);
    b_line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;
    std::vector<airwar::Weather> weather(30, airwar::Weather::Clear);
    weather[2] = airwar::Weather::Poor;
    GroundEnv env;
    env.theatre = &theatre;
    env.weather = &weather;
    std::shared_ptr<GroundWarData> d = HandData(250, 160, true, true);
    GroundWar a, b;
    SC_EXPECT(a.Build(d, a_line, theatre, nullptr, 11) && b.Build(d, b_line, theatre, nullptr, 11));
    b.SetReverseOrder(true);
    a.Step(24.0 * 8, env, a_line);
    b.Step(24.0 * 8, env, b_line);
    bool same = a.Axes().size() == 2 && b.Axes().size() == 2;
    for (std::size_t k = 0; same && k < 2; ++k) same = a.Axes()[k].depth_m == b.Axes()[k].depth_m && a.Axes()[k].status == b.Axes()[k].status;
    for (std::size_t i = 0; same && i < a.Formations().size(); ++i) {
        same = a.Formations()[i].unit.strength == b.Formations()[i].unit.strength && a.Formations()[i].tanks_lost == b.Formations()[i].tanks_lost;
    }
    for (std::size_t i = 0; same && i < a_line.StationCount(); ++i) same = a_line.StationOffset(i) == b_line.StationOffset(i);
    SC_EXPECT(same);
    // blue_div holds both sectors: each axis met half of it.
    SC_EXPECT(a.Axes()[0].depth_m > 0.0 && a.Axes()[1].depth_m > 0.0);
    // The draws are keyed by sector and day: the east axis alone, built with the same seed, draws the same days.
    std::printf("    8 days: east %.1f km, west %.1f km in; draws %.3f, %.3f\n", a.Axes()[0].depth_m / 1000.0, a.Axes()[1].depth_m / 1000.0,
                a.Axes()[0].draw, a.Axes()[1].draw);
    // Another seed, another war.
    LineOfContact c_line;
    c_line.Build(StraightFront(), 5000.0);
    GroundWar c;
    SC_EXPECT(c.Build(d, c_line, theatre, nullptr, 12));
    c.Step(24.0 * 8, env, c_line);
    SC_EXPECT(c.Axes()[0].depth_m != a.Axes()[0].depth_m);
}

SC_TEST("ground war (F3.3): ground.mode 0, and the district theatre at mode 1, leave the campaign as it was") {
    std::string e;
    AirWarData off, on;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", off, &e));
    on = off;
    on.tuning.values["ground.mode"] = 1.0;
    AirCampaign a, b;
    SC_EXPECT(a.Init(off, 21) && b.Init(on, 21));
    SC_EXPECT(a.Ground() == nullptr && b.Ground() == nullptr);  // no country map: nothing built
    RunTo(a, 72.0);
    RunTo(b, 72.0);
    SC_EXPECT(a.Journal().size() == b.Journal().size() && JournalHash(a) == JournalHash(b) && a.Packages().size() == b.Packages().size());
    SC_EXPECT(!b.FrontLine().Moved());
}

SC_TEST("ground war (F3.3): the nation's Pact war moves the line in its axes' sectors, and a save keeps ground_mode") {
    std::string e;
    AirWarData d;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json", d, &e));
    AirCampaign zero;
    SC_EXPECT(zero.Init(d, 21, &e));
    SC_EXPECT(zero.Ground() == nullptr);
    d.tuning.values["ground.mode"] = 1.0;
    AirCampaign c;
    SC_EXPECT(c.Init(d, 21, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(c.Ground() != nullptr);
    if (c.Ground() == nullptr) return;
    RunTo(c, 48.0);
    SC_EXPECT(c.FrontLine().Moved());
    int ground_lines = 0;
    for (const JournalEntry& j : c.Journal()) ground_lines += j.kind == "ground" ? 1 : 0;
    SC_EXPECT(ground_lines > 0);
    const GroundWar& g = *c.Ground();
    for (const Axis& x : g.Axes()) {
        std::printf("    %s (%s): %.1f km in, %s\n", x.name.c_str(), c.FrontLine().Sector(static_cast<std::size_t>(x.sector)).id.c_str(),
                    x.depth_m / 1000.0, x.status == AxisStatus::Halted ? HaltReasonName(x.halted) : "attacking");
        SC_EXPECT(x.depth_m >= 0.0 && x.depth_m < 2.0 * 70000.0);  // no runaway: two days at the broken enemy's cap at most
    }
    // Only the stations within an axis's breakthrough and taper move.
    const LineOfContact& fl = c.FrontLine();
    bool still = true;
    for (std::size_t i = 0; i < fl.StationCount(); ++i) {
        bool reached = false;
        for (const Axis& x : g.Axes()) reached = reached || std::fabs(fl.StationAlong(i) - x.along_m) <= x.half_width_m + x.taper_m;
        if (!reached) still = still && fl.StationOffset(i) == 0.0;
    }
    SC_EXPECT(still);
    // The save keeps the mode; a save without it replays at 0.
    std::string saved = SaveCampaignJson(c, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json");
    SC_EXPECT(saved.find("\"ground_mode\"") != std::string::npos);
    AirCampaign back;
    SC_EXPECT(LoadCampaignJson(saved, kData, back, &e));
    SC_EXPECT_NEAR(back.Data().tuning.Get("ground.mode"), 1.0, 1e-12);
    SC_EXPECT(back.Ground() != nullptr && back.Journal().size() == c.Journal().size() && JournalHash(back) == JournalHash(c));
    const std::size_t at = saved.find("\"ground_mode\"");
    const std::size_t end = saved.find(',', at);
    if (at != std::string::npos && end != std::string::npos) saved.erase(at, end - at + 1);
    AirCampaign old;
    SC_EXPECT(LoadCampaignJson(saved, kData, old, &e));
    SC_EXPECT_NEAR(old.Data().tuning.Get("ground.mode"), 0.0, 1e-12);
    SC_EXPECT(old.Ground() == nullptr && !old.FrontLine().Moved());
}

SC_TEST("ground war (F3.3): cheap: the nation's Pact ground war alone, a month of hours, well under its budget a day") {
    std::string e;
    AirWarData d;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json", d, &e));
    auto gd = std::make_shared<GroundWarData>();
    SC_EXPECT(LoadGroundWarData(kData, "warsaw_pact", *gd, &e));
    const FrontDef* front = d.theatre.FrontFor(d.faction.front);
    SC_EXPECT(front != nullptr && d.country != nullptr);
    if (front == nullptr || d.country == nullptr) return;
    LineOfContact line;
    line.Build(*front, d.tuning.Get("air_front.station_m"));
    GroundWar g;
    SC_EXPECT(g.Build(gd, line, d.theatre, d.country.get(), 21, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    std::vector<airwar::Weather> weather(30, airwar::Weather::Clear);
    std::vector<ObjectState> truth(d.theatre.objects.size());
    GroundEnv env;
    env.theatre = &d.theatre;
    env.weather = &weather;
    env.truth = &truth;
    g.SetProfiling(true);
    const auto t0 = std::chrono::steady_clock::now();
    for (int h = 1; h <= 24 * 30; ++h) {
        g.Step(static_cast<double>(h), env, line);
        g.ClearLog();
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("    30 days in %.2f ms (%.3f ms a day); combat %.2f, decisions %.2f, movement %.2f, line %.2f ms\n", ms, ms / 30.0,
                g.Profile().Ms(GroundProfile::Combat), g.Profile().Ms(GroundProfile::Decisions), g.Profile().Ms(GroundProfile::Movement),
                g.Profile().Ms(GroundProfile::Line));
    SC_EXPECT(ms / 30.0 < 20.0);
    SC_EXPECT(g.Profile().hours == 24u * 30u);
}
