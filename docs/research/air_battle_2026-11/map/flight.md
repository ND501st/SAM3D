# Map: how an aircraft flies and reacts in a SAM3D session today

Worktree read: `C:\dev\SAM3D-wt\air-battle` at 1d09489 (nothing changed). All paths below are relative to it.
Scope: the session's aircraft (the scenario runner's targets), not the air war's planning. The planner and the
abstract resolver appear only where they decide what a session's aircraft carry.

Short answer: an aircraft is a **constant-airspeed point mass that turns its velocity vector toward the next waypoint at
a fixed turn rate**, with a separate height-holding layer (terrain following, pop-up, dive) and a ground floor. It has
**no attitude, no g limit, no energy, no roll-in**. A pilot's only decisions are a **one-shot evasion** (break, dive or
jink) on two cues from our Osa (its radar lock, its launch) or the instructor, a rule-based ARM shot and turn-away,
and, in the dynamic campaign testbed only, **jettison and abort drawn by chance from damage**. Everything is **per
aircraft**: a "flight" is a name prefix and a lateral offset of the same route. Nothing of R3 (attitude) or R4
(defensive reactions) is built. There is no air-to-air combat in a session at all.

---

## 1. The flight model (`simcore/include/simcore/aircraft.hpp`, 75 lines)

### 1.1 State
`struct Aircraft` (aircraft.hpp:17-31):

| Field | Meaning |
|---|---|
| `position`, `velocity` | ENU m, m/s. `velocity` is over the ground (wind included). |
| `speed_mps` | airspeed, held constant (default 200) |
| `max_turn_rate_rad_s` | the only manoeuvre limit (default 6 deg/s) |
| `rcs_m2` | a scalar RCS ("aspect-dependent later") |
| `waypoints`, `next_waypoint`, `waypoint_capture_radius_m` (500 m) | the route |
| `alive`, `bottom_m` | dead/alive; belly below centre for the ground floor |

No heading/attitude field (bank, pitch, roll), no g limit, no thrust/drag/mass, no fuel, no stores, no flight id, no
side other than what `ScenarioTarget::friendly` says.

### 1.2 Step
`StepAircraft(Aircraft&, dt, wind_mps)` (aircraft.hpp:51-73):
- the nose is `velocity - wind` normalised (aircraft.hpp:54);
- capture: within `waypoint_capture_radius_m`, or the waypoint lies inside one of the turn circles
  (`WaypointInsideTurn`, aircraft.hpp:35-46, radius = speed / turn rate) -> `++next_waypoint` (no circling);
- otherwise the wanted direction is the unit vector to the waypoint, corrected for wind by the wind triangle
  (aircraft.hpp:63-67), and the nose is rotated toward it by at most `max_turn_rate_rad_s * dt` with `TurnToward`
  (vec3.hpp:73-83; a rotation about `from x to`, i.e. **in 3D**);
- `velocity = dir * speed_mps + wind`; `position += velocity * dt`.

Consequences:
- **Turn rate is independent of speed**: at 250 m/s a 20 deg/s evasion is ~8.9 g; at 100 m/s the same 6 deg/s is
  ~1 g. There is no load-factor limit.
- **Heading changes start instantly** at the full rate (no roll-in/roll-out).
- **Speed never changes** by manoeuvre or climb. The only speed changes are the pop-up's hover (`speed_mps = 0`,
  target_behaviour.hpp:158-159), the instructor's Speed order (scenario_runner.hpp:11033-11035) and the waves' strike
  `attack_mps` set at spawn (test_waves.hpp:416).
- An aircraft **without** a height behaviour turns its 3D velocity toward 3D waypoints (climbs and dives come from that
  rotation at the same turn rate). One **with** a height behaviour is flattened first (`BeforeStep`, §2.2) and moves in
  the horizontal only; its height is set afterwards.
- World stepping: `World::Step` (world.hpp:79-91) calls `StepAircraft` for every aircraft with the weather's wind at
  its altitude. Fixed step in the game: `ScenarioStepSeconds = 0.05` (unreal/.../SimWorldSubsystem.h:1918).
- `docs/ARCHITECTURE.md:211` and `:260-261` record this as a known limit: "a constant-speed, turn-rate-limited point
  (no energy, no g limit)".

### 1.3 Where attitude already exists (not for live aircraft)
- `fx::WreckFall` (fx/trajectories.hpp:343-460): a destroyed aircraft's fall, a point mass with drag and a **banked
  lift vector** (`Accel`, :442-455: bank 55-70 deg, 1.0-1.4 g fading), and `WreckRotation {yaw, pitch, roll}`.
  Presentation only, but it is the one place in simcore with bank-to-turn maths.
- Tacview: `AcmiWriter::WriteObject` (acmi.hpp:38-48) writes roll as a literal `0`, pitch and heading from the velocity.

### 1.4 How Unreal turns the model
- `USimWorldSubsystem::MakeState` (unreal/SAM3D/Source/SAM3D/Private/SimWorldSubsystem.cpp:2040-2051):
  `S.Rotation = S.Velocity.IsNearlyZero() ? FRotator::ZeroRotator : S.Velocity.Rotation();`
  So yaw and pitch follow the **ground** velocity (a crabbing aircraft is drawn along its track, not its nose), roll is
  always 0, and a hovering helicopter snaps to yaw 0 (north).
- `BuildEntityList` (:2464-2476) builds every aircraft through `MakeState`; `ASimEntityActor::Tick`
  (SimEntityActor.cpp:241-300) does `SetActorLocationAndRotation(At, State->Rotation)`; a wreck is drawn from the fx
  subsystem's `GetWreckPose` (:203-218).
- `FSimEntityState` (Public/SimWorldSubsystem.h:30-60) carries `Location`, `Velocity`, `Rotation`; no bank or attitude
  field of its own.
- Multiplayer: clients do not get aircraft entities from the server (`docs/MULTIPLAYER.md:104-105`: "entity positions
  need a stream of their own"), so any attitude has nowhere to travel yet.

### 1.5 Python mirror (`prototype/samproto/engagement.py`)
`class Aircraft` (:18-60) mirrors `WaypointInsideTurn` and the turn-rate step **without wind**. There is no mirror of
`TargetPilot`, `damage.hpp`, `aaa.hpp`, the waves or the runner. CLAUDE.md requires the mirror kept in sync "when
changing a model that both implement", and R3 explicitly says "The Python prototype is kept in sync".

---

## 2. The pilot (`simcore/include/simcore/target_behaviour.hpp`, 360 lines, roadmap 9.3)

### 2.1 Definitions
- `TargetBehaviourDef` (:32-47): `Kind {None, TerrainFollowing, PopUp}`, `agl_m`, `max_climb_mps`, `lookahead_s`,
  `waypoint_agl_m[]`, pop-up `hide_agl_m`, `popup_agl_m`, `first_hold_s`, `expose_s`, `hidden_s`, `cycles`.
- `EvadeDef` (:49-59): `On {Launch, Lock, Order}`, `Kind {Break, Dive, Jink}`, `turn_rate_rad_s` (12 deg/s),
  `dive_agl_m` (30), `jink_period_s` (4), `evade_s` (20).
- Header comment (:1-15): "All numbers come from the scenario (PLACEHOLDER flight profiles, not data of a real
  aircraft)".

### 2.2 The step hooks the runner calls
- `OnSpawn` (:87-89): a pop-up helicopter gets a 100 m capture circle.
- `BeforeStep` (:93-97): if the pilot controls height, every waypoint's z is set to the aircraft's z and `velocity.z = 0`
  (horizontal steering only).
- `AfterStep` (:101-114): pop-up phases (`StepPopUp`, :153-183), jink flips (`StepJink`, :222-237), `HoldHeight`
  (:295-336) and `GroundFloor` (:255-288). Returns an event string for the debrief.
- `HoldOverGround` (:130-132): only the floor, for an aircraft past its last waypoint.

### 2.3 Height
- `HoldHeight` (:295-336): the highest ground plus obstacles (`Terrain::ObstacleHeightAt`, `ObstaclesAlong`) over
  `lookahead_s` ahead; vertical speed eased within `max_climb_mps` and `kVerticalAccelMps2 = 15` (:337, "about 1.5 g
  beyond level flight: a game value"), levelling over `kLevelOffS = 1`. A dive uses at least 50 m/s (:321). The vertical
  speed is **added** to the horizontal `speed_mps`, so a climb does not cost speed.
- `GroundFloor` (:255-288, roadmap R5): belly 1 m over the highest ground 3 s ahead (`kFloorLookS`, `kFloorClearM`);
  never the belly under the ground. A weapon (`SetWeapon(true)`) has none.
- `WantedAglM` (:140-147): dive height, pop-up phase height, or the waypoint's own `waypoint_agl_m`.

### 2.4 Evasion: what exactly happens
`Evade(a, unit, t)` (:185-220), reached through `OnLaunch` (:117-119), `OnLocked` (:120-122) or `EvadeNow` (:124-128):
- ignored if already evading or dead;
- a hiding/exposed pop-up helicopter drops behind cover (:188-193);
- the aircraft's turn rate is raised to `e_.turn_rate_rad_s` (:194-195);
- **Jink**: the route is saved (`saved_waypoints_`, `saved_next_`), and the aircraft is sent at a point 20 km off at
  +-60 deg from its original heading, flipped every `jink_period_s`, until `evade_s`; then the route and turn rate are
  restored and `e_.present = false` ("once", :229). (`SetJinkPoint`, :239-245.)
- **Break**: one waypoint 30 km along the beam to the line of sight from the **unit** (`def_.unit_position`, i.e. our
  Osa, whatever fired) (:209-214). **Dive**: the same, plus `diving_ = true` (hold `dive_agl_m`).
- After a **break or dive the pilot never returns to his route**: `evading_` stays true, the 30 km beam point is the only
  waypoint left, and once it is passed the runner marks the target `gone` ("passed its last waypoint",
  scenario_runner.hpp:2112-2115). In effect a break is a mission abort; a waves strike aircraft that breaks never comes
  within its release range again, so it drops nothing. Only the jink resumes.
- One evasion per aircraft per life (the instructor can repeat a finished jink).

### 2.5 The legacy evasion (engagement.hpp)
`ScenarioTarget::evade_on_launch` (scenario.hpp:94-95) -> `EngagementTarget::evade_on_launch` (engagement.hpp:762-767);
on our launch `EngagementModel::Evade` (engagement.hpp:2464-2474) overwrites the waypoints with the same 30 km beam
point. The `"evade"` block "takes the place of" it (scenario.hpp:101).

### 2.6 Evasion costs our hit chance
The engagement model measures the target's turn rate from its velocity change each step
(`g.turn_rate_deg_s`, engagement.hpp:1263), keeps the shot's maximum (:1391), and multiplies the hit chance by
`max(manoeuvre_factor_min, 1 - manoeuvre_penalty_per_deg_s * max_turn_deg_s)` (:1403-1404, tuning keys :501-506).
This is the only place a manoeuvre changes an outcome. The battery's other launchers (`battery_fire.hpp:11-13`: "no crew
mistakes, no manoeuvre"; chance set at launch, :285) and the testbed's neighbours (`WavePassKillChance`,
test_waves.hpp:623-633: "the pilot does not evade: the testbed is not the air war") ignore it.

---

## 3. The runner's target stepping (`simcore/include/simcore/scenario_runner.hpp`, 12 838 lines; no `runner/` part files exist in this worktree)

### 3.1 Per-target state
- `struct TargetState` (:1924-1937): `spawned`, `gone`, `world_id`, `TargetPilot pilot`, the instructor's hold (`held`,
  `held_heading_rad`, `route_pilot`, `route_waypoints`, `route_next`), `AircraftDamage damage`, `last_position`.
- Parallel per-target vectors: `eng_targets_` (engagement), `rwr_warned_`, `arm_carriers_` (:2823-2828).
- `EnterTarget` (:1940-1954) appends a target once the session runs (instructor, waves, packages).

### 3.2 Order in one `Step(dt)` (:225-…)
1. `UpdateTargets(dt)` (:2019-2117): spawn due targets (`TargetPilot(t.behaviour, t.evade)`, `SetWeapon`, `OnSpawn`,
   `World::AddAircraft`, :2023-2038); each live one: weapon's last-waypoint capture 25 m (:2047), the instructor's
   held heading (:2048-2051), `pilot.BeforeStep`; then `world_.Step(dt)` (:2054); then `pilot.AfterStep` or
   `HoldOverGround` (:2060-2067); a belly under the ground is counted and the aircraft goes down (:2070-2076); a weapon
   strikes (`CruiseImpact`, :2087-2111); past the last waypoint -> `gone` (:2112-2115).
2. `UpdateWaves()` (:2713-…): new waves, loads released (`ReleaseWaveLoad`), `LandWaveLoads`,
   `FireNeighbourDefences` (:2243-2381) with its shots, gun rounds, damage and crashes; the long gone removed.
3. …crew, battery, nav, emission…, `UpdateNeighbourFire(dt)` (:10638, the regiment's other Osa batteries),
4. the engagement step and the **lock cue** (:246-255),
5. `UpdateLauncher()` (:7165) with the **launch cue** (:7206),
6. `CueFromWarnings()` (:2799-2816), `UpdateArms()` (:2835-2903).

Reactions therefore act on the next step.

### 3.3 The cues a pilot gets (all of them)
`CueTarget(id, launch)` (:2929-2940) is the single entry; it calls `pilot.OnLaunch` or `pilot.OnLocked` with
`def_.unit_position`. Its callers:

| Caller | When | Condition |
|---|---|---|
| :254 | our Osa's tracking radar holds it (`TrackMethod::Radar` / `RadarOnJam`) | target has no EM RWR |
| :7206 | our Osa launches at it | target has no EM RWR |
| :2813-2814 (`CueFromWarnings`) | its EM RWR (`EmSensorLink::WarningOf`, em_link.hpp:764-780) hears a lock (a "track" emitter) or the command link of a launch (`missile_guidance`) | EM layer on, target has `rwr` |
| :11037-11041 | the instructor's `Evade` order (`EvadeNow`) | instructor panel |

Not callers (verified): the testbed neighbours' missile launches (:2363-2379 push `SamLaunch` to the feed only), their gun
passes and flak (`StartGunPass` :2446, `FireGunPass` :2482), the regiment's other Osa batteries (`UpdateNeighbourFire`
:10691-10694: an event and an fx launch), the battery's own other launchers (`UpdateBatteryFire` :10836), damage
(`OnDamage`), other aircraft of the flight. R4 records the reason the RWR path misses the neighbours: their radars do
not radiate in the EM layer ("today's known limit", DEVELOPMENT_ROADMAP.md:299).

### 3.4 Rule-based pilot acts outside `TargetPilot`
- **ARM carriers** (`UpdateArms`, :2835-2903): a target with `arms` fires when its RWR hears a radar in its ARM's band
  within ~20 deg (`kArmBearingTolRad = 0.35`) of the briefed aim and within `launch_range_m`/`min_range_m`, every
  `interval_s`; after the last one, if `turn_away`, waypoints are overwritten with a 40 km leg away from the aim and the
  turn rate raised (:2892-2900; source cited: `voenizdat_osa_1991 p262`, the crew's sign of an ARM launch).
- **Damage** (testbed only, §4).
- **The instructor** (`Instruct`, :10984-11048; `TakeTarget` :11251-11283; `ReleaseTarget` :11286-11299): Take, Release,
  Turn (a 50 km leg on a heading), Height (`SetAgl`), Speed, Evade (a placeholder jink 10 deg/s, 4 s, 15 s if none
  defined), Jam (`instructor.hpp:17`). At most two held targets.

Every one of these acts by **overwriting `Aircraft::waypoints`** (or `next_waypoint`). There is no separate command
layer (heading, turn rate, height, speed demanded) between a decision and the kinematics; only the jink and the
instructor's hold save the route to go back to (`TargetPilot::Route`, :134-137).

### 3.5 Weapons release (not a pilot decision)
- Waves: `TestWaves::Releases` (test_waves.hpp:118-161): each `Pending` load (one per carrier, :218-229) is released once
  the carrier is within `release_m` of its aim (horizontal), not before `not_before_s`, given up after 3600 s. No
  re-attack, no second pass, no change of aim. Gun "weapons" exist as loads (`gau8`, `gsh30`, `door_guns`,
  data/air_war/weapons.json:85, :129, :144, kind `gun`, a firing pass with a footprint), released and landed like
  bombs: there is no flown gun run.
- Played raids and injected packages: the mission bridge releases at the route's release point
  (air_war/mission_bridge.hpp:5-14), same one-shot logic.

---

## 4. Damage, flak and their consequences (testbed waves only)

### 4.1 `damage.hpp` (433 lines, roadmap R1, docs/DAMAGE.md)
- `DamageLevel {None, Light, Moderate, Heavy, Destroyed}` (:35); `DamageTable` (:55-87) loaded from
  `data/gameplay/damage.json` with sources.
- `DrawBurst` (:206-219): blast radii `k W^(1/3)`, Poisson fragment strikes `N A/(4 pi d^2) exp(-d/R)`;
  `DrawRoundHits` (:222-227) per calibre.
- `ApplyDamage` (:231-279): three lights make a moderate, two moderates make a heavy; reaching **moderate** draws
  `smoke` 0.6, `jettison` 0.5, `abort` 0.6; reaching **heavy** draws smoke 1.0, `fire` 0.4, jettison 0.9, abort 1.0,
  crash 0.85 at 10-300 s (`crash_at_s`).
- `ApplyMissile` (:285-310): a calibrated kill is destroyed or (25%) heavy; a miss bursts near (50%) 15-60 m off,
  capped at moderate.
- `AircraftDamage` (:101-111) has the flags `jettison`, `abort`, but they are per aircraft and set only by the draw.

### 4.2 `aaa.hpp` (303 lines, roadmap R2)
Fire-control errors per pass (`DrawPassErrors` :103, `Corrected` :115), the aim (`AimGun` :140, a **straight-line**
prediction), contact-round groups (`RoundHitChance` :156, `GroupMiss` :166, `GroupHits` :178), time-fuzed shells
(`TimeShellBurst` :194). Header :9-10: the aircraft's own turns "grow with the square of the time of flight. Nothing here
models them: the round is judged against where the aircraft really is" — so a jinking aircraft is already harder to
hit by the guns, through the geometry alone.

### 4.3 In the runner
- Only in `FireNeighbourDefences` (:2243-2381, gated by `def_.waves` and `def_.waves->has_damage`):
  `EndGunRounds` (:2545-2572) and `EndNeighbourMissile` (:2577-2617) call `OnDamage`.
- `OnDamage(i, change)` (:2621-2661): debrief line, `fx::EventKind::AircraftDamaged`, on `jettison`
  `waves_->Jettison(...)` (test_waves.hpp:166-190: every pending load falls inert from where it is, delivery
  `"jettison"`), on `abort` `a->next_waypoint = a->waypoints.size() - 1` ("straight for its way out", :2660).
- `CrashDamaged` (:2664-2675): the heavy ones fall at `crash_at_s`.
- **Not reached** by played raids or injected packages (no waves): their aircraft are only alive or destroyed in the
  session; the Osa's own resolver still destroys outright (DAMAGE.md §5 "Left").
- No damage changes a flight model (a damaged aircraft keeps its speed and turn rate), and damage never triggers an
  evasion.

### 4.4 The fx feed (`fx/events.hpp`)
`EventKind` (:62-76): MissileLaunch/End, SamLaunch/End, AircraftDown, WeaponRelease/Impact, CruiseImpact,
ArmLaunch/Burst, GunPass, FlakBurst, AircraftDamaged. **The simulation never reads it back** (:6-9). So "a pilot sees
smoke" cannot be built on the feed; it must be built on the runner's own state (e.g. `nb_shots_`, `NeighbourShot`
:2191-2198, the engagement's shots, `nb_rounds_` for shell bursts), which already knows every launch and burst.

---

## 5. Where a session's aircraft come from, and what that gives them

### 5.1 The dynamic campaign testbed (`air_war/test_waves.hpp`)
`TestWaves::Next` (:99-114) -> `EnemyFlight` (:352-555) / `FriendlyFlight` (:557-601):
- one `ScenarioTarget` per aircraft, id `<callsign><n>-<k>` (:394), `formation = n` (:336);
- routes by role (:407-486), e.g. strike/DEAD `entry -> IP (14 km out, +-40 deg) -> aim -> 5 km past -> home` (:412-415);
  SEAD/hunter to a stand-off shot point; escort sweeps; jammers and support on racetracks (`Orbit`, :311-322);
- **straight legs**, no routing round defences, no use of valleys or terrain features beyond terrain following on low
  legs (`Route`, :288-309, `max_climb_mps` 120 jets / 8 helicopters);
- the flight is `Spread` (:343-350): **each aircraft's whole route shifted sideways** (line abreast, 250 m jets, 120 m
  helicopters, 400 m missiles). No lead, no wingman link, no position keeping;
- evasion set per aircraft (:505-510): on `Lock` if the type has an RWR, else on `Launch`; `Dive` for helicopters,
  `Jink` for low, `Break` for higher; turn rate `min(20, 1.5 x max_turn_deg_s)`;
- ours (`FriendlyFlight`): CAP racetracks, intercept, ground attack, transports; `friendly = true`, IFF reply, no
  evasion, no weapons against the enemy, no interaction with enemy aircraft.

### 5.2 Played raids and live packages (`air_war/raid_scenario.hpp`, `package_injector.hpp`)
- `RaidScenarioBuilder::Build` writes one target per surviving aircraft of each flight clipped to the circle
  (raid_scenario.hpp:430-530): the flight's route offset sideways by `scenario.flight_spacing_m` (:441-448),
  terrain following or pop-up (:485-503), the same evade rule (:504-516), RWR and EM ARMs (:517-…).
- `RaidAircraft::flight`/`index` (raid_scenario.hpp:205-236) keep flight membership **in the manifest only** (the
  bridge's outcome, e.g. `aborted_flights`, mission_bridge.hpp:297); the runner does not know it.
- `InjectPackage` (package_injector.hpp:84-131) loads that JSON and calls `ScenarioRunner::AddTargets` (:11078).
- The planner's routes (`Planner::MainRoute`, air_war/planner.hpp:860-914): `entry -> IP (offset 25% sideways) ->
  attack points by delivery (pull-up, apex, roll-in) -> release -> egress 12 km to the side -> exit`, timed to the TOT;
  straight legs. Threat enters only through the choice among the front's entries by expected losses and the officers'
  quirks (planner.hpp:1110-1170) and the weaponeering's `delivery_risk` (planner.hpp:228, data.hpp:438-440).
- The abstract resolver (not the session) has the package leader's **nerve**: turn back after losing a share set by
  boldness and quirks (resolve.hpp:115-121, :250-255) and the defender's fighters as one chance per unescorted package
  (resolve.hpp:231-235, `AircraftType::air_to_air` 0..1 "value as an escort", data.hpp:355). None of this exists in the
  played session.
- `MissionBridge` "launches the enemy sees" (mission_bridge.hpp:84-93) is a distance gate (`scenario.revealed_by_launch_m`)
  for intelligence afterwards; it makes no aircraft react.

### 5.3 Hand-written scenarios (`scenario.hpp`)
`ScenarioTarget` (scenario.hpp:82-125) keys: `start`, `waypoints`, `speed_mps`, `max_turn_rate_deg_s`, `rcs_m2`,
`appear_at_s`, `on_cue`, `class`, `evade_on_launch`, `iff`, `board_type`, `formation` (a count for the PU-12 board,
pu12.hpp:263, command_post.hpp:955), `behaviour`, `evade`, `size_m`, `rwr`, `model`, `decoy`, `arms`, `weapon`,
`bottom_m` (loader :1281-1331, `evade` :1010-1040). Chaff is scripted by time only (`own_unit.em.chaff` {target, at_s,
rcs_m2, lifetime_s}, scenario.hpp:857-869 -> `EmSensorLink` drops it at `at_s`, em_link.hpp:1038-1046); no pilot
dispenses chaff or flares by decision. Flares do not exist.

---

## 6. Per aircraft vs per flight, today

| Thing | Per aircraft | Per flight |
|---|---|---|
| Kinematics, route, speed, turn rate | `Aircraft`, `ScenarioTarget` | — |
| Behaviour, evasion state | `TargetPilot` in `TargetState` | — |
| RWR, ARMs, jammer | `ScenarioTarget::rwr`, `arms`; `ArmCarrierState` | — |
| Load to release | `TestWaves::Pending` (one per carrier) / bridge per aircraft | — |
| Damage, jettison, abort, crash | `AircraftDamage` | — |
| Flight identity | id prefix `<cs><n>-k`; `ScenarioTarget::formation` (count, for the board) | `Wave::flights` log line; `RaidAircraft::flight` in the bridge's manifest |
| Formation geometry | sideways offset of the whole route at spawn (`Spread`) | none kept |
| Leader, nerve, officer aboard | — | `airwar::Flight::officer`, `Package::leader` (abstract resolution only) |
| Comms between aircraft, to HQ | none | none |

---

## 7. Roadmap rows R3 and R4 (`docs/DEVELOPMENT_ROADMAP.md`)

- **"What is there today" (2 Nov 2026, :275-288)** still describes the code: "An aircraft is a point that turns at a fixed
  rate at constant speed. Unreal turns the model to its velocity: it never banks"; "Evasion: only when our Osa launches
  at the aircraft or locks it with a warning receiver aboard. The pilot never reacts to the other sites, to flak, or to
  what he sees." (R1 since added jettison by damage.)
- **R3, Flight with attitude (:298)**, not started: point mass flown by bank-to-turn, `bank = atan(v w / g)` within
  each type's g and roll-rate limits; pitch from path and AoA; speed traded in climbs and lost in hard turns; roll-in
  and roll-out at turn points; helicopters lean nose-down; type limits cited in the air war's aircraft data; simcore holds
  the orientation, the entity stream carries it, Unreal draws it. Done when a hand-calculated coordinated-turn test
  passes and the testbed's waves visibly bank. Python kept in sync. No orientation field exists anywhere for a live
  aircraft; `AircraftType` has `max_turn_deg_s` only (data.hpp:330), no g or roll limits.
- **R4, Defensive reactions (:299)**, not started, "needs R1 and R3", ordered last (:306): reacts to (1) his RWR on
  **any** EM radar, which needs the neighbours' radars to radiate in the EM layer; (2) a launch or flak he **sees**
  (smoke or bursts within his visual range by light and weather); (3) his own damage. By doctrine and threat: break,
  dive, jink, chaff or flares if carried, or press on; jettison tanks or load on a heavy threat, fighters or damage;
  stores fall in the fx feed; a raid that jettisons or aborts counts in the enemy's freedom-of-action ledger (14.6).
  Built of it: only R1's chance-drawn jettison/abort from damage (testbed) and the older 9.3 evasion.
- Milestone M-R (:308-316) expects "aircraft banking and pulling in turns, and breaking away when warned or when they
  see the smoke".

R3/R4 do **not** promise: formations, wingmen, mutual support, re-attacks, gun runs, target changes in flight, HQ
retasking, flight-to-flight or air-ground radio, pilots' visual search, airborne radars, or air-to-air combat.

---

## 8. What would have to change, by capability

1. **Attitude and an energy-limited flight model (R3).** `Aircraft` needs heading/bank/pitch, a load-factor limit and a
   roll rate (turn rate then follows `w = g sqrt(n^2-1)/v`), and a speed that changes (a specific-excess-power or
   thrust-minus-drag term). Hooks: `StepAircraft` (aircraft.hpp:51), `World::Step` (world.hpp:79), the height layer
   (`HoldHeight` adds vz on top of airspeed today), `AircraftType` limits (data.hpp:318-336 plus loader :1346-1362),
   `ScenarioTarget` keys, `MakeState`/`FSimEntityState` (SimWorldSubsystem.cpp:2040, .h:30-60), `AcmiWriter` roll,
   the prototype's `Aircraft.step`. Reusable maths: `fx::WreckFall::Accel` (bank-lift vector). Effect on outcomes:
   `max_turn_deg_s` measured by the engagement (engagement.hpp:1263) would fall for fast aircraft, moving calibrated
   kill totals.
2. **A command layer between pilot and aircraft.** Today every decision rewrites `waypoints`. Formations, defensive
   manoeuvres that return, re-attacks and retasking all need the pilot to hold a plan (route, task, aim) and send
   demands (heading or turn, height, speed, throttle) the flight model follows; `TargetPilot::Route` and the instructor's
   `route_waypoints` are the two existing places that save a route.
3. **Flights as objects.** The runner needs a per-flight state (members, lead, slot geometry, the flight's task and
   plan, its radio). `ScenarioTarget` needs a flight id and slot; `TestWaves::EnemyFlight`/`Spread`
   (test_waves.hpp:352-555, :343-350) and `RaidScenarioBuilder` (raid_scenario.hpp:430-530) would emit them;
   `RaidAircraft::flight` already exists for the bridge.
4. **Triggers (R4).** Extend `CueTarget` (:2929) or replace it with a per-pilot perception step: neighbours' launches
   (:2363-2379, :10691), battery launches (:10836), shell bursts (`nb_rounds_` shells, :2514-2538), our launches,
   damage (`OnDamage` :2621), RWR on neighbour radars (EM emitters for neighbours: R4's prerequisite). Visual: no
   pilot-side model; candidates are `nav::VisualRangeM` (nav/session.hpp:92, `data/gameplay/visual_range.json`,
   by sun and visibility) and `em/optical.hpp` (Johnson criteria) plus `HasLineOfSight` over terrain and obstacles.
   Must use sim state, not the fx feed.
5. **Defensive manoeuvres.** Break/dive must be able to end and rejoin the route or the flight; repeated reactions;
   chaff (the EM layer's `World::AddChaff`, em/world.hpp:301, is ready to be called by a pilot instead of a script);
   no flares or IR seekers exist to make them matter (MANPADS and Strela are abstract kill chances in the testbed).
6. **Jettison.** `TestWaves::Jettison` (test_waves.hpp:166) can be called on a threat as well as on damage; drop tanks
   are not modelled apart from the load; raids/packages need the bridge's equivalent.
7. **Re-attacks, gun runs, target changes.** `Pending` is one-shot with a fixed aim; `Releases` has no notion of a pass
   failing; gun "weapons" are footprints, not passes flown. A new target in flight needs an aim from what the flight
   sees or is told, and a route built in session (the planner's `MainRoute` is a candidate, it is a member function of
   the planner).
8. **HQ and comms.** Nothing in the session speaks for the enemy's aircraft. Retasking would go through a live air war
   (`LivePackages`, package_injector.hpp; `MissionSession`) or a session-side controller; intercepted emissions would
   need the enemy's radios as EM emitters (the EM layer models our radios and the enemy's jammers, not their voice
   nets).
9. **Air-to-air.** Nothing in a session: no AAM (missile.hpp is a demo PN missile with perfect knowledge), no airborne
   radar emitter for fighters, no visual acquisition, no friendly fighter engaging anything. The only air-to-air is the
   abstract resolver's `friendly_fighters` x `attrition.escort_fighter_kill` chance and `AircraftType::air_to_air`.
10. **Damage for raids and the Osa.** `OnDamage` and its jettison/abort run only under `def_.waves`; the Osa's own hits
    and battery fire destroy outright.

---

## 9. Constraints any new work must respect

- simcore: header-only C++17, std only, no exceptions/RTTI, avoid the macro names (`near`, `far`, `small`, `min`,
  `max`, `check`, `verify`, `PI`…; `tests/header_check.cpp`), SI suffixes, ENU (use `LocalFrame::AltitudeOf`),
  warnings as errors; every new model a test (physics against hand calculations: R3's coordinated-turn test).
- Determinism: draws from seeded generators (`defence_rng_` seeded `def_.seed + kDefenceSeed`, :2190, :2246; the
  waves' `rng_`; `fx::WreckFall` seeds so clients agree).
- First person and knowledge: the RWR path already makes a pilot react to what his receiver hears, not the truth
  (scenario.hpp:103-105); new perception should keep that. The fx feed is never read by the simulation (events.hpp:6-9).
- R5: nothing under the ground. Any manoeuvre keeps `GroundFloor`; `TerrainStrikes`, `ground_check data 3600` and
  `test_ground.cpp` must stay at zero.
- Balance: the Osa resolver's and the passes' calibrated kill totals (6.7, `09_BALANCE.md` §9) are the reference;
  `air_war_bench`, `aaa_bench`, `crew_balance`, `helper_coverage` (compare with HEAD's rows), `em_demo`'s 11 checks,
  `test_dynamic_campaign.cpp`. Evasion already lowers the Osa's hit chance (manoeuvre factor).
- Data: every real value sourced in `docs/SOURCES.md` with a tier; game values labelled; placeholder profiles labelled.
- Tests that pin today's behaviour: `simcore/tests/test_threats.cpp` (:375-624: terrain following, strike run, turn
  circle, pop-up, break and dive on lock, jink costs hit chance), `test_damage.cpp`, `test_instructor.cpp`,
  `test_dynamic_campaign.cpp`, `test_ground.cpp`, `test_world_map.cpp`.
- Python mirror kept in sync for any model both implement (today only the aircraft step).

---

## 10. Cold War consistency (the owner's note: "consistent with cold war, not modern tactics")

What the code already ties to the period:
- The war has a year: `"year": 1985` (data/air_war/theatre_pannonia.json:12). Types fly from their
  `in_service_year` (data.hpp:357; e.g. faction_nato.json:148/152, cruise missiles 1986 and 1988), ARMs by year
  (`arm_em_by_year`, data.hpp:343-345: "The F-4G fired the AGM-78 Standard ARM until the HARM reached the USAF (1987)").
- Command style per faction: `DoctrineDef::centralisation` (data.hpp:433) is 0.9 for the Warsaw Pact, 0.6 NATO, 0.5
  the neutral, 0.2 the rebels (faction_*.json); `simultaneous_bias` likewise. Officers carry traits (boldness,
  rigidity, composure…) and skills (airmanship, leadership) (data.hpp:524-541).

What this means for the requested features (flags for the research and design, not sourced data):
- Anything new that is equipment (air-to-air missiles, fighter radars, RWRs, chaff/flare dispensers, datalinks, pods)
  must be typed with its own `in_service_year` and gated by `TheatreDef::year` as the aircraft and ARMs are, so a 1985
  war cannot field later kit.
- Period mechanisms, not modern ones: control by **voice radio** from ground controllers (GCI), airborne early warning
  where a faction had it (the air war already flies AWACS orbits, docs/air_war/11_SUPPORT.md), and briefed plans;
  "HQ vectors SEAD onto a launch area" should be a voice tasking through such a controller or a hunter-killer flight
  already on station, with the delays and errors of voice and plotting, not a shared digital picture or a
  network-wide retask. The highly centralised Pact side would retask from the ground and its pilots would deviate
  less; NATO's doctrine `centralisation` 0.6 leaves more to flight leads.
- Avoid importing post-1990 practice: fire-and-forget radar AAMs as the norm, fighter datalinks sharing tracks,
  satellite navigation, precision-everything strike, "kill box" style dynamic targeting. Beyond-visual-range shots in
  the period were semi-active radar missiles constrained by identification; the project's own Bekaa 1982 battle is a
  period reference for controlled air combat. All of this to be sourced (tiers in docs/SOURCES.md) before it becomes
  data.
- Existing comments to watch: the `Delivery::Loft` comment cites "Tornado 1991" as an example (types.hpp:243); the
  technique is period, the example is not. The waves' choice of escort/sweep roles and the planner's packages are
  already 1985-gated by type.
