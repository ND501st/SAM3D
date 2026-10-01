# 9K33 Osa — SAM Simulator findings

Source: SAM Simulator v930.2 data files `osa001.dat`–`osa245.dat` (204 files), viewed locally from
`reference/samsim/` (git-ignored). Only facts and label transcriptions are recorded here. **No art, sounds or
data from the game are committed.** `sam.exe` has not been examined.

## What the files contain

| Kind | Count | Notes |
|---|---|---|
| JPEG | 184 | Control sprites (one image per position), scope frames, target photos and cards, maps |
| BMP | 15 | Scope backgrounds and overlays, blank screens, map layers |
| WAV | 5 | Two long loops (~10 MB, ~3.4 MB) and three short effects |

There is **no full console background**. Each control is a small sprite with one image per position, so the
console layout (where each sprite sits) and all the behaviour are in `sam.exe`'s forms. The install has no manual
or help file.

## Control types seen

These drive what `simcore` must support. Guarded and stepwise controls were added to `ControlPanel` because of them.

| Type | Example sprites | States | simcore model |
|---|---|---|---|
| Guarded push-button (hinged cover) | `osa010`–`011` ПЕРЕДАТ. ВКЛ. | cover closed / open | cover is its own control; button has `guard_id` |
| Guarded toggle (hinged cover) | `osa020`–`021` ВКЛ.; `osa034`–`035` СПК ВКЛ | cover closed / open | same |
| Guarded emergency button | `osa042`–`043` АВАРИЙНЫЙ ПУСК | cover closed / open | same |
| Key switch | `osa195`–`197` | no key / key in / key turned | 3 positions, `stepwise` |
| Rotary selector, 3 positions | `osa014`–`016` (next to ГОРИЗ. СМЕЩ.), `osa031`–`033` БОРТ | 3 | `positions = 3` |
| Rotary selector, 7 positions | `osa024`–`030` (centre ОТКЛ.) | 7 | `positions = 7` |
| Rotary with a 5-way voltage check | `osa050`–`052` +27В … | 3 sprites | `positions = 3` (TBC) |
| Lever / toggle, 3 positions | `osa017`–`019` range: 0–15 KM / 0–35 KM / 10–45 KM | 3 | `positions = 3` |
| Toggle, 2 positions | `osa036`–`041`, `osa044`–`049` (ПОДСВЕТ…, ОТКЛ., ШТ.) | 2 | default |
| Indicator lamp | `osa094`–`097` (β, ε) | off / lit | output from `Equipment`, not a control |

## Label transcriptions

Read from low-resolution sprites; partial words are marked with "…". Translations are literal.

| Label | Meaning | Where |
|---|---|---|
| ПЕРЕДАТ(ЧИК) ВКЛ. | SSC tracking-radar transmitter on (manual p16) | `osa010` |
| (БОРТ) СПК ВКЛ | missile / SPK command-guidance preparation on (manual p18, p34) | `osa034` |
| АВАРИЙНЫЙ ПУСК | emergency launch | `osa042` |
| УГОЛ МЕСТА | elevation angle | `osa042` |
| ГОРИЗ. СМЕЩ. | horizontal offset | `osa014` |
| ДАЛЬНОСТЬ 0÷15 KM / 0÷35 KM / 10÷45 KM | range scale | `osa017` |
| БОРТ | "board / on-board" | `osa031` |
| +27В ОБЩ., +27В ОТКЛ. ДУС I / II, +27В ИСХ. ДР. I / II | +27 V common; +27 V off, DUS I/II; +27 V source, DR I/II | `osa050` |
| ПОДСВЕТ… | backlight / illumination | `osa036`, `osa038` |
| ОТКЛ. | off | several |
| β, ε | azimuth and elevation (lamps) | `osa094`–`097` |

TV sight monitor, `osa220` (388×287, screen area blank for the game to draw into):

| Label | Meaning |
|---|---|
| РАБ. РЕЖ. ВКЛ. / ВЫКЛ. | operating mode on / off |
| ЯРКОСТЬ | brightness (two knobs) |
| ПЕРЕКРЕСТ. | crosshair |
| ЗАКР. (arrow) | closed (knob direction) |
| СВЕТОФИЛЬТР | light filter |
| КИНЕСКОПА | (of the) picture tube |
| ФОКУС | focus |
| ВИДИКОНА | (of the) vidicon |
| УГОЛ ЗРЕНИЯ ШИРОКИЙ / УЗКИЙ | field of view wide / narrow |
| КОНТРОЛЬ / РАБОТА | test / operate |
| КОНТРАСТ | contrast |
| … ЭЛЕКТР. ПЕРЕКРЕСТ. | electronic crosshair |

## Displays

- `osa175`–`176`: round PPI-style scope face, 268×268, with a bearing scale.
- `osa053`–`089` (37 frames), `osa100`–`136` (37), `osa137`–`172` (36): rotation animation sets (antenna or indicator
  drums); if each set is one full turn, that is about 10° per frame.
- `osa098`, `osa223`: blank 256×192 screens the game draws into.
- `osa199`–`201`: target drone cards with speeds (350, 450, 850 km/h), used for training targets.

## Official manual (SAM Simulator documentation v210109)

Source: `SA-8B_English.pdf`, "9K33M2 OSA-AK (SA-8B Gecko) Simulator Documentation", Version 1 (2014-04-25), 35 pages,
from the creator's documentation set (`SAMSIM_docs_210109.rar`, linked from his site). Kept in `reference/manuals/`
(git-ignored). Below is a paraphrased summary with page numbers; see the PDF for the full text and pictures.

### The biggest finding: no power-up, deploy or stow

SAM Simulator's Osa **starts already deployed and powered**. The manual has no power-up, march-to-combat or stow
procedure, and a session ends with ESC (p13). So SAM Simulator can't be the source for SAM3D's drive / deploy /
stow sequence. That has to come from public operator material, or be SAM3D's own design, clearly labelled.

### Stations (p13)

| Station | Key | What it has |
|---|---|---|
| Range Officer | Z or Y | SOC search radar controls, SSC transmitter, azimuth wheel and indicator, range indicator and wheel |
| Angle Officer | none listed | named on the keyboard diagram only; the manual gives no separate panel or key |
| Firing Officer | X | elevation wheel and indicator, TV camera (TOV), SPK/SVR lamps, launch key, ПУСК |
| PU-12 plotting board | C | target reports from the command post (BTR-60 9S482) |

The screen shows one continuous console split between them (Form56 and Form57 in the running game).

### Control behaviours that simcore must model

| Behaviour | Examples (page) | simcore today |
|---|---|---|
| Guard removed by right-click before use | ВЫСОКОЕ ВКЛ (SOC transmitter, p15), ПЕРЕДАТЧИК ВКЛ (SSC transmitter, p16), БОРТ СПК ВКЛ (p34) | `guard_id` |
| Key: insert, then rotate | ЗАПРЕТ ПУСКА launch-authorization key (p29–30) | 3-position `stepwise` |
| Wheel locked until right-clicked | azimuth and elevation wheels (p22–23) | lock as a `guard_id` control |
| Multi-position selectors | ДАЛЬНОСТЬ 0–15 / 0–35 / 10–45 km (p15); РОД РАБОТЫ, БР = live fire, leftmost (p29); beam select I / II / III / I–III / I–II (p15) | `positions` |
| Continuous wheels | АЗИМУТ, УГОЛ МЕСТА, range wheel, drag with the mouse; fast left/right buttons (p16–17) | `ControlKind::Continuous` |
| Momentary push-buttons | ЦУ АЗ (auto acquisition, p23), ПА (TV angle track, p25), ПОЛУАВТОМАТ (range-only track, p26), ПУСК (launch, p30) | `ControlKind::Momentary`; modes they engage are latching subsystems |
| Lamps driven by system state | ГОТОВ СПК, ВКЛ ВЫС, ЗАХВАТ, СХОД (p18); ВНИМАНИЕ, ПУСК ГОТОВ, ЦЕЛЬ В ЗОНЕ (p29); ТСЦ (p24), АВТОМАТ (p24), НЛЦ (p25) | Lamps are data (`data/cabins/osa_ak.json`). The per-channel guidance lamps are wrong today; see "Corrections from operators" below |
| Automatic sequences | missile gyro spin-up (15 s) starts by itself when ВНИМАНИЕ lights, 15 s before the target reaches the zone (p27, p29); target captured automatically once the SSC is on azimuth and in the range gate (p24) | sim inputs + `enable_any` (the sim doesn't set the inputs yet) |

The console is data: [`data/procedures/osa_ak.json`](../../data/procedures/osa_ak.json) has the controls, subsystems and
procedures A–E below, each with page references and confidence ratings.

### Procedures (order only; see the PDF for detail)

**A. Autonomous acquisition with the SOC radar (p21–24)**
1. Watch the plotting board until the target nears 50 km.
2. Range Officer: SOC antenna rotation on (ПОИСК ВКЛ), then remove the guard and switch the SOC transmitter on (ВЫСОКОЕ ВКЛ).
3. Select scanning of all three beams (I–III ЛУЧ) and set the range selector to 10–45 km.
4. When the echo shows, select the beam it appeared in (shown by the lit indicator).
5. Unlock the azimuth wheel and switch on its indicator lighting (ПОДСВЕТКА).
6. At about 35 km, switch to the 0–35 km scale; move the range gate to about 27.5 km.
7. Turn the SSC azimuth line onto the target mark.
8. Firing Officer: unlock the elevation wheel, switch on its lighting, press ЦУ АЗ (automatic acquisition).
9. Range Officer: at about 30 km, remove the guard and switch the SSC transmitter on (ПЕРЕДАТЧИК ВКЛ).
10. The SSC captures the target by itself: ТСЦ (angle track) and АВТОМАТ (range track) light.

**B. Ultra-low-altitude target (p25–26):** НЛЦ lights; switch the TV camera on (РАБ.РЕЖ. ВКЛ), select manual mode (РУ),
bring the target under the crosshair with the wheels, press ПА for TV angle tracking, then bring the echo into the range
gate with the range wheel and press ПОЛУАВТОМАТ for range-only tracking.

**C. Preparation for firing (p29):** РОД РАБОТЫ to БР (live fire); insert the launch key; check the missiles available
on each launcher (three per side, 1-II…3-II and 3-I…1-I); watch the target distance on the voltmeter (ДЦ, 10 V = 10 km).
ВНИМАНИЕ starts missile preparation automatically; wait for ЗАХВАТ, ВКЛ ВЫС, then ПУСК ГОТОВ and ЦЕЛЬ В ЗОНЕ.

**D. Launch (p30):** turn the launch key, press ПУСК; the missile is chosen automatically; СХОД lights when it leaves the
container; when ПУСК ГОТОВ lights again, a second missile can be launched. Two missile channels, one target channel (p14).

**E. Engagement with emissions control, TV only (p33–35):** find the target visually with the camera and wheels while the
radars stay silent; press ПА; select БР; start missile/SPK/SVR preparation manually (БОРТ СПК ВКЛ, guarded); set ПОМЕХА so the
fuse arms right after launch (range unknown); insert the key; switch to the narrow lens; judge range from the target's size
in the crosshair; turn the key and press ПУСК.

**F. Assessing the result (p31):** detonation and smoke on the TV; an "X" on the plotting board; the track ends.

## March, deploy and stow (public sources, not SAM Simulator)

SAM Simulator starts deployed, so this section comes from other public material. Sources (full references in
`docs/SOURCES.md`):

- **[wrk]** "2ПВО МСД. Часть 2. Зенитные ракетные полки (зрп)", an article on Soviet divisional SAM regiments
  posted as a Word attachment on the wrk.ru (Aviabaza) forum. Detailed, apparently written from service
  experience and Soviet documentation; unsigned in the file. Treated as **medium** confidence.
- **[gs]** GlobalSecurity.org, "SA-8 GECKO / 9K33M3 Osa-AKM".
- **[apa]** Air Power Australia, "9K33 Osa/Romb ... SA-8 Gecko", technical data table.
- **[mis]** missilery.info, "9K33M3 Osa-AKM".
- **[ru-wiki]** Russian Wikipedia, "Оса (зенитный ракетный комплекс)".

### What the sources say

| Fact | Source | Notes |
|---|---|---|
| Crew of 5: vehicle commander (an officer), guidance operator (sergeant, deputy crew chief), range operator, search operator, driver-mechanic-electromechanic | [wrk]; 5 also in [ru-wiki] for the 9A33B | [mis] and a Ukrainian museum page give 4 for later variants |
| Two power sources: the 9И120 gas-turbine power unit (ГТА: 9И56-3 turbine + generator), which powers the vehicle **on the move and at a halt**; and an engine power take-off generator (ГОМ), **only at a halt** | [wrk], [mis] | [apa] writes "9I210", probably a typo; 9И120 used here |
| Readiness for combat work, **including starting the GTA, within 4 minutes** | [wrk]; "combat deployment time is four minutes" [gs]; march-to-combat "up to 4 min" [mis] | [apa] gives 3–5 min each way |
| On the march the antenna-launcher unit (АПУ) is **locked in the travel position with the SSC antenna facing rear** | [wrk] | |
| When air targets appear on the march: the crew **pulls off the road, unlocks the АПУ**, captures the target and fires | [wrk] | |
| Search radar can search **on the move** (at up to 80 km/h, from a stabilised platform); tracking and launch only **from a short halt or stationary** | [wrk], [mis], [gs] | |
| "Excellent" standard: going to manual tracking, **including unlocking the АПУ and slewing it onto the target, 13 s**; with the АЗ method, 12 s. Tracking starts right after SSC high voltage comes on | [wrk] | |
| A trained crew keeps the АПУ in the combat position and starts the search in beam II, in MTI (СДЦ) mode, with three operators each watching a sector of the PPI | [wrk] | |
| For air transport (Il-76) the АПУ is stowed: the search radar **folds back 90°** to lower the vehicle for air transport and fast road travel | [wrk], [gs] | |

### What they don't say

No public source found gives the switch-level order for starting the GTA, unlocking or locking the АПУ, or
folding the antenna, or any warm-up times other than the 4-minute total. The deploy and stow procedures in
`data/procedures/osa_ak.json` are therefore **SAM3D's own design built on the facts above**, marked
`confidence: estimate`, with English labels where the real panel label isn't known.

## Still not known

- Exact screen positions of each control (only needed if SAM3D copies the layout; the cabin will be 3D anyway).
- What the game does when steps are done out of order (fault, no effect, or refusal). Can be checked in the running game.
- Power-up, deploy and stow: not in SAM Simulator. Public facts are above; the switch-level order is SAM3D's design.
- The real panel labels and positions of the GTA, power take-off and АПУ lock controls.

## Corrections from operators and the creator's forum (October 2026)

The manual is a player's guide; the creator's development thread on SimHQ (2013–2017) and an NVA veteran's record
(flak11.de) add or correct details. Full list with sources: `docs/research/OSA_RESEARCH_2026-10.md`.

| Topic | Manual | Forum / NVA |
|---|---|---|
| Stations | Range, Angle and Firing Officer (p13) | Range operator, azimuth operator, elevation operator (creator); NVA: senior operator (search scope, IFF, azimuth/range marks), operator (range display), deputy commander (elevation, TV, launch), commander (radio designations), driver |
| Guidance lamps, per channel | ЗАХВАТ and ВКЛ ВЫС light during preparation (p18, p29) | СХОД (left the container) → ЗАХВАТ (captured by the missile-tracking radar, wide beam) → СР.Л (medium beam) → УЗКИЙ ЛУЧ (narrow beam) → К3 (fuse armed, ~120 m before the target). ГОТОВ СПК = transmitter ready; ВКЛ ВЫС = transmitter high voltage on, automatically at ВНИМАНИЕ (NVA). 1/2/3-I and -II = missile in that container; flashes if its gyro overheats |
| Missile selection | "automatically selected missile" (p30) | Two missiles selected at ВНИМАНИЕ, outer ones first; a rotary switch on the deputy commander's console can pick another; ≥ 4 s between launches |
| Preparation | Starts with ВНИМАНИЕ; 15 s spin-up (p27, p29) | NVA: starts at "target in zone"; ПУСК ГОТОВ after 13 s |
| TV tracking | ПА engages TV angle tracking; footnote: in reality tracked by hand (p25) | "Only manual tracking" (ePap); 3-mil reticle; lenses 1 × 1.4° and 3.5 × 4.7° |
| Search scope | Scales 0–15 / 0–35 / 10–45 km (p15) | Rings every 5 km; on 0–15 km at 2, 7, 12 km; 0–35 km is the working scale; 33 rpm; beams I 0–4°, II 4–8°, III 8–28° (NVA) |
| Guidance methods | "automatically chosen" (p30) | T/T above ε 00-30; Горка or Ф (helicopters) below; Н against jammers (no range) |
| PU-12 | Plotting board read by the player (p20) | Designations reach each vehicle by voice: azimuth and range |
| Panels | White panels (the creator: "first white panel" in SAM Simulator) | Light front panels for reading in a dim compartment, and red dots beside knobs and switches marking their starting positions (Korovin, «Техника и вооружение» 2010) |
| Low targets | НЛЦ, then TV angle tracking (p25) | "Горка" method for targets at 50–100 m; the original Osa detects a 50 m target at about 27 km and locks it at about 14 km (Korovin) |

