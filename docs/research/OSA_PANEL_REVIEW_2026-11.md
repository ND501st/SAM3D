# The Osa's panels reviewed against the photos and the manual (12 Nov 2026)

The project owner asked for every panel's close-ups to be read again and checked against the documentation, for lamps,
controls and parts the model has missed, and was concerned that SŁ-2M lacks its top row of gauges and its bottom row of
blocks. Five readers went panel by panel through the book's 600 dpi scans (wwp_gecko, the ex-NVA **9A33BM3-1**) and
voenizdat_osa_1991 (which also describes the AKM); their reports are in `panel_review_2026-11/` (pixel boxes, legends
read with their certainty, and manual pages). This page collects what they found, sorted by what fixing it needs.

Page numbers are the manual's printed pages (the text file's `=== PAGE n` marker minus one), as the data cites them.
Every photo finding is from the BM3-1, not an Osa-AK: where the two may differ, the finding says so, and decision D1
(the AK's layout where SAM Simulator's AK photo shows it) still holds.

Spot-checked by hand: OR-31M's window legends (g092-d: ψ, θ, Qc left to right) and the 0094 strip's captions (SAM
Simulator's AK photo: СВР II, АЗИМУТ ССЦ, УГОЛ МЕСТА ССЦ, СВР I). Both as the readers report.

## 1. SŁ-2M is cut off at the top and the bottom

**Why.** The three columns (`op_panel`, `so_panel`, `fo_panel`) are 0.982 m tall because their layout came from SAM
Simulator's straight-on photo of the AK's columns, 982 px at 1 mm a pixel (`reference/samsim/columns_three.jpg`). That
photo is cropped: it shows the bottom 45-58 mm of the top row and the top 33-65 mm of the bottom row. None of SAM
Simulator's other images shows more.

**Measured** on g094-a through its certified grade-A camera, the photo's pixels cast onto the panels' plane (x -1.104;
a 2 cm error in depth moves these by 2-10 mm):

| | Rear column (op) | Middle (so) | Front (fo) | Model |
|---|---|---|---|---|
| Top row's top | 2.141 | 2.134 | 2.153 | 2.090 (the panels' top) |
| Top row's bottom | 1.986 (0062-6M) | 2.018 (0094) | 1.999 (0004-14M) | 1.983, 2.025, 2.034 (the data's blocks) |
| The rack's top rail over it | about 2.17 | 2.165 (the name plate's row) | | 2.10 (Fig. 3) |
| Bottom row's top | 1.183 | 1.164 | 1.196 | |
| Bottom row's bottom | 0.961 | 0.942 | 0.931 | 1.108 (the panels' bottom) |

So **the top row is about 155 mm tall and the model has the lower 90-105 mm of it; the bottom row is about 220-265 mm
tall and the model has its top 60-75 mm.** The real face runs from about 0.93 to 2.15 m, the model's from 1.11 to 2.09.
The bench's top does not hide the bottom row (it hides the panel only below about 0.89 m from this eye). This confirms
and enlarges the holistic pass's open note (the top blocks "about 3 cm above the 2.10 m rail").

**What fixing it touches:** the three panels grow (about +0.06 m up, +0.17 to 0.18 m down) with their faces' textures;
the rack carcass `SM_RackSL2M` (its top rail to about 2.17, against Fig. 3's 2.10: record both, the photo is used, as
for the aisle in section 27 of `docs/OSA_INTERIOR_HOLISTIC.md`); the roof lining's outboard edge (`roof_section`
`outboard_z_m` 2.1); the R-124 boxes A-3 and A-5 at z 1.0, which would stand on the bottom row's face; and the housings'
clearance tests. The AK's top and bottom rows exist in no AK photo found: the BM3-1's are the only source, so their
contents are drawn from it (decision D1 for anything the AK photo's crop does show).

**What the rows hold** (details in `panel_review_2026-11/sl2m.md`):
- **0062-6М1, "блок АРУ"** (op, top): meters **МОЩНОСТЬ ГЕНЕРАТОРА** (left), **КОНТРОЛЬ** (x10 µA, middle) and **ТОК
  ГЕНЕРАТОРА** (white mA meter, right); switches ТОК, МОЩНОСТЬ, РЕЖИМЫ, КАНАЛЫ (candidate); ПОДЖИГ ГШ with its toggle;
  knobs РРУ КШ ОСН., РРУ КШ ДОП.; switch АРУ — РРУ (p98); five fuses. **The data has `tx_current` on the МОЩНОСТЬ meter
  and `tx_power` on the КОНТРОЛЬ meter: wrong.**
- **0094 (-5 on the AK, -9 on the BM3)** (so, top): eight **pointer dials, not counters**, each with coarse and fine
  scales: Δβ, Δε of СВР II | Г.О., Т.О. of АЗИМУТ ССЦ | Г.О., Т.О. of УГОЛ МЕСТА ССЦ | Δβ, Δε of СВР I. There is no range
  dial (the data's ΔД is a misread Δβ) and the last two are СВР I, not spares. The data draws the strip about 60 mm low,
  over its own captions. A fuse holder "1А ~150В" and the key lock at its right end, where the data has `soc.top_toggle`.
  The manual reads the antenna's azimuth from this block's scale (p250).
- **0004-14М** (fo, top): four meters about 80 x 75 mm (the data's 66 x 42) under two white half-round hoods, three red
  caps (+?В, -150В, КОРПУС) and a lamp; on the AK, РЕЖИМЫ and ЧАСТОТА are left of the meters (D1: the AK's layout).
  `vent.workplaces` stands on a key lock.
- **0051-7М** (op, bottom): the five-position repetition-frequency switch (АВТ., РУЧН. Р-Р, РУЧН. F1-F3, F3, Р., p98;
  already in PLANNED), a data card, a red test socket.
- **ОК81-4М I** (so, bottom): the range tracker of missile channel I: **ВОЗВРАТ (channel I) is missing** (only
  `msl.return_2` exists), with БАЛАНС, a red 30-pin test socket, a white cover box, the tag КАНАЛ I.
- **ОК51-6М** (fo, bottom): **one block, not two**, with a narrow fuse panel forward of it (3 fuses, 8 jacks on the BM3).

## 2. Wrong in the data, fixable now (systems built)

| What | Fix | Source | Confidence |
|---|---|---|---|
| `or31_qc` on OR-31M's middle window (θ) | move to the right window, x ≈ 0.082 | g092-d (checked) | high |
| `gyro_fail` lamp and the ОТКАЗ Qc fixture on the dial's bezel screws | the two black caps under ОТКАЗ marked θΨ and Qc, right of АРРЕТИР. (the trace's "jacks" at 1057.7 and 1078.0 mm) | g086-a | high |
| 0062-6М1's meters swapped (`tx_current`, `tx_power`) | МОЩНОСТЬ left, КОНТРОЛЬ middle, ТОК right | SAM Sim AK photo, g094-b | high |
| 0094's dials: a ΔД dial and two spares | Δβ, Δε СВР II; Г.О., Т.О. azimuth; Г.О., Т.О. elevation; Δβ, Δε СВР I; pointer dials; ~60 mm higher | SAM Sim (checked), g095-a | high |
| `veh.bilge_pump` | it is the wave shield, ВОДООТРАЖАТЕЛЬНЫЙ ЩИТ ↓↑ (p224); PLANNED `p:vision` lists the shield as sources-only | g085-a | high |
| DP-3B's range switch ВЫКЛ./КОНТР./1/2 | ВЫКЛ., x1, x10, x100, x500; the button ПРОВЕРКА; the red lamp the signal lamp | g085-b | high |
| DP-3B cited to p189 | that page is the TZM's chassis; the launcher's DP-3B rests on the photo alone | manual | high |
| Plate ЛЮКИ ЗАКРЫТЫ on the commander's dash | ЛЮКИ ЗАКРЫТЬ (an instruction beside БЛОКИРОВКА) | g085-a | medium |
| `pwr.relay_heat` starts at ОТКЛ. | starts at ВКЛ. | p99, g097-a | high |
| `apu_unlocked_el` follows `apu_free` | it is РАССТОПОРЕНО Δε, εн: lit at АППАРАТУРА ВКЛ. when the ССЦ's and both СВР's elevation stops release | p99, p119 | medium |
| ЭКВИВАЛЕНТ СОЦ an inert fixture | lit at АППАРАТУРА ВКЛ., out when the antenna rises, switch at АНТЕННА (and ЭКВИВАЛЕНТ ССЦ alike) | p51-52, p99-100 | medium |
| ЭКВИВАЛЕНТ СПК I/II "out: not given" | out at launch: the switch turns to radiating by itself | p155 | medium |
| The voltmeter 0-250 | figured 100-250 V | g097-a, g097-c | high |
| PU-303's ГТА dial `value_per_turn` 12 | 10 a turn, 10 at the top | g084-b, g084-d | medium |
| ТОН. ВЫЗОВ on both R-123M on the lower block | the middle of three in the pocket under КОНТРОЛЬ НАПРЯЖЕНИЙ, about (-0.168, -0.010) | g090-d, g091-b | medium |
| A-2 box ВС/Р-123; A-1 with a volume knob and jack | the photographed A-2 has three positions Р-112 / ВС / Р-123, no knob, no jack | g087-c | medium (BM3) |
| `ssc.control_sel` "КОНТРОЛЬ" | the knob is РРУ КШ (ОСН./ДОП.); КОНТРОЛЬ is the meter | g094-b | medium |
| `ssc.modes`, `mon.tx_select` | РЕЖИМЫ the lower of the stacked knobs by meter 1, ТОК, МОЩНОСТЬ the upper | SAM Sim, g094-b | medium |
| Headset jacks on so and op | on OK81-4M's key lock and on the ПЕЛЕНГ oval's sockets: move | g094-a | medium |
| `gyro.supply` on the black ПУ-11 panel | ПИТАНИЕ ПТ — СЕТЬ is the toggle at the right of 0005-8M's middle row (~1041 mm); ОБОГРЕВ probably next left | g086-a | high / medium |
| Notes out of date | `pwr.gta` (labels legible on g084-b/d), `gta.load_ivp` (ИВП is the external supply, p164), `gta.lamp_test` (lights PU-303's lamps), soc/ssc.training (СОЦ right, ССЦ left) | | high |
| PLANNED `p:keyed` (a round keyed switch, g087-c) | it is the A-2 intercom box: merge | g087-c | medium |
| Dash: the stowing board's "small lamp" | a captive screw on a removable cover (plate 812.04) | g087-d | medium |

## 3. Missing, and their systems are built: can be wired

- **ОШИБКА ψ and ОШИБКА γ** on 0009-5M (the trace's two oval windows): glow 15-30 s after СИСТЕМА СТАБ. ВКЛ., γ narrow,
  ψ wide and narrowing when the antenna rises (p51-52).
- **OK81-4M I: ВОЗВРАТ** (channel I), with БАЛАНС.
- **OR-11M1** (the computer's control block): its lamps repeat ВНИМАНИЕ, ЦЕЛЬ В ЗОНЕ, УЗКИЙ ЛУЧ, К3, НЛЦ СУА (p122,
  p136-137); the data has only its nameplate. Also two x10 µA meters, РУ I/II, КОНТР. СРП, ПРЕДЕЛ ИЗМ. I/II, БАЛАНС.
- **ТРЕНИРОВКА — РАБОТА СПК-I and СПК-II**, both ends of 0091-17М1 (g092-e, p149): placed, nothing reads them, as
  `soc.training` and `ssc.training`.
- **OR-31M's ψ and θ scales** (p51): the gyro vertical exists.
- **0062-6М1's РРУ КШ ОСН., РРУ КШ ДОП., АРУ — РРУ** (p98): placed like the other maintenance controls.
- **The cabinet's hour meter** ВРЕМЯ РАБОТЫ АППАРАТУРЫ (g097-a): a counter like PU-303's МОТОЧАСЫ.
- **0005-8M's ПОТРЕБИТЕЛИ ОСН. — КОНТР. and КОРРЕКЦИЯ ОСН. — КОНТР.** (p51).
- **R-123M:** the КОНТРОЛЬ НАПРЯЖЕНИЙ rotary the mesh draws (РАБОТА 1/2 and the voltage checks), two ВКЛ./ВЫКЛ. switches
  in the pocket (one probably ШКАЛА), the КАЛИБРОВКА button-lamp.
- **A stabilisation mode switch** СИСТЕМА СТАБ. БР — ДР (p50) where `apu.lock` stands by SAM3D's choice: a separate
  `stab.mode`.

## 4. Missing, and their systems are not built: for PLANNED (later)

- **Fuse lamps everywhere:** every fuse holder on the racks has a lamp; the crew checks them dark at start-up (p52-53, p100,
  p166: 0096-9M1, 0096-10M on SŁ-3M; SP-1M's blocks, 0094-11M, OR-95-1M's two ПЕРЕГРУЗКА lamps). A fault class (fuse,
  lamp, system down) is not modelled.
- **The main engine** (p174: a diesel started electrically or by compressed air), its СТАРТЕР button, the drill's «К
  маршу готов» (p247); the ГОМ's engagement (a кран включения ГОМ off the engine, the speed held by a hand fuel-pump
  lever, Fig. 2.60, p164): `pwr.pto` now stands on PU-303's ВКЛЮЧЕНИЕ НАГРУЗОК ГОМ and `pto_generator` needs no engine.
- **The hand fuel-pump lever:** a ball-knobbed lever on a ratchet quadrant low left of the column (g086-d, g083-a).
- **The differential lock** (p175) and the fire system's on/off (p221).
- **Overheating** (ПЕРЕГРЕВ ОО04-20М1, already noted), **the IFF's 1L24-2 blocks** on the BM3 (not the AK's РЗМ set,
  p53-54).
- **OR-14М's** timers, ДЕЛИТЕЛИ НАПРЯЖЕНИЯ knobs and toggles (zeroed for ПУСК ГОТОВ, p154).
- **ПБ-107's** own fan on the engine bulkhead (p168).

## 5. Parts and plates the model lacks (presentation only)

- SŁ-2M: the rack plate "СЛ-2М2" on the top rail, the block plates ОО62-6М1 and ОО04-14М, every block's two key locks,
  0004-14M's red caps and lamp.
- PU-303: two fuse holders by ПИТАНИЕ УПРАВЛЕНИЯ, a red lens by КОНТР. ЛАМП, a fuse strip on the side face; the ТВГ dial's
  face is blue.
- 0094-11M: a lamp holder, jacks КОРПУС, +100В, +27В, -27В, a ПРЕДОХРАНИТЕЛИ cover, fuses, two key locks.
- The cab: the DP-3B's instruction plate and fuse box, the heater's start plate and lever-diagram plate, BP-26's
  СОБЛЮДАЙ ПОЛЯРНОСТЬ, the right cheek wall's two stowage doors, the blue lamp over the commander's visor (already in the
  docs), two toggles behind the wheel drawn only as a plate.
- SŁ-3M: five units in the lower row (g098-d), not four.
- Colours: the air gauge's dial white in a chrome rim; the ТВГ dial blue.

## 6. Open: cannot be settled from these sources

- The heater box: the cab air heater (the book's caption, the dash's ОТОПИТЕЛЬ, the duct) or the engine pre-heater
  (medium for the cab heater).
- The black bottles and the 250 gauge: fire bottles (the book) or air-start bottles (p174).
- The radio well's red-lever box: perhaps the battery switch ВБ (p163), low.
- ПР ВС on PU-303: ВС is the вентилятор-сепаратор; ПР unread.
- 0091-17M's check: currents on the BM3 (mA, ПЕРЕКЛ. ИСТОЧНИКОВ), voltages in the manual (КОНТРОЛЬ НАПР.); record both
  in PLANNED `p:0091_17`.
- The 0091-18 toggles used for `soc.training`/`ssc.training` carry no ТРЕНИРОВКА legend in the photo; the manual puts the
  switches on 0091-18 (p49, p97): kept.
- The ЗК-4 dial's legend У2 and its ring's divisions.
- op_panel's upper items stand up to 60 mm lower than SAM Simulator's photo has them, while the bottom agrees: which
  picture the face texture follows needs checking when the panel grows.
- Unreadable at the scans' resolution: SŁ-3M's plates, 0005-8M's ПУ-11 legends, the voltmeter's lowest figure, the upper
  repair toggles' legends, the air gauge's scale, several dash switches.

## 7. More photos (a web search, `panel_review_2026-11/web_photos.md`)

No straight-on, uncropped, high-resolution photo of SŁ-2M was found, and none of an unmodified Osa-AK's interior. What
helps, all copyrighted unless marked, none to be copied into the repo:
- **G. Prasał's oblique view of the console** (valka.cz `osa-ak_1.jpg` on the armedconflicts Osa thread, 1075 x 594):
  **the whole top row legible** (0062's two hooded meters and its switches, 0094's eight yellow dials, 0004's four
  hooded meters); the bottom row cut off. Said to be an ex-NVA 9A33BM3. The second view the top row needs; checked by
  eye here.
- **RWD-MB III** (an NVA veterans' site, rwd-mb3.de, page 9a33_3): 300 px photos straight on, 9a33_43 the rear and middle
  columns from the meters to the bottom row, 9a33_44 the front column whole; the page lists every rack's blocks by number
  (the BM3's SL-2M2 and OO96-2M2). Good for proportions and the blocks' order (`reference/photos/rwd_mb3/` has some).
- A forum photo along the left wall (kitmaker, vehicle unknown): the front column obliquely from 0004 down to the bottom
  row.
- The driver's dash in a public-domain US Marine Corps photo on Commons (3600 x 2400), the only free-licence find.
- Not opened: a dishmodels.ru walkaround (a certificate error: worth a look in a browser).


## 8. Applied (12 Nov 2026)

- **Section 1:** the three columns grown to their full height, 0.925-2.143 m (centre z 1.534, height 1.218); every item
  that stood on them moved up 65 mm in v, so it keeps its place in the cabin. The top row (0062-6M1, 0094-5, 0004-14M)
  and the bottom row (0051-7M, OK81-4M I, one OK51-6M with its fuse panel) are placed from drawing g094-c and the photos
  cast on the panel's plane: their blocks, meters, the eight 0094 dials (`svr2_*`, `ssc_*`, `svr1_*`), the controls
  `ssc.rru_main`, `ssc.agc`, `msl.return_1`, `msl.balance_1` and the relabelled `ssc.control_sel`. `SM_RackSL2M`'s rail
  runs to 2.14 and its posts to the left roof lining at 2.18 (the shell's new `left_outboard` section). Fig. 3's 2.10 m
  is recorded against the photos' 2.14-2.17; the photos are used. `tools/traces/osa_interior/panels/sl2m_op.json` keeps
  the old frame: its "about" says to add 65 mm to v before rerunning `panel_apply.py`.
- **Section 2:** every row fixed as the table says. `p:keyed` dropped from PLANNED (it is the A-2 box); `p:vision` keeps
  only the periscopes.
- **Section 3:** wired: ОШИБКА ψ and ОШИБКА γ (lamps on 0009-5M's windows, glowing 20 s after СИСТЕМА СТАБ. ВКЛ., the
  new subsystem `stab_error_glow`); OR-11M's repeats of ВНИМАНИЕ, ЦЕЛЬ В ЗОНЕ, УЗКИЙ ЛУЧ, К3 and НЛЦ СУА (low: placed in
  the top row of the block's 26 round openings, read as its lamp табло); OR-31M's ψ and θ scales (zero: the vehicle's
  tilt is not modelled); ВОЗВРАТ I with БАЛАНС; РРУ КШ ОСН., РРУ КШ ДОП., АРУ — РРУ. Placed and read by nothing:
  0005-8M's ПОТРЕБИТЕЛИ ОСН. — КОНТР. (`gyro.consumers`, the block's last free toggle); each R-123M's КОНТРОЛЬ
  НАПРЯЖЕНИЙ (nine positions) and КАЛИБРОВКА, in the mesh. Tested in `test_cabin.cpp` ("0009-5M's ОШИБКА indicators").
- **Not done, and why:** КОРРЕКЦИЯ ОСН. — КОНТР. (no toggle left on g091-a: `p:gyro_sw`); `stab.mode` БР — ДР, because
  `apu.lock` stands on that toggle by a sourced choice (mon_2112_80 p22: on the AK, unlocking the АПУ does that switch's
  work), so moving it is the owner's call; the R-123M pocket's three switches (unnamed); the cabinet's hour meter (the
  mesh draws it; nothing to read). The ψ indicator's sector narrowing as the antenna rises is not drawn.
- **Section 4:** in PLANNED (`p:fuse_lamps`, `p:engine`, `p:fuel_lever`, `p:diff_fire`, `p:iff_1l24`, `p:or14`,
  `p:pb107_fan`; overheating was already `p:ind_overheat`).
- **SAM Simulator:** new screenshots would not help: the game draws these columns from the same cropped 982 px images,
  so the top and bottom rows are not in it at all.
- **Still open:** headset jacks on the columns (no destination known), `vent.workplaces`' place (SAM3D's choice), A-1's
  volume and jack (not verified).
- **Second pass (same day):** the top and bottom rows read again on the flats and corrected part by part
  (`docs/OSA_INTERIOR_HOLISTIC.md` section 52): 0062-6M1's knobs, sockets and ПОДЖИГ ГШ, 0094-5's dial pitch,
  0004-14M's meters, hoods and red caps, and the bottom row's parts and blocks.
