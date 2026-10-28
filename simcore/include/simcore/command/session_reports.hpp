// simcore/command/session_reports.hpp — what a crew called in a played fight, as a fact of the chain (roadmap P4,
// docs/command/06_INTEGRATION.md §2).
//
// The crew's knowledge model (crew_knowledge.hpp) is the pipeline at the scale of one vehicle and one fight. Its calls
// leave the vehicle as its commander's report: each missile launched and each result his judge called, right or wrong
// (the crew's word, never the truth: docs/command/01_PRINCIPLES.md 9). The report starts with him at the hour he made it
// and goes up the chain like anyone's; the battery commander has it as the pipeline brings it, and reports it up as he
// tells it (P2). When the player's character is the vehicle commander, the report is his own: he tells it himself.
#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "simcore/command/info_net.hpp"
#include "simcore/command/scenes.hpp"
#include "simcore/scenario_runner.hpp"

namespace simcore {
namespace command {

// The commander of `vehicle` ("osa") reports his crew's calls of a session that started at campaign hour start_h. False
// (nothing to report) without calls or without a post at the vehicle's commander's seat.
inline bool CommanderReport(const InfoNet& net, const std::vector<ScenarioRunner::CrewCall>& calls, const std::string& vehicle, double start_h,
                       const std::string& key, Fact& out) {
    const ChainData& data = net.Data();
    const int cmdr = PositionAtSeat(data, vehicle + ":commander");
    if (calls.empty() || cmdr < 0) return false;
    const Position& p = data.positions[static_cast<std::size_t>(cmdr)];
    int launched = 0, destroyed = 0;
    std::string text = p.Label() + " reports:";
    for (const ScenarioRunner::CrewCall& c : calls) {
        const std::string at = FormatClock(start_h + c.time_s / 3600.0, net.StartHour());
        const std::string target = c.target.empty() ? std::string("a target") : "target " + c.target;
        if (c.kind == "launch") {
            ++launched;
            text += " " + at + " a missile launched at " + target + ";";
        } else if (c.kind == "destroyed") {
            ++destroyed;
            text += " " + at + " " + target + " destroyed;";
        } else if (c.kind == "missed") {
            text += " " + at + " " + target + " missed;";
        } else {
            text += " " + at + " " + target + ": the result not seen;";
        }
    }
    text.back() = '.';
    out = Fact();
    out.key = key;
    out.kind = FactKind::RaidResult;
    out.h = start_h + calls.back().time_s / 3600.0;
    out.origin = p.id;
    for (std::size_t i = 0; i < data.positions.size(); ++i) {
        std::string v, st;
        if (static_cast<int>(i) != cmdr && SplitSeat(data.positions[i].seat, v, st) && v == vehicle) out.witnesses.push_back(data.positions[i].id);
    }
    out.text = text;
    char summary[160];
    std::snprintf(summary, sizeof(summary), "%s: %d missile%s launched, %d target%s destroyed.", p.title.c_str(), launched, launched == 1 ? "" : "s",
                  destroyed, destroyed == 1 ? "" : "s");
    out.summary = summary;
    out.audience = data.unit_regiment;
    out.need_level = 3;  // to the battery commander, who reports it on
    out.precedence = 1;
    out.held = net.IsPlayer(cmdr);
    return true;
}

}  // namespace command
}  // namespace simcore
