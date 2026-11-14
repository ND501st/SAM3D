// ground_check: roadmap R5's check, "nothing under the map", from the simulation (simcore/ground_check.hpp). Plays the
// dynamic campaign testbed for an hour and every mission of the missions menu on a map ("terrain": {"type": "map"})
// for ten minutes, judges every aircraft, weapon, missile and ground item against the ground once a second, and lists
// what is under it (or a ground item floating over it). The game's own check (sam3d.ground.check, tools/ue_ground_check.py)
// does the same for what Unreal draws.
//
//   ground_check <data dir> [seconds=3600] [scenario file...]   (scenario files relative to the data dir)
//
// Exit 0 when nothing is listed; 1 when anything is, or a scenario does not load.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/ground_check.hpp"
#include "simcore/missions.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;

namespace {

// One scenario for `seconds`: how many listings, the first of each thing printed.
int Play(const std::string& data, const std::string& file, double seconds) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenarioFile(data + "/" + file, data, def, console, &e)) {
        std::printf("%s: does not load: %s\n", file.c_str(), e.c_str());
        return 1;
    }
    ScenarioRunner r(std::move(def), std::move(console));
    const auto t0 = std::chrono::steady_clock::now();
    std::map<std::pair<std::string, std::string>, int> seen;  // (kind, id) -> listings
    int listings = 0, checks = 0;
    double next_s = 0.0;
    while (r.Time() < seconds && !r.Finished()) {
        r.Step(0.1);
        if (r.Time() + 1e-9 < next_s) continue;
        next_s += 1.0;
        ++checks;
        for (const BelowGround& b : EntitiesOffGround(r)) {
            ++listings;
            if (seen[{b.kind, b.id}]++ == 0) std::printf("  %s\n", b.Line().c_str());
        }
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("%s: %.0f s played (%d checks, %.1f s wall): %d listings of %zu things; %d aircraft flew into the ground\n",
                file.c_str(), r.Time(), checks, wall, listings, seen.size(), r.TerrainStrikes());
    for (const auto& s : seen) {
        if (s.second > 1) std::printf("    %-9s %-28s listed %d times\n", s.first.first.c_str(), s.first.second.c_str(), s.second);
    }
    return listings + r.TerrainStrikes();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: ground_check <data dir> [seconds=3600] [scenario file...]\n");
        return 1;
    }
    const std::string data = argv[1];
    const double seconds = argc > 2 ? std::atof(argv[2]) : 3600.0;
    std::vector<std::pair<std::string, double>> runs;
    for (int i = 3; i < argc; ++i) runs.push_back({argv[i], seconds});
    if (runs.empty()) {
        runs.push_back({"scenarios/dynamic_campaign_test.json", seconds});
        MissionCatalog catalog;
        std::string e;
        if (!LoadMissionCatalog(data, catalog, &e)) {
            std::printf("missions: %s\n", e.c_str());
            return 1;
        }
        for (const MissionGroup& g : catalog.groups) {
            for (const MissionDef& m : g.missions) {
                if (m.link || m.file == runs.front().first) continue;
                ScenarioDef def;
                ConsoleData console;
                if (!LoadScenarioFile(data + "/" + m.file, data, def, console, &e) || !def.map) continue;  // on a map only
                runs.push_back({m.file, seconds < 600.0 ? seconds : 600.0});
            }
        }
    }
    int failures = 0;
    for (const std::pair<std::string, double>& run : runs) failures += Play(data, run.first, run.second) > 0 ? 1 : 0;
    std::printf("%s: %d of %zu scenarios with something off the ground\n", failures == 0 ? "PASS" : "FAIL", failures, runs.size());
    return failures == 0 ? 0 : 1;
}
