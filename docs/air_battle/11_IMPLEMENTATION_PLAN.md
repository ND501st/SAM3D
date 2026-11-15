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
| B1 | **Flights in the session.** The flight and the package are kept: leader, element lead, wingmen, package commander, formation slots. A command layer sits between the pilot and the aircraft (orders for route, height, speed, slot and manoeuvre); nothing overwrites `Aircraft::waypoints` any more. A runner call re-routes a whole flight. `RaidScenarioBuilder` builds flights, not lone targets, with per-leg speeds. Faction formations come from data | R3 | 02 |
| B2 | **Routes as flown.** A route planner works on the believed threats and the ground: let-down, the front crossed in a corridor at a time window, exposure-weighted turn points on map features, the IP on a feature, the target, an egress turn and a different way home. Heights and speeds come per faction, type and phase. Route variants enter the planner's options and the officers' tactic memory. Includes the fixes to the code that assumes `MainRoute`'s shape | — | 01 |
| B3 | **Perception.** The eye (air targets, smoke trails, launch dust and flash, flak, ground targets) on `em/optical.hpp`, the light, the weather and the line of sight, with the period's detection models. Warning receivers as they were: type and bearing with error, never our true position. The neighbours' and the PU-12M's radars radiate in the EM layer. The Pact's receiver library is fixed. The truth cues are removed | B1 | 03 |
| B4 | **Reactions under fire** (was R4). The chain runs cue, delay by skill and stress, call, manoeuvre by threat and faction, chaff and flares by type and year, jettison, press on or abort as one decision for the flight, then the report. Played and injected raids get R1's damage states. Stores fall in the fx feed. A raid denied its target counts in the enemy's freedom-of-action ledger (14.6). Scenarios with today's `evade` keep working | B1, B3, R1 | 04 |
| B5 | **Command in the air.** The enemy's C2 posts become people with authority by mission type (NATO: ATOC, ASOC, CRC, FAC; the Pact: the air army's post, its control posts, ГБУ, авианаводчики). Reports and requests travel as messages with their delays. Alert pools fly the planner's on-call share. Diverts stay within authority. Hunters are sent to a reported launch where the faction's rules allow. The resolver becomes time-stepped (A0), so the campaign clock no longer freezes during a live package. Every live decision is recorded for the save | B1, B9 | 05 |
| B6 | **Targets of opportunity.** The chain runs seen, identified, authorised, able, worth it, then act: ignore, report, recce pass, attack, gun run or re-attack. It is data per faction and type, moved by skill and traits. Attack passes are flown (dive, strafe, rockets) in place of the one-shot release gate and the gun footprints. Claims enter the enemy's records as claimed | B3, B5 | 06 |
| B7 | **Air-to-air weapons and sensors as data.** Missiles, guns, fighter radars, IRST and warning receivers per type, each with `in_service_year` and a source. The shot is a chain of chances calibrated to CHECO and the period's wars. Fighter radars become `RadarDef`s in the EM layer, so warning receivers hear their locks | — | 07 |
| B8 | **Air combat.** Rules of engagement (visual identification in central Europe), formations, commit and disengage (joker, bingo, numbers, weapons), the Pact's ground-controlled intercepts, and the effect of losing ground control. Strikers answer interceptors. Escort, CAP and sweep flights are planned and flown. The resolver's flat fighter roll is replaced by an abstract fight under the same rules (A0) | B1, B3, B7 | 07 |
| B9 | **The enemy's radio and our intercept.** Every flight's and post's call is an EM transmission with a band, a power and a kind of content. Emission and radio discipline come per faction and officer. Our intercept posts sit in the chain of command, hear by link budget, line of sight and language, and pass fast warnings and daily summaries down the information pipeline into the battery's journal. An intercept fact kind is added | B1 | 09 |
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
