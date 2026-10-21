// simcore/crew_training.hpp — in the crew: the training overlay's levels and what it shows (roadmap 8.7,
// docs/crew/08_TRAINING_AND_ASSISTANCE.md §2).
//
//   AssistLevel   Demonstrate (the seat's NPC does it, the overlay names what he moves and why), Guided (the drill
//                 master's best next move for the player's seat), Hint (the same, once the player hesitates longer than
//                 an NPC's patience at that seat), Unassisted (no overlay; the NPCs still remind), Exam (unassisted, graded)
//   AssistView    what the overlay shows for a seat now (ScenarioRunner::Assistance)
//   MoveVerdict   a player's move judged against the drill master's best: the best move and the seconds his cost
#pragma once

#include <string>
#include <vector>

namespace simcore {

enum class AssistLevel { Demonstrate, Guided, Hint, Unassisted, Exam };

inline const char* ToString(AssistLevel l) {
    switch (l) {
        case AssistLevel::Demonstrate: return "demonstrate";
        case AssistLevel::Guided: return "guided";
        case AssistLevel::Hint: return "hint";
        case AssistLevel::Unassisted: return "unassisted";
        case AssistLevel::Exam: return "exam";
    }
    return "?";
}

inline bool ParseAssistLevel(const std::string& s, AssistLevel& out) {
    for (AssistLevel l : {AssistLevel::Demonstrate, AssistLevel::Guided, AssistLevel::Hint, AssistLevel::Unassisted, AssistLevel::Exam}) {
        if (s == ToString(l)) {
            out = l;
            return true;
        }
    }
    return false;
}

// What the overlay shows for a seat.
struct AssistView {
    bool show = false;
    std::string task;           // the task it aims at (ready_halted, captured, launched)
    std::string control;        // the control to touch, and the position
    int position = 0;
    std::string description;
    std::vector<std::string> why;  // nearest reason first, up to the goal
    double due_in_s = 0.0;
    bool demonstrating = false;    // the NPC does it; the overlay names what he moves
};

// A player's move at a seat, judged when he made it.
struct MoveVerdict {
    double time_s = 0.0;
    std::string station;
    std::string control;
    int position = 0;
    std::string task;
    bool best = true;               // the drill master's best move (or as good: it costs nothing)
    std::string best_control;       // what the best move was
    int best_position = 0;
    std::string best_description;
    double cost_s = 0.0;            // how much later the task is done than after the best move
};

}  // namespace simcore
