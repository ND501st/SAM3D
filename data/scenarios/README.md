# Scenario files

One JSON file per scenario, loaded by `simcore::LoadScenarioFile` (`simcore/include/simcore/scenario.hpp`) and
played by `simcore::ScenarioRunner` (`scenario_runner.hpp`). Loading also loads the console the scenario names and
checks every reference in it (inputs, procedures, crew, targets, conditions), reporting the path to any problem.
An order must go to the crew member in the seat that owns the procedure (`data/procedures/README.md`).

Fictional and training scenarios use `"source": "sam3d"` with `"confidence": "estimate"`. Historical scenarios cite
their sources like any other data file (`data/systems/README.md`). Placeholder numbers (target speed, RCS, the
ranges at which zone lamps light) are labelled as such in `notes`.

Planned Osa scenarios (roadmap phase 10, sources in `docs/SOURCES.md` → "Validation scenarios"): training-range
shoots in the style of Ashuluk / Kapustin Yar / Ustka / NAMFI, Angola 1987–88 and Lebanon, 24 July 1982. Once the
battery exists (phase 7), scenarios will also say who designates targets (the PU-12 or the vehicle itself) and
the readiness state they start in (march, deployed, powered).

## Format

```jsonc
{
  "scenario_id": "osa_practice",
  "name": "...", "description": "...", "source": "sam3d", "confidence": "estimate", "notes": "...",
  "seed": 1,                                          // crew reaction times are drawn from this
  "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 },
  "terrain": { "type": "flat", "height_m": 110 },     // only flat so far
  "own_unit": {
    "console": "procedures/osa_ak.json",              // relative to data/
    "tuning": "gameplay/osa_ak.json",                 // optional: turns on the gameplay engagement model
    "em": { "library": "em/osa_ak.json", "search_radar": "osa_ak_soc", "track_radar": "osa_ak_ssc" },  // optional, see below
    "readiness": "deployed",                          // optional: "cold" (default) or a console readiness state
    "fuel_fraction": 1.0,                             // optional: the tank's starting level, 0 to 1 (roadmap 7.4)
    "position": { "east_m": 0, "north_m": 0, "up_m": 3 },
    "heading_deg": 0,                                 // optional: the hull's true heading, clockwise from north
    "inputs": { "vehicle_stopped": false }            // starting values of console inputs
  },
  "crew": [ { "name": "Commander", "role": "commander", "reaction_time_s": 1.0, "reaction_jitter_s": 0.3 },
            { "name": "Deputy commander", "role": "deputy_commander" } ],   // role = a console station, one each
  "targets": [
    { "id": "t1", "name": "...", "type": "Air+FixedWing", "class": "drone", "appear_at_s": 0,
      "evade_on_launch": false, "evade_turn_rate_deg_s": 12,          // optional: break away when fired at
      "iff": "none",                                                  // optional: "friend" replies to IFF
      "start": { "east_m": 2000, "north_m": 45000, "up_m": 1000 },   // or { "lat_deg", "lon_deg", "alt_m" }
      "waypoints": [ { "east_m": 2000, "north_m": -30000, "up_m": 1000 } ],
      "speed_mps": 150, "max_turn_rate_deg_s": 6, "rcs_m2": 0.5 }
  ],
  "triggers": [
    { "id": "halt",
      "when": { "after_s": 0, "target": "t1", "target_within_m": 40000,
                "conditions": [ { "subsystem": "power", "is": "ready" } ], "launches_at_least": 0 },
      "do": [ { "set_input": "vehicle_stopped", "value": true },
              { "order": "deploy_at_halt", "crew": "Commander" },     // drop everything, do this
              { "queue": "prepare_firing", "crew": "Deputy commander" },  // after current orders
              { "end": "reason text" } ] }
  ],
  "end": { "time_limit_s": 600, "when_targets_gone": true }
}
```

Each trigger fires once, when every part of `when` holds. Positions: `east_m`/`north_m`/`up_m` are the local frame
at `origin` (ENU metres); `lat_deg`/`lon_deg`/`alt_m` are converted to it.

A launch is recorded when the console's launch button (`"launch"` in the console file) is pressed while its armed
subsystem is ready.

With `"tuning"`, the gameplay engagement model (`simcore/engagement.hpp`, numbers in `data/gameplay/`) runs the
search and tracking picture and sets the console's capture, too-low and zone inputs itself, so scenarios don't need
triggers for those. The crew members whose roles match the tuning's `aim` roles turn the wheels onto contacts:
`azimuth_role`, `elevation_role` and `range_role` each turn their own wheel; a seat with no crew member (the player's)
is left alone.
Each launch is resolved as a hit or a miss from the target's `class` and the factors listed in the tuning file; the
debrief prints them. Heights given as `up_m` are in the flat local frame: far from the origin the true height is
higher (about 45 m at 24 km), so give low targets in `lat_deg`/`lon_deg`/`alt_m` or keep them close.

With `"em"` (needs `"tuning"`), the unit's search and tracking radars come from the electromagnetic layer
(`simcore/em_link.hpp`, library in `data/em/`, `docs/EM_ARCHITECTURE.md` §23) instead of the tuning's rules: the search
scope shows the search radar's own tracks (what the calibrated radar equation, terrain, MTI and clutter let it
detect, once per revolution), and the tracking radar is pointed by the azimuth, elevation and range wheels and locks
on a detected echo in its beam and range gate. The tuning still supplies the zone lamps, the crew's aiming speeds
and the outcome of each launch. `osa_practice_em.json` is `osa_practice.json` with the EM layer on. Target `rcs_m2`
becomes the radar cross-section from every aspect, so give real values: a 0.5 m2 drone at 1 km is first shown on the
scope at 25 to 34 km (the search radar's 50 % range is 28 km), and the tracking radar captures it at 13 to 15 km (its 50 % range is 14.8 km).

`"readiness"` starts the unit warm instead of cold (roadmap 6.2): one of the console's `readiness` states
(`data/procedures/README.md`). The Osa has `powered` (on the march: turbine running, equipment and stabilisation warm,
search possible on the move) and `deployed` (at a halt on the take-off generator, launcher unlocked, drives on,
vehicle oriented, radars silent). A cold start takes the documented 3.8 min switch-on cycle and the 3.0 min function
check (`function_check` procedure); in Unreal, `sam3d.TimeScale` (0.25 to 32) speeds the sim up so that stays playable.

A trigger can set the console input `tzm_alongside` (`{ "set_input": "tzm_alongside", "value": true }`) to bring the
transport-loader vehicle up: with the vehicle stopped, the launcher reloads one missile a minute (roadmap 7.3).

`"command_post": "command/pu12.json"` (top level, optional) adds the battery command post's plotting board
(`simcore/pu12.hpp`, `data/command/README.md`): it plots every target once a minute with the manual's notation, off by the
post's navigation error, and the vehicle crews aim by its last marks instead of the truth when their radars are
silent. A target gives its board type with `"board_type"` (0-9; default 1 for a friend, else 9) and its formation with
`"formation"`. The debrief logs each target's first mark ("PU-12 ... on the board: 6201 12/71 at 45 km, 00-71").

