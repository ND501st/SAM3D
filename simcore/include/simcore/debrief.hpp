// simcore/debrief.hpp — what happened on the consoles during a session, for the debrief.
//
// A SessionLog collects, on one session clock:
//   * crew reports (started, completed, waiting, equipment fault, aborted),
//   * equipment faults (and optionally every subsystem transition),
//   * procedure deviations by the player or crew (early or out-of-order actions),
// and turns them into a timeline plus a short summary. Nothing is scored with
// points: SAM Simulator's rule is "no points, as in reality" (its FAQ). The
// debrief shows what was done, when, and what went wrong.
//
// Call the Collect functions once per sim step (after Equipment::Update and the
// crew updates) and RecordAttempt after each player action.
#pragma once

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/equipment.hpp"
#include "simcore/procedures.hpp"

namespace simcore {

struct DebriefEntry {
    enum class Kind { Started, Completed, Waiting, Aborted, EquipmentFault, Deviation, Transition };
    double time_s = 0.0;
    Kind kind = Kind::Started;
    std::string who;     // crew member or player name; empty for equipment
    std::string what;    // procedure or subsystem id
    std::string detail;  // e.g. the control moved, the subsystem waited on, the new state
};

inline const char* ToString(DebriefEntry::Kind k) {
    switch (k) {
        case DebriefEntry::Kind::Started: return "started";
        case DebriefEntry::Kind::Completed: return "completed";
        case DebriefEntry::Kind::Waiting: return "waiting";
        case DebriefEntry::Kind::Aborted: return "aborted";
        case DebriefEntry::Kind::EquipmentFault: return "FAULT";
        case DebriefEntry::Kind::Deviation: return "DEVIATION";
        case DebriefEntry::Kind::Transition: return "state";
    }
    return "?";
}

struct DebriefSummary {
    int procedures_completed = 0;
    int procedures_aborted = 0;
    int early_actions = 0;
    int out_of_order_actions = 0;
    int equipment_faults = 0;
};

class SessionLog {
public:
    // Also log every subsystem state change, not just faults (noisy; useful for debugging).
    explicit SessionLog(bool log_all_transitions = false) : log_all_transitions_(log_all_transitions) {}

    void Record(DebriefEntry entry) { entries_.push_back(std::move(entry)); }

    // Drains the equipment's transitions. They carry the equipment's own clock, which should be the session clock.
    void CollectEquipment(Equipment& equipment) {
        for (SubsystemTransition& tr : equipment.TakeTransitions()) {
            if (tr.to == SubsystemState::Fault) {
                Record({tr.time_s, DebriefEntry::Kind::EquipmentFault, std::string(), tr.id, ToString(tr.from)});
            } else if (log_all_transitions_) {
                Record({tr.time_s, DebriefEntry::Kind::Transition, std::string(), tr.id, ToString(tr.to)});
            }
        }
    }

    // Drains a crew member's reports, stamping them with the session time.
    void CollectCrew(CrewMember& crew, double now_s) {
        for (CrewReport& r : crew.TakeReports()) {
            DebriefEntry::Kind kind = DebriefEntry::Kind::Started;
            switch (r.kind) {
                case CrewReport::Kind::Started: kind = DebriefEntry::Kind::Started; break;
                case CrewReport::Kind::Completed: kind = DebriefEntry::Kind::Completed; break;
                case CrewReport::Kind::Waiting: kind = DebriefEntry::Kind::Waiting; break;
                case CrewReport::Kind::EquipmentFault: kind = DebriefEntry::Kind::EquipmentFault; break;
                case CrewReport::Kind::Aborted: kind = DebriefEntry::Kind::Aborted; break;
            }
            Record({now_s, kind, crew.Name(), std::move(r.procedure_id), std::move(r.detail)});
        }
    }

    // After a player's ProcedureTracker::Attempt: logs the deviation it produced, if any.
    void RecordAttempt(double now_s, const std::string& who, const ProcedureTracker& tracker, StepResult result) {
        if (result != StepResult::AcceptedEarly && result != StepResult::NotNextStep) return;
        if (tracker.DeviationLog().empty()) return;
        const Deviation& d = tracker.DeviationLog().back();
        std::string detail = d.control_id + " -> " + std::to_string(d.value);
        detail += d.kind == DeviationKind::Early ? " (before its lamp/condition)" : " (out of order)";
        const std::vector<ProcedureStep>& steps = tracker.GetProcedure().steps;
        if (d.expected_step < steps.size()) detail += "; step due: " + steps[d.expected_step].description;
        Record({now_s, DebriefEntry::Kind::Deviation, who, tracker.GetProcedure().id, std::move(detail)});
        if (d.kind == DeviationKind::Early) {
            ++early_;
        } else {
            ++out_of_order_;
        }
    }

    const std::vector<DebriefEntry>& Entries() const { return entries_; }

    DebriefSummary Summary() const {
        DebriefSummary s;
        s.early_actions = early_;
        s.out_of_order_actions = out_of_order_;
        for (const DebriefEntry& e : entries_) {
            if (e.kind == DebriefEntry::Kind::Completed) ++s.procedures_completed;
            if (e.kind == DebriefEntry::Kind::Aborted) ++s.procedures_aborted;
            if (e.kind == DebriefEntry::Kind::EquipmentFault && e.who.empty()) ++s.equipment_faults;
        }
        return s;
    }

    // Plain-text timeline and summary, in time order (stable for equal times).
    std::string Format() const {
        std::vector<const DebriefEntry*> order;
        for (const DebriefEntry& e : entries_) order.push_back(&e);
        for (std::size_t i = 1; i < order.size(); ++i) {  // insertion sort: stable, and logs are nearly sorted
            const DebriefEntry* e = order[i];
            std::size_t j = i;
            while (j > 0 && order[j - 1]->time_s > e->time_s) {
                order[j] = order[j - 1];
                --j;
            }
            order[j] = e;
        }
        std::string out;
        char line[512];
        for (const DebriefEntry* e : order) {
            const std::string who = e->who.empty() ? std::string("equipment") : e->who;
            std::snprintf(line, sizeof(line), "%8.1f s  %-16s %-10s %s%s%s\n", e->time_s, who.c_str(), ToString(e->kind),
                          e->what.c_str(), e->detail.empty() ? "" : "  ", e->detail.c_str());
            out += line;
        }
        const DebriefSummary s = Summary();
        std::snprintf(line, sizeof(line),
                      "\nProcedures completed: %d, aborted: %d. Deviations: %d early, %d out of order. "
                      "Equipment faults: %d.\n",
                      s.procedures_completed, s.procedures_aborted, s.early_actions, s.out_of_order_actions,
                      s.equipment_faults);
        out += line;
        return out;
    }

private:
    bool log_all_transitions_;
    std::vector<DebriefEntry> entries_;
    int early_ = 0;
    int out_of_order_ = 0;
};

}  // namespace simcore
