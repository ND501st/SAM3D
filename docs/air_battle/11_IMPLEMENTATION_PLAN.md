# 11. Implementation plan: track B, the air battle

The tasks, their order, what each rests on and when each is done. Design: files 01-10 of this folder. Research:
`docs/research/AIR_BATTLE_RESEARCH_2026-11.md`. The state of the code on 15 November 2026:
`docs/research/air_battle_2026-11/map/`.

## 1. Where the track sits

**Track B rests on three rows of track R.**
- R3, flight with attitude: the flight model of doc 2 is R3's bank-to-turn with energy added.
- R1, damage states: built for the testbed. It is needed for played and injected raids.
- R4, defensive reactions: track B takes it over (doc 4). R4's row in the roadmap points here.

**It widens two rows of the battery's war.**
- 16.3, ELINT and COMINT: the enemy's calls become emissions (doc 9).
- 17.2, our fighters: they become live actors (doc 8).

**The order (decided 15 Nov 2026).** R3 is finished first, as track R planned. Track B then replaces R4 as track
R's last item, before the leftovers and the battery's war, and is not gated. The owner left this and decisions 2
(gameplay level) and 10 (our fighters as live actors) of the README to Claude, and all three stand as proposed.

**What stays as built.**
- The planner's choice of targets, packages and timing (`docs/air_war/04_PLANNING.md`).
- The officers (`02_AIR_STAFF.md`).
- The intelligence picture (`03_INTELLIGENCE.md`).
- The mission bridge's role (`05_EXECUTION.md`).

Track B changes how packages are routed and flown, and what comes back from them.

## 2. The tasks

| # | Task | Rests on | Doc |
|---|---|---|---|
| B0 | **Research.** **Done 15 Nov 2026**: ten topic reports, each checked against its sources (`AIR_BATTLE_RESEARCH_2026-11.md`). Left: the open items each report lists (NATO radar ranges, HAVE QUICK's fielding in USAFE, sensor pods' dates, Pact allies' flying hours, corridor widths, Parol's date). Its §6 lists the documents to acquire | | all |
| B1 | **Flights in the session.** The flight and the package are kept: leader, element lead, wingmen, package commander, formation slots. A command layer sits between the pilot and the aircraft (orders for route, height, speed, slot and manoeuvre); nothing overwrites `Aircraft::waypoints` any more. A runner call re-routes a whole flight. `RaidScenarioBuilder` builds flights, not lone targets, with per-leg speeds. Faction formations come from data. **Done 15 Nov 2026** (branch `tb-b1`; `02_FLIGHTS_AND_PILOTS.md` §2.5): `simcore/flights.hpp` (flights, packages, orders with their `by` and the call's delay, slot keeping, succession, the package's nerve, `PilotOverride`), `AircraftDemand` in `aircraft.hpp` (mirrored in the prototype), the scenario's `"flights"`, `"packages"` and a target's `"flight"`, `"slot"`, `"legs"`, the runner's `OrderFlight`, `RerouteFlight`, `Flight`, `FlightsOf`, each faction's `"flights"` block, the raid builder and the waves writing flights (`scenario.flight_spacing_m` gone), the bridge's `aborted_flights` from the orders, `retasks` and `Reroute`; `tests/test_flights.cpp`. **Left:** `Notice` and the reaction chain (B3, B4), `C2Authority::May` (B5, accepts all), the calls as transmissions (`hears`, B9), `TimeRoute` per leg and heights as demands (B2), splits for the attack and tactical turns (B8), jettison, the alternate and escorts for a damaged wingman (B4), pilots' ids and fates, the route flown in the outcome, `engagement.hpp`'s own `evade_on_launch` | R3 | 02 |
| B2 | **Routes as flown.** A route planner works on the believed threats and the ground: let-down, the front crossed in a corridor at a time window, exposure-weighted turn points on map features, the IP on a feature, the target, an egress turn and a different way home. Heights and speeds come per faction, type and phase. Route variants enter the planner's options and the officers' tactic memory. Includes the fixes to the code that assumes `MainRoute`'s shape. **Done 17 Nov 2026** (branch `tb-b2`; `01_ROUTES.md` "As built"): `simcore/air_war/route_planner.hpp` (the map's features and the sites' viewsheds, a search over features by time and believed exposure, IPs on features, the egress turn and home by another way, the helicopters' battle positions, `TimeRoute` per leg, the Pact's zone escapes), each type's `"low_level"` and each doctrine's `"routing"` with sources, the tuning's `route` group, variants as options and corridors in `TacticKey`, the gate book, the group ahead, NATO's corridor suppression, the resolver's passes and masked chords, the fixes of §8, `route.mode` 0 for saves made before it (`save_version` 2); `tests/test_routes.cpp`. **Left:** a fallback when no feature lies within a leg of the gate, the believed fighter picture in the leg cost, navigation error, the testbed's waves, routes as flown back from a session, the in-flight deviation (B4, B5 read the rules), a Pact helicopter's umbrella and `axis_vary` have no test of their own; the rebels' losses on the nation (14-20 %, out of their band) and the planner's cost on a theatre without a map (6-8 times mode 0) | — | 01 |
| B3 | **Perception.** The eye (air targets, smoke trails, launch dust and flash, flak, ground targets) on `em/optical.hpp`, the light, the weather and the line of sight, with the period's detection models. Warning receivers as they were: type and bearing with error, never our true position. The neighbours' and the PU-12M's radars radiate in the EM layer. The Pact's receiver library is fixed. The truth cues are removed. **Done 17 Nov 2026** (branch `tb-b3`; `03_PERCEPTION.md` §5.9): `air_war/perception.hpp` and `perception_feed.hpp` (the eye's law, the ground law, each pilot's picture, the warning receivers' displays by faction and year, the events the runner records), `data/gameplay/perception.json`, `data/em/pact_1985.json`, `sensors.json` `rwr_sets`, `rwr_by_year`; the neighbours' launchers and the PU-12M's P-19 radiate; the truth cues and `scenario.revealed_by_launch_m` are gone; `PackageOutcome::sightings`; `test_perception.cpp`, `test_perception_em.cpp`, `perception_bench`. **Left:** B4 reacts to the pictures (delays, calls, jettison; today's entry points are only re-fed); B9 carries a sighting to the wingmen; no A1/A0 level (B12); the rear blind arc is one rule; TISEO, pods and the weather's share by month | B1 | 03 |
| B4 | **Reactions under fire** (was R4). The chain runs cue, delay by skill and stress, call, manoeuvre by threat and faction, chaff and flares by type and year, jettison, press on or abort as one decision for the flight, then the report. Played and injected raids get R1's damage states. Stores fall in the fx feed. A raid denied its target counts in the enemy's freedom-of-action ledger (14.6). Scenarios with today's `evade` keep working. **Done 17 Nov 2026** (branch `tb-b4`; `04_REACTIONS.md` §15): `simcore/reactions.hpp` (the tables, keyed draws on `em::Stream::Reaction` = 20, the pure functions) and `reaction_chain.hpp` (cue from the pilot's picture, delay, call on B9's radio, manoeuvre as a `PilotOverride`, chaff and flares, jettison, the flight's decision once per situation by its leader, a wingman who heard nothing deciding alone), `data/gameplay/reactions.json`, `data/air_war/countermeasures.json`, a target's `"reactions"`, generated raids and the testbed's waves writing it, R1's damage states in played and injected raids (`"damage"`, the Osa's hits split, the bridge's neighbours drawing damage), flares against IR shots, chaff in the EM layer or the engagement model, stores and tanks falling in the fx feed (`chaff_drop`, `flare_drop`, a jettison's `weapon_release`), `PackageOutcome::jettisoned_flights`, `tanks_dropped`, `stores_dropped` and the ledger's `prevented_jettisoned`; `tests/test_reactions.cpp`. **Left:** hand-written `"evade"` still flies its old code (not turned into a scripted profile); the late-window manoeuvre factor is in the engagement model (`manoeuvre_window_s`) but off until re-tuned; our crews' records of what they saw of a jettison or chaff; Unreal draws neither chaff nor flares; the enemy's reports of what his pilots believed (`InFlightReport`, B5); escorts and fuel for a damaged aircraft; the A1/A0 levels (B12) | B1, B3, R1 | 04 |
| B5 | **Command in the air.** The enemy's C2 posts become people with authority by mission type (NATO: ATOC, ASOC, CRC, FAC; the Pact: the air army's post, its control posts, ГБУ, авианаводчики). Reports and requests travel as messages with their delays. Alert pools fly the planner's on-call share. Diverts stay within authority. Hunters are sent to a reported launch where the faction's rules allow. The resolver becomes time-stepped (A0), so the campaign clock no longer freezes during a live package. Every live decision is recorded for the save | B1, B9 | 05 |
| B6 | **Targets of opportunity.** The chain runs seen, identified, authorised, able, worth it, then act: ignore, report, recce pass, attack, gun run or re-attack. It is data per faction and type, moved by skill and traits. Attack passes are flown (dive, strafe, rockets) in place of the one-shot release gate and the gun footprints. Claims enter the enemy's records as claimed | B3, B5 | 06 |
| B7 | **Air-to-air weapons and sensors as data. Done 15 Nov 2026** (branch `tb-b7`). Missiles, guns, fighter radars, IRST and warning receivers per type, each with `in_service_year` and a source. The shot is a chain of chances calibrated to CHECO and the period's wars. Fighter radars become `RadarDef`s in the EM layer, so warning receivers hear their locks. Built: `data/air_war/air_combat.json` (18 missiles, 4 guns, 16 fits, 5 doctrines, the chain and its targets), `sensors.json` (17 sensors, 12 slots), `data/em/fighter_radars_1985.json` (7 radars, the airborne anchor and `look_down_db` in `RadarDef`), `air_war/air_combat_types.hpp`, `air_combat_data.hpp`, `air_combat.hpp`, `test_air_combat.cpp`. **Left:** the radars are not yet placed on aircraft in a session (`EmSensorLink::SyncTarget`: B8); `look_down_db` is read by nobody until B8; the warning receivers' display data and `rwr_by_year` are B3's; no AAM shapes in `munitions.json` yet (the `munition` key is empty: B13); the four data conflicts of 07 section 2.7 are B11's (the fits are already right for 1985: `draken_gun`'s starts in 1988, the F-16A's has no radar missile, `mig23ml` has its own fit beside `mig23mld`; B11 changes the faction files, retires `air_to_air` and gives our order of battle's `mig21` its type id); the chain's `calibration` targets for exchange ratios, gun shares and the infrared share are for B8's and B14's benches | — | 07 |
| B8 | **Air combat.** Rules of engagement (visual identification in central Europe), formations, commit and disengage (joker, bingo, numbers, weapons), the Pact's ground-controlled intercepts, and the effect of losing ground control. Strikers answer interceptors. Escort, CAP and sweep flights are planned and flown. The resolver's flat fighter roll is replaced by an abstract fight under the same rules (A0) | B1, B3, B7 | 07 |
| B9 | **The enemy's radio and our intercept.** Every flight's and post's call is an EM transmission with a band, a power and a kind of content. Emission and radio discipline come per faction and officer. Our intercept posts sit in the chain of command, hear by link budget, line of sight and language, and pass fast warnings and daily summaries down the information pipeline into the battery's journal. An intercept fact kind is added. **Done 17 Nov 2026** (branch `tb-b9`; `09_COMMS_AND_INTERCEPT.md` "As built"): `simcore/air_war/air_radio.hpp` (every flight's and post's call a transmission in a session-level EM world, its turn on the half-duplex net, stale calls dropped, a member acts only on what he heard, comm-out and discipline per faction, officer and unit, the opsec draw), `em/comms_intercept.hpp` (`RunCommsIntercept`: our receivers hear each call at its start by link budget and horizon), `command/intercept.hpp` (linguists, slips, continuity, habit, the fast road, the day summary, `PlaceEnemyListeners`), `FactKind::Intercept`, `ScenarioRunner::EnableAirRadio` and `AddDeliveries`, the mission session's intercept service into the battery's journal, `PackageOutcome::heard` and its save, a `radio` block in each faction file, `data/em/air_radios_1985.json`, `data/command/intercept.json`, the intercept posts in the chain, `tests/test_air_radio.cpp`. **Left:** the abstract level's calls (A0: B5's time-stepped resolver) and A1 (B12), rebuilding the intercept facts from `PackageOutcome::heard` on a load, the C2 posts' decisions and the `C2Message` mapping (B5), GCI and FAC posts placed by the raid builder (B5, B8), the enemy's listeners placed in the campaign screen and Teaball's relay, deception with dummy traffic, posts under attack, a no-HUD canary test | B1 | 09 |
| B10 | **Our fighters** (widens 17.2). Duty, alert and scrambles follow a rule table. Each flight is a live actor guided by our GCI on our picture. Zones or shared heights follow Mitronin (1976). Corridors are in the battery's order and on its working map. Notices and Kremniy-2M code changes reach the battery down the pipeline with their delays. The Osa's and the PU-12M's IFF read the current code. Friendly fire follows a missed notice or code and is traceable in the record. The enemy learns our corridors | B7, B8, B9 | 08 |
| B11 | **Faction profiles.** The hot-phase values per faction are data with source and confidence; officers' traits and pilots move them. Two data changes the research asks for, with their balance cost: NATO's `medium_altitude_man` made rare, and NATO's interdiction from day 1 at a reduced share in place of the rollback gate. Four entries in today's data that are not 1985 (`07_AIR_TO_AIR.md` §2): the neutral's Draken (1988; its 1985 jets are gun-armed Saab 105s), the Pact flying only the MiG-23MLD (the ML is 1985's standard; the MLD entered series production in 1984), the F-16A's escort value with no radar missile to back it, and the A-50 controlling fighters in 1985 | B4-B10 as each lands | 10 |
| B12 | **Levels of detail and cost.** A2 is full behaviour inside the play circle, A1 is coarse, and A0 is the time-stepped resolver. Aircraft move between levels with hysteresis and keep their state. Thinking runs at 1-5 Hz, staggered by a fixed key. The budget is about 0.5 ms per 0.05 s step for about 100 aircraft, measured in `em_bench` and in the testbed's hour. The testbed's waves fly through the same code | B1 onwards | 02 |
| B13 | **Unreal.** Roll from R3's bank. Formations, jettisoned tanks and stores, attack passes, flares and chaff, missile trails, air combat and our fighters drawn from simcore's state. The enemy's calls are heard only where the player's character would hear them (an intercept post's set, never the cockpit). The owner plays it; Claude cannot run the editor | B1-B10 | — |
| B14 | **Balance.** `air_war_bench` and `air_war_balance` gain rows for the hot phase. These cover jettison, press-on and abort shares against the F-105 records, and hit and out-of-parameter rates against CHECO. They also cover exchange ratios against Vietnam's 1968-72 swing and the Bekaa, friendly fire against the Soviet exercises' 25% ceiling, and retask latencies against the doctrine. `crew_balance`, `helper_coverage` and `ground_check` stay green | all | all |

## 3. Order

1. **B1 and B7** first. B1 needs R3; B7 is data and can run alongside.
2. **B2, B3 and B9** in parallel: routes, perception and the radio.
3. **B4** on B3: reactions.
4. **B5 and B6** together: command and opportunity.
5. **B8** on B3 and B7: air combat.
6. **B10** on B8 and B9: our fighters.
7. **B11 and B12** run through the whole track: each task adds its faction values and is measured for cost.
8. **B13** follows each simcore task.
9. **B14** last.

Each task is its own branch and PR, in its own worktree (`C:\dev\SAM3D-wt\<name>`). Design-heavy starts (B1, B2, B5,
B8, B10) suit Opus or Fable; data entry (B7, B11) and implementing a written design suit Sonnet.

## 4. Rules every task keeps

- **simcore's hard rules.** Header-only C++17, no exceptions or RTTI, SI suffixes, the ENU frame, warnings as
  errors. Every new model gets a test, checked against hand calculations where it is physics.
- **The period gate.** Every new type, weapon, sensor, dispenser or procedure has an `in_service_year`, or a doctrine
  date in its notes. The testbed's waves still ignore the year, as today; the 1985 war never does.
- **Sources.** Every value in `data/` has `source` and `confidence`, registered in `docs/SOURCES.md`, never tier 5.
  Game values are labelled as such.
- **Knowledge, never truth.** No enemy pilot or post acts on our true position. No NPC of ours acts on the enemy's
  truth.
- **Determinism.** Forked or keyed streams. No reading of the fx feed. Live outcomes return as recorded actions. A
  saved campaign replays to the bit.
- **First person.** No HUD panel of enemy intentions, intercepts or friendly flights. Everything reaches the player's
  character through people and means of signal and is written in the journal.
- **The Python prototype** mirrors any change to `aircraft.hpp`, the missile or the EM layer's random streams.

## 5. Milestone M-B: done when

The owner plays a NATO morning and a Pact afternoon on the nation's map against battery 21 and sees the following.

1. **Routes.** A NATO four comes in low along a valley, crosses the front in its corridor at its time, and stays
   outside the battery's believed ring until its IP on a river bend. It goes home by another way.
2. **The wingman's call.** After the Osa's launch, a wingman calls it by clock code on the flight's net and the
   flight breaks. Two aircraft jettison their tanks, one aborts, and the lead reports the launch area.
3. **The NATO answer.** Where its rules allow, a hunter pair from alert reaches the launch area within its doctrinal
   time. The battery has moved, or it has not.
4. **The Pact's flight.** It keeps to its plan through the same launch until its control post changes it. A Pact pair
   that sees a column on its way reports it and requests a strike. It does not attack on its own unless it was sent
   to hunt there.
5. **Air combat.** An F-15 CAP bound to visual identification meets our MiG-23s under ground control. When our GCI is
   jammed, our pilots fight worse. The exchange over many seeds falls in the band B14 sets.
6. **Friendly fire.** A MiG-21 whose notice did not reach the battery is challenged and then fired on, and the
   journal shows why.
7. **The intercept.** Our intercept post hears a NATO check-in, and a warning with a target and a time reaches the
   battery's journal minutes before the raid. Nothing appears on a HUD.
8. **The record.** The enemy's journal holds its pilots' claims, which are higher than the truth. The debrief shows
   the truth.
9. **Determinism.** The same campaign, saved and loaded, plays the same.
10. **Cost.** The testbed's hour stays within its budget.

## 6. Not planned

- Our air force as a planner: our fighters follow a rule table and our GCI.
- Flight physics beyond the point mass, and seeker physics.
- Post-1985 weapons and tactics in the 1985 war.
- Our side jamming the enemy's air nets, unless research for a Pact-like small nation of 1985 supports it (open in
  doc 9).
- The player flying an aircraft.
