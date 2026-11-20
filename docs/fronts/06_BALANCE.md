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
