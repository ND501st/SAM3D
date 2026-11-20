// simcore/air_war/air_front_def.hpp — the fronts' sectors, a faction's air front rules and the air front's game values as
// data (track F, F2.1-F2.3, docs/fronts/02_AIR_FRONT.md §2, §3 and §6).
//
// A theatre front's "sectors": named runs of the front's line, given as distances along it from its first point, so a
// sector stays valid when the line is anchored to the map or moved (F1). A front without them is one sector the length
// of its line. Loaded by data.hpp (LoadSectors); the line of contact and the sector lookups are front_line.hpp's, the
// believed cover over a sector's band air_front.hpp's. A faction's "air_front" block (AirFrontDef, F2.3) is its air staff's
// rules for moving its air front by sector: the gates, the loss line, the delay, the greatest depth, the crisis and the
// surge; loaded by data.hpp (LoadAirFrontBlock), read by air_front.hpp's DecideAirFront. The game values are
// TuningTable's "air_front.*".
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

// How a faction's air front moves (§3): by the gates, sector by sector (NATO, the Pact); forward for a few days only where
// our mobile SAMs are believed gone, then back (the rebels' surge); or not at all until a political event (the neutral,
// whose air front is its border by doctrine).
enum class AirFrontKind { Advance, Surge, Border };
// What a crisis on its own ground front (F3.5) lets its chief do: advance a step and ignore the loss gate for a while
// (NATO, the Pact: the Golan, 1973), or send its helicopters a step ahead of the fixed-wing front, once (the rebels, the
// neutral).
enum class AirFrontCrisis { None, Advance, Helicopters };

inline const char* AirFrontKindName(AirFrontKind k) {
    switch (k) {
        case AirFrontKind::Advance: return "advance";
        case AirFrontKind::Surge: return "surge";
        case AirFrontKind::Border: return "border";
    }
    return "advance";
}

// A faction's "air_front" block (data/air_war/faction_<id>.json). SAM3D estimates bounded by the cases
// (docs/research/fronts_2026-11/air_ground_cases.md section 7). The chief's traits move the gates by at most
// air_front.trait_shift of their value (air_front.hpp).
struct AirFrontDef {
    bool present = false;              // a faction without the block: its air front never moves
    AirFrontKind kind = AirFrontKind::Advance;
    double cover_at_most = 0.3;        // the believed cover over the next band at most this ...
    double mobile_destroyed_at_least = 0.5;  // ... and our mobile air defence there believed destroyed at least this
    double loss_line = 0.03;           // aircraft lost to ground fire per low-level sortie above which it pulls back
    int loss_days = 2;                 // the loss window, days (1-3)
    double delay_lo_h = 6.0;           // from the cycle that first saw the gates hold to the step: a bold chief's ...
    double delay_hi_h = 24.0;          // ... and a methodical one's
    double max_depth_m = 60000.0;      // its greatest depth beyond the line of contact (and never past the politics' depth)
    double min_reward = 1.0;           // the believed worth of the next band's targets a step needs (value x weight)
    AirFrontCrisis crisis = AirFrontCrisis::None;
    double crisis_lo_h = 24.0;         // how long a crisis lets it ignore the loss gate
    double crisis_hi_h = 72.0;
    double surge_lo_h = 24.0;          // Surge: how long a surge lasts before it is cut back to the border
    double surge_hi_h = 72.0;
    double surge_rest_h = 72.0;        // Surge: and how long after that before the sector may surge again
    std::string after_event;           // Border: the political event after which it may move (its politics' events)
    std::string source;
    std::string confidence;
};

// The game values of the air front (data/gameplay/air_war.json "air_front"), every one in TuningTable::Defaults.
inline std::vector<std::pair<std::string, double>> AirFrontTuningDefaults() {
    return {
        // 1: the enemy's air front by sector (F2.3: the per-cycle measure of the believed cover and the losses by sector,
        // and the air staff's decision on them, journalled; the planner reads the depths from F2.4); 0: before it, nothing
        // built, measured or decided (a save without the key air_front_mode replays at 0).
        {"air_front.mode", 0.0},
        {"air_front.station_m", 5000.0},  // the line of contact's stations along the front's line
        {"air_front.step_m", 20000.0},    // the air front's step, and the depth of the band beyond it the cover is weighed over
        // F2.3's decision. A sector's gates open only once a band of it was believed covered by at least this much (the sum
        // of BelievedThere over its radar SAMs or its mobile air defence): no cover where nothing was ever believed is not
        // a hole the staff has made (the tuned district, where our SAMs stand 94 km behind the Pact's line).
        {"air_front.min_baseline", 1.0},
        {"air_front.min_sorties", 6.0},   // low-level sorties in the loss window before its rate counts (one loss in two is not a rate)
        {"air_front.helo_lag_h", 24.0},   // the helicopters follow a step only after this long of clean fixed-wing work
        {"air_front.trait_shift", 0.2},   // the most a chief's trait moves a gate, as a share of its value
    };
}

}  // namespace airwar
}  // namespace simcore
