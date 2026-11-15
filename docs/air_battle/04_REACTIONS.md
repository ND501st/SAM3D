# 4. Reactions under fire

Code: `simcore/target_behaviour.hpp` (`TargetPilot`; `EvadeDef` retired), a new `simcore/reactions.hpp` beside
`damage.hpp` and `aaa.hpp`, `simcore/scenario_runner.hpp` (`CueTarget`, `CueFromWarnings`, `OnDamage`, `UpdateArms`),
`simcore/engagement.hpp` (the manoeuvre factor), `simcore/em_link.hpp` (`WarningOf`, chaff), `simcore/battery_fire.hpp`,
`simcore/air_war/test_waves.hpp`, `raid_scenario.hpp`, `mission_bridge.hpp`, `state.hpp` (`PackageOutcome`),
`simcore/campaign/outcomes.hpp`, `simcore/fx/events.hpp`. Data: a new `data/gameplay/reactions.json`, the aircraft types
in `data/air_war/faction_*.json`, `data/gameplay/damage.json`, `data/em/`. Research: `docs/research/air_battle_2026-11/`
`reactions.md` (main), `perception.md`, `air_combat.md`, `factions_cases.md`, `comms_emcon.md`, and
`docs/research/AAA_DAMAGE_2026-11.md` §5-7; code maps `map/flight.md` (main), `map/sensing.md`, `map/scale.md`.

This doc takes over roadmap **R4, defensive reactions**. It needs R1 (damage states, built in first form,
`docs/DAMAGE.md`) and R3 (flight with attitude, not built). It builds on `02_FLIGHTS_AND_PILOTS.md`'s flight and its
command layer: the
flight as an object with a leader and wingmen, and the demands (heading, height, speed, load factor) a pilot sends his
aircraft instead of rewriting its waypoints.

## 1. What this doc decides

1. A reaction is a chain: **a cue, the moment it is noticed, the call, the flight's decision, the manoeuvre with its
   countermeasures and jettison, then press on, go round or go home, then the report.** Each link is a model of its own
   with its own data, so that skill, stress, faction and equipment each act where the period shows they acted.
2. **Cues come only from what the pilot could perceive:** his eyes, his warning receiver, the radio, his own aircraft.
   The truth cues of today (`CueTarget` from our lock or launch for a pilot without a receiver, the bridge's 15 km
   "launch seen", the break flown off the true position of our vehicle) are removed.
3. **Timing is the main lever, more than the manoeuvre's shape.** The reaction delay by skill and stress, and the
   moment the manoeuvre starts against the missile's flight, decide most of the outcome. The engagement model's
   manoeuvre factor is changed to reward a well-timed break, not any break.
4. **A reaction table in data per faction** (cue × threat → manoeuvre, countermeasure, jettison, press-on and abort
   shares), moved by the pilot's skill and stress and the officers' traits, replaces the one-shot `EvadeDef`.
   Scenario files that use `"evade"` keep working unchanged.
5. **Countermeasures and stores are equipment, gated by year.** Chaff, flares, pods and drop tanks are fields of the
   aircraft type with sources and an `in_service_year`, as aircraft and ARMs already are.
6. **A jettison or an abort is a result for the defence.** It counts in the enemy's freedom-of-action ledger (14.6);
   the enemy's report carries it as his pilots claimed it; our record carries what our crews saw of it.

## 2. What exists today (`map/flight.md` §2-5, `map/sensing.md` §3-4)

- `EvadeDef` (`target_behaviour.hpp:49-59`): one evasion per aircraft per life, `On {Launch, Lock, Order}`,
  `Kind {Break, Dive, Jink}`. A break or dive overwrites the route with one point 30 km along the beam to our vehicle's
  **true** position and never returns: in effect an abort (`target_behaviour.hpp:185-220`). Only the jink resumes.
- Cues: our Osa's lock (`scenario_runner.hpp:254`) and launch (`:7206`) for a target without a receiver; the EM
  receiver's lock and launch warnings (`CueFromWarnings`, `:2799-2816`); the instructor's order. Never: the
  neighbours' launches, flak, smoke, fighters, a wingman going down, own damage.
- The Pact's receiver is a placeholder whose library names none of the Osa's emitters, so a Pact pilot with a
  receiver never evades the Osa at all (`map/sensing.md` §3.3).
- Damage draws jettison and abort (`damage.json`, `OnDamage`, `:2621-2661`) in the testbed's waves only. Nothing is
  jettisoned on a threat; drop tanks do not exist; no pilot dispenses chaff or flares (chaff is scripted by time,
  `own_unit.em.chaff`; flares exist nowhere).
- The engagement model multiplies its hit chance by the target's **maximum** turn rate over the shot
  (`engagement.hpp:1391-1404`), whenever the turn came. The battery's other launchers and the testbed's neighbours
  ignore manoeuvre (`battery_fire.hpp:11-13`, `test_waves.hpp:623-633`).
- Everything is per aircraft. A flight is a name prefix and a sideways offset (`test_waves.hpp:343-350`).

## 3. The cue

### 3.1 What a pilot can notice

A cue is a `ThreatCue` built from the simulation's own state, never from the fx feed (`fx/events.hpp:6-9`) and never
from a position the pilot could not know:

| Source | Cue | Sim fact it comes from | Period evidence |
|---|---|---|---|
| Eye | a SAM launch: dust, flash, the climbing missile | a launch from our Osa (`:7195`), the battery's launchers (`:10836`), the neighbours (`:10691`, `:2363`); the missile's motor time from `munitions.json` (`9m33m2`: 2 s boost, 13 s sustain) | SA-2 launches seen by the booster's dust (`checo_1967_tactics` p. 31); sites "seldom detected unless they are firing" (`agard_cp100_1972`, Hilgendorf B9) |
| Eye | a missile's smoke trail closing | the missile in flight | "if you can see Sam, you can usually escape" (`broughton_1969`, snippet only) |
| Eye | flak bursts or tracers near | `nb_rounds_` shells, the guns' passes (`aaa.hpp`) | the jink rule (`checo_1970_tactics` p. 21) |
| Eye | a fighter | a hostile aircraft in sight | 13 of 16 MiG attacks on F-105 flights first detected by eye with no prior warning (`red_baron_vol2`, events II-30 to II-47) |
| Eye | a wingman hit, smoking or going down | `AircraftDamage`, `AircraftDown` in the runner | Taksan 1 and the flights of `red_baron_vol2` |
| Receiver | a search paint, a lock, a launch light | `EmSensorLink::WarningOf` (`em_link.hpp:771`), widened (3.3) | type of radar, rough range, direction; launch light from the guidance signal (`nalty_1977`; `checo_1967_tactics` p. 19; `gao_masad_82_23`, partly unverified) |
| Radio | a call from a wingman, another flight, an observation aircraft, an E-3A or GCI | a voice message on the flight's net (§5) | launch warnings by radio from observation aircraft (`mt_1973_nvn` pp. 17-18; `mt_1984_babich`); helicopter spotters in 1973 (`werrell_2005` p. 153) |
| Own aircraft | damage | `OnDamage` (`:2621`) | `docs/DAMAGE.md` |

What a cue carries: its source, the threat class (3.2), the perceived bearing and elevation (from the eye, the
receiver's 10° sigma, or the clock code of a call), a rough range, the time it was noticed, and whether it was aimed at
this aircraft (a receiver's lock or launch light is; a launch seen across the valley may not be). The eye's chance of
noticing (light, haze, cloud, line of sight, where he is looking, how busy he is) is the sight model's
(`03_PERCEPTION.md` §2, the lobe and glimpse model, `nwc_tp5709_1974`); this doc takes its output, the `Sighting`s and
the `RwrPictureOf` contacts of `03_PERCEPTION.md` §3.2 and §5.1.

Three period facts set the shape of the receiver cue:

- **The receiver told less than a modern one.** Type, rough power and bearing, a launch light when the guidance signal
  appeared, false alarms (`gao_masad_82_23`, partly unverified; `reactions.md` reactions-07). Swamped in 1972: "the
  human eye was more reliable than electronics" (`nalty_1977`).
- **No receiver warns of an IR shot** (`babich_1988`). No source read places a missile approach warner on a fast jet by
  1985, so none exists in the 1985 war (`reactions.md` §5).
- **Cloud blinds the eye.** Wild Weasels kept 8,000 ft above a cloud deck to see the missiles (Linebacker, 1972,
  `nalty_1977`). A launch under the deck is unseen from above it.

### 3.2 Threat classes

| Class | What | Notes |
|---|---|---|
| `sam_high` | SA-2, SA-3 class: large, radar or command guided, medium and high altitude | the testbed's S-75, S-125 |
| `sam_low` | Osa, Kub, Roland, Rapier class: low-level radar or command guided | our Osa and its neighbours |
| `sam_ir` | Strela-1/-10, MANPADS: IR guided, no receiver warning | the testbed's Strela and MANPADS chances |
| `flak_light` | 23-57 mm | `aaa.hpp` contact guns |
| `flak_heavy` | 85-130 mm | `aaa.hpp` time-fuzed guns |
| `fighter` | a hostile fighter | air combat (`07_AIR_TO_AIR.md`) |

A cue's class is what the pilot believes. A receiver names it from its library; the eye names it by size and smoke; a
call names it as the caller did. A wrong class is possible and is kept (a Pact receiver that cannot name the Osa calls
it unknown; Soviet pilots took RPG airbursts for missile bursts, `markovsky_afghan_sky`).

### 3.3 Prerequisites in the EM layer

- **The neighbours' radars radiate** (R4's known limit, `DEVELOPMENT_ROADMAP.md` R4). The battery's other launchers
  (`battery_fire.hpp`), the regiment's neighbour batteries (`UpdateNeighbourFire`) and the testbed's sites
  (`FireNeighbourDefences`) get EM emitters for their search and tracking radars and command links, made in
  `EmSensorLink` as the own vehicle's are (`em_link.hpp:177-178`), radiating while they search, track and guide
  (`AddGroundEmitters`, `03_PERCEPTION.md` §3.4).
- **`WarningOf` becomes `RwrPictureOf`** (`03_PERCEPTION.md` §3.2) and passes more than lock and launch: a search paint (type, bearing), unknown emitters in band, and
  the bearing of each contact as measured, so the reaction is flown off the receiver's bearing, not the truth.
- **The Pact's receiver** stops being `generic_rwr` (`03_PERCEPTION.md` §3.3: `pact_spo10_class_rwr` on most types,
  `pact_spo15l_class_rwr` on the MiG-23MLD, unverified), each with its library and an `in_service_year`. The SPO-10 gave an 800 Hz tone to the whole crew and a direction of attack,
  and could start the chaff release by itself (`spo10_tulin`, tier 4, low confidence, undated). The SPO-15's
  in-service date was not found (Left open).

## 4. Noticing: the reaction delay

"Hits were mostly scored on aircraft that had taken no evasive action or had only initiated such action"
(`checo_1967_tactics` p. 30). The Soviet writers agree from the other side: a lead of up to 15 km "did not result in
frustrating guidance", and the manoeuvre's effect "depended on the accuracy of the determination of the moment for its
initiation" (`mt_1984_babich`). So the delay from cue to manoeuvre, and its spread, is the first model.

| Skill | Delay | Chance of no reaction to a warning | Anchor |
|---|---|---|---|
| Veteran | 1-3 s | 2% | reactions-14, -35 |
| Trained | 2-5 s | 10% | reactions-14 |
| Green | 4-10 s | 25% | "Neither B1 nor B2 reacted to the warnings" (`red_baron_vol2` II-1); reactions-32, -50 |

All of the table is a design estimate (`reactions.md` §2.6), anchored as named. Skill is not a tier picked by hand but
the pilot as a person (`README.md` decision 9; `02_FLIGHTS_AND_PILOTS.md` §4): annual hours (USAF TAC about 16-18 h a month in 1980-85,
`gao_1986_flying_hours`; Soviet tactical pilots probably 125-160 h a year in the CIA's 1969 view,
`cia_1969_soviet_tac_training`), combat sorties (losses concentrated in the first ten, `afmag_2000_redflag`, low
confidence) and nerve. The tiers are three points on that scale; the delay is interpolated between them.

What moves the delay, each a game value until a source gives it:

- **The cue's source.** A launch light or a lock tone is noticed at once; a sighting needs the eye to find and recognise
  it; a radio call adds the speech and the listener's own delay (§5).
- **Workload.** High-speed, low-level flight "results in loss of lookout" (`agard_cp267_1979`, Lee B3). A pilot in his
  attack run or terrain following low notices later; a two-seat crew less so (FB-111 crews: "at minimum a two-man job",
  DeHart B4).
- **Stress and fatigue.** A level per pilot (0-1), raised by a launch at him, a close burst, a wingman going down, his
  own damage, fighters seen; decaying with time. It lengthens the delay and raises the chance of no reaction for a green
  pilot, and raises the jettison and abort shares for all (§8, §9). Syrian pilots in 1982 "simply leveled off and headed
  straight back to Syria" (`nwc_83_027`); some ejected the moment they knew they had been spotted (`lambeth_1984`,
  "according to one report").
- **The Pact's programme.** A cue that matches a zone of the flight's preplanned counter-SAM programme (§6.2) is flown
  "at once by a preset programme" (`sve_prm`): the delay is the veteran's whatever the skill, but its timing is fixed by
  the plan, not judged against the missile.

## 5. The call

**Plain language, a callsign and a clock code.** The calls attested in the period are of the form "MIGs behind you,
break right" and "BLUE 1 called MIGs at 12 o'clock" (`red_baron_vol2`, events II-30 to II-47), "MIG RED" warnings from
Big Eye (`red_baron_vol2` II-2), code words for border warnings changed daily (`nalty_1977`). The 1997 multi-service
brevity list (FM 90-38) is excluded: no "Magnum", "Spike", "Mud", "Naked", "Defending" or "Notch" for 1985 crews
(`reactions.md` §5). SAM3D's call text is game text built from those forms:

| Call | Form (game text) | Who makes it |
|---|---|---|
| Launch seen | `<callsign>, SAM, your <n> o'clock <high/low>` | the one who saw it; the target's own flight first |
| Break | `<callsign>, break <left/right>` | a wingman who sees the missile closing on another |
| Fighters | `MIGs <n> o'clock <high/low>` | the one who saw them |
| Jettison | `<flight>, jettison tanks` / `jettison stores` | the flight leader |
| Abort | `<flight>, abort, abort` and the way home | the flight leader or the package commander |
| Hit | `<callsign>, I'm hit` | the damaged pilot |

The clock is read from the listener's nose (R3's heading). A call gives the listener a bearing good to about half an
hour of the clock (±15°, the code's own resolution: a game value) and starts his own delay when the call ends.

**Calls are transmissions.** Each is a `Voice` message on the flight's net (`em::Message`, `comms_system.hpp`), so it
radiates as a `Comms` emitter from the caller's position and our intercept can hear and fix it
(`09_COMMS_AND_INTERCEPT.md`). The net is half-duplex: one speaker at a time. **Congestion drowns warnings**: on 29 September 1972 "repetitive SAM
alerts choked the radio channels and prevented the Weasel crew from hearing the warning" (`nalty_1977`); a 4 April 1965
loss is put down to a "communications 'breakdown' between flight elements" (`red_baron_vol2`). So a call queued behind
another arrives late or not at all, and a jammed net (Israel jammed the Syrian fighters' nets in 1982, `grant_2002`)
carries nothing. A pilot who did not hear the call reacts only to what he sees himself.

**Radio discipline by faction.** Pact strike groups' radio is "reduced to a minimum or prohibited entirely" on the
attack (`fm_100_2_1_1984` ch. 12); NATO deep strikes fly comm-out and break silence only for an emergency, an abort or
the codeword (`comms_emcon.md` comms-11); Argentine flights left under radio silence (`fundacion_malvinas_24may`). A
launch or a break call is an emergency in every faction: discipline cuts the chatter around it, not the warning itself (a
SAM3D estimate; no source states a Pact pilot's launch call). Rebels have few radios.

**Calls beyond the flight.** NATO pilots expected the first SAM launch to be called by radio from observation aircraft
outside the kill zone (`mt_1973_nvn` pp. 17-18, the Soviet view). A launch call on the package's strike frequency is
heard by every flight on it, and a call to ground control or a hunter-killer team is how the launch area reaches those
who may attack it: that is `05_C2_AND_RETASKING.md`'s, and starts from the calls made here.

## 6. The manoeuvre

### 6.1 By threat and faction

| Threat | NATO | Warsaw Pact | Neutral | Rebels |
|---|---|---|---|---|
| `sam_high` seen or warned | chaff on the tracking cue if carried; wait while the missile closes; then a hard pull and a turn to the zone's nearest edge; at height, a dive toward the missile, then a pull (`werrell_2005` p. 120; `checo_1967_tactics` p. 31) | the preplanned programme at once: full speed, out by the zone's nearest boundary, a change of height, jamming on (`sve_prm`) | break and zigzag by eye; climb or descend (`infobae_2017_piuma`) | turn away and dive (a SAM3D estimate) |
| `sam_low` (the Osa) | low and fast; chaff and a break on a tracking call; leave the zone low behind terrain (`pook_1985`) | the programme, out low by the nearest boundary (`sve_prm`) | as above, on a sighting or a call only | as above |
| `sam_ir` | flares by reflex at low level over known MANPADS belts; speed and height: above 9,000 ft and over 500 kt was generally safe from the SA-7 (`nalty_1977`) | watch for "Strelas" and break hard toward the sun or dense cloud; a helicopter turns sharply with a salvo of flares (`markovsky_afghan_sky`) | Canberra flares where fitted; break on a seen flash (`twz_2025_argentine_chaff`) | none: slow aircraft and helicopters are easy prey (3.4) |
| `flak_light`, `flak_heavy` | jink, changing heading and height at random every 2/3 × the height in thousands of feet seconds; against barrage fire, speed (`checo_1970_tactics` p. 21) | the counter-flak manoeuvre on the same logic as the counter-SAM one (`sve_prm`) | jink by eye | press on or turn away |
| `fighter` | break into the attacker if seen; jettison tanks, then bombs if the fight is close; run for the deck at full speed (`red_baron_vol2` II-1, II-2) | strike groups keep the plan; escorts engage (`07_AIR_TO_AIR.md`) | jettison tanks and climb to outrun, as A-4s did from Sea Harriers (`fundacion_malvinas_24may`) | hide low, run or land (`air_combat.md` reb-01, low) |

**Against the Osa itself** the nearest period analogues are the 1973, 1982 and Falklands records: low and fast run-ins;
chaff, first improvised in the airbrake, and a break on a tracking call; leaving the zone low behind a hill, where "a
Roland was launched after each retreating aircraft" and none hit (`pook_1985`); turning toward the missile to show it a
"cold side" and steep dives onto SA-6 sites (IAF 1973, `werrell_2005` pp. 152-153). In the Soviet reading of the US
practice, a pilot warned of a launch dived to 300-400 m and turned 30-180° off course (`mt_1973_nvn`); Babich's feint
spiralled from 1,500-3,000 m toward the nearest boundary while the striker pressed on at 500-800 m (`mt_1984_babich`).
Speed and height cut vulnerability: 60-90 m gave over 75% survival in the US "survival corridor", and doubling speed
from 370 to 740 km/h cut it fourfold (`babich_1988`, a Western estimate relayed; the height band also in
`mt_1984_babich`). Avoidance counts too: "in certain
months thirty to fifty percent of the aircraft did not enter SAM battalion zones" (`mt_1973_nvn`).

### 6.2 The Pact's counter-SAM programme

The Soviet Military Encyclopedia's «противоракетный манёвр» is a change of course, speed and height together, its start
and end planned before the flight from the known sites' positions, types and zones, flown at once under sudden fire,
at full speed, out by the zone's nearest boundary, combined with jamming (`sve_prm`, volume believed 6, 1978,
unconfirmed). SAM3D builds it in the planner: for every believed SAM zone the route crosses (`IntelPicture::Threats`,
the enemy's knowledge, not the truth), the route planner writes a `ZoneEscape` (`01_ROUTES.md` §9: the zone, the
heading out to its nearest boundary, the height change, full speed, jamming on, where to rejoin) stored with the
flight's plan. On a cue whose bearing falls in a programmed zone the pilot flies it at once. A
battery that has moved since the enemy last saw it is not in the programme: the pilot then breaks away from the cue's
bearing with no plan, at his own delay. This makes the Pact quick against the picture it holds and slow against
surprise, which is the doctrine's own weakness.

### 6.3 What the manoeuvre is in code

A reaction sends the command layer (`02_FLIGHTS_AND_PILOTS.md` §2.2) a `PilotOverride`: a kind (break, break into,
dive, pull, jink, extend out by a boundary, mask behind terrain, climb, programme), a heading relative to the cue's
perceived bearing, a height above the ground, full or held speed, a load factor, a duration, and what follows:
**resume** the route, **rejoin** the flight, **go round** the zone (`01_ROUTES.md` §9), or **leave** for home. The command layer flies it within R3's limits (g, roll
rate, speed lost in the turn) and `GroundFloor` still holds (R5: nothing under the ground). Before R3 a placeholder turn
rate stands, as today's `turn_rate_deg_s`. A pilot may react more than once; each reaction ends in one of the four
follow-ons, never in a silent beam point.

**Timing in the engagement model.** The manoeuvre factor (`engagement.hpp:1403-1404`) is measured over the last
`manoeuvre_window_s` before the missile's arrival, not as the maximum over the whole shot, so a break flown on a launch
seen at 15 km and finished before the missile closed gains little, and a hard pull in the last seconds gains most
(`checo_1967_tactics` p. 31; `mt_1984_babich`). The battery's other launchers and the testbed's neighbours take the same
factor from the target's state. The window's length is a game value; the calibrated kill totals (roadmap 6.7,
`docs/air_war/09_BALANCE.md` §9) are re-checked and re-tuned (§12).

## 7. Countermeasures by type and year

Countermeasures are equipment: each aircraft type gets a `countermeasures` block in `data/air_war/faction_*.json`
(chaff and flares: the fit, the load, `in_service_year`, `source`, `confidence`), gated by `TheatreDef::year` as
`arm_em_by_year` is.

| Faction | Fit | In the 1985 war | Source |
|---|---|---|---|
| NATO | ECM pods and pod formation | yes; without pods the loss rate was "four time[s]" that with (`nalty_1977`; 0.0037 to 0.0021, `checo_1969_tactics` p. 20) | Vietnam lineage |
| NATO | chaff taped in the speed-brake well, opened on a sighting or a warning | the 1967 practice (type unverified) | `nalty_1977` |
| NATO | ALE-40 dispenser | from about 1978; **its load per type is not sourced**: only tier-5 pages give one, not used. The fit is data, the load a labelled placeholder until a period source is found | `reactions.md` reactions-28 |
| Pact | SPO-10 starting the chaff release by itself | low confidence, undated | `spo10_tulin` (tier 4) |
| Pact | Su-25: four ASO-2V with PPI-26 (LO-56) flares, about a minute's supply, panel at the pilot's side | carried, **rarely used**: when a launch was seen it was too late | `markovsky_afghan_sky` |
| Pact | Su-25: extra flare booms, 256 flares, automatic release | **excluded**: undated, likely 1986-87 | `markovsky_afghan_sky` |
| Pact | MiG-23MLD: BVP-50-60 with 60 × 50 mm LO-43 flares | yes, from 1984, in a few units; the ML (1976-81) is the 1985 standard, its flares not sourced | `airwar_ru` (MiG-23MLD); `air_combat.md` §2 |
| Neutral | improvised chaff (140 kg cut on a pasta machine) taped in the airbrakes of the Mirage IIIEA and Dagger | yes, by analogy | `twz_2025_argentine_chaff` |
| Neutral | Canberra: 7 tail flare launchers, 15 s burn; the A-4C's CITEFA launcher gave "no positive results" | Canberra yes, A-4C no | `twz_2025_argentine_chaff` |
| Neutral | no warning receiver, no ECM | yes: "Our planes had no radar warning receiver" | `twz_2025_argentine_chaff`; `werrell_2005` p. 161 |
| Rebels | none | | |

**What they do.** A pilot's chaff is an EM chaff cloud dropped at the aircraft (`EmWorld::AddChaff`, `em/world.hpp:301`),
replacing the scripted `own_unit.em.chaff` for generated raids (hand-written scenarios keep the script). Without the EM
layer the engagement model takes a chaff factor, a game value anchored on the miss distances against Navy aircraft of
1,586 ft with chaff and 1,162 ft without (`nalty_1977`). Flares act only on IR threats: the testbed's Strela and MANPADS
pass chances take a flare factor, a game value, when the flare was out before the seeker locked. The Osa's 9M33 is
command guided: flares do nothing to it. Each dispenser has a count that runs out.

**Misuse by skill.** Dagger pilots wasted their airbrake chaff by opening the brakes to keep formation
(`twz_2025_argentine_chaff`); Su-25 pilots saw the launch too late to dispense (`markovsky_afghan_sky`); a Canberra hit by an
AIM-9L had not launched its flares (`twz_2025_argentine_chaff`). The table gives each skill tier a chance to dispense
too early, too late or not at all (game values).

## 8. Jettison

Two things fall: **drop tanks** and **stores**. Tanks went more often than bombs. Each aircraft type's loads gain
`drop_tanks` (count and mass, sourced where possible, a placeholder otherwise); the stores are the load it carries
today.

Counts per attacked flight in the period:

- In 16 MiG attacks on F-105 flights (July-September 1966): 8 flights jettisoned (all stores 3, tanks only 2, mixed 2,
  ordnance only 1); 6 pressed on; 3 aborted. In the 21 events before them, about 8 jettisoned, often tanks only, about 6
  pressed on, about 6 disengaged or aborted, often for fuel (`red_baron_vol2`).
- One MiG attempt on 21 January 1967 "caused 21 U.S. aircraft to jettison ordnance" (`checo_1969_tactics` p. 4); 47
  aircraft jettisoned after MiG attacks in September 1967, and 16 F-105s jettisoned all bombs on 2 October 1967
  (`checo_1969_tactics` pp. 14-15).
- SAMs forced jettisons "many times" (`checo_1967_tactics`) and "caused aircrews to jettison ordnance"
  (`checo_1969_tactics` p. 3); no share of sorties is given.
- "MIGs would usually depart anytime you Jettisoned ordnance or tanks" (BLUE 1, `red_baron_vol2` II-28).

Shares per attacked flight, design estimates refining `AAA_DAMAGE_2026-11.md` §7 (`reactions.md` §2.6):

| Situation | Jettison anything | Of those, tanks only | Press on | Abort | Anchor |
|---|---|---|---|---|---|
| Fighters attack the flight, NATO trained | 50% | 40% | 35% | 20% | `red_baron_vol2` |
| Fighters attack, neutral | 60% (tanks first) | 60% | 25% | 30% | `fundacion_malvinas_24may` |
| SAM launch seen against the flight, NATO | 40% | n/a | 60% | 10% | `AAA_DAMAGE` §7, reactions-14 |
| SAM launch seen, Pact | 20% | n/a | 70% (programme) | 10% | `sve_prm` |
| Flak alone, no damage | 5% | n/a | game value | game value | `AAA_DAMAGE` §7 |
| Own damage, moderate / heavy | `damage.json` `moderate_jettison` / `heavy_jettison` | n/a | | `moderate_abort` / 1.0 | `docs/DAMAGE.md` |
| Our fighters under jamming, green | n/a | n/a | n/a | 30% go home | `nwc_83_027` |

The rest of a flight that neither presses on nor aborts **goes round**: it avoids the zone it can bypass, by
`01_ROUTES.md` §9's deviation rules (`mt_1973_nvn`'s 30-50% avoidance).

**The 21-aircraft case is the test.** One attack, called on a frequency several flights share, makes several flights
jettison: a call is a cue to everyone who hears it (§5), and each flight draws its own decision on its own table row
("fighters called, not seen"). So one fighter's pass through a package can cost the enemy many loads, which is what
the record shows.

**The mechanics.** Stores jettisoned fall inert: `TestWaves::Jettison` (`test_waves.hpp:166-190`) for the waves, and
a new `MissionBridge::Jettison` for played raids and live packages, so jettisoned weapons land where they fall and hit
nothing they were meant for. Each falls in the fx feed as a `WeaponRelease` with delivery `"jettison"` (as today), drop
tanks with their own munition shape in `munitions.json` (shapes to source); chaff and flares get two new presentation
kinds, `ChaffDrop` and `FlareDrop`. The feed only shows; nothing reads it. Tanks dropped before the release point leave
the flight on internal fuel: the fuel state of `02_FLIGHTS_AND_PILOTS.md` §3.2 decides whether it still reaches the target, and a flight that cannot
turns for home at its new bingo (the term was in use in 1967, `checo_1967_tactics`).

## 9. Press on, go round or go home

Who decides follows the faction (decisions 4 and 5 of `README.md`; `factions_cases.md`):

| Faction | A pilot alone | The flight leader | Above the flight |
|---|---|---|---|
| NATO | his own break and dispensing; he follows the leader's jettison and abort calls | jettison, press on, abort for the flight (initiative 0.7) | the package commander's abort; a FAC or controller's change |
| Warsaw Pact | the programme; his own break | little: the plan holds (initiative 0.25) | ground control may change the plan; abort on lost control is a fighter's rule, not a strike group's |
| Neutral | as NATO, without warning receivers | the leader (0.6) | none in the air |
| Rebels | everything (0.8) | rarely a flight at all | none |

Baselines per faction from `factions_cases.md` §1, all SAM3D estimates bounded by the sources there; the data keys
and values are `10_FACTIONS.md` §7.2's:

| Parameter | Rebels | Neutral | Pact | NATO |
|---|---|---|---|---|
| `press_on` (under fire) | 0.6, highly variable | 0.4 | 0.6 strike, 0.4 fighters without GCI | 0.6 |
| `turn_back` (when spotted or jammed) | 0.4 | 0.5 | strike 0.2, fighters 0.6 if GCI lost | 0.2 |
| `plan_adherence` | 0.3 | 0.6 | 0.9 | 0.7 |
| `jettison_readiness` | 0.5 | 0.6 (tanks first) | 0.2 | 0.4 (fighters attacking: 0.5) |

**Two splits from the sources.** Pact fighters under GCI hold or return when control is lost, while strike groups go on
to their planned target (Skinner in `lambeth_1984` against `fm_100_2_1_1984`, resolved in `factions_cases.md`
§4.2). The neutral pressed on at extreme range: Argentine pilots "could not afford to maneuver" (`usn_falklands_1983`).

**The flight decides together.** A wingman breaks for himself, but the flight's jettison, press-on and abort are the
leader's call through the command layer of `02_FLIGHTS_AND_PILOTS.md` (§1.4): drawn once per flight per situation on
the leader's row, then said on the net (§5), and carried out by each wingman who hears it. A wingman who did not hear
it decides on his own row. The package commander's nerve, which today exists only in the abstract resolver
(`resolve.hpp:117-122`), becomes live: once
the package's losses reach his share, he calls the main flights home.

**Traits and people.** An officer's traits move the faction's values by at most about ±0.2, a pilot's experience,
currency and nerve by about ±0.15 (`factions_cases.md` §1): boldness raises press-on, composure lowers turn-back,
rigidity keeps the programme, airmanship sharpens the timing. A Pact leader with high boldness is still a Pact leader:
he presses on, he does not invent a new plan in the air (Johnson found no example of a target change in the air,
`johnson_1986`, unverified).

## 10. Damaged aircraft

R1's states reach played raids and live packages, not only the testbed:

- `OnDamage` runs for every target, not under `def_.waves` alone; the bridge's neighbours draw damage from their
  passes (`damage.hpp` `ApplyMissile`) as the testbed's do; the Osa's own hits split into destroyed and heavy
  (`docs/DAMAGE.md` §5, Left).
- Own damage is a cue with its own table row: the damage table's jettison and abort draws stay in `damage.json` and are
  read as that row.
- A damaged aircraft flies on R3's limits cut by its state (g and speed, game values) and smokes where `damage.json`
  says.
- A damaged pilot calls it; his flight's leader may send a wingman home with him (no source read on escorting a cripple:
  a game value, Left open).
- `PackageOutcome::damaged` (`air_war/state.hpp:139`) counts them, and the enemy repairs over days (R1's availability).

## 11. Per faction

**NATO** has the most to work with and the most practice: receivers on most types (the ALR-46 class in the data),
pods, chaff, flares and the habit of mutual support and calls. Its weakness is the receiver's coarseness and a swamped
net. Hits in Vietnam fell on crews who did not react in time (`checo_1967_tactics` p. 30); NATO in 1985 trained against
exactly that (Red Flag from 1975, `vangilder_1979`).

**The Warsaw Pact** is quick and uniform against what it planned for and poor against surprise: the programme from the
briefing, little improvisation (`sve_prm`; `fm_100_2_1_1984` ch. 12). Flares carried but seldom used before 1985
(`markovsky_afghan_sky`). Its receiver's library decides whether its pilots ever hear our Osa (Left open).

**The neutral** (Argentina 1982 as analogue) had no receiver and no ECM (`werrell_2005` p. 161): it reacts only to a
sighting or a call, late, by breaking and zigzagging, with improvised chaff its pilots sometimes wasted. It pressed on
with determination and bravery (`usn_falklands_1983`) and ran from fighters by jettisoning its tanks
(`fundacion_malvinas_24may`). It is the faction most at the mercy of the Osa.

**The rebels** have eyes only. Their aircraft are slow and their helicopters slower: in 1972 the SA-7 needed 1.8
missiles per helicopter kill and 10 per slow fixed-wing kill, against 135 per F-4 (`werrell_2005` p. 128). With no
receiver the flash or the trail is the only cue, often too late (`markovsky_afghan_sky`). Their press-on is high and
variable, their initiative total.

**Our side's fighters** (`08_OUR_FIGHTERS.md`) react through the same table on our profile: the VPAF's hit-and-run
single pass under radio and radar silence (`checo_1969_tactics` p. 39), warnings by GCI, and the Syrian failure under
jamming (popped up, zoomed into the CAP, levelled off and flew home: `nwc_83_027`).

## 12. The design in code

### 12.1 Data: `data/gameplay/reactions.json`

Game values and design estimates, each row with its anchor, as `damage.json` is. The faction's `jettison_readiness`
and `press_on` (`10_FACTIONS.md` §7) summarise these rows, and the officers' and pilots' shifts (`10_FACTIONS.md` §8)
move them:

    {
      "notes": "Reactions under fire: docs/air_battle/04_REACTIONS.md. Design estimates; anchors per row.",
      "delay_s":     { "veteran": [1, 3], "trained": [2, 5], "green": [4, 10] },
      "no_reaction": { "veteran": 0.02,  "trained": 0.10,  "green": 0.25 },
      "manoeuvre_window_s": "<game value>",
      "profiles": {
        "nato": {
          "rows": [
            { "cue": "seen", "threat": "sam_low", "manoeuvre": "break_late", "then": "go_round",
              "countermeasure": "chaff", "jettison": 0.4, "tanks_only": 0.0, "press_on": 0.6, "abort": 0.1,
              "anchor": "AAA_DAMAGE 7; reactions-14, -24" }
          ]
        },
        "warsaw_pact": { "programme": true, "rows": [ ... ] },
        "neutral": { "rows": [ ... ] }, "rebels": { "rows": [ ... ] }, "our_side": { "rows": [ ... ] }
      }
    }

Lookup: the profile's row for (cue source, threat class), else its row for the threat with any source, else the
profile's default. `data/air_war/faction_*.json` names its profile (`"reactions": "nato"`); a hand-written scenario names
one per target (§12.3). The faction files' aircraft types gain `countermeasures` and `drop_tanks` (§7, §8).

### 12.2 simcore

- `simcore/reactions.hpp` (namespace `simcore`): `ThreatClass`, `CueSource`, `ThreatCue`, `ReactionRow`,
  `ReactionProfile`, `ReactionTable` (loaded and checked like `DamageTable`), `PilotState` (the session's copy of the
  pilot of `02_FLIGHTS_AND_PILOTS.md` §4: skill, nerve, stress, fatigue; plus chaff and flares left, tanks and stores
  aboard, the `ZoneEscape`s), and pure functions: `NoticeDelayS`,
  `ChooseReaction`, `FlightDecision`, `StressAfter`. No state of the runner inside; every draw keyed.
- `TargetPilot` (`target_behaviour.hpp`) holds a `PilotState` and a queue of noticed cues; `OnLaunch`, `OnLocked` and
  `EvadeNow` become `Notice(const ThreatCue&)`; `Evade` becomes the `PilotOverride` it sends to the command layer
  (`02_FLIGHTS_AND_PILOTS.md` §2.2).
- `ScenarioRunner`: after `03_PERCEPTION.md`'s `PerceptionTick` (called after `CueFromWarnings()`, `:259`), new
  sightings of launches, missiles in flight, bursts, wingmen going down, and heard calls and own damage become cues per
  aircraft; `CueTarget`'s truth paths (`:253-255`, `:7206`) removed;
  the bridge's `revealed_by_launch_m` replaced by what the flights saw (`mission_bridge.hpp:84-95`).
- **Determinism.** Draws are keyed, not sequential: `f(seed, kReactionStream, aircraft key, cue serial, purpose)` as
  the EM layer's `DrawHash`, so the order the runner visits aircraft in changes nothing. Nothing reads the fx feed.
  Everything that reaches the campaign (jettisons, aborts, sightings, claims) returns through `PackageOutcome`.
- **Levels of detail** (`README.md` decision 8; `02_FLIGHTS_AND_PILOTS.md` §5). A2: everything here per aircraft. A1: the flight only, at 1 Hz, on
  receiver launch warnings, calls and damage; the leader draws for the flight and the manoeuvre is the flight's. A0:
  the resolver keeps `(1 - evasion)` in `PassKillChance` and gains the table's expected jettison and abort shares, so
  abstract and played raids agree. A hand-over keeps the `PilotState`.
- **Cost.** Cues follow perception's staggered 1-5 Hz ticks (`03_PERCEPTION.md` §5.3); reactions are events. The sight checks are the
  costly part and are the sight model's budget; the rest is a small share of the track's about 5 µs per aircraft per
  step (`02_FLIGHTS_AND_PILOTS.md` §5.2, `map/scale.md` §1.3): at most about 0.05 ms per step for 100 aircraft, a
  SAM3D budget to be measured in `air_battle_bench`.

### 12.3 Retiring `EvadeDef`

- Generated raids and the testbed stop writing `"evade"` (`test_waves.hpp:505-510`, `raid_scenario.hpp:504-516`) and
  write `"reactions": { "profile": ... }` with the type's countermeasures and tanks; skill, nerve and stress come from
  the target's `"pilot"` block (`02_FLIGHTS_AND_PILOTS.md` §4.4).
- A hand-written `"evade"` block still loads (`scenario.hpp:1010-1040`, the same keys and checks) and is turned at load
  into a **scripted profile**: one row for its cue (`launch` or `lock`), no delay, no countermeasure, no jettison, its
  manoeuvre with its turn rate, and the follow-on today's code gives it: **resume** for a jink after `evade_s`,
  **leave** for a break or dive along the beam. The beam is taken from the cue's bearing, which for our own Osa's lock
  and launch is our vehicle's position, so the hand-written scenarios fly as they do today. `evade_on_launch` maps the
  same way. `"evade"` and `"reactions"` on one target is a load error.
- The 15 scenario files with `"evade"` (`data/scenarios/`: the Bekaa, Cuito, the Angola night, the Kesmark range test,
  the air war's sample raids and others) are unchanged; the sample raids move to `"reactions"` when they are next
  regenerated.
- The instructor's Evade order (`instructor.hpp`) is a cue of source `order` on the scripted row, as today.

## 13. What it counts, and who writes it down

**The enemy's freedom of action denied** (roadmap 14.6, `campaign/outcomes.hpp`). `PackageOutcome` gains
`jettisoned_flights` (flights that dropped stores before their release point) and counts of tanks and stores dropped;
`aborted_flights` exists. `PackageLedger` gains `prevented_jettisoned` beside `prevented_downed` and
`prevented_broke_away`, and `Prevented()` sums the three. These are truth ledgers for the debrief and the campaign's
reveal only (`outcomes.hpp:4-6`), never shown in play.

**The enemy's own record.** His pilots report what they believed: a launch seen at the bearing and range they judged,
a site "seen" where they thought it was, a jettison as forced. Reports made in the air go to the campaign as
`IntelSource::InFlightReport` (`05_C2_AND_RETASKING.md` §4.3), those made after landing as `IntelSource::PilotReport`,
and `LaunchSighting` with the errors of what was perceived, not the truth with a fixed error
(`campaign.hpp:1055-1064`). His staff learns from them that the battery makes flights jettison, and plans SEAD for it.

**Our record.** What our crews saw goes into the records as they saw it (the written record is the war's memory,
`docs/command/01_PRINCIPLES.md` 9). A target that turns away is on the scopes and the board; a jettison, a chaff
cloud, smoke or a flare string is seen only within the visual range by the light and weather (`nav::VisualRangeM`) or
in the TV sight (E5), and written in the commander's log and the PU-12M's journal as seen: "the target dropped its
load and turned away", not "jettison, 2 Mk 82". Chaff on the scope is a radar fact the operator may misread as a target.
New crew callouts for these need a place in the crew data's `"menu"` (`test_crew_talk.cpp`). The combat report from
the records (P9, `campaign/report.hpp`) carries the jettisons and turn-aways our crews wrote down, and nothing else.

## 14. Tests

- `test_reactions.cpp` (new): the delay's spread and the no-reaction share by tier over many draws; keyed draws give the
  same reactions whatever the order of the targets; row lookup and fallbacks; the year gate (no 256-flare Su-25 and no
  ALR-74 class fit in a 1985 war; MiG-23MLD flares only from 1984); a Pact pilot flies the programme at once on a
  programmed zone and breaks unplanned on a moved battery; a NATO pilot waits, then pulls; jettison, press-on and abort
  shares within a few per cent of the table; **the 21-aircraft case**: one fighter attack called on a shared net makes
  several flights jettison; a warning queued behind other calls arrives late, a jammed net carries none; **no truth
  cue**: a pilot without a receiver who cannot see the launch (night, cloud, terrain, looking away) does not react.
- `test_threats.cpp` (`:375-624`): today's break, dive and jink tests pass unchanged through the scripted profile.
- `test_engagement_model.cpp`: a pull in the last window cuts the hit chance more than the same turn flown at launch.
- `test_damage.cpp`: damage, jettison and abort in a played raid, not only the testbed.
- `test_outcomes.cpp`: a flight that jettisons short of its release point counts as prevented.
- `test_fx_events.cpp`: with the feed switched off the reactions are the same.
- Balance, as track R reports it: `crew_balance data 10`, `air_war_bench data 400`, `em_demo`'s 11 checks byte-identical
  or every moved cell explained; `helper_coverage` against the base's rows; `ground_check data 3600` PASS.

## Done when

- In the dynamic campaign test an aircraft breaks, dives or jinks on a launch it **saw**, by its receiver on **any**
  of our radars, or on a call, and never on a launch it could not have seen (night, cloud, behind terrain).
- A manoeuvre ends and the aircraft resumes its route, rejoins its flight, goes round the zone or turns for home; none
  flies on to a 30 km beam point and vanishes.
- Flights jettison tanks and stores under fighter attack and SAM fire in shares within a few per cent of §8's table over
  a testbed hour, and the stores and tanks are seen to fall in the game.
- One fighter attack called on a package's frequency makes more than one flight jettison.
- A well-timed late pull lowers the Osa's hit chance more than an early break, and roadmap 6.7's calibrated kill totals stay
  in their band after re-tuning.
- A Pact flight flies its programme at once against a battery it knew of and an unplanned break against one that moved.
- Chaff dropped by a pilot appears in the EM layer and on the Osa's scope; flares change only IR shots.
- The debrief's freedom-of-action ledger counts flights that jettisoned short of their release point; the PU-12M's
  journal and the commander's log record what the crew saw of it, and the combat report carries only that.
- The hand-written scenarios with `"evade"` play as before (`test_threats.cpp` unchanged).
- The same seed gives the same reactions in the game's 0.05 s step on any machine; the fx feed switched off changes
  nothing.

## Left open

- **The ALE-40's load per type** and the ASO-2V's cartridge count per unit: no tier 1-4 source reached
  (`reactions.md` §7). Fits are data, loads placeholders.
- **The SPO-15's in-service date**, and whether a 1985 Pact receiver's library named Soviet-built SAM radars like the
  Osa's at all: our side flies Soviet equipment, so a Pact pilot's warning of our Osa is a question of data no source
  settles.
- **The 9M33's smoke trail as seen from a cockpit**, and a launch-detection range by eye as a number from any period
  source (`reactions.md` §7, `perception.md` §7). Until found, the eye's cue for our launch rests on the sight model's
  estimates.
- **The reaction delays and every share in §4, §8 and §9** are design estimates anchored on counts, not measured
  figures. More counts may come from Red Baron III (F-4 events 1967-72) and Red Baron vol. II past what was read.
- **The period's own tactics manuals** (TACM 3-1, *USAF Fighter Weapons Review* of the 1980s) and the clean copy of
  «Зарубежное военное обозрение» 1982 No 2 (Kirillov, Dmitriev): not reached.
- **The SVE's volume and year** for «противоракетный манёвр» (believed vol. 6, 1978) and the `babich_1988` imprint.
- **Escorting a damaged aircraft home**, and how often a flight lost a wingman to it: no source read.
- **Drop tank shapes and masses per type** for `munitions.json`, to source as the munitions are.
- **The Argentine side's own account of its reactions** (Moro, *La guerra inaudita*, 1985) and the RAF Historical
  Society's Falklands journal: not read.
- **Target fixation** and press-on in the dive: only the CHECO dive and an Afghan dive to 900 m were found
  (`perception.md` §7).
