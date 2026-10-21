// simcore/board_reading.hpp — the plotting board read the way the battery commander reads it (docs/research/
// PLOTTING_BOARD_2026-10.md, proposals 1 and 2).
//
// A track on the board is a row of crosses, each labelled with the minute it was plotted (bguir_tsp, the plotting
// symbols). From the two newest crosses of a track at least `min_gap_s` apart the commander has its course and speed by
// eye; from them how far it is, how fast it closes, when it reaches the post and when it reaches the edge of the
// launchers' zone. A track with one cross has a place and no speed. Nothing here reads the truth: only the marks.
#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {

// One cross on the board: whose track (the board's number or the scenario's id), where, and when it was plotted.
struct BoardFix {
    std::string key;
    int number = 0;
    Vec3 position;
    double time_s = 0.0;
};

struct BoardReading {
    std::string key;
    int number = 0;
    Vec3 position;            // the newest cross
    double time_s = 0.0;      // when it was plotted
    double range_m = 0.0;     // from the post (horizontal)
    bool has_speed = false;   // two crosses far enough apart
    Vec3 velocity_mps;        // course and speed, horizontal
    double closing_mps = 0.0; // toward the post (> 0 closing)
    double arrival_s = -1.0;  // to the post from the newest cross, -1 if not closing (or no speed)
    double to_zone_s = -1.0;  // to `zone_m` from the post, -1 if not closing; 0 if already inside
};

// Each track's reading at the post `at`. `zone_m`: the edge of the launchers' zone (0: the post itself).
inline std::vector<BoardReading> ReadBoard(const std::vector<BoardFix>& fixes, const Vec3& at, double zone_m = 0.0,
                                           double min_gap_s = 30.0) {
    std::vector<BoardReading> out;
    for (const BoardFix& f : fixes) {
        BoardReading* r = nullptr;
        for (BoardReading& o : out) {
            if (o.key == f.key) r = &o;
        }
        if (r == nullptr) {
            out.push_back(BoardReading{});
            r = &out.back();
            r->key = f.key;
            r->time_s = -1e18;
        }
        if (f.time_s >= r->time_s) {
            r->number = f.number;
            r->position = f.position;
            r->time_s = f.time_s;
        }
    }
    for (BoardReading& r : out) {
        // The older cross: the newest one at least min_gap_s before the newest.
        const BoardFix* older = nullptr;
        for (const BoardFix& f : fixes) {
            if (f.key != r.key || f.time_s > r.time_s - min_gap_s + 1e-9) continue;
            if (older == nullptr || f.time_s > older->time_s) older = &f;
        }
        const Vec3 rel = r.position - at;
        r.range_m = rel.HorizontalLength();
        if (older == nullptr) {
            r.to_zone_s = r.range_m <= zone_m ? 0.0 : -1.0;
            continue;
        }
        const double dt = r.time_s - older->time_s;
        r.has_speed = true;
        r.velocity_mps = Vec3{(r.position.x - older->position.x) / dt, (r.position.y - older->position.y) / dt, 0.0};
        r.closing_mps = r.range_m > 1.0 ? -(rel.x * r.velocity_mps.x + rel.y * r.velocity_mps.y) / r.range_m : 0.0;
        if (r.range_m <= zone_m) {
            r.to_zone_s = 0.0;
        } else if (r.closing_mps > 1.0) {
            r.to_zone_s = (r.range_m - zone_m) / r.closing_mps;
        }
        if (r.closing_mps > 1.0) r.arrival_s = r.range_m / r.closing_mps;
    }
    return out;
}

}  // namespace simcore
