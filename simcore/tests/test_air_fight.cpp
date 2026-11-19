// The air battle (roadmap track B, B8; docs/air_battle/07_AIR_TO_AIR.md sections 4-5; simcore/air_war/air_fight.hpp,
// air_fight_session.hpp, air_fight_def.hpp): the abstract fight that replaced the resolver's flat fighter roll, and the
// live fight in a session. The bands are the period's exchange ratios (Linebacker 1972 about 2:1 for the USAF against
// the MiG-21; ACEVAL 1977's many-on-many near parity to 2:1 with all-aspect missiles; the Bekaa 1982 far beyond): the
// test checks the ordering and a wide band, not one engagement's numbers.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/air_fight.hpp"
#include "simcore/air_war/air_fight_session.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/scenery/munitions.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

AirWarData LoadWar(const std::string& faction) {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

Flight MakeFlight(FlightRole role, const std::string& type, int n, Profile prof) {
    Flight f;
    f.callsign = std::string(FlightRoleName(role)) + " " + type;
    f.role = role;
    f.aircraft = type;
    f.count = n;
    f.profile = prof;
    return f;
}

// The abstract fight over many seeds: the strikers lost, the cover lost, ours lost, and the strikers' answers.
struct Tally {
    int fights = 0, strikers_lost = 0, cover_lost = 0, ours_lost = 0, answered = 0, met = 0;
};
Tally Many(const AirWarData& d, const Package& p, const AbstractFightSetup& s, int runs) {
    const AbstractAirFight fight(d);
    Tally t;
    std::vector<int> alive;
    for (const Flight& f : p.flights) alive.push_back(f.count);
    for (int k = 1; k <= runs; ++k) {
        const AbstractFightResult r = fight.Resolve(p, alive, s, static_cast<std::uint64_t>(k) * 7919u);
        if (!r.found) continue;
        ++t.fights;
        t.met += r.met_cover ? 1 : 0;
        for (const AbstractFightResult::Loss& l : r.enemy_lost) {
            const FlightRole role = p.flights[static_cast<std::size_t>(l.flight)].role;
            (AbstractAirFight::IsMain(role) ? t.strikers_lost : t.cover_lost) += 1;
        }
        t.ours_lost += static_cast<int>(r.ours_lost.size());
        t.answered += static_cast<int>(r.aborted.size() + r.jettisoned.size());
    }
    return t;
}

AbstractFightSetup Day(double ff, double awacs = 0.0) {
    AbstractFightSetup s;
    s.friendly_fighters = ff;
    s.awacs = awacs;
    return s;
}

}  // namespace

// ------------------------------------------------------------------------------------------------ the data

SC_TEST("air fight B8: the fight's data loads; every doctrine has its plan and its rule for a lost controller") {
    const AirWarData d = LoadWar("nato");
    const AirCombatDef& ac = d.air_combat;
    SC_EXPECT(ac.fight.vid_m > 1000.0 && ac.fight.bingo_s >= ac.fight.joker_s && ac.fight.a0_rounds >= 1.0);
    SC_EXPECT(ac.ours.regiments.size() == 3 && ac.ours.Total() == 108);
    for (const char* f : {"nato", "warsaw_pact", "neutral", "rebels", "ours"}) {
        const AirDoctrineDef* doc = ac.Doctrine(f);
        SC_EXPECT(doc != nullptr);
        if (doc == nullptr) continue;
        SC_EXPECT(doc->plan_close + doc->plan_sweep + doc->plan_cap > 0.0);
        for (const std::string* s : {&doc->control_lost_green, &doc->control_lost_skilled}) SC_EXPECT(*s == "abort" || *s == "pairs" || *s == "continue");
    }
    // A green Pact pilot aborts when he loses his controller, a skilled one goes on in pairs; NATO's leader goes on.
    SC_EXPECT(ac.Doctrine("warsaw_pact")->OnControlLost(0.3) == "abort" && ac.Doctrine("warsaw_pact")->OnControlLost(0.8) == "pairs");
    SC_EXPECT(ac.Doctrine("nato")->OnControlLost(0.3) == "continue");
    // B11's list: the MiG-23ML is the Pact's standard of 1985 (the MLD from 1984), the Draken from 1988, the Saab 105
    // escorts; the guns of the strike types.
    const AirWarData wp = LoadWar("warsaw_pact");
    const AircraftType* ml = wp.faction.Aircraft("mig23ml_wp");
    const AircraftType* mld = wp.faction.Aircraft("mig23mld");
    SC_EXPECT(ml != nullptr && mld != nullptr && mld->in_service_year == 1984 && ml->rwr_em == "pact_spo10_class_rwr");
    SC_EXPECT(ac.LoadOutFor("mig23ml_wp", 1985, false).Carries("r24r"));
    for (const char* t : {"mig27", "su24", "su25", "su17m3"}) SC_EXPECT(ac.LoadOutFor(t, 1985, false).gun != nullptr);
    const AirWarData nt = LoadWar("neutral");
    const AircraftType* draken = nt.faction.Aircraft("draken_gun");
    const AircraftType* saab = nt.faction.Aircraft("saab105");
    SC_EXPECT(draken != nullptr && !draken->InService(1985) && draken->InService(1988));
    SC_EXPECT(saab != nullptr && saab->Has(FlightRole::Escort));
}

SC_TEST("air fight B8: an AWACS controls fighters only from its post's year (the E-3A from 1982, the A-50 not in 1985)") {
    AirWarData nato = LoadWar("nato");
    AirWarData wp = LoadWar("warsaw_pact");
    SC_EXPECT(nato.theatre.year == 1985);
    SC_EXPECT(AwacsControlsFighters(nato) && !AwacsControlsFighters(wp));
    wp.theatre.year = 1990;
    nato.theatre.year = 1980;
    SC_EXPECT(AwacsControlsFighters(wp) && !AwacsControlsFighters(nato));
    // The setup drops the A-50's share in 1985.
    wp.theatre.year = 1985;
    Package p;
    SC_EXPECT(AbstractAirFight::SetupFor(wp, p, 0.8, airwar::Weather::Clear).awacs == 0.0);
    SC_EXPECT_NEAR(AbstractAirFight::SetupFor(LoadWar("nato"), p, 0.8, airwar::Weather::Clear).awacs, 0.8, 1e-12);
}

// ------------------------------------------------------------------------------------------------ the abstract fight

SC_TEST("air fight B8: the abstract fight is the same for the same seed, and its kills are written to our regiments") {
    const AirWarData d = LoadWar("nato");
    Package p;
    p.flights = {MakeFlight(FlightRole::Strike, "f16a", 4, Profile::Low), MakeFlight(FlightRole::Sweep, "f15c", 2, Profile::Medium)};
    const AbstractAirFight fight(d);
    std::set<std::string> regiments;
    for (const OurRegimentDef& r : d.air_combat.ours.regiments) regiments.insert(r.id);
    int found = 0;
    for (std::uint64_t seed = 1; seed <= 300; ++seed) {
        const AbstractFightResult a = fight.Resolve(p, {4, 2}, Day(1.0), seed);
        const AbstractFightResult b = fight.Resolve(p, {4, 2}, Day(1.0), seed);
        SC_EXPECT(a.enemy_lost.size() == b.enemy_lost.size() && a.ours_lost == b.ours_lost && a.shots == b.shots && a.aborted == b.aborted);
        found += a.found ? 1 : 0;
        for (const AbstractFightResult::Loss& l : a.enemy_lost) SC_EXPECT(regiments.count(l.by) == 1);
        for (const std::string& r : a.ours_lost) SC_EXPECT(regiments.count(r) == 1);
    }
    // Sent half the time at the front's full threat, found 85 % of those by day, less the low profile's 0.6.
    std::printf("    found %d of 300 (expected about %.0f)\n", found, 300 * 0.5 * 0.85 * 0.6);
    SC_EXPECT(found > 50 && found < 110);
}

SC_TEST("air fight B8: exchange ratios by the period: NATO's F-15s far ahead of our MiG-21s, the Pact's MiG-23MLs ahead but less") {
    const AirWarData nato = LoadWar("nato");
    const AirWarData wp = LoadWar("warsaw_pact");
    Package pn;
    pn.flights = {MakeFlight(FlightRole::Strike, "f16a", 4, Profile::Medium), MakeFlight(FlightRole::Sweep, "f15c", 4, Profile::Medium)};
    Package pw;
    pw.flights = {MakeFlight(FlightRole::Strike, "su24", 4, Profile::Medium), MakeFlight(FlightRole::Sweep, "mig23ml_wp", 4, Profile::Medium)};
    const Tally tn = Many(nato, pn, Day(1.0), 6000);
    const Tally tw = Many(wp, pw, Day(1.0), 6000);
    const double rn = static_cast<double>(tn.ours_lost) / std::max(1, tn.cover_lost);
    const double rw = static_cast<double>(tw.ours_lost) / std::max(1, tw.cover_lost);
    std::printf("    NATO sweep: %d fights, ours lost %d, F-15s lost %d (%.1f:1); Pact sweep: %d fights, ours lost %d, MiG-23s lost %d (%.1f:1)\n",
                tn.fights, tn.ours_lost, tn.cover_lost, rn, tw.fights, tw.ours_lost, tw.cover_lost, rw);
    SC_EXPECT(tn.ours_lost > 20 && tw.ours_lost + tw.cover_lost > 20);
    // Linebacker's 2:1 is the floor for the F-15 with all-aspect AIM-7Fs and AIM-9Ls against MiG-21s with rear-aspect
    // missiles; the MiG-23ML's R-24R and R-60 against the same MiG-21s under the same GCI give an edge, not the F-15's.
    SC_EXPECT(rn >= 2.0 && rn > 1.3 * rw);
    SC_EXPECT(rw > 0.5 && rw < 4.0);
}

SC_TEST("air fight B8: a sweep and an escort keep our fighters off the strikers, an E-3A more so; the strikers answer") {
    const AirWarData d = LoadWar("nato");
    Package bare;
    bare.flights = {MakeFlight(FlightRole::Strike, "tornado_ids", 4, Profile::Medium)};
    Package swept = bare;
    swept.flights.push_back(MakeFlight(FlightRole::Sweep, "f15c", 2, Profile::Medium));
    swept.flights.push_back(MakeFlight(FlightRole::Escort, "f16a", 2, Profile::Medium));
    const Tally a = Many(d, bare, Day(1.0), 4000);
    const Tally b = Many(d, swept, Day(1.0), 4000);
    const Tally c = Many(d, swept, Day(1.0, 1.0), 4000);
    std::printf("    strikers lost: alone %d, with a sweep and an escort %d, and an E-3A up %d; answers (jettison or abort) alone %d\n", a.strikers_lost,
                b.strikers_lost, c.strikers_lost, a.answered);
    SC_EXPECT(a.strikers_lost > 0 && b.strikers_lost < a.strikers_lost && c.strikers_lost <= b.strikers_lost);
    SC_EXPECT(a.answered > 0);
    // The planner's expectation follows the fight: within a factor of three of its mean, and lower with the cover.
    const AbstractAirFight fight(d);
    const double sim = static_cast<double>(a.strikers_lost) / 4000.0;
    const double exp_bare = fight.ExpectedMainLoss(bare, Day(1.0));
    std::printf("    strikers lost a package: simulated %.3f, the planner's %.3f\n", sim, exp_bare);
    SC_EXPECT(exp_bare > sim / 3.0 && exp_bare < sim * 3.0);
    SC_EXPECT(fight.ExpectedMainLoss(swept, Day(1.0)) < exp_bare && fight.ExpectedMainLoss(swept, Day(1.0, 1.0)) < fight.ExpectedMainLoss(swept, Day(1.0)));
}

SC_TEST("air fight B8: the night, the weather and the low profile hide a package from our fighters") {
    const AirWarData d = LoadWar("warsaw_pact");
    Package p;
    p.profile = Profile::Medium;
    p.flights = {MakeFlight(FlightRole::Strike, "su24", 4, Profile::Medium)};
    const AbstractAirFight fight(d);
    AbstractFightSetup day = Day(1.0), night = Day(1.0), poor = Day(1.0);
    night.night = true;
    poor.poor_weather = true;
    Package low = p;
    low.profile = Profile::Low;
    SC_EXPECT(fight.FindChance(p, night) < fight.FindChance(p, day) && fight.FindChance(p, poor) < fight.FindChance(p, day));
    SC_EXPECT(fight.FindChance(low, day) < fight.FindChance(p, day));
    SC_EXPECT_NEAR(fight.FindChance(p, day), 0.5 * 0.85, 1e-9);
}

SC_TEST("air fight B14: attacked strike flights answer as the F-105s did; an E-3A's warning swings the exchange") {
    // The balance pass's bands (data/air_war/balance_bands.json, red_baron_vol2, aces_1976, checo_1978_linebacker); the
    // bench (air_war_bench) checks them over more fights.
    AirWarData d = LoadWar("nato");
    BalanceBands bands;
    std::string e;
    SC_EXPECT(LoadBalanceBands(kData + "/air_war/balance_bands.json", bands, &e));
    const BalanceBand* jett = bands.Find("air_war_bench", "f105_jettison");
    const BalanceBand* ab = bands.Find("air_war_bench", "f105_abort");
    const BalanceBand* swing = bands.Find("air_war_bench", "exchange_swing");
    SC_EXPECT(jett != nullptr && ab != nullptr && swing != nullptr);
    if (jett == nullptr || ab == nullptr || swing == nullptr) return;
    Package p;
    p.flights = {MakeFlight(FlightRole::Strike, "f4e", 4, Profile::Medium), MakeFlight(FlightRole::Strike, "f4e", 4, Profile::Medium)};
    const auto answers = [&](const AirWarData& data, int& attacked, int& jettison, int& aborted) {
        const AbstractAirFight fight(data);
        attacked = jettison = aborted = 0;
        for (std::uint64_t k = 1; k <= 3000; ++k) {
            const AbstractFightResult r = fight.Resolve(p, {4, 4}, Day(1.0), k * 7919u);
            attacked += r.attacked;
            jettison += r.jettison_drawn;
            aborted += static_cast<int>(r.aborted.size());
        }
    };
    int a1 = 0, j1 = 0, b1 = 0;
    answers(d, a1, j1, b1);
    d.tuning.values["balance.mode"] = 0.0;
    int a0 = 0, j0 = 0, b0 = 0;
    answers(d, a0, j0, b0);
    d.tuning.values["balance.mode"] = 1.0;
    std::printf("    %d flights attacked: %.0f%% jettisoned, %.0f%% turned home (before B14 %.0f%%)\n", a1, 100.0 * j1 / std::max(1, a1),
                100.0 * b1 / std::max(1, a1), 100.0 * b0 / std::max(1, a0));
    SC_EXPECT(a1 > 300 && a0 == a1 && j0 == j1);  // the abort's draw is one either way: nothing else moves
    SC_EXPECT(j1 >= jett->lo * a1 && j1 <= jett->hi * a1);
    SC_EXPECT(b1 >= ab->lo * a1 && b1 <= ab->hi * a1);
    SC_EXPECT(b0 > b1);  // the old reading turned NATO home 36 % of the time
    // An F-4E strike with an F-4E escort against our MiG-21s, without and with an E-3A: ours lost for each of theirs.
    Package v;
    v.flights = {MakeFlight(FlightRole::Strike, "f4e", 4, Profile::Medium), MakeFlight(FlightRole::Escort, "f4e", 4, Profile::Medium)};
    const Tally bare = Many(d, v, Day(1.0), 4000);
    const Tally warned = Many(d, v, Day(1.0, 1.0), 4000);
    const double rb = static_cast<double>(bare.ours_lost) / std::max(1, bare.strikers_lost + bare.cover_lost);
    const double rw = static_cast<double>(warned.ours_lost) / std::max(1, warned.strikers_lost + warned.cover_lost);
    std::printf("    exchange (ours lost for each of theirs): no warning %.2f, under an E-3A %.2f\n", rb, rw);
    SC_EXPECT(rw >= swing->lo * rb);
}

SC_TEST("air fight B8: the resolver fights once a package; its losses to fighters name the regiment, ours lost are counted") {
    AirWarData d = LoadWar("warsaw_pact");
    AirCampaign base;
    SC_EXPECT(base.Init(d, 1));
    const int bi = base.PlayerIndex();
    const Vec3 b = base.Truth()[static_cast<std::size_t>(bi)].pos;
    std::set<std::string> regiments;
    for (const OurRegimentDef& r : d.air_combat.ours.regiments) regiments.insert(r.id);
    int fights = 0, fighter_losses = 0, ours = 0, bad = 0;
    for (int seed = 1; seed <= 150; ++seed) {
        std::vector<ObjectState> truth = base.Truth();
        BatteryPosture posture;
        AirStaff staff;
        Rng rs(9);
        staff.Build(d, rs, 0.0);
        Package p;
        p.id = seed;
        p.kind = MissionKind::Strike;
        p.target_pos = b + Vec3{0.0, 40000.0, 0.0};
        p.part = DayPart::Day;
        p.profile = Profile::Medium;
        Flight st = MakeFlight(FlightRole::Strike, "su24", 4, Profile::Medium);
        const Vec3 aim = p.target_pos;
        st.route = {{aim + Vec3{0.0, 120000.0, 0.0}, 4000.0, 0.0, "entry"},
                    {aim + Vec3{0.0, 40000.0, 0.0}, 4000.0, 0.1, "ip"},
                    {aim, 4000.0, 0.15, "target"},
                    {aim + Vec3{0.0, 120000.0, 0.0}, 4000.0, 0.3, "exit"}};
        st.release_index = 2;
        st.aim_pos = aim;
        Flight sw = MakeFlight(FlightRole::Sweep, "mig23ml_wp", 2, Profile::Medium);
        sw.route = st.route;
        p.flights = {st, sw};
        Rng rng(static_cast<std::uint64_t>(seed));
        ResolveContext rc;
        rc.data = &d;
        rc.truth = &truth;
        rc.posture = &posture;
        rc.staff = &staff;
        rc.rng = &rng;
        Resolver r;
        const PackageOutcome o = r.Resolve(p, rc);
        SC_EXPECT(o.air_fights <= 1);
        fights += o.air_fights;
        for (const LossRecord& l : o.losses) {
            if (!l.fighter) continue;
            ++fighter_losses;
            bad += regiments.count(l.by) == 1 ? 0 : 1;
        }
        for (const std::string& by : o.our_fighters_lost) bad += regiments.count(by) == 1 ? 0 : 1;
        ours += static_cast<int>(o.our_fighters_lost.size());
    }
    std::printf("    150 packages: %d fights, %d of theirs and %d of ours shot down in them\n", fights, fighter_losses, ours);
    SC_EXPECT(fights > 5 && fighter_losses + ours > 0 && bad == 0);
}

// ------------------------------------------------------------------------------------------------ the live fight

namespace {

// A hand-made world: aircraft that fly straight, turned at once toward what they are told; pictures set by the test.
struct HandWorld {
    std::vector<Aircraft> air;
    std::vector<std::vector<Sighting>> pictures;
    std::vector<bool> gone;
    std::vector<std::string> events;
    std::function<double(int)> heard_at = [](int) { return 0.0; };  // < 0: never heard

    int Add(const Vec3& at, const Vec3& vel) {
        Aircraft a;
        a.position = at;
        a.velocity = vel;
        a.max_turn_rate_rad_s = 0.2;
        a.rcs_m2 = 5.0;
        a.max_load_g = 7.0;
        air.push_back(a);
        pictures.emplace_back();
        gone.push_back(false);
        return static_cast<int>(air.size()) - 1;
    }
    AirFightWorld World() {
        AirFightWorld w;
        w.air = [this](int t) -> const Aircraft* { return t >= 0 && static_cast<std::size_t>(t) < air.size() && !gone[static_cast<std::size_t>(t)] ? &air[static_cast<std::size_t>(t)] : nullptr; };
        w.picture = [this](int t) -> const std::vector<Sighting>* { return &pictures[static_cast<std::size_t>(t)]; };
        w.altitude = [](const Vec3& p) { return p.z; };
        w.agl = [](const Vec3& p) { return p.z; };
        w.steer = [this](int t, const PilotOverride& ov) {
            Aircraft& a = air[static_cast<std::size_t>(t)];
            Vec3 d = ov.point - a.position;
            d.z = 0.0;
            if (d.Length() > 1.0) a.velocity = d * (a.velocity.Length() / d.Length());
        };
        w.release = [](int) {};
        w.destroy = [this](int t, const std::string&) {
            gone[static_cast<std::size_t>(t)] = true;
            return true;
        };
        w.event = [this](const std::string& who, const std::string& what) { events.push_back(who + ": " + what); };
        w.say = [](const std::string&, int, const std::string&, double) { return 7; };
        w.heard = [this](int, int, double t) {
            const double h = heard_at(0);
            return h < 0.0 ? -1.0 : t + h;
        };
        return w;
    }
    void Move(double dt) {
        for (Aircraft& a : air) a.position += a.velocity * dt;
    }
};

// B8's fight (mode 0) unless asked for roadmap S3's (mode 1).
AirFightDef Def(int mode = 0) {
    AirFightDef d;
    d.present = true;
    d.year = 1985;
    d.mode = mode;
    std::string e;
    if (!LoadAirCombatFile(kData + "/air_war/air_combat.json", d.combat, &e)) std::printf("    %s\n", e.c_str());
    if (!LoadSensorsFile(kData + "/air_war/sensors.json", d.sensors, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

AirFightFighterDef Fighter(const std::string& target, const std::string& type, const std::string& faction, const std::string& role,
                           const std::string& controller = "", double skill = -1.0) {
    AirFightFighterDef f;
    f.target = target;
    f.type = type;
    f.faction = faction;
    f.role = role;
    f.controller = controller;
    f.skill = skill;
    f.regiment = faction == "ours" ? "fighter_rgt_1" : "";
    return f;
}

// An F-15C against one of our MiG-21MFs head-on, 60 km apart at medium height; `e3a`: the F-15 under an E-3A.
struct Duel {
    AirFightDef def = Def();
    HandWorld w;
    AirFightSession s;
    double t = 0.0;
    double first_shot_m = -1.0;
    double recognised_at_m = -1.0;
    bool shot_before_vid = false;
    // The presentation hooks (roadmap B13), when `fx`: each launch, end and burst as the fight told them.
    bool fx = false;
    struct FxShot {
        int shooter = -1, target = -1, shot = 0;
        std::string weapon;
        double t = 0.0, until = 0.0;
        bool kill = false, gone = false;
    };
    std::vector<FxShot> launches, ends, bursts;

    explicit Duel(bool e3a, std::uint64_t seed = 11) {
        w.Add(Vec3{0.0, 0.0, 6000.0}, Vec3{0.0, 250.0, 0.0});         // 0: the F-15
        w.Add(Vec3{0.0, 60000.0, 5000.0}, Vec3{0.0, -230.0, 0.0});    // 1: the MiG
        def.fighters.push_back(Fighter("eagle", "f15c", "nato", "sweep", e3a ? "e3a" : ""));
        def.fighters.push_back(Fighter("red", "mig21mf", "ours", "strike"));
        if (e3a) {
            AirFightControllerDef c;
            c.id = "e3a";
            c.side = "enemy";
            c.kind = "airborne_warning";
            c.pos = Vec3{-150000.0, 0.0, 9000.0};
            c.range_m = 400000.0;
            def.controllers.push_back(c);
        }
        std::string e;
        if (!s.Init(def, {"eagle", "red"}, seed, &e)) std::printf("    %s\n", e.c_str());
    }
    void Run(double until_s) {
        const double dt = 0.25;
        while (t < until_s) {
            t += dt;
            w.Move(dt);
            const double range = (w.air[1].position - w.air[0].position).Length();
            // The F-15 pilot's eyes: the MiG seen inside 9 km, recognised inside 5 km.
            w.pictures[0].clear();
            if (range < 9000.0 && !w.gone[1]) {
                Sighting sg;
                sg.kind = SightKind::Aircraft;
                sg.believed_enu = w.air[1].position;
                sg.range_est_m = range;
                sg.time_s = t;
                sg.recognised = range < 5000.0;
                sg.what = sg.recognised ? "MiG-21" : "aircraft";
                if (sg.recognised && recognised_at_m < 0.0) recognised_at_m = range;
                w.pictures[0].push_back(sg);
            }
            AirFightWorld aw = w.World();
            if (fx) {
                aw.fx_launch = [this](int shooter, int target, const AamDef& aam, int shot, double at, double arrive_s) {
                    launches.push_back({shooter, target, shot, aam.munition, at, arrive_s, false, false});
                };
                aw.fx_end = [this](int shooter, int target, const AamDef& aam, int shot, double at, bool kill, bool gone_before) {
                    ends.push_back({shooter, target, shot, aam.munition, at, at, kill, gone_before});
                };
                aw.fx_gun = [this](int shooter, int target, const AirGunDef& gun, double at, double burst_s, bool kill) {
                    bursts.push_back({shooter, target, 0, gun.id, at, at + burst_s, kill, false});
                };
            }
            s.Step(t, aw);
            for (const std::string& ev : w.events) {
                if (first_shot_m < 0.0 && ev.rfind("eagle: fires", 0) == 0) {
                    first_shot_m = range;
                    shot_before_vid = recognised_at_m < 0.0;
                }
            }
            w.events.clear();
            if (w.gone[1] || w.gone[0]) break;
        }
    }
};

}  // namespace

SC_TEST("air fight B8 live: a NATO pilot without an E-3A holds his fire until he has recognised the type by eye") {
    Duel d(false);
    d.Run(240.0);
    std::printf("    first shot at %.0f m, recognised at %.0f m\n", d.first_shot_m, d.recognised_at_m);
    SC_EXPECT(d.recognised_at_m > 0.0);
    SC_EXPECT(!d.shot_before_vid);
    SC_EXPECT(d.first_shot_m > 0.0 && d.first_shot_m <= d.def.combat.fight.vid_m);
}

SC_TEST("air fight B8 live: under an E-3A that controls him he is cleared beyond sight and fires his radar missile first") {
    Duel d(true);
    d.Run(240.0);
    std::printf("    first shot at %.0f m (recognised at %.0f m); vectors sent %d, heard %d\n", d.first_shot_m, d.recognised_at_m, d.s.VectorsSent(),
                d.s.VectorsHeard());
    SC_EXPECT(d.s.VectorsHeard() > 0);
    SC_EXPECT(d.first_shot_m > d.def.combat.fight.vid_m && d.shot_before_vid);
    bool sarh = false;
    for (const AirFightSession::ShotRecord& r : d.s.Shots()) sarh = sarh || r.weapon.rfind("aim7", 0) == 0;
    SC_EXPECT(sarh);
}

SC_TEST("air fight B8 live: the same seed gives the same fight") {
    Duel a(true, 5), b(true, 5);
    a.Run(240.0);
    b.Run(240.0);
    SC_EXPECT(a.s.Shots().size() == b.s.Shots().size());
    for (std::size_t i = 0; i < std::min(a.s.Shots().size(), b.s.Shots().size()); ++i) {
        SC_EXPECT(a.s.Shots()[i].kill == b.s.Shots()[i].kill && a.s.Shots()[i].weapon == b.s.Shots()[i].weapon);
    }
    SC_EXPECT(a.s.Kills().size() == b.s.Kills().size());
}

SC_TEST("air fight B8 live: a Pact pilot whose GCI is not heard loses control: the green one goes home, the skilled one on in pairs") {
    for (const double skill : {0.3, 0.8}) {
        AirFightDef def = Def();
        HandWorld w;
        w.heard_at = [](int) { return -1.0; };  // every vector jammed out
        w.Add(Vec3{0.0, 0.0, 5000.0}, Vec3{0.0, 230.0, 0.0});
        w.Add(Vec3{0.0, 140000.0, 5000.0}, Vec3{0.0, -200.0, 0.0});  // ours, beyond his radar
        def.fighters.push_back(Fighter("flogger", "mig23ml_wp", "warsaw_pact", "sweep", "gci", skill));
        def.fighters.push_back(Fighter("red", "mig21mf", "ours", "strike"));
        AirFightControllerDef c;
        c.id = "gci";
        c.side = "enemy";
        c.kind = "radar_control";
        c.pos = Vec3{0.0, -60000.0, 0.0};
        c.range_m = 300000.0;
        c.post = "gci";
        def.controllers.push_back(c);
        AirFightSession s;
        std::string e;
        SC_EXPECT(s.Init(def, {"flogger", "red"}, 3, &e));
        for (double t = 0.25; t <= 90.0; t += 0.25) {
            w.Move(0.25);
            s.Step(t, w.World());
        }
        std::printf("    skill %.1f: vectors sent %d heard %d, control lost %d, mode %s (%s)\n", skill, s.VectorsSent(), s.VectorsHeard(),
                    s.ControlLost("flogger") ? 1 : 0, AirFightSession::ModeName(s.ModeOf("flogger")), s.WhyHome("flogger").c_str());
        SC_EXPECT(s.VectorsSent() > 0 && s.VectorsHeard() == 0 && s.ControlLost("flogger"));
        if (skill < 0.5) SC_EXPECT(s.ModeOf("flogger") == AirFightSession::Mode::Home && s.WhyHome("flogger") == "lost control");
        else SC_EXPECT(s.ModeOf("flogger") != AirFightSession::Mode::Home);
    }
}

SC_TEST("air fight B8 live: in a session our fighter under its GCI closes on an enemy jet and fires; a kill names the regiment") {
    std::string text = ReadFile(kData + "/scenarios/osa_practice.json");
    const std::size_t a = text.find("\"targets\"");
    const std::size_t b = text.find("\"triggers\"");
    SC_EXPECT(a != std::string::npos && b != std::string::npos && b > a);
    if (a == std::string::npos || b == std::string::npos) return;
    const std::string targets = R"j("targets": [
    { "id": "red11", "name": "Our MiG-21MF (placeholder)", "type": "Air+FixedWing", "class": "fighter", "iff": "friend",
      "start": { "east_m": 0, "north_m": 40000, "up_m": 5000 }, "waypoints": [ { "east_m": 0, "north_m": 140000, "up_m": 5000 } ],
      "speed_mps": 230, "rcs_m2": 3 },
    { "id": "t1", "name": "An enemy strike jet (placeholder)", "type": "Air+FixedWing", "class": "fighter",
      "start": { "east_m": 3000, "north_m": 110000, "up_m": 4000 }, "waypoints": [ { "east_m": 3000, "north_m": 20000, "up_m": 4000 } ],
      "speed_mps": 220, "rcs_m2": 6 } ],
  "air_fight": { "year": 1985, "source": "sam3d", "confidence": "estimate", "notes": "a placeholder fight for the test",
    "fighters": [ { "target": "red11", "type": "mig21mf", "faction": "ours", "role": "intercept", "regiment": "fighter_rgt_1", "controller": "gci" },
                  { "target": "t1", "type": "su24", "faction": "warsaw_pact", "role": "strike" } ],
    "controllers": [ { "id": "gci", "side": "ours", "kind": "radar_control", "east_m": 0, "north_m": 0, "range_m": 200000 } ] },
  )j";
    text = text.substr(0, a) + targets + text.substr(b);
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    const bool ok = LoadScenario(text, kData, def, console, &e);
    if (!ok) std::printf("    %s\n", e.c_str());
    SC_EXPECT(ok && def.air_fight.present && def.air_fight.fighters.size() == 2);
    if (!ok) return;
    ScenarioRunner r(std::move(def), std::move(console));
    while (r.Time() < 400.0) {
        r.Step(0.1);
        if (r.Time() > 1.0 && (!r.TargetAlive("t1") || !r.TargetAlive("red11"))) break;
    }
    const AirFightSession* af = r.AirFight();
    SC_EXPECT(af != nullptr);
    if (af == nullptr) {
        for (const DebriefEntry& en : r.Log().Entries()) {
            if (en.what.find("air") != std::string::npos) std::printf("    %s: %s\n", en.what.c_str(), en.detail.c_str());
        }
        return;
    }
    std::printf("    %.0f s: our MiG fired %d, vectors heard %d, kills %zu\n", r.Time(), af->ShotsBy("red11"), af->VectorsHeard(), af->Kills().size());
    SC_EXPECT(af->VectorsHeard() > 0 && af->ShotsBy("red11") > 0);
    for (const AirFightSession::Kill& k : af->Kills()) {
        if (k.shooter == "red11") SC_EXPECT(k.by == "fighter_rgt_1" && !r.TargetAlive(k.target));
    }
    // The shots in the presentation feed (roadmap B13): each launch from the shooter's place toward its target, with a shape
    // the munitions library draws; each end matched to its launch, a hit where the target is and before its wreck.
    {
        std::vector<fx::Event> launches, ends, bursts;
        std::vector<fx::Event> downs;
        std::size_t early = 0;
        for (const fx::Event& ev : r.Fx().Since(0)) {
            if (ev.kind == fx::EventKind::AamLaunch) launches.push_back(ev);
            if (ev.kind == fx::EventKind::AamEnd) ends.push_back(ev);
            if (ev.kind == fx::EventKind::AamEnd && ev.end == fx::BurstEnd::Early) ++early;
            if (ev.kind == fx::EventKind::AirGunBurst) bursts.push_back(ev);
            if (ev.kind == fx::EventKind::AircraftDown) downs.push_back(ev);
        }
        std::printf("    feed: %zu launches, %zu ends, %zu bursts\n", launches.size(), ends.size(), bursts.size());
        SC_EXPECT(static_cast<int>(launches.size() + bursts.size()) >= af->ShotsBy("red11"));
        SC_EXPECT(ends.size() - early + bursts.size() == af->Shots().size());
        scenery::MunitionLibrary lib;
        SC_EXPECT(lib.Load(kData + "/scenery/munitions.json"));
        for (const fx::Event& l : launches) {
            const scenery::MunitionSpec* m = lib.Resolve(l.weapon);
            SC_EXPECT(m != nullptr && m->kind == "missile" && m->HasMesh() && m->has_motor);
            SC_EXPECT(!l.source.empty() && !l.target.empty() && l.world_id >= 0 && l.target_world_id >= 0 && l.world_id != l.target_world_id);
            SC_EXPECT(l.end_t_s > l.t_s && (l.aim - l.position).Length() > 100.0);
        }
        for (const fx::Event& en : ends) {
            int matched = 0;
            for (const fx::Event& l : launches) matched += l.source == en.source && l.number == en.number && l.seq < en.seq ? 1 : 0;
            SC_EXPECT(matched == 1);
            if (en.end == fx::BurstEnd::Hit) {
                SC_EXPECT(en.position.Length() > 0.0 && (en.aim - en.position).Length() < 1e-9);
                bool wreck_after = false;  // the burst before the wreck it makes
                for (const fx::Event& d : downs) wreck_after = wreck_after || (d.world_id == en.target_world_id && d.seq > en.seq);
                SC_EXPECT(wreck_after);
            }
        }
        for (const fx::Event& gb : bursts) SC_EXPECT(lib.Resolve(gb.weapon) != nullptr && lib.Resolve(gb.weapon)->kind == "gun" && gb.end_t_s > gb.t_s);
    }
    // The loader refuses a fighter with no target, an unknown doctrine and a controller that is not there.
    const auto refuse = [&](const std::string& from, const std::string& to, const std::string& expect) {
        std::string t2 = text;
        const std::size_t at = t2.find(from);
        if (at == std::string::npos) return false;
        t2.replace(at, from.size(), to);
        ScenarioDef d2;
        ConsoleData c2;
        std::string e2;
        const bool ok2 = LoadScenario(t2, kData, d2, c2, &e2);
        if (ok2 || e2.find(expect) == std::string::npos) std::printf("    expected \"%s\", got \"%s\"\n", expect.c_str(), e2.c_str());
        return !ok2 && e2.find(expect) != std::string::npos;
    };
    SC_EXPECT(refuse(R"("target": "t1")", R"("target": "nobody")", "no target"));
    SC_EXPECT(refuse(R"("faction": "warsaw_pact")", R"("faction": "martians")", "no doctrine"));
    SC_EXPECT(refuse(R"("controller": "gci")", R"("controller": "elsewhere")", "no controller"));
}

SC_TEST("air fight B13: the presentation hooks are told every launch, end and burst, and change nothing in the fight") {
    for (const bool e3a : {false, true}) {
        Duel plain(e3a, 7), shown(e3a, 7);
        shown.fx = true;
        plain.Run(240.0);
        shown.Run(240.0);
        // The same fight with the hooks as without.
        SC_EXPECT(plain.s.Shots().size() == shown.s.Shots().size() && plain.s.Kills().size() == shown.s.Kills().size());
        for (std::size_t i = 0; i < std::min(plain.s.Shots().size(), shown.s.Shots().size()); ++i) {
            SC_EXPECT(plain.s.Shots()[i].weapon == shown.s.Shots()[i].weapon && plain.s.Shots()[i].kill == shown.s.Shots()[i].kill &&
                      plain.s.Shots()[i].t_s == shown.s.Shots()[i].t_s);
        }
        // Every resolved shot is a missile's end or a gun's burst, in the same order and with the same outcome.
        std::size_t missiles = 0, guns = 0;
        std::vector<Duel::FxShot> resolved;  // the ends of missiles that met their target (a gone target's are not shots)
        for (const Duel::FxShot& en : shown.ends) {
            if (!en.gone) resolved.push_back(en);
        }
        for (const AirFightSession::ShotRecord& r : shown.s.Shots()) {
            const bool gun = shown.def.combat.Gun(r.weapon) != nullptr;
            if (gun) {
                SC_EXPECT(guns < shown.bursts.size() && shown.bursts[guns].kill == r.kill && shown.bursts[guns].t == r.t_s &&
                          shown.bursts[guns].until > r.t_s);
                ++guns;
            } else {
                SC_EXPECT(missiles < resolved.size() && resolved[missiles].kill == r.kill && resolved[missiles].t == r.t_s);
                ++missiles;
            }
        }
        SC_EXPECT(guns == shown.bursts.size());
        // Each end follows its launch (the same shooter and shot), whose time it was told then; every launch names its shape.
        for (const Duel::FxShot& en : shown.ends) {
            bool found = false;
            for (const Duel::FxShot& l : shown.launches) {
                if (l.shooter == en.shooter && l.shot == en.shot && l.target == en.target) {
                    found = true;
                    SC_EXPECT(l.t < en.t && en.t >= l.until - 1e-9 && en.t - l.until < 0.26);  // resolved on the first step past its arrival
                }
            }
            SC_EXPECT(found);
        }
        for (const Duel::FxShot& l : shown.launches) SC_EXPECT(!l.weapon.empty());
        std::printf("    %s: %zu launches, %zu ends, %zu bursts\n", e3a ? "under an E-3A" : "alone", shown.launches.size(), shown.ends.size(),
                    shown.bursts.size());
        SC_EXPECT(!shown.launches.empty() && shown.ends.size() <= shown.launches.size());
        SC_EXPECT(missiles + guns == shown.s.Shots().size());
    }
}

SC_TEST("air fight B13: the air battle shot check's scenario does in the first minutes what -SimAirShots photographs") {
    // scenarios/air_battle_shots.json (unreal/README.md, -SimAirShots): our pair and an F-15C fire missiles, the F-4Es
    // under the Osa's fire turn, dispense chaff and jettison, our fighters in the air: each at least once in the first
    // seven minutes. Flares (an IR SAM's launch seen: a neighbour fires only in a session with waves) and guns (a fight
    // closed to gun range) are photographed when they come and logged as not seen when they do not.
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    const bool ok = LoadScenario(ReadFile(kData + "/scenarios/air_battle_shots.json"), kData, def, console, &e);
    if (!ok) std::printf("    %s\n", e.c_str());
    SC_EXPECT(ok && def.air_fight.present);
    if (!ok) return;
    ScenarioRunner r(std::move(def), std::move(console));
    double banked_s = -1.0;
    bool ours_up = false;
    std::map<std::string, int> n;
    std::map<std::string, double> first;
    std::uint64_t cursor = 0;
    while (r.Time() < 420.0 && !r.Finished()) {
        r.Step(0.1);
        for (const Aircraft& a : r.GetWorld().AircraftList()) {
            if (!a.alive) continue;
            if (banked_s < 0.0 && std::fabs(a.bank_rad) >= DegToRad(30.0)) banked_s = r.Time();
        }
        ours_up = ours_up || r.TargetAlive("red11");
        for (const fx::Event& ev : r.Fx().Since(cursor)) {
            std::string k = fx::ToString(ev.kind);
            if (ev.kind == fx::EventKind::WeaponRelease) k = ev.delivery == "jettison" ? "jettison" : "release";
            if (ev.kind == fx::EventKind::AamEnd && ev.end == fx::BurstEnd::Early) k = "aam_end_early";
            if (n[k]++ == 0) first[k] = ev.t_s;
        }
        cursor = r.Fx().Last();
    }
    std::printf("    %.0f s (%s); banked at %.0f s;", r.Time(), r.Finished() ? r.EndReason().c_str() : "running", banked_s);
    for (const auto& kv : n) std::printf(" %s %d (%.0f s)", kv.first.c_str(), kv.second, first[kv.first]);
    std::printf("\n");
    SC_EXPECT(ours_up && banked_s >= 0.0);
    for (const char* k : {"chaff_drop", "jettison", "aam_launch", "aam_end"}) {
        if (n[k] == 0) std::printf("    none: %s\n", k);
        SC_EXPECT(n[k] > 0);
    }
}

SC_TEST("air fight B13: the shot check's second scenario gives flares in its first three minutes, by the reaction chain") {
    // scenarios/air_battle_flares.json (-SimAirShots plays it after air_battle_shots.json): NATO's waves low over a Strela-2
    // team. A pilot who sees its launch fires flares by his reactions (gameplay/reactions.json sam_ir: flares); each salvo
    // is the chain's own (its count), at an aircraft of the waves, near the battery where the check's camera goes.
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    const bool ok = LoadScenario(ReadFile(kData + "/scenarios/air_battle_flares.json"), kData, def, console, &e);
    if (!ok) std::printf("    %s\n", e.c_str());
    SC_EXPECT(ok && def.waves != nullptr && !def.neighbours.empty());
    if (!ok) return;
    ScenarioRunner r(std::move(def), std::move(console));
    int launches = 0, flares = 0;
    double first_s = -1.0, first_range_m = 0.0;
    std::uint64_t cursor = 0;
    while (r.Time() < 180.0 && !r.Finished()) {
        r.Step(0.1);
        for (const fx::Event& ev : r.Fx().Since(cursor)) {
            if (ev.kind == fx::EventKind::SamLaunch) ++launches;
            if (ev.kind != fx::EventKind::FlareDrop) continue;
            SC_EXPECT(ev.number > 0 && ev.world_id >= 0);
            if (flares++ == 0) {
                first_s = ev.t_s;
                first_range_m = std::hypot(ev.position.x, ev.position.y);
            }
        }
        cursor = r.Fx().Last();
    }
    std::printf("    %d IR SAM launches, %d flare salvos (the chain's %d), the first at %.0f s, %.1f km from the battery\n", launches,
                flares, r.Reactions().Totals().flares, first_s, first_range_m / 1000.0);
    SC_EXPECT(launches > 0);
    SC_EXPECT(flares > 0 && first_s > 0.0 && first_s < 180.0);
    SC_EXPECT(flares == r.Reactions().Totals().flares);
    SC_EXPECT(first_range_m < 8000.0);
}

// ------------------------------------------------------------------------------------------------ roadmap S3

namespace {

// S3's def: the fight's data, each radar's look-down from its EM entry, the factions' vector lines.
AirFightDef S3Def() {
    AirFightDef d = Def(1);
    std::string e;
    if (!FillLookDown(d.sensors, kData, &e)) std::printf("    %s\n", e.c_str());
    for (const char* f : {"nato", "warsaw_pact", "neutral"}) VectorLinesFromJson(ReadFile(kData + "/air_war/faction_" + f + ".json"), d.vector_lines[f]);
    return d;
}

// A world for S3: aircraft that turn at 12 deg/s toward the point they are steered at and fly the speed asked; eyes to
// 9 km (the type within 5 km); a receiver that hears a radar of the other side locked on him, and a launch while a radar
// round flies at him; launches seen within 10 km.
struct S3World {
    std::vector<Aircraft> air;
    std::vector<std::vector<Sighting>> pictures;
    std::vector<bool> gone;
    std::vector<int> locked_by;
    struct RadarCall {
        int target = -1, state = 0, locked = -1;
    };
    std::vector<RadarCall> radar_calls;
    struct Launch {
        int shooter = -1, target = -1;
        bool radar = false, done = false;
    };
    std::vector<Launch> launches;
    std::vector<std::string> events;
    std::function<bool(int)> far_from_ours = [](int) { return false; };
    std::function<int(int)> leader = [](int) { return -1; };
    std::function<bool(int, bool, double)> dispense;
    std::function<double(int, bool, double)> countermeasure;

    int Add(const Vec3& at, const Vec3& vel) {
        Aircraft a;
        a.position = at;
        a.velocity = vel;
        a.speed_mps = vel.Length();
        a.max_turn_rate_rad_s = DegToRad(12.0);
        a.max_speed_mps = 1.5 * vel.Length();
        a.rcs_m2 = 5.0;
        a.max_load_g = 7.0;
        air.push_back(a);
        pictures.emplace_back();
        gone.push_back(false);
        locked_by.push_back(-1);
        return static_cast<int>(air.size()) - 1;
    }
    AirFightWorld World() {
        AirFightWorld w;
        w.air = [this](int t) -> const Aircraft* {
            return t >= 0 && static_cast<std::size_t>(t) < air.size() && !gone[static_cast<std::size_t>(t)] ? &air[static_cast<std::size_t>(t)] : nullptr;
        };
        w.picture = [this](int t) -> const std::vector<Sighting>* { return &pictures[static_cast<std::size_t>(t)]; };
        w.altitude = [](const Vec3& p) { return p.z; };
        w.agl = [](const Vec3& p) { return p.z; };
        w.steer = [this](int t, const PilotOverride& ov) {
            Aircraft& a = air[static_cast<std::size_t>(t)];
            a.waypoints = {ov.point};  // where he is steered (read by Move)
            if (ov.speed_mps > 0.0) a.speed_mps = ov.speed_mps;
        };
        w.release = [](int) {};
        w.destroy = [this](int t, const std::string&) {
            gone[static_cast<std::size_t>(t)] = true;
            return true;
        };
        w.event = [this](const std::string& who, const std::string& what) { events.push_back(who + ": " + what); };
        w.say = [](const std::string&, int, const std::string&, double) { return 7; };
        w.heard = [](int, int, double t) { return t; };
        w.radar = [this](int t, const std::string&, int state, int locked) {
            radar_calls.push_back({t, state, locked});
            for (int& by : locked_by) {
                if (by == t) by = -1;
            }
            if (state == 2 && locked >= 0) locked_by[static_cast<std::size_t>(locked)] = t;
        };
        w.fx_launch = [this](int shooter, int target, const AamDef& aam, int, double, double) {
            launches.push_back({shooter, target, IsSemiActive(aam.guidance), false});
        };
        w.fx_end = [this](int shooter, int target, const AamDef&, int, double, bool, bool) {
            for (Launch& l : launches) {
                if (l.shooter == shooter && l.target == target) l.done = true;
            }
        };
        w.leader = [this](int t) { return leader(t); };
        w.far_from_ours = [this](int t) { return far_from_ours(t); };
        if (dispense) w.dispense = [this](int t, bool ir, double at) { return dispense(t, ir, at); };
        if (countermeasure) w.countermeasure = [this](int t, bool ir, double at) { return countermeasure(t, ir, at); };
        return w;
    }
    void Move(double dt) {
        for (Aircraft& a : air) {
            const Vec3 h{a.velocity.x, a.velocity.y, 0.0};
            double hd = std::atan2(h.x, h.y);
            if (!a.waypoints.empty()) {
                const Vec3 d = a.waypoints.front() - a.position;
                const double turn = std::remainder(std::atan2(d.x, d.y) - hd, kTwoPi);
                const double step = a.max_turn_rate_rad_s * dt;
                hd += std::max(-step, std::min(step, turn));
            }
            const double v = h.Length() + std::max(-10.0 * dt, std::min(10.0 * dt, a.speed_mps - h.Length()));
            a.velocity = Vec3{v * std::sin(hd), v * std::cos(hd), 0.0};
            a.position += a.velocity * dt;
        }
    }
    void See(double t) {
        for (std::size_t i = 0; i < air.size(); ++i) {
            std::vector<Sighting>& pic = pictures[i];
            pic.erase(std::remove_if(pic.begin(), pic.end(), [&](const Sighting& s) { return s.kind != SightKind::LaunchSign || t - s.time_s > 5.0; }),
                      pic.end());
            for (std::size_t j = 0; j < air.size(); ++j) {
                if (j == i || gone[j]) continue;
                const double r = (air[j].position - air[i].position).Length();
                if (r >= 9000.0) continue;
                Sighting s;
                s.kind = SightKind::Aircraft;
                s.believed_enu = air[j].position;
                s.range_est_m = r;
                s.time_s = t;
                s.recognised = r < 5000.0;
                s.what = s.recognised ? "type" : "aircraft";
                pic.push_back(s);
            }
            const int by = locked_by[i];
            if (by >= 0 && !gone[static_cast<std::size_t>(by)]) {
                Sighting s;
                s.kind = SightKind::Emitter;
                s.by = SightBy::Rwr;
                s.rwr = RwrState::Lock;
                for (const Launch& l : launches) {
                    if (!l.done && l.radar && l.shooter == by && l.target == static_cast<int>(i)) s.rwr = RwrState::Launch;
                }
                const Vec3 d = air[static_cast<std::size_t>(by)].position - air[i].position;
                s.bearing_rad = std::atan2(d.x, d.y);
                s.time_s = t;
                pic.push_back(s);
            }
        }
    }
    // The fight from `t` to `until_s`, seeing and moving each 0.25 s; `each` after every step.
    void Run(AirFightSession& s, double& t, double until_s, const std::function<void()>& each = nullptr) {
        while (t < until_s) {
            t += 0.25;
            Move(0.25);
            See(t);
            s.Step(t, World());
            if (each) each();
        }
    }
};

AirFightControllerDef Ctl(const std::string& id, const std::string& side, const std::string& kind, const Vec3& at) {
    AirFightControllerDef c;
    c.id = id;
    c.side = side;
    c.kind = kind;
    c.pos = at;
    c.range_m = 400000.0;
    return c;
}

}  // namespace

SC_TEST("air fight S3: each radar's look-down is its EM entry's; a radar's range looking down follows it") {
    const AirFightDef d = S3Def();
    const auto radar = [&](const std::string& type) -> const SensorDef* {
        const std::vector<const SensorDef*> v = d.sensors.Of(type, SensorKind::Radar, 1985, false);
        return v.empty() ? nullptr : v.front();
    };
    const SensorDef* apg63 = radar("f15c");
    const SensorDef* apq120 = radar("f4e");
    SC_EXPECT(apg63 != nullptr && apq120 != nullptr);
    if (apg63 == nullptr || apq120 == nullptr) return;
    std::printf("    look-down: APG-63 class %.0f dB, APQ-120 class %.0f dB\n", apg63->look_down_db, apq120->look_down_db);
    SC_EXPECT_NEAR(apg63->look_down_db, 40.0, 1e-9);
    SC_EXPECT_NEAR(apq120->look_down_db, 0.0, 1e-9);
    // 48 dB of clutter less the APG-63's 40 dB: 10^(-8/40) of 80 km, sensors.json's 50 km.
    SC_EXPECT_NEAR(80.0 * std::pow(10.0, -(d.combat.fight.look_down_clutter_db - 40.0) / 40.0), 50.0, 1.0);
}

SC_TEST("air fight S3: who may vector whom: NATO's CRC its CAP but no sweep, its E-3A any fighter; the Pact's GCI all of its own") {
    AirFightDef d = S3Def();
    SC_EXPECT(!d.MayVector("nato", "radar_control", "sweep") && !d.MayVector("nato", "radar_control", "escort"));
    SC_EXPECT(d.MayVector("nato", "radar_control", "cap") && d.MayVector("nato", "airborne_warning", "escort"));
    SC_EXPECT(d.MayVector("warsaw_pact", "radar_control", "escort") && !d.MayVector("warsaw_pact", "airborne_warning", "sweep"));
    SC_EXPECT(d.MayVector("ours", "radar_control", "intercept"));  // ours keep B10's assignment
    d.year = 1981;
    SC_EXPECT(!d.MayVector("nato", "airborne_warning", "sweep"));  // the E-3A's line from 1982
    d.mode = 0;
    SC_EXPECT(d.MayVector("nato", "radar_control", "sweep"));  // B8: every assigned fighter
    // In the fight: a NATO sweep given to a ground controller hears no vector; under the E-3A he does.
    for (const bool e3a : {false, true}) {
        AirFightDef def = S3Def();
        S3World w;
        w.Add(Vec3{0.0, 0.0, 6000.0}, Vec3{0.0, 250.0, 0.0});
        w.Add(Vec3{0.0, 90000.0, 5000.0}, Vec3{0.0, -230.0, 0.0});
        def.controllers.push_back(Ctl("c", "enemy", e3a ? "airborne_warning" : "radar_control", Vec3{-100000.0, 0.0, e3a ? 9000.0 : 0.0}));
        def.fighters.push_back(Fighter("eagle", "f15c", "nato", "sweep", "c"));
        def.fighters.push_back(Fighter("red", "mig21bis", "ours", "strike"));
        AirFightSession s;
        std::string e;
        SC_EXPECT(s.Init(def, {"eagle", "red"}, 3, &e));
        double t = 0.0;
        w.Run(s, t, 60.0);
        std::printf("    %s: vectors sent %d\n", e3a ? "E-3A" : "CRC", s.VectorsSent());
        SC_EXPECT(e3a ? s.VectorsSent() > 0 : s.VectorsSent() == 0);
    }
}

SC_TEST("air fight S3: a fighter's radar radiates: off while his GCI brings him in from afar, searching, then locked on the one he engages") {
    AirFightDef def = S3Def();
    S3World w;
    w.Add(Vec3{0.0, 0.0, 5000.0}, Vec3{0.0, 230.0, 0.0});        // the Pact's MiG-23ML
    w.Add(Vec3{0.0, 110000.0, 5000.0}, Vec3{0.0, -220.0, 0.0});  // ours, a striker that flies on
    def.controllers.push_back(Ctl("gci", "enemy", "radar_control", Vec3{0.0, -60000.0, 0.0}));
    def.fighters.push_back(Fighter("flogger", "mig23ml_wp", "warsaw_pact", "sweep", "gci"));
    def.fighters.push_back(Fighter("red", "su24", "ours", "strike"));
    AirFightSession s;
    std::string e;
    SC_EXPECT(s.Init(def, {"flogger", "red"}, 4, &e));
    double t = 0.0;
    double on_at_m = -1.0, lock_at_m = -1.0;
    bool striker_radiated = false;
    w.Run(s, t, 240.0, [&]() {
        const double range = (w.air[1].position - w.air[0].position).Length();
        for (const S3World::RadarCall& r : w.radar_calls) {
            if (r.target == 1 && r.state > 0) striker_radiated = true;
            if (r.target != 0) continue;
            if (r.state >= 1 && on_at_m < 0.0) on_at_m = range;
            if (r.state == 2 && r.locked == 1 && lock_at_m < 0.0) lock_at_m = range;
        }
        w.radar_calls.clear();
    });
    std::printf("    the MiG-23's radar on at %.0f m, %d vectors heard, locked at %.0f m\n", on_at_m, s.VectorsHeard(), lock_at_m);
    // Silent from 110 km until the one he was told of is within the Sapfir-23ML's cued 82.5 km (sensors.json).
    SC_EXPECT(on_at_m > 0.0 && on_at_m < 86000.0);
    SC_EXPECT(s.VectorsHeard() > 0);
    SC_EXPECT(lock_at_m > 0.0 && lock_at_m <= on_at_m);
    SC_EXPECT(!striker_radiated);
}

SC_TEST("air fight S3: the attacks told apart: NATO sets up on the beam, the Pact slashes and extends, a turning doctrine goes straight in") {
    struct Want {
        const char* type;
        const char* faction;
        const char* ctl;
        bool setup;
    };
    for (const Want& k : {Want{"f15c", "nato", "airborne_warning", true}, Want{"mig23ml_wp", "warsaw_pact", "radar_control", true},
                          Want{"draken_gun", "neutral", "", false}}) {
        AirFightDef def = S3Def();
        S3World w;
        w.Add(Vec3{0.0, 0.0, 5000.0}, Vec3{0.0, 240.0, 0.0});
        w.Add(Vec3{2000.0, 70000.0, 5000.0}, Vec3{0.0, -220.0, 0.0});
        const std::string kind = k.ctl;
        if (!kind.empty()) def.controllers.push_back(Ctl("c", "enemy", kind, Vec3{-150000.0, 0.0, kind == "airborne_warning" ? 9000.0 : 0.0}));
        def.fighters.push_back(Fighter("a", k.type, k.faction, "sweep", kind.empty() ? "" : "c", 0.9));
        def.fighters.push_back(Fighter("red", "su24", "ours", "strike"));
        AirFightSession s;
        std::string e;
        SC_EXPECT(s.Init(def, {"a", "red"}, 6, &e));
        double t = 0.0;
        w.Run(s, t, 300.0);
        const bool setup = s.EverWas("a", AirFightSession::Mode::Setup);
        const bool extend = s.EverWas("a", AirFightSession::Mode::Extend);
        std::printf("    %s: set up %d, engaged %d, extended %d, shots %d\n", k.faction, setup ? 1 : 0, s.EverWas("a", AirFightSession::Mode::Engage) ? 1 : 0,
                    extend ? 1 : 0, s.ShotsBy("a"));
        SC_EXPECT(setup == k.setup);
        SC_EXPECT(extend == (std::string(k.faction) == "warsaw_pact"));
    }
}

SC_TEST("air fight S3: warned of a lock he cannot answer, a fighter drags at full speed; the drag ends and he comes back") {
    AirFightDef def = S3Def();
    S3World w;
    w.Add(Vec3{0.0, 0.0, 5000.0}, Vec3{0.0, 230.0, 0.0});
    w.Add(Vec3{0.0, 60000.0, 5000.0}, Vec3{0.0, -10.0, 0.0});  // the radar that locks him, far beyond his weapons
    def.fighters.push_back(Fighter("red", "mig21bis", "ours", "intercept", "", 0.8));
    def.fighters.push_back(Fighter("x", "f4e", "nato", "strike"));
    AirFightSession s;
    std::string e;
    SC_EXPECT(s.Init(def, {"red", "x"}, 2, &e));
    double t = 0.0;
    w.locked_by[0] = 1;  // the F-4's radar holds him
    w.Run(s, t, 5.0);
    SC_EXPECT(s.ModeOf("red") == AirFightSession::Mode::Drag && s.Tally().drags == 1);
    w.locked_by[0] = -1;
    w.Run(s, t, 40.0);
    std::printf("    drags %d, speed %.0f m/s, then %s\n", s.Tally().drags, w.air[0].speed_mps, AirFightSession::ModeName(s.ModeOf("red")));
    SC_EXPECT(s.ModeOf("red") != AirFightSession::Mode::Drag && s.Tally().drags == 1);
    SC_EXPECT(w.air[0].speed_mps > 300.0);  // he ran at full speed
}

SC_TEST("air fight S3: a striker who knows of the missile dispenses once before it arrives, and the decoy goes into the shot") {
    int dispensed = 0, asked = 0, asked_cm = 0, fired = 0;
    for (std::uint64_t seed = 1; seed <= 6; ++seed) {
        AirFightDef def = S3Def();
        S3World w;
        w.Add(Vec3{0.0, 0.0, 5000.0}, Vec3{0.0, 240.0, 0.0});       // ours, under its GCI
        w.Add(Vec3{0.0, 50000.0, 4000.0}, Vec3{0.0, -230.0, 0.0});  // a Tornado striking, with flares and chaff
        def.controllers.push_back(Ctl("gci", "ours", "radar_control", Vec3{0.0, -20000.0, 0.0}));
        def.fighters.push_back(Fighter("red", "mig23ml", "ours", "intercept", "gci", 0.8));
        def.fighters.push_back(Fighter("t", "tornado_ids", "nato", "strike"));
        w.dispense = [&](int target, bool, double) {
            ++asked;
            dispensed += target == 1 ? 1 : 0;
            return true;
        };
        w.countermeasure = [&](int, bool, double) {
            ++asked_cm;
            return 1.0;
        };
        AirFightSession s;
        std::string e;
        SC_EXPECT(s.Init(def, {"red", "t"}, seed, &e));
        double t = 0.0;
        w.Run(s, t, 240.0);
        fired += static_cast<int>(w.launches.size());
        SC_EXPECT(s.Tally().dispenses <= static_cast<int>(w.launches.size()));
    }
    std::printf("    %d rounds fired, the striker asked to dispense %d times, the decoy's strength read %d times\n", fired, asked, asked_cm);
    SC_EXPECT(fired > 0 && dispensed > 0 && dispensed <= fired && asked == dispensed);
    SC_EXPECT(asked_cm > 0);
}

SC_TEST("air fight S3: an escort with nothing to fight keeps his station behind, beside and above his strikers' leader") {
    AirFightDef def = S3Def();
    S3World w;
    w.Add(Vec3{0.0, 0.0, 1000.0}, Vec3{0.0, 220.0, 0.0});           // the strikers' leader
    w.Add(Vec3{-6000.0, -8000.0, 1000.0}, Vec3{0.0, 220.0, 0.0});   // his escort, out of place
    w.air[1].max_speed_mps = 400.0;
    def.fighters.push_back(Fighter("lead", "tornado_ids", "nato", "strike"));
    AirFightFighterDef esc = Fighter("cover", "f15c", "nato", "escort");
    esc.escorts = "lead";
    def.fighters.push_back(esc);
    AirFightSession s;
    std::string e;
    SC_EXPECT(s.Init(def, {"lead", "cover"}, 1, &e));
    double t = 0.0;
    w.Run(s, t, 240.0);
    const FightParams& fp = def.combat.fight;
    const Vec3 rel = w.air[1].position - w.air[0].position;  // north is forward, east his right
    std::printf("    escort of %s, mode %s, %.0f m behind, %.0f m to the side\n", s.EscortOf("cover").c_str(), AirFightSession::ModeName(s.ModeOf("cover")),
                -rel.y, std::fabs(rel.x));
    SC_EXPECT(s.EscortOf("cover") == "lead" && s.ModeOf("cover") == AirFightSession::Mode::Station);
    SC_EXPECT(std::fabs(-rel.y - fp.escort_back_m) < 1500.0 && std::fabs(std::fabs(rel.x) - fp.escort_side_m) < 1500.0);
}

SC_TEST("air fight S3: far from our ground units a pair fights on its leader's picture, thinking at A1; the fight goes on") {
    int thinks[2] = {0, 0}, senses[2] = {0, 0}, far_thinks[2] = {0, 0}, shots[2] = {0, 0};
    for (int far_fight = 0; far_fight <= 1; ++far_fight) {
        AirFightDef def = S3Def();
        def.far_fight = far_fight;
        S3World w;
        w.Add(Vec3{0.0, 0.0, 6000.0}, Vec3{0.0, 250.0, 0.0});
        w.Add(Vec3{1500.0, 0.0, 6000.0}, Vec3{0.0, 250.0, 0.0});
        w.Add(Vec3{0.0, 80000.0, 5000.0}, Vec3{0.0, -240.0, 0.0});
        w.Add(Vec3{1500.0, 80000.0, 5000.0}, Vec3{0.0, -240.0, 0.0});
        w.far_from_ours = [](int) { return true; };
        w.leader = [](int t) { return t % 2 == 1 ? t - 1 : -1; };
        def.controllers.push_back(Ctl("e3a", "enemy", "airborne_warning", Vec3{-150000.0, 0.0, 9000.0}));
        def.controllers.push_back(Ctl("gci", "ours", "radar_control", Vec3{0.0, 120000.0, 0.0}));
        def.fighters.push_back(Fighter("e1", "f15c", "nato", "sweep", "e3a"));
        def.fighters.push_back(Fighter("e2", "f15c", "nato", "sweep", "e3a"));
        def.fighters.push_back(Fighter("r1", "mig23ml", "ours", "intercept", "gci"));
        def.fighters.push_back(Fighter("r2", "mig23ml", "ours", "intercept", "gci"));
        AirFightSession s;
        std::string e;
        SC_EXPECT(s.Init(def, {"e1", "e2", "r1", "r2"}, 9, &e));
        double t = 0.0;
        w.Run(s, t, 300.0);
        thinks[far_fight] = s.Tally().thinks;
        far_thinks[far_fight] = s.Tally().thinks_far;
        senses[far_fight] = s.Tally().senses;
        shots[far_fight] = static_cast<int>(s.Shots().size());
    }
    std::printf("    near: %d thoughts, %d looks, %d shots; far: %d thoughts (%d at A1), %d looks, %d shots\n", thinks[0], senses[0], shots[0], thinks[1],
                far_thinks[1], senses[1], shots[1]);
    SC_EXPECT(far_thinks[0] == 0 && far_thinks[1] > 0);
    SC_EXPECT(thinks[1] < thinks[0] && senses[1] < senses[0]);
    SC_EXPECT(shots[1] > 0);
}

SC_TEST("air fight S3: a missile fired at the edge of its reach falls short of a target that turns and runs") {
    int energy = 0, fired = 0;
    // The Eagle goes straight in (a turning doctrine: no beam set-up, which would bring him to 15 km before he fires), so
    // he fires near the AIM-7's head-on reach; at the launch his target turns tail and outruns it.
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        AirFightDef def = S3Def();
        for (AirDoctrineDef& d : def.combat.doctrines) {
            if (d.faction == "nato") d.tactic = "turn";
        }
        S3World w;
        w.Add(Vec3{0.0, 0.0, 6000.0}, Vec3{0.0, 250.0, 0.0});
        w.Add(Vec3{0.0, 70000.0, 6000.0}, Vec3{0.0, -250.0, 0.0});
        def.controllers.push_back(Ctl("e3a", "enemy", "airborne_warning", Vec3{-150000.0, 0.0, 9000.0}));
        def.fighters.push_back(Fighter("eagle", "f15c", "nato", "sweep", "e3a"));
        def.fighters.push_back(Fighter("red", "su24", "ours", "strike"));
        AirFightSession s;
        std::string e;
        SC_EXPECT(s.Init(def, {"eagle", "red"}, seed, &e));
        double t = 0.0;
        bool ran = false;
        w.Run(s, t, 200.0, [&]() {
            if (ran || w.launches.empty()) return;
            ran = true;  // at the first launch the target turns tail and runs at 330 m/s
            std::printf("    seed %d: fired at %.0f m, t %.0f s\n", static_cast<int>(seed), (w.air[1].position - w.air[0].position).Length(), t);
            w.air[1].velocity = Vec3{0.0, 330.0, 0.0};
            w.air[1].speed_mps = 330.0;
            w.air[1].waypoints = {w.air[1].position + Vec3{0.0, 500000.0, 0.0}};
        });
        fired += static_cast<int>(w.launches.size());
        for (const AirFightSession::ShotRecord& r : s.Shots()) {
            energy += r.reason == MissReason::Energy ? 1 : 0;
            std::printf("      %s at %.0f s: %s\n", r.weapon.c_str(), r.t_s, MissReasonName(r.reason));
        }
    }
    std::printf("    %d rounds, %d out of energy\n", fired, energy);
    SC_EXPECT(fired > 0 && energy > 0);
}

SC_TEST("air fight S3 (A0): the drag, the slash's extension and the strikers' decoys come only with air_fight.mode 1") {
    for (const char* faction : {"nato", "warsaw_pact"}) {
        const AirWarData d1 = LoadWar(faction);
        AirWarData d0 = d1;
        d0.tuning.values["air_fight.mode"] = 0.0;
        SC_EXPECT_NEAR(d1.tuning.Get("air_fight.mode"), 1.0, 1e-12);
        const bool nato = std::string(faction) == "nato";
        Package p;
        p.flights = {MakeFlight(FlightRole::Strike, nato ? "tornado_ids" : "su24", 4, Profile::Medium),
                     MakeFlight(FlightRole::Sweep, nato ? "f15c" : "mig23ml_wp", 2, Profile::Medium)};
        int drag[2] = {0, 0}, ext[2] = {0, 0}, disp[2] = {0, 0};
        for (int mode = 0; mode <= 1; ++mode) {
            const AbstractAirFight fight(mode == 0 ? d0 : d1);
            for (std::uint64_t k = 1; k <= 3000; ++k) {
                const AbstractFightResult r = fight.Resolve(p, {4, 2}, Day(1.0, nato ? 1.0 : 0.0), k * 7919u);
                drag[mode] += r.dragged;
                ext[mode] += r.extended;
                disp[mode] += r.dispensed;
            }
        }
        std::printf("    %s: mode 0 drags %d extends %d decoys %d; mode 1 drags %d extends %d decoys %d\n", faction, drag[0], ext[0], disp[0], drag[1],
                    ext[1], disp[1]);
        SC_EXPECT(drag[0] == 0 && ext[0] == 0 && disp[0] == 0);
        SC_EXPECT(ext[1] > 0);  // ours slash
        if (nato) SC_EXPECT(drag[1] > 0 && disp[1] > 0);  // F-15s drag our radar rounds; the Tornados carry flares and chaff
    }
}

SC_TEST("air fight S3: a save keeps the air fight's mode; a save made before S3 replays B8's abstract fight") {
    AirCampaign c;
    std::string e;
    SC_EXPECT(c.Init(LoadWar("nato"), 7, &e));
    const std::string saved = SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_nato.json");
    SC_EXPECT(saved.find("\"air_fight_mode\"") != std::string::npos);
    AirCampaign back;
    SC_EXPECT(LoadCampaignJson(saved, kData, back, &e));
    SC_EXPECT_NEAR(back.Data().tuning.Get("air_fight.mode"), 1.0, 1e-12);
    AirCampaign old;
    SC_EXPECT(LoadCampaignJson(ReadFile(kData + "/../simcore/tests/fixtures/air_war_save_v1.json"), kData, old, &e));
    SC_EXPECT_NEAR(old.Data().tuning.Get("air_fight.mode"), 0.0, 1e-12);
}

SC_TEST("air fight S3: the scenario's air_fight takes its mode and the far fight, loads the radars and vector lines; bad ones are refused") {
    // The shot check's scenario keeps B8's fight ("mode": 0, what -SimAirShots photographs); without the key, S3's.
    std::string text = ReadFile(kData + "/scenarios/air_battle_shots.json");
    {
        ScenarioDef as_is;
        ConsoleData c0;
        std::string e0;
        SC_EXPECT(LoadScenario(text, kData, as_is, c0, &e0) && as_is.air_fight.mode == 0);
        const std::size_t mk = text.find("\"mode\": 0,");
        SC_EXPECT(mk != std::string::npos);
        if (mk != std::string::npos) text.erase(mk, 10);
    }
    const std::size_t at = text.find("\"air_fight\"");
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    const std::size_t brace = text.find('{', at);
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(text, kData, def, console, &e));
    SC_EXPECT(def.air_fight.mode == 1 && def.air_fight.FarFight());
    SC_EXPECT(def.air_fight.radar_library.radars.size() >= 5);
    std::string zero = text;
    zero.insert(brace + 1, " \"mode\": 0, \"far_fight\": 1,");
    ScenarioDef d0;
    SC_EXPECT(LoadScenario(zero, kData, d0, console, &e));
    SC_EXPECT(d0.air_fight.mode == 0 && d0.air_fight.FarFight());
    std::string bad = text;
    bad.insert(brace + 1, " \"mode\": 2,");
    ScenarioDef d2;
    SC_EXPECT(!LoadScenario(bad, kData, d2, console, &e));
    SC_EXPECT(e.find("mode") != std::string::npos);
}
