// battery_link_demo: play a battery link headless (the player's Osa and the PU-12M in one battery, simcore/battery_link.hpp)
// and print both vehicles' debriefs.
//
// Usage: battery_link_demo [link.json] [data_dir] [out.acmi]
//   defaults: data/scenarios/battery_link_practice.json, data, battery_link.acmi (run from the repo root)
// Exits non-zero if the link fails to load or doesn't end within the Osa scenario's time limit.
#include <cstdio>
#include <memory>
#include <string>

#include "simcore/battery_link.hpp"

using namespace simcore;

int main(int argc, char** argv) {
    const std::string path = argc > 1 ? argv[1] : "data/scenarios/battery_link_practice.json";
    const std::string data_dir = argc > 2 ? argv[2] : "data";
    const std::string acmi_path = argc > 3 ? argv[3] : "battery_link.acmi";
    std::string error;
    std::unique_ptr<BatteryLink> link = LoadBatteryLinkFile(path, data_dir, &error);
    if (!link) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    const bool ended = link->RunToEnd(0.1);
    std::printf("==== The Osa %s\n%s\n==== The PU-12M\n%s", link->Callsign().c_str(), link->Osa().Debrief().c_str(),
                link->Post().Debrief().c_str());
    if (link->Osa().Acmi().SaveTo(acmi_path)) std::printf("\nWrote %s (open it in Tacview)\n", acmi_path.c_str());
    return ended ? 0 : 1;
}
