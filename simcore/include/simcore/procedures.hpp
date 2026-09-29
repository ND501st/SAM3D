// simcore/procedures.hpp — control panels, procedures, and NPC crew.
//
// Core design rule: the player and NPC crew operate the SAME controls through
// the SAME API. A crew member given an order ("power up the search radar")
// simply works through a Procedure step by step, with a human-like reaction
// time, exactly as the player would by clicking switches in the 3D cabin.
//
// Procedure content (which switch, which position, what order, warm-up times)
// comes from SAM Simulator's behaviour and operator manuals, and lives in data files.
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace simcore {

struct ControlDef {
    std::string id;       // stable identifier, e.g. "srch.hv_power"
    std::string label;    // what's printed on the panel (Cyrillic OK)
    int positions = 2;    // 2 = toggle; >2 = rotary / multi-position
    int initial = 0;
};

class ControlPanel {
public:
    bool AddControl(const ControlDef& def) {
        if (def.id.empty() || def.positions < 1 || index_.count(def.id) != 0) return false;
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

    // Physically move a control. Returns false for unknown controls or invalid positions.
    bool Set(const std::string& id, int value) {
        if (!IsValidPosition(id, value)) return false;
        values_[index_.at(id)] = value;
        return true;
    }

    const std::vector<ControlDef>& Controls() const { return defs_; }

private:
    std::unordered_map<std::string, std::size_t> index_;
    std::vector<ControlDef> defs_;
    std::vector<int> values_;
};

struct ProcedureStep {
    std::string control_id;
    int value = 1;
    double min_wait_s = 0.0;  // time that must pass after the previous step (e.g. valve warm-up)
    std::string description;
};

struct Procedure {
    std::string id;
    std::string name;
    std::vector<ProcedureStep> steps;
};

enum class StepResult {
    Accepted,          // correct next step; control moved; procedure advanced
    NotNextStep,       // control moved, but it wasn't the expected step (counted as a deviation)
    WaitRequired,      // correct step but too early; control NOT moved
    UnknownControl,
    InvalidPosition,
    ProcedureComplete  // procedure already finished; control moved
};

// Tracks progress through one procedure and judges each control action.
class ProcedureTracker {
public:
    ProcedureTracker() = default;
    explicit ProcedureTracker(Procedure procedure) : procedure_(std::move(procedure)) {}

    void Reset(Procedure procedure) {
        procedure_ = std::move(procedure);
        next_ = 0;
        elapsed_since_step_s_ = 0.0;
        deviations_ = 0;
    }

    void Update(double dt) { elapsed_since_step_s_ += dt; }

    StepResult Attempt(ControlPanel& panel, const std::string& control_id, int value) {
        if (!panel.Has(control_id)) return StepResult::UnknownControl;
        if (!panel.IsValidPosition(control_id, value)) return StepResult::InvalidPosition;
        if (IsComplete()) {
            panel.Set(control_id, value);
            return StepResult::ProcedureComplete;
        }
        const ProcedureStep& step = procedure_.steps[next_];
        if (step.control_id == control_id && step.value == value) {
            if (elapsed_since_step_s_ < step.min_wait_s) return StepResult::WaitRequired;
            panel.Set(control_id, value);
            ++next_;
            elapsed_since_step_s_ = 0.0;
            return StepResult::Accepted;
        }
        panel.Set(control_id, value);
        ++deviations_;
        return StepResult::NotNextStep;
    }

    bool IsComplete() const { return next_ >= procedure_.steps.size(); }
    std::size_t NextIndex() const { return next_; }
    const ProcedureStep* NextStep() const { return IsComplete() ? nullptr : &procedure_.steps[next_]; }
    double ElapsedSinceStep() const { return elapsed_since_step_s_; }
    int Deviations() const { return deviations_; }
    const Procedure& GetProcedure() const { return procedure_; }

private:
    Procedure procedure_;
    std::size_t next_ = 0;
    double elapsed_since_step_s_ = 0.0;
    int deviations_ = 0;
};

// An NPC crew member who carries out ordered procedures on a control panel.
class CrewMember {
public:
    CrewMember(std::string name, std::string role, double reaction_time_s)
        : name_(std::move(name)), role_(std::move(role)), reaction_time_s_(reaction_time_s) {}

    void Order(const Procedure& procedure) {
        tracker_.Reset(procedure);
        action_timer_s_ = 0.0;
        has_order_ = !procedure.steps.empty();
    }

    void CancelOrder() { has_order_ = false; }

    // Call every sim step. Performs at most one control action per call.
    void Update(double dt, ControlPanel& panel) {
        tracker_.Update(dt);
        if (!has_order_) return;
        action_timer_s_ += dt;
        const ProcedureStep* step = tracker_.NextStep();
        if (step == nullptr) {
            has_order_ = false;
            return;
        }
        if (action_timer_s_ < reaction_time_s_ || tracker_.ElapsedSinceStep() < step->min_wait_s) return;
        const StepResult r = tracker_.Attempt(panel, step->control_id, step->value);
        action_timer_s_ = 0.0;
        if (r == StepResult::UnknownControl || r == StepResult::InvalidPosition) {
            has_order_ = false;  // bad procedure data: stop rather than loop forever
        } else if (tracker_.IsComplete()) {
            has_order_ = false;
        }
    }

    bool IsBusy() const { return has_order_; }
    const std::string& Name() const { return name_; }
    const std::string& Role() const { return role_; }
    const ProcedureTracker& Tracker() const { return tracker_; }

private:
    std::string name_;
    std::string role_;
    double reaction_time_s_;
    double action_timer_s_ = 0.0;
    bool has_order_ = false;
    ProcedureTracker tracker_;
};

}  // namespace simcore
