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

## Status (16 October 2026)

- **8.0 done**: this folder, `tools/crew_map.py`, the pillars in every document.
- **8.1 done**: `data/crew/osa_ak_crew.json`, `data/crew/skill_levels.json`, `simcore/crew_data.hpp`; scenario
  `crew[].skill`, `own_unit.crew_data` (default `crew/<system>_crew.json`).
- **8.2 done** (first pass): reaction and jitter per action, slips (crew mistakes in the debrief), omissions, stress
  (`CrewMember::SetSkill`, `SetStressed`); the engagement's NPC wheel rates, TV lag and aim noise per seat, range error
  by hand (`AimCrew`). Not yet: judgement (launch timing, priorities), communication misses, stress set by the
  situation (the flag exists; nothing sets it yet), the calibration bands of `05` §3 as tests.
- **8.3 done** (v1): `simcore/drill_master.hpp`: plan (with the doctrine's preferred ways and the order's own drills,
  `GoalFor`), the ideal timeline by a ghost run, the best next move per seat with its why chain, the cost of a move.
  Ideal К бою from cold: 254.9 s. Not yet: recovering faulted equipment (switch off and on), the engagement network
  chart as part of the timeline, decisions under uncertainty (`06` §3.6).
- **8.4 done** (first pass): `simcore/crew_callouts.hpp` (one voice per channel, urgent first, speaking time); orders
  spoken with their acknowledgements and "done"; automatic reports from the data; contacts, losses, the jammer, IFF,
  hits and misses from the engagement's events; designations passed on; faults; reminders (a waiting NPC names the
  seat that can act; the NPC commander watches his orders to a player's seat and names the step); all to the radio
  log (the HUD) and the debrief.
- **8.6 done** (vehicle, first pass): `"crew_commander": "npc"`: the NPC commander brings the crew to readiness by
  the drill master's plan and fights by the crew data's orders (take, prepare, permit, jammer, low target); a player in
  a seat gets his orders and is reminded; a player in command gives orders through `GiveCrewOrder`. Scenario
  `osa_crew_practice.json`. Not yet: the march (he doesn't order the vehicle to move), target priorities among
  several, the second missile, reload and refuel, C2 source changes.
- **8.7, 8.8 first pass** (17 October 2026): in the cabin, H turns on the training overlay (the drill master's next control
  for the seat pulses green; the HUD says what, why and when; the task follows the fight: К бою, the capture, the
  launch), O opens the crew menu (orders of the crew data at the commander's seat, the seat's reports elsewhere), T takes
  the seat from its NPC (his orders become the player's, the crew waits for and reminds him) or gives it back. The
  debrief grades К бою against the drill master's ideal (excellent / good / satisfactory, within 110 / 130 / 160 %),
  detection to launch against the published 26–40 s when the target appeared in reach, and lists each seat's
  reminders, slips and forgotten steps. Not yet: a ghost hand, the cost of a wrong move shown at once, a radial menu,
  greying of orders that can't be carried out.

Second session (17 October 2026):

- **8.5 done**: `simcore/crew_knowledge.hpp`: each seat's picture from its own instruments (the senior operator's
  scope, the operator's range indicator) and from what it heard (callouts carry what they say about a target). The
  operator puts the range on what he sees or was told, never the truth; a crewmate may miss a report (his tier's miss
  chance) and asks "Повторите!"; the senior operator keeps reporting the nearest contact every 15 s until it is tracked;
  the NPC commander decides from his own picture; a player in a seat speaks reports through the menu (`PlayerSay`).
- **8.2 completed**: judgement (the decision controls, ПУСК, carry the tier's launch timing; the commander's priority
  slips and his hesitation at a branch), stress from the situation (own radars jammed; a hostile within 10 km and
  nothing tracked), and the calibration test: detection to launch per tier, master 29 s, 1st class 31 s, 2nd 38 s, 3rd
  41 s, recruit 81 s against the published 26–40 s.
- **8.3 completed** (but for the network chart in the timeline and the scored decisions of `06` §3.6): fault recovery
  (switch it off, its prerequisites, on again).
- **8.6 completed for the vehicle**: on the march (search on the move on the turbine; a hostile inside 20 km halts the
  vehicle at once, the battery's short halt when it drives us; a column stop is no halt); target priority (the nearest
  hostile he heard of, with his tier's priority slips); one launch permission per missile; logistics reports; between
  fights the function check (from a cold start only, broken off for a target) and, when it is quiet, К маршу and the
  march on.
- **8.9 first pass**: `simcore/battery_fire.hpp`: the battery's other launchers fight with an abstract crew (the network
  chart's times, geometric sensors, the same zones and hit chances, six missiles, TZM reload); the battery commander
  (NPC by default, the player with `"battery_commander": "player"`, or free fire) gives them targets from the PU-12 board
  on the battery net, leaving to our vehicle the targets it is nearest; our commander leaves theirs alone. Scenario
  `osa_battery_fight.json`; Unreal lists them under the plotting board. Not yet: the PU-12M as a seat with its own
  console (`docs/PU12_DEVELOPMENT.md`), the other vehicles' crews as full NPC crews, sectors.
- **8.11 first pass**: `osa_mission_march.json`: the battery on the march with our vehicle, a raid, halt, deploy, fight,
  stow and march on, run by NPC commanders without a fault. Not yet: choosing the position, orientation as part of the
  readiness, a reload inside the mission, the map's version in Unreal.
- 8.10 (bodies, animations, voices) and 8.12 (playtest and balance): not started.

Third session (17 October 2026):

- **8.10 done (first pass), `10_BODIES_AND_VOICES.md`.**
  - **Bodies:** every NPC crewmate is a body in his seat (`simcore/crew_body.hpp`; Unreal
    `USimCrewBodiesComponent`, meshes from `tools/blender/build_crew_body.py`). His hand goes to the control he is about
    to move, grips the wheel he turns; he bends, turns on his seat, or stands up for a panel out of seated reach; he
    looks at what he works and nods when he speaks.
  - **Reach check:** a test finds the controls that need standing (the deputy's orientation, the commander's right
    radio), and the drill pays the time.
  - **Voices:** every callout is spoken in its seat's voice (`crew_voice.hpp`, `data/crew/voices.json`; Unreal
    `FSimSpeech` and `USimCrewVoiceComponent` on Windows' speech engine). It is Russian with a Russian voice installed,
    English otherwise; recorded lines take precedence.
  - **Checked:** headless Unreal tests for the bodies and the speech, and 16 poses rendered in the furnished cabin.
- **8.7 completed.**
  - **Levels:** the assistance levels demonstrate, guided, hint, unassisted and exam (scenario `assistance`; H cycles
    them in the cabin).
  - **Wrong moves:** the cost of a move off the drill is shown at once ("that cost N s; the best move was ...") and
    goes to the debrief.
  - **Ghost hand:** it pulses on the advised control.
- **8.8 completed but for a radial menu:** honest greying (`ScenarioRunner::CheckOrder`, orders' `needs`, facts'
  `unmet_ru`/`unmet_en`), and a refused order is answered "Не могу: <reason>".
- **8.3 completed.**
  - **Network chart:** the engagement reference lays the network chart on the target's zone entry (the debrief judges
    each launch).
  - **Scored decisions** of `06` §3.6 (`crew_decisions.hpp`): target choice, launch timing, judged in the debrief.
- **8.9 completed but for the PU-12M seat:** sectors of fire (battery vehicles' `sector_deg`;
  `ScenarioRunner::SetBatterySector`).
- **8.11 completed in simcore but for choosing the position:**
  - orientation in the readiness: the NPC commander has the vehicle oriented;
  - a reload inside the mission: `own_unit.empty_containers`; the mission starts with one missile and the TZM comes;
  - the NPC commander gives no order whose needs are missing, and waits for the reload before marching on.
- **8.12 first pass, `11_PLAYTEST_AND_BALANCE.md`:**
  - `crew_balance` plays every tier over many seeds. Trained crews meet the published reaction time, recruits don't;
    hits fall with the tier where there is more than one target; no tier faults equipment.
  - A cold К бою barely depends on the crew: the equipment's warm-ups dominate.
  - Two NPC commander faults were found and fixed.
- **Not yet:**
  - the PU-12M as a seat with its own console (`docs/PU12_DEVELOPMENT.md`);
  - the other vehicles' crews as full NPC crews;
  - choosing the position;
  - a radial menu;
  - skinned bodies, lips;
  - a person playing the missions on the map (the second half of 8.12).
