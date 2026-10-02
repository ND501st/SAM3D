# SAM3D — Handoff for the next AI model

*Written 30 Sep 2026 (Australia/Sydney); last updated 3 Oct 2026 at the end of the session that played the running game for 6.0. Read this first, then
`docs/BRIEFING_2026-10.md`, the repo's `CLAUDE.md`, `docs/DEVELOPMENT_ROADMAP.md` and `docs/ARCHITECTURE.md`.*

---

## 1. What the project is

**SAM3D** is a first-person, realistic 3D surface-to-air missile game in **Unreal Engine 5**, rebuilding the console
and procedure mechanics of **SAM Simulator v930.2** (a free hobbyist "system simulator" by "Hpasp", Hungary) **with
its creator's blessing**. The first system is the **9K33M2 Osa-AK as fielded in the 1980s, played as a battery**:
the player drives the vehicle, orients and deploys it, takes any of the five crew seats or the battery commander's
PU-12, and NPC crew work the *same* controls the player does.

- **Vertical slice:** one Osa-AK battery position, five-man crew, PU-12 designations, one mission from march to stow.
- **Later:** the rest of the battery and the TZM, historic scenarios (Angola 1987–88, Lebanon 1982, training ranges),
  Osa-AKM, then ZSU-23-4 Shilka.
- **Developer:** Nicholas (GitHub `ND501st`), solo, part-time, Melbourne (Australia/Sydney time zone).

## 2. Where things live

| Thing | Location |
|---|---|
| Repo | **`ND501st/SAM3D`** (private) |
| Branches and PRs | Everything is on **`main`**: stacked PRs #1–#8 were merged in order with merge commits on 2 Oct 2026 and their branches deleted. New work: one branch per task from `main` (next: phase 6.1). |
| PC checkout | `C:\dev\SAM3D` (out of OneDrive) |
| Unreal | UE 5.7.4 at `D:\UE_5.7`, Cesium for Unreal 2.29+; level `Content/Maps/OsaPractice` (Git LFS), opened on startup |
| Local reference | `reference/` (git-ignored): SAM Simulator install and data, the creator's documentation set (`manuals/`, incl. `SA-8B_English.pdf` and its text) |
| Research | `docs/research/OSA_RESEARCH_2026-10.md` (findings and sources), `docs/BRIEFING_2026-10.md` (decisions), `docs/research/OSA_CONSOLE_MAP_2026-10.md` (control and feature map by seat; proposals, not adopted yet; checked against `reference/` in §12, open items in §11) |

## 3. Machines and tooling

| Machine | Role |
|---|---|
| **Windows PC** (32 GB, RTX 3060 Ti) | Main workstation: UE 5.7, VS 2022, Git + LFS, Python, `gh` (signed in) |
| **Mac M3 Pro** | Remote access (Moonlight, Tailscale), simcore/prototype work over SSH |

What Claude can do on the PC:
- **Build simcore and run everything:** `.\tools\verify.ps1` from a VS developer shell.
- **Build the Unreal module** from the command line when the editor is closed:
  `D:\UE_5.7\Engine\Build\BatchFiles\Build.bat SAM3DEditor Win64 Development -Project=C:\dev\SAM3D\unreal\SAM3D\SAM3D.uproject -WaitMutex -NoHotReload`.
  With the editor open, compile through Live Coding from a script (`LiveCoding.Compile`, see below); header changes
  need the editor closed.
- **Drive the open editor by script** (Python remote execution is on): `python tools/ue_remote.py "<python>"`
  (actors, Play, levels, screenshots, console commands). `python tools/ue_cabin_check.py` plays the level with the
  hands-free cabin tour and records every view (state + screenshot) to `out/cabin_check/`. The tour needs the editor
  started with `SAM3D_CABIN_TOUR=6` or the director's **Cabin Tour** ticked.
- **Not possible:** sending keystrokes into a playing game through screen control (the game holds the keyboard).

Gotchas seen so far:
- The editor sometimes hangs on exit after "Exiting." in the log; it does finish. Don't kill it without asking.
- The Epic launcher once half-updated Cesium for Unreal while the editor was open (CesiumEditor.dll new, Runtime old →
  error 127). Fix: finish the update in the launcher with the editor closed, then rebuild.
- `unreal/SAM3D/Config/DefaultEngine.ini` has uncommitted editor additions (an Android File Server block with a
  token, shader formats). Commit only your own lines (stage a constructed blob), never that block.
- Git Bash heredocs mangle `\n` and `\\` inside Python strings; write scripts to files instead.

## 4. Repo map

| Path | Contents |
|---|---|
| `simcore/` | **Header-only C++17 sim core.** Physics side: `vec3`, `geo`, `atmosphere`, `terrain`, `radar`, `aircraft`, `missile`, `world`, `acmi`. Game side: `panel`, `equipment` (incl. `start_ready`, `Restart`), `procedures`, `json` + `console_data`, `debrief`, `scenario` + `scenario_runner` (incl. the player's tracker, launch restarts), `engagement` (gameplay-level picture, lock, outcomes, `TrackReturns`), `cabin` (layout, lamps, displays), `scopes` (search-scope sweep and afterglow) |
| `simcore/tests/` | 84 tests + `header_check.cpp` |
| `prototype/` | Python mirror of the physics side (13 tests) |
| `unreal/SAM3D/` | UE 5.7 project: `USimWorldSubsystem`, `ASimScenarioDirector` (incl. cabin tour switch), `ASimOsaVehicle` (placeholder, cabin mode, tour), `USimCabinComponent`, `USimCabinDisplayComponent`, `ASimCabinHUD`, `ASimEntityActor`. See `unreal/README.md` |
| `data/` | `procedures/osa_ak.json` (console), `cabins/osa_ak.json` (cabin), `systems/osa.json` (cited system data), `scenarios/osa_practice.json`, `gameplay/osa_ak.json` (game balance) |
| `docs/` | `BRIEFING_2026-10.md`, `research/`, `DEVELOPMENT_ROADMAP.md`, `PROJECT_PLAN.md`, `ARCHITECTURE.md`, `SOURCES.md`, `DEV_SETUP.md`, `REVERSE_ENGINEERING.md`, `re/osa.md` |
| `tools/` | setup and verify scripts; `ue_remote.py`, `ue_cabin_check.py`; `build_overview_pdf.py` |

## 5. Commands

```bash
cmake -S simcore -B simcore/build -DCMAKE_BUILD_TYPE=Release
cmake --build simcore/build --config Release
ctest --test-dir simcore/build -C Release --output-on-failure
python -m pytest prototype -q
.\tools\verify.ps1                              # everything (Windows)
python tools/ue_cabin_check.py --until 270      # scripted Unreal check (editor open, tour on)
```

## 6. Architecture in one paragraph

**simcore is the game; Unreal is the presentation.** Everything that decides an outcome (detection, tracking,
procedure correctness, crew actions, hits) lives in simcore, which is deterministic and runs without Unreal.
`USimWorldSubsystem::Tick` steps the scenario at a fixed rate; never step simcore from actor ticks. simcore uses ENU
metres (x=E, y=N, z=U); Unreal converts only through the subsystem (via the Cesium georeference when present). The
player's cabin clicks and NPC `CrewMember`s act on the **same** `simcore::ControlPanel`; the player's moves go through
a `ProcedureTracker` so the debrief records early and out-of-order actions.

## 7. Hard rules (from CLAUDE.md — do not break)

- simcore: header-only C++17, standard library only, no exceptions/RTTI, no macro-clashing identifiers, SI units with
  suffixes, warnings are errors, every behaviour gets a test, keep `prototype/` in sync where both implement a model.
- Data: **public and declassified sources only**, cited with id, tier and confidence (`docs/SOURCES.md`); code and
  tests use labelled placeholder numbers. **Never commit SAM Simulator's art, sounds, data or `sam.exe`.**
- Unreal: Epic coding standard; game logic in simcore.

## 8. Status

| Phase | State |
|---|---|
| 0–3 | Done: Windows build, consoles and procedures from data, crew orders, debrief, scenarios, gameplay-level engagement model |
| 4 Unreal bring-up | Done except Chaos Vehicles (needs a real mesh), the real model, the performance check |
| 5 3D cabin | Done: layout data, controls, lamps (glowing), instruments (search scope, range indicator, TV with a black-and-white scene capture, βс/εс dials, ДЦ voltmeter), hands-free tour, scripted checks. The practice shoot runs end to end in Unreal: lock at ~28 km, launch at 235 s, **hit** at 257 s (seed 4) |
| Research review | Done (PR #8): research file, briefing, re-plan; the user's video transcript analysed (Korovin's «Техника и вооружение» series added as a tier-1 source) |
| **6 Accuracy pass** | **Next, after the user answers the briefing's decisions.** See below |

## 9. The research review (2 Oct 2026) and what it changed

The user asked for a step back: read everything on SAM Simulator and research the real Osa, preferring documents
from operator countries. Main findings (detail and sources in `docs/research/OSA_RESEARCH_2026-10.md`):

- **Crew of five** (commander, deputy commander, senior operator, operator, driver); SAM3D has four seats.
- **Target designation came mostly from the battery's PU-12 by voice** (azimuth, range); SAM3D has no PU-12 yet.
- **Real timings are minutes:** cold switch-on 3.8 min + function check 3.0 min; march-to-combat ≤ 4 min; reaction
  26–40 s. SAM3D powers up in about a minute.
- **Missiles:** six in containers, automatic selection outer first, two channels, ≥ 4 s apart; guidance lamps per
  channel (СХОД → ЗАХВАТ → СР.Л → УЗКИЙ ЛУЧ → К3). SAM3D has no missile load and maps ЗАХВАТ/ВКЛ ВЫС wrongly.
- **TV tracking is manual**, search radar beams matter, the vehicle must be oriented (SAM3D assumes the hull faces
  north).

Consequences: the roadmap was re-sequenced (phase 6 accuracy pass, phase 7 battery and PU-12, phase 8 crew and
mission, then threats and scenarios); the validation set is now Osa events; `docs/SOURCES.md` has source tiers and
Western "facts and stats" pages are no longer used for decisions. On the same day the user supplied a YouTube documentary transcript; it retells V. Korovin's «Техника и вооружение»
series (2010, now a tier-1 source) and adds unverified battery tactics (research file §12, briefing §6). **On 2 Oct
the user adopted every recommendation in `docs/BRIEFING_2026-10.md` §5**: five-man crew now, real timings with time
compression, manual TV tracking for the player, hit chances calibrated to the published ranges, Osa-AK first, Osa
scenarios. Phase 6 can start.

## 10. SAM Simulator, in short

- Seven systems; `.dat` files are images and sounds; logic in `sam.exe` (VB6 native). Its Osa starts deployed, has
  three operator panels plus the PU-12 plotting board, a TV sight, ARMs, jamming, night. "Realistic to the switch";
  "no points are given, as in reality"; a situation ends when you switch the system off.
- The SimHQ development thread (2013–2017, archived) is a primary source: the creator worked with a Greek Osa officer
  ("ePap") who answered as an operator.
- **Ask the creator** for the VB6 source or notes, written art permission, the source conflicts (research §10) and
  his operator contacts. Reverse-engineering is a fallback, only with his OK (`docs/REVERSE_ENGINEERING.md`).

## 11. Physics depth (resolved)

Outcomes come from game-balance numbers (`data/gameplay/`), not weapon physics: phase 3 option 1 (`e5271e7`). The
research adds published single-shot kill-probability ranges to calibrate against (roadmap 6.7). Detailed weapon
physics stays unscheduled.

## 12. Model plan the user set

| Work | Model |
|---|---|
| Architecture, sim-core maths, validation, hardest bugs | Claude Fable 5.1 or Opus 5.5 |
| Reading decompiler output, research synthesis, big refactors | Claude Opus 5.5 |
| Everyday Unreal C++/Blueprints, UI, crew AI, tests | Claude Sonnet 5.5 |
| Bulk data entry, log triage, quick lookups | Claude Haiku 4.5 |

## 13. Next steps

1. **User:** review and send `docs/outreach/creator_message_draft.md`; name the video (title/channel) so
   `video_osa` can be cited properly; acquire the BSU textbook / GDR handbooks if possible.
2. **Phase 6.1:** five-man crew and seats (console stations, cabin, procedures, scenario crews). Breaking data change;
   one PR, branched from `main` (in review as PR #10). Then **phase 6.0:** the eyes-on pass. **First half done**
   (branch `docs/eyes-on-pass`, stacked on PR #11): every §11 item of `docs/research/OSA_CONSOLE_MAP_2026-10.md`
   checked against `reference/` (the SAM Simulator manual's panel photos and the Osa sprites), the map corrected,
   findings in its §12. Main results: the 7-position knob is the missile selector; "БОРТ" sprites are the КАНАЛЫ
   channel selector; the "+27 V selector" is test socket Ш7 beside a guarded СМЕНА РКТ switch; the "β / ε lamps" are
   the dials' backlight; ЦУ and АЗ are separate buttons; **red dots mark working positions, not start positions**
   (this affects 6.1's red-dot plan). The `rwd_mb3` photos are now in `reference/photos/rwd_mb3/` and checked (map §12):
   seat columns and the seat row confirmed, labels too small to read. **Running game done (3 Oct 2026, map §12)**,
   except anything needing a tracked target: start positions measured for every sprite-backed control (ДАЛЬНОСТЬ
   starts at 0–35 km, not the data's 0–15; РОД РАБОТЫ and РАБОТА / КОНТРОЛЬ start away from their red dots, КАНАЛЫ on
   its dot); ЦУ lights ЦУ and АЗ, АЗ alone does nothing; СМЕНА РКТ is one three-step control; wrong orders are ignored
   or accepted, never faulted. **Still to do for 6.0:** with a target, the elevation screen's axes and what ЦЕЛЬ /
   КАНАЛЫ / СМЕНА РКТ do (no echo appeared at Ashuluk; try again or ask the creator), the creator's answers (questions now in `docs/outreach/creator_message_draft.md` §4), then put the corrections into `data/procedures/osa_ak.json` and
   `data/cabins/osa_ak.json` on top of 6.1 and check each seat with `tools/panel_sheet.py`.
3. Then 6.2–6.8 in order, each its own PR with tests and a scripted Unreal check where it shows.
4. Creator message: drafted (`docs/outreach/creator_message_draft.md`); send is the user's.

## 14. Working conventions with this user

- PRs on branches for review, not pushes to `main`; stacked PRs are fine.
- Commit/PR attribution: follow the session's instructions; the repo's `CLAUDE.md` takes precedence.
- C++ and Python kept matching, with tests, for any model both implement.
- Prefers Claude to test in Unreal itself (scripts, screenshots) and to explain findings plainly.

## 15. State at the end of the last session (3 Oct 2026, running-game checks)

- **Open PRs, stacked:** #9 `docs/osa-console-map` → #11 `tools/panel-sheet` → #12 `docs/eyes-on-pass` (docs only:
  map, `docs/re/osa.md`, `SOURCES.md`, creator draft; this session added the running-game results to it). #10
  `feature/five-man-crew` (6.1) is separate, from `main`. Merge in that order; retarget the next PR to `main` before
  deleting a merged base branch.
- Work happens in a separate worktree, `C:\dev\SAM3D-eyeson`, so the main checkout (`C:\dev\SAM3D`, on
  `feature/five-man-crew` with the editor's uncommitted `DefaultEngine.ini` lines) is not touched. Remove the
  worktree with `git worktree remove ../SAM3D-eyeson` once #12 is merged.
- **Playing SAM Simulator from a session:** it is installed at `C:\Program Files (x86)\SAM Simulator\sam.exe` and
  can be driven with desktop control (grant "SAM Simulator"). Hints: the game needs keyboard focus (click an empty
  part of the board before pressing Z / X / C); hold push-buttons about 0.4 s; wheels need a drag that starts on the
  wheel. Escape at the end of a session left the game stuck on a map, so close the process instead. For exact
  states, capture each console form with `PrintWindow` (the forms are `ThunderRT6FormDC` windows, drawn at 1.25× on
  screen) and template-match the sprites in `reference/samsim/osa_view/` with OpenCV (installed into the session
  scratchpad, not the system). `PrintWindow` returns a stale picture of the search scope, so read scopes from live
  screenshots.
- No target was captured: at Ashuluk the La-17M drone's track reached about 15 km on the PU-12 board, but no echo
  showed on the search scope on any scale or beam. Worth one more try (another target type, or the Ustka range), or
  leave it to the creator.
- Unreal was not used. No code or data changed; no build or tests needed. The data corrections (e.g. `soc.range`
  initial 1 for 0–35 km) wait for the 6.0 data step on top of 6.1.
- Earlier state still holds: decisions in briefing §5 adopted; CI was green on PRs #1–#8 before merging.
