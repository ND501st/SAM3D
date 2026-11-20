# Implementation plan: track F, the fronts

The tasks, their order, what each touches, what it rests on and how it is tested. Design: files 01-06 of this folder
and its README (decisions, the efficiency budget, the save modes). Research: `docs/research/FRONTS_RESEARCH_2026-11.md`.

## 1. Where the track sits

- **Track F rests on tracks B and R**, both done in simcore and merged into `track-f`: the routes (B2), C2 (B5), the
  air fight (B8), our fighters (B10), the factions' hot phase (B11), the balance bands (B14), the damage states (R1).
- **It runs beside track G** (the ground) and uses its route service (`simcore/nav/`) for everything that moves on
  the roads.
- **It changes one earlier principle**: the front moves by the units on it (`docs/campaign/README.md` B4). The roadmap's
  phases 13-21 stay as planned; track F feeds 14.2 (the regiment's tasking) and 14.4 (the working map).
- **F1 is built in parallel** (worktree `f-f1`). F2.1 and F3.1 need not wait for it (`01_ANCHORS.md` §3).

## 2. The tasks

Each row is one agent's task and one review, its own branch and worktree (`C:\dev\SAM3D-wt\<name>`), merged into
`track-f`. "Touches" names the files; "Tested by" the tests or tools that must be green at merge, besides every
existing test and, from F3.3 on, `fronts_bench --gate`.

| # | Task | Touches | Rests on | Tested by | Doc |
|---|---|---|---|---|---|
| F0 | **Research and design.** **Done 20 Nov 2026**: four topic files, the synthesis, 56 sources registered, this folder | `docs/research/`, `docs/fronts/`, `docs/SOURCES.md` | | `check_citations.py` | all |
| F1 | **The map as the source of truth** (anchors). Built in parallel; design in `tf-f1`'s commit `23f2e4e` | `data/maps/pannonia.json`, theatres, `air_war/data.hpp`, `command/data.hpp`, `tools/map_view.py` | F0 | its own: the 2 km move test, the identity suite | 01 |
| F2.1 | **Sectors and the line of contact.** `"sectors"` per theatre front; `LineOfContact` (stations every 5 km, offsets, `DistanceTo`, `SectorOf`); `AirCampaign::DistanceToFront` and the planner's `DistanceToLine(front_->line, ...)` read it. Zero offsets only | `air_war/front_line.hpp` (new), `air_war/data.hpp` (`LoadFront`), `air_war/campaign.hpp`, `planner.hpp`, `c2.hpp`, both theatres' fronts | F0 (F1 not needed) | `test_front_line.cpp`: lookups against hand calculations; zero offsets bit-identical; every balance tool's output unchanged | 02 §2 |
| F2.2 | **Believed cover per sector.** `SectorCover`: the believed working radar SAM fire units over a band and the mobile air defence share, over the sector's pre-war baseline; the per-sector loss window | `air_war/air_front.hpp` (new), `intel.hpp` (a band query beside `BelievedIadsShare`), `campaign.hpp` (losses by sector) | F2.1 | `test_air_front.cpp`: cover by hand on a small picture; decoys and believed-destroyed units; a unit leaving lowers it | 02 §3 |
| F2.3 | **The air front's decision.** `AirFrontDef` (a faction's `"air_front"` block), `AirFrontState`, `DecideAirFront` in `AirCampaign::Plan`; gates, reward, delay, steps, pull-back, crisis (stub until F3.5), helicopters, the rebels' surge, the neutral's border; the journal's entries; `air_front.mode` and its save key | `air_front.hpp`, `campaign.hpp`, `save.hpp`, four faction files, `data/gameplay/air_war.json` | F2.2 | `test_air_front.cpp`: a hole opens one sector only; losses pull it back; helicopters lag; on belief not truth; save without the key replays at 0 | 02 §3, §6 |
| F2.4 | **The planner, routes and C2 read it.** Close support, helicopter raids and harassment depths plus the sector's front; the let-down and helicopter holding areas at the air front; the umbrella; BAI depth | `planner.hpp` (the `MissionKind` switch, `NominationWaits`), `route_planner.hpp`, `c2.hpp` | F2.3 | `test_air_war.cpp` (the planner), `test_routes.cpp`, `test_c2.cpp` cases at depth 0 and 20 km; `air_war_balance` nation unchanged at mode 0 | 02 §4 |
| F2.5 | **Our side learns it.** Indicators per sector and day (raids seen deeper, intercepts, troops' reports); the army's air defence staff's conclusion as a fact after its delay | `command/air_war_feed.hpp`, `info_net.hpp` (if a field is needed), `chain_pannonia.json` (the staff's role) | F2.3 | `test_command.cpp` (the feed): the conclusion comes only after enough indicators, through the pipeline, never from `depth_m` | 02 §5 |
| F3.1 | **Ground data.** The enemy's ground orders of battle per faction (order of battle format), its plans; our deployment per adversary; equipment filled for our units; `ground_war.json`'s tables with sources; the loaders | `data/air_war/ground_<faction>.json` (new), `data/campaign/ground_defence.json` (new), `oob_pannonia.json`, `data/gameplay/ground_war.json` (new), `ground_war/ground_data.hpp` (new), `docs/SOURCES.md` | F0 (F1 for places, else marked) | `test_ground_data.cpp`: every file loads strictly; every value sourced or a labelled game value; `check_citations.py` | 03 §3, §11 |
| F3.2 | **The sector's combat model.** Strength, power, posture, ground and rivers, losses, advance; pure function | `ground_war/units.hpp`, `combat.hpp` (new) | F3.1 | `test_ground_combat.cpp`: FM 101-10-1's day at 3:1, HERO's classes, Dupuy's factors, a river holds, hand calculations | 03 §4 |
| F3.3 | **The ground war's clock and the line.** `GroundWar` at U0 stepped hourly in `StepTo`; the event queue; quiet sectors in closed form; the line's offsets from the units; the air war's `DistanceToFront` from it; `ground.mode` and its save key; `fronts_bench` (cost by part) and `air_war_balance --cpu` | `ground_war/ground_war.hpp` (new), `campaign.hpp`, `save.hpp`, `simcore/examples/fronts_bench.cpp` (new), `CMakeLists.txt` | F2.1, F3.2 | `test_ground_war.cpp`: the line moves by a hand case, keyed draws order-free, mode 0 and the district theatre bit-identical; `fronts_bench --gate` | 03 §2, §12, README efficiency |
| F3.4 | **Commanders.** Officers from pools; decision times and events; the Pact's and our rule tables, NATO's; retreat as a unit or dispersed | `ground_war/commanders.hpp` (new), `ground_war.hpp`, the ground data's pools | F3.3 | `test_ground_commanders.cpp`: no withdrawal without an order, the second echelon to success, NATO's breakpoints, dispersal under 1/2 | 03 §5 |
| F3.5 | **Each side's picture.** Per sector the believed line and units, by reports and the enemy's `IntelPicture`; commanders and F2.3's crisis read it | `ground_war/picture.hpp` (new), `commanders.hpp`, `air_front.hpp` | F3.4 | `test_ground_picture.cpp`: a commander acts on a stale picture; truth changed after a report does not move his decision | 03 §9 |
| F3.6 | **U1 in the player's sector; the march.** Regiments and battalions as actors in the battery's sector and its neighbours; up and down with margins; regrouping and reserves on the roads, cached | `ground_war.hpp`, `units.hpp`, `nav/routes.hpp` (cache key if needed) | F3.4 | `test_ground_levels.cpp`: U1 against U0 over 20 seeds within a band; strength kept across levels; `fronts_bench --player-sector` in budget | 03 §3.3, §7 |
| F3.7 | **The air war follows the units.** `Troops` objects follow their units, strikes come off strength, supply lines' ends follow, objects overrun, the relocation rule on the line, NATO's FSCL | `campaign.hpp`, `logistics.hpp`, `c2.hpp`, `ground_war.hpp` | F3.3, F2.4 | `test_ground_air.cpp`: a strike's damage reaches the unit; a column of supply follows; an overrun object leaves the target list; the FSCL by hand | 03 §6 |
| F3.8 | **Situation reports into the pipeline.** Our units' reports on schedule and on events, up the chain to the army, the national sector and the SAM regiment; `FactKind::Situation`; orders as `Order` | `command/ground_feed.hpp` (new), `info_net.hpp`, `post_view.hpp`, `chain_pannonia.json` | F3.4, F3.5 | `test_ground_feed.cpp`: reports reach the posts with their delays; a post holds only what reached it | 03 §10 |
| F4.1 | **Sector priorities.** Our AD commanders, their traits and rhythm; defend, the offence, economy; orders through the pipeline | `campaign/ad_strategy.hpp` (new), `data/campaign/ad_strategy.json` (new) | F2.5, F3.8 | `test_ad_strategy.cpp`: on reports only, after the delay; one change a decision | 04 §2 |
| F4.2 | **SAM and radar moves.** National units move whole on orders by the norms, on the roads; leapfrog; overrun; `Relocate`'s timer kept only for moves inside a sector; `ad_strategy.mode` and its save key | `ad_strategy.hpp`, `campaign.hpp` (`MoveUnit`, `Relocate`), `save.hpp` | F4.1, F3.7 | `test_ad_strategy.cpp`: S-75 move 10-15 h; three a day at most; overrun in time and late; save at 0 | 04 §3, §6 |
| F4.3 | **Fighters by sector.** `FighterCoverAt(pos)` from the sector's priority; the sky's sharing for own-air rules | `campaign.hpp`, `planner.hpp`, `route_planner.hpp`, `air_fight.hpp`, `command/own_air.hpp` | F4.1 | `test_air_fight.cpp`, `test_own_air.cpp`: mode 0 the front's number; the sum over sectors kept | 04 §5 |
| F4.4 | **The regiment and the battery follow.** New tasking triggers; tasks on the battery net | `campaign/regiment.hpp`, `data/campaign/tasking.json`, `mission_session.hpp` | F4.1, F3.8 | `test_regiment.cpp`, `mission_demo --war ... --hours 24`: a division's withdrawal gives the battery a march task | 04 §4 |
| F5.1 | **The situation map.** Per post from its facts; `PostViewJson`; the debrief | `command/situation_map.hpp` (new), `post_view.hpp` | F3.8 | `test_situation_map.cpp`: two posts, two maps; stale stays stale; no truth in the view | 05 §1 |
| F5.2 | **Units in sight in Unreal.** U2 movers in a session; drawn as basic models within visual range; the front's distant sounds | `nav/session.hpp`, `ground_war.hpp` (U2), Unreal: the mover actors, `FSimBasicModels`, the fx subsystems | F3.6 | simcore: `test_ground_levels.cpp` (U2 in sight); Unreal: the owner builds and looks | 05 §2 |
| F5.3 | **The campaign screen from our knowledge.** `ASimCampaignHUD` from the regiment's situation map; `tools/map_view.py --front` truth beside belief | Unreal `SimCampaignHUD.cpp`, `SimAirCampaignSubsystem`; `tools/map_view.py`, its tests | F5.1 | `python -m pytest tools -q`; the owner looks | 05 §3, §4 |
| F6.1 | **The fronts against history.** `fronts_bench --history`; the bands in `balance_bands.json` | `fronts_bench.cpp`, `balance_bands.json`, `docs/air_war/09_BALANCE.md` | F2-F4 | the bands green or argued | 06 §1, §2 |
| F6.2 | **The air war's bands on a moving front, and the player's effect.** B14's bands re-run with every F mode on; `fronts_bench --player`; the outcomes' ledger of the air front's delay | `air_war_balance`, `air_war_bench`, `campaign/outcomes.hpp`, `09_BALANCE.md` | F6.1 | the bands; the player's effect within its bound; `crew_balance`, `helper_coverage`, `ground_check` green | 06 §3, §4 |

## 3. Order

1. **F2.1** first: everything measures from the line of contact.
2. **F2.2-F2.5 and F3.1-F3.2** in parallel: the air front on the fixed line, the ground's data and combat as pure
   functions. F2.3 ships its crisis as a stub.
3. **F3.3** on F2.1 and F3.2: the line moves. From here `fronts_bench --gate` is each merge's check.
4. **F3.4, then F3.5**: commanders, then their pictures. F2.3's crisis reads F3.5 when it lands.
5. **F3.6, F3.7, F3.8** in parallel on F3.4.
6. **F4.1**, then **F4.2-F4.4** in parallel.
7. **F5.1** on F3.8; **F5.2** on F3.6; **F5.3** on F5.1. Unreal parts are the owner's to build.
8. **F6.1, F6.2** last.

Design-heavy starts (F2.3, F3.2, F3.4, F4.1) suit Opus or Fable; data entry (F3.1) and implementing a written design
(F2.1, F2.4, F3.7, F3.8, F5.1) suit Sonnet.

## 4. Rules every task keeps

- **simcore's hard rules.** Header-only C++17, standard library only, no exceptions or RTTI, SI suffixes, the ENU
  frame, warnings as errors. Every new model gets a test, checked by hand calculation where it is a table.
- **Save modes.** New behaviour behind `air_front.mode`, `ground.mode` or `ad_strategy.mode`, written in the save; a
  save without the key replays at 0. At 0, and on a theatre without a country map, nothing is built and no draw made.
- **Determinism.** Keyed forks by (sector, day) or (unit, decision time); no draw from the campaign's main stream; a
  saved campaign replays to the bit.
- **Knowledge, never truth.** The enemy's staff and commanders decide on its picture; ours on the facts their posts
  hold. No NPC of ours acts on the enemy's truth.
- **First person.** No omniscient front line in the game. The front reaches the player's character through reports
  and what is in his sight.
- **The written record.** Our side's decisions read the posts' records; reports carry their errors into them.
- **Sources.** Every value in `data/` with `source` and `confidence`, registered in `docs/SOURCES.md`, never tier 5,
  1985 or before (post-period only as flagged hindsight). Game values and SAM3D estimates labelled.
- **The budget.** Each task measures its part with `fronts_bench` and writes the numbers into its as-built note.
- **One enemy at a time**: each faction's ground war is its own; nothing plays two at once.

## 5. Milestone M-F: done when

The owner runs `fronts_bench` and a Pact campaign on the nation, plays a mission in battery 21's sector on day 4, and
sees the following.

1. **The line moves.** The Pact's main thrust breaks through our security zone in a day and is held at the main
   defensive zone; NATO's corps advances slower and stops at its breakpoints. Each by its officers' decisions, written
   in the journals.
2. **Retreat both ways.** A division ordered back goes back as a unit; one broken under air attack disperses and
   re-forms days later.
3. **The air front.** In the sector where our SAMs were destroyed, the enemy's close support and then its helicopters
   work 20 km deeper within a day; in the next sector, where the battery survived, they do not.
4. **Our answer.** The army's air defence chief, on reports, moves a Kub unit forward into an ambush and the regiment
   gives battery 21 a march task on the battery net.
5. **What the player knows.** The regiment's map in the PU-12M shows the line as last reported, a few hours old; the
   campaign screen the same; the debrief, after the war, the truth beside it.
6. **In sight.** On the march the battery passes a column of our tanks going forward, drawn as basic models.
7. **The bands.** The fronts' bands and track B's are green or argued; the player's effect within its bound.
8. **Cost.** `fronts_bench --gate` passes; `air_war_balance` on the nation is within 1.05 times its time without the
   fronts.
9. **Determinism.** The same campaign, saved and loaded, plays the same; an old save replays as it was played.

## 6. Not planned

- Fire by direct-fire weapons in a session: the session shows units; their combat is the sector's table.
- Encirclement and pockets.
- U1 everywhere at once.
- Our own air force's attacks on the enemy's ground units.
- An offensive into the enemy's country (the owner's decision 4).
- An economy or a national strategy game.
- The enemy's airfields moving forward with its front.
