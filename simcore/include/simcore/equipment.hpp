// simcore/equipment.hpp — the circuits and mechanisms behind a control panel.
//
// Electrical logic never blocks a switch (only covers, locks and keys do, in
// panel.hpp). Equipment turns switch positions into subsystem states — a
// filament warming up, a mast deploying, a fault when something is energised
// before its prerequisites are ready. Doing things in the wrong order has
// consequences here rather than being blocked.
//
// A subsystem is commanded on when all of `enable` hold and, if `enable_any` is
// not empty, at least one of `enable_any` holds. A latching subsystem (a mode
// engaged by a push-button, like the Osa's ЦУ АЗ) stays commanded after its
// enable conditions pass, until all of `release` hold.
// `prerequisites` must hold while starting or running, otherwise it faults.
// Progress runs 0..1: it rises over `start_time_s` and falls over `stop_time_s`,
// so reversing half-way (e.g. lowering a half-raised mast) takes only the time
// needed to undo what was done.
//
// Inputs are yes/no signals the rest of the sim sets (e.g. "target within 15 s
// of the firing zone"), so lamps and automatic sequences can depend on them.
//
// Placeholder model: a stopping subsystem winds down regardless of its
// prerequisites, and a fault clears as soon as the subsystem is commanded off.
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

// One thing that must be true: a control at a position, a subsystem in a state, or a sim input set.
struct Condition {
    enum class Kind { ControlEquals, ControlAtLeast, SubsystemIs, InputIs };

    Kind kind = Kind::ControlEquals;
    std::string id;                                // control, subsystem or input id
    int position = 1;                              // control kinds
    SubsystemState state = SubsystemState::Ready;  // SubsystemIs
    bool input_value = true;                       // InputIs

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
    static Condition Input(std::string input_id, bool value = true) {
        Condition c;
        c.kind = Kind::InputIs;
        c.id = std::move(input_id);
        c.input_value = value;
        return c;
    }
};

// Control conditions need only the panel; subsystem and input conditions need Equipment.
inline bool ControlConditionHolds(const Condition& c, const ControlPanel& panel) {
    const int pos = panel.Get(c.id);
    if (pos < 0) return false;
    return c.kind == Condition::Kind::ControlAtLeast ? pos >= c.position : pos == c.position;
}

inline bool IsControlCondition(const Condition& c) {
    return c.kind == Condition::Kind::ControlEquals || c.kind == Condition::Kind::ControlAtLeast;
}

struct SubsystemDef {
    std::string id;
    std::string name;
    std::vector<Condition> enable;         // all true = commanded on
    std::vector<Condition> prerequisites;  // must hold while starting/running, or it faults
    double start_time_s = 0.0;             // warm-up / deploy time
    double stop_time_s = 0.0;              // cool-down / stow time
    std::vector<Condition> enable_any;     // if not empty, at least one must also hold
    bool latching = false;                 // stays commanded once enabled, until `release`
    std::vector<Condition> release;        // latching only; all true = commanded off (must not be empty)
};

struct SubsystemTransition {
    std::string id;
    SubsystemState from = SubsystemState::Off;
    SubsystemState to = SubsystemState::Off;
    double time_s = 0.0;
};

class Equipment {
public:
    // Declare a yes/no signal set by the sim. Starts false.
    bool AddInput(const std::string& id) {
        if (id.empty() || inputs_.count(id) != 0) return false;
        inputs_[id] = false;
        return true;
    }

    bool SetInput(const std::string& id, bool value) {
        const auto it = inputs_.find(id);
        if (it == inputs_.end()) return false;
        it->second = value;
        return true;
    }

    bool HasInput(const std::string& id) const { return inputs_.count(id) != 0; }

    // False for undeclared inputs.
    bool GetInput(const std::string& id) const {
        const auto it = inputs_.find(id);
        return it != inputs_.end() && it->second;
    }

    // Rejects duplicate ids, negative times, unknown controls, positions or inputs, a latching
    // subsystem without release conditions, and subsystem references that aren't defined yet.
    // Definition order is therefore dependency order: one Update pass sees this step's upstream
    // states, deterministically.
    bool AddSubsystem(const SubsystemDef& def, const ControlPanel& panel) {
        if (def.id.empty() || index_.count(def.id) != 0) return false;
        if (def.start_time_s < 0.0 || def.stop_time_s < 0.0) return false;
        if (def.latching == def.release.empty()) return false;
        if (!RefsValid(def.enable, panel) || !RefsValid(def.prerequisites, panel) ||
            !RefsValid(def.enable_any, panel) || !RefsValid(def.release, panel)) {
            return false;
        }
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
        switch (c.kind) {
            case Condition::Kind::SubsystemIs: return Has(c.id) && State(c.id) == c.state;
            case Condition::Kind::InputIs: return HasInput(c.id) && GetInput(c.id) == c.input_value;
            case Condition::Kind::ControlEquals:
            case Condition::Kind::ControlAtLeast: return ControlConditionHolds(c, panel);
        }
        return false;
    }

    bool AllHold(const std::vector<Condition>& conds, const ControlPanel& panel) const {
        for (const Condition& c : conds) {
            if (!Holds(c, panel)) return false;
        }
        return true;
    }

    bool AnyHolds(const std::vector<Condition>& conds, const ControlPanel& panel) const {
        for (const Condition& c : conds) {
            if (Holds(c, panel)) return true;
        }
        return false;
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
        bool latched = false;
    };

    static constexpr double kProgressEps = 1e-9;

    bool RefsValid(const std::vector<Condition>& conds, const ControlPanel& panel) const {
        for (const Condition& c : conds) {
            switch (c.kind) {
                case Condition::Kind::SubsystemIs:
                    if (!Has(c.id)) return false;
                    break;
                case Condition::Kind::InputIs:
                    if (!HasInput(c.id)) return false;
                    break;
                case Condition::Kind::ControlEquals:
                case Condition::Kind::ControlAtLeast:
                    if (!panel.IsValidPosition(c.id, c.position)) return false;
                    break;
            }
        }
        return true;
    }

    bool Commanded(Unit& u, const ControlPanel& panel) {
        const bool enabled =
            AllHold(u.def.enable, panel) && (u.def.enable_any.empty() || AnyHolds(u.def.enable_any, panel));
        if (!u.def.latching) return enabled;
        if (u.latched && AllHold(u.def.release, panel)) {
            u.latched = false;
        } else if (!u.latched && enabled) {
            u.latched = true;
        }
        return u.latched;
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
        const bool enabled = Commanded(u, panel);
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
    std::unordered_map<std::string, bool> inputs_;
    std::vector<Unit> units_;
    std::vector<SubsystemTransition> transitions_;
    double time_s_ = 0.0;
};

// Conditions checked with or without Equipment. Without it, subsystem and input conditions count as met.
inline bool ConditionsHold(const std::vector<Condition>& conds, const ControlPanel& panel, const Equipment* equipment) {
    for (const Condition& c : conds) {
        if (!IsControlCondition(c)) {
            if (equipment != nullptr && !equipment->Holds(c, panel)) return false;
        } else if (!ControlConditionHolds(c, panel)) {
            return false;
        }
    }
    return true;
}

}  // namespace simcore
