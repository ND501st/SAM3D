// simcore/seat_duty.hpp — what a seat's NPC would do now, said as the move a player at that seat makes (the helper,
// docs/crew/13_HELPER.md).
//
// The NPC crew's behaviour lives in ScenarioRunner, each act behind a check that an NPC sits at the seat. When a player
// holds the seat the NPC doesn't act; the runner records the act instead, as a SeatDuty the player now owes
// (ScenarioRunner::NpcDoes). The helper (simcore/helper.hpp) shows the player his seat's duties, so whatever an NPC does
// at a seat, the helper can tell the player who sits there to do it, without a second copy of the NPC's rules.
//
//   Report     say a callout of the crew data (the crew menu's report line)          id: callout, fields: its words
//   Order      give an order of the crew data (the commander's order menu)            id: order, target: what it is about
//   Drill      work through a procedure of the console (its next undone step)         id: procedure
//   Control    set a switch, selector, key or button                                  id: control, number: position
//   Wheel      bring a hand wheel onto a value                                        id: wheel, value, tolerance, wraps
//   Designate  give a launcher a target (the battery commander, the battery net)      id: launcher callsign, number: target
//   Plot       write a target on the post's plotting board (the operator-plotter)     number: the report's target number
//   Answer     answer a ringing line or telephone                                     id: the line's number
//   Halt       stop the vehicle (the driver)
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/crew_knowledge.hpp"
#include "simcore/equipment.hpp"

namespace simcore {

struct SeatDuty {
    enum class Kind { Report, Order, Drill, Control, Wheel, Designate, Plot, Answer, Halt };

    Kind kind = Kind::Report;
    std::string seat;
    std::string id;
    int number = 0;
    double value = 0.0;
    double tolerance = 0.0;
    bool wraps = false;
    std::string target;                          // the target it is about ("" none)
    std::map<std::string, std::string> fields;   // a report's or an order's spoken words ({azimuth}, {range_km}, ...)
    bool has_about = false;                      // a report: what the crew learns from it (the target, its IFF reply)
    KnownTarget about;
    std::vector<Condition> holds_while;          // dropped once these stop holding (an automatic report's lamp)
    double keep_s = 30.0;                        // dropped this long after the NPC last would have done it
    std::string rule;                            // which NPC rule it comes from (the coverage map; the hint's "because")
    // Kept by the runner.
    double since_s = 0.0;  // when the NPC first would have done it
    double last_s = 0.0;   // when it last would have (still due)

    // What the crew learns from it; a report about one target is a duty of its own (two echoes: two reports).
    SeatDuty& About(const KnownTarget& k) {
        has_about = true;
        about = k;
        if (target.empty()) target = k.target;
        return *this;
    }

    // Two duties are the same when they ask for the same thing.
    std::string Key() const {
        return std::to_string(static_cast<int>(kind)) + "|" + seat + "|" + id + "|" + target + "|" + std::to_string(number);
    }

    static SeatDuty Make(Kind k, std::string seat_id, std::string what, std::string rule_text) {
        SeatDuty d;
        d.kind = k;
        d.seat = std::move(seat_id);
        d.id = std::move(what);
        d.rule = std::move(rule_text);
        return d;
    }
    static SeatDuty Report(std::string seat_id, std::string callout, std::string rule_text,
                           std::map<std::string, std::string> words = {}) {
        SeatDuty d = Make(Kind::Report, std::move(seat_id), std::move(callout), std::move(rule_text));
        d.fields = std::move(words);
        return d;
    }
    static SeatDuty Order(std::string seat_id, std::string order, std::string rule_text, std::string about = std::string()) {
        SeatDuty d = Make(Kind::Order, std::move(seat_id), std::move(order), std::move(rule_text));
        d.target = std::move(about);
        return d;
    }
    static SeatDuty Drill(std::string seat_id, std::string procedure, std::string rule_text) {
        return Make(Kind::Drill, std::move(seat_id), std::move(procedure), std::move(rule_text));
    }
    static SeatDuty Control(std::string seat_id, std::string control, int position, std::string rule_text) {
        SeatDuty d = Make(Kind::Control, std::move(seat_id), std::move(control), std::move(rule_text));
        d.number = position;
        return d;
    }
    static SeatDuty Wheel(std::string seat_id, std::string wheel, double to, double within, bool angle, std::string rule_text) {
        SeatDuty d = Make(Kind::Wheel, std::move(seat_id), std::move(wheel), std::move(rule_text));
        d.value = to;
        d.tolerance = within;
        d.wraps = angle;
        return d;
    }
    static SeatDuty Designate(std::string seat_id, std::string callsign, int target_number, std::string about, std::string rule_text) {
        SeatDuty d = Make(Kind::Designate, std::move(seat_id), std::move(callsign), std::move(rule_text));
        d.number = target_number;
        d.target = std::move(about);
        return d;
    }
    static SeatDuty Plot(std::string seat_id, int target_number, std::string about, std::string rule_text) {
        SeatDuty d = Make(Kind::Plot, std::move(seat_id), std::string(), std::move(rule_text));
        d.number = target_number;
        d.target = std::move(about);
        return d;
    }
    static SeatDuty Answer(std::string seat_id, int line, std::string rule_text) {
        SeatDuty d = Make(Kind::Answer, std::move(seat_id), std::to_string(line), std::move(rule_text));
        d.number = line;
        return d;
    }
    static SeatDuty Halt(std::string seat_id, std::string rule_text) {
        return Make(Kind::Halt, std::move(seat_id), std::string(), std::move(rule_text));
    }
};

inline const char* ToString(SeatDuty::Kind k) {
    switch (k) {
        case SeatDuty::Kind::Report: return "report";
        case SeatDuty::Kind::Order: return "order";
        case SeatDuty::Kind::Drill: return "drill";
        case SeatDuty::Kind::Control: return "control";
        case SeatDuty::Kind::Wheel: return "wheel";
        case SeatDuty::Kind::Designate: return "designate";
        case SeatDuty::Kind::Plot: return "plot";
        case SeatDuty::Kind::Answer: return "answer";
        case SeatDuty::Kind::Halt: return "halt";
    }
    return "?";
}

}  // namespace simcore
