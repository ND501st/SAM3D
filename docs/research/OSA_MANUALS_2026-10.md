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

