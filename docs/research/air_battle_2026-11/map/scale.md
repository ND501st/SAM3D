# Scale, determinism and LOD constraints for a live air battle (map of the existing code)

Mapped in `C:\dev\SAM3D-wt\air-battle` (branch `docs/air-battle`, HEAD `1d09489`, the same commit as
`feature/finish-oct21`). Read-only: nothing in the repo was changed. Paths below are relative to that worktree
unless they say otherwise. Line numbers are those of this checkout.

**Branch caveat.** `tools/test.ps1`, `docs/TESTING.md`, the test areas and the split runner (`simcore/runner/<area>.hpp`,
`neighbour_defences.hpp`, `loaders_impl.hpp`) **do not exist in this worktree**. They are on `feature/devloop`
(`C:\dev\SAM3D-wt\devloop`, commit `b95402d`), which is **not merged** into this HEAD (4 commits ahead, 15 behind).
Here `ScenarioRunner` is still one 12,838-line header and the tests are one program, `simcore_tests`. Section 7 gives both.

**Cold War scope (the owner's standing instruction for this work).** The code already pins the war to a year:
`TheatreDef::year = 1985` (`simcore/include/simcore/air_war/data.hpp:176`). The nation's theatre is dated 1985-10-15
(`data/air_war/theatre_pannonia_nation.json`). Types fly only once in service (`AircraftType::in_service_year`, `InService`,
`data.hpp:357,370`), and ARMs are picked by year (`ArmEm(year)`, `data.hpp:372`). Any new air-to-air weapon, radar,
procedure or comms means needs the same year gate. **Exception:** the dynamic campaign testbed deals every type "whatever
its year" (`docs/DYNAMIC_CAMPAIGN_TEST.md:43`), so behaviour tried there will meet post-1985 types (Tomahawk 1986, CALCM
1988). Section 8 has more.

---

## 1. The fixed step and its cost

### 1.1 Who steps what, and how often

| Context | Step | Where |
|---|---|---|
| Unreal, a scenario (every mission, the testbed, raids) | **0.05 s (20 Hz)** fixed, `ScenarioStepSeconds` | `unreal/SAM3D/Source/SAM3D/Public/SimWorldSubsystem.h:1918`; loop `Private/SimWorldSubsystem.cpp:282-299` |
| Unreal, demo mode only (`SimWorld`, no scenario) | 0.01 s (100 Hz), `FixedStepSeconds` | `SimWorldSubsystem.h:1876`, `.cpp:312-320` |
| Headless tools and most tests | **0.1 s** (`r.Step(0.1)`) | `examples/ground_check.cpp:42`, `helper_coverage.cpp:112`, `crew_balance.cpp:106`, `mission_demo.cpp:61`, `air_war_demo.cpp:126`, `scenario_demo.cpp:27`; `tests/test_dynamic_campaign.cpp:217` |
| EM layer inside a session | 0.1 s, its own accumulator (`EmSensorLink::kStepS`, `Advance`) | `simcore/include/simcore/em_link.hpp:130`, `:523-539` |
| EM sub-cadences | ESM 5 Hz, knowledge 2 Hz, links 1 Hz, ageing 1 Hz (`EmRates`) | `simcore/include/simcore/em/world.hpp:35-42` |
| Air campaign (abstract) | **1 hour** steps (`AirCampaign::Advance` -> `StepTo`) | `simcore/include/simcore/air_war/campaign.hpp:182-190`, `:651` |
| Live packages in a mission session | campaign advanced every 30 s of session time (`LivePackages::every_s_`) | `air_war/package_injector.hpp:205`, `:293` |
| Battery fire model's sight lines | cached 0.5 s per launcher and target (`kSightRefreshS`) | `simcore/include/simcore/battery_fire.hpp:352-361` |

`docs/ARCHITECTURE.md:85-88` still says "fixed 100 Hz". That is true only of demo mode. A scenario steps at 20 Hz in the
game.

Unreal's loop (`SimWorldSubsystem.cpp:280-299`):

```cpp
Accumulator += FMath::Min(static_cast<double>(DeltaTime), 0.25) * Scale;     // frame time clamped to 0.25 s
const int32 ScaleSteps = FMath::CeilToInt32(FMath::Max(1.0, Scale));
while (Accumulator >= ScenarioStepSeconds && Steps < 20 * ScaleSteps && ...) { Runner->Step(ScenarioStepSeconds); ... }
```

The time scale runs from 0.25 to 32 (`sam3d.TimeScale`, `SimWorldSubsystem.cpp:202-216`). Ctrl+Z reaches x32 in the
testbed (`docs/DYNAMIC_CAMPAIGN_TEST.md:96`), and the lessons fast-forward at x8 (`.cpp:1054-1063`). A network client
never steps (`IsMirrorOnly()`, `.cpp:276-279`).

`ScenarioRunner::Step` (`simcore/include/simcore/scenario_runner.hpp:225-315`) runs everything every step, in this order:
`UpdateTargets` (pilots and the World), `UpdateWaves` (the testbed's waves, releases and neighbour defences), orders,
voice, nets, battery, nav, emission, `UpdateNeighbourFire`, C2, radios, post, the engagement model (which syncs every
target into the EM layer, `engagement.hpp:1309`, and advances it, `:1318`), launcher, guidance, `CueFromWarnings`,
`UpdateArms`, triggers, equipment, crew, player, movers, callouts, the NPC commander, and the Tacview frame once a second.
Nothing in it has a level of detail for aircraft.

### 1.2 What a step costs today (measured, documented)

| Measurement | Wall time | Per runner step | Source |
|---|---|---|---|
| Testbed hour (`ground_check data 3600`, dt 0.1 = 36,000 steps), after R6's third pass | 64.1 s | **~1.8 ms** | `docs/EM_ARCHITECTURE.md:556-561`; `docs/DEVELOPMENT_ROADMAP.md` R6 |
| The same before R6's obstacles (r-track) | 48.4 s | ~1.3 ms | same |
| Kesmark raid, 10 min (6,000 steps) | 0.5 s | ~0.08 ms | same |
| `helper_coverage` over every mission and seat | 14 min 43 s | n/a | ROADMAP R6 |
| EM layer: 200 aircraft / 30 SAM radars / 12 ESM sites | 2.6 ms per 10 Hz tick (worst 23 ms) | n/a | `docs/EM_ARCHITECTURE.md:250-257` |
| EM layer: 500 / 60 / 24, one core | 14.2-14.7 ms per tick (worst 93 ms, cold paths) | n/a | `EM_ARCHITECTURE.md:257`, `:569-573` |
| EM layer: 500 / 60 / 24, 8 threads | 4.2 ms (goal ≤ 5 ms), identical results at any thread count | n/a | `EM_ARCHITECTURE.md:572`; roadmap E8 row (`DEVELOPMENT_ROADMAP.md:596`) |

The testbed is the heavy case: up to 60 (+40) wave aircraft, about 32 neighbour sites with flak worked round by round
(`aaa.hpp`), the battery fire model, the EM link and the crew. R6 put sight lines through every building and wood
footprint, which made the testbed a third slower. The roadmap leaves "obstacle profiles cached per path, if it tells
in the game's fixed step" as an open item (ROADMAP R6 "Left").

### 1.3 The budget per tick (derived from the numbers above)

These budgets are my inference from the measurements. No document sets them.

* **The game at x1:** 20 steps per sim second x ~1.8 ms ≈ 36 ms of game-thread time per real second. That is a few per
  cent of a 60 fps frame budget. Per-step cost at dt 0.05 is somewhat below the dt 0.1 figure, because the EM ticks on
  every second step.
* **At x8** (lessons): 160 steps/s ≈ 0.29 s per real second, about 30 % of the game thread.
* **At x32** (the testbed's Ctrl+Z): 640 steps/s ≈ 1.15 s per real second. **The testbed already cannot run x32 in
  real time.** The 0.25 s frame clamp and the 20 x scale step cap keep the game alive, but the time compression is lost.
* **A recommended envelope for the new system:**
  * **≤ 0.5 ms added per runner step** for the testbed's ~100 aircraft (the step total stays near 2-2.5 ms). That is
    roughly **≤ 5 µs per aircraft per step** for work done every step.
  * Anything heavier (pilot vision scans, line-of-sight checks, decisions, radio traffic, air-to-air radar looks) runs on
    lower cadences (1-5 Hz), **staggered across aircraft by a fixed key** (index or id), not all on one tick. EM does
    the same with `EmRates` and the battery fire model with its 0.5 s sight cache.
  * Watch the worst tick as well as the mean: EM's cold paths spike to 93 ms (`EM_ARCHITECTURE.md:257-259`).
* **What already costs per aircraft per step:**
  * `TargetPilot::HoldHeight` samples the ground and obstacles every 100 m over 6 s of look-ahead (about 17
    `HeightAt` + `ObstacleHeightAt` calls and 17 `ObstaclesAlong` walks at 250 m/s). See
    `simcore/include/simcore/target_behaviour.hpp:295-334`.
  * `GroundFloor` adds up to 60 samples over 3 s (`:255-288`).
  * The engagement model syncs every target into the EM layer (`engagement.hpp:1309`).
  * Pilot vision with line of sight through R6's footprints (`terrain.hpp` `ObstaclesAlong`, `ObstacleChords`) is the
    same expensive call. It must be cached and throttled per pair.
* **A battery link doubles the base cost.** `BatteryLink::Step` steps two runners, the Osa's and the PU-12M's, each with
  its own World and EM (`simcore/include/simcore/battery_link.hpp:76-81`). The aircraft are flown only in the Osa's
  runner, and the post copies their positions (`ScenarioRunner::MirrorTargets`, `scenario_runner.hpp:10893`). A live air
  battle must run in the target-owning runner. Any new per-aircraft state the post must see has to be mirrored.

### 1.4 O(n) and O(n²) traps in today's code

* `World::FindAircraft` is a linear scan (`IndexOfAircraft`, `simcore/include/simcore/world.hpp:147-150`). The runner calls
  it inside loops over targets (`UpdateTargets`, `scenario_runner.hpp:2040-2116`; the waves' lookup, `:2739-2748`;
  `CueTarget`, `:2929-2940`), so these loops are O(n²) in aircraft.
* **The target lists never shrink.** `EnterTarget` appends to `def_.targets`, `targets_`, `eng_targets_` and
  `rwr_warned_` (`scenario_runner.hpp:1940-1954`). Only the World's aircraft are removed (`UpdateWaves`, `:2753-2771`;
  `RemoveAircraft`, `world.hpp:112-118`). Every per-step loop over `def_.targets` (`UpdateTargets`, `CueFromWarnings`
  `:2801`, `CueTarget`) therefore grows with session length. The 4-hour testbed enters well over a thousand.
* `CueTarget` finds a target by comparing id strings (`:2930-2932`).
* An air battle with pairwise sensing (pilot sees pilot, radar sees aircraft, RWR hears radar) needs a broad phase. EM
  uses a uniform 10 km east-north grid rebuilt each tick, sweep gating and cheap bounds first (`EM_ARCHITECTURE.md:262-270`).
  Reuse that, or put the airborne radars into the EM layer as radars and let its culling do the work.

---

## 2. How many aircraft to plan for

| Where | Count | Code / data |
|---|---|---|
| Dynamic campaign testbed (the stress case) | **60 airborne** waves' aircraft (`max_airborne`); a wave can land on a nearly full sky, and the test allows **up to +40** over it. Cruise missiles are not counted. A gone aircraft stays in the World `remove_after_s` 60 s + 5 s. About 35 % of flights are ours (`friendly_share`). | `data/gameplay/test_waves.json` `timing`; `air_war/test_waves_data.hpp:72`; `TestWaves::Due` `air_war/test_waves.hpp:89-96`; `ScenarioRunner::WavesAirborne` `scenario_runner.hpp:11127-11133`; `tests/test_dynamic_campaign.cpp:286` (`<= max_airborne + 40`) |
| Testbed wave shape | a wave every 90-180 s, 3-6 flights, jets entering 55-85 km out, helicopters 18-28 km, jammers orbiting 45 km, AWACS, tankers and ELINT 105 km | `test_waves.json` `timing`, `geometry` |
| A raid played as a scenario | **≤ 24 targets** (`scenario.max_targets`) within **70 km** (`scenario.clip_m`) of the battery. Flights are ranked main (strike, DEAD, helicopters, missiles), then SEAD and hunters, decoys, recce, jammers, escorts; whole flights past the cap are resolved abstractly. | `data/gameplay/air_war.json` `scenario.*`; `air_war/raid_scenario.hpp:330`, `:407-440` |
| A mission session with the war live | **one played package at a time.** `LivePackages` holds one `bridge_`, and the campaign does not advance while it runs. Hence the same ≤ 24. | `air_war/package_injector.hpp:181-203` |
| The campaign (abstract) on the nation's theatre | ~1,100-1,150 sorties a week; the Pact's day 1 has 44 packages and over 300 sorties; 6-8 raids a week through the battery's sector | `docs/air_war/09_BALANCE.md:438-450` |
| EM layer design point | 500 aircraft, 60 SAM radars, 24 ESM sites (E8 goal ≤ 5 ms per tick) | `examples/em_bench.cpp`; `EM_ARCHITECTURE.md` §15, §28 |

**Plan for:**

* **About 100 aircraft at full behaviour** in one session (the testbed's ceiling), plus cruise missiles and air-to-air
  missiles in flight.
* **24-40** in a played raid. Raising `max_targets` to keep escorts and CAPs in is a tuning change.
* **Hundreds a day** in the abstract layer.
* If our fighters join sessions, they count too. Today our side's fighters exist only as one scalar per front,
  `FrontDef::friendly_fighters` 0..1 (`data.hpp:158`, used at `resolve.hpp:231-245`), plus placeholder friendly flights
  in the testbed with no air-to-air at all.

---

## 3. The played vs abstract split (what exists)

### 3.1 Abstract: `simcore/include/simcore/air_war/resolve.hpp`

* `Resolver::Resolve(Package&, ResolveContext&)` (`:101-103`) resolves a **whole package at once**, against the truth.
  "The same rules the planner estimates with, applied to the truth" (`:4`; `docs/air_war/05_EXECUTION.md` §2):
  * every route leg is walked through every working defence zone, with `PassKillChance` (`air_war/planner.hpp:95`);
  * SEAD suppression comes from `PackageSuppression` (`:51-56`), airmanship from `PackageAirmanship` (`:59-62`);
  * our fighters take a scalar chance at unescorted packages (`:231-245`);
  * an armed reconnaissance searches its stretch and attacks the first column it finds (`FindColumn`, `:282-310`).
    This is the only "target seen en route, attack it" logic in the code, and it is abstract.
* **When:** `AirCampaign::StepTo` (`campaign.hpp:651-700`) calls `ResolveAbstract(p)` (`:846`) for every package whose
  first aircraft takes off inside the current hour step. **No abstract package has a position in time while it flies.**
  Nothing can re-route or retask it in flight.
* `ResolveContext` (`resolve.hpp:30-47`) can leave out a played circle and time window (`played_centre`,
  `played_radius_m`, `played_from_h`, `played_to_h`). `ResolvedPieces` (`:72`) cuts each leg into what is resolved here
  and what the scenario flew.

### 3.2 Played: `raid_scenario.hpp`, `package_injector.hpp`, `mission_bridge.hpp`

* `RaidScenarioBuilder::Build` (`raid_scenario.hpp:281`) clips each flight to the 70 km circle and ranks and caps flights.
  Each aircraft becomes one scenario target in formation offsets (`flight_spacing_m` 800). It writes JSON for the strict
  loader with these per-target keys:
  * `behaviour`: terrain following, or pop-up for helicopters with a hide;
  * `evade`: on lock if the type has an RWR, else on launch; jink low, break high, dive for helicopters;
  * `rwr` and EM `arms` for SEAD and hunters, by the year (`ArmEm(d.theatre.year)`) (`:441-540`).
* `InjectPackage` / `LivePackages` (`package_injector.hpp`) bring a package into a running session with
  `ScenarioRunner::AddTargets`, `AddEmGear` and `AddTriggers` (`scenario_runner.hpp:11078`, `:11170`, `:11139`).
* `MissionBridge::Step(ScenarioRunner&)` (`mission_bridge.hpp:70`) reads the runner's public state and handles:
  * weapon release and impact;
  * ARMs: EM ones are read from `ArmResults`, otherwise the gameplay model;
  * the neighbours by the abstract `PassKillChance` (`Neighbours`, `:492`; `Engage`, `:539`);
  * what the enemy learns: ELINT seconds, and launches seen within `scenario.revealed_by_launch_m` 15 km (`:85-90`).

  `Finish()` (`:238`) returns a `PackageOutcome`, and `AirCampaign::CompletePlayerMission` (`campaign.hpp:198-250`) applies
  it, then resolves the rest of the package abstractly with the played circle cut out.
* The other Osa batteries of the regiment fight in `BatteryFire` (`battery_fire.hpp:60`): abstract crews, geometric sensors
  and the same zones. This is a medium level of detail for ground defence.

### 3.3 Inside a session: one level for every aircraft

* Every target is `Aircraft` (constant airspeed, turn-rate limited, straight lines in ENU; `aircraft.hpp:17-30`;
  `StepAircraft`, `:51-76`) stepped by `World::Step` (`world.hpp:79-109`). A `TargetPilot` (`target_behaviour.hpp:70`)
  adds:
  * terrain following and pop-up (`TargetBehaviourDef`, `:31-45`);
  * evasion (`EvadeDef`, `:48-56`: Break, Dive or Jink), set off by `OnLaunch` and `OnLocked`;
  * a ground floor.
* The cue to evade comes only from **the player's own Osa's lock or launch** (`CueTarget(id, launch)`,
  `scenario_runner.hpp:2929`, using `def_.unit_position`), or from an EM warning receiver hearing a lock or command link
  (`CueFromWarnings`, `:2799-2816`).
  * A pilot without an RWR reacts to the **truth** of a launch, not to seeing it.
  * Nothing cues a pilot from the battery fire model's launches, the neighbours' missiles, flak, or anything seen.
  * The roadmap's R4 ("Defensive reactions") is not built.
  * In the testbed only, damage brings smoke, jettison and a turn for home (`CrashDamaged`, `:2664`;
    `TestWaves::Jettison`, `test_waves.hpp:166`).
* R3 (flight with attitude, bank-to-turn, energy) is not built. `docs/ARCHITECTURE.md:260-261` lists "constant-speed,
  turn-rate-limited point (no energy, no g limit)" as a known limitation.
* **There is no air-to-air in the code:**
  * no airborne radar as a sensor (EM data has airborne fire-control radars only as emitters: `generic_airborne_fcr_x`,
    `nato_fighter_fcr_x`);
  * no air-to-air missiles, guns, pilot vision or flight-to-flight comms;
  * `AircraftType::air_to_air` is only "value as an escort, 0..1" (`data.hpp:355`).

  `World` does step a generic PN `Missile` (`world.hpp:66-77`, `:93-106`; `missile.hpp`), which only the engagement demo
  uses. The scenario path resolves its launches in the engagement model.

### 3.4 Level of detail: what exists and what is needed

**Exists**

| Level | Where | What moves | Where in the code |
|---|---|---|---|
| Campaign (abstract, hourly) | every package not played | nothing moves: a package is resolved whole at its take-off hour | `campaign.hpp:651-700`, `resolve.hpp` |
| Played circle and cap | a raid's flights inside 70 km, ≤ 24 aircraft | targets flown per aircraft; the rest abstract by `ResolvedPieces` | `raid_scenario.hpp:330-440`, `resolve.hpp:72` |
| Neighbour defences | in a raid, and in the testbed | per-pass abstract chance (bridge, `FireNeighbourDefences` `scenario_runner.hpp:2243`); flak round by round in the testbed (`aaa.hpp`) | `mission_bridge.hpp:492-600` |
| Other Osa batteries | the regiment's | `BatteryFire`, sight cached 0.5 s | `battery_fire.hpp` |
| **Ground movers M0/M1/M2** | campaign / session out of view / in view | M0 by plan; M1 head and tail on the path; M2 on the ground. Hand-over keeps the place along the route, with a hysteresis margin. **Keyed to the own vehicle's eye and the visual range from the sun and the weather, not the camera.** | `nav/movers.hpp:9-12, 26, 160-171`; `ScenarioRunner::StepNav` `scenario_runner.hpp:3366-3371`; `data/gameplay/visual_range.json` |
| EM | all of the session's aircraft are EM platforms | no LOD ("level of detail for distant units" is E8's open item) | `EM_ARCHITECTURE.md:575` |

Planned, not built:
* roadmap 15.5, the regiment's other batteries at L1 and headless L2 sessions, with `war_calibrate` comparing them
  (`DEVELOPMENT_ROADMAP.md:405`; `docs/campaign/05_THE_SYSTEMS.md:149-151`). `war_calibrate` does not exist;
* "the scenarios' own balance", played against abstract (`docs/air_war/09_BALANCE.md` §6, `:264-276`).

**Needed for a live air battle (proposal, following the movers' pattern)**

* **A0, campaign.** Package-level. For HQ to re-route or retask in flight, an abstract package needs a position in time
  (route points carry `t_h` already, `RoutePoint`, `air_war/state.hpp:18-23`). Otherwise retasking stays confined to packages in a session. Any
  retasking in A0 must stay inside the campaign's determinism (section 4.3).
* **A1, session, far.** Out of range of every sensor and eye of ours (the battery's radars, the PU-12M's radars, the
  player's eye or optic), and not engaged. Flight-level:
  * the leader stepped, wingmen at formation offsets;
  * the terrain look-ahead only for the leader;
  * decisions and radio at a low cadence;
  * no per-aircraft vision scans;
  * radars and jammers still radiate in EM exactly as at A2, because what the enemy emits is what our ESM hears.
* **A2, session, near or engaged.** Per-aircraft pilot: vision, RWR, evasion, R3's attitude, air-to-air.
* **Rules for the levels:**
  * Key the levels to **simulation facts** (our units' positions and ranges, engaged state, the visual range from
    `light_` and the weather, as `StepNav` does), **never the Unreal camera**. Then headless tools, the game and a
    dedicated server agree.
  * Use a margin against flicker, and keep every piece of state across a hand-over: route place, fuel, stores, damage,
    the pilot's knowledge.
  * The hand-over itself must be deterministic.

---

## 4. What must be deterministic

### 4.1 The rule and the generators in use

"Everything in simcore is deterministic for a given sequence of inputs" (`docs/ARCHITECTURE.md:87-88`). Runs are
deterministic for a seed (`:171-175`). The runner uses `std::mt19937` with a portable mapping, `Uniform01`
(`engagement.hpp:827`), because "std:: distributions are not" fully specified (`scenario_runner.hpp:12-14`). **Each
subsystem has its own generator, seeded from the scenario seed**, so adding draws in one does not move another's
sequence:

| Generator | Seed | Line |
|---|---|---|
| `rng_` (crew reactions, engagement) | `def_.seed` | `scenario_runner.hpp:94`, `:7325` |
| crew skill, per member | `seed*7919 + i*104729 + 17` | `:115` |
| `kill_` | `seed*6007 + 3` | `:143` |
| `fx_rng_` (presentation only) | `seed + kFxSeed (0xF0E5)` | `:144`, `:10100-10102` |
| `defence_rng_` (testbed neighbours) | `seed + kDefenceSeed (0xDEF5)` | `:2190`, `:2230`, `:2246` |
| `battery_fire_rng_`, `neighbour_fire_rng_` | `seed*4099+7`, `seed*6151+11` | `:10607`, `:10612`, `:10651`, `:10734` |
| `hear_rng_`, `relay_rng_` | fixed | `:12262-12263` |
| CommandPost, post EM | `seed ^ 0x51ed27`, `seed ^ 0x9e3779b9` | `:168`, `:177` |
| `TestWaves` | `seed*2654435761 + 97` | `test_waves.hpp:80` |
| `MissionBridge` | `seed*2654435761 + 99`; its fx `seed + kFxSeed` | `mission_bridge.hpp:57-60` |
| EM layer | counter-based keyed draws `f(seed, stream, keys)`, independent of evaluation order and thread count | `EM_ARCHITECTURE.md` §14 (`:240-245`) |
| Air campaign | `Rng` SplitMix64, sequential: the order of draws matters | `air_war/types.hpp:395-420`; `campaign.hpp` `Init` |

### 4.2 Decisions in the new system that must be deterministic

Each must be a function of the data, the seed, the fixed step and the ordered player inputs:

* flight path and route changes: re-routing round defences, terrain following, avoidance;
* perception: what a pilot sees (smoke trail, convoy, site), what his RWR hears (already EM-keyed), radar contacts;
* the pilot's and leader's choices: evade, jettison tanks or stores, press on, abort, take a target of opportunity
  (gun run, bombing run, recce pass), call it in;
* radio between flights, squadrons and HQ: who transmits, who hears, when, what is intercepted. The EM nets'
  delivery is already keyed by (message, recipient);
* HQ's retasking and vectoring of SEAD or strike onto a launch area;
* air-to-air detection, identification, launch, missile flight and kill draws; damage;
* the testbed's wave generator, which **reads the war's state** (`docs/DYNAMIC_CAMPAIGN_TEST.md:123-126`: from wave 4
  "the generator draws another war" once aircraft fly differently). Expect the testbed's rows to move with any change
  to behaviour.

**Never feed a decision from:**
* wall-clock time, frame rate or Unreal ticks;
* thread scheduling, or the iteration order of pointer-keyed or unordered containers (`BatteryFire` uses an
  `unordered_map` only as a cache keyed by id, `battery_fire.hpp:85`);
* **the presentation feed.** Nothing in the simulation reads `fx::Feed`. A pilot "seeing a smoke trail" must come from a
  sim fact (a missile in flight, its motor time, the light and the visual range), not from the fx events. The test
  `fx: with the feed switched off the practice shoot is the same` (`tests/test_fx_events.cpp:331-345`) enforces this
  pattern.

**Give the new system its own generator** (`seed + constant`) or EM-style keyed draws, so that the crew's and the
engagement's sequences do not shift.

### 4.3 Saves and the campaign

* An air war save is the data files, the seed and the player's actions in order
  (`air_war/save.hpp:1-10`; `CampaignAction` `campaign.hpp:75-85` with kinds `Advance`, `Relocate`, `Skip`, `Complete`,
  `Transmit`; `Replay` `:330-354`). Numbers are written with 17 significant digits.
* **A played raid's outcome is stored in the `Complete` action**, so a session need not replay bit for bit to load a
  campaign. Only the abstract side must.
* Consequence for HQ reacting live: if the campaign is to change anything from what happened inside a session (a launch
  area called in, a convoy reported, a flight diverted), that input has to enter the campaign as a **recorded action**,
  as `Transmit` does for our side's calls (`campaign.hpp:68-73`; `command/air_war_feed.hpp`). It must not be read from
  the runner's state on the side.
* Today the campaign does not advance at all while a bridge is active (`LivePackages::Step` returns early,
  `package_injector.hpp:182-203`).
* Unreal's campaign subsystem is "standalone games only" (`unreal/.../Public/SimAirCampaignSubsystem.h:60`).

### 4.4 Determinism gaps today (do not copy them)

* **The game and the tools step differently:** 0.05 s in Unreal against 0.1 s headless. One seed gives different runs.
  Headless balance tables are not frame-for-frame what a player sees.
* **The game steps the mission bridge per frame:** `USimAirCampaignSubsystem::Tick` calls `Bridge->Step(*Runner)`
  (`unreal/.../Private/SimAirCampaignSubsystem.cpp:607-647`) after however many fixed steps ran. Its results depend on
  frame rate. Headless, `LivePackages::Step` and `MissionSession::Step` (`campaign/mission_session.hpp:100-111`) step it
  once per runner step. **New live air logic must be stepped inside the fixed step** (`ScenarioRunner::Step`, or a
  session wrapper called per step), never from an Unreal tick. CLAUDE.md says the same: "Never step simcore from actor
  ticks".

---

## 5. Multiplayer constraints

`docs/MULTIPLAYER.md`:
* The server runs the one simulation, and a client never steps it (`:13-15`; `IsMirrorOnly`).
* Each player gets only his node's knowledge (`em::BuildView`, `simcore/include/simcore/em/replication.hpp`). Views have
  **no field** for platforms or truth, and a canary test proves names don't leak (`EM_ARCHITECTURE.md` §30;
  `tests/test_em_replication.cpp`).
* The console is mirrored ten times a second (`:28-31`).
* **The aircraft are not replicated:** "The 3D world on a client (the target aircraft actors ...) still comes from the
  client's own unstepped world; entity positions need a stream of their own" (`:104-105`).

So a live air battle lives wholly in simcore on the server. Attitude (R3), smoke, stores and air-to-air missiles reach a
client only once an entity stream exists, which is not built. What a player learns of the enemy's radio goes through his
node's knowledge or the information pipeline. CLAUDE.md "First person": never an omniscient HUD.

---

## 6. Rules any change here must respect (CLAUDE.md and the docs)

* **simcore:**
  * header-only C++17, standard library only, no exceptions, RTTI, `throw`, `dynamic_cast` or `typeid`;
  * no identifiers that Unreal or Windows define as macros (`check`, `verify`, `ensure`, `PI`, `TEXT`, `IN`, `OUT`,
    `ERROR`, `near`, `far`, `small`, `interface`, `min`, `max`, `DELETE`);
  * SI suffixes, ENU frame, `LocalFrame::AltitudeOf` for heights;
  * `-Wall -Wextra -Wshadow -Wconversion -Werror` and `/W4 /WX`;
  * **every new model or behaviour gets a test.**

  `tests/header_check.cpp` defines the macros and compiles the umbrella `simcore/simcore.hpp`, which includes the
  runner, `em.hpp`, `air_war.hpp` and `campaign.hpp`.
* **On the devloop branch** (when it is merged):
  * runner member functions are defined in `simcore/runner/<area>.hpp`;
  * a piece with its own state and data becomes its own class, held by `std::unique_ptr`, with small tests, as
    `neighbour_defences.hpp` was;
  * `Load...` entry points move to `loaders_impl.hpp` (`tools/split_loaders.py`);
  * every new test file joins exactly one area in `simcore/CMakeLists.txt`;
  * a whole-mission test is `SC_SLOW_TEST`, and a fast test runs under 1 s (`docs/TESTING.md` §2-§5 on devloop).

  A branch started from this worktree edits the old one-file runner and must be brought over as `docs/TESTING.md`
  "Branches started before the split" describes.
* **The crew and the helper:** every NPC act at a seat goes through `ScenarioRunner::NpcDoes(seat, SeatDuty)`. The test
  `tests/test_helper.cpp:284-296` counts `NpcAt(` in `scenario_runner.hpp` and expects **23**. After crew, console or
  mission changes, run `helper_coverage` and read its gaps against the base build's NPC rows (they move with the
  testbed's random war).
* **Knowledge, never the truth:**
  * our NPCs act on their instruments and what they heard;
  * the enemy's staff plans on its intelligence picture, which "never reads the truth" (`docs/ARCHITECTURE.md:198-205`);
  * enemy pilots and HQ should likewise act on what they perceive and are told.
* **Our side's information:** intelligence, warnings and after-action reports reach the player only through the
  pipeline (`simcore/command/`). The written record is the war's memory: reports come from the crews' records, never
  the truth.
* **Data:**
  * system values need `source` and `confidence`; gameplay numbers live in `data/gameplay/` labelled as game values;
  * `tools/check_citations.py` checks the sources;
  * public sources only, operator-country sources preferred (`docs/SOURCES.md` tiers);
  * every faction "fights as it actually did": per-faction doctrine already has `centralisation` (0 decentralised .. 1
    rigid timing from above) and `on_call_fraction` (`DoctrineDef`, `data.hpp:429-449`), natural hooks for "who favours
    rapid target changes".
* **The Python prototype mirror:**

  | Mirrored (must stay in sync) | simcore | prototype |
  |---|---|---|
  | `Aircraft` steering | `aircraft.hpp` | `engagement.py` `Aircraft` |
  | PN missile and the World | `missile.hpp`, `world.hpp` | `engagement.py` `pn_accel`, `step_missile`, `World` |
  | EM propagation and radar models | EM layer | `em.py` |
  | EM keyed random streams | `em/random` | `em.py` `draw_hash`, `Random` |
  | the optical sensor's contrast and resolution | `em/optical.hpp` | `em.py` `apparent_contrast`, `contrast_range_m`, `lines_across` |
  | the obstacle footprints | `terrain.hpp` | `radar.py` `Footprints` |

  R3 states "The Python prototype is kept in sync" (ROADMAP R3). **Not mirrored:** `target_behaviour.hpp`, the runner,
  the waves and the air war.

---

## 7. Tests and tools a change here must re-run

### 7.1 In this worktree (no test areas)

* `cmake --build simcore/build --config Release` then `ctest --test-dir simcore/build -C Release --output-on-failure`
  runs `simcore_tests` (every test file) plus the ctest entries in `simcore/CMakeLists.txt:133-252`:
  * `procedure_demo`, `battery_link_demo`;
  * `mission_demo`, `mission_demo_war` (`--war warsaw_pact 1 1`), `mission_demo_day` (`--hours 24`);
  * `scenario_demo`, `scenario_demo_em`, `crew_pose_dump`, `em_demo` (11 checks);
  * `air_war_demo` (Pact, 2 days), `air_war_demo_nation` (NATO, the nation's theatre, Kesmark map);
  * `country_dump_route`, `nav_check 200`, `ground_check 120 scenarios/kesmark_range_test.json`.
* `.\tools\verify.ps1` also runs:
  * `engagement_demo`;
  * `em_bench data 300 40 16` at 1 and 4 threads, with **checksums that must match**;
  * `air_war_demo` on the nation's theatre against all four factions with `maps/kesmark_range.json`;
  * `air_war_bench data 100`;
  * **`ground_check data 3600`**, which must list nothing;
  * `python -m pytest prototype tools -q`.
* **Tests that guard this area** (the files exist here):
  * `test_dynamic_campaign.cpp`:
    * waves, releases and the defences, `:78-194`;
    * the 30-minute session with the airborne bound, `:194-299`;
    * the battery's mask, `:302`;
    * "nothing is under the ground", `:335`;
  * `test_air_war.cpp` (incl. A11 EM ARMs), `test_packages_live.cpp`, `test_mission_session.cpp`, `test_outcomes.cpp`,
    `test_director.cpp`, `test_logistics.cpp`, `test_weapons.cpp`;
  * `test_damage.cpp`, `test_ground.cpp`, `test_map_obstacles.cpp`;
  * `test_em_arm.cpp`, `test_arm_drill.cpp`, `test_em_threats.cpp`, `test_em_link.cpp`, `test_em_replication.cpp`,
    `test_em_models.cpp` ("em scale": identical results at any thread count);
  * `test_fx_events.cpp` (the feed off changes nothing), `test_fx_bridge.cpp`;
  * `test_battery.cpp`, `test_battery_link.cpp`, `test_threats.cpp`, `test_engagement_model.cpp`, `test_helper.cpp`
    (the `NpcAt(` count);
  * `header_check.cpp` (object library, compiled `-fno-exceptions` under GCC/Clang strict).

### 7.2 On `feature/devloop` (once merged): `tools/test.ps1 <area> [-Fast]`, then `-All` before a commit

| Area | Test files | Why it is touched |
|---|---|---|
| `air_war` | `air_war`, `packages_live`, `logistics`, `dynamic_campaign` | the core |
| `threats` | `threats`, `em_threats`, `em_arm`, `arm_drill`, `emission_regime` | RWR, evasion, ARMs |
| `weapons` | `damage`, `weapons`, `munitions`, `neighbour_defences` | stores, jettison, guns |
| `radar` | `em_*`, `scopes`, `scope_feed` | airborne radars, intercept |
| `engagement` | `engagement`, `engagement_model`, `kill_assessment`, ... | evasion against the Osa |
| `campaign` | `mission_session`, `outcomes`, `director`, ... | live packages, ledgers |
| `fx` | `fx_bridge`, `fx_events`, ... | presentation of the new events |
| `terrain` | `ground`, ... | nothing under the map |
| `battery` | `battery`, `battery_link`, ... | mirroring, battery fire |
| `crew` | `helper`, ... | helper rules |

The areas come from `simcore/CMakeLists.txt` `SIMCORE_TESTS_<area>` on devloop.

### 7.3 Balance and coverage (tables; not in ctest; compare against the base build's output)

* `air_war_balance data 20 7` and `air_war_balance --theatre theatre_pannonia_nation.json data 10 7`
  (`docs/air_war/09_BALANCE.md`). Any change to `resolve.hpp`, the planner or the pass chances moves them.
* `air_war_bench data 400`: weapons, attacks and losses against history (`09_BALANCE.md` §9-10). It flies through the
  abstract resolver.
* `crew_balance data 10`: every crew scenario at every tier.
* `helper_coverage data docs/crew/generated/helper_coverage.md`: about 15 minutes. Compare the gaps with the base's NPC
  rows.
* `ground_check data 3600`: PASS required.
* `aaa_bench` if flak or damage is touched.
* `em_bench data 500 60 24 [threads]` as a trend (E8 goal ≤ 5 ms; the checksum identical across thread counts).
* Track R's practice: report `crew_balance`, `air_war_bench` and `em_demo` as **byte-identical or with every moved
  cell explained**, and the testbed's rows in `helper_coverage` against the base.

### 7.4 Unreal (Claude cannot run the editor; ask the user)

* Automation: `SAM3D.AirWar.Raid`, `SAM3D.Map.GroundAgrees`, `SAM3D.Knowledge.*`.
* `python tools/ue_ground_check.py` (`-SimGroundCheck`); `tools/ue_crew_smoke.ps1` (dedicated server and two clients)
  for multiplayer.

---

## 8. Cold War consistency hooks relevant to scale

* The year gate is data-driven (`TheatreDef::year`, `in_service_year`, `ArmEm(year)`). New air-to-air missiles, airborne
  radars, jammers, datalinks or procedures need the same gate and a source. Otherwise the 1985 war gets later kit.
* **The testbed ignores the year** by design (`test_waves.json` `notes`; `DYNAMIC_CAMPAIGN_TEST.md:43`). If the new
  behaviour is judged in the testbed, either gate the waves by `TheatreDef::year` or keep in mind that post-1985 types
  appear there.
* Command and control in the code is period-shaped:
  * planning cycles (`DoctrineDef::planning_cycle_h`, `lead_time_h`), centralisation and on-call sorties;
  * AWACS, tanker and ELINT orbits (`docs/air_war/11_SUPPORT.md`);
  * the enemy's COMINT with delays measured in hours (`IntelDef::comint_delay_h`, `data.hpp:462-464`).

  Any "HQ retasks on the fly" has to run through that: voice radio, AWACS or ground controllers, with the delays, rather
  than an instant picture shared by all.
* The EM layer already makes every transmission radiate and be direction-found (`EmWorld::AddNet`, `AddDummyTraffic`;
  `EM_ARCHITECTURE.md` §29). "What an intercept reveals by its content" is **not modelled** (`:586`), so flight-to-HQ
  chatter as intelligence for our side needs that.

---

## 9. Where new behaviour would attach (hooks)

* `ScenarioRunner::Step` (`scenario_runner.hpp:225-315`): a new per-step update between `UpdateTargets(dt)`/`UpdateWaves()`
  and the engagement model, or inside `UpdateTargets` before `world_.Step` (where pilots set waypoints:
  `s.pilot.BeforeStep`, `:2052`).
* `TargetPilot` (`target_behaviour.hpp:70`): `BeforeStep`, `AfterStep`, `OnLaunch`, `OnLocked`, `EvadeNow`, `Evade`
  (`:185`). R4's perception-driven reactions extend these. `CueTarget` and `CueFromWarnings` (`scenario_runner.hpp:2929`,
  `:2799`) are today's cue paths.
* `Aircraft`, `StepAircraft` (`aircraft.hpp:17`, `:51`) and `World::Step` (`world.hpp:79`): R3's attitude and energy.
  The World's PN `Missile` could fly air-to-air missiles; the prototype must mirror it.
* `TestWaves::Next`, `EnemyFlight`, `FriendlyFlight`, `Releases`, `Jettison` (`test_waves.hpp:99`, `:118`, `:166`): the
  testbed's flights, our fighters and stores.
* `RaidScenarioBuilder::Build` (`raid_scenario.hpp:281`): what a played raid's aircraft carry (behaviour, evade, rwr, arms),
  the cap and its ranking (escorts are cut first today).
* `MissionBridge::Step` and `Finish` (`mission_bridge.hpp:70`, `:238`): what flows back to the campaign; `PackageOutcome`
  fields.
* `LivePackages::Step` (`package_injector.hpp:181`); `AirCampaign::StepTo`, `ResolveAbstract`, `CompletePlayerMission`
  (`campaign.hpp:651`, `:846`, `:198`); `CampaignAction` (`:75`) for recorded live inputs; `Resolver::Resolve`
  (`resolve.hpp:103`) and its fighter chance (`:231`) and armed reconnaissance (`:282`).
* EM: `EmSensorLink` (`em_link.hpp`) for airborne radars as radars (`EmWorld::AddRadar(platform_id, def_id)`, `em/world.hpp:162`, on an air platform), RWRs
  (`SetTargetRwr`, `WarningOf`), flight and HQ nets (`EmWorld::AddNet`, `em/world.hpp:256`; `AddDummyTraffic`, `:227`) and intercept.
* The level-of-detail template: `nav::MoverLevel` and `UpdateLevels` (`nav/movers.hpp:26`, `:160`), driven from
  `ScenarioRunner::StepNav` (`scenario_runner.hpp:3366`).
* Presentation only: `fx::Feed` (`fx/events.hpp`) and Unreal's entity stream (`USimWorldSubsystem::BuildEntityList`,
  `RebuildEntityCache`, `Private/SimWorldSubsystem.cpp:2416`).
