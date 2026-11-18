# 8. Our fighters and the battery

Code: new `simcore/command/own_air.hpp` (the cooperation method, zones, corridors, flight notices, the IFF code
schedule, the fighter regiments' duty rule table), new `simcore/command/own_flights.hpp` (our flights as actors under our
GCI); changed `command/info_net.hpp`, `command_post.hpp`, `engagement.hpp`, `em/model.hpp`, `em/radar_system.hpp`,
`scenario.hpp`, `scenario_runner.hpp`, `battery_fire.hpp`, `campaign/battery_task.hpp`, `campaign/report.hpp`,
`air_war/resolve.hpp`, `air_war/planner.hpp`, `air_war/intel.hpp`, `air_war/test_waves.hpp`; data in
`data/command/own_air.json`, `data/command/iff_codes.json`, `data/command/chain_pannonia.json`,
`data/campaign/tasks/`, `data/gameplay/own_air.json`. Rests on the research `our_side_air.md` (main), `air_combat.md`
(the Pact's GCI and Turchin), `aam_weapons.md` (our MiG-21 and MiG-23 weapons), `comms_emcon.md`, and the code maps
`map/air_to_air.md` (main) and `map/c2_people.md`, all in `docs/research/air_battle_2026-11/`.

This doc is about our fighters as the battery meets them: aircraft in its sky that it must not shoot, told to it late
and by paper, answering an interrogator that is right only while both sides hold the same code. It widens roadmap 17.2
(section 12 says exactly how). How our fighters fight the enemy's in the air is `07_AIR_TO_AIR.md`; this doc gives them their duty,
their ground control and their place in the sky beside the SAMs.

Turchin's article is `mt_turchin_coop` throughout this set and in `air_combat.md`, one id, registered in
`docs/SOURCES.md`.

## 1. What is there today

- **Our fighters are one number.** `FrontDef::friendly_fighters` (0..1 per front, `data.hpp:158`) sizes the enemy's
  escorts (`planner.hpp:733-753`) and is a flat per-aircraft loss chance on unescorted main flights at their second route
  point (`resolve.hpp:231-247`, `attrition.escort_fighter_kill` 0.03). Our fighters are never lost, never placed, and
  struck air bases do not change the number. The loss is written with `LossRecord::by = ""`.
- **The order of battle has them, and nobody flies them.** `oob_pannonia.json:189-203`: three fighter regiments (MiG-21
  ×36, ×36, and ×24 with MiG-23ML ×12) under the National Air Defence. The post `fighter_rgt_duty` (Capt Szilagyi,
  Kecel) is in the chain (`chain_pannonia.json:18-21`) and no fact passes through it.
- **The testbed's flights are scenery.** `TestWaves::FriendlyFlight` (`test_waves.hpp:557-603`) flies CAP orbits and
  intercepts with `friendly = true`, `board_type = 1`; they engage nothing.
- **IFF is a binary transponder.** The Osa-AK has ЗАПРОС, ЗАПРОС К and КЛАПАН (`osa_ak.json:242-246`,
  `EngagementModel::UpdateIff`, `engagement.hpp:2273-2340`); with the EM layer a friend replies by
  `t.iff_reply && t.side == p.side` at 0.9 a look (`em/radar_system.hpp:399-400`). `em::Platform::iff_reply` says "codes
  and modes not modelled yet". Firing at a friend is possible and logged as a deviation (`engagement.hpp:915`). The NPC
  commander never launches at a `KnownFriend` or a friend he was told of (`scenario_runner.hpp:8767-8775`).
- **The PU-12M's picture takes identity from the truth.** `BoardContact::type = t.board_type >= 0 ? t.board_type :
  (t.friendly ? 1 : 9)` (`scenario_runner.hpp:6600, 6622`). Only the cabled radar's interrogator is honest
  (`scenario_runner.hpp:6293-6301`; the P-19's 1L23-6 for Kremniy-2/2M and Parol-4, `regiment_radars.json:14`).
- **The neighbours never err.** The battery fire model and the testbed's neighbours skip `t.friendly` by the truth
  (`battery_fire.hpp:337`, `scenario_runner.hpp:2282`).
- **The words exist and are never sent.** ASPD code 40 "own aircraft in the zone", 20 "fire ban", 21 "fire permitted"
  (`aspd_codes.json`, placeholders); talk-table group 402 «В зоне свои самолёты» (`talk_tables.json:19`). The task's
  coordination is free text: "our fighters' corridor north of Kesmark" (`bty21_cover_bridges.json:41`,
  `BatteryTask::coordination`).

## 2. How the Pact shared the sky (the period)

### 2.1 Two ways, and when each was used

The Warsaw Pact's own staff journal (Mitronin, 1976) gives two methods (`mitronin_1976` pp. 5-6):

- **By zones.** The SAM unit's zone is everything inside the far edge of its launch zone; the fighters own the rest and
  cross the SAM zone "in compliance with a prescribed procedure". Simple, safe for our aircraft, but it scatters effort
  and is hard to keep when SAM units move.
- **In one zone**, the effort shared out by heights (upper levels preferably to the fighters), by axes or sectors, by
  control lines (one after the other), by time, and by targets, in combinations, set before or during the raid. It needs
  reliable control and identification, or the SAMs' fire is restricted for fear of hitting our aircraft.

By zones was used against the first massed strike, while the SAM formations are unchanged, over slow-moving covered
objects, and as the main method in a fast offensive; one zone shared by height when the ground air defence cannot engage
every target of a massed strike. Plans provide for both (`mitronin_1976` pp. 6, 8-9). Lavrinovich adds that in a moving
battle "the fighter aircraft should be given preference in attacks on targets", because fighters cannot see the SAM
zones and the ground units have identification means, "although they are not sufficiently reliable"
(`mt_lavrinovich_mdex` p. 16). Turchin had argued for one zone, against zones, a decade earlier (`mt_turchin_coop`
pp. 10-12). SAM3D uses Mitronin as the rule, Turchin for the geometry, Lavrinovich for the offensive.

### 2.2 Keep out, or fly above

The regiments' own air defence sits inside the SAM zones and "frequently do[es] not have identification systems", so
fighters should not work in its fire zones; over dense air defence they get a height band above the SAM troops' upper
kill limit (`mitronin_1976` p. 7). When some SAM units cannot fire (on the move, out of missiles, faulty), the fighters'
floor of unrestricted use over their zones drops to **3,000 m** (p. 9). For an Osa battery our fighters are therefore
mostly aircraft passing through on a notified route and height, or fighting above it.

Hrbek's 3,000 m (`vr_hrbek_1991` p. 32) is a different number: a 1991 planning assumption that a division's air defence
is one continuous fire zone up to 3,000 m for a proposed unified system. It is after the period and is not used.

### 2.3 Corridors

"In the majority of cases, corridors are established in order to ensure the safety of the flights of our aircraft"; the
enemy may find and use them, so there should be several, covered by units that have identification means
(`mitronin_1976` p. 7). The 1984 regulations put "the corridor of passage and the duty zone of our aviation" on the radar
company commander's working map (`pvo_bu_x_1984` art. 37) and the AA artillery battery commander's (`pvo_bu_ix_1984`);
the 1991 Osa textbook puts it on the Osa battery commander's (`voenizdat_osa_1991` p. 244). Turchin opposed corridors:
they delay fighter-bombers, do not suit fighters under guidance or returning aircraft, form a pattern, and let the enemy
concentrate there (`mt_turchin_coop` pp. 10-12). SAM3D keeps corridors (the 1984 regulations) and gives Turchin's
objection to the enemy as the ability to learn and use them (section 9).

### 2.4 Who controls, and where they sit

A fighter combat control centre sits at the front air defence command post under the air army's deputy commander; each
army's centre has two or three **fighter guidance and target designation posts (ПНЦ)** co-located with SAM brigade and
regiment command posts; combat control groups go to first-echelon division command posts. "The commander of the
surface-to-air missile unit in control of its firing and the officer guiding fighter aviation must be at the same display
unit (plotting board)" (`mitronin_1976` pp. 4, 7-11; Turchin the same, `mt_turchin_coop` p. 13). Fighters stay under
the air army, not the air defence chief (`mt_turchin_coop` p. 12). On home ground at the start of a war the **national**
air defence organises the cooperation and covers the field army's troops, its control system being "stable and
automated" (`mt_rozhdestvenskiy_depth` pp. 8-10). In the national air defence, automation cut the time to pass
information to a fighter regiment's command post from 3-5 min to about 10 s (`vizh_tikshaev_pvo`, a 2023 history); the
field army's batteries were not on it.

For our nation (`docs/campaign/10_BRIEFING_FORCES_AND_NATION.md`): the fighters belong to the National Air Defence and
the sector command post (`sector_cp_duty`) controls them; the army's Osa regiment learns of them through the army's
chain.

### 2.5 Flight applications, and how late they arrive

A report on our aviation to the SAM and gun command posts must hold the time, directions and heights of our aircraft,
the times they cross the SAM and gun zones, the expected time back at their airfields, and the areas and heights of
airborne duty, for planned and actual flights, kept up as decisions change (`mt_franchuk_front` p. 6). The authors add
that "up to now it has not been decided by whom, how, and by which means" it is to be passed. The regulations call them
applications for flights (заявки на полёты своей авиации); radar units compare what they see with them and report
violators (`pvo_bu_x_1984` art. 56).

| Step | Time | Source |
|---|---|---|
| Our aircraft's actions, division air defence chief to the guns, by code table (Druzhba-76) | 10-15 min | `mitronin_1976` p. 11 |
| A low target seen by a P-15 before the forward edge | 4-5 min | `mt_franchuk_front` p. 5 |
| Reports passed by hand to the air defence chief's post; then to the active means' plotting boards | 2-3 min; about 1 min more | `mt_franchuk_front` p. 5 |
| Warning data through the intermediate levels (the UAR, 1967) | 5-8 min and more | `mt_umnov_1968` p. 8 |
| Fighters controlled from plotting boards (North Vietnam) | 3-6 min delay | `mt_1973_nvn` p. 14 |
| A low-level raid, detection by our radars to the first-echelon troops (ČSSR, 1988: after the period, hindsight) | 3-5 min | `vr_roucka_1988` p. 18 |
| Pact readiness 1 / 2 / 3, to take-off (NATO's view; the chart is for fighter-bombers) | 3-5 min / 15 min / 1-2 h | `fm_100_2_1_1984` p. 12-7 |

The notice is slower than the enemy. A pair scrambled from readiness 1 against a low raid is over the battery before its
notice reaches the guns. "Delayed data on a probable target is equivalent to letting the target through"
(`andersen_1979` p. 12); delayed data on our own fighter is the friendly fire of roadmap 17.2.

### 2.6 What the battery commander must tell and do

The 1984 regulation for the troop air defence's AA **artillery** battery (part IX), used for the Osa battery by analogy
as `docs/SOURCES.md` registers it (its articles were not re-read in the check of 15 Nov 2026):

- cooperation rests on every man knowing the control and recognition signals, on **timely delivery of the flight
  applications to every fire unit**, and on the commanders' watch over our aircraft near targets under fire (art. 15);
- the commander tells all personnel the time, place, height and direction of our aircraft's flights, the fighters' duty
  zones and types, and the current recognition and fire-prohibition signals, and updates them as the senior commander
  sends them; he identifies detected aircraft with the identification system (art. 34);
- from the senior commander's information he drops the targets our fighters are attacking, or lowers their priority
  (art. 45);
- the crews recognise our aircraft by their features and **the way they behave when crossing fire zones**, follow them on
  the scopes and by eye, and report at once when one of ours is close to a target about to be engaged (art. 53);
- before the battle he gives the order for cooperation with fighter aviation (art. 68).

The radar regulation (part X) adds: the commander gives the radar identification code and the visual recognition signal
(art. 41); every target is identified at once, again at intervals and whenever tracks split, join or cross (art. 53);
our aircraft's distress data go out of turn (art. 55); the commander checks the timely change of identification codes,
talk-table keys and the PVO grid (art. 17); every man must tell our aircraft from the enemy's by sight (art. 19);
positions go by coded squares of the PVO grid (art. 30) (`pvo_bu_x_1984`). The Osa textbook has the crew chief assess
"the position and actions of our aviation" and the battery commander weigh them in his fire decision
(`voenizdat_osa_1991` pp. 251, 258).

### 2.7 IFF in 1985: Kremniy-2M, and no launch block

- **Kremniy-2(M)** interrogates in band III (668 MHz); 12 plug-in code filters, 1-8 for peace, 9-12 kept for war,
  filter 4 for alignment; filters changed "on a secret, pre-arranged schedule every one to six hours"; a distress reply
  by answering twice; unencrypted, so it could be intercepted and its transponders triggered by US interrogators
  (`soviet_iff`, tier 4).
- **Parol** entered Soviet service in 1977 (unverified: a search excerpt of a dead page); the East German change-over,
  planned for 1989-90, was not carried out (`dewiki_p15`, a pointer only; its source says only "planned"). A small Pact
  nation flies Kremniy-2M in 1985. Our MiG-23ML carries the SRO-2M (`airwar_ru`, MiG-23ML).
- **The Osa's interrogator.** The Osa-AKM's 1L24-2 works with Parol, Kremniy-2 and Kremniy-2M; mode I is "a limited
  number of codes set by schedule", not imitation-resistant; mode II (Parol) is guaranteed. Azimuth resolution no worse
  than 30°, range resolution 0.9-1 km. **Launch blocking works only in mode II**: a guaranteed friend in the 1 km gate
  breaks the launch circuit for ±1.9 s each antenna pass (`voenizdat_osa_1991` pp. 17, 53-56, 64). So our Osa-AK has no
  launch block: the senior operator's reading of the reply on the PPI (`voenizdat_osa_1991` p. 254) is the only guard.
  That the AK's own interrogator works the same way is an inference: its type and modes are not confirmed (Left open).
- A warning "in no way compensates" for missing recognition devices, and warning the troops of our flights reveals them
  to the enemy (`mt_turchin_coop` pp. 10-12).

### 2.8 The cost both ways

- **Fire on our own.** In a Baltic Military District exercise, "under the existing system of recognition and warning",
  up to 25% of the targets fired upon by SAMs and guns were our own aircraft (`mt_turchin_coop` pp. 11-12; the sentence
  is awkward, the share-of-targets reading the likelier). Umpires counted "all fighters attacking targets at the moment
  that missiles were being launched against these targets" as destroyed by our own SAMs (`mt_lavrinovich_mdex`
  pp. 11-12). Soviet crews in Egypt shot down three Egyptian combat aircraft and damaged an airliner, blamed on old IFF
  (`vedomosti_2016_egypt`, tier 4, about 1970). A TAAG Yak-40 was shot down near Lubango in 1982 by an S-125 or S-75
  (`kuito_memoirs` p. 295).
- **Holding fire.** North Vietnam, 14 July 1966: two MiG-21s were lost partly because the guns "were not authorized to
  deliver fire on the enemy aircraft because their own fighters were in the air" (`mt_1973_nvn` pp. 13-14). Syria, June
  1967: aircraft destroyed while landing "at a moment when antiaircraft artillery could not fire because its own planes
  were in the air" (`mt_umnov_1968` pp. 10-11).

The game must make both costs real and let neither be removed by a rule that knows the truth.

### 2.9 Our fighters under ground control (`07_AIR_TO_AIR.md`)

Our interceptors are brought in by GCI (on the MiG-23ML also by the Lazur-SML command link) from the sector's command
post; their own initiative starts at contact, and the 1984 reforms are the best case (`air_combat.md` our-01, wp-02,
wp-04; `pennington_1985`). Lose the control post, or have the voice nets jammed, and they fall back to independent search
in pairs, or abort, as the Syrians did over the Bekaa (`lambeth_1984`, `grant_2002`). The MiG-23ML's radar finds a target
at 80-85 km under ground guidance and 50-55 km alone (`airwar_ru`, MiG-23ML); its R-24 was exported to the Pact allies
(`mis`; the allies' holding by 1985 unverified); our MiG-21s carry the rear-aspect R-13M and R-60 (`aam_weapons.md`
aam-our-01). A GCI system guides only so many groups at once; fighters beyond that fly "a sortie for nothing"
(`mt_1962_front_aviation`, the principle, not its number). Turchin's geometry near the SAM zone: fighters on airborne
alert at 1,500-2,000 m against low targets and 10-12 km against high ones, the zones' centre 50-60 km inside our
territory; fighters in independent search break off 25-30 km beyond the front line; against targets faster than
1,300 km/h a fighter is brought to a start point no closer than 6-8 km and launches at 4-6 km (`mt_turchin_coop`
pp. 6-10, 16). `07_AIR_TO_AIR.md` flies the fight; this doc decides who sends the fighters, where, and who may tell them to stop.

### 2.10 Per faction

| Faction | How its ground air defence knows its own aircraft in 1985 | Better or worse, and why |
|---|---|---|
| **Our side** (Pact pattern) | Notices by code table down the army's chain; Kremniy-2M (mode I only, changed every 1-6 h); no launch block on the Osa-AK; the regiment's ZSU and Strela by sight and notice (no source found for their interrogators) | The worst placed: slow notices, a spoofable code, the 25% exercise figure as its upper anchor |
| **Warsaw Pact** (enemy) | The same doctrine (Mitronin); Soviet units with Parol from 1977 (unverified) and, with the Osa-AKM (Soviet service from 1980), the automatic launch block in mode II | Better than ours for Soviet units; its allies on Kremniy share our problem |
| **NATO** (enemy) | Positive and procedural management: weapons free, tight, hold by area, type and time; minimum risk routes; a coordinating altitude; Mk XII with Mode 4 by code book; Stinger and the FAAR with interrogators, Chaparral and Vulcan by eye against hostile criteria (`fm44_1_2_1984` pp. 8-11, 9-1; `fm44_3_1981` ch. 5; `fm_100_42_1976` ch. 4) | Better procedures and a secure mode, but Mk XII "has operational problems and is subject to spoofing" (`cbo_1986_amraam` ch. 2 n. 5) and NATO's mixed IFF bound its fighters to visual rules (`lambeth_1985`) |
| **Neutral** | Guns and gun-armed jets only (no missiles until the 1990s on the Austrian analogue, `austrian_af_wiki`, a pointer only); identification by sight and procedure | A design inference, low confidence: little to identify and little to fear from its own guns |
| **Rebels** | No IFF; their few aircraft protected only by time and place | A design inference, low confidence |

## 3. Design: our fighter regiments by a rule table

Our air force stays out of the planner (the roadmap: "our air force as a planner is not planned"). Our fighter regiments
get **duty, alert and scrambles by a rule table**, read on our side's picture, with no utility search and no package
building.

`command::FighterDutyRules` (loaded from `data/command/own_air.json`) holds rows of the form *condition → posture*:

| Row | Condition (our knowledge only) | Posture | Source |
|---|---|---|---|
| Readiness states | always | readiness 1: take-off in 3-5 min, held 1-2 h; 2: 15 min, held 2-4 h; 3: 1-2 h | `fm_100_2_1_1984` p. 12-7 (NATO's view, a fighter-bomber chart; used by analogy) |
| War day | daylight, the war on | per regiment, so many pairs at readiness 1 and 2 | a game value |
| Raid warning | an air raid warning fact at `sector_cp_duty` | more pairs to readiness 1 | a game value |
| Massed raid expected | the sector's estimate of a massed blow | airborne alert: a pair in a low zone at 1,500-2,000 m and a high zone at 10-12 km, centre 50-60 km inside our territory | `mt_turchin_coop` pp. 6-7 |
| Scramble | a raid track on the sector's picture heading for an object of the regiment's area, a GCI channel free | the readiness-1 pairs nearest in time, so many pairs per enemy group | a game value (Turchin's groups of 4-8 as the enemy's unit) |
| Pursuit limit | a flight in independent search | break off 25-30 km beyond the front line, report it by radio | `mt_turchin_coop` p. 9 |
| Recall | fuel, weapons spent, GCI lost and the leader's skill low | home by the return corridor | game values; `07_AIR_TO_AIR.md` §4.5 |

The inputs are facts our side holds: the sector's air picture (our radars and warning posts, with their delays and
errors), the war's state from our journal, each regiment's aircraft and pilots, and its base's function (`kecel_airbase`,
`tiszavar_airbase` as `TheatreObject`s, so a struck base launches fewer and later). Never the enemy's plan. Draws come
from a stream keyed by regiment and hour (`rng_.Fork`, decision 6), so a save replays.

`command::FighterRegimentState` keeps aircraft by state (readiness 1/2/3, airborne, down, lost) and pilots as people with
skill, fatigue and nerve, as the enemy's pilots are (track B's people model). Losses and kills go into the regiment's
own record (its report through `fighter_rgt_duty`), as claimed.

## 4. Design: each flight a live actor under our GCI

`command::OwnFlight` (in `own_flights.hpp`): a pair or a four with a leader, a callsign, a type (MiG-21MF/bis, MiG-23ML),
loads, fuel and an IFF state (transponder on, the filter set). It moves through *on the ground → take-off → transit by
corridor → on station (duty zone) or vectored → contact (`07_AIR_TO_AIR.md`) → break-off → return by corridor → landed*. In the
session each is a `FlightState` with `side` "ours" (`02_FLIGHTS_AND_PILOTS.md` §1.6). Track B's levels apply: A2 inside the session's play circle, A1 flown coarse outside it, A0 in the campaign (section 11).

`command::OwnGci` is a post of our chain with a place, a controller (a person with skill), a number of **guidance
channels** (a game value, after `mt_1962_front_aviation`'s principle) and a picture: the sector's tracks, about 10 s old
when automated (`vizh_tikshaev_pvo`), 3-6 min on a manual board (`mt_1973_nvn`); the data sets which. Two posts are new in
`chain_pannonia.json`: `sector_gci` (the sector CP's controllers) and `pnts_<n>`, a guidance post co-located with the
national air defence's SAM brigade command post at the same plotting board (`mitronin_1976` p. 10).

- **Orders by voice.** Vectors, heights, "contact", "break off", "home" go on the GCI net as EM transmissions (the
  R-832 class, 100-150 and 220-400 MHz, `valka_r832m`, low confidence), so the enemy's COMINT can hear them and its
  comms jammers (16.2, `air_war/comms_jamming.hpp`) can deny them. The MiG-23ML's Lazur-SML link is a separate emitter
  with no voice in it (`comms_emcon.md` comms-23, an inference).
- **The picture, not the truth.** The GCI vectors on its own tracks. A flight beyond the channels, or with its net
  jammed, searches on its own with its own radar's shorter reach; by the leader's skill it keeps searching or aborts
  (`air_combat.md` our-01, wp-09).
- **Steering.** A2 and A1 flights are session aircraft steered through the runner calls that steer the enemy's flights,
  `ScenarioRunner::OrderFlight` and `RerouteFlight` (`02_FLIGHTS_AND_PILOTS.md` §2.3, without the trainer's two-target
  cap), with `by` the GCI post's id; they are not authored waypoints.
- **What the battery learns of their fights.** A kill or a loss is a fact at `sector_gci` or `fighter_rgt_duty` and
  reaches the battery only by the pipeline (`air_combat.md` our-03), as the neighbours' kills do today
  (`ScenarioRunner::RegimentTellsDestroyed`).

## 5. Design: the sky shared, in the battery's order and on its working map

`command::CooperationPlan`, issued with the national air defence's daily plan and passed down as combat instructions:

```cpp
enum class CooperationMethod { ByZones, OneZone };
struct OwnAirZone {                 // a corridor, a duty zone, or a SAM zone's edge
    std::string id, kind;           // "corridor", "duty_zone", "sam_zone"
    std::vector<Vec2> outline_m;    // ENU, from PVO grid squares
    double floor_m = 0.0, ceiling_m = 0.0;
    double from_s = 0.0, to_s = 0.0;
};
struct CooperationPlan {
    CooperationMethod method = CooperationMethod::ByZones;
    double split_height_m = 0.0;            // OneZone by height: fighters above, SAMs below
    std::vector<OwnAirZone> zones;          // several corridors, duty zones
    double fighter_floor_m = 0.0;           // above the regiment's highest kill limit
    double floor_unable_m = 3000.0;         // over units reported unable to fire (mitronin_1976 p. 9)
    std::vector<std::string> units_unable;  // as the regiment's records have them
    std::string recognition_signal, fire_prohibition_signal;
    int version = 0;
};
```

- **Which method.** The first massed raid of a war day runs by zones; a later raid may switch to one zone by height when
  the regiment announces it (`mitronin_1976` pp. 6, 8-9). The regiment's tasking (`campaign/regiment.hpp`) chooses by a
  row of `tasking.json`.
- **The fighters' floor** sits above the regiment's highest kill limit (the Osa-AK's as `data/gameplay/osa_ak.json` has
  it). Over units the regiment's **records** report on the march, empty or faulty, it drops to 3,000 m. A battery that is
  up while the record says it is down meets our fighters at 3,000 m: the record's error carries into the sky, as
  principle 9 asks.
- **In the order.** `BatteryTask::coordination` stops being free text: the task file's `"own_air"` block names the
  corridors, duty zones and their times, the method, the signals and the code document's issue. The battery commander's
  plan (`battery_plan.hpp`) draws them on his working map (`voenizdat_osa_1991` p. 244), and his oral combat order gives
  the order for cooperation (`pvo_bu_ix_1984` art. 68).
- **The NPC fire rule follows the method he was told.** By zones: fire freely inside the battery's zone, hold on a track
  in a notified corridor at its height and time. One zone by height: fire below the split, hold above it unless the
  regiment gives the target. By targets: never on a target the regiment says our fighters are attacking (art. 45), the
  regiment's "fighters attacking target N" said on its net. Each act goes through `ScenarioRunner::NpcDoes`.

## 6. Design: notices and codes down the pipeline, into the journal

**The notice.** `command::OwnFlightNotice` carries Franchuk's fields: flight, type and count, route as PVO grid squares,
height band, the times it crosses the SAM zones, its return time, its duty zone, heights and times, and whether it is
planned or actual, with an amendment number (`mt_franchuk_front` p. 6). It is a `Fact` of a new kind,
`FactKind::OwnAirNotice`, originated at `fighter_rgt_duty` for planned flights and at `sector_gci` for scrambles and
amendments, carried `sector_cp_duty → army_ad_chief → div_ad_cp_duty → regiment → battery 21's PU-12M`.

- **Coding is work.** A notice is `coded`: each hop that passes it adds encoding and decoding by the talk table to the
  means' `handling_s` (`InfoNet`, `info_net.hpp:939-985`). The work per hop is data in `own_air.json`, calibrated so the
  division-to-launcher run is 10-15 min (`mitronin_1976` p. 11). An air force liaison officer at the regiment
  (Mitronin's proposal for busy axes, p. 11) is a task option that starts the notice at the regiment, uncoded.
- **Into the session.** A notice due at the battery during a session arrives on the regiment's net as group 402 with its
  squares, heights and times, by a new `ScenarioRunner::RegimentNotifiesOwnAir(notice, after_s)` beside `RegimentSays`.
  The radiotelephonist writes it in the journal (`RadioJournal`: `heard_s`, `written_s`, `said`, `text`, with
  `net = "regiment net"`); the battery commander's NPC tells the crews on the battery net (art. 34), and each crew chief
  tells his crew. The PU-12M's board keeps the notified flight (`CommandPost::NoteOwnFlight`) **from the journal entry**,
  never from the truth; a garbled or misheard square is carried as written.
- **Fire bans.** The regiment's ASPD code 20 (fire ban) and 21 (fire permitted), and code 40 with group 402, are now
  sent: the regiment's tasking sends a fire ban in a sector and window when it decides to hold fire for our fighters
  (section 9), and code 40 with each notice that touches the battery's zone.
- **Violators and distress.** A track answering friend but off every notified route, height or time is reported up as a
  violator, not engaged (`pvo_bu_x_1984` art. 56). A double reply (distress, `soviet_iff`) is reported out of turn at
  precedence 2 (art. 55).

**The codes.** `command::IffSchedule` (from `data/command/iff_codes.json`: 12 filters, 9-12 in war, a period of 1-6 h,
`soviet_iff`) is drawn each day from the campaign's stream. It is a **document**: issued by the regiment's signals chief
for the next day and carried by a means that takes documents (in person, messenger, despatch rider, courier); the
handing-over time is a game value. Each Osa vehicle and the PU-12M hold a copy only if one reached them. At each change
time the crew chief orders the change and the senior operator fits the new filter (a job of a few tens of seconds, a game
value; the AK's real act is not known), with a callout; the commander checks it (art. 17). A battery without the day's
document stays on the last filter it had and says so in the commander's log. Our pilots set their transponders from the
same schedule at briefing and switch by the clock in the air; a pilot who misses a change answers on the old filter, by a
skill chance that is a game value (no period failure rate was found). An unscheduled change, ordered by voice when the
schedule is believed compromised, is a design inference with no source; it is off by default.

**In the record.** Every notice, its amendment, the code document's arrival and each change go into the radiotelephonist's
journal or the commander's log with the time written. Nothing here is a panel: the player learns of our fighters by the
net, the board, his commander's words and his own eyes.

## 7. Design: IFF at the Osa and the PU-12M

- `em::Platform` gains `iff_system` (Kremniy, Parol, MarkXII) and `iff_filter` (0: off). A reply rides the search radar's
  look only if the transponder is on, the systems match, and **the aircraft's filter equals the interrogator's filter**,
  at `iff_reply_probability` per look (`em/radar_system.hpp:399-400`). The side test (`t.side == p.side`) goes: a code
  answers whoever asks with it. The prototype's `em.py` mirrors the change.
- `ScenarioTarget` gains `iff_system`, `iff_filter` and `transponder_on`; `"iff": "friend"` stays as shorthand for "our
  system, today's filter". Without the EM layer `EngagementModel::UpdateIff` applies the same filter test.
- **No launch block** on the Osa-AK (section 2.7). The reply is an arc the senior operator must read; with the AKM's
  30° and 1 km resolution as the working figure, a friend close behind a foe smears his reply over both, and the NPC
  reports what he saw, not who answered.
- **The enemy's transponders.** NATO aircraft answer only Mark XII. A Pact enemy's Soviet aircraft on Parol give no reply
  to Kremniy unless they still carry a Kremniy set (not known; default no reply). The Pact's **allied** aircraft fly the
  same Kremniy hardware over the same four war filters on their own schedule, so when both schedules happen to hold the
  same filter an enemy MiG answers friend: an inference from the 12-filter hardware, flagged and on by default in a war
  with the Pact (Left open). Our MiG-23ML and the Pact's MiG-23MLD are the same type on both sides; SAM3D lets the TV
  tell them apart only by markings at short range (a design choice).
- **The enemy reading ours.** A NATO fighter that can trigger Kremniy transponders (QRC-248, APX-80/81 "Combat Tree")
  would identify our fighters beyond visual range by their replies: `soviet_iff` (tier 4) says so, while `air_combat.md`'s
  check found no official or period source (iff-02). It is a per-type flag, off by default, an input to `07_AIR_TO_AIR.md` (its `cooperative_id`).
- **The PU-12M.** `BoardContact::type` comes from the regiment's own interrogation on its copy of the schedule and from
  the notices it holds, never from `t.friendly`. The cabled P-19's 1L23-6 follows the same filter test. The ДХ keypad's
  СВ and foe keys stay the commander's by hand.
- **The neighbours err as we do.** The skips of `t.friendly` in `battery_fire.hpp:337` and `scenario_runner.hpp:2282`
  are replaced by each neighbour's own knowledge: its copy of the notices (by the pipeline, to its own post) and its
  interrogator if it has one; the ZU-23 and Strela by sight and notice only. A neighbour's fratricide is logged as its
  own.

## 8. Design: friendly fire, traceable in the record

Friendly fire is never rolled. It follows when the battery fires on an aircraft it has no reason to think is ours:

1. a scramble whose notice is still on its way (section 2.5: 10-15 min against a 3-5 min raid);
2. a notice that never arrived (a net jammed, a post lost, a coded group garbled, a battery moved and the hop failed);
3. a code document that did not arrive, a change missed by the crew or the pilot, a transponder off or failed;
4. our fighter attacking a target the battery is engaging (`mt_lavrinovich_mdex` pp. 11-12): the burst's damage is drawn
   for every aircraft near it (`damage.hpp`), ours included; the crew's art. 53 report "one of ours close to the target"
   uses Turchin's 600 m (`mt_turchin_coop` p. 11, a requirement never fielded) as SAM3D's threshold;
5. an aircraft off its notified route or height that answers no code (a damaged fighter coming home low).

**Traced.** The after-action report (`campaign/report.hpp`) lists our notified flights against the engagements from the
journal, the board and the commander's log. The fighter regiment reports its loss through `fighter_rgt_duty` (time,
square, cause as its pilots and controllers believe it); the regiment's check (P9) sets that against the battery's claim.
Where the records meet (a launch at 07:14 in square 4512, a MiG-21 lost at 07:14 in square 4512), the regiment's trust in
the battery falls, unless the battery's journal shows the notice came after the launch or not at all: the record says why.
The debrief, from the truth, shows the chain: the notice sent from Kecel at t0, through each post at t1, t2, t3, the
launch at tL, the filter each side held.

**Calibration.** A headless exercise sweep (`own_air_balance`, below) is the check: with 1960s conditions (notices only,
no interrogator, 10-15 min delays) the share of our own aircraft among the targets fired on should reach the 25% anchor
(`mt_turchin_coop`); with working codes and timely notices it should be far lower. The low band's target is a game value.

## 9. Design: the enemy uses our corridors and our holding of fire

- **It learns them.** Corridors and our fighters' return windows become an `IntelEntry` kind (`air_war/intel.hpp`):
  believed path, heights and time windows, with a sigma. Sources: COMINT of our notices and GCI voice
  (`CampaignFeed::BatteryCalls` → `AirCampaign::NoteTransmissions`, now with our GCI and regiment nets), talk-table groups
  broken by a chance that is a game value, and traffic its AWACS or ELINT saw (`docs/air_war/11_SUPPORT.md`).
  Turchin's warning that warnings reveal our flights (`mt_turchin_coop` p. 12) is the mechanism.
- **It uses them.** The planner prices a believed corridor's legs lower when the target lies past it and times the TOT to
  a believed return window, the Syrian landing case of 1967 (`mt_umnov_1968` pp. 10-11) as the model. Mitronin warns of
  it (p. 7); that a 1985 enemy planned it is not documented, so it is gated by the planner officer's creativity and the
  belief's age, a design inference.
- **Holding fire costs.** When the regiment bans fire for our fighters (code 20 in a sector and window), raids inside it
  meet only the guns that see them by eye, as in North Vietnam in 1966 (`mt_1973_nvn` pp. 13-14). The regiment's choice
  between a ban and "fire on non-replying targets" is a row of `tasking.json` per adversary; both costs show in the
  outcomes' ledgers (`campaign/outcomes.hpp`).
- **Several corridors.** The task may name several and the regiment may switch them by day (`mitronin_1976` p. 7); a
  switch is a notice like any other and arrives as late.

## 10. Design: the enemy's own aircraft going home

The enemy's raids end over their own ground air defence, and our fighters may follow them there.

- **NATO.** Its ground air defence holds a weapons control status per area, aircraft type and time: weapons free (fire at
  anything not positively friendly), tight (only at positive hostiles), hold (self-defence or a formal order); the region
  air defence commander sets it, army commanders may only tighten it (`fm44_1_2_1984` p. 9-1, from ATP 42;
  `fm44_3_1981` ch. 5, not re-read). Returning aircraft fly minimum risk routes at set times, above or below a
  coordinating altitude, across an IFF line with return procedures, Mode 4 set by code book (`fm_100_42_1976` ch. 4;
  `fm1_103_1981` app. E; `fm44_1_2_1984`). In SAM3D this is faction data (`faction_nato.json`, a new `"own_ad"` block):
  the egress leg of a NATO route ends on an MRR at its window; a flight off it, late, or with its Mode 4 failed (damaged)
  takes a fratricide chance from its own belt (a game value; no rate was found), written into the enemy's records as its
  pilots and gunners claim it. Our fighters pursuing past Turchin's 25-30 km line meet the belt under weapons free: the
  pursuit limit of section 3 keeps them out, and a flight that breaks it takes a loss chance (a game value).
- **Warsaw Pact.** The same Mitronin rules for its own aircraft; Soviet units with Parol and, where they have the Osa-AKM,
  the launch block, so its own fratricide is lower for Soviet units than for its allies or for us.
- **Neutral and rebels.** No SAM zones; the neutral's guns identify by sight and procedure; the rebels have nothing.
  Low confidence; no rule beyond "fire on what attacks".

## 11. The campaign: A0, the scalar, the ledgers

- `FrontDef::friendly_fighters` stops being authored truth. The resolver's fighter term reads our regiments' airborne and
  readiness-1 sorties from the duty table for the hour and the bases' function, and `07_AIR_TO_AIR.md` §5.6's A0 exchange replaces
  the flat `escort_fighter_kill`; our losses come back in `PackageOutcome::our_fighters_lost` and go to
  `FighterRegimentState`, the enemy's are written with `LossRecord::by` = the fighter regiment's id (`fighter_rgt_1`), as
  `07_AIR_TO_AIR.md` §5.6 gives it.
- The planner's escort sizing reads the enemy's **belief** of our fighter threat from its picture (interceptions its
  pilots reported), kept within the band of today's 0.35-0.4 on day 1 so `air_war_balance` still holds; a struck air
  base lowers the truth at once and the belief only when the enemy sees fewer of ours
  (`docs/campaign/02_WHAT_WE_DEFEND.md`: "Fewer of our fighters up: the enemy brings fewer escorts" is then true).
- The outcomes' ledgers (`campaign/outcomes.hpp`) count, for the debrief only, our aircraft lost to our own fire and the
  raids let through under a fire ban.

## 12. How this widens roadmap 17.2

| 17.2 as written | Widened to |
|---|---|
| "The friendly corridor in the order" | A cooperation plan: by zones or one zone shared by height, sector, line, time or target (`mitronin_1976`); several corridors and duty zones with times; the fighters' floor and its drop to 3,000 m over units the records say cannot fire; structured in the task file and drawn on the commander's working map |
| "IFF codes and their changes" | Kremniy-2M's 12 filters on a 1-6 h schedule, a document carried by a means; the crew's change as a job; the pilot's switch; reply only on a matching filter; no launch block; the enemy's transponders by system |
| "The fighter regiment's notices through the pipeline" | Franchuk's notice as a coded `Fact` with Druzhba-76's delay, into the radiotelephonist's journal and onto the board from the journal; fire bans and code 40/group 402 sent; violators and distress reported |
| "Friendly aircraft in the scenario" | Our fighter regiments as units with readiness, duty and scrambles by a rule table, each flight a live actor under our GCI on our picture, in session (A2/A1) and campaign (A0); fighting the enemy (`07_AIR_TO_AIR.md`) |
| "Friendly fire when a notice or the codes did not arrive" | The same, plus the double engagement and the enemy's false friend; never rolled; traced in the record, the regiment's check and the debrief; calibrated against the 25% exercise figure |
| (not in 17.2) | The enemy learning and using our corridors and our fire bans; the regiment's choice between holding fire and firing; NATO's weapons control status and minimum risk routes for its own returning aircraft; the neighbours erring as we do; `friendly_fighters` derived |

Still out: our air force as a planner (our strikes, our ATO), our fighter-bombers' missions beyond notified crossings in
the 50-100 m overflight zone (`mt_turchin_coop` p. 16), Parol and the launch block for our nation (after 1985), any data
link to the battery beyond the PU-12's own.

## 13. Data, code and tests

| Item | Change |
|---|---|
| `data/command/own_air.json` | New: the duty rule table, readiness states, airborne-alert zones, pursuit limit, coding work per hop, the cooperation defaults, and our side's `hot_phase` block (`10_FACTIONS.md` §7.5); each value with `source` and `confidence` or marked a game value |
| `data/command/iff_codes.json` | New: Kremniy-2M filters (1-8 peace, 9-12 war, 4 alignment), the change period 1-6 h, the document's means (`soviet_iff`) |
| `data/command/chain_pannonia.json` | `sector_gci`, `pnts_1`, and the air force's officer at the division AD CP; `fighter_rgt_duty` now sends and receives |
| `data/campaign/tasks/*.json` | An `"own_air"` block beside `coordination` |
| `data/crew/osa_ak_crew.json`, `pu12m_crew.json` | Lines and orders: own aircraft told, fire prohibited on a track, code changed, one of ours near the target, violator; each with a `"menu"` place (`test_crew_talk.cpp`) |
| `data/air_war/faction_*.json` | `"own_ad"` (weapons control, MRRs, IFF system and flags such as the Kremniy interrogator, off) |
| `data/gameplay/own_air.json` | Game values: duty pairs, scramble ratio, change job time, pilot miss chance, enemy break chance on talk tables, fratricide chance off an MRR, the low calibration band |
| `simcore/examples/own_air_balance.cpp` | New: a week of the bridges mission with our flights, notices and codes failing by settings; prints own aircraft among targets fired on |
| Tests | `test_own_air.cpp` (new), `test_command.cpp`, `test_command_post.cpp`, `test_engagement_model.cpp`, `test_em_link.cpp`, `test_packages_live.cpp`, `test_air_war.cpp`; `helper_coverage` after the crew changes |

## Done when

- A notice of a planned MiG-21 pair, sent from `fighter_rgt_duty`, reaches battery 21's PU-12M in 10-15 min from the
  division AD CP with the default chain, and sooner with the air force officer at the regiment (`test_own_air.cpp`).
- A MiG-21 in the notified corridor on today's filter answers ЗАПРОС on the player's scope; the same pair shows no reply
  at a battery that never got the day's code document and stayed on yesterday's filter (roadmap 17.2's done-when).
- A battery left without the notice, and with no reply, fires on the pair under the NPC commander; its journal shows no
  group 402 before the launch; the report and the regiment's check name the missing notice; the debrief shows the chain
  with each post's time.
- With the notice in the journal, the NPC commander holds fire on a non-replying track in the corridor at the notified
  height and time, and reports a replying track off its route as a violator.
- A readiness-1 pair is airborne 3-5 min after a scramble rule fires and a readiness-2 pair 15 min after; the same
  picture and seed give the same scrambles.
- With more flights than GCI channels, or the GCI net jammed, the extra flights search with their own radar's shorter
  reach or abort by the leader's skill.
- When the regiment's record has a battery on the march, the notices give our fighters a 3,000 m floor over its zone.
- `BoardContact::type`, the battery fire model and the neighbours' fire read no `t.friendly` (a count test, as
  `test_helper.cpp` counts `NpcAt(`).
- In a war with the Pact, an allied MiG on the battery's filter answers friend, and the debrief says it was the enemy.
- After days of corridor use heard by its COMINT, the enemy's picture holds the corridor and a package routes along it at
  a believed return window (seeded test).
- A NATO flight returning off its MRR with Mode 4 failed can be lost to its own belt and is recorded so in the enemy's
  records.
- `own_air_balance` gives up to about 25% of our own aircraft among the targets fired on under 1960s settings, and the
  low band with codes and notices working; `air_war_balance` and `air_war_bench` stay in their bands; a campaign with our
  flights saves and replays exactly.
- No Parol, no launch block and no data link to the battery appear for our side in the war of 1985.

## As built (B10, 18 Nov 2026, branch `tb-b10`)

A first form of sections 3-9: the codes, the notices, the crews' rule, the duty and its pairs, the trace and the enemy's
learning are in simcore and tested; the crew's lines, the task file's block, the report and the campaign are not (the
**Left** list below).

**Code and data.**

- `data/command/iff_codes.json`: the Kremniy-2M's 12 filters, 1-8 in peace and 9-12 in war, changed every 1-6 h
  (`soviet_iff`); the day's table a paper issued the evening before (a game value).
- `data/command/own_air.json`: the readiness states (`fm_100_2_1_1984`), the duty's rule table (game values), the
  airborne-alert zones and the pursuit limit (`mt_turchin_coop`), the GCI (3 channels; 10 s automated, 3-6 min manual;
  1.5 km error; 220 km reach), the notice (half a minute's coding per hop, priority precedence, a 5 km corridor), the
  pilot's missed change (3 %), the cooperation defaults (`mitronin_1976`) and the enemy's learning. Every value carries
  its source or is marked a game value.
- `simcore/command/own_air.hpp`: the loaders; `IffSchedule` (a day's changes, keyed), `IffHolding` (a holder keeps the
  code it had until the day's table reaches it), `PilotFilterAt` (a pilot who misses a change stays on the old code),
  `IffAnswers`; `OwnAirZone`, `CooperationPlan` (by zones or one zone by height, the 3,000 m floor over a unit unable to
  fire, `Words()` for the order's coordination line) and its loader; `OwnFlightNotice`, `NoticeWords` (talk-table group
  402 with the notice's number, the route as the PVO grid's small squares, heights, crossing hours, return, amendment,
  planned or actual) and `ReadNotice` (the same read back from what was written); `JudgeTrack`, the crews' rule (a
  reply on a notified route is a friend, a reply off every notice a violator, silence inside a notified corridor at its
  heights and hours held, above the split height in one zone held, the rest free; with no notice and no plan a reply is
  a friend, as before); `TraceFriendlyFire`; `FighterRegimentDef`, `Scramble` and `FighterDuty` (pairs at readiness 1
  and 2 by day and night, scaled by the base's function; more on a raid warning; a pair per group on the picture coming
  at a regiment's area while the GCI has a channel free; take-off by the readiness state's times, later from a struck
  base); `EnemyCorridorIntel`.
- `simcore/command/own_flights.hpp`: `OwnGciPicture` (our GCI's radar on its horizon, each track with its error and its
  reply on the regiment's code, read as old as the sector passes it), `PlanPair` and `NoticeOf` (a scrambled pair as two
  session aircraft and its actual notice), `RunNotice` (a notice down the chain by `InfoNet`, post by post with each
  hop's coding work), `OwnAirScenarioDef`.
- `command/info_net.hpp`: `FactKind::OwnAirNotice` and `Fact::hop_work_s`; `scenes.hpp` and `player_inputs.hpp` know the
  kind.
- `em/model.hpp`, `em/radar_system.hpp`, `em_link.hpp`: a transponder's `iff_code` and the interrogator's; a reply needs
  both the same (`SetInterrogatorCode`, `SyncTarget(..., iff_code)`). Both default to 0, so nothing changes without
  `"own_air"`.
- `engagement.hpp`: `EngagementTarget::iff_code`, `SetIffCode`; the gameplay IFF and the EM one reply only on the code.
- `air_war/air_fight_session.hpp`: `AirFightWorld::tracks` (an "ours" controller vectors on our picture's hostile
  tracks, not the truth, when the runner gives it), `AddFighter` (a pair joins the running fight), a fighter thinks
  only once airborne. `air_war/air_radio.hpp`, `air_radio_def.hpp`: our side's set (`our_radio`, `our_freq_mhz`) and
  `"our_gci"` posts, so our vectors can go out as transmissions; our calls are not logged by our own intercept.
- `scenario.hpp`: a scenario's `"own_air"` block (`data/scenarios/README.md`), a target's `"iff_code"` (-1: its
  transponder off) and `"iff_table"`; an `"air_fight"` may have no fighters when our GCI is there for `"own_air"`.
- `scenario_runner.hpp`: our pilots' codes each step; the Osa's interrogator on the battery's holding, the PU-12M's on
  the post's, the regiment's picture and the other batteries on the day's table; the board's type and the transponder,
  the battery fire model's and the testbed's neighbours' `friendly` by `RegimentHoldsFire` (a reply on their code, or a
  notice the regiment's post has, or the plan), never by the target's truth; each notice down the chain (`RunNotice`),
  then on the regiment's R-111 into the post's journal or, without a post, as the battery commander's line (a lone
  vehicle's commander told in person); the crew keeps the notice it read from the words (`CrewNotices`); the NPC
  commander holds fire on a track his crew's notices and plan hold and does not pick it, through his existing orders
  (`hold_fire`, `order_drop_target`); the duty scrambles pairs on `OwnGciPicture`, each pair entering by `AddTargets`
  and `AirFightSession::AddFighter` with its actual notice sent down the chain; a launch at one of ours writes
  `TraceFriendlyFire`'s finding to the log (`FriendlyFire`).
- `air_war/raid_scenario.hpp`: `ScenarioFormat::our_fighters` (off by default) writes `"own_air"` with the theatre's
  working airfields as our regiments, our GCI 40 km behind the battery from the raid, and the raid warning.
- `light.hpp` now includes `crew_data.hpp` for `detail::ReadTextFile` (an include-order fault the new headers exposed).

**Random draws.** No new stream. All on `Stream::AirCombat` with keys from `kOwnAirKey` (0xB10A0000): +0x100 the day's
schedule, +0x200 a pilot's missed change, +0x300 a scramble's take-off, +0x401-0x403 the GCI picture's age and error,
+0x500-0x501 the enemy's hearing and placing of a corridor. None meets B8's keys. `prototype/` has no IFF reply model,
so nothing is mirrored.

**Numbers.** The notice's way on `chain_pannonia.json`: the fighter regiment's duty officer to the sector's CP about
14.5 min, the sector to the division's AD CP about 14 min, the division to the regiment's CP about 14.5 min; with the
regiment's half minute on its net, division to battery about 15 min, the top of `mitronin_1976`'s 10-15 min (the chain's
own relay, a field telephone's 10-min connection off 'Vozdukh' precedence, is most of it). A readiness-1 pair was
airborne 247-282 s after the order (3 seeds) in the session test. Pilots miss about 3 % of changes. The enemy, hearing a quarter
of the uses and reading the talk table in 15 % more, learns a corridor used 40 times in every seed, placed about 8.5 km
off.

**Tests** (`tests/test_own_air.cpp`, 8 cases; one in `test_em_link.cpp`): the codes' schedule and holdings over 20 seeds;
the notice's words read back (40 notices) and the crews' rule case by case; the notice's way down the chain and its
timing; in a session, the Osa asking on the crew's code (a pilot on another code is not answered and is fired on, the
trace naming the code; on the same code never); a notice that reached the crew holding fire on a silent one of ours in
the corridor while the hostile outside it is engaged, and a lost notice letting the launch happen, traced as no notice
and the transponder off; the duty scrambling a readiness-1 pair, vectored on the picture, on the table's code, its
notice traced; the enemy's learning; and a count test that the board, the post's interrogator and the battery fire
model read no `t.friendly`.

**Left:**

- The crew's own lines (own aircraft told, fire prohibited on a track, code changed, one of ours near the target,
  violator reported) and their `"menu"` places: the NPC commander uses the existing `hold_fire` and
  `order_drop_target`, and the code change is an event with no seat's act; no cabin control sets the Kremniy's code, so a
  player's seat gets it changed for him.
- The task file's `"own_air"` block, the battery commander's plan and working map with the corridors, the report's and
  the regiment's check against the notices (`campaign/report.hpp`), and the 3,000 m floor from the regiment's record of
  a battery on the march.
- The raid builder writes no enemy `"air_fight"` fighters with our pairs (our pairs fight only the escorts a scenario
  gives), and `ScenarioFormat::our_fighters` is off in the campaign; the testbed's friendly flights stay scripted (they
  answer on code 0).
- Our GCI's vectors on `our_radio` need an `"air_radio"` block with `our_radio` set; the raid builder does not set it,
  so our vectors are heard at once as in B8. Flights beyond the channels search on their own only as B8's lost control.
- The fighter regiment's loss report down the chain, our fighters' claims through the pipeline, and the regiment's
  word to the battery after a friendly-fire launch.
- `EnemyCorridorIntel` is not fed by the air war's intercept nor read by its planner; no save mode is needed until it
  is.
- Airborne alert zones, the pursuit limit and recall rules are in the data and not yet flown; the allied false friend
  (a Pact enemy on our filters) and NATO's own belt are not built.
- `own_air_balance` is not written.

## Left open

- **Corridor widths and heights, and the notice lead time** the regulations required: not found (look in the НПП of the
  period, the air army's combat regulations, NVA or ČSLA instructions on «заявки на пролёт»).
- **IFF reply and failure rates** (transponder on, right code, garbling): none found; the 0.9 per look and the pilots'
  miss chance stay game values.
- **The Osa-AK's own interrogator**, its modes and how its code is set; whether the regiment's ZSU-23-4 and Strela had
  interrogators in 1985. Check `mon_2112_80`.
- **Parol's 1977 date** rests on a dead page's excerpt; the NVA's change-over date on Wikipedia.
- **Whether the SRO-2M answers Parol**, and whether a Pact enemy's allies share our Kremniy filters (section 7's false
  friend is an inference).
- **Combat Tree and Kremniy spoofing in Europe in 1985**: `soviet_iff` (tier 4) against `air_combat.md`'s unsupported
  verdict; kept off.
- **How automated our sector's control was** (a Vozdukh-type system or manual boards): set by data until a Hungarian,
  Polish or Czechoslovak source is read.
- **Our regiment's rule for holding fire** against firing on non-replying targets, by adversary: no period rule found.
- **The visual recognition signals** of art. 41 and the fire-prohibition signals of art. 34: their content not found.
- **NATO Central Region detail**: the Hawk and Nike belts' relation to fighter areas, 2nd and 4th ATAF return procedures,
  any fratricide rate; FM 44-3 and FM 100-42 not re-read.
- **Whether a Teaball-type warning service** turned our GCI voice into warnings for NATO's crews in Europe by 1985: not
  found (`comms_emcon.md` comms-06); off by default.
- **The readiness chart** is for fighter-bombers in NATO's view; a Soviet source for fighter readiness is wanted.
- **Cases to verify**: Egypt and Syria 1973, the Falklands (an Argentine Mirage, a British Gazelle), Iran-Iraq.
- **Our pilots' annual hours and skill**: no source for our nation; game values as for the enemy's.
