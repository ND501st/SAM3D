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

- `ControlPanel`: every physical control (toggle, rotary...) with its current position.
- `Procedure`: an ordered list of `{control, position, min_wait_s}` steps (e.g. power-up with warm-up delays).
- `ProcedureTracker`: judges each action. It accepts correct steps, refuses early ones (warm-up) and counts deviations.
- `CrewMember`: an NPC that executes an ordered `Procedure` on the **same** panel, with a reaction time.

The player's 3D cabin clicks call the same functions, so the player and the crew share one source of truth.

## Known limitations (deliberate, for now)

- Aircraft: constant speed, turn-rate limited, and they fly straight lines in ENU, so they drift in true altitude over long distances.
- Missile: PN only, perfect target knowledge, no autopilot lag or noise (so misses are unrealistically small),
  constant Cd, deterministic kill radius.
- Radar: single-look SNR threshold (no Swerling fluctuation or Pd/Pfa), no clutter or multipath, no scanning or tracking loops.
- Terrain: flat in the demo. `GridTerrain` exists; Cesium-backed terrain isn't implemented yet.
- ISA atmosphere treats geopotential altitude as geometric (< 0.5 % error below 32 km).
