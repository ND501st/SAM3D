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

## As built: F6.2 (23 Nov 2026)

**The air war's bands on the moving front.** `air_war_balance` and `air_war_bench` gained `--fronts`: `ground.mode`,
`air_front.mode` and `ad_strategy.mode` at 1, and in `air_war_balance` our side's pipeline pumped after every hour's step
(the units hear their orders from it), the way `fronts_bench --history` runs the war; `air_war_bench` also takes
`--theatre`. A band may now be written for that run alone (`balance_bands.json`'s `"modes": "fronts"`, read before a band
of any modes; `balance_bands.hpp`), and `BandRow` prints an argued band out of its range as ARGUED, never failed. None was
needed. The defaults are unchanged.

- `air_war_balance --theatre theatre_pannonia_nation.json --fronts --cpu data 10 7`: every band of the nation holds for
  all four factions and both postures (`docs/air_war/09_BALANCE.md` section 26, the table before and after). The Pact's
  loss a sortie 3.6/3.3 % → 3.2/3.1 % (careless/careful), NATO's 1.9/2.0 → 2.1/2.2 %, the exchanges and the on-call
  latencies inside theirs. Close support rises from none on day 1 to 2-6 % of the packages on days 3-5 as the line moves
  (NATO most), the mix otherwise the same; the air front makes no step in a week in any war (F6.1's "timid" item: its
  sourced gates), so the rollback's end and the low-level work are where they were.
- `air_war_bench data 400 --theatre theatre_pannonia_nation.json --fronts`: the same table, byte for byte, as without
  (the header aside), all 17 bands green. Its benchmarks fly their analogues through the resolver, the abstract fight,
  the delays' draws and the notices on small theatres of their own with no front line, so the modes reach none of them,
  and the nation's defence values are the tuned theatre's.

**The player's effect: `fronts_bench --player`.** Each faction's war twice on the same seeds (7, 8, 9; 7 days), every F
mode on and the pipeline pumped, the battery careless and careful as `air_war_balance` has them (the moves and the
emission discipline the headless campaign exposes for the player's battery; its engagement record is not set but follows
and is measured: its kills, whether it was wrecked, what the enemy knew of it). The battery's sector is where it stands at
the war's start. Four new bands, game values (`"player_*"`): the line of contact there at the end within 5 km careful
against careless on average; the air front there at most a step apart; its first step later (or earlier) by at most 48 h;
elsewhere the line's difference at most the draws' spread (the same posture's seeds against each other).

| Faction (the battery's sector) | Line apart there | Air front apart | Elsewhere: apart / spread | Held by the battery (ledger), careless / careful | Kills | Wrecked |
|---|---|---|---|---|---|---|
| Pact (northern_forest, no axis) | 0.00 km | 0 steps | 0.00 / 0.00 km | 3.7 / 2.3 h a war | 0 / 0 | 67 / 0 % |
| NATO (western_crossings, 34-38 km deep) | 0.49 km (careful +0.34) | 0 steps | 0.03 / 1.45 km | 0 / 0 h | 1.3 / 0.7 | 0 / 0 % |
| Neutral (border_forest) | 0.00 km | 0 steps | 0.00 / 0.00 km | 0 / 0 h | 0.7 / 0.3 | 0 / 0 % |
| Rebels (rava_crossing) | 0.00 km | 0 steps | 0.00 / 0.00 km | 0 / 0 h | 0.7 / 0 | 0 / 0 % |

Every band holds; the effect is well inside its bound, so nothing in F2.2's sector cover baseline or F4.1's priorities
needed changing. The ground war runs on its own draws, so a careful battery leaves the other sectors' lines all but as
they were (0.03 km against 1.45 km between seeds). The air front never steps in the battery's sector, so its delay cannot
be measured as a step's hour; the ledger bounds it instead: a step needs its gates open for the chief's 6-24 h, and the
battery's entry held them shut, with every other gate open, 0-11 h a war (the Pact's seed 8 careless the most). A
sensitivity run with `--set air_front.reward_reach_m=0` (F6.1's question: the targets already in reach weighed) moved the
Pact's air front a step in one seed elsewhere and in no battery sector, and left every player band where it was.

**The outcomes' ledger** (`campaign/outcomes.hpp`) gains the enemy's air front in the battery's sector as the enemy's
freedom of action denied: `CheckAirFront` reads, on the last planning cycle's measure, the believed radar SAM cover over the
sector's next band and our mobile air defence there believed destroyed with the battery's entry and without it
(`CoverWithout`: its believed working share off the sums, the baseline kept, so a battery gone counts as one destroyed),
against the gates as the chief's traits move them (`air_front.hpp`'s `GatesOf` and `AirFrontCap`, taken out of
`DecideAirFront` unchanged), and whether every other gate was open. `OutcomeLedger::AirFront` adds the hours the battery's
entry alone held the gates shut (`AirFrontHeldH`) and counts the steps made in its sectors while watched; the debrief
prints them. The mission session checks it every minute (`mission_session.hpp`); nothing in the simulation reads it.

**Tests.** `test_outcomes.cpp`: the cover without the battery by hand, equal to the book's own measure with it gone; the
gates held shut by it, not when another gate is shut or they open with it; the ledger's hours and steps, across a move to
another sector; in a running war the check on the staff's own measure, none at `air_front.mode` 0.

**Identity and gates.** At the defaults `air_war_balance data 20 7` and `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7` are byte-identical to track F at 8c073bb, and `air_war_bench data 400` too; the
replay fingerprint holds. `fronts_bench --ground --gate`, `--ground --gate --player-sector battery`, `--feed --gate` and
`--history --gate` pass. `crew_balance`, `helper_coverage` (the same 29 gaps as track F's own rows) and `ground_check`
are unchanged.

**Cost.** `--player` 188 s of CPU for the Pact's six weeks of war, 122 s for NATO's, a few seconds for the neutral and the
rebels; `air_war_balance --fronts` on the nation 6.4-7.1 s a campaign day for the Pact and 4.2-4.5 for NATO, most of it the
pipeline's pump, as F6.1's `--history`.

**Owner decisions.**
- The bound's values are game values set here: 5 km, a step, 48 h, the draws' spread. Tighter or looser is the owner's.
- The careful and careless battery are `air_war_balance`'s postures; the headless campaign has no control of the
  battery's engagement record (it fights as an abstract defence with its object's lethality). A crew tier or a
  lethality scale for the record would be a new player input.
- The air front stays at the border for a week in every war (F6.1's open item); while it does, the player's effect on it
  is bounded by the ledger's held hours, not measured as a step's delay. Whether the gates should weigh the targets
  already in reach (`air_front.reward_reach_m`) is the owner's: the sensitivity run shows it alone does not move the
  front.

**Left:**
- The player's effect on a front that moves in the battery's sector (a war or a theatre where the air front steps there).
- The battery's sector by where it stands at the war's start; a war in which it moves to another sector is measured there.
- `air_war_balance --fronts`'s CPU against the 1.05x line of section 5 with the pipeline left out (the pump dominates).

## Decisions after F6 (24 Nov 2026)

The owner left F6.1's and F6.2's open decisions to Claude. They were taken as follows.

- **Kept as built:**
  - `troops.strike_loss_share` stays at 0.1.
  - F6.1's ten argued bands stay argued, with their reasons as written.
  - The player's bounds stay as set: 5 km of line, one step of the air front, 48 h, and the draws' spread.
- **The repair return** stays at 0.25 a day. Withdrawals are measured again once F7.1 and F7.2 make the fighting
  heavier. Turning the return down without a source would only force the band.
- **The engagement record is not made a player input.** The headless campaign keeps its abstract defence. What the
  player's own missions do already reaches the campaign through the records (the written record is the war's memory).
- **Counterattacks:** F7.1, below.
- **The air front:** F7.2, below.

### F7.1: army reserves and the counterattack at the tip

- Each army of ours holds a reserve: a mobile formation from its order of battle, kept back by doctrine.
- The counterattack's ratio is weighed against the enemy formations at the penetration's tip and flanks, not against the
  whole axis's first echelon.
- The attack is launched when the penetration's momentum is spent (its advance slows, or its losses rise) and the reserve
  can reach the flank on the roads in time.
- After F7.1, counterattacks fire in some wars and not in others, by the sourced ratio, and F6.1's counterattack band
  comes out of argued.

### F7.2: the air front follows its own ground offensive

- A sector's reward also counts the enemy's need for close support of its own axes attacking there. Its air chief moves
  the air front behind an advancing axis, so that it does not stand at the border while the line is 50 km in.
- The sourced risk gates still hold. A sector whose air defence is intact keeps the air front back, whatever the reward.
- After F7.2, the Pact's air front steps where its axes advance. F6.2's player measure then has a step's delay to
  measure in the battery's sector, whenever an axis runs through it.

### F7.3: the bands again

- `fronts_bench --history`, `--player` and `air_war_balance --fronts` are re-run after F7.1 and F7.2.
- Each argued band is held, re-argued or returned to green.

## As built: F7.1 (24 Nov 2026)

Our army's reserve is held back by its deployment, and the counterattack is weighed against the enemy at the
penetration's tip once its momentum is spent (`03_GROUND_WAR.md`, As built: F7.1).
- `fronts_bench --history` now counts a war in the counterattack band when the enemy is halted on our ground, as well as
  when it penetrates the main zone. The label is "wars penetrated or held".
- It prints two new lines:
  - the tip's ratios (any hour, and the hours with the momentum spent);
  - the reserve: its share and depth, the counterattacks that went in, the ratio they were ordered on, and the line
    won back.

**The bands** (14 days x 3 seeds; before is track F at 61f4e4e, after is F7.1):

| Band | Range | Pact before → after | NATO before → after | Neutral before → after | |
|---|---|---|---|---|---|
| Wars penetrated or held: we counterattack | 0.3-1.0 | 0 → 0 | 0 → 0 | none → 1.00 (3 of 3) | Pact, NATO re-argued; the neutral's new and green |
| Our army's day | 0.2-1.2 % | 1.26 → 1.22 | 2.47 → 2.47 | 0.07 → 0.33 (no band) | argued as before |
| The air's share of our battle losses | 5-25 % | 20.15 → 17.39 | 8.45 → 8.45 | - | green |
| A strike on troops, of its division | 0.2-3 % | 1.79 → 1.23 | unchanged | - | green |
| Our radar SAM fire units lost at the end | 10-40 % | 24.24 → 25.76 | unchanged | - | green |
| Of those wrecked, working at the end | 33-100 % | 63.83 → 62.50 | unchanged | - | green |
| An S-75 class move, heard to ready | 7.5-20 h | 15.53 → 15.91 | none | - | green |
| Our formations a day in the fight, withdrawn | 0.05-0.6 | 0 → 0 | 0 → 0 | - | argued as before |

Every other band is the same to the digit, and none of NATO's moves. In the Pact's war the reserve now
stands 55 km back rather than at the capital (61 km), so the enemy's strikes find different troops; the enemy's loss
a sortie (printed beside, `air_war_balance`'s band) goes from 3.48 to 3.65 %. The neutral's printed values with no
band move with the counterattack: our division's day in contact 5.49 → 4.92 %, the enemy's army's day 0.08 → 0.12 %,
the exchange 0.95 → 2.50 (our counterattack pays as an attacker), the enemy's loss a sortie 3.85 → 2.97 %.

**Argued** (each band's `source`):
- *No counterattack against the Pact or NATO.* Our free force, the reserve with every other free formation, comes to
  0.28 (Pact) and 0.51 (NATO) of the enemy at a penetration's tip at best. In the hours its momentum is spent it comes
  to 0.29 and 0.52 (245 and 300 h a war). The army commander's ratio is 1.8-2.2. A front's three divisions on one
  axis, or a NATO division's first echelon, outweigh our whole free force even at one breakthrough sector. Lowering
  the ratio to that would send a brigade against a division.
- *No withdrawals.* As F6.1 argued: no heavier fighting came, since no counterattack goes in against the Pact or NATO.

**Counterattacks by war.**
- *The neutral.* In every war: ordered on 2.57:1, on average 2.0 km won back in 24 h, the line still in our security
  zone.
- *The Pact, NATO and the rebels.* None.

**Identity, cost, gates.** At the defaults `air_war_balance data 20 7` and `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` are byte-identical to 61f4e4e built the same way, and the replay fingerprint holds (journal 107, FNV 1375944258174313379, 41 packages). The ground war costs 0.02-0.07 ms a campaign day alone (03, As built: F7.1). `--ground --gate`, `--ground --gate --player-sector battery`, `--feed --gate` and `--history --gate` exit 0, and `--player`'s bands pass as F6.2's (the battery's line 0-0.49 km apart, its air front 0 steps, elsewhere 0-0.02).

**Left:**
- The neutral's counterattacks win little: they go in when ordered, at night or in the hills too.
- Against the Pact and NATO the band stays argued. A front's reserve, a second army, or our divisions' own second
  echelons fighting as one counterattack would be needed to reach the ratio: the owner's call.
- Withdrawals are still for F7.3, with F7.2.

## As built: F7.2 (24 Nov 2026)

The air front follows the enemy's own ground offensive (`02_AIR_FRONT.md`, As built: F7.2). At `air_front.mode` 2 a
sector's reward for a step also counts the faction's need for close support of its own axes advancing there: its
`close_support.worth` (the Pact and NATO 0.5, a game value whose weighting is sourced from `fm_100_2_1_1984`,
`mt_1965_krasovskiy_conventional`, `fm_100_5_1982`) for each axis its own picture has attacking at 2.8 km a day or more.
The risk gates are unchanged. The defaults stay at mode 1; `fronts_bench --feed`, `--ad`, `--history`, `--player` and
`--map` and `air_war_balance`/`air_war_bench --fronts` run at mode 2. `--history` takes `--set` (`--set
air_front.mode=1` runs F7.1's), prints a new line, "the air front behind its own axes (F7.2)", and now measures the
air front's latency to the cycle that stepped (`depth_since_h`), not the hour the bench sampled it, which read an hour
late.

**The bands** (14 days x 3 seeds; before is track F at c0d584e, F7.1, at mode 1; after is F7.2 at mode 2):

| Band | Range | Pact before → after | NATO before → after | |
|---|---|---|---|---|
| The air front: gates holding to the step | 0-24 h | none → 24.0 h (3 steps) | none → none | green (before: no step to measure) |
| The air front: loss line to the step back | 12-72 h | none → 14.0 h (2) | none → none | green |
| The air front's steps forward a war | (printed) | 0 → 1.0 | 0 → 0 | |
| The air's share of our battle losses | 5-25 % | 17.39 → 15.99 | 8.45 → 8.45 | green |
| A strike on troops, of its division | 0.2-3 % | 1.23 → 1.18 | 1.38 → 1.38 | green |
| Our formation's day in contact | 1-8 % | 7.03 → 6.93 | 5.00 → 5.00 | green |
| Our army's day | 0.2-1.2 % | 1.22 → 1.20 | 2.47 → 2.47 | argued as before |
| Our radar SAM fire units lost at the end | 10-40 % | 25.76 → 27.27 | 18.18 → 18.18 | green |
| Of those wrecked, working at the end | 33-100 % | 62.50 → 64.71 | 62.50 → 62.50 | green |
| The enemy's loss a sortie (`air_war_balance`'s band) | 1.5-4.5 %, 1.0-3.5 % | 3.65 → 3.73 | 2.50 → 2.50 | green |

Every other band is the same to the digit, and so are the neutral's and the rebels' wars; NATO's differs only in the
mean reward its gates saw (2.15 → 2.65: the need counted, but the cover gate held). F7.1's argued bands stay argued
with their reasons: no counterattack against the Pact or NATO, no withdrawals (the Pact's step brought no heavier
fighting: 6.0 strikes on troops a war against 6.3, the pull-back following within a day).

**The air front against the axes** (the new line; 02, As built: F7.2):
- *The Pact*: 100 sector-hours a war with an axis advancing in its picture, 80 of them contested; the cover gate held
  48 of those and the mobile 48. One step a war, every one behind an axis, two of three pulled back on the loss line.
  At the end its axes have pushed into two sectors a war (40.4 km on average) and its air front there stands 3.3 km
  beyond the line on average (one sector in three wars still a step in).
- *NATO*: 136 such sector-hours, 88 contested, the cover gate holding 80: no step. Our radar SAMs in its axes' sectors
  are believed working, so its air front stays at the line, as the gates require.
- *The neutral*: no block and gates that never open on their own: no step behind its three axes.

**The player's effect** (`fronts_bench --player`, 7 days x 3 seeds): every band passes as F6.2's.
- The battery's sector: the line 0-0.49 km apart (NATO's western_crossings, as before), the air front 0 steps apart.
- Elsewhere: the line 0-0.02 km apart; the Pact's air front 0.08 steps apart against the draws' 0.42.
- The battery's sector has no axis in the Pact's war (northern_forest), so the step's delay F6.2 asked for is still
  not measured there: no axis runs through it on these seeds. The ledger's hours the battery alone held the air front:
  the Pact 3.7/2.3 h → 0.0/2.3 h careless/careful.

**`air_war_balance --fronts`** (the nation, 10 seeds x 7 days): every band holds. Only the Pact's war moves: its air front steps forward 0.6 times a campaign (the deepest 60 km
careless, 40 km careful; before, 0). Its loss a sortie goes 3.10/3.03 → 3.12/3.02 %, the careless battery is wrecked in
40 → 30 % of runs and the exchange goes 1.61/2.03 → 1.52/2.00. The rebels', the neutral's and NATO's runs are the same to
the digit (`docs/air_war/09_BALANCE.md` section 28).

**Identity, cost, gates.** At the defaults `air_war_balance data 20 7` and `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7` are byte-identical to c0d584e (compared by content), and the replay fingerprint
holds (journal 107, FNV 1375944258174313379, 41 packages). The rule costs one pass over the axes at each decision. In paired runs side by side on the same machine
(`fronts_bench --history --seeds 2`, mode 1 against mode 2, the war with our pipeline pumped hourly) a campaign day takes
4471 → 4454 ms for the Pact and 3236 → 3214 ms for NATO: below the noise. In the gates the pipeline's pump at mode 2
takes 17.1 ms a campaign day for the Pact, 13.3 for NATO, 2.4 for the neutral and 3.1 for the rebels, against 20
(c0d584e's run at mode 1, on a busier machine: 25.1, 21.1, 6.0, 4.7). The ground war in the campaign takes 2.03 ms a day
against 2.13 (it does not read the mode). `--ground --gate`, `--ground --gate
--player-sector battery`, `--feed --gate` and `--history --gate` exit 0, and `--player`'s bands pass.

**Left:**
- The battery's sector sees no advancing axis in the Pact's war on these seeds, so F6.2's player measure with a step's
  delay waits for a war whose axis runs through it (F7.3, or a seed or player sector that has one).
- `mission_session.hpp`'s fronts flag still sets mode 1.
- Withdrawals: still none. The Pact's step is short-lived, so the fighting is no heavier (F7.3).
