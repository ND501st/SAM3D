# PU-12M and Osa crew communications: research (October 2026)

The deep-research report of 20 Oct 2026 behind `docs/crew/12_COMMUNICATIONS.md`: five researchers' notes (kept outside git in
`research_notes/`) synthesised. Its source keys are registered in `docs/SOURCES.md` where the data cites them. The Osa
textbook `voenizdat_osa_1991` (owner's exception of 3 Oct 2026) was read afterwards for the Osa's crew drill: pp. 244-252
(see `docs/crew/12_COMMUNICATIONS.md` §2).


In a 1980s Osa-AK battery the automated chain stopped at the battery's PU-12M. The regiment's air picture arrived as telecode on the R-407 or by wire, but **every 9A33BM2 received its target by voice on an R-123M, as bearing and range**, and only the vehicle commander carried that voice into the vehicle through the R-124 intercom ([flak11, GDR OD*][flak-pu0]; [flak11, GDR OD*][flak-9a33-3]). Inside the PU-12M the crew worked a purpose-built air-defence intercom, not a tank set: the **RTK-1 concentrator** at the commander's place (intercom, transmit on four radios, receive on two, **five telephone lines**, an acoustic call signal), **three PVS boxes** that each reach the same four voice radios, and an **RK-1U** hub that powers them, carries the calls and switches the telecode path between radio and wire, all heard through **four tank headsets and no loudspeaker** ([rwd_mb3, GDR OD*][rwd-pu12b]; [flak11, GDR OD*][flak-pu1]). Its five sets worked five nets: the plotter's air picture, the covered regiment, the SAM regiment's command, the battery's own launchers and the regiment's telecode. That is the NVA's reading; a ČSLA list contradicts it for the R-111 and R-407 ([flak11, GDR OD*][flak-pu1]; [valka, ČSSR OD*][valka-pu12]). All of it ran under the armed forces' 1986 radio manual: a main station that rules each net, call-answer-confirm, commands read back or answered «Понял», circular orders said twice without receipt, scheduled frequency changes without a word on air, and a timed drill for moving to the spare frequency ([RRS-1986, USSR OD][rrs]). Troop air defence added one decisive exception: **air data, target designation and warnings may go in clear**, with units, officials and places coded ([BU-X 1984, USSR OD][bu10]), and the official dictionary exempts air defence and the warning system from radio silence ([VST-1988, USSR OD][vst-rm]). Jamming was met by procedure (log and report, spare frequency, commands twice, relay, wire) and by set-specific steps such as switching the R-407's АПЧ off ([R-407 plate, USSR OD][r407plate]). Soviet authors expected the enemy to aim at troop air-defence nets first ([Palii 1989, USSR TT][palii-22]). What the public record lacks is the Osa's own words: no source found gives the designation phrase, the launchers' reports, the R-124 box each Osa crewman held, or the 1980s warning code words. For SAM3D this yields four data corrections, a flag on voicing azimuth in mils, and a sourced net-procedure and emission-control drill that simcore does not yet have.

*How to read the citations.* Each link names the source, its country and its type:

- **OD**: official document (regulation, manual, technical description, official dictionary).
- **OD\***: a veterans' site or catalogue that reproduces official documents.
- **TT**: training text, marked *post-Soviet* when written after 1991.
- **VA**: veteran's account.
- **RP**: reference page.
- **PH**: a researcher's reading of a photograph.

Original-language terms are quoted in «». Every inference is labelled as one. The appendix registers each source with its date and caveats.

## Four headsets and one concentrator ran the PU-12M's inside

### A purpose-built air-defence intercom, not a tank set

The NVA's technical catalogue lists the PU-12/PU-12M's internal communication as five kinds of part: the distribution boxes **RK-1U** and **RK-3U**, the commander's speech set **RTK-1** («Kommandeurssprechgerät»), and three boxes of internal communication, **PWS-1, PWS-2 and PWS-3** ([rwd_mb3, GDR OD*][rwd-pu12]). A Czech article built on ČSLA manuals names the same set in Czech: a radiotelephone concentrator RTK-1 and internal-communication boxes PVS-1 to PVS-3. It adds a TA-57 telephone and gives the hub as "RK-1V" ([valka, ČSSR OD*][valka-pu12]). A Russian training text on a later PU-12 and on the 9S80 supplies the Russian names, **радиотелефонный концентратор** and **пульт внутренней связи**, with an РК-1УМ hub ([helpiks, Russia TT post-Soviet][helpiks-pu12]). RK stands for распределительная коробка (distribution box).

The same RTK plus three PVS plus RK-1 combination recurs in the PU-12, its later build and the 9S80. It is best read as an **air-defence command-vehicle intercom family** (inference). "RK-1V" is most likely a transliteration slip for РК-1У (inference).

Two Russian texts give the PU-12 an R-124 tank intercom instead ([armforc, Russia RP][armforc]; [studfile §4.4, Russia TT post-Soviet][studfile-44]). Against them stand the museum nameplates, which read ПВС-1, and three foreign-army sources, which agree on PVS. The R-124 mention most plausibly reflects the base BTR-60PB's own fit or a loose use of the name (inference). **SAM3D should model PVS boxes, not R-124 subscriber sets, in the PU-12M.**

#### What each part does

The catalogue states the purpose of each part ([rwd_mb3, GDR OD*][rwd-pu12b]):

- **RTK-1**: handles the on-board intercom, makes contact over **four radios**, receives over **two radios**, and connects over **five telephone lines**.
- **PVS**: carries intercom conversations and radio traffic over the same **four radios**.
- **RK-1U**: powers the boxes and the RTK-1, interconnects the means of communication, switches in the radios' input circuits, and carries the **optical signalling and the calling of subscribers**. In the PU-12M it also selects the data path for transmit and receive: the ASPD-U's own code, or the S-23 code of the automated field command system.

#### The signals schematic

The signals schematic comes from the 1984 *Handbuch für Fla-Raketenschützen und Funkorter* ([flak11, GDR OD*][flak-pu1]). The layout drawing used in the next subsection comes from the 1990 *Handbuch für Fla-Einheiten*. The schematic shows:

- FuG-1 and FuG-2 run through RK-3 into RK-1.
- FuG-3 (R-111) and FuG-4 each run two lines into RK-1; FuG-5 (R-407) runs one.
- A separate **telecode line** enters RK-1.
- **All five telephone lines end on the RTK-1 alone**, and the RTK-1 has an **acoustic signal**.
- RK-1 connects to the ASPD's transmitter (block R) and receiver (block P).
- There is **one tank headset (Panzerkopfhaube) at each of the four stations, and no loudspeaker**.

#### The panels, as read from museum photographs

The researchers read the museum photographs of the panels. Positions are certain; labels are of medium to low confidence ([rwd_mb3 photos, GDR PH][rwd-pu12b]).

The **RTK-1** carries:

- four lamp push-buttons **РСТ-1 to РСТ-4** (радиостанция 1-4) under a label beginning «РАДИО…»;
- **five numbered line positions**, each with a lamp above a lever;
- a strip headed **«ВНУТРЕННЯЯ СВЯЗЬ»** with lamp push-buttons **ПВС-1, ПВС-2, ПВС-3 and ЦИРКУЛЯР** (a call to each box, and a call to all);
- a jack apparently labelled **ШЛЕМОФОН**;
- two knobs under a label ending «…ВЫЗОВА»;
- a rotary with illegible positions, a large round control (probably volume), a power toggle and lamps.

Each **PVS** carries:

- four lamps over РСТ-1 to РСТ-4;
- a rotary selector;
- a large black centre push-button;
- call buttons for the three other stations (PVS-1 shows ПВС-2, ПВС-3 and РТК);
- an orange lamp, connectors and a small toggle.

The **RK-1U** carries two five-position rotaries, **ПЕРЕДАЧА** and **ПРИЕМ**. Each reads ВЫКЛ / …123 / …111 / …407 / **ПРОВОД** (off, the R-123, the R-111, the R-407, wire). On the PU-12M there are also two toggles, **С-23 ↔ АСПД-У**. That selector fits a post-Soviet statement that the post can take radar data on an R-123 if the R-407 fails ([studfile §4.4, Russia TT post-Soviet][studfile-44]).

### Four boxes for four seats, and none for the plotter

| Box | Place in the NVA drawing (1990) | Crewman in the PU-12M | Reaches | Sets beside it |
|---|---|---|---|---|
| PVS-2 | front right (item 2) | squad leader, «командир отделения управления» (No. 1) | intercom, РСТ-1 to РСТ-4, calls | FuG-1 R-123 (air picture) |
| PVS-3 | front left (item 22) | driver-electrician (No. 4) | intercom, РСТ-1 to РСТ-4, calls | none |
| PVS-1 | left rear, at the radio operator's table (item 17) | radiotelephonist (No. 3) | intercom, РСТ-1 to РСТ-4, calls | FuG-3 R-111, FuG-4 R-123, RK-1U |
| RTK-1 | right middle (item 7) | battery commander at the IT-45 (the original PU-12's "operator") | intercom, transmit on 4 radios, receive on 2, five lines, ЦИРКУЛЯР | FuG-2 R-123, FuG-5 R-407 |
| none found | plotter's place, behind the commander | operator-plotter (No. 2) | not found | the plotting board |

The places come from the drawing's legend and the photo captions ([flak11, GDR OD*][flak-pu1]; [rwd_mb3, GDR OD*][rwd-pu12f]). The crew numbers come from a Russian training text ([helpiks, Russia TT post-Soviet][helpiks-pu12]).

The original PU-12 had a crew of four (squad leader, operator, radio operator-scout and driver) and exactly four boxes ([rwd_mb3, GDR OD*][rwd-pu12]). In the PU-12M the battery commander takes the operator's seat, captioned "Platz des Kommandeurs/Operators", and a separate plotter's place appears behind him ([rwd_mb3, GDR OD*][rwd-pu12f]). A post-Soviet text says the battery commander runs the post personally ([studfile §4.4, Russia TT post-Soviet][studfile-44]). **No source gives the plotter a box or a headset**, although his net, FuG-1 («Netz ZZW Nr. 2, Planzeichner»), is one of the four that any box can select. A Polish reference page places the board between the signaller on the left side and the commander on the right. It says talk between the rear and the front became difficult once the board was set up ([Opisy Broni, Poland RP][opisy]).

One disagreement needs recording. The same NVA catalogue twice captions the driver's box as "PWS-4" ([rwd_mb3, GDR OD*][rwd-pu12f]). The drawing, a ČSLA-based plan and every equipment list have only PVS-1 to PVS-3 ([rwd_mb3, GDR OD*][rwd-pu12g]). PVS-3 is the stronger reading.

#### What each РСТ position probably selects (inference)

- **РСТ-n most likely selects Funkgerät n** of the NVA numbering: FuG-1 R-123, FuG-2 R-123, FuG-3 R-111, FuG-4 R-123. The catalogue numbers the sets 1 to 5, and the boxes reach exactly four. The R-407 (FuG-5) is the data radio and goes to the ASPD through the RK-1U. No box label names a set, so this is medium confidence.
- The PVS selector probably also has an intercom-only position, like the ВС position of the R-124 and the R-145BM. The photos do not show one.
- The RTK-1's "receive over two radios" most plausibly lets the commander monitor two sets while talking on one (low confidence).

#### A correction for SAM3D

The drawing's legend and captions put **FuG-2, the covered unit's net, on the right beside the commander's RTK-1**, and **FuG-4, the battery's own command net, at the radio operator's table** with the R-111 and the RK-1U ([rwd_mb3, GDR OD*][rwd-pu12f]). `docs/research/PU12_RESEARCH_2026-10.md` and `data/procedures/pu12m.json` place them the other way round. Every box can select any of the four voice sets, so the commander still talks to his launchers on РСТ-4. The radio operator tunes and attends the set.

### Calls light lamps, and only a plugged-in headset hears anything

A crewman joins a net by selecting РСТ-n on his box and talking through his headset. Only the RTK-1 can take a telephone line, with the lever under the line's numbered lamp. Intercom calls go by the lamp push-buttons for the other stations, and only the RTK-1 has ЦИРКУЛЯР ([rwd_mb3 photos, GDR PH][rwd-pu12b]).

The schematic draws a headset at each station and no loudspeaker ([flak11, GDR OD*][flak-pu1]). So **a crewman must wear a plugged-in headset to hear the intercom, a radio or a line**. Unplugged, he hears only the acoustic signal and voices across the compartment, which the Polish source calls hard to talk across (inference).

Not found:

- any procedure text for the PU-12 intercom;
- whether its crews used a chest switch (нагрудный переключатель);
- whether the boxes had a loudspeaker mode;
- what the PVS centre button does (press-to-talk, or call).

#### The closest documented analogue: the R-145BM

The R-145BM command-staff vehicle is built on the same BTR-60PU base as the PU-12. It shares the AB-1-P/30-M1 generator set, the G-290B, the RR-361A regulator, the ЩР and ЩВ boards and the 16 m mast ([Andryushchenko 2004, Russia TT post-Soviet][andr-24]; [rwd_mb3, GDR OD*][rwd-pu12g]). Its switching gear is documented ([Andryushchenko 2004 §3.3, Russia TT post-Soviet][andr-33]; [Andryushchenko 2004 §3.4, Russia TT post-Soviet][andr-34]; [Andryushchenko 2004 §3.7, Russia TT post-Soviet][andr-37]):

- Each box has a **КАНАЛЫ** selector (any of four radios, or ВС for intercom), **radio-busy lamps** and call buttons.
- The chest switch has three positions: **ПРИЕМ / ПЕРЕДАЧА / ВЫЗОВ**.
- There is **one intercom net**, so two crewmen cannot talk privately.
- A **circular call** from a commander's or officer's box cuts into every box, whatever mode it is in.
- A line call **rings a bell (ревун) and lights ВЫЗОВ**.
- The drill is: check that the set's busy lamp is out, select the set, transmit, then **return the selector to ВС** so another crewman can get on.

Read against this (inference), the RTK-1 is the R-145BM's commander box and its radio operator's line block in one unit, and the PVS boxes are its other crew boxes. Two things differ. The PU-12's lines end at the commander's RTK-1, not at a radio operator's box. And no loudspeaker or remote-control line block is documented for the PU-12. Whether the RTK-1 can patch a line onto a radio is suggested by its name but stated nowhere.

### Five telephone lines end at the commander's RTK-1

The PU-12M takes **five telephone lines, all on the RTK-1, plus a separate telecode line into the RK-1** ([flak11, GDR OD*][flak-pu1]; [rwd_mb3, GDR OD*][rwd-pu12b]). A GDR page also lists a switchboard and field telephones («Vermittlung, Feldtelefone») ([flak11, GDR OD*][flak-9s482]).

The sources disagree on how many TA-57 telephones the post carried:

- two, in a Russian reference page ([armforc, Russia RP][armforc]);
- one, in the ČSLA list ([valka, ČSSR OD*][valka-pu12]);
- "a field telephone", in a Soviet veteran's list ([Chizhikov, USSR VA][shilka-pu12]).

The TA-57 itself ([Wikipedia ТА-57, Russia RP][wp-ta57]):

- sends an induction call from a built-in generator;
- works on central-battery lines, local-battery lines or lines with no voltage;
- talks 35-40 km over П-274М cable;
- deploys in under 2 minutes;
- can work a radio over up to 150 m of cable through the press-to-talk on its handset.

For the later BTR-80 build, full deployment **with four wire lines laid to 500 m takes up to 25 minutes by day and 30 at night, against 18 and 20 without wire**. Telecode runs **at least 15 km over П-274М** ([helpiks, Russia TT post-Soviet][helpiks-pu12]). A Polish page gives four lines at 500 m in 18 minutes, but attributes that figure to the PU-10 ([Opisy Broni, Poland RP][opisy]).

**No source says where the five lines went or who laid them.** The 500 m norm suits a halted post's near neighbours: the coupled radar, a co-located command post, an observation post. It does not suit Osa launchers spread over kilometres, which the NVA says received their designations by voice radio ([flak11, GDR OD*][flak-pu0]) (inference).

### «ПТ-1» and «ПТ-2» read best as talk tables

No PU-12, PU-12M, Osa, 9S80 or R-145BM source uses «ПТ-1» or «ПТ-2» for a box or a device. «ПТ» has two attested meanings:

- **Talk table.** In Soviet signals usage, «ПТ» with a number is a **переговорная таблица**. The catalogue of Russia's cryptographic museum lists talk tables ПТ-39, ПТ-41 and ПТ-50 under covert troop control, and the duty radio operator's talk tables ПТ-42 and ПТ-43 under communications ([fsrbit, Russia RP][fsrbit]).
- **Current converter.** In PU-12 equipment lists, «ПТ» is a current converter: преобразователь тока ПТ-200Ц-III or ПТ-200Ц-У, "Umformer PT-200Z" in German ([rwd_mb3, GDR OD*][rwd-pu12g]; [helpiks, Russia TT post-Soviet][helpiks-pu12]).

The ČSLA list also has a ceiling lamp PT-37 ([valka, ČSSR OD*][valka-pu12]).

So «ПТ-1/ПТ-2» means **talk tables No. 1 and No. 2** if it appears among signal documents, and a converter if it appears in an equipment or power list. It is almost certainly not an intercom box. This is inference: the label's original context was not given, and no document using it was found.

## Five radios tied the battery post to four masters

### The NVA net table, and where the ČSLA disagrees

The GDR handbooks give the PU-12M's sets and, "for example", their nets, in two columns: one for a SAM battery and one for a ZSU battery ([flak11, GDR OD*][flak-pu1]). The ČSLA manual reads two of the sets differently ([valka, ČSSR OD*][valka-pu12]). The R-407's band comes from its 1982 technical description ([R-407 ТО, USSR OD][r407to]).

| Set | Band and range | SAM battery (NVA) | ZSU battery (NVA) | ČSLA reading | Main station (inference) |
|---|---|---|---|---|---|
| FuG-1 R-123 | 20-51.5 MHz, 15-20 km | air picture, «Netz ZZW Nr. 2», the plotters | the same | the three R-123M: warning of air targets, orders and reports | the regiment's command post (KP) or its control battery |
| FuG-2 R-123 | 20-27 MHz, on a shared antenna | the covered regiment's air-defence chief (L-TLA MSR/PR) | the covered battalion | as above | the covered unit's air-defence post |
| FuG-3 R-111 | 20-52 MHz, 25-30 km | command net of the SAM regiment's commander | the regiment's air-defence chief | air-situation broadcast | the regiment's KP |
| FuG-4 R-123 | 37-51.5 MHz, on a shared antenna | command of the battery (its launchers) | the same | as above | the PU-12M itself |
| FuG-5 R-407 | 52-60 MHz (the table misprints "62-60"), to 15 km | reception of the SAM regiment's automated fire control | the same, from the division | orders and reports, 15 km | the regiment's telecode source |

The NVA reading is the stronger one. A Soviet Shilka-battery veteran says the ASPD-U takes telecode from the command post of the regiment's or the division's air-defence chief ([Chizhikov, USSR VA][shilka-pu12]), and the R-407 is built as a data set. Both readings belong in the data.

A second disagreement concerns what carried the telecode:

- A Russian compilation says the regiment's KP sent radar data **by telecode over an R-123** ([wrk compilation, Russia RP][wrk]).
- A post-Soviet text makes the R-123 the fallback when the R-407 fails ([studfile §4.4, Russia TT post-Soviet][studfile-44]).

Whether the 1980s BTR-60 build could take telecode on an R-123 is unconfirmed. Separately, the name «ZZW Nr. 2» (ZZW = Zielzuweisung, target designation) implies a designation net No. 1, which no source identifies.

### Regulations named three nets to the senior commander

The 1984 combat regulation for troop air-defence gun batteries puts the battery in three of its senior commander's nets ([BU-IX 1984, USSR OD][bu9]):

- the **command** net («командная»);
- the **reports** net («донесений»);
- the **target designation** net («целеуказания»).

It adds a radio direction to the reconnaissance group. Its appendix diagram links the battery command post (БКП) to the unit commander's control net, the unit staff's reports net and the designation net.

The part on radar units adds that the **designation (warning) nets** carry data on the air enemy to command posts and AA subunits, and it makes the station chief answer for timely, accurate data ([BU-X 1984, USSR OD][bu10]). A post-Soviet SAM-battery text repeats the three nets. It puts into the battery commander's own net the crew commanders of the combat, transport-loader and transport vehicles, the fire-section commanders and the maintenance section ([studfile §4.3, Russia TT post-Soviet][studfile-43]).

The three nets fit the NVA table only approximately (inference):

- command ≈ FuG-3;
- designation and warning ≈ FuG-1 by voice plus FuG-5 by telecode;
- the reports net has no set of its own, and may share FuG-3 or FuG-2.

In every net, **the station that serves the senior commander is the main station («главная радиостанция»)**. In cooperation nets the organising staff appoints it. Its operator enforces the regime and discipline, and every other station obeys him without question ([RRS-1986, USSR OD][rrs]). The PU-12M is therefore the main station of the battery net (FuG-4), and a subordinate station in the regiment's net (FuG-3) (inference).

### The regiment, its radar battery and the picture on the IT-45

#### The regiment and its radars

An NVA Osa regiment had five batteries of four target channels each. Its command battery carried a PU-12M in place of the K-1 ([flak11, GDR OD*][flak-struktur]). Fla-Raketen-Regiment 11 received six PU-12M in 1987: five for the batteries and one for the command battery ([flak11, GDR OD*][flak-9s482]; [flak11, GDR OD*][flak-pu0]):

- the command battery's PU-12M was **cabled to a P-18 and a P-40 at up to 300 m**;
- telecode went to the batteries **«per Funk oder Draht»** (by radio or by wire);
- the command battery's P-19 carried its own ASPD-U and could talk directly to the batteries' PU-12s.

On the Soviet side ([BU-X 1984, USSR OD][bu10]), the regiment's radar reconnaissance and control battery holds the radar crews and the control, signals and survey subunits. The division's radar and control platoon sends air-enemy data to the SAM regiment's KP and to the motor-rifle regiments' air-defence subunits. Its radars stand up to 3 km from the post and report by telecode.

A Russian compilation puts the regiment's KP 10-15 km from the batteries, and gives each KP (main and reserve) a PU-12 and an R-142 command-staff vehicle ([wrk compilation, Russia RP][wrk]). A former Soviet Osa regiment photographed in 2015 still had one R-145BM as its regimental command post and five battery PU-12s, hull numbers 300 to 340 ([zmei, Ukraine RP][zmei]). That is a post-Soviet state.

#### The picture on the IT-45

The PU-12M's IT-45 shows one of two pictures ([flak11, GDR OD*][flak-pu0]):

- a **primary picture** from a radar cabled to the post, kept in step by the SSP block;
- a **secondary picture** received by telecode from another post. The DTP block feeds in the post's offset from a reference point, so received targets are corrected for parallax.

A later build lists its sources and notes that in practice the post receives and transmits at once ([helpiks, Russia TT post-Soviet][helpiks-pu12]):

- automated-control sources: ППРУ, ПУ-12, МП-22, МП-25, 1РЛ134 and 9С18М1, through the ASPD-U and the С23 device;
- cabled radars: П-12, П-15, П-18, П-19, 1РЛ128 and 1РЛ111.

The crew chooses at the RK-1U's ПРОВОД position and its С-23/АСПД-У toggles ([rwd_mb3 photos, GDR PH][rwd-pu12b]). No source says who turns them. The nearest statements are that the battery commander runs the post and the squad leader organises the radios ([BU-IX 1984, USSR OD][bu9]).

Practice was thinner than the design. A Soviet veteran says the ASPDU was rarely switched on in peacetime and was not used in platoon live firings ([Chizhikov, USSR VA][shilka-pu12]). Poland had no ASPD in its batteries and passed targets by voice radio ([Opisy Broni, Poland RP][opisy]).

The GDR also removed some blocks from its PU-12Ms and returned them to the USSR as sensitive equipment. One veteran's page calls them SAU, for cooperation with frontal aviation ([flak11, GDR OD*][flak-pu0]). Every document that draws an SAU, the same site's signals drawing among them, means the R-111's antenna tuner. What went back in 1990 as sensitive was crypto, IFF and coding material, so the block was probably one of those (`docs/research/SAU_BLOCK_2026-10.md`).

### Neighbours and the covered unit shared nets, not a battery-to-battery link

**No source describes a battery-to-battery net or a peer exchange of telecode.** Distributing targets between batteries is the regiment's job: the ASPD receives and issues **addressed target assignments** ([flak11, GDR OD*][flak-pu0]). Inside the battery, the commander distributes targets to his platoons ([Chizhikov, USSR VA][shilka-pu12]).

Neighbours cooperate by prior agreement. A radar commander learns the positions of neighbouring radar, AA and combined-arms units ([BU-X 1984, USSR OD][bu10]). He agrees with them:

- **decentralised mutual warning**;
- the radio data of the nearest radar subunits' report links;
- warning and control signals;
- **electromagnetic-compatibility measures**.

A battery commander's map carries the nearest SAM and AA positions, the radar post, the unit's command post, friendly aviation's corridors and patrol zones, call signs and signals ([BU-IX 1984, USSR OD][bu9]).

The covered unit comes in on FuG-2:

- In a motor-rifle regiment, the PPRU «Овод» of the air-defence battalion commander passes the air picture to the battery PU-12Ms by ASPDU ([Chizhikov, USSR VA][shilka-ovod]).
- The 9S80 warns the covered subunits and keeps cooperation with them ([helpiks, Russia TT post-Soviet][helpiks-pu12]).
- Strela-10 platoons were to get commands and designations from the battery post and the regiment's air-defence post by radiotelephone until data equipment was fitted, and by radio telecode afterwards ([pvo.guns.ru, Russia RP][pvoguns-s10]).

### Warnings came down designation nets in clear, ahead of all other traffic

Troop air-defence communications must carry air-situation data, and warnings of the air and ground enemy and of NBC contamination ([BU-X 1984, USSR OD][bu10]). The gun-battery regulation sets the duty routine ([BU-IX 1984, USSR OD][bu9]):

- The battery command post receives air-situation data continuously.
- The duty subunit is controlled centrally from the senior duty command post. It reports at once every detection, every opening and ceasing of fire, each result and each expenditure of ammunition.
- At readiness Nos. 1, 2 and 3 alike, **radios, telecode and automation stand ready to receive designation (warning), combat commands and signals**.
- Readiness No. 1 is taken on order, or on the battery's own initiative when ground combat begins or a raid is detected.

In the 1986 manual, **«Воздух» is a traffic urgency category, second only to «Монолит», and is sent out of turn**; any delay in it is reported at once ([RRS-1986, USSR OD][rrs]). That makes «Воздух» a priority class. It does not prove that «Воздух!» was the spoken radio alarm. Post-Soviet texts call the air-enemy signals **uniform standing signals** («единые постоянно действующие»), passed out of turn on every channel to the units under threat ([Dmitryuk 2012, Belarus TT post-Soviet][dmitryuk]).

Assembled from these sources (inference), the 1980s warning chain ran:

1. from the division's air-defence control post to the SAM regiment's KP and its radar battery;
2. from there to the battery, by voice on FuG-1 to the plotter, and by telecode on FuG-5 (or by wire) to the IT-45;
3. from the PU-12M to the launchers on FuG-4, and to the covered unit on FuG-2.

Neighbours and the covered unit could warn first.

**Not found:**

- the period's warning code words;
- the field order of a voice plotting report in a Soviet troop air-defence source;
- any 1980s army or front HF warning net with receivers at battery level.

### Wire served the halted battery; the diplexer fixed the band plan

Radio is the main means of communication. **Wire serves control in the defence, in assembly areas before an offensive and inside positions. Signal means serve the march and the occupation and abandonment of positions** ([BU-X 1984, USSR OD][bu10]). A gun battery links its command post, fire-control radar and platoons by wire, with a loudspeaker system where fitted ([BU-IX 1984, USSR OD][bu9]). A post-Soviet SAM-battery text adds wire for when the enemy jams radio ([studfile §4.3, Russia TT post-Soviet][studfile-43]). Wire to the Osa's own launchers is not documented, and would be unusual given how widely they were dispersed (inference).

The one published band split is the schematic's: **FuG-2 on 20-27 MHz and FuG-4 on 37-51.5 MHz share one antenna through a diplexer («Antennenweiche»)** ([flak11, GDR OD*][flak-pu1]). The R-123's sub-bands are 20-35.75 and 35.75-51.5 MHz in 25 kHz steps ([Wikipedia R-123, RP][wp-r123]). The diplexer therefore forces the battery net into sub-band II and the covered unit's net into the bottom of sub-band I (inference).

Five VHF transmitters on one hull between 20 and 60 MHz demand co-site planning. The R-111's manual bars another set within 10 % of its frequency, or 8 or 16 MHz from it, in the same vehicle or within 300 m ([R-111 ТО, USSR OD][r111to]). The official definition of radio-electronic protection explicitly includes stable work under the mutual influence of one's own emitters ([VST-1988, USSR OD][vst-rez]). No actual frequency plan or radio data of the period was found, as expected for documents that were controlled in their day.

## A 1986 manual scripted every call, change and receipt

The armed forces' radio manual of 1986, brought in by General Staff directive No. 177/24/0945 of 19 May 1986, governs every open radio channel, telephony included ([RRS-1986, USSR OD][rrs]). The troop air-defence regulations build on it. Their main addition is the right to talk in clear about targets.

### Radio data arrived on a signed form for one validity period

Every station works from **«радиоданные» (radio data) on a special form** ([RRS-1986, USSR OD][rrs]; [VST-1988, USSR OD][vst-rd]). The form gives:

- frequencies and call signs, and the times they change;
- the mode of communication;
- where needed, azimuths to correspondents, keys to radio documents, and radio passwords.

Radio data are issued against signature or entered in the station log. **On portable sets they are written on the set's front panel.** They serve one validity period and are then withdrawn and destroyed.

A training text's sample form shows telephone call signs as word plus number (e.g. «СЫЧ–96»), four-character telegraph call signs, and working, spare and reserve frequencies. It also carries the keys to the duty operator's table, which change daily at 00:01 Moscow time ([studopedia, Russia TT post-Soviet][studop-tdr]).

Inside the battery, the commander gives the **командир отделения управления** (squad leader) the radio data, the call signs of telephone stations and officials, and the rules for coded documents ([BU-IX 1984, USSR OD][bu9]). The squad leader then:

- hands the radiotelephonists their data;
- checks that communications are ready;
- sets the operators' duty roster;
- knows the warning, recognition and control signals.

The radio operator logs every radiogram, signal, report and designation with its time, and reports it to the commander. The commander checks the timely change of IFF codes, **talk-table keys** and the PVO grid ([BU-X 1984, USSR OD][bu10]).

For call signs, a post-Soviet textbook's example for an AA missile regiment gives each official a **number** and each station a **word** ([bstudy 13.2, Russia TT post-Soviet][bstudy-132]):

- numbers: regiment commander 01, chief of staff 02, **battery commander 10, deputy battery commander 11**;
- words: the regimental command post «Тополь»; the batteries «Рябина», «Лист», «Осина», «Ива» and «Сосна».

The chief of communications compiles the table, issues extracts, and changes it daily or more often. The scheme is illustrative only.

### The main station rules the net; checks and passwords come on order

The 1986 manual sets the basic exchange ([RRS-1986, USSR OD][rrs]):

- **Making contact.** Two-way contact counts as established when the called station answers and the caller confirms.
- **No answer.** If there is no answer within **3 minutes** and the called station is not busy, the call is repeated **up to three times**, then reported and logged.
- **Telephony.** The code abbreviations become words: «Понял» (understood), «Принято» (received), «Приём» (over). A call runs «Альфа 45, я Бета 33, приём» and is answered «Я Альфа 45, приём». **On a good link, stations work without call signs.**
- **Spelling and numbers.** Hard words are spelled only with the official name alphabet (Анна, Борис, Василий…). Operators count «один, два … десять» when tuning and may never count backwards. Numbers are spoken in fixed groups.
- **Signal quality.** Audibility (ЩСА) and intelligibility (ЩРК) are graded on five points. Traffic is satisfactory at ЩСА 3 and ЩРК 4, and grades are sent only when hearing is bad.
- **Radio checks** are made only on order, never on the operators' own initiative.
- **Passwords** are demanded on first contact on new radio data, before combat orders, when a station outside the radio data enters the net, and on suspicion.
- **A senior station breaking into a subordinate net** tunes to that net's data, **listens first**, transmits only if there is no operational traffic, passes its command and switches off. The subordinates report the command to their own commander.

Operators listen before every transmission ([bstudy 13.3, Russia TT post-Soviet][bstudy-133]). A lesson adds that a station which fails the password is cut off ([lesson T2, Russia TT post-Soviet][ryk-t2]). Beyond call, answer, confirmation and, on new data, the password, no formal "entering the net" ritual was found (inference).

### Duty reception locks the transmitter; presets make changes quick

**Duty reception («дежурный приём») forbids switching a transmitter on, tuning with radiation, and even changing frequency under interference.** Only retuning without radiation to the scheduled changes is allowed ([RRS-1986, USSR OD][rrs]).

The **R-123M**:

- uses duty reception for long watches with the engine off;
- needs **3 minutes for the transmitter valves when going back to СИМПЛЕКС**, and 10 minutes from switch-on before calling ([KazNU 2017, Kazakhstan TT post-Soviet][kaznu]);
- can stay on duty reception without time limit, drawing about 80 W against 230 W when transmitting;
- may transmit continuously for no more than 10 minutes, at a 3:1 ratio of receive to transmit ([helpiks ch. 9, Russia TT post-Soviet][helpiks-rhbz]).

The **R-111**:

- has the modes ОТКЛ. (off), ДЕЖ. ПР. (duty reception) and transmit at **1 %, 20 % or 100 %** power;
- is tuned at 20 % after 1.5-3 minutes of valve warm-up, by pressing НАСТРОЙКА УМ ([R-111 ТО, USSR OD][r111to]);
- switches from receive to transmit in ≤0.7 s ([studopedia, Russia TT post-Soviet][studop-r111]).

The R-123M holds **four fixed frequencies**. Each is set before work with a mechanical fixator on a drum and a special key, together with its antenna tuning, and any of them is reached with one switch movement ([helpiks ch. 9, Russia TT post-Soviet][helpiks-rhbz]). One textbook puts the preset change at **15 s or less** ([SFU 2011, Russia TT post-Soviet][sfu]). An unfixed frequency drifts off with vibration, so smooth tuning is for halts only ([KazNU 2017, Kazakhstan TT post-Soviet][kaznu]). The R-111's four presets retune in **45 s or less** and take 4 minutes to set up ([studopedia, Russia TT post-Soviet][studop-r111]).

**What the four presets normally held is not stated.** The natural reading is the working and spare frequencies of two nets (inference).

### Moving to the spare frequency follows a 1-2, 5 and 10 minute clock

The 1986 manual separates scheduled and unscheduled frequency changes ([RRS-1986, USSR OD][rrs]):

- **Scheduled changes** happen at the times in the radio data, with no agreement, command or signal on air.
- **Unscheduled changes** in a net happen **only on the main station's command**, with the radio-centre duty officer's permission, and only when interference makes the working frequency unusable.
- Frequencies are always named by **conditional number**, never by value. With a single spare frequency, the code expression is **«Переходите на запасную частоту»** (go to the spare frequency).

The move to the spare frequency runs as a drill:

1. The main station sends the order on the **circular call sign**.
2. Each station repeats it back.
3. The main station confirms.
4. **All stations retune at once and wait for the main station's call**, normally under **new call signs**.

If contact does not come back, a timed fallback follows:

- after **1-2 minutes** with no call, a station calls the main station itself;
- after **5 minutes** with no contact, it tries the old and new frequencies together, with a second receiver on the old one where possible;
- after **10 minutes** with no contact on either, it reports loss of communication and acts on orders.

### Talk tables hid orders, but target data went in clear

Covert control in subunits used four kinds of document ([bstudy 13.2, Russia TT post-Soviet][bstudy-132]):

- call-sign tables of officials and stations;
- signal tables;
- code tables and talk tables;
- coded maps.

Troop air defence keeps all of them («переговорные таблицы (таблицы сигналов), кодированные карты, таблицы позывных») ([BU-X 1984, USSR OD][bu10]). But it **permits open talk** for:

- warnings of the air enemy and of contamination;
- data on targets detected by radars and observation posts;
- target designation.

Even in open talk, units, officials and places go by call sign or code. **The positions of air targets and of own aircraft go by the automated systems, or by coded squares of the PVO grid («сетка ПВО (сетка целеуказания)»).** The gun-battery part extends open text to fire-control commands and to reports of completed fire missions ([BU-IX 1984, USSR OD][bu9]).

Post-Soviet samples show what these documents looked like ([bstudy 13.2, Russia TT post-Soviet][bstudy-132]):

- A 1999 talk table of an AA missile regiment maps tactical terms (aviation, attack, battery, readiness Nos. 1-4, and so on) to **three-digit groups**.
- A battery's signal table gives each signal a radio code, a spoken word and a flare or horn signal. The air enemy is «Воздух» with a green flare; the all-clear is «Отбой» with a white flare.
- The СКК-7 coded map relabels the grid lines with random three-digit numbers.

Another textbook's sample puts the air alarm at «555» by radio, with «Воздух» by voice ([SFU 2011, Russia TT post-Soviet][sfu]). The first sample used 333. The codes were set per operation, so any numbers in SAM3D are placeholders.

The duty operator's table (ТДР-84) is a 100-cell table of service phrases. It is for operators, not commanders ([studopedia, Russia TT post-Soviet][studop-tdr]). The ASPD-U also passes commands and reports as **two-digit numbers** ([flak11, GDR OD*][flak-pu0]). Their table remains a gap (`data/command/aspd_codes.json`).

**No 1970s-1980s troop air-defence talk table, signal table or warning code word was found.**

### Launchers got bearing and range by voice, and read it back

The NVA states the core fact plainly. Automated fire control ended at the battery's PU-12M. From there **each launcher got its designation by voice radio, as bearing («Seitenwinkel») and range («Entfernung»)**. With the post's navigation set, designation errors reached **5° and 3,000 m** ([flak11, GDR OD*][flak-pu0]).

Two other accounts fill in the form:

- A Slovak forum member, who says he commanded a PU-12, recalls targets given by voice as target number, azimuth in degrees, range and altitude (as summarised by the researchers) ([buko1, Slovakia VA][valka-buko1]).
- A Russian compilation has the battery commander give azimuth and range on the battery net. The launchers report lock-on, and the commander names the vehicle or pair that fires ([wrk compilation, Russia RP][wrk]).

The only target order found in full is a post-Soviet cadet guide's worked example: «Дон, я Терек. Цель 101 уничтожить расход 2», meaning "destroy target 101, expend two" ([cadet guide, Russia TT post-Soviet][studfile-sprav]).

The PVO's non-automated air-situation report, as taught at a Belarusian military department, has five fields ([Dedyaev 2010, Belarus TT post-Soviet][bguir]):

1. **target number**;
2. **place**: a seven-digit PVO-grid group or, from a radar operator, **azimuth in degrees and range in km as two three-digit groups**;
3. **an identity-and-composition index**;
4. **height in hectometres**;
5. **time, in minutes of the hour**.

«00» stands for anything not yet known. A subunit post numbers targets 01-99, and a unit post uses blocks of four-digit numbers, with own aircraft starting with 0.

Receipts follow the 1986 manual ([RRS-1986, USSR OD][rrs]; [bstudy 13.3, Russia TT post-Soviet][bstudy-133]):

- **Radiotelephone commands** go without a preliminary call, and are **acknowledged at once by exact repetition or by «Понял»**.
- **Circular commands** are said twice and **get no receipt**, unless the main station asks for one.
- **Signals** are said twice between call signs, and are receipted by repeating each signal once. In the general form, the whole signal is repeated after a 10-second pause. The manual's own example happens to use the call sign «ОСА—31».
- Combat-control and warning signals go first, and routine talk is broken off for them.

#### Azimuth units disagree

- The PVO report form and the Slovak veteran use degrees.
- The NVA names no unit.
- A Russian compilation writes a sector in goniometer notation, "5-00" ([wrk compilation, Russia RP][wrk]).
- An unsourced Wikipedia passage says MANPADS teams got directions by cardinal points, with errors up to 40° and 5 km ([Wikipedia Igla, Russia RP][wp-igla]).

SAM3D voices azimuth in mils on the 60-00 circle (`simcore/include/simcore/pu12.hpp`, `docs/crew/04_COMMANDS_AND_CALLOUTS.md`). The sources found lean to degrees.

**Not found:**

- the exact designation phrase to an Osa, a Shilka or a Strela;
- whether course or speed was given;
- the launchers' report formulas;
- the meanings of the identity-index digits.

## Radio silence never silenced the air-defence nets

### Silence meant transmit-inhibit, with the warning net exempt

The 1988 military dictionary defines **«радиомолчание» (radio silence) as a ban on transmitting, to hide one's troops from enemy radio intelligence** ([VST-1988, USSR OD][vst-rm]). It **exempts individual transmitters** working:

- with reconnaissance;
- in aviation;
- in air defence («ПВО»);
- in the warning system («в системе оповещения»);
- in march traffic control.

The 1958 dictionary speaks of "a restriction or a complete ban", so partial regimes existed ([KST-1958, USSR OD][kst]).

Several levels set and enforce the regime:

- **The staff that organises the communications** sets it. Even without restrictions, transmit time must be minimal ([RRS-1986, USSR OD][rrs]).
- The main station enforces it on air (same source).
- The battery commander sets **emission by readiness level** and the sectors in which emission is allowed for checks and tuning ([BU-IX 1984, USSR OD][bu9]).
- The squad leader gives the order of radio work on the march (same source).

The regulation also tasks the battery with hiding the composition, location and characteristics of its emitters from enemy technical reconnaissance (same source). Throughout, radios, telecode and automation stay ready to receive designation and warning, and open text stays allowed for air data and designation.

Company-level practice in a post-Soviet text fills in the rest ([Kostroma lecture, Russia TT post-Soviet][kgu]):

- In the defence, awaiting attack, all radios sit on duty reception and transmit only warning signals. Control is by messengers and signals.
- On the march, sets stay on duty reception until a meeting engagement or a warning of weapons of mass destruction.
- The restrictions lift when combat starts.

Under silence a set may still be retuned **without radiating** to a scheduled frequency change ([RRS-1986, USSR OD][rrs]). The R-123M's drum may turn. The R-111's НАСТРОЙКА УМ, which tunes the output stage at 20 % power and so radiates, would be barred (inference).

**Not found:**

- any radio-silence rule written for a SAM battery or for the Osa;
- any NVA radio-silence (Funkstille) rule;
- the formal wording to impose or lift radio silence.

### Short, rare, low-power transmissions against fast direction finding

The 1986 manual tells operators that **shorter transmissions hamper enemy radio intelligence** ([RRS-1986, USSR OD][rrs]). It:

- confines service talk to set tables and code expressions;
- bans surnames, ranks, deployment areas and unit names on open channels;
- asks for new call signs on the spare frequency.

Other sources add more:

- A textbook adds forbidding transmission where radio is not needed, using the **minimum necessary power**, and using directional antennas ([bstudy 13.1, Russia TT post-Soviet][bstudy-131]).
- A ground-forces manual of unknown and probably post-Soviet edition adds intermittent emission and decoy emitters ([BUSV ch. 10, Russia OD edition unknown][busv]).
- A lesson claims that a set transmitting for **30 seconds is already detected** ([lesson T2, Russia TT post-Soviet][ryk-t2]). That is a single tier-4 figure.

Violations fall into three categories, with disciplinary liability ([bstudy 13.3, Russia TT post-Soviet][bstudy-133]). Unauthorised links could bring criminal liability under article 206 of the RSFSR criminal code ([RRS-1986, USSR OD][rrs]).

Power is a real lever. The R-111 is set to 1, 20 or 100 % **"for the distance"**, giving 65-80 W at full power ([R-111 ТО, USSR OD][r111to]). The code tables let a station be told to raise or lower its power (ЩРО, ЩРП) ([RRS-1986, USSR OD][rrs]).

The threat was concrete in Soviet eyes. Palii credits:

- the US TRQ-114 with **6-12 bearings a minute in 20-80 MHz**;
- the corps TSQ-112 with automatic VHF direction finders;
- Guardrail V with **30-40 VHF fixes an hour**.

He warns that NATO planned to combine electronic warfare with **fire strikes on command posts and emitters** ([Palii 1989, USSR TT][palii-22]; [Palii 1989, USSR TT][palii-24]).

**Not found:**

- a time to fix a single transmission;
- a maximum message length for tactical VHF (only the equipment's 10-minute duty limit and a 3-minute cap on long calls);
- any recorded strike on a Warsaw Pact air-defence emitter located by direction finding.

### Wire and remote lines kept emitters off the air or away from the post

Wire carried control in the defence and inside positions, as above. The PU-12M passed telecode by radio or by wire ([flak11, GDR OD*][flak-pu0]). Two remote-working options existed:

- The R-111 can be worked **from a field telephone on its line terminals, up to 500 m away** ([R-111 ТО, USSR OD][r111to]).
- The TA-57 can key a radio over 150 m of cable ([Wikipedia ТА-57, Russia RP][wp-ta57]).

Moving the transmitter away from the post as an anti-direction-finding measure follows from this equipment, but was not found as a rule (inference).

On the Osa, the Polish manual ([MON Uzbr. 2112/80, Poland OD][mon]):

- gives the R-123M a range of **at least 12 km with the vehicle's radar transmitters working, and 15 km with them off**;
- provides the TA-57 for a subscriber outside the vehicle at a halt;
- requires the frequencies of the radars, the missiles and the R-123M to be chosen for the greatest jam resistance and radio-electronic masking;
- puts launchers at least 200 m apart, with their command-transmitter frequencies interleaved.

A Russian compilation adds the radar's own discipline ([wrk compilation, Russia RP][wrk]). Launchers switch high voltage on only after designation, and off after the kill. A duty vehicle may stand apart from the battery. Visual and TV search continue throughout.

## Against jamming the drill was report, spare, repeat, relay

### Operators recognised jamming by ear

Soviet doctrine split jamming into **masking** (noise-like) and **imitating** (false signals), and split masking into **spot («прицельные»)** and **barrage («заградительные»)** ([BSE, USSR RP][bse]).

Palii tells operators what each type sounds like ([Palii 1989, USSR TT][palii-3]):

- Noise jamming in a radiotelephone receiver **sounds like the receiver's own noise**.
- Masking can also show as the **disappearance of the receiver's own noise, which is typical of VHF** (an FM receiver captured by a carrier).
- Multi-tone FM jamming is heard as tones.

Against radio links, jammers use noise, AM, FM or PM, or broadcast distorted speech and unsettling music mixed with noise. Against data links, they retransmit pulses, imitate code messages or replay recorded traffic ([Palii 1989, USSR TT][palii-7]). Voice is the hardest to jam, because the ear picks a signal out of heavy noise. Intelligibility rides on the **300-2400 Hz band**, and an operator stops understanding at about **50 % loss of information** (same source).

Jamming works only where the jam-to-signal ratio at the receiver exceeds a **suppression coefficient («коэффициент подавления»)**, which defines a suppression zone ([Palii 1989, USSR TT][palii-8]). NATO's methods were massed, selective and concentrated-selective jamming, with "combing" jamming in some conditions ([Palii 1989, USSR TT][palii-24]).

### The prescribed response was procedural

The battery regulation makes the vehicle or section commander **train his crew to recognise kinds of jamming, and to report promptly and clearly their effect and the measures taken**. The battery commander assesses the type, the intensity and the jammers' coordinates ([BU-IX 1984, USSR OD][bu9]).

The radio manual then prescribes the steps ([RRS-1986, USSR OD][rrs]):

- **Log and report.** Operators log the presence and duration of interference and the interferers' call signs, and report them up the chain. The station log records each case of deliberate interference and its nature.
- **Spare frequency.** Spare frequencies exist precisely to keep links working under jamming. The **main station orders the move**, with the drill and its 1-2, 5 and 10 minute clock described above.
- **Second receiver.** A station with two receivers **opens a watch on the spare frequency without further command**, while keeping the main one.
- **Under strong interference:** a long call (the called station's sign said three times) is allowed for up to 3 minutes; **commands may be said twice**; the **best operators take important traffic**; and reception is duplicated.
- **Relay.** Traffic can be **relayed** through a relay node, or by asking another station to relay it (code ЗЗХ).

A post-Soviet text adds wire when radio is jammed ([studfile §4.3, Russia TT post-Soviet][studfile-43]).

The same regulation gives radars a graduated response ([BU-IX 1984, USSR OD][bu9]):

- weak noise: adjust gain and scale;
- strong noise: anti-jamming circuits and a change of frequency;
- when retuning fails: other operating modes;
- **when the jamming stops: return to the main frequency**.

It also carries a rule worth borrowing for the radios: **switching a station off to hide the jamming's effect from the enemy is forbidden**.

### Each set added its own fix

The **R-407** ([R-407 plate, USSR OD][r407plate]; [R-407 ТО, USSR OD][r407to]):

- Its instruction plate says to **switch АПЧ (automatic frequency control) off in strong interference**. Otherwise АПЧ pulls up to 15 kHz of detuning to within 3 kHz.
- Certain receive and transmit frequencies are barred.
- Duplex working needs 4 MHz between receiver and transmitter.
- Two sets at least 15 m apart can be cross-connected on their four-wire terminals to relay.
- Range climbs from at least 3 km on the whip to 10 km on a 5 m support and 20 km on an 11 m mast.

The **R-111** ([R-111 ТО, USSR OD][r111to]):

- Its **ЗАЩИТА ВХОДА** (input protection) lamp lights when a strong nearby transmitter trips the receiver's input protection.
- It resets by switching off and on (at ДЕЖ. ПР. or 1 %), or by a brief transmission (at 20 or 100 %).
- It relays automatically or by hand on a double set.
- At the limit of range, it is retuned by hand for maximum antenna current.

The **R-123M** works on its 4 m whip, on a lowered antenna, or on a **3 m emergency wire** thrown out of the hatch, good for about 4 km ([TPU lecture, Russia TT post-Soviet][tpu-94]; [Legia Akademicka, Poland TT post-Soviet][legia]).

Raising power or the mast as a deliberate anti-jamming step fits Palii's suppression-zone model. The documents, however, give power and mast only as range and quality controls (inference).

**Not found:**

- how the R-123M's noise suppressor (ШУМЫ) behaves under jamming, or how operators were told to use it;
- sweep jamming of VHF FM nets;
- the ASPD-U's behaviour under jamming.

### Soviet authors expected NATO to aim at troop air defence first

Palii names **troop air defence («войсковой ПВО») among the first targets** of US divisional intelligence and electronic-warfare battalions, which also fix its radars for strikes ([Palii 1989, USSR TT][palii-22]). His figures, read from a noisy OCR copy ([Palii 1989, USSR TT][palii-22]; [Palii 1989, USSR TT][palii-23]):

- **Brigade EW company:** jams **6-9 HF/VHF links at once**.
- **Divisional EW company:** jams 12 links at a depth of 15-20 km. Its jammers include:
  - the MLQ-34, covering **20-150 MHz, up to three nets per jammer**;
  - the TLQ-17A, covering 1.5-60 MHz;
  - the helicopter-borne **Quick Fix II, covering 2-76 MHz at up to 60 km**.
- **Corps group:** **15-18 jammers of 1-2 kW, reaching 30 km**.
- **Expendable jammer:** a VHF barrage jammer fired **six to a 155 mm shell**. It **jams for a few minutes over up to 20 km**.

Air forces' group jammers suppress **the radio communications that control SAMs and fighters** ([Palii 1989, USSR TT][palii-27]). US exercises showed jamming causing delays, missing information and confusion. In the close fight, the main jamming effort went against several targets, among them **the air defence of first-echelon regiments** ([Palii 1989, USSR TT][palii-25]). The MLQ-34's band covers the R-123M, the R-111 and the R-407 alike (inference).

The 1986 manual also warns that printing-telegraph (data) links carry more traffic but are **more vulnerable to interference** ([RRS-1986, USSR OD][rrs]). Under jamming, then, the regiment's telecode could arrive corrupted or false, not just absent (inference). Palii's account of the Bekaa Valley in June 1982 does not mention jamming of Syrian voice nets ([Palii 1989, USSR TT][palii-43]).

### Launchers fell back on their own sensors

Search is done **on the senior commander's designation, or independently («самостоятельно»)**, and sudden targets and helicopters are engaged independently ([BU-IX 1984, USSR OD][bu9]). An Osa battery fired on suddenly detected targets on its own, and reported afterwards ([wrk compilation, Russia RP][wrk]). The Osa can detect, identify and engage a target alone ([MON Uzbr. 2112/80, Poland OD][mon]).

**Not found:**

- an explicit Osa rule for losing the PU-12M's link;
- any NVA anti-jamming drill for the PU-12M, whose handbooks are not online.

## The Osa commander was the crew's only ear on the battery net

### Two R-123M, an R-124 and a TA-57, and no data link

Every operator-country source agrees on the Osa's communications fit. The Polish manual, marked JAWNE (open), lists ([MON Uzbr. 2112/80, Poland OD][mon]):

- **two R-123 radios for external communication**;
- **the R-124 for internal communication between crew members**;
- **a TA-57 telephone for a subscriber outside the vehicle at a halt**.

It places an R-123M at each front seat (driver front left, commander front right), and names the R-123M as the link to external sources of target data.

The NVA catalogue lists two R-123 and the R-124 ([rwd_mb3, GDR OD*][rwd-9a33]). The BAZ-5937 chassis brought the intercom and one R-123 of its own ([flak11, GDR OD*][flak-9a33-2]; [Korovin 2010, Russia RP][korovin]). So one set came with the chassis, and the second with the vehicle.

**There was no data link to the launcher.** Automated designation stopped at the PU-12M ([flak11, GDR OD*][flak-9k33-0]). The designer later asked for a telecode channel from the battery post into the combat vehicle ([Korovin 2010, Russia RP][korovin]), and a 2000 upgrade offered one ([flak11, GDR OD*][flak-9k33-2]).

### The R-124 lets only two boxes reach a radio

The R-124 has **five subscriber boxes, A-1 to A-5**, five helmets with laryngophones, and five cords with chest switches. **A-1 (the commander) and A-2 can go out on a radio; the rest are intercom only.** It gives at least 90 % word intelligibility at 120 dB of noise ([Wikipedia Р-124, Russia RP][wp-r124]).

On a two-radio vehicle the mode switches read ([BMP-2 lesson, Russia TT][r124-bmp2]):

- A-1: **ВЫКЛ., ВС, Р-123, Р-112**;
- A-2: **Р-112, ВС, Р-123**.

The chest switch has **one fixed position, ПРМ (receive, and intercom), and two sprung positions, ПРД (transmit on the external net) and ВЫЗ (call)** ([BMP-2 lesson, Russia TT][r124-bmp2]; [R-123M lesson, Russia TT][r123m-tr]). Pressing ВЫЗ pulls subscribers who are on the radio back into the intercom. Any subscriber can make a circular call by holding ВЫЗ ([R-124 text, Russia TT][r124-studop]).

The commander's panel powers the intercom, and A-1 sets its volume. Everyone on the intercom hears everyone, whatever the position of their chest switch ([KazNU 2017, Kazakhstan TT post-Soviet][kaznu]). In infantry fighting vehicles, a driver's box (ПВ) with ВС / РС positions lets the gunner put the driver on the radio, for instance for a water crossing ([R-123M lesson, Russia TT][r123m-tr]).

#### Which Osa crewman holds which box is not documented

**No source says which Osa crewman has which box.** The closest documented case is the ZSU-23-4, an air-defence vehicle of the same regiments ([TPU lecture, Russia TT post-Soviet][tpu-94]):

- A-1: commander;
- A-2: search operator-gunner;
- A-3: range operator;
- A-4: driver.

On the ZSU, the commander and the operator can reach the R-123, and their correspondents are the battery commander and the other ZSUs.

Inference for the Osa:

- **A-1:** the vehicle commander, beside his R-123M.
- **A-2:** the deputy commander and guidance operator. He is the NVA's "Stv. Kommandant", who also has a lock-on button ([flak11, GDR OD*][flak-9a33-3]).
- **A-3 to A-5:** the search operator, the range operator and the driver, in some order. The five boxes match the crew of five.

This produces the defining fact of the Osa's communications. **With A-1 switched to Р-123, the commander hears the net and not his crew until someone presses ВЫЗ.** Every designation therefore passes through him.

### From the PU-12M's call to the search operator's scope

The NVA describes the receiving end ([flak11, GDR OD*][flak-9a33-3]). **The commander receives the designation by radio from the PU-12M, as bearing and range.** The vehicle must be correctly oriented by its TNA-3. The senior operator (Oberfunkorter) then finds the target on the search radar's indicator and interrogates it for IFF.

A Russian compilation completes the cycle ([wrk compilation, Russia RP][wrk]):

1. The battery commander gives azimuth and range on the battery net.
2. The launchers switch on their radars and search.
3. They report lock-on and tracking.
4. The battery commander names the vehicle, or pair of vehicles, to fire.
5. The radars go off after the kill.

Sudden targets are engaged independently, with a report afterwards. Search with a designation takes **6-15 s**, and an experienced operator sees the new mark on the first sweep.

In the ZSU analogue ([TPU lecture, Russia TT post-Soviet][tpu-22]):

- the commander passes azimuth and elevation to the search operator;
- the platoon's designation gives azimuth, search sector and range;
- the range operator reports every 500 m;
- the time to report tracking counts as part of the firing-preparation time.

Acknowledgement follows the general rule ([SFU 2011, Russia TT post-Soviet][sfu]):

- the order of a transmission is: the called station's sign; «я» and own sign; the content; «я» and own sign; «приём»;
- a command is confirmed by exact repetition or by «понял»;
- commands to all units get no confirmation;
- on a steady link, no call signs are used.

NVA norm 22, **"Frequenzwechsel durch Kommandant" (frequency change by the commander), is 30, 35 or 45 s** by grade. Whether it means the R-123M's frequency or the radar's is not stated ([flak11, GDR OD*][flak-9k33-1]).

#### Duty life and crew titles

Duty life comes from one veteran, who served in an Osa unit in Poland covering the 20th Tank Division ([Ek32, USSR VA][ek32]). At night the command post's duty officer called each vehicle's number on the field telephone, almost in a whisper. Crews wore their headsets with the telephone handset tucked to the ear so as not to miss the check. Each morning came the order to take up the air-defence watch, then readiness No. 1.

Crew titles disagree ([wrk compilation, Russia RP][wrk]; [Ek32, USSR VA][ek32]; [flak11, GDR OD*][flak-9a33-3]):

- **Russian compilation:** the sergeant guidance operator is deputy crew chief under an officer commander. The battery's first vehicle is commanded by a captain who is also deputy battery commander.
- **Veteran:** a sergeant operator is the crew chief.
- **NVA:** commander, deputy commander, senior operator, operator and driver.

MANPADS gunners in the battery net could also give designations ([wrk compilation, Russia RP][wrk]).

**Not found:**

- the net of the Osa's second R-123M (the air-picture or warning net is the likeliest; inference);
- where the R-124 boxes sit on the Osa, and whether its driver has a ПВ;
- the battery's radio-exchange table and designation phrase;
- the Osa's antennas;
- any procedure for losing the link.

## Three markings decide what the project may lean on

### ДСП: restricted handling, not a secrecy grade

**«Для служебного пользования» (ДСП, "for official use")** is a Soviet restricted-handling marking. It is not a secrecy grade such as «секретно» or «совершенно секретно». Both 1984 combat regulations carry it:

- **Part X** (troop air-defence radar units) is posted by veterans at rtv-pvo-gsvg.narod.ru and mirrored elsewhere. It is already in `docs/SOURCES.md` as `pvo_bu_x_1984`, tier 1, with the ДСП marking noted.
- **Part IX** (gun batteries, print order «3229дсп») was posted at sprotyvg7.com.ua in 2023. It is used here but **not yet registered**. It needs the owner's decision, as the P-19 technical description did (`p19_to`: marked secret, publicly posted, allowed on 19 Oct 2026). Part IX covers Shilka and gun batteries, so for the Osa it is analogy.

The 1986 radio manual showed no marking in its extracted text. The Polish Osa manual is marked JAWNE (open).

### Ex-GVS GDR documents

The *Auskunftsbuch der Truppenluftabwehr 1985*, behind flak11's structure diagrams, was "ex GVS B 597030": formerly a Geheime Verschlusssache, GDR classified matter ([flak11, GDR OD*][flak-struktur]). Only those diagrams are used, as the veterans published them.

The NVA handbooks of 1980, 1984 and 1990 reach this report only through flak11's and rwd_mb3's reproductions. rwd_mb3 also cites NVA document numbers (K 050/3/016, A 327/1/412 and others) whose handling markings are unknown. The issuing state no longer exists and the material is historic, but the provenance should still be recorded.

### The restricted 1991 Voenizdat Osa textbook

The textbook `voenizdat_osa_1991` (*Автономный войсковой зенитный ракетный комплекс «Оса»*, on the 9K33M3) is copy-numbered. A marking above the copy number is blacked out, and its imprint numbers end in «с», which usually marks a secret edition. The owner allowed its use on 3 Oct 2026, for facts with page references only (`docs/SOURCES.md`).

**It was deliberately not read for this report**, which was limited to public sources. Search hits show it covers:

- the commander switching on and tuning the R-123M sets in the crew drill;
- call signs in battery orders;
- the radio receivers' blanking during the radar's pulses.

It likely answers several of the Osa questions left open here.

### The period's own signal documents

The period's covert-control documents (radio data, call-sign and signal tables, talk tables, frequency plans) were restricted in their day. None was sought. The samples used above are post-Soviet textbook illustrations and must stay labelled as placeholders.

## What SAM3D should build, label and leave open

Every recommendation serves the crew pillars:

- knowledge moves only through instruments and voice;
- every action has an owner seat;
- in first person, the player knows only what reaches him.

The PU-12M's headset-only design makes the first pillar literal: **what a crewman knows depends on which РСТ his box is set to.**

### Corrections to make now

Four data points contradict the stronger sources:

1. **Swap the places of FuG-2 and FuG-4** in `docs/research/PU12_RESEARCH_2026-10.md`, `data/procedures/pu12m.json` and `data/cabins/pu12m.json`. The NVA drawing puts the covered unit's set beside the commander, and the battery-net set at the radio operator's table.
2. **Remove the R-124 from the PU-12M's equipment line** in `docs/PU12_DEVELOPMENT.md`, since the boxes are PVS, and write RK-1U, not RK-1V.
3. **Record both net readings in the radio data**: the NVA's (R-111 = regiment command, R-407 = telecode) as the one used, and the ČSLA's as the recorded alternative, per the rule on disagreeing sources.
4. **Review the mils in the designation callout** (`simcore/include/simcore/pu12.hpp`, `docs/crew/04_COMMANDS_AND_CALLOUTS.md`). The sources found voice battery-to-launcher azimuths in degrees. Either switch to degrees, or keep mils with the disagreement recorded and the choice labelled as SAM3D's.

### Build as sourced

| What to model | Source |
|---|---|
| PU-12M intercom: RTK-1, PVS-1/2/3, RK-1U, two RK-3U; four headsets; no loudspeaker; boxes per seat as in the table above (driver PVS-3) | flak11, rwd_mb3 |
| RTK-1: transmit on four radios, receive on two, five line levers with lamps, ПВС-1/2/3 and ЦИРКУЛЯР calls, acoustic signal. PVS: four radios and calls to the other boxes. Panel labels are medium confidence | rwd_mb3 (OD\*, PH) |
| RK-1U data path: ПЕРЕДАЧА and ПРИЕМ, each ВЫКЛ / R-123 / R-111 / R-407 / ПРОВОД, with С-23 ↔ АСПД-У toggles (medium confidence) | rwd_mb3 (PH) |
| Five nets per the NVA table; FuG-2 on 20-27 MHz and FuG-4 on 37-51.5 MHz behind one diplexer | flak11 |
| A main station per net (the PU-12M on FuG-4); call, answer, confirm; the 3-minute no-answer rule with three repeats, then a report; checks only on order; a password on new data | RRS-1986 |
| Read-back or «Понял» for commands; circular commands said twice without receipt; signals said twice and receipted by one repeat; signal and warning traffic first | RRS-1986 |
| Scheduled changes with no word on air; the spare-frequency drill on the main station's order, with read-back, simultaneous retune, new call signs and the 1-2 / 5 / 10 minute fallbacks | RRS-1986 |
| Duty reception forbids transmitting, radiating tuning and frequency changes; R-123M 3 minutes back to СИМПЛЕКС and a 10-minute warm-up; R-111 power at 1/20/100 % and 1.5-3 minutes for the cathodes | RRS-1986, KazNU 2017, R-111 ТО |
| R-123M: four presets, change in 15 s or less. R-111: 45 s or less | helpiks ch. 9, SFU 2011, studopedia |
| Open text for air data, designation, fire commands and warnings; units, officials and places coded; positions by PVO-grid squares or by the automated systems | BU-X, BU-IX |
| Radio silence as transmit-inhibit with sets on duty reception, air defence and the warning net exempt; emission by readiness level, set by the battery commander | VST-1988, RRS-1986, BU-IX |
| Jamming: log and report; the main station orders the spare; commands said twice; relay; a second-receiver watch; R-407 АПЧ off; R-111 ЗАЩИТА ВХОДА and its co-site rule | RRS-1986, BU-IX, R-407 plate, R-111 ТО |
| Osa: two R-123M; R-124 (only A-1 and A-2 reach a radio); chest switch ПРМ / ПРД / ВЫЗ; TA-57 at a halt; no data link; radio range 12 km with the radar on, 15 km with it off | MON 2112/80, Wikipedia Р-124, BMP-2 lesson, flak11 |
| Designation to launchers by voice as bearing and range, with errors up to 5° and 3,000 m; independent fire on sudden targets, with a report afterwards | flak11, wrk, BU-IX |
| The plotter's voice report form: number, place, identity and composition, height in hm, minute of the hour; «00» for unknown; targets 01-99 | Dedyaev 2010 (post-Soviet TT, teaching Soviet PVO procedure) |

Several of these already exist in `simcore/radio_sets.hpp` and the Osa procedures: the R-123M's 3-minute return to СИМПЛЕКС, the R-407's АПЧ, and the R-111's ЗАЩИТА ВХОДА. **The main-station procedure and the spare-frequency drill with its timers do not exist yet.** They are the highest-value addition, because they give jamming an owner seat, a callout and a clock.

### Build as labelled inference

| SAM3D choice | Basis |
|---|---|
| РСТ-n = FuG-n; a ВС (intercom) position on the PVS selector | the NVA set numbering; R-124 and R-145BM analogy |
| The plotter wears a second headset on PVS-1, kept on РСТ-1. Alternatives: he takes the RTK-1 when the commander is out, or the commander relays to him | the board stands between PVS-1 and the RTK-1 |
| PU-12M intercom behaviour: a busy lamp per set; one shared intercom net; ЦИРКУЛЯР cuts in; the box returns to intercom after transmitting; a line call is a lamp plus a bell | R-145BM analogue |
| Osa boxes: A-1 the commander; A-2 the deputy commander and guidance operator; A-3 to A-5 the others; the driver has a ПВ (ВС / РС) | ZSU-23-4 analogue; the NVA lock-on button |
| The Osa's second R-123M sits on the air-picture or warning net (FuG-1) | the "ZZW Nr. 2" naming; no source |
| R-123M presets: 1 and 2 are the battery net's working and spare frequencies; 3 and 4 are the second net's | radio data give working and spare frequencies per net |
| A designation phrase built on the general rules, e.g. «Осина-3, я 10, цель 28, азимут 135, дальность 21, высота 12, приём», with a read-back; word-plus-number call signs | the RRS-1986 form; the post-Soviet call-sign example |
| Warning and readiness codes («Воздух», «Отбой», three-digit groups) as per-scenario placeholders | post-Soviet samples, which disagree with each other |
| Source switching as an owned action at the RK-1U, with a callout saying which picture is on the IT-45. Order of preference: R-407 telecode, then wire, then R-123, then a cabled radar, then voice | flak11, helpiks, studfile |
| Detectability rising with duration × power, with about 30 s to detection, feeding the air war's picture rather than producing instant artillery | RRS-1986, Palii, one tier-4 lesson |
| Jamming scenarios: three nets per MLQ-34; a 155 mm expendable jammer near the battery; telecode corrupted or false under jamming. EMCON and jamming as separate states | Palii, RRS-1986 |
| Launchers jammed off the battery net fall back to autonomous search and visual watch | BU-IX "independently" |
| Line subscribers: the coupled radar, a co-located command post, an observation post | the 500 m deployment norm |

### Open questions and where to look

| Question | Best lead |
|---|---|
| Degrees or mils for designations; the exact designation phrase and the launchers' reports | NVA *Handbuch für Fla-Einheiten* (1990); `voenizdat_osa_1991` under the owner's exception |
| Which Osa crewman holds which R-124 box; whether the driver has a ПВ; the second R-123M's net | `voenizdat_osa_1991`; the Osa vehicle's ТО; interior photographs |
| The PVS selector positions, the centre button, the RTK-1's rotary and two knobs, and what "receive on two radios" means | an RTK-1/PVS ТО (not found); higher-resolution museum photographs |
| The plotter's headset in the PU-12M | NVA *Handbuch für Planzeichner und Funkorter der Truppenluftabwehr* (1980) |
| Where the five lines went, who laid them, the cable and reel, the TA-57 count | ČSLA *Del-25-53/1* (1982); NVA handbooks |
| ČSLA versus NVA for the R-111 and R-407; whether the 1980s PU-12M took telecode on an R-123 | ČSLA *PVOV-2-3 Bojová služba u PU-12(M)* |
| The 1980s warning code words, the talk-table format, the ASPD-U's two-digit command table | not expected in public sources |
| The meaning of NVA norm 22; which readiness level puts nets on duty reception and which on simplex | flak11's norm tables; the combat-regulation part for SAM batteries (not found) |
| The ASPD-U and the R-123M's ШУМЫ under jamming; a loss-of-link rule for the Osa; any battery-to-battery exchange | the R-123M ТО; NVA handbooks |

## Conclusion

The battery's most consequential link is its shortest one. Several things meet on the voice hop from the PU-12M to the four launchers:

- automation stopped there;
- doctrine allowed speech in clear there;
- each 9A33BM2's commander became his crew's only ear there;
- Soviet writers expected NATO's jammers to strike there first.

That convergence makes radio procedure the core of the battery's teamwork rather than flavour. A sourced main-station drill, with its 1-2, 5 and 10 minute clock, gives every jamming event an owner, a callout and a measurable cost in lost seconds. That is the interdependence SAM3D's crew pillars ask for. It also keeps emission control and jamming as separate states: one chosen by the commander, the other imposed by the enemy.

The record also draws a clear line for fidelity. The general rules (the 1986 manual and the 1984 regulations) are well sourced. So is the equipment (the R-111, R-407 and R-124 manuals, and the NVA catalogue and drawings). The Osa's own words are not, and the post-Soviet samples that fill the gap disagree with each other. More public searching is unlikely to move that line. Three documents would:

- the NVA's 1990 *Handbuch für Fla-Einheiten*;
- the NVA's 1980 *Handbuch für Planzeichner und Funkorter*;
- under the owner's existing exception, the 1991 Voenizdat textbook.

Until one of them is read, SAM3D should ship the sourced procedure, keep every Osa phrase labelled as its own construction, and treat azimuth units as an open decision.

## Appendix: source register

| Key | Source | Country, type | Notes |
|---|---|---|---|
| RRS-1986 | *Руководство по радиосвязи ВС СССР, ч. 2: Правила радиосвязи*, MoD USSR, Voenizdat 1986 (General Staff directive 177/24/0945, 19 May 1986) | USSR, OD | applies across the armed forces; no marking in the extracted text; mirror at numbers-stations.com |
| BU-IX 1984 | *Боевой устав Войск ПВО, ч. IX* (AA gun battery, platoon and crew of troop air defence), Voenizdat 1984, order No. 093 of 5 Aug 1983 | USSR, OD | ДСП; covers gun and ZSU batteries; not yet in `docs/SOURCES.md` |
| BU-X 1984 | *Боевой устав Войск ПВО, ч. X* (radar company/battery, platoon and crew of troop air defence), Voenizdat 1984 | USSR, OD | ДСП; registered as `pvo_bu_x_1984` |
| VST-1988 | *Словарь военных терминов*, Plekhov and Shapkin, Voenizdat 1988 | USSR, OD | entries for radio silence, radio data, radio-electronic protection |
| KST-1958 | *Краткий словарь оперативно-тактических и общевоенных терминов*, 1958 | USSR, OD | before the period |
| Palii 1989 | A. I. Palii, *Радиоэлектронная борьба*, Voenizdat 1989 | USSR, TT | noisy OCR; figures used only where legible |
| BSE | *Большая советская энциклопедия*, 3rd ed., «Радиолокационные помехи» | USSR, RP | |
| R-111 ТО | R-111 technical description and operating instruction | USSR, OD | read for SAM3D on 19 Oct 2026 (`docs/research/PU12_RADIOS_2026-10.md`) |
| R-407 plate and ТО | the R-407's lid instruction plate; ТО ИП1.201.038 (1982) as transcribed | USSR, OD | as above |
| MON Uzbr. 2112/80 | *Przeciwlotniczy zestaw rakietowy 9K33M2 OSA-AK*, MON, Warszawa 1981 | Poland, OD | marked JAWNE; local copy in `reference/` |
| flak11 | R. Wagner and A. Zerbst, flak11.de (2003-2015), on the NVA handbooks of 1980, 1984 and 1990 and the *Auskunftsbuch der TLA 1985* | GDR, OD\* | plain http only; the Auskunftsbuch is ex-GVS |
| rwd_mb3 | Technikkatalog, Raketen- und Waffentechnischer Dienst im Kdo. MB III, with museum photographs | GDR, OD\* (photographs: PH) | cites NVA K 050/3/016 and others |
| valka | valka.cz topic 23991, on the ČSLA *PVOV-2-3* (TMNO 24-65-86) and *Del-25-53/1* (1982) | ČSSR, OD\* | forum compilation |
| buko1 | valka.cz thread 146145, a former PU-12 commander | Slovakia, VA | known only through the researchers' summary of his post |
| Chizhikov | S. Chizhikov, shilka.narod.ru (2001), Berdyansk training centre, 1989 | USSR, VA | |
| Ek32 | forums.airbase.ru (2012), an Osa unit in the Northern Group of Forces | USSR, VA | years and post not stated |
| wrk | «2ПВО МСД. Часть 2. Зенитные ракетные полки», wrk.ru attachment (2018) | Russia, RP | compiles a regimental brochure, histories and veterans' accounts |
| Korovin 2010 | V. Korovin, «ЗРК Оса», *Техника и вооружение* 2010 | Russia, RP | history article |
| helpiks PU-12 | «Общие сведения о ПУ-12 и ППРУ 9С80» (2016) | Russia, TT post-Soviet | describes the later BTR-80 build |
| studfile §4.3-4.4 | a troop air-defence tactics textbook, «0001.doc» | Russia, TT post-Soviet | partial preview only |
| cadet guide | «Справочник для подготовки 3 курс» (sample orders dated 1995) | Russia, TT post-Soviet | |
| Andryushchenko 2004 | V. A. Andryushchenko and P. A. Pirozhkov, *Командно-штабные машины*, TGTU, Tambov 2004 | Russia, TT post-Soviet | the R-145BM analogue |
| Dedyaev 2010 | V. N. Dedyaev, *Тактико-специальная подготовка*, BGUIR 2010 | Belarus, TT post-Soviet | the PVO report form |
| Dmitryuk 2012 | A. M. Dmitryuk et al., *Основы организации связи*, BGUIR 2012 | Belarus, TT post-Soviet | |
| SFU 2011 | E. A. Drabatulin and Yu. B. Bayramukov, *Общая тактика. Управление подразделениями в бою*, SFU 2011 | Russia, TT post-Soviet | |
| KazNU 2017 | M. R. Toekin, «ЗСУ-23-4 Шилка», ch. 16, KazNU 2017 | Kazakhstan, TT post-Soviet | read through a web.archive.org copy from 2021 |
| TPU lecture | A. A. Gavrilov, Tomsk Polytechnic University military training centre, ZSU courses | Russia, TT post-Soviet | |
| bstudy 13.1-13.3 | *Общая тактика*, ch. 13 (examples dated 1999) | Russia, TT post-Soviet | the talk-table, signal-table and call-sign samples |
| studopedia | radio-operator course pages (2015), restating RRS-1986 | Russia, TT post-Soviet | |
| helpiks ch. 9 | «Подготовка по связи», ch. 9 (2015) | Russia, TT post-Soviet | R-123M and R-111 operating notes |
| lesson T2 | «Правила ведения переговоров…», rykovodstvo.ru | Russia, TT post-Soviet | the 30 s detection claim |
| Kostroma lecture | «Связь», Kostroma State University (2015) | Russia, TT post-Soviet | company-level practice |
| BUSV ch. 10 | *Боевой устав сухопутных войск*, ch. 10 | Russia, OD | edition unknown, probably post-Soviet; corroboration only |
| BMP-2 lesson, R-124 text, R-123M lesson | studfile and studopediasu pages | Russia, TT | R-124 and R-123M operation |
| Legia Akademicka | *Dowódca drużyny*, part 4 (2017 or later) | Poland, TT post-Soviet | |
| Wikipedia Р-124, ТА-57, R-123, Igla | Russian and English Wikipedia | Russia and international, RP | the Igla passage is unsourced |
| armforc | A. K. Lysochenko, «ПУ-12 (9С482)», armforc (2011) | Russia, RP | |
| pvo.guns.ru | «ЗРК Стрела-10» (2000) | Russia, RP | |
| Opisy Broni | «BTR-60 PU12» (2020) | Poland, RP | several points hedged by the page itself |
| zmei | «ЗРК Оса в ВСУ. 1129 ЗРП» (2015) | Ukraine, RP | post-Soviet state of the regiment |
| fsrbit | «Криптографический фронт» museum catalogue | Russia, RP (archive catalogue) | |

[flak-pu0]: http://www.flak11.de/PU12-0.htm
[flak-pu1]: http://www.flak11.de/PU12-1.htm
[flak-9s482]: http://flak11.de/9S482.htm
[flak-struktur]: http://www.flak11.de/TLA-struktur.html
[flak-9a33-2]: http://www.flak11.de/9A33-2.htm
[flak-9a33-3]: http://www.flak11.de/9A33-3.htm
[flak-9k33-0]: http://www.flak11.de/9K33-0.htm
[flak-9k33-1]: http://www.flak11.de/9K33-1.htm
[flak-9k33-2]: http://www.flak11.de/9K33-2.htm
[rwd-pu12]: https://www.rwd-mb3.de/pages/pu12.htm
[rwd-pu12b]: https://www.rwd-mb3.de/pages/pu12b.htm
[rwd-pu12f]: https://www.rwd-mb3.de/pages/pu12f.htm
[rwd-pu12g]: https://www.rwd-mb3.de/pages/pu12g.htm
[rwd-9a33]: https://www.rwd-mb3.de/pages/9a33.htm
[valka-pu12]: https://www.valka.cz/SOV-PU-12-PU-12M-velitelske-stanoviste-t23991
[valka-buko1]: https://www.valka.cz/topic/view/146145/BTR-60-PU-12-ziadost-o-dokumentaciu
[armforc]: https://armforc.su/srk_pu12.htm
[shilka-pu12]: https://shilka.narod.ru/pu_12.htm
[shilka-ovod]: https://shilka.narod.ru/ovod.htm
[helpiks-pu12]: https://helpiks.org/8-37305.html
[studfile-43]: https://studfile.net/preview/9392907/page:21/
[studfile-44]: https://studfile.net/preview/9392907/page:22/
[studfile-sprav]: https://studfile.net/preview/9478360/page:10/
[andr-24]: https://vuzmen.com/book/40-komandno-shtabnye-mashiny-va-andryushhenko/18-24-ustrojstvo-i-rabota-sostavnyx-chastej-kshm.html
[andr-33]: https://vuzmen.com/book/40-komandno-shtabnye-mashiny-va-andryushhenko/25-33-ustrojstvo-i-rabota-kommutacionnoj-apparatury.html
[andr-34]: https://vuzmen.com/book/40-komandno-shtabnye-mashiny-va-andryushhenko/26-34-ustrojstvo-sostavnyx-chastej-kommutacionnoj-apparatury.html
[andr-37]: https://vuzmen.com/book/40-komandno-shtabnye-mashiny-va-andryushhenko/29-37-poryadok-raboty.html
[opisy]: https://opisybroni.pl/btr-60-pu12/
[wp-r124]: https://ru.wikipedia.org/wiki/%D0%A0-124
[wp-ta57]: https://ru.wikipedia.org/wiki/%D0%A2%D0%90-57
[wp-r123]: https://en.wikipedia.org/wiki/R-123
[wp-igla]: <https://ru.wikipedia.org/wiki/Игла_(переносной_зенитный_ракетный_комплекс)>
[fsrbit]: https://museum.fsrbit.ru/docs
[bu9]: https://sprotyvg7.com.ua/wp-content/uploads/2023/08/%D0%91%D0%BE%D0%B5%D0%B2%D0%BE%D0%B9_%D1%83%D1%81%D1%82%D0%B0%D0%B2_%D0%B2%D0%BE%D0%B9%D1%81%D0%BA_%D0%BF%D1%80%D0%BE%D1%82%D0%B8%D0%B2%D0%BE%D0%B2%D0%BE%D0%B7%D0%B4%D1%83%D1%88%D0%BD%D0%BE%D0%B9_%D0%BE%D0%B1%D0%BE%D1%80%D0%BE%D0%BD%D1%8B_%D1%87_9_%D0%B7%D0%B5%D0%BD%D0%B8%D1%82%D0%BD%D0%B0%D1%8F_%D0%B0%D1%80%D1%82%D0%B1%D0%B0%D1%82%D0%B0%D1%80%D0%B5%D1%8F_%D1%80%D0%BE%D0%B7%D0%BF%D1%96%D0%B7%D0%BD%D0%B0%D0%BD%D0%B8%D0%B9.pdf
[bu10]: http://rtv-pvo-gsvg.narod.ru/doc/combat_statut.pdf
[rrs]: https://www.numbersoddities.nl/Rukovodstvo-Radiosvazi-SSSR.pdf
[bguir]: http://rtv-pvo-gsvg.narod.ru/doc/605322_12CB0_dedyaev_v_n_taktiko_specialnaya_podgotovka.pdf
[dmitryuk]: https://libeldoc.bsuir.by/bitstream/123456789/3870/2/Dmitryuk_osn.pdf
[studop-tdr]: https://studopedia.org/8-101784.html
[studop-r111]: https://studopedia.org/8-101803.html
[helpiks-rhbz]: https://helpiks.org/4-4377.html
[bstudy-131]: https://bstudy.net/968536/bzhd/osnovnye_printsipy_skrytogo_upravleniya_voyskami
[bstudy-132]: https://bstudy.net/968537/bzhd/dokumenty_skrytogo_upravleniya_voyskami_pravila_polzovaniya
[bstudy-133]: https://bstudy.net/968538/bzhd/obschie_pravila_vedeniya_peregovorov_otkrytym_kanalam_svyazi
[ryk-t2]: https://rykovodstvo.ru/exspl/2631/index.html?page=3
[pvoguns-s10]: https://pvo.guns.ru/strela10/strela10_01.htm
[zmei]: https://zmei-cvdk.livejournal.com/1073.html
[vst-rm]: https://military_terms.academic.ru/2011/%D0%A0%D0%B0%D0%B4%D0%B8%D0%BE%D0%BC%D0%BE%D0%BB%D1%87%D0%B0%D0%BD%D0%B8%D0%B5
[vst-rez]: https://military_terms.academic.ru/2039/
[vst-rd]: https://military_terms.academic.ru/1996/
[kst]: https://tactical_terms.academic.ru/1175/%D0%A0%D0%B0%D0%B4%D0%B8%D0%BE%D0%BC%D0%BE%D0%BB%D1%87%D0%B0%D0%BD%D0%B8%D0%B5
[palii-3]: https://studizba.com/show/1083413-3-paliy-a-i-radioelektronnaya-borba-m.html
[palii-7]: https://studizba.com/show/1083413-7-paliy-a-i-radioelektronnaya-borba-m.html
[palii-8]: https://studizba.com/show/1083413-8-paliy-a-i-radioelektronnaya-borba-m.html
[palii-22]: https://studizba.com/show/1083413-22-paliy-a-i-radioelektronnaya-borba-m.html
[palii-23]: https://studizba.com/show/1083413-23-paliy-a-i-radioelektronnaya-borba-m.html
[palii-24]: https://studizba.com/show/1083413-24-paliy-a-i-radioelektronnaya-borba-m.html
[palii-25]: https://studizba.com/show/1083413-25-paliy-a-i-radioelektronnaya-borba-m.html
[palii-27]: https://studizba.com/show/1083413-27-paliy-a-i-radioelektronnaya-borba-m.html
[palii-43]: https://studizba.com/show/1083413-43-paliy-a-i-radioelektronnaya-borba-m.html
[bse]: https://booksite.ru/FullText/1/001/008/094/939.htm
[r111to]: https://radiotract.ru/radiostancia-r-111-tehnicheskoe-opisanie-i-instrukcia-1.html
[r407plate]: https://rw6ase.narod.ru/index1/pr_per/rs_spec/r407.html
[r407to]: https://www.radionic.ru/node/56733
[tpu-94]: https://portal.tpu.ru/SHARED/g/GAA63/educational_activity/ust_zsy234/mat_disc1/t_9.4.pdf
[tpu-22]: https://portal.tpu.ru/SHARED/g/GAA63/educational_activity/str_zsy234/mat_disc/t-2.2.pdf
[kgu]: https://studfile.net/preview/4451088/page:2/
[busv]: https://studfile.net/preview/855933/page:5/
[mon]: <file:///C:/dev/SAM3D/reference/sources/AntiaircraftOSA-AK-9K32M2rocketkit.pdf>
[wrk]: https://www.wrk.ru/forums/attachment.php?item=1005313
[korovin]: http://www.rta-4.de/bilder/OSA_Entw_geschichte_RU.pdf
[sfu]: http://vii.sfu-kras.ru/images/libs/uprav_podrasboy.pdf
[kaznu]: https://www.kaznu.kz/Content/%D0%97%D0%A1%D0%A3-23-4%20%D0%A8%D0%B8%D0%BB%D0%BA%D0%B0/page16.html
[r124-bmp2]: https://studfile.net/preview/10023919/page:8/
[r124-studop]: https://studopediasu.com/4_19804_tankovoe-peregovornoe-ustroystvo-r-.html
[r123m-tr]: https://studfile.net/preview/6724081/page:84/
[legia]: https://inob.uws.edu.pl/images/Legia_Akademicka/DcaDruzynyPart4.pdf
[ek32]: https://forums.airbase.ru/2014/04/t83098_3--istoriya-razrabotki-zrk-osa.html
