# 9. Balance pass

Roadmap A12: the enemy air command is run for every adversary over many seeds and the results are read against the
period's shapes from the research (`docs/research/ENEMY_AIR_RESEARCH_2026-10.md`): NATO's rollback of the air defences
taking days, the Pact's opening blows, the small air arms' caution, and how often a raid comes through the player's
battery's sector. First pass, 18 October 2026, headless (simcore). A person playing a campaign through on the campaign
screen is the second half of this step.

## 1. How

`simcore/examples/air_war_balance [--theatre file] [data dir] [seeds] [days] [faction ...]` runs each campaign hour by
hour (on `theatre_pannonia.json`, the theatre tuned here, unless `--theatre` names another: `theatre_pannonia_nation.json`
is the whole nation, §8) with the raids
through the battery's sector resolved abstractly like the rest (`SkipPlayerMission`: the battery fights with its own
lethality), so the numbers are the campaign layer's alone. Each adversary is run with two battery postures:

| Posture | The battery commander |
|---|---|
| careless | radars radiate half of every hour, EMCON discipline 0.2, no dummy radio traffic, never moves |
| careful | radars radiate a tenth of the hour, EMCON discipline 0.7, dummy traffic, moves every 20 hours (`air_war_demo`'s) |

| Column | What it is |
|---|---|
| Phase | The doctrine phase at the day's end, averaged over the seeds (1 the first) |
| Packages, Sorties | Packages over their target that day; manned aircraft in them (decoy drones are counted apart) |
| SEAD share | Packages with a SEAD purpose: DEAD and hunter packages, and others with a SEAD flight |
| Lost, Loss % | Manned aircraft lost, and per sortie |
| Air defence working (believed) | The true working share of the player's side's radar SAM sites at the day's end (dummies left out), the set the enemy measures; in brackets what the enemy believes (`IntelPicture::BelievedIadsShare`). Until 22 October 2026 the true share also counted the early-warning radars and the optical Strela platoons; the enemy's belief never did |
| Wrecked | Objects of the player's side wrecked and not yet back |
| Battery raids | Raids offered to the player that day (the pacing's choice: through the sector, the gap, the daily cap, a working battery) |
| Battery | The battery's working share at the day's end |
| Enemy knows battery | `EnemyKnowledgeOfBattery`: how well the enemy knows where the battery is, 0..1 |

The lines under each table give the mission mix over the campaign, the staff's turnover (killed, captured, relieved,
promoted away), the false claims found out, the political events (and the pauses among them), in how many runs the
battery was wrecked at least once, the ARMs left at the end and the decoy drones flown; then where the packages went,
by the kind of target as the enemy believed it and by the objects struck most.

## 2. The shapes it was read against

| Adversary | Shape (the research) | Read in the tables as |
|---|---|---|
| Rebels | A dozen light aircraft; avoid known guns and missiles; every airframe irreplaceable | a few sorties a day, no SEAD, losses of a few per cent and few in number, raids that keep away from the battery |
| Neutral neighbour | Light jets and helicopters raiding along the front, rarely deep; very loss-averse | strikes, helicopter raids and reconnaissance near the line; losses after the first contact fall as it learns |
| Warsaw Pact | Massed blows in the first two days, then support of the front's offensive; basic SEAD | a surge on days 1-2 that breaks much of the air defence, a lower sustained rate after; SEAD falls off after the air operation |
| NATO | A long, methodical rollback (hunter-killer teams, DEAD on located sites, decoys), scarce ARMs, low tolerance for losses | SEAD-heavy first days, the air defences falling over several days, losses falling as they do; the rollback stalls when ARMs and fixes run short and resumes when sites return |

## 3. Results (20 seeds, 7 days)

The theatre the balance pass tuned (`theatre_pannonia.json`), its war in 1985: since 23 October 2026 with the Pact's Kh-22 cruise missiles (NATO's come in a later war).

**The Rava Liberation Front's air wing, careless battery** (20 seeds):

| Day | Phase | Packages | Sorties | SEAD share | Lost | Loss % | Air defence working (believed) | Wrecked | Battery raids | Battery | Enemy knows battery |
|---|---|---|---|---|---|---|---|---|---|---|---|
| D1 | 1.0 | 2.4 | 3.8 | 0% | 0.3 | 8.0% | 100% (100%) | 0.0 | 0.5 | 100% | 0.26 |
| D2 | 1.0 | 2.4 | 3.8 | 0% | 0.1 | 2.7% | 100% (100%) | 0.0 | 0.0 | 100% | 0.15 |
| D3 | 1.0 | 2.1 | 3.4 | 0% | 0.1 | 3.0% | 100% (100%) | 0.0 | 0.2 | 100% | 0.17 |
| D4 | 1.0 | 2.0 | 3.0 | 0% | 0.0 | 0.0% | 100% (100%) | 0.0 | 0.1 | 100% | 0.15 |
| D5 | 1.0 | 2.5 | 4.0 | 0% | 0.1 | 3.8% | 100% (100%) | 0.0 | 0.0 | 100% | 0.17 |
| D6 | 1.0 | 2.5 | 4.2 | 0% | 0.0 | 0.0% | 100% (100%) | 0.0 | 0.1 | 100% | 0.16 |
| D7 | 1.0 | 1.6 | 2.6 | 0% | 0.0 | 0.0% | 100% (100%) | 0.0 | 0.1 | 100% | 0.19 |
| all | | 15.4 | 24.6 | | 0.7 | 2.6% | | | 0.9 | | |

Mix: strike 43% recce 41% harassment 16%. Staff changes 0.6, false claims found out 0.0, political events 1.9 (pauses 0.5) a campaign; battery wrecked in 0% of runs; ARMs left 0; decoy drones 0.
Targets by kind (packages a campaign): troops 5.8, depot 4.8, bridge 1.8, power_plant 1.5, command_post 1.1, ? 0.1, sam_site 0.1, airfield 0.1.
Most struck: sw_depot 2.9, sw_border_battalion 2.4, lenta_bridge 1.8, group_southwest 1.6, paksany_power 1.5, sw_artillery 1.4, kesmark_depot 1.2, sw_brigade_cp 1.1, southern_depot 0.8, group_south 0.5, (nothing there) 0.2, kecel_s125 0.1.

**The Rava Liberation Front's air wing, careful battery** (20 seeds):

| Day | Phase | Packages | Sorties | SEAD share | Lost | Loss % | Air defence working (believed) | Wrecked | Battery raids | Battery | Enemy knows battery |
|---|---|---|---|---|---|---|---|---|---|---|---|
| D1 | 1.0 | 2.4 | 3.8 | 0% | 0.2 | 5.3% | 100% (100%) | 0.0 | 0.5 | 100% | 0.07 |
| D2 | 1.0 | 2.4 | 3.8 | 0% | 0.1 | 1.3% | 100% (100%) | 0.0 | 0.0 | 100% | 0.08 |
| D3 | 1.0 | 2.2 | 3.5 | 0% | 0.1 | 1.4% | 100% (100%) | 0.0 | 0.1 | 100% | 0.07 |
| D4 | 1.0 | 2.3 | 3.6 | 0% | 0.1 | 2.7% | 100% (100%) | 0.0 | 0.0 | 100% | 0.07 |
| D5 | 1.0 | 2.5 | 4.0 | 0% | 0.1 | 2.5% | 100% (100%) | 0.0 | 0.0 | 100% | 0.08 |
| D6 | 1.0 | 2.4 | 3.8 | 0% | 0.1 | 2.7% | 100% (100%) | 0.0 | 0.1 | 100% | 0.05 |
| D7 | 1.0 | 1.7 | 2.8 | 0% | 0.2 | 7.3% | 100% (100%) | 0.0 | 0.0 | 100% | 0.05 |
| all | | 15.8 | 25.1 | | 0.8 | 3.2% | | | 0.6 | | |

Mix: strike 43% recce 40% harassment 17%. Staff changes 0.6, false claims found out 0.0, political events 1.9 (pauses 0.7) a campaign; battery wrecked in 0% of runs; ARMs left 0; decoy drones 0.
Targets by kind (packages a campaign): troops 5.9, depot 5.1, bridge 2.4, power_plant 1.1, command_post 0.7, ? 0.3, airfield 0.2.
Most struck: sw_depot 3.2, sw_border_battalion 2.8, lenta_bridge 2.4, group_southwest 1.5, kesmark_depot 1.5, sw_artillery 1.2, paksany_power 1.1, sw_brigade_cp 0.7, (nothing there) 0.4, southern_depot 0.4, group_south 0.3, kecel_airbase 0.2.

**The Noric Republic's air force, careless battery** (20 seeds):

| Day | Phase | Packages | Sorties | SEAD share | Lost | Loss % | Air defence working (believed) | Wrecked | Battery raids | Battery | Enemy knows battery |
|---|---|---|---|---|---|---|---|---|---|---|---|
| D1 | 1.0 | 3.8 | 8.6 | 0% | 1.1 | 13.5% | 100% (100%) | 0.0 | 1.0 | 100% | 0.51 |
| D2 | 1.0 | 2.4 | 5.4 | 0% | 0.0 | 0.0% | 100% (100%) | 0.0 | 0.1 | 100% | 0.48 |
| D3 | 2.0 | 2.1 | 4.8 | 0% | 0.1 | 2.1% | 100% (100%) | 0.2 | 0.3 | 100% | 0.52 |
| D4 | 2.0 | 1.6 | 3.9 | 0% | 0.0 | 0.0% | 100% (100%) | 0.2 | 0.2 | 100% | 0.54 |
| D5 | 2.0 | 1.7 | 5.3 | 0% | 0.1 | 1.9% | 100% (100%) | 0.2 | 0.3 | 100% | 0.53 |
| D6 | 2.0 | 1.7 | 4.7 | 0% | 0.0 | 0.0% | 100% (100%) | 0.3 | 0.1 | 100% | 0.46 |
| D7 | 2.0 | 2.1 | 6.5 | 0% | 0.1 | 0.8% | 100% (100%) | 0.1 | 0.6 | 100% | 0.52 |
| all | | 15.4 | 39.0 | | 1.4 | 3.6% | | | 2.7 | | |

Mix: strike 32% recce 39% bda 5% helo_raid 23%. Staff changes 0.9, false claims found out 0.0, political events 1.6 (pauses 0.8) a campaign; battery wrecked in 0% of runs; ARMs left 0; decoy drones 0.
Targets by kind (packages a campaign): bridge 7.0, troops 6.0, command_post 1.2, depot 1.1, ? 0.1.
Most struck: lenta_bridge 7.0, sw_border_battalion 2.7, sw_artillery 2.2, sw_brigade_cp 1.2, group_southwest 1.1, sw_depot 1.1, (nothing there) 0.1.

**The Noric Republic's air force, careful battery** (20 seeds):

| Day | Phase | Packages | Sorties | SEAD share | Lost | Loss % | Air defence working (believed) | Wrecked | Battery raids | Battery | Enemy knows battery |
|---|---|---|---|---|---|---|---|---|---|---|---|
| D1 | 1.0 | 3.8 | 8.6 | 0% | 1.1 | 12.3% | 100% (100%) | 0.1 | 1.0 | 100% | 0.13 |
| D2 | 1.0 | 2.8 | 6.3 | 0% | 0.1 | 0.8% | 100% (100%) | 0.1 | 0.3 | 100% | 0.29 |
| D3 | 2.0 | 2.6 | 6.0 | 0% | 0.1 | 0.8% | 100% (100%) | 0.2 | 0.3 | 100% | 0.38 |
| D4 | 2.0 | 1.4 | 4.5 | 0% | 0.0 | 0.0% | 100% (100%) | 0.2 | 0.1 | 100% | 0.34 |
| D5 | 2.0 | 2.2 | 6.3 | 0% | 0.1 | 0.8% | 100% (100%) | 0.3 | 0.1 | 100% | 0.32 |
| D6 | 2.0 | 1.8 | 5.0 | 0% | 0.0 | 0.0% | 100% (100%) | 0.4 | 0.1 | 100% | 0.17 |
| D7 | 2.0 | 2.5 | 8.0 | 0% | 0.0 | 0.0% | 100% (100%) | 0.5 | 0.1 | 100% | 0.24 |
| all | | 17.1 | 44.6 | | 1.2 | 2.7% | | | 2.0 | | |

Mix: strike 35% recce 40% bda 5% helo_raid 20%. Staff changes 0.8, false claims found out 0.1, political events 1.4 (pauses 0.9) a campaign; battery wrecked in 0% of runs; ARMs left 0; decoy drones 0.
Targets by kind (packages a campaign): bridge 7.3, troops 6.7, depot 2.1, command_post 0.9, sam_site 0.1, ? 0.1.
Most struck: lenta_bridge 7.3, sw_border_battalion 3.1, sw_artillery 2.4, sw_depot 2.1, group_southwest 1.1, sw_brigade_cp 0.9, (nothing there) 0.1.

**The Warsaw Pact: air forces of the front, careless battery** (20 seeds):

| Day | Phase | Packages | Sorties | SEAD share | Lost | Loss % | Air defence working (believed) | Wrecked | Battery raids | Battery | Enemy knows battery |
|---|---|---|---|---|---|---|---|---|---|---|---|
| D1 | 1.0 | 22.1 | 153.2 | 33% | 2.9 | 1.9% | 49% (58%) | 10.1 | 1.4 | 70% | 0.60 |
| D2 | 2.0 | 20.1 | 142.6 | 35% | 2.1 | 1.5% | 24% (39%) | 14.8 | 0.9 | 12% | 0.22 |
| D3 | 2.0 | 8.2 | 68.5 | 30% | 0.8 | 1.2% | 24% (38%) | 11.3 | 0.1 | 16% | 0.22 |
| D4 | 2.0 | 9.2 | 65.9 | 32% | 0.9 | 1.4% | 26% (35%) | 8.6 | 0.2 | 27% | 0.27 |
| D5 | 2.0 | 8.9 | 63.1 | 36% | 1.1 | 1.7% | 48% (37%) | 7.2 | 0.5 | 70% | 0.38 |
| D6 | 2.0 | 8.9 | 64.5 | 39% | 2.7 | 4.2% | 77% (38%) | 5.8 | 0.9 | 88% | 0.48 |
| D7 | 2.0 | 8.7 | 67.8 | 38% | 2.4 | 3.5% | 86% (51%) | 5.7 | 0.6 | 93% | 0.46 |
| all | | 86.2 | 625.5 | | 12.8 | 2.1% | | | 4.7 | | |

Mix: strike 37% dead 27% hunter 8% recce 8% cas 2% helo_raid 1% deception 13% missile 5%. Staff changes 5.7, false claims found out 1.1, political events 1.1 (pauses 0.0) a campaign; battery wrecked in 80% of runs; ARMs left 3; decoy drones 0.
Cruise missiles: 8.0 fired a campaign, 0.2 shot down.
Targets by kind (packages a campaign): sam_site 26.1, ew_radar 15.6, command_post 9.7, airfield 9.3, troops 7.8, depot 7.0, bridge 4.4, rail_junction 2.5, ? 1.9, power_plant 1.0, headquarters 0.9.
Most struck: osa_22 9.5, sw_brigade_cp 5.7, kesmark_p40 5.6, ludas_p19 5.5, (nothing there) 5.3, kecel_airbase 5.2, bugac_rtb 4.4, tiszavar_airbase 4.2, div_ad_cp 4.0, tiszavar_kub 3.8, group_north 3.6, kecel_s125 2.6.

**The Warsaw Pact: air forces of the front, careful battery** (20 seeds):

| Day | Phase | Packages | Sorties | SEAD share | Lost | Loss % | Air defence working (believed) | Wrecked | Battery raids | Battery | Enemy knows battery |
|---|---|---|---|---|---|---|---|---|---|---|---|
| D1 | 1.0 | 22.1 | 152.8 | 33% | 3.2 | 2.1% | 51% (70%) | 9.9 | 1.2 | 94% | 0.37 |
| D2 | 2.0 | 19.1 | 139.9 | 32% | 2.6 | 1.9% | 37% (66%) | 14.1 | 1.1 | 79% | 0.33 |
| D3 | 2.0 | 7.5 | 63.5 | 29% | 0.5 | 0.8% | 35% (59%) | 10.5 | 0.3 | 72% | 0.29 |
| D4 | 2.0 | 8.4 | 64.3 | 32% | 1.2 | 1.9% | 32% (59%) | 7.3 | 0.4 | 67% | 0.29 |
| D5 | 2.0 | 8.1 | 60.3 | 37% | 0.6 | 0.9% | 47% (56%) | 6.7 | 0.5 | 73% | 0.33 |
| D6 | 2.0 | 8.4 | 62.6 | 35% | 1.9 | 3.0% | 73% (57%) | 5.8 | 0.1 | 83% | 0.29 |
| D7 | 2.0 | 8.5 | 66.3 | 38% | 3.5 | 5.3% | 82% (62%) | 5.0 | 0.3 | 93% | 0.33 |
| all | | 82.0 | 609.9 | | 13.4 | 2.2% | | | 4.0 | | |

Mix: strike 38% dead 28% hunter 5% recce 10% cas 2% helo_raid 1% deception 11% missile 5%. Staff changes 5.5, false claims found out 1.9, political events 1.1 (pauses 0.0) a campaign; battery wrecked in 40% of runs; ARMs left 2; decoy drones 0.
Cruise missiles: 8.0 fired a campaign, 0.2 shot down.
Targets by kind (packages a campaign): sam_site 22.9, ew_radar 15.5, command_post 9.4, airfield 8.8, troops 8.2, depot 7.0, bridge 4.4, rail_junction 2.9, ? 0.9, power_plant 0.9, headquarters 0.9.
Most struck: osa_22 8.6, sw_brigade_cp 6.2, (nothing there) 6.0, kesmark_p40 6.0, ludas_p19 5.1, tiszavar_airbase 5.0, bugac_rtb 4.5, tiszavar_kub 4.4, kecel_airbase 3.8, group_north 3.7, div_ad_cp 3.3, cegled_junction 2.9.

**NATO: an allied tactical air force (composite), careless battery** (20 seeds):

| Day | Phase | Packages | Sorties | SEAD share | Lost | Loss % | Air defence working (believed) | Wrecked | Battery raids | Battery | Enemy knows battery |
|---|---|---|---|---|---|---|---|---|---|---|---|
| D1 | 1.0 | 16.1 | 108.0 | 60% | 1.4 | 1.3% | 62% (76%) | 6.8 | 0.9 | 96% | 0.79 |
| D2 | 1.0 | 15.3 | 117.5 | 44% | 1.1 | 0.9% | 46% (63%) | 9.2 | 0.4 | 93% | 0.72 |
| D3 | 1.1 | 14.5 | 100.8 | 38% | 0.6 | 0.6% | 28% (51%) | 9.0 | 1.1 | 35% | 0.40 |
| D4 | 1.7 | 15.2 | 91.8 | 29% | 0.7 | 0.8% | 21% (40%) | 8.2 | 0.7 | 21% | 0.36 |
| D5 | 1.8 | 14.8 | 103.2 | 26% | 1.6 | 1.6% | 36% (33%) | 6.6 | 0.3 | 22% | 0.36 |
| D6 | 1.9 | 15.0 | 103.6 | 31% | 1.6 | 1.5% | 57% (46%) | 6.2 | 0.7 | 47% | 0.45 |
| D7 | 1.8 | 14.3 | 102.1 | 29% | 1.8 | 1.8% | 67% (57%) | 4.8 | 0.9 | 67% | 0.53 |
| all | | 105.2 | 727.0 | | 8.7 | 1.2% | | | 5.0 | | |

Mix: strike 35% dead 20% hunter 9% recce 21% bda 6% cas 2% helo_raid 0% deception 7%. Staff changes 5.8, false claims found out 3.5, political events 2.0 (pauses 0.0) a campaign; battery wrecked in 80% of runs; ARMs left 46; decoy drones 135.
Targets by kind (packages a campaign): sam_site 34.2, ew_radar 14.6, command_post 11.8, troops 11.6, airfield 11.1, depot 10.2, rail_junction 4.5, ? 3.7, bridge 3.6.
Most struck: sw_brigade_cp 7.5, (nothing there) 7.5, osa_22 6.6, kecel_s125 5.6, tiszavar_airbase 5.6, kecel_airbase 5.5, kesmark_p40 5.0, tiszavar_kub 5.0, ludas_p19 4.8, bugac_rtb 4.7, cegled_junction 4.5, div_ad_cp 4.3.

**NATO: an allied tactical air force (composite), careful battery** (20 seeds):

| Day | Phase | Packages | Sorties | SEAD share | Lost | Loss % | Air defence working (believed) | Wrecked | Battery raids | Battery | Enemy knows battery |
|---|---|---|---|---|---|---|---|---|---|---|---|
| D1 | 1.0 | 16.1 | 108.0 | 60% | 1.6 | 1.4% | 61% (82%) | 6.8 | 0.9 | 93% | 0.42 |
| D2 | 1.0 | 15.8 | 118.5 | 42% | 0.8 | 0.6% | 41% (74%) | 9.8 | 0.6 | 92% | 0.60 |
| D3 | 1.0 | 15.3 | 94.8 | 36% | 0.8 | 0.8% | 36% (69%) | 8.4 | 1.4 | 94% | 0.56 |
| D4 | 1.0 | 14.5 | 81.3 | 26% | 0.6 | 0.7% | 33% (64%) | 7.1 | 1.7 | 93% | 0.42 |
| D5 | 1.0 | 15.1 | 90.2 | 22% | 1.0 | 1.1% | 48% (66%) | 6.2 | 1.6 | 94% | 0.47 |
| D6 | 1.0 | 15.2 | 99.2 | 30% | 1.9 | 2.0% | 65% (68%) | 4.8 | 1.7 | 85% | 0.46 |
| D7 | 1.0 | 14.7 | 98.7 | 28% | 1.2 | 1.3% | 70% (71%) | 4.2 | 1.2 | 84% | 0.58 |
| all | | 106.7 | 690.6 | | 7.8 | 1.1% | | | 9.2 | | |

Mix: strike 32% dead 19% hunter 6% recce 23% bda 5% cas 2% deception 14%. Staff changes 5.4, false claims found out 3.5, political events 2.0 (pauses 0.0) a campaign; battery wrecked in 15% of runs; ARMs left 47; decoy drones 161.
Targets by kind (packages a campaign): sam_site 40.2, ew_radar 14.2, troops 12.5, command_post 11.4, airfield 10.1, depot 8.3, rail_junction 4.4, bridge 3.5, ? 2.0.
Most struck: (nothing there) 15.9, sw_brigade_cp 6.8, kecel_s125 6.0, osa_22 5.9, tiszavar_kub 5.4, tiszavar_airbase 5.1, kecel_airbase 5.0, ludas_p19 4.8, bugac_rtb 4.7, kesmark_p40 4.7, sw_depot 4.5, cegled_junction 4.4.

## 4. Findings and the changes they made

The first run (before any change) and the last are both reproducible: `air_war_balance data 20 7`. Ten changes, in the
order the runs showed the need:

1. **The rollback never ended: the enemy's measure of the air defences grew with every discovery.** It was the believed
   working sites over the sites known at the start, so every site, dummy or ELINT ghost found later added to the top of
   the fraction; NATO believed 99-100 % of the defences worked all week while the truth was 63-77 %, and stayed in the
   rollback phase. **Changed:** the measure is the share of the radar air defences believed there that are believed to
   work, each weighed by the belief that it exists and is real (`04_PLANNING.md` §2).
2. **Nothing stayed wrecked.** Repair began the hour an object was wrecked and climbed linearly, so the "Wrecked" column
   read 0.0 at every day's end and a wrecked SAM site was back at half strength within 12-36 hours. **Changed:** a
   wrecked object is out until its replacement comes, `repair_h` after it was wrecked, and comes back at
   `repair.restored_function` (0.5); damage short of a wreck is repaired in the field as before (`06` §3).
3. **The neutral neighbour flew only reconnaissance** (98-100 % of its packages, five sorties a week). Inside its 40 km
   depth limit there were two objects, both under the battery's umbrella, and every armed option failed its loss limit.
   **Changed:** a front-line belt behind the south-western border (a border battalion, the group's artillery, a bridge
   on the border river, a forward depot, a brigade command post; game values), some inside the battery's reach and some
   outside. Now a third of its packages are strikes and a fifth helicopter raids, about 40 sorties a week.
4. **The Pact had no opening blows** (106, 98, 94 ... sorties a day: flat). **Changed:** a phase may set a `tempo`
   (`data/air_war/faction_*.json`); the Pact's air operation flies 1.5 for its two days and its support of the front
   0.85 after (game values on `cia_1985_air_op`'s three crushing blows in the first two days). Now about 150 sorties a day
   on days 1-2, 62-68 after.
5. **Our SAM sites were in their peacetime posture**: radiating 30-50 % of every hour, the S-75 and S-125 never moving,
   and a wrecked battery back in two days. The Pact wrecked the Kub on day 1 in 10 of 10 campaigns. **Changed:** the
   sites radiate 10-20 % of the hour (cued by the warning net), move between prepared positions every 12-72 hours (a
   battery planned against on old imagery had moved: `wiki_model5`), and take 96-120 hours to replace.
6. **One ARM wrecked a whole Osa battery.** An Osa battery has four combat vehicles, each with its own radar; a Kub,
   S-125 or S-75 battery has one fire-control radar. **Changed:** a defence has `radars` (the Osas 4); an ARM's hit takes
   out one. The careful battery was wrecked in 85 % of Pact campaigns before, 30 % after; against NATO 60 % before, 15 %
   after.
7. **Targets without an air defence of their own cost nothing to attack**, so the Pact broke the air defences for
   1.2-1.4 % of its sorties. **Changed:** light guns and shoulder-fired missiles around military targets
   (`attrition.target_guns_troops` 0.04, `attrition.target_guns_site` 0.015 per aircraft attacking low or by helicopter;
   none at medium altitude), priced the same by the planner and the resolver (`05_EXECUTION.md`).
8. **The first runs counted decoy drones as losses.** NATO's BQM-74s are meant to be shot down and the campaign never
   counts them; the runner did, and showed NATO losing 3.1-3.4 % a sortie, more than the Pact. **Changed (the runner):**
   manned aircraft only; drones are reported apart. NATO loses 1.1-1.2 % a sortie, the Pact 1.9 %.
9. **NATO's ARMs ran out on the first day** (60 and 3 a day): from day 2 its strike packages flew "no ARMs to spare for
   SEAD", fewer than half of them with SEAD. **Changed:** 100 and 6 a day (game values; HARM was scarce and Shrike older,
   `afmag_1989_slam`): still spent in two or three days of rollback, so the shortage shapes the campaign's middle and the
   day-8 HARM shipment matters.
10. **A rollback could not resume**, and raids were offered to a wrecked battery. **Changed:** a phase that ended on the
    believed air defences comes back when they are believed `adapt.reopen_margin` (0.2) above its line (NATO resumed the
    rollback 3 times in the test's 10 campaigns); the pacing offers no raid while the battery is being repaired (function 0.3 or
    less).

## 5. What the tables say now

1. **Posture decides the battery's fate.** A careless battery is wrecked at least once in 80 % of Pact and NATO
   campaigns; a careful one in 15-40 %. The careful battery is also known far less well (on day 1, 0.37-0.42 against 0.60-0.79) and,
   because it survives, is offered more raids (NATO 9.2 a week against 5.0). This is the lesson the campaign is meant to
   teach.
2. **The Pact's opening blows break the air defences, then its basic SEAD lets them come back.** The radar SAM sites
   fall to 24-37 % by the end of day 2 at 1.5-2.1 % losses; after the air operation only a sixth of its effort is DEAD or
   hunting, the replacements arrive, the sites are back to 82-86 % by day 7 and its losses climb to 3.5-5.3 %. Its
   Kh-22s (one pair a day at most, about eight missiles a week) go at the deepest fixed targets; only the S-75 can take
   them (0.2 a week). Not changed:
   this is "basic SEAD".
3. **NATO's rollback takes days, stalls and resumes.** Its losses fall from 1.3-1.4 % on day 1 to 0.6-0.9 % on days 2-4
   while the radar SAM sites fall to 21-33 % by day 4; then its ARMs run short, the replaced sites return, they recover to 67-70 %
   and the losses rise to 1.1-2.0 %; against the careless battery it goes back to the rollback. Against the careful
   battery the decoy emitters and ELINT ghosts keep NATO believing 64-82 % of the defences work: it never leaves the
   rollback in the week (the deception that kept Serbia's air defence alive in 1999, a design reference only).
4. **The neutral neighbour blunders once and learns.** 12-14 % of its first day's sorties are lost (it meets the
   battery), then about 2 % a day or less: the officers' lessons. Its raids through the battery's sector: 2-3 a week.
5. **The rebels keep away from the battery.** 2.6-3.2 % losses, a raid through the battery's sector less than once a week
   where the battery starts: they avoid known missiles, as the research says. **Not changed; a recommendation:** the
   rebels' raids are on the campaign screen's map and in the INTSUM; the player can move the battery to where they
   strike (the southern depot, the troops at the front). If playtesting finds the rebel campaign too quiet, start the
   battery nearer their targets or widen `pacing.sector_m` for the inferior faction only.
6. **Officers come and go.** 5-6 staff changes a week against the Pact and NATO (killed, captured, relieved), under one
   against the small air arms; false claims found out 1 (Pact) to 3.5 (NATO, whose recce and BDA look again) a week.

## 6. Not done yet

- A person playing campaigns through on the campaign screen (the second half of A12), and the scenarios' own balance
  (a raid played by the crew against the same raid resolved abstractly).
- The ARM as an EM missile and the crew's ARM drill are built (A11, roadmap E6, 20 Oct 2026), but only for raids the
  crew plays: `air_war_balance` resolves every raid abstractly (`resolve.hpp`, `attrition.arm_kill`), so the numbers
  above are unchanged. Played raids' ARM outcomes against the abstract model's are part of the scenarios' own balance
  (the first item). In the A11 test (`test_air_war.cpp`, seed 2, a radiating battery, two raids each) NATO fired 12 EM
  ARMs (2 on the vehicle) and the Pact 16 (1 on the vehicle).
- Historical loss rates as a check: the tables are read against shapes, not against sourced rates per sortie; the
  period's loss rates (1973, Linebacker II, the Bekaa) are a research item for `docs/research/`.

## 7. Tests added

`tests/test_air_war.cpp` (14 tests): an ARM takes one of an Osa battery's four radars but blinds a one-radar site, and
guns at the target hit low attackers but not medium ones; a wrecked object is out until its replacement comes, no raid is
offered while the battery is repaired, and the Pact's day 1 is more than one and a half times its day 4; the believed
air defences are the share of known sites working, and NATO's rollback ends and resumes.

On the nation's theatre (N4, `tests/test_air_war.cpp`, `tests/test_country_map.cpp`): radar-laid guns are not part of the
enemy's measure; the Pact's and NATO's DEAD, hunter and deception packages go at SAM sites and radars, never guns; battery
21 deploys to its position in their wars, on open ground; the neutral neighbour never strikes a pipeline station; a
one-channel S-125 takes at most one of a passing four-ship (`EngagedAircraft`); a played raid's rest is resolved but for
what the scenario flew (`ResolvedPieces`); in played raids the neighbours fire, their kills are theirs, and the warning
posts are one of each kind, the nearest working radar company among them. Cruise missiles: they fly in the war's year
from their stock at fixed targets (the Pact's Kh-22 in 1985, NATO's Tomahawk only in a later war, from its sea launch
area); a Kh-22 is too fast for the Osa but not the S-75, and one shot down is never a crew lost; a played Tomahawk salvo
is four `cruise_missile` targets that strike or are shot down.

## 8. The nation's theatre: the first read (N4, 22 October 2026)

The roadmap's nation on the map, N4: the air war on `theatre_pannonia_nation.json`, the whole nation (170 objects:
`docs/COUNTRY_MAP.md`), with `air_war_demo` and `air_war_balance` as a first read. `air_war_balance --theatre
theatre_pannonia_nation.json data 10 7`, against the tuned theatre (10 seeds each; tuned → nation):

| Adversary, battery | Sorties a week | Loss % | Defences working D3, D7 (believed) | Phase D7 | Battery raids a week | Battery wrecked |
|---|---|---|---|---|---|---|
| Rava Liberation Front's air wing, careless | 24.3 → 22.1 | 2.1% → 8.6% | 100% (100%), 100% (100%) → 100% (100%), 100% (100%) | 1.0 → 1.0 | 0.7 → 0.8 | 0 % → 0 % |
| Rava Liberation Front's air wing, careful | 24.0 → 23.6 | 2.9% → 8.1% | 100% (100%), 100% (100%) → 100% (100%), 100% (100%) | 1.0 → 1.0 | 0.5 → 0.5 | 0 % → 0 % |
| Noric Republic's air force, careless | 39.5 → 31.5 | 3.3% → 3.5% | 100% (100%), 100% (100%) → 100% (100%), 100% (100%) | 2.0 → 2.0 | 2.7 → 1.5 | 0 % → 0 % |
| Noric Republic's air force, careful | 45.4 → 39.7 | 2.4% → 3.8% | 100% (100%), 100% (100%) → 100% (100%), 100% (100%) | 2.0 → 2.0 | 1.9 → 1.5 | 0 % → 0 % |
| Warsaw Pact, careless | 620.5 → 705.7 | 2.0% → 3.2% | 21% (42%), 75% (39%) → 73% (79%), 74% (64%) | 2.0 → 2.0 | 5.2 → 1.3 | 100 % → 10 % |
| Warsaw Pact, careful | 616.7 → 685.8 | 1.7% → 2.6% | 25% (60%), 80% (52%) → 73% (83%), 69% (72%) | 2.0 → 2.0 | 4.0 → 0.6 | 50 % → 0 % |
| NATO, careless | 744.3 → 813.6 | 1.2% → 2.2% | 41% (62%), 56% (44%) → 81% (85%), 67% (69%) | 1.7 → 1.0 | 4.5 → 5.5 | 100 % → 20 % |
| NATO, careful | 700.8 → 800.0 | 1.2% → 2.4% | 39% (72%), 67% (67%) → 83% (87%), 67% (76%) | 1.0 → 1.0 | 9.7 → 4.5 | 10 % → 10 % |

### What the first read found, and what was changed

1. **Radar-laid guns were hunted as SAM sites.** The nation's Shilkas, S-60s and KS-30s lay their guns by radar; the
   planner's rule ("guns are for the strike's escort, not DEAD") only tested the radar, and on the tuned theatre no gun
   had one. A quarter of the Pact's and NATO's packages went to Shilka platoons. **Changed:** DEAD, hunter and deception
   packages, and the enemy's measure of the air defences, take radar SAM sites only (`IntelEntry::IsSamRadar`). The
   tuned theatre's results are unchanged (every column but the air defence's, which is now the same set the enemy
   measures).
2. **The neutral neighbour bombed its own oil line**, the smaller pipeline's border station. **Changed:** banned; it
   closes the valve when the war begins (`faction_neutral.json`).
3. **The battery was out of the big wars.** At Kesmark neither the Pact nor NATO came near it: no raid through its
   sector all week. **Changed:** the nation's theatre gives battery 21 a position in those wars too, as the tuned theatre
   does in the rebels' and the neutral's: against NATO the Dombas hub (the helicopter base, the army's artillery, rear base
   and materials depot, the Cegled junction), against the Pact the approach to the Rava dam and the northern group. NATO
   now offers 4.5-5.5 raids a week; the Pact, whose war on the whole nation spreads from the border radars to the
   airfields and command posts of the centre and west, 0.6-1.3 (no fixed position tried did better).
4. **The air defences hold.** The whole nation has 22 radar SAM sites where the tuned theatre had five: the Pact's
   opening blows leave 73 % of them working on day 3 (21-25 % on the tuned theatre) and NATO's rollback is still in its
   first phase on day 7 with two thirds of them working. **Not changed:** this is the larger country working, and what the
   player's battery is for: a battery that survives anywhere keeps the rollback from ending (the enemy's freedom of
   action, `docs/campaign/02_WHAT_WE_DEFEND.md` §5). A measure limited to the area near the enemy's front was tried and
   dropped for that reason. How large an air force fights a whole nation is the rebalance's question (13.8).
5. **Losses rise.** The rebels lose 8-9 % a sortie (2-3 % on the tuned theatre): the south now has the regimental
   Shilkas and Strela-1s and Osa batteries 24 and 25. The Pact loses 2.6-3.2 %, NATO 2.2-2.4 %.
6. **The battery is wrecked less** (the Pact: 50-100 % of runs on the tuned theatre, 0-10 % on the nation's; NATO's
   careless battery 100 % to 20 %): it is one of 22 sites, and the enemy has more to hunt.
7. **It runs fast enough.** About 1.3 s a campaign-week on average (NATO's about 4 s), against 0.1 s on the tuned
   theatre: the planner weighs every candidate against every threat on its routes. Fine for the balance runs and the
   campaign screen's steps; a spatial filter is the remedy if it grows.

The demo plays raids on the nation's theatre (`air_war_demo --theatre theatre_pannonia_nation.json <faction>`): NATO's
hunters ARM'd a vehicle at the Dombas position; the neutral's strike on the south-western depot played through.

### Continued (23 October 2026): fire channels, the warning posts, the neighbours in a played raid

1. **A defence engaged a whole flight at once.** Every aircraft of a passing four-ship got its own chance from an S-75 or
   S-125 whose guidance radar takes one target at a time. **Changed:** a defence has fire channels (`defence.channels`:
   the S-75M2, S-125M1, Kub, KS-30 and S-60 batteries one, an Osa battery and a Shilka or Strela-1 platoon four, MANPADS
   three, the ZU-23s every aircraft in reach), scaled by how much of it works; the resolver and the planner's estimate
   share the rule (`EngagedAircraft`). The tuned theatre has no channels and is unchanged to the byte; on the nation's
   (10 seeds, 7 days; before → after):

| Adversary, battery | Sorties a week | Loss % | Defences working D3, D7 (believed) | Phase D7 | Battery raids a week | Battery wrecked |
|---|---|---|---|---|---|---|
| Rava Liberation Front's air wing, careless | 22.1 → 22.0 | 8.6% → 7.3% | 100% (100%), 100% (100%) → 100% (100%), 100% (100%) | 1.0 → 1.0 | 0.8 → 0.8 | 0 % → 0 % |
| Rava Liberation Front's air wing, careful | 23.6 → 22.5 | 8.1% → 7.1% | 100% (100%), 100% (100%) → 100% (100%), 100% (100%) | 1.0 → 1.0 | 0.5 → 0.5 | 0 % → 0 % |
| Noric Republic's air force, careless | 31.5 → 31.5 | 3.5% → 3.5% | 100% (100%), 100% (100%) → 100% (100%), 100% (100%) | 2.0 → 2.0 | 1.5 → 1.5 | 0 % → 0 % |
| Noric Republic's air force, careful | 39.7 → 39.7 | 3.8% → 3.8% | 100% (100%), 100% (100%) → 100% (100%), 100% (100%) | 2.0 → 2.0 | 1.5 → 1.5 | 0 % → 0 % |
| Warsaw Pact, careless | 705.7 → 695.1 | 3.2% → 3.2% | 73% (79%), 74% (64%) → 61% (75%), 61% (48%) | 2.0 → 2.0 | 1.3 → 1.9 | 10 % → 20 % |
| Warsaw Pact, careful | 685.8 → 682.3 | 2.6% → 3.1% | 73% (83%), 69% (72%) → 65% (81%), 62% (65%) | 2.0 → 2.0 | 0.6 → 1.4 | 0 % → 0 % |
| NATO, careless | 813.6 → 779.1 | 2.2% → 1.8% | 81% (85%), 67% (69%) → 74% (81%), 64% (66%) | 1.0 → 1.0 | 5.5 → 5.9 | 20 % → 10 % |
| NATO, careful | 800.0 → 781.5 | 2.4% → 1.9% | 83% (87%), 67% (76%) → 77% (84%), 65% (70%) | 1.0 → 1.0 | 4.5 → 4.3 | 10 % → 0 % |

   NATO loses less (1.8-1.9 % a sortie) and gets further with its rollback; the Pact's air operation leaves 61-65 % of
   the radar SAM sites working on day 3 where it left 73 %.
2. **The warning nets.** The radar companies and Osa batteries 23 to 25 are warning posts (`c2_source`); a raid's
   scenario takes, of each kind, the nearest that still works within 150 km (the radar company the battery is nearest,
   the neighbouring battery on the radio), not every post in reach.
3. **A played raid's way in met nothing.** After a played raid the campaign resolved the rest only from the scenario's
   end, so everything before it (a NATO raid's 150 km over the western defences, and flights elsewhere in the package)
   went unopposed, and a flight not in the scenario that released before its end never delivered. **Changed:** every
   part of every route is resolved except what the scenario flew, inside its circle while it ran (`ResolvedPieces`).
   Unplayed raids are untouched (the tuned balance is identical to the byte).
4. **The neighbours in a played raid** (17.1's first form). Our other air defences whose zones reach into the scenario
   take their chances at the aircraft passing through, by the resolver's rule, rolled in the mission bridge as each
   aircraft enters a zone; one hit goes down in the scenario half way through it, credited to that defence, which also
   spends its rounds and shows itself (`docs/air_war/05_EXECUTION.md` §4). In the demo's raids on the nation's theatre
   17-27 neighbours are in reach of a raid against the battery; the Pact's raids near the Rava dam lost three aircraft
   to them in five. They are not drawn yet.

The demo's raids also show the battery's own crew launching in few of them (on both theatres): the scenarios' own
balance, the open item in §6.

### Cruise missiles (23 October 2026)

The war has a year (`TheatreDef::year`, 1985) and each aircraft or missile type the year it entered service: only what
is in service flies. A cruise missile is a type with `"missile": true`: flown as a flight of its own mission kind
(`missile`) from its launch (its carrier's release at the border entry, or the front's sea launch area through it) to a
fixed target in its range, high and then a dive or low over the ground; from a stock resupplied daily; engaged by every
defence on its way like an aircraft, but too fast for some (`defence.max_target_mps`: the Osa-AK's 500 m/s,
`voenizdat_osa_1991` p15; the others estimates), and never a crew lost: one shot down is the defence's
(`PackageOutcome::missiles_downed`). The planner weighs a salvo's expected effect against the missiles spent, dearer
as the stock runs down; no crew at risk, so the commander's loss limit doesn't stop it. In a played raid the missiles are
targets of the engagement model's `cruise_missile` class; each strikes where it arrives or is shot down.

| Type | Who, from when | Notes |
|---|---|---|
| Kh-22M | the Pact, from the long-range aviation's Tu-22Ms, in service from the 1970s | about Mach 3 at height and a dive; too fast for the Osa and the S-125; the S-75 can take it |
| BGM-109C Tomahawk (TLAM-C) | NATO, from ships beyond the western border, from 1986 | the conventional land-attack Tomahawk (the nuclear TLAM-N 1984); low and slow, small on radar |
| AGM-86C CALCM | NATO, from B-52s, from 1988 | the nuclear AGM-86B converted from June 1986, first used in January 1991 |

So in the 1980-85 war only the Pact fires cruise missiles; a later war (`year` 1986 or after) gives NATO its Tomahawks,
and from 1988 the CALCMs. The tuned theatre's rebels, neutral and NATO are unchanged to the byte; the Pact fires about
eight Kh-22s a week (5 % of its packages), the S-75s shooting down 0.2 on the tuned theatre and 0.3-0.7 on the nation's.

In the test raid of four Tomahawks past the battery (`tests/test_air_war.cpp`) the crew had them on the PU-12M's board
at 17 km from the regiment's radars, designated, took one on the TV and was cleared to launch, but the tracking radar had
no echo from so small a target (range by hand) and no missile went before they struck; the search radar saw them at
3.6 km. `flak11` gives the Osa 0.12-0.38 against cruise missiles: whether the crew launches on a TV track with the range
by hand is a question for the scenarios' own balance (§6).

### Still to do in N4, and after

- The other Osa and regimental batteries as fighting batteries in the battery's fire model where the scenario has them
  (beyond their abstract chances). (The neighbours drawn in the scenario and on the PU-12M's board: done 23 Oct 2026,
  `docs/air_war/05_EXECUTION.md`.)
- The game's campaigns (`USimAirCampaignSubsystem`) stay on the tuned theatre until the rebalance (13.8) has run on the
  nation's.
- The regiment's tasking (phase 13): the battery moved to where the enemy strikes, which is the remedy for the Pact's
  dispersed war.
- Whether the enemy's pre-war survey knows our units' prepared positions (today it knows an object, then follows it).
