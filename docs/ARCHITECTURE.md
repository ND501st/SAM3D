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

- `ControlPanel` (`panel.hpp`): every physical control (toggle, rotary, key switch, cover) and its position. Only
  physical constraints block a move: a closed cover over a guarded control, or a stepwise control (key switch,
  detented rotary) jumping more than one position. These control types come from SAM Simulator's Osa panels
  (`docs/re/osa.md`).
- `Equipment` (`equipment.hpp`): the circuits and mechanisms behind the panel. Each subsystem is off, starting,
  ready, stopping or faulted, with start and stop times (warm-up, deploy, stow). Switching something on before its
  prerequisites are ready, or losing a prerequisite while it runs, faults it. Lamps and animations read these states.
- `Procedure`: an ordered list of steps `{control, position, min_wait_s, description, wait_for}`. `wait_for` is
  what the operator should see first (e.g. a READY lamp).
- `ProcedureTracker`: judges each action. It accepts correct steps, refuses ones before `min_wait_s`, and logs
  deviations with times: out-of-order actions, and "early" steps done before their `wait_for` held (these still
  count as done; `Equipment` decides the consequence). Physically blocked moves aren't judged.
- `CrewMember`: an NPC that works through ordered and queued procedures on the **same** panel. It waits for
  `wait_for`, reacts with a delay once a lamp comes on, and reports started / completed / waiting / equipment fault /
  aborted for voice lines and the order UI.

The player's 3D cabin clicks call the same functions, so the player and the crew share one source of truth.
Per step, update `Equipment` first, then crew, so crew see this step's lamps.

## Known limitations (deliberate, for now)

- Aircraft: constant speed, turn-rate limited, and they fly straight lines in ENU, so they drift in true altitude over long distances.
- Missile: PN only, perfect target knowledge, no autopilot lag or noise (so misses are unrealistically small),
  constant Cd, deterministic kill radius.
- Radar: single-look SNR threshold (no Swerling fluctuation or Pd/Pfa), no clutter or multipath, no scanning or tracking loops.
- Terrain: flat in the demo. `GridTerrain` exists; Cesium-backed terrain isn't implemented yet.
- ISA atmosphere treats geopotential altitude as geometric (< 0.5 % error below 32 km).
