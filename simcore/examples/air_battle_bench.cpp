// air_battle_bench — what the air battle costs a session's step, and what the levels of detail save (roadmap B12,
// docs/air_battle/02_FLIGHTS_AND_PILOTS.md §5.2, §5.4).
//
// Part 1: about 100 aircraft of the testbed's waves (air_war/test_waves.hpp: every faction's types with their warning
// receivers, reactions, formations and flights) entered at once into the dynamic campaign testbed (its battery, its
// neighbours and their radars, no waves of its own), flown for a few minutes at 0.05 s a step in three cases:
//   near, every aircraft at A2: the waves aimed at the battery, the levels off;
//   far, at A2: the same waves aimed at a city 220 km away, the levels off;
//   far, at A1: the same, the levels on (data/gameplay/air_levels.json's values, mode 1).
// For each it prints the mean and the worst step, the air battle's parts of it (the step profile, step_profile.hpp) and
// the EM world's warning receivers, against the budget of about 0.5 ms a step for about 100 aircraft.
//
// Part 2 (--testbed [seconds]): the dynamic campaign testbed itself, its waves as they come, with the levels off (mode 0)
// and on (mode 1), the mean and worst step and the parts, and whether the outcome is the same.
//
// Part 3 (--compare seconds, alone): the testbed with the levels off, off again and on; the first line where the
// debriefs part, and each flight's changes of level with the levels on.
//
// Usage: air_battle_bench [data_dir] [--seconds S] [--aircraft N] [--testbed [S]] [--compare S]
// Generic placeholder numbers in the testbed's data; timings are wall time on one core and move with the machine's load.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war/test_waves.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;

namespace {

struct Result {
    StepProfile prof;
    double esm_ms = 0.0;      // the unit's EM world's receivers (ms a step)
    double em_ms[7] = {};     // the unit's EM world by phase (ms a step): platforms, emitters, radars, ESM, TDOA, knowledge, comms
    double aircraft = 0.0;    // alive a step, on average
    double a1_share = 0.0;    // of the aircraft-steps
    int changes = 0;
    std::string debrief;      // the testbed's outcome: its debrief, our launches and the neighbours'
    int launches = 0, defence_launches = 0;
    std::vector<std::string> level_log;  // each flight's changes of level, with the time
    bool ok = false;
};

bool LoadTestbed(const std::string& data_dir, ScenarioDef& def, ConsoleData& console) {
    std::string error;
    if (!LoadScenarioFile(data_dir + "/scenarios/dynamic_campaign_test.json", data_dir, def, console, &error)) {
        std::fprintf(stderr, "air_battle_bench: %s\n", error.c_str());
        return false;
    }
    return true;
}

void EmPhases(ScenarioRunner& r, Result& res) {
    if (r.Em() == nullptr || res.prof.steps == 0) return;
    const em::EmStats& st = r.Em()->World().Scene().stats;
    const std::uint64_t ns[7] = {st.ns_platforms, st.ns_emitters, st.ns_radars, st.ns_esm, st.ns_tdoa, st.ns_knowledge, st.ns_comms};
    for (int k = 0; k < 7; ++k) res.em_ms[k] = static_cast<double>(ns[k]) * 1e-6 / static_cast<double>(res.prof.steps);
    res.esm_ms = res.em_ms[3];
}

void Sample(const ScenarioRunner& r, double& sum_alive, double& sum_a1) {
    int alive = 0, a1 = 0;
    r.LevelCounts(alive, a1);
    sum_alive += alive;
    sum_a1 += a1;
}

Result RunWaves(const std::string& data_dir, int n, double seconds, bool far, int mode) {
    Result res;
    ScenarioDef def;
    ConsoleData console;
    if (!LoadTestbed(data_dir, def, console)) return res;
    std::shared_ptr<const airwar::WavesDef> wd = def.waves;
    if (!wd) return res;
    def.waves.reset();
    def.air_levels.mode = mode;
    ScenarioRunner r(std::move(def), std::move(console));
    auto mine = std::make_shared<airwar::WavesDef>(*wd);
    mine->friendly_share = 0.0;  // the enemy's only: our own flights stay at A2 wherever they are
    airwar::TestWaves waves(mine, 1985u);
    airwar::WaveScene scene;
    scene.frame = &r.GetWorld().Frame();
    scene.terrain = &r.GetWorld().GetTerrain();
    scene.centre = r.UnitPosition() + (far ? Vec3{-150000.0, -160000.0, 0.0} : Vec3());
    scene.defences.push_back(scene.centre);
    scene.em = r.Em() != nullptr ? &r.Em()->World().Scene().lib : nullptr;
    std::vector<ScenarioTarget> air;
    std::vector<ScenarioFlight> flights;
    std::vector<EmJammerDef> jammers;
    while (static_cast<int>(air.size()) < n) {
        airwar::Wave w = waves.Next(0.0, scene);
        for (ScenarioTarget& t : w.aircraft) air.push_back(std::move(t));
        for (ScenarioFlight& f : w.flight_defs) flights.push_back(std::move(f));
        for (EmJammerDef& j : w.jammers) jammers.push_back(j);
    }
    std::string e;
    if (r.Em() != nullptr && !jammers.empty() && !r.AddEmGear(r.Em()->World().Scene().lib, jammers, "bench", &e)) std::fprintf(stderr, "  jammers: %s\n", e.c_str());
    if (!r.AddFlights(flights, {}, &e)) std::fprintf(stderr, "  flights: %s\n", e.c_str());
    if (!r.AddTargets(std::move(air), "bench", &e)) {
        std::fprintf(stderr, "air_battle_bench: %s\n", e.c_str());
        return res;
    }
    // The first 45 s bring the waves' later flights in; then measure.
    const double dt = 0.05;
    while (r.Time() < 45.0) r.Step(dt);
    r.SetProfile(true);
    double alive = 0.0, a1 = 0.0;
    int samples = 0;
    while (r.Time() < 45.0 + seconds && !r.Finished()) {
        r.Step(dt);
        Sample(r, alive, a1);
        ++samples;
    }
    res.prof = r.Profile();
    EmPhases(r, res);
    res.aircraft = samples > 0 ? alive / samples : 0.0;
    res.a1_share = alive > 0.0 ? a1 / alive : 0.0;
    res.changes = r.LevelChanges();
    res.ok = true;
    return res;
}

Result RunTestbed(const std::string& data_dir, double seconds, int mode) {
    Result res;
    ScenarioDef def;
    ConsoleData console;
    if (!LoadTestbed(data_dir, def, console)) return res;
    if (mode >= 0) def.air_levels.mode = mode;
    ScenarioRunner r(std::move(def), std::move(console));
    r.SetProfile(true);
    double alive = 0.0, a1 = 0.0;
    int samples = 0;
    std::vector<AirLevel> was;
    while (r.Time() < seconds && !r.Finished()) {
        r.Step(0.05);
        Sample(r, alive, a1);
        ++samples;
        const std::vector<FlightState>& fs = r.AllFlights().All();
        was.resize(fs.size(), AirLevel::A2);
        for (std::size_t k = 0; k < fs.size(); ++k) {
            const AirLevel l = r.FlightLevel(fs[k].id);
            if (l == was[k]) continue;
            char b[160];
            std::snprintf(b, sizeof(b), "%.1f s %s %s", r.Time(), fs[k].id.c_str(), AirLevelName(l));
            res.level_log.push_back(b);
            was[k] = l;
        }
    }
    res.prof = r.Profile();
    EmPhases(r, res);
    res.aircraft = samples > 0 ? alive / samples : 0.0;
    res.a1_share = alive > 0.0 ? a1 / alive : 0.0;
    res.changes = r.LevelChanges();
    res.debrief = r.Debrief();
    res.launches = r.Launches();
    res.defence_launches = r.DefenceLaunches();
    res.ok = true;
    return res;
}

// The air battle's own parts: what B1-B9 added to a step and the levels change.
double AirBattleMs(const StepProfile& p) {
    double ms = 0.0;
    for (int k : {StepProfile::Levels, StepProfile::Flights, StepProfile::World, StepProfile::Pilots, StepProfile::Radio, StepProfile::Perception,
                  StepProfile::Reactions, StepProfile::AirFight}) {
        ms += p.MeanMs(k);
    }
    return ms;
}

void Print(const char* name, const Result& r) {
    if (!r.ok) {
        std::printf("| %s | (did not run) |\n", name);
        return;
    }
    const StepProfile& p = r.prof;
    std::printf("| %s | %.0f | %.0f%% | %d | %.3f | %.2f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f |\n", name, r.aircraft, 100.0 * r.a1_share,
                r.changes, p.MeanStepMs(), p.worst_ms, AirBattleMs(p), p.MeanMs(StepProfile::Levels), p.MeanMs(StepProfile::Flights) + p.MeanMs(StepProfile::World),
                p.MeanMs(StepProfile::Pilots), p.MeanMs(StepProfile::Radio), p.MeanMs(StepProfile::Perception), p.MeanMs(StepProfile::Reactions),
                p.MeanMs(StepProfile::Engagement), r.esm_ms);
}

void Header() {
    std::printf("| Case | Aircraft | At A1 | Level changes | Step ms (mean) | Step ms (worst) | Air battle ms | Levels | Flights and flight model | Pilots' height | Radio | Perception | Reactions | Engagement + EM | of it receivers |\n");
    std::printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
}

void Parts(const char* name, const Result& r) {
    const StepProfile& p = r.prof;
    std::printf("%s, ms a step by part:", name);
    for (int k = 0; k < StepProfile::kParts; ++k) std::printf(" %s %.3f;", StepProfile::Name(k), p.MeanMs(k));
    static const char* const kEm[7] = {"platforms", "emitters", "radars", "ESM", "TDOA", "knowledge", "comms"};
    std::printf("\n  of the engagement, the EM world's phases:");
    for (int k = 0; k < 7; ++k) std::printf(" %s %.3f;", kEm[k], r.em_ms[k]);
    std::printf("\n");
}

std::vector<std::string> Lines(const std::string& t) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < t.size()) {
        const std::size_t e = std::min(t.find('\n', at), t.size());
        out.push_back(t.substr(at, e - at));
        at = e + 1;
    }
    return out;
}

// The first line two debriefs differ at (its number, both texts), or -1.
int FirstDiff(const std::string& x, const std::string& y, std::string& lx, std::string& ly) {
    const std::vector<std::string> a = Lines(x), b = Lines(y);
    for (std::size_t k = 0; k < std::max(a.size(), b.size()); ++k) {
        lx = k < a.size() ? a[k] : "(none)";
        ly = k < b.size() ? b[k] : "(none)";
        if (lx != ly) return static_cast<int>(k);
    }
    return -1;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data_dir = "data";
    double seconds = 180.0;
    int n = 100;
    bool testbed = false;
    double testbed_s = 600.0;
    double compare_s = 0.0;
    int pos = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--aircraft") == 0 && i + 1 < argc) {
            n = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--testbed") == 0) {
            testbed = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') testbed_s = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--compare") == 0 && i + 1 < argc) {
            compare_s = std::atof(argv[++i]);
        } else if (pos++ == 0) {
            data_dir = argv[i];
        }
    }
    if (compare_s > 0.0) {
        // Part 3: the testbed's outcome with the levels off, off again and on: where the debriefs part.
        const Result a = RunTestbed(data_dir, compare_s, 0);
        const Result b = RunTestbed(data_dir, compare_s, 0);
        const Result c = RunTestbed(data_dir, compare_s, 1);
        std::string x, y;
        const int ab = FirstDiff(a.debrief, b.debrief, x, y);
        std::printf("the testbed, %.0f s, off against off: %s\n", compare_s, ab < 0 ? "identical" : "differ");
        if (ab >= 0) std::printf("  line %d:\n    %s\n    %s\n", ab, x.c_str(), y.c_str());
        const int ac = FirstDiff(a.debrief, c.debrief, x, y);
        std::printf("off against on (%d level changes): %s; launches %d and %d\n", c.changes, ac < 0 ? "identical" : "differ", a.launches, c.launches);
        if (ac >= 0) std::printf("  line %d:\n    %s\n    %s\n", ac, x.c_str(), y.c_str());
        std::printf("the levels with them on:\n");
        for (const std::string& l : c.level_log) std::printf("  %s\n", l.c_str());
        return 0;
    }
    std::printf("# The air battle's cost (%d aircraft of the testbed's waves, %.0f s at 0.05 s a step; budget about 0.5 ms a step)\n\n", n, seconds);
    Header();
    const Result near_a2 = RunWaves(data_dir, n, seconds, false, 0);
    Print("near, A2 (levels off)", near_a2);
    const Result far_a2 = RunWaves(data_dir, n, seconds, true, 0);
    Print("far, A2 (levels off)", far_a2);
    const Result far_a1 = RunWaves(data_dir, n, seconds, true, 1);
    Print("far, A1 (levels on)", far_a1);
    std::printf("\n");
    Parts("near, A2", near_a2);
    Parts("far, A2", far_a2);
    Parts("far, A1", far_a1);
    if (testbed) {
        std::printf("\n# The testbed, %.0f s\n\n", testbed_s);
        Header();
        const Result off = RunTestbed(data_dir, testbed_s, 0);
        Print("testbed, levels off", off);
        const Result on = RunTestbed(data_dir, testbed_s, 1);
        Print("testbed, levels on", on);
        std::printf("\n");
        Parts("testbed, off", off);
        Parts("testbed, on", on);
        // The outcome: flights come in from far away at A1 and are at A2 before they come near (the levels must change
        // nothing inside the play circle).
        const std::vector<std::string> la = Lines(off.debrief), lb = Lines(on.debrief);
        int differ = static_cast<int>(la.size() > lb.size() ? la.size() - lb.size() : lb.size() - la.size());
        for (std::size_t k = 0; k < std::min(la.size(), lb.size()); ++k) differ += la[k] != lb[k] ? 1 : 0;
        std::printf("\nthe testbed's outcome, levels off against on: our launches %d against %d, the neighbours' %d against %d, "
                    "the debrief %s (%d lines differ)\n",
                    off.launches, on.launches, off.defence_launches, on.defence_launches, differ == 0 ? "identical" : "differs", differ);
    }
    return 0;
}
