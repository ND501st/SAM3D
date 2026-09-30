// simcore/panel.hpp — the physical controls of an operator console.
//
// A ControlPanel only records where each switch, knob, wheel or button physically
// is. What those positions *do* (warm-ups, faults, modes) lives in Equipment
// (equipment.hpp); who moves them and in what order lives in procedures.hpp.
//
// Control kinds, all seen on SAM Simulator's Osa consoles (docs/re/osa.md):
//   * Discrete: toggles, rotary selectors, covers, locks and key switches.
//   * Continuous: hand wheels (azimuth, elevation, range gate) with a real value.
//   * Momentary: push-buttons. A press reads as position 1 until the next
//     ReleaseMomentaries(), which the sim loop calls once per step right after
//     Equipment::Update, so Equipment sees every press exactly once.
//
// Electrical logic never blocks a control. Only physical constraints do:
//   * guarded controls: a cover or lock (itself a control) must be open first;
//   * stepwise controls: detented rotaries and key switches move one position at
//     a time (key out -> key in -> key turned).
#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace simcore {

enum class ControlKind { Discrete, Continuous, Momentary };

struct ControlDef {
    std::string id;        // stable identifier, e.g. "srch.hv_power"
    std::string label;     // what's printed on the panel (Cyrillic OK)
    int positions = 2;     // Discrete: 2 = toggle, >2 = rotary. Momentary: always 2 (released, pressed)
    int initial = 0;
    std::string guard_id;  // cover/lock control that must be open (position >= 1) before this one moves
    bool stepwise = false; // Discrete: may only move to an adjacent position
    ControlKind kind = ControlKind::Discrete;
    double min_value = 0.0;      // Continuous: range, in the unit the data file states
    double max_value = 1.0;
    double initial_value = 0.0;
    bool wraps = false;          // Continuous: wraps around (e.g. a 360-degree dial) instead of stopping
    std::string station;                      // which operator position it belongs to
    std::vector<std::string> position_labels; // Discrete: text for each position, for UI and voice lines
};

class ControlPanel {
public:
    // The guard, if any, must be added first.
    bool AddControl(const ControlDef& def) {
        if (def.id.empty() || index_.count(def.id) != 0) return false;
        if (!def.guard_id.empty()) {
            const ControlDef* guard = Find(def.guard_id);
            if (guard == nullptr || guard->kind != ControlKind::Discrete) return false;
        }
        switch (def.kind) {
            case ControlKind::Discrete:
                if (def.positions < 1 || def.initial < 0 || def.initial >= def.positions) return false;
                break;
            case ControlKind::Momentary:
                if (def.positions != 2 || def.initial != 0 || def.stepwise) return false;
                break;
            case ControlKind::Continuous:
                if (!(def.max_value > def.min_value) || def.stepwise) return false;
                if (def.initial_value < def.min_value || def.initial_value > def.max_value) return false;
                break;
        }
        if (!def.position_labels.empty() && def.position_labels.size() != static_cast<std::size_t>(def.positions)) {
            return false;
        }
        index_[def.id] = defs_.size();
        defs_.push_back(def);
        values_.push_back(def.kind == ControlKind::Continuous ? -1 : def.initial);
        analog_.push_back(def.kind == ControlKind::Continuous ? def.initial_value : 0.0);
        return true;
    }

    bool Has(const std::string& id) const { return index_.count(id) != 0; }

    const ControlDef* Find(const std::string& id) const {
        const auto it = index_.find(id);
        return it == index_.end() ? nullptr : &defs_[it->second];
    }

    // Current position, or -1 if the control doesn't exist or is continuous.
    int Get(const std::string& id) const {
        const auto it = index_.find(id);
        return it == index_.end() ? -1 : values_[it->second];
    }

    // Continuous value, or the position as a number for other kinds; 0 for unknown controls.
    double GetValue(const std::string& id) const {
        const auto it = index_.find(id);
        if (it == index_.end()) return 0.0;
        return defs_[it->second].kind == ControlKind::Continuous ? analog_[it->second]
                                                                  : static_cast<double>(values_[it->second]);
    }

    bool IsValidPosition(const std::string& id, int value) const {
        const auto it = index_.find(id);
        if (it == index_.end()) return false;
        const ControlDef& def = defs_[it->second];
        return def.kind != ControlKind::Continuous && value >= 0 && value < def.positions;
    }

    // A valid position that isn't physically blocked right now (closed cover, or a stepwise jump).
    bool CanMove(const std::string& id, int value) const {
        if (!IsValidPosition(id, value)) return false;
        const std::size_t i = index_.at(id);
        const ControlDef& def = defs_[i];
        if (GuardClosed(def)) return false;
        if (def.stepwise && (value > values_[i] + 1 || value < values_[i] - 1)) return false;
        return true;
    }

    bool IsBlocked(const std::string& id, int value) const { return IsValidPosition(id, value) && !CanMove(id, value); }

    // Physically move a discrete control or press/release a button.
    // Returns false for unknown or continuous controls, invalid positions, or physical blocks.
    bool Set(const std::string& id, int value) {
        if (!CanMove(id, value)) return false;
        values_[index_.at(id)] = value;
        return true;
    }

    // Turn a wheel to a value: clamped to its range, or wrapped if it wraps.
    // Returns false for unknown or non-continuous controls, or when its lock is closed.
    bool SetValue(const std::string& id, double value) {
        const auto it = index_.find(id);
        if (it == index_.end()) return false;
        const ControlDef& def = defs_[it->second];
        if (def.kind != ControlKind::Continuous || GuardClosed(def) || !std::isfinite(value)) return false;
        const double span = def.max_value - def.min_value;
        if (def.wraps) {
            value = def.min_value + std::fmod(value - def.min_value, span);
            if (value < def.min_value) value += span;
        } else if (value < def.min_value) {
            value = def.min_value;
        } else if (value > def.max_value) {
            value = def.max_value;
        }
        analog_[it->second] = value;
        return true;
    }

    bool Nudge(const std::string& id, double delta) { return SetValue(id, GetValue(id) + delta); }

    // Let go of every push-button. Call once per sim step, right after Equipment::Update.
    void ReleaseMomentaries() {
        for (std::size_t i = 0; i < defs_.size(); ++i) {
            if (defs_[i].kind == ControlKind::Momentary) values_[i] = 0;
        }
    }

    const std::vector<ControlDef>& Controls() const { return defs_; }

private:
    bool GuardClosed(const ControlDef& def) const { return !def.guard_id.empty() && Get(def.guard_id) < 1; }

    std::unordered_map<std::string, std::size_t> index_;
    std::vector<ControlDef> defs_;
    std::vector<int> values_;     // positions; -1 for continuous controls
    std::vector<double> analog_;  // continuous values; unused for other kinds
};

}  // namespace simcore
