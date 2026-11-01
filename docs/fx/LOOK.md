# The look of the weapons' effects (FX)

What each weapon effect should look like, as numbers against time, at 1, 5 and 15 km and at night, and how SAM3D draws
it with the layers of docs/fx/README.md section 4.1. Compiled 1 November 2026 from four research passes (real
explosions; trails and guns; how games draw effects cheaply; free assets). It is the target for `data/fx/effects.json`,
`fx::LookFor` (`simcore/fx/explosions.hpp`), `tools/fx_textures.py` and `SimFxEffects.cpp`; it changes none of them.
Where today's values differ from the target, each section says so under **Now**.

## 0. Conventions and what every burst shares

* Times run from the event (detonation, ignition). Sizes are diameters unless named a radius. Colours are sRGB hex as
  the eye sees them, before exposure; `effects.json` takes linear RGB, so convert (#9C8466 is linear 0.332, 0.231,
  0.133). Opacity is a sprite's or ribbon's at its centre, 0-1.
* Each number is **sourced** (a source states it), **derived** (from a sourced law; the formula is given) or an
  **estimate** (no source: an art value). **Snippet**: only a search engine's summary of the page was seen; weaker.
* W is the charge in kg of TNT equivalent (`charge_kg` in `munitions.json`).
* Pixels are on the player's camera, 70° across 1920 px (`SimViewComponent`): 0.636 mrad, 27.4 px per degree. The TV
  sight at 1.44° multiplies them by about 49. The tiers (README 4.1): over 48 px full, 6-48 px impostor, under 6 px a
  glint at night and nothing by day. A burst's tier is judged on 2 × max(fireball, smoke radius) (`SimFxEffects.cpp`):
  32 m for 100 kg, so it is full to about 1.0 km, an impostor to 8.4 km and gone by day beyond.
* Sound arrives 2.9 s after the flash at 1 km, 14.7 s at 5 km, 44 s at 15 km (340 m/s).
* Haze (Koschmieder's law, textbook): contrast C = exp(−3.9 d / V). With 25 km visibility a dark cloud keeps 86 % of its
  contrast at 1 km, 46 % at 5 km and 10 % at 15 km, a pale blue-grey shape; with 10 km visibility 68 %, 14 %, under 1 %.
* Source tiers as `docs/SOURCES.md`: **1** official documents, manufacturers and historic records; **2** operator and
  veteran accounts; **4** encyclopaedias and press; **5** Western facts-and-stats pages, held as a check only, never the
  basis. Two labels from SOURCES.md's other sections: **textbook** for physics papers, lab and test reports (as its
  world and physics rows) and **design** for game developers, engine documentation, modders and players (as its games
  rows). Registered ids are named where they exist (`wiki_guns`, `yugoimport_ammo`, `hrw_1999_cluster`,
  `parsch_dusrm`, `voenizdat_osa_1991`).

### 0.1 Flash, fireball and colour by temperature (the `heat` ramp)

* Detonation lasts microseconds; the fireball's temperature peaks about 1 ms after initiation and changes slowly from
  5 to 55 ms ([Piehler et al., ARL 2009](https://www.mobilityengineeringtech.com/component/content/article/5330-arl-0040), textbook).
* Thirteen field shots of 0.1-2 kg aluminised RDX: the fireball's largest radius 4.3-5.8 m by about 50 ms
  ([Gordon, AFIT ADA538513, 2010](https://apps.dtic.mil/sti/pdfs/ADA538513.pdf), textbook, snippet). Another series:
  0 to about 8 m in 230 ms, then shrinking over 1-3 s above 1800 K ([Schott, Gross, Perram 2008](https://meetings-archive.aps.org/osf/2008/c5/1), textbook, snippet).
* *Radiant Brass III*, 56 static and dropped bomb detonations: 1685-1885 K at the start, glowing 0.91-1.24 s; a cooling
  curve T(t) = 300 + 1700·e^(−t / 1 s) K ([Gross, Perram, Tuttle, SPIE 5811, 2005](https://nanophotonics.spiedigitallibrary.org/conference-proceedings-of-spie/5811/0000/Modeling-infrared-spectral-intensity-data-from-bomb-detonations/10.1117/12.603206.full),
  snippet; [their HITRAN 2006 poster](https://lweb.cfa.harvard.edu/hitran/HITRAN_conf06_presentations/PosterSession1/P1.17-Gross.pdf); textbook).
  The earliest temperatures 3000-3500 K (Gordon 2010, snippet).
* Fireball diameter D ≈ 3·W^0.3 m ([Mori 2020, safety distances in bomb disposal](https://www.earmi.it/balistica/Bombe%20ed%20evacuazioni.pdf), textbook: EOD practice).
  Ours is R = 1.1·W^(1/3), a diameter of 2.2·W^(1/3) (`fx::FireballRadiusM`): 10 m against Mori's 12 m at 100 kg.
* A cased bomb by day is mostly smoke and dust; its fireball is small and brief. A 1 kg military plastic charge "didn't
  appear very big at all"; film fireballs are petrol bags burst by det-cord ([Laughing Squid on Tom Scott and a pyrotechnician](https://laughingsquid.com/why-real-explosions-dont-look-like-those-in-movies/), 4).
  Sim players say the same of bombs ([DCS forum, why DCS explosions are so small](https://forum.dcs.world/topic/347894-why-is-dcs-explosions-so-small), design).
* Aluminised fills (Tritonal; A-IX-2: 73 % RDX, 23 % Al, 4 % wax, [Wikipedia](https://en.wikipedia.org/wiki/A-IX-2), 4) show Al
  lines at 394/396 nm and AlO at 484 nm (Piehler 2009): a whiter flash and a longer afterglow than TNT (estimate).

Blackbody colour (derived: CIE 1931 2°, Wyman et al. 2013 fit, sRGB, brightest channel 1) and luminance (derived:
Planck with the photopic curve, emissivity 1) against a sunlit white surface (100,000 lux / π ≈ 31,800 cd/m²). This is
`T_FX_HeatRamp` behind per-instance float 2, and it should carry brightness as well as hue:

| T (K) | 1000 | 1300 | 1500 | 1800 | 2000 | 2500 | 3000 | 3500 | 4000 |
|---|---|---|---|---|---|---|---|---|---|
| sRGB | #FF2F00 | #FF5800 | #FF6A00 | #FF8100 | #FF8D15 | #FFA64A | #FFB96E | #FFC88C | #FFD4A6 |
| × sunlit white | 0.0001 | 0.02 | 0.24 | 3.6 | 14 | 175 | 940 | 3200 | |

So a fireball falls below a sunlit wall's brightness near 1650 K, about 0.2 s in on Gross's curve: by day it vanishes
into its own smoke; at night the same 1300-1500 K stays a dull red glow. The eye saturates the
3000 K core to white; a sooty fireball reads redder than its temperature and its edge goes dark red into black
(estimate). **Now:** `fire_brightness` 30 is a 2100-2200 K blackbody held; `flash_brightness` 80 is far under the
physical 900-3200 of a 3000-3500 K core (fine: it clips either way; see T11).

### 0.2 Smoke colour by fill and by ground

| What burns or is thrown | Where | Colour | sRGB | Basis |
|---|---|---|---|---|
| TNT (oxygen balance −74 %) | FAB bombs, 85-130 mm AA shells | black soot | #2E2B28 | sourced: too little oxygen for its carbon, "large amounts of black, sooty smoke" ([Wikipedia, Oxygen balance](https://en.wikipedia.org/wiki/Oxygen_balance), 4; [scitoys, TNT](http://scitoys.com/tnt.html), 4); hex estimate |
| RDX (−21.6 %), A-IX-2 | 23-57 mm shells, many warheads | light grey to white | #C9C6C0 | derived from the balance; 37/57 mm "white puffballs" ([DCS forum, flak puffs, quoting a Vietnam source](https://forum.dcs.world/topic/160596-flak-puffs/), design); hex estimate |
| Tritonal (TNT + Al) | Mk 80 series | grey-black, whiter flash | #3A3734 | estimate |
| Soil: loess, brown earth | the Pannonian plain | yellow-brown | #9C8466 | estimate |
| Soil: chernozem | | dark brown | #4A3F35 | estimate |
| Soil: chalk | | off-white | #D8D3C4 | estimate |
| Masonry | buildings | pale grey-beige | #B9AE9C | estimate |
| Hydrocarbon fuel | aircraft, napalm, fuel stores | black | #1E1C1A | estimate |
| White phosphorus | Shrike's marker | white | #F2F1EC | estimate (dense white: §8) |

These are albedos: the smoke is lit (T1), so sunlit tops read lighter and undersides darker.

**Now** (linear in `effects.json`, here as sRGB): air smoke #353332 greying to #6E6A65, dust (loess) #9C8466, the
ground column from #5A554E low to #8B867E high (× `soot_darken` 0.6 for the TNT fills in `soot_munitions`), masonry
#A2998B, debris #555045, flash #FFEDCB, spark #FFDAA0, ring #F3EDE1, flak light #A2A09B and black #2E2B28, white
phosphorus #CBCBC8, cluster haze #817665, launch cloud #E7E6E3, missile trail #E8E6E1, tracers #FFCE61 by day (so the
additive streak over the sky reads #FFE8C0, §14) to #FF4A24 at night, muzzle flash #FFB36C, gun smoke #C8C5C0, wreck
smoke #2C2B2A, wreck column #303030, contrail #F9FAFD, afterburner #FFC459 by day to #FF6E0E at night (target #FF8D15)
and #5565FF for the F-15 and F-16 (target #7A8CFF): the night flames' weaker channels lowered so the night's exposure
does not clip them (§13), diamonds #FFE8C0.

### 0.3 The dust and smoke column

* Stabilised cloud: H = 92.6·W^0.25 m, R = 4.7·W^0.375 m ([Church 1969, Sandia SC-RR-68-903, via arXiv:0902.3824](https://arxiv.org/pdf/0902.3824), textbook).
  A 5 kg cloud stabilised at about 48 s; Israel's "Green Field" shots (0.25-100 kg, 2006-09) top out lower than Church
  under 50 kg ([Nasstrom et al., LLNL-PRES-523194](https://narac.llnl.gov/sites/narac/files/2024-02/LLNL-PRES-523194.pdf);
  [Sharon et al. 2012](https://manuscript.isc.ac/Inventory/10/2239476.htm), abstract; textbook). Height grows as
  t^½, width about linearly at first, temperature falls as 1/t ([Duan et al. 2019](https://www.bzycj.cn/en/article/doi/10.11883/bzycj-2017-0380), textbook, abstract).
  Burst height (ground or 1 m up) matters only early ([Cao et al. 2011](https://zendy.io/title/10.1002/prep.200900033), textbook, abstract).
* Shape: soil leaves as an ejecta curtain, an inverted cone ([Graettinger et al. 2015](https://inrs.ca/wp-content/uploads/2020/09/Graettinger-et-al-facies-distribution-2015-web.pdf), textbook);
  "a more V shaped explosion rather than the typical tall explosion"; fade ground debris by erosion, like mud, not by
  alpha; a short ground shock ring decal and a fading ground shadow ([realtimevfx, Artillery Explosion WIP](https://realtimevfx.com/t/artillery-explosion-wip/1047), design).
  "Dust should be hanging in the air a lot" ([Hell Let Loose devtracker](https://devtrackers.gg/hell-let-loose/p/fca4179d-unsatisfying-explosions), design).
* Derived column, H(t) = H_stab·√(t / t_stab); low bound 0.7 × Church (Green Field-like), high bound Church; t_stab an
  estimate. For t < 2 s the ejecta jet, not buoyancy, sets the height.

| Charge (W) | 1 s | 3 s | 10 s | 30 s | stabilised (t_stab) | width 2R |
|---|---|---|---|---|---|---|
| Mk 82, FAB-250 (95-100 kg) | 26-37 m | 45-65 m | 83-118 m | 143-204 m | 202-289 m (60 s) | 53 m |
| FAB-500 (200 kg) | 28-40 m | 49-70 m | 89-127 m | 154-220 m | 244-348 m (75 s) | 69 m |
| Tomahawk (300 kg) | 30-43 m | 52-75 m | 95-136 m | 165-236 m | 270-385 m (80 s) | 80 m |
| Mk 84 (470 kg) | 32-45 m | 55-79 m | 101-144 m | 174-249 m | 302-431 m (90 s) | 94 m |
| Kh-22M (900 kg) | 36-51 m | 61-88 m | 112-160 m | 194-278 m | 355-507 m (100 s) | 121 m |

* Column width (estimate): about the fireball at 1 s, twice it by 3 s, three to four times by 10 s, then 2R; after that
  the wind drifts and shears it for 1-10 min.
* A 500 lb bomb destroyed by EOD threw debris about 20 m high and its dust engulfed a minaret about 300 m away
  ([Inquirer, Marawi](https://newsinfo.inquirer.net/1222817/military-detonates-500-pound-unexploded-bomb-in-marawi/amp), 4).

### 0.4 Shock ring and condensation (Wilson) cloud

* A Wilson cloud forms only in humid air: the shock's negative phase cools the air below its dew point outside the
  fireball, and the cloud vanishes as the pressure recovers ([Wikipedia, Condensation cloud](https://en.wikipedia.org/wiki/Condensation_cloud), 4).
  Beirut 2020: relative humidity 0.73; the cloud lasted to t/t₀ ≈ 4 with t₀ ≈ 1 s and R₀ ≈ 280 m for 2.3 TJ; the shock
  ran at 363 m/s; the dense front showed ahead of the cloud by bending light ([Czarnecki et al., arXiv:2510.24742](https://arxiv.org/html/2510.24742v2), textbook).
  Outdoor shadowgraph shows explosion shocks against a textured background in daylight ([Biss et al., ISSW 2007](https://nmt.edu/academics/mecheng/faculty/mhargather/docs/2007-Biss-ISSW.pdf), textbook).
* Scaled by E^(1/3) (derived): 9M33 (6 kg) R₀ 6 m, under 0.1 s; Mk 82 R₀ 16 m, about 0.2 s; Mk 84 R₀ 27 m, about 0.4 s;
  Kh-22M R₀ 33 m, about 0.5 s.
* Draw the shock as a ring at 340-400 m/s (100 m in about 0.3 s), faint; a white condensation shell only when the
  weather's relative humidity is about 70 % or more (estimate).

### 0.5 Craters, scorch and vegetation

| Item | Value | Basis |
|---|---|---|
| Surface charge on soil | D = k·0.4·W^(1/3) m, k 1.7 (very hard) to 6 (very soft); ±10 %, up to 30-40 % | Mori 2020 (textbook) |
| Shell that penetrates the ground | D ≈ 10 × calibre | Mori 2020 |
| 500 / 1,000 / 2,000 lb GP bombs | 3-11 / 4.6-15 / 6-18 m, by soil and depth | FM 9-40 (1953) quoted on the [BOCN forum](https://www.bocn.co.uk/threads/bomb-size.78147) (1 through a forum) |
| German SC 500 | instantaneous fuze 5.6 m × 2 m deep; delay 10.5 × 4 m | same |
| Mk 84 | 15 m × 11 m deep (a delay fuze; weak citation); fragments lethal to 120 m | [Wikipedia, Mark 84](https://en.wikipedia.org/wiki/Mark_84_bomb) (4) |
| FAB-250 / FAB-500 / FAB-1500 | 3 / 4 / 6 m deep, "radius" 10 / 12 / 14 m (approximate) | [AiF, interview with Col. G. Kryukov](https://aif.ru/politics/russia/prosto-bomba-starye-modeli-fab-okazalis-vostrebovany-v-zone-svo) (4) |
| Vegetation, diameter | trees down 1.55·W^0.425; trees badly damaged 2.2·W^0.425; leaves stripped 3.3·W^0.425 m | Mori 2020 |

Derived: 100 kg, crater 3.2-11 m, vegetation zones 11, 16, 23 m; 470 kg, crater 5.3-19 m, zones 21, 30, 45 m.
Decals (estimate): a dark soot-stained bowl, a raised lip, an ejecta blanket with radial rays to 2-4 crater radii.
**Now:** `kCraterM` 3.5 m radius at 100 kg (7 m across) sits inside the range; the scorch decal is 2.5 × the crater.

### 0.6 Distance and night, for every burst

Pixels on the 70° camera (derived):

| Size | 1 km | 5 km | 15 km |
|---|---|---|---|
| 2 m (a 23 mm puff) | 3.1 | 0.6 | 0.2 |
| 5 m (a 9M33 fireball) | 7.9 | 1.6 | 0.5 |
| 12 m (a Mk 82 fireball) | 19 | 3.8 | 1.3 |
| 30 m (a large SAM's puff) | 47 | 9.4 | 3.1 |
| 58 m (a 1 t fuel fireball) | 91 | 18 | 6.1 |
| 100 m × 45 m (a Mk 82 column at 10 s) | 157 × 71 | 31 × 14 | 10 × 4.7 |
| 250 m (a column stabilised) | 393 | 79 | 26 |

* By day a 12 m fireball at 15 km is a 1-2 px point: draw it as a flash with bloom. A column is an impostor-sized
  shape at 15 km even when its fireball is invisible.
* Night: close flak bursts were "blinding red explosions" ([320th BG, Balls of fire](https://320thbg.org/balls_of_fire_2.html), 2, snippet).
  The flash's light peaks within 50 ms and decays over about 1 s, with the fireball (Gross 2005); it lights the underside
  of the column and the ground near it; at 15 km a burst is a flicker on the horizon and the cloud base; whatever
  burns keeps glowing for minutes (estimate).

## 1. Techniques (referred to as T1-T13 below)

From game developers, engine documentation and players (all design). The sprites carry 8 floats (0 frame, 1 opacity,
2 heat, 3-5 tint, 6 erosion, 7 lit); ribbons are one procedural mesh; 8 shadowless lights and 32 decals are pooled.

**T1 Six-way lightmaps for smoke.** Each flipbook frame rendered six times, a white light from top, bottom, left,
right, front and back, packed in two RGBA textures (Unity: A = right, top, back, alpha; B = left, bottom, front,
emissive) ([Unity, 6-way lighting in VFX Graph](https://unity.com/blog/engine-platform/realistic-smoke-with-6-way-lighting-in-vfx-graph);
[Unity manual](https://docs.unity3d.com/Packages/com.unity.visualeffectgraph@17.0/manual/six-way-lighting.html)).
Skull & Bones packs four (top, left, right, bottom) in a 1K uncompressed RGBA mask, 8x8 frames of 128 px, with a DXT1
colour key and alpha, a 16x16 albedo ramp and a 128x16 emissive ramp shared by every effect, 8.06 MB in all; top and
bottom can't share a channel. At run time pick one map per axis (h = R if lx > 0 else L; v = T if ly > 0 else B; d =
front if lz > 0 else back) and light = h·lx² + v·ly² + d·lz², the light vector in the sprite's tangent space
([Mederic, Skull & Bones smoke lighting](https://realtimevfx.com/t/smoke-lighting-and-texture-re-usability-in-skull-bones);
[6-way lightmap WIP](https://realtimevfx.com/t/6-way-lightmap-wip/5457)). It gives self-shadowing and a bright rim
when backlit, right under any sun, at about the cost of a plain lit sprite; it has no occlusion between particles and
must not be mixed with normal maps. For us: `fx_textures.py` ray-marches each frame's density field toward the six
directions and writes Unity's packing; the sun's direction and colour, the sky ambient and the flash colour go in a
material parameter collection; the vertex stage takes the quad's axes from the instance transform
(lts = L·right, L·up, L·toCamera). Float 7 ("lit") is then free for T9.

**T2 Soft particles and a camera fade.** Opacity × saturate((scene depth − pixel depth) / fade), with a contrast curve
symmetric about (0.5, 0.5) so no crease shows ([Lorach, NVIDIA Soft Particles, 2007](https://developer.download.nvidia.com/whitepapers/2007/SDK10/SoftParticles_hi.pdf));
compensate grazing views ([realtimevfx, small VFX tips](https://realtimevfx.com/t/small-vfx-tips/923)). IL-2's old
explosions clipped through the ground ([IL-2 forum, visual effects feedback](https://forum.il2sturmovik.com/topic/36629-visual-effects-feedback-thread/)).
For us: `DepthFade` in `M_FX_Smoke` and `M_FX_Fire` ([Epic, depth expressions](https://dev.epicgames.com/documentation/unreal-engine/depth-material-expressions-in-unreal-engine)),
fade distance 0.3-0.5 × the sprite's radius (the length of the instance's local X); a camera fade tested at the
sprite's pivot so the whole quad goes together ([vfxdoc, Fading](https://vfxdoc.readthedocs.io/en/latest/shaders/fading/)):
the player stands in launch smoke at the vehicle.

**T3 Alpha erosion.** alpha = smoothstep(e, e + feather, texAlpha), e from −feather to 1 + feather, erosion maps linear
([vfxdoc, Alpha erosion](https://vfxdoc.readthedocs.io/en/latest/shaders/alpha-erosion/)). Halo 3 found an alpha ramp
bland and used the black-point fade for dirt, water and explosions ([Game Developer, Halo 3's sparks](https://www.gamedeveloper.com/pc/gamefest-how-i-halo-3-i-s-lots-and-lots-of-sparks-got-made)).
For us: float 6 by age on a curve per layer; float 1 kept for whole-sprite fades (camera, distance).

**T4 Frame blending and motion vectors.** The frame's fraction cross-fades two frames, free. Motion vectors: UVcur =
SubUV(n) − M·frac·S, UVnext = SubUV(n+1) + M·(1 − frac)·S, M = 2·sample − 1; the vector map can be an eighth of the
base, uncompressed, sRGB off; it stretches an 8x8 sheet more than 10× ([Klemen Lozar](http://www.klemenlozar.com/frame-blending-with-motion-vectors/));
6x6 frames with vectors beat 11x11 without ([loronsodev, UE5 explosions](https://loronsodev.github.io/posts/explosion-fx)).
For us: the cross-fade now; vectors baked by `fx_textures.py` from the same density field later.

**T5 Fire and smoke in one premultiplied draw.** Unreal sorts translucency per component, not per instance
([UE forum](https://forums.unrealengine.com/t/per-instance-translucency-sort-priority-hism/2413260)), so our Fire ISM
draws wholly before or after the Smoke ISM and fire behind a cloud shows through it. Premultiplied alpha does additive
(alpha 0) and normal blending in one state, order-independent, without halos ([Microsoft, premultiplied alpha](https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2013/dn449510(v=vs.120))).
Sort inside an effect by age, oldest on top or a stable order (Mederic; realtimevfx artillery thread). For us: one
`AlphaComposite` material for smoke and fireballs: emissive = lit smoke·α + heat emission, opacity = α·(1 − additive);
clusters by distance, inside a cluster by age.

**T6 Curves over life and variety.** Size, colour and animation speed keyed over life, colours above 1 where they must
show ([BIKI, Arma 3 particle effects](https://community.bistudio.com/wiki/Arma_3_Particle_Effects), snippet). Slow
spiky smoke as its flipbook plays; randomise lifetimes; start ground dust at half size and fade it in fast; a few big
puffs for the silhouette and small ones over it, never mixed texel densities ([realtimevfx, explosion feedback (cEssex)](https://realtimevfx.com/t/looking-for-feedback-on-explosion-vfx/29528)).
Variants picked at random per event (Falcon BMS `emitter.probability`, [BMS forum](https://forum.falcon-bms.com/post/214378?page=2);
War Thunder, [Drone Age effects](https://devtrackers.gg/war-thunder/p/b08b5398-new-visual-effects-in-the-drone-age-update)).
For us: size(t), alpha(t), erosion(t), heat(t) per layer in `effects.json`, a random life factor of ±30 %, a random
start roll with drag on its spin, 2-4 variants per effect (estimate).

**T7 Fewer pixels.** Pixels drawn by quad shape: full 100 %, bounding box 69.2 %, fitted 4-gon 60.2 %, 6-gon 53.9 %,
8-gon 51.9 % ([Humus, particle trimming](https://www.humus.name/index.php?ID=266)). Half- to eighth-resolution buffers
suit soft smoke, not debris ([GPU Gems 3, ch. 23](https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-23-high-speed-screen-particles)).
Falcon BMS fell to about 10 fps near explosions and alpha-heavy scenes to 2-3 fps ([BMS forum, fishy explosions](https://forum.falcon-bms.com/post/42440?page=2)).
For us: the sprite ISM's mesh an octagon fitted to the union of the atlas frames' alpha (about 40-45 % less fill, same
instance data); the camera fade caps one sprite's screen area.

**T8 Columns on vertical-axis billboards.** 4-8 sprites stacked up a vertical axis, each rising and growing on its own,
dark soil low and lighter dust high; turned about the vertical only, so the column does not lean when the player looks
up ([Unity, particle renderer](https://docs.unity3d.com/es/Manual/PartSysRendererModule.html)); smoke-spike flipbooks
for ejecta (realtimevfx artillery thread). Size by W^(1/3) as README section 3.3.

**T9 The flash lights the smoke.** Frostbite lit smoke and debris with muzzle flashes and explosions ([EA, Lighting you up in Battlefield 3](https://www.ea.com/frostbite/news/lighting-you-up-in-battlefield-3));
IL-2's smoke takes light from nearby fires ([Stormbirds](https://stormbirds.blog/2018/09/14/a-look-at-il-2s-new-effects/));
a DCS script adds night flak flashes that light aircraft and clouds ([DCS Flak Flash Script](https://www.digitalcombatsimulator.com/en/files/3346486)).
Our 8 lights reach opaque surfaces only. For us: the CPU sums the pool's lights at each particle (inverse square within
a radius) into float 7; the material multiplies it by the flash colour and the front lightmap. The light's curve: a
1-2 frame peak, then 0.1-0.4 s white to orange ([Arma Reforger explosion flash mod](https://reforger.armaplatform.com/workshop/4558464C41534801)),
then the fireball's ~1 s orange tail at night.

**T10 Ribbons.** Puffs near the head, a ribbon behind: MSFS 2024 ships `Contrails_Quad` (many particles, near) and
`Contrails_Ribbon` (two particles and a textured ribbon, far), chosen by distance ([MSFS 2024 SDK, VFX templates](https://docs.flightsimulator.com/msfs2024/retail/content-configuration/models/modelbehaviors/usingbehaviours/vfx/visual-effects-templates/)).
Points by distance travelled, not per frame, and the gap filled for fast missiles ([UE forum, smoke trails](https://forums.unrealengine.com/t/best-way-to-do-smoke-trails/46199);
[Leadwerks](https://www.leadwerks.com/community/blogs/entry/667-smoke-ribbons-when-emitters-are-too-slow)); U = distance
along the path, so the texture stays in the air ([UE forum, ribbon UV](https://forums.unrealengine.com/t/niagara-ribbon-uv-and-trail-effect/479832));
noise scrolled slowly, or the trail looks like a thruster; width and erosion over age ([realtimevfx, Azeros](https://realtimevfx.com/t/azeros-sketchbook/20271/4)).
Light ribbons per vertex on the CPU: sun·HG(view·sun, g ≈ 0.5-0.7) + sky ambient into the vertex colour (Henyey-Greenstein,
textbook; g for smoke unverified). **Phone-wire AA**: never narrower than 1 px; below 1 px keep 1 px and scale alpha by
the true width in pixels, so a far line never breaks into dots ([Humus, phone-wire AA](http://www.humus.name/index.php?page=3D&ID=89)),
the width from the true angular size for the view that sees it, so the TV sight's zoom changes it smoothly. A trail is
owned by its effect, not its missile, and outlives it.

**T11 Flashes that read by day.** Clear-sky sun about 100,000 lux, EV 15; a 3000 cd/m² fire that glows at night is
nearly invisible at EV 18 ([taku25, lighting and symbolic VFX in UE](https://dev.to/taku25/balancing-physically-accurate-lighting-and-symbolic-vfx-in-unreal-engine-3fn3)).
Unreal adapts 3 stops/s up and 1 down by default ([Epic, automatic exposure](https://docs.unrealengine.com/4.27/RenderingAndGraphics/PostProcessEffects/AutomaticExposure)).
Battlefront II cut emissives because its bloom over-exaggerated them ([MP1st on Battlefront II's VFX](https://mp1st.com/news/star-wars-battlefront-2-visual-effects-community-transmission));
camera footage overexposes plumes because the meter exposes for the dark ground ([DCS forum, lighting for missiles](https://forum.dcs.world/topic/212516-correct-lighting-for-missilesabexplosions/), snippet):
don't copy video. For us (derived): sunlit ground of albedo 0.2 is about 6,400 cd/m²; peak frames at 10-30 × that clip
and bloom by day; compensate the sustained fireball only partly (emissive × EyeAdaptationInverse^0.5); a 0.1-0.3 s
flash moves exposure only 0.3-0.9 stop, so keep speed up ≤ 3.

**T12 Distance.** Cross-fade between tiers over about 0.3 s with ±15 % hysteresis in pixels (estimate; Halo 3 hid LOD
changes with a radial screen effect). Keep a smoke dot under 6 px by day for large charges: a 50 m cloud at 20 km is
about 4 px on our camera, a grey speck with phone-wire alpha; "nothing by day under 6 px" hides real smoke. DCS's
fixed spotting dots drew complaints both ways (1 px; then about 16 px "bricks"); players asked for the true angular
size ([DCS improved dot rendering](https://www.digitalcombatsimulator.com/en/files/3330454); [DCS spotting feedback](https://forum.dcs.world/topic/335334-new-spotting-is-great-new-spotting-is-overcooked-feedback-thread/page/55)).

**T13 Don't use Unreal's lit translucency.** Its lighting volume is 64³ over 15-50 m by default
(`r.TranslucencyLightingVolumeInnerDistance` 1500, outer 5000 uu), and doubling it costs 8×
([Epic, lit translucency](https://dev.epicgames.com/documentation/en-us/unreal-engine/lit-translucency-in-unreal-engine)).
Our effects sit hundreds of metres to tens of kilometres away: keep smoke unlit and shaded by our own parameters (T1, T9).

## 2. Ground burst by charge class

Weapons: Mk 82 (87-89 kg Tritonal, 241 kg; [Wikipedia](https://en.wikipedia.org/wiki/Mark_82_bomb), 4), FAB-250
(99-100 kg HE; [Wikipedia](https://en.wikipedia.org/wiki/FAB-250), 4), FAB-500 (201 kg HE in the series infobox; one
claim 150 kg; the 1930s FAB-500sv 235 kg TNT; [Wikipedia](https://en.wikipedia.org/wiki/FAB-500), 4), Mk 84 (429 kg
Tritonal; Wikipedia, 4). A FAB-500 holds about half a Mk 84's explosive.

### 2.1 The 100 kg class (Mk 82, FAB-250, OFAB-250): target against time

| t | What | Size | Colour | Opacity | Basis |
|---|---|---|---|---|---|
| 0-1 ms | white flash | — | #FFFFFF core | additive, clipped | sourced timing (Piehler; Gordon) |
| to 50 ms | fireball at its largest | 12-14 m | #FFC88C → #FF8100 (3500 → 1800 K) | emissive | derived (Mori; Gordon) |
| 0.05-0.3 s | fireball swallowed by soot and ejecta; one or two frames of white-yellow, then orange glimpsed through dust | | | | estimate |
| 0-0.3 s | refraction ring at 340-400 m/s; Wilson shell (R₀ 16 m, 0.2 s) only if RH ≥ ~70 % | to 100 m | white shell | 0.2-0.4 | derived (§0.4) |
| 0.05-2 s | V-shaped ejecta curtain; clods with dust tails, most upward in a ~45° cone, clumped | 26-37 m high at 1 s | soil #9C8466 darkened by soot | 1.0 | sourced shape (Graettinger; realtimevfx); cone from cEssex (design) |
| 0.2-3 s | low dust skirt racing outward, then drifting | radius 50-100 m on dry ground | #9C8466 | 0.6 → 0.3 | estimate |
| to 0.9-1.2 s | the orange afterglow dies (mostly hidden) | | #FF8100 → #FF5800 | | sourced (Radiant Brass III) |
| 1-60 s | column rising as t^½ (§0.3) | 45-65 m at 3 s, 83-118 m at 10 s, 202-289 m at 60 s; 53 m wide | grey-brown, dark low (#4A4540), lighter dust high (#8C8984 tinted by soil) | 0.95 low, 0.7 high | derived |
| 1-10 min | drifts and shears downwind, thins | widening with the wind | greyer as dust settles | 0.7 → 0 | estimate |
| minutes-hours | crater 3-11 m, scorch, ejecta rays to 2-4 crater radii | §0.5 | dark bowl #2E2B28, raised lip, soil rays | decal | derived; estimate |

FAB (TNT) smoke is black-grey soot mixed with soil; Mk 82 (Tritonal) a slightly whiter flash with grey-black smoke
(estimate).

**Distance.** 1 km: fireball 19 px for 50 ms, column 71-102 px tall by 3 s: full. 5 km: fireball 4 px, column at 10 s
26-37 × 14 px, at 60 s 64-91 px tall: impostor; the boom comes 15 s later. 15 km: the fireball a 1-2 px flash; the
column at 10 s 9-12 × 5 px, at 60 s 21-30 px tall, at 10 % contrast with 25 km visibility: a pale silhouette that our
32 m tier size (3.4 px) hides by day. **Night:** a point light peaking in 50 ms and decaying over ~1 s, lighting the
column's underside and the ground; at 15 km a flicker on the horizon; the column is unseen except where lit.

**Technique.** Glow: a spiky flash texture for 1-2 frames, bright per T11 (not a big glow sprite: cEssex). Fire: 4-8
spiky sprites round the edge so the core isn't a sphere, `heat` on the Gross curve through the §0.1 ramp, hidden
behind ejecta by 0.3 s (T5). Smoke: the column as T8, lit (T1), eroded (T3), soft (T2); the dust skirt as low,
wide sprites that sink and linger. Debris: clods in a clumped 45° cone, each with a short smoke puff trail; fade by
erosion like mud. Ring: the dust skirt's edge and a faint refraction-like ring (we have no distortion material: a
thin additive #F3EDE1 at 0.2-0.3, 0.3 s). Light: radius 2-3 × the fireball for the glow on the smoke (T9) and
`light_radius_per_fireball_m` for the ground. Decals: crater and scorch from §0.5; a fading ground shadow under
the column (War Thunder lets thick smoke cast shadows: [Dagor 6.5](https://devtrackers.gg/war-thunder/p/93601d35-development-dagor-engine-6-5-new-graphic-features), design).
Impostor: judge the tier by the column's height, not the 32 m puff (T12).

**Now:** at 100 kg, 4-8 fire sprites (`fire_sprites_full` 8, fewer in a crowd) swallowed by soot within 0.3 s; 8
ejecta jets (`ejecta_jets`) in a V falling back by about 2 s; the column as `column_members` 10 puffs (6-18 by the
charge) rooted in the ground and rising as t^½ to `column_scale` 0.85 × Church's top, graded dark low to light high,
each living `column_life_s` 240 × 0.8-1.2 s, with two faint stems; a skirt of 16 puffs out to 75 m for 40 s. Beyond
the full tier one fireball, the column as four stacked puffs and the skirt. The column's life sits inside the
target's 3-10 min at its low end.

### 2.2 Small charges on the ground (0.02-25 kg): rockets, submunitions, shells

| Charge | Fireball (Mori) | Crater D (k 1.7-6) | Column top (Church) | Puff (estimate) |
|---|---|---|---|---|
| 23 mm, 0.019 kg | 0.9 m | 0.2-0.6 m | (not reached) | 1-2 m dust spurt, 2-5 s |
| S-5, 0.25 kg; BLU-97, 0.29 kg | 2.0 m | 0.4-1.6 m | | 3-5 m, 5-10 s |
| S-8 or PTAB, 0.45-0.9 kg | 2.4-2.9 m | 0.5-2.3 m | | 4-8 m, 10-20 s |
| S-24, 23.5 kg | 7.8 m | 2.0-6.9 m | 145-205 m | as §2.1 scaled |

Below a few kilograms there is no column worth the name: a dust spurt, a flash and a puff that drifts low. DCS
players complained small rockets explode like heavy bombs ([DCS forum, more variation in explosion effects](https://forum.dcs.world/topic/169553-more-variation-in-new-explosion-effects/), design).
At 1 km a 4 m puff is 6 px (impostor), at 5 km 1.3 px (gone by day, a flicker at night). Technique: the `Small` style:
one Glow flash, 1-3 Smoke sprites of soil colour, a few Debris; no light under about 1 kg except at night (estimate).

### 2.3 The 200-500 kg class (FAB-500, Mk 84) and larger

* Fireball 15 m (FAB-500) and 19 m (Mk 84) by Mori; ours 13 and 17 m. Wilson shell R₀ 27 m, about 0.4 s (Mk 84). Column:
  §0.3 table, 300-430 m at about 90 s for the Mk 84. Crater 4-14 m (FAB-500), 5-19 m (Mk 84); delay-fuzed Mk 84
  15 × 11 m deep (Wikipedia, weak); FAB-500 4 m deep, "radius" 12 m (AiF). Fragments lethal to 120 m (Mk 84,
  Wikipedia): sparks and dust spurts over 100-300 m (estimate). Vegetation down to 21 m, leaves stripped to 45 m.
* Distance: a 430 m column is 45 px tall at 15 km, 135 px at 5 km: a landmark for the rest of the engagement.
* Technique as §2.1, more column members (6-8), and a second, slower wave of dust from the fallout (estimate).

### 2.4 Incendiaries (not in `weapons.json`; for when they come)

Napalm: a "black cloud of burning petrol" expanding to 46-91 m across, burning intensely a few seconds above
1000 °C ([Science for Peace, *Napalm*, 1952](https://archive.org/details/napalmwhatisnapa00unse), 4); NP1 burns at
900-1,300 °C, its remains 3-15 min ([Bristol student page](https://www.chm.bris.ac.uk/~paulmay/webprojects2001/wright/napalmbomb.htm), 4, weak).
Mk 77: 750 lb with about 110 gal of gel; Mod 1 500 lb with 75 gal ([Wikipedia](https://en.wikipedia.org/wiki/Mark_77_bomb), 4).
ZB-500 with "Ognesmes", 2,750-16,250 m² ([IPMS kit review](https://reviews.ipmsusa.org/review/soviet-zb-500-napalm-bombs), 4, weak).
The tumbling tank splashes a 3-5:1 pattern along its path; a rolling orange fireball (#FF8100-#FFA64A) under a thick
rising black cap; ground fires for minutes (estimate). The one effect where a big film fireball is right: it is fuel.

## 3. Airburst of a SAM warhead

| Missile | Warhead (sourced) | `charge_kg` (W) | Fireball (Mori) | Puff (estimate) |
|---|---|---|---|---|
| 9M33 (Osa) | 15 kg (ru_wiki, mis; 13 kg on weaponsystems.net, 5) | 6 (estimate) | 5 m, ~0.2 s | 10-20 m |
| 3M9 (Kub) | | 25 | 8 m | 15-30 m |
| V-601 (S-125) | | 33 | 9 m | 20-35 m |
| V-750/755 (S-75) | about 130 kg HE, a fan-shaped burst ahead of the missile, kill radius about 200 ft ([military-historian blog](https://military-historian.squarespace.com/blog/2020/7/9/tools-of-war-the-sa-2-guideline), 5); 196 kg warhead (commonly published) | 100 | 12 m, ~0.6 s | 30-60 m |

* An SA-2 "blew up ahead of me in a huge puff of orange smoke" ([Flight Journal, March 2021](https://www.pressreader.com/usa/flight-journal/20210301/281535113653921), 2, snippet).
  An F-105 hit by a proximity burst trailed fire and smoke, 14 February 1968 ([NMUSAF photo](https://commons.wikimedia.org/wiki/File:F-105_hit_by_SA-2_over_Vietnam.jpg), 1);
  premature SAM bursts over Hai Duong, 1 July 1967 ([US Navy photo 363-02412](https://archives-spec.unl.edu/keever/item/16423), 1, snippet).
  The orange may be NO₂ from the missile's nitric-acid oxidiser: unverified. No primary caption gives a SAM burst's
  colour.
* At altitude the puff grows by (P₀/P)^(1/3) (Sachs scaling, derived): × 1.23 at 5 km, × 1.57 at 10 km.

| t | What | Size | Colour | Opacity |
|---|---|---|---|---|
| 0-30 ms | flash, 1-2 frames | 2-3 × fireball | #FFF4E0 | additive |
| to 50 ms | fireball | 5 m (9M33) to 12 m (V-750) | #FFC88C → #FF8100 | emissive |
| 0-0.3 s | fragment sparks in a cone ahead of the missile along its velocity | 50-100 m reach | #FFDAA0 | additive |
| 0.1-1 s | puff grows to size, irregular, not a sphere | table × Sachs | 9M33: #3A3734; V-750: first 2-5 s rust #8A5E40 (estimate, NOx unverified), then #3A3734 | 0.9 |
| 1 s-2 min | drifts with the wind aloft, thins and greys, keeps rising a little | ×2-3 | → #6E6A64 | 0.9 → 0 |
| then | the target's own effects if hit (§6, §11) | | | |

**Distance.** A 9M33 burst at 5 km slant: fireball 1.6 px, a 15 m puff 4.7 px, a grey speck by day (show it, T12);
at 1 km 24 px (impostor); at 15 km 1.6 px. The TV sight, at about 49 ×, shows it full: this is what the operators
watch. A V-750's 45 m puff at 15 km is 4.7 px. **Night:** a red-orange flash; the puff only where the flash lights it
(T9) and then lost.

**Technique.** Glow flash and 2-4 Fire sprites; 3-5 Smoke sprites of uneven size for a non-spherical burst whose
smoke keeps rising from inertia and convection (IL-2 Korea, which also gained 65 → 125 fps in its B-29 scene by moving
from static to animated sprites: [dev blog 83](https://media.il2-korea.com/news/dd_83), design); Spark cone forward;
erosion fade; a light at night only. **Now:** the `Warhead` style: 3 fire sprites (1 beyond the full tier), 3-5
uneven puffs (2 beyond) of #353332 greying to #6E6A65 and widening about 2.5 times, hanging `air_puff_s` 120 ×
0.75-1.25 s, grown by Sachs's (P₀/P)^(1/3); `air_sparks` 30 in a 35° cone reaching 75 m ahead along the missile's way.

## 4. Flak bursts by calibre

| Calibre (mount) | Fill and charge | Fuze, where it bursts | Puff colour | Flash (Mori) | Puff (estimate) | Hangs (estimate) |
|---|---|---|---|---|---|---|
| 23 mm (ZU-23-2, ZSU-23-4) | 0.019 kg (`munitions.json`); fill unverified, probably A-IX-2 | contact, self-destruct 5.6-11.2 s (`wiki_guns`); arms at 100 m (`yugoimport_ammo`, 1) | light grey-white #C9C6C0 (inferred) | 0.9 m | 1-2 m | 10-30 s |
| 37 mm (generic AAA) OR-167 | 35 g net ([JMU CISR ordnance guide](https://www.jmu.edu/cisr/research/iraq-oig/03-projectile.pdf), 4) | self-destruct 9-12 s (`munitions.json`) | "white puffballs" (DCS forum quoting Vietnam) | 1.1 m | 2-3 m | 10-30 s |
| 57 mm (S-60) OR-281 | 0.153 kg A-IX-2 ([bulletpicker](https://www.bulletpicker.com/cartridge_-57mm-he-t_-uop-281_.html), 4) | self-destruct 13-17 s, arms at 250 m (`yugoimport_ammo`) | white, "cotton-like" (same) | 1.7 m | 3-5 m | 20-60 s |
| 85/100 mm (KS-19) OF-412 | TNT ([Fenix Insight METIS](https://metis.fenixinsight.com/munition/projectile/53-of-412), 5) | VM-30, VM-45 mechanical time and radio fuzes, set from the PUAZO-6 director; 12.7 km with time, 15 km with radio fuze ([ru.wikipedia, КС-19](https://ru.wikipedia.org/wiki/КС-19), 4) | black clouds (DCS forum); TNT soot | about 3 m | 8-15 m | 1-3 min |
| 130 mm (KS-30) | 3.5 kg (`munitions.json`) | time, 3-35 s | black #2E2B28 | 4.4 m | 12-20 m | 1-3 min |

* WWII heavy flak as the analogue: the 88 mm's "characteristic black cloud" ([NMUSAF, Flak 36](https://www.nationalmuseum.af.mil/Visit/Museum-Exhibits/Fact-Sheets/Display/Article/196233), 4);
  black when far, a blue centre closer, a red core very close ([398th BG, Bohn on coloured flak](https://398th.org/History/Veterans/History/Bohn_ColoredFlak.html), 2);
  "black balls of smoke" that "gradually … expand and look grayish as it is blown away", new puffs replacing old in a
  barrage ([486th BG, J. Albanese's letter](https://486th.org/Photos/Letters/FLAK.htm), 2); "a towering column of gray
  smoke with a small ring at the top" ([398th BG, Veenschoten diary](https://m.398th.org/History/Diaries/Veenschoten/Veen_450221.html), 2, snippet);
  puffs stayed hanging ([We Are The Mighty](https://www.wearethemighty.com/articles/what-happened-to-all-the-flak-fired-at-aircraft-during-world-war-ii.md), 4);
  they start about plane-sized and expand several times ([CombatAce](https://combatace.com/forums/topic/52051-i-was-wondering), [DCS](https://forum.dcs.world/topic/168323-minor-visual-fx-tweak-suggestion-to-flak-bursts), design).
* Against time (estimate): a red-orange flash for 1-2 frames; the puff to full size in 0.5-1 s; black puffs grey to
  #6E6A64 as they spread; light puffs thin to nothing.

**Distance.** A 2 m 23 mm puff is 3 px at 1 km and sub-pixel at 5 km: what is seen of 23 mm fire far off is the
tracers (§14), not the puffs. A 57 mm puff is 6 px at 1 km. A 100 mm puff is 16 px at 1 km, 3 px at 5 km, 1 px at
15 km: a barrage at 5 km is a field of black dots that stays (T12). **Night:** only the red-orange flashes.

**Technique.** One Glow flash and 1-3 Smoke sprites per burst (2-4 for heavy, slightly elongated, with a thin
"ring" sprite at the top for the 88 mm look); one impostor sprite each beyond 1 km; erosion; colour by calibre.
A barrage of hundreds of puffs takes the sprite budget by priority (persistent smoke kept, §17).
**Now:** `flak_by_calibre` gives each calibre its puff, its hang and its fill: 23 mm 1.5 m and 37 mm 2.5 m for 20 s,
57 mm 4 m for 40 s, light grey #A2A09B; 85 mm 10 m, 100 mm 11 m and 130 mm 16 m for 120 s, black #2E2B28 greying to
#6E6A65 (each × 0.8-1.2 in time, × Sachs's scale aloft); 2-3 puffs at the full tier, one beyond, the black ones
widening about 3.5 times as they hang. A self-destructing round's puff is its calibre's row. The puffs sit at the low
end of the table's sizes; the heavy guns' 2 min is inside the 1-3 min.

## 5. A cluster carpet

| Weapon | Contents | Release, footprint, burn | Basis |
|---|---|---|---|
| RBK-250-275 AO-1SCh | 150 AO-1, 1.2 kg each, 38 g HE; 5 rings of 30 | opens no lower than 400 m | [Bulgarian MoD 2013, demilitarisation of cluster munitions](https://www.clusterconvention.org/files/resources/Annex-B-ENG-Outline-technical-data.pdf) (1) |
| RBK-250 PTAB-2.5M | 42 shaped charges, 2.8 kg, 0.45 kg HE each | | same |
| RBK-500 AO-2.5RT | 60, 2.8 kg, 0.55 kg HE each | | same |
| RBK-250 / RBK-500 ZAB-2.5 | 48 incendiaries (RBK-250) | 3,900-28,400 m² (RBK-250), 20,000-80,000 m² (RBK-500); each burns 2-9 min | same |
| CBU-87 | 202 BLU-97/B, 3.41 lb | spin 0-2,500 rpm; opens at 300-3,000 ft; 70 × 70 to 150 × 450 ft; about 800 × 400 ft from medium-high altitude; at 500 rpm 120 × 200 ft, bomblets about 9 ft apart | [HRW 1999](https://hrw.org/reports/1999/nato2/nato995-04.htm) (4, `hrw_1999_cluster`); 20 × 20 to 120 × 240 m ([Wikipedia, CBU-87](https://en.wikipedia.org/wiki/CBU-87_Combined_Effects_Munition), 4) |

US slang "firecracker" or "popcorn", for the many small explosions ([Wikipedia, Cluster munition](https://en.wikipedia.org/wiki/Cluster_munition), 4).

| t (from first impact) | What | Size | Colour | Opacity |
|---|---|---|---|---|
| before | the canister opens: a small puff and fragments of the case at 300-900 m (estimate) | 2-4 m | #6E6A64 | 0.6 |
| 0-1.5 s | a rippling carpet of 40-200 white-orange flashes across the ellipse | each 1 m (AO-1) to 2.4 m (PTAB), derived | #FFF4E0 → #FFA64A | additive |
| 0-2 s | each leaves a puff | 2-5 m | grey-brown, soil #9C8466 with soot | 0.8 |
| 2-60 s | the puffs merge into a low haze that drifts | 5-10 m high over the footprint | #A8A39A | 0.5 → 0 |
| BLU-97 | white sparks from its zirconium incendiary ring | 5-10 m | #FFF4E0 | additive |
| PTAB on armour | a spark spray and spall | | | |
| ZAB-2.5 | dozens of fierce white-orange points, each with a smoke wisp | burning 2-9 min | #FFC88C, smoke #3A3734 | |

**Distance.** A 120 × 240 m carpet is 190-380 px wide at 1 km, 38-75 px at 5 km, 13-25 px at 15 km: the haze reads
as a low smear at 15 km while each flash is sub-pixel; at night a twinkle of glints, and ZAB fires as points for minutes.
**Technique.** Per bomblet one Glow flash (cheap instances) and at full tier one Smoke sprite; at impostor a few wide
haze sprites over the ellipse plus glints; one ellipse decal, not one per bomblet (32 decals). Sound `S_FX_Cluster_Ripple`.
**Now:** the weapon's own count from `munitions.json` (its submunitions, at most `cluster_bursts_max` 200;
`cluster_bursts` 24 only for a dispenser that gives none) of `Small` bursts rippling over `cluster_spread_s` 1.5 s,
each `cluster_burst_kg` 0.4 unless the munition gives its charge, and a haze of `cluster_haze_sprites` 8, 8 m high,
#817665, for 60 s over the footprint. Each burst still draws its own puff; ZAB fires and BLU-97 sparks are not drawn.

## 6. An aircraft hit and exploding in the air

* Fuel fireball: D = 5.8·M^(1/3) m, lasting 0.45·M^(1/3) s, M the fuel burning in kg, valid under 30 t (CCPS, via
  [a BLEVE thesis](https://www.memoireonline.com/11/19/11226/m_tude-et-modelisation-des-effets-thermiques-de-bleve-sur-la-sphere-de-stockage-de-GPL-au-niveau-d32.html)
  and [JRC RAPID-N](https://rapidn.jrc.ec.europa.eu/property_estimator/158), textbook). Derived: 300 kg 39 m, 3.0 s;
  500 kg 46 m, 3.6 s; 1 t 58 m, 4.5 s; 3 t 84 m, 6.5 s.
* Fuel from a ruptured tank mists while the aircraft still moves; if it lights, the fireball stays attached to the
  aircraft ([US patent 5,983,945, wing tank liner](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/5983945), textbook).
  When it doesn't, the aircraft trails fire and smoke (NMUSAF F-105 photo, 1) or tumbles into the sea
  ([IWM film 1060016328, Falklands](https://www.iwm.org.uk/collections/item/object/1060016328), 4).

| t (from the hit) | What | Size | Colour | Opacity |
|---|---|---|---|---|
| 0-0.3 s | the warhead's burst (§3); sparks on the airframe | | | |
| 0.1-0.6 s | if fuel lights: the fireball grows, carried along the flight path and slowing in the air, stretched along the velocity | to 39-58 m (fighter's 0.3-1 t burning) | core #FFA64A-#FF8100 (2500-1800 K) under a black soot envelope #1E1C1A | emissive / 1.0 |
| to 3-4.5 s | it cooks off to orange and black | | → #FF5800, smoke #1E1C1A | 1.0 → 0.8 |
| 0.3-10 s | 10-40 pieces fall, the larger with smoke streamers | 0.5-3 m | burning #FF8D15, smoke #2E2B28 | |
| then | the wreck's fall with its fire and smoke (§11) | | | |

**Distance.** A 58 m fireball is 91 px at 1 km, 18 px at 5 km and 6 px at 15 km: a bright orange spot by day for
about 4 s. **Night:** it lights the clouds and the ground below and is seen to the horizon; the light pool's
brightest. **Technique.** Fire sprites (8x8 flipbook, the heat ramp slower than HE: hydrocarbon flame is cooler, its
soot dark) in one premultiplied draw with the black envelope (T5); stretched along the velocity; Debris with short
sprite trails (ribbons are 48 and kept for missiles, contrails and wrecks); a light. **Now:** the `Kill` burst is sized
by the fuel, `kill_fuel_kg` 500: a 46 m fireball glowing about 3.6 s (CCPS), 5 large fire sprites carried on at
0.97-1.0 of the aircraft's speed and stretched along it, 7 black soot puffs trailing them; `kill_pieces` 24 falling,
`kill_burning_pieces` 8 of them burning with smoke. The warhead's own burst (`kill_charge_kg` 40) comes first.

## 7. A cruise missile's warhead (`CruiseImpact`)

`munitions.json`: Kh-22M 900 kg, BGM-109C 300 kg, AGM-86C 400 kg (W). The research found no source for the warheads
(Tomahawk commonly about 450 kg, Kh-22 about 1 t, unverified).
* In the open: §2.3 scaled. Kh-22M: fireball 23 m, Wilson shell 33 m for 0.5 s if humid, column 355-507 m at about
  100 s, 121 m wide, crater 7-23 m, vegetation down to 28 m.
* On a building: blast inside is amplified and buildings fall mainly to the reflected and return pressure wave (Mori
  2020, textbook). The flash shows in the openings, dust and smoke jet out of windows, the roof lifts; masonry dust
  #B9AE9C, paler than soil; black smoke from contents and fuel follows (#2E2B28); debris rains 100-300 m out; a lower,
  wider, paler cloud than in a field, and fire for hours (estimate).
* Distance: a 400 m column is 42 px tall at 15 km. At night the fire lights the town's smoke for hours.
* Technique: as §2 with masonry colours, Debris thrown wider and flatter, window jets as Smoke sprites aimed outward
  (estimate), a long-lived fire (`UpdateFires`) and its smoke column.

## 8. An ARM's burst near a radar

* AGM-45 Shrike: 66.6-67.5 kg warhead, proximity and impact fuze; from the AGM-45A-2 a white-phosphorus marker to show
  where it struck ([Wikipedia, AGM-45 Shrike](https://en.wikipedia.org/wiki/AGM-45_Shrike), 4); most landed more than
  20 m from the target, lethal radius 5-15 m ([GlobalSecurity, AGM-45](https://www.globalsecurity.org/military/systems/munitions/agm-45.htm), 5).
* AGM-88 HARM: about 66 kg warhead; WDU-21/B with about 25,000 steel fragments (later WDU-37/B, about 12,800 tungsten);
  FMU-111/B laser proximity fuze ([designation-systems, AGM-88](https://www.designation-systems.net/dusrm/m-88.html), 4, `parsch_dusrm`).
* White phosphorus: a shower of burning particles, each trailing smoke, the trails merging into dense white smoke;
  burns at about 800 °C; the hot smoke rises in columns, "pillaring" ([Wikipedia, White phosphorus munitions](https://en.wikipedia.org/wiki/White_phosphorus_munitions), 4).

| t | What | Size | Colour | Opacity |
|---|---|---|---|---|
| 0 | burst several metres above or ahead of the antenna: a sharp flash | fireball 8 m (W 24-25) | #FFF4E0 → #FF8100 | additive |
| 0-0.3 s | fragment dust spurts on the ground in a cone or ellipse stretched along the flight path; sparks on the radar's metal | 20-60 m long (estimate) | soil #9C8466; sparks #FFDAA0 | 0.8 |
| 0.1-20 s | a grey-black puff | 10-20 m | #3A3734 | 0.9 → 0 |
| Shrike only, 0-2 s | the white-phosphorus "octopus": 10-30 burning pieces arcing out, each trailing white smoke | 20-50 m out (estimate) | cores #FFF4E0, smoke #F2F1EC | 1.0 |
| Shrike, 2 s-5 min | the trails merge into a dense white cloud that rises in a column | 20-60 m wide, rising | #F2F1EC, shaded #C9C6C0 | 1.0 → 0 |

The player may stand 50-500 m from the radar the ARM is aimed at (at 300 m a 15 m puff is 79 px). **Technique.** Spark
cone and a ground ellipse of Smoke dust spurts; WP pieces as Spark particles with Smoke sprites spawned along their
paths (a sprite trail, not ribbons), then a white column (T8); a peppered fragment decal round the radar (estimate).

## 9. The SAM launch and its smoke trail, by missile

### 9.1 What every trail shares

* Primary smoke is solid particles in the exhaust (aluminised composite: Al₂O₃); secondary smoke is water condensing on
  HCl from perchlorate oxidisers and grows in cold, humid air; STANAG 6016 code AA is minimum smoke, AC reduced smoke
  ([US6364975B1](https://patents.google.com/patent/US6364975B1/en); [Bouma, TNO 2017](https://publications.tno.nl/publication/34637350/Cw38Vw/bouma-2017-modelling.pdf); textbook).
  AP composite exhaust is mostly water, CO₂, HCl and a metal oxide; the acid condenses moisture and enhances the trail
  ([Wikipedia, APCP](https://en.wikipedia.org/wiki/Ammonium_perchlorate_composite_propellant), 4). Double-base motors
  are relatively smoke-free, though their inhibitor can smoke (patents via search). So tie a trail's opacity to its
  propellant **and** the weather: the same motor leaves a denser, longer trail in cold, humid air (derived).
* The trail starts about as wide as the turbulent wake, set by the vehicle's widest part, usually the fin span; narrow
  trails photograph poorly whatever their density ([NASA TN D-2009, 1963](https://ntrs.nasa.gov/api/citations/19630013801/downloads/19630013801.pdf), textbook).
  Smoke follows the wind well (the same report measured wind profiles by its drift); winds at 11.6 km reached
  79-131 m/s at Wallops ([NASA TN D-6861, 1972](https://ntrs.nasa.gov/api/citations/19720021986/downloads/19720021986.pdf), textbook):
  each height band drifts with its own wind, and shear turns a straight climb into a staircase within tens of seconds,
  then breaks it into puffs (derived).
* Spread: no rocket rate found; contrails spread 2.3 m/s horizontally and 0.3 m/s vertically over 60-3600 s, 18-140 m
  per minute ([Schumann et al. 2017](https://acp.copernicus.org/articles/17/403/2017/), textbook); low trails in a
  convective day 1-3 m/s (estimate). Skywriting at about 10,000 ft lasts 5-20 min, up to an hour in calm air
  ([Plane & Pilot](https://planeandpilotmag.com/article/plane-facts-skywriting), 4, weak).
* The flame: a non-aluminised double-base motor is "mostly smokeless" with a bright orange afterburning flame that
  starts some distance aft, lit at the shock diamonds (a DTIC report seen only in a search summary, textbook, snippet).
  Aluminised composite: a white-yellow flame bright enough to see by day as a point (estimate). By day the flame lights
  only a few metres of trail (DCS players: the motor over-lights the trail, [DCS forum](https://forum.dcs.world/topic/212516-correct-lighting-for-missilesabexplosions/), design).
* Players want 30-60 s of missile smoke, not about 10 s, never cut by view distance ([DCS forum](https://forum.dcs.world/topic/124971-missilecontrails-smoke-disappear-too-fast), design, snippet;
  [War Thunder forum, about 1 min](https://forum.warthunder.com/t/realistic-smoke-trail-time/240804), design); the
  trail must not vanish with its missile ([DCS forum](https://forum.dcs.world/topic/352523-multiple-missiles-smoke-trail-instantly-disappears-upon-missile-destruction), design, snippet).
  DCS: low-smoke motors leave a dense contrail below −25 °C ([DCS newsletter 28 Feb 2025](https://www.amvi.it/news5618-dcs-world-newsletter-28022025.html), design).

### 9.2 By missile

| Missile | Motor and timing (sourced) | Smoke (sourced) | Draw (estimate unless said) |
|---|---|---|---|
| **9M33M2/M3 (ours)** | leaves at a constant 27°; the motor lights 0.3 s after the turbogenerator; dual-thrust, 13.2-19.2 s; flight 24-27 s; not burst by 25-28 s it is steered up and destroys itself (`voenizdat_osa_1991`, 1) | no document; footage: it leaves "in a trail of smoke and fire" ([Medium, Anatomy of an Engagement](https://medium.com/@yeast_vs_west/2-anatomy-of-an-engagement-17c664dee7a3), 4) | start width 0.65 m (wing span), white-grey #E8E6E0, 0.8 near the missile; tail tracers for the TV sight (`motor.tracer`) |
| **3M9 (Kub)** | integral rocket-ramjet: the solid start charge inside the afterburning chamber, no booster to drop; to Mach 1.5 on the start, 2.8 on the ramjet; sustainer up to 20 s on about 67 kg of fuel ([nfront.org.ua](https://nfront.org.ua/ru/zrk-kub-istoriya-harakterystyky-ta-bojove-zastosuvannya/); [visnyk-nanu](https://visnyk-nanu.org.ua/zrk-kub-harakterystyky-zona-urazhennya-raketa-3m9-sklad/); 4) | not found; a crew's split-S across the leader's vapour trail ([weaponsandwarfare](https://weaponsandwarfare.com/?p=4309), 4) | a dense start trail for 3-5 s, then a thin grey line |
| **V-750/755 (S-75)** | booster PRD-18 2-4 s, liquid sustainer 25-60 s on AK-20F/K and TG-02 ([ausairpower](https://ausairpower.net/APA-S-75-Volkhov.html), 5); booster off after about 6 s (USAF Museum, snippet); 4-5 s booster then about 22 s (snippet); PRD-58 2.5-4 s with summer and winter nozzle inserts (snippet) | "flying telephone poles"; launches "spewed a lot of … smoke" and left "a visible smoke trail" (popular histories, snippets) | thick white booster smoke 2.2 m wide (booster fin span) and a large launch dust cloud, then a much thinner sustainer trail; a brownish NOx tint at sustainer start unverified |
| **V-601 (S-125)** | solid booster, solid sustainer ([ru.wikipedia, С-125](https://ru.wikipedia.org/wiki/С-125), 4); "engine time 24 s" uncertain | none found | as the S-75's booster phase, then a solid sustainer's trail |
| **9M31/9M37 (Strela-1, -10)** | 9M37M 40 kg, mean speed 517 m/s ([missilery.info](https://en.missilery.info/node/13034), 4) | none found | |
| **9K32 Strela-2 and its line** | ejection charge burns out in the tube; leaves at 32 m/s; sustainer lights 0.3 s later at about 5.5 m; burns 0.5 s and 2.0 s; 430 m/s (500 m/s 2M); spins about 20 rev/s; self-destructs at 14-17 s ([Wikipedia](https://en.wikipedia.org/wiki/9K32_Strela-2), 4) | the seeker's dead zone causes repeated corrections, a wobble | a corkscrew: pitch 21.5 m (derived: 430 / 20), radius 0.3-1.5 m (estimate), with a slower lazy S; Igla's single-channel control switches 4 times a revolution, 10.2 g ([War Thunder forum citing the 9K38 manual, Akulov et al. 2011](https://forum.warthunder.com/t/manpads-missiles-and-overload-the-technical-details/61158), 1 as cited, not read) |
| **Stinger** | coasts 8.53 m before the flight motor lights ([FM 44-18-1](https://webarchive.library.unt.edu/web/20130213212857/http:/armypubs.army.mil/doctrine/DR_pubs/dr_a/pdf/fm44_18_1.pdf), 1, snippet); Mach 2.2 in about 2 s ([designation-systems](https://designation-systems.net/dusrm/m-92.html), 4); self-destructs 15-19 s (snippet) | exhaust can envelop the firing position (IMFDB talk page, design) | as Strela-2 |
| **HAWK** | M22E8 dual-thrust 25-32 s; M112: about 5 s boost, 21 s sustain; Mach 2.5 ([Wikipedia](https://en.wikipedia.org/wiki/MIM-23_Hawk); [designation-systems](https://designation-systems.net/dusrm/m-23.html); 4) | none found | |
| **Chaparral** | Mk 50: 12.2 kN for 4.7 s; the M121 "smokeless" motor built from 1980 ([designation-systems](https://designation-systems.net/dusrm/m-72.html), 4) | M121 greatly reduced firing smoke ([Wikipedia, M48](https://en.wikipedia.org/wiki/M48_Chaparral), 4); game forums disagree on how smokeless | 1985: minimal smoke |
| **Rapier** | two-stage solid | the tracker followed four tail flares (Wikipedia, 4); "hardly any visible smoke trail" ([DCS forum](https://forum.dcs.world/topic/199961-how-to-defend-rapier), design) | four flares, a faint trail |
| **Roland** | dual-thrust solid ([designation-systems](https://designation-systems.net/dusrm/m-115.html), 4) | not found | |

### 9.3 Our launch, seen from the vehicle

| t (from leaving the launcher) | What | Size | Colour | Opacity |
|---|---|---|---|---|
| 0-0.3 s | flash at the launcher and the first exhaust | 6 m (`launch_flash_m`) | #FFF4E0 | additive |
| 0-3 s | the launch cloud boils round the launcher and the vehicle, low and wide | 7 m puffs growing 2.5 × (`launch_cloud_*`) | #E7E6E3 lit, undersides #A8A39A | 0.85 |
| 3-30 s | the cloud drifts downwind, thins | to 20-40 m | | 0.85 → 0 |
| 0-15 s | the trail climbs away at 27° and gathers onto the target line; plume and tail tracers | 0.65 m at the missile, widening 0.3-3 m/s | #E8E6E0 | 0.8 → 0.5 |
| 15-60 s | the trail drifts and shears by height, breaks into puffs | 10-60 m wide | sunlit #F2F1EC, shadow #A8A39A | 0.5 → 0 |
| 25-28 s | a missed missile pulls up and destroys itself (§3) | | | |

A 0.65-2.5 m trail is 1-4 px at 1 km and sub-pixel beyond: draw it with phone-wire AA (T10), white against blue sky,
dark against a bright overcast. **Night:** the flame and tracers are a moving light; the trail shows only for a few
metres behind the flame and wherever a flash lights it. **Technique.** Smoke sprites for the launch cloud (soft, T2,
with a camera fade: the player stands in it); puffs near the missile, a ribbon behind (T10), its width from the fin
span, per-vertex sun lighting; Plume and Glow for the flame, the flame's light reaching a few metres by day; the
trail owned by the effect for its `smoke_s`. **Now:** the trail starts at the munition's fin span × its smoke's share
(`trail_span_share_*` 1.0 dense, 0.8 reduced, 0.6 minimal; `trail_width_*_m` 2.5/1.5/0.8 only for a munition without
fins), widens at `trail_spread_mps` 1.5 (± `trail_spread_jitter` 0.5), thins to 0.625 of its opacity by 15 s and
erodes from 0.35 of its life; the launch cloud is 12 puffs of 7 m growing 2.5 × over 20 s. At night a burning motor
is a glint 6 px wide that keeps half its opacity (`glint_keep`), and it lights its own smoke for `night_lit_trail_m`
25 m behind it. Seen end-on from the vehicle, the trail's near puffs scatter wider (1.4 against 0.5 seen side on) and
overlap less, so the tube seen along its length is a loose cloud, not a round blob.

## 10. Rockets

| Rocket | Facts |
|---|---|
| S-5M | motor 0.55-0.92 s; up to 725 m/s; 882 mm, 3.86 kg, fin span 232 mm; effective 800-1800 m ([ru.wikipedia, С-5](https://ru.wikipedia.org/wiki/С-5_(НАР)), 4) |
| S-8 | motor 0.69 s (base model; the M and KOM longer, not given); 692-700 m/s; double-base powders ВИК-2Д, БН-К, БНК-Р, БНК-1БСТ ([ru.wikipedia, С-8](https://ru.wikipedia.org/wiki/С-8), 4) |
| Hydra 70, Mk 66 Mod 4 | 1.07 s at 77 °F, 1413 lbf mean thrust ([NDIA brief](https://ndia.dtic.mil/wp-content/uploads/2003/gun/hawl.pdf); [GD-OTS datasheet](https://www.gd-ots.com/wp-content/uploads/2017/11/Hydra-70.pdf); 1); launch signature and trail "significantly reduced" against the Mk 4/Mk 40 ([GlobalSecurity](https://www.globalsecurity.org/military/systems/munitions/hydra-70.htm), 5), yet a baseline Mk 66 showed "a very high signature … throughout the one second burn" ([US 6054521](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/6054521), textbook) |

* A trail lasts only while the motor burns: S-5 about 250-450 m, S-8 about 250 m, Hydra about 350-450 m from the
  launcher (derived); the rocket flies on unseen, no tracer. A pod salvo is a ripple of parallel grey-white streaks that
  start at the pod and end in mid-air (estimate). Start width at the fin span (0.23 m for the S-5, NASA rule).
* Distance: a 0.6 m trail is 1 px at 1 km; at 5 km only a pod salvo's spreading smoke shows.
* **Now:** `munitions.json` gives the S-8 "reduced" smoke, from its double-base powders (§9.1: double-base motors
  relatively smoke-free, their inhibitor smoking); the S-5 keeps "dense" (its powder not found). Its `smoke_s` (20-30 s for rockets) is what the trail lingers; `rocket_smoke_s` 8 is only the
  fallback.

## 11. A falling, burning aircraft

* USAF captions: F-105 60-0418 "trailing fire and smoke just after interception by an SA-2", about 12 miles south of
  Hanoi, 14 February 1968; another F-105D trailed smoke after a near SA-2 burst (NMUSAF, 1, snippet). Leaking fuel that
  meets an ignition source burns within, on or under the wing ([SKYbrary, Wing fire](https://skybrary.aero/articles/wing-fire), 4).
  Burning fuel will not propagate forward against an airstream: the fire streams aft ([NTL ROSAP report](https://rosap.ntl.bts.gov/view/dot/81240/dot_81240_DS1.pdf), textbook, snippet).
* Target (estimate): a torch 1-3 fuselage lengths long (15-50 m for a fighter), attached at the wing root or tail,
  yellow-orange #FFA64A with a #FF8100 tail; black smoke starting about one wingspan wide (~10 m, NASA rule), opaque for
  the first 100-200 m (#1E1C1A, 1.0), then greying (#5A554F) and widening at §9.1's rates; in a spin a helix; at
  break-up 2-4 smaller trails and a cloud of flutterers each with a short puff; on the ground an impact burst (§2 with
  the fuel's fireball, §6) and a black column for 15 min or more.
* Distance: a 10 m trail is 16 px at 1 km, 3 px at 5 km, 1 px at 15 km (phone-wire); the torch shows to 15 km by day as
  a spot. **Night:** a moving light seen to the horizon, lighting its own smoke.
* **Technique.** Fire sprites at the head (rate by tier), the trail as a wreck ribbon, lit per vertex; light pool.
  **Now:** wreck smoke 10 m wide (`smoke_width_m`, a fighter's wingspan), spreading 2.5 m/s, opaque #2C2B2A, lasting
  50 s; a torch 40 × 6 m streaming aft with fire sprites at `fire_rate` 35 a second; on the ground a `ground_charge_kg`
  150 burst, fire for 300 s and a black column for 900 s. At night the torch is `torch_night_share` 0.02 of the fire's
  brightness (it holds its #FFA64A-#FF8100 under the night's exposure) and its light, `night_light_lumens` 4e6, sits at
  the torch's middle, off the wreck's model. Break-up into several trails and the spin's helix are not drawn.

## 12. Contrails

* Schmidt-Appleman: about 90 % of observed contrails formed at −60 to −40 °C, 7.4-18.7 km up; e.g. −50.4 °C at FL350
  with RHi 39 % ([Schumann et al. 2017](https://acp.copernicus.org/articles/17/403/2017/), textbook).
* The gap: 25-35 m behind the engines at threshold (DLR ATTAS, 1994); each young plume about 2 m across; in 1942 about
  3 m across at 20 m (0.26 s) behind the tail, the gap growing as the air warms toward the threshold (both via Schumann
  2017); visible at 1-2 wingspans ([Bouhafid & Bonne 2025](https://egusphere.copernicus.org/preprints/2025/egusphere-2025-1865/egusphere-2025-1865.pdf), textbook).

| Age | Width and form | Basis |
|---|---|---|
| onset | 2-3 m per engine; 2 or 4 lines | ATTAS; aufm Kampe 1942 (via Schumann) |
| a few s | the plumes roll up into the two wingtip vortices: 2 lines | Bouhafid & Bonne |
| 19-49 s | B747: two lines each 12 m wide, 43 m apart: about 55 m wide, 35 m deep | Baumann 1993 (via Schumann) |
| tens of s | Crow instability bends the pair into a chain of vortex rings ("puffs") at about 8.6 wingspans (about 400 m for a 58.8 m span); lasts a few minutes | Bouhafid & Bonne |
| 50 s | wake 125 m deep (vortices sink about 2.5 m/s) | via Schumann |
| under 200 s | 80-150 m | Freudenthaler (via Schumann) |
| 1-60 min | +18-140 m a minute | Freudenthaler 1995; Jäger 1998 |
| 1-4 min (A380) | primary wake 270 m below flight level, secondary above it: a faint upper curtain over a brighter lower one | [HAL 01895872](https://hal.archives-ouvertes.fr/hal-01895872), textbook |
| 23 min | about 1500 m | Schumann 2017 |
| 2.25 / 3.75 h | 6 / 10 km; up to 40 km | Schumann 2017 |

* In ice-subsaturated air short contrails of twin-engine aircraft vanish, mostly more than 4 s behind; four-engine
  contrails last to vortex breakdown, about 2 min (Sussmann & Gierens 2001, abstract via search, textbook). In
  ice-supersaturated air they become contrail cirrus lasting hours, observed to 11.5 h (Schumann 2017).
* Aerodynamic trails (wingtip vortices, vapour on the wing at high g, humid take-offs) are "usually much fainter"
  ([AMS Glossary](https://glossary.ametsoc.org/wiki/Condensation_trail), textbook; [atoptics](https://atoptics.co.uk/fz1135.htm), 4).
* Colour (estimate): sunlit #F9FAFD, its shaded side and underside blue-grey #C9CDD6, warm at sunrise and sunset; at
  night unseen except by moonlight.
* Distance at 10 km: a 2 m young line is 0.3 px (a hairline by contrast only: phone-wire floor); a 55 m pair 8.6 px;
  150 m 24 px; the 30 m gap 4.7 px.
* **Technique.** One ribbon at every range (T10), never culled; 2 or 4 lines merging to 2 within seconds; the pair
  sinking; Crow puffs by modulating width and opacity along U at 8.6 spans after 30-60 s; per-vertex lighting.
  **Now:** `gap_m` 30 and `width_m` 2.5 match; the pair sinks at `sink_mps` 2.5 to 270 m and Crow puffs come at 8.6
  spans between 30 and 60 s; `max_s` 600 caps contrail cirrus that lasts hours in supersaturated air, its oldest part
  eroding away (from 0.7 of its life) rather than cut.

## 13. Afterburners

* The first Mach disk sits at x ≈ 0.67·D₀·√(P₀/P_ambient) behind the nozzle; it glows where excess fuel lights behind
  the normal shock ([Wikipedia, Shock diamond](https://en.wikipedia.org/wiki/Shock_diamond), 4). With a 0.9 m nozzle
  and a pressure ratio of about 3, about 1.0 m aft; four to six diamonds make a 5-8 m plume at night (derived).
* Blue from CH and C₂ chemiluminescence, orange from glowing soot: "the blue shows at night, the orange dominates by
  day"; the diamonds most vivid low down; Su-27 and MiG-29 plumes look orange, F/A-18, F100 and NK-25 blue
  ([MiGFlug blog, citing NASA Glenn and The Aviationist](https://migflug.com/afterburner/?p=24902932), 4). Very
  visible at night, nearly invisible by day ([DCS forum](https://forum.dcs.world/topic/153190-realism-of-afterburner-glow-seen-at-large-distances), design).

| | Length | Colour | Opacity |
|---|---|---|---|
| Day | 2-4 m (estimate) | pale orange #FFC489 | 0.3-0.5 |
| Night, Soviet engines | 5-8 m (derived) | orange #FF8D15, diamonds #FFE8C0 | additive |
| Night, US engines | 5-8 m | blue-violet core #7A8CFF (estimate), diamonds #FFE8C0 | additive |

A 6 m plume is 9 px at 1 km, 2 px at 5 km; at night a glint to 15 km and beyond; by day not seen past about 2 km
(estimate). **Technique.** Plume sprites with 4-6 bright knots for the diamonds at night; Glow; a light at night.
**Now:** by day 3 m, brightness 6, #FFC459; at night 6.5 m, brightness 0.8, #FF8D15 (#7A8CFF for `blue_models`, the
F-15 and F-16; both authored with their weaker channels lowered, since the night's exposure puts the flame at the top
of the tone curve, where the strong channel clips), one 1.25 m wide flame per nozzle (`T_FX_Afterburner`, a column
tapering aft), with 5 diamonds #FFE8C0 0.4 m from 1.0 m aft every 0.9 m; blended by the sun's height; a light at night
of 3e5 lm 8 m aft of the nozzle (not lighting the basic model as a block). Under 6 px long the flame is its glow alone,
at night a glint 6 px wide (`glint_share` 6), seen to 15 km and beyond.

## 14. AAA tracers and gun fire

| Gun | Rate | Bursts used | Rounds and belts | Tracer, self-destruct |
|---|---|---|---|---|
| ZSU-23-4 | at least 3400 rounds/min from four barrels | 3-5 rounds a barrel against fast targets, 5-10 against slow, 50 against ground; 2-3 s pause ([ru.wikipedia, ЗСУ-23-4](https://ru.wikipedia.org/wiki/ЗСУ-23-4), 4, from the manuals); "2-3 per barrel" (en.wiki) | 2000 rounds (3000 in the M2); belts 3 OFZT/OFZ to 1 BZT, every 40th with a de-coppering agent; barrel life 3000 rounds | as the ZU-23 |
| ZU-23-2 | 2000/min cyclic, 400 practical a barrel | no manual norm found (use the Shilka's) | belts of 50; barrel changed after 100 rounds; 4 OFZ to 1 BZT ([ru.wikipedia](https://ru.wikipedia.org/wiki/ЗУ-23-2); [Wikipedia](https://en.wikipedia.org/wiki/ZU-23-2), 4, `wiki_guns`) | 970 ± 10 m/s; arms at 100 m; self-destructs 5.6-11.2 s; tracer at least 3.5 s (`yugoimport_ammo`, 1); 5 s (en.wiki) |
| S-60 (57 mm) | 105-120 cyclic, 70 sustained ([Wikipedia](https://en.wikipedia.org/wiki/AZP_S-60), 4) | 4-round clips; 50-60 continuous heat the barrel to 400-450 °C ([ruwiki, С-60](https://ru.ruwiki.ru/wiki/С-60), 4, snippet) | UOR-281 HE-T, 1000 m/s | tracer at least 10 s; self-destruct 13-17 s; arms at 250 m (`yugoimport_ammo`) |
| KS-19 (100 mm) | 14-15/min | single rounds by battery | time and radio fuzes from the PUAZO-6 | only AP rounds trace |
| M163 Vulcan | 3000/min in bursts of 10, 30, 60, 100, or 1000/min continuous ([Wikipedia](https://en.wikipedia.org/wiki/M163_VADS), 4; FM 44-5 and TM 9-2350-300-20-1 as cited in [a DCS forum post](https://forum.dcs.world/topic/149831-vulcan-sight-problems-and-combat-use), 1 through a forum) | the gunner holds the trigger to the burst's end | M246 HEI-T-SD; M56 HEI untraced | tracer burnout sets off the self-destruct at 3-7 s, about 1800 m |

* Bursts (derived): a Shilka burst is 12-20 rounds in 0.2-0.35 s, a stream 200-350 m long, then 2-3 s of nothing;
  Vulcan 10 rounds in 0.2 s, 100 in 2 s; a ZU-23's 50-round belt empties in 1.5 s at the cyclic rate.
* Tracer reach (derived): 23 mm tracers end about 2-3 km out; 57 mm about 5-6 km; M246 about 1.8 km, where it bursts.
* Tracer colour, sources disagree, both recorded: NATO red (strontium), "Russian and Chinese tracer ammunition generates
  green light using barium salts" ([Wikipedia, Tracer ammunition](https://en.wikipedia.org/wiki/Tracer_ammunition), 4,
  mainly small arms). For Soviet AA guns: red (a CombatACE poster), green (its administrator, from Vietnam accounts of
  "basketball sized green balls"), white or light yellow from memory ([CombatACE](https://combatace.com/forums/topic/101037-aaa-tracer-color), design);
  all Soviet tracers crimson, yellower in bright daylight ([DCS forum, Russian](https://forum.dcs.world/topic/23320-cveta-trasserov/), design, snippet).
  **Used:** Soviet 23 and 57 mm orange-red, crimson at night (#FF4A24) and yellow-white by day (#FFE8C0); Soviet small
  arms green (#8AFF5A); Western red (#FF4A24) (estimate between the sources).
* The hosepipe at night (derived): each round flies its own ballistic path; the gun traverses while firing, so the
  stream bends like water from a hose; from 3 km the stream rises and arcs over; the Shilka's pauses break it into dashes.
* Self-destruct puffs: §4.
* Muzzle flash and gun smoke: no source. Each barrel a 1-2 m yellow-orange flash (#FFC88C), grey-white smoke
  (#C9C6C0) drifting downwind from the mount (estimate). Don't blind the player with gun smoke ([War Thunder devblog comments](https://warthunder.com/en/devblog/current/6984?page=6), design).
* Distance: a tracer (0.35 m) is sub-pixel at every range: a 2 px floor with alpha by true width (T10), HDR emissive so
  bloom carries it, faded at burnout; length = speed × max(1/60 s, frame time), 16 m at 970 m/s (5 px at 5 km). DCS's
  ZU-23 tracers were nearly invisible by day and its 7.92 mm ones "huge laser beams" ([DCS forum](https://forum.dcs.world/topic/216905-792mm-tracers-are-huge-laser-beams), design, snippet); tracers grow and brighten as they near the viewer
  (an ED tester, same).
* **Now:** bursts of `burst_min_s` 0.2 to `burst_max_s` 0.35 with 2-3 s pauses (the Shilka's drill), never fewer
  than `burst_min_rounds` (4 for the S-60's clips and 37 mm); every 4th round traced on the ZSU-23-4 and every 5th on
  the ZU-23 (`traced_every`); tracers #FFE8C0 by day (authored as #FFCE61, `tracer_day_colour`, so the additive
  streak plus the sky reads #FFE8C0) to #FF4A24 at night by the sun's height, length by
  max(1/60 s, the frame), brightness `tracer_day_brightness` 80 by day and `tracer_brightness` 35 at night;
  `tracer_colour` #FFB36C is the muzzle flash's. A predicted flak burst scatters `flak_jitter_s` 0.12 s in time
  (±120 m along the line).

## 15. The night

* What carries the picture: flashes (§0.6), the light pool (T9), fires, tracers, motor flames and afterburners. Smoke,
  trails and contrails are unseen except where a flash, a fire or the moon lights them.
* A burst's light: a 1-2 frame peak, 0.1-0.4 s white to orange, then about 1 s of orange (§0.1); radius 2-3 × the
  fireball on the smoke, wider on the ground; it lights aircraft and cloud bases ([DCS Flak Flash Script](https://www.digitalcombatsimulator.com/en/files/3346486), design).
  Close flak: "blinding red explosions" (320th BG, 2). At 15 km: a flicker on the horizon and the cloud base.
* Under 6 px a burst is a glint (`glint_px` 3): right. A burning wreck, a ZAB field or a town on fire glows for minutes
  to hours, reddening the smoke above it (#FF5800 at 1300 K is 2 % of a sunlit wall: a dull red).
* Tracers crimson; afterburners 5-8 m with diamonds; missile flames a moving point with a few metres of lit trail.
* Exposure: EV −8 (starlight) to 18; let the eye adapt slowly (down 1 stop/s) so a flash doesn't pump the scene (T11).

## 16. Free assets worth fetching

Checked 31 October-1 November 2026 by HTTP HEAD, nothing downloaded. **README section 6 admits CC0 and public domain
only**; the CC BY items below need the project owner's word before they come in (with author, source link, licence
link and "changed" in the provenance list under `art/fx/`).

### 16.1 Textures

| Asset | Licence | URL | Size | Replaces |
|---|---|---|---|---|
| Unity Labs flipbooks and image sequences (28 elements: Explosion00-02, Explosion02HD, -nofire and -light variants, FireBall01-04, Flame02/03, SmallFlame, WispySmoke01-03, Cloud01-04, DiscSmoke01, CandleSmoke01; each as flipbooks, linear HDR EXR and TGA sequences; Houdini renders, 2016) | CC0, [Unity blog, T. Iché, 28 Nov 2016](https://unity.com/blog/engine-platform/free-vfx-image-sequences-flipbooks) | `https://unity3d.com/files/labs/downloads/vfx/assets01/<Name>/<Name>-flipbooks.zip` (or `-sequence-exr.zip`, `-sequence-tga.zip`) | flipbooks 2.7-15.3 MB (Explosion01 3.00, Explosion02HD 15.3, FireBall01 5.89, WispySmoke01 4.65, DiscSmoke01 8.51); EXR 23-99 MB | `T_FX_Fireball` (Explosion01/02HD), smoke after the fireball (-nofire), `T_FX_SmokeAtlas` and wreck columns (WispySmoke, DiscSmoke), the Fire layer (FireBall, Flame), the heat channel (-temperature); the EXRs let `fx_textures.py` repack into our atlas. Resolution and frame count not stated. They are images, not volumes: T1 needs a volume or our own field |
| JangaFX free VDB volumes (Ground Explosion 661 MB/200 fr; Mid-Air 1.14 GB/126; Aerial 6.6 GB/120; Gasoline 751 MB/131; Dust Shockwave 5.0 GB/155; Grenade Dust Impact 3.45 GB/200; Smoke Plume 2.8 GB/167; Chimney Smoke 1.2 GB/250; Fire Plume 5.5 GB/250; Simple Fire 64.7 MB/101; Camp Fire 59 MB/200; Cloud Pack 104 MB) | CC0, [JangaFX free VDB page](https://jangafx.com/software/embergen/download/free-vdb-animations) | MediaFire links on that page (no login; one answered 403, probably a bot check) | above | rendered in Blender to our layers, with the six T1 directions: ground burst, SAM airburst, fuel fireball (Gasoline), the dust ring, gun impacts, wreck columns and fires. Commit only the renders and a provenance line |
| Kenney Smoke Particles (77 PNG: black smoke, explosion, flash, white puff) | CC0, [kenney.nl](https://kenney.nl/assets/smoke-particles) | [smokeParticleAssets.zip](https://opengameart.org/sites/default/files/smokeParticleAssets.zip) | 13.2 MB | hand-drawn puffs: far flak, distant puffs, the impostor; not close fireballs |
| Kenney Particle Pack (80 sprites, 512 px) | CC0, [kenney.nl](https://kenney.nl/assets/particle-pack) | [kenney_particlePack.zip](https://opengameart.org/sites/default/files/kenney_particlePack.zip) | 9.8 MB | `T_FX_Glow`, `T_FX_Spark`, `T_FX_Streak` |
| Kenney explosion atlas by TheJosh (3×3) | CC0, [OpenGameArt](https://opengameart.org/content/explosion-particles-sprite-atlas) | [explosion_atlas_512x512.png](https://opengameart.org/sites/default/files/explosion_atlas_512x512.png) | 267 KB (full 1.9 MB) | impostor tests |
| Poly Haven `burned_ground_01` (to 8K), `rubble`, `concrete_debris` | CC0, [licence](https://polyhaven.com/license); download by hand (no scraping) | e.g. [1K diffuse](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/burned_ground_01/burned_ground_01_diff_1k.jpg) | 1.03 MB (1K), 4.1 MB (2K), 14.3 MB (4K) | `T_FX_Scorch` (masked radially); crater floors, building hits |
| ambientCG `Ground107`, `Ground111` | CC0, [licence](https://docs.ambientcg.com/license/) | `https://ambientcg.com/get?file=Ground111_1K-JPG.zip` | | crater decals (no scorch or smoke assets there) |

Not to commit: Unreal Starter Content (the EULA PDF of May 2023 lets it ship cooked in a game but does not list it among
what may be shared outside a product: each developer adds it from an install); Fab and Unity Asset Store packs, Pixabay,
Mixkit (no redistribution); CGHEVEN (CC0 claimed, unverified); OpenGameArt's low-resolution effects.

### 16.2 Sounds

| Recording | Licence | URL | Format, size | Replaces |
|---|---|---|---|---|
| ZU-23 firing, «Чистое небо» (Mil.ru, 4 Aug 2016) | CC BY 4.0, credit "Mil.ru" (Commons `Template:Mil.ru`, VRT ticket #2024021510008013) | [Commons file](https://upload.wikimedia.org/wikipedia/commons/b/b3/%C2%AB%D0%A7%D0%B8%D1%81%D1%82%D0%BE%D0%B5_%D0%BD%D0%B5%D0%B1%D0%BE%C2%BB_-_%D0%9F%D0%BE%D1%80%D0%B0%D0%B6%D0%B5%D0%BD%D0%B8%D0%B5_%D0%BC%D0%B8%D1%88%D0%B5%D0%BD%D0%B5%D0%B9_%D0%B8%D0%B7_%D0%B7%D0%B5%D0%BD%D0%B8%D1%82%D0%BD%D0%BE%D0%B9_%D1%83%D1%81%D1%82%D0%B0%D0%BD%D0%BE%D0%B2%D0%BA%D0%B8_%D0%97%D0%A3-23_%D1%83%D1%87%D0%B0%D1%81%D1%82%D0%BD%D0%B8%D0%BA%D0%B0%D0%BC%D0%B8_%D0%BA%D0%BE%D0%BD%D0%BA%D1%83%D1%80%D1%81%D0%B0_%D0%B2%D0%BE%D0%B9%D1%81%D0%BA%D0%BE%D0%B2%D0%BE%D0%B9_%D0%9F%D0%92%D0%9E.ogv) | 44.1 kHz stereo, 25.7 s, 37.7 MB video | `S_FX_Gun_23mm_Twin`, `_Quad`: the most needed recording (check for music or voice-over) |
| Air-defence firing in Buryatia, and Pantsir-S1 (Mil.ru, 2017) | CC BY 4.0, "Mil.ru" | Commons (Buryatia: 54.3 s, 52.4 MB; Pantsir: 48.6 s, 28.2 MB) | 44.1 kHz | mixed firing, missile launches |
| BRRRRRRT.wav, A-10 GAU-8 at Hardwood Range, 2011 | CC BY 4.0, nicStage | [file](https://upload.wikimedia.org/wikipedia/commons/3/39/BRRRRRRT.wav) | WAV 48 kHz/24-bit, 11.0 s, 3.16 MB | `S_FX_Gun_30mm_Gatling`; checks the crack's timing |
| Saab J35 Draken passes and taxi (Avon turbojet, afterburner) | CC BY 4.0, Work With Sounds / T. Nilsson | [passes](https://upload.wikimedia.org/wikipedia/commons/5/5c/WWS_SaabJ35DDraken8211passingtwotimes.ogg); [taxi](https://upload.wikimedia.org/wikipedia/commons/4/49/WWS_SaabJ35DDrakenstartengineandtaxiing.ogg) | Ogg, 55.6 s 0.72 MB; 137 s 1.67 MB | `S_FX_Jet_Roar_Turbojet`, `S_FX_Afterburner`, `S_FX_Jet_Whine_Turbojet` |
| CF-18 manoeuvres (F404, afterburner) | CC BY 3.0, lonemonk | [file](https://upload.wikimedia.org/wikipedia/commons/1/1c/Canadian_CF_18_maneuvers.wav) | WAV 44.1/16, 253.9 s, 44.8 MB | `S_FX_Jet_Roar_Turbofan`, `_Whine_Turbofan` |
| Boeing flyby (InspectorJ) | CC BY 4.0 | [file](https://upload.wikimedia.org/wikipedia/commons/3/3a/428086_inspectorj_airplane-boeing-flyby-right-to-left-a.wav) | WAV 44.1/24, 47.9 s, 12.7 MB | the heavy jet profile |
| An-24, Moscow 2008 | public domain, Grachev | [file](https://upload.wikimedia.org/wikipedia/commons/5/59/Sound_of_AN-24_airplane.ogg) | Ogg, 46.8 s, 0.49 MB | `S_FX_Prop_Turboprop` (the An-26's AI-24 family) |
| Chinook over Greenwich | CC0, JoBrodie | [file](https://upload.wikimedia.org/wikipedia/commons/3/3c/Chinook_helicopter_flying_over_Greenwich_in_London.ogg) | Ogg mono, 88.9 s, 0.75 MB | `S_FX_Rotor_Helicopter` (blade slap; tandem rotors, not a Mi-8) |
| Helicopter over a lake / a quiet neighbourhood | public domain, ezwa via PDSounds | [file](https://upload.wikimedia.org/wikipedia/commons/c/c6/Helicopter_over_a_lake.ogg) | 28 s, 22 s | far helicopter layer |
| Stinger firings: USMC, 2d LAAD, Avenger at Balikatan 25 | public domain (US government) | [Commons page](https://commons.wikimedia.org/wiki/File:FIM-92_Stinger_firing.webm); [Avenger file](https://upload.wikimedia.org/wikipedia/commons/6/66/Balikatan_25-_Avenger_Air_Defense_System_Fires_Stinger_Missiles_During_Balikatan_25_%28960159%29.webm) | 60.4 s 71 MB; 58.1 s 80 MB; 15.6 s 4.4 MB | `S_FX_Missile_Launch` (ejection pop, then boost) |
| A-10 exercise, Nevada; F-35A first GAU-22/A use | public domain, USAF | [A-10 file](https://upload.wikimedia.org/wikipedia/commons/2/22/A-10_Exercise_USAF.ogv) | 44.1 s 41 MB; 33.8 s 2.7 MB | gun runs at range, `S_FX_Gun_20mm_Gatling` |
| NPS thunder, military jet, helicopter | public domain, credit NPS ([gallery](https://www.nps.gov/subjects/sound/gallery.htm)) | [thunder](https://www.nps.gov/nps-audiovideo/legacy/mp3/nri/avElement/nri-Thunder.mp3); [jet](https://www.nps.gov/nps-audiovideo/legacy/mp3/nri/avElement/nri-JetYOSE.mp3) | mp3, 739 KB, 89.5 KB | the rolling tail of `S_FX_Explosion_*_Far`; far jet |
| NASA STS-131, SDO, STS-26 launches; SOFIA (747SP) take-off | public domain in the US ([media guidelines](https://www.nasa.gov/nasa-brand-center/images-and-media/)) | [STS-131](https://www.nasa.gov/wp-content/uploads/2015/01/590189main_ringtone_131_launchNats.mp3) and its neighbours | mp3, 264-420 KB | far layer of `S_FX_Rocket_Motor_Large`; heavy jet |
| Freesound CC0: F-15 (324370), F-16 (343747), F/A-18 (135495/6), jet flyby (189446), rocket launch (182794), field gun (702245) | CC0 | freesound.org (originals need a login: download them yourself) | WAV/FLAC 1.2-21.5 MB | jets, `S_FX_Gun_57mm_Single` |

Most needed (from the assets research): the 23 mm guns, then the jets, the helicopter, the turboprop, the 30 mm, the
missile launch and a thunder tail for far blasts. Keep the near blasts, the sonic booms, the bullet crack, fire and
debris synthesized: the Friedlander pulse and the N-wave are the physics. Recordings carry Doppler, absorption and
ground reflections that `USimFxAudio` adds again: take loops from steady stretches, use whole flybys only as far
one-shots. Not usable: BBC Sound Effects (no commercial use), DVIDS downloads (login), Sonniss, Internet Archive items
with only an uploader's licence (one "CC0" item is a 1995 commercial CD-ROM), CC BY-SA files, SoundBible (weak
provenance), Freesound 347795, 149966, 372086 (synthesised or not what they say) and 262436 (doubtful ownership, real
combat).

## 17. Players' do and don't

**Do**
* Make launch and impact smoke last: 30-60 s for missile smoke, minutes for bomb columns (DCS, War Thunder, BMS forums).
* Keep trails and contrails independent of draw distance, zoom and FOV ([DCS, contrails and FOV](https://forum.dcs.world/topic/349667-contrails-going-invisible-depending-on-fov); [DCS, smoke pops](https://forum.dcs.world/topic/177255-smoke-and-con-trails-pop-in-and-out); design).
* Scale explosions by charge; give them dust, time and a ground shadow ([DCS](https://forum.dcs.world/topic/169553-more-variation-in-new-explosion-effects/); Dagor 6.5).
* Let flashes light the ground, aircraft and clouds at night (DCS flak flash script).
* Drift smoke with the wind, but not too fast sideways ([War Thunder, the way the wind blows](https://warthunder.com/en/news/7455-development-the-way-the-wind-blows-en); IL-2 forum).
* Use variants per explosion (BMS, War Thunder).
* Prefer animated sprites to brute-force counts (IL-2 Korea).

**Don't**
* Let sprites clip into the ground (IL-2 forum).
* Let a trail vanish when its missile dies (DCS).
* Let contrails pop with zoom or view direction, or draw them blocky and segmented ([MSFS forum](https://forums.flightsimulator.com/t/contrails-look-very-blocky/563262), design).
* Hide launches with thin, transparent smoke, or over-light the trail with the motor by day (DCS).
* Draw tracers as invisible specks by day or laser beams up close (DCS, IL-2).
* Use huge fixed-size dots (DCS spotting).
* Blind the player with gun smoke, flashes or launch smoke ([War Thunder devblog comments](https://warthunder.com/en/devblog/current/6984?page=6); [tank explosions thread](https://forum.warthunder.com/t/id-like-it-if-tank-explosions-were-remade-or-improved/136902); design).
* Let the frame rate collapse inside smoke (Falcon BMS).
* Let the budget delete persistent smoke for new effects ([Blastcore changelog](https://steamcommunity.com/sharedfiles/filedetails/changelog/2257686620), design): budget by priority, persistent launch and impact smoke first.
* Over-bloom bright effects (Battlefront II).

## 18. Gaps

Not found, to look for next: measured dust-column height against time for 100-500 kg (the §0.3 table is derived);
DTIC cratering and dust reports (ADA083346, ADA053465, ADA025991, ADA164394) and Church's report itself (blocked); a
primary photo caption giving a SAM burst's colour; the fills of the 23 mm and 100 mm AA rounds and of the 9M33's
warhead; the Kh-22 and Tomahawk warheads; base-surge speed and extent; the 9M33's smoke; Kub's start charge's burn;
burn times of the S-125, Strela-1 and Strela-10; HAWK, Roland and Rapier smoke; a measured missile-trail spread rate; a
MANPADS corkscrew's radius; a burning aircraft's trail density and soot; MiG and F-4 afterburner lengths; the ZU-23-2's
burst norms in a manual; tracer colour in a Soviet ammunition document; muzzle flash sizes; the Unity flipbooks'
resolution and frame layout. Best next sources: Soviet НСД and firing manuals for the ZU-23 and S-60, FM 44-5 in
full, the DTIC rocket-plume reports.
