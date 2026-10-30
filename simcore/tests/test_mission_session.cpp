// The battery's mission as one session with the enemy's air war beside it (roadmap 15.2, first form):
// simcore/campaign/mission_session.hpp.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

}  // namespace

SC_TEST("mission session (15.2): a battery mission without a war is the scenario alone") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    SC_EXPECT(s.Campaign() == nullptr && s.Packages() == nullptr && s.Runner().HasTask());
    // A war needs the scenario's clock to put its day on.
    md.scenario = kData + "/scenarios/osa_practice.json";
    md.faction = "nato";
    SC_EXPECT(!s.Start(md, kData, &e) && e.find("clock_start") != std::string::npos);
}

SC_TEST("mission session (15.2): the bridges mission with the Pact's air war on its clock: a package comes in on the march, the arrival is the campaign's") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.faction = "warsaw_pact";
    md.seed = 1;
    md.day = 1;
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Campaign() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    const airwar::AirCampaign& c = *s.Campaign();
    // 19:00 on the war's first day, the theatre's day starting at its start hour.
    SC_EXPECT_NEAR(s.CampaignHourAtStart(), 19.0 - c.Data().theatre.start_hour, 1e-9);
    SC_EXPECT(c.Now() <= s.CampaignHourAtStart() + 1e-9 && !c.HasPlayerMission());
    // The campaign's battery stands where the session's launchers do.
    const Vec3 at_start = c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos;
    Vec3 launchers;
    int n = 0;
    for (const Battery::Vehicle& v : s.Runner().GetBattery()->Vehicles()) {
        if (v.def.kind != BatteryVehicleKind::Launcher) continue;
        launchers += v.position;
        ++n;
    }
    launchers = launchers / static_cast<double>(n);
    SC_EXPECT(std::hypot(at_start.x - launchers.x, at_start.y - launchers.y) < 50.0);  // the same origin as the theatre's
    ScenarioRunner& r = s.Runner();
    // The regiment's air picture comes in squares and late: a launcher may find nothing where it was told to look
    // («цели не наблюдаю»). The commander then gives that mark to nobody again until a fresher one is on his board.
    // A launcher that read a mark back has it, whatever aircraft it found there: he gives it to no second launcher.
    std::map<std::string, int> last_number;  // launcher -> the mark it was last given
    std::map<int, double> nothing_there_s;   // mark -> when a launcher said nothing was there
    struct Given {
        std::string launcher;
        double at_s = 0.0;
    };
    std::map<int, Given> given;  // a single mark -> the launcher it was last given to
    int answered = 0, stale = 0, doubled = 0;
    std::size_t seen = 0;
    while (!r.Finished()) {
        s.Step(0.1);
        const std::vector<DebriefEntry>& log = r.Log().Entries();
        for (; seen < log.size(); ++seen) {
            const DebriefEntry& entry = log[seen];
            if (entry.who == "Battery commander" && entry.detail.find(" by voice") != std::string::npos) {
                const int number = std::atoi(entry.detail.c_str() + entry.detail.find(' ') + 1);
                const auto told = nothing_there_s.find(number);
                if (told != nothing_there_s.end()) {
                    double mark_s = -1.0;
                    for (const BoardMark& m : r.PostBoard()) mark_s = m.number == number ? std::max(mark_s, m.time_s) : mark_s;
                    if (mark_s <= told->second) {
                        ++stale;
                        std::printf("    %.1f s: mark %d to %s again, no newer than the %.1f s «цели не наблюдаю»\n", entry.time_s, number,
                                    entry.what.c_str(), told->second);
                    }
                }
                if (entry.detail.rfind("target ", 0) == 0) {
                    const auto before = given.find(number);
                    if (before != given.end() && before->second.launcher != entry.what && entry.time_s - before->second.at_s < 60.0) {
                        ++doubled;
                        std::printf("    %.1f s: mark %d to %s, %.1f s after %s had it\n", entry.time_s, number, entry.what.c_str(),
                                    entry.time_s - before->second.at_s, before->second.launcher.c_str());
                    }
                    given[number] = {entry.what, entry.time_s};
                }
                last_number[entry.what] = number;
            } else if (entry.who.empty() && entry.detail == "no target where it was designated" && last_number.count(entry.what) != 0) {
                nothing_there_s[last_number[entry.what]] = entry.time_s;
                given.erase(last_number[entry.what]);
                ++answered;
            }
        }
    }
    std::printf("    «цели не наблюдаю» %d times, %d designations\n", answered, r.DesignationsMade());
    SC_EXPECT(answered > 0);
    SC_EXPECT(stale == 0);
    SC_EXPECT(doubled == 0);
    const airwar::LivePackages& live = *s.Packages();
    for (const std::string& note : live.Notes()) std::printf("    %s\n", note.c_str());
    SC_EXPECT(!live.PlayedPackages().empty());
    for (const airwar::LivePackages::Played& p : live.PlayedPackages()) {
        std::printf("    package %d '%s': %zu aircraft, in at %s, back at %s, %d shot down\n", p.package_id, p.name.c_str(), p.aircraft,
                    r.TaskClock(p.in_s).c_str(), p.out_s >= 0.0 ? r.TaskClock(p.out_s).c_str() : "-", p.lost);
    }
    // The package came while the battery was getting ready (before the march's end), and the battery still made its
    // ready-by; its aircraft were in the session.
    if (!live.PlayedPackages().empty()) {
        const airwar::LivePackages::Played& p = live.PlayedPackages().front();
        SC_EXPECT(p.in_s > 0.0 && p.in_s < r.TaskPlan().arrive_s && p.out_s > p.in_s);
        const std::string d = r.Debrief();
        SC_EXPECT(d.find("package " + std::to_string(p.package_id)) != std::string::npos && d.find("appeared:") != std::string::npos);
    }
    SC_EXPECT(r.TaskReadyS() >= 0.0 && r.TaskReadyS() <= r.TaskPlan().ready_by_s);
    // The launcher's kill on it went back to the campaign as a loss of the package (the bridge reads the battery fire
    // model's "destroys").
    if (!live.PlayedPackages().empty() && r.Debrief().find(" destroys ") != std::string::npos) {
        SC_EXPECT(live.PlayedPackages().front().lost >= 1);
    }
    // The battery's new position became the campaign's once it arrived, and the war went on with the session's clock.
    SC_EXPECT(s.BatteryMoved());
    const Vec3 at_end = c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos;
    SC_EXPECT(std::hypot(at_end.x - at_start.x, at_end.y - at_start.y) > 2000.0);
    SC_EXPECT(c.Now() > s.CampaignHourAtStart() + 1.0);
    // The two outcomes (14.6): the bridges covered from the battery's readiness on, every package back from the
    // session in the ledger in its turn, the enemy's phase checked; the debrief shows both, the combat report neither.
    const campaign::OutcomeLedger& o = s.Outcomes();
    const campaign::CoveredAsset* bridges = o.Find("kesmark_bridges");
    SC_EXPECT(bridges != nullptr);
    if (bridges != nullptr) {
        SC_EXPECT(bridges->object == "kesmark_road_bridge");
        SC_EXPECT(bridges->covered_h > 0.0 && bridges->working_h <= bridges->covered_h + 1e-9);
        SC_EXPECT_NEAR(bridges->covered_h, (r.Time() - r.TaskReadyS()) / 3600.0, 0.5 / 3600.0);  // from its «к бою готова» up
    }
    std::size_t back = 0;
    while (back < live.PlayedPackages().size() && live.PlayedPackages()[back].out_s >= 0.0) ++back;
    SC_EXPECT(o.Packages().size() == back);
    for (std::size_t i = 0; i < o.Packages().size() && i < back; ++i) {
        const campaign::PackageLedger& l = o.Packages()[i];
        SC_EXPECT(l.id == live.PlayedPackages()[i].package_id);
        SC_EXPECT(l.released + l.Prevented() == l.release_flights);
    }
    SC_EXPECT(!o.Checks().empty());
    const std::string debrief = s.Debrief();
    const std::size_t outcomes_at = debrief.find("The battery's two outcomes");
    SC_EXPECT(outcomes_at != std::string::npos);
    if (outcomes_at != std::string::npos) std::printf("%s", debrief.c_str() + outcomes_at);
    SC_EXPECT(debrief.find("Assets kept:") != std::string::npos && debrief.find("Freedom of action denied:") != std::string::npos);
    SC_EXPECT(r.Debrief().find("Assets kept:") == std::string::npos);
}

SC_TEST("mission bridge: a kill by the battery's launchers is the package's loss, unless the launcher takes it back") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    airwar::RaidScenario rs;
    while (c.Now() < 6.0 * 24.0 && !rs.ok) {
        if (!c.Advance(6.0 * 24.0)) break;
        rs = airwar::RaidScenarioBuilder().Build(c, *c.PlayerMission());
        if (!rs.ok) c.SkipPlayerMission();
    }
    SC_EXPECT(rs.ok && !rs.manifest.aircraft.empty());
    if (!rs.ok || rs.manifest.aircraft.empty()) return;
    const std::string first = rs.manifest.aircraft.front().target_id;
    const auto lost = [&](bool take_back) {
        ScenarioDef def;
        ConsoleData console;
        std::string err;
        if (!LoadScenario(rs.json_text, kData, def, console, &err)) return -1;
        ScenarioRunner r(std::move(def), std::move(console));
        airwar::MissionBridge bridge(rs.manifest, c.Data(), 3u);
        for (int i = 0; i < 20; ++i) {
            r.Step(0.1);
            bridge.Step(r);
        }
        // As the runner logs a battery launcher's kill (BatteryFire's Hit) and its taking it back (a Miss after it).
        r.Log().Record({r.Time(), DebriefEntry::Kind::Scenario, "", "22", "destroys " + first});
        if (take_back) r.Log().Record({r.Time(), DebriefEntry::Kind::Scenario, "", "22", "takes back the kill of " + first});
        return bridge.Finish(r).Lost();
    };
    SC_EXPECT(lost(false) == 1);
    SC_EXPECT(lost(true) == 0);
}

SC_TEST("our posts (17.3): a radar of the regiment off the air drops out of the post's feed, and its picture comes back with it") {
    // By hand: one radar at the origin, 100 km, its antenna 10 m up; a target 20 km off at 1000 m is within its range and
    // above its horizon (4.12 (sqrt 10 + sqrt 1000) km = 143 km).
    PostRegiment reg;
    RegimentRadar a;
    a.id = "a";
    a.max_range_m = 100000.0;
    a.antenna_height_m = 10.0;
    reg.radars = {a};
    BoardContact c;
    c.target_id = "t";
    c.position = Vec3{20000.0, 0.0, 1000.0};
    c.altitude_m = c.height_agl_m = 1000.0;
    SC_EXPECT(CommandPost::RegimentSees(reg, {c}, 50.0).size() == 1);
    C2Outage o;
    o.from_s = 100.0;
    o.to_s = 200.0;
    reg.radars[0].outages = {o};
    SC_EXPECT(CommandPost::RegimentSees(reg, {c}, 50.0).size() == 1);
    SC_EXPECT(CommandPost::RegimentSees(reg, {c}, 150.0).empty());  // its only radar off the air: the regiment sees nothing
    SC_EXPECT(CommandPost::RegimentSees(reg, {c}, 250.0).size() == 1);
    // In a session: a radar joins the feed once, of a type in the post's library; one goes off the air and on again; the
    // regiment's word reaches the post on its net.
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_mission_cover.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.AddRegimentRadar("p40_test", "p40", Vec3{2000.0, 14000.0, 2.0}, &e));
    SC_EXPECT(!r.AddRegimentRadar("p40_test", "p40", Vec3{}, &e));
    SC_EXPECT(!r.AddRegimentRadar("x", "no_such_radar", Vec3{}, &e));
    SC_EXPECT(r.Def().post_regiment.radars.size() == 2);
    SC_EXPECT(r.SetRegimentRadarDown("p19_regiment", true, "struck"));
    SC_EXPECT(!r.SetRegimentRadarDown("p19_regiment", true, "struck"));
    SC_EXPECT(!r.SetRegimentRadarDown("no_such_radar", true, "struck"));
    SC_EXPECT(CommandPost::RadarDownAt(r.Def().post_regiment.radars[0], r.Time()));
    SC_EXPECT(r.RegimentSays("Regiment to the battery: a test word.", 5.0));
    while (r.Time() < 120.0) r.Step(0.1);
    SC_EXPECT(r.SetRegimentRadarDown("p19_regiment", false, "repaired"));
    SC_EXPECT(!CommandPost::RadarDownAt(r.Def().post_regiment.radars[0], r.Time()));
    bool heard = false;
    for (const DebriefEntry& entry : r.Log().Entries()) {
        heard = heard || (entry.what == "regiment net" && entry.detail.rfind("heard", 0) == 0 && entry.detail.find("a test word.") != std::string::npos);
    }
    SC_EXPECT(heard);
}

SC_TEST("our posts (17.3): the regiment's radars the war struck before the mission join the post's feed off the air, and the regiment says so") {
    // The Pact's war, seed 1: by 19:00 on its first day the P-40 'Kesmark' and the P-19 'Ludas' are wrecked; seed 2 leaves
    // the P-40 standing.
    for (unsigned seed : {1u, 2u}) {
        campaign::MissionSessionDef md;
        md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
        md.faction = "warsaw_pact";
        md.seed = seed;
        md.day = 1;
        campaign::MissionSession s;
        std::string e;
        SC_EXPECT(s.Start(md, kData, &e));
        if (s.Campaign() == nullptr) return;
        const airwar::AirCampaign& c = *s.Campaign();
        const auto function = [&c](const std::string& id) {
            for (std::size_t i = 0; i < c.Data().theatre.objects.size(); ++i) {
                if (c.Data().theatre.objects[i].id == id) return c.Truth()[i].function;
            }
            return -1.0;
        };
        const ScenarioRunner& r = s.Runner();
        int joined = 0;
        for (const campaign::MissionSession::OurPost& p : s.OurPosts()) {
            if (!p.radar) continue;
            ++joined;
            SC_EXPECT(p.stands == (function(p.object) > 0.3));
            const RegimentRadar* radar = nullptr;
            for (const RegimentRadar& x : r.Def().post_regiment.radars) radar = x.id == p.object ? &x : radar;
            SC_EXPECT(radar != nullptr);
            if (radar != nullptr) SC_EXPECT(CommandPost::RadarDownAt(*radar, 0.0) == !p.stands);
            std::printf("    seed %u: %s %s (working %.2f)\n", seed, p.name.c_str(), p.stands ? "on the air" : "off the air", function(p.object));
        }
        SC_EXPECT(joined == 2);  // the P-40 and the P-19 of the theatre's regiment, both within 250 km
        SC_EXPECT(!CommandPost::RadarDownAt(r.Def().post_regiment.radars.front(), 0.0));  // the scenario's own P-19
        if (seed == 1) SC_EXPECT(function("kesmark_p40") <= 0.3 && function("ludas_p19") <= 0.3);
        if (seed == 2) SC_EXPECT(function("kesmark_p40") > 0.3);
        while (s.Runner().Time() < 300.0) s.Step(0.1);
        bool told = false;
        for (const DebriefEntry& entry : s.Runner().Log().Entries()) {
            told = told || (entry.what == "regiment net" && entry.detail.rfind("heard", 0) == 0 && entry.detail.find("out of action") != std::string::npos);
        }
        SC_EXPECT(told);
    }
}

SC_TEST("neighbours' kills (17.1): the regiment tells the battery a target a neighbour destroyed; the post wipes the mark and gives it to nobody") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_battery.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    std::string target;
    int number = 0;
    while (r.Time() < 900.0 && target.empty()) {
        r.Step(0.1);
        for (const BoardMark& m : r.PostBoard()) {
            if (m.number > 0 && !m.group_centre) {
                target = m.target_id;
                number = m.number;
            }
        }
    }
    SC_EXPECT(!target.empty());
    if (target.empty()) return;
    SC_EXPECT(!r.RegimentTellsDestroyed("no_such_target", "battery 22", 5.0));
    SC_EXPECT(r.RegimentTellsDestroyed(target, "battery 22", 5.0));
    const double told_s = r.Time();
    bool heard = false, wiped = false;
    while (r.Time() < told_s + 120.0 && !(heard && wiped)) {
        r.Step(0.1);
        for (const DebriefEntry& entry : r.Log().Entries()) {
            heard = heard || (entry.what == "regiment net" && entry.detail.rfind("heard", 0) == 0 && entry.detail.find("destroyed by battery 22") != std::string::npos);
        }
        bool on_board = false;
        for (const BoardMark& m : r.PostBoard()) on_board = on_board || m.target_id == target;
        wiped = heard && !on_board;
    }
    std::printf("    mark %04d (%s): told at %.0f s, %s, %s\n", number, target.c_str(), told_s, heard ? "heard" : "not heard",
                wiped ? "wiped" : "still on the board");
    SC_EXPECT(heard);
    SC_EXPECT(wiped);
}
