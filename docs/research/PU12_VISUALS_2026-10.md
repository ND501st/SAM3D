# What the PU-12M's crew sees: research for its look (17-18 October 2026)

What the compartment of the 1980s PU-12M (9S482M) looks like, its instruments and their pictures, and the Soviet
conventions for tubes, lamps, flashing and motion behind them. Read for the PU-12M refinement after P7
(`docs/PU12_DEVELOPMENT.md`): the IT-45, the IT boards, the ДХ keypad, the plotter's map, the colours. Sources are
registered in `docs/SOURCES.md`; each section ends with what SAM3D made of it.

## 1. What was read

| Id | Tier | What it gave |
|---|---|---|
| `rwd_mb3` | 1 | The NVA catalogue's PU-12 pages (after K 050/3/016, A 327/1/412, A 259/1/408-413, A 372/1/412 and the *Handbuch für Fla-Einheiten* 1990). The ASPD-U's blocks and what each does; the commander's and the operator's workplaces block by block; the IT-1, IT-1M and IT-2M boards with their signal groups; the air-situation map; colour photographs of the compartment (the Australian Armour and Artillery Museum's PU-12M and Red Star Company's) |
| `shilka_narod` | 2 | A veteran's interior photographs (O. Zheltonozhko): the IT-45 with its grid glass, the ДХ keypad at its left and the ФР block at its right, whose seat is whose |
| `csla_pu12_manual` | 1 | The ČSLA manual's plan (53 items) and workplace drawings, as before (`docs/research/PU12_RESEARCH_2026-10.md`) |
| `ppt_pu12_lesson` | 4 | A Russian training presentation: the ASPD-U's blocks as the crew learns them; the picture shown "on screens and boards" |
| `viko_commons` | 4 | A Soviet remote PPI (ВИКО-1) in a museum: the orange filter glass over the tube |
| `radiolamp_elt` | 4 | The Soviet tube naming and screen types, after the tube handbooks |
| `gost_21829_76` | 1 | The Soviet standard for coding visual information: how fast warnings and alarms flash |

Not found: a photograph of the IT-45 switched on, the IT-45's tube type, the ДХ's window lit, a colour photograph of
the IT-1M lit. Searched and left: forum threads on the PU-12 (valka.cz, already read in P0), model-kit pages (modell-art,
modellmix: the exterior only), equipment-sale pages (no interior).

## 2. The compartment

- **Colours** (`rwd_mb3`'s colour photographs). The lining is a cream white, not grey. The ASPD-U's blocks and the power
  blocks are a medium blue-grey with black knobs and faceted red and amber lamp caps. The IT-45's housing, the ДХ and the
  IT-2M are light grey. The S-23 behind the driver is a black box with a row of coloured lamps.
- **Who sits where.** The NVA catalogue lists the PU-12M commander's workplace as the IT-45, the ДХ, the IT-2M, the ФР
  block, the RTK-1 and the DT-2 coordinate pickup, and names the three rear places as commander-operator, plotter
  (Planzeichner) and radio operator. With the ČSLA workplace drawing this puts **the battery commander at the IT-45** and
  the operator-plotter at the air-situation map (the plan's item 24), not the other way round as P6 had it.
- **Item 24** is the plotter's air-situation map (Luftlagekarte), a plexiglass board, not the grid glass over the IT-45.
- **The GAK** stands on a plate with four legs, the **PT-200 converter** under it (not above, as P6 had it).
- **The DI** of the ČSLA plan is the PU-12M's **DT-2** coordinate pickup (the PU-12 had a block DI).

SAM3D: the paints `Paint_Light` (cream), `Paint_BlueGrey`, `Paint_LightGrey` (tools/ue_import_pu12m.py and
tools/ue_import_cabin_interior.py), the panels' `color` in `data/cabins/pu12m.json`; the seats swapped
(`tools/blender/pu12m_interior_trace.py` STATIONS); the map board, the GAK stand and the converter rebuilt
(`tools/blender/build_pu12m_interior.py`).

## 3. The IT-45

- **What it shows** (`rwd_mb3`). The primary air picture and the telecode's data "as digits, signs, characteristics and
  symbols". The ASPD-U sends and shows the targets' numbers as two-digit decimals, commands and reports as decimal
  numbers, each target's speed vector, and works out its time of arrival. The ФР block makes the tube's radial-circular
  sweep, the range marks and a test echo; the 220N block turns the radar antenna's azimuth into the sweep's sine and
  cosine, so the sweep turns with the radar it is cabled to. The rack RPS-M holds a character generator (ZG), height and
  speed converters, a computer and a store.
- **Its scales.** 35, 50 and 100 km (`ru_pu12_text`); German Wikipedia's PU-12 article says 50 or 100 km. SAM3D keeps
  the three.
- **Its face** (`rwd_mb3`, `shilka_narod` photographs). A light grey housing, a round tube about 40 cm across behind an
  **amber filter** and a glass ruled in squares, a strip of knobs above. The filter is the one the Osa's search and
  elevation tubes wear and the ВИКО-1 remote PPI wears (`viko_commons`).
- **Its tube.** Soviet tubes are named by the screen's diameter in centimetres first, then ЛО (electrostatic) or ЛМ
  (magnetic deflection), a model number and a letter for the screen (`radiolamp_elt`): so the IT-45 most likely carries a
  45 cm tube. The screens with a long afterglow, the radar screens, are В (a white flash, a long yellow afterglow),
  К (pink, a long orange afterglow) and С (orange). Through an amber filter both read as a pale yellow flash that sinks
  to amber. The tube's own type was not found.

SAM3D (`USimCabinDisplayComponent::DrawAspdScope`):

- The sweep turns once in 10 s (an assumption: the radar's rotation is not modelled for the post).
- The range marks light as the sweep passes and fade with a 6 s afterglow. Behind the sweep the colour moves from the
  flash's pale yellow to amber.
- A cabled radar's picture: its noise speckled over the tube where the sweep has been, and each target's echo painted
  under its symbol, fading until the sweep comes round again. Telecode tracks have no echo.
- The character generator's symbols, steady: the number (stroke digits), a zone mark (H1 a circle, H2 a square, H3 a
  triangle), the speed vector (a minute's travel), friends barred, groups ringed twice.
- The DT-2's marker is a small ring open at the quarters. The marked target is drawn brighter, with its time of arrival
  (minutes:seconds). The target the ДХ is on is bracketed.
- The grid glass's lines catch the cabin's light, with an azimuth ring every 10 degrees. The plotter's grease-pencil
  marks moved to his own map (§6).
- The tube's material: a tight halo, slight curvature, a vignette, the cabin lamps' reflection.

## 4. The ДХ keypad

`rwd_mb3`'s colour photograph and `shilka_narod`'s:

- A light grey plate with a **window of blue glass** at its top.
- Two rows of toggles and a red lamp button.
- Ivory keys with black legends in black frames, red keys at the bottom corners and one in the upper block.
- Legends read at low resolution: ВР, ТС, СР, СМ, СВ, Ч, ГР, Н1-Н3, Н, the digits, М, ПВ, СЗТ.

The NVA catalogue calls it the characteristics pickup: it enters a target's number, its height zone and characteristics,
and the designation signal with an address.

SAM3D:

- **Working keys.** The digits pick a target by number. ЦУ then a digit gives that target to the launcher in that place
  on the IT boards; the commander reads it out on the battery net. СБР clears.
- **The window** is drawn as blue-green vacuum-fluorescent seven-segment digits with their dark segments faintly
  showing. It shows the number the keypad is on, ЦУ flashing once a second while it waits (§7), and the last designation
  ("05>22"). A VFD is SAM3D's reading of the blue glass.
- **The rest of the keys** are drawn only.

## 5. The IT boards

- **What they are** (`rwd_mb3`, after A 372/1/412). The IT-1 shows the fire units' technical state and combat work. It
  is also the primary device for entering that information and checking how it is shown. It has three signal groups:
  ammunition (Kampfsatz), combat activity, readiness. It also has a fuse, an on toggle, input keys and a clear key.
- **Legends.** Read off the photographs: КОМПЛЕКТ 1, 1/2, 1/4; (РАБОТА, uncertain) ГОТОВ, ВИЖУ, ВЫП.; ТЕХ. СОСТОЯНИЕ
  РПК, БЧ, ШАССИ; a НОМЕР column. The lamps are amber.
- **The IT-2M** is a light plate with the lamp matrix over a row of buttons.
- **Where they are** (a disagreement). `rwd_mb3`'s picture page puts the IT-1M "at the commander's place" and the IT-2M
  "at the operator's place". Its workplace list puts the IT-2M at the commander's workplace in the PU-12M. The ČSLA plan
  has item 4 by the front right seat (the vehicle's commander) and item 18 over the IT-45.

SAM3D:

- **Placement.** The IT-1M at the squad leader (the vehicle commander) and the IT-2M over the IT-45 at the battery
  commander, which agrees with the plan and the workplace list.
- **Rows.** One row per launcher in the battery's order, and a support-vehicle row (dark).
- **Lamps** (`simcore::BoardLampsOf`). КОМПЛЕКТ is lit by the share of missiles left: "1" for three quarters or more,
  "1/2" for 0.4 or more, "1/4" for any. ГОТОВ means ready, ВИЖУ engaging, ВЫП. launched. ТЕХ. СОСТОЯНИЕ stays dark (no
  faults modelled).
- **Motion.** The lamps ease on in about 35 ms and off in about 90 ms (§7).

## 6. The air-situation map

`rwd_mb3`: the Luftlagekarte is where the targets' tracks are drawn in the target-designation squares, relative to the
radars' positions. The targets' azimuth and slant range from the post are taken off it. It is a plexiglass board.

SAM3D (`DrawPlotBoard`):

- **The board.** Etched plexiglass over a white backing, with the post at the centre. It reaches 100 km (the ASPD-U's
  longest scale), with circles every 25 km and 25 km squares lettered А-Л along the top and numbered down the side.
- **The marks.** The plotter's grease pencil: a cross for each plot, joined into the track, then the number, and the
  height over the code (the board's notation, `data/command/pu12.json`). Red for enemies and unknowns, blue for friends.
- **What is SAM3D's.** The spacing, the lettering and the pencil's colours.

## 7. Lamps, flashing and motion

- **Flashing** (`gost_21829_76`): warning signals flash at 0.5–1 Hz, alarms at 5–6 Hz; 4–6 Hz is the threshold of
  fusion; no more than three signs flashing at once. SAM3D: the ДХ's ЦУ flashes at 1 Hz (lit 0.6 s). The Osa's
  overheated-gyro lamp flashes at 2 Hz, SAM3D's rate (ePap in `samsim_thread` says only that it flashes); the standard
  would put a warning at 0.5–1 Hz. Left as it is for now: the Osa's console was not part of this pass.
- **Signal lamps.** The photographs show small incandescent lamps under faceted coloured caps (red, amber, green, white).
  SAM3D eases every cabin lamp: on in about 35 ms, off in about 90 ms, orange as it dims (an incandescent filament's
  warm-up and cool-down; SAM3D's figures).
- **The tubes' motion.** A PPI's picture is painted by the sweep and fades; symbols from a character generator are
  steady. Both are drawn that way.

## 8. Rendering fixes made with this

- **The mauve cast.** It was Lumen's sky light seeping through the closed compartment's lining. The PU-12M's hatches and
  windscreen covers are shut, so while the camera is inside, the sky lights are put out
  (`ASimOsaVehicle::SetDaylightShut`). The lining is 15 cm thick with a ×10 distance field, and the cabin is hidden from
  outside (`USimCabinComponent::SetCabinHidden`).
- **The DT-2 and the IT-45's bezel were polygons.** Nanite's fallback mesh did that, so the interior is imported without
  Nanite. In a commandlet the import pipeline's build settings don't reach the mesh, so `USimEditorScripting` sets the
  distance field scale.

## 9. Still SAM3D's

The IT-45's symbols, glyphs and the sweep's period; what the ДХ's window shows; the IT boards' lamp-to-report mapping
and their placement where the sources disagree; the map's grid and pencil colours; the lamps' timings. Each is marked
in the layout's `notes` with a pointer here.
