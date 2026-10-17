# The PU-12M battery command post: research (October 2026)

Why: phase P0 of `docs/PU12_DEVELOPMENT.md`. The PU-12M is to be a vehicle the player can sit in and run the battery
from. Before its console, procedures and interior are written, this note settles the crew, the stations and the
equipment at each station from the best public sources, and records what is still unknown. Read 17 October 2026.

## Sources read

| Id | Tier | What it is | What it gave |
|---|---|---|---|
| `csla_pu12_manual` | 1 | ČSLA manual Del-25-53/1 *Pojízdné velitelské stanoviště PU-12 – Technický popis* (Prague 1982): scans of its figures posted on `valka_pu12` | The plan of the fighting compartment with 53 numbered items (*Schéma rozmístění jednotlivých zařízení*); drawings of the battery commander's, the squad leader's and driver's, and the radio operator's workplaces; the post in the combat position (mast guyed, two whips, the generator set on the ground); the mast |
| `valka_pu12` | 4 | The armedconflicts.com / valka.cz topic 23991; its text is credited to *PVOV-2-3 Bojová služba na PU-12(M)*, TMNO 24-65-86 | Dimensions and masses; five crew places; the full equipment lists of the PU-12 and the PU-12M's ASPD-U differences; the radars it couples to |
| `flak11` | 1 | NVA veterans' pages PU12-0 and PU12-1 (R. Wagner, 2005), built on the GDR handbooks for plotters (1980) and for air-defence units (1984, 1990) | The ASPD/ASPD-U blocks and what each does, with the handbook's diagram of the information flow; the five radios and the nets each serves; the signals layout drawing (22 items); the 16 m mast; the KP-4's 1 % error and the 5° / 3 km designation error; coupling to a radar up to 300 m away; NVA service 1983-1990 |
| `ru_pu12_text` | 4 | «Общие сведения о ПУ-12 и ППРУ 9С80», a Russian training text (posted on helpiks.org, 2016). Describes the later build on the BTR-80 (R-171/R-173 radios, ТНА-4 navigation) | The crew by numbers; the ASPD-U's track data (100 targets, numbers 00-99, three height zones, height in 0.2 km steps, speed in 12.5 m/s steps shown as a vector); 12 targets in semi-automatic tracking, 2-3 s to take off and send one target; display scales 35, 50, 100 km; deployment norms (18 min by day, 20 at night; 25 / 30 with four wire lines at 500 m) |
| `shilka_narod` | 2 | S. Chizhikov's pages on his Shilka battery (Berdyansk training centre, 1989), with interior photographs by O. Zheltonozhko | Which seat is whose in the photographs (driver left, squad leader right; the operator at the IT-45); the ДХ keypad left of the tube and the ФР block right of it; the gyro behind the driver; the ДТП block behind the operator. Also: the battery commander gives targets to the platoons; the ASPD-U was rarely switched on in peacetime |
| `armforc_pu12` | 4 | Already registered | Crew of four (the numbers without the battery commander); 18 min |

Not found: the NVA plotters' handbook itself (`nva_handbooks`), the Russian service manual (ТО/ИЭ) of the 9С482М, a
deployment drill with its order of work. A page marked ДСП (for official use) turned up in a search and was not opened.

## The crew

| Number | Russian (`ru_pu12_text`) | ČSLA (`csla_pu12_manual` via `valka_pu12`) | SAM3D station |
|---|---|---|---|
| – | командир боевого расчёта (the battery commander) | velitel baterie (čety) | `battery_commander` |
| 1 | командир отделения управления | velitel družstva | `squad_leader` |
| 2 | оператор-планшетист | pátrač-kreslič (searcher-draughtsman) | `operator` |
| 3 | радиотелефонист (armforc: радиотелефонист-разведчик) | spojař | `radio_operator` |
| 4 | водитель-электромеханик | řidič | `driver` |

The sources agree once read together: the post has four numbers of its own, and the battery commander works in it
(`armforc_pu12` and `shilka_narod` list the four; the ČSLA manual and the Russian text add the commander). **Decision:
five stations.** The battery commander's seat can be empty (the NPC runs the battery from outside it).

## Where everything is (the ČSLA plan)

The plan is drawn with the front to the left; the side at the top of the drawing is the vehicle's right (starboard):
the squad leader's workplace drawing puts the KP-4 (item 2) in front of the right-hand front seat, and the Russian
photograph caption puts the driver on the left.

| Station | Items at it (numbers of the ČSLA plan) |
|---|---|
| Driver (front left) | the BTR-60PB's driving controls; GAK gyro direction indicator (30) behind him (`shilka_narod`); PVS-3 intercom box (29); vision blocks (50) |
| Squad leader (front right) | KP-4 coordinate and course computer (2), VO orientation telescope (1), IT-1 status board (4), DTP coordinate (parallax) sensor (5), PVS-2 intercom box (6), the GAK's control panel (7), an R-123M (8) with its power unit (9) |
| Battery commander / operator (right side, middle) | the IT-45 screen (19), the ДХ / DCh characteristics keypad (15), the DI information marker (20), the UO deflection amplifiers (14), the S synchronisation block (13), the ŠČK commander's panel (36), the IT-2 status board (18), the RTK-1 radiotelephone concentrator (16), the RK-3 junction box (17), an R-123M (22) with its power unit (23); the R-407 (25) behind, with its BP-3 power unit (21) and the SAU antenna tuner (26); the air-situation plotting board (24) beside the desk |
| Radio operator (left side, rear) | the radio operator's table (51), an R-123M (40) with power (39), the R-111 (41) with power (37), the RK-1 junction box (38), the PVS-1 intercom box (42); the power boards around him: VS-27 rectifier (43), ŠČR distribution board (44), FP filter (45), RR-361A regulator (47), BP power unit (48) |
| In the hull (no station) | the R (11) and P (34) telecode transmitter and receiver blocks, the SSP follow-up drive (49), the PT-200C-III converter (31), the ŠČV input board (32), RK-2 (33), the BAF-M antenna filters (35), the batteries (46), the telescopic mast (10), the cable entries (12), fans (50), water tanks (52), hour counters (53), two whip antennas (3, 28) |

The operator works at the IT-45: the photographs (`shilka_narod`) call that place the operator's, with the ДХ keypad
to the left of the tube and the ФР block to its right; the ČSLA drawing calls the same desk the battery commander's
workplace. Both work there: the operator marks targets on the tube and plots them, the commander watches the tube and
the board and gives the targets out. **SAM3D:** the operator sits at the tube's keypad (ДХ, DI), the commander beside
him with the ŠČK panel, the RTK-1, the IT-2 and the R-123M of the battery net.

## The equipment

### ASPD-U (the PU-12M's data equipment)

The blocks (`flak11` from the GDR handbook; `valka_pu12`; `shilka_narod`): the **RPS-M** cabinet with the telecode
transmitter (R), receiver (P) and synchronisation (S) blocks (separate in the PU-12, one cabinet in the PU-12M) and the
S23(-1) block; the **SSP** synchronous follow-up drive (works the ASPD in step with a radar coupled to it, for the
primary picture; the PU-12M's 220N); the **DTP** parallax sensor (puts the post's offset from a fixed point into the
telecode automatically); the **IT-45** screen; the **ДХ (DCh)** characteristics keypad (gives a selected target its
characteristics: friend/foe, group, height and so on, or a designation); the **ФР** phase-sweep block (the screen's
deflection voltages and range marks); the **UO** power block. The receiver decodes the air picture and works out the
designated targets' positions with the parallax; the transmitter codes and sends the targets picked with the DI
marker.

What it carries (`ru_pu12_text`): up to 100 targets with X and Y, a number 00-99, friend or foe, a group flag, one of
three height zones (H1 0-1.4 km, H2 1.6-3.0 km, H3 3.2-25.2 km), the height in 0.2 km steps and the speed in 12.5 m/s
steps up to 787.5 m/s, drawn as a vector. Up to 12 targets in semi-automatic tracking; taking off and sending one
target takes 2-3 s. `armforc_pu12`: 99 targets shown, 10 processed, 6 fire units. The PU-12M's ASPD-U adds to the
PU-12's ASPD the unit affiliation, formation, heights and the target's speed (`valka_pu12`).

Inputs: from the regiment's (division's) command post by telecode over the radio (the R-407 net of automated fire
control, `flak11`) or wire, or from a radar (P-12, P-15, P-18, P-19, 1RL128, 1RL111) by cable up to 300 m (the post
then sends the primary picture on as a transmitting post). The battery commander passes designations to his launchers
**by voice**, azimuth and range: the automated chain ends at the PU-12 (`flak11`).

The IT-45 shows the primary picture (a radar coupled by cable) or the secondary picture (telecode from another post)
on scales of 50 and 100 km (`flak11`), or 35, 50 and 100 km (`ru_pu12_text`); the ФР block in the photograph has a
three-position scale switch, read as М35 / М50 / М100. **SAM3D: three scales.**

The ДХ keypad (photograph, `shilka_narod`; labels read at low resolution, so uncertain): a small display, two rows of
five toggles numbered 1-5, keys ВР, ТС, СР, СМ, СВ, Ч, ГР, Н1, Н2, Н3, Н, 22, ЦУ, the digits 0-9, М, ПВ and a wide key
below. СВ and Ч read as свой / чужой, ГР as group, Н1-Н3 as the height zones and ЦУ as designation, which matches the
text. The rest are not interpreted.

The ФР block (same photograph): РАБОТА / КОНТРОЛЬ selector, the scale selector, brightness (ЯРКОСТЬ) and other knobs,
toggles ВКЛ and РАБОТА / КОНТРОЛЬ.

### Radios and intercom

| Set | Range | Net in a missile battery (`flak11`) | Where (ČSLA plan) |
|---|---|---|---|
| FuG-1 R-123M | 15-20 km | air picture (designation net no. 2, the plotters) | squad leader (8) |
| FuG-2 R-123M | 15-20 km | the covered unit's net | radio operator (40) |
| FuG-3 R-111 | 25-30 km | the regiment's command net | radio operator (41) |
| FuG-4 R-123M | 15-20 km | command of the battery (to its launchers) | battery commander (22) |
| FuG-5 R-407 | to 15 km | reception of the regiment's automated fire control (telecode) | battery commander's corner (25) |

Which R-123M is which net in the plan is SAM3D's assignment (the sources give the nets, not the places). The
intercom: RK-1 junction box, PVS-1/2/3 boxes with tank headsets, the RTK-1 radiotelephone concentrator (field
telephone lines and an acoustic signal), RK-3 (`flak11` signals diagram). Five wire lines can be connected; two TA-57
telephones. The R-111 has an automatic antenna tuner (SAU).

The mast: telescopic, **16 m** (`flak11`; a ČSLA caption on valka: "the signalman extends the 16 m mast"); 6 m in
`armforc_pu12` and `shilka_narod`. **SAM3D: 16 m**, the tier-1 figure. Guyed at two heights in the combat position
(the ČSLA drawing). The ČSLA mast drawings (the kinematic section and the general view): a fixed section (1) and eight
moving ones (2-9), raised by a hand winch (naviják, 10) on the fixed section's lower part, over cables and pulleys. Its
raising time is not published.

### Navigation and orientation

GAK (1G13M) gyro direction indicator with its control panel, the KP-4 coordinate and course plotter (draws the route
on maps of 1:50 000 to 1:200 000), a distance sensor, the VO (1T25) orientation telescope, the PAB-2(A/M) aiming
circle (`valka_pu12`, `flak11`). Errors (`flak11`): the KP-4 to 1 % of the distance travelled; in designations,
working with the navigation equipment, up to 5° in azimuth and 3 km in range. The later build: 0.6 % RMS in position,
0-20 (mils) in direction (`ru_pu12_text`, ТНА-4).

### Power

AB-1-P/30-M1 petrol generator set (220 V, 50 Hz) with its URB control unit, set on the ground behind the vehicle in
the combat position (ČSLA drawing); a G-290B generator driven by the engine for work on the move; ŠČV input board,
ŠČR distribution board, VS-27 rectifier, FP filter, RR-361A regulator, two 6ST-75 batteries (`valka_pu12`, `flak11`).
The equipment draws at most 2.5 kW (`ru_pu12_text`). The veteran: "in the field we even watched television off it"
(`shilka_narod`).

### Status boards

IT-1 and IT-2 information boards: the state of the air-defence units, the vehicles' activity, their ammunition
(`valka_pu12`). IT-1 at the squad leader, IT-2 at the commander's desk (ČSLA plan).

## Times

| What | Value | Source |
|---|---|---|
| Full deployment, no wire lines | 18 min by day, 20 at night | `ru_pu12_text`; 18 min `armforc_pu12` |
| With four wire lines of 500 m | 25 min by day, 30 at night | `ru_pu12_text` |
| One target taken off and sent | 2-3 s | `ru_pu12_text` |
| Board update | once a minute | `samsim_osa` p20 (already in `data/command/pu12.json`) |

The order of the deployment work is not published in what was read. SAM3D's order (an estimate, to be replaced by a
drill from the manuals): the driver halts, sets out and starts the AB-1; the radio operator and the squad leader raise
and guy the mast; the radios warm up (the R-123M's 10 min, `docs/research/OSA_COMMS_2026-10.md`); the squad leader
orients with the PAB-2 and sets the GAK and KP-4; the operator switches on the ASPD-U and the IT-45.

## Open questions

1. The ASPD-U panels' controls and their labels (only the ДХ keypad and the ФР block are seen in a photograph).
2. The R-407's and R-111's panels (public photographs exist; not yet traced).
3. The deployment drill's order and the mast's raising time.
4. ~~Who sits where behind the front seats.~~ Read off the plan for P6 (17 Oct 2026): a bench across the compartment,
   facing forward, three places (cushions 1288-1343 px, backs 1350-1385 px, across 338-600 px); the right place is
   straight behind the IT-45's face (its screen's bulge, 1175-1200 px), so the operator sits there, the ДХ keypad at the
   tube's left as the photograph shows. SAM3D: the commander in the middle place, the radio operator at the left (the
   R-111 and its R-123M ahead of that place, his table between them). See `tools/blender/pu12m_interior_trace.py`.
   **Corrected 18 Oct 2026** (`docs/research/PU12_VISUALS_2026-10.md` §2): the NVA catalogue (`rwd_mb3`) lists the
   PU-12M commander's workplace as the IT-45, the ДХ, the IT-2M, the ФР block, the RTK-1 and the DT-2, so the battery
   commander takes the right place at the IT-45 and the operator-plotter the middle one, at the air-situation map (the
   plan's item 24, a plexiglass board).
5. The telecode's message rate and range on the R-407 (only "to 15 km" for the set).
6. The NVA plotters' handbook (1980) for the plotting drill: still to acquire.
