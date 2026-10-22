# Development roadmap

How the project gets built, in order. Complements `PROJECT_PLAN.md` (goals and milestones) with the concrete sequence,
what blocks what, and how each task is finished. Written 30 Sep 2026; **re-sequenced 2 Oct 2026** after the research
review (`docs/BRIEFING_2026-10.md`).

**Status (22 Oct 2026):** phases 0–5 done (phase 4 except Chaos Vehicles, the real model and the performance check).
Phase 6, the accuracy pass, done. Phase 7, the battery and PU-12, done in simcore, and the PU-12M built through its P8.
Phase 8, crew and teamwork, first passes 8.0–8.15 done; its exit (M2) waits for a person to play the missions through.
Phases 9 (threats) and 10 (scenarios and training) done in simcore. Phase 11, the enemy air command, built; a person's
campaign playthrough left. Phase 12, our chain of command, C0–C5 built; P1–P3 and P9 next. The EM track: E0–E7 and E10
done, E8 and E9 started. **Next: the nation on the map** (prioritised 22 Oct 2026, below): the country's map, ultra-basic
placeholder models for every asset and the air war running on them; **then the battery's war, phases 13–21**.

The old "crew and mission" phase became phase 8, after the battery (phase 7), because the real Osa got most of its
targets from the battery's PU-12 and the crew changes ripple through everything after them (re-sequenced 2 Oct 2026).

## Principles

0. **Crew and teamwork (the project owner's design pillars, 16 Oct 2026; `docs/crew/README.md`).** Teamwork with NPC
   crewmates is paramount. The game has two halves: *in the crew*, the player is one member of the team, learning the
   drill and how to operate each seat; *in command*, the player gives the orders, first for the single Osa, then for the
   battery. Every NPC works with every other; every action and command is mapped and interconnected (the generated action
   map), with behaviour paths, callouts and commands for every environment and outcome. NPC skill levels come from player
   progression and quests and affect reaction, speed and accuracy everywhere. An ideal progression of events, the drill
   master's perfect performance, is the reference for the training overlay, like a chess engine that always knows the
   best move. One crewmate's action depends on others' knowledge and actions (someone must know where the target is,
   someone must have driven within reach), and that interdependence is mapped.
1. **simcore is the game, Unreal is the presentation.** Build and test behaviour in simcore first; Unreal only displays it.
2. **Procedures and consoles are the heart of the game.** Operator procedures, interlocks, crew, battery and scenarios
   come before any deeper physics.
3. **Accuracy before features.** When research shows the model is wrong, fix the data model before building UI on it.
4. **Vertical slice first.** One system (Osa-AK), one battery position, one complete mission, before a second system.
5. **Small PRs on branches**, one topic each. Never commit to `main` directly.
6. **Definition of done:** builds with warnings-as-errors, tests added, `tools/verify.ps1` passes, docs updated if
   behaviour or data format changed. Unreal changes: built from the command line and checked with
   `tools/ue_cabin_check.py` where it applies; the user plays it.
7. **Public and declassified sources only, best tier first** (`docs/SOURCES.md`). Every data value has `source` and
   `confidence`; disagreements between sources are recorded, not hidden. SAM Simulator's art, sounds and `sam.exe`
   never enter git.

## Phases 0–5 (done)

| Phase | What | Where |
|---|---|---|
| 0 | Environment: Windows build, UE 5.7, Cesium, `gh`, CI | PR #1 |
| 1 | Consoles and procedures from data; equipment and interlocks; crew order queue; debrief | PR #2 |
| 2 | Scenarios, deterministic runner, `osa.json` (vehicle, crew, power) | PR #3 |
| 3 | Gameplay-level engagement model (search picture, lock/break-lock, outcome resolver) | PR #4 |
| 4 | Unreal bring-up: scenario mode, Cesium terrain, placeholder Osa (open: 4.3 Chaos Vehicles, real model, 4.5 performance check) | PR #5 |
| 5 | 3D cabin: layout data, player actions judged by a tracker, instruments (search scope, range indicator, TV, dials, voltmeter), hands-free tour and scripted checks | PRs #6, #7 |

## Phase 6 - Accuracy pass (simcore, data, cabin)

Corrections from `docs/research/OSA_RESEARCH_2026-10.md`. Each task names the research section it comes from.
The control-level breakdown (which switch, at which seat, in which real block) is in
`docs/research/OSA_CONSOLE_MAP_2026-10.md`. Its task 6.0 was adopted on 2 Oct 2026. 6.1 was built the same day
without it (PR #10), so 6.0 runs after 6.1 and before 6.2, and corrects 6.1's seats and layout where the photos
differ (the map has the turbine switch at the driver's power panel, and the three operators side by side at one
cabinet).

| # | Task | Research |
|---|---|---|
| 6.0 | **Done (PRs #12–#16, closed out in console map §13).** **Eyes-on pass** (needs a session linked to the PC, for `reference/`). Go through the cabin photos and layout drawings (`rwd_mb3`), SAM Simulator manual p14–18, the control sprites and the running game. Record every control in the console and cabin files: label, kind, positions, start position, place on its panel; controls whose function isn't known go in as visible but inert. Check each seat with `tools/panel_sheet.py` against the photos. Closes or hands to the creator the open items in the console map §11 | console map §9, §11 |
| 6.1 | **Done (PR #10).** **Five-man crew and seats.** Split the "range officer" into the senior operator (search radar, IFF, azimuth) and the operator (range); the "firing officer" becomes the deputy commander (elevation, TV, launch); the commander receives designations and gives orders; the driver. Console `station` fields, cabin layout (five seats, operators' cabin behind the driver's cab), procedures, scenario crews, aim roles. Panel detail: red dots marking each control's starting position | §2, §6 |
| 6.2 | **Done (simcore and data; the Unreal time scale is written but not yet built).** Cold switch-on 4.0 min and function check to 7.1 min (published 3.8 + 3.0), reaction time 27 s (published 26-40), march to combat well inside 4 min (the physical deploy work isn't modelled, so it is faster than the unverified 35-50 s). Left: crew-skill timing for NPCs (next target 40-120 s, unverified). **Real start-up and readiness states.** Switch-on cycle and a function-check procedure (about 3.8 + 3.0 min cold), computer warm-up (100 s), march-to-combat ≤ 4 min, reaction 26–40 s as test targets; scenarios start cold, powered on the march, or deployed (a powered battery stops and deploys fast); crew-skill timing for NPCs (next target 40–120 s, unverified); time compression so it stays playable | §3, §10, §12 |
| 6.3 | **Done (`simcore/launcher.hpp`, the console's `launcher` block).** Also done: the command transmitter's letter change (ЗАХВ. АПЧ out for 1 s, ПУСК ГОТОВ waits), the emergency launch (ИЗДЕЛИЯ + АВАРИЙНЫЙ ПУСК, unguided 25.5 s), and the range indicator's missile sweeps Р I / Р II (simcore and the Unreal drawing, checked in the editor with `tools/ue_sweep_check.py`). Also done: a prepared pair is held at most 10 min, then the gyros have overheated (the ammunition lamp flashes), the next pair is prepared, and the hot missiles rest 10 min (1 h after the third hold). A part load: `own_unit.empty_containers` (e.g. `osa_mission_march.json`). **Missile load, selection and channels.** Six containers (1–3 I, 1–3 II) with ammunition lamps; automatic selection, outer first; selector switch; two channels, ≥ 4 s between launches; per-channel lamp sequence СХОД → ЗАХВАТ → СР.Л → УЗКИЙ ЛУЧ → К3; command transmitter on at ВНИМАНИЕ; preparation at "target in zone"; fix the ЗАХВАТ / ВКЛ ВЫС mapping | §6 |
| 6.4 | **Done on the EM radar (PR #29; `docs/EM_ARCHITECTURE.md` §24).** MTI's 35 km working range in the EM path done 18 Oct 2026 (`mti.max_range_m`: beyond it the normal channel, nothing cancelled; voenizdat_osa_1991 p30). The scope draws the console's rings for each scale (`ScopeRingsM`, 18 Oct 2026). IFF replies from the EM radar: the interrogator transmits while the console asks, own-side transponders reply on the search radar's own tracks (`EmSensorLink::SetInterrogating`, `Contact::iff`; test in `test_em_link.cpp`). **Search radar.** 33 rpm; elevation beams I/II/III that decide what the radar sees (4°, 4°, 19° high), plus the beam indicator; shorter detection and capture ranges against low targets (27 km and 14 km at 50 m, original Osa); range rings as operators describe them; IFF interrogation and friendly targets; overhead blind cone; normal video vs MTI | §4 |
| 6.5 | **Done (the gyro half-compass frame and orientation at a halt were already in `engagement.hpp`; this adds the Unreal heading conversion, which treated Unreal yaw as true heading and was 90 degrees off under the Cesium georeference, and a test of board designations on an unoriented vehicle).** **Completed 17 Oct 2026 (`simcore/orientation.hpp`, `test_orientation.cpp`):** the OR-31M Qc scale (a dial reading the gyro, turning while slaved and stopping on agreement: voenizdat_osa_1991 p48, p52); gyro drift (the Earth's rate × sin latitude, cancelled by the ШИРОТА latitude correction p51 when РЕЛЕ — Qc is back at РЕЛЕ, plus an estimated own drift 0.5 deg/h); the crew chief's compass work (ПАБ, mutual sighting, Qc = α − qв, p249–250: 180 s at a halt, 0.2 deg error, both estimates), commanded to the deputy ("Курс машины …"), whose drill now waits for it; the NPC commander surveys at each halt and breaks off for a target, the player commander has `order_compass` (and К бою). **Vehicle heading and orientation.** Drop "the hull faces north": azimuths relative to the hull, the navigation/orientation set aligned at a halt, designation errors when it isn't | §3, §7 |
| 6.6 | **Done (PR #33; `data/gameplay/README.md`). Completed 17 Oct 2026:** the 3-thousandth reticle's marks along the crosshair in the Unreal TV view, placed for the lens in use (`ReticleMarks`), and the range judged from the target's size in it by the formula of the thousandth (`tv.span_m`, the deputy's "Дальность по телевизору …", the range wheel set by hand, `RangeSource::TvSize`). **Manual TV tracking.** The TV channel follows the wheels (player) or a skill model (NPC); 3-mil reticle; operator lens fields; memory mode; range judged from the reticle | §5 |
| 6.7 | **Done (PR #32; `data/gameplay/README.md`).** Also done: ПОМЕХА selects method Н, and К3 then comes with the medium beam (1991 textbook p22-23, p136); the tracking radar needs its СДЦ on for targets under 100 m (an estimate). Jamming targets that hide the range, with Н firing at them, and the firing zone's crossing parameter (6 km at 300 m/s, 4 km at 500 m/s): done in 9.1. **Zones and guidance methods.** Head-on, pursuit and optical "Ф" zones; methods T/T, Горка, Ф, Н chosen automatically; hit chances calibrated inside the NVA single-shot ranges and Korovin's low-altitude figures (0.35–0.4 at 50 m) (game-balance file) | §5 |
| 6.8 | **Done.** No value in `data/` cites a tier-5 source; the photo shorthand in the cabin file now names its registered source; `tools/check_citations.py` (with a test) keeps it so. The radar frequencies still rest on `western_bands` (tier 4/5, low confidence): operator sources only say "centimetre band". **Citations.** Replace Western "facts" citations in `data/` with operator-country sources; record conflicts | §1, §10 |
| 6.9 | **Done (20 Oct 2026; `docs/research/OSA_MANUALS_2026-10.md`, "The 1991 textbook read in full").** `voenizdat_osa_1991` read cover to cover. ЦЕЛЬ В ЗОНЕ is now the computer's launch zone (the meeting point in the zone, the crews' Table 7.1 far edge), ВНИМАНИЕ its eq. 2.24, a shot's result at the meeting time; the command link waits in its loads until the launch; the command link's and radars' book values in the EM library; the battery tactics re-sourced (short halt in the column within 20 km, 1 h halts). Since built (20 Oct): the march's radio discipline; **kill assessment by its signs** (p267, `simcore/kill_assessment.hpp`: each seat calls the sign its instrument shows, the deputy commander calls the result from what he saw and heard, a wrong call where the signs settle nothing, the target seen flying on taken back); **group targets in a designation** (p261, `simcore/group_designation.hpp`: the PU-12M gives a group it can't resolve by its centre and a rule 1-4, the Osa's senior operator counts it on his scope). the ARM drill (E6, `docs/EM_ARCHITECTURE.md` §35). Left (recorded): small drones' signs, helicopter sectors and duty launchers, the stabilisation's standby and fault, signs of normal operation on the scopes; a rule chosen from the post's UI. Since built (21 Oct): the battery net's correction of a wrong kill report (`report_not_destroyed`; a linked post puts the target back on its board). **The 1991 textbook in full** | §5, §7 |

**Exit:** the practice scenario runs with five crew, real timings (with time compression), six missiles and a
two-missile salvo, and every lamp in the order the NVA account gives. `tools/ue_cabin_check.py` shows it.

## Phase 7 - Battery and PU-12

| # | Task |
|---|---|
| 7.1 | **Done in simcore (`simcore/pu12.hpp`, `data/command/pu12.json`, scenario `command_post`).** The board plots once a minute with the manual's notation (number, height in hectometres over type and formation, minute stamp), carries the PU-12M's navigation error (5 degrees, 3 km), and the crews aim by it when the radars are silent. The regiment's radars now feed it (`data/command/regiment_radars.json`, scenario `c2.regiment`): the board holds a target only when a P-19 or P-40 of the list covers it, range and horizon. Left: the plotting board in Unreal (7.7). PU-12 command post in simcore: air picture from the regiment, updated once a minute; SAM Simulator's plotting-board notation (target number, height, type, formation) |
| 7.2 | **Done in simcore (`simcore/pu12.hpp`).** Each target inside 50 km gets a voice designation (azimuth in mils, range, height) on every plot; the radio carries one message at a time (4 s operator delay + 8 s message, both estimates, then 2 s for the vehicle commander to pass it on), and the crew aims by the last designation it was told. Left: the player as battery commander (giving the designations), the designation on the real R-123M net with loss and jamming (E4), and a vehicle commander NPC that can get it wrong. Target designation by radio: the battery commander (NPC or player) gives "azimuth, range" to each vehicle; the vehicle commander passes it on |
| 7.3 | **TZM reload of one vehicle done (`simcore/launcher.hpp`).** With the `tzm_alongside` input set and the vehicle stopped, the crane loads one missile a minute into empty containers, only once the pair is spent and nothing is guiding: six in 6 minutes (`flak11`). **16 Oct 2026: the TZM is a battery vehicle that drives up (8 m alongside), sets the input, loads from its stock of 12 and drives back; an ATZ fuel truck (3.8 m³) does the same for fuel (`fuel_truck_alongside`); the runner calls them when the launcher is empty and nothing is guiding, or the fuel system asks (`battery.hpp` `RequestService`, `scenario_runner.hpp` `ServiceDrills`).** Placeholder models `SM_TZM_9T217`, `SM_ATZ_38_131` (`tools/blender/build_support_vehicles.py`). Left: NPC crews in the other vehicles, the crane's three-container traverse. Four vehicles per battery (others NPC-crewed) and the TZM: reload 6 missiles in about 6 min |
| 7.4 | **Done in simcore (`simcore/fuel.hpp`, the console's `fuel` block).** One 300 l tank; the turbine burns 75 l/h at a halt and 100 l/h moving, the take-off generator 25 l/h; at half the tank the crew asks for fuel and the power sources are cut to keep the rest for driving; the TZM refuels at a halt (60 l/min is an estimate). The driver's fuel gauge (`fuel_gauge` on the dashboard, readout `fuel`, litres through the scope feed and the knowledge stream; its face and place are estimates); the grid and the diesel set power the position (7.6). Left: the drive burning fuel (a driven vehicle in Unreal). Fuel: turbine and take-off generator consumption, the half-tank rule, refuelling from the TZM |
| 7.5 | **Done in simcore (`simcore/battery.hpp`, `data/command/battery_tactics.json`, scenario `battery` block, triggers `battery_order` / `air_threat`), 16 Oct 2026.** The battery around the player's vehicle: march column at 40 m (400 m when an air attack is expected: set by a trigger, or automatically when a designated target is inside 50 km), short halt off the road on alternating sides with deployment, combat formation in pairs (3 km apart, 200 m within, 2 km behind the covered unit, TZM 500 m behind its pair, PU-12 further back), halts every 3.5 h (1.5 h, 1 h at night); the other launchers' radars feed the crew as moving neighbour sources (`share_picture`). Numbers from `video_osa` (unverified) and estimates. **Drills, 16 Oct 2026:** ambush (pairs on prepared points, roving between them), bounding forward in pairs, river crossing (2 km short, then across in pairs at 8 km/h afloat once the far bank is held), TZM and fuel-truck runs. Left: NPC crew behaviour inside the other vehicles (they are moved, not crewed) | NPC battery behaviour from the battery tactics in the research (§7) |
| 7.6 | **Done in simcore (`simcore/position_prep.hpp`, `data/command/prepared_positions.json`, `own_unit.prepared_position`), 16 Oct 2026.** Emplacement and camouflage nets lower the vehicle's echo for the other side's radars (estimates: x0.2 and x0.5 of 20 m2); dummy positions are echoes of their own with an optional decoy emitter copying the search radar (E7); leaving the emplacement costs 60 s before the battery moves; wire is presentation. Grid and diesel power: `own_unit.position_power` (done earlier). **Unreal, 17 Oct 2026:** the works at each place a vehicle started in (`ScenarioRunner::Works`, `em::ViewWork` in the knowledge stream, `ASimPositionWork`): an earth parapet round the footprint with the back open, a net sagging on four poles that comes down when the vehicle leaves; launchers and the dummy positions' mock-ups drawn Osa-shaped (`UseOsaStandIn`). Left: smoke generators, the emplacement's protection against blast (no attack or damage model yet) | Prepared positions |
| 7.8 | **Done in simcore (`simcore/c2_sources.hpp`, `data/command/c2_sources.json`, scenario `c2` block, triggers `c2_source`/`c2_fail`/`c2_restore`).** Other early-warning and command sources besides the PU-12 (EWR voice net, division early-warning net, neighbouring battery, observation post, data link), shared voice nets, outages, the crew changing source (player: `RequestC2Source`; NPC commander: fallback on silence and return to the main source). The PU-12 stays the main source. Evidence and decisions: `docs/research/EM_C2_SOURCES_2026-10.md`. EM link budget as the link model done (sources with a position), and their voice designations are EM `Voice` messages (radiated, delivered or lost by the link budget). The cabin has both R-123M sets with their controls (`comms` block, `simcore/crew_comms.hpp`, `docs/research/OSA_COMMS_2026-10.md`): a set that is off, warming up (10 min) or turned down hears nothing, the squelch and the antenna shorten the EM link by the published ranges, and the R-124 box A-1 decides whether the commander hears the net or shouts his orders and the designations he passes on; the scenario's `frequency_mhz` puts each net on a frequency of the 25 kHz grid; NPC crew switch the sets on (`radio_left_on`, `radio_right_on`). **Transmitting done, 17 Oct 2026:** the commander's chest switch (ПРМ — ПРД) and ТОН. ВЫЗОВ; his reports go out only on a set that can transmit (on, warm, СИМПЛЕКС, A-1 on Р-123), radiate on the net in the EM layer, and the TZM and fuel requests need them (`CrewComms::CanTransmit`, `ScenarioRunner::SayOnRadio`). **Preset set-up done, 17 Oct 2026:** the fixators, УСТАНОВКА ЧАСТОТЫ, ПОДДИАПАЗОН, the antenna knob and its lock, smooth tuning, the drills `preset_setup_left/right` and `order_presets`, `own_unit.radio_presets` (`simcore/radio_setup.hpp`). **Audio done in Unreal, 17 Oct 2026:** the headset's hiss, suppressor, carrier quieting and call tone (`simcore/crew_audio.hpp`, `USimCrewAudioComponent`), the words are 8.10's crew voices. A VHF jamming scenario, 18 Oct 2026 (`osa_vhf_jamming_em.json`: a barrage jammer takes the PU-12's net down, the commander changes to the EWR's net below the band). Left: fusing two sources |
| 7.7 | **Done in Unreal, 16 Oct 2026 (built; checked over the network with `tools/ue_crew_smoke.ps1`, not yet looked at on screen).** The PU-12 plotting board as a HUD view (B): range rings, the battery at the centre, each target's marks of the last five plots joined into tracks, number, height/code and minute stamp; radio and intercom traffic as HUD lines (`ScenarioRunner::RadioLog`: command net, battery net, shouted orders); the battery's other vehicles and dummy positions on the map as entity actors (box stand-ins). All three also ride the knowledge stream, so network clients see them (`em::ViewExtras`). The practice map now plays `osa_battery_practice.json`. Placeholder models for the TZM, the ATZ fuel truck and the PU-12M (16 Oct 2026). Left: the launchers' and maintenance vehicle's models, the board as an object inside a PU-12 interior (the PU-12M plan: `docs/PU12_DEVELOPMENT.md`) | Unreal: plotting board in the PU-12, radio messages, battery vehicles on the map |
| 7.9 | **Done through P7, 18 Oct 2026 (`docs/PU12_DEVELOPMENT.md` Status): console, crew, the ASPD-U chain and IT-45, the battery commander (player or NPC), the exterior traced from photographs, the interior and cabin from the ČSLA plan, multiplayer seats and the post's picture over the network; the R-111 and the R-407 worked as sets, traced and modelled from their documents and photographs (19 Oct 2026, `docs/research/PU12_RADIOS_2026-10.md`).** Planned 16 Oct 2026: the PU-12M as a vehicle as detailed as the Osa: five stations, ASPD-U telecode and the IT-45 indicator, the radio rack (3 x R-123M, R-111, R-407, R-124, RTK-1), topographic orientation (GAK, KP-4, PAB-2), IT-1/IT-2 status boards, power, deploy in 18 min, the battery commander as player or NPC, a full interior and multiplayer seats (phases P0-P7). A placeholder exterior is on the map now (`SM_PU12M`) | PU-12M console, procedures, crew and interior |

## Phase 8 - Crew and teamwork, and the mission (the vertical slice)

Re-planned 16 October 2026 around the project owner's crew pillars (`docs/crew/README.md`): **teamwork with NPC
crewmates is paramount**; the game has two halves, **in the crew** (the player as one member of the team, learning the
drill and every seat) and **in command** (the player giving the orders, first for one Osa, then for the battery); every
NPC works with every other and every action and command is mapped and interconnected; behaviour paths, callouts and
commands for every environment and outcome; NPC skill levels from player progression and quests; and an ideal
progression of events, the "chess engine" reference for the training overlay (the **drill master**). The full plan with
its done-when criteria is `docs/crew/09_IMPLEMENTATION_PLAN.md`.

| # | Task |
|---|---|
| 8.0 | **Done 16 Oct 2026:** the crew reference (`docs/crew/`), the pillars in every document, the generated action map (`tools/crew_map.py`) |
| 8.1 | **Done 16 Oct 2026:** crew data (`data/crew/`): seats, orders, callouts, tasks, world facts, skill tiers; loader `simcore/crew_data.hpp`; scenario `crew[].skill` |
| 8.2 | **First pass done:** skill in behaviour: reaction, jitter, slips, omissions, wheel rates, TV lag and aim, range by hand, stress (flag). Left: judgement, communication misses, stress from the situation, calibration tests |
| 8.3 | **Done:** the drill master (`simcore/drill_master.hpp`): plan, ideal timeline, best next move per seat, cost of a move, fault recovery; the engagement reference (the network chart on the zone's entry) and the scored decisions (`simcore/crew_decisions.hpp`), judged in the debrief |
| 8.4 | **First pass done:** callouts (`simcore/crew_callouts.hpp`): orders, acknowledgements, automatic reports, engagement reports, designations passed on, faults, reminders; HUD radio lines and the debrief |
| 8.5 | **Done 17 Oct 2026:** knowledge per seat (`simcore/crew_knowledge.hpp`): pictures from own instruments and heard reports; missed reports and "Повторите"; the commander decides from his picture; player reports |
| 8.6 | **Vehicle first pass done:** the NPC vehicle commander (`"crew_commander": "npc"`, scenario `osa_crew_practice.json`): readiness by the drill master's plan, the engagement by the crew data's orders, supervision of a player's seat; the player in command through `GiveCrewOrder`. Left: march, priorities, second missile, logistics, C2 |
| 8.7 | **Done:** the assistance levels (demonstrate, guided, hint, unassisted, exam; H cycles them), the ghost hand, each move's cost shown at once, the crew's grades in the debrief |
| 8.8 | **Done but for a radial menu:** the crew menu (O) with honest greying and refusals ("Не могу"), taking a seat (T) |
| 8.9 | **First pass done:** the battery's other launchers fight (`simcore/battery_fire.hpp`), the battery commander's fire distribution (NPC or player) on the battery net; scenario `osa_battery_fight.json`; sectors of fire. **The PU-12M seat done 18 Oct 2026** (`simcore/battery_link.hpp`: the post and the player's Osa in one battery, in simcore and Unreal, single player and networked: `docs/PU12_DEVELOPMENT.md` P8); left: full NPC crews for the other launchers, the battery on the march under the link |
| 8.10 | **First pass done (17 Oct 2026):** the crew's bodies working their controls (`simcore/crew_body.hpp`, Unreal `USimCrewBodiesComponent`, meshes from `tools/blender/build_crew_body.py`), the reach check of the cabin (standing for far panels costs drill time), the crew's voices (`simcore/crew_voice.hpp`, Windows speech, recorded lines first). Left: skinned meshes, lips, Russian voices on this machine, voices on network clients |
| 8.11 | **First pass done in simcore:** `osa_mission_march.json` (march, raid, halt, deploy, fight, stow, march on). Orientation in the readiness and a reload inside the mission done (17 Oct). Left: choosing the position, the map |
| 8.12 | **First pass (headless) done:** `simcore/examples/crew_balance`, `docs/crew/11_PLAYTEST_AND_BALANCE.md`: trained crews meet the published reaction time, recruits don't; two NPC commander faults found and fixed. Every system of the console in the NPCs' decisions (IFF, СДЦ, EMCON by the TV, the second missile, radios, fuel; 17 Oct). Left: a person playing the missions on the map |
| 8.13 | **First pass done (20 Oct 2026):** the crews' communications (`docs/crew/12_COMMUNICATIONS.md`, `docs/research/CREW_COMMS_2026-10.md`): headsets and boxes gate who hears what in both vehicles (`simcore/voice_comms.hpp`: the PU-12M's RTK-1 and PVS-1/2/3, the Osa's R-124 A-1..A-4 and ПВ, the TA-57); the radio data, nets by number, scheduled and ordered frequency changes, jamming and the spare drill, radio silence and direction finding (`simcore/radio_nets.hpp`); the talk tables ПТ-1 and ПТ-2 (`simcore/talk_tables.hpp`); the air picture read out to the plotter by the radiotelephonist; designations with call signs and read-backs; the RTK-1's five lines; every seat's lesson (with the regiment's ПТ-1 group decoded by hand in the battery commander's, the Osa commander's A-1 between ВС and Р-123, his readiness report and read-back of the PU-12M's designation, the squad leader's radio data). Then (20 Oct): the plotter's spoken reports (art. 23) feeding the NPC commander's picture, the march's radio discipline (flags and lamps, voenizdat_osa_1991), the papers on their forms. Left: the EM layer's link budget for the voice nets, a person playing it |
| 8.14 | **First pass done (21 Oct 2026):** the crews out of their vehicles and back (`docs/crew/13_OUT_OF_THE_VEHICLE.md`, `docs/research/MOUNTING_2026-10.md`): the vehicles as bodies to climb (`data/vehicles/osa_ak.json`, `pu12m.json`: hatches, the roof and what not to stand on, the ways down by the wheels, the places round them, the jobs done there), the pace and the voice's carry (`data/crew/walking.json`, the drill regulation's pace, Tobler's slope, ISO 9921), a crewman's body leg by leg through a hatch, over the hull and the ground (`simcore/crew_mobility.hpp` `CrewMover`), NPCs walked out for jobs outside (the PU-12M's AB-1 and PAB-2, the Osa's aiming circle for Qc), the Osa's hatches shut in БР (p. 207), К машине / По местам (also from the order menu), a man outside off the intercom and heard only in earshot; Unreal: the player out through a hatch, on the roof, down, over the map and back in first person (F), the job switches walking him out, the NPCs drawn walking. Next: the drill master and the lessons knowing the walk, wire laying (P-274) as the first new task outside, the NPCs' bodies mirrored to clients, and the crew chief's compass survey (6.5, `orientation.hpp`) drawn as his walk out to the aiming circle (`measure_qc`): today the NPC commander's survey is timed in place and the walk happens when `measure_qc` is ordered |
| 8.15 | **First pass done (21 Oct 2026):** the helper (`docs/crew/14_HELPER.md`): turned on in the missions menu, it shows the move the player's seat's NPC would make now, as a lesson shows its step without the card. The NPCs' own rules record their acts at a player's seat as his duties (`ScenarioRunner::NpcDoes`, `simcore/seat_duty.hpp`); in command the NPC commander thinks in the player's place (the shadow commander); the wheels' marks are the engagement's own aim; `simcore/helper.hpp` puts them in order; `helper_coverage` plays every mission from every seat following it against the NPC crew (`docs/crew/generated/helper_coverage.md`). Found on the way: a player's drill now passes steps whose switches are already set; a player's report tells the crew what his NPC's would. Left: the TV camera in К бою for a commander (data), the acts with no menu move, the gaps the map lists |

**Exit = M2:** a person can play the whole mission without touching the editor, in a seat of the crew or in command.
**The way in done 18 Oct 2026:** the game opens on a missions menu (`data/scenarios/missions.json`,
`simcore/missions.hpp`, Unreal `ASimMissionMenuHUD`): every mission with its scenario's description, started in the
vehicle and seat the player picks (the commander's: in command; any other: in the crew); Esc in a mission pauses it
with restart, the missions and quit, and shows the debrief when it ends (`unreal/README.md`). **The first playable test
done 19 Oct 2026** (`docs/PLAYING.md`, `tools/play.ps1 -Tutorial`): a tutorial mission first in the menu with a guide on
the screen (`data/tutorials/`, `simcore/tutorial.hpp`; played through headless in `test_tutorial.cpp`), DCS-style camera
views on F1-F11 (`USimViewComponent`), time compression on Ctrl/Alt/Shift+Z, every key on G. **The lessons reworked
19 Oct 2026** (`docs/TUTORIALS.md`, research `docs/research/TUTORIAL_DESIGN_2026-10.md`): a first lesson (the camera,
the cabin, each kind of control) and a lesson for each of the Osa's five seats, step by step, each step's control lit
in the cabin and marked on the screen, the next step only once it is done; strict in the seat lessons; an instructor's
voice; every lesson played through headless by a perfect student. **After the first playtest (19 Oct 2026):** Backspace
back a step, N skips one (the instructor does it); the PU-12M's five seat lessons; walking in the cabin (X: up from the
seat, W A S D, X by a seat sits down; `CabinLayout::walk`); a held hand wheel turns the head to its display
(`CabinControl::watch`); a crewman getting up for a far panel keeps out of his seated crewmates (`AddSeatedCrew`); the
search scope and range indicator drawn with a long-afterglow phosphor's persistence, speckle and bloom. Left for M2: a
person playing them through.

## Phase 9 - Threats

| # | Task |
|---|---|
| 9.1 | **Done in simcore, 16 Oct 2026.** A noise jammer in the tracking radar's beam drowns the echo: the radar follows the jammer in angle (`angle_track_on_jam`, `em/radar_system.hpp`), with no range, and takes the echo again when it burns through (a few km against the placeholder pod). The engagement tracks "radar on the jammer" (`TrackMethod::RadarOnJam`); the range comes by hand, the operator keeping the range wheel on the target's blip on the search scope or on the PU-12's last range (Korovin: angles from one sensor, range from the search radar); ЦЕЛЬ В ЗОНЕ lights from that range. ПОМЕХА fires by method Н (`outcome.method_factor`, 0.75, a game value); with no range and ПОМЕХА off the fuse never arms (`no_range_fuse_factor` 0). New drill `fire_at_jammer`; the firing zone's crossing parameter (6 km at 300 m/s, 4 km at 500 m/s, `mon_2112_80`). Scenario `osa_jamming_em.json`. Noise jammers and range-gate pull-off on the scopes came earlier (E0, E3) |
| 9.2 | **EMCON TV engagement as a full mission done, 16 Oct 2026** (`osa_emcon_tv.json`: radars silent, the PU-12's designations, the TV tracked by hand, the range by hand from the board; the drill now waits for the TV track before the key, input `target_tv_tracked`). Emission discipline as doctrine (E6, 18 Oct 2026): the search radar in bursts against direction-finding (`doctrine.search_bursts`). The anti-radiation missile threat and the crew's drill against it, 20 Oct 2026 (E6, `docs/EM_ARCHITECTURE.md` §35; `osa_arm_drill.json`): the signs of a launch, silence after the burst, the least emission under `doctrine.arm_threat`, relocation |
| 9.4 | **Mixed tracking done with 9.1:** TV angles or the radar on a jammer, with the range from the search radar or the board by hand, or the range-only radar (ПОЛУАВТОМАТ). The external-coherence MTI against chaff exists (E0). **Counter-jamming done, 17 Oct 2026:** the textbook names the facility the video guessed at (voenizdat_osa_1991 p80-81, p98-99, p259, p266): the switch УВ. ПОМЕХА on the SSC receiver (`ssc.rgpo`), a circuit that blanks the video just after the tracked echo so a range-gate pull-off walking out can't take the gate, and СДЦ's staggered periods against one walking in; both switched on only when automatic tracking is impossible and off when the jamming stops. EM: `RadarState::rgpo_trailing_blank`, `prf_staggered`; a leading repeater `generic_rgpo_leading_ku`. Crew: the operator reports "Увод по дальности!" (`range_pulled`), the commander orders `counter_pull_off` and later `pull_off_protection_off`; switching it on with no pull-off seen is a logged mistake. Scenario `osa_rgpo_em.json`. The range-memory guess is not supported by the textbook. **The range-only radar jammed, 18 Oct 2026:** ПОЛУАВТОМАТ ranges only on an echo the tracking radar sees where the TV turns its antenna (one mount), coasting on the TV's 5 s memory; a noise pod drowns it and the range goes back to the hand (the TV picture, the search scope or the board). The NPC commander now orders the TV on НЛЦ also for a target the radar lost or never had, then ПОЛУАВТОМАТ once the TV holds it; a lost lock with nothing in the air frees the deputy from his launch drill (before, the crew froze). Mission `osa_low_flier_em.json` |
| 9.3 | **Done in simcore, 16 Oct 2026** (`simcore/target_behaviour.hpp`, scenario target `behaviour` / `evade`, waypoints with `agl_m`, `terrain.ridges`). Terrain following with a look-ahead (a low strike run is a route of heights: low ingress, pull-up, low egress); pop-up helicopters that hide, pop up and drop back, then fly on; evasion on a launch or on the tracking radar's lock (an RWR hears a lock, not a TV track): break, break and dive, or jink and resume. Masking needs no behaviour of its own: low behind a ridge, every sensor loses the line of sight. Scenario `osa_low_level_em.json`. Profiles are placeholders |

## Phase 10 - Scenarios and training

| # | Task |
|---|---|
| 10.1 | **Done in simcore, 18 Oct 2026.** Range days in the style of Ashuluk (from cold, two La-17M drones, one high in the search radar's beam III), Ustka (P-15 targets from the sea, a two-missile salvo at each: `doctrine.salvo`, the second 3-5 s after the first, `korovin_tiv_2010`) and Kapustin Yar (a target entering the zone at its far edge: the missile goes at 8-10 km, the preparation starting in the zone). NAMFI (Crete) left for later |
| 10.2 | **Done in simcore, 18 Oct 2026.** The Bekaa, 24 July 1982 (an F-4E pair over the mountains, hidden until the crest); Angola from the Pavlov memoir (dug in under nets, a night raid by Buccaneers and an Impala fought by the radars alone, a Puma hidden in a canyon); Cuito Cuanavale, 20 February 1988 (Mirage F1AZs tossing their bombs). The events are sourced, the profiles placeholders (`data/scenarios/README.md`, "Ranges and history"); the missions menu's "Ranges and history" group. Night moves with the search radar on are left for the march mission. Found and fixed on the way: targets circling a waypoint inside their turn circle for ever (after a jink), pull-ups that read as 100+ deg/s manoeuvres in the outcome, a high target's track losing its name in beam III, a manoeuvring target's split track and a kill's last blip reported as new echoes, НЛЦ lit by a target hidden behind the ground and called again at every flicker |
| 10.3 | **Done, 18 Oct 2026.** The 9Ф632 trainer's instructor (`flak11`, ePap: he enters the air picture and changes up to two targets' behaviour live; ECCM drills): `simcore/instructor.hpp`, `ScenarioRunner::Instruct` / `InstructorEnter` / `InstructorPicture`. He takes a target (two at a time), turns it, sends it up or down (a terrain-following pilot holds his height), sets its speed, makes it jink (again and again), switches its noise pod on or off, lets it go back to its route, or enters a training drone; everything is in the debrief under "Instructor". A scenario scripts him with the trigger action `"instructor"` (`osa_trainer.json`, the drills group). Unreal: the instructor panel (F9, where the simulation runs: single player or the server), `-SimInstructor`. Left: an instructor seat for a networked client (he is the host today) |
| 10.4 | **First pass done, 18 Oct 2026** (`docs/TEST_RANGE.md`). The Kesmark test range: a 40 km generated map of the theatre's district (ground, river, bridges, towns, woods, military sites, ground out to 150 km), weather over the whole airspace (winds aloft and the boundary layer, cloud layers the TV cannot see through, the day's air), the sun's place; the sim and the picture share the ground and the cloud. `/Game/Maps/KesmarkRange`, the missions menu's "Test range". Next: a person flying and driving it; E9 using its obstacle grid; missiles in the wind |

## Phase 11 - The enemy air command (the other side's air war)

Started 18 October 2026 at the project owner's request: the enemy air forces of 1980-85 (a rebel faction, a small neutral
neighbour, the Warsaw Pact, NATO) planned by officers who feel like real people and react to everything the player
does. Design: `docs/air_war/` (plan and done-when: `docs/air_war/08_IMPLEMENTATION_PLAN.md`); research:
`docs/research/ENEMY_AIR_RESEARCH_2026-10.md`; code `simcore/include/simcore/air_war/`; data `data/air_war/`,
`data/gameplay/air_war.json`.

| # | Task |
|---|---|
| A0-A9 | **Done in simcore, 18 Oct 2026.** Research and sources; the theatre and four factions as data; the air staff (traits, skills, 31 quirks, memory, stress, reputation, careers); the intelligence picture (satellite, reconnaissance, ELINT, radio intercept, agents, pilots' claims, ageing, ghosts, dummies); the planner (apportionment, utility, packages, approval, interference, reasons); abstract resolution; the campaign (lessons, politics and events, logistics, repair, relocation, our side's INTSUM and dossiers); raids through the battery's sector as scenarios played by the runner with the NPC crew, and the mission bridge back (`examples/air_war_demo.cpp`); seven sample raids in the missions menu ("Air war"); 14 tests |
| A10 | **Built 18 Oct 2026:** the Unreal campaign screen (`USimAirCampaignSubsystem`, `ASimCampaignHUD`, the missions menu's campaign box): map, INTSUM, dossiers, posture and relocation, the raid warning, fight or let pass, the reveal; raids played as missions and brought back. Left: a person playing a campaign through |
| A13 | **Done 18 Oct 2026:** save and load (`air_war/save.hpp`: the seed and the player's actions replayed exactly; saved after every step in the game) |
| A12 | **First pass done 18 Oct 2026:** the balance pass (`examples/air_war_balance.cpp`, `docs/air_war/09_BALANCE.md`): every adversary over 20 seeds and two battery postures; the believed air defences, wreck-and-replace repair, a front-line belt for the neutral, the Pact's surge, wartime SAM posture, radars per battery, guns at the target, NATO's ARM stock, the rollback resuming. A person playing a campaign through is the second half |
| A14-A15 | **Planned in the battery's war** (`docs/campaign/`): the battery's tasks given by the regiment's orders and the battery commander's planning (phase 14), the regiment's other batteries (17.5); our fighters in the battery's sky and in the enemy's escort need (17.2). The adversaries' doctrines and officers stay as built; they gain war aims in the data and one term in their target picking (phase 13). Our air force as a planner is not planned |
| A11 | **Done 20 Oct 2026:** the ARM as an EM missile and the crew's ARM drill (EM track E6). A SEAD or hunter aircraft with a warning receiver gets the targets' `"arms"` (its type's `arm_em`: the HARM class with a memory, else the Shrike class; the Pact's MiG-25BM the Kh-58 class), aimed where the enemy's intelligence has the battery; the mission bridge reads the runner's ARM results (vehicle hit, a dummy taken, a miss) and fires its own abstract ARMs only for carriers without (`docs/air_war/05_EXECUTION.md`) |

## Phase 12 - Our side's chain of command and the information pipeline (first person)

The project owner, 18 Oct 2026: the game is first person; intelligence, early warning and after-action reports reach
the player's character only through people and means of communication, with a realistic chain of command, senior
officers included, on a map of a small city, towns, villages, works and countryside (`docs/command/`, research
`docs/research/CHAIN_OF_COMMAND_RESEARCH_2026-10.md`).

| # | Task |
|---|---|
| C0-C4 | **Done in simcore, 18 Oct 2026.** Research and sources (`utin_2018`, `uvs_rf_2007`, `basta_lm`, `valka_lm`); the chain (38 posts from the national sector to a driver, the town's offices), the places (the city, the town, five villages, the posts), the means of signal (12) and the routines (briefings, visits, courier runs, the news) as data; the pipeline (`simcore/command/info_net.hpp`: facts travel hop by hop by the best means each pair has, routine news waits for contacts, carriers lost where bombs fall, wire cut, papers and summaries, the player's character passes nothing on by itself; deterministic); the air war's news fed in at its first knowers (`air_war_feed.hpp`); 6 tests |
| C5 | **Built 18 Oct 2026:** the campaign screen shows only what reached the player's character, who told them and how; their chain and its coming contacts; the raid warning as received |
| P1-P3 | Next: deliveries in the world (an officer who walks up and speaks, the telephone, the radio voice, the courier, papers, the evening briefing as a scene); the player telling people, by the means they choose; the pipeline's radio calls feeding the air war's intercept service (`docs/command/07_IMPLEMENTATION_PLAN.md`) |
| P9 | **After-action reports from the records** (project owner, 22 Oct 2026: `docs/command/01_PRINCIPLES.md` 9): the battery's combat report compiled from the radiotelephonist's journal (`simcore/radio_journal.hpp`), the board and the commander's log, never the truth; the regiment checks records and trusts each unit's reports as they have checked out; the crew's rating carries it. The radiotelephonist's accuracy decides what the regiment and the battery learn and plan: critical to the after-action loop. Next after P2; the regiment's staff who read the reports, and the next order they give, are the battery's war's 18.2 and 18.3 |
| P4-P8 | Later (`docs/command/07_IMPLEMENTATION_PLAN.md`): the crew's knowledge joined to the pipeline, radio hops through the EM layer, multiplayer posts, the district as a level, posts changing hands. The battery's war uses P1-P3 and P9 (phases 14-18) |

## The nation on the map: next (prioritised 22 Oct 2026)

The project owner, 22 Oct 2026: build out the map and ultra-basic placeholder assets, and combine them with the simcore
logic, first. This track comes **before** the battery's war and **is not gated** by M2, A10/A12 or P1-P3: it builds the
country of the briefing (`docs/campaign/10_BRIEFING_FORCES_AND_NATION.md`) as data, a map and placeholder models, and
makes the air war that is built today run on it. Phase 13's roles, rebuild needs and ledgers are then added to a country
that already exists.

| # | Task |
|---|---|
| N0 | **Done 22 Oct 2026.** The air war's new object kinds (`refinery`, `pipeline_station`, `substation`, `dam`, `materials_depot`, `port`, `government`, `broadcast`, `exchange`) with every faction's weights; the grid (substations, the dam) banned wherever power stations are, and NATO's escalation unbanning the whole grid; the air defence systems as a checked registry (`AirDefenceSystems()`: the S-75M2, S-125M1, Strela-1, Shilka, S-60, KS-30, ZU-23 and MANPADS beside the theatre's own), with the enemy's NATO names. Our order of battle's format and loader (`simcore/command/oob.hpp`, `data/command/oob_pannonia.json`): the services and units as a tree with their posts, theatre objects, equipment and establishment, a skeleton of what exists today (the Osa regiment's five batteries among it); every military theatre object owned by one unit, every post by at most one. The country map's format (N1) |
| N1 | **Done 22 Oct 2026** (`docs/COUNTRY_MAP.md`). `simcore/country_map.hpp` and `data/maps/pannonia.json`: the whole country, 331 by 321 km at 1 km, built in about 60 ms; its borders and who is across each (every enemy entry and base outside, the rebels' inside), the briefing's regions, the Rava, Lenta and Tisava, main roads, railways and the two oil pipelines as networks whose river crossings are named bridges that routes go round when cut; the capital on the Rava 59 km north of Kesmark, Novo Varosd, twelve towns, landmarks. The Kesmark range is a district of it: one ground function (`NaturalGround`) and the same terrain, equal to the last bit; its Rava and main lines point for point. `country_dump` (JSON, routes with bridges cut, heights) and `tools/map_view.py` for the country; `map_dump` added to the build. Tests: `tests/test_country_map.cpp` |
| N2 | **Done 22 Oct 2026** (`docs/COUNTRY_MAP.md`, "The nation's assets on it"). `data/air_war/theatre_pannonia_nation.json` **extends** the tuned theatre (a theatre file's `"extends"`: the base's objects in their places, replaced by id, `"remove"`; the air war keeps the tuned one until N4) with 130 objects, 170 in all: the National Air Defence (central and two sector posts with alternates, six S-75M2 and ten S-125M1 fire units, four KS-30 batteries, ten radar companies, Bory air base), the Air Force's bases, the Ground Forces (six regiments each with a Strela-1 and a Shilka platoon, Osa batteries 23-25, the 15th's technical battery, the S-60 regiment, the artillery, the tank brigade with the Kub, the army's brigades, pontoon park and rear, the territorial defence's guns), the rear services' depots and the industry and infrastructure of the briefing's §3, at the landmarks and crossings; the Strela-10 platoons become Strela-1, the S-75 and S-125 the M2 and M1. Every new object on open ground (rivers, towns, forests, and the Kesmark range's own map), **prepared positions** (`"positions"`) for every unit that moves on its own, which it moves between (`AirCampaign::Relocate`). Our order of battle filled to 97 units owning every military object. `tools/map_view.py` follows `extends` and draws the prepared positions. Tests: `tests/test_country_map.cpp` |
| N3 | **Done 22 Oct 2026** (`docs/scenery/README.md`). 30 new basic models, 109 in all: every equipment type of our order of battle has one (the Osa battery's 9V210M3, ZIP-2 and Strela-2s, the PU-12, the Shilka, the Strela-1's 9P31, the S-60 and KS-30 with their SON-9 and SON-30 and the AT-T, the D-30, the S-75, S-125 and Kub transloaders, the propellant tanker, the diesel power station, the P-18, P-15, P-14 and PRV-11, the PMP pontoon truck), and our aircraft are our own models in our colours, never the enemy's model of the same type (MiG-23ML, Su-22M3, L-39, An-26, An-2, Mi-24D, Mi-8 and Mi-17, Mi-2). The archetypes grew where a silhouette needed it (twin engines on the wings, several barrels, a dish's or array's own size, missiles lying on a transloader's bed, a `radar` post); the 79 earlier models' 473 parts are unchanged. Checked in the gallery (`-SimModelGallery`), whose camera now has a fixed exposure (its views had washed out white). Tests: `tests/test_scenery.cpp` |
| N4 | **Combined with the simcore logic**: the air war loads the built-up theatre (the new kinds with each faction's target weights, the bans unchanged); each defence resolved by its system in the abstract resolution; the regiment's radars and the warning nets from the new radar companies; raid scenarios written on the country's map with the neighbouring defences drawn and taking their kill chances (17.1's first form); the five Osa batteries and the regimental batteries in the battery's fire model where they are in the scenario; `air_war_demo` and `air_war_balance` run on it as a first read |
| N5 | **Unreal**: the country's map drawn beyond the Kesmark range at strategic scale; the campaign screen's map from the country's map; the new assets drawn with their basic models in raids and on the map |
| N6 | **Tests and tools**: the loaders and the new kinds; the map's agreement with the theatre and the places (`test_world_map.cpp`'s pattern, for the country); every asset on open ground; the gallery; the air war on the built-up theatre, deterministic; `tools/verify.ps1` and `verify.sh` |

**Done when:** `air_war_demo` against each adversary runs on the built-up country with every asset of the briefing in
the theatre; the country's map draws in Unreal with the Kesmark range inside it; the campaign screen shows the country;
a raid on the map shows its neighbouring S-125, ZU-23s and the Kub as basic models, and their kills appear in the
debrief; every new model shows in the gallery; the tests pass.

## The battery's war: phases 13-21 (planned 22 Oct 2026)

The project owner, 22 Oct 2026: our side and one enemy at a time fight over the fictional nation, the enemy grinding it
down (military forces, logistics, critical infrastructure, sovereignty and production) while its officers use their
intelligence to wear the defences down and strike. The strategy layer stays simple. The work is the game logic and the
implementation of the wider systems the Osa and PU-12M crews, vehicles and battery fit into. How does a mission start,
what are its objectives and why, what is attacked, defended, ambushed, jammed and reconnoitred around the battery and
why, and what makes it run seamlessly? Then, from the owner's further notes the same day: the crew as people
(fatigue and sleep, food, morale, progression) so the player can take different crew members across missions;
the battery's and the regiment's activities between fights (maintenance, briefings, recreation), interrupted at any
time, their scheduling part of the player's strategy; command between the layers that may break in combat and must be
improvised and rebuilt; doctrine by opponent; and the enemy's reach, from limited to near air superiority, as game
logic. And: the player cannot significantly change the wider war, only two outcomes (the survival of the assets they
protect, and the enemy's freedom of action, limited by surviving and threatening aircraft); many targets can be
rebuilt, but rebuilding takes time, resources, logistics and manpower that can be targeted too. Design: `docs/campaign/` (start at its README; done-when in
`docs/campaign/09_IMPLEMENTATION_PLAN.md`).

**What stays as built.** The four adversaries, one per campaign, with their doctrines, phases, target weights, bans,
depth limits, intelligence, officers and politics as their files have them (`docs/air_war/01_FACTIONS.md`), and their
officers' logic (`docs/air_war/02_AIR_STAFF.md`, `04_PLANNING.md`). The plan adds their war aims to the data and one
term to their target picking: what a target's loss would cut (`docs/campaign/01_WAR_AIMS_AND_TARGETS.md`).

**Starts after:** the nation on the map (above, not gated: it comes first), M2 (a person plays the missions through),
a person's playthrough of the one-battery campaign (A10, A12), and phase 12's P1-P3; P9 before phase 18. E8, E9, the scenery's models, 8.14's next steps and the other
leftovers can run alongside.

## Phase 13 - Why: war aims, roles, rebuilds, and what the battery changes (planned)

Every objective needs a reason that can be traced: the enemy's war aims → its officers' target picking → its raids,
and our objects' roles → what the regiment covers (`docs/campaign/01_WAR_AIMS_AND_TARGETS.md`, `02_WHAT_WE_DEFEND.md`).
The player cannot change the war's course. They change two outcomes, which the game measures: the survival of what
the battery covers, and the enemy's freedom of action, limited by the battery surviving and threatening (`01` §6,
`02` §5). Much can be rebuilt, at a cost in teams, materials, transport and time that the enemy can strike (`02` §3).

| # | Task |
|---|---|
| 13.0 | **Research**: what an Osa regiment covered and how its objects were ranked (`voenizdat_osa_1991` ch. 6, other operator documents); the objects' roles in the fictional country are estimates |
| 13.1 | **War aims** in each faction file (`aims`: text, and the phases it drives), shown at the reveal and in our analysts' estimate |
| 13.2 | **Object roles**: `supports` with alternatives and supply shares; the new objects the roles need (the technical battery at the Horka siding, a ferry) |
| 13.3 | **The worth term** in the enemy's target picking: what an object's loss would cut, as its picture believes it; bans and depth limits first |
| 13.4 | **Consequences into the built systems**: regiment radars to the PU-12M's board, the warning net to the c2 outages, bridges to the TZM's and ATZ's runs, the depots and the technical battery to the stocks, power to the positions' diesels; each a fact at its first knower |
| 13.5 | **Rebuilding**: jobs that need teams, materials, transport and time; temporary fixes; the division's priorities; the new kinds (engineer and railway units, materials depots, pontoon parks) in each faction's weights; re-strikes on repair and on its means; military units still replaced from the reserve |
| 13.6 | **The two ledgers**: assets kept (the covered objects' hours, hits and releases prevented, rebuilds finished under cover) and freedom of action denied (per package, what battery 21's believed threat changed: SEAD, decoys, jammers, route, height, rejection, ARMs, DEAD and hunter sorties, aircraft lost and damaged; per day, the suppression phase held open), by re-planning without the battery's entry |
| 13.7 | **The nation built up**: built first by the nation-on-the-map track (N0-N6, above) from the briefing (`docs/campaign/10_BRIEFING_FORCES_AND_NATION.md`); 13.7 adds what phase 13 brings to it: the roles (13.2) and rebuild needs (13.5) of every new object, fuel as a national stock fed mainly by the Pact neighbour's pipeline, Soviet missiles and spares cut in a war with the Pact, personnel establishments, intakes and reservists |
| 13.8 | **Balance**: each faction's choice of targets still in its doctrine's shape (`docs/air_war/09_BALANCE.md` §2); the ledgers against a careful and a careless battery; re-run on the built-up nation |

**Done when:** with the rail bridge down, NATO weighs the road bridge higher and its package's reasons say why; a
wrecked P-40 leaves the PU-12M's board blind beyond the P-19 and the battery commander hears of it from a person; no
ban is ever lifted by the new term; a rebuilt bridge comes back later when its materials depot is struck; a careful
battery's ledger against NATO shows the SEAD, ARMs and sorties it drew, a careless battery's ends the day it is
wrecked.

## Phase 14 - The battery's mission (planned)

How a mission starts: the regiment's order with its reasons, delivered through the chain; the battery commander's
sequence and working map (`voenizdat_osa_1991` pp. 232-245); the mission kinds; objectives and their measures
(`docs/campaign/03_THE_BATTERY_MISSION.md`).

| # | Task |
|---|---|
| 14.0 | **Research**: the regiment's tasking of its batteries and the combat order's form |
| 14.1 | **`BatteryTask`**: kind, covered object, reasons, situation (attack directions, helicopter, jammer and ARM lines), area (main, reserve, dummy positions), sectors, readiness and emission regime, time, coordination (neighbours, the friendly air corridor, IFF, call signs, nets), reports, support; a document in the pipeline |
| 14.2 | **The regiment's tasking**: a rule table with reasons, run by the regiment's NPC officers with their traits and quirks, at the evening briefing and on urgent triggers (an uncovered priority object, the covered troops moving, a corridor the analysts named, the enemy knowing a battery, ARM attacks, low stocks, the enemy's phase changing, a battery lost) |
| 14.3 | **The battery commander's planner**: the sequence, positions on the map with sight along the attack directions, sectors, dummies, regime, the oral combat order, an ideal timeline; the NPC battery commander, and the helper's shadow for a player |
| 14.4 | **The working map** in the PU-12M: marks with their source and time, beside the board |
| 14.5 | **The mission kinds** mapped to the battery's orders and drills: cover an object, cover troops, cover a march, cover a crossing, ambush (and roving), relocate; the end of a task |
| 14.6 | **Objectives and the mission debrief**, by the player's two outcomes. Assets kept: the covered object, troops, column or rebuild, and the releases prevented. Freedom of action denied: the battery kept and how well the enemy knows it, the ledger of what it cost the enemy, reconnaissance denied. Then missiles and the record. Each with its why chain, no points; the mission kind "cover a rebuild" |

**Done when:** `mission_demo` runs a cover task headless from the regiment's 19:00 order to «к бою готов» by the
ready-by time; a person as battery 21's commander receives the order at the briefing, works the map and gives the
oral combat order to NPC vehicle commanders who carry it out.

## Phase 15 - The continuous mission (planned)

One session for the whole task instead of one scenario per raid: the battery's state carried through it, the
enemy's packages entering it live, time compressed between events (`docs/campaign/05_THE_SYSTEMS.md` §6-8).

| # | Task |
|---|---|
| 15.1 | **`BatteryState`** in the campaign (positions, readiness, missiles per container, fuel, damage, dummies, the board, the journal, the crew's experience), replacing the posture-only battery |
| 15.2 | **`MissionSession`** and the `MissionScenarioBuilder`: one runner for the whole task and on into the next (the battery's days continue in it), built from the task and the battery's state |
| 15.3 | **Packages entering live**: the runner's `AddTargets`, `AddJammer`, `AddTrigger`; the raid builder split into its scenario part and a `PackageInjector`; the bridge per package; the campaign handing packages to the session instead of stopping at one pending mission |
| 15.4 | **Time**: compression and the events that wake the player; a coarse quiet step; both measured |
| 15.5 | **Levels of detail**: the regiment's other batteries at L1; L2 sessions headless; `war_calibrate` comparing L1 and L2 on the same packages (closes `09_BALANCE.md` §6) |
| 15.6 | **Districts** for the rebels' and the neutral's fronts (70-110 km from the Kesmark range), generated like the test range |
| 15.7 | **Saves** of a mission in progress |
| 15.8 | **Unreal**: the session in the level from the order to the report |

**Done when:** one task runs as one session from the order to the end, with three packages entering live and a reload
and a relocation between them; an injected package matches the same raid built as a scenario within the calibration's
bounds; a person plays a 24-hour cover task, woken only when something comes.

## Phase 16 - The enemy around the battery (planned)

Everything the enemy does near the battery as a real thing with a reason in its plan: strikes, DEAD, hunters, decoys,
feints, reconnaissance, ELINT, radio intercept, jamming (`docs/campaign/04_WHAT_HAPPENS_AND_WHY.md`).

| # | Task |
|---|---|
| 16.0 | **Research**: the period's jamming of voice nets and air reconnaissance practice, each side's own documents first |
| 16.1 | **Reconnaissance in the scenario**: photographs of what it overflies, delivered to the enemy only if it gets home |
| 16.2 | **Voice jamming** carried by packages where 16.0 supports it; the nets' drills answer; the jammer lines on the working map |
| 16.3 | **ELINT and COMINT** from the session's real emissions each minute and its real radio calls (P3) |
| 16.4 | **Each faction as its file has it**: no jammers or SEAD for the rebels and the neutral; the Pact's reconnaissance leads and suppression groups; NATO's decoys first |
| 16.5 | **Balance**: `mission_balance`, each faction × mission kind |

**Done when:** an RF-4C shot down before it gets home leaves the next strike planned on the old fix, and the reveal
says so; a Pact strike's jammer takes the battery net down and the spare-frequency drill brings it back; a battery that
radiates through a NATO morning is found by noon.

## Phase 17 - Our side around the battery (planned)

The layered defence, the warning, our fighters and the supply the battery depends on
(`docs/campaign/02_WHAT_WE_DEFEND.md` §2, `05_THE_SYSTEMS.md` §10).

| # | Task |
|---|---|
| 17.1 | **Neighbours' fire** on aircraft crossing their rings (the ZU-23 at the bridges, battery 22, the Strela platoons, the S-125), logged as theirs, heard on the net or seen |
| 17.2 | **Our fighters**: the friendly corridor in the order, IFF codes and their changes, the fighter regiment's notices through the pipeline, friendly aircraft in the scenario, friendly fire when a notice or the codes did not arrive |
| 17.3 | **The warning net and the regiment's radars** carried through the session with their outages |
| 17.4 | **Supply over the real roads**: the TZM and the ATZ from the stocks, round wrecked bridges |
| 17.5 | **The regiment's other four batteries** as units with their own tasks and NPC commanders, at L1 |

**Done when:** a raid over the bridges loses an aircraft to their guns and the net says so; a MiG-21 in the corridor
answers IFF on the player's scope, and a battery left without the notice fires on it; the TZM's run is longer the day
the road bridge falls.

## Phase 18 - The after-action loop (planned)

The fight, the record, the report, the regiment's picture, the next order (`docs/command/01_PRINCIPLES.md` 9;
`docs/campaign/05_THE_SYSTEMS.md` §11-12).

| # | Task |
|---|---|
| 18.1 | **The report from the records** (P9) after every engagement and each day, sent up by the post's means |
| 18.2 | **The regiment's picture** from the reports: the analysts' patterns, the enemy's estimated aims and phase, a trust per unit kept by checking records against one another |
| 18.3 | **The next order** from that picture: 14.2's rules fed by 18.2, never by the truth |
| 18.4 | **Consequences delivered** as facts: the bridge stood, the train passed, the depot is empty |
| 18.5 | **Our side's journal** in the air war from the reports |
| 18.6 | **A person plays a campaign at battery 21 through**, in a seat and in command |

**Done when:** a battery whose radiotelephonist lost the 05:12 raid sends its regiment's next ambush to the wrong
corridor; a battery whose records check out gets the hard sector; a person plays a week against NATO at battery 21 and
can say why every order came and why every raid did.

## Phase 19 - The crew as people (planned)

The battery's people with their qualifications, experience and needs (sleep, food, morale, health), on duty or resting,
always somewhere; the player may take any fit member of the battery across missions (`docs/campaign/06_THE_CREW_AS_PEOPLE.md`).

| # | Task |
|---|---|
| 19.0 | **Research**: the period's daily routine, rest on duty and meal norms, how many people each readiness level keeps at the posts (`pvo_bu_ix_1984` and other operator documents); public human-factors studies of sleep loss and vigilance |
| 19.1 | **The roster**: every member of battery 21 as a person: qualifications, experience, condition, place, duty, knowledge |
| 19.2 | **Needs and the effective tier**: sleep, food, morale and health as multipliers on the skill dimensions, like `stress_factor` |
| 19.3 | **The duty roster, and readiness as a computed time**: who is on watch at each readiness; the time to the posts from where each person is (`crew_mobility.hpp`, waking) |
| 19.4 | **Progression per person**: experience per seat, classes by threshold and exam, cross-training, people moving up, replacements as recruits |
| 19.5 | **Whom the player plays**: any fit member of the battery for a mission or an activity, with that person's knowledge; the main character's career apart |
| 19.6 | **Needs heard, not shown**: reports and voices, slower acknowledgements |
| 19.7 | **Balance**: `crew_balance` with needs |

**Done when:** a 1st-class operator kept awake for two nights works like a 3rd-class one and their vehicle commander
says so; readiness No. 1 from a sleeping battery takes as long as its people's walk; a person plays the driver on a
night march and the radiotelephonist the next morning, each as tired as that person really is.

## Phase 20 - Between fights (planned)

The battery's day: watch, sleep, meals, maintenance, repairs, reloads, digging, wire, briefings, training, guard and
recreation, each holding people and equipment away from readiness; scheduled by the battery commander, broken into by
the war at any time (`docs/campaign/07_BETWEEN_FIGHTS.md`).

| # | Task |
|---|---|
| 20.0 | **Research**: the textbook's maintenance pages (`voenizdat_osa_1991` pp. 206-224), the vehicles' and the PU-12M's servicing, the battery's daily routine |
| 20.1 | **Activities as data**, run as jobs at places: who, where, what they leave open or away, how long, what they give, how fast they are dropped, what is lost |
| 20.2 | **Readiness computed** from every person's and every piece of equipment's state |
| 20.3 | **Wear and faults**: hours radiating, turbine hours, kilometres, held missiles; fault chances growing since the last maintenance |
| 20.4 | **The schedule**: the NPC battery commander's daily routine given out in the morning orders; the player's schedule in the PU-12M; the helper's shadow; the vehicle commanders' shifts |
| 20.5 | **Interruptions**: alarms, warnings, raids and orders breaking into any activity or task; drop, fight, resume or reschedule |
| 20.6 | **The regiment's day**: the technical battery's tests and loads, the rear's runs, the routines; the battery under its deputy while the commander is at the briefing |
| 20.7 | **Balance**: scheduling strategies against each adversary |

**Done when:** a radar opened for maintenance at dawn against "the dawn raider" misses the first minutes of the raid and
the debrief says why; a battery that skipped maintenance for three days has more faults; a person plans a day against
the Noric Republic and another against NATO, and the two days differ for reasons they can name.

## Phase 21 - Reach, doctrine, and command under strain (planned)

The enemy's reach as game logic, from limited to near air superiority; our doctrine per opponent; command between the
layers that may break, with improvisation and reconstitution (`docs/campaign/08_COMMAND_DOCTRINE_AND_REACH.md`).

| # | Task |
|---|---|
| 21.0 | **Research**: the period's doctrine against each kind of enemy; succession, the alternate post (ЗКП) and action when contact is lost (`utin_2018`, `pvo_bu_ix_1984`, other operator documents) |
| 21.1 | **The enemy's reach**: depth limits, the hours its aircraft can fly, its sorties, air superiority after its phase changes; a battery moving by day under air superiority may be seen |
| 21.2 | **Our threat estimate** by area and hour, from the reports |
| 21.3 | **Doctrine profiles** per adversary: readiness by hour, emission, moves, deception, cover priorities, ambushes, rest and maintenance windows, radio and road discipline; changed as the regiment's picture changes, at the pace of its officers' characters (14.2 runs on one default until this) |
| 21.4 | **Command that breaks**: holders of posts who fall, command posts as objects (the regiment's CP added), the PU-12M, wire, radio, couriers, a commander away |
| 21.5 | **Improvisation**: contact clocks for NPC commanders; readiness on own initiative, independent search and fire in the sectors, the last order and its intent, the next means, the next senior; the helper's shadow for the player |
| 21.6 | **Reconstitution**: communications back, succession then replacements (P8), the alternate post, units merged, confirming orders |
| 21.7 | **Balance**: each adversary with the regiment's CP lost on day 2 |

**Done when:** against the rebels nothing comes beyond 80 km, while against NATO after the rollback a battery moving
by day is found; the first ARM attack changes our doctrine within a day; with the regiment's CP destroyed, battery 21
takes readiness No. 1 on its own, fights in its sectors, sends a runner, and receives the alternate post's orders.

**Order:** 13, 14, 15; then 16, 17 and 19 in parallel; then 18 (with P9) and 20; then 21. **Milestones**
(`docs/PROJECT_PLAN.md`): M5, the battery's war (13-18); M6, the battery's life (19-21). Not planned: the regiment's
posts for the player, the neighbours at full detail, our air force as a planner, a ground war simulation, an
economy, more than one enemy at a time.

## Scenery: ultra-basic models of every asset (18 Oct 2026)

`data/scenery/basic_models.json`, `simcore/scenery/basic_models.hpp`, `FSimBasicModels` (`docs/scenery/`): 79 models
built from primitives (every aircraft the four adversaries fly, our MiG-21, missiles, the Osa battery's vehicles, our
other air defences and army vehicles, buildings, trees, bridges); scenario targets name their model; the director draws
aircraft, missiles and battery vehicles with them at their real size. Checks: `test_scenery.cpp`, the gallery
(`-SimModelGallery`), scenario photographs (`-SimShots`). First test on the Kesmark range map (`/Game/Maps/KesmarkRange`,
the map workstream): the NATO and Pact sample raids played on it with the NPC crew, photographed; the campaign's raids are
written on the map's ground and fought in that level, the battery and its dummies set on the nearest open ground (the
test found a battery inside a wood); the game mode's stand-in pawn no longer floats over the battery. Next: real models
where they matter most (the Osa, the PU-12M and the TZM have them), the player's own Osa as the 9A33 model, wrecks, the
map's sites drawn with these models, woods in the sim's line of sight (E9).

## Electromagnetic (EM) track

Radar, passive sensing, EW, communications and command as one simulated environment
(`simcore/include/simcore/em/`, `docs/EM_ARCHITECTURE.md`). Phases chosen from the research
(`docs/research/EM_RESEARCH_2026-10.md`): the order is "get the Osa's own radar picture from the
physics first, then the battery's C2, then missiles and optics, then threats and deception", because
each later phase needs the earlier one's knowledge model to be right. Difficulty S/M/L; cost per
tick at the 500-aircraft benchmark (14 ms mean today on one core).

| Phase | Objective | Systems | Depends on | Difficulty | Perf cost | Testing | Done when |
|---|---|---|---|---|---|---|---|
| **E0** (done, 7 Oct 2026) | Research, architecture and a minimum viable EM layer | Propagation, calibrated radar, ESM, triangulation, TDOA, noise jamming, strobes, RGPO, chaff/MTI, tracking, knowledge/fusion, comms/C2, doctrine, overlays, Unreal demo mode | — | L | baseline | 47 new C++ tests (154 in all), 11 MVP checks, Python mirror | merged; Unreal demo visible with `sam3d.em.Start` |
| **E1** (first pass done 11 Oct 2026: `data/em/nato_1985.json`, `docs/research/EM_E1_NATO_DATA_2026-10.md`; FM 100-2-3 read 11 Oct 2026: NATO view of 13 Soviet radars, low confidence. **18 Oct 2026:** the Osa's two command transmitters (СПК) on the vehicle's grid frequencies, `mon_2112_80` p67, and the 9M33's responder, estimates as marked; NATO's command-guidance entry is what an RWR calls a launch. Left for want of a registered source: the bands of the ZSU-23-4's Gun Dish and the SA-9/13, named NATO variants, Pact jammers, the R-111's band) | Data hardening | Blue threat library from period sources (FM 100-2-3 etc.); generic-but-period NATO aircraft radars, RWRs, pods; Osa open items where sources appear; ESM band plans | E0 | S | none | data tests with provenance | every library value cited or labelled |
| **E2** (done 11 Oct 2026, PRs #28, #29; review fixes, jammers, chaff, IFF and the unit as a node added after: `docs/EM_ARCHITECTURE.md` §25) | The Osa's radar picture from physics | `ScenarioRunner` owns an `EmWorld`; console controls drive EM state (`soc.hv`, `soc.beam`, `soc.mti`, `iff.*`, `ssc.tx`, `fo.cu_az`); the engagement model's search picture and lock read EM plots and tracks instead of fixed thresholds; outcome resolver unchanged | E0, 6.1 | M | +1–2 ms | practice scenario: lock ranges in the published bands; procedure demos unchanged | practice scenario plays with EM-driven scopes and lock (this is most of 6.4) |
| **E3** (done 18 Oct 2026, `docs/EM_ARCHITECTURE.md` §32: echoes painted by their signal to noise and jamming, jamming strobes on the PPI, the sweep and the vehicle's search antenna following the EM antenna, the scenario's own EM world drawn on its terrain with `sam3d.em.Scenario 1`; no Gameplay Debugger category: the console overlays stand in) | Unreal presentation | PPI echoes from plots (brightness from SINR), strobes, IFF replies, range indicator from the SSC; overlays on real Cesium terrain; Gameplay Debugger category; antenna rotation from the emitter state | E2 | M | GPU only | scripted `ue_cabin_check.py` views | the cabin scopes show EM output |
| **E4** (done 18 Oct 2026) | Battery C2 and PU-12 | PU-12 node and plotting board fed by a regiment link and a P-19/P-40; voice designations "azimuth, range" as messages to commanders; battery net; four vehicles | E2, phase 7 | M | +0.5 ms | C2 scenarios: latency, saturation, loss of the PU-12 | phase 7 runs on EM messages | **Done 18 Oct 2026:** selectable early-warning and command sources (roadmap 7.8), voice designations as EM radio messages and the four-vehicle battery net (`docs/EM_HANDOFF.md` §15), the PU-12 lost (`osa_practice_c2.json`), a saturated net measured (`test_c2_sources.cpp`: with eight targets the crew aims by marks three and a half minutes old, with three by the last plot's).
| **E5** (done 18 Oct 2026, `docs/EM_ARCHITECTURE.md` §33: the TV sight as a passive optical sensor, Koschmieder and Johnson, scenario `environment`; the two command transmitters on from ВНИМАНИЕ into their loads and radiating from the launch (`voenizdat_osa_1991` p21, corrected 20 Oct 2026), СМЕНА ЛИТ. on their frequency; missiles in flight with their responders; a target's RWR warns of the lock and the launch) | Missiles and optics | Missile platforms with responders (ССР), the command transmitter (СПК) as an emitter, the TV sight as a passive optical sensor (angles, visibility), method Н | E2, 6.3, 6.6 | M | small | guidance-channel lamp sequence from EM states | launches visible to ESM; TV tracking without emission |
| **E6** (done, 18 and 20 Oct 2026, `docs/EM_ARCHITECTURE.md` §34-35: decoy drones the TV unmasks, doctrine `tv_check`; a stand-off jammer escort; self-protection pods (9.1); emission discipline, `search_bursts`; scenarios `osa_decoys.json`, `osa_soj_strike.json`. **ARMs** as EM missiles, `em/arm.hpp`: a passive seeker on the signal its carrier heard, sidelobes, decoys inside its resolution, a memory point (HARM class) or ballistic (Shrike class), a lofted flight, the fuze; carriers fire on their warning receivers and turn away. **The ARM drill** (voenizdat_osa_1991 pp262-264): the senior operator's signs on the scope, the commander's «Излучение выключить!» after the burst, the least emission under doctrine `arm_threat` (two-turn bursts, Table 7.3, the tracking transmitter off after the burst), the attack reported up, a change of position on the regiment's standing order; scenarios `osa_arm_drill.json` (HARM drill), `osa_arm_drill_shrike.json`; the air war's A11. the signs on the tracking radar's indicators and on the TV (21 Oct 2026). Not built: the TV for tracking under the threat) | Threats | ARMs homing on emitters (with memory after shutdown), decoy drones, self-protection pods, SOJ escorts; doctrine: ARM detection, blink, relocate | E5 | M | small | scenarios: Lebanon-style decoys, HARM drill | phase 9.2 |
| **E7** (done 16 Oct 2026, `simcore/em/deception.hpp`, `docs/EM_ARCHITECTURE.md` §29: decoys on the radar's parameters and their blinking modes, frequency change vs a re-tuning spot jammer, false-target repeaters, dummy radio traffic vs intercept/DF; VGPO not applicable; dummy positions = decoy + ground platform) | Deception and EMCON depth | Decoy emitters on the same parameters (Czechoslovak doctrine), dummy positions, frequency changes vs spot jammers, repeater false targets, VGPO, comms deception and the intercept value of voice | E6 | M | small | deception scenarios produce ghost fixes / misidentifications | phase 9 complete |
| **E8** (started 16 Oct 2026: ESM sites and search radars on worker threads, identical results at any count; bench 500 aircraft / 60 SAM radars / 24 ESM sites: 14.7 ms per tick on one core, 5.7 ms on 4 threads, 4.2 ms on 8; left: time-slicing the cold paths, hierarchical grid, LOD) | Scale | Threads for ESM/radar systems, time-sliced cold paths, hierarchical max-height grid, level of detail for distant units | any | M | goal: 500 aircraft ≤ 5 ms mean | `em_bench` in CI as a trend | goal met with identical results single- and multi-threaded |
| **E9** (started 16 Oct 2026: Deygout three-edge diffraction with distinct-crest sub-edges, ITU-R P.838 rain; left: forest/urban obstacle heights, sea areas) | Environment depth | Deygout multi-edge diffraction, forest/urban obstacle heights from land cover, rain at Ku band, sea areas | E2 | M | +10–30 % path cost | propagation tests vs ITU-R examples | — |
| **E10** (done 16 Oct 2026: `simcore/em/replication.hpp`, Unreal `USimKnowledgeComponent` transport, `docs/EM_ARCHITECTURE.md` §30; networked check and the multiplayer crew done 16 Oct 2026: `docs/MULTIPLAYER.md`) | Multiplayer knowledge | Server-side simcore; replicate each player's knowledge, never truth | multiplayer crew | L | network | soak tests | later |

## Later

Phase 4 leftovers (Chaos Vehicles with a real model, performance check); Osa-AKM variant (9M33M3, hovering targets);
ZSU-23-4 Shilka (shares the PU-12); mission editor; multiplayer crew (seats, console mirror and per-player knowledge done 16 Oct 2026, `docs/MULTIPLAYER.md`; left: scopes from the knowledge stream, voice, a lobby). After the battery's war (`docs/campaign/09_IMPLEMENTATION_PLAN.md` §4): the regiment's posts for the player, the neighbours at full detail, our air force as a planner.

## Parallel track (any time)

- **Creator outreach:** drafted in `docs/outreach/creator_message_draft.md` (VB6 source or notes, art permission,
  the source conflicts, operator contacts); the user reviews and sends it.
- **Acquire documents:** the BSU textbook *Боевое применение ЗРК 9К33М3* (2020), the GDR troop handbooks (1984,
  1990), the Angola memoir collection. Summaries go into `docs/re/osa.md` with page references.
- **SAM Simulator archive** (`reference/samsimv9302.rar`) and its documentation set: keep using them as reference
  for console behaviour; nothing from them enters git.
- **Reverse-engineering** (fallback only, and only with the creator's OK): see `REVERSE_ENGINEERING.md`.
- **Tooling:** keep `verify.ps1`/`verify.sh`, CI and the Unreal scripts (`tools/ue_remote.py`,
  `tools/ue_cabin_check.py`) current.

## Decision: how far the physics goes (resolved)

The user chose the **gameplay-level route** (phase 3 option 1, `e5271e7`): operators win or lose by how well they run
the procedures, track and engage, and the resolver turns that into outcomes with tunable numbers in `data/gameplay/`.
Hit chances are calibrated to published single-shot ranges (phase 6.7); detailed weapon-performance physics is not
scheduled.

## Risks

| Risk | Mitigation |
|---|---|
| Phase 6 changes break saved data and tests | One task per PR; tests for each correction; data format changes documented in the READMEs |
| Real timings make the game slow | Time compression (as SAM Simulator's F1–F4) and scenarios that start at different readiness states |
| Manual TV tracking is too hard | NPC operators can track; an assist option if playtests demand it |
| Creator never replies or says no | Original art and procedures from operator sources; the research file covers most behaviour |
| Unreal C++ build breaks unnoticed | Build from the command line after every Unreal change; scripted checks |
| Solo part-time scope | Vertical slice only; phases 9–10 wait until M2 ships |
