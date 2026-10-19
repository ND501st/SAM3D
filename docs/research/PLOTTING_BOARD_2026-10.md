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
| A troop air-defence radar company's mobile post holds the ASPD's remote indicators **and the boards of the general air situation and of each radar** | `pvo_bu_x_1984` art. 26 | 1 |
| The designation (warning) nets carry the air enemy to the command posts and the AA units; target data may be passed in clear; positions go by the automated systems or by **coded squares of the PVO grid**; sector boundaries are drawn on the indicators and the boards | `pvo_bu_x_1984` art. 29-30, 37 | 1 |
| The automated chain ends at the PU-12; from there designations go to the launchers **by voice, azimuth and range** | `flak11` (PU12-0) | 1 |
| The air-situation report: number, place (PVO grid square, or azimuth in degrees and range in km), identity and size as a two-digit index, height in hectometres, time of the fix in minutes of the hour. New targets are marked "00" before the first report. Reports at up to two minutes apart carry the height at least every fifth time | `bguir_tsp` | 2 |
| A non-automated report from the radar operator reaches the unit's post within 1 min (the higher posts within 2 and 3 min) | `bguir_tsp` | 2 |
| Tracks are drawn as a cross a fix, labelled with the minute (40, 41, 42...), the number, the index, height and speed at the head | `bguir_tsp` (table 2) | 2 |
| The plotters stand behind the glass and write in mirror image so the officers in front read it the right way round; grease pencil on edge-lit Piacryl | `planchette_dewiki`, `rwd_mb3` (llk72), `narodna_armiya_2018` | 4 |
| Information to the plotters every 1-2 min; the boards kept as the back-up of the automated system | `narodna_armiya_2018` | 4 |
| NVA plotters were conscripts; trained from the *Handbuch für Planzeichner und Funkorter der Truppenluftabwehr* (1980) | `flak11`, NVA veterans' pages | 1 (handbook to acquire) |
| In the troops the ASPD-U was **rarely switched on** (no real situation to use it for) and not used at platoon firings | `shilka_narod` | 2 |

### Who used it, when

* **Where it is:** in the battery's PU-12M (and in the radar companies' and higher posts). Not in the Osa: the 9A33BM2
  has no board. The Osa crew hears the battery commander's designation (azimuth and range) on the R-123M.
* **Who writes it:** the operator-plotter, from what he hears on the air-picture net (and what the commander's tube
  shows). **Who reads it:** the battery commander.
* **What for:** the air picture beyond the tube. The IT-45 shows 35-100 km; the board carries the regiment's picture
  as far as its radars reach (200 km rings in SAM Simulator). It tells the commander what is coming, from where, how
  high, how many and when, before anything is inside a launcher's 10 km zone: the battery's early warning.
* **When:** whenever the battery is deployed and the net is up. In peacetime, training and range days most of all,
  because the ASPD-U was seldom on (`shilka_narod`). Automation did not retire it: today's Ukrainian radio-technical
  troops keep their boards as the back-up (`narodna_armiya_2018`).

### The identity digit: a conflict

SAM Simulator's index: 1 friend, 7 practice target, 9 no IFF (`samsim_osa` p20). `bguir_tsp`'s examples use 7 for an
unexplained real target and **2** for "training air target". The full table (in the *Instruction on composing and passing
reports*) was not found. SAM3D keeps SAM Simulator's digits for now; both are recorded here.

### A source not used

The P-19 radar's technical description (*РЛС П-19Ш, Техническое описание ЕИ1.001.015 ТО/с*), found on the same veterans'
site, gives the P-19's built-in ASPD's scales, baud rates and coordinate steps. It is marked **secret** (ТО/с). Under
the project's rules (public and declassified only) none of its figures are used; the project owner decides whether it
may be. The figure that would have come from it is the telecode's coordinate step (`telecode_step_m`), left off in
`data/command/aspd_u.json`.

## The ASPD-U and the IT-45 against the sources

| Item | Sources | SAM3D before | Verdict |
|---|---|---|---|
| The IT-45 is the commander's, a repeater PPI ("Tochtersichtgerät") of the primary (cabled radar) or secondary (telecode) picture | `flak11`, `rwd_mb3` pu12a/pu12f | Yes | Correct |
| Scales | `flak11` 50/100; `ru_pu12_text` 35/50/100; the ФР block's three positions | 35/50/100 | Correct |
| 100 targets, numbers 00-99, H1-H3, height in 0.2 km, speed vector in 12.5 m/s, friend/foe, group | `ru_pu12_text`, `rwd_mb3` pu12 | Yes | Correct |
| The target's time of approach | `rwd_mb3` pu12 | Yes (the marked target's) | Correct |
| The DT-2 picks a target, the ДХ keypad sets number, height zone, attributes, heights, and the **designation with an address** | `rwd_mb3` pu12a | Marker, number, ЦУ + address | Attributes not entered (proposal 6) |
| Semi-automatic take-off of a cabled radar's targets: 12 tracks, 2-3 s each | `ru_pu12_text` | Yes | Correct |
| The telecode passes the radar information without recomputing coordinates; the sender takes targets off by hand (2-3 s) | `rwd_mb3` pu12, `ru_pu12_text`, `flak11` | Every target refreshed every 10 s cycle | **Changed:** the sender's round, about four targets a cycle |
| Commands and reports carried as decimal numbers and shown on the tube | `rwd_mb3` pu12, `flak11` PU12-0 | Launchers' reports on the IT boards only | Proposal 5 |
| **Receiving** addressed designations from the regiment | `flak11` PU12-0 | Not modelled | Proposal 4 |
| The ФР block makes the sweep, the range marks **and an imitated echo** (a training signal) | `rwd_mb3` pu12a | Not modelled | Proposal 7 |
| A switch for the primary radar's type (1RL14/131, 1RL111/128, 1RL13/134) | `rwd_mb3` pu12e | A generic cabled radar | Proposal 8 |
| The PU-12M's RK-1U selects the ASPD-U's code or the S-23's (the automated field command system's) | `rwd_mb3` pu12b | Not modelled | Not needed for the 1980s battery |
| The navigation error: 5 degrees and 3 km unoriented, 1 % of the distance with the KP-4 | `flak11`, `rwd_mb3` pu12 | Yes | Correct |

## What changed in the sim (19 Oct 2026)

* **The air-picture net feeds the board** (`simcore/command_post.hpp`; `data/command/aspd_u.json` `air_net_*`;
  `data/procedures/pu12m.json` subsystem `air_net`: FuG-1 warm and the mast up). The regiment's radar battery reads one
  report at a time (7 s, an estimate), each target at most once a minute, nearest new targets first. The reports are in
  the azimuth-range form (1 degree, 1 km) from the regiment's radar, out to the board's 200 km, and reach the post within
  the R-123M's 20 km of the regiment's post. They go on whether the post listens or not. What the post hears goes into
  its radio log (net "air picture") and waits for the plotter, who writes it before he copies the tube. One not written
  within a minute is forgotten. A player at the plotter's seat writes a heard report by its board number (`[ ]` then
  `P`; `ScenarioRunner::PlayerPlot`). The heard reports reach a network client (`PostFeed::heard`, `ViewExtras::post_heard`).
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

## Proposed next (not done)

1. **Readiness from the board.** The NPC battery commander raises the launchers' readiness from the board's time to
   arrival, not only a threat range (`battery_tactics.threat_range_m`). Score the player commander on the warning time
   he gave the launchers.
2. **Speed and arrival read off the board.** Two marks a minute apart give the speed; show the commander an estimate of
   arrival from the board as he would make it by eye (the IT-45 already shows the marked target's).
3. **The tube copied once a minute.** The plotter now copies a tube track at every new fix (about every 10-30 s); the
   sources' board runs on minutes. Limit a target to one mark a minute, whichever source it came from.
4. **Designations from the regiment.** The ASPD-U receives addressed designations; let the regiment's post assign
   targets to the battery (a fire task from above), shown on the IT-45.
5. **Commands as two-digit codes.** The ASPD-U carries commands and reports as decimal numbers (readiness, fire ban,
   cease fire): show the regiment's on the tube and the IT boards.
6. **The ДХ's attributes.** Let the commander set friend/foe, group and height for a track he takes off.
7. **The imitated echo.** The ФР block's training echo: an IT-45 drill without a radar, for the plotter's and the
   commander's lessons.
8. **The primary radar switch.** The three positions of `rwd_mb3` pu12e for the radar cabled to the post.
9. **Grid squares.** The PVO grid's coded squares as the reports' form (the designation grid about the radar); the
   azimuth-range form is the one used now.
10. **Peacetime realism.** Range days with the ASPD-U off and the picture by voice (`shilka_narod`).
11. **Sources to find:** the NVA plotters' handbook (1980); the report index table; the ASPD-U's own technical
    description; the timing of a report read out on the air-picture net.
