# The PU-12M battery command post: research (October 2026)

Why: phase P0 of `docs/PU12_DEVELOPMENT.md`. The PU-12M is to be a vehicle the player can sit in and run the battery
from. Before its console, procedures and interior are written, this note settles the crew, the stations and the
equipment at each station from the best public sources, and records what is still unknown. Read 17 October 2026.

## Sources read

| Id | Tier | What it is | What it gave |
|---|---|---|---|
| `csla_pu12_manual` | 1 | ČSLA manual Del-25-53/1 *Pojízdné velitelské stanoviště PU-12 – Technický popis* (Prague 1982): scans of its figures posted on `valka_pu12` | The plan of the fighting compartment with 53 numbered items (*Schéma rozmístění jednotlivých zařízení*); drawings of the battery commander's, the squad leader's and driver's, and the radio operator's workplaces; the post in the combat position (mast guyed, two whips, the generator set on the ground); the mast |
| `valka_pu12` | 4 | The armedconflicts.com / valka.cz topic 23991; its text is credited to *PVOV-2-3 Bojová služba na PU-12(M)*, TMNO 24-65-86 | Dimensions and masses; five crew places; the full equipment lists of the PU-12 and the PU-12M's ASPD-U differences; the radars it couples to |
| `flak11` | 1 | NVA veterans' pages PU12-0 and PU12-1 (R. Wagner, 2005), built on the GDR handbooks for plotters (1980) and for air-defence units (1984, 1990) | The ASPD/ASPD-U blocks and what each does, with the handbook's diagram of the information flow; the five radios and the nets each serves; the signals layout drawing (22 items); the 16 m mast; the KP-4's 1 % error and the 5° / 3 km designation error; coupling to a radar up to 300 m away; NVA service 1983-1990. Its two ASPD figures from the 1984 handbook, read closely on 22 Oct 2026: see "The GDR handbook's ASPD figures" below |
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

Who works the IT-45 (corrected 18 Oct 2026, `docs/research/PU12_VISUALS_2026-10.md` §2): the NVA catalogue (`rwd_mb3`)
lists the PU-12M commander's workplace as the IT-45, the ДХ, the IT-2M, the ФР block, the RTK-1 and the DT-2, and names
the rear places commander-operator, plotter (Planzeichner) and radio operator; the ČSLA drawing calls the tube's desk
the battery commander's workplace. The photographs (`shilka_narod`, a Shilka battery's post) call that place the
operator's, with the ДХ keypad to the left of the tube and the ФР block to its right: in the PU-12M the commander is that
operator. The plotter's place is beside it (`rwd_mb3` pu12f), at the air-situation board (item 24); he takes the reports
on targets and plots them (`pvo_bu_x_1984` art. 23). **SAM3D:** the battery commander at the tube (ДХ, DT-2, ФР, the
ŠČK panel, the RTK-1, the IT-2M and the R-123M of the battery net); the operator-plotter at the board, writing what he
hears on the air-picture net. He does not copy the tube onto the board: no source has him do it, the tube is in front of
the commander already, and the board is the back-up when the tube is dark (`narodna_armiya_2018`).

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
control, `flak11`) or wire, or from a radar (P-12, P-15, P-18, P-19, 1RL128, 1RL111; in the NVA the command battery's
PU-12M was coupled to the P-18 and the P-40, the P-15 also possible, `flak11`) by cable up to 300 m (the post then sends
the primary picture on as a transmitting post). The regiment's P-19 carried its own ASPD-U and talked to the batteries'
PU-12s directly (`flak11`). The battery commander passes designations to his launchers
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

## The GDR handbook's ASPD figures (`flak11`, read 22 Oct 2026)

`flak11` PU12-0 reproduces two figures from the *Handbuch für Fla-Raketenschützen und Funkorter* (1984; the page's
source 53): [figure 1](http://www.flak11.de/PU12/PU-12_Schema-ASPD-1.JPG) and
[figure 2](http://www.flak11.de/PU12/PU-12_Schema-ASPD-2.JPG). They belong to the handbook and are not stored here.
Figure 2 draws the PU-12's build: the R, P and S blocks as separate cabinets, which the PU-12M puts in one RPS-M.

**Figure 1, the information flow.** Four stages along the bottom: *Empfang* (reception), *Darstellung* (display),
*Bearbeitung* (processing), *Übertragung* (transmission). The radar's aerial and an incoming radio link feed the AFüSt
(the post); inside, the ASPD's tube (*Sichtgerät*) and the *Marker*; between them sits **Kdr, the commander**, who does
the processing; out again by radio. Processing is the commander's job, not something the equipment does by itself.
SAM3D already works this way: the commander-operator takes the targets off the tube.

**Figure 2, the blocks and their links.** What each element is follows the page's text (above, "ASPD-U"); what the
figure adds is the wiring:

| Element in the figure | Drawn as | Linked to |
|---|---|---|
| FuG with RK-1 Empf. (left) | a radio set, "15 km", over the RK-1's receive side | P, the telecode receiver |
| FuG with RK-1 Sender (right) | a radio set, "15 km", over the RK-1's send side | R, the telecode transmitter ("Sender") |
| DTP | the parallax sensor, between the two | P **and** R: the parallax goes into both what is received and what is sent |
| IT-45 | the tube with its grid glass; the DCh keypad on its left side | P; UO below |
| DI with the Marker | a desk in a dashed outline with a lever (the marker's handle); a small "Kennung" device beside it with an arrow from the tube | R (an arrow): the targets picked with the marker go to the transmitter |
| UO | the power block under the tube | the tube, S |
| S | the synchronisation cabinet in the middle, with rows of lamps and switches | P, R, UO, SSP and the junction from the radar |
| SSP | the follow-up drive under S, with dials | S |
| The radar (dashed box, "Drehmelder") | the radar's synchro on its aerial, **300 m** off by cable | a junction box, then S |

The cable from the radar carries, as labelled: *Echo* (3), *Kennung* (OC), *Entfernung* (DO), *Seitenwinkel* (AO),
*Anlaßimpuls* (3), then separately *Dienstverbindung*, *Synchrokabel* and *Kennung* again. Reading:

- **The primary picture is the radar's own video.** *Echo* is the raw echo video. *Anlaßimpuls* is the trigger that
  starts each sweep. The *Drehmelder* (synchro) on the radar's aerial, over the *Synchrokabel*, drives the SSP so that
  the sweep turns in step with the aerial (the text: "synchronous work with a coupled radar"). *Entfernung* and
  *Seitenwinkel* are read as the radar's range and azimuth mark signals. *Kennung* is the IFF: the radar's interrogator
  answers, put on the tube. The suffixes 3, OC, DO and AO are not interpreted; they may be connector or cable
  designations. So every echo the radar sees is on the tube before anyone takes it off. Taking it off gives it a number
  and its characteristics and sends it.
- **The *Dienstverbindung* is a service telephone line** in the same cable run: the post and the radar's crew talk over
  it.
- **The telecode comes in on one radio and goes out on another,** each through its own side of the RK-1, at 15 km (the
  R-407's "bis 15 km", `flak11` PU12-1). That matches the text: a post coupled to a radar sends its picture on, and the
  DTP's parallax goes into both directions. SAM3D's RK-1 already has separate receive and send controls (`rk1.rx`,
  `rk1.tx` in `data/procedures/pu12m.json`).

## NVA service, from the page's text (not yet recorded above)

- **Who had it** (`flak11`): the Shilka batteries; the mixed missile-artillery batteries (ZSU-23-4 with Strela-1 or
  Strela-10), where both the battery's and the platoon's command posts (BBS, ZBS) rode a PU-12 (the battery tables after
  DV 327/0/002, 1985, as reproduced by `panzerregiment4`); the Osa regiments; up to the mid-1980s also the CTLA's
  command battery and the divisions' chiefs of troop air defence. Introduced into the 11th MSD's Shilka batteries from
  about 1983. FRR-11 received **6 PU-12M in 1987: one for each of its five missile batteries and one for the command
  battery.**
- **Where the automation ended** (`flak11`): "a disadvantage was that automated fire control ended at the PU in the
  missile batteries", so the battery passed designations to each launcher by voice radio, azimuth and range. SAM3D
  already does this (`simcore/battery_link.hpp`).
- **The SAU block** (`flak11`): *Block zum Zusammenwirken mit den Frontfliegerkräften*, a block for cooperation with the
  frontal aviation. Before the equipment passed to the Bundeswehr it was removed and returned to the Soviet Union as
  sensitive technology. **This is not the R-111's САУ** (its antenna tuner, `docs/research/PU12_RADIOS_2026-10.md`).
  The letters are the same and the things are different. Nothing public says what the block did; SAM3D leaves it out.
  Followed up 22 Oct 2026 (`docs/research/SAU_BLOCK_2026-10.md`). Every document that names a САУ in the PU-12M means the
  R-111's tuner, and that includes `flak11`'s own signals drawing. No operator lists an aviation block. What the GDR
  handed back in 1990 as sensitive was crypto, IFF and coding material. The likeliest reading is crypto or IFF equipment
  remembered under the wrong label.
- **The vehicle** (`flak11`): a BTR-60PU (SPW-60PU) base, a BTR-60PB without its turret, the opening closed with a
  steel plate. It has NBC protection, observation and night-vision equipment, and heating and ventilation. In march
  order only the mast and the unit set where the turret would be distinguish it from a BTR-60PA.
- **After 1990** (`flak11`): several PU-12M went to Greece with the 9K33; most were scrapped; a few stayed with the
  Bundeswehr, little used, since threat simulation used single components. One PU-12 is held by the Militärhistorisches
  Museum in Dresden (not on display in 2005).

## The page's sources, followed (22 Oct 2026)

| Source (as `flak11` lists it) | What was found |
|---|---|
| *Handbuch für Planzeichner und Funkorter der Truppenluftabwehr*, Militärverlag der DDR 1980 | No scan or library record online. Still offered as an original by an NVA book dealer (`nva_handbooks`) |
| *Handbuch für Fla-Raketenschützen und Funkorter der Fla-Einheiten der Truppenluftabwehr*, 1st ed., 1984 (the figures' source) | A Deutsche Nationalbibliothek record via the Deutsche Digitale Bibliothek: 624 pages, illustrated, Berlin, held at the DNB Frankfurt. So it can be read in the DNB's reading room. No scan online. The DDB itself refused connections from here; the record was seen in search results |
| *Handbuch für Fla-Einheiten*, 1st ed., 1990 | No record found. `rwd_mb3` also cites it (its PU-12 pages) |
| *Auskunftsbuch der Truppenluftabwehr 1985*, NVA Kdo LaSK, CTLA, ex GVS B 597030 (declassified) | No record found. The NVA's troop air-defence regulations are in the Bundesarchiv, series DVW 1-DRUCK, *Vorschriften ab 1970 → Landstreitkräfte → Truppenluftabwehr* (for example DVW 1-DRUCK/3534, the 2K11 and 2K12, 1978; /3729, the ZSU-23-4M, 1978; /4513, a VHF radio set, 1983). That is the place to ask for it and for the PU-12's own regulations (`rwd_mb3` names A 259/1/408-413 and A 372/1/412). Not checked: the catalogue (invenio) did not load from here |
| H. Krause, Diplomarbeit, Militärgeschichtliches Institut der DDR 1984, ex GVS B415 732 | No record found. It covers the troop air defence up to the end of the 1960s, so nothing on the PU-12 |
| Photographs: (8) *Armeerundschau*, (19) F. Isemer, (40) the Poltava air-defence school | Photographs only (the vehicle in march order; the 1990 vehicle park). Nothing to follow |
| The page's link to pzaku.net | The domain now serves a gambling site; the old page is gone |

The site's full literature list ([Impressum](http://www.flak11.de/Impressum.htm)) names more worth having for the
PU-12M: P. Kneiphoff and M. Brix, *Die Truppenluftabwehr der NVA* (Verlag am Park, 2004); G. Fietsch,
*Nachrichtentechnik der Nationalen Volksarmee, Teil 1: Funkmittel und Antennen 1956-1990* (1993), which would cover the
R-111, R-123 and R-407 in NVA service; DV 327/0/002, the troop air defence's battle regulation for the flak, battery
down to gun (1985); the CTLA's studies of the troop air defence's development 1956-1974 (Lange, ex GVS B237077) and
1976-1980 (Steingrüber, ex GVS B595 593). Registered under `nva_handbooks` as **to acquire**.

`panzerregiment4.de` ("Fla-SFL-Batterie") copies `flak11`'s PU-12 and Shilka pages with permission, renumbered to the
4th MSD and FRR-4. It is the same text, so it does not count as a second source.

## What this changes in SAM3D (recommendations, 22 Oct 2026)

**Built the same day** at the project owner's word, all seven, the cabled radar's jamming strobes included (a P-19 entry
for the EM layer). Confirmed by the P-19's own
technical description (`p19_to` 4.3.1: block С-71 gives the ASPD the trigger and range marks, the echo signals, the
azimuth marks and the identification marks, and a resolver turns its sweep; 4.3.3: the 1L23-6 ground interrogator for
Kremniy-2, -2M and Parol-4, its replies on the indicators). What was built:

| # | What | Where |
|---|---|---|
| 1 | Every echo of the cabled radar on the IT-45 (raw marks, no number), symbols over the ones taken off. An NPC takes the nearest new one off; a player puts the DT-2 marker on it and presses СЪЁМ (`dt2.take_off`), and the helper shows him that move. With a scenario's `post.em` the radar runs in the EM layer (`data/em/p19.json`: the P-19's transmitter, receiver and antenna from `p19_to` §2, calibrated to the published 160 km, which those figures reproduce in the radar equation): its echoes come from the EM tracker, jamming masks them, and each noise jammer paints its strobe across the tube (`p19_to` §4: the radar finds an active jammer's azimuth). Without `post.em` the coverage model stands in | `CommandPost::Scope`, `TakeOffAt`, `NextNewEcho`; `ScenarioRunner::OweTakeOff`; `SimCabinDisplayComponent.cpp` |
| 2 | A cabled radar's target is a friend only if the radar has an interrogator (`iff_interrogator` in `data/command/regiment_radars.json`: the P-19 from `p19_to`, the P-40 an estimate) and the target's transponder answers. The IFF reply's dash paints beside the echo | `ScenarioRunner::UpdatePost`, `BoardContact::transponder` |
| 3 | The service line: the radar's crew ring a line laid to `"radar"` when their radar goes off the air (`post.radar.outages`) and when it is back. The radar's cable is a job at deployment: the radio operator lays it (`cable.radar`, 90 s, at the drum behind the vehicle), on the commander's «Кабель к РЛС проложить!», given only at a position with a radar | `UpdateRadarLine`, `QueueLineCall`; `data/procedures/pu12m.json` `radar_cable_laid`, `deploy_radar_cable`; `data/crew/pu12m_crew.json` `order_radar_cable` |
| 4 | The 300 m: a scenario's cabled radar further from the post is refused (`radar_cable_m` in `data/command/aspd_u.json`) | `scenario.hpp` |
| 5 | The transmitting post: with the RK-1U's ПЕРЕДАЧА on Р-407 (the set reaching the regiment's receiver in duplex, `R407::SendLink`) or on ПРОВОД, it sends what the commander took off, in the telecode's steps, once a cycle | `CommandPost::Sent`, `telecode_sending` |
| 6 | The DTP's parallax needed for sending as for receiving | `telecode_sending`'s enable |
| 7 | Telecode by wire: a scenario's `post.regiment.telecode_wire` and the RK-1U's ПРИЕМ on ПРОВОД bring the regiment's picture past a cold or jammed R-407 | `UpdateRadios` |

On the way a fault turned up: a target in both the regiment's telecode and the cabled radar's picture flipped back to a
telecode track at each telecode cycle. The commander's own fix now holds; the telecode only adds the designation.

The recommendations as written:

1. **The cabled radar's raw echoes on the IT-45** (figure 2: *Echo*, *Anlaßimpuls*, the synchro). Today
   `CommandPost::Scope` draws only `tracks_`, the targets already taken off, and the IT-45 paints an echo only under
   those (`SimCabinDisplayComponent.cpp`, the primary picture). An echo the radar sees but nobody has taken off is not
   on the tube. `NextTakeOff` picks the nearest new one by itself, so a player at the tube cannot choose. Change:
   `CommandPost` exposes the cabled radar's echoes (what `Covers` passes, plus that radar's jamming strobes from the EM
   layer) as untaken marks, and the tube paints them under the sweep. A player's take-off takes the echo under the DI
   marker; the NPC keeps its rule. This gives the figure's *Bearbeitung* stage its meaning: the commander sees the raw
   picture and chooses what goes on. Test: an echo in cover and not taken off is on the scope; taking it off adds its
   number.
2. **IFF on the primary picture from the cabled radar's interrogator** (the *Kennung* line). Today a cabled radar's
   target is a friend because the scenario says so (`TruthContacts`: `t.friendly` gives type 1). Under the first-person
   rule it should be a friend only while that radar's interrogator works and the target's transponder answers;
   otherwise type 9, no IFF. A telecoded target keeps the type as sent.
3. **The service line to the radar** (*Dienstverbindung*): a telephone line between the post and the cabled radar's
   crew, through `simcore/voice_comms.hpp` as a line. The radar crew report faults and switch-offs (the ARM drill) and
   take orders over it. With the cabled radar comes the job of laying the 300 m cable at deployment, which ties into 8.14's
   next step, wire laying.
4. **Use the 300 m.** `radar_cable_m` in `data/systems/pu12m.json` is not read by any code. The scenario loader should
   refuse a cabled radar placed further from the post, and the cable's laying time belongs in the deployment.
5. **The battery post as a transmitting post.** A post with a cabled radar sends its picture on by telecode over the
   RK-1's send side (figure 2 and the text). SAM3D receives only. This matters if the command battery's PU-12M (P-18 or
   P-40 cabled) is ever played; not before.
6. **The DTP for what is sent too.** `dtp.parallax` gates only the regiment's telecode coming in. Once the post sends
   (5), sending needs it too.
7. **Telecode by wire** (`flak11`: to the batteries "by radio or wire"). A wire line as the alternative when the R-407
   net is jammed. `simcore/radio_sets.hpp` carries telecode by radio only.

Not recommended: modelling the SAU block (nothing public on it) or the mixed missile-artillery batteries (outside the
Osa regiment).

## Open questions

1. The ASPD-U panels' controls and their labels (only the ДХ keypad and the ФР block are seen in a photograph).
2. ~~The R-407's and R-111's panels.~~ Read and traced (19 Oct 2026): `docs/research/PU12_RADIOS_2026-10.md` (the
   R-111's ТО with its front drawn and named; the R-407's instruction plate and photographs).
3. The deployment drill's order and the mast's raising time.
4. ~~Who sits where behind the front seats.~~ Read off the plan for P6 (17 Oct 2026): a bench across the compartment,
   facing forward, three places (cushions 1288-1343 px, backs 1350-1385 px, across 338-600 px); the right place is
   straight behind the IT-45's face (its screen's bulge, 1175-1200 px), so the operator sits there, the ДХ keypad at the
   tube's left as the photograph shows. SAM3D: the commander in the middle place, the radio operator at the left (the
   R-111 and its R-123M ahead of that place, his table between them). See `tools/blender/pu12m_interior_trace.py`.
   **Corrected 18 Oct 2026** (`docs/research/PU12_VISUALS_2026-10.md` §2): the NVA catalogue (`rwd_mb3`) lists the
   PU-12M commander's workplace as the IT-45, the ДХ, the IT-2M, the ФР block, the RTK-1 and the DT-2, so the battery
   commander takes the right place at the IT-45 and the operator-plotter the middle one, at the air-situation map (the
   plan's item 24, a plexiglass board). The console follows since the interior pass (18 Oct 2026): the ФР block's
   switches are the commander's. The RTK-1 stands on the desk at the right wall under the IT-2M (SAM3D's choice; the plan
   draws it over the tube's front, out of his seated reach).
5. The telecode's message rate on the R-407 (its reach by antenna: `docs/research/PU12_RADIOS_2026-10.md`).
6. The NVA plotters' handbook (1980) for the plotting drill: still to acquire; offered as an original by an NVA book
   dealer (20 Oct 2026, `docs/research/PLOTTING_BOARD_2026-10.md` → "The plotter's other duties"). The operator-plotter's
   duties beyond the board (`pvo_bu_x_1984` art. 19, 20, 23) and his part in decisions (none found) are recorded there.
   The 1984 handbook, the source of the ASPD figures, is in the DNB Frankfurt (22 Oct 2026, above).
7. The radar cable's suffixes in the handbook's figure 2 (3, OC, DO, AO), and what the "Kennung" device beside the DI
   marker is (an IFF key or lamp?).
8. The NVA's own PU-12 regulations (A 259/1/408-413, A 372/1/412, `rwd_mb3`) and the *Auskunftsbuch der
   Truppenluftabwehr 1985*: to ask the Bundesarchiv (DVW 1-DRUCK) whether it holds them.
