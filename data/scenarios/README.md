# Scenario files

One JSON file per scenario, loaded by `simcore::LoadScenarioFile` (`simcore/include/simcore/scenario.hpp`) and
played by `simcore::ScenarioRunner` (`scenario_runner.hpp`). Loading also loads the console the scenario names and
checks every reference in it (inputs, procedures, crew, targets, conditions), reporting the path to any problem.
An order must go to the crew member in the seat that owns the procedure (`data/procedures/README.md`).

`missions.json` is the game's missions menu (`simcore/missions.hpp`): it orders and groups the scenarios and gives
each a short title; the name, description, seats and the seat that commands come from the scenario. A new scenario
goes into it (or is a half of a listed battery link): `test_missions.cpp` checks every file here is listed and starts.

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
  "terrain": { "type": "flat", "height_m": 110,      // only flat so far
               "ridges": [ { "from": { "east_m": 1000, "north_m": 5500 }, "to": { "east_m": 9000, "north_m": 5500 },
                             "height_m": 60, "width_m": 900 } ] },   // optional: straight ridges for cover (9.3)
  "own_unit": {
    "console": "procedures/osa_ak.json",              // relative to data/
    "crew_data": "crew/osa_ak_crew.json",             // optional: the crew's orders, callouts, tasks (default: crew/<system>_crew.json if it exists)
    "tuning": "gameplay/osa_ak.json",                 // optional: turns on the gameplay engagement model
    "em": { "library": "em/osa_ak.json", "search_radar": "osa_ak_soc", "track_radar": "osa_ak_ssc" },  // optional, see below
    "readiness": "deployed",                          // optional: "cold" (default) or a console readiness state
    "fuel_fraction": 1.0,                             // optional: the tank's starting level, 0 to 1 (roadmap 7.4)
    "position": { "east_m": 0, "north_m": 0, "up_m": 3 },
    "heading_deg": 0,                                 // optional: the hull's true heading, clockwise from north
    "inputs": { "vehicle_stopped": false },           // starting values of console inputs
    "empty_containers": ["1-I", "1-II"]               // optional: launcher containers that start without a missile
  },
  "environment": { "visibility_km": 40, "daylight": true },  // optional: the air and the light for the TV sight (em/optical.hpp)
  "doctrine": { "tv_check": true,                     // optional (roadmap E6): warned of decoys, look on the TV before a launch
                "search_bursts": { "on_s": 4, "off_s": 16 },  // emission discipline: the search radar in bursts while nothing is near
                "salvo": 2 },                         // missiles per target at once (roadmap 10.1): 2 sends the second before the first arrives
  "assistance": "guided",                             // optional: the training overlay's level: demonstrate, guided (default), hint, unassisted, exam
  "crew_commander": "npc",                            // optional: "npc" runs the crew by doctrine (docs/crew/), "script" (default) leaves the orders to the triggers
  "crew": [ { "name": "Commander", "role": "commander", "reaction_time_s": 1.0, "reaction_jitter_s": 0.3 },
            { "name": "Deputy commander", "role": "deputy_commander", "skill": "class2" } ],   // role = a console station, one each; skill: a tier of data/crew/skill_levels.json
  "targets": [
    { "id": "t1", "name": "...", "type": "Air+FixedWing", "class": "drone", "appear_at_s": 0,
      "evade_on_launch": false, "evade_turn_rate_deg_s": 12,          // optional: break away when fired at
      "evade": { "on": "launch", "kind": "break", "turn_rate_deg_s": 12 },   // optional instead (9.3), see below
      "behaviour": { "kind": "terrain_following", "agl_m": 60 },      // optional (9.3), see below
      "iff": "none",                                                  // optional: "friend" replies to IFF
      "size_m": 4.5,          // optional: its critical dimension for the TV sight (em/optical.hpp; default a fighter's)
      "rwr": "nato_alr46_class_rwr",  // optional, with own_unit.em: its warning receiver; it evades on what it hears (E5)
      "decoy": true,          // optional: a drone whose echo looks like an aircraft's; the TV can unmask it (E6)
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

**How targets fly (roadmap 9.3, `simcore/target_behaviour.hpp`).** Without a `behaviour` a target flies straight at
constant speed from waypoint to waypoint. With one, the target's pilot holds the height:
- `{ "kind": "terrain_following", "agl_m": 60, "max_climb_mps": 30, "lookahead_s": 6 }` holds `agl_m` above the
  ground, climbing early for the highest ground `lookahead_s` ahead. Waypoints may then give `"agl_m"` instead of
  `"up_m"` (`{ "east_m", "north_m", "agl_m" }`): the target holds the height of the waypoint it is flying to. A low
  strike run is such a route: low ingress, a pull-up point at a few hundred metres, the attack, a low egress.
- `{ "kind": "pop_up", "hide_agl_m": 5, "popup_agl_m": 25, "first_hold_s": 30, "expose_s": 30, "hidden_s": 60,
  "cycles": 1 }`: a helicopter flies low to its first waypoint (the hide), hovers there, rises to `popup_agl_m` for
  `expose_s`, sinks back for `hidden_s`, `cycles` times, then flies on along the rest of the route low (it needs at
  least one waypoint after the hide).
- `"evade": { "on": "launch" | "lock", "kind": "break" | "dive" | "jink", "turn_rate_deg_s": 12, "dive_agl_m": 30,
  "jink_period_s": 4, "evade_s": 20 }`: when a missile is launched at it, or when the tracking radar locks it (a radar
  warning receiver hears a radar lock; a TV track gives no warning), it breaks onto a beam heading, breaks and dives to
  `dive_agl_m`, or jinks (hard turns 60 degrees either side of its course every `jink_period_s`) for `evade_s` and
  then resumes its route. A hiding helicopter drops behind its cover instead. Use either `evade` or the older
  `evade_on_launch`.
Terrain masking needs no setting: a target low behind a ridge (`terrain.ridges`, or Cesium's ground on the map) is out
of every sensor's line of sight. The scenarios `osa_low_level_em.json`, `osa_jamming_em.json` and `osa_emcon_tv.json`
show them.

**Jamming the tracking radar (roadmap 9.1).** A self-screening noise jammer in the tracking radar's band
(`own_unit.em.jammers`, e.g. `generic_selfscreen_noise_ku`) drowns its echo: the radar follows the jammer in angle with
no range. The range then comes by hand: the operator keeps the range wheel on the target's blip on the search scope, or
on the range the PU-12 last gave; ЦЕЛЬ В ЗОНЕ lights from it. The deputy commander's `fire_at_jammer` drill fires with
ПОМЕХА up (method Н).

**The battery's fight (roadmap 8.9).** With a `battery`, the other launchers fight too (`simcore/battery_fire.hpp`).
`"battery_commander": "npc"` (default) gives them the PU-12's targets on the battery net; `"player"` leaves that to
the player (`ScenarioRunner::AssignBatteryTarget`); `"none"` lets them fire freely at what is in their reach.
`osa_battery_fight.json` is a raid on the battery; `osa_mission_march.json` is one mission end to end (the battery on
the march, a raid, the fight, stowing and marching on). It starts with one missile (`empty_containers`), so the TZM
comes within the mission. A battery vehicle may have a sector of fire, `"sector_deg": [from, to]` (true azimuths,
clockwise): in free fire it takes only targets there, and the NPC battery commander gives it targets there first.

**The Osa and the PU-12M in one battery (a battery link, `simcore/battery_link.hpp`, docs/PU12_DEVELOPMENT.md P8).** A
link file names two scenarios that share a battery and a sky, `{ "link_id", "name", "description", "osa", "post",
"callsign", "source", "confidence", "notes" }`:

- **The Osa's scenario** has `"battery_commander": "post"` and the link's callsign as its own in the battery. The battery
  commander sits in the linked PU-12M: his designations reach this vehicle's commander as the PU-12's, on the command
  net, with their own timing. The other launchers are the post's, and their reports are heard on the battery net.
  The commander takes the target the post gave him first, and leaves targets given to another launcher (and echoes
  beside them) alone.
- **The post's scenario** (its own unit the PU-12M) has a launcher of that callsign in its battery and no targets: it
  takes the Osa's. The Osa's commander reports to it (к бою готов, пуск, цель поражена, ракет нет), and the post writes
  it on its IT boards.

`battery_link_practice.json` (with `link_osa_21.json` and `link_pu12m.json`) is the raid of `pu12m_battery.json` on a
deployed battery. Play it headless with `battery_link_demo`.

**The crew (phase 8, `docs/crew/`).** With crew data the crew speaks (orders, acknowledgements, reports, reminders:
the radio log and the debrief), and each crew member may have a `skill` tier (recruit, class3, class2, class1, master)
that sets its pace, slips, omissions and aim. With `"crew_commander": "npc"` no triggers are needed for the crew's
work: the NPC commander brings the vehicle to readiness and engages by doctrine (`osa_crew_practice.json`). A player
may take any seat (`ScenarioRunner::TakeSeat`): he gets that seat's orders and is reminded when the crew waits for him.
`"assistance"` sets the training overlay's level for him (`docs/crew/08_TRAINING_AND_ASSISTANCE.md`).

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

`own_unit.em` also takes the electromagnetic environment's scripted effects (`docs/EM_ARCHITECTURE.md` §25):
`"extra_libraries": ["em/nato_1985.json"]` merges more library files (jammers, threat libraries);
`"decoys": [ { "position": {...}, "mode": "with_radar" | "when_radar_off" | "always" } ]` puts decoy emitters copying the search radar at dummy positions (roadmap E7);
`"command_link_in_load": true` keeps the command transmitters in their equivalent load until a missile flies (a
peacetime check, `mon_2112_80` p67; in combat they radiate from ВНИМАНИЕ, so a target's RWR warns before the launch);
`"rain_mmh": 25` puts rain over the scene (ITU-R P.838: it costs Ku-band and C-band paths, not VHF; never part of a radar's calibration);
`"jammers": [ { "jammer": "<library id>", "target": "<target id>" | "position": {...}, "on_at_s": 270, "off_at_s": 400 } ]`
puts a jammer on a target (self-screening) or at a stand-off point, aimed at the unit; `"chaff": [ { "target": "<id>",
"at_s": 235, "rcs_m2": 300, "lifetime_s": 90 } ]` drops a cloud at a target's position; `"wind_mps": { "east": 0,
"north": -30 }` drifts it (the Osa's internal-coherence MTI cancels clouds slower than 15 m/s). Chaff and false echoes
show on the scope as ghost contacts `T<n>`. `osa_practice_em_ew.json` is the EM practice scenario with a noise pod and a
chaff drop. The auto-capture rule (`em_capture_looks`, `em_capture_hits`) is in the tuning file's `track` group.

`"c2"` adds early-warning and command sources beside the PU-12 (`command_post`), with outages and the commander's fallback policy; triggers `c2_source`, `c2_fail`, `c2_restore` change them (`data/command/README.md`). `osa_practice_c2.json` loses the PU-12 at 60 s.
`"c2.regiment": { "library": "command/regiment_radars.json", "radars": [ { "id": "p19_a", "type": "p19", "position": { "east_m": 0, "north_m": 60000, "up_m": 0 } } ] }`
makes the PU-12's picture the regiment radars' (roadmap 7.1): a target is plotted only when some radar covers it. Without the block the board sees every
target. `"c2.presets": ["pu12", "ewr_north", "", "division"]` says which source each of the R-123M's four presets is tuned to
(default: the main source, the fall-back order, then the rest); the cabin's two radio selectors and the headset switch use it (roadmap 7.8). A source can carry `"frequency_mhz"` (20.000 to 51.500 in
25 kHz steps, the R-123M's grid): the EM layer's net for that source is on that frequency (a source without one is at the middle of
the band). Each crew order goes through the R-124 (`ScenarioRunner::GiveOrder`): it arrives at once, or after the shouting time when
the commander's box A-1 is off or the member's own box is on the radio.

`"readiness"` starts the unit warm instead of cold (roadmap 6.2): one of the console's `readiness` states
(`data/procedures/README.md`). The Osa has `powered` (on the march: turbine running, equipment and stabilisation warm,
search possible on the move) and `deployed` (at a halt on the take-off generator, launcher unlocked, drives on,
vehicle oriented, radars silent). A cold start takes the documented 3.8 min switch-on cycle and the 3.0 min function
check (`function_check` procedure); in Unreal, `sam3d.TimeScale` (0.25 to 32) speeds the sim up so that stays playable.

A trigger can set the console input `tzm_alongside` (`{ "set_input": "tzm_alongside", "value": true }`) to bring the
transport-loader vehicle up: with the vehicle stopped, the launcher reloads one missile a minute (roadmap 7.3).
`external_power` connects an external 220 V 400 Hz supply (a cable, not a console control): at a halt it powers the
equipment without the turbine or the take-off generator and burns no fuel; it drops out when the vehicle moves.

`"battery"` (optional, roadmap 7.5) puts the rest of the battery around the player's vehicle: `{ "tactics": "command/battery_tactics.json", "drive_own": false, "night": false, "share_picture": true, "own": { "id": "21", "callsign": "21" }, "vehicles": [ { "id": "22", "kind": "launcher" | "reload" | "fuel" | "command_post" | "maintenance", "position": {...} } ], "orders": [ { "at_s": 0, "order": "march", "route": [ {...}, {...} ] } ] }`. Orders: `march` (with `route`), `short_halt`, `combat_formation` (with `covered` and `axis_deg`; `"bounding": true` moves one pair at a time while the other stays deployed), `halt`, `resume`, `ambush` (with `points`, the prepared ambush points; `"roving": true` moves the pairs on between them), `river_crossing` (with `crossing`, the crossing point on the near bank, `axis_deg` and `width_m`: the battery waits 2 km short, then crosses in pairs once told), `far_bank_held` (the covered force holds the far bank: the pairs go over), `emcon` (with `"radiate": false`, the battery's radars silent: the crews make ready without radiating and fight by the TV on the PU-12's designations; `"radiate": true` lifts it), `reload` / `refuel` (with `vehicle`: the TZM or the fuel truck drives alongside and back; the runner also orders them itself when the player's launcher is empty and nothing is flying, or the fuel system asks for fuel, and sets `tzm_alongside` / `fuel_truck_alongside`); a trigger can give them (`{ "battery_order": "short_halt" }`) or set `{ "air_threat": true }`. `"deployed": true` starts every vehicle set up where it stands. When the player's vehicle is the PU-12M (`own_unit.console` `procedures/pu12m.json`, with a `post` block), `"launchers": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json" }` gives the launchers their tuning and their tracking radar's reach; the battery commander in the post gives them targets by voice on the battery net (`ScenarioRunner::DesignateBatteryTarget`, or the NPC), and their reports fill the IT-1 and IT-2 boards (`StatusBoards`). `drive_own` lets the battery drive the player's vehicle too; `share_picture` makes the other launchers' radars neighbour sources of the command network.

`own_unit.prepared_position` (optional, roadmap 7.6): `{ "emplacement": true, "nets": true, "wire": true, "dummies": [ { "position": {...}, "decoy": "when_radar_off" } ] }`; values from `command/prepared_positions.json`.

`osa_battery_practice.json` is the EM practice shoot with the whole battery in a prepared position (the practice map's scenario).

`own_unit.position_power` (optional, roadmap 7.6) is a prepared position's supply, and sets `external_power` itself every
step: `{ "grid": true, "grid_cut_s": 900, "diesel_start_s": 30, "diesel_hours": 24, "source": "...", "confidence": "low" }`.
The civil grid carries the vehicle until it is cut (omit `grid_cut_s` for never); the diesel set takes over `diesel_start_s`
after the cut (the equipment is dark meanwhile) and runs `diesel_hours` (0 or absent: no diesel set). The debrief logs
each change. The 24 h is the only figure a source gives (`video_osa`, unverified); the start delay is an estimate.

`"command_post": "command/pu12.json"` (top level, optional) adds the battery command post's plotting board
(`simcore/pu12.hpp`, `data/command/README.md`): it plots every target once a minute with the manual's notation, off by the
post's navigation error, and the vehicle crews aim by its last marks instead of the truth when their radars are
silent. A target gives its board type with `"board_type"` (0-9; default 1 for a friend, else 9) and its formation with
`"formation"`. The debrief logs each target's first mark ("PU-12 ... on the board: 6201 12/21 at 45 km, 00-71").

## Ranges and history (roadmap 10.1, 10.2)

The missions menu's "Ranges and history" group. Each file's `notes` name the sources of its event and mark everything
else, the flight profiles first, as SAM3D's placeholders; `test_ranges_history.cpp` checks that each plays as its sources
describe.

| File | What | Checked |
|---|---|---|
| `osa_range_ashuluk.json` | Ashuluk: from cold to readiness, then two La-17M drones, one at 4 km | the readiness time against the norm; both drones engaged, the high one in the search radar's tall beam III |
| `osa_range_ustka.json` | Ustka: two P-15 targets in from the sea; `doctrine.salvo` 2 | a two-missile salvo at each, the second 3-8 s after the first (`korovin_tiv_2010`: 3-5 s) |
| `osa_range_kapustin_yar.json` | Kapustin Yar: a slow drone entering the zone at its far edge | the missile goes at 8-10.3 km (preparation starts in the zone, `flak11`; the 2013 kill at 10.3 km was an Osa-AKM's) |
| `osa_bekaa_1982.json` | The Bekaa, 24 July 1982 (`wapo_1982`): an F-4E pair over the mountains | the ridge hides the pair (seen over 5 km later than over flat ground); the crew engages; the pair's warning receivers hear the command link |
| `osa_angola_night.json` | Angola, the Pavlov memoir (`korovin_tiv_2010`): dug in under nets, a night raid, a Puma in a canyon | by the radars at night (no TV); the Puma first seen out of the canyon's mouth |
| `osa_cuito_1988.json` | Cuito Cuanavale, 20 February 1988 (`samsim_osa` p31): Mirage F1AZs toss their bombs | missiles away between the pull-up and the turn away |

## Generated raids (the enemy air command)

`air_war_*.json` are raids generated by the enemy air command (`simcore/air_war/raid_scenario.hpp`, `docs/air_war/05_EXECUTION.md`)
and kept as samples for the missions menu's "Air war" group. They use only the keys above: targets with waypoint heights,
`behaviour`, `evade`, `rwr` and `decoy`; `own_unit.em.jammers`; the `battery`, `prepared_position`, `c2` (with
`regiment` radars and outages) and `command_post` blocks; an `air_threat` trigger; `doctrine`; `environment`; NPC
commanders. Regenerate with `simcore/examples/air_war_demo` (it writes every raid it plays to its output folder).
