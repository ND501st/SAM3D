// Track F, F3.2: the sector's combat model as pure tables (simcore/ground_war/combat.hpp, units.hpp;
// docs/fronts/03_GROUND_WAR.md section 4). Each result is checked against a hand calculation from the cited norms in
// data/gameplay/ground_war.json: FM 101-10-1's day at 3:1, HERO's classes, Dupuy's factors, a river that holds.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "simcore/command.hpp"
#include "simcore/ground_war.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::groundwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

const GroundTables& T() {
    static GroundTables t;
    static bool loaded = false;
    if (!loaded) {
        std::string e;
        if (!LoadGroundTables(kData, t, &e)) std::printf("    %s\n", e.c_str());
        loaded = true;
    }
    return t;
}

// The Pact's caps (mt_kurkov_tank_army, mt_1967_volkov_nonnuclear): 30 breaking through, 50 in depth, 70 broken.
AdvanceCaps PactCaps() {
    AdvanceCaps c;
    c.breakthrough_km_per_day = 30.0;
    c.depth_km_per_day = 50.0;
    c.broken_km_per_day = 70.0;
    return c;
}

SectorSide Attacker(double power, int day = 0) {
    SectorSide s;
    s.power = power;
    s.posture = Posture::Attack;
    s.contact_day = day;
    return s;
}

SectorSide Defender(double power, Posture p, int day = 0) {
    SectorSide s;
    s.power = power;
    s.posture = p;
    s.contact_day = day;
    return s;
}

}  // namespace

SC_TEST("ground combat (F3.2): FM 101-10-1's day at 3:1 against a prepared defence, first and later days") {
    const SectorConditions open;
    // Attacker 4.5 against a prepared defender of 1.0 (Dupuy's 1.5): R = 4.5 / 1.5 = 3, the reference ratio.
    SectorResult r = ResolveSector(T(), Attacker(4.5), Defender(1.0, Posture::PreparedDefence), open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.ratio, 3.0, 1e-12);
    SC_EXPECT_NEAR(r.attacker_daily, 0.041, 1e-12);  // attack of a position, day 1: 3.8 + 0.3 %
    SC_EXPECT_NEAR(r.defender_daily, 0.022, 1e-12);  // defence of a position, day 1: 1.9 + 0.3 %
    SC_EXPECT_NEAR(r.attacker_loss, 0.041, 1e-12);   // a whole day's
    SC_EXPECT_NEAR(r.defender_loss, 0.022, 1e-12);
    // R = 3 is HERO's moderate opposition: 10.10 km a day, under the Pact's 30 breaking through.
    SC_EXPECT(r.opposition == Opposition::Moderate);
    SC_EXPECT_NEAR(r.advance_km_per_day, 10.10, 1e-12);
    SC_EXPECT_NEAR(r.advance_km, 10.10, 1e-12);
    // Later days: 1.9 + 0.3 and 1.0 + 0.3 %.
    r = ResolveSector(T(), Attacker(4.5, 1), Defender(1.0, Posture::PreparedDefence, 1), open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.attacker_daily, 0.022, 1e-12);
    SC_EXPECT_NEAR(r.defender_daily, 0.013, 1e-12);
    // One hour of it: 1 - (1 - 0.041)^(1/24), and a 24th of the day's advance.
    r = ResolveSector(T(), Attacker(4.5), Defender(1.0, Posture::PreparedDefence), open, PactCaps(), 1.0, 1.0);
    SC_EXPECT_NEAR(r.attacker_loss, 1.0 - std::pow(0.959, 1.0 / 24.0), 1e-15);
    SC_EXPECT_NEAR(r.advance_km, 10.10 / 24.0, 1e-12);
    // A fortified zone: 6.3 + 0.3 % for the attacker (Dupuy's 1.6: power 4.8 for R = 3).
    r = ResolveSector(T(), Attacker(4.8), Defender(1.0, Posture::FortifiedDefence), open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.ratio, 3.0, 1e-12);
    SC_EXPECT_NEAR(r.attacker_daily, 0.066, 1e-12);
    // A thin defence (wider than its frontage) is FM 101-10-1's defence of a sector: 3.2 + 0.3 %.
    SectorSide thin = Defender(1.0, Posture::PreparedDefence);
    thin.thin = true;
    r = ResolveSector(T(), Attacker(4.5), thin, open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.defender_daily, 0.035, 1e-12);
    // A covering force: 0.9 + 0.3 %.
    SectorSide cover = Defender(1.0, Posture::Delay);
    cover.covering = true;
    r = ResolveSector(T(), Attacker(3.6), cover, open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.ratio, 3.0, 1e-12);
    SC_EXPECT_NEAR(r.defender_daily, 0.012, 1e-12);
    SC_EXPECT_NEAR(r.attacker_daily, 0.016, 1e-12);  // pursuit of a delaying force: 1.3 + 0.3 %
}

SC_TEST("ground combat (F3.2): losses scale away from 3:1 by (3/R)^0.5 and (R/3)^0.5, clipped to 0.25-4") {
    const SectorConditions open;
    // R = 6: both sides' battle losses x sqrt(1/2) and sqrt(2).
    SectorResult r = ResolveSector(T(), Attacker(9.0), Defender(1.0, Posture::PreparedDefence), open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.ratio, 6.0, 1e-12);
    SC_EXPECT_NEAR(r.attacker_daily, 0.038 * std::sqrt(0.5) + 0.003, 1e-12);
    SC_EXPECT_NEAR(r.defender_daily, 0.019 * std::sqrt(2.0) + 0.003, 1e-12);
    // R = 1: the attacker pays sqrt(3) times, the defender sqrt(1/3).
    r = ResolveSector(T(), Attacker(1.5), Defender(1.0, Posture::PreparedDefence), open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.ratio, 1.0, 1e-12);
    SC_EXPECT_NEAR(r.attacker_daily, 0.038 * std::sqrt(3.0) + 0.003, 1e-12);
    SC_EXPECT_NEAR(r.defender_daily, 0.019 / std::sqrt(3.0) + 0.003, 1e-12);
    // R = 0.1: sqrt(30) clipped to 4 for the attacker, sqrt(1/30) to 0.25 for the defender.
    SC_EXPECT_NEAR(LossScale(T(), 0.1, true), 4.0, 1e-12);
    SC_EXPECT_NEAR(LossScale(T(), 0.1, false), 0.25, 1e-12);
    SC_EXPECT_NEAR(LossScale(T(), 3.0, true), 1.0, 1e-12);
    SC_EXPECT_NEAR(LossScale(T(), 100.0, false), 4.0, 1e-12);
}

SC_TEST("ground combat (F3.2): HERO's classes by ratio, the faction's caps, the stall below 1.5") {
    SC_EXPECT(OppositionOf(T(), 0.0) == Opposition::Stalled);
    SC_EXPECT(OppositionOf(T(), 1.49) == Opposition::Stalled);
    SC_EXPECT(OppositionOf(T(), 1.5) == Opposition::Intense);
    SC_EXPECT(OppositionOf(T(), 2.49) == Opposition::Intense);
    SC_EXPECT(OppositionOf(T(), 2.5) == Opposition::Moderate);
    SC_EXPECT(OppositionOf(T(), 3.99) == Opposition::Moderate);
    SC_EXPECT(OppositionOf(T(), 4.0) == Opposition::Slight);
    SC_EXPECT(OppositionOf(T(), 6.0) == Opposition::Negligible);
    SC_EXPECT(OppositionOf(T(), 1000.0) == Opposition::Negligible);
    SC_EXPECT_NEAR(AdvanceKmPerDay(T(), Opposition::Stalled), 0.0, 1e-12);
    SC_EXPECT_NEAR(AdvanceKmPerDay(T(), Opposition::Intense), 2.80, 1e-12);
    SC_EXPECT_NEAR(AdvanceKmPerDay(T(), Opposition::Slight), 23.34, 1e-12);
    SC_EXPECT_NEAR(AdvanceKmPerDay(T(), Opposition::Unopposed), 69.78, 1e-12);
    const SectorConditions open;
    // Stalled: no sustained advance.
    SectorResult r = ResolveSector(T(), Attacker(1.5), Defender(1.0, Posture::PreparedDefence), open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.advance_km, 0.0, 1e-12);
    // R = 10 against a prepared defence: negligible's 54.79 capped at the Pact's 30 breaking through.
    r = ResolveSector(T(), Attacker(15.0), Defender(1.0, Posture::PreparedDefence), open, PactCaps(), 1.0, 24.0);
    SC_EXPECT(r.opposition == Opposition::Negligible);
    SC_EXPECT_NEAR(r.advance_km_per_day, 30.0, 1e-12);
    // Against a delay (Dupuy's 1.2): the depth's 50.
    r = ResolveSector(T(), Attacker(12.0), Defender(1.0, Posture::Delay), open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.ratio, 10.0, 1e-12);
    SC_EXPECT_NEAR(r.advance_km_per_day, 50.0, 1e-12);
    // Against a broken enemy: the table's 54.79 under the cap of 70.
    SectorSide broken = Defender(1.0, Posture::Delay);
    broken.broken = true;
    r = ResolveSector(T(), Attacker(12.0), broken, open, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.advance_km_per_day, 54.79, 1e-12);
    // No defender: unopposed 69.78; the attacker loses only its nonbattle 0.3 %; a draw of 1.2 meets the cap of 70.
    r = ResolveSector(T(), Attacker(5.0), Defender(0.0, Posture::PreparedDefence), open, PactCaps(), 1.0, 24.0);
    SC_EXPECT(r.opposition == Opposition::Unopposed);
    SC_EXPECT_NEAR(r.advance_km_per_day, 69.78, 1e-12);
    SC_EXPECT_NEAR(r.attacker_daily, 0.003, 1e-12);
    SC_EXPECT_NEAR(r.defender_loss, 0.0, 1e-12);
    r = ResolveSector(T(), Attacker(5.0), Defender(0.0, Posture::PreparedDefence), open, PactCaps(), 1.2, 24.0);
    SC_EXPECT_NEAR(r.advance_km_per_day, 70.0, 1e-12);
    // The draw scales the table's rate below the cap: moderate x 0.5.
    r = ResolveSector(T(), Attacker(4.5), Defender(1.0, Posture::PreparedDefence), open, PactCaps(), 0.5, 24.0);
    SC_EXPECT_NEAR(r.advance_km_per_day, 5.05, 1e-12);
    // AdvanceCap by the defender's posture.
    SC_EXPECT_NEAR(AdvanceCap(PactCaps(), Posture::HastyDefence, false), 30.0, 1e-12);
    SC_EXPECT_NEAR(AdvanceCap(PactCaps(), Posture::Withdrawal, false), 50.0, 1e-12);
    SC_EXPECT_NEAR(AdvanceCap(PactCaps(), Posture::PreparedDefence, true), 70.0, 1e-12);
}

SC_TEST("ground combat (F3.2): Dupuy's factors, the ground, the night and a river that holds") {
    // Forest: the defender x 1.3, the advance x 0.6. Attacker 5.85 against 1.0 prepared in forest: R = 5.85 / 1.95 = 3.
    SectorConditions forest;
    forest.terrain = groundwar::Terrain::Forest;
    SectorResult r = ResolveSector(T(), Attacker(5.85), Defender(1.0, Posture::PreparedDefence), forest, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.ratio, 3.0, 1e-12);
    SC_EXPECT_NEAR(r.advance_km_per_day, 10.10 * 0.6, 1e-12);
    // Night: the attacker x 0.8.
    SectorConditions night;
    night.night = true;
    r = ResolveSector(T(), Attacker(4.5), Defender(1.0, Posture::PreparedDefence), night, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.ratio, 4.5 * 0.8 / 1.5, 1e-12);
    // Each posture's factor through the ratio: attacker 3 against 1.0 is R = 3 / factor.
    for (std::size_t i = 0; i < kPostureCount; ++i) {
        const Posture p = static_cast<Posture>(i);
        r = ResolveSector(T(), Attacker(3.0), Defender(1.0, p), SectorConditions(), PactCaps(), 1.0, 24.0);
        SC_EXPECT_NEAR(r.ratio, 3.0 / PostureFactor(T(), p), 1e-12);
    }
    // A river line: the defender x 1.5, and the line stops at the bank (2 km ahead) though the day's rate is 30 km.
    SectorConditions river;
    river.river_line = true;
    river.river_ahead_km = 2.0;
    r = ResolveSector(T(), Attacker(19.5), Defender(1.0, Posture::HastyDefence), river, PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.ratio, 19.5 / (1.3 * 1.5), 1e-12);
    SC_EXPECT_NEAR(r.advance_km_per_day, 30.0, 1e-12);
    SC_EXPECT_NEAR(r.advance_km, 2.0, 1e-12);
    SC_EXPECT(r.at_river);
    // Short of the bank it goes on as before.
    river.river_ahead_km = 50.0;
    r = ResolveSector(T(), Attacker(19.5), Defender(1.0, Posture::HastyDefence), river, PactCaps(), 1.0, 24.0);
    SC_EXPECT(!r.at_river);
    SC_EXPECT_NEAR(r.advance_km, 30.0, 1e-12);
    // Crossing: an intact bridge 2 h, a cut one 12 h, 6 h with the pontoon park.
    SC_EXPECT_NEAR(CrossingHours(T(), true, false), 2.0, 1e-12);
    SC_EXPECT_NEAR(CrossingHours(T(), false, false), 12.0, 1e-12);
    SC_EXPECT_NEAR(CrossingHours(T(), false, true), 6.0, 1e-12);
}

SC_TEST("ground combat (F3.2): an inactive sector, a quiet sector in closed form, the tanks' table, decision times") {
    // Neither side attacks: both take the inactive 1.0 % a day and nobody moves.
    SectorResult r = ResolveSector(T(), Defender(5.0, Posture::HastyDefence), Defender(1.0, Posture::PreparedDefence), SectorConditions(),
                                   PactCaps(), 1.0, 24.0);
    SC_EXPECT_NEAR(r.attacker_daily, 0.010, 1e-12);
    SC_EXPECT_NEAR(r.defender_daily, 0.010, 1e-12);
    SC_EXPECT_NEAR(r.advance_km, 0.0, 1e-12);
    // Three quiet days at once: 1 - 0.99^3.
    SC_EXPECT_NEAR(LossOverHours(DailyLoss(T(), LossAction::Inactive, 1), 72.0), 1.0 - 0.99 * 0.99 * 0.99, 1e-15);
    SC_EXPECT_NEAR(LossOverHours(0.041, 0.0), 0.0, 1e-15);
    SC_EXPECT_NEAR(LossOverHours(DailyLoss(T(), LossAction::Reserve, 0), 24.0), 0.006, 1e-12);
    // FM 101-10-1 table 7-7: a covering force heavily committed loses 73 % of its tanks on day 1, 8 % on day 2.
    SC_EXPECT_NEAR(TankLossShare(T(), TankCommitment::DelayHeavy, 1), 0.73, 1e-12);
    SC_EXPECT_NEAR(TankLossShare(T(), TankCommitment::DelayHeavy, 2), 0.08, 1e-12);
    SC_EXPECT_NEAR(TankLossShare(T(), TankCommitment::DefendModerate, 3), 0.06, 1e-12);
    SC_EXPECT_NEAR(TankLossShare(T(), TankCommitment::DefendModerate, 4), 0.0, 1e-12);
    // FM 100-2-1 p. 5-14: a division reacts in 2-4 h, a battalion plans in 20-45 min.
    SC_EXPECT_NEAR(DecisionHours(T(), Echelon::Division, false, 0.0), 2.0, 1e-12);
    SC_EXPECT_NEAR(DecisionHours(T(), Echelon::Division, false, 0.5), 3.0, 1e-12);
    SC_EXPECT_NEAR(DecisionHours(T(), Echelon::Division, false, 1.0), 4.0, 1e-12);
    SC_EXPECT_NEAR(DecisionHours(T(), Echelon::Battalion, true, 0.0) * 60.0, 20.0, 0.01);
    SC_EXPECT_NEAR(DecisionHours(T(), Echelon::Regiment, true, 2.0), 2.5, 1e-12);  // u clipped
}

SC_TEST("ground combat (F3.2): strength by weighted equipment, power, the effectiveness lines and NATO's breakpoints") {
    // 40 T-72 (1.6), 140 BMP-2 (0.7), 18 2S1 (0.55), 2400 men (0.01 each): 64 + 98 + 9.9 + 24 = 195.9.
    const std::vector<std::pair<std::string, int>> mrr = {{"t72", 40}, {"bmp2", 140}, {"2s1", 18}, {"ural_truck", 50}};
    SC_EXPECT_NEAR(StrengthOf(T(), mrr, 2400), 195.9, 1e-9);
    GroundForceDef pact;
    SC_EXPECT(LoadGroundForce(kData, "warsaw_pact", pact));
    const command::OobUnit* u = pact.oob.Find("pact_mrr_35_1");
    SC_EXPECT(u != nullptr && u->personnel.Total() == 2400);
    if (u != nullptr) SC_EXPECT_NEAR(StrengthOfUnit(T(), *u), 195.9, 1e-9);
    // The tree keeps its strength when built into units.
    const std::vector<GroundUnit> units = UnitsOf(T(), pact.oob, Side::Enemy);
    SC_EXPECT(units.size() == pact.oob.units.size() && units.front().parent == -1 && !units.front().children.empty());
    SC_EXPECT_NEAR(TreeStrength(units, 0), StrengthOfTree(T(), pact.oob, pact.oob.units.front().id), 1e-9);
    SC_EXPECT_NEAR(TreeStrength(units, 0, true), TreeStrength(units, 0), 1e-12);
    SC_EXPECT_NEAR(units[1].Fraction(), 1.0, 1e-12);
    // Power: supply and cohesion each from their floor (0.5 and 0.3) to 1.
    SC_EXPECT_NEAR(UnitPower(T(), 100.0, 1.0, 1.0), 100.0, 1e-12);
    SC_EXPECT_NEAR(UnitPower(T(), 100.0, 0.0, 1.0), 50.0, 1e-12);
    SC_EXPECT_NEAR(UnitPower(T(), 100.0, 1.0, 0.0), 30.0, 1e-12);
    SC_EXPECT_NEAR(UnitPower(T(), 100.0, 0.5, 0.5), 100.0 * 0.75 * 0.65, 1e-12);
    // The Pact's lines: over 2/3 effective, 1/2-2/3 degraded, under 1/2 (or 3/4 with the command hit) ineffective.
    const EffectivenessDef& e = pact.doctrine.effectiveness;
    SC_EXPECT(EffectivenessByLines(e, 0.70, false) == Effectiveness::Effective);
    SC_EXPECT(EffectivenessByLines(e, 0.60, false) == Effectiveness::Degraded);
    SC_EXPECT(EffectivenessByLines(e, 0.49, false) == Effectiveness::Ineffective);
    SC_EXPECT(EffectivenessByLines(e, 0.70, true) == Effectiveness::Ineffective);
    SC_EXPECT(EffectivenessByLines(e, 0.80, true) == Effectiveness::Effective);
    // NATO's breakpoints: an attack stops at 15-35 %, drawn at u; a bold commander's +1 pull moves it by 0.2 of the range.
    GroundForceDef nato;
    SC_EXPECT(LoadGroundForce(kData, "nato", nato));
    const EffectivenessDef& n = nato.doctrine.effectiveness;
    SC_EXPECT_NEAR(DrawBreakpoint(n, true, 0.5, 0.0, T().trait_shift), 0.25, 1e-12);
    SC_EXPECT_NEAR(DrawBreakpoint(n, true, 0.5, 1.0, T().trait_shift), 0.25 + 0.2 * 0.2, 1e-12);
    SC_EXPECT_NEAR(DrawBreakpoint(n, true, 0.0, -1.0, T().trait_shift), 0.15, 1e-12);  // kept in the range
    SC_EXPECT_NEAR(DrawBreakpoint(n, false, 1.0, 0.0, T().trait_shift), 0.50, 1e-12);
}

SC_TEST("ground combat (F3.2): the two armies' weighted strengths, and the Pact's attack at its 3:1 by the table") {
    GroundForceDef pact;
    GroundForceDef nato;
    GroundDefenceDef ours;
    SC_EXPECT(LoadGroundForce(kData, "warsaw_pact", pact) && LoadGroundForce(kData, "nato", nato) && LoadGroundDefence(kData, ours));
    const double p = StrengthOfTree(T(), pact.oob, "pact_army");
    const double n = StrengthOfTree(T(), nato.oob, "nato_corps");
    const double o = StrengthOfTree(T(), ours.oob, "army_3");
    std::printf("    strength (T-55 equivalents): the Pact's army %.0f, NATO's corps %.0f, our 3rd Army %.0f; %.2f:1 and %.2f:1\n", p, n, o,
                p / o, n / o);
    // Both invaders outweigh the 3rd Army, by less than the 6:1 nobody masses on a whole front.
    SC_EXPECT(p / o > 1.5 && p / o < 6.0);
    SC_EXPECT(n / o > 1.5 && n / o < 6.0);
    // A Pact division (about 600-800) against one of ours in prepared defence: the ratio by the table.
    const double pd = StrengthOfTree(T(), pact.oob, "pact_mrd_35");
    const double od = StrengthOfTree(T(), ours.oob, "mrd_7");
    const SectorResult r = ResolveSector(T(), Attacker(2.0 * pd), Defender(od, Posture::PreparedDefence), SectorConditions(), PactCaps(), 1.0, 24.0);
    std::printf("    two Pact divisions (%.0f each) on the 7th (%.0f) prepared: R %.2f, %s, %.1f km, losses %.1f%% and %.1f%%\n", pd, od, r.ratio,
                OppositionName(r.opposition), r.advance_km, 100.0 * r.attacker_loss, 100.0 * r.defender_loss);
    SC_EXPECT(r.ratio > 1.5);
}

SC_TEST("ground combat (F3.2): pure and cheap: the same inputs give the same result; a call costs well under a microsecond") {
    const GroundTables& t = T();
    const AdvanceCaps caps = PactCaps();
    const SectorResult a = ResolveSector(t, Attacker(7.3), Defender(2.1, Posture::HastyDefence), SectorConditions(), caps, 0.9, 1.0);
    const SectorResult b = ResolveSector(t, Attacker(7.3), Defender(2.1, Posture::HastyDefence), SectorConditions(), caps, 0.9, 1.0);
    SC_EXPECT(a.ratio == b.ratio && a.attacker_loss == b.attacker_loss && a.defender_loss == b.defender_loss && a.advance_km == b.advance_km);
    // The README's budget: 20 ms CPU a campaign day for the whole nation; at most 10 contested sectors resolved an hour.
    const int n = 1000000;
    double sink = 0.0;
    SectorConditions c;
    const std::clock_t c0 = std::clock();
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) {
        c.terrain = static_cast<groundwar::Terrain>(i % static_cast<int>(kTerrainCount));
        c.night = (i & 1) != 0;
        c.river_line = (i % 7) == 0;
        c.river_ahead_km = 3.0;
        const double ap = 1.0 + static_cast<double>(i % 97) * 0.13;
        const Posture p = static_cast<Posture>(1 + i % 5);
        const SectorResult r = ResolveSector(t, Attacker(ap, i % 3), Defender(1.0 + static_cast<double>(i % 13) * 0.1, p, i % 2), c, caps,
                                             0.8 + static_cast<double>(i % 5) * 0.1, 1.0);
        sink += r.attacker_loss + r.defender_loss + r.advance_km;
    }
    const double wall_ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / n;
    const double cpu_ns = 1e9 * static_cast<double>(std::clock() - c0) / CLOCKS_PER_SEC / n;
    // A campaign day at the budget's worst: 10 sectors x 24 hours.
    std::printf("    ResolveSector: %.0f ns a call (CPU %.0f ns); 240 calls a campaign day: %.3f ms (sink %.1f)\n", wall_ns, cpu_ns,
                240.0 * cpu_ns * 1e-6, sink);
    SC_EXPECT(sink > 0.0);
    SC_EXPECT(cpu_ns < 5000.0);  // loose: a timing test must not fail on a busy machine; measured ~0.1 us
}
