# Osa-AK in the electromagnetic layer: specification (October 2026)

How the 9K33M2 Osa-AK (9A33BM2 combat vehicle, 1980s) is represented in `simcore::em`, what is known
about each of its emitters and receivers, how its operating modes and crew actions map onto the
model, how it reacts to jamming and deception, and what remains unknown. Data:
`data/em/osa_ak.json` (per-value sources). Architecture: `docs/EM_ARCHITECTURE.md`. Background:
`docs/research/OSA_RESEARCH_2026-10.md`, `docs/research/EM_RESEARCH_2026-10.md`.

Labels: **Confirmed** (tier-1/2 source states it) · **Inferred** (follows from confirmed facts by
textbook reasoning) · **Approximation** (a modelling stand-in for something not public) ·
**Unknown**. Public sources only: the restricted 1991 Voenizdat textbook used elsewhere in the
project is **not** used for any RF parameter here.

## 1. The vehicle as EM entities

```
9A33BM2 platform (ground, antenna block ~5 m up when deployed)
├─ СОЦ  search radar (Polish RSWP) ............ emitter + receiver, rotating, 3 elevation beams
│    └─ НРЗ IFF interrogator ................... emitter (antenna built into the search antenna)
├─ ССЦ  tracking radar (Polish SSC) ........... emitter + monopulse receiver, single target
├─ ССР  missile-tracking radar, 2 channels .... receiver of the missiles' responders (wide/medium/narrow beams)
├─ СПК  command transmitter ................... emitter (uplink to the missiles)
├─ ТОВ  TV-optical sight ...................... passive (no emission): the EMCON and anti-jam fallback
├─ 2 × R-123M VHF radios ...................... emitter/receiver on the battery voice net
└─ TA-57 field telephone (at a halt) .......... wire: no emission
```

No radar warning receiver or ESM set on the 1980s Osa-AK (**Confirmed** by absence in
`mon_2112_80`; the L-150 passive DF set, 8–18 GHz, 3°, belongs to the post-Soviet Osa-1T upgrade,
`korovin_tiv_2010`, and is out of period).

## 2. Emitters and receivers: what is known

### 2.1 Search radar (СОЦ / RSWP) — `osa_ak_soc`

| Parameter | Value used | Label | Source |
|---|---|---|---|
| Type | Coherent-pulse search radar, magnetron with automatic frequency control, travelling-wave-tube front end; coherent on receive | Confirmed (inference for "coherent on receive") | `korovin_tiv_2010`; `mon_2112_80` p13, p15 |
| Band | Centimetre band; **7 GHz used** | Confirmed (band) / Approximation (value) | `korovin_tiv_2010`; Western reports "H band, 6–8 GHz" (`western_bands`) |
| Working frequency | One of a fixed grid, set per vehicle; neighbouring vehicles separated | Confirmed | `mon_2112_80` p67 |
| Peak power | 250 kW | Confirmed for the 9A33B; assumed for the AK | `korovin_tiv_2010` |
| Receiver sensitivity | ~10⁻¹³ W | Confirmed (9A33B) | `korovin_tiv_2010` |
| Pulse width / PRF | 0.4 µs / 2 kHz | Approximation (sensitivity ⇒ ~2.5 MHz bandwidth with a ~10 dB noise figure; PRF keeps 75 km unambiguous) | — |
| Azimuth beam | 1° | Confirmed | `korovin_tiv_2010` |
| Elevation beams | I 0–4°, II 4–8°, III 8–27° (4°, 4°, 19°); one beam per turn; programmes I–III, I–II or a fixed beam | Confirmed | `korovin_tiv_2010`, `flak11`, `mon_2112_80` p15, p21 |
| Antenna gain | ≈ 38 dB in beams I/II (≈ 31 dB in III) | Inferred from beamwidths (η ≈ 0.6) | — |
| Rotation | 33 rpm (1.82 s) | Confirmed | `korovin_tiv_2010`, `flak11`, `mon_2112_80` p15 |
| Stabilisation | Antenna platform stabilised (search on the move and afloat) | Confirmed | `mon_2112_80` p16; `korovin_tiv_2010` |
| Coverage | 45 km, 0–30° | Confirmed | `flak11` |
| MTI | "Fixed-echo suppression" (TES/СДЦ): double inter-period canceller shared with the SSC range channel; **internal** coherence (phase from the transmitter pulse) or **external** coherence (re-phased by the interference: cancels drifting chaff) | Confirmed | `mon_2112_80` p15, p23; `korovin_tiv_2010` (external coherence added with the Osa-A) |
| MTI improvement, notch width | 25 dB, ±15 m/s | Approximation | — |
| Detection, fighter | 40 km at 5 km altitude; 27 km at 50 m (original Osa) | Confirmed | `korovin_tiv_2010` |
| Detection, MiG-21 type (Osa-AK) | 20–35 km "depending on altitude" | Confirmed (Pd not stated) | `mon_2112_80` p10 |
| IFF | Interrogator integrated with the search antenna; replies shown on the PPI | Confirmed | `mon_2112_80` p14, p16, p21 |

### 2.2 Tracking radar (ССЦ / SSC) — `osa_ak_ssc`

| Parameter | Value used | Label | Source |
|---|---|---|---|
| Type | Monopulse, two-channel (main + additional) receive; "protects the SSC against responsive angle jamming and increases tracking accuracy" | Confirmed | `mon_2112_80` p16–17 |
| Band | Centimetre band; **14.5 GHz used** | Confirmed (band) / Approximation (value) | `korovin_tiv_2010`; Western "J band 14.5 GHz" |
| Peak power | 200 kW | Confirmed (9A33B); conflicts: 180 kW (SAM Simulator), 18 kW (a `flak11` page) | `korovin_tiv_2010` |
| Sensitivity | 2 × 10⁻¹³ W | Confirmed (9A33B) | `korovin_tiv_2010` |
| Beam | 1° | Confirmed | `korovin_tiv_2010` |
| Gain | ≈ 44 dB | Inferred | — |
| Pulse width / PRF | 0.2 µs / 3 kHz | Approximation | — |
| Range / angles | 0–28 km; −12° to +78°; azimuth slew 15°/s | Confirmed | `flak11` |
| Range gate | In TES only a 1.5 km segment around the tracked target is shown: gate ±750 m | Confirmed | `mon_2112_80` p23; `samsim_osa` p24 |
| Capture to automatic tracking | 23 km at 5 km altitude; 14 km at 50 m (original Osa) | Confirmed | `korovin_tiv_2010` |
| Low-target mode (НЛЦ/NLC) | Antennas held at a fixed elevation "to eliminate distortion of the antenna pattern by the ground"; lamp at 0-06…0-10 mils; **0.48° used** | Confirmed (mode) / Inferred (the hold angle ≈ the lamp threshold) | `mon_2112_80` p17, p23; `flak11`; `samsim_osa` p25 |
| MTI | Yes (against passive jamming) | Confirmed | `korovin_tiv_2010` |
| Under strong noise jamming | Angles from the TV sight, range from the search radar (much larger energy potential), three-point method | Confirmed | `korovin_tiv_2010`; `mon_2112_80` p12 |
| Angle-only guidance on a jammer ("method Н") | Modelled as `angle_track_on_jam` | Confirmed by the game's creator and a Greek Osa officer (tier 2/3) | `samsim_thread` |
| Range-gate pull-off susceptibility | 0.5 per look | Approximation (nothing public) | — |

### 2.3 Other emitters

| Item | Known | Label | In the model |
|---|---|---|---|
| ССР missile-tracking radar | Two identical channels; wide beam captures, medium brings in, narrow guides; main and additional channels; carrier frequency **B** on all vehicles, **A** only by special permission | Confirmed | Not yet (receives the missiles' responders; needs a missile platform: phase E5) |
| СПК command transmitter | Fixed frequencies; neighbouring vehicles interleaved (1st vehicle 3 and 1, 2nd 4 and 2, 3rd 7 and 5, 4th 8 and 6); switched on at ВНИМАНИЕ; checked into the equivalent load in peacetime | Confirmed (`mon_2112_80` p67; `flak11`) | Not yet: RF parameters **Unknown**. When added: an emitter that comes on before launch — the launch cue a Western RWR could use (**Inferred**, not claimed) |
| НРЗ IFF interrogator | Integrated antenna; Kremniy-2 family in the 1980s (Parol from 1977 in Soviet service, late 1980s in the NVA) | Confirmed (integration) / Inferred (family) | IFF modelled as replies from own-side transponders (§2.1); interrogation emission **Unknown** band, not yet an emitter |
| 2 × R-123M | 20–51.5 MHz, 4 presets, 20 W, 25 kHz spacing, ~2.5 µV | Confirmed (two radios: `mon_2112_80` p26) / tier 4 (specs) | `r123m`, fitted to the manual's ranges (§4) |

## 3. Operating modes and crew actions → model state

| Real control or mode (console id in `data/procedures/osa_ak.json`) | Seat | EM model state |
|---|---|---|
| ПОИСК, ВЫСОКОЕ (`soc.search`, `soc.hv`) | Senior operator | `RadarState::on` for `osa_ak_soc`; with the transmitter on into its equivalent (peacetime checks, `mon_2112_80` p67) → `EmitterState::dummy_load` |
| ЛУЧ I / II / III / АВТ. I–III / АВТ. I–II (`soc.beam`) | Senior operator | `RadarState::beam_program` = {0}, {1}, {2}, {0,1,2}, {0,1} |
| СДЦ / ШТ. (`soc.mti`, `ssc.mti`) | Senior operator / operator | `RadarState::mti` = Internal / Off; **the internal/external coherence selector is not in the console data yet** (open item for the console map) |
| ЗАПРОС, ЗАПРОС К, КЛАПАН (`iff.*`) | Senior operator | `RadarState::iff_interrogating` (gated mode = presentation) |
| ДАЛЬНОСТЬ scale (`soc.range`) | Senior operator | Presentation only (the PPI scale) |
| ЦУ (`fo.cu_az`) | Deputy commander | `osa_ak_ssc` designation from the search track under the azimuth line: `has_designation` |
| ПЕРЕДАТЧИК (`ssc.tx`) | Operator | `RadarState::on` for `osa_ak_ssc` (a Western RWR sees "lock" from here) |
| Capture and automatic tracking | Operator | Lock = single-target track kept while plots continue |
| НЛЦ | automatic | `low_target_elevation_rad` |
| ПОЛУАВТОМАТ (`ro.semi_auto`), TV tracking | Operator / deputy | Angles from an optical sensor (not an emitter; phase E5); range from the radar |
| ПОМЕХА (`fo.pomekha`) | Deputy commander | Fuse arming on launch against a jammer (engagement layer, not EM) |
| ШИР. / СРЕДН. (`svr.beam`) | Deputy commander | Missile-tracking beam (phase E5) |
| Working-frequency change | Crew chief / technicians | `EmitterState::freq_hz` from the channel list (data-ready; no control yet) |

## 4. Calibration results (from `simcore/tests/test_em_data.cpp`)

Fitted to Korovin's high-altitude figures with the default environment; low-altitude figures are
*predictions* (not fitted):

| Check | Model | Published | Verdict |
|---|---|---|---|
| SOC, fighter (3 m²) at 5 km altitude, Pd 0.5 held | 42.4 km (exact 0.5 at 40 km; beam II's shape gives the first sustained detection a little further out) | 40 km | fitted |
| **SOC, fighter at 50 m, Pd 0.5 held** | **26.2 km** | **27 km** | **prediction within 3 %** |
| SOC, Pd 0.8, 100–3000 m altitude | 27–33 km | 20–35 km "depending on altitude" (`mon_2112_80` p10) | consistent if the manual quotes a high Pd |
| SOC, Pd 0.8 at 5 km altitude | 15 km | — | beam III (19° tall, ~7 dB less gain) covers high, close targets; the coverage gap is real for a stacked-beam radar |
| SSC, fighter at 5 km, Pd 0.5 | 23.0 km | 23 km | fitted |
| SSC, fighter at 50 m, Pd 0.8 held | 14.2 km (an artefact: one deep multipath null at 17 km in the flat-ground check path; on the real path Pd is 0.5 at 22 km and 0.8 at 18 km) | 14 km | **does not match** (corrected in the E2 review) |
| SSC, fighter at 50 m, Pd 0.5 held | 24.6 km | 14 km | **open item**: over smooth ground the Ku-band first multipath lobe (< 0.1°) enhances a 50 m target; closing the gap needs a rougher surface, vegetation, clutter in the 1.5 km gate or another trials-site effect, not a different definition of capture |
| R-123M between two vehicles (3.5 m antennas), radars off | system loss fitted to 15 km: 0.7 dB | ≥ 15 km (`mon_2112_80` p12–13) | fitted; small loss = plausible |
| R-123M with the radars on | co-site desense fitted: 3.9 dB | ≥ 12 km | fitted (= 40 log 15/12, plane-earth law) |

## 5. Signature timeline (what an enemy RWR/ESM sees)

```
 search radar rotating (sweeps every 1.8 s; sidelobes continuous) ──────────────────────────────▶
 IFF interrogations (bursts)            ▲
 tracking radar on at ЦУ / ПЕРЕДАТЧИК ───────────── steady illumination ("lock") ───────────────▶
 command transmitter at ВНИМАНИЕ ──────────────────────────────── uplink while missiles fly ─────▶
 radios: designations by voice from the PU-12, reports up ─ ─ ─ ─ ─ (each transmission an emission)
 TV-only engagement: none of the above except the search radar for two turns, or nothing at all
```

The drills in the research file (search radar on for only two turns, a brief tracking-radar burst,
radar off as soon as the missile is far enough or detonates; ARM threat → stop emitting and move:
`samsim_thread` ePap, `video_osa`) map directly onto `EmconStep`s and doctrine.

## 6. Vulnerabilities and how they appear in the model

| Threat | How it shows up (emergent) | Osa-AK counter |
|---|---|---|
| ESM/ELINT location (ground DF, airborne ESM, TDOA systems like PLSS) | The search radar is heard far beyond its own range; three DF sites fix it; a moving aircraft ranges it over a baseline | EMCON, short emissions, moving after emitting, dummy loads, decoy emitters on the same parameters |
| Anti-radiation missiles (Shrike, Standard ARM, HARM) | (phase E6) home on `EmitterState` in air | Stop emitting and relocate (doctrine) |
| Stand-off noise jamming | Sidelobe J/N shrinks detection range in all directions, main-lobe J/N blinds the jammer's sector; strobes | Burn-through at short range; TV angles with radar range; method Н; frequency change against spot noise |
| Self-screening noise | Range denied until burn-through; strobe announces the aircraft | as above |
| Range-gate pull-off | Lock walks off and breaks | Re-acquisition; TV; (susceptibility unknown) |
| Angle deception | — | Monopulse: modelled as immune (`mon_2112_80` p17) |
| Chaff | False plots; target masked in the cell | External-coherence MTI cancels drifting chaff |
| Low fliers, terrain | Masking, multipath fades, clutter | Siting rules (`mon_2112_80` p66), НЛЦ, TV |
| Beaming targets | MTI notch hides zero-radial-speed targets | MTI off above the clutter |
| Comms jamming, C2 destruction | Designations late or lost; picture ages; autonomous search | Own search radar; voice backup; reserves (Czech doctrine) |
| Co-site interference | Radio range 15 → 12 km with radars on | Siting 200 m apart, frequency plan |

## 7. Validation events in EM terms

* **Lebanon 1982:** extensive Israeli EW failed to neutralise the Osa (`korovin_tiv_2010`); drones
  were used to draw missiles before strikes. A scenario should show noise jamming *degrading* the Osa
  (shorter ranges, strobes) without stopping it, and decoy drones consuming missiles.
* **Angola 1983–88:** dug-in vehicles, night moves with the search radar on, a helicopter low in a
  canyon out of radar view (`korovin_tiv_2010`, Pavlov memoir): terrain masking and EMCON on the move.
* **Training ranges:** detection and capture ranges against drones and P-15 targets; R-123M ranges
  between vehicles.

## 8. Open items

1. SSC low-altitude capture: Pd definition of Korovin's 14 km; terrain roughness at the trials site
   (Emba); clutter in the 1.5 km gate.
2. Actual frequencies, PRFs and pulse widths (not public); the AK's transmitter powers.
3. ССР, СПК and НРЗ RF parameters (needed to make them emitters).
4. MTI improvement factor and notch width; the coherence selector on the console.
5. Range-gate pull-off susceptibility of the SSC's range tracker.
6. The special circuit against asynchronous pulse interference (`mon_2112_80` p67) — model as reduced
   mutual interference between neighbouring vehicles once co-channel radar interference is modelled.
7. FM 100-2-3 and NATO-era threat handbooks for the blue library (what NATO believed in 1985–90).

## Command sources

The Osa's early warning comes from the PU-12 by default and may come from other sources; see `docs/research/EM_C2_SOURCES_2026-10.md`.
