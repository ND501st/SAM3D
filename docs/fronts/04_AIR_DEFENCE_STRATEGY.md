# 4. Our air defence strategy (F4)

The project owner's third point ends: the ground situation feeds our national air defence strategy: where to defend,
where to take the offence, and in which sectors. This file designs how our air defence commanders set each sector's
priority from what reached them of the ground war (F3) and of the enemy's air front (F2), and what follows: SAM units
moved, fighters given by sector, the regiment's tasking and the battery's task.

The research is `docs/research/fronts_2026-11/our_side.md` (troop air defence with the defence and on the move, the
national PVO, priorities as the ground war moved) and `air_ground_cases.md` §4 (how fast coverage was moved and
rebuilt), and the synthesis's §2.6.

## 1. What exists

- **The chain** (`data/command/chain_pannonia.json`): the national sector's command post (`sector_cp_duty`), the
  fighter regiment's duty officer, the army's air defence chief (`army_ad_chief`), the division's (`div_ad_chief`), the
  SAM regiment's commander and chief of staff (`rgt_commander`, `rgt_chief_of_staff`), the battery's commander.
- **SAM units' moves** by a timer (`AirCampaign::Relocate`): every `relocate_h` a unit with prepared positions takes
  another at random, none within 8 km of the front; one without takes a random point `relocate_m` away. No commander,
  no reason, no gap.
- **Fighters** as one number per front (`FrontDef::friendly_fighters`, 0-1), read by the planner's escort need, the
  route planner's fighter threat and the abstract air fight (`air_fight.hpp`); our fighter regiments' duty and GCI in
  sessions (track B10, B8).
- **The regiment's tasking** (`campaign::RegimentTasking`, `regiment.hpp`): a rule table on the regiment's picture from
  the campaign's journal, with two triggers (`no_task`, `enemy_knows_battery`); a new task given into a running session
  by `ScenarioRunner::GiveTask`.

## 2. Sector priorities

Each of our AD commanders who holds a part of the defence keeps a priority per sector of his part: **defend**, **take
the offence**, or **economy**.
- The **national sector's command post** for the national PVO's objects (S-75 and S-125 class units, radars, fighters):
  object and object-zonal defence by importance (`vr_lhotsky_1985` p. 38), covering our troops on home ground
  (`vr_popelka_1983` p. 53).
- The **army's air defence chief** for the army's troop air defence (the divisions' SAM regiments, the army's Kub
  units): the troops first, the army's main effort first.

The rules, a table moved by the commander's traits:

| Priority | When (on the reports he holds) | What it means |
|---|---|---|
| **Defend** | the sector has the main thrust (the reported enemy strength there is the front's largest), or national objects of high importance lie within the enemy's reach there, or our troops there are counterattacking | the share of the main thrust: 60 % of the air defence to it (`andersen_1979` p. 40), the national object list kept covered |
| **Take the offence** | the enemy's air front is reported deeper there (F2.5), and our coverage there is thin but the enemy's low-level work is dense | an ambush: mobile SAMs moved forward unannounced under emission discipline to where the enemy's low-level aircraft now fly; fighters given the sector |
| **Economy** | a quiet sector, no thrust, no air front movement | what remains: at least one fire unit in each object's cover, the rest given to the other sectors |

At most one change of a sector's priority a decision (6-12 h for the national sector, the army chief's every 2-4 h
with his division commanders' rhythm), and only on reports newer than his last decision. Traits: a bold commander
takes the offence at a lower threshold; a methodical one waits for a second report; a rigid one holds the pre-war
plan longer. Each decision is an order, a fact through the pipeline to the units it moves (`FactKind::Order`), and an
entry in his post's records.

The written priority table between national objects and our troops is an open item (`our_side.md` 6.3). The design
keeps the national PVO's object list and gives our troops the main thrust's share.

## 3. Moving SAM units

Under `ad_strategy.mode` 1 a national SAM unit moves only on its commander's order, replacing `Relocate`'s timer
for those units (the timer's random hops stay as the unit's own move between its prepared positions inside its
sector, which the period's units did after firing).

| Rule | Value | Source |
|---|---|---|
| Long- and medium-range SAM units | move whole, 2-3 times a day at most, at 80-100 km a day; an army's 2 moves a day, 3 h at most | `mt_rozhdestvenskiy_depth` pp. 6-7 |
| A fixed SAM battalion's move (S-75 class) | 30-40 km in 10-15 h, at night: 2.5 h to pack, 3 h to set up, 2-2.5 h of checks | `mt_1973_nvn` pp. 10-11 |
| Mobile batteries (Kub, Osa) | leapfrog: at least one stays in its firing position | `fm_100_2_1_1984` p. 11-9; `andersen_1979` p. 45 |
| The gap while units move | covered by organic air defence and fighters | `mt_lavrinovich_mdex` pp. 16-17; `andersen_1979` p. 45 |
| Prepared positions | about two per unit | `npic_1973_sam_egypt` |
| Radars | a first line of radar companies within 70-100 km of the forward edge, a second 50-70 km behind; a radar company moves 15-20 km in a night | `mt_rozhdestvenskiy_depth` p. 6; `mt_1973_nvn` pp. 10-11 |

**AAA** stays with the object it defends (the territorial defence's guns, `Aaa` objects): it moves only when its object is evacuated or overrun, and the army's own guns move with their divisions. **Radars** move by the same orders as the SAM units, to keep the field ahead of the line as it moves.

A move is to one of the unit's prepared positions (`TheatreObject::prepared`, anchored by F1) or a place the
order names, on the roads (`nav::RoadGraph::Find`, cached), packed for the march, emitting nothing on it, and set up
on arrival. While packed or on the march it is out of the coverage. **A unit about to be overrun** (the reported line
of contact within its reach of its position) is ordered back before it is: if the order is late, it is lost with the
ground.

## 4. The Osa regiment and the battery

The divisions' SAM regiments move with their division: the regiment's commander takes his division's orders and the
army's air defence chief's priorities.
- An Osa battery in defence stands 3-5 km behind the forward edge, pairs up to 3 km apart, 1.5-3 km from the troops it
  covers, with 1-2 reserve positions (`voenizdat_osa_1991` pp. 234-238). As the line moves the regiment moves its
  batteries so as to keep that.
- A battery changes position only on the SAM regiment commander's order (`voenizdat_osa_1991` p. 238): the regiment's
  tasking is the only way the battery moves.

The regiment's tasking (`RegimentTasking`, `TaskingRules`) gains triggers, each read from the regiment's records, as
the two it has now:

| Trigger | The task |
|---|---|
| `sector_priority` | the army's air defence chief changed the sector's priority: cover the main thrust, ambush, or economy positions |
| `covered_troops_moved` | the division the regiment covers moved back or forward (a withdrawal or counterattack ordered): a march to the new positions behind the new forward edge |
| `counterattack` | the division counterattacks: cover the counterattack force on the march and at its deployment lines |
| `security_zone_withdrawal` | the security zone's units withdraw past the battery's position: move back by pairs, one covering |

Each task goes to the battery on the battery net as tasks do now (14.1), so the player hears it in the first person.

## 5. Fighters by sector and the sharing of the sky

- `FrontDef::friendly_fighters` becomes a per-sector cover the national sector's command post sets each decision: its
  fighters given to the sectors by priority, more to defend and to the offence, less to economy. The planner, the
  route planner and the air fight read `AirCampaign::FighterCoverAt(pos)`; at mode 0 it returns the front's number, so
  nothing changes.
- The sky is shared by sector (each at least 60°, `vr_krupicka_1984` p. 62): the method set by the PVOS division's
  commander or the front's air force and air defence commander (p. 63); fighters into a SAM zone only by the PVOS
  formation commander's decision (p. 64). The national sector's post's priority sets which sectors are the fighters'
  and which the SAMs': the existing own-air rules (`own_air.json`, B10) read the sector's sharing.
- Our fighter regiments' alert pools (B8) take their duty from the sectors given.

## 6. Rebuilding coverage

| Rule | Value | Source |
|---|---|---|
| Unopposed rebuild | from 15-25 to 58-61 operational sites in about 8 weeks, mostly into prepared sites: about a third of the lost fire units back every 2-3 weeks | `frus_1969_76_v23_d179` p. 615 |
| Under daily attack | a refill overnight is destroyed the next day | `lambeth_1984` p. 8 |
| New mobile units into a lost area | about 3 weeks | `cordesman_1982`; `werrell_2005` p. 155 |
| Air defence losses in an operation | 25-30 % irrecoverable | `mt_rozhdestvenskiy_depth` p. 7 |

A destroyed unit's replacement is the theatre's repair (`repair_h`) as now; F4 adds only where it goes (the sector's
priority) and that it needs the sector to have been quiet of SEAD for a day (game value), so a refill under daily
attack is wasted as it was.

## 7. What the enemy sees of it

The enemy learns of our moves only by its intelligence: a unit that has moved is where it was in the enemy's picture
until found again (`IntelPicture::Decay`), and an ambush is an ambush because its sites are not in the picture. The
air front (F2) reads that picture, so a sector we re-cover raises its cover only once the enemy finds the new sites or
loses aircraft to them. That is the player's lever: a battery that survives and keeps threatening holds the enemy's
air front back in its sector.

## 8. Data

- `data/campaign/ad_strategy.json`: the commanders (positions in the chain) and their traits' pools, the priority
  table, the shares, the decision rhythms, the move norms with their sources, the rebuild rules.
- `data/campaign/tasking.json`: the regiment's new triggers.
- The theatre's sectors (F2.1) name which national objects lie in each.
- `data/gameplay/air_war.json`: `ad_strategy.mode`.

## 9. Code

- `simcore/campaign/ad_strategy.hpp` (new): `AdCommander`, `SectorPriority`, `AdStrategy::Decide` (pure: the facts
  his post holds, his traits, the time in; orders out), the move planner for national SAM units.
- `AirCampaign`: under `ad_strategy.mode` 1, `Relocate` skips the units that move on orders; a new `MoveUnit(id, to,
  depart_h)` packs, marches and sets up a unit by the norms; `FighterCoverAt(pos)`.
- `campaign/regiment.hpp`: the new triggers in `RegimentTasking::Decide`.
- `command/air_war_feed.hpp` or the ground feed (F3.8): the orders as facts, the unit's arrival as a report.

## 10. Cost

A handful of commanders deciding every few hours, each over the facts his post holds in the last day: under 5 ms a
campaign day (README). Unit moves are a route each, cached.

## 11. Tests

- **On reports only.** A main thrust that has not been reported does not move the priority; the same thrust reported
  does, after the decision delay.
- **The norms.** An S-75 class unit's move takes 10-15 h; a mobile battery's leapfrog keeps one firing; no unit moves
  more than three times a day.
- **Overrun.** A unit ordered back in time survives the line passing; one ordered late is lost.
- **Fighters.** At mode 0 `FighterCoverAt` is the front's number everywhere; at 1 the sum over sectors equals the
  front's force.
- **The regiment.** A division withdrawing gives the battery a march task on the battery net.
- **Save.** A save without `ad_strategy_mode` replays at 0.

## Done when

Our AD commanders set sector priorities from the reports they hold, SAM units and fighters follow by the period's
norms, the regiment and the battery follow by order, the enemy learns of it only by its intelligence, and old saves
replay unchanged.

## As built: F4.1 (22 Nov 2026)

Our air defence commanders keep a priority per sector and give it as orders through the pipeline (section 2,
`simcore/campaign/ad_strategy.hpp`: `AdStrategy`, a member of `CampaignFeed`, so whoever pumps the campaign gets it; rules
and commanders in `data/campaign/ad_strategy.json`, every value with its source, the game values labelled). Only at
`ad_strategy.mode` 1 (`data/gameplay/air_war.json`, default 0; the save keeps it as `ad_strategy_mode`, an old save loads
at 0), on a front with a line (F2.1) and a chain that has the commanders' posts; at 0 nothing is loaded, read or added.
F4.1 only decides and orders: nothing moves on the orders yet.

**The commanders** (the data's `commanders`). The national sector's commander, Colonel Ferenc Halasz, at its command post
(`sector_cp_duty`), for the national PVO's objects, a cycle of 6-12 h (`mt_lavrinovich_mdex` p. 9), methodical and rigid
(0.7, 0.65), his orders to the fighter regiment's duty officer and the army's chief of air defence; the 3rd Army's chief of
air defence (`army_ad_chief`, the chain's holder, Colonel Milan Hrubes), for the troop air defence, a cycle of 2-4 h (his
division commanders', `fm_100_2_1_1984` p. 5-14), his orders to the division's chief of air defence, the SAM regiment's
commander and the national sector's post. Traits move a threshold by at most `trait_shift` (0.2) of its range, as the
ground war's commanders: the cycle (a methodical man's longer: 9.5 h and 3.0 h with these), the offence's depth (10-20 km
of air front beyond the line; bold lower), confirmations (a methodical man, 0.6 or more, waits for a second report; a
rigid one, 0.6 or more, one more before he leaves the pre-war plan).

**What he decides on** (first person, the written record). Only what reached his post by the hour he decides
(`InfoNet::Known` with its hour at or before his), never the ground war's or the air campaign's state:
- the axes our units reported (F3.8): the army's summaries' latest depth and pace of each axis, the enemy in contact as
  the units believed him, our counterattack on it, and any unit's report that passed through his post;
- our commanders' counterattack orders (`sitorder:`);
- the army's air defence staff's estimates of the enemy's air front (F2.5's `FactKind::AirFront`);
- our SAM sites' wrecks reported (`OwnDamage` facts), out of his count for the site's repair time;
beside his own side's plan: where the theatre's national objects and SAM fire units stand (the order of battle he holds),
each placed in a sector by the front's line; the line he believes is the base line less the reported depth.

**The rule table** (`RulePriorities`, pure). Defend where the main thrust is (the most enemy in contact he holds, the deeper
axis on a tie; held at the line or not), where national objects worth 0.7 or more lie within 30 km of the line as reported
(`mt_kurkov_tank_army` p. 9: a day's advance after a breakthrough), or where our troops counterattack; take the offence
where the staff's estimate of the air front, under 48 h old, lies past his offence depth and our cover there is thin
(under 2 fire units, a pair that can leapfrog); economy elsewhere. **The pre-war plan** is the same table at hour 0 on what
he holds before the war (the objects within reach of the border). **One change a decision** (`DecideChange`): on his cycle
(the campaign's hour on his rhythm, never every step), only if a report reached his post since his last decision, at most
one sector, ranked defend, the offence, economy, then the enemy's strength, then the objects, then the sector's index;
the change needs his confirmations on successive reviews asking the same. `PriorityShares` gives the air defence's shares
by priority (defended sectors 60 % together, `andersen_1979` p. 40; the rest by weights 2 and 1), for F4.3.

**The order.** Each change is an entry in his records (`Decisions()`: the hour, the sector, from, to, why, the facts it
rested on with the hour each reached him) and an order (`ad_order:<commander>:<n>`, `FactKind::Order`, a document,
priority, subject `ad_priority:<sector>`, `Fact::to` the data's addressees) from his post `order_work_h` after his decision
(1 h the sector's, 0.5 h the chief's), its text what the priority means there and why. The pipeline carries it with its
delays; `PriorityKnownBy(commander, post, sector)` gives the priority a post holds by his orders that reached it (what
F4.4's tasking is to read).

**Cost.** Decided only on the cycle and when a new fact reached the post; each source (summaries, reports, orders,
estimates, wrecks) read from a cursor, with a short list per commander of what has not reached his post yet (the scans
F3.8 left to F4 as `LatestKnownBy`, now incremental). Deterministic: a decision hour is taken only below the watermark
(the earliest hour an unresolved package could still add a fact), so a campaign pumped every step and one pumped once
after its load make the same decisions and orders (tested).

**Mode 1 on the nation** (`fronts_bench data --ad --log --days 10 --seeds 3 warsaw_pact nato`: `ground.mode`,
`air_front.mode` and `ad_strategy.mode` 1, each campaign twice, the commanders on and off):

| Faction | Reviews | Changes | By sector | Delay, newest fact at his post to the order at the regiment (or the fighters) | Of it, to his decision | From the fact's own hour | Basis not held by his post |
|---|---|---|---|---|---|---|---|
| Warsaw Pact | 124 | 0 | the pre-war plan held: rava_reservoir and tisava defend, hradec, northern_forest and ne_crossings economy | - | - | - | 0 |
| NATO | 123 | 6 | western_crossings economy to defend, by the chief and then the sector, in each seed | 3.50 h | 2.40 h | 3.76 h | 0 |

Priorities over time, NATO: the army's chief took western_crossings to defend on D2 22:34, D4 23:12 and D5 11:22 (seeds 9,
8, 7: the enemy in contact there 2.7-3.6 of full strength, held at the line), the order at the SAM regiment's commander an
hour later (D5 09:10 the newest report, 11:22 his decision, 12:23 the order at the regiment); the sector's commander,
methodical and on a longer cycle, a day later (D3 22:22, D5 21:46, D6 16:43; the order at the fighter regiment's duty
officer about 1.2 h after). Every other sector stayed economy: upper_lenta's axis is 75 km deep at 26-27 km a day but
against 1.0-1.1 of the enemy's strength and no national object in reach, so the main thrust stayed western_crossings.
The Pact's pre-war plan already defended its two sectors with the thrusts (tisava 61 km deep against 2.8-3.4, three
objects in reach; rava_reservoir 20 km against 0.8-1.3, two objects), so its 124 reviews changed nothing in 10 days. No
estimate of the air front reached either post in these campaigns (the offence path is the tests'). Every fact a decision
rested on was held by the deciding post by its hour (0 not). The air war's journal is the same with the commanders on and
off in every seed (they order; nothing reads the orders yet).

**Cost** (`fronts_bench data --ad`, MSVC Release, std::clock): the pipeline's pump a campaign day with the commanders and
without, the Pact 13.367 ms against 13.300 (0.067 ms for them), NATO 11.733 against 11.500 (0.233 ms), against the 5 ms
budget. At mode 0 the pump returns at once on the mode: the gates (`fronts_bench data --ground --gate`, `--ground --gate --player-sector battery`, `--feed --gate`) pass as before (the ground war 0.016-0.067 ms a campaign day by faction; the neutral's campaign under std::clock's resolution, 0.06 ms a day against the absolute 20).

**Identity.** At `ad_strategy.mode` 0 nothing is loaded or added: `air_war_balance data 20 7` and `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` are byte-identical (`cmp`) to track F's tip before it (9c3b0ba, built the same way); at mode 1 the air war's journal is the same with the commanders on and off (the bench, every seed, and the test); the replay fingerprint (`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages) holds.

**Tests** (`test_ad_strategy.cpp`, 6): the data loads and the mode is 0 in it; the rule table and its tie-break by hand,
the offence's depth by trait (13, 15 and 17 km bold, plain and timid), the cycle and the shares; one change a decision,
the methodical man's second report, the rigid man's pre-war plan, a different ask restarting the count; on the nation
(the Pact, seed 7, 36 h) decisions only on the cycle and one an hour at most, every basis fact held by the post by its
hour, the order's fields, the order at the SAM regiment's commander and never at the battery (which is to hear of it by the regiment's tasking, F4.4),
`PriorityKnownBy`, and the same decisions and deliveries pumped once; with the reports off no decision while the truth
moves, and mode 0 adds no fact and leaves the journal identical, the save keeping the mode and an old save loading at 0;
on the district with a credulous staff, the army's chief takes the offence on an estimate of the air front that had
reached him (66.9 h).

**Left:**
- The national sector's post does not receive the air defence staff's estimates (their audience is the division's chief
  of air defence's tree); only the army's chief can take the offence on them until the chain sends them up.
- Nothing moves on the orders: national SAM units and radars on orders by the norms (F4.2; the national units have no
  posts in the chain, so the sector's orders go to the fighter regiment and the army's chief), fighters by sector from
  `PriorityShares` (F4.3), the regiment's tasking on `PriorityKnownBy` (F4.4).
- The commanders' orders are not in each other's picture: the army's chief does not read the sector's orders, nor the
  sector his, beyond what each decides himself.
- The believed line is the base line less the reported depth by axis (F3.8's left: the line by station is not a fact).
- Wrecks reported are out of the count for the site's repair time, not until a report that it is back.
- Unreal needs no call: the data loads from the campaign's data directory when the mode is 1.
- The thresholds, the traits and the rhythm are first values; balance is F6.
