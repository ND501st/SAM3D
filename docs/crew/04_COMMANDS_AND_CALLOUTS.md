# 04. Commands and callouts

Pillar 4 asks for callouts, instructions and commands for every possible action. The catalogue lives in data
(`data/crew/osa_ak_crew.json`, `callouts`), so the game, the tests and this file read the same list; this file gives
the rules and the list in readable form.

## 1. What a callout is

| Field | Meaning |
|---|---|
| `id` | Stable id used by code and procedures (`contact`, `order_k_boyu`) |
| `kind` | `order` (commander or a senior seat to a seat), `ack` (acknowledgement), `report` (a seat to the commander), `warning` (to everyone, urgent), `remind` (a waiting seat to the one it waits for), `radio` (on the R-123M, to or from the battery) |
| `from`, `to` | Seats (`commander`, `deputy_commander`, `senior_operator`, `operator`, `driver`), `crew` (everyone), `battery` (the PU-12 / battery net) |
| `ru`, `en` | The words, in Russian as the crew spoke them and in English. `{azimuth}`, `{range_km}`, `{number}`… are filled in when spoken |
| `priority` | 0 urgent (interrupts: "Воздух!", "Пуск!", "Срыв", "Отставить"), 1 normal, 2 routine |
| `ack` | Whether the receiver acknowledges ("Есть"). Every order is acknowledged; reports are not, except where the commander must confirm (fire permission) |
| `effect` | What it does: the procedures it starts at which seats, or the task (goal) it hands the drill master, or the knowledge it puts into the listeners' pictures |
| `source`, `confidence` | Where the phrasing is documented. The "К бою" drill's reports are in `voenizdat_osa_1991` pp. 246–248; most others are **sam3d** in Soviet style, to be checked against the NVA handbooks (`nva_handbooks`) and operator accounts |

## 2. Rules of speech

1. **Every order is acknowledged** ("Есть" = understood, doing it) or **refused with the reason** ("Не могу: АПУ
   застопорено"). An NPC never silently ignores an order.
2. **Every finished task is reported** ("Выполнено" or the task's own report: "Готов СОЦ").
3. **Designations go in a fixed order:** target number, azimuth (mils, `00-00` format on the 6000 circle), range (km),
   height (hectometres), as the PU-12 gives them (`pu12.hpp`) and as the commander passes them on.
4. **One speaker at a time per channel.** The intercom and each radio net carry one voice at a time; urgent callouts
   (priority 0) cut in. A listener whose R-124 box is on the radio misses intercom traffic (`crew_comms.hpp`).
5. **Speaking takes time:** 0.4 s plus 0.35 s per word (about 2.8 words/s; sam3d estimate). The listener acts after
   hearing it and his perception delay (`05`).
6. **Numbers are spoken as the crew reads them:** mils "пять сорок" for 05-40, ranges in km, heights in hectometres
   on the board, metres otherwise.
7. **A callout is knowledge.** Its fields (azimuth, range, target number) enter every listener's picture with the time
   and the speaker (`01` §4).

## 3. The catalogue

### 3.1 Readiness and movement orders (commander → crew)

| id | ru | en | Effect | Source |
|---|---|---|---|---|
| `order_k_boyu` | К бою! | Action stations! | Task "ready, halted" (R2): every seat's part of the drill (`03` §2.1) | `voenizdat_osa_1991` p246 |
| `order_gta` | Включить ГТА! | Start the turbine! | Driver `power_up` | `voenizdat_osa_1991` p246 |
| `order_generator` | Генератор на линию! | Generator on line! | Driver `pto_on_line` | sam3d |
| `order_engine_off` | Выключить двигатель! | Engine off! | Driver: engine off at a halt | `voenizdat_osa_1991` p246 |
| `order_equipment_on` | Включить аппаратуру! | Equipment on! | Operator `equipment_on` | sam3d |
| `order_unlock_apu` | Расстопорить АПУ! | Unlock the launcher-antenna unit! | Operator: `apu.lock` (part of `deploy_at_halt`); given by the deputy | `voenizdat_osa_1991` p246 |
| `order_drives_on` | Включить приводы! | Drives on! | Operator: `drv.az`, `drv.el`; given by the deputy | `voenizdat_osa_1991` p246 |
| `order_orient` | Ориентировать машину! | Orient the vehicle! | Deputy `orient_vehicle` | sam3d |
| `order_function_check` | Функциональный контроль! | Function check! | Deputy `function_check` | sam3d |
| `order_march` | К маршу! | Prepare to march! | Stow: operator `stow_ssc`, deputy `stow_launch_console`, senior op `stop_search`, driver `pto_off_line` | sam3d (the report "К маршу готов" is sourced) |
| `order_halt` | Стой! | Halt! | Driver halts; sets `vehicle_stopped` | sam3d |
| `order_go` | Вперёд! | Move! | Driver drives | sam3d |
| `order_otboy` | Отбой! | Stand down! | Every seat switches off in order (`equipment_off`, `power_down`, …) | sam3d |
| `order_antenna_down` | Антенну опустить! | Lower the antenna! | Commander `lower_search_antenna` (bridges) | sam3d |
| `order_antenna_up` | Антенну поднять! | Raise the antenna! | Commander `raise_search_antenna` | sam3d |
| `order_radio_on` | Радиостанции включить! | Radios on! | Driver `radio_left_on`, commander `radio_right_on` | sam3d |

### 3.2 Search and target orders

| id | ru | en | Effect | Source |
|---|---|---|---|---|
| `order_search` | Поиск! | Search! | Senior op `acquire_with_soc` | sam3d |
| `order_search_on_march` | Поиск в движении! | Search on the move! | Senior op `search_on_march` | sam3d |
| `order_stop_search` | Прекратить поиск! | Stop the search! | Senior op `stop_search` | sam3d |
| `order_beam` | Луч {beam}! | Beam {beam}! | Senior op: `soc.beam` | sam3d; beams I / I–II for low targets `voenizdat_osa_1991` p250 |
| `order_mti` | СДЦ включить! | MTI on! | Senior op `soc.mti`; operator `ssc.mti` | sam3d |
| `order_iff` | Запрос! | Interrogate! | Senior op `iff.query` | sam3d |
| `designation` | Цель номер {number}, азимут {azimuth}, дальность {range_km}, высота {height_hm}! | Target {number}, azimuth {azimuth}, range {range_km}, height {height_hm}! | Knowledge to the crew; the start of `03` §2.3 | phrasing as the PU-12's (`pu12.hpp`); sam3d |
| `order_take_target` | Цель номер {number} — взять! | Take target {number}! | Senior op azimuth; deputy `ready_capture`; operator `capture_with_ssc` | sam3d |
| `order_track_tv` | Сопровождать по телевизору! | Track on the TV! | Deputy: ПА (`fo.pa`) | sam3d |
| `order_low_target` | Низколетящая! Углы — по ТВ! | Low flier! Angles on the TV! | Deputy `track_low_target` | sam3d |
| `order_semi_auto` | Полуавтомат! | Range only! | Operator `track_range_semi_auto` | sam3d |
| `order_drop_target` | Цель сбросить! | Drop the target! | Deputy: РУ; back to search | sam3d |
| `order_as_you_were` | Отставить! | As you were! (cancel the last order) | Cancels the last order | sam3d (standard Soviet drill word) |

### 3.3 Fire orders

| id | ru | en | Effect | Source |
|---|---|---|---|---|
| `order_prepare` | Приготовиться к пуску! | Prepare to launch! | Deputy `prepare_firing` | sam3d |
| `fire_permitted` | Пуск разрешаю! | Launch permitted! | Deputy `launch` | sam3d |
| `hold_fire` | Пуск запрещаю! | Do not launch! | Deputy holds the key | sam3d |
| `order_second_missile` | Вторая — пуск! | Second missile, launch! | Deputy: ПУСК again ≥ 4 s after the first | sam3d; interval: ePap |
| `order_cease_fire` | Прекратить огонь! | Cease fire! | Deputy: key out; РУ | sam3d |
| `order_jammer` | По постановщику — метод Н! | At the jammer, method N! | Deputy `fire_at_jammer` | sam3d |
| `order_emcon_off` | Излучение выключить! | Radiation off! | Senior op `stop_search` (high voltage off); operator: SSC transmitter off | sam3d |
| `order_emcon_on` | Излучение включить! | Radiation on! | The reverse | sam3d |
| `order_emcon_tv` | Работать без излучения! | Engage without radiating! | Senior op `emcon_tv_azimuth`; deputy `emcon_tv_engagement` | sam3d, after `samsim_osa` p33–35 |
| `order_ssc_off` | Высокое ССЦ выключить! | Tracking transmitter off! | Operator `ssc_tx_off` (ПЕРЕДАТЧИК ОТКЛ): after the burst under the ARM threat, and with «Излучение выключить!» against an ARM | sam3d phrasing; the act `voenizdat_osa_1991` p262 |

The ARM drill's reports (roadmap E6): `arm_launch` «Пуск ПРР! Азимут {azimuth}!» and `arm_turn_away` «Цель, азимут
{azimuth}, отвернула — возможен пуск ПРР!» (the senior operator, on the signs of `voenizdat_osa_1991` p262; sam3d
phrasing), and on the battery net `report_arm_attack` «{callsign}: пуск ПРР, азимут {azimuth}!». The commander reacts
to hearing them, not to the truth (`docs/EM_ARCHITECTURE.md` §35).

### 3.4 Logistics and command orders

| id | ru | en | Effect | Source |
|---|---|---|---|---|
| `order_reload` | Перезарядка! | Reload! | Vehicle de-energised; the TZM is called (`reload`) | sam3d; de-energised: `mon_2112_80` |
| `order_refuel` | Заправка! | Refuel! | The fuel truck is called (`refuel`) | sam3d |
| `order_relocate` | Смена позиции! | Change position! | `order_march`, then drive to the new position | sam3d |
| `order_source` | Перейти на {source}! | Switch to {source}! | `RequestC2Source` | sam3d |

### 3.5 Acknowledgements (any seat → whoever ordered)

| id | ru | en |
|---|---|---|
| `ack` | Есть! | Yes! (understood, doing it) |
| `cannot` | Не могу: {reason}! | Can't: {reason}! |
| `done` | Выполнено! | Done! |
| `say_again` | Повторите! | Say again! |

### 3.6 Reports (seat → commander)

| id | From | ru | en | Trigger | Source |
|---|---|---|---|---|---|
| `power_on` | Driver | Питание подано! | Power on! | `power` ready | `voenizdat_osa_1991` p246 |
| `ready_to_march` | Driver | К маршу готов! | Ready to march! | Stowed, engine running | `voenizdat_osa_1991` p246 |
| `halted` | Driver | Стою! | Halted! | Vehicle stopped | sam3d |
| `fuel_half` | Driver | Топлива половина! | Half the fuel left! | Fuel ≤ 50 % | sam3d; the crew asks at half: ePap (`samsim_thread`) |
| `seat_ready` | Operator / senior op / deputy | {seat_number} готов! | Seat {n} ready! | The seat's part of "К бою" done | `voenizdat_osa_1991` p246 ("первый (второй, третий) готов") |
| `soc_ready` | Senior op | Готов СОЦ! | Search radar ready! | `soc_transmitter` ready | `voenizdat_osa_1991` p246 |
| `ssc_ready` | Operator | Готов ССЦ! | Tracking radar ready! | `ssc_transmitter` ready | sam3d (after "Готов СОЦ") |
| `spk_ready` | Deputy | Готов СПК! | Command link ready! | ГОТОВ СПК lamps | `voenizdat_osa_1991` p246 |
| `missiles_ready` | Deputy | СПК, СРП, ракеты готовы! | Link, tracker, missiles ready! | After БОРТ СПК checks | `voenizdat_osa_1991` p246 |
| `apu_locked` | Operator | АПУ застопорено! | Launcher-antenna unit locked! | `apu.lock` off | sam3d |
| `contact` | Senior op | Цель, азимут {azimuth}, дальность {range_km}, луч {beam}! | Target, azimuth {azimuth}, range {range_km}, beam {beam}! | New echo held two sweeps | sam3d |
| `contact_lost` | Senior op | Цель потерял! | Lost the target! | Echo gone | sam3d |
| `jamming` | Senior op / operator | Помеха, азимут {azimuth}! | Jamming, azimuth {azimuth}! | A strobe; range lost | sam3d |
| `iff_friend` | Senior op | Свой! | Friend! | IFF reply | sam3d |
| `iff_no_reply` | Senior op | Ответа нет! | No reply! | No reply after ЗАПРОС | sam3d |
| `az_on` | Senior op | Азимут на цели! | Azimuth on the target! | Azimuth line on the mark | sam3d |
| `range_on` | Operator | Дальность на цели! | Range on the target! | Gate on the echo | sam3d |
| `captured` | Operator | Захват! | Captured! | `target_captured` | sam3d |
| `capture_failed` | Operator | Захвата нет! | No capture! | Designated, nothing captured within the norm | sam3d |
| `lock_lost` | Operator | Срыв сопровождения! | Lost the lock! | Track lost | sam3d |
| `tv_track` | Deputy | Сопровождаю по ТВ! | Tracking on the TV! | TV track | sam3d |
| `on_jammer` | Operator | Дальности нет, сопровождаю помеху по углам! | No range, tracking the jammer in angle! | 9.1 | sam3d |
| `too_low` | Deputy | НЛЦ! | Too low for the radar! | НЛЦ lamp | sam3d |
| `target_near_zone` | Deputy | Внимание! | Attention (near the zone)! | ВНИМАНИЕ lamp | sam3d (the lamp's name) |
| `target_in_zone` | Deputy | Цель в зоне! | Target in the zone! | ЦЕЛЬ В ЗОНЕ lamp | sam3d (the lamp's name) |
| `missile_ready` | Deputy | Пуск готов! | Launch ready! | ПУСК ГОТОВ lamp | sam3d (the lamp's name) |
| `launch` | Deputy | Пуск! | Launch! | ПУСК pressed | sam3d |
| `missile_away` | Deputy | Сход! | Missile away! | СХОД lamp | sam3d (the lamp's name) |
| `hit` | Deputy | Цель поражена! | Target destroyed! | Hit on the TV / scope | sam3d |
| `miss` | Deputy | Промах! | Miss! | Miss | sam3d |
| `launcher_empty` | Deputy | Ракет нет! | No missiles left! | All containers empty | sam3d |
| `fault` | Any | Отказ {what}! | Fault: {what}! | A subsystem faulted at the seat | sam3d |
| `report_ready` | Commander → battery | {callsign} к бою готов, каналы {channels}, ракет {missiles}! | {callsign} ready for action, channels {channels}, {missiles} missiles! | R2 reached | `voenizdat_osa_1991` p246 ("к бою готов" with channels and missiles) |
| `report_result` | Commander → battery | {callsign}: цель {number} поражена, израсходовано {spent}! | {callsign}: target {number} destroyed, {spent} spent! | E8 | sam3d |
| `request_tzm` | Commander → battery | {callsign}: ракет нет, прошу ТЗМ! | {callsign}: out of missiles, request the TZM! | Launcher empty | sam3d |

### 3.7 Warnings and reminders

| id | ru | en | When |
|---|---|---|---|
| `air_alert` | Воздух! | Air! | Anyone: an aircraft seen or a threat called; urgent |
| `remind` | {seat}, {what}! | {seat}, {what}! | A waiting seat to the one it waits for, after its patience (`05`); e.g. "Оператор, передатчик!" |
| `remind_order` | {seat}, выполнять! | {seat}, carry it out! | Commander, after the second reminder |

## 4. Order → task → procedures

An order hands the drill master a **task** (conditions to make true) or starts named procedures. The mapping is data
(`data/crew/osa_ak_crew.json`, `orders`): each order lists its procedures per seat, or its task. When the commander
is the player, the order menu (`07`) offers the orders whose preconditions can be met, greys the others and says why
(the drill master's first unmet condition).

## 5. Reminders and patience

Each NPC has a patience (`05`): how long it waits on another seat's condition before calling it. The waiting seat
says `remind` naming the seat and the thing ("Оператор, приводы!"); after a second patience the commander gives
`remind_order` by name. In training the same moment raises the overlay on the player's control (`08`). The debrief
records every reminder against the seat that was waited for.
