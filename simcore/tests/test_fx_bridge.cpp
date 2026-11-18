// The presentation feed's other shooters (simcore/fx/events.hpp, docs/fx/README.md 3.1): a played raid's neighbours as
// the mission bridge fires them (SamLaunch and SamEnd with the aircraft's World id, every round launched ending; a gun
// neighbour's GunPass, its rounds by the munitions library's rate), the bridge's anti-radiation missiles (ArmLaunch,
// ArmBurst), a cruise missile's warhead (CruiseImpact, no wreck), the loads still falling when a session ends landing in
// Finish, or in a session that runs on at their own times (LandFalling); decoy drones drawing the neighbours' fire; the
// EM layer's ARMs bursting; the runner's fighting neighbour and the battery's launchers (their missiles and the wrecks of what they hit);
// the battery's other launchers seen in the linked Osa's feed; a missile thrown off unguided (АВАРИЙНЫЙ ПУСК); and a raid
// the same with the feed switched off. Positions, routes and the neighbours' figures are placeholders.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/weapon_flight.hpp"
#include "simcore/battery_link.hpp"
#include "simcore/fx/events.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/scenery/munitions.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::vector<fx::Event> OfKind(const std::vector<fx::Event>& all, fx::EventKind k) {
    std::vector<fx::Event> out;
    for (const fx::Event& e : all) {
        if (e.kind == k) out.push_back(e);
    }
    return out;
}

bool OnGround(const ScenarioRunner& r, const Vec3& p, double up_m = 0.0) {
    return std::fabs(r.GetWorld().Frame().AltitudeOf(p) - r.GroundHeightAt(p.x, p.y) - up_m) < 0.01;
}

// The SAMs in a feed: each SamLaunch (a shooter at a target) is followed by its SamEnd before that shooter fires at that
// target again, both with the target's World id in `r`; a miss flies on to destroy itself; a hit leaves a wreck where
// the aircraft was, after the burst (within `reach_m` of the burst). A round may still be on its way at `end_s` only if
// it left in the 30 s before (the longest flight shown is the Osa's 26 s): none is left open. Returns the launches.
int ExpectSams(const std::vector<fx::Event>& feed, const ScenarioRunner& r, double end_s, int& hits, int& wrecks,
               double reach_m = 5.0) {
    std::map<std::string, const fx::Event*> open;  // "<source>|<target>" -> its launch
    int launches = 0;
    for (const fx::Event& e : feed) {
        if (e.kind != fx::EventKind::SamLaunch && e.kind != fx::EventKind::SamEnd) continue;
        SC_EXPECT(e.world_id >= 0 && e.world_id == r.TargetWorldId(e.target));
        SC_EXPECT(!e.source.empty() && !e.target.empty() && !e.weapon.empty());
        const std::string key = e.source + "|" + e.target;
        if (e.kind == fx::EventKind::SamLaunch) {
            SC_EXPECT(open.count(key) == 0);
            open[key] = &e;
            ++launches;
            SC_EXPECT(e.end_t_s >= e.t_s && e.velocity.Length() > 1.0);
            continue;
        }
        const auto it = open.find(key);
        SC_EXPECT(it != open.end());
        if (it == open.end()) continue;
        SC_EXPECT(it->second->seq < e.seq && it->second->number == e.number && it->second->weapon == e.weapon);
        open.erase(it);
        if (e.end == fx::BurstEnd::SelfDestruct) {
            SC_EXPECT(e.end_t_s > e.t_s);
            continue;
        }
        SC_EXPECT(e.end == fx::BurstEnd::Hit && e.end_t_s == e.t_s);
        ++hits;
        const fx::Event* wreck = nullptr;
        for (const fx::Event& d : feed) {
            if (d.kind == fx::EventKind::AircraftDown && d.target == e.target && d.seq > e.seq && wreck == nullptr) wreck = &d;
        }
        if (wreck == nullptr) continue;  // destroyed already by another (its wreck is the other's)
        ++wrecks;
        SC_EXPECT(wreck->t_s == e.t_s && wreck->world_id == e.world_id);
        SC_EXPECT((wreck->position - e.aim).Length() <= reach_m + 1e-6);  // the burst within the warhead's reach of it
    }
    for (const auto& o : open) {
        if (o.second->t_s <= end_s - 30.0) std::printf("    %s never ended (launched at %.1f s)\n", o.first.c_str(), o.second->t_s);
        SC_EXPECT(o.second->t_s > end_s - 30.0);
    }
    return launches;
}

// ------------------------------------------------------------------------------------------------ a raid with neighbours

// A strike on the P-40 post 24 km north of the battery: two Tornados laying retarded bombs down from 60 m, then two
// F-16s bombing level from 4.5 km, and a Weasel pair through the battery with the bridge's anti-radiation missiles (not
// the EM layer's). Two neighbours added to the manifest (so fought by the bridge): an S-125 on the post, whose zone both
// strike flights cross, and a ZSU-23-4 under the strike's track 4 km short of the post.
struct Raid {
    AirWarData d;
    AirCampaign c;
    RaidScenario rs;
    RaidManifest m;
    bool ok = false;
};

void BuildRaid(Raid& raid) {
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", raid.d, &e));
    SC_EXPECT(raid.c.Init(raid.d, 3u));
    const AirWarData& d = raid.c.Data();
    const Vec3 post = raid.c.Truth()[static_cast<std::size_t>(d.theatre.IndexOf("kesmark_p40"))].pos;
    const Vec3 battery = raid.c.Truth()[static_cast<std::size_t>(raid.c.PlayerIndex())].pos;
    const Vec3 from = post + Vec3{38000.0, 46000.0, 0.0};
    const Vec3 back = (from - post).Normalized();
    const double leg_h = (post - from).HorizontalLength() / 240.0 / 3600.0;
    Package p;
    p.id = 92;
    p.kind = MissionKind::Strike;
    p.part = DayPart::Day;
    p.target_object = "kesmark_p40";
    p.target_label = "the P-40 post";
    p.entry = "W3";
    p.exit = "W3";
    p.tot_h = 2.0;
    const auto strike = [&](const char* callsign, const char* aircraft, Profile profile, double agl, const char* weapon, int count,
                            Delivery delivery, double shift_h) {
        Flight fl;
        fl.callsign = callsign;
        fl.role = FlightRole::Strike;
        fl.aircraft = aircraft;
        fl.count = 2;
        fl.profile = profile;
        fl.weapon = weapon;
        fl.weapons = count;
        fl.delivery = delivery;
        const double t0 = 2.0 + shift_h;
        fl.route = {{from, agl, t0 - leg_h, "entry"}, {post, agl, t0, "target"}, {from, agl, t0 + leg_h, "exit"}};
        fl.release_index = 1;
        fl.aim_pos = post;
        return fl;
    };
    Flight sead;
    sead.callsign = "Weasel 30";
    sead.role = FlightRole::Sead;
    sead.aircraft = "f4g";
    sead.count = 2;
    sead.profile = Profile::Medium;
    const double sead_t = 2.06;
    const double sead_leg_h = 45000.0 / 230.0 / 3600.0;
    sead.route = {{battery + Vec3{-45000.0, -5000.0, 0.0}, 4000.0, sead_t - sead_leg_h, "entry"}, {battery, 4000.0, sead_t, "target"},
                  {battery + Vec3{45000.0, -5000.0, 0.0}, 4000.0, sead_t + sead_leg_h, "exit"}};
    p.flights = {strike("Tonka 20", "tornado_ids", Profile::Low, 60.0, "mk82r", 8, Delivery::Laydown, 0.0),
                 strike("Viper 10", "f16a", Profile::Medium, 4500.0, "mk82", 6, Delivery::Level, 0.02), sead};
    ScenarioFormat fmt;
    fmt.target_arms = false;  // the Weasels' ARMs are the bridge's model
    fmt.reactions = false;    // its pilots fly the scripted evasion: the loads' timing is tested here, not the reactions (B4)
    RaidScenarioBuilder builder;
    raid.rs = builder.Build(raid.c, p, fmt);
    SC_EXPECT(raid.rs.ok);
    if (!raid.rs.ok) {
        std::printf("    %s\n", raid.rs.error.c_str());
        return;
    }
    raid.m = raid.rs.manifest;
    Vec3 aim;
    int arm_carriers = 0;
    for (const RaidAircraft& a : raid.m.aircraft) {
        if (a.has_release && !a.missile) aim = a.aim_enu;
        arm_carriers += a.arms > 0 && !a.em_arms ? 1 : 0;
    }
    SC_EXPECT(arm_carriers == 2);
    RaidNeighbour s125;
    s125.id = "test_s125";
    s125.name = "S-125 at the post";
    s125.enu = Vec3{aim.x, aim.y, 0.0};
    s125.defence.present = true;
    s125.defence.system = "s125";
    s125.defence.radius_m = 15000.0;
    s125.defence.lethality = 0.4;
    s125.defence.missiles = 8;
    s125.missiles = 8;
    RaidNeighbour zsu;
    zsu.id = "test_zsu";
    zsu.name = "ZSU-23-4 by the post";
    zsu.enu = Vec3{aim.x + back.x * 4000.0, aim.y + back.y * 4000.0, 0.0};
    zsu.defence.present = true;
    zsu.defence.system = "zsu234";
    zsu.defence.radius_m = 2500.0;
    zsu.defence.lethality = 0.3;
    zsu.defence.medium_cover = false;
    zsu.defence.missiles = -1;
    zsu.missiles = -1;
    raid.m.neighbours.push_back(s125);
    raid.m.neighbours.push_back(zsu);
    raid.ok = true;
}

struct RaidRun {
    std::unique_ptr<ScenarioRunner> runner;
    std::unique_ptr<MissionBridge> bridge;
    std::vector<fx::Event> feed;  // all of it, Finish's included
    std::uint64_t before_finish = 0;
    double stopped_s = 0.0;
    PackageOutcome o;
};

// Plays the raid (fx on or off, the munitions library or none) to its end, or `after_s` past the first level release.
void PlayRaid(const Raid& raid, bool fx_on, const scenery::MunitionLibrary* lib, double after_s, RaidRun& run) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(raid.rs.json_text, kData, def, console, &e));
    run.runner = std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
    run.bridge = std::make_unique<MissionBridge>(raid.m, raid.c.Data(), 3u);
    ScenarioRunner& r = *run.runner;
    MissionBridge& b = *run.bridge;
    r.SetFxOn(fx_on);
    b.SetMunitions(lib);
    double stop = 1e18;
    while (!r.Finished() && !b.UnitDestroyed() && r.Time() < raid.m.end_s + 60.0 && r.Time() < stop) {
        r.Step(0.1);
        b.Step(r);
        if (after_s < 0.0 || stop < 1e17) continue;
        for (const fx::Event& ev : r.Fx().Since(0)) {
            if (ev.kind == fx::EventKind::WeaponRelease && ev.delivery == "level") stop = r.Time() + after_s;
        }
    }
    run.stopped_s = r.Time();
    run.before_finish = r.Fx().Last();
    run.o = b.Finish(r);
    run.feed = r.Fx().Since(0);
}

std::string OutcomeText(const PackageOutcome& o) {
    std::string s;
    char buf[160];
    for (const LossRecord& l : o.losses) {
        std::snprintf(buf, sizeof(buf), "loss %d %s %.6f %.6f %d %d\n", l.flight, l.by.c_str(), l.where.x, l.where.y, l.officer ? 1 : 0,
                      l.captured ? 1 : 0);
        s += buf;
    }
    for (const auto& x : o.damage) {
        std::snprintf(buf, sizeof(buf), "damage %s %.9f\n", x.first.c_str(), x.second);
        s += buf;
    }
    for (const auto& x : o.emissions) {
        std::snprintf(buf, sizeof(buf), "emission %s %.6f\n", x.first.c_str(), x.second);
        s += buf;
    }
    for (const auto& x : o.missiles_by) s += "missiles_by " + x.first + " " + std::to_string(x.second) + "\n";
    const auto list = [&s](const char* what, const std::vector<std::string>& v) {
        for (const std::string& x : v) s += std::string(what) + " " + x + "\n";
    };
    list("destroyed", o.destroyed);
    list("decoy", o.decoys_hit);
    list("radar_killed", o.radars_killed);
    list("radar_suppressed", o.radars_suppressed);
    list("fired", o.fired);
    list("recce", o.recce_seen);
    list("missile_downed", o.missiles_downed);
    list("note", o.notes);
    for (int f : o.released_flights) s += "released " + std::to_string(f) + "\n";
    for (int f : o.aborted_flights) s += "aborted " + std::to_string(f) + "\n";
    s += "hit_nothing " + std::to_string(o.hit_nothing ? 1 : 0) + " delivered " + std::to_string(o.delivered ? 1 : 0) + " spent " +
         std::to_string(o.missiles_spent) + " arms " + std::to_string(o.arms_fired) + "\n";
    return s;
}

// Four decoy drones (BQM-74 class) low down the strike's track onto the P-40 post, past a ZU-23 site 20 km short of
// it (its zone clear of the S-125's) and into the S-125's zone at the post: both neighbours fought by the bridge.
void BuildDecoyRaid(Raid& raid) {
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", raid.d, &e));
    SC_EXPECT(raid.c.Init(raid.d, 3u));
    const AirWarData& d = raid.c.Data();
    const Vec3 post = raid.c.Truth()[static_cast<std::size_t>(d.theatre.IndexOf("kesmark_p40"))].pos;
    const Vec3 from = post + Vec3{38000.0, 46000.0, 0.0};
    const double leg_h = (post - from).HorizontalLength() / 200.0 / 3600.0;
    Package p;
    p.id = 94;
    p.kind = MissionKind::Strike;
    p.part = DayPart::Day;
    p.target_object = "kesmark_p40";
    p.target_label = "the P-40 post";
    p.entry = "W3";
    p.exit = "W3";
    p.tot_h = 2.0;
    Flight fl;
    fl.callsign = "Decoy 50";
    fl.role = FlightRole::Decoy;
    fl.aircraft = "bqm74";
    fl.count = 4;
    fl.profile = Profile::Low;
    fl.route = {{from, 150.0, 2.0 - leg_h, "entry"}, {post, 150.0, 2.0, "target"}, {from, 150.0, 2.0 + leg_h, "exit"}};
    p.flights = {fl};
    RaidScenarioBuilder builder;
    raid.rs = builder.Build(raid.c, p);
    SC_EXPECT(raid.rs.ok);
    if (!raid.rs.ok) {
        std::printf("    %s\n", raid.rs.error.c_str());
        return;
    }
    raid.m = raid.rs.manifest;
    int drones = 0;
    for (const RaidAircraft& a : raid.m.aircraft) drones += a.drone ? 1 : 0;
    SC_EXPECT(drones == 4);
    const LocalFrame theatre(d.theatre.origin);
    const LocalFrame scenario(raid.m.origin);
    const Vec3 at_post = scenario.GeodeticToEnu(theatre.EnuToGeodetic(post));
    const Vec3 out = scenario.GeodeticToEnu(theatre.EnuToGeodetic(from));
    const Vec3 back = Vec3{out.x - at_post.x, out.y - at_post.y, 0.0}.Normalized();
    RaidNeighbour s125;
    s125.id = "test_s125";
    s125.name = "S-125 at the post";
    s125.enu = Vec3{at_post.x, at_post.y, 0.0};
    s125.defence.present = true;
    s125.defence.system = "s125";
    s125.defence.radius_m = 15000.0;
    s125.defence.lethality = 0.4;
    s125.defence.missiles = 8;
    s125.missiles = 8;
    RaidNeighbour zu;
    zu.id = "test_zu23";
    zu.name = "ZU-23 on the way in";
    zu.enu = Vec3{at_post.x + back.x * 20000.0, at_post.y + back.y * 20000.0, 0.0};
    zu.defence.present = true;
    zu.defence.system = "zu23";
    zu.defence.radius_m = 2500.0;
    zu.defence.lethality = 0.3;
    zu.defence.medium_cover = false;
    zu.defence.missiles = -1;
    zu.missiles = -1;
    raid.m.neighbours.push_back(s125);
    raid.m.neighbours.push_back(zu);
    raid.ok = true;
}

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string ReplaceOnce(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    SC_EXPECT(at != std::string::npos);
    return at == std::string::npos ? s : s.replace(at, from.size(), to);
}

}  // namespace

SC_TEST("fx (bridge): a played raid's neighbours, guns and ARMs in the feed; the session the same with the feed off") {
    Raid raid;
    BuildRaid(raid);
    if (!raid.ok) return;
    RaidRun on, off;
    PlayRaid(raid, true, nullptr, -1.0, on);
    PlayRaid(raid, false, nullptr, -1.0, off);
    const ScenarioRunner& r = *on.runner;
    // The feed changes nothing: the same debrief, the same log and the same outcome for the campaign with it off.
    SC_EXPECT(off.feed.empty() && !on.feed.empty());
    SC_EXPECT(on.runner->Debrief() == off.runner->Debrief());
    SC_EXPECT(on.runner->Log().Entries().size() == off.runner->Log().Entries().size());
    SC_EXPECT(OutcomeText(on.o) == OutcomeText(off.o));
    SC_EXPECT(on.runner->Time() == off.runner->Time() && on.bridge->UnitDestroyed() == off.bridge->UnitDestroyed());
    // The missile neighbours' rounds (ours and the theatre's around the post): launched from the site's rails toward the
    // aircraft and ending on it or flying on, each with the aircraft's World id, each launched round ended (an aircraft
    // gone with a round on its way: the round flies on).
    const auto neighbour = [&raid](const std::string& id) -> const RaidNeighbour* {
        for (const RaidNeighbour& nb : raid.m.neighbours) {
            if (nb.id == id) return &nb;
        }
        return nullptr;
    };
    int hits = 0, wrecks = 0, ours = 0;
    const int sams = ExpectSams(on.feed, r, on.bridge->UnitDestroyed() ? 1e18 : r.Time(), hits, wrecks);
    for (const fx::Event& e : OfKind(on.feed, fx::EventKind::SamLaunch)) {
        const RaidNeighbour* nb = neighbour(e.source);
        SC_EXPECT(nb != nullptr && nb->defence.missiles >= 0);
        if (nb == nullptr) continue;
        SC_EXPECT(e.weapon == nb->defence.system && e.number == 1 && OnGround(r, e.position, 3.5));
        // At the site: its ground point (GroundEnu: off the site's ENU east and north by the up's tilt there) and up.
        SC_EXPECT((e.position - (r.GroundEnu(nb->enu.x, nb->enu.y) + Vec3{0.0, 0.0, 3.5})).Length() < 1e-6);
        SC_EXPECT(std::hypot(e.position.x - nb->enu.x, e.position.y - nb->enu.y) < 5.0);
        ours += e.source == "test_s125" ? 1 : 0;
    }
    SC_EXPECT(ours >= 1);
    // A neighbour's kill leaves the wreck the runner shows (DestroyTarget), named for the neighbour. The raid's own
    // S-125 hits nothing, so the same raid again with it at full lethality and the package's suppression and jamming
    // off: at least one kill, each a wreck at its burst, named for the S-125.
    Raid lethal_raid = raid;
    lethal_raid.m.neighbours[0].defence.lethality = 1.0;
    lethal_raid.m.sead_supp = 0.0;
    lethal_raid.m.jam = 0.0;
    RaidRun lethal;
    PlayRaid(lethal_raid, true, nullptr, -1.0, lethal);
    int lethal_hits = 0, lethal_wrecks = 0, named_wrecks = 0;
    ExpectSams(lethal.feed, *lethal.runner, lethal.bridge->UnitDestroyed() ? 1e18 : lethal.runner->Time(), lethal_hits, lethal_wrecks);
    for (const fx::Event& e : OfKind(lethal.feed, fx::EventKind::SamEnd)) {
        if (e.end != fx::BurstEnd::Hit) continue;
        const RaidNeighbour* nb = neighbour(e.source);
        bool named = false;
        for (const fx::Event& d : OfKind(lethal.feed, fx::EventKind::AircraftDown)) {
            named = named || (nb != nullptr && d.target == e.target && d.source == nb->name);
        }
        SC_EXPECT(named);
        named_wrecks += named ? 1 : 0;
    }
    SC_EXPECT(lethal_hits >= 1 && lethal_wrecks >= 1 && named_wrecks >= 1 && lethal.bridge->NeighbourKills() >= 1);
    // The guns' pass at each low aircraft: from the site on the ground at the aircraft, its rounds through the pass at
    // the bridge's own rate without a library, munitions.json's rate_rpm for the system's round (wiki_guns): the ZSU-23-4
    // 3400 a minute (four barrels), the ZU-23-2 2000, the S-60 120, the KS-30 12; other AAA as the ZU-23.
    const auto rpm = [](const std::string& system) {
        return system == "zsu234" ? 3400.0 : system == "s60" ? 120.0 : system == "ks30" ? 12.0 : 2000.0;
    };
    const std::vector<fx::Event> guns = OfKind(on.feed, fx::EventKind::GunPass);
    int zsu = 0;
    for (const fx::Event& g : guns) {
        const RaidNeighbour* nb = neighbour(g.source);
        SC_EXPECT(nb != nullptr && nb->defence.missiles < 0);
        if (nb == nullptr) continue;
        SC_EXPECT(g.weapon == nb->defence.system && OnGround(r, g.position));
        SC_EXPECT(g.world_id >= 0 && g.world_id == r.TargetWorldId(g.target) && g.end_t_s > g.t_s);
        SC_EXPECT(std::abs(g.number - static_cast<int>(std::lround(rpm(g.weapon) / 60.0 * (g.end_t_s - g.t_s)))) <= 1);
        zsu += g.source == "test_zsu" ? 1 : 0;
    }
    SC_EXPECT(zsu >= 1);
    // The bridge's ARMs: each launched at our radar's antenna, 4 m over the ground where the vehicle stands, from its
    // carrier, arriving at end_t_s; each burst at its arrival (the first step past it), on the antenna for a hit, else
    // 30-80 m wide of the vehicle on the ground.
    const std::vector<fx::Event> arms = OfKind(on.feed, fx::EventKind::ArmLaunch);
    const std::vector<fx::Event> bursts = OfKind(on.feed, fx::EventKind::ArmBurst);
    const Vec3 unit = r.UnitPosition();
    const Vec3 antenna = r.GroundEnu(unit.x, unit.y) + Vec3{0.0, 0.0, 4.0};
    SC_EXPECT(static_cast<int>(arms.size()) == on.o.arms_fired && !arms.empty());
    SC_EXPECT(bursts.size() <= arms.size());
    for (const fx::Event& a : arms) {
        bool carrier = false;
        for (const RaidAircraft& c : raid.m.aircraft) carrier = carrier || (c.target_id == a.source && c.arms > 0);
        SC_EXPECT(carrier);
        SC_EXPECT((a.aim - antenna).Length() < 1e-6 && a.end_t_s > a.t_s && a.world_id == r.TargetWorldId(a.source));
        SC_EXPECT(a.weapon == "agm78b_class_standard_arm");  // the F-4G's in a war of 1985 (afmag_1989_slam, agm78_refs)
    }
    std::size_t matched = 0;
    for (const fx::Event& b : bursts) {
        bool at_arrival = false;
        for (const fx::Event& a : arms) at_arrival = at_arrival || (a.source == b.source && b.t_s >= a.end_t_s - 1e-9 && b.t_s < a.end_t_s + 0.1 + 1e-9);
        matched += at_arrival ? 1 : 0;
        if (b.end == fx::BurstEnd::Hit) {
            SC_EXPECT((b.position - antenna).Length() < 1e-6);
        } else {
            const double wide = std::hypot(b.position.x - unit.x, b.position.y - unit.y);
            SC_EXPECT(b.end == fx::BurstEnd::Ground && wide >= 29.5 && wide <= 80.5 && OnGround(r, b.position));  // (on the ground: within a few cm over the ground)
        }
    }
    SC_EXPECT(matched == bursts.size());
    std::printf("    missile neighbours: %d rounds (%d the S-125's), %d hits (%d wrecks; %d and %d with the S-125 lethal); %d gun passes "
                "(%d the ZSU's); %d ARMs, %d burst; %d events; our vehicle %s\n",
                sams, ours, hits, wrecks, lethal_hits, lethal_wrecks, static_cast<int>(guns.size()), zsu, static_cast<int>(arms.size()), static_cast<int>(bursts.size()),
                static_cast<int>(on.feed.size()), on.bridge->UnitDestroyed() ? "out of action" : "fighting");
}

SC_TEST("fx (bridge): a gun's rounds follow the munitions library's rate; the loads still falling at the end land in Finish") {
    Raid raid;
    BuildRaid(raid);
    if (!raid.ok) return;
    // A library whose ZSU-23-4 fires 1200 rounds a minute (a placeholder, not the gun's): the GunPass counts by it.
    scenery::MunitionLibrary lib;
    scenery::MunitionSpec shell;
    shell.id = "test_round";
    shell.kind = "gun";
    shell.rate_rpm = 1200.0;
    lib.munitions.push_back(shell);
    lib.systems["zsu234"] = "test_round";
    RaidRun bare, with;
    PlayRaid(raid, true, nullptr, 2.0, bare);
    PlayRaid(raid, true, &lib, 2.0, with);
    const std::vector<fx::Event> g0 = OfKind(bare.feed, fx::EventKind::GunPass);
    const std::vector<fx::Event> g1 = OfKind(with.feed, fx::EventKind::GunPass);
    SC_EXPECT(!g1.empty() && g0.size() == g1.size() && bare.feed.size() == with.feed.size());
    for (std::size_t i = 0; i < g0.size() && i < g1.size(); ++i) {
        SC_EXPECT(g0[i].seq == g1[i].seq && g0[i].t_s == g1[i].t_s && g0[i].end_t_s == g1[i].end_t_s && g0[i].target == g1[i].target);
        const double pass_s = g1[i].end_t_s - g1[i].t_s;
        SC_EXPECT(std::abs(g1[i].number - static_cast<int>(std::lround(1200.0 / 60.0 * pass_s))) <= 1);
        SC_EXPECT(std::abs(g0[i].number - static_cast<int>(std::lround(3400.0 / 60.0 * pass_s))) <= 1);
    }
    // Stopped 2 s after the level release, its bombs half a minute from the ground: Finish lands them at their own
    // time (the release's end_t_s), after everything before it in the feed, and the log has their blast at that time.
    const ScenarioRunner& r = *with.runner;
    int pending = 0;
    for (const fx::Event& rel : OfKind(with.feed, fx::EventKind::WeaponRelease)) {
        int impacts = 0;
        for (const fx::Event& i : OfKind(with.feed, fx::EventKind::WeaponImpact)) {
            if (i.source != rel.source) continue;
            ++impacts;
            SC_EXPECT((i.position - rel.aim).Length() < 1e-6);
            if (rel.end_t_s <= with.stopped_s) continue;
            ++pending;
            SC_EXPECT(i.seq > with.before_finish && i.t_s == rel.end_t_s && i.t_s > r.Time());
            bool logged = false;
            for (const DebriefEntry& l : r.Log().Entries()) {
                logged = logged || (l.kind == DebriefEntry::Kind::Scenario && l.what == rel.source && l.time_s == rel.end_t_s &&
                                    l.detail.compare(0, 5, "bombs") == 0);
            }
            SC_EXPECT(logged);
        }
        SC_EXPECT(impacts == 1);
    }
    SC_EXPECT(pending >= 1 && with.o.delivered);
    std::printf("    %d gun passes, %d and %d rounds in the first; stopped at %.1f s, %d loads landed in Finish\n",
                static_cast<int>(g1.size()), g0.empty() ? 0 : g0[0].number, g1.empty() ? 0 : g1[0].number, with.stopped_s, pending);
}

// A package handed back while its loads still fall (LivePackages, once it is over by its clock) lands them in the
// session at their own times: FallingUntil says until when, LandFalling lands each at the first step past its time and
// moves nothing else of the raid; Finish then has nothing left to land. Nothing is dated ahead of the session.
SC_TEST("fx (bridge): loads still falling land in the running session at their own times (FallingUntil, LandFalling)") {
    Raid raid;
    BuildRaid(raid);
    if (!raid.ok) return;
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(raid.rs.json_text, kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    MissionBridge b(raid.m, raid.c.Data(), 3u);
    SC_EXPECT(b.FallingUntil() < 0.0);
    // To 2 s after the level release, its bombs half a minute from the ground.
    double stop = 1e18;
    while (!r.Finished() && !b.UnitDestroyed() && r.Time() < raid.m.end_s && r.Time() < stop) {
        r.Step(0.1);
        b.Step(r);
        if (stop > 1e17) {
            for (const fx::Event& ev : r.Fx().Since(0)) {
                if (ev.kind == fx::EventKind::WeaponRelease && ev.delivery == "level") stop = r.Time() + 2.0;
            }
        }
    }
    SC_EXPECT(!b.UnitDestroyed() && stop < 1e17);
    double last_impact = -1.0;
    std::map<std::string, double> due;  // each falling load's impact time, by its aircraft
    for (const fx::Event& ev : OfKind(r.Fx().Since(0), fx::EventKind::WeaponRelease)) {
        if (ev.end_t_s <= r.Time()) continue;
        due[ev.source] = ev.end_t_s;
        last_impact = std::fmax(last_impact, ev.end_t_s);
    }
    SC_EXPECT(!due.empty());
    SC_EXPECT_NEAR(b.FallingUntil(), last_impact, 1e-9);
    // On with the session, the raid's own steps stopped: each lands at the first step past its time, and nothing else
    // of the raid happens (no release, no ARM, no neighbour's round).
    std::uint64_t cursor = r.Fx().Last();
    std::size_t log_seen = r.Log().Entries().size();
    int landed = 0, other = 0, ahead = 0;
    while (b.FallingUntil() >= 0.0 && r.Time() < last_impact + 5.0) {
        r.Step(0.1);
        b.LandFalling(r);
        for (const fx::Event& ev : r.Fx().Since(cursor)) {
            if (ev.kind == fx::EventKind::WeaponImpact) {
                ++landed;
                const auto it = due.find(ev.source);
                SC_EXPECT(it != due.end());
                if (it != due.end()) SC_EXPECT(ev.t_s >= it->second - 1e-9 && ev.t_s < it->second + 0.1 + 1e-9);
                ahead += ev.t_s > r.Time() + 1e-9 ? 1 : 0;
            } else if (ev.kind == fx::EventKind::WeaponRelease || ev.kind == fx::EventKind::ArmLaunch ||
                       ev.kind == fx::EventKind::SamLaunch || ev.kind == fx::EventKind::GunPass) {
                ++other;
            }
        }
        cursor = r.Fx().Last();
        const std::vector<DebriefEntry>& log = r.Log().Entries();
        for (; log_seen < log.size(); ++log_seen) ahead += log[log_seen].time_s > r.Time() + 1e-9 ? 1 : 0;
    }
    SC_EXPECT(b.FallingUntil() < 0.0 && landed == static_cast<int>(due.size()) && other == 0 && ahead == 0);
    // Finish has none left to land.
    const PackageOutcome o = b.Finish(r);
    SC_EXPECT(OfKind(r.Fx().Since(cursor), fx::EventKind::WeaponImpact).empty() && o.delivered);
    std::printf("    %d loads landed in the session, the last at %.1f s\n", landed, last_impact);
}

// A decoy draws fire (MissionBridge::Engage, FxDecoyFire): a gun site's pass at each drone; a missile site's one or two
// rounds launched and bursting at once, the first on it if it goes down (its wreck named for the site), the others
// destroying themselves; every round it spent shown, and no drone a loss.
SC_TEST("fx (bridge): decoy drones draw the neighbours' fire: a gun pass at each, a missile site's rounds launched and ended at once") {
    Raid raid;
    BuildDecoyRaid(raid);
    if (!raid.ok) return;
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(raid.rs.json_text, kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    MissionBridge b(raid.m, raid.c.Data(), 3u);
    while (!r.Finished() && !b.UnitDestroyed() && r.Time() < raid.m.end_s + 60.0 && !b.Over(r)) {
        r.Step(0.1);
        b.Step(r);
    }
    const PackageOutcome o = b.Finish(r);
    const std::vector<fx::Event> feed = r.Fx().Since(0);
    int hits = 0, wrecks = 0;
    ExpectSams(feed, r, r.Time(), hits, wrecks);
    const std::vector<fx::Event> guns = OfKind(feed, fx::EventKind::GunPass);
    int gun_downs = 0, sam_downs = 0, salvoes = 0, rounds = 0;
    for (const RaidAircraft& a : raid.m.aircraft) {
        if (!a.drone) continue;
        // The ZU-23's pass at it, its rounds at the ZU-23-2's 2000 a minute (the bridge's table).
        int passes = 0;
        for (const fx::Event& g : guns) {
            if (g.target != a.target_id) continue;
            ++passes;
            SC_EXPECT(g.source == "test_zu23" && g.weapon == "zu23" && g.end_t_s > g.t_s);
            SC_EXPECT(std::abs(g.number - static_cast<int>(std::lround(2000.0 / 60.0 * (g.end_t_s - g.t_s)))) <= 1);
        }
        SC_EXPECT(passes == 1);
        std::string down_by;
        for (const fx::Event& dn : OfKind(feed, fx::EventKind::AircraftDown)) {
            if (dn.target == a.target_id) down_by = dn.source;
        }
        gun_downs += down_by == "ZU-23 on the way in" ? 1 : 0;
        // The S-125's rounds at it: none at a drone the guns brought down, else one or two, all at one moment, each
        // launched with no flight and ended at once, the first a hit exactly when the S-125 brought it down.
        std::vector<const fx::Event*> launches, ends;
        for (const fx::Event& s : feed) {
            if (s.source != "test_s125" || s.target != a.target_id) continue;
            if (s.kind == fx::EventKind::SamLaunch) launches.push_back(&s);
            if (s.kind == fx::EventKind::SamEnd) ends.push_back(&s);
        }
        SC_EXPECT(launches.size() == ends.size());
        if (down_by == "ZU-23 on the way in") {
            SC_EXPECT(launches.empty());
            continue;
        }
        SC_EXPECT(launches.size() == 1 || launches.size() == 2);
        if (launches.empty()) continue;
        ++salvoes;
        rounds += static_cast<int>(launches.size());
        const bool shot = down_by == "S-125 at the post";
        sam_downs += shot ? 1 : 0;
        for (std::size_t k = 0; k < launches.size() && k < ends.size(); ++k) {
            SC_EXPECT(launches[k]->t_s == launches[0]->t_s && launches[k]->end_t_s == launches[k]->t_s);
            SC_EXPECT(ends[k]->t_s == launches[0]->t_s && ends[k]->seq > launches[k]->seq);
            SC_EXPECT(ends[k]->end == ((shot && k == 0) ? fx::BurstEnd::Hit : fx::BurstEnd::SelfDestruct));
        }
    }
    // Every round the S-125 spent is in the feed; no drone is a loss.
    int spent = 0;
    for (const auto& m : o.missiles_by) spent += m.first == "test_s125" ? m.second : 0;
    SC_EXPECT(spent == rounds && salvoes >= 1 && o.losses.empty());
    SC_EXPECT(hits == sam_downs && wrecks == sam_downs);
    std::printf("    4 drones: %d gun passes (%d down), %d S-125 salvoes of %d rounds (%d down)\n", static_cast<int>(guns.size()),
                gun_downs, salvoes, rounds, sam_downs);
}

// The EM layer's anti-radiation missiles (ScenarioRunner::UpdateArms, the ARM drill of data/scenarios/osa_arm_drill.json):
// each one's end as the EM layer decided it is an ArmBurst in the feed (the missile itself is drawn as an entity while it
// flies) at its burst point, at the step it ended, from its carrier with the library's ARM: a Hit on what it hit (a
// dummy's decoy, the vehicle), else Ground. With the feed off none, and the fight the same.
SC_TEST("fx: the EM layer's anti-radiation missiles burst in the feed where the EM layer says, a hit on what they hit") {
    const std::string harm = ReplaceOnce(ReadText(kData + "/scenarios/osa_arm_drill.json"), "\"when_targets_gone\": false",
                                         "\"when_targets_gone\": true");
    const std::string dummy = ReplaceOnce(harm, "\"readiness\": \"deployed\",",
                                          "\"readiness\": \"deployed\",\n    \"prepared_position\": { \"dummies\": [ { \"position\": { \"east_m\": 600, "
                                          "\"north_m\": 500, \"up_m\": 0 }, \"decoy\": \"when_radar_off\" } ] },");
    struct ArmRun {
        std::vector<EmSensorLink::ArmResult> results;
        std::vector<fx::Event> bursts;
        std::string debrief;
    };
    const auto play = [](const std::string& text, bool fx_on) {
        ArmRun out;
        ScenarioDef def;
        ConsoleData console;
        std::string err;
        SC_EXPECT(LoadScenario(text, kData, def, console, &err));
        ScenarioRunner r(std::move(def), std::move(console));
        r.SetFxOn(fx_on);
        r.RunToEnd(0.1, 1200.0);
        out.results = r.ArmResults();
        out.bursts = OfKind(r.Fx().Since(0), fx::EventKind::ArmBurst);
        out.debrief = r.Debrief();
        return out;
    };
    int results = 0, hits = 0, misses = 0;
    for (const std::string* text : {&harm, &dummy}) {
        const ArmRun on = play(*text, true);
        SC_EXPECT(!on.results.empty() && on.bursts.size() == on.results.size());
        for (const EmSensorLink::ArmResult& a : on.results) {
            int found = 0;
            for (const fx::Event& bu : on.bursts) {
                if (bu.source != a.carrier || bu.weapon != a.weapon || (bu.position - a.burst_point).Length() > 1e-9) continue;
                ++found;
                SC_EXPECT(bu.aim.x == bu.position.x && bu.aim.y == bu.position.y && bu.aim.z == bu.position.z);
                SC_EXPECT(bu.end == (a.hit ? fx::BurstEnd::Hit : fx::BurstEnd::Ground) && bu.end_t_s == bu.t_s);
                SC_EXPECT(bu.t_s >= a.burst_s - 1e-9 && bu.t_s <= a.burst_s + 0.1 + 1e-9);
            }
            SC_EXPECT(found == 1 && !a.carrier.empty() && a.weapon == "agm88a_class_harm");
            ++results;
            hits += a.hit ? 1 : 0;
            misses += a.hit ? 0 : 1;
        }
        // The feed off: no burst, and the fight the same.
        const ArmRun off = play(*text, false);
        SC_EXPECT(off.bursts.empty() && off.results.size() == on.results.size() && off.debrief == on.debrief);
    }
    SC_EXPECT(hits >= 1 && misses >= 1);
    std::printf("    %d EM ARMs ended in the two fights: %d hits, %d misses, each one ArmBurst\n", results, hits, misses);
}

SC_TEST("fx (bridge): a cruise missile that strikes shows its warhead (CruiseImpact) and no wreck") {
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    d.theatre.year = 1990;  // the Tomahawk
    AirCampaign c;
    SC_EXPECT(c.Init(d, 3u));
    // Two Tomahawks low from the west onto the P-40 post, 24 km north of the battery and out of its reach.
    const Vec3 post = c.Truth()[static_cast<std::size_t>(c.Data().theatre.IndexOf("kesmark_p40"))].pos;
    Package p;
    p.id = 78;
    p.kind = MissionKind::Missile;
    p.part = DayPart::Day;
    p.target_object = "kesmark_p40";
    p.target_label = "the P-40 post";
    p.entry = "W3";
    p.exit = "W3";
    p.tot_h = 2.0;
    Flight fl;
    fl.callsign = "Hammer 10";
    fl.role = FlightRole::Missile;
    fl.aircraft = "tlam_c";
    fl.count = 2;
    fl.profile = Profile::Low;
    fl.route = {{post + Vec3{-120000.0, 8000.0, 0.0}, 50.0, 2.0 - 120000.0 / 245.0 / 3600.0, "entry"}, {post, 10.0, 2.0, "target"}};
    fl.release_index = 1;
    fl.aim_pos = post;
    p.flights = {fl};
    RaidScenarioBuilder builder;
    const RaidScenario rs = builder.Build(c, p);
    SC_EXPECT(rs.ok);
    if (!rs.ok) {
        std::printf("    %s\n", rs.error.c_str());
        return;
    }
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(rs.json_text, kData, def, console, &e));
    ScenarioRunner runner(std::move(def), std::move(console));
    MissionBridge bridge(rs.manifest, c.Data(), 3u);
    while (!runner.Finished() && !bridge.UnitDestroyed() && runner.Time() < rs.manifest.end_s + 60.0) {
        runner.Step(0.1);
        bridge.Step(runner);
    }
    const PackageOutcome o = bridge.Finish(runner);
    const std::vector<fx::Event> feed = runner.Fx().Since(0);
    int struck = 0;
    for (const RaidAircraft& a : rs.manifest.aircraft) {
        int impacts = 0, wrecks = 0;
        for (const fx::Event& ev : feed) {
            if (ev.kind == fx::EventKind::AircraftDown && ev.target == a.target_id) ++wrecks;
            if (ev.kind != fx::EventKind::CruiseImpact || ev.source != a.target_id) continue;
            ++impacts;
            // Where it arrived, its warhead going off on the ground where the bridge put it, with its World id.
            SC_EXPECT(ev.world_id >= 0 && ev.world_id == runner.TargetWorldId(a.target_id) && ev.weapon == "tlam_c" && ev.fire);
            SC_EXPECT(OnGround(runner, ev.aim) && std::hypot(ev.aim.x - a.aim_enu.x, ev.aim.y - a.aim_enu.y) <= 321.0);
            SC_EXPECT(ev.velocity.Length() > 50.0 && ev.target == rs.manifest.own_object);
        }
        SC_EXPECT(impacts + wrecks == 1);  // it struck, or it was shot down: never both
        struck += impacts;
    }
    SC_EXPECT(struck >= 1 && o.losses.empty());
    std::printf("    %d of %d Tomahawks struck the post\n", struck, static_cast<int>(rs.manifest.aircraft.size()));
}

// ------------------------------------------------------------------------------------------------ the runner's shooters

SC_TEST("fx: a fighting neighbour's launchers in the feed: each missile launched and ended with its target's World id, its kill a wreck") {
    // The practice scenario (one drone from the north) with the regiment's battery 23 30 km north on its way in.
    std::ifstream file(kData + "/scenarios/osa_practice.json", std::ios::binary);
    std::stringstream ss;
    ss << file.rdbuf();
    std::string text = ss.str();
    const std::size_t at = text.find("\"targets\"");
    SC_EXPECT(at != std::string::npos);
    text.insert(at, R"("neighbours": [{ "id": "osa_23", "name": "Osa battery 23", "system": "osa",
        "position": { "east_m": 2000, "north_m": 30000, "up_m": 0 }, "radius_km": 10, "fights": true, "launchers": 3 }],
  )");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(text, kData, def, console, &error));
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.NeighbourFights("osa_23"));
    while (!r.Finished() && r.Time() < 700.0) r.Step(0.1);
    const std::vector<fx::Event> feed = r.Fx().Since(0);
    int hits = 0, wrecks = 0;
    const int launches = ExpectSams(feed, r, r.Time(), hits, wrecks);
    SC_EXPECT(launches == r.NeighbourLaunches("osa_23") && launches >= 1);
    for (const fx::Event& e : OfKind(feed, fx::EventKind::SamLaunch)) {
        SC_EXPECT(e.source.compare(0, 7, "osa_23/") == 0 && e.weapon == "osa" && OnGround(r, e.position, 3.5));
    }
    // Its kill: the wreck, named for the battery that shot it down.
    const bool killed = r.NeighbourKills().count("t1") != 0;
    SC_EXPECT((hits >= 1) == killed && wrecks == hits);
    for (const fx::Event& d : OfKind(feed, fx::EventKind::AircraftDown)) {
        if (d.target == "t1" && killed) SC_EXPECT(d.source == "Osa battery 23" && d.world_id == r.TargetWorldId("t1"));
    }
    std::printf("    battery 23: %d missiles in the feed, %d hit\n", launches, hits);
}

SC_TEST("fx: the battery's other launchers in the feed: each missile launched and ended, its kill a wreck named for the launcher") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_battery_fight.json", kData, def, console));
    ScenarioRunner r(std::move(def), std::move(console));
    r.RunToEnd(0.1, 900.0);
    const std::vector<fx::Event> feed = r.Fx().Since(0);
    int hits = 0, wrecks = 0;
    const int launches = ExpectSams(feed, r, r.Time(), hits, wrecks);
    int shooter_launches = 0, shooter_hits = 0;
    for (const BatteryFire::Shooter& s : r.GetBatteryFire()->Shooters()) {
        shooter_launches += s.launches;
        shooter_hits += s.hits;
    }
    SC_EXPECT(launches == shooter_launches && hits == shooter_hits && launches >= 2 && hits >= 1);
    for (const fx::Event& e : OfKind(feed, fx::EventKind::SamLaunch)) {
        SC_EXPECT(e.source != "own" && e.source != "21" && e.weapon == "osa" && OnGround(r, e.position, 3.5));
    }
    for (const fx::Event& e : OfKind(feed, fx::EventKind::SamEnd)) {
        if (e.end != fx::BurstEnd::Hit) continue;
        bool named = false;
        for (const fx::Event& d : OfKind(feed, fx::EventKind::AircraftDown)) named = named || (d.target == e.target && d.source == e.source);
        SC_EXPECT(named);
    }
    std::printf("    the other launchers: %d missiles, %d hits, %d wrecks in the feed\n", launches, hits, wrecks);
}

SC_TEST("fx: in a linked battery the Osa's feed shows the post's launchers' missiles with the Osa's World ids") {
    std::string error;
    std::unique_ptr<BatteryLink> link = LoadBatteryLinkFile(kData + "/scenarios/battery_link_practice.json", kData, &error);
    SC_EXPECT(link != nullptr);
    if (!link) {
        std::printf("    %s\n", error.c_str());
        return;
    }
    link->RunToEnd(0.1, 900.0);
    const ScenarioRunner& osa = link->Osa();
    const ScenarioRunner& post = link->Post();
    const std::vector<fx::Event> mine = osa.Fx().Since(0);
    const std::vector<fx::Event> theirs = post.Fx().Since(0);
    int post_sams = 0, shown = 0;
    for (const fx::Event& e : theirs) post_sams += e.kind == fx::EventKind::SamLaunch || e.kind == fx::EventKind::SamEnd ? 1 : 0;
    for (const fx::Event& e : mine) shown += e.kind == fx::EventKind::SamLaunch || e.kind == fx::EventKind::SamEnd ? 1 : 0;
    SC_EXPECT(post_sams >= 2 && shown == post_sams);
    int hits = 0, wrecks = 0;
    // With the Osa's World ids; a wreck within a metre more of the burst (the post's sky follows the Osa's within one).
    const int launches = ExpectSams(mine, osa, osa.Time(), hits, wrecks, 6.0);
    for (const fx::Event& e : OfKind(mine, fx::EventKind::SamLaunch)) SC_EXPECT(e.source != link->Callsign() && e.source != "own");
    // Each kill of theirs is a wreck in the Osa's feed (the post's report on the net), after its burst.
    SC_EXPECT(wrecks == hits);
    std::printf("    the post's launchers: %d missiles, %d hits, shown in the Osa's feed\n", launches, hits);
}

SC_TEST("fx: a missile thrown off unguided (АВАРИЙНЫЙ ПУСК) climbs along the hull's heading and destroys itself after 25.5 s") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &e));
    def.has_crew_data = false;  // nobody else at the console
    ScenarioRunner r(std::move(def), std::move(console));
    for (int i = 0; i < 10; ++i) r.Step(0.1);
    const auto throw_off = [&r](int select) {
        ControlPanel& panel = r.Console().panel;
        SC_EXPECT(panel.Set("msl.emergency_select", select) && panel.Set("msl.emergency_cover", 1) && panel.Set("msl.emergency", 1));
        r.Step(0.1);
        SC_EXPECT(panel.Set("msl.emergency", 0));
        r.Step(0.1);
    };
    throw_off(6);  // 3-I
    throw_off(5);  // 2-I
    const std::vector<fx::Event> feed = r.Fx().Since(0);
    const std::vector<fx::Event> launches = OfKind(feed, fx::EventKind::MissileLaunch);
    const std::vector<fx::Event> ends = OfKind(feed, fx::EventKind::MissileEnd);
    SC_EXPECT(launches.size() == 2 && ends.size() == 2 && r.Launches() == 0);  // no launch of the crew's: no channel used
    const double heading = r.Engagement().UnitHeadingDeg();
    for (std::size_t i = 0; i < launches.size() && i < ends.size(); ++i) {
        const fx::Event& l = launches[i];
        const fx::Event& d = ends[i];
        // Numbered after the launches (1000 and its count), so never one of a launch's numbers.
        SC_EXPECT(l.number == 1001 + static_cast<int>(i) && d.number == l.number && l.source == "own" && l.target.empty());
        SC_EXPECT(OnGround(r, l.position, 3.5) && l.world_id < 0);
        SC_EXPECT_NEAR(RadToDeg(ElevationOf(l.velocity)), 27.0, 1e-9);
        SC_EXPECT(std::fabs(std::remainder(RadToDeg(AzimuthOf(l.velocity)) - heading, 360.0)) < 1e-6);
        SC_EXPECT_NEAR(l.end_t_s - l.t_s, 25.5, 1e-9);  // launcher.emergency.flight_s (data/procedures/osa_ak.json)
        SC_EXPECT(d.end == fx::BurstEnd::Wide && d.seq > l.seq && d.t_s == l.t_s && d.end_t_s == l.end_t_s);
        SC_EXPECT((d.aim - l.position).Length() > 5000.0);  // where it destroys itself, far up its line
    }
    std::printf("    thrown off: missiles %d and %d, %.1f s each\n", launches.empty() ? 0 : launches[0].number,
                launches.size() < 2 ? 0 : launches[1].number, launches.empty() ? 0.0 : launches[0].end_t_s - launches[0].t_s);
}

// ------------------------------------------------------------------------------------------------ each attack's release

// Each attack the planner flies (planner.hpp MainRoute: the pull-up, apex, roll-in and release points from
// data/gameplay/air_war.json's delivery.*), its two aircraft played through the bridge onto the P-40 post 24 km north of
// the battery: the bombs go where the throw from the aircraft's state at the release best meets the aim ahead
// (the aim's distance ahead along its track over the throw, `ratio`). Level and laydown releases on the throw itself (the
// first step it reaches the aim: within a step's travel), a dive near it (as it rolls in, where the throw comes to the
// aim, or the step after it came nearest), a pop-up past the top of its climb (never in the climb: a toss), at once since
// its throw from there reaches past the aim. The routes' tosses level off at 600 m before their
// release point, from which no throw reaches the aim, so a toss lets go in its climb where the throw came nearest. (The
// fixed 2 km capture of the release point before let a level load go 2 km short of its 7.5 km throw, and a laydown 2 km
// short of its 0.7 km throw.) Each load lands after its flight from that state, timed from its height over the ground
// where it lands.
SC_TEST("fx (bridge): each attack the planner flies lets its bombs go where their throw from the aircraft's state meets the aim") {
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
    struct Attack {
        const char* name;
        Delivery delivery;
        Profile profile;
        const char* aircraft;
        const char* weapon;
        double agl;
    };
    const std::vector<Attack> attacks = {
        {"Lay 10", Delivery::Laydown, Profile::Low, "tornado_ids", "mk82r", 60.0},
        {"Lev 20", Delivery::Level, Profile::Medium, "f16a", "mk82", 4500.0},
        {"Loft 30", Delivery::Loft, Profile::Low, "f16a", "mk82", 100.0},
        {"Pop 40", Delivery::PopUp, Profile::Low, "f16a", "mk82", 100.0},
        {"Dive 50", Delivery::Dive, Profile::Medium, "f16a", "mk82", 4500.0},
        {"Gbu 60", Delivery::Level, Profile::Medium, "f16a", "gbu12", 4500.0},
    };
    for (const Attack& at : attacks) {
        Package p;
        p.id = 95;
        p.kind = MissionKind::Strike;
        p.part = DayPart::Day;
        p.target_object = "kesmark_p40";
        p.target_label = "the P-40 post";
        p.entry = "W3";
        p.exit = "W3";
        p.tot_h = 2.0;
        Flight fl;
        fl.callsign = at.name;
        fl.role = FlightRole::Strike;
        fl.aircraft = at.aircraft;
        fl.count = 2;
        fl.profile = at.profile;
        fl.weapon = at.weapon;
        fl.weapons = 6;
        fl.delivery = at.delivery;
        const double ipd = 20000.0;
        std::vector<RoutePoint> r;
        r.push_back({from, at.agl, 0.0, "entry"});
        r.push_back({post - dir * ipd + side * (0.25 * ipd), at.agl, 0.0, "ip"});
        double rs = 0.0;
        RoutePoint tg{post, at.agl, 0.0, "target"};
        if (at.delivery == Delivery::PopUp) {
            rs = t.Get("delivery.release_popup_m");
            const double pull = t.Get("delivery.popup_pull_m");
            r.push_back({post - dir * pull + side * 1500.0, at.agl, 0.0, "pull_up"});
            r.push_back({post - dir * (0.5 * (pull + rs)) + side * 800.0, t.Get("delivery.popup_apex_m"), 0.0, "apex"});
            tg.agl_m = t.Get("delivery.popup_release_m");
        } else if (at.delivery == Delivery::Loft) {
            rs = t.Get("delivery.release_loft_m");
            r.push_back({post - dir * t.Get("delivery.loft_pull_m"), at.agl, 0.0, "pull_up"});
            tg.agl_m = t.Get("delivery.loft_release_m");
        } else if (at.delivery == Delivery::Dive) {
            rs = t.Get("delivery.release_dive_m");
            r.push_back({post - dir * t.Get("delivery.dive_roll_in_m"), at.agl, 0.0, "roll_in"});
            tg.agl_m = std::min(at.agl, t.Get("delivery.dive_release_m"));
        }
        tg.pos = post - dir * rs;
        const std::size_t ri = r.size();
        r.push_back(tg);
        const Vec3 out = rs > 1500.0 ? tg.pos + side * 12000.0 - dir * 2000.0 : post + side * 12000.0 + dir * 4000.0;
        r.push_back({out, at.agl, 0.0, "egress"});
        r.push_back({from, at.agl, 0.0, "exit"});
        TimeRoute(r, ri, 2.0, 270.0);
        fl.route = r;
        fl.release_index = static_cast<int>(ri);
        fl.aim_pos = post;
        p.flights = {fl};
        RaidScenarioBuilder builder;
        const RaidScenario built = builder.Build(c, p);
        SC_EXPECT(built.ok);
        if (!built.ok) continue;
        ScenarioDef def;
        ConsoleData console;
        SC_EXPECT(LoadScenario(built.json_text, kData, def, console, &e));
        def.has_crew_data = false;
        // This test is about the release geometry of an undisturbed attack: the pilots carry no receiver here, so none of
        // them jinks or breaks on hearing the unit's lock (those reactions are tested with B3's perception tests).
        for (ScenarioTarget& pilot : def.targets) pilot.rwr.clear();
        ScenarioRunner runner(std::move(def), std::move(console));
        MissionBridge bridge(built.manifest, c.Data(), 3u);
        std::size_t released = 0;
        while (!runner.Finished() && !bridge.UnitDestroyed() && runner.Time() < built.manifest.end_s + 60.0 && released < 2) {
            runner.Step(0.1);
            bridge.Step(runner);
            released = OfKind(runner.Fx().Since(0), fx::EventKind::WeaponRelease).size();
        }
        SC_EXPECT(released == 2);
        const World& w = runner.GetWorld();
        for (const fx::Event& ev : OfKind(runner.Fx().Since(0), fx::EventKind::WeaponRelease)) {
            const RaidAircraft* a = nullptr;
            for (const RaidAircraft& m : built.manifest.aircraft) {
                if (m.target_id == ev.source) a = &m;
            }
            SC_EXPECT(a != nullptr);
            if (a == nullptr) continue;
            // The aircraft's state at the release: its path angle from its velocity against the local up.
            const ReleaseState st = ReleaseStateOf(w.Frame(), ev.position, ev.velocity, ev.delivery);
            // Its throw to the aim (the flight's, before the scatter) from its height over the aim's ground.
            const double h_aim = w.Frame().AltitudeOf(ev.position) - runner.GroundHeightAt(a->aim_enu.x, a->aim_enu.y);
            const Vec3 to_aim = runner.GroundEnu(a->aim_enu.x, a->aim_enu.y) - ev.position;
            const double along = Dot(to_aim, st.along);
            const WeaponFlight wf = WeaponFlightOf(ev.delivery, ev.weapon, h_aim, st.speed_mps, st.path_deg, to_aim.Length(), &c.Data().weapons);
            SC_EXPECT(wf.throw_m > 0.0);
            const double ratio = along / wf.throw_m;
            if (at.delivery == Delivery::Level || at.delivery == Delivery::Laydown) {
                SC_EXPECT(along <= wf.throw_m + 1e-6 && along > wf.throw_m - st.ground_mps * 0.1 - 10.0);
            } else if (at.delivery == Delivery::Loft) {
                SC_EXPECT(ratio > 1.0 && ratio < 1.5 && st.path_deg > 10.0);  // in the climb
            } else if (at.delivery == Delivery::PopUp) {
                // Past the top of its climb (its path at or below the horizontal, nearer the aim than the apex), never in
                // the climb to it (that is a toss). The runner's terrain-following climb tops out near the release point
                // (about 1140 m up, 2 km short), not at the planner's apex, and goes on almost level, so from there the
                // throw already reaches past the aim and it lets go at once: the aim half the throw (docs/fx/README.md 7).
                const double apex_m = 0.5 * (t.Get("delivery.popup_pull_m") + t.Get("delivery.release_popup_m"));
                SC_EXPECT(st.path_deg <= 0.0 && along < apex_m);
                SC_EXPECT(ratio > 0.4 && ratio <= 1.0);
            } else {
                SC_EXPECT(ratio > 0.8 && ratio < 1.35);
            }
            // Timed by the weapon's flight from that state, from its height over the ground where the load lands (L7).
            const double h_impact = w.Frame().AltitudeOf(ev.position) - runner.GroundHeightAt(ev.aim.x, ev.aim.y);
            const WeaponFlight lands =
                WeaponFlightOf(ev.delivery, ev.weapon, h_impact, st.speed_mps, st.path_deg, (ev.aim - ev.position).Length(), &c.Data().weapons);
            SC_EXPECT_NEAR(ev.end_t_s - ev.t_s, lands.time_s, 1e-9);
            std::printf("    %-7s %-7s %-5s from %4.0f m at %3.0f m/s, %5.1f deg: the aim %4.0f m ahead, the throw %4.0f m (%.2f); %4.1f s to impact\n",
                        ev.source.c_str(), ev.delivery.c_str(), ev.weapon.c_str(), h_aim, st.speed_mps, st.path_deg, along, wf.throw_m, ratio,
                        ev.end_t_s - ev.t_s);
        }
    }
}
