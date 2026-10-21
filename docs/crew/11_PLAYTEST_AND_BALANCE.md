# 11. Playtest and balance

Roadmap 8.12: the crew is played at every skill tier over many seeds, and the results are read against the published
norms and the skill table's intent (`05`). First pass, 17 October 2026, headless (simcore). A person hasn't yet played
the missions on the map; that is the second half of this step.

## 1. How

`simcore/examples/crew_balance <data dir> [seeds] [scenario ...]` plays each crew scenario with every crew member at one
tier, over a number of seeds, and prints a table per scenario:

| Column | What it is |
|---|---|
| К бою s (ideal) | When the crew was ready at a halt, and the drill master's ideal from the same start |
| First echo to launch | From the first echo on the search scope to the first launch (the target appears far off in these scenarios: no norm applies) |
| Launch after the chart's ideal | From the network chart's ideal moment to the launch. The ideal is detection, or the crew free from the last target, plus 20.7 s, and no earlier than the zone's entry (`06` §4) |
| Slower than the norm | Launches more than 40 s − 20.7 s after the ideal: beyond the published 26–40 s (`nvo_2015`, `korovin_tiv_2010`) |
| Launches, hits | Our vehicle's |
| Faults | Equipment faults (a crew's mistakes that break something) |
| Out of order, slips, forgotten | The tier's slips (a step done before its turn) and omissions (a step forgotten until patience runs out) |
| Reminders | Reminders the crew gave (only players' seats get reminded: NPC seats never stall) |

The deployed reaction time per tier is measured separately by the calibration test (`test_crew.cpp`, 20 seeds):
master 28.5 s, 1st class 31.4 s, 2nd 36.9 s, 3rd 41.1 s, recruit 85.8 s. The published figure is 26–40 s.

## 2. Results (10 seeds per tier)

**osa_crew_practice.json** (one drone; NPC commander; from cold):

| Tier | К бою s (ideal) | Launch after the ideal s | Slower than the norm | Launches | Hits | Faults | Slips | Forgotten |
|---|---|---|---|---|---|---|---|---|
| master | 252 (252) | 11 | 0.0 | 1.3 | 1.0 | 0 | 0.0 | 0.0 |
| class1 | 253 (252) | 12 | 0.0 | 1.3 | 1.0 | 0 | 0.2 | 0.0 |
| class2 | 253 (252) | 13 | 0.0 | 1.3 | 1.0 | 0 | 0.4 | 0.1 |
| class3 | 254 (252) | 14 | 0.3 | 1.3 | 1.0 | 0 | 1.1 | 0.2 |
| recruit | 258 (252) | 17 | 1.0 | 1.4 | 1.0 | 0 | 2.1 | 1.0 |

**osa_battery_fight.json** (four targets on the battery; the NPC battery commander distributes them):

| Tier | К бою s (ideal) | Launch after the ideal s | Slower than the norm | Launches | Hits | Faults | Slips | Forgotten |
|---|---|---|---|---|---|---|---|---|
| master | 252 (252) | 9 | 0.0 | 2.3 | 1.2 | 0 | 0.0 | 0.0 |
| class1 | 253 (252) | 9 | 0.0 | 2.4 | 1.3 | 0 | 0.2 | 0.0 |
| class2 | 253 (252) | 12 | 0.0 | 1.7 | 1.0 | 0 | 0.6 | 0.1 |
| class3 | 254 (252) | 15 | 0.5 | 1.6 | 0.9 | 0 | 1.6 | 0.3 |
| recruit | 258 (252) | 14 | 1.0 | 1.8 | 0.6 | 0 | 2.5 | 1.5 |

**osa_mission_march.json** (march, raid, halt, one missile, reload, march on):

| Tier | Launch after the ideal s | Slower than the norm | Launches | Hits | Faults | Slips | Forgotten |
|---|---|---|---|---|---|---|---|
| master | 14 | 0.0 | 1.0 | 0.7 | 0 | 0.0 | 0.0 |
| class1 | 15 | 0.0 | 1.0 | 0.7 | 0 | 0.5 | 0.0 |
| class2 | 17 | 0.0 | 1.0 | 0.7 | 0 | 0.6 | 0.2 |
| class3 | 18 | 0.0 | 1.0 | 0.7 | 0 | 1.0 | 0.5 |
| recruit | 23 | 0.8 | 1.0 | 0.7 | 0 | 2.2 | 2.2 |

## 3. Findings

1. **Trained crews meet the published reaction time, recruits don't.** Master to 2nd class are never slower than the
   norm; 3rd class sometimes are (0.3–0.5 launches a run); recruits about once a run. This is the skill table's intent
   (`05` §3).
2. **Skill shows in the outcome where there is more than one target.** In the battery fight, hits per run fall from
   1.2–1.3 (master, 1st class) to 0.6 (recruit). The recruit crew engages fewer targets in the time, and its mistakes
   lower the chance per missile (the outcome model's crew factor). With one slow target (the practice drone), every
   crew gets it in the end.
3. **No tier breaks equipment.** Slips are always between two steps that wait for nothing (`CrewMember`), so they are
   recorded as out-of-order actions but never fault a subsystem. Recruits slip 2–2.5 times a run and forget a step
   1–2 times.
4. **К бою from cold barely depends on the crew.** It takes 252–258 s from master to recruit, against an ideal of 252
   s, because the equipment's warm-ups dominate: the power unit, the gyro's 40 s, the transmitters. The debrief's
   percent bands (110/130/160 %, `08` §3) therefore grade every crew "excellent" from cold.
   - **Not changed:** the grade should come from published нормативы, which are still an open research item (`08` §3).
   - **Recommendation:** grade a cold start by the seconds over the ideal (what the crew adds), not by the percentage.
     Readiness from a warmer state, where the crew's own actions dominate, separates the tiers.
5. **The first pass misjudged launches; corrected.**
   - The first judgement counted from the network chart's bare minimum (20.7 s from the detection) and called every
     launch late, the master's included.
   - Second targets were counted from their own first echo while the crew was still busy with the first target.
   - Now a launch is judged from when the crew was free and against the published norm. The numbers above are after
     the correction.
6. **The NPC commander had two faults, found in the mission run and fixed** (roadmap 8.11):
   - he permitted a launch with the launcher empty, so the deputy waited on it;
   - he stowed for the march while the TZM was coming to reload him.

   He now gives no order whose needs are missing (`OrderDef::needs`), and he waits for the reload.

## 4. To do

- A person plays the missions on the map: the H, O and T keys, the bodies, the voices. Note what feels wrong.
- Grade a cold-start К бою by the crew's own seconds once нормативы are found.
- More scenarios in the balance set: jamming, low level, EMCON TV, night. The tool takes any scenario file.
