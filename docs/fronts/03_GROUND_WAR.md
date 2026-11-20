# 3. The ground war (F3)

The project owner's third point: a unit that loses too many of its tactical assets may retreat, dispersed or as a
unit; one that is not taking many losses attacks and moves the front forward; its own officers decide, as the air
officers do. This file designs the ground war that makes the line of contact (`02_AIR_FRONT.md` §2) move.

The research is `docs/research/fronts_2026-11/pact_ground.md`, `nato_ground.md` and `our_side.md`, and the synthesis's
§2.1-2.4. The norms below cite their ids; the topic files give pages and confidence.

## 1. What exists

- **Our order of battle** (`data/command/oob_pannonia.json`, `command::OrderOfBattle`): the 3rd Army with the 7th and
  9th motor rifle divisions, a tank brigade, a brigade, the army's artillery and engineers, each unit with a parent,
  equipment and personnel where known, the theatre objects it owns (`OwnerOfObject`) and its posts.
- **Troops as targets.** The theatres' `Troops` objects (12 on the nation) with a value, hardness, supply
  (`ObjectState::supply`, `SupplyNetwork`) and a working share the enemy's strikes take down. They do not move or
  fight.
- **The enemy's ground side** exists only as words: its war aims (`docs/campaign/01_WAR_AIMS_AND_TARGETS.md`), its
  close support's targets, the front as a fixed line.
- **The route service** (`simcore/nav/`): the country's roads as a graph, routes between anchors, overlays for bridges
  down, movers at three levels (M0-M2), the march plan.

## 2. The line and its measures

The period's words (`fm_101_5_1_1980` pp. 1-52, 1-53, 1-62), used as our side's and NATO's documents use them:
- the **line of contact**: where the two sides are engaged; the one line the model keeps (F2.1's stations and offsets);
- the **FLOT**: our most forward positions; in the model the line of contact on our side;
- the **FEBA**: the forward limit of the main battle area; behind the security zone or covering force; held per
  sector as a depth behind the line of contact while a security zone is in front of it (§5);
- the **FSCL**: NATO's, set by corps on a terrain line (§6).

The line is drawn from the units, never set by hand: at each station the offset is the forward edge of our forward-most
unit there. An attack that advances pushes the offsets over its frontage (a breakthrough 6-10 km wide,
`fm_100_2_1_1984` pp. 5-18 to 5-21), tapering over 10-15 km on each side, so a penetration has a shape. Our
counterattack pulls them back, never past zero: we restore the border and stop (README, the owner's decision 4).

## 3. Units, frontages and levels

### 3.1 The units

Each side's ground units are an order of battle in `command::OrderOfBattle`'s format, so one loader reads both:
- **ours**: `oob_pannonia.json` as it is, its missing equipment filled in F3.1 as labelled estimates, with a
  deployment per adversary (`data/campaign/ground_defence.json`: which formations face which front, in which sectors,
  the security zone's depth);
- **the enemy's**: `data/air_war/ground_<faction>.json`, one per faction, in the same format: formations,
  equipment, commanders, and its plan (§5.2). NATO's a corps of three divisions and a cavalry regiment; the Pact's an
  army of three to four motor rifle and one or two tank divisions (`fm_100_2_1_1984` pp. 4-4 to 4-7); the rebels' and
  the neutral's §8. Every number a placeholder or estimate, labelled, as the factions' aircraft are.

A unit's **strength** is its equipment by type times a weight per type, by Perventsev and Sokolov's method
(`mt_perventsev_balance`), the weights recomputed for 1985's types (SAM3D estimates, `FRONTS_RESEARCH_2026-11.md` §4),
plus its personnel's share. Its **strength fraction** is that over its war establishment. Two more numbers:
**cohesion** (0-1, falls with losses in a short time and air attack, recovers out of contact) and **supply** (from its
supply line, `SupplyNetwork`, as troops' supply is now).

### 3.2 Frontages

| Side | Formation | Defence | Attack |
|---|---|---|---|
| Ours | division | 30-45 km wide, 20-25 km deep | (counterattack only) |
| | regiment | 10-15 km, up to 12 deep | |
| | battalion | up to 5 km, up to 3 deep | |
| The Pact | army | | 60-100 km |
| | division | 20-30 km, 15-20 deep | zone 15-25 km, breakthrough 6-10 |
| | regiment | 10-15 km | 3-8 km, typically 4-5 |
| NATO (estimates) | division | 25-40 km | 10-20 km |
| | brigade | 8-15 km | |
| | battalion | 3-6 km | |

Ours from `vr_poltikovic_1984` pp. 95-96, the Pact's from `fm_100_2_1_1984`, NATO's SAM3D estimates bounded by
`fm_100_5_1976` p. 5-3. A unit holds a run of stations by its frontage; a sector holds the units whose frontages lie in
it.

### 3.3 Levels

| Level | Where | The actor | What moves |
|---|---|---|---|
| **U0** | everywhere, by default | the division or brigade (the enemy's army or corps above it as the planner) | its frontage and posture; its regiments are a list of strengths inside it |
| **U1** | the player's sector and the sectors either side | the regiment and the battalion | each has its own stations, posture and decisions; its strength is its share of the formation's |
| **U2** | in a session, within the player's visual range and on the roads near the battery | battalions and companies as route-service movers (`nav::Movers`, `MoverLevel` M1/M2) | columns on the roads, positions at places; drawn in Unreal (F5.2) |

A formation goes up a level at once when the player's battery's sector or a session needs it; it goes down after it
has been out of need for 6 h, keeping strength, posture and place (the margins of track B's A0-A2, `air_levels.hpp`).
U1 must give the same expected outcome as U0 over seeds (F3.6's test); U2 adds nothing to the combat, only where its
parts are.

## 4. The sector's combat

Each hour, for each **contested sector** (one where the two sides' units are in contact on any station), one
resolution by table. A quiet sector takes the "inactive" loss rate in closed form when it is next touched.

**Power.** For each side:

```
power = Σ over its units in the sector of  strength × posture × supply × cohesion
```

The defender's posture factors (Dupuy, `ciano_1988_qjm` fig. 24): attack 1.0, hasty defence 1.3, prepared 1.5,
fortified 1.6, delay 1.2, withdrawal 1.15. The ground (`CountryMap`): forest (`InForest`), hills and marsh multiply the
defender's power by game values; a river line (`DistanceToRiver`, `NearestCrossing`) by more until crossed. Night and
the weather's visibility multiply the attacker's. The ratio is `R = power_attacker / power_defender`.

**Losses.** FM 101-10-1's daily rates for a division in contact (`fm_101_10_1_1976` table 5-8): attack of a position
4.1 % the first day then 2.2 %, defence of a position 2.2 % then 1.3 %, delay 1.0 %, inactive 1.0 %. They hold at the
reference ratio of 3:1; away from it each side's rate is scaled by `(3 / R)^k` for the attacker and `(R / 3)^k` for the
defender, `k` a game value near 0.5, the scale clipped to 0.25-4. Equipment is lost in proportion, tanks heavier in
the first day of a covering force's fight (`fm_101_10_1_1976` table 7-7: 73 % of a heavily committed covering force's
tanks on day 1, up to 80 % reparable). The hourly loss is the day's over the sector's hours of fighting.

**Advance.** HERO's normalised rates by opposition (`hero_1972_oralfore` p. IV-2), the opposition read from the ratio
(a SAM3D mapping):

| R | Opposition | km a day |
|---|---|---|
| under 1.5 | the attack stalls | 0 (no sustained advance below the threshold) |
| 1.5-2.5 | intense | 2.8 |
| 2.5-4 | moderate | 10.1 |
| 4-6 | slight | 23.3 |
| over 6 | negligible | 54.8 |
| no defender | unopposed | about 70 |

capped by the faction's own norms: the Pact 25-30 km a day breaking through and 40-50 in depth, 50-70 only against a
broken enemy (`mt_kurkov_tank_army` p. 9; `fm_100_2_1_1984` p. 4-4); NATO's exploitation 50-80
(`fm_100_5_1982` p. 8-7). Multiplied by the ground, and by a lognormal factor drawn once per sector and day. A river
stops the line at its bank until the attacker has a crossing: an intact bridge (the air war's `Bridge` objects, their
state) takes hours, a cut one longer (game values); the engineers' pontoon park shortens ours.

**The air in it.** The enemy's close support and interdiction act through the air war (§6): its strikes on our `Troops`
objects come off our units' strength. Its result is checked as a band, not set: the air's share of a sector's losses
about 5-25 % (`FRONTS_RESEARCH_2026-11.md` §4; F6).

## 5. Commanders and their decisions

### 5.1 Who decides

Commanders are people as the air war's officers are: `airwar::OfficerDef` with `Traits` and skills, drawn from a pool
(`OfficerPoolDef`) per side. One per formation at U0 (army or corps, divisions, brigades) and per regiment at U1. Each
decides at his next decision time, from FM 100-2-1's reaction times (division 2-4 h, regiment 1-3 h, battalion
25-60 min: `fm_100_2_1_1984` p. 5-14), or at once on an event: contact, a breakthrough reported, an order from above,
his unit crossing an effectiveness line. Each decides on **his side's picture** (§9), never the truth. Traits move a
threshold by at most about ±0.2 of its range: a bold man attacks at a lower ratio, a rigid one asks to withdraw later,
a methodical one takes the long end of his planning time.

### 5.2 The Pact and our side

| Rule | Value | Source |
|---|---|---|
| Attack on an axis | at about 3:1 by weighted strength over the axis; 3-5:1 locally at a breakthrough, never WWII's densities | `fm_100_2_1_1984` p. 2-11; `reznichenko_1984` p. 79 |
| Echelons | two against a deep defence, one and a reserve against a shallow one; a reserve about a ninth of the force | `fm_100_2_1_1984` pp. 2-10, 2-11 |
| The second echelon | committed to success, to the axis that moves, not to the one that stalls | `fm_100_2_1_1984` p. 2-11 |
| The plan's depths | the division's day 25-30 km; the army's 100-150 km in 3-4 days | `fm_100_2_1_1984` fig. p. 4-4 |
| Effectiveness | over 2/3: effective; 1/2-2/3: degraded, replaced first; under 1/2 (or about 25 % with command and artillery hit): ineffective, withdrawn and re-formed | `mt_stashek_losses` p. 7; `mt_novoselov_replacement` p. 9 |
| Withdrawal | only with the senior commander's permission | `reznichenko_1984` pp. 49, 152 |
| Counterattack | the next higher commander's; the second echelon from the march into the flank, tanks leading, to a set line | `reznichenko_1984` pp. 177-179; `vr_poltikovic_1984` p. 96 |
| Our security zone | 20-40 km deep, lines 6-8 km apart; a reinforced battalion delays a brigade 1-2 h a line; the army sets its depth, the division runs it and orders each withdrawal | `vr_poltikovic_1984` p. 96; `vr_krizek_1985` pp. 32-36 |
| Our prepared positions | a division 3-4, a regiment 2 | `vr_poltikovic_1984` p. 96 |
| Restoring control after a CP is lost | about 1 h to 2-3 h | `mt_stashek_losses` |

The Pact's plan (the enemy's ground block): its objectives as places in depth, its axes (a route between places, so a
changed map needs no edits), its start day relative to the air war, its echelons. The plan sets where it masses; the
ratio sets how it goes; the commanders change the plan by the rules above: switching the second echelon to the axis
that moves, stopping an axis that has stalled with no echelon left.

Our side defends: the security zone delays and withdraws line by line on the division's order; the main defensive
zone holds; a penetration is met by the army's second echelon or reserve in a counterattack when the army commander
orders it; a division below 1/2 withdraws behind the next when the army commander allows it.

### 5.3 NATO

| Rule | Value | Source |
|---|---|---|
| Attack | seeks 6:1 at the point of decision, 3:1 or better in the sector | `fm_100_5_1976` pp. 3-4, 3-5 |
| Defence | holds while under 3:1; briefly to 5:1 with air and artillery on good ground; then delays | `fm_100_5_1976` p. 5-3 |
| Breakpoints | drawn per unit from ranges by mission: an attack stops at 15-35 % cumulative losses, a defence breaks at 25-50 % (SAM3D estimates) | `clark_1954_oro`; `hero_1982_termination` |
| Covering force | told in time (72 h, 7 days, until the main body is ready); 30-50 % of a division's manoeuvre units | `fm_100_5_1976` p. 5-12; `fm_71_100_1978` p. 5-13 |
| Depth in hours | area of influence: brigade 12 h, division 24 h, corps 72 h | `fm_100_5_1982` p. 6-2 |

NATO's war on us is an attack (the fiction's): its corps attacks at its ratios, its divisions' commanders decide on
their own losses against their breakpoints (no permission needed to stop an attack), its covering force screens its
own line when we counterattack.

### 5.4 Retreat: as a unit or dispersed

The brief's two retreats:
- **As a unit**: ordered (or, for NATO, decided), with cohesion over a line: the unit goes back to its next prepared
  position or the line it was given, at march speed, keeping its strength, in delay posture while in contact.
- **Dispersed**: a unit under 1/2 strength with no order, or whose cohesion collapses (heavy losses in hours, air
  attack on a column), breaks. It leaves the line, its stations go to its neighbours or open; its remnants reassemble
  at a place behind in 1-3 days (game values) with a share of the stragglers lost, and re-form as a smaller unit.

## 6. The air war follows the units (and the FSCL)

- A `Troops` theatre object takes its place and working share from its ground unit (`OwnerOfObject` in our order of
  battle): its position in the campaign's state (`ObjectState::pos`) moves with the unit, its `function` is the unit's
  strength fraction. The theatre's data does not change; F1's anchor stays its peacetime place.
- The enemy's strikes on a `Troops` object come off its unit's strength, by the damage it takes times the object's
  share of the unit.
- Supply lines ending at troops end where the troops are (`SupplyNetwork`'s destination follows the object).
- Our objects behind the enemy's line are **overrun**: out of the air war's targets (the enemy holds them), out of
  ours. Our units leave them before, on the order of F4.
- The relocation rule that keeps a unit 8 km from the front (`AirCampaign::Relocate`, `DistanceToFront`) reads the
  line of contact.
- **NATO's FSCL**: drawn per sector 20-30 km beyond the line of contact in defence, the next terrain line beyond the
  day's objective in attack (`fm_101_5_1_1980` p. 1-49; `fm_6_20_1977` p. D-5; `ota_1987_fofa` pp. 78-79). C2's
  mission label (CAS short of it, BAI either side inside the area of influence, AI beyond) reads it in place of
  `c2.bai_depth_m`.

## 7. The march

At U0 a formation does not march on roads: it stands relative to the line by its posture (its echelon's depth) and
moves with it. Only a regrouping between sectors and a reserve's march to a counterattack ask the route service
(`nav::RoadGraph::Find` between anchors), once per order, cached by the two anchors and the overlay's version; the
time is the route's at the column's speed. At U1 the regiments ask it too within the sector. At U2 the columns are
movers.

The norms: columns by day 20-30 km/h, at night 15-20, cross-country 5-15; 25-50 m between vehicles, up to 100 m under
air attack; battalions 3-5 km apart on a route, regiments 5-10 km; halts of 20-30 min every 2-3 h
(`fm_100_2_1_1984` pp. 5-2 to 5-5); NATO's open column 50-100 m by day (`fm_71_2_1977` p. H-4). Against a large raid
the column leaves the road (`fm_100_2_1_1984` p. 5-5). A column on the road is the air war's target as supply columns
are now (`SupplyColumnMovers`), and bridges down are overlays that lengthen its route.

## 8. The rebels and the neutral

The research gives patterns, not norms. Both are SAM3D estimates for the owner to accept or replace (README).
- **The rebels** (the Rava Liberation Front): brigade-sized groups in the border region, light equipment. Without air
  defence they cannot hold ground against an air force (`cia_1986_nicaragua_af`): they raid where we are thin, mostly
  by night (an attack only where their ratio is high and our air is not near), and disperse rather than retreat as a
  unit. Their advance capped at a few km a day; their line ragged, held by small groups.
- **The neutral** (the Noric Republic): dissuasion on its own territory (`sadauer_schlup_1978`). In the war's fiction
  it makes a limited incursion to its political depth (the politics' `max_depth_m`, 40 km) and then holds; no
  exploitation.
- Or both fronts stay static and only the Pact's and NATO's ground wars move: the ground block of a faction may be left
  out, and its front stays the border.

## 9. Each side's picture

Each side decides on what it believes, per sector:
- its **own** units: where they are and their strength, from its units' reports, late by the reporting delay;
- the **other side's** units: their believed places, strengths and postures, with an age and an error. The enemy's
  belief of our troops is its `IntelPicture`'s `Troops` entries, which its reconnaissance and strikes already keep; the
  ground units it does not see from the air are added by its ground reconnaissance at the line (a game value's error).
  Ours comes from our units' contact reports and the army's intelligence (`army_intel`).
- the **line**: the believed offsets by station.

A believed strength is the truth times a factor drawn once per (sector, report time) with a spread by source. The
picture costs a few numbers per sector, updated when a report would arrive, not every hour.

## 10. Situation reports into the pipeline

Our units report up the chain as the period's staffs did: a division's combat report on a schedule (twice a day, game
value) and at once on a breakthrough, a withdrawal ordered or a commander lost. Each report is a fact
(`command::InfoNet`): the division's report to the army (`army_intel`, the army's staff), the army's summary to the
national sector's command post (`sector_cp_duty`) and to the army's air defence chief (`army_ad_chief`), the division's
to its SAM regiment (`rgt_chief_of_staff`), the regiment's word to the battery. A new `FactKind::Situation` carries the
believed line by station and the units' believed places as a note the posts' views can draw (F5.1); a withdrawal or
counterattack ordered is an `Order`. The facts are written into each post's records as they arrive: F4's commanders and
the regiment's tasking read those records, never the ground war's state. The written record is the war's memory.

A few dozen facts a day for the nation: only what a post of our chain would hold. Far formations' internal orders are
not facts; they wait out a delay table.

## 11. Data

- `data/air_war/ground_<faction>.json`: the enemy's formations (order of battle format), its commanders' pool, its plan
  (objectives as places, axes, echelons, start day), its doctrine's table (ratios, breakpoints or effectiveness lines).
- `data/campaign/ground_defence.json`: our deployment per adversary, the security zone's depth, our commanders' pool,
  equipment filled in for `oob_pannonia.json`'s units that have none (estimates labelled).
- `data/gameplay/ground_war.json`: weights by type, posture factors, loss table, the `k` exponent, advance classes,
  ground and river factors, decision times, report schedule, levels' margins. Every value with its source or labelled
  a game value.
- `data/gameplay/air_war.json`: `ground.mode`.

## 12. Code

`simcore/include/simcore/ground_war/` (namespace `simcore::groundwar`):

| Header | What |
|---|---|
| `ground_data.hpp` | `GroundWarDef`, `GroundPlanDef`, the doctrine and game tables; loaders reusing `command::LoadOrderOfBattle` |
| `units.hpp` | `GroundUnit` (side, level, parent, children, equipment, strength, cohesion, supply, posture, stations, state: in line, reserve, march, dispersed, re-forming) |
| `combat.hpp` | `ResolveSector` (pure: units, ground, hour in; losses and advance out) |
| `commanders.hpp` | `GroundCommander` (an `OfficerDef`), `Decide` by side's rule table, the event queue |
| `picture.hpp` | `GroundPicture` per side and sector |
| `ground_war.hpp` | `GroundWar`: owns the above, `Step(t_h)`, writes the line's offsets, levels U0/U1, the reports out |
| `command/ground_feed.hpp` | the reports into `InfoNet`, as `CampaignFeed::Pump` does for the air war |

`AirCampaign` holds a `GroundWar` only when `ground.mode` is on and the theatre has a country map; it steps it in
`StepTo` once an hour, after the hour's air results and before the next planning cycle reads the line. Every draw is a
fork keyed by (sector, day) or (unit, decision time).

## 13. Cost

The README's budget: 20 ms CPU per campaign day for the whole nation at U0, 15 ms more for the player's sector at U1.
What keeps it there: some 10-15 formations a side at U0, at most 10 contested sectors resolved an hour, about 5
decisions an hour from the queue, routes only on orders and cached, pictures updated on report times. `fronts_bench`
(F3.3) measures it by part.

## 14. Tests

- **The tables.** At R = 3 a day's losses are FM 101-10-1's; the advance classes return HERO's rates; the posture
  factors Dupuy's (hand calculations).
- **Doctrine.** Our unit never withdraws without its senior's order; the Pact's second echelon goes to the axis that
  moves; a NATO attack stops at its drawn breakpoint; a unit under 1/2 with no order disperses.
- **The line.** Zero offsets give the base line bit for bit; a breakthrough tapers; a river holds the line until a
  crossing; a counterattack never passes the border.
- **Levels.** U1 and U0 give the same mean advance and losses over 20 seeds within a band; a formation down a level
  keeps its strength.
- **Keyed draws.** Resolving sectors in another order gives the same war.
- **Off.** `ground.mode` 0, and any theatre without a country map, are bit-identical to before.
- **First person.** Our AD commanders' inputs (F4) come only from facts; a test breaks the truth after a report and
  checks the decision did not move.

## Done when

Each faction's ground war (or the owner's choice for the rebels and the neutral) moves the line of contact by its
units' combat and its officers' decisions, the air war's troops, supply and close support follow it, our side's posts
learn of it by reports, it costs under its budget, and old saves replay unchanged. Its outcomes against history are
F6's.
