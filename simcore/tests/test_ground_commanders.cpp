// Track F, F3.4: the ground war's commanders (simcore/ground_war/commanders.hpp, ground_war.hpp; docs/fronts/03_GROUND_WAR.md
// section 5 and 8). The rules by hand (a unit's decision on its own strength, where an echelon goes, the answer to a request,
// the thresholds a commander's traits move); then hand wars on a straight 100 km line: no withdrawal without an order and
// dispersal under the line when none comes, a relief and the withdrawal as a unit it allows, the second echelon to success,
// NATO's breakpoints, our counterattack to the border against a covering force's screen, the rebels' raids by night where
// we are thin; the order-free draws; and every faction's war on the nation, bounded and with no allocation in the hour.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
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

// Lines high enough that a defender reaches them in days (test values, not the doctrine's 2/3 and 1/2).
EffectivenessDef TestLines() {
    EffectivenessDef e;
    e.kind = EffectivenessKind::Lines;
    e.degraded_below = 0.95;
    e.ineffective_below = 0.9;
    e.hq_hit_ineffective_below = 0.6;
    return e;
}

// A straight 100 km line along the east axis from (0, 0); the enemy 50 km south, our country north. Sectors west
// (0-50 km) and east (50-100 km).
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

// The bare data: the tables (no draw, the commanders on the truth), both doctrines on the test lines, the enemy's caps; no
// formations yet.
std::shared_ptr<GroundWarData> Bare() {
    auto d = std::make_shared<GroundWarData>();
    d->faction = "test";
    d->tables = Tables();
    d->tables.draw_sigma = 0.0;
    d->tables.picture.truth = true;  // the rules on the truth: each side's picture (F3.5) is test_ground_picture.cpp's
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
    return d;
}

// An axis north through a sector at `along_km` towards a goal `deep_km` in.
void AddAxis(GroundWarData& d, const std::string& id, const std::string& sector, double along_km, double deep_km, std::vector<std::string> first,
             std::vector<std::string> second = {}) {
    ObjectiveDef goal;
    goal.id = id + "_obj";
    goal.name = id + "'s goal";
    goal.place = At(id + "_goal", along_km, deep_km);
    d.enemy.plan.objectives.push_back(goal);
    AxisDef ax;
    ax.id = id;
    ax.name = "the " + id + " axis";
    ax.main = d.enemy.plan.axes.empty();
    ax.route = {At(id + "_start", along_km, -20), At(id + "_crossing", along_km, 0), At(id + "_goal", along_km, deep_km)};
    ax.first_echelon = std::move(first);
    ax.second_echelon = std::move(second);
    ax.objective = goal.id;
    ax.sector = sector;
    d.enemy.plan.axes.push_back(ax);
}

void Deploy(GroundWarData& d, const std::string& unit, DeployRole role, std::vector<std::string> sectors, double e_km = 50, double n_km = 40) {
    DeployedUnitDef u;
    u.unit = unit;
    u.role = role;
    u.sectors = std::move(sectors);
    u.place = At(unit + "_place", e_km, n_km);
    d.ours.deployments.front().units.push_back(u);
}

int FormationOf(const GroundWar& g, const std::string& id) {
    for (std::size_t i = 0; i < g.Formations().size(); ++i) {
        if (g.Formations()[i].unit.id == id) return static_cast<int>(i);
    }
    return -1;
}

// The orders logged since the last ClearLog, kept: the tests read the whole war's.
struct Logged {
    std::vector<GroundLogEntry> all;
    void Take(GroundWar& g) {
        all.insert(all.end(), g.Log().begin(), g.Log().end());
        g.ClearLog();
    }
    int Count(GroundOrder o, int formation = -1) const {
        int n = 0;
        for (const GroundLogEntry& e : all) n += e.what == GroundNote::Order && e.order == o && (formation < 0 || e.formation == formation) ? 1 : 0;
        return n;
    }
    const GroundLogEntry* First(GroundOrder o, int formation = -1) const {
        for (const GroundLogEntry& e : all) {
            if (e.what == GroundNote::Order && e.order == o && (formation < 0 || e.formation == formation)) return &e;
        }
        return nullptr;
    }
    void Print(const GroundWar& g) const {
        for (const GroundLogEntry& e : all) {
            if (e.what != GroundNote::Order) continue;
            std::printf("      h %5.0f %-22s %-10s %8.3f\n", e.h, GroundOrderName(e.order),
                        e.formation >= 0 ? g.Formations()[static_cast<std::size_t>(e.formation)].unit.id.c_str() : "-", e.value);
        }
    }
};

// Steps a war hour by hour to `hours`, its log kept; `each` is called after every hour.
template <typename F>
void Run(GroundWar& g, LineOfContact& line, const GroundEnv& env, int hours, Logged& log, F each) {
    for (int h = 1; h <= hours; ++h) {
        g.Step(static_cast<double>(h), env, line);
        log.Take(g);
        each(h);
    }
}

}  // namespace

// ------------------------------------------------------------------------------------------------------------ the rules

SC_TEST("ground commanders (F3.4): a trait moves each threshold by at most 0.2 of its range, the right way") {
    const GroundTables& t = Tables();
    GroundDoctrineDef pact;
    pact.attack_ratio = 3.0;
    pact.effectiveness.kind = EffectivenessKind::Lines;
    pact.effectiveness.degraded_below = 0.667;
    pact.effectiveness.ineffective_below = 0.5;
    pact.security_delay_per_line_h = ValueRange{1.0, 2.0};
    Traits lo, mid, hi;
    lo.boldness = lo.rigidity = lo.composure = lo.methodical = 0.0;
    hi.boldness = hi.rigidity = hi.composure = hi.methodical = 1.0;
    mid.boldness = mid.rigidity = mid.composure = mid.methodical = 0.5;
    const double s = t.trait_shift;
    SC_EXPECT_NEAR(s, 0.2, 1e-12);
    // The attack ratio: 3 in 2-4; a bold man lower, a timid one higher, by 0.2 x 2 at most.
    SC_EXPECT_NEAR(AttackRatioOf(t, pact, mid), 3.0, 1e-12);
    SC_EXPECT_NEAR(AttackRatioOf(t, pact, hi), 3.0 - s * 2.0, 1e-9);
    SC_EXPECT_NEAR(AttackRatioOf(t, pact, lo), 3.0 + s * 2.0, 1e-9);
    // The counterattack's: 2 in 1.5-2.5.
    SC_EXPECT_NEAR(CounterattackRatioOf(t, hi), 2.0 - s * 1.0, 1e-9);
    SC_EXPECT_NEAR(CounterattackRatioOf(t, lo), 2.0 + s * 1.0, 1e-9);
    // The line he asks under: the ineffective 1/2; a flexible man earlier (towards 2/3), a rigid one no later than it.
    SC_EXPECT_NEAR(AskWithdrawLineOf(t, pact.effectiveness, mid), 0.5, 1e-12);
    SC_EXPECT_NEAR(AskWithdrawLineOf(t, pact.effectiveness, lo), 0.5 + s * 0.167, 1e-9);
    SC_EXPECT(AskWithdrawLineOf(t, pact.effectiveness, hi) >= 0.5 - 1e-12);
    // A stall stopped later by a rigid man; a composed man holds longer without an order.
    SC_EXPECT(StallHoursOf(t, hi) > StallHoursOf(t, lo));
    SC_EXPECT(StallHoursOf(t, hi) - StallHoursOf(t, mid) <= s * (t.stall_h.hi - t.stall_h.lo) + 1e-9);
    SC_EXPECT_NEAR(HoldWithoutOrderHoursOf(t, hi) - HoldWithoutOrderHoursOf(t, mid), s * (t.hold_without_order_h.hi - t.hold_without_order_h.lo), 1e-9);
    // A security line held longer by a rigid division commander, within the doctrine's 1-2 h.
    for (double u : {0.0, 0.5, 1.0}) {
        const double h0 = SecurityLineHoursOf(t, pact, lo, u), h1 = SecurityLineHoursOf(t, pact, hi, u);
        SC_EXPECT(h0 >= 1.0 - 1e-12 && h1 <= 2.0 + 1e-12 && h1 >= h0);
    }
    // A methodical man takes the long end of his reaction time, never outside FM 100-2-1's range.
    for (double u : {0.0, 0.3, 0.9}) {
        const double fast = DecideHoursOf(t, Echelon::Division, false, lo, u), slow = DecideHoursOf(t, Echelon::Division, false, hi, u);
        SC_EXPECT(slow >= fast && fast >= t.reaction_h[0].lo - 1e-12 && slow <= t.reaction_h[0].hi + 1e-12);
        SC_EXPECT(slow - fast <= 2.0 * s * (t.reaction_h[0].hi - t.reaction_h[0].lo) + 1e-9);
    }
    // A breakpoint fought on further by a composed man, by 0.2 of its range at most.
    EffectivenessDef nato;
    nato.kind = EffectivenessKind::Breakpoints;
    nato.attack_stop_losses = ValueRange{0.15, 0.35};
    nato.defence_break_losses = ValueRange{0.25, 0.5};
    const double b_mid = DrawBreakpoint(nato, true, 0.5, BreakpointPullOf(mid), s), b_hi = DrawBreakpoint(nato, true, 0.5, BreakpointPullOf(hi), s);
    SC_EXPECT(b_hi > b_mid && b_hi - b_mid <= s * 0.2 + 1e-9);
}

SC_TEST("ground commanders (F3.4): commanders drawn from a pool, keyed: the same fork the same man") {
    GroundForceDef pact;
    std::string e;
    SC_EXPECT(LoadGroundForce(kData, "warsaw_pact", pact, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const OfficerPoolDef& pool = pact.commanders.pool;
    SC_EXPECT(!pool.last_names.empty() && !pool.ranks.empty());
    Rng root(99);
    Rng a = root.Fork(5), b = root.Fork(5), c = root.Fork(6);
    const GroundCommander x = DrawCommander(pool, "division", a), y = DrawCommander(pool, "division", b), z = DrawCommander(pool, "regiment", c);
    SC_EXPECT(x.name == y.name && x.traits.boldness == y.traits.boldness && x.age == y.age);
    SC_EXPECT(x.echelon == Echelon::Division && z.echelon == Echelon::Regiment);
    SC_EXPECT(x.rank == pool.ranks.at("division"));
    for (double v : {x.traits.boldness, x.traits.rigidity, x.traits.composure, x.traits.methodical, z.traits.boldness}) SC_EXPECT(v >= 0.0 && v <= 1.0);
    std::printf("    %s (%s, %d); %s\n", x.Title().c_str(), x.background.c_str(), x.age, z.Title().c_str());
}

SC_TEST("ground commanders (F3.4): a unit's own decision: report, ask, never withdraw without an order, disperse when none comes") {
    UnitSituation s;
    s.lines = true;
    s.degraded_line = 0.667;
    s.ask_line = 0.5;
    s.hold_h = 6.0;
    s.needs_order = true;
    s.fraction = 0.8;
    SC_EXPECT(DecideUnit(s) == UnitAct::None);
    s.fraction = 0.6;
    SC_EXPECT(DecideUnit(s) == UnitAct::Report);
    s.reported = true;
    SC_EXPECT(DecideUnit(s) == UnitAct::None);
    s.fraction = 0.45;
    SC_EXPECT(DecideUnit(s) == UnitAct::Ask);  // never Withdraw: the Pact's and our doctrine need the senior's order
    s.asked = true;
    s.engaged = true;
    s.asked_for_h = 3.0;
    SC_EXPECT(DecideUnit(s) == UnitAct::None);  // holds while it can
    s.asked_for_h = 6.0;
    SC_EXPECT(DecideUnit(s) == UnitAct::Disperse);  // in contact, no order: it breaks
    s.engaged = false;
    SC_EXPECT(DecideUnit(s) == UnitAct::None);  // out of contact it waits
    s.has_order = true;
    SC_EXPECT(DecideUnit(s) == UnitAct::Withdraw);  // ordered: as a unit
    s.has_order = false;
    s.is_top = true;
    SC_EXPECT(DecideUnit(s) == UnitAct::Withdraw);  // the side's top is his own senior
    // NATO's breakpoints: an attack stops on its own, a defence breaks off on its own.
    UnitSituation n;
    n.lines = false;
    n.needs_order = false;
    n.attack_stop = 0.2;
    n.defence_break = 0.3;
    n.attacking = true;
    n.fraction = 0.85;
    SC_EXPECT(DecideUnit(n) == UnitAct::None);
    n.fraction = 0.79;
    SC_EXPECT(DecideUnit(n) == UnitAct::StopAttack);
    n.attack_stopped = true;
    SC_EXPECT(DecideUnit(n) == UnitAct::None);
    n.fraction = 0.69;
    SC_EXPECT(DecideUnit(n) == UnitAct::Withdraw);
    // The rebels disperse rather than retreat.
    n.disperse_not_retreat = true;
    SC_EXPECT(DecideUnit(n) == UnitAct::Disperse);
    n.attack_stopped = false;
    n.fraction = 0.79;
    SC_EXPECT(DecideUnit(n) == UnitAct::Disperse);
}

SC_TEST("ground commanders (F3.4): the echelon to success, NATO's to its own axis; our army's answer to a request") {
    AxisChoice ax[3];
    ax[0].open = ax[0].attacking = true;
    ax[0].pace_km_day = 1.0;  // its own: stalling
    ax[0].ratio_with = 2.0;
    ax[1].open = ax[1].attacking = true;
    ax[1].pace_km_day = 8.0;  // moving
    ax[1].ratio_with = 1.5;
    ax[2].open = true;  // halted short of its goal
    ax[2].ratio_with = 4.0;
    SC_EXPECT(EchelonTarget(ax, 3, 0, true, 2.8, 3.0) == 1);   // the Pact: to the one that moves
    SC_EXPECT(EchelonTarget(ax, 3, 0, false, 2.8, 3.0) == 0);  // NATO: its own while open
    ax[0].pace_km_day = 5.0;
    SC_EXPECT(EchelonTarget(ax, 3, 0, true, 2.8, 3.0) == 1);  // another that moves faster
    ax[0].pace_km_day = 9.0;
    SC_EXPECT(EchelonTarget(ax, 3, 0, true, 2.8, 3.0) == 0);  // its own, the fastest
    ax[0].pace_km_day = ax[1].pace_km_day = 0.5;
    SC_EXPECT(EchelonTarget(ax, 3, 0, true, 2.8, 3.0) == 2);  // none moves: where its weight brings the attack ratio
    ax[2].ratio_with = 2.5;
    SC_EXPECT(EchelonTarget(ax, 3, 0, true, 2.8, 3.0) == -1);  // nowhere: held back
    SC_EXPECT(AnswerRequest(true, true, false) == RequestAnswer::Permit);
    SC_EXPECT(AnswerRequest(false, true, false) == RequestAnswer::Relieve);
    SC_EXPECT(AnswerRequest(false, false, false) == RequestAnswer::Refuse);
    SC_EXPECT(AnswerRequest(false, false, true) == RequestAnswer::Permit);
}

// ------------------------------------------------------------------------------------------------------------ the wars

SC_TEST("ground commanders (F3.4): no withdrawal without an order; under the line with none it disperses, re-forms smaller") {
    std::shared_ptr<GroundWarData> d = Bare();
    d->enemy.oob.units.push_back(Unit("red_div", "division", 700, 10000));
    AddAxis(*d, "east", "east", 75, 200, {"red_div"});
    d->ours.oob.units.push_back(Unit("blue_army", "army", 0, 500));
    d->ours.oob.units.push_back(Unit("blue_div", "division", 160, 10000, "blue_army"));
    d->ours.oob.units.push_back(Unit("blue_art", "brigade", 0, 2000, "blue_army"));
    Deploy(*d, "blue_div", DeployRole::FirstEchelon, {"east"});
    Deploy(*d, "blue_art", DeployRole::Support, {});
    LineOfContact line;
    line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;
    GroundEnv env;
    env.theatre = &theatre;
    GroundWar g;
    std::string e;
    SC_EXPECT(g.Build(d, line, theatre, nullptr, 7, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const int blue = FormationOf(g, "blue_div");
    SC_EXPECT(blue >= 0);
    if (blue < 0) return;
    // The army commander is the side's top; the division's commander answers to him.
    const int top = g.TopOf(Side::Ours);
    SC_EXPECT(top >= 0 && g.Commanders()[static_cast<std::size_t>(top)].unit == "blue_army");
    const GroundCommander& dc = g.Commanders()[static_cast<std::size_t>(g.Formations()[static_cast<std::size_t>(blue)].cmdr)];
    SC_EXPECT(dc.senior == top && !dc.top);
    Logged log;
    bool out_before_order = false;
    Run(g, line, env, 24 * 12, log, [&](int) {
        const Formation& f = g.Formations()[static_cast<std::size_t>(blue)];
        if (f.unit.state == UnitState::Reforming) out_before_order = true;  // withdrawn as a unit: needs an order it never got
    });
    log.Print(g);
    SC_EXPECT(!out_before_order);
    SC_EXPECT(log.Count(GroundOrder::WithdrawAsked, blue) >= 1);
    SC_EXPECT(log.Count(GroundOrder::WithdrawRefused, blue) == 1);  // nobody behind, nobody free
    SC_EXPECT(log.Count(GroundOrder::WithdrawOrdered, blue) == 0 && log.Count(GroundOrder::WithdrawDecided, blue) == 0);
    const GroundLogEntry* asked = log.First(GroundOrder::WithdrawAsked, blue);
    const GroundLogEntry* broke = log.First(GroundOrder::Dispersed, blue);
    SC_EXPECT(asked != nullptr && broke != nullptr);
    if (asked == nullptr || broke == nullptr) return;
    SC_EXPECT(asked->value < 0.95 + 1e-9);
    // Not before he had held as long as his composure lets him (his reaction time after the line came first).
    SC_EXPECT(broke->h - asked->h >= HoldWithoutOrderHoursOf(Tables(), dc.traits) - 1e-6 || broke->value >= 0.5);
    SC_EXPECT(g.Formations()[static_cast<std::size_t>(blue)].dispersals >= 1);
    // Its remnants re-form smaller in 1-3 days.
    const GroundLogEntry* back = log.First(GroundOrder::Reformed, blue);
    SC_EXPECT(back != nullptr);
    if (back != nullptr) {
        SC_EXPECT(back->h - broke->h >= 24.0 * Tables().reassemble_days.lo - 1e-6 && back->h - broke->h <= 24.0 * Tables().reassemble_days.hi + 1e-6);
        SC_EXPECT(back->value < 0.9);
    }
}

SC_TEST("ground commanders (F3.4): a relief sent: the division withdraws as a unit on the army's order and re-forms") {
    std::shared_ptr<GroundWarData> d = Bare();
    d->enemy.oob.units.push_back(Unit("red_div", "division", 700, 10000));
    AddAxis(*d, "east", "east", 75, 200, {"red_div"});
    d->ours.oob.units.push_back(Unit("blue_army", "army", 0, 500));
    d->ours.oob.units.push_back(Unit("blue_div", "division", 160, 10000, "blue_army"));
    d->ours.oob.units.push_back(Unit("blue_res", "division", 160, 10000, "blue_army"));
    Deploy(*d, "blue_div", DeployRole::FirstEchelon, {"east"});
    Deploy(*d, "blue_res", DeployRole::Reserve, {}, 75, 40);
    LineOfContact line;
    line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;
    GroundEnv env;
    env.theatre = &theatre;
    GroundWar g;
    SC_EXPECT(g.Build(d, line, theatre, nullptr, 7));
    const int blue = FormationOf(g, "blue_div"), res = FormationOf(g, "blue_res");
    Logged log;
    Run(g, line, env, 24 * 2, log, [](int) {});
    log.Print(g);
    const GroundLogEntry* asked = log.First(GroundOrder::WithdrawAsked, blue);
    const GroundLogEntry* sent = log.First(GroundOrder::ReliefOrdered, res);
    const GroundLogEntry* done = log.First(GroundOrder::Relieved, blue);
    SC_EXPECT(asked != nullptr && sent != nullptr && done != nullptr);
    if (asked == nullptr || sent == nullptr || done == nullptr) return;
    SC_EXPECT(sent->other == blue && sent->h >= asked->h && done->h > sent->h);
    SC_EXPECT(log.Count(GroundOrder::WithdrawRefused, blue) == 0);
    const Formation& b = g.Formations()[static_cast<std::size_t>(blue)];
    SC_EXPECT(b.withdrawals == 1);
    SC_EXPECT(log.First(GroundOrder::Dispersed, blue) == nullptr || log.First(GroundOrder::Dispersed, blue)->h > done->h);
    // The relief holds the sector now.
    const Axis& x = g.Axes()[0];
    bool holds = false;
    for (int fi : x.defenders) holds = holds || fi == res;
    SC_EXPECT(holds);
    SC_EXPECT(b.spent && b.unit.state == UnitState::Reforming);
}

SC_TEST("ground commanders (F3.4): the Pact's second echelon goes to the axis that moves; NATO's to its own") {
    for (int to_success = 1; to_success >= 0; --to_success) {
        std::shared_ptr<GroundWarData> d = Bare();
        d->enemy.doctrine.second_echelon_to_success = to_success == 1;
        d->enemy.doctrine.effectiveness.degraded_below = 0.667;  // the doctrine's lines: the plan's day decides, not a worn axis
        d->enemy.doctrine.effectiveness.ineffective_below = 0.5;
        d->enemy.oob.units.push_back(Unit("red_army", "army", 0, 500));
        d->enemy.oob.units.push_back(Unit("red_east", "division", 600, 10000, "red_army"));
        d->enemy.oob.units.push_back(Unit("red_west", "division", 300, 10000, "red_army"));
        d->enemy.oob.units.push_back(Unit("red_td", "division", 300, 8000, "red_army"));
        AddAxis(*d, "east", "east", 75, 80, {"red_east"});
        AddAxis(*d, "west", "west", 25, 80, {"red_west"}, {"red_td"});
        d->ours.oob.units.push_back(Unit("blue_army", "army", 0, 500));
        d->ours.oob.units.push_back(Unit("blue_e", "division", 60, 6000, "blue_army"));
        d->ours.oob.units.push_back(Unit("blue_w", "division", 500, 12000, "blue_army"));
        Deploy(*d, "blue_e", DeployRole::FirstEchelon, {"east"});
        Deploy(*d, "blue_w", DeployRole::FirstEchelon, {"west"});
        LineOfContact line;
        line.Build(StraightFront(), 5000.0);
        TheatreDef theatre;
        GroundEnv env;
        env.theatre = &theatre;
        GroundWar g;
        SC_EXPECT(g.Build(d, line, theatre, nullptr, 7));
        SC_EXPECT(g.Commanders()[static_cast<std::size_t>(g.TopOf(Side::Enemy))].unit == "red_army");
        const int td = FormationOf(g, "red_td");
        Logged log;
        Run(g, line, env, 24 * 3, log, [](int) {});
        const GroundLogEntry* c = log.First(GroundOrder::EchelonCommitted, td);
        SC_EXPECT(c != nullptr);
        if (c == nullptr) continue;
        std::printf("    %s: red_td committed at h %.0f to %s (east %.1f km in, west %.1f)\n", to_success ? "to success" : "to its own",
                    c->h, g.Axes()[static_cast<std::size_t>(c->axis)].id.c_str(), g.Axes()[0].depth_m / 1000.0, g.Axes()[1].depth_m / 1000.0);
        // After the plan's day, its senior's reaction and planning.
        SC_EXPECT(c->h >= 2.0 + Tables().second_echelon_after_h - 1e-9);
        if (to_success == 1) {
            SC_EXPECT(c->axis == 0 && c->other == 1);  // switched from its planned west to the east that moves
        } else {
            SC_EXPECT(c->axis == 1);
        }
        SC_EXPECT(g.Formations()[static_cast<std::size_t>(td)].axis == c->axis);  // it has arrived and attacks there
    }
}

SC_TEST("ground commanders (F3.4): NATO's breakpoints: an attack stops on its own, no permission asked; the axis halts") {
    std::shared_ptr<GroundWarData> d = Bare();
    EffectivenessDef& bp = d->enemy.doctrine.effectiveness;
    bp.kind = EffectivenessKind::Breakpoints;
    bp.attack_stop_losses = ValueRange{0.06, 0.06};
    bp.defence_break_losses = ValueRange{0.3, 0.3};
    d->enemy.doctrine.withdraw_needs_order = false;
    d->enemy.oob.units.push_back(Unit("red_corps", "corps", 0, 500));
    d->enemy.oob.units.push_back(Unit("red_a", "division", 350, 10000, "red_corps"));
    d->enemy.oob.units.push_back(Unit("red_b", "division", 350, 10000, "red_corps"));
    AddAxis(*d, "east", "east", 75, 80, {"red_a", "red_b"});
    d->ours.doctrine.effectiveness.degraded_below = 0.667;  // ours on the doctrine's lines: they hold
    d->ours.doctrine.effectiveness.ineffective_below = 0.5;
    d->ours.oob.units.push_back(Unit("blue_army", "army", 0, 500));
    d->ours.oob.units.push_back(Unit("blue_div", "division", 350, 12000, "blue_army"));
    d->ours.oob.units.push_back(Unit("blue_art", "brigade", 0, 2000, "blue_army"));
    Deploy(*d, "blue_div", DeployRole::FirstEchelon, {"east"});
    Deploy(*d, "blue_art", DeployRole::Support, {});
    LineOfContact line;
    line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;
    GroundEnv env;
    env.theatre = &theatre;
    GroundWar g;
    SC_EXPECT(g.Build(d, line, theatre, nullptr, 7));
    const int a = FormationOf(g, "red_a");
    SC_EXPECT_NEAR(g.Formations()[static_cast<std::size_t>(a)].attack_stop, 0.06, 1e-12);
    Logged log;
    bool attacked_past = false;
    Run(g, line, env, 24 * 10, log, [&](int) {
        const Formation& f = g.Formations()[static_cast<std::size_t>(a)];
        // Past its breakpoint by more than a reaction time's losses it has stopped.
        if (!f.spent && !f.attack_stopped && 1.0 - f.unit.Fraction() > 0.12 && g.Axes()[0].status == AxisStatus::Attacking) attacked_past = true;
    });
    log.Print(g);
    SC_EXPECT(!attacked_past);
    SC_EXPECT(log.Count(GroundOrder::AttackStopped) >= 1);
    SC_EXPECT(log.Count(GroundOrder::WithdrawAsked) == 0);  // nobody asks
    const GroundLogEntry* stop = log.First(GroundOrder::AttackStopped, a);
    SC_EXPECT(stop != nullptr && stop->value <= 1.0 - 0.06 + 1e-9);
    SC_EXPECT(g.Axes()[0].status == AxisStatus::Halted);
    std::printf("    the axis %s %.1f km in\n", HaltReasonName(g.Axes()[0].halted), g.Axes()[0].depth_m / 1000.0);
}

namespace {

// Our counterattack's case: red_div and its covering force red_acr into the east sector against a thin blue_div; a tank
// division in reserve 40 km behind.
std::shared_ptr<GroundWarData> CounterData(bool covering) {
    std::shared_ptr<GroundWarData> d = Bare();
    d->enemy.oob.units.push_back(Unit("red_corps", "corps", 0, 500));
    d->enemy.oob.units.push_back(Unit("red_div", "division", 300, 10000, "red_corps"));
    d->enemy.oob.units.push_back(Unit("red_acr", "regiment", 120, 4000, "red_corps"));
    AddAxis(*d, "east", "east", 75, 60, {"red_div", "red_acr"});
    if (covering) d->enemy.plan.covering_force = {"red_acr"};
    d->ours.oob.units.push_back(Unit("blue_army", "army", 0, 500));
    d->ours.oob.units.push_back(Unit("blue_div", "division", 60, 6000, "blue_army"));
    d->ours.oob.units.push_back(Unit("blue_td", "division", 1500, 12000, "blue_army"));
    Deploy(*d, "blue_div", DeployRole::FirstEchelon, {"east"});
    Deploy(*d, "blue_td", DeployRole::Reserve, {}, 75, 40);
    d->ours.deployments.front().security_zone_km = 15.0;  // the main defensive zone 15 km in: the counterattack meets the penetration there
    return d;
}

}  // namespace

SC_TEST("ground commanders (F3.4): our counterattack into the penetration, back to the border at most; the covering force screens") {
    std::shared_ptr<GroundWarData> d = CounterData(true);
    LineOfContact line;
    line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;
    GroundEnv env;
    env.theatre = &theatre;
    GroundWar g;
    SC_EXPECT(g.Build(d, line, theatre, nullptr, 7));
    const int div = FormationOf(g, "red_div"), acr = FormationOf(g, "red_acr"), td = FormationOf(g, "blue_td");
    Logged log;
    double deepest = 0.0, at_start = -1.0, div_at = 0.0, acr_at = 0.0, div_loss = 0.0, acr_loss = 0.0;
    bool never_negative = true;
    Run(g, line, env, 24 * 6, log, [&](int) {
        const Axis& x = g.Axes()[0];
        deepest = std::max(deepest, x.depth_m);
        never_negative = never_negative && x.depth_m >= 0.0;
        for (std::size_t i = 0; i < line.StationCount(); ++i) never_negative = never_negative && line.StationOffset(i) >= 0.0;
        const double fd = g.Formations()[static_cast<std::size_t>(div)].unit.strength, fa = g.Formations()[static_cast<std::size_t>(acr)].unit.strength;
        if (x.status == AxisStatus::Counterattacked) {
            if (at_start < 0.0) {
                at_start = x.depth_m;
            } else {
                div_loss += div_at > 0.0 ? 1.0 - fd / div_at : 0.0;
                acr_loss += acr_at > 0.0 ? 1.0 - fa / acr_at : 0.0;
            }
        }
        div_at = fd;
        acr_at = fa;
    });
    log.Print(g);
    const GroundLogEntry* ordered = log.First(GroundOrder::CounterattackOrdered);
    const GroundLogEntry* starts = log.First(GroundOrder::CounterattackStarts, td);
    SC_EXPECT(ordered != nullptr && starts != nullptr);
    if (ordered == nullptr || starts == nullptr) return;
    SC_EXPECT(ordered->cmdr == g.TopOf(Side::Ours));  // the army commander's
    SC_EXPECT(ordered->value >= CounterattackRatioOf(Tables(), g.Commanders()[static_cast<std::size_t>(ordered->cmdr)].traits) - 1e-9);
    SC_EXPECT(log.First(GroundOrder::CounterattackEnds) != nullptr);
    SC_EXPECT(never_negative);
    SC_EXPECT(g.Axes()[0].depth_m < at_start);  // pushed back
    // The covering force alone met the counterattack: the division behind it lost less, share for share.
    SC_EXPECT(acr_loss > div_loss);
    std::printf("    deepest %.1f km, counterattack from %.1f km, now %.1f km (%s); losses under it: screen %.3f, division %.3f\n", deepest / 1000.0,
                at_start / 1000.0, g.Axes()[0].depth_m / 1000.0,
                g.Axes()[0].status == AxisStatus::Halted ? HaltReasonName(g.Axes()[0].halted) : "on", acr_loss, div_loss);
}

SC_TEST("ground commanders (F3.4): keyed and order-free: the same war resolved either way round, the same orders") {
    std::shared_ptr<GroundWarData> d = CounterData(false);
    d->tables.draw_sigma = Tables().draw_sigma;
    TheatreDef theatre;
    GroundEnv env;
    env.theatre = &theatre;
    LineOfContact la, lb;
    la.Build(StraightFront(), 5000.0);
    lb.Build(StraightFront(), 5000.0);
    GroundWar a, b;
    SC_EXPECT(a.Build(d, la, theatre, nullptr, 3) && b.Build(d, lb, theatre, nullptr, 3));
    b.SetReverseOrder(true);
    Logged x, y;
    Run(a, la, env, 24 * 6, x, [](int) {});
    Run(b, lb, env, 24 * 6, y, [](int) {});
    bool same = x.all.size() == y.all.size() && !x.all.empty();
    for (std::size_t k = 0; same && k < x.all.size(); ++k) {
        same = x.all[k].h == y.all[k].h && x.all[k].what == y.all[k].what && x.all[k].order == y.all[k].order &&
               x.all[k].formation == y.all[k].formation && x.all[k].value == y.all[k].value;
    }
    SC_EXPECT(same);
    for (std::size_t i = 0; i < a.Commanders().size(); ++i) SC_EXPECT(a.Commanders()[i].name == b.Commanders()[i].name);
    std::printf("    %zu entries, %d orders\n", x.all.size(), static_cast<int>(x.all.size()));
}

SC_TEST("ground commanders (F3.4): the rebels raid by night where we are thin, back to their line by dawn, never deeper than the plan") {
    std::shared_ptr<GroundWarData> d = Bare();
    d->enemy.plan.kind = PlanKind::Raids;
    d->enemy.plan.start_after_h = 0.0;
    d->enemy.plan.has_raids = true;
    d->enemy.plan.raids.night_only = true;
    d->enemy.plan.raids.min_ratio = 3.0;
    d->enemy.plan.raids.max_depth_km = 15.0;
    d->enemy.plan.raids.per_night = 2;
    d->enemy.plan.raids.lines = {"reb_a", "reb_b"};
    d->enemy.doctrine.effectiveness.kind = EffectivenessKind::Breakpoints;
    d->enemy.doctrine.effectiveness.attack_stop_losses = ValueRange{0.2, 0.3};
    d->enemy.doctrine.effectiveness.defence_break_losses = ValueRange{0.3, 0.4};
    d->enemy.doctrine.disperse_rather_than_retreat = true;
    d->enemy.doctrine.caps.breakthrough_km_per_day = 3.0;
    d->enemy.doctrine.caps.depth_km_per_day = 5.0;
    d->enemy.doctrine.caps.broken_km_per_day = 8.0;
    d->enemy.oob.units.push_back(Unit("reb_cmd", "group", 0, 100));
    d->enemy.oob.units.push_back(Unit("reb_a", "brigade", 10, 1500, "reb_cmd"));
    d->enemy.oob.units.push_back(Unit("reb_b", "brigade", 10, 1500, "reb_cmd"));
    d->enemy.garrisons = {GroundPlace(), At("a_camp", 25, -5), At("b_camp", 75, -5)};
    d->ours.oob.units.push_back(Unit("blue_reg", "regiment", 90, 3000));
    Deploy(*d, "blue_reg", DeployRole::FirstEchelon, {"east"});
    LineOfContact line;
    line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;
    GroundEnv env;
    env.theatre = &theatre;
    GroundWar g;
    std::string e;
    SC_EXPECT(g.Build(d, line, theatre, nullptr, 7, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(g.Axes().size() == 2);
    if (g.Axes().size() != 2) return;
    const int a = FormationOf(g, "reb_a"), b = FormationOf(g, "reb_b");
    SC_EXPECT(g.Axes()[0].raid && g.Axes()[0].sector == 0 && g.Axes()[1].sector == 1);
    Logged log;
    double deepest = 0.0;
    bool day_quiet = true;
    Run(g, line, env, 24 * 4, log, [&](int h) {
        for (const Axis& x : g.Axes()) deepest = std::max(deepest, x.depth_m);
        // By day every raid is back on its line.
        const bool night = TheatrePartAt(theatre, theatre.start_hour + static_cast<double>(h) + 0.5) == DayPart::Night ||
                           TheatrePartAt(theatre, theatre.start_hour + static_cast<double>(h) - 0.5) == DayPart::Night;
        if (!night) day_quiet = day_quiet && g.Axes()[0].depth_m == 0.0 && g.Axes()[1].depth_m == 0.0;
    });
    log.Print(g);
    SC_EXPECT(log.Count(GroundOrder::RaidStarts, a) >= 3);  // the west, where nobody holds: every night
    SC_EXPECT(log.Count(GroundOrder::RaidStarts, b) == 0);  // the east, held: never at 3 to 1
    SC_EXPECT(log.Count(GroundOrder::RaidEnds) >= 3);
    for (const GroundLogEntry& l : log.all) {
        if (l.what == GroundNote::Order && l.order == GroundOrder::RaidStarts) {
            SC_EXPECT(TheatrePartAt(theatre, theatre.start_hour + l.h + 0.5) == DayPart::Night);
        }
    }
    SC_EXPECT(deepest > 0.0 && deepest <= 15000.0 + 1e-6);
    SC_EXPECT(day_quiet);
    std::printf("    %d raids, the deepest %.1f km\n", log.Count(GroundOrder::RaidStarts), deepest / 1000.0);
}

SC_TEST("ground commanders (F3.4): every faction's war on the nation: bounded, our units withdraw only on an order, no allocation") {
    for (const char* faction : {"warsaw_pact", "nato", "neutral", "rebels"}) {
        std::string e;
        AirWarData d;
        SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", std::string("air_war/faction_") + faction + ".json", d, &e));
        auto gd = std::make_shared<GroundWarData>();
        SC_EXPECT(LoadGroundWarData(kData, faction, *gd, &e));
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
        const std::size_t cap = g.QueueCapacity();
        std::vector<double> last(g.Axes().size(), 0.0);
        double fastest_h = 0.0;
        Logged log;
        Run(g, line, env, 24 * 12, log, [&](int) {
            for (std::size_t k = 0; k < g.Axes().size(); ++k) {
                fastest_h = std::max(fastest_h, g.Axes()[k].depth_m - last[k]);
                last[k] = g.Axes()[k].depth_m;
            }
        });
        SC_EXPECT(g.QueueCapacity() == cap);  // the queue never grew past what Build reserved
        SC_EXPECT(fastest_h <= 70000.0 / 24.0 * 2.0 + 1.0);  // no hour faster than twice the broken enemy's day cap's share
        int decisions = 0;
        for (const GroundCommander& c : g.Commanders()) decisions += c.decisions;
        SC_EXPECT(decisions > 0);
        // Ours never withdraw on their own: only an order or a relief takes them out of the line as a unit.
        for (const GroundLogEntry& l : log.all) {
            if (l.what != GroundNote::Order || l.order != GroundOrder::WithdrawDecided || l.formation < 0) continue;
            const Formation& f = g.Formations()[static_cast<std::size_t>(l.formation)];
            SC_EXPECT(f.unit.side == Side::Enemy || g.Commanders()[static_cast<std::size_t>(l.cmdr)].top);
        }
        std::string axes;
        for (const Axis& x : g.Axes()) {
            char buf[96];
            std::snprintf(buf, sizeof buf, " %s %.1f km", x.id.c_str(), x.depth_m / 1000.0);
            axes += buf;
        }
        std::printf("    %-12s %zu commanders, %d decisions, %zu orders;%s\n", faction, g.Commanders().size(), decisions, log.all.size(), axes.c_str());
    }
}
