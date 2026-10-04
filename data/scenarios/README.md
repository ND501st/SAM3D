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
