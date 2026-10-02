# 9K33M2 Osa-AK: console and control map (October 2026)

A control-by-control and feature-by-feature plan for the Osa cabin, written 2 October 2026 after a research pass on
the real vehicle's workplaces and on SAM Simulator's panels. It extends `OSA_RESEARCH_2026-10.md` (which settled the
crew, timings and lamps) down to the level of individual panels, switches and displays, and says what each means for
`data/procedures/osa_ak.json`, `data/cabins/osa_ak.json` and the roadmap. Public and declassified material only.

Documentation only: no code or data files change with this file.

## 1. Bottom line

- **The real cabinet layout is now known at block level.** An NVA technical-service veterans' catalogue
  (`rwd_mb3`) captions every block in the operators' cabinet (Schrank SL-2M2) and numbers the seats: **1 operator
  (range), 2 senior operator (search scope, azimuth), 3 deputy commander (elevation, TV, launch)**. That is the
  order SAM Simulator shows left to right: "Range Officer – Angle Officer – Firing Officer" (`samsim_osa` p13; the
  Russian edition names the middle seat «Офицер Угла Места», though its panels are the search scope and azimuth).
- **One instrument is missing from SAM3D: the elevation search indicator** (block OS81-10M, captioned together with
  the TV monitor at the deputy commander's seat). SAM Simulator has it ("SSC elevation/range indicator"); SAM3D has only the εс dial.
- **Power is the driver's job.** The power-supply control panel PU-303 sits at the driver's left. SAM3D has the
  gas-turbine switch at the commander's seat.
- **Switch-on has real panels:** the primary power cabinet to the left of the operators (OO96-9M, with the red
  switch-off button), the gyro switch-on panel (OO05-8M), an antenna-lowering panel and two function-check blocks.
  SAM Simulator deliberately leaves them out; SAM3D needs them for the cold start (roadmap 6.2).
- **Seven items in SAM Simulator's sprites are not in SAM3D's console yet.** Corrected by the eyes-on pass (§12):
  the 7-position missile selector (3-II … ОТКЛ. … 3-I), АВАРИЙНЫЙ ПУСК, the guarded СМЕНА РКТ switch (not a +27 V
  selector: the +27 V list is the pin legend of test socket Ш7), the КАНАЛЫ channel selector (the sprite's "БОРТ" is
  the БОРТ СПК label below it), the range indicator's ЦЕЛЬ selector (next to the ГОРИЗ. СМЕЩ. trim knob), the
  РАБОТА / КОНТРОЛЬ / КАЛИБР. selector and the TV monitor's knobs. The "β / ε lamps" are not lamps: they are the βс /
  εс dials' backlight (ПОДСВЕТКА), unlit and lit.
- **The best documents are in the German Federal Archives**, listed as digitised: the NVA's own regulations for the
  9A33BM2 (A 327/1/154, 1984) and the Osa-AK training instruction (1983). They would settle most of the open labels.
- **The first session could not look at any picture.** A second session on the PC (2 Oct 2026, task 6.0) viewed
  the SAM Simulator manual's panel photos (`samsim_osa` pp. 13–35) and the Osa sprites, and corrected this file; its
  findings are in §12. Still not seen: the `rwd_mb3` photos and drawings, and the running game (§9, §11).

## 2. What was read, and what wasn't

| Material | Result |
|---|---|
| `rwd_mb3`: rwd-mb3.de *Technikkatalog*, pages 9A33 (data, interior photos, exterior photos, layout drawings), 9I120, 9S456, 9W611, 1S51, 1L24, 9Sch38, 9T217, PAB-2M | Read (text and photo captions). **New, the main source for §3–§4** |
| `samsim_thread`: SimHQ development thread, 52 pages | Pages 1–4, 8–23, 25–33, 35–51 read for console statements. **Skipped: 5, 6, 7, 24, 34** (the forum rate-limited me) and 52 |
| German Federal Archives catalogue (Deutsche Digitale Bibliothek) | Three NVA documents found and listed as digitised (§10). Not opened: the archive's viewer was down for maintenance |
| `valka_osa`, missilery.info layout drawing, WWP photo book listing | Captions and legends read |
| `flak11` | **Not reachable** from this session (site blocks automated readers; the Wayback Machine is blocked here). Used as recorded in `OSA_RESEARCH_2026-10.md` |
| SAM Simulator manual, sprites and the running game | First session: not reachable. **Eyes-on pass (2 Oct 2026): manual (English, with the Russian edition's text for comparison) and sprites viewed; the running game not** (§12) |
| Any photograph or drawing | First session: not viewed. Eyes-on pass: the manual's panel photos only (low resolution: about 100 px per panel row) |

Everything from the web came through an automated page reader that summarises. **Block designations and numbers
below are transcriptions to check against the page before they enter a data file.** German sources write Cyrillic
designations in Latin letters (OS82-9M = ОС82-9М, WPU-55 = ВПУ-55); they are kept as the source wrote them.

## 3. The real vehicle: compartments, seats, cabinets

Source for this section: `rwd_mb3` (9A33 interior photo captions and layout-drawing legends), unless noted.

| Place | Who | What is there |
|---|---|---|
| Front cab, left | Driver | Driving instrument panel; **power-supply control panel PU-303 at his left** (the power system's other blocks: BA-303, BKA-333, rectifier WU-1); entry hatch above. The vehicle also carries a PNV-57E night-vision device and TNPO-170A periscopes (positions not captioned) |
| Front cab, right | Commander | Seat with the DP-3B dose-rate meter at his right. Where his radio and intercom sets sit is not in the captions |
| Fighting compartment (*Kampfraum*) | Operator, senior operator, deputy commander | Three workplaces in front of **cabinet SL-2M2**: seat 1 operator (*Funkorter*), seat 2 senior operator (*Oberfunkorter*), seat 3 deputy commander (*Stv. Kommandant*) |
| Fighting compartment, other cabinets | – | SP-2M2 (computer 9S456; IFF blocks behind it; magnetic amplifiers and distribution in its right bay), SL-1M1 (function-check block, gyro switch-on panel, MTI), SL-3M1, SP-1M1 (command coder), SP-3M2, and a power cabinet transcribed both as OO96-2M2 and OO96-9M2 (probably one cabinet) |
| Fighting compartment, loose items | – | Antenna-lowering switch panel; indicator board (*Anzeigetafel*); radios R-123M I and II (both in the layout legend); R-124 intercom; TA-57 field telephone; null-current indicator; multimeter; PAB-2M aiming circle and its tripod; document pouch |

Five seats in all. The crew list in `rwd_mb3` is the same five as `flak11`.

### Blocks in the operators' cabinet (SL-2M2)

In the caption order of the source photo. Which column a block sits in is **inferred from its function** (the
caption order runs left seat, centre seat, right seat, then power supplies); confirm on the photo.

| Block | Source's name | What it is for the game | Seat (inferred) |
|---|---|---|---|
| OS62-6M1 | Automatic gain control | Tracking-radar gain (ambient) | 1 |
| **OS82-9M** | *Entfernungssichtgerät* | **Range indicator** (two sweeps, counter) | 1 |
| OK62-5M1 | Main amplifier, "Rakete" radar | Ambient | 1 |
| OS61-5M1 | Main amplifier, "Ziel" radar | Ambient | 1 |
| **OS81-18M** | *Entfernungsmesseinrichtung* | **Range-tracking unit**: range wheel, АВТОМАТ / ПОЛУАВТОМАТ | 1 |
| OO51-7M | Error-signal selection, "Ziel" | Ambient | 1 |
| OO94-9 | *Skalenmechanismen* | **Scale mechanisms**: the βс / εс dials | 2 |
| **OP81-16M3** | *Rundsichtgerät* | **Search scope (ИКО / PPI)** | 2 |
| OP63-6M3 | Main amplifier, search radar | Ambient | 2 |
| **OS52-12M** | *Seitenwinkelsteuerblock* | **Azimuth control block**: azimuth wheel | 2 |
| OK81-4M I | Automatic range tracker, "Rakete" (channel I) | Missile channel I (no player control) | 2 |
| **OO04-14M** | *Block zur Funktionsprüfung* | **Function-check block** | 3 |
| **WPU-55** | TV picture receiver (9Sch38-2) | **TV monitor**, screen 171 × 127 mm | 3 |
| **OS81-10M** | *Höhenwinkelsuchsichtgerät* | **Elevation search indicator** (elevation against range) | 3 |
| **OO04-11M2** | *Kommandantenpult* | **Launch console** ("commander's panel"; worked by the deputy commander) | 3 |
| **OS52-14M2** | *Höhenwinkelsteuerblock* | **Elevation control block**: elevation wheel | 3 |
| OK51-6M | Error-signal selection, "Rakete" | Ambient | 3 |
| OO93-28, OO92-10M, OO96-10M, OO92-8, OS94-10, OO92-4M | Power-supply blocks | Ambient | bottom row |

Other blocks named: OO04-12M second function-check block and OK81-4M II (channel II range tracker), in SL-1M1's left
part; **OO05-8M gyro switch-on panel**, OS51-8M (MTI) and OO09-5M in its right part; OK82-2M command coder in SP-1M1;
a "modernised version of the search scope" photo exists too.

**Checked against the SAM Simulator manual's photos (eyes-on pass, §12).** The name plates confirm the column
inference for seats 1 and 2 and show the stacking, top to bottom. Seat 1: range indicator, OK62-5M, OS61-5M (it
carries the tracking radar's СДЦ switch), OS81-18M (range wheel). Seat 2: OP81-16M (search scope), OP63-6M2 (the
search radar's СДЦ switch), OS52-12M (azimuth wheel). The βс and εс dials are mounted on the faces of the azimuth and
elevation control blocks, not on a separate panel (OO94-9 is probably the mechanism behind them). Plates that differ
from `rwd_mb3`: the search scope reads ОП81-16М (not -16M3), its amplifier ОП63-6М2 (not -6M3), the amplifiers ОК62-5М
and ОС61-5М (no M1). These are probably variant suffixes: the creator's photos are not of an NVA vehicle. The range
indicator's plate reads ОС81-8М or -9М at the photo's resolution, where `rwd_mb3` was transcribed as OS82-9M:
recheck both.

ePap independently names the power panel:
"the RED button which switches off the OSA-akm is in another panel 0096-9M1 … on your left looking towards the
existing panels" (`samsim_thread`, 19 May 2013), and the creator: "lots of switches (left of the Range Officer)",
intentionally not simulated (20 Jan 2014).

### The four displays

The creator's numbered list (`samsim_thread`, 26 Mar 2013), now matched to real blocks:

| # | SAM Simulator | Real block | Seat | SAM3D display |
|---|---|---|---|---|
| 1 | SSC range indicator | OS82-9M (`rwd_mb3`); photo plate ОС81-8М or -9М, recheck | Operator | `ssc_range` (have) |
| 2 | SOC azimuth/range indicator | OP81-16M3 | Senior operator | `soc_ppi` (have) |
| 3 | TV | WPU-55 | Deputy commander | `tov_monitor` (have) |
| 4 | **SSC elevation/range indicator** | OS81-10M | Deputy commander | **missing** |

## 4. Control map by station

Columns: **Id** = id in `data/procedures/osa_ak.json` (existing) or proposed (*italic*). **Status**: have / move
(wrong seat today) / fix / new. **Tier**: S = needed for the vertical slice, F = fidelity after the slice works,
A = ambience (visible, inert or nearly), L = later phase. **Conf.** uses the repo's scale. Labels are given only
where a source shows them; "label unknown" means it must come from the photos or the regulations.

Ids stay as they are in roadmap 6.1 (only `station` changes), so procedures and tests don't churn. New controls take
function prefixes (`pwr.`, `stab.`, `ant.`, `chk.`, `iff.`, `msl.`, `rad.`, `nav.`, `drv.`).

### 4.1 Driver (front cab, left) — `driver`

| Id | Label | What it does | Status | Tier | Evidence, conf. |
|---|---|---|---|---|---|
| (input axes) | – | Steering, throttle, brake | have (W/S/A/D/Space) | S | – |
| `pwr.gta` | label unknown (on PU-303) | Start/stop the gas-turbine unit (50 kW; on the move and at a halt) | **move** from commander | S | `rwd_mb3` 9I120, 9A33 interior: PU-303 at the driver's left. medium |
| `pwr.pto` | label unknown (on PU-303) | Reserve generator off the drive engine (35 kW; at a halt only) | have | S | same. medium |
| *`pwr.emergency_off`* | label unknown | Emergency shutdown (*Havarieabschaltung*) | new | F | `rwd_mb3` 9I120. low |
| *`pwr.external`* | label unknown | External (grid) power through the hull connector | new | L (7.6) | `rwd_mb3`: external-power connection and connection panel on the hull. medium |
| *`drv.night_vision`* | – | PNV-57E on/off for night driving | new | L | `rwd_mb3`. medium |
| *`drv.water`* | label unknown | Water jets and wave shield for swimming | new | L | Two water jets (*Za rulyom* article on the BAZ-5937, zr.ru); wave shield in the missilery.info drawing. low |
| Instruments | – | Speed, engine, fuel; generator voltage | new | F | Fuel matters for 7.4. Layout unknown |

The driver's title in `wrk` is "driver-mechanic-electromechanic", which fits the power panel being his.

### 4.2 Commander (front cab, right) — `commander`

| Id | Label | What it does | Status | Tier | Evidence, conf. |
|---|---|---|---|---|---|
| *`rad.battery_net`* | R-123M | Receives the PU-12's designations (azimuth, range); reports | new | S for phase 7 (7.2) | `flak11`, `rwd_mb3`. high that it exists; panel placement unknown |
| *`rad.intercom`* | R-124 | Passes orders to the crew | new (the order menu, 8.2) | S | `rwd_mb3`. high |
| *`nav.orient`* | label unknown | Orientation of the vehicle with the navigation set and the PAB-2M aiming circle at a halt | new | F (6.5) | `flak11` (navigation set), `rwd_mb3` (PAB-2M and tripod carried). Controls unknown. low |
| – | DP-3B | Dose-rate meter at his right | new | A | `rwd_mb3`. high |
| – | TNPO-170A | Looking out | new | A | `rwd_mb3`. high |
| `pwr.gta` | | | **move** to driver | | see 4.1 |
| `apu.lock`, `apu.stow` | | | **move** to the fighting compartment | | see 4.6 |

### 4.3 Operator, seat 1 (range) — `operator`

Blocks OS82-9M, OS81-18M. SAM Simulator's "Range Officer".

| Id | Label | What it does | Status | Tier | Evidence, conf. |
|---|---|---|---|---|---|
| `ssc.tx_cover`, `ssc.tx` | ПЕРЕДАТЧИК ВКЛ. (black push-button under a hinged guard) / ОТКЛ. (red push-button) | Tracking-radar transmitter | have | S | `samsim_osa` p16, p24 photo (on the range-indicator block); the creator's first sequence (`samsim_thread`, late March 2013) gives it to the range operator. high |
| *Lamps* | ГОТОВ, ВКЛ. ВЫСОК. | Transmitter ready / high voltage on, above the transmitter buttons | new | S | `samsim_osa` p16, p24 photos. medium |
| `ssc.range_wheel` | no label on the wheel; dial "Г.О. 1 делен. – 1000 м, Т.О. 1 делен. – 5 м" (coarse and fine scales) | Moves the range gate | have | S | `samsim_osa` p16, p24. Block OS81-18M **confirmed** by its plate |
| *`ssc.range_fast`* | – | Fast movement of the range gate | **change**: in SAM Simulator it is the wheel's coarse mode (one mouse button fine, the other coarse), not a separate control | F | `samsim_osa` p16 item 9. On the real block the only unexplained controls near the wheel are two push-buttons labelled ПЕЛЕНГ (function unknown). medium |
| `ro.semi_auto` | ПОЛУАВТОМАТ | Range-only tracking by the radar | have | S | `samsim_osa` p26. high |
| *Range mode buttons* | АВТОМАТ, ТСЦ, РУЧНОЕ (with ПОЛУАВТОМАТ, a row of four push-buttons on OS81-18M) | Range auto-track, angle auto-track, manual range | new (the lamps exist) | S | `samsim_osa` p24, p16 photos. The game drives them itself; who presses them in the drill is unknown. medium |
| *`ssc.pa_rate`* | СКОРОСТЬ П А (− / +) | Aided-tracking rate | new | F (6.6) | `samsim_osa` p24 photo. medium |
| `ssc.mti` | СДЦ / ШТ. (toggle on OS61-5M) | MTI on the tracking radar | have | F | `samsim_osa` p16 item 10. high |
| *`ssc.target_select`* | ЦЕЛЬ, 3 positions (scale marks unreadable) | Unknown; on the range indicator, right of ГОРИЗ. СМЕЩ. | new (this is sprite set `osa014`–`016`, formerly `unk.horiz`) | F | `samsim_sprites`; `samsim_osa` p16, p24 photos. low |
| *`ssc.check_mode`* | РАБОТА / КОНТРОЛЬ / КАЛИБР. | Range-indicator check and calibration | new (sprites `osa012`–`013`) | A | `samsim_osa` p24 photo. medium |
| Lamps `avtomat`, `tsc` | АВТОМАТ, ПОЛУАВТОМАТ (OS81-18M); ТСЦ (elevation block) | Range and angle auto-track | have | S | `samsim_osa` p24 |
| Display `ssc_range` | – | Range indicator: whole range above, magnified gate below, counter | have | S | `samsim_osa` p24. **Add:** the missile's own response is visible and tracked on this scope after launch (ePap, 26 Jan 2014) |
| Receiver and scope controls | OS61-5M: КОМПЕНСАЦИЯ ВЕТРА ГРУБО (0–360°) and ТОЧНО, КОМПЕНС. ВЕТРА / ОТКЛ., ДОП. УСИЛ. / ОТКЛ., УВ. ПОМ. / ОТКЛ.; range indicator: ВЕРТ. СМЕЩ., ГОРИЗ. СМЕЩ., ЯРКОСТЬ, ФОКУС and АСТИГМАТ. for each sweep (I, II), ЯРКОСТЬ СТРОБА Р, РПЧМ / АПЧМ, ССЦ РПЧМ; OS81-18M: БАЛАНС АВТОМАТА; test jacks | Wind (clutter-motion) compensation, extra gain, receiver frequency control, CRT trims | new | A | `samsim_osa` p16, p24 photos. No gain (РРУ) control is visible at this seat. medium |

From the thread (creator and ePap, 14–16 Jun 2013): the operator watches both the scope and the range dial; the dial
shows changes in target speed or heading that the search scope can't, and range can be set by hand for a hovering
target.

### 4.4 Senior operator, seat 2 (search radar, IFF, azimuth) — `senior_operator`

Blocks OP81-16M3, OS52-12M, OO94-9. SAM Simulator's "Azimuth Operator".

| Id | Label | What it does | Status | Tier | Evidence, conf. |
|---|---|---|---|---|---|
| `soc.search` | ПОИСК ВКЛ. / ВЫКЛ. (two push-buttons) | Search antenna rotation | have | S | `samsim_osa` p15. high |
| `soc.hv_cover`, `soc.hv` | ВЫСОКОЕ ВКЛ. / ВЫКЛ. (guarded), with a ВЫСОК. indicator window to its left | Search-radar transmitter | have | S | `samsim_osa` p15. high |
| `soc.range` | ДАЛЬНОСТЬ 0÷15 / 0÷35 / 10÷45 КМ (3-position toggle, sprites `osa017`–`019`) | Scope scale | have | S | `samsim_osa` p15; 0–35 is the working scale, 0–15 a zoom once tracking (ePap, 19 May 2013). The manual's sequence switches to 10–45 first, then to 0–35 at 35 km (p21–22), so the game does not start at 10–45. No red dot is visible on this switch. Start position still unknown; today's 0–15 is a guess |
| `soc.beam` | I ЛУЧ, II ЛУЧ, III ЛУЧ; I–III ЛУЧ (marked АВТ.) and I–II ЛУЧ | Elevation beam | **fix**: five push-buttons in two groups, with a three-window lit indicator (I/II/III ЛУЧ) beside them, not one rotary | S (6.4) | `samsim_osa` p15 photo (**confirmed**), `flak11`. high |
| `soc.mti` | СДЦ / ШТ. (toggle on OP63-6M2, below the scope) | MTI, to 35 km | have | S | `samsim_osa` p15 item 12. high |
| *`iff.interrogate`* | ЗАПРОС (reading at low resolution; with a КОНТР. button and a lamp) | IFF interrogation; the reply shows beside the target's mark on the search scope | new | F (6.4) | Lower left of OP81-16M (`samsim_osa` p15 photo; not numbered in the manual, not used by the game). `flak11` (senior operator interrogates); `rwd_mb3` 1S51: reply "next to the target mark on the PPI". medium |
| `ssc.az_lock`, `ssc.az_light`, `ssc.az_wheel` | lock; ПОДСВЕТКА; АЗИМУТ | Turns the АПУ / tracking radar in azimuth, ±330° | have | S | `samsim_osa` p16, p22. high |
| *`ssc.az_fast`* | ВЛЕВО / ВПРАВО | Fast slew left / right | new | F | `samsim_osa` p16 item 6 (one mouse button each way; noted in the console file, not modelled). high |
| Scope knobs | OP81-16M: ОСВЕЩ. ШКАЛЫ, ЯРКОСТЬ МЕТОК, ФОКУС, КОНТРАСТНОСТЬ, ЯРКОСТЬ, ЯРКОСТЬ ВИЗИРА, РПЧМ / АПЧМ, РЕГ. РПЧМ, ОТКЛ. ПРИЕМ / КЛАПАН; OP63-6M2: РРУ, КОМПЕНС. ВЕТРА, ГРУБО, ТОЧНО, СА, СДЦ ВНЕШН. / ВНУТР. | Scale lighting, mark and sweep brightness, focus, contrast, receiver frequency and gain, wind compensation | new | A | `samsim_osa` p15 photo. medium (small lettering) |
| Display `soc_ppi` | – | Search scope with the tracking radar's azimuth line and range mark | have (rings to fix, 6.4) | S | `samsim_osa` p15, p21–22 |
| Display `beta_c` | βс | Azimuth dial, backlit by ПОДСВЕТКА (sprites `osa094`–`095` unlit / lit) | have | S | `samsim_osa` p16; mounted on OS52-12M's face (mechanism probably OO94-9) |
| Lamps *`beam_1..3`* | – | Which beam the echo came from | new | S (6.4) | `samsim_osa` p15, p21 |

Who moves the range mark on the search scope is described two ways: the range operator (creator, late March 2013) or the
senior operator with both handwheels (`flak11`). Keep the wheel at seat 1 (where its block is) and let procedures
say who calls it.

### 4.5 Deputy commander, seat 3 (elevation, TV, launch) — `deputy_commander`

Blocks WPU-55, OS81-10M, OO04-11M2, OS52-14M2, OO04-14M. SAM Simulator's "Elevation Operator" / "Firing Officer".

| Id | Label | What it does | Status | Tier | Evidence, conf. |
|---|---|---|---|---|---|
| `ssc.el_lock`, `ssc.el_light`, `ssc.el_wheel` | lock; ПОДСВЕТКА; УГОЛ МЕСТА | Elevation of the tracking radar and TV | have; **fix** limits to −12°…+78° (now 0…90 estimate) | S | `samsim_osa` p17, p23; limits `flak11` |
| `fo.cu_az` | **ЦУ** (push-button with lamp) | Automatic acquisition after azimuth designation | have; **fix the label**: ЦУ and АЗ are two buttons | S | `samsim_osa` p23: the text says "ЦУ АЗ", the photo's marker is on the ЦУ button. high |
| *`fo.az`* | АЗ (push-button with lamp, between ЦУ and РУ) | Unknown; not used in the manual | new | F | `samsim_osa` p17, p23 photos. Function to ask the creator. low |
| *`fo.tsc`* | ТСЦ (push-button, below the ПА / ТСЦ / НЛЦ lamps) | Probably selects angle auto-track | new | F | `samsim_osa` p17 photo. low |
| `fo.ru` | РУ | Manual tracking | have | S | `samsim_osa` p17, p25; ePap: "PY (RU) manual tracking" |
| `fo.pa` | ПА | Aided (semi-automatic) angle tracking with the TV. Real: the operator tracks by hand with rate aiding; SAM Simulator tracks for him | have; behaviour to change (6.6) | S | ePap, 5 May 2014: "semi tracking (or aid tracking, so called PA mode)"; "Only manually" |
| `tov.power` | РАБ. РЕЖ. ВКЛ / ВЫКЛ | TV camera | have | S | `samsim_osa` p17 |
| `tov.fov` | УГОЛ ЗРЕНИЯ ШИРОКИЙ / УЗКИЙ | Lens | have; fields to fix | S | See §5 |
| *`tov.filter`* | СВЕТОФИЛЬТР | Light filter against glare; the camera is blinded by the sun, three filters (`samsim_thread`, 26 Jun 2013) | new | F (6.6) | Sprite `osa220`. medium |
| *`tov.brightness`*, *`tov.contrast`*, *`tov.focus_*`*, *`tov.crosshair`*, *`tov.test`* | ЯРКОСТЬ, КОНТРАСТ, ФОКУС КИНЕСКОПА / ВИДИКОНА, ПЕРЕКРЕСТ., КОНТРОЛЬ / РАБОТА | Monitor adjustments | new | A (brightness F for night) | Sprite `osa220`. medium |
| `fo.mode` | РОД РАБОТЫ: БР (red dot), ТР, and two more marks read as РР and ФК | Live fire, or training with the 9Ф632 trainer connected; the other positions unknown (ФК may be the function check) | **fix**: name the second position ТР; in ТР the real vehicle can't lock and fire. The game has two positions (sprites `osa022`–`023`) | S | ePap, 6 Jul 2013: "Rod switch is in TP position!!! Not in red dot! In real OSA you can't lock and fire with the switch in this position! Only with simulator connected." Dial marks: `samsim_osa` p29 photo, low resolution. medium (РР / ФК low) |
| `fo.key` | ЗАПРЕТ ПУСКА | Launch-authorisation key | have | S | `samsim_osa` p29–30. high |
| `fo.launch` | ПУСК (red) | Launch | have | S | `samsim_osa` p30. high |
| `fo.bort_spk_cover`, `fo.bort_spk` | БОРТ СПК: ВКЛ. (guarded) and a red ОТКЛ. push-button | Starts missile and command-link preparation by hand | have; add the ОТКЛ. button | S | `samsim_osa` p34, p29 photo. high |
| `fo.pomekha` | ПОМЕХА / ОТКЛ. (toggle) | Jamming target, no range: method Н, fuse armed right after launch (0.8 s) | have | F (9.1) | `samsim_osa` p34; `samsim_thread` (piston79, 1 Jun 2019). The only guidance method the crew picks by hand |
| *`msl.select`* | 7 positions: 3-II, 2-II, 1-II, ОТКЛ. (centre), 1-I, 2-I, 3-I; title unreadable | The missile selector: off (automatic selection) in the centre, the three containers of each launcher either side | new | S (6.3) | Sprites `osa024`–`030`; the dial's marks on the launch console (`samsim_osa` p29 photo), next to the ammunition lamps; `flak11` (rotary switch picks another missile). medium |
| *`msl.channels`* | КАНАЛЫ: КАН. II / КАН. I / СОВМ. (red dot at СОВМ.) | Selects missile channel II, channel I, or both together | new (sprites `osa031`–`033`, formerly `unk.bort`: "БОРТ" is the БОРТ СПК label under the knob) | S (6.3) | `samsim_osa` p29 photo; `samsim_sprites`. medium |
| *`msl.change_cover`*, *`msl.change`* | СМЕНА РКТ (rotary under a hinged latch; СМЕНА to the left, red dot at the normal position) | Probably changes to the next missile ("смена ракеты") | new (sprites `osa050`–`052`: latch closed; latch open; knob turned to СМЕНА) | F (6.3) | `samsim_sprites`; `samsim_osa` p29 photo. Function inferred from the label. low |
| *`msl.emergency_cover`*, *`msl.emergency`* | АВАРИЙНЫЙ ПУСК (guarded), with lamps АВАР. ПУСК ОБЕСТОЧ. and АВАР. ПУСК ГОТОВ | Throws a missile off unguided, on a ballistic path: when the launcher has lost power, or to get rid of missiles fast | new | L | Sprites `osa042`–`043` (the sprite shows the УГОЛ МЕСТА lettering of the elevation block right below it); `samsim_osa` p29 photo; ePap, 29–30 Jun 2013. medium |
| ~~*`chk.volt_select`*~~ → socket Ш7 | Ш7: +27В ОБЩ., +27В ОТКЛ. ДУС I, +27В ИСХ. ДР. I, +27В ИСХ. ДР. II, +27В ОТКЛ. ДУС II, +27В НАЧ. ПОДГ. I, +27В НАЧ. ПОДГ. II, КОРПУС | **Not a selector**: an 8-pin test socket and its pin legend (test points for a meter) | **drop `chk.volt_select`**; the socket is ambience, or a function-check step with a test meter (6.2) | A / F | `samsim_osa` p29 photo: "Ш7" above a connector. The earlier reading came from sprite `osa050`–`052`, which shows the legend beside the СМЕНА РКТ latch. medium |
| *`msl.beam`* | СР.Л. / ШИР. (toggle) | Probably medium or wide missile-capture beam | new | F | `samsim_osa` p29 photo; not used in the manual. low |
| Other launch-console items | ?МЦ / ОТКЛ. toggle (first letter unreadable); a key lock ЗАКР. / ОТКР.; a red lamp or button read as ЗАХ. АПУ II; a СХОД push-button by ПОМЕХА; lamps ЛЮКИ ЗАКР., ОТКАЗ … and БР | Unknown; ЛЮКИ ЗАКР. = hatches closed | check | – | `samsim_osa` p29–30 photos, low resolution |
| Function-check controls | label unknown | The 3.0-minute function check | new | F (6.2) | Block OO04-14M at this seat (`rwd_mb3`); time `flak11`. Possibly also РОД РАБОТЫ's ФК position (above). Not in the manual's photos |

Lamps and displays at this seat are in §5.

### 4.6 Fighting compartment: switch-on and support panels

No seat of their own; reached from seat 1 (to its left) or standing. Which crew member works each is not known:
default to the operator for power and the deputy commander for the gyros and antenna, and say so in `notes`.

| Id | Real panel | What it does | Status | Tier | Evidence, conf. |
|---|---|---|---|---|---|
| *`pwr.master`* | OO96-9M (AK: -9M1, AKM: -9M2) | Equipment power on; **red button switches the whole vehicle off** | new | S (6.2) | ePap, 19 May 2013; `rwd_mb3` (cabinet OO96-9M2, "voltage meter OO96-9M"). medium |
| *`pwr.equipment_*`* | same cabinet | The switch-on row ("lots of switches") | new | F: model as one guided sequence first, individual switches when labels are known | Creator, 20 Jan 2014. Labels unknown |
| *`stab.gyro`* | OO05-8M gyro switch-on panel | Stabilisation system 9В611: off / standby / combat work | new | S (6.2) | `rwd_mb3` 9W611: modes "combat work", "standby", "antenna lowering for passing under bridges". medium |
| `apu.stow` → *`ant.lower`* | Antenna-lowering switch panel | Lowers / raises the search antenna | **move** from commander, rename | S | `rwd_mb3` layout legend. medium |
| `apu.lock` | unknown | АПУ travel lock | have; location unknown | S | `wrk`. Keep at seat 3 until found |
| *`chk.block_2`* | OO04-12M | Second function-check block | new | F | `rwd_mb3`. low |
| (subsystem) | 9S456 in SP-2M2 | Computer: 100 s to switch on, 3 s to change mode | new subsystem `computer` | S (6.2) | `flak11`; `rwd_mb3` 9S456 |
| – | Indicator board, null-current indicator, multimeter, TA-57 | Ambient | new | A | `rwd_mb3` layout legend |

### 4.7 Outside and other vehicles (later phases)

| Item | Feature | Phase |
|---|---|---|
| PAB-2M aiming circle on its tripod (8× optic, 60-00 circle) | Orientation at a halt: dismount, set up, read the bearing | 6.5 / 8.3 |
| External-power connector on the hull | Grid power at prepared positions | 7.6 |
| TZM 9T217BM2: crew 3, 12 missiles, hydraulic crane with its own control panel | Reload mini-game: crane panel | 7.3 |
| PU-12: plotting board, radio | Battery commander's seat | 7.1–7.2 (`samsim_osa` p20; not researched further here) |

## 5. Lamps and displays

### Displays

| Display | Real block | Status | Notes |
|---|---|---|---|
| Search scope | OP81-16M3 | have | Rings every 5 km (2/7/12 on 0–15); IFF mark beside the blip; normal video vs MTI; jamming spirals later |
| Range indicator | OS82-9M | have | Add the missile's return after launch |
| TV monitor | WPU-55 | have | Screen 171 × 127 mm (4:3). Lens fields: `rwd_mb3` narrow 1°27′ × 1°05′ (f = 500 mm), wide transcribed as 4°05′ × 3°38′ (f = 150 mm; probably 4°50′, which fits 4:3); ePap 1.4° × 1° and 4.7° × 3.5°; game manual 1.5° / 5°. The three agree: use 1.45° × 1.08° and 4.8° × 3.6° once the page is rechecked. 3-mil reticle (ePap) |
| **Elevation search indicator** | OS81-10M (photo plate ОС8?-10М, consistent) | **new display kind** | The deputy commander acquires the target in elevation on it (step 6 of the creator's first sequence, late March 2013). **Manual p24:** a round CRT beside the elevation block. SAM Simulator draws one horizontal trace across it, with the target as a short bright mark in the middle once the tracking radar captures ("the target signal is displayed in the elevation indicator"). The photo doesn't show what the axes are (elevation against range, or an error signal): check in the running game. None of the sprites is a background bitmap for it |
| βс, εс dials | On the faces of OS52-12M and the elevation block (mechanism OO94-9) | have | Each backlit by its ПОДСВЕТКА toggle; sprites `osa094`–`097` are the two dials unlit and lit |
| Range dial | (OS81-18M?) | have as counter | SAM Simulator draws it large; see 4.3 for what the operator uses it for |
| ДЦ voltmeter | on OO04-11M2 | have | 10 V = 10 km (`samsim_osa` p29). It does not read the +27 V checks: those are pins of test socket Ш7 (§4.5) |

### Lamps at the deputy commander's seat

| Lamp | Meaning | Status |
|---|---|---|
| ВНИМАНИЕ, ЦЕЛЬ В ЗОНЕ, ПУСК ГОТОВ, НЛЦ | As now | have |
| МЕТОД Ф | Guidance method Ф selected (left of ЦЕЛЬ В ЗОНЕ on the launch console) | new (6.7). `samsim_osa` p18, p29 photos |
| ПА, ТСЦ, НЛЦ; ЦУ, АЗ, РУ | Lamps above the elevation block's buttons | ПА, ТСЦ, НЛЦ have; ЦУ, АЗ, РУ new |
| Per channel I and II: ГОТОВ СПК, ВКЛ ВЫС, СХОД, ЗАХВАТ, СР.Л, УЗКИЙ ЛУЧ, К3 | Sequence in `OSA_RESEARCH_2026-10.md` §6. Two clusters: channel II on the left, channel I on the right, mirrored (`samsim_osa` p18) | new (6.3); today's ЗАХВАТ / ВКЛ ВЫС are wrong |
| 1-I, 2-I, 3-I, 1-II, 2-II, 3-II | Missile in that container; flashes if its gyro overheats. In each channel's cluster | new (6.3) |
| АВАР. ПУСК ОБЕСТОЧ., АВАР. ПУСК ГОТОВ | Emergency launch: power off / ready | new (later, with `msl.emergency`) |
| ЛЮКИ ЗАКР., ОТКАЗ …, БР | Hatches closed; a failure lamp; live-fire mode | new (A); `samsim_osa` p29 photo |
| ~~β, ε~~ | **Not lamps**: sprites `osa094`–`097` are the βс / εс dials unlit and lit (ПОДСВЕТКА) | closed |

### Tracking and guidance modes (what the lamps and buttons add up to)

| Mode | Angles | Range | Controls | Source |
|---|---|---|---|---|
| Radar auto-track | Radar (target above about 0-10 elevation) | Radar | ЦУ АЗ, then capture: ТСЦ + АВТОМАТ | Creator, 4 May 2014; `samsim_osa` p24 |
| Low target | TV, by hand (ПА) | Radar (ПОЛУАВТОМАТ) | НЛЦ lights first | same; p25–26 |
| Radars silent | TV, by hand | Judged from the reticle | ПА, БОРТ СПК, ПОМЕХА | same; p33–35 |
| Manual | Wheels (РУ) | Wheel | – | ePap |

Guidance methods are chosen by the computer (T/T above ε 00-30; Горка or Ф below); only Н is set by hand, with
ПОМЕХА (creator, 15–17 Oct 2013).

## 6. Feature map

What the player can do, which controls and displays carry it, and where it sits in the roadmap.

| # | Feature | Controls and displays | Seat | Roadmap | Tier | Evidence |
|---|---|---|---|---|---|---|
| 1 | Power: turbine, reserve generator, master on/off | `pwr.gta`, `pwr.pto`, `pwr.master` | Driver, operator | 6.2 | S | medium |
| 2 | Cold switch-on (3.8 min): gyros, computer, equipment row | `stab.gyro`, `computer`, `pwr.equipment_*` | Fighting compartment | 6.2 | S (as one sequence), F (switch by switch) | panels real, labels unknown |
| 3 | Function check (3.0 min) | `chk.*`, РОД РАБОТЫ (ФК?), test socket Ш7 | Deputy commander | 6.2 | F | panels real, labels unknown |
| 4 | March ↔ combat: antenna, АПУ lock, stabilisation mode | `ant.lower`, `apu.lock`, `stab.gyro` | Fighting compartment | 6.2 | S | medium |
| 5 | Orientation at a halt | `nav.orient`, PAB-2M | Commander | 6.5 | F | low |
| 6 | Search: rotation, transmitter, scale, beams, MTI | `soc.*`, `soc_ppi`, beam lamps | Senior operator | have / 6.4 | S | high |
| 7 | IFF | `iff.interrogate`, mark on the scope | Senior operator | 6.4 | F | medium |
| 8 | Designation from the PU-12 by radio | `rad.battery_net`, order menu | Commander | 7.2 | S | high |
| 9 | Acquisition: azimuth line, range mark, ЦУ АЗ, **elevation search**, capture | `ssc.az_*`, `ssc.range_wheel`, `fo.cu_az`, elevation indicator | Seats 1–3 | have + new display | S | high; display to confirm |
| 10 | Tracking modes, manual TV tracking, memory | `fo.pa`, `fo.ru`, `ro.semi_auto`, wheels | Seats 1, 3 | 6.6 | S | medium |
| 11 | TV picture: lens, filter, brightness, night | `tov.*` | Deputy commander | 6.6 | F | medium |
| 12 | Missiles: load, selection, two channels, lamps | `msl.select`, `msl.channels`, `msl.change`, channel and ammunition lamps | Deputy commander | 6.3 | S | selectors medium, lamps high |
| 13 | Launch: mode, key, ПУСК, second missile ≥ 4 s | `fo.mode`, `fo.key`, `fo.launch` | Deputy commander | have / 6.3 | S | high |
| 14 | Jamming target (method Н) | `fo.pomekha` | Deputy commander | 9.1 | F | medium |
| 15 | Emergency launch | `msl.emergency` | Deputy commander | later | L | medium |
| 16 | Emission control: transmitters off fast, search for a few turns only | `soc.hv`, `ssc.tx` | Seats 1, 2 | 9.2 | F | ePap: "You should search for few sec and stop changing position and wait." (3 May 2014) |
| 17 | Trainer mode (ТР) with an instructor | `fo.mode` = ТР | – | 10.3 | L | medium |
| 18 | Reload from the TZM | Crane panel | TZM crew | 7.3 | L | medium |
| 19 | Driving, swimming, night driving | Driver's controls | Driver | 4.3 / later | S (driving), L (rest) | low |
| 20 | Cabin ambience: light panels, red dots at start positions, panel lighting, inert blocks | – | all | 6.1 | A | `korovin_tiv_2010`; ePap confirms the red dot |

**Things to show on the outside of the vehicle** (model and animation, all ePap, 13–14 Jul 2013): the containers sit
fixed at 28° elevation; once the tracking radar locks, the two missile-sighting antennas move to 28° and wait; after
lift-off one follows the tracking antenna and the other stays at 28° for the second missile; container covers open
only when ПУСК is pressed; strong fans blow the launch smoke away from the TV camera.

## 7. What this changes in the data and the roadmap

**Console data (`data/procedures/osa_ak.json`):**

1. 6.1 seat split, with the real order: `operator` = today's `ssc.tx*`, `ssc.range_wheel`, `ro.semi_auto`, `ssc.mti`;
   `senior_operator` = `soc.*`, `ssc.az_*`; `deputy_commander` = `ssc.el_*`, `fo.*`, `tov.*`.
2. Move `pwr.gta` to the driver. Move `apu.stow` off the commander (antenna-lowering panel).
3. `ssc.el_wheel` limits −12° to +78°. `soc.range` starts at 0–35 km until its red dot is known. `fo.mode` second
   position = ТР, and ТР blocks lock and launch.
4. New for 6.2: `pwr.master`, `stab.gyro`, subsystem `computer` (100 s), a function-check procedure.
5. New for 6.3: `msl.select` (7 positions, confirmed on the launch-console photo), `msl.channels`, `msl.change`,
   channel and ammunition lamps, МЕТОД Ф. No `chk.volt_select` (it was a misreading of socket Ш7).
6. New for 6.4: beam push-buttons and beam lamps, `iff.interrogate` (ЗАПРОС).
7. From the eyes-on pass (§12): `fo.cu_az`'s label is ЦУ, with АЗ as a separate button; the range indicator's ЦЕЛЬ
   and РАБОТА / КОНТРОЛЬ / КАЛИБР. selectors; the range gate's fast movement is the wheel's coarse mode.

**Cabin data (`data/cabins/osa_ak.json`):** three seats in a row facing one cabinet; panels named after the real
blocks (e.g. panel `os82_9m` "Range indicator OS82-9M") so photos map onto them one to one; a new display kind for
the elevation search indicator (`cabin.hpp`, `scopes.hpp`, with tests); inert blocks as plain panels; red dots as a
per-control flag. **Correction (§12):** the evidence points to the red dot marking a control's *working* (combat)
position, not where it starts (medium). РОД РАБОТЫ's dot is at БР, yet the manual has the player select БР; КАНАЛЫ's dot is at СОВМ. Name the
flag `work_mark` (or similar), and keep start positions separate.

**Roadmap:** add a task **6.0, "eyes-on pass"** before 6.1 (§9: one session on the PC with the photos, the manual
and the running game, to turn the open items below into data); add the elevation search indicator to 6.4; add the
driver's power panel to 6.1 and the real switch-on panels to 6.2.

## 8. How the player works the controls

Today (`unreal/README.md`): left click flips, steps or presses; right click steps back; drag or scroll turns a wheel;
Tab changes seat; C enters and leaves the cabin; 1–9 give crew orders. Proposed additions, all SAM3D design:

| Need | Proposal |
|---|---|
| Five seats | Direct seat keys as well as Tab; keep SAM Simulator's habit of showing two neighbouring seats at once as a wide "lean" view between seats 1–2 and 2–3 |
| Wheels while watching a scope | Hold-to-turn keys for the wheel of your own seat (slow), with a modifier for the fast-slew buttons; the mouse stays free for switches |
| Manual TV tracking (6.6) | At seat 3 the player holds elevation; an NPC senior operator holds azimuth, or the player takes both in a "one-man" option, as SAM Simulator does |
| Covers and locks | Keep right click to open (SAM Simulator's convention, `samsim_osa` p15, p22) |
| Start positions | A "red dots" check: a key that highlights every control not at its red dot (its working position; §12) |
| Long start-up | Time compression (SAM Simulator's F1–F4), already planned in 6.2 |

The creator's own limit, worth keeping in mind: "we are doing a 3–5 crews work, one handed" (`samsim_thread`,
16 Jun 2013). SAM3D's NPC crew is the answer; the one-man option is for practice.

## 9. Eyes-on shot list (task 6.0)

For a session linked to the PC, or for Nicholas. Each line is a question a picture answers.

| Look at | Questions |
|---|---|
| `rwd_mb3` → 9A33 → *Bilder innen* (rwd-mb3.de/pages/9a33_3.htm) | Position of every block in SL-2M2 (confirm the seat columns in §3); which way the seats face; where OO96-9M and OO05-8M sit; PU-303's controls; the commander's seat and radio; the antenna-lowering panel |
| `rwd_mb3` → 9A33 → *Schemen* (9a33_4.htm) | Plan of the fighting compartment and cabinets: seat and cabinet positions in metres for `data/cabins/` |
| SAM Simulator, running (Form56 / Form57) | **Still open** (the eyes-on pass had no way to play the game). Narrowed by the manual to: what the elevation screen's axes are; what ЦЕЛЬ, АЗ, СМЕНА РКТ and КАНАЛЫ do in the game; the start position of every switch; what a wrong order does. Answered from the manual and sprites: the 7-position selector, "БОРТ", ГОРИЗ. СМЕЩ., the β / ε "lamps", the range gate's fast movement (§12) |
| `reference/manuals/SA-8B_English.pdf` p14–18 | **Done** (eyes-on pass), with pp. 13 and 21–35: §12 lists every numbered item and the labelled but unnumbered controls on each panel photo |
| `valka_osa`: two interior photos and the labelled 9A33BM2 drawing | General view of the cabin; antenna names |
| Creator's interior photos, first page of `samsim_thread` (22 Mar 2013) | The panels he worked from. The image links point at the old simhq.com host and may be dead |
| WWP, *SA-8 Gecko in Detail* (to buy) | 450+ photos with a chapter on the launcher vehicle's interior: front cab and operators' compartment "with all the complex electronic panels". The best source for textures and the 3D cabin |
| missilery.info, 9A33BM3 layout drawing (18-item legend) | Hatches, steps, wave shield, antennas for the exterior model |

**Vehicle figures for the 3D model** (`rwd_mb3`, from NVA catalogue K 050/3/016): 9A33BM3 length 9,720 mm, height
3,998 mm, combat mass 18,800 kg; width transcribed as 2,130 mm, which looks wrong (the TZM on the sister chassis is
2,782 mm wide): recheck. TZM 9T217BM2: 9,225 × 2,782 × 3,100 mm, 16,564 kg. SAM3D's placeholder hull is 9.1 × 2.8 m.

## 10. Documents to get

| Document | Where | Why |
|---|---|---|
| **A 327/1/154**, *Fla-Raketenkomplex 9K33M2 (Osa-AK), Start- und Leitstation 9A33BM2, Aufklärungsstation und Stabilisierungssystem 9W611BM2*, NVA regulation, 1984 | Bundesarchiv, BArch DVW 1-DRUCK/4835; listed as digitised | Search radar and stabilisation: the senior operator's panel, gyro panel, antenna lowering |
| **A 327/1/156**, *… Start- und Leitstation 9A33BM3, Funkkommandosender und Funkmeßgerät "Rakete"*, 1985 | BArch DVW 1-DRUCK/4953; listed as digitised | Missile channels: the lamps and the launch console |
| **A 327/1/153** | Cited by `rwd_mb3` for the 9A33; not found in the catalogue search (the series A 327/1/15x probably holds the other volumes) | Likely the general description |
| ***Ausbildungsanleitung Fla-Raketenkomplex 9K33 M2 (Osa-AK)***, 1983 | BArch DVW 2-DRUCK/130; listed as digitised | Training instruction: crew duties and drills, the best single source for procedures |
| WWP, *SA-8 Gecko in Detail*, ISBN 978-80-87509-73-9 | Bookshops | Interior photographs |
| BSU textbook, GDR handbooks | As in `OSA_RESEARCH_2026-10.md` §1 | Unchanged |

Found but **not to be used until its status is known**: a 1991 Russian book or description of the Osa that ePap
linked as a .djvu file (`samsim_thread`, 17 Jul 2013). If it is a published book it is a tier-1 source; if it is a
service document that was never released, it stays out (CLAUDE.md, public and declassified only).

## 11. Still open

Status after the eyes-on pass against `reference/` (2 Oct 2026; detail in §12). `reference/` holds the SAM Simulator
manuals and archives, and the Osa sprites. It has no `rwd_mb3` photos or drawings, nothing on the driver's cab,
commander's seat or switch-on panels, and no way to run the game in this session.

| # | Question | Status | What `reference/` showed |
|---|---|---|---|
| 1 | Positions of everything (§9) | **Partly closed** | Seats 1 and 2: the block stacking and every control on each block face (§3, §12). Seat 3: the launch console sits directly above the elevation block, with the TV monitor and elevation indicator beside it; how they are arranged around the seat is not shown. Seat facing, OO96-9M, OO05-8M, PU-303, the commander's seat and the antenna-lowering panel: not in `reference/` (`rwd_mb3` photos) |
| 2 | Labels on PU-303, OO96-9M, OO05-8M, the function-check blocks and the IFF controls | **IFF partly closed; the rest open** | IFF: ЗАПРОС and КОНТР. buttons with a lamp on OP81-16M (low resolution). РОД РАБОТЫ may have a ФК (function check) position. The other panels are not in `reference/` |
| 3 | What БОРТ, ГОРИЗ. СМЕЩ. and the β / ε lamps do; whether the 7-position selector is the missile selector | **Closed** | "БОРТ" sprites = the КАНАЛЫ selector (КАН. II / КАН. I / СОВМ.). ГОРИЗ. СМЕЩ. = a range-indicator trim knob; its sprites show the ЦЕЛЬ selector beside it. β / ε = the dials' backlight. The 7-position selector = the missile selector, 3-II … ОТКЛ. … 3-I. **New open items:** what ЦЕЛЬ, АЗ, СМЕНА РКТ, СР.Л. / ШИР. and the two ПЕЛЕНГ buttons do |
| 4 | Where the АПУ travel lock is and who works it | **Open** | Not in `reference/`. One lead: a red lamp or button on the launch console's left edge reads "ЗАХ. АПУ II" at low resolution |
| 5 | Who works the switch-on panels in the drill | **Open** | Not in `reference/` (the game leaves the panels out) |
| 6 | Start position of РОД РАБОТЫ | **Answered for the game; changes the red-dot model** | The red dot is printed at БР (sprite `osa022`, p29 photo), and the manual has the player select БР (pp. 29, 34). So the game starts away from БР (probably ТР), and red dots mark working positions, not start positions (§7) |
| 7 | What the commander has besides the radio | **Open** | Not in `reference/`: SAM Simulator has no commander's seat (its views are the three operators' panels and the PU-12 board, `samsim_osa` p13) |
| 8 | The driver's controls on the BAZ-5937 | **Open** | Not in `reference/` |

Questions 2, 4, 5 and 7, the new item 3 questions and the axes of the elevation indicator are good ones for the
creator and, through him, ePap: add them to `docs/outreach/creator_message_draft.md`. Questions 1, 2 and 4 also need
the `rwd_mb3` interior photos (§9).

## 12. Eyes-on pass (task 6.0, 2 October 2026)

A session on the PC checked §11 against `reference/`: `samsim_osa` (`SA-8B_English.pdf`, all 35 pages, rendered and
zoomed, with the Russian edition's text for comparison) and `samsim_sprites` (all 204 files, with the contact sheets
`states_osa.jpg` and `labels_osa.jpg`). The manual's panel photos are small (about 100 px a panel row), so lettering
below about 2 mm on the real panel is a reading, marked "low resolution" where it matters. **Not done:** the running
game (it can't be driven in this session), the `rwd_mb3` photos and drawings (not in `reference/`), and the seat check
with `tools/panel_sheet.py` (that belongs to the data part of 6.0, after the corrections below are in the data).
The `SA-8B_Russian.txt` in `reference/` lost its Cyrillic when it was extracted; use the PDF.

### What the manual's photos show, panel by panel

The game's views (p13): **Z / Y** shows the Range Officer's and Angle Officer's columns, **X** the Angle Officer's and
Firing Officer's, **C** the PU-12 board.

| Panel (plate) | Seat | Controls and lamps, as labelled | Pages |
|---|---|---|---|
| Range indicator (ОС81-8М or -9М) | 1 | Lamps ГОТОВ, ВКЛ. ВЫСОК.; ПЕРЕДАТЧИК ВКЛ. (guarded) / ОТКЛ.; РПЧМ / АПЧМ; ССЦ РПЧМ; РАБОТА / КОНТРОЛЬ / КАЛИБР.; ЯРКОСТЬ СТРОБА Р; per sweep ВЕРТ. СМЕЩ., ГОРИЗ. СМЕЩ., ЯРКОСТЬ, ФОКУС, АСТИГМАТ.; ЦЕЛЬ (3 positions); a lock ОТКР. / ЗАКР.; jacks КОНТРОЛЬ, КОРПУС. Two sweeps: 0–28 km above, the 1.5 km around the gate below | 16, 23, 24 |
| ОК62-5М | 1 | Jacks only (СТРОБ I, СТРОБ II, voltages) | 16 |
| ОС61-5М | 1 | КОМПЕНСАЦИЯ ВЕТРА ГРУБО / ТОЧНО, КОМПЕНС. ВЕТРА / ОТКЛ., **СДЦ / ШТ.** (item 10), ДОП. УСИЛ. / ОТКЛ., УВ. ПОМ. / ОТКЛ., jacks | 16 |
| ОС81-18М (range tracking) | 1 | Lamps АВТОМАТ, ПОЛУАВТОМАТ; buttons АВТОМАТ, ПОЛУАВТОМАТ, ТСЦ, РУЧНОЕ; СКОРОСТЬ П А (− / +); range dial (coarse 1000 m, fine 5 m a division); range wheel (item 9: one mouse button fine, the other coarse); БАЛАНС АВТОМАТА; ПЕЛЕНГ (two buttons); lock; jacks | 16, 24, 26 |
| ОП81-16М (search scope) | 2 | ПОИСК ВКЛ. / ВЫКЛ. (items 1/2); ВЫСОКОЕ ВКЛ. (guarded) / ВЫКЛ. (3/4) with a ВЫСОК. window; ДАЛЬНОСТЬ 0÷15 / 0÷35 / 10÷45 (5); I, II, III ЛУЧ (6–8) and I–III, I–II ЛУЧ (9, 10) with the lit beam indicator; ОТКЛ. ПРИЕМ / КЛАПАН; ЗАПРОС, КОНТР. and a lamp (IFF); ОСВЕЩ. ШКАЛЫ, КОНТРАСТНОСТЬ, ЯРКОСТЬ МЕТОК, ФОКУС, ЯРКОСТЬ, ЯРКОСТЬ ВИЗИРА, РПЧМ / АПЧМ, РЕГ. РПЧМ; lock | 15, 21 |
| ОП63-6М2 | 2 | РРУ, **СДЦ / ШТ.** (item 12), КОМПЕНС. ВЕТРА, ГРУБО, ТОЧНО, СА, СДЦ ВНЕШН. / ВНУТР.; lock | 15 |
| ОС52-12М (azimuth) | 2 | ПОДСВЕТКА / ОТКЛ. (4); βс (5); ВЛЕВО / ВПРАВО (6); АЗИМУТ wheel (7) with its lock | 16, 22 |
| Elevation block (ОС52-1…, plate cut off) | 3 | Lamps ПА, ТСЦ, НЛЦ over buttons ПА, ТСЦ; ПОДСВЕТКА / ОТКЛ.; lamps ЦУ, АЗ, РУ over three buttons ЦУ, АЗ, РУ; БАЛАНС ПА; εс; УГОЛ МЕСТА wheel with a lock ЗАКР. / ОТКР. | 17, 23, 25 |
| Elevation indicator (ОС8?-10М) | 3 | Round CRT: one horizontal trace, the target a bright mark in the middle | 24 |
| TV monitor (9Ш38-2) | 3 | РАБ. РЕЖ. ВКЛ. / ВЫКЛ., ЯРКОСТЬ, ПЕРЕКРЕСТ., СВЕТОФИЛЬТР, ФОКУС КИНЕСКОПА / ВИДИКОНА, a lock; УГОЛ ЗРЕНИЯ ШИРОКИЙ / УЗКИЙ, КОНТРОЛЬ / РАБОТА, КОНТРАСТ, ЯРК. ЭЛЕКТР. ПЕРЕКРЕСТ., ЯРКОСТЬ, a lock. Lenses 5° (f = 150 mm) and 1.5° (f = 500 mm) | 17, 35 |
| Launch console (OO04-11M2) | 3 | Lamps per channel (II left, I right) and МЕТОД Ф, ЦЕЛЬ В ЗОНЕ, ПУСК ГОТОВ, ВНИМАНИЕ, БР; ДЦ voltmeter; РОД РАБОТЫ (БР •, ТР, РР?, ФК?); missile selector 3-II … ОТКЛ. … 3-I; КАНАЛЫ (КАН. II, КАН. I, СОВМ. •); СМЕНА РКТ under a latch; test socket Ш7; БОРТ СПК ВКЛ. (guarded) / ОТКЛ.; ПОМЕХА / ОТКЛ.; СХОД button; СР.Л. / ШИР.; ПУСК; ЗАПРЕТ ПУСКА key; АВАРИЙНЫЙ ПУСК (guarded) with two lamps; lamps ЛЮКИ ЗАКР., ОТКАЗ …; ?МЦ / ОТКЛ.; ЗАХ. АПУ II (?); a key lock | 18, 29, 30, 34 |

### Corrections made to this file

| Was | Now | Evidence |
|---|---|---|
| Sprites `osa024`–`030`: "probably" the missile selector (low) | The missile selector, 3-II … ОТКЛ. … 3-I (medium) | p29 photo: the dial's marks |
| Sprites `osa031`–`033`: `unk.bort`, "БОРТ, 3 positions" | `msl.channels`: КАНАЛЫ, КАН. II / КАН. I / СОВМ.; "БОРТ" is the БОРТ СПК label below | p29 photo; the sprite's top edge shows "КАН.II" |
| Sprites `osa014`–`016`: `unk.horiz`, "ГОРИЗ. СМЕЩ., 3 positions", seat 3 | The range indicator's ЦЕЛЬ selector, seat 1; ГОРИЗ. СМЕЩ. is the CRT trim beside it | p16, p24 photos |
| Sprites `osa050`–`052`: `chk.volt_select`, a voltmeter selector | СМЕНА РКТ under a latch (closed / open / turned); the +27 V list is socket Ш7's pin legend | p29 photo; sprites |
| Lamps β, ε (sprites `osa094`–`097`) | The βс / εс dials' backlight, unlit and lit | Sprites; p16–17 ПОДСВЕТКА |
| `fo.cu_az` labelled ЦУ АЗ | Two buttons, ЦУ and АЗ; the game's "ЦУ АЗ" step is the ЦУ button | p17, p23 photos |
| `ssc.range_fast`: a separate control to find | The wheel's coarse mode in the game | p16 item 9 |
| `soc.beam` five buttons "medium" | Confirmed: three beam buttons and two sequence buttons with a lit indicator (high) | p15 |
| IFF label unknown | ЗАПРОС / КОНТР. on OP81-16M (low resolution) | p15 |
| Gain and scope knobs "label unknown" | Labels listed per block (above) | p15, p16, p24 |
| SAM Simulator's seat order "Range – Azimuth – Elevation" | "Range Officer – Angle Officer – Firing Officer" | p13 |
| Red dots mark start positions | Probably working positions (РОД РАБОТЫ's dot at БР, which the player must select) | p29, p34; ePap |
| Range indicator OS82-9M; search scope OP81-16M3, OP63-6M3, OK62-5M1, OS61-5M1 | Plates on the photos: ОС81-8М/-9М, ОП81-16М, ОП63-6М2, ОК62-5М, ОС61-5М; both recorded | p15, p16 |
| Elevation indicator: "check what it draws" | One horizontal trace with the target mark | p24 |
| Lamps: no МЕТОД Ф, no emergency-launch or hatch lamps | Added | p18, p29 |
