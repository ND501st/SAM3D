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
  equipment filled in for `oob_pannonia.json`'s units that have none (estimates labelled; as built, in `oob_pannonia.json` itself).
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

## As built: F3.1 and F3.2 (21 Nov 2026)

Nothing runs the ground war yet: no loader on the campaign's path reads these files, so every balance tool and save
is as before. Proved at the commit: `air_war_balance data 20 7` and `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7` byte-identical to track F's tip before it (8f5fca4), and the replay fingerprint
(`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages) unchanged. F3.3 adds `ground.mode` and
the clock.

**The tables** (`data/gameplay/ground_war.json`, `GroundTables`, `LoadGroundTables`). Every scalar is
`{"value", "source", "confidence"}` and every range `{"min", "max", "source", "confidence"}`; a source is an id in
`docs/SOURCES.md` or `game_value` / `sam3d_estimate`. Weights by equipment type in T-55 equivalents
(`mt_perventsev_balance`'s method, the 1985 weights SAM3D estimates; a type not listed weighs nothing, so support and
air defence count only by their people at 0.01 a man); supply and cohesion floors (0.5, 0.3); Dupuy's posture factors
(`ciano_1988_qjm` fig. 24); the ground's factors on the defender and the advance (forest 1.3 and 0.6, hills, mountains,
marsh, urban: game values); night 0.8 and poor visibility 0.9 on the attacker, a river line 1.5 on the defender;
FM 101-10-1's eleven actions (`fm_101_10_1_1976` table 5-8) with the reference ratio 3, `k` = 0.5 and the clip
0.25-4; its tank table 7-7 (five commitments, three days, 80 % reparable); HERO's classes (`hero_1972_oralfore`
p. IV-2, thresholds 1.5/2.5/4/6 a SAM3D mapping) and the draw's sigma 0.35; crossing 2 h intact, 12 h cut, x 0.5
with a pontoon park; decision times by echelon (`fm_100_2_1_1984` p. 5-14); the trait's shift (0.2 of a range,
README decision 6); cohesion, dispersal, march, report and level values for F3.3-F3.6.

**Places and sectors** (F1, F2.1). Each ground file names its `"place_map"` (`maps/pannonia.json`) and its
`"theatre"` (`air_war/theatre_pannonia_nation.json`). `LoadGroundWorld` reads the map's places (`LoadMapPlaces`) and
the theatre on them; the loaders resolve every garrison, route point, objective (`"at"`) and deployment place
(`"at"`) to its point (`GroundPlace`: the id and the point to the bit) and refuse a place the map lacks, naming it and
the map, as F1's loaders do. Every axis names the sector of its faction's front it attacks in (`"sector"`; the test
checks it is the sector its border crossing projects into, by `LineOfContact::SectorOf`); every unit of ours on the
line holds a run of its adversary's front's sectors (`"sectors"`, each the next along the line; `from_km`/`to_km`
derived from them). The places the ground war needed that the map lacked are ten new sites in `pannonia.json`, after
F1's (`pact_assembly_east`, `pact_assembly_rava`, `pact_rava_gorge`, `nato_assembly_centre`, `nato_assembly_south`,
`nato_lenta_south_border`, `neutral_assembly`, `neutral_hold_line`, `rebels_base_west`, `rebels_base_forest`); two
are border points where no road crosses (the Rava gorge, the Lenta's south), named so rather than as `bx_` road
crossings. A test moves the Pact's assembly area and Mezovar on the map and checks that exactly what names them moves.

**The enemy** (`data/air_war/ground_<faction>.json`, `GroundForceDef`, `LoadGroundForce(data_dir, faction)`): an
`"oob"` in `oob_pannonia.json`'s format (read by `command::detail::OobLoader`), a commanders' pool by rank and the
named commanders, the doctrine (attack and breakthrough ratios, withdrawal only on order, the second echelon to
success, effectiveness lines for the Pact, breakpoint ranges for NATO and the small forces, advance caps, frontages)
and the plan. The owner's open decisions are taken as proposed (SAM3D estimates): the Pact attacks 2 h into its air
operation on two axes (the 35th and 39th MRDs down the north-eastern road on Mezovar and the capital in the Tisava
sector, the 9th TD behind them; the 57th down the Rava gorge in the Rava reservoir sector), NATO after 96 h of
rollback (the 2nd ACR, 1st AD and 5th Pz Div on Kisvar in the western crossings, the 3rd MD on Saldanj in the upper
Lenta); the rebels hold a ragged line and raid by night where the ratio reaches 3:1 (two raids a night, 15 km deep);
the neutral pushes to a hold line 40 km in (in the Lenta marshes sector) and holds. Strengths by the table: the Pact's
army 3634 T-55 equivalents, NATO's corps 3196, the neutral's 504, the rebels' 85.

**Our side** (`data/campaign/ground_defence.json`, `GroundDefenceDef`, `LoadGroundDefence`): the order of battle is
`command/oob_pannonia.json`. The five manoeuvre units that had no equipment there (the 71st, 72nd, 91st and 92nd
Motor Rifle Regiments and the south-western brigade) have it now in the order of battle itself, labelled estimates in
their notes (a tank battalion of 31 T-55, BMP-1s or BTR-60s, an artillery battalion of 18 D-30, of the order of
`fm100_2_3`'s view of the regiment); every type has a basic model, so `test_scenery.cpp`'s check holds, and nothing on
the campaign's path reads equipment. The 3rd Army weighs 977. The doctrine (our lines, caps, the counterattack that
stops at the border, frontages from `vr_poltikovic_1984`, the security zone) and one deployment per adversary: each
unit's role (first echelon, second, reserve, flank screen, support), its run of sectors on the line or, off it, its
place.

**The combat** (`ground_war/units.hpp`, `combat.hpp`): `StrengthOf`, `UnitPower` (strength x the supply and cohesion
factor), `EffectivenessByLines`, `DrawBreakpoint` (a range drawn at `u`, moved by a trait's pull);
`ResolveSector(tables, attacker, defender, conditions, caps, draw, hours)` returns the ratio, the opposition, each
side's daily rate and its loss over the hours (`1 - (1 - daily)^(h/24)`, so a quiet sector closes in one call), and the
advance: the class's rate x the ground x the draw, then the faction's cap (breakthrough against a defence, depth
against a delay, withdrawal or meeting, broken against a broken enemy or none), stopped at a river line's bank.
`CrossingHours`, `TankLossShare` and `DecisionHours` are the other tables. No state, no allocation: a call costs
0.12-0.16 us of CPU (`test_ground_combat.cpp` times a million, Release, MSVC, alone and in the full suite), so 240
calls (10 contested sectors an hour) cost about 0.04 ms a campaign day against the budget's 20 ms. The tests check FM 101-10-1's day at 3:1 (4.1 %
and 2.2 % on the first day, 2.2 % and 1.3 % after), the scale away from it, HERO's five classes and the stall below
1.5, Dupuy's six factors through the ratio, forest and night, a river line that stops the advance at its bank, the
inactive closed form and the tank and decision tables, all by hand.

**Left** to later tasks: `ground.mode`, the clock and `GroundWar` (F3.3; nothing of it was started here); the
commanders' quirks and the pools' use (F3.4); the enemy's equipment types' basic models (F5.2); the enemy's own air
defence units (not in these orders of battle; the air war's objects hold them); equipment and tank losses applied by
type (the tank table is a lookup, `ResolveSector` returns shares of strength; F3.3); the rebels' raids name no sectors
(they raid where we are thin, F3.4 decides where); every weight and NATO's frontages are estimates for F6 to check.

## As built: F3.3 (21 Nov 2026)

The ground war runs. `AirCampaign` holds a `groundwar::GroundWar` (`ground_war/ground_war.hpp`) when `ground.mode`
is 1 (`data/gameplay/air_war.json`'s new `"ground"` block, 0 in the data), the theatre has a country map (only
`theatre_pannonia_nation.json`) and the faction has a ground file; it is built at `Init` from `AirWarData::data_dir`
(the files are loaded at each `Init`, about 13 ms; `Build` under 0.5 ms) and stepped in `StepTo` after the hour's air results and
`Deliver`, before the next plan reads the line. At 0, on the district theatre or with no file nothing is built and no
draw is made. A save writes `ground_mode`; one without it replays at 0 (`save.hpp`).

**The step** (§3.3's U0 only). Each formation is the actor of its order of battle's tree (a division, brigade or
regiment the plan or our deployment names) with its equipment summed by type over the tree, stopping at another actor's,
and its people; its strength is F3.2's weighted sum. Three kinds of work:
- *the event queue*: a binary heap on a vector sized at `Build`, keyed by (hour, sequence): the plan's start, a second
  echelon's commitment, a river crossed, the day's end. A quiet hour costs a look at its top.
- *stepped*: each hour, only the axes attacking are resolved, one `ResolveSector` each, in two phases (every axis's
  sides read from the hour's start, then every formation's losses applied in a fixed order, its axes' summed in
  theirs), so the order the axes are resolved in changes nothing (`SetReverseOrder` proves it). The attacker's power is
  its committed echelons' `UnitPower`; the defender's is ours in the sector's first echelon and flank screens, shared
  between two axes when it holds both sectors, at `clock.security_zone_share` (0.25) while the enemy is in the
  security zone (posture Delay, covering) and whole, with the sector's second echelon, once he reaches the main
  defensive zone (Prepared defence). The ground is the defender's terrain cell at the axis's depth, sampled once along
  its route at `Build` (the map's land cover, then the height above the map's base: hills over 150 m, mountains over
  300 m); the night from the theatre's sun, the poor weather from the campaign's day; a river line within 3 km ahead.
- *closed form*: everything out of the fight takes its rest rate (FM 101-10-1's "reserve" before contact, "inactive"
  in contact) over the hours since it was last touched, when it is next touched (`LossOverHours`); `StrengthAt` reads
  it without changing anything.
No allocation in the hour once built (the log's capacity reserved, cleared by the campaign each hour; only the profile's
day list grows, when profiling).

**Losses by type.** A formation's loss share comes off every holding and its people in proportion; ours in contact
lose tanks by FM 101-10-1 table 7-7 instead (the commitment from the zone and the ratio: in the security zone light,
moderate or heavy delay below 2.5, 2.5-4, 4 and over; in the main zone moderate or heavy defence below and above 4, a
SAM3D mapping), its day counted from the commitment, 80 % of them reparable (tallied; F3.4 or later returns them).
The tanks are the weights marked `"tank": true` in `ground_war.json`.

**Movement and rivers.** The axis advances the hour's `advance_km` (the class's rate x ground x the sector's day's
draw, the faction's cap) up to its objective's depth (or the plan's political depth). A river crossing the route (the
map's rivers against the route's legs) stops it at the bank; the bridge there is the air war's nearest bridge object
within 5 km (standing while its function is 0.5 or more) or, with none, a road crossing on the map; the crossing takes
`CrossingHours` (2 h standing, 12 h down), an event in the queue.

**The line.** The axis's depth is drawn on the stations it reaches: the depth over the breakthrough's half width (8 km
wide, the middle of `fm_100_2_1_1984`'s 6-10), tapering linearly over 12.5 km each side; each station takes the deepest
of the axes there, never negative (`LineOfContact::SetStationOffset`). So `DistanceToFront`, F2.2's cover bands,
F2.3's decision and F2.4's readers (the planner's depths, the routes, C2's BAI area) follow the line; the resolver's
opportunity depth and the played raid's depth now read it too (`resolve.hpp`, `raid_scenario.hpp`, the same to the bit
at zero offsets).

**Who attacks until F3.4** (the minimal rule; §5's commanders replace it). Each axis of the enemy's plan goes in with
its first echelon at the plan's `start_after_h`; its second echelon is committed `clock.second_echelon_after_h` (24 h)
later if the axis has moved (to success), at once if the first is spent, and held back otherwise; an axis halts at its
objective or the political depth, when its attackers are spent, or after `clock.stall_halt_h` (24 h) without advancing
with no echelon left. A formation is spent below its doctrine's ineffective line (the Pact's and ours) or past its
breakpoint drawn per formation (NATO's and the small forces') and leaves the fight to re-form. Our side holds: no
withdrawal, no counterattack, no reserves moved. The rebels' raids are F3.4's: their line stays the border.

**Draws.** The sector's day's draw is a lognormal of mean 1 and sigma 0.35, a fork of the war's root keyed by (sector,
day); a formation's breakpoint by its index. The root is the campaign's seed's own, so the air war's draws are as they
were.

**The journal** (kind `"ground"`, hidden: the truth, which our side learns from its units' reports, F3.8): the
offensive's start on each axis, the second echelon, a river reached and crossed, a formation spent, an axis halted,
and at each day's end the line's depth in every sector it has moved in.

**Tools.** `fronts_bench data --ground [--days 30] [--seeds 5] [--campaign-days 10] [--campaign-seeds 1] [--gate]`:
the ground war alone as the campaign builds it, a campaign day's cost by part (combat, decisions, movement, line;
picture and reports are the campaign's), then whole campaigns at modes 0 and 1 with the ground war's own time, its
share and the line by sector and day; `--gate` exits 2 over any budget. `air_war_balance --cpu` prints each posture's
CPU and, at `ground.mode` 1, the line's depth by sector at each day's end; without it the output is unchanged.

**Identity.** At `ground.mode` 0 nothing is built: `air_war_balance data 20 7` and `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` are byte-identical (`cmp`) to track F's tip before it (4ae43b3, built the same way), and the replay fingerprint (`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages) holds. The district theatre at mode 1 builds nothing (no country map) and is the same war (`test_ground_war.cpp`).

**Mode 1 on the nation** (`air_war_balance --cpu --theatre theatre_pannonia_nation.json --set ground.mode=1 data 3 10`,
the line's mean depth by sector at the day's end, km; 3 seeds, the same for both battery postures):

| Faction | Sector | Day 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8-10 | The axes |
|---|---|---|---|---|---|---|---|---|---|---|
| Warsaw Pact | tisava | 34.7 | 45.9 | 55.3 | 60.9 | 60.9 | 60.9 | 60.9 | 60.9 | the main axis at the capital's depth (61 km) on day 4, halted at its objective |
| | ne_crossings | 26.7 | 35.4 | 42.5 | 46.8 | 46.8 | 46.8 | 46.8 | 46.8 | the main axis's taper (it crosses near the sectors' boundary) |
| | rava_reservoir | 13.9 | 20.0 | 20.0 | 20.0 | 20.0 | 20.0 | 20.0 | 20.0 | the Rava axis at Dravec (20 km) on day 2 |
| NATO | upper_lenta | 0 | 0 | 0 | 0 | 22.9 | 55.4 | 74.9 | 74.9 | from 96 h; the south axis at Saldanj (75 km) on day 7 |
| | western_crossings | 0 | 0 | 0 | 0 | 30.0 | 34.1 | 37.2 | 37.6-38.9 | the main axis slows against our second echelon, stalls about 47 km in (the first seed alone, 30 days) |
| | zlatno | 0 | 0 | 0 | 0 | 4.8 | 5.5 | 5.9 | 6.0-6.2 | the main axis's taper |
| Neutral | border_forest, lenta_marshes | 12.5, 10.9 | 17.9, 15.6 | | | | | | 17.9, 15.6 | the incursion at its hold line (18 km) on day 2, from hour 0 |
| Rebels | none | | | | | | | | | no axes: the line holds (the raids are F3.4's) |

The fastest day anywhere is 34.7 km (the Pact's first, under its 50 km/day depth cap); nothing near a runaway.
Every axis ends at its objective or stalls within ten days; F3.4's commanders, our counterattacks and F6's checks
against history decide whether that is right (the Pact at the capital's depth on day 4 is fast).

**Cost** (`fronts_bench data --ground --gate`, MSVC Release, Ryzen 5 3600, the tool alone on the machine):

| Faction | Mean ms a day | Worst day ms | Combat | Decisions | Movement | Line |
|---|---|---|---|---|---|---|
| Warsaw Pact | 0.0084 | 0.082 | 0.0052 | 0.0015 | 0.0001 | 0.0010 |
| NATO | 0.0186 | 0.071 | 0.0148 | 0.0017 | 0.0003 | 0.0012 |
| Neutral | 0.0038 | 0.035 | 0.0014 | 0.0010 | 0.00004 | 0.0007 |
| Rebels | 0.0022 | 0.0026 | 0 | 0.0010 | 0 | 0.0006 |

against 20 ms (mean over 30 days x 5 seeds) and 60 ms (the worst day): three orders of magnitude under. Picture is
F3.8's (0); reports, the journal's lines in the campaign, add 0.04-0.10 ms over ten days. In the campaign (10 days,
seed 7) the ground war's own time is 0.001-0.005 % of the campaign's CPU (the neutral's 1.7 % of a campaign that
plans 4 packages) and `fronts_bench --gate` passes.

The whole campaign at mode 1 against 0 (`air_war_balance --cpu`, 3 seeds x 10 days x 2 postures, std::clock): the
Pact 261.8 s against 261.0 (1.003), NATO 170.5 against 165.9 (1.028), the rebels 3.03 against 2.94 (1.031), the
neutral 2.51 against 1.45 (1.73); all four 437.9 s against 431.3 (1.015). The ground war's own time is none of that
difference: the war differs once the line has moved (the neutral's first seed plans 28 packages against 24, with more
decisions and intelligence; the Pact's seed 7 the same 232 packages for 7 % more CPU in `fronts_bench`). So
`fronts_bench --gate` gates the ground war's own share (5 %), and the whole ratio is printed beside it; the neutral's
1.73 is 0.12 s a campaign of a war that is otherwise idle. `AbsDistance` on a moved line costs 0.04 us against
0.03 at zero offsets.

**Left:**
- F3.4: the commanders and their rule tables in place of the rule above (decision times, the second echelon sent to
  the axis that moves rather than held, our withdrawal on order, counterattacks to the border, reserves, the rebels'
  raids and their sectors, re-forming the spent, the reparable tanks returned).
- F3.5: each side's picture; F2.3's crisis reads it.
- F3.6: U1 in the player's sector and its neighbours; the march (our second echelon and reserves still appear in
  place, without a road move).
- F3.7: the `Troops` objects and supply lines' ends following the units, overrun objects, the FSCL; the armed
  reconnaissance's patrol point (`logistics.hpp`) and the played raid's front point nearest the battery
  (`raid_scenario.hpp`'s air fight block) still read the front's base line; the holding areas measured from the moved
  line (F2.4's left).
- F3.8: our units' situation reports from the war into the pipeline (the journal's `"ground"` entries are the truth).
- Supply and cohesion stay 1 (F3.4's cohesion, F3.7's supply); pontoon parks are not modelled; the air war's strikes
  do not yet come off the formations' strength (F3.7).
- F6: `ground.mode` 1 in the data, and every weight, frontage and the tank mapping checked against history.

## Done when

Each faction's ground war (or the owner's choice for the rebels and the neutral) moves the line of contact by its
units' combat and its officers' decisions, the air war's troops, supply and close support follow it, our side's posts
learn of it by reports, it costs under its budget, and old saves replay unchanged. Its outcomes against history are
F6's.
