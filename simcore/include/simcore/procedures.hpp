// simcore/procedures.hpp — procedures, the procedure judge, and NPC crew.
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
#include <cstdint>
#include <deque>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "simcore/equipment.hpp"
#include "simcore/panel.hpp"

namespace simcore {

struct ProcedureStep {
    std::string control_id;
    int value = 1;
    double min_wait_s = 0.0;  // time that must pass after the previous step (e.g. valve warm-up)
    std::string description;
    // What the operator should see before doing this step (e.g. a READY lamp).
    // Crew wait for it; a player who acts sooner is judged "early".
    std::vector<Condition> wait_for;
};

struct Procedure {
    std::string id;
    std::string name;
    std::vector<ProcedureStep> steps;
    std::string station;  // the seat whose controls it uses (console_data fills it); empty = any crew member
    bool situational = false;  // done only when an order calls for it: the drill master never pulls it into a plan
};

enum class StepResult {
    Accepted,          // correct next step; control moved; procedure advanced
    AcceptedEarly,     // correct next step done before its wait_for held; moved, advanced, counted as a deviation
    NotNextStep,       // control moved, but it wasn't the expected step (counted as a deviation)
    WaitRequired,      // correct step but before min_wait_s; control NOT moved
    Toward,            // the step's selector moved one position on toward the step's position: moved, not yet the step,
                       // not judged (a rotary clicked through its positions)
    UnknownControl,
    InvalidPosition,
    Blocked,           // physically blocked (cover closed, or a stepwise jump); control NOT moved, not judged
    ProcedureComplete  // procedure already finished; control moved
};

enum class DeviationKind { OutOfOrder, Early };

struct Deviation {
    DeviationKind kind = DeviationKind::OutOfOrder;
    double time_s = 0.0;            // tracker clock
    std::string control_id;         // what was actually moved
    int value = 0;
    std::size_t expected_step = 0;  // index of the step that was due
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
        time_s_ = 0.0;
        log_.clear();
    }

    void Update(double dt) {
        elapsed_since_step_s_ += dt;
        time_s_ += dt;
    }

    // `equipment` is optional; without it, subsystem conditions in wait_for count as met.
    StepResult Attempt(ControlPanel& panel, const std::string& control_id, int value,
                       const Equipment* equipment = nullptr) {
        if (!panel.Has(control_id)) return StepResult::UnknownControl;
        if (!panel.IsValidPosition(control_id, value)) return StepResult::InvalidPosition;
        if (!panel.CanMove(control_id, value)) return StepResult::Blocked;
        PassChecks(panel, equipment, control_id, value);
        if (IsComplete()) {
            panel.Set(control_id, value);
            return StepResult::ProcedureComplete;
        }
        const ProcedureStep& step = procedure_.steps[next_];
        // A selector on its way to the step's position (clicked through the positions between): not a deviation.
        if (step.control_id == control_id && step.value != value) {
            const ControlDef* def = panel.Find(control_id);
            const int now = panel.Get(control_id);
            if (def != nullptr && def->kind == ControlKind::Discrete && def->positions > 2 &&
                ((now < value && value < step.value) || (step.value < value && value < now))) {
                panel.Set(control_id, value);
                return StepResult::Toward;
            }
        }
        if (step.control_id == control_id && step.value == value) {
            if (elapsed_since_step_s_ < step.min_wait_s) return StepResult::WaitRequired;
            const bool ready = ConditionsHold(step.wait_for, panel, equipment);
            panel.Set(control_id, value);
            if (!ready) Log(DeviationKind::Early, control_id, value);
            ++next_;
            elapsed_since_step_s_ = 0.0;
            return ready ? StepResult::Accepted : StepResult::AcceptedEarly;
        }
        panel.Set(control_id, value);
        Log(DeviationKind::OutOfOrder, control_id, value);
        return StepResult::NotNextStep;
    }

    // A step whose control already sits where the step puts it (a radio's switch left on СИМПЛЕКС, the RK-1U already
    // on the R-407) is a check, not a move: passed once its wait is over, unless it is the very move being made.
    void PassChecks(const ControlPanel& panel, const Equipment* equipment, const std::string& moving = std::string(), int value = -1) {
        while (!IsComplete()) {
            const ProcedureStep& due = procedure_.steps[next_];
            if (due.control_id == moving && due.value == value) break;
            if (!panel.Has(due.control_id) || panel.Get(due.control_id) != due.value) break;
            if (elapsed_since_step_s_ < due.min_wait_s || !ConditionsHold(due.wait_for, panel, equipment)) break;
            ++next_;
        }
    }

    bool IsComplete() const { return next_ >= procedure_.steps.size(); }
    std::size_t NextIndex() const { return next_; }
    const ProcedureStep* NextStep() const { return IsComplete() ? nullptr : &procedure_.steps[next_]; }
    double ElapsedSinceStep() const { return elapsed_since_step_s_; }
    int Deviations() const { return static_cast<int>(log_.size()); }
    const std::vector<Deviation>& DeviationLog() const { return log_; }
    const Procedure& GetProcedure() const { return procedure_; }

private:
    void Log(DeviationKind kind, const std::string& control_id, int value) {
        log_.push_back({kind, time_s_, control_id, value, next_});
    }

    Procedure procedure_;
    std::size_t next_ = 0;
    double elapsed_since_step_s_ = 0.0;
    double time_s_ = 0.0;
    std::vector<Deviation> log_;
};

enum class CrewStatus { Idle, Working, Waiting };

// A control worked outside the vehicle, at a place (crew_mobility.hpp's VehicleJob): its man stays there after working it
// until `stay_until` holds (the work goes on: the aiming circle measuring), then is free to go.
struct OutsideJob {
    std::string control;
    std::string place;
    std::vector<Condition> stay_until;
};

// What a crew member's drill work takes from its skill tier (docs/crew/05_SKILL.md, data/crew/skill_levels.json).
// Without one (the default) a crew member reacts after its fixed reaction time and never slips.
struct CrewSkill {
    std::string tier;            // the tier's id, for the debrief
    double reaction_s = 1.0;     // per control action
    double jitter_s = 0.0;       // uniform +- on each action
    double slip_chance = 0.0;    // per step: does the next step first (an out-of-order action), then this one
    double omission_chance = 0.0;  // per step: forgets it until its patience runs out
    double patience_s = 5.0;
    double stress_factor = 1.0;  // multiplies delays and chances while stressed
    // Judgement: a step on one of these controls (a decision such as ПУСК) waits up to this much more, drawn each time.
    std::vector<std::string> decision_controls;
    double decision_delay_s = 0.0;
};

// What a crew member tells the player (drives voice lines and the order UI).
struct CrewReport {
    enum class Kind {
        Started,         // began a procedure
        Completed,       // finished a procedure
        Waiting,         // waiting for equipment; detail = the condition's id
        EquipmentFault,  // saw a fault it was waiting on; stopped; detail = subsystem id
        Aborted,         // stopped: cancelled, superseded, bad procedure data, or another seat's
                         // procedure; detail says which
        Slip,            // did a later step first (skill); detail = the control moved out of order
        Forgot           // forgot the next step for a while (skill); detail = its control
    };
    Kind kind = Kind::Started;
    std::string procedure_id;
    std::string detail;
    double time_s = 0.0;  // crew member's clock
};

// An NPC crew member who carries out ordered procedures on a control panel.
class CrewMember {
public:
    CrewMember(std::string name, std::string role, double reaction_time_s)
        : name_(std::move(name)), role_(std::move(role)), reaction_time_s_(reaction_time_s) {}

    // Drop everything and do this now.
    void Order(const Procedure& procedure) {
        if (has_order_) Report(CrewReport::Kind::Aborted, "superseded");
        has_order_ = false;
        queue_.clear();
        queue_.push_back(procedure);
        StartNext();
    }

    // Do this after the current and already-queued orders.
    void Queue(const Procedure& procedure) {
        queue_.push_back(procedure);
        if (!has_order_) StartNext();
    }

    // Stop the current order and forget the queue.
    void CancelOrder() {
        if (has_order_) Report(CrewReport::Kind::Aborted, "cancelled");
        Stop();
    }

    // Give this crew member a skill tier; `seed` makes its slips and jitter repeatable.
    void SetSkill(const CrewSkill& skill, unsigned seed) {
        skill_ = skill;
        has_skill_ = true;
        rng_.seed(seed);
        reaction_time_s_ = skill.reaction_s;
        step_delay_s_ = -1.0;
    }
    bool HasSkill() const { return has_skill_; }
    const CrewSkill& Skill() const { return skill_; }
    // Under stress (night, jamming, under fire, a target in the zone with no track) delays and chances grow.
    void SetStressed(bool stressed) { stressed_ = stressed; }
    bool Stressed() const { return stressed_; }
    // Slips and omissions so far (the debrief and the tests).
    int Slips() const { return slips_; }
    int Omissions() const { return omissions_; }

    // Controls this crew member can't reach sitting down (data/crew/<system>_crew.json, a seat's "stand_for", checked
    // against the body model of crew_body.hpp): a step on one first costs `stand_s` to get up and step to its panel,
    // and the next step on any other control `sit_s` to sit back down. Idle, he sits down again by himself.
    void SetStanding(std::vector<std::string> stand_for, double stand_s, double sit_s) {
        stand_for_ = std::move(stand_for);
        stand_s_ = stand_s;
        sit_s_ = sit_s;
    }
    bool Standing() const { return standing_; }

    // Controls worked outside the vehicle, at a place (data/vehicles/<system>.json "jobs", crew_mobility.hpp): a step on one
    // needs him there, a step on any other control at his seat. He goes out once the step's conditions hold and acts once
    // he has arrived. After a job he stays at its place while its work goes on (its stay_until: out at the aiming circle
    // until the measurement is done); then a step inside that waits takes him back to wait at his seat.
    void SetOutsideJobs(std::vector<OutsideJob> jobs) { jobs_ = std::move(jobs); }
    bool HasOutsideJobs() const { return !jobs_.empty(); }
    // Where his body is (the runner's CrewMover): "" at his seat inside, a place's id, "lineup" or "point" outside;
    // `moving`: on his way.
    void SetWhere(std::string at, bool moving) {
        at_ = std::move(at);
        moving_ = moving;
    }
    // Where to be with nothing to do ("" his seat; "lineup": "К машинам").
    void StayAt(std::string place) { stay_ = std::move(place); }
    const std::string& StaysAt() const { return stay_; }
    // Where he wants his body now ("" his seat).
    const std::string& WantedPlace() const { return want_; }
    // The place a control is worked at ("" at his seat).
    std::string PlaceOf(const std::string& control) const {
        for (const OutsideJob& j : jobs_) {
            if (j.control == control) return j.place;
        }
        return std::string();
    }

    // What he is about to do, for his body (crew_body.hpp, roadmap 8.10): the next action's control and position, the
    // seconds until he moves it (in_s < 0 while he waits for a condition), the control he moved last and when, and
    // whether he is up at a panel. An empty control: nothing to do.
    struct Intent {
        std::string control;
        int value = 0;
        double in_s = -1.0;
        bool waiting = false;
        std::string last_control;
        double since_last_s = 1e9;
        bool standing = false;
        std::string stand_control;
        bool away = false;      // not at his seat (out of the vehicle, or on his way)
        std::string place;      // where he is going or is ("" his seat)
    };
    Intent CurrentIntent() const {
        Intent i;
        i.last_control = last_control_;
        i.since_last_s = last_control_.empty() ? 1e9 : time_s_ - last_time_s_;
        i.standing = standing_;
        i.stand_control = stand_control_;
        i.away = !at_.empty() || moving_;
        i.place = want_;
        if (!has_order_) return i;
        const ProcedureStep* step = tracker_.NextStep();
        if (step == nullptr) return i;
        const std::vector<ProcedureStep>& steps = tracker_.GetProcedure().steps;
        const std::size_t n = tracker_.NextIndex();
        const ProcedureStep& act = has_skill_ && slip_pending_ && n + 1 < steps.size() ? steps[n + 1] : *step;
        i.control = act.control_id;
        i.value = act.value;
        if (status_ == CrewStatus::Waiting) {
            i.waiting = true;
            return i;
        }
        if (moving_ || want_ != at_) return i;  // on his way: not due yet
        const double needed = (has_skill_ ? step_delay_s_ : reaction_time_s_) + extra_s_;
        double left = needed - action_timer_s_;
        const double min_left = step->min_wait_s - tracker_.ElapsedSinceStep();
        if (min_left > left) left = min_left;
        i.in_s = left > 0.0 ? left : 0.0;
        return i;
    }

    // Call every sim step, after Equipment::Update. Performs at most one control action per call.
    // Without `equipment`, subsystem conditions in wait_for count as met.
    void Update(double dt, ControlPanel& panel, const Equipment* equipment = nullptr) {
        time_s_ += dt;
        tracker_.Update(dt);
        if (!has_order_) {
            want_ = stay_;
            idle_s_ += dt;
            if (standing_ && idle_s_ > 1.0) {  // nothing more to do up there: back to his seat
                standing_ = false;
                stand_control_.clear();
            }
            return;
        }
        idle_s_ = 0.0;
        const ProcedureStep* step = tracker_.NextStep();
        if (step == nullptr) {
            FinishCurrent();
            return;
        }
        if (tracker_.NextIndex() != drawn_for_step_ || serial_ != drawn_serial_) BeginStep();
        const std::string faulted = FaultedSubsystem(step->wait_for, equipment);
        if (!faulted.empty()) {
            Report(CrewReport::Kind::EquipmentFault, faulted);
            Stop();
            return;
        }
        if (!ConditionsHold(step->wait_for, panel, equipment)) {
            // Out at a job whose work is done, the next step inside: he goes back to wait for it at his seat.
            if (!at_.empty() && !moving_ && PlaceOf(step->control_id).empty() && ConditionsHold(hold_, panel, equipment)) {
                want_.clear();
            }
            if (status_ != CrewStatus::Waiting) {
                status_ = CrewStatus::Waiting;
                Report(CrewReport::Kind::Waiting, FirstUnmet(step->wait_for, panel, equipment));
            }
            action_timer_s_ = 0.0;  // reaction time counts from when the lamp comes on
            return;
        }
        status_ = CrewStatus::Working;
        // A step at a place he isn't at: he goes there first (the runner walks him); his reaction counts from arriving.
        want_ = PlaceOf(step->control_id);
        if (want_ != at_ || moving_) {
            action_timer_s_ = 0.0;
            return;
        }
        action_timer_s_ += dt;
        const double needed_s = (has_skill_ ? step_delay_s_ : reaction_time_s_) + extra_s_;
        if (action_timer_s_ < needed_s || tracker_.ElapsedSinceStep() < step->min_wait_s) return;
        if (has_skill_ && slip_pending_) {
            slip_pending_ = false;
            const ProcedureStep& next = tracker_.GetProcedure().steps[tracker_.NextIndex() + 1];
            ++slips_;
            Report(CrewReport::Kind::Slip, next.control_id);
            tracker_.Attempt(panel, next.control_id, next.value, equipment);  // logged out of order (or blocked: nothing moves)
            last_control_ = next.control_id;
            last_time_s_ = time_s_;
            action_timer_s_ = 0.0;
            extra_s_ = 0.0;
            return;
        }
        const std::string control_id = step->control_id;
        const StepResult r = tracker_.Attempt(panel, control_id, step->value, equipment);
        hold_.clear();  // a job's work holds him at its place until it is done
        for (const OutsideJob& j : jobs_) {
            if (j.control == control_id) hold_ = j.stay_until;
        }
        last_control_ = control_id;
        last_time_s_ = time_s_;
        action_timer_s_ = 0.0;
        extra_s_ = 0.0;
        if (r == StepResult::UnknownControl || r == StepResult::InvalidPosition || r == StepResult::Blocked) {
            // Bad procedure data (e.g. a missing "open cover" step): stop rather than loop forever.
            Report(CrewReport::Kind::Aborted, control_id);
            Stop();
        } else if (tracker_.IsComplete()) {
            FinishCurrent();
        } else {
            BeginStep();  // on to the next step at once (his body already turns to it)
        }
    }

    bool IsBusy() const { return has_order_; }
    CrewStatus Status() const { return status_; }
    std::size_t QueuedCount() const { return queue_.size(); }
    const std::string& Name() const { return name_; }
    const std::string& Role() const { return role_; }
    const ProcedureTracker& Tracker() const { return tracker_; }

    // Reports since the last call, oldest first.
    std::vector<CrewReport> TakeReports() {
        std::vector<CrewReport> out;
        out.swap(reports_);
        return out;
    }

private:
    void Report(CrewReport::Kind kind, std::string detail = std::string()) {
        reports_.push_back({kind, tracker_.GetProcedure().id, std::move(detail), time_s_});
    }

    double Uniform() { return static_cast<double>(rng_()) / 4294967296.0; }

    // A new step is due: getting up for a control out of seated reach (or sitting back down), then the skill's draw.
    void BeginStep() {
        drawn_for_step_ = tracker_.NextIndex();
        drawn_serial_ = serial_;
        extra_s_ = 0.0;
        const std::string& c = tracker_.NextStep()->control_id;
        bool needs = false;
        for (const std::string& s : stand_for_) needs = needs || s == c;
        if (needs && !standing_) {
            extra_s_ = stand_s_;
            standing_ = true;
        } else if (!needs && standing_) {
            extra_s_ = sit_s_;
            standing_ = false;
            stand_control_.clear();
        }
        if (needs) stand_control_ = c;
        if (has_skill_) DrawStep();
    }

    // How long the step takes this crew member (reaction and jitter, a forgotten step's patience, stress), and
    // whether he slips (does the next step first). A slip is only possible between two steps that wait for nothing.
    void DrawStep() {
        const double stress = stressed_ ? skill_.stress_factor : 1.0;
        double d = skill_.reaction_s + (2.0 * Uniform() - 1.0) * skill_.jitter_s;
        d = (d > 0.05 ? d : 0.05) * stress;
        for (const std::string& c : skill_.decision_controls) {
            if (c == tracker_.NextStep()->control_id) d += Uniform() * skill_.decision_delay_s * stress;
        }
        if (Uniform() < skill_.omission_chance * stress) {
            d += skill_.patience_s;
            ++omissions_;
            Report(CrewReport::Kind::Forgot, tracker_.NextStep()->control_id);
        }
        step_delay_s_ = d;
        const std::vector<ProcedureStep>& steps = tracker_.GetProcedure().steps;
        const std::size_t i = tracker_.NextIndex();
        slip_pending_ = i + 1 < steps.size() && steps[i].wait_for.empty() && steps[i + 1].wait_for.empty() &&
                        steps[i + 1].min_wait_s <= 0.0 && Uniform() < skill_.slip_chance * stress;
    }

    void StartNext() {
        while (!queue_.empty()) {
            Procedure next = std::move(queue_.front());
            queue_.pop_front();
            if (next.steps.empty()) continue;
            if (!next.station.empty() && next.station != role_) {
                // Not this seat's controls: refuse it rather than reach across the vehicle.
                reports_.push_back({CrewReport::Kind::Aborted, next.id, "station " + next.station, time_s_});
                continue;
            }
            tracker_.Reset(std::move(next));
            ++serial_;
            action_timer_s_ = 0.0;
            has_order_ = true;
            status_ = CrewStatus::Working;
            Report(CrewReport::Kind::Started);
            BeginStep();
            return;
        }
        has_order_ = false;
        status_ = CrewStatus::Idle;
    }

    void FinishCurrent() {
        Report(CrewReport::Kind::Completed);
        has_order_ = false;
        StartNext();
    }

    void Stop() {
        has_order_ = false;
        queue_.clear();
        status_ = CrewStatus::Idle;
    }

    // A subsystem this step is waiting on that has faulted (waiting would be forever).
    static std::string FaultedSubsystem(const std::vector<Condition>& conds, const Equipment* equipment) {
        if (equipment == nullptr) return std::string();
        for (const Condition& c : conds) {
            if (c.kind == Condition::Kind::SubsystemIs && c.state != SubsystemState::Fault &&
                equipment->State(c.id) == SubsystemState::Fault) {
                return c.id;
            }
        }
        return std::string();
    }

    static std::string FirstUnmet(const std::vector<Condition>& conds, const ControlPanel& panel,
                                  const Equipment* equipment) {
        for (const Condition& c : conds) {
            if (!ConditionsHold({c}, panel, equipment)) return c.id;
        }
        return std::string();
    }

    std::string name_;
    std::string role_;
    double reaction_time_s_;
    bool has_skill_ = false;
    CrewSkill skill_;
    std::mt19937 rng_;
    bool stressed_ = false;
    std::size_t drawn_for_step_ = static_cast<std::size_t>(-1);
    unsigned serial_ = 0;        // procedures started (a new one's first step is a new step even at the same index)
    unsigned drawn_serial_ = 0;
    double step_delay_s_ = -1.0;
    double extra_s_ = 0.0;       // this step's getting up or sitting down
    std::vector<std::string> stand_for_;
    std::vector<OutsideJob> jobs_;
    std::vector<Condition> hold_;
    std::string at_;
    bool moving_ = false;
    std::string stay_;
    std::string want_;
    double stand_s_ = 0.0;
    double sit_s_ = 0.0;
    bool standing_ = false;
    std::string stand_control_;
    double idle_s_ = 0.0;
    std::string last_control_;
    double last_time_s_ = 0.0;
    bool slip_pending_ = false;
    int slips_ = 0;
    int omissions_ = 0;
    double action_timer_s_ = 0.0;
    double time_s_ = 0.0;
    bool has_order_ = false;
    CrewStatus status_ = CrewStatus::Idle;
    std::deque<Procedure> queue_;
    ProcedureTracker tracker_;
    std::vector<CrewReport> reports_;
};

}  // namespace simcore
