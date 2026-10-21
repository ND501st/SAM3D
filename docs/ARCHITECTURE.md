# Architecture

```
            ┌──────────────────────────── Unreal Engine 5 (Windows PC) ────────────────────────────┐
            │  Player pawn / vehicle / 3D cabin / crew animation / UI / audio / Cesium terrain       │
            │                     │ input: switch clicks, orders           ▲ entity states, events  │
            │                     ▼                                        │                          │
            │   USimWorldSubsystem  — owns simcore::World, fixed 100 Hz step, ENU<->Unreal mapping   │
            └─────────────────────┬────────────────────────────────────────┬─────────────────────────┘
                                  │ #include "SimCoreIncludes.h"           │
            ┌─────────────────────▼────────────────────────────────────────┴─────────────────────────┐
            │  simcore (header-only C++17, no engine deps)                                             │
            │   geo · atmosphere · terrain(interface) · radar · aircraft · missile · procedures · world │
            │   acmi (Tacview export)                                                                  │
            └─────────────────────▲──────────────────────────────────────────────────────────────────┘
                                  │ same models, same tests
                         prototype/ (Python)  — fast experiments on any machine
```

## Why this split

- **simcore is the game.** Everything that decides an outcome (detection, tracking, missile flight, fuzing,
  procedure correctness, crew actions) lives there. It's deterministic, fast to test, and runs without Unreal,
  so you can develop it on the Mac.
- **Unreal is the presentation.** Rendering, input, cabins, animation, audio, UI and terrain data.
- **The Python prototype** lets you try a model in minutes (and plot it) before committing it to C++.

## Coordinate frames

| Frame | Axes | Units | Used by |
|---|---|---|---|
| Geodetic (WGS-84) | lat, lon, alt above ellipsoid | rad (deg at edges), m | origin, terrain, Cesium, Tacview |
| ECEF | Earth-centred | m | conversions only |
| **ENU (sim frame)** | x=East, y=North, z=Up at the origin | m | all of simcore |
| Unreal world | X=North, Y=East, Z=Up (left-handed) | cm | all of Unreal |

With a Cesium georeference, Unreal positions go ENU → geodetic → Unreal through the georeference (Cesium's frame
is +X east, +Y south at its origin), and the director puts the georeference origin on the scenario origin
(`USimWorldSubsystem::ToUnreal` / `ToEnu`). Without Cesium, the flat X=North, Y=East mapping above is used.

`LocalFrame` converts geodetic ↔ ENU exactly (via ECEF). Far from the origin, ENU `z` differs from altitude by
about d²/2R (≈ 785 m at 100 km), so use `LocalFrame::AltitudeOf` whenever altitude matters.

## Time

`USimWorldSubsystem` accumulates frame time and calls `World::Step(0.01)` in a loop (fixed 100 Hz, capped per frame).
Everything in simcore is deterministic for a given sequence of inputs, which makes replays and tests reproducible.

## Controls, procedures and crew

**Crew and teamwork (the project owner's design pillars, 16 Oct 2026; `docs/crew/README.md`).** Teamwork with NPC
crewmates is paramount. The game has two halves: *in the crew*, the player is one member of the team, learning the
drill and how to operate each seat; *in command*, the player gives the orders, first for the single Osa, then for the
battery. Every NPC works with every other; every action and command is mapped and interconnected (the generated action
map), with behaviour paths, callouts and commands for every environment and outcome. NPC skill levels come from player
progression and quests and affect reaction, speed and accuracy everywhere. An ideal progression of events, the drill
master's perfect performance, is the reference for the training overlay, like a chess engine that always knows the
best move. One crewmate's action depends on others' knowledge and actions (someone must know where the target is,
someone must have driven within reach), and that interdependence is mapped. The crew design is `docs/crew/`; this section describes what is built.

- `ControlPanel` (`panel.hpp`): every physical control and its position. Kinds: discrete (toggles, selectors,
  covers, locks, key switches), continuous (hand wheels, with a range that clamps or wraps) and momentary
  (push-buttons, which read pressed until the step's `ReleaseMomentaries()`). Only physical constraints block a
  move: a closed cover or lock over a guarded control, or a stepwise control (key switch, detented rotary) jumping
  more than one position. These all come from SAM Simulator's Osa panels (`docs/re/osa.md`).
- `Equipment` (`equipment.hpp`): the circuits and mechanisms behind the panel. Each subsystem is off, starting,
  ready, stopping or faulted, with start and stop times (warm-up, deploy, stow). Switching something on before its
  prerequisites are ready, or losing a prerequisite while it runs, faults it. Latching subsystems are modes a
  button engages until a release condition. Inputs are yes/no signals the sim sets (e.g. "target in zone") so
  lamps and automatic sequences can use them. Lamps and animations read these states.
- Console data (`console_data.hpp`, `json.hpp`): each system's crew stations, controls, subsystems and procedures
  are loaded from `data/procedures/<system>.json` by a strict loader that requires a source and confidence on every
  item. Every control belongs to a station (the Osa-AK has five: commander, deputy commander, senior operator,
  operator, driver), and every procedure stays at one station, so each order goes to one seat.
- `Procedure`: an ordered list of steps `{control, position, min_wait_s, description, wait_for}`. `wait_for` is
  what the operator should see first (e.g. a READY lamp).
- `ProcedureTracker`: judges each action. It accepts correct steps, refuses ones before `min_wait_s`, and logs
  deviations with times: out-of-order actions, and "early" steps done before their `wait_for` held (these still
  count as done; `Equipment` decides the consequence). Physically blocked moves aren't judged.
- `CrewMember`: an NPC in one seat (its role is a station id) that works through ordered and queued procedures on
  the **same** panel, refusing another seat's procedures. It waits for
  `wait_for`, reacts with a delay once a lamp comes on, and reports started / completed / waiting / equipment fault /
  aborted for voice lines and the order UI.

- Crew data and skill (`crew_data.hpp`, `data/crew/`): seats, the callout catalogue, orders, tasks, world facts,
  automatic reports, skill tiers. A crew member with a tier (`CrewMember::SetSkill`) reacts per action with jitter,
  slips (an out-of-order action, logged as a crew mistake) and forgets steps for its patience; NPC aiming takes wheel
  rates, TV lag and aim noise from the tiers (`AimCrew`).
- The drill master (`drill_master.hpp`): the reference performer. Backward chaining over the console's own dependency
  data to each seat's actions, the sourced drills preferred, a ghost run on a copy of the console for the ideal
  timeline, each seat's best next move and the cost of a move. It drives reminders, the NPC commander's readiness
  orders and (to come) the training overlay.
- Callouts (`crew_callouts.hpp`): the crew's voice on the intercom and the battery net, one voice per channel, urgent
  first; `ScenarioRunner` speaks orders, acknowledgements, automatic reports, engagement events
  (`EngagementModel::TakeEvents`), designations and reminders, into the radio log and the debrief.
- The NPC vehicle commander (`ScenarioRunner`, `"crew_commander": "npc"`): readiness by the drill master's plan, the
  engagement by the crew data's orders, supervision of players' seats; `GiveCrewOrder` is the order menu's backend.
  On the march he halts for a hostile inside 20 km; between fights the function check (from cold) and the stow.
- Knowledge per seat (`crew_knowledge.hpp`): each seat's picture from its own instruments and the reports it heard;
  NPCs act on it (the operator's range, the commander's choice of target), never on the truth.
- The battery's fight (`battery_fire.hpp`): the other launchers with an abstract crew (the network chart's times at
  their tier, geometric sensors, the same zones and hit chances); the battery commander's fire distribution; sectors of
  fire.
- The crew's bodies (`crew_body.hpp`): a seated crewman from `data/crew/body.json`, reach planning (bend, turn on the
  seat, stand), two-bone IK, a per-seat animator driven by `ScenarioRunner::CrewActivity`; Unreal draws it
  (`USimCrewBodiesComponent`). Presentation, except the controls out of seated reach (crew data `stand_for`), which cost
  drill time.
- The crew's voices (`crew_voice.hpp`): what a speech engine says for a callout and in whose voice; Unreal speaks it
  (`FSimSpeech`, `USimCrewVoiceComponent`).
- Training and command (`crew_training.hpp`, `crew_decisions.hpp`): the assistance levels, each player move judged
  (`MoveVerdicts`), the honest order menu (`CheckOrder`, `TryCrewOrder`), the evaluation of target choice and launch
  timing judged in the debrief.

The player's 3D cabin clicks call the same functions, so the player and the crew share one source of truth.
- Player: `ScenarioRunner::PlayerSet` puts the player's moves through a `ProcedureTracker` (the procedure set with
  `SetPlayerProcedure`, or none for free play), so early and out-of-order actions reach the debrief like a crew
  member's; `PlayerTurn` turns hand wheels.
- Cabin layout (`cabin.hpp`, `data/cabins/<system>.json`): where stations, panels, controls and lamps sit in the
  vehicle's body frame, each control's look (toggle, rotary, key, button, wheel, cover), and when each lamp is lit
  (console conditions). Presentation data checked against the console: every control placed once, on its own
  station's panel. Its displays (search scope, range indicator, TV, dials, meters) say when each instrument is powered
  and lit. Unreal's `USimCabinComponent` builds and animates the cabin from it.
- Scopes (`scopes.hpp`): `SearchScope` keeps a search scope's sweep angle and fading echoes from the engagement
  model's search picture, so the display shows an echo only after the sweep passes it. The engagement model's
  `TrackReturns` are the tracking radar's echoes for the range indicator.
- `SessionLog` (`debrief.hpp`): crew reports, equipment faults and player deviations on one clock, formatted as a
  debrief timeline and summary. No points: SAM Simulator's rule is "no points, as in reality".
  `examples/procedure_demo.cpp` runs an Osa session from the march to the debrief.

- Scenarios (`scenario.hpp`, `scenario_runner.hpp`): `data/scenarios/<id>.json` places the unit, its crew and
  the targets, and lists triggers (time, target range, console conditions, launches) that set sim inputs, give
  crew orders or end the run. `ScenarioRunner` owns the World, console, crew and log, steps them in the order
  below, records launches, writes a Tacview track and produces the debrief. Runs are deterministic for a given
  seed. `examples/scenario_demo.cpp` plays a file.
- Engagement (`engagement.hpp`, numbers in `data/gameplay/<system>.json`): a deliberately gameplay-level model, not
  radar or missile physics. The console's `sensors` block maps the search radar, tracking radar and TV sight onto
  its controls. The model builds the search picture (range scale, line of sight, clutter/MTI), locks the tracking
  radar when the operator has the azimuth line and range gate on a target in acquisition mode (or the TV has it in
  view), follows it until the lock breaks, sets the capture / too-low / zone inputs, lets the aiming crew turn
  their wheels (azimuth, elevation and range each have their own seat), and resolves each launch: losing the track loses the missile, otherwise the hit chance is a product
  of plain, tunable factors printed in the debrief, decided by one draw from the scenario seed.

Per step: `Equipment::Update`, then `ControlPanel::ReleaseMomentaries`, then crew. Crew see this step's lamps, and
Equipment sees every button press exactly once.

## Electromagnetic layer

`simcore/include/simcore/em/` (namespace `simcore::em`) models radar detection and tracking, passive receivers
(RWR, ESM, ELINT, TDOA), triangulation, noise and deception jamming, chaff, radio nets and command posts, and each
unit's knowledge kept apart from truth, from one propagation model over the terrain. Radars and radios are fitted to
published ranges; data in `data/em/`. It runs at 10 Hz (`EmWorld::Step`), deterministically, and produces debug
overlay primitives drawn by Unreal (`FSimEmLayer`, `sam3d.em.Debug`) and by `tools/em_view.py`. It is not yet linked
to the Osa scenario's engagement model (roadmap E2). Full design: `docs/EM_ARCHITECTURE.md`.

## The enemy air command

`simcore/include/simcore/air_war/` (namespace `simcore::airwar`, umbrella `air_war.hpp`; design `docs/air_war/`) is a
campaign layer above the scenarios: an enemy staff of officers with characters plans air tasking orders on an
intelligence picture that never reads the truth; raids are resolved abstractly by the same rules the planner estimates
with, or, when they come through the player's battery's sector, become scenario JSON for the existing loader and runner
(NPC crew and battery, PU-12, C2, EM layer, prepared position) with a mission bridge that adds weapon release, ARMs and
the enemy's ELINT and brings the outcome back. The campaign then applies damage, losses, pilots' claims, sightings and
fixes, the officers' lessons, grudges and careers, politics, logistics and repair, and writes what our side learns.
Deterministic for a seed. `examples/air_war_demo.cpp` runs it end to end.

## Known limitations (deliberate, for now)

Generic simcore models (used by the demo, not by the Osa's outcomes):

- Aircraft: constant airspeed, turn-rate limited, and they fly straight lines in ENU, so they drift in true altitude over long distances. With a scenario's "weather" they drift with the wind and crab to hold their track (`weather.hpp`); missiles ignore the wind.
- Cloud blocks only the TV sight (and its EM-layer optical sensor is not yet told: `em/optical.hpp` is, through the engagement model). The map's obstacle grid (woods, buildings) is not yet used by line of sight (E9).
- Demo missile (`missile.hpp`): PN only, perfect target knowledge, no autopilot lag or noise, constant Cd,
  deterministic kill radius. The scenario path doesn't fly a missile; the engagement model resolves launches.
- Radar physics: single-look SNR threshold, no clutter or multipath.
- ISA atmosphere treats geopotential altitude as geometric (< 0.5 % error below 32 km).

Osa model gaps found by the October 2026 research review (`docs/research/OSA_RESEARCH_2026-10.md`), scheduled as
roadmap phase 6:

- Timings: power-up in about a minute; real cold start-up is about 6.8 min including the function check. (6.2)
- Missiles: load, selection, two channels, letter change, emergency launch and the range indicator's missile sweeps
  are modelled, and so is the 10-minute hold limit with the flashing ammunition lamp. (6.3)
- Search radar: the beam switch doesn't affect detection; no IFF; 2 s sweep instead of 33 rpm. (6.4)
- Tracking radar capture: azimuth, range and elevation (the antenna's 1° beam on the target; during ЦУ it scans the
  search beam). The scan period is an estimate, and in the automatic beam programmes the beam searched is the one the
  echo is seen in, not the real search radar's beam commands. Manual capture has no slow elevation scan of its own:
  the elevation wheel has to be on the target. (6.4)
- Heading: the gyro half-compass and its orientation drill are modelled (the crew's azimuths are in the gyro's
  frame); gyro drift, the OR-31M Qc scale and the crew chief's compass work are not, and the player deputy reads
  the commanded Qc from the log. (6.5)
- IFF: interrogation, replies on the scope and the gated mode; no codes, modes I/II or the AKM's launch blocking.
- Out of the vehicle (`simcore/crew_mobility.hpp`, `docs/crew/13_OUT_OF_THE_VEHICLE.md`, 8.14): the climbing times, the
  Osa's axles and which hatch each seat uses are estimates; the hatches don't touch the console's ЛЮКИ ЗАКР. (an open
  crew hatch doesn't hold off ПУСК ГОТОВ); the walk on the hull is in straight lines round rectangles, on the ground
  round the hull only (not round woods, buildings or water); a road's traffic side isn't kept; the drill master and the
  lessons don't know a job's walk; on a networked client the NPCs' bodies out of their seats aren't mirrored (they stay
  seated there), only the player's own goes to the server. The RF hazard near the radars (p. 207) isn't modelled.
- TV channel: locks and follows automatically; the real one is tracked by hand. (6.6)
- Engagement: zones (with the crossing parameter) and the methods T/T, Горка, Ф and Н are selected. A noise jammer
  drowns the tracking radar's echo and the radar follows it in angle; the range is then by hand (search scope or PU-12)
  and Н has an outcome factor (9.1). Not modelled: the counter-jamming used "only while firing", the range-only radar
  (ПОЛУАВТОМАТ) being jammed too, the anti-radiation missile threat. (6.7, 9.1)
- Targets: terrain following, low strike runs, pop-up helicopters and evasion on launch or lock are scenario behaviours
  (9.3); the aircraft is still a constant-speed, turn-rate-limited point (no energy, no g limit).
- Power: GTA, take-off generator and an external supply; a prepared position's grid, cut and diesel set are a scenario
  block (7.6). No emplacements, nets or dummy positions yet.
- PU-12 board and voice designations are built (7.1, 7.2); the board's picture is the regiment radars' (P-19, P-40: range and horizon only; no clutter, terrain or blind cone) when the scenario lists them.

The PU-12M's R-111 and R-407 (`simcore/radio_sets.hpp`): their reach is the documents' range by antenna and the fourth
root of the power, not the EM layer's link budget over the terrain; interference on the telecode is a scenario outage
(`"kind": "interference"`), not a jammer; the R-111's other modes (telegraphy, relay, remote control) are positions only.

Radios and intercom: `simcore/crew_comms.hpp` reads the console's R-123M and R-124 controls and tells the command network which set hears and whether the commander is on the radio, and the EM link what the squelch and the antenna cost; orders to crew go through the R-124 (`ScenarioRunner::GiveOrder`).

Voice paths (`simcore/voice_comms.hpp`, `simcore/radio_nets.hpp`, `docs/crew/12_COMMUNICATIONS.md`): who hears a callout is
decided by headsets, boxes and selectors, and a net's reach by its set being on, warm, on the net's numbered frequency
and not jammed out; the reach of a voice net is not the EM layer's link budget over the terrain (the R-123M nets are
assumed to reach within the battery). Jamming is a scenario's or a lesson's (barrage or spot, weak or strong, a
direction-finding fix bringing a follow-on jammer), not an EM jammer's J/S. The talk tables' groups are SAM3D's
placeholders (the real tables are not public); a regiment's group heard on the R-111 is decoded by an NPC, never by the
player's hand. Which PVS/RTK-1 position reaches which set is SAM3D's reading of museum photographs (medium confidence).

Command sources: `simcore/c2_sources.hpp` (`C2Network`) gives the engagement model the crew's air picture from the active of several sources; the PU-12 board is one of them (`docs/research/EM_C2_SOURCES_2026-10.md`).
