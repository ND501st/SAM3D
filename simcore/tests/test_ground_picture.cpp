// Track F, F3.5: each side's picture of the ground war (simcore/ground_war/picture.hpp, ground_war.hpp; docs/fronts/
// 03_GROUND_WAR.md section 9). A report arrives with its delay and the belief holds between reports; a believed strength is
// the truth when compiled times a factor keyed by (side, source, sector, report hour); the enemy sees ours from the air
// where its picture has troops and by ground reconnaissance at the line, ours see the enemy by contact and the army's
// intelligence; a commander acts on a stale picture, and the truth changed after a report does not move his decision;
// the enemy's picture says where its ground front collapses, the air front's crisis input, only while the ground war runs.
// F3.7: the air's sightings are of the theatre's troops objects tied to a formation, not of a place near it.
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

// The test war (F3.4's counterattack case, with the picture's data): the enemy's division and regiment attack in the
// east, a second-echelon division behind them; our division holds the east, a brigade the west, a tank division in reserve.
std::shared_ptr<GroundWarData> War() {
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

int FormationOf(const GroundWar& g, const std::string& id) {
    for (std::size_t i = 0; i < g.Formations().size(); ++i) {
        if (g.Formations()[i].unit.id == id) return static_cast<int>(i);
    }
    return -1;
}

std::size_t Idx(int i) { return static_cast<std::size_t>(i); }

const GroundLogEntry* FirstOrder(const std::vector<GroundLogEntry>& log, GroundOrder o) {
    for (const GroundLogEntry& e : log) {
        if (e.what == GroundNote::Order && e.order == o) return &e;
    }
    return nullptr;
}

struct Fixture {
    LineOfContact line;
    TheatreDef theatre;
    GroundEnv env;
    GroundWar g;
    std::vector<GroundLogEntry> log;
    Fixture() {
        line.Build(StraightFront(), 5000.0);
        env.theatre = &theatre;
    }
    bool Build(std::shared_ptr<GroundWarData> d, unsigned seed) { return g.Build(std::move(d), line, theatre, nullptr, seed); }
    // Steps to hour `h` (the hours before it run), keeping the log.
    void To(int h) {
        g.Step(static_cast<double>(h), env, line);
        log.insert(log.end(), g.Log().begin(), g.Log().end());
        g.ClearLog();
    }
};

}  // namespace

SC_TEST("ground picture (F3.5): a believed strength's factor is lognormal of mean 1, keyed: the same report the same factor") {
    const Rng root(12345);
    double sum = 0.0, sum_log = 0.0, sum_log2 = 0.0;
    const int n = 4000;
    for (int k = 0; k < n; ++k) {
        const double f = BeliefFactor(root, Side::Enemy, PictureSource::GroundRecon, k % 7, static_cast<double>(k), 0.35);
        sum += f;
        sum_log += std::log(f);
        sum_log2 += std::log(f) * std::log(f);
    }
    const double mean = sum / n, ml = sum_log / n, sd = std::sqrt(sum_log2 / n - ml * ml);
    SC_EXPECT_NEAR(mean, 1.0, 0.03);
    SC_EXPECT_NEAR(sd, 0.35, 0.02);
    // Keyed: the same (side, source, sector, hour) gives the same factor; any key changed another.
    const double a = BeliefFactor(root, Side::Ours, PictureSource::Contact, 1, 18.0, 0.25);
    SC_EXPECT(a == BeliefFactor(root, Side::Ours, PictureSource::Contact, 1, 18.0, 0.25));
    SC_EXPECT(a != BeliefFactor(root, Side::Enemy, PictureSource::Contact, 1, 18.0, 0.25));
    SC_EXPECT(a != BeliefFactor(root, Side::Ours, PictureSource::ArmyIntel, 1, 18.0, 0.25));
    SC_EXPECT(a != BeliefFactor(root, Side::Ours, PictureSource::Contact, 0, 18.0, 0.25));
    SC_EXPECT(a != BeliefFactor(root, Side::Ours, PictureSource::Contact, 1, 30.0, 0.25));
    SC_EXPECT(BeliefFactor(root, Side::Ours, PictureSource::Own, 1, 18.0, 0.0) == 1.0);
    std::printf("    mean %.3f, log sd %.3f over %d draws\n", mean, sd, n);
}

SC_TEST("ground picture (F3.5): reports arrive with their delay, the belief holds between them, the truth then times its factor") {
    const PictureTables& pt = Tables().picture;
    Fixture w;
    SC_EXPECT(w.Build(War(), 7));
    const std::size_t div = Idx(FormationOf(w.g, "red_div")), acr = Idx(FormationOf(w.g, "red_acr"));
    const std::size_t second = Idx(FormationOf(w.g, "red_second")), mine = Idx(FormationOf(w.g, "blue_div"));
    // At the start: our own units as they stand, the enemy's as peacetime intelligence put them.
    const GroundPicture& ours = w.g.Picture(Side::Ours);
    SC_EXPECT(ours.units[mine].source == PictureSource::Own && ours.units[div].source == PictureSource::Prior);
    SC_EXPECT(w.g.Picture(Side::Enemy).units[mine].source == PictureSource::Prior);
    // The theatre's clock starts at 06:00 local: reports compiled at 0, 12, 24...; ours arrive own_delay_h later.
    const double period = 24.0 / Tables().division_reports_per_day;
    std::vector<double> truth_div(200, 0.0);
    std::vector<int> arrivals;
    double last_obs = ours.units[div].observed_h;
    bool true_then = true, same_factor = false;
    for (int h = 1; h <= 120; ++h) {
        w.To(h);
        // The two attackers in the first report (compiled at 0, in at 2): the same sector's report, the same factor.
        if (h == 3) {
            same_factor = ours.units[div].source == PictureSource::Contact && ours.units[acr].source == PictureSource::Contact &&
                          ours.units[div].factor == ours.units[acr].factor && ours.units[div].factor != 1.0;
        }
        truth_div[static_cast<std::size_t>(h)] = w.g.StrengthAt(div, static_cast<double>(h)) / w.g.Formations()[div].unit.establishment;
        const UnitBelief& b = ours.units[div];
        if (b.observed_h != last_obs) {
            arrivals.push_back(h - 1);  // merged in the hour that just ran
            last_obs = b.observed_h;
            // The truth when it was compiled, times its factor.
            const std::size_t at = static_cast<std::size_t>(b.observed_h);
            const double then = at == 0 ? 1.0 : truth_div[at];
            true_then = true_then && std::fabs(b.fraction - then * b.factor) < 1e-9;
            SC_EXPECT_NEAR(static_cast<double>(h - 1) - b.observed_h, pt.own_delay_h, 1e-9);
        }
    }
    SC_EXPECT(static_cast<double>(arrivals.size()) >= 120.0 / period - 1.0);
    SC_EXPECT(true_then && same_factor);
    // The second echelon, until committed, is the army's intelligence's.
    if (w.g.Formations()[second].axis < 0) SC_EXPECT(ours.units[second].source == PictureSource::ArmyIntel);
    // Stale: the believed strength is not the truth now (the attack goes on between reports).
    std::printf("    %zu arrivals (first at %d h); red_div believed %.3f (x%.3f, %s, %.0f h old) against %.3f now\n", arrivals.size(),
                arrivals.empty() ? -1 : arrivals.front(), ours.units[div].fraction, ours.units[div].factor,
                PictureSourceName(ours.units[div].source), w.g.Now() - ours.units[div].observed_h, w.g.Formations()[div].unit.Fraction());
}

// The theatre's troops objects tied to our formations (F3.7): east_troops the division's, td_troops the reserve's;
// stray_troops no unit's.
TheatreObject TroopsObject(const std::string& id, double e_km, double n_km) {
    TheatreObject o;
    o.id = id;
    o.name = id;
    o.kind = ObjectKind::Troops;
    o.pos = Km(e_km, n_km);
    return o;
}

SC_TEST("ground picture (F3.5, F3.7): the enemy sees ours from the air where its picture holds their troops, by ground reconnaissance elsewhere") {
    Fixture w;
    w.theatre.objects = {TroopsObject("east_troops", 55, 10), TroopsObject("td_troops", 75, 40), TroopsObject("stray_troops", 80, 45)};
    std::shared_ptr<GroundWarData> d = War();
    for (command::OobUnit& u : d->ours.oob.units) {
        if (u.id == "blue_div") u.objects = {"east_troops"};
        if (u.id == "blue_td") u.objects = {"td_troops"};
    }
    SC_EXPECT(w.Build(d, 7));
    SC_EXPECT(w.g.Ties().size() == 2u);
    IntelPicture intel;
    IntelEntry tr;
    tr.key = tr.object_id = "east_troops";
    tr.kind = ObjectKind::Troops;
    tr.pos = Km(55, 10);  // the division's troops, 10 km behind the line in the east
    tr.observed_h = 0.0;
    intel.entries.push_back(tr);
    IntelEntry stray = tr;
    stray.key = stray.object_id = "stray_troops";
    stray.pos = Km(80, 45);  // troops 5 km from the reserve's place, but none of its own: not a sighting of it (F3.7)
    intel.entries.push_back(stray);
    w.env.intel = &intel;
    w.To(6);
    const GroundPicture& theirs = w.g.Picture(Side::Enemy);
    const std::size_t east = Idx(FormationOf(w.g, "blue_div")), west = Idx(FormationOf(w.g, "blue_bde")), td = Idx(FormationOf(w.g, "blue_td"));
    SC_EXPECT(theirs.units[east].source == PictureSource::Air);
    SC_EXPECT(theirs.units[west].source == PictureSource::GroundRecon);
    SC_EXPECT(theirs.units[td].source == PictureSource::Prior);  // the reserve: not seen yet
    // A sighting of the reserve's own troops: the next report sees it too; one too old is not a sighting.
    intel.entries[1].key = intel.entries[1].object_id = "td_troops";
    intel.entries[1].observed_h = 10.0;
    intel.entries[0].observed_h = -100.0;  // the east's sighting now stale
    w.To(18);
    SC_EXPECT(theirs.units[td].source == PictureSource::Air);
    SC_EXPECT(theirs.units[east].source == PictureSource::GroundRecon);
    std::printf("    east %s x%.3f, west %s x%.3f, reserve %s x%.3f\n", PictureSourceName(theirs.units[east].source), theirs.units[east].factor,
                PictureSourceName(theirs.units[west].source), theirs.units[west].factor, PictureSourceName(theirs.units[td].source),
                theirs.units[td].factor);
}

SC_TEST("ground picture (F3.5): a commander acts on a stale picture; the truth changed after a report does not move his decision") {
    // No error, only the delay: the belief is the truth when the report was compiled.
    std::shared_ptr<GroundWarData> d = War();
    PictureTables& pt = d->tables.picture;
    pt.own_sigma = pt.contact_sigma = pt.prior_sigma = pt.army_intel_sigma = pt.air_sigma = pt.ground_recon_sigma = 0.0;
    const int hours = 24 * 5;
    // A: the war as it goes; the hour our army commander orders the counterattack, and when his picture of his reserve
    // last changed before it.
    Fixture a;
    SC_EXPECT(a.Build(d, 7));
    const std::size_t td = Idx(FormationOf(a.g, "blue_td"));
    std::vector<double> td_obs(static_cast<std::size_t>(hours + 1), 0.0);
    for (int h = 1; h <= hours; ++h) {
        a.To(h);
        td_obs[static_cast<std::size_t>(h)] = a.g.Picture(Side::Ours).units[td].observed_h;
    }
    const GroundLogEntry* ordered = FirstOrder(a.log, GroundOrder::CounterattackOrdered);
    SC_EXPECT(ordered != nullptr);
    if (ordered == nullptr) return;
    const int hc = static_cast<int>(ordered->h);
    // The report on the reserve his decision read: compiled at `compiled` (his picture as the hour began), a scheduled
    // own report, so in at `arrived` (merged by To(arrived + 1)).
    const int compiled = static_cast<int>(td_obs[static_cast<std::size_t>(hc)]);
    const int arrived = compiled + static_cast<int>(pt.own_delay_h);
    SC_EXPECT(arrived >= 0 && arrived < hc);
    if (arrived < 0 || arrived >= hc) return;
    // B: the same war, the reserve losing half its strength in the hour after that report arrived (an air attack the
    // army's staff has not heard of): the order comes at the same hour on the same ratio, on the stale picture.
    Fixture b;
    SC_EXPECT(b.Build(d, 7));
    b.To(arrived + 1);
    b.g.TestLoss(td, 0.5);
    const Formation& tf = b.g.Formations()[td];
    const double lost_to = UnitPower(Tables(), tf.unit.strength, tf.unit.supply, tf.unit.cohesion);
    b.To(hc + 1);
    const GroundLogEntry* stale = FirstOrder(b.log, GroundOrder::CounterattackOrdered);
    SC_EXPECT(stale != nullptr && stale->h == ordered->h && stale->value == ordered->value);
    const double believed = b.g.Picture(Side::Ours).units[td].power;
    const double truth = UnitPower(Tables(), b.g.Formations()[td].unit.strength, b.g.Formations()[td].unit.supply, b.g.Formations()[td].unit.cohesion);
    // He decided on about half what he had once the loss was taken (F7.1: his order now comes later, and what makes
    // up the loss has begun to return by the order's hour, so the loss is measured when taken).
    SC_EXPECT(lost_to < 0.6 * believed && truth < believed);
    // C: the same loss before the last report was compiled: the report carries it, and his decision moves.
    Fixture c;
    SC_EXPECT(c.Build(d, 7));
    c.To(compiled);  // the hours before it: the report not yet compiled
    c.g.TestLoss(td, 0.5);
    c.To(hc + 1);
    const GroundLogEntry* told = FirstOrder(c.log, GroundOrder::CounterattackOrdered);
    SC_EXPECT(told == nullptr || told->value < ordered->value - 1e-6);
    std::printf("    ordered at %d h on %.2f:1 (the reserve's report compiled at %d h, in at %d h); with the loss after it %.2f:1 on a "
                "reserve of %.0f believed, %.0f true (%.0f at the loss); with the loss before it %s\n",
                hc, ordered->value, compiled, arrived, stale != nullptr ? stale->value : 0.0, believed, truth, lost_to,
                told == nullptr ? "no counterattack" : "a lower ratio");
}

SC_TEST("ground picture (F3.5): the enemy's crisis by sector from its own picture: a formation under half, our counterattack") {
    std::shared_ptr<GroundWarData> d = War();
    d->enemy.doctrine.effectiveness.degraded_below = 2.0 / 3.0;  // the Pact's lines: under 1/2 it asks to withdraw
    d->enemy.doctrine.effectiveness.ineffective_below = 0.5;
    d->ours.deployments.front().units.pop_back();  // no reserve: no counterattack, the crisis is the division's
    Fixture w;
    SC_EXPECT(w.Build(d, 7));
    const std::size_t div = Idx(FormationOf(w.g, "red_div"));
    const int east = w.line.SectorIndex("east"), west = w.line.SectorIndex("west");
    SC_EXPECT(w.g.EnemyCrisis().size() == w.line.SectorCount());
    w.To(13);  // the offensive on; the 12 h report in
    SC_EXPECT(w.g.EnemyCrisis()[Idx(east)] == 0);
    // Its division loses 60 % at 14 h: the enemy's staff hears of it by the division's own report (its commander's
    // reaction, then the delay) or the next scheduled one (compiled at 24 h, in at 26 h), not before.
    w.To(14);
    w.g.TestLoss(div, 0.6);
    w.To(16);
    SC_EXPECT(w.g.EnemyCrisis()[Idx(east)] == 0);  // not yet reported
    int at = -1;
    for (int h = 17; h <= 27 && at < 0; ++h) {
        w.To(h);
        if (w.g.EnemyCrisis()[Idx(east)] != 0) at = h - 1;
    }
    SC_EXPECT(at >= 0 && w.g.EnemyCrisis()[Idx(west)] == 0);
    // Our counterattack (the war with our reserve): the crisis once its attackers' report of it reaches the enemy's staff.
    Fixture v;
    SC_EXPECT(v.Build(War(), 7));
    int starts = -1, crisis_at = -1;
    for (int h = 1; h <= 48 && crisis_at < 0; ++h) {
        v.To(h);
        const GroundLogEntry* s = FirstOrder(v.log, GroundOrder::CounterattackStarts);
        if (s != nullptr && starts < 0) starts = static_cast<int>(s->h);
        if (v.g.EnemyCrisis()[Idx(east)] != 0) crisis_at = h - 1;
    }
    SC_EXPECT(starts >= 0 && crisis_at == starts + static_cast<int>(Tables().picture.own_delay_h));
    SC_EXPECT(v.g.Picture(Side::Enemy).axes[0].counterattacked);
    std::printf("    red_div believed %.2f in the enemy's picture from %d h: crisis in the east; our counterattack at %d h, its crisis from %d h\n",
                w.g.Picture(Side::Enemy).units[div].fraction, at, starts, crisis_at);
}

SC_TEST("ground picture (F3.5): the air front reads the enemy's crisis only while the ground war runs; order-free, no allocation") {
    std::string e;
    AirWarData d;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json", d, &e));
    AirCampaign zero;
    SC_EXPECT(zero.Init(d, 21, &e));
    AirFrontRules rules;
    AirFrontInputs in;
    AirFrontChief who;
    zero.AirFrontDecisionInputs(rules, in, who);
    SC_EXPECT(in.crisis == nullptr);
    d.tuning.values["ground.mode"] = 1.0;
    AirCampaign c;
    SC_EXPECT(c.Init(d, 21, &e));
    SC_EXPECT(c.Ground() != nullptr);
    if (c.Ground() == nullptr) return;
    c.AirFrontDecisionInputs(rules, in, who);
    SC_EXPECT(in.crisis == &c.Ground()->EnemyCrisis() && in.crisis->size() == c.FrontLine().SectorCount());
    // The picture's own war, either way round, the same; the queue never past what Build reserved.
    std::shared_ptr<GroundWarData> gd = War();
    gd->tables.draw_sigma = Tables().draw_sigma;
    Fixture x, y;
    SC_EXPECT(x.Build(gd, 3) && y.Build(gd, 3));
    y.g.SetReverseOrder(true);
    const std::size_t cap = x.g.QueueCapacity();
    x.To(24 * 6);
    y.To(24 * 6);
    bool same = x.log.size() == y.log.size();
    for (std::size_t k = 0; same && k < x.log.size(); ++k) same = x.log[k].h == y.log[k].h && x.log[k].order == y.log[k].order && x.log[k].value == y.log[k].value;
    for (std::size_t i = 0; same && i < x.g.Formations().size(); ++i) {
        same = x.g.Picture(Side::Ours).units[i].fraction == y.g.Picture(Side::Ours).units[i].fraction &&
               x.g.Picture(Side::Enemy).units[i].fraction == y.g.Picture(Side::Enemy).units[i].fraction;
    }
    SC_EXPECT(same);
    SC_EXPECT(x.g.QueueCapacity() == cap);
    std::printf("    %zu entries; %llu reports merged in ours, %llu in the enemy's\n", x.log.size(),
                static_cast<unsigned long long>(x.g.Picture(Side::Ours).reports), static_cast<unsigned long long>(x.g.Picture(Side::Enemy).reports));
}
