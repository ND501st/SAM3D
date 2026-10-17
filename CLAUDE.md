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
| `simcore/tests/` | Dependency-free unit tests (`SC_TEST`, `SC_EXPECT_NEAR`). |
| `simcore/examples/` | `engagement_demo`: one radar vs one target, writes a Tacview `.acmi`. |
| `prototype/` | Pure-Python mirror of simcore for fast experiments (runs on the Mac too). |
| `unreal/SAM3D/` | UE5 C++ project. Includes simcore through `SimCoreIncludes.h` only. |
| `data/systems/` | Per-system parameters. **Every value needs a source.** |
| `data/procedures/` | Per-system consoles: controls, subsystems, procedures (loaded by `console_data.hpp`). Same sourcing rules. |
| `data/scenarios/` | Scenarios: unit, crew, targets, triggers (loaded by `scenario.hpp`, played by `scenario_runner.hpp`). |
| `data/cabins/` | 3D cabin layouts: stations, panels, control and lamp positions (loaded by `cabin.hpp`). Presentation data; lamps need sources. |
| `data/gameplay/` | Game-balance numbers for the gameplay engagement model (`engagement.hpp`). Not system data. |
| `data/command/` | Command posts (PU-12M plotting board): cited like `data/systems/`; loaded by `pu12.hpp`, named by a scenario's `command_post`. |
| `data/crew/` | The crew: seats, orders, callouts (Russian and English), tasks, world-fact rules (`<system>_crew.json`), skill tiers (`skill_levels.json`), the crewman's body (`body.json`) and the crew's voices (`voices.json`). See `docs/crew/`. |
| `data/em/` | EM libraries: radars, receivers, jammers, radios, threat libraries. Real systems cite every value (`"provenance": "per_value"`); `generic.json` is placeholder. |
| `docs/` | Plan, roadmap, architecture, sources (with tiers), research (`docs/research/`), briefing, dev setup, reverse-engineering notes. |
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
./simcore/build/em_bench data 500 60 24           # EM scaling benchmark (aircraft, SAM radars, ESM sites)

# Python prototype and tools
python -m pytest prototype tools -q
python prototype/scripts/run_demo.py out/demo_py.acmi
python tools/panel_sheet.py                       # draw the cabin panels flat: out/panel_sheets/osa_ak.html
python tools/check_citations.py                   # every data source registered in docs/SOURCES.md, none tier 5
python tools/button_map.py                        # flow chart of every control and what's not built: out/button_map/osa_ak.html
python tools/crew_map.py                          # every crew action and how it connects: docs/crew/generated/osa_ak_action_map.md
python tools/pu12m_cabin.py                       # write data/cabins/pu12m.json from the PU-12M interior's trace (after build_pu12m_interior.py)
./simcore/build/crew_balance data 10              # every crew scenario at every skill tier over 10 seeds (docs/crew/11_*)
./simcore/build/crew_pose_dump data out/crew_poses.json   # the crew's bodies posed; draw them: tools/blender/render_crew_poses.py
python tools/em_view.py                           # EM overlays in a browser from em_demo's JSON: out/em_view.html
python tools/photo_trace.py rectify tools/traces/r123m_front.json PHOTO   # flatten a reference photo to mm (docs/MODELLING_FROM_PHOTOS.md)

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

See `docs/DEVELOPMENT_ROADMAP.md` (phase 6, the accuracy pass, done; phase 7, the battery and PU-12, done in simcore, and the PU-12M command post built through its plan's P0-P7 (console, crew, information chain, battery commander, exterior, interior and cabin, multiplayer: `docs/PU12_DEVELOPMENT.md`); phase 9, threats (jamming, method Н, target behaviour), done in simcore; phase 8, crew and teamwork, re-planned around the crew pillars (`docs/crew/09_IMPLEMENTATION_PLAN.md`): 8.0-8.12 done in a first pass (8.10 bodies and voices, 8.12 a headless balance pass); next a person playing the missions on the map, then the PU-12M as a seat; the EM track is at E10) and `docs/HANDOFF.md`. Known gaps are listed in
`docs/ARCHITECTURE.md` → "Known limitations". Unreal can be driven by script: `tools/ue_remote.py`,
`tools/ue_cabin_check.py` (see `unreal/README.md`).

Modelling equipment from photographs: always follow `docs/MODELLING_FROM_PHOTOS.md` (skill `model-from-photos`): flatten the
photo to mm, trace in 3D with heights, overlap tests, then check the model rendered from the photo's own camera.

Multiplayer: the server simulates, clients mirror; players take crew seats (`docs/MULTIPLAYER.md`).

Early-warning sources: the PU-12 is the main source but the crew can switch to others (`simcore/c2_sources.hpp`, `data/command/c2_sources.json`; roadmap 7.8, E4).
