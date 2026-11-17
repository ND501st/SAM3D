// The enemy's packages entering a running mission (roadmap 15.3): ScenarioRunner::AddTargets, AddEmGear and
// AddTriggers, and simcore/air_war/package_injector.hpp (InjectPackage, LivePackages).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
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

AirWarData LoadWar(const std::string& faction) {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

// The campaign's first package through the battery's sector that builds as a scenario.
bool FirstRaid(AirCampaign& c, RaidScenario& rs) {
    c.Posture().emission = 0.1;
    c.Posture().relocate_every_h = 20.0;
    while (c.Now() < 6.0 * 24.0) {
        if (!c.Advance(6.0 * 24.0)) continue;
        RaidScenarioBuilder builder;
        rs = builder.Build(c, *c.PlayerMission());
        if (rs.ok) return true;
        c.SkipPlayerMission();
    }
    return false;
}

// The campaign's first package to the battery (seed `seed`) run in step with a session that starts three minutes before
// it (as the test "the campaign runs in step with a session" below), to its hand-back: its twin's raid, what the
// session's feed showed of its loads, and how much of the feed and of the bridge's log ("enemy air") came dated after
// the session's time when it came.
struct LiveRun {
    bool ok = false;
    RaidScenario rs;  // as its twin built it
    LivePackages::Played played;
    std::vector<fx::Event> releases, impacts;
    int ahead = 0;
    double end_s = 0.0;  // the raid's end on the session's clock
};
LiveRun RunFirstPackage(const AirWarData& d, unsigned seed) {
    LiveRun out;
    AirCampaign c1;
    SC_EXPECT(c1.Init(d, seed));
    if (!FirstRaid(c1, out.rs)) return out;
    const double due_h = c1.Now();
    AirCampaign c;
    SC_EXPECT(c.Init(d, seed));
    c.Posture().emission = 0.1;
    c.Posture().relocate_every_h = 20.0;
    while (c.Advance(due_h - 0.05)) c.SkipPlayerMission();
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(out.rs.json_text, kData, def, console, &e));
    def.targets.clear();
    def.triggers.clear();
    def.em.jammers.clear();
    def.time_limit_s = 3600.0;
    def.end_when_targets_gone = false;
    ScenarioRunner r(std::move(def), std::move(console));
    LivePackages live(c, kData, c.Now());
    std::uint64_t cursor = 0;
    std::size_t log_seen = 0;
    while (!r.Finished() && r.Time() < 3600.0) {
        r.Step(0.1);
        live.Step(r);
        for (const fx::Event& ev : r.Fx().Since(cursor)) {
            if (ev.kind == fx::EventKind::WeaponRelease && ev.delivery != "jettison") out.releases.push_back(ev);  // not a load thrown off (B4)
            if (ev.kind == fx::EventKind::WeaponImpact) out.impacts.push_back(ev);
            out.ahead += ev.t_s > r.Time() + 1e-9 ? 1 : 0;
        }
        cursor = r.Fx().Last();
        const std::vector<DebriefEntry>& log = r.Log().Entries();
        for (; log_seen < log.size(); ++log_seen) out.ahead += log[log_seen].who == "enemy air" && log[log_seen].time_s > r.Time() + 1e-9 ? 1 : 0;
        if (!live.PlayedPackages().empty() && !live.Active()) break;
    }
    if (live.PlayedPackages().size() != 1) return out;
    out.played = live.PlayedPackages().front();
    out.end_s = out.rs.manifest.end_s + out.played.in_s;
    out.ok = true;
    return out;
}

// The raid's own triggers and EM jammers, as its scenario has them.
void RaidGear(const RaidScenario& rs, std::size_t& triggers, std::size_t& jammers) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenario(rs.json_text, kData, def, console, &e)) return;
    triggers = def.triggers.size();
    jammers = def.em.jammers.size();
}

}  // namespace

SC_TEST("packages live (15.3): aircraft added to a running session appear on time, are targets like its own, and an id twice is refused") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &e));
    if (def.targets.empty()) return;
    ScenarioTarget late = def.targets.front();
    ScenarioRunner r(std::move(def), std::move(console));
    while (r.Time() < 60.0) r.Step(0.1);
    late.id = "late-1";
    late.name = "a second drone";
    late.appear_at_s = 90.0;
    ScenarioTarget twice = late;
    std::string why;
    SC_EXPECT(!r.AddTargets({late, twice}, "package 1", &why) && why.find("already") != std::string::npos);
    SC_EXPECT(r.TargetRange("late-1") < 0.0);  // all or none: nothing was added
    SC_EXPECT(!r.AddTargets({}, "package 1", &why));
    SC_EXPECT(r.AddTargets({late}, "package 1", &why));
    SC_EXPECT(!r.AddTargets({late}, "package 2", &why) && why.find("already") != std::string::npos);
    while (r.Time() < 89.0) r.Step(0.1);
    SC_EXPECT(r.TargetRange("late-1") < 0.0);  // not yet
    while (r.Time() < 91.0) r.Step(0.1);
    SC_EXPECT(r.TargetRange("late-1") > 0.0);
    // It keeps the session from ending while it flies, and is in its debrief like the scenario's own.
    while (!r.Finished() && r.Time() < 1500.0) r.Step(0.1);
    const std::string d = r.Debrief();
    SC_EXPECT(d.find("package 1") != std::string::npos && d.find("appeared: a second drone") != std::string::npos);
}

SC_TEST("packages live (15.3): a point moved between two frames near the battery keeps its place on the earth") {
    GeoPoint a = GeoPoint{DegToRad(46.90), DegToRad(19.70), 120.0};
    GeoPoint b = GeoPoint{DegToRad(46.95), DegToRad(19.80), 150.0};
    const LocalFrame fa(a), fb(b);
    const Vec3 p{12000.0, -8000.0, 600.0};
    const Vec3 q = MoveFrame(fa, fb, p);
    const Vec3 back = MoveFrame(fb, fa, q);
    SC_EXPECT_NEAR(back.x, p.x, 1e-6);
    SC_EXPECT_NEAR(back.y, p.y, 1e-6);
    SC_EXPECT_NEAR(back.z, p.z, 1e-6);
    const GeoPoint ga = fa.EnuToGeodetic(p), gb = fb.EnuToGeodetic(q);
    SC_EXPECT_NEAR(ga.lat_rad, gb.lat_rad, 1e-12);
    SC_EXPECT_NEAR(ga.lon_rad, gb.lon_rad, 1e-12);
    SC_EXPECT_NEAR(ga.alt_m, gb.alt_m, 1e-6);
    // About 0.1 deg of longitude east and 0.05 deg of latitude north of a: b's frame sees it that much south-west.
    SC_EXPECT(q.x < p.x - 7000.0 && q.y < p.y - 5000.0);
}

SC_TEST("packages live (15.3): a raid injected into a running session flies as the same raid played as a scenario, and its bridge reads it") {
    const AirWarData d = LoadWar("nato");
    AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    RaidScenario rs;
    SC_EXPECT(FirstRaid(c, rs));
    if (!rs.ok) return;
    // As built: the raid its own scenario.
    ScenarioDef da;
    ConsoleData ca;
    std::string e;
    SC_EXPECT(LoadScenario(rs.json_text, kData, da, ca, &e));
    if (da.targets.empty()) return;
    std::string first = da.targets.front().id;
    double first_s = da.targets.front().appear_at_s;
    for (const ScenarioTarget& t : da.targets) {
        if (t.appear_at_s < first_s) {
            first = t.id;
            first_s = t.appear_at_s;
        }
    }
    const double look_s = first_s + 5.0;
    ScenarioRunner ra(std::move(da), std::move(ca));
    while (ra.Time() < look_s - 1e-6) ra.Step(0.1);
    const Vec3 seen_a = ra.TargetPosition(first);
    // Injected: the same session with no aircraft of its own until the package comes in 300 s later than it would.
    const double at_s = 300.0;
    ScenarioDef db;
    ConsoleData cb;
    SC_EXPECT(LoadScenario(rs.json_text, kData, db, cb, &e));
    db.targets.clear();
    db.triggers.clear();
    db.em.jammers.clear();
    db.time_limit_s += at_s;
    ScenarioRunner rb(std::move(db), std::move(cb));
    while (rb.Time() < 120.0) rb.Step(0.1);
    const InjectedPackage inj = InjectPackage(rb, rs, kData, at_s);
    if (!inj.ok) std::printf("    %s\n", inj.error.c_str());
    SC_EXPECT(inj.ok && inj.aircraft == rs.manifest.aircraft.size());
    if (!inj.ok) return;
    // Twice is refused: its aircraft are in the session already.
    SC_EXPECT(!InjectPackage(rb, rs, kData, at_s).ok);
    SC_EXPECT_NEAR(inj.manifest.end_s, rs.manifest.end_s + at_s, 1e-9);
    SC_EXPECT_NEAR(inj.manifest.start_h, rs.manifest.start_h - at_s / 3600.0, 1e-12);
    for (std::size_t i = 0; i < inj.manifest.aircraft.size(); ++i) {
        SC_EXPECT_NEAR(inj.manifest.aircraft[i].appear_s, rs.manifest.aircraft[i].appear_s + at_s, 1e-9);
        SC_EXPECT_NEAR(inj.manifest.aircraft[i].aim_enu.x, rs.manifest.aircraft[i].aim_enu.x, 1e-3);  // the same frame
    }
    while (rb.Time() < at_s + look_s - 1e-6) rb.Step(0.1);
    const Vec3 seen_b = rb.TargetPosition(first);
    std::printf("    %s %.0f s after the start: %.1f m apart as built and as injected\n", first.c_str(), look_s, (seen_b - seen_a).Length());
    SC_EXPECT((seen_b - seen_a).Length() < 1.0);
    // The raid's bridge on the moved manifest plays it out in the session.
    MissionBridge bridge(inj.manifest, c.Data(), 5u);
    while (!rb.Finished() && !bridge.UnitDestroyed() && rb.Time() < inj.manifest.end_s + 60.0) {
        rb.Step(0.1);
        bridge.Step(rb);
    }
    const PackageOutcome o = bridge.Finish(rb);
    SC_EXPECT(o.package_id == rs.manifest.package_id && o.played);
    SC_EXPECT(bridge.EmittedSeconds() > 0.0);
    SC_EXPECT(bridge.Over(rb));
    // Its warning and its jammers came with it, on the session's clock.
    std::size_t raid_triggers = 0, raid_jammers = 0;
    RaidGear(rs, raid_triggers, raid_jammers);
    std::printf("    %zu triggers, %zu jammers came with it\n", inj.triggers, inj.jammers);
    SC_EXPECT(inj.triggers == raid_triggers && inj.jammers == raid_jammers && inj.notes.empty());
    const std::string debrief = rb.Debrief();
    SC_EXPECT(inj.triggers == 0 || debrief.find("package " + std::to_string(rs.manifest.package_id) + " air_raid_warning") != std::string::npos);
}

SC_TEST("packages live (15.3): the campaign runs in step with a session, hands it its package live and takes the outcome back") {
    const AirWarData d = LoadWar("nato");
    // When the first package comes to the battery (a twin of the campaign, same seed and posture).
    AirCampaign c1;
    SC_EXPECT(c1.Init(d, 1));
    RaidScenario rs;
    SC_EXPECT(FirstRaid(c1, rs));
    if (!rs.ok) return;
    const double due_h = c1.Now();
    // The session starts three minutes before, its campaign at that hour (the packages that would not build skipped,
    // as the twin did).
    AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    c.Posture().emission = 0.1;
    c.Posture().relocate_every_h = 20.0;
    while (c.Advance(due_h - 0.05)) c.SkipPlayerMission();
    SC_EXPECT(!c.HasPlayerMission());
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(rs.json_text, kData, def, console, &e));
    def.targets.clear();
    def.triggers.clear();
    def.em.jammers.clear();
    def.time_limit_s = 3600.0;
    def.end_when_targets_gone = false;  // the mission goes on after a package
    ScenarioRunner r(std::move(def), std::move(console));
    LivePackages live(c, kData, c.Now());
    while (!r.Finished() && r.Time() < 3600.0) {
        r.Step(0.1);
        live.Step(r);
        if (!live.PlayedPackages().empty() && !live.Active()) break;
    }
    for (const std::string& n : live.Notes()) std::printf("    %s\n", n.c_str());
    SC_EXPECT(live.PlayedPackages().size() == 1);
    if (live.PlayedPackages().empty()) return;
    const LivePackages::Played& p = live.PlayedPackages().front();
    std::printf("    package %d '%s': %zu aircraft, %zu jammers, in at %.0f s, back at %.0f s, %d lost\n", p.package_id, p.name.c_str(),
                p.aircraft, p.jammers, p.in_s, p.out_s, p.lost);
    SC_EXPECT(p.package_id == rs.manifest.package_id);
    SC_EXPECT(p.aircraft == rs.manifest.aircraft.size());
    // It entered on its own time (the raid's t = 0 after the session's start) and was handed back once over.
    SC_EXPECT(p.in_s > 0.0 && p.out_s > p.in_s);
    SC_EXPECT(!c.HasPlayerMission());
    bool played = false;
    for (const Package& pk : c.Packages()) played = played || (pk.id == p.package_id && pk.played);
    SC_EXPECT(played);
    // The campaign goes on from there: the session's next minutes advance it.
    const double before_h = c.Now();
    for (int i = 0; i < 1200 && !r.Finished(); ++i) {
        r.Step(0.1);
        live.Step(r);
    }
    SC_EXPECT(c.Now() > before_h);
}

// A package over by its clock (the session past its raid's end, MissionBridge::Over) while its loads still fall: they land
// in the session at their own times and the package is handed back only once the last has (LivePackages,
// MissionBridge::FallingUntil); nothing of it is dated ahead of the session (Finish would have landed them at once, their
// blasts and impacts dated seconds later than the session). The raid's end is put (scenario.margin_s) a second after the
// last release a first run of the same package showed.
SC_TEST("packages live (15.3): a package over by its clock while its loads still fall is handed back once they have landed") {
    const AirWarData d = LoadWar("nato");
    LiveRun first;
    unsigned seed = 1;
    double last_release_s = -1.0, last_impact_s = -1.0;
    for (; seed <= 12; ++seed) {
        first = RunFirstPackage(d, seed);
        if (!first.ok || first.releases.empty()) continue;
        last_release_s = last_impact_s = -1.0;
        for (const fx::Event& ev : first.releases) {
            last_release_s = std::fmax(last_release_s, ev.t_s);
            last_impact_s = std::fmax(last_impact_s, ev.end_t_s);
        }
        if (last_impact_s > last_release_s + 2.0) break;  // a load still falls a second after the last release
    }
    SC_EXPECT(seed <= 12 && first.ok && first.ahead == 0);
    if (seed > 12 || !first.ok) return;
    const double end_s = last_release_s + 1.0;
    AirWarData shortened = d;
    shortened.tuning.values["scenario.margin_s"] = d.tuning.Get("scenario.margin_s") + (end_s - first.end_s);
    const LiveRun second = RunFirstPackage(shortened, seed);
    SC_EXPECT(second.ok);
    if (!second.ok) return;
    std::printf("    seed %u, package %d '%s': in at %.0f s, its last release at %.1f s, its end put at %.1f s; its last load lands at "
                "%.1f s, handed back at %.1f s (%d of %d loads landed)\n",
                seed, second.played.package_id, second.played.name.c_str(), second.played.in_s, last_release_s, second.end_s,
                last_impact_s, second.played.out_s, static_cast<int>(second.impacts.size()), static_cast<int>(second.releases.size()));
    SC_EXPECT(second.played.package_id == first.played.package_id && second.played.in_s == first.played.in_s);
    SC_EXPECT_NEAR(second.end_s, end_s, 1e-6);
    // The same releases, all before its end; every load landed in the session, at its own time.
    SC_EXPECT(second.releases.size() == first.releases.size() && second.impacts.size() == second.releases.size());
    double last_s = -1.0;
    for (const fx::Event& rel : second.releases) {
        SC_EXPECT(rel.t_s < end_s);
        last_s = std::fmax(last_s, rel.end_t_s);
        int landed = 0;
        for (const fx::Event& imp : second.impacts) {
            if (imp.source != rel.source) continue;
            ++landed;
            SC_EXPECT(imp.t_s >= rel.end_t_s - 1e-9 && imp.t_s < rel.end_t_s + 0.1 + 1e-9);
        }
        SC_EXPECT(landed == 1);
    }
    SC_EXPECT_NEAR(last_s, last_impact_s, 1e-6);
    // Handed back once the last has landed, not at its end; nothing dated ahead of the session.
    SC_EXPECT(second.played.out_s > end_s + 1.0 && second.played.out_s >= last_s - 1e-9 && second.played.out_s < last_s + 0.1 + 1e-6);
    SC_EXPECT(second.ahead == 0 && second.played.outcome.delivered);
}

SC_TEST("packages live (15.3): a package's EM gear and warning enter a running session: its library, a stand-off jammer on at its time, an air threat") {
    em::EmLibrary nato;
    SC_EXPECT(em::LoadEmLibraryFile(kData + "/em/nato_1985.json", nato).ok);
    EmJammerDef soj;
    soj.jammer_id = "nato_alq99_band8";
    soj.position = Vec3{20000.0, 30000.0, 8000.0};
    soj.on_at_s = 40.0;
    std::string why;
    {  // Without the EM layer there is nothing for it to jam.
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &e));
        ScenarioRunner plain(std::move(def), std::move(console));
        SC_EXPECT(!plain.AddEmGear(nato, {soj}, "package 1", &why) && why.find("no EM layer") != std::string::npos);
    }
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice_em.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    while (r.Time() < 20.0) r.Step(0.1);
    SC_EXPECT(r.Em() != nullptr);
    if (r.Em() == nullptr) return;
    const em::EmScene& scene = r.Em()->World().Scene();
    const int soc = scene.lib.Radar("osa_ak_soc");
    SC_EXPECT(soc >= 0 && scene.lib.Jammer("nato_alq99_band8") < 0);
    EmJammerDef unknown = soj;
    unknown.jammer_id = "no_such_jammer";
    SC_EXPECT(!r.AddEmGear(nato, {soj, unknown}, "package 1", &why) && why.find("no_such_jammer") != std::string::npos);
    SC_EXPECT(scene.lib.Jammer("nato_alq99_band8") < 0);  // all or none
    const std::size_t jammers_before = scene.jammers.size();
    SC_EXPECT(r.AddEmGear(nato, {soj}, "package 1", &why));
    // The library grew; what was in it kept its place.
    const int alq = scene.lib.Jammer("nato_alq99_band8");
    SC_EXPECT(alq >= 0 && scene.lib.Radar("osa_ak_soc") == soc);
    // The stand-off jammer is in the sky now, silent until its time.
    SC_EXPECT(scene.jammers.size() == jammers_before + 1);
    const auto jamming = [&]() {
        for (const em::JammerState& j : scene.jammers)
            if (j.def_index == alq && j.on) return true;
        return false;
    };
    r.Step(0.1);
    SC_EXPECT(!jamming());
    while (r.Time() < 41.0) r.Step(0.1);
    SC_EXPECT(jamming());
    // Its warning: an air threat on the session's clock. A raid's console orders stay out (another vehicle's crew).
    ScenarioTrigger warn;
    warn.id = "package 1 air_raid_warning";
    warn.after_s = 60.0;
    TriggerAction threat;
    threat.kind = TriggerAction::Kind::AirThreat;
    threat.air_threat = true;
    warn.actions.push_back(threat);
    ScenarioTrigger order = warn;
    order.id = "package 1 order";
    order.actions.front().kind = TriggerAction::Kind::Order;
    SC_EXPECT(!r.AddTriggers({warn, order}, &why) && why.find("package 1 order") != std::string::npos);
    SC_EXPECT(r.AddTriggers({warn}, &why));
    SC_EXPECT(!r.AddTriggers({warn}, &why));  // its id is the session's now
    while (r.Time() < 59.0) r.Step(0.1);
    SC_EXPECT(r.Debrief().find("package 1 air_raid_warning") == std::string::npos);
    while (r.Time() < 61.0) r.Step(0.1);
    SC_EXPECT(r.Debrief().find("package 1 air_raid_warning") != std::string::npos);
}
