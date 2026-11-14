# The Osa-AK as a vehicle: chassis, electrics, lights, climate, fire (research, 14 Nov 2026)

Research for the roadmap's controls C1-C3 and C7-C9 (`docs/DEVELOPMENT_ROADMAP.md`, "Controls still to wire") and the
leftovers of C4-C6. What SAM3D builds from it is in `data/vehicles/osa_ak_systems.json` (read by
`simcore/vehicle_systems.hpp`) and the console's new inputs and subsystems in `data/procedures/osa_ak.json`.

Page conventions:
- `voenizdat_osa_1991`: the printed page. The book describes the 9A33BM3 (Osa-AKM), but its chassis (2.10, pp. 173-176),
  power (2.9.1, pp. 163-166) and ventilation (2.9.2, pp. 166-169) chapters are the same chassis 5937 and the same 9I120
  unit as the AK, and agree with `mon_2112_80` (the AK's own manual) wherever both speak. Taken as valid for the 9A33BM2
  (medium) unless noted.
- `mon_2112_80`: the printed page (the PDF page minus 5).
- `wwp_gecko`: the book's scan page ids (g084-d etc.), as in `reference/sources/wwp_gecko/notes/`.

Confidence: high = a tier-1 source states it; medium = tier 1 by analogy, or several lower sources agree; low = one
lower source or an inference. *Inference* marks SAM3D's reading, not a source's.

Not used: `vbtt_1988` (Вестник бронетанковой техники 1988 no. 4, an article on AFV fire extinguishing). The journal was
a restricted-circulation (ДСП) publication; although the article is posted publicly, it was left out under the
public-sources rule. Nothing below rests on it.

---

## 1. The chassis 5937 (C1)

### 1.1 Engine

| Fact | Value | Source | Conf. |
|---|---|---|---|
| Engine | 5Д20Б-300: six-cylinder V, four-stroke diesel, liquid-cooled, direct injection, at the rear | `voenizdat_osa_1991` p174; `korovin_tiv_2010` part 2 (a modification of the BMP's UTD-20) | high |
| Power | 300 hp (220 kW) at 2 600 rpm; V 120°, 15.9 l | `ru_wiki_baz5937` (after Kochnev); `wwp_gecko` p6 | medium |
| Lubrication | dry sump: oil tank, oil-priming pump, oil cooler | `voenizdat_osa_1991` p174 | high |
| Fuel tank | 320 l, left, ahead of the third axle | `voenizdat_osa_1991` p174 (place); `wwp_gecko` p6 (320 l) | medium |
| Cooling | closed liquid, ejector cooling, louvres with a drive, expansion tank | `voenizdat_osa_1991` p174 | high |
| Pre-heating | a liquid pre-heater, direct-flow, with a heat exchanger | `voenizdat_osa_1991` p174 | high |
| Starting | two systems: the electric starter from the batteries (or an external DC source), and compressed air from high-pressure bottles | `voenizdat_osa_1991` p174; `kochnev_baz` | high |
| Dash | speedometer 0-120 km/h; coolant and oil temperature; oil pressure; ammeter; tachometer 0-40 ×100 rpm; an hour counter | `wwp_gecko` g084-a, g085-a | medium |

### 1.2 Engine operating numbers (by analogy with the UTD-20)

Nothing was found for the 5Д20Б-300 itself; the BMP-2's manual gives its parent engine's norms (`bmp2_to_studfile`,
low-medium):

- minimum idle 700-800 rpm; working range 1 500-2 600 rpm, recommended 1 800-2 400, maximum 2 800;
- oil pressure 6-10.5 kgf/cm² at 2 200 rpm with the oil at 80-100 °C, at least 2 at minimum idle, up to 13 with the
  oil below 55 °C;
- coolant and oil: at least 55 °C, recommended 80-100 °C, at most 120 °C continuous (125 °C for 10 min).

SAM3D uses idle 750, rated 2 600, maximum 2 800 rpm, working temperature 90 °C, oil pressure 2 at idle rising to 6 at
rated speed and 1.4 times higher with cold oil. The dash's oil-pressure gauge was read as 0-8 in one note and 0-12 in
the cabin's own trace; the trace's 0-12 is kept, which shows the norm.

### 1.3 Transmission, running gear, brakes (`voenizdat_osa_1991` pp174-175, high)

- Twin-plate dry clutch with a hydraulic drive and a pneumatic booster.
- Gearbox: **five forward, one reverse**, synchronisers on 2nd-5th, two power take-offs. **No ratios found.**
- Two-speed transfer case with a lockable inter-side differential.
- Final drives 2.273, wheel reducers 3.73. *Inference:* 2 600 rpm through 8.48 on a rolling radius of about 0.56 m
  gives about 65 km/h, so the documented 60 km/h fits a direct top gear.
- Independent torsion-bar suspension; steering on the 1st and 3rd axles; pneumo-hydraulic service brakes; **two
  parking brakes on the front final drives, with a manual mechanical drive** (the handbrake).
- Tyres 0.4 MPa, central inflation (`mon_2112_80` p26).

The lower gears' top speeds (8, 15, 25, 40 km/h in 1st-4th, 6 in reverse) are SAM3D's estimate.

### 1.4 Speeds, fuel, range

| Fact | Value | Source | Conf. |
|---|---|---|---|
| Road / dirt / off road / afloat | **60 / 30 / 10 / 8 km/h** | `voenizdat_osa_1991` p16, p231; `mon_2112_80` p6 | high |
| At night | up to 25 km/h on the road | `kochnev_baz` | medium |
| Highway maximum of the bare chassis | 70 (Kochnev), 80 (Korovin) | as named | conflict: 60 (tier 1) used for the combat vehicle |
| Range | at least 250 km on dirt roads with 2 h of equipment work on the ГТА | `voenizdat_osa_1991` p17 | high |
| Continuous work | 48 h with refuelling | `voenizdat_osa_1991` p16 | high |

*Inference (low):* 320 l less 2 h of the ГТА at 67 kg/h leaves about 132 kg (≈ 157 l at 0.84 kg/l) for 250 km,
about **63 l per 100 km on dirt roads**. SAM3D's fuel model gives the drive engine 25 l/h on the move (63 l per 100 km at
40 km/h; SAM Simulator's 100 l/h moving against the turbine's 75 at a halt), the turbine its own 75 l/h either way, and
nothing more at a halt, where the engine's burn driving the ГОМ is the ГОМ's 25 l/h.

### 1.5 Air

With the engine running the chassis' 20 l receiver is held at 5-7 kgf/cm² and can feed the antenna compartments'
pressurisation through ЭПК-2 (`voenizdat_osa_1991` p170-173). Not modelled further.

### 1.6 Afloat

| Fact | Source | Conf. |
|---|---|---|
| Two propeller water-jet pumps driven from the gearbox | `voenizdat_osa_1991` p175 | high |
| **Two flaps with a hydraulic drive** and channels in the hull give reverse and manoeuvring afloat | `voenizdat_osa_1991` p175 | high |
| The wave shield is raised before deep water | `voenizdat_osa_1991` p224; its toggle ВОДООТРАЖАТЕЛЬНЫЙ ЩИТ (`wwp_gecko` g085-a) | high |
| The crew watches the bilge pumps while crossing; enter and leave the water square to the bank; equipment off, travel position, hatches shut | `voenizdat_osa_1991` p223-224 | high |

*Inference (medium):* a flap closed sends that jet's water into the reverse channel, as on the BTR-60/70/80; so both
open drive the vehicle ahead, one closed turns it, both closed go astern. SAM3D: afloat needs both flaps open and the
shield up, or the vehicle does not make way.

### 1.7 The driver's drill

- «К бою» (`voenizdat_osa_1991` p246-247): the driver starts the ГТА (ГОМ), reports «Питание подано», **starts the
  drive engine and reports «К маршу готов»**. At a halt the crew chief orders «Выключить двигатель».
- March (p222-223): at night under blackout the road is watched through night-vision devices and **all outside lights
  are off**; through woods the travel locks on; above +40 °C the engine hatch ajar.
- The clutch plate on the driver's pillar: move off only in first gear (`wwp_gecko` g084-c).
- **No cold-start procedure** (pre-heater times, cranking, air-bottle pressure) was found for the 5937. SAM3D: below
  5 °C coolant the pre-heater runs first (its glow plug, then its button), warming the coolant until the engine will
  start; the engine starts in neutral only.

### 1.8 The C1 controls, what the sources say

| Control | Reading | Conf. |
|---|---|---|
| veh.gear | 5 + R; gate Н, 1-5, З.Х. | high (count) |
| veh.handbrake | the parking brakes' manual drive (p175) | high |
| veh.hand_throttle | the fuel pump's hand lever on the power system's diagram (p164, Fig. 2.60): engine speed set by hand while the ГОМ runs at a halt | medium (inference) |
| veh.waterjet_left/right | the two flaps (p175) | high (what), medium (sense) |
| veh.bilge_pump | the wave shield (p224; legend ВОДООТРАЖАТЕЛЬНЫЙ ЩИТ) | high |
| veh.starter | the starter's button below its plate СТАРТЕР on the dash (the plate seen, the button hidden by the wheel's hub in every photo) | high (exists) |
| veh.engine_stop | SAM3D's: the engine is stopped by a control the photos do not show (on the UTD-20 family, the fuel cut-off) | low |
| differential lock, ГОМ drive engage, louvres, tyre inflation, air start | exist (p163, p174-175); their places not identified | — |

---

## 2. The electrics (C7)

### 2.1 What the power system makes (`voenizdat_osa_1991` p163-166, high)

- Three-phase **400 Hz, 218-230 V** for the equipment, and a stabilised **27.5 V DC** for the generators' excitation,
  the system's control and **charging the batteries**.
- The power unit is started from the chassis' batteries, **nominal 24 V**: two 12СТ-70 (p175), single-wire, negative
  to the hull.
- Sources: the **ГТА** (9I120: gas turbine 9И56-3 and generator С-75), 50 kW at 67 kg/h, on the move and at a halt;
  the **ГОМ** (С-75 driven from the engine) 35 kW at 20 kg/h, at a halt only; the external supply (ИВП).
- Common parts: ПУ-303, switching block БКА-333, the driver's additional instrument panel, rectifier **ВУ-1**, the
  **battery switch (ВБ)**, the hatch system; in the equipment the **voltmeter on 0096-9M1**, АВАР. ОТКЛ. СЭП and the
  frequency monitor **УКЧ** in ОС52-16М, which **sheds the load when the frequency falls to 360 Hz**.
- 220 V 400 Hz is distributed and protected in cabinets 0096-9M1 and 0096-10M (contactors, fuses with lamps).

*Inference (medium):* the 27.5 V comes from ВУ-1 off the running generator; with a source on line the 24 V network
stands at 27.5 V and the batteries charge; with none it falls to the batteries' 24-25 V.

### 2.2 External supply

220 V ± 3.5 %, 400 ± 10 Hz (`voenizdat_osa_1991` p23) or 400 Hz ± 2.5 % (`mon_2112_80` p27): both recorded; at
least 35 kW; ≤ 180 A per phase.

### 2.3 ПУ-303 and the voltage setters

ПУ-303 (`wwp_gecko` g084-d) carries НАПР. ГТА and НАПР. ГОМ: the two С-75 generators' voltage setters (the ГОМ's
generator has a voltage-regulating block, p163). *Inference (medium):* the driver trims the output to 220 V (range
218-230) while the operator reads the cabinet's voltmeter (the operator checks 220 V 400 Hz there before АППАРАТУРА
ВКЛ., p247). SAM3D: each knob trims its generator over ±12 V about 220 V; the voltmeter reads the live source's voltage.

### 2.4 The 9И56 gas turbine (manufacturer's catalogue, `kadvi_9i56`, medium)

Output shaft 8 000 rpm; **ready to take load in 35 s**; 48 h continuous; petrol, kerosene or diesel; starts and runs
from -50 to +50 °C; 150 kg. "40 hp" on kolesa.ru conflicts with 50 kW electrical (p163) and is not used.

### 2.5 The C7 controls

| Control | Reading | Conf. |
|---|---|---|
| veh.well_switch | the battery switch (ВБ) of the chassis (p163, p223): SAM3D's candidate, the red-levered box by the driver | low |
| gta.volt_gta, gta.volt_gom | the generators' voltage setters | medium |
| pwr.ukch | switches the frequency monitor in: with it in, a supply below 360 Hz sheds the load | medium (inference) |
| veh.converter (ПРЕОБРАЗОВАТЕЛЬ) | not identified; not the ПЧТ-1,25СТ12А, which is switched from 0004-11M1 (p166) | unknown |
| veh.protection (ЗАЩИТА) | probably the anti-nuclear protection (ПАЗ, p16; the ФВУ, `mon_2112_80` p26) | low |
| veh.interlock (БЛОКИРОВКА) | beside the lamp ЛЮКИ ЗАКРЫТЫ: probably the hatch interlock's override (p99, p154, p207) | low |
| dash lamp БОРТ. СЕТЬ | *inference:* the on-board network on (the battery switch closed) | low |

---

## 3. Lights and light discipline (C2)

| Fact | Source | Conf. |
|---|---|---|
| The series chassis has **built-in blackout headlights** | `kochnev_baz` | medium |
| Each front corner: a sealed-beam headlamp and a sidelight under a guard; rear lamp clusters amber over red | `wwp_gecko` g021, g009, g013 | high (photo) |
| The commander's strip of toggles: ...ТАТЕЛЬ, ЗАЩИТА, ПРЕОБРАЗОВАТЕЛЬ, (hidden), СВЯЗЬ, ПОДФ(АРНИКИ), ...ПЕРЕДНЯЯ / ЗАДНЯЯ, ОТОПИТЕЛЬ | `wwp_gecko` g085-a | high (photo), labels partly read |
| **At night under blackout the road is watched through night-vision devices and all outside lights are off** | `voenizdat_osa_1991` p222 | high |
| The driver's PNV-57E works with the headlamps' IR filters as its source; the TNP periscopes and PNV-57 let him drive "with masked lights" | `mon_2112_80` p26; `vokrachko_1963` | high / medium |
| The battery marches best in darkness or bad weather; the battery commander controls the column by flags and lamps, radio forbidden except for fire control against an air attack | `voenizdat_osa_1991` p239-240 | high |
| Soviet blackout kit: the rear lamp's distance indicator is designed to be seen only by the next vehicle, its figures readable from 150 m in; safe speed blacked out, single vehicle ≤ 25 km/h | `vokrachko_1963` | medium (trucks, 1963) |
| How far a lit vehicle is seen at night | not found in any Soviet source | — |

*Inferences (SAM3D):*
- Toggle 1 reads "...ТАТЕЛЬ", which fits НАГНЕТАТЕЛЬ (the ФВУ blower) better than ОСВЕТИТЕЛЬ; `veh.illuminator` keeps
  its id with the doubt noted.
- The central switch (`veh.light_switch`) 0 / I sidelights / II headlights, and НАРУЖНОЕ ОСВЕЩЕН. (`veh.outside_lights`)
  as the master of every outside lamp.
- A blacked-out vehicle is seen at night to **150 m** (the rear indicator's design distance), a lit one with headlights
  to **6 km** (estimate; no source). The enemy's armed reconnaissance finds a blacked-out column at night a quarter as
  often as a lit one (estimate).
- The night speed limit (25 km/h) applies when driving at night without the headlights.

---

## 4. The cabin's climate (C3)

### 4.1 The ventilation (`voenizdat_osa_1991` p166-169, high; `mon_2112_80` p26, p68)

- The equipment ventilation keeps the radio equipment at its working temperature, left and right racks separately.
  Two modes, **ЛЕТО** and **ЗИМА**, set by the switch on 0096-9M1 and the two ЗИМА—ЛЕТО covers on the engine bulkhead.
- **ЛЕТО**: outside air in through the fan-separators' hatches (opened by hand, the levers over SP-1M1 and SL-1M1), past
  the blocks, out into the engine compartment. 600 W fans: two in and one out per side, four more for the lower sections.
- **ЗИМА**: the air circulates inside the hull along the racks, is warmed by them and is blown back into the
  operators' compartment, heating it; the outside hatches stay shut, the separating fans off.
- A board on the indication panel shows the РТА's hatches closed (p168).
- The operators' fresh air: the chassis' heating-ventilation unit (ОВ-65Г, `korovin_tiv_2010`) in its «Вентиляция»
  mode; a separate fan blows on the operators, switched by ВЕНТИЛЯЦИЯ РАБОЧИХ МЕСТ (p168).
- АППАРАТУРА ВКЛ. switches the filaments and the ventilation together; АППАРАТУРА ОТКЛ. leaves the antenna unit's
  ventilation on 10 min more (p165).
- The operators' compartment is lined with 5-20 mm of sound- and heat-insulating foam (p173).

### 4.2 The rules

| Rule | Source | Conf. |
|---|---|---|
| Above 0 °C outside, keep ЛЕТО; below -5 °C, ЗИМА | `voenizdat_osa_1991` p223 | high |
| **Below -10 °C** ЗИМА, back to ЛЕТО once the cabin is above +10 °C "to protect the equipment from overheating" | `mon_2112_80` p68 | high |
| Above +40 °C the engine hatch ajar; in rain ЗИМА with the inlet hatches open | `voenizdat_osa_1991` p223 | high |
| **After humidity over 80 % and a sudden frost**: switch on with the anodes off, 15-20 min on filaments only, then anodes | `voenizdat_osa_1991` p223 | high |
| The gyro's heating on when the cabin is below +5 °C | `voenizdat_osa_1991` p51 | high |
| ПЕРЕГРЕВ: thermal sensors in the transmitters' hottest points and in ОС61-3M1 trip at a critical temperature (not given); the lamp ПЕРЕГРЕВ 0004-20M1 on the indication panel; "the compartment may fail" | `voenizdat_osa_1991` p169 | high |

The conflict between -5 °C (the AKM's textbook) and -10 °C (the AK's own manual) is settled for the AK's manual.

### 4.3 Not found

No source gives the warm-up times growing with the cold for the 9A33BM2 (the only temperature-dependent figure is the
МТО's readiness, 40 min growing to 60 min at -30..-10 °C, `mon_2112_80` p42), the critical temperatures, the ОВ-65Г's
output or the cabin's working range for the crew. SAM3D's estimates (all marked `estimate` in the data):
- the equipment's heat 8 kW; the racks 15 °C over the cabin with the ЛЕТО draught, 45 °C over it with the hatches
  shut in summer;
- in ЗИМА the cabin 25 °C over the outside with the equipment on, the heater 20 °C more, the crew 4 °C;
- a rack over 75 °C for 3 min fails (an СРП fault); the transmitters 35 °C over the cabin, ПЕРЕГРЕВ at 85 °C, failing
  after 5 min over it;
- below 0 °C the warm-up stretches, to twice as long at -40 °C;
- the crew works slower outside 12-30 °C in the cabin, 2 % per degree, at most 1.5 times.

---

## 5. Fire and stowage (C8)

| Fact | Source | Conf. |
|---|---|---|
| An automatic fire-extinguishing system of the «Роса» type, for fires **in the engine compartment and the power unit's (ГТА) compartment** | `mon_2112_80` p27; `korovin_tiv_2010` (PDF p21) | high |
| The control box low in the driver's left corner: left flap ПРОВЕРКА ЦЕПЕЙ / ПЕРЕВОД ДИСКА / ОСТАТОК БАЛЛОНОВ, right flap "in a fire open the cover and press the button", two red lamps, a selector disc | `wwp_gecko` g086-c | high (photo) |
| «Роса» (T-55): **three bottles**; automatic mode signals the fire, stops the engine and the fans, fires a bottle **after a 4-6 s delay** for the engine to stop; semi-automatic: only the signal, the driver presses the button | `btvt_t55` | medium (the same named system on a tank) |
| Fire equipment checked and expired extinguishers replaced; no soldering in the engine compartment with the fire system off | `voenizdat_osa_1991` p207, p221 | high |

*Inferences (SAM3D):*
- The two lamps are the two zones (engine, ГТА); the disc points at the next bottle to fire (positions 3, 2, 1); the
  button fires the bottle the disc points at, so a spent bottle there gives no agent and the disc must be turned on.
- The automat fires after 5 s; a bottle puts the fire out with chance 0.8; a fire burning 2 min takes the vehicle
  (destroyed); an anti-radiation missile's fragments set it burning with chance 0.15 per hit (all estimates).
- A burning engine compartment stops the engine; a burning ГТА compartment stops the ГТА.
- What the cab's two stowage covers hold is not stated (`voenizdat_osa_1991` p176, p220 list the ЗИП and documents
  carried, not their places).

---

## 6. C9 and the C4-C6 leftovers

### 6.1 fo.mc: the МЦ mode (identified)

- The toggle reads **МЦ — ОТКЛ.** on the straight-on AK console photo (`samsim_panels`). High.
- It is the ПКР (МЦ) mode, switched on once the target is judged a small remotely piloted aircraft: speed under
  100 m/s, the ССЦ АРУ at 2.5-3 divisions, a small image on the TV, no helicopter modulation; automatic tracking best;
  one missile; the 9M33M2 best at 1.5-3 km (`voenizdat_osa_1991` p265, the AKM text). High.
- The AK was reworked from 1980 against small targets near the ground (detection, the control loop, the fuze); the only
  outward sign a toggle ПКР on the main console (`korovin_tiv_2010`). High.
- What it changes inside is not given. SAM3D: with МЦ on, the engagement model treats a small slow target (under
  100 m/s, a drone or cruise missile) as the mode is meant for; with it off against such a target, the fuze's chance
  falls (game values in `data/gameplay/engagement.json`).

### 6.2 ind.select and ОТКАЗ (identified)

The selector and the button ОТКАЗ on the indication panel give the **forced refusal of the missile standing on
preparation** (`voenizdat_osa_1991` p151, p156). The selector's seven positions 3—II … ОТКЛ. … 3—I are the missiles'
табло names (p156, p247). A missile stays ready at most 10 min, then the automation drops it and prepares the next
(p248). SAM3D: the deputy sets the selector to a missile and presses ОТКАЗ; that missile is refused.

### 6.3 Still unknown (left inert, listed for the owner)

veh.strip_4, veh.knob_ammeter, veh.knob_left, veh.knob_tacho, veh.lever_box, veh.converter (ПРЕОБРАЗОВАТЕЛЬ),
veh.protection (ЗАЩИТА), veh.interlock (БЛОКИРОВКА), veh.column_stalk, veh.rear_signal (СИГНАЛ), the stowage covers,
veh.illuminator (perhaps НАГНЕТАТЕЛЬ), the five unread caps of the ПАНЕЛЬ ИНДИКАЦИИ, ro.speed's rate. Candidates
(low): the driver's knobs and lever for the tyre inflation, louvres, differential lock or air start; the five caps
the IFF's signals (as on the AKM, p54, p64-65).

### 6.4 C4-C6 leftovers

- **Fault causes** the text names (`voenizdat_osa_1991`): HV breakdown in the transmitters (p33), overheating of the
  transmitter compartments (p169), the ЭМУ motor's thermal relays (p166), humidity then frost (p223), missile
  safety-circuit faults (p154). Crew-level repairs are fuses, tubes and fusible inserts from ЗИП-1А (p220). No MTBF
  found. Built: rack overheat, transmitter overheat, frost.
- **R-123M** (`bsuir_r123m_2019`): КОНТРОЛЬ НАПРЯЖЕНИЙ at БС reads the vehicle's network, its norm the meter's shaded
  sector; supply 23-28 V. Built: only `VehicleSystems::RadioSupplyInNorm()` (the network within 23-28 V); the modelled selector has no БС
  position and no meter reads it yet.
- **УКЧ** in ОС52-16М sheds the load at 360 Hz (p163). Built.
- **Magnetron drift**: "mainly the resonator cavities' temperature" (p33); no rate. Built: drift grows with the
  transmitter's temperature over its working level (rate an estimate).
- **PRF**: 2 800 Hz from an 11.2 kHz crystal ÷ 4 (p89). *Calculation (low):* at about 7 GHz the first blind speed
  λF/2 ≈ 60 m/s; unambiguous range 53.5 km.
- **ro.speed**: the range finder's manual and semi-automatic tracking exist (p67, p264); no rate found.

---

## 7. Conflicts

1. The ГОМ on the 5937: fitted (`voenizdat_osa_1991` p163, `mon_2112_80`) vs on the BAZ-5939 only (`korovin_tiv_2010`).
   Used: fitted.
2. Road speed 60 (tier 1) vs 70-80 (the bare chassis). Afloat 8 vs 7-12. Used: 60 and 8.
3. ЗИМА below -5 °C (AKM textbook) vs -10 °C (the AK's manual). Used: -10 °C.
4. The ИВП's frequency tolerance ±10 Hz vs ±2.5 %.
5. The oil-pressure gauge's scale, 0-8 vs 0-12.
6. ОСВЕТИТЕЛЬ vs "...ТАТЕЛЬ" for the commander's toggle 1.

## 8. New sources registered

`ru_wiki_baz5937` (4), `bmp2_to_studfile` (4), `kadvi_9i56` (4), `vokrachko_1963` (1), `btvt_t55` (4),
`bsuir_r123m_2019` (2), all in `docs/SOURCES.md`.
