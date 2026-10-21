# Crew and teamwork: design reference

Written 16 October 2026 at the start of phase 8. This folder is the reference for everything the crew does in SAM3D:
who does what, what each crew member knows and how they come to know it, how every action depends on the others, how
the crew behaves in every situation, what they say, how good they are, and how the game knows the best possible
performance at every moment. It is written before the code and the code follows it; when they disagree, fix one of
them in the same change.

## The project owner's instructions (16 October 2026)

These are the design pillars of SAM3D's crew. They are repeated in `CLAUDE.md`, the roadmap, the architecture and the
handoff so that no change loses them.

1. **Teamwork with NPC crewmates is paramount.**
2. **The game has two halves.**
   - **In the crew:** the player is one member of the team, learning the drill and how to operate each role or seat.
   - **In command:** the player is in charge of the team: gives the orders and commands first the single Osa, then the
     whole battery.
3. **Every NPC crewmate can work with every other**, and **every action and command is mapped out and interconnected.**
4. **NPC behaviour paths are mapped for every operational environment and every outcome**, with **callouts,
   instructions and commands for every possible action**.
5. **Skill levels:** NPCs have different skill levels, set by the player's progression and quests. Skill affects
   reaction times, speeds and accuracy **in every facet of activity**.
6. **An ideal progression of events:** for every task there is an (almost) objective **perfect task performance**. It is
   the reference for the help and assistance overlaid during the player's training while they learn each task and role.
   It is like a chess engine that always knows the best possible move (SAM3D's interpretation: `06_REFERENCE_PERFORMER.md`).
7. **Interdependence:** for one crewmate to act (lock onto an aircraft), another must know where the aircraft is
   (radar, optics, early-warning radar, radio), and another must have driven the vehicle within range of the air
   battle. This interdependence is mapped accurately, with every possible action and combination factored in.

## How SAM3D reads them (decisions taken here)

| Pillar | Interpretation | Where |
|---|---|---|
| Teamwork | Nothing is done by "the vehicle": every action has an owner seat, preconditions that are other seats' actions or knowledge, and a callout that tells the others. Knowledge moves only through instruments and voice. | `01`, `02` |
| Two halves | One simulation, one crew API. A seat is held by the player or by an NPC and nothing else changes. "In the crew": NPC commander, the player in a seat. "In command": the player is the commander (vehicle, later battery); NPCs fill every seat. | `01`, `07`, `08` |
| Every action mapped | Generated from the data, so it can't fall behind: `tools/crew_map.py` writes every control, subsystem, input, procedure and their links (`generated/`). Hand-written parts cover what the data can't say (knowledge, decisions, speech). | `02`, `generated/` |
| Every environment and outcome | A finite list of environments (garrison, march, halt, position, formation, ambush, river, night, jamming, EMCON, low targets, helicopters, saturation, friends, C2 loss, faults, reload, refuel, relocation, battery) and of outcomes per phase, each with its path per seat. | `03` |
| Callouts and commands | A catalogue in data (`data/crew/osa_ak_crew.json`): the order, the acknowledgement, the report, in Russian (as the crew spoke) and English, with sources where the phrasing is documented and "sam3d" where it is ours. | `04` |
| Skill | Eight dimensions (perception, reaction, dexterity, accuracy, procedure, judgement, communication, composure), five tiers named after the Soviet specialist classes, per crewmate and per seat; all numbers game values calibrated to the few published crew times. | `05` |
| Perfect performance | The **drill master** (`simcore/drill_master.hpp`): a planner over the console's own dependency data that, from any state, gives the best next action for every seat and the ideal timeline to a goal; decisions under uncertainty by a doctrine-and-outcome evaluation. It drives the training overlay, the grading and NPC initiative. | `06`, `08` |
| Interdependence | The kill chain (detect, designate, acquire, identify, decide, prepare, launch, guide, assess) and the mobility chain (position, drive, halt, deploy, orient) as a graph of seat actions and information hand-offs. | `02` |

## Contents

| File | What |
|---|---|
| `01_ROLES_AND_KNOWLEDGE.md` | The five seats of the Osa (and the battery's), their duties, their instruments, what each knows and how |
| `02_INTERDEPENDENCE.md` | The kill chain and the mobility chain as a dependency graph; hand-offs; who waits for whom |
| `03_ENVIRONMENTS_AND_PATHS.md` | Every operational environment and outcome, and every seat's behaviour path through them |
| `04_COMMANDS_AND_CALLOUTS.md` | Orders, acknowledgements, reports and cross-seat callouts; the phrasing rules |
| `05_SKILL.md` | The skill model: dimensions, tiers, calibration, progression |
| `06_REFERENCE_PERFORMER.md` | The drill master: the algorithm that knows the best move |
| `07_COMMANDER_MODE.md` | The player in command: the order menu for the vehicle, then the battery |
| `08_TRAINING_AND_ASSISTANCE.md` | The player in the crew: curricula per seat, the assistance overlay, grading, quests |
| `09_IMPLEMENTATION_PLAN.md` | Phases, what is built, what is next |
| `10_BODIES_AND_VOICES.md` | The crewmates seen and heard: the body model, the reach check of the cabin, the voices |
| `11_PLAYTEST_AND_BALANCE.md` | Every tier played over many seeds against the published norms, and what that showed |
| `13_HELPER.md` | The helper: the move your seat's NPC would make now, shown as a lesson's step is; how it stays up to date |
| `generated/osa_ak_action_map.md` | Every action of the Osa-AK console and how it connects (`python tools/crew_map.py`) |
| `generated/helper_coverage.md` | Every mission from every seat, following the helper against the NPC crew (`simcore/build/helper_coverage`) |

## Words used here

| Word | Meaning |
|---|---|
| Seat | A console station (`commander`, `deputy_commander`, `senior_operator`, `operator`, `driver`). One person each. |
| Crewmate | Whoever holds a seat: an NPC or a player. |
| Order | From the commander (or another seat's senior) to a seat: "do this". Always acknowledged. |
| Report | From a seat to the commander: "done", "ready", "I see", "I can't". |
| Callout | Any spoken line on the intercom (R-124) or the radio (R-123M): orders, reports and warnings. |
| Drill | A sourced sequence of control actions for one seat (`data/procedures/<system>.json` procedures). |
| Task | What an order asks for, as conditions on the console and the world (e.g. "ready to fire" = `launch_armed` ready). |
| Reference performance | The drill master's ideal timeline for a task from the current state. |
| Knowledge | What a crewmate has seen on their own instruments or been told; never the simulation's truth. |

## Sources

The crew's titles, seats and duties: `flak11` (NVA), `wrk`, `voenizdat_osa_1991` (the "К бою" drill, pp. 246–258,
with its spoken reports, and the network chart of an engagement, Fig. 7.2), `mon_2112_80` (Polish manual: march,
deploy, operating rules), `korovin_tiv_2010`, `samsim_osa` and `samsim_thread` (SAM Simulator and its creator),
`video_osa` (unverified, used only where marked). Everything else here is SAM3D's design and is marked "sam3d".
