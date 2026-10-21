# Development roadmap

How the project gets built, in order. Complements `PROJECT_PLAN.md` (goals and milestones) with the concrete sequence,
what blocks what, and how each task is finished. Written 30 Sep 2026; **re-sequenced 2 Oct 2026** after the research
review (`docs/BRIEFING_2026-10.md`).

**Status (2 Oct 2026):** phases 0–5 done (phase 4 except Chaos Vehicles, the real model and the performance check),
merged to `main` on 2 Oct 2026 (PRs ND501st/SAM3D#1–#8). **The briefing's decisions were all adopted (five-man
crew, real timings with time compression, manual TV tracking, calibrated hit chances, Osa-AK, Osa scenarios). Next:
phase 6, the accuracy pass: 6.0 and 6.1 are done (8 Oct 2026); 6.2 (real start-up and readiness states) is next.** The old "crew and mission"
phase is now phase 8, after the battery (phase 7), because the real Osa got most of its targets from the battery's
PU-12 and the crew changes ripple through everything after them.

**Electromagnetic layer (7 Oct 2026):** phase E0 of the new EM track is done on branch `feature/em-environment`
(research, architecture, `simcore::em`, MVP scenario, tests, Unreal overlays): see the EM track section below and
`docs/EM_HANDOFF.md`. E1–E2 are proposed to run alongside 6.4, which they largely implement.

## Principles

1. **simcore is the game, Unreal is the presentation.** Build and test behaviour in simcore first; Unreal only displays it.
2. **Procedures and consoles are the heart of the game.** Operator procedures, interlocks, crew, battery and scenarios
   come before any deeper physics.
3. **Accuracy before features.** When research shows the model is wrong, fix the data model before building UI on it.
4. **Vertical slice first.** One system (Osa-AK), one battery position, one complete mission, before a second system.
5. **Small PRs on branches**, one topic each. Never commit to `main` directly.
6. **Definition of done:** builds with warnings-as-errors, tests added, `tools/verify.ps1` passes, docs updated if
   behaviour or data format changed. Unreal changes: built from the command line and checked with
   `tools/ue_cabin_check.py` where it applies; the user plays it.
7. **Public and declassified sources only, best tier first** (`docs/SOURCES.md`). Every data value has `source` and
   `confidence`; disagreements between sources are recorded, not hidden. SAM Simulator's art, sounds and `sam.exe`
   never enter git.

## Phases 0–5 (done)

| Phase | What | Where |
|---|---|---|
| 0 | Environment: Windows build, UE 5.7, Cesium, `gh`, CI | PR #1 |
| 1 | Consoles and procedures from data; equipment and interlocks; crew order queue; debrief | PR #2 |
| 2 | Scenarios, deterministic runner, `osa.json` (vehicle, crew, power) | PR #3 |
| 3 | Gameplay-level engagement model (search picture, lock/break-lock, outcome resolver) | PR #4 |
| 4 | Unreal bring-up: scenario mode, Cesium terrain, placeholder Osa (open: 4.3 Chaos Vehicles, real model, 4.5 performance check) | PR #5 |
| 5 | 3D cabin: layout data, player actions judged by a tracker, instruments (search scope, range indicator, TV, dials, voltmeter), hands-free tour and scripted checks | PRs #6, #7 |

## Phase 6 - Accuracy pass (simcore, data, cabin)

Corrections from `docs/research/OSA_RESEARCH_2026-10.md`. Each task names the research section it comes from.
The control-level breakdown (which switch, at which seat, in which real block) is in
`docs/research/OSA_CONSOLE_MAP_2026-10.md`. Its task 6.0 was adopted on 2 Oct 2026. 6.1 was built the same day
without it (PR #10), so 6.0 runs after 6.1 and before 6.2, and corrects 6.1's seats and layout where the photos
differ (the map has the turbine switch at the driver's power panel, and the three operators side by side at one
cabinet).

| # | Task | Research |
|---|---|---|
| 6.0 | **Done (PRs #12–#16, closed out in console map §13).** **Eyes-on pass** (needs a session linked to the PC, for `reference/`). Go through the cabin photos and layout drawings (`rwd_mb3`), SAM Simulator manual p14–18, the control sprites and the running game. Record every control in the console and cabin files: label, kind, positions, start position, place on its panel; controls whose function isn't known go in as visible but inert. Check each seat with `tools/panel_sheet.py` against the photos. Closes or hands to the creator the open items in the console map §11 | console map §9, §11 |
| 6.1 | **Done (PR #10).** **Five-man crew and seats.** Split the "range officer" into the senior operator (search radar, IFF, azimuth) and the operator (range); the "firing officer" becomes the deputy commander (elevation, TV, launch); the commander receives designations and gives orders; the driver. Console `station` fields, cabin layout (five seats, operators' cabin behind the driver's cab), procedures, scenario crews, aim roles. Panel detail: red dots marking each control's starting position | §2, §6 |
| 6.2 | **Done (simcore and data; the Unreal time scale is written but not yet built).** Cold switch-on 4.0 min and function check to 7.1 min (published 3.8 + 3.0), reaction time 27 s (published 26-40), march to combat well inside 4 min (the physical deploy work isn't modelled, so it is faster than the unverified 35-50 s). Left: crew-skill timing for NPCs (next target 40-120 s, unverified). **Real start-up and readiness states.** Switch-on cycle and a function-check procedure (about 3.8 + 3.0 min cold), computer warm-up (100 s), march-to-combat ≤ 4 min, reaction 26–40 s as test targets; scenarios start cold, powered on the march, or deployed (a powered battery stops and deploys fast); crew-skill timing for NPCs (next target 40–120 s, unverified); time compression so it stays playable | §3, §10, §12 |
| 6.3 | **Done (`simcore/launcher.hpp`, the console's `launcher` block).** Also done: the command transmitter's letter change (ЗАХВ. АПЧ out for 1 s, ПУСК ГОТОВ waits), the emergency launch (ИЗДЕЛИЯ + АВАРИЙНЫЙ ПУСК, unguided 25.5 s), and the range indicator's missile sweeps Р I / Р II (simcore and the Unreal drawing, checked in the editor with `tools/ue_sweep_check.py`). Also done: a prepared pair is held at most 10 min, then the gyros have overheated (the ammunition lamp flashes), the next pair is prepared, and the hot missiles rest 10 min (1 h after the third hold). Left: a scenario key for a part load (`Launcher::Unload` exists). **Missile load, selection and channels.** Six containers (1–3 I, 1–3 II) with ammunition lamps; automatic selection, outer first; selector switch; two channels, ≥ 4 s between launches; per-channel lamp sequence СХОД → ЗАХВАТ → СР.Л → УЗКИЙ ЛУЧ → К3; command transmitter on at ВНИМАНИЕ; preparation at "target in zone"; fix the ЗАХВАТ / ВКЛ ВЫС mapping | §6 |
| 6.4 | **Done on the EM radar (PR #29; `docs/EM_ARCHITECTURE.md` §24).** Left: drawing the rings in Unreal (the data is loaded), MTI's 35 km limit in the EM path, IFF replies from the EM radar (IFF stays the gameplay model). **Search radar.** 33 rpm; elevation beams I/II/III that decide what the radar sees (4°, 4°, 19° high), plus the beam indicator; shorter detection and capture ranges against low targets (27 km and 14 km at 50 m, original Osa); range rings as operators describe them; IFF interrogation and friendly targets; overhead blind cone; normal video vs MTI | §4 |
| 6.5 | **Done (the gyro half-compass frame and orientation at a halt were already in `engagement.hpp`; this adds the Unreal heading conversion, which treated Unreal yaw as true heading and was 90 degrees off under the Cesium georeference, and a test of board designations on an unoriented vehicle).** Left: the OR-31M Qc scale, a measurement error on Qc and gyro drift (no sources found yet). **Vehicle heading and orientation.** Drop "the hull faces north": azimuths relative to the hull, the navigation/orientation set aligned at a halt, designation errors when it isn't | §3, §7 |
| 6.6 | **Done (PR #33; `data/gameplay/README.md`).** Left: drawing the 3-mil reticle and reading range from the target's size in it, in the TV view (Unreal). **Manual TV tracking.** The TV channel follows the wheels (player) or a skill model (NPC); 3-mil reticle; operator lens fields; memory mode; range judged from the reticle | §5 |
| 6.7 | **Done (PR #32; `data/gameplay/README.md`).** Also done: ПОМЕХА selects method Н, and К3 then comes with the medium beam (1991 textbook p22-23, p136); the tracking radar needs its СДЦ on for targets under 100 m (an estimate). Left: jamming targets that hide the range, so Н has an effect (with E5), and the NVA's crossing-offset "parameter" limit. **Zones and guidance methods.** Head-on, pursuit and optical "Ф" zones; methods T/T, Горка, Ф, Н chosen automatically; hit chances calibrated inside the NVA single-shot ranges and Korovin's low-altitude figures (0.35–0.4 at 50 m) (game-balance file) | §5 |
| 6.8 | **Done.** No value in `data/` cites a tier-5 source; the photo shorthand in the cabin file now names its registered source; `tools/check_citations.py` (with a test) keeps it so. The radar frequencies still rest on `western_bands` (tier 4/5, low confidence): operator sources only say "centimetre band". **Citations.** Replace Western "facts" citations in `data/` with operator-country sources; record conflicts | §1, §10 |

**Exit:** the practice scenario runs with five crew, real timings (with time compression), six missiles and a
two-missile salvo, and every lamp in the order the NVA account gives. `tools/ue_cabin_check.py` shows it.

## Phase 7 - Battery and PU-12

| # | Task |
|---|---|
| 7.1 | **Done in simcore (`simcore/pu12.hpp`, `data/command/pu12.json`, scenario `command_post`).** The board plots once a minute with the manual's notation (number, height in hectometres over type and formation, minute stamp), carries the PU-12M's navigation error (5 degrees, 3 km), and the crews aim by it when the radars are silent. The regiment's radars now feed it (`data/command/regiment_radars.json`, scenario `c2.regiment`): the board holds a target only when a P-19 or P-40 of the list covers it, range and horizon. Left: the plotting board in Unreal (7.7). PU-12 command post in simcore: air picture from the regiment, updated once a minute; SAM Simulator's plotting-board notation (target number, height, type, formation) |
| 7.2 | **Done in simcore (`simcore/pu12.hpp`).** Each target inside 50 km gets a voice designation (azimuth in mils, range, height) on every plot; the radio carries one message at a time (4 s operator delay + 8 s message, both estimates, then 2 s for the vehicle commander to pass it on), and the crew aims by the last designation it was told. Left: the player as battery commander (giving the designations), the designation on the real R-123M net with loss and jamming (E4), and a vehicle commander NPC that can get it wrong. Target designation by radio: the battery commander (NPC or player) gives "azimuth, range" to each vehicle; the vehicle commander passes it on |
| 7.3 | **TZM reload of one vehicle done (`simcore/launcher.hpp`).** With the `tzm_alongside` input set and the vehicle stopped, the crane loads one missile a minute into empty containers, only once the pair is spent and nothing is guiding: six in 6 minutes (`flak11`). Left: the TZM as a vehicle that drives up and carries 12 (the scenario sets the input), four vehicles per battery with NPC crews, and the crane's three-container traverse. Four vehicles per battery (others NPC-crewed) and the TZM: reload 6 missiles in about 6 min |
| 7.4 | **Done in simcore (`simcore/fuel.hpp`, the console's `fuel` block).** One 300 l tank; the turbine burns 75 l/h at a halt and 100 l/h moving, the take-off generator 25 l/h; at half the tank the crew asks for fuel and the power sources are cut to keep the rest for driving; the TZM refuels at a halt (60 l/min is an estimate). Left: the drive burning fuel (a driven vehicle in Unreal), the grid and diesel supplies of 7.6, and a fuel gauge in the cabin. Fuel: turbine and take-off generator consumption, the half-tank rule, refuelling from the TZM |
| 7.5 | NPC battery behaviour from the battery tactics in the research (§7): march spacing, short-halt drill, combat formation in pairs, reload vehicles behind, halts every 3–4 h; batteries feeding each other's air picture |
| 7.6 | Prepared positions: emplacements, nets, dummy positions, grid power in peacetime and diesel when cut off. The `external_power` input and `external_supply` subsystem (the third power source) exist, and `own_unit.position_power` gives a scenario a grid, a cut and a diesel set (24 h, `video_osa`, unverified). Left: emplacements, nets, dummy positions |
| 7.8 | **Done in simcore (`simcore/c2_sources.hpp`, `data/command/c2_sources.json`, scenario `c2` block, triggers `c2_source`/`c2_fail`/`c2_restore`).** Other early-warning and command sources besides the PU-12 (EWR voice net, division early-warning net, neighbouring battery, observation post, data link), shared voice nets, outages, the crew changing source (player: `RequestC2Source`; NPC commander: fallback on silence and return to the main source). The PU-12 stays the main source. Evidence and decisions: `docs/research/EM_C2_SOURCES_2026-10.md`. EM link budget as the link model done (sources with a position), and their voice designations are EM `Voice` messages (radiated, delivered or lost by the link budget). The cabin has both R-123M sets with their controls (`comms` block, `simcore/crew_comms.hpp`, `docs/research/OSA_COMMS_2026-10.md`): a set that is off, warming up (10 min) or turned down hears nothing, the squelch and the antenna shorten the EM link by the published ranges, and the R-124 box A-1 decides whether the commander hears the net or shouts his orders and the designations he passes on; the scenario's `frequency_mhz` puts each net on a frequency of the 25 kHz grid; NPC crew switch the sets on (`radio_left_on`, `radio_right_on`). Left: transmitting from the vehicle, the presets' set-up drill, audio, a VHF jamming scenario, fusing two sources |
| 7.7 | Unreal: plotting board in the PU-12, radio messages, battery vehicles on the map |

## Phase 8 - Crew and mission (the vertical slice)

| # | Task |
|---|---|
| 8.1 | NPC crew bodies, animations, seats, voice-line hooks (five seats) |
| 8.2 | Order menu (radial or list) on top of the crew API; the player can take any seat |
| 8.3 | One mission end to end: march, choose a position, orient, deploy, function check, engage, reload, stow, debrief |
| 8.4 | Playtest and balance |

**Exit = M2:** a person can play the whole mission without touching the editor.

## Phase 9 - Threats

| # | Task |
|---|---|
| 9.1 | Jamming: noise jammers and range-gate pull-off now come from the EM layer (E0 done; scopes in E3); remaining: method Н and the ПОМЕХА switch in the engagement layer |
| 9.2 | Anti-radiation missiles and the drills (stop emitting, relocate; TV-first tracking, the search radar on for two turns, brief tracking-radar bursts); EMCON TV engagement as a full mission → EM phase E6 |
| 9.4 | ECCM: the external-coherence MTI mode against chaff exists in the EM layer (E0); mixed tracking and counter-jamming only while firing → E5/E6 |
| 9.3 | Target behaviour: pop-up helicopters, low strike runs, evasion, terrain masking (masking and multipath are in the EM layer) |

## Phase 10 - Scenarios and training

| # | Task |
|---|---|
| 10.1 | Training-range scenarios in the style of Ashuluk / Kapustin Yar / Ustka / NAMFI (drones, P-15 targets) |
| 10.2 | Angola 1983–88 (dug-in Osa-AK unit, night moves, night bombers; Cuito Cuanavale) and Lebanon, 24 July 1982 |
| 10.3 | Instructor mode modelled on the 9Ф632 trainer: a second player changes target behaviour live |

## Electromagnetic (EM) track

Radar, passive sensing, EW, communications and command as one simulated environment
(`simcore/include/simcore/em/`, `docs/EM_ARCHITECTURE.md`). Phases chosen from the research
(`docs/research/EM_RESEARCH_2026-10.md`): the order is "get the Osa's own radar picture from the
physics first, then the battery's C2, then missiles and optics, then threats and deception", because
each later phase needs the earlier one's knowledge model to be right. Difficulty S/M/L; cost per
tick at the 500-aircraft benchmark (14 ms mean today on one core).

| Phase | Objective | Systems | Depends on | Difficulty | Perf cost | Testing | Done when |
|---|---|---|---|---|---|---|---|
| **E0** (done, 7 Oct 2026) | Research, architecture and a minimum viable EM layer | Propagation, calibrated radar, ESM, triangulation, TDOA, noise jamming, strobes, RGPO, chaff/MTI, tracking, knowledge/fusion, comms/C2, doctrine, overlays, Unreal demo mode | — | L | baseline | 47 new C++ tests (154 in all), 11 MVP checks, Python mirror | merged; Unreal demo visible with `sam3d.em.Start` |
| **E1** (first pass done 11 Oct 2026: `data/em/nato_1985.json`, `docs/research/EM_E1_NATO_DATA_2026-10.md`; FM 100-2-3 read 11 Oct 2026: NATO view of 13 Soviet radars, low confidence) | Data hardening | Blue threat library from period sources (FM 100-2-3 etc.); generic-but-period NATO aircraft radars, RWRs, pods; Osa open items where sources appear; ESM band plans | E0 | S | none | data tests with provenance | every library value cited or labelled |
| **E2** (done 11 Oct 2026, PRs #28, #29; review fixes, jammers, chaff, IFF and the unit as a node added after: `docs/EM_ARCHITECTURE.md` §25) | The Osa's radar picture from physics | `ScenarioRunner` owns an `EmWorld`; console controls drive EM state (`soc.hv`, `soc.beam`, `soc.mti`, `iff.*`, `ssc.tx`, `fo.cu_az`); the engagement model's search picture and lock read EM plots and tracks instead of fixed thresholds; outcome resolver unchanged | E0, 6.1 | M | +1–2 ms | practice scenario: lock ranges in the published bands; procedure demos unchanged | practice scenario plays with EM-driven scopes and lock (this is most of 6.4) |
| **E3** | Unreal presentation | PPI echoes from plots (brightness from SINR), strobes, IFF replies, range indicator from the SSC; overlays on real Cesium terrain; Gameplay Debugger category; antenna rotation from the emitter state | E2 | M | GPU only | scripted `ue_cabin_check.py` views | the cabin scopes show EM output |
| **E4** | Battery C2 and PU-12 | PU-12 node and plotting board fed by a regiment link and a P-19/P-40; voice designations "azimuth, range" as messages to commanders; battery net; four vehicles | E2, phase 7 | M | +0.5 ms | C2 scenarios: latency, saturation, loss of the PU-12 | phase 7 runs on EM messages | **Started 11 Oct 2026:** multiple selectable early-warning/command sources (PU-12 main) in simcore (roadmap 7.8); still to do: voice designations as EM radio messages (link model), battery net with four vehicles, loss/saturation scenarios on the EM net.
| **E5** | Missiles and optics | Missile platforms with responders (ССР), the command transmitter (СПК) as an emitter, the TV sight as a passive optical sensor (angles, visibility), method Н | E2, 6.3, 6.6 | M | small | guidance-channel lamp sequence from EM states | launches visible to ESM; TV tracking without emission |
| **E6** | Threats | ARMs homing on emitters (with memory after shutdown), decoy drones, self-protection pods, SOJ escorts; doctrine: ARM detection, blink, relocate | E5 | M | small | scenarios: Lebanon-style decoys, HARM drill | phase 9.2 |
| **E7** | Deception and EMCON depth | Decoy emitters on the same parameters (Czechoslovak doctrine), dummy positions, frequency changes vs spot jammers, repeater false targets, VGPO, comms deception and the intercept value of voice | E6 | M | small | deception scenarios produce ghost fixes / misidentifications | phase 9 complete |
| **E8** (started 16 Oct 2026: ESM sites and search radars on worker threads, identical results at any count; bench 500 aircraft / 60 SAM radars / 24 ESM sites: 14.7 ms per tick on one core, 5.7 ms on 4 threads, 4.2 ms on 8; left: time-slicing the cold paths, hierarchical grid, LOD) | Scale | Threads for ESM/radar systems, time-sliced cold paths, hierarchical max-height grid, level of detail for distant units | any | M | goal: 500 aircraft ≤ 5 ms mean | `em_bench` in CI as a trend | goal met with identical results single- and multi-threaded |
| **E9** (started 16 Oct 2026: Deygout three-edge diffraction with distinct-crest sub-edges, ITU-R P.838 rain; left: forest/urban obstacle heights, sea areas) | Environment depth | Deygout multi-edge diffraction, forest/urban obstacle heights from land cover, rain at Ku band, sea areas | E2 | M | +10–30 % path cost | propagation tests vs ITU-R examples | — |
| **E10** | Multiplayer knowledge | Server-side simcore; replicate each player's knowledge, never truth | multiplayer crew | L | network | soak tests | later |

## Later

Phase 4 leftovers (Chaos Vehicles with a real model, performance check); Osa-AKM variant (9M33M3, hovering targets);
ZSU-23-4 Shilka (shares the PU-12); mission editor; multiplayer crew.

## Parallel track (any time)

- **Creator outreach:** drafted in `docs/outreach/creator_message_draft.md` (VB6 source or notes, art permission,
  the source conflicts, operator contacts); the user reviews and sends it.
- **Acquire documents:** the BSU textbook *Боевое применение ЗРК 9К33М3* (2020), the GDR troop handbooks (1984,
  1990), the Angola memoir collection. Summaries go into `docs/re/osa.md` with page references.
- **SAM Simulator archive** (`reference/samsimv9302.rar`) and its documentation set: keep using them as reference
  for console behaviour; nothing from them enters git.
- **Reverse-engineering** (fallback only, and only with the creator's OK): see `REVERSE_ENGINEERING.md`.
- **Tooling:** keep `verify.ps1`/`verify.sh`, CI and the Unreal scripts (`tools/ue_remote.py`,
  `tools/ue_cabin_check.py`) current.

## Decision: how far the physics goes (resolved)

The user chose the **gameplay-level route** (phase 3 option 1, `e5271e7`): operators win or lose by how well they run
the procedures, track and engage, and the resolver turns that into outcomes with tunable numbers in `data/gameplay/`.
Hit chances are calibrated to published single-shot ranges (phase 6.7); detailed weapon-performance physics is not
scheduled.

## Risks

| Risk | Mitigation |
|---|---|
| Phase 6 changes break saved data and tests | One task per PR; tests for each correction; data format changes documented in the READMEs |
| Real timings make the game slow | Time compression (as SAM Simulator's F1–F4) and scenarios that start at different readiness states |
| Manual TV tracking is too hard | NPC operators can track; an assist option if playtests demand it |
| Creator never replies or says no | Original art and procedures from operator sources; the research file covers most behaviour |
| Unreal C++ build breaks unnoticed | Build from the command line after every Unreal change; scripted checks |
| Solo part-time scope | Vertical slice only; phases 9–10 wait until M2 ships |
