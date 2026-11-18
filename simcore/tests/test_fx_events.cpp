// The presentation feed (simcore/fx/events.hpp, docs/fx/README.md 3.1): the feed itself; our missiles' launch and end in
// a played practice shoot, the end agreeing with the crew's kill assessment; the wreck of a kill, as the shot's end drew
// it; the feed the same run to run, and the simulation the same with the feed switched off (SetFxOn); a raid's loads
// released where their weapon's own throw puts them on the aim and landing after its flight
// (simcore/air_war/weapon_flight.hpp), checked against hand calculations. The other shooters' pushes (neighbours, the
// battery, the bridge's ARMs and cruise missiles, a missile thrown off) are test_fx_bridge.cpp's. Positions and routes
// are placeholders.
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/weapon_flight.hpp"
#include "simcore/fx/events.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
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

// Plays a scenario file to `max_s`; `read` takes the feed's new events every step (as the game does); `fx` false
// switches the feed off (SetFxOn).
struct Played {
    std::string debrief;
    int hits = 0, misses = 0, launches = 0;
    std::vector<fx::Event> feed;
    std::vector<DebriefEntry> log;
    double rails_err_m = 0.0;  // the worst of our launches' heights over the ground against the rails' 3.5 m
};
Played Play(const std::string& file, double max_s, bool read, bool callouts = true, bool fx = true) {
    Played p;
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenarioFile(kData + "/scenarios/" + file, kData, def, console, &e)) {
        std::printf("    %s\n", e.c_str());
        return p;
    }
    if (!callouts) def.has_crew_data = false;
    ScenarioRunner r(std::move(def), std::move(console));
    r.SetFxOn(fx);
    std::uint64_t cursor = 0;
    while (!r.Finished() && r.Time() < max_s) {
        r.Step(0.1);
        if (!read) continue;
        for (const fx::Event& ev : r.Fx().Since(cursor)) p.feed.push_back(ev);
        cursor = r.Fx().Last();
    }
    if (!read) p.feed = r.Fx().Since(0);
    p.debrief = r.Debrief();
    const DebriefSummary s = r.Log().Summary();
    p.hits = s.hits;
    p.misses = s.misses;
    p.launches = r.Launches();
    p.log = r.Log().Entries();
    for (const fx::Event& ev : p.feed) {
        if (ev.kind != fx::EventKind::MissileLaunch) continue;
        const double agl = r.GetWorld().Frame().AltitudeOf(ev.position) - r.GroundHeightAt(ev.position.x, ev.position.y);
        p.rails_err_m = std::fmax(p.rails_err_m, std::fabs(agl - 3.5));
    }
    return p;
}

bool SameLog(const std::vector<DebriefEntry>& a, const std::vector<DebriefEntry>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].time_s != b[i].time_s || a[i].kind != b[i].kind || a[i].who != b[i].who || a[i].what != b[i].what ||
            a[i].detail != b[i].detail) {
            std::printf("    log entry %d differs: %.2f %s %s / %.2f %s %s\n", static_cast<int>(i), a[i].time_s, a[i].what.c_str(),
                        a[i].detail.c_str(), b[i].time_s, b[i].what.c_str(), b[i].detail.c_str());
            return false;
        }
    }
    return true;
}

// Our launches and ends pair by number: a MissileLaunch from the rails 3.5 m over the ground, then its MissileEnd.
void ExpectPairs(const Played& p, int& ends) {
    std::map<int, const fx::Event*> launched;
    SC_EXPECT(p.rails_err_m < 1e-6);
    for (const fx::Event& e : p.feed) {
        if (e.kind == fx::EventKind::MissileLaunch) {
            SC_EXPECT(launched.count(e.number) == 0);
            launched[e.number] = &e;
            SC_EXPECT(e.source == "own" && e.weapon == "osa");
            SC_EXPECT_NEAR(e.velocity.Length(), 30.0, 1e-9);
            SC_EXPECT_NEAR(RadToDeg(ElevationOf(e.velocity)), 27.0, 1e-9);  // voenizdat_osa_1991: it leaves at 27 degrees
            SC_EXPECT(e.end_t_s > e.t_s);
        }
        if (e.kind == fx::EventKind::MissileEnd) {
            const auto it = launched.find(e.number);
            SC_EXPECT(it != launched.end());
            if (it == launched.end()) continue;
            SC_EXPECT(it->second->seq < e.seq && it->second->target == e.target);
            ++ends;
            // A burst now, or flying on (always some time after now) to destroy itself 25-28 s after its launch.
            if (e.end == fx::BurstEnd::Wide || e.end == fx::BurstEnd::Early) {
                SC_EXPECT(e.end_t_s > e.t_s);
                SC_EXPECT(e.end_t_s >= it->second->t_s + 25.0 - 1e-9 && e.end_t_s <= std::fmax(it->second->t_s + 28.0, e.t_s + 0.5) + 1e-9);
            } else {
                SC_EXPECT(e.end_t_s == e.t_s);
            }
            launched.erase(it);
        }
    }
}

}  // namespace

// ------------------------------------------------------------------------------------------------ the feed

SC_TEST("fx: the feed numbers its events from 1, gives a reader what is new, and drops the oldest past its capacity") {
    fx::Feed f(3);
    SC_EXPECT(f.Last() == 0 && f.Since(0).empty() && f.Capacity() == 3);
    for (int i = 1; i <= 5; ++i) {
        fx::Event e;
        e.kind = fx::EventKind::WeaponImpact;
        e.number = i;
        SC_EXPECT(f.Push(e) == static_cast<std::uint64_t>(i));
    }
    SC_EXPECT(f.Last() == 5 && f.Size() == 3);
    const std::vector<fx::Event> all = f.Since(0);  // a reader that fell behind skips the dropped
    SC_EXPECT(all.size() == 3 && all[0].seq == 3 && all[0].number == 3 && all[2].seq == 5);
    const std::vector<fx::Event> late = f.Since(4);
    SC_EXPECT(late.size() == 1 && late[0].seq == 5);
    SC_EXPECT(f.Since(5).empty() && f.Since(99).empty());
    f.Clear();  // the numbers go on: a reader's cursor stays good
    SC_EXPECT(f.Size() == 0 && f.Last() == 5 && f.Since(0).empty());
    SC_EXPECT(f.Push(fx::Event{}) == 6 && f.Since(5).size() == 1 && f.Since(5)[0].seq == 6);
    SC_EXPECT(fx::Feed().Capacity() == 4096);
    SC_EXPECT(std::string(fx::ToString(fx::EventKind::GunPass)) == "gun_pass" &&
              std::string(fx::ToString(fx::BurstEnd::NearBurst)) == "near_burst");
}

// ------------------------------------------------------------------------------------------------ our missiles

SC_TEST("fx: a missile that missed flies on to destroy itself 25-28 s after launch, and never ends at the moment it missed") {
    // A miss by chance at the meeting, 12 s after a launch at 100 s: the window as it is, 125-128 s.
    SC_EXPECT_NEAR(fx::SelfDestructTime(100.0, 112.0, 0.0, 25.0, 28.0, 0.5), 125.0, 1e-12);
    SC_EXPECT_NEAR(fx::SelfDestructTime(100.0, 112.0, 0.5, 25.0, 28.0, 0.5), 126.5, 1e-12);
    SC_EXPECT_NEAR(fx::SelfDestructTime(100.0, 112.0, 1.0, 25.0, 28.0, 0.5), 128.0, 1e-12);
    // "Out of reach": decided at the end of the 9M33's 26 s of flight (data/gameplay/osa_ak.json), inside the window. The
    // draws that put it at 125-126 s (a third of them) would have ended it now, at the aircraft: it goes 126.5-128 s.
    SC_EXPECT_NEAR(fx::SelfDestructTime(100.0, 126.0, 0.0, 25.0, 28.0, 0.5), 126.5, 1e-12);
    SC_EXPECT_NEAR(fx::SelfDestructTime(100.0, 126.0, 1.0, 25.0, 28.0, 0.5), 128.0, 1e-12);
    for (int i = 0; i <= 20; ++i) SC_EXPECT(fx::SelfDestructTime(100.0, 126.0, i / 20.0, 25.0, 28.0, 0.5) > 126.0 + 0.05);
    // Decided past the window (a long flight): half a second after now, whatever the draw.
    SC_EXPECT_NEAR(fx::SelfDestructTime(100.0, 131.0, 0.0, 25.0, 28.0, 0.5), 131.5, 1e-12);
    SC_EXPECT_NEAR(fx::SelfDestructTime(100.0, 131.0, 1.0, 25.0, 28.0, 0.5), 131.5, 1e-12);
}

SC_TEST("fx: in the practice shoot each missile's launch and end are in the feed, the end the one the crew's signs show") {
    const Played p = Play("osa_practice_em.json", 700.0, true);
    SC_EXPECT(p.launches >= 1);
    int ends = 0;
    ExpectPairs(p, ends);
    const std::vector<fx::Event> launches = OfKind(p.feed, fx::EventKind::MissileLaunch);
    SC_EXPECT(static_cast<int>(launches.size()) == p.launches);
    // Each end agrees with the engagement model (HIT or MISS in the log) and with the kill assessment's draw: the sign
    // the crew sees of it ("missile N)" in the log): the burst for a hit or a near burst, the reply lost before the
    // meeting for an early burst, still there after it for a missile flying on.
    int checked = 0, downs = 0;
    const std::vector<fx::Event> down = OfKind(p.feed, fx::EventKind::AircraftDown);
    for (const fx::Event& e : OfKind(p.feed, fx::EventKind::MissileEnd)) {
        const std::string who = "Missile " + std::to_string(e.number);
        const std::string tag = "missile " + std::to_string(e.number) + ")";
        bool hit = false, miss = false, tv_fire = false;
        std::set<std::string> signs;
        for (const DebriefEntry& l : p.log) {
            hit = hit || (l.kind == DebriefEntry::Kind::Hit && l.who == who);
            miss = miss || (l.kind == DebriefEntry::Kind::Miss && l.who == who);
            if (l.detail.find(tag) == std::string::npos || l.detail.compare(0, 5, "sees ") != 0) continue;
            if (l.detail.find("the burst at the meeting") != std::string::npos) signs.insert("burst");
            if (l.detail.find("lost before the meeting") != std::string::npos) signs.insert("early");
            if (l.detail.find("still there after the meeting") != std::string::npos) signs.insert("after");
            tv_fire = tv_fire || l.detail.find("fire and break-up on the TV") != std::string::npos;
        }
        SC_EXPECT(hit != miss && hit == (e.end == fx::BurstEnd::Hit));
        SC_EXPECT(!(e.fire || e.falls) || e.end == fx::BurstEnd::Hit);  // only a hit leaves a wreck to burn or fall
        if (e.end == fx::BurstEnd::Hit) {
            ++downs;
            // The burst within the warhead's reach of the aircraft; the wreck where the aircraft was at that moment, after
            // the burst, burning and falling as the shot's end drew it: the kill assessment's draw, so the crew sees fire
            // and break-up on the TV only for a wreck that burns.
            const fx::Event* wreck = nullptr;
            for (const fx::Event& d : down) {
                if (d.target == e.target && d.seq > e.seq && wreck == nullptr) wreck = &d;
            }
            SC_EXPECT(wreck != nullptr);
            if (wreck != nullptr) {
                SC_EXPECT(wreck->source == "own" && wreck->t_s == e.t_s && wreck->world_id >= 0);
                SC_EXPECT((wreck->position - e.aim).Length() <= 5.0 + 1e-6);
                SC_EXPECT(wreck->fire == e.fire && wreck->falls == e.falls);
            }
            SC_EXPECT(!tv_fire || e.fire);
        } else {
            SC_EXPECT(!tv_fire);
        }
        if (signs.empty()) continue;  // the session ended before the crew saw anything of it
        ++checked;
        const char* want = e.end == fx::BurstEnd::Hit || e.end == fx::BurstEnd::NearBurst ? "burst"
                           : e.end == fx::BurstEnd::Early                                 ? "early"
                                                                                           : "after";
        SC_EXPECT(signs.size() == 1 && signs.count(want) == 1);
        std::printf("    missile %d: %s, the crew saw '%s'\n", e.number, fx::ToString(e.end), want);
    }
    SC_EXPECT(ends == p.launches && checked >= 1);
    SC_EXPECT(static_cast<int>(down.size()) >= downs);
    for (const fx::Event& d : down) SC_EXPECT(d.world_id >= 0 && !d.target.empty());
    std::printf("    %d launches, %d hits, %d wrecks, %d events in all\n", p.launches, p.hits, static_cast<int>(down.size()),
                static_cast<int>(p.feed.size()));
}

SC_TEST("fx: with no crew to call the signs the feed still shows every shot's end, and the wreck of a kill") {
    const Played p = Play("osa_practice.json", 900.0, true, false);
    SC_EXPECT(p.launches >= 1);
    int ends = 0;
    ExpectPairs(p, ends);
    SC_EXPECT(ends == p.launches);
    for (const fx::Event& e : OfKind(p.feed, fx::EventKind::MissileEnd)) {
        if (e.end != fx::BurstEnd::Hit) continue;
        bool wreck = false;
        for (const fx::Event& d : OfKind(p.feed, fx::EventKind::AircraftDown)) wreck = wreck || (d.target == e.target && d.seq > e.seq);
        SC_EXPECT(wreck);
    }
    std::printf("    %d launches, %d hits\n", p.launches, p.hits);
}

// ПУСК with no target track (ScenarioRunner::FxLaunch): the engagement model logs it unguided, and the feed shows it
// leaving the rails along the hull's heading at the 9M33's 27 degrees and destroying itself 25-28 s after its launch
// (voenizdat_osa_1991 p22), its end Wide, as far up its line as the missile's average speed takes it. Nobody at the
// console: the pair prepared by hand (БОРТ СПК under its guard), live-fire mode, the hatches shut, the key inserted and
// turned, then ПУСК.
SC_TEST("fx: a missile launched with no target track goes off along the hull's heading and destroys itself 25-28 s later") {
    struct NoTrack {
        std::vector<fx::Event> feed;
        std::string debrief;
        int launches = 0;
        bool armed = false, tracked = true, unguided = false;
        double heading_deg = 0.0, speed_mps = 0.0;
    };
    const auto play = [](bool fx_on) {
        NoTrack out;
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &e));
        def.has_crew_data = false;  // nobody else at the console
        def.readiness = "deployed";  // at a halt, the equipment warm: armed in seconds, the target not yet up (200 s)
        def.triggers.clear();        // no march orders
        out.speed_mps = def.tuning.missile_average_speed_mps > 0.0 ? def.tuning.missile_average_speed_mps : 500.0;
        ScenarioRunner r(std::move(def), std::move(console));
        r.SetFxOn(fx_on);
        r.Step(0.1);
        ControlPanel& panel = r.Console().panel;
        SC_EXPECT(panel.Set("fo.bort_spk_cover", 1) && panel.Set("fo.bort_spk", 1));
        SC_EXPECT(panel.Set("fo.mode", 0) && panel.Set("veh.hatches", 1));
        SC_EXPECT(panel.Set("fo.key", 1) && panel.Set("fo.key", 2));
        while (r.Time() < 30.0 && !r.Console().equipment.IsReady("launch_armed")) r.Step(0.1);
        out.armed = r.Console().equipment.IsReady("launch_armed");
        out.tracked = !r.Engagement().TrackedTarget().empty();
        SC_EXPECT(panel.Set("fo.launch", 1));
        r.Step(0.1);
        SC_EXPECT(panel.Set("fo.launch", 0));
        while (r.Time() < 70.0) r.Step(0.1);
        out.feed = r.Fx().Since(0);
        out.debrief = r.Debrief();
        out.launches = r.Launches();
        out.heading_deg = r.Engagement().UnitHeadingDeg();
        for (const DebriefEntry& l : r.Log().Entries()) {
            out.unguided = out.unguided || (l.kind == DebriefEntry::Kind::Miss && l.who == "Missile 1" &&
                                             l.detail == "launched with no target track: unguided");
        }
        return out;
    };
    const NoTrack on = play(true);
    SC_EXPECT(on.armed && !on.tracked && on.launches == 1 && on.unguided);
    const std::vector<fx::Event> launches = OfKind(on.feed, fx::EventKind::MissileLaunch);
    const std::vector<fx::Event> ends = OfKind(on.feed, fx::EventKind::MissileEnd);
    SC_EXPECT(launches.size() == 1 && ends.size() == 1);
    if (launches.size() == 1 && ends.size() == 1) {
        const fx::Event& l = launches[0];
        const fx::Event& d = ends[0];
        SC_EXPECT(l.number == 1 && l.source == "own" && l.target.empty() && l.world_id < 0 && l.weapon == "osa");
        SC_EXPECT_NEAR(l.velocity.Length(), 30.0, 1e-9);
        SC_EXPECT_NEAR(RadToDeg(ElevationOf(l.velocity)), 27.0, 1e-9);
        SC_EXPECT(std::fabs(std::remainder(RadToDeg(AzimuthOf(l.velocity)) - on.heading_deg, 360.0)) < 1e-6);
        const double flight_s = l.end_t_s - l.t_s;
        SC_EXPECT(flight_s >= 25.0 - 1e-9 && flight_s <= 28.0 + 1e-9);
        SC_EXPECT(d.end == fx::BurstEnd::Wide && d.seq > l.seq && d.number == 1 && d.t_s == l.t_s && d.end_t_s == l.end_t_s);
        SC_EXPECT(((d.aim - l.position) - l.velocity.Normalized() * (on.speed_mps * flight_s)).Length() < 1e-6);
        std::printf("    no track: missile 1 off at %.1f s, destroys itself %.2f s later %.1f km up its line\n", l.t_s, flight_s,
                    (d.aim - l.position).Length() / 1000.0);
    }
    // The feed off: nothing in it, and the session the same.
    const NoTrack off = play(false);
    SC_EXPECT(off.feed.empty() && off.debrief == on.debrief && off.launches == 1 && off.unguided);
}

SC_TEST("fx: the feed is the same run to run, read every step or once at the end") {
    const Played quiet = Play("osa_practice_em.json", 700.0, false);
    const Played read = Play("osa_practice_em.json", 700.0, true);
    SC_EXPECT(!quiet.debrief.empty() && quiet.debrief == read.debrief);
    SC_EXPECT(quiet.hits == read.hits && quiet.misses == read.misses && quiet.launches == read.launches);
    SC_EXPECT(quiet.feed.size() == read.feed.size());
    for (std::size_t i = 0; i < quiet.feed.size() && i < read.feed.size(); ++i) {
        const fx::Event& a = quiet.feed[i];
        const fx::Event& b = read.feed[i];
        SC_EXPECT(a.seq == b.seq && a.kind == b.kind && a.number == b.number && a.end == b.end && a.t_s == b.t_s);
        SC_EXPECT(a.aim.x == b.aim.x && a.aim.y == b.aim.y && a.aim.z == b.aim.z);
    }
}

// The feed changes nothing in the simulation: with it switched off (SetFxOn(false): no push and no draw of its own) the
// session is the same to the last log entry, and the feed stays empty.
SC_TEST("fx: with the feed switched off the practice shoot is the same: its debrief, log, hits, misses and launches") {
    const Played on = Play("osa_practice_em.json", 700.0, true);
    const Played off = Play("osa_practice_em.json", 700.0, true, true, false);
    SC_EXPECT(!on.feed.empty() && off.feed.empty());
    SC_EXPECT(on.launches >= 1 && !on.debrief.empty() && on.debrief == off.debrief);
    SC_EXPECT(on.hits == off.hits && on.misses == off.misses && on.launches == off.launches);
    SC_EXPECT(SameLog(on.log, off.log));
    // And with no crew (the presentation's own draws for each shot's end and wreck).
    const Played bare_on = Play("osa_practice.json", 900.0, true, false);
    const Played bare_off = Play("osa_practice.json", 900.0, true, false, false);
    SC_EXPECT(!bare_on.feed.empty() && bare_off.feed.empty() && bare_on.debrief == bare_off.debrief && SameLog(bare_on.log, bare_off.log));
    std::printf("    on and off: %d launches, %d hits, %d log entries; %d events with the feed on\n", on.launches, on.hits,
                static_cast<int>(on.log.size()), static_cast<int>(on.feed.size()));
}

// ------------------------------------------------------------------------------------------------ the weapons' flight

SC_TEST("fx: a weapon's flight from release to impact against hand calculations (weapon_flight.hpp)") {
    WeaponsDef weapons;
    std::string e;
    SC_EXPECT(LoadWeaponsFile(kData + "/air_war/weapons.json", weapons, &e));
    // Free fall from 500 m with no drag: sqrt(2 h / g) = 10.10 s; with a 500 lb bomb's drag a little more, and more
    // still at a jet's speed (the drag along the path holds the fall back).
    const double vacuum = FallTime(500.0, 0.0, 0.0, 0.0, 241.0);
    SC_EXPECT_NEAR(vacuum, std::sqrt(2.0 * 500.0 / kStandardGravity), 0.01);
    const double still = WeaponFlightTime("level", "bomb", 500.0, 0.0, 500.0);
    const double fast = WeaponFlightTime("level", "mk82", 500.0, 250.0, 1500.0, &weapons);
    SC_EXPECT(still > vacuum && still < vacuum + 0.3);
    SC_EXPECT(fast > still && fast < 11.0);
    std::printf("    500 m: %.3f s in a vacuum, %.3f s dropped still, %.3f s at 250 m/s\n", vacuum, still, fast);
    // A dive throws it down, a loft up: sooner and later than a level release from the same height.
    SC_EXPECT(WeaponFlightTime("dive", "mk82", 500.0, 250.0, 1500.0, &weapons) < fast - 3.0);
    SC_EXPECT(WeaponFlightTime("loft", "mk82", 500.0, 250.0, 1500.0, &weapons) > fast + 10.0);
    // A parachute bomb (FAB-500ShN): free fall for 1.2 s (7.06 m), then 47 m/s: 1.2 + (300 - 7.06) / 47 = 7.43 s.
    SC_EXPECT(WeaponPathOf("fab500sh", &weapons) == WeaponPath::Chute && WeaponPathOf("mk82r", &weapons) == WeaponPath::Retarded);
    const double fall_open = 0.5 * kStandardGravity * 1.2 * 1.2;
    SC_EXPECT_NEAR(WeaponFlightTime("laydown", "fab500sh", 300.0, 250.0, 400.0, &weapons), 1.2 + (300.0 - fall_open) / 47.0, 1e-9);
    // A Snakeye at 60 m: its blades hold it up longer than a slick bomb (3.5 s from 60 m).
    const double slick = WeaponFlightTime("laydown", "mk82", 60.0, 250.0, 300.0, &weapons);
    SC_EXPECT_NEAR(slick, std::sqrt(2.0 * 60.0 / kStandardGravity), 0.1);
    const double snakeye = WeaponFlightTime("laydown", "mk82r", 60.0, 250.0, 300.0, &weapons);
    SC_EXPECT(snakeye > slick + 0.1 && snakeye < slick + 2.0);
    std::printf("    60 m at 250 m/s: a slick bomb %.2f s, a Snakeye %.2f s\n", slick, snakeye);
    // A guided bomb takes 1.3 times the free fall.
    SC_EXPECT_NEAR(WeaponFlightTime("level", "gbu12", 4000.0, 240.0, 3000.0, &weapons),
                   1.3 * WeaponFlightTime("level", "mk82", 4000.0, 240.0, 3000.0, &weapons), 1e-9);
    // Rockets, missiles, anti-tank missiles and guns: their range over their average speed.
    SC_EXPECT_NEAR(WeaponFlightTime("hover_fire", "s5", 30.0, 0.0, 3000.0, &weapons), 3000.0 / 600.0, 1e-9);
    SC_EXPECT_NEAR(WeaponFlightTime("standoff", "agm65b", 1000.0, 240.0, 8000.0, &weapons), 8000.0 / 320.0, 1e-9);
    SC_EXPECT_NEAR(WeaponFlightTime("standoff", "kh29l", 1000.0, 240.0, 9000.0, &weapons), 9000.0 / 500.0, 1e-9);
    SC_EXPECT_NEAR(WeaponFlightTime("hover_fire", "tow", 15.0, 0.0, 3750.0, &weapons), 3750.0 / 200.0, 1e-9);
    SC_EXPECT_NEAR(WeaponFlightTime("dive", "gau8", 300.0, 180.0, 1800.0, &weapons), 2.0, 1e-9);
    // Clamped to 1-60 s; an unknown weapon is the caller's to time.
    SC_EXPECT(WeaponFlightTime("dive", "gau8", 30.0, 180.0, 100.0, &weapons) == 1.0);
    SC_EXPECT(WeaponFlightTime("level", "mk84", 20000.0, 250.0, 20000.0, &weapons) == 60.0);
    SC_EXPECT(WeaponFlightTime("level", "no_such_weapon", 500.0, 250.0, 500.0, &weapons) < 0.0);
    SC_EXPECT(WeaponFlightTime("level", "mk82", 500.0, 250.0, 500.0) < 0.0);  // an id with no weapons to look it up in
}

SC_TEST("fx: a falling weapon's throw over the ground, from the aircraft's own path angle, against hand calculations") {
    WeaponsDef weapons;
    std::string e;
    SC_EXPECT(LoadWeaponsFile(kData + "/air_war/weapons.json", weapons, &e));
    const double g = kStandardGravity;
    // In a vacuum: level from 500 m at 250 m/s, 250 * sqrt(2 * 500 / g) = 2524 m over the ground in 10.10 s; diving at
    // 30 degrees from 1500 m (125 m/s down), t = (-125 + sqrt(125² + 2 g 1500)) / g = 8.66 s and 216.5 m/s times that.
    SC_EXPECT_NEAR(VacuumThrow(500.0, 250.0, 0.0), 250.0 * std::sqrt(2.0 * 500.0 / g), 1e-9);
    const double t_dive = (-125.0 + std::sqrt(125.0 * 125.0 + 2.0 * g * 1500.0)) / g;
    SC_EXPECT_NEAR(VacuumThrow(1500.0, 250.0, -30.0), 250.0 * std::cos(DegToRad(30.0)) * t_dive, 1e-6);
    // The integrator with no drag agrees with them: within its 0.01 s step in time (its semi-implicit steps reach the
    // ground about 5 ms early), so within one step's travel over the ground; and with no drag the speed over the ground
    // holds, so its throw is that speed times its own time.
    const Fall still = FallPath(500.0, 250.0, 0.0, 0.0, 241.0);
    const Fall dived = FallPath(1500.0, 250.0, -30.0, 0.0, 241.0);
    SC_EXPECT_NEAR(still.t_s, std::sqrt(2.0 * 500.0 / g), 0.01);
    SC_EXPECT_NEAR(still.x_m, VacuumThrow(500.0, 250.0, 0.0), 250.0 * 0.01);
    SC_EXPECT_NEAR(still.x_m, 250.0 * still.t_s, 1e-6);
    SC_EXPECT_NEAR(dived.t_s, t_dive, 0.01);
    SC_EXPECT_NEAR(dived.x_m, VacuumThrow(1500.0, 250.0, -30.0), 250.0 * std::cos(DegToRad(30.0)) * 0.01);
    SC_EXPECT_NEAR(dived.x_m, 250.0 * std::cos(DegToRad(30.0)) * dived.t_s, 1e-6);
    // A loft at 30 degrees up from 200 m: t = (125 + sqrt(125² + 2 g 200)) / g.
    const double t_loft = (125.0 + std::sqrt(125.0 * 125.0 + 2.0 * g * 200.0)) / g;
    SC_EXPECT_NEAR(FallPath(200.0, 250.0, 30.0, 0.0, 241.0).t_s, t_loft, 0.01);
    // With the bomb's drag it goes less far and takes longer: a Mk 82 from 4.5 km at 240 m/s, and the Snakeye's blades
    // far less far again from 60 m.
    const WeaponFlight mk82 = WeaponFlightOf("level", "mk82", 4500.0, 240.0, 0.0, 7000.0, &weapons);
    const double vacuum = VacuumThrow(4500.0, 240.0, 0.0);
    SC_EXPECT(mk82.throw_m > 0.8 * vacuum && mk82.throw_m < vacuum);
    SC_EXPECT(mk82.time_s > std::sqrt(2.0 * 4500.0 / g));
    const WeaponFlight slick = WeaponFlightOf("laydown", "mk82", 60.0, 250.0, 0.0, 900.0, &weapons);
    const WeaponFlight snakeye = WeaponFlightOf("laydown", "mk82r", 60.0, 250.0, 0.0, 900.0, &weapons);
    SC_EXPECT(snakeye.throw_m < slick.throw_m - 100.0 && slick.throw_m < VacuumThrow(60.0, 250.0, 0.0));
    std::printf("    4.5 km at 240 m/s: a Mk 82 %.0f m on (%.0f m in a vacuum); 60 m at 250 m/s: slick %.0f m, Snakeye %.0f m\n",
                mk82.throw_m, vacuum, slick.throw_m, snakeye.throw_m);
    // The chute bomb: 1.2 s at 250 m/s (300 m), then ln(1 + k v τ) / k with k = g / 47² and τ its time under the chute.
    const double fall_open = 0.5 * g * 1.2 * 1.2;
    const double tau = (300.0 - fall_open) / 47.0;
    const double k = g / (47.0 * 47.0);
    const WeaponFlight chute = WeaponFlightOf("laydown", "fab500sh", 300.0, 250.0, 0.0, 400.0, &weapons);
    SC_EXPECT_NEAR(chute.throw_m, 250.0 * 1.2 + std::log1p(k * 250.0 * tau) / k, 1e-9);
    SC_EXPECT_NEAR(chute.time_s, 1.2 + tau, 1e-9);
    // Climbing at release its chute opens higher: 125 m/s up for 1.2 s less the fall, so it hangs longer.
    SC_EXPECT_NEAR(WeaponFlightOf("loft", "fab500sh", 300.0, 250.0, 30.0, 400.0, &weapons).time_s,
                   1.2 + (300.0 + 125.0 * 1.2 - fall_open) / 47.0, 1e-9);
    // A guided bomb is released on the free fall's solution: the same throw, 1.3 times the time.
    const WeaponFlight gbu = WeaponFlightOf("level", "gbu12", 4000.0, 240.0, 0.0, 3000.0, &weapons);
    const WeaponFlight dumb = WeaponFlightOf("level", "mk82", 4000.0, 240.0, 0.0, 3000.0, &weapons);
    SC_EXPECT(gbu.throw_m == dumb.throw_m && gbu.throw_m > 0.0);
    SC_EXPECT_NEAR(gbu.time_s, 1.3 * dumb.time_s, 1e-9);
    // The ones that fly to their aim have no throw; an unknown weapon neither, nor a time.
    SC_EXPECT(WeaponFlightOf("standoff", "agm65b", 1000.0, 240.0, 0.0, 8000.0, &weapons).throw_m < 0.0);
    SC_EXPECT(WeaponFlightOf("hover_fire", "s5", 30.0, 0.0, 0.0, 3000.0, &weapons).throw_m < 0.0);
    const WeaponFlight unknown = WeaponFlightOf("level", "no_such_weapon", 500.0, 250.0, 0.0, 500.0, &weapons);
    SC_EXPECT(unknown.time_s < 0.0 && unknown.throw_m < 0.0);
    // The nominal angle stays the wrapper's: WeaponFlightTime is WeaponFlightOf at DeliveryPathDeg.
    SC_EXPECT(WeaponFlightTime("dive", "mk82", 1500.0, 250.0, 2000.0, &weapons) ==
              WeaponFlightOf("dive", "mk82", 1500.0, 250.0, -40.0, 2000.0, &weapons).time_s);
    // The aircraft's state over the ground (ReleaseStateOf): its path angle from its velocity against the local up,
    // which far from the origin is not ENU z. 20 km east at 47.5 degrees north the up leans 0.18 degrees west of z.
    LocalFrame frame(GeoPoint::FromDegrees(47.5, 19.0, 100.0));
    const Vec3 at{20000.0, 0.0, 3000.0};
    GeoPoint above = frame.EnuToGeodetic(at);
    above.alt_m += 1.0;
    const Vec3 up = (frame.GeodeticToEnu(above) - at).Normalized();
    SC_EXPECT(std::fabs(RadToDeg(std::acos(up.z)) - RadToDeg(20000.0 / 6371000.0)) < 0.01);
    const Vec3 east = Cross(Vec3{0.0, 1.0, 0.0}, up).Normalized();
    const Vec3 vel = east * (250.0 * std::cos(DegToRad(-30.0))) + up * (250.0 * std::sin(DegToRad(-30.0)));
    const ReleaseState st = ReleaseStateOf(frame, at, vel, "level");
    SC_EXPECT_NEAR(st.speed_mps, 250.0, 1e-6);
    SC_EXPECT_NEAR(st.path_deg, -30.0, 1e-6);
    SC_EXPECT_NEAR(st.ground_mps, 250.0 * std::cos(DegToRad(30.0)), 1e-6);
    SC_EXPECT(Dot(st.along, east) > 1.0 - 1e-9 && Dot(st.up, up) > 1.0 - 1e-12);
    // Its ENU z alone would have the dive 0.18 degrees steeper.
    SC_EXPECT(std::fabs(RadToDeg(ElevationOf(vel)) - -30.0) > 0.1);
    // No speed: the delivery's nominal angle.
    SC_EXPECT(ReleaseStateOf(frame, at, Vec3{}, "dive").path_deg == -40.0);
}

// ------------------------------------------------------------------------------------------------ a raid's loads

SC_TEST("fx: a raid's loads are released where their throw puts them on the aim and land after their weapon's own flight") {
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    AirCampaign c;
    SC_EXPECT(c.Init(d, 3u));
    // A strike on the P-40 post 24 km north of the battery, out of its reach: two F-16s bombing level from 4.5 km and two
    // Tornados laying retarded bombs down from 60 m. Their release point is the target itself, as the planner puts a
    // level or laydown attack's (data.hpp: 0 m short): the bombs are let go a throw short of it.
    const Vec3 post = c.Truth()[static_cast<std::size_t>(d.theatre.IndexOf("kesmark_p40"))].pos;
    const Vec3 from = post + Vec3{38000.0, 46000.0, 0.0};
    const Vec3 dir = (post - from).Normalized();
    const double leg_h = (post - from).HorizontalLength() / 240.0 / 3600.0;
    Package p;
    p.id = 91;
    p.kind = MissionKind::Strike;
    p.part = DayPart::Day;
    p.target_object = "kesmark_p40";
    p.target_label = "the P-40 post";
    p.entry = "W3";
    p.exit = "W3";
    p.tot_h = 2.0;
    const auto flight = [&](const char* callsign, const char* aircraft, Profile profile, double agl, const char* weapon,
                            int count, Delivery delivery, double shift_h) {
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
        fl.route = {{from, agl, t0 - leg_h, "entry"}, {post - dir * 1000.0, agl, t0 - 1000.0 / 240.0 / 3600.0, "ip"},
                    {post, agl, t0, "target"}, {from, agl, t0 + leg_h, "exit"}};
        fl.release_index = 2;
        fl.aim_pos = post;
        return fl;
    };
    p.flights = {flight("Viper 10", "f16a", Profile::Medium, 4500.0, "mk82", 6, Delivery::Level, 0.0),
                 flight("Tonka 20", "tornado_ids", Profile::Low, 60.0, "mk82r", 8, Delivery::Laydown, 0.02)};
    RaidScenarioBuilder builder;
    const RaidScenario rs = builder.Build(c, p);
    SC_EXPECT(rs.ok);
    if (!rs.ok) {
        std::printf("    %s\n", rs.error.c_str());
        return;
    }
    for (const RaidAircraft& a : rs.manifest.aircraft) SC_EXPECT(!a.weapon.empty() && a.weapons > 0 && a.delivery != Delivery::Count);
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(rs.json_text, kData, def, console, &e));
    ScenarioRunner runner(std::move(def), std::move(console));
    MissionBridge bridge(rs.manifest, c.Data(), 3u);
    const double dt = 0.1;
    std::size_t impacts = 0;
    while (!runner.Finished() && !bridge.UnitDestroyed() && runner.Time() < rs.manifest.end_s + 60.0 && impacts < 4) {
        runner.Step(dt);
        bridge.Step(runner);
        impacts = OfKind(runner.Fx().Since(0), fx::EventKind::WeaponImpact).size();
    }
    const std::vector<fx::Event> feed = runner.Fx().Since(0);
    const std::vector<fx::Event> releases = OfKind(feed, fx::EventKind::WeaponRelease);
    SC_EXPECT(releases.size() == 4 && impacts == 4);
    const World& w = runner.GetWorld();
    const double capture = d.tuning.Get("scenario.release_capture_m");
    for (const fx::Event& r : releases) {
        const RaidAircraft* a = nullptr;
        for (const RaidAircraft& m : rs.manifest.aircraft) {
            if (m.target_id == r.source) a = &m;
        }
        SC_EXPECT(a != nullptr);
        if (a == nullptr) continue;
        // Timed by the weapon's flight from the aircraft's height over the ground where the load lands, its speed and its
        // own path angle (from its velocity against the local up), and the distance to where the load lands.
        const double agl = w.Frame().AltitudeOf(r.position) - runner.GroundHeightAt(r.aim.x, r.aim.y);
        const ReleaseState st = ReleaseStateOf(w.Frame(), r.position, r.velocity, r.delivery);
        const double flight_s =
            WeaponFlightOf(r.delivery, r.weapon, agl, st.speed_mps, st.path_deg, (r.aim - r.position).Length(), &d.weapons).time_s;
        SC_EXPECT(flight_s > 0.0);
        SC_EXPECT_NEAR(r.end_t_s - r.t_s, flight_s, 1e-9);
        SC_EXPECT_NEAR(w.Frame().AltitudeOf(r.aim), runner.GroundHeightAt(r.aim.x, r.aim.y), 0.01);
        SC_EXPECT(r.world_id >= 0);
        const bool level = r.weapon == "mk82";
        SC_EXPECT(level ? r.delivery == "level" && r.number == 6 : r.weapon == "mk82r" && r.delivery == "laydown" && r.number == 8);
        // A level release from 4.5 km takes about half a minute; a laydown from 60 m a few seconds (the fixed 15 s before).
        SC_EXPECT(level ? flight_s > 25.0 && flight_s < 40.0 : flight_s > 2.0 && flight_s < 8.0);
        // Released at the first step where the aim (the flight's, before the scatter) is no farther ahead along its track
        // than the throw from here: within one step's travel short of the throw, and within the capture of its line (the
        // flight's aircraft fly abreast and aim at one point).
        const Vec3 to_aim = runner.GroundEnu(a->aim_enu.x, a->aim_enu.y) - r.position;
        const double h = w.Frame().AltitudeOf(r.position) - runner.GroundHeightAt(a->aim_enu.x, a->aim_enu.y);
        const double throw_m = WeaponFlightOf(r.delivery, r.weapon, h, st.speed_mps, st.path_deg, to_aim.Length(), &d.weapons).throw_m;
        const double along = Dot(to_aim, st.along);
        const double up = Dot(to_aim, st.up);
        const double cross = std::sqrt(std::fmax(0.0, to_aim.LengthSquared() - along * along - up * up));
        SC_EXPECT(throw_m > 0.0 && along <= throw_m + 1e-6 && along > throw_m - st.ground_mps * dt - 10.0);
        SC_EXPECT(cross <= capture);
        // A level release lets go about 7 km short (half a minute at 240 m/s, a little less for the drag), a laydown
        // under a kilometre.
        SC_EXPECT(level ? along > 5500.0 && along < 7500.0 : along > 300.0 && along < 1000.0);
        // Its impact: after the release, at the first step past the impact time, where the release said.
        bool found = false;
        for (const fx::Event& i : OfKind(feed, fx::EventKind::WeaponImpact)) {
            if (i.source != r.source) continue;
            found = true;
            SC_EXPECT(i.seq > r.seq && i.weapon == r.weapon && i.number == r.number);
            SC_EXPECT(i.t_s >= r.end_t_s - 1e-9 && i.t_s < r.end_t_s + dt + 1e-9);
            SC_EXPECT((i.position - r.aim).Length() < 1e-6 && (i.aim - r.aim).Length() < 1e-6);
        }
        SC_EXPECT(found);
        std::printf("    %s: %d %s (%s) from %.0f m at %.0f m/s (%.1f deg), %.0f m short of the aim (throw %.0f m), %.1f s to impact\n",
                    r.source.c_str(), r.number, r.weapon.c_str(), r.delivery.c_str(), agl, st.speed_mps, st.path_deg, along, throw_m,
                    flight_s);
    }
}
