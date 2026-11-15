# 5. Command in the air: reports, diverts and the alert

Code it will touch: `simcore/include/simcore/air_war/` `campaign.hpp` (`Advance`, `StepTo`, `ResolveAbstract`,
`Deliver`, `AimOnCall`, `CampaignAction`), `resolve.hpp` (`Resolver`, made time-stepped), `planner.hpp` (`BestPackage`,
`Build`, `Commit`, a new on-call builder), `package_injector.hpp` (`LivePackages`), `officers.hpp` and `data.hpp`
(`StaffRole`, a new `C2Def`), `state.hpp` (`SquadronState`, `PackageOutcome`), `intel.hpp` and `types.hpp`
(`IntelSource`), `save.hpp`; data `data/air_war/faction_*.json` (a new `"c2"` block), `data/gameplay/air_war.json` (a
new `c2` group). Research: `docs/research/air_battle_2026-11/c2_retasking.md` (the main source), `factions_cases.md`,
`comms_emcon.md`, `opportunity.md`; code maps `map/c2_people.md`, `map/routes.md`, `map/scale.md`.

The question this doc answers: once a raid is in the air, who may change a flight's route or target, by which means,
and how long it takes. In 1985 the answer is "rarely, by voice, through a ground node, and slowly". The design gives the
enemy that, and no more.

## 1. What the period says, in short

1. **Planned strikes fly as planned, on both sides.** NATO's interdiction was "hardly" retasked in the air: a 1987
   thesis sponsored by USAFE's 17th Air Force wrote that the means to retask air interdiction "up to the very point of
   target engagement" did not exist, and that "someday airborne diverts of our air interdiction assets will be
   possible" (`schoeck_1987_ai_retasking` ch. 1-2). Pact aircraft "follow the original flight plan through friendly
   antiaircraft defenses unless changed by ground control" (`fm_100_2_1_1984` p. 12-4).
2. **In-flight changes happen near the front, for close support, through a ground node.** NATO: the corps' ASOC and the
   ATOC, with a FAC on the ground or in an Army helicopter. Pact: the division's aviation control element, the control
   and target-identification posts and the regiment's FAC at the checkpoint (`fm_100_2_1_1984` pp. 12-2 to 12-6).
3. **A battery that shows itself can be attacked on three clocks** (`c2_retasking.md` §1.2): by aircraft already there
   with leave to attack threats (minutes); by an immediate request near the front (15-50 min); by nomination into the
   next tasking cycle (a day or two).
4. **Warnings are not retasking.** An E-3A, a CRC or another flight warns by voice; the flight leader evades, jettisons
   or aborts on his own. Those reactions are `03_PERCEPTION.md`'s, `04_REACTIONS.md`'s and `02_FLIGHTS_AND_PILOTS.md`'s.
5. **Voice is the medium.** No data link reached a strike aircraft in 1985 (`miller_1980_tacs_jamming`: JTIDS
   not fielded; the data links of `smart_1985_tacs` ch. 3 join the CRC/CRP to the E-3A, not to aircraft). Every
   request, divert and report is a transmission that can be jammed, lost, late or heard
   by our side.

What the code does today (`map/c2_people.md` §0, §2): no enemy decision is taken during a package; an abstract package
resolves whole at its take-off hour (`campaign.hpp:664-680`); the campaign stands still while a package is played
(`campaign.hpp:183`, `package_injector.hpp:182-203`); `doctrine.on_call_fraction` lowers the tempo and the held-back
sorties are never flown (`planner.hpp:297`); report latencies are hours and the picture merges hourly
(`campaign.hpp` `Deliver`); `AimOnCall` aims close support at our troops' true place (`campaign.hpp:836-844`).

## 2. Who may change a flight in the air, 1985

### 2.1 NATO, Central Region

| Node | May | Means | Evidence |
|---|---|---|---|
| Flight leader | inside his frag only: attack, take the frag's alternate, abort, jettison, evade on a warning; no new target outside the frag | his own judgement | `c2_retasking.md` §1.1; `smith_1979_cas` (one pass in a high threat) |
| FAC (ground, or in an Army helicopter) | brief and clear flights handed to him; cannot pull flights off other missions | VHF-FM/UHF voice | `tradoc_jaat_1983`; `hoover_1979_fac`; no airborne FAC squadrons left in Europe (`skattum_1988_oa10`) |
| ASOC (at each corps) | fill immediate requests from allocated sorties, airborne alert, ground alert at 5 and 15 min, and authorised diversion of preplanned CAS | request net up; voice to flights, direct or relayed by a CRC, CRP or E-3A | `hoak_1987_retasking` ch. 3-4 (citing TACR 55-46, 1986); `tradoc_jaat_1983` |
| ATOC (the TACC in US terms; its FIDO) | tactical control of the offensive aircraft allotted to it; divert any BAI (J-SAK, 1984, informing the Army only); scramble outside one corps' allocation; rarely redirect an airborne interdiction mission before its IP; divert a recovery to another base | voice through the CRC/CRP/E-3A; teletype and EIFEL to the wings | `rippe_1985_airland` pp. 31-35; `rifenburg_1989_c2`; `schoeck_1987_ai_retasking` |
| CRC/CRP | warn; scramble or divert air defence fighters "with TACC concurrence"; no source gives it authority over strike flights | UHF voice | `smart_1985_tacs` ch. 3 |
| E-3A (NATO, Geilenkirchen; 12 of 18 delivered by late 1984) | detect, warn, control fighters; no period source gives it strike retasking | UHF voice | `afmag_1984_nato`; `rippe_1985_airland`; `c2_retasking.md` c2-13 |
| Wild Weasel lead (F-4G hunter-killer team) | his own team only; 7th Air Force in 1966 refused Iron Hand "blanket authority to divert strike flights" | voice within the team (an inference, unverified) | `checo_1969_tactics`; `neitzel_1985_weasel`; `c2_retasking.md` c2-15 |

The **request chain** under the 1976 test rules: a TACP sends an immediate request on the Air Force request net to the
ASOC; the levels between acknowledge; the request stands unless one denies it within "a preset time limit"; "airborne
diverts must be approved by the Army" (`morrow_1977_tacs`, citing TACR 55-46 (TEST), 1976). The only number found for
the window is 5 min, from the early Vietnam ARVN/VNAF net (`morrow_1977_tacs`). Under J-SAK the BAI divert is the
ATOC's alone (`rippe_1985_airland`). SAM3D uses both, by mission: a CAS divert needs the corps' agreement; a BAI divert
is the ATOC's (`c2_retasking.md` §4.2). In the Central Region air support was target-oriented, not mission-oriented: a
ground unit cannot hold a flight and use it as it likes (`rippe_1985_airland` pp. 33-35).

**How fast.** Response times as the research corrects them (`luke_2002_cas`, hindsight, third-hand from the JCS study
of 1972; medium confidence, never the only basis):

| Case | Request to bombs |
|---|---|
| Korea 1950-53, Army/USAF system | 40-50 min average |
| Korea, Marines (aircraft preassigned to front-line units on air alert) | 10-15 min |
| Vietnam before the 1965 changes | over 90 min, about half met |
| Vietnam after 1965 | 20 min from airborne alert, 40 min from ground alert |
| JCS goals, 1972 | 50 % within 15 min, 75 % within 20 min, all within 40 min |

SAM3D takes the achieved figures as typical and the goals only with airborne alert (`c2_retasking.md` §4.4). An A-10
sortie lasted about 2 h (`tradoc_jaat_1983`); a pilot's attention flagged after 30 min of low-level airborne alert (US
tests, in the Soviet reading: `mt_1986_dobedin`).

**Slow paths.** A division's BAI nomination took 48 h to be flown, against a division planning horizon of 24 h
(`gallant_knight_1983`). A site found on photographs had six to eight hours to move (`checo_1967_tactics`). Crews sent
results in flight only if asked (`army_it0654_inflightrep`). A diverted aircraft got its next target at the allied base
it landed at, not in the air (`afmag_1983_usafe`). In 1987 "dynamic retasking" by the ASOC was "not currently recognized"
officially, although "considerable retasking" had happened in recent conflicts, and the ASOC's procedures were manual
(`hoak_1987_retasking` ch. 1-2).

### 2.2 The Warsaw Pact

| Node | May | Means | Evidence |
|---|---|---|---|
| Pilot | fly the plan; some targets of opportunity along the route on his own initiative (Streb, from the Soviet press); picks targets in an area only on armed reconnaissance or a free hunt | none | `fm_100_2_1_1984` pp. 12-4, 12-6; `streb_1980_frontal_aviation` |
| Flight leader | assign individual targets once the FAC confirms | short calls at most | `fm_100_2_1_1984` p. 12-4 |
| FAC (an air forces officer, generally a pilot, at regiment when air is allotted) and the control and target-identification posts, on the ground or in the air | designate or confirm the target at the checkpoint; change the route through friendly air defence | "brief coded messages and prearranged signals" on the command radio; beacons, flares, smoke, panels | `fm_100_2_1_1984` pp. 12-2 to 12-5 |
| Division's aviation control element (two sections in BTR-60s) | approve an immediate strike within the division's allocation; army or front above it | radio to the air army's KP | `fm_100_2_1_1984` p. 12-5 |
| Air army and air regiment KPs | scramble from readiness 1 and 2; "occasionally" divert an armed reconnaissance aircraft to a request in its area | command radio | `fm_100_2_1_1984` pp. 12-5, 12-7 |

Readiness (`fm_100_2_1_1984` p. 12-7; given for fighter-bomber aviation, "probably also applicable" to other ground
attack):

| Readiness | Crews | To take-off | Held for |
|---|---|---|---|
| 1 | in the cockpit | 3-5 min | 1-2 h |
| 2 | near the aircraft | 15 min | 2-4 h |
| 3 | stores not loaded | 1-2 h | 2-4 days |

Request to strike from airfield alert: 20-30 min in Soviet doctrine of 1962; MiG-17s in a Northern Group of Forces
exercise of November 1961 took 17-28 min (`mt_1962_kontsevoy`). The two agree with the readiness table if transit and
the checkpoint take 10-15 min (`c2_retasking.md` §4.6). Free hunting waited for air superiority; armed reconnaissance
flew one reconnaissance aircraft with two to four attack aircraft; crews on any mission were "expected to immediately
report observed enemy activity" (`fm_100_2_1_1984` p. 12-6). Radio on the attack was "reduced to a minimum or
prohibited entirely", the approach 50-100 m high (p. 12-8), so a flight is often outside the ground post's radio cover
when it would report. Frontal aviation would be released to the fronts only after the opening air operation, small at
first (`dia_1977_fa_support`; `cia_1985_air_op`, excerpt only): on-call strikes are rare in the first phase and grow.

**Afghanistan, 1980-85, as local-war practice** (`markovsky_afghan_sky`): Su-25s held airborne-alert zones at
3,000-3,500 m on a schedule or on the KP's order, and the infantry could call them at once; a pair stood ground alert at
Bagram (from spring 1985); An-26RT relays hung over strike areas because aircraft below the ridges lost radar and radio cover; pilots
reported to the regiment's staff within 5-10 min of landing and it went at once to the air force KP; in summer 1984
near Kandahar an Su-17 leader refused the FAC and bombed his own battalion (4 killed, 9 wounded). SAM3D uses this for a
Pact war only where the faction data says the airborne-alert zone is allowed (§5.4), not as the European default.

**Fighters under GCI.** A GCI system had a channel limit: an air army with three division and nine regiment command
posts could guide 30-40 groups or single aircraft at once, and fighters beyond that flew "a sortie for nothing"
(`mt_1962_front_aviation`). A Western reading held that missions under positive ground control were aborted if contact
was lost (`lambeth_1984`, quoting Skinner). Fighter control is `07_AIR_TO_AIR.md`'s (the enemy's) and
`08_OUR_FIGHTERS.md`'s (ours); here a GCI post only relays reports and warnings.

### 2.3 The neutral

No period source was found (`c2_retasking.md` c2-40; Switzerland's FLORIDA and Austria's Goldhaube are open).
**SAM3D assumption:** a small, centralised national system; fighters ground-controlled; ground attack flies planned
missions only, on ground alert at most; no FACs in the air; no in-air retasking beyond recall. `factions_cases.md`
proposed "by the leader, 5-15 min" for the neutral (a SAM3D estimate); this doc uses the stricter assumption of
`c2_retasking.md`, and the leader's own choice inside his plan stays (`factions_cases.md` table).

### 2.4 The rebels

No source. **SAM3D assumption** (`c2_retasking.md` c2-41): a handful of aircraft; each sortie flies its own plan; the
leader is his own retasker; ground radio to their own troops at most; no warning system beyond their eyes and agents.
The pattern of small air arms is the most individual initiative and the least coordination (`ellitoral_crippa`,
`biafra_minicoin`, both tier 4).

### 2.5 Who is better at it

- **NATO** is the most flexible in execution but not in the air: pilots trained for comm-out packages and their own
  judgement (`rafhs_j20_1999`, Red Flag packages "with no radio calls required"), and its corps ASOCs could divert CAS.
  Its weakness is the slow, manual chain above the flight and the 48 h BAI cycle.
- **The Pact** is fast at launching a new task from alert (3-5 min) and slow or unable to retask an airborne flight
  (`factions_cases.md` §4.3). Its weakness is the dependence on ground control: Syria in 1982, cut off from GCI by
  jamming, "lost any semblance of air discipline" (`lambeth_1984`). Johnson found no example of a target change
  in the air (`johnson_1986`, unverified: not re-read).
- **The neutral** is slow and careful; **the rebels** have nothing to retask with, and act on what they see.

The faction values the design uses (`factions_cases.md` §1; every number a SAM3D estimate bounded by the sources; keys
and values as in `10_FACTIONS.md` §7):

| Parameter | Rebels | Neutral | Pact | NATO |
|---|---|---|---|---|
| `initiative` (the flight leader's, in the air) | 0.8 | 0.6 | 0.25 | 0.7 |
| Retask an airborne flight | the pilot's own choice | recall only (§2.3) | ground control only; armed reconnaissance "occasionally" | FAC; ASOC (CAS); ATOC (BAI) |
| Latency to retask an airborne flight | none (no net) | recall only, 5-15 min (`10_FACTIONS.md` §7.4) | 10-30 min, often never | 3-10 min (FAC route only) |
| Latency to launch a new tasking from the ground | hours | 30-60 min | 3-5 min (1), 15 min (2), 1-2 h (3) | 5 min (airborne alert) or 15 min (ground alert) to launch (`hoak_1987_retasking`; `10_FACTIONS.md` §7.4); from request to aircraft over the target longer (§2.1); next ATO otherwise |
| `plan_adherence` | 0.3 | 0.6 | 0.9 | 0.7 |
| `opportunity` (attack targets of opportunity) | 0.7 | 0.4 | 0.2 | 0.5 |

## 3. The owner's example, faction by faction

A wingman sees a launch from our battery, the flight evades, the lead reports. Whether, and how fast, SEAD or attack
aircraft are sent to the launch area. The seeing, the evasion and the calls inside the flight are `03_PERCEPTION.md`'s,
`04_REACTIONS.md`'s and `02_FLIGHTS_AND_PILOTS.md`'s; this doc takes it from "the lead decides to report". The times are composed from the
parts in §2 (a SAM3D composition, `c2_retasking.md` c2-50), each part sourced or marked.

| Step | NATO | Pact | Neutral | Rebels |
|---|---|---|---|---|
| Inside the flight | "SAM" call on the flight's frequency, seconds; SAM-launch calls by radio were US practice (`mt_1973_nvn`, `mt_1984_babich`) | a short call at most; radio minimal or prohibited on the attack | a call (game value) | a call if the set works |
| The lead reports | deep (AI) strike: no; it flies comm-out and reports at debrief. In the corps' area (CAS, BAI): to the FAC, or by a CRC, CRP or E-3A relay to the ASOC | "immediately" if in a post's radio cover; at 50-100 m often not | to the national CP by voice if in cover | no net |
| Who can act at once | a Weasel team in the same package, on its own authority, on the site it hears or finds: minutes | an armed reconnaissance or free-hunt flight given that area, "occasionally": minutes | nobody | the lead himself: turn and attack, or go home |
| Who can act by request | the ATOC diverting a BAI flight or scrambling ground alert (5 or 15 min); CAS diverts only with the corps' agreement. Aircraft over the place 20-50 min after the report | the division's aviation control element, within its allocation: readiness 1 or 2, 20-30 min request to strike (17-28 min in 1961). Rare during the opening air operation | the CP may scramble ground alert: 30-60 min (SAM3D estimate) | none |
| If nobody acts | report at debrief, intelligence in hours (`pirep_delay_h` 1.0), nomination to the next ATO: BAI 48 h; a photo fix 6-8 h old | report 5-10 min after landing to the regiment's staff, then the KP (Afghan practice); the next 12 h plan | report after landing; the next 24 h plan | report after landing (`pirep_delay_h` 2); whenever they fly again |
| The position's age when struck | minutes (Weasel); 20-50 min (divert); a day or more (nomination) | minutes (hunt); 20-30 min (alert); 12 h or more (plan) | an hour or more | hours |

What it means for the player. Against NATO near the front, a launch that a CAS or BAI flight saw can bring aircraft over
the launch area in half an hour to an hour, by which time a mobile battery that moved is somewhere else; the Weasel team
inside a package is the fast and real danger. Against the Pact, the danger is the on-call strike from readiness 1, if
the division has an allocation and the report got through a radio net at low level; early in the war it seldom does.
The neutral and the rebels punish a launch only on their next plan. In every case the enemy strikes the **reported**
place, with its sigma and its age, never the true one.

## 4. The design

### 4.1 Enemy command posts as places with people

A new `"c2"` block in each faction file lists the posts (`C2PostDef` in `data.hpp`): id, kind, place (theatre ENU, or
an orbit for an airborne post), the nets it works on (`09_COMMS_AND_INTERCEPT.md`), its reach, and whether it is staffed by an authored or a
generated officer. Kinds:

| Kind | NATO | Pact | Neutral | Rebels |
|---|---|---|---|---|
| `tasking` | ATOC (with its FIDO) | air army KP | national CP | — |
| `support_centre` | ASOC per corps | division aviation control element | — | — |
| `radar_control` | CRC, CRP | GCI post (channel limit) | GCI post | — |
| `airborne_warning` | E-3A orbit (`docs/air_war/11_SUPPORT.md`) | none in the 1985 war: the A-50's service date is at the edge and not used for 1985 control (`air_combat.md` §5; `07_AIR_TO_AIR.md` §2.7); a later war's option by `from_year` | — | — |
| `fac` | ground or helicopter FAC with a corps' unit | regiment FAC; control and target-identification post | — | — |
| `squadron` | wing operations (alert pools) | regiment KP (readiness) | base | the airstrip |

`StaffRole` gains `CpController` (the duty officer of a `tasking`, `support_centre` or `squadron` post), `GciController`
and `Fac`. `AirStaff::Build` (`officers.hpp:113-154`) appoints them as it does the others: authored where the faction
file names them, otherwise generated from `officer_pool` with the role's trait means. Each has a place on the map, a
net and a fate: a post bombed or overrun loses its officer, and `AirStaff::Replace` appoints a successor as today.

Traits bend each post's decision parameters, never by more than about ±0.2 from the faction's value
(`factions_cases.md` §1, a SAM3D design bound):

| Trait or quirk | Effect on a C2 decision |
|---|---|
| rigidity, `by_the_book` | longer decision time; less likely to divert off the plan even where authority allows |
| boldness | more likely to commit alert aircraft into a defended area |
| methodical | waits for a second report before acting; fewer wrong targets |
| scepticism, intelligence skill | believes a single pilot's report less |
| composure | decision time grows less with the number of messages waiting |
| vindictiveness, `grudge_holder` | more likely to commit against the unit credited with his losses |
| `loss_shy` | will not send alert aircraft into a believed SAM zone without SEAD |
| planning skill | the noise in the on-call package's estimate (`utility.noise_at_zero_skill`, as today) |
| `loose_lips` | more and longer transmissions on his nets (`09_COMMS_AND_INTERCEPT.md`'s intercept) |

A pilot's own state (experience, currency, nerve) moves his flight's choices by about ±0.15; that is
`02_FLIGHTS_AND_PILOTS.md` §4.3's, with the weights of `10_FACTIONS.md` §8.

### 4.2 Authority by mission type, as data

Authority is a table in the faction's `"c2"` block, read by one function, `C2Authority::May(post kind, act, mission,
where, year)`. Each entry names a post kind, an act (`warn`, `designate`, `approve`, `divert_airborne`,
`redirect_before_ip`, `divert_recovery`, `scramble`, `recall`), the mission kinds it covers, where it applies
(`own_corps`, `within_allocation`, `own_area`, `free_hunt_zone`, `any`), whose agreement it needs, and `from_year`.
Every entry carries `source` and `confidence`. A sketch of NATO's:

```json
"c2": {
  "authority": [
    {"post": "fac", "act": "designate", "missions": ["cas"], "where": "own_corps",
     "source": "tradoc_jaat_1983; skattum_1988_oa10", "confidence": "high"},
    {"post": "support_centre", "act": "divert_airborne", "missions": ["cas"], "where": "own_corps",
     "needs": "army", "source": "morrow_1977_tacs (TACR 55-46 TEST 1976); hoak_1987_retasking", "confidence": "medium"},
    {"post": "support_centre", "act": "scramble", "missions": ["cas"], "where": "own_corps",
     "source": "hoak_1987_retasking; tradoc_jaat_1983", "confidence": "medium"},
    {"post": "tasking", "act": "divert_airborne", "missions": ["bai"], "where": "any",
     "source": "rippe_1985_airland (J-SAK 1984)", "confidence": "high"},
    {"post": "tasking", "act": "redirect_before_ip", "missions": ["strike"], "where": "any", "chance": 0.1,
     "source": "schoeck_1987_ai_retasking; chance a game value", "confidence": "low"},
    {"post": "tasking", "act": "divert_recovery", "missions": ["any"], "where": "any",
     "source": "afmag_1983_usafe; schoeck_1987_ai_retasking", "confidence": "medium"},
    {"post": "radar_control", "act": "warn", "missions": ["any"], "where": "any",
     "source": "smart_1985_tacs", "confidence": "high"}
  ]
}
```

`MissionKind` has no BAI: a `Strike` whose target lies within `c2.bai_depth_m` of the front's `line`
(`FrontDef::line`) counts as BAI, beyond as AI (a game value; the RIPL's depth is not in the research). The Weasel's
authority is not an entry: a `Hunter` flight's leader may attack what his team finds by the flight leader's room
(`02_FLIGHTS_AND_PILOTS.md` §1.2) and `06_OPPORTUNITY.md`'s chain, and no
C2 act lets him divert another flight. The Pact's entries give `designate` and route changes to `fac`, `approve` within
the allocation to `support_centre`, `scramble` to `tasking` and `squadron`, `divert_airborne` of `ArmedRecce` in its own
area to `tasking` with a low chance ("occasionally"), and free hunt only in `free_hunt_zone`s, opened once the faction's
phase holds the air (`fm_100_2_1_1984` p. 12-6). The neutral has `warn`, `scramble` (fighters) and `recall`. The rebels
have none.

`from_year` keeps later practice out of the 1985 war and lets a later war add it (§6). The coarse gates of `10_FACTIONS.md` §7.3
(`hot_phase.authority`: what a flight may attack beyond its task, who may retask it) summarise this table; the loader
checks that the two agree for each faction.

### 4.3 Requests, reports and orders as messages

`C2Message` (new, `air_war/c2.hpp`): id, kind (`Warning`, `InFlightReport`, `Request`, `Approval`, `Refusal`, `Divert`,
`Scramble`, `CheckIn`, `Brief`, `Recall`, `Codeword`, `DebriefReport`), from and to (a post, a flight or a pool), the net,
the time sent, its length in seconds, the language, the precedence, and what it is about: an intel key or a place with
its sigma and **the time it was observed**, so that every position has an age. A message reaches its addressee hop by
hop along the post's chain; each hop has a delay drawn from the faction's `c2.timing` (relay, decide, encode) and the
officer's modifiers, game values set so the composed time falls in the faction's `hot_phase.retask_air` band
(`10_FACTIONS.md` §7.4-7.5). Each transmission is an emission on the faction's net (`09_COMMS_AND_INTERCEPT.md`): a jammed hop is lost and repeated
after `c2.retry_s`, or late; a heard one is intercepted by our listeners by `09_COMMS_AND_INTERCEPT.md`'s rule and reaches the player's
character only through the pipeline. On the air each message is one of `09_COMMS_AND_INTERCEPT.md` §2.2's call kinds:
`Warning` a `threat_call`, `InFlightReport` a `report`, `Request` a `request`, `Divert`, `Brief` and `Recall` a `divert`,
`Approval` and `Refusal` a short `request` reply, `CheckIn` a `check_in`, `Codeword` a `codeword`; a `Scramble` goes by
land line or the command radio to the pool, and a `DebriefReport` is not a transmission.

Silence counts as consent where the faction's data says so: NATO's CAS request stands unless the corps refuses within
`c2.silence_consent_s` (300 s, a game value from the early Vietnam figure; the 1976 text gives only "a preset time
limit"). A refusal is a message too.

A post decides on a report in this order:

1. **Aircraft already there with leave**: a `Hunter` or free-hunt flight in that area on the same net gets the report
   as a warning; what it does is the flight's.
2. **An alert pool within authority**: commit if the believed value of the target, weighed against the expected loss
   to its believed threat (`Planner::ExpectedLosses`), passes the post's threshold, moved by the traits in §4.1.
3. **Otherwise** the report becomes an `IntelReport` with a new source, `IntelSource::InFlightReport`, arriving in
   minutes, and is used at the next plan; on NATO's side a target in the BAI area can also be nominated, and is flown
   no sooner than `c2.nomination_h` later (48 h, `gallant_knight_1983`).

A report older than the faction's `c2.max_report_age_h` for a mobile target is not struck without a fresh one; it
becomes a `Hunter` search instead (the photo case, 6-8 h, `checo_1967_tactics`, as the evidence of how fast a site
moves). Pilots' claims go into the enemy's records as claimed (`BeliefsFrom`, as today); the truth stays for the
debrief.

`AimOnCall`'s use of the truth goes: close support is aimed where the FAC post believes our troops are, from its own
sighting with its observed time (§4.3), and the FAC's brief is a `Brief` message.

### 4.4 Alert pools: the on-call sorties finally flown

`SquadronState` gains `alert`: a list of `AlertPool`s (state `airborne`, `ground_5`, `ground_15` for NATO;
`readiness_1`, `readiness_2`, `readiness_3` for the Pact; `ground` for the neutral), each with aircraft, weapons, time
to take-off, the hour it began and how long it can be held. The take-off times and holds are read from each faction's
`hot_phase.alert` (`10_FACTIONS.md` §7.4-7.5, the same pool ids); this doc builds the pools from them. The values:

| Pool | To take-off | Held | Source |
|---|---|---|---|
| NATO airborne alert | in the air; A-10 sortie about 2 h, attention flags after 30 min at low level | 2 h | `tradoc_jaat_1983`; `mt_1986_dobedin` |
| NATO ground alert | 5 min, 15 min | game value | `hoak_1987_retasking` |
| Pact readiness 1, 2, 3 | 3-5 min, 15 min, 1-2 h | 1-2 h, 2-4 h, 2-4 days | `fm_100_2_1_1984` p. 12-7 |
| Pact airborne-alert zone (only where allowed) | in the air at 3,000-3,500 m | game value | `markovsky_afghan_sky` |
| Neutral ground alert | 15 min, as NATO's longer ground alert | game value | SAM3D assumption ("assumed to follow ground-alert timings", `c2_retasking.md` c2-40) |

At each plan `Planner::Plan` turns the sorties it holds back today (`tempo × (1 − on_call_fraction)`,
`planner.hpp:297`) into pools by the doctrine's new `alert_mix`, on the squadrons whose types fly the on-call mission
kinds (`Cas`, `Hunter`, `Dead`, `Strike`). The Pact's share grows with its phase (small during the opening air
operation, `dia_1977_fa_support`). A pool not used by the end of its hold stands down and its sorties are spent, as the
held-back sorties are today.

The on-call builder, `Planner::OnCall(const C2Message& request, const AlertPool& pool, Package& out)`, reuses
`BestPackage` restricted to the pool's squadron and to the target the request names (its intel entry, with the
report's sigma and age), `Build` with the entry from the pool's base and a time on target of now + time to take-off +
transit + the checkpoint, and `Commit`. It draws only from `rng_.Fork(request id)`, so the main stream does not move.
`Planner::DoubleTap` (`planner.hpp:1381`) is the precedent: a second package built at once.

### 4.5 The per-package controller in a session

`LivePackages` (`package_injector.hpp:162`) gets a `PackageControl` beside each `MissionBridge`, stepped with it inside
the fixed step (never from an Unreal tick, `map/scale.md` §4.4). It holds the package's link to its posts: it takes the
flight leaders' reports (`02_FLIGHTS_AND_PILOTS.md` §1.4), sends them as messages on the session's EM nets, runs the
posts' decisions for this package by §4.3, and applies what reaches a flight: a `Divert`, a `Recall`, a `Brief` with a
new aim. Applying uses the flight calls of `02_FLIGHTS_AND_PILOTS.md` §2.3: `ScenarioRunner::RerouteFlight(flight,
legs, aim, by)` re-routes a whole flight with new legs, aim and release point, and `OrderFlight` with a `FlightOrder` whose
`by` is the post's id carries a `Recall` (`ReturnHome`) or a hold; the runner refuses a `by` that `C2Authority::May`
does not allow. The trainer's `Instruct` is capped at two held targets (`scenario_runner.hpp:11251-11257`) and stays
as it is. An on-call package launched by a post enters the session with `InjectPackage` at its time, so `LivePackages`
must hold several bridges at once (today one, `package_injector.hpp:156-161`), at the levels of detail of
`02_FLIGHTS_AND_PILOTS.md` §5.

`PackageOutcome` gains `calls` (each message sent: kind, time, net, to whom), `reports` (each in-flight report with its
place, sigma and observed time) and `retasks` (flight, new aim key, time, by which post); the jettisons are
`04_REACTIONS.md` §13's `jettisoned_flights` and the debrief's sightings `03_PERCEPTION.md` §5.1's. `MissionBridge::Finish`
fills them; `CompletePlayerMission` applies them, and `save.hpp` writes them inside the `Complete` action as the rest of
the outcome is today.

### 4.6 Packages in the air in the campaign (A0)

Today a package resolves whole at its take-off hour; nothing is in the air for a post to act on. The resolver is made
time-stepped:

- `Resolver::Begin(Package&)`, `Resolver::StepTo(double h)`, `Resolver::Done()`: the leg walk of `resolve.hpp:134-267`
  done by time along `RoutePoint::t_h`, so a package has a place at every hour. Each step emits `AirEvent`s (a launch
  seen at a place, a loss, a column found, a release) that the abstract posts read as their flights' reports.
- `AirCampaign` keeps an `airborne_` list. `StepTo` begins each package as its first aircraft takes off and steps every
  airborne package to `t1`. The played circle and time are left out as today (`ResolveContext::played_*`).
- **Each airborne package draws from `rng_.Fork(package id)`** and each flight from a fork of that, so interleaving the
  packages in time does not change any one package's draws. This changes every campaign once (the stream no longer
  serves the resolver in sequence): the balance and bench rows are re-run and the new rows recorded
  (`docs/air_war/09_BALANCE.md`), and `save_version` is raised.
- The leader's nerve is judged on the losses so far in time, not in the order of target times. That is more faithful
  and moves results a little; recorded with the rebalance.
- **Sub-hour steps.** `Advance` steps at most `c2.step_h` (1/12 h, a game value) while any package is airborne or any
  message is in transit, and by the hour otherwise. `Deliver` merges reports due by the step's end, not by the next
  hour. `IntelSource::InFlightReport` carries a delay in minutes; `DebriefReport` the faction's after-landing delay (Pact
  5-10 min to the regiment's staff, `markovsky_afghan_sky`; the others `pirep_delay_h` as today).
- **The campaign clock no longer freezes.** A played package no longer blocks `Advance` (`campaign.hpp:183`): it is
  marked played, the campaign advances with the session every `every_s` as `LivePackages` already asks, and other
  packages fly and resolve meanwhile. A package another post diverts into the played circle is handed to the session by
  `InjectPackage` from where it is (the hand-over of `02_FLIGHTS_AND_PILOTS.md` §5.1).

The budget: the nation's theatre flies about 1,100-1,150 sorties a week, and the Pact's first day has 44 packages
(`map/scale.md` §2); a 5-minute step over the packages in the air with a handful of posts is small next to the
planner's cycle. The test in §7 holds the balance tool's run time.

### 4.7 Determinism and saves

- Every C2 draw (a hop's delay, a decision, a lost message) comes from a fork keyed by (package id, message id) or
  (post id, hour), never from the main stream's sequence, never from the fx feed, the frame rate or container order
  (`map/scale.md` §4.2).
- Abstract C2 is a pure function of the campaign's state and seed; a loaded save re-derives it.
- A played package's C2 events return in its `PackageOutcome`. A report or request from the played package that must
  reach the campaign **while the session still runs** (the campaign now advances during it) enters as a new recorded
  action, `CampaignAction::Kind::Report` (the message and the hour), as `Transmit` records our side's calls today. A
  save replays it at the same hour.
- `test_packages_live.cpp:159` (a raid injected into a session flies as the same raid played alone) keeps holding with
  every authority off; with C2 on the test changes on purpose.

### 4.8 What our side hears

Every message in §4.3 is a transmission `09_COMMS_AND_INTERCEPT.md` makes an emission: a NATO CAS check-in, a FAC's brief, a CRC's divert
and a Weasel's call near the front; a Pact KP's command signal and a control post's coded message; little or nothing
from a comm-out deep strike or a Pact strike group on the move (`comms_emcon.md` comms-11, -18). Our listeners hear
aircraft far and ground posts behind the horizon hardly at all (comms-08). A call that names a target and a time is the
10-30 minute warning of the period (comms-02), and it reaches the battery only as a fact carried by the pipeline, said
on the regiment's net, and written in the journal.

### 4.9 Data and tuning

- Faction files: `"c2"` with `posts`, `authority`, `alert_mix` (in `doctrine`; the pools' timings are `hot_phase.alert`), `timing` (per hop: relay,
  decide, encode, in seconds, each with `source` and `confidence`), `silence_consent_s`, `max_report_age_h`,
  `nomination_h`, `free_hunt_zones` (Pact). Strict loader (`Keys` in `data.hpp`).
- `data/gameplay/air_war.json`: `c2.step_h`, `c2.retry_s`, `c2.bai_depth_m`, `c2.trait_bound` (0.2), and the game
  values named above; every key in `TuningTable::Defaults`.
- `docs/SOURCES.md`: the ids this doc uses that are not yet registered (`hoak_1987_retasking`,
  `schoeck_1987_ai_retasking`, `gallant_knight_1983`, `morrow_1977_tacs`, `luke_2002_cas` as hindsight, `rippe_1985_airland`,
  `rifenburg_1989_c2`, `smart_1985_tacs`, `skattum_1988_oa10`, `hoover_1979_fac`, `tradoc_jaat_1983`, `mt_1962_kontsevoy`,
  `mt_1962_front_aviation`, `mt_1986_dobedin`, `markovsky_afghan_sky`, `dia_1977_fa_support`, `afmag_1983_usafe`,
  `lambeth_1984`, `army_it0654_inflightrep`, `streb_1980_frontal_aviation`, `miller_1980_tacs_jamming`,
  `neitzel_1985_weasel`, `johnson_1986`) are registered with their tiers before any value uses them
  (`tools/check_citations.py`).

## 5. Our side

Our fighters and our ground control are `08_OUR_FIGHTERS.md`'s. The same message model serves them: our GCI's vectors and the
notices down the pipeline are messages with delays, on nets the enemy can hear.

## 6. Left out as later than 1985

Kept out of the 1985 war's data, available to a later war only through `from_year`:

- data-linked retasking of strike aircraft (JTIDS/Link 16 in fighters: 1990s; not fielded in 1980,
  `miller_1980_tacs_jamming`);
- kill boxes and "killer scouts" (1991; "kill box" is not a 1980 or 1984 term, `opportunity.md` opp-nato-03);
- push CAS from an ABCCC (1991);
- JSTARS-cued strikes (first used 1991);
- AWACS control of strike packages (no period source shows it for NATO's E-3A; Israel's E-2C over the Bekaa in 1982 and
  Rivet Top's "direction of the strike force" in 1967 are other wars' precedents, `c2_retasking.md` c2-13, c2-16);
- the OA-10A as a FAC aircraft (1987-88), automated TACC and ASOC decision aids (from 1987), Modular Control Equipment
  (mid-1988), Soviet flying command posts over large operations (summer 1986, `markovsky_afghan_sky`);
- the 1997 brevity list; in 1985 calls are plain language with callsigns, and the JAAT glossary's few words (Bandit,
  Bingo, Blind, No Joy, Tally-ho, Visual, Winchester: `tradoc_jaat_1983`).

## 7. Done when

- With the 1985 data, `C2Authority::May` never allows a NATO `divert_airborne` of a strike beyond `c2.bai_depth_m`, any
  act by an `airborne_warning` post other than `warn` and fighter control, any act for the neutral other than `warn`,
  `scramble` of fighters and `recall`, or any act for the rebels; a test lists every authority entry and fails on one
  with `from_year` after 1985 that is active.
- A Pact test: a launch report heard by a division's aviation control element within its allocation produces an
  on-call package from readiness 1 that takes off 3-5 min after the approval and is over the reported place 17-30 min
  after the report; the same report outside any post's radio cover produces only a `DebriefReport` 5-10 min after
  landing.
- A NATO test: a CAS divert waits for the corps' agreement or `silence_consent_s`; a BAI divert does not; a Weasel team
  attacks a site reported inside its package and no strike flight is diverted to it.
- A jammed hop loses or delays the message by `c2.retry_s`; an intercepted one becomes a fact at our listening post
  (`09_COMMS_AND_INTERCEPT.md`'s test).
- Over a campaign week on the nation's theatre, the sorties flown from alert pools are more than zero and no more than
  `on_call_fraction` of the sorties; unused pools stand down; the Pact's share is smaller in its first phase.
- Every report carries its observed time; a mobile target reported more than `max_report_age_h` ago is not struck
  without a fresh report.
- `AimOnCall` no longer reads our troops' true place (a test moves the troops after the FAC's sighting and the aim
  stays at the sighting).
- The stepped resolver with every authority off gives the same outcome as the atomic one for a one-flight package with
  the same forks; `air_war_balance` and `air_war_bench` are re-run and their new rows recorded in `docs/air_war/09_BALANCE.md`
  within its stated bands; the balance tool's run time grows by no more than a quarter (a SAM3D budget).
- A package elsewhere resolves while a played package is in the session (its journal line falls between the played
  package's take-off and its `Complete`).
- A seed replays exactly with C2 on (`test_air_war.cpp:139`'s pattern); a save taken after a played package that sent
  a `Report` loads to the same campaign (`:943`'s pattern); `test_packages_live.cpp:159` holds with authority off.
- `tools/check_citations.py` passes with every new source registered; no value without `source` and `confidence`.

## 8. Left open

- The silence-is-consent window for 1985: no number after the early Vietnam 5 min (`c2_retasking.md` §7).
- The NATO E-3A's wartime role in 1985 in a primary source; whether an ABCCC served the Central Region.
- How often the ATOC redirected an airborne interdiction mission before its IP in 1985 (Schoeck writes in 1987 of what
  the FIDO "must decide"); the `redirect_before_ip` chance is a game value.
- The share of sorties retasked in flight, 1980-85: no number found.
- The depth of NATO's BAI area (the RIPL) as data; `c2.bai_depth_m` is a game value.
- British and German CAS latency in the Central Region (Harrier, Alpha Jet); when the OV-10s left Sembach (about 1984,
  a search snippet only).
- A Soviet period manual on the ПУА, ГБУ and the авианаводчик; the 2007 Krasnaya Zvezda article in full; how often an
  armed reconnaissance aircraft was diverted ("occasionally").
- The neutral's C2 (FLORIDA, Goldhaube) and its alert timings; the rebels' radio to their own troops.
- Whether NATO's hunter-killer teams in Europe in 1985 passed sites by voice, and whether their killers were F-4Es
  (both unverified).
- HAVE QUICK's fielding in USAFE by 1985 (`09_COMMS_AND_INTERCEPT.md`), which decides how well our jammers break NATO's diverts.
- Johnson (1986) and Red Baron, not re-read: the Pact's "no target change in the air" rests on them and on FM 100-2-1's
  direction only.
