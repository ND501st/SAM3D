# 10. Each enemy in the air

Written 15 November 2026. How each of the four adversaries, and our side, fights once the raid is flying, and the
numbers that make it so. Code: `simcore/include/simcore/air_war/data.hpp` (`FactionDef`, `DoctrineDef`, `PhaseDef`,
`PackageTemplate`, `QuirkDef`, `KnownQuirkEffects`, `LoadFactionFile`), `officers.hpp` (`Officer`, `Traits`),
`planner.hpp` (profile choice, apportionment), `resolve.hpp` (the leader's nerve), a new `hot_phase.hpp`; data:
`data/air_war/faction_*.json`, `data/air_war/traits.json`, our side's air file. Research:
`docs/research/air_battle_2026-11/factions_cases.md` (main), with `c2_retasking.md`, `reactions.md`, `opportunity.md`,
`comms_emcon.md`, `air_combat.md`, `aam_weapons.md`, `routing.md`, `perception.md`, `our_side_air.md`; code maps
`map/c2_people.md` and `map/air_to_air.md`. The war's year is 1985: nothing here is later practice.

## 1. What this doc settles

Every other doc in this set builds a mechanism: the flight and its leader, perception, reactions, retasking through
ground nodes, targets of opportunity, the radio, air combat. Each mechanism has choices in it (act or report, press on
or turn back, talk or keep silent), and this doc sets **how each faction makes those choices**, from the period's
evidence, as data.

Three rules hold throughout (`factions_cases.md` §1):

1. **Faction first, then person.** The faction's value is its doctrine and training. An officer's traits move it by at
   most about ±0.2, a pilot's experience, currency and nerve by at most about ±0.15. A bold, creative Pact flight leader
   is still a Pact flight leader: he may take the free hunt he was allowed; he does not invent a new target in the air.
   Johnson found no example of one (`johnson_1986`, unverified: not fetched in the check).
2. **Authority is a gate, not a number.** What a flight may do in the air (attack outside its task, take a new target,
   leave its route) is doctrine, per faction and mission kind (§7.3). The numbers only choose among the acts allowed.
3. **Every number in §7 is a SAM3D estimate.** No source gives "initiative 0.25". The sources give the order between the
   factions and the rough spread, and each value carries the bound they set and the ids that set it.

## 2. The Warsaw Pact: the plan holds

**Before take-off everything is fixed.** The flight plan, the route through friendly air defence and the air-ground
coordination are set before launch, and US Army analysts judged that this "often inhibit[s] flexibility" once missions
are under way (`fm_100_2_1_1984` ch. 12, pp. 12-3, 12-4; the adversary's view, about close support). "Aircraft follow
the original flight plan through friendly antiaircraft defenses unless changed by ground control" (p. 12-4). The target
is confirmed with the forward air controller at a checkpoint; then the flight leader assigns targets in the flight
(c2-21). Approach at 50-100 m, 3-6 s to identify the target, clear within 10 s, radio "reduced to a minimum or prohibited
entirely", re-attack from another direction or out of the sun (p. 12-8).

**Initiative was urged, then curbed.** Johnson's count of *Aviatsiia i kosmonavtika* shows the emphasis on initiative
rising from the mid-1970s and falling back in 1983-84; critics wrote of "fettered initiative" and pilots flying "the
same patterns over and over again"; a cadet was grounded in 1983 for two rolls more than the plan
(`johnson_1986`, unverified: the checker could not fetch it). FM 100-2-1 independently gives "probably
little tactical flexibility" below regiment, which supports the direction. *Soviet Military Power 1983* said initiative
was "now stressed" (`lambeth_1984`); the research uses the lower reading for 1985 (§4.1 there). In 1984 the PVO
moved initiative "mainly to the flight leaders" at contact, the controller still leading the search (`pennington_1985`,
in `air_combat.md` wp-04).

**Two latencies.** The same centralisation makes a new tasking from the ground fast and a change to an airborne flight
slow (`factions_cases.md` §4.3):

| | Pact practice | Source |
|---|---|---|
| New task from alert | readiness 1: crews in the aircraft, 3-5 min to take-off, held 1-2 h; readiness 2: crews near the aircraft, 15 min, held 2-4 h; readiness 3: stores not loaded, 1-2 h, held 2-4 days (fighter-bombers; "probably also applicable" to other ground attack) | `fm_100_2_1_1984` p. 12-7 |
| Request to strike from airfield alert | 20-30 min; 17-28 min in a 1961 exercise (background, out of period) | `mt_1962_kontsevoy` |
| Divert an airborne flight | only by ground control (GCI, the control and target-identification post, the FAC at the checkpoint); an armed reconnaissance aircraft "occasionally" diverted to a request in its area | `fm_100_2_1_1984` pp. 12-4, 12-5 |
| Free hunt | held back until air superiority is won; armed reconnaissance as 1 reconnaissance + 2-4 attack aircraft | p. 12-6 |
| Fighters | most missions under positive ground control, aborted if contact is lost (Skinner, a Western view) | `lambeth_1984` |

**Afghanistan: adapting between missions, not in the air.** The Su-17s flew a set sequence: a target-designation group
of the most experienced pilots searching 3-5 min at 800-1000 m, a suppression group of 2-6 aircraft diving from
2000-2500 m 1-2 min later, the main group 1-2 min after that; pilots reported to the regiment's staff within 5-10 min of
landing (`markovsky_afghan_sky`). A Su-17M3 lost on its sixth pass near Kandahar in summer 1983 was blamed on stereotyped
attacks («шаблонность атак»). An order of 1981 forbade attacks on unidentified targets. In summer 1984 a Su-17 leader
refused the FAC's help and bombed his own battalion (4 killed, 9 wounded). The changes came in the next sequence on the
ground. In SAM3D this is the existing after-package loop (`ApplyOutcome` → `Lessons`, tactic memory by `LearningRate`),
not a decision in flight.

**The clients are the lower bound.** Syria, 9-10 June 1982: jammed off its GCI, its pilots broke into pairs and singles
and flew figure-eights (a Western attaché's account); some ejected the moment they knew they had been seen (a footnote,
attribution unchecked); 85 lost by the end of July, none of Israel's aircraft to enemy fighters (`lambeth_1984`,
from press and IDF accounts). Iraq, 1980-87: only the leader knew the target, pilots kept to rigid profiles for fear of
punishment, piled on the easiest targets, and in the early years bombed from high altitude at high speed
(`cia_1987_iraq_af`, the CIA's estimate). In SAM3D the lower bound is the tail of the Pact's pilot draw (a green or
ill-led flight behaves so) and our side's fighters under jamming (§6).

## 3. NATO: decentralised execution at low level

**Trained for it.** Red Flag from November 1975, to give pilots their "first ten combat sorties" (`vangilder_1979`);
under 25% of TAC aircrew had combat time in August 1978 (same). RAF Tornado crews trained at 250 ft daily and 100 ft at
Goose Bay, Buccaneers worked up to 100 ft for Red Flag in 1977 (`rafhs_j20_1999`, a 1999 retrospective). Red Flag
packages of 60-70 aircraft went through corridors in 20-30 min to a FRAG received the afternoon before, "no radio calls
were required" (same, Pitchfork). The Tactical Leadership Programme trained multinational package leaders from January
1978 (Fürstenfeldbruck), with a flying phase at Jever from September 1979, 8 four-week courses a year with 15 sorties per
crew by 1983 (`afmag_1983_usafe`, `tlp_official_history`). A Soviet writer gave the US "survival corridor" as 60-90 m
(`mt_1984_babich`).

**Who changes what in the air** (`c2_retasking.md` §1.1). The flight leader acts within his frag: attacks, takes the
frag's alternate, aborts, jettisons, evades, and may choose his route round a newly seen threat, his attack mode,
direction and release height (`fm_100_42_1976`, `rafhs_j20_1999`, in `routing.md` §1.7). He does not take a new target
outside his frag. A ground FAC briefs and clears flights handed to him; there were no airborne FAC squadrons left in
Europe (`skattum_1988_oa10`). The ASOC fills immediate requests from airborne and ground alert (5 and 15 min, named by an
ASOC planner in 1987: `hoak_1987_retasking`) and from preplanned CAS it may divert. The ATOC may divert BAI (under
J-SAK, 1984, informing the Army: `rippe_1985_airland`). Interdiction and deep strike were "hardly" retasked in the air
(`schoeck_1987_ai_retasking`, 1987: "someday airborne diverts ... will be possible"). After a diversion to another base
(cross-servicing from 1978) the ATOC fed the aircraft's next target to the receiving base, after landing
(`afmag_1983_usafe`). A Wild Weasel lead commands only his own team; that his team passed sites by voice is an inference
for 1985 (c2-15, unverified). The E-3A (12 of 18 by late 1984: `rippe_1985_airland`) warns and controls fighters; no
period source shows it retasking strikers (`factions_cases.md` §5).

**Talk where controlled, silent elsewhere.** Deep ingress flew silent; packages near the front, warned by an E-3A and
controlled by a CRC or a FAC, talked (`comms_emcon.md` §1.4). NATO's air voice was AM in 225-400 MHz; HAVE QUICK was introduced in 1980, its
fielding in USAFE by 1985 not found (comms-09): a per-unit flag.

**Air combat.** Pairs in line abreast, engaged and free, "left, low, lead" (`anderegg_2001`); visual identification
bound the BVR shot in most cases (`lambeth_1985`, `gao_1985_id`). The Bekaa is the period's model of a superior air
force: 17 of 19 SA-6 batteries in a strike that "reportedly took only 10 minutes", and Lambeth's lesson was initiative,
leadership and adaptability more than equipment (`lambeth_1984`). A superior force still errs when it re-plans a
SEAD strike in hours on stale intelligence (Dugman 5, 1973: `wiki_model5`, a pointer only, unverified, never data).

## 4. The neutral: a small force that husbands its aircraft

No neutral air force fought in 1980-85, and no period source on a neutral's retasking, radio or air combat was found
(c2-40, comms-24, opp-neu-03). The faction is built from analogues for the pattern only:

- **Shape.** Austria's light jets and helicopters, Goldhaube radar control activated 1970-83 (`goldhaube_refs`), a
  guided-missile ban that left its interceptors with guns (`austrian_af_wiki`, a Wikipedia pointer; the ban unverified
  from a citable source: aam-neu-01). Sweden's STRIL 60 shows a neutral could control fighters closely
  (`swe_stril_c2`). So: centralised, ground-controlled, no in-air retasking beyond a recall (c2-40, a SAM3D assumption).
- **Initiative inside the sortie.** Crippa's lone MB-339 at San Carlos, sent to count ships and allowed to attack, chose
  HMS Argonaut himself and noted the ships on his kneepad (`ellitoral_crippa`, tier 4). Argentina's pilots pressed on at
  extreme range "with determination and bravery" and could not afford to manoeuvre over the target
  (`usn_falklands_1983`).
- **No warning receivers.** Argentina's attackers flew "unprotected" (`twz_2025_argentine_chaff`); a launch is seen or it
  is not. Against Sea Harriers A-4s jettisoned tanks and climbed away (`fundacion_malvinas_24may`).
- **Husbanding.** A small force without replacements cuts its operations as the threat rises: the SAAF's F1 sorties fell
  once Cuban MiG-23MLs appeared in 1987 (`angola_saaf_1987`, after the war's year, pattern only). The faction file already
  says "every aircraft is irreplaceable" (loss tolerance one a day).

So the neutral's leader has room to choose within his sortie (initiative 0.6), its HQ cannot retask him, and its will to
press on falls with every aircraft lost.

## 5. The rebels: the most initiative, the least coordination

- Biafra's five MFI-9Bs raided at tree-top height (`biafra_minicoin`); the strikes were planned on known targets, below
  300 ft, one run; but some pilots, "caught up in the adrenaline game of shooting rockets", fired at empty ground, and two
  aircraft with their pilots were lost in such shoot-ups (`garrison_biafra`, one veteran's account, low confidence).
- Katanga's single Fouga held the UN at Jadotville until Canberras came (`katanga_fouga`).
- No net, no plan beyond take-off, no warning receiver, no retasking (c2-41, reactions-48, SAM3D assumptions). Calls in
  plain language, on civil or captured sets, in our language (comms-25, an inference): our posts understand every word.
- No fighters: against ours a rebel can only hide low, run or land (`air_combat.md` reb-01).
- Croatia 1991, the faction file's model, is after the period and gives only the shape (`croatia_1991_migflug`).

## 6. Our side: fighters bound to the ground

Our fighters follow Pact practice: brought in by the sector's GCI, or by the Lazur command link on the MiG-23ML
(`airwar_ru`), the pilot's own initiative beginning at contact (`pennington_1985`). Lose the control post, or have the
voice nets jammed, and they fall back to independent search in pairs or abort; the failure case is Syria 1982
(`lambeth_1984`; reactions-50: some popped up to reorient, some zoomed into the CAP, confused pilots "simply
leveled off and headed straight back"). Pilots trained to keep radars off until close for surprise (`pennington_1985`).
Our worst problem is our own SAMs: up to 25% of the targets "fired upon" in a Baltic MD exercise were our own aircraft
(`mt_turchin_coop`). Our side's profile is in §7 so that `08_OUR_FIGHTERS.md` reads the same block; its sharing of the
sky, notices and IFF are that doc's, not this one's.

## 7. The hot-phase parameters

### 7.1 What each one decides

| Key | 0..1 unless stated | Read when |
|---|---|---|
| `initiative` | chance the flight leader acts on his own, within his authority, on a cue the plan does not cover (re-route round a newly seen site, take the frag's alternate, take a target in his free-hunt zone), instead of holding the plan and reporting | each unplanned cue (sighting, warning, loss of control) |
| `plan_adherence` | how closely route and timing are kept: the share of a disruption absorbed by the plan; the gate time tolerance shrinks with it | each leg; at gates and the IP |
| `press_on` | will to continue to the target under fire; sets the share of the package lost before the leader turns back | after each loss or damage |
| `jettison_readiness` | chance a threatened flight jettisons tanks, then stores | fighter attack, a launch seen at the flight |
| `radio_discipline` | chance an optional call is not made (chatter, a warning a disciplined flight would hold); required calls (check-in, GCI) are always made | each optional call |
| `emission_discipline` | own radars and jammers off until needed | each sensor decision |
| `air_combat_skill` | the mean quality the air-combat chain starts from (envelope, sort, shot discipline) | each engagement |
| `opportunity` | chance a flight that is allowed and able to attack a seen target does so, rather than report it and go on | each target of opportunity identified |
| `turn_back` | chance a flight turns back when it knows it is spotted, or is jammed off its control | on being spotted or losing control |
| `reaction_delay_s` | [min, max] from cue to manoeuvre for the faction's trained pilot | each threat cue |
| `husbanding` | how much `press_on` falls as the day's losses approach the faction's `loss_tolerance_per_day` | after each loss |

The shares per situation (a fighter attack, a launch seen, flak) stay in `04_REACTIONS.md`'s table (§8, §12.1);
`jettison_readiness` and `press_on` are the faction's summary of it, and the officer's and pilot's shifts of §8 move that
table's row by the same amount.

### 7.2 Values by faction

All SAM3D estimates bounded by the sources named. A role override is in brackets.

| Key | Rebels | Neutral | Warsaw Pact | NATO | Our side | Basis |
|---|---|---|---|---|---|---|
| `initiative` | 0.8 | 0.6 | 0.25 | 0.7 | 0.25 (flight leader at contact) | `fm_100_2_1_1984`, `johnson_1986`, `lambeth_1984`; `rafhs_j20_1999`, `vangilder_1979`; `ellitoral_crippa`, `biafra_minicoin`; `pennington_1985` |
| `plan_adherence` | 0.3 | 0.6 | 0.9 | 0.7 | 0.9 (to the controller) | as above; `cia_1987_iraq_af` for the rigid end |
| `press_on` | 0.6, wide spread | 0.4 | 0.6 (escort without GCI 0.4) | 0.6 | 0.6 (0.4 without GCI) | `usn_falklands_1983`, `lambeth_1984`, `checo_1969_tactics` |
| `jettison_readiness` | 0.5 | 0.6 (tanks first) | 0.2 | 0.4 (fighters attacking: 0.5) | tanks before combat | `reactions.md` §2.6 (`red_baron_vol2`, `checo_1969_tactics`, `fundacion_malvinas_24may`); rebels a game value |
| `radio_discipline` | 0.3 | 0.6 | 0.8 (strike); fighters talk to GCI | 0.8 deep (comm-out); controlled packages talk | 0.7; GCI by voice or Lazur | `fm_100_2_1_1984`, `rafhs_j20_1999`, `mt_1964_air_army`, comms-25; `wseg_116_1967` (unverified) for the failure |
| `emission_discipline` | 0.2 (few emitters) | 0.5 | 0.6 | 0.7 | 0.6 (radars off until close) | SAM3D estimate; `pennington_1985` |
| `air_combat_skill` | 0.2 | 0.5 (guns only) | 0.5 (clients 0.3) | 0.8 | 0.45 | `lambeth_1984`, `vangilder_1979`, `cia_1987_iraq_af`; ours between Pact and clients, an estimate |
| `opportunity` | 0.7 | 0.4 | 0.2 | 0.5 | 0.2 (independent search only) | `fm_100_2_1_1984`, `markovsky_afghan_sky`, `ellitoral_crippa`, `garrison_biafra`, `mt_turchin_coop` |
| `turn_back` | 0.4 | 0.5 | 0.2 (escort, GCI lost: 0.6) | 0.2 | 0.6 if GCI lost | `lambeth_1984` (Skinner against FM 100-2-1, §4.2 there: split by mission) |
| `reaction_delay_s` | 4-10 | 2-5 from a sighting | 2-5 (the preset programme); improvised manoeuvres +2 | 2-5 | 2-5 under GCI warning; 4-10 jammed | `reactions.md` §2.6 (veteran 1-3, trained 2-5, green 4-10; `sve_prm`) |
| `husbanding` | 0.15 | 0.3 | 0.0 | 0.1 | 0.0 | game values on `angola_saaf_1987` (pattern) and each file's `loss_tolerance_per_day` |

The order is what the sources give and what tests pin (§11): adherence Pact > NATO > neutral > rebels; initiative rebels >
NATO > neutral > Pact; air combat NATO > Pact = neutral > rebels.

### 7.3 Authority: the gates

Gates are lists in data, not numbers. A flight acts only inside them; `initiative` and `opportunity` choose within.

| Faction | May attack beyond its task | Who may retask it in the air | What its leader may change |
|---|---|---|---|
| Pact | armed reconnaissance and hunter flights in their area; free hunt only in given zones and once the air is held (SAM3D reads this as the phase after the air operation: c2-30); every crew reports what it sees (`fm_100_2_1_1984` p. 12-6; "report rather than attack" is a SAM3D inference) | ground control: GCI, the control post, the FAC at the checkpoint; the regiment's KP diverts an armed reconnaissance within its area | targets inside the flight after the FAC confirms; the preset counter-SAM programme (`sve_prm`) |
| NATO | armed reconnaissance beyond the FSCL or in a free fire area (`fm_101_5_1_1980`, US Army terms); others hit back at guns that fire on them (`checo_roe_1973`) | FAC (CAS handed to him); ASOC (CAS divert, scramble); ATOC (BAI divert); never interdiction or deep strike in the air | route round a seen threat, attack mode, direction, release height, the frag's alternate, abort |
| Neutral | its armed reconnaissance pairs near the line | the national control centre: recall only | the attack within the sortie; break off |
| Rebels | anything that looks military | nobody | everything |
| Our side | what the GCI assigns; independent search in the first echelon (`mt_turchin_coop`, 1967-68 frame) | the sector's GCI, continuously | at contact, the attack |

In data, `authority` in the `hot_phase` block (§7.5); `free_hunt_from_phase` counts the doctrine's phases from 0. These
gates summarise `05_C2_AND_RETASKING.md` §4.2's `c2.authority` table (who may do which act, on which missions, where);
the loader checks that the two agree for each faction.

### 7.4 Latencies

| Faction | From the ground (a new tasking flown) | In the air (an airborne flight changed) |
|---|---|---|
| Pact | readiness 1: 180-300 s; 2: 900 s; 3: 1-2 h (`fm_100_2_1_1984` p. 12-7) | 600-1800 s through ground control, "never" 0.5 for planned strikes (SAM3D estimate) |
| NATO | ground alert 300 and 900 s (`hoak_1987_retasking`); otherwise the next ATO, 24 h; BAI 48 h from division (`gallant_knight_1983`) | 180-600 s by a FAC for CAS and BAI (rests on the FAC route alone); interdiction never |
| Neutral | 30-60 min (SAM3D estimate) | recall only, 300-900 s |
| Rebels | hours | none |
| Our side | the scramble table of `08_OUR_FIGHTERS.md` §3 | GCI vectors, seconds |

The ground latencies are the alert pools that `on_call_fraction` (today held back and never flown, `planner.hpp:297`)
becomes; building the pools and the request chain is `05_C2_AND_RETASKING.md`'s work (§4.4). This doc gives each faction's
table: `hot_phase.alert` is the source of the pools' take-off and hold times (the same pool ids as §4.4 there), and
`hot_phase.retask_air` the band that `05_C2_AND_RETASKING.md` §4.3's per-hop `c2.timing` must compose to.

### 7.5 Data and code

Each faction file gets a `hot_phase` block. Every value is an object with its bound, source and confidence, as
`data/systems/` values are:

```json
"hot_phase": {
  "initiative":     { "value": 0.25, "range": [0.1, 0.4], "source": "fm_100_2_1_1984 ch. 12 (pp. 12-3, 12-4); johnson_1986 (unverified); lambeth_1984; pennington_1985 (the flight leader at contact)", "confidence": "estimate" },
  "press_on":       { "value": 0.6, "by_role": { "escort": 0.4 }, "range": [0.4, 0.7], "source": "lambeth_1984; checo_1969_tactics; usn_falklands_1983", "confidence": "estimate" },
  "turn_back":      { "value": 0.2, "by_role": { "escort": 0.6 }, "when_control_lost": true, "range": [0.1, 0.6], "source": "lambeth_1984 (Skinner); fm_100_2_1_1984 p. 12-4", "confidence": "estimate" },
  "reaction_delay_s": { "value": [2, 5], "improvised_extra_s": 2, "source": "reactions.md 2.6; sve_prm", "confidence": "estimate" },
  "alert": [ { "id": "readiness_1", "takeoff_s": [180, 300], "held_h": [1, 2] }, { "id": "readiness_2", "takeoff_s": [900, 900], "held_h": [2, 4] },
             { "id": "readiness_3", "takeoff_s": [3600, 7200], "held_h": [48, 96] } ],
  "retask_air": { "delay_s": [600, 1800], "never": 0.5 },
  "authority": { "beyond_task": [ "armed_recce", "hunter" ], "free_hunt_from_phase": 1, "retask_by": [ "gci", "control_post", "fac_checkpoint" ], "retask_kinds": [ "armed_recce", "cas" ], "recall_only": false },
  "on_control_lost": { "green": "abort", "skilled": "pairs" },
  "source": "factions_cases.md section 1 (design-01: SAM3D estimates ordered by the sources)", "confidence": "estimate"
}
```

`data.hpp`: `enum class HotParam { Initiative, PlanAdherence, PressOn, Jettison, RadioDiscipline, EmissionDiscipline,
AirCombat, Opportunity, TurnBack, Husbanding, Count }`; `struct HotValue { double value, lo, hi; double
by_role[FlightRole::Count]; bool has_role[...]; }`; `struct AlertLevelDef`, `struct RetaskAirDef`, `struct
AuthorityDef`; `struct HotPhaseDef` on `FactionDef::hot`. `LoadFactionFile` reads it with `Keys` (strict: an unknown key
fails, a value outside `range` fails, `range` outside 0..1 fails). `on_control_lost` takes `07_AIR_TO_AIR.md`'s answers (`abort`, `pairs`, `continue`, §4.2 and §5.1 there). The block is
required in all four faction files and in our side's `data/command/own_air.json` (`08_OUR_FIGHTERS.md` §13), changed in
one commit.

`hot_phase.hpp` (new, header-only): `double HotPhase(const FactionDef&, HotParam, FlightRole, const OfficerPoolDef&,
const Officer* leader, const Officer* commander, const PilotState* pilot)`, pure, no random draw: the callers draw from
their keyed streams (decision 6 of `README.md`). The faction value is read once per flight at the hand-over to A2 or A1 and
kept on the flight; stress and losses update it at events, not every step.

**Phase one changes no abstract number.** Only played flights (A2, A1) read the block. The abstract resolver keeps its
nerve (`resolve.hpp:117-122`: 0.25 + 0.3 boldness + the quirks) so `air_war_balance` and `air_war_bench` stay byte for
byte. Phase two, with the stepped resolver (A0), moves the nerve onto `press_on` and `husbanding` and re-runs the balance
(§9.3).

## 8. Officers and pilots move them

### 8.1 The rule

`effective = clamp01(faction + Δofficer + Δpilot)`, with `Δofficer = clamp(Σ w × (trait − pool mean), ±0.2)` and
`Δpilot = clamp(..., ±0.15)`. Traits are centred on the faction's own `officer_pool.trait_mean`, so an average officer of
that faction leaves its doctrine as it is; a Pact officer is judged against Pact officers. Which officer: the squadron
leader for his squadron's flights (full weight when he flies, `Flight::officer`; half when he does not: the habits he
trains), the commander for the alert pools and the ground latencies. Weights are game values.

### 8.2 Traits

| Parameter | Up with | Down with |
|---|---|---|
| `initiative` | creativity 0.3, boldness 0.1 | rigidity 0.3 |
| `plan_adherence`, retask latency (×(1 + Δ)) | rigidity 0.3, methodical 0.2 | creativity 0.2 |
| `press_on` | boldness 0.3, ambition 0.1 | |
| `jettison_readiness` | | composure 0.2, boldness 0.1 |
| `turn_back` | | composure 0.3, boldness 0.1 |
| `opportunity` | boldness 0.2, ambition 0.1; vindictiveness 0.2 against a grudge's target only | methodical 0.1 |
| `radio_discipline`, `emission_discipline` | methodical 0.2 | |
| `air_combat_skill` | airmanship skill 0.4 (through `Effective`, so stress lowers it) | |
| `reaction_delay_s` | | airmanship skill (−20% at 1.0) |
| flight cohesion when the net is jammed | leadership skill (Syria 1982 is the failure case) | |

### 8.3 Pilots

Each pilot below the squadron leader is a person (`02_FLIGHTS_AND_PILOTS.md` §4). Three inputs move the hot-phase values:

| Input | Scale | Moves | Basis |
|---|---|---|---|
| Experience: combat sorties survived | min(1, sorties / 10) | `air_combat_skill` +0.1, `reaction_delay_s` −30%, no-reaction chance (`reactions.md` §2.6: veteran 2%, trained 10%, green 25%), find chance | the first ten sorties (`vangilder_1979`); a pilot's chance of being killed levelling after about ten encounters (`bilikam_1970`) |
| Currency: hours in the last year against the faction's reference | ratio, capped 1.2 | `air_combat_skill` ±0.05, `plan_adherence` (timing) ±0.05 | NATO guide 15 h a month (`hansard_1981_nato_hours`), TAC 16.3-18.3 h a month 1980-85 (`gao_1986_flying_hours`); Soviet 125-160 h a year (`cia_1969_soviet_tac_training`, 1969, the only figure; 1980-85 open); neutral and rebels game values |
| Nerve, lowered by stress | 0..1 | `press_on` ±0.1, `turn_back` ±0.1, `jettison_readiness` ±0.1; at the bottom, a rare ejection when spotted | 47 jettisons under MiG attack in September 1967 (`checo_1969_tactics`); Syria 1982 (`lambeth_1984`) |

Starting draws per faction are game values: NATO pilots start with few combat sorties (Red Flag counts as training, not
sorties), Pact pools include some Afghanistan veterans (the pool's backgrounds already say so), rebels start green.

### 8.4 Existing quirks read in the hot phase

| Quirk (`traits.json`) | Effect key | In the air |
|---|---|---|
| `loss_shy` / `press_on` | `abort_threshold` | `press_on` ±0.1, `turn_back` ∓0.1, `jettison_readiness` ±0.05 |
| `gambler` | `risk` | `press_on` +0.05 |
| `by_the_book` | `doctrine_weight` | `plan_adherence` +0.1, `initiative` −0.1 |
| `punctual` / `ragged_timing` | `punctuality` | gate and TOT tolerance |
| `chases_kills` | `restrike` | a second pass on the target (from a new heading; it costs: `06_OPPORTUNITY.md` §5.4) |
| `grudge_holder` | `grudge` | `opportunity` against the defence that hurt his men |
| `hoards_arms` | `arm_reserve` | an ARM carrier keeps its rounds for the planned emitter |
| `low_level_purist` / `medium_altitude_man` | `low_level` / `medium_altitude` | holds or leaves low level when guns are seen |
| `flies_himself` | `flies` | the officer's traits at full weight in his flight |
| `loose_lips` | `comsec_leak` | before the raid, the staff's `opsec` (`09_COMMS_AND_INTERCEPT.md` §5.3) in place of today's fixed 900 s warning; in the air, `radio_discipline` −0.2 on his nets (`09_COMMS_AND_INTERCEPT.md` §3.2, `05_C2_AND_RETASKING.md` §4.1) |

### 8.5 New quirks

New effect keys in `KnownQuirkEffects`: `opportunity`, `radio_discipline`, `control_bound`. Signatures are what our crews
or intercept can see; each reaches the player only as our analysts' phrase.

| Id | Effect | Signature | Tier weights (inferior, near_peer, peer, superior) | Anchor |
|---|---|---|---|---|
| `opportunist` | `opportunity` +1 (×0.1) | "Flights on other tasks shoot up what they pass." | 2.0, 0.5, 0.3, 1.0; roles squadron_leader, commander | Biafra's shoot-ups (`garrison_biafra`); a Pact one still only inside its gate |
| `chatterbox`, weakness | `radio_discipline` −1 (×0.1) | "His flights talk all the way in." | 2.0, 1.0, 0.5, 0.3 | the failure mode of `wseg_116_1967` (unverified) |
| `silent_runner` | `radio_discipline` +1 (×0.1) | "His raids come in without a word on the radio." | 0.3, 0.7, 1.2, 1.2 | `rafhs_j20_1999`, `mt_1964_air_army`, `checo_1969_tactics` |
| `controller_bound`, weakness | `control_bound` +1: `initiative` −0.1, `turn_back` on control lost +0.15 | "His fighters turn for home when their controller goes quiet." | 0.0, 0.5, 1.5, 0.1; roles squadron_leader | Skinner in `lambeth_1984`; pilots rebuked for waiting for GCI (`pennington_1985`) |

## 9. Two current data choices that are not 1985 practice

These were proposals; both were taken in B11 (§12, `docs/air_war/09_BALANCE.md` §16).

### 9.1 `medium_altitude_man` for NATO

**Why.** NATO's attack aircraft in the Central Region trained and planned to fly low (`rafhs_j20_1999`,
`mt_1984_babich`). Medium altitude was a period choice only over broken defences (Linebacker 1972, the Bekaa 1982); the
switch to it after low-level losses is the Gulf War of 1991 (`factions_cases.md` §2, §5).

**Change.**
1. `traits.json`: `medium_altitude_man` tier weight at superior 1.3 → 0.1; its description gains "once the defences are
   broken".
2. `faction_nato.json`: Halloran's `medium_altitude_man` → `low_level_purist` (`by_the_book` is the research's other
   choice; it would also stiffen his apportionment, `planner.hpp:308`, and slow the rollback, so not that one).
   `docs/air_war/02_AIR_STAFF.md` §5: "suppression first, above the flak" → "suppression first, low level".
3. Medium altitude by phase: a new `PackageTemplate` key `medium_below_iads` (default −1, no gate). NATO's `strike`,
   `cas` and `armed_recce` get 0.45, the rollback's own line: the planner offers `Profile::Medium` for them only once
   `BelievedIadsShare()` is below it. `Hunter`, `Dead`, `Sead`, `Escort`, `Jammer` and `Decoy` flights keep their profiles as now (Wild
   Weasels flew high enough to see the missiles: reactions-04, 1972).

**What it touches.** Today the profile bonus reads only the planner's quirks (`planner.hpp:1134-1135`: `pln.Quirk`), so
Halloran's quirk changes no number now; step 2 alone is a text change. Step 3 is the one with a cost.

**Balance cost** (`09_BALANCE.md`). While the defences stand, NATO strike and close support packages fly low: into the
target guns (`attrition.target_guns_troops` 0.04, `target_guns_site` 0.015 per aircraft attacking low, §4.7 there) and
the SAMs' low cover; the weaponeer will pick more toss and stand-off deliveries (`delivery_risk` 0.4). Expect NATO's
rollback-phase loss % to rise, by an amount only a run shows. Runs: `air_war_balance data 20 7 nato`,
`air_war_balance --theatre theatre_pannonia_nation.json data 10 7 nato`, `air_war_bench data 400` (its analogues set
their own profiles and should not move). Accept when NATO keeps the lowest loss rate of the four and `09_BALANCE.md` §2's shape holds
(losses falling as the rollback bites); if NATO's day 1 exceeds the Pact's, look at `delivery_risk` before the gate.

### 9.2 NATO's rollback phase

**Why.** A rollback "until fewer than 45% work" before interdiction is closer to 1991 than to 1985. The period precedent
is the Bekaa: suppress, then go on, in the same strike (`lambeth_1984`); JSEAD doctrine existed by 1982
(`jsead_1982`). That NATO in 1985 would have flown counter-air and interdiction from the first day because the land
battle would not wait is a SAM3D inference (`factions_cases.md` §1, unverified: `afmag_1983_usafe` gives only the 15
minutes between the forward elements).

**Change.** Keep the phase and its `until_iads` 0.45 (its end and reopening are pinned by `test_air_war.cpp:574`); rename
it "counter-air and suppression, with interdiction from the first day"; move its apportionment toward interdiction and
support at a reduced share:

| Mission | dead | hunter | deception | recce | strike | bda | cas | missile | armed_recce |
|---|---|---|---|---|---|---|---|---|---|
| Now | 0.25 | 0.15 | 0.10 | 0.15 | 0.22 | 0.05 | 0.05 | 0.06 | 0.03 |
| Proposed (game values) | 0.22 | 0.12 | 0.08 | 0.11 | 0.25 | 0.03 | 0.08 | 0.05 | 0.06 |

The SEAD kinds (dead, hunter, deception) fall from 0.50 to 0.42 before the commander's bend for method and believed
defences (`planner.hpp`, apportionment step 1). Missile stays: a 1985 war has no type for it (`MissileType()`).

**Balance cost** (`09_BALANCE.md`). On the tuned theatre the rollback ends later (the radar SAM sites are at 21-33% by
day 4 now, §5.3); on the nation's it already holds all week (§8.4), so there the cost is mostly earlier strikes on bridges and junctions:
the Kesmark bridges (battery 21's task) are raided in week 1 and NATO offers the battery more raids. NATO's losses rise a
little on days 1-2 (strikes into defences not yet suppressed). Runs as in 9.1. Accept when NATO's day-1 SEAD share is
still the highest of its week, the rollback still ends and resumes in `test_air_war.cpp:574`'s campaigns, and §12's
bridge line-hours cut rise in week 1.

### 9.3 Phase two of the parameters

When the stepped resolver lands, the nerve becomes `0.1 + 0.5 × press_on_eff` less `husbanding` × the day's losses over
tolerance (equal to today's for a NATO or Pact leader of average boldness; lower for the neutral). The four factions are
re-run as in 9.1; the neutral's and rebels' week-1 losses are the numbers to watch (`09_BALANCE.md` §5, items 4-5).

## 10. What our crews see, and what is written down

None of this reaches the player as a number. It reaches the crews as sights and sounds, and the records as the crews
wrote them (`docs/command/01_PRINCIPLES.md` 9; decision 7 of `README.md`):

| Faction | What the battery sees | What our intercept hears (by rule, through the pipeline) |
|---|---|---|
| Pact | the raid on its minute, at 50-100 m; a strike group that holds course under fire; a second blow later, not a second pass | little from the strikers; the GCI and the controllers talking |
| NATO | low, fast, silent packages; a leader who swings wide of a battery that has fired; a Weasel pair coming back for it | check-ins with a FAC or a CRC near the front; nothing deep |
| Neutral | a raid that breaks off at the first launch; tanks dropped | German (or its language) on a few nets |
| Rebels | a lone aircraft back for a second pass; rockets at empty ground | everything, in our language |
| Our fighters | aircraft passing on a notified route, or not notified | our own GCI |

The enemy's pilots' claims go into the enemy's records as claimed; the truth is kept for the debrief, where
`AirCampaign::EnemyMind` shows each officer's hot-phase shifts beside his traits.

## 11. Tests

In the air war's test area (`simcore/tests/test_hot_phase.cpp`, new):

- the four faction files and our side's load with `hot_phase`; an unknown key, a missing source or a value outside its
  `range` fails the load;
- the orders of §7.2 hold for the files as shipped;
- an officer whose traits equal his pool's means leaves every value unchanged; the most extreme officer moves none by more
  than 0.2, the most extreme pilot by more than 0.15, both together by no more than 0.35;
- a Pact strike flight led by an officer with boldness and creativity 1.0 is never offered an attack outside its gate;
  a Pact armed reconnaissance pair in the air operation phase is offered no free hunt, and in the next phase is;
- latencies: a Pact readiness-1 launch within 180-300 s; a Pact planned strike retasked in the air either never or after
  at least 600 s; a NATO interdiction flight never; a NATO CAS flight through a FAC within 180-600 s; the neutral recall
  only; the rebels none;
- the new effect keys are in `KnownQuirkEffects` and `traits.json` loads;
- the same seed gives the same draws (decision 6);
- 9.1: a NATO strike is offered no medium profile while the believed defences are above 0.45, a hunter still is;
- 9.2: the rollback's apportionment sums to 1 and gives armed reconnaissance on day 1.

`air_war_balance` and `air_war_bench` are unchanged byte for byte by phase one.

## 12. As built (B11, 18 November 2026, branch `tb-b11`)

**The data.** Each of the four faction files and our side's `data/command/own_air.json` has a `hot_phase` block with
all eleven keys of §7.1. Each key has a value, a `range`, a source and a confidence, plus `by_role` and
`when_control_lost` where §7.2 gives them. The values are §7.2's. The loader (`air_war/hot_phase_def.hpp`
`LoadHotPhase`) is strict:
- an unknown key fails the load;
- so does a missing source or confidence;
- so does a value or role value outside its range;
- so does a range outside 0..1.

The block is required. The enemy's blocks also carry `alert`, `retask_air`, `authority` and `on_control_lost`. Our side
has no alert pools, because its readiness block covers that, and no authority, because its fighters' tasks come from the
GCI.

**What moved into it**, and what reads it now:

| Was | Now | Read by |
|---|---|---|
| `opportunity_rules.tendency` (B6) | `hot_phase.opportunity` | the loader writes it back to `opportunity.tendency`; B6's `ActShare` is unchanged |
| `radio.discipline`, `radio.discipline_gci_fighters` (B9) | `hot_phase.radio_discipline`, with `by_role` 0.2 or 0.3 for escort, sweep and CAP | written back; B9's radio is unchanged |
| `c2.alert` (B5) | `hot_phase.alert` | written back to `c2.alert` |
| `c2.free_hunt` (B5, the Pact) | `hot_phase.authority.free_hunt_from_phase`, `free_hunt_depth_km` | written back to `c2.free_hunt` |
| `reactions.json` profiles' `initiative` (B4, read by nothing) | `hot_phase.initiative` | `HotPhase()` |

The old keys are refused in their old places, so a value cannot live in two files.

**What stayed where it was**, and why:
- **B4's reaction rows and B8's `strikers_answer`.** These are the shares per situation, which §7.1 keeps in `04_REACTIONS.md`'s table.
- **B8's doctrine `skill` and `on_control_lost` in `air_combat.json`.** A session reads `air_combat.json` without the faction files (`scenario.hpp`). The hot phase repeats these values, and `LoadAirWarData` refuses a faction whose values differ. The test checks our side's.
- **B5's `c2.timing`.** It is per hop; `retask_air` is the whole delay of §7.4, so it is new.
- **B5's `c2.authority` table.** The gates in `hot_phase.authority` must agree with it on load:
  - `retask_by` must equal the posts holding `divert_airborne` or `designate`;
  - `retask_kinds` must equal their missions;
  - `recall_only` must hold exactly when the posts may recall and do nothing else;
  - a free hunt zone used by `hunter_leave` or the opportunity rules needs `free_hunt_from_phase`.

  Moving the table itself would have been a large rewrite of `c2.hpp` with no gain.
- **B9's quirks `chatterbox` and `silent_runner`.** They keep their `radio_chatter` key and B9's tier weights. §8.5's new `radio_discipline` key and weights would have moved every faction's generated officers.

**The code.**
- `simcore/air_war/hot_phase_def.hpp`: the data.
- `simcore/air_war/hot_phase.hpp`:
  - `HotPhase(faction, param, role, pool, who)` returns the faction's value, plus the officer's shift (§8.2's weights and §8.4-8.5's quirks, at most ±0.2, half weight when he does not fly), plus the pilot's shift (experience, currency, nerve, at most ±0.15).
  - `ReactionDelayS`.
  - `RetaskAirS`, `RecallS` and `AlertTakeoffS` (§7.4).
  - `BeyondTaskOffered` and `FreeHuntOffered` (§7.3). These take no officer, so no officer can open a gate.
  - `HotShiftsLine`, which `AirCampaign::EnemyMind` shows beside each flying officer's traits (§10).

  All of these are pure functions. The callers draw from their own keyed streams; no stream was added.
- When a raid is handed to a played session (`raid_scenario.hpp`), each aircraft's `"reactions"` carries its officer's `press_on_shift` and `jettison_shift`. If that officer does not fly, it is his squadron leader's shift at half weight. `reaction_chain.hpp` moves the row by them (`reactions.hpp` `ShiftedRow`).
- The abstract resolver reads none of it (§7.5, phase one).

**The quirks.** Two are new, `opportunist` and `controller_bound`, with the effect keys `opportunity` and
`control_bound`. They are drawn after the other quirks, each from a stream keyed on the officer (`hot_draw`), with the
chance the main draw would have given them. So an older campaign's officers keep every quirk they had.

**§9.1 and §9.2** were taken as written (results in `docs/air_war/09_BALANCE.md` §16):
- `medium_altitude_man`'s superior weight is 0.1.
- Halloran is a `low_level_purist`.
- NATO's strike, close support and armed reconnaissance templates have `medium_below_iads` 0.45. The planner offers them medium altitude only once the believed working defences are below it, unless a political order to fly high stands.
- NATO's first phase is "counter-air and suppression, with interdiction from the first day", with the shares in §9.2's table.

**Old saves.** A save carries `hot_mode`, and one without it loads with mode 0 (the tuning `hot_phase.mode`). Mode 0
keeps:
- the old shares (the phase's `apportion_mode0`);
- the old weight (the quirk's `tier_weight_mode0`);
- no medium gate;
- no hot quirks.

**Tests.** `tests/test_hot_phase.cpp` covers each item of §11.

**Left:**
- Most values are data that played flights do not read yet. Played flights now read the officer's press-on and jettison shifts, the opportunity, the radio discipline, the alert pools and the free hunt. They do not yet read `initiative`, `plan_adherence`, `emission_discipline`, `turn_back` (with its control-lost role values), `husbanding`, `reaction_delay_s` or `leader_may`. Wiring each one needs the behaviour it governs to exist in the session first: the leader's re-route (B2/B4), timing at gates (B2), our controller lost in a played raid (B8/B10), the stepped resolver (§9.3).
- ~~`retask_air` with no caller~~ **Settled by B14 (18 Nov 2026):** under `balance.mode` 1 B5's divert waits for `RetaskAirS`, so an airborne Pact flight has its new task 10-30 min after the decision, not in the order's 160-490 s of hops (`c2.hpp`, `docs/air_war/09_BALANCE.md` §17.3 item 3). `AlertTakeoffS` and `RecallS` are read by the bench only. The loader does not check the composition.
- The pilot's shift needs pilots with sorties, currency and nerve. The session's nerve is passed; the sorties and currency are B12's.
- Our side's 4-10 s reaction delay when jammed is not modelled. The jammed state does not reach a flight.
- NATO's `redirect_before_ip` line (c2, 10%) is not counted as a retask: `RetaskAirS` says interdiction is never retasked.
- §8.5's tier weights for `chatterbox` and `silent_runner` were not applied (see above).

## Done when

- Every faction file and our side's air file carries `hot_phase` with a value, range, source and confidence per key, and
  `python tools/check_citations.py` passes with every new id registered in `docs/SOURCES.md` (Lambeth R-3000 and
  Johnson 1986 are registered once, as `lambeth_1984` and `johnson_1986`).
- `HotPhase()` returns the values of §7.2 for an average officer and an average pilot, and the tests of §11 pass.
- In a played Pact raid a strike flight holds its route when a launch is seen, and its escort pair turns for home when
  its controller is jammed; in a played NATO raid a strike leader re-routes round a battery that fired and keeps his TOT;
  a neutral flight breaks off after its first loss more often than not; both are in the session log with the reason.
- The four new quirks have signatures and analysts' phrases, and appear in generated officers by their tier weights.
- 9.1 and 9.2 are decided by the owner, and if taken, run as written with the results in `09_BALANCE.md`.

## Left open

- Every value in §7 is an estimate. A person playing raids of each faction is the check; the weights of §8 are game
  values until then.
- Soviet and Pact peacetime flying hours in 1980-85 (no period source; a Polish retrospective's 40 h and forum figures
  are unsourced; `cia_1969_soviet_tac_training` is 1969).
- Johnson 1986 and Red Baron were not fetched: the 1983-84 ebb of initiative and the radio-chatter failure mode rest on
  the authors' first reading.
- Whether NATO in 1985 planned day-1 interdiction or waited for SEAD (a USAFE or 2 ATAF document): 9.2 rests on it.
- Whether Weasel teams in Europe in 1985 paired with F-4E killers and passed sites by voice (c2-15).
- The share of sorties retasked in 1980-85 (c2-51: none found).
- A neutral's rules of engagement, radio and retasking (Austrian or Swiss doctrine), and the missile ban from a citable
  source.
- HAVE QUICK's fielding in USAFE by 1985 (comms-09).
- Whether the officers' shift should follow the flight lead pilot rather than the squadron leader when he does not fly.
