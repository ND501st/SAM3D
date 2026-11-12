# Review: left racks, cabinet 0096-9M2, air panel, engine bulkhead (Osa-AK cabin)

Read-only review, 12 Nov 2026. Worktree `C:\dev\SAM3D-wt\osa-book`.
- Photos: `out/recon/dryrun/photos/raw/gNNN-x.jpg`. Pixel boxes are (x0,y0,x1,y1) in the raw file's full resolution.
  Crops are in `scratchpad/review/left_racks/` and are never for the repo.
- Manual: voenizdat_osa_1991 `text.txt`. **Page numbers are printed pages.** The printed page is the file's
  `=== PAGE n ===` marker minus 1, the same convention the data uses (for example p50 = the marker PAGE 51).
- The photographed vehicle is an ex-NVA 9A33BM3-1 (AKM family): its cabinet plate reads ОО96-9М2, and the manual
  describes 0096-9M1 (9A33BM3). Where the AK's own block may differ, this report says so.
- Certainty: H means read clearly, M means read with some doubt, L means a candidate reading.

---

## 1. gyro_panel (rack SŁ-1M) and gyro_arrest_panel

### 1.1 Legends read

| photo, box | text | cert. |
|---|---|---|
| g086-a (1030,390,1140,410) | plate `0005-8М`; data plate `ПБ2.390.155 / 8 кг / №` | H |
| g086-a (0,200,330,720) | fuses `5А +27V`, `1А СИГН.`; toggle `ПИТАНИЕ ПТ` (up) / `СЕТЬ` (down); `...ЕВ` (ОБОГРЕВ) to its left; `ОТКАЗ` heading over **two black lamp caps marked `θΨ` and `Qc`**, directly right of a small dark button `...ИР` (АРРЕТИР.) | H |
| g086-a (230,280,680,680) | the round dial's legend reads **`У2`**, not ЗК-4. A four-lobed black bezel with a knurled knob on its right ear. The ring is graduated in a dark field and its figures are not legible; L: 0-60 (gun-sight divisions) rather than degrees | H for У2, L for the scale |
| g086-a (0,1300,900,2083) | 0009-5M: `ПРИВОД V(γ)` guarded toggle `ВЫКЛ./ВКЛ.`; `+27V` guarded toggle `ВЫКЛ./ВКЛ.`; fuse `2А`; jacks `СИГНАЛ`: `УПР.` and `ОШ.` over `V`, `Vo`, `θ`, `θ`, `⊥`; key lock `ОТКР./ЗАКР.`; plate `0009-5М` | H |
| g091-a (1440,1170,2240,1520) | 0009-5M: **two oval dark windows under the heading `ОШИБКА`** (6 letters), marked ψ and γ, between ПРИВОД ψ and ПРИВОД γ | M |
| g091-a (1740,620,2000,880) | 0005-8M middle row, right of the black ПУ-11 panel: three black jacks, then three toggles; the right-most toggle is `ПИТАНИЕ ПТ/СЕТЬ` (agrees with g086-a); the bottom row is a row of jacks/lamps under `КОНТРОЛЬ СИГНАЛ`(?) | M |
| g091-a (900,560,1440,1100) | 0004-12M: two `x10 мкА` meters, `КОНТРОЛЬ СИСТЕМ`, `ПРОВЕРКА ПЕРЕХОДНЫХ ХАРАКТЕРИСТИК`; top-left a dark cap over a red-orange cap marked `+27В`(?) and `2А`(?) | M / L |

### 1.2 In the photo, not in the data (or placed wrong)
1. **ОТКАЗ Ψ,Θ and ОТКАЗ Qc are misplaced.** The data puts lamp `gyro_fail` and the `ОТКАЗ Qc` fixture at
   (0.5643, 0.2134) and (0.5643, 0.159): the trace's "lamps" at x 1201.8 mm, right of the dial. In g086-a those two
   dark dots mirror two on the dial's left side, so they are the bezel's fixing screws.
   - The real lamps are the two black caps under the legend `ОТКАЗ`: `θΨ` and `Qc`.
   - They stand right of АРРЕТИР. These are the trace's two black "jacks" at (1057.7, 142.7) and (1078.0, 142.7) mm.
2. **ОШИБКА ψ and ОШИБКА γ indicators** on 0009-5M: the trace's two `window` parts at about (945-1024, 400-435) mm.
   The data has no display for them. See 1.3: they are a normal-work sign of the stabilisation system, which is
   modelled.
3. **0005-8M's middle row** (H for ПИТАНИЕ, M for ОБОГРЕВ):
   - `ПИТАНИЕ ПТ — СЕТЬ` is the toggle at the row's right end, about (1041-1050, 85-93) mm. The trace calls it a
     black jack. The data puts `gyro.supply` at (773.6, 93) on the black ПУ-11 panel.
   - The `ОБОГРЕВ` legend sits about one toggle further left, about 1015 mm. That is where the data puts
     `orient.relay` (РЕЛЕ — Qc). `gyro.heat` is at (891, 95.5) on the black panel.
   - The book's notes also read "(ПЕР)ЕВ. СИГН. РЕЛЕ" and "Qc" in this row.
   - Proposal: re-read this row on g086-a and g091-a together before moving the orientation toggles. Candidates for
     the 992 mm and 1015 mm toggles: РЕЛЕ — Qc, ОБОГРЕВ, АК — ЗК.
4. The 0005-8M fuse row (`ГИРОВЕРТИКАЛЬ`/`ГИРОАЗИМУТ` groups `IФ IIФ IIIФ +27V СИГН.`) and the 0009-5M fuses and
   jacks are in the mesh only. That is fine (inert). The fuses' indicator bulbs are covered in 3.3.

### 1.3 In the manual, not in the data
| item | page | system modelled? |
|---|---|---|
| ОШИБКА ψ / ОШИБКА γ electron-light indicators on 0009-5M. They glow 15-30 s after СИСТЕМА СТАБ. ВКЛ., γ's sector narrow and ψ's wide; ψ's narrows when the antenna rises at 3 min | p51, p52 | **yes** (stabilisation, antenna raise): a ready-made normal-work cue |
| ПОТРЕБИТЕЛИ ОСН. — КОНТР. and КОРРЕКЦИЯ ОСН. — КОНТР. (0005-8M) | p51 | partly (GA-3 orientation); not in PLANNED, which lists only КОРРЕКЦИЯ ВКЛ. Ψ/Θ |
| potentiometer θ (setting up latitude correction); potentiometer К "not connected" | p51 | no (adjustment only) |
| lamp ОТКАЗ Ψ,Θ lights while АРРЕТИР. is held (the data does this through gyro_erected=starting) | p51-52 | yes. Its place is wrong (1.2 item 1) |
| p52: "lamps ОБРЫВ ФАЗЫ β,γ (0096) and ОТКАЗ Ψ,Θ, ОТКАЗ Qc (0005-8M) must not be lit": a crew check | p52 | lamps exist; the check is not in the procedures |

### 1.4 Data contradicted
- `gyro_fail` / ОТКАЗ Qc placement: see 1.2 item 1 (H).
- `qc_setter` label "ЗК-4 (Qc)": the photo's legend over the dial is `У2`. The manual (p48) says ЗК-4 is on 0005-8M,
  so the dial is probably the ЗК-4 under the photo legend У2.
  - Keep the data's id but draw the legend `У2`.
  - The scale's "degrees, 12 marks" is an estimate. If the ring is 0-60, then 12 marks fits 60-00 divisions, not 360°.
- `gyro.supply` (and probably `gyro.heat` and `orient.relay`) placement: see 1.2 item 3.

---

## 2. pwr_cabinet (0096-9M2 door), repair_toggles_panel, repair_lid_panel

### 2.1 Legends read (g097-a is the sharpest; g096-a confirms)
| photo, box | text | cert. |
|---|---|---|
| g097-a (1830,1040,2210,1110) | plate `ОО96-9М2` and a small plate beside it | H |
| g097-a (180,1200,560,1500) | `ВРЕМЯ РАБОТЫ АППАРАТУРЫ`; hour meter `ЧАСЫ`, 4 drums, `220В`, `400Гц`, `БГ3.058.000`(?), `№211476` | H |
| g097-a (620,1200,900,1400) | `АППАРАТУРА`: black `ВКЛ.`, red `ОТКЛ.` | H |
| g097-a (280,1640,1250,1924) | `СИСТЕМА СТАБ.`: lamp, buttons `ВКЛ.`/`ОТКЛ.`. The bracket from СИСТЕМА СТАБ. runs right to the toggle **`ДР` (up) / `БР` (down)**. `АНТЕННА СОЦ ЗАСТОПОРЕНА`: lamp, toggle `РАССТОП.` (up) / `СТОП.` (down). `ПРИВОДЫ` lamp labels `СВР I`, `εн`, `СВР II`; `ПРИВОД βн` under the ДР/РАССТОП. pair. `ОТКЛ.` / `РЕЛЕ ТЕПЛ. ВКЛ.` toggle, bat pointing down (ВКЛ.) | H |
| g097-a (1220,1250,1640,1600) | voltmeter `V`, figures `100 150 200 250`, a **suppressed-zero scale** starting below 100 (L: about 75-80), class mark bottom right, green sticker `11NVA`; `220В 400Гц` beside it | H for the figures |
| g097-a (1500,1100,1800,1200) | `УСТАВКА 220В 400Гц`(?) over a small knob: L, it reads like "220В1000Гц" on the print | L |
| g097-a / g096-a (2950,1900,3220,2100) | `ПОДСВЕТКА` under a lamp fitting with a small knurled knob; `РАССТОПОРЕНО` over two green lamps marked **`Δε, εн`** (left) and **`βн`** (right) | M for Δε, H for the rest |
| g096-a (2560,2450,2800,2520) | `ТОВ` toggle (`ТОВ` up / `ОТКЛ.` down); `ФАЗЫ` with two buttons `АС`, `ВС` on one guard | H |
| g096-a (2660,2330,2980,2650) | two lamps `ВКЛ.` and `Р≥0,18`(?); a vertical bracket legend **ПОДКАЧКА ОТСЕКОВ** (letters stacked: 8+7); capsules `ШТАТНАЯ` (button) and `АВАРИЙНАЯ` (toggle, `ОТКЛ.` below); lamp `ОБРЫВ ФАЗЫ β, γ` with a fuse pair `2,3`(?) | M |
| g096-a (1980,2490,2900,2830) | fuse groups `НАКАЛ, АНОД`; `СУА 5А`; `ПРИЕМН. СОЦ 3А`; `ПРИЕМН. ССЦ 5А`; `0095-10`; `0095-23 / 0095-50`; `-150В`; `-600В`(?); `-250В +600В`(?); `СОЦ 5А`; `НГЗ 5А`(?); `0095-20М СЕТЬ 1А`; lamp `КОМПРЕССОР ВКЛ.`; lamp `НЕИСПРАВНОСТЬ ...` (its second line not legible) | M / L |
| g096-a (2880,2690,3220,3300) | `ЭКВИВАЛЕНТ` column of lamps `СОЦ`, `ССЦ`, `СПК I`, `СПК II`. The two black strips are **fuse strips ПР1-ПР10**: `ВЕНТИЛ. ПРИЕМН. СОЦ` (ПР1-2), `ПРИВОД βн` (ПР3-4), `ПРИВОД βн КОМПРЕССОР` (ПР5-6), `ПРИВОД СВР I` (ПР7-8), `ПРИВОД СВР II` (ПР9-10) | M |
| g096-a (2050,3000,2400,3100) | lamp `ОБРЫВ ФАЗЫ ГО?`. The print is ambiguous between Н and М; the manual (p96, p100) says **ГОН**, so the data's ГОН stands and the book notes' "ГОМ" is wrong | M |
| g097-a (580,120,1120,190) | lid plate `ТУМБЛЕРЫ РЕМОНТНЫЕ / РАБОЧЕЕ ПОЛОЖЕНИЕ ВКЛЮЧЕНО` | H |
| g097-a (480,690,1700,1000) | repair toggles, 13 upper + 10 lower (the data's 23 is right; the book notes' "15" is wrong). Lower row: `АНОД` group of 4 (`ОТКЛ.` under); `СОЦ`, `ССЦ` with `АНТЕННА` above / `ЭКВИВАЛЕНТ` below; `СПК I`, `СПК II` with **`ЭКВИВАЛЕНТ` above / `АНТЕННА` below** (opposite sense); `КОМПРЕССОР ВКЛ.` / `ОТКЛ.`; `ПОДСВЕТКА ОТКЛ.`(?) above / `ВКЛ.` below. Upper row: group headings `НАКАЛ ...`, `...ДАТЧИКИ`, `ПРИВОД` (over 2-3 toggles marked like `СВР I`, `СВР II`(?)), `ОТКЛ.` along the row; the last toggle `ВКЛ.`(?) / **`ГОН`(?)** | M lower, L upper |

### 2.2 In the photo, not in the data
1. The hour meter `ВРЕМЯ РАБОТЫ АППАРАТУРЫ` (a 4-drum counter) is not carried; the panel notes say so.
   - Cheap to add as a counter display reading equipment-on time.
   - The PU-303 hour counters are already fixtures, so this would match them.
2. The fuse strips ПР1-ПР10 and their group legends are traced as plain "black strips".
3. The `ПОДСВЕТКА` lamp fitting and the `ПОДСВЕТКА` repair toggle that lights the lid's tube lamp are not carried.
   They are known, and minor.
4. Repair toggles: only 7 of 23 have legends in the data. Candidate legends from the manual are in 2.3.
   The АНТЕННА/ЭКВИВАЛЕНТ position legends (opposite for СОЦ/ССЦ and for СПК) are missing.

### 2.3 In the manual, not in the data
| item | page | modelled? |
|---|---|---|
| under the lid: НАКАЛ ВЕНТИЛЯЦИЯ ПЕРЕДАТЧИКИ — СОЦ — ОТКЛ. (likely the upper row's НАКАЛ group, with the same switches for СПК-I, СПК-II, p149) | p49, p149 | transmitters yes; the switches are inert fixtures without legends |
| under the lid: ГОН — ОТКЛ. (scanner motor of ОС04-18М): **candidate for the upper row's last toggle, legend `ГОН`, which the data reads as "— ГОТ."** | p96 | no |
| under the lid: ПРИВОД СВР-I — ОТКЛ., ПРИВОД СВР-II — ОТКЛ. (the upper row's `ПРИВОД` group) | p119 | yes (drives); inert |
| АНТЕННА — ЭКВИВАЛЕНТ СОЦ / ССЦ / СПК-I / СПК-II (lower row toggles 5-8) | p49, p96, p149 | the toggles are present without position legends |
| fuse-blown indicator lamps in 0096-9M1 and 0096-10M; p52 and p100: the crew checks "fuse lamps of the power blocks not lit" | p166, p52, p100 | no |
| hour meter | not found in the manual | photo only |

### 2.4 Data contradicted
1. **`pwr.relay_heat` initial position.** The manual gives the start position ВКЛ. (p99: "РЕЛЕ ТЕПЛ. ОТКЛ. — ВКЛ.
   ... (исходное положение ВКЛ.)"), and the photo shows the bat at ВКЛ. The data has no `initial`, so 0 = ОТКЛ. (H)
2. **`apu_unlocked_el` ("РАССТОПОРЕНО εн"), label and logic.**
   - The legend is `Δε, εн`.
   - The manual (p99, p119) lights it when the elevation stops of the ССЦ and of both СВР channels are set to
     РАССТОПОР., and right after АППАРАТУРА ВКЛ. ("загораются лампы РАССТОПОРЕНО Δε, εн и ЭКВИВАЛЕНТ ССЦ").
   - Only `РАССТОПОРЕНО βн` follows the АПУ's travel lock. The data ties both lamps to `apu_free`. (H on the
     source; the AK may differ: same manual family)
3. **ЭКВИВАЛЕНТ СОЦ** is an inert fixture whose note says it is lit only with the repair toggle at ЭКВИВАЛЕНТ.
   - The manual (p51, p52) says it lights at АППАРАТУРА ВКЛ., and goes out when the antenna rises (about 3 min) if
     the switch is at АНТЕННА.
   - It should be a lamp, with the raise as its condition. (H)
4. **ЭКВИВАЛЕНТ ССЦ** also lights at АППАРАТУРА ВКЛ. (p99) and goes out when the АНТЕННА—ЭКВИВАЛЕНТ toggle is set to
   АНТЕННА (p100). The data has it as an inert fixture. (M: whether the crew throws that under-lid toggle in normal
   work is not clear against the lid's "working position" plate.)
5. **`spk1_dummy` / `spk2_dummy`.** Their notes say "when they go out again is not given". p155 does give it: at
   launch the СПК's АНТЕННА—ЭКВИВАЛЕНТ switch goes over to radiating automatically, so the lamp should go out at
   launch, or at БОРТ СПК or the channel's start. (M)
6. **`pwr_voltmeter`** (`marks 5`, `full_scale 250`, from 0 implied): the dial is suppressed-zero, figured 100-250.
   The drawing in g097-c also reads 100-250. (H)
7. **The ДР — БР toggle is the СИСТЕМА СТАБ. mode switch.** It sits under the СИСТЕМА СТАБ. bracket, and p50
   reads "СИСТЕМА СТАБ. БР — ДР, боевой или дежурный (на стоянке) режим".
   - `apu.lock` stands on it. That is a documented SAM3D choice, so it is not new, but the photo confirms the
     legend's bracket.
   - On a later pass, consider a separate `stab.mode` and moving `apu.lock` off it.
8. `soc.rotation_lock` labels "РАССТОПОР. — СТОПОР." (the manual's) against the door's `РАССТОП.` / `СТОП.`: draw
   the door's abbreviations. (H, cosmetic)
9. `press_018` label "Р=0,18": the manual's lamp is `Р≥0,18` (p171-172). (M, cosmetic)
10. Repair toggle 13's label "— ГОТ.": a candidate `ГОН` (p96). Toggle 10's "ПОДСВЕТКА — ВКЛ." reads with ВКЛ.
    below and ОТКЛ.(?) above, the opposite sense to КОМПРЕССОР ВКЛ. (L)

---

## 3. sl3m_panel (rack SŁ-3M)

### 3.1 Legends read
| photo, box | text | cert. |
|---|---|---|
| g099-a (2000,1000,2983,2243) | 0096-10M fuse cover `ПР1 ПР2 ПР3 ПР4 ПР7 ПР9 ПР10 ПР11` (from the notes, confirmed by eye); three 90° plugs | M |
| g099-b (200,50,1060,1020) | six blocks; plates not legible. Each block has a black knob at the left, rows of fuse holders with bulbs, small lamps/jacks and a key lock with a **red lead seal** at the right; 0093-28 has 7 fuses, 7 + 6 small lamps and two large fuses | M (layout), legends L |
| g099-b (1260,0,1440,860) | a tall aluminium placard on the strut listing blocks and fuses (headings like `0091-18М`, `ОК81-?М`, `ОС61-5М`...) | L |

### 3.2 In the photo, not in the data
- No lamps, fuses or locks on SŁ-3M in the data (blocks only; the mesh SM_SL3MFace may carry them). The fuse
  bulbs (3.3) are the only part a crew would read.
- The strut's fuse placard, the `КЛЮЧИ` plate with two T-handled keys on the strut foot (g099-b
  (1050,1550,1250,1800)), and the tall fuse placard beside SŁ-3M (g096-a, right): presentation only.

### 3.3 In the manual, not in the data
- 0096-10M in SŁ-3M distributes 220 V 400 Hz to the РТА's filament and anode, the drives ψ, γ and βн, the ПЧТ-1,25,
  the ventilation of the АПУ and the racks, and 0093-28, with fuses (p165). Its consumers are switched from
  0096-9M1, 0009-5M and 0004-11M1 (p166).
- Fuse-blown indicator lamps in 0096-10M (p166) and the crew check at start-up (p52, p100). This is a whole fault
  class (blown fuse → lamp → system down) that the console could carry. Not modelled.
- The AK difference: the manual (9A33BM3) and the photo (AKM) agree on the block list with the data
  (`mon_2112_80` Fig. 3), so no AK-specific gap was found.

### 3.4 Data contradicted
None found (the plates are illegible on every photo).

---

## 4. air_panel

### 4.1 Read
- g099-b (800,1140,1080,1420): a round gauge, **white dial** in a chrome bezel, a green sticker, on an angled sheet
  bracket. The figures are not legible.
- g099-b (650,1780,1000,2080): a brass tee cock with a flat tab handle on the blue line, a side branch to the left.

### 4.2 Not in the data, and data contradicted
- The fixture's colour is `black`; the dial is white with a chrome rim. (H, cosmetic)
- Candidate for what it reads (L): the manual names the chassis receiver (20 l, 5-7 kgf/cm² with the engine
  running) and the azimuth block's receiver (1.5 l, compressor cycling at 2.5-2.6 kgf/cm²). Both are on p169-172.
  The blue line runs to the turret's rotating air joint, so the gauge plausibly shows one of these. Its scale is
  still unread.
- No page names the gauge or the cock, so the panel's notes stand.

---

## 5. bulkhead_panel and bulkhead_left_panel

### 5.1 Read
| photo, box | text | cert. |
|---|---|---|
| g100-a (0,0,1841,2083) | red plate `ВНИМАНИЕ! / ПРИ ДВИЖЕНИИ ШАССИ И РАБОТЕ ГОМ / ВХОД В МОТОРНОЕ ОТДЕЛЕНИЕ / ЗАПРЕЩЕН.` | H |
| g100-c (80,1160,360,1440) | black plate: lever diagram `ОТКРЫТО` / `ЗАКРЫТО` / `ГЕРМЕТИЗАЦИЯ` (three positions; the data's labels agree) | H |
| g100-c (1330,560,2050,1840) | a tall dark-grey box over a rectangular opening with a round blower inside; stencils `М16`, `ПУ..`(?) | M |
| g100-c (1720,1870,2340,2320) | the sloped hinged cover with a coil spring (the right ЗИМА—ЛЕТО cover) | H |

### 5.2 Not in the data / in the manual
- **ПБ-107** (p168): "the block ПБ-107, placed on the bulkhead between the operators' and the engine compartments,
  is ventilated separately: air enters through a slot from the operators' compartment and its fan blows it back".
  - The tall grey box with the blower behind it (g100-c) fits this. The book calls it a heating outlet. (L-M)
  - Nothing in the data names ПБ-107. What it is (a power block?) is not stated in the extract.
- The door warning plate and the lid's `ТУМБЛЕРЫ РЕМОНТНЫЕ` plate are not in the cabin data. The trace says the lid
  plate's legend is not drawn. The door plate may be in the bulkhead mesh's texture; check.
- p173: the bulkhead is a sealed partition with a door. The door's ГЕРМЕТИЗАЦИЯ position ties to the vehicle's
  sealing (NBC/water). Not modelled; the controls are inert, as the data says.

### 5.3 Data contradicted
None.

---

## Not checked / limits
- rwd_mb3 photos are 300 px thumbnails, and SAM Simulator's exe_img art shows no SŁ-1M, SŁ-3M or 0096 face, so
  neither added legends.
- Halftone screens limit every crop past about 4×. The SŁ-3M plates, 0005-8M's black ПУ-11 panel legends, the
  voltmeter's lowest figure and the repair toggles' upper-row legends stay unread.

## Top findings
1. **ОТКАЗ Ψ,Θ and ОТКАЗ Qc are on the wrong parts.**
   - The data's `gyro_fail` lamp and the ОТКАЗ Qc fixture sit on the ЗК-4/У2 dial's bezel screws (trace x 1201.8 mm).
   - The real lamps are the two black caps under `ОТКАЗ` marked `θΨ` and `Qc`, just right of АРРЕТИР. They are the
     trace's "jacks" at (1057.7, 142.7) and (1078.0, 142.7) mm (g086-a, H).
2. **ОШИБКА ψ / ОШИБКА γ indicators on 0009-5M are missing.**
   - They are the two oval windows (trace `window`s, g091-a).
   - The manual (p51-52) makes them a stabilisation normal-work sign: they glow 15-30 s after СИСТЕМА СТАБ. ВКЛ.
     with γ narrow and ψ wide, and ψ narrows when the antenna rises. The system is modelled.
3. **ПИТАНИЕ ПТ — СЕТЬ is the toggle at 0005-8M's middle-row right end** (about 1041 mm; g086-a, H), not on the
   black ПУ-11 panel where `gyro.supply` stands. `ОБОГРЕВ` is probably the next toggle left (about 1015 mm, where
   `orient.relay` stands; M). Re-read that row before moving the orientation toggles.
4. **`pwr.relay_heat` should start at ВКЛ.** (p99 "исходное положение ВКЛ."; the photo's bat is down at ВКЛ.).
   The data defaults to ОТКЛ.
5. **`apu_unlocked_el` is РАССТОПОРЕНО Δε, εн.** It lights at АППАРАТУРА ВКЛ. when the ССЦ and СВР elevation stops
   are released (p99, p119), not with the АПУ's travel lock. Only РАССТОПОРЕНО βн follows `apu.lock`.
6. **ЭКВИВАЛЕНТ СОЦ should be a working lamp**: lit at АППАРАТУРА ВКЛ., out when the antenna rises if the switch is
   at АНТЕННА (p51-52). The data's note, "lit only with the repair toggle at ЭКВИВАЛЕНТ", is contradicted.
   ЭКВИВАЛЕНТ ССЦ likewise lights at АППАРАТУРА ВКЛ. (p99-100).
7. **ЭКВИВАЛЕНТ СПК I/II go out at launch**: the СПК's АНТЕННА—ЭКВИВАЛЕНТ switch turns to radiating automatically
   (p155). The data says when is "not given".
8. **The voltmeter is suppressed-zero, figured 100-250 V** (g097-a, H; the drawing g097-c agrees). `pwr_voltmeter`
   implies 0-250.
9. **The ЗК-4 dial's legend on the vehicle is `У2`.** Its ring is probably 0-60 divisions, not degrees. The data's
   label and its "degrees" estimate need a second look.
10. **The ДР — БР toggle is "СИСТЕМА СТАБ. БР — ДР"** (the bracket on the door; p50: combat/standby mode).
    `apu.lock` stands on it by SAM3D's choice. Consider a separate `stab.mode` and moving `apu.lock` off it.
11. **The repair toggles' legends from the manual**:
    - the lower row's АНТЕННА/ЭКВИВАЛЕНТ position sense is opposite for СОЦ/ССЦ and for СПК;
    - the upper row holds НАКАЛ ВЕНТИЛЯЦИЯ ПЕРЕДАТЧИКИ (p49, p149) and ПРИВОД СВР-I/II (p119);
    - toggle 13 is probably **ГОН — ОТКЛ.** (p96), not "— ГОТ.".
12. **Fuse-blown indicator lamps** in 0096-9M1 and 0096-10M (SŁ-3M) (p166), with the crew's start-up check (p52,
    p100). There is no fault path for them: a possible fault class.
13. **The 0096-9M2 hour meter `ВРЕМЯ РАБОТЫ АППАРАТУРЫ`** (4 drums, `ЧАСЫ`, `220В 400Гц`) is not carried. It is a
    cheap counter, like PU-303's МОТОЧАСЫ.
14. **ПБ-107 on the engine bulkhead** (p168: its own fan, slot from the compartment) is likely the tall grey box
    over the blower in g100-c, which the book calls a heating outlet. Not in the data.
15. **0005-8M also has ПОТРЕБИТЕЛИ ОСН. — КОНТР. and КОРРЕКЦИЯ ОСН. — КОНТР.** (p51), which PLANNED does not list
    (it has only КОРРЕКЦИЯ ВКЛ.). The air gauge's dial is white in a chrome rim, not black. ОБРЫВ ФАЗЫ ГОН is right
    as it stands: the book notes' "ГОМ" is wrong (p96, p100).
