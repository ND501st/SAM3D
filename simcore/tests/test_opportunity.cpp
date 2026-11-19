// Targets of opportunity (roadmap B6, docs/air_battle/06_OPPORTUNITY.md): the chain a flight leader runs on what he finds
// (seen, identified, authorised, able, worth it, act; air_war/opportunity.hpp), the faction's rules and the type's ease
// from the data, the attack pass flown on B1's override layer (attack_pass.hpp), the pass's effect against the Array 20
// trial, the claims, the session's passes through the bridge (opportunity_session.hpp, mission_bridge.hpp), the records
// saved, and the abstract level (resolve.hpp FindOpportunity).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/fx/events.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

AirWarData LoadOpp(const std::string& faction) {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

// A plain situation: a column seen clearly, long enough, with room for a pass, 30 km past the line, a gun aboard.
OppSituation Seen(MissionKind mission, OppTarget target, double ease) {
    OppSituation s;
    s.mission = mission;
    s.role = FlightRole::Strike;
    s.ease = ease;
    s.bingo_share = 0.3;
    s.target = target;
    s.confidence = 0.9;
    s.watched_s = 60.0;
    s.range_m = 8000.0;
    s.depth_km = 30.0;
    s.has_gun = true;
    s.fuel_share = 0.8;
    s.speed_mps = 200.0;
    s.release_m = 1115.0;
    return s;
}

std::vector<fx::Event> OfKindOpp(const std::vector<fx::Event>& all, fx::EventKind k) {
    std::vector<fx::Event> out;
    for (const fx::Event& e : all) {
        if (e.kind == k) out.push_back(e);
    }
    return out;
}

std::string JournalText(const AirCampaign& c) {
    std::string t;
    for (const JournalEntry& j : c.Journal()) t += j.kind + "|" + j.text + "\n";
    return t;
}

AirCampaign RunDays(const AirWarData& d, unsigned seed, double days) {
    AirCampaign c;
    c.Init(d, seed);
    while (c.Now() < days * 24.0) {
        if (c.Advance(days * 24.0)) c.SkipPlayerMission();
    }
    return c;
}

}  // namespace

SC_TEST("opportunity: every faction's rules and every type's ease load with their sources") {
    for (const char* f : {"nato", "warsaw_pact", "neutral", "rebels"}) {
        const AirWarData d = LoadOpp(f);
        SC_EXPECT(d.faction.opportunity.present);
        SC_EXPECT(!d.faction.opportunity.cites.empty());
        for (const OpportunityRulesDef::Cite& c : d.faction.opportunity.cites) SC_EXPECT(!c.source.empty() && !c.confidence.empty());
    }
    const AirWarData nato = LoadOpp("nato");
    const AirWarData pact = LoadOpp("warsaw_pact");
    SC_EXPECT(!nato.faction.opportunity.report_in_flight && nato.faction.opportunity.fired_on_may_attack);
    SC_EXPECT(pact.faction.opportunity.report_in_flight && pact.faction.opportunity.armed_recce == "free_hunt_zone");
    SC_EXPECT_NEAR(nato.faction.Aircraft("a10a")->opportunity_ease, 0.8, 1e-12);
    SC_EXPECT_NEAR(nato.faction.Aircraft("tornado_ids")->opportunity_ease, 0.12, 1e-12);
    SC_EXPECT_NEAR(pact.faction.Aircraft("su25")->opportunity_ease, 0.7, 1e-12);
    // The GAU-8's pass from the Array 20 trial (cdat_array20_1980): open fire at 1,115 m in a 3.3 deg dive, 0.8 of the
    // tanks a pass hit out of action, 0.375 of them burnt.
    const WeaponDef* gau8 = nato.Weapon("gau8");
    SC_EXPECT(gau8 != nullptr && gau8->pass.present);
    if (gau8 == nullptr) return;
    SC_EXPECT_NEAR(gau8->pass.open_fire_m, 1115.0, 1e-9);
    SC_EXPECT_NEAR(gau8->pass.per_pass[static_cast<int>(TargetClass::Armour)], 0.8, 1e-9);
    SC_EXPECT_NEAR(gau8->pass.catastrophic_share, 0.375, 1e-9);
    SC_EXPECT(!gau8->pass.source.empty() && !gau8->pass.confidence.empty());
    for (const char* rk : {"s5", "s8", "hydra70"}) {
        const WeaponDef* w = (std::string(rk) == "hydra70" ? nato : pact).Weapon(rk);
        SC_EXPECT(w != nullptr && w->pass.present && w->pass.rounds <= 20);
    }
}

SC_TEST("opportunity: the stream's keyed draw matches the Python mirror") {
    SC_EXPECT_NEAR(static_cast<double>(em::Stream::Opportunity), 21.0, 1e-12);
    SC_EXPECT_NEAR(OppDraw(42, 7, 3, 1, 0), 0.7313714574912689, 1e-15);
}

SC_TEST("opportunity: a Pact strike reports a column at once; its armed reconnaissance hunts only in the free hunt's zone") {
    const AirWarData d = LoadOpp("warsaw_pact");
    const OpportunityRulesDef& rules = d.faction.opportunity;
    const OppLeader ld;
    const OppDecision strike = RunChain(rules, Seen(MissionKind::Strike, OppTarget::Column, 0.2), ld, d.tuning, 5, 1, 1);
    SC_EXPECT(strike.act == OppAct::Report && strike.stopped == OppStep::Authorised && !strike.may);
    OppSituation hunt = Seen(MissionKind::ArmedRecce, OppTarget::Column, 0.2);
    hunt.free_hunt_zone = false;
    std::string why;
    SC_EXPECT(!MayAttack(rules, hunt, &why));
    SC_EXPECT(why == "outside the free hunt's zone");
    hunt.free_hunt_zone = true;
    SC_EXPECT(MayAttack(rules, hunt, &why));
    // Hunting is its task: most leaders attack what they may.
    int attacks = 0;
    for (std::uint64_t k = 0; k < 400; ++k) {
        const OppDecision dec = RunChain(rules, hunt, ld, d.tuning, 5, k, 1);
        SC_EXPECT(dec.may && dec.able);
        attacks += dec.act == OppAct::GunRun || dec.act == OppAct::Attack ? 1 : 0;
    }
    std::printf("    Pact armed recce in its zone: %d of 400 attack (share %.2f)\n", attacks, ActShare(rules, hunt, ld, d.tuning));
    SC_EXPECT(attacks > 300 && attacks < 380);
    // Too near his own troops (700 m: fm_100_2_1_1984), whatever the zone.
    hunt.depth_km = 0.5;
    SC_EXPECT(!MayAttack(rules, hunt, &why));
    SC_EXPECT(why == "too near his own troops");
}

SC_TEST("opportunity: NATO's A-10 attacks beyond the FSCL or in its squadron's area; other close support waits for the FAC") {
    const AirWarData d = LoadOpp("nato");
    const OpportunityRulesDef& rules = d.faction.opportunity;
    std::string why;
    OppSituation a10 = Seen(MissionKind::Cas, OppTarget::Column, 0.8);
    a10.depth_km = 25.0;
    SC_EXPECT(MayAttack(rules, a10, &why) && why == "beyond the fire support coordination line");
    a10.depth_km = 10.0;
    SC_EXPECT(MayAttack(rules, a10, &why) && why == "in its squadron's area");
    OppSituation f16 = Seen(MissionKind::Cas, OppTarget::Column, 0.45);
    f16.depth_km = 10.0;
    SC_EXPECT(!MayAttack(rules, f16, &why));
    f16.designated = true;
    SC_EXPECT(MayAttack(rules, f16, &why) && why == "designated by the controller");
    a10.depth_km = 0.5;  // inside 1,000 m of his own troops
    SC_EXPECT(!MayAttack(rules, a10, &why) && why == "too near his own troops");
}

SC_TEST("opportunity: a NATO Tornado on a strike reports what it sees after landing; fired on, it may hit back at that site") {
    const AirWarData d = LoadOpp("nato");
    const OpportunityRulesDef& rules = d.faction.opportunity;
    OppSituation s = Seen(MissionKind::Strike, OppTarget::Battery, 0.12);
    s.has_gun = false;
    s.has_bombs = true;
    s.primary_due = false;
    const OppDecision dec = RunChain(rules, s, OppLeader(), d.tuning, 9, 2, 4);
    SC_EXPECT(dec.act == OppAct::Report && !dec.may);
    SC_EXPECT(!rules.report_in_flight);  // reported at the debrief (army_it0654_inflightrep)
    s.fired_on = true;
    std::string why;
    SC_EXPECT(MayAttack(rules, s, &why) && why == "it fired on the flight");
    // A planned strike's bombs are owed to its target: fired on, it has nothing else to hit back with.
    s.primary_due = true;
    bool too_close = false;
    SC_EXPECT(!AbleToAttack(s, d.tuning, &too_close, &why) && why == "his stores are for the target");
}

SC_TEST("opportunity: a Pact Mi-24's night hunt waits for the ground post's clearance") {
    const AirWarData d = LoadOpp("warsaw_pact");
    OppSituation s = Seen(MissionKind::HeloRaid, OppTarget::Vehicles, 0.7);
    s.helicopter = true;
    s.night = true;
    std::string why;
    SC_EXPECT(!MayAttack(d.faction.opportunity, s, &why) && why == "a night hunt waits for the ground post's clearance");
    s.designated = true;
    SC_EXPECT(MayAttack(d.faction.opportunity, s, &why));
    s.night = false;
    s.designated = false;
    // By day close support on the Pact's side still waits for its controller (fm_100_2_1_1984 p. 12-8).
    SC_EXPECT(!MayAttack(d.faction.opportunity, s, &why) && why == "close support strikes what the controller designates");
}

SC_TEST("opportunity: a pass needs room; too close he looks again later; identification takes seconds") {
    const AirWarData d = LoadOpp("rebels");
    const TuningTable& t = d.tuning;
    // 2.5 s of decision, roll-in and roll-out at 200 m/s, and the 1,115 m open-fire range.
    SC_EXPECT_NEAR(MinimumRunM(200.0, 1115.0, t), 2.5 * 200.0 + 1115.0, 1e-9);
    // Never under the 3,000 ft minimum release (nwc_tp6005_1978).
    SC_EXPECT_NEAR(MinimumRunM(0.0, 300.0, t), 914.4, 1e-9);
    OppSituation s = Seen(MissionKind::Strike, OppTarget::Column, 0.5);  // the rebels attack what they find
    s.range_m = 1500.0;
    const OppDecision dec = RunChain(d.faction.opportunity, s, OppLeader(), t, 3, 1, 1);
    SC_EXPECT(dec.may && !dec.able && dec.too_close && dec.stopped == OppStep::Able);
    // Identification: centred on 4.5 s (fm_100_2_1_1984: 3-6 s), sooner for a skilled pilot.
    OppLeader good;
    good.skill = 1.0;
    OppLeader poor;
    poor.skill = 0.0;
    SC_EXPECT(IdentifyTimeS(s, good, 0.0, t) < IdentifyTimeS(s, poor, 0.0, t));
    SC_EXPECT(IdentifyTimeS(s, OppLeader(), 0.0, t) > 3.0 && IdentifyTimeS(s, OppLeader(), 0.0, t) < 6.0);
    s.range_m = 8000.0;
    s.watched_s = 0.5;
    const OppDecision early = RunChain(d.faction.opportunity, s, OppLeader(), t, 3, 1, 1);
    SC_EXPECT(early.stopped == OppStep::Identified && early.act == OppAct::Ignore);
    // The same keys give the same decision.
    s.watched_s = 60.0;
    const OppDecision a = RunChain(d.faction.opportunity, s, OppLeader(), t, 77, 4, 9);
    const OppDecision b = RunChain(d.faction.opportunity, s, OppLeader(), t, 77, 4, 9);
    SC_EXPECT(a.act == b.act && a.identify_s == b.identify_s && a.misidentified == b.misidentified);
}

SC_TEST("opportunity: a bold leader acts more readily than a rigid one; worth falls with the guns seen around it") {
    const AirWarData d = LoadOpp("nato");
    const OpportunityRulesDef& rules = d.faction.opportunity;
    OppSituation s = Seen(MissionKind::Strike, OppTarget::Column, 0.45);
    OppLeader bold;
    bold.traits.boldness = 0.9;
    bold.traits.ambition = 0.9;
    bold.traits.rigidity = 0.1;
    OppLeader rigid;
    rigid.traits.boldness = 0.1;
    rigid.traits.ambition = 0.1;
    rigid.traits.rigidity = 0.9;
    rigid.by_the_book = true;
    const double hi = ActShare(rules, s, bold, d.tuning);
    const double lo = ActShare(rules, s, rigid, d.tuning);
    std::printf("    act share: bold %.2f, rigid %.2f\n", hi, lo);
    SC_EXPECT(hi > lo + 0.2);
    // The F-111's crews act on about half the faction's tendency, the A-10's on 1.4 times it.
    s.ease = 0.0;
    SC_EXPECT_NEAR(ActShare(rules, s, OppLeader(), d.tuning), 0.4 * rules.tendency, 1e-9);
    s.ease = 0.8;
    const double calm = OppWorth(s, OppLeader(), d.tuning);
    s.threat = 1.0;
    const double hot = OppWorth(s, OppLeader(), d.tuning);
    SC_EXPECT(hot < calm);
    s.passes = 2;  // each re-attack faces the guns again (opportunity.reattack_risk)
    SC_EXPECT(OppWorth(s, OppLeader(), d.tuning) < hot);
}

SC_TEST("opportunity: claims and re-attacks") {
    const AirWarData d = LoadOpp("nato");
    const TuningTable& t = d.tuning;
    // A miss is claimed a kill at claim_on_miss_day (0.25) by day, 0.6 at night; a kill claimed more at claim_extra_*.
    SC_EXPECT(Claim(0, false, 0.2, 0.9, t) == 1);
    SC_EXPECT(Claim(0, false, 0.3, 0.9, t) == 0);
    SC_EXPECT(Claim(0, true, 0.5, 0.9, t) == 1);
    SC_EXPECT(Claim(1, false, 0.9, 0.2, t) == 2);
    SC_EXPECT(Claim(1, false, 0.9, 0.5, t) == 1);
    SC_EXPECT(Claim(1, true, 0.9, 0.9, t) == 2);
    // The mean claim over many passes is above the truth (afmag_2000_kosovo, design reference).
    int claimed = 0, kills = 0;
    for (int i = 0; i < 1000; ++i) {
        const int k = OppDraw(1, 1, static_cast<std::uint64_t>(i), 9, 0) < 0.5 ? 1 : 0;
        kills += k;
        claimed += Claim(k, false, OppDraw(1, 1, static_cast<std::uint64_t>(i), 9, 1), OppDraw(1, 1, static_cast<std::uint64_t>(i), 9, 2), t);
    }
    std::printf("    1000 passes by day: %d kills, %d claimed\n", kills, claimed);
    SC_EXPECT(claimed > kills);
    const OpportunityRulesDef& rules = d.faction.opportunity;
    OppSituation s = Seen(MissionKind::Strike, OppTarget::Column, 0.8);
    s.passes = 1;
    OppLeader ld;
    SC_EXPECT(Reattack(rules, s, ld, t, 0.0));
    SC_EXPECT(!Reattack(rules, s, ld, t, 0.99));
    s.passes = static_cast<int>(t.Get("opportunity.max_passes"));
    SC_EXPECT(!Reattack(rules, s, ld, t, 0.0));
    s.passes = 1;
    s.believed_hit = true;
    SC_EXPECT(!Reattack(rules, s, ld, t, 0.0));
    ld.chases_kills = true;
    SC_EXPECT(Reattack(rules, s, ld, t, 0.0));
    s.has_gun = false;
    SC_EXPECT(!Reattack(rules, s, ld, t, 0.0));
}

SC_TEST("opportunity: an attack pass opens fire at the weapon's slant range near his nose and recovers past the aim") {
    const AirWarData d = LoadOpp("nato");
    const GunPassDef pass = d.Weapon("gau8")->pass;
    AttackPass ap;
    const Vec3 aim{0.0, 0.0, 0.0};
    Vec3 pos{0.0, -8000.0, 60.0};
    const Vec3 vel{0.0, 200.0, 0.0};
    ap.Begin(pos, vel, aim, pass, d.tuning.Get("opportunity.run_in_m"), d.tuning.Get("opportunity.recover_m"), 0.0, 0.0);
    SC_EXPECT(ap.GetPhase() == AttackPass::Phase::Run);
    SC_EXPECT(ap.Override(0.0).kind == PilotOverride::Kind::Attack);
    double fired_at = -1.0;
    for (int i = 1; i < 1000 && fired_at < 0.0; ++i) {
        pos = pos + vel * 0.1;
        if (ap.Step(pos, vel, aim, false, 0.1 * i)) fired_at = (aim - pos).Length();
    }
    std::printf("    opened fire %.0f m from the aim\n", fired_at);
    SC_EXPECT(fired_at <= 1115.0 && fired_at > 1115.0 - 20.0 - 1e-6);
    SC_EXPECT(ap.Fired() && ap.GetPhase() == AttackPass::Phase::Recover);
    const PilotOverride rec = ap.Override(30.0);
    SC_EXPECT(rec.point.y > 0.0 && rec.after == PilotOverride::After::Rejoin);
    ap.Step(rec.point, vel, aim, true, 40.0);
    SC_EXPECT(ap.Done());
    // Off his nose (the aim 90 deg to the side): no fire, even within range.
    AttackPass side;
    side.Begin(Vec3{-8000.0, 0.0, 60.0}, Vec3{200.0, 0.0, 0.0}, aim, pass, 6000.0, 2500.0, 0.0, 0.0);
    SC_EXPECT(!side.Gate(Vec3{0.0, -800.0, 60.0}, Vec3{200.0, 0.0, 0.0}, aim));
    // A pass from too near sets up out at run_in_m first.
    AttackPass near_aim;
    near_aim.Begin(Vec3{0.0, -2000.0, 60.0}, vel, aim, pass, 6000.0, 2500.0, 0.0, 0.0);
    SC_EXPECT(near_aim.GetPhase() == AttackPass::Phase::Setup);
    SC_EXPECT(near_aim.Override(0.0).point.y < -5000.0);
}

SC_TEST("opportunity: a GAU-8 pass puts 0.8 of the tanks it hits out of action, fewer moving and under fire (Array 20)") {
    const AirWarData d = LoadOpp("nato");
    const GunPassDef pass = d.Weapon("gau8")->pass;
    const TuningTable& t = d.tuning;
    SC_EXPECT_NEAR(GunPassEffect(pass, TargetClass::Armour, false, false, false, t).kill, 0.8, 1e-12);
    SC_EXPECT_NEAR(GunPassEffect(pass, TargetClass::Armour, true, false, false, t).kill, 0.4, 1e-12);
    SC_EXPECT_NEAR(GunPassEffect(pass, TargetClass::Armour, true, true, false, t).kill, 0.8 * 0.5 * t.Get("opportunity.gun_burst_under_fire"), 1e-12);
    SC_EXPECT(GunPassEffect(pass, TargetClass::Armour, false, false, true, t).kill < 0.8);
    SC_EXPECT(GunPassEffect(pass, TargetClass::Soft, false, false, false, t).kill >= 0.8);
    SC_EXPECT_NEAR(GunPassEffect(pass, TargetClass::Armour, false, false, false, t).catastrophic, 0.375, 1e-12);
    // Ten passes on ten known stationary tanks: about eight out of action.
    int out = 0;
    for (int i = 0; i < 10; ++i) {
        out += OppDraw(11, 1, static_cast<std::uint64_t>(i), 40, 0) < GunPassEffect(pass, TargetClass::Armour, false, false, false, t).kill ? 1 : 0;
    }
    SC_EXPECT(out >= 5 && out <= 10);
}

// The A-10s' planned GAU-8 attack on the P-40 post flown through the bridge as passes: each begins at run_in_m out, opens
// fire within the gun's slant range of the aim, and its first burst is its release.
SC_TEST("opportunity (bridge): two A-10s fly their planned GAU-8 attack as passes and open fire within 1,115 m of the aim") {
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    AirCampaign c;
    SC_EXPECT(c.Init(d, 3u));
    const TuningTable& t = c.Data().tuning;
    const Vec3 post = c.Truth()[static_cast<std::size_t>(c.Data().theatre.IndexOf("kesmark_p40"))].pos;
    const Vec3 from = post + Vec3{38000.0, 46000.0, 0.0};
    const Vec3 dir = Vec3{post.x - from.x, post.y - from.y, 0.0}.Normalized();
    const Vec3 side{-dir.y, dir.x, 0.0};
    Package p;
    p.id = 96;
    p.kind = MissionKind::Strike;
    p.part = DayPart::Day;
    p.target_object = "kesmark_p40";
    p.target_label = "the P-40 post";
    p.entry = "W3";
    p.exit = "W3";
    p.tot_h = 2.0;
    Flight fl;
    fl.callsign = "Hog 10";
    fl.role = FlightRole::Strike;
    fl.aircraft = "a10a";
    fl.count = 2;
    fl.profile = Profile::Low;
    fl.weapon = "gau8";
    fl.weapons = 2;
    fl.delivery = Delivery::Strafe;
    const double agl = 100.0;
    const double ipd = 20000.0;
    std::vector<RoutePoint> r;
    r.push_back({from, agl, 0.0, "entry"});
    r.push_back({post - dir * ipd + side * (0.25 * ipd), agl, 0.0, "ip"});
    const double rs = t.Get("delivery.release_strafe_m");
    const std::size_t ri = r.size();
    r.push_back({post - dir * rs, agl, 0.0, "target"});
    r.push_back({post + side * 12000.0 + dir * 4000.0, agl, 0.0, "egress"});
    r.push_back({from, agl, 0.0, "exit"});
    TimeRoute(r, ri, 2.0, 180.0);
    fl.route = r;
    fl.release_index = static_cast<int>(ri);
    fl.aim_pos = post;
    p.flights = {fl};
    RaidScenarioBuilder builder;
    const RaidScenario built = builder.Build(c, p);
    SC_EXPECT(built.ok);
    if (!built.ok) return;
    SC_EXPECT(!built.manifest.leaders.empty());
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(built.json_text, kData, def, console, &e));
    def.has_crew_data = false;
    for (ScenarioTarget& pilot : def.targets) pilot.rwr.clear();
    ScenarioRunner runner(std::move(def), std::move(console));
    MissionBridge bridge(built.manifest, c.Data(), 3u);
    SC_EXPECT(bridge.Opportunities().On());
    SC_EXPECT(bridge.Opportunities().PlannedPass(0) && bridge.Opportunities().PlannedPass(1));
    bool attack_flown = false;
    std::vector<std::string> fired_by;
    while (!runner.Finished() && !bridge.UnitDestroyed() && runner.Time() < built.manifest.end_s + 60.0 && fired_by.size() < 2) {
        runner.Step(0.1);
        bridge.Step(runner);
        for (const RaidAircraft& a : built.manifest.aircraft) {
            if (const PilotOverride* ov = runner.PassOf(a.target_id)) attack_flown |= ov->kind == PilotOverride::Kind::Attack;
        }
        fired_by.clear();
        for (const fx::Event& ev : OfKindOpp(runner.Fx().Since(0), fx::EventKind::WeaponRelease)) {
            if (std::find(fired_by.begin(), fired_by.end(), ev.source) == fired_by.end()) fired_by.push_back(ev.source);
        }
    }
    SC_EXPECT(attack_flown);
    SC_EXPECT(fired_by.size() == 2);
    for (const fx::Event& ev : OfKindOpp(runner.Fx().Since(0), fx::EventKind::WeaponRelease)) {
        const RaidAircraft* a = nullptr;
        for (const RaidAircraft& m : built.manifest.aircraft) {
            if (m.target_id == ev.source) a = &m;
        }
        SC_EXPECT(a != nullptr);
        if (a == nullptr) continue;
        const double slant = (runner.GroundEnu(a->aim_enu.x, a->aim_enu.y) - ev.position).Length();
        std::printf("    %s %s %s: opened fire %.0f m from the aim, %d rounds\n", ev.source.c_str(), ev.weapon.c_str(), ev.delivery.c_str(), slant,
                    ev.number);
        SC_EXPECT(ev.delivery == "strafe" && ev.weapon == "gau8");
        SC_EXPECT(slant <= 1115.0 + 0.1 * 250.0 + 1.0);
        SC_EXPECT(ev.number == 135);
    }
    SC_EXPECT(OfKindOpp(runner.Fx().Since(0), fx::EventKind::WeaponImpact).size() >= 2);
}

// A Pact strike flight on its way to the P-40 post passes low by our battery: its leader's chain on what his pilots see
// ends in a report (fm_100_2_1_1984: a flight on another task reports and requests), said at once on the flight's net and
// queued for the posts; the outcome's record holds it.
SC_TEST("opportunity (bridge): a Pact strike flight passing our battery reports it in flight and does not attack it") {
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", d, &e));
    AirCampaign c;
    SC_EXPECT(c.Init(d, 4u));
    const Vec3 post = c.Truth()[static_cast<std::size_t>(c.Data().theatre.IndexOf("kesmark_p40"))].pos;
    const Vec3 bat = c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos;
    const Vec3 dir = Vec3{post.x - bat.x, post.y - bat.y, 0.0}.Normalized();
    const Vec3 side{-dir.y, dir.x, 0.0};
    Package p;
    p.id = 97;
    p.kind = MissionKind::Strike;
    p.part = DayPart::Day;
    p.target_object = "kesmark_p40";
    p.target_label = "the P-40 post";
    p.entry = "W3";
    p.exit = "W3";
    p.tot_h = 8.0;  // 12:00 local: by day by the dated theatre's sun (roadmap S8), as its part says
    Flight fl;
    fl.callsign = "Rook 30";
    fl.role = FlightRole::Strike;
    fl.aircraft = "su25";
    fl.count = 2;
    fl.profile = Profile::Low;
    fl.weapon = "fab250";
    fl.weapons = 4;
    fl.delivery = Delivery::PopUp;
    const double agl = 150.0;
    std::vector<RoutePoint> r;
    r.push_back({bat - dir * 30000.0 + side * 1200.0, agl, 0.0, "entry"});
    r.push_back({bat + side * 1200.0, agl, 0.0, "ip"});
    const std::size_t ri = r.size();
    r.push_back({post - dir * 1500.0, agl, 0.0, "target"});
    r.push_back({post + side * 15000.0, agl, 0.0, "egress"});
    r.push_back({post + side * 15000.0 - dir * 40000.0, agl, 0.0, "exit"});
    TimeRoute(r, ri, 8.0, 220.0);
    fl.route = r;
    fl.release_index = static_cast<int>(ri);
    fl.aim_pos = post;
    p.flights = {fl};
    RaidScenarioBuilder builder;
    const RaidScenario built = builder.Build(c, p);
    SC_EXPECT(built.ok);
    if (!built.ok) return;
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(built.json_text, kData, def, console, &e));
    def.has_crew_data = false;
    for (ScenarioTarget& pilot : def.targets) pilot.rwr.clear();
    ScenarioRunner runner(std::move(def), std::move(console));
    MissionBridge bridge(built.manifest, c.Data(), 4u);
    std::vector<OpportunityRun::Report> reports;
    while (!runner.Finished() && !bridge.UnitDestroyed() && runner.Time() < built.manifest.end_s + 60.0) {
        runner.Step(0.1);
        bridge.Step(runner);
        for (const OpportunityRun::Report& rp : bridge.TakeOpportunityReports()) reports.push_back(rp);
    }
    const std::vector<PackageOutcome::OpportunityRecord>& recs = bridge.Opportunities().Records();
    for (const PackageOutcome::OpportunityRecord& rec : recs) {
        std::printf("    %s: %s at %.0f m from the battery, stopped at %s (%s)%s\n", rec.believed.c_str(), rec.act.c_str(),
                    std::hypot(rec.east_m - built.manifest.own_enu.x, rec.north_m - built.manifest.own_enu.y), rec.stopped.c_str(), rec.why.c_str(),
                    rec.reported_in_flight ? ", reported in flight" : "");
    }
    bool battery_reported = false;
    for (const OpportunityRun::Report& rp : reports) {
        battery_reported |= Dist2d(rp.where, built.manifest.own_enu) < 2000.0;
    }
    SC_EXPECT(!recs.empty());
    SC_EXPECT(battery_reported);
    for (const PackageOutcome::OpportunityRecord& rec : recs) SC_EXPECT(rec.passes == 0 && rec.act != "attack" && rec.act != "gun_run");
    SC_EXPECT(!bridge.UnitDestroyed());
}

SC_TEST("opportunity: the records are saved and read back; a save from before B6 replays without them") {
    PackageOutcome o;
    o.package_id = 4;
    PackageOutcome::OpportunityRecord r;
    r.flight = 1;
    r.t_h = 12.5;
    r.believed = "battery";
    r.east_m = 1234.0;
    r.north_m = -567.0;
    r.act = "gun_run";
    r.stopped = "act";
    r.why = "it fired on the flight";
    r.reported_in_flight = true;
    r.passes = 2;
    r.claimed = 3;
    r.object = "battery_21";
    r.truth = "dummy:0";
    r.kills = 1;
    r.misidentified = true;
    o.opportunities.push_back(r);
    save_detail::Reader rd;
    const PackageOutcome back = rd.Outcome(save_detail::OutcomeJson(o));
    SC_EXPECT(back.opportunities.size() == 1);
    if (back.opportunities.size() == 1) {
        const PackageOutcome::OpportunityRecord& b = back.opportunities[0];
        SC_EXPECT(b.flight == 1 && b.believed == "battery" && b.act == "gun_run" && b.stopped == "act" && b.why == r.why);
        SC_EXPECT_NEAR(b.t_h, 12.5, 1e-9);
        SC_EXPECT_NEAR(b.east_m, 1234.0, 1e-9);
        SC_EXPECT_NEAR(b.north_m, -567.0, 1e-9);
        SC_EXPECT(b.reported_in_flight && b.passes == 2 && b.claimed == 3 && b.object == "battery_21" && b.truth == "dummy:0" && b.kills == 1 &&
                  b.misidentified);
    }
    // An outcome without them writes no key.
    PackageOutcome none;
    SC_EXPECT(save_detail::OutcomeJson(none).Find("opportunities") == nullptr);
    // A campaign saved before B6 has no opportunity_mode: it loads with mode 0.
    const AirWarData d = LoadOpp("warsaw_pact");
    AirCampaign c;
    c.Init(d, 5u);
    if (c.Advance(12.0)) c.SkipPlayerMission();
    std::string text = SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json");
    const std::size_t at = text.find("\"opportunity_mode\"");
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    const std::size_t end = text.find_first_of(",}", at);
    if (text[end] == ',') {
        text.erase(at, end - at + 1);
    } else {
        const std::size_t comma = text.rfind(',', at);
        text.erase(comma, end - comma);
    }
    AirCampaign old;
    std::string e;
    SC_EXPECT(LoadCampaignJson(text, kData, old, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT_NEAR(old.Data().tuning.Get("opportunity.mode"), 0.0, 1e-12);
    AirCampaign now;
    SC_EXPECT(LoadCampaignJson(SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json"), kData, now, &e));
    SC_EXPECT_NEAR(now.Data().tuning.Get("opportunity.mode"), 1.0, 1e-12);
}

// The abstract level (resolve.hpp FindOpportunity): with no finds it runs as before B6, its draws keyed apart from the
// resolver's own stream; with them, flights find columns on the move and attack or report them.
SC_TEST("opportunity (A0): with no finds a campaign runs as before B6; with them its flights find columns on the move") {
    int total = 0;
    for (const char* f : {"warsaw_pact", "nato", "neutral", "rebels"}) {
        AirWarData d0 = LoadOpp(f);
        d0.tuning.values["opportunity.mode"] = 0.0;
        AirWarData d1 = LoadOpp(f);
        d1.tuning.values["opportunity.a0_find_scale"] = 0.0;
        const AirCampaign a = RunDays(d0, 7u, 5.0);
        const AirCampaign b = RunDays(d1, 7u, 5.0);
        SC_EXPECT(JournalText(a) == JournalText(b));
        // A find is rare (a column on the road under a flight's route): the seeds are tried until one campaign has one,
        // so the property is tested, not one seed's course (B8's air battle changes every campaign's course).
        int found = 0;
        for (unsigned seed = 7u; seed < 12u && total + found == 0; ++seed) {
            const AirCampaign on = RunDays(LoadOpp(f), seed, 5.0);
            for (const JournalEntry& j : on.Journal()) {
                std::size_t pos = 0;
                while ((pos = j.text.find("on its way and attacked it", pos)) != std::string::npos) {
                    ++found;
                    ++pos;
                }
            }
            std::printf("    %s, seed %u: %d columns attacked on the way in five days\n", f, seed, found);
        }
        total += found;
    }
    SC_EXPECT(total > 0);
}
