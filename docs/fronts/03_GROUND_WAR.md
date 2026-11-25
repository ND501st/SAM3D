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

## As built: F3.4 (21 Nov 2026)

The minimal rule is gone: commanders decide the ground war (`ground_war/commanders.hpp`, the rules as pure functions;
`ground_war.hpp`, their clock and what their orders do). Still only at `ground.mode` 1 on the nation; at 0 nothing is
built.

**Who.** One commander per formation at U0 and one per headquarters between the formations and the side's top: the
lowest unit of its order of battle over all of a side's formations (the Pact's combined arms army, NATO's allied corps,
the rebels' military command, our 3rd Army; against the rebels our 7th Division, whose regiments are the formations). With no unit over them
(the neutral's division and its two territorial regiments), the first formation of the plan's main axis is the top. Each
answers to the nearest commander above him. A commander written in the ground files (`"authored"`, by unit: Gromov,
Hayes, Eder, Radic, Kral) is used as written; the rest are drawn from the side's pool (`OfficerPoolDef`: a name, a background, an
age, the eight traits each normal about the pool's mean, a fork of the war's root keyed by the side and his index), with
the pool's rank for his unit's kind. No quirks yet. His echelon (division for an army, corps, division or group;
regiment for a brigade or regiment) gives his FM 100-2-1 reaction and planning times.

**Traits move thresholds** by at most `trait_shift` (0.2) of each range (`ShiftedThreshold`), each a pull 2t-1:
boldness lowers the attack ratio (the doctrine's in 2-4: the Pact's 3, NATO's from its file) and the counterattack ratio
(1.5-2.5); rigidity asks to withdraw later (the ineffective line, never below it, a flexible man up towards the degraded
line), tolerates a longer stall before stopping an axis (12-36 h about `stall_halt_h`) and holds a security line longer
(1-2 h); composure holds longer without an order (3-9 h) and moves a breakpoint up (NATO, the small forces);
methodical takes the long end of his reaction and planning times. New `ground_war.json` block `"commanders"`, every
value a labelled game value or estimate (`moving_km_per_day` 2.8 after `hero_1972_oralfore`).

**When** (no polling). A decision is an event in the queue (`Decide`, the commander and his decision's number: a later
wake that comes sooner supersedes it). Each side's top decides from the offensive's start at his reaction time while
his war is active (an axis attacking or counterattacked, an echelon marching or uncommitted): that is his decision
cycle. Everyone else decides only on an event: his unit under its degraded or ask line or past a breakpoint (seen after
the hour it fought, then his reaction time), a request or report from below (his reaction time), an axis halted, a
penetration reaching our main defensive zone, a unit re-formed, the time he could hold without an answer run out (or,
after it, his unit in contact again). Orders take effect as events too: an echelon's or a relief's arrival (his planning
time plus the march: the straight distance x 1.3 at the day's road speed, 20-30 km/h; F3.6 asks the roads), a security
line given up, a unit re-formed (withdrawn: 1-2 days; dispersed: 1-3), a raid's night and its dawn.

**The commanders' picture** is a stand-in: `GroundWar::SeenPower`, `SeenFraction`, `SeenDepth` and `SeenPace` are the
only reads a commander makes of the war, and until F3.5's picture they return the truth as of the hour he decides (no
delay, no error), for his own side and the enemy's alike. F3.5 replaces these four (done: "As built: F3.5").

**The enemy's top** (the Pact's army, NATO's corps, the neutral's division): commits its echelons (the plan's second
echelon, its reserve and units re-formed) once the plan's `second_echelon_after_h` (24 h) has passed since the
offensive's start or an axis's attackers are worn (one under its degraded line, stopped at its breakpoint or gone):
the Pact to success (`EchelonTarget`: its planned axis if it moves at 2.8 km a day or more, else another that moves
faster), NATO and the neutral to their own axis while it is open, failing both the open axis where its weight brings the
ratio to his attack ratio; held back otherwise (journaled once). It stops an axis that has not advanced for his stall
time with no echelon coming and no crossing under way, and resumes one our counterattack has stopped once the ratio
there is back at his attack ratio. It permits its units' requests (its doctrine withdraws and re-forms the ineffective).

**A unit's commander** (`DecideUnit`, the Pact and ours on lines, NATO and the small forces on breakpoints): under 2/3
he reports (the Pact's senior then has a worn axis); under his ask line he asks his senior and never withdraws on his
own (unless he is the top); with no answer, once he has held in contact as long as his composure allows, his unit
breaks and disperses. NATO's division stops its attack at its attack breakpoint (drawn per formation, 15-35 % losses,
moved by its commander's composure) and holds, asking nobody; past its defence breakpoint it withdraws on its own
decision. The rebels' groups disperse rather than retreat. A unit whose cohesion falls under 0.3 breaks at once.

**Our side.** The security zone (where our doctrine has one: 30 km against the Pact and NATO, 20 against the neutral,
10 against the rebels) is held line by line, every 7 km from the forward edge: the attack stops at the line the
covering force holds; when it reaches it, the division's commander (the regiment's senior for a regiment) orders the
covering force back after 1-2 h (his rigidity), journaled; then to the next. The main defensive zone holds (no
withdrawal without an order). A request to withdraw goes to the army commander: permitted behind the next when another
of ours holds the same ground (a defender beside it, or the sector's second echelon engaged in depth), else a relief
from the free formations (the reserve, or one re-formed) marched in (the asking unit holds until it comes, then
withdraws as a unit and the relief takes its sectors), else refused (journaled once; the unit disperses if no order
comes while it is in contact). The army commander counterattacks the deepest penetration of the main defensive zone with
no counterattack on it when his force there (the reserve, the re-formed and that sector's second echelon) is at least his
counterattack ratio over the penetration's attackers: it marches in and attacks; the axis's attackers defend in a
meeting engagement (in hasty defence if the axis had halted), or NATO's covering force (the plan's new
`"covering_force"`, `nato_acr_2`) alone in delay while it stands, the rest behind it out of the fight; the line goes
back, never past the border; it ends there, when it has not gained ground for his reaction time's low end or when its
force is spent, and its force holds the axis.

**Retreat** (§5.4). As a unit: ordered (the senior's permission, a relief) or NATO's own decision: out of the line at
once with its strength, at the reserve's rest rate, re-formed after 1-2 days (`reform_days`). Dispersed: no order while
in contact under the line, its cohesion gone, or a rebel group: 20 % of what is left lost as stragglers (their tanks not
reparable), out of the line, its remnants re-formed after 1-3 days. Re-formed, a unit's establishment is what it has
(it is a smaller unit, cohesion whole), the enemy's an echelon for its top, ours free for the army commander. Cohesion
now moves: it falls by twice the hour's strength losses and is regained at 0.2 a day, in contact too (the data's
text says so now), so a steady grind wears it slowly and only heavy losses in hours break a unit. Of our tanks lost by
table 7-7, the reparable 80 % go to repair and a quarter of those in repair come back each day's end
(`repair_return_per_day`).

**The rebels** (§8). One raid axis per line group (straight in from its camp's place on the line, as wide as the
group's attack frontage, the plan's 15 km deep). At each night's start their commander raids where a group's power over
the share of our defenders' frontage its width meets reaches his ratio (3, moved by his boldness), the best first, up
to the plan's two a night; where nobody holds, at once. At dawn the raids end and the line goes back to the border:
raided, not held. The neutral's incursion is the plan's limited axis to its political depth (18 km), then held.

**The journal** (kind `"ground"`, hidden): every decision, with who decided (rank and name) and about whom: echelons
committed (and switched) or held, axes stopped and resumed, attacks stopped at a breakpoint, reports, requests,
permissions, refusals, reliefs, withdrawals as a unit and dispersals, units re-formed, our counterattacks ordered,
going in and ending, the covering force ordered back a line, raids and their dawn.

**Tools.** `fronts_bench data --ground --log` prints the first seed's decisions as they happen; the axes' line ends with
the decisions made.

**Mode 1 on the nation** (`air_war_balance --cpu --theatre theatre_pannonia_nation.json --set ground.mode=1 data 3 10`,
the line's mean depth by sector at the day's end, km; 3 seeds, the same for both battery postures; F3.3's in brackets):

| Faction | Sector | Day 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8-10 | What decided it |
|---|---|---|---|---|---|---|---|---|---|---|
| Warsaw Pact | tisava | 23.1 (34.7) | 42.4 (45.9) | 45.9 (55.3) | 56.6 (60.9) | 60.9 | 60.9 | 60.9 | 60.9 | our covering force back a line at a time (7 km, 1-2 h each) slows the first day by a third; the 9th Tank Division committed on the main axis about 25 h in (to success: its own axis moves); the capital's depth on day 5, a day later |
| | ne_crossings | 17.8 (26.7) | 32.6 (35.4) | 35.3 (42.5) | 43.5 (46.8) | 46.8 | 46.8 | 46.8 | 46.8 | the main axis's taper |
| | rava_reservoir | 9.4 (13.9) | 20.0 | 20.0 | 20.0 | 20.0 | 20.0 | 20.0 | 20.0 | Dravec on day 2, the covering lines first |
| NATO | upper_lenta | 0 | 0 | 0 | 0 | 13.2 (22.9) | 40.2 (55.4) | 69.7 (74.9) | 74.9 | from 96 h; the security zone's lines, then the brigade reports under 2/3 on day 7; Saldanj a day later |
| | western_crossings | 0 | 0 | 0 | 0 | 20.8 (30.0) | 32.4 (34.1) | 36.1 (37.2) | 37.2-38.6 (37.6-38.9) | slows against our second echelon; the corps commander stops it on day 16, 41 km in (the first seed, 30 days) |
| | zlatno | 0 | 0 | 0 | 0 | 3.3 (4.8) | 5.2 (5.5) | 5.8 (5.9) | 5.9-6.2 | the main axis's taper |
| Neutral | border_forest, lenta_marshes | 7.0, 6.1 (12.5, 10.9) | 17.9, 15.6 | | | | | | 17.9, 15.6 | the covering lines; both territorial regiments committed on day 2 (its own axis); the hold line (18 km) on day 2, held |
| Rebels | rebels_forest | 1.8 (0) | 1.8 | 1.8 | 1.8 | 1.8 | 1.8 | 1.8 | 1.8 | a raid out at midnight every night, 2-3 km in where nobody of ours holds, back to the border by dawn |
| | hollos | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0-0.2 | the same, one seed's on day 10 |

Nothing near a runaway: the fastest day anywhere is 23.1 km (the Pact's first). No counterattack goes in on the nation:
against the Pact our army has a tank brigade against a three-division axis, against NATO a division and a brigade
against three divisions, both under 1.5:1, so the army commander does not order one (the hand case shows it does go in
at 2.9:1, pushes the line back to the border and NATO's covering force screens). The neutral and the rebels never bring
a unit under its line; the Pact's and NATO's units on the nation lose too slowly to ask within ten days (the south-western
brigade's report on day 7 is the only line crossed), so no withdrawal, dispersal or relief happens there in ten days:
the hand cases exercise them.

**Cost** (`fronts_bench data --ground --gate`, MSVC Release, the tool alone on the machine; ms a campaign day, 30 days x
5 seeds; the decisions this run made, the first seed's):

| Faction | Mean | Worst day | Combat | Decisions | Movement | Line | Decisions made (30 days) |
|---|---|---|---|---|---|---|---|
| Warsaw Pact | 0.0096 | 0.085 | 0.0057 | 0.0021 | 0.0002 | 0.0010 | 62 |
| NATO | 0.0200 | 0.076 | 0.0151 | 0.0027 | 0.0004 | 0.0013 | 156 |
| Neutral | 0.0046 | 0.043 | 0.0019 | 0.0013 | 0.0001 | 0.0007 | 20 |
| Rebels | 0.0575 | 0.079 | 0.0272 | 0.0275 | 0.0005 | 0.0020 | 35 |

against 20 ms (mean) and 60 ms (worst): still three orders of magnitude under; F3.3's were 0.002-0.019 ms. The rebels'
cost is their raids (a night's two axes resolved hourly, the night's decision scanning its groups). In the campaign (10
days, seed 7) the ground war's own time is 0.001-0.002 % of the campaign's CPU (the neutral's 1.9 % and the rebels'
0.16 % of campaigns that plan 4 and 13 packages); `--gate` passes.

The whole campaign at mode 1 against 0 (`air_war_balance --cpu`, 3 seeds x 10 days x 2 postures, std::clock): the
Pact 260.7 s against 261.7 (0.996), NATO 172.4 against 166.7 (1.034), the rebels 3.02 against 2.94 (1.027), the
neutral 2.41 against 1.45 (1.66); all four 438.5 s against 432.8 (1.013). As in F3.3, the difference is the war
differing once the line moves (the planner, routes and C2 read it), not the ground war's own time.

**Identity.** At `ground.mode` 0 nothing is built: `air_war_balance data 20 7` and `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7` are byte-identical (`cmp`) to track F's tip before it (81d8e2d, built the same
way); the replay fingerprint (`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages) holds.

**Tests** (`test_ground_commanders.cpp`): the thresholds' shifts by hand (at most 0.2 of each range, the right way);
commanders drawn keyed; `DecideUnit` by hand (report, ask, never withdraw without an order, hold, disperse in contact,
withdraw on an order or as the top; NATO's stop and break; the rebels' dispersal); `EchelonTarget` and `AnswerRequest`
by hand; then hand wars on a straight line: a division under its line asks, is refused (nobody behind, nobody free),
never leaves the line until it disperses after holding, and re-forms smaller in 1-3 days; with a reserve, a relief is
sent, the division holds until it comes and withdraws as a unit; the Pact's second echelon switched to the axis that
moves, NATO's to its own; NATO's divisions stop at their breakpoints without asking and the axis halts; our
counterattack at the army commander's ratio, back to the border at most, NATO's covering force taking it; the same war
resolved in either order gives the same orders; the rebels raid only where nobody holds, only by night, never past 15
km, back by day; every faction's war on the nation for 12 days: bounded, ours never withdraw on their own, the queue
never past its reserved capacity. F3.3's order-free test now compares strengths too (both axes reach their goal once a
broken defender is withdrawn).

**Left:**
- F3.5: each side's picture behind `SeenPower`, `SeenFraction`, `SeenDepth` and `SeenPace` (the commanders read the
  truth now); F2.3's crisis reads it.
- F3.6: the march on the roads (an echelon's, a relief's and a counterattack's march is the straight line x 1.3 at the
  day's road speed); U1's regiments and their commanders; the counterattack into the flank (it goes in on the axis).
- F3.7: strikes off strength and cohesion (`cohesion.air_attack_loss` is not read yet), supply below 1.
- F3.8: the decisions into our posts' reports (the journal's `"ground"` entries are the truth).
- Quirks for the ground commanders; restoring control after a command post is lost (`restore_control_h`), the
  prepared positions' count and NATO's defence ratio (`defend_ratio`) are not read; the covering force's own share of
  a NATO division (30-50 %) is the plan's unit, not a share.
- F6: whether the counterattack threshold against a whole axis's attackers is right (no counterattack happens on the
  nation), the security lines' pace against history, and every value here.

## As built: F3.5 (21 Nov 2026)

Each side decides on its picture (`ground_war/picture.hpp`; `GroundWar`'s `Picture(side)`). Still only at `ground.mode`
1 on the nation; at 0 nothing is built.

**What a picture holds** (`GroundPicture`, one a side, sized at `Build`): a `UnitBelief` per formation of both sides
(believed strength fraction and power, the hour the report was compiled, so its age, the factor drawn, so its error,
the sector it was seen in, its posture, its source, whether it was out of the fight), an `AxisBelief` per axis (depth,
pace, our counterattack on it), the line's offsets by station, and for the enemy's picture the sectors where its own
ground front is collapsing. A few numbers per formation and axis and a line a side.

**Where it comes from** (`data/gameplay/ground_war.json`'s new `"picture"` block, every value a labelled game value or
estimate; the report period is `reports.division_per_day`'s):
- *its own units*: scheduled returns twice a day, compiled at 06:00 and 18:00 local, and a unit's report at once when it
  reports, asks, stops its attack, withdraws, disperses or re-forms; each reaches the staff `own_delay_h` (2 h) after it
  was compiled, at a 5 % spread. Its own side knows at once which of its units are out of the fight (it ordered them out
  or was told), so `SeenPower` gives 0 for them.
- *ours of the enemy*: the formations our units touch (an axis's attackers, a rebel group on its line) by our units'
  contact reports, riding with them (scheduled and at once, 25 %); the rest (echelons, reserve) by the army's
  intelligence summary (`army_intel`), once a day at 06:00, 6 h late, 40 %.
- *the enemy's of ours*: where its `IntelPicture` holds a troops entry (not a ghost) observed within 24 h behind the line
  in a sector one of ours holds (within 40 km) or within 30 km of one of ours off the line, its air reconnaissance (20 %);
  else, for ours at the line (holding a sector, or fighting on an axis), its ground reconnaissance (35 %); both 3 h late.
  The match by sector and distance is a stand-in until F3.7 ties the theatre's troops objects to the formations. The
  campaign lends the ground war its `intel_` (`GroundEnv::intel`); without it (the hand tests) there are no air sightings.
- *the prior*: at `Build`, its own units as they stand and the other side's at a 30 % spread (peacetime intelligence).
- *the axes and the line*: every scheduled report carries each axis's depth and pace and the line by station (one batch
  in flight a side: the loader refuses a delay as long as the period); at once on a breakthrough into our main zone
  (ours), a halt or a resumption (the enemy's), our counterattack's start and end (both). Depths carry no error (a unit
  knows where it stands and where the enemy faces it), only the delay.

**A believed strength** is the truth when the report was compiled (`StrengthAt` at that hour) times a lognormal factor
of mean 1 and the source's spread, drawn by `BeliefFactor` as a fork of the war's own root keyed by (side, source,
sector, report hour): the same for every formation of that sector in that report, and the same whatever is drawn or
resolved first (`SetReverseOrder` gives the same pictures).

**When** (no polling). Every report is an event in the war's (hour, sequence) queue: `ReportsDue` (a side's batch) and
`IntelDue` compile and queue `UnitReport`, `AxisReport` and `LineReport` events at their arrival hours; a report merges
only if it is no older than what the staff holds. A quiet hour costs nothing more; nothing is allocated in the hour (the
queue's reserve grew by six events a formation and four an axis; the nation's 12-day wars stay inside it).

**What reads it.** The four accessors read the deciding commander's side's picture: `SeenPower` (the enemy's echelon
commitment and its axis ratio, our counterattack's ratio, the rebels' raid ratio), `SeenFraction` (the strength a
senior's answer journals), `SeenDepth` (our army commander's deepest penetration), `SeenPace` (the Pact's echelon to
success). A formation's own commander reads his own unit's count (`OwnFraction`: the lines and breakpoints it crosses,
his `DecideUnit`), as he did: his reaction time is his staff's count. Who stands on which axis and whether an axis
attacks stay known to both sides. Tests switch the picture off with `tables.picture.truth` (F3.4's hand cases keep
deciding on the truth; never in the file).

**The air front's crisis.** `GroundWar::EnemyCrisis()` is, by the line's sector, 1 where the enemy's picture holds a
formation of its own on an axis (or its raid line) there believed under `crisis_below` (0.5), or our counterattack on an
axis there, recomputed only when a report reaches the enemy's picture. `AirCampaign::AirFrontDecisionInputs` passes it as
`AirFrontInputs::crisis` while the ground war runs (`ground.mode` 1, a theatre with a country map and the faction's
ground file); otherwise it stays null and nothing changes. The campaign's `env.intel` is set every hour; at mode 0 the
ground war is not stepped.

**First person.** Our side's picture is our army staff's belief, never the player's: nothing reads it outside the ground
war. F3.8 turns our side's reports into situation reports in the pipeline.

**Tools.** `fronts_bench --ground` times the picture's own part (the reports' events and the crisis) and lends the
ground war the campaign's pre-war `IntelPicture`.

**Mode 1 on the nation** (`air_war_balance --cpu --theatre theatre_pannonia_nation.json --set ground.mode=1 data 3 10`):
the line's depth by sector at each day's end is F3.4's to the decimal in every sector of every faction (tisava 23.1,
42.4, 45.9, 56.6, 60.9...; upper_lenta 13.2, 40.2, 69.7, 74.9 from day 5; border_forest 7.0 then 17.9; rebels_forest
1.8; hollos 0.2 on day 10), and the whole output but the CPU lines is byte-identical to F3.4's tip (d765717) at mode 1.
`fronts_bench --ground --log` over 30 days (the first seed) makes the same decisions at the same hours for all four
factions; the only lines that differ are the rebels' raids on rlf_bde_1 (days 13, 19, 26), decided on a believed ratio
(9.8, 7.4, 4.1 to 1 against the truth's 6.3, 5.0, 4.9) that clears the commander's 3 to 1 either way. Why nothing moves:
on the nation every decision sits far from the picture's errors. The Pact's and the neutral's echelons go to their own
axes, which move at many times 2.8 km a day whatever the report's age; NATO's stall stop reads the axis's own last
advance; our army never counterattacks (its force against a whole axis's attackers is under 1.5 to 1, and a 25 % error
does not lift it to the commander's 1.5-2.5); the rebels raid where nobody holds or at ratios well over 3. No crisis
fires: no formation of the enemy's falls under half in ten days (F3.4: the south-western brigade's report on day 7 is the
only line crossed, and it is ours) and we never counterattack. The hand cases show the picture deciding: our army
commander orders the counterattack at 13 h on 2.89:1 from a reserve report compiled at 0 h, and gives the same order on
the same ratio when the reserve has lost half its strength after that report reached him (a reserve of 1620 believed,
809 true); with the loss before the report was compiled he does not counterattack.

**Cost** (`fronts_bench data --ground --gate`, MSVC Release, the tool alone; ms a campaign day, 30 days x 5 seeds; F3.4's
mean in brackets):

| Faction | Mean | Worst day | Combat | Decisions | Movement | Line | Picture |
|---|---|---|---|---|---|---|---|
| Warsaw Pact | 0.0203 (0.0096) | 0.093 | 0.0062 | 0.0044 | 0.0002 | 0.0009 | 0.0081 |
| NATO | 0.0313 (0.0200) | 0.091 | 0.0159 | 0.0052 | 0.0004 | 0.0013 | 0.0081 |
| Neutral | 0.0123 (0.0046) | 0.050 | 0.0018 | 0.0030 | 0.0001 | 0.0007 | 0.0061 |
| Rebels | 0.0678 (0.0575) | 0.122 | 0.0280 | 0.0306 | 0.0006 | 0.0019 | 0.0065 |

The picture is 0.006-0.008 ms a campaign day (about a hundred report events a day for the nation), and the decisions'
part grew by 0.002-0.003 ms (the at-once reports compiled where a unit decides, and the profiler's own laps around the
report events); still three orders of magnitude under 20 ms and 60 ms. In the campaign (10 days, seed 7) the ground war's
own time is 0.002-0.003 % of the Pact's and NATO's CPU (3.9 % and 0.18 % of the neutral's and the rebels' campaigns of 4
and 13 packages); `--gate` passes.

The whole campaign at mode 1 against 0 (`air_war_balance --cpu`, 3 seeds x 10 days x 2 postures, std::clock, the two
modes run side by side): the Pact 267.3 s against 269.3 (0.992), NATO 177.3 against 172.6 (1.028), the rebels 3.07
against 2.98 (1.030), the neutral 2.45 against 1.49 (1.64); all four 450.1 s against 446.4 (1.008). As in F3.3 and F3.4
the difference is the war that differs once the line moves, not the ground war's time.

**Identity.** At `ground.mode` 0 nothing is built: `air_war_balance data 20 7` and `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7` are byte-identical (`cmp`) to track F's tip before it (d765717, built the same
way); the replay fingerprint (`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages) holds.

**Tests** (`test_ground_picture.cpp`): the factor's mean and spread over 4000 draws and its key; a hand war's reports
arriving at their delay, the belief holding between them and equal to the truth when compiled times its factor, two
attackers of one sector's report sharing a factor, the second echelon the army intelligence's; the enemy's air
sightings where its `IntelPicture` has troops (in a sector held, near a reserve; a stale one is none) and its ground
reconnaissance elsewhere; a commander acting on a stale picture (the counterattack above); the enemy's crisis from a
division under half only once its report arrives, and from our counterattack its delay after it starts; the air front's
input null at mode 0 and the enemy's crisis at mode 1; the same war and pictures either way round; the queue within its
reserve. F3.4's hand cases decide on the truth (`picture.truth`), its nation war on the picture.

**Left:**
- F3.6: the march on the roads; U1's regiments and their pictures (a regiment's own reports at its echelon's times).
- F3.7 (done: As built: F3.7): the theatre's troops objects tied to the formations (the air sighting's sector and distance match is a
  stand-in); strikes off strength reported in the pictures; supply below 1 believed.
- F3.8 (done: As built: F3.8): our units' reports into the pipeline as situation reports (`FactKind::Situation`), the army's
  staff as a post, its intelligence summary as a fact; the believed line by station is not a fact (F3.8's Left).
- Believed places finer than the sector (a unit's believed position and its spread) and believed postures read by
  anyone; the enemy's own reports' delays by faction (one table for all four now); the reconnaissance's spread falling
  with the time in contact; a commander's own traits weighing his picture (a sceptical one discounting a report).
- F6: whether any decision on the nation should turn on the picture (none does in 30 days), the spreads and delays
  against history.

## As built: F3.6 (21 Nov 2026)

U1 in the player's sector and the march on the roads (`ground_war.hpp`, `units.hpp`'s `Regiment` and `UnitLevel`,
`nav/routes.hpp`'s `RouteCache` and `Overlay::version`). Still only at `ground.mode` 1 on the nation; at 0 nothing is built.

**The regiments** (`GroundWar::Regiments()`, built once at `Build`). A formation's children in its order of battle of a
regiment's, brigade's or battalion's kind that are not formations themselves and hold weighted equipment, each with its
tree's strength as its share of the formation's (`part`, summing to 1; the formation's own troops go with them in
proportion). Support is a regiment with no tanks (the artillery). A formation needs two tank-holding regiments to have
any: the Pact's divisions have five (three motor rifle, a tank and an artillery regiment), NATO's four (three brigades
and the artillery), the neutral's three brigades, our divisions four or five; our brigades and the rebels' groups none,
so they stay U0 everywhere. The first echelon is as many as the frontage takes at the regiment's (ours: the run of
sectors over the defence frontage's widest; the enemy's: the breakthrough's width over the attack frontage), at least
one, one kept back. Each regiment has a commander drawn from its side's pool, keyed by the regiment (a regiment's
reaction time, 1-3 h, FM 100-2-1), his senior the formation's commander.

**Up and down** (`levels` in `ground_war.json`: `down_after_h` 6, `neighbours` 1). A formation is needed when it holds
a sector, attacks or marches on an axis, or is an echelon planned for an axis, within one sector of the player's
battery's (`GroundEnv::player_sector`: the campaign sets it every hour from the battery's position; -1 none). It goes up
at once (the regiments take its cohesion, their stations and postures; their shares are as they were) and down 6 h after
its need ended (its strength, posture and place untouched; the regiments stand still at U0 until it comes up again).
Both are journaled (`level_up`, `level_down`).

**The same war at a finer grain.** The formation's holdings stay the truth, so its strength and power are exactly what
U0 gives: U1 only shares out the hour's loss among its regiments, axis by axis in proportion to what each cost it, by
where each stands. In the security zone only the covering regiment (the first echelon's that the axis meets, else the
nearest) takes it; in the main zone and attacking, the regiment the axis meets `hit_weight` (2), the rest of the first
echelon 1, the second echelon and support `support_weight` (0.5: fire reaches them, the assault does not), a regiment on
the march to a relief 1. Each regiment's cohesion falls with its own loss and comes back at the daily rate; the
formation's is their mean by strength. So U1 parts from U0 only where a regiment's own line wakes its commanders sooner,
where one regiment's cohesion is held at 1 while another's falls, and through the draws those decisions consume.

**A regiment's decisions** (on its own count, its share of its establishment's times the formation's fraction, by its
side's lines or breakpoints and its commander's traits). Crossing a line, it decides after his reaction time: under the
degraded line (or past the attack breakpoint) he reports (`regiment_reported`); under the ask line (or past the defence
breakpoint), in the first echelon with no relief coming, he asks to be relieved (`regiment_asked`). Each goes into his
side's picture as his formation's report (F3.5's `ReportAtOnce`), and the ask wakes his formation's commander, who (in
`Decide`, at U1 only) relieves each regiment that asked with the freshest of his second echelon, at least 0.05 of its
establishment stronger (`regiment_relief_ordered`, the march's hours as its value). It marches on the roads within the
sector between the two regiments' anchors (each the nearest town, landmark or theatre object to its place); on arrival
they change stations, places and anchors and the relieved goes back to recover (`regiment_relieved`). Re-forming sets
the regiments' establishment to what they have.

**The march** (section 7). Every march the ground war times is now asked of the route service: an echelon committed or
switched (the enemy's), a division's relief and a counterattack (ours), a regiment's relief. A formation carries its
anchor (its deployment's or garrison's place; once arrived on an axis, the axis's anchor); an axis carries its route's
places as anchors by depth. The march is the straight line from the formation's place to its anchor, the road from its
anchor to the axis's anchor on its side of the fight (ours: the shallowest at or beyond the axis's depth; the enemy's:
the deepest at or short of it), and the straight line from there to the fight, the two ends at F3.4's rule. The road
part (`RoadHours`): `RouteCache::Get` keyed by the two anchors, the column's mobility and the overlay's version (a search
only on a new key; 128 entries, the oldest dropped; never in the hour), its road legs at the column's speed by day
(`road_day_kmh`'s middle, 25 km/h) or by night (17.5) as the march starts, its ways off the road at the cross-country
speed (10), and a halt of 25 min every 2.5 h of driving. The overlay is the theatre's bridges joined to the road
crossings they stand on (within 100 m, as the mission's session joins them), cut while the air war has their function
under 0.5, rebuilt (its version moved) only when one changes and only on an order. Where the bridges down leave no way
round, the road as it was plus `crossing_cut_h` (12 h) for each cut river on it. Where there is no anchor, no road net
(the hand tests: no country map) or an end more than 6 km from the map's roads, the whole march is F3.4's straight line
x 1.3 at the day's speed, counted as a fall-back.

**What the roads changed.** On the nation almost nothing yet, for a reason worth knowing: the map's roads end at our
border, and every enemy echelon and the neutral's march from an assembly area 20-60 km beyond it (no road within 6 km),
so all of them (5 in the Pact's 30 days x 5 seeds, 10 in the neutral's) fall back to x 1.3; and our side orders no
division's relief or counterattack on the nation in these wars (F3.4). In the campaigns: the Pact's one echelon and the neutral's two (seed 7, 10 days) fall back the same way, and NATO marches none (`fronts_bench --ground`'s campaign lines). Where the
roads are asked they lengthen marches: dravec to mezovar is 4.3 h on the roads by day against F3.4's 2.7 h (the road
runs 97 km round for 52 km straight), 6.4 h by night, and 16.3 h with mezovar's road bridge down (no way round: the road
and the river crossed at the cut's 12 h). A regiment's relief inside a sector is 1-3 h.

**Mode 1 on the nation** (`air_war_balance --cpu --theatre theatre_pannonia_nation.json --set ground.mode=1 data 3 10`,
3 seeds; F3.5's in brackets where they differ): the rebels (rebels_forest 1.8 nightly, hollos 0.2 on day 10) and the
neutral (border_forest 7.0 then 17.9, lenta_marshes 15.6) are F3.5's to the decimal, and so is the Pact's first battery
posture (tisava 23.1, 42.4, 45.9, 56.6, 60.9; ne_crossings to 46.8; rava 20). In the Pact's second posture tisava runs
46.6 and 58.1 on days 3-4 (45.9, 56.6) and ne_crossings 35.8 and 44.7 (35.3, 43.5), both reaching F3.5's 60.9 and 46.8
on day 5. NATO's upper_lenta is F3.5's (0 four days, then 13.2, 40.2, 69.7, 74.9); western_crossings goes 37.3, 38.0,
38.7, 39.3 on days 7-10 (36.1, 37.2, 37.9, 38.6) and zlatno 6.0-6.3 (5.8-6.2), in both postures. NATO's change is
U1's: its divisions and ours in the battery's sector go up on day 1 and their regiments report (23 regiment decisions
over 30 days x 5 seeds alone), the reports waking the division commanders sooner; alone, NATO's main axis stalls at
41-51 km instead of 38-42 for seed 7. The Pact's is a day's difference that closes by day 5 (its marches all fall
back, so it is U1's or the campaign's own divergence once the line moves). Packages and losses move by about one a
campaign (the loss rate 1.87 % against 1.78 %, inside 1-3.5 %).

**U1 against U0** (`test_ground_levels.cpp`, 20 seeds, 10 days, every faction's own war on the nation, U1 centred on
the battery's sector against U0 everywhere). The band: U1's mean within 10 % of U0's, or 3 km of summed axis depth and
0.02 of a side's loss share where they are small. Chosen because the day's draw spreads an axis's day by sigma 0.35,
about 8 % on a 20-seed mean, and U1 is built to conserve the losses, so a real departure would land far outside it;
every faction is inside 1 %:

| Faction | Battery's sector | Axis depth U0 / U1 (km, summed) | Our loss U0 / U1 | Enemy's loss U0 / U1 | Ups | Regiment decisions | Reliefs |
|---|---|---|---|---|---|---|---|
| Warsaw Pact | 1 | 80.84 / 80.84 | 0.0823 / 0.0823 | 0.0828 / 0.0827 | 43 | 0 | 0 |
| NATO | 2 | 113.10 / 114.01 | 0.1228 / 0.1227 | 0.0992 / 0.0997 | 100 | 20 | 0 |
| Neutral | 1 | 17.93 / 17.93 | 0.0080 / 0.0080 | 0.0602 / 0.0602 | 40 | 0 | 0 |
| Rebels | 2 | 4.47 / 4.47 | 0.0012 / 0.0012 | 0.0350 / 0.0350 | 0 | 0 | 0 |

The hand war shows the rest: a division of three tank regiments and an artillery regiment holding the east sector at
U1, the axis meeting its first regiment, which falls to 0.83 while the other first-echelon regiment is at 0.92 and the
second echelon at 0.95; it asks at 14 h (0.88), the division commander sends the second-echelon regiment at 18 h (a
2.0 h march), the two change places at 21 h; out of the player's area from 22 h, the division goes down at 28 h with
its strength, posture and place, its regiments' shares unchanged at U0, and comes up again at once with them. The
formation's strength is the sum of its regiments' to 4e-16 every hour.

**Cost** (`fronts_bench data --ground --gate --player-sector battery`, MSVC Release, the tool alone, 30 days x 5 seeds;
ms a campaign day; U1's extra is U1 centred on the battery's sector minus U0 everywhere):

| Faction | U0 mean | U1 extra (mean) | U1 extra (worst day) | Levels part | Regiments | Level changes | Route asks / searches / hits / fall-backs | Route ms (all seeds) |
|---|---|---|---|---|---|---|---|---|
| Warsaw Pact | 0.0217 | 0.0014 | 0.036 | 0.0015 | 30 | 10 | 5 / 5 / 0 / 5 | 0.058 |
| NATO | 0.0313 | 0.0065 | 0.057 | 0.0018 | 22 | 25 | 1 / 1 / 0 / 0 | 0.011 |
| Neutral | 0.0137 | 0.0006 | 0.041 | 0.0007 | 8 | 10 | 10 / 5 / 5 / 10 | 0.047 |
| Rebels | 0.0656 | -0.0002 | 0.021 | 0 | 0 | 0 | 0 | 0 |

Against 15 ms and 45 ms: four orders of magnitude under. A route search is about 0.01 ms on the nation's graph; the
levels' part is the hourly need check over the formations with regiments. In the campaign (10 days, seed 7) the ground
war's own time is 0.002 % (the Pact), 0.003 % (NATO), 0.19 % (the rebels) and 3.8 % (the neutral's campaign of 4 packages) of each campaign's CPU; the whole campaign at mode 1 against 0 is 1.051 for the Pact, 0.829 for NATO (187 packages against 221), 1.005 for the rebels and 0.857 for the neutral (0.01 s either way) (the
war that differs once the line moves, as in F3.3-F3.5); `--gate` passes.

**Identity.** At `ground.mode` 0 nothing is built: `air_war_balance data 20 7` and `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7` are byte-identical (`cmp`) to track F's tip before it (6c227db, built the same
way); the replay fingerprint (`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages) holds.

**Tests** (`test_ground_levels.cpp`): the hand war above (shares, echelons, the commander's senior, up at once, the sum
every hour, the regiment the axis meets losing more, the ask, the relief by the second echelon and the change of
places, down 6 h after with nothing changed, up again with the same shares, the queue within its reserve); the route
service on the nation (one search for a pair of anchors and a cache hit, slower by night on the same route, a bridge
down in the truth moving the overlay's version and searching again, the march longer, the bridge back up the first
route's hours); U1 against U0 over 20 seeds (the table).

**Left:**
- The regiments' own pictures (a regiment's reports at its echelon's times, the formation's picture by regiment) and a
  regiment's believed place; our counterattack into the flank by regiments (F3.4's counterattack is still the
  formation's); the line drawn by regiment rather than by the formation's run.
- The enemy's roads beyond our border (the country's map has none): its echelons stay on F3.4's rule. Believed
  overlays (a unit planning on the bridges it believes down, track G's rule) and a column's exposure on the road
  (`SupplyColumnMovers`) for the air war to read.
- U2 (movers in a session, drawn in Unreal) is F5.2's.
- F6: whether the hit and support weights and the relief margin match history (a regiment's losses in the main blow).

## As built: F3.7 (21 Nov 2026)

The air war follows the ground war's units (section 6). Only at `ground.mode` 1 on a theatre with a country map; at 0
nothing is tied, drawn or counted.

**The ties** (`GroundWar::Ties()`, `TroopsTie`, built at `Build`). Each of the theatre's `Troops` objects (not a decoy)
whose owner in our order of battle (`OwnerOfObject`) is or sits under one of our formations is tied to it, and to the
regiment it sits under where the formation has regiments (F3.6). On the nation 11-12 a faction (the border guards'
battalion has no formation). An object's share of its unit is one over the objects tied to the same regiment (or
formation); its base the regiment's part of the formation's establishment at the war's start (or the formation's).
- **Working share**: the unit's strength now (its rest rate in closed form) over that base, in [0, 1]
  (`TroopsFunction`); set after the ground war's step whenever its units took a loss or a gain
  (`GroundWar::StrengthVersion`), else once a day for the rest rate (`AirCampaign::TroopsSync`), a wreck at 0. The air
  war's repair skips a tied object, and the supply lines' decay out of stocks is put back (the unit's supply carries
  it): its unit's strength is its state.
- **Place**: its home until its formation marches to an axis (`Formation::arrived`, set on a counterattack's or relief's
  arrival) or leaves the line (spent), then the formation's place with the objects about it as at home; never nearer the
  line of contact than its unit keeps (its home's depth, up to 12 km in the line and in reserve, 3 km on an axis,
  `troops.reform_depth_km` (25, a game value) out of the line), pushed straight back along the line's normal. Asked only
  when the line's version or the ground war's `PlacesVersion` (any order that moves a formation) changes; the object's
  own relocations (`Relocate`, its prepared positions moved with its unit) are kept round it.

**Strikes** (`AirCampaign::TroopsStruck`, `GroundWar::AirStrike`). Every drop of a tied object's working share since
the last hour is the enemy's, whatever made it (the abstract resolution, a played raid applied between hours, an armed
reconnaissance). It comes off the unit: damage x the object's share x its base, never more than the regiment has, off
every holding and the men in proportion (a regiment's: off that regiment alone, the others keeping theirs);
`cohesion.air_attack_loss` (0.1) off the struck regiment's cohesion (the formation's by that regiment's part at U0); a
line crossed wakes the commander, cohesion under the break line in contact disperses the formation. Journalled hidden
(`air_attack`: who lost how much of its strength on which object). A rise of a tied object's share in the supply lines'
step is a march column's strength brought (`Reinforce`, up to the establishment at the start).

**Supply.** A line ending at troops that moved ends where they are (`SupplyNetwork::MoveEnd`: its last stretch on local
roads redrawn, routed again from the network once they are `logistics.snap_m` from where it was routed to; its columns
keep their share of the way); a line whose depot is overrun carries nothing. Each tied formation's supply is the mean by
weight of its objects' stocks against `troops.full_supply_days` (1, a game value) of the air war's consumption, set
before the hour's combat (`TroopsSupply`); the combat's power reads it (`UnitPower`). Formations with no tied object
keep 1.

**Overrun and the relocation rule.** When the line's version changes: our mobile objects within 8 km of the line of
contact (not the player's battery, its dummies, the columns or the tied troops) move back at once to the nearest of their
prepared positions 8 km behind it, else straight back to 8 km and their relocation distance (`Escape`); then our objects
whose homes are on our side and now stand behind the enemy's line (not cities, the battery, its dummies, the columns or
the tied troops) are overrun (`ObjectState::overrun`): no function, no missiles, no repair, no relocation, out of the
enemy's picture and no report of them merged (`IntelPicture::Exclude`), so out of both sides' targets; ours again,
wrecked, once the line is back past them. The relocation rule's 8 km is signed from the line of contact
(`BehindFront`: a place on the enemy's side is never far enough). Measured from the line of contact too: the
helicopters' holding area (the gate's offset off its distance behind their front), logistics' armed reconnaissance
patrol point, and a played raid's air-fight front point.

**NATO's FSCL** (`ground_nato.json`'s doctrine `"fscl"`: 20-30 km in defence, `fm_101_5_1_1980` p. 1-49, `ota_1987_fofa`
pp. 78-79, `fm_6_20_1977` p. D-5; the terrain line's reach 30 km, an estimate). Per sector: the band's middle (25 km)
beyond the sector's deepest station; where an axis attacks, on the first river its route crosses beyond the day's
objective (its depth and a day at its breakthrough cap, short of its objective) within the reach, else the band's middle
beyond it; the deeper of the two. Drawn at the start, at each day's end, on a halt and a resumption. C2's battlefield
interdiction area reaches the FSCL in place of `c2.bai_depth_m` (`AirFrontView::BaiDepth`: the sector's FSCL less the
line's offset there, read by `InBaiArea` and `NominationWaits`). The other factions draw none.

**The air's sightings** (F3.5's stand-in replaced): the enemy's air sees a formation when its `IntelPicture` holds a live
entry, observed within `air_fresh_h`, for one of the theatre objects tied to it (`air_depth_km` and `air_reach_km` gone).

**Mode 1 on the nation** (`air_war_balance --cpu --theatre theatre_pannonia_nation.json --set ground.mode=1 data 3 10`;
F3.6's in brackets where they differ). The rebels and the neutral are F3.6's to the decimal (rebels_forest 1.8 nightly,
hollos 0.2 on day 10; border_forest 7.0 then 17.9, lenta_marshes 15.6). The Pact's careless posture is F3.6's but for
tisava's day 4 (56.5, 56.6); careful: tisava 46.4, 56.9 on days 3-4 (46.6, 58.1), ne_crossings 35.7, 43.8 (35.8, 44.7),
both F3.6's 60.9 and 46.8 from day 5, rava 20. NATO: upper_lenta F3.6's (0 four days, then 13.2, 40.2, 69.7, 74.9);
western_crossings 33.6, 38.7, 39.8, 41.3, 44.2 on days 6-10 (32.4, 37.3, 38.0, 38.7, 39.3) and 45.5 careful; zlatno 7.1-7.3
on day 10 (6.3): our troops struck from the air give ground.

| Faction (posture) | Strikes on troops | Strength lost (of establishments) | Reinforcements | Overrun | Escapes | Troop moves | Lines routed again | CAS / BAI / AI | BAI / AI at the fixed depth | Packages (F3.6) | Loss rate (F3.6) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Rebels (careless) | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 / 0 / 2.7 | 0 / 2.7 | 13.3 (10.7) | 20.3 % (17.8) |
| Neutral (careless) | 0 | 0 | 5.0 | 0 | 8.0 | 32 (28 km) | 0 | 0 / 1.0 / 0 | 1.0 / 0 | 18.0 (20.0) | 5.7 % (4.2) |
| Warsaw Pact (careless) | 1.3 | 0.215 | 3.0 | 3.0 | 0 | 0 | 0 | 0.3 / 10.7 / 39.3 | 10.7 / 39.3 | 216.0 (213.3) | 4.2 % (3.7) |
| Warsaw Pact (careful) | 0.7 | 0.076 | 3.7 | 3.0 | 0 | 0 | 0 | 0.3 / 11.0 / 39.3 | 11.0 / 39.3 | 215.3 (215.3) | 3.6 % (3.8) |
| NATO (careless) | 3.0 | 0.304 | 0.7 | 0.3 | 17.3 | 38.7 (42 km) | 2.0 | 2.7 / 3.0 / 46.0 | 0 / 49.0 | 161.7 (180.7) | 2.4 % (1.8) |
| NATO (careful) | 2.3 | 0.379 | 0.7 | 0.7 | 16.3 | 38.7 (42 km) | 2.0 | 1.7 / 7.0 / 47.3 | 0 / 54.3 | 170.7 (176.7) | 2.3 % (1.9) |

A campaign's means over 3 seeds. The Pact's 3 overrun objects are all still held at the end (nothing is retaken: no
counterattack gets back past them); NATO's FSCL moves 3-7 strikes a campaign from air interdiction to BAI (at the fixed
30 km depth none of its strikes is BAI). The neutral's escapes are our mobile objects near its two shallow
penetrations; NATO's troop moves are our divisions marched to its axes. The few strikes on troops cost a lot: a struck object's whole regiment's share
goes with a destroyed object (0.1-0.16 of a division a strike), which is the air war's damage model on a troops area
taken at its word; F6's to calibrate. Two of the balance's bands are just missed at mode 1 with 3 seeds, both on a few
aircraft (the rebels' careless 20.3 % against 0-20 %, 4 aircraft of 20 sorties; the neutral's careful 8.2 % against
0-8 %, 2 of 24); `ground.mode` stays 0 in the data.

**Cost** (`fronts_bench data --ground --gate` and `--player-sector battery --gate`, MSVC Release, the tool alone;
both pass). The ground war alone is F3.6's (it does nothing new without the campaign). In the campaign (10 days, seed 7),
F3.7's parts in ms a campaign day, counted in the ground war's time and its share:

| Faction | Strikes | Supply | Sync | Overrun | Labels | F3.7 | The ground war in all | Share of the campaign |
|---|---|---|---|---|---|---|---|---|
| Warsaw Pact | 0.0028 | 0.0134 | 0.0088 | 0.0329 | 0.0032 | 0.061 | 0.139 | 0.004 % |
| NATO | 0.0024 | 0.0112 | 0.0261 | 0.0490 | 0.0028 | 0.091 | 0.181 | 0.007 % |
| Neutral | 0.0010 | 0.0072 | 0.0091 | 0.0075 | 0 | 0.025 | 0.052 | (under the clock) |
| Rebels | 0.0010 | 0.0065 | 0.0043 | 0.0500 | 0.0001 | 0.062 | 0.142 | 0.37 % |

Nothing hourly that an event can do: the shares are set on a loss or gain (else daily), the places and supply ends on the
line's or an order's version, overrun and escapes on the line's version with each object's projection on the base line
kept until it moves (a step of the line is an offset's lookup an object), the FSCL on its version, the labels when a
package is planned; hourly only the strikes' check and the supply's (twelve objects). The neutral's campaign (4 packages
in 10 days) takes 0.00-0.01 s at mode 0, under `std::clock`'s resolution for a share (F3.6 measured 3.8 %, this run
0.00 s or 8.7 % on the same work): `fronts_bench` now holds a campaign under 0.1 s of CPU to the absolute 20 ms a day
instead (0.052 ms). The whole campaign at mode 1 against 0 (`air_war_balance --cpu`, 3 seeds x 10 days, both postures,
the tool alone): 393.6 s against 385.0 s, 1.022 (the Pact 1.010, NATO 1.033, the rebels 1.15 and the neutral 1.77 on 2-3 s
and 1-2 s: another war once the line moves, as in F3.3-F3.6).

**Identity.** At `ground.mode` 0 nothing is built: `air_war_balance data 20 7` and `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7` are byte-identical (`cmp`) to track F's tip before it (6d9298d, built the same
way); the replay fingerprint (`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages) holds.

**Tests** (`test_ground_air.cpp`, 9): a tie by its owner, whole and at home; a strike of 0.3 coming off the unit
(damage x share x base, the cohesion's 0.1, journalled, never more than it has); a column's strength back up to the
establishment; tied troops keeping their unit's depth behind the moved line; the FSCL by hand (25 km in defence, a day's
objective and the band in attack, the readers' depth less the line's offset, none without the doctrine's block); an
overrun object out of the picture and its reports until ours again; the nation's Pact war (every tied object's share its
unit's, every supply line ending where its troops are, every overrun object without function, behind the line and out of
the picture; the labels as the fixed depth gives them without an FSCL); NATO's FSCL per sector in C2's view; mode 0
nothing tied, overrun, labelled or drawn. `test_ground_picture.cpp`'s air sighting now by the tie.

**Left:**
- F6: the damage a strike on a troops object takes off its unit (a destroyed object is its regiment's share, 0.1-0.16 of
  a division: too much for a package against a regiment's area), `cohesion.air_attack_loss`, `troops.full_supply_days`
  and the reform depth against history; the FSCL's terrain reach. (F6.1: the strike's share is `troops.strike_loss_share`,
  0.1, a strike 1.4-1.8 % of a division; the rest stand, `06_BALANCE.md` As built: F6.1.)
- Escapes and returning overrun objects move at once and straight; on the roads with the route service, and our units
  leaving objects before the line reaches them on an order, are F4's (F4.2's overrun in time and late).
- The deep-strike line beyond the FSCL (the corps' area of interest) and the FSCL told to the enemy's own C2 posts as an
  order with its delay; the other factions' coordination lines.
- A column's exposure on the road (`SupplyColumnMovers`) for the air war to read (F3.6's).
- F3.8 (done: As built: F3.8): strikes off strength and supply below 1 in the units' reports as the unit believes them.

## As built: F3.8 (22 Nov 2026)

Our units' reports and orders are facts in the pipeline (section 10, `command/ground_feed.hpp`: `GroundFeed`, a member of
`CampaignFeed`, so whoever pumps the campaign gets it). Only at `ground.mode` 1 on a theatre
with a country map and a chain that has the posts; at 0 nothing is logged, added or read.

**What the ground war logs** (`GroundWar::SitReports()`, `SitAxes()`, `SitOrders()`, `SitAirs()`, `SitIntels()`; new types
beside `TroopsTie`). Where the picture's reports are compiled (F3.5) our side's are also written down, once, for the whole
war, read by index, never read by the simulation:
- *a unit's report* (`SitReport`): its own return as its staff counted it (`Observe`'s believed strength fraction and its
  factor, so the picture's error to the bit; supplies, `unit.supply` times the same factor; posture, in or out of the
  fight, the sector), the air attacks on it since its last return (a count, and the share of its establishment they cost,
  times the same factor: *believed strikes*, F3.7's left) and the axes it reports (`SitAxis`: depth and pace, whether our
  counterattack is on it, the enemy's formations in contact on it and their believed strength by the contact report's
  error). Scheduled (06:00 and 18:00 local, `reports.division_per_day`), or at once with its cause (reports, asks,
  attack stopped, withdrawn, dispersed, re-formed: `SitCause`, `ReportAtOnce`'s new argument), or an axis's news at once
  from the unit on it (a breakthrough, a halt, our counterattack's start or end: `AxisAtOnce`). An axis is reported by the
  first of ours defending on it (else counterattacking, else the second echelon);
- *an order* (`SitOrder`): our commander's withdrawal permitted, relief sent and counterattack ordered (one to each unit
  sent), by whom, to whom, about what. The units' own internal orders (a division's covering force, its regiments'
  reliefs, echelons committed) are not facts, as section 10 says;
- *a strike* (`SitAir`): the enemy's strike on one of our tied troops objects (`AirStrike`), as the unit counted it;
- *the army's intelligence summary* (`SitIntel`): the enemy's formations our units do not touch, as `army_intel` believes
  them (its error), reaching the army's staff `picture.army_intel_delay_h` after it was compiled.
A report is compiled where the picture's already is (two events a day a side, and the at-once ones): nothing per hour.

**The posts** (`data/command/chain_pannonia.json`, `places_pannonia.json`; the new ones sourced as game values, as
`army_ad_staff` was). `army_ops`, the 3rd Army's operations duty officer at its headquarters, receives the formations'
reports and sends the army's orders (links to the chief of air defence, the national sector and the 7th Division's
commander); `div9_staff_duty`, `swbde_staff_duty` and `tankbde_staff_duty`, the duty officers of the 9th Division, the
south-western brigade and the tank brigade, at places on the map where the Pact deployment puts them (Mezovar, Hradec
Dolni, the capital: fixed, they do not follow their units), linked to `army_ops`. The 7th Division reports by its chief of
staff, and a unit with no post of its own (the army's artillery and engineer brigades, the rebels' regiments, which are the
7th Division's) by the nearest above it that has one, else by the army's operations section itself (no carrier needed, it
holds the report at once). These are all the chain gains: the army, the national sector, the SAM regiment, the division
and the battery commander were there. `Fact::to` (info_net.hpp) names a fact's addressees when they are not a tree: a
post it names wants it, the posts on its way pass it on and so hold it.

**The facts** (`FactKind::Situation`, new; `scenes.hpp`'s paper title "Situation report", `player_inputs.hpp`'s parser):
- *`sitrep:<n>`, a unit's report*: from its duty officer, priority (a combat report goes by the wire if it is whole, else the
  radio, else a despatch rider), subject `situation:<unit>`, the scheduled ones documents, for `army_ops` and, for the
  7th Division and what is under it, the regiment's chief of staff and the battery commander: the regiment's word to the
  battery is the pipeline's own way down;
- *`sitsummary:<n>`, the army's summary*: from `army_ops` at 09:00 and 21:00 local (three hours after the returns), a
  document for `sector_cp_duty` and `army_ad_chief`, made of the latest return of each unit that had reached the
  operations section by then (`Known` at that hour, never more), and the latest depth and pace of each axis it was given;
  made when its hour is settled, like F2.5's conclusions (the earliest an unresolved package could still change what
  reached it), so a campaign pumped every step and one pumped once after a load make the same summaries;
- *`sitorder:<n>`, an order* (`FactKind::Order`, document, priority): from the commander's staff (the army's operations
  section for the army commander, the unit's own for a division's), for the unit it is to, the chief of air defence and, for
  the regiment's division, the regiment and the battery;
- *`ground_intsum:<n>`, the army's intelligence summary* (`Intsum`): from `army_intel`, for the operations section, the chief
  of air defence and the sector;
- *strikes*: each is told to `CampaignFeed::ReportAirAttack` (F2.5's left) from the post of the unit that counted it, at the
  place the unit stood: a sign to the air defence staff's watch where it lies deeper than the enemy's close support
  usually works, otherwise nothing; the count is in the unit's next return too.
`GroundFeed::Reports()`, `Orders()`, `Summaries()` hold what each says in numbers beside the fact; `LatestKnownBy(post,
unit)` and `LatestAxisKnownBy(post, axis)` give what a post holds (what F4's commanders and the regiment's tasking are to
read), `PostOf`, `WithRegiment`, `AirReports`, `AirSigns`, `IntelSummaries` the rest.

**Delays are the pipeline's.** A unit's report is a fact from the hour it was compiled; it reaches each post as the means
of each hop carry it (the wire, the radio, a rider), not by the picture's `own_delay_h` (that stays the staff's collation
in the picture, which F3.5's commanders still read). Two clocks, one source: the facts and the picture carry the same
believed numbers (tested to the bit).

**First person.** The feed reads the ground war's logs of what our units compiled, never its state. The battery commander,
the player's default post, holds the 7th Division's reports, orders and the regiment's word as they reach him; no panel
reads them (F5.1's situation board is the posts' own records).

**Mode 1 on the nation** (`fronts_bench data --feed warsaw_pact nato`: 3 seeds x 10 days with `air_front.mode` 1 so the
air defence staff's watch runs, each campaign twice, with the reports and without them). Per post, the first seed: the
7th Division's chief of staff sent 20 reports and the army's operations section had each after 0.29 h on average, the 9th
Division's duty officer 21 (0.23 h), the south-western brigade's 20 (0.25 h), the tank brigade's 20 (0.23 h), and the army's
own brigades' 20 (held at once). All three seeds:

| Faction | Reports (a day) | To the army | Mean delay | The regiment's staff, of its division's | The battery commander | Summaries, to the sector | Orders | Intelligence summaries | Strikes on our troops counted, told the watch, signs |
|---|---|---|---|---|---|---|---|---|---|
| Warsaw Pact | 303 (10) | 303 | 0.20 h | 60 of 60, 0.58 h | 60, 0.46 h | 60, 60 at 0.33 h | 0 | 6 | 2, 2, 0 |
| NATO | 317 (11) | 317 | 0.20 h | 60 of 60, 0.59 h | 60, 0.46 h | 60, 60 at 0.33 h | 1 (a relief) | 0 | 7, 7, 2 |

Every report reached the army's staff, a few minutes to half an hour after its compile hour (the wire where it is whole;
the army's own brigades' none); the regiment's chief of staff and the battery commander hold the 7th Division's, the
battery commander a little ahead of the staff (the pipeline's shortest way to him does not pass the regiment's chief of
staff, and every post on a way holds what passes through it). The sector's post
and the chief of air defence hold the summaries and no unit's report. **Against F3.7:** the strikes on troops now reach
the watch as the unit counted them (NATO's 7 strikes in 30 campaign days gave 2 signs under the watch's reach, none before
the reports), and the watch's conclusions are the same in 3 of 3 seeds for both: the signs are too few (3 within a few days
in a sector) to move a conclusion. With the reach at 0 the nation's NATO seed 8 gives 4 signs of 4 strikes in 10 days (the
test), so the path from a struck unit to the watch is live; nothing in the data's rule asks for more.

**Cost** (`fronts_bench data --feed`, MSVC Release, std::clock, the tool alone). The pipeline's pump a campaign day (the net's pass over its facts at each campaign step, which is
the pipeline's own cost before F3.8): the Pact 14.87 ms with the reports, 14.13 without, so 0.73 ms for them (1464 facts
against 1095 in three 10-day campaigns: the reports, the summaries and the orders are about 12 facts a day); NATO 12.50
against 11.57, 0.93 ms (1255 against 875). Against the 20 ms budget; `--gate` passes. The reports' own work (adding the
facts, the summary's pass over the reports) is inside that; nothing is read each step but the logs' lengths, and the
summaries are made twice a day. In the ground war the logs are written where the picture's reports are compiled (two
events a day a side and the at-once ones): `fronts_bench data --ground --gate` and `--player-sector battery --gate` pass, the
mean a campaign day 0.026 ms for the Pact, 0.036 NATO, 0.016 the neutral, 0.066 the rebels (F3.7's 0.020, 0.031, 0.012,
0.068; the worst day 0.13 ms; the picture's part 0.012, 0.012, 0.008 and 0.007 against 0.006-0.008: about 0.004 ms a day
for the logs, written inside the picture's own events).

**Identity.** At `ground.mode` 0 nothing is built or logged: `air_war_balance data 20 7` and `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7` are byte-identical (`cmp`) to track F's tip before it (ad4a14f, built the same
way); at mode 1 (`air_war_balance --theatre theatre_pannonia_nation.json --set ground.mode=1 data 3 10`) the output is
byte-identical to the same tip's too (the logs change nothing the war reads); the replay fingerprint (`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages)
holds. The gates pass: `fronts_bench data --ground --gate`, `--ground --gate --player-sector battery` and `--feed --gate` exit 0.

**Tests** (`test_ground_feed.cpp`, 5): our reports reach the army's staff with the pipeline's delays, from the posts that
compiled them (the army's own brigades hold theirs at once); a return is the picture's believed strength to the bit, an
error and all; the sector's post, the regiment and the battery hold none of the other units' reports; what has not arrived
is on its way; each summary made at its hour of the latest returns that had reached the staff by then; the same pumped once
after a load; a chain without the army's post, or ground.mode 0, adds nothing; NATO's order reaches the unit it is to
and the chief of air defence; each strike counted is told to the watch and a sign where deep. `test_command.cpp` and the
ground tests as before.

**Left:**
- The believed line by station (the picture's `line_m`) is not a fact; the returns carry depth and pace by axis and the
  army's summary the latest of each. F5.1's board draws them.
- The enemy's own reports in his pipeline (his orders and returns, F4 for our side only): the enemy's picture is his
  staff's belief and does not go through a pipeline.
- Unit staffs' places are fixed where the Pact deployment puts them (NATO's 9th Division is at Kisvar); they do not
  follow their units (`MovePlace` could, hour by hour). A commander lost is not modelled in the ground war, so no report
  of it.
- Units' internal orders, the regiments' own returns at U1 (the formation's return carries them), the regiment's separate
  word to the battery (the pipeline's way down serves), and the SAM regiment's reports to its division.
- F4: the regiment's tasking and our air defence commanders read `LatestKnownBy` (a scan of the reports: cache by post
  when they are asked every step), F4.2's units moving on orders as `Order`s the same way.
- F6: the spreads and delays, the summaries' and returns' hours, whether a division's report to a SAM regiment should be
  routine or priority.

## As built: F7.1 (24 Nov 2026)

Our army keeps a reserve, and its commander counterattacks the enemy at a penetration's tip, not its whole axis
(`06_BALANCE.md`, Decisions after F6). Only at `ground.mode` 1, and only with the new `"counterattack"` block in
`ground_war.json`. Without the block F3.4's rule stands as built (`GroundTables::counter_at_tip`), so the modes' defaults
and old saves are unchanged.

**The army's reserve** (`HoldReserve`). The 3rd Army's tank brigade is the deployment's reserve in every deployment but the
rebels', whose reserve is a regiment of the 7th Division. It is 12.5 % of the army's weighted strength against the Pact
and NATO, and 18.9 % against the neutral, where the army deploys fewer of its formations. The doctrine's norm is about a
ninth (`reserve_share` 0.111, `fm_100_2_1_1984` p. 2-11). A deployment may give a `reserve_depth_km`:
- 55 km against the Pact and NATO, behind the 30 km security zone and a division's defence 20-25 km deep
  (`vr_poltikovic_1984` pp. 95-96);
- 45 km against the neutral, behind its 20 km security zone.
At Build the reserve moves to that depth behind the line, at its place's station along it, and stands at the map's
nearest anchor, so the route service asks its march from there. It stays free, in reserve, on no axis and out of the
line until an order commits it. That order may be a counterattack or, as before, a relief.

**The tip** (`TipPower`). The counterattack meets the enemy at one breakthrough sector, not every regiment on the axis.
- A formation's share at the tip (`TipShareOf`) is its first-echelon manoeuvre regiments' parts (F3.6), plus its
  artillery's at `levels.support_weight` (0.5). A formation with no regiments counts whole.
- The tip holds as many regiments as one formation's first echelon, out of all the first-echelon regiments the axis's
  attackers bring (`TipFraction`, at most 1).
- The tip's power is the attackers' power, as the army commander believes it, times their share times that fraction.
The second echelons behind the tip do not meet the counterattack. When it goes in, each defending formation fights with
that share (`ResolveCounter`), and its losses are spread by the same share. NATO's covering force still takes the
counterattack alone while it screens.

**When.** The army commander counterattacks the deepest axis that meets all of these (`CounterTarget`):
- it has gone through the security zone, or it is halted more than 1 km inside our ground (by his picture). Restoring
  the state border is a counterstrike's aim (`vr_svatos_1985` p. 28), so the neutral's incursion, which halts in our
  security zone, is a target too;
- its momentum is spent (`MomentumSpent`, his picture): the axis is halted, its pace is under
  `commanders.moving_km_per_day` (2.8 km a day), or its attackers are believed under our degraded line (2/3);
- he has no counterattack on it already, and none since the last ended unless the enemy has advanced since.
Only a candidate whose planning time plus its march on the roads to the axis is within `counterattack.reach_h` (12 h,
a game value) counts toward his force. He orders the attack when that force reaches his counterattack ratio over the
tip: 1.5-2.5, moved by his boldness, 1.8-2.2 for the drawn commanders. The order still stops at the border. He keeps
watching while an axis is a target and a candidate is free (`CounterWatch`), so a halted war still wakes him.

**On the nation** (`fronts_bench data --history`, 14 days x 3 seeds):
- *The Pact.* No counterattack. Our free force is 0.28 of the enemy at a penetration's tip at best (0.29 in the 245 h a
  war its momentum is spent), against 1.8-2.2. Three divisions on one axis outweigh our whole free force even at a
  single breakthrough sector.
- *NATO.* No counterattack. Our free force is 0.51 of the tip at best (0.52 in the 300 h a war its momentum is spent).
- *The neutral.* A counterattack in every war (3 of 3), ordered on 2.57:1 by the commander's picture. On average it
  wins back 2.0 km and ends after 24 h, once it stops gaining, with the line still inside our ground. In the first
  seed it went in at night: against the hasty defence's 1.3 and the night's 0.8 it fell under the first advance class
  and stopped within 3 h.
- *The rebels.* Their raids end at dawn, so no counterattack.
The Pact's war moves a little because the reserve now stands 55 km back rather than at the capital. The air's share of
our losses goes from 20.2 to 17.4 %, and a strike's toll from 1.79 to 1.23 %. NATO's bands are the same to the digit.
Every band is in, or argued as before (`06_BALANCE.md`, As built: F7.1).

**Cost.** The ground war alone (`fronts_bench data --ground`, 30 days x 5 seeds, against 61f4e4e on its own data): the Pact 0.026 → 0.038 ms a campaign day (worst 0.11 → 0.13), NATO 0.037 → 0.044, the neutral 0.016 → 0.022, the rebels 0.068 → 0.074, against a budget of 20. The army commander now keeps deciding at his reaction time while a penetration with its momentum spent is in reach of a free formation (the Pact's decisions a war 62 → 240). The tip's power is a few sums over the axis's attackers, and the reach one road ask per candidate. `--history`'s CPU (14 days x 3 seeds, everything on): the Pact 172.8 → 177.5 s, NATO 113.2 → 116.9 s, with other work on the machine.

**Tests** (`test_ground_commanders.cpp`, "ground counterattack (F7.1)"):
- the tip by hand: a division's share at it, (2 x a regiment + 0.5 x its artillery) over the division, and the tip's
  fraction, 2 of the axis's 5 first-echelon regiments;
- a hand war in which the reserve stays out of the line until committed and the order comes only once the momentum is
  spent. The order's ratio is the free force over the tip (2.66:1), while over the whole axis it is 0.76, where F3.4's
  rule would not have gone in;
- no counterattack when the reserve cannot arrive in time, nor under F3.4's rule;
- an incursion halted 10 km inside our 15 km security zone is counterattacked back toward the border, never past it.
  F3.4's rule ignores it;
- on the nation: the reserve is about a ninth of the army for the Pact, NATO and the neutral, and stands at its
  deployment's depth.
`test_ground_picture.cpp`'s stale-picture test now measures the reserve's loss when it is taken. The order now comes
later, and part of the loss has come back by then.

**Left:**
- The counterattack goes in on the axis, not into its flank: the tip's share is how the flank is weighed.
- The commander does not wait for daylight or open ground. The neutral's counterattacks stall at night in the hills.
- No army or front reserve on the enemy's side beyond its plan's echelons.
- `reach_h` is a game value.

## As built: F8.1 (24 Nov 2026)

Our national command holds a reserve of its own, a tank division out of the 3rd Army's line, and the front's commander
commits it on F7.1's rule at the tip (`06_BALANCE.md`, Decisions after F7). Only at `ground.mode` 1, with F7.1's
`"counterattack"` block, and only with the new `"front_reserve"` block in `data/campaign/ground_defence.json`. Without it
the division is never built and nothing changes: the defaults and old saves are as before.

**The division.** Our order of battle had no tank division standing unused (the 3rd Army's tank brigade is its own
reserve, the 7th Division and the territorial defence are in its line), so one was added: the **11th Tank Division**
(`td_11`, `data/command/oob_pannonia.json`), under the Ground Forces' command, with three tank regiments (94 T-55s, 45
BMP-1s and 18 D-30s each), a motor rifle regiment and an artillery regiment, in this army's own types (briefing §7). It is
the Pact's tank division as NATO drew it (`fm_100_2_1_1984` pp. 4-4 to 4-7, `fm100_2_3` for the organisation only), held
back as a small Pact-model nation would hold one: the front's and the armies' second echelons and reserves up to half the
force (`fm_100_2_1_1984` p. 6-1), the second-echelon division preparing the counterstrike on the main axis
(`vr_poltikovic_1984` p. 96). No new source: every one cited was registered. Its equipment and establishment are
estimates. Every type has a basic model. It has no object in the nation's theatre, so the enemy's air cannot strike it:
adding one would change the theatre at the defaults.

**The block** (`FrontReserveDef`, `ground_data.hpp`): the unit (`td_11`), the command that releases it
(`ground_forces`), its home (`cegled_junction`, the central rail junction 112-131 km behind the line, a place of the
map by id), the front's planning (6-12 h, a game value: a quarter to half a day against a division's 1-3 h,
`fm_100_2_1_1984` p. 5-14) and its reach (24 h, a game value: twice the army's 12 h). The loader refuses a unit or command
not in the order of battle, a unit not under that command, and a unit that is deployed in any deployment or over or under
one that is (`test_ground_data.cpp`).

**Who commits it** (`AddFrontReserve`, `FrontTop`, `TryCounter`). A separate front decision, not the army commander's:
- At Build the division is a formation of ours in reserve at its home, free, on no axis, marked `front`. Its commander
  answers to a commander drawn for the Ground Forces (marked `front`, not the army's top), keyed like every other.
- The army commander never sends it, as a counterattack or a relief (`CounterCandidate`, `FreeFormation`), and the
  army's reserve (`HoldReserve`) leaves it where it is.
- The front's commander decides at his own reaction time while a penetration is a target for F7.1's rule and his reserve
  is free (`FrontWatch`). His rule is F7.1's: the deepest axis whose momentum is spent, his force being every free
  formation of ours (his reserve with the army's free ones) whose planning plus its march on the roads is within his
  reach, over the enemy at the tip as he believes it, against his counterattack ratio (1.5-2.5 by his boldness). He
  orders only if his own reserve is in that force. The planning is drawn in the block's range, a methodical man the long
  end.
- Every counterattack under F7.1 now goes in together, at its slowest formation's hour (`reznichenko_1984` pp. 177-179:
  the line and time of deployment are set for the whole force). Before, each formation went in as it arrived. With one
  reserve that made no difference. With the division and the brigade from different depths, the brigade went in alone
  and the army commander ended the attack before the division was there. F3.4's rule is unchanged.
- The army commander still ends a counterattack that has stopped gaining ground (F7.1).

**On the nation** (`fronts_bench data --history`, 14 days x 3 seeds):
- *The Pact.* The front counterstrikes in every war (3 of 3), on 2.03:1 at the tip by his picture. The division is 64 % of
  the army's power, home 112 km behind the line, and the force goes in 15-19 h after the order. It wins back no ground.
  The Pact's axes are halted at their objectives, which are towns, in hasty defence (1.3 x the town's 1.4). His picture
  had the tip lighter than it was, so the strike's 2.0 is 1.6-1.7 in truth, and that becomes 0.75-0.90 in the combat
  (0.75 at night). That stalls under the first advance class (1.5), and the army commander ends it after 2-4 h.
- *NATO.* The same in every war (3 of 3), on 2.05:1, the force in 11-13 h. NATO's axis is halted at its objective, a town
  75 km in, and the strikes go in at night: 1.9 in truth, 0.82 in the combat. None gains ground.
- *The neutral.* A counterattack in every war as before: the front's in one (on 4.13:1), the army's in the others, now
  going in together. The line won back is 6.4 km on average (2.0 before), and the enemy's axes at or near their
  objective fall from 1.00 to 0.67.
- *The rebels.* Their raids end at dawn: no counterattack.
To win ground against a town in hasty defence, the raw ratio would need 2.7 by day and 3.4 at night. The commander does
not weigh the ground or the dark, and does not choose an axis still moving in the open (F7.1's "Left").

**Cost.** The ground war alone (`fronts_bench data --ground`, 30 days x 5 seeds, against cc635af on its own data): the
Pact 0.037 → 0.032 ms a campaign day (worst 0.12 → 0.14), NATO 0.040 → 0.041 (0.12 → 0.14), the neutral 0.019 → 0.021,
the rebels 0.069 → 0.070, against a budget of 20. In the campaign (10 days): the Pact 1.99 → 2.01 ms, NATO 2.26 → 2.41,
the neutral 0.72 → 0.99, the rebels 1.81 → 1.75. The front's commander decides only while a penetration is a target, and
his rule is the army's with one more formation.

**Tests** (`test_ground_commanders.cpp`, "front reserve (F8.1)"; `test_ground_data.cpp`):
- a hand war: the division is held out of the line, free and at its home, until the front's commander orders it; the
  army's brigade alone is under the army commander's ratio (0.44 against 1.89); the order is the front's, on his ratio,
  and its value is his force over the enemy at the tip, by hand (3.58:1); the force goes in together, the division in it,
  6-24 h after the order;
- with a reach of 1 h no counterstrike is ordered and the division never leaves, though its commander keeps watching;
- without the block the division is never built and the war's every order is that of the war whose order of battle
  never had it; with the block but without F7.1's rule, none either;
- on the nation, for every faction: the 11th Tank Division at Cegled, the Ground Forces' commander over it, out of every
  axis's line; without the block one formation fewer;
- the loader's refusals: deployed, over a deployed unit, not in the order of battle, not under its command, a home not on
  the map, an unknown key.
F3.4's counterattack test now finds the tank division among the force rather than the first to start.

**Left:**
- The front's commander weighs the raw ratio at the tip, not the defender's ground, posture or the dark, so his
  counterstrikes go into towns at night and stall. Weighing them (or choosing an axis still moving in the open, or waiting
  for daylight) is a design call.
- The division has no object in the theatre: the enemy's air cannot find or strike it.
- `planning_h` and `reach_h` are game values; the division's equipment and establishment are estimates.

## As built: F8.2 (24 Nov 2026)

If a formation's command post or artillery has been hit, it is judged against its doctrine's three-quarters line
(`hq_hit_ineffective_below`, 0.75) instead of its commander's ask line. This holds for as long as its own staff knows
of the hit. The source is `mt_stashek_losses` p. 7: a formation is ineffective at about 24-25 % losses when its command,
rockets and artillery are hit. The decision is in `06_BALANCE.md`, Decisions after F7.

The rule needs the new `"command_hit"` block in `data/gameplay/ground_war.json`. Without it, no command post is tied, no
hit is recorded, and every war matches the war before it entry by entry. The defaults and old saves are unchanged; the
defaults do not build the ground war at all.

**What counts as a hit** (`CommandHitKind`):
- *The command post.* This is a theatre object of kind command post or headquarters (not a decoy) whose owner in the
  order of battle is itself one of our formations (`TieTroops`, `CommandTies()`).
  - On the nation there are three: `mrd9_hq` (the 9th Division), `sw_brigade_cp` (the south-western brigade) and
    `div_ad_cp` (the 7th Division's air defence command post, which our order of battle lists as the division's).
  - The army's `army_hq` and `army_cp_alt` are not tied. They belong to the 3rd Army, which is not a formation of the
    ground war.
  - At each hour's start, a fall in the object's working share since the staff last saw it counts as a strike
    (`CommandPostsSeen`). A fresh fall after a repair is a new hit.
- *The artillery, struck from the air.* This is a strike on a troops object (F3.3's ties) that belongs either to the
  formation's support regiment (a regiment with no tanks) or to a formation with no tanks of its own
  (`TroopsTie::artillery`).
  - On the nation these are `mrd7_artillery`, `mrd9_artillery` and `army_artillery` (the army's artillery brigade).
  - `sw_artillery` does not count. Its owner is the south-western brigade itself, which has tanks, so a strike there is
    a strike on the brigade, not on its artillery.
- *The artillery, in the fighting.* At U1 a formation's regiments are actors. A hit is recorded the first time its
  support regiment's own count falls under `support_hit_below` (0.85, a game value) (`RegimentsAfterHour`). At U0 the
  support regiment's losses are not kept apart from the formation's, so this trigger works at U1 only.

**First person: the staff learns it.** Nothing reads the strike from the truth.
- The command post's own staff knows its post is hit at once, at the next hour's start. This matches F3.6's `SitStruck`:
  the staff that was hit knows.
- The artillery's hit is known only when its report reaches the formation's staff, `picture.own_delay_h` (2 h) after
  the strike or the loss (`PushHit`).
- When the staff learns of a hit (`HitLearned`):
  - the journal gets a note (`GroundNote::CommandHit`): "X's staff learns its command post / its artillery, struck from
    the air / in the fighting hit: judged against the three-quarters line until command is restored, in N h";
  - the formation's `hit_until_h` is set;
  - its line crossings are checked again against the raised line (`AskLine`);
  - its commander is woken after his reaction time.

**The commander's rule.** While the hit is known (`HitKnown`), his ask line is the higher of two lines: his own (half, or
earlier for a flexible man) and the doctrine's three-quarters line (`UnitDecision`, `AskLine`). His senior answers as
before (`AnswerRequests`: permit, relieve or refuse).
- Each side uses its own doctrine block: ours is `ground_defence.json`, the Pact's is `ground_warsaw_pact.json`, and
  both set 0.75.
- A doctrine of breakpoints (NATO's, the rebels') has no such line, so this rule does not apply to it.

**It wears off** as command is restored.
- The command post: in the doctrine's `restore_control_h`. Ours is 1-3 h (`mt_stashek_losses`: about 1 h through a
  forward command post, otherwise 2-3 h).
- The artillery: in `artillery_restore_h`, 12-24 h, a game value. The theatre repairs a troops object in 24 h.
- Each time is drawn in its range by a keyed draw of the formation and the hit's number. A second hit extends the
  first.
- When the hit wears off, a formation that has not asked is judged against its own line again (`HitWornOff`).
- The enemy's doctrine has no `restore_control_h`. Its command posts are not tied, because the air war strikes only
  ours. Its artillery's hit in the fighting uses `artillery_restore_h`.

**Not forced:** no war is lengthened and no formation is kept in contact. The war's length and tempo stay the enemy's.

**On the nation** (`fronts_bench data --history`, 14 days x 3 seeds; `06_BALANCE.md`, As built: F8.2):

| | The Pact | NATO |
|---|---|---|
| Command post hits our staffs learn a war | 2.0 | 4.7 |
| Artillery hits a war | none | none |
| Hours a war with a hit known | 2 | 7 |
| Our formations under 75 % at some hour | 1 | 3 |
| Of those, under 75 % while their staff knew a hit | 0 | 1 |

None withdraws, for three reasons:
- A command post is restored in 1-3 h, before the division commander's reaction time (2-4 h) has run.
- The enemy's air never struck our artillery's objects. Its strikes on troops fall on the regiments' areas and the
  command posts.
- In the fighting, the support regiment's losses, at the support weight, keep it above 0.85.

A sensitivity run on a copy of the data restored the command post in 6-12 h instead. With that, NATO withdraws one
formation of nine (0.11) and every other band is as before. The Pact still withdraws none: its one formation under 75 %
was not hit then. See `06_BALANCE.md`.

**Cost.** Below the noise (`06_BALANCE.md`, As built: F8.2). The command posts are read once an hour (three objects on
the nation), and a hit is an event.

**Tests** (`test_ground_commanders.cpp`, "command hit (F8.2)"; `test_ground_data.cpp`):
- A hand war:
  - a formation at 66 % whose command post is hit asks within the restore time, is relieved and withdraws;
  - a formation at 66 % with no hit reports and never asks.
- The hit is known only as its staff learns it:
  - a loss alone is no hit, nor is a strike on the army's post or on a tank regiment's area;
  - the post is known at the next hour's start, and the artillery 2 h after the strike;
  - a fresh fall after a repair is a new hit.
- It wears off: the post at its restore time (8 h, a test value), the artillery inside 12-24 h.
- Without the block, no post is tied and no hit is recorded. With the block and no hit, a four-day war is the same entry
  by entry and strength by strength.
- On the nation (the Pact's war), the ties are the three command posts and the three artillery objects above.
- The loader refuses an unknown key, a line over 1, a range out of order, a restore of 0 and an unsourced value.

**Left:**
- The enemy's air does not pick our artillery: on the nation its strikes on troops never land on the artillery's
  objects. Its target choice belongs to the air war (13.8).
- The command post's sourced restore time (1-3 h) is shorter than the division commander's reaction time (2-4 h). At the
  sourced values, a hit on the post alone never brings a withdrawal.
- At U0 the support regiment's losses in the fighting are not kept apart, so the fighting's artillery trigger works at U1
  only. `support_hit_below` and `artillery_restore_h` are game values.

## Done when

Each faction's ground war (or the owner's choice for the rebels and the neutral) moves the line of contact by its
units' combat and its officers' decisions, the air war's troops, supply and close support follow it, our side's posts
learn of it by reports, it costs under its budget, and old saves replay unchanged. Its outcomes against history are
F6's.
