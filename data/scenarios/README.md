# Scenario files

One JSON file per scenario, loaded by `simcore::LoadScenarioFile` (`simcore/include/simcore/scenario.hpp`) and
played by `simcore::ScenarioRunner` (`scenario_runner.hpp`). Loading also loads the console the scenario names and
checks every reference in it (inputs, procedures, crew, targets, conditions), reporting the path to any problem.

Fictional and training scenarios use `"source": "sam3d"` with `"confidence": "estimate"`. Historical scenarios cite
their sources like any other data file (`data/systems/README.md`). Placeholder numbers (target speed, RCS, the
ranges at which zone lamps light) are labelled as such in `notes`.

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
    "position": { "east_m": 0, "north_m": 0, "up_m": 3 },
    "inputs": { "vehicle_stopped": false }            // starting values of console inputs
  },
  "crew": [ { "name": "Commander", "role": "commander", "reaction_time_s": 1.0, "reaction_jitter_s": 0.3 } ],
  "targets": [
    { "id": "t1", "name": "...", "type": "Air+FixedWing", "appear_at_s": 0,
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
              { "queue": "prepare_firing", "crew": "Commander" },     // after current orders
              { "end": "reason text" } ] }
  ],
  "end": { "time_limit_s": 600, "when_targets_gone": true }
}
```

Each trigger fires once, when every part of `when` holds. Positions: `east_m`/`north_m`/`up_m` are the local frame
at `origin` (ENU metres); `lat_deg`/`lon_deg`/`alt_m` are converted to it.

A launch is recorded when the console's launch button (`"launch"` in the console file) is pressed while its armed
subsystem is ready. Missile flight and the outcome are not simulated yet; that is roadmap phase 3.
