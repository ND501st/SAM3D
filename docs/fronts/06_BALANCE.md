# 6. Balance (F6)

The fronts against history, the air war's bands kept or argued on the moving front, the player's effect kept small,
and the budget held. Same way as track B's B14 (`docs/air_war/09_BALANCE.md` §17): every band in
`data/air_war/balance_bands.json` with its anchor (a registered source or a game value), read by
`simcore/air_war/balance_bands.hpp`, each tool exiting with 2 when a band is out.

## 1. The tool: `fronts_bench`

`simcore/examples/fronts_bench.cpp` (begun in F3.3 for the cost, finished here):

```
./simcore/build/fronts_bench data [--faction nato] [--days 30] [--seeds 5] [--player-sector] [--air-front] [--ad]
./simcore/build/fronts_bench data --gate          # the budget as a gate: exit 2 above it
./simcore/build/fronts_bench data --history       # the fronts against the bands below
./simcore/build/fronts_bench data --player        # the player's effect: careful against careless battery, same seeds
```

It runs the nation's theatre headless (the air war, the ground war, our AD strategy), prints per faction and seed:
the line's advance by sector and day, losses by side, cause and posture, the air's share of losses, the security zone's
and the covering force's hours, the air front's moves and their latency, coverage lost and rebuilt; then the band table
and the cost table by part. Its JSON is what `tools/map_view.py --front` draws.

## 2. The bands

| Band | Range | Anchor |
|---|---|---|
| A division's daily losses in contact | 1.0-6.6 % by action | `fm_101_10_1_1976` table 5-8 |
| Losses over an operation, by level | army 0.6-1.2 % a day, division 3-6 %, regiment 10-16 % (WWII; 2-3 times lower in depth) | `mt_novoselov_replacement` p. 4 |
| The Pact's advance | 25-30 km a day breaking through, 40-50 in depth; the army's 100-150 km in 3-4 days as the plan, not the expected outcome | `mt_kurkov_tank_army` p. 9; `fm_100_2_1_1984` p. 4-4 |
| An attack below the threshold | no sustained advance under 1.5:1 | `hero_1972_oralfore` p. IV-2 |
| Our security zone | a reinforced battalion's line holds a brigade 1-2 h; the zone 20-40 km deep gives the main defensive zone hours to a day | `vr_krizek_1985` p. 34 |
| NATO's covering force | 1-3 days of delay as told; tanks 73 % lost on day 1 heavily committed | `fm_100_5_1976` p. 5-12; `fm_101_10_1_1976` table 7-7 |
| The air's share of a sector's ground losses | about 5-25 % | SAM3D estimate bounded by `werrell_2005` p. 154 and `nwc_83_027` pp. 20-21 |
| The air front's latency | under 24 h from the gate holding to close support at the new depth | `werrell_2005` p. 153 |
| The air front pulled back | after 1-3 days above the loss line | `werrell_2005` pp. 151, 154; `jvl_1973_losses` |
| Helicopters against an intact defence | 40-70 % of a raid lost when they break the rule | `werrell_2005` p. 153 |
| Coverage rebuilt unopposed | a third of the lost fire units back in 2-3 weeks | `frus_1969_76_v23_d179` p. 615 |
| Air defence losses in an operation | 25-30 % irrecoverable | `mt_rozhdestvenskiy_depth` p. 7 |
| A SAM unit's move | S-75 class 10-15 h; no unit more than 3 moves a day | `mt_1973_nvn` pp. 10-11; `mt_rozhdestvenskiy_depth` pp. 6-7 |

Each band's tool is `fronts_bench`. A band the model misses for a reason the period's numbers cannot fix (a 1985
force against WWII rates) is argued in `09_BALANCE.md` as B14's were, not forced.

## 3. The air war's bands on a moving front

Track B's bands (`air_war_bench`, `air_war_balance`) are re-run on the nation with every F mode on. Expected to move:
the Pact's and NATO's losses (close support deeper where the air front advanced, against our ambushes), sortie mixes
(more close support later in the war), the rollback's end. Each band either holds, or is argued and re-anchored in
`09_BALANCE.md`. `crew_balance`, `helper_coverage` and `ground_check` stay green.

## 4. The player's effect

The principle: the player changes the war only a little, by the assets kept and the enemy's freedom of action denied.
`fronts_bench --player` runs each faction's war twice on the same seeds, the battery careful (moves, emission
discipline, a good engagement record) and careless, and reports the difference:
- the line of contact in the battery's sector at day 7: at most about 5 km (a game value to set here);
- the enemy's air front in the battery's sector: at most a step, and later by up to a day or two;
- elsewhere in the nation: no difference beyond the draws' spread.

A larger effect means the battery weighs too much in the enemy's sector cover (F2.2's baseline) or in our sector
priorities (F4.1). The outcomes' ledgers (`campaign/outcomes.hpp`) gain the air front's delay in the battery's sector
as the enemy's freedom of action denied.

## 5. The budget as a gate

`fronts_bench --gate` fails above the README's budget: 20 ms CPU per campaign day for the nation at U0 (worst day 60),
15 ms more for the player's sector at U1, 1 ms per planning cycle for the air front, 5 ms a day for our AD strategy.
`air_war_balance --theatre theatre_pannonia_nation.json --cpu` with every F mode on at most 1.05 times off. Both run
in `tools/verify.ps1` as a trend; from F3.3 on, the gate is each task's merge check.

## Done when

The fronts' bands are in `balance_bands.json` and green or argued, the air war's bands re-run on the moving front,
the player's effect bounded, and the budget held.

## As built: F6.1 (23 Nov 2026)

**The tool.** `fronts_bench data --history [--days 14] [--seeds 3] [faction ...] [--gate]` runs each faction's war on the
nation (`theatre_pannonia_nation.json`, seeds 7, 8, 9) with `ground.mode`, `air_front.mode` and `ad_strategy.mode` at 1
and the pipeline pumped (the feed and the sector commanders' orders, as the campaign screen runs it), samples it every
hour and prints, per faction, the band table (PASS, FAIL, or ARGUED: a band whose miss is argued below, printed and never
failed) and the accounts behind it: formation-days in contact, the orders a war, our formations' strength at the end,
the best counterattack ratio a penetrated axis offered, losses on the axes, the air front's moves and its gates hour by
hour, our air defence's losses and moves, the air war's packages and losses, and the advance by the ratio's class. With
`--gate` it exits 2 when a band fails. The bands are `balance_bands.json`'s `"tool": "fronts_bench"` entries (33), read by
`balance_bands.hpp` (which gained `"status": "argued"`). A day in contact is a formation's day with 12 h or more in the
fight; its loss is its battle losses (fighting and air) that day over its strength at the day's start. An army's day is
its side's battle losses over its strength at the war's start, from the offensive's first hour to the end.

The ground war keeps a passive tally for it (`ground_war.hpp`; nothing reads it, the war is unchanged): each
formation's losses by cause (attacking, defending, from the air, as stragglers, at rest) and hours in the fight, each
axis's losses attacking and defending and the hour it reached our main defensive zone, and the orders given by kind
(`OrderCount`); the campaign gained `AirFrontRewards()` for the gates' account.

**What changed.** One game value, new: `ground_war.json` `troops.strike_loss_share` 0.1. F3.7 took a strike's damage to a
tied troops object off its unit one for one: a destroyed troops area was its regiment's share, 10-20 % of the division a
strike, and the air made 38-60 % of our battle losses. Troops dispersed and dug in lose their men and vehicles far less
than their positions and stores are hit, so a tenth of the damage is now the unit's: a strike takes 1.4-1.8 % of the
division and the air 8-20 % of our battle losses (the band's estimate, `werrell_2005`, `nwc_83_027`). And one band
re-anchored by a source: our (the defender's) division's day in contact runs to 8.0 % with `fain_1988_posture`'s
defender (7.73 % a day over 62 WWII divisional engagements), above FM 101-10-1's planning 6.6 %. Nothing else was tuned:
the advance, the loss table, the commanders' lines and the air front's gates are sourced and land in their bands or miss
for the reasons argued. A Pact cover gate at 0.30 (the middle of Krasovskiy's 60-80 % neutralised) was tried and
reverted: where the Pact's gates held, nothing worth flying for lay a step deeper, so it changed nothing.

**The bands** (14 days x 3 seeds; before is the same build on F5.3's data, after is F6.1's; the neutral's and the
rebels' wars are border fights and raids of 1-20 sorties, printed, with no ground band but the generic ones):

| Band | Anchor | Range | Pact before → after | NATO before → after | |
|---|---|---|---|---|---|
| Our division's day in contact | `fm_101_10_1_1976` t. 5-8, `dupuy_1986_attrition`, `fain_1988_posture` | 1.0-8.0 % (was 6.6) | 8.73 → 7.03 | 5.39 → 5.00 | green |
| The enemy's division's day in contact | `fm_101_10_1_1976`, `dupuy_1986_attrition` | 1.0-6.6 % | 1.44 → 1.57 | 2.18 → 2.33 | green |
| Our army's day | `mt_novoselov_replacement`, `helmbold_1996_echelons` | 0.2-1.2 % | 2.49 → 1.26 | 3.39 → 2.47 | argued |
| The enemy's army's day | the same | 0.2-1.2 % | 0.31 → 0.35 | 1.69 → 1.80 | Pact green, NATO argued |
| Attacker's losses over the defender's | `dupuy_1986_attrition` verities 7, 10, fig. 19; `helmbold_1995_attrition` | 0.5-2.5 | 1.16 → 1.29 | 2.57 → 2.56 | Pact green, NATO argued |
| An axis's day of attack | `mt_kurkov_tank_army`, `fm_100_2_1_1984`, `hero_1972_oralfore`, `ssi_1991_iran_iraq`; NATO `fain_1988_posture` | Pact 5-30, NATO 2.8-30 km | 14.2 → 13.8 | 10.2 → 9.3 | green |
| The fastest day | `fm_100_2_1_1984`; `fm_100_5_1982` | Pact ≤ 50, NATO ≤ 80 km | 23.1 → 23.1 | 30.0 → 30.0 | green |
| Advance under the stall ratio | `hero_1972_oralfore` | ≤ 0.5 km | 0 → 0 | 0 → 0 | green |
| The security zone to the main zone | `vr_krizek_1985`, `vr_poltikovic_1984` | 3-30 h | 29 → 29 | 36 → 36 | Pact green, NATO argued |
| The enemy's axes at or near the objective | `dupuy_1986_attrition` fig. 19, verity 2; `fain_1988_posture` | 0.4-0.9 | 1.00 → 1.00 | 0.67 → 0.50 | Pact argued, NATO green |
| Our formations a day in the fight, withdrawn | `fain_1988_posture`, `mt_stashek_losses` | 0.05-0.6 | 0.17 → 0 | 0 → 0 | argued |
| Wars with a penetration: we counterattack | `reznichenko_1984`, `vr_poltikovic_1984`, `fain_1988_posture` | 0.3-1.0 | 0 → 0 | 0 → 0 | argued |
| The air's share of our battle losses | estimate bounded by `werrell_2005`, `nwc_83_027` | 5-25 % | 60.4 → 20.2 | 37.6 → 8.5 | green |
| A strike on troops, of its division | game value | 0.2-3 % | 16.4 → 1.79 | 10.1 → 1.38 | green |
| The air front's latency; its pull-back | `werrell_2005`; `jvl_1973_losses` | 0-24 h; 12-72 h | none | none | not measured: no step |
| Our radar SAM fire units lost at the end | `mt_rozhdestvenskiy_depth` p. 7 | 10-40 % | 25.8 → 24.2 | 12.1 → 18.2 | green |
| Of those wrecked, working at the end | `frus_1969_76_v23_d179` | 33-100 % | 63.8 → 63.8 | 75.0 → 62.5 | green |
| An S-75 class move, heard to ready | `mt_1973_nvn` | 7.5-20 h | 15.5 → 15.5 | none (no move) | green |
| A unit's moves begun in a day | `mt_rozhdestvenskiy_depth` pp. 6-7 | 1-3 | 1 → 1 | 1 → 1 | green |
| The enemy's loss a sortie (`air_war_balance`'s band, read beside) | `09_BALANCE.md` §§ 8, 14 | Pact 1.5-4.5, NATO 1.0-3.5 % | 3.73 → 3.48 | 2.54 → 2.50 | green |

Not measured on the nation: NATO's covering force (it appears only on our counterattack, which never goes in) and the
helicopters against an intact defence (no crisis entry, so the helicopters' rule is never broken); both are exercised
by the hand cases of `test_ground_commanders.cpp` and `test_air_front.cpp`.

**Argued** (each in its band's `source`, `"status": "argued"`):
- *Our army's day* (Pact 1.26, NATO 2.47 %) and *NATO's army's day* (1.80 %): the band is a large army's, most of it out of
  contact. Our army is a corps' size and stands whole in the enemy's path; NATO's attacking force is a corps, all of it
  engaged for two weeks (`helmbold_1996_echelons`: a corps' short intense battles to about 10 % a day). Their divisions
  are inside the division bands. Over 21 days the Pact's war measures 0.87 %: its axes reach their objectives by about day
  7, and the measure divides by the war's days.
- *NATO's exchange* (2.56): NATO attacks at 1.5-2.5:1 for half its hours (the intense class, 476 of 933 hours a war, and 105
  stalled), below history's usual ratios, and pays as FM 101-10-1's table says an attacker at a low ratio does.
- *NATO through our security zone* (36 h): at the intense class's pace, slower than the Pact's tempo the zone's "hours to a
  day" was written against.
- *The Pact wins every axis*: it attacks at 4:1 and over for three quarters of its hours, where history's attacker wins
  nearly always; Dupuy's 61-75 % is over battles at every ratio.
- *No withdrawals* (the open item): our formations are in the fight 3-5 days each (Pact) or most of the war (NATO) and end
  at 82-88 % of their establishment (80-85 % the lowest after 21 days), repairs returned and columns brought; the
  sourced line of withdrawal is half (`mt_stashek_losses`), Fain's defender breakpoint about 40 % losses. Before F6.1 the
  one natural withdrawal (a Pact war, a relief) came from the oversized strike toll. The hand cases exercise asking,
  refusal, relief, dispersal and re-forming.
- *No counterattacks* (the open item): the free formations and the sector's second echelon our army commander may send
  come to 0.06 (Pact) and 0.19-0.20 (NATO) of a penetrating axis's attackers at best, against his 1.5-2.5. Our order of
  battle holds no army reserve that could; lowering the ratio to that would send a brigade into three divisions.
- *The air front stays at the border* (the open item, "timid"): both factions' gates are sourced. The Pact's cover and mobile
  gates held 162-173 sector-hours a war in a few sparse sectors, where nothing worth flying for lay a step deeper (the
  reward 0 where they held); NATO's mobile gate (half our mobile air defence believed destroyed) held 8 sector-hours a
  war, never the 6-24 h its delay needs. Our radar SAMs lose 18-24 % by the end, in their band; Krasovskiy's 60-80 %
  neutralised is not reached in two weeks, as the IAF's was not over Suez until the crossing overran the sites (`werrell_2005`).
- *The Pact's loss rate with the fighter cover* (F4.3: 3.92 against 3.39 %): with every mode on it is 3.48 % (3.73 before),
  NATO's 2.50 %, both inside `air_war_balance`'s nation bands; no value moved for it.

**Cost.** `--history` with everything on and the bench's hourly accounts: the Pact 6.4 s of CPU a campaign day, NATO
4.0 s, the neutral and the rebels 0.03-0.07 s (MSVC Release, with other work on the machine). The gates (`--ground --gate`,
`--ground --gate --player-sector battery`, `--feed --gate`) pass: the tally is a few additions an hour.

**Identity.** At the modes' defaults `air_war_balance data 20 7` and `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7` are byte-identical to track F's tip before F6.1 (83caa77, built the same way);
the replay fingerprint holds (`test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages).

**Tests.** `test_ground_air.cpp`'s strike test moved: the loss is the damage times the share times the unit's strength
times `troops.strike_loss_share` (between 0 and 1), the object's working share falling by the damage times it, the strike
that takes all it has sized by it, and the reinforcement test's column smaller than the strike's loss. `test_situation_map.cpp`'s
shared NATO run goes to 120 h: its axes at 72 h came from a unit the old toll broke before NATO's corps attacked (96 h).
`test_ground_commanders.cpp` gains the tally's test: every
order counted as logged, the losses booked by axis the formations' own, the zone's hour after the axis's start.

**Left:**
- The army's day divides by the war's days from the offensive's start: a war whose axes stop early reads low. Measure it
  over the days an axis attacks.
- An army reserve for our order of battle (or a counterattack weighed against the penetration's tip, not its whole first
  echelon) if counterattacks on the nation are wanted: the owner's decision.
- Withdrawals on the nation need losses our formations do not take in two or three weeks; whether the repair return
  (0.25 a day, a game value) is too generous is for a later pass with a source.
- The air front's reward reach (40 km beyond the front, a step deep) finds nothing in the Pact's sparse sectors; whether
  its gates should weigh the targets already in reach is F6.2's, with the player's effect.
- The neutral's and the rebels' wars have no ground bands of their own (border fights, raids).
- NATO's covering force and the helicopters' broken rule unmeasured on the nation (above).
