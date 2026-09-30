// simcore/panel.hpp — the physical controls of an operator console.
//
// A ControlPanel only records where each switch, knob or rotary physically is.
// What those positions *do* (warm-ups, deploy times, faults) lives in Equipment
// (equipment.hpp); who moves them and in what order lives in procedures.hpp.
//
// Electrical logic never blocks a control. Only physical constraints do, and
// the SAM Simulator Osa panels have both kinds modelled here:
//   * guarded controls: a hinged cover (itself a control) must be open first;
//   * stepwise controls: detented rotaries and key switches move one position at
//     a time (key out -> key in -> key turned).
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace simcore {

struct ControlDef {
    std::string id;       // stable identifier, e.g. "srch.hv_power"
    std::string label;    // what's printed on the panel (Cyrillic OK)
    int positions = 2;    // 2 = toggle; >2 = rotary / multi-position
    int initial = 0;
    std::string guard_id;  // cover control that must be open (position >= 1) before this one moves
    bool stepwise = false; // may only move to an adjacent position
};

class ControlPanel {
public:
    // The guard, if any, must be added first.
    bool AddControl(const ControlDef& def) {
        if (def.id.empty() || def.positions < 1 || index_.count(def.id) != 0) return false;
        if (def.initial < 0 || def.initial >= def.positions) return false;
        if (!def.guard_id.empty() && !Has(def.guard_id)) return false;
        index_[def.id] = defs_.size();
        defs_.push_back(def);
        values_.push_back(def.initial);
        return true;
    }

    bool Has(const std::string& id) const { return index_.count(id) != 0; }

    // Current position, or -1 if the control doesn't exist.
    int Get(const std::string& id) const {
        const auto it = index_.find(id);
        return it == index_.end() ? -1 : values_[it->second];
    }

    bool IsValidPosition(const std::string& id, int value) const {
        const auto it = index_.find(id);
        return it != index_.end() && value >= 0 && value < defs_[it->second].positions;
    }

    // A valid position that isn't physically blocked right now (closed cover, or a stepwise jump).
    bool CanMove(const std::string& id, int value) const {
        if (!IsValidPosition(id, value)) return false;
        const std::size_t i = index_.at(id);
        const ControlDef& def = defs_[i];
        if (!def.guard_id.empty() && Get(def.guard_id) < 1) return false;
        if (def.stepwise && (value > values_[i] + 1 || value < values_[i] - 1)) return false;
        return true;
    }

    bool IsBlocked(const std::string& id, int value) const { return IsValidPosition(id, value) && !CanMove(id, value); }

    // Physically move a control. Returns false for unknown controls, invalid positions, or physical blocks.
    bool Set(const std::string& id, int value) {
        if (!CanMove(id, value)) return false;
        values_[index_.at(id)] = value;
        return true;
    }

    const std::vector<ControlDef>& Controls() const { return defs_; }

private:
    std::unordered_map<std::string, std::size_t> index_;
    std::vector<ControlDef> defs_;
    std::vector<int> values_;
};

}  // namespace simcore
