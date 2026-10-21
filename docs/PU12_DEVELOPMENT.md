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

Decision (to confirm): five, as the ČSLA manual, with the battery commander one of them; the four-man version is the
crew without the battery commander. The flak11 PU-12 pages (`flak11`, NVA) should settle it; the site's certificate had
expired on 16 Oct 2026.

### 2.3 Equipment (PU-12M, ČSLA manual via `valka_pu12`; cross-checked with `armforc_pu12`)

| Group | Items |
|---|---|
| Data (telecode) | **ASPD-U**: the RPS-M rack with the S23-1 block, the 220N instrument, the FR phase-sweep block, the IT-45 overview indicator (the round air-picture tube), DT-2 information sensor, DCh characteristics sensor; the PU-12 had ASPD-12 (R, S, P blocks, SSP drive, IT indicator, DI and DTP encoders) |
| Radios | 3 x R-123M (air target information to the vehicles, commands and reports), 1 x R-111 (to the higher command post, air situation), 1 x R-407 (commands; coded secondary data from other PU-12s); R-124 intercom; RTK-1 radiotelephone concentrator; PVS-1/-2/-3 consoles; 2 x TA-57 field telephones; RK-1V / RK-3U junction boxes; telescopic antenna mast |
| Ranges | R-407 15 km, R-123 15-20 km, R-111 25-30 km |
| Topographic | GAK (1G13M) gyro direction indicator, KP-4 course plotter, distance sensor, VO orientation telescope, PAB-2 (2AM) aiming circle, TZK commander's telescope; maps 1:50 000 to 1:200 000 |
| Status display | IT-1 and IT-2 information boards (subunit status, vehicle activity, ammunition); the air-situation plotting board |
| Power | AB-1-P/30 petrol generator set (220 V, 50 Hz) with URB control; G-290B generator on the move; VS-27 rectifier, regulators, filters; 2 x 6ST-75 batteries |
| NBC and other | DP-3B radiometer, chemical survey kit, DK-4, filter-ventilation; hours counter 228-CP-PO; water tank; operators' table; lights, fans; the SChK command panel |
| Capacity (armforc) | 99 targets displayed, 10 processed at once, 6 fire units controlled |
| Mast | 6 m (armforc); "about 16 m" raised by the signalman (a forum user on valka): to be settled |
| Radars it can take | 1RL-14, 1RL-114(M), 1RL-111D, 1RL-128D(M1), 1RL-131, 1RL-13M1/M2 (`valka_pu12`) |

### 2.4 How it works (what the sim already does, and what is missing)

1. Higher command (the regiment's radars, P-19/P-40) sends the air picture by telecode to the ASPD-U; it shows on the
   IT-45 tube and the plotter writes it on the glass board once a minute (`samsim_osa` p20). **Modelled**:
   `PlottingBoard`, `RegimentRadars`.
2. The battery commander designates targets to the vehicles by voice, azimuth and range, on the R-123M (`flak11`).
   **Modelled**: voice designations, EM voice messages, the two R-123M sets in the Osa.
3. The board carries the PU-12M's navigation error, up to 5 degrees and 3 km (`flak11`). **Modelled**; to come: from
   the GAK, KP-4 and PAB-2 orientation drill (the error is what the drill leaves).
4. The IT-1/IT-2 boards show each vehicle's status and ammunition. **Not modelled**: needs the vehicles' reports
   (battery net), which the battery model can now produce.
5. R-407: coded data from other PU-12s; R-111: the higher command post. **Partly** (C2 sources, neighbour feeds).

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

## 6. Placeholder now

`SM_PU12M` (`art/support_vehicles/`, imported to `/Game/SAM3D/Vehicles/`): the BTR-60PB hull at 7.22 x 2.825 m, 2.055 m
to the armour top, four axles at the BTR-60PB's spacing, the mast stowed on the roof, four whip antennas, the roof box,
vision blocks and side doors. No interior. Battery entities of kind `command_post` use it on the map.

## Sources

| Id | Tier | What |
|---|---|---|
| `valka_pu12` | 4 (transcribes the ČSLA manual Del-25-53/1, 1982: tier 1 content) | PU-12 / PU-12M: dimensions, crew of five, the equipment lists, photographs of the workplaces ([valka.cz / armedconflicts.com topic 23991](https://www.armedconflicts.com/topic/view/23991)) |
| `armforc_pu12` | 4 | ПУ-12 (9С482): history, crew of four, equipment, 18 min to combat position, capacity ([armforc.su](https://armforc.su/srk_pu12.htm)) |
| `pu12m7_page` | 4 | PU-12M7 (later) |
| `flak11`, `nva_handbooks`, `samsim_osa` | 1, 1, 3 | the NVA view, the plotting drill, SAM Simulator's board |
