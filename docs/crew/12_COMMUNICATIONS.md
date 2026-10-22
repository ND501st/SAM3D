# 12 — Communications: headsets, intercom, radios, telephones and nets (roadmap 8.13)

The plan and the design for the crews' communications in the PU-12M battery command post and the Osa-AK launcher. The
research is `docs/research/CREW_COMMS_2026-10.md` (deep research of 20 Oct 2026). The Osa's crew drill comes from
`voenizdat_osa_1991` pp. 244-252, read under the owner's exception of 3 Oct 2026. Both rest on the crew pillars
(`README.md`): knowledge moves only through instruments and voice, every action has an owner seat, and the player's
character knows only what reaches him (`docs/command/`).

**The one rule that changes the game:** a crewman hears the intercom, a radio net or a telephone line only through a
headset he is wearing, plugged into a box that is switched to it. Without the headset he hears only voices in the cabin.
The PU-12M has no loudspeaker (`flak11` signals schematic). What a crewman knows therefore depends on how his box is set.

## 1. What the sources settle

| Fact | Source | Tier |
|---|---|---|
| PU-12M intercom: RTK-1 radiotelephone concentrator (the commander's), PVS-1/2/3 boxes, RK-1U hub, two RK-3U; a tank headset at each of the four stations; no loudspeaker | `flak11` (PU12-1, the NVA 1984 signals schematic), `rwd_mb3` | 1 |
| RTK-1: intercom, transmits on four radios, receives on two, five telephone lines with lamps and levers, calls to PVS-1/2/3 and ЦИРКУЛЯР (all), an acoustic signal | `rwd_mb3` (catalogue text; panel photographs, labels medium confidence) | 1 |
| PVS: intercom and the same four radios, РСТ-1..4 lamps, a selector, call buttons for the other three stations | `rwd_mb3` | 1 |
| RK-1U: ПЕРЕДАЧА and ПРИЕМ rotaries, ВЫКЛ / Р-123 / Р-111 / Р-407 / ПРОВОД, and С-23 / АСПД-У toggles: the data path | `rwd_mb3` (photograph) | 1 |
| Boxes by seat: PVS-2 squad leader (front right), PVS-3 driver (front left), PVS-1 radiotelephonist (rear left, with FuG-3 R-111, FuG-4 and RK-1U), RTK-1 battery commander (with FuG-2 and FuG-5) | `flak11` (1990 layout drawing), `rwd_mb3` captions | 1 |
| **Correction:** FuG-2 (the covered unit's net) stands beside the commander, FuG-4 (the battery net) at the radiotelephonist's table: SAM3D had them the other way round | same | 1 |
| The operator-plotter has no box or headset in any source; he "takes the signals and reports on targets from the radiotelephonist and plots them", by no stated means. **SAM3D:** a second headset on PVS-1 (the reading of `docs/research/PLOTTING_BOARD_2026-10.md`: voice nets put plotters on headphones; FuG-1 is named for the plotters) | `rwd_mb3`, `pvo_bu_x_1984` art. 23, `flak11` | 1 / SAM3D |
| The radiotelephonist takes and writes into the journal every radiogram, signal, report and designation with the time he took it, and reports them to the commander | `pvo_bu_x_1984` art. 24; `pvo_bu_ix_1984` App. 7 (the columns) | 1 |
| Nets: FuG-1 air picture (designation net no. 2, the plotters), FuG-2 covered unit, FuG-3 R-111 the SAM regiment's command net, FuG-4 the battery's command net, FuG-5 R-407 the automated fire control (telecode); the ČSLA reads FuG-3/5 the other way (recorded, not used) | `flak11` | 1 |
| FuG-2 (20-27 MHz) and FuG-4 (37-51.5 MHz) share one antenna through a diplexer | `flak11` schematic | 1 |
| Every net has a main station (the senior commander's); call, answer, confirm; no answer in 3 min: repeat, three times, then report; checks and passwords only on order | `rrs_1986` | 1 |
| Commands acknowledged at once by exact repetition or «Понял»; circular commands said twice with no receipt; signals and warnings go first | `rrs_1986` | 1 |
| Scheduled frequency changes at the set time with no word on air; unscheduled only on the main station's order «Переходите на запасную частоту», read back, all retune at once and wait for its call under new call signs; 1-2 min no call: call the main station; 5 min: try both frequencies; 10 min: report loss of communication | `rrs_1986` | 1 |
| Duty reception forbids transmitting, tuning with radiation and changing frequency (except scheduled retuning without radiating) | `rrs_1986` | 1 |
| R-123M: warm 10 min before calling; from Д. ПРИЁМ back to СИМПЛЕКС 3 min for the transmitter valves; four fixed frequencies, a change in 15 s or less; transmit at most 10 min at 3:1 | `r123m_training`, `sfu_tactics_2011` | 4 |
| Radio silence: a ban on transmitting; air defence and the warning system exempt; the battery commander sets emission by readiness level | `vst_1988`, `pvo_bu_ix_1984` | 1 |
| Air data, designations and warnings may go in clear; units, officials and places by call sign or code; target positions by the PVO grid | `pvo_bu_x_1984` art. 30, `pvo_bu_ix_1984` | 1 |
| Jamming: train to recognise it, report its effect and the measures promptly; log it; the main station orders the spare; commands may be said twice; relay; a station with two receivers watches the spare without orders; switching a station off to hide the jamming's effect is forbidden | `pvo_bu_ix_1984`, `rrs_1986` | 1 |
| Noise jamming sounds like the receiver's own noise, or the VHF receiver's noise disappears (captured); US divisional jammers (MLQ-34 20-150 MHz, three nets each) aimed at troop air defence first; 155 mm expendable VHF jammers, minutes, to 20 km | `palii_1989` | 1 |
| Osa: two R-123M for external communication, the R-124 for the crew, a TA-57 for a subscriber outside at a halt; R-123M range 12 km with the radars transmitting, 15 km with them off; no data link | `mon_2112_80` | 1 |
| R-124: boxes A-1..A-5; only A-1 (commander) and A-2 reach a radio; chest switch ПРМ / ПРД / ВЫЗ; ВЫЗ pulls those on the radio back to the intercom; the one on the radio leaves the intercom | `r124_refs`, `r124_bmp2`, `r123m_training` | 4 |
| At «К бою» the crew chief switches on and tunes the R-123M sets; the deputy and the operators check the intercom (ТПУ) and report «Первый (второй, третий) готов»; the crew chief reports to the battery command post «Такой-то к бою готов, столько-то каналов, столько-то ракет» | `voenizdat_osa_1991` pp. 246-247 | 1 (owner's exception) |
| The battery commander assesses readiness from the crew chiefs' reports, the squad leader's report and a check of the means of control and communication | `voenizdat_osa_1991` p. 251 | 1 (owner's exception) |
| The R-123M frequencies are chosen for jam resistance and radio masking; launchers at least 200 m apart | `voenizdat_osa_1991` p. 208, `mon_2112_80` | 1 |
| Designations to the launchers by voice, bearing and range, errors up to 5° and 3 km; launchers report lock-on; sudden targets engaged independently, reported afterwards | `flak11`, `wrk`, `pvo_bu_ix_1984` | 1 |
| «ПТ-1», «ПТ-2»: no device of that name in any PU-12, Osa, 9S80 or R-145BM source. In Soviet signals use «ПТ-<n>» is a **переговорная таблица** (talk table) | `fsrbit_pt`, `bstudy_tactics` | 4 |

Azimuth units: the PVO report form and a PU-12 veteran use **degrees** for the battery's designations; the Osa textbook
writes angles inside the vehicle on the 60-00 circle. SAM3D: degrees on the battery net, thousandths on the Osa's own
intercom (as now). Both recorded.

## 2. The model

### 2.1 Headsets

Each seat has a headset (шлемофон), a control `headset.<station>`: СНЯТ / НАДЕТ. НАДЕТ means on the head and plugged
into the seat's box (the cord is part of putting it on, about 3 s, an estimate). An NPC puts his on as the first step of
deploying or «К бою»; a player puts his on with J or by clicking it. Without it the seat hears only the cabin
(`docs/crew/04` shouting times) and cannot use a radio, the intercom or a line.

### 2.2 Boxes and what they route

| Vehicle | Box | Seat | Positions | Hears | Talks on |
|---|---|---|---|---|---|
| Osa | R-124 A-1 (`intercom.a1`) | commander | ВЫКЛ / ВС / Р-123 (`radio.headset` picks the set) | ВС: intercom; Р-123: that set, not the intercom | the same |
| Osa | A-2 (`intercom.a2`) | deputy commander | ВС / Р-123 (the left set) | as A-1 | the same |
| Osa | A-3, A-4 | senior operator, operator | (none) | intercom | intercom |
| Osa | ПВ (`intercom.pv`) | driver | ВС / РС (the left set) | as A-1 | the same |
| PU-12M | RTK-1 (`rtk1.channel`, `rtk1.monitor`) | battery commander | talk: ВС / РСТ-1..4; monitor: ВЫКЛ / РСТ-1..4; five line levers | the intercom always, the talk set and the monitored set, a raised line | the talk selection, or a raised line |
| PVS-1 (`intercom.pvs1`) | | radiotelephonist | ВС / РСТ-1..4 | the intercom and the selected set | the selection |
| PVS-2 (`intercom.pvs2`) | | squad leader | as PVS-1 | | |
| PVS-3 (`intercom.pvs3`) | | driver | as PVS-1 | | |

РСТ-n is FuG-n (the NVA numbering; inference, medium confidence). A PVS or RTK box hears the intercom whatever its
selector (one shared intercom net, the R-145BM analogue; inference). An R-124 box on the radio does not (sourced). Calls:
the R-124's ВЫЗ and the RTK-1's ЦИРКУЛЯР reach every box; the PVS/RTK call buttons light the called box's lamp and sound
its tone. The R-124 is powered while A-1 is not at ВЫКЛ; the PVS family while the RTK-1 is on.

### 2.3 Sets

The PU-12M's three R-123M become sets like the Osa's: ПИТАНИЕ, mode Д. ПРИЁМ / СИМПЛЕКС, preset ФИКС. 1-4, ГРОМКОСТЬ,
ШУМЫ. Warm 10 min, transmit 3 min after leaving Д. ПРИЁМ, a preset change 15 s (`sfu_tactics_2011`: 15 s or less). The R-111
and R-407 stay as `radio_sets.hpp` has them. A set hears a net when it is on, warm, turned up, on the net's present
frequency and within reach, and the net is not jammed out at it. It transmits when it also is on СИМПЛЕКС (R-111: tuned,
power on) and the 3 min have passed.

### 2.4 Nets and radio data

The scenario's `radio_data` block is the battery's radio data: for each net its set, its main station, the call signs,
its frequencies by conditional number (working, spare) and the times of scheduled changes. The presets hold the working
and spare frequencies of the set's net (inference). Defaults for the PU-12M:

| Net | Set | Main station | Traffic |
|---|---|---|---|
| `air_picture` | FuG-1 | the regiment's radar battery | air-situation reports (the plotters' net) |
| `covered` | FuG-2 | the covered unit's air-defence chief | warnings to the covered unit; its requests |
| `regiment` | FuG-3 R-111 | the regiment's command post | orders in talk-table code, readiness, reports up |
| `battery` | FuG-4 | **this PU-12M** | designations, orders, the launchers' reports |
| telecode | FuG-5 R-407 | the regiment | the ASPD-U picture and two-digit commands |

### 2.5 Who listens to what (the watch)

| Seat | Watch (SAM3D's arrangement: the boxes' places and the regulation's division of duties) |
|---|---|
| Battery commander | RTK-1 talk РСТ-4 (the battery net), monitor РСТ-3 (the regiment's net) |
| Radiotelephonist | PVS-1 on РСТ-1 (the air picture): reads each report out to the plotter (`pvo_bu_x_1984` art. 23) |
| Squad leader | PVS-2 on РСТ-2 (the covered unit) |
| Operator-plotter | no box: hears the radiotelephonist across the cabin and writes the board |
| Driver | PVS-3 on ВС |
| Osa commander | A-1 on Р-123, the right set on the battery net (preset 1 working, 2 spare) |
| Osa deputy, operators, driver | ВС |

An NPC who must talk on another channel turns his box to it (1 s), talks and turns it back.

### 2.6 What gates what

Every producer asks the router who hears it, so nothing reaches anyone without a path:
* the air-picture reports reach the plotter's queue only through a seat hearing FuG-1 who reads them out;
* the regiment's voice orders count only when someone hears FuG-3 (and are understood once read against ПТ-1);
* the telecode reaches the ASPD-U only through the RK-1U's ПРИЕМ on Р-407 (or ПРОВОД, a telecode line laid: the scenario's
  `post.regiment.telecode_wire`); the post's own telecode goes out only through ПЕРЕДАЧА on Р-407 (the set reaching the
  regiment's receiver in duplex) or ПРОВОД, with the DTP set (a transmitting post, `telecode_sending`);
* a designation goes out only from a commander whose RTK-1 talks on РСТ-4 on a set that can transmit; the launchers'
  reports reach the IT boards only when someone hears FuG-4;
* in the Osa, the commander hears the battery net only on A-1 Р-123; he reads a designation back on the net and passes
  it to the crew on ВС;
* intercom orders and reports reach a seat only through its headset; otherwise they are shouted (the existing times).

### 2.7 Frequency changes, jamming and emission control

* **Scheduled changes** (`radio_data.nets[].changes`): every station turns its preset at the time, with no word on air.
* **Jamming** (the scenario's `jamming` list: nets or a band, from-to, noise or spot, weak or strong, a spot jammer's time
  to find the new frequency): strong jamming loses every message at a receiver; weak loses each with a chance
  (`data/gameplay/comms.json`), and commands are said twice. A listener recognises it after his skill's perception time and
  says «Помехи на …»; it is logged and reported.
* **The spare-frequency drill**: the main station orders «Переходите на запасную частоту» (circular, said twice); the
  members read back, turn their sets to the spare, and wait for its call. A member who missed the order calls the main
  station after 1-2 min, tries both frequencies after 5 min, and reports loss of communication after 10 min
  (`rrs_1986`). On the battery net the PU-12M is the main station: the commander orders it (an NPC on recognising the
  jamming; a player from the orders menu). On the regiment's nets the regiment orders it (scripted) and the post's crew
  must follow. A launcher off the net fights on its own sensors.
* **Emission control** (the regiment's order by ПТ-1, or the battery commander's for his own net): the sets go to duty
  reception (R-123M Д. ПРИЁМ, R-111 ДЕЖ. ПР.). Air data, designations and warnings are exempt (`vst_1988`), so the
  commander may still designate, but a launcher on Д. ПРИЁМ cannot read back and needs 3 min to transmit again: lifting
  silence costs time. Any other transmission under silence is a violation in the debrief. Time on air adds up per net; past
  the gameplay threshold the enemy is taken to have fixed the net and a spot jammer comes on it (`palii_1989`, inference;
  `data/gameplay/comms.json`).
* **The R-407's АПЧ off in interference** and **the R-111's ЗАЩИТА ВХОДА** stay as `radio_sets.hpp` has them.
* **On the march** (`voenizdat_osa_1991` pp. 232-239): the battery commander, at the column's head in the PU-12M,
  signals by flags and lamps; radio is forbidden but for fire control while the battery repels an attack. The battery's
  march orders reach the column as flag (or lamp) signals seen by the drivers and commanders; the net orders and the
  report to the regiment are refused on the march; the warning to the covered troops stays open talk. A hostile within the
  column's stop-and-fight range (`short_halt_in_column_m`) or an engagement opens the radio (`MarchRadioBan`).

### 2.8 Telephones

The RTK-1 takes five lines (`rtk1.line1..5`: down / up). The scenario's `lines` say which are laid and to whom (a launcher's
TA-57, the coupled radar, the regiment's post); laying four 500 m lines adds 7 min to the deployment (`ru_pu12_text`: 25 min
against 18). A call rings the far end (the RTK-1's acoustic signal and the line's lamp; a TA-57's bell). It is answered by
raising the lever (RTK-1) or taking the handset (`ta57.handset`, the Osa's at a halt). A line carries voice with no
emission and no jamming: the commander may designate or check by line, as a veteran describes the night checks
(`airbase_ek32_2012`).

The coupled radar's line is the service line in its cable (the *Dienstverbindung* of the 1984 GDR handbook's ASPD figure,
`flak11` PU12-0). Its crew ring the post when their radar goes off the air (the scenario's `post.radar.outages`: the ARM
drill, a fault) and when it is back («Станция выключена. Работу прекратил.», «Станция включена, работаю.»). Without the
line laid the post learns it only from its tube going empty (`ScenarioRunner::UpdateRadarLine`).

### 2.9 Talk tables (ПТ-1, ПТ-2)

`data/command/talk_tables.json`: **ПТ-1**, the talk table of the regiment's command net (three-digit groups for readiness
1-4, fire ban, fire free, cease fire, radio silence, lift silence, the spare frequency, report readiness), and **ПТ-2**, the
battery's signal table («Воздух», «Отбой», the warnings). The groups are placeholders in the form of the period's tables
(`bstudy_tactics`, `sfu_tactics_2011`: post-Soviet samples, which disagree). The regiment's voice orders arrive as groups
(«Рябина, я Тополь: 143. Приём.»). An NPC who heard one decodes it and says what it means on the intercom. A player reads the
table on his desk (a document in the cabin). The ASPD-U's two-digit telecode commands stay as they are
(`data/command/aspd_codes.json`).

**The papers in the cabin** (`ScenarioRunner::DocumentText`, drawn on lit, matt paper by `M_CabinPaper`, not the tubes'
glowing material): the radiotelephonist's **radio data** on their form (`rrs_1986`, `vst_1988`): each net's frequencies by
number with the decimal comma, the call signs (his own and the main station's; on the battery net the launchers'), the
scheduled changes as Ч+minutes (the scenarios keep no clock), the mode (telephony), the azimuth to the regiment's post;
then the R-407's telecode channel and the RTK-1's lines. The squad leader hands the radiotelephonists their data
(`pvo_bu_ix_1984`), so the extract lies on the radiotelephonist's table. The commander's **talk tables**: ПТ-1 as groups and
meanings, ПТ-2 as the signal table, each signal with its visible sign (`bstudy_tactics`: «Воздух» a green flare, «Отбой» a
white one). Everything on them is in Russian, as issued; passwords and the talk tables' keys are not modelled.

### 2.9a The radiotelephonist's journal

`simcore/radio_journal.hpp`: every message the post's radios carry that the radiotelephonist hears (the air picture,
the regiment's net, the battery net, the covered unit's) waits in his head until he writes it into the journal open on
his table (`pvo_bu_x_1984` art. 24, `pvo_bu_ix_1984` App. 7: the time, the net, who, what). Writing one takes
`journal_write_s` at his speed, one at a time, now and then a digit wrong (his tier's `slip_chance`); one not written
within `journal_remember_s` is lost (the debrief logs it). A player takes it down as **dictation**: a click on the page
holds it before his eyes (the camera locked on it); the oldest message he heard stands on its line with its words
printed and a box for each group of digits, which he fills from memory (digits, Backspace, Tab, Enter; S the short form,
the number and the time only, `rrs_1986` art. 154; Esc puts the page down). What he writes is what the page says. The
page is the cabin's document `radio_journal`, the only record: the screen's radio lines are a subtitle of what is being
said and fade in seconds; anyone may click the page to read it.

The plotter goes to the page only for a report he missed: one that came while PVS-1 was off FuG-1, or (an NPC) one his
tier's `miss_chance` let slip. He plots it as the page has it (`CommandPost::FromWords`): a wrong square digit puts the
mark in the wrong square, a wrong number makes a second track under that number with no aircraft behind it, a short
entry or an empty box can't be plotted at all. A report read to him from the page carries the same errors. A player
plotter's mouse wheel offers the page's numbers not yet on the board. The debrief sets the journal against what was said
(digits as said, entries not as said, lost) and counts the marks plotted from the page and those it had wrong.

**Beyond the fight** (project owner, 22 Oct 2026; `docs/command/01_PRINCIPLES.md` 9). The journal is the battery's record
of the fight, and the after-action report is to be compiled from it, not from the truth (`docs/command/07_IMPLEMENTATION_PLAN.md`
P9): the raids' times, routes and sizes as written, the ones never written missing. The regiment's and the battery's
analysis and planning are built on those reports, so the radiotelephonist's accuracy carries into the next raid: the
corridors the analysts name, the ambush positions, the emission policy. It is critical to the after-action gameplay
loop, and the regiment's checks of the record feed the crew's rating.

### 2.10 The plotter's voice

The operator-plotter's headset is a second one on PVS-1 (`second_station` on the box): he hears FuG-1 while the
radiotelephonist's box is on it, and the intercom. Turned to the R-111 for an order, PVS-1 takes the picture from him
until it is turned back; meanwhile the radiotelephonist writes what he hears, and reads a written report to the plotter
from the page (`air_report`, the fallback). He reports (`pvo_bu_x_1984` art. 23)
each new target as he writes it (number, bearing and range from the post, height, identity, size), a turn, and a target
lost; the commander decides (art. 17). An NPC plotter says each at once; a player's wait in his reports menu, filled in,
and go stale after `plot_stale_s`. The NPC commander knows a board target from the plotter's call, or only on his next look
over the board (`notice_board_s`). Details: `docs/research/PLOTTING_BOARD_2026-10.md`.

## 3. The crews' loops, interconnected

**PU-12M deployment.**
1. All five put their headsets on.
2. The driver gives power: the AB-1 on line.
3. The radiotelephonist raises the mast and switches the sets on (10 min warm).
4. The squad leader orients the post and reports «Связь готова» when the sets are warm and on their nets.
5. The radiotelephonist sets the RK-1U to Р-407 and the ASPD-U and his PVS-1 to the air picture; the commander
   switches the ASPD-U on, puts the RTK-1 to its watch (talk РСТ-4, second receiver РСТ-3), and reports the battery
   ready to the regiment on РСТ-3 «Рябина к бою готова».

**PU-12M in battle.**
* The radiotelephonist hears the air picture and reads it to the plotter.
* The plotter writes the board, and the commander reads it.
* The plotter calls each new target, a turn and a lost target to the commander aloud (§2.10).
* The commander hears the regiment on РСТ-3 and decodes its orders with ПТ-1 (a crewman whose box is on the net
  too reads the group with him and says what it means; nobody else decodes it for him).
* He designates on РСТ-4 and hears the read-backs and reports; the squad leader keys the reports into the IT-1.
* The squad leader warns the covered unit on РСТ-2 on the commander's order.
* Under jamming: whoever hears it says so, the commander orders the battery net to the spare and the radiotelephonist
  turns the FuG-4 («Батарейную — на запасную!»). When the regiment moves its net, the commander, who hears it on his
  second receiver, tells the radiotelephonist «Р-111 — на запасную, ЗПЧ два!» (the FuG-1's change goes to the squad
  leader the same way); an NPC who hears the order on his own box acts on it himself.
* Under silence: the sets go to duty reception, and the commander plans the 3 min back.

**Osa «К бою»** (`voenizdat_osa_1991` pp. 246-247).
1. Headsets on.
2. The commander switches on and tunes the R-123M sets.
3. The deputy and the operators check the intercom and report «Первый (второй, третий) готов».
4. When ready, the commander reports to the post on the battery net «21 к бою готов, два канала, шесть ракет».

**Osa designation.**
1. The commander hears it on Р-123 and reads it back.
2. He turns A-1 to ВС, gives it to the senior operator, and turns back.
3. The battery commander hears the read-back. A designation without a read-back is said again.

**A target in a group** (`voenizdat_osa_1991` p261; `simcore/group_designation.hpp`). When the post's picture can't tell
the group's aircraft apart (its marks closer together than `group.post_unresolved_m`), the battery commander gives the
group's centre and a selection rule instead of a target:
* PU-12M, `order_assign`: «21, я 10: группа 6201, центр, азимут 015, дальность 24, правило 2 — уничтожить! Приём.» (a
  single target as before: «…: цель 6201, азимут …»). The NPC gives each launcher on the group the next rule (1-4, a
  launcher per aircraft); the player's designation does the same unless he names a rule (0: none, the crew chief chooses).
* Osa: the commander reads it back and passes it on, `designation_group`: «Группа номер 6201, центр: азимут …, дальность
  …, высота …, правило 2!». The senior operator counts the group on his scope (the first to fourth from the left of the
  nearest targets; on one azimuth by range) and reports the one the rule picks, `group_pick`: «По правилу 2: цель, азимут
  …, дальность …!». The commander takes it (counting on what he knows himself if the report doesn't come in 15 s).

## 4. Unreal

* The cabins get the controls.
  * PU-12M: the RTK-1's talk and monitor selectors, ЦИРКУЛЯР, call buttons, five line levers and lamps, the PVS
    selectors and call buttons, the RK-1U's ПРИЕМ / ПЕРЕДАЧА and its АСПД-У toggle, the R-123M fronts (as the Osa's), the
    headsets, the TA-57s.
  * Osa: the headsets and the TA-57.
  * The talk tables are documents on the desks (a display kind `talk_table`).
  * FuG-2 and FuG-4 swap places.
* The radio log shows only what the player's seat heard: its headset, its box, its sets. Lines keep their net's colour;
  cabin voices are white. Without a headset, the HUD says so and shows the cabin only.
* A small comms strip at the bottom left shows the player's own box: the headset, the selector, the РСТ lamps lit by
  carriers, and a ringing line. It shows only what is in front of him.
* Keys: J puts the headset on or off; K and Shift+K turn the box's selector; Y answers a ringing line or telephone.
* The voice plays only what the seat hears: radio through the radio voice, lines through a telephone voice, the cabin
  plain. A ringing line sounds its signal.
* Multiplayer: each seat's hearing goes with the post feed and the view extras.

## 5. Tutorials

Built (20 Oct 2026; `docs/TUTORIALS.md`). Every seat lesson but the plotter's starts with the headset off (the lesson
scenario's `own_unit.controls`) and puts it on first. The lessons cue the nets with `net_order:<net>`, `jam:<net>`,
`line_call:<n>` and `regiment:<group or code>` (a regiment's command held `"on_cue"` in `post.regiment.commands`), and wait
on them with `group:<code>` and `given_target` (`simcore/tutorial.hpp`); what follows is the crew's real answer.

* **Radiotelephonist**: the PVS-1, the R-123M's presets, the RK-1U's data path (the lesson starts it on ВЫКЛ / С-23),
  the radio data; the PVS-1 to РСТ-1 in the deploy drill; reading the radar company's report out to the plotter; the
  regiment moving its net (the commander, who hears it on his second receiver, says «Р-111 — на запасную, ЗПЧ два!»);
  the battery net jammed and the FuG-4 to its spare on the commander's order.
* **Battery commander**: the RTK-1 (talk on РСТ-4, the regiment on its second receiver, the lines and calls); the talk
  tables (a tour); the regiment's ПТ-1 group in his second receiver («Рябина, я Тополь: 707. Приём.»), decoded by hand
  against the table on his desk (a `"decode"` step: nothing marks the answer, nobody else is on the net to decode it for
  him) and answered with «Доложить в полк о готовности!» (the radiotelephonist reports on РСТ-3); designating on the
  battery net with call signs and hearing the read-back; ordering the battery net to the spare under jamming; answering
  line 5.
* **Squad leader**: the PVS-2 to the covered regiment's net in the deploy drill; the radio data read (on the
  radiotelephonist's table), then the check of communications against it and «Связь готова!».
* **Plotter**: no headset; the radiotelephonist reads him the reports.
* **Driver**: the headset and the PVS-3.
* **Each Osa seat's lesson**: the headset first; the deputy and the operators check in on the intercom right after
  «К бою!» («Первый (второй, третий) готов!»).
* **Osa commander**: the A-1 starts on ВС; after «К бою!» his own part: the headset switch to the right set (the battery
  net) and the A-1 to Р-123; «21 к бою готов, каналы 2, ракет 6!» to the post; the battery's PU-12M (his lesson's
  scenario now has it, as `c2` primary) gives him the drone on the battery net and he reads it back
  («10, я 21: цель …, понял. Приём.»); the A-1 to ВС to give the crew «Цель номер … — взять!», and back to Р-123. The
  crew menu lists his readiness report and read-back after his orders (`ScenarioRunner::PlayerRadioReports`).

Not yet: a crewman whose box is on the regiment's net decoding a group for a player commander in a lesson (the NPC path
exists: `DecodeHeard`); the Osa commander's lesson on the spare frequency (the post's order «Переходите на запасную
частоту» heard and the set turned).

## 6. Phases

| Phase | What | Done when |
|---|---|---|
| C1 | Research note, sources, corrections (FuG-2/FuG-4, the R-124 out of the PU-12M, degrees on the battery net), talk tables, gameplay numbers | citations check clean |
| C2 | `simcore/voice_comms.hpp`: headsets, boxes, routing; the callout bus on channels; hearing gated; orders shouted without headsets | tests: a seat without a headset hears nothing on the intercom; A-1 on Р-123 misses the crew |
| C3 | The PU-12M's R-123M sets; radio data and nets; scheduled changes; jamming; the spare drill with its clock; emission control; lines and telephones | tests for each |
| C4 | The PU-12M's producers gated: the air-picture relay, the regiment's coded orders, the battery net both ways, the covered unit, the RK-1U data path | the deployed post's first mark comes through the radiotelephonist; a designation needs РСТ-4 |
| C5 | The Osa: headsets, the intercom check, the readiness report, the read-back, Д. ПРИЁМ / СИМПЛЕКС, the spare on order, the TA-57 | tests |
| C6 | The crews' data: procedures, tasks, orders, callouts, reports for both; the drill master; the action maps; the balance run | every crew scenario still reaches its end at every tier |
| C7 | Unreal: controls, HUD, keys, voice, the post feed | the build and the automation tests pass |
| C8 | Tutorials, docs, the full suite | every lesson plays through headless (done 20 Oct 2026) |

## 7. Open

* The PVS selector's and the RTK-1's real position names; what "receives on two radios" means exactly.
* Whether the plotter had a headset. SAM3D follows the regulation: he hears the radiotelephonist.
* The R-124 boxes' seats on the Osa beyond A-1. The ZSU-23-4 analogue and the NVA's deputy with the lock-on button
  suggest A-2 for the deputy.
* The Osa's second R-123M's net (likely the warning net, unsourced).
* The 1980s warning words and talk tables. SAM3D's are placeholders.
