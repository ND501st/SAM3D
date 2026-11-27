# Map: the enemy's command and its people, and our side's pipeline, during a raid

Read-only map of `C:\dev\SAM3D-wt\air-battle` (branch state at `1d09489`) for the owner's request of 15 Nov 2026
(the hot phase of the enemy's air war: live routes, retasking, HQ loop, intercepts, air-to-air). Paths below are
relative to that worktree. "Today" means what the code does now. Where the request touches Cold War versus modern
practice, section 12 says what the code already gets right for 1985, and which modern ideas a new design must keep out.

## 0. Short answers

| Question | Answer |
|---|---|
| Is any enemy decision taken live during a package? | **Almost none.** Every staff decision is made at planning (`Planner::Plan`, every `planning_cycle_h`: 12 h Pact, 24 h others) or after a package is resolved (`ApplyOutcome` -> `Lessons`, `Daily` -> `Careers`). An abstract package is resolved **whole, at once, at its take-off hour** (`campaign.hpp:664-680` -> `ResolveAbstract` 846-853). Inside that resolution there are three rule-driven in-package choices: the leader's **nerve** turns the main flights back (`resolve.hpp:116-122, 249-255`), an **armed reconnaissance** flight attacks the first column it finds on its stretch (`resolve.hpp:285ff`, `FindColumn`), and close support is **aimed at take-off** at the troops' true place plus 300 m of noise (`AimOnCall`, `campaign.hpp:836-844`). In a played session there are only per-aircraft reflexes (evade on lock or launch, ARMs at a radiating radar, terrain following, damage aborts). No flight, package, squadron or HQ decision exists in the session, and **the campaign is frozen while a package is in it** (`package_injector.hpp:182-203`; `AirCampaign::Advance` returns at once while `pending_ >= 0`, `campaign.hpp:183`). |
| Do pilots exist as people? | **Only the officers who fly.** Each squadron has one leader (`AirStaff::leaders_`, authored in the faction file or generated, with name, rank, age, background, 8 traits, 6 skills, quirks, stress, confidence, reputation, grudges, tactic memory, career). Staff officers fly only with the `flies_himself` quirk (`officers.hpp:278-282`). A package has at most one flying officer, `Package::leader`, put in the first eligible flight of his squadron (`planner.hpp:1342-1349`). Every other pilot is anonymous: a `Flight` is a callsign and a count (`state.hpp:25-48`), and in a session each aircraft is a target named `<callsign>-k` (`raid_scenario.hpp:446-454`). Crew quality is the type's `evasion` plus the leader's `airmanship` (`PackageAirmanship`, `resolve.hpp:59-62`). |
| Where would an airborne or ground HQ loop sit? | Three levels, each with a hook (section 10): (a) **in the session**, a package controller beside `MissionBridge` in `LivePackages` that reads the runner and steers the package's targets; (b) **in the campaign**, a stepped resolution and an alert pool fed by in-flight reports, which today does not exist (packages resolve atomically and reports merge hourly); (c) **the people**: a new enemy C2 post (the CP of the air army or the ATOC, GCI and FAC posts) as data beside the staff. Latency has to be new data, by faction. |
| How could live enemy emissions and intercepts reach the player? | Today intercept only runs **the other way**: our calls are heard by the enemy (`InfoNet::RadioCalls` -> `CampaignFeed::BatteryCalls` -> `AirCampaign::NoteTransmissions` -> `Collect`'s COMINT; `command::RadioListener` is the enemy's listener). Our side hears the enemy only as abstract journal lines ("intercepts suggest a change in the enemy's air staff", `campaign.hpp:1479, 1488`) and the `loose_lips` quirk's earlier warning (`raid_scenario.hpp:852-855`). The first-person path would be: enemy voice and links as EM emitters; our listeners as **posts of the chain** (`army_intel` exists and is the only one: "radio intercepts of the enemy's air forces", `data/command/chain_pannonia.json:26-30`); each intercept a `Fact` at that post, carried by `InfoNet` to the battery, and said in the session on the regiment's net (`ScenarioRunner::RegimentSays`, `scenario_runner.hpp:5956`) or as a delivery. Never a HUD. |
| What do the faction files say about initiative, rigidity, on-call strikes? | No field is called initiative. `doctrine.centralisation` (NATO 0.6, neutral 0.5, rebels 0.2, Pact 0.9) is read only for **timing jitter** (`planner.hpp:1338`). `doctrine.on_call_fraction` (NATO 0.1, neutral 0.15, rebels 0.0, Pact 0.2) **only lowers the planned tempo** (`planner.hpp:297`): the held-back sorties are never flown. Rigidity lives on the officers (`officer_pool.trait_mean.rigidity`: NATO 0.35, neutral 0.5, rebels 0.3, Pact 0.7; Gen-Maj Sorokin 0.85). Pact text: "on-call strikes on what the reconnaissance finds, hunting flights where a battery is suspected ... Punctual, rigid when things go wrong" (`faction_warsaw_pact.json:6`). Hunter and free-hunt (armed reconnaissance) packages exist as **planned** packages (section 7). |

## 1. The enemy's people

### 1.1 `Officer` (`simcore/include/simcore/air_war/officers.hpp:39-80`)

| Field | What | Line |
|---|---|---|
| `def` (`OfficerDef`) | id, name, rank, role, squadron, age, background, bio, `traits`, `skills`, `quirks`, `flies`, `authored` | `data.hpp` (OfficerDef), filled by `Make` 198-206 or `Generate` 210-285 |
| `quirks` | `QuirkDef`s from `data/air_war/traits.json`; `Quirk(key)` sums an effect, `HasQuirk(id)` | 41, 61-71 |
| `active`, `fate`, `appointed_h`, `left_h` | in post, or relieved / promoted / killed / captured | 43-46 |
| `stress`, `confidence`, `reputation` | 0..1 state | 48-50 |
| `grudges` | intel key -> weight; raised by losses in `Lessons` | 51 |
| `tactics` (`TacticMemory`) | per `TacticKey(entry, profile, day part, kind)`: loss rate and claimed success, smoothed; uses; last used | 28-37, 52 |
| `lesson_sead` | extra SEAD per threat learned from losses to radar SAMs | 54 |
| `packages_planned`, `aircraft_lost`, `damage_claimed`, `history` | career counters and notes | 55-58 |
| `Effective(skill)` | skill x (1 - 0.5 stress (1 - composure)) | 73 |
| `LearningRate(base)` | base x (1 - 0.8 rigidity) x (0.6 + 0.4 methodical) | 75-77 |

Traits (8): boldness, methodical, creativity, rigidity, ambition, skepticism, vindictiveness, composure. Skills (6):
planning, intelligence, sead, airmanship, leadership, politics (`traits.json:7-16`, `docs/air_war/02_AIR_STAFF.md` §2).

### 1.2 `AirStaff` (`officers.hpp:83-286`)

- Posts: Commander, Planner, Intelligence, Sead (only where the doctrine allows DEAD or Hunter: `HasSead` 189-192),
  Political (only where the pool names a political rank: the Pact), and one SquadronLeader per squadron (`leaders_`).
  A drone detachment has no flying leader (141).
- `Build` (113-154) appoints the authored officers, generates the rest from `officer_pool` (names, ranks, trait and
  skill means and SDs, quirks by tier weight and role: 210-285).
- `Replace` (175-187): a relieved officer's successor is drawn against his failing (bolder after a timid one, less
  rigid after a rigid one, +0.1 methodical: 243-251).
- `od.flies` is true for squadron leaders and for anyone with a quirk whose `flies` effect is positive (278-282).

### 1.3 Quirks that bear on the hot phase (`data/air_war/traits.json`)

| Quirk | Effect key | Read where | Live today? |
|---|---|---|---|
| `loss_shy` / `press_on` | `abort_threshold` -1 / +1 | the leader's nerve, `resolve.hpp:117-122` | Abstract resolution only; **not in a played session** (the bridge has no nerve) |
| `loose_lips` | `comsec_leak` | the warning time in a raid scenario, `raid_scenario.hpp:852-855` (`own_intel.leak_warning_s` 900 s vs `warning_s` 300 s); revealed by analysts `campaign.hpp:1586` | Planning-time constant |
| `punctual` / `ragged_timing` | `punctuality` | timing jitter and coordination at `Commit`, `planner.hpp:1335-1338` | Planning |
| `flies_himself` | `flies` | `Generate` 278-282; the officer can be shot down (`LossRecord::officer`) | Planning |
| `by_the_book` | `doctrine_weight` | apportionment, `planner.hpp:308` | Planning |
| `chases_kills`, `double_tap` | `restrike`, `double_tap` | target choice; `Planner::DoubleTap` 1381-1404 (a second package 0.6 h later, planned at once) | Planning (the double tap is pre-planned, not a reaction) |
| `grudge_holder` | `grudge` | grudge weight in `Lessons` 1349; target worth in the planner | After the package |
| `elint_believer`, `imagery_man` | `elint_trust`, `imagery_wait`, `recce_first` | target choice | Planning |
| `sead_first`, `hoards_arms` | `sead_ratio`, `recce_first`, `arm_reserve` | package build | Planning |

No quirk or trait governs a decision in flight except the nerve. There is no quirk for "takes targets of
opportunity", "breaks off to help a wingman", "reports launches", "silent on the radio", or "follows GCI to the
letter".

### 1.4 Pilots as people: what exists and what does not

- Exists: the squadron leader per squadron (authored, e.g. Pact `wp_ldr_su24` Oleg Tarasenko, airmanship 0.85,
  `night_owl`, `press_on`; `wp_ldr_fighter` Pavel Gribov, MiG-23, `grudge_holder`; NATO `nato_ldr_weasel` Earl Becker,
  `press_on`, `grudge_holder`, airmanship 0.85). He is `Package::leader` and `Flight::officer` on one flight of a
  package where his squadron flies a strike, DEAD, hunter, helicopter or SEAD flight (`planner.hpp:1342-1349`).
- The officer's `airmanship` (through `Effective`, so stress lowers it) enters the evasion term of every pass
  (`resolve.hpp:194`, `mission_bridge.hpp:581` via `RaidManifest::airmanship`, `raid_scenario.hpp:269`).
- `LossRecord::officer` is set on the first loss of the officer's flight (`resolve.hpp:215, 241`) or from the
  `RaidAircraft::officer` flag in a played raid (`mission_bridge.hpp:291`); `ApplyOutcome` then kills or captures him
  and appoints a successor (`campaign.hpp:913-934`).
- Does not exist: wingmen and element leads; per-pilot names, skill, experience, fatigue, nerve or memory; the
  squadron's pilots as a roster; flight-level discipline. `SquadronState` (`state.hpp:190-209`) counts airframes
  only (total, lost, damaged until), not crews.
- `AircraftType::air_to_air` (`data.hpp:355`, loaded at 1361) is declared and loaded but **read by no model**.
  Escort is a presence flag (`p.Has(FlightRole::Escort) ? 0.3 : 1.0`, `resolve.hpp:235`, `planner.hpp:1030`).

## 2. When the enemy decides: the clock

| Decision | Who | Where | When | Inputs |
|---|---|---|---|---|
| Phase change (and back) | commander | `AirCampaign::Plan` 758-781 | each plan | `BelievedIadsShare`, the day |
| Tempo, apportionment | commander | `Planner::Plan` (planner.hpp 269ff; tempo 296-297) | each plan | traits, stress, pressure, `on_call_fraction`, `doctrine_weight` |
| Targets, packages, routes, profile, entry, weapons | planner (+ SEAD officer for coordination) | `Planner` (`BestPackage`, `Build`, `Commit`) | each plan | `IntelPicture`, tactic memory, quirks, utility noise by skill |
| Higher authority moves a TOT | politics, against the commander | `Planner::Interference` 1406-1434 | each plan, chance `interference_per_cycle` | the commander's politics skill |
| Aim of CAS/helicopter raid on troops | forward air controllers (abstract) | `AirCampaign::AimOnCall` 836-844 | at take-off | **truth** (troops' position + N(0, 300 m)) |
| Leader turns back | flying leader | `Resolver::Resolve` 249-255 | inside the atomic resolution | losses / package size vs nerve |
| Armed recce attacks a column | flight | `Resolver::FindColumn` 285ff | inside the atomic resolution | truth of columns on the move within `logistics.search_m` of its aim, find chance by light and weather |
| Beliefs from the raid | intelligence officer | `BeliefsFrom` 970-1095 | after resolution, reports arrive `pirep_delay_h` later | claims, sightings, ELINT seconds, photos |
| Lessons, stress, grudges, SEAD lesson | planner, commander, leaders | `Lessons` 1313-1376 | after resolution | losses vs expected, claimed success |
| Careers | superiors | `Careers` 1461-1492 | daily | reputation thresholds |
| Picture merge | analysts | `AirCampaign::Deliver` 708-736 | **hourly** (merges reports due by now + 1 h) | `IntelPicture::Deliver` |

Nothing in the table runs on minutes, and nothing reads a package that is in the air.

## 3. What is learned, and when (`campaign.hpp`)

`ApplyOutcome(Package&, const PackageOutcome&, bool observe)` (858-955):

1. Truth: damage, decoys hit, our missiles spent, the neighbours' rounds (863-887).
2. Squadrons: losses (drones not counted), damaged aircraft back in `logistics.damaged_return_h` (889-906).
3. Politics: losses and damage today, collateral (`Collateral` 957-968), prisoners' pressure (936-942).
4. Officers shot down: killed or captured, `AirStaff::Replace`; a captured one is named to our side at once
   (`OfficerFile::name_known`, 913-934).
5. `BeliefsFrom` (970-1095), every report queued with an arrival time:
   - pilots' claims of the target, inflated by `intel.bda_overclaim` x U(0.5, 1.5) x (1 - 0.5 scepticism) plus
     0.3 x the commander's `report_inflation` (987-988), arriving `pirep_delay_h` later (980);
   - dummies destroyed claimed as SAM sites; radars killed or silent under ARMs claimed (`intel.claim_destroy_when_off`);
   - **launch sightings**: every defence in `o.fired` reported at 700 m sigma, with the losses credited to it, if any
     aircraft survived (1056-1065). In a played raid "fired" is one bool for our vehicle, `launch_seen_`, set when
     any live aircraft is within `scenario.revealed_by_launch_m` (15 km) of the battery at a launch
     (`mission_bridge.hpp:85-94`) - no line of sight, light or smoke;
   - losses nobody saw fire: "unidentified air defence", existence `intel.suspect_after_loss` (1067-1077);
   - ELINT fixes from `o.emissions` (seconds radiated) through `Elint` (1162-1198);
   - photographs from `o.recce_seen`, `recce_latency_h` later.
6. `Lessons` (1313-1376): the planner's `TacticMemory` for `p.tactic` (smoothed by `LearningRate`), stress,
   confidence, reputation; the commander's stress and grudges against `l.by`; each flying officer's grudges; the SEAD
   lesson (`lesson_sead`) after losses to radar SAMs on a package with little SEAD.
7. The journal line (hidden) and `ObserveRaid` (our side's record: 1511-1560) with `RevealFromPatterns` (1562-1587).

Report latencies by faction (`intelligence` blocks): `pirep_delay_h` NATO 1.0, neutral 1.5, rebels 2, Pact 1.0;
`elint_delay_h` NATO 0.3, neutral 2.0, Pact 1.0; `comint_delay_h` NATO 1.0, neutral 4, rebels 3, Pact 1.5. These are
the staff's picture latencies, hours long. There is no minute-scale report path (a flight lead on the radio to his CP).

## 4. The enemy's picture (`simcore/include/simcore/air_war/intel.hpp`)

- `IntelEntry` (27-65): key (truth id or `ghost-n`), believed kind, position and sigma, `exists`, `function`,
  `observed_h`, `source`, mobile, emitter, column speed, believed threat (radius, lethality, low/medium cover, radar,
  channels, max target speed), record (strikes, last struck, claimed destroyed, losses credited). `Live()` =
  exists x function x (1 - 0.7 suspected decoy).
- `IntelReport` (68-94) with `arrive_h` and `observed_h`; `IntelSource` values in `types.hpp`.
- `IntelPicture::Queue` / `Deliver(now, analyst_skill)` (114-132) merge by inverse-variance blend, take a new place
  when it moved more than 3 sigma, believe claims by `1 - 0.6 analyst_skill`, collect `resurrected` and `repaired`.
- `Decay` (136-152): mobile sigma grows by `intel.mobile_sigma_growth_mps`, existence halves every
  `intel.mobile_confidence_half_life_h`; fixed damage heals at the assumed repair rate.
- `BelievedIadsShare` (161-171) drives the phases; `Threats` (174-180) feeds the planner's route pricing.

The picture is one per faction (the staff's). There is no per-flight or per-pilot picture: an aircraft in a session
knows nothing; it follows waypoints and reacts to cues (section 5.2).

## 5. A played package: what the session does and what comes back

### 5.1 Built as a scenario (`raid_scenario.hpp`)

- One target per aircraft of each flight inside `scenario.clip_m` (70 km), spread abreast by
  `scenario.flight_spacing_m`, id `<callsign>-<k>` (446-460). Each aircraft has its own copy of the flight's waypoints:
  **there is no flight object at run time**, only `RaidAircraft::flight` in the manifest (`RaidManifest` 250-272).
- Evasion per target: on the lock with an RWR (EM), on the launch without (504-515); the type's RWR, self-protection
  jammer, ARMs (518-522).
- C2 for our side: the warning nets near the battery (nearest working of each `c2_source` within 150 km), the
  regiment's radars still standing, the PU-12 blind if none (637-700, 912-925).
- The warning: `own_intel.warning_s` (300 s) before the first aircraft, 900 s with a `loose_lips` planner (852-855);
  replaced by the pipeline's own delivery times when a `DeliverySource` is given (859-911).

### 5.2 In the session (runner and bridge)

| What | Where | Knowledge used |
|---|---|---|
| Evade on lock | `TargetPilot::OnLocked` (`target_behaviour.hpp:120-122`), cued by the runner (`CueTarget`, `scenario_runner.hpp:2929-2940`) | the EM RWR's warning when it has one (not the truth) |
| Evade on launch | `TargetPilot::OnLaunch` (117-119), cued by `CueTarget(id, true)` | **the truth**: the launch at that aircraft, no sighting check, no warning from a wingman |
| ARM launch (no EM ARMs) | `MissionBridge::Step` 150-165 | **the truth**: our console's transmitter state (`radiating`, 77-79) |
| EM ARMs | runner, from the aircraft's own RWR (`ArmCarrierDef`, `scenario.hpp:66-80`) | its warning receiver |
| Damage: smoke, fire, jettison, abort | `ScenarioRunner::OnDamage` 2621-2661: abort sends it to its last waypoint (2660); jettison only with the testbed's `waves_` (2653-2659) | its own damage |
| Weapon release | `MissionBridge::Releases` (by throw geometry) | its route and aim |
| Neighbours' fire | `MissionBridge::Neighbours` (by `PassKillChance`) | rule |

No leader's nerve, no flight-level abort, no call to the CP, no retarget, no new target in the session.

### 5.3 What comes back (`MissionBridge::Finish`, `mission_bridge.hpp:238-328` -> `PackageOutcome`, `state.hpp:136-188`)

`losses` (flight, by, where, officer, captured at 50 %), `released_flights`, `aborted_flights` (alive, had a release,
did not release: geometry, not a decision), `delivered`, `hit_nothing`, `damage`, `decoys_hit`, `destroyed`,
`missiles_spent`, `arms_fired`, `emissions` (seconds our radar radiated), `fired` (our vehicle if `launch_seen_`, the
neighbours that fought), `missiles_by`, `radars_killed` / `radars_suppressed`, `recce_seen`, `notes`. With a post,
`reported*` from the records (`LivePackages::ReportedFromRecords`, `package_injector.hpp:246-253`).

Nothing about what the pilots saw or said, when, to whom; nothing about a retask, a jettison, a new target, a call for
help. `CompletePlayerMission` (`campaign.hpp:199-249`) applies it and resolves the rest of the routes outside the
played circle and time (`ResolveContext::played_*`, `resolve.hpp:37-46, 72-99`).

## 6. `LivePackages`: the campaign in step with a session (`package_injector.hpp:162-299`)

- `Step` after every runner step. While a bridge is active it only steps the bridge, lands falling loads and hands the
  outcome back; **it does not advance the campaign** (182-203). Otherwise every `every_s` (30 s) it calls
  `AirCampaign::Advance(HourOf(now))` (204-238), which itself stops at the first player package (`StepTo` 669-678).
- One package at a time; another that comes due waits (header 156-161; `05_THE_SYSTEMS.md` §7 "Left").
- `InjectPackage` (84-154) loads the raid scenario and adds its targets (`AddTargets`), EM gear (`AddEmGear`), warning
  and C2 triggers (`AddTriggers`); **the pipeline's messages are dropped** ("messages of the pipeline are not
  played", 150).
- `JamTheBatteryNet` (261-287): the faction's `comms_jamming` on the battery net by FM 34-40-7's rule
  (`comms_jamming.hpp`). Note: `docs/campaign/04_WHAT_HAPPENS_AND_WHY.md:45` still says voice jamming is "not
  generated by the air war yet"; that line is stale.
- `campaign/mission_session.hpp` owns `LivePackages` (`live_`, 92, 102) beside the regiment's tasking (every minute).

## 7. The faction files on initiative, rigidity, SEAD, hunters, on-call and free hunt

| Field | NATO | Neutral | Rebels | Pact | Read by |
|---|---|---|---|---|---|
| `doctrine.planning_cycle_h` / `lead_time_h` | 24 / 12 | 24 / 4 | 24 / 6 | 12 / 3 | `AirCampaign::Init` 170-174, `StepTo` 659-662 |
| `doctrine.on_call_fraction` | 0.1 | 0.15 | 0.0 | 0.2 | `planner.hpp:297` only: tempo x (1 - f). **Never flown** |
| `doctrine.centralisation` | 0.6 | 0.5 | 0.2 | 0.9 | `planner.hpp:1338` only: TOT jitter |
| `doctrine.simultaneous_bias` | 0.6 | 0.4 | 0.3 | 0.8 | planner |
| `politics.interference_per_cycle` | 0.06 | 0.05 | 0.02 | 0.08 | `Planner::Interference` |
| `officer_pool.trait_mean.rigidity` | 0.35 | 0.5 | 0.3 | 0.7 | generated officers; `LearningRate`, `doctrine_weight` |
| `officer_pool.trait_mean.creativity` | 0.55 | 0.45 | 0.6 | 0.35 | exploration in utility (`planner.hpp:1151`), deception share |
| Hunter template | 1 x 2, medium, dawn/day/dusk: "hunter-killer team on patrol over a suspected battery: fires on whatever radiates (opportune suppression: jp3_01_4_1995)" (`faction_nato.json:46`) | none | none | 1 x 2, medium: "Free search, 'the hunt', when the target's place is not known precisely (mai_tactics_vvs)" (`faction_warsaw_pact.json:40`) | planned as a package with a loiter route; fires ARMs on emitters in resolution and session |
| Armed recce template | A-10A/F-16A/F-4E pairs: FOFA, adopted Nov 1984 (`wiki_fofa`) (39) | Saab 105 pairs near the line (25) | none | Su-25/MiG-27/Su-17M3 pairs: "The free hunt (свободная охота) ... (mai_tactics_vvs)" (34) | `FindColumn` (attacks the first column found) |
| Descriptions | ATO, rollback, low loss tolerance (6) | small, short, low, every aircraft irreplaceable (6) | night, low, "avoiding known guns" (6) | "on-call strikes on what the reconnaissance finds ... Punctual, rigid when things go wrong" (6) | text only |
| `comms_jamming` | TACJAM ground jammers (palii_1989) | none | none | R-330B jamming battalion (stemasov_1969) | `JamTheBatteryNet` |
| Support aircraft | E-3A (3), RC-135 (2), EF-111A (4), KC-135 | — | — | A-50 (2, in service 1985, "very few"), Il-20M (2); no tankers (Il-78 from 1987) | `SupportNow`: AWACS only cuts our fighters' kill chance and the escort need (`resolve.hpp:233`, `planner.hpp:64`); ELINT aircraft widen the ELINT reach |

The Pact's `source` for `on_call_fraction` already names the doctrine: "strikes on call from ground or airborne
alert" (`mai_tactics_vvs`, `faction_warsaw_pact.json:22`). The data has no field for: report latency, retask latency,
who may retask (flight lead / CP / FAC), alert states (ground alert minutes, airborne alert), GCI dependence, radio
discipline in flight, or a faction's appetite for targets of opportunity.

## 8. Gameplay tuning that touches the hot phase (`data/gameplay/air_war.json`)

`pacing.sector_m` 10000, `pacing.max_per_day` 3, `pacing.min_gap_h` 3 (which raids the player meets);
`scenario.revealed_by_launch_m` 15000 (any aircraft near a launch "sees" it); `intel.launch_sighting_m` 20000;
`intel.comint_per_call` 0.3; `own_intel.warning_s` 300, `own_intel.leak_warning_s` 900;
`attrition.escort_fighter_kill` 0.03 and `support.awacs_fighter_cut` 0.5 (our fighters, abstract); `adapt.*`
(learning, stress, reputation). Every key must be listed in `TuningTable::Defaults` (`data.hpp`, after line 705).

## 9. Our side's pipeline (`simcore/include/simcore/command/`)

### 9.1 How a fact reaches the player's character

- `InfoNet` (`info_net.hpp`): a `Fact` (81-100: key, kind, h, origin post, witnesses, text/summary, audience,
  need_level, precedence 0/1/2, document, broadcast, expires, held) moves hop by hop by the best means each pair has
  (urgent: radio, telephone, warning net, siren, runner). `Delivery` (103-113) records who learned it, when, from whom,
  by what means. `Tell` (267) is the player's own telling. The net is a pure function of time-stamped inputs
  (header 22-25), so saves rebuild it. `FactKind` (44-59): War, AirWarning, Order, Intsum, RaidSeen, RaidResult,
  OwnDamage, Prisoner, EnemyOfficer, EnemyKnowsBattery, BatteryNews, Press, Rumour, Mail. **No kind for an intercept,
  an enemy radio call or a tactical warning of an enemy's intention.**
- `CampaignFeed` (`air_war_feed.hpp`): reads the campaign and adds facts at their first knowers (`FeedRoles` 37-51:
  `sector_cp_duty` raid warnings; `div_ad_cp_duty` the warning net's raids; `army_intel` intercepts and analysts;
  `div_intel` prisoners; `kesmark_police`; `rgt_chief_of_staff`; `bty21_commander`; `army_ad_chief`;
  `rgt_political_deputy`). `Pump` (67-180): the battery's places follow its track, danger on the roads and cut wire
  where bombs fell, wrecks reported by the nearest post, raids seen, the journal's visible entries (`FromJournal`
  216-304). The air raid warning is a fact at `sector_cp_duty`, precedence 2, `need_level` 5 (217-228).
- `BatteryCalls` (184-199): our radio calls for the enemy's COMINT, scaled by the radio model's `heard`.
- `em_radio.hpp`: radio hops as EM link budgets; `RadioListener` (53) is **the enemy's** listener ("a ground intercept
  station, a listening aircraft"); with none placed the enemy hears everything (header 9-11). Our listeners: none.
- In the session: `SessionSource` (`scenes.hpp:291`) gives the raid builder the deliveries for the vehicle's seats
  (`DeliverySource::Between`, `AirWarnedAt`, `session_deliveries.hpp:205-213`), computed **before** the session from
  the net. `ScenarioRunner::RegimentSays(text, after_s)` (5956-5966) puts a message on the regiment's command net
  mid-session (heard on the post's R-111, repeated until receipted); `RegimentTellsDestroyed` (5972) and
  `RegimentTellsRoad` (6000) use it. There is no runtime `AddDeliveries`.
- `people.hpp`: the registry of people and the character's papers (staff list, communications diagram), written from
  what reached him.

### 9.2 Our people who would hear the enemy

| Post | Where | Relevant duty |
|---|---|---|
| `army_intel`, Lt Col Viktor Ondrejka | army HQ, level 0 | "radio intercepts of the enemy's air forces; what the analysts make of the enemy's officers" (`chain_pannonia.json:26-30`) |
| `sector_cp_duty`, Maj Laszlo Benedek | national sector CP | "sees the raids coming on the national radars first ... knows where our own fighters are" (13-17) |
| `fighter_rgt_duty`, Capt Gabor Szilagyi | Kecel air base | our fighters' duty officer (18-21); the fighters themselves are abstract (`FrontDef::friendly_fighters`, `data.hpp:158`) |
| `div_ad_cp_duty`, Capt Petr Holub | division AD CP | runs the early-warning net (R-311 receivers) |
| radio-technical battalions | `oob_pannonia.json:301-339` | radar troops (the warning net's radar companies), not signals intelligence |

There is no radio-intercept (radio reconnaissance) post near the battery, no ESM post, and no unit that would place
listeners against the enemy's air nets.

### 9.3 Our fighters and air-to-air

None exist as aircraft. The enemy faces `front.friendly_fighters` x `attrition.escort_fighter_kill` on unescorted
packages at the second route point (`resolve.hpp:231-248`), reduced by AWACS; the planner adds escorts by it. In a
session no friendly or enemy fighter flies against another; `05_THE_SYSTEMS.md` §10 plans "our fighters in the
battery's sky" as IFF-answering targets only.

## 10. Where an HQ loop would sit (hooks)

### 10.1 In the session (minutes; the player sees it)

- **Owner**: `LivePackages` (`package_injector.hpp:162`) already owns one `MissionBridge` per package; a per-package
  controller (flight leads and the package's link to its CP) would live beside `bridge_` and step with it (182-203).
- **Reads**: the runner's public state (`TargetAlive`, `TargetPosition`, `TargetVelocity`, `Launches`, `ArmResults`,
  `Log`), and events the bridge already detects (`launch_seen_`, releases, ARM ends: `mission_bridge.hpp:85-195`). A
  sighting model (smoke and trail by range, light and weather) would replace the 15 km bool.
- **Steers**: the runner can already hold and steer a target: `ScenarioRunner::Instruct` (10984-11048: Take, Turn,
  Height, Speed, Evade, Jam, Release) and `InstructorEnter` (11051: new aircraft now). Limits: `TakeTarget` refuses a third
  held target ("the trainer holds two targets at a time", 11251-11257: `kInstructorTargets`), and there is no call to
  give a target a new route. A package controller needs a runner call to re-route many aircraft (new waypoints, a
  new aim and release point in the manifest) without the trainer's cap.
- **New aircraft**: an on-call or diverted package enters with `InjectPackage` / `AddTargets` (11078); `LivePackages`
  must take more than one package at a time.
- **Back to the campaign**: `PackageOutcome` must carry what changed (retargeted flights and their new aims,
  jettisons, calls made, reports sent), and `save.hpp` must save it (the campaign replays from `CampaignAction`s;
  `CompletePlayerMission` stores the whole outcome: `campaign.hpp:199-204`).

### 10.2 In the campaign (the war beyond the battery)

- Packages resolve atomically at take-off (`StepTo` 664-680 -> `ResolveAbstract`). An HQ that reacts to a report
  needs packages in the air across steps: a stepped `Resolver` (the leg walk of `resolve.hpp:134-267`, by time
  instead of whole) with an event list (launch seen at `LossRecord::where`, column found, losses).
- Reports in: a minute-scale `IntelSource` for in-flight reports, with its own `arrive_h`; `AirCampaign::Deliver`
  (708-712) merges hourly and must become finer for them.
- Orders out: the on-call reserve is already priced (`planner.hpp:297`) and never spent. A pool on `SquadronState`
  (`state.hpp:190`) with alert states, and a planner entry that builds one package now for a reported target
  (reusing `BestPackage`/`Build`/`Commit`), is the natural hook. `Package* PlayerMissionMutable()` (`campaign.hpp:195`)
  exists for amending the package in play.
- People: the decision maker per faction (the commander's CP, a GCI controller, a FAC) as `Officer`s or a new role in
  `StaffRole`, so traits and quirks bend retasking (rigidity, boldness, `by_the_book`, `loss_shy`).
- Latency and authority: new faction data (report, decide, transmit, alert-to-airborne, who may retask), each value
  sourced.

### 10.3 Determinism

Both the campaign (`save.hpp` header 1-10) and `InfoNet` (header 22-25) are pure functions of their seeded inputs. Live
decisions must draw from forked RNGs keyed by package and event (as `rng_.Fork` is used at `campaign.hpp:920`) and
their results must reach the save through `CampaignAction`s or the `PackageOutcome`.

## 11. Enemy emissions and intercepts to the player, first person

What exists to build on:

1. The EM layer is side-aware: `em/esm_system.hpp` (passive receivers: RWR, ESM, ELINT, TDOA; contacts identified
   against "what its side believes", `Identify` 77-94) and `em/comms_system.hpp` (radio nets as link budgets).
2. The enemy aircraft in a session emit only through jammers and ARMs (and are heard by their own RWRs): no voice
   nets, no air-to-ground or air-to-air radars, no navigation emitters are written by the raid builder
   (`raid_scenario.hpp` writes `rwr`, jammers, `arms`).
3. The chain has the right owner for intercepts (`army_intel`) and the feed already maps "intercepts" journal lines to
   an `EnemyOfficer` paper from him (`air_war_feed.hpp:286-293`).
4. `RegimentSays` carries a message into a running session on the regiment's net; the radiotelephonist's journal keeps
   it (the record principle, `docs/command/01_PRINCIPLES.md` 9).

The path a design would follow (no HUD at any step): enemy transmission (EM emitter, on its band) -> heard by **our
listener at a post** (a new chain position with a place, means and skill; an ESM or radio-intercept site as an EM
receiver of our side) -> written down there -> a `Fact` of a new kind at that post, precedence by urgency ->
`InfoNet` carries it by that post's means (the army's teleprinter and telephone are slow; a tactical warning would need
a direct net to the division's AD CP or the regiment) -> in the session, `RegimentSays` or a delivery to the seat ->
the battery's journal -> the report and the regiment's picture. Items: a `FactKind` for intercepts; a chain position
for the listener; a runtime delivery call (or use `RegimentSays`); live pipeline messages for injected packages
(dropped today, `package_injector.hpp:150`).

## 12. Cold War consistency (the owner's "again - consistent with cold war, not modern tactics")

What the code already holds to 1985 (theatre `year` 1985, `data/air_war/theatre_pannonia.json:12`; the nation's
theatre dated 15 Oct 1985):

- Weapons by year: the F-4G's AGM-78 before HARM (1987) (`arm_em_by_year`, `data.hpp:343-345`); the Pact's Kh-22
  but no tankers (Il-78 from 1987: `faction_warsaw_pact.json:133-134` notes); Tomahawk and CALCM gated to later wars.
- Pact centralised planning, massed timed blows, on-call from ground or airborne alert, free hunt, tactical groups
  (`mai_tactics_vvs`, `cia_1985_air_op`); A-50 "very few in 1985".
- NATO's 24-hour ATO, rollback, FOFA adopted Nov 1984, E-3A NAEW operational by 1985 (`afmag_1984_nato`).
- Our side's pipeline is voice, wire, runners, teleprinter, the R-311 warning net (`utin_2018`), no data links to the
  battery beyond the PU-12's designations.

What a new design should keep out, because it is later than 1985 (to confirm against registered sources before data):

- Dynamic targeting as practised from 1991: kill boxes, "Killer Scout" flights, JSTARS-cued retasking, time-sensitive
  targeting cells, GPS weapons. In 1985 a strike retargeted in flight would be by voice from a FAC, a CP or an
  airborne command post, with the delays and garbles of voice.
- Fighter and strike data links as a general means: in 1985 strike aircraft talk by voice. Exceptions that are period
  (to verify): Soviet GCI command links for interceptors (the MiG-21/MiG-23 "Lazur" class), and the E-3's own links to
  ground C2.
- `jp3_01_4_1995` is cited for NATO's hunter ("opportune suppression") at `faction_nato.json:46`: a 1995 joint
  publication. Wild Weasel hunter-killer teams are period (Vietnam onward), but any new rule taken from that source
  should be checked against a 1980s one.

Period behaviour the request asks for, and where the code would hold it:

- Pact strike crews: routes and times fixed by the CP, little room for initiative, retarget only on the CP's or the
  FAC's order (ПАН with the troops: `mai_tactics_vvs` already cited at `campaign.hpp:833-835`), interceptors under
  GCI. Sits in the faction data (centralisation, a new authority/latency block) and the officers' rigidity.
- NATO strike crews: pre-planned ATO packages, CAS and BAI through TACP/ASOC and FACs, Weasels hunting with the
  flight lead's judgement, AWACS controlling fighters by voice.
- Radio: aircraft voice nets are UHF/VHF AM (NATO 225-400 MHz; Pact sets of the R-832 family, to verify); the
  battery's R-123M (20-51.5 MHz) does not cover them, so no crewman overhears pilots in the cabin. Intercepts reach the
  battery only through our radio-intelligence people and the chain.

## 13. Gaps (plainly)

1. No live enemy decision in a played session above the single aircraft; no leader's nerve there.
2. Campaign frozen while a package is in the session; one package at a time.
3. Abstract packages resolve whole at take-off: nothing is "in the air" for an HQ to act on.
4. `on_call_fraction` held back and never flown; no alert pool, no on-call package builder.
5. `centralisation` used only for timing jitter; no authority or latency model for retasking.
6. Pilots below the squadron leader are not people; no wingmen, no crew skill per pilot.
7. No in-flight report path (minutes); report latencies are hours; picture merges hourly.
8. No enemy C2 posts (CP, GCI, FAC, airborne CP) as people or places.
9. Launch "seen" is one bool within 15 km; evade-on-launch without RWR uses the truth (the launch at that aircraft).
10. Non-EM ARM firing uses our console's true transmitter state.
11. No enemy voice nets, radars or nav emitters in the session; nothing for our ESM or intercept to hear.
12. No listener of ours (no post, no EM receiver); no `FactKind` for intercepts; no runtime deliveries; injected
    packages drop their pipeline messages.
13. No air-to-air: our fighters abstract (`friendly_fighters`), `air_to_air` read by nothing, escorts a presence flag,
    AWACS only a multiplier.
14. Retargeting, jettison, calls and reports are not in `PackageOutcome` or the save.
15. Doc drift: `04_WHAT_HAPPENS_AND_WHY.md:45` (voice jamming "not generated by the air war yet") is outdated.

## 14. Constraints new work must respect

- simcore: header-only C++17, standard library, no exceptions or RTTI, no Unreal or Windows macro names, SI units,
  ENU, warnings as errors (`CLAUDE.md`); every model gets a test (`simcore/tests/test_air_war.cpp`,
  `test_packages_live.cpp`, `test_command.cpp`).
- First person (`docs/command/01_PRINCIPLES.md` 1-8): nothing about the enemy's live intentions on a HUD; every fact
  has an origin post and travels by a means.
- The written record (`01_PRINCIPLES.md` 9): intercepts and warnings that reach the battery go into the journal; our
  reports and the regiment's picture come from records, not truth.
- NPCs act on knowledge (crew pillar 3), by analogy for enemy pilots: the request's "pilots' vision and sensors"
  removes truth cues such as `CueTarget(id, true)` without a sighting.
- Determinism and saves: seeded, replayable from `CampaignAction`s and `PackageOutcome`s; `InfoNet` a pure function of
  inputs.
- Strict loaders: unknown keys fail (`data.hpp` `Keys`); every tuning key listed in `TuningTable::Defaults`.
- Sources: every new faction or doctrine value needs `source` and `confidence`, registered in `docs/SOURCES.md`, no
  tier 5 (`tools/check_citations.py`); prefer operator-country sources for the Pact, period sources for both sides.
- Balance: `air_war_balance` and `air_war_bench` calibrations (`docs/air_war/09_BALANCE.md` §8-10) must hold; the
  director never changes a package's odds (`05_THE_SYSTEMS.md` §8).
- Crew and talk rules: new speech goes through the callout bus's router and nets (`docs/crew/12_COMMUNICATIONS.md`,
  `15_TALK.md`); a new callout needs a menu place.
