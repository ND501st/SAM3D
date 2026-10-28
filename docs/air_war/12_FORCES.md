# 12. Forces and their build-up

Written 23 October 2026. How much of each enemy's air force is in the war, and when. Code: `AirCampaign::Arrived` and
the daily reinforcement in `AirCampaign::Daily` (`campaign.hpp`), the front's `force_scale` in `LoadAirWarData`
(`data.hpp`); data: each faction's `logistics.buildup_initial` and `buildup_full_by_day` (a squadron may set its own
`initial` and `full_by_day`), each front's `force_scale` in the theatre. Research: `docs/research/AIR_WAR_MISSIONS_2026-10.md` §5.

## The build-up

A squadron has `initial` of its aircraft on day 1 and the rest arrive evenly until `full_by_day`; the journal logs each
day's arrivals ("buildup") and the campaign counts them (`Reinforcements`). Losses are replaced on top as before
(`replacements_per_day`).

| Faction | Day 1 | Full by | Why |
|---|---|---|---|
| NATO | 45 % | day 28 | Reinforcement from the United States over weeks: Desert Shield's first squadrons in place in four weeks (`gwaps_vol3` pp. 298-299), Kosovo's 430 aircraft becoming nearly 1,000 (`rand_mr1365` p. 33) |
| Warsaw Pact | 70 % | day 7 | The front air forces in place, the rest of the front's divisions and the long-range aviation within days (`fm100_2_3` p. 4-127) |
| Neutral | 100 % | | Its air force is at home |
| Rebels | 80 % | day 30 | An improvised wing that grows as aircraft are found, captured or defect (`croatia_1991_migflug`) |

The support squadrons set their own: NATO's E-3As and RC-135s are there from the start, its tankers at 60 %.

## A whole nation's war

A front's `force_scale` (0.25-4) scales its faction's squadrons and its packages a cycle on that theatre: the tuned
theatre's are the faction's (1.0); on the nation's theatre the Pact fights with two divisions' worth (2.0; `fm100_2_3`:
a front's air forces are two or three divisions) and NATO with 1.5. Cruise missile stocks are not the front's and stay.

## What it gives

On the nation's theatre NATO starts with some 120 aircraft and ends the first week with 170; the Pact with 190 and
260 (`09_BALANCE.md` §10). NATO's first week is lighter than before the build-up (700 sorties against 1,100): the
rollback takes longer, as it would while the reinforcements are on their way.
