// The EM layer wired to the Osa scenario: loader rules, the radars driven by the console, the practice scenario on
// EM radars, and detection and lock ranges inside the bands the calibration fixes.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
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
    double altitude_m = -1.0;  // >= 0: fly at this true altitude (the ENU height follows the Earth's curvature)

    explicit Bench(unsigned seed = 7u) {
        ok = LoadEmLinkDef(kData, "em/osa_ak.json", "osa_ak_soc", "osa_ak_ssc", def);
        link = std::make_unique<EmSensorLink>(GeoPoint::FromDegrees(46.9, 19.7, 0.0), &ground, seed, def, unit);
        ok = ok && link->Ok();
    }

    Vec3 Target() const { return {0.0, north_m, height_m}; }

    // One tick: the target moves, the radar designation (if `designate`) is put on its true position.
    void Tick(bool designate, bool alive = true) {
        north_m -= 150.0 * 0.1;
        if (altitude_m >= 0.0) height_m = altitude_m - north_m * north_m / (2.0 * wgs84::kMeanRadius);
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

SC_TEST("em link: the auto-capture takes a 3 m2 target at 5 km near the published range (Korovin: 23 km)") {
    // data/em/osa_ak.json fits Pd 0.5 at 23 km; the radar's own range limit is 28 km. The designation is on the
    // target's true position (a perfect search picture) so this tests the tracking radar alone. The radar locks on
    // the first echo in the gate, and Swerling I detection falls off slowly (Pd 0.24 at 28 km), so that happens
    // far out; the auto-capture waits for a local Pd of about one half (22 hits in 40 looks), which is the figure Korovin
    // gives.
    const em::RadarDef ssc = OsaRadar("osa_ak_ssc");
    SC_EXPECT_NEAR(em::DetectionRangeAt(ssc, em::Environment(), 5000.0, 3.0, 0.5), 23000.0, 400.0);
    std::vector<double> locks, captures;
    for (unsigned seed = 1; seed <= 12; ++seed) {
        Bench b(seed);
        SC_EXPECT(b.ok);
        b.link->SetTrack(true);
        b.height_m = 5000.0;
        b.north_m = 40000.0;
        double lock_m = -1.0, capture_m = -1.0;
        for (int i = 0; i < 2000 && capture_m < 0.0; ++i) {
            b.Tick(true);
            if (lock_m < 0.0 && b.link->Locked(nullptr)) lock_m = b.north_m;
            if (b.link->Captured(nullptr)) capture_m = b.north_m;
        }
        locks.push_back(lock_m);
        captures.push_back(capture_m);
    }
    std::sort(locks.begin(), locks.end());
    std::sort(captures.begin(), captures.end());
    std::printf("    first lock %.1f .. %.1f km; auto-capture %.1f .. %.1f km (median %.1f)\n", locks.front() / 1000.0,
                locks.back() / 1000.0, captures.front() / 1000.0, captures.back() / 1000.0,
                captures[captures.size() / 2] / 1000.0);
    SC_EXPECT(captures.front() > 0.0);  // every seed captured
    SC_EXPECT(locks.back() <= 28100.0);  // never beyond the radar's range limit
    SC_EXPECT(captures[captures.size() / 2] > 20000.0 && captures[captures.size() / 2] < 25500.0);  // about 23 km
    SC_EXPECT(captures.back() < locks.front());  // the lock always comes first
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
    // The auto-capture takes it at about the tracking radar's R50 (the crew sets the gate on the contact first).
    SC_EXPECT(locks.front() > 0.8 * ssc_50 && locks.back() < 1.15 * ssc_50);  // the antenna now stands at its calibrated height: lobing moves the spread
    SC_EXPECT(ssc_10 > ssc_50);
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

// First range at which the search radar shows a 3 m2 target flying inbound at `altitude_m` (a negative number if it
// never does before 5 km), with the beam programme given, and the beam the echo came in.
struct FirstContact {
    double range_m = -1.0;
    int beam = -1;
};

FirstContact SearchFirst(const std::vector<int>& program, double altitude_m, unsigned seed, bool mti = false,
                         double rcs_m2 = 3.0) {
    Bench b(seed);
    b.link->SetSearch(true, mti);
    b.link->SetSearchBeamProgram(program);
    b.rcs_m2 = rcs_m2;
    b.altitude_m = altitude_m;
    b.north_m = 60000.0;
    FirstContact f;
    while (b.north_m > 5000.0 && f.range_m < 0.0) {
        b.Tick(false);
        const std::vector<EmSensorLink::Contact> pic = b.link->SearchPicture();
        if (!pic.empty()) {
            f.range_m = b.north_m;
            f.beam = pic.front().beam;
        }
    }
    return f;
}

double MedianFirst(const std::vector<int>& program, double altitude_m, int* beam = nullptr) {
    std::vector<double> r;
    for (unsigned seed = 1; seed <= 5; ++seed) {
        const FirstContact f = SearchFirst(program, altitude_m, seed);
        r.push_back(f.range_m);
        if (beam != nullptr) *beam = f.beam;
    }
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}

SC_TEST("em 6.4: the beam programme decides what the search radar sees, and the contact says which beam") {
    int beam = -1;
    // Beam I (0-4 degrees): a low or middle-height target far out, nothing high.
    SC_EXPECT(MedianFirst({0}, 1000.0, &beam) > 35000.0 && beam == 0);
    SC_EXPECT(MedianFirst({0}, 5000.0) < 0.0);
    // Beam II (4-8 degrees): a 5 km target far out, a 50 m one not at all until it is close.
    SC_EXPECT(MedianFirst({1}, 5000.0, &beam) > 35000.0 && beam == 1);
    SC_EXPECT(MedianFirst({1}, 50.0) < 0.0);
    // Beam III (8-28 degrees) is tall and weak: everything it shows is close.
    SC_EXPECT(MedianFirst({2}, 5000.0, &beam) < 28000.0 && beam == 2);
    SC_EXPECT(MedianFirst({2}, 1000.0) < 16000.0);
    // The automatic programme finds all three, each in its own beam.
    SC_EXPECT(MedianFirst({0, 1, 2}, 1000.0, &beam) > 35000.0 && beam == 0);
    SC_EXPECT(MedianFirst({0, 1, 2}, 5000.0, &beam) > 35000.0 && beam == 1);
}

SC_TEST("em 6.4: a target at 50 m is seen at the shorter range the calibration check gives (Korovin: 27 km)") {
    const em::RadarDef soc = OsaRadar("osa_ak_soc");
    const double r50 = em::DetectionRangeAt(soc, em::Environment(), 50.0, 3.0, 0.5);
    const double high = MedianFirst({0, 1, 2}, 5000.0);
    const double low = MedianFirst({0, 1, 2}, 50.0);
    std::printf("    SOC first contact %.1f km at 5 km altitude, %.1f km at 50 m (R50 at 50 m: %.1f km; published 40 and 27)\n",
                high / 1000.0, low / 1000.0, r50 / 1000.0);
    SC_EXPECT(low > 0.75 * r50 && low < 1.1 * r50);
    SC_EXPECT(low < 0.7 * high);
}

SC_TEST("em 6.4: the tracking radar captures a 50 m target at about 22 km (open item: Korovin gives 14 km)") {
    // The full-path Pd of a 50 m target is smooth: 0.5 at 22 km, 0.8 at 18 km (measured in a bench run). The E0 check
    // that reads Korovin's 14 km as a Pd 0.8 range (tests/test_em_data.cpp) only gets 14 km from a deep multipath
    // null in its own simplified path at 17 km. So a rule that fits the fitted 5 km figure also captures the 50 m
    // target at 22 km; fitting 14 km would take a rougher surface or another trials-site effect. Pinned here so a
    // change to the model or the rule shows up.
    const em::RadarDef ssc = OsaRadar("osa_ak_ssc");
    const double r50 = em::DetectionRangeAt(ssc, em::Environment(), 50.0, 3.0, 0.5);
    const double r80 = em::SustainedDetectionRange(ssc, em::Environment(), 50.0, 3.0, 0.8);
    std::vector<double> caps;
    for (unsigned seed = 1; seed <= 8; ++seed) {
        Bench b(seed);
        b.link->SetTrack(true);
        b.altitude_m = 50.0;
        b.north_m = 30000.0;
        double cap = -1.0;
        while (b.north_m > 3000.0 && cap < 0.0) {
            b.Tick(true);
            if (b.link->Captured(nullptr)) cap = b.north_m;
        }
        caps.push_back(cap);
    }
    std::sort(caps.begin(), caps.end());
    std::printf("    SSC auto-capture of a 50 m target: %.1f .. %.1f km (Pd 0.5 first %.1f km, Pd 0.8 held %.1f km; published 14)\n",
                caps.front() / 1000.0, caps.back() / 1000.0, r50 / 1000.0, r80 / 1000.0);
    SC_EXPECT_NEAR(r80, 14000.0, 2800.0);  // the check path's number (E0), kept for the record
    SC_EXPECT(caps.front() > 0.0);
    SC_EXPECT(caps[caps.size() / 2] > 0.7 * r50 && caps[caps.size() / 2] < 1.1 * r50);
}

SC_TEST("em 6.4: overhead there is a blind cone; the target comes back out of it") {
    Bench b;
    b.link->SetSearch(true, false);
    b.link->SetSearchBeamProgram({0, 1, 2});
    b.altitude_m = 4000.0;
    b.north_m = 30000.0;
    bool seen_before = false, seen_inside = false, seen_after = false;
    // Elevation 30 degrees (the search radar's limit) at 4 km is a ground distance of 6.9 km.
    while (b.north_m > -30000.0) {
        b.Tick(false);
        const bool shown = !b.link->SearchPicture().empty();
        if (b.north_m > 9000.0 && b.north_m < 14000.0) seen_before = seen_before || shown;
        if (std::fabs(b.north_m) < 4500.0) seen_inside = seen_inside || shown;
        if (b.north_m < -9000.0 && b.north_m > -14000.0) seen_after = seen_after || shown;
    }
    SC_EXPECT(seen_before && !seen_inside && seen_after);
}

SC_TEST("em 6.4: MTI brings a low target out of the ground clutter that normal video drowns it in") {
    // A target 30 m up, crossing the 4 to 12 km where the clutter is. Count the ticks the scope shows it.
    auto visible_ticks = [&](bool mti) {
        int ticks = 0;
        for (unsigned seed = 1; seed <= 4; ++seed) {
            Bench b(seed);
            b.link->SetSearch(true, mti);
            b.link->SetSearchBeamProgram({0});
            b.altitude_m = 30.0;
            b.north_m = 16000.0;
            while (b.north_m > 4000.0) {
                b.Tick(false);
                if (b.north_m < 12000.0 && !b.link->SearchPicture().empty()) ++ticks;
            }
        }
        return ticks;
    };
    const int normal = visible_ticks(false);
    const int moving = visible_ticks(true);
    std::printf("    low target in clutter: shown for %d ticks on normal video, %d with MTI%s", normal, moving, "\n");
    SC_EXPECT(moving > normal + 100);
}

SC_TEST("em 6.4: the console's beam selector, range rings and beam lamps load and bad ones are refused") {
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", console, &error));
    const SearchSensorDef& d = console.search;
    SC_EXPECT(d.beam_control == "soc.beam" && d.beam_programs.size() == 5 && d.beam_inputs.size() == 3);
    SC_EXPECT(d.beam_programs[3] == std::vector<int>({0, 1, 2}) && d.beam_programs[4] == std::vector<int>({0, 1}));
    SC_EXPECT(d.range_rings_m.size() == 3 && d.range_rings_m[0] == std::vector<double>({2000.0, 7000.0, 12000.0}));
    SC_EXPECT_NEAR(d.rotation_period_s, 1.818, 1e-9);
    const std::string text = ReadFile(kData + "/procedures/osa_ak.json");
    auto bad = [&](const std::string& from, const std::string& to, const std::string& expect) {
        ConsoleData c;
        std::string e;
        const bool ok = LoadConsole(ReplaceAll(text, from, to), c, &e);
        if (ok || e.find(expect) == std::string::npos) std::printf("    expected \"%s\", got ok=%d \"%s\"\n", expect.c_str(), ok ? 1 : 0, e.c_str());
        return !ok && e.find(expect) != std::string::npos;
    };
    SC_EXPECT(bad("[ [0], [1], [2], [0, 1, 2], [0, 1] ]", "[ [0], [1], [2], [0, 1, 2] ]", "needs one programme per position"));
    SC_EXPECT(bad("[ [0], [1], [2], [0, 1, 2], [0, 1] ]", "[ [0], [1], [2], [0, 1, 2], [0, 3] ]", "names a beam that has no input"));
    SC_EXPECT(bad("\"inputs\": [ \"search_beam_1\"", "\"inputs\": [ \"search_beam_9\"", "expected the id of an input"));
    SC_EXPECT(bad("[ [2000, 7000, 12000], ", "[ ", "needs one list per range scale"));
}

namespace {

// Plays the practice scenario to a time when the search antenna is turning, then records the search beam for a while.
std::vector<int> BeamsOver(const std::string& scenario_text, int selector_position, double seconds, std::vector<bool>* lamps_ok) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    std::vector<int> beams;
    if (!LoadScenario(scenario_text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return beams;
    }
    ScenarioRunner runner(std::move(def), std::move(console));
    while (runner.Time() < 290.0) runner.Step(0.1);  // powered, searching, the crew's procedures done
    runner.Console().panel.Set("soc.beam", selector_position);
    for (int i = 0; i < static_cast<int>(seconds / 0.1); ++i) {
        runner.Step(0.1);
        const int beam = runner.Engagement().SearchBeam();
        beams.push_back(beam);
        if (lamps_ok != nullptr) {
            bool ok = beam >= 0;
            for (int k = 0; k < 3; ++k) {
                ok = ok && runner.Console().equipment.GetInput("search_beam_" + std::to_string(k + 1)) == (k == beam);
            }
            lamps_ok->push_back(ok);
        }
    }
    return beams;
}

}  // namespace

SC_TEST("em 6.4: the beam selector holds a beam or steps through the programme, one beam per turn, and lights the lamps") {
    for (const std::string& text : {PracticeEm(), ReadFile(kData + "/scenarios/osa_practice.json")}) {
        std::vector<bool> lamps;
        const std::vector<int> fixed = BeamsOver(text, 2, 8.0, &lamps);
        SC_EXPECT(!fixed.empty());
        for (std::size_t i = 20; i < fixed.size(); ++i) SC_EXPECT(fixed[i] == 2);  // beam III held, from the next turn
        for (std::size_t i = 20; i < lamps.size(); ++i) SC_EXPECT(lamps[i]);

        const std::vector<int> lower = BeamsOver(text, 4, 16.0, nullptr);  // АВТ. I-II
        int steps = 0;
        bool saw[3] = {false, false, false};
        for (std::size_t i = 25; i < lower.size(); ++i) {  // after the programme has settled
            if (lower[i] >= 0 && lower[i] < 3) saw[lower[i]] = true;
            if (lower[i] != lower[i - 1]) ++steps;
        }
        SC_EXPECT(saw[0] && saw[1] && !saw[2]);
        SC_EXPECT(steps >= 6 && steps <= 9);  // about one step per 1.82 s turn over 14 s

        const std::vector<int> all = BeamsOver(text, 3, 16.0, nullptr);  // АВТ. I-III
        bool seen_all[3] = {false, false, false};
        for (std::size_t i = 25; i < all.size(); ++i)
            if (all[i] >= 0 && all[i] < 3) seen_all[all[i]] = true;
        SC_EXPECT(seen_all[0] && seen_all[1] && seen_all[2]);
    }
}

// ------------------------------------------------------------------------------------ review fixes (October 2026)

namespace {
// A bench whose link is built from a definition with extra libraries and scripted effects.
struct EffectsBench : Bench {
    EffectsBench(const std::vector<std::string>& extras, const std::function<void(EmLinkDef&)>& effects, unsigned seed = 7u)
        : Bench(seed) {
        EmLinkDef d;
        std::string error;
        ok = LoadEmLinkDef(kData, "em/osa_ak.json", "osa_ak_soc", "osa_ak_ssc", d, &error, extras);
        if (!ok) std::printf("    %s\n", error.c_str());
        effects(d);
        def = d;
        link = std::make_unique<EmSensorLink>(GeoPoint::FromDegrees(46.9, 19.7, 0.0), &ground, seed, def, unit);
        ok = ok && link->Ok();
    }
};
}  // namespace

SC_TEST("em link: the antenna stands at the radar's calibrated height, whatever the hull's up_m") {
    Bench b;
    SC_EXPECT(b.ok);
    SC_EXPECT_NEAR(b.link->Antenna().z, OsaRadar("osa_ak_soc").antenna_height_m, 1e-9);
    SC_EXPECT_NEAR(b.link->Antenna().x, b.unit.x, 1e-9);
}

SC_TEST("em link: chaff reaches the scope as a ghost contact that no target owns") {
    EffectsBench b({}, [](EmLinkDef& d) { d.chaff.push_back({"t", 0.0, 800.0, 120.0}); });
    SC_EXPECT(b.ok);
    b.link->SetSearch(true, false);
    b.height_m = 3000.0;
    b.north_m = 38000.0;
    bool saw_ghost = false, saw_target = false, ghost_owned = false;
    Vec3 ghost_at;
    for (int i = 0; i < 1100; ++i) {
        b.Tick(false);
        for (const EmSensorLink::Contact& c : b.link->SearchPicture()) {
            if (c.ghost) {
                saw_ghost = true;
                ghost_at = c.position;
                ghost_owned = ghost_owned || c.target_id == "t";
                SC_EXPECT(c.target_id.size() > 1 && c.target_id[0] == 'T');
            } else {
                saw_target = saw_target || c.target_id == "t";
            }
        }
    }
    SC_EXPECT(saw_ghost);
    SC_EXPECT(saw_target);
    SC_EXPECT(!ghost_owned);
    SC_EXPECT(ghost_at.y > 33000.0);  // left behind where the cloud was dropped, far from the aircraft
}

SC_TEST("em link: a stand-off noise jammer shortens the search radar's first contact") {
    double first_km[2] = {0.0, 0.0};
    for (int jam = 0; jam < 2; ++jam) {
        EffectsBench b({"em/nato_1985.json"}, [&](EmLinkDef& d) {
            if (jam == 0) return;
            EmJammerDef j;
            j.jammer_id = "nato_alq99_band8";
            j.position = {30000.0, 20000.0, 8000.0};
            d.jammers.push_back(j);
        });
        SC_EXPECT(b.ok);
        b.link->SetSearch(true, false);
        b.rcs_m2 = 3.0;
        b.height_m = 3000.0;
        b.north_m = 60000.0;
        for (int i = 0; i < 3500 && first_km[jam] == 0.0; ++i) {
            b.Tick(false);
            for (const EmSensorLink::Contact& c : b.link->SearchPicture())
                if (c.target_id == "t") first_km[jam] = b.north_m / 1000.0;
        }
    }
    std::printf("    first contact %.1f km clear, %.1f km with a stand-off jammer\n", first_km[0], first_km[1]);
    SC_EXPECT(first_km[0] > 25.0);
    SC_EXPECT(first_km[1] > 0.0 && first_km[1] < 0.8 * first_km[0]);
}

SC_TEST("em link: the unit's own radar tracks become knowledge and fuse with a report from elsewhere") {
    Bench b;
    SC_EXPECT(b.ok);
    b.link->SetSearch(true, false);
    b.height_m = 3000.0;
    b.north_m = 30000.0;
    b.Run(40.0, false);
    std::vector<em::FusedTrack> pic = b.link->UnitPicture();
    SC_EXPECT(pic.size() == 1);
    if (pic.size() != 1) return;
    SC_EXPECT(pic[0].fused.source == em::InfoSource::OwnRadar);
    SC_EXPECT(pic[0].fused.PositionSigmaM() > 0.0 && pic[0].contributors == 1);
    // A neighbour reports the same aircraft, less precisely: one fused track, never more uncertain than before.
    const double before = pic[0].fused.PositionSigmaM();
    em::KnowledgeTrack rep;
    rep.origin = {99, 1, 1};
    rep.category = em::Category::Air;
    rep.position_valid = true;
    rep.position = pic[0].fused.position;
    rep.cov = em::Mat3::Diagonal(2000.0 * 2000.0);
    rep.time_s = b.link->Time();
    b.link->ReceiveReport(rep);
    pic = b.link->UnitPicture();
    SC_EXPECT(pic.size() == 1 && pic[0].contributors == 2);
    SC_EXPECT(pic[0].fused.PositionSigmaM() <= before * 1.001);
}

SC_TEST("em link: scenario em.jammers and em.chaff load; bad ones are refused with the path") {
    const std::string em_block =
        "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" }";
    auto with = [&](const std::string& tail) {
        return ReplaceAll(PracticeEm(), em_block,
                          "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\", " + tail + " }");
    };
    auto load = [&](const std::string& text, std::string* e) {
        ScenarioDef d;
        ConsoleData c;
        return LoadScenario(text, kData, d, c, e);
    };
    std::string e;
    SC_EXPECT(load(with("\"extra_libraries\": [\"em/nato_1985.json\"], \"jammers\": [ { \"jammer\": \"nato_alq99_band8\", "
                        "\"target\": \"t1\", \"on_at_s\": 60 } ], \"chaff\": [ { \"target\": \"t1\", \"at_s\": 90, \"rcs_m2\": 200 } ]"),
                   &e));
    SC_EXPECT(!load(with("\"jammers\": [ { \"jammer\": \"nato_alq99_band8\", \"target\": \"t1\" } ]"), &e) &&
              e.find("own_unit.em.jammers[0].jammer: unknown jammer") != std::string::npos);
    SC_EXPECT(!load(with("\"extra_libraries\": [\"em/nato_1985.json\"], \"jammers\": [ { \"jammer\": \"nato_alq99_band8\" } ]"), &e) &&
              e.find("either a target") != std::string::npos);
    SC_EXPECT(!load(with("\"chaff\": [ { \"target\": \"t1\" } ]"), &e) && e.find("own_unit.em.chaff[0].at_s") != std::string::npos);
}

SC_TEST("em practice: the jamming-and-chaff variant plays to the end and shows the chaff as a ghost contact") {
    const std::string text = ReadFile(kData + "/scenarios/osa_practice_em_ew.json");
    SC_EXPECT(!text.empty());
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(text, kData, def, console, &error));
    SC_EXPECT(def.has_em && def.em.jammers.size() == 1 && def.em.chaff.size() == 1);
    def.seed = 3;
    ScenarioRunner runner(std::move(def), std::move(console));
    bool ghost_seen = false;
    for (int i = 0; i < 7000 && !runner.Finished(); ++i) {
        runner.Step(0.1);
        if (runner.Em() != nullptr)
            for (const EmSensorLink::Contact& c : runner.Em()->SearchPicture()) ghost_seen = ghost_seen || c.ghost;
    }
    SC_EXPECT(runner.Em() != nullptr && runner.Em()->Ok());
    SC_EXPECT(ghost_seen);  // the chaff cloud shows on the scope as a contact no target owns
}

SC_TEST("em link: IFF replies come from the friendly transponder's track, only while the interrogator transmits") {
    Bench b;
    SC_EXPECT(b.ok);
    b.link->SetSearch(true, false);
    double north = 28000.0;
    auto run = [&](double seconds, bool friendly, int* iff_ticks, int* contact_ticks) {
        for (int i = 0; i < static_cast<int>(seconds / 0.1); ++i) {
            north -= 0.5;  // slow, so the target stays in the same place on the scope
            b.link->SyncTarget("f", true, Vec3{0.0, north, 3000.0 - north * north / (2.0 * wgs84::kMeanRadius) + 3000.0 * 0.0}, Vec3{0.0, -5.0, 0.0},
                               50.0, friendly);
            b.link->Advance(0.1);
            for (const EmSensorLink::Contact& c : b.link->SearchPicture()) {
                if (c.target_id != "f") continue;
                ++*contact_ticks;
                *iff_ticks += c.iff ? 1 : 0;
            }
        }
    };
    int iff_a = 0, seen_a = 0, iff_b = 0, seen_b = 0;
    run(40.0, true, &iff_a, &seen_a);  // interrogator silent
    SC_EXPECT(seen_a > 20);
    SC_EXPECT(iff_a == 0);
    b.link->SetInterrogating(true);
    run(40.0, true, &iff_b, &seen_b);
    SC_EXPECT(seen_b > 20 && iff_b > seen_b / 2);
    // A target that does not carry a friendly transponder never replies.
    Bench hostile;
    hostile.link->SetSearch(true, false);
    hostile.link->SetInterrogating(true);
    int iff_h = 0, seen_h = 0;
    double n2 = 28000.0;
    for (int i = 0; i < 400; ++i) {
        n2 -= 0.5;
        hostile.link->SyncTarget("h", true, Vec3{0.0, n2, 3000.0 - n2 * n2 / (2.0 * wgs84::kMeanRadius)}, Vec3{0.0, -5.0, 0.0}, 50.0, false);
        hostile.link->Advance(0.1);
        for (const EmSensorLink::Contact& c : hostile.link->SearchPicture()) {
            ++seen_h;
            iff_h += c.iff ? 1 : 0;
        }
    }
    SC_EXPECT(seen_h > 20 && iff_h == 0);
}

SC_TEST("em link: the Osa's decoys copy its search radar and blink with it (own_unit.em.decoys)") {
    EmLinkDef def;
    SC_EXPECT(LoadEmLinkDef(kData, "em/osa_ak.json", "osa_ak_soc", "osa_ak_ssc", def));
    def.decoys.push_back({Vec3{1500.0, 800.0, 0.0}, em::DecoyMode::WithRadar});
    def.decoys.push_back({Vec3{-2000.0, 500.0, 0.0}, em::DecoyMode::WhenRadarOff});
    FlatTerrain ground{0.0};
    EmSensorLink link(GeoPoint::FromDegrees(46.9, 19.7, 0.0), &ground, 3u, def, Vec3{0.0, 0.0, 3.0});
    SC_EXPECT(link.Ok() && link.DecoyEmitters().size() == 2);
    if (!link.Ok() || link.DecoyEmitters().size() != 2) return;
    const em::EmScene& s = link.World().Scene();
    link.SetSearch(true, false);
    link.Advance(0.2);
    const em::EmitterState* with = s.FindEmitter(link.DecoyEmitters()[0]);
    const em::EmitterState* off = s.FindEmitter(link.DecoyEmitters()[1]);
    SC_EXPECT(with->radiating && !off->radiating);
    SC_EXPECT(with->def.kind == em::EmitterKind::Decoy);
    SC_EXPECT_NEAR(with->def.waveform.prf_hz, s.lib.radars[static_cast<std::size_t>(s.lib.Radar("osa_ak_soc"))].emitter.waveform.prf_hz, 1e-9);
    link.SetSearch(false, false);
    link.Advance(0.2);
    SC_EXPECT(!s.FindEmitter(link.DecoyEmitters()[0])->radiating && s.FindEmitter(link.DecoyEmitters()[1])->radiating);
}
