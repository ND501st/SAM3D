# 9. Implementation plan (roadmap phases 13-21, the battery's war)

Planned 22 October 2026, reworked the same day. Each phase starts with its research step where it has one, adds a
test for every model (`CLAUDE.md`), keeps balance numbers in `data/gameplay/`, and ends with a headless balance pass
before a person plays it.

## 1. What must be done first

| Item | Why it gates the battery's war |
|---|---|
| **M2**: a person plays the missions through, in a seat of the crew and in command (phase 8's exit) | Every raid in a mission is fought by the crews as the missions fight them now |
| **A10 and A12's second half**: a person plays a campaign through on the campaign screen | The enemy's campaign against one battery must be right before the battery's missions are built on it |
| **Phase 12 P1 and P2**: deliveries in the world, and the player telling people | The order comes to the battery commander through P1, and the reports and requests go up through P2 |
| **Phase 12 P3**: the pipeline's radio calls feed the enemy's COMINT | Needed by 16.3 |
| **Phase 12 P9**: after-action reports from the records | Needed by phase 18 |

**One track comes first and is not gated** (owner, 22 Oct 2026): the nation on the map (`docs/DEVELOPMENT_ROADMAP.md`,
"The nation on the map", N0-N6): the country's map, ultra-basic placeholder models for every asset of the briefing,
and the air war running on them. Phase 13 adds its roles, rebuilds and ledgers to that country.

These may run alongside: E8, E9, the scenery's models, 8.14's next steps, the leftovers of 6.9, 7.x and 10.3, E1's
data, and the "Later" list.

## 2. The phases

### Phase 13: why (`01_WAR_AIMS_AND_TARGETS.md`, `02_WHAT_WE_DEFEND.md`)

| # | Task |
|---|---|
| 13.0 | **Research**: what an Osa regiment covered and how its objects were ranked (`voenizdat_osa_1991` ch. 6, other operator documents). The objects' roles in the fictional country are estimates |
| 13.1 | **War aims** in each faction file (an `aims` block: text, and the phases it drives), shown at the reveal and in our analysts' estimate |
| 13.2 | **Object roles**: `supports` with alternatives and supply shares; the new objects the roles need (the technical battery at the Horka siding, a ferry) |
| 13.3 | **The worth term**: what an object's loss would cut, as the enemy's picture believes it, with its own weight; bans and depth limits applied first |
| 13.4 | **Consequences into the built systems**: regiment radars to the PU-12M's board, the warning net's posts to the c2 outages, bridges to the TZM's and ATZ's run times, the depots and the technical battery to the stocks, power to the positions' diesels; each a fact at its first knower |
| 13.5 | **Rebuilding** (`02` §3): jobs that need teams, materials, transport and time; temporary fixes; the division's rebuild priorities; the new kinds (engineer and railway units, materials depots, pontoon parks) with their weights in each faction file; the enemy's re-strikes on repair and on its means; military units still replaced from the reserve |
| 13.6 | **The freedom-of-action ledger** (`01` §6, `05` §14): per package, what battery 21's entry in the enemy's picture changed (SEAD, decoys, jammers, route, height, rejection, ARMs, DEAD and hunter sorties, aircraft lost and damaged), from re-planning without it; per day, whether it held the suppression phase open. And the assets-kept ledger |
| 13.7 | **The nation built up**: built first by the roadmap's nation-on-the-map track (N0-N6) from the briefing (`10_BRIEFING_FORCES_AND_NATION.md`); 13.7 adds the roles (13.2) and rebuild needs (13.5) of every new object, fuel as a national stock (the Pact neighbour's pipeline), Soviet missiles and spares cut in a war with the Pact, personnel establishments, intakes and reservists |
| 13.8 | **Balance**: `air_war_balance` re-read with the roles and the rebuilds; each faction's choice of targets still in its doctrine's shape (`docs/air_war/09_BALANCE.md` §2); the ledgers against a careful and a careless battery |

**Done when:** with the rail bridge down, NATO's interdiction weighs the road bridge above its kind's weight and says
why in the package's reasons; a wrecked P-40 leaves the PU-12M's board blind beyond the P-19, and the battery commander
hears of it from a person; no ban is ever lifted by the new term; each faction's tables keep their shapes. A
rebuilt bridge comes back later when its materials depot is struck, and NATO re-strikes the pontoon bridge it sees.
A careful battery's ledger against NATO shows the SEAD, ARMs and sorties it drew, and a careless battery's ledger
ends the day it is wrecked.

### Phase 14: the battery's mission (`03_THE_BATTERY_MISSION.md`, `05_THE_SYSTEMS.md` §3-5)

| # | Task |
|---|---|
| 14.0 | **Research**: the regiment's tasking of its batteries and the combat order's form (`voenizdat_osa_1991` pp. 224-245; other operator documents) |
| 14.1 | **`BatteryTask`**: the format, and its document form in the pipeline with a voice summary |
| 14.2 | **The regiment's tasking**: the rule table with reasons, run by the regiment's NPC officers (traits and quirks) at the evening briefing and on urgent triggers; the division's priorities and the front data per campaign |
| 14.3 | **The battery commander's planner**: the sequence, positions on the map with sight along the attack directions, sectors, dummies, regime, the oral combat order, the ideal timeline; the NPC battery commander, and the helper's shadow for a player |
| 14.4 | **The working map** in the PU-12M: in simcore the marks with their source and time; in Unreal the map beside the board |
| 14.5 | **The mission kinds** mapped to the battery's orders and drills; the end of a task (its time, a relief, the covered unit's move, the battery out of action) |
| 14.6 | **Objectives and the mission debrief**, grouped by the player's two outcomes (assets kept, freedom of action denied; `03` §6), each with its why chain; the mission kind "cover a rebuild" |

**Done when:** `mission_demo` runs a cover task headless: the regiment's order at 19:00 with its reasons, the NPC
battery commander's sequence, the march, occupation and «к бою готов» by the ready-by time. A person as battery 21's
commander receives the order in person at the briefing, works the map, and gives the oral combat order to NPC vehicle
commanders who carry it out.

### Phase 15: the continuous mission (`05_THE_SYSTEMS.md` §6-8, §13)

| # | Task |
|---|---|
| 15.1 | **`BatteryState`** in the campaign, replacing the posture-only battery: carried into and out of every session |
| 15.2 | **`MissionSession`** and the `MissionScenarioBuilder`: one runner for the whole task and on into the next (the battery's days continue in it), built from the task and the battery's state |
| 15.3 | **Packages entering live**: the runner's `AddTargets`, `AddJammer` and `AddTrigger`; the raid builder split into its scenario part and a `PackageInjector`; the bridge per injected package; `AirCampaign` handing packages to the session instead of stopping at one pending mission |
| 15.4 | **Time**: the director's compression and wake events; the coarse quiet step; both measured |
| 15.5 | **Levels of detail**: the other batteries at L1; L2 sessions headless; `war_calibrate` comparing L1 and L2 on the same packages (closes `09_BALANCE.md` §6) |
| 15.6 | **Districts** for the rebels' and the neutral's fronts, so the session plays on real ground wherever the battery fights |
| 15.7 | **Saves** of a mission in progress |
| 15.8 | **Unreal**: the session in the level from the order to the report; the campaign screen hands over to it |

**Done when:** one task runs as one session headless from the order to the end, with three packages entering live, a
reload and a relocation between them. An injected package's outcome matches the same raid built as a scenario, within
`war_calibrate`'s bounds. A person plays a 24-hour cover task at battery 21, woken only when something comes.

### Phase 16: the enemy around the battery (`04_WHAT_HAPPENS_AND_WHY.md`, `05_THE_SYSTEMS.md` §9)

| # | Task |
|---|---|
| 16.0 | **Research**: the period's jamming of voice nets (the Pact's radioelectronic combat as `fm100_2_3` describes it; NATO's own documents) and air reconnaissance practice, each side's own documents first |
| 16.1 | **Reconnaissance in the scenario**: photographs of what it overflies, delivered to the enemy's picture only if it gets home |
| 16.2 | **Voice jamming** carried by packages where 16.0 supports it, in the nets' band; the nets' drills answer; the jammer lines on the working map |
| 16.3 | **ELINT and COMINT** from the session's real emissions each minute and its real radio calls (P3) |
| 16.4 | **Each faction as its file has it**: the rebels and the neutral without jammers or SEAD; the Pact's reconnaissance leads and suppression groups; NATO's decoys first |
| 16.5 | **Balance**: `mission_balance`, each faction × mission kind |

**Done when:** an RF-4C shot down before it gets home leaves the next strike planned on the old fix, and the reveal
says so. A Pact strike's jammer takes the battery net down and the commander's spare-frequency drill brings it back.
A battery that radiates through a NATO morning is found by noon.

### Phase 17: our side around the battery (`02_WHAT_WE_DEFEND.md` §2, `05_THE_SYSTEMS.md` §10)

| # | Task |
|---|---|
| 17.1 | **Neighbours' fire** on injected aircraft crossing their rings (the ZU-23 at the bridges, battery 22, the Strela platoons, the S-125), logged as theirs, heard on the net or seen |
| 17.2 | **Our fighters**: the friendly corridor in the order, IFF codes and their changes, the fighter regiment's notices through the pipeline, friendly aircraft in the scenario; friendly fire when the notice or the codes did not arrive |
| 17.3 | **The warning net and the regiment's radars** carried through the session, with their outages |
| 17.4 | **Supply over the real roads**: the TZM and the ATZ from the stocks, round wrecked bridges |
| 17.5 | **The regiment's other four batteries** as units with their own tasks and NPC commanders, at L1 |

**Done when:** a raid that crosses the bridges' guns loses an aircraft to them and the net says so. A MiG-21 in the
corridor answers IFF on the player's scope, and a battery left without the notice fires on it. The TZM's run takes
longer the day the road bridge falls.

### Phase 18: the after-action loop (`05_THE_SYSTEMS.md` §11-12; `docs/command/01_PRINCIPLES.md` 9)

| # | Task |
|---|---|
| 18.1 | **The report from the records** (P9) at the end of every engagement and day, sent up by the post's means |
| 18.2 | **The regiment's picture** from the reports: the analysts' patterns, the enemy's estimated aims and phase, a trust per unit kept by checking records against each other |
| 18.3 | **The next order** from that picture: 14.2's rules fed by 18.2, not by the truth |
| 18.4 | **Consequences delivered** as facts through the pipeline: the bridge stood, the train passed, the depot is empty |
| 18.5 | **Our side's journal** in the air war from the reports |
| 18.6 | **A person plays a campaign at battery 21 through**, in a seat and in command |

**Done when:** a battery whose radiotelephonist lost the 05:12 raid sends its regiment's next ambush to the wrong
corridor; a battery whose records check out is given the hard sector; the debrief shows the chain from the record to
the order. A person plays a week against NATO at battery 21 and can say why every order came and why every raid did.

### Phase 19: the crew as people (`06_THE_CREW_AS_PEOPLE.md`)

| # | Task |
|---|---|
| 19.0 | **Research**: the period's daily routine, rest on duty and meal norms, and how many people each readiness level keeps at the posts (`pvo_bu_ix_1984` and other operator documents); public human-factors studies of sleep loss and vigilance for the needs' effects |
| 19.1 | **The roster**: every member of battery 21 as a person with qualifications, experience, condition, place, duty and knowledge (`data/crew/battery_roster.json`) |
| 19.2 | **Needs and the effective tier**: sleep, food, morale and health as multipliers on the skill dimensions, like `stress_factor` |
| 19.3 | **The duty roster, and readiness as a computed time**: who is on watch at each readiness; the time to the posts from where each person is (`crew_mobility.hpp` legs, waking) |
| 19.4 | **Progression per person**: experience per seat, classes by threshold and exam, cross-training, people moving up into posts, replacements as recruits |
| 19.5 | **Whom the player plays**: any fit member of the battery for a mission or an activity, with that person's knowledge; the main character's career apart |
| 19.6 | **Needs heard, not shown**: reports and voices ("the crew has not slept for two days"), slower acknowledgements |
| 19.7 | **Balance**: `crew_balance` with needs: a rested trained crew meets the published reaction time, the same crew after two nights awake does not |

**Done when:** a 1st-class operator kept awake for two nights works like a 3rd-class one, and their vehicle commander
says so; readiness No. 1 from a sleeping battery takes as long as its people's walk; a person plays the driver on a
night march and the radiotelephonist the next morning, each as tired as that person really is.

### Phase 20: between fights (`07_BETWEEN_FIGHTS.md`)

| # | Task |
|---|---|
| 20.0 | **Research**: the textbook's maintenance pages (`voenizdat_osa_1991` pp. 206-224), the PU-12M's and the vehicles' servicing, the battery's daily routine |
| 20.1 | **Activities as data**, run as jobs at places: who, where, what they leave open or away, how long, what they give, how fast they are dropped, what is lost |
| 20.2 | **Readiness computed** from every person's and every piece of equipment's state, so the debrief can say why it took as long as it did |
| 20.3 | **Wear and faults**: hours radiating, turbine hours, kilometres, held missiles; fault chances growing since the last maintenance, through the consoles' fault model and the drill master's recovery |
| 20.4 | **The schedule**: the NPC battery commander's daily routine as a planner, given out in the morning orders; the player's schedule in the PU-12M; the helper's shadow; the vehicle commanders' own crews' shifts |
| 20.5 | **Interruptions**: alarms, warnings, raids, orders breaking into any activity or task; drop, fight, resume or reschedule |
| 20.6 | **The regiment's day**: the technical battery's tests and loads, the rear's runs, the briefing and courier routines; the battery run by its deputy while the commander is at the briefing |
| 20.7 | **Balance**: `mission_balance` with scheduling strategies against each adversary: readiness, faults, fatigue and losses |

**Done when:** a radar opened for maintenance at dawn against "the dawn raider" leaves its launcher out of the first
minutes of the raid, and the debrief says why; a battery that skipped maintenance for three days has more faults than
one that didn't; a person as battery commander plans a day against the Noric Republic and another against NATO, and
the two days look different for reasons they can name.

### Phase 21: reach, doctrine, and command under strain (`08_COMMAND_DOCTRINE_AND_REACH.md`)

| # | Task |
|---|---|
| 21.0 | **Research**: the period's doctrine for fighting each kind of enemy (emission by readiness, against ARMs, against TDOA location, against helicopters); command posts' succession, the alternate post and improvisation when contact is lost (`utin_2018`, `pvo_bu_ix_1984`, other operator documents) |
| 21.1 | **The enemy's reach as game logic**: depth limits, the hours its aircraft can fly, its sorties, air superiority after its phase changes; a battery moving by day under air superiority may be seen by the enemy's aircraft over the area, as a pilot's report |
| 21.2 | **Our threat estimate** by area and hour, built from the reports; read by the doctrine, the tasking and the schedule, never the truth |
| 21.3 | **Doctrine profiles** per adversary (`data/campaign/doctrine.json`): readiness by hour, emission, moves, deception, cover priorities, ambushes, rest and maintenance windows, radio and road discipline; read by the tasking (14.2 uses one default until this), the order and the schedule; changed by the regiment's picture at the pace of its officers' characters |
| 21.4 | **Command that breaks**: posts whose holders fall (the roster's people, 19.1), command posts as objects (the regiment's CP added), the PU-12M, wire, radio, couriers, a commander away |
| 21.5 | **Improvisation**: contact clocks for every NPC commander; readiness on its own initiative, independent search and fire in the sectors, the last order and its intent, the next means, the next senior; the helper's shadow for the player |
| 21.6 | **Reconstitution**: communications back, posts refilled by succession then replacements (P8), the alternate post, units merged, orders that confirm or replace the improvised ones |
| 21.7 | **Balance**: each adversary with the regiment's CP lost on day 2; how long batteries fight on their own and how well |

**Done when:** against the rebels nothing comes beyond 80 km and the battery is seldom moved, while against NATO after
the rollback a battery moving by day is found; the first ARM attack changes the regiment's doctrine within a day, faster
under a flexible chief of staff; with the regiment's CP destroyed, battery 21 takes readiness No. 1 on its own, fights in
its sectors, sends a runner, and receives the alternate post's orders when it reports.

## 3. Order

```
gate (M2, A10/A12 played, P1-P3) --> 13 why --> 14 the battery's mission --> 15 the continuous mission --+--> 16 the enemy around the battery --+
                                                                                                       +--> 17 our side around the battery ---+--> 18 the after-action loop (with P9)
                                                                                                       +--> 19 the crew as people ------------+--> 20 between fights
                                                                                                                                              +--> 21 reach, doctrine, command under strain
```

13 and 14.0-14.1 can overlap. 16, 17 and 19 touch different code and can run at once. 18 needs P9. 20 needs 15 and
19 (people and their legs). 21 needs 17 (our side) and 19 (the holders of posts as people); its doctrine profiles can
start earlier, since 14.2 runs on one default profile until 21.3.

## 4. Milestones, and what is not planned

**M5, the battery's war:** a person commands battery 21, or crews a vehicle of it, through a campaign against one
adversary. Every task comes as an order with its reasons. Every raid, reconnaissance flight, jammer and ambush around
the battery has a cause the debrief can show. The debrief shows the two things the battery changed: what it kept
standing and what it cost the enemy to work near it. The battery's records decide what its regiment learns next.

**M6, the battery's life:** the same campaign with the battery's people tired, hungry, heartened or shaken, their day
planned by the player against the enemy's habits and broken into by its raids, our doctrine fitted to the enemy, and
command that breaks and is rebuilt. After 19, 20 and 21.

**Later, not planned here:** the regiment's posts for the player; the neighbours at full detail; our air force as a
planner; a ground war simulation; an economy; anything beyond one enemy at a time.
