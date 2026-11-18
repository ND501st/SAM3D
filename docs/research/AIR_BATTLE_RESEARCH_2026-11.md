# The air battle: research (November 2026)

Why: track B, the air battle. The project owner asked on 15 November 2026 for the enemy's air war to be fought in its
hot phase, while a raid is flying: routes flown as the period flew them, reactions to missiles, flak and fighters, flights
retasked by voice through ground nodes, targets of opportunity, pilots' eyes and sensors, live radio that can be heard,
and air-to-air fighting between our aircraft and the enemy's. He said twice that it must be consistent with the Cold War
and not with modern tactics. **The war's year is 1985.** Everything here was in service or in doctrine by 1985. Later
things are listed in section 4 as a later war's options and are not the model. The Gulf War of 1991 and after is hindsight
and never data.

How it was done. Ten topic reports were written on 14-15 November 2026: routing, reactions, c2_retasking, opportunity,
aam_weapons, air_combat, our_side_air, perception, comms_emcon and factions_cases. Each was then checked adversarially
against the texts it cites, and each ends with a "Verification (15 Nov 2026)" section. Where the check corrected the
body, this note uses the correction. The topic files are kept in `docs/research/air_battle_2026-11/`, with six code maps
of the state of the simulation on 15 November 2026 (commit `1d09489`) in its `map/` folder: `routes`, `flight`, `sensing`,
`c2_people`, `air_to_air` and `scale`. Source ids are the topic files' own, and they are registered in `docs/SOURCES.md`
only when the data is written. Tiers follow `docs/SOURCES.md`. A US document about the Soviet side, or a Soviet one about
NATO, is the adversary's view and is marked so.

This note follows `docs/research/AAA_DAMAGE_2026-11.md` (R0, the guns, damage and first reactions) and extends it. Nothing
in it is repeated as new.

## 1. Summary: what the research changed in the design

1. **A route is a line of turn points on map features, planned to minimise time in view of the defences the planners
   believed in, not to avoid them.** A Soviet analyst said a clean bypass works only on a staff map and that every route
   ends inside the target's own firing zone (`mt_1984_babich`). NATO crossed the front through a gate on a minimum risk
   route in a time window, with a suppression window on that gate (`fm_100_42_1976`, `jsead_1982`). No public source gives
   leg lengths, turn-point counts or a stand-off from a SAM ring: those stay game values.
2. **The eye comes first, and the receiver tells less than a modern one.** In 13 of 16 MiG attacks on F-105 flights the
   first sighting was visual (`red_baron_vol2`). A 1985 receiver gives type, rough bearing and a launch light for guidance
   it knows, with false alarms, and nothing for an infrared shot. No missile approach warner on fast jets by 1985 is
   shown in any source. Today's truth cues (a cue to a pilot from our launch, the 15 km "launch seen") are therefore
   wrong and go.
3. **Timing of the reaction decides survival, more than the manoeuvre's shape.** Hits fell on aircraft that had taken no
   evasive action or had only begun it (`checo_1967_tactics`). Reaction delay and its spread by skill are the main lever.
   About half of the attacked flights jettisoned something, a third pressed on, and a fifth to a third aborted
   (`red_baron_vol2`). Jettison and abort count as a win for the defence, which fits the ledger of the enemy's freedom of
   action denied.
4. **Nobody retasked a strike aircraft by data link in 1985.** Retasking was by voice through ground nodes: for NATO the
   FAC, the ASOC at corps and the ATOC; for the Pact the control and target-identification posts, the division's aviation
   control element and the regiment's command post. Interdiction and deep strikes were hardly retasked in the air on
   either side. The live headquarters loop belongs on those nodes, with a delay for each hop, and the message can be late,
   lost, jammed or heard.
5. **A battery that reveals itself is attacked on three clocks:** aircraft already in the area and allowed to attack
   threats (minutes), an immediate request from a ground unit (15-50 minutes), or nomination into the next tasking cycle
   (24-48 hours). A position found by photograph was 6-8 hours old when used (`checo_1967_tactics`).
6. **Most flights never see a target they were not sent for.** Authority to attack is set before take-off by the mission
   and the area, not by the pilot. The Pact's default is to report and call a strike from readiness, not to hunt; it hunts
   in the zones it is given and once it holds the air. Night hunting worked only where a ground post with the movement
   plan cleared each strike. A re-attack is allowed but costs more. Pilots' claims run far above the truth (7 of 103
   reported truck kills confirmed by photograph), so the enemy's journal keeps the claim and the debrief keeps the truth.
7. **The air-to-air shot is a chain of four chances,** not a physics model: launched inside the envelope, the round works,
   it guides, it kills. The period gives each: 35% of AIM-7 and 26% of AIM-9 shots in 1967 were outside parameters
   (`checo_1967_a2a`); 25-30% of the 1972 rounds showed guidance failure (`checo_1978_linebacker`). Soviet design figures
   are the ceiling, not the combat rate.
8. **Infrared missiles did most of the killing, and BVR was mostly switched off by NATO's own rules.** NATO's mixed IFF
   forced visual identification before a radar shot in central Europe (`afit_1982_vid`, `gao_1985_id`). The default rule of
   engagement for 1985 is VID, with BVR only where the identification chain allows it. No fighter on either side had a
   launch-and-leave radar missile.
9. **The two blocs fought differently, and that difference is the heart of the model.** NATO flew pairs, line abreast,
   with their own radars, helped by AWACS or ground radar, deciding for themselves when to commit. The Pact's fighters
   were brought to the target by ground control, and its flight leader's initiative began at contact. Cut the ground
   control (Bekaa, 1982) and the Pact pilot fights badly. The reform of 1983-84 was patchy.
10. **Our fighters are live actors under our ground control, and our own SAMs are their worst problem.** Fighters and SAMs
    share the sky by zones or in one zone by height (`mitronin_1976`). The battery hears of our flights by notice and code
    table, 10-15 minutes in Druzhba-76. In a Baltic exercise up to 25% of the targets fired upon were our own aircraft
    (`mt_turchin_coop`). Kremniy-2M IFF in 1985, not Parol; the Osa-AK has no launch block (an inference). Friendly fire
    follows from a late or missing notice or code change, and the radio journal shows why.
11. **Perception is by eye, set by target, background, visibility, masking and the cue.** Speed matters little to the
    chance, only to the time. A deployed Osa battery is hard to find until it fires: its launch, dust and smoke trail
    are how a strike pilot finds it. Skill rests on annual hours, a combat-experience term, workload and, in two-seaters,
    the crew.
12. **Radio is a source for the other side, and it is slow and fast.** Traffic analysis and flight plans gave hours of
    warning in Vietnam; calls in the clear gave minutes (`nsa_purple_dragon_1993`). Radar usually warned of a raid in the
    air before COMINT did, so intercept adds who, how many, where to and when. Deep NATO strikes flew silent; packages
    near the front talked. A Pact strike was nearly silent; its ground control and warning nets kept talking. Linguists,
    call-sign rotation and radio discipline are people's traits.
13. **Faction profiles are data, and the people move them a little.** The faction value is the doctrine; an officer's
    traits move it by up to about 0.2 and a pilot's experience, currency and nerve by up to about 0.15. These are SAM3D
    estimates ordered and bounded by the sources, not sourced numbers.
14. **Two current data choices do not fit 1985.** The NATO commander's medium-altitude quirk is not Central Region
    practice (NATO planned and trained low). The rollback-then-interdiction phase with percentage gates is closer to 1991
    than 1985. Medium altitude is a choice once the believed defence is broken (Vietnam 1972, Bekaa 1982).

## 2. Findings by topic

Each paragraph gives the findings by faction with their source ids. The full text, the numbers and the checks are in the
topic file named at its end.

### 2.1 Routing and penetration

NATO. US navigation doctrine put turn points on identifiable features and began the attack at an IP (`afm_51_40_1973`
ch. 26). Minimum risk routes were built through the friendly area from the known air defence, and the corps was asked to
suppress the enemy's air defence along a route and time at the front (`fm_100_42_1976` §4-6d, `jsead_1982` ch. 4). At Red
Flag (Nellis, from 1977) 60-70 aircraft entered through corridors in 20-30 minutes with no radio calls (`rafhs_j20_1999`).
Heights were 200-500 ft in US doctrine, 500 ft and 420 kt for the CF-104 in tactical air support, under 100 ft for the
A-10, and 100 ft for RAF four-ships; war heights were expected below training heights (`agard_cp267_1979`). The F-111F used
terrain-following radar to 200 ft (`crandall_twz`). RAND found that choosing the path could cut intercepts by up to a
factor of three and that exposure kept to 20 s or less degrades mobile defences (`crawford_1977_rand`). Peacetime floors
(UK 250 ft, 100 ft in three areas; West Germany 150-450 m, 75 m in seven areas, nothing lower over the FRG) are law, not
the war. The Tornado GR1 account is undated and may be after 1985.

Warsaw Pact. In NATO's view a strike was planned on terrain models, went to a checkpoint behind its own lines, confirmed
the target with ground control, crossed its own air defence by strict time and path, then approached at 50-100 m with
radio silence, 3-6 s to find the target and 10 s to clear it (`fm_100_2_1_1984` pp. 11-11 to 12-8). The safe corridors are
the US authors' inference. It held to the plan unless ground control changed it. The Soviet rules of 1985 class 0-200 m as
extremely low (`opp_85` §45(7)). Helicopters stayed under their own air defence in Europe; the Afghan heights of 1500-1700 m
above the guns belong to a rebel war (`airwar_mi24_markovsky`). Iraq, Soviet-trained, kept rigid profiles under tight
control (`cia_1987_iraq_af`).

Neutral and rebels: no period routing source. The neutral flies short, simple low routes in its own airspace; the rebels
fly at night, low, by roads and rivers (design inferences). File: `air_battle_2026-11/routing.md`.

### 2.2 In-flight reactions

NATO. Eye first: launch dust and the climbing missile were seen; USAF crews in 1972 judged the eye more reliable than
swamped receivers (`nalty_1977`). The SA-2's guidance signal came no later than 4 s after launch, and the pulse repetition
rate doubled 30-40 s before launch (`nalty_1977`). The doctrine was to wait until the missile could no longer follow, then
pull hard (`werrell_2005` p. 120). Chaff came by hand or from airbrakes, and Linebacker II set a rule for turns over 45
degrees (`nalty_1977`). Against low-level SAMs of the Osa's class, the 1973, 1982 and Falklands evidence shows low and
fast run-ins, chaff, a break on the tracking call, and leaving the zone low behind terrain (`werrell_2005`, `pook_1985`).
Jettison under MiG attack: 8 of 16 events in 1965-67 and 21 aircraft on one day in January 1967 (`red_baron_vol2`,
`checo_1969_tactics`).

Warsaw Pact. The Soviet anti-missile manoeuvre was a preset programme planned from the known sites and flown at once, to
the zone's nearest boundary (`sve_prm`, `mt_1984_babich`). The SPO-10 receiver gave a tone and a direction. Flare
dispensers on the Su-25 were rarely used before about 1985 (`markovsky_afghan_sky`).

Neutral and rebels: no receiver, so react to a sighting or a call; Argentine pilots in 1982 wasted improvised chaff by
opening the airbrakes to keep formation (`twz_2025_argentine_chaff`). Our side's pilots under jamming flew worse (Syria
1982, `nwc_83_027`). The design rules by faction and skill in the topic file are SAM3D estimates anchored on these counts.
File: `air_battle_2026-11/reactions.md`.

### 2.3 Command, control and retasking in the air

NATO. The flight leader changes only within his fragged mission. The FAC (on the ground or in an Army helicopter; no
airborne FAC squadrons left in Europe) briefs and clears flights handed to him. The ASOC fills immediate requests from its
allocation and from authorised diversion of preplanned sorties. The ATOC has tactical control and may divert BAI. The
CRC and the E-3A warn and control fighters; no source gives them authority to retask strike flights. Requests go by voice;
there is no data link to a strike aircraft (`rippe_1985_airland`, `hoak_1987_retasking`, `schoeck_1987_ai_retasking`,
`smart_1985_tacs`). A BAI nomination took 48 hours (`gallant_knight_1983`). An immediate CAS request took 40-50 minutes
in Korea and over 90 minutes in Vietnam before 1965; goals were 15-20 minutes (`luke_2002_cas`, hindsight, third-hand).

Warsaw Pact. The pilot flies the plan. Ground control (control and identification posts, the regiment's FAC) may change
the route through friendly air defence and confirms the target at the checkpoint. Takeoff is 3-5 minutes from readiness 1
and 15 minutes from readiness 2 (`fm_100_2_1_1984` p. 12-7). Doctrine gave 20-30 minutes from request to strike
(`mt_1962_kontsevoy`). In Afghanistan, FACs went with every battalion, and the pilot's report came 5-10 minutes after
landing (`markovsky_afghan_sky`). Neutral and rebels: no source; the neutral is assumed centralised with no in-air
retasking, and the rebels have none. File: `air_battle_2026-11/c2_retasking.md`.

### 2.4 Targets of opportunity

NATO. In the early 1980s F-4 crews on close air support usually never saw a target (an A-10 pilot's interview,
`bush_a10_simhq`, unverified: it could not be re-read). Misty FACs judged that strike pilots on armed reconnaissance could
not see or identify targets (`checo_rp1_1969`). Authority is set by area: armed reconnaissance beyond the FSCL or in a free
fire area (`fm_101_5_1_1980`, US Army terms), a 200 yd (later 200 m) corridor either side of roads in Vietnam, and a 500 m
rule for villages (`checo_interdiction_1969`, `checo_roe_1973`). Fast jets hit back at guns that fired on them. The A-10's
gun immobilised 8 of 10 stationary undefended tanks in ten passes in a 1979 test (`cdat_array20_1980`). Two of the three
GR3s lost in the Falklands were re-attacking (`rafm_falklands`).

Warsaw Pact. Report at once by radio; on-call strikes from readiness 1 or 2; an armed reconnaissance group of one
reconnaissance aircraft and 2-4 attack aircraft; NATO noted "some reservations" about free hunting until air superiority
(`fm_100_2_1_1984` ch. 12). The Soviet text of 1970 names strikes on request as the principal method, but for the maritime
battle only (`mt_1970_bukhantsev`). Helicopter night hunts in Afghanistan were cleared by an outpost with the movement plan
(`rgs_afghan_2002`). Neutral: planned low runs approached as unseen as possible (`ericsson_f17_recce`). Rebels: single
low passes on infrastructure, impulsive shoot-ups that cost aircraft (`garrison_biafra`, low confidence). File:
`air_battle_2026-11/opportunity.md`.

### 2.5 Air-to-air weapons and sensors

NATO. AIM-7 and AIM-9 hit rates in 1967, the 1972 reliability problem, and the 137 USAF MiG kills by weapon give the
calibration (`checo_1967_a2a`, `checo_1978_linebacker`, `aces_1976`). All-aspect AIM-9L from 1978 production and AIM-9M
from 1983; the F-16 could not carry Sparrow (`cbo_1986_amraam`, `navy_factfiles`). Infrared missiles killed about 200 Arab
aircraft against about 7 for Sparrow in 1973 and about 50 against 2 over Lebanon in 1982 (`cbo_1986_amraam`). Skill
decided more than the missile: the exchange ratio went from 1:0.47 to 3.8:1 in 1972 once Teaball and Combat Tree gave
warning and identification (`checo_1978_linebacker`).

Warsaw Pact. R-3S, R-13M and R-60 are rear-aspect; R-24 (adopted 6 April 1981) and R-23; the R-73 (adopted 22 June 1984)
only where MiG-29s were already in Soviet units (not established). The MiG-23ML's radar found a fighter at 50-55 km by
hand and 80-85 km under ground guidance (`airwar_ru`). The R-27 (adopted 1987-90) is post-period. The Syrian R-24 claims of
1982 stay claims. Neutral: no missiles by treaty (unverified, Wikipedia only). Rebels: none. Our side: a Pact-like
small nation with MiG-21 class aircraft. The topic file ends with a proposed gameplay air-to-air model fitted to these
numbers. File: `air_battle_2026-11/aam_weapons.md`.

### 2.6 Air combat tactics

NATO. The pair, line abreast about a mile apart with an altitude stack, one fighter "engaged" and one "free", sorted "left,
low, lead" (`anderegg_2001` pp. 59, 83-84, 160). VID bound the BVR shot (`lambeth_1985`, `gao_1985_id`). In the Bekaa
(June 1982) Syria lost 23 aircraft on the first day by Lambeth's count, about 85 by the end of July, and Israel none in air
combat (`lambeth_1984`, `cordesman_1982`). Exchange ratios fall as numbers rise (ACEVAL). Argentine attackers at the end
of their range could not afford to manoeuvre (`usn_falklands_1983`).

Warsaw Pact. Fighters were brought to the target by ground control, often by data link to the autopilot; initiative began
at contact; the reform of 1983-84 widened it patchily (`lambeth_1984`, `pennington_1985`, `johnson_1986`). The pair was
inside a group of four to eight; the rear-hemisphere attack started no closer than 6-8 km and launched at 4-6 km
(`mt_turchin_coop`, for targets above 1,300 km/h). Neutral: only an unsupported Wikipedia analogue. Rebels: nothing. Our side:
the Soviet model; air combat skill rests on ground control. File: `air_battle_2026-11/air_combat.md`.

### 2.7 Our fighters and our ground air defence

The Pact's own staff journal gives two methods: by zones, and in one zone shared by height, axes, lines, time and targets
(`mitronin_1976` pp. 5-6). Fighters should not work in the SAMs' fire zones; over dense troop air defence they get a band
above the SAMs' upper limit or a corridor (p. 7). Control posts sit together, the SAM commander and the fighter guidance
officer at one plotting board (pp. 7-11). The battery commander tells every man our flights, duty zones and the signals,
and excludes targets our fighters attack (`pvo_bu_ix_1984`, the artillery battery's regulation, used by analogy). Flight
applications are compared with what radars see and violators reported (`pvo_bu_x_1984` art. 56). Holding fire for our
fighters is a weakness: two MiG-21s were lost on 14 July 1966 partly for that (`mt_1973_nvn`). IFF is Kremniy-2: 12 code
filters changed every 1-6 hours, unencrypted (`soviet_iff`). NATO had weapons control status (free, tight, hold),
minimum risk routes and high-density airspace zones (`fm44_3_1981`, `fm_100_42_1976`, `fm1_103_1981`). The neutral and the
rebels: no source. File: `air_battle_2026-11/our_side_air.md`.

### 2.8 Pilots' perception and human factors

By 1985 every faction's attack pilots found ground targets by eye on a short, busy run-in. The chance was set mostly by the
target and its background (62% of the variance in JTF-2), the visibility, terrain masking and the cue (`jones_atg_1974`).
The China Lake models give the physics: a foveal detection formula and a 1.65 s glimpse (`nwc_tp5709_1974`) and a launch
opportunity model for ground targets (`nwc_tp6005_1978`), both on US crews; for other air forces this is a SAM3D
assumption. AA sites were seldom seen unless firing (AGARD 1972, CHECO 1967). Ground observers saw jets at about 6,200 m
(`humrro_1968_ground_observer`). USAF tactical pilots flew 16-18 hours a month in 1980-85 (`gao_1986_flying_hours`); Soviet
pilots 125-160 hours a year in 1969, in the US view (`cia_1969_soviet_tac_training`). Afghan Su-17 pathfinders searched at
800-1,000 m (`markovsky_afghan_sky`). Sensor in-service dates (Pave Spike, Pave Tack, Kaira-23, Klen-PS) are not sourced.
File: `air_battle_2026-11/perception.md`.

### 2.9 Communications, emission control and intercept

NATO. Deep strikes flew silent; Red Flag packages used no calls; a 1982 Bekaa package was run by voice from an airborne and
a ground command post (`grant_2002`). HAVE QUICK, UHF frequency hopping, was introduced in 1980; when it reached USAFE is
not found (`fh_radios`). Four E-3As at Geilenkirchen in April 1983, 18 planned (`afmag_1983_usafe`). The dedicated jammer
of fighter control, Compass Call, was based in Europe from 1986-87 only.

Warsaw Pact. Silent on the move (`mt_1964_air_army`); fighters dependent on ground control with a channel limit of 30-40
groups per air army in 1962 doctrine (`mt_1962_front_aviation`). NATO expected the Pact to jam its tactical air control
nets and intercept at about 25-80 km from the line, locating a ground radio in 20-25 s (`fm_100_2_1_1984` p. 15-2). The
East German intercept arrangements are the state of 1989-90, a pattern for game values only. Warning from administrative
traffic took hours; tactical warning from calls in the clear took 10-30 minutes (`nsa_purple_dragon_1993`). Neutral:
nothing. Rebels: a guerrilla intercept company in Vietnam read 7,745 messages a month (analogue). File:
`air_battle_2026-11/comms_emcon.md`.

### 2.10 Faction profiles and case studies

Pact-style air forces plan everything before take-off and change little in the air, but are fast at launching a new task
from alert. In Afghanistan the Soviets changed the sequence on the ground, not in the air. Soviet-trained clients show the
lower bound: Syria 1982, cut off from ground control, broke into pairs and singles (`lambeth_1984`); Iraq kept rigid
profiles and piled onto easy targets (`cia_1987_iraq_af`). NATO trained for decentralised, low-level execution: Red Flag
from November 1975, the Tactical Leadership Programme with 8 courses a year (`afmag_1983_usafe`). Small and improvised air
arms show the most individual initiative and the least coordination (Biafra, Katanga, the Argentine MB-339). The
topic's parameter table (initiative, retasking latency, plan adherence, press-on, radio and emission discipline, skill) is
a set of SAM3D estimates. File: `air_battle_2026-11/factions_cases.md`.

## 3. Numbers usable as data

Only values that the verification confirmed or corrected. "Conf." is the topic file's confidence. "Verified" says what the
check found: confirmed, or corrected (the value is as corrected). Values marked as a design estimate or a game value are not
here: they are in the topic files, labelled. Source ids are the topic files'. Heights and speeds are as printed.

| Quantity | Value | Faction | Date | Source | Conf. | Verified |
|---|---|---|---|---|---|---|
| Normal low-level combat height | 200-500 ft AGL (61-152 m) | NATO (US) | 1973 | `afm_51_40_1973` ch. 26 | high | confirmed |
| CF-104 tactical air support | 500 ft, 420 kt; with FAC 500-1,000 ft at 400+ kt | NATO (Canada) | c. 1973-79 | `agard_cp267_1979` p. 23-1 | high | corrected (the strike and reconnaissance profiles are 1963-72) |
| B-52 / FB-111 / A-10 low level | 500 ft day, 800 ft night, 325-350 kt / 400 ft, 420-480 kt / often under 100 ft, 200-300 kt | NATO (US) | 1979 | `agard_cp267_1979` pp. 4-2, 4-3 | high | confirmed |
| Exposure that degrades mobile SAMs | 20 s or less (rests on accurate site intelligence) | NATO (US view) | 1977 | `crawford_1977_rand` | medium | confirmed |
| Intercepts cut by path choice | up to x3 (one example site) | NATO (US view) | 1977 | `crawford_1977_rand` | medium | confirmed |
| Peacetime floors | UK 250 ft, 100 ft in three areas; FRG normal band 150-450 m, 75 m in seven areas, below 75 m abroad only | NATO | 1979; 1984-87 | `hansard_lowflying`, `btd_10_2980_1985`, `btd_11_558_1987` | high | corrected (FRG terms) |
| Red Flag package flow | 60-70 aircraft through corridors in 20-30 min, no radio calls | NATO (USAF, RAF) | from 1977 | `rafhs_j20_1999` | medium | corrected (Red Flag, not RAF Germany) |
| Soviet height classes | 0-200 m extremely low; 200-1,000 m low | Pact | 1985 | `opp_85` §45(7) | high | confirmed |
| Soviet approach over enemy territory | 50-100 m; 3-6 s to identify; clear within 10 s; radio minimal | Pact (NATO's view) | 1984 | `fm_100_2_1_1984` p. 12-8 | medium | confirmed |
| Survival by height (Vietnam, Soviet reading) | 60-90 m over 75%; 30-60 and 90-200 m 50-75%; under 30 m or over 200 m under 50% | general | 1984 | `mt_1984_babich` | medium | confirmed |
| Mi-24 armed reconnaissance (Afghanistan) | 1,500-1,700 m; 600-800 m apart | Pact | 1980-85 | `airwar_mi24_markovsky` | medium | corrected (cover pairs at 1,200-1,800 m are over the landing zone) |
| First detection of a fighter attack | visual, no warning, 13 of 16 events | NATO (US) | 1965-67 | `red_baron_vol2` II-30 to II-47 | high | confirmed |
| First-sighting range of MiGs | 600 yd to 4-5 mi typical; about 10 mi once; a bogey at about 15 nm | NATO (US) | 1965-66 | `red_baron_vol2` | high | corrected |
| Jettison, press on, abort per attacked flight (16 events) | 8 / 6 / 3 | NATO (US) | 1965-67 | `red_baron_vol2` | high | confirmed |
| Aircraft jettisoning after one MiG attempt | 21 (21 Jan 1967); 47 (Sept 1967) | NATO (US) | 1967 | `checo_1969_tactics` | high | confirmed |
| SA-2 radar cue | PRF change 30-40 s before launch; guidance signal within 4 s of launch | our side (SA-2) | 1965-67 | `nalty_1977` | high | confirmed |
| SA-2 warhead arming; self-destruct | about 1,000 ft from intercept, about 23 s; 57-63 s | our side (SA-2) | 1965-67 | `nalty_1977` | high | confirmed |
| Height above cloud to see SAMs | 8,000 ft | NATO | 1972 | `nalty_1977` | high | corrected (a 1972 Linebacker figure) |
| Loss rate before and after jamming pods | 0.0037 to 0.0021 | NATO | 1966-68 | `checo_1969_tactics` p. 20 | high | confirmed |
| Linebacker II | 882 SAMs; 15 B-52s destroyed, 9 damaged; chaff in turns over 45 degrees at 30 degrees bank or more | NATO | Dec 1972 | `nalty_1977` | high | confirmed |
| Chaff corridor | 2 nm x 4,000 ft x 34 nm; misses within 200 ft 22.5% outside, 6.5% inside | NATO | May 1972 | `nalty_1977` | high | corrected (formation time not found) |
| SA-7 1972 | 351 fired, 221 incidents, 26 down; missiles per kill 1.8 helicopter, 10 slow fixed-wing, 135 F-4 | NATO losses | 1972 | `werrell_2005` p. 128 | high | confirmed |
| Wild Weasel ARM kill probability | about 5% | NATO | 1966-72 | `nalty_1977` | medium | confirmed |
| Su-25 flare dispensers | 4 ASO-2V; supply about 1 min; rarely used before about 1985 | Pact | 1981-84 | `markovsky_afghan_sky` | medium | confirmed |
| Argentine improvised chaff | 140 kg by early May | neutral | 1982 | `twz_2025_argentine_chaff` | medium | corrected |
| Pact readiness levels (take-off time, held for)| Readiness 1 / 2 / 3: 3-5 min (held 1-2 h) / 15 min (2-4 h) / 1-2 h (2-4 days) | Pact | 1984 | `fm_100_2_1_1984` p. 12-7 | high | confirmed |
| Pact air plan covers | first 1-2 h (up to 24 h if static) | Pact | 1984 | `fm_100_2_1_1984` p. 12-7 | high | corrected (page) |
| BAI nomination to mission | 48 h; division horizon at most 24 h; corps 72 h | NATO (US) | 1983-84 | `gallant_knight_1983`, `rippe_1985_airland` | high | confirmed |
| Immediate CAS achieved | Army/USAF 40-50 min, Marines 10-15 min (Korea); over 90 min, half met (Vietnam before 1965) | US | 1950-65 | `luke_2002_cas` | medium (hindsight) | corrected (Korea and Vietnam had been swapped) |
| Silence counted as consent | 5 min on the early Vietnam ARVN/VNAF net; "a preset time limit" in 1976 doctrine | US | 1960s; 1976 | `morrow_1977_tacs` | medium | corrected (5 min is not 1976 doctrine) |
| Soviet request to strike | 20-30 min from the air army KP; 17-28 min in a Nov 1961 exercise | USSR | 1961-62 | `mt_1962_kontsevoy` | medium-high | confirmed |
| NATO ground control | TPS-43E over 200 NM; TSQ-91 up to 24 h to set up; FACP 59 people, 12 h | NATO (US) | 1985 | `smart_1985_tacs` | high | confirmed |
| E-3A force | 4 on station Apr 1983; 12 of 18 planned by late 1984 | NATO | 1983-84 | `afmag_1983_usafe`, `rippe_1985_airland` | high | corrected |
| Afghan support routine | airborne alert at 3,000-3,500 m; attack up to 15-20 min; report 5-10 min after landing; suppression group 2-6 Su-17 then main group 1-2 min later | Pact | 1980-85 | `markovsky_afghan_sky` | medium | confirmed |
| JAAT time in the AAA zone | A-10 20 s or less; helicopters 40-45 s | NATO (Soviet view) | before 1986 | `mt_1986_dobedin` | medium | confirmed |
| Free armed reconnaissance corridor | 200 yd (183 m), later 200 m, each side of motorable roads; no strike within 500 m of an active village unless fired from | NATO (US) | 1965-72 | `checo_interdiction_1969`, `checo_roe_1973` | high | confirmed |
| Fast FAC training | 20 missions to train, about 40 to be highly qualified; 83% day and 26% night strikes FAC-assisted | NATO (US) | 1968 | `checo_rp1_1969` | high | confirmed |
| Reported truck kills confirmed by photograph | 7 of 103 | NATO (US) | 1969-70 | `nalty_2005` p. 89 | high | confirmed (hindsight, flagged) |
| GAU-8 test, ten passes on stationary undefended tanks | 8 immobilised; mean dive 3.3 degrees; open fire at 1,115 m; hits 0.14 direct | NATO | 1979 | `cdat_array20_1980` | high (test only) | corrected (3.3 degrees, not 3) |
| Pact armed reconnaissance group | 1 reconnaissance + 2-4 attack aircraft; data processed in 2-8 h | Pact (NATO's view) | 1984 | `fm_100_2_1_1984` p. 12-6 | medium | confirmed |
| Caravan inspection by helicopter | circling at 1,500-2,000 m; landing 200-300 m ahead; 30-35 min on station | Pact | 1980s | `rgs_afghan_2002` | high | confirmed |
| Re-attacks | 2 of the 3 GR3s lost were re-attacking | NATO (UK) | 1982 | `rafm_falklands` | medium | confirmed |
| IAF losses on the Golan | 80; about 50% to AAA; about 40% to SA-6 (author's estimate) | NATO analogue | Oct 1973 | `entwistle_1988` | medium | confirmed |
| AIM-7 and AIM-9 hits per missile fired; launches outside parameters | 11.1% and 18.6%; 35% and 26% | NATO (USAF) | Apr-Jul 1967 | `checo_1967_a2a` pp. 44-45 | high | confirmed |
| AIM-7E-2 / AIM-9E guidance failure indications | 25-30% | NATO (USAF) | 1972 | `checo_1978_linebacker` p. 30 | high | confirmed |
| USAF MiG kills by weapon | 137: AIM-7 50, AIM-9 33, guns 40, AIM-4 5, other 9 | NATO (USAF) | 1965-73 | `aces_1976` Table 4 | high | confirmed |
| Strike aircraft engaged by MiGs that jettisoned | 107 of 192 (55.7%) | NATO (USAF) | Sept-Dec 1966 | `aces_1976` p. 10 | high | confirmed |
| Exchange ratio, Linebacker | 1:0.47 (May 1972); 19:5 (Aug-mid-Oct 1972); about 17 of the last 20 kills helped by Combat Tree | NATO (USAF) | 1972 | `checo_1978_linebacker` pp. 52-53 | high | confirmed |
| Kills by infrared against Sparrow | about 200 vs about 7 (1973); about 50 vs 2 (1982) | Israel | 1973, 1982 | `cbo_1986_amraam` n. 4 | medium | confirmed |
| Argentine aircraft lost to Sidewinders | Dagger 9, Skyhawk 6, Mirage III 2, Canberra 1, C-130 1 | NATO (RN) | 1982 | `harr_1988` App. E | medium | confirmed |
| AIM-9B envelope; AIM-9L/M warhead | 900 m - 4.8 km tail-on; 9.4 kg | NATO | 1956; 1978-82 | `parsch_dusrm` | medium-high | confirmed |
| AIM-7 minimum launch range | 1,500 m (Parsch's general Vietnam-era figure); "Dogfight Sparrow" in CHECO | NATO | 1963-69 | `parsch_dusrm`, `checo_1969_a2a` | medium | corrected |
| K-13 (US view) | under 2 g below 47,500 ft; minimum range 1.0 km; no guidance within 30 degrees of the sun | Pact | 1967 | `checo_1967_a2a` pp. 19-20 | medium | confirmed |
| R-60 | adopted 18 Dec 1973; 7.2 km at about 12 km height, about a third near the ground; minimum 0.25 km; launch load 7 g; warhead 3 kg, lethal radius 2.5 m; design kill chance 0.5 (0.7-0.8 a pair) | Pact, our side | 1973 | `airwar_ru`, `ru_wiki_aam` | medium | confirmed |
| R-60M design kill chance | 0.9, close range; adoption date unverified | Pact | 1977-early 1980s | `ru_wiki_aam` | low | corrected |
| R-24 | decree 6 Apr 1981; 32 km head-on, 20 km rear (text); 10 s autonomous flight; warhead 35 kg, lethal radius 10 m; minimum 0.5 km | Pact | 1981 | `mis` | medium | confirmed |
| R-73 | adopted 22 Jun 1984; 30 km head-on; target to 12 g; designation 45 degrees; Soviet units only | Pact | 1984 | `airwar_ru` | medium | confirmed |
| MiG-23ML radar | detect 50-55 km by hand, 80-85 km under ground guidance; lock head-on 30-50 km | Pact, our side | 1976-85 | `airwar_ru` | medium | confirmed |
| Pilots' chance of being killed in a decisive encounter | levels at about 0.02 after about ten encounters | general | WWI-WWII data | `bilikam_1970` | medium | confirmed |
| Fluid Attack | pair line abreast about 1 mile apart; altitude stack a couple of thousand feet | NATO (USAF) | from mid-1970s | `anderegg_2001` pp. 59, 83-84 | high | confirmed |
| Bekaa air battle | about 90 Israeli and 60 Syrian aircraft airborne; Syria lost 23 on the first day (Lambeth) and about 85 by end of July; Israel none in air combat; guns 7% | Israel, Syria | 9 Jun 1982 | `lambeth_1984` | medium-high | confirmed |
| Pact attack geometry | groups of 4-8; independent search broken off 25-30 km beyond the front; rear-hemisphere launch 4-6 km (targets over 1,300 km/h) | Pact | 1967-68 | `mt_turchin_coop` | high | confirmed |
| Fratricide in a troop air defence exercise | up to 25% of targets fired upon were own aircraft; 600 m disengagement radius proposed | USSR (Baltic MD) | late 1960s | `mt_turchin_coop`, `mt_turchin_coop` | high (as reported) | confirmed |
| Sea Harrier CAP | rarely more than 4 aircraft; 2 AIM-9L and 2 Aden; 90 min, about 20 min on station | UK | 1982 | `usn_falklands_1983` | high | confirmed |
| Exchange ratios | Rolling Thunder about 2.3:1; 1970-73 USAF 1.88:1, USN 12.5:1; Korea about 10:1 | US | 1950-73 | `vangilder_1979` | medium | confirmed |
| European weather | ceiling under 3,000 ft and visibility under 3 miles more than 50% of the year | general | 1970s-80s | `anderegg_2001` p. 61 | medium | confirmed |
| Sapfir-23ML detect / lock, typical target | 85 / 55 km (design); the MiG-23ML is the Pact's 1985 standard | Pact | 1976-85 | `airwar_ru` | low | corrected |
| Passing our aircraft's actions down the air defence | 10-15 min by code table, division air defence chief to the guns | Pact | 1976 | `mitronin_1976` p. 11 | high | confirmed |
| Warning delay, P-15 sees a low target | 4-5 min before the forward edge; 2-3 min passing reports by hand; about 1 min more to plotting boards | USSR (front AD) | about 1966-67 | `mt_franchuk_front` p. 5 | high | confirmed (added by the check) |
| Warning data reaching the active air defence | 5-8 min late and more | Pact-equipped (UAR, Egypt) | 1967 | `mt_umnov_1968` p. 8 | medium | corrected (Egypt, not Syria) |
| Fighter control from plotting boards | delay of 3-6 min | VPA (Pact-trained) | 1966-67 | `mt_1973_nvn` p. 14 | high | confirmed |
| Fighters over SAM units that cannot fire | floor 3,000 m; alert zones 1,500-2,000 m (low targets), 10-12 km centred 50-60 km inside own territory (high) | Pact | 1967-76 | `mitronin_1976` p. 9, `mt_turchin_coop` p. 6 | high | confirmed |
| Kremniy-2 IFF | 12 code filters (1-8 peace, 9-12 war); changed every 1-6 h on a secret schedule; 668 MHz | Pact | late 1950s on | `soviet_iff` | medium (tier 4) | confirmed |
| 1L24-2 interrogator (Osa-AKM) | 920 W (Kremniy-2M), 870 W (Parol); launch block only in Parol mode II, 1 km gate, +-1.9 s | USSR | 1980s | `voenizdat_osa_1991` pp. 53, 64 | high | confirmed |
| Foveal detection range | R = 1006 sqrt((C0 - 1.565) A) m (A in m2, C0 in %); empty-field myopia x0.5; glimpse about 1.65 s | general (US model) | 1974 | `nwc_tp5709_1974` | high (formula) | confirmed |
| Air-to-ground acquisition | P = 0.2 / 0.5 / 0.8 of P_max at 0.625 / 0.5 / 0.375 of the shorter of unmask range and visibility; TV lock about 9 s | NATO (US study) | 1978 | `nwc_tp6005_1978` | medium | confirmed |
| Acquisition against speed | probability not affected from 350 to 700 kt; range differences not significant | NATO (US) | 1964 | `jones_atg_1974` (Dyer) | low | corrected |
| Vehicle recognition by visible and infrared sensors | at most about 3 km in good weather, about 1 km in poor (the eye is SAM3D's inference) | NATO | 1977 | `crawford_1977_rand` | medium | corrected |
| Ground observer detection of jets | mean 6,200 m slant | general | 1967 | `humrro_1968_ground_observer` | medium | confirmed |
| Flying hours | TAC 16.3 h a month (FY80), 18.3 (FY84-85); NATO guide 15 h a month; Soviet 125-160 h a year (US view, 1969 MiG-17 era); Su-25 224 h and MiG-23 84 h per aircraft (1985) | NATO; Pact | 1969-86 | `gao_1986_flying_hours`, `hansard_1981_nato_hours`, `cia_1969_soviet_tac_training`, `markovsky_afghan_sky` | high / medium | confirmed |
| TAC aircrew with combat time | under 25% | NATO (USAF) | Aug 1978 | `vangilder_1979` | high | confirmed |
| Afghan MANPADS and damage | about 50 launches and 5 hits (1984); 1 combat-damage forced landing per 4,605 h (1985) | Pact | 1984-85 | `markovsky_afghan_sky` | medium | confirmed |
| Warning from enemy broadcasts | 34% of B-52 strikes alerted, average 8.5 h (Dec 1966); 5%, under 30 min (Apr 1967); strike warnings in the clear 10-30 min before bombs | general | 1966-69 | `nsa_purple_dragon_1993` pp. 22-26, 69 | high | confirmed |
| Relay of intercepted traffic | about 5 min; new US signal instructions broken in as little as 6 h; brevity codes within 6 h | general | 1960s-71 | `nsa_purple_dragon_1993` pp. 60-61, 68-69 | medium | confirmed |
| Pact GCI capacity | an air army with 3 division and 9 regiment CPs: 30-40 groups at once | Pact | 1962 | `mt_1962_front_aviation` | medium | confirmed |
| Coalition coordination means | deployed within 30 min (10-15 some); reliability at least 0.98; tropo links 250-500 km | Pact | late 1960s | `mt_coord_wp_air` p. 6-7 | medium | confirmed |
| Pact ground-signals intercept (NATO view) | VHF about 40 km, HF ground wave about 80 km, artillery radar about 25 km; ground radio located in 20-25 s; VHF DF +-3.5 degrees | Pact | 1984 | `fm_100_2_1_1984` p. 15-2 | medium | corrected (DF +-3.5, not 3-5) |
| HAVE QUICK | introduced 1980; frequency hopping keyed by time of day, word of day and net number; ECCM, not encryption; fielding in USAFE by 1985 not found | NATO | from 1980 | `fh_radios` | medium | corrected (format unverified) |
| NATO E-3As | 4 at Geilenkirchen April 1983; 18 planned by 1985 | NATO | 1983 | `afmag_1983_usafe` | medium | confirmed |
| Bekaa control | E-2C downlink, voice from the command post, four-ships fed in one at a time, fights of 1-2 min, about 26 MiGs in 30 min | Israel | 9 Jun 1982 | `grant_2002` | medium | confirmed |
| Tactical Leadership Programme | 8 four-week courses a year, 15 sorties per crew | NATO | 1983 | `afmag_1983_usafe` | high | corrected |
| Iraq (CIA) | only the leader knew the target; rigid profiles; piled on easy targets; nearly 90 lost in six weeks, 300-325 in seven years | Pact client | 1980-87 | `cia_1987_iraq_af` | medium (adversary estimate) | corrected (high-altitude bombing is the early years only) |

Game values with no source, and the faction parameter tables (initiative, retasking latency, plan adherence, press-on,
radio and emission discipline, skill, reaction delay and chance of no reaction by skill, jettison and abort shares by
situation, the pilots' hours as skill) are in `factions_cases.md` section 1, `reactions.md` section 2.6 and
`perception.md` section 3. They are SAM3D estimates, ordered and bounded by the sources. They are not data until they are
set in `data/gameplay/` and labelled so.

## 4. Post-period items excluded from the 1985 war

Listed only as a later war's options. Dates as the topic files give them.

| Item | Date | Where met |
|---|---|---|
| AIM-120 AMRAAM (launch and leave); IOC | requirement 1978; planned 1989; in service 1991 | aam_weapons, air_combat, c2_retasking |
| R-27R/T formal adoption; R-27ER/ET | 1987-1990; about 1990 | aam_weapons, air_combat |
| R-73 exported (GDR); MiG-29 with Pact allies | 1988 | aam_weapons |
| R-24M | 1986 | aam_weapons |
| MiG-23MLD with two R-73 and a helmet sight | later variants, from 1984, not in service | aam_weapons |
| MiG-23MLD series (at the edge); the ML is the 1985 standard | from 1984 | air_combat |
| R-60M kill of a Pakistani F-16A | 1986-87 | aam_weapons |
| R-77; AIM-9X; ASRAAM; Mark XV IFF; Magic 2, Super 530D (dates unchecked) | 1990s onward | aam_weapons, air_combat, comms_emcon |
| Tornado F3 with Foxhunter as combat-ready | 1987-89 | aam_weapons |
| Desert Storm Sidewinder kills; the Gulf War's switch to medium altitude and its figures | 1991 | all topics |
| JTIDS / Link 16 in fighters | late 1980s onward (1981: planned only for the EF-111A and the E-3A) | c2_retasking, air_combat, comms_emcon, our_side_air |
| LANTIRN, TALD decoys, NVG fast-jet strikes | 1987; 1987; late 1980s-1990s | all topics |
| Kill boxes as a term; killer scouts; JSTARS; ABCCC push CAS; AWACS strike control or retasking | 1991 | c2_retasking, opportunity, air_combat, factions_cases |
| OA-10A as a FAC; F-16Cs as the F-4G's killers | 1987-88; June 1989 in place (start year 1987 unverified) | c2_retasking |
| NATO's airborne holding and pop-up tasking for the A-10 | spring 1986 | opportunity |
| Multi-service brevity list (FM 90-38) and its words Magnum, Spike, Mud, Naked, Defending, Notch; "Fox three" for an active radar missile | April 1997; 1990s | reactions, c2_retasking, air_combat, comms_emcon |
| Compass Call based in Europe (43d ECS at Sembach) | 1 Oct 1986; aircraft 1987-91 | comms_emcon |
| HAVE QUICK II; SATURN; SINCGARS | production 1986 (tier 5); 1992; 1988 | comms_emcon |
| Rivet Joint as near-real-time provider; GPS time of day for HAVE QUICK | modern | comms_emcon |
| Stinger in Afghanistan; the Soviet 4,500 m floor; Su-25 extra armour and flare booms | late 1986; late 1986; Aug 1987 (flare booms undated, likely 1986-87) | reactions, perception, routing, factions_cases |
| ALR-74 RWR; missile approach warners on fast jets | not fielded by 1985; none shown | reactions |
| El Dorado Canyon (comm-out refuelling, routes round Europe) | 14-15 Apr 1986 | routing, comms_emcon |
| SAAF long toss and 4-5 nm pull-up; SAAF and Angola air combats | 1987-88 | routing, air_combat, factions_cases |
| Austrian Drakens; Croatia's improvised air arm; Kosovo | 1988; 1991; 1999 | routing, air_combat, factions_cases |
| A-50 Mainstay in Pact control | 1984-85 edge, formally later | air_combat, factions_cases |
| Parol IFF in the NVA and other allied armies | planned 1989-90, not carried out | our_side_air |
| Osa-AKM automatic launch block on a guaranteed friend | needs Parol mode II; AKM in Soviet service from 1980 | our_side_air |
| Czechoslovak unified air defence and Masiv; a division's air defence as one 3,000 m fire zone | 1990-91 | our_side_air |
| FM 100-103 (1987); ATP-40(C) (2002); JP 3-52 | 1987; 2002; 1990s | routing, our_side_air |
| Computer-aided retasking of interdiction (Schoeck, AFIT) | 1987 | factions_cases |
| Soviet readiness as described by Air Force Magazine | March 1989 | factions_cases |
| Tornado ECR hours (planned 180 h a year) | 1989 for 1990 and later | perception |
| East German air intercept: operative groups, about 600 receivers, 14 kHz-40 GHz; An-26 airborne centre | state of 1989-90; build-up from Aug 1985; Dresden LuAZ in the 1986 structure | comms_emcon |
| Tornado GR1 route practice of an undated memoir | may be after 1985 | routing |
| Look-down fighters (MiG-29, MiG-31, Su-27) facing NATO | mostly after 1985; the FRG's 1987 analysis is hindsight | routing |
| Israeli evaluation of a defected MiG-23ML | 1989 | air_combat |
| Stinger's identification problem, Mi-24 night attack of April 1986, Dobedin's July 1986 Soviet reading | 1986 | perception |

## 5. Disagreements and which was used

1. **How low NATO flew.** AFM 51-40 gives 200-500 ft; training was 500 ft for bombers and 100 ft for RAF Buccaneers and
   Jaguars in special areas; the FRG's peacetime band was 150-450 m; war heights were expected lower. Used: per type and
   phase, 100-250 ft for RAF and Luftwaffe strike in war, 200 ft terrain following for the F-111 at night and in cloud,
   500 ft for bombers and the night, 50 ft on the last leg for fighter-bombers; peacetime floors are not the war.
2. **Bypass or not.** The Soviet open press and a teaching aid say avoid the air defence; Babich says a full bypass works
   only on a staff map. Used: minimise believed exposure, never require none, and accept the target's own defence on the
   last leg.
3. **Who decides the route in the air.** NATO: the crew chooses whether to use a minimum risk route and has a free choice of
   attack. Pact (NATO's view): the plan holds unless ground control changes it. Iraq: keep the profile or be punished.
   These are faction differences, used as such.
4. **Was there a Soviet FAC?** Streb (1980) says no; FM 100-2-1 (1984) says one at regiment when air is allotted;
   Afghanistan had FACs with every battalion. Used: FM 100-2-1 for a Pact war in Europe and Afghan practice for local wars;
   the difference is probably a real change between the 1970s and mid-1980s.
5. **Who approves a US airborne divert.** The 1976 test rules: the Army; J-SAK (1984): the TACC for BAI, Army informed;
   1986: the ASOC when authorised. Used: a CAS divert needs the corps' agreement; a BAI divert is the ATOC's alone.
6. **How fast is immediate CAS.** Goals 15-20 minutes; achieved 40-50 (Korea, Army/USAF) and 10-15 (Marines). Used: the
   achieved figures as typical, the goals only with airborne alert.
7. **How common was in-air retasking.** Dobedin: undesirable and sometimes impossible; Hoak (1987): considerable;
   Schoeck (1987): airborne interdiction diverts not yet possible. Used: CAS and BAI flights can be retasked in low or
   medium threat; interdiction and deep strike are hardly retasked in the air.
8. **How much the Pact hunts.** Bukhantsev (1970): hunting is one of three methods; MAI: free search by up to a flight;
   FM 100-2-1: reservations until air superiority. Used: report and request is the default; hunting only in given zones
   and once the air is held. Bukhantsev wins where he differs from FM 100-2-1.
9. **The A-10's gun against reality.** The 0.8 tanks a pass is for stationary undefended tanks; finding the target is the
   real limit. Used: apply the effect only once a target is found and identified; draw finding and identifying separately.
10. **One pass or several.** Used: one pass by default where defences are seen; a second pass from a new heading, with a
    higher loss chance, for NATO and the Pact; neutrals and rebels rarely re-attack.
11. **How bound the Soviets were to ground control in 1985.** Western writers: bound and predictable; Pennington (1985):
    a reform under way; Johnson (1986): the talk ebbing by 1984. Used: skill dependent. The average Pact unit is control
    bound; the best units (the national air defence after 1984) pass initiative to the flight leader at contact. Strike groups
    go on to the planned target when control is lost; fighters hold or return.
12. **The Bekaa's first day.** 23 (Lambeth) or 29 (Cordesman) Syrian aircraft. Used: Lambeth. Syrian claims are kept as
    claims, which is the written record rule in the enemy's journal. The same for the R-24 claims.
13. **What the E-2C was.** Dubrov: an airborne command post; Lambeth: it advised and the F-15 could search itself. Used:
    Lambeth for NATO practice, Dubrov for what a Pact staff believes.
14. **Soviet timing.** 1962 doctrine 20-30 minutes request to strike against FM 100-2-1's takeoff times: both, as parts of
    one budget (transit and the checkpoint take 10-15 minutes).
15. **Zones or one zone, and corridors, for our side.** Levchenko held zones; Turchin one zone; Mitronin (1976) plans both:
    zones against the first massed strike and static objects, one zone by altitude when the air defence is overwhelmed.
    Used: Mitronin as the rule, Turchin for the geometry. Corridors are kept (the regulations of 1984); Turchin's objection
    is modelled as the enemy's ability to learn and use them.
16. **Warnings or IFF.** Turchin: warnings cannot replace recognition. Used: both. The Osa has IFF; the regiment's ZSU-23-4
    and Strela teams identify by sight and notice only (no source found for an interrogator).
17. **Speed and acquisition.** Dyer's range drop at higher speed is not statistically significant. Used: probability flat
    from 350 to 550 kt, range only as a weak trend.
18. **SAM site visibility.** NWC TP 6005 counts SA-2 pads and roads as a strong pattern; CHECO and AGARD say sites were
    seldom seen unless firing. Used: an Osa battery is a small pattern with low contrast until it fires, then large and
    high.
19. **Day against night.** The 1968 figures make fixed-wing night hunting poor; Afghan helicopter night hunts worked against a
    curfew with lights showing. Used: night find chance low for fixed-wing, raised only for a target showing lights or moving
    on a road under a known ban.
20. **Damage claims.** Official totals and photo checks differ by an order of magnitude. Used: the enemy's journal records the
    claim; its picture discounts claims only when photo control is flown; the truth stays in the debrief ledger.
21. **Radar warned before intercept, or the reverse.** Used: radar first for a strike in the air; intercept adds identity,
    intent, target and timing (an inference from PURPLE DRAGON).
22. **Teaball's result.** 30 MiGs for 10 (Boyne) against 19 for 5 (CHECO). Different windows; the CHECO figure where a number
    is needed.
23. **Kremniy and Parol.** Our Kremniy-2M fighters answer only Kremniy interrogators. Used: Kremniy-2M as unreliable and
    spoofable in 1985.
24. **NATO attack height in the data.** The commander's `medium_altitude_man` quirk against the period's low level. Used: low
    by default; medium altitude once the believed defence is below the phase threshold; the quirk made rare.

Smaller disagreements (SA-7 counts, the SA-2's missiles per kill, Falklands SAM and Sidewinder tallies, the R-60 and R-24
figures between a text and its table, the R-73 designation angle, the MiG-23MLD's date, AIMVAL-ACEVAL's year, KAL 007's
ground controller, PURPLE DRAGON's start, Compass Call's target, the Dugman 5 retasking) are in each topic's section 4.

## 6. What was not found, and documents to acquire

Merged and deduplicated. Several gaps are there only because the web search budget ran out (section 7).

Numbers and rules no period source gave:

- Leg lengths, turn-point counts, IP distance and a stand-off from a SAM ring (routing).
- The launch-detection range by eye as a number, and the 9M33's smoke trail as pilots saw it (reactions, perception).
- The share of sorties retasked in flight in 1980-85 (c2_retasking).
- Corridor widths and heights for our aircraft, the notice lead time by regulation, IFF reply and failure rates, and what the
  Osa-AK (not the AKM) carried as interrogator (our_side_air).
- Turn rates, sustained g and specific excess power per type, for the point-mass model (air_combat).
- Joker and bingo fuel practice as numbers; NATO brevity words of 1985 (only the TRADOC TT 17-50-3 glossary of 1983 is
  known: Bandit, Bingo, Blind, No Joy, Tally-ho, Visual, Winchester, Have Quick, Vinson); whether "Fox three" meant the gun.
- Sensor in-service dates: Pave Spike, Pave Tack, Pave Penny, Kaira-23, Klen-PS, Mi-24 sights, helicopter NVGs (perception).
- When HAVE QUICK reached USAFE's aircraft; whether KY-58 was in fighters by 1985 (comms_emcon).
- NATO's own radio procedure in 1985 (check-ins, call-sign rotation); Pact jamming of NATO UHF nets as a named jammer.
- Soviet and Pact peacetime flying hours for the 1980s from a period source; RAF and Luftwaffe hours for 1985.
- The in-flight report formats (NATO INFLTREP, ATP-27(B); the Soviet «доклад с воздуха»), and Soviet photo control of strike
  results.
- NATO's day-1 plan for interdiction against SEAD-first in 1985; the E-3A's wartime role in 1985 in a primary source;
  ABCCC in Europe in 1985.
- The neutral and the rebels: nearly everything. Neutral air radio, rules of engagement and attack pairs (Austrian, Swiss,
  Swedish); the Argentine decision chain at Bluff Cove.
- Latency of British and German CAS in the Central Region; the A-10 interview (`bush_a10_simhq`) re-read or replaced by a
  USAFE or 81st TFW document.

Documents to acquire:

- US: TACM 3-1 / MCM 3-1 (classified, not to be sought); *USAF Fighter Weapons Review* 1978-84; ATP-27(B); ATP-40 / STANAG
  3805 in 1980s editions; TACR 55-46 (1976 test, 1986); TACM 2-1 (1978); TRADOC Pam 525-45 (J-SAK) in full; `jsead_1982` in
  full; FM 90-21 (1983); TACP 50-20 (1979); TC 1-201 (1984); FM 44-1 (1983 edition); AFM 51-40 (1983 revision); R. L. Shaw,
  *Fighter Combat* (1985); Skinner, *USAFE: A Primer of Modern Air Combat in Europe* (1983); Nordeen, *Air Warfare in the
  Missile Age* (1985); RAND R-3000 and R-2070; AIMVAL and ACEVAL reports; GAO C-MASAD-82-6; Red Baron III; the Ault report
  (1969); CIA NI IIM 84-10008 (1984, the owner to download); the CIA's January 1985 paper on the Pact air operation; NSA's
  *American Cryptology during the Cold War*, Book IV, and *Teaball*.
- Soviet and Pact: НПП and КБП for frontal aviation; a manual on the control and target-identification posts and the
  авианаводчик; *Авиация и космонавтика* 1980-85 (Tomilin No. 12, 1980; Dubrov 1978 and 1983); «Зарубежное военное
  обозрение» 1982 No. 2; the Lazur and Vozdukh-1 descriptions; Soviet practical aerodynamics manuals for the MiG-21 and
  MiG-23; flight manuals for the R-13M, R-60, R-24 and the Sapfir-23ML; the R-832M and R-862 descriptions; Markovsky's
  books; *Военная мысль* 1975 (fighter aviation in front air defence, a damaged PDF) and Levchenko 1967; the Polish MON
  manual `mon_2112_80` for the Osa-AK's interrogator; Czechoslovak, Polish and East German period journals.
- Others: Lambeth R-3000's Dubrov translation checked against the original; Argentine accounts (Moro, *La guerra inaudita*,
  1985); RAF Historical Society Journal 30; Freedman, *Official History of the Falklands Campaign* (2005); Cordesman and
  Wagner, vol. I (1990); Hansen, *International Defense Review* 1984; the Hansard written answers of 1985-86 on RAF hours.

## 7. How the verification went

Each topic's findings were re-read against the cited texts (archive.org full texts of DTIC, Army and CIA documents,
and the journals and press pages named in each file). Counts are the rows of each file's verification table. A row can
cover several findings.

| Topic | Rows | Confirmed | Corrected | Unsupported | Other |
|---|---|---|---|---|---|
| routing | 26 | 15 | 10 | 1 | |
| reactions | 34 | 17 | 14 | 3 (two only partly: the GAO and Litton texts were unreadable) | |
| c2_retasking | 24 | 12 | 12 | 0 | |
| opportunity | 32 | 18 | 8 | 5 (the A-10 interview) | 1 moved to post-period |
| aam_weapons | 24 | 18 | 4 | 2 | |
| air_combat | 27 | 19 | 6 | 2 | |
| our_side_air | 31 | 18 | 10 | 3 | |
| perception | 20 | 11 | 7 | 2 | |
| comms_emcon | 25 | 14 | 9 | 1 | 1 row of three findings not re-checked |
| factions_cases | 29 | 14 | 11 | 3 | 1 moved to post-period |

Most corrections were small: a wrong page, a swapped pair of figures (c2-07), a date that belonged to another role or a
later year (the CF-104 strike profile, the Tornado GR1, the 355th and 388th pod formations), a Wikipedia or forum page used
as a source (now a pointer only), a view attributed to the wrong side, or a US inference presented as the Pact's rule. The
unsupported rows rest on a source that could not be read again (the A-10 interview, Johnson 1986, Red Baron's later
volumes, the DIA paper of 1977, the 1986 Military Thought article), on Wikipedia, or on a dead page (Parol's date).
Unsupported items are marked "(unverified)" in the topic files and are not data.

The web search budget ran out in several topics. In the research itself: reactions (200 searches used before the end),
aam_weapons (before the Czechoslovak, Polish and East German journals and Soviet *Zarubezhnoye voyennoye obozreniye* could be
searched), perception (the sensor in-service dates were never sourced), comms_emcon (thirteen searches), and our_side_air
(before the Falklands, Iran-Iraq, Polish and Hungarian searches). During the check: reactions, opportunity, aam_weapons,
air_combat, perception, comms_emcon and factions_cases could fetch only documents at known addresses, so nothing new was
found by searching. Topics left partly unchecked for that reason or because a source could not be fetched:

- reactions (the GAO 1982 and Litton RWR texts and the Soviet encyclopedia entry);
- opportunity (everything resting on the A-10 interview, which the Wayback Machine refused);
- aam_weapons (the R-13M date, the neutral without missiles);
- air_combat (Combat Tree, the Austrian analogue);
- our_side_air (Parol's 1977 date; BU part IX, FM 44-3, FM 100-42 and FM 1-103 not re-read);
- perception (the DIA 1977 paper and the 1986 Military Thought article, the 1985-86 Hansard answers);
- comms_emcon (the museum fact sheet, one row of three findings);
- factions_cases (Johnson 1986, Red Baron and the Dugman 5 sources, which could not be fetched).

Routing and c2_retasking report no budget problem, but routing did not re-check its `dtic_linebacker` item, its
pre-period CF-104 profiles, its two design inferences and the HC Deb 30 Oct 1987 answer, and c2_retasking did not re-read
Streb, Olmsted, the MAI aid, Pstygo, `kz_2007_pan` or `pchelov_2018_pan`.

A period scan was run on every topic. No post-1985 tactic, weapon, data link or brevity word is presented as 1985 practice
after the corrections. Items that had slipped in were moved to section 4: the R-27R and the MiG-23MLD's R-73 load, "kill
box", the 1989 Israeli evaluation of a MiG-23ML, the 1989 Soviet readiness article, Schoeck 1987, and the Tornado ECR hours.
