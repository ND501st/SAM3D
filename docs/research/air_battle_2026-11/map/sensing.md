# What an aircraft and its pilot can sense, and what emits, in simcore today

Map for the air-battle request (15 Nov 2026). Read-only survey of `C:\dev\SAM3D-wt\air-battle` at `1d09489`.
Paths are relative to that worktree. "Today" means the code as it stands; nothing here was changed.

## 0. Short answers

| Question | Answer |
|---|---|
| What does an aircraft's warning receiver hear today, and from which emitters? | Only scenario targets that name an `"rwr"` carry one (an `em::EsmState` of kind `Rwr` on the target's EM platform, `em_link.hpp:381-397`, `755-763`). It hears every emitter in the session's one EM world that radiates in its band (2-18 GHz for both RWRs in the data): **our own vehicle's** search radar (СОЦ), tracking radar (ССЦ), the two command-link transmitters once they radiate, each 9M33's responder (only within a few hundred metres), decoy emitters, and other aircraft's noise jammers. It does **not** hear the battery's other launchers, the regiment's neighbour batteries, the testbed's Kub/S-75/S-125/guns, any AAA radar, the PU-12M's P-19 (below band) or any radio. It warns the pilot of only two things: a **lock** (steady illumination from an entry of kind `track`/`fire_control`) and a **launch** (an entry of kind `missile_guidance`), `em_link.hpp:771-789`. |
| Is there a model of a pilot's eyes? | **No.** Nothing lets an aircraft see another aircraft, a smoke trail, a launch, flak, a ground vehicle or a site. What stands in for sight: a 15 km proximity rule for "the enemy saw our launch" (`mission_bridge.hpp:84-95`), post-raid `LaunchSighting`/`PilotReport` intel hours later (`campaign.hpp:1055-1064`), a chance per part of the day for armed reconnaissance finding a column (`logistics.hpp:69`, `movement.hpp:239`, `resolve.hpp:285`). A pilot without an RWR is cued to evade on the **truth** of our lock or launch (`scenario_runner.hpp:253-255`, `7206`). |
| Any airborne radar or IR sensor? | **No airborne radar operates in any session**, and **nothing infrared exists anywhere** in simcore (no IR signature, seeker, IRST, flare or IR missile). Two airborne fire-control radars exist as *emitter definitions only* (`nato_fighter_fcr_x` in `data/em/nato_1985.json`, `generic_airborne_fcr_x` in `data/em/generic.json`), radiated in the EM demo, tests and bench, never as an `em::RadarDef` that detects. No air-to-air missile exists in any data. |
| Can enemy aircraft radio calls be EM emissions our side intercepts? | **The mechanism exists in the EM layer, but nothing uses it for aircraft.** Any `em::NodeState` put on a net gets a `Comms` emitter that radiates while one of its messages is on the air (`world.hpp:256-283`, `comms_system.hpp:272-283`), and an ESM receiver of kind `comms_intercept` in band hears and DFs it. But no enemy aircraft is ever a node, no airborne radio is in the data, the only intercept receiver definition covers 20-100 MHz (not the air bands), no intercept receiver is placed in any scenario, and messages carry no content. |

## 1. The EM layer in brief (what all sensing is built on)

`simcore/include/simcore/em/`, design in `docs/EM_ARCHITECTURE.md` §1-22. Truth lives only in `em::Platform`
(`model.hpp:70`); sensors read it inside their system functions and emit observations; trackers, knowledge, doctrine
and UI work from observations. Fields named `debug_truth_*` are for overlays and tests only (`model.hpp:9-12`).

Layers: truth (`Platform`) -> observation (`RadarPlot`, `EsmIntercept`) -> sensor track (`SensorTrack`, `EsmContact`)
-> knowledge (`KnowledgeTrack`, per node) -> picture (`FusedTrack`). `knowledge.hpp:1-17`.

`EmWorld::Step` order (`world.hpp:331-399`): ARMs, platforms, EMCON steps, SAM doctrine, decoys, jammer tuning,
`UpdateEmitters` (scan), chaff, `RunRadars`, then every `esm_period_s` (0.2 s) `RunEsm` + `RunTdoa`, every 0.5 s
`IngestOwnSensors`, dummy traffic, `RunComms` (links every 1 s), knowledge ageing. Rates: `EmRates` `world.hpp:35-42`.

Key types (all `simcore::em`):

| Type | Where | What |
|---|---|---|
| `Side { Blue, Red, Neutral }`, `PlatformKind { FixedWing, RotaryWing, Missile, Ground, Decoy }` | `model.hpp:33-34` | `EmSensorLink` maps scenario targets to Blue and our unit/friends to Red (`em_link.hpp:387`). |
| `RcsModel` + `RcsToward` | `model.hpp:47-67` | nose/beam/tail RCS interpolated in dB by aspect. (A pattern reusable for an IR signature by aspect.) |
| `EmitterKind { SearchRadar, TrackingRadar, MissileUplink, MissileResponder, Iff, Comms, Jammer, Navaid, Decoy }`, `ScanKind { Fixed, Circular, Sector, Tracking, Omni }` | `emitter.hpp:27-28` | what radiates. |
| `EmitterDef`, `EmitterState` (`radiating`, `dummy_load`, pointing, `freq_hz`, `track_target_platform`) | `emitter.hpp:88-120` | a packet-level signal per tick (carrier, PRF, PW, power, pointing), not pulses. |
| `RadarDef` (role Search/Track, calibration anchor, MTI, clutter, tracker, IFF, ECCM) | `model.hpp:110-160` | calibrated radar (`Calibrate`, `radar_system.hpp:108`). |
| `EsmDef` (`kind` Rwr/Esm/Elint/TdoaSite/CommsIntercept, band, sensitivity, bearing sigma, threat library) | `model.hpp:161-193` | passive receivers. |
| `ThreatEntry`/`ThreatLibrary` (kind string, owner, `cw`, `max_speed_mps`, freq/PRF/PW windows) | `model.hpp:196-213` | one side's **belief** about the other's emitters. |
| `JammerDef` (Barrage, Spot, RGPO, FalseTargets) | `model.hpp:217-235` | noise and deception. |
| `RadioDef`, `NetState`, `NodeState`, `Message { TrackReport, Picture, Voice }`, `LinkState` | `model.hpp:236-541` | radio nets, C2 nodes, messages. |
| `ArmDef`/`ArmState` | `model.hpp:551-610`, `arm.hpp` | the only seeker-in-the-loop missile in the EM world. |

## 2. What emits today, and in which sessions

| Emitter | Created by | Band | In which sessions |
|---|---|---|---|
| Osa search radar СОЦ `osa_ak_soc` (circular, 7.0 GHz, PRF 2800, 0.45 us) | `EmSensorLink` ctor `em_link.hpp:177` | C/H | every scenario with an `"em"` block |
| Osa tracking radar ССЦ `osa_ak_ssc` (14.5 GHz) | `em_link.hpp:178` | Ku | same |
| Command links I/II `osa_ak_command_link_1/2` (8.6/8.2 GHz, 100 kW, `Tracking` scan along the tracking antenna) | `SetCommandLink` `em_link.hpp:668-695`: into the dummy load from ВНИМАНИЕ, radiating only from the launch | X | same, when the console's transmitters are on |
| 9M33 responder `9m33_responder` (14.2 GHz, 10 W, omni) | `SyncMissiles` `em_link.hpp:712-751`, one platform per missile in flight | Ku | same, while missiles fly |
| Decoy emitters copying the СОЦ (`DecoyMode` Always/WithRadar/WhenRadarOff) | `em_link.hpp:190-201` (`own_unit.em.dummies`, `.decoys`) | as the СОЦ | scenarios that place them |
| Self-protection and stand-off jammers (`generic_selfscreen_noise_ku`, ALQ-99 class bands 8-10, ...) | `EmJammerDef`, `AddStandOffJammer`, `AddJammers` (`em_link.hpp:209-226`, `1050`) | per def | scenarios and injected packages with jammers |
| Radio sources' and the unit's R-123M/R-111 voice (20-52 MHz) | `AddRadioSource` `em_link.hpp:232-245`; `SendVoice`, `SendVoiceFromUnit` `em_link.hpp:249-276` | VHF | scenarios with EM C2 sources |
| Anti-radiation missiles (platforms, no emitter) | `FireArm` `world.hpp:324` via `LaunchArmFrom` `em_link.hpp:833` | - | carriers with `"arms"` |
| Airborne fire-control radar `nato_fighter_fcr_x` / `generic_airborne_fcr_x` (sector scan about heading) | `AddEmitter` in `em/demo_scenario.hpp:114`, `examples/em_bench.cpp:82`, tests | X | **never in a played session** |

Not emitters anywhere: the battery's other launchers and neighbour batteries (`battery_fire.hpp` has no EM code; the
roadmap R4 row calls this "today's known limit", `docs/DEVELOPMENT_ROADMAP.md:299`), the testbed's Kub, S-75M2, S-125M1,
Strelas, MANPADS and guns (`FireNeighbourDefences`, `scenario_runner.hpp:2243`), AAA fire-control radars (no Gun Dish
or SON-9 emitter exists), IFF interrogators as emitters (`EmitterKind::Iff` declared, unused), enemy aircraft radios,
enemy AWACS/ELINT orbits (abstract shares, `docs/air_war/11_SUPPORT.md`). Each `ScenarioRunner` owns one
`EmSensorLink` (`scenario_runner.hpp:7327`); `battery_link.hpp` has no EM code, so the Osa and the PU-12M in a battery
link are two separate EM worlds.

## 3. The aircraft's radar warning receiver (the only airborne sensor)

### 3.1 How one is attached

1. Scenario target field `rwr` (`scenario.hpp:105`); load-time checks: needs `own_unit.em`, must be an ESM of kind
   `rwr` in the libraries (`scenario.hpp:1311-1317`).
2. The air war fills it from `AircraftType::rwr_em` (`air_war/data.hpp:333`): raids `raid_scenario.hpp:504-523`, the
   testbed's waves `test_waves.hpp:506-514`, injected packages via the raid scenario (`package_injector.hpp:7-10,109`).
3. `ScenarioRunner` -> `EmSensorLink::SetTargetRwr` (`scenario_runner.hpp:186`, `1952`; `em_link.hpp:755`); the ESM
   is made on the target's platform when the target first appears (`SyncTarget`, `em_link.hpp:395-397`).

Which types carry one (`data/air_war/faction_*.json`):

| Faction | RWR | Types |
|---|---|---|
| NATO | `nato_alr46_class_rwr` (2-18 GHz, -95 dBW, 10 deg bearing sigma, library `blue_rwr_nato_view`) | F-4G, F-4E, F-16A, F-111F, Tornado IDS, A-10A, F-15C, EF-111A, RF-4C |
| Warsaw Pact | `generic_rwr` (**PLACEHOLDER**, `data/em/generic.json`, library `blue_rwr_generic`) | MiG-23MLD, MiG-27, Su-24, Su-25, Su-17M3, MiG-25BM |
| none | - | NATO AH-1S, E-3A, KC-135, RC-135, cruise missiles, BQM-74; Pact MiG-25RB, Yak-28PP, Mi-24, A-50, Il-20, Kh-22M; every rebel and neutral type |

### 3.2 The receiver's physics (`esm_system.hpp:205-325`, `RunEsmSite`)

For each emitter `InAir()` (radiating, not into its dummy load), not on its own platform, in the receiver's band:
a free-space main-beam cull, then `Illumination` (`esm_system.hpp:108-148`: a circular scan counts as main beam if
the beam swept the receiver during the 0.2 s listening window; sector/tracking scans by pointing; omni always main
beam), EIRP toward the receiver by `AntennaGainDb`, `LinkLossCached` (terrain diffraction, gases, rain, ground
multipath, woods and buildings), received power vs `sensitivity_dbw`. An intercept gets a bearing (sigma widening
near threshold), measured frequency/PRF/PW, keyed draws (`DrawHash`, `Stream::EsmBearing`...). Intercepts associate
into contacts by measured parameters and bearing gates; contacts estimate scan period, flag `steady` after 1 s of
continuous main-beam illumination (`UpdateContact`, `esm_system.hpp:150-198`), store bearing lines every
`line_interval_s`, time out after `contact_timeout_s`, and are named by `Identify` -> `MatchLibrary`
(`esm_system.hpp:52-100`). The bearing is a true azimuth from the receiver; there is no antenna pattern of the
aircraft (no airframe shadowing, no quadrant ambiguity), no display, no priority logic.

### 3.3 What it calls each emitter (computed with `MatchLibrary`'s rule from the library windows)

`blue_rwr_nato_view` gives **band windows only** (no PRF/PW); ties go to the first-listed entry (`sc > best`).
`blue_rwr_generic` gives band, PRF and PW windows that do not match the Osa's data:

| Emitter | NATO ALR-46 class (`blue_rwr_nato_view`) | Pact placeholder (`blue_rwr_generic`) |
|---|---|---|
| `osa_ak_soc` | `sa8_land_roll_search` (search), conf 0.25 | **unknown** (7.0 GHz vs window 5.8-6.3) |
| `osa_ak_ssc` | `sa8_land_roll_track` (track), 0.95 | **unknown** (PRF 2800 vs 3500-4500, PW 0.225 vs 0.4-0.6 us) |
| command link I / II | `sa8_command_guidance` (missile_guidance), 0.33 / 0.20 | **unknown** (no entry near 8-9 GHz) |
| `9m33_responder` | `sa8_land_roll_track` (track) | unknown |
| `p19` (857 MHz) | `flat_face_p15` (search) if it were in band; it is below 2 GHz | unknown |

Consequence (by these windows; not run as a session): **a Pact aircraft's RWR never names any Osa emitter, so it never
warns of a lock or a launch.** Because `HasRwr` is true for it (`em_link.hpp:790`), the truth cue is also suppressed
(`scenario_runner.hpp:253`, `7206`), and its evasion is set to `On::Lock` (`test_waves.hpp:507`,
`raid_scenario.hpp:511`): such pilots never evade the Osa. The sample raids `data/scenarios/air_war_pact_dead.json` and
`air_war_pact_massed_strike.json` carry `"rwr": "generic_rwr"`. Its ARMs still fire, because `RwrRadarInBand` accepts
unknown emitters in band (`em_link.hpp:808-830`).

### 3.4 What reaches the pilot

- `EmSensorLink::WarningOf` (`em_link.hpp:771-789`): contacts heard in the last second, `hostile`, kind
  `missile_guidance` -> `launch`; `steady` and kind `track`/`fire_control` -> `lock`. Search paints, unknown emitters,
  jammers and bearings are not passed on.
- `ScenarioRunner::CueFromWarnings` (`scenario_runner.hpp:2799-2816`) every step: a debrief line on the first warning
  of each kind, then `CueTarget(id, launch)`.
- `CueTarget` (`scenario_runner.hpp:2929-2940`) -> `TargetPilot::OnLaunch`/`OnLocked` (`target_behaviour.hpp:117-122`)
  -> `Evade` (`target_behaviour.hpp:185-220`): pop-up helicopters drop behind cover; Jink: hard turns for `evade_s`;
  Break/Dive: **a beam heading to the line from `def_.unit_position`, the true position of our vehicle**, not the RWR
  contact's bearing. Evasion happens once (`evading_`), and only for the evade's `on` trigger (Launch, Lock, Order).
- Without an RWR (rebels, neutrals, helicopters, any target in a scenario without `"em"`): our radar lock and our
  launch cue the pilot directly from the engagement state (`scenario_runner.hpp:253-255`, `7206`): a truth cue.
- Never cued: the neighbours' and testbed's launches, flak and gun passes, smoke trails, other aircraft going down.
  The roadmap states it: "The pilot never reacts to the other sites, to flak, or to what he sees"
  (`docs/DEVELOPMENT_ROADMAP.md:284-285`). R3 (attitude) and R4 (defensive reactions to sighted launches and flak) are
  not built (`DEVELOPMENT_ROADMAP.md:298-299`).
- Own damage does change behaviour (testbed waves only): `OnDamage` (`scenario_runner.hpp:2621-2660`) jettisons the
  load and turns for home by chance from `data/gameplay/damage.json`; `ts.damage.smoke`/`fire` are recorded per target.

### 3.5 ARMs fired on what the RWR hears

`ScenarioRunner::UpdateArms` (`scenario_runner.hpp:2835-2905`): a carrier's `arms` fire when
`RwrRadarInBand` finds a contact in the weapon's band within `kArmBearingTolRad` (0.35 rad) of the **briefed** aim
(`t.arms.aim`, default `def_.unit_position`), within launch range; each ARM is tuned to the measured frequency/PRF
(`LaunchArmFrom`). After the last ARM the carrier turns away (waypoint 40 km out). The ARM's own seeker
(`arm.hpp:127-178`, `ArmSeekerLook`) hears in-band emitters within its FOV, cannot resolve emitters closer than
`resolution_rad`, smooths its line of sight, remembers the ground point if it has memory.

## 4. Pilot's eyes: nothing; what stands in for them

| Stand-in | Where | What it is |
|---|---|---|
| "The enemy saw our launch" | `mission_bridge.hpp:84-95` | any raid aircraft alive within `scenario.revealed_by_launch_m` (15 km, `data/gameplay/air_war.json:77`) of our vehicle when we launch -> `launch_seen_`; no light, weather, line of sight, direction or attention. |
| Launch sightings, pilot reports, claims | `campaign.hpp:995-1075`, `IntelSource::{PilotReport, LaunchSighting}` (`air_war/types.hpp:376`) | post-raid intelligence with `pirep_h` delay and fixed errors (700 m, 1.5-4 km); the enemy's staff learns, the flight in the air does not. |
| Armed reconnaissance finds a column | `ColumnFindChance` `air_war/logistics.hpp:69`; `nav::ArmedRecceSees` `nav/movement.hpp:239`; `Resolver::FindColumn` `air_war/resolve.hpp:285-310` | a chance by part of day, weather and night capability per mover within `search_m`; abstract (campaign) or per mover (nav), never from a flight's position, height or line of sight. |
| Visual attacks | `planner.hpp:164-197` | pop-up, dive and hover-fire deliveries refused in poor weather and less accurate at night by eye (`delivery.night_visual_factor`): a CEP factor, not a sighting. |
| Weapon release | `mission_bridge.hpp:120-150`, `test_waves.hpp` `Releases` | at the planned release point toward `aim_enu`, "where the enemy believed the target was"; no target acquisition in the air. |
| Recce photographs | `mission_bridge.hpp:124-126` | a recce aircraft at its release point "photographs the area" (a flag). Roadmap 16.1 (photographs of what it overflies) not built. |
| Our side's eyes, for comparison | `c2_sources.hpp:72`, `765-784`; `data/command/c2_sources.json` `observation_post` | a ground observer sees every aircraft inside a flat 6 km disk (estimate), no line of sight, light or weather. `nav::VisualRangeM` (`nav/session.hpp:92-95`, `data/gameplay/visual_range.json`): the player's character sees vehicles 8 km by day, 3 km at twilight, 1.5 km at night, capped by visibility (game values; used only to set mover detail, `scenario_runner.hpp:3367-3372`). |

There is no aircraft attitude in simcore (`Aircraft`, `aircraft.hpp:17-30`: position, velocity, speed, turn rate,
waypoints; R3 not built), so there is no cockpit field of view, canopy blind zone or look direction to build on yet.

## 5. Airborne radar, IR and air-to-air: what exists

- **Airborne radar**: no `RadarDef` for any aircraft. `UpdateEmitters` already scans a `Sector` emitter back and forth
  about the platform's heading ("an airborne radar", `scene_ops.hpp:57-70`); the demo's striker radiates
  `generic_airborne_fcr_x` and carries a `generic_rwr` attached to a Blue node (`em/demo_scenario.hpp:112-117`); tests
  show a red ESM site naming it `blue_fighter_fcr` (`tests/test_em_scenarios.cpp:145-167`). The Pact has no airborne
  radar entry at all; NATO's RWR library has no entry for a MiG radar; `red_esm_nato_view` has `nato_x_band_fcr`.
- **IR**: none. No signature, seeker, IRST, flare, IR SAM model (the Strela/Igla/MANPADS appear only as abstract kill
  chances, `air_war/data.hpp:51-58`, `scenario_runner.hpp:2169`, and as munition shapes `9m32m`, `9m36`, `9m39`).
  `docs/EM_ARCHITECTURE.md:362` lists IR seekers as "an optical sensor model (new system, same knowledge)".
- **Air-to-air missiles**: none in `data/scenery/munitions.json` or `data/air_war/weapons.json`. `missile.hpp` is a
  generic point-mass PN flyout placeholder (`missile.hpp:1-10`, `MissileParams` at `:20`).
- **Air combat**: abstract. The defender's fighters take a kill chance per aircraft of an unescorted package, cut by
  AWACS share and night (`air_war/resolve.hpp:231-247`); `AircraftType::air_to_air` (0..1, "value as an escort",
  `air_war/data.hpp:355`). Our side's fighters in the testbed (`cap`, `intercept` roles) fly routes and engage nothing
  (`test_waves.hpp:576-600`). AWACS and ELINT aircraft are shares of the day (`docs/air_war/11_SUPPORT.md`).

## 6. Radio: emissions, intercept and jamming as they stand

EM layer (`comms_system.hpp`):
- `AddNet` gives each member node without one a `Comms` emitter from the net's `RadioDef` (omni, the net's frequency;
  `world.hpp:256-283`). `RunComms` makes it radiate for exactly the time one of its messages is on the air
  (`comms_system.hpp:272-283`). So **any** node's transmissions are heard by in-band ESM, DF'd (`BearingFix` via the
  knowledge base) and, with TDOA sites in band, multilaterated. `AddDummyTraffic` makes a node transmit empty voice
  messages on a schedule (`world.hpp:227-236`).
- Messages: `TrackReport` (own-sensor tracks up), `Picture` (a C2 centre's fused picture down), `Voice` (timed by the
  sender, no payload) (`model.hpp:477`). Delivery by link margin `1/(1+exp(-margin/2))`, half-duplex nets, operator
  delays, relays, C2 status Connected/Degraded/Autonomous (`comms_system.hpp:239-337`). "Not modelled: what an
  intercept reveals by its content" (`docs/EM_ARCHITECTURE.md:586`).
- Radios in data: `r123m`, `r111` (`data/em/osa_ak.json`, 20-52 MHz, calibrated to ground ranges by
  `CalibrateRadio`, `comms_system.hpp:52-59`), `generic_vhf_fm`, `generic_telecode`. **No airborne radio** (no
  NATO UHF AM air set, no Pact air VHF/UHF set).
- Intercept receivers in data: `generic_comms_intercept` (20-100 MHz, PLACEHOLDER, library `blue_comint_generic` with
  one entry `red_vhf_voice`). Only tests and the EM demo place ESM sites (`em/demo_scenario.hpp:95`); no scenario key
  places an ESM or intercept receiver on our side.

Elsewhere:
- The enemy hears **us** (abstract): the air war's COMINT on the pipeline's radio calls (`campaign.hpp:1280-1305`,
  `intel.comint_per_call`), P5's listeners by link margin (`command/em_radio.hpp:1-11`), the battery nets' DF fix by
  time on air and a spot jammer's reaction (`radio_nets.hpp:273-300`, `375`).
- The enemy jams **us**: `airwar::VoiceJammersFor` and FM 34-40-7's J/S (`air_war/comms_jamming.hpp:37-188`), ground
  and helicopter jammers behind the front, on with each package.
- Our side's knowledge of the enemy's air forces' traffic: a paper ("intercepts; the analysts' notes on the enemy's
  officers") carried by the army's intelligence through the pipeline (`docs/command/06_INTEGRATION.md:15`,
  `docs/command/02_CHAIN_OF_COMMAND.md:28`), not derived from any emission.

## 7. Environment pieces a perception model can read

| Piece | Where | Use for a pilot |
|---|---|---|
| Sun and light: `SkyClock`, `LightAt` -> `SunLight{sun_elevation_deg, sun_azimuth_deg, phase, night_share, daylight, dark}` | `light.hpp:26-115`, `data/gameplay/light.json`, `docs/LIGHT.md`; runner `UpdateLight` `scenario_runner.hpp:1989-2003` | day/twilight/night sight; sun azimuth for glare and attacks out of the sun. No moon (`LIGHT.md` §6). |
| Visibility | `WeatherPreset::visibility_km` `weather.hpp:60`; `OpticalConditions` `em/optical.hpp:33` | Koschmieder contrast loss. |
| Cloud | `Weather::InCloud`, `CloudOpticalDepth`, `CloudTransmission` (`weather.hpp:185-241`; Kunkel extinction) | between any two points: an aircraft in or above cloud, a launch under a deck. |
| Line of sight | `HasLineOfSight` (`radar.hpp:83-128`) with `Terrain::ObstacleHeightAt`/`ObstaclesAlong` (`terrain.hpp:37`, `92`), 60 m end skip `kObstacleSkipM` (`terrain.hpp:80`) | aircraft to anything over terrain, woods and buildings (exact footprints on the map). |
| Optical detection | `em::ApparentContrast`, `AnchorThreshold`, `ContrastRangeM`, `LinesAcross`, `Look` (`em/optical.hpp:47-97`) | contrast and Johnson's lines, anchored to a stated range; written for the Osa's TV (575 lines, 0.3 contrast, estimates). |
| Signatures (presentation data, sourced) | `MunitionMotor{boost_s, sustain_s, smoke, sustain_smoke, tracer}` and `smoke_s` (`scenery/munitions.hpp:107-150`; `9m33m2`: 2 s boost, 13 s sustain, dense smoke, tracer); `fx/contrails.hpp:83-96` (Schmidt-Appleman); `fx/explosions.hpp:97-140` (smoke radius and time); damage smoke/fire | what a launch, a burst or a damaged aircraft looks like and for how long. |
| Events of what happened | the simulation's own sources: our launch (`scenario_runner.hpp:7195-7206`), the neighbours' fire (`UpdateNeighbourFire` `:10638`), the testbed defences (`FireNeighbourDefences` `:2243`), AAA passes and flak bursts (`aaa.hpp`), damage (`OnDamage` `:2621`) | The fx feed (`fx/events.hpp`) mirrors these but **the simulation must never read it** (`fx/events.hpp:6-9`). |

## 8. What could be reused

**Pilot perception (eyes):** `em::Look` generalised from TV lines to the eye's acuity (lines across = target angle /
resolvable angle; anchor the contrast threshold with `AnchorThreshold` to a sourced sighting range per target class);
`ApparentContrast` with `visibility_m`; `CloudTransmission` and `InCloud` between the cockpit and the object;
`HasLineOfSight` from the aircraft's altitude; `SunLight` for night and glare; munitions' smoke class and burn times
as the size and duration of a launch's visible trail; damage `smoke`/`fire` for a wingman going down; contrails for
high aircraft. Store what a flight saw as `em::KnowledgeTrack` in a Blue node's `KnowledgeBase` (new `InfoSource`
value for sight; `Category::Air/Ground/Emitter` exist), so fusion, ageing and `BuildView` (`em/replication.hpp:214`)
work unchanged. The ground observer's sense (`c2_sources.hpp:765-784`) shows the flat-disk shortcut to avoid.

**Air-to-air radar:** `RadarDef` + `Calibrate` + `RunSearchRadar`/`RunTrackRadar` + `Tracker` + `IngestOwnSensors`;
`Sector` scanning about the heading already works (`scene_ops.hpp:57-70`); a single-target track radar's steady beam
is exactly what an RWR's `steady` lock flag needs, so a fighter's STT lock warns its target with no new RWR code;
noise jamming J/N, chaff, RGPO and IFF apply as for ground radars. Missing: look-down clutter (today's clutter rule
is a ground radar's: `in_clutter` by elevation below `clutter_max_elevation_rad` within the radar platform's ground
horizon, `radar_system.hpp:192-205`; no pulse-Doppler main-lobe/side-lobe clutter or velocity gates, and
`docs/EM_ARCHITECTURE.md:585` says no radar here tracks in Doppler), elevation bars, an anchor form for an
air-to-air published range (`AnchorLook` assumes an antenna `antenna_height_m` over flat ground, `radar_system.hpp:77`).

**Air-to-air missiles:** `missile.hpp` PN flyout (placeholder); `arm.hpp`'s structure (seeker look in FOV, resolution,
LOS smoothing, memory, steering limit, max flight time, lethal radius, burst record) as the template for a
semi-active or IR seeker; `damage.hpp` for what a burst does; `fx/events.hpp` needs a new kind to show it.

**IR:** nothing to reuse directly; `RcsToward`'s aspect interpolation and `ChaffCloud`'s life (`model.hpp:446-463`)
are patterns for an IR intensity by aspect and for flares. Koschmieder is a visible-band law; IR transmission would
need its own source.

**Flight radio and intercept:** make each flight lead (or flight) an `em::NodeState` on a net with an airborne
`RadioDef`; `Picture` broadcasts from a `c2_center` node map onto GCI/AWACS control, `TrackReport` onto contact
reports, `Voice` onto timed calls; every call then radiates as a `Comms` emitter that our in-band `comms_intercept`
ESM sites hear and fix (`BearingFix`, TDOA). Content needs a payload (the pipeline's facts, `simcore/command/`), and
what our intercept learns must travel to the player through the pipeline.

## 9. Gaps

1. No pilot vision of any kind: aircraft, smoke trails, launches, flak, tracers, ground vehicles, sites, wrecks.
2. No airborne radar in sessions; no airborne radar data for the Pact; no air-to-air clutter or Doppler model.
3. No IR at all (signatures, seekers, IRST, flares, IR SAMs as fire, IR AAMs).
4. No air-to-air missiles, guns, BVR or dogfight model; air combat is one chance per package in the resolver.
5. Enemy aircraft are never EM nodes: no flight nets, no airborne radios in data, no intercept receivers of ours
   placed, no message content; intercept band 20-100 MHz only.
6. RWR warnings carry only lock/launch flags; search paints, unknowns and bearings never reach the pilot; evasion
   geometry uses the truth position of our vehicle (`target_behaviour.hpp:210`).
7. Neighbour batteries, testbed defences and AAA radars do not radiate, so no RWR can hear them (R4's known limit).
8. Pact RWR is a placeholder whose library names none of the Osa's emitters (section 3.3).
9. No aircraft attitude in simcore (R3), so no cockpit field of view.
10. Sightings for the enemy's intelligence are proximity rules and post-raid reports, not what a flight saw.
11. No live retasking channel: nothing changes a flight's route or target mid-flight except evasion, ARM turn-away,
    damage abort and the instructor (`scenario_runner.hpp:2897`, `2659`, `11038`).
12. NATO RWR library is band-only; ambiguous IDs are settled by list order.

## 10. Hooks

- `ScenarioRunner::Step` right after `CueFromWarnings()` (`scenario_runner.hpp:259`): a sight pass per enemy aircraft
  (and later a flight comms pass), reading the launch, burst and damage sources listed in section 7.
- `ScenarioRunner::CueTarget` (`:2929`) and `TargetPilot::OnLaunch/OnLocked/Evade` (`target_behaviour.hpp:117-220`):
  replace `def_.unit_position` with the perceived threat direction; allow more than one reaction; add jettison.
- `EmSensorLink::SyncTarget` (`em_link.hpp:381-397`): where per-target receivers are made; the place to add an
  airborne radar (`world_.AddRadar(pid, ...)`), and a node and net per flight (`AddNode`, `AddNet`).
- `EmSensorLink::WarningOf` (`em_link.hpp:771`): richer warnings (search, fire control, unknown, bearing).
- `airwar::AircraftType` (`air_war/data.hpp:318-372`): new fields (radar, IR sensor, radio, AAM loads, lookout,
  retasking agility), each with an `in_service_year` gate like `arm_em_by_year`.
- `data/em/`: new per-side libraries (airborne radars, Pact and NATO RWRs by type and year, air radios, intercept
  receivers) with `"provenance": "per_value"`.
- `FireNeighbourDefences` (`scenario_runner.hpp:2243`), `UpdateNeighbourFire` (`:10638`), `BatteryFire`: give the
  neighbours' radars EM emitters so RWRs hear them (R4).
- `MissionBridge::Step` (`mission_bridge.hpp:84`): replace the 15 km launch-seen rule with what flights saw.
- Our intercept results: `simcore/command/` (an intercept post as a place in `chain_pannonia.json`, facts routed by
  `InfoNet`), never a HUD.

## 11. Constraints

- simcore: header-only C++17, standard library only, no exceptions/RTTI, no Unreal/Windows macro names (`check`,
  `near`, `far`, `min`, `max`...), SI units with suffixes, ENU (`LocalFrame::AltitudeOf` for altitude), warnings as
  errors; every new model gets a test; keep `prototype/` in sync where both implement it (optics are mirrored).
- Data: every value sourced with confidence; EM files `per_value` or labelled `placeholder`; public sources,
  operator-country first; threat libraries are a side's beliefs; tests use generic placeholders.
- No sensor, tracker, doctrine or UI reads `debug_truth_*` or `Platform` directly; knowledge only through
  instruments and voice; first person for our side (no omniscient panel; intercepts through the pipeline); the
  written record is the war's memory.
- The fx feed is presentation only; the simulation never reads it.
- Determinism: draws keyed by `DrawHash` streams (`em/random.hpp`), independent of thread count and order.
- Calibrated totals: the Osa resolver, `WavePassKillChance` and `PassKillChance` keep their kills (roadmap R1; 6.7);
  new evasion shifts them, so `air_war_bench`, `crew_balance`, `em_demo`'s checks and `helper_coverage` must be rerun.
- Cost: every RWR evaluates every emitter each 0.2 s and every sight check walks a terrain profile (R6's timings);
  `em_bench` measures the EM layer.
- The player cannot significantly change the wider war; one enemy at a time.

## 12. Cold War (1985) consistency: what the code says, and what new sensing must not assume

The wars are dated (`theatre_pannonia_nation.json`, 15 October 1985) and types are gated by `in_service_year` and
`arm_em_by_year` (`air_war/data.hpp:345`, `357`; the war's year `:176`). New sensing should follow the same rule. Points to research and source
before data is entered (general knowledge here, not data):

- **Warning receivers were coarse.** 1985 RWRs named threats by band and simple parameters, with poor bearings; the
  ALR-46 class entry (10 deg sigma, band windows) fits that. The Pact's should be the period sets by type and year
  (the SPO-10 family on older types, the SPO-15 on newer ones), not `generic_rwr`.
- **Eyes were the main sensor against SAMs and AAA.** Period accounts of SAM fights stress the smoke trail and the
  launch flash seen by a wingman and called on the radio; a model of sight and of the flight's radio is the period's
  mechanism, not a modern sensor-fused picture.
- **No shared datalinked picture in the cockpit.** Fighters were vectored by voice from ground control (and on the
  Pact side by command datalink from the ground), NATO's AWACS controlled by voice; JTIDS/Link 16 was not in fighter
  cockpits. A flight knows what it sees, hears on its RWR and is told on the radio.
- **BVR was semi-active and rule-bound.** AIM-7F/M and R-23/R-24 class missiles need the launcher's radar to hold the
  target to impact (a steady lock the target's RWR hears); no active fire-and-forget AAM (AMRAAM came in 1991);
  identification rules often required a visual ID. IR missiles of the period (AIM-9L/M, R-60, the R-73 just entering
  service with the MiG-29) and IRSTs (MiG-23ML, MiG-29) need year gates.
- **Night and weather limited both sides**: few FLIR pods (Pave Tack), no LANTIRN; `AircraftType::night` and
  `all_weather` already carry this.
- **Radio**: NATO tactical air on UHF AM, the Pact on VHF/UHF air sets; secure voice and frequency hopping existed
  only in part. Their bands are outside today's intercept receiver (20-100 MHz).
- Existing data worth rechecking for period fit: the A-50 orbiting for a 1985 front (`11_SUPPORT.md`); calibrations
  drawn from Desert Storm (tanker receivers per sortie, `11_SUPPORT.md`; IR and AAA loss shares, `planner.hpp:129`)
  are later than the period and should be labelled so or replaced by 1970s-80s figures.
