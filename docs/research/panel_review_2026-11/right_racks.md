# Right racks review: SP-1M, SP-2M (9S456), SP-3M-2

Reviewer pass, 12 Nov 2026, read-only. Worktree `C:\dev\SAM3D-wt\osa-book`.

Conventions:
- Pixel boxes are in the raw 600 dpi scans `out/recon/dryrun/photos/raw/gNNN-x.jpg` as (x0,y0)-(x1,y1), or a point (x,y).
- Manual pages are the **printed** page, which is the `=== PAGE n` marker in `text.txt` minus 1. The data's citations
  already use this convention (for example soc.training "p49" is the text's PAGE 50).
- Certainty: **sure** (letters clear), **likely** (shape and context agree), **candidate** (best guess, could be wrong).
- The photographed vehicle is the ex-NVA **9A33BM3-1** (rack plate СП-2М2, block suffixes -М1). The manual is the 1991
  textbook for the 9A33BM3 (racks СП-1М1, СП-2М1, СП-3М1). Neither is the 1980s Osa-AK, so wherever they differ from the
  data the difference is reported, not taken as an error.
- Crops are in `scratchpad\review\right_racks\`, made by `rr_crop.py` (a descreen blur, then upscale and autocontrast). The
  shared `scratchpad\review\crop.py` was overwritten mid-session by another reviewer, so no crop here depends on it.

---

## 1. SP-1M (`sp1m_panel`)

The data has five blocks, no controls, lamps or displays ("no control of this rack is worked in the game").
Photos: g090-a (mostly hidden behind the R-123M and its cover); g090-b, a line drawing whose plates are not legible.

### 1.1 Legends read
| Photo | Box | Text | Certainty |
|---|---|---|---|
| g090-a | (1400,300)-(2110,480) | fuse plates "5А У27", "5А У..", "5А У31", "5А У33"(?), "5А У30"(?), "5А У.." | likely (the numbers are candidates) |
| g090-a | (1700,380)-(2000,470) | jacks "+6,3В У32"(?), "+6,3В У.." and "КОРПУС" | "КОРПУС" sure; "+6,3В" likely |
| g090-b | plates at (995-1110, 95-125), (2240-2290, 105-125) and others | block plates, 4-5 characters | not legible ("0093-1.." for the rear top is a candidate only) |

### 1.2 In the photo, not in the data
- **A small signal lamp over every fuse holder**: about 6 on the front top block and 7 on the rear top block (drawing
  g090-b, and g090-a's lenses over the orange caps). The data has no lamps at all.
- The rear top block (the data's 0093-19) carries "+6,3В" filament jacks and КОРПУС. This fits 0093-19 as the filament
  supply (manual p38), so it **supports the data's front/rear assignment**.
- The drawing shows a row of 5 fuses and 5 holders with jacks on the lower left block (0092-9М), and on the tall
  0065-2М two groups of 4 connectors on strips with about 20 jacks. None of these is drawn as a part in the data (the
  mesh may carry them; the rack is `bare`).

### 1.3 In the manual, not in the data
- p38: 0065-2M takes anode and grid power from **0093-14А** and filament power from **0093-19**. This agrees with the data.
- p138: the SPK's **encoder and its power supply are in SP-1M1**, that is OK82-2M (p141, the encoder) and 0092-9M (p144:
  "the encoder is powered from block 0092-9M"). This agrees with the data.
- p54 and p64 (9A33BM3): **0092-9M1 also makes +5 V for the IFF** (1L24-2). This is the AKM's arrangement, so note
  it only.
- p26: SOC blocks are in SP-1M1 (0065-2M, the canceller ЧПК, is common to SOC and SSC). Its АРУ — РРУ and gain controls
  are already listed in PLANNED (`p:0065`): not new.
- p52 printed: during the functional check the crew confirms that **the power blocks' fuse signal lamps are dark**.
  The fuse lamps above are therefore crew-relevant. Not modelled anywhere in the data (no fuse lamp in
  `osa_ak.json`; "предохран" occurs 0 times in the cabin and procedures data).

### 1.4 Data contradicted
- Nothing contradicted. The block names stay unverified: no plate is legible in either source (I3 stays open).

---

## 2. SP-2M (`sp2m_panel`, the 9S456 computer and the HV distribution)

Photos: g092-a (whole wall), g092-d (rack plate, OR-31M, OR-11M close), g092-e (OR-22M1, OR-13M1, 0091-17M1, 0091-18),
g098-a (the rear column's plates, per the notes).

### 2.1 Legends read
| Photo | Box / point | Text | Certainty |
|---|---|---|---|
| g092-d | (680,250)-(960,330) | rack plate **СП-2М2** | sure |
| g092-d | (1530,470)-(1680,530) | **ОР-31М** (top of the middle column) | likely |
| g092-d | (1500,1140)-(1680,1200) | **ОР-11М1** | likely |
| g092-d | (1460,1730)-(1680,1800) | ОР-2?М1 (the data's ОР-21М) | candidate |
| g092-d | bezels at (602,538), (916,564), (1230,564); legends under them about 120 px lower | **ψ**, **θ**, **Qc** from left to right, each "Г.О. 1ДЕА-100 ДУ / Т.О. 1ДЕА-1 ДУ" (coarse and fine counts) | ψ/θ/Qc sure; the counts likely |
| g092-d | (300,640)-(420,900) | key lock ОТКР./ЗАКР.; "220В 400Гц" beside the red and yellow fuse holder; "+27В(Д)" | likely |
| g092-d | (330,940)-(1700,1000) | "УПР. СИГН." with "ψ θ Qc ψθ", "+27В СРЕД."; jack pairs "1Δψ", "1Δθ", "1ΔQc"(?); "~36В θ ~36В ψ"(?), "-27В(Д)" | candidate |
| g092-d | (1634,904) | toggle **СС / ОТКЛ.** at OR-31M's right edge | sure |
| g092-d | (372,1320) | button **БАЛАНС** | sure |
| g092-d | (596,1397), (1461,1346) | two zero-centre meters **x10 μA** | sure |
| g092-d | (830,1210)-(1330,1400) | **cluster of 25 dark caps with two-line legends**, mostly ending in I or II. Candidates: "ΔД ВОЦ I", "НаА I", "КЗ I", "ΔБ/Δε I", "РУ I", "РУ II", "НЛЦ СУА"(?), "ЗАХВ. I"(?), "ВНИМ."(?) | candidate only |
| g092-d | (852,1538) | rotary **РУ I (РУ II)**, printed "РИ (РУБ)" in the notes | likely |
| g092-d | (1077,1474) | rotary **КОНТР. СРП**, about 12 positions; lamps "U1" and "U2" over it | sure (positions not legible) |
| g092-d | (615,1666), (1461,1564) | **ПРЕДЕЛ ИЗМ. I** and **ПРЕДЕЛ ИЗМ. II** (ОТКЛ., 100В, 50В, 10В, 5В, 1В, 0,5В, ...) | sure |
| g092-d | (1211,1602) | toggle **ОСВЕЩ. / ОТКЛ.** | sure |
| g092-d | (743,1679), (1339,1602) | lamps **U1** and **U2** | likely |
| g092-d | (1602,1250) | toggle **КАНАЛЫ I / II** | likely |
| g092-d | (1333,1423) | toggle "ГН"(?) / ОТКЛ. | candidate |
| g092-a | (190,150)-(960,400) | top front block: fuses **"СЛЕД. СИСТЕМЫ"** (3 × "1А"), lamps **"ПЕРЕГРУЗКА"** labelled "+60В"(?) and "-40В"(?), a vertical test socket | heading sure, voltages candidate |
| g092-a | (95,555)-(960,900) | second front block: yellow windows "t1"(?) and "t2"(?); **"ДЕЛИТЕЛИ НАПРЯЖЕНИЯ"** over 4 knobs; two toggles "ОТКЛ. I/ПУСК I"(?) and "ОТКЛ. II/ПУСК II"(?) | heading likely, the rest candidate |
| g092-a | blocks 3-4 of the front column, and 4-5 of the middle | white covers "СХЕМА РАСПОЛОЖЕНИЯ УЗЛОВ БЛОКА И ПОДКЛЮЧЕНИЯ УСИЛИТЕЛЕЙ", "БЛОК СИГН. И РЕЗЕРВ.", grips "НУО" and "РЕЗЕРВ" | sure (also in the notes) |
| g092-e | (2566,453)-(2775,478) | **0091-17М1** (Ш0465?) | sure |
| g092-e | (1034,597) | **ТРЕНИРОВКА** toggle with a white guard flap, under "СПК-I", key lock ЗАКР./ОТКР. below it | sure |
| g092-e | (2655,577) | **ТРЕНИРОВКА** toggle with a white flap, at the СПК-II end; "ОТКЛ." below | sure |
| g092-e | (1900,656) | meter **mA** with a green tag | sure |
| g092-e | (1860,935) | selector **ПЕРЕКЛ. ИСТОЧНИКОВ**, positions "ССЦ +6кВ", "СПК-I +18кВ", "СПК-I +600В-I", ... | likely |
| g092-e | (2069,915)-(2447,995) | table of currents: "ССЦ +6кВ / СПК-I +18кВ / 2,6±0,6(?)мА / 0,47±0,03мА"; "СПК-II +18кВ / СПК-II +600В / 0,47±0,03мА / 0,33±0,07(?)мА" | figures candidate |
| g092-e | 0091-17М1 fuse rows | headings "НАКАЛ" (5А), "-900В" (1А), "+300В +600В-I" (3А), "+250В +600В-II" (3А), "+18кВ" (5А) | likely |
| g092-e | (2029,1100)-(2188,1118) | **0091-18** | sure |
| g092-e | 0091-18 | halves **ССЦ** and **СОЦ**, each "НАКАЛ" (5А), "ПРЕДОХРАНИТЕЛИ", "+6кВ" (ССЦ) / "+16кВ"(?) (СОЦ) | likely |
| g092-e | (1495,1335), (1909,1316) | two toggles with **white guard flaps**, one per half, **with no legible legend of their own** | sure (the legend is absent or illegible) |

### 2.2 In the photo, not in the data
1. **ТРЕНИРОВКА — РАБОТА СПК-I and СПК-II**: two flagged toggles at the two ends of 0091-17М1 (g092-e (1034,597) and
   (2655,577)), with the clearest legends on the rack. Neither is in the cabin or the procedures, and neither is in PLANNED.
2. **OR-31M's ψ and θ scales**: the left and middle bezels. The data has only `or31_qc`, and it is on the wrong
   bezel (see 2.4).
3. **OR-11M1 (the SRP's control block) is wholly absent**: two μA meters, the 25-lamp signal cluster, РУ I/II,
   КОНТР. СРП, ПРЕДЕЛ ИЗМ. I/II, БАЛАНС, lamps U1 and U2, ОСВЕЩ., КАНАЛЫ. The nameplate is the only part in the data.
4. **The power blocks' fuse signal lamps**: every orange fuse cap on OR-95-1М (3), 0091-17М1 (about 18) and
   0091-18 (about 12) has a lamp lens. Two blocks also have **ПЕРЕГРУЗКА** lamps (OR-95-1М's 2), ОК52-15М has 3 small
   lamps each (×4 blocks; headings illegible), and ОС52-16М has 2 groups of 3 lamps and a fuse.
5. **OR-14М's** two windows, ДЕЛИТЕЛИ НАПРЯЖЕНИЯ knobs and two toggles (legends candidate).
6. **0091-17М1's current table plate**: a plate of the nominal currents for the mA meter, a natural texture item.
7. Every block has key locks ЗАКР./ОТКР. with red wire seals. Presumably these are in the mesh; not checked.

### 2.3 In the manual, not in the data (and whether the system is modelled)
| Item | Page | Modelled? |
|---|---|---|
| **ТРЕНИРОВКА — РАБОТА СПК-I, СПК-II** on 0091-17M (lowered high voltage while the magnetrons are trained) | p149 | The magnetrons' training is not modelled; soc.training and ssc.training already stand placed and unread, so the two СПК switches fit the same pattern. **New.** |
| 0091-17M: КОНТРОЛЬ НАПР. switch and meter (SOC +16 кВ; SSC +6 кВ; СПК +600 В and +18 кВ) | p49, p98, p150 | Already in PLANNED `p:0091_17`. The photo's selector legend reads **ПЕРЕКЛ. ИСТОЧНИКОВ** and the meter reads **mA** (currents), while the manual says КОНТРОЛЬ НАПР. (voltage). Record both. |
| OR-11M1 signal табло: **ВНИМАНИЕ** and **ЦЕЛЬ В ЗОНЕ** (p122), **УЗКИЙ ЛУЧ** and **К3** (p136), **НЛЦ СУА** (p137); its meters and switches for checking the SRP | p122, p136-137 | The launch zone, ВНИМАНИЕ, the narrow beam and НЛЦ are modelled (0004-11M's табло), so these OR-11M1 repeaters could be lit from the same state. The SRP check itself is maintenance. **New.** |
| OR-31M: switch СС — ОТКЛ. and **the follow-up systems' scales ψ, θ, Qc** | p51 | ψ and θ: the gyro vertical exists (`s:gyro_erected`), so they could turn with it or stay still. Only Qc is built. |
| OR-14M1, the time mechanisms: they **must be zeroed for ПУСК ГОТОВ**; СБРОС returns them | p120, p154 | Not modelled as a part (an automatic condition). Note only. |
| OS52-16M: the frequency-control node УКЧ (sheds the load on a low supply frequency) | p163 | Not modelled. Note only. |
| Functional check: the power blocks' fuse lamps must be dark | p53 | Not modelled (see SP-1M 1.3). |
| SRP composition: OR-11M1, OR-12M1 ×2, OR-13M, OR-22M, OR-21M1 (with sub-block 00-95-2BM), OR-14M1, OR-15M1, OR-95-1M1 | p121 | Agrees with the data's 10 OR blocks. The plates in the data read "ОР95-1М"; the manual writes **ОР-95-1М1** (with a hyphen). |
| Racks: SP-2M1 holds OS52-16M (the SSC's magnetic amplifier) and 0091-18 (p67); SVR's control and magnetic amplifiers (p105); SRP (p121); 0091-17M (p138) | as given | Agrees with the data. |
| Ventilation: the lower sections of SP-2M1 and SŁ-2M1 are cooled by fans on the wheel arches' slope | p167 | Not checked here (vent covered by procedures line 527). |

### 2.4 Data contradicted
1. **`or31_qc` sits on the θ bezel, not on Qc.** In g092-d the three windows are legended **ψ, θ, Qc from left to
   right**. The data puts Qc in the middle bezel (at_m x −0.0515, "which bezel is the Qc scale: SAM3D's choice"). By
   sp2m_trace's M0 bezels (763.5 / 893.5 / 1026.9 mm, block centre 948.6 mm ↔ x 0.0036), Qc belongs at
   **x ≈ 0.082** with the same y 0.5756.
2. **ssc.training and soc.training on 0091-18**: the manual confirms the switches are on 0091-18 (p49, p97). The
   toggles the data uses are the white-flagged toggles at (1495,1335) and (1909,1316), but **no ТРЕНИРОВКА legend is
   visible next to them**, whereas 0091-17М1 shows ТРЕНИРОВКА plainly. Their identification remains SAM3D's choice;
   keep it, but cite g092-e honestly.
3. **A stale procedures note**: soc.training's note (procedures line 585) says "at the upper toggle **left** of the
   divider" and ssc.training's says "**right**". The cabin (and the photo: ССЦ on the left half, СОЦ on the right) has
   them the other way round. The cabin is right; the procedures notes need flipping.
4. The rack plate reads **СП-2М2** (the BM3-1) and the manual **СП-2М1**; the data's label "SP-2M" is fine for the AK.

---

## 3. SP-3M-2 (`sp3m_panel`)

The data has seven blocks, named by Fig. 3's order ("an assumption"), and no controls, lamps or displays.
Photos: g098-a, g098-b, g098-c (front flap folded down); g098-d, the drawing.

### 3.1 Legends read
| Photo | Box / point | Text | Certainty |
|---|---|---|---|
| g098-d | (880,170)-(1060,225) | plate **0094-1?М** (the data's 0094-11М) | likely |
| g098-c | (188-573, 344) | jacks **КОРПУС, +100В У6, +27В У5, -27В У4** | sure |
| g098-c | (385,164) | a red twin-lens lamp holder, a white lens to its right; the drawing's heading over it reads "...ПРЕДОХРАНИТЕЛ..."(?) | heading candidate |
| g098-c | (704-1048,164-540) | hinged cover **ПРЕДОХРАНИТЕЛИ** (drawing: ПР2, ПР3) | sure |
| g098-c | (1179-1375,213-352) | fuses **"1А У4"**, **"3А СЕТЬ"** ×2 | likely |
| g098-c | two locks | ЗАКР. / ОТКР. | sure |
| g098-c | (1539-2357,164-622) | a drawer with a clear front and two handles; a stamp "...6-2"(?) | illegible |
| g098-d | top-right box | 12 large sockets numbered 1-12 and 12 smaller numbered 1-12 on a black field, terminal strips under them | sure (drawing) |
| g098-c | (0-770,737-1555) | lower left block: two toggles "ОТКЛ."; knob **ЧАСТОТА ГЕНЕРАТОРА**; a calibration chart with a red note "...В СЕКТОРЕ 0-50(?) НЕ РАБОТАТЬ!"; socket "Ш3" with "ДС" and "АРЧ"(?); note **"ПО ОКОНЧАНИИ НАСТРОЙКИ ПЕРЕКЛЮЧАТЕЛЬ ПРИБОР УСТАНОВИТЬ В ПОЛОЖЕНИЕ ОТКЛ."**; meter "x10 μA"; knob **"УРОВЕНЬ ВЫХОД. ВЧ СИГНАЛА"** with a small scale; selector ПРИБОР ("ОТКЛ.", "УРОВЕНЬ ..."(?), "ТОК ДЕТЕКТ. ГЕТЕРОДИНА"(?), "ТОК СМЕСИТЕЛЯ"(?), "ТОК ПЕРЕ..."(?)) | headings likely, positions candidate |
| g098-c | lower blocks 2-4 | black data plates, coax connectors, a 15-pin orange socket, fuses, "ГЕНЕРАТОР"(?) | illegible |

### 3.2 In the photo, not in the data
- On 0094-11М: the lamp holder, four jacks, the ПРЕДОХРАНИТЕЛИ cover with ПР2 and ПР3, three fuses and two locks. None is
  in the data (the mesh may carry them; the rack is `bare`).
- **Five units in the lower row, not four.** The drawing g098-d shows them at about x −0.55…−0.28, −0.26…−0.07,
  −0.06…0.15, a **narrow unit at 0.16…0.22 (one round connector)**, and 0.23…0.56 (panel frame). The data's 4РЗМ-2
  (0.164…0.514) swallows the narrow unit.
- **The top right is one drawer** (clear front, two handles; in the drawing, the 12+12 socket field). The data splits
  it into two blocks, 12РЗМ and 6РЗМ-2. For the AK this may be right (two blocks behind one door), but the photo cannot
  show it.
- The louvred front flap, which folds down as a shelf. Already noted in OSA_INTERIOR_HOLISTIC (§470).

### 3.3 In the manual, not in the data
- p53-54 (9A33BM3): **the IFF is 1L24-2** ("Пароль"/"Кремний-2М"). Its blocks are УО030100 (receiver),
  УО070100 (encoder-decoder), УО080100 (reply simulator), УО020100-А (master oscillator), УО020400-А (feeder) and the
  crypto unit 6110-23 (912-3). They sit "in the frame for the IFF and block **0094-11M1**", which confirms that
  0094-11M shares this rack. The **key-input device with its key plates is kept apart, in the secret section**. On the
  photographed BM3-1 the lower left block (ЧАСТОТА ГЕНЕРАТОРА, ВЫХОД ВЧ СИГНАЛА) is likely УО020100-А (a candidate). The
  data's РЗМ names (mon_2112_80, the BM2) **cannot be checked against these photos**: a different interrogator.
- p85, p113: **0094-11M (-1) powers the SSC's antenna drives (СУА) and the SVR's drives**: a power block, not IFF.
  The panel label "IFF, power" is fine.
- p105: SP-3M1 holds power blocks for the SVR.
- No crew action on SP-3 in the 1991 text, apart from the IFF's switching on (from 0096-9M1 and ОП81-16М2, not here) and
  the code keys (kept apart). The data's "drawn only" is right for the game.

### 3.4 Data contradicted
- Nothing firmly contradicted. The block naming stays an assumption (I3), and the lower row's 4-versus-5 split
  differs from the drawing (3.2).

---

## Top findings (ranked)

1. **`or31_qc` is on the wrong bezel.** g092-d legends the OR-31M windows ψ, θ, Qc from left to right; the data
   draws Qc in the middle bezel (the θ). Move it to the right bezel, x ≈ 0.082 (trace M0 bezel at 1026.9 mm).
2. **ТРЕНИРОВКА — РАБОТА СПК-I and СПК-II are missing.** They are clearly legended toggles at both ends of 0091-17М1
   (g092-e (1034,597), (2655,577)) and named in the manual (p149). Add them like soc.training and ssc.training (placed,
   unread); not in PLANNED.
3. **OR-11M1's signal табло and meters are absent.** The 25-lamp cluster, two μA meters, РУ I/II, КОНТР. СРП,
   ПРЕДЕЛ ИЗМ. I/II, БАЛАНС, U1/U2, ОСВЕЩ., КАНАЛЫ (g092-d). The manual (p122, p136-137) says ВНИМАНИЕ, ЦЕЛЬ В ЗОНЕ,
   УЗКИЙ ЛУЧ, К3 and НЛЦ СУА repeat on it: states the sim already has.
4. **Fuse signal lamps are modelled nowhere.** Every fuse holder on SP-1M, SP-2M and SP-3M has a lamp, and the
   functional check requires the power blocks' fuse lamps to be dark (p53). Also OR-95-1М's ПЕРЕГРУЗКА lamps.
5. **No ТРЕНИРОВКА legend at the 0091-18 toggles the data uses** for ssc.training and soc.training (white-flagged
   toggles, g092-e (1495,1335) and (1909,1316)). The manual puts the switches on 0091-18, so keep them, but say the
   photo shows no legend there.
6. **Stale procedures notes.** soc.training's says "left of the divider" and ssc.training's says "right"; the cabin
   and the photo have СОЦ right and ССЦ left.
7. **OR-31M's ψ and θ scales are not built.** The manual (p51) says the block carries the follow-up systems' scales;
   the gyro vertical exists in the sim.
8. **0091-17M's check reads ПЕРЕКЛ. ИСТОЧНИКОВ with an mA meter** and a nominal-currents table on the BM3-1; the manual
   calls it КОНТРОЛЬ НАПР. (voltages). Record both on the PLANNED `p:0091_17` row.
9. **SP-3's lower row has five units** in g098-d (a narrow one at x 0.16-0.22); the data's 4РЗМ-2 covers two of them.
10. **SP-3's photographed IFF is the 1L24-2** (УО0x0100 blocks, 6110-23, p53-54), not the AK's РЗМ set. The РЗМ
    naming cannot be checked from these photos. The lower left block (ЧАСТОТА ГЕНЕРАТОРА, ВЫХОД ВЧ СИГНАЛА) is likely
    УО020100-А.
11. **0094-11М's face parts** (g098-c/d): a lamp holder, jacks КОРПУС, +100В У6, +27В У5, -27В У4, the ПРЕДОХРАНИТЕЛИ
    cover (ПР2, ПР3), fuses 1А У4 and 3А СЕТЬ ×2. The plate reads 0094-1?М (likely -11М).
12. **SP-1M's rear top block has +6,3В filament jacks** (g090-a), which supports 0093-19 as the filament supply there
    (manual p38). SP-1M holds the SPK encoder and its supply (p138, p144), as the data has it. No plate is legible.
13. **OR-14М's** windows, ДЕЛИТЕЛИ НАПРЯЖЕНИЯ knobs and two toggles are not in the data. The manual: its timers must be
    zeroed for ПУСК ГОТОВ (p154). Low priority.
14. **Plate spelling**: the manual writes ОР-95-1М1 with a hyphen; the data's nameplate reads "ОР95-1М".
