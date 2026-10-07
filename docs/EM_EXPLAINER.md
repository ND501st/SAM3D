# How SAM3D's radar, signals and electronic-warfare world works

*A guide for the project owner: what the electromagnetic layer is, why it is built this way, and how
each part behaves. Written 7 October 2026. The engineering detail is in `docs/EM_ARCHITECTURE.md`;
the research behind it in `docs/research/EM_RESEARCH_2026-10.md`.*

---

## 1. The idea in one page

Every radar, jammer and radio in the game is a **transmitter putting energy into the air**. Every
radar, warning receiver, ESM station and radio is a **receiver** listening for it. Between them is the
terrain, the curve of the Earth, the ground's reflection and a little air. The game works out, for
each pair that matters, *how much energy arrives* and *compared with what noise*. Everything else
follows from that number:

* a radar **detects** an aircraft with a probability that rises as the echo gets stronger;
* what it **measures** (range, bearing) is more precise the stronger the echo;
* a warning receiver **hears** a radar long before that radar can see the aircraft;
* a **jammer** adds noise, so echoes get harder to see — more so the closer the jammer is to where
  the radar is looking;
* a **radio message** gets through, late, or not at all, depending on its link.

Nothing is scripted as "jammer on → radar off" or "aircraft within 30 km → detected". And, crucially,
**no unit knows the truth**. Each unit (an Osa vehicle, a command post, an aircraft) only has its own
**knowledge**: reports from its own sensors and what reached it over the radio, each with an age, an
uncertainty area and a confidence. The AI makes its decisions from that knowledge, exactly as a crew
would.

```
  what exists ──▶ what is radiated ──▶ what arrives ──▶ what is detected ──▶ what is tracked ──▶ what a unit believes ──▶ what it decides
  (truth)         (emissions)          (propagation)    (sensors)            (tracks)            (knowledge)               (doctrine / player)
                                                                                     ▲                    │
                                                                                     └── radio messages ──┘   (late, lossy, jammable)
```

## 2. Why it is built this way

| Choice | Why it matters to you |
|---|---|
| **Physics for the shape, published numbers for the size** | We rarely know a Soviet radar's every internal figure, but we do know "it sees a fighter at 40 km". The game sets each radar's overall sensitivity so that it reproduces the published range, and lets textbook physics decide how that range changes with target size, height, terrain and jamming. Nothing secret is invented, and what *is* known is respected. |
| **Truth separate from knowledge** | It is the difference between a game that "knows" and a simulation where a crew can be surprised, deceived or blind. It also makes multiplayer cheat-proof later: a player's machine only ever receives that player's knowledge. |
| **Everything is an emission** | A radar, a jammer and a radio call are the same kind of object, so a SIGINT station can hear all of them and a jammer can hurt all of them, without special rules. |
| **Systems are data** | A new radar is a block of JSON with sources, not new code. The Osa-AK's file cites a source for every number. |
| **Same results every run** | Random events use "dice" keyed to what they decide (this radar, this target, this sweep), so a replay with the same seed is identical — vital for testing, debriefs and multiplayer. |
| **Built outside Unreal** | The whole simulation runs and is tested without the engine (the demo runs 8 minutes of battle in 0.4 s). Unreal shows it. |

## 3. How radar works in the game

### 3.1 The fourth power

A radar's echo has to travel out *and back*, spreading out both ways, so its strength falls with the
**fourth power of range**. Halve the distance and the echo is 16 times stronger. Doubling the target's
radar cross-section (RCS) only buys 19 % more range (2^¼). That is why big aircraft are not seen *that*
much further away than small ones, and why closing in makes such a difference.

### 3.2 Probability, not a line on the map

The radar compares echo with noise (the **signal-to-noise ratio**, SNR). The game converts SNR into a
**probability of detection** for each sweep and rolls the dice. Around the published range the chance
is about 50 % per look; closer in it rises quickly.

| SNR (dB) | Chance of seeing it on one sweep |
|---|---|
| 6 | 1 % |
| 12.8 | 50 % (this is where published "detection range" usually sits) |
| 17.8 | 80 % |
| 21 | 90 % |

So a target at 45 km might flicker onto the scope now and then; at 30 km it is seen most sweeps. A
track needs it seen on 2 of 3 looks.

### 3.3 The beam and the sweep

The Osa's search radar turns once every 1.8 s with a beam 1° wide. Its beam is also split vertically
into three positions (0–4°, 4–8°, 8–27°), and it switches **one position per turn**. With all three
selected, any given height band is looked at every 5.5 s; with only the low beam, every 1.8 s but
nothing high is seen. That is a real operator choice, and the game models it. The tall top beam has
less gain, so a high, close target can actually be harder to see than one further out — a genuine
feature of radars with stacked beams.

### 3.4 Hills, the horizon and flying low

```
   radar ▲                                          ▲ aircraft at 300 m: visible
         │    ____                                 /
         │___/    \______ ridge _____             /
                          masked   \____  ....  low flier at 100 m behind the ridge: invisible
```

The game samples the terrain along the line from radar to target. If the ground is in the way, the
signal must **bend over the ridge** (diffraction). For radar frequencies that costs so much that the
target simply vanishes; for VHF radio the same ridge costs much less — which is why radios work over
hills that hide aircraft from radar. Even with a clear line of sight, a low aircraft's echo interferes
with its own reflection off the ground, so low fliers **fade in and out**, and near the horizon they
disappear long before the geometric horizon. With no tuning at all, this physics gives the Osa's search
radar 26 km against a fighter at 50 m; the Soviet developers' published figure is 27 km.

### 3.5 Clutter and the moving-target filter

The ground itself echoes. The Osa's radar has a **moving-target filter** (СДЦ) that cancels echoes that
do not move. Two consequences, both modelled:

* an aircraft flying a circle around the radar has no motion *towards* it and is filtered out with the
  ground ("beaming" — a real tactic);
* chaff (clouds of foil) drifting in the wind is not cancelled by the normal filter, but the Osa-AK has
  an **external-coherence** mode that takes its reference from the chaff itself and cancels it.

## 4. How signals and passive detection work

### 4.1 One way is much easier than two

A warning receiver on an aircraft only needs the radar's signal to arrive **once**, so it falls with the
*square* of range, not the fourth power. Rough numbers for the Osa's search radar (its power is
published; its antenna gain is estimated from its beam size):

| | At 100 km |
|---|---|
| Energy arriving at an aircraft from the main beam | about −29 dBm |
| A 1980s radar warning receiver needs | about −65 dBm |
| Margin | ~36 dB: the main beam is heard as far as the horizon allows (~300 km to an aircraft at 5 km) |
| Even the radar's weak sidelobes | heard out to ~350 km in free space |
| The radar's own detection range against that aircraft | ~40 km |

**That gap is the heart of electronic warfare.** Switching a radar on announces you to everyone in line
of sight. In the demo, ground direction-finding stations hear the attacking aircraft's radar at
80 km straight away, 99 seconds before the SAM's radar sees the aircraft at all.

### 4.2 What a passive receiver knows

It never learns "that is an F-16". It measures: **bearing** (direction), **frequency**, **pulse rate**,
**pulse width**, whether it is being swept once per turn or **illuminated steadily** (a tracking radar
locked on), and how strong the signal is. It compares that with its **threat library** — which is its
own side's intelligence, possibly wrong — and says, for example, "SAM search radar, 74 %". A signal
that fits nothing stays "unknown emitter". A decoy transmitting the same parameters as a real radar is
identified as that radar.

## 5. How detection, tracking and uncertainty work

```
 plot (one detection) ──▶ tentative track ──(seen 2 of 3 looks)──▶ confirmed track ──(missed when looked at)──▶ coasting ──(4 misses / too vague)──▶ dropped
```

* Every detection carries its error: range error a few tens of metres; bearing error a fraction of the
  beam, larger when the echo is weak or jammed.
* A track is a **best estimate plus an uncertainty ellipse** (a Kalman filter, the same mathematics
  real trackers use). The ellipse shrinks as detections come in and grows while the target is missed.
* A track is only "missed" when the radar actually looked where the target should have been.
* Quality runs from 0 (useless) to 1 (precise).

## 6. How triangulation works

One station gives a **line**: the emitter is somewhere along it. Two stations give a crossing; three give
a small triangle ("cocked hat"). The game solves for the most likely point and an honest **error
ellipse**:

```
       DF-1 ●───────────────╲
                              ╲      ╭──╮
       DF-2 ●──────────────────●────│ ● │  95 % ellipse, long axis along the lines
                              ╱      ╰──╯
       DF-3 ●───────────────╱
```

* Accuracy depends on how good each bearing is (1–2° for ground DF stations, 10–15° for an aircraft's
  warning receiver), the distance, and the **crossing angle**: stations close together, far from the
  emitter, give a long thin ellipse.
* **One moving receiver** can triangulate a stationary radar by taking bearings over time. In the
  tests, an aircraft flying 60 km across the line to a SAM 70 km away fixes it to 0.3 km with a 2° ESM
  pod — but only to ~10 km uncertainty with a warning receiver. That is why warning receivers warned
  and specialised ESM located.
* **Time difference of arrival** (TDOA): several sites note *when* the same pulse arrives; the
  differences give hyperbolas that cross at the emitter. This is how the Czechoslovak **Ramona** (1979)
  and **Tamara** (1987) systems worked. In the tests, four sites fix an aircraft's radar at 114 km to
  about 120 m horizontally — though only to a kilometre or two in height, because ground sites are
  nearly in one plane.
* In the demo the command post's three stations fix the attacking aircraft's radar to 3.3 km (CEP
  2 km) at about 80 km — "something is radiating in this area", not "there is the aircraft".

## 7. How jamming works

### 7.1 Noise jamming

A noise jammer shouts into the radar's receiver. The radar now compares the echo with **noise plus
jamming**. The more jamming gets in (`J/N`, jamming-to-noise), the shorter the radar's detection range:

| J/N | Detection range left |
|---|---|
| 0 dB (jamming equal to the radar's own noise) | 84 % |
| 10 dB | 55 % |
| 17 dB (seen in the demo from a stand-off jammer, through the radar's sidelobes) | 37 % |
| 30 dB | 18 % |

Where the jammer is matters: when the radar looks *straight at* the jammer, it takes the jamming through
its main beam and that whole sector goes blind; looking elsewhere, it takes it only through its weak
sidelobes. So the jammed coverage is a **lopsided shape**, not a smaller circle (the `jamming` overlay
draws it). And the jammer gives itself away: the radar sees a **strobe** — a bright line on the scope
pointing at the jammer, with no range. Two radars' strobes cross at the jammer.

### 7.2 Burn-through

A jammer *on the attacking aircraft* fights a losing battle as it closes: its noise weakens with the
square of range, the aircraft's echo with the fourth power, so eventually the echo wins. In the test a
small self-protection pod keeps the aircraft hidden until 19 km (against 53 km without it) — but the
strobe announced it from 70 km.

### 7.3 Deception

* **Range-gate pull-off:** a repeater on the aircraft returns the tracking radar's pulse slightly late,
  louder than the real echo, drags the range gate away, then stops — the radar is left tracking empty
  space and loses lock. Modelled and tested.
* **Chaff:** foil clouds that make false echoes and hide aircraft inside them, defeated by the right
  moving-target mode (§3.5).
* **Angle deception** does not work against the Osa's monopulse tracking radar (its manual says so),
  so the game treats it as immune.

### 7.4 What jamming does to each function

Detection range shrinks; tracks get bigger ellipses and drop more often; position accuracy falls;
identification of the jammer itself gets easier (it is "noise jammer, hostile"); engagements need
burn-through or a fallback — the Osa can track by TV and take range from its search radar, or guide on
the jammer's bearing alone ("method Н"). Radios are jammed the same way: a jammer in the VHF band
lowers every link's margin, and messages are delayed or lost.

## 8. How command and control works

```
                    Battery command post (PU-12)
                   ╱  voice net, one speaker at a time  ╲
              SAM-1                                    SAM-2
       reports its tracks up                    receives the picture, stays dark
```

* **Radio nets** are shared channels: one person talks at a time. A voice report takes several seconds
  plus the operator's reaction, so a busy net **queues** — in the demo the battery net occasionally
  saturates and the vehicles briefly mark their picture as "degraded".
* Each **link** is worked out with the same propagation as radar, plus the radios' own interference: the
  Osa-AK manual says its R-123M radios reach 15 km, but only 12 km while the vehicle's radars are on —
  the game reproduces both.
* Subordinates **report** their own tracks up; the command post **fuses** everything and **broadcasts its
  picture** down. Reports of the same aircraft from different routes are combined so that hearing a
  rumour twice never makes it more certain.
* **Losing the command post** (demo, 330 s): nothing more arrives; the vehicles' picture ages, the
  aircraft positions get vaguer and finally vanish; after about 25 s they report "degraded", after about
  60 s "autonomous" — and SAM-2, which had been sitting dark relying on the command post, switches its
  own radar on "no picture from command: radiating autonomously". That is the downstream consequence of
  attacking C2, emerging rather than scripted.

## 9. How the AI fits in

AI units have exactly the same information as a crew in their position. The current SAM rules (more
will come):

* connected, nothing threatening close: **stay dark** (the command post's radar is watching);
* a hostile or unknown aircraft predicted inside the cue range, from the unit's own picture: **radar on**,
  and log why ("cue: track 4 at 21 km (network)");
* nothing for a while: **dark again**;
* command lost: **radiate autonomously**;
* a threat inside engagement range: point the tracking radar at it.

Because they read knowledge, AI units can be deceived, cued late, or left blind — the same as you.

## 10. What Unreal does

Unreal draws the world and the cabin; simcore runs the simulation at a fixed 10 times per second inside
the game's world subsystem. For now the electromagnetic layer runs its demo scenario at the current map
location, with toggleable overlays:

| Console command | Effect |
|---|---|
| `sam3d.em.Start` / `sam3d.em.Stop` | start / stop the demo |
| `sam3d.em.Debug -1` | show every overlay (`0` none; a number picks layers, see below) |
| `sam3d.em.Owner -1` | show every unit's knowledge (`0` command posts only, or a node id) |

Overlay layers (add the numbers): 1 truth · 2 emitters · 4 beams · 8 coverage · 16 line of sight ·
32 signal paths · 64 bearings · 128 fixes · 256 uncertainty · 512 tracks · 1024 C2 links ·
2048 radio traffic · 4096 jamming · 8192 status · 16384 detection numbers.

Outside Unreal, `tools/em_view.py` makes a browser page from the demo with the same overlays, a time
slider and the event log, and the demo writes a Tacview replay (`out/em_demo.acmi`) showing the real
aircraft beside the command post's beliefs.

## 11. What is simulated, what is abstracted, what is left out

| Simulated properly | Simulated simply | Left out on purpose |
|---|---|---|
| Line of sight, Earth curvature, terrain masking, bending over ridges | Ground reflection (smooth-Earth version) | Individual pulses and waveforms |
| Radar sensitivity from published ranges; probability of detection | Ground clutter strength | Ray-traced reflections off buildings |
| Beams, scanning, elevation-beam programmes | Aircraft RCS (nose/side/tail only) | Polarisation |
| One-way passive detection, main beam vs sidelobes | IFF (replies, no codes yet) | Ducting and freak propagation |
| Noise jamming, strobes, burn-through | Radio message content (track data only) | Spread-spectrum / LPI radars (not of the period) |
| Moving-target filter, beaming, chaff vs coherence modes | Tracker (simple association) | Packet-level radio protocols |
| Measurement errors, tracks, uncertainty ellipses | | |
| Triangulation and TDOA with honest error areas | | |
| Range-gate pull-off | | |
| Radio links, latency, saturation, jamming, destruction, autonomy | | |
| Knowledge, ageing, fusion without double counting | | |

## 12. Limitations today

* The electromagnetic layer is **not yet wired into the Osa's own console and scopes** — that is the
  next step (it will replace the current simple "search picture" rules).
* No missiles, anti-radiation missiles or TV sight inside this layer yet; the AI doctrine is basic.
* Terrain is treated as one dominant ridge per path; forests and towns are not obstacles yet.
* Several Osa internals (exact frequencies, pulse rates) are not public; they are labelled estimates and
  only affect what enemy receivers hear and how jamming bites, not the published ranges.
* One known gap: over smooth ground the model lets the Osa's tracking radar lock a 50 m target further
  out (about 25 km) than the published 14 km unless "lock" means reliable detection (then it gives
  14 km). It is recorded as an open question.

## 13. How it grows

* **New systems are data**: an SA-6, a NATO fighter's radar, an AWACS, a civilian air-traffic radar
  are entries in `data/em/`. Their behaviour comes from the same physics.
* **Next phases** (roadmap): drive the Osa's scopes and lock from this layer; the battery's PU-12 and
  voice designations; missiles and the TV sight; anti-radiation missiles and decoys; deception and
  emission control in depth; more speed; richer terrain; multiplayer that sends each player only their
  own knowledge.

## 14. Try it

```bash
cmake --build simcore/build --config Release
simcore/build/Release/em_demo.exe data out/em_demo.json out/em_demo.acmi    # timeline + 11 checks
python tools/em_view.py                                                    # then open out/em_view.html
```

In Unreal: play the level, open the console (`~`), type `sam3d.em.Start`, then `sam3d.em.Debug -1`.
