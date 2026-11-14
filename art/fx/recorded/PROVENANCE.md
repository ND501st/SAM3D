# Recorded and drawn effects assets: provenance

Everything under `art/fx/recorded/` comes from a public source under **CC0** or in the **public domain** (US federal
government works). Nothing here is CC BY, CC BY-SA, NC or "royalty free", and nothing came from behind a login.
All files were fetched on **1 November 2026**; each licence was read on the licence page itself that day (quoted
below). The SHA-256 is that of the file as downloaded (the zip, mp3, ogg or jpg), before any change.

Credit is not required by any of these licences; the credits below are given as good practice ("NASA", "NPS",
"Kenney.nl", "Unity Technologies / Thomas Iché", "Rob Tuytel / Poly Haven", the Commons authors).

US government works (NASA, NPS) are free of copyright in the United States (17 U.S.C. 105); NASA's guidelines add that
its insignia and logos are not public domain and that its material must not imply endorsement. None of these files
carries a logo.

Processing was done with Python 3.12 in isolated mode (Pillow, numpy, soundfile/libsndfile 1.2.2); nothing
downloaded was executed. No ffmpeg or other converter was used.

## 1. Textures (`textures/`)

All PNG, 8 bits per channel. Colour images are sRGB; the grey "data" maps (EmissionLog, Temperature, Roughness,
the Kenney masks) are linear (import with sRGB off).

### 1.1 Unity Labs VFX flipbooks (`textures/unity/`)

* Title: "Free VFX Image Sequences & Flipbooks" (28 elements rendered in Houdini)
* Author: Thomas Iché, Unity Technologies; blog post of 28 November 2016
* Source page: https://unity.com/blog/engine-platform/free-vfx-image-sequences-flipbooks
* Licence: CC0 1.0, https://creativecommons.org/publicdomain/zero/1.0/. The page: "Here are some image sequences we
  want to share with you under CC0 license."
* File URL pattern: `https://unity3d.com/files/labs/downloads/vfx/assets01/<Name>/<Name>-flipbooks.zip` (each zip
  holds the flipbook as an 8-bit TGA and as a linear half-float EXR; server Last-Modified 13 October 2022)

| Download | Bytes | SHA-256 |
|---|---|---|
| Explosion02HD-flipbooks.zip | 15 284 536 | e50413b9b7adaf2b85ee81c25fcf8e6ee9365e9912f2fa6b35875e25d5ddf3e6 |
| Explosion01-nofire-flipbooks.zip | 2 666 836 | ada1640da1421a9861740ff6377a84bfa5fd7774d093ae24851849059e623dd4 |
| FireBall01-flipbooks.zip | 5 891 208 | f60de3f2ce56f941d6f089ea4ea93e3ae51c63fac84c1aa0d6615022f8647010 |
| WispySmoke01-flipbooks.zip | 4 646 205 | f9bdc88948ff18a7d3e6332b68f2f2ff17e9a64c6feb821e05388f4f23dbcff0 |
| DiscSmoke01-flipbooks.zip | 8 506 874 | e749a28e436c592f35b40a1da47a761602b3323c7d34f72965e6b2b63c7a6723 |
| Cloud01-flipbooks.zip | 3 007 131 | 37d3e0b631c357f8704e99128ad6701346b96298041de80e09c05588fa778d4e |

What the flipbooks are (measured from the files):

* The TGA's colour is exactly `sRGB(clamp(EXR, 0, 1))` (mean error 0.002 on FireBall01): whatever is brighter than
  1.0 in the render is clipped to white in the 8-bit image.
* Alpha is **straight (not premultiplied)**: RGB is filled in under alpha 0 (both in the TGA and the EXR).
* Frames run left to right, top to bottom. 5x5 grids on 2048 or 1024 px have non-integer cells (409.6 or 204.8 px):
  use UV fractions (Unreal's SubUV does), not pixel cells.
* All EXRs are uncompressed half floats; Unity's tool wrote a wrong size (55 instead of 73) in the channel-list
  attribute of the four-channel EXRs, which strict EXR readers may reject.
* Lighting is baked into the images (light from the upper left); there is no normal, thinness or erosion channel.

| File | Size, grid, frames | Channels | What it shows, what it is good for | Changed |
|---|---|---|---|---|
| `T_FXR_Unity_Explosion02HD_5x5.png` | 2048x2048, 5x5, 25 | RGBA colour, straight alpha | A one-shot explosion: white-hot ball (about a third of the cell wide in frame 1) to an orange and black fireball to a black smoke cloud that keeps half the cell; the close fireball of a ground burst or a bomb | TGA to PNG, nothing else |
| `T_FXR_Unity_Explosion02HD_5x5_EmissionLog.png` | 2048x2048, 5x5, 25 | grey, linear | The same frames' brightness from the HDR EXR, log encoded: `L = 161^v - 1` (v = pixel/255, Rec.709 luminance of the linear EXR, clamped at 160; the EXR peaks at 141). Lets a material put back the HDR core the 8-bit colour clips, or drive a heat ramp | New, derived from the EXR |
| `T_FXR_Unity_Explosion01_nofire_5x5.png` | 1024x1024, 5x5, 25 | RGBA, grey (R=G=B), straight alpha | The same kind of blast without fire: dark grey billowing smoke that drifts sideways and thins to nothing by frame 25; the smoke after a fireball, a dust or smoke burst. EXR peaks at 0.80, so nothing is clipped | TGA to PNG |
| `T_FXR_Unity_FireBall01_8x8.png` | 1024x1024, 8x8, 64 | RGB colour, **no alpha** (additive on black) | A burning ball of fire, looping (last frame to first differs like one step); the burning wreck, a fuel fire, a fire layer in a fireball. The core is clipped white (the EXR reaches 45) | TGA to PNG |
| `T_FXR_Unity_FireBall01_8x8_Temperature.png` | 1024x1024, 8x8, 64 | grey, linear | The render's temperature pass (EXR only, R=G=B): `T = v * 37.5` (v = pixel/255). Our R "temperature" channel in all but name | New, from the EXR, divided by its maximum 37.5 |
| `T_FXR_Unity_WispySmoke01_8x8.png` | 1024x1024, 8x8, 64 | RGBA, grey, straight alpha | A thin, wispy grey puff, looping; light smoke, a smouldering wreck's wisps, the trail's thin parts | TGA to PNG |
| `T_FXR_Unity_DiscSmoke01_16x4.png` | 2048x1024, 16x4, 64 (128x256 cells) | RGBA, grey, straight alpha | A column of smoke rising from a disc-shaped source, nearly looping (last to first is 1.4 steps); wreck and fire columns. Its highlights are clipped (the EXR reaches 23) | TGA to PNG |
| `T_FXR_Unity_Cloud01_8x8.png` | 1024x1024, 8x8, 64 | RGBA, grey, straight alpha | A dense, rolling puff, looping; billowing smoke puffs: the SAM's booster smoke, a smoke screen, our smoke atlas | TGA to PNG |

### 1.2 Kenney Particle Pack (`textures/kenney/T_FXR_Kenney_<name>.png`, `T_FXR_Kenney_Puffs_4x4.png`)

* Title: "Particle Pack" (80 sprites, 512 px), by Kenney Vleugels (Kenney.nl)
* Source pages: https://kenney.nl/assets/particle-pack ("Creative Commons CC0", linked to the CC0 1.0 deed) and the
  same pack uploaded by Kenney to OpenGameArt, https://opengameart.org/content/particle-pack-80-sprites
  ("License(s): CC0")
* File URL: https://opengameart.org/sites/default/files/kenney_particlePack.zip (kenney.nl serves its own copy only
  through a scripted signed link, so the OpenGameArt copy uploaded by Kenney was used; server Last-Modified
  30 June 2018)
* Licence: CC0 1.0, https://creativecommons.org/publicdomain/zero/1.0/. The zip's `License.txt`: "You may use these
  assets in personal and commercial projects."
* Download: 9 760 286 bytes, SHA-256 163b839ba7f62f8a420da93a62beaa80013588d5d6b1eb30841e1a553635ad70

The sprites are white shapes on opaque black. Changed: each chosen sprite reduced to luminance, resized 512 to 256 px
(Lanczos) and written as RGBA with **R = G = B = A = the grey value** (our convention for glows: "grey in RGB, the
same value in A").

| File(s) | What it is good for |
|---|---|
| `circle_05` | A soft round glow: `T_FX_Glow` |
| `flare_01` | A point flare with a thin horizontal streak: the missile's motor seen far, a flare |
| `star_01`, `star_04`, `star_05`, `star_06`, `star_07`, `star_09` | Glints and four-point stars: `T_FX_Spark`, the burst's first-frame glint |
| `trace_01`, `trace_06`, `trace_07` | Smooth vertical streaks: tracers, `T_FX_Streak` (rotate 90 degrees) |
| `circle_02`, `circle_03`, `circle_04` | Thin bright rings: the shock ring, `T_FX_Ring` |
| `muzzle_01` to `muzzle_05` | Gun muzzle flashes (side view): the AAA guns |
| `scorch_01` to `scorch_03` | Star-shaped bursts: a burst's flash core, or the mask of a scorch decal |
| `dirt_01` to `dirt_03` | Splatter clusters: thrown earth and debris of a ground burst |
| `T_FXR_Kenney_Puffs_4x4.png` (1024x1024, 4x4 of 256 px) | 16 puff variants, row by row: `smoke_01` to `smoke_10`, `flame_01` to `flame_04`, `fire_01`, `fire_02`; random-variant smoke puffs |

Not taken: the magic, symbol, window, slash, scratch, twirl, light and electric-spark sprites, `flame_05`, `flame_06`,
the rotated copies, and the zip's Unity package and `.url` shortcut files (never opened).

### 1.3 Kenney Smoke Particles (`textures/kenney/T_FXR_Kenney_BlackSmoke_4x4.png`, `T_FXR_Kenney_WhitePuff_4x4.png`)

* Title: "Smoke Particles" (77 PNG in five sets), by Kenney Vleugels (Kenney.nl)
* Source pages: https://kenney.nl/assets/smoke-particles ("Creative Commons CC0") and
  https://opengameart.org/content/smoke-particle-assets (uploaded by Kenney, "License(s): CC0")
* File URL: https://opengameart.org/sites/default/files/smokeParticleAssets.zip (server Last-Modified 23 August 2014)
* Licence: CC0 1.0, https://creativecommons.org/publicdomain/zero/1.0/. The zip's `license.txt`: "You may use these
  graphics in personal and commercial projects."
* Download: 13 193 295 bytes, SHA-256 da9260580d0f6a522496d03783fc9f740f13faa70844b52fb8df90ea9b72cb7a

Changed: the first 16 of each set (`blackSmoke00`-`15`, `whitePuff00`-`15`; straight-alpha colour PNGs of 336-583 px)
fitted into 256 px cells of a 4x4 atlas (1024x1024, longest side 248 px, centred, Lanczos), and the colour under
transparent pixels filled from the neighbours (push-pull) so filtering shows no dark fringe. Alpha unchanged. Good for:
far, hand-drawn puffs (the black ones for flak bursts at range, the white ones for distant motor smoke), not close
fireballs. Not taken: the cartoon Explosion, Flash and Fart sets.

### 1.4 Poly Haven `burned_ground_01` (`textures/polyhaven/`)

* Title: "Burned Ground 01", by Rob Tuytel, Poly Haven
* Source page: https://polyhaven.com/a/burned_ground_01
* Licence: CC0, https://polyhaven.com/license: "Our assets are all licensed as CC0, which is effectively Public Domain"
  (the page adds that they may be redistributed). Three single files were fetched by their direct links; nothing was
  scraped.

| File | From (1K JPG) | Bytes | SHA-256 of the JPG | Changed |
|---|---|---|---|---|
| `T_FXR_BurnedGround01_Diffuse.png` (1024x1024 RGB, sRGB) | https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/burned_ground_01/burned_ground_01_diff_1k.jpg | 1 033 112 | be83310cad64d23539e628f14525af651a03646f5a1cc6029bd19a9446ea72f9 | JPG to PNG |
| `T_FXR_BurnedGround01_Roughness.png` (1024x1024 grey, linear) | `.../burned_ground_01_rough_1k.jpg` | 319 073 | 4dbbb69dc3391d5b98b4346ffe5f2b46340ad0307c3d1d7311b59407146753f5 | JPG to PNG, one channel |
| `T_FXR_BurnedGround01_Normal_DX.png` (1024x1024 RGB, linear) | `.../burned_ground_01_nor_gl_1k.jpg` | 366 727 | 86125af02b64b05b0079dd6fa08ffae11878a3f9f50326a265e94c7a64cba865 | JPG to PNG, green flipped (OpenGL to DirectX/Unreal) |

Burnt grass, pine needles and charred earth, tiling. Good for the scorch decal and crater floors.

## 2. Sounds (`sounds/`)

All WAV, 48 kHz, 16-bit, mono. Changes common to all: the channels averaged to mono (every stereo source here has
left and right correlated 0.98-1.00, so nothing is lost), the useful part cut, DC removed, resampled to 48 kHz
(FFT, where the source was 44.1 kHz), peak normalised to -1 dBFS, TPDF dither to 16 bits.

**Loops** loop as whole files: loop start = sample 0, loop end = the last sample. Each was cut from a steady
stretch `[start, end + crossfade)` of the source and the stretch after `end` crossfaded (equal power, the two ends'
correlation being low) into its first `crossfade` seconds, so the last sample runs on into the first.
**One-shots** are cut with the fades given.

### 2.1 NASA (public domain in the US)

* Source page: https://www.nasa.gov/historical-sounds/ (NASA's sounds page, credit "NASA"; no third-party credit shown)
* Licence: https://www.nasa.gov/nasa-brand-center/images-and-media/: NASA content, audio included, "generally are not
  subject to copyright in the United States"; acknowledge NASA as the source.

| File | Source (title on the page; file URL) | Download bytes, SHA-256 | Cut | Good for |
|---|---|---|---|---|
| `S_FXR_Rocket_Shuttle_Far_STS131_Loop.wav` (9.989 s, 479 468 samples, loop 0-479 467) | "Discovery / STS-131: Sound of Launch"; https://www.nasa.gov/wp-content/uploads/2015/01/590189main_ringtone_131_launchNats.mp3 (19.0 s, 44.1 kHz stereo) | 307 824; a08ebbedfd34d1d97c941831bd461f8790912cd2c33cabdb662a888e914d4ae7 | loop 2.000-11.989 s, 1.0 s crossfade, +8.97 dB | Far roar of a very large rocket: the far layer of `S_FX_Rocket_Motor_Large` |
| `S_FXR_Rocket_AtlasV_Far_SDO_Loop.wav` (9.966 s, 478 342 samples, loop 0-478 341) | "Atlas V: Launch" (the SDO launch); https://www.nasa.gov/wp-content/uploads/2015/01/590329main_ringtone_SDO_launchNats.mp3 (20.1 s) | 324 921; 726772ac193b3d1adfe1c638fd4fe3525511ad87b35a8172f42656df458577c3 | loop 3.000-12.966 s, 1.0 s crossfade, +12.8 dB | Far rocket roar with crackle; second variant for the same layer |
| `S_FXR_Rocket_StaticFire_SLS_Loop.wav` (9.953 s, 477 720 samples, loop 0-477 719) | "SLS Test Fire" (the page does not say which test or motor); https://www.nasa.gov/wp-content/uploads/2015/01/663784main_SLS_Audio_D.mp3 (23.6 s; a spoken countdown in the first 3 s, then 20 s of steady roar) | 191 094; 0b13815e4d2ccf4a1ae430c9bc169dcfc46182abab8c6cb0618fce89b5ccbe3e | loop 6.000-15.953 s, 1.0 s crossfade, -2.29 dB. The source is mastered hot: 0.13 % of samples in the stretch above 0.98, peaks to 1.13 | A steady, closer rocket roar: the body of `S_FX_Rocket_Motor_Large` |
| `S_FXR_Jet_Heavy_SOFIA_Takeoff_Loop.wav` (8.002 s, 384 114 samples, loop 0-384 113) | "SOFIA Takeoff Audio" (the 747SP); https://www.nasa.gov/wp-content/uploads/2015/01/sofia_takeoff_audio.mp3 (25.8 s) | 420 253; 1f4f49cedaa30eb742116f656f91091b35b9345fbc66ecdb0fbf0358578216cb | loop 12.000-20.002 s (after a whine sweep at 2-5 s), 1.0 s crossfade, +11.36 dB | The heavy jet profile: four turbofans at take-off power, `S_FX_Jet_Roar_Turbofan` for transports and bombers |

Fetched and not used: "Discovery / STS-26: Liftoff",
https://www.nasa.gov/wp-content/uploads/2015/01/640392main_STS-26_Liftoff.mp3 (264 320 bytes, SHA-256
7fa1c170d22ab05efb275c487a951e8c51661d9ddb35b08ba63b630db27d537b): launch commentary over the whole clip.

### 2.2 National Park Service (public domain)

* Source page: https://www.nps.gov/subjects/sound/gallery.htm (NPS Sound Gallery, Natural Sounds and Night Skies
  Division). Licence on that page: "The files are in the public domain and may be downloaded. Enjoy, and please give
  the appropriate credit." Credit: NPS.

| File | Source | Download bytes, SHA-256 | Cut | Good for |
|---|---|---|---|---|
| `S_FXR_Thunder_Roll_NPS.wav` (31.800 s, one-shot) | "Thunder" (https://www.nps.gov/subjects/sound/sounds-thunder.htm: "Sounds of rain and loud rolling thunder"); https://www.nps.gov/nps-audiovideo/legacy/mp3/nri/avElement/nri-Thunder.mp3 (46.0 s, mono) | 739 018; 54d6a7fc85b5c7b0c65dd36ff3c5f398d698c37e733bfc98a234a6724c2d0a49 | 2.2-34.0 s, fade in 0.05 s, fade out 5 s, -1.02 dB; two isolated rain ticks patched (source 20.750 s and 30.685 s: 6 ms replaced by the 6 ms before, 1 ms crossfades). The crack is at 1.08 s in the file, the roll fades over 30 s; rain hiss throughout | The rolling tail of `S_FX_Explosion_*_Far` (after our synthesized onset) |
| `S_FXR_Jet_Military_Far_NPS_Loop.wav` (3.608 s, 173 190 samples, loop 0-173 189) | "Jet" (https://www.nps.gov/subjects/sound/sounds-jet.htm: "Sound recording of military jet, Yosemite National Park"; type not given); https://www.nps.gov/nps-audiovideo/legacy/mp3/nri/avElement/nri-JetYOSE.mp3 (5.4 s, mono) | 89 510; b9d28e0aca2429d24ba2a6238f51fbd9c293297d2931f96eb0f523976194dbe8 | loop 0.800-4.408 s, 0.5 s crossfade, +0.39 dB | A military jet heard far off, overhead: the far jet layer |

The NPS helicopter page (https://www.nps.gov/subjects/sound/sounds-helicopter.htm) has no audio link; not fetched.

### 2.3 Wikimedia Commons (each licence read on the file's own page)

| File | Source | Licence (from the file page) | Download bytes, SHA-256 | Cut | Good for |
|---|---|---|---|---|---|
| `S_FXR_Prop_Turboprop_An24_Loop.wav` (3.547 s, 170 268 samples, loop 0-170 267) | "Sound of AN-24 airplane" by Grachev, recorded on a dictaphone in Moscow, 29 January 2008; https://commons.wikimedia.org/wiki/File:Sound_of_AN-24_airplane.ogg; file https://upload.wikimedia.org/wikipedia/commons/5/59/Sound_of_AN-24_airplane.ogg (Vorbis, 48 kHz, 46.8 s, bandwidth about 3 kHz) | Public domain: "This work has been released into the public domain by its author, Grachev, at the English Wikipedia project. This applies worldwide." | 491 365; 25b85c42b9d6cc27c40eb3dcfd61a5ac9f99b0d72d7fd520ab979322f937fe9c | loop 7.400-10.947 s (between a handling noise at 7.0 s and a tick at 11.1 s), 0.5 s crossfade, +11.32 dB | `S_FX_Prop_Turboprop`: the AI-24 family's propeller tones (the An-26 has the same engines) |
| `S_FXR_Prop_Turboprop_An24_Pass_Far.wav` (34.800 s, one-shot) | the same | the same | the same | 1.2-36.0 s, fade in 0.3 s, fade out 5 s, +5.99 dB; the aircraft passing and going away. Faint ticks of the recorder remain near source 7.0, 11.1, 17.9 and 21.7 s | A far turboprop pass, as a one-shot |
| `S_FXR_Rotor_Chinook_Loop.wav` (5.010 s, 240 488 samples, loop 0-240 487) | "Chinook helicopter flying over Greenwich in London" by JoBrodie, 31 May 2016, own work; https://commons.wikimedia.org/wiki/File:Chinook_helicopter_flying_over_Greenwich_in_London.ogg; file https://upload.wikimedia.org/wikipedia/commons/3/3c/Chinook_helicopter_flying_over_Greenwich_in_London.ogg (Vorbis mono, 44.1 kHz, 88.9 s) | CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/): "This file is made available under the Creative Commons CC0 1.0 Universal Public Domain Dedication." | 751 739; 88b7d21bd2583db7e293c2312b8169cd6ab1731f731be9f1474d1180bb9ccaf7 | loop 48.700-53.710 s, the length 52 blade-slap periods (measured 0.0961 s, 10.4 Hz), 0.5 s crossfade, +1.15 dB | `S_FX_Rotor_Helicopter`: blade slap overhead (tandem rotors, not a Mi-8: its rate and timbre differ) |
| `S_FXR_Rotor_Chinook_Pass_Far.wav` (41.000 s, one-shot) | the same | the same | the same | 37.0-78.0 s, fade in 2 s, fade out 6 s, +1.15 dB; two clicks patched (source 48.497 s and 62.702 s, as for the thunder). City noise under the approach | A far helicopter pass, as a one-shot |
| `S_FXR_Rotor_Heli_Far_Lake_Loop.wav` (8.001 s, 384 054 samples, loop 0-384 053) | "Helicopter over a lake" by ezwa, 12 November 2008, from PDSounds (http://www.pdsounds.org/sounds/helicopter_over_a_lake, no. 671), "Helicopter flying over lake, while on a boat (hence the voices)"; https://commons.wikimedia.org/wiki/File:Helicopter_over_a_lake.ogg; file https://upload.wikimedia.org/wikipedia/commons/c/c6/Helicopter_over_a_lake.ogg (28.0 s, mono 44.1 kHz) | Public domain: "This work has been released into the public domain by its author, ezwa. This applies worldwide." | 351 628; a235a94c053019bc1ed56994f3a49b01881768e44c320c3102dd4f100421c5a8 | The Ogg carries a Skeleton stream libsndfile cannot read: its pages were dropped and the Vorbis pages kept unchanged, then loop 4.000-12.001 s, 1.0 s crossfade, +13.1 dB. The page mentions voices; the stretch was chosen where a spectrogram shows no speech and no clicks (not checked by ear) | A distant helicopter layer with its turbine whine |

## 3. Left out, and why

* **Videos** (the USMC and 2d LAAD Stinger firings, the Avenger at Balikatan 25, the A-10 exercise in Nevada; all
  public domain): skipped. No `ffmpeg` is on the PATH, and the rule was to take audio out of a video only with a local
  ffmpeg. (PyAV 19.0.2, which bundles FFmpeg's libraries, is installed in `out/voice_gen/.venv`; it was not used.)
* The CC BY recordings of LOOK.md 16.2 (ZU-23 and other Mil.ru films, BRRRRRRT, the Draken, the CF-18, the Boeing
  flyby) and the Freesound CC0 originals (login): not fetched.
* The Unity EXR and TGA image sequences (23-99 MB each), JangaFX volumes, ambientCG, the other Unity elements and
  Explosion01 (Explosion02HD was taken, being under 20 MB): not fetched.

## 4. What could replace what (for whoever wires them in)

**In use (1 Nov 2026):** `T_FXR_Unity_Explosion02HD_5x5` and its `_EmissionLog` are imported by
`tools/ue_make_fx_assets.py` (sRGB colour BC7; the log brightness linear grey) and drawn by `M_FX_Smoke`'s colour
path (custom data kind 2) for every explosion's fireball: the colour picture's straight alpha as opacity, its fire
(where `161^v - 1` is over 0.6) glowing by the fireball's heat⁴, its smoke lit by our sun, sky and flashes. The
generated `T_FX_Fireball` stays as the fallback when they are missing and for fires (kind 1). The Unity cloud and
smoke flipbooks were not taken: their silhouettes are hard-edged at our sizes; `T_FX_SmokeAtlas` is generated.

Our generated textures (`tools/fx_textures.py`) are **data**, not pictures: smoke atlas, contrail and debris are
R lit from above / G thinness / B erosion noise / A density; the fireball is R temperature / G lit / B erosion /
A density, coloured through `T_FX_HeatRamp`. The Unity flipbooks are **colour pictures with the light baked in** and
straight alpha. Using them as they are needs a second material path; repacking them into our channels is possible
for some:

* **`T_FX_Fireball`** <- `T_FXR_Unity_Explosion02HD_5x5` + `_EmissionLog`. A colour path: base colour and emissive
  from RGB (sRGB), opacity from A (straight alpha, translucent), emissive strength `161^v - 1` from EmissionLog so the
  core goes above 1.0 again (the 8-bit colour stops at white). Or repack into ours: R = EmissionLog normalised (as
  temperature, for the heat ramp), G = luminance of the colour (light is baked from the upper left, not from above),
  B = our erosion noise, A = A. 25 frames on a 5x5 grid instead of our 64 on 8x8: the SubUV size and frame rate
  change; no motion vectors are given, so blend between frames.
* **The fire layer** <- `T_FXR_Unity_FireBall01_8x8` (additive, no alpha) or, better for us, its `_Temperature`
  map as our R channel straight into the heat ramp (A from a smoothstep of the temperature). Loops.
* **`T_FX_SmokeAtlas`** <- `T_FXR_Unity_Cloud01_8x8` (dense puffs) or `WispySmoke01_8x8` (thin): repack as R lit =
  the grey RGB, G thinness ~ 1 - A, B = our erosion noise, A = A; 64 frames, so take every fourth for our 4x4 or
  play them as an animation. Smoke after a fireball: `Explosion01_nofire_5x5` (one-shot). Wreck and fire columns:
  `DiscSmoke01_16x4` (128x256 cells, loops).
* **`T_FX_Glow`** <- `T_FXR_Kenney_circle_05`; **`T_FX_Spark`** <- `star_04`, `star_06`, `star_07`;
  **`T_FX_Streak`** <- `trace_01`/`06`/`07` rotated 90 degrees and cropped to 256x32; **`T_FX_Ring`** <-
  `circle_02`-`04`. These already follow our "grey in RGB, same in A" convention.
* **`T_FX_DebrisAtlas`** <- `dirt_01`-`03` only as splatter clusters (they are not single pieces, so not a
  one-piece-per-cell atlas).
* **`T_FX_Scorch`** (RGB soot colour, A mask) <- RGB from `T_FXR_BurnedGround01_Diffuse`, A from a Kenney `scorch_0x`
  mask or our radial noise mask; roughness and normal (DirectX) for a decal that catches light.
* **No generated counterpart yet**: `muzzle_01`-`05` (AAA muzzle flashes), the black and white Kenney puff atlases
  (far flak bursts, distant motor smoke).

Sounds (`tools/fx_sounds.py` names). The recordings carry their own distance (air absorption, ground reflection,
Doppler in the passes); `USimFxAudio` adds these again, so use them as far layers or loops, not as near sources:

* `S_FX_Rocket_Motor_Large` <- `S_FXR_Rocket_StaticFire_SLS_Loop` (steady body), with `..._Shuttle_Far_STS131_Loop`
  or `..._AtlasV_Far_SDO_Loop` as the far layer. These are much bigger rockets than a SAM: pitch up and shorten for a
  missile. `S_FX_Rocket_Motor_Small` and `S_FX_Missile_Launch` stay synthesized (no Stinger audio, see above).
* `S_FX_Jet_Roar_Turbofan` <- `S_FXR_Jet_Heavy_SOFIA_Takeoff_Loop` for heavy aircraft; a far fighter layer <-
  `S_FXR_Jet_Military_Far_NPS_Loop`. `S_FX_Jet_Roar_Turbojet`, the whines and `S_FX_Afterburner` stay synthesized.
* `S_FX_Prop_Turboprop` <- `S_FXR_Prop_Turboprop_An24_Loop` (`..._Pass_Far` as a far one-shot).
* `S_FX_Rotor_Helicopter` <- `S_FXR_Rotor_Chinook_Loop` (`..._Pass_Far` one-shot; `S_FXR_Rotor_Heli_Far_Lake_Loop`
  for distance).
* `S_FX_Explosion_Small/Medium/Large_Far` <- keep the synthesized onset (the Friedlander pulse is the physics) and
  layer `S_FXR_Thunder_Roll_NPS` from about 1 s in as the rolling tail.
* Stay synthesized: the guns (the 23 mm recordings are CC BY), the near blasts, sonic booms, bullet crack, fire,
  debris, bomb rush, flare pop.

**Sounds in use (1 Nov 2026).** `tools/fx_sounds.py` makes nine bank sounds `S_FX_Rec_*` from these files
(written to `art/fx/sounds/` with the synthesized ones), and `data/fx/sounds.json` plays them. Its `"recordings"`
block gives each sound's file, the distance it was heard at (`recorded_at_m`, an estimate from the source's
description and its spectrum, so the game dulls it only by the distance beyond that) and its level match. Each
recording is heard at the profile's pitch, through the air's cut-off at the match distance, and given the same RMS as
the synthesized sound it stands for. `fx_sounds.py --check` recomputes every match (within 1 dB) and the layer
levels.

| Bank sound | From | Processing | Plays as |
|---|---|---|---|
| `S_FX_Rec_Jet_Far` | `S_FXR_Jet_Military_Far_NPS_Loop` | as cut | the far layer of the `turbojet` and `turbofan` profiles (fighters), crossfaded from the synthesized near roar between 600 m and 2.5 km |
| `S_FX_Rec_Jet_Heavy` | `S_FXR_Jet_Heavy_SOFIA_Takeoff_Loop` | as cut | the `heavy_jet` roar at every distance (the synthesized whine stays, near only) |
| `S_FX_Rec_Prop_Turboprop` | `S_FXR_Prop_Turboprop_An24_Loop` | as cut | the `turboprop` profile |
| `S_FX_Rec_Rotor_Near` | `S_FXR_Rotor_Chinook_Loop` | level flattened (1 s RMS windows, circular, so it still loops) | the `helicopter` near layer, pitch 1.4 |
| `S_FX_Rec_Rotor_Far` | `S_FXR_Rotor_Heli_Far_Lake_Loop` | level flattened (a 6 dB swell taken out) | the `helicopter` far layer |
| `S_FX_Rec_Rocket_Motor_Far` | `S_FXR_Rocket_AtlasV_Far_SDO_Loop` | as cut | the `rocket_small` far layer, pitch 1.8 (the near layer stays synthesized) |
| `S_FX_Rec_Rocket_Motor_Large` | `S_FXR_Rocket_StaticFire_SLS_Loop` | as cut | the `rocket_large` near layer, pitch 1.3 |
| `S_FX_Rec_Rocket_Motor_Far_Large` | `S_FXR_Rocket_Shuttle_Far_STS131_Loop` | as cut | the `rocket_large` far layer, pitch 1.2 |
| `S_FX_Rec_Blast_Tail` | `S_FXR_Thunder_Roll_NPS` | 11 s from 3.0 s (after the crack), low-pass 700 Hz (4th order: takes out the rain hiss above 2 kHz), a further -3 dB/s, 0.5 s fade-in | the rolling tail of every blast over 3 kg (the classes up to 50 kg, up to 250 kg and the largest), from 0.8-1.2 s after the blast, 12-6 dB under it, on the far side of the crossfade only |

Not used: the two flybys (`S_FXR_Prop_Turboprop_An24_Pass_Far`, `S_FXR_Rotor_Chinook_Pass_Far`). They carry their
own Doppler and their own approach and departure. No event marks a far pass, and the loops already move with the
aircraft. Use them only as far one-shots if such an event is added.
