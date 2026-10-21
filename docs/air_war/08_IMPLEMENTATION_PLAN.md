# 8. Implementation plan (roadmap phase 11, the enemy air command)

| # | Task | Status |
|---|---|---|
| A0 | Research: doctrine of the four adversaries, the period's campaigns and their lessons, intelligence disciplines, games survey (`docs/research/ENEMY_AIR_RESEARCH_2026-10.md`); sources registered | **Done 18 Oct 2026** |
| A1 | Data: the theatre, four factions (doctrine, intelligence, logistics, politics and events, aircraft, squadrons, officers), 31 quirks, the balance table; strict loaders | **Done** (`data/air_war/`, `data/gameplay/air_war.json`, `air_war/data.hpp`) |
| A2 | The air staff: officers, traits, skills, quirks, memory, stress, reputation, careers, replacement | **Done** (`officers.hpp`) |
| A3 | The intelligence picture: disciplines, latency, ageing, ghosts, dummies, claims | **Done** (`intel.hpp`, `campaign.hpp`) |
| A4 | The planner: apportionment, candidates, utility, packages, approval, interference, reasons | **Done** (`planner.hpp`) |
| A5 | Abstract resolution | **Done** (`resolve.hpp`) |
| A6 | The campaign: hourly loop, outcomes, lessons, politics, logistics, repair, relocation, careers, our side's intelligence | **Done** (`campaign.hpp`) |
| A7 | Raids as scenarios and the mission bridge; the demo plays them with the NPC crew | **Done** (`raid_scenario.hpp`, `mission_bridge.hpp`, `examples/air_war_demo.cpp`) |
| A8 | Missions menu: seven sample raids | **Done** ("Air war" group) |
| A9 | Tests | **Done**: 14 tests (`tests/test_air_war.cpp`) and the demo as a test |
| A10 | **Unreal campaign screen**: the map with what our side knows, the INTSUM and dossiers, the posture and relocation orders, the next raid's warning, the debrief's reveal | **Built 18 Oct 2026** (`USimAirCampaignSubsystem`, a game-instance subsystem; `ASimCampaignHUD`; the missions menu's campaign box; `?SimCampaign`, `?SimAirWarRaid` in the director; automation test `SAM3D.AirWar.Raid`; `unreal/README.md`). Built from the command line; not yet played on screen by a person |
| A11 | **The ARM as an EM missile and the crew's ARM drill** (roadmap E6, the EM session): stop emitting on the warning, relocate, bursts; replace the bridge's ARM model | In progress on the EM track (18 Oct 2026): ARMs become scenario objects the runner fires and resolves, seen by the crew's instruments, with an NPC drill. Then the raid builder writes each aircraft's ARMs into its scenario target (`arms`: count, range, speed, memory) and the bridge reads the runner's ARM results instead of keeping its own |
| A12 | **Balance pass** like crew 8.12: every faction over many seeds, the losses and tempo against the period's shapes (NATO's rollback taking days, the Pact's opening blows, small air arms' caution), the player's sector rate | **First pass done 18 Oct 2026** (`09_BALANCE.md`, `examples/air_war_balance.cpp`): eleven findings, ten changes to the models and data; a person's playthrough is the second half |
| A13 | Save and load a campaign | **Done 18 Oct 2026** (`air_war/save.hpp`: the seed and the player's actions, replayed exactly; Unreal saves after every step to `Saved/AirWar/campaign.json`, the menu's C continues it) |
| A14 | The battery commander's campaign orders through the PU-12M (in command, the battery level); several batteries of the player's side | Later |
| A15 | Our side's air force as a player of the same game (the defender's fighters are a number today) | Later |
| A16 | More doctrine from the documents still to acquire (CIA 1985 in full, FM 100-2-1, TT 100-44-1, Pact exercise records) | Parallel |

## Done-when for the next steps

- **A10**: a person starts a campaign from the menu, sees the INTSUM and the map, sets the posture, plays the raids that
  come, and sees the enemy's staff revealed at the end, without the editor. (Built; the person's playthrough is left.)
- **A11**: an NPC crew warned of an ARM launch stops emitting; the bridge's ARM model is gone.
- **A12**: a table like `docs/crew/11_PLAYTEST_AND_BALANCE.md` for the four adversaries, and the numbers it changed.
  (Done headless: `09_BALANCE.md`.)
