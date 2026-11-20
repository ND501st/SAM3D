// simcore/air_war/air_front_def.hpp — the fronts' sectors and the air front's game values as data (track F, F2.1-F2.2,
// docs/fronts/02_AIR_FRONT.md §2 and §6).
//
// A theatre front's "sectors": named runs of the front's line, given as distances along it from its first point, so a
// sector stays valid when the line is anchored to the map or moved (F1). A front without them is one sector the length
// of its line. Loaded by data.hpp (LoadSectors); the line of contact and the sector lookups are front_line.hpp's, the
// believed cover over a sector's band air_front.hpp's. The game values are TuningTable's "air_front.*".
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace simcore {
namespace airwar {

// One sector of a front: from `from_m` to `to_m` along the front's line (its first point at 0).
struct SectorDef {
    std::string id;
    std::string name;
    double from_m = 0.0;
    double to_m = 0.0;
};

// The game values of the air front (data/gameplay/air_war.json "air_front"), every one in TuningTable::Defaults.
inline std::vector<std::pair<std::string, double>> AirFrontTuningDefaults() {
    return {
        // 1: the enemy's air front by sector (F2.3; until then the per-cycle measure of the believed cover and the losses
        // by sector, which nothing reads); 0: before it, nothing built or measured (a save without the key replays at 0).
        {"air_front.mode", 0.0},
        {"air_front.station_m", 5000.0},  // the line of contact's stations along the front's line
        {"air_front.step_m", 20000.0},    // the air front's step, and the depth of the band beyond it the cover is weighed over
    };
}

}  // namespace airwar
}  // namespace simcore
