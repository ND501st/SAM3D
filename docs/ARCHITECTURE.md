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

`LocalFrame` converts geodetic ↔ ENU exactly (via ECEF). Far from the origin, ENU `z` differs from altitude by
about d²/2R (≈ 785 m at 100 km), so use `LocalFrame::AltitudeOf` whenever altitude matters.

## Time

`USimWorldSubsystem` accumulates frame time and calls `World::Step(0.01)` in a loop (fixed 100 Hz, capped per frame).
Everything in simcore is deterministic for a given sequence of inputs, which makes replays and tests reproducible.

## Controls, procedures and crew

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
- Console data (`console_data.hpp`, `json.hpp`): each system's controls, subsystems and procedures are loaded from
  `data/procedures/<system>.json` by a strict loader that requires a source and confidence on every item.
- `Procedure`: an ordered list of steps `{control, position, min_wait_s, description, wait_for}`. `wait_for` is
  what the operator should see first (e.g. a READY lamp).
- `ProcedureTracker`: judges each action. It accepts correct steps, refuses ones before `min_wait_s`, and logs
  deviations with times: out-of-order actions, and "early" steps done before their `wait_for` held (these still
  count as done; `Equipment` decides the consequence). Physically blocked moves aren't judged.
- `CrewMember`: an NPC that works through ordered and queued procedures on the **same** panel. It waits for
  `wait_for`, reacts with a delay once a lamp comes on, and reports started / completed / waiting / equipment fault /
  aborted for voice lines and the order UI.

The player's 3D cabin clicks call the same functions, so the player and the crew share one source of truth.
- `SessionLog` (`debrief.hpp`): crew reports, equipment faults and player deviations on one clock, formatted as a
  debrief timeline and summary. No points: SAM Simulator's rule is "no points, as in reality".
  `examples/procedure_demo.cpp` runs an Osa session from the march to the debrief.

Per step: `Equipment::Update`, then `ControlPanel::ReleaseMomentaries`, then crew. Crew see this step's lamps, and
Equipment sees every button press exactly once.

## Known limitations (deliberate, for now)

- Aircraft: constant speed, turn-rate limited, and they fly straight lines in ENU, so they drift in true altitude over long distances.
- Missile: PN only, perfect target knowledge, no autopilot lag or noise (so misses are unrealistically small),
  constant Cd, deterministic kill radius.
- Radar: single-look SNR threshold (no Swerling fluctuation or Pd/Pfa), no clutter or multipath, no scanning or tracking loops.
- Terrain: flat in the demo. `GridTerrain` exists; Cesium-backed terrain isn't implemented yet.
- ISA atmosphere treats geopotential altitude as geometric (< 0.5 % error below 32 km).
