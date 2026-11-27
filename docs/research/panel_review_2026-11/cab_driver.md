# Review: the driver's cab, left front (Osa-AK cabin data vs photos and manual)

Scope: driver_panel, pu303_panel, column_stalk_panel, gear_lever_panel, handbrake_panel, stowage_panel, fire_panel,
fire_valve_panel, post_valve_panel, radio_well_panel, header_valve_left, header_valve_right.

Sources: wwp_gecko scans g083-a, g084-a/b/c/d, g085-a, g086-a/c/d, g089-a, g091-d (600 dpi; crops in
scratchpad `review/cab_driver/`, not in the repo); rwd_mb3 9a33_31 (too small to add anything); voenizdat_osa_1991.
**Manual page numbers:** text.txt marks `=== PAGE n ===` = pdf page; the printed page is n-1 (pdf 164 = p163).
Pages below are printed pages, as the data cites them.
**Vehicle:** the photographed vehicle is a 9A33BM3-1 (Osa-AKM family). The manual's chapter 2.9-2.10 also describes
9A33BM3 and chassis 5937. Neither source is specific to the 1980s Osa-AK (9A33BM2). Same chassis 5937; no AK-specific
difference found for these panels. Treat everything below as AKM evidence applied to the AK.

Pixel boxes are in the raw scan's pixels (x0,y0,x1,y1).

---

## driver_panel (the dash: the plate behind the wheel + the instrument plate)

### 1. Legends read
| Photo | Box | Text | Certainty |
|---|---|---|---|
| g085-a | 180,1090,480,1320 | 4 toggles, ОТКЛ under each; above: ПРИБОРЫ / НАРУЖНОЕ ОСВЕЩЕН. / ВНУТРЕН. ОСВЕЩЕН. / 4th two lines "СИГНАЛ… / З…" (2nd line 4-5 letters, not read) | high (1-3), low (4th) |
| g085-a | 720,1230,1290,1380 | lamp row plates: БОРТ. СЕТЬ / **ДИФФЕ-РЕНЦИАЛ** / [ЦЕНТР]АЛЬНЫЙ [ПЕРЕКЛ]ЮЧАТЕЛЬ СВЕТА / ПОВОРОТ | high (1,4), medium-high (ДИФФЕРЕНЦИАЛ), medium (3) |
| g085-a | 710,930,960,1200 | oil pressure gauge "кгс/см²": figures **0, 6, 9, 15** visible, ticks between (scale 0-15) | medium |
| g085-a | 1170,960,1420,1200 | ammeter "A": 20-0-20-40-60, a **second small scale above it**; a small yellow word on the face and a **small lever/toggle on the meter's lower edge** | medium (scale), low (word) |
| g085-a | (overview) | ОХЛ. ЖИДКОСТЬ, МАСЛО ДВИГАТЕЛЯ (x2), ЗАРЯД, РАЗРЯД, "x100 ОБ/МИН" tacho 0-40, km/h 0-120 odometer 03374 | high |
| g083-a | 640,410,790,560 | small black plate: 2 toggles, ВКЛ at top of each; labels under them not legible | high (ВКЛ), none (labels) |
| g083-a | 380,330,1260,700 | gauge "кгс/см²" figures 2,4,6,8,10 (0-10); plate СТАРТЕР below it | high |
| g083-a | 1000,250,1230,400 | ПОЛОЖЕНИЯ РЫЧАГА ПЕРЕКЛЮЧЕНИЯ ПЕРЕДАЧ: H-gate 3 over 3 with a neutral circle in the middle; digits not legible | high (title), none (digits) |
| g084-c | 1120,760,1300,980 | gauge 0-6, red band at the low end (~0-0.7) and at the high end (~4.5-6), green/blue near 1; **apparently two pointers**; unit line under the hub not legible | medium (scale), low (two pointers) |
| g084-c | 860,780,1050,940 | plate ВОДИТЕЛЬ! + 5 lines; overview g083-a suggests "ДЛЯ ОБЕСПЕЧЕНИЯ НАДЕЖН(ОГО) ПУСКА ДВИГ(АТЕЛЯ)…" | high (heading), low (rest) |
| g084-c | 1030,1020,1080,1340 | vertical strip beside the flat lever: starts "ПУСК", a symbol, then ~6 letters not read | low |
| g084-c | 820,1080,1080,1300 | small plate over the stepped-gate lever, 3 lines, first word may be "ПОВОРОТ…" | low |
| g084-c | 1080,1080,1260,1300 | round instrument in a square flange, pale face, two red dots, a large knurled centre knob; label 3-4 letters (candidate "ЧАСЫ", i.e. a clock) | low |

### 2. In the photo, not in the data
- **controls**: the two toggles on the small ВКЛ/ВКЛ plate (behind the wheel's left spoke) exist only as a nameplate
  fixture; no toggle. Function unknown. Candidates are the transfer box or differential lock (manual p175), or
  starting aids (p174). Low.
- **control**: the starter button under СТАРТЕР (hidden by the hub in every view; data has the plaque only).
- **part**: the vertical slot indicator between the ЗАСЛОНКА ВОДОМЕТОВ toggles (g083-a 1130-1160,470-560), not drawn.
- **control**: the small lever on the ammeter's lower edge (g085-a). It may select between the meter's two scales, so the
  meter would be a volt-ammeter. Low. The data has it as `veh.knob_ammeter`, a knob.
- **lamp**: none new on this panel.

### 3. In the manual, not in the data
- p163-164: power is controlled from "щиток приборов механика-водителя шасси 5937 **дополнительный**" (an additional
  driver's instrument board), PU-303 and the ПАНЕЛЬ ИНДИКАЦИИ. Fig. 2.60 links the board to ВУ-1, the 27 V generator
  and the battery switch ВБ. The left plate behind the wheel may be that board: it carries the ПУСК strip, the two ВКЛ
  toggles and the gauges. That is a hypothesis; the data does not mention it. The power system is modelled (`gta`,
  `pto_generator`, `power`).
- p174: the engine is the diesel 5Д20Б-300 (OCR reads "5Д2056-300"). It can be started by the electric starter or by
  compressed air from high-pressure bottles. **The main engine is not a subsystem in the data** (no starter, no
  running state). Only `preheater` exists.
- p175: the gearbox is a 5-speed with 1 reverse. That fits `veh.gear`'s 5 + З.Х. + Н. The transfer box is two-speed
  with a forced-lock inter-side **differential**. There is no differential-lock control in the data, only the lamp. The
  clutch is hydraulic with an air booster. The **parking brakes** (two) are worked by hand. Steering has two hydraulic
  boosters. Electrics are 24 V with two 12СТ-70 batteries.
- p175: the water jets have two flaps on a hydraulic drive. That is consistent with the waterjet toggles (not modelled).

### 4. Contradictions
- Fixture nameplate "БОРТ. СЕТЬ   ДИФФЕРЕНЦ. ПЕРЕДАЧА   ЦЕНТРАЛЬНЫЙ…" (driver_panel 0.3041,-0.1103): the photo reads
  **ДИФФЕ-/РЕНЦИАЛ**, not "ДИФФЕРЕНЦ. ПЕРЕДАЧА" (medium-high).
- The oil-pressure fixture's marks 0-12 (0.2403,-0.0215): the photo shows figures to **15** (0, 6, 9, 15) (medium).
- The 0-6 gauge fixture (-0.3668,0.0686) has one pointer and "not identified". The photo suggests two pointers and red
  bands at both ends. A two-pointer air gauge is possible (the clutch's air booster and the air-hydraulic brakes,
  p175). Low.
- `radio.chest_driver`, `intercom.pv` and `headset.driver` are drawn at driver_panel x 0.4585, the instrument plate's
  right end. In g085-a and g083-a that area holds a tubular handle and the 3-pin socket; no ПВ box is there. The
  placement is SAM3D's own (the note says so), but it puts parts on a photographed surface that shows none.
- `veh.hand_throttle` (the flat lever on the dash) is labelled "hand throttle?". The hand fuel-pump lever of Fig. 2.60
  (p164, "рычаг ручного управления топливным насосом" of the engine) better fits the **ball-knobbed lever on a toothed
  ratchet quadrant**, low left of the column (g086-d 780-1700,400-1000; g083-a 600-760,1150-1300). That lever is not in
  the data. The flat dash lever sits beside the "ПУСК…" strip, so it is more likely a start/pull control. Low-medium.

---

## pu303_panel (ПУЛЬТ УПРАВЛЕНИЯ ТИП ПУ303)

### 1. Legends read (g084-d drawing; g084-b photo)
All the drawing's labels agree with the data: ПИТАНИЕ УПРАВЛЕНИЯ, ПОДСВЕТКА, ГТА, ТВГ, ЛЮКИ ОТКР/ЗАКР, ДАВЛ. МАСЛА,
СТОП, ХОЛ ПРОКР, ПУСК, ОБРЫВ ФАЗ, ПР ВС, ВКЛЮЧЕНИЕ НАГРУЗОК ГТА/ГОМ/ИВП, ОБРАТ. ЧЕРЕД, ОТКЛ НАГР, КОНТР ЛАМП
ОТКЛ/ВКЛ, РАБОТАЕТ ГТА/ГОМ/ИВП, НАПР ГТА/ГОМ, МОТОЧАСЫ ГТА/ГОМ, plate "№ Л-0178" (high). The photo prints
"ХОД ПРОКР." (OCR-like ambiguity; the drawing's ХОЛ is right) and "x100°C" on ТВГ (high).
- g084-b 600,380,790,660: the ГТА dial is a **full-circle** dial: 10 (=0) at the top, then 2, 4, 6 and 8 clockwise
  (high).
- g084-b 200,300,540,660: the gauge left of PU-303 runs 0-0.2-0.4-0.6-0.8-1. The unit is read as "кПа" or "МПа". It
  carries "Сделано в СССР" and "ГОСТ 2405-…" (the Soviet standard for pressure gauges) (medium). So it is a pressure
  gauge.

### 2. In the photo or drawing, not in the data
- **parts**: two screw-cap fuse holders flanking the ПИТАНИЕ УПРАВЛЕНИЯ toggle (drawing; one visible in the photo).
- **lamp?**: a red lens just right of the КОНТР. ЛАМП knob (g084-b 520,880,720,1100), photo only. The drawing shows
  only position marks there. Low-medium.
- **part**: a strip of ~7 fuses with orange tips on the side face just right of PU-303 (g084-b 1430,820,1555,1300),
  with a plate "…ОТ…". Not in the data.

### 3. In the manual (p163-164, Fig. 2.60), not in the data
- **ИВП = источник внешнего питания** (external supply, through the ШРА-200 socket on the left side, p164). Rated
  powers: ГТА 50 kW at 67 kg/h, ГОМ 35 kW at 20 kg/h.
- **ВС = вентилятор-сепаратор**, shared by the turbine generator and ГОМ. Fig. 2.60 wires it to PU-303. This is the
  likely subject of lamp **ПР ВС**. The reading of "ПР" is open (candidates: привод, проверка); low.
- **The ГОМ is engaged by "кран включения ГОМ"** on the power-take-off drive from the main engine (СД). The engine's
  speed is held by the hand fuel-pump lever (Fig. 2.60). Neither is in the data, and `pto_generator` does not need the
  engine running.
- **УКЧ** (in ОС52-16М) cuts the load when the ГТА, ГОМ or ИВП frequency falls to 360 Hz. Not modelled.
- The turbine's hatch on Fig. 2.60 is "шасси 5937 люк выхлопных газов" (the exhaust hatch), wired to PU-303. The data
  calls ЛЮКИ "intake and exhaust hatches" (SAM3D's reading).
- 9И120 parts (ГТД 9И56-3, starter box КП, ignition У5, current limiter У6, boost pump ПН, temperature warner У9): the
  sources of ДАВЛ. МАСЛА and ТВГ. Only `gta` is modelled.
- p246-247 (drill): "К бою, включить ГТА (ГОМ)". The driver sets the controls to the start position, starts the ГТА (or
  ГОМ), reports «Питание подано», starts the main engine and reports «К маршу готов». The crew data has these lines.

### 4. Contradictions
- `pwr.gta` note: "Its labels can't be read in any picture found". This is stale: g084-b and g084-d give every label.
- `pwr.pto` is labelled "power take-off generator" and placed on the ВКЛЮЧЕНИЕ НАГРУЗОК **ГОМ** button. The drawing
  shows that button connects the loads. The manual has the generator engaged by the кран включения ГОМ. There is no
  `gta.load_gom` control.
- `gta.load_ivp` note: "ИВП's meaning is not known". The manual (p164) defines it, and the data's own `ivp_on` lamp
  cites that page.
- `gta.lamp_test` note: "Not modelled: it lights no lamp in the sim". The cabin lamps on PU-303 read it in
  `lit_when_any`, so the note is stale.
- Display `gta_rpm` has `value_per_turn` 12 and 12 marks. The dial shows 10 per turn, with 10 at the top (high). With
  12, "10" falls at 300°.
- The `gta_gas_temp` fixture colour is black. The ТВГ face is pale blue in g084-b and g086-c (medium; looks only).
- The ДАВЛ. МАСЛА, ПР ВС, ОБРЫВ ФАЗ and ОБРАТ. ЧЕРЕД lamps: in g084-b, ОБРЫВ ФАЗ and ОБРАТ. ЧЕРЕД show red centres.
  That supports the data's red, against section 46's doubt (medium).
- The data's ДАВЛ. МАСЛА is lit whenever the turbine is not running (SAM3D). There is no source for this; the manual is
  silent.

---

## fire_panel, fire_valve_panel (g086-c, g086-a)

### 1. Legends read
| Photo | Box | Text | Certainty |
|---|---|---|---|
| g086-c | 390,540,600,720 | left flap: ПРОВЕРКА ЦЕПЕЙ / ПЕРЕВОД ДИСКА ОСТАТОК БАЛЛОНОВ, an arrow down to a slotted disc | high |
| g086-c | 680,600,1000,740 | right flap: ПРИ ПОЖАРЕ ОТКРОЙ КРЫШКУ И НАЖМИ КНОПКУ; two red lamps below | high |
| g086-c | 50,0,290,130 | gauge figure "250", unit above illegible | high (250) |

### 2. In the photo, not in the data
- **part**: the pale cylindrical canister with a central port (filter or separator), left of the box (g086-c 0,520,390,900).
- **part**: three black bottles under the box; the standing one has the red hand wheel (data has the wheel only).

### 3. In the manual
- p16 lists fire suppression among the BM's systems; p221 forbids soldering in the engine compartment with the fire
  system switched off. So the system has an on/off. That switch is not on this box. It may be on the commander's toggle
  row (ЗАЩИТА? not verified; outside this group). The data's subsystems `fire_circuit_check` and `fire_discharged` do
  not model "system on".
- p174: the engine can be started by **compressed air from high-pressure bottles**. Black bottles with a hand-wheel valve
  and a 250-scale gauge are typical of such an air bank. The book's caption calls them the fire system's bottles. The
  scans cannot settle which they are. Record both; the data follows the caption.

### 4. Contradictions
- None firm. `fire.check` is drawn as a black button under the left cover. The photo shows only legends on the closed
  flap, so the button's look is unverified.

---

## post_valve_panel (the left front pillar, g086-a)

### 1. Legends read
- g086-a 1169,680,1530,930: "ВНИМАНИЕ! ПРИ ВКЛЮЧЕНИИ ОТОПИТЕЛЯ ПРОВЕРЬ ПОЛОЖЕНИЕ ЛЮКОВ" (high). Left caption "ОТОПЛЕНИЕ"
  over "…РЫТЫ" (закрыты or открыты); right caption "ВЕНТИЛЯЦИЯ ОТКРЫТЫ" (medium). The line drawing of the hull
  indicates the hatches.
- Plate "ВЕНТИЛЯЦИЯ ⇄ ОТОПЛЕНИЕ" with a flap diagram and a double arrow (high).
- "ИНСТРУКЦИЯ ПО ПОЛЬЗОВАНИЮ СЦЕПЛЕНИЕМ / ВОДИТЕЛЬ! 1. ВЫЖИМАЙ СЦЕПЛЕНИЕ БЫСТРО…" (high).

### 2. Not in the data
- **part**: the large flat ring bracket between the vent plate and the clutch plate. No fixture; it may be in the
  shell.
- **part**: the flanged valve body (6 screws) with pipes, top right of the pillar (g086-a ~1500-1900,0-330), not
  identified.

### 3. Manual
- p168: the "Зима" mode needs the operators' compartment air-intake hatches opened and the ЗИМА—ЛЕТО flap levers set
  (the ЗИМА—ЛЕТО switch is on 0096). p173-174 describe the chassis' heating and ventilation unit. The cabin's air is
  not modelled.

### 4. Contradictions
- `veh.heat_vent` is drawn as a black knob at the panel's top (y +0.08), which is where the photo shows the cast
  electrical junction box (glands, a black cap). No lever or knob is visible by the ВЕНТИЛЯЦИЯ—ОТОПЛЕНИЕ plate. Where the
  actuator is, is unknown.

---

## column_stalk_panel, gear_lever_panel, handbrake_panel
- Gear: the plate's gate is 3 over 3 with a neutral in the centre, which agrees with the manual's 5 + R (p174-175). The
  slot labels are SAM3D's; the digits are illegible.
- Stalk: red-knobbed switch on the column's right (g084-c). Nothing more is legible.
- Handbrake: the manual (p175) has two parking brakes, worked by hand. The T-topped rod between the seats (g083-a,
  g088-a) is the handbrake in the data; that is plausible. **Missing:** the ball-knobbed ratchet-quadrant lever low left
  of the column (see driver_panel 4).
- Also low left of the column: a red cap with a knob on a valve (g083-a 450-560,1340-1450; g086-d top left). It is not
  in the data. Candidates: the кран включения ГОМ, an air-start valve, or a fuel tap. Low.

## stowage_panel
- g086-b: nothing new is legible; the data matches the notes (two covers, a small gauge).

## radio_well_panel (g091-d 1460,580,1900,860)
- A grey cast box with a **red lever in a slot** and a black knob beside it. Its legend is not legible. The form matches
  a Soviet battery master switch. The manual (p163, Fig. 2.60) lists a battery switch **ВБ of chassis 5937**. Candidate
  identification, low-medium; the data has "not identified".
- The yellow canister reads "Н-542"(?): not in the data (loose item).

## header_valve_left / right (g089-a, g089-b)
- The T-handle valve sits on a pipe tee fed from the grey cylindrical unit at the left, which is the wiper motor. That
  supports the register's "wiper tap" reading (medium).
- Not in the data, on the same header: a black box with screw terminals and a stem below (g089-a ~1850-2270,900-1150),
  possibly a switch; a white plastic holder; the periscope blank with its strap and wing nut. The periscope matches
  PLANNED `p:vision`.

---

## Top findings (ranked)
1. **ГОМ semantics wrong.** `pwr.pto` (the generator) is placed on PU-303's ВКЛЮЧЕНИЕ НАГРУЗОК ГОМ *load* button. The
   manual's Fig. 2.60 (p164) engages ГОМ with a "кран включения ГОМ" off the main engine, whose speed is held by the hand
   fuel-pump lever. Neither control exists, `gta.load_gom` is missing, and `pto_generator` does not need the engine
   running.
2. **The main engine is not modelled** (p174: diesel, electric or compressed-air start). The СТАРТЕР button is hidden
   and absent. The drill (p247) has the driver start the ГТА or ГОМ, then the engine, and report «К маршу готов».
3. **Missing control:** the ball-knobbed lever on a ratchet quadrant low left of the column (g086-d, g083-a). It is the
   likely hand fuel-pump lever (hand throttle) of Fig. 2.60. The data's `veh.hand_throttle` is probably a different
   lever.
4. **The PU-303 ГТА dial's scale is wrong:** 10 per turn with 10 at the top (g084-b, g084-d). The data has
   `value_per_turn` 12.
5. **Stale or contradicted notes** in data/procedures: `pwr.gta` ("labels can't be read"), `gta.load_ivp` ("ИВП not
   known"; p164 defines it as the external supply), `gta.lamp_test` ("lights no lamp"; the cabin lamps read it).
6. **Dash nameplate misread:** "ДИФФЕРЕНЦ. ПЕРЕДАЧА" reads **ДИФФЕРЕНЦИАЛ** (g085-a). The differential-lock control
   itself (p175) is missing.
7. **Two unwired toggles** on the small ВКЛ/ВКЛ plate behind the wheel are only a nameplate fixture. The left plate may
   be the "щиток приборов механика-водителя дополнительный" through which the power system is controlled (p163).
8. **ПР ВС identified in part:** ВС is the вентилятор-сепаратор shared by the turbine generator and ГОМ, wired to
   PU-303 (p163, Fig. 2.60). "ПР" is not read.
9. **The radio-well switch box** with the red lever is a candidate for the chassis battery switch ВБ (p163, Fig. 2.60).
10. **The black bottles and the 250 gauge** may be the air-start bottles (p174) rather than fire bottles (book caption).
    Record both.
11. **The fire system's on/off** (p221) is not on the fire box and not modelled.
12. **The oil-pressure gauge runs to 15**, not 12 (g085-a). The 0-6 gauge may have two pointers (an air gauge).
13. **PU-303 extras not in the data:** two fuse holders by ПИТАНИЕ УПРАВЛЕНИЯ, a red lens by КОНТР. ЛАМП (photo only),
    and a fuse strip on the adjacent side face. The ТВГ face is blue, not black.
14. **The driver's ПВ, chest switch and headset jack** are drawn on the instrument plate's right end, where the photos
    show a handle and the 3-pin socket.
15. **The ВЕНТИЛЯЦИЯ—ОТОПЛЕНИЕ knob** sits on what the photo shows to be an electrical junction box. The real actuator is
    not visible.
