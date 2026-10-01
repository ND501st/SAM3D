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
| 1 | **wrk.ru article** (`wrk`) | *2ПВО МСД. Часть 2. Зенитные ракетные полки (зрп)*, unsigned Russian article on divisional SAM regiments | Crew of 5, power sources, march/combat positions (already used) |
| 2 | **ePap, SimHQ** (in `samsim_thread`) | Greek Army Osa-AK/AKM officer whose unit took over FRR-11's equipment in 1993; "at least 30" live missiles, NAMFI (Crete) firings; also Tor-M1 | Lamps, TV channel, scopes, fuel, salvo interval, tactics, trainer |
| 2 | **Russian conscript account** (`ilya_prosto`) | Deputy crew chief, Osa-AKM, 6th Tank Brigade, 2012–13 (LiveJournal) | Crew of 4 in Russian service, drills against time norms, Kapustin Yar live firing |
| 2 | **Gyűrösi, Wojsko i Technika 11/2019** (`zbiam_2019`) | Polish defence magazine on the 9A33-2B modernisation | Operators' cabin behind the driver's cab; why the vehicles needed upgrading |
| 3 | **SAM Simulator Osa-AK manual** (`samsim_osa`) | The creator's 35-page manual, v1 2014 (`reference/manuals/SA-8B_English.pdf`) | Console behaviour, procedures A–F, PU-12 board, missile, TZM, ARMs |
| 3 | **SAM Simulator site** (`samsim_site`) | Creator's site, archived 5 Nov 2016 (FAQ, version history, tutorials) | Design philosophy, features |
| 3 | **SimHQ Osa thread** (`samsim_thread`) | *9K33M2 OSA-AK (SA-8B Gecko)*, SimHQ forum, 2013–2017 (Wayback copy of 8 Apr 2022), ~400,000 characters; the creator ("Hpasp") designing the Osa with operators | Real station split, lamp meanings, guidance methods, regiment structure, scenario plans |
| 4 | Russian reference pages (`vpvo`, `ru_wiki`, `mis`) | Encyclopaedic pages from user countries | Cross-checks only (9И120 power unit) |
| 4 | Contemporary press: *Washington Post*, 25 Jul 1982, "Missile Sites Hit in Bekaa" (`wapo_1982`) | Historic report | First combat use (Lebanon, 24 Jul 1982) |
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
`samsim_osa` p28, `nvo_2015`).

## 7. Battery, regiment, PU-12

| Level | Composition | Source |
|---|---|---|
| Battery (NVA) | Battery commander's PU-12M (with a reconnaissance team), **two platoons of two 9A33 and one TZM**, maintenance vehicle 9В210, spares set 9Ф372, and a close-defence platoon (Strela-2M MANPADS, ZU-23-2) | `flak11` |
| Regiment (NVA) | 5 firing batteries, HQ battery (PU-12M, P-40, 2 × P-19, P-18), technical battery, transport company; 480 missiles | `flak11` |
| Regiment (Soviet, theoretical) | HQ, 5 firing batteries, technical battery; 2 × PU-12M at HQ | `samsim_thread` (creator) |
| Target designation | Automated (telecode) only down to the battery PU-12M; **from there by voice radio, azimuth and range, to each vehicle**. PU-12M navigation errors up to 5° and 3 km | `flak11` |
| PU-12 plotting board (game) | Target notation (number, height in hectometres, type/IFF digit, formation size), updated once a minute, range rings 50–200 km, mils | `samsim_osa` p20 |
| Trainer 9Ф632 | Ural-375 with all vehicle workplaces plus an instructor seat who enters the air picture; can be coupled to a real vehicle; instructor changes up to two targets' behaviour live; ECCM and HARM drills | `flak11`, ePap |

## 8. Threats and tactics

- Jamming: SAM Simulator shows range-gate pull-off (spirals) on the search scope; noise jammers are engaged by method
  Н; ПОМЕХА switch arms the fuse right after launch (`samsim_osa` p34, `samsim_thread`).
- Anti-radiation missiles: SAM Simulator models Shrike, Standard ARM and HARM (`samsim_osa` p32). ePap: the trainer
  has a HARM scenario; the drill is "stop any emission and change position" rather than engaging it.
- Emissions control: TV-only engagement with the plotting board for azimuth (`samsim_osa` p33–35).
- Fuel and position changes are part of the crew's job (ePap).

## 9. Combat and live-fire history (for scenarios and validation)

| Event | What is reported | Source |
|---|---|---|
| Lebanon, 24 Jul 1982 | Syrian SA-8 shoots down an Israeli F-4E over the Bekaa; first combat use there; the sites are then attacked | `wapo_1982`; Jane's (quoted by the creator) |
| Angola, 1987–88 (Cuito Cuanavale) | FAPLA Osa units; claims of two Seeker RPVs and an AM-3C Bosbok (Jane's, quoted); Mirage F1AZ of Maj. Ed Every, 20 Feb 1988 (game manual p31); Soviet adviser memoirs mention a Mirage F1 | `samsim_osa`, `samsim_thread`, `kuito_memoirs` (to acquire) |
| Live-fire ranges | Ashuluk and Kapustin Yar (Soviet/Russian), Ustka (Poland; the game has an Osa double kill against a P-15 target there), NAMFI Crete (Greece) | `samsim_thread`, `ilya_prosto` |
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

## 11. What SAM Simulator does, as the creator describes it

- "Realistic to the switch"; "rather a system simulator, not a game"; "simulate what the operators of the SAM
  battalion could see and hear"; "No points are given during game play, as in reality"; a situation ends when you
  switch the system off (`samsim_site`).
- Osa added in v0.928 (30 Apr 2013); "OSA and Shilka in Egypt" (v928.3); night situations; NASA terrain for clutter;
  jamming burn-through; 3D after-action report to Google Earth; time compression F1–F4 (`samsim_site`).
- Its Osa starts deployed; no march, deploy or stow (`samsim_osa`). The creator's stated limit: "we are doing a 3~5
  crews work, one handed" (`samsim_thread`). SAM3D's NPC crew, driving and 3D world address exactly those limits.
