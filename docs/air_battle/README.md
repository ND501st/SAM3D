# The air battle: the war in the air while it is fought (track B)

Written 15 November 2026 at the project owner's request. This folder is the design for the air war's **hot phase**:
what happens once a raid is flying. That covers the routes as they were flown, the pilots and their flights, what they
see and hear, how they answer fire, who may change their task in the air, what they do with a target they were not
sent for, air combat between the enemy's aircraft and ours, our fighters and the battery sharing the sky, and the
enemy's radio and who hears it. The planning that comes before it is `docs/air_war/` (the enemy air command). The
research is `docs/research/AIR_BATTLE_RESEARCH_2026-11.md`, with its ten topic reports and the code maps of
15 November 2026 in `docs/research/air_battle_2026-11/`. The tasks are roadmap track B (`11_IMPLEMENTATION_PLAN.md`).

## The brief (project owner, 15 November 2026)

1. The roadmap does not flesh out the enemy's air warfare in the hot, active phase. Attack aircraft fly direct routes
   to their targets, not the routes doctrine flew. Doctrine kept away from the air defences where it could, followed
   terrain features, and evaded aircraft and missiles. Today's aircraft also never change their priorities as the
   fight unfolds.
2. A pilot or wingman who sees a missile's smoke trail tells his flight, or others, to evade, to jettison tanks and
   stores, to re-engage a new target, or he radios HQ. HQ may then send SEAD or attack aircraft to the launch area.
3. A flight on its way (or another flight, or HQ through intelligence) may spot another target, a convoy or a site.
   The answer may be nothing, an impromptu attack, a gun run, a bombing run or a reconnaissance pass.
4. This needs several things:
   - pilots' vision from the cockpit and their sensors;
   - live communication between flights, HQ and squadrons;
   - better flight control and coordination for AI pilots;
   - HQ able to change routes and targets in flight;
   - live communications and emissions that can be intercepted.

   Some types favour quick changes of target and others do not, because of logistics, priorities or emission
   control.
5. Air combat between our aircraft and the enemy's must be built: the air-to-air missiles, radar, IR and visual
   tracking, air combat tactics and dogfighting, coordination between ground and air, and BVR combat.
6. All of it is researched, so each faction fights as it actually did, some better than others. The decisions,
   traits, flaws and skills of every party have an effect.
7. **The Cold War, not modern tactics** (said twice).

## How SAM3D reads it (decisions)

| # | Decision | Why |
|---|---|---|
| 1 | **The war's year is 1985.** Every tactic, weapon, sensor, C2 arrangement, radio procedure and brevity word is gated as aircraft and ARMs already are (`TheatreDef::year`, `in_service_year`). Later things (AMRAAM, the R-27, JTIDS, LANTIRN, NVG fast-jet strikes, kill boxes as a term, JSTARS, AWACS strike control, the 1997 brevity list) are kept only as a later war's options | The brief, point 7; the research's post-period lists |
| 2 | **Gameplay level, not physics.** Aircraft are point masses with energy and turn limits on R3's bank-to-turn. An air-to-air shot is a chain of chances calibrated to the period's combat records | The roadmap's resolved decision on physics; CHECO's hit and out-of-parameter rates (`aam_weapons.md`) |
| 3 | **Knowledge, never truth, for enemy pilots too.** A pilot acts on what he sees and what his receiver and radio tell him. The truth cues of today are removed | Crew pillar 3 by analogy; most threats were first seen by eye (`reactions.md`) |
| 4 | **Flights, not lone aircraft.** Leader, wingmen and the package commander survive into the session. Orders go through a command layer | Today a flight is an id prefix and a sideways shift (`map/flight.md`) |
| 5 | **Retasking by voice through ground nodes, per faction.** There was no datalink to strike aircraft in 1985. Who may divert what, by which means and how fast, is faction data | `c2_retasking.md`: planned strikes flown as planned on both sides; alert pools and on-call strikes, not airborne diverts |
| 6 | **Live decisions are deterministic and recorded.** Anything the enemy learns or changes in a session returns to the campaign as a recorded outcome or action | Saves replay the seed and the actions (`map/scale.md`) |
| 7 | **First person and the record.** The enemy's calls are EM emissions. Our intercept posts hear them by rule, and what they learn reaches the player only through the information pipeline. Pilots' claims go into the enemy's records as claimed | `docs/command/01_PRINCIPLES.md`; `comms_emcon.md` |
| 8 | **Levels of detail A2, A1, A0.** A2 is full behaviour near the play. A1 flies with coarse cues. A0 is the resolver made time-stepped, so abstract packages exist in the air and can report and be retasked | The budget and today's atomic resolution (`map/scale.md`) |
| 9 | **People.** Pilots below the squadron leader become people: skill from flying hours and combat, nerve, stress and fatigue. Officers' traits move a faction's hot-phase values by up to about ±0.2, and pilots by about ±0.15 | `factions_cases.md`, `perception.md` |
| 10 | **Our fighters are live actors under our ground control.** Their duty and scrambles follow a rule table, and they share the sky with the SAMs by zones or by height. The battery learns of them only by notices and IFF codes, so friendly fire is possible and traceable. Our air force as a planner stays unplanned | `our_side_air.md` (Mitronin 1976, Druzhba-76, Kremniy-2M); widens roadmap 17.2 |

The owner left decisions 2 and 10 and the track's place in the order to Claude (15 Nov 2026). All three stand as
written: gameplay level, our fighters as live actors, and track B after R3 in place of R4
(`11_IMPLEMENTATION_PLAN.md` §1).

## What the research changed

- **Routes keep exposure low; they do not avoid every defence.** A clean bypass works only on a staff map, and every
  route ends inside the target's firing zone (Babich 1984). Routes run on turn points on map features to an IP on a
  feature, cross the front through corridors at set times, and come back a different way.
- **The eye found most threats.** Thirteen of sixteen MiG attacks on F-105 flights came with no warning. Receivers of
  the period gave a type and a rough bearing, warned of no IR missile, and lit a launch light only on a known
  guidance signal. The Osa's own launch, dust and smoke is how a strike pilot most often finds the battery.
- **Jettison and abort were common.** About half of the F-105 flights attacked jettisoned something, a third pressed
  on, and a fifth to a third aborted (`reactions.md`).
- **In-flight retasking was rare in 1985 on both sides.** The Pact's flights kept to the plan unless ground control
  changed it. NATO's airborne diverts of interdiction aircraft were still called a future capability in 1987. Speed
  came from alert aircraft launched on request, at readiness 1 (3-5 min) or 2 (15 min).
- **Most flights never see a target they were not sent for.** Detection, not the weapon, is the limit. Pilots'
  claims were badly inflated: 7 of 103 claimed truck kills were confirmed in 1969-70 (`opportunity.md`).
- **Air combat in Central Europe was bound to visual identification.** IR missiles did most of the killing in the
  period's wars. Pact fighters depended on ground control, and cutting it decided the Bekaa (`air_combat.md`).
- **Our side's fighters and SAMs shared the sky by zones or by height.** Notices took 10-15 min to reach the guns, and
  Kremniy-2M codes changed every 1-6 h. Soviet exercises found up to 25% of the targets fired on were our own
  aircraft (`our_side_air.md`).
- **Intercept gave warning of identity, intent, target and time.** Radar usually warned first of a raid already in
  the air (`comms_emcon.md`).

## Contents

| File | What |
|---|---|
| `01_ROUTES.md` | Routes as they were flown: turn points, IPs, corridors, heights and speeds by faction, the route planner over the believed threats and the ground |
| `02_FLIGHTS_AND_PILOTS.md` | The flight and the package kept in the session, the command layer, the flight model beyond R3, pilots as people, the levels of detail |
| `03_PERCEPTION.md` | What a pilot sees and hears: the eye, the period's warning receivers, airborne sensors by type and year |
| `04_REACTIONS.md` | Reactions under fire (takes over R4): cue, delay, call, manoeuvre, countermeasures, jettison, press on or abort, report |
| `05_C2_AND_RETASKING.md` | Command in the air: the enemy's C2 posts and their authority, reports, diverts, alert pools, the resolver in time |
| `06_OPPORTUNITY.md` | Targets of opportunity: seen, identified, authorised, able, worth it, then act and report; flown attack passes |
| `07_AIR_TO_AIR.md` | Air combat: weapons and sensors of 1985, the shot as a chain of chances, tactics and rules of engagement by faction |
| `08_OUR_FIGHTERS.md` | Our fighters and the battery: zones, corridors, notices, IFF codes, friendly fire |
| `09_COMMS_AND_INTERCEPT.md` | The enemy's radio as emissions, its discipline, our intercept posts and the pipeline |
| `10_FACTIONS.md` | Each enemy in the air: the hot-phase values per faction with their sources, and how people move them |
| `11_IMPLEMENTATION_PLAN.md` | Track B's tasks, order, levels of detail, budget, tests, milestone M-B |

## The loop in a session

```
 planner (01 routes) ──► package in the session: flights with leaders (02)
                                  │
       sees, hears (03) ◄─────────┤ every 0.2-1 s per A2 aircraft, staggered
            │                     │
            ▼                     ▼
   reacts (04): call, break,   target of opportunity (06): report,
   jettison, press on, abort   recce pass, attack, gun run, ignore
            │                     │
            └──► calls on the flight's and HQ's nets (09) ──► our intercept posts ──► pipeline ──► the journal
                                  │
                         enemy C2 posts (05): divert within authority, launch alert aircraft,
                         send SEAD to a reported launch; fighters (07) vs ours under our GCI (08)
                                  │
                   PackageOutcome + recorded actions ──► the campaign (docs/air_war/)
```
