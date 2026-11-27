# Cab review: commander's side, radios, intercom (osa-book worktree, 12 Nov 2026)

Read-only review. Photos: wwp_gecko 600 dpi scans (`out/recon/dryrun/photos/raw`), the ex-NVA **9A33BM3-1**
(Osa-AKM family), so every photo finding is "BM3-1 as photographed" until an AK source agrees. Manual: voenizdat_osa_1991
(`text.txt`); **page numbers below are the book's printed pages** (= the text file's `=== PAGE n` minus 1), the
convention the data already uses. Pixel boxes are in the raw scan's pixels (x0,y0,x1,y1). Crops are in
`scratchpad/review/cab_commander/` (not for the repo).

Already known and not re-reported as new (PLANNED / docs): ПРЕД. УГОЛ dark, the de-energisation indicator box, signal
flags, the air gauges over the commander's screen, PNV/TNPO/wave shield (`p:vision`), the BP-26 fuses and the racks'
test jacks (mesh only), the black lever forward of each vent plate, the blue lamp under the header, the А-2 plate
conflict (kept open in docs section 10).

---

## 1. commander_panel (g085-a; also g083-a, g087-d, g082-b)

### 1.1 Legends read
| Photo | Box (px) | Text | Certainty |
|---|---|---|---|
| g085-a | 1860-2740, 600-860 | Nine toggles, `ВКЛ` over each | high |
| g085-a | same, label row | 1 `?ЗНЕ-/ТАТЕЛЬ` (fits ОСВЕ-/ТИТЕЛЬ; ОЧИ-/СТИТЕЛЬ also possible); 2 `ЗАЩИТА`; 3 `ПРЕОБРА-/ЗОВАТЕЛЬ`; 4 hidden by a hanging key tag, ends `...ДЫ`/`...ТЫ`(?); 5 `СВЯЗЬ`; 6 `ПОД...ТЫ`(?) (data ПОДФАРНИКИ; ending not clearly -НИКИ); 7-8 one label `ЛАМПЫ / ПЕРЕДНЯЯ  ЗАДНЯЯ`; 9 `ОТО-/ПИТЕЛЬ` | 1, 4, 6 low; 2, 3, 5, 9 high; 7-8 medium |
| g085-a | 1880-2140, 1180-1300 | `БЛОКИ-/РОВКА` under a large black button in a chrome ring | high |
| g085-a | 2030-2140, 1180-1300 | **`ЛЮКИ / ЗАКРЫТЬ`** (ends in Ь) under a small dark-green lens in a chrome bezel | medium-high |
| g085-a | 1880-2150, 1350-1420 | **`↓ ВОДООТРАЖА-/ТЕЛЬНЫЙ ЩИТ ↑`** under a toggle | medium-high (ЩИТ clear) |
| g085-a | 2420-2860, 870-1530 | Inset `ПРОХОД ПОД МОСТОМ`; two push-buttons (red-brown caps, chrome bezels, on a raised plinth) `ВКЛ.` left, `ВЫКЛ.` right | high |
| g085-a | 2470-2680, 1080-1350 | Five black round legend caps: `СТОПОР/ПОХОД.` (left top), `ГАБАРИТ/3800` (right), `УКЛАД./ПРОИЗВ.` (left; ПОХОД. not excluded), `qн (or βн)/ЗАСТОП.` (right), `ПРЕД./УГОЛ` (left bottom) | high except УКЛАД. line 2 and the q/β glyph (medium) |
| g085-a | 2600-2700, 1440-1490 | `812.04` on the caps' cover tab (finger notch); `№Щ0510`(?) at the inset's foot | medium / low |

### 1.2 In the photo, not in the data
- **plate/part**: the five legend caps sit on a separate hinged/removable cover (finger notch at the foot, plate
  `812.04`) held by one knurled captive screw at its top. The data's chrome "lamp" fixture at (-0.0361, 0.0012)
  ("a small lamp under the buttons") is that screw, not a lamp (medium).
- **control**: the 4th strip switch's tag/key on a lanyard (loose item; the data already has `veh.strip_4`).
- **part**: large vertical tubular grab handle at the plate's right edge (g085-a x 2900-3000) and the bow handles
  either side of the strip (the handles are in `SM_CabUnits` per docs section 43; check the right-hand big one).

### 1.3 In the manual, not in the data
- Book p50: the stowing panel's buttons ПРОХОД ПОД МОСТОМ ВКЛ. — ВЫКЛ. lower/raise the search antenna on the move
  without switching stabilisation off; табло УКЛАДКА ПРОИЗВ. = antenna stowed for passing a bridge; ПРЕД. УГОЛ = a drive
  on its limit angle; ГАБАРИТ 3800 = antenna stowed, ССЦ locked and launchers lowered. Data agrees (two buttons as one
  switch is a recorded simplification).
- Book p99: табло **qн ЗАСТОПОР.** lights on the stowing panel when the АПУ is locked; book p223: on march, check
  СТОПОР. ПОХОД. and qн ЗАСТОП. Data has both (`apu_locked`, `apu_travel_lock`).
- Book p224: "при преодолении глубоких водоемов волноотражательный щит должен быть поднят" (wave shield raised for deep
  water); book p224 and p238: the crew watches the bilge pumps (водооткачивающие насосы) while swimming, but no control
  for them is named. Swimming/water is not modelled (no subsystem).
- Book p163: the hatches' opening and closing system is part of СЭП (powered hatches). Not modelled beyond `veh.hatches`.
- Note: the manual's "пульт командира" (pp21, 99, 119, 141, 150) is **block ОО04-11М1**, not this dash: ЛЮКИ ЗАКР. as a
  табло is the launch console's (`luki_zakr` on fo_panel, correctly).

### 1.4 Data the photo or manual contradicts
- **`veh.bilge_pump` "ВОДООТКАЧИВАЮЩИЙ НАСОС"**: the plate reads ВОДООТРАЖАТЕЛЬНЫЙ ЩИТ with down/up arrows: the bow
  wave-shield (trim vane) raise/lower switch (g085-a; supported by book p224). The PLANNED `p:vision` lists "wave shield"
  as sources-only: it is in fact on the dash under the wrong name.
- **`veh.hatches` look label "ЛЮКИ ЗАКРЫТЫ"** (and its nameplate fixture): the plate reads ЛЮКИ ЗАКРЫТЬ ("close the
  hatches"), an order/command lamp or a button, not a state "closed". Its pairing with БЛОКИРОВКА suggests a hatch
  interlock; function not in the manual found.
- `veh.illuminator` ОСВЕТИТЕЛЬ: the reading is not confirmed (see 1.1); keep low; ОЧИСТИТЕЛЬ (screen wiper) is a
  credible alternative given the wiper motors over both screens.
- `veh.sidelights` ПОДФАРНИКИ: the label's ending does not read -НИКИ at this resolution; low.
- `stowed` label "УКЛАДКА ПРОИЗВ.": manual form; the cap is abbreviated УКЛАД. ПРОИЗВ.(?) — fine.
- `ta57.handset` and `ta57_ringing` at (0.12, 0.055/0.115): g085-a and g087-d show nothing there: the plate's right edge
  carries the big tubular handle and the hull. Already recorded as SAM3D's choice; it collides with a photographed
  handle.

---

## 2. heater_panel (g087-c, g087-d; g085-b right edge)

### 2.1 Legends read
| Photo | Box (px) | Text | Certainty |
|---|---|---|---|
| g087-c | ~820-1290, 230-460 | Legend strip, three fields: `СВЕЧА` · `ЭЛЕКТРОДВИГАТЕЛЬ` · `КОНТРОЛЬ ГОРЕНИЯ` | first medium, 2-3 low (length and shape fit) |
| g087-c | ~1170-1340, 160-480 | Vertical black plate beside the box with a T-handle/valve diagram; vertical words both sides illegible | - |
| g085-b | 2190-2450, 1240-2000 | Tall black plate on the duct: heading `ПОРЯ(ДОК ...)`, steps `1 ОТКР...`, `2 ВКЛ...`, `3 ЧЕР(ЕЗ)...`, `4 ЧЕР(ЕЗ)... КОНТ(РОЛЬ)... ПРИ...`, `5 ЧЕР...`, `6 ...` (cut by the frame) | heading/step words medium |

Box layout (g087-c): top left a chrome ring with a small central hole (the glow-coil window), top right a square black
bezel with a red lens, under the strip: a toggle (left, under СВЕЧА), a large black button (centre, under
ЭЛЕКТРОДВИГАТЕЛЬ), a small black knob/lens in a chrome bezel (right, under КОНТРОЛЬ ГОРЕНИЯ).

### 2.2 In the photo, not in the data
- **control** (right item under КОНТРОЛЬ ГОРЕНИЯ): drawn as a black "jack" fixture at (0.045, -0.02); it is a knob or a
  lens in a bezel, not a jack (low).
- **plate**: the T-handle diagram plate, and probably a handle/lever it belongs to (a black grip hangs on the pillar
  below it in g087-c) — fuel cock or air flap of the heater (unknown).
- **plate**: the heater's start-procedure plate on the duct (six steps).
- **part**: the flat heater duct and its elbow outlet in the commander's footwell (g085-d) — listed as missing in the
  handoff, not new.

### 2.3 In the manual, not in the data
- Book p172-173: the chassis 5937 has an **отопительно-вентиляционное устройство** to heat the operators' compartment in
  winter and ventilate it in summer; book p174: separately the engine's liquid direct-flow pre-heater (подогреватель).
  Book p16: the BM includes systems of ventilation and heating (обогрева). No cab-heater subsystem is modelled; the data
  models only `preheater` (engine).

### 2.4 Data contradicted
- **What the box is**: the data calls it the engine pre-heater (p174). The book's caption calls it the commander's
  "personal heater control panel"; strip switch 9 is ОТОПИТЕЛЬ; the pillar plate "при включении отопителя проверь
  положение люков" and the ВЕНТИЛЯЦИЯ — ОТОПЛЕНИЕ flap lever, and the duct running from this corner under the
  commander's dash into the cab (g085-d), all point to the **cab air heater** (fuel-burning, glow plug, fan motor,
  flame check). Medium: an engine pre-heater panel carries similar words, so the manual does not settle it.
- `heat.start` "pre-heater button": by the strip the large button is under ЭЛЕКТРОДВИГАТЕЛЬ (fan motor) (low).

---

## 3. dp3b_panel (g085-b best; g087-d, g082-b)

### 3.1 Legends read
| Photo | Box (px) | Text | Certainty |
|---|---|---|---|
| g085-b | 640-950, 1050-1380 | Meter `R/h`, two scales (upper to 500 marks visible) | high |
| g085-b | 1000-1500, 1000-1250 | Range switch positions **`ВЫКЛ.`, `x1`, `x10`, `x100`, `x500`** | high (ВЫКЛ. partly hidden by the button) |
| g085-b | 1000-1150, 960-1100 | Large chrome push-button; the plate names it **ПРОВЕРКА** | medium |
| g085-b | 1180-1330, 960-1060 / 1380-1520 | Green lens, red lens | high |
| g085-b | 1000-1200, 1260-1340 | `ДП-3Б / № 4 ...` serial plate | high |
| g085-b | 680-1480, 1330-1560 | Instruction plate, 3 points: (1) in switch position "ХР"/"x1"(?) with ПРОВЕРКА pressed the meter reads 0.4-0.8 R/h; (2) on x1, x10, x100 read the upper scale, on x500 the lower; (3) the signal lamp's flashing rate rises with the radiation | medium (the first position's name low) |
| g085-b | 470-1400, 1610-1800 | Lower box: two fuse holders `2А`, `2А`, a black ball knob between, a small plate `ДП-3Б/1`(?); two cable connectors under it | fuses high, plate low |
| g085-b | 830-990, 1660-1800 | Zero-adjust screw under the meter | medium |

### 3.2 In the photo, not in the data
- **plate**: the 3-point instruction plate; the type/serial plate.
- **part/control**: the lower junction box with two 2 A fuses and the black knob; the two connectors and braided
  cables (one goes to the external detector, not in the cab).
- **part**: the bracket hanging the box from the screen frame by a threaded stud (handoff lists "DP-3B as a cast box on
  a bracket" as missing, not new).

### 3.3 In the manual
- The only ДП-3Б mention is **book p189, in the list of the TZM 9T217BM2's chassis 5939** (with R-123M, R-124, PNV-57E,
  TNPO-170A, fire system, FVU). No BM page names ДП-3Б. The data's `source` "voenizdat_osa_1991 p189 (the radiometer
  DP-3B on the vehicle)" therefore cites the loader's fit, not the launcher's; the BM's DP-3B rests on wwp_gecko alone
  (BM3-1). No radiation is modelled.

### 3.4 Data contradicted
- **`veh.dp3b` positions `ВЫКЛ., КОНТР., 1, 2`**: the switch reads ВЫКЛ., x1, x10, x100, x500 (five positions; the check
  is made with the ПРОВЕРКА button, plate point 1).
- `dp3b_control` (red, lit on КОНТР.): by the plate the red is most likely the **signal lamp that flashes faster with
  rising radiation**; with no radiation it would stay dark (low-medium).
- `dp3b.check` "its name is not legible": the plate names the button ПРОВЕРКА.
- Source citation p189 (see 3.3).

---

## 4. radio_left_panel / radio_right_panel (R-123M; g090-d right set, g091-b left set)

### 4.1 Legends read (g090-d unless noted)
| Box (px) | Text | Certainty |
|---|---|---|
| 380-640, 1240-1590 | `КОНТРОЛЬ НАПРЯЖЕНИЙ`, `РАБОТА` `1` `2`; left `ПЕРЕДАЧА` 600в, 250в, 150в, 1,2в; right `ПРИЕМ` 1,2в, 6,3в, 150в | high |
| 400-610, 1580-1700 | Three items in a pocket under it: left and right each `ВКЛ.` above / `ВЫКЛ.` below; centre **`ТОН. ВЫЗОВ`**; vertical side words illegible | ВКЛ/ВЫКЛ medium, ТОН ВЫЗОВ medium |
| 280-330, 1290-1420 | `РЕГ. / ДЕЖ.ПР(ИЕМ)` strip by the ШУМЫ knob | medium |
| 650-800, 1290-1420 | `СИМПЛЕКС  Д.ПРИЕМ`; `КОРРЕКТОР` (vertical) | high |
| 620-800, 1640-1800 | `УСТАНОВКА ЧАСТОТЫ`; plate "перед работой проверь частоту по внутреннему калибратору" with a white button-lamp `КАЛИБРОВКА` | high |
| 1110-1300, 1330-1370 | `НАСТРОЙКА АНТЕННЫ` (red knob), `ПОДДИАПАЗОН`, `ПЛАВНЫЙ ПОДДИАПАЗОН`, `ГРОМКОСТЬ`, nameplate `Р-123М` | high |
| 330-370, 1810-1900 | Vertical `ПИТАНИЕ` beside the large round power connector (lower block) | medium |
| g091-b BP-26 | `БП-26 № 880341`; fuses `3А 150В`, `8А 26В`(?) x2; `МАССА` terminal; red wing terminal `+26 В` with plate **`СОБЛЮДАЙ ПОЛЯРНОСТЬ`** | plate medium; fuses low |

### 4.2 In the photo, not in the data
- **control**: the КОНТРОЛЬ НАПРЯЖЕНИЙ rotary (the voltage well is in the mesh with `'sim': None`); it carries
  РАБОТА 1 / РАБОТА 2, the working positions (research doc: КОНТРОЛЬ НАПРЯЖЕНИЙ (РАБОТА 1)).
- **controls**: the two ВКЛ./ВЫКЛ. switches in the pocket (the mesh's three `BAT_HANDLES`); one is probably ШКАЛА (dial
  lamp, named in OSA_COMMS research), the other unknown.
- **control/lamp**: the КАЛИБРОВКА button-lamp (frequency check against the internal calibrator, a pre-work step on its
  plate).
- **control**: РЕГ. ДЕЖ.ПР. regulators (two brass slotted heads; maintenance).
- **part**: the green canvas cover on each set (handoff: not modelled), BP-26's terminals.

### 4.3 In the manual
- Book p16: the BM carries two R-123M and an R-124. Nothing more on the sets or the intercom in the manual for the BM
  (p189, p191, p202 are TZM/MTO/trainer).

### 4.4 Data contradicted
- **`radio.left.tone` / `radio.right.tone` at (-0.1375, -0.0995)** ("between АНТЕННА and ПИТАНИЕ on the lower block"):
  the photo puts ТОН. ВЫЗОВ as the centre item of the pocket under КОНТРОЛЬ НАПРЯЖЕНИЙ, about trace (35.5, 122) mm =
  panel (-0.168, -0.010), ~9 cm higher and 3 cm further left (medium; perspective).
- `radio.*.power` on the lower block: the photo's ПИТАНИЕ word belongs to the power connector; the trace itself says
  the cabin added the power and antenna plates "not on the real front". How the real set is switched on (via
  КОНТРОЛЬ НАПРЯЖЕНИЙ / a pocket switch / the A-box) is not settled by these photos.

---

## 5. r124_a1_panel / r124_a2_panel (g087-c, g088-a/b/c)

### 5.1 Legends read
| Photo | Box (px) | Text | Certainty |
|---|---|---|---|
| g087-c | 1750-2240, 1010-1310 | `А-2`; positions `Р-112 · ВС · Р-123`, handle at ВС (centre) | high |

Box: rounded-triangular cast case, one central rotary with a bar handle, four screws; **no volume knob, no face jack**;
cable connectors on its left and right flanks. It stands on the flat inner face of the front wheel housing beside the
commander's seat, under SP-1M.

### 5.2 In the photo, not in the data
- PLANNED `p:keyed` "Commander's round keyed switch on the front wheel housing (g087-c), function not identified" is
  **this А-2 box** (handoff line 486 places the "round keyed switch" on that wheel housing face; nothing else is there).
  It should be merged with the R-124 box, not kept as an unknown.

### 5.3 Manual
- Book p16 only (R-124 present). No subscriber layout.

### 5.4 Data contradicted
- Already open: the commander's box is plated А-2, the data's commander box is A-1 (kept open, docs section 10).
- New detail: the photographed box has **three positions Р-112 / ВС / Р-123** (no ВЫКЛ), no ГРОМКОСТЬ knob and no face
  ШЛЕМОФОН jack. Data: `intercom.a1` [ВЫКЛ, ВС, Р-123] plus `intercom.a1_volume` and a face jack; `intercom.a2`
  [ВС, Р-123]. The BM3-1's box suggests the A-2 type has three positions; Р-112 is a radio the Osa did not carry.
- `radio.chest` drawn on the A-1 face: no chest switch on the photographed box (it hangs on the cord; data says so).

---

## 6. Right wall round the radios: SP-1M's block over the A-2 (g087-c), BP-26, ZIP (g090-a)

### 6.1 Legends read (g087-c, block face 1650-2860, 30-500 px)
- Key lock left with arrow `ОТКР./ЗАКР.`(?) (low).
- Six red lamps, each over a fuse: `0,25А У3`, `2А У4`, `1А У5`, `0,25А У6`, `0,25А У7`, `0,25А У8` (medium; У-numbers
  medium-low).
- Seven jacks below: `+13В У..`, `-13В У7`(?), `+6,3В У8`(?), `-6,3В У..`, `+5В У4`(?), `+250В У6`(?), `НОРМ`(?) (low).
- Orange plastic connector covers lying on the shelf under it (loose).

### 6.2 Not in the data
- **lamps/fuses**: `sp1m_panel` has five named blocks and **no lamps, fuses or fixtures**. This block (a front-column
  block of SP-1M by its place over the A-2; drawing g090-b's "5 lamp-and-jack pairs" block is the closest) carries six
  red lamps, likely fuse-blown indicators per sub-unit У3-У8 (dark in normal work). g090-a shows another SP-1M block
  with a row of 8 orange fuse caps `5А У..`, `1А У..` with small lamps and jacks `+6,3В У..`, `КОРПУС`.
  Which block is which (0093-14А / ОК82-2М / 0092-9М) is not readable.
- **plate**: `ЗИП №1А ЯЩИК №2` on the box under the right set (box built; plate?), and the steel step cover's
  `Р-123М-1` end plate (built per handoff).
- **lamp**: blue-lensed emergency/blackout lamp on the roof above the commander's visor (g089-c) — docs list it as kit
  the model lacks.
- **parts**: the two hinged stowage doors on the **right** cheek wall (g087-b, g087-a through the glass); the data has
  the left web's (`stowage_panel`) only. The small trapezoid side window in the right cheek (g087-b) — check the glass
  mesh includes it.

---

## 7. aiming_circle_case
- No photo shows it. Book p249-250: the aiming circle is the field artillery **ПАБ** (буссоль ПАБ), set 20-50 m ahead
  of the vehicle for Qc. Its stowage place is in no source found; the data's estimate stands. Label could name ПАБ.

## 8. vent_hatch_right / vent_hatch_left
- g090-d (900-1180, 330-500): the black plate with a lever diagram over SP-1M, words `ЛЮК ВЕНТИЛЯЦИИ`, `ЗАКР.`, `ОТКР.`
  (medium at the overview, illegible at the crop). Book p167: levers over SP-1M1 and SŁ-1M1, plates ЛЮК ВЕНТИЛЯЦИИ
  ЗАКР.—ОТКР. Book p176: the ventilation hatches' covers are opened when the equipment is switched on and closed by hand
  when it is switched off; book p223: shut all ventilation hatches for a march with the equipment off. Data agrees; the
  march rule is not in the drill (check).

---

## Top findings (ranked)

1. **`veh.bilge_pump` is the wave-shield switch**: plate ВОДООТРАЖАТЕЛЬНЫЙ ЩИТ with ↓/↑ (g085-a); manual p224 (raise
   it for deep water). Rename; PLANNED `p:vision`'s "wave shield" is already on the dash.
2. **DP-3B range switch is ВЫКЛ., x1, x10, x100, x500** with a ПРОВЕРКА button (g085-b, legible face and instruction
   plate), not ВЫКЛ./КОНТР./1/2; the red lamp is likely the flashing signal lamp.
3. **DP-3B's manual citation (p189) is the TZM 9T217BM2's chassis list**, not the BM's; the launcher's DP-3B rests on
   the BM3-1 photo only.
4. **ЛЮКИ ЗАКРЫТЬ, not ЗАКРЫТЫ** on the commander's dash (g085-a): a command lamp/button beside БЛОКИРОВКА; the manual's
   ЛЮКИ ЗАКР. табло is ОО04-11М1's (already on fo_panel).
5. **The heater box is most likely the cab air heater** (book caption "personal heater"; strip СВЕЧА · ЭЛЕКТРОДВИГАТЕЛЬ
   · КОНТРОЛЬ ГОРЕНИЯ; switch ОТОПИТЕЛЬ; heater-and-hatches warning plate; duct into the cab), not the engine
   pre-heater; book p172-173 gives the chassis a heating-ventilation unit. Medium.
6. **PLANNED `p:keyed` ("round keyed switch" on the wheel housing) is the А-2 intercom box** of g087-c: merge it.
7. **The photographed А-2 has three positions Р-112 / ВС / Р-123, no volume knob, no face jack** (g087-c), against the
   data's A-1 [ВЫКЛ, ВС, Р-123] + volume + jack and A-2 [ВС, Р-123].
8. **R-123M ТОН. ВЫЗОВ is misplaced**: it is the centre of three items in the pocket under КОНТРОЛЬ НАПРЯЖЕНИЙ
   (~panel (-0.168, -0.010)), not on the lower block (g090-d, g091-b).
9. **R-123M controls missing**: КОНТРОЛЬ НАПРЯЖЕНИЙ (РАБОТА 1/2 and voltage checks; the well is in the mesh with no
   control), two ВКЛ./ВЫКЛ. pocket switches (one likely ШКАЛА), the КАЛИБРОВКА button-lamp (a pre-work step on its plate).
10. **SP-1M faces carry fuse-blown lamps and fuses**: six red lamps over six fuses (У3-У8) and seven jacks on the block
    over the А-2 (g087-c), 8 fuses with lamps on another (g090-a); `sp1m_panel` has no lamps or fixtures.
11. **The stowing board's five caps are on a removable cover** (finger notch, plate 812.04, one captive screw); the
    data's chrome "lamp" under the ВКЛ./ВЫКЛ. buttons is that screw.
12. **Plates the cab lacks**: the DP-3B's instruction plate and its lower fuse box (2 x 2 A), the heater's six-step
    procedure plate (ПОРЯДОК...), the T-handle plate by the heater box, BP-26's СОБЛЮДАЙ ПОЛЯРНОСТЬ.
13. **Strip switch names 1, 4, 6 remain uncertain**: 1 fits ОСВЕТИТЕЛЬ or ОЧИСТИТЕЛЬ; 6 does not clearly read
    ПОДФАРНИКИ; 4 is hidden in every view.
14. **TA-57 handset and lamp** on commander_panel sit where g085-a/g087-d show the big grab handle and bare hull.
15. **Right cheek's two stowage doors** (g087-b) and the blue lamp over the commander's visor (g089-c) are not in the
    data (the lamp already noted in docs).
