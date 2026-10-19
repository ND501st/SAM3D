#include <cmath>
#include <cstdio>
#include <string>

#include "simcore/cabin.hpp"
#include "simcore/console_data.hpp"
#include "test_framework.hpp"

using namespace simcore;

// The PU-12M's cabin (data/cabins/pu12m.json, written by tools/pu12m_cabin.py from the interior's trace; docs/PU12_DEVELOPMENT.md
// P6): a seat for each of the console's five stations, every control placed on a panel of its own station, the IT-45 and
// the two status boards on their panels.

namespace {

const std::string kData = SIMCORE_DATA_DIR;

bool LoadPu12m(ConsoleData& console, CabinLayout& layout) {
    if (!LoadConsoleFile(kData + "/procedures/pu12m.json", console)) return false;
    std::string error;
    const bool ok = LoadCabinFile(kData + "/cabins/pu12m.json", console, layout, &error);
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    return ok;
}

}  // namespace

SC_TEST("pu12m cabin: five seats, every control placed, the IT-45 and the status boards") {
    ConsoleData console;
    CabinLayout layout;
    SC_EXPECT(LoadPu12m(console, layout));
    SC_EXPECT(layout.stations.size() == 5);
    for (const std::string& id : layout.UnplacedControls(console.panel)) std::printf("    unplaced: %s\n", id.c_str());
    SC_EXPECT(layout.UnplacedControls(console.panel).empty());
    const CabinDisplay* it45 = nullptr;
    int boards = 0;
    for (const CabinDisplay& d : layout.displays) {
        if (d.kind == CabinDisplayKind::AspdScope) it45 = &d;
        if (d.kind == CabinDisplayKind::StatusBoard) ++boards;
    }
    SC_EXPECT(it45 != nullptr && it45->glass);
    SC_EXPECT(boards == 2);
    // The IT-45 is the battery commander's, in front of his place on the bench (csla_pu12_manual, rwd_mb3); the plotter's map
    // in front of his.
    const CabinPanel* tube = it45 != nullptr ? layout.FindPanel(it45->panel_id) : nullptr;
    const CabinStation* cmdr = layout.FindStation("battery_commander");
    SC_EXPECT(tube != nullptr && cmdr != nullptr && tube->station == "battery_commander");
    if (tube != nullptr && cmdr != nullptr) {
        const Vec3 to_tube = tube->centre_m - cmdr->eye_m;
        SC_EXPECT(to_tube.y > 0.4 && to_tube.y < 0.9);              // ahead of him, at reading distance
        SC_EXPECT(std::fabs(to_tube.x) < 0.15);                      // about straight ahead
        SC_EXPECT(tube->Normal().y < -0.99);                        // facing back at him
    }
    const CabinStation* plotter = layout.FindStation("operator");
    const CabinDisplay* map = nullptr;
    const CabinDisplay* window = nullptr;
    for (const CabinDisplay& d : layout.displays) {
        if (d.kind == CabinDisplayKind::PlotBoard) map = &d;
        if (d.kind == CabinDisplayKind::KeypadWindow) window = &d;
    }
    SC_EXPECT(map != nullptr && window != nullptr && plotter != nullptr);
    if (map != nullptr && plotter != nullptr) {
        const Vec3 to_map = layout.FindPanel(map->panel_id)->centre_m - plotter->eye_m;
        // Swung open on its hinge at the plan's folded strip (on the centreline before his place): ahead of him, a little
        // to his right, below his eye.
        SC_EXPECT(to_map.y > 0.3 && to_map.y < 0.7 && to_map.x > 0.05 && to_map.x < 0.3 && to_map.z < -0.2);
    }
    // The panels take the equipment's paint.
    const CabinPanel* fr = layout.FindPanel("fr_panel");
    SC_EXPECT(fr != nullptr && fr->color == "bluegrey");
    // The R-123Ms are bare panels on the Osa cabin's traced mesh.
    for (const char* id : {"r123_fug1_panel", "r123_fug2_panel", "r123_fug4_panel"}) {
        const CabinPanel* p = layout.FindPanel(id);
        SC_EXPECT(p != nullptr && p->bare);
    }
}

SC_TEST("pu12m cabin: the front seats are ahead of the bench, the driver on the left") {
    ConsoleData console;
    CabinLayout layout;
    SC_EXPECT(LoadPu12m(console, layout));
    const CabinStation* driver = layout.FindStation("driver");
    const CabinStation* leader = layout.FindStation("squad_leader");
    const CabinStation* commander = layout.FindStation("battery_commander");
    const CabinStation* radio = layout.FindStation("radio_operator");
    SC_EXPECT(driver != nullptr && leader != nullptr && commander != nullptr && radio != nullptr);
    if (driver == nullptr || leader == nullptr || commander == nullptr || radio == nullptr) return;
    SC_EXPECT(driver->eye_m.x < 0.0 && leader->eye_m.x > 0.0);     // body x is to the right
    SC_EXPECT(driver->eye_m.y > commander->eye_m.y + 2.5);         // the plan: 3 m between the front seats and the bench
    SC_EXPECT(radio->eye_m.x < commander->eye_m.x);                // the radio operator on the bench's left place
}

SC_TEST("pu12m cabin: an aspd_scope or a status_board needs the console's post block") {
    ConsoleData osa;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", osa));
    const std::string text = R"({ "system_id": "osa_ak",
      "stations": [ { "id": "senior_operator", "label": "SO", "eye_m": [0, 0, 1.5] }, { "id": "operator", "label": "O", "eye_m": [0, 1, 1.5] },
                    { "id": "deputy_commander", "label": "DC", "eye_m": [1, 0, 1.5] }, { "id": "commander", "label": "C", "eye_m": [1, 3, 1.5] },
                    { "id": "driver", "label": "D", "eye_m": [-1, 3, 1.5] } ],
      "panels": [ { "id": "p", "label": "P", "station": "operator", "centre_m": [0, 0, 1], "facing_deg": 0, "tilt_deg": 0, "size_m": [1, 1] } ],
      "displays": [ { "id": "d", "label": "D", "kind": "aspd_scope", "panel": "p", "at_m": [0, 0], "size_m": [0.3, 0.3],
                      "source": "sam3d", "confidence": "low" } ] })";
    CabinLayout layout;
    std::string error;
    SC_EXPECT(!LoadCabin(text, osa, layout, &error));
    SC_EXPECT(error.find("post block") != std::string::npos);
}
