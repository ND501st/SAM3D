# Development roadmap

How the project gets built, in order. Complements `PROJECT_PLAN.md` (goals and milestones) with the concrete sequence,
what blocks what, and how each task is finished. Written 30 Sep 2026.

**Status (1 Oct 2026, `885f568`):** phases 1-3 done; phase 4 done except Chaos Vehicles, the real model and the
performance check; phase 0 open only for `gh auth login`, the stacked PRs and CI. Next: phase 5. Details in
`docs/HANDOFF.md` §8.

## Principles

1. **simcore is the game, Unreal is the presentation.** Build and test behaviour in simcore first; Unreal only displays it.
2. **Procedures and consoles are the heart of the game.** SAM Simulator is about operating the system correctly, so
   operator procedures, interlocks, crew and scenarios come before any deeper physics.
3. **Vertical slice first.** One system (Osa), one complete engagement, before a second system.
4. **Small PRs on branches**, one topic each. Never commit to `main` directly.
5. **Definition of done for every task:** builds with warnings-as-errors, tests added, `tools/verify.ps1` passes, docs
   updated if behaviour or data format changed. Unreal changes: the user builds and plays it; Claude can't run the editor.
6. **Public and declassified sources only.** Every data value has `source` and `confidence`. Code and tests use labelled
   placeholder numbers. SAM Simulator's art, sounds and `sam.exe` never enter git.

## Phase 0 - Environment and first contact (now)

| # | Task | Blocked by |
|---|---|---|
| 0.1 | Unreal Engine install finishes (new Epic account) | download |
| 0.2 | `gh auth login`, push `fix/windows-build-and-verify`, open PR | user sign-in |
| 0.3 | Decide whether `docs/HANDOFF.md` and this file get committed | - |
| 0.4 | Switch `SAM3D.uproject` to the installed engine, generate project files, build Development Editor | 0.1 |
| 0.5 | Fix whatever the first-ever Unreal compile reports; play the demo level (see `unreal/README.md`) | 0.4 |
| 0.6 | Confirm CI is green on GitHub (Ubuntu, Windows, macOS, Python) | 0.2 |

**Exit:** demo engagement plays in the editor, CI green, the PR is merged.

## Phase 1 - Console and procedure system (no Unreal needed, start immediately)

Why first: it is the core of the game, it can be tested headless, and it does not depend on the creator's answers
(placeholder procedures are fine until real ones arrive).

| # | Task | Notes |
|---|---|---|
| 1.1 | Data-driven panels and procedures: JSON loader for `ControlDef`/`Procedure` | no third-party deps; a tiny hand-written parser, or a generated header |
| 1.2 | Interlocks and dependencies ("switch A needs B"), warm-up timers, states (off / warming / ready / fault) | extends `ControlPanel` |
| 1.3 | Osa power-up -> deploy -> ready -> stow state machine, using generic labelled placeholder steps | real steps replace placeholders later |
| 1.4 | Crew order API: orders queue, priorities, reaction-time and error model, status reports back to the player | builds on `CrewMember` |
| 1.5 | Score/deviation reporting: what the player or crew got wrong, and when | feeds the debrief |
| 1.6 | Tests for each of the above; extend the demo to print a procedure run | |

**Exit:** a headless scenario where a crew member powers up and stows the Osa from an order, with a correct-step and
wrong-step test for every interlock.

## Phase 2 - Scenarios and debrief (simcore, still no Unreal)

| # | Task |
|---|---|
| 2.1 | Scenario file format (JSON): origin, terrain, own unit, targets with waypoints, triggers, win/lose conditions |
| 2.2 | Scenario loader + runner in simcore, with deterministic seeds |
| 2.3 | Debrief report (text + ACMI): detections, launches, outcomes, procedure deviations |
| 2.4 | `data/systems/osa.json` with cited, rated values, filled only from public sources |

## Phase 3 - Engagement outcome model (needs a decision, see below)

Start from what exists: flat-earth point-mass missile, PN guidance, proximity fuze, single-look radar detection.
Gameplay needs, in order of value:

| # | Task |
|---|---|
| 3.1 | Scanning search radar and a track radar with lock/break-lock behaviour at gameplay level (states the operator sees) |
| 3.2 | **Outcome resolver** (abstracted): hit/miss decided from tunable gameplay factors - operator procedure quality, tracking quality, target type and manoeuvre - rather than from detailed physics |
| 3.3 | Target AI behaviour: attack profiles, terrain masking, evasion at gameplay level |
| 3.4 | Keep `prototype/` in parity for anything both implement |

**Decision needed from the user:** how far the physics goes (see "Open decision" below). Tasks 3.1-3.4 are scoped
to the gameplay-level option until the user says otherwise.

## Phase 4 - Unreal bring-up (after Phase 0)

| # | Task |
|---|---|
| 4.1 | Cesium for Unreal plugin, `CesiumGeoreference` matching the director's origin |
| 4.2 | `simcore::Terrain` backed by Cesium heights (line traces or height query), cached to keep the sim at 100 Hz |
| 4.3 | Drivable Osa proxy (Chaos Vehicles), placeholder mesh from Blender |
| 4.4 | Deploy/stow driven by the Phase 1 state machine: animations follow sim state, never the reverse |
| 4.5 | Performance check: 100 Hz sim step cost, terrain queries, frame time |

## Phase 5 - 3D cabin

| # | Task |
|---|---|
| 5.1 | First-person interaction framework: look, reach, click/drag controls |
| 5.2 | Controls bound to `ControlPanel` by `ControlDef.id`; the panel layout is data, not hard-coded |
| 5.3 | Radar scopes as render targets fed by sim data |
| 5.4 | Lights, indicators, audio cues driven by panel state |
| 5.5 | Placeholder cabin first; real art only after the creator's permission, or original art otherwise |

## Phase 6 - Crew and mission (the vertical slice)

| # | Task |
|---|---|
| 6.1 | Two NPC crew: bodies, animations, seat positions, voice-line hooks |
| 6.2 | Order menu (radial or list) that calls the Phase 1 crew API |
| 6.3 | One scripted mission end to end: drive, choose position, deploy, engage, stow, debrief |
| 6.4 | Playtest pass and balance; fix the rough edges |

**Exit = M2:** a person can play the whole mission without touching the editor.

## Parallel track (any time)

- **Creator outreach:** send the message with the three open questions (VB6 source or notes, written permission for art
  and procedures, manuals). Drafting it is a task Claude can do now. The answers decide how much reverse-engineering
  is ever needed.
- **SAM Simulator archive** (`reference/samsimv9302.rar`): extract into `reference/samsim/` when panel art or
  behaviour reference is needed. Needs about 3.6 GB free on `C:`; move it to `D:` if space is tight.
- **Reverse-engineering** (fallback only, and only with the creator's OK): VB Decompiler Lite for the form/handler
  map, Ghidra for the timer handlers first. Notes go in `docs/re/<system>.md`.
- **Tooling:** keep `verify.ps1`/`verify.sh` and CI current; add clang-tidy or sanitizers to CI when useful.

## Decision: how far the physics goes (resolved)

The user chose the **gameplay-level route** (phase 3 option 1, `e5271e7`): operators win or lose by how well they run
the procedures, track and engage, and the resolver turns that into outcomes with tunable numbers in `data/gameplay/`.
Detailed, calibrated weapon-performance physics is not scheduled; if it comes up again, treat it as its own
discussion and PR.

## Risks

| Risk | Mitigation |
|---|---|
| Unreal C++ build breaks unnoticed | Build from the command line (`unreal/README.md`) after every Unreal change |
| Creator never replies or says no | Original art and original procedures; Phase 1 works with placeholders either way |
| Solo part-time scope | Vertical slice only; cut M3 ideas until M2 ships |
| Disk space on `C:` (30 GB free) | Engine and caches on `D:`; repo stays on `C:` |
| OneDrive sync damaging builds | Repo stays at `C:\dev\SAM3D`, out of OneDrive |
