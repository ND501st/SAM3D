# 06. The reference performer: the drill master

Pillar 6 asks for an (almost) objective **perfect task performance**, from which help and assistance are overlaid in
training: "something like a chess bot that always knows the best possible move". SAM3D's answer is the **drill master**
(`simcore/drill_master.hpp`). This file is its specification.

## 1. Why a planner and not a script

A recorded "perfect run" only helps when the player is exactly on it. A chess engine is useful because it answers from
**any** position. The drill master does the same: from the console's current state (any switch in any position, any
subsystem warming or faulted, any target situation) it computes:

1. the **best next action for every seat** now (and why);
2. the **ideal timeline** from now to the goal (every seat's actions with times), which gives the time a perfect crew
   would still need;
3. the **cost of a mistake** (how much later the goal comes after the action actually taken).

It can do this because the console data already **is** the dependency graph: every subsystem lists the conditions
(controls, other subsystems, inputs) that switch it on, its warm-up time, and every control belongs to one seat. The
drill master adds what the data can't hold: the **world facts** (what makes "target captured" or "target in zone"
true, `02`), and the **decisions** (which target, when to launch, which method).

## 2. Inputs

| Input | From |
|---|---|
| Stations, controls (with positions, covers, locks), subsystems (enable / enable_any / prerequisites / latching / release, start and stop times), inputs | `data/procedures/<system>.json` via `ConsoleData` |
| Procedures (the sourced drills) | same |
| The current panel and equipment state | the live console |
| World-fact rules: which seats and conditions make each simulation input true | `data/crew/<system>_crew.json` (`facts`) |
| Tasks: named goals ("ready, halted", "engage") as conditions | `data/crew/<system>_crew.json` (`tasks`) |
| The reference skill (Master tier, no jitter) | `data/crew/skill_levels.json` |

## 3. The algorithm

### 3.1 Goal expansion (backward chaining)

A goal is a set of conditions. Each unmet condition expands:

| Condition | Expands to |
|---|---|
| Control C at position P | An **action**: seat = C's station; plus whatever physically blocks the move (open the cover first; a key switch steps one position at a time) |
| Subsystem S ready | All of S's `enable`, the **cheapest** of its `enable_any` (by estimated time to make true), S's `prerequisites`; then S's `start_time_s` as a **duration** after the last of them |
| Subsystem S off | The cheapest single `enable` condition to make false (or the latching `release`) |
| Input I = v (a world fact) | The fact's rule: other conditions (subsystems, controls), knowledge (a target known with azimuth and range), and world waits ("the target closes to the zone": a time, not an action), or another seat's mobility action (halt) |

Expansion stops at conditions already met. Cycles are cut (a condition already being expanded is not expanded again).
The result is a **dependency graph**: actions (owned by seats) and durations (warm-ups, world waits), with precedence.

### 3.2 Drill discipline

Real crews are graded on doing the drill **in its order**, not just reaching the goal. So the drill master prefers the
sourced procedures: every needed control action that is a step of a procedure of its seat pulls in that procedure from
the step's current place to the end (or to the needed step), in order. Actions that are in no procedure (hand wheels,
a single switch) stay loose actions. The plan is therefore **procedures per seat, in order, plus loose actions**.

### 3.3 Scheduling (the ideal timeline)

The plan is turned into a timeline by **running it**: a **ghost run** on a copy of the console (the same `Equipment`
model, so warm-ups, faults and latches behave exactly as in play), with a perfect crew at the reference skill doing
each seat's queue as fast as the conditions allow. Each seat is one person: one action at a time, each taking the
reference reaction time; seats work in parallel. Order across seats follows the precedence; ties go to the action on
the **critical path** (the longest remaining chain of durations to the goal), which is the classic list-scheduling rule
and optimal for these short chains of mostly independent seats.

The timeline lists, per seat, every action with its time, and the time the goal is reached. It is the **ideal
progression of events** of pillar 6.

### 3.4 The best move now (receding horizon)

Like a chess engine re-searching after every move, the drill master re-plans from the live state whenever it is asked
(each time the player looks for help, on every change of the console when the overlay is on). The first action of each
seat's queue is that seat's **best move**; with it come:

- the **why chain**: the action → the subsystem it enables → … → the goal ("ПЕРЕДАТЧИК ВКЛ.: the tracking radar's
  transmitter, needed for the capture, needed to engage target 6201");
- **when** it is due in the ideal timeline (now, or after a warm-up: "wait for ГОТОВ, about 40 s");
- **what not to do**: actions that would fault a subsystem (energising before its prerequisites) are flagged.

### 3.5 The cost of a move

To judge a player's action, the drill master compares the ideal time to the goal **before** and **after** it: an
action off the plan that delays the goal costs that delay; one that faults equipment costs the restart; one out of the
drill's order costs a deviation even if it costs no time. This is what grading and the overlay's "that was not the best
move" use (`08`).

### 3.6 Decisions under uncertainty (the evaluation)

Some choices aren't in the console's graph: which target first, when to launch, radar or TV, ПОМЕХА or not, salvo or
single, whether to move. For these the drill master scores the options with an **evaluation**, the way a chess engine
scores positions, built from SAM3D's own models:

| Decision | Options | Score | Rules the score must respect (sourced) |
|---|---|---|---|
| Which target | Known targets | Threat (time to reach the defended point or weapon release; type) × the chance of killing it (below) / the time needed | A friend (IFF, board code 1x) is never chosen |
| When to launch | Now, or wait | The gameplay outcome model's chance (`engagement.hpp`: zone, method, height, manoeuvre) for a launch now vs later, minus the cost of waiting (the target's time to release) | First missile to meet the target at the far edge of the zone; re-engage while ЦЕЛЬ В ЗОНЕ stays lit (`voenizdat_osa_1991` p257) |
| Tracking method | Radar, TV, ПОЛУАВТОМАТ, jammer track | The chance each gives (TV factor, method Н factor), whether it can be held (НЛЦ, visibility, jamming) | Low targets: TV angles with radar range; in heavy jamming, angles from one sensor and range from the search radar (`korovin_tiv_2010`) |
| ПОМЕХА | On / off | With no range at launch, off means an unfused missile | `samsim_osa` p34 |
| Salvo | One or two | The kill chance of two vs the missiles left and the targets coming | Interval ≥ 4 s (ePap) |
| Move | Stay, move within reach, relocate | The target's track vs the zone from here and from reachable positions; time to get there | Target beyond 20 km on the march: pull off; within 20 km: stop at once (`mon_2112_80`) |

Where a source states a rule, the rule wins over the score.

## 4. The reference for the engagement itself

Inside the engagement (designation → launch → result), the timeline comes from the console where the console has the
steps (switches, keys, ПУСК) and from the **network chart** (`voenizdat_osa_1991` Fig. 7.2) for the parts that are
crew work on the instruments: the tracking radar onto the target 10 s, capture 6–7 s, computer 3 s, missile
preparation 15 s, launch 1.2 s, salvo interval 4 s, watching the result 10 s. The drill master's engagement reference
is that chart, laid on the actual target's geometry (when the target enters the zone).

## 5. Interface (simcore)

```cpp
// A task: conditions to make true, with a name for the overlay and the debrief.
struct CrewTask { std::string id, name; std::vector<Condition> conditions; };

class DrillMaster {
public:
    DrillMaster(const ConsoleData& console, const CrewData& crew);
    // The plan from the current state: for each seat, its procedures and loose actions in order.
    CrewPlan Plan(const CrewTask& task, const ControlPanel& panel, const Equipment& equipment) const;
    // The ideal timeline from the current state (a ghost run on a copy of the console).
    IdealTimeline Ideal(const CrewTask& task, const ConsoleData& now, double max_s = 1200.0) const;
    // Each seat's best next action now, with the why chain and when it is due.
    std::vector<Advice> NextBest(const CrewTask& task, const ConsoleData& now) const;
    // The cost of an action: how much later the task is done after it than before it.
    double CostOf(const CrewTask& task, const ConsoleData& before, const std::string& control, int position) const;
};
```

## 6. What it is not

- Not a learner: no training data, no randomness. Given the same state it gives the same answer, and every answer can
  be explained by the chain of conditions in the data.
- Not a replacement for the drills: where a source gives the order of steps, the drill master follows it.
- Not omniscient in the world: it plans from what the crew knows. Where it scores a decision on the simulation's
  models (the outcome chance), it uses the crew's knowledge of the target, not the truth; the debrief may afterwards
  compare with the truth.
