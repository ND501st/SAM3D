# SAM3D — Handoff for the next AI model

*Written 30 Sep 2026 (Australia/Sydney) from the SAM project notes, previous chats, and a read of the repo at commit `5cb032e`. Updated 1 Oct 2026 for the state at `885f568` (branch `feature/unreal-bringup`). Read this first, then the repo's `CLAUDE.md`, `docs/DEVELOPMENT_ROADMAP.md` and `docs/ARCHITECTURE.md`.*

---

## 1. What the project is

**SAM3D** is a first-person, realistic 3D surface-to-air missile (SAM) game in **Unreal Engine 5**, rebuilding the console and procedure mechanics of **SAM Simulator v930.2** (a free hobbyist game) **with its creator's blessing**. The player drives a SAM vehicle, picks a position, deploys and stows it, works the operator consoles as 3D cabins, runs an engagement, and gives orders to NPC crew who operate the *same* controls the player does.

- **First system / vertical slice:** 9K33 Osa — drive, deploy, 2 NPC crew, one complete engagement. Estimate: 6–12 months part-time, solo.
- **Second system:** ZSU-23-4 Shilka. Fixed sites (S-75, S-125, S-200) later; they don't fit "drive around" and need big crews.
- **Developer:** Nicholas (GitHub `ND501st`), solo, part-time, Melbourne (Australia/Sydney time zone).

## 2. Where things live

| Thing | Location |
|---|---|
| Repo | **`ND501st/SAM3D`** (private) — https://github.com/ND501st/SAM3D |
| Branches | `main` is still at the initial commit **`5cb032e`**. All later work is on **stacked branches**, each built on the one before, all pushed, **no PRs opened yet** (`gh` isn't signed in on the PC): `fix/windows-build-and-verify` → `feature/equipment-and-crew-orders` → `feature/scenarios` → `feature/engagement` → `feature/unreal-bringup` (newest). Merge them in that order. |
| PC checkout | `C:\dev\SAM3D` |
| Mac checkout | Any non-synced folder |
| Project notes | claude.ai project "SAM" → `claude/SAM 3D - project plan and dev setup.md` |
| Local reference material | `reference/` in the repo (git-ignored): SAM Sim install, manuals, Ghidra project, photos |

**Keep the repo out of OneDrive/iCloud.** Sync clashes with `.git` and Unreal's `Intermediate/`/`DerivedDataCache/`, and OneDrive paths are too long for Unreal. Git is the only sync between machines.

## 3. Machines and tooling

| Machine | Role | Key software |
|---|---|---|
| **Windows PC** (32 GB, RTX 3060 Ti) | Main workstation | UE5 + Cesium for Unreal, VS 2022 (Game dev with C++), Git + LFS, Claude Code, Ghidra, Python, Blender, SAM Sim reference install, Sunshine + OpenSSH + Tailscale. Installed by `tools/setup-pc.ps1`. |
| **Mac M3 Pro** (16 GB) | Remote access + native simcore/prototype work | Moonlight (streams the PC), Tailscale, VS Code Remote-SSH, Claude Code over SSH. Installed by `tools/setup-mac.sh`. |

**State as of 1 Oct:** the PC is set up. **UE 5.7.4 is at `D:\UE_5.7`** with Cesium for Unreal 2.29.1. `.\tools\verify.ps1` passes on Windows (from a VS developer shell), and the editor target builds from the command line without opening the editor (close the editor first):

```
D:\UE_5.7\Engine\Build\BatchFiles\Build.bat SAM3DEditor Win64 Development -Project=C:\dev\SAM3D\unreal\SAM3D\SAM3D.uproject -WaitMutex -NoHotReload
```

So Claude can compile Unreal C++ itself. It still can't run the editor or play the level; the user does that.

**Claude chat ↔ computers:** a claude.ai chat links to whichever machine's Claude desktop app last sent a message in it. That machine's folders must be granted before Claude can see them, and the other machine isn't reachable from that chat. Claude cannot run the Unreal editor; after Unreal C++ changes, ask the user to build.

## 4. Repo map

| Path | Contents |
|---|---|
| `simcore/` | **Header-only C++17 sim core** — engine-agnostic. Physics-side headers: `vec3`, `geo` (WGS-84/ECEF/ENU `LocalFrame`), `atmosphere` (ISA), `terrain` (`FlatTerrain`, `GridTerrain`), `radar`, `aircraft`, `missile`, `world`, `acmi`. Game-side headers: `panel` (`ControlPanel`: switches, guarded/stepwise controls, wheels, push-buttons), `equipment` (subsystems off/starting/ready/stopping/fault, interlocks, inputs, latching modes), `procedures` (`ProcedureTracker`, `CrewMember` with order queue), `json` + `console_data` (load consoles from JSON), `debrief` (`SessionLog`), `scenario` + `scenario_runner` (deterministic scenario playback), `engagement` (gameplay-level radar picture, lock/break-lock, outcome resolver). |
| `simcore/tests/` | 70 dependency-free tests (`SC_TEST`, `SC_EXPECT_NEAR`) + `header_check.cpp` (enforces the macro-name ban). |
| `simcore/examples/` | `engagement_demo` (writes `.acmi`), `procedure_demo` (Osa console session + debrief), `scenario_demo` (plays `osa_practice.json`). The last two run under CTest. |
| `prototype/` | Pure-Python mirror of the physics side of simcore (13 tests). The console/scenario/engagement-model side has no Python mirror. |
| `unreal/SAM3D/` | UE 5.7 C++ project, builds with no warnings. `USimWorldSubsystem` (runs a `ScenarioRunner` at 100 Hz, console API for Blueprints, Cesium terrain sampling, ENU↔Unreal through the Cesium georeference), `ASimScenarioDirector` (loads a scenario, georeference, spawns/possesses the vehicle, on-screen status + debrief), `ASimOsaVehicle` (kinematic placeholder Osa, number-key crew orders), `ASimEntityActor` (target/missile proxy). See `unreal/README.md`. |
| `data/` | `procedures/osa_ak.json` (Osa-AK consoles and procedures), `systems/osa.json` (cited system data; radar and missile sections deliberately empty), `scenarios/osa_practice.json`, `gameplay/osa_ak.json` (game-balance numbers). |
| `docs/` | `DEVELOPMENT_ROADMAP.md` (task order), `PROJECT_PLAN.md`, `ARCHITECTURE.md`, `DEV_SETUP.md`, `REVERSE_ENGINEERING.md`, `SOURCES.md`, `re/osa.md` (Osa notes from SAM Simulator's manual, with page references) |
| `tools/` | `setup-pc.ps1`, `setup-mac.sh`, `verify.ps1`, `verify.sh` |
| `.github/workflows/ci.yml` | simcore build+test on Ubuntu/Windows/macOS; prototype pytest on Python 3.12 |
| `CLAUDE.md` | Conventions for Claude Code — **authoritative; read it.** |

## 5. Commands

```bash
# simcore (Windows: run from "Developer PowerShell for VS 2022")
cmake -S simcore -B simcore/build -DCMAKE_BUILD_TYPE=Release
cmake --build simcore/build --config Release
ctest --test-dir simcore/build -C Release --output-on-failure
./simcore/build/engagement_demo out/demo.acmi

# Python prototype
python -m pytest prototype -q
python prototype/scripts/run_demo.py out/demo_py.acmi

# Everything
./tools/verify.sh        # Mac/Linux
.\tools\verify.ps1       # Windows
```

## 6. Architecture in one paragraph

**simcore is the game; Unreal is the presentation.** Everything that decides an outcome (detection, tracking, flight, fuzing, procedure correctness, crew actions) lives in simcore, which is deterministic and runs without Unreal. Unreal does rendering, input, cabins, animation, audio, UI and terrain data. `USimWorldSubsystem::Tick` accumulates frame time and calls `World::Step(0.01)` in a loop — never step simcore from actor ticks. Frames: simcore uses **ENU metres (x=E, y=N, z=U)**; Unreal uses **cm, X=N, Y=E, Z=U (left-handed)**; convert only via `EnuToUnreal`/`UnrealToEnu`. ENU `z` ≠ altitude far from origin (≈785 m off at 100 km) — use `LocalFrame::AltitudeOf`. Player cabin clicks and NPC `CrewMember`s act on the **same** `simcore::ControlPanel` through `ProcedureTracker` (accepts correct steps, refuses early ones during warm-up, counts deviations).

## 7. Hard rules (from CLAUDE.md — do not break)

**simcore**
- Header-only, C++17, standard library only. No Unreal types, no third-party deps.
- No exceptions, no RTTI (`throw`, `dynamic_cast`, `typeid` banned).
- Don't use identifiers Unreal/Windows define as macros: `check verify ensure PI TEXT IN OUT ERROR near far small interface min max DELETE`.
- SI units with suffixes (`_m`, `_s`, `_mps`, `_rad`, `_db`); degrees only at API edges.
- Warnings are errors (`-Wall -Wextra -Wshadow -Wconversion -Werror`, MSVC `/W4 /WX`).
- Every new model/behaviour gets a test. **Keep `prototype/` in sync** with simcore where both implement a model.

**Realism and data**
- **Public and declassified sources only.** Never request, use or add classified or export-controlled material.
- Every value in `data/systems/*.json` needs `source` and `confidence` (`high/medium/low/estimate`). Code demos/tests use **generic placeholder numbers**, labelled, never real system data.
- **Never commit SAM Simulator's art, sounds, data or `sam.exe`.** They stay in `reference/`.

**Unreal**
- Epic coding standard (tabs, `F/U/A/E` prefixes, `b` bools), categories `SAM3D|...`.
- Game logic that affects the sim goes in simcore.

## 8. Status and roadmap

Task-level detail and order: `docs/DEVELOPMENT_ROADMAP.md`. Done as of `885f568`:

| Phase | State |
|---|---|
| 0 Environment | Windows build fixed, `verify.ps1` passes, Unreal builds. **Open:** `gh auth login`, open the five stacked PRs, confirm CI green. |
| 1 Consoles and procedures | Done: equipment/interlocks, guarded/stepwise controls, wheels, push-buttons, JSON consoles, Osa power-up/deploy/stow, crew order queue, session debrief. |
| 2 Scenarios and debrief | Done: scenario JSON + loader, deterministic `ScenarioRunner`, ACMI track, `osa.json` (crew/vehicle/deploy/power only). |
| 3 Engagement outcome | Done at **gameplay level** (option 1): search picture, lock/break-lock, TV tracking, outcome resolver with every factor in the debrief, target breakaway. |
| 4 Unreal bring-up | Mostly done: Cesium georeference + terrain sampled into `GridTerrain`, kinematic placeholder Osa, antenna fold follows sim state. **Open:** Chaos Vehicles (needs a mesh with wheel bones), real model/animation, performance check. |
| 5 3D cabin | `feature/cabin`: cabin layout data + loader (`cabin.hpp`, `data/cabins/osa_ak.json`), player actions judged by a `ProcedureTracker`, Unreal cabin component (placeholder shapes, lamps, first-person interaction, HUD). **Open:** scopes as render targets (5.3), audio, real interior. |
| 6 Crew and mission | Not started (crew orders are number keys for now). |

**Known deliberate simplifications** (ARCHITECTURE.md): aircraft constant speed in straight ENU lines; the old physics missile (`missile.hpp`, used only by `engagement_demo`) is PN-only with perfect target knowledge, so it shows 0.0 m misses; the scenario path resolves launches with the gameplay model instead of flying a missile; radar physics is a single-look SNR threshold.

## 9. The physics-depth decision (resolved)

On 30 Sep a session declined to build detailed missile-performance models (guidance noise, autopilot lag, Mach-dependent drag, probabilistic warhead kill) and offered an abstracted, hand-tuned outcome resolver instead. The user took that route: **phase 3 option 1** (`e5271e7`). Outcomes come from game-balance numbers in `data/gameplay/` (operator tracking, target class, zone, manoeuvre, crew mistakes), not from weapon physics. Detailed, calibrated weapon-performance physics stays unscheduled; if it comes up again, treat it as its own discussion.

## 10. SAM Simulator findings

- Covers 7 systems: S-75 Dvina, S-75M Volkhov, S-125 Neva, 2K11 Krug, 9K33 Osa, S-200 Vega, ZSU-23-4 Shilka. ~1,600 data files, ~3.6 GB installed.
- `.dat` files (prefixed `volhov`, `krug`, `neva`, `osa`, `dvina`, `vega`, `silka`, `world`) are **only JPEG/BMP panel art and WAV audio** — no mechanics.
- `sam.exe`: ~100 MB PE32, **VB6 compiled to native x86** (not P-code), 67 unnamed forms (`Form1`–`Form67`) + `Module1`, ~1,300 event handlers; nearly all logic is in the forms. Deps: MSVBVM60, COMCTL32, MSWINSCK, MCI32, MSSTDFMT; no DirectX. Installer is a VB6 P&D Wizard package (`SAM1–7.CAB`).
- **Biggest shortcut: ask the creator for the VB6 source (`.frm`/`.bas`) and written permission to reuse art.** Reverse-engineering (VB Decompiler Lite for form/handler map → Ghidra for pseudo-C, timer handlers first) is the fallback, and only after confirming the creator is happy with decompilation. Workflow in `docs/REVERSE_ENGINEERING.md`; findings go in `docs/re/<system>.md`.
- Tacview isn't needed for the core game; ACMI export already exists for debriefs/debugging.

**Open questions for the creator** (none answered yet):
1. Will he share the VB6 source, or notes on how the consoles/procedures behave?
2. Written permission to reuse or reference panel art, sounds and procedures?
3. Operator manuals or veteran contacts he used?

## 11. Realism approach (as planned by the user)

| Layer | Source of truth |
|---|---|
| Consoles, switch order, procedures | SAM Simulator's behaviour + public Warsaw Pact operator manuals + veteran accounts |
| Physics | Public textbooks (Skolnik, Mahafza, Zarchan, Fleeman — see `docs/SOURCES.md`) |
| Per-system numbers | Public/declassified figures, each cited in `data/systems/*.json` |
| Validation | Historical engagements (1960 U-2, Vietnam SA-2, 1999 F-117) |

Physics depth: see §9 (gameplay-level outcomes chosen).

## 12. Model plan the user set

| Work | Model |
|---|---|
| Architecture, sim-core maths, validation, hardest bugs | Claude Fable 5.1 or Opus 5.5 |
| Reading decompiler output, research synthesis, big refactors | Claude Opus 5.5 |
| Everyday Unreal C++/Blueprints, UI, crew AI, tests | Claude Sonnet (docs say "Sonnet 5"; current is Sonnet 5.5) |
| Bulk data entry, log triage, quick lookups | Claude Haiku 4.5 |

## 13. Next steps

1. **User:** merge the stacked PRs ND501st/SAM3D#1–#5 in order (merge commits or rebase, not squash), then the cabin PR.
2. **Phase 5.3:** SOC/SSC scopes and the TV picture as render targets fed by the engagement model, placed on the cabin panels.
3. **Phase 6:** crew order menu calling the existing order API (replaces the number keys), then the scripted mission end to end.
4. Remaining phase 4: Chaos Vehicles once there's an Osa mesh with wheel bones; performance check of the 100 Hz step and terrain sampling.
5. Draft the message to the SAM Sim creator with the three open questions (§10).

## 14. Working conventions with this user

- Wants PRs on branches for review, not pushes to `main`.
- Commit/PR attribution: follow the session's instructions; the repo's `CLAUDE.md` takes precedence.
- Wants the C++ and Python versions kept matching, with tests, for any model change.
