# Radar, signals, EW and C2 for SAM3D: research report (October 2026)

Research done on 7 October 2026 for the electromagnetic (EM) environment layer: radar, passive
detection (ESM/SIGINT), direction finding and geolocation, electronic warfare, emissions control,
decoys, propagation over terrain, and the communications and command network. It asks two questions
for every topic: *how does it really work?* and *how much of that should a game simulate?* The
architecture that follows from it is `docs/EM_ARCHITECTURE.md`; the Osa-AK details are
`docs/research/OSA_EM_SPEC_2026-10.md`; the engine question is `docs/research/ENGINE_COMPARISON_2026-10.md`.

Rules followed (project `CLAUDE.md`): public and declassified sources only; operator-country
documents first; Western "facts and stats" pages only for what the West *believed* (which is exactly
what a NATO threat library should contain); every number in `data/` carries a source and confidence.
Source ids are those of `docs/SOURCES.md`; the bibliography is §14.

**Labels used for claims about specific systems:** **Confirmed** (a tier-1/2 source states it),
**Inferred** (follows from confirmed facts by textbook reasoning), **Approximation** (a modelling
choice standing in for something not public), **Unknown**.

---

## 1. Summary of findings

1. **Model information, not waves.** Every decision a radar crew, an ESM operator or a fighter pilot
   makes depends on a handful of quantities: signal-to-noise(-and-interference) ratio, the
   probability that an echo or intercept is seen, the accuracy of what is measured, and how old
   the information is. Those can be computed cheaply and *emergently* from geometry, power,
   antenna patterns and terrain. Waveforms, pulses and Maxwell's equations add nothing a player or
   an AI could perceive. This matches the principle set for the project and the approach of
   serious simulation frameworks (MIXR passes per-frame "emission packets", not pulses — §12).
2. **Calibrate to published performance.** Full transmitter, antenna and receiver figures are
   rarely public for Warsaw Pact systems, but detection ranges against a stated target are. A
   radar model whose loop gain is fitted to the published range — and whose *shape* (scaling with
   RCS, range, terrain, multipath, jamming) comes from textbook physics — reproduces what is known
   without inventing what is not. The Czechoslovak army taught exactly this scaling
   (`D = D_ref · (σ/σ_ref)^¼`, Vojenské rozhledy 1989/3). For the Osa-AK the fitted model then
   *predicts* the published low-altitude range of the search radar (26.2 km vs 27 km, not fitted).
3. **One propagation model for every band.** Free-space loss, knife-edge diffraction over the
   terrain profile (ITU-R P.526), gaseous absorption (ITU-R P.676) and ground reflection give the
   right behaviour at C/X/Ku band (a hill blinds the radar, low fliers fade in and out) and at VHF
   (radios work over the same hill) from the same code.
4. **One-way beats two-way.** A radar is heard by an ESM receiver much further than it can see
   (received power falls as R² instead of R⁴). In the MVP scenario the striker's radar is located
   by three ground DF sites at 80 km, 99 s before the SAM's radar first sees the aircraft. This
   asymmetry is the core of SEAD and EMCON, and it emerges for free once emissions are modelled.
5. **Jamming degrades, it does not switch off.** Noise jamming adds to receiver noise:
   `SINR = SNR / (1 + J/N + C/N)`. Detection range shrinks by `(1 + J/N)^-¼`, accuracy degrades as
   `1/√SINR`, the jammer shows as a strobe (bearing without range), and the radar burns through as
   S/J rises with closure. Deception (range-gate pull-off, chaff) works on the tracker and the MTI,
   and is defeated by real counter-countermeasures (the Osa-AK's external-coherence MTI against
   chaff, monopulse against angle deception, TV tracking against noise).
6. **Knowledge is separate from truth.** Each unit holds reports with an origin, a time, a
   covariance and an identity confidence; it fuses them with covariance intersection, so the same
   report reaching a node twice never makes it more certain. AI reads only that knowledge. Destroy
   the command post and dependent units' pictures age, grow uncertain and vanish; they go
   autonomous and must radiate themselves (exactly the behaviour Czechoslovak doctrine expected:
   "temporary gaps … will often lead to decentralised warning and autonomous combat by the active
   elements of the defence", Vojenské rozhledy 1985/6).
7. **Period limits matter.** Late-1980s Warsaw Pact tactical radios were single-channel FM with
   presets (R-123M: four), not frequency hopping; radars changed frequency between a fixed grid of
   working frequencies, not pulse to pulse; Western UHF air radios hopped (HAVE QUICK, 1980) and
   SINCGARS VHF reached service in 1988. Towed RF decoys and LPI radars belong to the next decade.
   TDOA emitter location, however, was a Czechoslovak speciality of the period (KRTP-81 Ramona,
   KRTP-86 Tamara) and fits the setting perfectly.

---

## 2. Radar fundamentals and what SAM3D does with them

### 2.1 The radar equation, and why SAM3D calibrates it

Single-pulse SNR of a monostatic radar (Skolnik, *Introduction to Radar Systems*, ch. 1–2):

```
          Pt · G² · λ² · σ
SNR = ─────────────────────────────── · F⁴
       (4π)³ · R⁴ · k·T0·B·Fn · L
```

`Pt` peak power, `G` antenna gain, `λ` wavelength, `σ` radar cross-section, `R` range, `k·T0`
thermal noise density (−204 dBW/Hz), `B` receiver bandwidth, `Fn` noise figure, `L` losses, `F`
the propagation factor (ground reflection, §2.6). Pulse integration over the dwell adds gain.

For Warsaw Pact radars the public record usually gives *some* of these (the Osa's search radar:
250 kW, ~10⁻¹³ W sensitivity, 1° × 4° beam — `korovin_tiv_2010`) and a *detection range against a
stated target*. The model therefore uses the **calibrated** form:

```
SNR = K · σ / R⁴ · G_pattern² · F⁴ / L_path²         K fitted so the published anchor gives its Pd
```

`K` absorbs everything not public (integration gain, losses, processing). The absolute figures are
still used where they matter on their own: what an ESM receiver hears (EIRP = Pt·G) and how a
jammer's noise compares with the receiver's own (`J/N`, §6). Estimates for missing internals
(pulse width, PRF, noise figure) are labelled and derived from published numbers where possible;
because of the calibration they change ESM and jamming behaviour, never the published ranges.

**Decision:** tier 1 (must be simulated). Implemented: `simcore/em/radar_system.hpp` (`Calibrate`,
`AnchorLook`, `RadarSnr`).

### 2.2 Detection is probabilistic

A radar does not "see" at a range; it has a probability of detection `Pd` for a given false-alarm
probability `Pfa`. For a target whose RCS fluctuates from scan to scan (Swerling I, the textbook
model for aircraft), one look gives `Pd = Pfa^(1/(1+SNR))` (Skolnik ch. 2; Richards, *Fundamentals
of Radar Signal Processing*, ch. 6). With `Pfa = 10⁻⁶`, Pd = 0.5 needs 12.8 dB and Pd = 0.9 needs
21.1 dB: an 8 dB difference, i.e. the "90 %" range is 62 % of the "50 %" range. This matters when
comparing sources: the Polish Osa-AK manual's 20–35 km for a MiG-21 (`mon_2112_80` p10) and
Korovin's 40 km for a fighter at 5 km are consistent if the former is a high-Pd figure — the
calibrated model gives Pd 0.8 at 27–33 km for altitudes 100–3000 m.

**Decision:** tier 1. One draw per look, keyed by (radar, target, scan) so the result never
depends on evaluation order (§13).

### 2.3 Beams, scan and revisit

A rotating search radar sees a target only when its beam sweeps it. The Osa's search antenna turns
at 33 rpm (1.82 s) with a 1° azimuth beam, and switches *one of three elevation beams per turn*
(4°, 4°, 19° high; `korovin_tiv_2010`, `mon_2112_80` p15). A target in beam II is therefore looked
at once every three turns (5.5 s) in the I–III programme, and the taller beam III has ~6.8 dB less
gain. These are real operational constraints (which beam programme to run against low fliers) and
cost nothing to model: each tick the radar evaluates only targets inside the arc swept that tick
and inside the active beam.

**Decision:** tier 1. Antenna patterns are Gaussian main lobes (−12(θ/θ₃)² dB per axis, Mahafza)
floored at a sidelobe level; sidelobes matter for ESM and jamming.

### 2.4 Radar cross-section and aspect

RCS varies by tens of dB with aspect; a nose-on fighter is a few m², a beam-on one several times
more. The Czechoslovak 1989 article notes absorbers could cut the TR-1's RCS to 0.25 m² and that it
"fluctuated within 20 dB", causing intermittent tracking (Vojenské rozhledy 1989/3). SAM3D uses
nose/beam/tail values per platform interpolated in dB plus the Swerling fluctuation. Full RCS maps
by azimuth and elevation are tier 3 (data-limited and invisible to players beyond "head-on is
harder to see").

### 2.5 Horizon, terrain masking and diffraction

The radio horizon over a 4/3-Earth is `R ≈ 4.12 (√h₁ + √h₂)` km with heights in metres — the
formula the Czechoslovak article uses for passive intercept range (`R = 4.12·√H`). A radar antenna
5 m up sees a 50 m target to ~38 km at best; terrain cuts that further. SAM3D samples the terrain
profile along each path (on the shared, frequency-independent `PathGeometry`) and applies single
dominant knife-edge diffraction, ITU-R P.526 eq. (31):

```
J(v) = 6.9 + 20 log10( √((v−0.1)² + 1) + v − 0.1 )  dB,    v = h · √( 2/λ · (1/d₁ + 1/d₂) )
```

Behind a 300 m ridge 10 km away this is ~45 dB at 6 GHz (blind) but ~15 dB at 50 MHz (radios
still work) — the same formula gives both. Multiple-edge methods (Deygout, ITU-R P.526 §4.5) are a
later refinement. The Osa manual's siting rules show how much the real crews cared: a flat position
of at least 10 m radius, at least 500 m from large buildings, forests and power lines, and no hills,
forests or buildings higher than 10 m within 1 km in the working sector (`mon_2112_80` p66) — i.e.
a masking angle under ~0.6°.

**Decision:** tier 1 (the user's example: a SAM behind a hill must not be seen).

### 2.6 Ground reflection (multipath)

Near the ground the direct echo interferes with its reflection, multiplying power by `F⁴`
(two-way). Over a spherical Earth with effective heights (Blake, *Radar Range-Performance Analysis*;
Skolnik, *Radar Handbook* ch. 2):

```
F² = 1 + ρ² − 2ρ·cos(2π·Δ/λ),    Δ = 2·h₁'·h₂'/d,     ρ = ρ₀ · exp(−8 (π σh sin ψ / λ)²)
```

Low targets fade in and out across lobes, and near the horizon the rays cancel — which is why
low-altitude detection ranges are far shorter than the geometric horizon. Rough ground (σh) kills
the reflection at microwave but not at VHF. With an antenna-pattern weighting (a beam pointed up
suppresses the reflection) this model, *with nothing fitted to it*, gives the Osa-AK search radar
26.2 km against a fighter at 50 m; Korovin's published figure is 27 km. The tracking radar is
discussed in the Osa spec (§5 there): the Ku-band first lobe lies below 0.1°, so over smooth ground
a 50 m target is *enhanced*; the published 14 km is **not** reproduced (the model locks a 50 m target
at about 22 km). An earlier reading that it matched the Pd 0.8 range was an artefact of one deep multipath
null in the calibration-check path (found in the E2 review, October 2026); it is an open item. The Osa-AK's own low-target mode (НЛЦ/NLC: the tracking and missile-tracking
antennas are held at a fixed elevation "to eliminate distortion of the antenna pattern by the
ground", `mon_2112_80` p17; engaged at 0-06…0-10 mils, `flak11`) is modelled as a minimum pointing
elevation.

Below 1 GHz (radios near the ground), the plane-earth law `F = 4π h₁ h₂ / (λ d)` (power ∝ 1/d⁴;
Parsons) replaces the two-ray formula; with it the R-123M's published 15 km between two vehicles
(`mon_2112_80` p12–13) is met with a fitted system loss of only 0.7 dB.

**Decision:** tier 2 (useful realism, cheap). Ducting and anomalous propagation: tier 3/4.

### 2.7 Atmosphere

Gaseous absorption is ~0.01 dB/km below 10 GHz and peaks near 22 GHz (ITU-R P.676). It hardly
matters for the Osa's bands but costs nothing; rain attenuation at Ku band (ITU-R P.838) is a later
weather feature. Refraction is the 4/3-Earth factor (variable `k` per scenario).

**Decision:** gases tier 2 (one table lookup), rain tier 3, ducting tier 4.

### 2.8 Clutter, Doppler and MTI

Ground clutter competes with low targets in the same resolution cell; coherent radars cancel echoes
that do not move (moving-target indication). The Osa-AK's search radar had a "fixed-echo
suppression" mode (TES/СДЦ) with a double inter-period canceller shared with the tracking radar's
range channel, and **two coherence modes**: *internal* (phase reference from the transmitter pulse)
and *external* (phase re-referenced by the interference itself), the latter cancelling wind-blown
chaff (`mon_2112_80` p15, p23; `korovin_tiv_2010` dates it to the Osa-A, ~1975). MTI has a
price: a target with no radial velocity (flying a circle around the radar, "beaming") is cancelled
too. SAM3D models clutter-to-noise in the cell (beam-limited, ∝ R⁻³, only inside the radar's own
ground horizon), the MTI improvement factor, the notch around zero radial speed, and chaff with its
own drift velocity. Both effects are tested: a beaming target vanishes with MTI on; external
coherence removes chaff that internal coherence lets through.

**Decision:** tier 1 for MTI notch and chaff (they drive real tactics); clutter maps from land use
are tier 3.

### 2.9 Measurement accuracy

Thermal accuracy (Barton, *Radar System Analysis and Modeling*, ch. 18):
`σ_R = c·τ / (2√(2·SNR))`, `σ_θ = θ₃ / (k_m √(2·SNR))`, each with a floor for bias and servo noise.
A search radar knows elevation only to its beam (beam height/√12). Jamming lowers SINR and
therefore accuracy — one of the ways it degrades tracking without "turning the radar off".

**Decision:** tier 1.

### 2.10 Track formation, maintenance and loss

Real track-while-scan: plots are associated with tracks inside a statistical gate; a new plot opens
a *tentative* track; M detections out of N looks confirm it; a looked-at track without a plot coasts
and its uncertainty grows; enough misses (or too much uncertainty) drop it (Blackman & Popoli,
*Design and Analysis of Modern Tracking Systems*; Bar-Shalom, Li & Kirubarajan). SAM3D: a
nearly-constant-velocity Kalman filter on converted measurements, global-nearest-neighbour
association in a χ² gate, 2-of-3 confirmation, drop after K misses; misses count only when the
radar actually looked where the track should be. A single-target tracking radar (the Osa's ССЦ)
locks, holds a range gate (the Osa's TES window is 1.5 km, `mon_2112_80` p23) and loses lock after
consecutive misses. JPDA/MHT trackers are tier 3: the air pictures here are sparse.

### 2.11 Search vs track, illumination, IFF

Search radars rotate; tracking radars dwell on one target and illuminate it continuously — which a
target's RWR recognises as "lock". The Osa-AK's IFF interrogator (НРЗ) shares the search antenna
(`mon_2112_80` p14, p16) and friendly replies appear on the scope. Kremniy-2 IFF dates from the late
1950s; its successor Parol entered Soviet service in 1977 and reached East German forces in the late
1980s (mil-airfields.de; tier 4). SAM3D models the reply as a physical process (a working
transponder of the interrogator's side, in the beam, replies with a probability) — never as "the
game knows who is friendly". Modes and codes are tier 2 (later).

---

## 3. ESM / SIGINT: hearing emitters

### 3.1 Detection

An ESM receiver hears an emitter when the one-way received power beats its sensitivity:
`Pr = EIRP(θ) · G_rx · λ² / (4πR)² · path factors`. Typical sensitivities (Adamy, *EW 101*;
Wiley, *ELINT*): radar warning receivers (crystal video) −40 to −65 dBm; tactical ESM −65 to −80
dBm; ELINT −80 to −100 dBm. Because the radar's EIRP is enormous (the Osa's search radar: 250 kW ×
~38 dB ≈ 1.6 GW peak in the main beam), its *sidelobes* are audible to sensitive receivers at long
range, and its main beam to almost anyone in line of sight. The Czechoslovak article says the
PLSS's airborne receivers would detect "every newly switched-on source practically immediately …
within a few seconds, regardless of the quality of camouflage" (Vojenské rozhledy 1985/6).

**Scan-on-scan:** a receiver with a narrow-band or scanning front end hears a rotating radar's main
beam only when both point at each other; a wide-open RWR hears every main-beam pass (once per
antenna turn) and, if sensitive enough, the sidelobes continuously. SAM3D evaluates the main beam
analytically (did the beam sweep the receiver's bearing during the receiver's listening window?),
so receivers can run slower than the radar without missing a paint.

### 3.2 What is measured

Bearing (angle of arrival), carrier frequency, pulse repetition frequency/interval, pulse width,
scan period (from the timing of main-beam paints), and illumination type (sweeping vs steady). These
are what a 1980s ESM system measured and compared with its library. SAM3D draws each measurement
with the receiver's accuracy (`freq_sigma_fraction`, `pri_sigma_fraction`, `pw_sigma_fraction`,
`bearing_sigma`), worsening near the sensitivity threshold.

### 3.3 Association, identification, confidence

Intercepts of one emitter are grouped into a *contact* by parametric association (frequency and PRF
within statistical gates, bearing within a gate). This is harder than it looks: during this work an
RWR whose PRF gate was tighter than its own measurement noise split one emitter into several
contacts, and nearest-neighbour association then gave each contact a biased slice of the bearing
distribution — a 10° bias from unbiased measurements. The fix (gates sized from the receiver's own
accuracy, merging of drifting contacts) is the standard one and is now tested.

Identification compares the measured parameters with a **threat library** written by the
receiver's side: a likelihood per entry, normalised against an "unknown emitter" prior so that a
signal fitting nothing stays unknown. Libraries say whether the emitter type is continuous wave or
noise (so a pulsed radar is never called a jammer) and whether it is airborne (so old bearings of a
fast emitter count for less). **The library is one side's belief, not the truth**: the NATO view of
the SA-8 (Land Roll search radar "H band 6–8 GHz", tracking radar "J band 14.5 GHz", reported as
"frequency agile") comes from Western sources and goes into a blue library; the Osa's actual
working frequencies are not public, and the Polish manual describes fixed working frequencies chosen
from a grid (§7). Mismatches between belief and truth — an unknown emitter, a misidentification, an
emitter type re-used by a decoy — are gameplay, not bugs.

**Unknown, intermittent and LPI emitters.** Unknown emitters appear as "unknown" with their measured
parameters. Intermittent emitters (short EMCON bursts) produce contacts that time out; their
bearings still accumulate. LPI radars (low peak power, wide spread spectrum) are not of the period
and are left out.

### 3.4 Multi-sensor correlation

At a command node, bearing-only reports of the same signature from different receivers (or one
moving receiver over time) are triangulated (§4). A fix that falls inside a radar track's ellipse
tags that track with the emitter's identity ("track 7 is carrying a fighter fire-control radar"),
which is how ESM supports classification of radar tracks.

---

## 4. Direction finding, triangulation and geolocation

### 4.1 Bearing accuracy by DF method

| Method (period-appropriate) | Typical 1σ bearing error | Notes |
|---|---|---|
| RWR, four-quadrant amplitude comparison | 10–15° | Enough to point the aircraft, not to locate |
| Shipborne/airborne ESM with multi-antenna amplitude or phase comparison | 2–5° | |
| Ground DF station, interferometer or rotating antenna | 1–2° | |
| Large-baseline ELINT array | < 1° | |

(Adamy, *EW 101* / *EW 102*; Poisel, *Electronic Warfare Target Location Methods*.) Accuracy also
degrades near the sensitivity limit and with multipath at low elevation.

### 4.2 Triangulation

Two or more lines of bearing cross at the emitter. The classical estimator (Stansfield, *Statistical
theory of D.F. fixing*, J. IEE 1947) weights each line by `1/(σ·r)²` and solves a least-squares
problem; it is cheap but **biased towards the receivers** when bearings are poor and the baseline
short. SAM3D refines it by maximum-likelihood Gauss–Newton on the angle residuals (consistent
covariance) and rejects solutions behind the receivers. Error areas are ellipses from the 2×2
covariance (semi-axes = √eigenvalues, scaled by `√(−2 ln(1−P))`: 1.18 for 50 %, 2.45 for 95 %);
CEP ≈ 0.59(σ₁+σ₂). Monte Carlo tests check that the reported ellipses are honest (normalised
squared error averaging 2 for 2 dof) and that more stations give smaller errors.

**Synthetic baseline:** one moving receiver measuring a stationary emitter over time triangulates by
itself. In the tests an aircraft flying 60 km across the line of sight to a SAM 70 km away ranges it
to 0.3 km (CEP 2 km) with a 2° ESM pod, but only to ~2.5–18 km (CEP 8–13 km) with a 10° RWR —
which is why RWRs were not targeting systems.

**Moving emitters:** old bearings of an emitter that may have moved are de-weighted
(`σ² + (v_max·age/r)²`), with `v_max` from the threat library (ground vs air).

### 4.3 Time difference of arrival (TDOA)

Comparing the arrival time of the *same pulse* at three or four sites gives hyperbolae whose
intersection is the emitter (Foy 1976; Torrieri 1984). This is exactly how the Czechoslovak
**KRTP-81 Ramona** (fielded 1979; central site plus two or three side sites on 25 m masts;
0.8–18 GHz; ~20 targets semi-automatically) and **KRTP-86 Tamara** (accepted 1987; 23 radar and 48
IFF targets) worked; a Ramona was deployed to Djebel Baruk in Lebanon and attacked in 1982
(Wikipedia articles, tier 4). A Czech army article of 1989 credits the "Ramona type passive location
system" with catching emitters at 500–600 km (Vojenské rozhledy 1989/3). On the NATO side the
**PLSS** (Precision Location Strike System), TR-1 aircraft locating emitters by TDOA for strike
direction, was the subject of a long Czechoslovak analysis in 1985 that assumed ~10 m accuracy and
near-instant detection of any new transmitter (Vojenské rozhledy 1985/6); it never reached service.

SAM3D's TDOA groups multilaterate intercepts of the same pulse heard by enough sites in one tick
(Gauss–Newton with the correlated-noise weighting of a common reference site). In the test, four
sites with 30 ns timing locate a radar-carrying aircraft at 114 km to ~120 m horizontally — but its
height only to ~0.8–2 km, because near-coplanar ground sites have poor vertical geometry, as they
should.

**FDOA** (Doppler differences between moving receivers) is not of this period's tactical systems;
tier 4.

### 4.4 What changes accuracy

Sensor accuracy and signal strength (built in), geometry/crossing angle (built in through the
covariance), distance (bearing error × range), terrain (no line of sight → no intercept; low-angle
multipath errors are tier 3), synchronisation (TDOA timing σ), emitter and receiver motion (age
inflation; moving receivers record their position per line).

---

## 5. Electronic warfare

### 5.1 Noise jamming and J/S

| Technique | What it is | Effect in SAM3D |
|---|---|---|
| Barrage | Noise over a wide band | Power density = P/B_j; only the part inside the radar's bandwidth counts (B_r/B_j) |
| Spot | Noise concentrated on one radar's channel | Full power in band; useless if the radar changes channel |
| Sweep | Spot noise swept across a band | Time-averaged barrage; tier 2 (not yet) |

`J/N` at the radar uses the radar's receive gain *in the direction of the jammer* while it looks at
the target: main-lobe jamming blinds the sector containing the jammer; sidelobe jamming (a stand-off
jammer off-axis) raises the noise floor everywhere by less. The resulting detection envelope is a
lobed shape around the radar, drawn by the `jamming` overlay. The jammer itself appears as a
**strobe** (bearing, no range) whenever the main beam sweeps it with `J/N` above a threshold, which
is both a detection (the jammer is announced) and a measurement (two strobes from two radars
triangulate it).

**Burn-through:** for a self-screening jammer, S ∝ 1/R⁴ and J ∝ 1/R², so S/J ∝ 1/R² and the radar
regains the target inside the burn-through range `R_bt = √(K / (req · J₀))` (closed form in
`SelfScreenBurnThroughRangeM`). Against a powerful SAM radar a small self-screening noise pod buys
little range but denies range information until close in — and its strobe gives it away at once (in
the test: strobe from 70 km, track at 19 km instead of 53 km).

### 5.2 Deception

| Technique | Against | SAM3D |
|---|---|---|
| Range-gate pull-off (RGPO) | Range-gate trackers | A repeater on the target captures the gate when its repeated pulse beats the skin echo by a margin, walks it off, then falls silent: the gate sits on empty space and lock is lost. Tested. |
| Velocity-gate pull-off | Doppler trackers | Tier 2 (the Osa's radars are pulse with MTI, not pulse-Doppler trackers) |
| Angle deception (inverse gain, cross-eye) | Conical scan, lobing | The Osa-AK's tracking radar is two-channel monopulse, "protecting it against responsive angle jamming" (`mon_2112_80` p17): modelled as immune; tier 2 for older radars |
| False targets | Search radars | Tier 2 (repeater writes extra plots) |
| Chaff | MTI and trackers | Cloud entity with blooming/decaying RCS, drifting with the wind; MTI internal coherence cancels it only if slow, external coherence cancels drifting chaff (Osa-AK). Tested. |
| Saturation | Operators and trackers | Emerges from many targets/decoys and a voice net with limited capacity |

### 5.3 What jamming degrades, and how SAM3D shows it

| Function | Mechanism | Emergent result |
|---|---|---|
| Detection range | SINR | Range × (1+J/N)^−¼ in the jammed sector |
| Track quality | σ_θ, σ_R ∝ 1/√SINR; missed looks | Bigger ellipses, dropped tracks |
| Track stability | Pd per look | Coasting, re-acquisition |
| Position accuracy | as above | Larger CEP in the picture |
| Identification | IFF replies still work; ESM IDs the jammer | Strobes tagged "noise jammer" |
| Engagement | Lock needs consecutive detections; RGPO breaks lock | Delayed or broken engagements; fallback to angles-only (TV, "method Н") |
| Communications | Barrage noise in the VHF band raises the receiver's noise | Link margins shrink; messages lost or delayed |

### 5.4 Counter-countermeasures modelled or planned

MTI with internal/external coherence (Osa-AK), monopulse (immune to simple angle deception),
working-frequency changes between engagements (tier 2: a jammer on the old channel then misses),
angle tracking on the jammer (home-on-jam; the Osa's "method Н" uses the jammer's angles for
three-point guidance), optical tracking under jamming ("in strong active jamming, angles from the TV
sight and range from the search radar", `korovin_tiv_2010`; "in strong radio-electronic jamming and
good visibility the target can be tracked by day with the TV sight", `mon_2112_80` p12), the special
circuit against asynchronous pulse interference (`mon_2112_80` p67; tier 3).

### 5.5 Historical notes for calibration

`korovin_tiv_2010` reports that against the Syrian Osas in 1982 "practically all EW means known at
the time" were tried and that the state trials' conclusion — the Osa's jamming resistance exceeded
every army system of its generation — was confirmed, forcing the Israelis to tactics: drones to
make the crews expend missiles, then strikes. Western and Israeli accounts of Operation Mole
Cricket 19 (9 June 1982) describe decoy drones (Samson/Delilah) to provoke radar emissions, RPVs
(Scout, Mastiff) and stand-off EW aircraft to record and jam, then anti-radiation missiles and
bombs (secondary accounts, tier 4/5). Both views fit a model where noise jamming degrades but does
not neutralise a well-protected radar, and emission control is the defence that matters.

---

## 6. Frequency agility, hopping, EMCON — what fits 1985–1992

| Technique | Period status | SAM3D |
|---|---|---|
| Radar working-frequency grid, changed between engagements | **Confirmed** for the Osa-AK: transmitter frequencies of the SAM's radars, command transmitter and radios chosen from fixed frequencies "for the greatest jamming resistance and radio-electronic masking"; neighbouring vehicles' command-transmitter frequencies interleaved (1st vehicle 3 & 1, 2nd 4 & 2, …); missile-tracking receivers have two frequencies, A only "by special permission" (a war-reserve frequency) (`mon_2112_80` p67) | `AgilityKind::Channels`; channel changes are a crew action (later) |
| Pulse-to-pulse frequency agility | Existed in some 1980s Western radars; Western sources call the SA-8 "frequency agile", which the Polish manual does not support | Supported as data (`PulseToPulse`), not used for the Osa |
| Tactical VHF radios | Warsaw Pact: single-channel FM with presets (R-123M: 20–51.5 MHz, 4 presets, 20 W — tier 4) | Fixed channel per net |
| Frequency hopping, air UHF | HAVE QUICK, introduced 1980 (225–400 MHz) | Data only (blue aircraft) |
| Frequency hopping, ground VHF | SINCGARS RT-1439 first fielded 1988 (30–88 MHz) | Data only; a jammer then needs far more power |
| Soviet hopping radios | No evidence found for fielded 1980s tactical VHF hopping; later families (Akveduk) are post-1990 | **Unknown**/unlikely for the setting |
| Encryption / COMSEC | Voice nets with code tables and coded maps were normal; secure-voice equipment existed at higher echelons | Messages carry no content beyond the track data; interception value of plain voice is tier 2 |
| EMCON | **Confirmed** practice: transmitters checked into dummy loads to deny reconnaissance (`mon_2112_80` p67); selected transmitters banned in peacetime; moves at night; switch on only briefly; simultaneous "active dummies" on the same parameters to confuse location (Vojenské rozhledy 1985/6) | Radars/jammers on/off by doctrine or schedule; `dummy_load` flag; decoy emitters |
| Short transmissions, burst comms | Voice reports are long (seconds); telecode at command-post level is short | Message durations by radio type; each transmission is an emission ESM can hear |
| LPI radar | Not of the period | Left out |

---

## 7. Decoys and deception

| Mechanism | Period-appropriate? | Source | SAM3D |
|---|---|---|---|
| Chaff (corridors, self-protection) | Yes | Universal | Chaff clouds (§5.2) |
| Expendable decoy drones (Samson, ADM-141 TALD) | Israel 1982; TALD in US Navy service 1987 | Mole Cricket accounts; ADM-141 TALD | A decoy is a platform with an RCS model (and optionally an emitter); no new code |
| RPVs as targets and spotters | Yes (Scout, Mastiff 1982; SAAF Seeker in Angola) | `korovin_tiv_2010`, research §9 | Platforms |
| Towed RF decoys (ALE-50) | No: 1990s | — | Leave out |
| Ground radar decoys, active dummies | Yes: Czechoslovak doctrine recommends switching on "one or more active dummies working on the same parameters (frequency, power, mode)" with the real transmitter to enlarge the location uncertainty, and roaming decoy transmitters | Vojenské rozhledy 1985/6 | Decoy emitters (`generic_decoy_emitter_x`) on ground platforms; ESM association then produces ghost or wrong fixes — emergent |
| Corner reflectors, dummy positions | Yes (prepared Osa positions had dummy positions and nets: research §7) | research §7 | Platforms with RCS |
| False radar returns (repeater false targets) | Yes | Textbook | Tier 2 |
| Communications deception (dummy traffic, imitative) | Yes | Vojenské rozhledy 1985/6 ("complex electromagnetic situation") | Dummy nets transmitting = emissions (no code needed) |

---

## 8. Communications and command

### 8.1 What the Osa battery actually used

- Each 9A33BM2 carries **two R-123M** VHF radios for external links and a TA-57 field telephone at a
  halt (`mon_2112_80` p12, p26). Radio range at least **12 km with the vehicle's radar transmitters
  on, 15 km with them off** (p12–13) — a co-site interference effect that SAM3D reproduces with a
  fitted 3.9 dB desense (plane-earth law: 40·log10(15/12)).
- Designations came from the battery's PU-12(M) by **voice: azimuth and range** to each vehicle;
  automated telecode only down to the battery command post (`flak11`). The PU-12 board was updated
  about once a minute (`samsim_osa`).
- Siting: vehicles at least 200 m apart, similar SAM positions 500 m apart, "to reduce mutual
  interference" (`mon_2112_80` p66–67).
- Czechoslovak doctrine (1985): radar gaps after strikes "will often lead to decentralisation of
  warning and to autonomous combat by the active elements"; build radio, radio-technical and radar
  **reserves**; combine stationary and mobile transmitters "with easy change of position, including
  airborne relays".

### 8.2 Model

Nets (shared half-duplex channels), radios (power, sensitivity, antenna height, bandwidth, required
SNR, co-site desense; fitted to published ranges like radars), links evaluated with the shared
propagation (plane earth + terrain), delivery probability from link margin, voice operator delay and
message duration, relays, store-and-forward, report and picture schedules. A busy voice net queues
traffic — in the MVP run the battery net saturates at times and subordinates briefly flip to
"degraded", a real effect of voice C2. Jamming raises the receiver noise; destroying a node cuts its
links; subordinates go degraded then autonomous on a timer and their network knowledge ages out.

**Tier 1:** link up/down by propagation and jamming, latency, loss, node destruction, autonomy.
**Tier 2:** message priorities, encryption/intercept value, relay aircraft, telecode formats.
**Tier 4:** packet-level protocols, modulation.

---

## 9. AI and operator behaviour

AI must work from the same knowledge a crew would have. The implemented doctrine (`SamDoctrine`)
decides radar on/off from the node's fused picture: dark while the command post covers it and
nothing is in the cue zone; radiate on a network cue or when command is lost; dark again after a
quiet period; designate the tracking radar on the nearest threat. Every decision is logged with its
reason (e.g. "cue: track 4 at 21 km (network)"). The operator workflow of the Osa (detect → IFF →
halt → tracking radar on → lock → launch; ARM threat → stop emitting and move, `samsim_thread`
ePap) maps onto the same primitives. Richer AI (StateTree/behaviour trees in Unreal) sits on top
of this layer and reads only knowledge.

---

## 10. The Osa-AK

Summarised in `docs/research/OSA_EM_SPEC_2026-10.md`, with every parameter labelled
Confirmed / Inferred / Approximation / Unknown, the calibration results, and the open items.

---

## 11. Environment effects: what matters

| Effect | Matters for gameplay? | Cost | Tier |
|---|---|---|---|
| Line of sight and terrain masking | Decisive (low-level penetration, SAM siting) | Profile per pair, cached | 1 |
| Knife-edge diffraction | Yes: smooth fade behind ridges; radios over hills | Free given the profile | 1 |
| Earth curvature / refraction | Yes (horizon) | Free | 1 |
| Multipath lobing | Yes for low fliers and low-angle tracking | Cheap closed form | 2 |
| Ground clutter + MTI notch | Yes (beaming, low fliers without MTI) | Cheap | 1/2 |
| Gaseous absorption | Marginal | Table | 2 |
| Rain attenuation | Marginal at C/X, some at Ku | Cheap | 3 |
| Buildings / urban multipath | Local, data-heavy | Expensive | 3 (obstacle heights could be added to the profile) |
| Forest canopy | Some (low antennas, VHF) | Add canopy height to profile | 3 |
| Sea clutter and sea-surface multipath | Only over water bodies (lakes, the Adriatic) | Cheap | 3 |
| Ducting / anomalous propagation | Rare, confusing | Moderate | 4 |
| Fresnel-zone clearance | Covered by J(v) for v > −0.78 | Free | 1 (implicit) |

---

## 12. Lessons from existing games and simulations

| Title | What it simulates | What it abstracts | Lessons for SAM3D |
|---|---|---|---|
| **DCS World** (Eagle Dynamics) | Radar detection with terrain masking; RWRs with symbology; per-aircraft ECM | Jamming as a flag: noise jamming hides range but announces presence; burn-through ranges set per aircraft; chaff treated much like a flare for radar missiles (player analyses, e.g. flyandwire.com 2022; DCS forums) | Players notice when EW is a switch. Strobes and range denial are right and should emerge from J/N, not be scripted |
| **Skynet IADS** (DCS mission script) | Early-warning radars cueing SAM sites, command centres, power sources, connection nodes, autonomy when C2 dies, SAMs going dark under HARM attack, probability-based jamming with burn-through | Uses DCS's own detection; rules are scripted | The *C2 layer* is what makes an IADS feel alive: destroy the command centre and sites go autonomous. SAM3D does this with real links and knowledge, not flags |
| **Falcon BMS** | Detailed avionics; RWR shows only illuminating emitters and can misidentify; ECM pods with threat programmes and power split between them | Underlying propagation is simplified | Misidentification and "power split across programmes" are good, cheap realism; library-based ID with confidence gives it naturally |
| **Command: Modern Operations** (Warfare Sims) | Radar with RCS by aspect and horizon; ESM with bearing lines and *areas of uncertainty* that shrink as more detections arrive; EMCON per unit; contact classification in stages; datalinks share contacts between units | Sensor ranges come from database values with generation-based ECM/ECCM factors; no physical propagation beyond horizon/terrain | The closest model to this project's information layer. Its uncertainty areas and staged classification are proven gameplay; SAM3D adds physical propagation, real triangulation and C2 latency |
| **WARNO / Steel Division** (Eugen) | AA radars must radiate to be targeted by SEAD missiles; players toggle radars | Everything else abstracted | Even a binary EMCON choice creates good play; the full model should keep EMCON as a first-class decision |
| **Sea Power** (Triassic Games, 2025) | Physically modelled radar, ESM, sonar; ESM needs line of sight; EMCON | Unity-based; details not public | Shows a solo/small team can ship physically grounded sensors in a commercial engine |
| **Steel Beasts Pro** | Gunnery and crew procedures to training standard | Little EM modelling | Procedure fidelity is the draw — consistent with SAM3D's console focus |
| **Arma 3 + ACRE2 / TFAR** (mods) | Radio range and quality affected by terrain between players | Simple propagation | Terrain-aware radio links are affordable per pair and change behaviour (players climb hills to talk) |
| **F-19 Stealth Fighter** (MicroProse, 1988) | An "EMV" scale: the aircraft's electromagnetic visibility vs threat radar coverage | Highly abstract | A single, readable indicator of "how visible am I" is valuable UI over a deep model |
| **SAM Simulator** (Hpasp) | The Osa's consoles; jamming with burn-through; RGPO spirals on the scope; ARMs; TV-only EMCON engagement | One unit, simplified world | The behaviours to reproduce at console level; now driven by this layer's physics |
| **MIXR / OpenEaagles** (open source, C++) | `RfEmission` packets per frame (frequency, bandwidth, PW, PRF, pulses, ERP, gain, polarisation, ECM flag) passed from antennas to players; `Tdb` broad-phase target lists; radar, RWR, jammer, track manager, datalink classes | Not a game; no terrain propagation by default | Confirms the abstraction level: per-tick emission packets, broad-phase filtering, common emission object for radar/RWR/jammer |
| **AFSIM** (US Air Force) | Mission-level sensors, ESM, jammers and EW effects, communications | Government-only; little public detail | Same layering (platform, sensor, processor, comm) as SAM3D |
| **Stone Soup** (UK Dstl, open source, Python) | Tracking and data-fusion framework: filters, associators, initiators, deleters, metrics | Not a simulator | Validated component boundaries for SAM3D's tracker (initiate → associate → update → delete) and a place to compare algorithms offline |

**Cross-cutting lessons.** (1) Make emissions the shared currency: radar, RWR, ESM, jammer and
radio all consume the same emission data. (2) Represent what a sensor *reports*, with uncertainty
areas, not the truth. (3) C2 and EMCON produce more gameplay than extra RF detail. (4) Never make
jamming a switch. (5) Debug views of beams, strobes and ellipses are needed to trust and tune the
model (every team above that is credible has them). (6) Keep the database the source of
performance numbers, with sources.

---

## 13. Realism vs gameplay: the tiers

### Tier 1 — Essential (simulated accurately)

| Item | Why |
|---|---|
| Line of sight, horizon, terrain masking, diffraction | Decides who sees whom; the core of low-level tactics and SAM siting |
| Calibrated radar equation with Pd/Pfa and per-look draws | Detection ranges match sources; uncertainty is real |
| Beam, scan, elevation-beam programme, revisit | Real operator choices and timing |
| One-way ESM detection, main beam vs sidelobes | Makes EMCON and SEAD meaningful |
| Measurement accuracy vs SINR; track formation, coast, drop | Uncertainty as a mechanic |
| Noise jamming via J/N; strobes; burn-through | Degradation, not switches |
| MTI notch and chaff vs coherence mode | Real tactics (beaming, chaff corridors) and a real Osa ECCM feature |
| Triangulation and TDOA with honest error ellipses | "Something is emitting in this area" |
| Truth/knowledge separation, fusion without double counting, ageing | The information game itself |
| C2 links with latency, loss, jamming, destruction | Downstream consequences of attacking C2 |

### Tier 2 — Useful (simulated, cheaply; refine if needed)

Multipath lobing and low-angle effects; RCS by aspect; gaseous absorption; co-site interference;
IFF reply physics; RGPO; repeater false targets; frequency channels and spot-jammer mismatch;
library-based emitter identification with confidence; voice-net capacity; relays.

### Tier 3 — Cosmetic or data-limited (approximate or visualise)

Rain; forest and urban obstruction heights; clutter maps from land use; full RCS maps; VGPO; angle
deception against old radars; scope artefacts (noise grass, interference spokes) drawn from the
SINR the model already computes.

### Tier 4 — Unnecessary (not simulated)

Individual pulses and waveforms; Maxwell's equations or ray tracing; polarisation beyond a constant
loss; ducting; FDOA; packet-level communications protocols; modulation schemes; LPI. Each would cost
CPU and data and change nothing a player or AI could perceive or decide on in this period.

---

## 14. Bibliography

Operator-country and period documents (see `docs/SOURCES.md` for tiers):

- *Przeciwlotniczy zestaw rakietowy 9K33M2 OSA-AK. Wiadomości ogólne. Użytkowanie zestawu*, MON
  Uzbr. 2112/80, Warszawa 1981 (JAWNE) — `mon_2112_80`, local copy in `reference/sources/`.
- V. Korovin, «Зенитный ракетный комплекс «Оса» — история создания», *Техника и вооружение* 2010
  ([part 2](http://otvaga2004.ru/tiv/zenitnyj-raketnyj-kompleks-osa-istoriya-sozdaniya-chast-2/)) —
  `korovin_tiv_2010`.
- V. Lhotský et al., «Možnosti ničení systému PLSS a ochrana vlastních vojsk», *Vojenské rozhledy*
  1985/6, pp. 43–49 ([PDF](https://www.vojenskerozhledy.cz/pdfs/vor/1985/06/08.pdf)) — `vr_1985_plss`.
- M. Janošek, «Možnosti příletu stíhacích letounů do prostoru činnosti letounu TR-1», *Vojenské
  rozhledy* 1989/3, pp. 48–53 ([PDF](https://vojenskerozhledy.cz/pdfs/vor/1989/03/10.pdf)) — `vr_1989_tr1`.
- R. Wagner, flak11.de (NVA Fla-Raketen-Regiment 11) — `flak11`.
- SAM Simulator Osa-AK documentation and SimHQ thread — `samsim_osa`, `samsim_thread`.
- Ramona and Tamara passive sensors: [Ramona](https://en.wikipedia.org/wiki/Ramona_passive_sensor),
  [Tamara](https://en.wikipedia.org/wiki/Tamara_passive_sensor),
  [ArmedConflicts KRTP-81](https://www.armedconflicts.com/-t12985) — `czech_tdoa` (tier 4).
- 1L222 Avtobaza: [Wikipedia](https://en.wikipedia.org/wiki/Avtobaza) (tier 4).
- Soviet IFF: [Kremniy-2](https://mil-airfields.de/equipment/soviet-iff-kremniy.htm),
  [Parol](https://mil-airfields.de/equipment/soviet-russian-iff-parol.htm),
  [Crypto Museum](https://cryptomuseum.com/radar/kremnij2/index.htm) (tier 4).
- R-123: [Wikipedia](https://en.wikipedia.org/wiki/R-123); greenradio.de Eastern Mobiles (tier 4) — `r123_refs`.
- Western view of the SA-8: [9K33 Osa, Wikipedia](https://en.wikipedia.org/wiki/9K33_Osa) — `western_bands` (tier 4/5).
- HAVE QUICK: [Wikipedia](https://en.wikipedia.org/wiki/Have_Quick), [Crypto Museum](https://cryptomuseum.com/radio/havequick);
  SINCGARS: [Crypto Museum](https://cryptomuseum.com/radio/sincgars).
- ADM-141 TALD: [Wikipedia](https://en.wikipedia.org/wiki/ADM-141_TALD); Operation Mole Cricket 19:
  [Wikipedia](https://en.wikipedia.org/wiki/Operation_Mole_Cricket_19) (secondary).
- US Army FM 100-2-3, *The Soviet Army: Troops, Organization and Equipment* (1991),
  [FAS copy](https://irp.fas.org/doddir/army/fm100-2-3.pdf) — to read for the NATO threat library (not yet read).

Textbooks and standards:

- M. Skolnik, *Introduction to Radar Systems*, 3rd ed. (McGraw-Hill 2001); M. Skolnik (ed.), *Radar Handbook*, 3rd ed. (2008).
- M. A. Richards, *Fundamentals of Radar Signal Processing*, 2nd ed. (2014).
- D. K. Barton, *Radar System Analysis and Modeling* (Artech 2005).
- L. V. Blake, *Radar Range-Performance Analysis* (Artech 1986).
- B. R. Mahafza, *Radar Systems Analysis and Design Using MATLAB* (CRC).
- D. Adamy, *EW 101* (2001), *EW 102* (2004) (Artech).
- D. C. Schleher, *Electronic Warfare in the Information Age* (Artech 1999).
- R. G. Wiley, *ELINT: The Interception and Analysis of Radar Signals* (Artech 2006).
- R. A. Poisel, *Electronic Warfare Target Location Methods* (Artech, 2nd ed. 2012).
- S. Blackman, R. Popoli, *Design and Analysis of Modern Tracking Systems* (Artech 1999).
- Y. Bar-Shalom, X. R. Li, T. Kirubarajan, *Estimation with Applications to Tracking and Navigation* (Wiley 2001).
- J. D. Parsons, *The Mobile Radio Propagation Channel*, 2nd ed. (Wiley 2000).
- ITU-R P.526 (diffraction), P.676 (gaseous attenuation), P.838 (rain) — [ITU-R P.526](https://www.itu.int/rec/R-REC-P.526).
- R. G. Stansfield, "Statistical theory of D.F. fixing", *J. IEE* 94 IIIA (1947) ([DOI](https://www.doi.org/10.1049/JI-3A-2.1947.0096)).
- W. H. Foy, "Position-location solutions by Taylor-series estimation", *IEEE TAES* 12 (1976).
- D. J. Torrieri, "Statistical theory of passive location systems", *IEEE TAES* 20 (1984).
- S. J. Julier, J. K. Uhlmann, "A non-divergent estimation algorithm in the presence of unknown correlations", *ACC* 1997.
- W. Niehsen, "Information fusion based on fast covariance intersection filtering", *FUSION* 2002.
- G. L. Steele, D. Lea, C. H. Flood, "Fast splittable pseudorandom number generators", *OOPSLA* 2014 (SplitMix64).

Games, simulations and tools:

- Skynet IADS: [github.com/walder/Skynet-IADS](https://github.com/walder/Skynet-IADS).
- DCS ECM analysis: [flyandwire.com, 2022](https://flyandwire.com/2022/11/11/electronic-countermeasures-f-14-avionics/).
- MIXR: [github.com/doughodson/mixr](https://github.com/doughodson/mixr); B. Garland et al., "A Framework for Modeling a Real-Time Radar System", CSCI 2020 ([record](https://scholar.afit.edu/facpub/1857)).
- Stone Soup: [github.com/dstl/Stone-Soup](https://github.com/dstl/Stone-Soup).
- Sea Power EMCON notes: [sea-power.pmctactical.org](https://sea-power.pmctactical.org/emcon.php).
- Command: Modern Operations: Matrix Games / Warfare Sims documentation (behaviour summarised from the game's manuals and play; not independently verified here).
