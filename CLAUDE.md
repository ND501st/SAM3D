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
| `simcore/include/simcore/command/` | **Our side's chain of command and the information pipeline** (`simcore::command`): who is who and where, the means of signal, how a fact reaches the player's character (first person: nothing else does), the air war's news fed in. See `docs/command/`. |
| `simcore/include/simcore/scenery/` | **Ultra-basic models** (`simcore::scenery`): every aircraft, vehicle, site and building as a few primitives, from `data/scenery/basic_models.json`; drawn by Unreal's `FSimBasicModels`. See `docs/scenery/`. |
| `simcore/include/simcore/air_war/` | **Enemy air command** (`simcore::airwar`): the other side's air staff as people, its intelligence picture, the planning cycle, abstract resolution, raids generated as scenarios and the mission bridge back. See `docs/air_war/`. |
| `simcore/tests/` | Dependency-free unit tests (`SC_TEST`, `SC_EXPECT_NEAR`). |
| `simcore/examples/` | `engagement_demo`: one radar vs one target, writes a Tacview `.acmi`. |
| `prototype/` | Pure-Python mirror of simcore for fast experiments (runs on the Mac too). |
| `unreal/SAM3D/` | UE5 C++ project. Includes simcore through `SimCoreIncludes.h` only. |
| `data/systems/` | Per-system parameters. **Every value needs a source.** `pu12m_radios.json`: the PU-12M's R-111 and R-407 as sets (`simcore/radio_sets.hpp`). |
| `data/procedures/` | Per-system consoles: controls, subsystems, procedures (loaded by `console_data.hpp`). Same sourcing rules. |
| `data/scenarios/` | Scenarios: unit, crew, targets, triggers (loaded by `scenario.hpp`, played by `scenario_runner.hpp`). |
| `data/cabins/` | 3D cabin layouts: stations, panels, control and lamp positions, the floor a crewman walks on up from his seat (`walk`), the display each hand wheel is worked against (`watch`) (loaded by `cabin.hpp`). Presentation data; lamps need sources. |
| `data/vehicles/` | Each vehicle as a body to climb (`simcore/crew_mobility.hpp`): where each seat stands up, the hatches (and when the drill keeps them shut), the roof and its keep-outs, the ways down by the wheels, the places round it and the jobs done there, the line-up for К машине. Cited like `data/systems/`. See `docs/crew/13_OUT_OF_THE_VEHICLE.md`. |
| `data/gameplay/` | Game-balance numbers for the gameplay engagement model (`engagement.hpp`). Not system data. |
| `data/command/` | Command posts (PU-12M plotting board): cited like `data/systems/`; loaded by `pu12.hpp`, named by a scenario's `command_post`. Also our side's chain of command, its places and the means of signal (`chain_pannonia.json`, `places_pannonia.json`, `signal_means.json`; `simcore/command/`), and the talk tables ПТ-1/ПТ-2 (`talk_tables.json`, `simcore/talk_tables.hpp`). |
| `data/crew/` | The crew: seats, orders, callouts (Russian and English), tasks, world-fact rules (`<system>_crew.json`), skill tiers (`skill_levels.json`), the crewman's body (`body.json`), his pace and his voice's carry out of doors (`walking.json`), the crew's voices (`voices.json`) and the voice each seat's lines are recorded in (`recorded_voices.json`, read by `tools/voice_lines.py`). See `docs/crew/`. |
| `data/air_war/` | The enemy air command: the fictional theatre, the four adversaries (rebels, neutral neighbour, Warsaw Pact, NATO) with doctrine, intelligence, politics, aircraft, squadrons and officers, and the quirk catalogue. Balance numbers in `data/gameplay/air_war.json`. |
| `data/tutorials/` | The lessons (`simcore/tutorial.hpp`): a first lesson and one per seat of the Osa and of the PU-12M, each step one action on a lit control, shown over a mission named in `missions.json` (`"tutorial"`). See `docs/TUTORIALS.md`. |
| `data/scenery/` | Presentation data: the ultra-basic models of every asset (`basic_models.json`: archetype, size, colour per type; the air war's aircraft ids). Placeholders until real models exist. |
| `data/maps/` | Generated maps (`simcore/terrain_map.hpp`): ground, rivers, roads, towns, woods, military sites, ground out to 150 km. `kesmark_range.json` is the 40 km test map. See `docs/TEST_RANGE.md`. `pannonia.json` is **the whole country** (`simcore/country_map.hpp`, `docs/COUNTRY_MAP.md`): ground at 1 km, borders, regions, rivers, and roads, railways and pipelines as networks with bridges that can be cut; the Kesmark range is a district of it and must keep agreeing (`tests/test_country_map.cpp`). |
| `data/weather/` | Weather presets a scenario names (`"weather"`): winds, cloud layers, the air (`simcore/weather.hpp`). |
| `data/em/` | EM libraries: radars, receivers, jammers, radios, threat libraries. Real systems cite every value (`"provenance": "per_value"`); `generic.json` is placeholder. |
| `docs/` | Plan, roadmap, architecture, sources (with tiers), research (`docs/research/`), briefing, dev setup, reverse-engineering notes. |
| `docs/campaign/` | **The battery's war** (planned, roadmap phases 13-21): the enemy's war aims and target picking, what we defend and why, how a battery mission starts (the regiment's order, the battery commander's sequence and working map), what is attacked, defended, ambushed, jammed and reconnoitred around the battery and why, the systems that run it as one continuous mission, the crew as people, the battery's day between fights, the enemy's reach, doctrine by opponent, and command under strain. Start at its README. |
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
./simcore/build/em_demo data out/em_demo.json out/em_demo.acmi   # EM layer MVP: timeline + 11 checks
./simcore/build/air_war_demo nato 5 data out/air_war 1          # enemy air command: a campaign, its raids played as scenarios, the journal
python tools/air_war_view.py out/air_war/air_war_nato.json       # that campaign as a page: the enemy's mind beside what our side learned
./simcore/build/air_war_balance data 20 7                         # every adversary over 20 seeds and two battery postures (docs/air_war/09_BALANCE.md)
./simcore/build/air_war_balance --theatre theatre_pannonia_nation.json data 10 7   # the same on the whole nation (N4); the demo takes --theatre too
./simcore/build/air_war_bench data 400            # weapons, attacks and losses against history (docs/air_war/09_BALANCE.md section 9)
./simcore/build/em_bench data 500 60 24           # EM scaling benchmark (aircraft, SAM radars, ESM sites)

# Play the game (docs/PLAYING.md): the missions menu, or straight into the first lesson or a seat's lesson
.\tools\play.ps1 [-Tutorial | -Lesson senior_operator] [-Build]

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
add an omniscient HUD panel: route new information through the pipeline. Next: deliveries as scenes in the world, the
player telling people. **The written record is the war's memory** (project owner, 22 Oct 2026;
`docs/command/01_PRINCIPLES.md` 9): after-action reports are compiled from what the crews wrote down (the
radiotelephonist's journal, `simcore/radio_journal.hpp`, then the board and the commander's log), never from the truth,
and the battery's and regiment's analysis and plans are built on those reports. Keep the record's accuracy carrying
through to the after-action loop in anything that builds reports, our side's picture or its planning.

Out of the vehicle (roadmap 8.14, `docs/crew/13_OUT_OF_THE_VEHICLE.md`): a crewman's body is always somewhere (seated,
inside, in a hatch, on the hull, on the ground) and moves only by the vehicle's real ways, leg by leg with their times
(`simcore/crew_mobility.hpp`, `data/vehicles/`): no teleport, for NPCs or the player. A control worked outside is a job
at a place; the man goes there to work it. Out of the vehicle he is off the intercom and heard only in earshot. Next:
the drill master knowing the walk, wire laying.

Voices (`docs/crew/12_COMMUNICATIONS.md`): inside a vehicle a seat hears the intercom, a radio or a telephone line only
through its headset and its box's selector (`simcore/voice_comms.hpp`); a net only while its set is on, warm, on the net's
numbered frequency and not jammed out (`simcore/radio_nets.hpp`). Route new speech through the callout bus's router, never
straight to every seat.

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
economy; the front as data; the regiment's tasking a rule table). Every objective has a traceable why: the enemy's war
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

Early-warning sources: the PU-12 is the main source but the crew can switch to others (`simcore/c2_sources.hpp`, `data/command/c2_sources.json`; roadmap 7.8, E4).
