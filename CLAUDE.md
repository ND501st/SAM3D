# CLAUDE.md — SAM3D

A first-person, realistic 3D surface-to-air missile (SAM) simulator, rebuilding the mechanics of
SAM Simulator v930.2 (free, by a single hobbyist developer, used with his blessing) in Unreal Engine 5.
The player drives a SAM unit, deploys it, runs engagements, and gives orders to NPC crew.
The first system is the **9K33M2 Osa-AK as fielded in the 1980s, played as a battery** (five-man crew, PU-12 command
post). Research basis and open decisions: `docs/BRIEFING_2026-10.md`, `docs/research/OSA_RESEARCH_2026-10.md`.

## Repo map

| Path | What |
|---|---|
| `simcore/` | **Header-only C++17 sim core** (radar, missile, aircraft, procedures, crew, world). Engine-agnostic. |
| `simcore/include/simcore/em/` | **Electromagnetic layer** (`simcore::em`): propagation, calibrated radar, ESM/TDOA, jamming, tracking, knowledge, comms/C2, doctrine, debug overlays. See `docs/EM_ARCHITECTURE.md`, `docs/EM_HANDOFF.md`. |
| `simcore/include/simcore/command/` | **Our side's chain of command and the information pipeline** (`simcore::command`): who is who and where, the means of signal, how a fact reaches the player's character (first person: nothing else does), the air war's news fed in; the people at their posts, the staff list and the communications diagram as papers (`people.hpp`, G8); our radio intercept service for the enemy's air radio: posts, linguists, continuity, the fast road and the day summary into the pipeline (`intercept.hpp`, `FactKind::Intercept`, B9); our aviation: the day's IFF codes and who holds them, the plan of cooperation, the notices of our flights and the crews' rule on them, the fighter regiments' duty and our GCI's picture (`own_air.hpp`, `own_flights.hpp`, `FactKind::OwnAirNotice`, B10). See `docs/command/`. |
| `simcore/include/simcore/nav/` | **The route service** (`simcore::nav`, roadmap G1): mobility classes, one road graph derived from the map's data (the country's roads and the district's tracks), routes between anchors (places, positions, sites), overlays true and believed, the ground off the road cell by cell (`nav/ground.hpp`); the march plan and the column on it (`nav/march.hpp`, G2), movers at levels M0-M2 (`nav/movers.hpp`), the pipeline's carriers on the roads (`nav/carriers.hpp`), the movement table and exposure (`nav/movement.hpp`, G3), and the session's roads, movers and visual range (`nav/session.hpp`, a scenario's `"nav"`, G11). See `docs/campaign/11_BATTERY_LIFE_PROPOSAL.md` §5.1-5.3, 5.12. |
| `simcore/include/simcore/campaign/` | **The battery's life on the ground** (`simcore::campaign`, track G): the battery area and its places as anchors, position work as jobs (`battery_area.hpp`, G4); the missiles from the rear, the TZMs' runs and the commander's ledger (`stocks.hpp`, G5), a TZM's run in a session on the roads round the bridges down, ordered on the battery net (`TzmRun`, 17.4); the combat report from the post's records, the regiment's check and its trust in each unit (`report.hpp`, P9); the regiment's combat task to the battery (`battery_task.hpp`, 14.1) and the battery commander's plan for it, timed to its ready-by (`battery_plan.hpp`, 14.3); the regiment's tasking by its rules on what reached it (`regiment.hpp`, 14.2); the mission as one session with the enemy's air war on its clock, our radars, warning posts and road bridges following the war's damage to them (`mission_session.hpp`, 15.2, 17.3, 17.4); the player's two outcomes, assets kept and the enemy's freedom of action denied, ledgers from the truth for the debrief only (`outcomes.hpp`, 14.6); the director, waking a seat only on what reaches it and measuring the quiet between (`director.hpp`, 15.4). Rules from `data/campaign/`. |
| `simcore/include/simcore/scenery/` | **Ultra-basic models** (`simcore::scenery`): every aircraft, vehicle, site and building as a few primitives, from `data/scenery/basic_models.json`; drawn by Unreal's `FSimBasicModels`. See `docs/scenery/`. The munitions (`munitions.hpp`): every bomb, missile, rocket and round as a sourced shape, a body of revolution with fins, bands, retarders and motors, meshed at four LODs for `FSimMunitionModels`. The triangle mesh both use (`trimesh.hpp`); the map's trees, wood masses, houses, barns, flats, halls and bridges as meshes (`map_meshes.hpp`, R6); the map's dressing, where they go: the land cover raster the ground is coloured from, the trees planted in the woods (never above the sight lines' obstacle heights) and the woods' masses, a building's variant (`map_dressing.hpp`, rules in `data/scenery/map_dressing.json`); the register of third-party art that may replace them, with its licence rules (`asset_register.hpp`, `data/scenery/assets.json`, `docs/scenery/ASSETS.md`). |
| `simcore/include/simcore/fx/` | **Weapons' effects for show** (`simcore::fx`): the presentation feed the runner fills (`events.hpp`, `ScenarioRunner::Fx()`: launches, bursts, releases, impacts, gun passes, aircraft down; nothing in the simulation reads it), sound that travels (`acoustics.hpp`: speed of sound, absorption, retarded time, Doppler, booms), blast and fireball scaling, missile, bomb, wreck and round paths, contrails. Drawn by Unreal's `USimFxSubsystem` and heard through `USimFxAudioSubsystem`. See `docs/fx/`. |
| `simcore/include/simcore/flights.hpp` | **Flights and packages in the session** (track B, B1): leader, element lead and wingmen in formation slots by faction and phase, orders with who gave them and the call's delay, succession, the package's nerve, the pilot's override (break, dive, jink, the ARM turn-away), steering each aircraft by its `AircraftDemand` (`aircraft.hpp`); the runner's `OrderFlight`, `RerouteFlight`, `Flight`, `FlightsOf`. A target with no flight is a flight of one and flies as before. See `docs/air_battle/02_FLIGHTS_AND_PILOTS.md` §2.5. |
| `simcore/include/simcore/air_war/` | **Enemy air command** (`simcore::airwar`): the other side's air staff as people, its intelligence picture, the planning cycle, abstract resolution, raids generated as scenarios and the mission bridge back; a package injected into a running session and the campaign run in step with it (`package_injector.hpp`, roadmap 15.3); the faction's jammers of our voice nets by FM 34-40-7's rule, put on the battery net with each package (`comms_jamming.hpp`, 16.2); the dynamic campaign testbed's endless random waves of every type (`test_waves.hpp`, a scenario's `"waves"`, `docs/DYNAMIC_CAMPAIGN_TEST.md`); the enemy's radio, every flight's and post's call a transmission in a session-level EM world with its turn on the net, discipline per faction and officer, and the hours warning's opsec draw (`air_radio.hpp`, a scenario's `"air_radio"`, track B's B9, `docs/air_battle/09_COMMS_AND_INTERCEPT.md`); what a pilot sees and hears (track B, B3): the eye's law, the ground law, each pilot's picture, the warning receivers' displays and the visible events the runner records from the simulation's own sources (`perception.hpp`, `perception_feed.hpp`, `docs/air_battle/03_PERCEPTION.md`); routes as flown, planned over the believed threats and the ground with corridors, gates, turn points and IPs on map features and a different way home (`route_planner.hpp`, track B2, `docs/air_battle/01_ROUTES.md`); command in the air (track B, B5): the faction's posts as people with authority by mission type and year, reports and requests as messages with their delays, the alert pools flying the planner's on-call share, diverts and hunters sent to a reported launch, packages flown time-stepped beside the campaign's clock, and a session's flights obeying only the orders a post may give (`c2.hpp`, `c2_def.hpp`, `package_control.hpp`, a faction's `"c2"`, `docs/air_battle/05_C2_AND_RETASKING.md`); targets of opportunity (track B, B6): a flight leader's chain on what his pilots see (seen, identified, authorised, able, worth it, act) by his faction's rules, his type's ease and his traits, attack passes and gun runs flown on the flights' override layer, claims into the enemy's records as claimed, and the same at the abstract level (`opportunity.hpp`, `opportunity_def.hpp`, `attack_pass.hpp`, `opportunity_session.hpp`, a faction's `"opportunity_rules"`, `docs/air_battle/06_OPPORTUNITY.md`); air combat (track B, B8): the abstract fight that replaced the resolver's flat fighter roll, our regiments' pair or four against the package's sweep, CAP and close escort and then its strikers, every shot through B7's chain (`air_fight.hpp`), and the live fight in a session, fighters that find, identify by the rules of engagement, are vectored by a GCI or an E-3A over B9's nets, lose control, commit and go home on joker and bingo (`air_fight_session.hpp`, `air_fight_def.hpp`, a scenario's `"air_fight"`, `docs/air_battle/07_AIR_TO_AIR.md`); each faction's hot phase (track B, B11): how its flights choose in the air, sourced values moved by their officer and pilot, the gates and the delays (`hot_phase.hpp`, `hot_phase_def.hpp`, a faction's `"hot_phase"`, `docs/air_battle/10_FACTIONS.md`); the fronts (track F): each theatre front's sectors and its line of contact, stations of offsets along the front's line that the campaign and the planner measure from (`front_line.hpp`, a front's `"sectors"`, F2.1), and what the enemy's air staff weighs by sector, the believed SAM cover over the next band against a pre-war baseline and its losses per low-level sortie (`air_front.hpp`, `air_front.mode`, F2.2; `fronts_bench --air-front`, `docs/fronts/`). See `docs/air_war/`. |
| `simcore/tests/` | Dependency-free unit tests (`SC_TEST`, `SC_EXPECT_NEAR`). |
| `simcore/examples/` | `engagement_demo`: one radar vs one target, writes a Tacview `.acmi`. |
| `prototype/` | Pure-Python mirror of simcore for fast experiments (runs on the Mac too). |
| `unreal/SAM3D/` | UE5 C++ project. Includes simcore through `SimCoreIncludes.h` only. |
| `data/systems/` | Per-system parameters. **Every value needs a source.** `pu12m_radios.json`: the PU-12M's R-111 and R-407 as sets (`simcore/radio_sets.hpp`). |
| `data/procedures/` | Per-system consoles: controls, subsystems, procedures (loaded by `console_data.hpp`). Same sourcing rules. |
| `data/scenarios/` | Scenarios: unit, crew, targets, triggers (loaded by `scenario.hpp`, played by `scenario_runner.hpp`). A scenario's origin, `"date"`, `"utc_offset_h"` and `"clock_start"` are its sky clock: one sun the whole session's light is read from (`simcore/light.hpp`, `docs/LIGHT.md`, R7); the missions menu's time of day moves its start hour (`missions.json` `"time"`). |
| `data/cabins/` | 3D cabin layouts: stations, panels, control and lamp positions, the floor a crewman walks on up from his seat (`walk`), the display each hand wheel is worked against (`watch`) (loaded by `cabin.hpp`). Presentation data; lamps need sources. |
| `data/vehicles/` | Each vehicle as a body to climb (`simcore/crew_mobility.hpp`): where each seat stands up, the hatches (and when the drill keeps them shut), the roof and its keep-outs, the ways down by the wheels, the places round it and the jobs done there, the line-up for К машине. Cited like `data/systems/`. See `docs/crew/13_OUT_OF_THE_VEHICLE.md`. `mobility.json`: each kind of mover's speeds for the route service (`simcore/nav/`). |
| `data/gameplay/` | Game-balance numbers for the gameplay engagement model (`engagement.hpp`). Not system data. `visual_range.json`: how far the player's character sees a vehicle by day, dusk and night (`nav/session.hpp`); `after_action.json`: the combat report from the records and the regiment's check (`campaign/report.hpp`); `damage.json` and `aaa.json`: damage states by chance and the guns' fire by range (`damage.hpp`, `aaa.hpp`, `docs/DAMAGE.md`, track R); `light.json`: the sun's thresholds for day, dawn and dusk, night, the TV's daylight and its twilight, the battery's dark and the moon's share of the night (`light.hpp`, `docs/LIGHT.md`, R7, S8); `clutter.json`: the far ground's towns' clutter heights for the line of sight (`terrain_map.hpp`, ITU-R P.1812, R6); `perception.json`: the pilot's eye (NWC TP 5709), the ground law (NWC TP 6005), the launch's signs and the memory of a picture (`air_war/perception.hpp`, track B, B3); `reactions.json`: the enemy pilots' reactions under fire by skill tier and faction profile (`simcore/reactions.hpp`; the chain from cue to call, manoeuvre, chaff and flares, jettison and the flight's decision is `simcore/reaction_chain.hpp`, B4, `docs/air_battle/04_REACTIONS.md`). |
| `data/campaign/` | The battery area's layout rules (`battery_area.json`) and the missiles' logistics (`logistics.json`), cited like `data/systems/` (`simcore/campaign/`); the battery commander's sequence on a task (`battery_plan.json`), the regiment's tasks (`tasks/`, named by a scenario's `"task"`) and its tasking rules (`tasking.json`). |
| `data/command/` | Command posts (PU-12M plotting board): cited like `data/systems/`; loaded by `pu12.hpp`, named by a scenario's `command_post`. Also our side's chain of command, its places and the means of signal (`chain_pannonia.json`, `places_pannonia.json`, `signal_means.json`; `simcore/command/`), and the talk tables ПТ-1/ПТ-2 (`talk_tables.json`, `simcore/talk_tables.hpp`); the battery's orders carried by a means (`battery_orders.json`, `simcore/command/battery_orders.hpp`); our radio intercept service (`intercept.json`: its posts, linguists by tier, the liaison's delay, the enemy's listeners; `simcore/command/intercept.hpp`); our aviation's rules and the Kremniy-2M's codes (`own_air.json`, `iff_codes.json`; `simcore/command/own_air.hpp`, B10). |
| `data/crew/` | The crew: seats, orders, callouts (Russian and English), tasks, world-fact rules (`<system>_crew.json`), skill tiers (`skill_levels.json`), the crewman's body (`body.json`), his pace and his voice's carry out of doors (`walking.json`), the crew's voices (`voices.json`) and the voice each seat's lines are recorded in (`recorded_voices.json`, read by `tools/voice_lines.py`). See `docs/crew/`. |
| `data/air_war/` | The enemy air command: the fictional theatre, the four adversaries (rebels, neutral neighbour, Warsaw Pact, NATO) with doctrine, intelligence, politics, aircraft, squadrons and officers, and the quirk catalogue. Balance numbers in `data/gameplay/air_war.json`. The air battle's air-to-air weapons, guns, fits, doctrine and the chain of chances' numbers are `air_combat.json`, its airborne sensors `sensors.json` (track B, B7; `simcore/air_war/air_combat*.hpp`; fighter radars in `data/em/fighter_radars_1985.json`); the warning receivers' displays are `sensors.json`'s `rwr_sets` (B3); chaff, flares and drop tanks by type and year are `countermeasures.json` (B4); the air battle's game values and our fighter regiments as the abstract fight sees them are `air_combat.json`'s `fight` and `ours` (B8). |
| `data/tutorials/` | The lessons (`simcore/tutorial.hpp`): a first lesson and one per seat of the Osa and of the PU-12M, each step one action on a lit control, shown over a mission named in `missions.json` (`"tutorial"`). See `docs/TUTORIALS.md`. |
| `data/scenery/` | Presentation data: the ultra-basic models of every asset (`basic_models.json`: archetype, size, colour per type; the air war's aircraft ids). Placeholders until real models exist. `munitions.json`: every munition's shape, motor, smoke and charge, cited like `data/systems/`. `map_dressing.json`: the rules of the map's dressing (tree spacing, rows, species, field parcels, crops, cover colours), game values. `assets.json`: the register of third-party art (origin, author, licence, redistributable), empty: every part procedural; files that may not be redistributed live in `unreal/SAM3D/Content/ThirdParty/`, ignored (`tools/check_assets.py`). |
| `data/fx/` | Effect and sound parameters (`effects.json`, `sounds.json`): game values for the weapons' effects, sound levels with their sources. Textures and sounds in `art/fx/`, generated by `tools/fx_textures.py` and `tools/fx_sounds.py`. |
| `data/maps/` | Generated maps (`simcore/terrain_map.hpp`): ground, rivers, roads, towns, woods, military sites, ground out to 150 km. `kesmark_range.json` is the 40 km test map, with two detail areas (battery 21's and the capital's, finer ground levels, R6). See `docs/TEST_RANGE.md`. `pannonia.json` is **the whole country** (`simcore/country_map.hpp`, `docs/COUNTRY_MAP.md`): ground at 1 km, borders, regions, rivers, and roads, railways and pipelines as networks with bridges that can be cut; the Kesmark range is a district of it and must keep agreeing (`tests/test_country_map.cpp`). It is the only source of truth for where things are (track F, F1): its settlements, landmarks and `sites` are places by id, and the theatres' objects, prepared positions and fronts' points (lines and entries among them) and our chain's places name one (`"at"`, the file's `"place_map"`) and stand at its point; move a site and everything anchored to it moves (`docs/fronts/F1_ANCHORS_AS_BUILT.md`). |
| `data/weather/` | Weather presets a scenario names (`"weather"`): winds, cloud layers, the air (`simcore/weather.hpp`). |
| `data/em/` | EM libraries: radars, receivers, jammers, radios, threat libraries. Real systems cite every value (`"provenance": "per_value"`); `generic.json` is placeholder. `air_radios_1985.json`: the air battle's radio sets and our comms intercept receivers (B9). |
| `docs/` | Plan, roadmap, architecture, sources (with tiers), research (`docs/research/`), briefing, dev setup, reverse-engineering notes. |
| `docs/campaign/` | **The battery's war** (planned, roadmap phases 13-21): the enemy's war aims and target picking, what we defend and why, how a battery mission starts (the regiment's order, the battery commander's sequence and working map), what is attacked, defended, ambushed, jammed and reconnoitred around the battery and why, the systems that run it as one continuous mission, the crew as people, the battery's day between fights, the enemy's reach, doctrine by opponent, and command under strain. Start at its README. |
| `docs/air_battle/` | **The air battle** (planned, roadmap track B): the war in the air while it is fought, bounded to 1985: routes as flown, flights and pilots, what a pilot sees and hears, reactions under fire, command and retasking in the air, targets of opportunity, air combat, our fighters and the battery, the enemy's radio and our intercept, each faction in the air. Start at its README. |
| `docs/crew/` | **Crew and teamwork reference** (the design pillars below): roles and knowledge, interdependence, behaviour paths, commands and callouts, skill, the drill master, commander mode, training. `generated/` is written by `tools/crew_map.py`. |
| `reference/` | Git-ignored. Original SAM Sim files, manuals, Ghidra project. |
| `tools/` | Setup and verify scripts for the PC (PowerShell) and Mac (bash); `panel_sheet.py` draws the cabin layout as a page; `button_map.py` draws every control's flow to subsystems, lamps and launch. |

## Commands

```bash
# simcore: build + test (any OS; on Windows run from a "Developer PowerShell for VS 2022")
cmake -S simcore -B simcore/build -DCMAKE_BUILD_TYPE=Release
cmake --build simcore/build --config Release
ctest --test-dir simcore/build -C Release --output-on-failure
./simcore/build/engagement_demo out/demo.acmi     # Windows: simcore\build\Release\engagement_demo.exe
./simcore/build/procedure_demo                    # Osa console session + debrief (run from the repo root)
./simcore/build/scenario_demo                     # play data/scenarios/osa_practice.json, print the debrief, write .acmi
./simcore/build/battery_link_demo                 # the Osa and the PU-12M in one battery (battery_link_practice.json): both debriefs
./simcore/build/mission_demo --war warsaw_pact 1 1  # the bridges mission (the regiment's task) with the Pact's air war live on its clock
./simcore/build/em_demo data out/em_demo.json out/em_demo.acmi   # EM layer MVP: timeline + 11 checks
./simcore/build/air_war_demo nato 5 data out/air_war 1          # enemy air command: a campaign, its raids played as scenarios, the journal
python tools/air_war_view.py out/air_war/air_war_nato.json       # that campaign as a page: the enemy's mind beside what our side learned
./simcore/build/air_war_balance data 20 7                         # every adversary over 20 seeds and two battery postures (docs/air_war/09_BALANCE.md)
./simcore/build/air_war_balance --theatre theatre_pannonia_nation.json data 10 7   # the same on the whole nation (N4); the demo takes --theatre too
./simcore/build/air_war_bench data 400            # weapons, attacks and losses against history (docs/air_war/09_BALANCE.md section 9)
./simcore/build/air_battle_bench data [--testbed 600]  # 100 aircraft near at A2, far at A2 and A1: ms a step by part (docs/air_battle/02 section 5.4)
./simcore/build/air_combat_bench data [seeds]       # the live air fight in a small world: B8's beside S3's, the far fight off and on (docs/air_battle/07 section 9)
./simcore/build/air_battle_bench data --compare 600    # the testbed with the levels off, off again and on: where the outcomes part
./simcore/build/air_battle_bench data --compare 3600 --seeds 3 --first 1   # the testbed's hour over seeds, levels off and on side by side
./simcore/build/air_battle_bench data --trace 900 [--who id]   # the testbed off and on in lockstep: where each aircraft's place, receiver, picture or stress first part
./simcore/build/em_bench data 500 60 24           # EM scaling benchmark (aircraft, SAM radars, ESM sites)
./simcore/build/own_air_balance data 20            # our fighters and friendly fire by notices and codes (docs/air_battle/08_OUR_FIGHTERS.md)

# Play the game (docs/PLAYING.md): the missions menu, or straight into the first lesson or a seat's lesson
.\tools\play.ps1 [-Tutorial | -Lesson senior_operator] [-Build]
.\tools\play_latest.ps1 [-Tutorial | -Lesson ...] [-NoUpdate]   # the newest commit from its own copy, C:\dev\SAM3D-play

# Python prototype and tools
python -m pytest prototype tools -q
python prototype/scripts/run_demo.py out/demo_py.acmi
python tools/panel_sheet.py                       # draw the cabin panels flat: out/panel_sheets/osa_ak.html
python tools/check_citations.py                   # every data source registered in docs/SOURCES.md, none tier 5
python tools/button_map.py                        # flow chart of every control and what's not built: out/button_map/osa_ak.html
python tools/crew_map.py                          # every crew action and how it connects: docs/crew/generated/osa_ak_action_map.md
out/voice_gen/.venv/Scripts/python -I tools/voice_lines.py   # record the fixed English lines: art/crew_voices/en/<seat>/ (docs/crew/10 section 4)
python tools/pu12m_cabin.py                       # write data/cabins/pu12m.json from the PU-12M interior's trace (after build_pu12m_interior.py)
./simcore/build/crew_balance data 10              # every crew scenario at every skill tier over 10 seeds (docs/crew/11_*)
./simcore/build/helper_coverage data docs/crew/generated/helper_coverage.md   # every mission from every seat following the helper vs the NPC crew
./simcore/build/helper_coverage data - 900 osa_crew_practice:operator --trace --debrief   # one seat: every move, refusal and duty
./simcore/build/crew_pose_dump data out/crew_poses.json [pu12m]   # the crew's bodies posed; draw them: tools/blender/render_crew_poses.py
blender --background --python tools/blender/pu12m_interior_check.py -- --poses out/crew_poses_pu12m.json   # PU-12M interior: what intersects
python tools/em_view.py                           # EM overlays in a browser from em_demo's JSON: out/em_view.html
./simcore/build/map_dump data maps/kesmark_range.json out/kesmark_range_map.json   # the test map's layout (or --height e_km n_km)
python tools/map_view.py out/kesmark_range_map.json --png                          # the test map from above: out/map_view/
./simcore/build/country_dump data maps/pannonia.json out/pannonia_map.json         # the whole country (or --route road e1 n1 e2 n2 [cut bridges], --height e n)
./simcore/build/nav_check data 200                # the route service: anchors off the road net listed, the graph and 200 routes timed
./simcore/build/ground_check data 3600            # nothing under the map (R5): the testbed's hour and every map mission, anything under the ground listed
python tools/ue_ground_check.py                   # the same in the game (-SimGroundCheck): every drawn actor against the drawn ground, from the logs
python tools/map_view.py out/pannonia_map.json --png                              # the country with the theatre's objects and fronts
python tools/photo_trace.py rectify tools/traces/r123m_front.json PHOTO   # flatten a reference photo to mm (docs/MODELLING_FROM_PHOTOS.md)
blender --background --python tools/blender/build_pu12m_radios.py         # the R-111 and R-407 meshes (check_pu12m_radios.py proves them)

# Everything at once
./tools/verify.sh          # Mac/Linux
.\tools\verify.ps1         # Windows
```

Unreal: open `unreal/SAM3D/SAM3D.uproject` (see `unreal/README.md`). Claude cannot run the editor.
After changing Unreal C++, ask the user to build (or run UnrealBuildTool from the command line if the engine path is known).

## Hard rules for simcore

- **Header-only, C++17, standard library only.** No Unreal types, no third-party dependencies.
- **No exceptions, no RTTI** (Unreal builds without them). No `throw`, `dynamic_cast` or `typeid`.
- **Avoid identifiers Unreal/Windows define as macros:** `check`, `verify`, `ensure`, `PI`, `TEXT`, `IN`, `OUT`,
  `ERROR`, `near`, `far`, `small`, `interface`, `min`, `max`, `DELETE`. `tests/header_check.cpp` enforces this.
- **SI units everywhere**, named with suffixes (`_m`, `_s`, `_mps`, `_rad`, `_db`). Degrees only at API edges (`FromDegrees`, `DegToRad`).
- **ENU frame:** x=East, y=North, z=Up. ENU `z` is NOT altitude far from the origin. Use `LocalFrame::AltitudeOf`.
- **Warnings are errors** (`-Wall -Wextra -Wshadow -Wconversion -Werror`, MSVC `/W4 /WX`).
- Every new model or behaviour gets a test. Physics tests compare against hand calculations or reference tables.
- **Keep `prototype/` in sync** when changing a model that both implement. The test suites mirror each other.

## Realism and data rules

- **Public and declassified sources only.** Never ask for, use or add classified or export-controlled material.
  Exception by the project owner's decision (19 Oct 2026): `p19_to`, the P-19 radar's Soviet technical description,
  marked secret but publicly posted by veterans, is used (`docs/SOURCES.md`).
- **Prefer sources from countries that operated the system** (tiers in `docs/SOURCES.md`): official documents and
  historic records first, then operator accounts, then SAM Simulator, then operator-country reference pages. Don't
  base decisions on Western "facts and stats" pages. When sources disagree, record both and say which was used.
- Every system parameter in `data/systems/*.json` needs `source` and `confidence`. Code demos and tests use
  **generic placeholder numbers**, clearly labelled, never real system data.
- Consoles and procedures follow SAM Simulator's behaviour, corrected by operator-country documents and operator
  accounts (`docs/re/osa.md`). Outcomes come from the gameplay-level model (`data/gameplay/`), calibrated to published
  ranges; generic physics comes from textbooks. Validate against Osa events (training ranges, Lebanon 1982, Angola
  1987–88).
- **Never commit the original SAM Simulator art, sounds, data or `sam.exe`.** They live in `reference/` (git-ignored).

## Crew and teamwork: the design pillars (project owner, 16 Oct 2026)

These govern all crew work; the full reference is `docs/crew/` (start at `docs/crew/README.md`).

1. **Teamwork with NPC crewmates is paramount.**
2. **Two halves of the game:** *in the crew*, the player is one member of the team, learning the drill and how to operate
   each role or seat; *in command*, the player gives the orders, first for the single Osa, then for the whole battery.
3. **Every NPC crewmate can work with every other; every action and command is mapped out and interconnected.** Nothing
   is done by "the vehicle": every action has an owner seat, preconditions that are other seats' actions or knowledge,
   and a callout that tells the others. Knowledge moves only through instruments and voice (no NPC acts on the truth).
4. **Behaviour paths for every operational environment and outcome, with callouts, instructions and commands for every
   possible action** (`docs/crew/03_*`, `04_*`, `data/crew/<system>_crew.json`).
5. **Skill levels** set by player progression and quests, affecting reaction times, speeds and accuracy in every facet
   (`docs/crew/05_SKILL.md`, `data/crew/skill_levels.json`).
6. **An ideal progression of events**: an (almost) objective perfect performance of every task, the reference for the
   training overlay. SAM3D's form of it is the **drill master** (`simcore/drill_master.hpp`, `docs/crew/06_*`): a planner
   over the console's own dependency data that, like a chess engine, gives the best next move for every seat from any state.
7. **Interdependence is mapped accurately**: to lock a target someone must know where it is (radar, optics, EWR, radio)
   and someone must have driven the vehicle within reach (`docs/crew/02_INTERDEPENDENCE.md`, the generated action map).

## Unreal conventions

- Epic coding standard (tabs, `F`/`U`/`A`/`E` prefixes, `b` for bools). Categories `SAM3D|...`.
- Convert coordinates only through `USimWorldSubsystem::EnuToUnreal` / `UnrealToEnu` (ENU m ↔ Unreal cm, X=N, Y=E).
- The simulation runs at a fixed step in `USimWorldSubsystem::Tick`. Never step simcore from actor ticks.
- Game logic that affects the simulation goes in simcore. Unreal handles presentation, input, UI and animation.
- Player and NPC crew act on the **same** `simcore::ControlPanel` through `ProcedureTracker`/`CrewMember`.

## Which Claude model for what

| Task | Model |
|---|---|
| Architecture, radar/guidance/physics maths, validation, hard bugs | Claude Fable 5.1 or Opus 5.5 |
| Reading Ghidra output of `sam.exe`, research synthesis, large refactors | Claude Opus 5.5 |
| Everyday Unreal C++/Blueprints, UI, crew AI, tests | Claude Sonnet 5 (default) |
| Bulk data entry (switch/procedure tables), log triage | Claude Haiku 4.5 |

Switch with `/model` in Claude Code.

## Current status / next steps

See `docs/DEVELOPMENT_ROADMAP.md` (phase 6, the accuracy pass, done; phase 7, the battery and PU-12, done in simcore, and the PU-12M command post built through its plan's P0-P8 (console, crew, information chain, battery commander, exterior, interior and cabin, multiplayer, and P8 the post and the player's Osa in one battery, `simcore/battery_link.hpp`: `docs/PU12_DEVELOPMENT.md`; its R-111 and
R-407 worked as sets, the regiment's command net and telecode on their frequencies, `simcore/radio_sets.hpp`,
`docs/research/PU12_RADIOS_2026-10.md`); phase 9, threats (jamming, method Н, target behaviour), done in simcore; phase 10.1-10.2, range days (Ashuluk, Ustka, Kapustin Yar) and battles (the Bekaa 1982, Angola, Cuito Cuanavale 1988) as scenarios, done in simcore (the missions menu's "Ranges and history"); phase 10.3, the 9Ф632 trainer's instructor (`simcore/instructor.hpp`, the F9 panel), done; phase 8, crew and teamwork, re-planned around the crew pillars (`docs/crew/09_IMPLEMENTATION_PLAN.md`): 8.0-8.13 done in a first pass (8.10 bodies and voices, 8.12 a headless balance pass, 8.13 the crews' communications: headsets, boxes, nets, jamming, lines, `simcore/voice_comms.hpp`, `simcore/radio_nets.hpp`, `docs/crew/12_COMMUNICATIONS.md`); the game opens on a missions menu (`data/scenarios/missions.json`, `simcore/missions.hpp`, `ASimMissionMenuHUD`: pick a mission, a vehicle and a seat; Esc pauses with restart, the missions, the debrief); next a person playing the missions through; the EM track has E0-E7 and E10 done (E6 without anti-radiation missiles: decoys, stand-off jamming, emission discipline; E5 the TV as an optical sensor and the command link as an emitter), E8 and E9 started) and `docs/HANDOFF.md`. Known gaps are listed in
`docs/ARCHITECTURE.md` → "Known limitations". Unreal can be driven by script: `tools/ue_remote.py`,
`tools/ue_cabin_check.py` (see `unreal/README.md`).

Modelling equipment from photographs: always follow `docs/MODELLING_FROM_PHOTOS.md` (skill `model-from-photos`): flatten the
photo to mm, trace in 3D with heights, overlap tests, then check the model rendered from the photo's own camera.

Multiplayer: the server simulates, clients mirror; players take crew seats (`docs/MULTIPLAYER.md`).

Enemy air command (roadmap phase 11, `docs/air_war/`): officers with characters plan the other side's raids on what their
intelligence believes and learn from what they think happened; raids through the battery's sector become scenarios the
runner plays; the game's missions menu starts a campaign (1-4) and its campaign screen (`ASimCampaignHUD`) runs it, saved
after every step; a first balance pass is done (`docs/air_war/09_BALANCE.md`); once our radars have come under ARM attack the
regiment orders the ARM drill and the raids carry it (E6). The war has a year (1985) and types the year they entered
service: the Pact fires Kh-22 cruise missiles now, NATO's Tomahawks (1986) and CALCMs (1988) come in a later war (A16).
Next: a person playing a campaign through.

**First person** (project owner, 18 Oct 2026; roadmap phase 12, `docs/command/`): the player's character knows only what
reaches them through people and means of signal. Intelligence, warnings and after-action reports are facts held by posts
of our side's chain of command (the national sector, the army, the division, the regiment, the battery, the town's
offices, placed on a map of a small city, a town, villages, works and countryside) and travel by in person, runners,
despatch riders, couriers, telephone, radio, the warning net, sirens, the post and the news (`simcore/command/`). Never
add an omniscient HUD panel: route new information through the pipeline. P1-P8 built (deliveries as scenes, the player
telling people, the enemy's intercept from the calls, the crew's knowledge, radio through the EM layer, several players'
posts, the district drawn, posts changing hands: `docs/command/07_IMPLEMENTATION_PLAN.md`); P9, the reports from the
records, built in first form (`simcore/campaign/report.hpp`: the combat report from the PU-12M's journal, board and
commander's log, the regiment's check against the radar company's log, a trust per unit). **The written record is the war's memory** (project owner, 22 Oct 2026;
`docs/command/01_PRINCIPLES.md` 9): after-action reports are compiled from what the crews wrote down (the
radiotelephonist's journal, `simcore/radio_journal.hpp`, then the board and the commander's log), never from the truth,
and the battery's and regiment's analysis and plans are built on those reports. Keep the record's accuracy carrying
through to the after-action loop in anything that builds reports, our side's picture or its planning.

Out of the vehicle (roadmap 8.14, `docs/crew/13_OUT_OF_THE_VEHICLE.md`): a crewman's body is always somewhere (seated,
inside, in a hatch, on the hull, on the ground) and moves only by the vehicle's real ways, leg by leg with their times
(`simcore/crew_mobility.hpp`, `data/vehicles/`): no teleport, for NPCs or the player. A control worked outside is a job
at a place; the man goes there to work it. Out of the vehicle he is off the intercom and heard only in earshot. The
drill master walks him out to a job and back in its ideal run (`DrillMaster::SetOutside`). The PU-12M's lines are laid
by the crew: a man pays the P-274M out to the line's far end (the job's place `line:<n>`) and the line works only once
laid (`ScenarioRunner::LineLaid`). The lessons know the walk: a step worked outside says where the learner must go,
and the instructor leads him there (`TutorialStatus::away`). At a road the crew gets on and off away from the traffic
(`ScenarioRunner::TrafficSide`). The Osa crew chief's compass survey is his walk out to the aiming circle and back
(the orientation data's `survey_procedure`, `orientation.hpp`): he tells the deputy the Qc once back in earshot, and out
there only a signal from the crew calls him back. Next: the NPCs' bodies mirrored to clients.

Voices (`docs/crew/12_COMMUNICATIONS.md`): inside a vehicle a seat hears the intercom, a radio or a telephone line only
through its headset and its box's selector (`simcore/voice_comms.hpp`); a net only while its set is on, warm, on the net's
numbered frequency and not jammed out (`simcore/radio_nets.hpp`). Route new speech through the callout bus's router, never
straight to every seat. The crew's talk (roadmap 8.15, `docs/crew/15_TALK.md`): the bus takes turns (a gap between
speakers, urgent lines break in, stale lines dropped, one caller per fact), stress is a level per crewman from what he
heard and saw, and the crew menu is a radial marking menu on Caps Lock (eight sectors in fixed places, the replies on U,
I, P). A new callout or order needs a place in its crew data's `"menu"` (`test_crew_talk.cpp` checks every seat's lines).
The emission regime (roadmap 8.16, `docs/crew/16_EMISSION.md`, `data/command/emission_regimes.json`): the regiment's
order or the battery commander's decision, said on the nets, told to each crew by its commander with the turns and
seconds of its bursts; the crew keeps only the regime it was told (`CrewRegime`), the commander orders by the one he heard
(`CommanderRegime`). Never read a regime straight from the scenario's doctrine in crew behaviour.

The helper (`docs/crew/14_HELPER.md`, `simcore/helper.hpp`; the missions menu's Helper, H in the cabin): it shows the
move the player's seat's NPC would make now, so it has no rules of its own. **Every NPC act at a seat goes through
`ScenarioRunner::NpcDoes(seat, SeatDuty)`, never a bare `NpcAt(seat)` check before the act** (`NpcAt` only chooses who
speaks and how fast); a direct act inside the NPC commander's think checks `!shadow_` (the shadow commander thinks in a
player commander's place and must change nothing). After changing the crew's behaviour, the consoles or the missions,
run `helper_coverage` and read its Gaps; `test_helper.cpp` counts the `NpcAt(` uses to make you look.

**The nation on the map** (prioritised 22 Oct 2026, roadmap N0-N6, **done 23 Oct 2026**): whole conflicts simulated
headless on the country of the briefing, so the player feels a small part of a large war. The air war's infrastructure
kinds and checked air defence systems, our order of battle (`simcore/command/oob.hpp`), the country's map
(`simcore/country_map.hpp`, `docs/COUNTRY_MAP.md`), the briefing's assets placed on it
(`data/air_war/theatre_pannonia_nation.json`, which extends the tuned theatre; mobile units move between their prepared
positions), a basic model for every equipment type, ours never drawn as the enemy's (`data/scenery/basic_models.json`).
The air war runs on the nation's theatre (`docs/air_war/09_BALANCE.md` §8): fire channels, warning posts, a played
raid's neighbours (drawn, and the regiment's other Osa batteries fighting in the battery fire model), cruise missiles by
the war's year, and a rebalance for a whole nation (a front's `force_scale`); **the game's new campaigns are fought on
it**. Unreal draws the whole country around the Kesmark range with every asset as basic models, and the campaign screen
shows it. `theatre_pannonia.json` stays the theatre the tests and the first balance pass tuned. Left to 13.8: the
ledgers and the factions' target choice re-read. N7 (23 Oct 2026), the air war as it was fought: weapons and attack
profiles with damage calibrated against history (`docs/air_war/10_WEAPONS.md`, `simcore/examples/air_war_bench.cpp`,
`09_BALANCE.md` sections 9-10), AWACS, tanker and ELINT orbits (`11_SUPPORT.md`), force build-ups (`12_FORCES.md`),
package sizes (`13_PACKAGES.md`), our supply lines on the country's roads and railways and the enemy's armed
reconnaissance (`simcore/air_war/logistics.hpp`, `14_LOGISTICS.md`), and the country's terrain: ranges, gorges, lakes and
marshes (`docs/COUNTRY_MAP.md`).

The battery's war (project owner, 22 Oct 2026; roadmap phases 13-21, `docs/campaign/`): after the nation on the map,
M2, a person's campaign playthrough and the pipeline's P1-P3, the systems the Osa and PU-12M crews, vehicles and battery fit into. **One enemy at a
time**, its doctrine and officers as built (`docs/air_war/`); the strategy layer stays thin (object roles, not an
economy; the regiment's tasking a rule table; the front moved by its units since track F). Every objective has a traceable why: the enemy's war
aims, its officers' target picking and its raids; our objects' roles, the regiment's order and the battery's task. A
battery mission starts with the regiment's combat order and runs as one continuous session to the report from the
records, with the enemy's packages entering it live. The battery's people have needs (sleep, food, morale) that change
their effective skill, and the player may play any fit member of the battery; between fights the battery's activities
hold people and equipment away from readiness, readiness is computed from where everyone is, and the war interrupts at
any time. The enemy's reach (depth, hours, air superiority) is game logic; our doctrine is a profile per adversary; and
command between the layers can break, so NPCs and the player improvise by sourced rules and then reconstitute.
**The player cannot significantly change the wider war**: they change two outcomes, measured in ledgers, the survival
of the assets they protect and the enemy's freedom of action (a battery that survives and threatens makes the enemy
add SEAD, fire ARMs, spend sorties, re-route and lose aircraft). Targets can be rebuilt, but rebuilding needs teams,
materials, transport and time, all of them targetable.
Phase 14 is begun ahead of its gate (30 Oct 2026): the regiment's task given on its net, its tasking (a rule table read
on our intelligence's estimates in the campaign's journal, a new task given into the running session by
`ScenarioRunner::GiveTask`: `simcore/campaign/regiment.hpp`), the battery commander's plan
timed to the ready-by, his preliminary and oral combat orders on the battery net, the march and the task kind's
formation, «к бою готова» up to the regiment as the task's readiness (`simcore/campaign/battery_task.hpp`,
`battery_plan.hpp`, `pu12m_mission_cover.json`). Phase 15.2-15.3 too, in first form: the battery's mission as one
session with an enemy's air campaign on its clock (`simcore/campaign/mission_session.hpp`, `mission_demo --war`), its
packages entering the running session live (`simcore/air_war/package_injector.hpp`: the runner's `AddTargets`,
`AddEmGear`, `AddTriggers`; `LivePackages` runs the campaign in step with the session), held through a whole day
(`mission_demo --hours 24`). With no air picture on the post's board after a raid warning, the battery commander orders
the launchers to search on their own in their sectors (roadmap 21.5, `ScenarioRunner::OrderSearchOwn`).

**Track G, the ground** (project owner, 28 Oct 2026; `docs/campaign/11_BATTERY_LIFE_PROPOSAL.md`, the roadmap's
track G; not gated): command, battery orders and movement first. Routes are derived from the map's data at load and
asked between anchors (places, positions) by id, never authored or saved as node numbers, so a changed map needs no
navigation edits; changes in play are overlays, which a unit plans on only once it believes them. A battery order is
carried by a means to a person who carries it out: no vehicle acts on an order its commander did not hear. Every
element of the battery moves in Unreal, and everything within the player's visual range appears there, where simcore
has it. Wear and maintenance (G6) and the battery's day (G7) are on hold; the TZM and its crew are not playable.
Every other phase (G0-G5, G8-G11) is built in a first form (29 Oct 2026; what each left is in the proposal's "Left:"
lines): a scenario's `"nav"` gives the session the roads, its places and its movers, and a march order may name a place
(`to_place`) that the battery commander plans on the roads when he gives it. Next: a person playing M-G's night march.

**Track R, the review** (project owner, 2 Nov 2026; `docs/DEVELOPMENT_ROADMAP.md` track R; priority, not gated):
damage states drawn by chance per burst (none, light, moderate, heavy, destroyed; flak bursting 5-50 m away mostly does
nothing) with the AAA's accuracy falling with range, both in simcore and drawn from the same bursts; nothing drawn under
the map; aircraft flown with bank, pitch and roll; a more realistic map (free Fab, Epic and online assets as
placeholders, each with its licence recorded); the time of day in the missions menu; pilots who break, dive and jettison
on a warning, a sighted launch or flak, or damage. Damage seen by our crews goes into the records as they saw it.

**Track F, the fronts** (project owner, 19 Nov 2026; `docs/DEVELOPMENT_ROADMAP.md` track F, `docs/fronts/`): the map is the
only source of truth for where things are (every placed object takes its place from the country map by id); the enemy's air
front starts at the border and moves forward in a sector it judges low risk and high reward (degraded air defence, our
mobile ground assets destroyed), low-level attack and helicopters first; the ground war's units make the front, attacking
or withdrawing (as a unit or dispersed) by their officers' logic, and the ground situation sets our national air defence's
sector priorities. The player learns the front only through reports.

Early-warning sources: the PU-12 is the main source but the crew can switch to others (`simcore/c2_sources.hpp`, `data/command/c2_sources.json`; roadmap 7.8, E4).
