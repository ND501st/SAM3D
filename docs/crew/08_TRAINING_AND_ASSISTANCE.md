# 08. In the crew: training, assistance and grading

The first half of the game (pillar 2): the player is one member of the team, learning the drill and how to operate
each role or seat, with help overlaid from the perfect performance (pillar 6).

## 1. Curriculum per seat

Each seat has the same ladder of lessons; each lesson is a scenario with the player in that seat, the other seats
NPCs, and a task the drill master knows.

| Step | Lesson | Task (goal) |
|---|---|---|
| 1 | The seat: what each control, lamp and display is | Free exploration; the overlay names whatever the player points at |
| 2 | The seat's part of "К бою" from cold | `ready_halted` (`03` §2.1) |
| 3 | The seat's part of search, designation and capture | `captured` on a practice drone |
| 4 | The full engagement from the seat | A hit on a practice drone |
| 5 | Low targets and helicopters (НЛЦ, TV, Ф) | `03` V9, V10 |
| 6 | Jamming (9.1) and EMCON | `03` V11–V14 |
| 7 | Faults and recovery | `03` V18 |
| 8 | March, halt, stow; reload and refuel | `03` §2.6, §2.7 |
| 9 | Exam | Any of the above, unassisted, graded |

The driver's ladder is power, march, halt and the column; the commander's ladder is the command half (`07`).

## 2. Assistance levels

| Level | What the overlay does | Who it is for |
|---|---|---|
| Demonstrate | The NPC who normally holds the seat does it while the player watches; the overlay highlights each control as it is moved and says why | First time on a seat |
| Guided | The overlay highlights the drill master's best next action for the player's seat, with its why chain and when it is due; the NPCs call it out too | Learning |
| Hint | Nothing until the player hesitates longer than an NPC's patience at his seat's tier; then the highlight | Practising |
| Unassisted | No overlay; the NPCs still remind as they would a crewmate | Proficient |
| Exam | Unassisted and graded against the norms | Qualification |

What the overlay shows, from the drill master (`06` §3.4):

- the control to touch and the position to set (a highlight on the 3D control, a ghost hand or arrow);
- one line: what and why ("ПЕРЕДАТЧИК ВКЛ. — the tracking radar's transmitter: needed for the capture");
- when: "now" or "after ГОТОВ, about 40 s";
- a warning on a move that would fault equipment;
- after a move off the plan: what the best move was and what it cost (seconds, or a deviation).

As built, 17 October 2026 (`simcore/crew_training.hpp`, `ScenarioRunner::Assistance`): the levels are as in the
table, set by the scenario's `assistance` (default guided). In the cabin, H cycles guided, hint, demonstrate and off;
an exam scenario keeps the overlay off.

- **The task** follows the fight: К бою until ready, then the capture, then the launch (`TrainingTask`).
- **Hint** waits for the player's hesitation: time since he took the seat or last moved anything there, against the
  patience of that seat's tier.
- **Demonstrate** names the move the seat's NPC is about to make (`CrewMember::CurrentIntent`) with the drill master's
  why. Its body makes the move in the cabin (`10`).
- **Every move the player makes at a held seat is judged** against the drill master's best (`ScenarioRunner::MoveVerdicts`).
  A move off the drill shows at once: "Off the drill: that cost N s. The best move was ...". Costly moves reach the
  debrief.
- **The ghost hand** pulses on the advised control (Unreal `USimCrewBodiesComponent::SetGhostHand`).

As built, 21 October 2026: **the helper** (`14_HELPER.md`) takes over the overlay. Guided and hint show what the seat's
NPC would do now (its duties, the ordered drill's next step, the wheels it would turn; in command, the shadow
commander's orders), not only the drill master's next switch; the drill master stays its fallback where nobody orders
the seat. The level is set in the missions menu (kept between missions) and H in the cabin.

## 3. Grading

The debrief already lists crew reports, faults and deviations (`debrief.hpp`, no points: SAM Simulator's rule). The
grading adds, per task and per seat:

| Measure | From |
|---|---|
| Time to the task vs the ideal | The drill master's ideal timeline at the start of the task (`06` §3.3) |
| Seat delays | Where the actual timeline lagged the ideal, attributed to the seat whose action was late (the critical path) |
| Deviations | Out-of-order and early actions (`ProcedureTracker`) |
| Faults caused | Equipment faults after the player's action |
| Reminders received | `04` §5 |
| Decisions | Launch timing, target choice and method vs the drill master's evaluation |

Norms (excellent / good / satisfactory, the Soviet "отлично / хорошо / удовлетворительно") are set per task from the
ideal time: within 110 %, 130 %, 160 % of it with no faults (sam3d; to be replaced by published нормативы when found:
an open research item).

## 4. Quests and progression

- **The player's qualification**: passing a seat's exam qualifies him in that seat. Qualification in the working seats
  unlocks the vehicle commander's seat; the vehicle commander's exams unlock the battery.
- **The crew's skill** (`05` §5): the campaign sets who the NPC crewmates are and at what tier; they gain experience
  in the drills and engagements they take part in with the player and are promoted at thresholds. Some quests are
  about the crew: train a recruit operator to 3rd class, run "К бою" in under the norm with a mixed crew.
- Scenario files say which seat the player starts in, the assistance level and the crew's tiers (`crew[].skill`,
  phase 8.1).
