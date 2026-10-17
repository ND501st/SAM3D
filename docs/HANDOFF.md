# SAM3D — Handoff for the next AI model

*3D interior work (5 Oct 2026): see `docs/INTERIOR_HANDOFF.md` first.*
*Electromagnetic layer — radar, ESM, EW, C2 (7 Oct 2026, branch `feature/em-environment`): see `docs/EM_HANDOFF.md`.*

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
| Unreal | UE 5.8.3 at `D:\UE_5.8`, Cesium for Unreal; Unreal MCP (`.mcp.json`, see `unreal/README.md`); level `Content/Maps/OsaPractice` (Git LFS), opened on startup |
| Local reference | `reference/` (git-ignored): SAM Simulator install and data, the creator's documentation set (`manuals/`, incl. `SA-8B_English.pdf` and its text) |
| Research | `docs/research/OSA_RESEARCH_2026-10.md` (findings and sources), `docs/BRIEFING_2026-10.md` (decisions), `docs/research/OSA_CONSOLE_MAP_2026-10.md` (control and feature map by seat; proposals, not adopted yet; checked against `reference/` in §12, open items in §11) |

## 3. Machines and tooling

| Machine | Role |
|---|---|
| **Windows PC** (32 GB, RTX 3060 Ti) | Main workstation: UE 5.8, VS 2022, Git + LFS, Python, `gh` (signed in) |
| **Mac M3 Pro** | Remote access (Moonlight, Tailscale), simcore/prototype work over SSH |

What Claude can do on the PC:
- **Build simcore and run everything:** `.\tools\verify.ps1` from a VS developer shell.
- **Build the Unreal module** from the command line when the editor is closed:
  `D:\UE_5.8\Engine\Build\BatchFiles\Build.bat SAM3DEditor Win64 Development -Project=C:\dev\SAM3D\unreal\SAM3D\SAM3D.uproject -WaitMutex -NoHotReload`.
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
| `simcore/tests/` | 88 tests + `header_check.cpp` |
| `prototype/` | Python mirror of the physics side (13 tests) |
| `unreal/SAM3D/` | UE 5.8 project: `USimWorldSubsystem`, `ASimScenarioDirector` (incl. cabin tour switch), `ASimOsaVehicle` (placeholder, cabin mode, tour), `USimCabinComponent`, `USimCabinDisplayComponent`, `ASimCabinHUD`, `ASimEntityActor`. See `unreal/README.md` |
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
| **6 Accuracy pass** | **6.1 done** (branch `feature/five-man-crew`): five seats, one seat per procedure, aiming split per wheel, red start-position dots. Next: 6.2 |

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
2. **Phase 6.0 and 6.1: done** (6.1 in PR #10; 6.0 across PRs #12–#16, closed out in `docs/research/OSA_CONSOLE_MAP_2026-10.md` §13 on
   8 Oct 2026). Left open and handed on: a person with both mouse buttons at NAMFI (ЗРП toggle, lock graphics with a target
   in play), the creator's answers (`docs/outreach/creator_message_draft.md` §4), and labels on PU-303 / OO96-9M / OO05-8M,
   which need sharper photographs. **Next: 6.2.**
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
- No target was captured. Ashuluk's drone never showed on the scope; Ustka's P-15 and NAMFI's Iris drone did. At
  NAMFI the session got the azimuth line on the drone, ЦУ, the tracking transmitter on and the target spike into the
  range gate, but no capture. The desktop-control tools can't hold the right mouse button (fast slew only turns
  anticlockwise, no coarse range wheel) and the wheels barely move with synthetic drags. To see echoes, capture the
  scope region ~25 times over one turn and take the per-pixel maximum (`burst.ps1` / `maxc.py` style, in the scratchpad).
- A player's video of a full engagement: `video_samsim_osa` (`docs/SOURCES.md`). In a browser pane, YouTube frames
  can be drawn from the `<video>` element into a canvas for crops; the embed player is blocked.
- Data step part 1 is PR #13 (`feature/console-accuracy`, worktree `C:\dev\SAM3D-console`, stacked on #10): simcore
  tests pass (90); its Unreal changes (red-dot call renamed, power hotkeys to the driver) still need a build.
  Merge order: #9 → #11 → #12, and #10 → #13.
- Approach note: playing SAM Simulator by desktop control is slow and limited by input; the game is tier 3. Prefer
  the data step, the creator's answers and the NVA documents; use the game only for specific questions.
- Earlier state still holds: decisions in briefing §5 adopted; CI was green on PRs #1–#8 before merging.

Early-warning/command sources (E4, roadmap 7.8): `simcore/c2_sources.hpp`, `data/command/c2_sources.json`, `docs/research/EM_C2_SOURCES_2026-10.md`. PU-12 is the main source; the crew can switch (`RequestC2Source`) and an NPC commander falls back on silence.

## 0. The crew pillars (read first)

**Crew and teamwork (the project owner's design pillars, 16 Oct 2026; `docs/crew/README.md`).** Teamwork with NPC
crewmates is paramount. The game has two halves: *in the crew*, the player is one member of the team, learning the
drill and how to operate each seat; *in command*, the player gives the orders, first for the single Osa, then for the
battery. Every NPC works with every other; every action and command is mapped and interconnected (the generated action
map), with behaviour paths, callouts and commands for every environment and outcome. NPC skill levels come from player
progression and quests and affect reaction, speed and accuracy everywhere. An ideal progression of events, the drill
master's perfect performance, is the reference for the training overlay, like a chess engine that always knows the
best move. One crewmate's action depends on others' knowledge and actions (someone must know where the target is,
someone must have driven within reach), and that interdependence is mapped.

## 16. State on 16 October 2026 (phase 9 done in simcore)

Sections 8, 13 and 15 above describe early October. Since then (all as stacked PRs, newest last): the accuracy pass
(phase 6), the EM track to E10 (multiplayer knowledge, client scopes from the knowledge stream), the battery and PU-12
work of phase 7 (7.1-7.8 in simcore, 7.7 in Unreal; drills, TZM and fuel-truck runs; placeholder models of the TZM,
the ATZ and the PU-12M; the PU-12M plan in `docs/PU12_DEVELOPMENT.md`), and now phase 9, threats:

- **Jamming in the engagement (9.1):** the tracking radar follows a noise jammer in angle (`docs/EM_ARCHITECTURE.md`
  §31), the range comes by hand from the search scope or the PU-12, ПОМЕХА fires by method Н, a missile with no range
  and ПОМЕХА off is never fused, and the firing zone has its crossing parameter. Drill `fire_at_jammer`.
- **EMCON TV engagement as a mission (9.2):** `data/scenarios/osa_emcon_tv.json`. The anti-radiation missile threat
  itself is not modelled.
- **Mixed tracking (9.4):** TV angles or the radar on a jammer, with the range by hand.
- **Target behaviour (9.3):** `simcore/target_behaviour.hpp`: terrain following, low strike runs, pop-up helicopters,
  evasion on launch or lock; `terrain.ridges` for cover in scenario files.
- Scenarios: `osa_jamming_em.json`, `osa_low_level_em.json`, `osa_emcon_tv.json`. 323 simcore tests.

Next, as the user set it: NPC crewmate behaviour and teamwork (phase 8), then the PU-12M (`docs/PU12_DEVELOPMENT.md`).

**Phase 8 started the same day** (`docs/crew/`, plan and status in `docs/crew/09_IMPLEMENTATION_PLAN.md`): the
reference documents and the generated action map; crew data and skill tiers; skill in the crew's behaviour; the drill
master (the ideal crew: best move per seat from any state); callouts; the NPC vehicle commander
(`osa_crew_practice.json` is fought from cold with no triggers). 343 simcore tests.

**17 October 2026:** 8.5 knowledge per seat, the rest of 8.2 (judgement, stress, the calibration against the published
reaction time), fault recovery in the drill master, the commander on the march and between fights, the training
overlay and crew menu in Unreal (H, O, T), the crew's grades in the debrief, the battery's fight with fire distribution
(`osa_battery_fight.json`) and one mission end to end (`osa_mission_march.json`). 356 simcore tests. Next: 8.10 (bodies,
animations, voices), playing it on the map, then the PU-12M (`docs/PU12_DEVELOPMENT.md`).

**17 October 2026, later:** 8.10 bodies and voices (`docs/crew/10_BODIES_AND_VOICES.md`):
- simcore's body model and a reach check of the cabin (standing for far panels costs drill time);
- Unreal crew bodies working their controls, a ghost hand;
- callouts spoken through Windows' speech engine (English on this machine: no Russian voice installed);
- headless Unreal tests `SAM3D.Crew.Speech` and `SAM3D.Crew.Bodies`.

Also:
- the assistance levels and each move's cost (8.7);
- the honest order menu with refusals (8.8);
- the engagement reference and scored decisions in the debrief (8.3);
- sectors of fire (8.9);
- the vehicle oriented in the readiness, and a reload inside the mission (8.11);
- `crew_balance` and the first balance pass (8.12, `docs/crew/11_PLAYTEST_AND_BALANCE.md`).

381 simcore tests. Next: a person plays the missions on the map (the crew half and the command half), then the PU-12M
as a seat.
