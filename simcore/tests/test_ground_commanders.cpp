// Track F, F3.4: the ground war's commanders (simcore/ground_war/commanders.hpp, ground_war.hpp; docs/fronts/03_GROUND_WAR.md
// section 5 and 8). The rules by hand (a unit's decision on its own strength, where an echelon goes, the answer to a request,
// the thresholds a commander's traits move); then hand wars on a straight 100 km line: no withdrawal without an order and
// dispersal under the line when none comes, a relief and the withdrawal as a unit it allows, the second echelon to success,
// NATO's breakpoints, our counterattack to the border against a covering force's screen, the rebels' raids by night where
// we are thin; the order-free draws; and every faction's war on the nation, bounded and with no allocation in the hour.
#include <algorithm>
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
    bool never_negative = true, td_in = false;
    Run(g, line, env, 24 * 6, log, [&](int) {
        const Axis& x = g.Axes()[0];
        td_in = td_in || std::find(x.counter.begin(), x.counter.end(), td) != x.counter.end();
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
    // F7.1: the force goes in together, so the start is logged for whichever of it is first; the tank division is in it.
    const GroundLogEntry* starts = log.First(GroundOrder::CounterattackStarts);
    SC_EXPECT(ordered != nullptr && starts != nullptr && td_in);
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

SC_TEST("ground commanders (F6.1): the war's tally: every order counted as logged, the fighting losses by side and axis, the zone's hour") {
    // fronts_bench --history reads the tally at a war's end (the campaign clears the log hourly): it must agree with the
    // log a test keeps, and the losses it books by axis must be the formations' own.
    std::shared_ptr<GroundWarData> d = CounterData(true);
    LineOfContact line;
    line.Build(StraightFront(), 5000.0);
    TheatreDef theatre;
    GroundEnv env;
    env.theatre = &theatre;
    GroundWar g;
    SC_EXPECT(g.Build(d, line, theatre, nullptr, 7));
    Logged log;
    Run(g, line, env, 24 * 6, log, [&](int) {});
    for (std::size_t o = 0; o < kGroundOrderCount; ++o) SC_EXPECT(static_cast<int>(g.OrderCount(static_cast<GroundOrder>(o))) == log.Count(static_cast<GroundOrder>(o)));
    SC_EXPECT(g.OrderCount(GroundOrder::CounterattackOrdered) >= 1u);
    double by_form[2] = {0.0, 0.0}, by_axis[2] = {0.0, 0.0};
    for (const Formation& f : g.Formations()) {
        SC_EXPECT(f.lost_attack >= 0.0 && f.lost_defence >= 0.0 && f.lost_air == 0.0 && f.fought_h >= 0.0);
        by_form[0] += f.lost_attack;
        by_form[1] += f.lost_defence;
        if (f.lost_attack + f.lost_defence > 0.0) SC_EXPECT(f.fought_h >= 1.0);
    }
    for (const Axis& x : g.Axes()) {
        by_axis[0] += x.lost_attack;
        by_axis[1] += x.lost_defence;
        if (x.depth_m > 0.0) SC_EXPECT(x.lost_attack > 0.0 && x.lost_defence > 0.0);
        if (x.through_zone) SC_EXPECT(x.zone_h >= x.start_h);
    }
    SC_EXPECT_NEAR(by_form[0], by_axis[0], 1e-6 * (1.0 + by_axis[0]));
    SC_EXPECT_NEAR(by_form[1], by_axis[1], 1e-6 * (1.0 + by_axis[1]));
    std::printf("    attacking %.1f, defending %.1f (T-55 equivalents); counterattacks %u, orders logged %zu\n", by_axis[0], by_axis[1],
                g.OrderCount(GroundOrder::CounterattackOrdered), log.all.size());
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

// ------------------------------------------------------------------------- F7.1: the army reserve and the counterattack at the tip

namespace {

// A division of three tank regiments and an artillery regiment (its first echelon two of the three: Bare's doctrines give no
// frontage, so F3.6's half of its manoeuvre regiments, one kept back).
void AddDivision(GroundWarData& d, bool enemy, const std::string& id, const std::string& parent, int tanks_each) {
    command::OrderOfBattle& oob = enemy ? d.enemy.oob : d.ours.oob;
    oob.units.push_back(Unit(id, "division", 0, 1000, parent));
    for (int k = 1; k <= 3; ++k) oob.units.push_back(Unit(id + "_r" + std::to_string(k), "regiment", tanks_each, 2000, id));
    command::OobUnit art = Unit(id + "_art", "regiment", 0, 1000, id);
    art.equipment.push_back({"d30", 54});
    oob.units.push_back(art);
}

double OobStrength(const command::OrderOfBattle& oob, const std::string& id) {
    for (const command::OobUnit& u : oob.units) {
        if (u.id == id) return StrengthOfUnit(Tables(), u);
    }
    return 0.0;
}

// The tip's war: two red divisions and a red brigade on the east axis (60 km deep) against a thin blue division; a blue tank
// brigade in reserve. Its power is under 1.5 times the axis's attackers but over 2.5 times the enemy at the tip (two of the
// five first-echelon regiments the axis brings).
std::shared_ptr<GroundWarData> TipData() {
    std::shared_ptr<GroundWarData> d = Bare();
    d->enemy.oob.units.push_back(Unit("red_army", "army", 0, 500));
    AddDivision(*d, true, "red_d1", "red_army", 100);
    AddDivision(*d, true, "red_d2", "red_army", 100);
    d->enemy.oob.units.push_back(Unit("red_bde", "brigade", 150, 3000, "red_army"));
    AddAxis(*d, "east", "east", 75, 60, {"red_d1", "red_d2", "red_bde"});
    d->ours.oob.units.push_back(Unit("blue_army", "army", 0, 500));
    d->ours.oob.units.push_back(Unit("blue_div", "division", 60, 6000, "blue_army"));
    d->ours.oob.units.push_back(Unit("blue_tb", "brigade", 700, 3000, "blue_army"));
    Deploy(*d, "blue_div", DeployRole::FirstEchelon, {"east"});
    Deploy(*d, "blue_tb", DeployRole::Reserve, {}, 75, 40);
    d->ours.deployments.front().security_zone_km = 15.0;
    d->ours.deployments.front().reserve_depth_km = 30.0;
    return d;
}

struct TipWar {
    LineOfContact line;
    TheatreDef theatre;
    GroundEnv env;
    GroundWar g;
    Logged log;
    explicit TipWar(std::shared_ptr<GroundWarData> d) {
        line.Build(StraightFront(), 5000.0);
        env.theatre = &theatre;
        SC_EXPECT(g.Build(d, line, theatre, nullptr, 7));
    }
};

double PowerOf(const GroundWar& g, int i) {
    const Formation& f = g.Formations()[static_cast<std::size_t>(i)];
    return f.spent ? 0.0 : UnitPower(g.Data().tables, f.unit.strength, f.unit.supply, f.unit.cohesion);
}

}  // namespace

SC_TEST("ground counterattack (F7.1): the enemy at the tip by hand: the first echelons' regiments one breakthrough sector holds") {
    std::shared_ptr<GroundWarData> d = TipData();
    TipWar w(d);
    const GroundWar& g = w.g;
    const int d1 = FormationOf(g, "red_d1"), d2 = FormationOf(g, "red_d2"), bde = FormationOf(g, "red_bde");
    SC_EXPECT(d1 >= 0 && d2 >= 0 && bde >= 0);
    // Each division: four regiments, two of its three tank regiments in its first echelon, the artillery at the support weight.
    const Formation& f1 = g.Formations()[static_cast<std::size_t>(d1)];
    SC_EXPECT(f1.regiment_count == 4);
    int first = 0;
    for (int k = 0; k < f1.regiment_count; ++k) first += g.Regiments()[static_cast<std::size_t>(f1.first_regiment + k)].first ? 1 : 0;
    SC_EXPECT(first == 2);
    const double r = OobStrength(d->enemy.oob, "red_d1_r1"), art = OobStrength(d->enemy.oob, "red_d1_art");
    const double share = (2.0 * r + Tables().levels_support_weight * art) / (3.0 * r + art);
    SC_EXPECT_NEAR(g.TipShare(static_cast<std::size_t>(d1)), share, 1e-9);
    SC_EXPECT_NEAR(g.TipShare(static_cast<std::size_t>(bde)), 1.0, 1e-12);  // no regiments: all first echelon
    // The tip holds one formation's first echelon (two regiments) of the five the axis brings (2 + 2 + the brigade).
    SC_EXPECT_NEAR(g.TipFractionOf(0), 2.0 / 5.0, 1e-12);
    const double tip = (PowerOf(g, d1) * share + PowerOf(g, d2) * share + PowerOf(g, bde)) * 2.0 / 5.0;
    SC_EXPECT_NEAR(g.TipPowerTruth(0), tip, 1e-9 * tip);
    SC_EXPECT_NEAR(g.TipSeen(0), tip, 1e-9 * tip);  // the picture on the truth (Bare)
    // So our reserve, under 1.5 times the axis, is over 2.5 times its tip.
    const double reserve = PowerOf(g, FormationOf(g, "blue_tb")), axis = PowerOf(g, d1) + PowerOf(g, d2) + PowerOf(g, bde);
    SC_EXPECT(reserve < 1.5 * axis && reserve > 2.5 * tip);
    std::printf("    a division's share at the tip %.3f, the tip's fraction %.2f: tip %.0f of the axis's %.0f; our reserve %.0f (%.2f : 1 at the tip, %.2f over the axis)\n",
                share, g.TipFractionOf(0), tip, axis, reserve, reserve / tip, reserve / axis);
}

SC_TEST("ground counterattack (F7.1): the army reserve held back out of the line until committed; once the momentum is spent, at the tip") {
    // Our doctrine's lines (2/3 and 1/2), so the thin division holds without asking for the reserve as its relief.
    std::shared_ptr<GroundWarData> d = TipData();
    d->ours.doctrine.effectiveness.degraded_below = 2.0 / 3.0;
    d->ours.doctrine.effectiveness.ineffective_below = 0.5;
    TipWar w(d);
    GroundWar& g = w.g;
    const int tb = FormationOf(g, "blue_tb");
    const Formation& r = g.Formations()[static_cast<std::size_t>(tb)];
    // Held back: at its place's station along the line, the deployment's 30 km behind it (from 40), out of the line.
    SC_EXPECT_NEAR(r.pos.x, 75000.0, 1.0);
    SC_EXPECT_NEAR(r.pos.y, 30000.0, 1.0);
    const Axis& x = g.Axes()[0];
    bool held = true, spent_at_order = false, gated = false;
    double whole_at_order = 0.0, tip_at_order = 0.0, ours_at_order = 0.0;
    int order_h = -1;
    Run(g, w.line, w.env, 24 * 6, w.log, [&](int h) {
        const bool ordered = w.log.First(GroundOrder::CounterattackOrdered) != nullptr;
        if (!ordered && w.log.First(GroundOrder::ReliefOrdered, tb) == nullptr) {
            // Out of the line until committed: free, in reserve, on no axis.
            held = held && r.free && r.task == Task::None && r.unit.state == UnitState::Reserve;
            for (int fi : x.defenders) held = held && fi != tb;
            for (int fi : x.depth_defenders) held = held && fi != tb;
            // Through the main zone, its tip under our reserve's ratio, and still moving: no counterattack yet.
            if (x.through_zone && !g.MomentumSpentSeen(0) && PowerOf(g, tb) / g.TipSeen(0) >= 2.5) gated = true;
        } else if (order_h < 0) {
            order_h = h;
            spent_at_order = g.MomentumSpentSeen(0) || x.status == AxisStatus::Counterattacked;
            for (int fi = 0; fi < static_cast<int>(g.Formations().size()); ++fi) {
                const Formation& f = g.Formations()[static_cast<std::size_t>(fi)];
                const bool depth = std::find(x.depth_defenders.begin(), x.depth_defenders.end(), fi) != x.depth_defenders.end();
                if (f.unit.side == Side::Ours && f.role != DeployRole::Support && (f.free || depth || f.task == Task::Counter)) ours_at_order += PowerOf(g, fi);
            }
            tip_at_order = g.TipPowerTruth(0);
            for (int fi : x.first) whole_at_order += PowerOf(g, fi);
        }
    });
    w.log.Print(g);
    const GroundLogEntry* ordered = w.log.First(GroundOrder::CounterattackOrdered);
    SC_EXPECT(held);
    SC_EXPECT(ordered != nullptr && order_h >= 0);
    if (ordered == nullptr) return;
    const double threshold = CounterattackRatioOf(Tables(), g.Commanders()[static_cast<std::size_t>(ordered->cmdr)].traits);
    SC_EXPECT(spent_at_order);
    SC_EXPECT(ordered->value >= threshold - 1e-9);
    // His ratio is his free force over the tip (the hour's march costs it nothing in the fight), never over the whole axis, where
    // F3.4's rule would not have gone in.
    SC_EXPECT_NEAR(ordered->value, ours_at_order / tip_at_order, 0.02 * ordered->value);
    SC_EXPECT(ours_at_order / whole_at_order < threshold);
    SC_EXPECT(w.log.First(GroundOrder::CounterattackStarts, tb) != nullptr);
    std::printf("    ordered at %.0f h on %.2f : 1 at the tip (%.2f over the axis, his line %.2f); held back and gated before: %s\n", ordered->h,
                ordered->value, ours_at_order / whole_at_order, threshold, gated ? "yes" : "no");
}

SC_TEST("ground counterattack (F7.1): no counterattack the reserve cannot bring in time, nor by F3.4's rule against the whole axis") {
    // The reserve's march exceeds the reach: it stays free and out of the line.
    std::shared_ptr<GroundWarData> slow = TipData();
    slow->tables.counter_reach_h = 0.5;
    TipWar a(slow);
    Run(a.g, a.line, a.env, 24 * 6, a.log, [](int) {});
    const Formation& r = a.g.Formations()[static_cast<std::size_t>(FormationOf(a.g, "blue_tb"))];
    SC_EXPECT(a.log.First(GroundOrder::CounterattackOrdered) == nullptr);
    SC_EXPECT(r.free && r.task == Task::None);
    // Without the data's "counterattack" block: F3.4's rule, the reserve against the axis's whole first echelon.
    std::shared_ptr<GroundWarData> old = TipData();
    old->tables.counter_at_tip = false;
    TipWar b(old);
    Run(b.g, b.line, b.env, 24 * 6, b.log, [](int) {});
    SC_EXPECT(b.log.First(GroundOrder::CounterattackOrdered) == nullptr);
    std::printf("    in reach 0.5 h: %s; by the whole axis: %s\n", a.log.First(GroundOrder::CounterattackOrdered) ? "ordered" : "none",
                b.log.First(GroundOrder::CounterattackOrdered) ? "ordered" : "none");
}

SC_TEST("ground counterattack (F7.1): an incursion halted in our security zone is counterattacked back to the border, never past it") {
    // The enemy's goal 10 km in, inside the 15 km security zone: it halts there holding our ground.
    std::shared_ptr<GroundWarData> d = TipData();
    d->enemy.plan.axes.front().route.back().pos = Km(75, 10);
    d->enemy.plan.objectives.front().place.pos = Km(75, 10);
    TipWar w(d);
    GroundWar& g = w.g;
    const Axis& x = g.Axes()[0];
    bool never_negative = true, through = false;
    double halted_at = -1.0;
    Run(g, w.line, w.env, 24 * 6, w.log, [&](int) {
        never_negative = never_negative && x.depth_m >= 0.0;
        for (std::size_t i = 0; i < w.line.StationCount(); ++i) never_negative = never_negative && w.line.StationOffset(i) >= 0.0;
        through = through || x.through_zone;
        if (halted_at < 0.0 && x.status == AxisStatus::Halted) halted_at = x.depth_m;
    });
    w.log.Print(g);
    const GroundLogEntry* ordered = w.log.First(GroundOrder::CounterattackOrdered);
    SC_EXPECT(!through);
    SC_EXPECT(ordered != nullptr && w.log.First(GroundOrder::CounterattackEnds) != nullptr);
    SC_EXPECT(never_negative);
    SC_EXPECT(x.depth_m < halted_at);
    // F3.4's rule counterattacks only a penetration of the main zone.
    std::shared_ptr<GroundWarData> old = TipData();
    old->enemy.plan.axes.front().route.back().pos = Km(75, 10);
    old->enemy.plan.objectives.front().place.pos = Km(75, 10);
    old->tables.counter_at_tip = false;
    TipWar b(old);
    Run(b.g, b.line, b.env, 24 * 6, b.log, [](int) {});
    SC_EXPECT(b.log.First(GroundOrder::CounterattackOrdered) == nullptr);
    std::printf("    halted %.1f km in, counterattacked at %.0f h, %.1f km in at the end\n", halted_at / 1000.0, ordered != nullptr ? ordered->h : -1.0,
                x.depth_m / 1000.0);
}

SC_TEST("ground counterattack (F7.1): our army's reserve on the nation: about a ninth of the army, held back by its deployment, out of the line") {
    for (const char* faction : {"warsaw_pact", "nato", "neutral"}) {
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
        SC_EXPECT(g.Data().tables.counter_at_tip);
        const DeploymentDef* dep = gd->ours.For(faction);
        SC_EXPECT(dep != nullptr && dep->reserve_depth_km > dep->security_zone_km);
        if (dep == nullptr) return;
        double all = 0.0, reserve = 0.0;
        int n = 0;
        for (std::size_t i = 0; i < g.Formations().size(); ++i) {
            const Formation& f = g.Formations()[i];
            if (f.unit.side != Side::Ours || f.front) continue;  // F8.1: the national command's reserve is not the army's
            all += PowerOf(g, static_cast<int>(i));
            if (f.role != DeployRole::Reserve) continue;
            ++n;
            reserve += PowerOf(g, static_cast<int>(i));
            const FrontPlace pl = line.Place(f.pos);
            SC_EXPECT_NEAR(pl.side * pl.distance_m, dep->reserve_depth_km * 1000.0, 500.0);
            SC_EXPECT(f.free && f.unit.state == UnitState::Reserve && !f.anchor.empty());
            for (const Axis& x : g.Axes()) {
                for (int fi : x.defenders) SC_EXPECT(fi != static_cast<int>(i));
            }
        }
        // About a ninth (fm_100_2_1_1984 p. 2-11): within half and twice the doctrine's share (against the neutral the army
        // deploys only part of its formations, so the same brigade is a larger share of them).
        const double share = all > 0.0 ? reserve / all : 0.0;
        const double norm = gd->ours.doctrine.reserve_share;
        SC_EXPECT(n == 1 && norm > 0.1 && share >= 0.5 * norm && share <= 2.0 * norm);
        std::printf("    %-12s the reserve %.1f %% of the army (doctrine %.1f %%), %.0f km behind the line\n", faction, 100.0 * share, 100.0 * norm,
                    dep->reserve_depth_km);
    }
}

// ------------------------------------------------------------------------------------------------- the front reserve (F8.1)

namespace {

// F8.1's war: the tip's war with our army's reserve too weak to counterattack alone (100 tanks against the tip), and the
// national command's reserve, a division of three 250-tank regiments and an artillery regiment under the national
// command over our army, at home 70 km behind the east axis. `block` false: the same order of battle without the
// "front_reserve" block.
std::shared_ptr<GroundWarData> FrontData(bool block, double reach_h = 24.0) {
    std::shared_ptr<GroundWarData> d = TipData();
    d->ours.doctrine.effectiveness.degraded_below = 2.0 / 3.0;
    d->ours.doctrine.effectiveness.ineffective_below = 0.5;
    for (command::OobUnit& u : d->ours.oob.units) {
        if (u.id == "blue_tb") u.equipment = {{"t55", 100}};
        if (u.id == "blue_army") u.parent = "blue_land";
    }
    d->ours.oob.units.push_back(Unit("blue_land", "service", 0, 0));
    AddDivision(*d, false, "blue_td", "blue_land", 250);
    if (block) {
        FrontReserveDef& r = d->ours.front_reserve;
        r.present = true;
        r.unit = "blue_td";
        r.command = "blue_land";
        r.place = At("blue_td_home", 75, 70);
        r.planning_h = ValueRange{6.0, 12.0};
        r.reach_h = reach_h;
    }
    return d;
}

const GroundLogEntry* FirstOrderBy(const Logged& log, GroundOrder o, int cmdr) {
    for (const GroundLogEntry& e : log.all) {
        if (e.what == GroundNote::Order && e.order == o && e.cmdr == cmdr) return &e;
    }
    return nullptr;
}

}  // namespace

SC_TEST("front reserve (F8.1): held out of the line until the front's commander commits it, on the tip rule, within his reach") {
    std::shared_ptr<GroundWarData> d = FrontData(true);
    TipWar w(d);
    GroundWar& g = w.g;
    const int td = FormationOf(g, "blue_td"), tb = FormationOf(g, "blue_tb");
    SC_EXPECT(td >= 0 && tb >= 0 && g.FrontReserve() == td && g.FrontCommander() >= 0);
    if (td < 0 || tb < 0 || g.FrontCommander() < 0) return;
    const Formation& r = g.Formations()[static_cast<std::size_t>(td)];
    const GroundCommander& front = g.Commanders()[static_cast<std::size_t>(g.FrontCommander())];
    // Its own: at its home, free, out of every sector; its commander answers to the front's, who is not the army's top.
    SC_EXPECT(r.front && r.free && r.first_sector < 0 && r.unit.state == UnitState::Reserve);
    SC_EXPECT_NEAR(r.pos.x, 75000.0, 1e-6);
    SC_EXPECT_NEAR(r.pos.y, 70000.0, 1e-6);
    SC_EXPECT(r.anchor == "blue_td_home" && r.regiment_count == 4);
    SC_EXPECT(front.front && !front.top && front.unit == "blue_land" && g.TopOf(Side::Ours) != g.FrontCommander());
    SC_EXPECT(r.cmdr >= 0 && g.Commanders()[static_cast<std::size_t>(r.cmdr)].senior == g.FrontCommander());
    const Axis& x = g.Axes()[0];
    bool held = true, army_short = true;
    double ours_at_order = 0.0, tip_at_order = 0.0;
    int order_h = -1, together = -1;
    Run(g, w.line, w.env, 24 * 6, w.log, [&](int h) {
        // The whole force goes in together: when the counterattack starts, the division and the brigade are both in it.
        if (together < 0 && w.log.First(GroundOrder::CounterattackStarts) != nullptr) {
            const bool d_in = std::find(x.counter.begin(), x.counter.end(), td) != x.counter.end();
            const bool b_in = std::find(x.counter.begin(), x.counter.end(), tb) != x.counter.end();
            together = d_in && b_in ? 1 : 0;
        }
        if (w.log.First(GroundOrder::CounterattackOrdered) == nullptr) {
            // Out of the line until committed; never sent by the army, as a counterattack or a relief.
            held = held && r.free && r.task == Task::None && r.unit.state == UnitState::Reserve;
            for (int fi : x.defenders) held = held && fi != td;
            for (int fi : x.depth_defenders) held = held && fi != td;
        } else if (order_h < 0) {
            order_h = h;
            for (int fi = 0; fi < static_cast<int>(g.Formations().size()); ++fi) {
                const Formation& f = g.Formations()[static_cast<std::size_t>(fi)];
                const bool depth = std::find(x.depth_defenders.begin(), x.depth_defenders.end(), fi) != x.depth_defenders.end();
                if (f.unit.side == Side::Ours && f.role != DeployRole::Support && (f.free || depth || f.task == Task::Counter)) ours_at_order += PowerOf(g, fi);
            }
            tip_at_order = g.TipPowerTruth(0);
            // The army's own free force at the tip, under his ratio: he could not have gone in alone.
            const GroundCommander& army = g.Commanders()[static_cast<std::size_t>(g.TopOf(Side::Ours))];
            army_short = PowerOf(g, tb) / tip_at_order < CounterattackRatioOf(Tables(), army.traits);
        }
    });
    w.log.Print(g);
    const GroundLogEntry* ordered = w.log.First(GroundOrder::CounterattackOrdered);
    SC_EXPECT(held && army_short);
    SC_EXPECT(ordered != nullptr && order_h >= 0);
    if (ordered == nullptr) return;
    // The front's commander's order, on his ratio, once the momentum was spent: his force in reach (his reserve and the army's
    // brigade) over the enemy at the tip, by hand.
    SC_EXPECT(ordered->cmdr == g.FrontCommander());
    SC_EXPECT(ordered->value >= CounterattackRatioOf(Tables(), front.traits) - 1e-9);
    SC_EXPECT_NEAR(ordered->value, ours_at_order / tip_at_order, 0.02 * ordered->value);
    SC_EXPECT(g.FrontOrders() >= 1 && g.FrontOrderedRatioSum() >= ordered->value - 1e-9);
    // It goes in after his planning (6-12 h) and its march, within his reach of the order.
    const GroundLogEntry* starts = w.log.First(GroundOrder::CounterattackStarts);
    SC_EXPECT(starts != nullptr && together == 1);
    if (starts != nullptr) {
        SC_EXPECT(starts->h - ordered->h >= 6.0 - 1.0);
        SC_EXPECT(starts->h - ordered->h <= 24.0 + 1.0);
    }
    std::printf("    the front ordered at %.0f h on %.2f : 1 at the tip (his line %.2f; the army's brigade alone %.2f), in at %.0f h\n", ordered->h, ordered->value,
                CounterattackRatioOf(Tables(), front.traits), PowerOf(g, tb) / tip_at_order, starts != nullptr ? starts->h : -1.0);
}

SC_TEST("front reserve (F8.1): no counterstrike it cannot reach in time; the army never sends it") {
    // His reach under his least planning: never in reach, so never weighed and never sent; the army's brigade is too weak alone.
    std::shared_ptr<GroundWarData> d = FrontData(true, 1.0);
    TipWar w(d);
    Run(w.g, w.line, w.env, 24 * 6, w.log, [](int) {});
    const int td = FormationOf(w.g, "blue_td");
    SC_EXPECT(td >= 0 && w.g.FrontCommander() >= 0);
    if (td < 0 || w.g.FrontCommander() < 0) return;
    const Formation& r = w.g.Formations()[static_cast<std::size_t>(td)];
    SC_EXPECT(w.log.First(GroundOrder::CounterattackOrdered) == nullptr && w.log.First(GroundOrder::ReliefOrdered, td) == nullptr);
    SC_EXPECT(r.free && r.task == Task::None && r.unit.state == UnitState::Reserve && w.g.FrontOrders() == 0);
    SC_EXPECT(w.g.FrontBestRatio() == 0.0);
    SC_EXPECT(FirstOrderBy(w.log, GroundOrder::CounterattackOrdered, w.g.FrontCommander()) == nullptr);
    // The front's commander still watched: he decided while a penetration was a target and his reserve free.
    const int decided = w.g.Commanders()[static_cast<std::size_t>(w.g.FrontCommander())].decisions;
    SC_EXPECT(decided > 0);
    std::printf("    reach 1 h: no order; the front's commander decided %d times\n", decided);
}

SC_TEST("front reserve (F8.1): without its block (or F7.1's) the division is never built, and the war is the war without it") {
    // The same order of battle, no block: no front reserve, no front commander, and every order as in the war whose order of
    // battle never had the division (TipData with the same weak brigade).
    std::shared_ptr<GroundWarData> none = FrontData(false);
    std::shared_ptr<GroundWarData> bare = TipData();
    bare->ours.doctrine.effectiveness = none->ours.doctrine.effectiveness;
    for (command::OobUnit& u : bare->ours.oob.units) {
        if (u.id == "blue_tb") u.equipment = {{"t55", 100}};
    }
    TipWar a(none), b(bare);
    SC_EXPECT(FormationOf(a.g, "blue_td") < 0 && a.g.FrontReserve() < 0 && a.g.FrontCommander() < 0);
    SC_EXPECT(a.g.Formations().size() == b.g.Formations().size() && a.g.Commanders().size() == b.g.Commanders().size());
    Run(a.g, a.line, a.env, 24 * 6, a.log, [](int) {});
    Run(b.g, b.line, b.env, 24 * 6, b.log, [](int) {});
    bool same = a.log.all.size() == b.log.all.size();
    for (std::size_t k = 0; same && k < a.log.all.size(); ++k) {
        const GroundLogEntry& p = a.log.all[k];
        const GroundLogEntry& q = b.log.all[k];
        same = p.h == q.h && p.what == q.what && p.order == q.order && p.formation == q.formation && p.cmdr == q.cmdr && p.value == q.value;
    }
    SC_EXPECT(same);
    SC_EXPECT(a.g.Axes()[0].depth_m == b.g.Axes()[0].depth_m);
    // With the block but without F7.1's counterattack at the tip: none either.
    std::shared_ptr<GroundWarData> old = FrontData(true);
    old->tables.counter_at_tip = false;
    TipWar c(old);
    SC_EXPECT(FormationOf(c.g, "blue_td") < 0 && c.g.FrontCommander() < 0);
    std::printf("    %zu log entries the same; the line %.1f km in either way\n", a.log.all.size(), a.g.Axes()[0].depth_m / 1000.0);
}

SC_TEST("front reserve (F8.1): the 11th Tank Division on the nation: the national command's, at its home, out of every war's line") {
    for (const char* faction : {"warsaw_pact", "nato", "neutral", "rebels"}) {
        std::string e;
        AirWarData d;
        SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", std::string("air_war/faction_") + faction + ".json", d, &e));
        auto gd = std::make_shared<GroundWarData>();
        SC_EXPECT(LoadGroundWarData(kData, faction, *gd, &e));
        const FrontDef* front = d.theatre.FrontFor(d.faction.front);
        SC_EXPECT(front != nullptr && d.country != nullptr && gd->ours.front_reserve.present);
        if (front == nullptr || d.country == nullptr) return;
        LineOfContact line;
        line.Build(*front, d.tuning.Get("air_front.station_m"));
        GroundWar g;
        SC_EXPECT(g.Build(gd, line, d.theatre, d.country.get(), 21, &e));
        const int td = g.FrontReserve();
        SC_EXPECT(td >= 0 && td == FormationOf(g, "td_11") && g.FrontCommander() >= 0);
        if (td < 0 || g.FrontCommander() < 0) return;
        const Formation& f = g.Formations()[static_cast<std::size_t>(td)];
        SC_EXPECT(f.front && f.free && f.anchor == "cegled_junction" && f.first_sector < 0);
        SC_EXPECT(g.Commanders()[static_cast<std::size_t>(g.FrontCommander())].unit == "ground_forces");
        double army = 0.0;
        for (std::size_t i = 0; i < g.Formations().size(); ++i) {
            const Formation& o = g.Formations()[i];
            if (o.unit.side == Side::Ours && !o.front) army += PowerOf(g, static_cast<int>(i));
        }
        for (const Axis& x : g.Axes()) {
            for (int fi : x.defenders) SC_EXPECT(fi != td);
            for (int fi : x.depth_defenders) SC_EXPECT(fi != td);
        }
        const FrontPlace pl = line.Place(f.pos);
        std::printf("    %-12s the 11th Tank Division %.0f %% of the army's power, its home %.0f km behind the line\n", faction,
                    army > 0.0 ? 100.0 * PowerOf(g, td) / army : 0.0, pl.side * pl.distance_m / 1000.0);
        // Without the block: one formation fewer, no front commander.
        auto off = std::make_shared<GroundWarData>(*gd);
        off->ours.front_reserve.present = false;
        GroundWar h;
        SC_EXPECT(h.Build(off, line, d.theatre, d.country.get(), 21, &e));
        SC_EXPECT(h.Formations().size() + 1 == g.Formations().size() && h.FrontCommander() < 0 && FormationOf(h, "td_11") < 0);
    }
}

// ------------------------------------------------------------------------------------------ F8.2: command or artillery hit

namespace {

// F8.2's hand war: the enemy's offensive a month out (no fighting), our two divisions on the line, the
// doctrine's lines (2/3, 1/2, 3/4 hit). blue_a has its own command post; blue_b its regiments (two of tanks, one of
// artillery, each with a troops object); the army's command post is no formation's; a reserve division to relieve.
// The command post restored in 8 h (a test value inside the division's reaction time; the doctrine's is 1-3 h).
enum : std::size_t { kACp = 0, kBArt = 1, kBR1 = 2, kArmyCp = 3 };

struct HitWar {
    LineOfContact line;
    TheatreDef theatre;
    std::vector<ObjectState> truth;
    GroundEnv env;
    GroundWar g;
    Logged log;
    int a = -1, b = -1;
    explicit HitWar(bool block) {
        std::shared_ptr<GroundWarData> d = Bare();
        EffectivenessDef e;
        e.kind = EffectivenessKind::Lines;
        e.degraded_below = 0.667;
        e.ineffective_below = 0.5;
        e.hq_hit_ineffective_below = 0.75;
        d->ours.doctrine.effectiveness = e;
        d->enemy.doctrine.effectiveness = e;
        d->ours.doctrine.restore_control_h = ValueRange{8.0, 8.0};
        d->tables.command_hit.on = block;
        d->enemy.plan.start_after_h = 24.0 * 30.0;
        d->enemy.oob.units.push_back(Unit("red_div", "division", 700, 10000));
        AddAxis(*d, "east", "east", 75, 200, {"red_div"});
        command::OrderOfBattle& o = d->ours.oob;
        o.units.push_back(Unit("blue_army", "army", 0, 500));
        o.units.back().objects = {"army_cp"};
        o.units.push_back(Unit("blue_a", "division", 160, 10000, "blue_army"));
        o.units.back().objects = {"a_cp"};
        o.units.push_back(Unit("blue_b", "division", 0, 1000, "blue_army"));
        o.units.push_back(Unit("b_r1", "regiment", 60, 3000, "blue_b"));
        o.units.back().objects = {"b_r1_area"};
        o.units.push_back(Unit("b_r2", "regiment", 60, 3000, "blue_b"));
        o.units.push_back(Unit("b_art", "regiment", 0, 2000, "blue_b"));
        o.units.back().objects = {"b_art_area"};
        o.units.push_back(Unit("blue_res", "division", 160, 10000, "blue_army"));
        Deploy(*d, "blue_a", DeployRole::FirstEchelon, {"east"});
        Deploy(*d, "blue_b", DeployRole::FirstEchelon, {"west"});
        Deploy(*d, "blue_res", DeployRole::Reserve, {}, 75, 40);
        const auto add = [this](const std::string& id, ObjectKind kind, double e_km, double n_km) {
            TheatreObject t;
            t.id = id;
            t.name = id;
            t.kind = kind;
            t.pos = Km(e_km, n_km);
            theatre.objects.push_back(t);
            ObjectState s;
            s.id = id;
            s.pos = t.pos;
            truth.push_back(s);
        };
        add("a_cp", ObjectKind::CommandPost, 75, 20);
        add("b_art_area", ObjectKind::Troops, 25, 20);
        add("b_r1_area", ObjectKind::Troops, 20, 10);
        add("army_cp", ObjectKind::CommandPost, 50, 60);
        line.Build(StraightFront(), 5000.0);
        env.theatre = &theatre;
        env.truth = &truth;
        std::string err;
        SC_EXPECT(g.Build(d, line, theatre, nullptr, 7, &err));
        if (!err.empty()) std::printf("    %s\n", err.c_str());
        a = FormationOf(g, "blue_a");
        b = FormationOf(g, "blue_b");
    }
    const Formation& F(int i) const { return g.Formations()[static_cast<std::size_t>(i)]; }
    // Steps to hour `to`, the log kept.
    void To(int to) {
        for (int h = static_cast<int>(g.Now()) + 1; h <= to; ++h) {
            g.Step(static_cast<double>(h), env, line);
            log.Take(g);
        }
    }
    const GroundLogEntry* Hit(int formation, CommandHitKind kind) const {
        for (const GroundLogEntry& e : log.all) {
            if (e.what == GroundNote::CommandHit && e.formation == formation && e.other == static_cast<int>(kind)) return &e;
        }
        return nullptr;
    }
    int Hits() const {
        int n = 0;
        for (const GroundLogEntry& e : log.all) n += e.what == GroundNote::CommandHit ? 1 : 0;
        return n;
    }
};

}  // namespace

SC_TEST("command hit (F8.2): its command post hit, a division at 66 % asks to withdraw on the 3/4 line and is relieved; one not hit does not") {
    SC_EXPECT(Tables().command_hit.on);  // the data's block
    HitWar w(true);
    SC_EXPECT(w.a >= 0 && w.b >= 0);
    if (w.a < 0 || w.b < 0) return;
    // Only blue_a's own command post is a formation's: the army's is not.
    SC_EXPECT(w.g.CommandTies().size() == 1 && w.g.CommandTies()[0].object == static_cast<int>(kACp) && w.g.CommandTies()[0].formation == w.a);
    w.To(1);
    w.g.TestLoss(static_cast<std::size_t>(w.a), 0.34);
    w.g.TestLoss(static_cast<std::size_t>(w.b), 0.34);
    SC_EXPECT_NEAR(w.F(w.a).unit.Fraction(), 0.66, 1e-9);
    SC_EXPECT(w.g.AskLine(static_cast<std::size_t>(w.a)) < 0.6 && w.g.AskLine(static_cast<std::size_t>(w.b)) < 0.6);
    // blue_a's command post struck; blue_b's tank regiment struck (its commander looks at his unit: under 2/3, he reports).
    w.truth[kACp].function = 0.4;
    const int r1 = w.g.TieOf(kBR1);
    SC_EXPECT(r1 >= 0 && !w.g.Ties()[static_cast<std::size_t>(r1)].artillery);
    if (r1 >= 0) w.g.AirStrike(static_cast<std::size_t>(r1), 0.05, w.g.Now());
    w.To(2);
    SC_EXPECT(w.g.HitKnown(static_cast<std::size_t>(w.a)) && !w.g.HitKnown(static_cast<std::size_t>(w.b)));
    SC_EXPECT_NEAR(w.g.AskLine(static_cast<std::size_t>(w.a)), 0.75, 1e-12);
    w.To(48);
    w.log.Print(w.g);
    const GroundLogEntry* hit = w.Hit(w.a, CommandHitKind::CommandPost);
    const GroundLogEntry* asked = w.log.First(GroundOrder::WithdrawAsked, w.a);
    SC_EXPECT(hit != nullptr && asked != nullptr);
    if (hit == nullptr || asked == nullptr) return;
    SC_EXPECT(hit->h == 1.0 && hit->value == 8.0);
    SC_EXPECT(asked->h > hit->h && asked->h <= hit->h + 8.0 && asked->value < 0.75 && asked->value > 0.5);
    SC_EXPECT(w.log.Count(GroundOrder::ReliefOrdered) == 1 && w.F(w.a).withdrawals == 1);
    // blue_b: decided (reported, under 2/3) on its own line; never asked.
    SC_EXPECT(w.log.Count(GroundOrder::Reported, w.b) == 1 && w.log.Count(GroundOrder::WithdrawAsked, w.b) == 0 && w.F(w.b).withdrawals == 0);
    SC_EXPECT(w.Hits() == 1 && w.F(w.a).hits[0] == 1);
}

SC_TEST("command hit (F8.2): known only as its staff learns it: the command post at the hour's start, the artillery by its report") {
    HitWar w(true);
    if (w.a < 0 || w.b < 0) return;
    w.To(1);
    // A loss is not a hit; nor is a strike on the army's command post, nor on a tank regiment.
    w.g.TestLoss(static_cast<std::size_t>(w.a), 0.30);
    w.truth[kArmyCp].function = 0.2;
    const int r1 = w.g.TieOf(kBR1), art = w.g.TieOf(kBArt);
    SC_EXPECT(r1 >= 0 && art >= 0);
    if (r1 < 0 || art < 0) return;
    w.g.AirStrike(static_cast<std::size_t>(r1), 0.5, w.g.Now());
    w.To(4);
    SC_EXPECT(!w.g.HitKnown(static_cast<std::size_t>(w.a)) && !w.g.HitKnown(static_cast<std::size_t>(w.b)) && w.Hits() == 0);
    // The command post struck between the hours: its staff has it at the next hour's start, not before.
    w.truth[kACp].function = 0.5;
    SC_EXPECT(!w.g.HitKnown(static_cast<std::size_t>(w.a)));
    w.To(5);
    SC_EXPECT(w.g.HitKnown(static_cast<std::size_t>(w.a)));
    // The artillery struck from the air: blue_b's staff has it by the artillery's report, own_delay_h later.
    SC_EXPECT(w.g.Ties()[static_cast<std::size_t>(art)].artillery && w.g.Ties()[static_cast<std::size_t>(art)].regiment >= 0);
    const double at = w.g.Now();
    const double delay = Tables().picture.own_delay_h;
    w.g.AirStrike(static_cast<std::size_t>(art), 0.5, at);
    SC_EXPECT(!w.g.HitKnown(static_cast<std::size_t>(w.b)));
    w.To(static_cast<int>(at + delay));
    SC_EXPECT(!w.g.HitKnown(static_cast<std::size_t>(w.b)));
    w.To(static_cast<int>(at + delay) + 1);
    SC_EXPECT(w.g.HitKnown(static_cast<std::size_t>(w.b)));
    const GroundLogEntry* hit = w.Hit(w.b, CommandHitKind::ArtilleryStruck);
    SC_EXPECT(hit != nullptr && std::fabs(hit->h - (at + delay)) < 1e-9);
    // Seen once: the command post's share standing low again is no new hit; risen and struck again is.
    const int before = w.Hits();
    const int now = static_cast<int>(w.g.Now());
    w.To(now + 2);
    SC_EXPECT(w.Hits() == before);
    w.truth[kACp].function = 0.9;
    w.To(now + 3);
    w.truth[kACp].function = 0.3;
    w.To(now + 4);
    SC_EXPECT(w.Hits() == before + 1 && w.F(w.a).hits[0] == 2);
}

SC_TEST("command hit (F8.2): it wears off as command is restored: the command post's in restore_control_h, the artillery's in 12-24 h") {
    HitWar w(true);
    if (w.a < 0 || w.b < 0) return;
    const double base = w.g.AskLine(static_cast<std::size_t>(w.a));
    w.To(1);
    w.truth[kACp].function = 0.5;
    w.To(2);
    SC_EXPECT(w.g.HitKnown(static_cast<std::size_t>(w.a)) && w.F(w.a).hit_until_h == 9.0);
    w.To(8);
    SC_EXPECT(w.g.HitKnown(static_cast<std::size_t>(w.a)));
    w.To(10);
    SC_EXPECT(!w.g.HitKnown(static_cast<std::size_t>(w.a)) && w.g.AskLine(static_cast<std::size_t>(w.a)) == base);
    // The artillery's: drawn in the data's 12-24 h; a second hit inside it holds the line to the later end.
    const int art = w.g.TieOf(kBArt);
    if (art < 0) return;
    w.g.AirStrike(static_cast<std::size_t>(art), 0.5, w.g.Now());
    w.To(14);
    const GroundLogEntry* hit = w.Hit(w.b, CommandHitKind::ArtilleryStruck);
    SC_EXPECT(hit != nullptr);
    if (hit == nullptr) return;
    const ValueRange r = Tables().command_hit.artillery_restore_h;
    SC_EXPECT(hit->value >= r.lo - 1e-9 && hit->value <= r.hi + 1e-9);
    const double end = w.F(w.b).hit_until_h;
    SC_EXPECT_NEAR(end, hit->h + hit->value, 1e-9);
    w.To(static_cast<int>(std::ceil(end)) - 1);
    SC_EXPECT(w.g.HitKnown(static_cast<std::size_t>(w.b)));
    w.To(static_cast<int>(std::ceil(end)) + 1);
    SC_EXPECT(!w.g.HitKnown(static_cast<std::size_t>(w.b)));
    std::printf("    the command post's hit 8 h (a test value), the artillery's %.1f h\n", hit->value);
}

SC_TEST("command hit (F8.2): without its block no hit is learned, the line is the ask line, and the war is the war without it") {
    // The same strikes without the block: no command post watched, no hit, no ask at 70 %.
    HitWar off(false);
    if (off.a < 0 || off.b < 0) return;
    SC_EXPECT(off.g.CommandTies().empty());
    off.To(1);
    off.g.TestLoss(static_cast<std::size_t>(off.a), 0.30);
    off.truth[kACp].function = 0.4;
    const int art = off.g.TieOf(kBArt);
    if (art >= 0) off.g.AirStrike(static_cast<std::size_t>(art), 0.5, off.g.Now());
    off.To(48);
    SC_EXPECT(off.Hits() == 0 && !off.g.HitKnown(static_cast<std::size_t>(off.a)) && !off.g.HitKnown(static_cast<std::size_t>(off.b)));
    SC_EXPECT(off.log.Count(GroundOrder::WithdrawAsked) == 0 && off.g.AskLine(static_cast<std::size_t>(off.a)) < 0.6);
    // With the block and nothing hit, every entry and strength as without it.
    HitWar p(true), q(false);
    if (p.a < 0 || q.a < 0) return;
    for (HitWar* w : {&p, &q}) {
        w->To(1);
        w->g.TestLoss(static_cast<std::size_t>(w->a), 0.40);
        const int r1 = w->g.TieOf(kBR1);
        if (r1 >= 0) w->g.AirStrike(static_cast<std::size_t>(r1), 0.5, w->g.Now());
        w->To(24 * 4);
    }
    bool same = p.log.all.size() == q.log.all.size() && !p.log.all.empty();
    for (std::size_t k = 0; same && k < p.log.all.size(); ++k) {
        const GroundLogEntry& x = p.log.all[k];
        const GroundLogEntry& y = q.log.all[k];
        same = x.h == y.h && x.what == y.what && x.order == y.order && x.formation == y.formation && x.cmdr == y.cmdr && x.value == y.value;
    }
    SC_EXPECT(same);
    for (std::size_t i = 0; i < p.g.Formations().size(); ++i) SC_EXPECT(p.g.Formations()[i].unit.strength == q.g.Formations()[i].unit.strength);
    std::printf("    %zu log entries the same with the block and without\n", p.log.all.size());
}

SC_TEST("command hit (F8.2): on the nation, each division's own command post and artillery; the army's and the regiments' are not") {
    std::string e;
    AirWarData d;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json", d, &e));
    auto gd = std::make_shared<GroundWarData>();
    SC_EXPECT(LoadGroundWarData(kData, "warsaw_pact", *gd, &e));
    const FrontDef* front = d.theatre.FrontFor(d.faction.front);
    SC_EXPECT(front != nullptr && gd->tables.command_hit.on);
    if (front == nullptr) return;
    // Both doctrines' 3/4 line, and our restore of control (mt_stashek_losses).
    SC_EXPECT(gd->ours.doctrine.effectiveness.hq_hit_ineffective_below == 0.75 && gd->enemy.doctrine.effectiveness.hq_hit_ineffective_below == 0.75);
    SC_EXPECT(gd->ours.doctrine.restore_control_h.lo == 1.0 && gd->ours.doctrine.restore_control_h.hi == 3.0);
    LineOfContact line;
    line.Build(*front, d.tuning.Get("air_front.station_m"));
    GroundWar g;
    SC_EXPECT(g.Build(gd, line, d.theatre, d.country.get(), 21, &e));
    const auto object = [&d](int k) { return d.theatre.objects[static_cast<std::size_t>(k)].id; };
    std::vector<std::string> cps, arts;
    for (const CommandTie& c : g.CommandTies()) cps.push_back(object(c.object) + ":" + g.Formations()[static_cast<std::size_t>(c.formation)].unit.id);
    for (const TroopsTie& t : g.Ties()) {
        if (t.artillery) arts.push_back(object(t.object) + ":" + g.Formations()[static_cast<std::size_t>(t.formation)].unit.id);
    }
    const auto has = [](const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); };
    SC_EXPECT(has(cps, "mrd9_hq:mrd_9") && has(cps, "sw_brigade_cp:sw_brigade"));
    SC_EXPECT(has(arts, "mrd7_artillery:mrd_7") && has(arts, "mrd9_artillery:mrd_9"));
    for (const std::string& s : cps) SC_EXPECT(s.find("army_") != 0 && s.find("general_staff") != 0);
    for (const std::string& s : arts) SC_EXPECT(s.find("tr_") != 0 && s.find("group_") != 0);
    std::string all;
    for (const std::string& s : cps) all += " " + s;
    all += " |";
    for (const std::string& s : arts) all += " " + s;
    std::printf("    command posts and artillery:%s\n", all.c_str());
}
