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
// debriefs part, and each flight's changes of level with the levels on. With --seeds N [--first K]: N seeds from K (1 if
// not given; each its own waves), off and on, the outcomes side by side.
//
// Part 4 (--trace seconds [--who id], alone): the testbed off and on stepped side by side; for every aircraft, the first
// moment its place, its receiver's display, its picture or its stress parts between the two.
//
// Usage: air_battle_bench [data_dir] [--seconds S] [--aircraft N] [--testbed [S]] [--compare S [--seeds N [--first K]]] [--trace S [--who id] [--seeds 1 --first K]]
// Generic placeholder numbers in the testbed's data; timings are wall time on one core and move with the machine's load.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
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
    int downed = 0;           // aircraft destroyed (by us, a neighbour, anything) or flown into the ground
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

Result RunTestbed(const std::string& data_dir, double seconds, int mode, int seed = -1) {
    Result res;
    ScenarioDef def;
    ConsoleData console;
    if (!LoadTestbed(data_dir, def, console)) return res;
    if (mode >= 0) def.air_levels.mode = mode;
    if (seed >= 0) def.seed = static_cast<unsigned>(seed);
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
    for (const DebriefEntry& e : r.Log().Entries()) {
        if (e.kind != DebriefEntry::Kind::Scenario) continue;
        if (e.detail == "destroyed" || e.detail.compare(0, 13, "destroyed by ") == 0 || e.detail == "flew into the ground") ++res.downed;
    }
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

// Part 4 (--trace seconds): the testbed with the levels off and on stepped side by side; for every aircraft, the first
// moment its place, its receiver's display, its picture or its stress parts between the two, and whether it was then a
// wingman at A1 (hearing on his leader's receiver) in the run with the levels on. The first lines say where a difference
// starts and how it spreads.
std::string g_who;  // --who: an aircraft whose distance between the two runs is printed every 5 s

struct TraceMark {
    double t = -1.0;
    std::string what;
    bool at_a1 = false;  // a wingman at A1 then
};

std::string PictureKey(const std::vector<airwar::Sighting>& p) {
    std::string k;
    char b[160];
    for (const airwar::Sighting& s : p) {
        std::snprintf(b, sizeof(b), "%d/%d/%d/%llx/%.2f/%d/%s;", static_cast<int>(s.kind), static_cast<int>(s.by), static_cast<int>(s.rwr),
                      static_cast<unsigned long long>(s.debug_truth_key), s.time_s, s.seen_count, s.what.c_str());
        k += b;
    }
    return k;
}

std::string RwrKey(ScenarioRunner& r, const std::string& id) {
    std::string k;
    if (r.Em() == nullptr) return k;
    char b[96];
    for (const EmSensorLink::RwrContactView& c : r.Em()->RwrPictureOf(id)) {
        std::snprintf(b, sizeof(b), "%llx/%d/%.4f;", static_cast<unsigned long long>(c.key), c.state, c.bearing_rad);
        k += b;
    }
    return k;
}

int Trace(const std::string& data_dir, double seconds, int mode_on, int seed) {
    ScenarioDef d0, d1;
    ConsoleData c0, c1;
    if (!LoadTestbed(data_dir, d0, c0) || !LoadTestbed(data_dir, d1, c1)) return 1;
    d0.air_levels.mode = 0;
    d1.air_levels.mode = mode_on;
    if (seed >= 0) d0.seed = d1.seed = static_cast<unsigned>(seed);
    ScenarioRunner r0(std::move(d0), std::move(c0));
    ScenarioRunner r1(std::move(d1), std::move(c1));
    std::map<std::string, TraceMark> pos, rwr, pic, stress;
    std::map<std::string, std::string> rwr_on_a1;  // a wingman's receiver display as he was woken
    std::vector<std::pair<double, std::string>> log;
    std::vector<AirLevel> was;
    while (r0.Time() < seconds && !r0.Finished() && !r1.Finished()) {
        r0.Step(0.05);
        r1.Step(0.05);
        const std::vector<ScenarioTarget>& ts = r0.Def().targets;
        const std::size_t n = std::min(ts.size(), r1.Def().targets.size());
        for (std::size_t i = 0; i < n; ++i) {
            const std::string& id = ts[i].id;
            if (r1.Def().targets[i].id != id) {
                log.push_back({r0.Time(), "target list parts at " + id});
                break;
            }
            const Aircraft* a0 = r0.TargetAircraft(id);
            const Aircraft* a1 = r1.TargetAircraft(id);
            if (a0 == nullptr || a1 == nullptr) continue;
            const bool at_a1 = r1.TargetLevel(id) == AirLevel::A1;
            char b[200];
            const double dp = (a0->position - a1->position).Length();
            if (pos[id].t < 0.0 && (dp > 0.01 || a0->alive != a1->alive)) {
                std::snprintf(b, sizeof(b), "%.1f m apart", dp);
                pos[id] = {r0.Time(), b, at_a1};
            }
            if (rwr[id].t < 0.0 && !at_a1) {
                const std::string x = RwrKey(r0, id), y = RwrKey(r1, id);
                if (x != y) rwr[id] = {r0.Time(), "off " + x.substr(0, 90) + " | on " + y.substr(0, 90), at_a1};
            }
            if (pic[id].t < 0.0 && !at_a1) {
                const std::string x = PictureKey(r0.PilotPicture(id)), y = PictureKey(r1.PilotPicture(id));
                if (x != y) {
                    std::snprintf(b, sizeof(b), "(%.1f m apart) ", dp);
                    pic[id] = {r0.Time(), b + std::string("off ") + x.substr(0, 110) + " | on " + y.substr(0, 110), at_a1};
                }
            }
            if (id == g_who && std::fmod(r0.Time() + 1e-6, 5.0) < 0.05) {
                std::snprintf(b, sizeof(b), "%s: %.1f m apart, speeds %.1f %.1f, picture %zu %zu%s", id.c_str(), dp, a0->speed_mps, a1->speed_mps,
                              r0.PilotPicture(id).size(), r1.PilotPicture(id).size(), at_a1 ? " (at A1)" : "");
                log.push_back({r0.Time(), b});
            }
            const double s0 = r0.Reactions().Stress(static_cast<int>(i), r0.Time()), s1 = r1.Reactions().Stress(static_cast<int>(i), r1.Time());
            if (stress[id].t < 0.0 && std::fabs(s0 - s1) > 1e-9) {
                std::snprintf(b, sizeof(b), "%.3f against %.3f", s0, s1);
                stress[id] = {r0.Time(), b + std::string("; pictures off ") + PictureKey(r0.PilotPicture(id)).substr(0, 300) + " | on " +
                                             PictureKey(r1.PilotPicture(id)).substr(0, 300),
                              at_a1};
            }
        }
        const std::vector<FlightState>& fs = r1.AllFlights().All();
        was.resize(fs.size(), AirLevel::A2);
        for (std::size_t k = 0; k < fs.size(); ++k) {
            const AirLevel l = r1.FlightLevel(fs[k].id);
            if (l == was[k]) continue;
            log.push_back({r1.Time(), "flight " + fs[k].id + " to " + AirLevelName(l)});
            was[k] = l;
        }
    }
    for (const auto& m : {std::make_pair("place", &pos), std::make_pair("receiver", &rwr), std::make_pair("picture", &pic),
                          std::make_pair("stress", &stress)}) {
        for (const auto& kv : *m.second) {
            if (kv.second.t < 0.0) continue;
            log.push_back({kv.second.t, std::string(m.first) + " " + kv.first + (kv.second.at_a1 ? " (at A1) " : " ") + kv.second.what});
        }
    }
    std::stable_sort(log.begin(), log.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    for (const auto& l : log) std::printf("%8.2f  %s\n", l.first, l.second.c_str());
    std::string x, y;
    const int dl = FirstDiff(r0.Debrief(), r1.Debrief(), x, y);
    std::printf("debriefs: %s\n", dl < 0 ? "identical" : "differ");
    if (dl >= 0) std::printf("  line %d:\n    %s\n    %s\n", dl, x.c_str(), y.c_str());
    std::printf("launches %d and %d, the neighbours' %d and %d\n", r0.Launches(), r1.Launches(), r0.DefenceLaunches(), r1.DefenceLaunches());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data_dir = "data";
    double seconds = 180.0;
    int n = 100;
    bool testbed = false;
    double testbed_s = 600.0;
    double compare_s = 0.0;
    double trace_s = 0.0;
    int seeds = 0;
    int first_seed = 1;
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
        } else if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) {
            seeds = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--first") == 0 && i + 1 < argc) {
            first_seed = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace_s = std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--who") == 0 && i + 1 < argc) {
            g_who = argv[++i];
        } else if (pos++ == 0) {
            data_dir = argv[i];
        }
    }
    if (trace_s > 0.0) return Trace(data_dir, trace_s, 1, seeds > 0 ? first_seed : -1);
    if (compare_s > 0.0 && seeds > 0) {
        // Part 3b: the testbed over several seeds (each its own waves), the levels off and on: the outcomes side by side,
        // to set any difference the levels make beside what a seed changes.
        std::printf("| Seed | Launches off | on | Neighbours' off | on | Aircraft down off | on | Level changes | Debrief |\n|---|---|---|---|---|---|---|---|---|\n");
        int same = 0;
        for (int sd = first_seed; sd < first_seed + seeds; ++sd) {
            const Result a = RunTestbed(data_dir, compare_s, 0, sd);
            const Result c = RunTestbed(data_dir, compare_s, 1, sd);
            std::string x, y;
            const int at = FirstDiff(a.debrief, c.debrief, x, y);
            if (at < 0) ++same;
            std::printf("| %d | %d | %d | %d | %d | %d | %d | %d | %s |\n", sd, a.launches, c.launches, a.defence_launches, c.defence_launches, a.downed,
                        c.downed, c.changes, at < 0 ? "identical" : ("parts at line " + std::to_string(at)).c_str());
            std::fflush(stdout);
        }
        std::printf("\n%d of %d seeds identical\n", same, seeds);
        return 0;
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
