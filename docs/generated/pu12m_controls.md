# 9S482M PU-12M battery command post: every control and what it does

Written by `tools/button_map.py pu12m` from the console (`data/procedures/pu12m.json`), the cabin (`data/cabins/pu12m.json`) and where else simcore names each control; the same states colour `out/button_map/pu12m.html`. Do not edit by hand.

| State | Meaning | Controls |
|---|---|---|
| wired | drives a subsystem or the launch | 32 |
| model | read by a simulation model | 72 |
| display | sets a screen's picture or what a meter reads | 0 |
| indicator | only lights a lamp or turns on a display | 0 |
| code | named by simcore's source or a system file, not by the console | 8 |
| drill | worked in a drill, lesson or readiness state; changes nothing | 0 |
| inert | nothing names it | 2 |
| **all** | | **114** |

## wired (32): drives a subsystem or the launch

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| driver_panel | G-290B generator (on the move) | `pwr.g290` | Driver-electrician (No. 4) | G-290B generator running (on the move) |
| driver_panel | AZS-50 battery master switch | `pwr.mass` | Driver-electrician (No. 4) | 27 V network (batteries) |
| dtp_panel | DTP: parallax to the reference point set | `dtp.parallax` | Squad leader (No. 1) | The regiment's picture received by telecode |
| fr_panel | ASPD-U: power | `aspd.power` | Battery commander | ASPD-U warmed up |
| fr_panel | ФР block: РАБОТА / КОНТРОЛЬ | `fr.mode` | Battery commander | ASPD-U working (ФР on РАБОТА); ФР at КОНТРОЛЬ: the imitated echo on the tube |
| gak_panel | GAK: enter the measured direction | `gak.enter_heading` | Squad leader (No. 1) | Post oriented (the GAK holds the measured direction) |
| gak_panel | GAK control panel: power | `gak.power` | Squad leader (No. 1) | GAK gyro run up |
| kp4_panel | KP-4: power | `kp4.power` | Squad leader (No. 1) | KP-4 plotting the post's position |
| map_panel | Plotting board: folded / open | `map.board` | Operator-plotter (No. 2) | Plotting board swung open |
| pab2_panel | PAB-2 aiming circle: stowed / set up outside | `nav.pab2` | Squad leader (No. 1) | Direction measured with the PAB-2 |
| pvs1_panel | Headset (шлемофон): radiotelephonist (PVS-1) | `headset.radio_operator` | Radiotelephone operator (No. 3) | Communications ready (the squad leader's check); Voices: the intercom boxes, headsets, sets and telephone |
| pvs2_panel | Headset (шлемофон): squad leader (PVS-2) | `headset.squad_leader` | Squad leader (No. 1) | Communications ready (the squad leader's check); Voices: the intercom boxes, headsets, sets and telephone |
| pvs3_panel | Headset (шлемофон): driver (PVS-3) | `headset.driver` | Driver-electrician (No. 4) | Communications ready (the squad leader's check); Voices: the intercom boxes, headsets, sets and telephone |
| r111_panel | R-111: ИНДИКАТОР | `radio.fug3.indicator` | Radiotelephone operator (No. 3) | R-111 tuned to transmit (НАСТРОЙКА УМ pressed at 20 % on telephony, the cathodes warm); The radio sets worked as sets |
| r111_panel | R-111 (FuG-3): РЕЖ. РАБОТЫ | `radio.fug3.mode` | Radiotelephone operator (No. 3) | R-111 FuG-3 on, its scale settled; R-111 tuned to transmit (НАСТРОЙКА УМ pressed at 20 % on telephony, the cathodes warm); R-111's output valves' cathodes warm; The radio sets worked as sets; Шкала (x100 кГц) |
| r111_panel | R-111: НАСТРОЙКА УМ (button УМ) | `radio.fug3.um` | Radiotelephone operator (No. 3) | R-111 tuned to transmit (НАСТРОЙКА УМ pressed at 20 % on telephony, the cathodes warm); The radio sets worked as sets |
| r111_panel | R-111: ВИД РАБОТЫ | `radio.fug3.work` | Radiotelephone operator (No. 3) | R-111 tuned to transmit (НАСТРОЙКА УМ pressed at 20 % on telephony, the cathodes warm); The radio sets worked as sets |
| r123_fug1_panel | R-123M (FuG-1): режим | `radio.fug1.mode` | Squad leader (No. 1) | R-123M FuG-1 able to transmit (3 min after Д. ПРИЕМ); Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug1_panel | R-123M (FuG-1): ПИТАНИЕ | `radio.fug1.power` | Squad leader (No. 1) | R-123M FuG-1 able to transmit (3 min after Д. ПРИЕМ); R-123M FuG-1 warmed up; Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug2_panel | R-123M (FuG-2): режим | `radio.fug2.mode` | Battery commander | R-123M FuG-2 able to transmit (3 min after Д. ПРИЕМ); Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug2_panel | R-123M (FuG-2): ПИТАНИЕ | `radio.fug2.power` | Battery commander | R-123M FuG-2 able to transmit (3 min after Д. ПРИЕМ); R-123M FuG-2 warmed up; Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug4_panel | R-123M (FuG-4): режим | `radio.fug4.mode` | Radiotelephone operator (No. 3) | R-123M FuG-4 able to transmit (3 min after Д. ПРИЕМ); Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug4_panel | R-123M (FuG-4): ПИТАНИЕ | `radio.fug4.power` | Radiotelephone operator (No. 3) | R-123M FuG-4 able to transmit (3 min after Д. ПРИЕМ); R-123M FuG-4 warmed up (the battery net); Voices: the intercom boxes, headsets, sets and telephone |
| r407_panel | R-407 (FuG-5): ПИТАНИЕ | `radio.fug5.power` | Battery commander | R-407 FuG-5's valves warm (the regiment's telecode); R-407's scales settled (15 min); The radio sets worked as sets; x100 мкА; ПЕРЕДАТЧИК; ПРИЕМНИК |
| rtk1_panel | Headset (шлемофон): battery commander (RTK-1) | `headset.battery_commander` | Battery commander | Communications ready (the squad leader's check); Voices: the intercom boxes, headsets, sets and telephone |
| rtk1_panel | RTK-1 radiotelephone concentrator | `rtk1.power` | Battery commander | RTK-1 concentrator working |
| scr_panel | ŠČR distribution board: 220 V to the equipment | `pwr.equipment` | Radiotelephone operator (No. 3) | 220 V switched to the equipment |
| scv_panel | AB-1 set: on the vehicle / on the ground and cabled | `pwr.ab1_set` | Driver-electrician (No. 4) | AB-1 set on the ground, earthed and cabled |
| scv_panel | AB-1 set: start | `pwr.ab1_start` | Driver-electrician (No. 4) | AB-1 set running (220 V 50 Hz) |
| scv_panel | ŠČV input board: supply | `pwr.input` | Driver-electrician (No. 4) | AB-1 feeding the post; External network feeding the post; G-290B feeding the post |
| vs27_panel | VS-27 rectifier | `pwr.rectifier` | Radiotelephone operator (No. 3) | VS-27 rectifier charging the 27 V network |
| winch_panel | Telescopic mast: lowered / raised and guyed | `mast.raise` | Radiotelephone operator (No. 3) | Mast raised to 16 m and guyed |

## model (72): read by a simulation model

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| dch_panel | ДХ: 0 | `dch.key.0` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: 1 | `dch.key.1` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: 2 | `dch.key.2` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: 3 | `dch.key.3` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: 4 | `dch.key.4` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: 5 | `dch.key.5` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: 6 | `dch.key.6` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: 7 | `dch.key.7` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: 8 | `dch.key.8` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: 9 | `dch.key.9` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: ЦУ (designation) | `dch.key.cu` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: Ч (foe: чужой) | `dch.key.foe` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: СВ (friend: свой) | `dch.key.friend` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: ГР (group) | `dch.key.group` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: Н1 (height zone 1, 0-1.4 km) | `dch.key.h1` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: Н2 (height zone 2, 1.6-3.0 km) | `dch.key.h2` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: Н3 (height zone 3, 3.2-25.2 km) | `dch.key.h3` | Battery commander | The command post: plotting board, telecode, nets |
| dch_panel | ДХ: СБР (reset) | `dch.key.reset` | Battery commander | The command post: plotting board, telecode, nets |
| dt2_panel | DT-2 marker: azimuth | `dt2.azimuth` | Battery commander | The command post: plotting board, telecode, nets |
| dt2_panel | DT-2 marker: range | `dt2.range` | Battery commander | The command post: plotting board, telecode, nets |
| fr_panel | Primary radar source switch | `aspd.radar_source` | Battery commander | The command post: plotting board, telecode, nets |
| fr_panel | ФР block: scale | `fr.scale` | Battery commander | The command post: plotting board, telecode, nets |
| pvs1_panel | ПВС-1: канал | `intercom.pvs1` | Radiotelephone operator (No. 3) | Voices: the intercom boxes, headsets, sets and telephone |
| pvs2_panel | ПВС-2: канал | `intercom.pvs2` | Squad leader (No. 1) | Voices: the intercom boxes, headsets, sets and telephone |
| pvs3_panel | ПВС-3: канал | `intercom.pvs3` | Driver-electrician (No. 4) | Voices: the intercom boxes, headsets, sets and telephone |
| r111_panel | R-111: ВЫЗОВ | `radio.fug3.call` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: УСТАНОВКА ЧАСТОТЫ (scale, MHz on sub-band II) | `radio.fug3.dial` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: УРОВЕНЬ ПР. | `radio.fug3.rx_level` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: НАСТРОЙКА САУ (button САУ) | `radio.fug3.sau` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: ПШ (noise suppressor) | `radio.fug3.squelch` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: ДИАПАЗ. (sub-band, quartz calibrator) | `radio.fug3.subband` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: УРОВЕНЬ ПРД | `radio.fug3.tx_level` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: ЗПЧ 1 | `radio.fug3.zpch_1` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: ЗПЧ 2 | `radio.fug3.zpch_2` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: ЗПЧ 3 | `radio.fug3.zpch_3` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: ЗПЧ 4 | `radio.fug3.zpch_4` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_panel | R-111: ПЛ (ПЛАВНО) | `radio.fug3.zpch_pl` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r111_strip | R-111: ТЛК / ОТКЛ. (under the cap) | `radio.fug3.tlk` | Radiotelephone operator (No. 3) | The radio sets worked as sets |
| r123_fug1_panel | R-123M (FuG-1): ФИКСИР. ЧАСТОТЫ | `radio.fug1.preset` | Squad leader (No. 1) | Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug1_panel | R-123M (FuG-1): ШУМЫ | `radio.fug1.squelch` | Squad leader (No. 1) | Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug1_panel | R-123M (FuG-1): ГРОМКОСТЬ | `radio.fug1.volume` | Squad leader (No. 1) | Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug2_panel | R-123M (FuG-2): ФИКСИР. ЧАСТОТЫ | `radio.fug2.preset` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug2_panel | R-123M (FuG-2): ШУМЫ | `radio.fug2.squelch` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug2_panel | R-123M (FuG-2): ГРОМКОСТЬ | `radio.fug2.volume` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug4_panel | R-123M (FuG-4): ФИКСИР. ЧАСТОТЫ | `radio.fug4.preset` | Radiotelephone operator (No. 3) | Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug4_panel | R-123M (FuG-4): ШУМЫ | `radio.fug4.squelch` | Radiotelephone operator (No. 3) | Voices: the intercom boxes, headsets, sets and telephone |
| r123_fug4_panel | R-123M (FuG-4): ГРОМКОСТЬ | `radio.fug4.volume` | Radiotelephone operator (No. 3) | Voices: the intercom boxes, headsets, sets and telephone |
| r407_panel | R-407: АПЧ | `radio.fug5.apch` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: УЗКАЯ / ШИРОКАЯ | `radio.fug5.band` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: КАЛИБРАТОР – СВЕТ | `radio.fug5.calibrator` | Battery commander | The radio sets worked as sets; ПЕРЕДАТЧИК; ПРИЕМНИК |
| r407_panel | R-407: ВЫЗОВ | `radio.fug5.call` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: the modes switch (the channel's terminals) | `radio.fug5.lines` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: ИЗМЕРИТ. УРОВЕНЬ | `radio.fug5.measure` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: УРОВНИ ИЗМЕРИТ. | `radio.fug5.measure_level` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: the meter's switch | `radio.fug5.meter` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: РОД РАБОТЫ | `radio.fug5.mode` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: ПУ (the handset) | `radio.fug5.pu` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: УСТАНОВКА ЧАСТОТЫ, ПРИЕМНИК (MHz) | `radio.fug5.rx_freq` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: УРОВНИ ПРИЕМ | `radio.fug5.rx_level` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: СТОПОР (receiver) | `radio.fug5.rx_lock` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: УСТАНОВКА ЧАСТОТЫ, ПЕРЕДАТЧИК (MHz) | `radio.fug5.tx_freq` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: УРОВНИ ПЕРЕДАЧА | `radio.fug5.tx_level` | Battery commander | The radio sets worked as sets |
| r407_panel | R-407: СТОПОР (transmitter) | `radio.fug5.tx_lock` | Battery commander | The radio sets worked as sets |
| rtk1_panel | РТК-1: канал (передача) | `rtk1.channel` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| rtk1_panel | РТК-1: ЦИРКУЛЯР | `rtk1.circular` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| rtk1_panel | РТК-1: линия 1 | `rtk1.line1` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| rtk1_panel | РТК-1: линия 2 | `rtk1.line2` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| rtk1_panel | РТК-1: линия 3 | `rtk1.line3` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| rtk1_panel | РТК-1: линия 4 | `rtk1.line4` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| rtk1_panel | РТК-1: линия 5 | `rtk1.line5` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| rtk1_panel | РТК-1: второй приём | `rtk1.monitor` | Battery commander | Voices: the intercom boxes, headsets, sets and telephone |
| sau_panel | R-111 SAU block: antenna distributor ШТЫРЬ / ТЕЛЕСКОП | `radio.fug3.antenna` | Battery commander | The radio sets worked as sets |

## code (8): named by simcore's source or a system file, not by the console

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| pvs1_panel | ПВС-1: вызов РТК | `pvs1.call` | Radiotelephone operator (No. 3) | simcore/include/simcore/scenario_runner.hpp |
| pvs2_panel | ПВС-2: вызов РТК | `pvs2.call` | Squad leader (No. 1) | simcore/include/simcore/scenario_runner.hpp |
| pvs3_panel | ПВС-3: вызов РТК | `pvs3.call` | Driver-electrician (No. 4) | simcore/include/simcore/scenario_runner.hpp |
| rk1_panel | РК-1У: С-23 / АСПД-У | `rk1.code` | Radiotelephone operator (No. 3) | simcore/include/simcore/scenario_runner.hpp |
| rk1_panel | РК-1У: ПРИЕМ | `rk1.rx` | Radiotelephone operator (No. 3) | simcore/include/simcore/scenario_runner.hpp |
| rtk1_panel | РТК-1: вызов ПВС-1 | `rtk1.call_pvs1` | Battery commander | simcore/include/simcore/scenario_runner.hpp |
| rtk1_panel | РТК-1: вызов ПВС-2 | `rtk1.call_pvs2` | Battery commander | simcore/include/simcore/scenario_runner.hpp |
| rtk1_panel | РТК-1: вызов ПВС-3 | `rtk1.call_pvs3` | Battery commander | simcore/include/simcore/scenario_runner.hpp |

## inert (2): nothing names it

| Panel | Control | Id | Seat | Reaches |
|---|---|---|---|---|
| rk1_panel | РК-1У: ПЕРЕДАЧА | `rk1.tx` | Radiotelephone operator (No. 3) | nothing |
| rtk1_panel | РТК-1: ВЫЗОВ (линия) | `rtk1.ring` | Battery commander | nothing |
