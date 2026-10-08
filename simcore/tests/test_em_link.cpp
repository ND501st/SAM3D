// The EM layer wired to the Osa scenario: loader rules, the radars driven by the console, the practice scenario on
// EM radars, and detection and lock ranges inside the bands the calibration fixes.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/em.hpp"
#include "simcore/em_link.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string ReplaceAll(std::string s, const std::string& from, const std::string& to) {
    std::size_t at = 0;
    while ((at = s.find(from, at)) != std::string::npos) {
        s.replace(at, from.size(), to);
        at += to.size();
    }
    return s;
}

const std::string& PracticeEm() {
    static const std::string text = ReadFile(kData + "/scenarios/osa_practice_em.json");
    return text;
}

struct Flight {
    bool loaded = false;
    bool ended = false;
    double first_contact_m = -1.0;  // range of the target when the search scope first showed it
    double first_lock_m = -1.0;     // range when the engagement model first held a radar track
    int launches = 0;
    int hits = 0;
    std::string debrief;
};

Flight Fly(const std::string& scenario_text, unsigned seed) {
    Flight p;
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    p.loaded = LoadScenario(scenario_text, kData, def, console, &error);
    if (!p.loaded) {
        std::printf("    load: %s\n", error.c_str());
        return p;
    }
    def.seed = seed;
    ScenarioRunner runner(std::move(def), std::move(console));
    while (!runner.Finished() && runner.Time() < 700.0) {
        runner.Step(0.1);
        const double r = runner.TargetRange("t1");
        if (r < 0.0) continue;
        if (p.first_contact_m < 0.0 && !runner.Engagement().SearchPicture().empty()) p.first_contact_m = r;
        if (p.first_lock_m < 0.0 && runner.Engagement().Method() == TrackMethod::Radar) p.first_lock_m = r;
    }
    p.ended = runner.Finished();
    p.launches = runner.Log().Summary().launches;
    p.hits = runner.Log().Summary().hits;
    p.debrief = runner.Debrief();
    return p;
}

em::RadarDef OsaRadar(const char* id) {
    em::EmLibrary lib;
    em::LoadEmLibraryFile(kData + "/em/osa_ak.json", lib);
    em::RadarDef d = lib.radars[static_cast<std::size_t>(lib.Radar(id))];
    em::Calibrate(d, em::Environment());
    return d;
}

// A bare link on flat ground at the origin, the Osa's two radars on the unit, no console. The target flies due
// south toward the unit at 150 m/s: both radars cancel anything slower than 15 m/s as clutter (MTI).
struct Bench {
    EmLinkDef def;
    FlatTerrain ground{0.0};
    std::unique_ptr<EmSensorLink> link;
    Vec3 unit{0.0, 0.0, 3.0};
    bool ok = false;
    double north_m = 0.0;
    double height_m = 0.0;
    double rcs_m2 = 3.0;

    explicit Bench(unsigned seed = 7u) {
        ok = LoadEmLinkDef(kData, "em/osa_ak.json", "osa_ak_soc", "osa_ak_ssc", def);
        link = std::make_unique<EmSensorLink>(GeoPoint::FromDegrees(46.9, 19.7, 0.0), &ground, seed, def, unit);
        ok = ok && link->Ok();
    }

    Vec3 Target() const { return {0.0, north_m, height_m}; }

    // One tick: the target moves, the radar designation (if `designate`) is put on its true position.
    void Tick(bool designate, bool alive = true) {
        north_m -= 150.0 * 0.1;
        link->SyncTarget("t", alive, Target(), {0.0, -150.0, 0.0}, rcs_m2, false);
        if (designate) {
            const Vec3 rel = Target() - unit;
            link->Designate(0.0, RadToDeg(ElevationOf(rel)), rel.Length());
        }
        link->Advance(0.1);
    }

    void Run(double seconds, bool designate) {
        for (int i = 0; i < static_cast<int>(seconds / 0.1 + 0.5); ++i) Tick(designate);
    }
};

}  // namespace

SC_TEST("em link: the scenario's em block loads; bad ones are refused with the path") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(PracticeEm(), kData, def, console, &error));
    SC_EXPECT(def.has_em && def.em.search_radar == "osa_ak_soc" && def.em.track_radar == "osa_ak_ssc");
    SC_EXPECT(def.em.library.radars.size() == 2);

    const std::string em_block =
        "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" }";
    auto bad = [&](const std::string& with, const std::string& expect) {
        const std::string text = ReplaceAll(PracticeEm(), em_block, with);
        ScenarioDef d;
        ConsoleData c;
        std::string e;
        const bool ok = LoadScenario(text, kData, d, c, &e);
        if (ok || e.find(expect) == std::string::npos) {
            std::printf("    expected \"%s\", got ok=%d \"%s\"\n", expect.c_str(), ok ? 1 : 0, e.c_str());
        }
        return !ok && e.find(expect) != std::string::npos;
    };
    SC_EXPECT(bad("\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"nope\", \"track_radar\": \"osa_ak_ssc\" }",
                  "own_unit.em: radar 'nope' is not in em/osa_ak.json"));
    SC_EXPECT(bad("\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_ssc\", \"track_radar\": \"osa_ak_ssc\" }",
                  "is not a search radar"));
    SC_EXPECT(bad("\"em\": { \"library\": \"em/missing.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" }",
                  "own_unit.em"));
    SC_EXPECT(bad("\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\" }", "own_unit.em.track_radar"));
    SC_EXPECT(bad("\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\", "
                  "\"colour\": 1 }",
                  "own_unit.em.colour: unknown key"));
    // Without the gameplay tuning there is no engagement model for the EM layer to feed.
    const std::string no_tuning = ReplaceAll(PracticeEm(), "\"tuning\": \"gameplay/osa_ak.json\",", "");
    ScenarioDef d2;
    ConsoleData c2;
    SC_EXPECT(!LoadScenario(no_tuning, kData, d2, c2, &error) && error.find("needs own_unit.tuning") != std::string::npos);
    // The plain practice scenario has no EM layer.
    ScenarioDef d3;
    ConsoleData c3;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, d3, c3, &error) && !d3.has_em);
}

SC_TEST("em link: the search radar shows a target after a few revolutions, in the range band its calibration fixes") {
    Bench b;
    SC_EXPECT(b.ok);
    b.link->SetSearch(true, false);
    b.rcs_m2 = 0.5;
    b.height_m = 1000.0;
    b.north_m = 60000.0;
    const em::RadarDef soc = OsaRadar("osa_ak_soc");
    const double r50 = em::DetectionRangeAt(soc, em::Environment(), 1000.0, 0.5, 0.5);
    const double r10 = em::DetectionRangeAt(soc, em::Environment(), 1000.0, 0.5, 0.1);
    double first_m = -1.0;
    for (int i = 0; i < 3000 && first_m < 0.0; ++i) {  // inbound from 60 km
        b.Tick(false);
        const std::vector<EmSensorLink::Contact> pic = b.link->SearchPicture();
        if (pic.empty()) continue;
        first_m = b.north_m;
        SC_EXPECT(pic.front().target_id == "t");
        SC_EXPECT_NEAR(pic.front().true_az_deg, 0.0, 2.0);                     // due north
        SC_EXPECT(std::fabs(pic.front().range_m - (b.north_m - 3.0)) < 800.0);  // measured, not exact
    }
    std::printf("    first contact at %.1f km (R50 %.1f km, R10 %.1f km)\n", first_m / 1000.0, r50 / 1000.0, r10 / 1000.0);
    SC_EXPECT(first_m > 0.75 * r50 && first_m < 1.1 * r10);
}

SC_TEST("em link: radars that are switched off show and hold nothing") {
    Bench b;
    SC_EXPECT(b.ok);
    b.height_m = 3000.0;
    b.north_m = 31000.0;
    b.Run(30.0, false);
    SC_EXPECT(b.link->SearchPicture().empty());  // nothing radiates yet
    b.link->SetSearch(true, false);
    b.link->SetTrack(true);
    b.Run(30.0, true);
    SC_EXPECT(!b.link->SearchPicture().empty());
    std::string id;
    SC_EXPECT(b.link->Locked(&id) && id == "t");
    b.link->SetSearch(false, false);
    SC_EXPECT(b.link->SearchPicture().empty());
    b.link->SetTrack(false);
    SC_EXPECT(!b.link->Locked(&id));
}

SC_TEST("em link: the tracking radar locks what its designation points at, and only that") {
    Bench b;
    SC_EXPECT(b.ok);
    b.link->SetTrack(true);
    b.height_m = 1500.0;
    b.north_m = 16000.0;
    // Designated well off to the east: it never sees the target on the northern bearing.
    bool locked_wrong = false;
    for (int i = 0; i < 60; ++i) {
        b.Tick(false);
        b.link->Designate(90.0, 5.0, b.north_m);
        locked_wrong = locked_wrong || b.link->Locked(nullptr);
    }
    SC_EXPECT(!locked_wrong);
    // Designated on it (the antenna slews round, the beam finds it): it locks and reports the right target.
    bool locked = false;
    for (int i = 0; i < 200 && !locked; ++i) {
        b.Tick(true);
        std::string id;
        EmSensorLink::Contact est;
        if (b.link->Locked(&id, &est)) {
            locked = true;
            SC_EXPECT(id == "t");
            SC_EXPECT(std::fabs(est.range_m - (b.Target() - b.unit).Length()) < 300.0 && std::fabs(est.true_az_deg) < 1.5);
        }
    }
    SC_EXPECT(locked);
    // It holds the lock while the echo keeps coming, and breaks it on command.
    b.Run(5.0, false);
    SC_EXPECT(b.link->Locked(nullptr));
    b.link->ClearLock();
    SC_EXPECT(!b.link->Locked(nullptr));
}

SC_TEST("em link: the lock is lost when the target is destroyed") {
    Bench b;
    SC_EXPECT(b.ok);
    b.link->SetTrack(true);
    b.height_m = 1500.0;
    b.north_m = 14000.0;
    b.Run(15.0, true);
    SC_EXPECT(b.link->Locked(nullptr));
    for (int i = 0; i < 20; ++i) b.Tick(false, false);  // shot down
    SC_EXPECT(!b.link->Locked(nullptr));
}

SC_TEST("em link: the range indicator shows echoes in the beam, not just in the gate") {
    Bench b;
    SC_EXPECT(b.ok);
    b.link->SetTrack(true);
    b.height_m = 1500.0;
    b.north_m = 12000.0;
    // The antenna comes round to north and up to the target; the gate is set at 6 km, short of the target.
    const double el = RadToDeg(ElevationOf(b.Target() - b.unit));
    bool shown = false;
    for (int i = 0; i < 40; ++i) {
        b.Tick(false);
        b.link->Designate(0.0, el, 6000.0);
        shown = shown || b.link->EchoVisible("t");
    }
    SC_EXPECT(shown);
    SC_EXPECT(!b.link->Locked(nullptr));  // outside the range gate: no lock
    // Aimed 20 degrees higher it does not show.
    for (int i = 0; i < 100; ++i) {
        b.link->Designate(0.0, el + 20.0, 6000.0);
        b.link->Advance(0.1);
    }
    SC_EXPECT(!b.link->EchoVisible("t"));
}

SC_TEST("em link: the tracking radar captures a 3 m2 target at 5 km inside the published band (Korovin: 23 km)") {
    // data/em/osa_ak.json fits Pd 0.5 at 23 km; the radar's own range limit is 28 km. The designation is on the
    // target's true position (a perfect search picture) so this tests the tracking radar alone. Capture happens on
    // the first echo in the gate, and Swerling I detection falls off slowly (Pd 0.24 at 28 km), so an inbound
    // target is captured between R50 and the range limit, not at R50 (docs/EM_HANDOFF.md, open question 3).
    const em::RadarDef ssc = OsaRadar("osa_ak_ssc");
    SC_EXPECT_NEAR(em::DetectionRangeAt(ssc, em::Environment(), 5000.0, 3.0, 0.5), 23000.0, 400.0);
    std::vector<double> locks;
    for (unsigned seed = 1; seed <= 12; ++seed) {
        Bench b(seed);
        SC_EXPECT(b.ok);
        b.link->SetTrack(true);
        b.height_m = 5000.0;
        b.north_m = 40000.0;
        double lock_m = -1.0;
        for (int i = 0; i < 2000 && lock_m < 0.0; ++i) {
            b.Tick(true);
            if (b.link->Locked(nullptr)) lock_m = b.north_m;
        }
        locks.push_back(lock_m);
    }
    std::sort(locks.begin(), locks.end());
    std::printf("    locks %.1f .. %.1f km (median %.1f)\n", locks.front() / 1000.0, locks.back() / 1000.0,
                locks[locks.size() / 2] / 1000.0);
    SC_EXPECT(locks.front() > 0.0);                // every seed captured
    SC_EXPECT(locks.back() <= 28100.0);            // never beyond the radar's range limit
    SC_EXPECT(locks[locks.size() / 2] > 21000.0);  // and at least as far out as the published capture range
}

SC_TEST("em practice: the Osa plays the practice scenario on EM radars, from contact to a resolved launch") {
    const Flight f = Fly(PracticeEm(), 1);
    SC_EXPECT(f.loaded && f.ended);
    SC_EXPECT(f.launches == 1);
    SC_EXPECT(f.debrief.find("SOC              event      t1  contact") != std::string::npos);
    SC_EXPECT(f.debrief.find("radar track") != std::string::npos);
    SC_EXPECT(f.debrief.find("in the zone") != std::string::npos);
    SC_EXPECT(f.debrief.find("HIT") != std::string::npos || f.debrief.find("MISS") != std::string::npos);
    // The same seed plays the same flight.
    SC_EXPECT(Fly(PracticeEm(), 1).debrief == f.debrief);
}

SC_TEST("em practice: contact and lock ranges sit in the bands the radars' calibration fixes") {
    const em::RadarDef soc = OsaRadar("osa_ak_soc");
    const em::RadarDef ssc = OsaRadar("osa_ak_ssc");
    const em::Environment env;
    // The scenario's drone: 0.5 m2 at 1 km.
    const double soc_50 = em::DetectionRangeAt(soc, env, 1000.0, 0.5, 0.5);
    const double soc_10 = em::DetectionRangeAt(soc, env, 1000.0, 0.5, 0.1);
    const double ssc_50 = em::DetectionRangeAt(ssc, env, 1000.0, 0.5, 0.5);
    const double ssc_10 = em::DetectionRangeAt(ssc, env, 1000.0, 0.5, 0.1);
    std::vector<double> contacts, locks;
    for (unsigned seed = 1; seed <= 10; ++seed) {
        const Flight f = Fly(PracticeEm(), seed);
        SC_EXPECT(f.ended && f.launches == 1);
        contacts.push_back(f.first_contact_m);
        locks.push_back(f.first_lock_m);
    }
    std::sort(contacts.begin(), contacts.end());
    std::sort(locks.begin(), locks.end());
    std::printf("    drone 0.5 m2 at 1 km: SOC R50 %.1f km R10 %.1f km, contacts %.1f..%.1f km; SSC R50 %.1f km R10 %.1f km, "
                "locks %.1f..%.1f km\n",
                soc_50 / 1000.0, soc_10 / 1000.0, contacts.front() / 1000.0, contacts.back() / 1000.0, ssc_50 / 1000.0,
                ssc_10 / 1000.0, locks.front() / 1000.0, locks.back() / 1000.0);
    // A confirmed contact needs two hits in three looks: it appears between where Pd is 0.1 and where it is high.
    SC_EXPECT(contacts.front() > 0.75 * soc_50 && contacts.back() < 1.1 * soc_10);
    // The tracking radar captures from about R50 out toward R10 (the crew sets the gate on the contact first).
    SC_EXPECT(locks.front() > 0.7 * ssc_50 && locks.back() < 1.25 * ssc_10);
    // And the search radar always shows the target before the tracking radar holds it.
    for (std::size_t i = 0; i < contacts.size(); ++i) SC_EXPECT(contacts[i] > locks[0]);
}

SC_TEST("em practice: a ridge masks the target from the EM radars; removing it brings it back") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(ReplaceAll(PracticeEm(), "\"up_m\": 1000", "\"up_m\": 300"), kData, def, console, &error));
    ScenarioRunner runner(std::move(def), std::move(console));
    SC_EXPECT(runner.Em() != nullptr);
    // A 1 km-high ridge running east-west, 5 km north of the unit (grid covers 30 x 30 km; the ground is at 110 m).
    std::vector<double> heights(61 * 61, 110.0);
    for (int col = 0; col < 61; ++col) heights[static_cast<std::size_t>(40 * 61 + col)] = 1100.0;
    const GridTerrain ridge(-15000.0, -15000.0, 500.0, 61, 61, heights);
    runner.SetTerrain(&ridge);
    bool seen = false;
    for (int i = 0; i < 2900; ++i) {  // 290 s: powered and searching, the target (from 200 s) 23-45 km out
        runner.Step(0.1);
        seen = seen || !runner.Engagement().SearchPicture().empty();
    }
    SC_EXPECT(!seen);
    runner.SetTerrain(nullptr);  // back to flat ground: the target comes into view
    for (int i = 0; i < 1200 && runner.Engagement().SearchPicture().empty(); ++i) runner.Step(0.1);
    SC_EXPECT(!runner.Engagement().SearchPicture().empty());
}

SC_TEST("em practice: the gameplay path is untouched when a scenario has no em block") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console));
    ScenarioRunner runner(std::move(def), std::move(console));
    SC_EXPECT(runner.Em() == nullptr && !runner.Engagement().EmActive());
}
