# 3. What a pilot sees and hears

Written 15 November 2026 (roadmap B3). Code it touches: a new `simcore/include/simcore/air_war/perception.hpp`
(`simcore::airwar`), `em/optical.hpp`, `em/random.hpp`, `em_link.hpp` (`SyncTarget`, `WarningOf`), `scenario_runner.hpp`
(`Step`, `CueTarget`, `UpdateLauncher`, `UpdateNeighbourFire`, `UpdateBatteryFire`, `FireNeighbourDefences`, `OnDamage`),
`air_war/mission_bridge.hpp`, `air_war/data.hpp` (`AircraftType`); data: a new `data/gameplay/perception.json`, a new
`data/air_war/sensors.json`, a new `data/em/pact_1985.json`, `data/em/nato_1985.json`, `data/air_war/faction_*.json`.
Research: `docs/research/air_battle_2026-11/perception.md` (main), `reactions.md`, `opportunity.md`, `aam_weapons.md`,
`air_combat.md`; code maps `map/sensing.md` (main) and `map/flight.md`.

## 1. What a pilot knows, and from where

An enemy pilot knows four things and nothing else: what he sees, what his warning receiver tells him, what his own
radar, infrared set or pod shows him if his type carried one in 1985, and what he is told on the radio
(`04_REACTIONS.md`, `05_C2_AND_RETASKING.md`, `09_COMMS_AND_INTERCEPT.md`). Never our true position (decision 3).

The period evidence is one-sided. By 1985 every faction's attack pilots found ground targets with their eyes, the crews of
NATO's best two-seaters included (`jones_atg_1974`, `agard_cp100_1972`, `cia_1969_soviet_tac_training`,
`markovsky_afghan_sky`). Most threats were first seen, not received: in 16 MiG attacks on F-105 flights in 1966, 13 were
first detected by eye with no warning (`red_baron_vol2`, events II-30 to II-47), and in 1972 USAF crews judged the eye
"more reliable than electronics" (`nalty_1977`). A camouflaged air defence site was "seldom detected unless [it was]
firing" (`agard_cp100_1972`, Hilgendorf B9); an F-105F flight found an SA-2 site by "the cloud of dust caused by the
launch" (`checo_1967_tactics`). So in SAM3D **the Osa's own launch, its dust and its smoke trail are the main way a strike
pilot finds the battery**; the warning receiver and the enemy's ELINT are the other ways.

| Faction | Eye | Warning receiver (1985) | Airborne sensors that find things | Who is better, and the evidence |
|---|---|---|---|---|
| NATO | trained lookout; about 220 h a year (TAC 18.3 h a month, `gao_1986_flying_hours`) | on every combat type: type, rough bearing, launch light on a known guidance signal (§3) | fighter radars; TISEO on the F-4E, Eagle Eye on the F-15 (`anderegg_2001`); pods with dates open (§4) | the best receivers, still faulty (`gao_masad_82_23`); the most hours |
| Warsaw Pact | trained on preplanned, unopposed, reconnoitred targets; 125-160 h a year in the US view of 1969 (`cia_1969_soviet_tac_training`, dated) | SPO-10 class: direction and the radar's mode, no type (§3) | Sapfir-23 radars and TP-23 infrared sets on the MiG-23 (`airwar_ru`); attack sights open (§4) | pathfinder pairs of the most experienced pilots found hard targets for the rest (`markovsky_afghan_sky`) |
| Neutral | its light jets' pilots, eyes only | none (Argentina 1982 as the analogue: `twz_2025_argentine_chaff`, `werrell_2005` p. 161) | none | sees a launch late or not at all (`reactions.md` reactions-48) |
| Rebels | few hours, eyes only | none | none | the worst |
| Our side | our fighters' pilots use the same model (`08_OUR_FIGHTERS.md`); what they see reaches our side only by their radio through the pipeline | SPO-10 on the MiG-23ML (`airwar_ru`) | Sapfir-23ML, TP-23M | as the Pact's MiG-23ML, on worse ground control when jammed (`grant_2002`) |

The hours are a skill input for `02_FLIGHTS_AND_PILOTS.md`; this doc uses the skill it produces.

## 2. The eye

### 2.1 The law of seeing

Two period US Navy models carry the eye, on top of the TV sight's optics already in `em/optical.hpp`. Both are studies of
US crews; using them for Soviet, Pact or rebel pilots is a SAM3D assumption (`perception.md` §1).

**Threshold (NWC TP 5709, October 1974).** The foveal maximum range of an object of area `A` and inherent contrast `C0`
(in per cent) with no haze is R_M = 0.1655·√((C0 − 1.565)·A) nmi with A in ft², or **R_M ≈ 1006·√((C0 − 1.565)·A) m
with A in m²** (`nwc_tp5709_1974`; 55 ft² at 30 % gives about 6.5 nmi). Inverted, the threshold contrast at range R is

    C_th(R) = F · (1.565 + R² / (1006² · A))      [per cent, R in m, A in m²]

and an object is within the eye's reach while its apparent contrast stays above it:

    C0 · exp(−3.912 R / V) · T_cloud ≥ C_th(R)

with Koschmieder's law and `V` the visibility (`em::ApparentContrast`), `T_cloud` from `Weather::CloudTransmission`.

- **Field factors `F`** (Taylor, in `nwc_tp5709_1974`): 1.64 for confidence, 1.31 for an unknown location, 1.19 for
  vigilance, 2.00 for an untrained observer (5.12 together), and about 1.2 more for ordinary seeing. In SAM3D: the 1.31
  falls away when the pilot is cued to the object's place (a wingman's clock call, an RWR bearing, a briefed target);
  the 2.00 is moved by the pilot's skill from `02_FLIGHTS_AND_PILOTS.md` (trained 1.0, untrained 2.0, a straight line
  between); fatigue and stress raise the vigilance factor (a game value, at most back to 1.19 × 1.5).
- **Empty-field myopia** halves the range (`nwc_tp5709_1974`): applied to aircraft and missiles seen against empty
  sky, not to a search of the ground or to an object near a cloud edge or a contrail.

**Glimpses (the same report).** One glimpse lasts about 1.65 s. A glimpse finds an object with probability g = Ω_lobe /
Ω_search, the lobe's solid angle over the solid angle being searched (±90° in azimuth and ±30° in elevation in the
report's example, π sr), and over glimpses P = 1 − Π(1 − g_i). What drives the curve is "pilot's expectation", knowing
where to look. In SAM3D a cue shrinks Ω_search to the cued sector (±15° about a called clock bearing, a game value), so
a wingman's call or an RWR bearing makes a sighting likely within a few glimpses. The lobe's size against the margin
over threshold is the report's figure, not read into data yet (§8); until it is, Ω_lobe is a game value that falls to
zero at the threshold range.

**Recognition.** Seeing a thing is not knowing what it is. Johnson's criteria (detection two lines, recognition eight,
as `em::LinesAcross` uses them for the TV) applied to the eye's resolution of about one minute of arc (Snellen 20/20, a
textbook value to register in `docs/SOURCES.md`). For a fighter of about 3 m critical size this gives recognition near
1.3 km. The period's figure is longer: visual range for identification was "about three to four miles" (4.8-6.4 km,
`cbo_1986_amraam`, ch. 2 n. 4), and NATO pilots were bound to visual identification in most cases (`lambeth_1985`).
The two are not reconciled by any source read, so the number of lines for recognition (`eye.recognise_lines`) is a game
value set so a fighter's type is recognised at about 5-6 km in clear air by day, the sourced band and the range
`07_AIR_TO_AIR.md` §5.2 uses for VID; Johnson's eight lines stay the TV's. The trouble remains: F-15 pilots saw a dot
in the HUD box and could not identify it before the Sparrow's minimum range (`anderegg_2001` p. 161). TISEO (4× and 10×,
F-4E) and Eagle Eye (F-15) multiply the recognition range inside their narrow field (`anderegg_2001` pp. 33, 161;
the factors are the telescopes' powers, the use a SAM3D estimate). A recognised object gets its type; a seen one only
its class (aircraft, trail, burst, vehicle, site).

### 2.2 Where he looks

The pilot does not look everywhere. Each A2 aircraft has a **field of regard** about its heading (from the velocity
until R3 gives attitude; with R3 the lowered wing hides the ground on that side in a turn, an estimate) and a **lookout
share** by phase, the fraction of glimpses spent outside:

| Phase | Lookout share | Anchor |
|---|---|---|
| transit at height | 0.9 | game value |
| low-level navigation | 0.5 | "loss of lookout" at high speed and low level (`agard_cp267_1979`, Lee B3) |
| attack run, head in the sight | 0.2 | TV Maverick lock about 9 s (`nwc_tp6005_1978`); "eyes-out-of-the-cockpit" asked of the A-10 pilot (DeHart B4) |
| defensive | 0.7, all toward the threat's sector | game value |

- **Two-seaters** add the back-seater's glimpses, mostly to the rear arcs, less while his head is in the radar or pod.
  FB-111 crews held low level to be "at minimum a two-man job"; the F-111's view was poor right-to-left and downward
  (`agard_cp267_1979`, DeHart B4). In a simulation, pilots found 61 % of targets and observers with lighter loads 73 %
  (`jones_atg_1974`, Schohan 1965): the second seat's share of glimpses is set so a two-seat crew finds about that
  much more. Blind arcs per cockpit (below the nose, behind the tail) are estimates in `data/air_war/sensors.json`.
- **The sun.** An attacker within about 15° of the sun is seen late (a game value; "out of the sun: late" is the design
  estimate of `reactions.md` §2.6). The sun's place is `LightAt` (`light.hpp`).
- **Check six.** A single seat spends a share of its lookout glimpses behind the beam (game value); behind the beam a
  launch is seen mostly when a wingman calls it (`reactions.md` §2.6).

### 2.3 Aircraft

The eye law with an aircraft's presented area by aspect (an `AircraftType` estimate, `visual_area_m2` nose, beam and plan,
from the type's published size) and an inherent contrast against the sky of 30 % (`nwc_tp5709_1974`'s example; the TV's
`inherent_contrast` 0.3 is the same estimate). Against the ground the contrast is lower (a game value). Contrails form
by `fx::Contrail` (Schmidt-Appleman, a pure function of the weather, never the fx feed) and are a large object seen far:
the cap is about 20 nm, "detections of contrails at 20 miles and over" (`wseg_116_1967` vol. 1). The calibration
target: unwarned first sightings of fighters typically 1-5 nm, single events 1-12 miles (`wseg_116_1967`; 600 yd to
4-5 mi in the F-105 events, `red_baron_vol2`); haze and cloud often caused misses (`wseg_116_1967`). At night an
aircraft is not seen by eye unless it shows a flame (afterburner) or a fire from damage (estimates).

### 2.4 Missiles, launches and smoke

A launch makes two objects, both from the simulation's own record of it (§5.2):

- **The launch sign at the launcher**: the flash, dust and smoke on the ground. It is the battery's tell. A camouflaged
  Osa battery is a ground target of small pattern and low contrastiness until it fires; while the sign lasts it is large
  and high (`perception.md` §4 item 2: NWC TP 6005 scores a site's pattern high, CHECO and AGARD say camouflaged sites were
  seldom seen unless firing, so an Osa without pads starts low). The sign is searched for by the ground law (§2.6), so it
  gives the launch area with the eye's errors. How long dust and smoke hang is a game value (no source: §8).
- **The trail**: the segment from the launch point to the missile's place while the motor burns, then hanging for the
  munition's `smoke_s`. For the 9M33M2: a 2 s boost and 13 s sustain (`data/scenery/munitions.json`; the motor burns
  13.2-19.2 s in `voenizdat_osa_1991`), dense smoke and a 40 s hang (estimates), and **tracers in the tail for the TV
  sight** (`voenizdat_osa_1991`). The trail is an air object under the eye law, with its area (length seen × width) and a
  smoke contrast that is a game value, set so a small SAM's trail is seen at 3-6 km in day VMC: the design estimate of
  `reactions.md` §2.6 (no source gives a number; §8). By night the smoke is not seen but the tracers and the motor's flame
  are: an estimate, open.

Other launches use their munition's data the same way: the SA-2 class booster's dust (`checo_1967_tactics` p. 31) and
"flying telephone pole" (`werrell_2005` p. 120); a MANPADS flash or trail at 1-3 km, often too late (`reactions.md` §2.6,
design estimate, after `twz_2025_argentine_chaff` and `markovsky_afghan_sky`). Height above cloud matters: Wild Weasels kept
8,000 ft above a cloud deck "to react to SAM launchings" because they "had to see missiles" (`nalty_1977`, Linebacker
experience, 1972). In SAM3D a pilot above a deck sees a missile only once it has climbed out of the cloud
(`Weather::InCloud`, `CloudTransmission`).

### 2.5 Flak bursts and tracer

Bursts and tracer are objects from the AAA's own rounds (`aaa.hpp`, the runner's `nb_rounds_` shells), never from the
feed. By day a burst is a smoke puff whose size and life come from the same scaling the fx draws (`fx/explosions.hpp`,
called as a pure function of the charge); by night the burst's flash and the tracer streams are seen far and the puff
not at all. No period number for either was found (`perception.md` §7, `reactions.md` §7): the ranges are game values, and the
eye law with a flash contrast gives their shape. Seeing flak tells a pilot where the guns are (a bearing and a rough
range) and raises his stress (`02_FLIGHTS_AND_PILOTS.md`).

### 2.6 Ground targets from low level

**The launch-opportunity model (NWC TP 6005, January 1978).** P_max by the target's contrastiness and pattern:

| Pattern | High | Medium | Low |
|---|---|---|---|
| Large | 1.00 | 0.75 | 0.50 |
| Medium | 0.75 | 0.56 | 0.37 |
| Small | 0.50 | 0.37 | 0.25 |

With R* = min(unmask range, visibility), the cumulative acquisition probability is 0.2·P_max at 0.625 R*, 0.5·P_max at
0.5 R* and 0.8·P_max at 0.375 R* (`nwc_tp6005_1978`); SAM3D interpolates linearly between these points, from zero at
0.75 R* (an interpolation, not the report's). It is valid at 500-2,500 ft and 350-550 kt; about 1 % is lost per 3° off
the target line. Its results with P_max 0.75: flat open terrain about 0.50, sharply rolling terrain at low altitude
0.05, the same at 4,000 ft 0.25 (all `nwc_tp6005_1978`). The unmask range is the range at which `HasLineOfSight` first
opens on the run in, through R6's obstacles.

**How it is drawn.** One keyed uniform `u` per aircraft, object and pass (§5.5). The object is acquired at the first
range where P(R) ≥ u; if u ≥ P_max it is not acquired on this pass. Speed therefore does not change the chance, which
is what the trials found: acquisition probability did not change significantly from 350 to 700 kt and the range
differences were not significant (`jones_atg_1974`, Dyer 1964); no significant difference from 300 to 450 kt
(Valentine 1972, same). Speed acts through what follows: the time left to decide, roll in and release
(`06_OPPORTUNITY.md`).

**The object, not the speed, decides.** Target and background explained 62 % of the variance in JTF-2
(`jones_atg_1974`), so each object class carries its pattern and contrastiness in `data/gameplay/perception.json`
(a game value per class, anchored on Bailey's checklist: a unique uncamouflaged target, a line feature, movement and a
good approach aspect help; a camouflaged pin-point target and poor contrast hurt, `agard_cp100_1972` B10):

| Object | Pattern, contrastiness | Note |
|---|---|---|
| Osa battery deployed, camouflaged, silent | small, low (0.25) | `perception.md` §4 item 2 |
| the same while its launch sign lasts | large, high (1.00) | the same |
| column on the move on a road | large, medium; high with dust | moving vehicles at slightly greater range (`jones_atg_1974`) |
| one or two parked vehicles | small, low | F-4 crews "usually never even saw a target" (`bush_a10_simhq`, unverified) |
| bridge, built-up works | large, high | game values |

**Recognition range.** Reliable recognition of a vehicle-sized target by visible or infrared sensors at no more than
about 3 km in relatively good weather and possibly 1 km in poor (`crawford_1977_rand`; it speaks of sensors, applying it to
the eye is SAM3D's inference). A pilot who acquires a vehicle beyond that range has a vehicle, not a type; he must close
to recognise it. At low level and high speed the Pact's pilot has 3-6 s to identify his target (`fm_100_2_1_1984`
p. 12-8, NATO's view): `06_OPPORTUNITY.md` uses it.

**Briefed against unbriefed.** Targets "expected" where the briefing put them were found with higher probability and
sooner (`jones_atg_1974`); a target of opportunity is a different, harder problem (same). So the briefed target is
searched with the 1.31 location factor off and its R* measured from the briefed point; an unbriefed object has its
P_max scaled by a game value below 1.

**Above about 1,500 ft vision no longer limits search** (`jones_atg_1974`): the higher the search, the longer
the unmask range. This is why the Pact's pathfinders searched at 800-1,000 m and 850-900 km/h for 3-5 min
(`markovsky_afghan_sky`). The model needs no rule for it: the unmask range does it.

**Helicopters** search at a pop-up: 50 % of armoured targets found in 20 s, none after 48 s (`jones_atg_1974`),
the calibration target for a hovering search.

**At night** the eye finds ground targets by what they show: fires, the launch sign's flash, a column's headlights
(Soviet night hunts in Afghanistan found vehicles by their headlights, `rgs_afghan_2002` pp. 219-220), and flares: in
the early 1970s "the majority of night-time aerial activity" was under parachute flares, with no gain from more than two
over about 1.5 × 5 km and 610 m the best search height in simulation (`agard_cp100_1972`, Hilgendorf B9). A flight that
drops flares lights an area for its burn (game value); under them the day law runs with a lower contrast.

### 2.7 Weather, light and line of sight

- **Weather.** Central Europe was below a 5,000 ft ceiling and 5 nmi visibility about 85 % of the time in winter and
  about 50 % in summer (`crawford_1977_rand`). The session's visibility is `WeatherPreset::visibility_km`; cloud between the
  eye and the object is `CloudTransmission`. The campaign's daily draw (`weather.clear_share` 0.5, `weather.poor_share`
  0.15 in `data/gameplay/air_war.json`) gives 50 % not clear in every month: it becomes a share by month, so a winter
  war is mostly flown under weather that forces visual attackers low and short-sighted. A Hawker Hunter pilot needed 5 nm
  visibility and 1,500 ft clear of cloud (`agard_cp267_1979`, Lee; OCR damaged): visual deliveries are already refused
  in poor weather by `CanDeliver` (`docs/air_war/10_WEAPONS.md`).
- **Light.** `LightAt` (`light.hpp`) gives the phase and `daylight`; by night the eye sees only flames, flashes, tracer,
  fires, lights and what flares light; at dawn and dusk the contrast falls by `night_share` (a game value). No moon
  (`docs/LIGHT.md` §6).
- **Line of sight.** `HasLineOfSight` (`radar.hpp`) over terrain with R6's obstacles, from the aircraft's altitude
  (`LocalFrame::AltitudeOf`) to the object. A launch behind a ridge or a wood is not seen until the trail climbs clear of
  it.

## 3. The warning receiver as it was

### 3.1 The sets, by faction and date

| Faction | Set | In service | What it showed | Source |
|---|---|---|---|---|
| NATO (US) | APR-25, APR-26 | APR-25 500 contracted November 1965; APR-26 first combat success 1966 | radar type and direction; launch light on the guidance signal | `litton_rwr_history` (tier 4, partly unverified); `checo_1967_tactics` p. 19 |
| NATO (USN) | ALR-45 | to the Fleet 1970-74 | as above | `litton_rwr_history` |
| NATO (USAF) | ALR-46 | first digital RWR; flown in Southeast Asia from January 1973 | as above; the data's `nato_alr46_class_rwr` | `litton_rwr_history`; `nato_ew_refs` |
| NATO (USAF) | ALR-69 | contracts from 1975; on the F-16 and A-10 by 1982; in "all F-16 aircraft" by 1987 | type, the radar's range, direction of arrival on a TV screen; faults in "azimuth accuracy, threat ambiguities, false alarms, and detection ranges" | `gao_masad_82_23` (body not re-checked); `gao_nsiad_87_167` |
| NATO | ALR-74 | in development 1982, **not fielded by 1985** | excluded | `gao_masad_82_23` |
| Warsaw Pact | SPO-10 | by the 1970s (low confidence); on the MiG-23ML | an intermittent 800 Hz tone to the crew and a light on the S3M indicator giving the direction of attack; the signal's character showed the radar's mode; four antenna units; could start chaff | `spo10_tulin` (tier 4); `airwar_ru` |
| Warsaw Pact | SPO-15L "Beryoza" | date not found; on the MiG-23MLD (unverified) | "more accurate bearings" | `airwar_ru` (not legible on re-reading) |
| Neutral, rebels | none | | | `twz_2025_argentine_chaff`, `werrell_2005` p. 161 |

Common to all: **no warning of an infrared missile** (receivers "did not help if the attack was carried out with IR
missiles", `babich_1988`); **no missile approach warner on a fast jet by 1985** (none in any source read, `reactions.md`
§5); receivers **swamped** in a dense fight ("inundated by radar signals" in 1972, `nalty_1977`); a launch light only
when the guidance signal appears (the Fan Song's no later than 4 s after launch, `nalty_1977`), and none for a radar the
library does not know.

### 3.2 What it shows the pilot in SAM3D

`EmSensorLink::WarningOf` (two flags, `em_link.hpp:771`) becomes `RwrPictureOf(target_id)`: the contacts heard in the
last second, each with what that set can show and nothing more.

| Field | NATO (ALR-46/69 class) | Pact SPO-10 class | Pact SPO-15L class |
|---|---|---|---|
| class | the library's kind: search, track, fire control, missile guidance; "unknown" for no match | search (scanning) or lock (steady); no type | as SPO-10 until a source says more |
| name | the library entry; on an ambiguous match the more dangerous class, flagged ambiguous | none | none |
| bearing | measured, `bearing_sigma_deg` 10 (estimate), wider near threshold (as today) | quantised to the quadrant of four antenna units | measured, sigma an estimate below the SPO-10's |
| range | a class (near, middle, far) from received power against the entry's nominal power, error a game value | none | none |
| launch | on a `missile_guidance` contact: the Osa's command link radiates from the launch (`SetCommandLink`) and NATO's library knows it (`sa8_command_guidance`) | none: the command link is steady along the tracking antenna, so it reads as a lock | none until sourced |
| false alarms | a rate per hour (game value) on `Stream::FalseAlarm` | the same | the same |
| saturation | at most `max_display` contacts (game value); the oldest and weakest drop | the same | the same |

The display behaviour lives in `data/air_war/sensors.json`, separate from the receiver's physics in `data/em/`
(`EsmDef` is unchanged). Each type names its set by year, as ARMs already are: `rwr_by_year` beside `arm_em_by_year`
(`air_war/data.hpp`), checked against `TheatreDef::year`.

**A bearing, never a position.** Every RWR contact becomes a sighting at the aircraft's position plus the measured
bearing and the range class (§5.1). `TargetPilot::Evade` turns on that believed point instead of `def_.unit_position`
(`target_behaviour.hpp:210`, changed in `04_REACTIONS.md`). An ARM shot still needs the contact within 0.35 rad of the
briefed aim (`UpdateArms`), as today.

The NATO library (`blue_rwr_nato_view`) is written from a 1991 US Army manual (`fm100_2_3`): it states what NATO
believed, but a later edition's belief (§8).

### 3.3 The Pact's receivers: replacing `generic_rwr`

Today every Pact type carries `generic_rwr`, a placeholder whose library (`blue_rwr_generic`) names none of the Osa's
emitters, so a Pact pilot never warns of a lock or a launch and, because `HasRwr` is true, never gets the truth cue
either: he never evades the Osa (`map/sensing.md` §3.3). The fix:

- a new `data/em/pact_1985.json` (`"provenance": "per_value"`) with `pact_spo10_class_rwr` and `pact_spo15l_class_rwr`;
  their bands are estimates until a source gives them;
- a library `blue_rwr_pact_view` by band and scan character. The Warsaw Pact built the Osa, the Kub, the S-75, the
  S-125 and the P-19 our side fields, so its belief about them is their real parameters (a SAM3D design decision, not a
  sourced fact); the SPO-10 class still names no type, it only tells search from lock;
- the MiG-23MLD gets the SPO-15L class by `rwr_by_year` (unverified, §8), the MiG-23ML the SPO-10; the MiG-27, Su-24,
  Su-25, Su-17M3 and MiG-25BM get the SPO-10 class as a SAM3D estimate until a source names their sets;
- the loader refuses `generic_rwr` on any faction's type in a dated war (placeholders stay for tests and the EM demo).

The result by faction: a NATO pilot hears the Osa's search, its lock and its launch, with bearing errors and the
ALR-69's faults; a Pact pilot hears that something is scanning him and when it locks him, from which quadrant, and must
see the launch; a neutral or rebel pilot hears nothing and must see everything.

### 3.4 What must radiate

A receiver hears only what radiates in the session's EM world. Today that is our own vehicle alone (`map/sensing.md`
§2). For the receivers to matter (R4's "known limit", now B3's):

- **The battery's other launchers and the regiment's neighbours** (`UpdateBatteryFire`, `UpdateNeighbourFire`,
  `BatteryFire`): each vehicle an EM platform with the Osa's emitters (`osa_ak_soc`, `osa_ak_ssc`, the two command links),
  its scan state driven by its fire model (search on duty, track while engaging, the links from the launch) and its
  emission regime (`CrewRegime`). A new `EmSensorLink::AddGroundEmitters(id, defs, position)` and `SetGroundState`.
- **The testbed's Kub, S-75, S-125 and guns** (`FireNeighbourDefences`): the same, with defs for the 1S91, the SNR-75,
  the SNR-125 and the guns' fire-control radars. None of those defs exist with sources (§8); until they do they are
  placeholders, labelled.
- **The PU-12M**: its P-19 radiates below the receivers' 2-18 GHz band, so no RWR hears it, but the enemy's ELINT does
  (`09_COMMS_AND_INTERCEPT.md`). In a battery link the post and the Osa are two EM worlds (`battery_link.hpp`): the post's
  emitters are registered in the target-owning runner's `EmSensorLink`, so one world holds every emitter the aircraft can
  hear.

## 4. Airborne sensors by type

Each sensor is an entry in `data/air_war/sensors.json` with its kind, its carriers, its `in_service_year` and source; a
type lists its sensors, and a war uses only those in service by its year. **An entry whose year the research could not
source stays out of the data** (`perception.md` verification: the dates "must not become data until a period source is
found"); its slot is designed now so it can be filled.

| Sensor | Carrier | In service | What it does in SAM3D | Source |
|---|---|---|---|---|
| Sapfir-23D radar | MiG-23M | 1974 | air search 55 km, lock 35 km for a typical target | `airwar_ru` (MiG-23M) |
| Sapfir-23ML radar | MiG-23ML | built 1976-81 (USSR), to 1985 for export | 50-55 km by hand, 80-85 km under ground guidance; lock head-on 30-50 km | `airwar_ru` (MiG-23ML) |
| Sapfir-23MLA-2 | MiG-23MLD | 1984 | as the ML until sourced | `airwar_ru` (MiG-23MLD) |
| TP-23 infrared search set | MiG-23M | 1974 | an F-4 from the rear at about 20 km in simple weather | `airwar_ru` (MiG-23M) |
| TP-23M | MiG-23ML, MLD | 1976 on | 35 km from the rear against a clear sky | `airwar_ru` (MiG-23ML) |
| APG-63, APG-66, APQ-120 | F-15, F-16, F-4E | APG-63 1973 (`nato_radar_refs`) | detection ranges not found in a NATO source; the AFIT study's 24 nm is a model input; the Soviet view (about 50 km against clutter, over 80 km looking up: `dubrov_1983`, `lambeth_1984`) calibrates `apg63_class` in `07_AIR_TO_AIR.md` §5.2 | `aam_weapons.md` aam-nato-13 |
| TISEO telescope | F-4E | late 1970s | 4× and 10×, slaved to the radar: recognition range | `anderegg_2001` |
| Pave Spike (F-4), Pave Tack (F-111F), Pave Penny (A-10) | | **open** | pods: narrow field, needs a cue; Pave Tack's infrared works at night | not sourced (`perception.md` §7) |
| AGM-65A/B as a finder | A-10, F-4 | weapon in data | TV lock about 9 s (2 to over 12 s) | `nwc_tp6005_1978` |
| radar ground mapping | F-111, Tornado, F-4E | **open** | large features only | not sourced |
| Kaira-23 (MiG-27K), Klen-PS (Su-25) | | **open** | laser rangefinder and designator sights | not sourced; `dia_1977_fa_support` (unverified) |
| AN/PVS-5, ANVIS | US Army helicopters | **open** | the eye at night to a game-value range | not sourced |
| PNV-57E infrared goggles | Mi-8 | Afghanistan war years | night hunts with a flare to light the target | `markovsky_afghan_sky` |

- **Built (B7, 15 Nov 2026):** `data/air_war/sensors.json` holds the sensors above with a year (17: the three Sapfirs, TP-23 and
  TP-23M, APG-63, APG-66 and RP-21/RP-22 as placeholders, APQ-120, TISEO, the SPO-10, ALR-46 and ALR-69 by type, the MLD's
  flares, the Lazur links) and a `slots` list for those without one (SPO-15L, Eagle Eye, the pods, the AGM-65 as a finder,
  radar ground mapping, Kaira-23, Klen-PS, the night-vision goggles, the AMRAAM's guidance). A radar entry names its EM
  `RadarDef` and carries the published ranges; `AirWarData::sensors.Of(type, kind, year, allied)` is what a war offers. The
  receivers' display data and `rwr_by_year` (section 3.2) are still B3's.
- **Fighter radars** are `em::RadarDef` on air platforms and **IR sets** a new optical sensor: `07_AIR_TO_AIR.md` builds
  them. What this doc adds is where their contacts go: into the same picture as a sighting (§5.1), with
  `InfoSource`-style origin `Radar` or `Irst`, and a fighter's single-target lock is heard by its target's receiver
  through the EM layer with no new code (`map/sensing.md` §8).
- **Pods and sights** need a cue to point them (`perception.md` §1): a pod raises the recognition range and lowers the
  contrast threshold inside its narrow field, only after the eye, the radar or the briefing has put the target there.
  The `designator` field the weapons model already reads (`docs/air_war/10_WEAPONS.md`) is unchanged.
- **None of it is a datalink.** What a pod, a radar or an IR set shows stays in that cockpit; it reaches the wingman or
  the ground only by voice (decision 5). The MiG-23's "Lazur" command link carries the ground controller's commands to
  the fighter, not a picture between aircraft (`airwar_ru`; `07_AIR_TO_AIR.md`).

## 5. The design

### 5.1 The perception component

In `simcore/include/simcore/air_war/perception.hpp`:

```cpp
enum class SightKind { Aircraft, Trail, LaunchSign, Burst, Tracer, GroundObject, Fire, Contrail };
enum class SightBy { Eye, Rwr, Radar, Irst, Pod, Voice };

struct Sighting {
    SightKind kind; SightBy by;
    std::string what;            // the class, or the type once recognised ("aircraft", "MiG-21", "SAM launch")
    double bearing_rad, elevation_rad;
    double range_est_m, range_sigma_m;
    Vec3 believed_enu;           // the pilot's estimate, never the truth
    double confidence;           // 0..1: seen once, watched, recognised, confirmed by a second sense
    double time_s;               // when last seen; age = now - time_s
    std::string seen_by;         // the aircraft
    std::uint64_t debug_truth_key;  // overlays and tests only, as em's debug_truth_* fields
};

struct PerceptionDef { /* eye: blind arcs, seats, lookout by phase; rwr set and display; sensors */ };
struct PerceptionState { double next_tick_s; std::vector<Sighting> picture; /* glimpse products, pass draws */ };

void PerceptionTick(const PerceptionDef&, PerceptionState&, const PerceptionInputs&, double now_s);
```

`PerceptionInputs` is a view the runner builds: the aircraft's state and phase, the cues it holds, the light, the
weather, the terrain, the other aircraft (positions, types, areas), the visible events (§5.2), the ground objects near
its track, and its `RwrPictureOf`. Decisions read only `Sighting` fields; no decision reads `debug_truth_key`.

- **The pilot's picture** is the aircraft's `PerceptionState::picture`: sightings merged by kind and a position gate,
  aged out after a time by kind (a burst in seconds, a site in minutes: game values).
- **The flight's picture** is held by its leader (`02_FLIGHTS_AND_PILOTS.md`). A sighting reaches it only by a call on
  the flight's net (`04_REACTIONS.md` for the threat calls, `09_COMMS_AND_INTERCEPT.md` for the emission): a delay and a
  chance of being lost or garbled, and the call's own error (a clock code is 30° wide). A heard call is a `Voice`
  sighting and also a **cue**: the listener's search sector shrinks to the called clock and the 1.31 factor falls away.
- **Up the chain** (`05_C2_AND_RETASKING.md`): a flight reports what is in its picture, with its errors. After the raid
  `PackageOutcome::fired` (truth object ids today) becomes `sightings`: what was reported, where it was believed to be,
  by whom and how sure. The campaign's `ReportAbout` uses each report's own error instead of the fixed 700 m
  (`campaign.hpp:1060`). Claims go into the enemy's records as claimed; the truth stays in the debrief (decision 7).

### 5.2 Visible events from the simulation's own sources

The fx feed is never read (`fx/events.hpp:6-9`). Instead, beside each place that pushes an fx event, the runner records
a `VisibleEvent { serial, kind, position or segment, start_s, until_s, area_m2, contrast, shows_at_night }` in a short
list it keeps: our launch (`UpdateLauncher`, with the missile's flown path for the trail), the battery's and the
neighbours' launches (`UpdateBatteryFire`, `UpdateNeighbourFire`), the testbed's shots and gun rounds
(`FireNeighbourDefences`, `nb_rounds_`), damage smoke and fire (`OnDamage`), an aircraft going down. A perception tick
looks at every event live since its last tick, so a 1 Hz pilot cannot miss a 15 s trail; a short flash is kept for its
visible life. The serial is given in the order the simulation makes events, which is deterministic.

### 5.3 Schedule and stagger

Perception runs per A2 aircraft at 1 Hz in transit, 2 Hz inside a believed threat zone or with an RWR contact, 5 Hz
while defensive or on an attack run (game values within decision 8's 1-5 Hz). Each aircraft's first tick is offset by
`DrawHash(seed, Stream::Sight, aircraft_key)` within its period, so ticks spread over steps. Glimpses per tick = the tick
period / 1.65 s × the lookout share, with the cumulative non-detection product kept per object between ticks.
`ScenarioRunner::Step` calls it right after `CueFromWarnings()` (`scenario_runner.hpp:259`).

### 5.4 Levels of detail

- **A2**: everything above.
- **A1** (flown but coarse, decision 8): no glimpse model. At 1 Hz the leader alone checks the visible events within the
  light's and weather's visual range (`nav::VisualRangeM`'s form, `nav/session.hpp:92`) with line of sight from his
  altitude, and reads `RwrPictureOf`; a hit is a sighting with doubled errors. Radars still radiate and receivers still
  hear in EM, because what the enemy emits is what our ESM hears (`map/scale.md`).
- **A0**: the resolver's `LaunchSighting` and `PilotReport` (`campaign.hpp:1040-1071`) stay as they are.
- **Hand-over** keeps the picture, the glimpse products and the pass draws.

### 5.5 Deterministic draws

New `em::Stream` values (`em/random.hpp`): `Sight` (the glimpse draws and the tick offset), `SightError` (bearing and
range errors), `GroundAcquire` (the pass's `u`); `FalseAlarm` exists. Every draw is keyed on stable keys: the aircraft
(a hash of its id), the object (an event serial or a hash of a target or object id), and the tick index
(`floor(t / period)`) or the pass number. No draw depends on the order of a vector, the thread count, the camera or the
fx feed, so a save replays exactly (decision 6).

### 5.6 The truth cues removed

| Today | After |
|---|---|
| `CueTarget(id, false)` on our radar lock for a target without an RWR (`scenario_runner.hpp:253-255`) | removed: without a receiver a lock is not felt |
| `CueTarget(id, true)` on our launch for a target without an RWR (`:7206`) | removed: the launch is seen (§2.4) or not |
| `Evade` toward a beam of `def_.unit_position` (`target_behaviour.hpp:210`) | on the believed position of the sighting that cued it (`04_REACTIONS.md`) |
| `MissionBridge` "launch seen" for any aircraft within `scenario.revealed_by_launch_m` 15 km (`mission_bridge.hpp:84-95`) | a launch is seen when some aircraft's picture holds its launch sign or trail; the key leaves `data/gameplay/air_war.json` |
| the instructor's Evade order (`EvadeNow`) | kept: it is the trainer's tool, not the pilot's knowledge |

`test_threats.cpp`'s "break and dive on lock" tests change to cue by an RWR in an `"em"` scenario or by a sighting.

### 5.7 Cost

The budget is about 0.5 ms per 0.05 s step for about 100 aircraft, all of track B (decision 8). Perception's share is
set at 0.15 ms mean, measured, worst tick reported. At a mean 2 Hz, 100 A2 aircraft make about 10 ticks a step. The
levers:

- **Cheap culls first**: range against the object's largest possible contrast range, the field of regard, the light
  (no ground search by eye at night without lights or flares), before any line of sight.
- **Line of sight only for survivors**, cached per pair (0.5 s near, 2 s far, as the battery fire model's 0.5 s sight
  cache, `battery_fire.hpp:352-361`). Both ends above about 1,500 m over the ground: terrain only, no obstacles.
- **The unmask range** only for ground objects within ±30° of the track and inside 0.75 R*.
- **No per-aircraft search at A1**, and the `World::FindAircraft` linear scans (`map/scale.md` §1.4) not used in the
  loop: the inputs view is built once per step.

A new `simcore/examples/perception_bench.cpp` (`perception_bench data 100`) flies 100 A2 aircraft over the Kesmark range
with the testbed's defences firing and prints the mean and worst step time with and without perception. The EM cost of
the neighbours' new emitters is measured with `em_bench` as E8 does.

### 5.8 Tests and the prototype

`simcore/tests/test_perception.cpp` (area `threats`), with generic placeholder objects and numbers. The eye law is
mirrored in `prototype/samproto/em.py` beside the TV optics it extends, with the same tests.

## 6. Numbers for data

Into `data/gameplay/perception.json` unless named; each value with `source` and `confidence`.

| Key | Value | Source | Confidence |
|---|---|---|---|
| `eye.rm_constant_m` | 1006 (R_M = 1006·√((C0 − 1.565)·A)) | `nwc_tp5709_1974` | high (formula), medium (other air forces) |
| `eye.threshold_offset_pct` | 1.565 | `nwc_tp5709_1974` | high |
| `eye.factor_confidence`, `_unknown_location`, `_vigilance`, `_untrained`, `_ordinary` | 1.64, 1.31, 1.19, 2.00, ~1.2 | `nwc_tp5709_1974` (Taylor) | medium |
| `eye.empty_field_range_factor` | 0.5 | `nwc_tp5709_1974` | medium |
| `eye.glimpse_s` | 1.65 | `nwc_tp5709_1974` | medium |
| `eye.search_az_deg`, `eye.search_el_deg` | ±90, ±30 | `nwc_tp5709_1974` (example) | medium |
| `eye.resolution_rad` | 2.9e-4 (one arcminute) | textbook, to register | high |
| `eye.recognise_lines` | set so a fighter is recognised at about 5-6 km by day in clear air | game value on `cbo_1986_amraam` (3-4 miles) | estimate |
| `eye.aircraft_contrast_pct` | 30 | `nwc_tp5709_1974` (example) | estimate |
| `eye.contrail_cap_m` | 37,000 (20 nm) | `wseg_116_1967` vol. 1 | medium |
| `ground.pmax` table | 0.25 to 1.00 (§2.6) | `nwc_tp6005_1978` | medium |
| `ground.range_points` | 0.2/0.5/0.8 × P_max at 0.625/0.5/0.375 × R* | `nwc_tp6005_1978` | medium |
| `ground.off_angle_loss_per_deg` | 0.0033 | `nwc_tp6005_1978` | low |
| `ground.recognise_m` good, poor | 3,000, 1,000 | `crawford_1977_rand` (sensors; eye by inference) | medium |
| `ground.helicopter_popup` | 50 % in 20 s, none after 48 s | `jones_atg_1974` | medium |
| `ground.flares` | ≤ 2 per 1.5 × 5 km; search height 610 m | `agard_cp100_1972` | low |
| `weather.not_clear_share` by month | ~0.85 winter, ~0.50 summer | `crawford_1977_rand` | medium |
| `lookout.*` by phase, `second_seat_share`, `sun_glare_deg`, `cue_sector_deg` | §2.2 | game values | estimate |
| `trail.contrast_pct`, `trail.width_m`, `launch_sign.life_s` | set so a small SAM's trail is seen at 3-6 km in day VMC | `reactions.md` §2.6 design estimate | estimate |
| `burst.*`, `tracer.*` ranges by day and night | | game values (no period number) | estimate |
| `ground.class.*` pattern and contrastiness | §2.6 | game values on Bailey's checklist (`agard_cp100_1972` B10) | estimate |
| `rwr.*` display, false alarm rate, `max_display`, range classes (`data/air_war/sensors.json`) | §3.2 | `gao_masad_82_23`, `spo10_tulin`; rates game values | low to estimate |
| SPO-10 class band, bearing quadrants (`data/em/pact_1985.json`) | | `spo10_tulin` (four units); band estimate | low |
| Sapfir-23D, -23ML, TP-23, TP-23M ranges (`sensors.json`) | §4 | `airwar_ru` | medium |
| TV Maverick lock | ~9 s (2 to >12 s) | `nwc_tp6005_1978` | medium |
| `perception.hz` by state | 1, 2, 5 | decision 8 | game value |

## 7. Done when

1. `test_perception.cpp` reproduces NWC TP 5709's example (55 ft² at 30 %, no haze, factors off: 6.5 nmi within 2 %),
   halves it for empty field, and multiplies the threshold by 5.12 for the untrained, unknown-location case.
2. The ground law gives 0.2, 0.5 and 0.8 of P_max at 0.625, 0.5 and 0.375 R*, never more than P_max, and the same
   acquisition chance at 350 and 550 kt for the same geometry.
3. Over 500 seeded runs, a fast jet at 150 m and 450 kt over flat ground in 10 km visibility acquires a silent,
   camouflaged Osa battery in its path at most at the small-low P_max (0.25); once the battery fires in its field of
   regard it finds the launch area most of the time, with a believed position off by the eye's errors.
4. A small SAM's trail in day VMC with a cue is seen at 3-6 km in the reference haze; unwarned first sightings of a
   fighter fall mostly in 1-5 nm; a contrail is seen at up to about 20 nm and no further.
5. By night no aircraft and no smoke is seen by eye, but the 9M33's tracers and a burst's flash are; a launch behind a
   ridge is unseen until the trail clears it; a pilot above an overcast sees a missile only once it leaves the cloud.
6. A NATO ALR-46-class receiver warns of the Osa's search, lock and launch with bearing errors; a Pact SPO-10 class
   warns of search and lock by quadrant and gives no launch; nothing warns of an infrared shot; a neighbour battery's
   radar now warns a receiver; no Pact type loads `generic_rwr`.
7. The truth cues are gone: no `CueTarget` from our lock or launch, no `revealed_by_launch_m`; a launch with no aircraft
   looking (night, cloud, all behind the beam) leaves `PackageOutcome::sightings` empty; with a wingman looking it holds
   one believed position and the campaign's report carries that error.
8. Same seed, same sightings: after a save and load, at 1 and 8 threads, and with the scenario's targets shuffled.
9. A 1985 war loads no sensor whose `in_service_year` is after 1985 or unsourced.
10. `perception_bench data 100` stays within 0.15 ms mean per step with its worst tick printed; `air_war_bench`,
    `crew_balance`, `helper_coverage` (compared with HEAD's rows), `em_demo`'s checks and `ground_check data 3600` are
    rerun and every change is explained (evasion by perception moves the calibrated kill totals).
11. `prototype/samproto/em.py`'s eye law matches the C++ in its mirrored tests.

## 8. Left open

- **Sensor in-service dates**: Pave Spike, Pave Tack, Pave Penny, the AGM-65A as a finder, radar ground mapping on the
  F-111, Tornado and F-4E, Kaira-23, Klen-PS, the Mi-24's sights, AN/PVS-5 and ANVIS (`perception.md` §7). Their slots exist;
  they enter the data only with a period or official source.
- **The SPO-15's date and fit**: its in-service year, its library and whether it gave a launch indication; its tie to
  the MiG-23MLD is unverified (`aam_weapons.md` aam-wp-10). The SPO-10's band and family name; the sets of the MiG-27,
  Su-24, Su-25, Su-17M3 and MiG-25BM; the AH-1S's receiver.
- **NATO's library** is written from a 1991 manual (`fm100_2_3`); a pre-1985 statement of what NATO's receivers knew of
  the SA-8 is wanted. The ALR-69's faults rest on a GAO body not re-checked; the APR-25/26 numbers on a maker's
  brochure.
- **No launch-detection range by eye** exists as a number in any source read; the 9M33's smoke trail as pilots saw it
  is unsourced (`perception.md` §7, `reactions.md` §7). The trail's contrast and the launch sign's life are estimates.
- **Flak and tracer** by day and night as seen from the cockpit: no period number beyond CHECO's narratives.
- **TP 5709's lobe** (detection against off-axis angle) is to be read from the report's figures into data; until then
  the lobe is a game value.
- **Our emitters for the testbed's Kub, S-75, S-125 and the guns' fire-control radars** need sourced EM defs before
  their warnings mean more than placeholders.
- **Hours** for the RAF and the Luftwaffe in 1985, Soviet frontal aviation in the 1980s, the Pact allies, a small
  neutral and rebels (`perception.md` §7): the skill input stays an estimate where no figure exists.
- **The "80 % never saw the attacker"** share (Red Baron III) and the primary study behind "first ten missions" were not
  found; neither is used.
- **Recognising an aircraft by eye**: Johnson's criteria at one arcminute give about 1.3 km for a fighter, the period's
  identification range 3-4 miles (`cbo_1986_amraam`); the eye is calibrated to the latter (§2.1) until a source gives
  recognition ranges by type and aspect.
- **The same law for our observers**: the observation post's flat 6 km disk (`c2_sources.hpp:765-784`) could take the
  eye law in reverse; HumRRO's ground observers saw jets at a mean 6,200 m and before the pilot found his target in 60 %
  of runs (`humrro_1968_ground_observer`). Not part of B3.
