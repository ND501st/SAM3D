# 7. Integration with every system built so far

The enemy air command is a layer above the existing game: it produces scenarios the existing runner plays and reads
their outcome through public interfaces only. Nothing in the runner, the engagement model or the EM layer was changed
for it (the EM session's fixes of 18 October 2026, own ground objects no longer seen as contacts and a deployed battery
no longer stowing between raids, came from issues the raids exposed).

| System | What the air war takes from it | What it gives it |
|---|---|---|
| Scenarios (`scenario.hpp`, `data/scenarios/README.md`) | the format and the strict loader: every generated raid is validated like a hand-written scenario | targets, waypoints with heights, behaviours, evasion, triggers, battery, C2, prepared position, doctrine, environment |
| The runner (`scenario_runner.hpp`) | `Step`, `Time`, `Launches`, `TargetAlive`, `TargetPosition`, `Console()`, `Log()`, `ArmsFired()`, `ArmResults()` (the EM ARMs' ends, A11) | events recorded in its log (releases, ARMs, bombs), so the debrief shows them |
| Target behaviour (`target_behaviour.hpp`, roadmap 9.3) | terrain following, pop-up, evasion on lock or launch | low raids with waypoint heights, helicopter hides, how each type evades |
| Engagement model (`engagement.hpp`, `data/gameplay/osa_ak.json`) | hit chances by class (`fighter`, `bomber`, `helicopter`, `drone`, `default`), HIT records | each aircraft type's class, RCS, speed, height and turn rate |
| EM layer (`em_link.hpp`, `data/em/`) | the Osa's radars and their emission; jammers (`nato_alq99_band8`, `generic_soj_barrage_c`, `generic_selfscreen_noise_ku`), RWRs (`nato_alr46_class_rwr`, `generic_rwr`), decoy emitters, rain | the faction's EM libraries; jammers on their aircraft; RWRs; rain in poor weather; ELINT exposure (seconds radiated) back to the campaign |
| Threats (roadmap 9.1, 9.2, E5, E6) | method Н against jammers, the TV check against decoys, search bursts; anti-radiation missiles as EM missiles (`em/arm.hpp`, the library's `agm45_class_shrike`, `agm88a_class_harm`, `kh58_class` in `em/nato_1985.json`) and the crew's ARM drill | decoy drones marked `decoy`; the crew's `doctrine` from our side's intelligence and the battery's posture; the SEAD and hunter aircraft's `"arms"` (their type's `arm_em`, aimed at where the enemy believes the battery is) |
| Crew (`crew_data.hpp`, `docs/crew/`) | NPC vehicle commander and crew, skill tiers, callouts | the crew tier from the campaign's progression (pillar 5); missions with no scripted orders: the NPCs fight by doctrine |
| Battery (`battery.hpp`, `battery_fire.hpp`) | the battery's other launchers fighting, air-threat alert, emcon order, TZM | the vehicles the battery still has, its layout, night |
| Prepared positions (`position_prep.hpp`, roadmap 7.6) | emplacement, nets, dummies with decoy emitters | where the dummies stand, their decoy mode |
| PU-12 and C2 (`pu12.hpp`, `c2_sources.hpp`, `data/command/`) | the board, the regiment's radars, warning nets, outages, fallback | the regiment's radars that still stand; the warning nets near the battery with outages for destroyed ones; the PU-12 blind when its radars are gone; board types and formations |
| Missions menu (`missions.hpp`, `data/scenarios/missions.json`) | the catalogue | an "Air war" group of seven generated raids (one per adversary and kind), loaded and started by `test_missions` |
| Debrief (`debrief.hpp`) | the session log | the raid's events; the campaign's journal and `EnemyMind` for the campaign debrief |
| Chain of command and the information pipeline (`simcore/command/`, `docs/command/`) | the people of our side, their places and means of signal; the player's character's knowledge | everything our side learns (its journal's visible entries, the raids the warning net saw, wrecked objects) becomes a fact held by its first knower and reaches the player only down the chain (`air_war_feed.hpp`); the raids' danger zones on the roads, cut wire, the battery's moves; first person: the campaign screen shows only the player's deliveries |
| Multiplayer (`docs/MULTIPLAYER.md`) | the server simulates | the campaign is deterministic for a seed and runs on the server |
| Unreal (`unreal/README.md`, "Air war campaign") | the missions menu, the scenario director, the world subsystem's runner (`GetScenarioRunner`), the vehicle and its cabin as in any mission | `USimAirCampaignSubsystem` (the campaign across level loads, raid files in `Saved/AirWar/`, the bridge stepped while a raid plays, saves), `ASimCampaignHUD` (the campaign screen), the menu's campaign box, `?SimCampaign` and `?SimAirWarRaid` |
| Tools (`tools/check_citations.py`) | the citation rules | every source of `data/air_war/` registered (`docs/SOURCES.md`, "Enemy air command sources") |

## Scenario format flags

`ScenarioFormat` (raid_scenario.hpp) switches the keys added on the EM branch (`environment`, target `rwr`, target
`decoy`, `doctrine`) on or off, for a build whose loader lacks them. All are on by default. `model` writes each
aircraft's basic model (`docs/scenery/`), `weather` the day's weather.

## On the map

With `map_file` the raid plays on a map's ground (`"terrain": {"type": "map"}`, `simcore/terrain_map.hpp`); the campaign
screen writes its raids that way when `data/maps/kesmark_range.json` exists, and fights them in `/Game/Maps/KesmarkRange`.

The campaign moves the battery kilometres at a time (the posture's moves, or the player's own on the campaign screen)
without seeing woods, villages or the river, so a raid could put a launcher inside a wood: hidden, its radar under the
canopy. The first test of the raids on the map found exactly that (18 October 2026: the NATO decoy raid's battery stood
in a wood with a 17 m canopy). With `map` (the loaded map) the builder sets the battery down on the nearest open ground:
the smallest move, in rings 100 m apart out to 2.5 km, that leaves every vehicle of its layout with nothing taller than
3 m within 20 m (houses and woods are; a revetment's earth wall is not) and no river within 20 m; each dummy the same
way. Every run aimed at the battery or a dummy (its flight's `aim_key`) moves with it, route and aim, so the attack keeps
its geometry; everything else stays where it is on the map. The campaign's own positions are unchanged (it stays
independent of the map and deterministic); `RaidManifest::own_moved` records the move. `air_war_demo <faction> <days>
data <out_dir> <seed> maps/kesmark_range.json` writes and plays its raids on the map (two of the first three NATO raids
needed a move of 100-200 m). Test: `test_air_war`, "on the map a raid sets the battery and its dummies on open ground".
