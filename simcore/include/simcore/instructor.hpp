// simcore/instructor.hpp — the trainer's instructor (roadmap 10.3).
//
// The 9Ф632 trainer (a Ural-375 with every workplace of the vehicle and an instructor's seat; it could be coupled to a
// real vehicle) let the instructor enter the air picture and change up to two targets' behaviour while the crew worked,
// with ECCM drills among its exercises (flak11; ePap in samsim_thread; docs/research/OSA_RESEARCH_2026-10.md section 7).
// SAM3D's instructor is a second player, or the player between drills, or a scenario's scripted drill (trigger action
// "instructor"): he takes a target and turns it, sends it up or down, changes its speed, makes it manoeuvre, switches its
// jammer, lets it go back to its route, or enters a new target into the picture. Everything he does is in the debrief
// under "Instructor"; the crew knows of it only what its instruments show.
#pragma once

#include <string>

namespace simcore {

struct InstructorOrder {
    enum class Kind { Take, Release, Turn, Height, Speed, Evade, Jam };
    Kind kind = Kind::Take;
    std::string target;
    // Turn: the true heading, degrees from north; Height: metres above the ground; Speed: m/s; Jam: 1 on, 0 off.
    double value = 0.0;
};

// The trainer changes up to two targets' behaviour at once (flak11, ePap).
constexpr int kInstructorTargets = 2;

inline const char* ToString(InstructorOrder::Kind k) {
    switch (k) {
        case InstructorOrder::Kind::Take: return "take";
        case InstructorOrder::Kind::Release: return "release";
        case InstructorOrder::Kind::Turn: return "turn";
        case InstructorOrder::Kind::Height: return "height";
        case InstructorOrder::Kind::Speed: return "speed";
        case InstructorOrder::Kind::Evade: return "evade";
        case InstructorOrder::Kind::Jam: return "jam";
    }
    return "?";
}

inline bool InstructorKindFrom(const std::string& s, InstructorOrder::Kind& out) {
    for (InstructorOrder::Kind k : {InstructorOrder::Kind::Take, InstructorOrder::Kind::Release, InstructorOrder::Kind::Turn,
                                    InstructorOrder::Kind::Height, InstructorOrder::Kind::Speed, InstructorOrder::Kind::Evade,
                                    InstructorOrder::Kind::Jam}) {
        if (s == ToString(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

// Does the order need a value (a heading, a height, a speed, on or off)?
inline bool InstructorOrderHasValue(InstructorOrder::Kind k) {
    return k == InstructorOrder::Kind::Turn || k == InstructorOrder::Kind::Height || k == InstructorOrder::Kind::Speed ||
           k == InstructorOrder::Kind::Jam;
}

}  // namespace simcore
