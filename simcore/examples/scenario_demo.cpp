// scenario_demo: play a scenario file headless and print its debrief.
//
// Usage: scenario_demo [scenario.json] [data_dir] [out.acmi]
//   defaults: data/scenarios/osa_practice.json, data, osa_practice.acmi (run from the repo root)
// Exits non-zero if the scenario fails to load or doesn't end within its time limit.
#include <cstdio>
#include <string>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;

int main(int argc, char** argv) {
    const std::string path = argc > 1 ? argv[1] : "data/scenarios/osa_practice.json";
    const std::string data_dir = argc > 2 ? argv[2] : "data";
    const std::string acmi_path = argc > 3 ? argv[3] : "osa_practice.acmi";

    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenarioFile(path, data_dir, def, console, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    ScenarioRunner runner(std::move(def), std::move(console));
    const bool ended = runner.RunToEnd(0.1);
    std::printf("%s", runner.Debrief().c_str());
    if (runner.Acmi().SaveTo(acmi_path)) std::printf("\nWrote %s (open it in Tacview)\n", acmi_path.c_str());
    return ended ? 0 : 1;
}
