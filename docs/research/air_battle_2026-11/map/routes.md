# Map: how the enemy's raids are routed and turned into aircraft today

Read-only map of `C:\dev\SAM3D-wt\air-battle` (branch `docs/air-battle`, clean, at `1d09489`) for the owner's request of
15 Nov 2026 (the enemy's air war in its hot phase: routes by doctrine, live retasking, pilots' vision, live comms,
air-to-air). The owner's added instruction: **keep it consistent with the Cold War (the war's year is 1985), not
modern tactics.** Section 10 covers that.

Paths are under `simcore/include/simcore/` unless they start with `docs/`, `data/` or `simcore/tests/`.

---

## 1. The answers in brief

| Question | Answer |
|---|---|
| Why are routes straight today? | Every route comes from one of four fixed shapes in `air_war/planner.hpp`: `MainRoute` (entry, IP, the attack's points, target, egress, exit), `LoiterRoute`, `OrbitRoute` and `MissileRoute`. Each is a few straight legs computed from one entry point and the target by vector arithmetic. The only lateral choice is which of the front's 3-4 entry points to use. The exit is always the entry (`o.exit = ei`, planner.hpp:1083). The IP and egress offsets always go to the same (left) side. No terrain is sampled, and no path search exists. In the session, `StepAircraft` (aircraft.hpp:51) flies straight at constant speed between waypoints. `TargetPilot` only adds height (terrain following), pop-ups and evasion (target_behaviour.hpp). |
| What do threats and terrain contribute when routing? | **Threats only score a route; they never shape it.** `ExpectedLosses` (planner.hpp:981) walks every believed threat (`IntelPicture::Threats`, intel.hpp:174) along each flight's polyline (`RouteChord`, a ring widened by half the position's sigma). It also sizes SEAD by the radar threats on the route (planner.hpp:685-692). **Terrain contributes nothing.** `PlanContext` (planner.hpp:39-73) has no ground. A low route is a single height above ground (`ProfileHeight`). Masking is one scalar for long-range SAMs (`attrition.low_vs_long_range_sam` 0.35 in `PassKillChance`, planner.hpp:104). **Our fighters are one number per front** (`FrontDef::friendly_fighters`, data.hpp:158). That number costs a package the same wherever it flies (planner.hpp:1029-1030). |
| Does the flight and package structure survive into the session? | **Only in the bridge's manifest.** `RaidScenarioBuilder::Build` writes one independent `ScenarioTarget` per aircraft (raid_scenario.hpp:446-587). Each aircraft is offset sideways on the same waypoints and carries a `formation` count that only the PU-12's board reads (pu12.hpp:263, 296). `RaidAircraft` keeps `flight`, `index`, `role` and `officer` (raid_scenario.hpp:206-236), so `MissionBridge` can roll results up per flight. Nothing in the runner flies or decides as a flight or package: there is no leader or wingman, no mutual support, no package commander, and no shared abort. Timing survives only as each aircraft's `appear_at_s` and speed. |
| Where would a route-planning step plug in? | `Planner::Build`, at its seven `MainRoute` and `OrbitRoute` calls (planner.hpp:676, 721, 749, 768/770, 790, 808, 830). Add a route-variant dimension to the option loop of `BestPackage` (planner.hpp:1072-1180) and to `TacticKey` (officers.hpp:35). `PlanContext` needs the ground (`AirWarData::country`, data.hpp:2079) and a believed fighter picture. Downstream code already takes any polyline: the resolver, the raid builder's clipping and the injector. A few places assume `MainRoute`'s shape (section 11). |
| What do determinism and save/replay require? | A save is the data files, the seed and the player's actions (`save.hpp`). Loading re-runs the planner and the resolver from the seed (save.hpp:302-368). The planner and resolver must stay pure functions of the campaign state and its one `Rng` stream (types.hpp:395). Any extra draw shifts everything after it, so use `Rng::Fork`. Anything a played session decides that changes the campaign must come back inside the saved `PackageOutcome` or as a new recorded `CampaignAction` (campaign.hpp:75-85). A changed planner makes existing saves diverge (section 12). |

---

## 2. The vocabulary (types.hpp, state.hpp, data.hpp)

- `RoutePoint` (state.hpp:18-23): `pos` (theatre ENU, z unused), `agl_m`, `t_h` (campaign hour), `tag` (`entry`, `ip`,
  `hide`, `pull_up`, `apex`, `roll_in`, `target`, `egress`, `exit`, `orbit`, `loiter`, `launch`, `dive`).
  A route has **one height per point above the ground there** and no speed per leg. Time comes from one speed
  (`TimeRoute`).
- `Flight` (state.hpp:25-48): `callsign`, `role` (`FlightRole`), `squadron`, `aircraft`, `count` (2-4), `profile`
  (`Low` / `Medium` / `High`), `route`, `release_index` (the index tagged `target`), `aim_key`/`aim_pos` (the
  *believed* aim), `officer` (a staff officer in the lead aircraft), `arms`, `weapon`/`weapons`/`delivery`. Results:
  `lost`, `damaged`, `aborted`, `released`.
- `Package` (state.hpp:50-125): `kind` (`MissionKind`), `target_key`/`target_object`/`target_pos` (believed),
  `tot_h`, `part`, `profile`, `entry`, `exit`, `flights`, `planner`, `commander`, `leader` (the officer who leads in the
  air), `expected_losses`, `expected_value`, `utility`, `coordination` (SEAD/strike timing), `load_factor`, `refuelled`,
  `interference`, `tactic` (`TacticKey`), `rationale`, `resolved`, `played`, `found`.
  **There is no package commander as a decision-maker in the air**: `leader` is only used for the abstract nerve and
  airmanship (resolve.hpp:114-122, 59-62) and the officer-loss bookkeeping.
- `FlightRole` (types.hpp:177-193): Strike, Dead, Sead, Hunter, Escort, Jammer, Recce, Decoy, Helicopter, Missile, plus
  the support roles Awacs, Tanker, Elint (orbits by the day, never in a package: `docs/air_war/11_SUPPORT.md`).
- `Profile` (types.hpp:227), `Delivery` (types.hpp:241-250: Laydown, Loft, PopUp, Dive, Level, Standoff, HoverFire).
- `EntryPoint` (data.hpp:146-149: `id`, `pos`), `FrontDef` (data.hpp:152-165): `line`, `entries`, `bases` (one point
  for all the enemy's airfields), `friendly_fighters` (0..1), `battery_position`, `missile_launch`, `force_scale`.
  Data: the tuned theatre's fronts have 3-4 entries each (`S1-3`, `SW1-3`, `N1-4`, `W1-4`); friendly_fighters 0.15-0.4.
- `AircraftType` (data.hpp:318-385): `cruise_mps`, `attack_mps`, `low_agl_m`, `medium_alt_m`, `max_turn_deg_s`,
  `night`, `all_weather`, `rwr_em`, `self_jammer_em`, `standoff_jammer_em`, ARMs, `loads`, `designator`, `computing`,
  `radius_m`, `recce_quality`, **`air_to_air` (loaded, used nowhere)**, `evasion`, `in_service_year`.
- `PackageTemplate` (data.hpp:401-417): `main_flights`, `flight_size`, `sead_per_threat`, `sead_max`, `escort_ratio`,
  `jammers`, `escort_jamming`, `decoys`, **`chaff` (loaded, used nowhere)**, `recce_lead`, `profiles`, `day_parts`,
  `aircraft`.
- `DoctrineDef` (data.hpp:429-449): `planning_cycle_h`, `lead_time_h`, **`on_call_fraction`** (subtracted from the
  tempo at planner.hpp:297; those sorties are never flown), `centralisation`, `simultaneous_bias`, `delivery_risk`.
  Data: NATO cycle 24 h / lead 12 h / centralisation 0.6; Pact 12 h / 3 h / 0.9 / on-call 0.2; neutral and rebels are
  lighter.
- `IntelEntry` (intel.hpp:27-65): what the enemy believes about one object, including the threat it poses:
  `threat_radius_m`, `threat_lethality`, `threat_low`/`threat_medium`, `threat_radar`, `threat_channels`,
  `threat_max_target_mps`, with `pos`, `sigma_m`, `exists`, `function`, `suspected_decoy`; `Live()`.

---

## 3. The planner: how a package and its routes are made (planner.hpp)

### 3.1 The cycle
`AirCampaign::Plan` (campaign.hpp:758-810) runs every `planning_cycle_h` and builds a `PlanContext`
(planner.hpp:39-73). That context has the data, staff, intel picture, squadrons, politics, daily weather, `now_h`, phase,
ARM and missile stocks, `rng`, the journal, the truth (used only for object existence), the AWACS share,
`tanker_receivers` and the supply network. **It has no ground or terrain, no picture of our fighters (only
`FriendlyFighters(front)`, planner.hpp:63-65) and no airspace.**

`Planner::Plan` (planner.hpp:269-385) sets the tempo and apportionment, then fills mission kinds in a fixed order
(planner.hpp:338-341). For each kind it calls `BestPackage`, or `BestMissilePackage` for cruise missiles, and then
`Approve` and `Commit`. It may add a `DoubleTap`, and finishes with `Interference`.

### 3.2 The options it weighs (BestPackage, planner.hpp:1037-1189)
For each candidate (`Candidates`, planner.hpp:463-531) it tries every entry point (`front_->entries`), every profile in
the template (Low is dropped when politics sets `min_profile`) and every day part in the template (`NextTot`, the next
such time in the window). Sized kinds also try a size of 0.5, 1 or 2 times the template (Strike, Dead and Cas, when
`utility.sortie_cost > 0`). The `Option` struct (planner.hpp:571-578) is exactly `entry, exit, profile, part, tot_h,
size`. **There is no route variant.** `o.exit = o.entry` always (planner.hpp:1082-1083).

Each option is built (`Build`), priced (`ExpectedLosses`) and valued (`ValueOf`, `p_loc` for a mobile target's
uncertainty, `p_deliver`, `WeaponsEffect`). It then scores utility with quirks (time of day, `axis_repeat` /
`axis_vary` against the last entry, low or medium, weather, imagery wait, restrike, helicopters), tactic memory
(`pln.tactics[TacticKey]`), exploration and **one `rng->Normal` draw per option** (planner.hpp:1152). The best option
wins.

### 3.3 Build: the package's flights and their routes (planner.hpp:592-858)
| Flight | Route call | Timing against the ToT | Height |
|---|---|---|---|
| Main flights (`main_flights` × mass × size) | `MainRoute(entry, exit, e.pos, …, ip_dist 20 km or 6 km for helos, release_short, delivery)` (676); hunters use `LoiterRoute` (674) | `+ (1 − simultaneous_bias) × 0.05 h × i` per flight (672) | `ProfileHeight(type, profile)` |
| SEAD (`sead_per_threat` × radar threats on the route) | `MainRoute` to the **same target**, over it (721) | −0.05 h | Low if the package is low, else Medium |
| Escort | `MainRoute`, the strike's own path (749): a close escort, never a sweep or barrier | same ToT | Medium |
| Jammers | `MainRoute` if `escort_jamming`, else `OrbitRoute` (768/770) | orbit anchored at −0.15 h | Medium |
| Decoys | `MainRoute` (790) | −0.07 h | Medium |
| Recce lead | `MainRoute` (808) | −0.13 h | the package's |
| Feint (planner quirk) | `MainRoute` from the *next* entry to 75 % of the way to the target, out the same way (830) | −0.2 h | Medium |

`radar_threats` (planner.hpp:685-692) counts believed radar threats whose ring the **first** main flight's route
crosses. It sizes SEAD and decides whether jammers and decoys come at all.

The reach check (planner.hpp:838-856) measures distance as `Dist2d(entry, target) + Dist2d(bases, entry)`. It is a
straight-line radius check that ignores the route's own length.

### 3.4 The route shapes
- **`MainRoute`** (planner.hpp:863-914):
  ```cpp
  const Vec3 dir = Vec3{in.x / len, in.y / len, 0.0};   // entry -> target
  const Vec3 side{-dir.y, dir.x, 0.0};                    // always the left
  RoutePoint ip{target - dir * ipd + side * (0.25 * ipd), …, "ip"};
  …  // attack points by delivery: pull_up/apex (PopUp), pull_up (Loft), roll_in (Dive)
  const Vec3 out = rs > 1500.0 ? tg.pos + side * 12000.0 - dir * 2000.0 : target + side * 12000.0 + dir * 4000.0;
  TimeRoute(r, 2 + attack.size(), tot_h, helicopter_hide ? a.cruise_mps : a.attack_mps);
  ```
  The route is entry, IP (20 km short, a quarter of that to the left), the attack points, target, egress (12 km to the
  left) and exit (the same entry point). The whole route is timed at `attack_mps`, ingress included, and the anchor
  index `2 + attack.size()` is hard-coded.
- **`LoiterRoute`** (planner.hpp:917-929): entry, then four points on a 9 km circle around the believed site (the first
  tagged `target`), then exit. It is timed from index 1.
- **`OrbitRoute`** (planner.hpp:932-945): from the front's `bases`, a three-point racetrack 5 km behind the entry,
  24 km wide across the axis, back to `bases`.
- **`MissileRoute`** (planner.hpp:1209-1221): the launch area (sea-launched) or the entry, a dive point 15 km short when
  High, and the target at 10 m.
- **`TimeRoute`** (planner.hpp:85-91): one speed for every leg, anchored at one index.

### 3.5 How threats, terrain and fighters enter (they price; they do not steer)
- `ExpectedLosses` (planner.hpp:981-1035): per flight and believed threat, it skips threats too slow for the type. It
  computes `ring = threat_radius_m + 0.5 σ`, `spread = (r/ring)²` and `chord = RouteChord(route, pos, ring)`
  (planner.hpp:953-957, the sum of `ChordInCircle2d` over every leg, types.hpp:469). It then applies
  `PassKillChance` (planner.hpp:95-114: exposure = chord / 2r, the altitude band, SEAD, jamming, environment and
  evasion), times `Live()` and the fire channels' share. The target's own guns (`TargetGunsChance`) come on top for the
  attackers. Our fighters add `FriendlyFighters × attrition.escort_fighter_kill (0.03) × (escort ? 0.3 : 1) × Main()`,
  which is independent of the route.
- **Terrain**: none. The planner never asks for a height. A Low flight's "masking" is
  `attrition.low_vs_long_range_sam` for threats with a radius above 15 km (planner.hpp:101-104) plus each threat's
  `threat_low` flag. `AirWarData::country` (data.hpp:2079, a `CountryMap` with `GroundHeight(e, n)` at 1 km,
  country_map.hpp:245) is loaded only on the nation's theatre, and only the supply lines use it
  (air_war/logistics.hpp:87-93, 261-278). The tuned theatre (`theatre_pannonia.json`) has no ground at all for the
  planner.
- **Our fighters**: `FrontDef::friendly_fighters` is static data. I found no code that lowers it when our air bases are
  struck, although `docs/campaign/02_WHAT_WE_DEFEND.md` says the air bases' role is that.

So the planner already *evaluates* any polyline correctly. It only *generates* a handful of fixed shapes, and the
cheapest is chosen among 3-4 entries × profiles × parts × sizes.

### 3.6 After the choice
- `Approve` (planner.hpp:1305-1327): expected loss per aircraft against the commander's limit, and the political veto.
- `Commit` (planner.hpp:1329-1379) does three things. It shifts the whole package by
  `rng->Normal(0, jitter)` for punctuality, using `centralisation` and the planner's `punctuality` quirk. It puts the
  squadron leader (`AirStaff::LeaderOf`) in the first Strike, Dead, Hunter, Helicopter or Sead flight
  (`fl.officer`, `p.leader`). It spends sorties, ARMs, tanker capacity and missiles.
- `DoubleTap` (planner.hpp:1381-1404) adds half a strike flight 0.6 h later. `Interference` (planner.hpp:1406-1433)
  moves the most visible strike 1-3 h earlier, halves `coordination` and sets `load_factor` 0.6.

---

## 4. The abstract resolution (resolve.hpp)

- **When**: `AirCampaign::StepTo` (campaign.hpp:651-698) resolves a whole package **at the step in which its first
  aircraft takes off** (`p.FirstH() < t1`, campaign.hpp:668-679). It is resolved in one call, route and all, unless the
  package is offered to the player. **Packages never overlap or interact in time**, and none can be diverted once
  resolved.
- **How** (`Resolver::Resolve`, resolve.hpp:103-270): flights are sorted by their `target`/`orbit` time
  (`TargetTime`, resolve.hpp:275-280). For each route leg, and each piece not flown in a played session
  (`ResolvedPieces`, resolve.hpp:72-99), every real defence takes a pass. The pass uses the chord through its true
  circle and `PassKillChance` with true values (resolve.hpp:152-229). The pass charges emissions, fires SEAD and hunter
  ARMs (`FireArms`, resolve.hpp:319-353), draws decoys' fire, and draws a loss or damage per engaged aircraft
  (`EngagedAircraft`).
- **Fighters**: one roll per main flight at `si == 1`, the leg after the IP in `MainRoute`'s shape (resolve.hpp:232-248).
  This is the whole air-to-air model.
- **The leader's nerve**: one package-wide threshold, `0.25 + 0.3 × boldness` plus quirks (resolve.hpp:117-122). Once
  the losses reach it, every main flight before its release turns back (resolve.hpp:145-148, 250-255).
- **Release**: `Deliver` (resolve.hpp:355-492) finds what is really near `aim_pos` and resolves the weapons. An armed
  reconnaissance first searches its stretch (`FindColumn`, resolve.hpp:285-310): the only "find and attack what you
  see" behaviour in the code. `Photograph` covers recce along the polyline (resolve.hpp:494-508).
- **On call**: `AimOnCall` (campaign.hpp:836-844) aims CAS and helicopter raids at the troops' true position plus
  ±300 m, representing the FAC talking them on. It is the only retargeting at execution time.

---

## 5. A raid as the player's scenario (raid_scenario.hpp)

`RaidScenarioBuilder::Build(campaign, package, format)` (raid_scenario.hpp:283-967):
1. **Clip** (raid_scenario.hpp:341-400): each flight's polyline is moved to scenario ENU through the geodetic. A run at
   the battery or a dummy moves with it (`moved_by`). Only the part inside `scenario.clip_m` (70 km) is kept, with
   points where it crosses the circle (`edge`). The release point is kept if inside. Any number of waypoints is
   handled.
2. **Order and cap** (raid_scenario.hpp:408-439): flights are ranked main, SEAD, decoys, recce, jammers, escorts and cut
   at `scenario.max_targets` (24). Cut flights get `flight_in = false` and are resolved abstractly.
3. **One target per surviving aircraft** (raid_scenario.hpp:446-587):
   - The id is `<callsign>-<k>`, with a name like "Callsign 1: type (role)".
   - The start and every waypoint are shifted **sideways by `(k − (n−1)/2) × 800 m`** (`scenario.flight_spacing_m`)
     at right angles to the *first clipped leg* (441-448). The same offset applies to every waypoint, so in a turn the
     line abreast is not kept.
   - Heights: low flights get `agl_m` per waypoint and a `terrain_following` behaviour (agl, `max_climb_mps` 120 for
     jets and 8 for helicopters, lookahead 6 s). Medium and high flights get `up_m` = the route's agl over the frame's
     origin height.
   - `speed_mps` = the clipped length / the clipped time, clamped (475-478): one speed for the whole clipped route.
   - `appear_at_s` = when the route crosses the circle.
   - `formation` = the aircraft alive in the flight (484), used only for the board's marker.
   - Helicopters with a `hide` get the `pop_up` behaviour (485-495).
   - Evasion (504-516) triggers on `lock` with an RWR, otherwise on `launch`: a jink when low, a break otherwise, and
     a dive for helicopters.
   - SEAD and hunter types with an RWR and an `arm_em` carry EM ARMs aimed at the flight's believed aim (518-538).
   - Jammers go on their aircraft (543-554).
   - `RaidAircraft` (555-584) keeps flight index, `index`, role, release point and aim, weapons and delivery, `officer`
     (k == 0 of the leader's flight), speed and evasion.
4. **The world around**: the battery, dummies, warning nets, regiment radars, neighbours that fight or roll
   (raid_scenario.hpp:637-848), the air-raid warning trigger, and the pipeline's messages with the raid's azimuth from
   the first clipped point (851-911).

**The flattening is complete in the runner**: `ScenarioTarget` (scenario.hpp:82-125) has no flight, leader or role
field.

---

## 6. In the session: how the aircraft fly and react

- **Flight model**: `StepAircraft` (aircraft.hpp:51-73) moves at constant airspeed with turn-rate-limited steering to
  the next waypoint, and wind. `WaypointInsideTurn` skips unflyable turn points. There is no bank, pitch, energy or
  g-limit; track R3 plans attitude. The prototype mirrors this `Aircraft` in `prototype/samproto/engagement.py:18-62`.
- **The pilot** (`TargetPilot`, target_behaviour.hpp:70-358):
  - Terrain following looks ahead `lookahead_s` and clears obstacles (`HoldHeight`, 295-336), with a ground floor 3 s
    ahead (`GroundFloor`, 255-288).
  - The helicopter pop-up has hidden and exposed cycles.
  - Evasion runs once, `OnLaunch`/`OnLocked` (116-122): break onto the beam with a 30 km leg (no return to the route),
    dive, or jink for `evade_s` and then back to the route (185-245).
  - **Terrain avoidance in the horizontal does not exist**: the pilot climbs over the ground. Terrain masking emerges
    only from line of sight in the sensors.
- **Damage, jettison and abort** (`ScenarioRunner::OnDamage`, scenario_runner.hpp:2621-2661): from moderate damage a
  target may jettison or "turn for home" (`a->next_waypoint = last`, 2660). These paths are fed only by the
  testbed's defences (`FireNeighbourDefences`, `EndGunRounds`, `EndNeighbourMissile`), which return early without
  `def_.waves` (2243-2244, 2546, 2586). **A played or injected raid has no damage states: an aircraft is alive or
  destroyed** (R1's "Left: the Osa's own hits").
- **Mid-session control precedent**: the trainer's instructor (`ScenarioRunner::Instruct`, scenario_runner.hpp:10984;
  `TakeTarget`/`ReleaseTarget`, 11251-11297) takes up to two targets. It swaps their `TargetPilot`, stores and restores
  their waypoints, and flies a heading, height and speed. There is **no public API to give a target new waypoints**.
- **The bridge** (`MissionBridge::Step`, mission_bridge.hpp:70-195) reads the runner each step:
  - releases at the right throw (`Releases`) and ARMs (gameplay or EM);
  - the neighbours' passes, each decided **once at zone entry on the present heading** (`Engage`,
    mission_bridge.hpp:539-…). A later reroute does not change the chance.
  - "launch seen": any enemy aircraft within `scenario.revealed_by_launch_m` (15 km) when we launch (84-94). This is
    the only in-session "pilot's vision", and it only feeds the post-raid intel (`fired`).
  - `Finish` (238-328) rolls losses up per flight. It marks `released_flights` and `aborted_flights` (a flight with a
    release that is alive and did not release).
- **After the session**: `CompletePlayerMission` (campaign.hpp:199-249) applies the outcome. It resolves every other
  piece of the routes abstractly **on the planned polyline**, outside the 70 km circle or outside the session's hours.

---

## 7. Packages entering a running session (package_injector.hpp)

- `InjectPackage` (package_injector.hpp:84-154) takes the raid builder's JSON through `LoadScenario`. It moves targets,
  waypoints and ARM aims into the session's frame (`MoveFrame`, 48), shifts `appear_at_s` by `at_s`, and calls
  `ScenarioRunner::AddTargets` (scenario_runner.hpp:11078-11101), `AddEmGear` and `AddTriggers`. Pipeline messages are
  not played; the notes say so. `MoveManifest` (52-79) puts our vehicle and battery where the session has them.
- `LivePackages` (package_injector.hpp:162-299) advances the campaign every `every_s` (30 s) to the session's clock.
  When `Advance` hands it the player's package, it builds the raid, injects it, jams the battery net
  (`JamTheBatteryNet`, 261-287) and creates a `MissionBridge`. When the bridge is `Over` and the loads have landed, it
  calls `CompletePlayerMission`.
  - **One package at a time**: while a bridge is active, `Step` returns before `Advance` (181-203). **The whole air
    war's clock stands still** until that package is over. Every other package due meanwhile, anywhere, is resolved
    afterwards in one go.
  - Used by `campaign/mission_session.hpp:91-92` (`mission_demo --war`).

---

## 8. The testbed's waves (test_waves.hpp)

The dynamic campaign test generates random waves for a session. They are not the air war (header lines 1-16).
`EnemyFlight` (352-555) draws a type and a role from a deck and an entry bearing toward a front entry ±8°
(`EntryBearing`, 267-273). Routes are straight legs:
- strike/DEAD: entry, IP 14 km from the aim at ±40°, the aim, 5 km past, home; low or (40 %) medium;
- SEAD: 24 km stand-off (hunter 14 km), then 15 km to the side;
- escort: a sweep over the centre;
- missiles; jammer and support racetracks.

`FriendlyFlight` (557-601) flies our MiG-21 and MiG-23ML CAP and intercept routes as orbits and lines
(`data/gameplay/test_waves.json`). **No air-to-air engagement exists.** The defences use `WavePassKillChance` (632-647)
with no suppression, jamming or evasion. Randomness is `std::mt19937` **with `std::uniform_*_distribution` and
`std::shuffle`** (236-243, 357, 560, 609). That is not portable across standard libraries; types.hpp:393-394 warns
against it.

---

## 9. The request's items against the code

| Requested | Exists | Where / what is missing |
|---|---|---|
| Routes by doctrine round air defences, along terrain, away from fighters | No | Fixed shapes (§3.4); threats only price (§3.5); no ground in planning; fighters a scalar |
| Pilots react to a missile's smoke, tell the flight, evade, jettison | Partly | Evasion per aircraft on our launch or lock (`TargetPilot`); jettison and abort only in the testbed (`OnDamage`); nobody tells anybody; R4 in `docs/DEVELOPMENT_ROADMAP.md` (track R) is the roadmap's slot |
| A flight radios HQ; HQ vectors SEAD or strikers onto the launch area | No | Launch sightings exist only as a post-raid intel report (`BeliefsFrom`, campaign.hpp:970-…, `IntelSource::LaunchSighting`, delayed `intel.pirep_delay_h`); `on_call_fraction` reserves sorties never flown (planner.hpp:297); `AimOnCall` is the only execution-time retarget |
| A target found en route: nothing, an impromptu attack, a gun run, a recce pass | Only armed recce | `Resolver::FindColumn` (resolve.hpp:285-310) on its own stretch of road or rail |
| HQ changes routes and targets on the fly | No | Packages are resolved whole at take-off (campaign.hpp:664-680); in a session the campaign clock stands still (§7); no runner API to re-route a target |
| Pilots' vision and sensors | Minimal | The RWR as an EM ESM (`ScenarioTarget::rwr`; scenario.hpp:103-105); `revealed_by_launch_m`; recce corridor in `Photograph`; no visual model for aircrew |
| Live comms between flights, HQ and squadrons, interceptable | No (for the enemy's air nets) | Our side has nets, intercept and DF in the EM layer (`em/comms_system.hpp` header 1-17), our voice nets jammed by the enemy (`air_war/comms_jamming.hpp`), our calls heard by its intercept (`CampaignAction::Transmit`, campaign.hpp:69-84). The enemy's own air-ground and inter-flight nets are not modelled |
| Types that favour or avoid rapid retasking | No | Officer traits (`Traits`, data.hpp:524-533: boldness, methodical, creativity, rigidity, composure...) and doctrine `centralisation` exist to hang it on |
| Air-to-air: missiles, radars, IR, visual, BVR, dogfights, GCI | No | Escort and friendly fighters are scalars; `AircraftType::air_to_air` unused; our fighters fly only in the testbed's waves, unarmed |
| Factions fight as they did, with traits, flaws and skills | Planning only | Doctrine per faction, officers with traits, skills, quirks and tactic memory; nothing acts on them in the air except the abstract nerve and airmanship |

---

## 10. Cold War consistency (1985): what holds, and what the new work must not assume

### 10.1 What the code and data already keep to the period
- Only what is in service in the war's year flies: `AircraftType::InService`, `WeaponDef::InService` and `ArmEm(year)`.
  So the F-4G fires the AGM-78 class until 1987 and the HARM after (`docs/air_war/05_EXECUTION.md` §4). There are no
  Tomahawk (1986) or CALCM (1988) in a 1985 war, even though NATO's missile template lists `tlam_c` and `agm86c`
  (`MissileType`, planner.hpp:1193-1204). The A-50 is from 1984-85 (`docs/air_war/11_SUPPORT.md`).
- Period structures: the Pact's tactical groups and NATO's package roles (`FlightRole`). Wild Weasel hunter-killers
  (`Hunter`). The free hunt as armed reconnaissance. Massed and successive blows (`simultaneous_bias`). Strikes held
  for on-call (`on_call_fraction`). The Pact's centralisation 0.9 and 12 h cycle against NATO's 0.6 and 24 h.
- The repo's own research gives the period doctrine that the new routing should follow
  (`docs/research/ENEMY_AIR_RESEARCH_2026-10.md` §, from `mai_tactics_vvs` §3.3.3, tier 1):
  - strike groups approach at low and extremely low altitude using the terrain;
  - **"the route is the shortest, with a variable profile, bypassing air defences"**, and the strike itself is short;
  - control passes to forward air controllers with the troops at set lines;
  - successive strikes **on call** from ground or airborne alert against targets found during the fighting;
  - the free hunt by up to a flight;
  - at night, a column of pairs at different heights.

  `cyberleninka_airfields` adds dense formations through corridors where the air defences have been suppressed.
  `dtic_linebacker` adds that rigid, centrally fixed, repeated routes cost B-52s until routes were varied.

### 10.2 Post-period sources the calibration leans on (effects, not tactics)
Several tuning notes calibrate *effects* on 1991 and 1999 records:
- `rjlee_ds_losses` (Desert Storm's low against medium altitude), `TargetGunsChance`'s comment (planner.hpp:129-130);
- `gwaps_vol3` for tanker receivers (`data/gameplay/air_war.json` support notes);
- `rand_mr1365` (Allied Force HARMs) for `arm_kill_memory_off`;
- the `Loft` comment "Tornado 1991" (types.hpp:243).

`docs/research/AIR_WAR_MISSIONS_2026-10.md` §2 calls Desert Storm's switch to medium altitude "the period's lesson".
**For the new behaviour these must stay calibration of outcomes only.** The 1985 central-front tactics they followed
(medium-altitude bombing above the guns as NATO's default, precision stand-off as the norm, SEAD by HARM at range)
are post-1985 and should not be adopted. NATO in 1985 planned low-level penetration by terrain-masked routes, which
the current `profiles` lists allow (NATO strike `["low", "medium"]`, DEAD `["medium", "low"]`).

### 10.3 Things a 1985 design must not assume (general period knowledge: **to be confirmed and cited in a research pass before any data**, per CLAUDE.md's realism rules)
- **Navigation and route planning**: no GPS for tactical aviation. Routes were planned on paper maps by the squadron
  (mission planning cells) and flown by INS or Doppler, map and stopwatch, with radar and visual fixes. Turn points and
  IPs therefore sit on recognisable features (rivers, lakes, towns, road and rail junctions, ridges), and navigation
  error grows between fixes. A route planner should **snap turn points to features of `CountryMap`** (rivers,
  settlements, landmarks, roads: country_map.hpp) and avoid inventing arbitrary geometric doglegs.
- **Retasking in the air**: no tactical datalink on the strike force. Link 16 and JTIDS were not in general service;
  retasking was by voice. On NATO's side: AWACS or the CRC, an airborne command post, the FAC. On the Pact's side: the
  command post's ground control, the regiment's KP, the forward air controller with the troops, and strict radio
  procedure.
  - Voice nets can be jammed, take seconds per message and **radiate**: our intercept and DF can hear them
    (`em/comms_system.hpp` already models nets, latency, jamming and intercept).
  - Retasking applied mostly to **on-call and alert forces and to CAS under a FAC**, not to pre-planned deep strikes.
    The pre-planned force flew its plan; the Pact's was more rigid than NATO's.
- **What HQ could know in the air**: no real-time video or ground-moving-target radar. JSTARS was a development
  programme. Remotely piloted vehicles existed (the Israelis over the Bekaa in 1982, which the repo already cites)
  but did not feed fighter-bombers live. New targets en route come from:
  - the aircrew's eyes and their RWR;
  - a FAC;
  - ELINT (stand-off, `IntelDef` delays);
  - reconnaissance film after processing (`recce_latency_h` 4 h).
- **Air-to-air**: BVR missiles of the period are semi-active radar types (AIM-7F/M and Sky Flash on NATO's side;
  R-23/R-24 on the MiG-23). Rules of engagement and IFF limited BVR shots, and many engagements closed to visual
  range with IR missiles (AIM-9L; R-60) and guns.
  - Active-radar fire-and-forget missiles (AIM-120) came after the Cold War.
  - Pact and our side's interceptors (MiG-21, MiG-23ML in `test_waves.json`) fought under close **ground-controlled
    interception (GCI)**, with little freedom of action.
  - Our side's fighters should be vectored by our own radars and command posts through the pipeline, not
    free-roaming.
- **Self-protection**: chaff and flares, ECM pods, RWRs of the period. The `chaff` template flag exists unused; the EM
  layer has self-protection jammers and RWRs.
- **Night and weather**: few types could attack at night (the `night` flag: F-111F and Tornado yes, A-10 no), which
  the planner already enforces (`PickSquadronOf`, `CanDeliver`).

---

## 11. Where a route-planning step plugs in (and what it must fix on the way)

1. **Generate**: a `RoutePlanner` that `Planner::Build` calls in place of `MainRoute` and `OrbitRoute`
   (planner.hpp:674-677, 721, 749, 768-770, 790, 808, 830-831). Its inputs:
   - entry and exit, target, delivery and `release_short`, the type's heights and speeds;
   - the believed threats (`c_->intel->Threats()`) with `sigma_m`, `threat_low`/`threat_medium`, radius and
     lethality;
   - the ground and features (`c_->data->country`: `GroundHeight`, rivers, settlements, landmarks; the theatre's ENU
     is the country map's frame as `logistics.hpp` uses it, data.hpp:2079);
   - our believed fighters (today only `FriendlyFighters(front)`; a believed picture of our air bases and CAP areas
     would be new intel);
   - the officer's traits (methodical, creativity, rigidity) for how hard he works the route.

   It returns `RoutePoint`s with the tags kept: exactly one `target`, the attack's `pull_up`/`apex`/`roll_in` before
   it, `ip`, `entry` first and `exit` last.
2. **Choose**: add a route-variant dimension to `Option` (planner.hpp:571-578) and the option loop
   (planner.hpp:1072-1180). Let the exit differ from the entry (`o.exit` exists, always set equal today). Add the
   variant to `TacticKey` (officers.hpp:35) so officers remember corridors, and so `axis_repeat`/`axis_vary` mean
   corridors. `ExpectedLosses` and `RouteChord` already price any polyline.
3. **Fix the shape assumptions** in code that relies on `MainRoute`'s layout:
   - `TimeRoute(r, 2 + attack.size(), …)` (planner.hpp:912) uses a hard-coded anchor. Anchor at the `target` index,
     and give each leg its own speed: cruise on transit, `attack_mps` from the IP.
   - The resolver's fighter roll at `si == 1` (resolve.hpp:232) assumes leg 1 is IP to attack.
   - `ExpectedLosses`' stand-off jammer test `route[1].tag == "orbit"` (planner.hpp:990-991).
   - `radar_threats` reads only the first main flight's route (planner.hpp:691).
   - The reach check uses straight distances, not the route's length (planner.hpp:840-850).
   - The raid builder's single speed per clipped route (raid_scenario.hpp:475-478) and its constant sideways offset
     taken from the first leg (raid_scenario.hpp:441-448, 458-473). Doglegs need per-leg offsets or leader-following.
4. **In the session** (live behaviour, HQ, retasking):
   - A flight-level controller needs the flight structure the runner lacks. `RaidAircraft::flight`/`role`/`officer`
     are in the manifest, so the natural host is `MissionBridge::Step` (mission_bridge.hpp:70) or a sibling stepped
     after `ScenarioRunner::Step`.
   - It needs a new runner API to set a target's waypoints, heights and speed (precedent: `Instruct`, `TakeTarget`,
     `ReleaseTarget`, scenario_runner.hpp:10984, 11251-11297; `OnDamage`'s `next_waypoint = last`, 2660).
   - The neighbours' pass chance is fixed at zone entry (`MissionBridge::Engage`), so it must be re-decided on a
     reroute.
   - Anything flown off the planned route must reach `CompletePlayerMission` as the route actually flown. Today the
     rest of the raid is resolved on the planned polyline (`ResolvedPieces`).
5. **In the abstract war**: `StepTo` resolves a package whole at take-off (campaign.hpp:664-680). In-flight
   retasking, HQ vectoring other flights, on-call strikes (`on_call_fraction`) and packages meeting fighters need
   resolution by leg in time order across packages, or an event queue within the hour step. In a live session the
   campaign clock is frozen while a package is in (`LivePackages::Step`, package_injector.hpp:181-203), so
   cross-package reactions there need that freeze lifted first: roadmap 15.3's "Left: packages one at a time".
6. **Comms**: the enemy's air nets (strike frequency, GCI or KP, AWACS, FAC) as nets of `em/comms_system.hpp`. Their
   transmissions are emissions that our side's ESM and intercept can hear. What our side hears must reach the
   player's character through `simcore/command/` (the first-person rule in CLAUDE.md), never a HUD panel.
7. **Defensive reactions** belong to roadmap R4 (`docs/DEVELOPMENT_ROADMAP.md`, track R: RWR on any radar, sighted
   launches and flak, own damage; break, dive, jink, chaff and flares, jettison). It needs R1's damage in played raids
   (today only in the testbed) and R3's attitude flight.

---

## 12. Determinism and save/replay

- **One stream**: `AirCampaign::rng_` (campaign.hpp:530; `Rng` SplitMix64, types.hpp:395-442) serves the planner
  (noise per option at planner.hpp:1152, `Commit`'s jitter at 1339, `Interference` at 1409/1418), the resolver, the
  weather, daily events, `AimOnCall` and the beliefs.
  - **Every extra draw shifts every later one**: the whole campaign, its journal and every balance row move
    (`air_war_balance`, `air_war_bench`, `docs/air_war/09_BALANCE.md`).
  - A route planner should draw nothing, or draw from `rng_.Fork(key)`. `Fork` is const and leaves the parent's state
    alone (types.hpp:433-437).
  - Adding route variants inside `BestPackage`'s loop adds one `Normal` draw per extra option unless the noise is
    taken once per existing option or from a fork.
- **The save** (`save.hpp`):
  - `SaveCampaignJson` (250-299) writes the theatre and faction files, the seed and the actions: advance, relocate,
    skip, complete with the full `PackageOutcome`, and transmit. Numbers have 17 significant digits.
  - `LoadCampaignJson` (302-368) reloads the data from the files, `Init`s with the seed and `Replay`s
    (campaign.hpp:330-353).
  - Replay re-runs the planner. A planner change changes the packages, and a saved `complete`/`skip` can then meet no
    pending raid ("the save doesn't fit the data"). `save_version` is 1, with no planner or data version.
  - Tests: `simcore/tests/test_air_war.cpp:139` (a seed replays exactly and another differs) and `:943` (a save loads
    to the same campaign).
- **A played session** is not replayed. Its result is the saved `PackageOutcome` (losses with flight index and place,
  released and aborted flights, damage, emissions, sightings...). So:
  - in-session AI need only be deterministic per scenario seed (the bridge's `Rng` is seeded from the session seed
    XOR the package id, package_injector.hpp:223; the raid's seed is `1 + (id × 7919) % 100000`,
    raid_scenario.hpp:601; presentation draws use their own `std::mt19937`, mission_bridge.hpp:388);
  - **anything the session decides that changes the campaign must be in the outcome or a new recorded
    `CampaignAction`**: HQ diverting another package, an impromptu attack on another object, an on-call strike
    launched on a sighting, a route flown off-plan. Otherwise a loaded save diverges.
  - Saving a mission in progress (roadmap 15.7) is not built.
- `test_packages_live.cpp:159` checks that a raid injected into a session flies as the same raid played as its own
  scenario. A live controller must keep that equivalence, or the test must change on purpose.
- The testbed's waves are not portable across compilers (std distributions, §8). New air-battle randomness should use
  `airwar::Rng`.

---

## 13. Constraints any new work inherits
- simcore is header-only C++17 with the standard library only: no exceptions, no RTTI, and no macro-named identifiers
  (`check`, `near`, `far`, `min`, `max`...: `tests/header_check.cpp`). SI units with suffixes. Warnings are errors.
- `PassKillChance`, `EngagedAircraft` and `TargetGunsChance` are shared by the planner's estimate and the resolver's
  truth, and the bridge uses them for the neighbours. Keep one rule.
- `aircraft.hpp` changes must be mirrored in `prototype/samproto/engagement.py` (`Aircraft`). There is no air-war
  mirror.
- Every system value is sourced, and sources are registered in `docs/SOURCES.md` with tiers (`tools/check_citations.py`).
  The air war's aircraft numbers are PLACEHOLDERS (raid_scenario.hpp:539-541).
- The first-person and written-record rules: our side learns of enemy comms, sightings and kills only through the
  pipeline and the crews' records.
- Balance gates: `air_war_balance`, `air_war_bench`, `crew_balance`, `ground_check data 3600`, and `helper_coverage`
  after mission changes.
