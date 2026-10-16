# Electromagnetic environment layer — architecture

The radar, passive-sensing, electronic-warfare, communications and command layer of SAM3D
(`simcore/include/simcore/em/`, namespace `simcore::em`). Research behind every choice:
`docs/research/EM_RESEARCH_2026-10.md`. How the Osa-AK fits: `docs/research/OSA_EM_SPEC_2026-10.md`.
Handover and status: `docs/EM_HANDOFF.md`. Plain-language explanation: `docs/EM_EXPLAINER.md`.

## 1. Principles

1. **Simulate the information and decisions available to military systems**, not the waves:
   power, geometry, antenna patterns and terrain decide signal-to-noise; that decides what is
   detected, measured, reported and believed.
2. **Truth and knowledge are separate.** Truth lives only in `Platform`. Sensors read it inside
   their system functions and emit *observations*; everything downstream (trackers, fusion, AI,
   displays) works from observations. Fields named `debug_truth_*` exist for overlays and tests only.
3. **Every emission is the same kind of thing.** A radar, a jammer and a radio transmission are all
   `EmitterState`s; any receiver can hear any of them if its band and the physics allow.
4. **Calibrate to published performance; take the shape from physics.** Radars and radios are
   fitted to their published ranges; the model's scaling with RCS, range, terrain, multipath and
   jamming comes from textbook formulas. Estimates are labelled.
5. **Data, not code, describes systems.** Adding a radar, receiver, jammer or radio is a JSON entry
   in `data/em/` with sources.
6. **Deterministic.** Every random draw is a pure function of (seed, stream, keys).
7. **Engine-agnostic, header-only C++17** under the simcore rules (no exceptions/RTTI, no
   macro-clashing names, SI units, warnings as errors), mirrored in Python for the maths.

## 2. The pipeline

```
 TRUTH                 PROPAGATION              SENSORS                    TRACKS / CONTACTS          KNOWLEDGE (per node)          DECISIONS
┌───────────┐  emits  ┌──────────────────┐  ┌──────────────────────┐   ┌──────────────────────┐   ┌──────────────────────────┐   ┌──────────────┐
│ Platform  │────────▶│ path profile     │─▶│ radar look (2-way)   │──▶│ Tracker: associate,  │──▶│ KnowledgeBase: entries   │──▶│ SamDoctrine  │
│ position  │         │ (terrain, Earth) │  │ ESM intercept (1-way)│   │ filter, confirm, drop│   │ by origin, ageing        │   │ EMCON steps  │
│ velocity  │ reflects│ diffraction J(v) │  │ jamming J/N, strobes │   │ EsmContact: group,   │   │ Picture(): correlate,    │   │ (later: crew │
│ RCS       │◀────────│ gases, multipath │  │ IFF replies          │   │ identify (library)   │   │ CI fusion, triangulate   │   │  orders, AI) │
│ Emitters  │         │ PathCache        │  │ TDOA groups          │   │ StrobeTrack          │   │ bearing-only, ESM tags   │   └──────┬───────┘
└───────────┘         └──────────────────┘  └──────────────────────┘   └──────────────────────┘   └────────────▲─────────────┘          │
      ▲                                                                                                       │ messages            │ radar on/off,
      │                                                                 COMMS: nets, links (same propagation),│ (late, lossy)       │ designations
      └────────────────────────────── platforms move (simcore::World / scenario) ───────────────────────────┴─────────────────────┘
```

## 3. Information layers

| Layer | Type | Who produces it | What it holds |
|---|---|---|---|
| Truth | `Platform` | scenario / `simcore::World` | position, velocity, RCS model, side, alive |
| Emission | `EmitterState` (+ `EmitterDef`) | radars, jammers, radios, decoys | on/dummy load, pointing, sweep this tick, beam, channel |
| Observation | `RadarPlot`, `EsmIntercept`, `JamStrobe` | radar and ESM systems | range/az/el with σ; bearing, power, frequency, PRF, PW; strobe bearing |
| Sensor track | `SensorTrack`, `EsmContact`, `StrobeTrack` | trackers | Kalman state and covariance; contact signature, ID, lines of bearing |
| Knowledge | `KnowledgeTrack` in a node's `KnowledgeBase` | own sensors, network messages, TDOA groups | origin (node, sensor, item), source, hops, position+covariance or bearing lines, velocity, identity + confidence, emitter signature, age |
| Picture | `FusedTrack` | `KnowledgeBase::Picture(now)` on demand | correlated and fused view; fixes from bearings; bearing-only items; stable display numbers |

A KnowledgeBase never merges destructively: it stores the latest report from each origin and
recomputes the picture, so losing a source just lets its entries age out, and the same report
arriving by two routes is not double counted (covariance intersection).

## 4. Data model (entity-component style)

`EmScene` holds plain arrays of components; *systems* are free functions that run over them.

| Component | Header | Notes |
|---|---|---|
| `Platform` | `model.hpp` | truth; `altitude_m`, `height_agl_m` derived per tick |
| `EmitterState` | `emitter.hpp` | carries a copy of its `EmitterDef`; `InAir()` = radiating and not into a dummy load |
| `RadarState` | `model.hpp` | def index, emitter id, on, MTI mode, beam programme, `Tracker`, lock state, last looks/plots, strobes |
| `EsmState` | `model.hpp` | intercepts this listen, contacts |
| `TdoaGroup` | `model.hpp` | sites (ESM ids) and the node that receives fixes |
| `JammerState` | `model.hpp` | on, aim point (briefed area, not a sensed position), RGPO victim, silent-until |
| `ChaffCloud` | `model.hpp` | RCS blooming then decaying, drifting with the wind |
| `NodeState` | `model.hpp` | a unit that knows things: platform, parent, nets, cabled sensors, `KnowledgeBase`, C2 status, schedules |
| `NetState`, `Message`, `LinkState` | `model.hpp` | radio nets, transmissions in flight, link margins |
| `EmLibrary` | `model.hpp` | definitions loaded from `data/em/*.json` |

Systems and helpers:

| File | Contents |
|---|---|
| `random.hpp` | counter-based draws (SplitMix64) |
| `linalg.hpp` | fixed-size matrices (2×2, 3×3, 6×6), inverses, symmetric eigen |
| `propagation.hpp` | free-space loss, knife-edge J(v), gases, path profile, reflection geometry, two-ray and plane-earth factors, `PathCache` |
| `emitter.hpp` | antennas (Gaussian main lobe + sidelobe floor), waveforms, scan, `InSweep`, EIRP |
| `detection.hpp` | Swerling I, accuracy formulas, received power, band overlap, J/N, burn-through, ellipse scales |
| `geolocation.hpp` | bearing fix (pseudolinear + ML), TDOA (Gauss–Newton), ellipses, CEP |
| `tracking.hpp` | Kalman CV filter, converted measurements, GNN association, M-of-N, coasting/dropping, single-target update |
| `knowledge.hpp` | knowledge items, signatures, covariance intersection, `KnowledgeBase::Picture` |
| `scene_ops.hpp` | platform update, scan rotation, `LinkLoss` (shared propagation), noise J/N in a look |
| `radar_system.hpp` | calibration, looks, plots, MTI/clutter/chaff, strobes, TWS tracking, tracking radar lock and RGPO |
| `esm_system.hpp` | intercepts, illumination windows, measurements, association, threat-library ID, TDOA groups |
| `knowledge_system.hpp` | own sensors → node knowledge |
| `comms_system.hpp` | radio calibration, links, nets, messages, relays, C2 status |
| `doctrine.hpp` | EMCON schedules, SAM doctrine (radiate/dark/designate) |
| `world.hpp` | `EmWorld`: construction helpers and the per-step order |
| `debug_draw.hpp` | overlay primitives by layer, JSON export |
| `data.hpp` | strict JSON loader with per-value provenance |
| `demo_scenario.hpp` | the MVP scenario (used by `em_demo` and Unreal) |

## 5. Signal representation

Per tick an emission is described as a **packet**, not a waveform:

| Field | Used for | Kept? |
|---|---|---|
| Carrier frequency (channel in use) | wavelength (propagation, antenna, multipath), band checks, ESM ID, jammer overlap | yes |
| Instantaneous bandwidth | receiver noise, jammer overlap (barrage vs spot) | yes |
| PRF / pulse width | ESM measurement and ID; range accuracy; CW flag | yes |
| Peak power, transmit loss | EIRP for ESM and J/N | yes |
| Antenna: gain, az/el beamwidths, sidelobe level | main beam vs sidelobe for every receiver; jamming geometry | yes |
| Pointing and sweep this tick; elevation beam | scan-on-scan, revisit, main-beam paints | yes |
| Agility (fixed / channels / pulse-to-pulse) | ESM association; spot jammer effectiveness | yes (data) |
| Polarisation | constant loss | not modelled (tier 4) |
| Modulation, pulse compression codes, individual pulses | — | not modelled (tier 4) |

This is the abstraction level of MIXR's `RfEmission` (frequency, bandwidth, PW, PRF, pulses per
packet, ERP, gain, polarisation, ECM flag). New characteristics can be added to `WaveformDef` and
read by the receivers that care, without touching the rest.

## 6. Propagation (`propagation.hpp`, `scene_ops.hpp::LinkLoss`)

```
one-way loss = FSPL(R, f)  +  J(v_max)  +  gases(R, h1, h2, f)  −  20 log10 F
```

* **Path profile** (`AnalysePath`): samples terrain + 4/3-Earth bulge along the ground path (100 m
  spacing, at most 600 samples); records line of sight, minimum clearance and the worst
  `h·√(1/d₁+1/d₂)`. That last number is frequency-independent: `v = it · √(2/λ)`, so one profile
  serves radar, ESM, jammer and radio at any band.
* **Diffraction**: ITU-R P.526 eq. (31), single dominant edge.
* **Gases**: ITU-R P.676 sea-level table, thinned with altitude.
* **Ground reflection**: ≥ 1 GHz: spherical-Earth two-ray with effective heights, effective
  reflection `ρ₀·exp(−8(π σh sinψ/λ)²)`, antenna-pattern weighting of the reflected ray (radar).
  < 1 GHz: plane-earth factor, with the excess loss the *larger* of plane-earth and knife-edge loss.
* **PathCache**: geometry per platform pair, reused until either end moves more than
  max(25 m, 0.2 % of the path length) horizontally or 25 m vertically.

## 7. Radar (`radar_system.hpp`)

**Calibration.** `Calibrate(def, env)` evaluates the full model at the published anchor (target at
the anchor's range and altitude over flat ground, best beam, multipath and gases included) with
`K = 1`, then sets `K = SNR_required(Pd_anchor, Pfa) / SNR_unit`. `SustainedDetectionRange` and
`DetectionRangeAt` report what the fitted model gives at other altitudes (the low-altitude anchor
is a *check*, not a fit).

**Search radar, per tick:** advance the beam programme at each revolution; grid query within the
instrumented range; for each candidate inside this tick's swept arc and near the active elevation
beam: `EvaluateLook` → SNR from the calibrated equation with aspect RCS, pattern loss in the active
beam, path loss and `F⁴`; clutter-to-noise in the cell (inside the radar's own ground horizon) and
MTI (improvement factor, notch around zero radial speed); chaff in the same cell (with internal vs
external coherence); noise jamming J/N with the receive gain towards each jammer while looking at
the target; `SINR = SNR/(1+J/N+C/N)`; Swerling I Pd; one keyed draw; on detection a plot with
range/azimuth noise from SINR and elevation known only to the beam. Plots feed the TWS tracker;
only tracks predicted inside the swept arc and the active beam can collect a miss. Jammers swept by
the main beam with J/N above threshold produce strobes, kept as angle-only `StrobeTrack`s.

**Tracking radar:** points at its designation or locked track (slew-rate limited; a low-target mode
holds a minimum elevation), looks inside its beam and range gate, keeps the best detection, updates
its single track; consecutive misses lose the lock. A range-gate pull-off repeater on the
illuminated platform captures the gate when its repeated power beats the skin echo by a margin,
walks it off at its rate, then falls silent so the gate sits on empty space.

**IFF:** an own-side platform with a working transponder in the beam replies with a probability; the
reply tags the plot, and confirmed tracks with recent replies become `Identity::Friend` knowledge.

## 8. Passive receivers (`esm_system.hpp`)

Every `esm_period_s` (0.2 s): for each emitter in air, in band, not culled by a free-space main-beam
bound: illumination geometry (main beam if the emitter's beam swept the receiver during the
listening window), EIRP towards the receiver, `LinkLoss`, received power vs sensitivity; an
intercept with bearing σ that worsens near the threshold, measured frequency/PRF/PW, TDOA arrival
time. Intercepts are associated into contacts with gates sized from the receiver's own accuracy;
drifting contacts of one emitter merge; contacts time out. Contacts store a line of bearing every
`line_interval_s` (with the receiver's position: a moving receiver builds a baseline), estimate the
scan period from main-beam paints, flag steady illumination (lock), and are identified against the
receiver's threat library (likelihood vs an unknown prior; CW flag; platform type → maximum speed).
TDOA groups cluster intercepts of one pulse across sites and multilaterate them.

## 9. Geolocation (`geolocation.hpp`)

* `BearingFix`: weighted pseudolinear least squares (Stansfield weights 1/(σr)²) for a start, then
  maximum-likelihood Gauss–Newton on angle residuals; rejects nearly parallel lines and fixes behind
  the receivers; old lines inflated by `v_max·age/r`. Returns covariance and χ².
* `SolveTdoa`: Gauss–Newton on range differences to a reference site, correlated-noise weighting
  `(I + 11ᵀ)⁻¹` by Sherman–Morrison; 3-D with ≥ 4 sites or 2-D with a known altitude and ≥ 3.
* `EllipseOf`, `CepM`, `EllipseScaleForProbability`.

## 10. Knowledge and fusion (`knowledge.hpp`, `knowledge_system.hpp`)

* Entries are keyed by origin `(node, sensor, item)`; newer reports replace older ones; bearing-only
  entries accumulate lines.
* Own sensors are re-ingested every 0.5 s (confirmed radar tracks, ESM contacts, strobe tracks) and
  removed when the sensor drops them; network entries age out (`max_age_s`, `drop_sigma_m`).
* `Picture(now)`: predict every entry to now (position moves with velocity; covariance grows with
  velocity uncertainty, or with an assumed speed if there is no velocity); cluster by Mahalanobis
  distance (never two tracks of one sensor together); fuse with fast covariance intersection
  (Niehsen); group bearing-only entries by emitter signature and triangulate them; tag a radar track
  with an emitter identity when a fix falls inside it; keep stable display numbers per origin.
* Identity: IFF → friend; hostile emitter type → hostile with the library's confidence; otherwise
  unknown. Kinematic classification gives a hedged type guess ("slow: helicopter or light aircraft").

## 11. Communications and C2 (`comms_system.hpp`)

* `RadioDef` (power, gain, height, sensitivity, required SNR, bandwidth, co-site desense, voice,
  message time, operator delay, tracks per message) with an optional **range anchor** that fits the
  system loss (and the co-site desense) to a published range, like a radar's calibration.
* `NetState`: a half-duplex channel. A message waits for the channel, then (voice) an operator delay,
  then occupies the net for `message_s` per chunk of tracks; a newer report replaces a queued one.
* Links are evaluated every second: received power vs sensitivity raised by co-site interference
  and by noise jamming in the radio's band (J/N). Delivery at the end of a transmission with
  probability `1/(1+exp(−margin/2))`.
* Every transmission makes the sender's radio emitter radiate for its duration (heard by ESM).
* Subordinates report their own-sensor items to their parent; command posts broadcast their picture
  re-originated at themselves; relays forward between nets once per message.
* C2 status from the time since the parent's last picture: connected (≤ 2.5 periods), degraded
  (≤ 5), autonomous.

## 12. Doctrine (`doctrine.hpp`)

`ApplyEmconSteps` runs scripted on/off steps. `RunSamDoctrine` reads only the node's picture: radar on
for a non-friendly air track inside the cue range (and known well enough) or when autonomous; off
after a quiet period while connected; designate the tracking radar inside the engagement range. Each
decision is an `EmEvent` with its reason.

## 13. Update rates and order

| Step (`EmWorld::Step`, dt = 0.1 s) | Rate | Why |
|---|---|---|
| Platforms: altitude, AGL, spatial grid | every tick | cheap, O(n) |
| EMCON steps and doctrine | every tick | cheap; decisions want 0.1 s resolution |
| Emitters: scan rotation, jammer pointing; chaff drift | every tick | scans are continuous |
| Radars | every tick | a search radar evaluates only what its beam swept (1/18 of the circle per tick at 33 rpm) |
| ESM and TDOA | 5 Hz | main-beam paints in between are caught analytically |
| Own sensors → knowledge | 2 Hz | displays and AI need no more |
| Links | 1 Hz | slow-changing |
| Messages and C2 status | every tick | message ends are timed events |
| Knowledge ageing | 1 Hz | |

In the game, `USimWorldSubsystem::Tick` runs the EM layer at a fixed 10 Hz (`FSimEmLayer`).
Strategic systems (higher command, a regiment's air picture) belong at 0.1–1 Hz.

## 14. Determinism

`Random` draws are `f(seed, stream, keys)` (SplitMix64 finaliser chain). A radar detection is keyed by
(radar, target, revolution), measurement noise by the same key on other streams, ESM by (receiver,
emitter, tick), message delivery by (message, recipient). Evaluation order, thread scheduling and
frame batching cannot change results; replays and lockstep comparison are exact. Tested: identical
seeds give identical event histories. The Python prototype reproduces the streams bit for bit.

## 15. Performance

Measured with `em_bench` (AMD Ryzen 5 3600, one core, Release, 60 s simulated at 10 Hz, hilly
200 m terrain grid over 300 × 300 km; a quarter of the aircraft radiating, one jammer per 40):

| Aircraft | SAM search radars | ESM sites | Mean per tick | Worst tick | Radar looks / tick | ESM evaluations / tick | Path-cache hits |
|---|---|---|---|---|---|---|---|
| 50 | 10 | 6 | 0.22 ms | 3.5 ms | 2 | 38 | 92 % |
| 200 | 30 | 12 | 2.6 ms | 23 ms | 24 | 510 | 91 % |
| 500 | 60 | 24 | 14.2 ms | 93 ms | 121 | 2,376 | 92 % |

At 10 Hz the 500-aircraft case uses ~14 % of one core on average. The worst ticks are the first
ones (every path profiled from cold). What keeps the cost down:

1. **Broad phase:** a uniform 10 km grid on east/north rebuilt each tick (air pictures are wide and
   flat, so a grid beats an octree); range checks before any physics.
2. **Sweep gating:** a rotating search radar only evaluates what its beam swept this tick.
3. **Receiver cadence:** ESM at 5 Hz with analytic main-beam windows (halved the ESM cost).
4. **Cheap bounds before expensive work:** an ESM pair is culled if even the free-space main beam
   cannot reach the sensitivity.
5. **Frequency-independent path cache** with range-relative tolerance (65 % → 92 % hits; together
   with 3, the 500-aircraft case went from 50 ms to 14 ms per tick).
6. **Counter-based random numbers** make parallelism safe.

Next steps when needed (roadmap E8): split ESM work across threads (receivers are independent; the
path cache becomes per-thread or read-mostly), time-slice cold-cache profiling, a hierarchical
max-height grid to skip profiles whose ray clears the highest terrain, and level-of-detail for
far-away units (aggregate emitters, slower knowledge updates).

## 16. Data (`data/em/`)

* One file per library: `radars`, `esm`, `jammers`, `radios`, `emitters` (stand-alone), `threat_libraries`.
* `"provenance": "per_value"` (real systems): every physical value is `{value, source, confidence,
  notes}`; the loader refuses a plain number and records every value (`EmLoadResult::values`).
  `"provenance": "placeholder"` (tests, demos): plain numbers, `source` "PLACEHOLDER".
* Radars must have a `calibration` anchor; a `low_altitude_check` is reported, not fitted.
* Radios may have a `range_anchor` (and `cosite_range_m`).
* Threat libraries belong to a side and describe *that side's belief* (`owner`, `platform`, `cw`).

Adding a radar is data only:

```json
{ "id": "example_search_s", "name": "Example S-band search radar", "role": "search",
  "emitter": { "freq_hz": 3.0e9, "prf_hz": 1000, "pulse_width_s": 2e-6, "peak_power_w": 500000,
               "antenna": { "gain_db": 33, "beamwidth_az_deg": 1.5, "beamwidth_el_deg": 6 },
               "scan": "circular", "scan_period_s": 6.0 },
  "noise_figure_db": 4, "max_range_m": 150000,
  "calibration": { "range_m": 120000, "rcs_m2": 5, "target_altitude_m": 8000, "pd": 0.5,
                   "source": "…", "confidence": "medium" },
  "source": "…", "confidence": "medium" }
```

## 17. Debug overlays

`BuildDebugScene(scene, layers, picture_node, reference_rcs)` returns primitives (line, circle,
ellipse, sector, text, point, polygon) in ENU metres with a layer bit and an owner node:

| Bit | Layer | Shows |
|---|---|---|
| 1 | truth | real positions, velocity leaders |
| 2 | emitters | who is radiating (radar amber, jammer magenta, radio cyan) |
| 4 | beams | antenna pointing and scan sectors |
| 8 | coverage | instrumented range; calibrated R50 ring for a reference RCS |
| 16 | line_of_sight | radar → candidate this tick: clear (solid) or masked (dashed) |
| 32 | signal_paths | emitter → receiver for every intercept (bright = main beam) |
| 64 | bearings | ESM lines of bearing with ±σ wedge and ID; bearing-only knowledge |
| 128 | fixes | triangulated/TDOA positions with 95 % ellipses and the lines used |
| 256 | uncertainty | 95 % ellipses of a node's fused picture with identity, type, CEP, sources |
| 512 | tracks | sensor tracks with velocity leader, 2σ ellipse, quality, IFF |
| 1024 | c2_links | radio links coloured by margin (dB) |
| 2048 | comms_traffic | transmissions on the air |
| 4096 | jamming | jammer beams, strobes, the jammed detection envelope |
| 8192 | sensor_status | radar modes, node C2 status |
| 16384 | detection | Pd, SINR and J/N of each look |

Front ends: Unreal (`sam3d.em.Debug <mask>`, `sam3d.em.Owner <node>`), the browser viewer
(`tools/em_view.py`, from `em_demo`'s JSON), Tacview (`em_demo`'s `.acmi` with the command post's
picture as separate objects).

## 18. Unreal integration (current)

`FSimEmLayer` (`unreal/SAM3D/Source/SAM3D/Public/SimEmLayer.h`) owns an `EmDemoScenario`, is
advanced at a fixed 10 Hz from `USimWorldSubsystem::Tick`, and draws its overlays every frame with
`DrawDebug*` (primitives rebuilt only after an EM step). It uses the active sim world's origin and
terrain, and is stopped when either changes. API: `StartEmDemo`, `StopEmDemo`, `IsEmDemoRunning`,
`GetEmTimeSeconds`, `GetEmStatusText`; console `sam3d.em.Start`, `sam3d.em.Stop`, `sam3d.em.Debug`,
`sam3d.em.Owner`. The Osa scenario's own radars use the EM layer through `EmSensorLink` (§23); the Unreal
demo of §18 is still a separate synthetic battlefield.

## 19. Networking (design, not built)

Run simcore on the server only. Replicate to each player: their own vehicle's console and equipment
state, and **their node's knowledge** (fused picture, ESM contacts, strobes) — never the truth list.
Knowledge is small (tens of items) and already filtered by what that unit could know, so it doubles
as anti-cheat. Deterministic draws make server-side replays and desync investigations exact.
Co-operative crews share one node.

## 20. Testing

| Level | Where | What |
|---|---|---|
| Maths vs hand calculations | `simcore/tests/test_em_models.cpp` | FSPL, J(v), gases, two-ray/plane-earth, Swerling, accuracy, overlap, burn-through, calibration, ellipses, bearing fix, TDOA, Kalman tracking, CI, knowledge ageing |
| Statistical honesty | same | Monte Carlo: reported ellipses contain the truth as often as they claim (NEES ≈ 2) |
| Data and calibration | `simcore/tests/test_em_data.cpp` | loaders (strict provenance), Osa-AK ranges vs Korovin and the Polish manual, R-123M fit |
| Behaviour scenarios | `simcore/tests/test_em_scenarios.cpp` | Pd vs range, terrain masking, passive detection without illumination, triangulation vs station count, noise jamming and strobes, self-screen burn-through, track loss, MTI notch, C2 destruction, network cue, VHF over a ridge, comms jamming, RGPO, chaff vs coherence, TDOA, passive ranging (ESM pod vs RWR), determinism |
| End-to-end | `simcore/examples/em_demo.cpp` (CTest `em_demo`) | the eleven MVP behaviours in one scenario |
| Cross-language | `prototype/tests/test_em.py` | Python mirror reproduces C++ numbers (loop gains, ranges, fix covariance, random streams) |
| Tools | `tools/tests/test_em_view.py` | viewer generation |

## 21. Extending

| Future system | What it needs |
|---|---|
| SA-6 (Kub) 1S91 | Search (C/G band) and CW illuminator: a `track` radar plus a `missile_uplink`-style CW emitter; data only |
| SA-9/SA-13 | IR seekers: an optical sensor model (new system, same knowledge) |
| SA-10 / long-range radars | Data (search, track); phased-array scan = sector/agile scheduling (new `ScanKind`) |
| MiG/Su and NATO fighters | Airborne radars as `RadarDef` with sector scan (supported), RWRs as `EsmDef`, pods as jammers |
| AWACS, EW aircraft, ELINT aircraft | Data; long-range nodes; datalink radios |
| Artillery-locating and ground surveillance radars | Data; ground targets |
| Civilian emitters (ATC, broadcast) | Emitters with neutral-owner library entries: clutter for ESM operators |
| Ships | Platforms; sea-surface reflection (`surface_reflection` per area) |

## 22. Known limitations

* Single dominant knife edge (no Deygout multi-edge); no buildings, forest canopy or land-use clutter.
* Two-ray multipath uses a smooth-earth reflection point; real terrain reflections are not traced.
* Search-radar elevation measurement is the beam centre; no height-finding radars yet.
* Tracker is GNN (no JPDA/MHT); no manoeuvre-adaptive filter.
* VGPO, angle deception against non-monopulse radars, repeater false targets and sweep jamming not
  yet; frequency-channel changes are data-only (no crew action).
* Comms: no message priorities, encryption or interception value; relays forward everything.
* Doctrine is minimal (one SAM rule set); no ARM detection or relocation yet.
* The Osa's two radars are connected (§23); its IFF replies, TV channel, missiles and the crew's jamming
  response are not.
* The tracking radar locks on the first echo in its gate, and Swerling I detection falls off slowly (Pd 0.24 at
  28 km for the Korovin target), so the raw lock comes at 27 km. The Osa's auto-capture is modelled as waiting for
  22 hits in the last 40 looks (`EmSensorLink::Captured`, a local Pd of about 0.55), which puts it at 21 to 25 km,
  median 23 km (Korovin: 23 km). The window and count are a model choice, not published.

## 23. The Osa's radars (phase E2)

`simcore/em_link.hpp`: `EmSensorLink` owns an `EmWorld` for one scenario unit. A scenario turns it on with an
`own_unit.em` block (`data/scenarios/README.md`); everything else keeps the gameplay model.

* **Each step** (`EngagementModel::StepEm`): the unit's position, each target's position, velocity, RCS and
  alive flag go in as platforms; the console says whether the search radar radiates (antenna rotating and
  transmitter ready) and whether MTI is selected, and whether the tracking radar radiates; the EM layer runs in
  fixed 0.1 s ticks.
* **Search picture** (`SearchPicture`): the search radar's confirmed tracks whose last look was a hit, one per
  target. The search radar runs its three elevation beams one per revolution, so a blip is refreshed once per
  revolution (1.8 s), from the radar's measurement, and high targets are seen at shorter range (beam III). The
  console's range-scale window still decides what is displayed. The scenario's target id is the truth platform the
  track came from; positions the crew sees are the measured ones.
* **Tracking radar** (the engagement model takes a lock once `Captured()`: 22 hits in 40 looks): with the acquisition mode engaged, the transmitter ready and the antenna free to turn, the
  engagement model puts a designation at the wheels' azimuth (true, through the gyro), the antenna elevation (the
  elevation search's scan, or the elevation wheel) and the range wheel. The antenna slews there, looks only in its
  beam and range gate, and locks on a detected echo; the model accepts the lock after `lock_time_s`, then the servo
  follows the radar's filtered estimate. Losing the echo (four missed looks, terrain, a hole in the multipath
  lobing) breaks the lock after ten missed looks (one second): "track lost: radar lost the echo".
* **Range indicator** (`EchoVisible`): the tracking radar's echoes are the targets its beam really picks up
  (pattern, range, Pd), whatever the gate, so the elevation seat sees a target appear as the scan crosses it.
* **What changes, what does not**: the clutter height and the angular-rate, minimum-altitude and beam-crossing
  rules are replaced by the radar equation, MTI/clutter and the antenna pattern. TV tracking, the IFF replies,
  the zone lamps, the НЛЦ lamp, the crew's aiming speeds and the hit/miss outcome are unchanged.
* **Bands** (`simcore/tests/test_em_link.cpp`): for the practice drone (0.5 m2 at 1 km) first contact lies between
  the search radar's R50 (28 km) and R10 (37.5 km) minus the revolution it takes to confirm (25 to 34 km over 10
  seeds); the tracking radar captures it at 13.3 to 15.2 km (its R50 is 14.8 km). A 3 m2 target at 5 km is
  locked at 27 km and auto-captured at 21 to 25 km with a perfect
  designation.
* **Not synchronised**: the scope's own sweep (`SearchScope`) paints the picture on its own phase; the EM radar's
  sweep decides when a track is updated.

## 24. The search radar's beams, rings and coverage (roadmap 6.4)

* **Beam selector.** `sensors.search.beams` in the console file maps each position of `soc.beam` to a programme (beam
  indices, one per antenna turn): I, II, III hold a beam, АВТ. I-III steps through all three, АВТ. I-II through
  the lower two. With the EM layer the search radar really steps through it
  (`EmSensorLink::SetSearchBeamProgram`; a new programme takes effect at the next turn); without it the engagement
  model steps the same programme by the antenna's turns (`rotation_period_s`). `Engagement().SearchBeam()` is the beam
  radiating now.
* **Beam lamps.** The cabin's `beam_1..3` lamps are lit by the inputs `search_beam_1..3`, which the engagement model
  sets from the active beam: they step with the antenna in the automatic programmes.
* **Which beam an echo came in.** `Contact::beam` (EM only). A blip is held on the picture for one full programme
  (three turns in АВТ. I-III), so a target seen only in beam I is refreshed every third turn; it drops off when it
  leaves coverage.
* **What the beams do** (tests in `test_em_link.cpp`, 3 m2 target inbound): beam I shows a 1 km target from over
  35 km and nothing at 5 km altitude; beam II shows the 5 km target from over 35 km and a 50 m one not at all
  until it is close; beam III is tall and weak, so everything it shows is inside 28 km. Nothing in the practice
  scenario needs the operator to pick: the crew starts in beam II on the march and goes to АВТ. I-III to acquire.
* **Low targets.** First contact on a 3 m2 target is 42 km at 5 km altitude and 21 km at 50 m (Korovin: 40 and 27;
  the 50 m R50 is 26 km). The tracking radar's auto-capture of a 50 m target is 22 km, not Korovin's 14 km. The full-path Pd of that
  target is smooth (0.5 at 22 km, 0.8 at 18 km); E0's calibration check gets 14 km at Pd 0.8 only from a deep
  multipath null in its own simplified path at 17 km, so reading his figure as a Pd 0.8 range does not hold. An
  open item, pinned by a test: closing it needs a rougher surface or another trials-site effect.
* **Overhead blind cone.** The search radar's elevation limit (30 degrees) makes it: a target at 4 km altitude is not
  shown within 6.9 km of the unit and comes back out the far side (test).
* **Normal video and MTI.** The EM clutter model hides a 30 m target from 12 km in on normal video; with MTI on it is shown
  2.5 times as long over the same pass (test). The 35 km limit of the console's MTI is not applied in the EM path.
* **Range rings.** `sensors.search.range_rings_m`: per scale, 2, 7 and 12 km on 0-15 km, multiples of 5 km inside the
  others (ePap, `samsim_thread`). The data is loaded; drawing it is up to the display.


## 25. Review of the E2 integration, and what was done about it (October 2026)

`simcore/em_link.hpp` (`EmSensorLink`) and its use in `engagement.hpp` were read against sections 1-14 on 11 October
2026. The wiring kept the layering where it matters and the numbers are honest. Findings and their fixes:

1. **Truth reached the crew through `debug_truth_platform`** (the link turned a sensor track into the scenario's target
   id with it). *Fixed.* The link no longer reads the debug fields. A confirmed track becomes a contact labelled with
   the scenario target it lines up with in angle (`ResolveTarget`, a referee-side association with gates of 2.5 deg in
   azimuth, 10 deg in elevation and, for the search radar, 1.5 km in range; each target is claimed by one track); a
   track that lines up with no target (chaff, a false echo) is a **ghost** contact `T<n>` (`Contact::ghost`) that
   reaches the scope, the crew's aiming and the debrief. The tracking radar's lock resolves by angle only (a range-gate
   pull-off must not make a lock look like a ghost); a lock on a ghost is not a capture. Not done: the range
   indicator's echoes (`EchoVisible`) are still computed per truth target, so chaff does not show there yet.
2. **Antenna height.** *Fixed.* The platform's antenna stands at the search radar definition's `antenna_height_m`
   above the ground (`EmSensorLink::Antenna`), the height the radar was calibrated at, whatever the hull's `up_m`.
   Contact geometry and the tracking designation use the same point. (The tracking radar is calibrated at 4.5 m, the
   search radar at 5 m; the platform uses 5 m.)
3. **Capture constants.** *Fixed.* `em_capture_looks` / `em_capture_hits` are in `data/gameplay/osa_ak.json` (`track`),
   loaded into `EngagementTuning`, validated, and given to the link every step; the reasoning is in that file's notes.
4. **Jammers and chaff.** *Fixed.* A scenario's `own_unit.em` takes `extra_libraries` (e.g. `em/nato_1985.json`),
   `jammers` (on a scenario target, self-screening, or at a stand-off point, aimed at the unit, with on/off times),
   `chaff` (a cloud dropped at a target's position at a time) and `wind_mps`. The radar equation does the rest:
   noise J/N (a stand-off band-8 jammer cut first contact from 38 km to 10 km in the test), chaff in the cell and as
   false plots (the MTI notch passes it only if it drifts faster than 15 m/s). `data/scenarios/osa_practice_em_ew.json`
   shows both. Not done: ESM, nets and a jamming response by the crew.
5. **The unit as a node.** *Fixed.* The unit is an `em::NodeState` with both radars cabled to it, so its tracks are
   knowledge (`EmSensorLink::Knowledge`, `UnitPicture`), and `ReceiveReport` lets another node's report fuse with them
   (covariance intersection; tested). The crew's scope still reads the radar tracker directly (hold times and
   per-revolution refresh are pinned by tests); moving it onto the fused picture belongs with E4.

Also done with it (the E2 remainder): **IFF replies** now come from the EM layer. The interrogator is the search
radar's (`SetInterrogating`); an own-side platform with a working transponder in the beam replies with probability
0.9 per detection; the reply tags the track (`Contact::iff`, held 2.5 s) and the engagement model's reply list and
debrief use it. A target that is not a friendly transponder never replies.

Still not electromagnetic in the Osa link: the TV channel, missiles and the command link (E5), the crew's response to
jamming (E6), codes and modes.

Two smaller observations that needed no change: `beam_of_track_` and the capture look record live in the link rather
than in the EM scene (rebuilt per run, deterministic, but not in the overlays); and nothing contradicts determinism,
the update rates or the data rules.

## 26. Command sources (E4, started 11 October 2026)

The battery's air picture is not tied to the PU-12. `simcore/c2_sources.hpp` (`C2Network`) holds selectable sources, each with its own plotting board: the PU-12 (main, from `command_post`), and library types (EWR voice net, division early-warning net, neighbouring battery, observation post, data link; `data/command/c2_sources.json`). Sources share voice nets (one message at a time), go down (outages, `c2_fail`, link range, a `C2LinkModel`), and the crew listens to one at a time: `RequestSource` (player/UI/trigger) or the NPC commander's fallback on silence (2.5 / 5 periods, as the EM C2 status) and return to the main source. The EM link budget is now the `C2LinkModel` for sources with a position (`EmSensorLink::AddRadioSource`). Voice designations as EM messages are still to do. Evidence: `docs/research/EM_C2_SOURCES_2026-10.md`.

## 27. Environment depth (E9, started 16 October 2026)

* **Deygout diffraction.** `AnalysePath` keeps, besides the main edge, the worst crest on each side of it measured against the line to the main edge's top (`sub_v_geometry_a/b`); `DiffractionLossDb` sums J(v) over the three. A sub-edge must be a crest of its own, with a 20 m valley between it and the main edge, so the flat top of one wide ridge stays one edge. Sub-edges are only looked for when the main edge blocks the ray. Two 300 m ridges on a 20 km path: 21 dB at 50 MHz for one, 38 dB for two.
* **Rain.** `Environment::rain_rate_mmh` (scenario `own_unit.em.rain_mmh`) adds ITU-R P.838-3 specific attenuation (k R^alpha, horizontal polarisation, 1-40 GHz table; f^2 below 1 GHz) times the P.530 distance factor to a path's atmospheric loss, for radars and radios. Radar and radio calibration always uses the dry environment (`EmWorld::Dry`), so rain lowers ranges instead of being fitted away. 10 GHz, 50 mm/h: 1.66 dB/km; over 30 km 17.7 dB.
* Left: forest and urban obstacle heights (needs a land-cover layer), sea areas and ducting.

## 28. Scale (E8, started 16 October 2026)

`em_bench data 500 60 24 [threads]` now prints the time per phase of `EmWorld::Step` (`EmRates::profile`) and a checksum of the result. At the start of E8 the mean tick was 16 ms on one core, 10 ms of it the ESM sites (every site evaluates every emitter, none culled) and 4 ms the radars.

* **Lookups.** `EmScene::FindIn` is a binary search (ids are added in increasing order and never reordered; a miss falls back to a scan).
* **Per-site passes.** `RunEsm` runs each ESM site as an independent unit and `RunRadars` each search radar: a site or radar owns its tracker or contacts, its own `PathCache` (`EsmState::paths`, `RadarState::paths`, `LinkLossCached`), its counters and its event list; counters and events are merged in site order afterwards. Chaff no longer needs the scene mutated for a look (`skip_chaff_id`). Tracking radars run after the search radars, in order, on one thread (a repeater's gate pull-off changes a jammer all radars see). `EmRates::threads` (default 1) sets the worker count; the result is **identical** at any count (test `em scale`, and the bench checksum `eee3c1357697d113` at 1, 4 and 8 threads).
* **Result.** 14.7 ms per tick on one core, 5.7 ms on 4 threads, 4.2 ms on 8 (goal: 5 ms).
* Side effect: path profiles are no longer shared between a site, a radar and the links, so a profile is computed once per cache rather than once; the hit rate is similar.
* Left: time-slicing cold paths (TDOA, knowledge ageing), a hierarchical grid for the radar candidate queries, level of detail for distant units, `em_bench` in CI as a trend.

## 29. Deception and EMCON depth (E7, done 16 October 2026)

`simcore/em/deception.hpp`, false targets in `radar_system.hpp`, tests in `simcore/tests/test_em_deception.cpp`. All numbers in tests and in `data/em/generic.json` are placeholders.

* **Decoys on the radar's parameters** (`EmWorld::AddDecoy`, `DecoyLink`; scenario `own_unit.em.decoys` copies the Osa's search radar). A decoy copies the radar's transmission (frequency, PRF, pulse width, scan) and follows its frequency; it radiates with the radar, while the radar is off (blinking), or always. The Czechoslovak army's own view was that radio-electronic deception measures should scatter enemy strikes (`vr_1985_plss`). Effect, without any special rule: the receivers identify the decoy as the radar, and because bearings to radar and decoy carry one signature, the enemy's DF net fixes the emitter off the radar (test: 0.5 km from the radar alone, 4.4 km with a decoy 9 km away).
* **Dummy positions** are a decoy plus a ground platform with the echo the scenario gives it; nothing more is needed in the EM layer.
* **Frequency change against a spot jammer.** A radar moves to another of its preset channels (`EmWorld::SetEmitterChannel`, `waveform.channels_hz`); a spot jammer set on it (`JammerState::victim_radar`) follows after `JammerDef::retune_s` (0: never). Jammers now jam at their own emitter's frequency, so a re-tune is real. Test: strobes stop at the change and return after the 8 s re-tune. The Osa's own channels are not in its data (no source).
* **False-target repeaters** (`JamTechnique::FalseTargets`): swept by the main beam and loud enough above the radar's noise, the repeater adds a string of echoes behind itself on its bearing. They are plots like any other, so the tracker builds ghost tracks (test: one real, four ghosts). Repeaters are not noise: no strobe, no J/N. An abstraction (count, spacing, J/N needed), not a waveform model.
* **Velocity-gate pull-off: not applicable.** No radar here tracks in Doppler; the Osa's tracking radar is a pulse radar with MTI (`mon_2112_80` p15-17).
* **Dummy radio traffic and the intercept value of voice** (`EmWorld::AddDummyTraffic`). Every transmission radiates, so radio intercept hears and direction-finds it. A silent battery is not found; a dummy station is; once the battery's own voice designations go out on the same net, the two blend into a fix between them. Not modelled: what an intercept reveals by its content.

## 30. Multiplayer knowledge (E10, simcore side done 16 October 2026)

The server runs the one simulation; each player receives what *their own node* knows, never the truth (`simcore/em/replication.hpp`, tests in `simcore/tests/test_em_replication.cpp`).

* **KnowledgeView** (`BuildView(scene, node)`): the node's fused picture (id, identity, category, source, position, velocity, horizontal sigma, time, fix / bearing-only flags with the newest bearing line, type guess), the contacts of the ESM receivers cabled to it (bearing, sigma, last seen, hostile, steady, what its own threat library calls the signal), the tracks of the radars cabled to it, its own position and its command status. It is built from knowledge structures only and has **no field** for platforms, emitters, origin keys, names or any `debug_truth_*`: what it cannot hold, it cannot leak. Test: a red view holds nothing near a blue aircraft its radar cannot see, and a canary string in the platforms' names never reaches the bytes.
* **Wire format**: `SKV1`, little-endian, fixed quantisation (1 m, 0.01 m/s, 1e-5 rad, 1 ms), items sorted by key; one view always encodes to the same bytes (decoded once, re-encoded identically). Truncated or foreign packets are refused.
* **KnowledgeReplicator** (server): a numbered stream per client; each packet is a delta against the newest view that client has acknowledged (items new or changed, keys removed), or a keyframe when there is no usable base (64 views kept). Works over an unreliable transport: a lost packet is simply never acknowledged.
* **KnowledgeMirror** (client): applies keyframes and deltas onto the base it kept, ignores duplicates and late packets, acknowledges the newest view it holds.
* **Soak test**: 10 simulated minutes, two clients (red battery, blue ESM), sends every 0.5 s over a channel that loses 25 % each way, delays 0-3 sends (reordering) and duplicates 5 %: 1477 views applied, every one byte-identical to what the server sent under that number, 4 keyframes per 1200 packets, about 0.8 KiB/s for both, both clients converged at the end.
* A moving track changes every item, so a delta carries about as much as a keyframe for a busy picture; value-level delta coding is possible later if bandwidth matters.
* **Unreal transport (built 16 October 2026)**: `USimKnowledgeComponent` on a PlayerController. On the server it registers with `USimWorldSubsystem`, which after the sim steps (`PumpKnowledge`, by sim time) sends each player the view of its `NodeId` (0: the scenario unit's node, `EmSensorLink::UnitNodeId()`) every `SendPeriodSeconds` (0.5 s) through an unreliable client RPC; the client applies it with a `KnowledgeMirror`, fires `OnKnowledgeUpdated`, and acknowledges by an unreliable server RPC. Blueprint: `GetPictureTracks`, `GetEsmContacts`, `GetSensorTracks` (Unreal coordinates), `GetKnowledgeSeq`, `GetKnowledgeSummary`. The scene is the scenario's EM layer, else the EM demo (`GetActiveEmScene`). Automation test `SAM3D.Knowledge.ComponentAppliesStream` (headless) feeds the component a keyframe and deltas and checks it holds byte for byte what the server sent. Since then: a networked check with a dedicated server and two clients, the client's own simulation switched off, and the scopes' feed (`em::ViewScope`) carried in each view (`docs/MULTIPLAYER.md`).

