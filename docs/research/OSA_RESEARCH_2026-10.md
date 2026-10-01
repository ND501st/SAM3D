# 9K33 Osa: research review (October 2026)

Research done on 1 October 2026 to check SAM3D against SAM Simulator and the real Osa, with a strong preference for
documents and accounts from countries that operated the system. Public and declassified material only. The
decisions that follow from it are in `docs/BRIEFING_2026-10.md`; the plan changes are in `PROJECT_PLAN.md` and
`DEVELOPMENT_ROADMAP.md`. Source ids are the ones used in `docs/SOURCES.md` and `data/systems/osa.json`.

## 1. Sources consulted, by tier

| Tier | Source (id) | What it is | Used for |
|---|---|---|---|
| 1 | **Wagner, flak11.de** (`flak11`) | R. Wagner, veteran of NVA Fla-Raketen-Regiment 11 (Weißenfels, Osa-AK from 1987), 2005. Pages 9K33-0, 9K33-3, 9A33-1, 9A33-3, PU12-0. Cites the GDR's official troop handbooks and the declassified *Auskunftsbuch der Truppenluftabwehr 1985* (ex GVS B 597030) | Crew, stations, firing sequence, battery and regiment, timings, PU-12 |
| 1 | **Rykov, NVO 21 Aug 2015** (`nvo_2015`) | I. Rykov, deputy chief designer, IEMZ "Kupol" (the Osa's manufacturer), *Смертоносная «Оса» в новом исполнении*, Независимое военное обозрение | March-to-combat ≤ 4 min, reaction 26–40 s, six missiles, one or two per target, AKM1 crew cut by one |
| 1 | **Korovin, «Техника и вооружение» 2010** (`korovin_tiv_2010`) | В. Коровин, «Зенитный ракетный комплекс «Оса» — история создания», parts 1–3, «Техника и вооружение» (part 3: September 2010), republished on otvaga2004.ru. Developer-archive history; part 3 includes the memoir of A. P. Pavlov (BAZ test engineer with an Angolan Osa-AK unit from 1983) | Development, crew staffing decision, radar coverage, capture ranges, single-shot kill chances, modernisations, Angola and Iraq claims (added 2 Oct, see §12) |
| 1 | **wrk.ru article** (`wrk`) | *2ПВО МСД. Часть 2. Зенитные ракетные полки (зрп)*, unsigned Russian article on divisional SAM regiments | Crew of 5, power sources, march/combat positions (already used) |
| 2 | **ePap, SimHQ** (in `samsim_thread`) | Greek Army Osa-AK/AKM officer whose unit took over FRR-11's equipment in 1993; "at least 30" live missiles, NAMFI (Crete) firings; also Tor-M1 | Lamps, TV channel, scopes, fuel, salvo interval, tactics, trainer |
| 2 | **Russian conscript account** (`ilya_prosto`) | Deputy crew chief, Osa-AKM, 6th Tank Brigade, 2012–13 (LiveJournal) | Crew of 4 in Russian service, drills against time norms, Kapustin Yar live firing |
| 2 | **Gyűrösi, Wojsko i Technika 11/2019** (`zbiam_2019`) | Polish defence magazine on the 9A33-2B modernisation | Operators' cabin behind the driver's cab; why the vehicles needed upgrading |
| 3 | **SAM Simulator Osa-AK manual** (`samsim_osa`) | The creator's 35-page manual, v1 2014 (`reference/manuals/SA-8B_English.pdf`) | Console behaviour, procedures A–F, PU-12 board, missile, TZM, ARMs |
| 3 | **SAM Simulator site** (`samsim_site`) | Creator's site, archived 5 Nov 2016 (FAQ, version history, tutorials) | Design philosophy, features |
| 3 | **SimHQ Osa thread** (`samsim_thread`) | *9K33M2 OSA-AK (SA-8B Gecko)*, SimHQ forum, 2013–2017 (Wayback copy of 8 Apr 2022), ~400,000 characters; the creator ("Hpasp") designing the Osa with operators | Real station split, lamp meanings, guidance methods, regiment structure, scenario plans |
| 4 | Russian reference pages (`vpvo`, `ru_wiki`, `mis`) | Encyclopaedic pages from user countries | Cross-checks only (9И120 power unit) |
| 4 | Contemporary press: *Washington Post*, 25 Jul 1982, "Missile Sites Hit in Bekaa" (`wapo_1982`) | Historic report | First combat use (Lebanon, 24 Jul 1982) |
| 4 | **YouTube documentary on the 9K33 Osa** (`video_osa`; title and channel not recorded, transcript supplied by the user on 2 Oct 2026) | Secondary. Its history and technical parts retell `korovin_tiv_2010` closely; its battery-tactics part cites unnamed "manuals" and veterans' forum posts | Battery tactics, ARM/ECCM drills, deploy times (unverified); see §12 |
| 5 | GlobalSecurity, Air Power Australia, Jane's (`gs`, `apa`, `janes`) | Western "facts and stats" pages | Not used for new decisions; existing citations to be replaced (see §9) |

**Found but not read (worth acquiring):**
- Гринкевич Ю. Б., Дударенок Е. П., *Боевое применение ЗРК 9К33М3*, БГУ, Минск 2020, ISBN 978-985-566-960-0
  (`bsu_2020`). Belarusian military-faculty textbook on firing rules, combat work and fire control of an Osa-AKM
  battery. Closed access in the university repository; a published book.
- GDR troop handbooks (Militärverlag der DDR), cited by `flak11`: *Handbuch für Fla-Raketenschützen und Funkorter der
  Fla-Einheiten der Truppenluftabwehr* (1984), *Handbuch für Fla-Einheiten* (1990), *Handbuch für Planzeichner und
  Funkorter der Truppenluftabwehr* (1980), K.-H. Otto, *MTH Fla-Raketen* (1987).
- *Куито-Куанавале. Неизвестная война. Мемуары ветеранов войны в Анголе* (Soviet veterans' memoirs; mentions a 52nd
  brigade Osa-AK specialist and an Osa kill of a Mirage F1). The online copy (docplayer.ru) was unreachable.
- The SimHQ thread's later pages (2013–2017) were searched by keyword, not read in full.

**Could not be read:** the creator's live site (Google sign-in) and SimHQ itself (bot check; the archived copy was
used instead). YouTube videos can't be watched here; the creator's site lists tutorial videos by "UV59", an ex SAM
battery commander, for other systems.

## 2. Crew and stations

| Source | Crew on the vehicle | Stations |
|---|---|---|
| `flak11` (NVA) | **5**: commander (officer), deputy commander (career NCO), senior radar operator (*Oberfunkorter*, NCO), radar operator (*Funkorter*, conscript), driver | Senior operator: search scope (RSG), IFF, puts the target under the azimuth and range marks with handwheels. Operator: range display, range tracking; captures with ТСЦ. Deputy commander: launch console (key, launch-ready lamp, missile-selection rotary switch, voltmeter), elevation wheel, TV monitor, elevation display. Commander: receives target designation from the PU-12 by radio. The platoon leader commands one vehicle |
| `wrk` (Soviet) | 5: commander, guidance operator (sergeant, deputy crew chief), range operator, search operator, driver-mechanic | (already in `osa.json`) |
| `samsim_thread` (creator, 2013) | 3 operators plus the battery commander in the PU-12 | "Range Operator – Azimuth Operator – Elevation Operator" (left to right); the game shows them on two screens |
| `samsim_osa` | Range Officer, Angle Officer, Firing Officer | Keys Z/Y (range + angle) and X (angle + firing); C = PU-12 board |
| `korovin_tiv_2010` (Soviet, 1971 decision) | – | The state commission judged one vehicle's workload equal to a long-range battery's and considered two officers (a gunner, major, and a guidance officer, captain); the Ministry rejected it: the crew chief was ranked as a platoon commander, and the **guidance seat went to a conscript sergeant, later a warrant officer (прапорщик)** |
| `ilya_prosto`, `nvo_2015` | **4** in Russian service from the Osa-AKM1 upgrade ("crew reduced by one") | – |
| Polish sources, `zbiam_2019` | 5; driver and commander in the front cab, operators' cabin behind it | – |

**Conclusion:** the 1980s Osa-AK crew is five: commander, deputy commander (elevation, TV, launch), senior operator
(search radar, IFF, azimuth), operator (range) and driver. SAM Simulator's three operators are the same three
working seats under other names. **SAM3D has four stations and folds the senior operator and the operator into one
"range officer". That is the largest accuracy gap in the console data.**

## 3. Power-up, march and deploy, timings

| Item | Value | Source |
|---|---|---|
| Power on the move and at a halt | 9И120 gas-turbine unit (9И56-3 engine + С-75 generator), ~50 kW | `vpvo`, `samsim_thread` (ePap) |
| Power at a halt only | Engine power take-off generator, ~35 kW | same |
| Fuel | ~300 l tank; turbine ~75 l/h stationary, ~100 l/h moving; take-off generator ~25 l/h; half the tank is kept for driving, and the crew asks for fuel at half | ePap |
| Switch-on cycle (cold) | 3.8 min, plus 3.0 min function check = **6.8 min** preliminary preparation; 14 min with the PU-12M fully set up | `flak11` |
| Fire-control computer 9С456М3 (analogue) | 100 s to switch on, 3 s to change mode | `flak11` |
| March to combat position | **≤ 4 min** (manufacturer), 3 min (Russian norms quoted elsewhere) | `nvo_2015`, `wrk` |
| Reaction time | **26–40 s** (target detection to launch) | `nvo_2015` |
| Target designation to missile ready | 41–60 s; firing cycle 60 s | `flak11` |
| Missile preparation | 13 s after "target in zone" (NVA); 15 s gyro spin-up (game manual); "12 s" (creator, 2013) | `flak11`, `samsim_osa` p27, `samsim_thread` |
| Two missiles in the air | at least 4 s apart (ePap), 3–5 s (Russian) | ePap, `nvo_2015` context |
| Reloading 6 missiles from the TZM | 6 min; crane traverse holds up to 3 containers | `flak11` |
| Vehicle setup for correct designations | Vehicle must be oriented with its navigation/orientation set (TNA-3) | `flak11` |
| Readiness requirement at the 1970 trials | 2 min, not met at first; heaters were added to speed warm-up | `korovin_tiv_2010`, `video_osa` |
| Speeds (9A33B) | Dirt roads average 36 km/h by day, 25 km/h at night; road up to 80 km/h; water 7–10 km/h | `korovin_tiv_2010` |
| Battery deploy / march order (from a powered march) | 35–50 s / 15–30 s, by crew skill | `video_osa` only, **unverified**; Russian reference pages give 3–5 min for deploying and stowing |
| New target after a kill | 40 s (excellent crew) to 120 s (poor) | `video_osa` only, **unverified** |

SAM3D today: the turbine starts in 60 s (estimate) and the practice scenario reaches "power ready" in about a minute.
**Real cold start-up is minutes, not seconds**, and there is a function-check step SAM3D doesn't have.

## 4. Search radar (СОЦ) and the search scope

| Item | Value | Source |
|---|---|---|
| Rotation | 33 rpm (1.82 s per turn) | `flak11`, `samsim_thread` |
| Coverage | 360°, elevation 0–30°, 45 km | `flak11` |
| Beams | Three: I = 0–4°, II = 4–8°, III = 8–28°; selected by push-button, or scanned I–III / I–II. An indicator shows which beam the echo came from | `flak11`, `samsim_osa` p15, p21 |
| Display scales | 0–15, 0–35, 10–45 km (game, ePap); "0–30 km" in `flak11` | conflict, see §10 |
| Range rings | Every 5 km; on the 0–15 km scale at 2, 7 and 12 km | ePap |
| Use | 0–35 km is the main working scale; 0–15 km is a zoom for pop-up helicopters once tracking; never start at 0–15 | ePap |
| IFF | Integrated (НРЗ, "Kennungsgerät"); the senior operator interrogates | `flak11`, `vpvo` |
| Video | Normal video (noisy, clutter) or MTI (СДЦ, to 35 km) | ePap, `samsim_osa` p15 |
| Blind cone overhead | Yes (ePap: "negative cone of the acquisition radar") | ePap |
| Beam geometry (original 9A33B) | Beam switched to one of three positions each turn; 1° in azimuth; 4° high in the two lower positions, 19° in the upper; 27° total | `korovin_tiv_2010` |
| Detection range vs target height (original 9A33B, fighter-size) | 40 km at 5 km altitude; **27 km at 50 m** | `korovin_tiv_2010` |
| Sequence on the move | Detect, check IFF (НРЗ), then a short halt, tracking transmitter on, auto-track in angle and range | `korovin_tiv_2010` |
| External coherence mode | Added with the Osa-A work (by 1975): better auto-tracking in passive jamming (chaff) | `korovin_tiv_2010` |

SAM3D today: 2 s sweep, rings every 5 or 10 km from the centre, beam switch does nothing in the engagement model, no
IFF.

## 5. Tracking radar (ССЦ), TV and engagement

| Item | Value | Source |
|---|---|---|
| Tracking radar | Range 0–28 km, elevation −12° to +78°, azimuth ±330° (electronic limit), slewing 15°/s azimuth, 30°/s elevation | `flak11`, `samsim_thread` |
| Capture | Automatic once the radar is on the azimuth and the target is in the range gate (ТСЦ, АВТОМАТ) | `samsim_osa` p24 |
| Low target | НЛЦ lamp at 0-06…0-10 mils elevation; angle tracking then by TV, range by the radar (ПОЛУАВТОМАТ) | `flak11`, `samsim_osa` p25 |
| **TV sight (ТОВ)** | **Manual tracking only** ("Only manual tracking", ePap); the camera is for identification and assessing results. 3-mil reticle; narrow lens 1 × 1.4°, wide 3.5 × 4.7° (ePap) vs 1.5° / 5° (game manual). Range judged from target size in the reticle | ePap, `samsim_osa` p17, p25 footnote |
| Memory tracking | Semi-automatic mode continues at the last angular rates (masked targets) | ePap |
| Guidance method, chosen automatically | T/T for targets above ε 00-30; low targets: "Горка" (zoom climb) or "Ф" (helicopters, hovering); "Н" against jammers (no range) | `samsim_thread` (creator, ePap) |
| Engagement zones | Head-on: 1.5–10 km, 25–5000 m at ≤ 300 m/s, 100–5000 m at ≤ 500 m/s, parameter 6/4 km. Pursuit: to 6.5 km, 100–5000 m, ≤ 300 m/s. Optical "Ф": ≤ 100 m/s or hovering, 2–6.5 km, 10–25 m | `flak11` |
| Single-missile kill probability | Manoeuvring targets 0.55–0.85; strike aircraft at 10 m 0.12–0.55; helicopters at 10 m 0.07–0.12; cruise missiles at 10 m 0.12–0.38 | `flak11` |
| Single-missile kill probability (original 9K33, F-4C) | 0.35–0.4 at 50 m; 0.42–0.85 above 100 m (modelling and live firings) | `korovin_tiv_2010` |
| Tracking-radar capture range (original 9A33B) | 23 km at 5 km altitude; **14 km at 50 m** | `korovin_tiv_2010` |
| "Горка" method | Used against targets at 50–100 m: the missile comes in from above, so the fuse isn't set off by the ground | `korovin_tiv_2010` |
| Helicopter method (Osa-AKM) | Semi-automatic angle tracking with the TV sight, radar range; the missile flies above the line of sight and dives onto the target; hovering or near-ground helicopters at 2–6.5 km, parameter up to 6 km | `korovin_tiv_2010`, `video_osa` |
| Cruise-missile switch (Osa-AKM) | "ПКР" (поражение крылатых ракет) toggle on the main console; Osa-AKM only, not the Osa-AK | `korovin_tiv_2010` |

SAM3D today: TV tracking locks and follows automatically (like SAM Simulator's simplification), one zone box, no
method selection, hit chances are game values (drone 0.8 etc.).

## 6. Missile channels, lamps and the launch

Firing officer's lamps for **each** of the two missile channels (`samsim_thread`, creator and ePap; `flak11`):

| Lamp | Meaning |
|---|---|
| ГОТОВ СПК | Command transmitter ready |
| ВКЛ ВЫС | Command transmitter high voltage on (switched on automatically at ВНИМАНИЕ, `flak11`) |
| СХОД | Missile has left the container |
| ЗАХВАТ | Missile captured by the missile-tracking radar (wide beam, ~0.8 s / ~60 m after launch) |
| СР.Л | Medium beam: missile being brought onto the guidance line |
| УЗКИЙ ЛУЧ | Narrow beam: guidance |
| К3 | Fuse-arming command sent (≈120 m before the target; with a jamming target, together with УЗКИЙ ЛУЧ) |
| 1/2/3-I, 1/2/3-II | A missile is in that container (left II, right I); flashes if its gyro overheats. AKM: an extra lamp per missile for the 9M33M3 type |

The 2014 game manual describes ЗАХВАТ and ВКЛ ВЫС as lighting during preparation ("SVR ready to receive", "SPK
transmitting to the equivalent antenna"); the creator's 2013 forum answer and the NVA account place ЗАХВАТ after
launch. SAM3D's current mapping (ЗАХВАТ = preparation started, ВКЛ ВЫС = preparation done) matches neither.

Launch sequence (`flak11`): seconds before the target enters the launch zone, ВНИМАНИЕ lights, the command
transmitter switches on, and **two missiles are selected automatically (outer ones first, left and right)**. At
"target in zone" they start preparing; ПУСК ГОТОВ after 13 s. The deputy commander may pick a different missile
with a rotary switch. Key, then ПУСК. ПУСК ГОТОВ lights again for the second missile (`samsim_osa` p30). Command
guidance by the three-point method through wide, medium and narrow beams; K3 at 120 m; the fuse works at ~15 m.

Missile load: six in containers on the vehicle; each TZM 9Т217 carries 12; a battery has 48 (`flak11`,
`samsim_osa` p28, `nvo_2015`). The original 9K33 had four open missiles and a TZM 9Т217Б carrying eight
(`korovin_tiv_2010`). The two missiles are launched at least 3–5 s apart, each captured by its own set of wide- and
medium-beam antennas (`korovin_tiv_2010`).

Panels (`korovin_tiv_2010`, Osa-A work): light-coloured front panels so labels read in a dim fighting compartment,
and **red dots beside knobs and switches marking their starting positions**, for quick preparation before firing. A
useful cabin detail for SAM3D.

## 7. Battery, regiment, PU-12

| Level | Composition | Source |
|---|---|---|
| Battery (NVA) | Battery commander's PU-12M (with a reconnaissance team), **two platoons of two 9A33 and one TZM**, maintenance vehicle 9В210, spares set 9Ф372, and a close-defence platoon (Strela-2M MANPADS, ZU-23-2) | `flak11` |
| Regiment (NVA) | 5 firing batteries, HQ battery (PU-12M, P-40, 2 × P-19, P-18), technical battery, transport company; 480 missiles | `flak11` |
| Regiment (Soviet, theoretical) | HQ, 5 firing batteries, technical battery; 2 × PU-12M at HQ | `samsim_thread` (creator) |
| Target designation | Automated (telecode) only down to the battery PU-12M; **from there by voice radio, azimuth and range, to each vehicle**. PU-12M navigation errors up to 5° and 3 km | `flak11` |
| PU-12 plotting board (game) | Target notation (number, height in hectometres, type/IFF digit, formation size), updated once a minute, range rings 50–200 km, mils | `samsim_osa` p20 |
| Trainer 9Ф632 | Ural-375 with all vehicle workplaces plus an instructor seat who enters the air picture; can be coupled to a real vehicle; instructor changes up to two targets' behaviour live; ECCM and HARM drills | `flak11`, ePap |
| Regiment air picture | Regiment command post with a PU-12M fed by a P-19; a P-19 could be attached to each battery (often for exercises); batteries exchange data, so a jammed battery can be fed by the others or the regiment | `video_osa` (consistent with `flak11`) |
| Battery reconnaissance | Called the battery's most important capability: radar plus each crew watching through the TV sight, and the battery command post's own air observation post with optics (targets at 8–25 km) | `video_osa` |
| Close defence | MANPADS team with each vehicle position (four in the video), the battery command post guarded by MANPADS; NVA batteries had their own Strela-2M and ZU-23-2 platoon | `video_osa`, `flak11` |

**Battery tactics** (`video_osa`, from unnamed manuals; **unverified**, but consistent with Soviet practice and
useful for missions and NPC battery behaviour):

| Situation | What the battery does |
|---|---|
| March | Single column, PU-12 and maintenance vehicles at the front or rear; 25–50 m between vehicles, 200–800 m when an air attack is expected; ready to pull off the road for a short-halt engagement at any time |
| Advance or pursuit (combat formation) | Vehicles in pairs about 3 km apart, 200 m within a pair, 1.5–3 km behind the covered unit; reload vehicles about 500 m from the pairs; usually one battery per motorised rifle regiment |
| River crossing | Positions about 2 km from the river; cross in pairs once the far bank is held |
| Ambush | From several prepared points, or pairs roving between them; bounding in pairs as the covered force advances |
| Halts | Every 3–4 h: equipment check with the diagnostic vehicles, fuel, food, rest up to 2 h; at night shorter halts of about 1 h without breaking formation |
| Prepared (peacetime) positions | Concrete emplacements, two rows of wire, camouflage nets, dummy positions; powered from the civil grid in peacetime to save fuel, then diesel tanks for about 24 h when cut off; smoke generators and decoys |

## 8. Threats and tactics

- Jamming: SAM Simulator shows range-gate pull-off (spirals) on the search scope; noise jammers are engaged by method
  Н; ПОМЕХА switch arms the fuse right after launch (`samsim_osa` p34, `samsim_thread`).
- Anti-radiation missiles: SAM Simulator models Shrike, Standard ARM and HARM (`samsim_osa` p32). ePap: the trainer
  has a HARM scenario; the drill is "stop any emission and change position" rather than engaging it.
- Emissions control: TV-only engagement with the plotting board for azimuth (`samsim_osa` p33–35).
- Fuel and position changes are part of the crew's job (ePap).
- Jamming (`video_osa`, citing manuals): "mixed" tracking, switching between semi-automatic and automatic modes; the
  counter-jamming facility only when no other way to track is left, and only while firing, not while just tracking.
  The video's guess that this is a range-memory circuit is speculation; ePap's memory mode (last angular rates) and the
  method Н (no range) are the documented parts.
- Anti-radiation missiles (`video_osa`): track without emitting or with as little as possible: the TV sight (wide
  lens for an approaching aircraft), the search radar on for only two turns, a brief tracking-radar burst to get data
  then off, the radar off once the missile has gone far enough or as soon as it detonates. Consistent with ePap's
  "stop any emission and change position". Decoys for ARMs on the Osa-AKM are rumoured (veterans' forums) and not
  confirmed.
- Jamming resistance: the video repeats secondary claims that Israeli EW in Syria and Soviet EW trials at Emba failed
  to stop the Osa. Treat as claims.

## 9. Combat and live-fire history (for scenarios and validation)

| Event | What is reported | Source |
|---|---|---|
| Lebanon, 24 Jul 1982 | Syrian SA-8 shoots down an Israeli F-4E over the Bekaa; first combat use there; the sites are then attacked | `wapo_1982`; Jane's (quoted by the creator) |
| Angola, 1987–88 (Cuito Cuanavale) | FAPLA Osa units; claims of two Seeker RPVs and an AM-3C Bosbok (Jane's, quoted); Mirage F1AZ of Maj. Ed Every, 20 Feb 1988 (game manual p31); Soviet adviser memoirs mention a Mirage F1 | `samsim_osa`, `samsim_thread`, `kuito_memoirs` (to acquire) |
| Angola, from 1983 (Pavlov memoir) | An Osa-AK unit guarding the border and the north–south road; vehicles in 2 m-deep emplacements under nets; night moves with the search radar on; refuelling from improvised containers; SAAF Impala, Mirage and Buccaneer bombing at night; the unit claims 10 aircraft and helicopters, including a Puma flying low in a canyon out of radar view | `korovin_tiv_2010` (memoir of A. P. Pavlov) |
| Iraq, 1991 | Claim: Iraqi Osas, with Shilkas, among the most effective defences against Tomahawk cruise missiles; claim: US forces captured an Iraqi Osa-AKM in Kuwait with crew and documents | `korovin_tiv_2010`; claims, unconfirmed |
| Live-fire ranges | Ashuluk and Kapustin Yar (Soviet/Russian), Ustka (Poland; the game has an Osa double kill against a P-15 target there), NAMFI Crete (Greece), Emba (state trials 1970, 1974, 1979) | `samsim_thread`, `ilya_prosto`, `korovin_tiv_2010` |
| Libya 1986 | **Not an Osa scenario**: the creator's El Dorado Canyon document lists only S-75, S-125 and Kub defenders | `reference/manuals/Operation El-Dorado Canyon_English.pdf` |

SAM3D's current validation list (U-2 1960, Vietnam, F-117 1999) is for other systems.

## 10. Open conflicts between sources

| Topic | Values | What SAM3D should do |
|---|---|---|
| Middle search scale | 0–35 km (game, ePap) vs 0–30 km (`flak11`) | Keep 0–35 km (game parity and an operator), note the conflict |
| Missile preparation time | 12 s / 13 s / 15 s | Keep 15 s (game), note 13 s (NVA) |
| ЗАХВАТ / ВКЛ ВЫС meaning | Preparation (game manual) vs after launch / at ВНИМАНИЕ (forum, NVA) | Follow the NVA account and the creator's forum answer, which agree; note the manual |
| TV lens fields | 1.5° / 5° (game) vs 1 × 1.4° / 3.5 × 4.7° (operator) | Use the operator's values for the 3D camera, keep the game's in notes |
| Crew size | 5 (Osa-AK) vs 4 (Osa-AKM1, Russian service) | 5 for the 1980s Osa-AK |
| When preparation starts | At ВНИМАНИЕ (game) vs at "target in zone" after missile selection at ВНИМАНИЕ (NVA) | Follow NVA: selection and transmitter at ВНИМАНИЕ, preparation at the zone |
| Deploy and stow times | 35–50 s / 15–30 s for a battery (`video_osa`, unverified) vs ≤ 4 min march-to-combat (`nvo_2015`) vs 3–5 min (Russian reference pages) vs 6.8 min cold start (`flak11`) | Different starting states: a powered battery stopping from the march is fast; one that must start the turbine takes minutes; a cold start with the function check longest. Model readiness states (roadmap 6.2) and keep the video's figures as a crew-skill target until a source is found |
| Guidance seat rank | Sergeant (`wrk`), NCO (`flak11`), conscript sergeant then warrant officer (`korovin_tiv_2010`) | "Deputy commander" for the seat; rank varies by army and period |
| Minimum target altitude (Osa-AK) | 25 m (game, `flak11`) vs 27 m (`video_osa`) | 25 m |
| Regiment missile count | 120 on an Osa-AK regiment's vehicles, "80" for an Osa-AKM regiment (`video_osa`) vs 20 vehicles × 6 = 120 and 480 in all (`flak11`) | The video's 80 looks wrong; use `flak11` |
| Tracking-radar power | 200 kW (`korovin_tiv_2010`, original) vs 180 kW (game) vs 18 kW (`flak11` page) | Not used by SAM3D |

## 11. What SAM Simulator does, as the creator describes it

- "Realistic to the switch"; "rather a system simulator, not a game"; "simulate what the operators of the SAM
  battalion could see and hear"; "No points are given during game play, as in reality"; a situation ends when you
  switch the system off (`samsim_site`).
- Osa added in v0.928 (30 Apr 2013); "OSA and Shilka in Egypt" (v928.3); night situations; NASA terrain for clutter;
  jamming burn-through; 3D after-action report to Google Earth; time compression F1–F4 (`samsim_site`).
- Its Osa starts deployed; no march, deploy or stow (`samsim_osa`). The creator's stated limit: "we are doing a 3~5
  crews work, one handed" (`samsim_thread`). SAM3D's NPC crew, driving and 3D world address exactly those limits.

## 12. The video transcript (added 2 October 2026)

A transcript of a long YouTube documentary on the Osa (from 3:55 to about 1:00:30; title and channel not recorded),
supplied by the user. Checked against the sources above.

**Reliability.** The development history and the technical figures follow Korovin's «Техника и вооружение» series
(`korovin_tiv_2010`) almost point for point; where they can be checked against it they match. The battery-tactics
chapter cites unnamed manuals, and the ARM-decoy and Syrian-EW points cite veterans' forums and "secondary sources";
the narrator says so himself. So: history and figures, tier 1 through Korovin; tactics, tier 4 until a manual is
found (the BSU textbook `bsu_2020` covers exactly this subject and is the best candidate).

**Confirmed by tier-1 sources:** 33 rpm search radar switching one of three beams per turn; IFF check, then a short
halt and auto-track; two missile channels on one target, never one missile on two targets; launches 3–5 s apart;
"Горка" against low targets; firing only from a short halt, search on the move (stabilisation dropped in 1970); crew
staffing decision (officer commander, sergeant then warrant officer in the guidance seat); Osa-K's six containers;
Osa-AKM's helicopter method and "ПКР" switch; regiment of five batteries of four vehicles with PU-12s and P-19.

**New and useful for SAM3D:**
- Detection and capture ranges against low targets (27 km and 14 km at 50 m): a basis for low-target behaviour in the
  search picture and lock (roadmap 6.4, 6.7).
- Single-shot kill chances at 50 m (0.35–0.4): a calibration point for low targets (6.7).
- Battery tactics (§7): march spacing, short-halt drill, combat formation in pairs, river crossings, ambushes, halts
  every 3–4 h, prepared positions on grid power: material for NPC battery behaviour and missions (phases 7, 8, 10).
- ARM and ECCM drills (§8): intermittent radiation, TV-first tracking, counter-jamming only while firing (phase 9).
- Crew-skill timings: battery deploy 35–50 s, march order 15–30 s, next target 40–120 s (unverified; 6.2, phase 8).
- Panel detail from Korovin, found while checking: red dots marking each control's starting position (6.1).

**Not adopted:** the "80 missiles" per Osa-AKM regiment (conflicts with 20 × 6); 27 m minimum altitude (25 m in the
operator sources); the ARM decoy (unconfirmed by the video itself); claims about Syrian and Emba EW results.

**Auto-caption errors to ignore when re-reading the transcript:** "N20 / NE20" = NII-20 (НИИ-20); "Coutaii /
Coutisei" = Kutaisi automobile plant; "Bonsk" = Bryansk automobile plant (BAZ); "Gurusen / Gushin / Gusin" = P. D.
Grushin (OKB-2); "Eustenov / Eustanov" = D. F. Ustinov; "Rosplatin" = A. A. Raspletin; "A&12 or I76" = An-12 or
Il-76; "Gora" = Горка; "9V120" = 9В210; "P12M" = PU-12M; "Isaskque / Isvk" = Izhevsk; "LA7" = La-17 target drone;
"10 -3 watts" = 10⁻¹³ W; "Ember Range" = Emba; "Tangaska" = Tunguska.
