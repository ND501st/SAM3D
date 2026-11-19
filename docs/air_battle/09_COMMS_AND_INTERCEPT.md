# 9. The enemy's radio, and who hears it

Code it will touch: `simcore/em/model.hpp` (`Message`, `RadioDef`, `EsmDef`), `em/comms_system.hpp` (`RunComms`),
`em/world.hpp` (`AddNode`, `AddNet`, `AddDummyTraffic`), `em_link.hpp` (`SyncTarget`), `air_war/data.hpp` (the faction
files' new `radio` block), `air_war/campaign.hpp` (`Transmission`, `CampaignAction`, the commit of a package),
`air_war/state.hpp` (`PackageOutcome`), `command/info_net.hpp` (`FactKind`), `command/air_war_feed.hpp` (`FeedRoles`,
`CampaignFeed`), `command/em_radio.hpp` (`RadioListener`), `scenario_runner.hpp` (`RegimentSays`, `PlayDeliveries`), and
the data `data/em/*.json`, `data/command/chain_pannonia.json`, `data/air_war/faction_*.json`. Research:
`docs/research/air_battle_2026-11/comms_emcon.md` (main), `c2_retasking.md`, `factions_cases.md`, `reactions.md`; code
maps `map/sensing.md` (main), `map/c2_people.md`, `map/scale.md`.

Every call an enemy flight or command post makes becomes a transmission in the session's EM world: a position, a band, a
power, some seconds on the air, and a content. Our side's intercept posts hear it or not by link budget and line of
sight, understand it or not by their linguists, and pass what they learned by the information pipeline, fast to the air
defence or slowly up the intelligence chain. The battery learns of it only as a message on its nets, written into the
radiotelephonist's journal. Nothing of it is shown on a HUD (`docs/command/01_PRINCIPLES.md` 1, 3, 9).

## 1. What the period shows

### 1.1 Two kinds of warning, by two roads

The best-documented case is the NSA's own study of how North Vietnam knew of US raids (`nsa_purple_dragon_1993`).

| Kind | Evidence | Lead | Road |
|---|---|---|---|
| **Hours**, from administrative and airspace traffic | In December 1966 two North Vietnamese army broadcast stations alerted 34 % of B-52 ARC LIGHT strikes, average warning 8.5 h, the likely source the flight plans, altitude reservations and NOTAMs of civil air traffic control; after the PURPLE DRAGON fixes, in April 1967, 5 %, with under 30 min (`nsa_purple_dragon_1993` pp. 22-26) | hours | traffic analysis, a slow product |
| **Minutes**, from tactical calls in the clear | Intercepted "Heavy Artillery Warnings" gave target coordinates and time on target "usually ten to thirty minutes before the bombs started falling" (p. 69); a hostile relay net passed intercepted US aircraft traffic on "on average within five minutes", in the original English (pp. 60-61) | 10-30 min | a heard call passed straight on |

For a raid already in the air, radar usually warned first. The same study left Rolling Thunder's tanker communications
alone because changing them would not significantly lessen the enemy's warning, given "the radar surveillance problem"
(p. 27; the detail redacted). That radar outran intercept is an inference (comms-05). So in SAM3D intercept rarely beats
the warning net's radars to a raid in flight. Its value is **identity, intent, target and timing**: who is coming, how
many, where to and when.

### 1.2 Habits are what intercept lives on

Static call signs and frequencies let enemy radio units follow allied units "for years at a time"; B-52s used fixed
call signs; new US signal operating instructions were broken out "in as little as six hours" and brevity codes "within
six hours" (`nsa_purple_dragon_1993` pp. 68-69, from prisoners: medium); an EC-121 in Korea kept one call sign for years
(p. 78). Border-warning code words were changed daily in 1965-72 (`nalty_1977`).

### 1.3 NATO

- **Deep strikes flew silent.** Black Buck One (1982) sent one codeword, "superfuse", at 0746Z; Northwood had it by
  0830Z (via `wiki_black_buck` as a pointer to Burden et al. 1986 and White 2012, the books not read). Red Flag packages
  of 60-70 aircraft went through in 20-30 minutes with "no radio calls required" (`rafhs_j20_1999`). El Dorado Canyon
  (April 1986, after the period) flew and refuelled in radio silence (`mobility_forum_2020`): evidence the practice
  existed, not a 1985 event.
- **Packages near the front talked.** Over the Bekaa (June 1982) a package was run by voice from an airborne and a
  ground command post, four-ships fed in one at a time (`grant_2002`); the Soviet reading made the E-2C "one of the
  main elements" of the formation (`mt_1984_babich`). An airborne controller is heard far, because it is high (§4.2).
- **The nets.** NATO air voice was AM in 225-400 MHz (`fh_radios`). NATO's tactical air control system used HF for
  immediate air requests, VHF-FM to the forward air controllers and UHF for strike control (`fm_100_2_1_1984` p. 15-3;
  `lawrence_1982`). There was no data link to a strike aircraft; retasks went by voice, directly or relayed by a CRC or
  an E-3A, which relayed but had no authority to retask strikers (`c2_retasking.md` c2-01 to c2-14).
- **HAVE QUICK**, UHF frequency hopping keyed by time of day, word of day and net number, was introduced in 1980; it is
  anti-jam, "not an encryption system" (`fh_radios`). When it reached USAFE's aircraft was **not found**. HAVE QUICK II
  (production 1986, tier 5 snippet only) is later than the war. Whether KY-58 secure voice was in fighters by 1985 is
  not found.
- **NATO's belief about the Pact.** The Pact would jam the whole tactical air control system (`fm_100_2_1_1984`
  pp. 15-2 to 15-3, naming the R-330A and R-834; `lawrence_1982`); the doctrine of the time held that the Soviets would
  destroy a third of NATO's C3, jam a third and listen to the rest (`miller_1980_tacs_jamming`).
- **Compass Call**, NATO's airborne jammer of fighter control, reached initial capability in 1983 but was based in
  Europe only from 1986-87 (`dvids_2018_ec130h`, `wiki_usafe_ew_units`): a later war's option.

### 1.4 The Warsaw Pact

- **Silent on the move.** An air army rebased "in small groups at low altitudes under conditions of complete radio
  silence" (`mt_1964_air_army` p. 6); NATO believed radio silence was standard operating procedure (`lawrence_1982`). On
  the attack radio was "reduced to a minimum or prohibited entirely" (`fm_100_2_1_1984` p. 12-8). The air defence and
  warning nets were exempt from radio silence (`vst_1988`, a dictionary of military terms of 1988).
- **Dependent on GCI, which had a channel limit.** An air army with three division and nine regiment command posts with
  automated control could guide 30-40 groups or single aircraft at once; fighters beyond that flew "a sortie for
  nothing" (`mt_1962_front_aviation`). Cut off from GCI by jamming, Syria's pilots broke into pairs and singles in 1982
  (`lambeth_1984`; `grant_2002`).
- **Coalition control was agreed beforehand.** Coordinating posts needed frequencies and call signs known in advance,
  lines of hand-over of aircraft in the air, and procedure charts "in the languages of their countries"
  (`mt_coord_wp_air` pp. 4-9). An impromptu change, or a flight handed to another nation's GCI, costs time and can fail.
- **Some fighters were steered by command link** (Lazur with the ground system Vozdukh-1: `air_combat.md` wp-02 from
  `ru_wiki_mig23`, a Wikipedia pointer: unverified). Such guidance sends no voice, but the ground transmitter still radiates (inference). Pact air
  sets: the R-832M, 100-150 and 220-400 MHz, 15 W (`valka_r832m`, low).

### 1.5 The neutral and the rebels

Nothing was found on a small neutral air arm's radio procedure or listening (comms-24). The Austrian analogue
(`austrian_af_wiki`, for scale only) would talk German (inference). The rebels' model is a guerrilla army's intercept
service: one company of 130 processed 7,745 messages in September 1966, living on plain voice
(`nsa_purple_dragon_1993` p. 68). Their own few aircraft would talk in our language on civil or captured sets
(inference): no language barrier for our posts.

### 1.6 Language

Soviet voice intercept operators of the 82nd radio-technical brigade OSNAZ at Torgau (1981-83) listened to US Army VHF
voice in English learnt in civilian life and at the junior specialists' school; a telegraphist with weak English
mis-wrote words; intercepts went on ruled forms and tape, and a doubtful translation was checked the same day by playing
the tape over the telephone (`osipov_2013`). Teaball's EC-121s carried Vietnamese-speaking specialists in 1972
(`boyne_2008_teaball`).

### 1.7 Our side's pattern

East Germany's radio intelligence regiment FuAR-2 had a reconnaissance depth of 400-450 km and noted "direct
cooperation with the central command post of the LSK/LV" at the first detection of a TR-1 on 17 July 1984
(`militaermuseum_fuar2`). By 1989-90 air defence division operative groups sat at the intercept centres "to pass
air-situation information to the LVD command post", with daily reports to the central command post
(`bischoff_fufutakl`). **That liaison is documented only for 1989-90**; when it began is not found. For the 1985 war
it is a design choice modelled on it, and the data says so.

### 1.8 Congestion

On 29 September 1972 "repetitive SAM alerts choked the radio channels and prevented the Weasel crew from hearing the
warning" (`nalty_1977`); Red Baron found radio chatter blocking warnings (`wseg_116_1967`, unverified). A busy
net loses calls for the enemy and gives our posts more to hear.

## 2. Every call a transmission

### 2.1 Who has a radio

| Node | Faction | Band | Power | Height | Notes |
|---|---|---|---|---|---|
| Flight (each aircraft at A2, the leader at A1) | NATO | UHF AM 225-400 MHz (`fh_radios`) | placeholder (the ARC-164's 10 W is tier 5, recorded only) | its altitude | HAVE QUICK and secure voice per unit (§3.2) |
| Flight | Pact | VHF 100-150 or UHF 220-400 MHz (`valka_r832m`, low) | 15 W (same) | its altitude | |
| Flight | neutral, rebels | VHF AM (inference), placeholder power | | its altitude | rebels on civil or captured sets |
| E-3A (orbit from `11_SUPPORT.md`); the A-50 only in a later war (`07_AIR_TO_AIR.md` §2.6) | NATO | as its fighters' nets | placeholder | orbit height | controls fighters by voice |
| CRC / GCI post | all but rebels | as the fighters' nets | placeholder | mast (game value) | Pact GCI: a channel count per post (§2.6) |
| Command link ground transmitter (Lazur class) | Pact | not found | placeholder | mast | no voice; a carrier with timing only |
| FAC (TACP, ПАН) | NATO, Pact | NATO VHF-FM to the TACP, UHF to aircraft (`fm_100_2_1_1984` p. 15-3); Pact R-809M at the ПАН (`c2_retasking.md` c2-29) | placeholder | ground or helicopter | |
| ASOC / ATOC request net | NATO | HF (`fm_100_2_1_1984` p. 15-3) | placeholder | ground | between ATOCs the ATO went by data (EIFEL 1, `afmag_1983_usafe`): not radio |
| Air army or regiment KP command radio | Pact | not found | placeholder | ground | |

Powers and receiver data no source gives are labelled `"provenance": "placeholder"` in `data/em/`, as `generic.json`
already does. New entries go in `data/em/nato_1985.json` and a Pact file with `per_value` provenance where a source
exists, each with an `in_service_year`.

### 2.2 What a call says

A call has a **kind**. The doc that makes the call (`04_REACTIONS.md` for threat calls, `05_C2_AND_RETASKING.md` for
its `C2Message`s, `07_AIR_TO_AIR.md` and `08_OUR_FIGHTERS.md` for fighter control) sets how often each kind is said and
hands this layer the kind, the speaker, the addressee and the words; this doc gives it a length, a net and what it tells
a listener who understands it. `05_C2_AND_RETASKING.md` §4.3 maps its message kinds onto these call kinds, and this doc
keeps that mapping.

| Kind | Said by | Typical length (game value) | What a listener learns |
|---|---|---|---|
| `check_in` | a flight to a FAC, CRC, E-3A or GCI: call sign, number, ordnance, time on station (`c2_retasking.md` §1.3) | 10-20 s | the unit (by call sign and continuity), how many, what they carry, how long they stay |
| `threat_call` | a wingman or a controller (05's `Warning`): a launch seen, MiGs, "break right" (`reactions.md` reactions-11) | 2-5 s | a launch was seen and roughly where the flight is; NATO's SAM-launch call came on the strike frequency seconds after the launch (`mt_1973_nvn` pp. 17-18) |
| `report` | a flight to its controller, only if asked in flight (05's `InFlightReport`) (`c2_retasking.md` c2-17); a recce find | 10-30 s | what the enemy now knows: a site or column found, its square as said |
| `request` | a TACP to the ASOC on HF (05's `Request`; `Approval` and `Refusal` are short replies of this kind); a Pact division's aviation control element up its chain | 20-60 s | a target and a wanted time, 15-50 min before bombs (`c2_retasking.md` c2-50) |
| `divert` | ASOC, ATOC, CRC, GCI or FAC to a flight: a new target, a new time or a new route (05's `Divert`, `Brief` and `Recall`) | 10-30 s | the new target and time |
| `vector` | GCI or E-3A to fighters: heading, height, range (Teaball's calls were heading, speed and vector, `boyne_2008_teaball`) | 3-8 s, repeated | that fighters are up and what they are being steered at |
| `codeword` | a comm-out strike after its attack or on an abort (comms-11) | 1-3 s | almost nothing, until the analysts know the habit (§4.4) |
| `admin` | tankers, recovery, bingo, frequency changes | 3-10 s | timing and numbers, little else |

A `Scramble` goes by land line or the command radio to the alert pool: on a radio it is a short `divert` to a ground
node, heard only near the front; a `DebriefReport` is made after landing and is not an air call.

Calls are plain language with call signs and clock codes (the brief's decision 1). For US voice the words defined by
TRADOC TT 17-50-3 (1983) may be used: Bandit, Bingo, Blind, No Joy, Tally-ho, Visual, Winchester (`tradoc_jaat_1983`).
Nothing from the 1997 brevity list.

### 2.3 The message carries content

Today `em::Message` has no payload ("Not modelled: what an intercept reveals by its content",
`docs/EM_ARCHITECTURE.md:586`). Change: `Message` gets `int payload = -1`, an index into a session-level `CallLog` (new,
`simcore/air_war/air_radio.hpp`) that holds each call's kind (and its `C2Message` id where it is one), call sign, language, words, the square and time it names,
and the truth subject (for the debrief only). The EM layer stays engine- and content-agnostic: it times the message,
radiates it and decides who hears it; only the intercept post reads the payload, and only through its linguist (§4.3).
`MessageKind::Voice` is used for every call; a command link's data is `MessageKind::TrackReport` with no payload a
listener can read.

### 2.4 Aircraft as EM nodes, by level of detail

Enemy aircraft are never EM nodes today (`map/sensing.md` §6). They become nodes by the brief's levels:

- **A2** (inside the play circle): one `em::NodeState` per aircraft, made in `EmSensorLink::SyncTarget` beside the RWR
  (`em_link.hpp:381-397`), on its flight's net and its package's control net; each call radiates from the speaker's own
  platform.
- **A1** (flown coarsely): one node per flight, on the leader's platform.
- **A0** (abstract): no node. The time-stepped resolver gives each package its calls as `Transmission`-like records
  with a position, a kind and a height; the campaign rolls our posts' hearing of them by the same horizon rule (§4.2)
  in closed form, keyed by package and call index. These feed only the slow product (§5.2).

C2 nodes (CRC, GCI, FAC, ASOC, KP) are ground nodes at their places in the enemy's command data
(`05_C2_AND_RETASKING.md` §4.1); an E-3A or A-50
is an air node on its orbit. Nodes are made when a package enters the session and removed when its last aircraft is
gone, so the target lists do not grow without end (`map/scale.md` §1.4).

### 2.5 Turns on a net

`NetState::busy_until_s` already makes a net half-duplex. A call waits for the net; a threat call breaks in after the
current transmission; a call older than its use (a threat call after 15 s, a vector after 10 s: game values) is
dropped. Congestion then follows from the number of calls, as in 1972 (§1.8). A stressed pilot's calls are longer and
repeated (game value), so a hot fight chokes its own net.

### 2.6 GCI channels

Each Pact GCI post (and ours, for our fighters) has `channels`, the groups it can guide at once. The air army's 30-40
for twelve command posts (`mt_1962_front_aviation`) gives about three per command post: a SAM3D reading of the
principle, not a sourced per-post number. A group without a channel gets no vectors and searches on its own, worse.
Each channel in use is a `vector` stream on the air, which our posts hear.

## 3. Discipline: who talks, and how much

### 3.1 By faction

Values are the research's SAM3D estimates, ordered by the sources (`factions_cases.md` §1).

| | Rebels | Neutral | Warsaw Pact | NATO |
|---|---|---|---|---|
| Radio discipline (silence on the attack) | 0.3 | 0.6 | 0.8 strike; fighters talk to GCI | 0.8 (comm-out packages) |
| What talks | everything, in our language | short flights near the border, GCI | GCI, warning and command nets; the FAC at the checkpoint | controllers near the front (E-3A, CRC, FAC); Weasel teams within the team; the HF request net |
| What is silent | nothing by doctrine | ground attack on planned missions | strike groups en route and on the attack; transfers | deep strikes (AI, OCA) to their codeword |
| Language | ours | German (inference) | Russian; allied air forces their own (`mt_coord_wp_air`) | English (NATO's common air language: inference) |
| Call sign rotation | never (game value) | weekly (game value) | per plan cycle (game value) | daily (game value; code words changed daily, `nalty_1977`) |
| Staff OPSEC (§5.3) | low | medium | high: planning on landlines and couriers (`lawrence_1982`) | medium-high |

A faction's file gets a `radio` block (new, loaded strictly in `air_war/data.hpp`, every value with `source` and
`confidence`): `air_band_hz`, `modulation`, `language`, `callsign_rotation_h`, `opsec`, `have_quick_share`,
`secure_voice_share`, `gci_channels_per_post`. The discipline row above is not in it: it is the faction's
`hot_phase.radio_discipline`, with a `by_role` value for fighters under GCI (`10_FACTIONS.md` §7.2, §7.5). Tuning keys
(`radio.call_length_s.<kind>`, `radio.stale_s.<kind>`, `radio.stress_repeat`) go in `data/gameplay/air_war.json` and
`TuningTable::Defaults`.

### 3.2 By unit and person

- **HAVE QUICK** and **secure voice** are per-squadron flags drawn from the faction's share, both 0 by default until a
  USAFE fielding date is found (§1.3). A hopping net is still heard and direction-found, but its content is followed
  only with difficulty (comms-09, inference): the content chance is multiplied by `radio.have_quick_content`
  (game value, 0.2).
- **Officers** move discipline by at most about ±0.2 (the brief's decision 9): methodical raises it
  (`10_FACTIONS.md` §8.2), and the new quirks `chatterbox` and `silent_runner` move it (§8.5 there). The existing quirk
  `loose_lips` ("Our intercepts give warning of his raids hours ahead", `data/air_war/traits.json`) lowers it by 0.2 on
  his nets (`05_C2_AND_RETASKING.md` §4.1 the same) and lowers the staff's OPSEC (§5.3), replacing today's fixed 900 s
  leak warning (`raid_scenario.hpp:852-855`). The call-sign rotation interval is the staff's: a `by_the_book` planner keeps the
  faction's interval, a careless one doubles it (game value).
- **Pilots** move it by about ±0.15: a green or frightened pilot talks more and longer; Syria's pilots, once cut off,
  "lost any semblance of air discipline" (`lambeth_1984`).

## 4. Our listeners

### 4.1 Posts in the chain of command

`army_intel` (Lt Col Viktor Ondrejka, `chain_pannonia.json`) already holds "radio intercepts of the enemy's air forces"
and has no listeners. New posts below him, in `data/command/chain_pannonia.json` with places in
`places_pannonia.json`:

| Post | Kind | Where | Means |
|---|---|---|---|
| `intercept_centre` | the air section's radio intercept company's centre (the army-level radio and radar intercept company of `npic_1981_intercept`, 75-80 vehicles) | near the army's headquarters | field telephone, teleprinter, despatch rider |
| `intercept_post_<n>` (two or three) | forward intercept and DF posts on high ground, as the OSNAZ brigade's mountain sites (Brocken, Schneekopf: `osipov_2013`) | the hills nearest each front | field telephone, radio |
| `ad_liaison_<n>` | the air defence liaison at each forward post (the 1989-90 pattern, §1.7) | with the post | a direct telephone to `div_ad_cp_duty` |

These are radio-technical units in the OSNAZ sense (signals intelligence), not the radio-technical battalions of
`oob_pannonia.json`, which are radar troops. Each post has a holder with a skill tier and linguists (§4.3). The posts
can be bombed, jammed or moved like any post (`docs/command/05_PLACES.md`).

### 4.2 Hearing: link budget and line of sight

Each post is an `em::EsmDef` of kind `CommsIntercept` (new entries, `pact_air_intercept_vhf_uhf` for 100-400 MHz and
`pact_hf_intercept` for the request nets) placed by a new scenario key `"intercept_posts"`, filled by the raid builder
from the chain's posts within reach. Today's only intercept receiver covers 20-100 MHz (`generic.json`). The receivers'
sensitivity is a placeholder until a set is sourced; the East German regiment's ~600 receivers over 14 kHz-40 GHz
(`bischoff_fufutakl`, 1989-90) is an upper bound, not a 1985 value.

A call is heard when the link budget through `LinkLossCached` (terrain, gases, ground, woods) clears the receiver's
sensitivity. Line of sight dominates in the air bands. The 4/3-earth radio horizon (textbook, generic physics) to a
30 m mast (game value):

| Transmitter height | Horizon |
|---|---|
| 15 m (a ground GCI, FAC or CRC mast) | about 39 km |
| 50 m (a strike flight at low level, `factions_cases.md` our-01) | about 52 km |
| 1,000 m | about 153 km |
| 6,000 m | about 342 km |
| 9,000 m (an E-3A on orbit) | about 413 km |

So our posts hear aircrew at height and airborne controllers far, and the enemy's ground nodes only near the front: the
KAL 007 case points the same way, the pilots' frequency heard, the ground controller's disputed (`frus_1981_88_v4_d84`;
`acig_kal007`, low). A NATO low-level package is heard only in its last 50 km, if it talks at all; an E-3A behind
NATO's line, 170 km from the Kesmark battery, is heard by a post near the front. HF sky wave has no such limit
(`fm_100_2_1_1984` p. 15-2, the Pact's reach as NATO saw it), so the request net is heard far.

Bearings: VHF DF "usually within ±3.5°" (`fm_100_2_1_1984` p. 15-2; `lawrence_1982`) is the bearing sigma of the new
defs; two posts' bearings make a fix through the existing `BearingFix`.

### 4.3 Understanding: linguists

Hearing gives the time, frequency, bearing and length. The content needs a linguist for the call's language. Each post
has linguists per language with a skill tier from `data/crew/skill_levels.json`:

- **delay** to write the call on the form (game value by tier, 20-120 s);
- **errors**: a word dropped or a digit written wrong, by tier (the Torgau telegraphist's weak English, `osipov_2013`);
  the written form is the post's record, right or wrong;
- **doubt**: a call below a tier's confidence is checked by telephone up the chain before it goes on, as at Torgau,
  costing minutes;
- **no linguist** for the language: the call is logged as traffic only (time, bearing, length, net). The relay of
  §1.1 passed calls on untranslated in about five minutes (`nsa_purple_dragon_1993` pp. 60-61): a post may do the
  same, faster and understood only further up.

Draws are keyed by (call id, post id) on the session's own stream (the brief's decision 6).

### 4.4 Continuity and traffic analysis

Per enemy unit (squadron, controller) each post keeps a **continuity** 0..1: it grows with each call heard under an
unchanged call sign, and falls to near 0 when the call signs change, regained in about 6 h of listening
(`nsa_purple_dragon_1993` pp. 68-69: new instructions broken out "in as little as six hours"). With continuity the
analysts name the unit, its type and its base; without it a call is "an unidentified flight". A comm-out strike's
codeword means something once the analysts have heard the habit before (§1.3): a learned pattern per faction, held
by `army_intel`, like today's `RevealFromPatterns` (`campaign.hpp:1562-1587`).

### 4.5 Cost

Intercept is evaluated once per message and post, at the message's start, not on the 0.2 s ESM tick: a new
`RunCommsIntercept` inside `RunComms` (`comms_system.hpp:239-337`), one cached link budget per pair. A busy raid
makes some tens of calls a minute; five posts make a few hundred budgets a minute, far inside the brief's 0.5 ms per
step. A2 nodes' emitters radiate only while a call is on the air, so the RWR and ESM sweeps see them only then.

## 5. Two roads to the battery

### 5.1 The fast road (minutes)

A call whose content names a time and a place within the army's area (a `divert`, `request`, `check_in` with a target,
a `report` of a site) goes from the linguist's form to the post's liaison, by telephone to `div_ad_cp_duty`, and from
there down the warning net or the regiment's command net to the battery. The liaison passes it in about 5 min (a game
value anchored to comms-03); the means' own delays are the pipeline's. It is worth something when the call's lead
(10-30 min, §1.1) exceeds the road's delay; otherwise it confirms what the radar already shows.

In the pipeline: a new `FactKind::Intercept` (`info_net.hpp:44`), origin the intercept post, precedence 2 when it names
the battery's area or an object it covers within the hour, 1 otherwise; `summary` the words as the linguist wrote
them. A heard `report` that names the battery's square is also an `EnemyKnowsBattery` fact for `rgt_chief_of_staff`:
our side learns what the enemy knows from what it said.

In the session: the facts are made live, so the session needs a runtime path that does not exist today (the
deliveries are computed before the session, `map/c2_people.md` §9.1). `MissionSession` steps the `InfoNet` with the
session's new facts each minute (it already runs the regiment's tasking each minute), and a new
`ScenarioRunner::AddDeliveries(std::vector<DeliveryDef>)` hands the due ones to `PlayDeliveries`
(`scenario_runner.hpp:4157`), said on the warning net's speaker or the regiment's net. `RegimentSays` already carries a
message to the post's R-111 and repeats it until receipted. The radiotelephonist writes it into the journal
(`radio_journal.hpp`) as heard, with its slips: a wrong digit from the linguist and another from the journal both go
into the report and the regiment's picture (`01_PRINCIPLES.md` 9).

No crewman overhears a pilot: the battery's R-123M and R-111 cover 20-52 MHz, below every air band
(`map/c2_people.md` §12).

### 5.2 The slow road (the daily summary)

Everything the posts logged, understood or not, goes into the air section's daily summary: units heard and their
continuity, traffic volume by front and hour (a rise before a massed blow), call-sign changes, codewords and the habits
learned, the enemy's controllers' places by DF. It is a `FactKind::Intsum` document from `army_intel`, routine, need
level down to battery commanders, carried by teleprinter and despatch rider to the regiment's evening briefing
(`01_PRINCIPLES.md` 4). The analysts' notes on officers (`EnemyOfficer`, `air_war_feed.hpp:286-293`) now rest on it:
a `loose_lips` or `flies_himself` officer is revealed by what was heard of him.

### 5.3 The hours warning: the enemy staff's OPSEC

When a package is committed (`Planner::Commit`), a keyed draw on the staff's `opsec` decides whether its preparation
shows in the enemy's administrative traffic (tasking, tanker and airspace coordination). PURPLE DRAGON's two ends set
the scale: a careless staff alerts about a third of its raids hours ahead (34 %, 8.5 h), a careful one a few with under
half an hour (5 %) (`nsa_purple_dragon_1993` pp. 22-26). The mapping of `opsec` 0..1 onto that range is a SAM3D game
value; the Pact's centralised planning on landlines and couriers sits near the careful end (`lawrence_1982`); NATO's ATO
went between ATOCs by data (`afmag_1983_usafe`), whose intercept is not modelled. An alert is a fact at
`intercept_centre` with the lead drawn from the range: "a raid towards the Kesmark district expected between 14 and 16
hours", partial and sometimes wrong (game value). The draw happens in the campaign, so it is replayed from the seed.

### 5.4 Back to the campaign and the save

`PackageOutcome` gets `heard`, beside `05_C2_AND_RETASKING.md` §4.5's `calls` (what the enemy sent): each call our posts
logged, with post, time, kind, the text as written and the fact made.
`CompletePlayerMission` stores it in the `Complete` action, so a loaded campaign rebuilds the facts without replaying
the session (`map/scale.md` §4.3). Abstract packages' calls are rolled in the campaign itself (§2.4).

## 6. The other way: the enemy hears us

P3 is built: the battery's pipeline calls feed the enemy's COMINT (`CampaignFeed::BatteryCalls`,
`AirCampaign::NoteTransmissions`, `intel.comint_per_call`), and P5 hears them by link margin at the enemy's
`RadioListener`s, which are not placed: with none placed the enemy hears everything (`command/em_radio.hpp:9-11`). This
doc places them, so the two sides are symmetric:

- **Pact enemy**: a front's radio and radar intercept regiment behind its line (`fm_100_2_1_1984` p. 15-2: VHF about
  40 km, HF ground wave about 80 km; the adversary's view) and the Il-20M orbits (`11_SUPPORT.md`).
- **NATO enemy**: the RC-135 orbits (`11_SUPPORT.md`); NATO's ground intercept of Pact voice in Europe was not found in
  the research (`comms_emcon.md` §7), so its posts are game values.
- **Rebels**: a few posts with native speakers near their home ground, reading our plain voice well (comms-25).
- **Our GCI is heard too.** A NATO enemy can turn our GCI voice into MiG warnings for its flights, as Teaball did in
  1972 (`boyne_2008_teaball`; CHECO's 19 MiGs for 5 losses, `checo_1978_linebacker`), with a delay of about a minute and a relay that can fail. A Teaball-type service in Europe by 1985 was **not found**: it is
  a design choice for NATO, off by default. Our flight notices down the warning net reveal our flights to the enemy
  (`mt_turchin_coop`).

The enemy's listeners use the same per-message rule (§4.5) and the same linguist model with English, German or Russian
speakers; what they learn reaches their staff through `Transmission` records, as today.

## 7. Jamming the enemy's air nets

Our side does not jam today (`docs/campaign/04_WHAT_HAPPENS_AND_WHY.md` §4). The period doctrine, as NATO read it,
says a Pact army would: the tactical air control system's HF, VHF-FM and UHF nets (§1.3), and an enemy commander
chooses per net whether to jam it or listen to it ("you cannot jam it when you want to get information", Ivry,
`grant_2002`).

**Decision for track B: not planned in its first form.** The army's jamming units are above the battery, and the
player cannot significantly change the wider war. The data is laid so it can follow: an `our_jamming` entry in
`data/command/` reusing `air_war/comms_jamming.hpp`'s J/S rule with the sides swapped, put on the enemy's control nets
in the session; a jammed net delays or loses `05_C2_AND_RETASKING.md`'s diverts and requests; a HAVE QUICK net resists spot jamming. It is
listed in Left open as the owner's decision. The enemy's jamming of our nets stays as built (roadmap 16.2); Compass Call
is a later war's option (§1.3); in the Bekaa scenarios Israel's jamming of Syrian GCI is the scenario's own.

## 8. Deception with dummy traffic

- **Ours.** `EmWorld::AddDummyTraffic` already makes a node transmit empty calls on a schedule (`vr_1985_plss`, the
  Czechoslovak 1985 doctrine). It applies to our GCI and warning nets as to the battery's, so the enemy's listeners of
  §6 can be fed false traffic.
- **Theirs.** An officer with `feint_artist` or `decoy_lover` can give a fighter sweep a strike's call signs and nets, so
  our analysts name the raid wrongly: Operation Bolo flew F-4s on an F-105 strike's route, height and pods
  (`nmusaf_bolo`, unverified). A comm-out strike with a talkative feint elsewhere is the same idea. A Pact or NATO text on
  dummy air radio traffic in the period was not found, so the shares are game values.

Our analysts' identification of a raid from what they heard can therefore be wrong on purpose, and that error travels
into the summary, the journal and the regiment's estimate like any other.

## 9. Changes in one place

| Area | Change |
|---|---|
| EM | `Message::payload`; airborne and ground `RadioDef`s for the enemy's nets with `in_service_year`; `CommsIntercept` defs for 100-400 MHz and HF; `RunCommsIntercept` per message; A2/A1 nodes and nets made in `EmSensorLink::SyncTarget` and removed with the package |
| Air war | the faction `radio` block and tuning keys; `CallLog` and the call kinds, with `05_C2_AND_RETASKING.md`'s `C2Message` kinds mapped onto them (`air_war/air_radio.hpp`); `opsec` draw at `Planner::Commit` replacing the fixed leak warning; `loose_lips` read as discipline and OPSEC; A0 calls rolled in the stepped resolver; `PackageOutcome::heard` and its save |
| Command | `FactKind::Intercept`; the intercept posts, liaisons and places in `chain_pannonia.json` and `places_pannonia.json`; `FeedRoles::intercept`; linguists and continuity per post; the daily summary; the enemy's `RadioListener`s placed |
| Session | `MissionSession` steps the `InfoNet` live; `ScenarioRunner::AddDeliveries`; a scenario's `"intercept_posts"` |
| Tests | `simcore/tests/test_air_radio.cpp` (new), `test_command.cpp`, `test_packages_live.cpp`, `test_em_*.cpp` |

## Done when

1. A NATO flight at 1,000 m checking in with a FAC 120 km from a forward post is heard there; the same call from 50 m
   at 80 km is not; a call from an E-3A's orbit 300 km away is heard (test against the horizon table).
2. A NATO deep strike flown comm-out leaves only its codeword in the posts' logs; the codeword is logged as traffic until
   `army_intel` has learned the habit, and as "strike complete" after.
3. A Pact strike group gives no calls between take-off and the checkpoint; its GCI net and the FAC's confirmation are
   heard; a GCI post asked for more groups than its channels leaves the rest without `vector` calls.
4. A heard `divert` naming the battery's square reaches the battery's radiotelephonist as a message on its net and is
   written in the journal; the time from the call to the journal is the linguist's delay plus about 5 min plus the
   pipeline's; no panel, HUD or debug view outside the debrief shows it (a canary test as in `test_em_replication.cpp`).
5. With no linguist for the language the post logs time, bearing and length only; with a weak linguist a digit is wrong
   in some calls, and the wrong digit appears in the journal, the combat report and the regiment's picture.
6. After a call-sign change a post's continuity on the unit falls below 0.2 and is back above 0.8 within about 6 h of
   steady traffic.
7. A HAVE QUICK unit's calls are heard and direction-found at the normal rate and understood at the reduced rate; in the
   1985 theatre no unit has HAVE QUICK II or Compass Call.
8. Over 200 committed packages, a staff at `opsec` 0 is alerted ahead in about 34 % with leads near 8.5 h, at `opsec` 1
   in about 5 % with under 30 min (the mapping's ends).
9. A heard `report` that names the battery's square makes an `EnemyKnowsBattery` fact for the regiment's chief of staff.
10. The enemy's placed listeners hear our GCI and the battery's nets by link margin; with none in reach they hear
    nothing (no longer everything).
11. The same seed gives the same intercept log; a campaign loaded from its save rebuilds the facts from
    `PackageOutcome::heard` without replaying the session.
12. With 100 aircraft and five posts the radio and intercept add under 0.1 ms per step on `em_bench`'s scale; the
    `air_war_balance`, `air_war_bench` and `helper_coverage` tables are rerun and their changes read.

## Left open

- **HAVE QUICK in USAFE**: when HAVE QUICK I reached the F-15, F-16, A-10, F-111 and F-4 in Europe, and whether KY-58
  secure voice was in fighters by 1985. Both shares stay 0 until found.
- **The words of 1985**: NATO's radio procedure (check-ins, frequency plans, call-sign rotation) beyond the TRADOC
  glossary; the Soviet aviation radio exchange rules (НПП) and the Lazur/Vozdukh-1 description, its types and update rate.
- **The sets' powers and our receivers' sensitivity**: no tier 1-4 source read gives an air set's power for NATO or a
  1985 Pact intercept receiver; all are placeholders.
- **The liaison's date**: when the East German air defence operative groups at the intercept centres began; until then
  the fast road is a design choice for 1985.
- **Soviet air OSNAZ** (the 16th Air Army's units, the «Борт-Земля» post at Torgau) and what Pact intercept of NATO air
  traffic yielded in exercises; NATO's intercept of Pact GCI in Europe and whether a Teaball-type warning reached NATO
  aircrews in the 1980s.
- **Our side's jamming of the enemy's air nets**: not planned; the owner decides whether it enters track B later, and
  whether the army's commander chooses per net to listen or to jam.
- **Dummy air radio traffic**: no period text on either side; the feint and deception shares are game values.
- **The neutral's radio and listening**: nothing found; an Austrian, Swiss or Swedish document is needed.
- **The GCI channels per post**: 30-40 groups for twelve command posts is a 1962 air army figure; a per-post value for a
  small nation's GCI in 1985 is a SAM3D reading of it.
- **Our order of battle**: the nation's `oob_pannonia.json` has no radio intercept unit; its size and posts follow the
  NPIC company until a source for a small Pact army's air intercept is found.

## As built (B9, 17 November 2026, branch `tb-b9`)

**Files.** `simcore/air_war/air_radio.hpp` (the calls, the discipline, the opsec draw, `AirRadio`), `simcore/air_radio_def.hpp` (what a
scenario says: the block, the flight's radio, the call tuning), `simcore/em/comms_intercept.hpp` (`RunCommsIntercept`, called by
`RunComms`), `simcore/command/intercept.hpp` (the desk, the linguists, continuity, the facts, the bridge to the pipeline),
`FactKind::Intercept` in `command/info_net.hpp`, `Flights::SetRadio` and the `FlightRadio` hook in `flights.hpp`, the scenario's
`"air_radio"` block and a flight's `"radio"` block (`scenario.hpp`), `ScenarioRunner::EnableAirRadio`, `AirRadio()`,
`AddDeliveries`, the mission session's intercept service (`campaign/mission_session.hpp`), `HeardRecord` and
`PackageOutcome::heard` with its save, a `radio` block in each faction file, the `radio` group of `data/gameplay/air_war.json`, the
quirks `chatterbox` and `silent_runner`, `data/em/air_radios_1985.json`, `data/command/intercept.json`, seven posts and places in
the chain, and `tests/test_air_radio.cpp`. A new random stream, `Stream::AirRadio` (16), is mirrored in the prototype's table; it
draws only for the calls and our posts, so no other stream moves.

**Transmissions.** Each aircraft of an enemy flight is a platform and a node of a session-level EM world that `AirRadio` owns,
each flight a net (one channel above the last), each ground post (`gci`, `crc`, `fac`, `awacs`) a node and a net of its own. A leader's
act (`Flights::Order`, by `"leader"` or `"package"`) or a post's call (`SayFrom`) becomes a voice `em::Message` with the call's number
in `Message::payload`. Its length is its kind's (`radio.call_<kind>_s`) times `1 + stress_repeat x stress` (a threat call 1, an attack
0.5); its turn is `max(now, busy_until)`, a threat call after the call on the air only, and a call older than `stale_*` is dropped
(`kDropped`). A member acts on it only if the EM link budget from the speaker's platform to his clears the receiver's
(`em::EvaluateLink` and the layer's own delivery draw, `Stream::CommsDelivery`): the call's end plus the flight's `call_s`, else he
keeps the last order he heard. Without an `air_radio` block, B1's fixed delay stands. Every aircraft is a node (the A2 level); A1 and A0
are not told apart. A flight whose aircraft are all gone leaves the world 30 s after its last call.

**Discipline.** The faction's `radio.discipline`, plus the flight's `discipline_adjust` (the officer's quirks: `chatterbox` and
`silent_runner` through `radio_chatter`, `comsec_leak` through `radio.discipline_loose_lips`), held to 0..1. A flight is comm-out when
its builder says so, or when it is a strike, DEAD or recce flight and a keyed draw falls under its discipline (faction discipline
at least 0.5). A comm-out flight says no routine call (`check_in`, `report`, `admin`, `request`) before its attack; the act goes by
signal (`kSignalled`): only to those within `radio.signal_range_m` of the leader, `radio.signal_factor` later than a call.
Threat calls are always said. A strike flight past its attack keeps routine calls off the air with the discipline's chance. After
its attack a comm-out flight says its unit's codeword once (`codeword`, `AirRadio::Watch` on the phase change from `attack`).
At the front (phase `ingress`) a flight not flying silent checks in. HAVE QUICK and secure voice are per unit flags, both 0 in the
data. Call signs rotate by the faction's interval (`CallsignAt`; a careless planner doubles it; 0 never).

**Our posts.** The scenario's `air_radio.posts` of kind `intercept` are ESM receivers of kind `comms_intercept` (the library's
`air_intercept_vhf_uhf`, 100-400 MHz, and `air_intercept_hf`, 2-30 MHz) on a platform at a 30 m mast. At each call's start
`RunCommsIntercept` asks each receiver once: free-space power first, then the cached path loss (terrain, ground, gases; HF is the sky
wave with a flat loss), against its sensitivity; past the 4/3-earth horizon (no line of sight) a VHF or UHF call is not heard at all, whatever the diffraction leaves of it (a game rule, section 4.2's table); with a bearing error keyed on (receiver, message). The mission session places the
chain's posts (`intercept_post_1` and `_2` at their places, with a VHF/UHF and an HF receiver each) in the session's frame. The
posts' hearing is what `AirRadio::HeardLog` holds: time, bearing, length, level, which call.

**Understanding.** `InterceptDesk::Process` writes each heard call up as the post's linguists would: a call in a language the post
has no linguist for goes untranslated to the post it relays to (`relay_untranslated_s` later), else it is logged as traffic (time,
bearing, length, "not read"); a secure net is never read, a hopping one with chance `radio.have_quick_content`; the form is done
`delay_s` of the tier after the call, plus `doubt_check_s` when a doubt is checked by telephone; a digit may be written wrong and a
word dropped by the tier's chances. A written square is read back from the form (`FindGrid`). Continuity is per (post, unit) and
runs by the time heard under an unchanged call sign, `1 - exp(-hours / tau)`, restarting at a change; from `continuity_name_at`
the post names the unit. A codeword is traffic ("meaning not known") until it has been heard `habit_after` times or
`LearnHabit` has been told, then "strike complete".

**Two roads.** The fast road: a read call that names a place (`CallNamesTarget`) is a `FactKind::Intercept` fact from the post's
liaison at the form's time plus `liaison_s`, for `div_ad_chief`'s tree down to level 3, precedence 2 within `area_m` of the battery
(the session's launchers' middle, updated each minute), else 1; `InterceptBridge` runs the chain's `InfoNet` for the session and
gives when the battery commander learns it, and the mission session says it once per call (several posts hearing one call make one telling) on the
battery's net (`ScenarioRunner::SayOnBatteryNet`), where the post's journal keeper writes it down as heard. A heard `report` that names the battery's square also makes an
`EnemyKnowsBattery` fact for `rgt_chief_of_staff`. The slow road: `InterceptDesk::DailySummary` (calls, read and unread, units and
their continuity, the busiest hour, more call signs than units, codewords, two posts' bearings as fixes) is a `FactKind::Intsum`
document from `army_intel` at the day's `daily_summary_hour`, delivered to the commander as a paper by the means the pipeline
gives (`AddDeliveries`). The hours warning: `DrawOpsecAlert(opsec)` maps opsec 0..1 in a straight line from 34 % with 8.5 h to 5 %
with 0.4 h, keyed on the seed and the package; the campaign's feed puts it into the net as a fact at the intercept centre
(`CampaignFeed::AddOpsecAlert`), and the raid builder uses it in place of the fixed leak warning when `ScenarioFormat::air_radio`
is on (`loose_lips`, the `comsec_leak` quirk, lowers opsec by `radio.opsec_loose_lips`). `PackageOutcome::heard` holds what the
posts logged of a played package as they wrote it, and is saved.

**The enemy's listeners.** `data/command/intercept.json` gives each faction's receivers (theatre km); `PlaceEnemyListeners` adds
them to a `command::EmRadio`, so our nets are heard by link margin and, with some placed and none in reach, not at all. With none
placed the abstract service still hears everything (P3), as before.

**Cost (measured).** `test_air_radio.cpp` times `AirRadio::Step` with 100 aircraft in 25 flights, five intercept posts and a call every
20 s (30 calls in a 600 s run, steps of 0.5 s): 0.34 ms a step on a machine busy with three other builds, so about 0.1 ms on a quiet one
is likely but was not measured. This is the whole world's step (its EM links and nodes), above the design's 0.1 ms for the calls alone, and far
inside the 0.5 ms per-step brief of section 4.5. The test fails only above 5 ms. `em_bench` does not use the radio and is unchanged.

**Balance.** `air_war_balance data 20 7` against the tree before B9 (run on the old data, as the first baseline file was made with new data and
an old binary): the six non-NATO blocks and the NATO blocks' first five days are unchanged; NATO days 6-7 move by tenths (a loss rate of 1.0% to 1.3%,
'enemy knows battery' 0.52 to 0.55, one in 47 columns). Cause: the opsec alert and intercept facts are new facts in our pipeline, which the regiment's
tasking reads on its estimates, so the battery's posture shifts a little in the long NATO campaigns. `air_war_bench` and `crew_balance` are identical.
`helper_coverage` differs only in the `dynamic_campaign_test` rows, which could not start in the first baseline (new data, old binary).

**Left.**
- **A0 and A1.** Abstract packages' calls (`radio.abstract_calls`, section 2.4) are not rolled in the resolver: that needs B5's
  time-stepped resolver. An aircraft is a node at every level.
- **The campaign's rebuild.** `PackageOutcome::heard` is saved, but the campaign's feed does not yet rebuild the intercept facts
  from it on a load; only the hours warning is replayed from the seed.
- **Who speaks.** A controller's call is made by `SayFrom` and by an order whose `by` names a post of the scenario; B5 gives the
  posts their decisions and maps its `C2Message` kinds onto the call kinds. The command link's ground transmitter (a carrier with timing only),
  the FAC and ASOC request nets and the E-3A's orbit are posts the data can place; none is placed by the raid builder yet, and
  the builder puts no GCI posts in a session (B5, B8).
- **The enemy's listeners** are data and a function; no caller places them in the campaign screen's `EmRadio` yet, and the
  Teaball-type relay of section 6 and the dummy traffic of section 8 are not built.
- **Posts under attack.** A post is not bombed, jammed or moved in the session; its chief and liaison are the chain's.
- **A canary.** No test yet proves that nothing outside the debrief shows the intercepts; the session's only outputs are the journal's
  entry, the delivery's paper and `MissionSession::Desk()` for the debrief.
- **The word of 1985.** The words are generic plain language with call signs and clock codes; the TRADOC glossary's words are not used
  (the words a flight says are its kind's), and none of section 1's open items has moved.

## As built (S6, 20 November 2026, branch `tbr-s6`)

Roadmap S6 closes most of B9's Left under a new save mode, `radio.mode` (the save's `"radio_mode"`; a save without it was
made before S6 and replays at 0, so the replay fingerprint of `test_routes.cpp` is unchanged). Every new draw is keyed on the
seed, the package and the call with stable string hashes (`AbstractKeyOf`), on salts of its own: no existing stream draws
more in an old mode.

**The abstract level's calls (A0, section 2.4).** `air_war/abstract_radio.hpp`. When the campaign resolves a package no session
flies, `AbstractPackageCalls` gives its flights their calls at their route's points by the session's rules: the entry's
"airborne", the gate's check-in, a strike's "IP inbound, target <square>" (by signal for a comm-out flight), a threat call at the
target when the flight lost or had damaged aircraft, the codeword or "off target" after the attack, "RTB" at the exit, "aborting"
at the gate for an aborted flight, nothing after the target from a flight all lost; at most `radio.abstract_calls` a flight. A
comm-out strike says nothing routine before its attack, and past it a strike keeps routine calls off the air with its
discipline's chance (section 3.2's rule, with the officer's chatter and loose lips). The messages of the enemy's command about
the package (`c2.hpp`'s log, `C2CallKind`) are calls too (`AbstractC2Call`): said by the post at its place (an E-3A at
`radio.awacs_height_m`, a ground post at `radio.abstract_mast_m`) or by a flight where its route had it; requests and their
answers on the HF request net. `HearAbstract` is section 4.2's horizon in closed form: a VHF or UHF call within the 4/3-earth
horizon between the speaker's antenna and our 30 m mast (`RadioHorizonM`, `vr_1989_tr1`'s 4.12 sqrt(H)) and never beyond
`radio.abstract_range_m`; an HF call within `radio.abstract_hf_range_m` by the post's HF set only; with chance
`radio.abstract_hear`; the bearing with the VHF DF's error, `radio.df_sigma_deg` 3.5 degrees (`fm_100_2_1_1984` p. 15-2).

**Into the pipeline.** `CampaignFeed::UseIntercept` (command/air_war_feed.hpp) puts the posts of `data/command/intercept.json` at
their chain places in the theatre frame (a VHF/UHF and an HF receiver each, as in a session) and runs an `InterceptDesk` for the
campaign: the same write-up (linguists, slips, continuity, codeword habits; the new `WriteUpCall`) and the air section's day
summary at `daily_summary_hour`, a `FactKind::Intsum` document from `army_intel` for the regiment (key
`intercept_intsum_campaign:<n>`), now with the squares the read calls named. The abstract calls take only this slow road (section
2.4), never the fast one. The forms are written in order of time up to a watermark (the campaign's hour, held back to the first
point of any package not yet resolved), so a feed pumped after every step and one pumped once after a load write the same forms
in the same order. A played package is left to its session's posts.

**The facts rebuilt on a load.** `HeardRecord` keeps the campaign hour heard and written, the bearing, the precedence, the chain
post and whether the square was the battery's (filled by `MissionSession`'s heard source; saved when known). The feed reads every
`Complete` action's `outcome.heard` and makes the facts the session made: a read call on the fast road an `Intercept` fact from the
post's liaison (`intercept_played:<package>:<call>`), a heard report naming the battery's square an `EnemyKnowsBattery` fact for the
regiment's chief of staff (`intercept_played_knows:`), one per call whichever posts logged it. A save made before S6 has no hours:
the package's time over target stands in. Since the feed is a function of the campaign's history, a load gives the same facts.

**The enemy's listeners on the campaign screen.** `PlaceCampaignListeners` (air_war_feed.hpp) places the data's receivers for the
campaign's enemy on the campaign's `EmRadio` from `radio.mode` 1 (none before, and the enemy hears everything, as in P3); the
campaign screen (`USimAirCampaignSubsystem::StartInfoNet`) calls it and gives the feed the intercept data. The data's listeners
were inside our country in B9; they now stand behind each front: the Pact's front intercept post 160 km north (30 m) and an Il-20
on its orbit (8,000 m); NATO's ground post 200 km west and an RC-135 orbit (9,000 m); the rebels' post unchanged.

**Teaball's relay (section 6).** A faction's `radio.teaball` block (`on`, `delay_s`, `fail`, `error_m`, its source). NATO's is
written and **off**: a Teaball-type warning service for Europe by 1985 was not found, so turning it on is the owner's choice.
In the abstract fight (`air_fight.hpp`), with the relay on and our GCI not jammed, the surprise of our pass is lost with chance
1 - `fail`, on a draw of its own. In a session (`air_fight_def.hpp`'s `teaball`, the scenario's `air_fight.teaball` naming an
enemy controller as `relay`): each vector our controller says is overheard when the two antennas are within the radio horizon of
each other, and `delay_s` later, unless the relay fails, the relay's controller warns each of its fighters it may vector of our
fighter where he was, with `error_m` of error, on its own net (`AirFightSession::TeaballOverheard`, `TeaballWarnings`). The raid
builder puts no enemy controller in a session yet, so the relay is not in a generated raid.

**Deception with dummy traffic (section 8).** Ours: each day `radio.our_dummy_corridors` false corridors, a working airfield's real
corridor turned `radio.our_dummy_turn_deg` about the airfield (the side alternating by day), are used on our GCI's and the notices'
nets at the real one's hours (`AirCampaign::FeedDummyCorridors`), so the enemy's corridor learning of S1 can believe a corridor
where our fighters never fly; a raid timed into one gets no held fire (`own_corridor_found` false). `vr_1985_plss` (dummy
transmitters, imitated traffic) and `nazarenko_1984` (radio disinformation and imitation, tier 1, a Czechoslovak digest of
*Военная мысль* 9/1984) are the practice; the count and the angle are game values. Theirs: a deception package's flights, or a
decoy flight, talk as a strike would (the IP call naming a square) with the faction's `radio.demonstration` share (the Pact and
NATO 0.5: `nazarenko_1984`'s radio demonstration, NATO's air nets in the Vintex-75 feint; the rebels and the neutral 0, nothing
found), plus `radio.demonstration_quirk` with a feint artist or a decoy man planning or commanding it, so our analysts read a raid
where there is none.

**The canary.** `test_air_radio.cpp`'s canary: an enemy call with a marked call sign is said near the battery in a mission session;
every output a player's character could meet (the battery's nets, the radiotelephonist's journal, the deliveries and papers, the
commander's log, the radar company's log, the crew's calls, the code groups heard, the reports and orders made) is searched for
it; it may appear only as the regiment's telling of the intercept post's report and in the air section's summary, and must appear
there. In the campaign, every fact made of the enemy's calls is held by a person of the chain, and the player's character has it
only from someone who told him.

**Tests.** `test_air_radio.cpp` (S6): the abstract calls' rules and the demonstration share; the horizon, the HF set and the DF
spread; a campaign's abstract raids into the day summary, identical pumped step by step and once after a load, silent at mode 0;
a played session's records rebuilt from the save and none from a save made before S6; the listeners placed from mode 1 only; the
canary; the B9 listener test moved to the new places and the save test extended to the new fields. `test_air_fight.cpp` (S6): the
relay in the abstract fight (off in the data, on, always failing, mode 0) and in a session (within the horizon and beyond), and its
scenario block. `test_own_air.cpp` (S6): the dummy corridors heard and believed, none at mode 0.

**Balance.** See `docs/air_war/09_BALANCE.md` section 23.

**Left.**
- **The fast road for abstract calls.** The abstract level feeds only the day summary (section 2.4); a heard divert naming the
  battery in an abstract raid does not reach the battery in minutes.
- **A1.** A flight flown coarsely in a session is still one node per aircraft.
- **Teaball in a generated raid.** The raid builder places no enemy controller (B5, B8's Left), so the relay works only in a
  hand-written fight; NATO's relay stays off until a source or the owner says otherwise.
- **Our dummy traffic in a session.** The false corridors are traffic in the campaign's learning only; no dummy transmitter is
  placed on our GCI's net in a session (`EmWorld::AddDummyTraffic` is not called from the battery's scenario).
- **The codeword's word.** The session draws a squadron's codeword with `std::hash`, the campaign with a stable hash: the same
  squadron can have different words in the two, so a habit learned in one is not the other's.
- **The listeners under attack.** The enemy's listeners are fixed; none is struck, moved or put on a schedule (`from_h`,
  `until_h`).
- **The airborne listener's reach.** `EmRadio` takes the loss past the horizon as one knife edge over the earth's bulge, which
  loses too little (about 24 dB at 670 km): the Il-20 at 8,000 m hears a 3 m whip far past its 375 km horizon, and an RC-135
  likewise. Smooth-earth diffraction (ITU-R P.526) in the EM layer would bound it; until then the airborne listeners hear
  most of the country, as the enemy did with none placed before S6.
