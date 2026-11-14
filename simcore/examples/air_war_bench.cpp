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
//      aircraft lost to them).
// The figures are what the model makes of the analogue, not a claim it reproduces the battle: the settings are stated
// with each line. Exits non-zero if the data fail to load.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"

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
    return 0;
}
