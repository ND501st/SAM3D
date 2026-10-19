# PU-12M battery command post: development plan

Written 16 October 2026. Goal: the PU-12M as detailed as the Osa-AK, a vehicle the player (or a crew of players) can sit
in and run the battery from, with its own consoles, procedures, radios, plotting board, crew and interior, tied to the
simulation that already exists (`simcore/pu12.hpp`, `simcore/c2_sources.hpp`, `simcore/battery.hpp`, the EM layer).
Today the PU-12 exists as a board in simcore, a HUD view in Unreal, and a placeholder exterior (`SM_PU12M`,
`tools/blender/build_support_vehicles.py`).

The same rules as for the Osa apply: public and declassified sources only, operator-country documents first, every
number cited or labelled an estimate, SAM Simulator's behaviour where it has any (it has the board, `samsim_osa` p20-21).

## 1. Which PU-12

| Version | Index | Chassis | Years | Use in SAM3D |
|---|---|---|---|---|
| PU-12 | 9S482 | BTR-60PB | adopted 1977 | background |
| **PU-12M** | **9S482M** | **BTR-60PB** | **from 1978** | **the target: the Osa-AK battery of the 1980s** |
| PU-12M5 / M6 | 9S482M5 / M6 | BTR-60PB / BTR-80 | 1993-95 | later era, data only |
| PU-12M7 | 9S482M7 | BTR-80 | from 1995 | later era (digital sources: `pu12m7_page`) |

Development history (`armforc_pu12`): requirements by GRAU in 1974, design from February 1975 at the Penza radio plant
(chief designer O. L. Parkhomenko), prototype mid-1976, trials at the Donguz range, adopted 1977; the PU-12M in 1978.
Electronics by the Penza radio plant, final assembly at the Kurgan wheeled-tractor plant. Exported to Algeria, Hungary,
the GDR, Greece, India, Cuba, Poland and Czechoslovakia (`armforc_pu12`).

## 2. What is known (with sources)

### 2.1 The vehicle

| Item | Value | Source |
|---|---|---|
| Chassis | BTR-60PB, 8x8, amphibious | `valka_pu12`, `armforc_pu12` |
| Length / width | 7.22 m / 2.825 m (armforc: 7.56 m / 2.83 m) | `valka_pu12`, `armforc_pu12` |
| Height | 2.055 m to the top of the armour; 3.32-3.36 m overall; hull 2.38 m (armforc) | `valka_pu12`, `armforc_pu12` |
| Ground clearance | 475 mm | both |
| Mass | 10.4-10.5 t (armforc: 11 t) | both |
| Engines | 2 x GAZ-49B (armforc: GAZ-40P), 2 x 90 hp; the left drives axles 2 and 4, the right axles 1 and 3 and the winch | `valka_pu12` |
| Speed | 80-82 km/h road, 10 km/h in water; range 500 km | both |
| Armour | 5-11 mm | `armforc_pu12` |
| **Time to combat position** | **18 min** | `armforc_pu12` |

### 2.2 The crew

The sources disagree:

* ČSLA manual Del-25-53/1 (1982), via `valka_pu12`: five places: battery (or platoon) commander, squad leader,
  searcher-draughtsman (the plotter), signalman, driver.
* `armforc_pu12`: four: squad leader, radiotelephone operator-reconnaissance, operator, driver-mechanic-electrician.

**Settled (17 Oct 2026, `docs/research/PU12_RESEARCH_2026-10.md`):** five, as the ČSLA manual: the battery commander
works in the post, and the post has four numbers of its own (squad leader, operator-plotter, radiotelephone operator,
driver-electrician; `ru_pu12_text`). The four-man lists are the numbers without him.

### 2.3 Equipment (PU-12M, ČSLA manual via `valka_pu12`; cross-checked with `armforc_pu12`)

| Group | Items |
|---|---|
| Data (telecode) | **ASPD-U**: the RPS-M rack with the S23-1 block, the 220N instrument, the FR phase-sweep block, the IT-45 overview indicator (the round air-picture tube), DT-2 information sensor, DCh characteristics sensor; the PU-12 had ASPD-12 (R, S, P blocks, SSP drive, IT indicator, DI and DTP encoders) |
| Radios | 3 x R-123M (air target information to the vehicles, commands and reports), 1 x R-111 (to the higher command post, air situation), 1 x R-407 (commands; coded secondary data from other PU-12s); R-124 intercom; RTK-1 radiotelephone concentrator; PVS-1/-2/-3 consoles; 2 x TA-57 field telephones; RK-1V / RK-3U junction boxes; telescopic antenna mast |
| Ranges | R-407 15 km, R-123 15-20 km, R-111 25-30 km (`flak11`). The sets' own documents: the R-111 35 km whip to whip, 45 with one end on a mast, 60 with both (`r111_to`); the R-407 3-4 km on its whip, 10 on its 5 m support, 20 on an 11 m mast (`r407_to`). Both sets: `docs/research/PU12_RADIOS_2026-10.md` |
| Topographic | GAK (1G13M) gyro direction indicator, KP-4 course plotter, distance sensor, VO orientation telescope, PAB-2 (2AM) aiming circle, TZK commander's telescope; maps 1:50 000 to 1:200 000 |
| Status display | IT-1 and IT-2 information boards (subunit status, vehicle activity, ammunition); the air-situation plotting board |
| Power | AB-1-P/30 petrol generator set (220 V, 50 Hz) with URB control; G-290B generator on the move; VS-27 rectifier, regulators, filters; 2 x 6ST-75 batteries |
| NBC and other | DP-3B radiometer, chemical survey kit, DK-4, filter-ventilation; hours counter 228-CP-PO; water tank; operators' table; lights, fans; the SChK command panel |
| Capacity (armforc) | 99 targets displayed, 10 processed at once, 6 fire units controlled |
| Mast | **16 m** (`flak11` PU12-1, tier 1; a ČSLA caption on valka); 6 m in `armforc_pu12` and `shilka_narod` |
| Radars it can take | 1RL-14, 1RL-114(M), 1RL-111D, 1RL-128D(M1), 1RL-131, 1RL-13M1/M2 (`valka_pu12`) |

### 2.4 How it works (what the sim already does, and what is missing)

1. Higher command (the regiment's radars, P-19/P-40) sends the air picture by telecode to the ASPD-U; it shows on the
   IT-45 tube. The regiment's radar battery also reads it out on the air-picture voice net (FuG-1), and the plotter
   writes what he hears on the glass board, the battery's long-range picture (`rwd_mb3` pu12e, `flak11` PU12-1,
   `pvo_bu_x_1984`). **Modelled**: `PlottingBoard`, `RegimentRadars`, `CommandPost` (the air-picture net in PVO grid
   squares, the regiment's designations and two-digit commands, the ДХ's characteristics, the ФР block's imitated echo,
   the primary radar switch, readiness from the board's warning time; 19 Oct 2026: `docs/research/PLOTTING_BOARD_2026-10.md`).
2. The battery commander designates targets to the vehicles by voice, azimuth and range, on the R-123M (`flak11`).
   **Modelled**: voice designations, EM voice messages, the two R-123M sets in the Osa.
3. The board carries the PU-12M's navigation error, up to 5 degrees and 3 km (`flak11`). **Modelled**; to come: from
   the GAK, KP-4 and PAB-2 orientation drill (the error is what the drill leaves).
4. The IT-1/IT-2 boards show each vehicle's status and ammunition. **Not modelled**: needs the vehicles' reports
   (battery net), which the battery model can now produce.
5. R-407: the regiment's telecode; R-111: the higher command post's command net. **Modelled** as sets
   (`simcore/radio_sets.hpp`): their controls, warm-up, frequencies, tuning and reach, the R-407's channel and АПЧ, the
   R-111's presets and its input protection; the regiment's orders on the R-111 net (see Status, the radios).

## 3. What SAM3D needs to build

### 3.1 simcore

| Work | Detail | Builds on |
|---|---|---|
| `data/systems/pu12m.json` | The vehicle's numbers (2.1, 2.3), every value cited | `data/systems/osa.json` |
| `data/procedures/pu12m.json` | Stations (5), controls of each workplace (ASPD-U panel, IT-45 indicator controls, the radios' controls (reuse the R-123M and R-124 definitions), the IT-1/IT-2 boards, the power panel, GAK/KP-4), subsystems (power from AB-1 or G-290, ASPD-U warm-up, radios, mast up/down), inputs, procedures (deploy: 18 min; orientation; switch-on; receive and plot; designate; march) | `console_data.hpp` |
| The ASPD-U feed | Telecode messages from the regiment as EM data messages (fast, not voice), their loss and jamming; the IT-45 picture as a scope (`scopes.hpp`) | `regiment_radars`, EM comms |
| The plotting drill | The plotter's work as a crew procedure: read the IT-45, write marks; a minute per plot is the result, not a constant | `pu12.hpp`, `procedures.hpp` |
| Orientation | GAK gyro drift, KP-4 dead reckoning on the march, PAB-2 orientation at a halt: the navigation error grows on the move and is reset by the drill | `pu12.hpp` (error), Osa orientation (6.x) |
| Vehicle status boards | The vehicles report state and ammunition on the battery net; IT-1/IT-2 show it | `battery.hpp`, `launcher.hpp` |
| The battery commander | Player or NPC: assigns targets to vehicles (fire distribution: who takes which), orders (march, short halt, formation, ambush, river crossing, reload); NPC logic from the battery tactics | `battery.hpp`, `c2_sources.hpp` |
| Power | AB-1 set and G-290 generator, fuel, the mast | `fuel.hpp`, `equipment.hpp` |
| Seats | The PU-12M's five seats in the multiplayer crew (a player can be the battery commander, another the plotter) | `ScenarioRunner` seats |

### 3.2 Unreal

| Work | Detail |
|---|---|
| Exterior | BTR-60PB hull at the published size, hatches, vision blocks, wheels, the folding mast (animated up/down), whip antennas, the roof box; the AB-1 set where it stands (to research) |
| Interior | The troop compartment as the command post: the battery commander's workplace (IT-45 tube, plotting board, IT-1/IT-2), the plotter's place at the board, the radio operator's rack (3 x R-123M, R-111, R-407, RTK-1, PVS consoles), the driver's and squad leader's places in front; lights, fans, the table. Layout from the ČSLA manual's photographs ("Battery commander's workplace", "Squad leader and driver", "Radio operator's workplace", "Layout of systems in the PU-12M", on `valka_pu12`) |
| Cabin data | `data/cabins/pu12m.json` in the Osa's format: stations, panels, control positions, lamps |
| Displays | The IT-45 round indicator (a PPI-like scope of telecode tracks), the IT-1/IT-2 boards, the glass plotting board as an object you write on |
| Models | `tools/blender/build_pu12m.py`: exterior and interior parts, following `docs/MODELLING_FROM_PHOTOS.md` (trace from photographs) |
| Player | Enter and leave the PU-12M like the Osa cabin; seats; the board as a real object (the HUD view stays as an aid) |

## 4. Research to do

1. **The ČSLA manual Del-25-53/1** (Prague 1982): the source behind the valka.cz equipment list and photographs. Acquire
   it (Czech military archive, VHA; second-hand). Tier 1.
2. **flak11 PU-12 pages** (PU12-0 and the rest), the NVA view: crew, procedures, the board, the regiment's telecode.
3. **NVA handbook for plotters and radio locators** (*Handbuch für Planzeichner und Funkorter der Truppenluftabwehr*,
   1980, `nva_handbooks`): the plotting drill, symbols, timings. Tier 1.
4. Photographs of the interior: the manual's, museum vehicles (Czech, Hungarian, Polish), the 1rota.com collection.
5. The ASPD-U's telecode: message rate, format and range (only "99 targets, 10 processed, 6 fire units" so far).
6. The mast height (6 m or 16 m) and how long it takes to raise; the AB-1's place and run time.
7. The IT-45 indicator's scales and controls.

## 5. Phases

| Phase | Work | Done when |
|---|---|---|
| P0 | Research 4.1-4.4; `docs/research/PU12_RESEARCH.md` with sources and tiers | the crew, stations and equipment per station settled from a tier-1 source |
| P1 | `data/systems/pu12m.json`, `data/procedures/pu12m.json` (stations, controls, subsystems), console tests | the PU-12M console loads with every value cited |
| P2 | Procedures (deploy 18 min, orientation, switch-on, plot, designate, march) and NPC crew for them | a PU-12M crew deploys and plots on its own |
| P3 | ASPD-U telecode as EM messages; IT-45 scope; plotting as a drill; navigation error from the drill | the board's picture comes from the regiment through the PU-12M's own equipment |
| P4 | The battery commander: fire distribution, orders, vehicle status boards; player and NPC | a player in the PU-12M runs a battery of NPC Osas |
| P5 | Exterior model at full detail, animated mast | the exterior matches the photographs |
| P6 | Interior model and cabin data; displays; seats | a player can sit at every PU-12M station |
| P7 | Multiplayer: PU-12M seats in the crew system; the battery commander's picture over the network | a commander and Osa crews play together |
| P8 | The PU-12M as a seat of the Osa's battery: the post and the player's Osa in one battery (a battery link), then in Unreal | a player in the PU-12M gives targets to an Osa crewed by NPCs or another player |

## Status

- **P0 done (17 Oct 2026):** `docs/research/PU12_RESEARCH_2026-10.md`. The crew (five), the stations and the equipment
  at each from the ČSLA manual's plan and workplace drawings (`csla_pu12_manual`, tier 1), the NVA pages PU12-0 and
  PU12-1 (`flak11`, tier 1), a Russian training text (`ru_pu12_text`) and a veteran's photographs (`shilka_narod`).
  Still to find: the ASPD-U panels' controls, the deployment drill's order, the plotters' handbook.
- **P1 done:** `data/systems/pu12m.json` (every value cited) and `data/procedures/pu12m.json`: five stations, 25
  controls, 26 subsystems (power from the AB-1, the G-290B or a network; the mast; five radios; GAK, PAB-2, KP-4, DTP;
  the ASPD-U), one deployment drill per seat, stowing, the march and deployed states. Tests
  `simcore/tests/test_pu12m_console.cpp`: a crew of five deploys from cold in about 10 min (the norm: at most 18).
- **P2 first pass:** `data/crew/pu12m_crew.json` (the five seats, the orders Пункт управления развернуть / свернуть and
  the single jobs, the reports, the tasks, the world facts) and `data/scenarios/pu12m_deploy.json`: with nothing
  scripted, the NPC battery commander has the post deployed in 602 s; each seat reports its part. The crew data names
  its commanding seat (`commander_seat`), so the runner's NPC commander, reminders and pictures work for any crew. The
  post's own board (`post` in the console; the scenario's `command_post`) fills only while the ASPD-U receives the
  regiment's picture or a radar is cabled in. Crew map: `docs/crew/generated/pu12m_action_map.md`.
- **P3 done (simcore):** `simcore/command_post.hpp`, `data/command/aspd_u.json`, the scenario's `post` block.
  - The regiment's telecode arrives once a cycle while the post is within the R-407's 15 km of the regiment's post and
    the net is on the air (outages: jamming, a cut).
  - The ASPD-U's tracks on the IT-45: numbers 00-99, heights in 0.2 km steps and zones H1-H3, speed vectors in 12.5 m/s
    steps. The tube follows the ФР block's scale switch (35, 50, 100 km).
  - A cabled radar's picture is taken off by the operator, 12 targets at 2-3 s each.
  - The operator-plotter writes the board by hand, one mark at a time, new targets first and nearest first, a target
    again after each new fix; his tier sets his pace. A player in the seat writes the marks himself.
  - The navigation error comes from the orientation drill: up to 5 degrees and 3 km until the post is oriented with the
    aiming circle (flak11), then small (his tier's), growing on the move by up to 1 % of the distance (the KP-4).
  - Not yet: the telecode through the EM layer's link budget (its reach is the R-407's antennas' published range,
    jamming an outage or interference); the IT-45 drawn in Unreal (P6).
- **The R-111 and the R-407 as sets (19 Oct 2026).** `docs/research/PU12_RADIOS_2026-10.md` (the R-111's ТО, tier 1, with
  its front drawn and every control named; the R-407's ТО data and its own instruction plate, tier 1), `simcore/radio_sets.hpp`,
  `data/systems/pu12m_radios.json`; tests `simcore/tests/test_radio_sets.cpp`.
  - **The R-111 (FuG-3), the radio operator's.** РЕЖ. РАБОТЫ (ОТКЛ., ДЕЖ. ПР., 1 / 20 / 100 %), ИНДИКАТОР and its meter,
    ВИД РАБОТЫ (eight positions), the preset buttons ПЛ and 1-4, ДИАПАЗ., the frequency knob and its scale, НАСТРОЙКА УМ
    and САУ, ВЫЗОВ, ПШ, ТЛК, the two levels, and the antenna distributor ШТЫРЬ / ТЕЛЕСКОП on the SAU block (at the
    commander's place: he sets it, the radio operator tunes after). The scale settles in 5 min; the cathodes want 3 min
    at 20 % before the set is tuned; a preset turns the mechanism and the set tunes itself (45 s in all). It hears the
    regiment on the net's frequency with ТЛФ (or a relay position), transmits tuned for its frequency and antenna, its
    reach the ТО's by the antennas each end has up and the fourth root of the power. A set of the post's own within 10 %
    (or 8 or 16 MHz off) trips its input protection (ЗАЩИТА ВХОДА) until it is reset.
  - **The regiment's command net.** A scenario's `post.regiment.messages` are said on it; the post hears them on its
    R-111 and, with the set tuned and in reach, receipts them; else the regiment says them again (twice, 2 min apart:
    SAM3D). The radio log has them; the debrief has what was heard, receipted, or never got through.
  - **The R-407 (FuG-5), the battery commander's.** ПИТАНИЕ, РОД РАБОТЫ (ДУПЛЕКС, ДЕЖ. ПРИЕМ – СИМПЛЕКС, ТА), АПЧ, УЗКАЯ /
    ШИРОКАЯ, ПУ, the modes switch, the meter switch, the receiver's and the transmitter's drums behind their СТОПОР levers,
    КАЛИБРАТОР – СВЕТ, ВЫЗОВ, ИЗМЕРИТ. УРОВЕНЬ, the three levels. The regiment's telecode reaches the ASPD-U when the set
    is warm, in a radio mode, its receiver on the regiment's channel (within the АПЧ's 15 kHz, else 7), its duplex pair 4
    MHz apart and off the plate's spurious channels, the channel on the 4-wire terminals, within the mast's 15 km (4 km
    with the mast down). In interference (`"kind": "interference"` on a regiment outage) the АПЧ chases the noise: АПЧ off
    holds the channel (the plate).
  - **The frequency plan.** The scenario's `post.regiment` (`command_net_mhz`, `telecode_mhz`, `telecode_return_mhz`,
    `mast`) and `post.radios` (the R-111's four presets, the post's own R-123M nets, `prepared`: the drums and the scale
    set in the park). The numbers are placeholders.
  - **The drills.** The radio operator's `tune_r111` (after the mast), the commander's R-407 steps and the R-111's antenna
    in `deploy_command_radios`, `stow_antenna`, and the situational `r111_protection`, `r407_interference`,
    `r407_apch_on`, with their orders and reports in `data/crew/pu12m_crew.json`. The NPC commander has the R-111 tuned
    as part of the post deployed (`fug3_tuned`).
  - **The fronts traced and modelled** (`docs/MODELLING_FROM_PHOTOS.md`): the R-111 from a square-on photograph and the
    ТО's fig. 4 (`tools/traces/r111_front.json`, `tools/blender/r111_trace.py`), the R-407 from a sale photograph and its
    labels' close-ups (`r407_front.json`, `r407_trace.py`); one builder for both (`tools/blender/radio_front_model.py`,
    `build_pu12m_radios.py`: `art/pu12m_interior/SM_RadioR111.fbx`, `SM_RadioR407.fbx`), proved on their photographs from
    the solved cameras (`check_pu12m_radios.py`). The cabin layout places them with bare panels in their fronts' planes,
    every control, lamp and display where the set has it (the meters read the set: `"reading"` in a meter display).
- **P4 done (simcore):** the battery commander in the post. Scenario `data/scenarios/pu12m_battery.json` (four NPC
  launchers, a TZM, a raid with a friend in it); tests `simcore/tests/test_pu12m_battery.cpp`.
  - Designations go by voice on the battery net (the post's FuG-4 up and the mast raised), one message at a time:
    number, azimuth and range from the launcher (flak11). He reads the number off the board and the place off the
    IT-45, its latest fix led along its speed vector to when the launcher hears it.
  - The launcher's crew takes the target nearest the point it was told within 6 km (sam3d): with the post's navigation
    error it can take another target, or report «цели не наблюдаю».
  - The NPC battery commander gives each hostile on the board to the nearest free launcher (its sector first), never a
    friend; a player in his seat does it himself (`DesignateBatteryTarget`).
  - The launchers report on the battery net (к бою готов with their missiles, launch, result, out of missiles, reloaded);
    the post hears them only while its set is up, and writes them on the IT-1 and IT-2 boards (`StatusBoards`).
  - Battery orders (march, halt, formation, ambush, river crossing, reload, emission control) and sectors of fire work
    from the post as before (`OrderBattery`, `SetBatterySector`). Not yet in Unreal: the boards and the designation UI
    (P6).
- **P5 done:** the exterior traced from photographs (docs/MODELLING_FROM_PHOTOS.md, the new "Vehicles" section).
  - `tools/blender/pu12m_trace.py` (the hull's stations, wheels, roof fittings, the mast's geometry), tested by
    `tools/tests/test_pu12m_trace.py` (published dimensions, the wheels clear of the hull, the fittings clear of each
    other, the mast nesting and reaching 16 m).
  - The side profile: a BTR-60PB's side photograph with its camera fitted (EXIF focal length, the published length,
    roof height, track and the ČSLA plan's widths as priors; 3.5 px rms; the wheelbase lands on the published 4.40 m).
    The overlay from that camera lines up the tyres, the chine, the ends and the glacis; the roof shows 6-10 cm low
    (the photo reads 5-8 % taller than the published 2.055 m).
  - `tools/blender/pu12m_model.py` builds `SM_PU12M` (hull, wheels, hatches, vision blocks, lights, the roof box, whips,
    rails, the side stowage box, the exhaust, the mast's base), the mast's five sections, the AB-1 set and a guy, and
    `art/pu12m/pu12m_rig.json`.
  - Unreal: `USimPu12MastComponent` stands the sections for an extension 0-1 (stowed 1.5 m above the roof, raised to 16 m),
    shows eight guys at two heights once it is up and the AB-1 once it is set out. A battery's PU-12M follows its
    deployment at the mast's pace; the player's own post follows the console's `mast_up` progress
    (`USimWorldSubsystem::GetPostMastExtension`, `IsPostGeneratorOut`). Test `SAM3D.PU12M.Mast`.

- **P6 done (17-18 Oct 2026):** the interior, the cabin, the displays, the seats.
  - `tools/blender/pu12m_interior_trace.py`: the ČSLA equipment plan read item by item (scaled on the hull's outline as
    the exterior is; 47 of its 53 numbered items, every one inside the hull and clear of the others, each move noted),
    heights from the workplace drawings and the veteran's photographs, the hull's inside from the exterior's stations.
    Tests `tools/tests/test_pu12m_interior_trace.py`. Acceptance: the top view rendered from the model over the plan
    (`out/pu12m_interior/overlay_plan.png`, made by the scratch overlay script described in the modelling doc).
  - The seats from the plan: the driver front left, the squad leader front right, and **a bench across the compartment
    facing forward with three places**: the operator-plotter at the right (the IT-45 straight ahead of him, the ДХ
    keypad at its left and the ФР block at its right, as the photograph shows), the battery commander in the middle,
    the radio operator at the left (the R-111 with an R-123M on it ahead, his table between). Eye points 1.22 m above
    the floor (the body model's), every control within reach, three of them standing (`stand_for` in the crew data).
  - `tools/blender/build_pu12m_interior.py` builds the lining (light grey, the photographs), the equipment, the bench,
    the front seats, the driver's wheel, pedals and dash, the mast's foot with its hand winch; the Osa's traced R-123M
    serves three times. `tools/ue_import_pu12m.py` imports it headless.
  - `data/cabins/pu12m.json` is written by `tools/pu12m_cabin.py` from the trace: 27 panels on the equipment's faces,
    all 25 controls, 37 lamps (each sourced), 153 fixtures (the ДХ keypad, the ФР block and the RTK-1 read off the
    photographs). Displays: the IT-45 (`aspd_scope`: the ASPD-U's tracks on the ФР block's scale, numbers, zone marks,
    speed vectors, with the grid glass over it carrying the plotter's marks in grease pencil) and the IT-1 / IT-2
    (`status_board`: a column of legend lamps per launcher and its missiles). Tests
    `simcore/tests/test_pu12m_cabin.cpp`.
  - The mast corrected from the ČSLA mast drawings: a fixed section and **eight** moving ones, raised by a hand winch at
    its foot; its place from the plan (2.30 m behind the bow, 0.63 m right).
  - Unreal: the player's vehicle wears the PU-12M's exterior and mast when the scenario's own vehicle is the post; the
    director loads the cabin of the scenario's own vehicle (`cabins/<system>.json`). At the battery commander's seat,
    `[` `]` pick a target on the board and 1-5 give it to that launcher; at the plotter's, `[` `]` pick one on the IT-45
    and P writes it on the board (`ASimOsaVehicle::PickTarget`, `DesignatePicked`, `PlotPicked`). Tests
    `SAM3D.PU12M.Cabin`, `SAM3D.PU12M.Mast`; photographs of every seat from a scripted offscreen run
    (`-SimScenario=scenarios/pu12m_battery.json -SimCabinShots=<folder>`).
- **P7 done (18 Oct 2026):** multiplayer. The seats are the crew system's (any station id). The post's instruments ride in
  each player's knowledge stream (`simcore/post_feed.hpp`, `em::ViewExtras`: the IT-45's tracks, the board as the
  commander reads it, the IT boards), so a client's cabin draws the same picture; a client's designation or plot goes to
  the server through its seat (`USimCrewSeatComponent::Designate`, `PlotTrack`), checked there (only the commanding seat
  designates, only the plotter plots: `ScenarioRunner::SeatDesignate`, `SeatPlot`). Checked with
  `tools/ue_crew_smoke.ps1 -Pu12m` (a dedicated server and two clients, `docs/MULTIPLAYER.md`).
- **The look refined (18 Oct 2026):** research `docs/research/PU12_VISUALS_2026-10.md` (the NVA catalogue's PU-12 pages
  and colour photographs, the Soviet tube table, GOST 21829-76).
  - Corrections from it: the battery commander sits at the IT-45 (the ДХ, the IT-2M, the ФР block, the RTK-1 and the
    DT-2 are his), the operator-plotter at his own air-situation map (the plan's item 24, a plexiglass board); the lining
    cream, the ASPD-U's blocks blue-grey, the IT-45, the ДХ and the IT-2M light grey; the GAK on a stand over the PT-200;
    the S-23 a black box.
  - The instruments work: the DT-2's two wheels put the IT-45's marker on a target; the ДХ's digits pick a target, ЦУ and
    a digit give it to the launcher in that place on the IT boards (by voice on the battery net), СБР clears
    (`data/procedures/pu12m.json` keypad and marker, `ScenarioRunner::UpdateKeypad`, test in `test_pu12m_battery.cpp`).
  - The displays: the IT-45 an amber long-afterglow tube with its sweep, range marks and a cabled radar's echoes fading,
    the symbols steady, the marker and the marked target's time of arrival; the ДХ's window; the plotter's map with the
    board's grease-pencil tracks; the IT-1M and IT-2M as lamp boards (`simcore::BoardLampsOf`); the cabin's lamps ease on
    and off.
  - Fixes: the mauve cast (Lumen's sky light through the closed compartment: the sky lights are put out while the camera
    is inside, the lining 15 cm thick with a finer distance field, the cabin hidden from outside); the DT-2's and the
    IT-45's round parts drawn as polygons (Nanite's fallback: the interior without Nanite). The PU-12M's crew has its
    voices (`data/crew/voices.json` serves both crews).

- **P8 first part, simcore (18 Oct 2026):** the post and the player's Osa in one battery. `simcore/battery_link.hpp`
  steps two runners, the Osa's and the PU-12M's, as one battery (`data/scenarios/battery_link_practice.json`):
  - **One sky.** The Osa's runner moves the targets; the post's mirrors them every step; a kill on either side is a
    kill on both.
  - **Designations.** The post's designation to 21 (by voice: number, azimuth and range) reaches the Osa's commander as
    the PU-12's on the command net, at the message's own times and over the Osa's link. None comes while the post's
    battery-net set is off the air.
  - **Reports.** The Osa's commander reports on the battery net (к бою готов, пуск, цель поражена, ракет нет), and the
    post writes it on its IT boards. The post's other launchers' assignments and reports are heard in the Osa.
  - **Fire distribution.** The Osa counts in the post's distribution with its own missiles and the target its crew is
    on.
  - **The Osa's commander in a linked battery.** He fights the target the post gave him first. He leaves targets given
    to another launcher alone, and echoes beside them too (a second track of the same aircraft). If his crew still
    locks one, he has it let go.
  - **A general fix found on the way:** the commander's "Цель номер … — взять!" now names the ordered target's number,
    not the latest one heard.
  - **Checks.** Tests in `simcore/tests/test_battery_link.cpp`; `battery_link_demo` plays it headless and prints both
    debriefs. In the practice raid each hostile is destroyed once, by the launcher it was given to.
- **P8 second part, Unreal single player (18 Oct 2026):** the world subsystem holds a link like a scenario
  (`-SimScenario=scenarios/battery_link_practice.json`, or the director's `ScenarioFile`).
  - **Which vehicle.** The player starts in the Osa (`-SimVehicle=post`: in the PU-12M). **V** moves him to the other
    vehicle: its console, crew, cabin, bodies and voices. The player's vehicle actor moves to where that vehicle stands
    and puts on or takes off the post's exterior and mast. The vehicle he leaves fights on with its NPC crew, and shows
    on the map as a battery vehicle of the one he is in.
  - **Checks.** Test `SAM3D.PU12M.BatteryLink`; an offscreen game run in the post showed the linked Osa reporting ready
    on the battery net and on the IT boards.
  - **The IT boards' rows follow the battery's order (21, 22, ...)**, not who reported first: the ДХ's launcher places
    count down them.
- **P8 third part, multiplayer (18 Oct 2026):** a commander in the post and players in the Osa.
  - **The player's vehicle.** He chooses it when he joins (`-SimVehicle=post` or `osa`). His seat carries it
    (`USimCrewSeatComponent::SeatVehicle`, sent with his seat request).
  - **Server routing.** The server routes his seat's moves, designations and plots to that vehicle's runner, and sends
    him that vehicle's console snapshots and knowledge (`USimWorldSubsystem::RunnerFor`).
  - **On the client.** The client loads the link and follows its own vehicle (its cabin, its exterior).
  - **No moving mid-game.** Changing vehicle in the middle of a networked game is not offered (V stays single player).
  - **Check.** `tools/ue_crew_smoke.ps1 -Link`. The commander's client in the PU-12M was refused the radio operator's mast
    winch by the post's console and gave the board's first target to 21. The Osa's driver started its gas turbine. The Osa's
    commander heard the designation on its command net.
- **The interior refined (18 Oct 2026):** nothing in the compartment goes into anything else, the crew included.
  - **The check.** `tools/blender/pu12m_interior_check.py` builds every item and part of the interior and the cabin's
    panels and finds what intersects, with its depth (rays cast from each mesh's vertices and edges into the other).
    With `--poses` it adds the crew's bodies posed at rest and at their controls
    (`crew_pose_dump data out/crew_poses_pu12m.json pu12m`). Run headless:
    `blender --background --python tools/blender/pu12m_interior_check.py -- --poses out/crew_poses_pu12m.json`.
    From 30 intersecting pairs to none.
  - **The equipment.** The GAK on its stand over the PT-200; the IT-2M moved in from the sloping side; the IT-45's
    screen raised above the desk. The wheel raised 8 cm, its rim clear of the dash and the driver's knees. The PAB-2
    case off the mast's clamp. The VO a periscope from the bow plate. The map board's hanger to a roof bracket; the gear
    lever's boot on the floor. The ФР and S panels sit on their block's face.
  - **The RTK-1** stands on the desk at the right wall under the IT-2M (SAM3D). It hung over the tube's front, as the
    plan has it, and the commander getting up for it put his head into the IT-45.
  - **The ФР block is the commander's.** Its switches (power, mode, scale) belong to the battery commander, as does the
    deploy and stow drill of the ASPD-U (`rwd_mb3`: the tube's place is the commander's, the plotter's is at the map).
    The operator-plotter has no switch of his own: he plots. `tools/ue_crew_smoke.ps1 -Pu12m` now has the plotter
    refused the scale switch and the commander setting it; `-Link` has the commander refused the mast's winch.
  - **The crew's bodies.** The desk's inner edge stops 9 cm short of the plan's, clear of the commander's arm. The radio
    operator's table hangs on struts from the R-111 (its leg stood between his knees). The reach planner keeps heads
    under the roof (`ceiling_m` in the cabin, 1.40 m over the floor) and bodies out of the equipment. A crewman who
    gets up for the DTP or the winch crouches a step back with his knees clear (`docs/crew/10_BODIES_AND_VOICES.md`).
    Hands set off in time to arrive on the control.
  - **The light.** The rear dome lamp now hangs over the map and the desk. Just over the bench's heads it threw their
    shadows across the whole rear bulkhead.
  - **Shots all round.** `-SimShotsAround` adds left, right, behind, down and up to each seat's shots
    (`unreal/README.md`).

## 6. What is still SAM3D's

- The plan's item 24 is the plotter's air-situation map (`rwd_mb3`); its grid, range circles and the pencil's colours
  are SAM3D's.
- Heights, the seats' cushions and the desk, the faces of the blocks not photographed (the S block, the ŠČK, the power
  boards), the lamps' meanings. Of the ДХ's keys only the digits, ЦУ and СБР work (the console does
  not model the characteristics, the height zone or the other keys); what its window shows is SAM3D's.
- The IT-45's symbols and sweep period, the IT boards' lamp-to-report mapping and which board stands where (the NVA
  catalogue's two pages disagree), the lamps' timings (`docs/research/PU12_VISUALS_2026-10.md` §9).
- The two whips' bases inside: the plan's items 3 and 28 stand elsewhere than the exterior's whips (placed from the
  photographs); left out of the interior until the photographs are read again.
- The radios (`docs/research/PU12_RADIOS_2026-10.md`, decisions and open questions): the R-111's front width (scaled
  from the plan, about 5 %), the R-407's (from its overall size, about 3 %); which antenna the mast carries for each set;
  the ASPD-U on the R-407's 4-wire terminals; the split of the R-111's 45 s retune; the regiment's frequencies and how
  often it repeats a message; the R-407 standing on the floor behind the commander, its back to the side (the plan draws
  it smaller than the set is), the BP-3 on its bracket lifted over it.

## Sources

| Id | Tier | What |
|---|---|---|
| `valka_pu12` | 4 (transcribes the ČSLA manual Del-25-53/1, 1982: tier 1 content) | PU-12 / PU-12M: dimensions, crew of five, the equipment lists, photographs of the workplaces ([valka.cz / armedconflicts.com topic 23991](https://www.armedconflicts.com/topic/view/23991)) |
| `armforc_pu12` | 4 | ПУ-12 (9С482): history, crew of four, equipment, 18 min to combat position, capacity ([armforc.su](https://armforc.su/srk_pu12.htm)) |
| `pu12m7_page` | 4 | PU-12M7 (later) |
| `flak11`, `nva_handbooks`, `samsim_osa` | 1, 1, 3 | the NVA view, the plotting drill, SAM Simulator's board |
