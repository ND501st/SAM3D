# SAM3D electromagnetic layer — technical handover

*Written 7 October 2026 at the end of the session that researched, designed and built the first
version (branch `feature/em-environment`). Read this first, then `docs/EM_ARCHITECTURE.md`. For the
whole project read `docs/HANDOFF.md` and the repo's `CLAUDE.md`.*

---

## 1. What this is

A real-time electromagnetic environment for SAM3D: radar detection and tracking, passive detection
and identification of emitters (RWR, ESM, ELINT, TDOA networks), triangulation and geolocation,
noise and deception jamming, chaff, radio nets and command posts, and per-unit knowledge kept apart
from truth — all emerging from one physical model rather than scripted effects. Its job in the game:
the Osa-AK crew (and the AI) should see the battlespace the way a real crew would — late, uncertain,
jammed, partial — and the enemy should find the Osa the way a real enemy would: by its emissions.

Design principle given by the project owner and followed throughout: *simulate the information and
decisions available to military systems, not Maxwell's equations*; realism first, then architecture,
performance, extensibility, ease of development, visuals.

## 2. Where everything is

| What | Where |
|---|---|
| Research report (radar, ESM, DF, EW, EMCON, decoys, environment, C2, games survey, tiers, bibliography) | `docs/research/EM_RESEARCH_2026-10.md` |
| Engine comparison and Unreal mapping | `docs/research/ENGINE_COMPARISON_2026-10.md` |
| Architecture | `docs/EM_ARCHITECTURE.md` |
| Osa-AK specification (labelled Confirmed/Inferred/Approximation/Unknown) | `docs/research/OSA_EM_SPEC_2026-10.md` |
| Roadmap (EM track E0–E10) | `docs/DEVELOPMENT_ROADMAP.md`, "Electromagnetic (EM) track" |
| Plain-language explainer for the project owner | `docs/EM_EXPLAINER.md` |
| Code | `simcore/include/simcore/em/*.hpp` (umbrella `simcore/em.hpp`, included by `simcore.hpp`) |
| Data | `data/em/generic.json` (placeholder), `data/em/osa_ak.json` (cited), `data/em/README.md` |
| Tests | `simcore/tests/test_em_models.cpp`, `test_em_data.cpp`, `test_em_scenarios.cpp`; `prototype/tests/test_em.py`; `tools/tests/test_em_view.py` |
| MVP demo, benchmark | `simcore/examples/em_demo.cpp` (CTest `em_demo`), `simcore/examples/em_bench.cpp` |
| Python mirror of the maths | `prototype/samproto/em.py` |
| Browser debug viewer | `tools/em_view.py` → `out/em_view.html` |
| Unreal | `unreal/SAM3D/Source/SAM3D/{Public,Private}/SimEmLayer.*`; API on `USimWorldSubsystem` |

## 3. Status (7 October 2026)

| Item | State |
|---|---|
| simcore EM layer | Built: propagation, calibrated radar (search and tracking), ESM/RWR/ELINT, TDOA, triangulation (Stansfield + ML), noise jamming with strobes and burn-through, RGPO, chaff vs MTI coherence modes, IFF replies, Kalman TWS tracking, knowledge with covariance intersection, radio nets with latency/loss/relays/jamming/co-site, C2 status, SAM doctrine, overlays, JSON loader |
| Tests | 154 simcore tests pass (47 of them for the EM layer), 4/4 CTest (incl. `em_demo` and the Unreal-macro header check), 39 Python tests (13 for the EM layer) |
| MVP scenario | All eleven checks pass (§6) |
| Osa-AK data | Search and tracking radars and the R-123M, every value cited or labelled; calibration checks recorded (spec §4) |
| Performance | 500 aircraft / 60 search radars / 24 ESM sites: 14 ms per 10 Hz tick on one Ryzen 5 3600 core |
| Unreal | `FSimEmLayer` builds (UE 5.8, command line, no warnings); not yet run in the editor by Claude (project rule: the owner runs the editor) |
| Linked to the Osa scenario/console | **Search and tracking radars: yes (E2, branch `feature/em-osa-link`, stacked on E0)**: `simcore/em_link.hpp`, scenario `own_unit.em`, `osa_practice_em.json`, 11 tests in `test_em_link.cpp` (165 simcore tests, 5/5 CTest). IFF, TV, missiles and the Unreal scope are not on it yet |
| Environment | Nothing new installed: VS 2022, CMake, Python 3.12 + pytest, UE 5.8, Git were all present. numpy is present but not required; matplotlib is not installed and not needed |

## 4. How to build, run and test

```bash
cmake -S simcore -B simcore/build -DCMAKE_BUILD_TYPE=Release
cmake --build simcore/build --config Release
ctest --test-dir simcore/build -C Release --output-on-failure
simcore/build/Release/simcore_tests.exe                     # prints the Osa calibration table and scenario numbers
simcore/build/Release/em_demo.exe data out/em_demo.json out/em_demo.acmi
python tools/em_view.py                                     # out/em_view.html: open in a browser (or serve out/ over http)
simcore/build/Release/em_bench.exe data 500 60 24           # aircraft, SAM radars, ESM sites
python -m pytest prototype tools -q
```

Unreal (editor closed): `D:\UE_5.8\Engine\Build\BatchFiles\Build.bat SAM3DEditor Win64 Development
-Project=C:\dev\SAM3D\unreal\SAM3D\SAM3D.uproject -WaitMutex -NoHotReload`. In the editor or game
console: `sam3d.em.Start`, then `sam3d.em.Debug -1` (everything) or a mask, `sam3d.em.Owner -1`
(everyone's knowledge), `sam3d.em.Stop`. The demo runs at the current sim origin on its terrain.

## 5. Architecture in one paragraph

`EmWorld` owns an `EmScene`: arrays of platforms (truth), emitters, radars, ESM receivers, TDOA
groups, jammers, chaff, nodes, nets, messages and links, plus the loaded library. Each 0.1 s step:
update platforms and the spatial grid → doctrine and EMCON → scan rotation → radars (only what the
beam swept; calibrated SNR with terrain diffraction, multipath, clutter/MTI, chaff and noise J/N;
Swerling draw; plots → Kalman TWS; strobes; tracking-radar lock and RGPO) → ESM at 5 Hz (one-way
power, main beam vs sidelobes over the listening window; measurements; association; library ID)
and TDOA → own sensors into each node's knowledge (2 Hz) → comms (links 1 Hz, reports, pictures,
deliveries, C2 status) → knowledge ageing (1 Hz). AI reads `KnowledgeBase::Picture`, never truth.
Every random draw is keyed, so results are order-independent and reproducible.

## 6. The MVP scenario and its checks

`simcore/em/demo_scenario.hpp`, fictional border area, generic equipment. Red: SAM-1 (search radar
always on, tracking radar), SAM-2 (dark until cued), battery CP, three DF sites, VHF voice net. Blue:
striker (fire-control radar on until 120 s, RWR), terrain-following low raider, stand-off jammer
(on at 200 s). CP destroyed at 330 s. Results of the current build:

| # | Check | Result |
|---|---|---|
| 1 | Aircraft emits a radar signal | striker's X-band radar radiating 0–120 s |
| 2 | SAM radar detects it | first detection at 99 s |
| 3 | Pd changes with distance | mean Pd 0.05 beyond 50 km → 0.23 inside 40 km |
| 4 | Terrain blocks detection | 30 masked looks at the raider behind the ridge, 0 detections; 27 after it crosses |
| 5 | Radar forms a track | confirmed tracks; tracking radar locks |
| 6 | Track uncertainty | covariance ellipses on every track and picture item |
| 7 | Aircraft turns radar off | EMCON at 120 s; ESM contacts time out |
| 8 | Passive detection | DF sites hear the striker at 0.1 s — 99 s before the radar sees it |
| 9 | Triangulation | CP fix on the striker's radar: error 3.3 km, CEP 2.0 km at ~80 km |
| 10 | Jammer degrades radar | sidelobe J/N up to 17 dB on the striker's looks; strobes; shrunken envelope |
| 11 | C2 distributes, loss matters | SAM-2 holds CP's tracks while connected; CP destroyed → degraded (355 s) → autonomous (392 s) → SAM-2 radiates |

## 7. Decisions, and the alternatives rejected

| Decision | Why | Alternatives considered |
|---|---|---|
| Calibrated radar equation (fit K to a published range) | Reproduces what is known without inventing classified internals; physics gives the scaling | Full absolute parameters (unavailable; would invent numbers); gameplay range tables (the previous engagement model: no emergent jamming, terrain or multipath) |
| Absolute EIRP/noise kept for ESM and J/N | One-way detection and jamming depend on absolute power, not on the fitted loop gain | Calibrating ESM ranges separately (no published anchors) |
| Swerling I, one look per tick | Conservative textbook model for aircraft; analytic | Swerling III/V; pulse-level integration (no observable benefit) |
| Per-tick emission packets | The level at which receivers and operators act; matches MIXR | Pulse-level or waveform simulation (tier 4) |
| Single knife-edge diffraction on a sampled profile | Cheap, physically right for both radar and VHF | Longley-Rice/ITM (heavier, VHF-oriented); Deygout multi-edge (later, E9); ray tracing (tier 4) |
| Two-ray multipath ≥ 1 GHz, plane-earth < 1 GHz, excess = max(plane-earth, knife-edge) at VHF | Matches published low-altitude radar range and VHF radio range with no fitting / a 0.7 dB fit | Two-ray everywhere (failed badly at VHF beyond 10 km); sum of losses (double counting) |
| Knowledge stored by origin, picture recomputed, covariance intersection | No rumour amplification; losing a source just ages out; simple to reason about | Destructive track merging; naive Kalman fusion of network reports (over-confident with unknown correlation) |
| GNN association | Sparse air pictures; simple, deterministic | JPDA, MHT (later if dense raids demand it) |
| Counter-based random numbers | Order- and thread-independent determinism | One `std::mt19937` stream (order-dependent; breaks with parallelism or batching) |
| ECS-style arrays inside simcore | Engine-free, testable, cache-friendly | Unreal Mass Entity for the simulation (ties the sim to the engine) |
| Uniform 2-D grid broad phase | Air pictures are wide and flat; O(n) rebuild | Octree/BVH (no benefit here) |
| ESM at 5 Hz with analytic main-beam windows | Halved the dominant cost without missing paints | Every tick (2× cost); per-receiver scan modelling (later) |
| Threat libraries as one side's beliefs | Misidentification and unknowns become gameplay; Western sources go where they belong | One shared truth library |
| Per-value provenance in JSON | Project rule; tools can print the spec | Free-text notes only |
| Overlay primitives produced in simcore | One definition for Unreal, the browser viewer and tests | Each front end computing its own overlays |
| EM layer stepped inside `USimWorldSubsystem::Tick` | One clock, the project rule against actor ticks | A second tickable subsystem (two clocks) |

## 8. Technical assumptions (labelled where they matter)

* Radar anchors are Pd 0.5 per look unless a source says otherwise; Pfa 10⁻⁶.
* Fighter-class RCS 3 m² for the Osa anchors (Korovin says "fighter"); aspect variation nose/beam/tail.
* Default environment: k = 4/3, ρ₀ = 0.9, terrain roughness 1 m (the demo uses 3 m, "rolling farmland").
* ESM sensitivities and bearing accuracies follow textbook classes (`EM_RESEARCH` §3–4).
* Radio required SNR 10–12 dB; voice report 8 s plus 4 s operator delay (estimates).
* Knowledge: air tracks without velocity grow at 250 m/s; emitters at the library's platform speed
  (air 350 m/s, ground 20 m/s, unknown 100 m/s); entries forgotten after 120 s or 15 km σ.

## 9. Implementation notes and gotchas

* **Macro-clashing identifiers:** `simcore/tests/header_check.cpp` compiles everything with Unreal's
  and Windows' macros defined (`near`, `far`, `small`, `check`, `PI`, …). It caught a variable named
  `near` in this work. Do not use those names.
* **MSVC C4686** inside Unreal for `(vᵀ·M·v)(0,0)` on the template matrix: use `QuadraticForm`.
* **UE 5.8 has no world-space `DrawDashedLine`**; `FSimEmLayer` draws dashes itself.
* **Git Bash heredocs** mangle `\n` inside Python strings (and therefore C++ string literals written
  by them): write patch scripts to files (as the project handoff already warns).
* **Lifetime:** `FSimEmLayer` holds a pointer to the active sim world's terrain; it is stopped when the
  scenario, the demo world or the sampled Cesium terrain is replaced.
* **ESM association** must use gates sized from the receiver's own measurement accuracy; a too-tight
  PRF gate split one emitter into several contacts and biased their bearings by ~10° (found and fixed
  in this session; tested).
* **Bearing-only fixes** need the ML refinement: the pseudolinear (Stansfield) solution collapses
  towards the receiver with poor bearings and short baselines.
* **Search-radar misses** count only for tracks inside the active elevation beam's 3 dB height;
  counting the whole evaluation band dropped young tracks.
* **Voice nets saturate** if a command post broadcasts faster than a report takes to read: pictures
  are coalesced with queued ones and empty pictures are short calls.
* The Python mirror must stay in sync with the C++ maths (project rule); `test_em.py` pins C++ numbers.
* **Checking GCC/Clang warnings on the PC** (CI builds Ubuntu and macOS with `-Werror`): there is no
  gcc/clang here, but Visual Studio's `clang-tidy.exe` (`VC\Tools\Llvm\x64\bin`) runs Clang's front end.
  From a vcvars64 shell: `clang-tidy --checks=-*,modernize-use-nullptr,clang-diagnostic-* --header-filter=simcore
  <file.cpp> -- -std=c++17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
  -Wno-missing-field-initializers -I simcore/include`. The `clang-diagnostic-*` check must be listed or
  compiler warnings are silently filtered. All EM tests and examples were clean this way on 7 Oct 2026.

## 10. Testing methodology

Hand-calculated values for the maths; Monte Carlo consistency of reported uncertainty (normalised
estimation error squared ≈ dof); behaviour scenarios with explicit expectations (§ EM_ARCHITECTURE 20);
an end-to-end MVP with a referee that may look at truth; calibration tests against published Osa
figures (and an explicit test pinning the known SSC discrepancy so a model change shows up);
cross-language checks; determinism (identical seeds → identical histories). Add a test with every new
behaviour; scenario tests should assert *relations* (more stations → smaller error; jamming → shorter
range) more than exact numbers, except where a source gives a number.

## 11. Known limitations

See `docs/EM_ARCHITECTURE.md` §22. The big ones were: not yet connected to the Osa console and scopes (done in E2,
E3); no missiles or optical sensor (done in E5, §33); minimal doctrine; single knife-edge (E9 started); GNN tracker.
Anti-radiation missiles are not modelled in this track.

## 12. Next steps

1. **Review and merge** `feature/em-environment` (the owner pushes and opens the PR).
2. **Owner checks in Unreal:** open the project, PIE, console `sam3d.em.Start`, `sam3d.em.Debug -1`;
   look at beams, strobes, ellipses and C2 links over the practice area. Report anything odd.
3. **E2 (done, `docs/EM_ARCHITECTURE.md` §23):** the Osa's search and tracking radars run on the EM
   layer in scenarios with an `own_unit.em` block, and the practice scenario plays end to end. Left of it:
   IFF replies from the EM radar's transponder model, and the scope's sweep in step with the radar's.
3b. **Roadmap 6.4 on the EM radar (done, branch `feature/osa-search-radar`, `docs/EM_ARCHITECTURE.md` §24):**
   the beam selector, the beam lamps, per-contact beam, range rings, and tests of the low-target ranges, the
   blind cone and MTI against normal video. Not done: MTI's 35 km limit in the EM path, the ring drawing
   in Unreal (the data and `ConsoleData::search.range_rings_m` are there), and the open 50 m capture item below.
   **Findings to read before building on it:** (a) a tracking radar locks on the first echo in its gate, and Swerling I Pd falls slowly (0.24 at 28 km
   for the Korovin target), so the raw lock came at 27 km, not his 23 km. The engagement model now takes the
   lock only once the radar has echoed on 22 of its last 40 looks (`EmSensorLink::Captured`), which gives 21 to
   25 km. The 22-of-40 rule is my model choice, not a published figure; a 1 s echo-fading correlation time
   was tried and moved the raw lock by 0.4 km, so it was dropped; (b) a target above about 8 degrees elevation is only in beam III (19 degrees tall, 13.6 dB
   less two-way gain), so a 3 m2 target at 5 km shows at 16 to 35 km depending on the seed instead of
   at 40 km; (c) both radars cancel radial speeds under 15 m/s as clutter, so a hovering target is
   invisible to them; (d) the tracking radar's auto-capture of a 50 m target comes at 22 km, not Korovin's 14 km, and no capture
   rule can fix that: the full-path Pd of that target is smooth (0.5 at 22 km, 0.8 at 18 km), and E0's "Pd 0.8 held
   14 km" comes from a deep multipath null at 17 km in the calibration-check path (`AnchorLook`) that the real path
   does not have. I chose the capture rule that reproduces the fitted 5 km figure (a Pd of about 0.55); a Pd of 0.7
   would put it at 19 km (-17 %) and move the 50 m case by nothing. Closing the 50 m gap needs a rougher
   surface or another trials-site effect.
4. **E1 data (first pass done, 11 Oct 2026):** `data/em/nato_1985.json` (NATO fighter radar, RWR, ESM station and
   ALQ-99-class jammer classes, and both sides' threat libraries; FM 100-2-3 read for 13 Soviet radars, low confidence;
   `docs/research/EM_E1_NATO_DATA_2026-10.md`). Still to do: named period NATO aircraft radars/RWRs/pods when a source
   gives distinct values, ZSU-23-4/SA-9/13 and Soviet radios from FM 100-2-3, ARM seeker data for E6.
5. **E2 review fixes (done):** the review's findings 1-5 are fixed (`docs/EM_ARCHITECTURE.md` §25): no truth reads in
   the link (ghost contacts for chaff and false echoes), antenna at the calibrated height, capture rule as data,
   scripted jammers and chaff in scenarios, the unit as a node with knowledge; IFF replies now come from the EM layer.
   Next EM code: E3 (Unreal scopes from EM output) or E4 (PU-12 node, voice designations as messages).

## 12b. E1, E3-E6 (18 October 2026)

* **E3**: the PPI from the EM radar: echoes painted by signal to noise and jamming, jamming strobes, the sweep and the
  vehicle's antenna following the EM antenna, `sam3d.em.Scenario 1` (`docs/EM_ARCHITECTURE.md` §32).
* **E4**: done; the saturated voice net measured in `test_c2_sources.cpp`.
* **E5**: the TV sight as a passive optical sensor, the command link and the missile's responders as emitters, a
  target's RWR (§33).
* **E6**: decoy drones the TV unmasks, a stand-off jammer escort, emission discipline (§34). No anti-radiation
  missiles.
* **E1**: the command link and the responder entered (estimates as marked); the other open data items still lack a
  registered source.

## 13. Open questions for the owner

1. Merge strategy: one PR for E0 (large, documentation-heavy) or split code and docs?
2. Should the EM layer replace the gameplay search picture in E2 outright, or run side by side behind
   a scenario flag for comparison during 6.4?
3. Interpretation of Korovin's tracking-radar capture ranges (Pd 0.5 vs reliable lock): the spec
   records both; is there a source that defines it (the NVA handbooks, `bsu_2020`)?
4. How far to take the blue side (aircraft avionics) before the vertical slice: generic only, or
   specific period aircraft?

## 14. References

The bibliography is `docs/research/EM_RESEARCH_2026-10.md` §14; source ids and tiers in `docs/SOURCES.md`.

## 15. Command sources (E4)

Done: `simcore/c2_sources.hpp`, `data/command/c2_sources.json`, scenario `c2` block and triggers `c2_source`/`c2_fail`/`c2_restore`, `osa_practice_c2.json`, tests in `simcore/tests/test_c2_sources.cpp`. The PU-12 is source `pu12`, always main. Link model done (`EmSensorLink::AddRadioSource`, `EmC2LinkModel`; sources with a `position` and an `own_unit.em` block). VHF jamming tested; the PU-12 takes a `position` in the scenario's `c2.sources`. Unreal: `RequestC2Source` etc. on `USimWorldSubsystem` (unbuilt). Voice designations are now EM `Voice` messages for sources with a radio. Next: (done: move voice designations onto EM messages, four-vehicle battery net, an R-123M preset control and Unreal switching UI (`USimWorldSubsystem` -> `ScenarioRunner::RequestC2Source`). The commander only monitors the sources on the presets of the two R-123M sets (`c2.presets`, `select_s`, `retune_s`; `TuneSet`, `ListenToSet`); see `docs/research/EM_C2_SOURCES_2026-10.md`. Console controls `radio.left.preset`, `radio.right.preset`, `radio.headset` are synced to the network by `ScenarioRunner::SyncRadios`; cabin panels `radio_left_panel`, `radio_right_panel`.

## 16. Deception (E7, done 16 Oct 2026)

`simcore/em/deception.hpp` (decoys, spot-jammer re-tune, dummy radio traffic) and `AddFalseTargetPlots` in `radar_system.hpp`; scenario `own_unit.em.decoys`. See `docs/EM_ARCHITECTURE.md` §29. Next ideas: decoys and dummy traffic as crew procedures (who switches the decoy on), scenario-level dummy nets for the battery, content value of intercepted voice.

## 17. Multiplayer knowledge (E10, simcore side)

`simcore/em/replication.hpp`: `BuildView`, `EncodeView`/`DecodeView`, `KnowledgeReplicator` (server), `KnowledgeMirror` (client); `EmSensorLink::UnitNodeId()`. See `docs/EM_ARCHITECTURE.md` §30. Unreal transport done (`USimKnowledgeComponent`, `USimWorldSubsystem::PumpKnowledge`, automation test `SAM3D.Knowledge`). Next: a two-client play-in-editor check, and the multiplayer crew (several players on one vehicle share its node's view; per-seat filtering if wanted).

