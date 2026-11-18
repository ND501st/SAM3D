// air_war_bench: the air war's weapons, attacks and losses against what history recorded (docs/air_war/09_BALANCE.md
// section 9, docs/research/AIR_WAR_MISSIONS_2026-10.md).
//
// Usage: air_war_bench [data_dir] [runs] [--why] [--set tuning.key=value ...]
//   --why: the losses by role and defence after each benchmark; --set: try a tuning value (a calibration sweep).
//
// Each benchmark sets up a small theatre (a target and the defences around it, with the theatre's own values for its
// guns and SAMs), flies a historical analogue through the abstract resolver (resolve.hpp) many times, and prints what
// the model gives beside what history recorded:
//   1. a massive bridge: unguided bombs in dives against laser-guided bombs (the Thanh Hoa bridge stood through 871
//      sorties, then fell to one strike of 14 F-4s with 24 laser-guided bombs, 13 May 1972);
//   2. accuracy: the misses of unguided and laser-guided bombs (1972-73: 447 ft against 23 ft on average);
//   3. low against medium altitude over an airfield's guns and shoulder-fired missiles, its radar SAM suppressed
//      (Desert Storm: 18 losses in the low-level first week, 22 in the five weeks at medium altitude after);
//   4. the runway: JP233 laid along it, runway-piercing bombs in dives, 2,000 lb bombs from medium altitude
//      (Operation Focus cut the runways of some ten airfields with a few flights each);
//   5. anti-radiation missiles against crews that radiate and crews that switch off (Allied Force: some 743 HARMs,
//      3 of about 25 SA-6 batteries confirmed destroyed);
//   6. bombs dropped unretarded from low level (the Falklands: 11 of 25 went off);
//   7. a SAM belt without suppression (Yom Kippur's first three days: 50 aircraft in about 1,220 sorties, 4 %);
//   8. a full suppression package against batteries that radiate (the Bekaa, 9 June 1982: 17 of 19 batteries, no
//      aircraft lost to them);
//   9. the gun pass (roadmap B6, attack_pass.hpp GunPassEffect): two A-10s' ten GAU-8 passes on ten known, stationary,
//      undefended tanks (Array 20, 19 September 1979: 8 put out of action, 3 of them burnt), and the same on a column on
//      the move whose guns fire back.
// The figures are what the model makes of the analogue, not a claim it reproduces the battle: the settings are stated
// with each line. Then the hot phase (roadmap B14, docs/air_war/09_BALANCE.md section 17), each measure against its band
// in data/air_war/balance_bands.json: the strikers' answers to our fighters against the F-105s' records, the 1967
// missiles' hits and shots out of parameters against CHECO, exchange ratios against Vietnam's swing and the Bekaa,
// the retask latencies against doctrine, friendly fire against the Soviet exercises' 25 %. Exits 1 if the data fail to
// load, 2 if a measure is out of its band.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/command/data.hpp"
#include "simcore/command/own_air.hpp"
#include "simcore/command/own_flights.hpp"
#include "simcore/reactions.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

struct Bench {
    AirWarData d;
    std::vector<ObjectState> truth;
    BatteryPosture posture;
    AirStaff staff;
    Rng rng{1};
};

void Theatre(Bench& b, const std::vector<TheatreObject>& objects) {
    b.d.theatre.objects = objects;
    b.truth.clear();
    for (const TheatreObject& o : objects) {
        ObjectState s;
        s.id = o.id;
        s.pos = o.pos;
        s.missiles = o.defence.present ? o.defence.missiles : -1;
        b.truth.push_back(s);
    }
}

TheatreObject Target(ObjectKind kind, const char* cls = nullptr) {
    TheatreObject o;
    o.id = "target";
    o.name = "the target";
    o.kind = kind;
    if (cls != nullptr) o.has_target_class = ParseTargetClass(cls, o.target_class);
    return o;
}

// A defence with the theatre's values (data/air_war/theatre_pannonia.json): a Kub battery, light guns, a Strela-10.
TheatreObject Defence(const std::string& id, const Vec3& at, const char* kind) {
    TheatreObject o;
    o.id = id;
    o.name = id;
    o.pos = at;
    o.defence.present = true;
    o.defence.medium_cover = o.defence.radar = false;  // guns and an IR SAM; the Kub sets its own
    const std::string k = kind;
    if (k == "kub") {
        o.kind = ObjectKind::SamSite;
        o.defence.radius_m = 20000.0;
        o.defence.lethality = 0.25;
        o.defence.low_cover = o.defence.medium_cover = o.defence.radar = true;
        o.defence.missiles = 12;
    } else if (k == "strela") {
        o.kind = ObjectKind::SamSite;
        o.defence.radius_m = 5000.0;
        o.defence.lethality = 0.12;
        o.defence.low_cover = true;
        o.defence.missiles = 8;
    } else {
        o.kind = ObjectKind::Aaa;
        o.defence.radius_m = 2500.0;
        o.defence.lethality = 0.12;
        o.defence.low_cover = true;
        o.defence.missiles = -1;
    }
    return o;
}

// A flight from 80 km south to the target and back, as the planner flies it (planner.hpp MainRoute): through an
// initial point to its release point short of the target, then away (turned back short of a stand-off release).
Flight MakeFlight(const Bench& b, const char* type, FlightRole role, int count, Profile prof, const char* weapon, int weapons, Delivery del) {
    const AircraftType& a = *b.d.faction.Aircraft(type);
    const WeaponDef* w = weapon != nullptr ? b.d.Weapon(weapon) : nullptr;
    Flight f;
    f.callsign = a.id;
    f.role = role;
    f.aircraft = a.id;
    f.count = count;
    f.profile = prof;
    f.weapon = weapon != nullptr ? weapon : "";
    f.weapons = weapons;
    f.delivery = del;
    f.arms = (role == FlightRole::Sead ? a.arms * count : 0);
    const double agl = ProfileHeight(a, prof);
    const double rs = w != nullptr ? ReleaseShort(del, w, b.d.tuning) : 0.0;
    f.route = {{Vec3{0.0, -80000.0, 0.0}, agl, 0.0, "entry"},
               {Vec3{5000.0, -20000.0, 0.0}, agl, 0.0, "ip"},
               {Vec3{0.0, -rs, 0.0}, agl, 0.0, "target"},
               {rs > 1500.0 ? Vec3{12000.0, -rs - 2000.0, 0.0} : Vec3{12000.0, 4000.0, 0.0}, agl, 0.0, "egress"},
               {Vec3{40000.0, -80000.0, 0.0}, agl, 0.0, "exit"}};
    TimeRoute(f.route, 2, 1.0 + (role == FlightRole::Sead || role == FlightRole::Decoy ? -0.03 : 0.0), a.attack_mps);
    f.release_index = 2;
    return f;
}

Package MakePackage(DayPart part = DayPart::Day) {
    Package p;
    p.kind = MissionKind::Strike;
    p.part = part;
    p.tot_h = 1.0;
    p.target_object = "target";
    return p;
}

// With --why: the losses of the benchmark just run, by the role shot down and the defence that did it.
bool g_why = false;
std::map<std::string, int> g_losses;

void Why() {
    if (!g_why) return;
    std::printf("|   losses by role and defence:");
    for (const auto& kv : g_losses) std::printf(" %s %d;", kv.first.c_str(), kv.second);
    std::printf(" |||\n");
    g_losses.clear();
}

PackageOutcome Fly(Bench& b, Package p) {
    ResolveContext rc;
    rc.data = &b.d;
    rc.truth = &b.truth;
    rc.posture = &b.posture;
    rc.staff = &b.staff;
    rc.weather = airwar::Weather::Clear;
    rc.rng = &b.rng;
    for (Flight& f : p.flights) {
        f.aim_pos = b.truth.front().pos;
        f.aim_key = "target";
    }
    Resolver r;
    const PackageOutcome o = r.Resolve(p, rc);
    for (const LossRecord& l : o.losses) {
        g_losses[std::string(FlightRoleName(p.flights[static_cast<std::size_t>(l.flight)].role)) + " by " + (l.by.empty() ? "fighters" : l.by)] += 1;
    }
    return o;
}

double CepOf(const Bench& b, const char* type, const char* weapon, Delivery del) {
    const AircraftType* a = b.d.faction.Aircraft(type);
    const WeaponDef* w = b.d.Weapon(weapon);
    return a != nullptr && w != nullptr ? DeliveryCep(*w, del, *a, DayPart::Day, airwar::Weather::Clear, 0.5, b.d.tuning) : 0.0;
}

bool Load(Bench& b, const std::string& data_dir, const char* faction) {
    std::string e;
    if (!LoadAirWarData(data_dir, "air_war/theatre_pannonia.json", std::string("air_war/faction_") + faction + ".json", b.d, &e)) {
        std::fprintf(stderr, "%s\n", e.c_str());
        return false;
    }
    for (FrontDef& fr : b.d.theatre.fronts) fr.friendly_fighters = 0.0;  // ground fire only, as in the analogues
    Rng srng(5);
    b.staff.Build(b.d, srng, 0.0);
    return true;
}

double Within(double r, double cep) { return 1.0 - std::pow(0.5, (r / cep) * (r / cep)); }

// ------------------------------------------------------------------------------------------------ the hot phase (B14)

// The abstract fight over many seeds (air_fight.hpp): the strikers' answers, the losses each side.
struct FightTally {
    int fights = 0, attacked = 0, jettison = 0, aborted = 0, theirs_lost = 0, ours_lost = 0;
};
FightTally Fights(const AirWarData& d, const Package& p, double awacs, int runs, std::uint64_t key) {
    const AbstractAirFight fight(d);
    AbstractFightSetup s;
    s.friendly_fighters = 1.0;
    s.awacs = awacs;
    FightTally t;
    std::vector<int> alive;
    for (const Flight& f : p.flights) alive.push_back(f.count);
    for (int k = 1; k <= runs; ++k) {
        const AbstractFightResult r = fight.Resolve(p, alive, s, key + static_cast<std::uint64_t>(k) * 7919u);
        if (!r.found) continue;
        ++t.fights;
        t.attacked += r.attacked;
        t.jettison += r.jettison_drawn;
        t.aborted += static_cast<int>(r.aborted.size());
        t.theirs_lost += static_cast<int>(r.enemy_lost.size());
        t.ours_lost += static_cast<int>(r.ours_lost.size());
    }
    return t;
}

Flight FightFlight(FlightRole role, const char* type, int n, Profile prof) {
    Flight f;
    f.callsign = std::string(FlightRoleName(role)) + " " + type;
    f.role = role;
    f.aircraft = type;
    f.count = n;
    f.profile = prof;
    return f;
}

// The hot phase against history, each measure against its band in data/air_war/balance_bands.json; returns the number
// out of band.
int HotPhase(const std::string& data_dir, int runs, const Bench& nato, const Bench& pact) {
    BalanceBands bands;
    std::string e;
    if (!LoadBalanceBands(data_dir + "/air_war/balance_bands.json", bands, &e)) {
        std::fprintf(stderr, "%s\n", e.c_str());
        return 1;
    }
    const auto band = [&bands](const char* id) { return bands.Find("air_war_bench", id); };
    int fails = 0;
    const int n_fights = std::max(2000, runs * 20);
    std::printf("\n# The hot phase against history (roadmap B14; %d abstract fights each)\n\n", n_fights);
    std::printf("| Measure | The model | Band | Anchor | |\n|---|---|---|---|---|\n");

    // 1. Strikers attacked by fighters: two flights of four F-4Es, no escort, no warning (the F-105s of 1966), against our
    // MiG-21s under GCI; and the same answer in a session (reactions.hpp DecideFlight: NATO's row for a fighter seen,
    // the stress a fighter's attack raises, the leaders' nerve spread about their faction's).
    {
        Package p;
        p.flights = {FightFlight(FlightRole::Strike, "f4e", 4, Profile::Medium), FightFlight(FlightRole::Strike, "f4e", 4, Profile::Medium)};
        const FightTally t = Fights(nato.d, p, 0.0, n_fights, 0xB14F105ull);
        const double a = std::max(1, t.attacked);
        const double jett = t.jettison / a, ab = t.aborted / a;
        // Pressed on with its stores: neither jettisoned nor turned home (the two draws are independent).
        const double press_share = (1.0 - jett) * (1.0 - ab);
        std::printf("| *Strike flights attacked by our fighters (abstract fight, %d flights)* | | | | |\n", t.attacked);
        fails += BandRow(band("f105_jettison"), "jettisoned (any stores)", jett, 100.0, "%") ? 0 : 1;
        fails += BandRow(band("f105_press_on"), "pressed on with their stores", press_share, 100.0, "%") ? 0 : 1;
        fails += BandRow(band("f105_abort"), "turned for home", ab, 100.0, "%") ? 0 : 1;
        ReactionTable rt;
        if (!LoadReactionTable(data_dir, rt, &e)) {
            std::fprintf(stderr, "%s\n", e.c_str());
            return fails + 1;
        }
        const ReactionProfile* prof = rt.Profile("nato");
        if (prof != nullptr) {
            const ReactionRow& row = ChooseRow(*prof, CueSource::Eye, ThreatClass::Fighter);
            const double stress = StressAdd(0.0, rt.stress_fighter);
            Rng r(0xB14DEC1ull);
            int sj = 0, sp = 0, sa = 0;
            const int n = 20000;
            for (int k = 0; k < n; ++k) {
                const double nerve = r.Uniform(0.3, 0.7);
                const double uj = r.Uniform(), ut = r.Uniform(), uc = r.Uniform();
                const FlightDecision d = DecideFlight(rt, row, stress, nerve, uj, ut, uc);
                sj += d.jettison ? 1 : 0;
                sa += d.choice == FlightChoice::Abort ? 1 : 0;
                sp += d.choice != FlightChoice::Abort && !d.jettison ? 1 : 0;
            }
            std::printf("| *The same in a session (reactions.hpp, NATO's row for a fighter seen, %d draws)* | | | | |\n", n);
            fails += BandRow(band("f105_jettison"), "jettisoned (any stores)", static_cast<double>(sj) / n, 100.0, "%") ? 0 : 1;
            fails += BandRow(band("f105_press_on"), "pressed on with their stores", static_cast<double>(sp) / n, 100.0, "%") ? 0 : 1;
            fails += BandRow(band("f105_abort"), "turned for home", static_cast<double>(sa) / n, 100.0, "%") ? 0 : 1;
        }
    }

    // 2. The missiles of 1967 (CHECO): the reference mix's pilots (the calibration's skill and stress) firing AIM-7Es and
    // AIM-9Bs (air_combat.hpp SimulateLaunches), hits and shots out of parameters per launch.
    {
        const AirCombatDef& ac = nato.d.air_combat;
        Pilot of67;
        of67.skill = ac.calibration.mix.skill;
        of67.stress = ac.calibration.mix.stress;
        const AamDef* aim7 = ac.Aam("aim7e");
        const AamDef* aim9 = ac.Aam("aim9b");
        if (aim7 != nullptr && aim9 != nullptr) {
            const int n = std::max(20000, runs * 50);
            const LaunchStats s7 = SimulateLaunches(*aim7, ac.calibration, of67, n, 0xB14C7ull);
            const LaunchStats s9 = SimulateLaunches(*aim9, ac.calibration, of67, n, 0xB14C9ull);
            std::printf("| *The 1967 crews' launches (air_combat.hpp, %d each)* | | | | |\n", n);
            fails += BandRow(band("checo_aim7_hit"), "AIM-7E hits a launch", s7.HitRate(), 100.0, "%") ? 0 : 1;
            fails += BandRow(band("checo_aim7_out"), "AIM-7E out of parameters", s7.OutOfEnvelopeShare(), 100.0, "%") ? 0 : 1;
            fails += BandRow(band("checo_aim9_hit"), "AIM-9B hits a launch", s9.HitRate(), 100.0, "%") ? 0 : 1;
            fails += BandRow(band("checo_aim9_out"), "AIM-9B out of parameters", s9.OutOfEnvelopeShare(), 100.0, "%") ? 0 : 1;
        }
    }

    // 3. Exchange ratios, the US view (our fighters lost for each of theirs): analogues flown with 1985 types, since the
    // fits begin in 1974. Vietnam 1968: F-4E strikers with an F-4E escort against our MiG-21s under GCI, no warning;
    // Linebacker's swing in 1972 came with Teaball's warning: the same under an E-3A; the Bekaa: F-16A strikers, an F-15C
    // sweep, an EF-111A jamming our GCI, under an E-3A.
    {
        Package viet;
        viet.flights = {FightFlight(FlightRole::Strike, "f4e", 4, Profile::Medium), FightFlight(FlightRole::Escort, "f4e", 4, Profile::Medium)};
        Package bekaa;
        bekaa.flights = {FightFlight(FlightRole::Strike, "f16a", 4, Profile::Medium), FightFlight(FlightRole::Sweep, "f15c", 4, Profile::Medium),
                         FightFlight(FlightRole::Jammer, "ef111a", 1, Profile::Medium)};
        const FightTally a = Fights(nato.d, viet, 0.0, n_fights, 0xB14E01ull);
        const FightTally b = Fights(nato.d, viet, 1.0, n_fights, 0xB14E02ull);
        const FightTally c = Fights(nato.d, bekaa, 1.0, n_fights, 0xB14E03ull);
        const auto ratio = [](const FightTally& t) { return static_cast<double>(t.ours_lost) / std::max(1, t.theirs_lost); };
        std::printf("| *Exchanges, ours lost for each of theirs (fights: %d, %d, %d)* | | | | |\n", a.fights, b.fights, c.fights);
        fails += BandRow(band("exchange_no_warning"), "strike and escort, no warning (1968)", ratio(a)) ? 0 : 1;
        fails += BandRow(band("exchange_warning"), "the same under an E-3A (1972, Teaball)", ratio(b)) ? 0 : 1;
        fails += BandRow(band("exchange_swing"), "the warning's swing (times)", ratio(b) / std::max(1e-9, ratio(a))) ? 0 : 1;
        fails += BandRow(band("exchange_bekaa"), "sweep, jammer and E-3A (the Bekaa)", ratio(c)) ? 0 : 1;
        // The Pact's MiG-23MLs sweeping for its Su-24s (B8's own check, here for the record).
        Package wp;
        wp.flights = {FightFlight(FlightRole::Strike, "su24", 4, Profile::Medium), FightFlight(FlightRole::Sweep, "mig23ml_wp", 4, Profile::Medium)};
        const FightTally w = Fights(pact.d, wp, 0.0, n_fights, 0xB14E04ull);
        fails += BandRow(band("exchange_pact_sweep"), "the Pact's MiG-23ML sweep", ratio(w)) ? 0 : 1;
    }

    // 4. Retask latencies against doctrine (hot_phase.hpp): an airborne flight retasked (the Pact's armed reconnaissance,
    // NATO's close support) and a new tasking from readiness 1, over the delays' draws with the faction's mean officer;
    // beside them B5's order hops (c2.timing), which since B14 the retask delay floors (c2.hpp Divert).
    {
        const auto mean_s = [](const std::function<double(double)>& f) {
            double s = 0.0;
            int n = 0;
            for (int k = 0; k < 200; ++k) {
                const double v = f((k + 0.5) / 200.0);
                if (v < 0.0) continue;
                s += v;
                ++n;
            }
            return n > 0 ? s / n : -1.0;
        };
        const double pact_air = mean_s([&](double u) { return RetaskAirS(pact.d.faction, "armed_recce", 0.99, u); });
        const double nato_air = mean_s([&](double u) { return RetaskAirS(nato.d.faction, "cas", 0.99, u); });
        const double pact_r1 = mean_s([&](double u) { return AlertTakeoffS(pact.d.faction, "readiness_1", u); });
        const auto hop = [](const FactionC2& c2, const char* name) {
            const C2HopDef* h = c2.Hop(name);
            return h == nullptr ? 0.0 : 0.5 * (h->relay_s.lo + h->relay_s.hi + h->decide_s.lo + h->decide_s.hi + h->encode_s.lo + h->encode_s.hi);
        };
        std::printf("| *Latencies (minutes; the order's hops alone: Pact %.1f, NATO %.1f)* | | | | |\n", hop(pact.d.faction.c2, "order") / 60.0,
                    hop(nato.d.faction.c2, "order") / 60.0);
        fails += BandRow(band("retask_pact_air_min"), "the Pact: an airborne flight retasked", pact_air / 60.0) ? 0 : 1;
        fails += BandRow(band("retask_nato_air_min"), "NATO: close support retasked in the air", nato_air / 60.0) ? 0 : 1;
        fails += BandRow(band("alert_pact_r1_min"), "the Pact: readiness 1 to wheels up", pact_r1 / 60.0) ? 0 : 1;
    }

    // 5. Friendly fire in an exercise (command/own_air.hpp, own_flights.hpp): tracks cross a battery's zone, a share of
    // them ours; each of ours is notified down the chain from its regiment (RunNotice, the notice's real hops) some minutes
    // before it crosses, and the crew judges it by its rule (JudgeTrack). 1960s settings: notices only, no interrogator;
    // 1985: the day's codes too (the table's schedule, the pilots' missed changes). The share of ours among the tracks fired on.
    {
        command::ChainData chain;
        command::OwnAirDef rules;
        command::IffCodesDef codes;
        if (!command::LoadChainData(data_dir, "command/chain_pannonia.json", chain, &e) ||
            !command::LoadOwnAirFile(data_dir + "/command/own_air.json", rules, &e) || !command::LoadIffCodesFile(data_dir + "/command/iff_codes.json", codes, &e)) {
            std::fprintf(stderr, "%s\n", e.c_str());
            return fails + 1;
        }
        const LocalFrame frame(GeoPoint::FromDegrees(46.9, 19.7, 110.0));
        // Game values (no period figure): ours among the tracks crossing a battery's zone in an exercise, and how long
        // before it crosses a notice leaves the fighter regiment (08_OUR_FIGHTERS.md "Left open": the lead time).
        const double own_share = 0.35;
        const double lead_lo_min = 15.0, lead_hi_min = 75.0;
        const int n = std::max(200, runs);
        int fired_own[2] = {0, 0}, fired_hostile = 0;
        Rng r(0xB14FF0ull);
        for (int k = 0; k < n; ++k) {
            const bool own = r.Chance(own_share);
            const double lead_h = r.Uniform(lead_lo_min, lead_hi_min) / 60.0;
            if (!own) {
                ++fired_hostile;
                continue;
            }
            command::OwnFlightNotice nt;
            nt.number = 400 + k;
            nt.flight = "Sokol " + std::to_string(k + 1);
            nt.route = {Vec3{-25000.0, -20000.0, 0.0}, Vec3{0.0, 30000.0, 0.0}};
            nt.floor_m = 2000.0;
            nt.ceiling_m = 6000.0;
            const double sent_h = 10.25;
            const double cross_h = sent_h + lead_h;
            nt.cross_from_h = cross_h - 0.1;
            nt.cross_to_h = cross_h + 0.2;
            const command::NoticeTrace tr = command::RunNotice(chain, nt, command::NoticeWords(nt, frame), "fighter_rgt_duty", "rgt_cp_duty", sent_h - 10.0,
                                                               rules.notice, static_cast<unsigned>(k + 1), 10.0);
            const bool reached = !tr.hops.empty() && tr.hops.back().post == "rgt_cp_duty" &&
                                 10.0 + tr.hops.back().h + rules.notice.liaison_s / 3600.0 <= cross_h;
            const command::IffSchedule sched = command::IffSchedule::Days(codes, true, 0, 0, static_cast<std::uint64_t>(k + 1));
            const double clock_h = cross_h;
            command::TrackSeen ts;
            ts.pos = Vec3{-12500.0, 5000.0, 0.0};
            ts.alt_m = 4000.0;
            ts.clock_h = clock_h;
            std::vector<command::OwnFlightNotice> known;
            if (reached) known.push_back(nt);
            const command::CooperationPlan told;
            for (int era = 0; era < 2; ++era) {
                ts.replied = false;
                if (era == 1) {
                    const int bf = sched.FilterAt(clock_h);
                    const int pf = command::PilotFilterAt(sched, clock_h, rules.iff_change.pilot_miss, static_cast<std::uint64_t>(k + 1), 1);
                    ts.replied = command::IffAnswers(command::IffSystem::Kremniy, pf, command::IffSystem::Kremniy, bf);
                }
                const command::OwnAirCall call = command::JudgeTrack(ts, known, told, rules.notice.corridor_half_width_m, rules.notice.slack_s / 3600.0);
                if (call == command::OwnAirCall::Free) ++fired_own[era];
            }
        }
        std::printf("| *Friendly fire in an exercise (%d tracks, %.0f%% ours, notices %.0f-%.0f min ahead)* | | | | |\n", n, 100.0 * own_share, lead_lo_min,
                    lead_hi_min);
        const auto share = [&](int own) { return static_cast<double>(own) / std::max(1, own + fired_hostile); };
        fails += BandRow(band("friendly_fire_1960s"), "ours among those fired on: notices only", share(fired_own[0]), 100.0, "%") ? 0 : 1;
        fails += BandRow(band("friendly_fire_1985"), "ours among those fired on: notices and codes", share(fired_own[1]), 100.0, "%") ? 0 : 1;
    }
    std::printf("\n%d measure(s) out of band.\n", fails);
    return fails;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string data_dir = argc > 1 ? argv[1] : "data";
    const int runs = argc > 2 ? std::max(1, std::atoi(argv[2])) : 200;
    Bench nato, pact;
    if (!Load(nato, data_dir, "nato") || !Load(pact, data_dir, "warsaw_pact")) return 1;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--why") g_why = true;
        if (arg == "--set" && i + 1 < argc) {
            const std::string kv = argv[++i];
            const std::size_t eq = kv.find('=');
            if (eq == std::string::npos || TuningTable::Defaults().count(kv.substr(0, eq)) == 0) {
                std::fprintf(stderr, "--set needs a tuning key=value: %s\n", kv.c_str());
                return 1;
            }
            nato.d.tuning.values[kv.substr(0, eq)] = pact.d.tuning.values[kv.substr(0, eq)] = std::atof(kv.c_str() + eq + 1);
        }
    }
    std::printf("# The air war against history (%d runs each)\n\n", runs);
    std::printf("| Benchmark | The model | History |\n|---|---|---|\n");

    // 1. A massive bridge (very_hard, two spans), F-4Es four at a time, its guns round it.
    {
        const int cap = 871;
        int dropped = 0;
        double sorties_to_drop = 0.0, sorties = 0.0, lost = 0.0;
        const int bridge_runs = std::max(1, runs / 4);
        for (int r = 0; r < bridge_runs; ++r) {
            Theatre(nato, {Target(ObjectKind::Bridge, "very_hard")});
            int s = 0;
            while (nato.truth[0].function > 0.5 + 1e-9 && s < cap) {
                Package p = MakePackage();
                p.flights.push_back(MakeFlight(nato, "f4e", FlightRole::Strike, 4, Profile::Medium, "mk84", 2, Delivery::Level));
                lost += Fly(nato, p).Lost();
                s += 4;
            }
            sorties += s;
            if (nato.truth[0].function <= 0.5 + 1e-9) {
                ++dropped;
                sorties_to_drop += s;
            }
        }
        std::printf("| A massive bridge (very hard, two spans): F-4Es with two Mk 84 each from medium altitude (the model's level delivery, its error the Vietnam average miss), until a span is down or 871 sorties | a span down in %.0f%% of campaigns (after %.0f sorties when it fell); %.1f lost a 100 sorties | the Thanh Hoa bridge: 871 sorties, 11 lost (1.3 a 100), not dropped |\n",
                    100.0 * dropped / bridge_runs, dropped > 0 ? sorties_to_drop / dropped : 0.0, 100.0 * lost / std::max(1.0, sorties));
        int down = 0;
        for (int r = 0; r < runs; ++r) {
            Theatre(nato, {Target(ObjectKind::Bridge, "very_hard")});
            Package p = MakePackage();
            p.flights.push_back(MakeFlight(nato, "f4e", FlightRole::Strike, 12, Profile::Medium, "gbu10", 2, Delivery::Dive));
            Fly(nato, p);
            down += nato.truth[0].function <= 0.5 + 1e-9 ? 1 : 0;
        }
        std::printf("| The same bridge, one strike of 12 F-4s with 24 GBU-10 Paveways | a span down in %.0f%% of strikes | 27 April 1972: damaged, not dropped; 13 May 1972: 14 F-4s, 24 laser-guided bombs, a span down |\n",
                    100.0 * down / runs);
        Why();
    }
    // 2. Accuracy (a Rayleigh miss: its mean is 1.06 CEPs).
    {
        const double ung = CepOf(nato, "f4e", "mk84", Delivery::Level);
        const double lgb = CepOf(nato, "f4e", "gbu10", Delivery::Level);
        std::printf("| Average miss, unguided from medium altitude against laser-guided | %.0f ft against %.0f ft | 1972-73: 447 ft against 23 ft |\n",
                    1.0645 * ung / 0.3048, 1.0645 * lgb / 0.3048);
        std::printf("| Within 20 ft of the aim (a direct hit on a bridge's span or a building) | %.1f%% against %.0f%% | 5.5%% against 48%% direct hits (a real miss distribution has a long tail of gross errors; the model's has none) |\n",
                    100.0 * Within(20.0 * 0.3048, ung), 100.0 * Within(20.0 * 0.3048, lgb));
        Why();
    }
    // 3. Low against medium altitude at night: an airfield with light guns and a Strela-10, a Kub 6 km off, two F-4Gs
    // with each package.
    {
        double lost_low = 0, lost_med = 0, sort = 0;
        const auto airfield = [] {
            return std::vector<TheatreObject>{Target(ObjectKind::Airfield), Defence("guns", Vec3{800.0, -400.0, 0.0}, "aaa"),
                                              Defence("strela", Vec3{-1500.0, 500.0, 0.0}, "strela"), Defence("kub", Vec3{0.0, -6000.0, 0.0}, "kub")};
        };
        for (int r = 0; r < runs; ++r) {
            Theatre(nato, airfield());
            Package lo = MakePackage(DayPart::Night);
            lo.flights.push_back(MakeFlight(nato, "f4g", FlightRole::Sead, 2, Profile::Medium, nullptr, 0, Delivery::Count));
            lo.flights.push_back(MakeFlight(nato, "tornado_ids", FlightRole::Strike, 8, Profile::Low, "jp233", 2, Delivery::Laydown));
            lost_low += Fly(nato, lo).Lost();
        }
        if (g_why) std::printf("|   (low)");
        Why();
        for (int r = 0; r < runs; ++r) {
            Theatre(nato, airfield());
            Package md = MakePackage(DayPart::Night);
            md.flights.push_back(MakeFlight(nato, "f4g", FlightRole::Sead, 2, Profile::Medium, nullptr, 0, Delivery::Count));
            md.flights.push_back(MakeFlight(nato, "f16a", FlightRole::Strike, 8, Profile::Medium, "mk84", 2, Delivery::Level));
            lost_med += Fly(nato, md).Lost();
            sort += 10;
        }
        std::printf("| An airfield at night with guns and a Strela-10, a Kub 6 km off, two F-4Gs along: 8 Tornados laying JP233 against 8 F-16s bombing from medium altitude (losses a 100 sorties) | %.1f against %.1f (%.1f times) | Desert Storm: 18 losses in the low-level first week, 22 in the five weeks at medium altitude after (some 4 times a sortie) |\n",
                    100.0 * lost_low / sort, 100.0 * lost_med / sort, lost_med > 0 ? lost_low / lost_med : 0.0);
        Why();
    }
    // 4. The runway (an airfield's four aim points: its runways and taxiways).
    {
        double jp = 0, bet = 0, mk = 0;
        for (int r = 0; r < runs; ++r) {
            Theatre(nato, {Target(ObjectKind::Airfield)});
            Package a = MakePackage();
            a.flights.push_back(MakeFlight(nato, "tornado_ids", FlightRole::Strike, 2, Profile::Low, "jp233", 2, Delivery::Laydown));
            Fly(nato, a);
            jp += 1.0 - nato.truth[0].function;
            Theatre(pact, {Target(ObjectKind::Airfield)});
            Package p = MakePackage();
            p.flights.push_back(MakeFlight(pact, "su24", FlightRole::Strike, 4, Profile::Low, "betab500", 6, Delivery::Dive));
            Fly(pact, p);
            bet += 1.0 - pact.truth[0].function;
            Theatre(nato, {Target(ObjectKind::Airfield)});
            Package m = MakePackage();
            m.flights.push_back(MakeFlight(nato, "f16a", FlightRole::Strike, 4, Profile::Medium, "mk84", 2, Delivery::Level));
            Fly(nato, m);
            mk += 1.0 - nato.truth[0].function;
        }
        std::printf("| An airfield's runways and taxiways cut: 2 Tornados with two JP233 each; 4 Su-24s with six BetAB-500 each in dives; 4 F-16s with two Mk 84 each from medium altitude | %.0f%%; %.0f%%; %.0f%% | Focus, 5 June 1967: flights of four with runway-piercing bombs cut the runways of some ten airfields; JP233 closed runways from low level; bombing from medium altitude cratered few |\n",
                    100.0 * jp / runs, 100.0 * bet / runs, 100.0 * mk / runs);
        Why();
    }
    // 5. Anti-radiation missiles: two F-4Gs past five SAM radars, crews that radiate against crews that switch off.
    {
        double shots[2] = {0, 0}, kills[2] = {0, 0};
        for (int disciplined = 0; disciplined < 2; ++disciplined) {
            nato.posture.emcon_discipline = disciplined ? 0.95 : 0.05;
            for (int r = 0; r < runs; ++r) {
                std::vector<TheatreObject> objs{Target(ObjectKind::Depot)};
                for (int k = 0; k < 5; ++k) {
                    TheatreObject s = Defence("kub" + std::to_string(k), Vec3{(k - 2) * 4000.0, -30000.0 + k * 5000.0, 0.0}, "kub");
                    s.player = true;  // the resolver reads the player's posture for a crew's discipline
                    objs.push_back(s);
                }
                Theatre(nato, objs);
                Package p = MakePackage();
                p.flights.push_back(MakeFlight(nato, "f4g", FlightRole::Sead, 2, Profile::Medium, nullptr, 0, Delivery::Count));
                const PackageOutcome o = Fly(nato, p);
                shots[disciplined] += o.arms_fired;
                kills[disciplined] += static_cast<double>(o.radars_killed.size());
            }
        }
        nato.posture = BatteryPosture();
        std::printf("| Anti-radiation missiles fired for each radar destroyed: crews that radiate against crews that switch off at once (HARM, with its memory) | %.1f against %.1f | the Bekaa: radiating batteries destroyed in an afternoon; Allied Force: some 743 HARMs, 3 of about 25 SA-6 batteries confirmed destroyed (many fired pre-emptively) |\n",
                    kills[0] > 0 ? shots[0] / kills[0] : 0.0, kills[1] > 0 ? shots[1] / kills[1] : 0.0);
        Why();
    }
    // 6. Unretarded bombs from low level.
    {
        const WeaponDef* w = nato.d.Weapon("mk82");
        std::printf("| Unretarded bombs dropped from low level that go off | %.0f%% | the Falklands: 11 of 25 (44%%) |\n",
                    100.0 * (w != nullptr ? w->laydown_reliability : 0.0));
        Why();
    }
    // 7. A SAM belt without suppression: four F-4Es popping up on troops behind four Kub batteries, guns at the front.
    {
        double lost = 0, sort = 0;
        for (int r = 0; r < runs; ++r) {
            std::vector<TheatreObject> objs{Target(ObjectKind::Troops)};
            for (int k = 0; k < 4; ++k) objs.push_back(Defence("kub" + std::to_string(k), Vec3{(k - 1.5) * 9000.0, -15000.0 + k * 3000.0, 0.0}, "kub"));
            for (int k = 0; k < 3; ++k) objs.push_back(Defence("guns" + std::to_string(k), Vec3{(k - 1.0) * 2500.0, -2000.0, 0.0}, "aaa"));
            Theatre(nato, objs);
            Package p = MakePackage();
            p.flights.push_back(MakeFlight(nato, "f4e", FlightRole::Strike, 4, Profile::Low, "mk82", 6, Delivery::PopUp));
            lost += Fly(nato, p).Lost();
            sort += 4;
        }
        std::printf("| Four F-4Es popping up on troops behind a belt of four Kub batteries and three gun batteries, by day, no suppression (losses a 100 sorties) | %.1f | Yom Kippur, the first three days: 4 a 100 (50 in about 1,220 sorties, most of them not into the belt) |\n",
                    100.0 * lost / sort);
        Why();
    }
    // 8. The Bekaa: a package of decoys, jammers, SEAD and DEAD against a battery that radiates, one of five.
    {
        double destroyed = 0, lost = 0, sort = 0;
        nato.posture.emcon_discipline = 0.05;
        for (int r = 0; r < runs; ++r) {
            std::vector<TheatreObject> objs;
            TheatreObject t = Defence("target", Vec3{}, "kub");
            t.player = true;
            objs.push_back(t);
            for (int k = 0; k < 4; ++k) {
                TheatreObject s = Defence("kub" + std::to_string(k), Vec3{(k - 1.5) * 10000.0, (k % 2) * 8000.0 - 4000.0, 0.0}, "kub");
                s.player = true;
                objs.push_back(s);
            }
            Theatre(nato, objs);
            Package p = MakePackage();
            p.kind = MissionKind::Dead;
            p.flights.push_back(MakeFlight(nato, "bqm74", FlightRole::Decoy, 4, Profile::Medium, nullptr, 0, Delivery::Count));
            p.flights.push_back(MakeFlight(nato, "ef111a", FlightRole::Jammer, 1, Profile::Medium, nullptr, 0, Delivery::Count));
            p.flights.push_back(MakeFlight(nato, "f4g", FlightRole::Sead, 4, Profile::Medium, nullptr, 0, Delivery::Count));
            p.flights.push_back(MakeFlight(nato, "f4e", FlightRole::Dead, 4, Profile::Medium, "rockeye", 4, Delivery::Dive));
            const PackageOutcome o = Fly(nato, p);
            destroyed += nato.truth[0].function <= 0.0 ? 1.0 : 0.0;
            lost += o.Lost();
            sort += 9;
        }
        nato.posture = BatteryPosture();
        std::printf("| A package of four decoys, a jammer, four F-4Gs and four F-4Es with Rockeye against a Kub battery that radiates, four more round it (each package) | the battery destroyed in %.0f%%; %.1f lost a 100 sorties | the Bekaa, 9 June 1982: 17 of 19 batteries in an afternoon, none of the aircraft lost to them |\n",
                    100.0 * destroyed / runs, 100.0 * lost / std::max(1.0, sort));
        Why();
    }
    // 9. The gun pass: each pass one keyed draw against the GAU-8's per_pass for armour (the session's PassFired).
    {
        const WeaponDef* gau8 = nato.d.Weapon("gau8");
        if (gau8 != nullptr && gau8->pass.present) {
            const PassEffect still = GunPassEffect(gau8->pass, TargetClass::Armour, false, false, false, nato.d.tuning);
            const PassEffect hot = GunPassEffect(gau8->pass, TargetClass::Armour, true, true, false, nato.d.tuning);
            double out[2] = {0, 0}, burnt[2] = {0, 0};
            int lo[2] = {10, 10}, hi[2] = {0, 0};
            for (int k = 0; k < 2; ++k) {
                const PassEffect& e = k == 0 ? still : hot;
                for (int r = 0; r < runs; ++r) {
                    int n = 0;
                    for (int pass = 0; pass < 10; ++pass) {
                        const std::uint64_t key = static_cast<std::uint64_t>(r) * 16u + static_cast<std::uint64_t>(pass);
                        if (OppDraw(1979, static_cast<std::uint64_t>(k), key, 40, 0) < e.kill) {
                            ++n;
                            burnt[k] += OppDraw(1979, static_cast<std::uint64_t>(k), key, 41, 0) < e.catastrophic ? 1.0 : 0.0;
                        }
                    }
                    out[k] += n;
                    lo[k] = std::min(lo[k], n);
                    hi[k] = std::max(hi[k], n);
                }
            }
            std::printf("| Two A-10s' ten GAU-8 passes on ten tanks: known, stationary and undefended; against a column on the move whose guns fire back (out of action, of them burnt) | %.1f (%d-%d), %.1f burnt; %.1f (%d-%d), %.1f burnt | Array 20, 19 September 1979: 8 of 10 put out of action, 3 of them burnt (no figure for a moving, defended column) |\n",
                        out[0] / runs, lo[0], hi[0], burnt[0] / runs, out[1] / runs, lo[1], hi[1], burnt[1] / runs);
        }
        Why();
    }
    // The hot phase against history (roadmap B14): exits 2 when a measure is out of its band.
    return HotPhase(data_dir, runs, nato, pact) > 0 ? 2 : 0;
}
