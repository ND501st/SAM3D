# Getting in and out of the vehicles, and walking (20 October 2026)

Research for the crew's bodies out of their seats (`docs/crew/13_OUT_OF_THE_VEHICLE.md`): the real ways in and out of
the 9A33BM2 and the PU-12M, the rules that keep the hatches shut, the drill for dismounting and mounting, how fast a man
moves, and how far his voice carries. Public sources only; the sources are registered in `docs/SOURCES.md` ("Getting
in and out of the vehicles, and walking"). Everything here is paraphrased.

## 1. The Osa (9A33BM2 on the chassis 5937)

From `voenizdat_osa_1991` (the 1991 textbook, read in full on 20 Oct; printed page numbers):

- **The hull** (p173) is divided by a sealed bulkhead into the operators' compartment (forward: the equipment, the crew's
  seats, the driving controls) and the engine compartment (aft). **A door in the bulkhead** joins them.
- **The entry hatch.** "For entry into the operators' compartment there is an entry hatch on the chassis roof" (p173,
  singular).
- **Two windscreens** in the front plate, **one of which opens by hand as the crew's emergency exit** (p173).
- **An emergency hatch on the engine compartment's roof** (p174), for the crew's emergency exit.
- **Safety** (p207): the hatches must not be opened in БР (combat work); **leaving the vehicle is allowed only once the
  readiness to launch is cancelled**; no one within 10 m of the gas turbine's exhaust while it runs; no one in the
  engine compartment while the engine runs; RF limits near the radars (search: 15 min within 50 m; tracking: a 15-20 m
  sector, 15 min within 80 m).
- **On the march** (pp. 222-223): all hatches shut but those the work needs; in forest the road and its edges may be
  watched **through the open entry hatch**.
- **Orientation** (p250): **the aiming circle is set up 20-50 m ahead of the vehicle**; the launcher is laid on it with
  the TV sight (or by the sight); βн read from block 0094-9; the aiming circle's monocular laid on the TV sight's lens
  and its azimuth read; Qc worked out. p249: the crew chief finds Qc with a field artillery aiming circle (the existing
  data's reading; the textbook says Qc "is determined", not by whom).

**Disagreement.** The textbook names one entry hatch; SAM3D's cabin model (`data/cabins/osa_ak.json`,
`tools/blender/build_cabin_interior.py`) has two hatches over the cab seats, after a US Air Force photograph of a
9A33BM2's roof hatch from inside (`usaf_shaw_2017` -036) and the exterior photographs. SAM3D keeps both: the
commander's is taken as the entry hatch the operators use (they walk up the aisle and step up past his seat), the
driver's is the driver's own. Which hatch the drill uses for whom is not known.

**Who goes out for Qc.** The brief named the deputy commander (p248's orientation). The book's sequence needs one man
inside laying the launcher on the aiming circle with the TV sight and reading βн (the deputy commander's and the
operator's consoles), and one man outside at the aiming circle. SAM3D sends **the crew chief** out (the existing note
of p249) and keeps the deputy at his gyro panel, where the Qc goes on the course setter (`orient_vehicle`).

Not found: how long the crew takes to get out or in, the order they leave in, whether steps or handholds are fitted
for climbing the hull. SAM3D's times are estimates (§5).

## 2. The PU-12M (on the BTR-60PB)

- **The BTR-60PB's troops get in and out by the roof hatches** (`fair_btr_compare`: the comparison table gives the PB's
  exit as "upper hatches", the BTR-70's as side doors and hatches, the BTR-80's as side doors).
- The PB's **troop roof hatches are set diagonally over the compartment**, and **two rectangular hatches high in the
  sides near the turret** were added (`vk_btr60`). The traces (`tools/blender/pu12m_trace.py`) already have them:
  the driver's and commander's hatches over the front seats (0.5 x 0.5 m), one middle roof hatch on the right (the left
  one under the PU-12M's roof box), and the side hatches forward of the middle and at the middle.
- **What the PU-12M's equipment leaves of them.** Read against the interior trace
  (`tools/blender/pu12m_interior_trace.py`): the right middle roof hatch stands over the IT-45 (its top 1.92 m under a
  2.025 m ceiling); the side hatches over the RK-3 junction box and the ФР block on the right and the R-123M's power
  unit and the RK-1 on the left. So **the bench crew (battery commander, operator-plotter, radio operator) go forward
  and out by the front hatches**: along the bench's knee room to the gap between the R-111 and the ДХ keypad (25 cm,
  under the folded map board), up the aisle between the telecode units, between the front seats. This is SAM3D's
  inference from its own trace.
- **The AB-1 set** goes on the ground behind the vehicle in the combat position (`csla_pu12_manual`, the existing
  data's reading). **The PAB-2** is set up **30-40 m from the PU-12M in its direction of travel**, its monocular laid on
  the PU-12M's sight (`voenizdat_osa_1991` p249, the battery's topographic preparation).
- **Deployment norms** (`ru_pu12_text`, already in the data): 18 min by day, 20 at night; 25 / 30 min with four wire
  lines laid to 500 m. The wire lines are the first task this system is to carry next (§6).

## 3. The drill: dismounting and mounting

From the drill regulation (`stroevoy_ustav_1993`, the RF's 1993 regulation carrying over the USSR's 1975 one; the 1975
text itself was not found online):

- **К МАШИНАМ** (art. 145-146, 154): line up at the vehicles before mounting; from the vehicles, **get out quickly and
  line up beside them** (or act on the commanders' orders). Heavy weapons are handed down to those already out.
- **ПО МЕСТАМ** (art. 149-150): **get in quickly using the wheels, tracks and steps**; the commander (or the vehicle's
  senior) checks the doors and hatches are shut and takes his seat last.
- **BMP and BTR crews and troops get in and out through their hatches** (art. 147, 154); **men told off open them** on
  К МАШИНАМ (art. 148); **all hatches are shut once in**.
- **Not on the traffic side**: at a road's right edge, no getting on or off over the left side (art. 147, 154).
- **A tank crew lines up in one rank in front of its vehicle, the commander two steps ahead of the right track, the
  others to his left**; on ПО МЕСТАМ they turn about together and each goes to his own hatch (art. 146, 150). SAM3D
  takes this for the wheeled crews (no rule for a SAM crew was found).

The regulation's commands are for a unit (К МАШИНАМ, plural); to one crew SAM3D's commander says «К машине!».

## 4. Pace

- **Walking**: 110-120 steps a minute of 70-80 cm (art. 31): 1.28-1.60 m/s; SAM3D takes the middle, **1.44 m/s**.
- **Running**: 165-180 steps a minute of 85-90 cm: 2.34-2.70 m/s; **2.49 m/s**.
- **The slope**: Tobler's hiking function (`tobler_1993`), v = 6 exp(-3.5 |s + 0.05|) km/h, used as a factor of the
  flat pace (fastest a little downhill).
- **Woods**: no number found; SAM3D estimates 0.6 of the pace through a wood (undergrowth, roots).
- **Inside and on the hull**: estimates, 0.9 m/s stooping along a compartment (the in-cabin walk's pace) and 0.8 m/s on
  a hull's top.

## 5. Times (SAM3D estimates)

No published times were found for a crewman leaving a SAM vehicle. SAM3D's, per leg (`data/vehicles/*.json`): up from
the seat 1.5 s (the crew file's), the hatch lid 2 s, up and out of a roof hatch 4 s (in and down 5 s), down the side
over a wheel 3 s (up 4 s), the Osa's opening windscreen 3 s to open, 10 s to climb out over the bow (14 s in). A crew
of lower skill climbs slower (the tier's reaction time as a factor, 0.7-1.8). A crewman at a seat beside his hatch is
on the ground in about 15 s; the PU-12M's bench crew in about 25 s. Worth checking against a veteran's account.

## 6. How far a voice carries

ISO 9921 (`iso_9921`) gives the vocal efforts at 1 m: normal 60, raised 66, loud 72, very loud 78 dB(A). SAM3D: a man
talks raised (66) and shouts (78); speech falls 6 dB a doubling of distance; it must stand 3 dB over the noise to be
understood (estimate); the countryside's background 45 dB (estimate); a closed hull takes 25 dB off, an open hatch near
the man outside 8 dB (estimates). The machinery adds its noise: the Osa's gas turbine 110 dB at 1 m (estimate; the 10 m
exhaust rule of p207 says it is loud), its diesel 95, the PU-12M's engines 95, the AB-1 set 92 (estimates). So a shout
carries about 30 m in a quiet field and less than a metre beside the running turbine; a man in the line-up hears no one
inside the shut vehicle.

## 7. For later tasks (not built)

- **Laying wire**: the RTK-1's five lines (`simcore/radio_nets.hpp` LineDef), the P-274 field cable to the launchers and
  the regiment, the norm of 25-30 min with four lines at 500 m (`ru_pu12_text`).
- **Mechanical work, the AB-1's servicing, the dugouts and small command posts, rest**: places round the vehicle with a
  job each (`data/vehicles/<system>.json` "places" and "jobs").
- The march signals by flags and lamps (`voenizdat_osa_1991` pp. 232-239) need a man in a hatch or on the ground with
  the flags: the hatch is now a place a body can be.
