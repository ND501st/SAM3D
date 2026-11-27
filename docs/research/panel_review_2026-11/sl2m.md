# SŁ-2M review: op_panel, so_panel, fo_panel (12 Nov 2026)

> **Page numbers in this file** are the manual text file's `=== PAGE n` markers, one above the printed page that the rest of
> the project cites (PAGE 162 = printed p161). Subtract one before citing. Block heights here are rough estimates; the
> rows' measured heights are in `../OSA_PANEL_REVIEW_2026-11.md` section 1.


Read-only review. Photos: wwp_gecko g094-a/b/c, g095-a/b/c, g093-a (ex-NVA 9A33BM3-1), rwd_mb3 9a33_42/43/44 (another
BM3, low res, same layout), SAM Simulator `columns_three.jpg` and `exe_img/j_0068..`, `j_0076..`, `j_0628..` (the AK
columns, 1 mm/px). Manual: voenizdat_osa_1991 `text.txt`. Page numbers are the file's `=== PAGE n` markers (the printed
folio is one lower, e.g. PAGE 162 = printed 161), as the project already cites them.
Pixel boxes: full-resolution raw scans (`out/recon/dryrun/photos/raw`). Panel coordinates: x, y in metres from the
panel centre (+x forward / image right, +y up), as `at_m`.

**How the photos map to the panels.** SAM Sim's texture → panel: rear `x = col - 0.270` (checked on 0062's five fuses:
SAM Sim -0.048..+0.018, data -0.042..+0.010), middle `x = col - 0.340` (0094 plate), front `x = col - 0.305`;
`y = 0.491 - row`. Block gaps found by an intensity profile of `columns_three.jpg` (rows of dark gaps): rear 41-59,
317-334, 482-494, 643-659, 932; middle 49-79, 516-532, 682-685, 930-948; front 58, 357-374, 638-641, 901-917.

**Block heights.** On g095-a (straight on, sharp) the OP81-16M3 face is 1500 px tall for 450 mm and 2210 px wide for
~666 mm: 0.30 mm/px both ways, so the 0094 face (460 px) is **~138 mm**. On g094-a the vertical scale changes down the
console (local mm/px from blocks of known height: TV 0.59, 0004-11M 0.59, OS52-14M 0.69; OP81 0.73, OP63 0.80, OS52-12M
0.82), so the top and bottom blocks are taken from their neighbours' scale and the width (aspect corrected): ±15 %.

| Block | Real height (est.) | Shown on the texture (SAM Sim) | Data block | Missing beyond the panel edge |
|---|---|---|---|---|
| 0062-6M(1) (rear top) | ~150 mm (g094-a 210 px x ~0.72) | bottom 45 mm (gap at row 41-59) | 107 mm, 0.384..0.491 | ~40-100 mm above (see finding 12) |
| 0094-5/-9 (middle top) | ~138 mm (g095-a), 140-150 (g094-a) | bottom 49 mm | 65 mm, 0.426..0.491 | ~90 mm above |
| 0004-14M (front top) | ~130 mm (g094-a 225 px x 0.57-0.59; 120-150 by other reads) | bottom 58 mm | 57 mm, 0.434..0.491 | ~75 mm above |
| 0051-7M (rear bottom) | ~250-300 mm (g094-a 315 px; the rear column is oblique, low confidence) | top 50 mm | 61 mm | ~200-240 mm below |
| OK81-4M-I (middle bottom) | ~280 mm (g094-a 345 px x 0.82; aspect 0.40 x 666 x 1.07) | top 34 mm | 33 mm | ~250 mm below |
| OK51-6M (front bottom) | >=245 mm (g094-a 340 px, bottom at the scan's edge) | top 65 mm | 64 mm (two blocks) | >=180 mm below |

The holistic pass's "about a third and a half of the PPI block" (150 and 225 mm) agrees for the top; the bottom row reads
taller here. A middle column of 138 + 450 + 144 + 253 + 280 + gaps is about 1.30 m, against the panels' 0.982 m: the
panels need ~0.09 m more at the top and ~0.25 m at the bottom, which must be checked against the bench, the floor plinth
and the roof lining in the cabin (the bottom row may stand in the hull's chamfer; g094-a shows chamfered lower corners
and 0051-7M set in from the aft one).

---

## Top row

### Rear column: 0062-6M1 (op_panel) - g094-b (best), g094-a, SAM Sim row 0-45

Block: "блок АРУ (ОО62-6М1)" (p151). Manual (p50, p51, p99, p151, p162): switch **ТОК, МОЩНОСТЬ** and meters **МОЩНОСТЬ
ГЕНЕРАТОРА**, **ТОК ГЕНЕРАТОРА** (magnetrons' mean power and current, СОЦ, ССЦ, СПК-I/II); switches **КАНАЛЫ**, **РЕЖИМЫ**,
meter **КОНТРОЛЬ**, button **ПОДЖИГ ГШ** (noise-figure check: noise from ГШ-2/ГШ-5 through the receivers, read on КОНТРОЛЬ,
p162), knobs **РРУ КШ ОСН.**, **РРУ КШ ДОП.**, switch **АРУ — РРУ** (switches off the ССЦ's AGC for maintenance, p99).

Face, aft to forward (g094-b crop of box (352,156,1809,860); panel x from the SAM Sim labels):
- aft end: a coaxial (BNC) socket, low left (~x -0.29, y ~0.43).
- **meter 1** (black case, black hood): label under it "...ОЩНОСТЬ ГЕНЕРАТОРА СОЦ, ССЦ, СПК I, СПК II" (g094-b, SAM Sim
  row 22 cols 0-95: certain) = **МОЩНОСТЬ ГЕНЕРАТОРА**, x ~ -0.21. Scale not legible ("x10 µA" possible, doubtful).
- two black knobs stacked right of it (~x -0.15): the upper with an arc "СОЦ, ССЦ, ..." and the legend "ТОК, МОЩНОСТЬ"
  above (g094-b, medium); the lower with "РЕЖИМЫ" under it (SAM Sim row 18, col 130 → x -0.14, certain on the AK).
- a single jack "⊥" (SAM Sim col 187 → x -0.083, row 18 → y 0.473).
- a column of three round push buttons and a small toggle (~x -0.10): legends "ДР", "ТОК ДЕТ.", others unread (g094-b, low).
- a large black pointer rotary with an arc of legends (~x -0.07, upper half of the block): unlabelled in my reads;
  candidate **КАНАЛЫ** (the only named switch left), low.
- "ПОДЖИГ ГШ" orange-capped button and a toggle "ОСН./ОТКЛ." beside it (g094-b (840-960, 590-680), legend medium).
- five orange fuses "-6,3В +6,3В +6,3В +27В +150В" (SAM Sim row 22: certain on the AK; g094-b reads "+6,3В,+24В..." on the
  BM3), x -0.048..+0.018. In data.
- **meter 2** (black case, black hood) "x10 µA", label **КОНТРОЛЬ** under it (SAM Sim col 345 → x +0.075; g094-b: certain).
- two black knobs stacked right of meter 2 (SAM Sim col ~420 → x ~+0.15): legend "РРУ КШ ДОП." under them (g094-b, medium)
  = **РРУ КШ ОСН. / РРУ КШ ДОП.**
- **meter 3**: white case, black hood, scale "mA", green tag at its lower right, label **ТОК ГЕНЕРАТОРА СОЦ, ССЦ, СПК I,
  СПК II** (SAM Sim cols 440-545 → x +0.17..+0.28; g094-b certain), x ~+0.17.
- plate "ОО62-6М1" + serial box (top right, g094-b), key lock ОТКР./ЗАКР. at the forward end.

### Middle column: the dial strip 0094-9 (BM3) / 0094-5 (AK) (so_panel) - g095-a (excellent), SAM Sim row 0-49

- Rack frame's top rail above it: plate **"СЛ-2М2 Ш0410"** (g095-a (760,40,1040,95): certain).
- Block face ~138 mm tall; a raised rounded-rectangle bezel ~124 mm tall across almost the full width (g095-a
  (200,130,2110,570)); aft end a key lock; small black plate at the lower left "0094-9" (g095-a (100,540,200,580), medium;
  SAM Sim reads "0094-5", the data's nameplate (-0.31, 0.436) matches its place).
- **Eight round pointer dials, not digit counters**: each a cream/amber dial ~50 mm across with printed scales and a
  needle behind glass, pitch ~65-70 mm, centres ~87 mm above the block's bottom (panel y ~0.52-0.53), x (from SAM Sim's
  captions) -0.245, -0.175, -0.110, -0.033, +0.025, +0.092, +0.155, +0.225. Captions, left to right (g095-a and SAM Sim:
  certain): **Δβ, Δε - "1 ДЕЛ. 0-20" - СВР II**; **Г.О. 1 ДЕЛ. 1-00, Т.О. 1 ДЕЛ. 0-02 - АЗИМУТ ССЦ**; **Г.О. 1 ДЕЛ. 1-00,
  Т.О. 1 ДЕЛ. 0-02 - УГОЛ МЕСТА ССЦ**; **Δβ, Δε - "1 ДЕЛ. 0-20" - СВР I**. Dial figures legible in part ("300-400", "40 30",
  "0 60", "10 5", "60 40 20").
- Right end: a glass fuse holder (white cap) over the legend **"1А ~150В"**, a small plate "70606"(?) above it, and below
  it the block's key lock with a red tag (g095-a (2140,150,2240,430)).
- Manual: p251 the commander reads the antenna's azimuth βу "со шкалы блока 0094-9" when orienting by the aiming circle
  (so the strip is the ССЦ angle readout); p82, p86, p114: 0094-9 is also a power-supply block (ВСО ССЦ, СУА, ВСО СВР).

### Front column: 0004-14M (fo_panel) - g095-b (good), g094-a, SAM Sim row 0-58

Manual (p162, holistic section 49): РЕЖИМЫ, ВЫВОД — НАВЕДЕНИЕ, the four meters, ЧАСТОТА (function-check block).

BM3 (g095-b, box (318,159,1891,876)), aft to forward:
- key lock ЗАКР./ОТКР. (aft end, ~x -0.27), a small lamp/screw above;
- toggle **ОСВЕЩ. / ОТКЛ.** (certain), two small round items below it;
- four meters, black square cases ~80 mm, scale "x10 µA" centre zero (-4..0..+4), in two pairs, each pair under a white
  half-round **hood** (meter lamp shade) on a bracket; labels **СО ε, КАНАЛ Ц, СО β** and **СО ε, КАНАЛ Р, СО β** (certain);
- three red caps in a row (test jacks or fuses) **"+?В", "-150В", "КОРПУС"** (first figure unread: "+12В"/"+26В"
  candidates) and a small lamp/jack beside КОРПУС;
- bat lever **ВЫВОД — НАВЕДЕНИЕ**; bat knob **РЕЖИМЫ** with arc legends "СМЕНА ЗНАКА СО ε,β", "СО ε,β", "КОМПЕНС. ε,β",
  "ПП(?) СОЦ" (medium); round knob **ЧАСТОТА** under it; plate "ОО04-14М" + serial; key lock (forward end).

**AK (SAM Sim, j_0628 row 0-58) is laid out differently**: key lock "ОТКР." at col ~45 (x -0.26); **РЕЖИМЫ** knob at col
~100 (x -0.205) and **ЧАСТОТА** at col ~145 (x -0.16), both left of the meters; the four meter cases at cols 170-250,
255-335, 342-422, 427-507 (centres x -0.095, -0.010, +0.077, +0.162), labels СОε КАНАЛ Ц СОβ, СОε КАНАЛ Р СОβ;
"ОТКЛ." at col ~530 (x +0.225) and "НАВЕДЕНИЕ" at col ~565 (x +0.26) at the right (the ВЫВОД — НАВЕДЕНИЕ switch).

---

## Bottom row

### Rear: 0051-7M (op_panel) - g094-a (270..)/(395,1652,892,2007), g095-c, SAM Sim row 932+
- SAM Sim (AK): the block starts ~75 mm in from the aft edge (x ~-0.195 to the forward edge); plate "0051-7М" + "Л0710"
  at x +0.235..+0.285 (in data, 0.269); **aft of it a separate small unit with "ОТКЛ." and red-capped jacks** (cols 0-70,
  x -0.27..-0.20), not in data.
- BM3 (g094-a): aft key lock; in the middle a black **pointer knob** (the five-position switch) with an arc of legends
  above it (illegible at the scan's halftone); left of the knob a pale engraved **data card** (~130 x 45 mm); right a
  **vertical red multi-pin test socket** (~8 contacts with legends both sides, illegible); forward key lock; plate top right.
- Manual: p99 (СИД section, "Схема синхронизации БМ"): on 0051-7M a **five-position switch АВТ., РУЧН. Р-Р, РУЧН. F1-F3,
  F3, Р**, switching the vehicle's repetition frequencies in СДЦ against second-trace clutter; p108, p114: 0051-7M holds the
  ВСО СВР **guidance channels** ("Узкий луч") of both missile channels.

### Middle: OK81-4M-I (so_panel) - g094-a (892,1723,1723,2128)
Panel x from g094-a (column 830-1700 px, 0.766 mm/px), y below the block's top (panel -0.457):
- aft key lock (x ~-0.30);
- **vertical red 30-pin test socket**, labelled contacts both sides (x ~-0.165, y -0.47..-0.67); legends illegible here
  (on OK81-4M-II, g091-a: "ИМП. ЗАП.", "ВИДЕОСИГН.", "КОНТР. ВИДЕОСИГН.", "+150В", "-150В", "+250В");
- small slotted control (**БАЛАНС**, p120) at x ~+0.03, y ~-0.50; a small white rectangular window/toggle at x ~-0.035,
  y ~-0.60; **red button in a beige ring = ВОЗВРАТ** (p120) at x ~+0.03, y ~-0.64;
- **white hinged cover box** ~150 x 170 mm at x +0.09..+0.24, y -0.51..-0.65 (four holes in its lid);
- plate "ОК81-4М" + serial at x ~+0.26, y ~-0.47, and a tag **"КАНАЛ I"** under it (SAM Sim row ~975: "КАНАЛ I", certain;
  data has no tag); forward key lock (x ~+0.29, y ~-0.62).
- Manual: p112-113 OK81-4MI and -4MII are the missiles' range trackers (one per channel); p120 ВОЗВРАТ ("Сброс", tracker
  back to its start) and БАЛАНС (channel balance); p155 the trackers must be at their start for ПУСК ГОТОВ.

### Front: OK51-6M + fuse panel (fo_panel) - g094-a (1733,1804,2878,2189), SAM Sim j_0628 row 917+
- BM3 (g094-a; column 1700-2880 px, 0.517 mm/px): **one OK51-6M** about two thirds of the width (x -0.30..+0.09): aft key
  lock (x ~-0.26), vertical red test socket with ~6 legends right of it (x ~-0.22), a large engraved aluminium **data
  plate** (~150 x 45 mm, x -0.18..-0.03, 3-4 lines, illegible), plate "ОК51-6М" + serial (x ~0..+0.05), forward key lock
  (x ~+0.06). Forward of it a **separate fuse panel** (x +0.10..+0.21; box (2470,1880,2760,2200)): **three large fuse caps**
  in a row, under them **three pairs of black jacks** (legends "+27В"(?), "+24В"(?)), under the first pair two beige jacks
  with legends (illegible).
- AK (SAM Sim): likewise **one** OK51-6M from x -0.297 to ~+0.205, plate "ОК51-6М"/"Л0717" at x ~+0.12..+0.18, and a
  separate unit from x ~+0.22 to the edge.
- Manual: p107-108, p113-114, p117: OK51-6M is the ВСО СВР (missile channels' error-signal extraction) for capture and
  ВЫВОД, both channels; fed by 0094-9 and ОС94-10.

---

## op_panel

1. Legends read: see Top/Bottom above; also g094-b "ОС81-9М", "ОК62-5М1", "ОС61-5М1", "ОС81-18М1" plates (certain).
2. In the photo, not in the data:
   - the white **ТОК ГЕНЕРАТОРА mA meter** as a display (only a fixture, (0.168, 0.453));
   - the **КОНТРОЛЬ** meter as such (the data reads it as МОЩНОСТЬ, see 4);
   - **РРУ КШ ОСН./ДОП.** pair right of the КОНТРОЛЬ meter; the **⊥ jack**; the coaxial socket at the aft end; the three push
     buttons/toggle column ("ДР", "ТОК ДЕТ."); ПОДЖИГ ГШ's companion toggle; the plate "ОО62-6М1"; the meters' black hoods;
   - 0051-7M's five-position switch, data card, red test socket, key locks; the small "ОТКЛ." unit aft of 0051-7M (SAM Sim).
3. In the manual, not in the data: КАНАЛЫ, ПОДЖИГ ГШ, КОНТРОЛЬ (already PLANNED); **РРУ КШ ОСН., РРУ КШ ДОП., АРУ — РРУ**
   (p99) - new; function check modelled (`function_check`) but no noise check; 0051-7M switch (PLANNED, `ssc_transmitter`).
4. Data contradicted:
   - **`tx_current` (ТОК) sits on the МОЩНОСТЬ ГЕНЕРАТОРА meter (-0.213) and `tx_power` (МОЩНОСТЬ) on the КОНТРОЛЬ meter
     (0.052)**; ТОК ГЕНЕРАТОРА is the white mA meter at +0.17 (SAM Sim labels and g094-b, certain). Should be: МОЩНОСТЬ at
     -0.213, ТОК at +0.168, КОНТРОЛЬ at 0.052.
   - `ssc.control_sel` "КОНТРОЛЬ" (0.129, 0.413): КОНТРОЛЬ is the meter, no switch of that name (p51, p99); the knob there is
     one of РРУ КШ ОСН./ДОП.
   - `ssc.modes` РЕЖИМЫ (-0.072, 0.408) sits by the "⊥" jack; РЕЖИМЫ is the lower of the stacked knobs at ~x -0.15
     (SAM Sim label col 130). `mon.tx_select` (-0.128, 0.431) is on "the large knob between the meters"; the photos put
     ТОК, МОЩНОСТЬ on the upper stacked knob by meter 1 (medium) and the large rotary is more likely КАНАЛЫ (low).
   - Block boxes vs the texture: data's rear blocks sit up to ~60 mm lower than SAM Sim's straight photo at the top
     (0062 bottom 0.384 vs 0.450; OS81-9M 0.133..0.372 vs 0.174..0.432; OK62 top 0.119 vs 0.157; ПЕРЕДАТЧИК button 0.266
     vs ~0.32) while the bottom agrees (АВТОМАТ -0.253 vs -0.249). If the face texture is SAM Sim's, blocks and items disagree
     with it; if it is the g094-a flat, the note should say so.
   - `headset.operator` jack (0.23, -0.391): no ШЛЕМОФОН jack there in any photo (SAM Sim: the ПЕЛЕНГ oval's sockets).

## so_panel

1. Legends read (g095-a, certain unless noted): "СЛ-2М2 Ш0410" (760,40,1040,95); the eight dials' captions (200,380,2050,
   470); "1А ~150В" (2080,400,2150,440); "0094-9" (100,540,200,580, medium); "ОП81-16М3 Ш0505" (1950,640,2200,690);
   "ОП63-6М3" (1940,2300,2120,2340). SAM Sim: "0094-5", "ОК81-4М Л0538"(?), "КАНАЛ I".
2. In the photo, not in the data: the strip's **pointer dials and their scales** (drawn as digit counters), its captions,
   the fuse holder 1А ~150В, both key locks, the rack plate СЛ-2М2 on the top rail; OK81-4M-I's **red test socket, БАЛАНС,
   ВОЗВРАТ, white cover box, small window, КАНАЛ I tag, key locks**.
3. In the manual, not in the data: **ВОЗВРАТ and БАЛАНС of OK81-4M-I** (channel I; p120) - known (holistic 49 left them
   unplaced as below the face); `msl.return_2` exists for channel II, so ВОЗВРАТ for channel I is simply missing.
4. Data contradicted:
   - **`soc_counters` readouts**: data `range_error, elevation_error, az_coarse, az_fine, el_coarse, el_fine, none, none`
     with the note's captions "ΔД, Δε". The photos read **Δβ, Δε of СВР II; Г.О./Т.О. of АЗИМУТ ССЦ; Г.О./Т.О. of УГОЛ МЕСТА
     ССЦ; Δβ, Δε of СВР I**: dials 1-2 are missile channel II's angle errors (1 div = 0-20), not a range error; 3-6 are
     right in kind; 7-8 are СВР I's, not "none". "ΔД" is a misreading of Δβ.
   - **Kind**: they are scale (pointer) dials (Г.О. 1 дел. 1-00, Т.О. 1 дел. 0-02 are coarse/fine dial divisions in mils),
     not digit counters (so_panel note, `kind: counters`).
   - **Place**: dial centres are ~0.52-0.53 up; data draws them at 0.466 (size 0.05), over the texture's captions.
   - so_panel note: "lamp row 0094-9" then "0094-5"; the strip is dials, not lamps; the block on the AK is 0094-5 (D1).
   - `soc.top_toggle` (0.293, 0.462): the right end has the fuse holder and the block's key lock, no toggle.
   - `headset.senior_operator` (-0.302, -0.445): the photo has OK81-4M's aft key lock there.
   - OK81-4M block 33 mm (it is ~280 mm).

## fo_panel

1. Legends read (g095-b, certain unless noted): ОСВЕЩ./ОТКЛ.; СО ε / КАНАЛ Ц / СО β; СО ε / КАНАЛ Р / СО β; "x10 µA";
   "-150В", "КОРПУС" (first unread); ВЫВОД, НАВЕДЕНИЕ; РЕЖИМЫ arc (medium); ЧАСТОТА; "ОО04-14М" + serial. SAM Sim (AK):
   ОТКР., РЕЖИМЫ, ЧАСТОТА, СОε КАНАЛ Ц СОβ, СОε КАНАЛ Р СОβ, ОТКЛ., НАВЕДЕНИЕ; "ОК51-6М Л0717".
2. In the photo, not in the data: the meters' white **hoods** (two half-round shades) and black cases at full size
   (~80 x 75 mm; data gauges 66 x 42); the three red caps +?В/-150В/КОРПУС and the small lamp; the plate ОО04-14М; both key
   locks; OK51-6M's red test socket, data plate and locks; the **fuse panel's three fuses and eight jacks**.
3. In the manual, not in the data: nothing new on these two blocks (OK51-6M has no named control in the manual; 0004-14M's
   four controls are placed).
4. Data contradicted:
   - **0004-14M on the AK texture**: data follows the BM3 (meters at -0.152..+0.101, РЕЖИМЫ/ЧАСТОТА at the right). SAM
     Sim's AK has the meters ~57 mm further forward (centres -0.095, -0.010, +0.077, +0.162) and РЕЖИМЫ (x -0.205) and
     ЧАСТОТА (x -0.16) at the left. So on the AK face: `err.light` and `vent.workplaces` sit on the texture's ЧАСТОТА/
     РЕЖИМЫ area and `err.mode` (+0.252)/`err.freq` (+0.215) where the AK has ВЫВОД — НАВЕДЕНИЕ. Decision D1 (the AK keeps its
     own) would put the AK layout here.
   - **`vent.workplaces`** (-0.265, 0.477) is on the block's key lock (ЗАКР./ОТКР.) on both vehicles: every block has a lock
     at each end (g094-c, g095-b). p168's switch is not on 0004-14M in any photo.
   - Meters drawn at y 0.477-0.479 (gauge fixtures 0.464..0.506): real case centres ~0.51, hoods to ~0.56.
   - **Two OK51-6M blocks** (`g_ok51_6m` -0.30..+0.04 with a plate at -0.005, `g_ok51_6m_2` +0.05..+0.30 with a plate at
     +0.255): both photos show **one** OK51-6M (AK: -0.297..+0.205, plate +0.12..+0.18; BM3: -0.30..+0.09) and a separate
     narrow unit forward (on the BM3 a fuse panel). The data's first plate is where neither photo has one.

---

## Top findings

1. **Top and bottom rows are whole blocks, cut off**: 0062 ~150, 0094 ~138, 0004-14M ~130 mm tall (shown 45-58 mm);
   0051-7M ~250-300, OK81-4M ~280, OK51-6M >=245 mm (shown 33-65). Panels need ~+90 mm up and ~+250 mm down: check the
   bench, plinth and roof lining first.
2. **0062-6M1's meters are swapped/misassigned**: left = МОЩНОСТЬ ГЕНЕРАТОРА, middle = КОНТРОЛЬ, right white mA =
   ТОК ГЕНЕРАТОРА. `tx_current` is on the МОЩНОСТЬ meter and `tx_power` on КОНТРОЛЬ.
3. **The 0094 strip's readouts are wrong**: Δβ, Δε СВР II | Г.О., Т.О. АЗИМУТ ССЦ | Г.О., Т.О. УГОЛ МЕСТА ССЦ | Δβ, Δε
   СВР I. There is no ΔД (range) dial; the last two are СВР I, not spare.
4. **The strip is eight pointer dials with coarse/fine scales (1 дел. 1-00 / 0-02 / 0-20)**, not digit counters; p251
   reads the antenna azimuth "со шкалы блока 0094-9".
5. **The strip is drawn ~60 mm too low** (0.466 vs ~0.52-0.53), over its own captions.
6. **fo_panel's 0004-14M follows the BM3 layout over the AK texture**: on the AK, РЕЖИМЫ and ЧАСТОТА are left of the meters
   and the meters ~57 mm further forward. Under D1 the AK's layout should win.
7. **`vent.workplaces` is placed on a key lock**; no photo shows a ventilation switch on 0004-14M.
8. **OK51-6M is one block, not two**, on both vehicles, with a separate fuse panel (BM3: 3 fuses, 8 jacks) forward of it.
9. **`ssc.control_sel` "КОНТРОЛЬ" is not a switch**: the knob is РРУ КШ (ОСН./ДОП.); КОНТРОЛЬ is the meter.
10. **0062-6M1's manual controls missing**: РРУ КШ ОСН., РРУ КШ ДОП., АРУ — РРУ (p99) are not in the data or PLANNED.
11. **`ssc.modes` and `mon.tx_select` are probably on the wrong knobs**: РЕЖИМЫ is the lower stacked knob by meter 1
    (x ~-0.15), ТОК, МОЩНОСТЬ the upper; the large central rotary is likely КАНАЛЫ (low confidence).
12. **op_panel's block boxes and top items sit up to 60 mm below SAM Sim's straight photo** (0062/OS81-9M/OK62 tops,
    ПЕРЕДАТЧИК), while the bottom agrees: check which picture the face texture is.
13. **OK81-4M-I's ВОЗВРАТ (channel I) is missing**; only channel II's `msl.return_2` exists. Place: red button at about
    (+0.03, -0.64) once the block is extended. Its БАЛАНС, test socket and white cover box are missing too.
14. **0004-14M's meters are too small and too low**: ~80 x 75 mm cases (data 66 x 42) centred ~0.51 (data ~0.485), with
    two white half-round hoods.
15. **0051-7M's five-position switch** (АВТ., РУЧН. Р-Р, РУЧН. F1-F3, F3, Р; p99) is confirmed by its pointer knob, with a
    data card and a red test socket; legends unreadable. It is already PLANNED.
16. **`soc.top_toggle` is not a toggle**: the right end of 0094 has a fuse "1А ~150В" and the block's key lock.
17. **Headset jacks are placed on hardware the photos show otherwise** (so_panel on OK81-4M's lock, op_panel on the ПЕЛЕНГ
    oval's sockets).
18. **Missing plates and labels**: the rack plate "СЛ-2М2" on the top rail; the "ОО62-6М1" and "ОО04-14М" plates; OK81-4M's
    "КАНАЛ I" tag; every block's two key locks.
19. **0004-14M's three red caps "+?В / -150В / КОРПУС"** and a lamp are not in the data (test jacks or fuses; the first
    figure is unread).
20. **AK vs BM3 differences**: 0094-5 vs 0094-9 and -М3 plates (already known); 0004-14M's mirrored control layout (new);
    a small "ОТКЛ." unit aft of 0051-7M on the AK only (SAM Sim). The AK's bottom row and top row above row 0 are not
    photographed anywhere I found: SAM Sim's exe_img has no uncropped view, and rwd_mb3 is a BM3.
