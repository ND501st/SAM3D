# 01. Roles and knowledge

The Osa-AK of the 1980s has a crew of five (`flak11`, `wrk`, `voenizdat_osa_1991`; `docs/research/OSA_RESEARCH_2026-10.md`
§2). Each sits at one console station; every control belongs to one station (`data/procedures/osa_ak.json`), so every
action has exactly one owner. This file says what each seat is for, what it can see, what it is told, what it does
without being told, and what it must tell the others.

## 1. The seats

| Seat (id) | Russian title (`wrk`, `flak11`) | Place | Duties in one line |
|---|---|---|---|
| Commander (`commander`) | Командир боевой машины (an officer; in a battery, a platoon commander may command one vehicle) | Front cab, right | Commands the crew; takes designations and orders from the PU-12 on the R-123M; decides the target, fire and cease fire; reports to the battery |
| Deputy commander (`deputy_commander`) | Заместитель командира, оператор наведения (career NCO, later a warrant officer) | Seat 3, launch console | Elevation, TV sight, tracking modes, the launch console: missile preparation, key, launch; orients the vehicle at a halt; the senior of the three operators |
| Senior operator (`senior_operator`) | Старший оператор (оператор поиска; NVA *Oberfunkorter*) | Seat 2 | The search radar (СОЦ), the IFF, the azimuth wheel: puts the tracking radar on the target in azimuth |
| Operator (`operator`) | Оператор (оператор дальности; NVA *Funkorter*) | Seat 1 | Switches the equipment on, the antenna drives and the АПУ lock; the tracking radar's transmitter; the range wheel; ТСЦ / ПОЛУАВТОМАТ range modes |
| Driver (`driver`) | Механик-водитель (driver-mechanic-electromechanic) | Front cab, left | Drives; the gas-turbine power unit and the take-off generator; the left R-123M; the gyro arrest at start-up |

The battery adds (phase 7, `docs/PU12_DEVELOPMENT.md`): the **battery commander** in the PU-12M (the player's seat in the
battery half of "in command"), the PU-12M's plotter, signaller and driver, the other vehicles' crews (four Osas per
battery), the TZM crew and the fuel-truck driver. Their behaviour is in `03` §13 and `07` §3.

## 2. What each seat can see (its instruments)

Knowledge starts at the instruments. A crewmate sees only the displays and lamps at their own station
(`data/cabins/osa_ak.json`), hears the intercom and, if their set is on, the radio.

| Seat | Displays | Lamps at the seat | Hears |
|---|---|---|---|
| Commander | none in the cab: the TNPO-170A periscope (looking out) | R-123M right: готова, прогрев; βн ЗАСТОПОР.; УКЛАДКА ПРОИЗВ.; power; equipment fault | The command net and the battery net on the right R-123M (designations, orders), the intercom (A-1 box) |
| Deputy commander | TV monitor (`tov_monitor`), εс dial (`epsilon_c`), ДЦ voltmeter (`dts_voltmeter`), Qc setter (`qc_setter`), elevation-search indicator (`elev_search`) | ВНИМАНИЕ, ЦЕЛЬ В ЗОНЕ, ПУСК ГОТОВ, НЛЦ, РУ, ЦУ, АЗ, БР, both channels' ЗАХВАТ / ВКЛ. ВЫС. / СХОД / СР.Л / УЗКИЙ ЛУЧ / К3, ЗАХВ. АПЧ, the six missile lamps, ГОТОВ СПК, ЛЮКИ ЗАКР., the emergency-launch lamps (33 in all) | Intercom (A-2) |
| Senior operator | Search scope (`soc_ppi`) with the tracking radar's azimuth line and range mark, βс dial (`beta_c`), the SOC/SSC counter strip (`soc_counters`) | ГОТОВ, ВЫСОК., ЗАПРОС, beam I / II / III | Intercom |
| Operator | Range indicator (`ssc_range`): the whole range above, the gate below, the counter | АНТЕННА СОЦ ЗАСТОПОРЕНА, СИСТЕМА СТАБ., РАССТОПОРЕНО, βн ПРИВОД, ПРИВОДЫ εн / СВР, ГОТОВ, ВКЛ. ВЫСОК., ТСЦ, АВТОМАТ | Intercom |
| Driver | The road (vision blocks, PNV-57E at night), engine instruments, fuel | R-123M left: готова, прогрев; generator on line; ОТКАЗ Ψ, Θ; ГТА running | The left R-123M, the intercom (ПВ box) |

Consequences that the behaviour must respect:

- **Only the senior operator sees the search picture.** Everyone else knows a target's azimuth and range because he
  calls it out (or the commander passes the PU-12's designation).
- **Only the operator sees range on the tracking radar**, and only the deputy commander sees the TV and the launch
  lamps. ЦЕЛЬ В ЗОНЕ and ПУСК ГОТОВ reach the commander as the deputy's callouts.
- **The commander sees nothing of the air picture himself** except through the periscope; he commands by what he hears.
  This is why the callouts are not decoration: without them the commander, NPC or player, is blind.
- **The driver knows the ground**, the fuel and the power, and nothing of the air battle unless told.

## 3. What each seat is told (knowledge by voice)

| From | To | What | Channel |
|---|---|---|---|
| PU-12 (battery commander) | Commander | Designations: target number, azimuth (mils), range, height; orders (readiness, march, formation, fire, cease fire) | R-123M, command and battery nets |
| Early-warning sources (EWR, neighbour, observation post) | Commander | Designations as above, when the commander has switched to them (`simcore/c2_sources.hpp`) | R-123M |
| Commander | Crew | Orders, designations passed on ("цель номер…, азимут…, дальность…"), fire permission | Intercom (R-124), or shouted when the box is off (`crew_comms.hpp`) |
| Senior operator | Commander, deputy, operator | Contacts (azimuth, range, beam), IFF reply, "on the target in azimuth" | Intercom |
| Operator | Commander, deputy | Range, capture ("захват"), lock lost, transmitter ready | Intercom |
| Deputy commander | Commander | Target in zone, missile ready, launch, missile away, hit or miss (from the TV and the lamps), faults on the launch console | Intercom |
| Driver | Commander | Power on, ready to march, fuel state, obstacles, halted | Intercom |
| Anyone | Commander | Faults at their seat, "I can't" with the reason | Intercom |

## 4. The knowledge model (how the sim represents it)

Each crewmate holds a **picture**: entries with what was known, when, and from where.

| Kind | Example | Sources |
|---|---|---|
| Air targets | "Target 6201, azimuth 05-40, range 24 km, height 1000 m, 12:31:20, told by the commander" | Own instrument (scope, range indicator, TV), a callout, a radio designation |
| Equipment state | "The SOC transmitter is ready" | Own lamps, a callout ("Готов СОЦ") |
| Orders and permissions | "Fire permitted on 6201", "radars off" | The commander's orders |
| Own task | "I am turning the azimuth onto 05-40" | The order being worked |
| The vehicle | Halted or moving, position, heading, oriented or not | The driver's callouts, the commander's own view |

Rules:

1. **No telepathy.** An NPC acts on its picture, never on the simulation's truth. A crewmate who wasn't told doesn't know.
   (Today's NPC aiming reads the engagement model's search picture directly; phase 8.5 routes it through the
   senior operator's callouts.)
2. **Every entry ages.** A designation a minute old is a minute old; the picture keeps the time and the source, and an
   NPC who acts on stale knowledge does what a real one would (aims where the target was).
3. **A callout is a hand-off.** Speaking puts the entry into every listener's picture after the speaking time and the
   listener's perception delay; a listener whose box is on the radio misses it (`crew_comms.hpp`).
4. **The commander's picture is the crew's common picture.** He collates; NPC commanders decide from it, and the
   player in command sees it on the HUD as what his crew has told him.

## 5. What each seat does without being told (standing duties)

A trained crew doesn't wait for every order. These run on their own as soon as the seat's equipment allows, and are the
"initiative" part of NPC behaviour (all **sam3d** unless a source is given):

| Seat | Standing duty | Trigger | Callout |
|---|---|---|---|
| Senior operator | Watch the scope; report every new contact with azimuth, range and beam; report contacts lost | A new echo held for two sweeps; an echo gone | `contact`, `contact_lost` |
| Senior operator | Interrogate a new contact (IFF) when the IFF is on and the commander has asked for it routinely | New contact | `iff_friend`, `iff_no_reply` |
| Operator | Report the transmitter ready; report range on the target once the gate is on it; report capture and its loss | Lamps; the gate | `ssc_ready`, `range_on`, `captured`, `lock_lost` |
| Deputy commander | Report ВНИМАНИЕ, ЦЕЛЬ В ЗОНЕ, ПУСК ГОТОВ, СХОД, the result on the TV; watch faults on the launch console | Lamps; the TV | `target_near_zone`, `target_in_zone`, `missile_ready`, `missile_away`, `hit`, `miss` |
| Driver | Report power on and the generator on line; fuel at half (the crew asks for fuel at half, ePap via `samsim_thread`); halted | Lamps; fuel gauge; the vehicle stopped | `power_on`, `fuel_half`, `halted` |
| Commander | Pass every designation from the PU-12 on to the crew; report readiness to the battery ("к бою готов", `voenizdat_osa_1991` p246) | Radio; the crew's reports | `designation`, `report_ready` |
| Everyone | Report a fault at their seat; report "ready" at the end of their part of "К бою" (`voenizdat_osa_1991`: "первый (второй, третий) готов", "Готов СОЦ", "Готов СПК") | Lamps | `fault`, `seat_ready` |

## 6. Who may hold which seat

- Any seat can be held by the player or by an NPC; the other seats behave the same either way (pillar 3). In
  multiplayer, several seats are players (`docs/MULTIPLAYER.md`).
- **In the crew** (training half): the player holds one of the four working seats (or the driver's); the commander is
  an NPC who gives the orders the player must carry out, and the other NPCs depend on the player's part and remind
  him when it is late (`03` §2.4, `04` §5).
- **In command**: the player holds the commander's seat (vehicle) or the battery commander's (battery). He gives orders
  through the order menu (`07`); he may also reach any control himself, but the drill expects him to order it.
- An NPC never works another seat's controls ("refuse rather than reach across the vehicle", `CrewMember`); a seat that
  is empty or whose holder can't act stalls whatever depends on it, and the dependants report why.
