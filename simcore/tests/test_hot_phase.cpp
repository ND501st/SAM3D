// The hot phase (roadmap B11, docs/air_battle/10_FACTIONS.md): each faction's and our side's values for how a flight
// chooses in the air, with their ranges and sources; the officer's and pilot's shifts and their bounds; the gates and
// latencies; the two new quirks; and section 9's changes to NATO (medium altitude only over broken defences, interdiction
// from the first day), each with the mode a save made before B11 keeps.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/hot_phase.hpp"
#include "simcore/command/own_air.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

AirWarData LoadHot(const std::string& faction) {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

std::string ReadText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// An officer exactly at his faction's pool means, unstressed: he moves nothing.
Officer AverageOfficer(const FactionDef& f) {
    Officer o;
    o.def.traits = f.pool.trait_mean;
    o.def.skills = f.pool.skill_mean;
    o.stress = 0.0;
    return o;
}

// The most extreme officer one way or the other: every trait at 0 or 1, airmanship too, and the quirks that push.
Officer ExtremeOfficer(const AirWarData& d, bool high) {
    Officer o;
    const double v = high ? 1.0 : 0.0, w = high ? 0.0 : 1.0;
    o.def.traits.boldness = v;
    o.def.traits.ambition = v;
    o.def.traits.creativity = v;
    o.def.traits.vindictiveness = v;
    o.def.traits.methodical = w;
    o.def.traits.rigidity = w;
    o.def.traits.composure = w;
    o.def.traits.skepticism = v;
    o.def.skills.airmanship = v;
    o.stress = 0.0;
    for (const char* q : high ? std::vector<const char*>{"press_on", "gambler", "opportunist", "grudge_holder", "chatterbox", "loose_lips"}
                              : std::vector<const char*>{"loss_shy", "by_the_book", "controller_bound", "silent_runner", "punctual"}) {
        if (const QuirkDef* qd = d.Quirk(q)) o.quirks.push_back(*qd);
    }
    return o;
}

std::string Without(std::string text, const std::string& what, const std::string& with) {
    const std::size_t at = text.find(what);
    if (at != std::string::npos) text.replace(at, what.size(), with);
    return text;
}

}  // namespace

SC_TEST("hot phase (B11): the four factions and our side load it with a value, range, source and confidence per key") {
    for (const char* f : {"rebels", "neutral", "warsaw_pact", "nato"}) {
        const AirWarData d = LoadHot(f);
        SC_EXPECT(d.faction.hot.present);
        for (int i = 0; i < kHotParams; ++i) {
            const HotValue& v = d.faction.hot.Value(static_cast<HotParam>(i));
            SC_EXPECT(v.set && !v.source.empty() && !v.confidence.empty());
            SC_EXPECT(v.lo <= v.value && v.value <= v.hi);
        }
        SC_EXPECT(d.faction.hot.reaction_delay.set && d.faction.hot.on_control_lost.set && d.faction.hot.authority.set);
        // The values the earlier blocks read are written from it.
        SC_EXPECT_NEAR(d.faction.opportunity.tendency, d.faction.hot.Value(HotParam::Opportunity).value, 1e-12);
        SC_EXPECT_NEAR(d.faction.radio.discipline, d.faction.hot.Value(HotParam::RadioDiscipline).value, 1e-12);
        // And the air battle's doctrine agrees with it (checked on load).
        const AirDoctrineDef* doc = d.air_combat.Doctrine(f);
        SC_EXPECT(doc != nullptr);
        if (doc != nullptr) {
            SC_EXPECT_NEAR(doc->skill, d.faction.hot.Value(HotParam::AirCombat).value, 1e-12);
            SC_EXPECT(doc->control_lost_green == d.faction.hot.on_control_lost.green);
            SC_EXPECT(doc->control_lost_skilled == d.faction.hot.on_control_lost.skilled);
        }
    }
    const AirWarData pact = LoadHot("warsaw_pact");
    SC_EXPECT_NEAR(pact.faction.radio.discipline_gci_fighters, 0.2, 1e-12);
    SC_EXPECT(pact.faction.c2.alert.size() == 3 && pact.faction.c2.free_hunt.from_phase == 1);
    SC_EXPECT_NEAR(pact.faction.c2.free_hunt.depth_km, 30.0, 1e-12);

    command::OwnAirDef ours;
    std::string e;
    SC_EXPECT(command::LoadOwnAirFile(kData + "/command/own_air.json", ours, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(ours.hot.present && ours.hot.alert.empty());
    const AirDoctrineDef* od = pact.air_combat.Doctrine("ours");
    SC_EXPECT(od != nullptr);
    if (od != nullptr) {
        SC_EXPECT_NEAR(od->skill, ours.hot.Value(HotParam::AirCombat).value, 1e-12);
        SC_EXPECT(od->control_lost_green == ours.hot.on_control_lost.green && od->control_lost_skilled == ours.hot.on_control_lost.skilled);
    }
    SC_EXPECT_NEAR(ours.hot.Value(HotParam::TurnBack).For(FlightRole::Cap, true), 0.6, 1e-12);
    SC_EXPECT_NEAR(ours.hot.Value(HotParam::TurnBack).For(FlightRole::Cap, false), 0.2, 1e-12);
}

SC_TEST("hot phase (B11): an unknown key, a missing source, a value outside its range or a parked key fails the load") {
    const std::string text = ReadText(kData + "/air_war/faction_warsaw_pact.json");
    FactionDef f;
    std::string e;
    SC_EXPECT(LoadFactionJson(text, f, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const std::string ini = R"("initiative": {"value": 0.25, "range": [0.1, 0.4], "source": )";
    SC_EXPECT(text.find(ini) != std::string::npos);
    const std::vector<std::string> bad = {
        Without(text, R"("hot_phase": {)", R"("hot_phase": { "boldness": 0.5,)"),
        Without(text, ini, R"("initiative": {"value": 0.25, "range": [0.1, 0.4], "notes": )"),
        Without(text, ini, R"("initiative": {"value": 0.45, "range": [0.1, 0.4], "source": )"),
        Without(text, ini, R"("initiative": {"value": 0.25, "range": [0.1, 1.4], "source": )"),
        Without(text, R"("hot_phase": {)", R"("hot_phase_none": {)"),
        // The parked keys live in hot_phase only.
        Without(text, R"("have_quick_share": 0.0,)", R"("have_quick_share": 0.0, "discipline": 0.8,)"),
        Without(text, R"("report_in_flight": {)", R"("tendency": {"value": 0.2, "source": "x", "confidence": "low"}, "report_in_flight": {)"),
        // The gates must agree with the c2 block's authority table.
        Without(text, R"("retask_by": ["fac", "tasking", "squadron"])", R"("retask_by": ["fac", "tasking"])"),
        Without(text, R"("free_hunt_from_phase": 1,)", R"("free_hunt_from_phase": -1,)"),
    };
    for (std::size_t i = 0; i < bad.size(); ++i) {
        SC_EXPECT(bad[i] != text);
        FactionDef g;
        std::string why;
        const bool ok = LoadFactionJson(bad[i], g, &why);
        if (ok) std::printf("    case %zu loaded\n", i);
        SC_EXPECT(!ok && !why.empty());
    }
}

SC_TEST("hot phase (B11): the orders of section 7.2 hold for the files as shipped") {
    const AirWarData reb = LoadHot("rebels"), neu = LoadHot("neutral"), pact = LoadHot("warsaw_pact"), nato = LoadHot("nato");
    const auto v = [](const AirWarData& d, HotParam p) { return d.faction.hot.Value(p).value; };
    SC_EXPECT(v(pact, HotParam::PlanAdherence) > v(nato, HotParam::PlanAdherence));
    SC_EXPECT(v(nato, HotParam::PlanAdherence) > v(neu, HotParam::PlanAdherence));
    SC_EXPECT(v(neu, HotParam::PlanAdherence) > v(reb, HotParam::PlanAdherence));
    SC_EXPECT(v(reb, HotParam::Initiative) > v(nato, HotParam::Initiative));
    SC_EXPECT(v(nato, HotParam::Initiative) > v(neu, HotParam::Initiative));
    SC_EXPECT(v(neu, HotParam::Initiative) > v(pact, HotParam::Initiative));
    SC_EXPECT(v(nato, HotParam::AirCombat) > v(pact, HotParam::AirCombat));
    SC_EXPECT_NEAR(v(pact, HotParam::AirCombat), v(neu, HotParam::AirCombat), 1e-12);
    SC_EXPECT(v(neu, HotParam::AirCombat) > v(reb, HotParam::AirCombat));
    SC_EXPECT(v(neu, HotParam::Husbanding) > v(reb, HotParam::Husbanding) && v(reb, HotParam::Husbanding) > v(nato, HotParam::Husbanding));
    SC_EXPECT(v(nato, HotParam::Husbanding) > v(pact, HotParam::Husbanding));
    // The Pact's escort without its controller: less press-on, more turn-back; a strike flight's are its own.
    const HotValue& po = pact.faction.hot.Value(HotParam::PressOn);
    const HotValue& tb = pact.faction.hot.Value(HotParam::TurnBack);
    SC_EXPECT_NEAR(po.For(FlightRole::Escort, true), 0.4, 1e-12);
    SC_EXPECT_NEAR(po.For(FlightRole::Escort, false), 0.6, 1e-12);
    SC_EXPECT_NEAR(tb.For(FlightRole::Escort, true), 0.6, 1e-12);
    SC_EXPECT_NEAR(tb.For(FlightRole::Strike, true), 0.2, 1e-12);
    // Fighters talk; strikers keep silence.
    for (const AirWarData* d : {&neu, &pact, &nato}) {
        const HotValue& rd = d->faction.hot.Value(HotParam::RadioDiscipline);
        SC_EXPECT(rd.For(FlightRole::Escort, false) < rd.For(FlightRole::Strike, false));
    }
}

SC_TEST("hot phase (B11): an average officer moves nothing; the extremes stay within 0.2, 0.15 and 0.35 together") {
    for (const char* f : {"rebels", "neutral", "warsaw_pact", "nato"}) {
        const AirWarData d = LoadHot(f);
        const FactionDef& fd = d.faction;
        const Officer avg = AverageOfficer(fd);
        const HotPilot plain;
        const Officer hi = ExtremeOfficer(d, true), lo = ExtremeOfficer(d, false);
        HotPilot ace;
        ace.combat_sorties = 40;
        ace.currency = 1.2;
        ace.nerve = 1.0;
        HotPilot rookie;
        rookie.combat_sorties = 0;
        rookie.currency = 0.2;
        rookie.nerve = 0.0;
        for (int i = 0; i < kHotParams; ++i) {
            const HotParam p = static_cast<HotParam>(i);
            for (FlightRole role : {FlightRole::Strike, FlightRole::Escort, FlightRole::Sead}) {
                for (bool lost : {false, true}) {
                    const double base = Clamp01(fd.hot.Value(p).For(role, lost));
                    HotWho w;
                    w.leader = &avg;
                    w.pilot = &plain;
                    w.control_lost = lost;
                    SC_EXPECT_NEAR(HotPhase(fd, p, role, fd.pool, w), base, 1e-12);
                    for (const Officer* o : {&hi, &lo}) {
                        for (bool grudge : {false, true}) {
                            SC_EXPECT(std::fabs(OfficerShift(p, fd.pool, *o, lost, grudge)) <= kHotOfficerBound + 1e-12);
                            for (const HotPilot* pl : {&ace, &rookie}) {
                                SC_EXPECT(std::fabs(PilotShift(p, *pl)) <= kHotPilotBound + 1e-12);
                                HotWho x;
                                x.leader = o;
                                x.pilot = pl;
                                x.control_lost = lost;
                                x.grudge_target = grudge;
                                SC_EXPECT(std::fabs(HotPhase(fd, p, role, fd.pool, x) - base) <= 0.35 + 1e-12);
                                // A leader who does not fly at half weight.
                                x.leader_flies = false;
                                SC_EXPECT(std::fabs(HotPhase(fd, p, role, fd.pool, x) - base) <= 0.25 + 1e-12);
                            }
                        }
                    }
                }
            }
        }
        // The bounds are reached: the extremes are extreme.
        SC_EXPECT(OfficerShift(HotParam::PressOn, fd.pool, hi) > 0.1 && OfficerShift(HotParam::PressOn, fd.pool, lo) < -0.1);
        SC_EXPECT(PilotShift(HotParam::AirCombat, ace) > 0.1);
        // The debrief's line names the extremes' shifts and none of the average's.
        SC_EXPECT(HotShiftsLine(fd, avg).empty() && HotShiftsLine(fd, hi).find("press_on +") != std::string::npos);
    }
    // The quirks of 8.4-8.5 move what they say.
    const AirWarData d = LoadHot("warsaw_pact");
    Officer bound = AverageOfficer(d.faction);
    bound.quirks.push_back(*d.Quirk("controller_bound"));
    SC_EXPECT_NEAR(OfficerShift(HotParam::Initiative, d.faction.pool, bound), -0.1, 1e-12);
    SC_EXPECT_NEAR(OfficerShift(HotParam::TurnBack, d.faction.pool, bound, true), 0.15, 1e-12);
    SC_EXPECT_NEAR(OfficerShift(HotParam::TurnBack, d.faction.pool, bound, false), 0.0, 1e-12);
    Officer opp = AverageOfficer(d.faction);
    opp.quirks.push_back(*d.Quirk("opportunist"));
    SC_EXPECT_NEAR(OfficerShift(HotParam::Opportunity, d.faction.pool, opp), 0.1, 1e-12);
    Officer lips = AverageOfficer(d.faction);
    lips.quirks.push_back(*d.Quirk("loose_lips"));
    SC_EXPECT_NEAR(OfficerShift(HotParam::RadioDiscipline, d.faction.pool, lips), -0.2, 1e-12);
    // His shifts reach a played flight's reaction rows (reactions.hpp ShiftedRow).
    ReactionRow row;
    row.press_on = 0.5;
    row.abort = 0.4;
    row.jettison = 0.3;
    const ReactionRow moved = ShiftedRow(row, 0.1, -0.05);
    SC_EXPECT_NEAR(moved.press_on, 0.6, 1e-12);
    SC_EXPECT_NEAR(moved.abort, 0.3, 1e-12);
    SC_EXPECT_NEAR(moved.jettison, 0.25, 1e-12);
}

SC_TEST("hot phase (B11): no officer opens a gate; the Pact's free hunt waits for its second phase") {
    const AirWarData pact = LoadHot("warsaw_pact");
    const FactionDef& f = pact.faction;
    SC_EXPECT(!BeyondTaskOffered(f, MissionKind::Strike) && !BeyondTaskOffered(f, MissionKind::Cas));
    SC_EXPECT(BeyondTaskOffered(f, MissionKind::ArmedRecce));
    SC_EXPECT(!FreeHuntOffered(f, MissionKind::ArmedRecce, 0, true));
    SC_EXPECT(FreeHuntOffered(f, MissionKind::ArmedRecce, 1, true));
    SC_EXPECT(!FreeHuntOffered(f, MissionKind::ArmedRecce, 1, false));
    SC_EXPECT(!FreeHuntOffered(f, MissionKind::Strike, 2, true));
    // A strike led by the boldest, most creative leader reports what it finds and attacks nothing.
    OppSituation s;
    s.mission = MissionKind::Strike;
    s.role = FlightRole::Strike;
    s.ease = 0.45;
    s.bingo_share = 0.3;
    s.target = OppTarget::Column;
    s.confidence = 0.9;
    s.watched_s = 60.0;
    s.range_m = 8000.0;
    s.depth_km = 30.0;
    s.has_gun = true;
    s.fuel_share = 0.8;
    s.speed_mps = 200.0;
    s.release_m = 1115.0;
    OppLeader bold;
    bold.traits.boldness = 1.0;
    bold.traits.creativity = 1.0;
    bold.traits.ambition = 1.0;
    bold.traits.rigidity = 0.0;
    bold.press_on = true;
    bold.skill = 1.0;
    for (std::uint64_t k = 0; k < 300; ++k) {
        const OppDecision dec = RunChain(f.opportunity, s, bold, pact.tuning, 11, k, 1);
        SC_EXPECT(!dec.may && dec.act == OppAct::Report);
    }
    // The other factions' gates: the rebels may attack beyond any task they fly; the neutral and NATO only armed reconnaissance.
    const AirWarData reb = LoadHot("rebels"), neu = LoadHot("neutral"), nato = LoadHot("nato");
    SC_EXPECT(BeyondTaskOffered(reb.faction, MissionKind::Strike) && BeyondTaskOffered(reb.faction, MissionKind::HeloRaid));
    SC_EXPECT(!BeyondTaskOffered(neu.faction, MissionKind::Strike) && BeyondTaskOffered(neu.faction, MissionKind::ArmedRecce));
    SC_EXPECT(!BeyondTaskOffered(nato.faction, MissionKind::Strike) && BeyondTaskOffered(nato.faction, MissionKind::ArmedRecce));
    SC_EXPECT(!FreeHuntOffered(nato.faction, MissionKind::ArmedRecce, 2, true));
}

SC_TEST("hot phase (B11): the latencies of section 7.4") {
    const AirWarData reb = LoadHot("rebels"), neu = LoadHot("neutral"), pact = LoadHot("warsaw_pact"), nato = LoadHot("nato");
    const Officer rigid = ExtremeOfficer(pact, false), loose = ExtremeOfficer(pact, true);
    int pact_never = 0, pact_retasked = 0, pact_cas = 0;
    for (int i = 0; i <= 100; ++i) {
        const double u = i / 100.0, w = 1.0 - u;
        // A Pact readiness-1 launch within 180-300 s; airborne pools at once; no pool, none.
        const double r1 = AlertTakeoffS(pact.faction, "readiness_1", u);
        SC_EXPECT(r1 >= 180.0 - 1e-9 && r1 <= 300.0 + 1e-9);
        SC_EXPECT(AlertTakeoffS(pact.faction, "no_such_pool", u) < 0.0);
        // A Pact planned strike: never, or after at least 600 s (480 s for the loosest duty officer).
        const double ps = RetaskAirS(pact.faction, "strike", u, w);
        SC_EXPECT(ps < 0.0 || ps >= 600.0 - 1e-9);
        (ps < 0.0 ? pact_never : pact_retasked) += 1;
        const double po = RetaskAirS(pact.faction, "strike", u, w, &loose);
        SC_EXPECT(po < 0.0 || po >= 480.0 - 1e-9);
        const double pr = RetaskAirS(pact.faction, "strike", u, w, &rigid);
        SC_EXPECT(pr < 0.0 || pr >= ps);
        const double pc = RetaskAirS(pact.faction, "cas", u, w);
        SC_EXPECT(pc >= 600.0 - 1e-9 && pc <= 1800.0 + 1e-9);
        pact_cas += pc >= 0.0 ? 1 : 0;
        // NATO: interdiction never; close support through a FAC within 180-600 s.
        SC_EXPECT(RetaskAirS(nato.faction, "strike", u, w) < 0.0);
        const double nc = RetaskAirS(nato.faction, "cas", u, w);
        SC_EXPECT(nc >= 180.0 - 1e-9 && nc <= 600.0 + 1e-9);
        SC_EXPECT(RetaskAirS(nato.faction, "bai", u, w) >= 180.0 - 1e-9);
        // The neutral recalls only; the rebels have no net to do either.
        SC_EXPECT(RetaskAirS(neu.faction, "cas", u, w) < 0.0 && RetaskAirS(neu.faction, "armed_recce", u, w) < 0.0);
        const double nr = RecallS(neu.faction, u);
        SC_EXPECT(nr >= 300.0 - 1e-9 && nr <= 900.0 + 1e-9);
        SC_EXPECT(RetaskAirS(reb.faction, "strike", u, w) < 0.0 && RecallS(reb.faction, u) < 0.0);
    }
    SC_EXPECT(pact_never > 30 && pact_retasked > 30 && pact_cas == 101);
    // Reaction delays: the Pact's improvised manoeuvre slower than its programme; a veteran quicker.
    const OfficerPoolDef& pool = pact.faction.pool;
    HotWho none;
    const HotDelay prog = ReactionDelayS(pact.faction, pool, none, false), impro = ReactionDelayS(pact.faction, pool, none, true);
    SC_EXPECT_NEAR(prog.lo_s, 2.0, 1e-12);
    SC_EXPECT_NEAR(prog.hi_s, 5.0, 1e-12);
    SC_EXPECT_NEAR(impro.lo_s, 4.0, 1e-12);
    HotPilot vet;
    vet.combat_sorties = 20;
    HotWho v;
    v.pilot = &vet;
    SC_EXPECT_NEAR(ReactionDelayS(pact.faction, pool, v, false).hi_s, 3.5, 1e-12);
    const HotDelay green = ReactionDelayS(reb.faction, reb.faction.pool, none, false);
    SC_EXPECT(green.lo_s >= 4.0 - 1e-12 && green.hi_s <= 10.0 + 1e-12);
}

SC_TEST("hot phase (B11): the new effect keys are known; the hot quirks come from their own stream, the same for the same seed") {
    const std::vector<std::string>& k = KnownQuirkEffects();
    for (const char* key : {"opportunity", "control_bound", "radio_chatter"}) SC_EXPECT(std::find(k.begin(), k.end(), key) != k.end());
    std::vector<QuirkDef> quirks;
    std::string e;
    SC_EXPECT(LoadQuirksFile(kData + "/air_war/traits.json", quirks, &e));
    int hot = 0;
    for (const QuirkDef& q : quirks) {
        SC_EXPECT(!q.signature.empty() && !q.intel_hint.empty());
        hot += q.hot_draw ? 1 : 0;
    }
    SC_EXPECT(hot == 2);
    // The Pact's generated squadron leaders: the same seed gives the same staff; some have the controller-bound habit; under
    // mode 0 none has a hot quirk and every other quirk is as under mode 1 (the hot draw moves no other draw).
    AirWarData d1 = LoadHot("warsaw_pact");
    AirWarData d0 = LoadHot("warsaw_pact");
    d0.tuning.values["hot_phase.mode"] = 0.0;
    int bound = 0, generated = 0;
    for (unsigned seed = 1; seed <= 30; ++seed) {
        AirCampaign a, b, z;
        a.Init(d1, seed);
        b.Init(d1, seed);
        z.Init(d0, seed);
        const std::vector<Officer>& oa = a.Staff().officers;
        const std::vector<Officer>& ob = b.Staff().officers;
        const std::vector<Officer>& oz = z.Staff().officers;
        SC_EXPECT(oa.size() == ob.size() && oa.size() == oz.size());
        if (oa.size() != ob.size() || oa.size() != oz.size()) continue;
        for (std::size_t i = 0; i < oa.size(); ++i) {
            SC_EXPECT(oa[i].def.id == ob[i].def.id && oa[i].def.quirks == ob[i].def.quirks);
            std::vector<std::string> plain;
            for (const std::string& q : oa[i].def.quirks) {
                if (q != "opportunist" && q != "controller_bound") plain.push_back(q);
            }
            SC_EXPECT(plain == oz[i].def.quirks);
            for (const std::string& q : oz[i].def.quirks) SC_EXPECT(q != "opportunist" && q != "controller_bound");
            if (oa[i].def.role == StaffRole::SquadronLeader && oa[i].def.traits.boldness != 0.0) {
                ++generated;
                bound += oa[i].HasQuirk("controller_bound") ? 1 : 0;
            }
        }
    }
    SC_EXPECT(generated > 0 && bound > 0 && bound < generated);
}

SC_TEST("hot phase (B11) 9.1: NATO strike, close support and armed reconnaissance fly medium only over broken defences") {
    const AirWarData nato = LoadHot("nato");
    const DoctrineDef& doc = nato.faction.doctrine;
    for (MissionKind m : {MissionKind::Strike, MissionKind::Cas, MissionKind::ArmedRecce}) {
        const PackageTemplate* t = &doc.packages[static_cast<int>(m)];
        SC_EXPECT(t->allowed);
        SC_EXPECT(!MediumOffered(*t, 0.6, 1) && !MediumOffered(*t, 0.45, 1) && MediumOffered(*t, 0.3, 1));
        SC_EXPECT(MediumOffered(*t, 0.6, 0));  // a save made before B11
    }
    for (MissionKind m : {MissionKind::Hunter, MissionKind::Dead}) {
        const PackageTemplate* t = &doc.packages[static_cast<int>(m)];
        SC_EXPECT(t->allowed && MediumOffered(*t, 0.9, 1));
    }
    // The commander is a low-level man now; the habit is rare in a superior force (but kept for an old save).
    SC_EXPECT(nato.Quirk("medium_altitude_man")->TierWeight(Tier::Superior, 1) < 0.2);
    SC_EXPECT_NEAR(nato.Quirk("medium_altitude_man")->TierWeight(Tier::Superior, 0), 1.3, 1e-12);
    // In a campaign: while the rollback lasts on day 1, no strike, close support or armed reconnaissance flies medium.
    for (unsigned seed = 1; seed <= 4; ++seed) {
        AirCampaign c;
        c.Init(nato, seed);
        while (c.Now() < 24.0) {
            if (c.Advance(24.0)) c.SkipPlayerMission();
        }
        if (c.Politics().min_profile >= 1 || c.Intel().BelievedIadsShare() < 0.45) continue;
        for (const Package& p : c.Packages()) {
            if (p.kind == MissionKind::Strike || p.kind == MissionKind::Cas || p.kind == MissionKind::ArmedRecce) SC_EXPECT(p.profile != Profile::Medium);
        }
    }
}

SC_TEST("hot phase (B11) 9.2: NATO's first phase flies interdiction and armed reconnaissance from day 1") {
    const AirWarData nato = LoadHot("nato");
    const PhaseDef& ph = nato.faction.doctrine.phases.front();
    double sum1 = 0.0, sum0 = 0.0;
    for (int k = 0; k < static_cast<int>(MissionKind::Count); ++k) {
        sum1 += ph.Apportion(1)[k];
        sum0 += ph.Apportion(0)[k];
    }
    SC_EXPECT_NEAR(sum1, 1.0, 1e-9);
    SC_EXPECT_NEAR(sum0, 1.0, 1e-9);
    SC_EXPECT(ph.has_apportion_mode0);
    const int ar = static_cast<int>(MissionKind::ArmedRecce), st = static_cast<int>(MissionKind::Strike);
    SC_EXPECT(ph.Apportion(1)[ar] > ph.Apportion(0)[ar] && ph.Apportion(1)[st] > ph.Apportion(0)[st]);
    const double sead1 = ph.Apportion(1)[static_cast<int>(MissionKind::Dead)] + ph.Apportion(1)[static_cast<int>(MissionKind::Hunter)] +
                         ph.Apportion(1)[static_cast<int>(MissionKind::Deception)];
    SC_EXPECT_NEAR(sead1, 0.42, 1e-9);
    SC_EXPECT_NEAR(ph.until_iads, 0.45, 1e-12);
    // The other factions' phases are as before: no mode-0 shares.
    for (const char* f : {"rebels", "neutral", "warsaw_pact"}) {
        const AirWarData d = LoadHot(f);
        for (const PhaseDef& p : d.faction.doctrine.phases) SC_EXPECT(!p.has_apportion_mode0);
    }
    // Over a few seeds, NATO's first day plans armed reconnaissance or a strike beside its suppression.
    int interdiction = 0;
    for (unsigned seed = 1; seed <= 6; ++seed) {
        AirCampaign c;
        c.Init(nato, seed);
        while (c.Now() < 24.0) {
            if (c.Advance(24.0)) c.SkipPlayerMission();
        }
        for (const Package& p : c.Packages()) interdiction += p.kind == MissionKind::ArmedRecce || p.kind == MissionKind::Strike ? 1 : 0;
    }
    SC_EXPECT(interdiction > 0);
}

SC_TEST("hot phase (B11): a save keeps its hot mode; one made before B11 loads with mode 0") {
    const AirWarData d = LoadHot("nato");
    AirCampaign c;
    c.Init(d, 9u);
    if (c.Advance(12.0)) c.SkipPlayerMission();
    std::string text = SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_nato.json");
    std::string e;
    AirCampaign now;
    SC_EXPECT(LoadCampaignJson(text, kData, now, &e));
    SC_EXPECT_NEAR(now.Data().tuning.Get("hot_phase.mode"), 1.0, 1e-12);
    SC_EXPECT(now.Journal().size() == c.Journal().size());
    const std::size_t at = text.find("\"hot_mode\"");
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
    SC_EXPECT(LoadCampaignJson(text, kData, old, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT_NEAR(old.Data().tuning.Get("hot_phase.mode"), 0.0, 1e-12);
}

SC_TEST("balance (B14): a save keeps its balance mode; one made before B14 loads with mode 0") {
    const AirWarData d = LoadHot("nato");
    SC_EXPECT_NEAR(d.tuning.Get("balance.mode"), 1.0, 1e-12);
    AirCampaign c;
    c.Init(d, 9u);
    if (c.Advance(12.0)) c.SkipPlayerMission();
    std::string text = SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_nato.json");
    std::string e;
    AirCampaign now;
    SC_EXPECT(LoadCampaignJson(text, kData, now, &e));
    SC_EXPECT_NEAR(now.Data().tuning.Get("balance.mode"), 1.0, 1e-12);
    SC_EXPECT(now.Journal().size() == c.Journal().size());
    const std::size_t at = text.find("\"balance_mode\"");
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
    SC_EXPECT(LoadCampaignJson(text, kData, old, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT_NEAR(old.Data().tuning.Get("balance.mode"), 0.0, 1e-12);
}

SC_TEST("balance (B14): the bands load, each with its source; the bench's measures and the campaign's are all there") {
    BalanceBands b;
    std::string e;
    SC_EXPECT(LoadBalanceBands(kData + "/air_war/balance_bands.json", b, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    for (const char* id : {"f105_jettison", "f105_press_on", "f105_abort", "checo_aim7_hit", "checo_aim7_out", "checo_aim9_hit", "checo_aim9_out",
                           "exchange_no_warning", "exchange_warning", "exchange_swing", "exchange_bekaa", "retask_pact_air_min", "retask_nato_air_min",
                           "alert_pact_r1_min", "friendly_fire_1960s", "friendly_fire_1985"}) {
        SC_EXPECT(b.Find("air_war_bench", id) != nullptr);
    }
    for (const char* f : {"rebels", "neutral", "warsaw_pact", "nato"}) {
        for (const char* t : {"theatre_pannonia.json", "theatre_pannonia_nation.json"}) SC_EXPECT(b.Find("air_war_balance", "loss_share", t, f, "careful") != nullptr);
    }
    SC_EXPECT(b.Find("air_war_balance", "battery_wrecked", "theatre_pannonia.json", "nato", "careful") != nullptr);
    SC_EXPECT(b.Find("air_war_balance", "battery_wrecked", "theatre_pannonia.json", "nato", "careful")->hi <= 0.4 + 1e-12);
    // The retask delays the data give agree with the doctrine's bands (hot_phase.hpp; c2.hpp floors a divert by them).
    const AirWarData pact = LoadHot("warsaw_pact");
    const BalanceBand* rp = b.Find("air_war_bench", "retask_pact_air_min");
    SC_EXPECT(rp != nullptr);
    if (rp == nullptr) return;
    for (double u = 0.0; u <= 1.0; u += 0.25) {
        const double s = RetaskAirS(pact.faction, "armed_recce", 0.99, u);
        SC_EXPECT(s >= rp->lo * 60.0 - 1e-9 && s <= rp->hi * 60.0 + 1e-9);
    }
}
