# 9K33M2 Osa-AK: what the Polish 1981 manual and the Soviet 1991 textbook say (October 2026)

Two documents read on 3 Oct 2026 (files in `reference/sources/`, git-ignored; text extracted with
`reference/tools/pdf_extract.py`, figures with `figure_prep.py`). Facts only, paraphrased, with page numbers. Page
numbers are the documents' printed numbers unless marked "PDF p".

| Key | Document | Status | Use |
|---|---|---|---|
| `mon_2112_80` | *Przeciwlotniczy zestaw rakietowy 9K33M2 OSA-AK. Wiadomości ogólne. Użytkowanie zestawu*, MON, Szefostwo Służby Uzbrojenia i Elektroniki, Uzbr. 2112/80, Warszawa 1981 (introduced by order pf. 48/Uzbr., 2 Dec 1980). 87 PDF pages, typewritten, Polish | Marked **JAWNE** (unclassified) | Tier 1: an operator country's official manual for exactly the Osa-AK |
| `voenizdat_osa_1991` | *Автономный войсковой зенитный ракетный комплекс «Оса»*, USSR MoD, Voenizdat, Moscow 1991; textbook approved by the C-in-C Ground Forces for PVO SV troops; about the 9K33M3 Osa-AKM (9A33BM3), "all modifications" in scope. 272 pp | Copy-numbered (Экз. №), a marking above it blacked out; imprint Изд. № 13/8918с, Зак. 2278с (the "с" suffix usually marks a secret edition). **Used by the project owner's decision (3 Oct 2026)**, against the public-only rule | Tier 1 for content; describes the AKM, so differences from the AK are marked |

## Polish manual (mon_2112_80): the Osa-AK as a whole

### Data (pp. 10–12)

- Fire zone against aircraft and helicopters at 300 m/s (1100 km/h): height 25–5000 m, range 1.5–10 km, crossing
  parameter up to 6 km; at 500 m/s (1800 km/h): height 100–5000 m, range 1.5–10 km, parameter up to 4 km.
- Detection of a MiG-21-size target by the search radar: 20–35 km depending on height.
- One missile or a salvo of two; command guidance by the three-point method. Low targets (ε 0–30): three-point in
  azimuth, in elevation three-point with a lead above the line, removed as the missile closes (ΔD → 0).
- TV-optical tracking channel; switching from radar auto-tracking to the TV is possible when visibility is 20–35 km.
- March to combat: **at most 4 min, including starting the gas-turbine generator** (p. 11–12).
- Speed: road 60 km/h, dirt roads 30, cross-country 10, water 8 km/h.
- Use from −40 to +50 °C, humidity to 98 %, ground wind to 20 m/s. Radio range ≥ 12 km (R-123M).
- Transport by rail, Il-76, water; the antenna-launcher unit must be folded for rail gauge 02-1 and air transport.

### Vehicle composition (pp. 13–15)

Search radar (RSWP = СОЦ) with antenna-waveguide, transmitter, magnetron AFC (АПЧМ), receiver, a moving-target
canceller (CzPK = ЧПК) shared with the tracking radar, and the PPI; IFF interrogator (NRZ); tracking radar (SSC = СОЦ's
pair, СОЦ/ССЦ) with error-signal and antenna-control channels and a range unit shared with the missile tracker;
two-channel missile tracking radar (SSR = СВР); two-channel command transmitter (SPK = СПК) with encoders; launch
automation; computer (SRP: launch and command units); TV-optical sight (TOW = ТОВ); launcher 9P35M2; **search-antenna
stabilisation 9W611BM2**; function-check system; **gas-turbine generator 9I120**. Six missiles in containers.

### Functional description (pp. 15–20)

- Search antenna turns at **33 rpm**; the beam steps **once per turn** (I, II, III) to cover the chosen elevation
  sector. MTI (TES = СДЦ) with external or internal coherence; signals pass through the canceller (ЧПК).
- IFF: the interrogator's antenna is built into the search antenna; replies go to the PPI.
- The search platform is stabilised (amplifier 0009-5M, two hydraulic drives) so search works on the move; error
  sources: artificial horizon OGW-10P and gyro azimuth GA-3.
- Tracking radar: aimed in azimuth from the search radar's data, in elevation roughly to the **centre of the search
  beam**, then **searches automatically in elevation within that beam**. Monopulse, two channels (main and
  auxiliary) against angle deception; range to the elevation-search indicator, range indicator and range counter.
- НЛЦ (low targets): tracking and missile antennas held at a fixed elevation; **switches on automatically** when the
  target (or missile) reaches that angle; in the tracking radar it switches off automatically when the elevation
  rises 00-01.5 above the fixed value, in the missile tracker on the ВОЗВРАТ signal (p. 21–23).
- Missile tracker (SSR): when the tracking radar starts auto-tracking, the SSR goes to **ЦУ** (start position for
  capture), captures the missile in the **wide beam at 30–50 m**, switches to the **medium beam at 100–150 m**
  automatically, and to the **narrow beam** (guidance) on the computer's signal. Two identical channels; two
  missiles can be guided at once (different carrier frequencies and codes).
- Computer: from the tracking radar's data the launch unit works out the launch zone and sends **launch permission
  to block 0004-11M (the launch console)**; then the command unit forms K1, K2 (steering) and **K3 (fuse arming)** at
  a closing range ΔD that depends on the closing speed.
- **Function check: at most 10 min**: transmitters, receiver sensitivity, antenna-control limits, range units,
  commands K1–K3, a static problem in the computer.
- TV sight: optical head with the camera on the tracking antenna, **monitor on the main console**; tracking is
  **semi-automatic**: the operator keeps the crosshair on the target mark.
- Power: **on the march from the gas-turbine generator (GTA); at a halt from the GTA or the engine-driven generator
  (GOM), or an external supply** (≥ 35 kW, 3-phase 220 V ± 3.5 %, 400 Hz ± 2.5 %, ≤ 180 A per phase; cable in the
  ZCzZ-2 vehicle; up to 80 m away). Vehicle supply 3 × 220 V, 400 ± 10 Hz.
- On the march: search and identification. Pointing the tracking radar, auto-tracking, launch and guidance **only
  after the vehicle stops**.

### Operating modes (pp. 21–23)

| Mode | What it does |
|---|---|
| Search | Antenna turns at 33 rpm; beams **I–II** for the low sector (0–8°) or **I–II–III** for the whole (0–28°), chosen by the commander |
| Identification | General interrogation (ЗАПРОС and ЗАПРОС К): reply shown as a **20° arc** on the PPI; or ПРОПУСКНОЙ (gated) mode: the reply is gated by the search echo, so the mark is much narrower |
| Fast azimuth slew | ("przerzut w azymucie", переброс по азимуту) for large differences between the target bearing and the tracking antenna |
| **ЦУ** (tracking radar) | Rough pointing in elevation: antenna to the **centre of the search beam (I, II or III)** and sector search in elevation within it |
| **АЗ** (автозахват, automatic capture) | Switches on automatic tracking in angles and range; **capture happens when the tracking radar is on the target in all three coordinates** (azimuth, elevation, range) |
| **ТСЦ** (точное сопровождение) | Automatic angle tracking; on after the radar is on the target in angles and range |
| **ПА** (semi-automatic) | Angle tracking with the TV sight |
| SSR "0" | Missile antennas held parallel to the tracking antenna |
| SSR ЦУ | On the **ТСЦ signal**: missile antennas to their start position for capture |
| ШИРОКИЙ / СРЕДНИЙ / УЗКИЙ ЛУЧ | Missile capture (30–50 m), lead-in (automatic at 100–150 m), guidance (on the computer's signal). Commands go out through the wide-beam СПК antenna at capture, then the medium-beam antenna |
| ВОЗВРАТ | Missile antennas and range units back to the start after guidance |
| Stabilisation **ДЕЖУРНЫЙ** (standby) | At a halt: antenna levelled, hydraulic drives off, axes locked. **After the АПУ is unlocked the system switches itself to БОЕВАЯ РАБОТА**, hydraulics on |
| СДЦ (TES) | Separately in search and tracking radars; shared canceller. In the tracking radar's range channel only the **1.5 km around the tracked target** is shown; in the search radar everything except that 1.5 km |
| НЛЦ | As above |

### Layout (Fig. 3, pp. 24–25; text p. 23–24)

Plan, A-A (right wall) and B-B (left wall) views:

- Front cab: **driver front left**, with the **gas-turbine control panel ("pulpit sterowniczy 9I120") beside
  him**; **commander front right**, with the **antenna-lowering board ("tablica opuszczania")** and DP-3B; an R-123M
  radio at each.
- At the front of the left rack: **launch-prohibit lock ("zamek zakazu startu") and a signalling board ("tablica
  sygnalizacji")**.
- **Three operators on one bench along the left wall facing rack SŁ-2M (the main console)**; front to rear:
  **senior (guidance) operator, search operator, range operator**. Telephone TA-57 at the front seat.
- Left wall, front to rear: **SŁ-1M** (stabilisation amplifier, function-check block, gyro control panel: 0004-12M,
  0005-8M, OS51-8M, OK81-4M-II, 0009-5M), **SŁ-2M main console** (three columns, front to rear: 0004-14M,
  WPU-44 + OS81-10M, 0004-11M, OS52-14M1, OK51-6M | 0094-9, OP81-16M, OP63-6M2, OS52-12M, OK81-4M-I | 0062-6M, OS81-9M,
  OK62-5M, OS61-5M, OS81-18M, 0051-7M), **SŁ-3M** (power: 0093-28, 0092-8, 0092-10M, OS94-10, 0096-10M, 0092-4M).
- Right wall: SP-1M (encoder, canceller, power), SP-2M (computer: nine blocks, OR-31M and five control blocks),
  SP-3M-2 (IFF and power 0095-11M). Power cabinet 0096-9M angled behind the bench; gyros OGW-10P, GA-3.
- Engine compartment right front: the 9I120 generator. Antenna-launcher (0004-20M) turns **±342°** in azimuth.
- All antennas have electric and **hand-operated locks for the travel position**. Cooling **LATO / ZIMA** (summer:
  fans; winter: hatches closed, heater OV-65).

### Operating rules (pp. 63–68)

- RF safety: search transmitter: outside cover ≤ 15 min within 50 m; tracking transmitter: a 15–20 m wide sector,
  ≤ 15 min at 80 m.
- **Hatches must not be opened in БОЕВАЯ РАБОТА**; leave the vehicle only after the readiness is changed.
- Switch equipment on and off **exactly in the order of the operating instructions**.
- In a battery, the crew orients the vehicle mutually before combat work; **orientation is lost when the equipment is
  switched off** and must be redone.
- Reloading six missiles with the TZM: **5–8 min**, vehicle fully de-energised, chassis switch off.
- March: target found beyond 20 km → pull 30–50 m off the road; within 20 km → stop at once and carry on.
- Site: level, radius ≥ 10 m; ≥ 500 m from large objects; vehicles ≥ 200 m apart.
- Command-transmitter frequencies interleaved per vehicle (1st: 3 and 1, 2nd: 4 and 2, 3rd: 7 and 5, 4th: 8 and 6);
  missile-tracker receivers on carrier B (A only by special permission).

## Soviet 1991 textbook (voenizdat_osa_1991): controls, timers and the crew drill

Describes the Osa-AKM (9A33BM3); block names carry later suffixes (OP81-16M2 for the AK's OP81-16M, 0096-9M1 for
0096-9M). Page numbers are the book's.

### Power and switching on (pp. 163–166, 51–53, 99–100)

- Power system: gas-turbine unit 9I120 (GTD 9I56-3 + generator; **50 kW**, 67 kg/h of fuel; started from the 24 V
  batteries), engine power take-off generator (**35 kW**, at a halt only), external supply. Controlled from the
  driver's additional instrument panel, **PU-303** and the indication panel (АВАР. ОТКЛ. СЭП button). A frequency
  monitor sheds the load below 360 Hz.
- **Distribution cabinet 0096-9M1** in the operators' compartment: voltmeter; **АППАРАТУРА ВКЛ. / ОТКЛ.**;
  СИСТЕМА СТАБ. ВКЛ. / ОТКЛ. (+ lamp); ПРИВОД βн ВКЛ. / ОТКЛ.; ПРИВОДЫ εн, СВР-I, СВР-II ВКЛ. / ОТКЛ. (+ lamps);
  РАССТОПОР. — СТОПОР. (search antenna rotation) with lamp АНТЕННА СОЦ ЗАСТОПОРЕНА; АНТЕННА — ЭКВИВАЛЕНТ for each
  radar (+ ЭКВИВАЛЕНТ lamps); ТОВ — ОТКЛ.; РЕЛЕ ТЕПЛ. (start ВКЛ.); lamps РАССТОПОРЕНО Δε, εн and РАССТОПОРЕНО βн;
  ОБРЫВ ФАЗЫ lamps; maintenance switches under a cover; the IFF's ДР НРЗ and БР НРЗ switches.
- **Timers after АППАРАТУРА ВКЛ.:** filaments and fans at once → 40 s: −150 V → **1 min: anode supplies of all
  systems** (sweeps appear) → **3 min: the three-minute relay** readies the transmitters' high voltage and powers the
  receivers' inputs (ГОТОВ lamps on both radars and ГОТОВ СПК-I, -II). **3 min after switching on, the stabilisation
  drives come on and the search antenna rises and levels**; УКЛАДКА ПРОИЗВ. goes out. АППАРАТУРА ОТКЛ. switches all off
  but the turret fans (10 min more).
- Lamps lit right after АППАРАТУРА ВКЛ.: АНТЕННА СОЦ ЗАСТОПОРЕНА, ЭКВИВАЛЕНТ (each radar), УКЛАДКА ПРОИЗВ., I ЛУЧ,
  НАКАЛ lamps, РАССТОПОРЕНО Δε, εн; **РУ on the elevation block, but only if ЛЮКИ ЗАКР. is lit on the commander's
  console**; βн ЗАСТОПОР. on the stowing panel while the АПУ is locked.

### Search radar (pp. 49–53)

- OP81-16M2: buttons **ЛУЧ I, II, III, АВТ. I–II ЛУЧ, АВТ. I–III ЛУЧ** and lamps I/II/III ЛУЧ; ВЫСОКОЕ ВКЛ. / ВЫКЛ.;
  **ГОТОВ** (ready for high voltage) and **ВЫСОК.** (on; ГОТОВ goes out); ПОИСК ВКЛ. / ВЫКЛ.; РПЧМ — АПЧМ, РЕГ. РПЧМ;
  PPI knobs ЯРКОСТЬ, ФОКУС, ЯРКОСТЬ ВИЗИРА, ЯРКОСТЬ МЕТОК, КОНТРАСТНОСТЬ, ОСВЕЩЕНИЕ ШКАЛЫ; ДАЛЬНОСТЬ 0–15 / 0–35 /
  10–45 km (seven range rings on 0–35 and 10–45, three on 0–15).
- OP63-6M3: РРУ (full gain for combat), ШТ — СДЦ, СДЦ ВНЕШН. — ВНУТР., КОМПЕНС. ВЕТРА — ОТКЛ., wind compensation
  ГРУБО / ТОЧНО, СА — ОТКЛ. (protection against strong pulse jamming). 0091-18: ТРЕНИРОВКА — РАБОТА СОЦ (magnetron
  training at reduced voltage), lamp НАКАЛ СОЦ. 0062-6M1: ТОК, МОЩНОСТЬ meters and switch. 0065-2M (canceller): АРУ —
  РРУ, gains.
- **Stowing panel:** ПРОХОД ПОД МОСТОМ ВКЛ. / ВЫКЛ. lower and raise the search antenna on the move without stopping
  the stabilisation; lamps УКЛАДКА ПРОИЗВ., ПРЕД. УГОЛ, ГАБАРИТ 3800 (antenna stowed, tracking radar locked, launchers
  lowered).
- Gyro panel 0005-8M: ПИТАНИЕ СЕТЬ — ПТ (start СЕТЬ), РЕЛЕ — Qc (start РЕЛЕ), РЕЛЕ — Ψ, Θ, КОРРЕКЦИЯ ВКЛ. switches
  (start ВКЛ.), ОБОГРЕВ, φ АВТ. — РУЧН. (start РУЧН.), МК — ГПК — АК (at ГПК), **АРРЕТИР.**, latitude knob; lamps
  ОТКАЗ Ψ, Θ (lit while arresting) and ОТКАЗ Qc.

### IFF 1L24-2 (pp. 53–66; AKM equipment)

Switches ДР НРЗ — ОТКЛ., БР НРЗ — МЕСТ. on the cabinet; on OP81-16M2: ПОНИЖ., ДИАПАЗОН — РЕЖИМ, **КОНТР. ИНД.**,
**ЗАПРОС** (1 s per press), **ЗАПРОС К** (special order). Launch blocking (switch and lamp БЛОКИР. ПУСКА on the launch
console): a friendly reply from the tracked target breaks the launch circuit.

### Tracking radar (pp. 66, 96–101)

- Limits: azimuth 330°, elevation −12° to +78°, range 28 km; auto or manual capture; НЛЦ antenna angle 00-10.
- OS81-9M (range indicator): **ПЕРЕДАТЧИК ВКЛ. / ОТКЛ.**, lamps **ГОТОВ** and **ВКЛ. ВЫСОКОЕ**; РПЧМ — АПЧМ;
  **Р I — ЦЕЛЬ — Р II** (fine sweep from the target or missile channel I / II range unit); **РАБОТА — КОНТРОЛЬ —
  КАЛИБРОВКА** (combat at РАБОТА); ЯРКОСТЬ СТРОБА (missile-tracker gate on the coarse sweep).
- OS61-5M1: СДЦ — ШТ, КОМПЕНС. ВЕТРА — ОТКЛ., УВ. ПОМЕХА — ОТКЛ. (range-gate pull-off protection), wind compensation,
  ДОП. УСИЛ. — ОТКЛ.
- OS81-18M (range): **АВТОМАТ** (+ lamp), **ТСЦ** (parallel to the elevation block's), **РУЧНОЕ**, the range wheel
  with a revolution lever (coarse / fine), range dial. 0051-7M: repetition-frequency switch АВТ., РУЧН. F1–F3, F3, F.
- OS81-10M (elevation-search indicator): КОНТРАСТ, ЯРКОСТЬ, ФОКУС; trims. **Fig. 2.37: elevation on the vertical
  axis, range on the horizontal; a raster band per search beam (about −1° to 30°); the target as a mark.**
- OS52-12M1 (azimuth): АЗИМУТ wheel (РУ and ПА modes); **ВЛЕВО — ВПРАВО (fast slew of the АПУ)**; **ЭМУ ОТКЛ. — ВКЛ.**
  (cuts the amplifier's control winding to stop the АПУ turning).
- OS52-14M2 (elevation): УГОЛ МЕСТА wheel; buttons and lamps **ПА, ТСЦ, ЦУ, РУ**; lamp **НЛЦ**; button and lamp **АЗ**
  (auto-capture circuit). Pressing ТСЦ lights ТСЦ and НЛЦ (at elevation below 00-06); РУ puts them out and lights РУ.
- Range indicator, Fig. 2.36: coarse sweep with the transmitted pulse and ground return, the **"pedestal"** (range
  gate), target and missile spikes; fine sweep with the dark "hole" sight and the missile-tracker gate.

### TV sight 9Sh38-2 (pp. 100–104)

Camera KT-101 on the tracking radar's elevation housing; monitor VPU-55 on the main console. Fields **00-60 x 00-80
wide, 00-18 x 00-24 narrow** (lenses 150 / 500 mm; switch within 1 s, УГОЛ ЗРЕНИЯ ШИРОКИЙ — УЗКИЙ); not within 16° of
the sun. РАБ. РЕЖ. ВКЛ. — ВЫКЛ. opens the lens cover; КОНТРОЛЬ — РАБОТА (test chart / crosshair); СВЕТОФИЛЬТР cycles
blue → clear → orange; ЯРКОСТЬ ПЕРЕКРЕСТ., КОНТРАСТ., ЯРКОСТЬ. The narrow lens shows a larger crosshair.

### Missile tracker, computer, command transmitter (pp. 104, 119–121, 137, 150)

- Missile tracker: two missiles **at least 4 s apart**; ±15° from the tracking antenna; to 14 km; capture **0.8 s
  after the missile leaves** (angles) and at **150 m** (range); ЦУ angle 22.8° (3-80). Launch console: lamps
  **ЗАХВАТ** (missile tracker in automatic), **СР. Л.** (range capture, medium beam); switch **ШИР. — СРЕДН.** (medium
  beam in РР mode); button **СХОД** (ФК mode); OK81-4M: ВОЗВРАТ, БАЛАНС; 0004-14M: error meters.
- Computer 9S456M3: ready **100 s** after power-on; combat mode **3 s** after the target is tracked; methods three-point,
  and for low targets "Горка", Н, Ф; lamp **ОТКАЗ СРП** on the launch console.
- Command transmitter: launch console buttons **БОРТ СПК ВКЛ. / ОТКЛ.** (high voltage), lamps **ГОТОВ СПК** (3 min
  after switching on) and **ВКЛ. ВЫС.** (on; ГОТОВ goes out), switch **КАНАЛЫ**, switch **СМЕНА ЛИТЕРЫ — НОРМА**
  (changes the transmitter's frequency letter), lamps **ЗАХВ. АПЧ-I, -II** (out and back within 1 s on a letter
  change).

### Launch automation (pp. 150–156)

- Pre-launch cycle starts on **ВНИМАНИЕ** from the computer or by hand with **БОРТ СПК ВКЛ.**; two missiles prepared,
  one per channel; done in **12.8 s**. **ПУСК ГОТОВ needs:** command transmitters on and locked (ЗАХВ. АПЧ), missile
  range units and timers reset, **hatches closed**, preparation complete; not with БЛОКИРОВКА ПУСКА lit.
- The launch cycle starts when the **key removes the prohibition**; ПУСК fires; no departure → power cut after 2.5 s;
  СХОД puts out ПУСК ГОТОВ; the automation switches to the other channel within 3.7 s and ПУСК ГОТОВ lights again.
- Launch console: **РОД РАБОТЫ** (the automation works only in **БР**, lamp БР), **КАНАЛЫ**, БОРТ СПК, **ПУСК**,
  ЗАПРЕТ ПУСКА lock, **АВАРИЙНЫЙ ПУСК** (lamps АВАРИЙНЫЙ ПУСК ОБЕСТОЧЕН, АВАРИЙНЫЙ ПУСК ГОТОВ), **ИЗДЕЛИЯ (missile for
  the emergency launch)**, missile lamps 1-I … 3-II (blinking = faulty), ПУСК ГОТОВ, БЛОКИРОВКА ПУСКА. Emergency
  launch from the batteries: the missile flies with locked fins and climbs 24–27 s until it self-destructs.
- The book calls the launch console block (0004-11M1) the **commander's console** ("пульт командира").

### Function check (pp. 157–163)

Block 0004-12M1: АНОД ВКЛ., КОНТРОЛЬ СИСТЕМ, К3 (+ lamp), СМЕНА ЗНАКА, УРОВНИ КОМАНД, meters ИП1, ИП2, ПРОВЕРКА
ПЕРЕХОДНЫХ ХАРАКТЕРИСТИК, ПРОВЕРКА СМЕНЫ ЛИТЕРА, ПУСК — ИСХ. ПОЛОЖ. Block 0004-14M: РЕЖИМЫ, ВЫВОД — НАВЕДЕНИЕ, four
meters (КАНАЛ Ц, КАНАЛ Р), ЧАСТОТА. The function check must not delay opening fire (p. 251).

### The crew drill "К бою" (pp. 246–248) and firing (pp. 250–258)

- **Crew chief:** "К бою, включить ГТА (ГОМ)"; tunes the R-123M radios; checks the IFF codes; "Выключить двигатель" at a
  halt; reports to the battery command post "к бою готов" with channels and missiles.
- **Operators:** uncover the АПУ, **unlock the search, tracking and missile-tracker antennas by hand**; with the deputy,
  inspect, set all controls to their start positions, check the intercom, report "первый (второй, третий) готов".
- **Driver:** controls to their start positions; starts the GTA (GOM): "Питание подано"; starts the engine: "К маршу
  готов"; **holds АРРЕТИР. 40 s**; stops the engine on order.
- **Deputy crew chief:** checks АВАР. ПУСК ОБЕСТОЧ. and ДВЕРЬ ЗАКРЫТА; at a halt orients the vehicle (АК — ЗК to ЗК,
  course angle on the setter ЗК-4, РЕЛЕ — Qc to Qc, hold СОГЛАСОВАНИЕ, then back to АК and РЕЛЕ); orders "Расстопорить
  АПУ"; presses ТСЦ then РУ and checks the target-channel error meters (≤ 2 divisions); orders "Включить приводы";
  checks ГОТОВ СПК-I, -II: "Готов СПК"; presses БОРТ СПК ВКЛ., checks ВКЛ. ВЫС. and ЗАХВАТ АПЧМ, no blinking missile
  lamps, ОТКАЗ СРП dark: "СПК, СРП, ракеты готовы".
- **Operator:** checks 220 V 400 Hz on the cabinet, presses **АППАРАТУРА ВКЛ.** and **СИСТЕМА СТАБ. ВКЛ.**, switches
  the TV on; at the deputy's orders **unlocks the АПУ and switches on the drives**; ГОТОВ on the range indicator:
  reports ready.
- **Senior operator:** ГОТОВ on the search scope: "Готов СОЦ".
- Missile preparation 10–12 s; a prepared missile is held at most 10 min, then the next is prepared (a missile can be
  prepared again after 10 min; three cycles, then a 1 h break). With one channel faulty: КАНАЛЫ to the good channel,
  single missiles, then СМЕНА ЛИТ.
- Firing preparation: search with beam I (I–II) for low targets; stop on detection on the march (> 20 km: leave the
  column to the side away from the target; < 20 km: stop in the column); **unlock and slew the АПУ only after a full
  stop**; **tracking-radar high voltage once the azimuth line is on the target mark**; choose tracking and capture;
  launch range from tables; first missile to meet the target at the far edge of the zone; re-engage if ЦЕЛЬ В ЗОНЕ
  stays lit; a second engagement possible up to 300 m/s if the target spends ≥ 25 s in the launch zone.
- Network chart (Fig. 7.2): tracking radar on the target 10 s, capture 6–7 s, computer 3 s, missile preparation 15 s,
  launch 1.2 s, salvo interval 4 s, watching the result 10 s.

## What changed in SAM3D's data (3 Oct 2026)

`data/procedures/osa_ak.json` and `data/cabins/osa_ak.json` (branch `feature/osa-sources-revision`):

- New controls: the power cabinet's АППАРАТУРА, СИСТЕМА СТАБ., РАССТОПОР. — СТОПОР., drive switches and ТОВ (operator);
  hatches (commander); АРРЕТИР. (driver); ТСЦ (deputy commander); ШИР. — СРЕДН. (deputy commander, inert).
- Corrected: ЦЕЛЬ → Р I — ЦЕЛЬ — Р II; СМЕНА РКТ → СМЕНА ЛИТ. — НОРМА (`spk.letter`); the 7-position selector →
  ИЗДЕЛИЯ for the emergency launch (`msl.emergency_select`); РОД РАБОТЫ has БР, ТР, РР, ФК; the antenna-lowering control
  is ПРОХОД ПОД МОСТОМ (air-transport folding uses a lifting device); the АПУ lock is the operator's; АЗ arms
  auto-capture (as ЦУ does in the game); TV fields 4.8° / 1.44°; missile preparation 13 s.
- New subsystems: the 1 min warm-up and 3 min high-voltage relay; stabilisation (3 min) before the search antenna rises
  and search starts; drives (with the АПУ unlocked) before the radar can turn and track; auto-capture and manual
  capture as the radar's capture mode; hatches closed for ПУСК ГОТОВ and РУ.
- Procedures follow the drill: driver (power_up with АРРЕТИР., power_down), operator (equipment_on, deploy_at_halt,
  stow_ssc, equipment_off), commander (lower / raise the search antenna). The practice scenario now launches its drone
  200 s in, giving the crew its 4 min to readiness.
- Not done yet: the vehicle orientation (АК — ЗК, СОГЛАСОВАНИЕ), the IFF controls, the function check, the
  elevation-search indicator display (axes now known). The cabin seats followed in `feature/cabin-bench` (below).

## Two more documents (3 Oct 2026, later)

| Key | Document | Use |
|---|---|---|
| `korovin_tiv_2010` | `OSA_Entw_geschichte_RU.pdf`: a 2012 Word compilation (52 pp.) of Korovin's «Зенитный ракетный комплекс «Оса»» from the ser-sarajkin.narod2.ru mirror of «Техника и вооружение», with its photos and a summary table (PDF p. 51) | Already a source; this copy has all three parts in one file |
| `grezicki_wzu` | M. Gręzicki, J. Walczak (Wojskowe Zakłady Uzbrojenia SA, Grudziądz), *Modernizacja i modyfikacja jako determinanty rozwoju przeciwlotniczych zestawów rakietowych*, journal pp. 217–232 (after 2018; PDF made Jan 2025). Polish | Tier 2, for seat order and crew titles only: the vehicles shown are the modernised Osa-P/PM, not the 1980s console |

### Korovin: what wasn't in our notes yet

- Summary table (PDF p. 51), Osa / Osa-AK / Osa-AKM: range 2–9 / 1.5–10 / 1.5–10 km; height 50–5000 / 25–5000 /
  25–5000 m; parameter 4 / 6 km; target speed 420 / 500 / 500 m/s; target manoeuvre 5 / 8 g; missiles 4 / 6 / 6;
  **reaction time 26–34 / 27–39 / 27–39 s**.
- Firing at low targets: the tracking radar's antenna was locked (стопорилась), so at the 1970 trials the crew could
  not judge the target's height and used a height-to-range table to pick the launch range (PDF p. 26). Agrees with the
  НЛЦ mode already in the data (`voenizdat_osa_1991` p. 100: antennas held at a fixed elevation).
- In heavy jamming: angles from the TV sight, range from the search radar, three-point guidance (PDF p. 24). Matches
  `emcon_tv_engagement` and `track_range_semi_auto`.
- Osa-A work (from 1971): light front panels and **red dots beside the knobs, toggles and switches that are set to
  their starting position** before live firing (PDF p. 31). Supports the `red_dot` placements.
- 9M33M2 (Osa-AK missile): wings fold in pairs to fit the square container; better radiation hardness; 5-year storage
  guarantee (PDF p. 35). The Osa-AK's 27 m floor came from a dual-mode fuse receiver that checks height at arming.
- OKR «Мара» (from Nov 1975, the Osa-AKM): helicopters needed a changed guidance method, a sharper ИКО, multi-mode
  warhead control and fuse-delay correction; semi-automatic TV angle tracking was proposed for them (PDF p. 36).
- Organisation: a battery is four vehicles and a battery command post; the regiment's control battery holds the
  PU-12 (PU-12M) and a P-15 (P-19) (PDF p. 40–41). Recorded in `OSA_RESEARCH_2026-10.md` §7.
- Nothing in it changes the console: it is a development history with exterior photos only.

### WZU paper: seats and crew titles

- Fig. 4 (camera views of a live firing, Polish Osa-PM): the **guidance operator (operator naprowadzania, ON)** sits
  beside the **search operator (operator poszukiwania, OP)** on the forward side, and the **range operator (operator
  odległości, OO)** beside OP on the rear side; a safety officer (OB) supervises. This is the same order as
  `mon_2112_80` Fig. 3: guidance, search, range, front to rear, on the left-wall bench.
- Fig. 3: the 9F632.M1 trainer (cabin imitator of the 9A33BM2/3 upgraded to Osa-PM standard, five crews at once).
- The upgrade replaced the PPI with a digital one (OP81-16PM) and added an EO head with laser rangefinder (100–30 000 m)
  and video tracker (BZK01). Not 1980s equipment; not used.

### What changed in SAM3D's data (`feature/cabin-bench`)

- `data/cabins/osa_ak.json`: the deputy commander moves from the right wall to the front seat of the left bench, with
  the deputy commander's panel (TV monitor, elevation, launch) as the front column of the main console; the senior operator (search) sits
  in the middle and the operator (range) at the rear; the power cabinet is behind the bench. The cabin's status now
  cites the two sources. Distances and panel sizes remain estimates.
- Test: `cabin: the Osa operators share the left-wall bench, guidance in front, then search, then range`.

## Cabin photos (`cabin_photos`, 3 Oct 2026, later)

Five sets of real-cabin photos, read for layout (labels are mostly too small):

- **Cab and bench** (opisybroni forward view; USAF photo looking aft; Instagram): driver front left at the wheel,
  commander front right; one bench along the left wall facing three tall console columns; the operators' seats are
  chairs on that bench. Confirms `mon_2112_80` Fig. 3.
- **Front column (guidance)**, top to bottom, matching Fig. 3's list 0004-14M, WPU-44 + OS81-10M, 0004-11M,
  OS52-14M1, OK51-6M:
  1. meter block: four meters (ε and β of the target channel, ε and β of the missile channel), a selector (target and
     missile error signals, compensation, two PJ test positions), INJECT and FREQ (from the photo with English tape
     labels);
  2. TV monitor, and to its right the **round elevation-search indicator OS81-10M** with three knobs below it;
  3. launch block 0004-11M (label readable): two КАНАЛЫ button groups with three lamps above each, a vertical numbered
     list beside a selector (ИЗДЕЛИЯ), a small square meter, **РОД РАБОТЫ** (label readable) and a second rotary
     beside it, red push buttons, a guarded **АВАРИЙНЫЙ ПУСК** (label readable);
  4. elevation block with jacks, an amber lamp and the hand wheel on the right;
  5. a bottom block.
- **Middle column (search)**: a row of eight round lamps; the search indicator OP81-16M (in the Ukrainian photos it
  has an added 0–360° azimuth bezel and a rotating cursor of parallel lines, and shows the tracking radar's azimuth
  line); a block with a printed diagram; the azimuth block with a cranked hand wheel; a bottom block.
- **Rear column (range)**: a block of three meters; the range indicator with a small rectangular screen; two blocks
  with printed diagrams; the range block (АВТОМАТ, ПОЛУАВТОМАТ, ТСЦ, РУЧНОЕ along the top with lit push buttons,
  СКОРОСТЬ, a large hand wheel on the right with a range window above it); a bottom block.

### SAM Simulator's panel photos (`samsim_panels`)

The game's two console forms are built on three straight-on photographs of a real vehicle's main console, one per
column, embedded in `sam.exe` (610 × 982, 670 × 982 and 610 × 982 px; extracted to `reference/samsim/exe_img/`, not in
git). Their block plates are readable and match `mon_2112_80` Fig. 3 exactly:

- Range (rear): 0062-6M (generator currents, modes), **OS81-9M** range indicator (ГОТОВ, ВКЛ. ВЫСОК., ПЕРЕДАТЧИК
  ВКЛ. / ОТКЛ., РЧЧМ / АПЧМ, РАБОТА / КОНТРОЛЬ / КАЛИБР., ЯРКОСТЬ СТРОБА, ЦЕЛЬ Р I — Р II, astigmatism and shift
  knobs), **OK62-5M**, **OS61-5M** (wind compensation ГРУБО / ТОЧНО, СДЦ / ШТ., УВС), **OS81-18M** range block
  (АВТОМАТ, ПОЛУАВТОМАТ, ТСЦ, РУЧНОЕ, range window 1 division = 1000 m / 5 m, hand wheel, СКОРОСТЬ П А, ПЕЛЕНГ),
  0051-7M.
- Search (middle): the azimuth readout strip (ΔД, Δε, β and ε of the tracking radar), **OP81-16M** (ГОТОВ / ВЫСОК.
  window, ВЫСОКОЕ and ПОИСК ВКЛ. / ВЫКЛ. button pairs, ДАЛЬНОСТЬ, beam lamps I–III ЛУЧ and their buttons, ОСВЕЩ.
  ШКАЛЫ, IFF ЗАПРОС, РЧЧМ / АПЧМ, brightness knobs), **OP63-6M2** (РРУ, СДЦ / ШТ., wind compensation, СА, СДЦ ВНЕШН. /
  ВНУТР.), **OS52-12M** azimuth block (ПОДСВЕТКА, БАЛАНС ПА, ВЛЕВО / ВПРАВО lever, βс dial, АЗИМУТ hand wheel, ЭМУ
  ОТКЛ. / ВКЛ.), OK81-4M.
- Guidance (front): the meter strip (ЧАСТОТА, КАНАЛ Ц, КАНАЛ Р), the TV block (РАБ. РЕЖ. ВКЛ., УГОЛ ЗРЕНИЯ ШИРОК. /
  УЗКИЙ, КОНТРОЛЬ / РАБОТА, contrast and brightness) with **OS81-10M** to its right, the launch block (КАНАЛ II and
  КАНАЛ I lamp-button groups: СХОД, ЗАХВАТ, СР.Л., УЗКИЙ ЛУЧ, К.З., ГОТОВ СПК, ВКЛ. ВЫС., 1–3; centre МЕТОД, ЦЕЛЬ В
  ЗОНЕ, ПУСК ГОТОВ, ВНИМАНИЕ, БР; МЦ, the ШР list, СМЕНА ЛИТ. — НОРМ., КАНАЛЫ, БОРТ СПК, ПОМЕХА, СР.Л. / ШИР., the ДЦ
  meter, ЛЮКИ ЗАКР., ОТКАЗ СРП, РОД РАБОТЫ, ИЗДЕЛИЯ, ПУСК, the ЗАПРЕТ ПУСКА key, АВАР. ПУСК ОБЕСТОЧ. / ГОТОВ and the
  guarded АВАРИЙНЫЙ ПУСК), **OS52-14M** (ПА, ТСЦ, НЛЦ lamps; ПА and ТСЦ buttons; ПОДСВЕТКА; ЦУ, АЗ, РУ lamps and
  buttons; БАЛАНС ПА; εс dial; elevation hand wheel on the right edge), OK51-6M.

The game's sprites match these photos at 0.96–1.00, which gives exact positions for 20 controls; the rest were read
off the labels.

### What changed in SAM3D's data

- `data/cabins/osa_ak.json`: each operator panel is the game's photo of its column at 1 mm per pixel (0.61, 0.67 and
  0.61 m wide, 0.982 m high, side by side along the left wall, centred 1.35 m above the ground so the search screen
  is at eye height). Every control, lamp and screen is at its position in the photo. The wheel locks have no picture
  (the game unlocks a wheel by right-clicking it), so they sit just below their wheels. Space is left for OS81-10M.
  An overlay of the placements on the photos was checked by eye (`reference/samsim/overlay_check.jpg`).
- `data/procedures/osa_ak.json`: sources `grezicki_wzu`, `cabin_photos` and `samsim_panels`.
- Test: the bench test also checks that the columns are taller than wide and that screens sit above wheels and the
  launch block between the TV and elevation blocks.

## Elevation-search indicator OS81-10M (display added)

`voenizdat_osa_1991` p95 and Fig. 2.37 (p97): a two-coordinate sector indicator with a rectangular raster and a
brightness mark, for pointing the tracking antenna in elevation from the search radar's designation. It covers 31° of
elevation and 2 km of range; the middle of the range span is tied to the tracking radar's range gate (the "visir" on
the range indicator). The sector is split into three ranges for search beams I, II and III, and the raster sits in a
different place on the screen for each (Fig. 2.37: beam I −1° to 5°, beam II 3° to 9°, beam III 6° to 30°). During ЦУ
the antenna oscillates in elevation through the beam the target was found in (p86: the elevation channel's scan
circuit; the tracking servo drives the indicator's elevation sweep). The block holds the auto-capture circuit: armed by
ЦУ and АЗ, it fires when the target is in the antenna's beam (a mark appears) and the range gate is on the target,
sending АВТОМАТ to OS81-18M and ТСЦ to OS52-14M.

In SAM3D (`feature/cabin-bench`):

- Console data: `sensors.track.elevation_search` in `data/procedures/osa_ak.json` (the beams' elevations from Fig.
  2.37, the ЛУЧ position for each beam, scan period 2 s (estimate), tracking beam 1° (`korovin_tiv_2010`)).
- Engagement model: during ЦУ the antenna scans the selected beam (in the automatic programmes, the beam the search
  radar sees the target in near the azimuth line) and drives the elevation wheel; otherwise it points where the wheel
  is. The radar captures only when azimuth and range hold and the antenna's beam is on the target in elevation, so
  the wrong beam never captures. The NPC deputy commander aims elevation by the indicator once the target shows on it.
  The crew drill now leaves the search radar on АВТ. I–III (an NPC can't pick the beam the echo appeared in).
- Display kind `elevation_scope` (`data/cabins/README.md`); the Osa's `elev_search` sits where the round screen is in
  the game's photo (right of the TV), with one raster per beam placed as in Fig. 2.37 (approximate).
  `simcore::ElevationScope` keeps the fading marks (1.5 s afterglow, an estimate) of targets inside the 2 km window that
  the antenna's beam passed over.

## Vehicle orientation and IFF (added to the data)

**Orientation** (`voenizdat_osa_1991` p48, p50–52, p246, p249; `mon_2112_80` p65):

- The stabilisation system measures the vehicle's heading **Qc** (clockwise from north to the hull's axis) with the
  gyro half-compass **GA-3**, which only holds north once oriented.
- The crew chief finds Qc with an artillery compass (PAB) and the TV sight by mutual sighting, and commands it.
- The deputy crew chief, at a halt, sets **АК — ЗК** to ЗК and the Qc on the course setter **ЗК-4** (block 0005-8M,
  panel ПУ-11), sets **РЕЛЕ — Qc** to Qc (azimuth correction), and holds **СОГЛАСОВАНИЕ** until the Qc scale on OR-31M
  stops. Then АК — ЗК goes back to АК and РЕЛЕ — Qc to РЕЛЕ. The orientation is lost when the equipment is switched
  off.

In SAM3D: `sensors.orientation` and the controls `orient.ak_zk`, `orient.relay`, `orient.qc`, `orient.sync` on a new
gyro panel at the front of the bench (its layout is an estimate); the drill `orient_vehicle` (deputy commander). The
engagement model keeps the gyro's heading:

- It turns with the hull, and it is slaved to the setter in ЗК + Qc (1°/s, or 15°/s after СОГЛАСОВАНИЕ until they
  agree; rates estimated).
- Without power it restarts taking the hull to face north.
- Every azimuth the crew sees and sets (scope, azimuth wheel) is in the gyro's frame. An unoriented vehicle shows the
  picture turned by its error, and plotting-board designations (true azimuth) point the antenna wrong.
- The crew chief's Qc is logged when ЗК is selected; an NPC deputy turns the setter to it.

**Drift and the compass work** (added 17 Oct 2026; `voenizdat_osa_1991` p48, p51–52, p249–250):

- After orienting, АК — ЗК goes to АК and РЕЛЕ — Qc to РЕЛЕ: the GA-3's correction unit then works as latitude
  correction (p48). Block 0005-8M carries the potentiometer φ with the scale ШИРОТА that feeds the site's latitude into
  it, a switch φ АВТ. — РУЧН. (start РУЧН.), and a lamp ОТКАЗ Qc for a lost phase of the gyro's supply (p51–52). The
  textbook gives no drift figure; the Earth's rate (15.041 deg/h × sin latitude) is textbook physics of a gyro
  half-compass. In SAM3D: the wheel `orient.latitude` and the drift in `simcore/orientation.hpp`; the GA-3's own
  drift (0.5 deg/h) is an estimate.
- OR-31M carries the follow-up systems' scales and a switch СС — ОТКЛ. for maintenance (p52). In SAM3D: the dial
  `or31_qc` on the SP-2M rack reads the gyro.
- The crew chief's compass work (p249–250): the artillery compass ПАБ 20–50 m in front of the vehicle; the launcher
  laid on it with the TV sight (or the sight); the angle qв read off block 0094-9; the compass's azimuth on the vehicle
  read on its black ring and drum, turned by 30-00, corrected (ΔAm from the map's declination and convergence, the
  compass's daily change and its instrument error) to the grid bearing α; Qc = α − qв (or 60-00 + α − qв). The
  textbook names the commander's check of "the correctness of the orientation" among the readiness checks (p252).
  In SAM3D it is one timed activity of the commander's at a halt (180 s, 0.2 deg error: estimates).

**IFF** (`mon_2112_80` p14, p21; `voenizdat_osa_1991` p17, p53–56):

- The interrogator's antenna is built into the search antenna.
- General interrogation (**ЗАПРОС**, **ЗАПРОС К**): a friend's reply is a 20° arc on the PPI beside its echo.
- In the gated mode (КЛАПАН) the echo gates the reply, which keeps the mark narrow and suppresses replies from
  aircraft the radar can't see.
- ЗАПРОС К is a control interrogation confirming a friend.
- The 1991 book's 1L24-2 (AKM) adds codes, modes I/II and launch blocking, none of which are in the Osa-AK sources;
  not modelled.

In SAM3D:

- `sensors.search.iff`, with the controls `iff.query`, `iff.query_k` and `iff.gate` placed from the game's photo of
  OP81-16M, and a lamp lit while interrogating.
- A press interrogates for 1 s; friendly targets (`"iff": "friend"` in a scenario) within 30 km reply. In КЛАПАН only
  those on the scope reply.
- The PPI paints replies as arcs as the sweep passes them.
- The acquisition drill interrogates once the echo shows. Firing at a friend is logged as a crew mistake.

## The 1991 textbook read in full (20 Oct 2026)

`voenizdat_osa_1991` was read cover to cover on 20 Oct 2026 (the OCR of all 273 PDF pages; the formulas and tables
checked against the page images). The 3 Oct reading took the consoles, the timers and the drill; this pass adds the
rest. Printed page numbers (PDF page minus one). Everything is paraphrased; the book describes the Osa-AKM (9A33BM3,
missiles 9M33M3 and 9M33M2), so AKM-only items are marked.

Contents: 1 the complex (pp. 10-23); 2 the vehicle: search radar and stabilisation (24-53), IFF 1L24-2 (53-66),
tracking radar and TV (66-104), missile tracker (104-120), fire-control computer 9S456M3 (120-137), command
transmitter (137-150), launch automation and launcher 9P35M2 (150-156), function check (156-162), power, ventilation,
pressurisation (163-172), chassis 5937 (172-176); 3 the missile (177-187); 4 the support vehicles and the trainer
(188-205); 5 operation: safety, frequencies and emission masking, maintenance, transport, special conditions (206-224);
6 combat use: regiment, battery capabilities, battle and march orders, the march, control (224-245); 7 combat work:
preparation, firing under jamming, kill assessment (246-267).

### Chapter 1: the complex

- Zone (p15): at 300 m/s 25-5000 m, 1.5-10.3 km, crossing parameter to 6 km; at 500 m/s 100-5000 m, 1.5-10 km, to 4 km;
  helicopters hovering or to 100 m/s on the TV: 10-25 m, 2-6.5 km (AKM). Single-missile kill probability without
  jamming 0.4-0.96 (aircraft), 0.24-0.76 (helicopters). MiG-19-size detection 20-30 km by height. One target channel,
  two missile channels. Three-point; low targets Горка or Н; helicopters below 25 m Ф (AKM).
- Protection (p16): chaff including second-trace returns, angle repeaters, range-gate pull-off, asynchronous pulses from
  neighbouring radars; the tracking radar can hold a noise jammer in angle. TV tracking to 20-25 km in good visibility.
  IFF «Пароль», «Кремний-2», «Кремний-2М» (AKM), blocking a launch at a friend. March to combat within 4 min. Two R-123M
  and an R-124 intercom; heating, fire extinguishing, NBC filtering. 48 h of continuous work with refuelling; 250 km on
  dirt roads with 2 h of the turbine.
- Crew (p17): five: crew chief, guidance, search and range operators, driver-mechanic-electrician.
- Sequence (p20-22): a target not answering the IFF is judged for whether it can reach the zone; if it is dangerous,
  the vehicle stops and captures. **ВНИМАНИЕ comes 15 s before the target enters the launch zone and starts the
  preparation of one missile per channel and the command transmitters' high voltage; the transmitters work into their
  dummy loads until the launch, for concealment.** The missile tracker captures the missile in angles 0.8 s after it
  leaves (wide beam, 30-50 m out) and in range at 150 m (medium beam); near the line of sight it is guided in the
  tracking radar's own beam. The fuze is armed 120 m before the meeting; 150 m past it the computer gives 1 s of "up"
  commands and the channel resets; the missile destroys itself at 25-28 s. Launches at least 4 s apart. Against a
  jammer ПОМЕХА selects method Н and arms the fuze with the medium beam.

### Chapter 2: the vehicle

- **Search radar** (pp. 24-53). Instrumented range 45 km. Guaranteed MiG-19 detection with no screening: 12 km at 25 m,
  17 at 50, 22 at 100, 35 at 1000, 30 at 4000-5000 m; 27 km at 1000 m in MTI, with or without chaff. 33 +- 2 rpm;
  azimuth beam 1.3 +- 0.4 deg; Fig. 2.1 beams I and II 4 deg wide on axes near 2 and 6 deg, III at least 5.5 deg on an
  axis near 12 deg (shaped upward). Designation accuracy to the tracking radar: azimuth +-00-15, elevation to the beam's
  middle, range +-300 m. Pulse 0.45 us. Internal coherence (fixed clutter, with wind compensation and, on the move, the
  vehicle's own speed from a wheel tachometer) or external (fixed and moving clutter); a shared double canceller (search
  radar 0-35 km, tracking radar the 1.5 km around its target); staggered periods against blind speeds, a fixed one
  against second-trace clutter; beyond 35 km the plain channel shows 35-45 km unfiltered. The transmitter is ready for
  high voltage only after the 3-min relay, with its covers shut and the waveguide pressurised.
- **PPI** (p40-41): scales 0-15 km (rings at 2, 7, 12 km), 0-35 and 10-45 km (seven rings 5 km apart); a rotating
  course-parameter scale with seven lines 2 km apart in the bezel (the parameter is read with it, p256).
- **Stabilisation 9V611BM2** (p42-48): levels the antenna to +-00-08 at a halt, +-00-30 / 00-20 on the move; ready in
  3 min; stows for a bridge in 10 s. Modes БР, ДР (standby at a halt: pumps off and axes locked, back to БР when the
  azimuth drive is switched on), bridge stow, emergency (the antenna tipping, or a drive on its stop: ПРЕДЕЛЬНЫЙ УГОЛ;
  cleared on level ground with ПРОХОД ПОД МОСТОМ). Levelling is lost when the hull leans more than 9 deg on the move.
  The travel lock handle in the antenna unit must be released before the stabilisation will switch on.
- **Signs of normal operation** (pp. 51-53, 99-100, 119-120): the sweeps at 1 min (the search sweep and the tracking
  radar's azimuth marker drawn in opposite directions, the marker's dark range strobe moving with the range wheel; the
  range indicator's "pedestal" and "hole" sight); at 3 min the antenna rises, ГОТОВ lamps, a bright noise strip on the
  search sweep and on the range indicator (growing with range on the coarse sweep); ground clutter once search starts;
  ТСЦ lights ТСЦ and НЛЦ (elevation below 00-06), РУ puts them out.
- **IFF 1L24-2** (pp. 53-66, AKM): bands III and VII, modes I, II (guaranteed friend) and ЗАПРОС К; «Клапан» gates the
  reply by the echo (the mark then sits 1 km beyond the echo); with the target tracked and the command transmitters on,
  it interrogates automatically as the search antenna passes the tracking azimuth and a guaranteed friend breaks the
  launch circuit (БЛОКИР. ПУСКА). Interrogation lasts 1 s per press.
- **Tracking radar** (pp. 66-100): auto-tracks a MiG-19 in chaff to 25 km; 330 deg, -12 to +78 deg, 28 km; works only
  at a halt; hidden conical scan, so a repeater can't learn the scan rate; a notch keeps the missiles' replies out.
  Antenna up to 30 deg/s in azimuth, 15 deg/s in elevation. НЛЦ: on automatically during tracking when the elevation is
  00-06 or less, the antenna held at 00-10 while the error is measured, off at 1.5 thousandths above. Range-gate
  pull-off: the angle track is lost when the pull-off stops, because the angle channels are gated by the range tracker;
  a leading pull-off is beaten by СДЦ (staggered periods), a lagging one by УВ. ПОМЕХА (a blanking circuit after the
  echo). Both radars at 2800 Hz; tracking pulse 0.225 us. The radios' receivers are blanked during the radar pulses.
- **TV 9Sh38-2** (pp. 100-104): silhouette and type at medium range, a dot far out; not within 16 deg of the sun; the
  lens cover opens with РАБ. РЕЖ.; КОНТРОЛЬ shows a test chart; the narrow lens shows a larger crosshair.
- **Missile tracker** (pp. 104-120): two channels, missiles at least 4 s apart, +-15 deg about the tracking antenna,
  14 km; capture works for launch errors up to 10 deg; columns held at 22.8 deg (3-80) for the launch, at 0-13 above the
  tracking antenna in НЛЦ. Lamps ЗАХВАТ and СР. Л. on the commander's console.
- **Fire-control computer 9S456M3** (pp. 120-137): ready 100 s after power, combat mode 3 s after tracking starts; fully
  automatic. Its launch system (p122-129) takes the target as flying straight on, finds the meeting point (the flight
  time a function of the meeting range) and the parameter, slant parameter and predicted path, and lights **ЦЕЛЬ В
  ЗОНЕ while the meeting point is in the zone**: eight conditions at once, of which these are printed in full: the far
  edge 10300 m; the slant parameter at most 8700 - 8 V (eq. 2.18); a receding meeting point at most 8700 - 8 V (2.19);
  the receding inner cylinder 14 V - 1300 (2.21); a "corridor" P >= V / 0.15 (the time to swing the launcher from the
  approach to the pursuit, 2.22); the near sphere 1500 m. Two more use speed functions F1, F2 the book does not give
  (the far edge before the parameter, 2.17; the manoeuvre-load limit, 2.20; a cone 107 m below the horizon, 2.23).
  **ВНИМАНИЕ: (range - 10300 - 21 V) / V <= 15 s, the 21 s being the missile's flight to the far edge** (2.24). It also
  sets the fuze delay for pursuit and slow targets (lamp Vг < 100) and method Ф (AKM). Guidance: Горка below 00-30;
  Ф from the TV below 00-06, under 100 m/s and 7 km, the missile diving at 25 deg (no miss mode: it bursts on the
  ground); Н with ПОМЕХА, needing no range. Inertial range tracking holds the target's range tracker between K3 and the
  miss point so the first missile's burst doesn't capture it.
- **Command transmitter** (pp. 137-150): 100 kW, 0.75 us, eight letter frequencies; wide and medium beams; triggered at
  a quarter of the radars' rate; **into its dummy load from ВНИМАНИЕ (or БОРТ СПК ВКЛ.) until ПУСК switches it to the
  antenna**; a false code until 0.8 s after departure; СМЕНА ЛИТ. swaps the two channels' frequencies and codes (the
  lock lamps go out for under 1 s).
- **Launch automation** (pp. 150-156): two missiles prepared at once, 12 s (12.8 s for the cycle), held 10 min; ПУСК
  ГОТОВ needs both transmitters on and locked, the missile range trackers and timers reset, the hatches shut; after
  ПУСК the missile's own turbogenerator, then the motor 0.3 s later; no departure in 2.5 s cuts the power; the next
  channel within 3.7 s.
- **Power and housekeeping** (pp. 163-176): turbine 50 kW at 67 kg/h, take-off generator 35 kW at 20 kg/h (halt only);
  АППАРАТУРА ОТКЛ. leaves the antenna unit's fans on for 10 min; summer and winter ventilation (ЗИМА — ЛЕТО, hatch
  levers); compartment pressurisation needed above 1 km and in rain; the chassis has a roof hatch, two windows (one an
  emergency exit), a door in the engine bulkhead, 10.65 t kerb, 18.68 t loaded.

### Chapters 3-5: missile, support, operation

- Missile 9M33M3 (pp. 177-187): 126.3 kg, 3158 mm, warhead 14.27 kg; dual-thrust motor 13.2-19.2 s; flight 24-27 s;
  fuze 9E316M-1 switched on by K3 150 m out, a 20 ms delay against slow targets; four arming stages (motor pressure,
  8 g, 2.5-3.3 s, K3); launched at a constant 27 deg; if it hasn't burst by 25-28 s it is steered up to destroy itself
  at a safe height.
- TZM 9T217BM2 (p188): 1-12 missiles, loads a launcher in 5-8 min, carries 640 l of fuel for it. Maintenance vehicle
  9V210M3 (p189): four launchers, deploys in 40 min, crew of four. Alignment vehicle 9V914, ZIP-2, missile test
  station 9V242-1 (6-7 missiles an hour), ground kit 9F16M2.
- Trainer 9F632 (pp. 201-205): two crews at once; 1-4 groups in formations, manoeuvres, terrain masking, IFF, two ARM
  launches; noise, asynchronous, repeater, pull-off and chaff jamming; modes autonomous, «Сопряжение» (the crew in its
  own vehicle on a simulated picture) and «Наложение» (simulated over real, jamming set from a remote panel), with an
  objective scoring panel. The model for SAM3D's own training mode.
- Safety (p207): hatches shut in combat work; leave only when the readiness is cancelled; no loading with the vehicle
  on; RF limits (search: 15 min within 50 m; tracking: a 15-20 m sector, 15 min within 80 m).
- Frequencies (pp. 208-211): neighbouring vehicles at least 200 m apart; letters per vehicle (Table 5.1: search Б1-Б4,
  tracking В1-В4, command channel I А3, А4, А7, А8 and II А1, А2, А5, А6, the same pairs as `mon_2112_80`); wartime
  letters freed by a district-wide signal (two vehicles a battery retune their radars); in peacetime the command
  transmitters radiate only on ranges; even into the loads the emission is detectable along the beam to 10 / 4 / 2 km.
- Special conditions (pp. 222-224): marching with the equipment on, all hatches shut but the needed ones, the antenna
  locks released, pressurisation on; in forest always in travel position; low bridges at 3.8 m loaded (the procedure);
  water crossing preparation; humidity then frost: warm on filaments 15-20 min before the anodes.

### Chapters 6-7: the battery and its fire

- Regiment (pp. 224-226): five batteries, a control and radar reconnaissance battery (the regiment's command post), a
  technical battery. **Battery** (p226, AKM): command, control section, four launcher crews, a MANPADS section, a supply
  section, a maintenance crew; PU-12M, four launchers, three MANPADS launchers, two TZM, maintenance and ZIP-2 vehicles.
- Reconnaissance (p228): radar, the MANPADS section's passive direction finder, and visual (naked eye 4-7 km,
  binoculars 8-25 km, the launcher's TV 10-24 km); five target channels.
- Fire capability (pp. 229-230): M = channels x firings x readiness 0.8-0.9 x **control 0.3-0.5 (fire control from the
  battery post by signal means only)** x losses x kill probability with a jamming/manoeuvre factor 0.7-0.8 (0.3-0.5
  against intense countermeasures, from the local wars); a well-trained crew's firing cycle at the far edge 73 s. A
  candidate yardstick for the air war's abstract resolution.
- Mobility (p231): **battery into combat order in 35-50 s, back to march order in 15-30 s**; a new target 40-120 s by
  crew training (one missile).
- Orders (pp. 232-239): the march column (PU-12M, two launchers, a TZM, two launchers, a TZM, maintenance), 25-50 m
  gaps, at least 200 m between launchers when an attack is expected; on the march some launchers search and, on a
  detection, leave the road, unlock and fire; lines of pairs up to 3 km apart, 200-500 m within a pair, 1.5-3 km behind
  the covered companies, TZMs within 500 m; defence 3-5 km from the forward edge; reserve positions within 2 km;
  1000 m apart under nuclear threat; **sectors of responsibility for each launcher and the MANPADS section**; halts of up
  to 1 h every 3-4 h, a 2 h meal halt; **on the march the battery commander rides the PU-12M at the column's head and
  signals by flags and lamps; radio is forbidden except for fire control while repelling an attack**; changing position
  only on the regiment commander's order.
- Control (pp. 240-245): the commander's sequence (task, time, preliminary orders, situation, decision, reconnaissance,
  the oral combat order, support, political work), the working map (attack directions, helicopter and jammer lines, ARM
  launch lines, SOC detection limits, the zones' far edges for the likeliest heights, the friendly air corridor, call
  signs and signals) and the battery's logs. Lebanon 1982: underrating Israeli EW cut the Syrian «Феда» group's
  effectiveness.
- Preparation (pp. 246-258): the drill (already in the data); the commander's report «к бою готов» with channels and
  missiles; orienting the vehicle with an aiming circle 20-50 m ahead (Qc = grid bearing - the launcher's βн);
  readiness No. 1; the function check must not delay fire; clutter cards prepared in advance; **stopping on the move
  beyond 20 km leaves the column to the side away from the target, within 20 km stops in the column**; the tracking
  transmitter on only when its azimuth marker is on the echo; the parameter read with the PPI's parameter ruler, the
  speed by a stopwatch over the 5 km rings; **Table 7.1, the far edge of the launch zone head-on: 8 / 9 km at 25-50 m,
  11 / 12 at 100 m, 13 / 14 at 150 m, 16 / 15 km at 200-5000 m (up to / over 300 m/s)**; Table 7.2, the time in the
  launch zone by height and parameter; re-engagement at up to 300 m/s if the target spends at least 25 s in the zone,
  otherwise a salvo; the battery commander distributes targets.
- Firing (pp. 258-267): signs of each jamming type on the scopes (asynchronous jamming as spirals out from the PPI's
  centre); ECCM switched on only when search or tracking is otherwise impossible. Low targets (AKM: under 180 m and
  00-06): launch at 6-8 km (under 300 m/s) or 12-14 km (400-500 m/s) whatever the lamp; manoeuvring and low targets two
  missiles 4 s apart. Groups: the lead or the brightest mark; a battery post's designation inside an unresolved group as
  the group's centre plus a selection rule (the first to fourth from the left of the nearest); two missiles 6-8 s apart.
  **ARM**: the signs (jamming stops, the target manoeuvres or heads in at zero parameter; a small fast mark separating);
  fire with least emission: the search transmitter on for 2 turns in every 3-4, the TV, the tracking transmitter on only
  at the Table 7.3 ranges (24 / 20 km for a meeting at the far edge, 9 / 7 km at the near edge, over / under 300 m/s),
  the search radar off while the tracking radar holds, the tracking radar off after the burst. Pursuit: within 7 / 5 / 3
  km at 100 / 200 / 300 m/s. Helicopters: a smeared mark in clutter; TV search with the screen's bottom edge on the
  crests; duty launchers with 45-60 deg sectors; first missile within 6.5 km; launch smoke on the TV: stop the wheels,
  go wide. TV firing: ПОМЕХА with a hand-set range; without a range the far edge when the target fits in the crosshair's
  gap; out of the sun the two launchers furthest apart. Small drones: one missile whatever the lamp, 1.5-7 km. Chaff:
  СДЦ, external coherence; noise jammers: the slowest-moving strobe, the strongest beam, gain down to narrow it.
  **Kill assessment** (p267): the burst mark and the missile's reply gone; the target's mark breaking up or vanishing;
  its elevation dropping; a group splitting; jamming or chaff stopping; fire on the TV. A miss: the reply lost early or
  still there after the meeting; the target's motion unchanged.

### Where it disagrees with what SAM3D had

| Item | Before | The book | Used |
|---|---|---|---|
| ЦЕЛЬ В ЗОНЕ | the target's own range in 1.5-10.3 km | the meeting point in the zone (p121-129); Table 7.1 launch ranges (16 km at altitude) | the book |
| ВНИМАНИЕ | 15 s before the target reaches 10.3 km | 10.3 km + 36 s x V (eq. 2.24) | the book |
| Missile average speed | 450 m/s (game value) | 10.3 km in 21 s: 490 m/s (p129) | the book |
| Pursuit edge | flat 6.5 km (`flak11`) | 8700 - 8 V (eq. 2.19): 7.1 km at 200 m/s, 6.3 km at 300 | the book (flak11's figure is the 275 m/s value) |
| Command link before launch | radiating from ВНИМАНИЕ (a reading of `mon_2112_80` p67) | into its dummy load until ПУСК (p21, p140, p155) | the book |
| Command link | 5 kW, 1 us, 6 deg beam (estimates) | 100 kW, 0.75 us, medium beam 1.8-2.4 deg (p138) | the book |
| Radar repetition rate, pulses | 2000 / 3000 Hz, 0.4 / 0.2 us (estimates) | 2800 Hz both, 0.45 / 0.225 us (p32, p68, p78, p89) | the book |
| Halts on the march | 1.5 h (`video_osa`) | up to 1 h (2 h for a meal) | the book |
| Short halt | always off the road, alternating sides | within 20 km stop in the column; beyond, off the road away from the target | the book (as `mon_2112_80` p66) |
| Tracking antenna rate | 15 deg/s (`flak11`) | 30 deg/s azimuth, 15 elevation (AKM) | `flak11` kept for the EM layer's one rate |
| Pursuit launch rule | none | within 7 / 5 / 3 km at 100 / 200 / 300 m/s (p262) | recorded only: with 490 m/s these meetings fall 0.9-1.4 km past eq. 2.19's edge, so the rule of thumb and the computer's formula don't agree under a constant average speed |

### What changed in SAM3D (20 Oct 2026)

- `simcore/engagement.hpp`: `EngagementTuning::MeetingFor`, `LaunchZoneFor`, `LaunchRangeFor`, `AttentionFor`; the
  zone bands judged at the meeting point; the slant-parameter and receding-edge limits; a shot's result at the meeting
  time (out of reach past the flight time). `crew_decisions.hpp` (the drill master) and `battery_fire.hpp` (the other
  launchers) use the same launch zone and meeting time.
- `data/gameplay/osa_ak.json`: zone sources, the pursuit edge, the slant parameter, Table 7.1, the missile's 490 m/s.
- The command transmitters wait in their loads until the launch (`scenario_runner.hpp`); the scenario switch
  `own_unit.em.command_link_in_load` is gone. `data/em/osa_ak.json`: the command link's power, pulse, rate and beam; the
  radars' repetition rate and pulses; the book's detection minimums and beams as notes.
- `data/command/battery_tactics.json` re-sourced to chapter 6; `short_halt_in_column_m` and the short halt's 20 km
  rule (`battery.hpp`; the NPC commander passes the target's range); halts 1 h.
- `data/systems/osa.json`, the fuel block and the crew's zone, departure and result callouts cite the book.
- Tests: the meeting point and the launch zone against hand calculations, ВНИМАНИЕ's eq. 2.24, a scenario launch beyond
  10.3 km meeting the target inside it, the command link silent until the launch, the short halt's two cases.

### Recorded, not built

- (Built 20 Oct 2026, roadmap E6, `docs/EM_ARCHITECTURE.md` §35: the ARM drill, pp262-264. The search bursts, Table
  7.3 in `data/gameplay/osa_ak.json` `arm`, the tracking transmitter off after the burst, the search radar off while the
  tracking radar holds a target alone, the PPI signs and the turn away. Still not built from that page: the signs on the
  tracking radar's indicators and on the TV, the TV for tracking under the threat with the range by short bursts, p264.)
- Small drones' signs, helicopter sectors and duty launchers; the other group rules (the lead or the brightest mark when
  tracking a group; which mark to take when a group splits before or after the launch; two missiles 6-8 s apart).
- The stabilisation's ДР mode and ПРЕДЕЛЬНЫЙ УГОЛ fault, ventilation summer / winter, pressurisation, the noise strip
  and clutter on the scopes as signs of normal operation; the PPI's parameter scale.
- The inertial range tracking at a burst, the fuze-delay and Vг < 100 lamp, the trainer's modes for a training screen,
  the fire-capability formula as an air-war yardstick.

Built since (20 Oct 2026, `scenario_runner.hpp` `MarchRadioBan`): **the march's radio discipline** (pp. 232-239). While
the battery forms its column and marches, the battery commander's orders go by flags (lamps at night) from the column's
head, seen by the column's drivers and commanders, not on the battery net; the post's net orders and its report to the
regiment are refused on the march with the reason, and the NPC commander neither moves a jammed net nor calls the
launchers. Radio opens again for fire control while the battery repels an attack: a hostile within the range at which the
column stops where it is to fight (`short_halt_in_column_m`, p254), or a target being engaged.

Built since (20 Oct 2026, `simcore/kill_assessment.hpp`): **kill assessment from the instruments' signs** (p267). The
outcome model still decides what happened; the crew learns it only from the signs: the burst and the missile's reply gone
(the range indicator, else the launch console's ЗАХВАТ lamp), the target's mark gone (read from the instruments: the range
indicator, else the search scope), its elevation dropping (the elevation dial), a group's mark splitting, a jammer's strobe
or a chaff drop stopping (the search scope), fire on the TV; for a miss the reply lost before the meeting or still there
after it, the target flying on unchanged, the target whole on the TV. Each sign is called by the seat whose instrument
shows it (`kill_sign_*`); the deputy commander (the TV and the elevation dial: the book's fullest source) calls the result
once a sign settles it, no sooner than 2 s, or with what he has at 8 s, and may call it wrong where the signs settle
nothing (a near miss at night: the burst, then the target flying on; a target diving away after a near miss). The crew's
picture, the report to the battery and the commander's next move (wait for the call; drop a target called destroyed;
engage again after a miss) follow his call. A target called destroyed that is still on the search scope 10 s later is
reported flying on and taken back. The World has no wreck, so the wreck's fall, fire and the group and jamming signs are
drawn; the miss's signs are read from the live target. The game numbers: `data/gameplay/osa_ak.json` `kill_assessment`
(estimates).

Built since (20 Oct 2026, `simcore/group_designation.hpp`): **a target in a group in a designation** (p261, printed page;
the "pp. 258-267" above is the section). When the post's picture can't resolve a group (its hostile marks linked closer
together than 3 km, the PU-12M's own error after `flak11`: a sam3d reading), its designation is the group's centre and a
selection rule: «группа 6201, центр, азимут …, дальность …, правило 2» (`order_assign`). The NPC battery commander gives
each launcher on the group the next rule, a launcher per aircraft, four at most; a player's designation does the same
unless a rule is given. The launcher's crew counts the group on its own screen: rule n is the n-th from the left of the
nearest targets, on one azimuth (within the search radar's 1.3 deg beam, p24) by range; with no rule the crew chief
chooses (SAM3D: the first). "The nearest targets" is read as the group nearest the given centre: the echo nearest it
within 6 km and the echoes linked to it within 2 km (estimates). In the Osa the commander passes it on
(`designation_group`), the senior operator counts it on his scope and reports the pick (`group_pick`), and the commander
takes that aircraft; the post's other launchers count from their own places. A rule beyond the group's size takes the last
(sam3d).
