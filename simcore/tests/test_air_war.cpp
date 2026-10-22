// The enemy air command (simcore/air_war/, data/air_war/, docs/air_war/).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;
const char* kFactions[] = {"rebels", "neutral", "warsaw_pact", "nato"};

AirWarData Load(const std::string& faction) {
    AirWarData d;
    std::string e;
    const bool ok = LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_" + faction + ".json", d, &e);
    if (!ok) std::printf("    %s\n", e.c_str());
    return d;
}

// Every package planned in the first `days` of a campaign.
std::vector<Package> PlanDays(const AirWarData& d, unsigned seed, double days, AirCampaign* out = nullptr) {
    AirCampaign c;
    c.Init(d, seed);
    while (c.Now() < days * 24.0) {
        if (c.Advance(days * 24.0)) c.SkipPlayerMission();
    }
    if (out != nullptr) *out = c;
    return c.Packages();
}

int CountAt(const std::vector<Package>& ps, DayPart part) {
    int n = 0;
    for (const Package& p : ps) n += p.part == part ? 1 : 0;
    return n;
}

}  // namespace

SC_TEST("air war: every faction loads with the theatre, the traits and the tuning; the staff is complete") {
    for (const char* f : kFactions) {
        AirWarData d;
        std::string e;
        SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", std::string("air_war/faction_") + f + ".json", d, &e));
        if (!e.empty()) std::printf("    %s: %s\n", f, e.c_str());
        SC_EXPECT(d.faction.officers.size() >= 3);
        SC_EXPECT(!d.quirks.empty());
        SC_EXPECT(d.theatre.PlayerBattery() != nullptr);
        AirStaff staff;
        Rng rng(5);
        staff.Build(d, rng, 0.0);
        SC_EXPECT(staff.Holder(StaffRole::Commander) >= 0 && staff.Holder(StaffRole::Planner) >= 0 &&
                  staff.Holder(StaffRole::Intelligence) >= 0);
        // The Pact has a political officer; only factions with SEAD have an EW/SEAD officer.
        SC_EXPECT((staff.Holder(StaffRole::Political) >= 0) == (std::string(f) == "warsaw_pact"));
        SC_EXPECT((staff.Holder(StaffRole::Sead) >= 0) == AirStaff::HasSead(d.faction));
        for (const SquadronDef& s : d.faction.squadrons) {
            const AircraftType* a = d.faction.Aircraft(s.aircraft);
            SC_EXPECT(a != nullptr);
            if (a != nullptr && !a->drone) SC_EXPECT(staff.LeaderOf(s.id) >= 0);
        }
    }
}

SC_TEST("air war: the loaders refuse unknown keys, bad values and unknown quirks") {
    FactionDef f;
    std::string e;
    SC_EXPECT(!LoadFactionJson("{\"faction_id\": \"x\", \"nmae\": \"typo\"}", f, &e) && e.find("unknown key") != std::string::npos);
    TheatreDef t;
    SC_EXPECT(!LoadTheatreJson("{\"theatre_id\": \"t\", \"name\": \"n\", \"source\": \"sam3d\", \"confidence\": \"guess\"}", t, &e) &&
              e.find("confidence") != std::string::npos);
    TuningTable tt;
    SC_EXPECT(!LoadAirWarJson("{\"source\": \"sam3d\", \"confidence\": \"estimate\", \"utility\": {\"valu_weight\": 1}}", tt, &e,
                              [](airwar::detail::AirWarLoader& l, const json::Value& r, TuningTable& o) { return l.LoadTuning(r, o); }) &&
              e.find("unknown tuning value") != std::string::npos);
    std::vector<QuirkDef> q;
    SC_EXPECT(!LoadAirWarJson("{\"source\": \"sam3d\", \"confidence\": \"estimate\", \"quirks\": [{\"id\": \"a\", \"name\": \"a\", "
                              "\"description\": \"d\", \"signature\": \"s\", \"intel_hint\": \"h\", \"effects\": {\"pref_noon\": 1}}]}",
                              q, &e, [](airwar::detail::AirWarLoader& l, const json::Value& r, std::vector<QuirkDef>& o) { return l.LoadQuirks(r, o); }) &&
              e.find("unknown effect") != std::string::npos);
    // Every number the code reads is in the balance file (so the file documents them all).
    TuningTable full;
    SC_EXPECT(LoadTuningFile(kData + "/gameplay/air_war.json", full, &e));
    for (const auto& kv : TuningTable::Defaults()) {
        if (full.values.count(kv.first) == 0) std::printf("    missing from data/gameplay/air_war.json: %s\n", kv.first.c_str());
        SC_EXPECT(full.values.count(kv.first) == 1);
    }
}

SC_TEST("air war: a campaign replays exactly for a seed, and differs for another") {
    const AirWarData d = Load("warsaw_pact");
    AirCampaign a, b, c;
    PlanDays(d, 11, 2.0, &a);
    PlanDays(d, 11, 2.0, &b);
    PlanDays(d, 12, 2.0, &c);
    SC_EXPECT(a.Journal().size() == b.Journal().size());
    bool same = a.Journal().size() == b.Journal().size();
    for (std::size_t i = 0; same && i < a.Journal().size(); ++i) same = a.Journal()[i].text == b.Journal()[i].text;
    SC_EXPECT(same);
    bool differs = a.Journal().size() != c.Journal().size();
    for (std::size_t i = 0; !differs && i < a.Journal().size(); ++i) differs = a.Journal()[i].text != c.Journal()[i].text;
    SC_EXPECT(differs);
}

SC_TEST("air war: the planner's habits show in the raids (dawn raider vs dusk raider)") {
    AirWarData d = Load("nato");
    // Make the planner's habit strong and the only one.
    auto set_planner = [&](const char* quirk) {
        for (OfficerDef& o : d.faction.officers) {
            if (o.role == StaffRole::Planner) o.quirks = {quirk};
        }
    };
    set_planner("dawn_raider");
    for (QuirkDef& q : d.quirks) {
        if (q.id == "dawn_raider") q.effects["pref_dawn"] = 4.0;
        if (q.id == "dusk_raider") q.effects["pref_dusk"] = 4.0;
    }
    const std::vector<Package> dawn = PlanDays(d, 3, 3.0);
    set_planner("dusk_raider");
    const std::vector<Package> dusk = PlanDays(d, 3, 3.0);
    std::printf("    dawn raider: %d of %zu at dawn; dusk raider: %d of %zu at dusk\n", CountAt(dawn, DayPart::Dawn), dawn.size(),
                CountAt(dusk, DayPart::Dusk), dusk.size());
    SC_EXPECT(CountAt(dawn, DayPart::Dawn) > CountAt(dawn, DayPart::Dusk));
    SC_EXPECT(CountAt(dusk, DayPart::Dusk) > CountAt(dusk, DayPart::Dawn));
}

SC_TEST("air war: each adversary flies its own kind of war") {
    // Rebels: no SEAD, no DEAD, no jammers; small packages; only light aircraft and helicopters.
    {
        const std::vector<Package> ps = PlanDays(Load("rebels"), 2, 4.0);
        SC_EXPECT(!ps.empty());
        for (const Package& p : ps) {
            SC_EXPECT(p.kind != MissionKind::Dead && p.kind != MissionKind::Hunter);
            SC_EXPECT(!p.Has(FlightRole::Sead) && !p.Has(FlightRole::Jammer));
            SC_EXPECT(p.Aircraft() <= 4);
        }
    }
    // Neutral: nothing deeper than its depth limit, nothing banned.
    {
        const AirWarData d = Load("neutral");
        const std::vector<Package> ps = PlanDays(d, 2, 5.0);
        const FrontDef* front = d.theatre.FrontFor("neutral");
        for (const Package& p : ps) {
            SC_EXPECT(DistanceToLine(front->line, p.target_pos) <= d.faction.politics.max_depth_m + 1.0);
            const TheatreObject* o = d.theatre.Find(p.target_object);
            if (o != nullptr) SC_EXPECT(o->kind != ObjectKind::City && o->kind != ObjectKind::PowerPlant && o->kind != ObjectKind::Headquarters);
        }
    }
    // NATO: DEAD and strike packages carry SEAD and jamming; power plants and towns are off limits at the start.
    {
        const std::vector<Package> ps = PlanDays(Load("nato"), 2, 2.0);
        int dead = 0, with_sead = 0, with_jam = 0;
        for (const Package& p : ps) {
            if (p.kind != MissionKind::Dead && p.kind != MissionKind::Strike) continue;
            ++dead;
            with_sead += p.Has(FlightRole::Sead) ? 1 : 0;
            with_jam += p.Has(FlightRole::Jammer) ? 1 : 0;
            SC_EXPECT(p.target_label.find("power station") == std::string::npos);
        }
        std::printf("    NATO strike/DEAD packages: %d, with SEAD %d, with jamming %d\n", dead, with_sead, with_jam);
        SC_EXPECT(dead > 0 && with_sead * 2 >= dead && with_jam > 0);
    }
    // The Pact: the opening air operation goes for airfields, command posts and radars, its strike groups in massed
    // blows (the hunters, scouts and decoys around them fly in pairs). Judged over eight campaigns: one campaign's first
    // day is a few dozen packages, and a platform's maths library sends it down its own path (on Windows the seeds give
    // 30-54% on those targets and 62-95% of the blows in massed groups; over eight, 41% and 77%).
    {
        const AirWarData pact = Load("warsaw_pact");
        int first = 0, aimed = 0, blows = 0, big = 0;
        for (unsigned seed = 1; seed <= 8; ++seed) {
            const int f0 = first, a0 = aimed, b0 = blows, g0 = big;
            for (const Package& p : PlanDays(pact, seed, 1.0)) {
                ++first;
                const std::string& l = p.target_label;
                aimed += (l.find("air base") != std::string::npos || l.find("command post") != std::string::npos ||
                          l.find("Radar") != std::string::npos || l.find("radar") != std::string::npos ||
                          l.find("battalion") != std::string::npos || l.find("headquarters") != std::string::npos)
                             ? 1
                             : 0;
                if (p.kind != MissionKind::Strike && p.kind != MissionKind::Dead) continue;
                ++blows;
                big += p.Aircraft() >= 6 ? 1 : 0;
            }
            std::printf("    Pact day 1, seed %u: %d packages, %d on air bases, command posts and radars; %d blows, %d massed\n", seed,
                        first - f0, aimed - a0, blows - b0, big - g0);
        }
        std::printf("    Pact day 1, eight campaigns: %d packages, %d on air bases, command posts and radars; %d strike and DEAD packages, %d of six aircraft or more\n",
                    first, aimed, blows, big);
        SC_EXPECT(first > 0 && aimed * 10 >= first * 3 && blows > 0 && big * 3 >= blows * 2);
    }
}

SC_TEST("air war: intelligence ages: a mobile fix spreads and fades, a fixed one stays") {
    const AirWarData d = Load("nato");
    IntelPicture pic;
    IntelReport mob;
    mob.arrive_h = 0.0;
    mob.object_id = "osa_21";
    mob.kind = ObjectKind::SamSite;
    mob.sigma_m = 300.0;
    mob.exists = 0.9;
    IntelReport fix = mob;
    fix.object_id = "kesmark_road_bridge";
    fix.kind = ObjectKind::Bridge;
    pic.Queue(mob);
    pic.Queue(fix);
    pic.Deliver(0.0, 0.5);
    for (int h = 0; h < 24; ++h) pic.Decay(1.0, d.tuning);
    const IntelEntry* m = pic.Find("osa_21");
    const IntelEntry* f = pic.Find("kesmark_road_bridge");
    SC_EXPECT(m != nullptr && f != nullptr);
    SC_EXPECT(m->sigma_m > 10000.0 && m->exists < 0.5);
    SC_EXPECT_NEAR(f->sigma_m, 300.0, 1.0);
    SC_EXPECT_NEAR(f->exists, 0.9, 1e-9);
    // A pilot's claim that a target is destroyed is believed less by sceptical analysts.
    IntelPicture a, b;
    for (IntelPicture* p : {&a, &b}) {
        p->Queue(fix);
        p->Deliver(0.0, 0.5);
    }
    IntelReport claim = fix;
    claim.arrive_h = 1.0;
    claim.observed_h = 1.0;
    claim.function = 0.0;
    claim.claim_destroyed = true;
    a.Queue(claim);
    b.Queue(claim);
    a.Deliver(1.0, 0.1);
    b.Deliver(1.0, 0.9);
    SC_EXPECT(a.Find("kesmark_road_bridge")->function < b.Find("kesmark_road_bridge")->function);
}

SC_TEST("air war: a battery that radiates is found; one that keeps silent and moves is not") {
    AirWarData d = Load("nato");
    for (TheatreObject& o : d.theatre.objects) {
        if (o.player) o.prior_known = 0.0;
    }
    double loud = 0.0, quiet = 0.0;
    for (unsigned seed = 1; seed <= 4; ++seed) {
        AirCampaign c;
        c.Init(d, seed);
        c.Posture().emission = 0.8;
        c.Posture().radio_discipline = 0.1;
        while (c.Now() < 36.0) {
            if (c.Advance(36.0)) c.SkipPlayerMission();
        }
        loud += c.EnemyKnowledgeOfBattery();
        AirCampaign s;
        s.Init(d, seed);
        s.Posture().emission = 0.0;
        s.Posture().radio_discipline = 1.0;
        s.Posture().relocate_every_h = 10.0;
        while (s.Now() < 36.0) {
            if (s.Advance(36.0)) s.SkipPlayerMission();
        }
        quiet += s.EnemyKnowledgeOfBattery();
    }
    std::printf("    enemy's knowledge of the battery after 36 h: radiating %.2f, silent and moving %.2f (sum of 4 seeds)\n", loud, quiet);
    SC_EXPECT(loud > quiet + 0.5);
}

SC_TEST("air war: an ARM without memory misses a radar that switched off; stale aim points and dummies take the bombs") {
    AirWarData d = Load("nato");
    AirCampaign base;
    base.Init(d, 1);
    // A hand-made package: a SEAD pair through the player's battery, a DEAD flight aimed at a stale point.
    const int bi = base.PlayerIndex();
    const Vec3 b = base.Truth()[static_cast<std::size_t>(bi)].pos;
    auto run = [&](double discipline, const Vec3& aim, int seed, PackageOutcome& out) {
        std::vector<ObjectState> truth = base.Truth();
        BatteryPosture posture;
        posture.emcon_discipline = discipline;
        AirStaff staff;
        Rng rs(9);
        staff.Build(d, rs, 0.0);
        Package p;
        p.id = 1;
        p.kind = MissionKind::Dead;
        p.target_object = d.theatre.objects[static_cast<std::size_t>(bi)].id;
        p.target_pos = aim;
        p.part = DayPart::Day;
        Flight sead;
        sead.callsign = "Magnum 1";
        sead.role = FlightRole::Sead;
        sead.aircraft = "f4e";  // Shrike: no memory (afmag_1989_slam)
        sead.count = 2;
        sead.arms = 4;
        sead.profile = Profile::Medium;
        sead.route = {{b + Vec3{-30000.0, 0.0, 0.0}, 4000.0, 0.0, "entry"}, {b, 4000.0, 0.1, "target"}, {b + Vec3{30000.0, 0.0, 0.0}, 4000.0, 0.2, "exit"}};
        sead.release_index = 1;
        Flight dead = sead;
        dead.callsign = "Hammer 1";
        dead.role = FlightRole::Dead;
        dead.aircraft = "f4e";
        dead.count = 4;
        dead.arms = 0;
        dead.route[1].pos = aim;
        dead.aim_pos = aim;
        p.flights = {sead, dead};
        Rng rng(static_cast<std::uint64_t>(seed));
        ResolveContext rc;
        rc.data = &d;
        rc.truth = &truth;
        rc.posture = &posture;
        rc.staff = &staff;
        rc.rng = &rng;
        Resolver r;
        out = r.Resolve(p, rc);
    };
    int killed_loose = 0, killed_tight = 0, empty = 0, decoy = 0;
    for (int seed = 1; seed <= 40; ++seed) {
        PackageOutcome o;
        run(0.0, b, seed, o);
        killed_loose += o.radars_killed.empty() ? 0 : 1;
        run(1.0, b + Vec3{0.0, 0.0, 0.0}, seed, o);
        killed_tight += o.radars_killed.empty() ? 0 : 1;
        run(0.5, b + Vec3{0.0, 6000.0, 0.0}, seed, o);  // where the battery was this morning, 6 km away
        empty += o.hit_nothing ? 1 : 0;
        const Vec3 dummy = base.Truth()[static_cast<std::size_t>(base.Dummies().front())].pos;
        run(0.5, dummy, seed, o);
        decoy += o.decoys_hit.empty() ? 0 : 1;
    }
    std::printf("    radar killed by ARMs: careless crew %d/40, disciplined %d/40; DEAD on a stale point hit nothing %d/40; on a dummy %d/40\n",
                killed_loose, killed_tight, empty, decoy);
    SC_EXPECT(killed_loose > killed_tight + 5);
    SC_EXPECT(empty >= 30);
    SC_EXPECT(decoy >= 20);
}

SC_TEST("air war: an ARM takes one of an Osa battery's four radars but blinds a one-radar site; guns at the target hit low attackers") {
    AirWarData d = Load("nato");
    AirCampaign base;
    base.Init(d, 1);
    // A SEAD pair with memory-ARMs flies over a site whose crew never switches off (the other sites' crews: 0.5).
    auto arm_run = [&](const std::string& site, int seed, double& function_after) {
        std::vector<ObjectState> truth = base.Truth();
        const int si = d.theatre.IndexOf(site);
        const Vec3 at = truth[static_cast<std::size_t>(si)].pos;
        BatteryPosture posture;
        posture.emcon_discipline = 0.0;
        AirStaff staff;
        Rng rs(9);
        staff.Build(d, rs, 0.0);
        Package p;
        p.id = 1;
        p.kind = MissionKind::Hunter;
        p.part = DayPart::Day;
        Flight sead;
        sead.callsign = "Weasel 1";
        sead.role = FlightRole::Sead;
        sead.aircraft = "f4g";
        sead.count = 2;
        sead.arms = 8;
        sead.profile = Profile::Medium;
        sead.route = {{at + Vec3{-30000.0, 0.0, 0.0}, 4000.0, 0.0, "entry"}, {at, 4000.0, 0.1, "target"},
                      {at + Vec3{30000.0, 0.0, 0.0}, 4000.0, 0.2, "exit"}};
        p.flights = {sead};
        Rng rng(static_cast<std::uint64_t>(seed));
        ResolveContext rc;
        rc.data = &d;
        rc.truth = &truth;
        rc.posture = &posture;
        rc.staff = &staff;
        rc.rng = &rng;
        Resolver r;
        const PackageOutcome o = r.Resolve(p, rc);
        // Only this site's radars (the route passes others).
        const bool hit = std::find(o.radars_killed.begin(), o.radars_killed.end(), site) != o.radars_killed.end();
        function_after = hit ? truth[static_cast<std::size_t>(si)].function : -1.0;
    };
    int osa_hit = 0, osa_out = 0, s125_hit = 0, s125_out = 0;
    for (int seed = 1; seed <= 60; ++seed) {
        double f = 0.0;
        arm_run(d.theatre.objects[static_cast<std::size_t>(base.PlayerIndex())].id, seed, f);
        if (f >= 0.0) {
            ++osa_hit;
            osa_out += f <= 0.0 ? 1 : 0;
        }
        arm_run("kecel_s125", seed, f);
        if (f >= 0.0) {
            ++s125_hit;
            s125_out += f <= 0.0 ? 1 : 0;
        }
    }
    std::printf("    ARM hits: the Osa battery %d (out of the fight %d), the S-125 %d (out %d)\n", osa_hit, osa_out, s125_hit, s125_out);
    SC_EXPECT(osa_hit > 10 && osa_out * 4 < osa_hit);
    SC_EXPECT(s125_hit > 5 && s125_out == s125_hit);

    // Light guns at a target without an air defence of its own: troops more than a depot, only low and helicopters.
    const TuningTable& t = d.tuning;
    const double troops = TargetGunsChance(ObjectKind::Troops, Profile::Low, false, 0.0, 1.0, t);
    const double depot = TargetGunsChance(ObjectKind::Depot, Profile::Low, false, 0.0, 1.0, t);
    SC_EXPECT_NEAR(troops, t.Get("attrition.target_guns_troops"), 1e-12);
    SC_EXPECT(troops > depot && depot > 0.0);
    SC_EXPECT(TargetGunsChance(ObjectKind::Troops, Profile::Medium, false, 0.0, 1.0, t) == 0.0);
    SC_EXPECT(TargetGunsChance(ObjectKind::Troops, Profile::Medium, true, 0.0, 1.0, t) == troops);  // a helicopter is always low
    SC_EXPECT(TargetGunsChance(ObjectKind::City, Profile::Low, false, 0.0, 1.0, t) == 0.0);
    SC_EXPECT_NEAR(TargetGunsChance(ObjectKind::Troops, Profile::Low, false, 1.0, 0.7, t), troops * 0.5 * 0.7, 1e-12);
    // In the resolver: four strikers at low on the southern group lose aircraft over many raids; at medium, none.
    auto strike_run = [&](Profile prof, int seed) {
        std::vector<ObjectState> truth = base.Truth();
        const int ti = d.theatre.IndexOf("group_south");
        const Vec3 at = truth[static_cast<std::size_t>(ti)].pos;
        BatteryPosture posture;
        AirStaff staff;
        Rng rs(9);
        staff.Build(d, rs, 0.0);
        Package p;
        p.id = 2;
        p.kind = MissionKind::Strike;
        p.part = DayPart::Day;
        p.target_object = "group_south";
        Flight f;
        f.callsign = "Hog 1";
        f.role = FlightRole::Strike;
        f.aircraft = "a10a";
        f.count = 4;
        f.profile = prof;
        // Far from every SAM: the guns at the target are the only threat.
        f.route = {{at + Vec3{0.0, -20000.0, 0.0}, 100.0, 0.0, "entry"}, {at, 100.0, 0.1, "target"}, {at + Vec3{0.0, -20000.0, 0.0}, 100.0, 0.2, "exit"}};
        f.release_index = 1;
        f.aim_pos = at;
        p.flights = {f};
        Rng rng(static_cast<std::uint64_t>(seed));
        ResolveContext rc;
        rc.data = &d;
        rc.truth = &truth;
        rc.posture = &posture;
        rc.staff = &staff;
        rc.rng = &rng;
        Resolver r;
        int by_guns = 0;
        for (const LossRecord& l : r.Resolve(p, rc).losses) by_guns += l.by == "group_south" ? 1 : 0;
        return by_guns;
    };
    int low = 0, medium = 0;
    for (int seed = 1; seed <= 200; ++seed) {
        low += strike_run(Profile::Low, seed);
        medium += strike_run(Profile::Medium, seed);
    }
    std::printf("    guns at the troops: %d aircraft lost in 200 low attacks of four, %d at medium\n", low, medium);
    SC_EXPECT(low > 4 && medium == 0);
}

SC_TEST("air war: a wrecked object is out until its replacement comes; no raid is offered while the battery is repaired; the Pact surges") {
    const AirWarData d = Load("warsaw_pact");
    const double restored = d.tuning.Get("repair.restored_function");
    int checked = 0, held = 0, back = 0, offered_wrecked = 0;
    double sorties_d1 = 0.0, sorties_d4 = 0.0;
    for (unsigned seed = 1; seed <= 4; ++seed) {
        AirCampaign c;
        c.Init(d, seed);
        c.Posture().emission = 0.5;  // careless: the battery gets wrecked
        std::vector<double> wrecked_at(c.Truth().size(), -1.0);
        while (c.Now() < 6 * 24.0) {
            if (c.Advance(std::floor(c.Now()) + 1.0)) {
                offered_wrecked += c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].function <= 0.3 ? 1 : 0;
                c.SkipPlayerMission();
            }
            for (std::size_t i = 0; i < c.Truth().size(); ++i) {
                const ObjectState& st = c.Truth()[i];
                const TheatreObject& o = c.Data().theatre.objects[i];
                if (o.decoy || o.repair_h <= 0.0) continue;
                if (wrecked_at[i] < 0.0 && st.function <= 0.0 && st.wrecked_h >= 0.0) wrecked_at[i] = st.wrecked_h;
                if (wrecked_at[i] < 0.0) continue;
                const double due = wrecked_at[i] + o.repair_h;
                if (c.Now() < due - 1.0) {
                    ++checked;
                    held += st.function <= 0.0 ? 1 : 0;
                } else if (c.Now() >= due + 1.0 && c.Now() < due + 2.0) {
                    back += st.function >= restored - 1e-9 ? 1 : 0;
                    wrecked_at[i] = -1.0;  // it may be wrecked again later
                }
            }
        }
        for (const Package& p : c.Packages()) {
            if (!p.resolved) continue;
            if (p.tot_h < 24.0) sorties_d1 += p.Aircraft();
            if (p.tot_h >= 72.0 && p.tot_h < 96.0) sorties_d4 += p.Aircraft();
        }
    }
    std::printf("    wrecked objects: %d of %d hours out before the replacement came; %d came back; raids offered to a wrecked battery: %d\n",
                held, checked, back, offered_wrecked);
    std::printf("    the Pact's sorties: day 1 %.0f, day 4 %.0f (four campaigns)\n", sorties_d1, sorties_d4);
    SC_EXPECT(checked > 50 && held == checked);
    SC_EXPECT(back > 3);
    SC_EXPECT(offered_wrecked == 0);
    SC_EXPECT(sorties_d1 > 1.5 * sorties_d4);
}

SC_TEST("air war: the believed air defences are the share of known sites working; the rollback resumes when they come back") {
    IntelPicture pic;
    auto site = [](const std::string& id, double exists, double function) {
        IntelEntry e;
        e.key = id;
        e.object_id = id;
        e.kind = ObjectKind::SamSite;
        e.exists = exists;
        e.function = function;
        e.threat_radius_m = 10000.0;
        e.threat_lethality = 0.2;
        e.threat_radar = true;
        return e;
    };
    SC_EXPECT_NEAR(pic.BelievedIadsShare(), 1.0, 1e-12);  // none known: nothing broken
    pic.entries.push_back(site("a", 1.0, 1.0));
    pic.entries.push_back(site("b", 1.0, 0.0));
    SC_EXPECT_NEAR(pic.BelievedIadsShare(), 0.5, 1e-12);
    pic.entries.push_back(site("c", 1.0, 1.0));  // found later, working: one more of the known three
    SC_EXPECT_NEAR(pic.BelievedIadsShare(), 2.0 / 3.0, 1e-12);
    pic.entries.back().suspected_decoy = 1.0;  // probably a dummy: it weighs a third as much
    SC_EXPECT_NEAR(pic.BelievedIadsShare(), (1.0 + 0.3) / (2.0 + 0.3), 1e-12);
    IntelEntry guns = site("g", 1.0, 1.0);
    guns.threat_radar = false;  // guns are not part of the rollback
    pic.entries.push_back(guns);
    SC_EXPECT_NEAR(pic.BelievedIadsShare(), (1.0 + 0.3) / (2.0 + 0.3), 1e-12);
    IntelEntry shilka = site("z", 1.0, 0.0);
    shilka.kind = ObjectKind::Aaa;  // radar-laid guns neither (the nation's Shilkas, S-60s and KS-30s)
    pic.entries.push_back(shilka);
    SC_EXPECT_NEAR(pic.BelievedIadsShare(), (1.0 + 0.3) / (2.0 + 0.3), 1e-12);

    // NATO's rollback ends when the defences are believed broken and resumes when they are believed back.
    const AirWarData d = Load("nato");
    int ended = 0, resumed = 0;
    for (unsigned seed = 1; seed <= 10; ++seed) {
        AirCampaign c;
        c.Init(d, seed);
        c.Posture().emission = 0.5;
        while (c.Now() < 7 * 24.0) {
            if (c.Advance(7 * 24.0)) c.SkipPlayerMission();
        }
        for (const JournalEntry& j : c.Journal()) {
            ended += j.text.find("the air defences are believed broken") != std::string::npos ? 1 : 0;
            resumed += j.text.find("The air defences are working again") != std::string::npos ? 1 : 0;
        }
    }
    std::printf("    NATO over ten weeks: the rollback ended %d times and resumed %d times\n", ended, resumed);
    SC_EXPECT(ended > 0 && resumed > 0 && resumed <= ended);
}

SC_TEST("air war: officers learn at their own pace, and the staff replaces whoever falls") {
    const AirWarData d = Load("warsaw_pact");
    Officer rigid, flexible;
    rigid.def.traits.rigidity = 0.9;
    flexible.def.traits.rigidity = 0.1;
    SC_EXPECT(rigid.LearningRate(0.5) < flexible.LearningRate(0.5) * 0.5);
    AirStaff staff;
    Rng rng(3);
    staff.Build(d, rng, 0.0);
    const int cdr = staff.Holder(StaffRole::Commander);
    staff.officers[static_cast<std::size_t>(cdr)].def.traits.boldness = 0.95;
    const int succ = staff.Replace(cdr, "relieved", "the losses", d, rng, 10.0);
    SC_EXPECT(succ >= 0 && staff.Holder(StaffRole::Commander) == succ);
    SC_EXPECT(!staff.officers[static_cast<std::size_t>(cdr)].active && staff.officers[static_cast<std::size_t>(cdr)].fate == "relieved");
    SC_EXPECT(staff.officers[static_cast<std::size_t>(succ)].def.traits.boldness < 0.8);  // chosen against the failing
    SC_EXPECT(staff.officers[static_cast<std::size_t>(succ)].def.name != staff.officers[static_cast<std::size_t>(cdr)].def.name);
}

SC_TEST("air war: a raid through the battery's sector becomes a scenario the loader accepts and the runner plays") {
    const AirWarData d = Load("nato");
    AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    c.Posture().emission = 0.1;
    c.Posture().relocate_every_h = 20.0;
    int played = 0;
    while (c.Now() < 6.0 * 24.0 && played < 2) {
        if (!c.Advance(6.0 * 24.0)) continue;
        const Package* p = c.PlayerMission();
        RaidScenarioBuilder builder;
        const RaidScenario rs = builder.Build(c, *p);
        if (!rs.ok) {
            c.SkipPlayerMission();
            continue;
        }
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        const bool loaded = LoadScenario(rs.json_text, kData, def, console, &e);
        if (!loaded) std::printf("    %s\n", e.c_str());
        SC_EXPECT(loaded);
        if (!loaded) break;
        SC_EXPECT(def.targets.size() == rs.manifest.aircraft.size());
        SC_EXPECT(def.npc_commander && def.has_battery);
        ScenarioRunner runner(std::move(def), std::move(console));
        MissionBridge bridge(rs.manifest, c.Data(), 5u);
        while (!runner.Finished() && !bridge.UnitDestroyed() && runner.Time() < rs.manifest.end_s + 60.0) {
            runner.Step(0.1);
            bridge.Step(runner);
        }
        SC_EXPECT(runner.Finished() || bridge.UnitDestroyed());
        const PackageOutcome o = bridge.Finish(runner);
        SC_EXPECT(o.package_id == p->id && o.played);
        SC_EXPECT(bridge.EmittedSeconds() > 0.0);  // the NPC crew searched: the enemy's ELINT has something to hear
        const std::size_t journal_before = c.Journal().size();
        c.CompletePlayerMission(o, rs.manifest.start_h + runner.Time() / 3600.0);
        SC_EXPECT(!c.HasPlayerMission() && c.Journal().size() > journal_before);
        ++played;
    }
    std::printf("    played %d generated raids end to end\n", played);
    SC_EXPECT(played >= 1);
}

SC_TEST("air war: SEAD aircraft with a warning receiver carry EM anti-radiation missiles, which the bridge reads back (A11)") {
    for (const char* adversary : {"nato", "warsaw_pact"}) {
        const AirWarData d = Load(adversary);
        AirCampaign c;
        SC_EXPECT(c.Init(d, 2));
        c.Posture().emission = 0.9;  // a radiating battery draws the suppression
        int raids = 0, with_arms = 0, em_fired = 0, abstract_fired = 0, unit_hit = 0;
        while (c.Now() < 10.0 * 24.0 && with_arms < 2) {
            if (!c.Advance(10.0 * 24.0)) continue;
            const Package* p = c.PlayerMission();
            RaidScenarioBuilder builder;
            const RaidScenario rs = builder.Build(c, *p);
            bool carriers = false;
            for (const RaidAircraft& a : rs.manifest.aircraft) carriers = carriers || a.em_arms;
            if (!rs.ok || !carriers) {
                c.SkipPlayerMission();
                continue;
            }
            ++raids;
            ScenarioDef def;
            ConsoleData console;
            std::string e;
            const bool loaded = LoadScenario(rs.json_text, kData, def, console, &e);
            if (!loaded) std::printf("    %s\n", e.c_str());
            SC_EXPECT(loaded);
            if (!loaded) break;
            bool written = false;
            for (const ScenarioTarget& t : def.targets) written = written || t.arms.count > 0;
            SC_EXPECT(written);
            SC_EXPECT(def.em.library.Arm(std::string(adversary) == "nato" ? "agm88a_class_harm" : "kh58_class") >= 0);
            ++with_arms;
            ScenarioRunner runner(std::move(def), std::move(console));
            MissionBridge bridge(rs.manifest, c.Data(), 5u);
            while (!runner.Finished() && !bridge.UnitDestroyed() && runner.Time() < rs.manifest.end_s + 60.0) {
                runner.Step(0.1);
                bridge.Step(runner);
            }
            const PackageOutcome o = bridge.Finish(runner);
            em_fired += runner.ArmsFired();
            abstract_fired += o.arms_fired - runner.ArmsFired();
            for (const auto& a : runner.ArmResults()) unit_hit += a.hit && a.hit_what == "unit" ? 1 : 0;
            if (runner.EndReason() == "the vehicle was hit by an anti-radiation missile") SC_EXPECT(bridge.UnitDestroyed() && !o.radars_killed.empty());
            c.CompletePlayerMission(o, rs.manifest.start_h + runner.Time() / 3600.0);
        }
        std::printf("    %s: %d raids with EM ARM carriers (%d played), %d EM ARMs fired (%d on the vehicle), %d by the bridge's model\n",
                    adversary, with_arms, raids, em_fired, unit_hit, abstract_fired);
        SC_EXPECT(with_arms >= 1);
        // Every carrier with a warning receiver fires through the EM layer: the bridge's model only for the others.
        for (const AircraftType& t : d.faction.aircraft) {
            if (t.arms > 0 && !t.rwr_em.empty()) SC_EXPECT(!t.arm_em.empty());
        }
    }
}

SC_TEST("air war: once our radars have come under ARM attack the regiment orders the ARM drill, and the raids carry it (E6)") {
    const AirWarData d = Load("nato");
    AirCampaign c;
    SC_EXPECT(c.Init(d, 3));
    SC_EXPECT(!c.ArmsExpected());  // nothing seen yet: no drill ordered
    c.Posture().emission = 0.9;    // a radiating battery draws the suppression
    int before = 0, before_drill = 0, after = 0, after_drill = 0;
    while (c.Now() < 20.0 * 24.0 && after < 2) {
        if (!c.Advance(20.0 * 24.0)) continue;
        const bool expected = c.ArmsExpected();
        const Package* p = c.PlayerMission();
        RaidScenarioBuilder builder;
        const RaidScenario rs = builder.Build(c, *p);
        c.SkipPlayerMission();
        if (!rs.ok) continue;
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        if (!LoadScenario(rs.json_text, kData, def, console, &e)) {
            std::printf("    %s\n", e.c_str());
            SC_EXPECT(false);
            break;
        }
        (expected ? after : before) += 1;
        (expected ? after_drill : before_drill) += def.doctrine.arm_threat ? 1 : 0;
    }
    bool ordered = false;
    for (const JournalEntry& j : c.Journal()) ordered = ordered || (j.who == "regiment" && j.visible && j.text.find("ARM drill") != std::string::npos);
    std::printf("    raids before the first ARM attack: %d (with the drill %d); after: %d (with the drill %d)\n", before, before_drill, after,
                after_drill);
    SC_EXPECT(c.ArmsExpected() && ordered);
    SC_EXPECT(before_drill == 0);
    SC_EXPECT(after >= 1 && after_drill == after);
}

SC_TEST("air war: on the map a raid sets the battery and its dummies on open ground, and the runs at them follow") {
    std::string e;
    const std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
    if (!map) std::printf("    %s\n", e.c_str());
    SC_EXPECT(map != nullptr);
    if (!map) return;
    // The battery's campaign position inside a wood (the nearest to its own; the theatre and the map share their frame).
    AirWarData d = Load("warsaw_pact");
    int pi = -1;
    for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
        if (d.theatre.objects[i].player) pi = static_cast<int>(i);
    }
    SC_EXPECT(pi >= 0);
    if (pi < 0) return;
    Vec3& bpos = d.theatre.objects[static_cast<std::size_t>(pi)].pos;
    GeoPoint torigin = d.theatre.origin;
    torigin.alt_m = d.theatre.ground_height_m;
    const MapTerrain theatre_ground(map, torigin);
    bool wood = false;
    for (double r = 0.0; r < 6000.0 && !wood; r += 50.0) {
        for (int k = 0; k < 16 && !wood; ++k) {
            const Vec3 at = bpos + Vec3{r * std::sin(k * kPi / 8.0), r * std::cos(k * kPi / 8.0), 0.0};
            if (theatre_ground.ObstacleHeightAt(at.x, at.y) > 12.0) {
                bpos = at;
                wood = true;
            }
        }
    }
    SC_EXPECT(wood);
    AirCampaign c;
    SC_EXPECT(c.Init(d, 2u));
    c.Posture().emission = 1.0;  // a battery that radiates all day is found, and raids come for it
    c.Posture().relocate_every_h = 0.0;
    std::set<std::string> dummy_ids;
    for (int di : c.Dummies()) dummy_ids.insert(c.Data().theatre.objects[static_cast<std::size_t>(di)].id);
    const std::string battery_id = c.Data().theatre.objects[static_cast<std::size_t>(c.PlayerIndex())].id;
    ScenarioFormat in_place;  // on the map's ground, but exactly where the campaign has the battery
    in_place.map_file = "maps/kesmark_range.json";
    ScenarioFormat open_ground = in_place;
    open_ground.map = map;
    int raids = 0, at_battery = 0, elsewhere = 0;
    for (int day = 0; day < 20 && (raids == 0 || at_battery == 0); ++day) {
        if (!c.Advance(c.Now() + 24.0) || !c.HasPlayerMission()) continue;
        const Package& p = *c.PlayerMission();
        RaidScenarioBuilder builder;
        const RaidScenario where = builder.Build(c, p, in_place);
        const RaidScenario open = builder.Build(c, p, open_ground);
        if (!where.ok || !open.ok) {
            c.SkipPlayerMission();
            continue;
        }
        ++raids;
        // In place, the player's vehicle stands in the wood; on open ground no vehicle or dummy does, within 2.5 km of it.
        const MapTerrain where_ground(map, where.manifest.origin);
        const MapTerrain open_ground_at(map, open.manifest.origin);
        if (raids == 1) SC_EXPECT(where_ground.ObstacleHeightAt(0.0, 0.0) > 12.0);
        SC_EXPECT(where.manifest.own_moved.HorizontalLength() == 0.0);
        const double moved = open.manifest.own_moved.HorizontalLength();
        SC_EXPECT(moved > 0.0 && moved <= 2500.0);
        SC_EXPECT(open_ground_at.ObstacleHeightAt(0.0, 0.0) <= 3.0);
        for (const auto& v : open.manifest.battery) SC_EXPECT(open_ground_at.ObstacleHeightAt(v.second.x, v.second.y) <= 3.0);
        for (const auto& dm : open.manifest.dummies) SC_EXPECT(open_ground_at.ObstacleHeightAt(dm.second.x, dm.second.y) <= 3.0);
        // A run at the battery keeps its aim relative to the battery; one at anything else keeps its place on the map.
        for (const RaidAircraft& a : open.manifest.aircraft) {
            const RaidAircraft* w = nullptr;
            for (const RaidAircraft& x : where.manifest.aircraft) {
                if (x.target_id == a.target_id) w = &x;
            }
            if (w == nullptr) continue;
            const std::string& key = p.flights[static_cast<std::size_t>(a.flight)].aim_key;
            if (key == battery_id) {
                ++at_battery;
                SC_EXPECT((a.aim_enu - w->aim_enu).HorizontalLength() < 5.0);
            } else if (dummy_ids.count(key) == 0) {
                ++elsewhere;
                SC_EXPECT((a.aim_enu + open.manifest.own_moved - w->aim_enu).HorizontalLength() < 5.0);
            }
        }
        ScenarioDef def;
        ConsoleData console;
        const bool loaded = LoadScenario(open.json_text, kData, def, console, &e);
        if (!loaded) std::printf("    %s\n", e.c_str());
        SC_EXPECT(loaded);
        if (raids == 1) std::printf("    the battery set %.0f m off the wood\n", moved);
        c.SkipPlayerMission();
    }
    std::printf("    %d raids: %d aircraft aimed at the battery, %d at other things\n", raids, at_battery, elsewhere);
    SC_EXPECT(raids >= 1 && at_battery >= 1);
}

SC_TEST("air war: a saved campaign loads to the very same campaign (seed and actions replayed)") {
    const AirWarData d = Load("warsaw_pact");
    AirCampaign c;
    SC_EXPECT(c.Init(d, 21));
    int played = 0, skipped = 0;
    for (int day = 0; day < 4; ++day) {
        c.Posture().emission = day % 2 == 0 ? 0.1 : 0.6;
        c.Posture().dummy_traffic = day % 2 == 1;
        while (c.Advance(c.Now() + 24.0 * (day + 1) - c.Now() + 0.37)) {
            if ((played + skipped) % 2 == 0) {
                // A played raid's outcome, as a bridge would give it: one aircraft down near the battery, a dummy hit.
                PackageOutcome o;
                o.package_id = c.PlayerMission()->id;
                o.played = true;
                LossRecord l;
                l.flight = 0;
                l.by = d.theatre.objects[static_cast<std::size_t>(c.PlayerIndex())].id;
                l.where = Vec3{1234.5678, -987.654321, 0.0};
                l.captured = true;
                o.losses.push_back(l);
                o.decoys_hit.push_back(d.theatre.objects[static_cast<std::size_t>(c.Dummies().front())].id);
                o.emissions.push_back({l.by, 123.456789});
                o.released_flights.push_back(0);
                c.CompletePlayerMission(o, c.Now() + 0.123456789);
                ++played;
            } else {
                c.SkipPlayerMission();
                ++skipped;
            }
        }
        if (day == 1) {
            const Vec3 b = c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos;
            c.RelocateBattery(b + Vec3{3000.0, 1000.0, 0.0});
        }
    }
    std::printf("    %d raids played, %d let pass, %zu actions\n", played, skipped, c.Actions().size());
    SC_EXPECT(played + skipped > 0);
    const std::string text = SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json");
    AirCampaign loaded;
    std::string e;
    SC_EXPECT(LoadCampaignJson(text, kData, loaded, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT_NEAR(loaded.Now(), c.Now(), 0.0);
    SC_EXPECT(loaded.Journal().size() == c.Journal().size());
    bool same = loaded.Journal().size() == c.Journal().size();
    for (std::size_t i = 0; same && i < c.Journal().size(); ++i) same = loaded.Journal()[i].text == c.Journal()[i].text;
    SC_EXPECT(same);
    SC_EXPECT(loaded.Packages().size() == c.Packages().size() && loaded.AircraftLost() == c.AircraftLost());
    const Vec3 a = loaded.Truth()[static_cast<std::size_t>(loaded.PlayerIndex())].pos;
    const Vec3 b = c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos;
    SC_EXPECT_NEAR(Dist2d(a, b), 0.0, 0.0);
    // And it carries on identically.
    loaded.Advance(loaded.Now() + 12.0);
    c.Advance(c.Now() + 12.0);
    SC_EXPECT(loaded.Journal().size() == c.Journal().size());
    // A save that doesn't fit the data is refused.
    std::string bad = text;
    const std::size_t at = bad.find("\"kind\": \"skip\"");
    if (at != std::string::npos) {
        bad.replace(at, 14, "\"kind\": \"jump\"");
        SC_EXPECT(!LoadCampaignJson(bad, kData, loaded, &e) && e.find("unknown action") != std::string::npos);
    }
}

SC_TEST("air war on the nation's theatre: the rollback hunts SAM sites, never guns; the battery deploys to the war; the neutral spares its own oil") {
    // The nation on the map, N4's first read (docs/air_war/09_BALANCE.md §8).
    const auto load = [](const std::string& faction) {
        AirWarData d;
        std::string e;
        if (!LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_" + faction + ".json", d, &e)) {
            std::printf("    %s\n", e.c_str());
        }
        return d;
    };
    for (const char* f : {"warsaw_pact", "nato"}) {
        const AirWarData d = load(f);
        AirCampaign c;
        const std::vector<Package> ps = PlanDays(d, 3, 2.0, &c);
        // The nation's Shilkas, S-60s and KS-30s lay their guns by radar: still guns, flown round or over, never hunted.
        int hunted = 0, guns = 0;
        for (const Package& p : ps) {
            if (p.kind != MissionKind::Dead && p.kind != MissionKind::Hunter && p.kind != MissionKind::Deception) continue;
            const IntelEntry* e = c.Intel().Find(p.target_key);
            if (e == nullptr) continue;
            ++hunted;
            guns += e->kind == ObjectKind::Aaa ? 1 : 0;
        }
        std::printf("    %s: %d DEAD, hunter and deception packages in two days, %d at guns\n", f, hunted, guns);
        SC_EXPECT(hunted > 5 && guns == 0);
        // In a war on this front battery 21 stands where the enemy raids, not at Kesmark.
        const FrontDef* front = d.theatre.FrontFor(f);
        SC_EXPECT(front != nullptr && front->has_battery_position);
        if (front != nullptr) SC_EXPECT(Dist2d(c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos, front->battery_position) < 1.0);
    }
    // The neutral neighbour's smaller oil line is its own: it closes the valve, it doesn't bomb the station.
    const AirWarData nd = load("neutral");
    const std::vector<ObjectKind>& banned = nd.faction.politics.banned;
    SC_EXPECT(std::find(banned.begin(), banned.end(), ObjectKind::PipelineStation) != banned.end());
    for (const Package& p : PlanDays(nd, 4, 4.0)) {
        const TheatreObject* o = nd.theatre.Find(p.target_object);
        if (o != nullptr) SC_EXPECT(o->kind != ObjectKind::PipelineStation);
    }
}

SC_TEST("air war: a defence engages as many of a passing flight as it has fire channels") {
    AirWarData d = Load("nato");
    AirCampaign base;
    base.Init(d, 1);
    const int si = d.theatre.IndexOf("kecel_s125");
    SC_EXPECT(si >= 0);
    if (si < 0) return;
    TheatreObject& site = d.theatre.objects[static_cast<std::size_t>(si)];
    site.defence.lethality = 1.0;  // every engaged aircraft at the chance's cap
    const Vec3 at = base.Truth()[static_cast<std::size_t>(si)].pos;
    // Four strikers at medium straight over it, north to south, past no other defence's reach.
    auto run = [&](int channels, int seed) {
        site.defence.channels = channels;
        std::vector<ObjectState> truth = base.Truth();
        BatteryPosture posture;
        AirStaff staff;
        Rng rs(9);
        staff.Build(d, rs, 0.0);
        Package p;
        p.id = 1;
        p.kind = MissionKind::Strike;
        p.part = DayPart::Day;
        Flight fl;
        fl.callsign = "Hammer 1";
        fl.role = FlightRole::Strike;
        fl.aircraft = "f4e";
        fl.count = 4;
        fl.profile = Profile::Medium;
        fl.route = {{at + Vec3{0.0, -30000.0, 0.0}, 4000.0, 0.0, "entry"}, {at + Vec3{0.0, 10000.0, 0.0}, 4000.0, 0.1, "exit"}};
        p.flights = {fl};
        Rng rng(static_cast<std::uint64_t>(seed));
        ResolveContext rc;
        rc.data = &d;
        rc.truth = &truth;
        rc.posture = &posture;
        rc.staff = &staff;
        rc.rng = &rng;
        Resolver r;
        return r.Resolve(p, rc).Lost();
    };
    int most_one = 0, most_all = 0, total_one = 0, total_all = 0;
    for (int seed = 1; seed <= 40; ++seed) {
        const int one = run(1, seed), all = run(0, seed);
        most_one = std::max(most_one, one);
        most_all = std::max(most_all, all);
        total_one += one;
        total_all += all;
    }
    std::printf("    four strikers over an S-125, 40 passes: one channel %d lost (at most %d a pass), every aircraft engaged %d (at most %d)\n",
                total_one, most_one, total_all, most_all);
    SC_EXPECT(most_one == 1 && most_all > 1 && total_all > 2 * total_one);
    // The planner's estimate follows the same rule, and a damaged Osa battery engages fewer.
    SC_EXPECT(EngagedAircraft(0, 1.0, 4) == 4 && EngagedAircraft(1, 1.0, 4) == 1 && EngagedAircraft(4, 1.0, 2) == 2);
    SC_EXPECT(EngagedAircraft(4, 0.5, 4) == 2 && EngagedAircraft(1, 0.2, 4) == 1);
}

SC_TEST("air war: after a played raid the rest is resolved, the way in included, all but what the scenario flew") {
    ResolveContext c;
    c.played_centre = Vec3{0.0, 0.0, 0.0};
    c.played_radius_m = 70000.0;
    c.played_from_h = 10.0;
    c.played_to_h = 10.5;
    const auto pieces = [&](const Vec3& a, double ta, const Vec3& b, double tb) {
        return ResolvedPieces(RoutePoint{a, 1000.0, ta, ""}, RoutePoint{b, 1000.0, tb, ""}, c);
    };
    // Through the circle while it was played: the way in and the way out, not the middle.
    std::vector<LegPiece> through = pieces(Vec3{-200000.0, 0.0, 0.0}, 9.8, Vec3{200000.0, 0.0, 0.0}, 10.6);
    SC_EXPECT(through.size() == 2);
    if (through.size() == 2) {
        SC_EXPECT_NEAR(through[0].a, 0.0, 1e-12);
        SC_EXPECT_NEAR(through[0].b, 130.0 / 400.0, 1e-9);
        SC_EXPECT_NEAR(through[1].a, 270.0 / 400.0, 1e-9);
        SC_EXPECT_NEAR(through[1].b, 1.0, 1e-12);
    }
    // Through it after the scenario ended (the way home): all of it.
    SC_EXPECT(pieces(Vec3{-200000.0, 0.0, 0.0}, 11.0, Vec3{200000.0, 0.0, 0.0}, 11.8).size() == 1);
    // Wholly inside while played: nothing; wholly outside: all of it; without a played raid: all of it.
    SC_EXPECT(pieces(Vec3{-10000.0, 0.0, 0.0}, 10.1, Vec3{10000.0, 0.0, 0.0}, 10.2).empty());
    SC_EXPECT(pieces(Vec3{-200000.0, 100000.0, 0.0}, 10.1, Vec3{200000.0, 100000.0, 0.0}, 10.2).size() == 1);
    ResolveContext none;
    SC_EXPECT(ResolvedPieces(RoutePoint{Vec3{}, 1000.0, 10.1, ""}, RoutePoint{Vec3{1000.0, 0.0, 0.0}, 1000.0, 10.2, ""}, none).size() == 1);
}

SC_TEST("air war on the nation's theatre: in a played raid the neighbours fire and the nearest warning posts report") {
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_nato.json", d, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    int played = 0, neighbours = 0, fired = 0, kills = 0, credited = 0;
    for (unsigned seed = 1; seed <= 4 && played < 3; ++seed) {
        AirCampaign c;
        SC_EXPECT(c.Init(d, seed));
        while (c.Now() < 4.0 * 24.0 && played < 3) {
            if (!c.Advance(4.0 * 24.0)) continue;
            const Package* p = c.PlayerMission();
            RaidScenarioBuilder builder;
            const RaidScenario rs = builder.Build(c, *p);
            if (!rs.ok) {
                c.SkipPlayerMission();
                continue;
            }
            // The warning nets: one post of each kind, the nearest that works.
            json::Value root;
            SC_EXPECT(json::Parse(rs.json_text, root, &e));
            std::vector<std::string> kinds;
            if (const json::Value* c2 = root.Find("c2")) {
                if (const json::Value* src = c2->Find("sources")) {
                    for (const json::Value& s : src->items) {
                        const std::string kind = s.Find("type")->string;
                        SC_EXPECT(std::find(kinds.begin(), kinds.end(), kind) == kinds.end());
                        kinds.push_back(kind);
                        if (kind != "ewr_voice") continue;
                        // No working radar company nearer than the one reporting.
                        const Vec3 b = c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos;
                        const int ri = d.theatre.IndexOf(s.Find("id")->string);
                        const double dr = Dist2d(c.Truth()[static_cast<std::size_t>(ri)].pos, b);
                        for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
                            if (d.theatre.objects[i].c2_source != "ewr_voice" || c.Truth()[i].function <= 0.3) continue;
                            SC_EXPECT(Dist2d(c.Truth()[i].pos, b) >= dr - 1.0);
                        }
                    }
                }
            }
            SC_EXPECT(std::find(kinds.begin(), kinds.end(), "ewr_voice") != kinds.end());
            neighbours += static_cast<int>(rs.manifest.neighbours.size());
            ScenarioDef def;
            ConsoleData console;
            SC_EXPECT(LoadScenario(rs.json_text, kData, def, console, &e));
            ScenarioRunner runner(std::move(def), std::move(console));
            MissionBridge bridge(rs.manifest, c.Data(), seed);
            while (!runner.Finished() && !bridge.UnitDestroyed() && runner.Time() < rs.manifest.end_s + 60.0) {
                runner.Step(0.1);
                bridge.Step(runner);
            }
            const PackageOutcome o = bridge.Finish(runner);
            kills += bridge.NeighbourKills();
            for (const LossRecord& l : o.losses) credited += l.by != rs.manifest.own_object ? 1 : 0;
            for (const RaidNeighbour& nb : rs.manifest.neighbours) {
                fired += std::find(o.fired.begin(), o.fired.end(), nb.id) != o.fired.end() ? 1 : 0;
            }
            c.CompletePlayerMission(o, rs.manifest.start_h + runner.Time() / 3600.0);
            ++played;
        }
    }
    std::printf("    %d raids played: %d neighbouring defences in reach, %d of them fired, %d aircraft they shot down\n", played, neighbours,
                fired, kills);
    SC_EXPECT(played >= 2 && neighbours >= played && fired > 0);
    SC_EXPECT(credited == kills);  // the neighbours' kills are theirs, not the battery's
}
