# 5. The systems that make it seamless

What each system does, what exists, and what changes. Planned code is in `simcore/include/simcore/campaign/`
(namespace `simcore::campaign`). The air war's code stays where it is and gains the hooks named here. Every new model
gets a test (`CLAUDE.md`).

## 1. Overview

| System | Exists | Changes or new | § |
|---|---|---|---|
| The enemy's campaign | `airwar::AirCampaign`: one adversary, planner, intelligence, resolution, politics | War aims in the data; the target's worth gains what its loss would cut; packages offered to a running mission, not one at a time | 2 |
| Object roles and consequences | `TheatreObject` (value, hardness, repair, defence) | Roles and supply shares; consequences into the systems built | 2 |
| The regiment's tasking | Our chain's posts and routines (`command/`) | A rule table run by the regiment's NPC officers, giving each battery its task with reasons | 3 |
| The battery task | `BatteryOrder` (march, combat formation, ambush...) | A task with its area, sectors, regime, time, coordination and reasons, sent as a document | 4 |
| The battery commander's planner | The drill master (a vehicle's), the NPC battery commander in a fight | The sequence: positions, sectors, dummies, the oral combat order, the working map | 5 |
| The continuous mission | One scenario per raid, the battery deployed in place | One session for the whole task, the battery's state carried through it | 6 |
| Packages entering live | `RaidScenarioBuilder` writes a whole scenario | The same builder adds a package's aircraft to the running scenario | 7 |
| The director | The pacing: raids within 10 km of the battery | Time compression and the events that wake the player | 8 |
| The enemy's activity in the scenario | Strikes, DEAD, hunters, decoys, ARMs, jammers in a raid scenario | Reconnaissance that photographs and must get home; voice jamming; emissions and radio calls continuously | 9 |
| Our side around the battery | Neighbours as objects, the warning net, the TZM and ATZ | Neighbours' fire, our fighters in the sky, supply over the real roads | 10 |
| The after-action loop | The journal and the board; our side's journal lines from the truth | The report from the records, the regiment's picture, the next order | 11 |
| The debrief | The crew's grades, the campaign's reveal | Objectives with their why chains | 12 |
| The battery's people | One crew tier per campaign; 21-1's crew in a fight | A roster of people with qualifications, experience, needs and duty; whom the player plays | `06` |
| Activities and the schedule | Routines (briefings, visits, the sergeant major's run); jobs at places (8.14) | The battery's day as activities with their readiness cost, wear and faults, the schedule, interruptions | `07` |
| Reach, doctrine, command | The enemy's depth limits, phases and day parts; the pipeline's failures | The air situation and our estimate of it; a doctrine profile per adversary; posts and links that break, improvisation and reconstitution | `08` |
| Rebuilding | Field repair at `1 / repair_h`; a wreck out until its replacement comes | Rebuild jobs that need teams, materials and transport, each of them targetable; temporary fixes; priorities | 2, `02` §3 |
| The player's two outcomes | The crew's grades; the campaign's reveal | The assets-kept ledger and the freedom-of-action ledger | 14 |

## 2. The enemy's campaign, object roles and consequences

- **War aims** (`01` §2): an `aims` block in each faction file (text and the phases it drives), read by the reveal
  and by our analysts' estimate ("the enemy is rolling back the air defences").
- **Roles**: each theatre object gains `supports` (the dependants it serves, with alternatives: `{"group_south":
  ["kesmark_road_bridge", "kesmark_rail_bridge", "ferry*0.33"]}`), new objects where the role needs one (the technical
  battery at the Horka siding, a ferry). A dependant's supply share is the best of its alternatives' working shares,
  recomputed when an object changes (`02` §3).
- **Worth**: the planner's worth gains the believed share its loss would cut (`01` §3). The enemy believes the roles
  as its `IntelPicture` holds the objects. The term has its own weight in `data/gameplay/air_war.json`. The faction's
  bans and depth limit still remove the candidate first.
- **Consequences** hook into what is built: a wrecked regiment radar leaves `regiment_radars` (the PU-12M's board);
  the warning net's posts down feed the c2 outages (the raid builder does this today); bridges change the TZM's and
  ATZ's route time; the depot and the technical battery hold the stocks the TZM and ATZ draw from; the power station
  moves positions on the grid to their diesels. Each change is a fact at its first knower (`own_damage`, as the feed
  does today), so it reaches the battery only through people.
- **Offering**: `AirCampaign::Advance` today stops at one pending player mission. It will instead hand every package
  that comes within the running mission's circle to the session (§7) as it takes off, and resolve the rest abstractly,
  as now.
- **Rebuilds** (`campaign/rebuild.hpp`, `02` §3): a wrecked or damaged object opens a job with its needs: teams of a
  kind, materials of a kind, hours. Teams are units based somewhere; materials sit in depots; both travel over the
  transport links whose state the roles already keep. A job progresses only while its teams are on site with their
  materials, and the people at the site are at risk when bombs fall there, as people at any place are. The new kinds
  (engineer and railway units, materials depots, pontoon parks) get their weights in each faction file, within its
  bans. The enemy's re-strike rule already looks for repair it sees; it now weighs the means too.

## 3. The regiment's tasking

`campaign/regiment.hpp`, data `data/campaign/tasking.json` (the rules of `03` §1) and the division's priorities per
campaign (`data/campaign/<faction>.json`).

- **Inputs**, all from what the regiment can know: the division's priority list; the regiment's picture (§11); its
  estimate of what the enemy knows of each battery (built: the daily estimate); each battery's last report
  (readiness, missiles, fuel, damage); the campaign's front data (where the covered troops are and go).
- **When**: at the evening briefing (the routine at 19:00, `places_pannonia.json`), and at once on an urgent trigger
  (a battery out, a reconnaissance photograph reported, ARM attacks).
- **Who**: the regiment's officers as `airwar::Officer`s (traits, skills, quirks), authored from `chain_pannonia.json`.
  Traits bend the rules (the order's moves on schedule, ambushes forward, the same positions). Quirks are named and
  visible in the orders. Skill adds noise to the estimates.
- **Output**: a `BatteryTask` for each of the regiment's five batteries, with its reasons. Battery 21's goes to the
  player's battery commander (or the NPC). The others go to NPC battery commanders, resolved at the level of detail
  the director gives them (§6).

## 4. The battery task

A struct and its JSON (`campaign/battery_task.hpp`), the order of `03` §2:

```
BatteryTask {
  kind            cover_object | cover_troops | cover_march | cover_crossing | ambush | relocate
  covered         object or unit id; for a march, the column and its route
  reasons         the officer's words, as an enemy package's reasons
  situation       attack directions, helicopter lines, jammer orbit lines, ARM launch lines (from the regiment's picture)
  area            main positions, reserve positions (within 2 km), dummy positions, the TZM's place
  sectors         per launcher and for the MANPADS section; duty launchers and their 45-60 deg sectors
  regime          readiness by time of day; emission: bursts, silent on designations, the ARM drill
  ready_by, until
  coordination    neighbours' sectors; the friendly air corridor and its times; IFF codes; call signs, signals;
                  nets and frequencies (radio_nets)
  reports         what, when, by which means
  support         the TZM's run, fuel, repairs
}
```

It travels as a document fact through the pipeline (P1/P2), with a voice summary for the radio. The battery commander's
planner turns it into `BatteryOrder`s (`battery.hpp`): `March`, `CombatFormation`, `Ambush` (`roving`), `RiverCrossing`,
`Relocate`, `Emcon`, `Reload`, `Refuel`.

## 5. The battery commander's planner

`campaign/battery_plan.hpp`: the sequence of `03` §3 as a timed plan, like the drill master's for a vehicle.

- **Positions**: candidate ground within the task's area from the map (`terrain_map.hpp`'s obstacle grid), open (the
  raid builder's open-ground rule, generalised), with a line of sight along the attack directions (the terrain line of
  sight), pairs 200-500 m apart and up to 3 km between pairs, reserve positions within 2 km.
- **Sectors, dummies, regime**: from the task and the situation.
- **Outputs**: the battery's orders with their times; the oral combat order as callouts to the vehicle commanders on
  the battery net or in person (`voice_comms.hpp`); the working map's marks (each with its source and time); an ideal
  timeline that the helper shows a player battery commander, and that the debrief compares against.

## 6. The continuous mission

`campaign/mission_session.hpp`: one `ScenarioRunner` alive for the whole task, from the order to the end.
The session does not stop between tasks. The battery's days go on in it with their activities (`07`), and the next
task arrives as an order into the running session.

- **Built from the task**, not from a raid: a `MissionScenarioBuilder` takes the battery's state, the task and the
  map. It reuses the raid builder's battery, prepared position, c2, doctrine and environment parts
  (`raid_scenario.hpp`), and has no targets at the start.
- **The battery's state carries** in and out of every session: vehicles and their positions, readiness, missiles per
  container, fuel, damage, dummies and nets, the board, the journal and the commander's log, and the battery's people with their condition and experience (`06`).
  Between missions it lives in the campaign (`BatteryState`, replacing today's posture-only battery).
- **The air war runs in step**: the session advances the campaign to the runner's time. The packages the campaign hands
  over enter live (§7). The rest resolve abstractly.
- **The other batteries of the regiment** run at L1 (the abstract resolution against their defence) unless the director
  needs one at full detail. L2, a raid played headless by an NPC crew, is the same session without a player. The air
  war's open item, "the scenarios' own balance" (`docs/air_war/09_BALANCE.md` §6), is closed by comparing the two on the
  same raids (`war_calibrate`, §13).
- **The map**: the session plays on the district where the battery is. The Kesmark range (40 km) holds battery 21's
  home position by the bridges, where it fights the Pact and NATO. The rebels' and the neutral's fronts send it 70-110
  km south and south-west (`FrontDef::battery_position`), onto the coarse ground beyond the map. Each of those gets a
  district generated the same way (`terrain_map.hpp`, deterministic for its seed), so the battery marches, crosses and
  moves on real ground wherever it fights.

## 7. Packages entering a running mission

- **The runner gains** `AddTargets` (a package's aircraft with their behaviour, EM gear, ARMs and decoy flags, at a
  time), `AddJammer` (a stand-off jammer's orbit) and `AddTrigger` (the warning), mid-run, and drops a target when it
  leaves the scenario's circle.
- **The raid builder splits** into the scenario part (kept for the missions menu's sample raids) and a
  `PackageInjector` that writes the same targets into a running session.
- **The mission bridge** follows each injected package as it does a raid today (releases, ARMs, emissions, launch
  sightings), and returns its `PackageOutcome` when its last aircraft leaves. The campaign applies it and resolves the
  package's legs outside the circle, as `CompletePlayerMission` does now.
- **The warning** comes as today: the air-raid warning's trigger before the first aircraft, `own_intel.warning_s`
  ahead (15 min for a planner with loose lips), delivered through the warning net and the pipeline.

## 8. Time and the director

- **Compression**: duty passes fast. The director wakes the player at full speed on: an order or a delivery, a warning,
  a designation on the board, a contact on the search radar, the TZM or the ATZ arriving, a crew report, a briefing or
  a courier due, the start or end of a scheduled activity the player's person takes part in (`07` §5), an interruption
  (`07` §6), or the player's own alarm. Time compression on the keys stays (`docs/PLAYING.md`).
- **Long quiet stretches**: when nothing is within reach and nothing is due, the session steps coarsely with the EM
  layer idle until the next wake event's lead time. Fuel still burns, gyros still drift, the warning net still listens.
  This is the session's own fast path, measured in 15.4.
- **What the player lives**: every package that comes to the battery's area is lived at the battery. The director
  never skips one and never changes its odds. Between missions it chooses which steps of the next mission's life
  (`03` §5) are lived and which are summarised. The march may be lived; the TZM's run is summarised unless something
  happens on it.

## 9. The enemy's activity in the scenario

| Activity | Today | Change |
|---|---|---|
| Strikes, DEAD, hunters, decoys, SEAD, escorts | In the raid scenario; ARMs as EM missiles (A11) | Injected live (§7) |
| Stand-off and self-protection jamming, chaff | In the raid scenario (`own_unit.em.jammers`) | Injected live |
| **Reconnaissance** | Abstract: photographs what lies along its route | In the scenario, a reconnaissance aircraft photographs what it overflies within its quality's reach (nets halve it). The photographs reach the enemy's picture only if it gets home: shooting it down denies them |
| **Voice jamming** | A hand-written scenario only (`osa_vhf_jamming_em.json`) | A comms jammer kind in the faction data where 16.0's research supports it; the package's jammer jams the nets in its band (`radio_nets.hpp`); the nets' drills answer |
| ELINT and COMINT | The seconds radiated, from the raid; the posture's radio discipline | The session's real emissions every minute, and its real radio calls (P3) |
| Satellites | Abstract passes in the campaign | Unchanged; their pass times as a known hazard is a design option with no source yet (`04` §5) |

## 10. Our side around the battery

- **Neighbours' fire**: an injected aircraft crossing a neighbour's ring (the ZU-23 at the bridges, battery 22, the
  Strela platoons, the S-125) takes that defence's kill chance, as the abstract resolution's `PassKillChance` gives it.
  It is logged as theirs, and heard on the net or seen. A neighbour at full detail is a later choice (`battery_fire.hpp`
  already fights the battery's own other launchers).
- **Our fighters**: friendly aircraft in the battery's sky by the task's coordination (the corridor and its times),
  written as targets that answer IFF. The fighter regiment's notices reach the battery through the pipeline (its duty
  officer is already a post), and friendly fire follows from stale codes or a notice that never arrived.
- **The warning net and the regiment's radars**: as the raid builder sets them today (outages for wrecked radars,
  the PU-12M blind when none stands), carried through the session.
- **Supply**: the TZM and the ATZ draw from the technical battery's and the depots' stocks and drive the real roads and
  bridges. A wrecked bridge lengthens the run (§2).

## 11. The after-action loop

The record principle (`docs/command/01_PRINCIPLES.md` 9) carried into the campaign:

1. **The report** (`campaign/report.hpp`, the pipeline's P9): compiled from the battery's records, the radiotelephonist's
   journal, the board's marks and the commander's log. Raids are given with their times, routes by grid square, and
   sizes as written. Claims, missiles spent, damage. A raid nobody wrote down is not in it.
2. **Up the chain** by the means the post has (P2): the combat report at priority, the day's report routine.
3. **The regiment's picture** (`campaign/regiment_picture.hpp`): the analysts' patterns (the air war's `RaidPatterns`,
   fed from the reports instead of the truth), the enemy's estimated aims and phase (`02` §4), and a trust per unit,
   kept by checking records against one another (the journal against the board and the radar company's log). A unit
   whose reports proved wrong is believed less.
4. **The next order** comes from that picture (§3). A battery whose radiotelephonist lost half the raid sends its
   regiment's ambush to a corridor the enemy never flew.
5. **Our side's journal lines** in the air war (`[ours]`) come from the reports, not the truth, as
   `docs/air_war/README.md` already notes.

## 12. The debrief

The mission debrief (`debrief.hpp` grown) gives the objectives of `03` §6, grouped by the player's two outcomes (§14),
each with its why chain: the order's
reasons, and at the campaign's reveal the enemy package's reasons from its journal ("dead '956 10' against SA-8
battery near Kesmark ... this one has cost us 3 aircraft"). Then the crew's grades and the record's accuracy. The
campaign's reveal stays as built (`AirCampaign::EnemyMind`), with the regiment's officers and their reasons beside the
enemy's.

## 13. Saves, tools and tests

- **Saves**: the campaign stays a replay (`airwar/save.hpp`). A mission in progress is saved as the battery's state and
  the session's clock at a quiet moment, with the outcomes of the packages already fought. Raids are not replayed.
- **Tools**: `simcore/examples/mission_demo` plays one task headless from the order to the report, with the NPC
  battery commander and crews, and prints the why chains. `mission_balance` runs each faction × mission kind over
  seeds: the object's survival, the battery's survival, enemy losses, reconnaissance denied, the enemy's knowledge of
  the battery. `war_calibrate` plays the same packages at L1 and L2.
- **Tests**: roles and supply shares against hand calculations; the worth term never lifts a ban; each tasking rule
  firing on its trigger with its reason; the planner's positions on open ground with sight along the attack direction;
  a package injected mid-run giving the same outcome as the same raid built as a scenario; the battery's state
  surviving a save and load; a report compiled from a journal with a lost entry missing that raid; a reconnaissance
  aircraft shot down taking its photographs with it.

## 14. The player's two outcomes, measured

`campaign/outcomes.hpp`: two ledgers kept from the truth, for the debrief and the reveal (decision B14).

- **Assets kept**: for each object or unit the battery covered, the hours it worked while covered, the hits on it, the
  releases prevented (aircraft downed or turned back before their release point), the rebuilds finished under cover,
  and the consequences that did not follow (`02` §1).
- **Freedom of action denied**: for each enemy package, what battery 21 changed in it. The planner already prices every
  believed threat on a route (`PassKillChance`), adds SEAD per believed radar threat, and logs its reasons
  (`docs/air_war/04_PLANNING.md` §4-7). The ledger records the difference battery 21's entry in the enemy's picture made
  to each package: the SEAD, decoys and jammers added for it; a route or height changed; a package rejected on losses it
  accounted for; the ARMs fired at it; the DEAD, hunter and deception sorties against it; the aircraft it downed or
  damaged. Per day it records whether the battery held the enemy's suppression phase open (the believed share of
  working air defences with and without it). The difference comes from planning each package again without the
  battery's entry, which costs little next to a scenario.

During the campaign our side sees only what its reports show: SEAD in the raids, raids that turned back, the heights
they came at. The regiment's picture can infer part of the ledger from those (§11). The whole ledger is shown at the
reveal, in the enemy officers' own reasons.

Tests: a rebuild stops when its materials depot is wrecked and resumes when another depot supplies it; the people at a
repair site are lost when it is struck; a package re-planned without battery 21's entry carries less SEAD when the
battery was the route's only radar threat; a battery that survives a week against NATO holds the rollback open on the
days its entry tips the believed share.
