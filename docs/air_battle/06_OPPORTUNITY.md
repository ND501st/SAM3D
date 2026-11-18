# 6. Targets of opportunity

Code: a new `simcore/include/simcore/air_war/opportunity.hpp` (the chain and its rules) and `attack_pass.hpp` (passes
flown); changed: `mission_bridge.hpp` (`Releases`), `test_waves.hpp` (`Pending`, `Releases`), `resolve.hpp`
(`FindColumn`), `campaign.hpp` (`AimOnCall`, `BeliefsFrom`), `state.hpp` (`PackageOutcome`), `nav/movement.hpp`
(`ArmedRecceSees`); data: an `opportunity_rules` block in each `data/air_war/faction_*.json` (the faction's base
tendency is `hot_phase.opportunity`, `10_FACTIONS.md` §7), per-type keys in the same files, the `opportunity` group of `data/gameplay/air_war.json`, `kind: gun` and `rockets` entries of
`data/air_war/weapons.json`. Research: `docs/research/air_battle_2026-11/opportunity.md` (main), `perception.md`,
`c2_retasking.md`, `factions_cases.md`, `reactions.md`; code maps `map/routes.md`, `map/c2_people.md`, `map/flight.md`.

What a flight does when it sees something it was not sent for: a column on the road, a parked vehicle, a site that
fires at it. Today only an armed reconnaissance in the abstract resolver attacks what it finds (`FindColumn`, the first
column on its stretch), close support is aimed at the troops' true place (`AimOnCall`), and every weapon, guns
included, is released once at a fixed point. This doc replaces those with a chain of decisions each flight leader
takes on what his flight has seen, and with attack passes that are flown.

## 1. The period's main finding: most flights never see the target

The evidence is one-sided and strong. Seeing, not the weapon, limits attacks on targets nobody briefed.

- In the early 1980s F-4 crews flying close support in Germany "usually never even saw a target", and when they did it
  was too late to attack. The targets were one or two parked vehicles, dark on a dark background, with no movement,
  smoke or dust. At 450-500 kt the pop-up began about 3 nm out (`bush_a10_simhq`, unverified: the interview could not
  be re-read on 15 Nov 2026, so its numbers are low confidence).
- Misty FACs in 1968 judged that strike pilots on armed reconnaissance were "unable to see and/or identify targets". A
  flight lead with over 20 sorties in Route Package One first saw a gun site and a truck on a Misty ride, though he had
  "struck" both many times. A Fast FAC needed 20 missions to train and about 40 to be highly qualified
  (`checo_rp1_1969`, night armed reconnaissance section).
- NATO's reading of Soviet practice gave the low, fast attacker 3-6 s to identify his target and 10 s to leave the
  area (`fm_100_2_1_1984` p. 12-8).
- Targets expected where the briefing put them were found sooner and more often; finding a target of opportunity is a
  separate, harder problem. In one simulation pilots found 61 % of targets and observers with fewer tasks 73 %
  (`jones_atg_1974`).
- Target and background explained 62 % of acquisition variance in JTF-2; speed between 350 and 550 kt changed little
  (`jones_atg_1974`). Movement, a line feature and an uncamouflaged target help (`agard_cp100_1972`, Bailey).
- Air defence sites "are seldom detected unless they are firing" (`agard_cp100_1972`, Hilgendorf); an F-105F flight
  found an SA-2 site by the dust of its launch (`checo_1967_tactics`).

Three rules follow. A flight looks for targets of opportunity only as a by-product of flying its mission, so the find
chance is low and depends on the type, the crew, the light and whether the target moves. A target seen late cannot be
attacked on that pass. And a deployed, camouflaged Osa battery is found mostly by its own launch, dust and smoke, and by
the warning receivers and ELINT, which are other docs' business.

## 2. The authority to attack is set before take-off

A pilot's right to attack what he finds comes from his mission and the area he is in, fixed at the briefing. None of
the factions gave it to him in the air, except by a ground node clearing a strike.

| Faction | Authority, 1985 | Sources |
|---|---|---|
| NATO | **Armed reconnaissance**: a mission "to find and attack targets of opportunity" in assigned areas or along assigned routes. **Beyond the FSCL** (normally set by corps) or in a **free fire area**, any target may be attacked "without ... additional coordination". **Squadron areas**: the 81st TFW's A-10 plan from the late 1970s to about 1984 made each 5 x 10 nm border area a box assigned to a squadron; once NATO's own forces had withdrawn west of it, "any military object in it would, by definition, be a target" (unverified). The word "kill box" is the interview's later term, absent from the 1980 and 1984 manuals: SAM3D calls these `squadron_area`. Elsewhere a flight on another task reports, and hits back only at guns or SAMs that fire on it. | `fm_101_5_1_1980`; `bush_a10_simhq` (unverified); `checo_roe_1973` ch. V |
| Warsaw Pact | **Report and request** is the default: every crew reports enemy activity at once by radio; the target goes to the ground and an on-call strike comes from readiness one (3-5 min) or two (15 min). **Free hunt** only in zones given by the plan, and only once the air is held ("some reservations ... until air superiority"); an armed reconnaissance is one reconnaissance aircraft with two to four attack aircraft. A planned strike follows its plan unless ground control changes it. Bukhantsev names strikes on request the principal method, but for the anti-landing battle on a maritime front: reading it as the general default is an extrapolation that FM 100-2-1 supports. An order of 1981 in Afghanistan forbade attacks on unidentified targets. | `fm_100_2_1_1984` pp. 12-4, 12-6, 12-7; `mt_1970_bukhantsev`; `markovsky_afghan_sky` |
| Neutral | No period source. SAM3D assumption: centralised, no in-air retasking; attack pairs near the line strike what the plan names and report the rest after landing. A small force's pilot sent to look may be "allowed to attack" by his brief (Crippa's MB-339 at San Carlos, 1982, an analogue, not a neutral). | `c2_retasking.md` c2-40; `ellitoral_crippa` |
| Rebels | No source on rules. Pilots plan single low passes on infrastructure, but "the adrenaline game of shooting rockets" cost two MFI-9s to ground fire in shoot-ups (one veteran's account, low confidence). SAM3D: no net, no clearance; the pilot is his own authority. | `garrison_biafra` |

Two period rules belong here because they show what a restrictive war looks like, and the air war's politics may
impose them:

- **The road corridor.** In Laos from December 1965 armed reconnaissance was free within 200 yards of motorable roads;
  beyond that a target needed validation, a FAC, the attaché's approval, or guns firing on the aircraft
  (`checo_interdiction_1969` ch. IV). By 1969-72 the corridor was 200 m, with no strike within 500 m of an active
  village unless fired from (`checo_roe_1973` ch. III). SAM3D carries this as `road_corridor_m` and
  `village_keepout_m` in a faction's `opportunity_rules` block, off by default, switched on by a political event (NATO's
  `escalation_power` lifts it, a ceasefire-talks event can set it).
- **Protective reaction.** Over North Vietnam in 1970-72 SAM and AAA sites could be struck right after they fired;
  attacks on "other unfriendly forces or installations encountered" were prohibited (`checo_roe_1973` ch. V). In 1964
  aircraft fired on at low level could retaliate on the first or on circling passes (`checo_roe_1966`). SAM3D:
  `fired_on_may_attack` per faction.

**Night hunting cleared by the ground.** In Afghanistan the most experienced helicopter crews, in pairs or sections,
found vehicles moving against the curfew by their headlights, reported the coordinates to the nearest outpost, and
struck only once its commander, who held his region's convoy movement plan, had cleared them (`rgs_afghan_2002`
pp. 219-220; the text gives no year, so its dating before 1986 is an assumption). It is the period's clearest rule
against hitting one's own side, and it needs a curfew or a movement plan to work. SAM3D gives it to the Pact's
helicopters as `night_hunt_clearance: "ground_post"`: the strike waits for the clearance, which travels by the
faction's voice net (`09_COMMS_AND_INTERCEPT.md`) and can be late, refused or jammed.

## 3. The chain

Each flight leader runs the chain at 1-5 Hz, staggered by a fixed key, inside the flight's command layer (decision 4 of
the brief): wingmen see and call, the leader decides, the flight acts together. Every step is data per faction and per
type, moved by the officer's traits (at most about +-0.2) and the pilots' experience, currency and nerve (about +-0.15,
`factions_cases.md` §1). The order of the steps is a design choice, not a sourced rule (`opportunity.md` opp-gen-01).

```
seen -> identified -> authorised -> able -> worth the threat -> act -> report
```

### 3.1 Seen

Input: what each pilot's eyes give him, from the perception model (`03_PERCEPTION.md`), never the truth. For a
ground target that is the launch-opportunity model of NWC TP 6005: `P_max` by contrastiness and pattern (1.00 for a
large high-contrast pattern down to 0.25 for a small low-contrast one), reached at 0.375 of `R* = min(unmask range,
visibility)`, half of it at 0.5 R*, a fifth at 0.625 R* (`nwc_tp6005_1978`). What this doc adds is the target's side
of it, in `opportunity.hpp`:

| Target state | Pattern, contrast (SAM3D mapping) | Why |
|---|---|---|
| Parked, camouflaged vehicle or battery not firing | small, low | opp-nato-06; `agard_cp100_1972` |
| Column on the move by day, dust | large (a line feature), medium | movement and line features help (`agard_cp100_1972`, Bailey); moving vehicles seen at slightly greater range (`jones_atg_1974`) |
| A site at the moment it launches | large, high | "the cloud of dust caused by the launch" (`checo_1967_tactics`) |
| Vehicles with lights at night | medium, high, within the flare or light's reach only | `rgs_afghan_2002`; night by eye under flares (`agard_cp100_1972`) |
| Same at night without lights, fixed wing | the find chance stays at `logistics.find_night` (0.15) | opp. §4 item 6 |

The movers come from the session's roads (`nav/session.hpp`: our columns, the TZM's run, the battery on the march),
the battery's vehicles and dummies, and the objects of the map. A battery on the march is therefore far more likely to
be found than one deployed and silent, which is the period's lesson and fits track G.

Type and crew enter here: two-seaters and slow attack aircraft see more (the observer with fewer tasks, 73 % against
61 %, `jones_atg_1974`; FB-111 low level "at minimum a two-man job", the A-10 pilot "eyes-out-of-the-cockpit",
`agard_cp267_1979` DeHart); high-speed low-level flight "results in loss of lookout" (`agard_cp267_1979` Lee). The
per-type key `seats` is `02_FLIGHTS_AND_PILOTS.md`'s (§3.2) and the lookout shares are `03_PERCEPTION.md`'s (§2.2);
this doc only reads them.

### 3.2 Identified

A sighting is a believed kind and place, with an error. Identification takes time: the attacker has 3-6 s
(`fm_100_2_1_1984` p. 12-8); recognition after detection was modelled at 2.7 s in 1970, judged optimistic against
flight trials (`jones_atg_1974`). The chain draws an identification time from the type and the crew
(`opportunity.identify_s`, a game value centred on 4.5 s), and the target is identified only if the aircraft is still
outside its minimum release range when it ends (`MinimumRunM`, §5.1). Otherwise it was "too late to attack it" on this
pass.

Identification can be wrong. The chance of taking a dummy, a civilian lorry or our own column for the target is
`opportunity.misidentify` by target state (game values), higher at night, under stress and for the less experienced.
Where friendly troops are near or the forces are mixed, the flight does not attack: it reports or makes a
reconnaissance pass. After the Golan breakthrough in 1973 "no clear distinction between friendly and enemy positions"
made close support less workable (`entwistle_1988`), and an Su-17 leader who refused his FAC near Kandahar in 1984
bombed his own battalion (`markovsky_afghan_sky`). The faction's `own_troops_keepout_m` (the Pact's peacetime 200-700 m,
`fm_100_2_1_1984` p. 12-5; helicopters 1,500 m for bombs, 500 m for rockets, 300 m for machine guns,
`markovsky_afghan_sky`) is read against where the flight believes its own troops are.

### 3.3 Authorised

`OpportunityRules::May(faction, mission, area, target, fired_on)` returns attack, report-only or clearance-needed:

| Case | NATO | Pact | Neutral | Rebels |
|---|---|---|---|---|
| Armed reconnaissance in its area or on its route | attack | attack, only in a zone the plan gave and once the faction's picture shows the air held | attack near the line | attack |
| Beyond the FSCL or in a free fire area | attack | n/a | n/a | n/a |
| In its squadron area, own forces gone (`squadron_area`, unverified) | attack | n/a | n/a | n/a |
| Strike (`Strike`, `Dead`) or escort on another task | report; hit back at what fires on it | report | report after landing | attack if able |
| CAS under a FAC | the FAC's brief only | the FAC or control post confirms at the checkpoint | n/a | n/a |
| Helicopter night hunt | n/a (no source) | clearance from the ground post | n/a | attack |
| Road corridor rule switched on | attack only inside it | as left | as left | ignored |

"The air held" is the Pact's believed state, not the truth: its campaign has reached the phase
`hot_phase.authority.free_hunt_from_phase` (`10_FACTIONS.md` §7.3; phases end on the believed `BelievedIadsShare`), and
it has had no losses to our fighters in the last `opportunity.air_held_h` (a game value). Who may widen the authority in
flight is `05_C2_AND_RETASKING.md`'s business: a FAC, an ASOC divert, a Pact control post; there is no datalink and no airborne retasking of
strike packages in 1985 (`c2_retasking.md` §1.2).

### 3.4 Able

What the flight still has decides what it can do:

- **Stores.** Before the release on its primary a flight on a planned strike keeps its weapons for the primary unless
  its mission is armed reconnaissance (the primary's priority). After the release, only what is left: the gun, rockets
  or a CBU kept back. A-10 pilots in Europe carried Mavericks, the gun and CBUs and meant to "shoot them from a safe
  distance, and then mop up later with the gun" (`bush_a10_simhq`, unverified). The weapons that fit come from the
  existing `CanDeliver` and `Weaponeer` (`10_WEAPONS.md`) against the believed target class, so a flight with only
  laser-guided bombs and no designator cannot take a column.
- **Fuel.** A flight below its type's `bingo` share of fuel after the attack's time and the way home reports only. An
  A-10's sortie was about 2 h (`tradoc_jaat_1983`); fast jets on deep strikes have minutes over the area.
- **Time.** A flight on a time on target or an escort's slot does not leave it (`opportunity.slot_slack_s`, a game
  value; AI time windows of 20 min were assumed in 1987, `schoeck_1987_ai_retasking`).

### 3.5 Worth the threat

Guns or SAMs seen or firing near the target cut the chain to one pass or none. "Most pilots of high performance aircraft
consider more than one attack in the target area to be suicidal" (`smith_1979_cas`). Over the Golan in 1973 about half
of the 80 Israeli aircraft lost fell to AAA (`entwistle_1988`), and two of the three Harrier GR3s shot down in 1982 were
re-attacking (`rafm_falklands`, the official historian quoted second-hand). The threat test reads the flight's believed
threats (what it has seen fire, what its warning receivers say, the briefed threats), weighted by the doctrine's
`delivery_risk` (`10_WEAPONS.md`) and the leader's nerve (`04_REACTIONS.md` §9; `hot_phase.press_on`, `10_FACTIONS.md` §7).

### 3.6 Act

| Action | When | How it is flown |
|---|---|---|
| Ignore | not authorised and nothing worth reporting, or radio silence holds and a debrief will do | flies on; the sighting is kept for the debrief |
| Report | authorised to report only, or not able; the Pact's default | a call on the faction's net if in reach and the radio rules allow (`09_COMMS_AND_INTERCEPT.md`); else at the debrief |
| Reconnaissance pass | seen but not identified, or friendly troops may be near, and the threat allows | NATO's Misty way: do not circle, fly on, come back once low and fast (`afmag_2006_misty`); the Pact's helicopters circle at 1,500-2,000 m to judge a caravan, at most 30-35 min on station (`rgs_afghan_2002`); a WWII hunter made one or two dummy passes when unsure (`skomorokhov_1985`, background only) |
| Impromptu attack | identified, authorised, stores that fit | a pass from §5 with the weapon the weaponeer picks |
| Gun or rocket run | identified, authorised, only the gun or rockets left | a low-angle gun pass or a gentle dive (§5.2) |
| Re-attack | the first pass failed or was too late, the threat allows, the faction re-attacks | from a new heading after a separation, or out of the sun (`fm_100_2_1_1984` p. 12-8), with a higher loss chance (§5.4) |

The leader's choice among the allowed actions is a weighted draw: the faction's base tendency for targets of
opportunity, shifted by the officer and the pilots, then by the threat. The bases are the faction's `hot_phase.opportunity`
(`10_FACTIONS.md` §7.2), SAM3D estimates bounded by the sources (`factions_cases.md` §1): rebels 0.7, NATO 0.5, neutral
0.4, the Pact 0.2. Boldness and the `press_on` quirk
raise the attack and the re-attack; rigidity and `by_the_book` raise reporting over attacking; `loss_shy` and low nerve
cut re-attacks; `chases_kills` (already in `traits.json`: "Goes back to what he has struck") raises the re-attack after
a claimed hit. A Pact leader with high boldness may take the free hunt he was allowed; he does not invent a target
change in the air (Johnson found no example of one: `johnson_1986`, unverified).

### 3.7 Report

Every sighting ends in a report, in the air or after landing, with the pilot's error of position and of kind:

- **Pact**: "aircraft crews on any mission are expected to immediately report observed enemy activity"
  (`fm_100_2_1_1984` p. 12-6), by voice to the ground post in reach; at low level radio is "reduced to a minimum or
  prohibited entirely" (p. 12-8), so a strike flight on its run in reports after leaving. After landing the pilots
  report to the regiment's staff within 5-10 min, and it goes at once to the air force's command post
  (`markovsky_afghan_sky`).
- **NATO**: crews send results in flight only if requested (`army_it0654_inflightrep`); a flight under a FAC or on an
  ASOC net tells them; the rest at the debrief.
- **Neutral and rebels**: after landing.

A report in flight is an emission on the faction's net: our intercept posts may hear it by rule and pass it through the
pipeline (decision 7). A report in flight reaches the enemy's staff as an `IntelSource::InFlightReport` (new in
`05_C2_AND_RETASKING.md` §4.3) with an arrival in minutes, not the hours of today's `intel.pirep_delay_h`; a report after
landing stays a `PilotReport` with the faction's debrief delay. Reported targets are what the posts of
`05_C2_AND_RETASKING.md` (§4.3-4.4) turn into on-call strikes.

## 4. Types that favour rapid target change, and types that do not

The owner asked for this explicitly. The mission sets the authority (§3.3); the type sets how easily its crew can use
it. A MiG-27 on armed reconnaissance hunts by the armed-reconnaissance rules; the same type on a planned deep strike is
on the right of this table. Each type gets `opportunity_ease` (0..1, a SAM3D estimate ordered by the reasons; it scales
the find-to-act steps, not the find itself) and `bingo_share`.

| Type (data id) | Ease | Why |
|---|---|---|
| A-10A (`a10a`) | 0.8 | Close support was its main mission; pilots flew routinely over the expected battle area, had three radios against the F-4's one and better maps (`bush_a10_simhq`, unverified); a 2 h sortie and check-in with ordnance and time on station (`tradoc_jaat_1983`); 40-60 % of a mission below 500 ft, "eyes-out-of-the-cockpit" (`agard_cp267_1979` DeHart); the GAU-8 for leftover passes (`cdat_array20_1980`) |
| Su-25 (`su25`) | 0.7 | Held airborne alert zones at 3,000-3,500 m and the infantry could call it onto firing points at once; a support attack lasted up to 15-20 min (`markovsky_afghan_sky`); cannon and rocket pods |
| Mi-24 (`mi24`) | 0.7 | Helicopters were preferred for immediate strikes near own troops, under the air defence umbrella (`fm_100_2_1_1984` p. 12-8); crews relied on eye and skill more than the sight (`markovsky_afghan_sky`); slow, can circle, inspect and fire |
| AH-1S (`ah1s`) | 0.6 | The joint air attack team with A-10s and a FAC in an Army helicopter (`tradoc_jaat_1983`); helicopter attacks lasted 40-45 s (`mt_1986_dobedin`, Soviet view); needs the team's handover, so lower than the Mi-24 alone |
| Harrier GR3 | not in the data | Two GR3s a day stood at a forward strip for quick reaction in 1982; many sorties were "risky armed reconnaissance" (`rafm_falklands`). RAF Germany's Harrier force in 1985 is to be sourced before the type is added; it would sit beside the A-10 |
| Mi-8 with door guns (`mi8_doorgun`), light jets (`galeb`, `utva75`, `saab105`) | 0.5-0.6 | Slow or light, few stores, the pilot his own authority (rebels) or a pair near the line (neutral) |
| F-16A (`f16a`), F-4E (`f4e`) | 0.4-0.5 | Fast single-seat or two-seat strikers flying CAS as a back-up role "usually never even saw a target" (`bush_a10_simhq`, unverified); one radio in the F-4 |
| MiG-27 (`mig27`) on a planned strike | 0.2 | The Pact's planned strike follows its flight plan unless ground control changes it (`fm_100_2_1_1984` p. 12-4); radio at a minimum on the run in |
| Tornado IDS (`tornado_ids`), F-111F (`f111f`), Su-24 (`su24`) | 0.1-0.15 | Deep strike and interdiction were not retasked "up to the very point of target engagement"; the means "to change rapidly and repeatedly" did not exist (`schoeck_1987_ai_retasking`, 1987, writing of it as a capability still to come); fuel and the time on target leave no slack; loads chosen for the primary (runway weapons, laser-guided bombs); poor outside visibility in the F-111 and a workload that pulls the eyes inside (`agard_cp267_1979` DeHart); the terrain-following radar and autopilot widen the flying limits (Lee) but do not free the crew to search |

The four reasons, each a term in the chain rather than a hidden number:

1. **Stores**: what fits the believed target after the primary (§3.4). A Tornado with JP233 has nothing for a column.
2. **Fuel**: time on station against the way home (`bingo_share`, the type's `radius_m`).
3. **Emission and radio discipline**: a flight under radio silence cannot report or ask for clearance until it may
   speak; the Pact's strike crews are the strictest (`hot_phase.radio_discipline` 0.8, `10_FACTIONS.md` §7.2, a SAM3D estimate).
4. **The primary's priority**: before its release a planned strike holds course; a CAS, armed-reconnaissance or alert
   flight has no primary but the area.

## 5. Attack passes flown

Today a weapon is released once at a fixed point (`MissionBridge::Releases`, `TestWaves::Pending` and `Releases`); gun
"weapons" (`gau8`, `gsh30`, `door_guns`) are footprints released like bombs, and there is no re-attack, no second pass
and no change of aim (`map/flight.md` §3.5). This doc replaces the release gate with `AttackPass`, a small state machine
the flight's command layer flies on the R3 flight model:

```
setup -> run_in -> (pull_up -> apex | roll_in) -> track -> release or fire -> recover -> egress | separate -> re-attack
```

The planned attacks keep `10_WEAPONS.md`'s profiles as the pass's geometry: the pop-up's pull-up 6 km out and 1.5 km
to the side, apex 1,200 m, release 700 m at 1.5 km; the dive's roll-in 5 km out and release at 1,500 m and 2.5 km; the
toss, laydown, level, stand-off and hover as there. The release itself still uses the throw from
`weapon_flight.hpp`, so a planned attack in a played raid lands where it lands today, and `test_packages_live.cpp`'s
equivalence holds. What changes is that the pass is a state the leader enters, can enter again, and can enter for a new
aim.

### 5.1 Too late on this pass

The NWC launch-opportunity sample gives a decision time of 1 s, a roll-in of 0.5 s, a minimum release range of 3,000
ft and a 1 s roll-out at 450 kt; raising the operating time from 2 to 10 s dropped the launch probability from 0.5 to
0.35 (`nwc_tp6005_1978`). `MinimumRunM(type, delivery)` is the distance flown in decision, roll-in and tracking plus the
minimum release range. A target identified inside it cannot be attacked on this pass: the flight passes it and either
reports or sets up a re-attack. This is how "if we did see it, it was too late to attack it" enters the game.

### 5.2 Gun and rocket runs

Two new `Delivery` values: `Strafe` (a low-angle gun pass) and `GentleDive` (cannon and rockets from a shallow dive).

- **Strafe**, the A-10's pass from the 1979 test: open fire at a mean slant range of 1,115 m in a mean 3.3 degree dive,
  a 2.01 s burst of 135 rounds; 0.14 of rounds hit directly and 0.18 with ricochets; ten passes on ten stationary,
  undefended tanks immobilised 8, about 0.8 a pass, 3 of them catastrophic kills (`cdat_array20_1980`; the report's own
  tables are not fully consistent).
- **GentleDive**: the Pact's conventional attack with cannon and rockets from a 10-25 degree dive
  (`mt_1970_bukhantsev`, stated for attacks on landing forces; the angle is used here for any rocket or cannon pass).

A gun run's effect per pass is a chance per aim point under the gun's cone, set so that the bench reproduces Array 20
against stationary, undefended, found tanks and nothing more: the report's own caveat is that the targets were known,
stationary and undefended, and finding them is the real limit (`opportunity.md` §4 item 3). Under fire the burst is
shorter and opens further out (`opportunity.gun_burst_under_fire`, a game value), and a moving target lowers the hit
share. The `footprint_m` and `effect` of `kind: gun` and `rockets` stay for the abstract resolver (A0) only.

### 5.3 Columns

- A march column, a line target usually 1 km or longer, is attacked by a single aircraft or a small group along its
  long axis; helicopters attack it from the flank; a multiple target of 10-20 in 1-1.5 km takes 2-8 aircraft
  (`fm_100_2_1_1984` p. 12-7).
- Crossing a column gives a short exposure and little time to acquire; running down the road gives more time and more
  exposure (`crawford_1977_rand`). The leader picks the run by the threat he believes.
- The WWII Soviet hunters struck a column's head or caught it in a defile (`skomorokhov_1985`, background for the idea,
  not 1985 tactics). SAM3D aims the first pass at the column's head where the road allows.
- Our column answers: on the commander's signal its air defence fires, it speeds up and opens to up to 100 m between
  vehicles, and against a large group it may disperse or take cover off the road (`fm_100_2_1_1984` ch. 5, the march;
  NATO's view of Soviet practice). The march column's movers in `nav/movers.hpp` take this as an order from their
  commander, heard by voice: no mover acts on the truth.

### 5.4 Re-attack

A re-attack is allowed by NATO and the Pact, rarely by the neutral and the rebels (`opportunity.md` §4 item 4). It comes
after a separation of at least `opportunity.reattack_separation_m` and on a heading at least
`opportunity.reattack_heading_deg` from the first, or out of the sun when the sun is low enough to blind (the sky
clock, `simcore/light.hpp`). Each re-attack multiplies the defences' kill chance on the aircraft by
`opportunity.reattack_risk`, a game value: the sources give the direction (the Golan, the Falklands GR3s, `smith_1979_cas`)
but no number. An Su-17M3 was shot down near Kandahar in summer 1983 on its sixth pass, which Markovsky blames on
stereotyped attacks (`markovsky_afghan_sky`): the chain counts passes, and each pass after the second on the same
heading raises the risk again. For our crews a flight turning back toward the battery is the attacker's riskiest moment
(`opportunity.md` opp-our-02).

## 6. Targets from headquarters

A target can reach a flight from outside: a FAC talks it on, a Pact control post confirms it at the checkpoint, an ASOC
diverts a CAS flight, a Weasel lead passes a site within his team, an on-call package is launched on a report
(`05_C2_AND_RETASKING.md` §4.3-4.5). The flight then runs the same chain from "seen", with two differences:

- The briefed or talked-on place makes the target expected, and expected targets are found sooner and more often
  (`jones_atg_1974`): the find uses the TP 6005 curve with the cue's error as the search area.
- Authority comes with the message (the FAC's clearance, the control post's confirmation), so §3.3 is already
  answered, unless the flight doubts it. The 1984 Kandahar leader who refused his FAC is the failure case.

`AirCampaign::AimOnCall` stops reading the troops' true position: the aim is the FAC's or control post's believed
position from `05_C2_AND_RETASKING.md`'s picture (§4.3), with its age, and the flight must still find the target there.

## 7. Claims go into the enemy's records as claimed

Pilots' reports of what they hit were badly inflated. First-light photographs over six weeks in 1969-70 confirmed 7 of
103 reported truck kills, and the totals were never revised (`nalty_2005` p. 89, an official history written in 2005
about period records). SAM3D keeps two ledgers:

- **The claim**: what the leader says he hit, with his error, written to the flight's record and reported. It enters
  `BeliefsFrom` as a pilot report, inflated by the existing `intel.bda_overclaim` and the commander's
  `report_inflation`, plus a per-target-class factor `opportunity.claim_factor` (game values; vehicle kills at night
  the highest, anchored on the 7 of 103). The enemy's picture discounts claims only when photo control is flown
  (`opportunity.md` §4 item 7).
- **The truth**: what the pass did, kept for the debrief and the campaign's truth only.

`PackageOutcome` gains `std::vector<OpportunityRecord> opportunities`: flight, time, the believed kind and place, the
action, the claim, and the truth's effect (object id and working share removed). `MissionBridge::Finish` fills it in a
played raid; the resolver fills it at A0; the save writes it with the outcome, so a loaded campaign replays the same
claims. Our side sees the other half: the crews write what they saw (a pair circling at height, a low pass, a column
hit), and the records, not the truth, feed our reports.

## 8. Levels of detail

| Level | What runs |
|---|---|
| A2 (inside the play circle) | The whole chain per flight at 1-5 Hz, sightings from the perception model, passes flown by `AttackPass`, reports on the nets |
| A1 (flown, coarse) | A sighting roll per flight at 1 Hz against movers within `logistics.search_m` by `ColumnFindChance` (`nav::ArmedRecceSees` widened from armed reconnaissance to any flight, with the type's ease), the authority and ability steps, an attack resolved as one pass by the weapon's footprint, a report |
| A0 (abstract, time-stepped) | `Resolver::FindColumn` generalised to `FindOpportunity`: armed reconnaissance as today; any other flight with leftover stores and authority on the movers its route passes, at a rate cut by `opportunity_ease`; claims and truth recorded |

Hand-over keeps a flight's sightings and its pass count, so a flight that saw a column at A1 and came into the circle
can re-attack it at A2.

## 9. Determinism and data

- Every draw (find, identify, misidentify, the action, the pass's hits) comes from `rng.Fork(key)` keyed by package,
  flight, target id and pass number; nothing reads the fx feed. Adding the chain to the abstract resolver must not
  shift the campaign's main stream (`map/routes.md` §12).
- A faction's `opportunity_rules` block: `report_in_flight`, `fired_on_may_attack`, `night_hunt_clearance`, `own_troops_keepout_m`, `road_corridor_m`, `village_keepout_m`, `reattack`, each with
  `source` and `confidence`; values with no source say "SAM3D estimate". The base tendency and the free-hunt gate are not here: they are
  `hot_phase.opportunity` and `hot_phase.authority` (`10_FACTIONS.md` §7.5). Each aircraft type: `opportunity_ease`,
  `bingo_share`. Game values in `data/gameplay/air_war.json` `opportunity.*`: `identify_s`, `misidentify`,
  `air_held_h`, `slot_slack_s`, `gun_burst_under_fire`, `reattack_separation_m`, `reattack_heading_deg`,
  `reattack_risk`, `claim_factor`; every key listed in `TuningTable::Defaults`.
- New sources (`checo_interdiction_1969`, `checo_roe_1973`, `checo_roe_1966`, `cdat_array20_1980`, `nalty_2005`,
  `jones_atg_1974`, `rgs_afghan_2002`, `mt_1970_bukhantsev`, `afmag_2006_misty`, `rafm_falklands`, `entwistle_1988`,
  `garrison_biafra`, `bush_a10_simhq`, `fm_101_5_1_1980`, `skomorokhov_1985`) registered in `docs/SOURCES.md` with
  their tiers; `tools/check_citations.py` passes.
- Nothing from after 1985: no kill boxes as a term, no Killer Scouts, no push CAS, no JSTARS cue, no NVG fast-jet
  night hunting, no OA-10 FAC role (1987), no airborne holding with pop-up tasking as the default (a 1986 NATO
  evaluation: at most an officer's quirk, `opportunity.md` §4 item 5).

## 10. As built (B6, 18 November 2026, branch `tb-b6`)

**Files.** `simcore/air_war/opportunity_def.hpp` (the faction's `"opportunity_rules"` block with a source per rule, a
weapon's `"pass"`, the `"opportunity"` game values), `opportunity.hpp` (the chain: `RunChain`, `MayAttack`,
`AbleToAttack`, `OppWorth`, `ActShare`, `Reattack`, `Claim`), `attack_pass.hpp` (`AttackPass`, `GunPassEffect`),
`opportunity_session.hpp` (`OpportunityRun`: the chain and the passes in a played raid), small hooks in
`mission_bridge.hpp`, `resolve.hpp` (`FindOpportunity`), `campaign.hpp`, `package_injector.hpp`, `raid_scenario.hpp`,
`flights.hpp` (`PilotOverride::Kind::Attack`, `OverrideOf`), `scenario_runner.hpp` (`FlyPass`, `PassOf`, `EndPass`,
`StrikeMover`, and the dummies and the roads' movers as things a pilot can see), `state.hpp`
(`PackageOutcome::OpportunityRecord`), `save.hpp`; the data in each `faction_*.json`, `weapons.json` and
`data/gameplay/air_war.json`; `tests/test_opportunity.cpp`; case 9 of `air_war_bench`. The chain's draws use
`em::Stream::Opportunity` (21), mirrored in `prototype/samproto/em.py`.

**The chain (§3).** One leader per flight runs it, at `opportunity.tick_s` (1 s), staggered by a keyed draw.
- *Seen.* The flight's members' own sightings (B3's `PilotPicture`): ground objects, launch signs, the origin of a
  trail. Sightings within `opportunity.merge_m` are one contact. A contact within 1,500 m of a member's own aim is his
  target, not an opportunity.
- *Identified.* A contact less sure than `min_confidence` waits. Identification takes a log-normal time about
  `identify_s` (4.5 s), sooner for a skilled pilot, an easy type and a calm one. He may take it for what it is not
  (`misidentify_day` 0.05, `misidentify_night` 0.15): then he believes it a battery.
- *Authorised.* The faction's rules (`MayAttack`): own troops' keep-out; fired on (NATO, the rebels) he may hit back
  at that site only; a village keep-out; the Pact's helicopter night hunt waits for the ground post; armed
  reconnaissance by the faction's rule (NATO and the rebels: its area; the Pact: the free hunt's zone and phase, from
  B5's `c2.free_hunt`; the neutral: near the line); close support by the FAC, beyond NATO's FSCL, or in an A-10
  squadron's area; any other task reports, unless the faction attacks what it finds (the rebels).
- *Able.* Not damaged; a gun, rockets, or bombs no longer owed to his target; above bingo fuel (estimated from the time
  flown); time before his slot; room for a pass (`MinimumRunM`: 2.5 s at his speed plus his open-fire range, never under
  3,000 ft). Too close, he looks again on a later tick.
- *Worth it.* The value of what he believes it is, by his confidence, less the threat he has seen near it times his
  delivery's exposure, times `reattack_risk` for each pass already flown, by his nerve.
- *Act.* On a hunting task (armed reconnaissance, close support, a helicopter raid) he acts at 0.85 moved by his traits
  and pilots; on any other at the faction's `tendency` moved the same way, times `0.4 + 1.2 x ease` of his type. If not,
  an unsure pilot on an easy type flies a recce pass; the rest report.

**Reports.** A faction with `report_in_flight` (the Pact) says it on the flight's net (`AirRadio`), and
`LivePackages` passes it to the posts (`NoteReport`: a column, parked vehicles as troops, a battery as a launch). NATO,
the neutral and the rebels report at the debrief: the record is written, and the campaign reads it after the mission.

**Attack passes (§5).** An attack or gun run puts every member with stores on a pass in trail (`trail_m` apart); a recce
pass is the leader alone. A pass is a `PilotOverride` of kind `Attack` on B1's layer (`ScenarioRunner::FlyPass`): out to a
setup point `run_in_m` back if he has no room, the run at the aim, and the recovery past it and to the right. It opens
fire within the weapon's `open_fire_m` and 15 degrees of his nose. A break, a dive or a jink (B4) takes the layer and
ends the pass. Re-attacks (`Reattack`) come from `reattack_heading_deg` off at `reattack_separation_m`, at most
`max_passes`. A fixed-wing aircraft whose planned load is a gun or rockets flies its planned attack as passes too, from
`run_in_m` out: its first burst or salvo is its release, and the one-shot release gate is skipped for it. Bombs keep
the release gate.

**Effect.** At the burst the pilot lines up on what is really there within `find_m` (300 m) of where he believed it:
our vehicle, the battery's vehicles, a dummy, a theatre object or a mover on the roads. One keyed draw against the
weapon's `pass.per_pass` for its class (`GunPassEffect`), halved or so if it moves, times `gun_burst_under_fire` under
fire, worse by eye at night. A hit puts it out of action through the bridge's existing damage (R1's states for our
vehicles and objects; a mover is `BrokenDown`). The fx feed gets a release and an impact with delivery `strafe` or
`gentle_dive`.

**Claims (§7).** `Claim`: a miss is claimed a kill at `claim_on_miss_day` (0.25) or `_night` (0.6); a kill claims
`claim_extra_*` more. The record (`OpportunityRecord`) holds the belief, the act, where the chain stopped and why, the
passes, the claim, the object the enemy takes it to be about, and the truth. The campaign's `BeliefsFrom` reads the
claim, not the kill: a claimed object reads as destroyed, unless a skeptical staff doubts a lone claim (function 0.5).

**The abstract level (A0).** `Resolver::FindOpportunity`: a flight that is not on armed reconnaissance may find a
column on the move its leg passes within `logistics.search_m`, at `ColumnFindChance x a0_find_scale x ease`. It runs
the same chain. An attack removes `a0_column_hit` of the column per aircraft and loses one to its guns at
`a0_column_guns`. Draws are keyed by the campaign's seed, the package and the column: the main stream is not moved,
and with `a0_find_scale` 0 a campaign runs as before B6 (`test_opportunity`).

**Old saves.** The save writes `opportunity_mode`; a save without it loads with mode 0 and replays as before. Records
are written only when present.

**Shortcuts.** Passes are flown level at the pilot's held height: the dive is only in the open-fire range and the
effect. Opportunistic attacks use guns and rockets only, never bombs, in a session. Fuel is estimated from the time
flown. The FAC's designation reaches the session's chain since S2. The tendency stays in `opportunity_rules` until B11's
`hot_phase`. The claim has day and night factors, no `claim_factor` per class. The test waves gate on the open-fire
range but fly no passes. A contact's chain draws are fixed per contact.

**Balance.** `air_war_bench` case 9 (400 runs): ten GAU-8 passes on ten known, stationary, undefended tanks put 7.9
out of action (4-10), 2.9 burnt, against Array 20's 8 and 3; on a column on the move under fire 2.5 (0-7). Cases 1-8
do not move (the bench's resolver has no campaign seed, so no finds). The weaponeer never picks `strafe` or
`gentle_dive` in the campaigns: no attack line moves for them. In `air_war_balance` (20 seeds) the A0 finds move our
columns' losses: the rebels 0 to 1 vehicle a campaign, the neutral 0 to 3-4, the Pact 28 to 32-42, NATO 48-50 to 50-54
(on the nation, 10 seeds: the neutral 0 to 13-21, NATO 20-33 to 60). The rest moves within the seeds' noise (packages
and losses a few per cent either way), because a column hit on the way changes what the staff plans next. NATO's attacks
on unbriefed columns outnumber the Pact's (its A-10 beyond the FSCL against the Pact's strikes that only report); the
rebels attack least, for want of aircraft with guns or rockets near our lines, not of will. `crew_balance`,
`helper_coverage` and `ground_check data 3600` do not move (nothing under the ground). `test_air_war`'s NATO check of a
strike with SEAD past a known radar was judged over four seeds; such strikes are 3 in 100, and the finds moved seeds 1-4
to none, so it is now judged over sixteen.

**S2 (19 November 2026, branch `tbr-s2`; `05_C2_AND_RETASKING.md` §9).** The FAC's `designate` reaches the session's
chain: when a close support flight (or the Pact's helicopter raid) of a raid in the session comes within
`c2.fac_contact_m` (25 km) of the FAC who last saw its target, the FAC calls the mark on his net (`LivePackages`,
`AirRadio::SayFrom`). If the flight's leader hears it, `OpportunityRun::Designate` marks the place for that flight, its
radius three times the sighting's sigma and at least 500 m, and a contact the leader sees there has `designated` set:
NATO's close support, which otherwise strikes only what the controller designates, may attack it. A jammed net stops a
strike: unheard (our jammer on the FAC's net, out of range, the FAC's post out), the faction's `clearance` rule decides;
NATO's close support holds (`Hold` by its leader, `hold_s` 5-15 min) and asks again, then goes home with its load; the
Pact's attacks on its plan. The abstract campaign does the same at the brief (`AirC2::Brief`). `Played` counts the
marks, the unheard marks, the holds and the aborts. Off with `c2.live_mode` 0.

**Left:** `TestWaves::Releases` through `AttackPass`; bombs dropped on targets of opportunity in a session; the dive flown
in the pass; A1 (the coarse flown level); a check that a silent battery is found less often than one on the march;
the 24 h `mission_demo` counts per faction (§ Done when 3) as a test; `AimOnCall` (already legacy only).

## Done when

1. `tests/test_opportunity.cpp` passes:
   - a Pact MiG-27 on a planned strike that sees a column reports it and does not attack; the same type on an armed
     reconnaissance in its zone attacks once the faction believes the air held, and reports while it does not;
   - a NATO A-10 flight in a `squadron_area` or beyond the FSCL attacks a column it identifies; a Tornado on an
     interdiction strike with JP233 sees it, cannot attack, and reports at the debrief;
   - a NATO flight fired on by a gun may attack that gun and nothing else it passes;
   - a Pact helicopter night hunt waits for the ground post's clearance, and a jammed net means no strike;
   - a target identified inside `MinimumRunM` is not attacked on that pass;
   - friendly troops believed within `own_troops_keepout_m` stop the attack;
   - every decision replays bit for bit from the same keys, and adding a flight's chain does not move another
     package's draws.
2. A new case in `air_war_bench` (Array 20) flies ten `Strafe` passes by two A-10s on ten stationary, undefended,
   known tanks and immobilises 6-10 of them; the same against a moving column under fire immobilises far fewer.
3. Over a 24 h `mission_demo --war nato` and `--war warsaw_pact`, the counts of sightings, reports, attacks and
   re-attacks per faction keep the order rebels > NATO > neutral > Pact for attacks on unbriefed targets, and most
   sightings by fast jets on deep strikes end in a report or nothing.
4. A deployed, silent battery is found by armed reconnaissance far less often than the same battery on the march, and
   most often in the minute after it launches.
5. `MissionBridge::Releases` and `TestWaves::Releases` go through `AttackPass`; `test_packages_live.cpp` still finds an
   injected raid flies as the same raid played as its own scenario; `test_threats.cpp`, `test_damage.cpp`,
   `ground_check data 3600` (nothing under the ground in a dive or strafe) and the `air_war_balance` rows hold or
   move with a written reason.
6. `PackageOutcome::opportunities` round-trips through the save; the enemy's journal shows claims, the debrief the
   truth, and our records only what our crews wrote.
7. `AimOnCall` reads no truth; `rg "AimOnCall"` shows it takes a believed position from `05_C2_AND_RETASKING.md`'s picture.

## Left open

- **In-flight report formats.** NATO's INFLTREP and ATP-27(B), and the Soviet in-flight report rules (доклад с
  воздуха), were not found: what a pilot says and how long it takes is a game value until they are.
- **USAFE and RAF Germany armed reconnaissance rules, 1980-85.** The only European evidence is one A-10 pilot's
  interview, which could not be re-read; the squadron areas, the 3 nm pop-up and the F-4's 450-500 kt are unverified.
  FM 90-21 (JAAT, 1983) and TACP 50-20 are to acquire.
- **Soviet photo control** (фотоконтроль) and how far the Pact's staff trusted claims: not found, so the Pact's claim
  discount copies NATO's 1969-70 record by analogy.
- **Soviet gun pods and S-8 rockets against columns**: their use is not sourced; the S-8's 1984 service date is in
  `weapons.json` from `wiki_weapons`.
- **The neutral's and the rebels' rules**: no period source; both are SAM3D assumptions.
- **The Harrier in RAF Germany in 1985**: not in the data and not sourced for 1985.
- **The re-attack's added risk, the misidentification rate, identification times by type and the claim factors**: the
  sources give directions, not numbers. They are game values to calibrate on the Golan and Falklands loss shares and
  the 7 of 103.
- **The Osa's launch smoke and dust as seen from a cockpit**: no source; `munitions.json`'s "dense" is an estimate, and
  it drives the most important sighting in this doc.
- **The share of sorties that took targets of opportunity or were retasked in 1980-85**: no figure found
  (`c2_retasking.md` c2-51).
- **Sensor dates** (Pave Penny on the A-10, Kaira on the MiG-27K, Klen on the Su-25) are unsourced; until they are, no
  type finds ground targets by sensor in this chain.
