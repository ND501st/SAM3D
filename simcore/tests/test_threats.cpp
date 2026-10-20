// Roadmap phase 9, threats: noise jamming in the engagement (the tracking radar on the jammer in angle, the range by
// hand, method Н and the fuse), the crossing parameter of the firing zone, and how targets fly (terrain following, low
// strike runs, pop-up helicopters, evasion on launch or lock, masking). Target numbers are placeholders.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/em_link.hpp"
#include "simcore/engagement.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/target_behaviour.hpp"
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

// The Osa's two radars on a bare link over flat ground, the target flying due south at 150 m/s, a self-screening Ku-band
// noise pod on it (PLACEHOLDER, data/em/generic.json) that comes on at `jam_on_s`.
struct JamBench {
    EmLinkDef def;
    FlatTerrain ground{0.0};
    std::unique_ptr<EmSensorLink> link;
    Vec3 unit{0.0, 0.0, 3.0};
    bool ok = false;
    double north_m = 20000.0;
    double height_m = 1500.0;

    explicit JamBench(double jam_on_s, double jam_off_s = -1.0, unsigned seed = 7u) {
        ok = LoadEmLinkDef(kData, "em/osa_ak.json", "osa_ak_soc", "osa_ak_ssc", def, nullptr, {"em/generic.json"});
        EmJammerDef j;
        j.jammer_id = "generic_selfscreen_noise_ku";
        j.target_id = "t";
        j.on_at_s = jam_on_s;
        j.off_at_s = jam_off_s;
        def.jammers.push_back(j);
        link = std::make_unique<EmSensorLink>(GeoPoint::FromDegrees(46.9, 19.7, 0.0), &ground, seed, def, unit);
        ok = ok && link->Ok();
        link->SetTrack(true);
    }
    Vec3 Target() const { return {0.0, north_m, height_m}; }
    double Range() const { return (Target() - unit).Length(); }
    // One tick; `designate`: the wheels on the target (azimuth, elevation and the range by hand).
    void Tick(bool designate) {
        north_m -= 150.0 * 0.1;
        link->SyncTarget("t", true, Target(), {0.0, -150.0, 0.0}, 5.0, false);
        if (designate) {
            const Vec3 rel = Target() - unit;
            link->Designate(0.0, RadToDeg(ElevationOf(rel)), rel.Length());
        }
        link->Advance(0.1);
    }
};

// The gameplay Osa scenario of test_engagement_model.cpp: halted, powered up and deployed by its NPC crew of five.
std::string Engage(const std::string& target, const std::string& triggers = "", unsigned seed = 1) {
    return R"({
      "scenario_id": "e", "name": "E", "source": "sam3d", "confidence": "estimate", "seed": )" +
           std::to_string(seed) + R"(,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 },
      "terrain": { "type": "flat", "height_m": 0 },
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [ )" +
           target + R"( ],
      "triggers": [
        { "id": "up", "when": {}, "do": [ { "order": "power_up", "crew": "Driver" },
                                           { "queue": "pto_on_line", "crew": "Driver" },
                                           { "order": "equipment_on", "crew": "Operator" },
                                           { "queue": "deploy_at_halt", "crew": "Operator" },
                                           { "order": "search_on_march", "crew": "Senior operator" } ] } )" +
           triggers + R"( ],
      "end": { "time_limit_s": 700 } })";
}

const char* kAcquireAndFire = R"(,
        { "id": "acq", "when": { "conditions": [ { "subsystem": "soc_transmitter", "is": "ready" } ] },
          "do": [ { "order": "acquire_with_soc", "crew": "Senior operator" }, { "order": "ready_capture", "crew": "Deputy" },
                  { "order": "capture_with_ssc", "crew": "Operator" },
                  { "queue": "prepare_firing", "crew": "Deputy" }, { "queue": "launch", "crew": "Deputy" } ] })";

const char* kEmconTv = R"(,
        { "id": "tv", "when": { "target": "t1", "target_within_m": 30000, "conditions": [ { "subsystem": "apu_free", "is": "ready" } ] },
          "do": [ { "order": "emcon_tv_azimuth", "crew": "Senior operator" },
                  { "order": "emcon_tv_engagement", "crew": "Deputy" } ] })";

std::string Target(double north_m, double up_m, const std::string& extra = "") {
    return R"({ "id": "t1", "name": "T", "class": "drone", "start": { "east_m": 1500, "north_m": )" +
           std::to_string(north_m) + R"(, "up_m": )" + std::to_string(up_m) +
           R"( }, "waypoints": [ { "east_m": 1500, "north_m": -20000, "up_m": )" + std::to_string(up_m) +
           R"( } ], "speed_mps": 150, "appear_at_s": 230)" + extra + " }";
}

struct Run {
    std::string debrief;
    DebriefSummary summary;
    bool loaded = false;
};

// `edit` may change the console before the run (e.g. take a step out of a drill).
template <typename Edit>
Run PlayWith(const std::string& text, Edit edit, double max_s = 700.0) {
    Run r;
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    r.loaded = LoadScenario(text, kData, def, console, &error);
    if (!r.loaded) {
        std::printf("    load: %s\n", error.c_str());
        return r;
    }
    edit(console);
    ScenarioRunner runner(std::move(def), std::move(console));
    runner.RunToEnd(0.1, max_s);
    r.debrief = runner.Debrief();
    r.summary = runner.Log().Summary();
    return r;
}
Run Play(const std::string& text, double max_s = 700.0) {
    return PlayWith(text, [](ConsoleData&) {}, max_s);
}

bool Has(const Run& r, const std::string& text) {
    const bool found = r.debrief.find(text) != std::string::npos;
    if (!found) std::printf("    missing \"%s\" in:\n%s\n", text.c_str(), r.debrief.c_str());
    return found;
}

// The EM practice scenario with the Ku-band pod on its drone from `on_s`.
std::string PracticeEmJammed(double on_s) {
    const std::string em_block =
        "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" }";
    const std::string jammed =
        "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\", "
        "\"extra_libraries\": [\"em/generic.json\"], "
        "\"jammers\": [ { \"jammer\": \"generic_selfscreen_noise_ku\", \"target\": \"t1\", \"on_at_s\": " +
        std::to_string(on_s) + " } ] }";
    return ReplaceAll(ReadFile(kData + "/scenarios/osa_practice_em.json"), em_block, jammed);
}

// A pilot flying an aircraft over `terrain` for `seconds`, calling `each` after every 0.1 s step.
template <typename Each>
void Fly(TargetPilot& pilot, Aircraft& a, const Terrain& terrain, const LocalFrame& frame, double& t, double seconds, Each each) {
    for (int i = 0; i < static_cast<int>(seconds / 0.1 + 0.5); ++i) {
        t += 0.1;
        pilot.BeforeStep(a);
        StepAircraft(a, 0.1);
        const std::string ev = pilot.AfterStep(a, terrain, frame, 0.1, t);
        each(ev);
    }
}

double Agl(const Aircraft& a, const Terrain& terrain, const LocalFrame& frame) {
    return frame.AltitudeOf(a.position) - terrain.HeightAt(a.position.x, a.position.y);
}

}  // namespace

// ------------------------------------------------------------------------------------------ 9.1 jamming

SC_TEST("threats 9.1: a self-screening noise pod drowns the SSC's echo; the radar follows the jammer in angle, no range") {
    JamBench b(15.0);
    SC_EXPECT(b.ok);
    for (int i = 0; i < 140; ++i) b.Tick(true);  // 14 s: locked, the pod still off
    std::string id;
    SC_EXPECT(b.link->Locked(&id) && id == "t");
    SC_EXPECT(!b.link->JamTracked(nullptr));
    for (int i = 0; i < 40; ++i) b.Tick(false);  // the pod comes on at 15 s; nobody touches the wheels
    SC_EXPECT(!b.link->Locked(nullptr));
    EmSensorLink::Contact c;
    SC_EXPECT(b.link->JamTracked(&id, &c) && id == "t");
    SC_EXPECT(c.no_range && c.range_m == 0.0);
    SC_EXPECT(std::fabs(c.true_az_deg) < 1.0);
    SC_EXPECT_NEAR(c.el_deg, RadToDeg(ElevationOf(b.Target() - b.unit)), 1.0);
    std::printf("    on the jammer at %.1f km: J/N %.0f dB, azimuth %.2f deg\n", b.Range() / 1000.0, b.link->JamToNoiseDb(), c.true_az_deg);
    // The antenna follows it: 20 s later it is still on the jammer, pointing at the target.
    for (int i = 0; i < 200; ++i) b.Tick(false);
    SC_EXPECT(b.link->JamTracked(&id) && id == "t");
    SC_EXPECT(std::fabs(b.link->AntennaAzimuthDeg()) < 1.0 || std::fabs(b.link->AntennaAzimuthDeg() - 360.0) < 1.0);
}

SC_TEST("threats 9.1: closing in, the echo burns through the jamming and the radar has range again") {
    JamBench b(0.0);
    SC_EXPECT(b.ok);
    double burn_m = -1.0;
    bool jam_seen = false;
    for (int i = 0; i < 1300 && burn_m < 0.0; ++i) {  // in from 20 km, the range wheel on the target (by hand)
        b.Tick(true);
        jam_seen = jam_seen || b.link->JamTracked(nullptr);
        if (jam_seen && b.link->Locked(nullptr)) burn_m = b.Range();
    }
    std::printf("    burn-through at %.1f km\n", burn_m / 1000.0);
    SC_EXPECT(jam_seen);
    SC_EXPECT(burn_m > 1500.0 && burn_m < 15000.0);
    SC_EXPECT(!b.link->JamTracked(nullptr));  // the lock is the echo's again
}

SC_TEST("threats 9.1: when the pod falls silent the radar has no jamming to follow; designated again it locks the echo") {
    JamBench b(0.0, 20.0);
    SC_EXPECT(b.ok);
    for (int i = 0; i < 150; ++i) b.Tick(true);
    SC_EXPECT(b.link->JamTracked(nullptr));
    for (int i = 0; i < 60; ++i) b.Tick(false);  // the pod goes off at 20 s, nobody designates
    SC_EXPECT(!b.link->JamTracked(nullptr));
    bool locked = false;
    for (int i = 0; i < 100 && !locked; ++i) {
        b.Tick(true);
        locked = b.link->Locked(nullptr);
    }
    SC_EXPECT(locked);
}

SC_TEST("threats 9.1: jammed after the lock, the crew holds the target in angle, takes the range off the search scope and fires") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(PracticeEmJammed(420.0), kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner runner(std::move(def), std::move(console));
    bool on_jam = false;
    while (!runner.Finished() && runner.Time() < 700.0) {
        runner.Step(0.1);
        on_jam = on_jam || runner.Engagement().Method() == TrackMethod::RadarOnJam;
    }
    const std::string d = runner.Debrief();
    SC_EXPECT(on_jam);
    SC_EXPECT(d.find("range lost in the jamming") != std::string::npos);
    SC_EXPECT(runner.Log().Summary().launches >= 1);
    SC_EXPECT(d.find("NO RANGE") == std::string::npos);  // the range came by hand from the search scope
    SC_EXPECT(d.find("HIT") != std::string::npos || d.find("MISS") != std::string::npos);
    if (!on_jam || runner.Log().Summary().launches < 1 || d.find("range lost in the jamming") == std::string::npos) {
        std::printf("%s\n", d.c_str());
    }
}

SC_TEST("threats 9.1: jammed from the start, designating onto the strobe gives the radar the jammer in angle") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(PracticeEmJammed(0.0), kData, def, console));
    ScenarioRunner runner(std::move(def), std::move(console));
    bool on_jam = false;
    while (!runner.Finished() && runner.Time() < 700.0 && !on_jam) {
        runner.Step(0.1);
        on_jam = runner.Engagement().Method() == TrackMethod::RadarOnJam;
    }
    SC_EXPECT(on_jam);
    SC_EXPECT(runner.Debrief().find("radar on the jammer track") != std::string::npos);
}

SC_TEST("threats 9.1: fired with no range, ПОМЕХА makes it method Н; without ПОМЕХА the fuse is never armed") {
    // The TV-only drill: radars silent, so no range at all.
    const Run with = Play(Engage(Target(12000, 800), kEmconTv));
    SC_EXPECT(with.summary.launches == 1);
    SC_EXPECT(Has(with, "NO RANGE (method N)"));
    SC_EXPECT(Has(with, "x method N 0.75"));
    // The same drill with the ПОМЕХА step left out.
    const Run without = PlayWith(Engage(Target(12000, 800), kEmconTv), [](ConsoleData& c) {
        for (Procedure& p : c.procedures) {
            if (p.id != "emcon_tv_engagement") continue;
            p.steps.erase(std::remove_if(p.steps.begin(), p.steps.end(),
                                         [](const ProcedureStep& s) { return s.control_id == "fo.pomekha"; }),
                          p.steps.end());
        }
    });
    SC_EXPECT(without.summary.launches == 1 && without.summary.hits == 0);
    SC_EXPECT(Has(without, "NO RANGE, ПОМЕХА off"));
    SC_EXPECT(Has(without, "x fuse 0.00 (no range, ПОМЕХА off)"));
}

SC_TEST("zones: the crossing parameter limits the head-on zone (6 km at 300 m/s, 4 km at 500 m/s)") {
    EngagementTuning t;
    std::string error;
    SC_EXPECT(LoadEngagementTuningFile(kData + "/gameplay/osa_ak.json", t, &error));
    using Z = EngagementTuning::ZoneKind;
    SC_EXPECT(t.ZoneFor(8000.0, 1000.0, 250.0, 100.0, 5500.0) == Z::HeadOn);
    SC_EXPECT(t.ZoneFor(8000.0, 1000.0, 250.0, 100.0, 6500.0) == Z::None);
    SC_EXPECT(t.ZoneFor(8000.0, 1000.0, 450.0, 300.0, 3500.0) == Z::HeadOn);
    SC_EXPECT(t.ZoneFor(8000.0, 1000.0, 450.0, 300.0, 4500.0) == Z::None);
    SC_EXPECT(t.ZoneFor(8000.0, 1000.0, 250.0, 100.0) == Z::HeadOn);  // parameter unknown: not checked
    // A target whose course passes 8 km to the side never lights ЦЕЛЬ В ЗОНЕ, though it comes within 10 km: beyond the
    // head-on band's 6 km, and never within the receding edge (8700 - 8 x 150 = 7.5 km, voenizdat_osa_1991 eq. 2.19).
    // At 7 km aside it would, briefly: 1 km before abeam the missile meets it 1.2 km past, 7.17 km out, receding.
    SC_EXPECT(t.LaunchZoneFor(Vec3{7000.0, 1000.0, 1000.0}, Vec3{0.0, -150.0, 0.0}, 1000.0) == Z::Pursuit);
    SC_EXPECT(t.LaunchZoneFor(Vec3{8000.0, 1000.0, 1000.0}, Vec3{0.0, -150.0, 0.0}, 1000.0) == Z::None);  // 8.2 km out
    const std::string wide = R"({ "id": "t1", "name": "T", "class": "drone", "start": { "east_m": 8000, "north_m": 24000, "up_m": 1000 },
        "waypoints": [ { "east_m": 8000, "north_m": -20000, "up_m": 1000 } ], "speed_mps": 150, "appear_at_s": 230 })";
    const Run r = Play(Engage(wide, kAcquireAndFire));
    SC_EXPECT(r.summary.launches == 0);
    SC_EXPECT(Has(r, "radar track"));
    // Loader: a negative limit is refused with its path.
    std::string text = ReadFile(kData + "/gameplay/osa_ak.json");
    text = ReplaceAll(text, "\"max_parameter_m\": 6000", "\"max_parameter_m\": -1");
    SC_EXPECT(!LoadEngagementTuning(text, t, &error) && error.find("zones.head_on[0].max_parameter_m") != std::string::npos);
}

SC_TEST("tuning: method factors and the no-range fuse load; unknown methods are refused") {
    EngagementTuning t;
    std::string error;
    SC_EXPECT(LoadEngagementTuningFile(kData + "/gameplay/osa_ak.json", t, &error));
    SC_EXPECT_NEAR(t.MethodFactor("N"), 0.75, 1e-9);
    SC_EXPECT_NEAR(t.MethodFactor("T/T"), 1.0, 1e-9);
    SC_EXPECT_NEAR(t.no_range_fuse_factor, 0.0, 1e-9);
    std::string text = ReadFile(kData + "/gameplay/osa_ak.json");
    SC_EXPECT(!LoadEngagementTuning(ReplaceAll(text, "{ \"N\": 0.75 }", "{ \"X\": 0.75 }"), t, &error) &&
              error.find("outcome.method_factor.X") != std::string::npos);
    SC_EXPECT(!LoadEngagementTuning(ReplaceAll(text, "{ \"N\": 0.75 }", "{ \"N\": 1.5 }"), t, &error) &&
              error.find("outcome.method_factor.N") != std::string::npos);
}

// ------------------------------------------------------------------------------------------ 9.3 target behaviour

SC_TEST("threats 9.3: terrain following holds its height over a ridge, climbing early for the crest") {
    // A 300 m ridge running east-west 8 km north of the start, 1 km wide.
    std::vector<double> h(41 * 41, 0.0);
    for (int col = 0; col < 41; ++col) {
        for (int row = 0; row < 41; ++row) {
            const double north = -2000.0 + row * 500.0;
            if (std::fabs(north - 8000.0) <= 500.0) h[static_cast<std::size_t>(row * 41 + col)] = 300.0;
        }
    }
    const GridTerrain ridge(-10000.0, -2000.0, 500.0, 41, 41, h);
    const LocalFrame frame(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    TargetBehaviourDef b;
    b.kind = TargetBehaviourDef::Kind::TerrainFollowing;
    b.agl_m = 60.0;
    b.max_climb_mps = 40.0;
    TargetPilot pilot(b, EvadeDef());
    Aircraft a;
    a.position = {0.0, 0.0, 60.0};
    a.speed_mps = 200.0;
    a.waypoints = {{0.0, 16000.0, 60.0}};
    a.velocity = {0.0, 200.0, 0.0};
    double t = 0.0, min_clear = 1e9, max_agl_flat = 0.0, agl_at_crest = 0.0;
    Fly(pilot, a, ridge, frame, t, 75.0, [&](const std::string&) {
        const double agl = Agl(a, ridge, frame);
        min_clear = std::min(min_clear, agl);
        if (a.position.y < 4000.0) max_agl_flat = std::max(max_agl_flat, agl);
        if (std::fabs(a.position.y - 8000.0) < 100.0) agl_at_crest = agl;
    });
    std::printf("    lowest %.0f m above the ground; at the crest %.0f m; on the flat up to %.0f m\n", min_clear, agl_at_crest, max_agl_flat);
    SC_EXPECT(min_clear > 30.0);           // never close to flying into the ridge
    SC_EXPECT(agl_at_crest > 40.0);        // over the top at about its height
    SC_EXPECT(max_agl_flat < 75.0);        // and low on the flat
    SC_EXPECT(std::fabs(Agl(a, ridge, frame) - 60.0) < 5.0);  // back down on the far side
}

SC_TEST("threats 9.3: a low strike run follows its waypoints' heights: low ingress, pull-up, low egress") {
    const FlatTerrain flat(100.0);
    const LocalFrame frame(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    TargetBehaviourDef b;
    b.kind = TargetBehaviourDef::Kind::TerrainFollowing;
    b.max_climb_mps = 60.0;
    b.waypoint_agl_m = {50.0, 600.0, 50.0};
    TargetPilot pilot(b, EvadeDef());
    Aircraft a;
    a.position = {0.0, -20000.0, 150.0};
    a.speed_mps = 250.0;
    a.velocity = {0.0, 250.0, 0.0};
    a.waypoints = {{0.0, -6000.0, 0.0}, {0.0, -1000.0, 0.0}, {0.0, 20000.0, 0.0}};
    double t = 0.0, top = 0.0, max_pitch_rate_deg_s = 0.0;
    Vec3 last = a.velocity;
    std::vector<double> agl_at;  // 10 km out and 8 km past
    Fly(pilot, a, flat, frame, t, 140.0, [&](const std::string&) {
        const double y = a.position.y;
        if ((agl_at.empty() && y > -10000.0) || (agl_at.size() == 1 && y > 9000.0)) agl_at.push_back(Agl(a, flat, frame));
        top = std::max(top, Agl(a, flat, frame));
        max_pitch_rate_deg_s = std::max(max_pitch_rate_deg_s, RadToDeg(AngleBetween(last, a.velocity)) / 0.1);
        last = a.velocity;
    });
    SC_EXPECT(agl_at.size() == 2);
    if (agl_at.size() == 2) {
        std::printf("    ingress %.0f m, top of the pull-up %.0f m, egress %.0f m; the flight path turns up to %.1f deg/s\n", agl_at[0],
                    top, agl_at[1], max_pitch_rate_deg_s);
        SC_EXPECT(std::fabs(agl_at[0] - 50.0) < 5.0);
        SC_EXPECT(std::fabs(top - 600.0) < 15.0);
        SC_EXPECT(std::fabs(agl_at[1] - 50.0) < 5.0);
    }
    // The pull-up eases in and out (15 m/s2 at 250 m/s: 3.4 deg/s), not a jump in the vertical speed that would read as a
    // hard manoeuvre in the outcome (before: 60 m/s of climb in one step, over 100 deg/s).
    SC_EXPECT(max_pitch_rate_deg_s < 4.0);
}

SC_TEST("aircraft: a waypoint inside its turn circle is passed for the next, not circled for ever") {
    Aircraft a;
    a.position = {0.0, 0.0, 1000.0};
    a.speed_mps = 230.0;
    a.velocity = {0.0, 230.0, 0.0};
    a.max_turn_rate_rad_s = DegToRad(6.0);  // a turn radius of 2.2 km
    // 1.1 km to the right and 0.5 km ahead: inside the right-hand turn circle (its centre 2.2 km to the right).
    a.waypoints = {{1100.0, 500.0, 1000.0}, {20000.0, 0.0, 1000.0}};
    for (int i = 0; i < 600; ++i) StepAircraft(a, 0.1);
    std::printf("    after 60 s: waypoint %zu, at (%.0f, %.0f)\n", a.next_waypoint, a.position.x, a.position.y);
    SC_EXPECT(a.next_waypoint >= 1);
    SC_EXPECT(a.position.x > 8000.0);  // on its way to the next one
    // One straight ahead is still flown to.
    Aircraft b;
    b.position = {0.0, 0.0, 1000.0};
    b.speed_mps = 230.0;
    b.velocity = {0.0, 230.0, 0.0};
    b.waypoints = {{0.0, 3000.0, 1000.0}};
    for (int i = 0; i < 120; ++i) StepAircraft(b, 0.1);
    SC_EXPECT(b.next_waypoint == 1 && std::fabs(b.position.x) < 1.0);
}

SC_TEST("threats 9.3: a pop-up helicopter hides, pops up, drops back and flies on") {
    const FlatTerrain flat(0.0);
    const LocalFrame frame(GeoPoint::FromDegrees(45.0, 25.0, 0.0));
    TargetBehaviourDef b;
    b.kind = TargetBehaviourDef::Kind::PopUp;
    b.hide_agl_m = 5.0;
    b.popup_agl_m = 25.0;
    b.max_climb_mps = 5.0;
    b.first_hold_s = 20.0;
    b.expose_s = 30.0;
    b.hidden_s = 40.0;
    b.cycles = 2;
    TargetPilot pilot(b, EvadeDef());
    Aircraft a;
    a.position = {0.0, 10000.0, 5.0};
    a.speed_mps = 60.0;
    a.velocity = {0.0, -60.0, 0.0};
    a.waypoints = {{0.0, 7000.0, 5.0}, {5000.0, 7000.0, 5.0}};
    pilot.OnSpawn(a);
    std::vector<std::string> events;
    double t = 0.0, top = 0.0, hover_speed = 1e9;
    Fly(pilot, a, flat, frame, t, 400.0, [&](const std::string& ev) {
        if (!ev.empty()) events.push_back(ev);
        top = std::max(top, Agl(a, flat, frame));
        if (pilot.PopUpPhase() == TargetPilot::Phase::Exposed) hover_speed = std::min(hover_speed, std::hypot(a.velocity.x, a.velocity.y));
    });
    std::string all;
    for (const std::string& e : events) all += e + "; ";
    std::printf("    %s\n", all.c_str());
    const std::vector<std::string> want = {"hovers behind cover", "pops up", "drops back behind cover", "pops up",
                                           "drops back behind cover", "flies on, low"};
    SC_EXPECT(events == want);
    SC_EXPECT_NEAR(top, 25.0, 1.0);
    SC_EXPECT(hover_speed < 1e-6);  // hovering while exposed
    SC_EXPECT(pilot.PopUpPhase() == TargetPilot::Phase::Egress && a.position.x > 1000.0);
    SC_EXPECT(Agl(a, flat, frame) < 7.0);  // low again for the egress
}

SC_TEST("threats 9.3: behind a ridge the hiding helicopter is masked; popping up it shows on the search scope") {
    // A 120 m ridge 4 km north of the unit; the helicopter hides 6 km north at 5 m and pops up to 200 m.
    const std::string heli = R"({ "id": "t1", "name": "Helicopter", "class": "helicopter", "type": "Air+Rotorcraft",
        "start": { "east_m": 0, "north_m": 9000, "up_m": 5 },
        "waypoints": [ { "east_m": 0, "north_m": 6000, "agl_m": 5 }, { "east_m": 8000, "north_m": 6000, "agl_m": 5 } ],
        "speed_mps": 50, "appear_at_s": 200,
        "behaviour": { "kind": "pop_up", "hide_agl_m": 5, "popup_agl_m": 200, "max_climb_mps": 10, "first_hold_s": 60,
                       "expose_s": 60, "hidden_s": 60, "cycles": 1 } })";
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(Engage(heli), kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner runner(std::move(def), std::move(console));
    std::vector<double> h(61 * 61, 0.0);
    for (int col = 0; col < 61; ++col) h[static_cast<std::size_t>(38 * 61 + col)] = 120.0;  // north = -15000 + 38 * 500 = 4000
    const GridTerrain ridge(-15000.0, -15000.0, 500.0, 61, 61, h);
    runner.SetTerrain(&ridge);
    bool seen_hiding = false, seen_up = false;
    while (runner.Time() < 450.0) {
        runner.Step(0.1);
        const bool seen = !runner.Engagement().SearchPicture().empty();
        const std::string d = runner.Debrief();
        const bool up = d.find("pops up") != std::string::npos && d.find("drops back behind cover") == std::string::npos;
        const bool hiding = d.find("hovers behind cover") != std::string::npos && d.find("pops up") == std::string::npos;
        if (hiding) seen_hiding = seen_hiding || seen;
        if (up) seen_up = seen_up || seen;
    }
    SC_EXPECT(!seen_hiding);
    SC_EXPECT(seen_up);
}

SC_TEST("threats 9.3: warned by the radar lock, a target breaks and dives; a TV track gives it no warning") {
    const std::string dive = R"(, "evade": { "on": "lock", "kind": "dive", "dive_agl_m": 40, "turn_rate_deg_s": 10 })";
    const Run r = Play(Engage(Target(24000, 1000, dive), kAcquireAndFire), 450.0);
    SC_EXPECT(Has(r, "radar track"));
    SC_EXPECT(Has(r, "breaks onto a beam heading and dives for the ground"));
    // Tracked by the TV alone it never hears a thing.
    const Run tv = Play(Engage(Target(12000, 800, dive), kEmconTv));
    SC_EXPECT(Has(tv, "TV track"));
    SC_EXPECT(tv.debrief.find("dives for the ground") == std::string::npos);
}

SC_TEST("threats 9.3: a jinking target turns hard both ways, which costs hit chance, then resumes its route") {
    const std::string jink = R"(, "evade": { "on": "launch", "kind": "jink", "turn_rate_deg_s": 15, "jink_period_s": 3, "evade_s": 15 })";
    const Run r = Play(Engage(Target(24000, 1000, jink), kAcquireAndFire));
    SC_EXPECT(Has(r, "jinks (hard turns left and right)"));
    SC_EXPECT(r.summary.launches >= 1);
    SC_EXPECT(Has(r, "manoeuvre 0.40 (15 deg/s)"));
}

SC_TEST("threats 9.3: the loader checks behaviour, evade and waypoint heights with paths") {
    auto bad = [&](const std::string& extra, const std::string& expect, const std::string& wps = "") {
        std::string tgt = Target(20000, 1000, extra);
        if (!wps.empty()) tgt = ReplaceAll(tgt, R"([ { "east_m": 1500, "north_m": -20000, "up_m": 1000.000000 } ])", wps);
        ScenarioDef d;
        ConsoleData c;
        std::string e;
        const bool ok = LoadScenario(Engage(tgt), kData, d, c, &e);
        if (ok || e.find(expect) == std::string::npos) std::printf("    expected \"%s\", got ok=%d \"%s\"\n", expect.c_str(), ok ? 1 : 0, e.c_str());
        return !ok && e.find(expect) != std::string::npos;
    };
    SC_EXPECT(bad(R"(, "behaviour": { "kind": "loop" })", "targets[0].behaviour.kind: unknown behaviour 'loop'"));
    SC_EXPECT(bad(R"(, "behaviour": { "kind": "pop_up" })", "a pop-up needs the hide"));
    SC_EXPECT(bad(R"(, "evade": { "on": "radio" })", "targets[0].evade.on: unknown cue 'radio'"));
    SC_EXPECT(bad(R"(, "evade": { "kind": "loop" })", "targets[0].evade.kind: unknown evasion 'loop'"));
    SC_EXPECT(bad(R"(, "evade": {}, "evade_on_launch": true)", "use either evade or evade_on_launch"));
    SC_EXPECT(bad("", "agl_m needs a behaviour", R"([ { "east_m": 1500, "north_m": -20000, "agl_m": 50 } ])"));
    SC_EXPECT(bad(R"(, "behaviour": { "kind": "terrain_following" })", "targets[0].waypoints[0].agl_m: must be >= 0",
                  R"([ { "east_m": 1500, "north_m": -20000, "agl_m": -5 } ])"));
    ScenarioDef d;
    ConsoleData c;
    std::string e;
    SC_EXPECT(LoadScenario(Engage(Target(20000, 1000, R"(, "behaviour": { "kind": "terrain_following", "agl_m": 80 },
        "evade": { "on": "lock", "kind": "jink" })")), kData, d, c, &e));
    SC_EXPECT(d.targets[0].behaviour.kind == TargetBehaviourDef::Kind::TerrainFollowing && d.targets[0].evade.present &&
              d.targets[0].evade.kind == EvadeDef::Kind::Jink && d.targets[0].evade.on == EvadeDef::On::Lock);
}

// ------------------------------------------------------------------------------------------ the shipped scenarios

namespace {
Run PlayFile(const std::string& name, double max_s) {
    return Play(ReadFile(kData + "/scenarios/" + name), max_s);
}
}  // namespace

SC_TEST("threat scenarios: against the jamming strike aircraft the crew fires by method Н with the range by hand") {
    const Run r = PlayFile("osa_jamming_em.json", 700.0);
    SC_EXPECT(r.loaded);
    SC_EXPECT(Has(r, "radar on the jammer track"));
    SC_EXPECT(Has(r, "started    fire_at_jammer"));
    SC_EXPECT(r.summary.launches >= 1);
    SC_EXPECT(Has(r, "N method"));
    SC_EXPECT(r.debrief.find("NO RANGE") == std::string::npos);
    SC_EXPECT(Has(r, "breaks away onto a beam heading"));
}

SC_TEST("threat scenarios: the low-level attack flies its strike run and the helicopter pops up from behind the ridge") {
    const Run r = PlayFile("osa_low_level_em.json", 900.0);
    SC_EXPECT(r.loaded);
    SC_EXPECT(Has(r, "h1  hovers behind cover"));
    SC_EXPECT(Has(r, "h1  pops up"));
    SC_EXPECT(Has(r, "h1  drops back behind cover"));
    SC_EXPECT(r.summary.launches >= 1);
    SC_EXPECT(Has(r, "t1  jinks (hard turns left and right)"));
}

SC_TEST("threat scenarios: with emissions control nothing radiates; the TV and the board's range carry the shot") {
    const Run r = PlayFile("osa_emcon_tv.json", 700.0);
    SC_EXPECT(r.loaded);
    SC_EXPECT(Has(r, "TV track"));
    SC_EXPECT(r.debrief.find("SOC              event") == std::string::npos);  // the search radar never saw anything
    SC_EXPECT(r.summary.launches == 1);
    SC_EXPECT(r.debrief.find("HIT") != std::string::npos || r.debrief.find("MISS") != std::string::npos);
}
