// Track F, F3.7: the air war follows the ground units (simcore/ground_war/ground_war.hpp's troops ties, simcore/air_war/
// campaign.hpp; docs/fronts/03_GROUND_WAR.md section 6). A troops object takes its place and working share from its unit;
// the enemy's strike on it comes off the unit's strength and cohesion; the supply lines ending at troops end where they
// are; our objects behind the enemy's line are overrun, out of its targets and ours; NATO's FSCL by sector, in defence
// its band beyond the line, in attack beyond the day's objective, read by C2's labels; ground.mode 0 changes nothing.
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

EffectivenessDef TestLines() {
    EffectivenessDef e;
    e.kind = EffectivenessKind::Lines;
    e.degraded_below = 0.95;
    e.ineffective_below = 0.9;
    e.hq_hit_ineffective_below = 0.6;
    return e;
}

// A straight 100 km line along the east axis; the enemy south, our country north; sectors west and east.
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

// The test war (test_ground_picture's): the enemy attacks in the east from hour 2; our division holds the east, a brigade
// the west, a tank division in reserve. The division's troops object and the reserve's are the theatre's, tied to them.
std::shared_ptr<GroundWarData> War(bool fscl) {
    auto d = std::make_shared<GroundWarData>();
    d->faction = "test";
    d->tables = Tables();
    d->tables.draw_sigma = 0.0;
    d->enemy.faction = "test";
    d->enemy.name = "the red army";
    d->enemy.doctrine.effectiveness = TestLines();
    d->enemy.doctrine.caps.breakthrough_km_per_day = 30.0;
    d->enemy.doctrine.caps.depth_km_per_day = 50.0;
    d->enemy.doctrine.caps.broken_km_per_day = 70.0;
    if (fscl) {
        d->enemy.doctrine.has_fscl = true;
        d->enemy.doctrine.fscl_defence_km.lo = 20.0;
        d->enemy.doctrine.fscl_defence_km.hi = 30.0;
        d->enemy.doctrine.fscl_terrain_reach_km = 30.0;
    }
    d->enemy.plan.kind = PlanKind::Offensive;
    d->enemy.plan.start_after_h = 2.0;
    d->ours.name = "our army";
    d->ours.doctrine.effectiveness = TestLines();
    d->ours.doctrine.caps = d->enemy.doctrine.caps;
    DeploymentDef dep;
    dep.adversary = "test";
    dep.security_zone_km = 15.0;
    d->ours.deployments.push_back(dep);
    d->enemy.oob.units.push_back(Unit("red_corps", "corps", 0, 500));
    d->enemy.oob.units.push_back(Unit("red_div", "division", 300, 10000, "red_corps"));
    d->enemy.oob.units.push_back(Unit("red_acr", "regiment", 120, 4000, "red_corps"));
    d->enemy.oob.units.push_back(Unit("red_second", "division", 250, 9000, "red_corps"));
    ObjectiveDef goal;
    goal.id = "east_obj";
    goal.name = "east's goal";
    goal.place = At("east_goal", 75, 60);
    d->enemy.plan.objectives.push_back(goal);
    AxisDef ax;
    ax.id = "east";
    ax.name = "the east axis";
    ax.main = true;
    ax.route = {At("east_start", 75, -20), At("east_crossing", 75, 0), At("east_goal", 75, 60)};
    ax.first_echelon = {"red_div", "red_acr"};
    ax.second_echelon = {"red_second"};
    ax.objective = goal.id;
    ax.sector = "east";
    d->enemy.plan.axes.push_back(ax);
    d->ours.oob.units.push_back(Unit("blue_army", "army", 0, 500));
    d->ours.oob.units.push_back(Unit("blue_div", "division", 60, 6000, "blue_army"));
    d->ours.oob.units.push_back(Unit("blue_bde", "brigade", 40, 3000, "blue_army"));
    d->ours.oob.units.push_back(Unit("blue_td", "division", 1500, 12000, "blue_army"));
    d->ours.oob.units[1].objects = {"east_troops"};
    d->ours.oob.units[3].objects = {"td_troops"};
    const auto deploy = [&d](const std::string& unit, DeployRole role, std::vector<std::string> sectors, double e_km, double n_km) {
        DeployedUnitDef u;
        u.unit = unit;
        u.role = role;
        u.sectors = std::move(sectors);
        u.place = At(unit + "_place", e_km, n_km);
        d->ours.deployments.front().units.push_back(u);
    };
    deploy("blue_div", DeployRole::FirstEchelon, {"east"}, 75, 10);
    deploy("blue_bde", DeployRole::FirstEchelon, {"west"}, 25, 10);
    deploy("blue_td", DeployRole::Reserve, {}, 75, 40);
    return d;
}

TheatreObject TroopsObject(const std::string& id, double e_km, double n_km) {
    TheatreObject o;
    o.id = id;
    o.name = id;
    o.kind = ObjectKind::Troops;
    o.pos = Km(e_km, n_km);
    return o;
}

int FormationOf(const GroundWar& g, const std::string& id) {
    for (std::size_t i = 0; i < g.Formations().size(); ++i) {
        if (g.Formations()[i].unit.id == id) return static_cast<int>(i);
    }
    return -1;
}

struct Fixture {
    LineOfContact line;
    TheatreDef theatre;
    GroundEnv env;
    GroundWar g;
    std::vector<GroundLogEntry> log;
    explicit Fixture(bool fscl = false) {
        line.Build(StraightFront(), 5000.0);
        theatre.objects = {TroopsObject("east_troops", 70, 10), TroopsObject("td_troops", 75, 40)};
        env.theatre = &theatre;
        ok = g.Build(War(fscl), line, theatre, nullptr, 7);
    }
    bool ok = false;
    void To(int h) {
        g.Step(static_cast<double>(h), env, line);
        log.insert(log.end(), g.Log().begin(), g.Log().end());
        g.ClearLog();
    }
};

void RunTo(AirCampaign& c, double h) {
    while (c.Now() < h) {
        if (c.Advance(h)) c.SkipPlayerMission();
    }
}

}  // namespace

SC_TEST("ground air (F3.7): a theatre troops object is tied to its owner's formation, whole at the start, at home while nothing moves") {
    Fixture w;
    SC_EXPECT(w.ok);
    SC_EXPECT(w.g.Ties().size() == 2u);
    if (w.g.Ties().size() != 2u) return;
    const TroopsTie& east = w.g.Ties()[0];
    SC_EXPECT(east.object == 0 && east.formation == FormationOf(w.g, "blue_div"));
    SC_EXPECT(w.g.TieOf(0) == 0 && w.g.TieOf(1) == 1 && w.g.TieOf(5) == -1);
    SC_EXPECT_NEAR(east.share, 1.0, 1e-12);
    SC_EXPECT(east.unit_base > 0.0);
    SC_EXPECT_NEAR(w.g.TroopsFunction(0, 0.0), 1.0, 1e-9);
    const Vec3 p = w.g.TroopsPlace(0, w.line);
    SC_EXPECT_NEAR(p.x, 70000.0, 1e-9);
    SC_EXPECT_NEAR(p.y, 10000.0, 1e-9);
}

SC_TEST("ground air (F3.7): the enemy's strike on troops comes off the unit: damage times share times its strength, and cohesion") {
    Fixture w;
    SC_EXPECT(w.ok && !w.g.Ties().empty());
    if (w.g.Ties().empty()) return;
    w.To(1);
    const std::size_t fi = static_cast<std::size_t>(FormationOf(w.g, "blue_div"));
    const Formation& f = w.g.Formations()[fi];
    const double s0 = w.g.StrengthAt(fi, 1.0), c0 = f.unit.cohesion, fn0 = w.g.TroopsFunction(0, 1.0);
    const double lost = w.g.AirStrike(0, 0.3, 1.0);
    const TroopsTie& t = w.g.Ties()[0];
    SC_EXPECT_NEAR(lost, std::min(fn0, 0.3) * t.share * t.unit_base, 1e-9 * t.unit_base);
    SC_EXPECT_NEAR(f.unit.strength, s0 - lost, 1e-9 * s0);
    SC_EXPECT_NEAR(w.g.TroopsFunction(0, 1.0), fn0 - 0.3, 1e-9);
    SC_EXPECT(f.unit.cohesion < c0 && f.unit.cohesion >= c0 - Tables().cohesion_air_attack_loss - 1e-12);
    bool noted = false;
    for (const GroundLogEntry& l : w.g.Log()) noted = noted || (l.what == GroundNote::AirAttack && l.other == 0 && l.formation == static_cast<int>(fi));
    SC_EXPECT(noted);
    // Never more than it has; nothing for nothing.
    SC_EXPECT(w.g.AirStrike(0, 0.0, 1.0) == 0.0);
    const double rest = w.g.AirStrike(0, 5.0, 1.0);
    SC_EXPECT(rest <= s0 - lost + 1e-9);
    SC_EXPECT(w.g.TroopsFunction(0, 1.0) < 1e-9);
    std::printf("    strike 0.3: %.1f of %.1f lost, cohesion %.3f -> %.3f\n", lost, s0, c0, f.unit.cohesion);
}

SC_TEST("ground air (F3.7): a march column's strength brought reinforces the unit, up to its establishment at the start") {
    Fixture w;
    SC_EXPECT(w.ok && !w.g.Ties().empty());
    if (w.g.Ties().empty()) return;
    w.To(1);
    const std::size_t fi = static_cast<std::size_t>(FormationOf(w.g, "blue_div"));
    const Formation& f = w.g.Formations()[fi];
    const double room = std::max(0.0, f.base_establishment - w.g.StrengthAt(fi, 1.0));
    SC_EXPECT(w.g.Reinforce(0, 0.2, 1.0) <= room + 1e-9 * f.base_establishment);
    const double lost = w.g.AirStrike(0, 0.4, 1.0);
    const double s1 = f.unit.strength;
    const double back = w.g.Reinforce(0, 0.1, 1.0);
    SC_EXPECT(back > 0.0 && back <= lost + 1e-9);
    SC_EXPECT_NEAR(f.unit.strength, s1 + back, 1e-9 * s1);
    SC_EXPECT(f.unit.strength <= f.base_establishment + 1e-9);
}

SC_TEST("ground air (F3.7): tied troops keep their unit's depth behind the moved line of contact") {
    Fixture w;
    SC_EXPECT(w.ok);
    for (int h = 1; h <= 36; ++h) w.To(h);
    SC_EXPECT(w.line.Moved());
    for (std::size_t k = 0; k < w.g.Ties().size(); ++k) {
        const TroopsTie& t = w.g.Ties()[k];
        const Vec3 p = w.g.TroopsPlace(k, w.line);
        const FrontPlace pl = w.line.Place(p);
        const double depth = pl.side * pl.distance_m - w.line.Offset(pl.along_m);
        const Formation& f = w.g.Formations()[static_cast<std::size_t>(t.formation)];
        const double need = f.spent ? 1000.0 * Tables().troops.reform_depth_km : f.arrived ? 3000.0 : std::min(t.home_depth_m, 12000.0);
        SC_EXPECT(depth >= need - 1.0);
        std::printf("    %s: %.1f km behind the line of contact (needs %.1f)\n", w.theatre.objects[static_cast<std::size_t>(t.object)].id.c_str(),
                    depth / 1000.0, need / 1000.0);
    }
}

SC_TEST("ground air (F3.7): NATO's FSCL by hand: the band's middle beyond the line in defence, beyond the day's objective in attack") {
    Fixture none;
    SC_EXPECT(none.ok && none.g.Fscl().empty());
    Fixture w(true);
    SC_EXPECT(w.ok);
    SC_EXPECT(w.g.Fscl().size() == 2u);
    if (w.g.Fscl().size() != 2u) return;
    SC_EXPECT_NEAR(w.g.Fscl()[0], 25000.0, 1e-9);  // nothing attacks yet: 25 km beyond the line everywhere
    SC_EXPECT_NEAR(w.g.Fscl()[1], 25000.0, 1e-9);
    const std::uint64_t v0 = w.g.FsclVersion();
    for (int h = 1; h <= 6; ++h) w.To(h);
    SC_EXPECT(w.g.FsclVersion() > v0);
    // The east attacks: a day at its breakthrough cap (30 km) beyond its depth when drawn, short of the objective (60 km),
    // then the band's middle; the west still the band beyond its line.
    SC_EXPECT_NEAR(w.g.Fscl()[0], 25000.0, 1e-9);
    SC_EXPECT(w.g.Fscl()[1] >= 55000.0 - 1e-6 && w.g.Fscl()[1] <= 85000.0 + 1e-6);
    // The readers: the area's depth is the FSCL less the line's offset at a place; with none, the fixed depth.
    AirFrontView v;
    v.line = &w.line;
    SC_EXPECT_NEAR(v.BaiDepth(Km(25, 10), 30000.0), 30000.0, 1e-12);
    std::vector<double> fscl = w.g.Fscl();
    v.fscl_m = &fscl;
    SC_EXPECT_NEAR(v.BaiDepth(Km(25, 10), 30000.0), 25000.0 - w.line.Offset(25000.0), 1e-9);
    const double east = fscl[1] - w.line.Offset(75000.0);
    SC_EXPECT_NEAR(v.BaiDepth(Km(75, 30), 30000.0), std::max(0.0, east), 1e-9);
    std::printf("    FSCL west %.1f km, east %.1f km beyond the base line; the east's line of contact %.1f km in\n", fscl[0] / 1000.0,
                fscl[1] / 1000.0, w.line.Offset(75000.0) / 1000.0);
}

SC_TEST("ground air (F3.7): an object the enemy holds leaves its picture, and no report of it merges until ours again") {
    IntelPicture p;
    IntelReport r;
    r.object_id = "depot";
    r.kind = ObjectKind::Depot;
    r.pos = Km(10, 10);
    r.function = 1.0;
    p.Queue(r);
    p.Deliver(1.0, 0.5);
    SC_EXPECT(p.Find("depot") != nullptr);
    p.Exclude("depot");
    SC_EXPECT(p.Find("depot") == nullptr && p.Held("depot"));
    p.Queue(r);
    p.Deliver(1.0, 0.5);
    SC_EXPECT(p.Find("depot") == nullptr);
    p.Include("depot");
    SC_EXPECT(!p.Held("depot"));
    p.Queue(r);
    p.Deliver(1.0, 0.5);
    SC_EXPECT(p.Find("depot") != nullptr);
}

SC_TEST("ground air (F3.7): the nation's Pact war: tied troops as their units, supply lines to where they are, overrun out of the targets") {
    std::string e;
    AirWarData d;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json", d, &e));
    d.tuning.values["ground.mode"] = 1.0;
    AirCampaign c;
    SC_EXPECT(c.Init(d, 21, &e));
    if (c.Ground() == nullptr) return;
    const GroundWar& g = *c.Ground();
    SC_EXPECT(g.Ties().size() >= 8u);
    RunTo(c, 72.0);
    SC_EXPECT(c.FrontLine().Moved());
    const LineOfContact& line = c.FrontLine();
    for (std::size_t k = 0; k < g.Ties().size(); ++k) {
        const TroopsTie& t = g.Ties()[k];
        const ObjectState& st = c.Truth()[static_cast<std::size_t>(t.object)];
        SC_EXPECT_NEAR(st.function, g.TroopsFunction(k, g.Now()), 0.02);  // set on a loss or gain, else daily (the rest rate between)
        SC_EXPECT(!st.overrun);
        // Every line ending at it ends where it is (unless the way to it is cut).
        for (std::size_t li = 0; li < c.Data().theatre.supply_lines.size(); ++li) {
            if (c.Data().theatre.IndexOf(c.Data().theatre.supply_lines[li].to) != t.object) continue;
            const SupplyPath& p = c.Supply().Path(li);
            if (p.cut || p.pts.empty()) continue;
            SC_EXPECT(Dist2d(p.pts.back(), st.pos) < 1.0);
        }
    }
    int overrun = 0;
    for (std::size_t i = 0; i < c.Truth().size(); ++i) {
        const ObjectState& st = c.Truth()[i];
        if (!st.overrun) continue;
        ++overrun;
        SC_EXPECT(st.function == 0.0);
        SC_EXPECT(line.DistanceTo(st.pos) < 0.0);
        const IntelEntry* ie = c.Intel().Find(c.Data().theatre.objects[i].id);
        SC_EXPECT(ie == nullptr || ie->ghost);
    }
    const GroundAirStats& s = c.GroundAir();
    std::printf("    %zu ties; %d strikes on troops (%.3f of establishments), %d overrun, %d escapes, %d moves (%.0f km), %d reroutes; "
                "labels cas %d bai %d ai %d\n",
                g.Ties().size(), s.troop_strikes, s.strength_lost, overrun, s.escapes, s.troop_moves, s.troops_moved_km, s.supply_reroutes, s.cas,
                s.bai, s.ai);
    SC_EXPECT(s.cas + s.bai + s.ai > 0);
    SC_EXPECT(s.bai == s.bai_fixed && s.ai == s.ai_fixed);  // the Pact draws no FSCL: the labels as c2.bai_depth_m gives them
}

SC_TEST("ground air (F3.7): NATO's war draws an FSCL per sector, and C2's labels read it") {
    std::string e;
    AirWarData d;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_nato.json", d, &e));
    d.tuning.values["ground.mode"] = 1.0;
    AirCampaign c;
    SC_EXPECT(c.Init(d, 21, &e));
    if (c.Ground() == nullptr) return;
    const AirFrontView v = c.FrontView();
    SC_EXPECT(v.fscl_m != nullptr && v.fscl_m->size() == c.FrontLine().SectorCount());
    RunTo(c, 48.0);
    const GroundAirStats& s = c.GroundAir();
    std::printf("    labels with the FSCL cas %d bai %d ai %d; by c2.bai_depth_m bai %d ai %d\n", s.cas, s.bai, s.ai, s.bai_fixed, s.ai_fixed);
    SC_EXPECT(s.bai + s.ai == s.bai_fixed + s.ai_fixed);
    for (std::size_t i = 0; i < c.FrontLine().SectorCount(); ++i) SC_EXPECT((*c.FrontView().fscl_m)[i] >= 20000.0);
}

SC_TEST("ground air (F3.7): ground.mode 0: nothing tied, overrun or labelled; no FSCL") {
    std::string e;
    AirWarData d;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_nato.json", d, &e));
    AirCampaign c;
    SC_EXPECT(c.Init(d, 21, &e));
    SC_EXPECT(c.Ground() == nullptr);
    RunTo(c, 24.0);
    const GroundAirStats& s = c.GroundAir();
    SC_EXPECT(s.troop_strikes == 0 && s.overrun == 0 && s.escapes == 0 && s.troop_moves == 0 && s.cas + s.bai + s.ai == 0);
    for (const ObjectState& st : c.Truth()) SC_EXPECT(!st.overrun);
    const AirFrontView v = c.FrontView();
    SC_EXPECT(v.fscl_m == nullptr || v.fscl_m->empty());
    SC_EXPECT_NEAR(v.BaiDepth(Vec3{}, 30000.0), 30000.0, 1e-12);
}
