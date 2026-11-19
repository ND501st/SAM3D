// Roadmap B4: reactions under fire (simcore/reactions.hpp, simcore/reaction_chain.hpp; docs/air_battle/04_REACTIONS.md).
// The tables' numbers are the data's (data/gameplay/reactions.json, data/air_war/countermeasures.json); positions,
// speeds and seeds here are generic placeholders.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/mission_bridge.hpp"
#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/air_war/save.hpp"
#include "simcore/campaign/outcomes.hpp"
#include "simcore/em/random.hpp"
#include "simcore/reaction_chain.hpp"
#include "simcore/reactions.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

ReactionTable Table() {
    ReactionTable t;
    std::string e;
    if (!LoadReactionTable(kData, t, &e)) std::printf("    %s\n", e.c_str());
    return t;
}

// A sky for the chain alone: aircraft, their flights, the chain on them. Placeholder jets: 250 m/s north at 1000 m.
struct Sky {
    ReactionTable table;
    std::vector<Aircraft> air;
    Flights flights;
    ReactionChain chain;
    ReactionChain::Context ctx;
    double t = 0.0;
    std::vector<ReactionAct> acts;

    explicit Sky(std::uint64_t seed = 7, int year = 1985) : table(Table()) {
        chain.Configure(&table, seed, year);
        ctx.air = [this](int i) -> Aircraft* {
            return i >= 0 && i < static_cast<int>(air.size()) && air[static_cast<std::size_t>(i)].alive ? &air[static_cast<std::size_t>(i)] : nullptr;
        };
        ctx.agl_m = [](int) { return 1000.0; };
        ctx.attacking = [](int) { return false; };
    }
    Sky(const Sky&) = delete;
    Sky& operator=(const Sky&) = delete;

    int Jet(const Vec3& at, const std::string& id, const std::string& profile, double skill, const std::string& aircraft = "f4e",
            std::vector<ProgrammeZone> zones = {}) {
        Aircraft a;
        a.position = at;
        a.velocity = Vec3{0.0, 250.0, 0.0};
        a.speed_mps = 250.0;
        a.max_speed_mps = 400.0;
        a.max_turn_rate_rad_s = DegToRad(6.0);
        air.push_back(a);
        const int i = static_cast<int>(air.size()) - 1;
        ReactionPilotDef d;
        d.on = true;
        d.profile = profile;
        d.aircraft = aircraft;
        d.stores = "mk82";
        d.zones = std::move(zones);
        chain.Register(i, id, d, skill, false);
        return i;
    }
    // A flight of `n` jets in a package, each with its own pilot; returns its index.
    int Flight(const std::string& id, const std::string& package, int n, const Vec3& at, const std::string& profile, double skill) {
        FlightState f;
        f.id = id;
        f.callsign = id;
        f.package = package;
        for (int k = 0; k < n; ++k) {
            FlightSlot s;
            s.target = Jet(at + Vec3{300.0 * k, -300.0 * k, 0.0}, id + "-" + std::to_string(k + 1), profile, skill);
            s.number = k + 1;
            f.slots.push_back(s);
        }
        return flights.Add(f);
    }
    void Run(double until_s) {
        while (t < until_s - 1e-9) {
            t += 0.1;
            for (const ReactionAct& a : chain.Step(t, flights, ctx)) acts.push_back(a);
        }
    }
    int Count(int target, ReactionAct::Kind k) const {
        int n = 0;
        for (const ReactionAct& a : acts) n += a.target == target && a.kind == k ? 1 : 0;
        return n;
    }
    const ReactionAct* First(int target, ReactionAct::Kind k) const {
        for (const ReactionAct& a : acts) {
            if (a.target == target && a.kind == k) return &a;
        }
        return nullptr;
    }
};

airwar::Sighting RwrSighting(airwar::RwrState state, const Vec3& believed, double range_m, const std::string& name = "SA-8 Gecko") {
    airwar::Sighting s;
    s.kind = airwar::SightKind::Emitter;
    s.by = airwar::SightBy::Rwr;
    s.rwr = state;
    s.what = name;
    s.believed_enu = believed;
    s.range_est_m = range_m;
    return s;
}

airwar::Sighting EyeSighting(airwar::SightKind kind, const std::string& what, const Vec3& believed, double range_m, bool recognised = false) {
    airwar::Sighting s;
    s.kind = kind;
    s.by = airwar::SightBy::Eye;
    s.what = what;
    s.believed_enu = believed;
    s.range_est_m = range_m;
    s.recognised = recognised;
    return s;
}

}  // namespace

SC_TEST("reactions B4: the tables load, every faction has its profile, and a reaction draw is the prototype's (stream 20)") {
    const ReactionTable t = Table();
    SC_EXPECT(t.loaded);
    SC_EXPECT(t.tiers.size() == 3);
    for (const char* f : {"nato", "warsaw_pact", "neutral", "rebels"}) SC_EXPECT(t.Profile(f) != nullptr);
    SC_EXPECT(t.Profile("warsaw_pact")->programme && !t.Profile("nato")->programme);
    SC_EXPECT(t.Profile("nato")->package_net && !t.Profile("rebels")->calls);
    SC_EXPECT(t.RwrClass("SA-8 Gecko") == ThreatClass::SamLow && t.RwrClass("SA-2 Guideline") == ThreatClass::SamHigh);
    SC_EXPECT(t.RwrClass("something else") == ThreatClass::Unknown);
    // The keyed draw: the same value as prototype/samproto/em.py's draw_hash(42, 20, 7, 3, 1).
    SC_EXPECT_NEAR(em::Uniform01(em::DrawHash(42, em::Stream::Reaction, 7, 3, 1)), 0.08319907501352408, 1e-15);
}

SC_TEST("reactions B4: the delay's spread and the no-reaction share by tier over many draws") {
    const ReactionTable t = Table();
    for (const ReactionTier& tier : t.tiers) {
        NoticeContext c;
        c.source = CueSource::Eye;
        double sum = 0.0, lo = 1e9, hi = -1e9;
        int none = 0;
        const int n = 20000;
        for (int k = 0; k < n; ++k) {
            const double d = NoticeDelayS(t, tier.skill, c, ReactionU(5, 11, static_cast<std::uint64_t>(k), ReactionDraw::Delay));
            sum += d;
            lo = std::min(lo, d);
            hi = std::max(hi, d);
            none += ReactionU(5, 11, static_cast<std::uint64_t>(k), ReactionDraw::NoReaction) < NoReactionChance(t, tier.skill, c) ? 1 : 0;
        }
        SC_EXPECT(lo >= tier.delay_min_s - 1e-9 && hi <= tier.delay_max_s + 1e-9);
        SC_EXPECT_NEAR(sum / n, 0.5 * (tier.delay_min_s + tier.delay_max_s), 0.05 * tier.delay_max_s);
        SC_EXPECT_NEAR(static_cast<double>(none) / n, tier.no_reaction, 0.01);
    }
    // A receiver's warning comes sooner than the eye; workload and stress slow him; damage always gets a reaction; a
    // programmed cue is flown at the veteran's delay whatever his skill.
    NoticeContext eye, rwr, busy, hit, prog;
    rwr.source = CueSource::Rwr;
    busy.workload = true;
    busy.stress = 1.0;
    hit.source = CueSource::Damage;
    prog.programmed = true;
    SC_EXPECT(NoticeDelayS(t, 0.2, rwr, 0.5) < NoticeDelayS(t, 0.2, eye, 0.5));
    SC_EXPECT(NoticeDelayS(t, 0.2, busy, 0.5) > NoticeDelayS(t, 0.2, eye, 0.5));
    SC_EXPECT(NoReactionChance(t, 0.2, hit) == 0.0 && NoReactionChance(t, 0.2, prog) == 0.0);
    SC_EXPECT(NoticeDelayS(t, 0.2, prog, 1.0) <= t.tiers.back().delay_max_s + 1e-9);
}

SC_TEST("reactions B4: a row is the source's and class's, then the class's, then the source's, then the fallback") {
    ReactionProfile p;
    ReactionRow a, b, c;
    a.cue = CueSource::Eye;
    a.any_threat = false;
    a.threat = ThreatClass::Fighter;
    a.manoeuvre = Manoeuvre::BreakInto;
    b.any_threat = false;
    b.threat = ThreatClass::Fighter;
    b.manoeuvre = Manoeuvre::None;
    c.cue = CueSource::Rwr;
    c.manoeuvre = Manoeuvre::Jink;
    p.rows = {a, b, c};
    p.fallback.manoeuvre = Manoeuvre::Dive;
    SC_EXPECT(ChooseRow(p, CueSource::Eye, ThreatClass::Fighter).manoeuvre == Manoeuvre::BreakInto);
    SC_EXPECT(ChooseRow(p, CueSource::Radio, ThreatClass::Fighter).manoeuvre == Manoeuvre::None);
    SC_EXPECT(ChooseRow(p, CueSource::Rwr, ThreatClass::SamLow).manoeuvre == Manoeuvre::Jink);
    SC_EXPECT(ChooseRow(p, CueSource::Eye, ThreatClass::FlakLight).manoeuvre == Manoeuvre::Dive);
    // The pure helpers against hand calculations.
    const ReactionTable t = Table();
    SC_EXPECT_NEAR(JinkPeriodS(t, 1828.8), 4.0, 1e-9);  // 6,000 ft: 2/3 x 6 = 4 s
    SC_EXPECT_NEAR(JinkPeriodS(t, 30.0), t.jink_min_s, 1e-9);
    SC_EXPECT_NEAR(LateBreakWaitS(t, 6000.0), 6000.0 / t.late_missile_mps - t.late_lead_s, 1e-9);
    SC_EXPECT_NEAR(LateBreakWaitS(t, 100000.0), t.late_max_wait_s, 1e-9);
    SC_EXPECT(ClockOf(0.0, 0.0) == 12 && ClockOf(0.5 * kPi, 0.0) == 3 && ClockOf(kPi, 0.0) == 6 && ClockOf(0.0, 0.5 * kPi) == 9);
    SC_EXPECT_NEAR(StressAdd(0.5, 0.5), 0.75, 1e-12);
    SC_EXPECT_NEAR(StressDecay(0.8, t.stress_half_life_s, t.stress_half_life_s), 0.4, 1e-12);
}

SC_TEST("reactions B4: the year gate: no 256-flare Su-25 and no automatic dispenser in 1985; the MiG-23MLD's flares only from 1984") {
    const ReactionTable t = Table();
    SC_EXPECT(t.LoadoutOf("su25", 1985).flares == 128);
    SC_EXPECT(t.LoadoutOf("su25", 1988).flares == 256);
    SC_EXPECT(t.LoadoutOf("mig23mld", 1983).flares == 0);
    SC_EXPECT(t.LoadoutOf("mig23mld", 1985).flares == 60);
    SC_EXPECT(t.LoadoutOf("su24", 1985).chaff == 0);  // its dispenser is automatic: not modelled in the 1985 war
    SC_EXPECT(t.LoadoutOf("f4e", 1985).chaff > 0 && t.LoadoutOf("f4e", 1977).chaff == 0);
    for (const CountermeasureFit& f : t.fits) {
        // Every fit in a 1985 war is one of 1985 or before, or not loaded at all.
        const Loadout l = t.LoadoutOf(f.type, 1985);
        if (f.in_service_year > 1985 && f.kind == "flares") SC_EXPECT(l.flares != f.count || f.count == 0);
    }
}

SC_TEST("reactions B4: jettison, press-on and abort shares within a few per cent of the table") {
    const ReactionTable t = Table();
    const ReactionRow& row = ChooseRow(*t.Profile("nato"), CueSource::Eye, ThreatClass::Fighter);
    int jet = 0, tanks = 0, press = 0, abort = 0, round = 0;
    const int n = 20000;
    for (int k = 0; k < n; ++k) {
        const std::uint64_t s = static_cast<std::uint64_t>(k);
        const FlightDecision d = DecideFlight(t, row, 0.0, 0.5, ReactionU(3, 99, s, ReactionDraw::Jettison), ReactionU(3, 99, s, ReactionDraw::TanksOnly),
                                              ReactionU(3, 99, s, ReactionDraw::Choice));
        jet += d.jettison ? 1 : 0;
        tanks += d.tanks_only ? 1 : 0;
        press += d.choice == FlightChoice::PressOn ? 1 : 0;
        abort += d.choice == FlightChoice::Abort ? 1 : 0;
        round += d.choice == FlightChoice::GoRound ? 1 : 0;
    }
    SC_EXPECT_NEAR(static_cast<double>(jet) / n, row.jettison, 0.02);
    SC_EXPECT_NEAR(static_cast<double>(tanks) / n, row.jettison * row.tanks_only, 0.02);
    SC_EXPECT_NEAR(static_cast<double>(press) / n, row.press_on, 0.02);
    SC_EXPECT_NEAR(static_cast<double>(abort) / n, row.abort, 0.02);
    SC_EXPECT_NEAR(static_cast<double>(round) / n, 1.0 - row.press_on - row.abort, 0.02);
    // Stress raises the jettison and the abort; a pilot of more nerve presses on more.
    const FlightDecision calm = DecideFlight(t, row, 0.0, 0.5, row.jettison + 0.01, 0.9, row.press_on + 0.01);
    const FlightDecision scared = DecideFlight(t, row, 1.0, 0.5, row.jettison + 0.01, 0.9, row.press_on + 0.01);
    SC_EXPECT(!calm.jettison && scared.jettison && calm.choice == FlightChoice::Abort);
    SC_EXPECT(DecideFlight(t, row, 0.0, 1.0, 0.99, 0.9, row.press_on + 0.01).choice == FlightChoice::PressOn);
}

SC_TEST("reactions B4: no truth cue: what his picture does not name is no cue; a search paint is stress, no reaction") {
    Sky sky;
    const int i = sky.Jet(Vec3{0.0, 0.0, 1000.0}, "a-1", "nato", 0.95);
    // An aircraft he sees but does not recognise, a contrail: nothing to react to.
    sky.chain.Sighted(i, EyeSighting(airwar::SightKind::Aircraft, "aircraft", Vec3{0.0, 8000.0, 1000.0}, 8000.0), 0.0, sky.ctx);
    sky.chain.Sighted(i, EyeSighting(airwar::SightKind::Contrail, "contrail", Vec3{0.0, 8000.0, 9000.0}, 9000.0), 0.0, sky.ctx);
    SC_EXPECT(sky.chain.Totals().cues == 0);
    // A search radar on his receiver: his stress rises, he does nothing.
    sky.chain.Sighted(i, RwrSighting(airwar::RwrState::Search, Vec3{0.0, 15000.0, 0.0}, 15000.0), 0.0, sky.ctx);
    SC_EXPECT(sky.chain.Totals().cues == 0 && sky.chain.Stress(i, 0.0) > 0.0);
    sky.Run(30.0);
    SC_EXPECT(sky.acts.empty());
    // A recognised MiG is a fighter; a launch seen is a SAM: cues.
    sky.chain.Sighted(i, EyeSighting(airwar::SightKind::Aircraft, "mig23ml", Vec3{0.0, 8000.0, 1000.0}, 8000.0, true), 30.0, sky.ctx);
    sky.chain.Sighted(i, EyeSighting(airwar::SightKind::LaunchSign, "SAM launch", Vec3{0.0, 9000.0, 0.0}, 9000.0), 30.0, sky.ctx);
    SC_EXPECT(sky.chain.Totals().cues == 2);
}

SC_TEST("reactions B4: keyed draws give the same reactions whatever the order the aircraft are visited in") {
    std::vector<std::string> runs;
    for (int order = 0; order < 2; ++order) {
        Sky sky(11);
        std::vector<int> ids;
        for (int k = 0; k < 8; ++k) ids.push_back(sky.Jet(Vec3{2000.0 * k, 0.0, 1000.0}, "j" + std::to_string(k), "neutral", 0.5));
        for (int k = 0; k < 8; ++k) {
            const int i = ids[static_cast<std::size_t>(order == 0 ? k : 7 - k)];
            sky.chain.Sighted(i, RwrSighting(airwar::RwrState::Lock, Vec3{2000.0 * i, 12000.0, 0.0}, 12000.0), 1.0, sky.ctx);
        }
        sky.Run(40.0);
        std::string s;
        for (const ReactionAct& a : sky.acts) s += std::to_string(a.target) + ":" + std::to_string(static_cast<int>(a.kind)) + ":" + a.what + ";";
        runs.push_back(s);
    }
    SC_EXPECT(!runs[0].empty() && runs[0] == runs[1]);
}

SC_TEST("reactions B4: a Pact pilot flies his programme at once on a briefed zone, and an unplanned break on a battery that moved") {
    Sky sky;
    int programmed = 0, programme_turns = 0, unplanned_breaks = 0, unplanned_programme = 0, reacted_late = 0;
    std::vector<int> briefed, moved;
    for (int k = 0; k < 20; ++k) {
        briefed.push_back(sky.Jet(Vec3{40000.0 * k, 5000.0, 1000.0}, "p" + std::to_string(k), "warsaw_pact", 0.2, "mig23ml", {{Vec3{40000.0 * k, 20000.0, 0.0}, 10000.0}}));
        moved.push_back(sky.Jet(Vec3{40000.0 * k, -100000.0, 1000.0}, "m" + std::to_string(k), "warsaw_pact", 0.2, "mig23ml", {{Vec3{40000.0 * k, -85000.0, 0.0}, 10000.0}}));
    }
    for (int k = 0; k < 20; ++k) {
        // The briefed site lights him up from where he was told it is.
        sky.chain.Sighted(briefed[static_cast<std::size_t>(k)], RwrSighting(airwar::RwrState::Lock, Vec3{40000.0 * k, 20000.0, 0.0}, 15000.0), 0.0, sky.ctx);
        // The battery has moved 15 km east of its briefed place: his receiver points off the zone, which is far.
        sky.chain.Sighted(moved[static_cast<std::size_t>(k)], RwrSighting(airwar::RwrState::Lock, Vec3{40000.0 * k + 15000.0, -85000.0, 0.0}, 18000.0), 0.0, sky.ctx);
    }
    sky.Run(sky.table.tiers.back().delay_max_s + 0.2);
    for (int i : briefed) {
        const ReactionAct* m = sky.First(i, ReactionAct::Kind::Manoeuvre);
        if (m == nullptr) continue;
        ++programmed;
        if (m->ov.kind == PilotOverride::Kind::TurnAway && m->ov.end == PilotOverride::End::Point && m->dive) ++programme_turns;
    }
    for (int i : moved) reacted_late += sky.First(i, ReactionAct::Kind::Manoeuvre) != nullptr ? 0 : 1;
    sky.Run(30.0);
    for (int i : moved) {
        const ReactionAct* m = sky.First(i, ReactionAct::Kind::Manoeuvre);
        if (m == nullptr) continue;
        unplanned_breaks += m->ov.kind == PilotOverride::Kind::Break ? 1 : 0;
        unplanned_programme += m->ov.kind == PilotOverride::Kind::TurnAway ? 1 : 0;
    }
    std::printf("    briefed: %d of 20 flew the programme at once; moved: %d broke unplanned (none at once: %d of 20 still flying straight)\n",
                programme_turns, unplanned_breaks, reacted_late);
    SC_EXPECT(programmed == 20 && programme_turns == 20);  // a programmed cue always gets its reaction, at once
    SC_EXPECT(unplanned_programme == 0 && unplanned_breaks >= 10);
    SC_EXPECT(reacted_late >= 15);  // a green pilot's own delay: few within the veteran's
    SC_EXPECT(sky.chain.Totals().programmed == 20);
}

SC_TEST("reactions B4: a NATO pilot watches the missile, then pulls late; his chaff goes out with his reaction") {
    Sky sky;
    const int i = sky.Jet(Vec3{0.0, 0.0, 1000.0}, "n-1", "nato", 0.95);
    SC_EXPECT(sky.chain.Chaff(i) == sky.table.LoadoutOf("f4e", 1985).chaff);
    sky.chain.Sighted(i, RwrSighting(airwar::RwrState::Launch, Vec3{0.0, 9000.0, 0.0}, 9000.0), 0.0, sky.ctx);
    sky.Run(sky.table.tiers.back().delay_max_s * sky.table.rwr_factor * 2.0 + 0.2);
    const ReactionAct* watch = nullptr;
    for (const ReactionAct& a : sky.acts) {
        if (a.what.find("pull late") != std::string::npos) watch = &a;
    }
    SC_EXPECT(watch != nullptr);
    SC_EXPECT(sky.First(i, ReactionAct::Kind::Manoeuvre) == nullptr);  // not yet
    sky.Run(sky.t + sky.table.late_max_wait_s + 0.5);
    const ReactionAct* pull = sky.First(i, ReactionAct::Kind::Manoeuvre);
    SC_EXPECT(pull != nullptr && pull->what == "pulls hard now" && pull->ov.kind == PilotOverride::Kind::Break);
    // His chaff (if his tier dispenses on time with this draw) or none (early, late, never): a salvo at most.
    SC_EXPECT(sky.chain.Chaff(i) >= sky.table.LoadoutOf("f4e", 1985).chaff - static_cast<int>(sky.table.chaff_salvo));
}

SC_TEST("reactions B4: the 21-aircraft case: one fighter call on a package's frequency makes several flights jettison; a deaf package none") {
    int several = 0, seeds = 0, total = 0;
    for (std::uint64_t seed = 1; seed <= 12; ++seed) {
        for (int deaf = 0; deaf < 2; ++deaf) {
            Sky sky(seed);
            std::vector<int> fl;
            for (int f = 0; f < 5; ++f) fl.push_back(sky.Flight("f" + std::to_string(f), "pkg", 4, Vec3{3000.0 * f, 0.0, 1000.0}, "nato", 0.7));
            sky.Jet(Vec3{0.0, -20000.0, 8000.0}, "jammer", "nato", 0.7);  // the 21st: alone, in no flight
            if (deaf == 1) {
                // A jammed strike frequency: no member of any flight hears anything said on it.
                for (const FlightState& f : sky.flights.All()) {
                    for (const FlightSlot& sl : f.slots) const_cast<FlightSlot&>(sl).hears = false;
                }
            }
            // The leader of the first flight sees the MiGs and calls them.
            sky.chain.Sighted(sky.flights.All()[0].slots[0].target,
                              EyeSighting(airwar::SightKind::Aircraft, "mig23ml", Vec3{0.0, 9000.0, 1000.0}, 9000.0, true), 0.0, sky.ctx);
            sky.Run(60.0);
            int jettisoned = 0, decided = 0;
            for (int f : fl) {
                bool j = false;
                for (const FlightSlot& sl : sky.flights.All()[static_cast<std::size_t>(f)].slots) {
                    j = j || sky.Count(sl.target, ReactionAct::Kind::JettisonTanks) + sky.Count(sl.target, ReactionAct::Kind::JettisonStores) > 0;
                }
                jettisoned += j ? 1 : 0;
                decided += sky.chain.Decision(f, ThreatClass::Fighter) >= 0 ? 1 : 0;
            }
            if (seed == 1) {
                std::printf("    %s: %d of 5 flights decided, %d jettisoned (calls %d, heard %d)\n", deaf ? "deaf" : "heard", decided, jettisoned,
                            sky.chain.Totals().calls, sky.chain.Totals().heard);
            }
            if (sky.chain.Totals().calls == 0) continue;  // the caller did not react (his tier's no-reaction share): no call
            if (deaf == 0) {
                ++seeds;
                total += jettisoned;
                several += jettisoned >= 2 ? 1 : 0;
                SC_EXPECT(decided >= 3);  // the other flights' leaders heard the call and decided (some may not react)
            } else {
                SC_EXPECT(decided <= 1 && sky.chain.Totals().heard == 0);
                SC_EXPECT(jettisoned <= 1);  // only the caller's own flight can
            }
        }
    }
    std::printf("    %d of %d seeds with the call: more than one flight jettisoned on it (%d flights in all)\n", several, seeds, total);
    SC_EXPECT(seeds >= 8);
    SC_EXPECT(2 * several >= seeds);  // half a flight's chance each (nato's fighter row): two of five or more, mostly
    SC_EXPECT(total >= 2 * seeds);
}

SC_TEST("reactions B4: a lone damaged pilot's call, a wingman lost in sight is a cue, and the jettison is recorded once") {
    Sky sky(5);
    const int f = sky.Flight("g", "", 2, Vec3{0.0, 0.0, 1000.0}, "rebels", 0.25);
    const int lead = sky.flights.All()[static_cast<std::size_t>(f)].slots[0].target;
    const int wing = sky.flights.All()[static_cast<std::size_t>(f)].slots[1].target;
    sky.chain.Lost(lead, sky.air[static_cast<std::size_t>(lead)].position, 0.0, sky.flights, sky.ctx);
    SC_EXPECT(sky.chain.Totals().cues == 1);  // the wingman saw him go
    sky.chain.RecordJettison(wing, 1.0, "damaged");
    sky.chain.RecordJettison(wing, 2.0, "damaged");
    SC_EXPECT(sky.chain.Jettisons().size() == 1 && sky.chain.Jettisons()[0].stores && sky.chain.StoresGone(wing));
}

SC_TEST("reactions B4: a flight that threw its stores off short of its release point counts as prevented, and the save keeps it") {
    airwar::Package p;
    p.id = 3;
    p.kind = airwar::MissionKind::Strike;
    airwar::Flight a;
    a.role = airwar::FlightRole::Strike;
    a.count = 4;
    p.flights = {a, a, a};
    airwar::PackageOutcome o;
    o.released_flights = {0};
    o.jettisoned_flights = {1};
    o.aborted_flights = {1, 2};
    o.tanks_dropped = 6;
    o.stores_dropped = 3;
    const campaign::PackageLedger l = campaign::LedgerOf(p, o, {0, 1, 2}, "osa_21");
    SC_EXPECT(l.released == 1 && l.prevented_jettisoned == 1 && l.prevented_broke_away == 1 && l.Prevented() == 2);
    airwar::save_detail::Reader rd;
    const airwar::PackageOutcome back = rd.Outcome(airwar::save_detail::OutcomeJson(o));
    SC_EXPECT(rd.error.empty());
    SC_EXPECT(back.jettisoned_flights == o.jettisoned_flights && back.tanks_dropped == 6 && back.stores_dropped == 3);
}

namespace {

// A hand-written scenario with one target that reacts (the Osa fires on it; placeholder numbers).
std::string ReactingScenario(const std::string& target_extra) {
    return R"({
      "scenario_id": "rx", "name": "RX", "source": "sam3d", "confidence": "estimate", "seed": 3,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 },
      "terrain": { "type": "flat", "height_m": 0 },
      "damage": "gameplay/damage.json",
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [ { "id": "t1", "name": "T", "class": "fighter", "model": "f4e", "pilot_skill": 0.95,
                     "start": { "east_m": 1500, "north_m": 24000, "up_m": 1000 },
                     "waypoints": [ { "east_m": 1500, "north_m": -20000, "up_m": 1000 } ], "speed_mps": 200, "appear_at_s": 230)" +
           target_extra + R"( } ],
      "triggers": [
        { "id": "up", "when": {}, "do": [ { "order": "power_up", "crew": "Driver" },
                                           { "queue": "pto_on_line", "crew": "Driver" },
                                           { "order": "equipment_on", "crew": "Operator" },
                                           { "queue": "deploy_at_halt", "crew": "Operator" },
                                           { "order": "search_on_march", "crew": "Senior operator" } ] },
        { "id": "acq", "when": { "conditions": [ { "subsystem": "soc_transmitter", "is": "ready" } ] },
          "do": [ { "order": "acquire_with_soc", "crew": "Senior operator" }, { "order": "ready_capture", "crew": "Deputy" },
                  { "order": "capture_with_ssc", "crew": "Operator" },
                  { "queue": "prepare_firing", "crew": "Deputy" }, { "queue": "launch", "crew": "Deputy" } ] } ],
      "end": { "time_limit_s": 700 } })";
}

}  // namespace

SC_TEST("reactions B4: a scenario's target with \"reactions\" loads, and with \"evade\" as well it is refused") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    const std::string rx = R"(, "reactions": { "profile": "nato", "aircraft": "f4e", "stores": "mk82" })";
    SC_EXPECT(LoadScenario(ReactingScenario(rx), kData, def, console, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(def.targets.size() == 1 && def.targets[0].reactions.on && def.targets[0].reactions.profile == "nato");
    SC_EXPECT(def.reactions.loaded && def.has_damage);
    ScenarioDef bad;
    ConsoleData c2;
    SC_EXPECT(!LoadScenario(ReactingScenario(rx + R"(, "evade_on_launch": true)"), kData, bad, c2, &e));
    SC_EXPECT(e.find("evade") != std::string::npos);
    ScenarioDef none;
    ConsoleData c3;
    SC_EXPECT(!LoadScenario(ReactingScenario(R"(, "reactions": { "profile": "nobody" })"), kData, none, c3, &e));
}

SC_TEST("reactions B4: in a played session the pilot reacts to our launch he saw; the same seed plays the same, with the feed off too") {
    std::vector<std::string> debriefs;
    for (int fx = 0; fx < 2; ++fx) {
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        SC_EXPECT(LoadScenario(ReactingScenario(R"(, "reactions": { "profile": "nato", "aircraft": "f4e", "stores": "mk82" })"), kData, def, console, &e));
        ScenarioRunner runner(std::move(def), std::move(console));
        if (fx == 1) runner.SetFxOn(false);
        runner.RunToEnd(0.1, 700.0);
        const ReactionChain::Counts& n = runner.Reactions().Totals();
        std::printf("    cues %d, reactions %d, no reaction %d, chaff %d, jettisons %d, decisions %d\n", n.cues, n.reactions, n.no_reaction, n.chaff,
                    n.jettisons, n.decisions);
        SC_EXPECT(n.cues >= 1);
        SC_EXPECT(n.reactions + n.no_reaction >= 1);
        debriefs.push_back(runner.Debrief());
    }
    SC_EXPECT(debriefs[0] == debriefs[1]);
}

SC_TEST("reactions B4: a generated raid writes reactions and damage states, and the bridge carries its jettisons to the outcome") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", d, &e));
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(d, 2));
    int played = 0, reacting = 0, with_zones = 0, cues = 0;
    while (c.Now() < 4.0 * 24.0 && played < 2) {
        if (!c.Advance(4.0 * 24.0)) continue;
        const airwar::Package* p = c.PlayerMission();
        airwar::RaidScenarioBuilder builder;
        const airwar::RaidScenario rs = builder.Build(c, *p);
        if (!rs.ok) {
            c.SkipPlayerMission();
            continue;
        }
        ScenarioDef def;
        ConsoleData console;
        SC_EXPECT(LoadScenario(rs.json_text, kData, def, console, &e));
        SC_EXPECT(def.has_damage);
        for (const ScenarioTarget& t : def.targets) {
            SC_EXPECT(!t.evade.present);
            reacting += t.reactions.on ? 1 : 0;
            with_zones += t.reactions.zones.empty() ? 0 : 1;
            if (t.reactions.on) SC_EXPECT(t.reactions.profile == "warsaw_pact");
        }
        ScenarioRunner runner(std::move(def), std::move(console));
        airwar::MissionBridge bridge(rs.manifest, c.Data(), 5u);
        while (!runner.Finished() && !bridge.UnitDestroyed() && runner.Time() < rs.manifest.end_s + 60.0) {
            runner.Step(0.1);
            bridge.Step(runner);
        }
        const airwar::PackageOutcome o = bridge.Finish(runner);
        cues += runner.Reactions().Totals().cues;
        int stores = 0;
        for (const JettisonRecord& j : runner.Reactions().Jettisons()) stores += j.stores ? 1 : 0;
        SC_EXPECT(o.stores_dropped == stores);
        for (int fi : o.jettisoned_flights) {
            SC_EXPECT(std::find(o.released_flights.begin(), o.released_flights.end(), fi) == o.released_flights.end());
        }
        c.CompletePlayerMission(o, rs.manifest.start_h + runner.Time() / 3600.0);
        ++played;
    }
    std::printf("    %d raids: %d pilots reacting (%d briefed on zones), %d cues\n", played, reacting, with_zones, cues);
    SC_EXPECT(played >= 1 && reacting > 0);
}

SC_TEST("reactions S4: a hand-written evade is a scripted row: its cue only, from the picture, no tables, not On()") {
    Sky sky;
    EvadeDef launch;
    launch.present = true;
    launch.on = EvadeDef::On::Launch;
    EvadeDef lock = launch;
    lock.on = EvadeDef::On::Lock;
    sky.chain.RegisterScripted(0, "s-1", launch);
    sky.chain.RegisterScripted(1, "s-2", lock);
    sky.chain.RegisterScripted(2, "s-3", EvadeDef());  // not present: no row
    SC_EXPECT(sky.chain.Scripted(0) && sky.chain.Scripted(1) && !sky.chain.Scripted(2));
    SC_EXPECT(!sky.chain.On(0) && !sky.chain.On(1));
    ReactionChain::ScriptedCue cue;
    const Vec3 believed{2000.0, 9000.0, 0.0};
    // A search paint is no cue for either; a lock is the lock row's (not a launch), a launch on the receiver the launch row's.
    SC_EXPECT(!sky.chain.SightedScripted(0, RwrSighting(airwar::RwrState::Search, believed, 9000.0), cue));
    SC_EXPECT(!sky.chain.SightedScripted(1, RwrSighting(airwar::RwrState::Search, believed, 9000.0), cue));
    SC_EXPECT(!sky.chain.SightedScripted(0, RwrSighting(airwar::RwrState::Lock, believed, 9000.0), cue));
    SC_EXPECT(sky.chain.SightedScripted(1, RwrSighting(airwar::RwrState::Lock, believed, 9000.0), cue));
    SC_EXPECT(!cue.launch && (cue.from - believed).Length() < 1e-9);
    SC_EXPECT(sky.chain.SightedScripted(0, RwrSighting(airwar::RwrState::Launch, believed, 9000.0), cue));
    SC_EXPECT(cue.launch && (cue.from - believed).Length() < 1e-9);
    // A launch seen by eye: the place he believes the trail began, not the unit's.
    airwar::Sighting trail = EyeSighting(airwar::SightKind::Trail, "SAM trail", Vec3{0.0, 5000.0, 800.0}, 5000.0);
    trail.has_origin = true;
    trail.origin_enu = Vec3{300.0, 7000.0, 0.0};
    SC_EXPECT(sky.chain.SightedScripted(0, trail, cue));
    SC_EXPECT(cue.launch && (cue.from - trail.origin_enu).Length() < 1e-9);
    SC_EXPECT(!sky.chain.SightedScripted(1, trail, cue));  // a lock row: no
    // An aircraft seen is nothing to a scripted row; the instructor's order is any row's.
    SC_EXPECT(!sky.chain.SightedScripted(0, EyeSighting(airwar::SightKind::Aircraft, "mig23ml", believed, 8000.0, true), cue));
    SC_EXPECT(sky.chain.OrderedScripted(1) && !sky.chain.OrderedScripted(2));
    SC_EXPECT(sky.chain.Totals().scripted == 4 && sky.chain.Totals().cues == 0);  // the lock, two launches, the order
    sky.Run(10.0);
    SC_EXPECT(sky.acts.empty());  // the chain's own acts: none (the pilot flies the evasion himself)
}

// (A lock reaches him only on a warning receiver, which this target has none of: the launch, seen by eye.)
SC_TEST("reactions S4: in a played session a hand-written evade breaks on the launch his picture holds") {
    for (const char* on : {"launch"}) {
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        SC_EXPECT(LoadScenario(ReactingScenario(std::string(R"(, "evade": { "on": ")") + on + R"(", "kind": "break" })"), kData, def, console, &e));
        if (!e.empty()) std::printf("    %s\n", e.c_str());
        ScenarioRunner runner(std::move(def), std::move(console));
        runner.RunToEnd(0.1, 700.0);
        const ReactionChain::Counts& n = runner.Reactions().Totals();
        std::printf("    evade on %s: scripted cues %d (chain cues %d)\n", on, n.scripted, n.cues);
        SC_EXPECT(n.scripted >= 1 && n.cues == 0);
        SC_EXPECT(runner.Debrief().find("break") != std::string::npos);
    }
}
