# The plotting board, the ASPD-U and the IT-45 (October 2026)

Why: the plotting board came into SAM3D from SAM Simulator (`samsim_osa` p20), where one player works the whole Osa and
reads the PU-12's board for his targets. SAM3D had carried that over: every Osa seat could open the board (`B`), and the
Osa's procedures said "watch the plotting board". This note records what the operator-country sources say the board
was, who used it, how and when; checks the PU-12M's ASPD-U and IT-45 against them; and says what changed in the sim and
what is proposed. Sources are in `docs/SOURCES.md`.

## What the sources say

### The board

| Fact | Source | Tier |
|---|---|---|
| The PU-12 / PU-12M carries an air-situation map (Luftlagekarte) among its equipment, beside the IT boards | `rwd_mb3` (pu12, after K 050/3/016) | 1 |
| Its purpose: to plot the tracks of air targets **in the squares of the designation grid about the radar's position**, and to take the targets' **azimuth and slant range about the post's own position** off it | `rwd_mb3` (pu12e, after regulation A 372/1/412) | 1 |
| The PU-12 has a plotter's place (Platz des Planzeichners) beside the commander-operator's place at the tube | `rwd_mb3` (pu12f) | 1 |
| The post's FuG-1 (an R-123, 15-20 km) works the **air-picture net (designation net no. 2, the plotters)** | `flak11` (PU12-1, after the NVA handbooks) | 1 |
| The plotter is responsible for clear, timely and correct display of the air situation; he prepares the board, knows the symbols, the target numbering and the signals; **takes the signals and reports on targets from the radio operator and plots them**; reports targets' appearance, actions, identity and size, jamming, and targets leaving cover or lost | `pvo_bu_x_1984` art. 23 | 1 |
| The **operator (operator-plotter)**, the PU-12's own title for No. 2, has wider duties than the plotter: knows the equipment, its modes and its protection against jamming and anti-radiation weapons, keeps it ready, checks and services it and readies the station for work; searches, detects, identifies and tracks targets and **reports their coordinates, characteristics and motion, and the type and strength of jamming, to the station chief (squad leader)**; knows the radio procedure and works the radio and the intercom; stands ready to replace the station chief | `pvo_bu_x_1984` art. 20 | 1 |
| Every crew member: knows the mission and the warning, control and cooperation signals; takes and leaves the position, readies the equipment; **keeps watch on the air and ground enemy and reports him to the commander at once**; tells own aircraft from the enemy's by sight; uses ground, cover and protection, camouflages, digs in; self- and mutual aid; repairs the vehicle when damaged | `pvo_bu_x_1984` art. 19 | 1 |
| The commander alone assesses the air situation and **makes the decisions**; he directly leads the post's operators (operator-plotters), shares the targets among them and sets and checks the order in which they give the air data; he commands by voice, by orders, commands and signals | `pvo_bu_x_1984` art. 16-17 | 1 |
| A troop air-defence radar company's mobile post holds the ASPD's remote indicators **and the boards of the general air situation and of each radar** | `pvo_bu_x_1984` art. 26 | 1 |
| The designation (warning) nets carry the air enemy to the command posts and the AA units; target data may be passed in clear; positions go by the automated systems or by **coded squares of the PVO grid**; sector boundaries are drawn on the indicators and the boards | `pvo_bu_x_1984` art. 29-30, 37 | 1 |
| The automated chain ends at the PU-12; from there designations go to the launchers **by voice, azimuth and range** | `flak11` (PU12-0) | 1 |
| The air-situation report: number, place (PVO grid square, or azimuth in degrees and range in km), identity and size as a two-digit index, height in hectometres, time of the fix in minutes of the hour. New targets are marked "00" before the first report. Reports at up to two minutes apart carry the height at least every fifth time | `bguir_tsp` | 2 |
| A non-automated report from the radar operator reaches the unit's post within 1 min (the higher posts within 2 and 3 min) | `bguir_tsp` | 2 |
| Tracks are drawn as a cross a fix, labelled with the minute (40, 41, 42...), the number, the index, height and speed at the head | `bguir_tsp` (table 2) | 2 |
| The plotters stand behind the glass and write in mirror image so the officers in front read it the right way round; grease pencil on edge-lit Piacryl | `planchette_dewiki`, `rwd_mb3` (llk72), `narodna_armiya_2018` | 4 |
| Information to the plotters every 1-2 min; the boards kept as the back-up of the automated system | `narodna_armiya_2018` | 4 |
| NVA plotters were conscripts; trained from the *Handbuch für Planzeichner und Funkorter der Truppenluftabwehr* (1980) | `flak11`, NVA veterans' pages | 1 (handbook to acquire: offered as an original, below) |
| In the troops the ASPD-U was **rarely switched on** (no real situation to use it for) and not used at platoon firings | `shilka_narod` | 2 |

### Who used it, when

* **Where it is:** in the battery's PU-12M (and in the radar companies' and higher posts). Not in the Osa: the 9A33BM2
  has no board. The Osa crew hears the battery commander's designation (azimuth and range) on the R-123M.
* **Who writes it:** the operator-plotter, from what he hears on the air-picture net (`pvo_bu_x_1984` art. 23: he
  takes the reports on targets and plots them). Not the tube: it is the commander's own (`rwd_mb3` names him the
  commander-operator), and the board is the back-up when it is dark. **Who reads it:** the battery commander.
* **What for:** the air picture beyond the tube. The IT-45 shows 35-100 km; the board carries the regiment's picture
  as far as its radars reach (200 km rings in SAM Simulator). It tells the commander what is coming, from where, how
  high, how many and when, before anything is inside a launcher's 10 km zone: the battery's early warning.
* **When:** whenever the battery is deployed and the net is up. In peacetime, training and range days most of all,
  because the ASPD-U was seldom on (`shilka_narod`). Automation did not retire it: today's Ukrainian radio-technical
  troops keep their boards as the back-up (`narodna_armiya_2018`).

### The plotter's other duties, and his part in decisions (20 Oct 2026)

Part X of the regulations (`pvo_bu_x_1984`) is written for radar companies, so for the PU-12M it is an analogy, not the
post's own manual. It gives the role three layers (table above):

* **The plotter** (art. 23): the board, and speaking: he reports targets appearing, their actions, identity and size,
  jamming, and targets leaving cover or lost.
* **The operator (operator-plotter)** (art. 20), the PU-12's title for No. 2 (`ru_pu12_text`; the troop air-defence
  tactics text on [studfile](https://studfile.net/preview/9392907/page:22/) lists the same crew): a radar operator who
  also plots. He keeps and services the equipment, searches and tracks, reports to the squad leader, works the radio and
  the intercom, and is the squad leader's stand-in. The ČSLA title *pátrač-kreslič* (searcher-draughtsman) says the same.
* **Every crew member** (art. 19): watch for the enemy and report him at once, recognise aircraft by sight, take and
  leave the position, camouflage, dig in, repair.

**Decisions:** none of the sources give him a part in them. The commander assesses the situation and decides
(art. 17); the operator-plotter's voice is his reports, which the commander decides on. Nothing found has him advise or
propose. The NVA plotters' handbook may say more.

**For SAM3D (proposed, not built):** the plotter speaks the art. 23 reports aloud to the commander (a new target on the
board, jamming reported on the net, a target lost), not only writes them; as the operator of art. 20 he can stand in for
the squad leader; and the art. 19 duties (watching, camouflage, digging in) are the whole crew's at deployment.

**The NVA handbook:** no scan online. The NVA book dealer [nva.4mg.com](http://nva.4mg.com/shopbaus.htm) lists
*Handbuch für Funkorter und Planzeichner der Truppenluftabwehr* (order no. 11065H, €49.95, marked as an original, stock
low; seen 20 Oct 2026). An older *Handbuch für Aufklärer, Planzeichner, E-Messer der Truppenluftabwehr* (Militärverlag
der DDR, 1972) was on [booklooker](https://www.booklooker.de/B%C3%BCcher/Ministerium-f%C3%BCr-Nationale-Verteidigung-der-DDR+Handbuch-f%C3%BCr-Aufkl%C3%A4rer-Planzeichner-E-Messer-der/id/A02hXdZE01ZZV)
at €85.

### The identity digit: a conflict

* SAM Simulator's index (`samsim_osa` p20): 0 jamming, 1 friend, 2 identified, 3 border violator, 4 supervisor, 5 own
  target, 6 rule violator, 7 practice, 8 enemy, 9 no IFF.
* `bguir_tsp`'s worked examples: 7 in ordinary reports (meaning not given), and **2** for "учебная воздушная цель" in
  the reports of an exercise.
* A second Belarusian text of the same department (*Специальная подготовка*, Dedyaev, BGUIR 2010, on the same veterans'
  site) lists the ten "main indices" in order (own fighter; with an IFF reply; foreign aircraft over the border; control
  aircraft; own aircraft on request; own aircraft breaking the flying rules; training target; air enemy; no IFF reply;
  jammer) but every digit is printed "00". The order is SAM Simulator's table exactly, which puts the training target 7th.
* **Decision (project owner, 19 Oct 2026): the training target is 2**, `bguir_tsp`'s exercise index. 7 is left
  unassigned. The order of the second text argues for 7; if the table itself turns up, revisit.

### The P-19's technical description

The P-19 radar's technical description (*РЛС П-19Ш, Техническое описание ЕИ1.001.015 ТО/с*, `p19_to`), found on the same
veterans' site, is marked **secret** (ТО/с). **Used by the project owner's decision of 19 Oct 2026** (CLAUDE.md, the data
rules). From it: the P-19's built-in ASPD (ХЛ2.390.012) has scales of 40, 50 and 100 km; it sends at 234 or 468 baud in
messages of 32 or 40 bits, once or repeated; it passes target coordinates in steps of 500 m (40 and 50 km scales) and
1 000 m (100 km); the operator takes the coordinates off the ASPD's indicator by hand; the indicator shows the primary
picture, the received secondary one, control points, the electronic marker, the check-back of a sent mark and the
received target's symbols; the antenna turns at 6 or 12 rpm; the P-19 is the 1RL134. SAM3D uses the 1 000 m step
(`telecode_step_m`), the semi-automatic take-off, the 10 s turn (`telecode_period_s`, `radar_period_s`) and the index
(the primary radar switch's position).

## The ASPD-U and the IT-45 against the sources

| Item | Sources | SAM3D before | Verdict |
|---|---|---|---|
| The IT-45 is the commander's, a repeater PPI ("Tochtersichtgerät") of the primary (cabled radar) or secondary (telecode) picture | `flak11`, `rwd_mb3` pu12a/pu12f | Yes | Correct |
| Scales | `flak11` 50/100; `ru_pu12_text` 35/50/100; the ФР block's three positions | 35/50/100 | Correct |
| 100 targets, numbers 00-99, H1-H3, height in 0.2 km, speed vector in 12.5 m/s, friend/foe, group | `ru_pu12_text`, `rwd_mb3` pu12 | Yes | Correct |
| The target's time of approach | `rwd_mb3` pu12 | Yes (the marked target's) | Correct |
| The DT-2 picks a target, the ДХ keypad sets number, height zone, attributes, heights, and the **designation with an address** | `rwd_mb3` pu12a | Marker, number, ЦУ + address | **Done:** СВ, Ч, ГР, Н1-Н3 set on the target |
| Semi-automatic take-off of a cabled radar's targets: 12 tracks, 2-3 s each | `ru_pu12_text` | Yes | Correct |
| The telecode passes the radar information without recomputing coordinates; the sender takes targets off by hand (2-3 s); coordinates in 1 km steps | `rwd_mb3` pu12, `ru_pu12_text`, `flak11`, `p19_to` | Every target refreshed every 10 s cycle, exact | **Changed:** the sender's round, about four targets a cycle; 1 km steps |
| Commands and reports carried as decimal numbers and shown on the tube | `rwd_mb3` pu12, `flak11` PU12-0 | Launchers' reports on the IT boards only | **Done:** the regiment's codes on the tube |
| **Receiving** addressed designations from the regiment | `flak11` PU12-0 | Not modelled | **Done:** a diamond on the tube; the NPC gives them out first |
| The ФР block makes the sweep, the range marks **and an imitated echo** (a training signal) | `rwd_mb3` pu12a | Not modelled | **Done:** at КОНТРОЛЬ |
| A switch for the primary radar's type (1RL14/131, 1RL111/128, 1RL13/134) | `rwd_mb3` pu12e | A generic cabled radar | **Done:** the switch and the library's positions |
| The PU-12M's RK-1U selects the ASPD-U's code or the S-23's (the automated field command system's) | `rwd_mb3` pu12b | Not modelled | Not needed for the 1980s battery |
| The navigation error: 5 degrees and 3 km unoriented, 1 % of the distance with the KP-4 | `flak11`, `rwd_mb3` pu12 | Yes | Correct |

## What changed in the sim (19 Oct 2026)

* **The air-picture net feeds the board** (`simcore/command_post.hpp`; `data/command/aspd_u.json` `air_net_*`;
  `data/procedures/pu12m.json` subsystem `air_net`: FuG-1 warm and the mast up). The regiment's radar battery reads one
  report at a time (7 s, an estimate), each target at most once a minute, nearest new targets first. The reports are in
  the azimuth-range form (1 degree, 1 km) from the regiment's radar, out to the board's 200 km, and reach the post within
  the R-123M's 20 km of the regiment's post. They go on whether the post listens or not. What the post hears goes into
  its radio log (net "air picture") and waits for the plotter, who writes it. One not written
  within a minute is forgotten. A player at the plotter's seat writes a heard report by its board number
  (`ScenarioRunner::PlayerPlotAt`, below). The heard reports reach a network client (`PostFeed::heard`, `ViewExtras::post_heard`).
* **The telecode** is refreshed at the sending post's take-off rate (`telecode_take_off_s`, 2.5 s): with twelve
  targets each comes every 30 s, not every 10 s, and stays on the tube three of those rounds.
* **A launcher sees only what it heard.** The Osa's `B` page holds the designations its crew has heard, each target's
  latest, labelled so (`ScenarioRunner::ViewExtrasNow`, `ASimCabinHUD::DrawBoard`); the PU-12M's seats still see the
  board.
* **The Osa's procedures** start on the battery commander's designation, not "watch the plotting board"
  (`acquire_with_soc`, `emcon_tv_azimuth`, `emcon_tv_engagement` in `data/procedures/osa_ak.json`).
* Tests: `test_command_post.cpp` (the net, the telecode round, the scenario and the stream), `test_battery.cpp` (the
  launcher's view), `test_pu12m_console.cpp` (the deployed post's first mark from the net).

## Gameplay: the board as the battery's long-range watch

The board earns its place as the **early warning** of the battery, the picture of what will arrive in minutes, while
the tube and the launchers' own radars handle what is near. The timings make the case: an Osa from cold needs about
6.8 min (3.8 min switch-on and 3.0 min function check, `flak11`). A target at 250 m/s covers 100 km in 6.7 min. A
battery that waits for the IT-45's 50 km or the launchers' 30 km detection is too late; one whose commander reads the
board at 150 km has time to bring the launchers up.

So the board's game is the commander's, and the plotter's:

* **The plotter (in the crew):** listen to the air-picture net, keep up with the reports, write each one in time (a
  report not written in a minute is lost), new and near targets first. Falling behind in a heavy raid is the skill.
* **The commander (in command):** read the board to decide *when* and *what*: the readiness level (stand the
  launchers to, power on, warm up) from each track's range and speed; which launcher faces which approach (sectors);
  when the search radars may emit (emissions control); then, once a target is inside the designation range, give it to
  a launcher by voice from the tube or the board.
* **The launcher crews:** never see the board. They hear the designation and the readiness orders, and act on them.

## The proposals, done (19 Oct 2026)

1. **Readiness from the board** (`simcore/board_reading.hpp`, `ScenarioRunner::CommanderBoard`; `battery_tactics.json`
   `warning_s` 408 s, `zone_edge_m` 10 km, `flak11`). The NPC battery commander raises readiness when a track will reach
   the zone's edge within the launchers' warm-up from cold, its range alone when it has one cross. The debrief says why
   ("from the board: 6203 at 132 km, 300 m/s: at the zone in 6.8 min") and, for each target, how much warning the
   battery had at the zone's edge. Test: a 300 m/s track 150 km out is warned of at 61 s, a 100 m/s one at 1021 s.
2. **Speed and arrival read off the board**: from the two newest crosses of a track at least 30 s apart, by their
   minute labels. The commander's HUD line gives each board number's range and "here in N min".
3. **The tube copied once a minute**: a target goes on the board from the tube again only after a board period.
4. **Designations from the regiment**: the scenario's `post.regiment.designate_within_m`; the telecode carries them, the
   IT-45 draws a diamond round the symbol, the NPC commander gives them out first.
5. **Commands as two-digit codes**: `post.regiment.commands`, the table `data/command/aspd_codes.json` (the codes are
   SAM3D's placeholders: no table was found). On the tube's top left and the commander's HUD line; the NPC acts on
   readiness, fire ban, fire permitted and cease fire.
6. **The ДХ's attributes**: the keys СВ, Ч, ГР, Н1-Н3 set friend or foe, group and the height zone on the target in the
   window; they stay through later fixes and go on the board (a friend is never given out).
7. **The imitated echo**: the ФР block at КОНТРОЛЬ draws a training target going round the post at 30 km, taken off like
   a cabled radar's echo, never written on the board.
8. **The primary radar switch** (`aspd.radar_source`, positions 1RL14/1RL131, 1RL111/1RL128, 1RL13/1RL134): a cabled
   radar's picture comes through only with the switch at its type (`regiment_radars.json` `switch_position`: P-19 3,
   P-40 2). In the cabin on the ФР block's face (the real one is on the back of the commander's rack).
9. **Grid squares** (`simcore/pvo_grid.hpp`; `aspd_u.json` `air_net_grid_levels` 2): the air-picture net reads each
   target's PVO grid square to the small square ("square 8265"); the plotter marks its centre, up to some 7 km off. The
   zone and sector numbering and the coding tables are not in the source: SAM3D gives the big square's row and column
   from the whole degree, then the medium and small ninths. The commander designates from the tube when the target is
   on it (finer), else from the board.
10. **Peacetime realism**: the mission "PU-12M range day: the picture by voice, the ASPD-U off"
    (`data/scenarios/pu12m_range_day.json`, `post.regiment.telecode: false`): the tube stays dark, the board fills from
    the net out to 166 km, the battery is warned and shoots.
11. **Sources searched**: the NVA plotters' handbook (1980) is listed by booksellers and the Deutsche Nationalbibliothek,
    no scan online; the index table: only the blanked list above; the ASPD-U's own technical description: not found; the
    time to read a report: not found (7 s stays an estimate).

## The board's place in the PU-12M (19 Oct 2026)

The ČSLA equipment plan (`csla_pu12_manual`, the scan on `valka_pu12`) draws item 24, "planšet vzdušné situace", as a
thin strip on the centreline just before the bench's middle place (1218-1311 x 468-482 px): 0.40 m long, 6 cm across, a
board folded edge-on. The NVA catalogue's photograph (`rwd_mb3` pu12e, pu12_llk1) shows the board open, square, the hinge
bar down its left edge, the commander's ДХ keypad and DT-2 behind it as seen from the plotter's place. SAM3D
(`tools/blender/pu12m_interior_trace.py` MAP_BOARD) now hinges it at the front end of the plan's strip, on the strip's
line, and swings it a quarter turn to stand across the compartment, 0.40 m wide from the hinge to the right, facing the
plotter 0.45 m ahead of his eyes and a little to his right, hanging from a roof bracket over the hinge, 1.26-1.64 m from
the ground (SAM3D: its top below the commander's line of sight to the ДХ). The plotter's eye looks down at it (yaw 20,
pitch -38). From the commander's place the board's top edge crosses the lower left of the tube, as the photograph's view
suggests. The plotter reaches it seated; the crew bodies stand when a reach fails (`crew_body.hpp`), but writing a mark
is not animated (plotting is not a control).

## Radio traffic: heard and seen (19 Oct 2026)

Every line of the radio log that is not a crew callout (the command net's designations, the air-picture net's reports,
the battery's own traffic) is read aloud in an English voice through the radio chain (`SimCrewVoiceComponent`): the
squelch opening, the voice in the radio's band (two filters each side), driven into the set's limiter, hiss, the
squelch's tail; the radio callouts go through the same chain. On screen each radio net has its own colour (amber the air
picture, cyan the command net, blue the battery net), a bar down the line's left edge and the net's name in capitals;
intercom lines and orders shouted in the cabin stay plain.

## Folding, glass and plotting by hand (20 Oct 2026)

* **Folding.** The board swings on its hinge (data/cabins/pu12m.json `hinges`; `SM_PU12MMapBoard`): folded, it lies
  along the centreline behind the hinge as the ČSLA plan draws it; open, across before the plotter. The plotter works it
  (`map.board`, subsystem `map_open`, 4 s each way, an estimate; `deploy_map`, `stow_map`; the NPC crew opens it as part
  of the deployment). Folded, nothing can be written: the reports heard wait, and are forgotten after a minute.
* **Glass.** The board is drawn through a translucent material (`M_CabinGlassBoard`, tools/ue_make_glass_material.py):
  clear plexiglass, the etching frosted into it (range rings every 50 km to 200 km, azimuth ticks, the PVO grid's big
  squares with their two digits and their ninths), the grease pencil in yellow (blue for friends), each cross labelled
  with its minute.
* **Plotting by hand** (the player at the plotter's seat). With the cross on the board, the mouse wheel turns through the
  numbers of the reports he has heard, shown in a small chip beside the cross; a
  click writes the picked one where the cross is. Within 5 cm of the report's square the mark goes
  exactly there; up to 20 cm it goes off the right place by more than 5 cm but no more than the miss; beyond, further off
  still (data/gameplay/plotting.json, the project owner's numbers; simcore/command_post.hpp `PlotAt`). The board is
  0.36 m across for 200 km each side, so 5 cm is about 55 km: generous, so that the plotter remembers the square rather
  than every figure. The air-picture lines fade from the screen in 6 s; the reports are remembered or lost.
* **The regiment's two nets.** Its commands come by telecode on the R-407 (two digits on the tube) and by voice on its
  command net through the R-111 (in violet, read through the radio voice), whichever is up; the R-111 and R-407 are
  worked as sets (simcore/radio_sets.hpp, docs/research/PU12_RADIOS_2026-10.md), the regiment's telecode off (the range
  day) is their channel off the air.

## The tube is the commander's (20 Oct 2026)

The plotter no longer copies the IT-45's tracks onto the board. The sources put the battery commander at the tube (the
commander-operator, `rwd_mb3`; the ČSLA drawing) and the plotter at the board taking the reports on targets
(`pvo_bu_x_1984` art. 23); none has the plotter copying the tube, which is in front of the commander already. So the
board carries what was heard on the air-picture net, and the commander reads the near picture off the tube: the NPC
commander gives out targets from both (`ScenarioRunner::PostPicture`), the player from the tube with the DT-2 and the
ДХ, or from the board. A cabled radar's targets are taken off by the commander's hands at the tube (`Inputs::tube_free`),
not the plotter's.
