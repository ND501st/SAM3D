// simcore/air_levels.hpp — the aircraft of a session at levels of detail (roadmap B12,
// docs/air_battle/02_FLIGHTS_AND_PILOTS.md §5).
//
// A2, full: every aircraft flown by R3's model with its own pilot, perception, receiver and reactions. A flight is at A2
// near any of our units (within the reach of our radars or eyes, plus a margin) or engaged (a missile of ours at it, a
// launch, a burst or a lock in its pilots' pictures within the last minute, a pilot manoeuvring or in an attack pass).
// A1, coarse: the leader flown by R3's model and thinking at his own rates; the wingmen placed on their slots from the
// leader each step, their receivers off and their eyes shut (the leader looks for the flight). Their emitters radiate as
// at A2 (what the enemy emits is what our ESM hears). A0, abstract: the campaign's time-stepped resolver
// (air_war/resolve.hpp), outside the session.
//
// Whole flights change level together. Up to A2 at once; down to A1 only past `down_factor` times the near range and after
// `down_dwell_s` there, so a route that runs along the edge does not flicker. Levels follow simulation facts, never the
// camera, so a headless run, the game and a server agree. Everything a flight is (route place, order, stores, fuel,
// damage, the pilots' stress and pictures) stays where it is through a change: the level only says who is stepped how.
//
// The stagger (§5.2): a task at rate f runs in the step that takes the clock past a tick of floor(t f + phase), the phase
// from a fixed serial's hash, so it runs at the same simulation times whatever the step (0.05 s in the game, 0.1 s headless).
#pragma once

#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/em/random.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

enum class AirLevel : int { A0 = 0, A1 = 1, A2 = 2 };

inline const char* AirLevelName(AirLevel l) {
    switch (l) {
        case AirLevel::A0: return "A0";
        case AirLevel::A1: return "A1";
        case AirLevel::A2: return "A2";
    }
    return "?";
}

// data/gameplay/air_levels.json (game values). Without the file every aircraft of a session is at A2, as before B12.
struct AirLevelValues {
    int mode = 0;                     // 0: every aircraft at A2; 1: flights far from our units at A1
    double radar_m = 45000.0;         // our launchers' radars: the session's own vehicle, the battery's, the neighbours'
    double post_radar_m = 0.0;        // the PU-12M's radar (0: not counted)
    double fighter_radar_m = 30000.0; // our fighters in the session
    double ground_m = 40000.0;        // our ground objects a pilot may find (the session's movers, the dummies)
    double margin_m = 10000.0;        // added to every reach
    double down_factor = 1.2;         // down to A1 only beyond this times the near range...
    double down_dwell_s = 30.0;       // ...and after this long there
    double engaged_s = 60.0;          // a launch, a burst or a lock seen within this keeps the flight at A2
};

// The stagger's phase in [0, 1) for a fixed serial.
inline double StaggerPhase(std::uint64_t serial) {
    return em::Uniform01(em::Mix64(serial ^ 0xB12A1E7E15ull));
}
// Is a task at `hz` due in the step from `t_prev` to `t`: has floor(t hz + phase) moved on?
inline bool StaggerDue(std::uint64_t serial, double hz, double t_prev, double t) {
    if (hz <= 0.0) return false;
    const double ph = StaggerPhase(serial);
    return std::floor(t * hz + ph + 1e-9) > std::floor(t_prev * hz + ph + 1e-9);
}

// One of our units' reach: a point and how far from it an aircraft is near.
struct NearPoint {
    Vec3 at;
    double reach_m = 0.0;
};

// The near ratio of a position: its horizontal distance to each point over that point's reach (with the margin), the
// smallest. <= 1: near. With no points, far (1e9).
inline double NearRatio(const Vec3& p, const std::vector<NearPoint>& points, double margin_m) {
    double best = 1e9;
    for (const NearPoint& n : points) {
        const double reach = n.reach_m + margin_m;
        if (reach <= 0.0) continue;
        const double d = std::hypot(p.x - n.at.x, p.y - n.at.y);
        if (d < best * reach) best = d / reach;
    }
    return best;
}

// The levels of the flights of a session, with their hysteresis. Indexed by the flight's index in `Flights`.
class AirLevels {
public:
    struct Entry {
        AirLevel level = AirLevel::A2;
        bool seen = false;
        double far_since_s = -1.0;  // since when it has been beyond down_factor times the near range (< 0: not)
        int changes = 0;            // how often it changed level
    };

    void Configure(const AirLevelValues& v) { v_ = v; }
    const AirLevelValues& Values() const { return v_; }
    bool On() const { return v_.mode != 0; }

    // The flight's level after this step: `ratio` its nearest member's near ratio, `engaged` as the header says. Returns
    // true when its level is not what it was (its first, A1, as it comes into the sky far away, too; `changes` does not
    // count that one).
    bool Update(int flight, double ratio, bool engaged, double t) {
        if (flight < 0) return false;
        if (static_cast<std::size_t>(flight) >= e_.size()) e_.resize(static_cast<std::size_t>(flight) + 1);
        Entry& e = e_[static_cast<std::size_t>(flight)];
        AirLevel next = e.level;
        if (!On() || engaged || ratio <= 1.0) {
            next = AirLevel::A2;
            e.far_since_s = -1.0;
        } else if (ratio > v_.down_factor) {
            if (!e.seen) {
                next = AirLevel::A1;  // it enters the session far away: no history to wait for
            } else {
                if (e.far_since_s < 0.0) e.far_since_s = t;
                if (t - e.far_since_s + 1e-9 >= v_.down_dwell_s) next = AirLevel::A1;
            }
        } else {
            e.far_since_s = -1.0;  // between the near range and its margin: as it is
        }
        const bool was_seen = e.seen;
        e.seen = true;
        if (next == e.level) return false;
        e.level = next;
        if (was_seen) ++e.changes;  // its first level, as it comes into the sky, is not a change
        return true;
    }
    AirLevel LevelOf(int flight) const {
        if (flight < 0 || static_cast<std::size_t>(flight) >= e_.size()) return AirLevel::A2;
        return e_[static_cast<std::size_t>(flight)].level;
    }
    const Entry* EntryOf(int flight) const {
        if (flight < 0 || static_cast<std::size_t>(flight) >= e_.size()) return nullptr;
        return &e_[static_cast<std::size_t>(flight)];
    }
    std::size_t Size() const { return e_.size(); }

private:
    AirLevelValues v_;
    std::vector<Entry> e_;
};

namespace detail {

class AirLevelLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, AirLevelValues& out) {
        if (!Keys(root, "", {"air_levels_id", "status", "notes", "source", "confidence", "values"}) || !Provenance(root, "")) return false;
        const json::Value* values = root.Find("values");
        if (values == nullptr) return Fail("values", "required");
        double mode = 0.0;
        const std::pair<const char*, double*> each[] = {{"mode", &mode},
                                                        {"radar_m", &out.radar_m},
                                                        {"post_radar_m", &out.post_radar_m},
                                                        {"fighter_radar_m", &out.fighter_radar_m},
                                                        {"ground_m", &out.ground_m},
                                                        {"margin_m", &out.margin_m},
                                                        {"down_factor", &out.down_factor},
                                                        {"down_dwell_s", &out.down_dwell_s},
                                                        {"engaged_s", &out.engaged_s}};
        if (!Keys(*values, "values", {"mode", "radar_m", "post_radar_m", "fighter_radar_m", "ground_m", "margin_m", "down_factor", "down_dwell_s", "engaged_s"})) {
            return false;
        }
        for (const auto& e : each) {
            const std::string p = Join("values", e.first);
            const json::Value* v = values->Find(e.first);
            if (v == nullptr) return Fail(p, "required");
            if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number >= 0.0)) return Fail(Join(p, "value"), "a number >= 0");
            *e.second = n->number;
        }
        if (mode != 0.0 && mode != 1.0) return Fail("values.mode.value", "0 (every aircraft at A2) or 1 (the levels)");
        out.mode = static_cast<int>(mode);
        if (out.down_factor < 1.0) return Fail("values.down_factor.value", "at least 1");
        return true;
    }
};

}  // namespace detail

// Reads `rel` under data_dir. A data folder without it keeps every aircraft at A2 (true, `out` untouched).
inline bool LoadAirLevelValues(const std::string& data_dir, const std::string& rel, AirLevelValues& out, std::string* error = nullptr) {
    std::ifstream f(data_dir + "/" + rel, std::ios::binary);
    if (!f) return true;
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "air levels: " + e;
        return false;
    }
    AirLevelValues v;
    detail::AirLevelLoader loader;
    if (!loader.Load(root, v)) {
        if (error != nullptr) *error = "air levels: " + loader.Message();
        return false;
    }
    out = v;
    return true;
}

}  // namespace simcore
