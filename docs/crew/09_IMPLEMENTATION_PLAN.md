# 09. Implementation plan

Phase 8 of the roadmap, re-planned on 16 October 2026 around the crew pillars (`README.md`). Each step is a PR with
tests; the Unreal parts come after the simcore parts they show.

| Step | What | Done when |
|---|---|---|
| 8.0 | This reference (`docs/crew/`), the pillars in every document, and the generated action map (`tools/crew_map.py` → `generated/osa_ak_action_map.md`, checked in sync by a test) | The map lists every control, subsystem, input and procedure of the Osa with owners and links |
| 8.1 | Crew data: `data/crew/osa_ak_crew.json` (seats' duties and instruments, orders, callouts, world-fact rules, tasks) and `data/crew/skill_levels.json` (tiers); loader `simcore/crew_data.hpp`; scenario `crew[].skill`; citations checked | Loads, checked against the console (every procedure, control, input it names exists) |
| 8.2 | Skill in behaviour: a crew member's reaction, jitter, slips and omissions from its tier; the engagement's NPC wheel rates, TV lag and aim noise per seat; stress | A recruit crew is measurably slower and less accurate than a master crew in the same scenario; calibration bands of `05` §3 hold |
| 8.3 | The drill master v1: plan, ideal timeline by ghost run, best next action per seat with the why chain, cost of an action; tasks from data | From cold, the ideal "К бою" is within the 4 min norm; the best move from any state of a test set is the expected one |
| 8.4 | Callouts: a callout bus on the intercom and the radios (speaking time, one voice per channel, urgent first); NPC reports and acknowledgements from the catalogue; reminders with patience; the debrief and the radio log show them | Every report of `04` §3.6 that the sim can trigger is spoken at the right moment in a test run |
| 8.5 | Knowledge per seat: what each NPC acts on comes from its instruments and the callouts it heard (the operator ranges on the senior operator's callout, not the truth) | An NPC who missed a callout doesn't act on it |
| 8.6 | The NPC vehicle commander: the paths of `03` driven by the crew's reports and the radio; a scenario can leave the crew to its commander instead of scripting triggers; the player in any seat | A scenario with no engagement triggers is fought end to end; with the player in a seat, the NPCs wait for and remind him |
| 8.7 | Assistance and grading: the overlay API (best move, why, when, cost) in Unreal; the grading in the debrief | A guided lesson highlights each next control in the cabin |
| 8.8 | Commander mode: the order menu in Unreal (radial or list), honest greying | A player commands "К бою" to a hit without touching a console control |
| 8.9 | The battery: NPC vehicle commanders, fire distribution, the battery commander's seat (with the PU-12M work, `docs/PU12_DEVELOPMENT.md`) | A player commands a battery of four NPC crews |
| 8.10 | Bodies, animations, seats, voice lines (the former 8.1) | Crewmates seen working their controls and heard |
| 8.11 | One mission end to end (the former 8.3): march, position, orient, deploy, function check, engage, reload, stow, debrief | M2: a person plays the whole mission without the editor |
| 8.12 | Playtest and balance (the former 8.4), including the skill table | – |

## Status

- 8.0: done 16 October 2026 (this folder, `tools/crew_map.py`).
- 8.1–8.4: started 16 October 2026 (see the roadmap).
