# The Osa's radios and intercom: R-123M and R-124 (October 2026)

Why: the vehicle has two R-123M VHF sets (`mon_2112_80` p26, Fig. 3) and an intercom (`rwd_mb3`, listed in the console map as
R-124), and the battery's designations arrive on them. The command network (`simcore/c2_sources.hpp`, roadmap 7.8) already knew
two sets with four presets each; this file records what the devices are and how the sim uses them (`simcore/crew_comms.hpp`).

## What the sources say

| Fact | Source | Tier | Used as |
|---|---|---|---|
| R-123M: FM, 20.00-51.50 MHz in two sub-bands (I 20-35.75, II 35.75-51.5), 25 kHz steps, 1,260 channels, 4 presets plus smooth tuning, 20 W, superheterodyne | `r123_refs` (Wikipedia, greenradio.de), `r123m_training` | 4 | the net frequencies must be on the 25 kHz grid in 20.000-51.500 MHz (`frequency_mhz`) |
| Supply 27 V (+2/-5) from the vehicle's network (BP-26 converter makes the anode voltages); 9.6 A transmitting, 3 A receiving | `r123m_training`, `greenradio.de` | 4 | the sets run from the 27 V network, not the 220 V 400 Hz `power` subsystem; nothing is drawn from a battery model yet |
| Controls by name: ПИТАНИЕ ВКЛ/ВЫКЛ, режим СИМПЛЕКС / Д. ПРИЕМ (duty reception), ШУМЫ (noise suppressor), ГРОМКОСТЬ, ШКАЛА (dial lamp), КОНТРОЛЬ НАПРЯЖЕНИЙ (РАБОТА 1), ФИКСИР. ЧАСТОТЫ 1-4 / ПЛАВНЫЙ ПОДДИАПАЗОН I-II, УСТАНОВКА ЧАСТОТЫ, НАСТРОЙКА АНТЕННЫ (red knob, locked after tuning), ПОДДИАПАЗОН I-II, ТОН. ВЫЗОВ, fixators 1-4 under a cover (key) | `r123m_training`; panel photograph `r123_commons` | 4 | the console's `radio.*` controls |
| Warm-up 10 min before calling; from Д. ПРИЕМ back to СИМПЛЕКС wait 3 min for the transmitter valves | `r123m_training` | 4 | subsystems `r123m_left/right` (600 s) and `r123m_*_tx` (180 s) |
| Range on a medium-rough route at up to 40 km/h: 4 m whip at least 20 km with the suppressor off, up to 13 km with it on; 1 m whip up to 8 km; emergency wire (3 m, thrown out of the commander's hatch) up to 4 km. Other sources: 14 km same type (ru.wikipedia), 20 km (en.wikipedia), 15 km and 12 km with the radars on (`mon_2112_80`, already fitted in the EM layer) | `r123m_training`, `r124_refs`, `mon_2112_80` | 1-4 | `comms.ranges_km`, as relative losses on the EM link (40 log10 of the range ratio) |
| Smooth tuning only on a halt: the unlocked frequency drifts with vibration | `r123m_training` | 4 | not modelled |
| R-124: five subscriber boxes A-1 to A-5 with helmets and chest switches (ПРД/приём); intercom among five, each hears himself; A-1 (commander) and A-2 can go on the radio; 27 V, 0.45 A; 90 % word intelligibility at 120 dB noise | `r124_refs` (ru.wikipedia), `r123m_training` | 4 | the console's `intercom.*` controls |
| A-1 mode switch ВС (intercom) / Р-123 (radio) / ВЫКЛ; A-2 ВС / Р-123; the driver's box ПВ ВС / РС; the one on the radio leaves the intercom while the rest stay on it; the chest switch changes between the two | `r123m_training` | 4 | `CrewComms` |
| The Osa carries two R-123M and an R-124 intercom (rwd_mb3), TA-57 field telephone | `mon_2112_80` p26, `rwd_mb3`, `docs/research/OSA_CONSOLE_MAP_2026-10.md` | 1-2 | which subscribers have which box is SAM3D's choice |

The R-124's use on the Osa is only in `rwd_mb3` (a photo of the vehicle's equipment list): no Osa document found describes its
subscribers. The student texts describe the R-123M and R-124 on tanks and the ZSU-23-4 (`kaznu.kz`, `studfile`); the sets and
the intercom are the same family.

## Photograph references used (not stored in the repository)

* An R-123 front panel in a museum, [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Funkstation_R-123_IMG_1390.JPG), 3000 x 2250 px: top row left to right ШУМЫ, СИМПЛЕКС / ДУПЛЕКС / Д. ПРИЕМ selector, the ЧАСТОТА x 100 (кГц) window, НАСТРОЙКА АНТЕННЫ (red), four preset fixators under a hinged cover; below them КОНТРОЛЬ НАПРЯЖЕНИЙ, УСТАНОВКА ЧАСТОТЫ, the round preset drum with its cap, the big ФИКСИР. 1-4 / ПЛАВНЫЙ ПОДДИАПАЗОН selector, ГРОМКОСТЬ, the ПОДДИАПАЗОН I-II windows. This is the older R-123; the R-123M's labels are the same family.
* An R-123M in a vehicle with its BP-26 power block above it, [qrz.ru](https://www.qrz.ru/catalog/detail/r-123m_906) (640 x 480).

## What the sim does with them

* Each set hears only when it is switched on, warmed up and turned up; `CrewComms::Receiving` tells the C2 network
  (`SetSetReceiving`). A set that does not hear takes the commander off the net ("cannot hear the net: R-123M left is switched
  off"); the source on it goes silent to him and the NPC commander falls back to the other set's source, as before.
* Squelch and antenna become a receiving-chain loss on the EM link (`em::NodeState::rx_extra_loss_db`): 7.5 dB for the
  suppressor, 15.9 dB for the 1 m whip, 28 dB for the emergency wire. With the EM layer's radio sources a shorter antenna or the
  suppressor shortens the net (test: an EWR 8 km away is p = 1.0 on the 4 m whip, 0.96 with the suppressor set, 0.28 on the 1 m
  whip, below 0.01 on the wire). The loss of the set the headset is on applies to all the radio sources (a simplification).
* The commander's A-1: on Р-123 he hears the net; on ВС he does not (the net goes by unheard, what was said is lost); off, he
  shouts. A designation he passes on takes the shouting time longer (`PlottingBoard::SetPassOnExtra`), and his orders to the
  crew are delayed (`ScenarioRunner::GiveOrder`; the scenario's triggers and Unreal's order menu both go through it). The
  deputy's A-2 on Р-123 and the driver's ПВ on РС each leave the intercom, so an order to them is shouted too. Shouting takes 1.5 s,
  4 s while the turbine or the take-off generator runs (estimates).
* Net frequencies (`c2.sources[].frequency_mhz`) go to the EM net, so the frequency plan is the scenario's; a source without one
  stays at the middle of the band as before.
* Procedures `radio_left_on` (driver) and `radio_right_on` (commander, with A-1 to Р-123) let NPC crew do it. The vehicle starts with both sets on and warm (`start_ready`): the 10 min applies when a crew switches one
  off and on.

## Not done

* Transmitting: the sets only receive. The commander's reports, ТОН. ВЫЗОВ and the chest switch's transmit position need a
  sender in the sim (the transmit state exists).
* The set-up drill for the presets, smooth tuning, the fixators (the preset frequencies are the scenario's).
* Audio in Unreal, and the 27 V battery (a set draws 3 A receiving, 9.6 A transmitting; the battery's capacity and the
  generators' charging are not modelled).
* Which crew member has which R-124 box beyond A-1 (commander), A-2 (deputy commander) and ПВ (driver).
