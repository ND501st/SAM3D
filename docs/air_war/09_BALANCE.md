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

### The other batteries fighting (23 October 2026)

The regiment's other Osa batteries near a played raid no longer take abstract chances: the raid's scenario marks an Osa
neighbour `"fights"`, with its launchers that work (four times its working share) and its missiles, and the runner sets
its launchers up in the battery's combat formation about its place (two pairs 3 km apart, 200 m within) as shooters of
the battery fire model, the one the player's battery's other launchers fight in (`battery_fire.hpp`: the network chart's
times, the firing zones and hit chances of the Osa's gameplay tuning). Their own post is not ours, so each takes the
nearest hostile in its reach itself. A missile launched is logged against its target, a hit is "destroyed by" the
battery, and the mission bridge reads their kills, missiles and radar time from the runner instead of rolling for them
(`ScenarioRunner::NeighbourFights`, `docs/air_war/05_EXECUTION.md`). The other systems (the S-75s, S-125s, the Kub, the
guns and the Strelas) keep the abstract chance: the game has no fire model of theirs. In the test, battery 23 thirty
kilometres up the practice drone's way in shot it down with its first missile.

### The rebalance on the nation, and the game's campaigns on it (23 October 2026)

**How large an air force fights a whole nation** (point 4 of the first read). The factions' squadrons were sized for the
tuned district; a war on the whole nation is fought by more. A theatre's front now says by how much
(`"force_scale"`: every squadron's aircraft and the packages the staff plans a cycle, `max_packages_per_cycle`; the
cruise missiles' stocks are the long-range aviation's and the fleet's and stay): the Pact's front **2** (the Air Forces
of the Front have two or three divisions of fighters and fighter-bombers, `fm100_2_3` pp. 4-127 to 4-134, and the
faction's squadrons are one division's worth), NATO's **1.5** (a game value: an allied tactical air force commits more
wings to the whole nation than to a district; at 1.5 and at 2 its rollback is held back alike by the sites returning to
service, not by its numbers). The rebels and the neutral fight as they are. Scaling the squadrons alone changed little:
the staff's packages a cycle held the tempo, so they scale too. The tuned theatre has no scale and is unchanged.

10 seeds, 7 days, on the nation's theatre (before → after):

| Adversary, battery | Sorties a week | Loss % | Defences working D3, D7 (believed) | Phase D7 | Battery raids a week | Battery wrecked |
|---|---|---|---|---|---|---|
| Warsaw Pact, careless | 697.8 → 1150.5 | 2.6% → 3.3% | 57% (71%), 68% (49%) → 41% (55%), 75% (52%) | 2.0 → 2.0 | 3.2 → 7.1 | 0 % → 0 % |
| Warsaw Pact, careful | 675.0 → 1119.2 | 3.0% → 3.2% | 57% (78%), 71% (60%) → 47% (68%), 72% (57%) | 2.0 → 2.0 | 1.7 → 6.2 | 0 % → 0 % |
| NATO, careless | 779.1 → 1106.8 | 1.8% → 2.4% | 63% (72%), 68% (69%) → 55% (63%), 72% (66%) | 1.0 → 1.0 | 5.9 → 8.1 | 10 % → 20 % |
| NATO, careful | 781.5 → 1095.6 | 1.9% → 2.3% | 67% (77%), 73% (74%) → 52% (67%), 74% (73%) | 1.0 → 1.0 | 4.3 → 6.8 | 0 % → 10 % |

Read against the shapes (§2): the Pact's two massed days now break much of the air defence (44 packages and over 300
sorties on day 1, 41-47 % of the radar SAM sites working on day 3; the tuned theatre's 21-25 % is a district of five
sites), then a lower sustained rate while sites return; it fires about 27 Kh-22s a week, its missile packages at their
floor of one a cycle. NATO's air defences fall over the first three days (77-79 % working on day 1, 52-55 % on day 3) and
the rollback stalls as sites return (72-74 % on day 7), with ARMs to spare: the shape of a rollback that cannot keep
the sites down, which is the player's battery's work (§8, point 4). Both now offer the battery 6-8 raids a week, as the
tuned theatre's 4-9. The rebels (0.5-0.8 raids a week, 7 % losses a sortie in the south's regimental air defence) and the
neutral (1.5) are unchanged.

**The game's campaigns move to the nation's theatre.** A new campaign on the missions menu is fought on
`theatre_pannonia_nation.json` (`USimAirCampaignSubsystem::TheatreFile`); a saved one goes on on the theatre it was saved
with. Its raids open in the Kesmark range's level, which now draws the whole country around the district (the nation on
the map N5, `docs/COUNTRY_MAP.md`): battery 21 stands at the Rava dam against the Pact, the Dombas hub against NATO and
in the south against the rebels and the neutral, on the map's ground everywhere (its far ground is the country's).
Checked: the campaign screen on the nation's theatre against NATO (the country under it, a 19-aircraft raid warned down
the chain), and Unreal's `SAM3D.AirWar.Raid` automation test.

### Still to do, after N4

- The ledgers against a careful and a careless battery, and the factions' target choice re-read (13.8, with phase 13's
  roles and ledgers).
- The regiment's tasking (phase 13): the battery moved to where the enemy strikes.
- A fire model for the other systems (the S-75, S-125 and Kub batteries, the guns), so they fight in a raid as the Osa
  batteries do.
- Whether the enemy's pre-war survey knows our units' prepared positions (today it knows an object, then follows it).

## 9. Against history: the benchmarks (23 October 2026)

The weapons, attacks, support, forces and logistics of `10_WEAPONS.md` to `14_LOGISTICS.md` were calibrated against
what the period's air wars recorded (`docs/research/AIR_WAR_MISSIONS_2026-10.md`). `simcore/examples/air_war_bench.cpp`
flies each analogue through the abstract resolver, with the theatre's own defence values, many times:

```bash
./simcore/build/air_war_bench data 400                                     # Windows: simcore\build\Release\air_war_bench.exe
./simcore/build/air_war_bench data 400 --why --set attrition.pass_scale=0.4   # losses by role and defence; try a value
```

| Benchmark | The model | History |
|---|---|---|
| A massive bridge, F-4Es with Mk 84s from medium altitude, up to 871 sorties | a span down in 29 % of campaigns | the Thanh Hoa bridge stood through 871 sorties (`wiki_thanh_hoa`) |
| The same bridge, one strike of 24 GBU-10s | a span down in 48 % of strikes | damaged 27 April 1972, dropped 13 May |
| Average miss: unguided from medium altitude, laser-guided | 472 ft, 21 ft | 447 ft, 23 ft (`afmag_2003_bombs`) |
| Losses a 100 sorties over an airfield at night: low with JP233, medium with Mk 84 | 6.2, 0.4 | about 4 times a sortie low (`rjlee_ds_losses`) |
| An airfield's runways cut: 2 Tornados with JP233; 4 Su-24s with BetAB in dives; 4 F-16s with Mk 84s from medium altitude | 54 %, 96 %, 5 % | Focus's flights of four closed runways (`wiki_focus`); medium altitude cratered few |
| ARMs for each radar destroyed: radiating crews, crews that switch off | 3.6, 11.7 | the Bekaa; Allied Force's 743 HARMs for 3 SA-6 batteries (`rand_mr1365`) |
| Unretarded bombs from low level that go off | 45 % | 11 of 25 (`navalgazing_falklands`) |
| Four F-4Es popping up behind four Kub batteries and guns, no suppression | 25 lost a 100 sorties | 4 a 100 over all of Yom Kippur's first three days (`jvl_1973_losses`); Dugman 5's six F-4s (`wiki_model5`) |
| A suppression package (decoys, a jammer, four F-4Gs, four F-4Es) against a radiating Kub battery | destroyed in 66 %, 4.5 lost a 100 sorties | the Bekaa: 17 of 19, none lost to them (`bekaa_popular`) |

### What the first read found, and what was changed

1. **Unguided bombs dropped the bridge** (264 sorties). A very hard target's effects were cut to a few per cent per
   hit (a Mk 84's 0.1 to 0.03); 24 Paveways now drop a span half the time, as in 1972, and dumb bombs from medium
   altitude seldom in 871 sorties.
2. **Runway bombs did nothing from a dive** (8 %): the footprint was a crater's. A runway is cut by a crater anywhere
   across its width (`delivery.runway_strip_m` 25 m); JP233's effect came down from 0.95 to 0.6 (a pair's pass cuts half
   an airfield, not all of it).
3. **Medium altitude was as deadly as low.** The benchmark's own guns and Strela had been given medium cover by mistake;
   fixed, low costs nine to fifteen times medium against an airfield's thick guns (Desert Storm's average was about four).
4. **SAMs were far too lethal per pass.** A pair of Wild Weasels past five radiating Kubs died 92 % of the time, a 4-ship
   into a belt lost 44-50 %. Every defence's lethality is now a relative one times `attrition.pass_scale` (0.5); SEAD
   suppresses 0.6 (was 0.45); a radar that fires at decoys is suppressed at least 0.6 by the SEAD behind them
   (`attrition.decoy_exposure`, the Bekaa's method). The belt now costs 25 a 100 sorties, the Bekaa package 4.5.
5. **ARMs barely cared about discipline** (5.9 against 3.7 for each radar). An ARM with memory against a radar that
   switched off in time now kills 0.08 (was half the radiating chance): 11.7 against 3.6.
6. **Light air arms stopped flying.** With weapons that do what they did, a pair of light aircraft achieves a few per
   cent; their doctrines got their own thresholds (`13_PACKAGES.md`), a bomb's blast its reach against soft and light
   targets, and the border battalion and the artillery their class (light).

## 10. The rebalance with weapons, support, build-ups, sizes and logistics (23 October 2026)

`air_war_balance data 20 7` (tuned) and `--theatre theatre_pannonia_nation.json data 10 7` (the nation) after §9 and with
`utility.sortie_cost` 0.1, the support orbits, the build-ups and our supply lines; before is §3 (tuned) and §8 (nation).

| Adversary, battery (tuned theatre) | Sorties a week | Loss % | Defences working D3, D7 | Battery raids a week | Attackers a package |
|---|---|---|---|---|---|
| Rebels, careless | 24.6 → 21.9 | 2.6 % → 2.3 % | 100 %, 100 % | 0.9 → 2.9 | 2.0 |
| Rebels, careful | 25.1 → 18.8 | 3.2 % → 2.1 % | 100 %, 100 % | 0.6 → 1.0 | 2.0 |
| Neutral, careless | 39.0 → 35.1 | 3.6 % → 4.4 % | 100 %, 100 % | 2.7 → 7.7 | 4.9 |
| Neutral, careful | 44.6 → 25.2 | 2.7 % → 5.2 % | 100 %, 100 % | 2.0 → 2.9 | 5.1 |
| Warsaw Pact, careless | 625.5 → 560.1 | 2.1 % → 1.1 % | 24 %, 86 % → 26 %, 78 % | 4.7 → 6.8 | 7.1 |
| Warsaw Pact, careful | 609.9 → 560.6 | 2.2 % → 1.3 % | 35 %, 82 % → 39 %, 80 % | 4.0 → 9.3 | 6.8 |
| NATO, careless | 727.0 → 515.9 | 1.2 % → 0.9 % | 28 %, 67 % → 40 %, 69 % | 5.0 → 5.2 | 5.3 |
| NATO, careful | 690.6 → 496.2 | 1.1 % → 0.7 % | 36 %, 70 % → 56 %, 64 % | 9.2 → 8.1 | 5.2 |

Read against the shapes (§2) and the period's rates:

1. **Losses fell to the period's.** The Pact loses 1.1-1.3 % a sortie and NATO 0.7-0.9 % (Israel's average over the
   October war was about 1 %; Desert Storm's far less). The planners, expecting less, are bolder: the Pact offers the
   battery 7-9 raids a week.
2. **NATO's first week is lighter** (500 sorties against 700): its force arrives over four weeks (`12_FORCES.md`), so the
   rollback is slower to bite (40-56 % of the radar SAM sites working on day 3 against 28-36 %).
3. **The Pact's opening blows still break the air defences** (26-39 % working on day 3) and size themselves to the
   target: 10-12 attackers at airfields and command posts, 4 at radars and SAM sites.
4. **Weapons and attacks as the doctrines flew them.** The Pact: Kh-29Ls and KAB-500Ls from height, RBK-500s and BetABs
   in dives, retarded FAB-500ShPs laid down; NATO: Paveways, Mavericks, Rockeye, JP233 and BL755 low, tosses.
5. **The supply war grinds.** The Pact's bridges down cut our lines for 66-100 line-hours a week and the troops end it
   with 80-86 % of their stocks; armed reconnaissance flies 6-11 searches a week and finds a column in 27-38 % of them.
6. **The light air arms fly again**, the rebels by night with their crop sprayers (43-49 % of their packages), more
   often through the battery's sector when it radiates all day (2.9 a week); the neutral's losses rose with the
   regiment's guns in the south (4-5 %).

| Adversary, battery (the nation) | Sorties a week | Loss % | Defences working D3, D7 | Battery raids a week |
|---|---|---|---|---|
| Rebels, careless | 22.0 → 17.9 | 7.3 % → 7.3 % | 100 %, 100 % | 0.8 → 1.4 |
| Rebels, careful | 22.5 → 18.6 | 7.1 % → 4.3 % | 100 %, 100 % | 0.5 → 0.6 |
| Neutral, careless | 31.5 → 30.9 | 3.5 % → 3.6 % | 100 %, 100 % | 1.5 → 7.3 |
| Neutral, careful | 39.7 → 25.6 | 3.8 % → 4.3 % | 100 %, 100 % | 1.5 → 4.3 |
| Warsaw Pact, careless | 1150.5 → 1044.3 | 3.3 % → 2.0 % | 41 %, 75 % → 38 %, 72 % | 7.1 → 7.8 |
| Warsaw Pact, careful | 1119.2 → 1009.4 | 3.2 % → 2.1 % | 47 %, 72 % → 42 %, 72 % | 6.2 → 7.0 |
| NATO, careless | 1106.8 → 736.8 | 2.4 % → 1.5 % | 55 %, 72 % → 71 %, 69 % | 8.1 → 5.9 |
| NATO, careful | 1095.6 → 732.3 | 2.3 % → 1.7 % | 52 %, 74 % → 66 %, 66 % | 6.8 → 6.8 |

On the whole nation the shapes hold: the Pact's opening blows leave 38-42 % of the radar SAM sites working on day 3,
NATO's rollback starts slower while its force arrives (66-71 % on day 3), both offer the battery 6-8 raids a week, and
their losses fall to 1.5-2.1 % a sortie. The 14 supply lines are cut 75-127 line-hours a week by bridges down, sent
round one to three times, and searched 8-14 times; the troops end the week with 83-87 % of their stocks (NATO's
interdiction of the bridges bites hardest). The neutral's helicopter raids and the rebels' harassment come through the
battery's sector more often when it radiates (4-7 a week for the neutral).

### Still to do, after this pass

- The direct-hit tail of unguided bombs (gross errors) if a target's survival turns on single hits.
- Exercise records as a second check (`docs/research/AIR_WAR_MISSIONS_2026-10.md` §7).
- Our side's reports of the supply war through the chain of command, and the regiment's answer to cut lines (13.x).

## 11. The weapons' effects branch: before and after (1 November 2026)

The branch that gives the fight its effects (`docs/fx/`) changed the mission bridge's timing: a release where the
aircraft's own path takes it to the release point (a pop-up only in its dive after the apex), each load falling for its
own time and landing then, and a package handed back to the campaign only once its last load has landed
(`LivePackages`). `air_war_balance data 20 7` was built and run from HEAD before the branch and from the branch.

| Adversary, battery (tuned theatre) | Sorties a week | Loss % | Defences working D3, D7 | Battery raids a week |
|---|---|---|---|---|
| Rebels, careless | 21.9 = 21.9 | 2.3 % = 2.3 % | 100 %, 100 % | 2.9 = 2.9 |
| Rebels, careful | 18.8 = 18.8 | 2.1 % = 2.1 % | 100 %, 100 % | 1.0 = 1.0 |
| Neutral, careless | 35.1 = 35.1 | 4.4 % = 4.4 % | 100 %, 100 % | 7.7 = 7.7 |
| Neutral, careful | 25.2 = 25.2 | 5.2 % = 5.2 % | 100 %, 100 % | 2.9 = 2.9 |
| Warsaw Pact, careless | 560.1 = 560.1 | 1.1 % = 1.1 % | 26 %, 78 % | 6.8 = 6.8 |
| Warsaw Pact, careful | 560.6 = 560.6 | 1.3 % = 1.3 % | 39 %, 80 % | 9.3 = 9.3 |
| NATO, careless | 515.9 = 515.9 | 0.9 % = 0.9 % | 40 %, 69 % | 5.2 = 5.2 |
| NATO, careful | 496.2 = 496.2 | 0.7 % = 0.7 % | 56 %, 64 % | 8.1 = 8.1 |

The two outputs are the same byte for byte, and the same as §10: the balance tool resolves the battery's raids by the
abstract model (`SkipPlayerMission`), so the bridge, where everything the branch changed lives, never runs in it.

The raids played through the bridge were read with `air_war_demo <faction> <seed> data <out> 7`, seeds 1-3 for each
adversary, before and after (summed over the three seeds):

| Adversary | Raids played | Aircraft | Our launches | Shot down (by neighbours) | Radars on, s | ARMs | Our vehicle hit | Enemy aircraft lost |
|---|---|---|---|---|---|---|---|---|
| Rebels | 3 = 3 | 5 = 5 | 10 = 10 | 2 (0) = 2 (0) | 5647 = 5647 | 0 = 0 | 0 = 0 | 3 = 3 |
| Neutral | 8 = 8 | 33 = 33 | 2 = 2 | 0 (0) = 0 (0) | 3753 = 3753 | 0 = 0 | 0 = 0 | 1 = 1 |
| Warsaw Pact | 33 = 33 | 105 = 105 | 0 = 0 | 1 (1) = 1 (1) | 7997 = 7997 | 84 = 84 | 6 = 6 | 0 = 0 |
| NATO | 21 = 21 | 165 = 165 | 0 = 0 | 3 (13) = 3 (13) | 11754 = 11754 | 42 = 42 | 2 = 2 | 0 = 0 |

The enemy's knowledge of the battery at the end and the journal's length are the same too. What differs is only the
order of the aircraft's releases and hits inside a package's line in the journal (a load now lands at its own time, so
two aircraft's hits can swap places; in one neutral campaign the same number of hits is credited to different aircraft).

**No rebalance is needed.** The caveat: the balance tool does not play raids through the bridge, so its tables cannot
show the bridge's effect; the demo's three seeds are the check of it, and a larger played sample is left for a person's
campaign playthrough. Known gaps the branch leaves in the bridge's deliveries (`docs/fx/README.md` §7): the toss lets
go about 1.8 km early (its aim a third farther than its throw, the planner's data unchanged), and the pop-up's
terrain-following climb tops out near its release point (about 1140 m) instead of the planner's apex.

## 12. The nation's theatre by the sun (2 November 2026)

Roadmap R7 gave every session one sun (`docs/LIGHT.md`), and the air war uses it only once a theatre has a `"date"`
(`DayPartBySun`, `DayPartHourBySun`, §5 there). `theatre_pannonia_nation.json` now has one: 15 October 1985, UTC+1. At
its origin (46.9 N 19.7 E) the parts of the day become the sun's, and the planner aims each at its sun's middle:

| Part | The buckets (undated, still the tuned theatre's) | By the sun on 15 October | The planner's aim |
|---|---|---|---|
| Dawn | 04:00-07:00 | 05:29-06:41 (the sun from 6 degrees down to 6 up) | 05:30 → sunrise, 06:04 |
| Day | 07:00-18:00 | 06:41-16:13 | 12:00 → noon, 11:27 |
| Dusk | 18:00-21:00 | 16:13-17:25 | 19:30 → sunset, 16:50 |
| Night | 21:00-04:00 | 17:25-05:29 | 01:00 → midnight, 23:27 |

`air_war_balance data 20 7` (the tuned theatre, undated) is byte for byte the same totals as §11: R7 changes nothing
there. Nor in the raids it plays as sessions (`air_war_demo`, `test_packages_live.cpp`): a raid from an undated
theatre carries no clock and keeps the legacy light keys from the planner's part (`docs/LIGHT.md` §5), so it is lit as it
was planned and as before R7. `--theatre theatre_pannonia_nation.json data 10 7` was run undated (identical to §10's nation table) and dated:

| Adversary, battery (the nation) | Sorties a week | Loss % | Defences working D3, D7 | Battery raids a week |
|---|---|---|---|---|
| Rebels, careless | 17.9 → 16.1 | 7.3 % → 6.2 % | 100 %, 100 % | 1.4 → 1.5 |
| Rebels, careful | 18.6 → 15.7 | 4.3 % → 8.3 % | 100 %, 100 % | 0.6 → 0.5 |
| Neutral, careless | 30.9 → 28.4 | 3.6 % → 4.9 % | 100 %, 100 % | 7.3 → 6.0 |
| Neutral, careful | 25.6 → 31.6 | 4.3 % → 4.4 % | 100 %, 100 % | 4.3 → 3.6 |
| Warsaw Pact, careless | 1044.3 → 1035.5 | 2.0 % → 2.0 % | 38 %, 72 % → 35 %, 72 % | 7.8 → 9.8 |
| Warsaw Pact, careful | 1009.4 → 1059.2 | 2.1 % → 1.7 % | 42 %, 72 % → 36 %, 72 % | 7.0 → 8.6 |
| NATO, careless | 736.8 → 747.7 | 1.5 % → 1.8 % | 71 %, 69 % → 69 %, 67 % | 5.9 → 6.3 |
| NATO, careful | 732.3 → 740.5 | 1.7 % → 1.6 % | 66 %, 66 % → 70 %, 65 % | 6.8 → 6.5 |

The shapes hold and most of the shift is within ten seeds' noise: the light air arms lose one aircraft a week or so,
so a loss rate moving between 4 and 8 % is a fraction of an aircraft. What moved with the sun: the rebels' strikes
fell from 17 % to 6 % of their packages when careless (their dusk is an hour and a quarter now, not three, and their
night twelve hours, not seven), the Pact offers the battery one or two raids a week more, its opening blows leave 35-36 % of the radar SAM sites working
on day 3 against 38-42 %, and the bridges are cut a little less (Pact 93 and 71 line-hours a week against 102 and 75;
NATO 114 and 111 against 113 and 127). `air_war_bench data 400` builds its benches on the tuned theatre's values, undated, and runs as before.

**No rebalance is needed.** The nation's theatre stays dated; the tuned theatre stays undated (the tests and §3-§11 were read on
its buckets) until S8 (§18).


## 13. Command in the air and the time-stepped resolver (track B, B5, 17 November 2026)

B5 (`docs/air_battle/05_C2_AND_RETASKING.md` §9) changed two things the balance tool sees. First the resolver became
time-stepped (A0): a package takes off at its first aircraft's hour and flies leg by leg beside the campaign's clock in
steps of `c2.step_h` (5 min), its draws from a fork keyed by its id, and its outcome is applied when it lands; before,
each package was resolved whole at its first hour from the campaign's own stream. Second, the enemy's posts: reports in
flight, the FACs' sightings, diverts, hunters warned and the alert pools flying the planner's held-back on-call
sorties. `air_war_balance data 20 7` was run from HEAD before B5, from B5 with every faction's `"c2"` block removed (the
A0 step alone), and from B5:

| Adversary, battery (tuned theatre) | Packages | Sorties a week | Lost | Loss % | Defences working D3, D7 | Battery raids a week | On-call sorties (share) |
|---|---|---|---|---|---|---|---|
| Rebels, careless | 13.7 → 13.6 → 13.6 | 21.9 → 21.6 → 21.6 | 0.5 → 0.5 → 0.5 | 2.3% → 2.3% → 2.3% | 100%, 100% throughout | 2.9 → 2.5 → 2.5 | none (no posts) |
| Rebels, careful | 11.9 → 13.0 → 13.0 | 18.8 → 20.6 → 20.6 | 0.4 → 0.7 → 0.7 | 2.1% → 3.2% → 3.2% | 100%, 100% throughout | 1.0 → 1.1 → 1.1 | none (no posts) |
| Neutral, careless | 16.6 → 13.5 → 13.5 | 35.1 → 29.2 → 29.2 | 1.6 → 1.9 → 1.9 | 4.4% → 6.3% → 6.3% | 100%, 100% throughout | 7.7 → 6.0 → 6.0 | 0 (pools stood down) |
| Neutral, careful | 11.0 → 13.7 → 13.7 | 25.2 → 30.9 → 30.9 | 1.3 → 1.1 → 1.1 | 5.2% → 3.7% → 3.7% | 100%, 100% throughout | 2.9 → 3.7 → 3.7 | 0 (pools stood down) |
| Warsaw Pact, careless | 94.7 → 94.2 → 97.5 | 560.1 → 556.4 → 563.1 | 6.3 → 8.6 → 8.7 | 1.1% → 1.5% → 1.5% | 26%, 78% → 28%, 79% → 27%, 81% | 6.8 → 6.4 → 6.5 | 17.2 (3.1%) |
| Warsaw Pact, careful | 95.0 → 93.2 → 98.5 | 560.6 → 552.5 → 571.5 | 7.0 → 8.4 → 9.8 | 1.3% → 1.5% → 1.7% | 39%, 80% → 41%, 76% → 39%, 79% | 9.3 → 9.5 → 8.4 | 20.2 (3.5%) |
| NATO, careless | 91.5 → 91.5 → 94.2 | 516.1 → 515.5 → 530.5 | 4.8 → 5.2 → 5.2 | 0.9% → 1.0% → 1.0% | 40%, 69% → 50%, 61% → 49%, 60% | 5.2 → 4.8 → 4.8 | 13.8 (2.6%) |
| NATO, careful | 89.6 → 90.5 → 93.5 | 496.1 → 501.0 → 513.3 | 3.6 → 5.1 → 4.9 | 0.7% → 1.0% → 1.0% | 56%, 65% → 56%, 64% → 55%, 66% | 8.2 → 7.2 → 7.4 | 12.8 (2.5%) |

The tool's new line, "Command in the air", a campaign (20 seeds, careless then careful):

| Adversary | In-flight reports (out of cover) | Requests | Scrambles (refused) | FAC briefs | Diverts, hunters warned | Our listeners heard | Pools (aircraft), stood down unused | On-call packages |
|---|---|---|---|---|---|---|---|---|
| Neutral | 6.7 (0.0), 3.8 (0.1) | 0 | 0 | 0 | 0, 0 | 1.1, 0.6 calls | 0.6-0.8 (1.2-1.6), all | 0 |
| Warsaw Pact | 10.4 (34.6), 11.2 (37.9) | 15.2, 15.6 | 5.2 (0.9), 6.0 (1.0) | 4.9, 5.7 | 0, 0 | 0.5 calls | 18.3-18.5 (73-74), 11 | 4.3, 5.0 |
| NATO | 0.0 (0.5), 0.0 (0.3) | 9.1, 8.3 | 5.0 (1.6), 5.0 (1.8) | 4.1, 4.0 | 0, 0 | 0 | 12.9-13.2 (52-53), 7 | 3.5, 3.2 |

**What moved, and why.**

- **The A0 step alone (the first arrow).** Nothing in the model of a leg changed (`test_c2`: the stepped resolver gives
  the atomic one's outcome with the same stream). Two things did: the legs of a package's flights are walked in the
  order of time (before, each flight was walked whole, decoys and SEAD first), and a package's draws now come from its
  own fork, not the campaign's stream, so from the first package on every campaign takes a different but equally
  likely course. For the light air arms, who lose about one aircraft a week, that is a reshuffle within the seeds'
  noise: run over 60 seeds instead of 20, the rebels give 19.3 → 19.3 and 19.1 → 19.8 sorties, 2.9 % → 2.1 % and
  1.7 % → 2.1 % loss, and the neutral 29.1 → 25.7 and 23.4 → 24.9 sorties, 4.8 % → 5.9 % and 3.7 % → 3.9 % loss (the
  20-seed rows above swing as much between the base's own 20 and 60 seeds: the neutral careless 35.1 against 29.1).
- **The Pact's and NATO's loss rate up by 0.2-0.4 points with A0.** A cycle's packages are in the air together now.
  Before, a package resolved at its first hour wrecked and suppressed radars at once, so a package taking off minutes
  later met defences already cut by one still on its way in. Now a package's damage counts when it lands, so packages
  flying together meet the defences as they stand: the Pact loses 8.4-8.6 aircraft a week against 6.3-7.0, NATO
  5.1-5.2 against 3.6-4.8. NATO's careless opening blow cuts less by D3 (50 % working against 40 %), and its campaign
  ends lower by D7 (61 % against 69 %); the careful rows and the Pact's hardly move, so this is read as the same
  reshuffle within the seeds' noise.
- **The posts (the second arrow).** The rebels have no posts and the neutral's reports go to intelligence (its
  authority has no divert, and its one ground pool, 15 % of its few sorties, finds nothing it may scramble for), so their
  rows do not move. The Pact and NATO fly 2.5-3.5 % of their sorties from the alert pools (`on_call_fraction` 0.2 and
  0.1 of the capacity is held back; most pools stand down unused, 11 of 18 and 7 of 13 a week), so packages rise by 3
  and sorties by 7-19 a week; the Pact's on-call packages go to reported launches and columns and lose about one
  aircraft more a week when the battery is careful (9.8 against 8.4). NATO reports in flight only from CAS and BAI
  (its `report_missions`), which rarely see a launch in the abstract campaign, so its scrambles come from the FACs'
  requests (4 a week) through the ASOC; the Pact's 10-11 reports a week reach a post, 35-38 are out of every post's
  cover and wait for the debrief. No flight was diverted and no hunter warned in these campaigns: a divert needs an
  armed reconnaissance flight within 40 km at the Pact's 20 % chance or a NATO CAS or BAI flight near a report, and the
  hunters' leave needs a report inside a Weasel's package or the Pact's free-hunt zone; both are exercised by
  `test_c2`, and they will matter more once B6 gives the flights targets of opportunity to report.
- **The battery's raids** move within the seeds' noise (the Pact careful 9.3 → 8.4, NATO careful 8.2 → 7.4).

The shapes of §2 hold: the opening blows, the attrition well under the period's 2 % a sortie for the Pact and NATO, the
light air arms still light. **No rebalance is needed.**

**Cost.** The A0 step's sub-steps run only while a package is in the air or a message is on its way. `air_war_balance data 20 7`, the base and B5 back to back, twice, on a quiet machine: 46.1 s and 46.6 s for the base, 46.0 s and 46.9 s for B5, the same within a percent (the quarter allowed is not touched). `air_war_bench data 400` (4.2 and 4.5 s against 5.5 and 4.2 s, back to
back) resolves its benches with `Resolver::Resolve`, now `Begin` + `StepTo` to the end, so only the leg order moved its
rows, and only those where a flight's suppression or losses count for another flying at the same time: JP233 Tornados
against F-16s at medium altitude 6.2 against 0.4 losses a 100 sorties (14.6 times) → 5.4 against 0.9 (5.7 times;
Desert Storm's about 4 times), the runways cut 54 % → 53 %, ARMs for each radar destroyed 3.6 against 11.7 → 3.7
against 12.3, the F-4Es popping up behind the Kub belt 25.0 → 25.3, and the Bekaa package destroying the battery in
66 % → 64 % of runs with 4.5 → 5.1 lost a 100 sorties (the F-4Gs' shots now reach the Kub only as early as they are
flown, not before every other flight's legs). Every other row is the same. `crew_balance data 10`, `helper_coverage` (the same 19 gaps, its page byte for byte), `ground_check data 3600` and `em_bench` give the base's results; only their wall times differ, with the load.

## 14. B2 and B5 together (track B, 18 November 2026)

B2 (routes as flown) and B5 were built side by side and merged on `tb-b5`: the stepped resolver keeps B2's one
engagement a pass, its ground-masked chord, its fighters on the IP's leg and its corridor suppression, and on-call
packages are planned on routes (`docs/air_battle/05_C2_AND_RETASKING.md`, "With B2's routes"). Three faults were found
by the runs and fixed in the merge: a flight under flak reported "a launch seen", so the Pact's posts scrambled DEAD
at the guns (the nation's rule that guns are never hunted, §8; a gun's fire is no longer reported); B2's route planner
read the egress point's node after its side's search had removed it (`RealTurn`, a wrong home turn when the first side
won); and the loader's columns were made at their depot through a pointer the next column's `push_back` could move
(`MakeColumns`, from N7), which crashed `air_war_balance` on the nation when the Pact's campaigns had run before NATO's.
Against B2's own runs (district 20 seeds, nation 10, 7 days; B2 → B2 and B5):

| District | Packages | Sorties a week | Lost | Loss % | Defences working D3, D7 | Battery raids | On-call sorties (share) |
|---|---|---|---|---|---|---|---|
| Rebels, careless | 13.3 → 13.1 | 21.0 → 20.6 | 0.3 → 0.2 | 1.7% → 1.2% | 100%, 100% | 1.7 → 1.6 | none (no posts) |
| Rebels, careful | 14.3 → 13.9 | 23.1 → 22.2 | 0.3 → 0.5 | 1.5% → 2.3% | 100%, 100% | 1.1 → 0.8 | none (no posts) |
| Neutral, careless | 20.9 → 19.5 | 48.2 → 45.9 | 1.1 → 1.6 | 2.4% → 3.4% | 100%, 100% | 10.2 → 8.9 | 0 (pools stood down) |
| Neutral, careful | 16.1 → 17.1 | 32.8 → 38.1 | 1.4 → 0.8 | 4.4% → 2.0% | 100%, 100% | 5.6 → 5.0 | 0 (pools stood down) |
| Warsaw Pact, careless | 94.5 → 97.8 | 552.1 → 568.5 | 6.5 → 9.3 | 1.2% → 1.6% | 30%, 78% → 31%, 80% | 7.5 → 7.2 | 13.8 (2.4%) |
| Warsaw Pact, careful | 93.5 → 99.2 | 555.2 → 578.3 | 7.8 → 7.8 | 1.4% → 1.4% | 38%, 79% → 44%, 78% | 8.4 → 8.2 | 18.8 (3.3%) |
| NATO, careless | 93.8 → 94.7 | 518.5 → 528.0 | 4.8 → 5.2 | 0.9% → 1.0% | 34%, 61% → 39%, 54% | 5.8 → 5.4 | 10.2 (1.9%) |
| NATO, careful | 92.9 → 95.2 | 498.6 → 533.7 | 5.2 → 4.7 | 1.0% → 0.9% | 39%, 71% → 49%, 62% | 7.7 → 6.9 | 17.0 (3.2%) |

| Nation | Packages | Sorties a week | Lost | Loss % | Defences working D3, D7 | Battery raids | On-call sorties (share) |
|---|---|---|---|---|---|---|---|
| Rebels, careless | 9.4 → 10.1 | 13.4 → 14.9 | 1.9 → 1.6 | 14.2% → 10.7% | 100%, 100% | 5.7 → 7.3 | none (no posts) |
| Rebels, careful | 8.5 → 10.2 | 12.2 → 15.3 | 2.4 → 2.1 | 19.7% → 13.7% | 100%, 100% | 2.5 → 2.6 | none (no posts) |
| Neutral, careless | 16.0 → 16.7 | 26.9 → 28.2 | 0.8 → 0.9 | 3.0% → 3.2% | 100%, 100% | 7.0 → 8.5 | 0 (no pools) |
| Neutral, careful | 12.7 → 14.0 | 20.5 → 23.7 | 1.3 → 0.8 | 6.3% → 3.4% | 100%, 100% | 5.3 → 4.0 | 0 (no pools) |
| Warsaw Pact, careless | 182.6 → 183.2 | 992.3 → 989.9 | 28.3 → 30.1 | 2.9% → 3.0% | 36%, 72% → 37%, 66% | 9.3 → 8.4 | 41.2 (4.2%) |
| Warsaw Pact, careful | 173.8 → 189.1 | 947.8 → 1016.7 | 27.7 → 28.2 | 2.9% → 2.8% | 41%, 69% → 41%, 69% | 9.1 → 8.9 | 35.6 (3.5%) |
| NATO, careless | 129.2 → 129.7 | 722.0 → 725.9 | 11.9 → 15.0 | 1.6% → 2.1% | 73%, 71% → 73%, 74% | 8.6 → 7.6 | 17.6 (2.4%) |
| NATO, careful | 131.1 → 128.7 | 724.4 → 721.3 | 13.1 → 14.5 | 1.8% → 2.0% | 76%, 75% → 76%, 78% | 8.4 → 8.0 | 18.0 (2.5%) |

**What B5 moves on top of B2.** The same three things as on the base (§13), and nothing new from the meeting of the two:

- **The posts and the alert pools.** The Pact and NATO fly 2-4 % of their sorties on call from the pools, on the
  district as on the base (2.4-3.3 % against 2.5-3.5 %). On the nation the Pact's posts hear more: 146-154 in-flight
  reports a campaign, three quarters of them out of every post's cover and kept for the debrief, 34-37 scrambles of
  which 24-28 refused as too dangerous for the alert, and 9-10 on-call packages; NATO's come from the FACs' requests
  (15 a campaign, 11 scrambles, 4-5 packages). The rebels have no posts and the neutral's pools find nothing they may
  scramble for, so their rows move only within the noise.
- **Packages flying together.** A package's damage now counts when it lands, so packages in the air at the same time
  meet the defences as they stand. On the nation NATO loses 1.4-3.1 aircraft a week more (2.0-2.1 % against 1.6-1.8 %),
  and the Pact careless 1.8 more; the Pact careful and the district's rows hardly move. All stay inside the period's
  2 % a sortie or close to it, as on the base.
- **A reshuffle of the seeds.** Every package draws from its own fork, so every campaign takes another, equally likely
  course. The light air arms' rows (a loss or two a week, a dozen packages) swing by what one aircraft is worth: the
  rebels on the nation 1.6-2.1 lost against 1.9-2.4, the neutral careful 0.8 against 1.3 in both theatres; §13 showed
  how much such rows swing between 20 and 60 seeds of the same code.

B2's own shapes hold: one engagement a pass, low flights masked by the ground, the Pact's checkpoint and
counter-SAM programme, NATO's gates and corridors. **No rebalance is needed.** `air_war_bench data 400` moves the same
rows as B5 did on the base and by the same amounts (JP233 Tornados against medium-altitude F-16s 5.5 against 1.8 → 2.0
losses a 100 sorties, the runways cut 51 % → 52 %, ARMs for each radar destroyed 3.6 → 3.8 against 11.8 → 12.8, the Bekaa
package destroying the battery in 62 % → 65 % of runs with 7.6 → 6.3 lost a 100 sorties); every other row is B2's.
Wall times on a machine with another build running: the district 295 s (B2's own run took 2090 s under a heavier
load), the nation 1103 s with the test suite running beside it (B2: 1457 s), the bench 0.21 s (0.19 s).

## 15. The air battle (track B, B8, 18 November 2026)

B8 (`docs/air_battle/07_AIR_TO_AIR.md` §8) replaced the resolver's flat fighter roll with the abstract air battle
(`air_war/air_fight.hpp`). The planner now sends sweep and CAP flights by each doctrine's plan and expects its losses to
our fighters from the same model. The 1985 data list of B7 changed the Pact's MiG-23 regiments (20 MLs and 10 MLDs, not
30 MLDs), the neutral's escorts (the Saab 105; the Draken only from 1988) and the A-50, which no longer cuts our fighter
threat in 1985. `air_war_balance` gained an "Air battle" line: fights a campaign, the enemy's aircraft our fighters shot
down, ours lost. Against §14's runs (the same code without B8; district 20 seeds, nation 10, 7 days; base → B8). B8 was
run on track-b before B6 (targets of opportunity) was merged into it; B6 finds columns so rarely at the abstract level
(the neutral alone, a few a campaign) that these rows should hold after the merge; they were not rerun on it:

| District | Packages | Sorties a week | Lost | Loss % | Defences working D3, D7 | Battery raids | Air battle: fights, theirs down, ours lost |
|---|---|---|---|---|---|---|---|
| Rebels, careless | 13.1 → 12.8 | 20.6 → 20.2 | 0.2 → 0.5 | 1.2% → 2.2% | 100%, 100% → 100%, 100% | 1.6 → 1.6 | 0.1, 0.1, 0.0 |
| Rebels, careful | 13.9 → 13.7 | 22.2 → 21.8 | 0.5 → 0.7 | 2.3% → 3.2% | 100%, 100% → 100%, 100% | 0.8 → 0.8 | 0.2, 0.2, 0.0 |
| Neutral, careless | 19.5 → 17.6 | 45.9 → 40.0 | 1.6 → 1.1 | 3.4% → 2.9% | 100%, 100% → 100%, 100% | 8.9 → 8.3 | 0.5, 0.2, 0.1 |
| Neutral, careful | 17.1 → 15.4 | 38.1 → 34.4 | 0.8 → 1.2 | 2.0% → 3.6% | 100%, 100% → 100%, 100% | 5.0 → 3.4 | 0.5, 0.5, 0.1 |
| Warsaw Pact, careless | 97.8 → 96.4 | 568.5 → 563.0 | 9.3 → 10.4 | 1.6% → 1.9% | 31%, 80% → 37%, 73% | 7.2 → 7.5 | 6.5, 3.8, 3.1 |
| Warsaw Pact, careful | 99.2 → 94.4 | 578.3 → 567.1 | 7.8 → 11.5 | 1.4% → 2.0% | 44%, 78% → 43%, 81% | 8.2 → 8.8 | 6.1, 4.3, 2.5 |
| NATO, careless | 94.7 → 94.8 | 528.0 → 533.7 | 5.2 → 8.4 | 1.0% → 1.6% | 39%, 54% → 35%, 60% | 5.4 → 5.2 | 5.8, 4.1, 4.8 |
| NATO, careful | 95.2 → 94.5 | 533.7 → 524.0 | 4.7 → 8.2 | 0.9% → 1.6% | 49%, 62% → 47%, 67% | 6.9 → 7.2 | 5.5, 4.0, 4.3 |

| Nation | Packages | Sorties a week | Lost | Loss % | Defences working D3, D7 | Battery raids | Air battle: fights, theirs down, ours lost |
|---|---|---|---|---|---|---|---|
| Rebels, careless | 10.1 → 10.6 | 14.9 → 15.8 | 1.6 → 1.7 | 10.7% → 10.8% | 100%, 100% → 100%, 100% | 7.3 → 7.6 | 0.2, 0.1, 0.0 |
| Rebels, careful | 10.2 → 10.2 | 15.3 → 15.2 | 2.1 → 2.1 | 13.7% → 13.8% | 100%, 100% → 100%, 100% | 2.6 → 2.5 | 0, 0, 0 |
| Neutral, careless | 16.7 → 15.6 | 28.2 → 25.5 | 0.9 → 0.8 | 3.2% → 3.1% | 100%, 100% → 100%, 100% | 8.5 → 7.3 | 0.4, 0.1, 0.0 |
| Neutral, careful | 14.0 → 12.7 | 23.7 → 21.4 | 0.8 → 1.0 | 3.4% → 4.7% | 100%, 100% → 100%, 100% | 4.0 → 3.6 | 0.5, 0.4, 0.0 |
| Warsaw Pact, careless | 183.2 → 171.3 | 989.9 → 996.7 | 30.1 → 35.8 | 3.0% → 3.6% | 37%, 66% → 44%, 71% | 8.4 → 10.8 | 11.6, 8.0, 5.1 |
| Warsaw Pact, careful | 189.1 → 164.6 | 1016.7 → 971.8 | 28.2 → 36.6 | 2.8% → 3.8% | 41%, 69% → 45%, 70% | 8.9 → 10.8 | 12.8, 10.4, 5.7 |

The base run of the nation stopped after the Pact's campaigns (the `MakeColumns` fault §14 fixed was still in the base
binary), so NATO's nation rows are compared with §14's own: careless 129.7 → 124.9 packages, 725.9 → 726.8 sorties,
15.0 → 18.5 lost (2.1 % → 2.5 %), 9.6 fights, 5.9 down, 9.0 of ours lost; careful 128.7 → 128.3, 721.3 → 739.3,
14.5 → 17.3 lost (2.0 % → 2.3 %), 7.4 fights, 5.2 down, 6.2 of ours lost.

**What moved, and why.**

- **Our fighters kill more, and lose aircraft.** The flat roll killed a strike aircraft at its fighter leg with a chance
  of the front's threat (0.15-0.4) × 0.03, a third of that with an escort, half at night: about one or two aircraft a
  week for the Pact and NATO on the district. The air battle shoots down 3.8-4.3 a week of the Pact's and 4.0-4.1 of
  NATO's on the district, 8-10 and 5-6 a campaign on the nation; it costs us 2.5-3.1 and 4.3-4.8 on the district, 5-6
  and 6-9 on the nation. So the Pact and NATO lose 1-4 more aircraft a week, and their loss rates rise by 0.3-0.8 points
  (the Pact 1.4-1.6 % → 1.9-2.0 % on the district, 2.8-3.0 % → 3.6-3.8 % on the nation; NATO 0.9-1.0 % → 1.6 %, and
  2.0-2.1 % → 2.3-2.5 % on the nation). The district's rates stay at the period's 2 % a sortie or under it; the nation's
  were above it before (§8) and stay the highest.
- **The exchange.** Against the Pact our fighters come out ahead, 1.2-1.8 of theirs for one of ours: most of the
  Pact's losses are strikers our pairs reach, while its MiG-23MLs beat our MiG-21MFs one against one (2.8:1 in
  `test_air_fight`) and its clearing group and close escort meet us first 60 % and 50 % of the time. Against NATO we lose more than
  we kill, 0.7-0.9 of theirs for one of ours: NATO sends sweeps and CAPs, not close escort, its E-3A controls them and
  warns its strikers (the meet chance ×1.5, the strikers warned 90 % of the time against 20 % by eye), and its F-15
  sweep shoots our MiG-21s down at 4.8:1 alone. That is the period's order: Linebacker's 2:1 for the USAF against
  MiG-21s with Teaball's warning, ACEVAL near parity without it, the Bekaa far beyond with an E-2C and the Syrian GCI
  jammed; ours, a Pact-style air force of MiG-21s with a third of its regiments' MiG-23MLs, against NATO's F-15s and
  E-3A, should lose the fighter-on-fighter fight and make the strikers pay.
- **The Pact plans for our fighters.** With the A-50 out of 1985 its planner no longer halves our fighter threat
  (`support.awacs_fighter_cut`), so it escorts more and gathers its strikers into bigger groups: on the nation 6.5-6.6 →
  7.0-7.1 attackers a package, 30-32 % → 36-37 % of eight or more, and 171-165 packages a campaign against 183-189 with
  the same sorties (990-1017 → 972-997). DEAD falls from 27-29 % of the mix to 25 %, helicopter raids rise from 10-12 %
  to 15-16 %. On the district the Pact's packages move 97.8-99.2 → 94.4-96.4.
- **The neutral flies less.** Its Draken is out of service in 1985, so it fields fewer jets: 45.9-38.1 → 40.0-34.4
  sorties a week on the district, 28.2-23.7 → 25.5-21.4 on the nation; its losses move by what one aircraft is worth
  (0.8-1.6 → 1.1-1.2).
- **The rebels** have no fighters to send and almost nothing changes. A fight now and then (0.1-0.2 a week) is a light
  aircraft our pair catches; their loss rate moves 1.2-2.3 % → 2.2-3.2 % on the district, within what one aircraft is
  worth in a dozen packages (§13).
- **A reshuffle.** The flat roll drew from the package's stream for every strike aircraft even at no threat; the air
  battle forks its own stream, so every package after the first fight takes another, equally likely course. The battery
  raids (7.2-8.2 → 7.5-8.8 for the Pact on the district, 8.4-8.9 → 10.8 on the nation) and the defences working move
  within the seeds' noise.

`air_war_bench data 400` has no fighters in its cases; only the reshuffle moves it, on the rows that §13 and §14 named:
JP233 Tornados against medium-altitude F-16s 5.5 against 2.0 → 6.2 against 1.9 losses a 100 sorties, the runways cut
52 % → 54 %, ARMs for each radar destroyed 3.8 → 3.5 against 12.8 → 12.6, the Thanh Hoa analogue's span down in 29 % →
28 % of campaigns, the F-4Es popping up behind the Kub belt 25.8 → 25.2, the Bekaa package destroying the battery in
65 % → 62 % with 6.3 → 6.5 lost a 100 sorties. `crew_balance data 10`, `helper_coverage` (its page byte for byte),
`ground_check data 3600` and `em_bench` give the base's results.

**No rebalance is needed.** The shapes of §2 hold, the district stays within the period's 2 % a sortie, and the
fighters' share of the enemy's losses (a third to a half on the district) is new and wanted: our fighters now count.

**Cost.** The abstract fight runs once a package and only where our fighters find it: a few dozen keyed
draws and chain products, nothing per step. `air_war_balance data 5 7`, the base on its own data and B8 back to back on a
quiet machine, twice: 76.9 s and 70.7 s for the base, 78.0 s and 70.7 s for B8, the same within the noise.
`crew_balance data 3` back to back: 20.2 s and 20.1 s (the runner's `UpdateAirFight` returns at once in a scenario
without `"air_fight"`). The full runs above were made while another worktree compiled with thirteen compilers and this
one built at `-j 2`, so their wall times (the district 5m03 → 5m12, `crew_balance` 1m15 → 8m48, `helper_coverage`
16m → 53m, `ground_check` 4m → 5m) measure the load, not B8; the base's nation run stopped before NATO's campaigns, so
its 10m49 against 19m39 is not a comparison either. `em_bench` 11.4 s → 10.9 s. The live fight is not in any raid yet
(B10); in `test_air_fight` a two-aircraft session of 155 s steps it at the runner's 10 Hz with no measurable cost.

## 16. Faction profiles: NATO low until the defences break, interdiction from day 1 (track B, B11, 18 November 2026)

B11 (`docs/air_battle/10_FACTIONS.md` §12) moved each faction's hot-phase values into a sourced `hot_phase` block. The
abstract resolver does not read it, so on its own it changes no number. Two data choices that were not 1985 practice
were changed with it (`10_FACTIONS.md` §9):

- **§9.1.**
  - NATO's strike, close support and armed reconnaissance templates have `medium_below_iads` 0.45. The planner offers them medium altitude only once the believed working defences are below the rollback's line.
  - `medium_altitude_man` is rare in a superior force (1.3 → 0.1).
  - Halloran is a `low_level_purist`.
- **§9.2.** NATO's first phase is "counter-air and suppression, with interdiction from the first day":

| Mission | dead | hunter | deception | recce | strike | bda | cas | missile | armed_recce |
|---|---|---|---|---|---|---|---|---|---|
| Before | 0.25 | 0.15 | 0.10 | 0.15 | 0.22 | 0.05 | 0.05 | 0.06 | 0.03 |
| B11 | 0.22 | 0.12 | 0.08 | 0.11 | 0.25 | 0.03 | 0.08 | 0.05 | 0.06 |

**The base** was the B12 build (track-b at 65b3dbd) on a copy of the data as it was. The rows below compare it with B11
(base → B11), on the district over 20 seeds and the nation over 10, 7 days each.

**What stayed byte for byte:**
- the rebels', the neutral's and the Pact's tables, on both theatres;
- `air_war_bench data 400`;
- `crew_balance data 10`;
- `helper_coverage` (its page as committed);
- `ground_check data 3600` (0 of 15 scenarios with anything off the ground; only the wall times moved).

The `hot_phase.mode` switch, the hot quirks' own stream and the per-mode tier weights are what keep the other factions
still.

| NATO | Packages | Sorties a week | Lost | Loss % | D1 SEAD share | D1 loss % | Defences working D3, D7 | Battery raids | Battery wrecked |
|---|---|---|---|---|---|---|---|---|---|
| District, careless | 95.2 → 99.8 | 537 → 547 | 8.8 → 8.2 | 1.6% → 1.5% | 39% → 35% | 2.5% → 2.5% | 32%, 64% → 44%, 53% | 5.2 → 6.0 | 90% → 95% |
| District, careful | 93.8 → 101.2 | 522 → 549 | 9.3 → 8.4 | 1.8% → 1.5% | 39% → 35% | 2.5% → 2.5% | 47%, 63% → 48%, 68% | 7.8 → 7.0 | 0% → 15% |
| Nation, careless | 126.7 → 131.2 | 737 → 765 | 17.1 → 16.3 | 2.3% → 2.1% | 42% → 46% | 3.0% → 2.5% | 74%, 70% → 79%, 80% | 8.6 → 8.3 | 50% → 10% |
| Nation, careful | 126.4 → 131.3 | 732 → 747 | 16.2 → 16.0 | 2.2% → 2.1% | 42% → 46% | 3.0% → 2.5% | 77%, 75% → 80%, 75% | 8.7 → 9.0 | 0% → 0% |

**What moved, and why:**

- **More packages, more interdiction.**
  - Strike, close support and armed reconnaissance share the first phase from day 1. The mix shows it: armed reconnaissance rises from 5-6% to 9% of packages on the district, strike from 24-25% to 27-28% on the nation.
  - Packages rise by 3.5-8% and sorties by 2-5%: interdiction pairs and fours are smaller than the SEAD packages they replace.
  - Armed reconnaissance searches rise from 4.8-6.2 to 6.9-9.5 a campaign, and columns found from 25-40% to 30-51%.
  - On the nation, the line-hours cut in the week rise from 112-123 to 113-132. §9.2 asked for that.
- **The rollback ends later on the district.** Fewer SEAD packages a day leave more of the defences standing: the careless battery's district has 44% working on day 3 against 32%. The phase means show the rollback ending later (day 4 1.6 → 1.4, day 6 1.9 → 1.7). It still ends and resumes: in `test_air_war.cpp`'s ten NATO campaigns of twelve days, it ended 13 times and resumed 9. On the nation it held all week before and still does (§8.4).
- **Day 1's SEAD share stays the highest of the week**, on both theatres (§9.2's test).
  - On the district it falls from 39% to 35%, as the shares say.
  - On the nation it rises from 42% to 46%. A strike that must fly low under standing defences takes more suppression with it (each template's `sead_per_threat` against the threats on a low route).
- **Losses did not rise.**
  - §9.1 expected low strikes into the target guns to cost more, and they hardly did: 1.5% against 1.6-1.8% on the district, 2.1% against 2.2-2.3% on the nation, within what a few aircraft are worth across the seeds.
  - The weaponeer answered the low level as `09_BALANCE.md` §9 would predict, with fewer dives (14.8-15.0 → 7.8-10.1 flights a campaign) and more pop-ups, lofts and stand-off shots (popup 10.8-12.6 → 15.9-18.4, loft 10.1-11.0 → 11.8-12.7, standoff 17.9-20.2 → 21.4-21.6).
  - NATO keeps the lowest loss rate of the four on both theatres. On the district the Pact loses 1.8-2.1%, the rebels 2.7% and the neutral 3.1-3.8%; on the nation the Pact loses 3.7-4.1%.
  - Losses still fall as the rollback bites (§2's shape).
  - NATO's day 1 on the district (2.5%) was above the Pact's careless day 1 (2.3%) before B11 as well, and B11 does not change it. `delivery_risk` was left alone.
- **The battery.**
  - On the district the careful battery is now wrecked in 3 runs of 20, where it was wrecked in none. The interdiction and armed reconnaissance flights from day 1 find it on the roads and in the open more often.
  - On the nation the careless battery is wrecked in 1 run of 10, where it was wrecked in 5. More of NATO's day goes to bridges, junctions and columns, away from the battery.
  - Battery raids move within the noise (5.2-8.7 → 6.0-9.0).
- **The fighters** fight a little less on the nation (8.6-8.7 → 7.9-8.0 a campaign), and the tankers refuel more (10-15 → 28-35 aircraft a campaign): deep low-level interdiction needs fuel.

**No rebalance is needed.** NATO keeps the lowest loss rate, the rollback ends and resumes, day 1 keeps its SEAD
peak, and week 1 cuts more lines.

**Cost.** The planner's gate is one comparison per template and the officers' hot quirks a few draws at
appointment. `HotPhase` is pure and is read once per aircraft when a raid is handed to a session. Nothing runs per step.
The wall times are not comparable: the base set ran while this worktree compiled, B11's while the tests ran.

## 17. The balance pass: the hot phase against history (track B, B14, 18 November 2026)

B14 (`docs/air_battle/11_IMPLEMENTATION_PLAN.md`) turns the track's measures into bands. Each band is a range in
`data/air_war/balance_bands.json` with its anchor: a registered source, or "game value" with the reason. The two
benches read the bands and fail when a measure falls outside one: they print PASS or FAIL on each row, and exit with 2
when anything is out of band (`simcore/air_war/balance_bands.hpp`).

- `air_war_bench` ends with a new block, "The hot phase against history": the strikers' answers to our fighters, the
  1967 missiles, the exchanges, the latencies and friendly fire.
- `air_war_balance` prints three new lines for each faction and posture: the losses by what shot them down (beside
  what the planners expected), the strike flights our fighters reached and how they answered, and the latencies from
  a pilot's sighting to an on-call package over the place and to a diverted flight's new task. It ends with a band
  table for the theatre, the faction and the posture.

Fast checks went into the tests: `test_air_fight.cpp` (the strikers' answers and the warning's swing over 3000 and
4000 abstract fights), `test_c2.cpp` (a Pact divert's new task 10-30 min after the decision) and `test_hot_phase.cpp`
(the bands load with their sources; `RetaskAirS` stays in its band; the save's `balance_mode`).

### 17.1 The bands against history (`air_war_bench data 400`)

| Measure | The model | Band | Anchor |
|---|---|---|---|
| Strike flights attacked by our fighters (abstract fight): jettisoned | 49.3% | 40-65% | `red_baron_vol2` (16 of 37 F-105 flights, 1966), `aces_1976` (107 of 192, 55.7%) |
| the same: pressed on with their stores | 41.1% | 18-55% | `red_baron_vol2` (12 of 37) |
| the same: turned for home | 19.0% (36% before B14) | 10-42% | `red_baron_vol2` (9 of 37) |
| The same in a session (`reactions.hpp`, NATO's row for a fighter seen): jettisoned, pressed on, turned home | 55.9%, 34.4%, 22.3% | the same bands | the same |
| AIM-7E: hits a launch, out of parameters | 11.0%, 35.3% | 9.6-12.6%, 31-39% | `checo_1967_a2a` p. 44 (11.1%, 35%) |
| AIM-9B: hits a launch, out of parameters | 18.8%, 26.0% | 17.1-20.1%, 22-30% | `checo_1967_a2a` p. 44 (18.6%, 26%) |
| Exchange, ours lost for each of theirs: an F-4E strike and escort, no warning | 0.91 | 0.7-3.0 | `mcallister_1985` (2.25:1 in 1965-68, 1:1 in 1968) |
| the same under an E-3A (Teaball's warning) | 2.36 (1.60 before B14) | 2.0-8.0 | `checo_1978_linebacker` (3.8:1 in August-October 1972) |
| the warning's swing (times) | 2.60 (1.76) | 1.5-10 | `checo_1978_linebacker` (0.47:1 in May 1972 to 3.8:1) |
| an F-16A strike with an F-15C sweep, an EF-111A and an E-3A (the Bekaa) | 3.73 (2.92) | 3-1000 | `lambeth_1984` (about 85 to none); the floor a game value |
| the Pact's MiG-23ML sweep | 1.10 | 0.5-4.0 | game value (`07_AIR_TO_AIR.md`) |
| Latency: the Pact's airborne flight retasked | 20.0 min | 10-30 min | `mt_1962_kontsevoy`, `fm_100_2_1_1984` p. 12-7, SAM3D estimate |
| NATO's close support retasked in the air | 6.5 min | 3-10 min | SAM3D estimate (`10_FACTIONS.md` §7.4) |
| the Pact's readiness 1 to wheels up | 4.0 min | 3-5 min | `fm_100_2_1_1984` p. 12-7 |
| Friendly fire in an exercise, ours among those fired on: notices only (the 1960s) | 22.5% | 12-30% | `mt_turchin_coop` pp. 11-12 (up to 25%) |
| the same with notices and the day's codes (1985) | 0.4% | 0-6% | game value (`08_OUR_FIGHTERS.md` §8) |

How the rows are measured:

- **The strikers' answers.** Two flights of four F-4Es on a medium strike meet our pair of MiG-21s by day, 8000 times.
  "Pressed on" is a flight that neither jettisoned nor turned home. The F-105 records count flights attacked by MiGs,
  not flights that only saw them, so the bench counts only the flights our fighters actually attacked. The session's
  row draws NATO's reaction row for a fighter seen by eye (`DecideFlight`), with the flight's nerve between 0.3 and 0.7.
- **The exchanges** are the abstract fight's own (two passes, the warning, the escort's screen), at 2000 fights a case,
  counted as the US counted them: our losses for each of theirs. The bands are wide because the analogue is flown with
  1985's missiles (AIM-7F, AIM-9L), which hit far more often than 1968's.
- **The latencies** are the means of `RetaskAirS` and `AlertTakeoffS` over a grid of draws and officers. The order's
  own hops (report, decision, order) are printed beside them for reference: 2.2 min for the Pact and 2.8 for NATO.
- **Friendly fire** is a small Monte Carlo of an exercise: 400 tracks, 35% of them ours, each notice sent 15-75 min
  before the crossing (game values). A notice reaches the battery by `RunNotice` (the chain from the fighter
  regiment's duty officer to the regiment's post, about 43 min), and the crew judges each track by `JudgeTrack`:
  in the 1960s by the notice alone, in 1985 by the notice and the day's codes (`IffSystem::Kremniy`).

### 17.2 The bands of the campaign (`air_war_balance`)

| Band | District | Nation | Anchor |
|---|---|---|---|
| Aircraft lost a sortie: the rebels | 0-6% | 0-20% | §2; the nation's band argued below (item 1) |
| the neutral | 0-8% | 0-8% | §2, §5, §8 |
| the Pact | 1-4% | 1.5-4.5% | §5, §8, §14 |
| NATO | 0.5-3% | 1-3.5% | §2, §5 (the lowest rate of the four), §8 |
| Runs with the battery wrecked: careless (Pact, NATO) | 50-100% | - | §5 (the lesson the campaign teaches) |
| careful | 0-40% | 0-40% | §5 |
| The air battle, theirs lost for each of ours: the Pact | 0.8-2.5 | 0.8-2.5 | game value (`07_AIR_TO_AIR.md`, §15) |
| NATO | 0.25-1.2 | 0.25-1.2 | game value; `checo_1978_linebacker`, `lambeth_1984`: the bench's cases turned round, from the Bekaa (1/3.7) to 1968 unwarned (1/0.9) |
| On call, the sighting to over the place: the Pact | 15-45 min | 15-45 min | `mt_1962_kontsevoy` (20-30 min, 17-28 in the exercise), widened |
| NATO | 15-60 min | 15-60 min | `luke_2002_cas` (40-50 min in Korea, the goal 15-20) |
| A divert, the sighting to the new task: the Pact | 10-40 min | 10-40 min | SAM3D estimate (`10_FACTIONS.md` §7.4, plus the report's way) |

A band is checked only where the measure is worth reading: the exchange with at least 10 aircraft down, the on-call
latency with at least 5 packages and the divert with at least 3 diverts.

### 17.3 The items B14 settled

1. **The rebels' losses on the nation (14-17% a sortie; B2's Left).** The losses come from B2's route planner, not from
   a fault: with `route.mode` 0 they are 4.1-5.7%. They run at 2.0-2.4 aircraft a week. Most are single low
   reconnaissance aircraft by day over our troops (the motor rifle division's artillery, group south, the tank
   regiment's area). They are shot down by the Osa batteries guarding those troops (battery 21, battery 24 and the
   regiment's others) and by the S-125 at Paksany. The rebels' intelligence has no ELINT and a weak analyst, so it
   never places the mobile batteries. Their planner prices those flights at 0.4 losses a week; they lose 1.1-1.5.
   Reconnaissance makes up 52-55% of their packages, against an apportionment of 15%, because their other missions
   find few targets they can reach. Few sorties and two losses a week give a high share. That is a fair picture of a
   rebel air arm flying blind into a division's air defence, so the band on the nation is 0-20%, a game value. The
   district keeps 0-6%. **Left:** the rebels' intelligence never places the mobile batteries, and the planner does not
   learn a threat from losses it cannot explain.
2. **NATO's day 1 against the Pact's, and `delivery_risk`.** NATO's day 1 costs 2.6-2.7% a sortie against the Pact's
   2.3-2.7%, inside the noise of about ±0.45 points over 20 seeds. Its losses that day go to our fighters (0.7 aircraft
   a campaign), the SAMs (0.5) and the player's battery (0.3); the target's guns take only 0.1. `delivery_risk` (the
   price of a low delivery into the target's guns) cannot move what it does not cause: at 0.8 day 1 cost 2.3-2.5%, no
   different. It stays at 0.4.
3. **The retask latency: B5's hops against B11's `retask_air`.** B5's divert composed the report's and the order's
   hops (160-490 s for the Pact), well under the doctrine's 10-30 min for an airborne flight. Under `balance.mode` 1 a
   divert now waits for `RetaskAirS` (the faction, the mission, the officer, from a stream keyed on the message id,
   not a new keyed stream): the order is sent at once but the flight has its new task no earlier than the decision
   plus the retask delay (`C2Message::not_before_h`). The test in `test_c2.cpp` diverts a Pact armed reconnaissance
   flight onto a reported column: under 6 min in mode 0, inside the band in mode 1. In the campaigns no divert happened
   at all (the Pact's only divertable line is armed reconnaissance within its own area at a 0.2 chance, and NATO had no
   in-flight report to divert on), so the campaign rows of `divert_min` stay unread. **Left:** diverts are too rare
   in the campaign to measure there.
4. **B4's two outcome-changing savings (B12's Left).** Declined. They are the defensive perception rate capped at the
   threat rate (about 7 s in the testbed's hour) and the warning receivers' period at 0.4 s instead of 0.2 s (about
   25 s). Both act inside the play circle, where the player watches, and change what a pilot knows and when. B12's
   levels of detail already carry the cost outside it. Both stay as data (`perception.json`'s `defensive` rate, the
   EM library's ESM period) should a later pass need them.
5. **B6's order of attacks on unbriefed targets.** The data keep the design's order for each encounter
   (`hot_phase.opportunity`: rebels 0.7, NATO 0.5, the neutral 0.4, the Pact 0.2). The counts a campaign come out
   NATO, the Pact, the neutral, the rebels, because of exposure: the rebels fly few gun or rocket aircraft near our
   lines. That is right for 1985, and nothing changed.
6. **B11's careful battery wrecked in 3 runs of 20 against NATO.** Now 2 of 20 against NATO and 3 of 20 against the
   Pact, inside the careful band of 0-40%. (The first pass in §5 gave 15-40% for a careful battery; 0-40% keeps a
   battery that never meets its raid as a pass.)
7. **B8's exchange ratios in a band.** In the campaigns the Pact's sit at 1.42-1.88 of theirs for each of ours and
   NATO's at 0.32-0.68 (the band's floor moved, 17.5). The bench's Teaball case was out of band: the E-3A's warning lifted the exchange only
   from 0.91 to 1.60 against the record's 0.47:1 to 3.8:1. Two changes under `balance.mode`, both in
   `air_war/air_fight.hpp`:
   - A package under an E-3A is no longer surprised when the controller sees our fighters coming
     (`a0_seen_awacs`): Teaball's whole point was the warning.
   - The E-3A's controller brings the package's fighters to the fight more often: `a0_awacs_meet` 1.5 → 2.0
     (`a0_awacs_meet_mode0` keeps 1.5 for old saves).
   The warned case is now 2.36 and the swing 2.60. The Bekaa case rose from 2.92 to 3.73. Its floor was lowered from
   6 to 3, a game value: the abstract fight's two passes against our pilots of 1985 carry neither the destruction of
   the Syrians' ground control with their SAMs nor the Syrians' training, so the band asks only that a sweep, a jammer
   and an E-3A do better than the warned 1972 package's floor.
8. **The F-105 and CHECO rows** are in the bench (17.1). One more change: the abstract fight's strikers answered by
   reading `strikers_abort` as a share of the strikers that did not press on, which turned NATO home 36% of the time
   against the record's 24%. Under `balance.mode` 1 the abort is drawn at its own rate (19%).

### 17.4 What changed, under `balance.mode`

All four changes are behind `tuning["balance.mode"]` (1 in `data/gameplay/air_war.json`'s `balance` group). A save written before B14 has no `balance_mode` and
loads with 0, the old behaviour: B5's composed divert, the old abort reading, no surprise denied, `a0_awacs_meet` 1.5.

| Change | Where | Effect |
|---|---|---|
| A divert waits for the retask delay | `air_war/c2.hpp` (`Divert`, `Send`) | an airborne Pact flight has its new task 10-30 min after the decision, not 3-8 |
| The strikers' abort at its own rate | `air_war/air_fight.hpp` | NATO's attacked flights turn home 19% of the time, not 36% |
| No surprise under an E-3A that sees our fighters | `air_war/air_fight.hpp` | the warned exchange 1.6 → 2.4 |
| `a0_awacs_meet` 1.5 → 2.0 | `data/air_war/air_combat.json` | the same; the Bekaa case 2.9 → 3.7 |

For the counters only (not saved): `PackageOutcome` counts the flights our fighters attacked, the jettisons and the
aborts; `C2Stats` keeps each divert's and each on-call package's latency; `AirCampaign::ObserveOutcomes` lets a bench
see each package's outcome as it is applied.

### 17.5 Balance before and after

The base is track-b at ef9952b on a copy of the data as it was; the after is this branch. District 20 seeds, nation
10, 7 days each (base → after).

| District | Loss % | Battery wrecked | Exchange, theirs for ours | On call (min) |
|---|---|---|---|---|
| Rebels, careless | 2.7% → 2.7% | 0% → 0% | - | - |
| Rebels, careful | 2.7% → 2.7% | 0% → 0% | - | - |
| Neutral, careless | 3.1% → 3.1% | 0% → 0% | - | - |
| Neutral, careful | 3.8% → 3.8% | 0% → 0% | - | - |
| Warsaw Pact, careless | 1.8% → 1.8% | 70% → 75% | 1.40 → 1.42 | 23.8 |
| Warsaw Pact, careful | 2.1% → 2.0% | 15% → 15% | 1.60 → 1.73 | 23.8 |
| NATO, careless | 1.5% → 1.6% | 95% → 90% | 0.80 → 0.68 | 23.1 |
| NATO, careful | 1.5% → 1.5% | 15% → 10% | 0.80 → 0.56 | 22.8 |

| Nation | Loss % | Battery wrecked | Exchange, theirs for ours | On call (min) |
|---|---|---|---|---|
| Rebels, careless | 16.9% → 16.9% | 0% → 0% | - | - |
| Rebels, careful | 13.9% → 13.9% | 0% → 0% | - | - |
| Neutral, careless | 4.8% → 4.8% | 0% → 0% | - | - |
| Neutral, careful | 6.4% → 6.4% | 0% → 0% | - | - |
| Warsaw Pact, careless | 4.1% → 4.1% | 60% → 60% | 1.40 → 1.51 | 27.8 |
| Warsaw Pact, careful | 3.7% → 3.6% | 10% → 10% | 1.93 → 1.88 | 29.7 |
| NATO, careless | 2.1% → 1.9% | 10% → 10% | 0.49 → 0.42 | 20.6 |
| NATO, careful | 2.1% → 2.2% | 0% → 10% | 0.57 → 0.32 | 20.3 |

What moved:

- **The rebels and the neutral did not move** on either theatre: they fly few fights under an E-3A and few diverts,
  and the same seeds give the same campaigns.
- **NATO's exchange fell**, on the district from 0.8 to 0.56-0.68 of theirs for each of ours and on the nation from
  0.49-0.57 to 0.32-0.42: with no surprise under the E-3A and its fighters brought to the fight more often, NATO's
  escorts and sweeps win more of the fights. This is the swing of 1972 the bench asks for. The nation's careful run
  (2.3 of theirs down for 7.2 of ours) fell below the first band's floor of 0.4, which was set before the bench's
  cases existed. The band now runs from the Bekaa case turned round (1/3.7, about 0.27) to the unwarned 1968 package
  (1/0.9), floor 0.25: NATO's campaign mixes sweeps under an E-3A with strikes that fly without one, so it should
  fall between the two.
- **The Pact's exchange rose a little** (1.40-1.60 → 1.42-1.73): its strikers turn home less often under the new
  abort reading and stay in the fight to be shot at.
- **Losses a sortie and the battery's fate** moved inside the noise of a few aircraft and one or two runs of 20.
- **The latencies** sit in their bands: an on-call package is over the place 23-24 min after the pilot's sighting
  for both alliances.

`crew_balance data 10`, `helper_coverage` (its page as committed), `ground_check data 3600` (0 of 15 scenarios with
anything off the ground), `em_bench data 500 60 24` and `air_battle_bench` give the same outcomes as the base; only the
wall times moved (the base set ran beside a build). The old rows of `air_war_bench data 400` are unchanged.

**Cost.** One more comparison and at most one draw per attacked flight, per fight under an E-3A and per divert.
Nothing runs per step.

**Left:**
- The rebels' intelligence placing mobile batteries, and the planner learning from losses it cannot explain (item 1).
- Diverts too rare in the campaigns to read their latency there (item 3); the `divert_min` band waits for them.
- The exchange bands are wide: the abstract fight is calibrated to 1967-72 rates and flown with 1985 missiles.
- B13 (Unreal) is the owner's; the hot phase's look will need its own pass once it is drawn.

## 18. Attack passes, reactions and damage: before and after (roadmap S4, 19 November 2026)

S4 (`docs/air_battle/06_OPPORTUNITY.md` §10, `04_REACTIONS.md` §6 and §13, `02_FLIGHTS_AND_PILOTS.md` §3,
`docs/DAMAGE.md`) changes what a session flies (passes in their dive, the climb as a path angle, top speed and
acceleration by altitude, damaged aircraft cut and escorted) and one thing in the campaign: a damaged aircraft comes back
into service by its level (`damage.mode` 1 in `data/gameplay/air_war.json`, saved with the campaign; a save without the
key replays with 0, as before). Targets of opportunity may now be dive-bombed in a session (`opportunity.mode` 2; a save
with 1 replays without the bombs). The benches were run on the branch's base and on S4, the same seeds.

`air_war_balance data 20 7` (the district), every band PASS before and after:

| Run | Sorties | Loss % | Battery wrecked | Exchange (air battle) |
|---|---|---|---|---|
| Rebels, careless | 20.6 -> 20.6 | 2.7% -> 2.7% | 0% -> 0% | - |
| Rebels, careful | 22.0 -> 22.0 | 2.7% -> 2.7% | 0% -> 0% | - |
| Neutral, careless | 39.9 -> 39.9 | 3.1% -> 3.1% | 0% -> 0% | - |
| Neutral, careful | 35.6 -> 35.6 | 3.8% -> 3.8% | 0% -> 0% | - |
| Warsaw Pact, careless | 568.5 -> 567.8 | 1.9% -> 1.8% | 70% -> 70% | 1.53 -> 1.54 |
| Warsaw Pact, careful | 562.0 -> 560.5 | 2.1% -> 2.1% | 15% -> 15% | 1.77 -> 1.75 |
| NATO, careless | 539.0 -> 538.1 | 1.7% -> 1.7% | 90% -> 90% | 0.73 -> 0.70 |
| NATO, careful | 536.4 -> 536.2 | 1.5% -> 1.5% | 20% -> 20% | 0.71 -> 0.70 |

`air_war_balance --theatre theatre_pannonia_nation.json data 10 7` (the nation), every band PASS before and after:

| Run | Sorties | Loss % | Battery wrecked | Exchange (air battle) |
|---|---|---|---|---|
| Rebels, careless | 14.2 -> 14.2 | 16.9% -> 16.9% | 0% -> 0% | - |
| Rebels, careful | 14.4 -> 14.4 | 13.9% -> 13.9% | 0% -> 0% | - |
| Neutral, careless | 25.0 -> 25.0 | 4.8% -> 4.8% | 0% -> 0% | - |
| Neutral, careful | 20.3 -> 20.3 | 6.4% -> 6.4% | 0% -> 0% | - |
| Warsaw Pact, careless | 975.4 -> 971.9 | 4.2% -> 4.3% | 70% -> 70% | 1.56 -> 1.51 |
| Warsaw Pact, careful | 964.0 -> 964.2 | 3.6% -> 3.6% | 10% -> 10% | 1.96 -> 2.02 |
| NATO, careless | 761.3 -> 762.5 | 2.0% -> 2.1% | 20% -> 20% | 0.49 -> 0.57 |
| NATO, careful | 760.9 -> 759.0 | 2.1% -> 2.3% | 10% -> 10% | 0.36 -> 0.49 |

What moved, and why:

- **The rebels and the neutral did not move**: they have few damaged aircraft, and those few came back as before.
- **The Pact and NATO moved by a few sorties and a tenth of a point of loss.** A heavily damaged aircraft is now out
  three times its faction's `damaged_return_h` (`damage.heavy_repair_factor`), a moderately damaged one as before, so
  a few fewer sorties are flown later in the week; the abstract resolver draws a damaged aircraft heavy at
  `damage.heavy_share` (0.3) by a keyed draw. The aircraft that would have flown change which fights happen after them,
  which moves the exchange of the air battle (NATO's on the nation 0.36-0.49 → 0.49-0.57, inside its band of 0.25-1.2;
  the Pact's 1.51-2.02, inside its band of 0.8-2.5). The battery's fate did not change in any run.

The other benches:

- `air_war_bench data 400`: the same rows, before and after.
- `crew_balance data 10`: the same before and after, and the same again with the late window off (its targets do not
  manoeuvre).
- `air_battle_bench data --compare 600`: identical (the testbed off against off, off against on, the same launches and
  level changes).
- `ground_check data 3600`: PASS, 0 of 15 scenarios with anything off the ground, before and after (the passes' dives
  keep `kPassFloorM`, 30 m, and the ground floor).
- `helper_coverage`: 19 gaps before and after, the same ones; only `dynamic_campaign_test`'s rows moved (its waves'
  aircraft fly passes, cut limits and escorts now, so the session's targets differ), its page regenerated.

**After merging track-br** (S1, our fighters in full, and the stable sorts) the two theatres were run again: every band
PASS. The moves from S4's numbers above come with S1 (our pairs in the played raids, its own changes to
the air battle) meeting S4: on the district the Pact's exchange 1.54-1.75 → 1.61-1.71 and NATO's
0.70 → 0.67-0.69, the careful NATO run's battery wrecked in 25% of runs (20%); on the nation the careless Pact run's
battery wrecked in 90% (70%), its exchange 1.51-2.02 → 1.60-2.10, NATO's 0.49-0.57 → 0.51-0.57. Losses a sortie
moved by at most 0.2 points.

**The late window (`manoeuvre_window_s`).** S4 turns it on in the Osa's tuning at 8 s (`data/gameplay/osa_ak.json`). The
14 scenarios with targets that may break (the range days and battles, `osa_decoys`, `osa_low_level_em`,
`osa_soj_strike`, `osa_jamming_em`, `kesmark_range_test` and the six `air_war_*` samples) were played by `scenario_demo`
with the window at 8 s and at 0 (the whole flight, as before):

| Scenario | Shots | Hits | Summed chance, window 0 → 8 s |
|---|---|---|---|
| `osa_jamming_em` | 1 | 1 | 0.25 → 0.41 (a 10 deg/s break finished before the missile closed) |
| `osa_angola_night` | 4 | 2 | 2.07 → 2.14 (one shot's break 7 → 4 deg/s in the window) |
| the other twelve | 13 | 5 | 3.62 → 3.62 |
| all | 18 | 8 | 5.94 → 6.17 |

No hit changed. `manoeuvre_penalty_per_deg_s` stays 0.04: the rise is the window's intent (a break flown on the launch
and over long before the missile arrives gains little, `checo_1967_tactics` p. 31) and is a few per cent of the summed
chance, inside the calibrated totals' spread (§9, roadmap 6.7).

**Left:** the abstract resolver's damaged aircraft fly no escorts and leak no fuel (only a session's do); the share of
damaged aircraft escorted home (`escort_share` 0.5) has no source.

## 19. The tuned theatre by the sun (roadmap S8, 19 November 2026)

S8 gave `theatre_pannonia.json`, the theatre the tests and §3-§11 were tuned on, the war's date: 15 October 1985, UTC+1,
the same parts of the day as the nation's (§12's table). Its date is read only under the air war's save mode
`light.mode` 1 (`data/gameplay/air_war.json`, `docs/LIGHT.md` §5); under mode 0 the theatre is undated and its parts
of the day are the buckets, as before. A campaign saved before S8 carries no `"light_mode"` and replays with mode 0,
so `test_routes.cpp`'s fingerprint (journal 107, FNV 1375944258174313379, 41 packages) holds. Under mode 1 every
theatre's light thresholds are `light.json`'s rather than the code's own; the values are equal today.

`air_war_balance --set light.mode=0 data 20 7` against `air_war_balance data 20 7` (mode 1, the default):

| Adversary, battery | Sorties a week | Loss % | Defences working D3, D7 | Battery raids a week |
|---|---|---|---|---|
| Rebels, careless | 20.6 → 20.2 | 2.7% → 1.7% | 100%, 100% | 1.6 → 1.6 |
| Rebels, careful | 22.0 → 21.3 | 2.7% → 2.3% | 100%, 100% | 0.8 → 0.9 |
| Neutral, careless | 39.8 → 41.2 | 3.1% → 2.8% | 100%, 100% | 8.8 → 8.2 |
| Neutral, careful | 35.9 → 38.0 | 3.8% → 2.2% | 100%, 100% | 3.8 → 3.6 |
| Warsaw Pact, careless | 567.6 → 558.2 | 1.9% → 2.0% | 35%, 75% → 29%, 78% | 8.2 → 7.2 |
| Warsaw Pact, careful | 562.0 → 558.0 | 2.0% → 1.8% | 42%, 83% → 41%, 82% | 9.0 → 9.8 |
| NATO, careless | 537.0 → 537.0 | 1.7% → 1.3% | 43%, 56% → 40%, 55% | 5.8 → 6.5 |
| NATO, careful | 536.4 → 535.9 | 1.5% → 1.3% | 44%, 60% → 48%, 60% | 7.7 → 8.1 |

Every band row passes under both modes. The shapes hold, and the shifts are of the size §12 found on the nation:
within twenty seeds' noise for the light air arms (a few tenths of an aircraft a week), a few points of the defences
on day 3 for the two large air forces, and a raid a week more or less at the battery. The losses fall a little almost
everywhere: the dusk and night by the sun begin some three hours earlier than the buckets' (16:13 and 17:25 against
18:00 and 21:00); the likely reason is that more of the raids planned for the dark now fly in it, not measured.

**No rebalance is needed.** The seven sample raids of the missions menu were regenerated from the dated theatre
(`docs/LIGHT.md` §5; the command in each file's notes).

## 20. The air fight in detail: before and after (roadmap S3, 19 November 2026)

S3 (`docs/air_battle/07_AIR_TO_AIR.md` §9) changes the live fight (fighter radars as emitters with their look-down,
authority lines for vectors, the beam, slash, turn and drag, flown rounds that run out of energy, the strikers' chaff
and flares, the escort's station, the far fight) behind the scenario's `air_fight` `"mode"` (1; a played session's
outcome is saved, so no replay moves), and the abstract fight (A0) behind the tuning's `air_fight.mode` (1 in
`data/gameplay/air_war.json`, saved as `"air_fight_mode"`; a save without it replays B8's A0 with 0). In A0: beam shots
from the beam, the slash's one pass and its share back (`a0_slash_return` 0.4), a turning doctrine's extra round
(`a0_turn_rounds` 1), a radar round from beyond sight dragged at `a0_drag` (0.6) times the skill, and strikers that saw
the shot answering with flares or chaff by `gameplay/reactions.json`. The raid builder writes the package's sweep and
CAP as `sweep` and `cap` (before, as strikers). The benches were run on the branch's base and on S3, the same seeds; the
replay fingerprint (`test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages) is unchanged.

`air_war_balance data 20 7` (the district), every band PASS before and after:

| Run | Sorties | Loss % | Battery wrecked | Air battle: theirs / ours lost | Exchange (band) |
|---|---|---|---|---|---|
| Rebels, careless | 20.6 -> 20.6 | 2.7% -> 2.7% | 0% -> 0% | 0.1/0.0 -> 0.1/0.0 | - |
| Rebels, careful | 22.0 -> 22.0 | 2.7% -> 2.7% | 0% -> 0% | 0.2/0.0 -> 0.2/0.0 | - |
| Neutral, careless | 39.8 -> 39.8 | 3.1% -> 3.1% | 0% -> 0% | 0.3/0.1 -> 0.3/0.1 | - |
| Neutral, careful | 35.9 -> 35.9 | 3.8% -> 3.8% | 0% -> 0% | 0.5/0.1 -> 0.5/0.1 | - |
| Warsaw Pact, careless | 567.6 -> 568.5 | 1.9% -> 1.7% | 70% -> 65% | 4.3/2.7 -> 3.7/2.7 | 1.59 -> 1.37 |
| Warsaw Pact, careful | 562.0 -> 567.5 | 2.0% -> 1.8% | 15% -> 15% | 5.3/3.0 -> 3.9/3.2 | 1.74 -> 1.20 |
| NATO, careless | 538.9 -> 536.5 | 1.7% -> 1.5% | 90% -> 100% | 4.0/5.5 -> 3.1/5.2 | 0.73 -> 0.60 |
| NATO, careful | 536.4 -> 538.1 | 1.5% -> 1.5% | 25% -> 25% | 3.4/4.9 -> 3.4/4.8 | 0.69 -> 0.69 |

`air_war_balance --theatre theatre_pannonia_nation.json data 10 7` (the nation), every band PASS before and after:

| Run | Sorties | Loss % | Battery wrecked | Air battle: theirs / ours lost | Exchange (band) |
|---|---|---|---|---|---|
| Rebels, careless | 14.2 -> 14.2 | 16.9% -> 16.9% | 0% -> 0% | 0.1/0.0 -> 0.1/0.0 | - |
| Rebels, careful | 14.4 -> 14.4 | 13.9% -> 13.9% | 0% -> 0% | 0.2/0.0 -> 0.2/0.0 | - |
| Neutral, careless | 25.0 -> 25.0 | 4.8% -> 4.8% | 0% -> 0% | 0.3/0.0 -> 0.3/0.0 | - |
| Neutral, careful | 20.3 -> 20.3 | 6.4% -> 6.4% | 0% -> 0% | 0.6/0.0 -> 0.6/0.0 | - |
| Warsaw Pact, careless | 978.2 -> 982.6 | 4.0% -> 3.6% | 90% -> 80% | 9.4/5.9 -> 7.7/5.4 | 1.59 -> 1.43 |
| Warsaw Pact, careful | 964.1 -> 956.8 | 3.5% -> 3.6% | 10% -> 0% | 8.9/4.4 -> 8.0/4.0 | 2.02 -> 2.00 |
| NATO, careless | 771.4 -> 763.8 | 2.0% -> 2.3% | 20% -> 10% | 3.4/7.2 -> 4.6/8.2 | 0.47 -> 0.56 |
| NATO, careful | 761.4 -> 749.3 | 2.1% -> 2.2% | 10% -> 10% | 2.6/7.2 -> 3.5/8.3 | 0.36 -> 0.42 |

The rebels and the neutral neighbour do not move: their raids meet few of our fighters and their doctrines turn (no
slash, no beam). Against the Pact the exchange falls (district 1.59-1.74 -> 1.20-1.37, nation 1.59-2.02 -> 1.43-2.00):
its MiG-23MLs slash and extend instead of staying in a fight that our MiG-21s' R-60s and guns win, and drag our R-23Rs
from beyond sight; its losses fall (district 4.3-5.3 -> 3.7-3.9 a campaign) while ours hold. Against NATO on the
nation both sides lose more (theirs 2.6-3.4 -> 3.5-4.6, ours 7.2 -> 8.2-8.3) and the exchange rises (0.36-0.47 ->
0.42-0.56); on the district it stays (0.69) or falls (the careless run's 0.73 -> 0.60). A0's beam shots, drags and the
strikers' decoys move both sides' losses; how much each part gives was not isolated. All stay inside
their bands (0.80-2.50 against the Pact, 0.25-1.20 against NATO). Losses a sortie moved by at most 0.4 points; the
careless NATO run's battery is wrecked in all 20 district runs (90% before), the careless Pact run's in 65% (70%).

`air_combat_bench data 50` (new, `simcore/examples/air_combat_bench.cpp`) flies the live fight's cases in a small world
of its own, B8's beside S3's and the far fight off and on; its table and reading are in `07_AIR_TO_AIR.md` §9.

**Left:** the live fight's exchange is not banded (`air_combat_bench` counts, it does not judge); the bands above are
the abstract fight's.

## 21. The faction values read (roadmap S5, 20 November 2026)

S5 (`docs/air_battle/10_FACTIONS.md` §13) makes new campaigns play `hot_phase.mode` 2. Most of what it adds is read in
a played session: the leader's initiative and route gate, plan adherence, the spotted turn-back, the pilots' delays,
the held jammers, the air fight's turn-back. Three things reach the campaign layer:
- **§9.3's nerve** in the abstract resolver: `0.1 + 0.5 × press_on`, less `husbanding` × the day's losses over
  tolerance, plus the planner's quirk. The leader's own `abort_threshold` acts only through his press-on shift (±0.05 of
  the nerve), no longer adding its separate ±0.15.
- **§8.5's tier weights** for `chatterbox` and `silent_runner`, which change the generated officers.
- **The squadrons' combat sorties**, counted always but read only by the raid builder.

The benches were run on the same build with the old mode (`--set hot_phase.mode=1`) and the new default, the same seeds.

`air_war_balance data 20 7` (the district), mode 1 → mode 2, every band PASS in both:

| Run | Sorties | Lost | Loss % | Battery wrecked | Exchange (air battle) |
|---|---|---|---|---|---|
| Rebels, careless | 20.6 -> 20.6 | 0.6 -> 0.6 | 2.7% -> 2.7% | 0% -> 0% | - |
| Rebels, careful | 22.0 -> 22.0 | 0.6 -> 0.6 | 2.7% -> 2.7% | 0% -> 0% | - |
| Neutral, careless | 39.8 -> 39.8 | 1.2 -> 1.2 | 3.1% -> 3.1% | 0% -> 0% | - |
| Neutral, careful | 35.9 -> 35.9 | 1.4 -> 1.4 | 3.8% -> 3.8% | 0% -> 0% | - |
| Warsaw Pact, careless | 567.0 -> 568.0 | 10.2 -> 10.2 | 1.8% -> 1.8% | 70% -> 70% | 1.61 -> 1.63 |
| Warsaw Pact, careful | 560.5 -> 560.5 | 11.4 -> 11.4 | 2.0% -> 2.0% | 15% -> 15% | 1.71 -> 1.71 |
| NATO, careless | 535.6 -> 535.3 | 8.7 -> 8.7 | 1.6% -> 1.6% | 90% -> 85% | 0.67 -> 0.68 |
| NATO, careful | 536.2 -> 535.8 | 8.1 -> 8.2 | 1.5% -> 1.5% | 25% -> 25% | 0.69 -> 0.67 |

`air_war_balance --theatre theatre_pannonia_nation.json data 10 7` (the nation), mode 1 → mode 2, every band PASS in
both:

| Run | Sorties | Lost | Loss % | Battery wrecked | Exchange (air battle) |
|---|---|---|---|---|---|
| Rebels, careless | 14.2 -> 14.2 | 2.4 -> 2.4 | 16.9% -> 16.9% | 0% -> 0% | - |
| Rebels, careful | 14.4 -> 14.4 | 2.0 -> 2.0 | 13.9% -> 13.9% | 0% -> 0% | - |
| Neutral, careless | 25.0 -> 25.0 | 1.2 -> 1.2 | 4.8% -> 4.8% | 0% -> 0% | - |
| Neutral, careful | 20.3 -> 20.3 | 1.3 -> 1.3 | 6.4% -> 6.4% | 0% -> 0% | - |
| Warsaw Pact, careless | 976.5 -> 971.8 | 39.9 -> 38.9 | 4.1% -> 4.0% | 90% -> 80% | 1.60 -> 1.61 |
| Warsaw Pact, careful | 964.3 -> 965.8 | 34.3 -> 34.2 | 3.6% -> 3.5% | 10% -> 10% | 2.10 -> 2.05 |
| NATO, careless | 772.6 -> 772.2 | 16.3 -> 16.3 | 2.1% -> 2.1% | 20% -> 20% | 0.57 -> 0.58 |
| NATO, careful | 753.8 -> 753.8 | 17.7 -> 17.7 | 2.3% -> 2.3% | 10% -> 10% | 0.51 -> 0.51 |

What moved, and why:

- **The rebels and the neutral did not move in their losses or sorties.** Two things have to meet for §9.3's lower
  nerve to act: a loss before the release, and a loss share between the old nerve (about 0.4) and the new one. The
  neutral's new nerve is 0.3 fresh and falls with the day's losses. But each campaign loses about one aircraft a week, so
  the two never meet: a pair losing one aircraft has lost 0.5 and turns back under either nerve, and a four losing one
  (0.25) goes on under both unless that day's losses have already lowered the new one. The neutral's district runs differ only in its knowledge of the battery on day 5 (0.35 →
  0.36) and the reports sent to intelligence (3.5 → 3.4). Those are the radio's: §8.5's weights change which generated
  officers are chatterboxes or silent runners, and our intercept hears their flights' calls.
- **The Pact and NATO moved by a few sorties and a tenth of a point of loss**, the size of the seeds' spread. An average
  leader of either keeps the old 0.4, so what changes is the spread over leaders:
  - A bold leader no longer gains up to +0.15 from his own `abort_threshold` quirk, only his press-on shift.
  - The generated officers differ where §8.5's weights changed a draw.
  - These move which packages turn back and so which fights follow. On the nation, the careless Pact run's battery was
    wrecked in 80% of runs (90%); the band is 50-100% on the district, and the nation has no band for this run.
- **The suspects the review named are not at work.** The neutral's and rebels' week-1 losses are unchanged. The dropped
  0.15 quirk term moved no band. No value was changed and no band loosened.

`air_battle_bench` and `air_war_bench` were not re-run: S5 changes nothing in the testbed's waves (they carry no `"hot"`
values) or in the bench's analogues (they set their own flights).

**Left:** the neutral's nerve has not yet been seen to act. Seeing it needs a played raid, or a bench of larger neutral
packages under losses before release. Played raids now carry the spotted turn-back and the initiative gate, which the
abstract resolver does not model (§13 of `10_FACTIONS.md`).

## 22. Routes, perception, cost and learning: before and after (roadmap S7, 20 November 2026)

S7 (`docs/air_battle/01_ROUTES.md` "As built (S7)", `03_PERCEPTION.md` 5.10, `02_FLIGHTS_AND_PILOTS.md` 5.4) changes the
campaign behind three switches in `data/gameplay/air_war.json`, each saved with the campaign: `route.mode` 2 (a gate with
no feature joined to its nearest features, our believed airfields in a leg's cost, the coarser lattice without a map,
navigation drift in the raids built, the routes as flown fed back; a save made before S7 wrote 1), `learning.mode` 1 (a
loss nobody can explain becomes a suspected defence; the rebels' sympathisers find our mobile batteries) and
`weather.mode` 1 (the share of days not clear by the month of a dated theatre). A save without `learning_mode` or
`weather_mode` reads 0. The replay fingerprint (`test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages)
is unchanged. "Before" is the branch with `--set route.mode=1 --set learning.mode=0 --set weather.mode=0`, "after" the
data as shipped, the same seeds; the two ran side by side on a shared machine.

`air_war_balance data 20 7` (the district), every band PASS before and after:

| Run | Sorties | Loss % | Battery wrecked | Air battle: theirs / ours lost |
|---|---|---|---|---|
| Rebels, careless | 20.2 -> 19.0 | 1.7% -> 2.9% | 0% -> 0% | 0.1/0.0 -> 0.0/0.0 |
| Rebels, careful | 21.3 -> 20.5 | 2.3% -> 2.2% | 0% -> 0% | 0.2/0.0 -> 0.1/0.0 |
| Neutral, careless | 41.2 -> 40.6 | 2.8% -> 2.7% | 0% -> 0% | 0.2/0.1 -> 0.3/0.1 |
| Neutral, careful | 38.0 -> 36.8 | 2.2% -> 2.0% | 0% -> 0% | 0.2/0.1 -> 0.2/0.1 |
| Warsaw Pact, careless | 562.0 -> 533.8 | 1.8% -> 1.7% | 65% -> 60% | 4.2/2.9 -> 3.8/2.4 |
| Warsaw Pact, careful | 558.7 -> 537.2 | 1.9% -> 1.9% | 0% -> 20% | 3.9/2.6 -> 3.5/2.5 |
| NATO, careless | 537.4 -> 516.3 | 1.3% -> 1.4% | 95% -> 95% | 2.6/4.5 -> 2.9/4.8 |
| NATO, careful | 539.6 -> 519.3 | 1.3% -> 1.3% | 15% -> 5% | 3.0/5.3 -> 2.6/4.6 |

`air_war_balance --theatre theatre_pannonia_nation.json data 10 7` (the nation), every band PASS before and after:

| Run | Sorties | Loss % | Battery wrecked | Air battle: theirs / ours lost |
|---|---|---|---|---|
| Rebels, careless | 14.2 -> 12.8 | 16.9% -> 13.3% | 0% -> 0% | 0.1/0.0 -> 0.0/0.0 |
| Rebels, careful | 14.4 -> 12.5 | 13.9% -> 17.6% | 0% -> 0% | 0.2/0.0 -> 0.2/0.0 |
| Neutral, careless | 25.0 -> 22.2 | 4.8% -> 6.3% | 0% -> 0% | 0.3/0.0 -> 0.2/0.0 |
| Neutral, careful | 20.3 -> 17.3 | 6.4% -> 5.8% | 0% -> 0% | 0.6/0.0 -> 0.4/0.0 |
| Warsaw Pact, careless | 982.6 -> 921.9 | 3.6% -> 3.5% | 80% -> 30% | 7.7/5.4 -> 8.7/5.9 |
| Warsaw Pact, careful | 956.8 -> 934.4 | 3.6% -> 3.3% | 0% -> 0% | 8.0/4.0 -> 8.2/5.8 |
| NATO, careless | 763.8 -> 735.8 | 2.3% -> 1.9% | 10% -> 20% | 4.6/8.2 -> 3.4/8.8 |
| NATO, careful | 749.3 -> 717.3 | 2.2% -> 2.0% | 10% -> 0% | 3.5/8.3 -> 3.8/8.7 |

**Sorties fall 2-15 %** on both theatres and for every faction: the tuned theatres are dated 15 October, when the
month's share of days not clear (about 0.67, interpolated between `crawford_1977_rand`'s summer and winter) is above the
theatre's fixed half, so more days are cloudy or poor and fewer packages fly. With `weather.mode` 0 alone the Pact's
sorties on the nation are back at 986 and 1,008 (careless, careful).

**The Pact's careless battery on the nation** is wrecked in 30 % of runs (80 % before). Each switch off alone, over the
same ten seeds: `route.mode` 1, 70 %; `weather.mode` 0, 60 %; `learning.mode` 0, 30 %. The routes give most of it: the
Pact's routes now cost the legs near our believed airfields and join gates that had no feature, so fewer of its
packages come the short way past the battery; the autumn weather gives the rest. Ten seeds move a share like this by
10-15 points, so it is a direction, not a measure. The careful battery is not wrecked either way. Elsewhere the wrecked
shares move within that noise (the district's careful Pact run 0 % -> 20 %, NATO careful 15 % -> 5 %).

**The rebels on the nation** stay out of the period's "few per cent" (13-18 %), as since B2 (`01_ROUTES.md` Left open);
their sympathisers report our mobile batteries (five reports in three days in `test_routes.cpp`). Raids through the
battery's sector a campaign: on the district 1.6 -> 1.9 careless and 0.9 -> 0.9 careful, on the nation 7.3 -> 6.1 and
2.2 -> 3.1: the careful battery, the one the other sources find least, is found more on the nation. Losses a
sortie elsewhere move by less than 0.6 points.

**Cost.** The district's run took 27.6 min with S7 and 28.0 min without, the nation's 68.6 and 68.7 min, each pair side
by side on a machine shared with other builds: the coarser lattice and fewer IP headings without a map pay for the new
fighter term and the joined gates, but the district's run is no cheaper than before.

`air_battle_bench data --compare 600` (the testbed, `air_levels.json` mode 2 against the levels off): identical, two
level changes, launches 2 and 2. The levels' mode 2 moves nothing a test or the bench can see; the receivers' bound is
not bit-exact through the path cache (`02_FLIGHTS_AND_PILOTS.md` 5.4) but changed no line here.

**Left:** the rebels' losses on the nation; the share of wrecked batteries needs more seeds than ten to say how much each
switch moves it.
