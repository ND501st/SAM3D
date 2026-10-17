// simcore/drill_master.hpp — the reference performer: the crew's perfect performance from any state (roadmap 8.3).
//
// docs/crew/06_REFERENCE_PERFORMER.md is the specification. In short: the console data already is the dependency
// graph (each subsystem lists the controls, subsystems and inputs that switch it on, with its warm-up; each control
// belongs to one seat). The drill master
//   1. expands a goal backwards through that graph to the control actions it needs, each with the chain of reasons
//      that leads to the goal ("why"), choosing the cheapest alternative where a subsystem can be switched on more
//      than one way, and the world facts it needs from the crew data (data/crew/<system>_crew.json);
//   2. prefers the sourced drills: needed actions that are steps of a procedure of their seat pull in that procedure
//      (when most of its steps are needed), so the plan is drills per seat plus loose actions;
//   3. runs the plan on a copy of the console with a perfect crew (the reference tier's reaction, no jitter, no slips)
//      to get the ideal timeline: every seat's actions with their times, and when the goal is reached;
//   4. from that, gives each seat's best next move now (what, why, when) and the cost of any move (how much later the
//      goal comes after it).
// It is deterministic and explainable: the same state gives the same answer, and every answer is a chain of
// conditions in the data. It plans from the console and the crew data, never from the truth of the world: a world
// fact it can't make true (a target captured, a target in the zone) is reported as what the run waits for.
#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/procedures.hpp"

namespace simcore {

// One control action of a seat (in a plan or a timeline).
struct DrillAction {
    std::string station;
    std::string control;     // empty for an action that sets a world fact (e.g. the driver halts)
    int position = 0;
    std::string procedure;   // the drill it belongs to; empty: a loose action
    std::size_t step = 0;    // its index in the drill
    std::string description;
    double time_s = 0.0;     // in a timeline: when it is done, from the start of the run
};

// What the drill master aims at: conditions to make true, and the doctrine's preferred ways where the data allows
// several (a task's "prefer": К бою starts the turbine, not the take-off generator).
struct DrillGoal {
    std::vector<Condition> conditions;
    std::vector<Condition> prefer;
    std::vector<std::string> drills;  // the drills of the order behind the task: chosen first where they help
    std::string name;  // for the why chains ("the goal: Ready for action at a halt")
    DrillGoal(std::vector<Condition> c) : conditions(std::move(c)) {}  // NOLINT: implicit on purpose
    DrillGoal(const CrewTask& t) : conditions(t.conditions), prefer(t.prefer), name(t.name) {}  // NOLINT
};

struct DrillPlan {
    struct Seat {
        std::string station;
        std::vector<std::string> procedures;  // drills, in order
        std::vector<DrillAction> loose;       // actions in no drill (with what they wait for in `loose_waits`)
        std::vector<std::vector<Condition>> loose_waits;
        std::vector<std::string> world_actions;  // world facts this seat makes true by an action (an input id)
    };
    std::vector<Seat> seats;                             // the console's station order; only seats with work
    std::vector<Condition> world;                        // world facts no seat can make true: the run waits for them
    std::map<std::string, std::vector<std::string>> why;  // "control=position" -> reasons, nearest first, up to the goal
    bool Empty() const { return seats.empty() && world.empty(); }
    const Seat* FindSeat(const std::string& station) const {
        for (const Seat& s : seats) {
            if (s.station == station) return &s;
        }
        return nullptr;
    }
};

struct IdealTimeline {
    bool reached = false;
    double time_s = 0.0;                              // to the goal, or how long the run went on
    std::vector<DrillAction> actions;                 // every action of every seat, in time order
    std::vector<std::pair<double, std::string>> ready;  // when each subsystem came ready
    std::vector<Condition> blocked_on;                // the goal's world facts still unmet when the run stopped
};

// A seat's best next move.
struct DrillAdvice {
    std::string station;
    std::string control;
    int position = 0;
    std::string procedure;
    std::string description;
    double due_in_s = 0.0;          // when the ideal run does it, from now (about the reaction time: do it now)
    std::vector<std::string> why;   // the reasons, nearest first, up to the goal
};

class DrillMaster {
public:
    // `crew` may be null (no world facts: inputs are only waited for). `reaction_s`: the reference crew's time per action.
    DrillMaster(const ConsoleData& console, const CrewData* crew, double reaction_s = 0.55)
        : console_(console), crew_(crew), reaction_s_(reaction_s) {}

    double ReactionS() const { return reaction_s_; }

    // A task as the drill master aims at it: its conditions, its preferred ways, and the drills of the crew data's
    // orders that ask for it (К бою's drills for "ready, halted"), which win over other drills that would also do.
    DrillGoal GoalFor(const CrewTask& task) const {
        DrillGoal g(task);
        if (crew_ != nullptr) {
            for (const OrderDef& o : crew_->orders) {
                if (o.task != task.id) continue;
                for (const OrderStart& s : o.procedures) g.drills.push_back(s.procedure);
            }
        }
        return g;
    }

    // The plan from a state (a panel and its equipment) to the goal.
    DrillPlan Plan(const DrillGoal& goal, const ControlPanel& panel, const Equipment& equipment) const {
        Expansion x{panel, equipment, {}, {}, {}, {}, {}, &goal.prefer};
        for (const Condition& c : goal.conditions) Expand(x, c, {goal.name.empty() ? std::string("the goal") : "the goal: " + goal.name});
        // The drills chosen for those actions wait for lamps of their own: plan those too, until nothing new is needed.
        std::map<std::string, std::vector<std::string>> drills;
        for (int round = 0; round < 6; ++round) {
            drills.clear();
            std::map<std::string, std::vector<std::pair<std::string, int>>> needed;
            for (const auto& a : x.actions) needed[Station(a.first)].push_back(a);
            for (const auto& n : needed) ChooseDrills(n.first, n.second, goal.drills, drills[n.first]);
            const std::size_t before = x.actions.size() + x.world.size() + x.world_actions.size();
            for (const auto& d : drills) {
                for (const std::string& id : d.second) {
                    const Procedure* p = console_.FindProcedure(id);
                    if (p == nullptr) continue;
                    for (const ProcedureStep& s : p->steps) {
                        for (const Condition& w : s.wait_for) Expand(x, w, {"the drill '" + id + "' waits for it"});
                    }
                }
            }
            if (x.actions.size() + x.world.size() + x.world_actions.size() == before) break;
        }
        DrillPlan plan;
        plan.why = x.why;
        plan.world = x.world;
        std::map<std::string, std::vector<std::pair<std::string, int>>> needed;
        for (const auto& a : x.actions) needed[Station(a.first)].push_back(a);
        for (const ConsoleStation& st : StationsOrDerived()) {
            const auto it = needed.find(st.id);
            std::vector<std::pair<std::string, int>> todo = it != needed.end() ? it->second : std::vector<std::pair<std::string, int>>();
            DrillPlan::Seat seat;
            seat.station = st.id;
            seat.procedures = drills[st.id];
            OrderDrills(seat.procedures);
            for (const auto& a : todo) {  // what no chosen drill covers is a loose action (cover first, keys step by step)
                if (Covered(seat.procedures, a)) continue;
                AddLoose(seat, a, x);
            }
            for (const std::string& in : x.world_actions) {
                const CrewFact* f = crew_ != nullptr ? crew_->FindFact(in) : nullptr;
                if (f != nullptr && f->seats.front() == st.id) seat.world_actions.push_back(in);
            }
            if (!seat.procedures.empty() || !seat.loose.empty() || !seat.world_actions.empty()) plan.seats.push_back(std::move(seat));
        }
        return plan;
    }

    // The ideal timeline from `now` to the goal: the plan run on a copy of the console by a perfect crew.
    IdealTimeline Ideal(const DrillGoal& goal, const ConsoleData& now, double max_s = 1800.0, double dt = 0.1) const {
        IdealTimeline out;
        ConsoleData ghost = now;
        if (Holds(goal.conditions, ghost)) {
            out.reached = true;
            return out;
        }
        const DrillPlan plan = Plan(goal, ghost.panel, ghost.equipment);
        struct Hand {
            CrewMember member;
            std::string station;
            std::size_t last_index = 0;
            std::string last_proc;
        };
        std::vector<Hand> hands;
        std::vector<std::pair<std::string, double>> world_due;  // inputs set by a seat's action, and when
        for (const DrillPlan::Seat& s : plan.seats) {
            hands.push_back({CrewMember("ghost " + s.station, s.station, reaction_s_), s.station, 0, std::string()});
            CrewMember& m = hands.back().member;
            if (crew_ != nullptr) {  // the perfect crewman still gets up for a control out of seated reach
                const CrewSeatDef* seat = crew_->FindSeat(s.station);
                if (seat != nullptr && !seat->stand_for.empty()) m.SetStanding(seat->stand_for, crew_->stand_s, crew_->sit_s);
            }
            std::vector<Procedure> later;
            for (std::size_t i = 0; i < s.loose.size(); ++i) {  // loose actions with nothing to wait for come first
                Procedure p;
                p.id = "loose: " + s.loose[i].control;
                p.station = s.station;
                ProcedureStep st;
                st.control_id = s.loose[i].control;
                st.value = s.loose[i].position;
                st.description = s.loose[i].description;
                st.wait_for = s.loose_waits[i];
                p.steps.push_back(st);
                if (st.wait_for.empty()) {
                    m.Queue(p);
                } else {
                    later.push_back(p);
                }
            }
            for (const std::string& id : s.procedures) {
                if (const Procedure* p = ghost.FindProcedure(id)) m.Queue(*p);
            }
            for (const Procedure& p : later) m.Queue(p);
            for (const std::string& in : s.world_actions) {
                world_due.push_back({in, reaction_s_});
                out.actions.push_back({s.station, std::string(), 1, std::string(), 0, WorldActionText(in), reaction_s_});
            }
        }
        for (Hand& h : hands) h.member.TakeReports();
        double t = 0.0;
        while (t < max_s) {
            t += dt;
            for (const auto& w : world_due) {
                if (t + 1e-9 >= w.second) ghost.equipment.SetInput(w.first, true);
            }
            ghost.equipment.Update(dt, ghost.panel);
            for (const SubsystemTransition& tr : ghost.equipment.TakeTransitions()) {
                if (tr.to == SubsystemState::Ready) out.ready.push_back({tr.time_s, tr.id});
            }
            if (Holds(goal.conditions, ghost)) {
                out.reached = true;
                break;
            }
            ghost.panel.ReleaseMomentaries();
            for (Hand& h : hands) {
                const std::string proc_before = h.member.IsBusy() ? h.member.Tracker().GetProcedure().id : std::string();
                const std::size_t index_before = h.member.IsBusy() ? h.member.Tracker().NextIndex() : 0;
                const Procedure* p_before = h.member.IsBusy() ? &h.member.Tracker().GetProcedure() : nullptr;
                ProcedureStep step_before;
                if (p_before != nullptr && index_before < p_before->steps.size()) step_before = p_before->steps[index_before];
                h.member.Update(dt, ghost.panel, &ghost.equipment);
                const bool moved = !proc_before.empty() &&
                                   (!h.member.IsBusy() || h.member.Tracker().GetProcedure().id != proc_before ||
                                    h.member.Tracker().NextIndex() != index_before);
                if (moved && !step_before.control_id.empty()) {
                    const bool loose = proc_before.compare(0, 6, "loose:") == 0;
                    out.actions.push_back({h.station, step_before.control_id, step_before.value, loose ? std::string() : proc_before,
                                           index_before, step_before.description, t});
                }
                h.member.TakeReports();
            }
        }
        out.time_s = t;
        if (!out.reached) {
            for (const Condition& c : goal.conditions) {
                if (c.kind == Condition::Kind::InputIs && !ghost.equipment.Holds(c, ghost.panel)) out.blocked_on.push_back(c);
            }
            for (const Condition& c : plan.world) {
                if (!ghost.equipment.Holds(c, ghost.panel)) out.blocked_on.push_back(c);
            }
        }
        std::stable_sort(out.actions.begin(), out.actions.end(), [](const DrillAction& a, const DrillAction& b) { return a.time_s < b.time_s; });
        return out;
    }

    // Each seat's best next move from `now` toward the goal: the first action of its ideal timeline.
    std::vector<DrillAdvice> NextBest(const DrillGoal& goal, const ConsoleData& now) const {
        std::vector<DrillAdvice> out;
        const IdealTimeline tl = Ideal(goal, now);
        const DrillPlan plan = Plan(goal, now.panel, now.equipment);
        for (const DrillAction& a : tl.actions) {
            bool have = false;
            for (const DrillAdvice& d : out) have = have || d.station == a.station;
            if (have) continue;
            DrillAdvice d;
            d.station = a.station;
            d.control = a.control;
            d.position = a.position;
            d.procedure = a.procedure;
            d.description = a.description;
            d.due_in_s = a.time_s;
            const auto w = plan.why.find(Key(a.control, a.position));
            if (w != plan.why.end()) {
                d.why = w->second;
            } else if (!a.procedure.empty()) {
                d.why = {"a step of the drill '" + a.procedure + "'"};
            }
            out.push_back(std::move(d));
        }
        return out;
    }

    // The best next move of one seat (control empty: nothing for that seat to do toward the goal).
    DrillAdvice NextFor(const std::string& station, const DrillGoal& goal, const ConsoleData& now) const {
        for (DrillAdvice& d : NextBest(goal, now)) {
            if (d.station == station) return d;
        }
        DrillAdvice none;
        none.station = station;
        return none;
    }

    // How much later the goal is reached after moving `control` to `position` than after the best move: 0 for a best
    // move (within a step), the delay otherwise; a large number when the move makes the goal unreachable in `max_s`.
    double CostOf(const DrillGoal& goal, const ConsoleData& before, const std::string& control, int position,
                  double max_s = 1800.0) const {
        const IdealTimeline base = Ideal(goal, before, max_s);
        ConsoleData after = before;
        if (!after.panel.Set(control, position)) return 0.0;  // a blocked move changes nothing
        after.equipment.Update(0.0, after.panel);  // its consequence at once (a fault)
        after.equipment.TakeTransitions();
        const IdealTimeline tl = Ideal(goal, after, max_s);
        if (!tl.reached) return base.reached ? max_s : 0.0;
        const double c = tl.time_s - (base.time_s - reaction_s_);
        return c > 0.3 ? c : 0.0;  // within a step of the best move
    }

    // A condition as words, for the why chains and the overlay.
    std::string Text(const Condition& c) const {
        switch (c.kind) {
            case Condition::Kind::ControlEquals: return LabelOf(c.id, c.position);
            case Condition::Kind::ControlAtLeast: return LabelOf(c.id, c.position) + " or beyond";
            case Condition::Kind::SubsystemIs: {
                const SubsystemDef* d = console_.equipment.Def(c.id);
                const std::string name = d != nullptr && !d->name.empty() ? d->name : c.id;
                return name + (c.state == SubsystemState::Ready ? " ready" : std::string(" ") + ToString(c.state));
            }
            case Condition::Kind::InputIs: return c.id + (c.input_value ? "" : " not") + " (world)";
        }
        return c.id;
    }

private:
    struct Expansion {
        const ControlPanel& panel;
        const Equipment& eq;
        std::vector<std::pair<std::string, int>> actions;  // needed control positions, in order found
        std::map<std::string, std::vector<std::string>> why;
        std::map<std::string, std::vector<Condition>> waits;  // control=pos -> prerequisites to wait for before it
        std::vector<Condition> world;
        std::vector<std::string> world_actions;
        const std::vector<Condition>* prefer;
    };

    static bool Same(const Condition& a, const Condition& b) {
        return a.kind == b.kind && a.id == b.id && a.position == b.position && a.state == b.state && a.input_value == b.input_value;
    }

    static std::string Key(const std::string& control, int position) { return control + "=" + std::to_string(position); }

    std::string LabelOf(const std::string& control, int position) const {
        const ControlDef* d = console_.panel.Find(control);
        std::string out = d != nullptr && !d->label.empty() ? d->label : control;
        if (d != nullptr && position >= 0 && static_cast<std::size_t>(position) < d->position_labels.size()) {
            out += " to " + d->position_labels[static_cast<std::size_t>(position)];
        } else if (d != nullptr && d->kind == ControlKind::Momentary) {
            out += " (press)";
        } else {
            out += " to " + std::to_string(position);
        }
        return out;
    }

    std::string WorldActionText(const std::string& input) const {
        const CrewFact* f = crew_ != nullptr ? crew_->FindFact(input) : nullptr;
        return f != nullptr ? input + ": " + f->set_by : input;
    }

    std::vector<ConsoleStation> StationsOrDerived() const {
        if (!console_.stations.empty()) return console_.stations;
        std::vector<ConsoleStation> out;
        for (const Procedure& p : console_.procedures) {
            bool have = false;
            for (const ConsoleStation& s : out) have = have || s.id == p.station;
            if (!have) out.push_back({p.station, p.station});
        }
        return out;
    }

    std::string Station(const std::string& control) const {
        const ControlDef* d = console_.panel.Find(control);
        return d != nullptr ? d->station : std::string();
    }

    static bool Holds(const std::vector<Condition>& goal, const ConsoleData& c) { return c.equipment.AllHold(goal, c.panel); }

    // How hard a condition is to make true from this state: seconds of warm-up plus a reaction per action; world facts
    // nobody can act on are very expensive (a crew waits for the world only when nothing else will do).
    double Cost(const Expansion& x, const Condition& c, std::set<std::string>& seen) const {
        if (x.eq.Holds(c, x.panel)) return 0.0;
        const std::string k = Text(c);
        if (!seen.insert(k).second) return 0.0;
        switch (c.kind) {
            case Condition::Kind::ControlEquals:
            case Condition::Kind::ControlAtLeast: return reaction_s_;
            case Condition::Kind::InputIs: {
                const CrewFact* f = crew_ != nullptr ? crew_->FindFact(c.id) : nullptr;
                if (f == nullptr) return 1e6;
                if (f->by_action) return reaction_s_;
                double sum = 1e5;
                for (const Condition& n : f->needs) sum += Cost(x, n, seen);
                return sum;
            }
            case Condition::Kind::SubsystemIs: {
                const SubsystemDef* d = x.eq.Def(c.id);
                if (d == nullptr) return 1e6;
                if (c.state == SubsystemState::Ready) {
                    double sum = x.eq.State(c.id) == SubsystemState::Starting ? 0.0 : d->start_time_s;
                    for (const Condition& n : d->enable) sum += Cost(x, n, seen);
                    for (const Condition& n : d->prerequisites) sum += Cost(x, n, seen);
                    if (!d->enable_any.empty() && !x.eq.AnyHolds(d->enable_any, x.panel)) {
                        double best = 1e9;
                        for (const Condition& n : d->enable_any) {
                            std::set<std::string> s2 = seen;
                            best = std::min(best, Cost(x, n, s2));
                        }
                        sum += best;
                    }
                    return sum;
                }
                return reaction_s_ + d->stop_time_s;
            }
        }
        return 0.0;
    }

    void Need(Expansion& x, const std::string& control, int position, const std::vector<std::string>& why,
              const std::vector<Condition>& waits) const {
        const std::string k = Key(control, position);
        if (x.why.count(k) != 0) return;
        x.why[k] = why;
        x.waits[k] = waits;
        x.actions.push_back({control, position});
    }

    // Backward chaining: what must be done for `c` to hold, recorded in `x`. `why` is the chain from `c` up to the goal.
    void Expand(Expansion& x, const Condition& c, std::vector<std::string> why, const std::vector<Condition>& waits = {}) const {
        if (x.eq.Holds(c, x.panel)) return;
        for (const std::string& w : why) {  // a cycle: this condition is already being expanded further up
            if (w == Text(c)) return;
        }
        switch (c.kind) {
            case Condition::Kind::ControlEquals:
            case Condition::Kind::ControlAtLeast:
                Need(x, c.id, c.position, why, waits);
                return;
            case Condition::Kind::InputIs: {
                const CrewFact* f = crew_ != nullptr ? crew_->FindFact(c.id) : nullptr;
                if (f != nullptr && f->by_action && c.input_value) {
                    if (std::find(x.world_actions.begin(), x.world_actions.end(), c.id) == x.world_actions.end()) x.world_actions.push_back(c.id);
                    return;
                }
                if (f != nullptr) {
                    why.insert(why.begin(), Text(c));
                    for (const Condition& n : f->needs) Expand(x, n, why);
                }
                for (const Condition& w : x.world) {
                    if (w.id == c.id && w.input_value == c.input_value) return;
                }
                x.world.push_back(c);
                return;
            }
            case Condition::Kind::SubsystemIs: {
                const SubsystemDef* d = x.eq.Def(c.id);
                if (d == nullptr) return;
                std::vector<std::string> up = why;
                up.insert(up.begin(), Text(c));
                if ((c.state == SubsystemState::Ready || c.state == SubsystemState::Starting) &&
                    x.eq.State(c.id) == SubsystemState::Fault) {
                    // Recover a fault: its enabling switch off, then on again once it is off and its prerequisites hold.
                    for (const Condition& n : d->enable) {
                        if (!IsControl(n)) continue;
                        const int off = OffPosition(n);
                        if (off < 0) continue;
                        std::vector<std::string> rec = up;
                        rec.insert(rec.begin(), "recover the fault: switch it off first");
                        Need(x, n.id, off, rec, {});
                        std::vector<Condition> wait_on = d->prerequisites;
                        wait_on.push_back(Condition::Subsystem(c.id, SubsystemState::Off));
                        Need(x, n.id, n.position, up, wait_on);
                        break;
                    }
                    for (const Condition& n : d->prerequisites) Expand(x, n, up);
                    return;
                }
                if (c.state == SubsystemState::Ready || c.state == SubsystemState::Starting) {
                    for (const Condition& n : d->prerequisites) Expand(x, n, up);
                    // Controls of this subsystem wait for its prerequisites (switching on before them faults it).
                    const std::vector<Condition>& pre = d->prerequisites;
                    if (d->latching && x.eq.State(c.id) != SubsystemState::Off) return;  // already latched on
                    for (const Condition& n : d->enable) Expand(x, n, up, IsControl(n) ? pre : std::vector<Condition>());
                    if (!d->enable_any.empty() && !x.eq.AnyHolds(d->enable_any, x.panel)) {
                        // The doctrine's way if the task names one, else the quickest.
                        const Condition* best = nullptr;
                        for (const Condition& n : d->enable_any) {
                            for (const Condition& p : *x.prefer) {
                                if (best == nullptr && Same(n, p)) best = &n;
                            }
                        }
                        if (best == nullptr) {
                            double best_cost = 1e18;
                            for (const Condition& n : d->enable_any) {
                                std::set<std::string> seen;
                                const double cost = Cost(x, n, seen);
                                if (cost < best_cost) {
                                    best_cost = cost;
                                    best = &n;
                                }
                            }
                        }
                        if (best != nullptr) Expand(x, *best, up, IsControl(*best) ? pre : std::vector<Condition>());
                    }
                    return;
                }
                if (c.state == SubsystemState::Off) {
                    if (d->latching) {
                        for (const Condition& n : d->release) Expand(x, n, up);
                        return;
                    }
                    // Switch it off by its cheapest control: the position a drill sets it back to.
                    for (const Condition& n : d->enable) {
                        if (!IsControl(n)) continue;
                        const int off = OffPosition(n);
                        if (off >= 0) {
                            Need(x, n.id, off, up, {});
                            return;
                        }
                    }
                    for (const Condition& n : d->enable) {  // else through an upstream subsystem
                        if (n.kind == Condition::Kind::SubsystemIs) {
                            Condition o = Condition::Subsystem(n.id, SubsystemState::Off);
                            Expand(x, o, up);
                            return;
                        }
                    }
                }
                return;
            }
        }
    }

    static bool IsControl(const Condition& c) {
        return c.kind == Condition::Kind::ControlEquals || c.kind == Condition::Kind::ControlAtLeast;
    }

    // The position that breaks a control condition: the one a drill moves the control to, else 0 (or 1 if it needs 0).
    int OffPosition(const Condition& n) const {
        for (const Procedure& p : console_.procedures) {
            for (const ProcedureStep& s : p.steps) {
                if (s.control_id != n.id) continue;
                const bool breaks = n.kind == Condition::Kind::ControlEquals ? s.value != n.position : s.value < n.position;
                if (breaks) return s.value;
            }
        }
        const ControlDef* d = console_.panel.Find(n.id);
        if (d == nullptr || d->kind == ControlKind::Continuous) return -1;
        return n.position == 0 ? 1 : 0;
    }

    bool IsGuardOf(const std::string& control, const std::vector<std::pair<std::string, int>>& todo) const {
        for (const auto& a : todo) {
            const ControlDef* d = console_.panel.Find(a.first);
            if (d != nullptr && d->guard_id == control) return true;
        }
        return false;
    }

    static bool Covered(const std::vector<std::string>& procs, const ConsoleData& console, const std::pair<std::string, int>& a) {
        for (const std::string& id : procs) {
            const Procedure* p = console.FindProcedure(id);
            if (p == nullptr) continue;
            for (const ProcedureStep& s : p->steps) {
                if (s.control_id == a.first && s.value == a.second) return true;
            }
        }
        return false;
    }
    bool Covered(const std::vector<std::string>& procs, const std::pair<std::string, int>& a) const { return Covered(procs, console_, a); }

    // Greedy: the seat's drill that covers most of its needed actions, while most of the drill's steps are needed (a drill
    // that would do much else, such as a launch, is not pulled in for one switch); ties to the shorter drill.
    void ChooseDrills(const std::string& station, const std::vector<std::pair<std::string, int>>& todo,
                      const std::vector<std::string>& preferred, std::vector<std::string>& out) const {
        std::vector<std::pair<std::string, int>> left = todo;
        for (const std::string& id : preferred) {  // the order's own drills first, wherever they help
            const Procedure* p = console_.FindProcedure(id);
            if (p == nullptr || p->station != station || std::find(out.begin(), out.end(), id) != out.end()) continue;
            bool helps = false;
            for (const auto& a : left) helps = helps || Covered({id}, a);
            if (!helps) continue;
            out.push_back(id);
            std::vector<std::pair<std::string, int>> rest;
            for (const auto& a : left) {
                if (!Covered({id}, a)) rest.push_back(a);
            }
            left.swap(rest);
        }
        for (;;) {
            const Procedure* best = nullptr;
            int best_cover = 0;
            for (const Procedure& p : console_.procedures) {
                if (p.station != station || std::find(out.begin(), out.end(), p.id) != out.end()) continue;
                int cover = 0;
                int useful = 0;
                for (const ProcedureStep& s : p.steps) {
                    const bool need = std::find(left.begin(), left.end(), std::make_pair(s.control_id, s.value)) != left.end();
                    cover += need ? 1 : 0;
                    useful += need || IsGuardOf(s.control_id, left) ? 1 : 0;
                }
                if (cover == 0 || 2 * useful < static_cast<int>(p.steps.size())) continue;
                if (cover > best_cover || (cover == best_cover && best != nullptr && p.steps.size() < best->steps.size())) {
                    best = &p;
                    best_cover = cover;
                }
            }
            if (best == nullptr) return;
            out.push_back(best->id);
            std::vector<std::pair<std::string, int>> rest;
            for (const auto& a : left) {
                if (!Covered({best->id}, a)) rest.push_back(a);
            }
            left.swap(rest);
        }
    }

    // A seat's drills in an order that works: a drill that waits for something another of the seat's drills switches on
    // goes after it; otherwise the console file's order (which follows the drill book).
    void OrderDrills(std::vector<std::string>& procs) const {
        auto index_of = [&](const std::string& id) {
            for (std::size_t i = 0; i < console_.procedures.size(); ++i) {
                if (console_.procedures[i].id == id) return i;
            }
            return console_.procedures.size();
        };
        std::sort(procs.begin(), procs.end(), [&](const std::string& a, const std::string& b) { return index_of(a) < index_of(b); });
        for (std::size_t pass = 0; pass < procs.size(); ++pass) {
            bool swapped = false;
            for (std::size_t i = 0; i + 1 < procs.size(); ++i) {
                if (Needs(procs[i], procs[i + 1])) {
                    std::swap(procs[i], procs[i + 1]);
                    swapped = true;
                }
            }
            if (!swapped) return;
        }
    }

    // Whether drill `a` waits for something drill `b` does (through the subsystems' enable chains).
    bool Needs(const std::string& a, const std::string& b) const {
        const Procedure* pa = console_.FindProcedure(a);
        const Procedure* pb = console_.FindProcedure(b);
        if (pa == nullptr || pb == nullptr) return false;
        std::set<std::string> controls_b;
        for (const ProcedureStep& s : pb->steps) controls_b.insert(s.control_id);
        for (const ProcedureStep& s : pa->steps) {
            for (const Condition& w : s.wait_for) {
                std::set<std::string> seen;
                if (DependsOnControls(w, controls_b, seen)) return true;
            }
        }
        return false;
    }

    bool DependsOnControls(const Condition& c, const std::set<std::string>& controls, std::set<std::string>& seen) const {
        if (IsControl(c)) return controls.count(c.id) != 0;
        if (!seen.insert(c.id).second) return false;
        if (c.kind == Condition::Kind::SubsystemIs) {
            const SubsystemDef* d = console_.equipment.Def(c.id);
            if (d == nullptr) return false;
            for (const std::vector<Condition>* l : {&d->enable, &d->enable_any, &d->prerequisites}) {
                for (const Condition& n : *l) {
                    if (DependsOnControls(n, controls, seen)) return true;
                }
            }
            return false;
        }
        const CrewFact* f = crew_ != nullptr ? crew_->FindFact(c.id) : nullptr;
        if (f == nullptr) return false;
        for (const Condition& n : f->needs) {
            if (DependsOnControls(n, controls, seen)) return true;
        }
        return false;
    }

    // A loose action: its cover opened first, a key turned one position at a time.
    void AddLoose(DrillPlan::Seat& seat, const std::pair<std::string, int>& a, const Expansion& x) const {
        const ControlDef* d = console_.panel.Find(a.first);
        if (d == nullptr || d->kind == ControlKind::Continuous) return;
        const std::vector<Condition>& waits = x.waits.count(Key(a.first, a.second)) != 0 ? x.waits.at(Key(a.first, a.second)) : std::vector<Condition>();
        if (!d->guard_id.empty() && x.panel.Get(d->guard_id) < 1) {
            seat.loose.push_back({seat.station, d->guard_id, 1, std::string(), 0, "open " + LabelOf(d->guard_id, 1), 0.0});
            seat.loose_waits.push_back({});
        }
        int from = x.panel.Get(a.first);
        if (d->stepwise) {
            while (from != a.second) {
                from += a.second > from ? 1 : -1;
                seat.loose.push_back({seat.station, a.first, from, std::string(), 0, LabelOf(a.first, from), 0.0});
                seat.loose_waits.push_back(from == a.second ? waits : std::vector<Condition>());
            }
            return;
        }
        seat.loose.push_back({seat.station, a.first, a.second, std::string(), 0, LabelOf(a.first, a.second), 0.0});
        seat.loose_waits.push_back(waits);
    }

    const ConsoleData& console_;
    const CrewData* crew_;
    double reaction_s_;
};

}  // namespace simcore
