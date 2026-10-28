# 13. Package sizes and compositions

Written 23 October 2026. How big a strike is and what flies with it. Code: `Planner::BestPackage` and `Build`
(`planner.hpp`); data: each faction's package templates (`doctrine.packages`), `utility.sortie_cost` in
`data/gameplay/air_war.json`. The library of the period's missions is in `docs/research/AIR_WAR_MISSIONS_2026-10.md` §2.

## Sizes

A template gives a package's main flights and their size (NATO's strike: two flights of four; the Pact's massed blows
grow with its `simultaneous_bias`). With `utility.sortie_cost` above zero the planner weighs, for every strike, DEAD and
close-support option, half the template's group, the template's and a doubled one: each aircraft costs `sortie_cost` in
the utility, so a pair goes to a radar and a massed blow to an airfield. A sweep (`09_BALANCE.md` §10):

| sortie_cost | The Pact's attackers a package | NATO's |
|---|---|---|
| 0 (one size) | 6.8 | 5.0 |
| 0.05 | 7.9 | 5.6 |
| 0.1 (chosen) | 7.0-7.3 | 5.3-5.8 |
| 0.15 | 6.1 | 4.9 |
| 0.3 | 4.3 | 4.1 |

At 0.1 the Pact sends 10-14 aircraft at airfields, command posts, bridges and rail junctions and 4-6 at radars, SAM sites
and troops, its massed blows where they count (`cia_1985_air_op`); NATO stays near its four-ship flights everywhere.

## Compositions

Around the main flights a package takes, from its template and the planner's picture: SEAD for the radar threats it
believes are on the way (`sead_per_threat`, up to `sead_max`; NATO's DEAD packages nearly always, its strikes when the
route meets a known SAM), escorts against the defender's fighters (`escort_ratio` times the front's fighter threat, less
an AWACS's cut), stand-off or escort jammers, decoys ahead of a DEAD strike, a chaff corridor, reconnaissance ahead and
after. The weaponeer picks each flight's weapons and attack (`10_WEAPONS.md`); tankers let it reach (`11_SUPPORT.md`).

## The order a staff fills its plan

Cruise missiles, DEAD, hunters, deception, a light air arm's nuisance raids, strikes, reconnaissance, close support,
armed reconnaissance (`14_LOGISTICS.md`), helicopter raids, damage assessment: each kind up to its share of the cycle's
sorties and slots. The rebels' night harassment is filled before their strikes, since the same few aircraft fly both.

## A light air arm's thresholds

The doctrine may set its own `min_effect` and `min_value` (the least a package must achieve and be worth): the rebels'
0.005 and -0.5, the neutral's 0.02 and 0.3. With weapons that do what they did, a pair of light aircraft with home-made
bombs achieves a few per cent against a battalion; such air arms flew anyway, for their presence (`croatia_1991_migflug`).
A nuisance raid is judged by the sleep it costs, not its damage.
