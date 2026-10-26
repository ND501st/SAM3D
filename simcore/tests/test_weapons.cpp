// The air war's weapons, attacks and damage (simcore/air_war/planner.hpp, resolve.hpp, data/air_war/weapons.json,
// docs/air_war/10_WEAPONS.md): what each side carries, how an attack is chosen and flown, what it does, and two of the
// benchmarks against history (simcore/examples/air_war_bench.cpp, docs/air_war/09_BALANCE.md section 9).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;
const char* kFactions[] = {"rebels", "neutral", "warsaw_pact", "nato"};

AirWarData Load(const std::string& faction) {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

// A target and nothing else: no defences, no fighters (the weapons alone).
struct Range {
    AirWarData d;
    std::vector<ObjectState> truth;
    BatteryPosture posture;
    AirStaff staff;
    Rng rng{11};

    Range(const std::string& faction, ObjectKind kind, const char* cls) {
        d = Load(faction);
        TheatreObject t;
        t.id = "target";
        t.name = "the target";
        t.kind = kind;
        if (cls != nullptr) t.has_target_class = ParseTargetClass(cls, t.target_class);
        d.theatre.objects = {t};
        d.theatre.supply_lines.clear();
        for (FrontDef& f : d.theatre.fronts) f.friendly_fighters = 0.0;
        Rng srng(5);
        staff.Build(d, srng, 0.0);
        Reset();
    }
    void Reset() {
        ObjectState s;
        s.id = "target";
        truth = {s};
    }
    // One strike of `count` aircraft with `weapons` each, delivered so; the target's function after it.
    double Strike(const char* type, int count, const char* weapon, int weapons, Delivery del, Profile prof, DayPart part = DayPart::Day) {
        const AircraftType& a = *d.faction.Aircraft(type);
        Package p;
        p.kind = MissionKind::Strike;
        p.part = part;
        p.target_object = "target";
        Flight f;
        f.callsign = "Test";
        f.role = FlightRole::Strike;
        f.aircraft = type;
        f.count = count;
        f.profile = prof;
        f.weapon = weapon;
        f.weapons = weapons;
        f.delivery = del;
        f.aim_pos = Vec3{};
        f.route = {{Vec3{0.0, -60000.0, 0.0}, ProfileHeight(a, prof), 0.0, "entry"},
                   {Vec3{0.0, -ReleaseShort(del, d.Weapon(weapon), d.tuning), 0.0}, ProfileHeight(a, prof), 0.0, "target"},
                   {Vec3{20000.0, -60000.0, 0.0}, ProfileHeight(a, prof), 0.0, "exit"}};
        TimeRoute(f.route, 1, 1.0, a.attack_mps);
        f.release_index = 1;
        p.flights.push_back(f);
        ResolveContext rc;
        rc.data = &d;
        rc.truth = &truth;
        rc.posture = &posture;
        rc.staff = &staff;
        rc.rng = &rng;
        Resolver r;
        r.Resolve(p, rc);
        return truth[0].function;
    }
};

}  // namespace

SC_TEST("weapons: every aircraft's loads are known weapons, and every attack type carries one in service in the war's year") {
    for (const char* f : kFactions) {
        const AirWarData d = Load(f);
        SC_EXPECT(!d.weapons.weapons.empty());
        for (const AircraftType& a : d.faction.aircraft) {
            if (a.missile || a.drone) continue;
            bool attacks = false;
            for (FlightRole r : {FlightRole::Strike, FlightRole::Dead, FlightRole::Helicopter}) attacks = attacks || a.Has(r);
            if (!attacks) continue;
            int in_service = 0;
            for (const auto& l : a.loads) {
                const WeaponDef* w = d.Weapon(l.first);
                SC_EXPECT(w != nullptr && l.second > 0);
                in_service += w != nullptr && w->InService(d.theatre.year) ? 1 : 0;
            }
            if (in_service == 0) std::printf("    %s: %s carries nothing in service in %d\n", f, a.id.c_str(), d.theatre.year);
            SC_EXPECT(in_service > 0);
        }
    }
    // Every kind of object is a target of some class with aim points.
    const AirWarData d = Load("nato");
    for (int k = 0; k < static_cast<int>(ObjectKind::Count); ++k) SC_EXPECT(d.weapons.targets[k].aim_points > 0);
}

SC_TEST("weapons: the kill model: guided bombs miss by metres and dumb bombs from height by a hundred; a runway is cut across its width; blast reaches further against soft targets") {
    const AirWarData d = Load("nato");
    const AircraftType& f4 = *d.faction.Aircraft("f4e");
    const AircraftType& f111 = *d.faction.Aircraft("f111f");
    const WeaponDef& mk84 = *d.Weapon("mk84");
    const WeaponDef& gbu10 = *d.Weapon("gbu10");
    const TuningTable& t = d.tuning;
    const double dumb = DeliveryCep(mk84, Delivery::Level, f4, DayPart::Day, airwar::Weather::Clear, 0.5, t);
    const double lgb = DeliveryCep(gbu10, Delivery::Level, f4, DayPart::Day, airwar::Weather::Clear, 0.5, t);
    // The 1972-73 averages: 447 ft and 23 ft (afmag_2003_bombs); a Rayleigh miss's mean is 1.06 CEPs.
    SC_EXPECT(std::fabs(1.0645 * dumb / 0.3048 - 447.0) < 60.0);
    SC_EXPECT(std::fabs(1.0645 * lgb / 0.3048 - 23.0) < 5.0);
    // A better sight and a better crew aim better; a dive by eye at night worse, unless the type sees at night.
    SC_EXPECT(DeliveryCep(mk84, Delivery::Dive, f111, DayPart::Day, airwar::Weather::Clear, 0.5, t) < DeliveryCep(mk84, Delivery::Dive, f4, DayPart::Day, airwar::Weather::Clear, 0.5, t));
    SC_EXPECT(DeliveryCep(mk84, Delivery::Dive, f4, DayPart::Day, airwar::Weather::Clear, 0.9, t) < DeliveryCep(mk84, Delivery::Dive, f4, DayPart::Day, airwar::Weather::Clear, 0.1, t));
    SC_EXPECT(DeliveryCep(mk84, Delivery::Dive, f4, DayPart::Night, airwar::Weather::Clear, 0.5, t) > 2.0 * DeliveryCep(mk84, Delivery::Dive, f4, DayPart::Day, airwar::Weather::Clear, 0.5, t));
    // A bomb's kill: inside its footprint, gone off, and its effect on the class.
    const double cep = 40.0;
    const double soft = WeaponKill(mk84, Delivery::Dive, TargetClass::Soft, cep, t);
    const double light = WeaponKill(mk84, Delivery::Dive, TargetClass::Light, cep, t);
    const double armour = WeaponKill(mk84, Delivery::Dive, TargetClass::Armour, cep, t);
    const double runway = WeaponKill(mk84, Delivery::Dive, TargetClass::Runway, cep, t);
    SC_EXPECT(soft > light && light > armour);
    const double inside_strip = 1.0 - std::pow(0.5, std::pow(t.Get("delivery.runway_strip_m") / cep, 2.0));
    SC_EXPECT_NEAR(runway, inside_strip * mk84.reliability * mk84.effect[static_cast<int>(TargetClass::Runway)], 1e-9);
    // Unretarded from low level, most don't go off (the Falklands); retarded, they do.
    SC_EXPECT(WeaponKill(mk84, Delivery::Laydown, TargetClass::Soft, cep, t) < 0.6 * WeaponKill(mk84, Delivery::Dive, TargetClass::Soft, cep, t));
    const WeaponDef& mk82r = *d.Weapon("mk82r");
    SC_EXPECT(WeaponKill(mk82r, Delivery::Laydown, TargetClass::Soft, cep, t) > 0.9 * WeaponKill(mk82r, Delivery::Dive, TargetClass::Soft, cep, t));
    // Spread over aim points: twice the weapons, more than once the share, never all.
    SC_EXPECT(ExpectedShare(0.3, 8, 4) > ExpectedShare(0.3, 4, 4) && ExpectedShare(0.3, 8, 4) < 1.0);
    std::printf("    CEP from medium altitude: dumb %.0f m, laser-guided %.1f m; a Mk 84 at CEP 40 m: soft %.2f, light %.2f, armour %.2f, runway %.2f\n",
                dumb, lgb, soft, light, armour, runway);
}

SC_TEST("weapons: the weaponeer picks what the type, the light and the weather allow") {
    const AirWarData nato = Load("nato");
    const AirWarData pact = Load("warsaw_pact");
    const AircraftType& f111 = *nato.faction.Aircraft("f111f");
    const AircraftType& f4 = *nato.faction.Aircraft("f4e");
    const AircraftType& tornado = *nato.faction.Aircraft("tornado_ids");
    const AircraftType& su24 = *pact.faction.Aircraft("su24");
    // A bridge by day from medium altitude: laser-guided bombs from the types with a designator.
    const Weaponeering bridge = Weaponeer(nato, f111, TargetClass::Hard, 2, Profile::Medium, DayPart::Day, airwar::Weather::Clear, 0.5, 0.4);
    SC_EXPECT(bridge.weapon != nullptr && bridge.weapon->guidance == "laser");
    // At night an F-4E has no laser (it can't see the spot) and drops by its sight.
    const Weaponeering f4_night = Weaponeer(nato, f4, TargetClass::Hard, 2, Profile::Medium, DayPart::Night, airwar::Weather::Clear, 0.5, 0.4);
    SC_EXPECT(f4_night.weapon == nullptr || f4_night.weapon->guidance != "laser");
    // Under low cloud nothing is dropped by eye.
    const Weaponeering cloud = Weaponeer(nato, f4, TargetClass::Light, 6, Profile::Low, DayPart::Day, airwar::Weather::Poor, 0.5, 0.4);
    SC_EXPECT(cloud.weapon == nullptr || (cloud.delivery != Delivery::PopUp && cloud.delivery != Delivery::Dive) || cloud.weapon->Guided());
    // An airfield from low level: the Tornado lays its JP233 along the runway.
    const Weaponeering runway = Weaponeer(nato, tornado, TargetClass::Runway, 4, Profile::Low, DayPart::Night, airwar::Weather::Clear, 0.5, 0.4);
    SC_EXPECT(runway.weapon != nullptr && runway.weapon->id == "jp233" && runway.delivery == Delivery::Laydown);
    // The Su-24 against a runway: from low level retarded bombs laid along it (a BetAB-500 is dropped from a dive); from
    // height a dive with runway-piercing bombs, or laser-guided ones.
    const Weaponeering pact_runway = Weaponeer(pact, su24, TargetClass::Runway, 4, Profile::Low, DayPart::Day, airwar::Weather::Clear, 0.5, 0.3);
    SC_EXPECT(pact_runway.weapon != nullptr && pact_runway.weapon->kind == "retarded_bomb" && pact_runway.delivery == Delivery::Laydown);
    const Weaponeering pact_high = Weaponeer(pact, su24, TargetClass::Runway, 4, Profile::Medium, DayPart::Day, airwar::Weather::Clear, 0.5, 0.3);
    SC_EXPECT(pact_high.weapon != nullptr && (pact_high.weapon->id == "betab500" || pact_high.weapon->guidance == "laser"));
    // A doctrine that fears the target's guns stays out of them: never a laydown when it can stand off or toss.
    const Weaponeering bold = Weaponeer(nato, f4, TargetClass::Light, 6, Profile::Low, DayPart::Day, airwar::Weather::Clear, 0.5, 0.0);
    const Weaponeering wary = Weaponeer(nato, f4, TargetClass::Light, 6, Profile::Low, DayPart::Day, airwar::Weather::Clear, 0.5, 1.0);
    SC_EXPECT(bold.weapon != nullptr && wary.weapon != nullptr && wary.exposure <= bold.exposure);
    std::printf("    a bridge: %s %s; an airfield low: %s %s; the Su-24 at a runway: %s; bold %s %s, wary %s %s\n", bridge.weapon->id.c_str(),
                DeliveryName(bridge.delivery), runway.weapon->id.c_str(), DeliveryName(runway.delivery), pact_runway.weapon->id.c_str(),
                bold.weapon->id.c_str(), DeliveryName(bold.delivery), wary.weapon->id.c_str(), DeliveryName(wary.delivery));
}

SC_TEST("weapons: each attack flies its delivery: a pop-up's pull-up and apex, a toss's pull-up and climbing release, a dive's roll-in") {
    int popups = 0, lofts = 0, dives = 0;
    for (const char* f : {"warsaw_pact", "nato"}) {
        const AirWarData d = Load(f);
        const TuningTable& t = d.tuning;
        for (unsigned seed = 1; seed <= 2; ++seed) {
            AirCampaign c;
            c.Init(d, seed);
            while (c.Now() < 3 * 24.0) {
                if (c.Advance(3 * 24.0)) c.SkipPlayerMission();
            }
            for (const Package& p : c.Packages()) {
                for (const Flight& fl : p.flights) {
                    if (fl.role != FlightRole::Strike && fl.role != FlightRole::Dead) continue;
                    const AircraftType* a = d.faction.Aircraft(fl.aircraft);
                    if (a == nullptr || a->helicopter || fl.release_index < 1) continue;
                    const std::size_t ri = static_cast<std::size_t>(fl.release_index);
                    SC_EXPECT(fl.route[ri].tag == "target");
                    const std::string before = fl.route[ri - 1].tag;
                    if (fl.delivery == Delivery::PopUp) {
                        ++popups;
                        SC_EXPECT(before == "apex" && fl.route[ri - 2].tag == "pull_up");
                        SC_EXPECT(fl.route[ri - 1].agl_m == t.Get("delivery.popup_apex_m") && fl.route[ri].agl_m == t.Get("delivery.popup_release_m"));
                        SC_EXPECT(fl.route[ri - 2].agl_m < 200.0);  // in low, under the radar, to the pull-up
                    } else if (fl.delivery == Delivery::Loft) {
                        ++lofts;
                        SC_EXPECT(before == "pull_up" && fl.route[ri].agl_m == t.Get("delivery.loft_release_m"));
                        SC_EXPECT(Dist2d(fl.route[ri - 1].pos, fl.aim_pos) > Dist2d(fl.route[ri].pos, fl.aim_pos));
                    } else if (fl.delivery == Delivery::Dive) {
                        ++dives;
                        SC_EXPECT(before == "roll_in" && fl.route[ri].agl_m <= t.Get("delivery.dive_release_m"));
                    } else {
                        SC_EXPECT(before == "ip" || before == "hide");
                    }
                    // In time order, the release at the time on target.
                    for (std::size_t i = 1; i < fl.route.size(); ++i) SC_EXPECT(fl.route[i].t_h > fl.route[i - 1].t_h);
                }
            }
        }
    }
    std::printf("    attacks flown: %d pop-ups, %d tosses, %d dives (two campaigns of three days each, the Pact and NATO)\n", popups, lofts, dives);
    SC_EXPECT(popups > 0 && lofts > 0 && dives > 0);
}

SC_TEST("weapons: unretarded bombs from low level seldom go off; retarded ones do (the Falklands)") {
    Range r("nato", ObjectKind::Depot, "soft");
    double dumb = 0.0, retarded = 0.0;
    const int runs = 300;
    for (int i = 0; i < runs; ++i) {
        r.Reset();
        dumb += 1.0 - r.Strike("tornado_ids", 2, "mk82", 8, Delivery::Laydown, Profile::Low);
        r.Reset();
        retarded += 1.0 - r.Strike("tornado_ids", 2, "mk82r", 8, Delivery::Laydown, Profile::Low);
    }
    std::printf("    a depot's share destroyed by two Tornados laying eight bombs each: unretarded %.0f%%, retarded %.0f%%\n", 100.0 * dumb / runs,
                100.0 * retarded / runs);
    SC_EXPECT(dumb < 0.75 * retarded);
}

SC_TEST("weapons against history: a massive bridge stands against dumb bombs and falls to a strike of laser-guided ones (the Thanh Hoa bridge)") {
    Range r("nato", ObjectKind::Bridge, "very_hard");
    // 24 Paveways in one strike: a span down about every other time (27 April and 13 May 1972).
    int down = 0;
    const int strikes = 200;
    for (int i = 0; i < strikes; ++i) {
        r.Reset();
        down += r.Strike("f4e", 12, "gbu10", 2, Delivery::Dive, Profile::Medium) <= 0.5 + 1e-9 ? 1 : 0;
    }
    // Mk 84s from medium altitude for 871 sorties: a span down in a minority of the campaigns.
    int dropped = 0;
    const int campaigns = 20;
    for (int i = 0; i < campaigns; ++i) {
        r.Reset();
        for (int s = 0; s < 871 && r.truth[0].function > 0.5 + 1e-9; s += 4) r.Strike("f4e", 4, "mk84", 2, Delivery::Level, Profile::Medium);
        dropped += r.truth[0].function <= 0.5 + 1e-9 ? 1 : 0;
    }
    std::printf("    24 laser-guided bombs: a span down in %.0f%% of strikes; 871 sorties of Mk 84s: in %d of %d campaigns\n", 100.0 * down / strikes,
                dropped, campaigns);
    SC_EXPECT(down > strikes / 4 && down < 3 * strikes / 4);
    SC_EXPECT(dropped < campaigns / 2);
}
