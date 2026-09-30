// simcore/equipment.hpp — the circuits and mechanisms behind a control panel.
//
// Electrical logic never blocks a switch (only covers and keys do, in panel.hpp). Equipment turns switch
// positions into subsystem states — a filament warming up, a mast deploying, a
// fault when something is energised before its prerequisites are ready. Doing
// things in the wrong order has consequences here rather than being blocked.
//
// Each subsystem has an `enable` list (all true = commanded on) and a
// `prerequisites` list (must hold while starting or running, otherwise it
// faults). Progress runs 0..1: it rises over `start_time_s` and falls over
// `stop_time_s`, so reversing half-way (e.g. lowering a half-raised mast) takes
// only the time needed to undo what was done.
//
// Placeholder model: a stopping subsystem winds down regardless of its
// prerequisites, and a fault clears as soon as the subsystem is switched off.
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "simcore/panel.hpp"

namespace simcore {

enum class SubsystemState { Off, Starting, Ready, Stopping, Fault };

inline const char* ToString(SubsystemState s) {
    switch (s) {
        case SubsystemState::Off: return "off";
        case SubsystemState::Starting: return "starting";
        case SubsystemState::Ready: return "ready";
        case SubsystemState::Stopping: return "stopping";
        case SubsystemState::Fault: return "fault";
    }
    return "?";
}

// One thing that must be true: a control at a position, or a subsystem in a state.
struct Condition {
    enum class Kind { ControlEquals, ControlAtLeast, SubsystemIs };

    Kind kind = Kind::ControlEquals;
    std::string id;                                // control id or subsystem id
    int position = 1;                              // control kinds
    SubsystemState state = SubsystemState::Ready;  // SubsystemIs

    static Condition Control(std::string control_id, int pos) {
        Condition c;
        c.kind = Kind::ControlEquals;
        c.id = std::move(control_id);
        c.position = pos;
        return c;
    }
    static Condition AtLeast(std::string control_id, int pos) {
        Condition c = Control(std::move(control_id), pos);
        c.kind = Kind::ControlAtLeast;
        return c;
    }
    static Condition Subsystem(std::string subsystem_id, SubsystemState s) {
        Condition c;
        c.kind = Kind::SubsystemIs;
        c.id = std::move(subsystem_id);
        c.state = s;
        return c;
    }
    static Condition Ready(std::string subsystem_id) { return Subsystem(std::move(subsystem_id), SubsystemState::Ready); }
    static Condition Stopped(std::string subsystem_id) { return Subsystem(std::move(subsystem_id), SubsystemState::Off); }
};

// Control conditions need only the panel; subsystem conditions need Equipment.
inline bool ControlConditionHolds(const Condition& c, const ControlPanel& panel) {
    const int pos = panel.Get(c.id);
    if (pos < 0) return false;
    return c.kind == Condition::Kind::ControlAtLeast ? pos >= c.position : pos == c.position;
}

struct SubsystemDef {
    std::string id;
    std::string name;
    std::vector<Condition> enable;         // all true = commanded on
    std::vector<Condition> prerequisites;  // must hold while starting/running, or it faults
    double start_time_s = 0.0;             // warm-up / deploy time
    double stop_time_s = 0.0;              // cool-down / stow time
};

struct SubsystemTransition {
    std::string id;
    SubsystemState from = SubsystemState::Off;
    SubsystemState to = SubsystemState::Off;
    double time_s = 0.0;
};

class Equipment {
public:
    // Rejects duplicate ids, negative times, unknown controls or positions, and
    // subsystem references that aren't defined yet. Definition order is therefore
    // dependency order: one Update pass sees this step's upstream states, deterministically.
    bool AddSubsystem(const SubsystemDef& def, const ControlPanel& panel) {
        if (def.id.empty() || index_.count(def.id) != 0) return false;
        if (def.start_time_s < 0.0 || def.stop_time_s < 0.0) return false;
        if (!RefsValid(def.enable, panel) || !RefsValid(def.prerequisites, panel)) return false;
        index_[def.id] = units_.size();
        Unit u;
        u.def = def;
        units_.push_back(std::move(u));
        return true;
    }

    void Update(double dt, const ControlPanel& panel) {
        time_s_ += dt;
        for (Unit& u : units_) StepUnit(u, dt, panel);
    }

    bool Has(const std::string& id) const { return index_.count(id) != 0; }

    // Off for unknown ids.
    SubsystemState State(const std::string& id) const {
        const auto it = index_.find(id);
        return it == index_.end() ? SubsystemState::Off : units_[it->second].state;
    }

    bool IsReady(const std::string& id) const { return State(id) == SubsystemState::Ready; }

    // 0 = fully off/stowed, 1 = fully ready/deployed. For progress bars and animations.
    double Progress(const std::string& id) const {
        const auto it = index_.find(id);
        return it == index_.end() ? 0.0 : units_[it->second].progress;
    }

    bool Holds(const Condition& c, const ControlPanel& panel) const {
        if (c.kind == Condition::Kind::SubsystemIs) return Has(c.id) && State(c.id) == c.state;
        return ControlConditionHolds(c, panel);
    }

    bool AllHold(const std::vector<Condition>& conds, const ControlPanel& panel) const {
        for (const Condition& c : conds) {
            if (!Holds(c, panel)) return false;
        }
        return true;
    }

    double Time() const { return time_s_; }

    // Transitions since the last call, in the order they happened.
    std::vector<SubsystemTransition> TakeTransitions() {
        std::vector<SubsystemTransition> out;
        out.swap(transitions_);
        return out;
    }

private:
    struct Unit {
        SubsystemDef def;
        SubsystemState state = SubsystemState::Off;
        double progress = 0.0;
    };

    static constexpr double kProgressEps = 1e-9;

    bool RefsValid(const std::vector<Condition>& conds, const ControlPanel& panel) const {
        for (const Condition& c : conds) {
            if (c.kind == Condition::Kind::SubsystemIs) {
                if (!Has(c.id)) return false;
            } else if (!panel.IsValidPosition(c.id, c.position)) {
                return false;
            }
        }
        return true;
    }

    void Go(Unit& u, SubsystemState to) {
        transitions_.push_back({u.def.id, u.state, to, time_s_});
        u.state = to;
    }

    void Rise(Unit& u, double dt) {
        u.progress = u.def.start_time_s > 0.0 ? u.progress + dt / u.def.start_time_s : 1.0;
        if (u.progress >= 1.0 - kProgressEps) {
            u.progress = 1.0;
            Go(u, SubsystemState::Ready);
        }
    }

    void Fall(Unit& u, double dt) {
        u.progress = u.def.stop_time_s > 0.0 ? u.progress - dt / u.def.stop_time_s : 0.0;
        if (u.progress <= kProgressEps) {
            u.progress = 0.0;
            Go(u, SubsystemState::Off);
        }
    }

    void StepUnit(Unit& u, double dt, const ControlPanel& panel) {
        const bool enabled = AllHold(u.def.enable, panel);
        const bool prereqs_ok = AllHold(u.def.prerequisites, panel);
        switch (u.state) {
            case SubsystemState::Off:
                if (!enabled) return;
                if (!prereqs_ok) {
                    Go(u, SubsystemState::Fault);
                    return;
                }
                Go(u, SubsystemState::Starting);
                if (u.def.start_time_s <= 0.0) Rise(u, 0.0);
                return;
            case SubsystemState::Starting:
            case SubsystemState::Ready:
                if (!enabled) {
                    Go(u, SubsystemState::Stopping);
                    if (u.def.stop_time_s <= 0.0) Fall(u, 0.0);
                    return;
                }
                if (!prereqs_ok) {
                    Go(u, SubsystemState::Fault);
                    return;
                }
                if (u.state == SubsystemState::Starting) Rise(u, dt);
                return;
            case SubsystemState::Stopping:
                if (enabled) {
                    if (!prereqs_ok) {
                        Go(u, SubsystemState::Fault);
                        return;
                    }
                    Go(u, SubsystemState::Starting);
                    if (u.def.start_time_s <= 0.0) Rise(u, 0.0);
                    return;
                }
                Fall(u, dt);
                return;
            case SubsystemState::Fault:
                if (!enabled) {
                    u.progress = 0.0;
                    Go(u, SubsystemState::Off);
                }
                return;
        }
    }

    std::unordered_map<std::string, std::size_t> index_;
    std::vector<Unit> units_;
    std::vector<SubsystemTransition> transitions_;
    double time_s_ = 0.0;
};

// Conditions checked with or without Equipment. Without it, subsystem conditions count as met.
inline bool ConditionsHold(const std::vector<Condition>& conds, const ControlPanel& panel, const Equipment* equipment) {
    for (const Condition& c : conds) {
        if (c.kind == Condition::Kind::SubsystemIs) {
            if (equipment != nullptr && !equipment->Holds(c, panel)) return false;
        } else if (!ControlConditionHolds(c, panel)) {
            return false;
        }
    }
    return true;
}

}  // namespace simcore
