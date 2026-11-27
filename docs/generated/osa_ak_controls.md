# 9K33M2 Osa-AK (SA-8B Gecko): every control and what it does

Written by `tools/button_map.py osa_ak` from the console (`data/procedures/osa_ak.json`), the cabin (`data/cabins/osa_ak.json`) and where else simcore names each control; the same states colour `out/button_map/osa_ak.html`. Do not edit by hand.

| State | Meaning | Controls |
|---|---|---|
| wired | drives a subsystem or the launch | 77 |
| model | read by a simulation model | 130 |
| display | sets a screen's picture or what a meter reads | 33 |
| indicator | only lights a lamp or turns on a display | 7 |
| code | named by simcore's source or a system file, not by the console | 1 |
| drill | worked in a drill, lesson or readiness state; changes nothing | 3 |
| inert | nothing names it | 26 |
| **all** | | **277** |

## wired (77): drives a subsystem or the launch

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| aiming_circle_case | буссоль (aiming circle): stowed / set up ahead | `nav.aiming_circle` | Commander | Qc measured with the aiming circle (mutual sighting) |
| commander_panel | ПРОХОД ПОД МОСТОМ | `apu.stow` | Commander | A stabilisation drive went to its limit angle: search off, the antenna and the pumps locked (ПРЕД. УГОЛ); Search antenna up and levelled |
| commander_panel | СВЯЗЬ (communications) | `veh.comms_power` | Commander | R-123M left warmed up (10 min); R-123M right warmed up (10 min); Р-124 intercom working (commander's box on) |
| commander_panel | люки (hatches) | `veh.hatches` | Commander | Launch circuit armed (live-fire mode, hatches closed, key turned, missile ready); ЛЮКИ ЗАКР.; РУ |
| dp3b_panel | DP-3B: ПРОВЕРКА | `dp3b.check` | Commander | DP-3B tested |
| dp3b_panel | DP-3B: ПЕРЕКЛЮЧАТЕЛЬ ДИАПАЗОНОВ | `veh.dp3b` | Commander | DP-3B tested; DP-3B: on |
| driver_panel | ОСТАНОВ ДВИГАТЕЛЯ (the drive engine stopped) | `veh.engine_stop` | Driver | Drive engine 5Д20Б-300 running; Engine: starter, stop, hand throttle, warm-up and oil |
| driver_panel | СТАРТЕР (the drive engine's starter button) | `veh.starter` | Driver | Drive engine 5Д20Б-300 running; Engine: starter, stop, hand throttle, warm-up and oil |
| fire_panel | fire button (ПРИ ПОЖАРЕ ... НАЖМИ КНОПКУ) | `fire.button` | Driver | Fire-suppression bottles fired; Fire: the compartments, the bottles, the automat |
| fire_panel | ПРОВЕРКА ЦЕПЕЙ | `fire.check` | Driver | Fire-suppression circuits checked (ПРОВЕРКА ЦЕПЕЙ); Fire: the compartments, the bottles, the automat |
| fire_panel | cover over ПРОВЕРКА ЦЕПЕЙ | `fire.check_cover` | Driver | Fire-suppression circuits checked (ПРОВЕРКА ЦЕПЕЙ) |
| fire_panel | cover: ПРИ ПОЖАРЕ ОТКРОЙ КРЫШКУ И НАЖМИ КНОПКУ | `fire.cover` | Driver | Fire-suppression bottles fired; Fire: the compartments, the bottles, the automat |
| fo_panel | АЗ | `fo.az` | Deputy commander | Auto-capture (АЗ) armed |
| fo_panel | БОРТ СПК | `fo.bort_spk` | Deputy commander | Missile, SPK and SVR preparation (ВКЛ ВЫС, then ПУСК ГОТОВ) |
| fo_panel | guard over БОРТ СПК | `fo.bort_spk_cover` | Deputy commander | nothing |
| fo_panel | ЦУ | `fo.cu_az` | Deputy commander | Auto-capture (АЗ) armed; Target designation (ЦУ): tracking antenna to the search beam, elevation search |
| fo_panel | ЗАПРЕТ ПУСКА | `fo.key` | Deputy commander | Launch circuit armed (live-fire mode, hatches closed, key turned, missile ready) |
| fo_panel | ПУСК | `fo.launch` | Deputy commander | ПУСК: missile away |
| fo_panel | РОД РАБОТЫ | `fo.mode` | Deputy commander | Function check (ФК) running; Launch circuit armed (live-fire mode, hatches closed, key turned, missile ready); The missile tracker's medium-beam antenna switched in by hand (РР, ШИР. — СРЕДН. at СРЕДН.); БР |
| fo_panel | ПА | `fo.pa` | Deputy commander | TOV angle tracking (ПА) |
| fo_panel | РУ | `fo.ru` | Deputy commander | Auto-capture (АЗ) armed; Manual capture (ТСЦ pressed on the target); TOV angle tracking (ПА); Target designation (ЦУ): tracking antenna to the search beam, elevation search |
| fo_panel | ТСЦ | `fo.tsc` | Deputy commander | Manual capture (ТСЦ pressed on the target) |
| fo_panel | guard over АВАРИЙНЫЙ ПУСК | `msl.emergency_cover` | Deputy commander | АВАР. ПУСК ОБЕСТОЧ. |
| fo_panel | elevation wheel lock | `ssc.el_lock` | Deputy commander | nothing |
| fo_panel | lid over БАЛАНС ПА (УГОЛ МЕСТА) | `ssc.pa_balance_el_cover` | Deputy commander | nothing |
| fo_panel | ШИР. — СРЕДН. | `svr.beam` | Deputy commander | The missile tracker's medium-beam antenna switched in by hand (РР, ШИР. — СРЕДН. at СРЕДН.) |
| fo_panel | КОНТРОЛЬ — РАБОТА (ВПУ) | `tov.check` | Deputy commander | The TV shows its test chart 0249 (ВПУ at КОНТРОЛЬ); The TV shows the scene through its lens (ВПУ at РАБОТА) |
| fo_panel | РАБ.РЕЖ. | `tov.power` | Deputy commander | TOV camera on |
| gear_lever_panel | gear lever | `veh.gear` | Driver | Drive engine 5Д20Б-300 running; Drive: gears, handbrake, the water-jets and wave shield |
| gyro_arrest_panel | АРРЕТИР. | `gyro.arrest` | Driver | Gyro vertical erected (АРРЕТИР., 40 s) |
| gyro_panel | ПРИВОД γ | `drv.gamma_service` | Deputy commander | Search-antenna stabilisation (9W611BM2) working |
| gyro_panel | +27 В | `drv.plus27` | Deputy commander | Search-antenna stabilisation (9W611BM2) working |
| gyro_panel | ПРИВОД Ψ | `drv.psi_service` | Deputy commander | Search-antenna stabilisation (9W611BM2) working |
| gyro_panel | АНОД ВКЛ. | `fk.anode` | Deputy commander | The function-check set's anode supply on (АНОД ВКЛ.) |
| gyro_panel | ОБОГРЕВ — ВКЛ. | `gyro.heat` | Deputy commander | Climate: the cabin, racks and transmitters' heat; The gyro warm enough to erect (its heater on below +5 C) |
| heater_panel | СВЕЧА (pre-heater) | `heat.plug` | Commander | Engine pre-heater burning; pre-heater: glow plug window |
| heater_panel | pre-heater button | `heat.start` | Commander | Engine pre-heater burning |
| ind_panel | АВАР. ОТКЛ. СЭП | `pwr.emergency_off` | Deputy commander | The power system cut by АВАР. ОТКЛ. СЭП |
| op_panel | АВТОМАТ | `ro.auto` | Operator | SSC range tracking by the radar (ПОЛУАВТОМАТ, АВТОМАТ) |
| op_panel | РУЧНОЕ | `ro.manual` | Operator | SSC range tracking by the radar (ПОЛУАВТОМАТ, АВТОМАТ) |
| op_panel | ПОЛУАВТОМАТ | `ro.semi_auto` | Operator | SSC range tracking by the radar (ПОЛУАВТОМАТ, АВТОМАТ) |
| op_panel | cover over АСТИГМАТ. I | `ssc.astig_cover_1` | Operator | nothing |
| op_panel | cover over АСТИГМАТ. II | `ssc.astig_cover_2` | Operator | nothing |
| op_panel | ПЕРЕДАТЧИК | `ssc.tx` | Operator | SSC range tracking by the radar (ПОЛУАВТОМАТ, АВТОМАТ); SSC transmitter radiating |
| op_panel | guard over ПЕРЕДАТЧИК | `ssc.tx_cover` | Operator | nothing |
| pu303_panel | ПИТАНИЕ УПРАВЛЕНИЯ (control power) | `gta.control_power` | Driver | The gas turbine's hatches open; ДАВЛ. МАСЛА; ЛЮКИ ЗАКР.; ЛЮКИ ОТКР.; НАГРУЗКИ ГОМ; НАГРУЗКИ ГТА; НАГРУЗКИ ИВП; ОБРАТ. ЧЕРЕД.; ОБРЫВ ФАЗ; ПИТАНИЕ УПРАВЛЕНИЯ; ПОДСВЕТКА (left); ПОДСВЕТКА (right); ПР ВС |
| pu303_panel | ЛЮКИ ЗАКР. (the turbine's hatches closed) | `gta.hatches_close` | Driver | The gas turbine's hatches open |
| pu303_panel | ЛЮКИ ОТКР. (the turbine's hatches open) | `gta.hatches_open` | Driver | The gas turbine's hatches open |
| pu303_panel | ВКЛЮЧЕНИЕ НАГРУЗОК: ГТА (the loads on the turbine's generator) | `gta.load_gta` | Driver | The loads taken off the generators (ОТКЛ. НАГР.) |
| pu303_panel | ВКЛЮЧЕНИЕ НАГРУЗОК: ИВП | `gta.load_ivp` | Driver | The loads taken off the generators (ОТКЛ. НАГР.) |
| pu303_panel | ОТКЛ. НАГР. (loads off) | `gta.loads_off` | Driver | The loads taken off the generators (ОТКЛ. НАГР.) |
| pu303_panel | СТОП (the turbine stopped) | `gta.stop` | Driver | The ГТА stopped by СТОП (until ПУСК is put off) |
| pu303_panel | ГТА (gas-turbine power unit) | `pwr.gta` | Driver | Gas-turbine power unit (9И120) running; The power system cut by АВАР. ОТКЛ. СЭП; The ГТА stopped by СТОП (until ПУСК is put off) |
| pu303_panel | power take-off generator | `pwr.pto` | Driver | Engine power take-off generator on line |
| pwr_cabinet | походный стопор АПУ (АПУ travel lock) | `apu.lock` | Operator | АПУ unlocked; СТОПОР ПОХОД. |
| pwr_cabinet | ПРИВОД βн ВКЛ. — ОТКЛ. | `drv.az` | Operator | The ЭМУ's thermal relays open: the azimuth drive's motor off; Tracking-radar and missile-tracker drives on (АПУ can turn); ПРИВОД βн |
| pwr_cabinet | ПРИВОДЫ εн, СВР-I, СВР-II ВКЛ. — ОТКЛ. | `drv.el` | Operator | Tracking-radar and missile-tracker drives on (АПУ can turn); ПРИВОДЫ εн, СВР; ПРИВОДЫ СВР I; ПРИВОДЫ СВР II |
| pwr_cabinet | ПОДКАЧКА ОТСЕКОВ АВАРИЙНАЯ — ОТКЛ. | `press.emergency` | Operator | Compartments' pressurisation on (ЭПК opened) |
| pwr_cabinet | ПОДКАЧКА ОТСЕКОВ ШТАТНАЯ | `press.standard` | Operator | Compartments' pressurisation on (ЭПК opened) |
| pwr_cabinet | АППАРАТУРА ВКЛ. — ОТКЛ. | `pwr.equipment` | Operator | 0009-5M's ОШИБКА ψ and ОШИБКА γ glowing; Climate: the cabin, racks and transmitters' heat; Equipment warmed up (anode supplies on, scopes sweeping); Fire-control computer 9С456М3 switched on; I ЛУЧ; II ЛУЧ; III ЛУЧ; Search-antenna stabilisation (9W611BM2) working; The missile tracker's medium-beam antenna switched in by hand (РР, ШИР. — СРЕДН. at СРЕДН.); The power system cut by АВАР. ОТКЛ. СЭП; Transmitters ready for high voltage (three-minute relay); βн ЗАСТОПОР.; АНТЕННА СОЦ ЗАСТОПОРЕНА; ГАБАРИТ 3800; НАКАЛ СОЦ; НАКАЛ СПК-I; НАКАЛ СПК-II; НАКАЛ ССЦ; ОТКАЗ СРП; ПРЕД. УГОЛ; ПРИВОД βн; ПРИВОДЫ εн, СВР; ПРИВОДЫ СВР I; ПРИВОДЫ СВР II; РАССТОПОРЕНО Δε, εн; РАССТОПОРЕНО βн; РУ; СТОПОР ПОХОД.; УКЛАДКА ПРОИЗВ.; ЭКВИВАЛЕНТ СОЦ; ЭКВИВАЛЕНТ СПК I; ЭКВИВАЛЕНТ СПК II |
| pwr_cabinet | РЕЛЕ ТЕПЛ. ВКЛ. — ОТКЛ. | `pwr.relay_heat` | Operator | The turret turns in azimuth (the drives on, the ЭМУ's control winding fed); The ЭМУ's drive motor overloaded with its relays shunted; The ЭМУ's thermal relays open: the azimuth drive's motor off; ПРИВОД βн |
| pwr_cabinet | РАССТОПОР. — СТОПОР. | `soc.rotation_lock` | Operator | SOC search antenna rotating; АНТЕННА СОЦ ЗАСТОПОРЕНА |
| pwr_cabinet | СИСТЕМА СТАБ. ВКЛ. — ОТКЛ. | `stab.power` | Operator | 0009-5M's ОШИБКА ψ and ОШИБКА γ glowing; Search-antenna stabilisation (9W611BM2) working |
| pwr_cabinet | ТОВ — ОТКЛ. | `tov.supply` | Operator | TOV camera on |
| r124_a1_panel | Р-124 А-1: род работы | `intercom.a1` | Commander | R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone; Р-124 intercom working (commander's box on) |
| radio_left_panel | R-123M left: режим | `radio.left.mode` | Driver | R-123M left ready to transmit (3 min after duty reception); R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| radio_left_panel | R-123M left: ПИТАНИЕ | `radio.left.power` | Driver | R-123M left warmed up (10 min); R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| radio_right_panel | R-123M right: режим | `radio.right.mode` | Commander | R-123M right ready to transmit (3 min after duty reception); R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| radio_right_panel | R-123M right: ПИТАНИЕ | `radio.right.power` | Commander | R-123M right warmed up (10 min); R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| radio_well_panel | switch box with the red lever (left radio well) | `veh.well_switch` | Driver | Electrics: batteries, the network, the 220 V supply and УКЧ; The chassis' 24 V network on (battery switch ВБ) |
| repair_lid_panel | 0096-9M2: крышка ремонтных тумблеров | `pwr.repair_lid` | Operator | 0096-9M2's repair toggles' cover shut |
| so_panel | ВЫСОКОЕ | `soc.hv` | Senior operator | SOC transmitter radiating |
| so_panel | guard over ВЫСОКОЕ | `soc.hv_cover` | Senior operator | nothing |
| so_panel | ПОИСК | `soc.search` | Senior operator | SOC search antenna rotating |
| so_panel | ЭМУ ОТКЛ. — ВКЛ. | `ssc.emu` | Senior operator | The turret turns in azimuth (the drives on, the ЭМУ's control winding fed); The ЭМУ's drive motor overloaded long enough to heat its thermal relays |
| sp2m_panel | ТРЕНИРОВКА — РАБОТА СОЦ | `soc.training` | Senior operator | SOC transmitter radiating |
| sp2m_panel | ТРЕНИРОВКА — РАБОТА ССЦ | `ssc.training` | Senior operator | SSC transmitter radiating |

## model (130): read by a simulation model

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| bulkhead_left_panel | ЗИМА — ЛЕТО (left cover) | `vent.cover_left` | Operator | Climate: the cabin, racks and transmitters' heat |
| bulkhead_panel | ЗИМА — ЛЕТО (right cover) | `vent.cover_right` | Operator | Climate: the cabin, racks and transmitters' heat |
| commander_panel | ТА-57: трубка | `ta57.handset` | Commander | Voices: the intercom boxes, headsets, sets and telephone |
| commander_panel | ВОДООТРАЖАТЕЛЬНЫЙ ЩИТ ↓↑ (wave shield) | `veh.bilge_pump` | Commander | Drive: gears, handbrake, the water-jets and wave shield |
| commander_panel | ОТОПИТЕЛЬ (heater) | `veh.heater` | Commander | Climate: the cabin, racks and transmitters' heat |
| commander_panel | ЛАМПЫ: ПЕРЕДНЯЯ (front lamp) | `veh.lamps_front` | Commander | Lights: headlamps, sidelights, blackout, the cabin |
| commander_panel | ЛАМПЫ: ЗАДНЯЯ (rear lamp) | `veh.lamps_rear` | Commander | Lights: headlamps, sidelights, blackout, the cabin |
| commander_panel | ПОДФАРНИКИ (side lights) | `veh.sidelights` | Commander | Lights: headlamps, sidelights, blackout, the cabin |
| driver_panel | Headset (шлемофон): driver (ПВ) | `headset.driver` | Driver | Voices: the intercom boxes, headsets, sets and telephone |
| driver_panel | Р-124 ПВ: род работы | `intercom.pv` | Driver | R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| driver_panel | нагрудный переключатель (ПВ): ПРМ — ПРД | `radio.chest_driver` | Driver | R-123M sets and R-124 intercom |
| driver_panel | flat lever under the left gauge (hand throttle?) | `veh.hand_throttle` | Driver | Engine: starter, stop, hand throttle, warm-up and oil |
| driver_panel | ВНУТРЕН. ОСВЕЩЕН. (inside lighting) | `veh.inside_lights` | Driver | Lights: headlamps, sidelights, blackout, the cabin |
| driver_panel | ПРИБОРЫ (instrument lighting) | `veh.instr_lights` | Driver | Lights: headlamps, sidelights, blackout, the cabin; Амперметр (A); Вода (coolant °C); Давление масла (kgf/cm²); Масло (oil °C); Спидометр (km/h); Тахометр (×100 rpm); Топливо (fuel) |
| driver_panel | ЦЕНТРАЛЬНЫЙ ПЕРЕКЛЮЧАТЕЛЬ СВЕТА (central light switch) | `veh.light_switch` | Driver | Lights: headlamps, sidelights, blackout, the cabin |
| driver_panel | НАРУЖНОЕ ОСВЕЩЕН. (outside lighting) | `veh.outside_lights` | Driver | Lights: headlamps, sidelights, blackout, the cabin |
| driver_panel | ЗАСЛОНКА ВОДОМЕТОВ: ЛЕВАЯ | `veh.waterjet_left` | Driver | Drive: gears, handbrake, the water-jets and wave shield |
| driver_panel | ЗАСЛОНКА ВОДОМЕТОВ: ПРАВАЯ | `veh.waterjet_right` | Driver | Drive: gears, handbrake, the water-jets and wave shield |
| fire_panel | ПЕРЕВОД ДИСКА — ОСТАТОК БАЛЛОНОВ | `fire.disc` | Driver | Fire: the compartments, the bottles, the automat |
| fo_panel | ЧАСТОТА | `err.freq` | Deputy commander | The BM's checks: the function-check meters, the noise check |
| fo_panel | РЕЖИМЫ | `err.mode` | Deputy commander | The BM's checks: the function-check meters, the noise check |
| fo_panel | ВЫВОД — НАВЕДЕНИЕ | `err.output` | Deputy commander | The BM's checks: the function-check meters, the noise check |
| fo_panel | МЦ — ОТКЛ. | `fo.mc` | Deputy commander | Tracking radar and TV channel |
| fo_panel | ПОМЕХА | `fo.pomekha` | Deputy commander | Missiles and guidance channels I / II |
| fo_panel | КАНАЛЫ | `msl.channels` | Deputy commander | Missiles and guidance channels I / II |
| fo_panel | АВАРИЙНЫЙ ПУСК | `msl.emergency` | Deputy commander | Missiles and guidance channels I / II |
| fo_panel | ИЗДЕЛИЯ | `msl.emergency_select` | Deputy commander | Missiles and guidance channels I / II |
| fo_panel | СМЕНА ЛИТ. — НОРМА | `spk.letter` | Deputy commander | Missiles and guidance channels I / II |
| fo_panel | ПОДСВЕТКА (εс) | `ssc.el_light` | Deputy commander | εс |
| fo_panel | УГОЛ МЕСТА | `ssc.el_wheel` | Deputy commander | Tracking radar and TV channel; εс |
| fo_panel | БАЛАНС ПА (УГОЛ МЕСТА) | `ssc.pa_balance_el` | Deputy commander | Tracking radar and TV channel |
| fo_panel | СВЕТОФИЛЬТР | `tov.filter` | Deputy commander | Tracking radar and TV channel |
| fo_panel | УГОЛ ЗРЕНИЯ | `tov.fov` | Deputy commander | TOV monitor; Tracking radar and TV channel |
| fo_panel | ВЕНТИЛЯЦИЯ РАБОЧИХ МЕСТ — ОТКЛ. | `vent.workplaces` | Deputy commander | Climate: the cabin, racks and transmitters' heat |
| gyro_panel | ПУСК — ИСХ. ПОЛОЖ. | `fk.launch` | Deputy commander | The BM's checks: the function-check meters, the noise check |
| gyro_panel | ПРОВЕРКА СМЕНЫ ЛИТЕРА | `fk.letter` | Deputy commander | The BM's checks: the function-check meters, the noise check |
| gyro_panel | УРОВНИ КОМАНД | `fk.levels` | Deputy commander | The BM's checks: the function-check meters, the noise check |
| gyro_panel | СМЕНА ЗНАКА | `fk.sign` | Deputy commander | The BM's checks: the function-check meters, the noise check |
| gyro_panel | КОНТРОЛЬ СИСТЕМ | `fk.systems` | Deputy commander | The BM's checks: the function-check meters, the noise check |
| gyro_panel | ПРОВЕРКА ПЕРЕХОДНЫХ ХАРАКТЕРИСТИК | `fk.transients` | Deputy commander | The BM's checks: the function-check meters, the noise check |
| gyro_panel | МК — ГПК — АК | `gyro.compass_mode` | Deputy commander | Heading: gyro half-compass |
| gyro_panel | ПОТРЕБИТЕЛИ ОСН. — КОНТР. | `gyro.consumers` | Deputy commander | Heading: gyro half-compass |
| gyro_panel | φ АВТ. — РУЧН. | `gyro.phi_mode` | Deputy commander | Heading: gyro half-compass |
| gyro_panel | ВОЗВРАТ (II) | `msl.return_2` | Deputy commander | Missiles and guidance channels I / II |
| gyro_panel | АК — ЗК | `orient.ak_zk` | Deputy commander | Heading: gyro half-compass |
| gyro_panel | ШИРОТА (φ) | `orient.latitude` | Deputy commander | Heading: gyro half-compass; ШИРОТА |
| gyro_panel | ЗАДАТЧИК КУРСА ЗК-4 (Qc) | `orient.qc` | Deputy commander | Heading: gyro half-compass; ЗК-4 (Qc) |
| gyro_panel | РЕЛЕ — Qc | `orient.relay` | Deputy commander | Heading: gyro half-compass |
| gyro_panel | СОГЛАСОВАНИЕ | `orient.sync` | Deputy commander | Heading: gyro half-compass |
| handbrake_panel | handbrake | `veh.handbrake` | Driver | Drive: gears, handbrake, the water-jets and wave shield |
| ind_panel | ОТКАЗ (the missile on preparation refused) | `ind.refuse` | Deputy commander | Missiles and guidance channels I / II |
| ind_panel | ПЕРЕКЛЮЧАТЕЛЬ ПАНЕЛИ ИНДИКАЦИИ | `ind.select` | Deputy commander | Missiles and guidance channels I / II |
| ind_panel | УКЧ ВКЛ. — ОТКЛ. | `pwr.ukch` | Deputy commander | Electrics: batteries, the network, the 220 V supply and УКЧ |
| op_panel | БАЛАНС АВТОМАТА | `ro.balance` | Operator | Tracking radar and TV channel |
| op_panel | РПЧМ — АПЧМ (ССЦ) | `ssc.afc` | Operator | Tracking radar and TV channel |
| op_panel | ССЦ РПЧМ | `ssc.afc_trim` | Operator | Tracking radar and TV channel |
| op_panel | РРУ КШ ДОП. | `ssc.control_sel` | Operator | The BM's checks: the function-check meters, the noise check |
| op_panel | РЕЖИМЫ | `ssc.modes` | Operator | The BM's checks: the function-check meters, the noise check |
| op_panel | СДЦ / ШТ. (SSC) | `ssc.mti` | Operator | Tracking radar and TV channel |
| op_panel | ПОДЖИГ ГШ | `ssc.noise_fire` | Operator | The BM's checks: the function-check meters, the noise check |
| op_panel | range wheel | `ssc.range_wheel` | Operator | Tracking radar and TV channel |
| op_panel | УВ. ПОМЕХА — ОТКЛ. | `ssc.rgpo` | Operator | Tracking radar and TV channel |
| op_panel | РРУ КШ ОСН. | `ssc.rru_main` | Operator | Range indicator; The BM's checks: the function-check meters, the noise check |
| op_panel | Р I — ЦЕЛЬ — Р II | `ssc.target_select` | Operator | Missiles and guidance channels I / II |
| op_panel | КОМПЕНСАЦИЯ ВЕТРА ГРУБО | `ssc.wind_coarse` | Operator | Tracking radar and TV channel |
| op_panel | КОМПЕНС. ВЕТРА — ОТКЛ. (ССЦ) | `ssc.wind_comp` | Operator | Tracking radar and TV channel |
| op_panel | КОМПЕНСАЦИЯ ВЕТРА ТОЧНО | `ssc.wind_fine` | Operator | Tracking radar and TV channel |
| post_valve_panel | ВЕНТИЛЯЦИЯ — ОТОПЛЕНИЕ | `veh.heat_vent` | Driver | Climate: the cabin, racks and transmitters' heat |
| pu303_panel | НАПР. ГОМ | `gta.volt_gom` | Driver | Electrics: batteries, the network, the 220 V supply and УКЧ |
| pu303_panel | НАПР. ГТА | `gta.volt_gta` | Driver | Electrics: batteries, the network, the 220 V supply and УКЧ |
| pwr_cabinet | ЗИМА — ЛЕТО | `vent.season` | Operator | Climate: the cabin, racks and transmitters' heat |
| r124_a1_panel | Headset (шлемофон): commander (A-1) | `headset.commander` | Commander | Voices: the intercom boxes, headsets, sets and telephone |
| r124_a1_panel | ГРОМКОСТЬ (A-1) | `intercom.a1_volume` | Commander | Voices: the intercom boxes, headsets, sets and telephone |
| r124_a1_panel | нагрудный переключатель (А-1): ПРМ — ПРД | `radio.chest` | Commander | R-123M sets and R-124 intercom |
| r124_a2_panel | Headset (шлемофон): deputy commander (A-2) | `headset.deputy_commander` | Deputy commander | Voices: the intercom boxes, headsets, sets and telephone |
| r124_a2_panel | Р-124 А-2: род работы | `intercom.a2` | Deputy commander | R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| r124_a3_panel | Headset (шлемофон): senior operator (A-3) | `headset.senior_operator` | Senior operator | Voices: the intercom boxes, headsets, sets and telephone |
| r124_a5_panel | Headset (шлемофон): operator (A-4) | `headset.operator` | Operator | Voices: the intercom boxes, headsets, sets and telephone |
| radio_left_panel | R-123M left: стопор НАСТРОЙКА АНТЕННЫ | `radio.left.ant_lock` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: НАСТРОЙКА АНТЕННЫ | `radio.left.ant_tune` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: антенна | `radio.left.antenna` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: фиксатор 1 | `radio.left.fix1` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: фиксатор 2 | `radio.left.fix2` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: фиксатор 3 | `radio.left.fix3` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: фиксатор 4 | `radio.left.fix4` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: крышка фиксаторов | `radio.left.fix_cover` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: УСТАНОВКА ЧАСТОТЫ | `radio.left.freq` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: preset | `radio.left.preset` | Driver | Command network: sources, presets, headset; Voices: the intercom boxes, headsets, sets and telephone |
| radio_left_panel | R-123M left: ШУМЫ | `radio.left.squelch` | Driver | R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| radio_left_panel | R-123M left: ПОДДИАПАЗОН I — II | `radio.left.subband` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: ТОН. ВЫЗОВ | `radio.left.tone` | Driver | R-123M sets and R-124 intercom |
| radio_left_panel | R-123M left: ГРОМКОСТЬ | `radio.left.volume` | Driver | R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| radio_right_panel | headset: which R-123M the commander listens to | `radio.headset` | Commander | Command network: sources, presets, headset; Voices: the intercom boxes, headsets, sets and telephone |
| radio_right_panel | R-123M right: стопор НАСТРОЙКА АНТЕННЫ | `radio.right.ant_lock` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: НАСТРОЙКА АНТЕННЫ | `radio.right.ant_tune` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: антенна | `radio.right.antenna` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: фиксатор 1 | `radio.right.fix1` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: фиксатор 2 | `radio.right.fix2` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: фиксатор 3 | `radio.right.fix3` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: фиксатор 4 | `radio.right.fix4` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: крышка фиксаторов | `radio.right.fix_cover` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: УСТАНОВКА ЧАСТОТЫ | `radio.right.freq` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: preset | `radio.right.preset` | Commander | Command network: sources, presets, headset; Voices: the intercom boxes, headsets, sets and telephone |
| radio_right_panel | R-123M right: ШУМЫ | `radio.right.squelch` | Commander | R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| radio_right_panel | R-123M right: ПОДДИАПАЗОН I — II | `radio.right.subband` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: ТОН. ВЫЗОВ | `radio.right.tone` | Commander | R-123M sets and R-124 intercom |
| radio_right_panel | R-123M right: ГРОМКОСТЬ | `radio.right.volume` | Commander | R-123M sets and R-124 intercom; Voices: the intercom boxes, headsets, sets and telephone |
| so_panel | ОТКР. ПРИЕМ — КЛАПАН | `iff.gate` | Senior operator | IFF interrogator |
| so_panel | КОНТР. ИНД. | `iff.ind_check` | Senior operator | IFF interrogator |
| so_panel | ЗАПРОС | `iff.query` | Senior operator | IFF interrogator |
| so_panel | ЗАПРОС К | `iff.query_k` | Senior operator | IFF interrogator |
| so_panel | ВОЗВРАТ (I) | `msl.return_1` | Senior operator | Missiles and guidance channels I / II |
| so_panel | РПЧМ — АПЧМ (СОЦ) | `soc.afc` | Senior operator | Search radar picture |
| so_panel | РЕГ. РПЧМ | `soc.afc_trim` | Senior operator | Search radar picture |
| so_panel | ЛУЧ | `soc.beam` | Senior operator | Elevation search in the beam (ЦУ); Search radar picture |
| so_panel | СДЦ / ШТ. (SOC) | `soc.mti` | Senior operator | Search radar picture |
| so_panel | СДЦ ВНЕШН. — ВНУТР. | `soc.mti_ext` | Senior operator | Search radar picture |
| so_panel | ДАЛЬНОСТЬ | `soc.range` | Senior operator | Search radar picture |
| so_panel | СА — ОТКЛ. | `soc.sa` | Senior operator | Search radar picture |
| so_panel | КОМПЕНСАЦИЯ ВЕТРА ГРУБО (СОЦ) | `soc.wind_coarse` | Senior operator | Search radar picture |
| so_panel | КОМПЕНС. ВЕТРА — ОТКЛ. (СОЦ) | `soc.wind_comp` | Senior operator | Search radar picture |
| so_panel | КОМПЕНСАЦИЯ ВЕТРА ТОЧНО (СОЦ) | `soc.wind_fine` | Senior operator | Search radar picture |
| so_panel | ПОДСВЕТКА (βс) | `ssc.az_light` | Senior operator | βс |
| so_panel | azimuth wheel lock | `ssc.az_lock` | Senior operator | Tracking radar and TV channel |
| so_panel | АЗИМУТ | `ssc.az_wheel` | Senior operator | Tracking radar and TV channel; βс |
| so_panel | БАЛАНС ПА (АЗИМУТ) | `ssc.pa_balance_az` | Senior operator | Tracking radar and TV channel |
| so_panel | ВЛЕВО — ВПРАВО | `ssc.slew` | Senior operator | Tracking radar and TV channel |
| sp2m_panel | СС — ОТКЛ. | `gyro.follow_up` | Senior operator | Heading: gyro half-compass |
| vent_hatch_left | ЛЮК ВЕНТИЛЯЦИИ ЗАКР. — ОТКР. (left) | `vent.hatch_left` | Deputy commander | Climate: the cabin, racks and transmitters' heat; ЛЮКИ РТА ЗАКРЫТЫ |
| vent_hatch_right | ЛЮК ВЕНТИЛЯЦИИ ЗАКР. — ОТКР. (right) | `vent.hatch_right` | Commander | Climate: the cabin, racks and transmitters' heat; ЛЮКИ РТА ЗАКРЫТЫ |

## display (33): sets a screen's picture or what a meter reads

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| fo_panel | ЯРКОСТЬ (ε) | `fo.el_bright` | Deputy commander | Elevation-search indicator OS81-10M |
| fo_panel | КОНТРАСТ (ε) | `fo.el_contrast` | Deputy commander | Elevation-search indicator OS81-10M |
| fo_panel | ФОКУС (ε) | `fo.el_focus` | Deputy commander | Elevation-search indicator OS81-10M |
| fo_panel | ГОРИЗ. СМЕЩ. (ε) | `fo.el_hshift` | Deputy commander | Elevation-search indicator OS81-10M |
| fo_panel | АМПЛ. РАЗВ. ε | `fo.el_sweep` | Deputy commander | Elevation-search indicator OS81-10M |
| fo_panel | ВЕРТ. СМЕЩ. (ε) | `fo.el_vshift` | Deputy commander | Elevation-search indicator OS81-10M |
| fo_panel | ЯРКОСТЬ (ВПУ) | `tov.bright` | Deputy commander | TOV monitor |
| fo_panel | КОНТРАСТ (ВПУ) | `tov.contrast` | Deputy commander | TOV monitor |
| fo_panel | ПЕРЕКРЕСТ | `tov.cross` | Deputy commander | TOV monitor |
| fo_panel | ФОКУС (ВПУ) | `tov.focus` | Deputy commander | TOV monitor |
| fo_panel | ВИДИКОН | `tov.vidicon` | Deputy commander | TOV monitor |
| op_panel | ТОК, МОЩНОСТЬ | `mon.tx_select` | Operator | МОЩНОСТЬ; ТОК |
| op_panel | АРУ — РРУ | `ssc.agc` | Operator | Range indicator |
| op_panel | АСТИГМАТ. I | `ssc.astig_1` | Operator | Range indicator |
| op_panel | АСТИГМАТ. II | `ssc.astig_2` | Operator | Range indicator |
| op_panel | ЯРКОСТЬ I | `ssc.bright_1` | Operator | Range indicator |
| op_panel | ЯРКОСТЬ II | `ssc.bright_2` | Operator | Range indicator |
| op_panel | ДОП. УСИЛ. — ОТКЛ. | `ssc.extra_gain` | Operator | Range indicator |
| op_panel | ФОКУС I | `ssc.focus_1` | Operator | Range indicator |
| op_panel | ФОКУС II | `ssc.focus_2` | Operator | Range indicator |
| op_panel | ЯРКОСТЬ СТРОБА Р | `ssc.gate_bright` | Operator | Range indicator |
| op_panel | ГОРИЗ. СМЕЩ. I | `ssc.hshift_1` | Operator | Range indicator |
| op_panel | ГОРИЗ. СМЕЩ. II | `ssc.hshift_2` | Operator | Range indicator |
| op_panel | ВЕРТ. СМЕЩ. I | `ssc.vshift_1` | Operator | Range indicator |
| op_panel | ВЕРТ. СМЕЩ. II | `ssc.vshift_2` | Operator | Range indicator |
| so_panel | ЯРКОСТЬ | `soc.bright` | Senior operator | SOC indicator |
| so_panel | КОНТРАСТНОСТЬ | `soc.contrast` | Senior operator | SOC indicator |
| so_panel | ШКАЛА КУРСОВОГО ПАРАМЕТРА | `soc.course_scale` | Senior operator | SOC indicator |
| so_panel | ЯРКОСТЬ ВИЗИРА | `soc.cursor_bright` | Senior operator | SOC indicator |
| so_panel | ФОКУС | `soc.focus` | Senior operator | SOC indicator |
| so_panel | РРУ | `soc.gain` | Senior operator | SOC indicator |
| so_panel | ЯРКОСТЬ МЕТОК | `soc.marks_bright` | Senior operator | SOC indicator |
| so_panel | ОСВЕЩ. ШКАЛЫ | `soc.scale_light` | Senior operator | SOC indicator |

## indicator (7): only lights a lamp or turns on a display

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| bulkhead_panel | engine door: lower dog | `hull.door_dog_lower` | Operator | ДВЕРЬ ЗАКРЫТА |
| bulkhead_panel | engine door: upper dog | `hull.door_dog_upper` | Operator | ДВЕРЬ ЗАКРЫТА |
| fo_panel | ОСВЕЩ. | `err.light` | Deputy commander | СО β; СО ε |
| gyro_panel | КЗ | `fk.kz` | Deputy commander | КЗ |
| op_panel | РАБОТА / КОНТРОЛЬ / КАЛИБР. | `ssc.check_mode` | Operator | Range indicator |
| pu303_panel | ПОДСВЕТКА (the panel's lights) | `gta.backlight` | Driver | ПОДСВЕТКА (left); ПОДСВЕТКА (right) |
| pu303_panel | КОНТР. ЛАМП (lamp test) | `gta.lamp_test` | Driver | ДАВЛ. МАСЛА; ЛЮКИ ЗАКР.; ЛЮКИ ОТКР.; НАГРУЗКИ ГОМ; НАГРУЗКИ ГТА; НАГРУЗКИ ИВП; ОБРАТ. ЧЕРЕД.; ОБРЫВ ФАЗ; ПР ВС |

## code (1): named by simcore's source or a system file, not by the console

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| so_panel | КОД | `iff.code` | Senior operator | simcore/include/simcore/scenario_runner.hpp |

## drill (3): worked in a drill, lesson or readiness state; changes nothing

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| gyro_panel | РЕЛЕ — Ψ, Θ | `gyro.relay_pt` | Deputy commander | data/crew/osa_ak_crew.json |
| gyro_panel | ПИТАНИЕ ПТ — СЕТЬ | `gyro.supply` | Deputy commander | data/crew/osa_ak_crew.json |
| gyro_panel | КОНТРОЛЬ РАБОТЫ (ОС51-8М) | `soc.mti_check` | Deputy commander | data/crew/osa_ak_crew.json |

## inert (26): nothing names it

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| air_panel | cock under the air gauge | `air.cock` | Operator | nothing |
| column_stalk_panel | column stalk (red knob) | `veh.column_stalk` | Driver | nothing |
| commander_panel | ПРЕОБРАЗОВАТЕЛЬ (converter) | `veh.converter` | Commander | nothing |
| commander_panel | ОСВЕТИТЕЛЬ (the strip's first switch) | `veh.illuminator` | Commander | nothing |
| commander_panel | БЛОКИРОВКА | `veh.interlock` | Commander | nothing |
| commander_panel | ЗАЩИТА (protection) | `veh.protection` | Commander | nothing |
| commander_panel | the switch strip's fourth switch (its name is under a hanging tag) | `veh.strip_4` | Commander | nothing |
| driver_panel | small knob under the ammeter (unidentified) | `veh.knob_ammeter` | Driver | nothing |
| driver_panel | the box's knob (unidentified) | `veh.knob_left` | Driver | nothing |
| driver_panel | chrome knob under the tachometer (unidentified) | `veh.knob_tacho` | Driver | nothing |
| driver_panel | lever in the box under the gauges (unidentified) | `veh.lever_box` | Driver | nothing |
| driver_panel | СИГНАЛ (the fourth lighting switch) | `veh.rear_signal` | Driver | nothing |
| fire_valve_panel | bottle valve (red hand wheel) | `fire.valve` | Driver | nothing |
| gyro_panel | БАЛАНС (II) | `ok81.balance_2` | Deputy commander | nothing |
| header_valve_left | valve over the left screen | `veh.header_valve_left` | Driver | nothing |
| header_valve_right | valve over the right screen | `veh.header_valve_right` | Commander | nothing |
| op_panel | СКОРОСТЬ П А | `ro.speed` | Operator | nothing |
| pu303_panel | ХОЛ. ПРОКР. (cold cranking) | `gta.cold_crank` | Driver | nothing |
| pwr_cabinet | ФАЗЫ АС — ВС | `pwr.phase` | Operator | nothing |
| radio_left_panel | R-123M left: КАЛИБРОВКА | `radio.left.calibrate` | Driver | nothing |
| radio_left_panel | R-123M left: КОНТРОЛЬ НАПРЯЖЕНИЙ | `radio.left.volt_check` | Driver | nothing |
| radio_right_panel | R-123M right: КАЛИБРОВКА | `radio.right.calibrate` | Commander | nothing |
| radio_right_panel | R-123M right: КОНТРОЛЬ НАПРЯЖЕНИЙ | `radio.right.volt_check` | Commander | nothing |
| so_panel | БАЛАНС (I) | `msl.balance_1` | Senior operator | nothing |
| stowage_panel | stowage cover (lower) | `veh.stowage_lower` | Driver | nothing |
| stowage_panel | stowage cover (upper) | `veh.stowage_upper` | Driver | nothing |

## Not built (19)

| Item | Where it stands | Notes |
|---|---|---|
| 0062-6M1 switch КАНАЛЫ | on the model, not wired | The noise check is built (КОНТРОЛЬ, ПОДЖИГ ГШ, РЕЖИМЫ, the РРУ КШ knobs: simcore/checks.hpp); КАНАЛЫ, likeliest the large pointer knob, is not a control: РЕЖИМЫ picks the channel (voenizdat_osa_1991 p98, p161). |
| PU-303: ХОЛ. ПРОКР., the hour counters | on the model, not wired | The lamps, the ГТА and ТВГ gauges, the load buttons (ОТКЛ. НАГР. takes the loads off until ГТА or ИВП puts them back: subsystem loads_off) and СТОП (gta_stopped, until ПУСК is put off) work; the cold cranking and the hour counters wait for a model of the turbine start. |
| БОРТ СПК ОТКЛ. as its own push-button | on the model, not wired | Drawn beside the guarded БОРТ СПК ВКЛ.; one switch fo.bort_spk stands for both (voenizdat_osa_1991 p119-121). |
| Fault lamp НЕИСПРАВНОСТЬ | in the sources only: silent | ОТКАЗ СРП, ОТКАЗ Qc and ОБРЫВ ФАЗЫ light with their faults (roadmap C4); НЕИСПРАВНОСТЬ is silent in the sources (voenizdat_osa_1991 p121); the PU-303's fault lamps light on its lamp test. |
| ПАНЕЛЬ ИНДИКАЦИИ: five legend lamps not read, two blank spares | on the model, not wired | Drawn dark (wwp_gecko g095-b): four three-line and one two-line legends not legible; the left column's bottom two blank, under a cool and a reddish lens (spares, SAM3D's reading). |
| De-energisation indicator box | seen in a photo: owner to decide | Stencilled box aft of the spares box (wwp_gecko g090-a, g092-a; rwd_mb3 layout). |
| March signals: flags and lamps | seen in a photo: owner to decide | Two signal flags and a bag by the commander (wwp_gecko g089-d); the march signals (voenizdat_osa_1991 p232-239). |
| Air pressure gauges over the commander's screen and by the right radio | seen in a photo: owner to decide | With pipe and valve (wwp_gecko g087-d, g089-b, g090-a); what they serve is not sourced. |
| 0091-17M: КОНТРОЛЬ НАПР. and its meter | on the model, not wired | The НАКАЛ lamps now light; the SOC's voltage check (voenizdat_osa_1991 p49) and ПЕРЕКЛ. ИСТОЧНИКОВ are drawn in the mesh, not wired. |
| 0005-8M КОРРЕКЦИЯ ОСН. — КОНТР. | in the sources only: owner to decide | The latitude correction to the ГА-3 (voenizdat_osa_1991 p51); МК — ГПК — АК (gyro.compass_mode) and ПОТРЕБИТЕЛИ ОСН. — КОНТР. (gyro.consumers, on the block's last free toggle) are built; no toggle is left on g091-a for this one. Also 0009-5M's ОШИБКА ψ sector narrowing when the antenna rises (p52): its lamp only glows. |
| 0051-7M repetition frequency АВТ., РУЧН. F1-F3 | in the sources only: owner to decide | On the operator's bottom block (voenizdat_osa_1991 p96-101); its nameplate is drawn. |
| 0065-2M АРУ — РРУ and gains | in the sources only: owner to decide | The MTI canceller on SP-1M, under flaps modelled closed (voenizdat_osa_1991 p49-53). |
| Driver's vision: PNV-57E, TNPO-170A | in the sources only: owner to decide | rwd_mb3; g089-a/b show only periscope blanks. The wave shield is built: veh.bilge_pump, ВОДООТРАЖАТЕЛЬНЫЙ ЩИТ (voenizdat_osa_1991 p224). |
| Fuse lamps in 0096-9M1, on SP-1M, 0094-11M; OR-95-1M ПЕРЕГРУЗКА | in the sources only: owner to decide | 0096-10M's four (СОЦ, ССЦ, СВР, СПК) light with their faults (fuse_*, docs/FAULTS.md); 0096-9M1's twins and the racks' own lamps are not drawn: their legends and places are not in the sources (voenizdat_osa_1991 p166). |
| Hand fuel-pump lever on its ratchet quadrant | seen in a photo: owner to decide | Ball-knobbed, low left of the column (wwp_gecko g086-d, g083-a). |
| Differential lock; the fire system on/off | in the sources only: owner to decide | voenizdat_osa_1991 p175, p221. The fire bottles, the automat and the driver's disc are built (roadmap C8). |
| IFF 1L24-2 blocks | in the sources only: owner to decide | On the BM3, not the AK, which carries the РЗМ set (voenizdat_osa_1991 p53-54). |
| OR-14M: timers, ДЕЛИТЕЛИ НАПРЯЖЕНИЯ knobs and toggles | in the sources only: owner to decide | Zeroed for ПУСК ГОТОВ (voenizdat_osa_1991 p154). |
| ПБ-107: its own fan on the engine bulkhead | in the sources only: owner to decide | voenizdat_osa_1991 p168. |
