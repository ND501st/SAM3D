# CLAUDE.md — SAM3D

A first-person, realistic 3D surface-to-air missile (SAM) simulator, rebuilding the mechanics of
SAM Simulator v930.2 (free, by a single hobbyist developer, used with his blessing) in Unreal Engine 5.
The player drives a SAM unit, deploys it, runs engagements, and gives orders to NPC crew.
The first system is the **9K33 Osa**.

## Repo map

| Path | What |
|---|---|
| `simcore/` | **Header-only C++17 sim core** (radar, missile, aircraft, procedures, crew, world). Engine-agnostic. |
| `simcore/tests/` | Dependency-free unit tests (`SC_TEST`, `SC_EXPECT_NEAR`). |
| `simcore/examples/` | `engagement_demo`: one radar vs one target, writes a Tacview `.acmi`. |
| `prototype/` | Pure-Python mirror of simcore for fast experiments (runs on the Mac too). |
| `unreal/SAM3D/` | UE5 C++ project. Includes simcore through `SimCoreIncludes.h` only. |
| `data/systems/` | Per-system parameters. **Every value needs a source.** |
| `docs/` | Plan, architecture, sources, dev setup, reverse-engineering notes. |
| `reference/` | Git-ignored. Original SAM Sim files, manuals, Ghidra project. |
| `tools/` | Setup and verify scripts for the PC (PowerShell) and Mac (bash). |

## Commands

```bash
# simcore: build + test (any OS; on Windows run from a "Developer PowerShell for VS 2022")
cmake -S simcore -B simcore/build -DCMAKE_BUILD_TYPE=Release
cmake --build simcore/build --config Release
ctest --test-dir simcore/build -C Release --output-on-failure
./simcore/build/engagement_demo out/demo.acmi     # Windows: simcore\build\Release\engagement_demo.exe

# Python prototype
python -m pytest prototype -q
python prototype/scripts/run_demo.py out/demo_py.acmi

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
- Every system parameter in `data/systems/*.json` needs `source` and `confidence`. Code demos and tests use
  **generic placeholder numbers**, clearly labelled, never real system data.
- Consoles and procedures follow SAM Simulator's behaviour, plus public operator manuals. Physics comes from textbooks
  (`docs/SOURCES.md`). Validate against historical engagements.
- **Never commit the original SAM Simulator art, sounds, data or `sam.exe`.** They live in `reference/` (git-ignored).

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

See `docs/PROJECT_PLAN.md` → "Roadmap". Known simplifications are listed in `docs/ARCHITECTURE.md` → "Known limitations".
