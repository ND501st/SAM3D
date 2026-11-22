# Faction profiles and case studies for the hot phase (research note, November 2026)

How each of SAM3D's four adversaries behaves once a raid is airborne: initiative, flexibility, adherence to the plan,
tactical skill, risk, aggression and training. **The period is 1980-85; the air war's year is 1985.** Everything below is
dated. Evidence from 1991 or later appears only as flagged hindsight (section 5). Source ids are the ones in
`docs/SOURCES.md`, or new ones listed in section 6.

Three warnings about the evidence:
- Most of what we know about Soviet air practice comes from the adversary: US Army field manuals, CIA papers and RAND.
  Soviet self-criticism in *Aviatsiia i kosmonavtika*, as analysed by Johnson (1986), and Markovsky's Afghanistan history
  are the nearest we have to the operator's own view.
- None of the authored numbers in the faction files (initiative, latency and so on) exist in any source as numbers.
  The table in section 1 is a set of SAM3D estimates, ordered and bounded by the sources.
- Many period cases are small or one-sided. Each is used for a pattern, not as a statistic.

## 1. Summary for SAM3D's design

**What the period evidence says.**
- **Soviet-style air forces (the Pact) plan everything before take-off and change little in the air.**
  - The flight plan, the route through friendly air defence and the coordination are fixed before launch. US Army
    analysts in 1984 said this "inhibits flexibility" once missions are under way (`fm_100_2_1_1984`, ch. 12).
  - Fighters fly under ground control (GCI). Western writers said losing it meant aborting (`lambeth_1984_r3000`, quoting
    Skinner).
  - Soviet writers criticised their own exercises for stereotyped patterns and "fettered initiative". Johnson's content
    analysis found that the emphasis on initiative rose from the mid-1970s and fell back in 1983-84
    (`johnson_1986_nps_initiative`).
  - The same centralisation makes re-allocation between waves fast: readiness levels with 3-5 minute and 15 minute
    take-offs answer immediate requests (`fm_100_2_1_1984`).
- **In Afghanistan the Soviets did adapt, but by changing the sequence on the ground, not in the air.** Examples
  (`markovsky_afghan_sky`):
  - a set target-designation, suppression and main-group sequence;
  - a Su-17M3 shot down on its sixth (!) pass near Kandahar in summer 1983, blamed on stereotyped attacks
    («шаблонность атак»); separately, Mi-24 crews held 1200-1500 m between machines because "the template was punished
    at once" (corrected 15 Nov 2026: the interval and the phrase are the helicopters', not a change after the Su-17 loss);
  - an order of 1981 forbidding attacks on unidentified targets (which Markovsky says was broken round the airfields).
- **Soviet-trained client air forces show the lower bound of the Pact model.**
  - Syria in 1982 was cut off from its GCI by jamming. Its pilots broke into pairs and singles; some ejected the moment
    they knew they had been seen (`lambeth_1984_r3000`).
  - Iraq in 1980-87: only the leader knew the target. Pilots kept to rigid profiles for fear of punishment, piled on the
    easiest targets, and bombed from high altitude at high speed (`cia_1987_iraq_af`).
- **NATO in 1985 trained for decentralised, low-level execution.**
  - Red Flag (from November 1975) aimed to give pilots their "first ten combat sorties" (`vangilder_1979_cgsc`,
    `rafhs_j20_1999`).
  - RAF Tornado crews flew 250 ft daily and 100 ft at Goose Bay. Red Flag packages of 60-70 aircraft went through in
    20-30 minutes with "no radio calls required" (`rafhs_j20_1999`).
  - The Tactical Leadership Programme at Jever trained multinational package leaders: 8 courses a year, 15 sorties per
    crew (`afmag_1983_usafe`).
  - An aircraft could divert to any allied base (cross-servicing, from 1978), and the allied tactical operations centre
    (ATOC) fed its next target to the receiving base, i.e. after landing, not in the air (`afmag_1983_usafe`; corrected
    15 Nov 2026).
- **Small and improvised air arms show the most individual initiative but the least coordination.**
  - Crippa's lone MB-339 at San Carlos chose its own attack (`ellitoral_crippa`).
  - Biafra's five MFI-9Bs flew rocket raids at tree-top height (`biafra_minicoin`).
  - Katanga's single Fouga held the UN at Jadotville (`katanga_fouga`).
  - Small air forces without replacements husband aircraft as soon as the threat rises. The SAAF cut its F1 operations
    once Cuban MiG-23MLs appeared in 1987 (`angola_saaf_1987`).
- **The period's air combat evidence favours the trained side heavily.** Red Baron (1967) found that about half of the US
  pilots shot at were unaware of the MiG until it was within two miles (`redbaron_1967_wseg116`; unverified, not re-read).
  Bekaa 1982 ended 85 Syrian aircraft downed by the end of July for no Israeli aircraft lost to enemy fighters (Israel
  did lose aircraft to ground fire) (`lambeth_1984_r3000`).

**Period check on the current data.**
- **The NATO commander's `medium_altitude_man` quirk is not 1985 Central Region practice.** NATO's tactical aircraft there
  trained and planned to fly low. Medium-altitude attack was used in the period, but only where the defences were already
  broken (Linebacker 1972, Bekaa 1982).
  - Recommendation: make `medium_altitude_man` rare for NATO 1985 (weight about 0.1 at "superior").
  - Give the NATO commander `low_level_purist` or `by_the_book` instead.
  - Keep medium altitude as a phase-dependent choice: allowed once the believed air defence falls below the phase's
    threshold.
- **NATO's phase 1, the "rollback" of air defences until fewer than 45% work, is closer to 1991 than to 1985.**
  - The period precedent is Israel's Bekaa strike of 1982, which suppressed first and then went on. JSEAD doctrine
    existed by 1982 (`jsead_1982`).
  - In 1985 NATO probably planned counter-air and interdiction from the first day, because the land battle would not
    wait (SAM3D inference, unverified: `afmag_1983_usafe` gives only the about 15 minutes of flying time between the
    forward elements, not a day-1 plan; see open items).
  - Recommendation: keep the phase, but let interdiction and support sorties start on day 1 at a reduced share.

**Proposed hot-phase parameters per faction.** These are SAM3D estimates. The sources give the order and the rough
spread, not the values. 0 = none, 1 = most.

| Parameter | Rebels | Neutral | Warsaw Pact | NATO | Basis |
|---|---|---|---|---|---|
| Initiative of the flight leader in the air | 0.8 | 0.6 | 0.25 | 0.7 | Pact: `fm_100_2_1_1984`, `johnson_1986_nps_initiative`, `lambeth_1984_r3000`; NATO: `rafhs_j20_1999`, `vangilder_1979_cgsc`; small air arms: `ellitoral_crippa`, `biafra_minicoin` |
| Retask in flight possible | only by the pilot's own choice | by the leader | only by ground control (GCI or forward air controller); an armed-recce aircraft "occasionally" diverted to a request | by the FAC; ATOC retargets a diverted aircraft at its receiving base, not in the air; AWACS retasking of strikers not shown for 1985 (unverified) | `fm_100_2_1_1984`; `afmag_1983_usafe` |
| Latency to retask an airborne flight | n/a (no net) | 5-15 min | 10-30 min, often never | 3-10 min | SAM3D estimate |
| Latency to launch a new tasking from the ground | hours | 30-60 min | 3-5 min (readiness 1), 15 min (readiness 2), 1-2 h (readiness 3) | 15-60 min (alert pads), next ATO otherwise | `fm_100_2_1_1984` for the Pact; others SAM3D estimates |
| Adherence to plan (route, timing, target) | 0.3 | 0.6 | 0.9 | 0.7 | as above |
| Press-on under fire (aggression) | 0.6, highly variable | 0.4 | 0.6 strike, 0.4 fighters without GCI | 0.6 | `usn_1983_falklands_lessons`; `lambeth_1984_r3000`; `checo_1969_tactics` |
| Radio discipline (silence on the attack) | 0.3 | 0.6 | 0.8 strike ("reduced to a minimum or prohibited"), fighters talk to GCI | 0.8 (comm-out packages) | `fm_100_2_1_1984`; `rafhs_j20_1999`; `redbaron_1967_wseg116` for the failure mode |
| Emission discipline (own radars, jammers) | 0.2 (few emitters) | 0.5 | 0.6 | 0.7 | SAM3D estimate |
| Air combat skill (mean) | 0.2 | 0.5 | 0.5 (0.3 for clients) | 0.8 | `lambeth_1984_r3000`; `vangilder_1979_cgsc`; `cia_1987_iraq_af` |
| Attack targets of opportunity | 0.7 (anything that looks military) | 0.4 | 0.2 (only on armed-recce or free-hunt tasks; report, do not attack) | 0.5 (armed recce, FAC handoff) | `fm_100_2_1_1984`; `markovsky_afghan_sky`; `ellitoral_crippa` |
| Turn back when spotted or jammed | 0.4 | 0.5 | strike 0.2, fighters 0.6 if GCI lost | 0.2 | `lambeth_1984_r3000` |

**How officers and pilots should modulate them.** These are SAM3D design proposals, with the period pattern each one
rests on.
- **Faction first, then person.** The faction value is the doctrine. An officer's traits move it by at most about ±0.2,
  and a pilot's state by about ±0.15. A Pact flight leader with high boldness and creativity is still a Pact flight
  leader: he may take the free hunt he was allowed. He does not invent a target change in the air. Johnson found no
  example of one.
- **Rigidity and methodical raise plan adherence and retask latency.** Creativity lowers both. Boldness raises press-on
  and attacks on targets of opportunity. Composure lowers turn-back when spotted or jammed. Airmanship sets air combat
  skill, and leadership holds the flight together when its net is jammed (Syria 1982 is the failure case).
- **Pilots as people.** Three numbers per pilot (a SAM3D proposal):
  - **Experience** in combat sorties. The "first ten sorties" are the most dangerous, which is why Red Flag existed.
    Under 25% of TAC aircrew had combat time in 1978.
  - **Currency** in flying hours. NATO's guide was 15 hours a month, about 180 a year. In Afghanistan in 1985 the Su-25
    flew 224 hours per aircraft and the MiG-23 84.
  - **Nerve.** Jettison and break under attack: 47 US aircraft jettisoned under MiG attack in September 1967. Syrian
    pilots ejected when spotted in 1982. An Afghan Su-17 leader refused the FAC's help in 1984 and bombed his own
    battalion.
- **Quirks that fit the period and should stay:** loss_shy (neutral), by_the_book and massed_blows (Pact), press_on
  (Argentina 1982), night_owl and flies_himself (rebels: Biafra, Katanga), feint_artist and decoy_lover (Bekaa 1982,
  Vietnam), sead_first (Bekaa 1982; Wild Weasels in Vietnam from late 1965).
- **Through the first-person pipeline.** None of this reaches the player as numbers. It reaches the crew as what they see
  and hear: the raid kept its timing, a flight broke off, aircraft jettisoned and turned for home, a lone jet came back
  for a second pass. The records then carry it as the crews wrote it down.

## 2. Findings by faction

### NATO (superior)

- **nato-01 Low level is the 1985 attack height in the Central Region.** RAF Tornado GR1 crews trained for low-level
  airfield attack at 250 ft daily and 100 ft at Goose Bay (Witts describes the force "by the late '80s"; the GR1 was in
  service from the early 1980s). Buccaneer crews worked up to 100 ft for Red Flag in 1977. [`rafhs_j20_1999`, Witts and
  Pitchfork contributions, a 1999 retrospective; date 1977-90]
- **nato-02 Babich's Soviet reading of US survivability supports this.** In 1984 a Soviet writer gave the US "survival
  corridor":
  - 60-90 m gave over 75% survival;
  - 30-60 m and 90-200 m gave 50-75%;
  - under 30 m or over 200 m gave under 50%.

  He also said that doubling speed from 370 to 740 km/h cut vulnerability fourfold. [`mt_1984_babich`; 1984]
- **nato-03 Red Flag trains the first ten combat sorties.**
  - Founded November 1975 from studies showing most losses fell in a pilot's first 8-10 combat sorties.
  - Aggressor squadrons (first formed October 1972, later on F-5E) flew Soviet tactics in Pact colours.
  - TAC Manual 51-50 had aircrew fly Red Flag every six-month cycle.

  [`vangilder_1979_cgsc`; `rafhs_j20_1999`; `nellis_red_flag`; 1975-85]
- **nato-04 Packages ran comm-out to a FRAG received the afternoon before.** At Red Flag 60-70 aircraft went through
  corridors in 20-30 minutes with no radio calls required. [`rafhs_j20_1999`, Pitchfork; 1977-80s]
- **nato-05 Multinational package leadership was trained from 1978-79.** The Tactical Leadership Programme was formed in
  January 1978 at Fürstenfeldbruck by six nations (Belgium, Canada, Germany, the Netherlands, the UK and the US) as
  two-week seminars on NATO tactics (not an academic course: the academic branch came only at Florennes in 1989), and
  moved to Jever in September 1979, where a four-week course with a flying phase began. By 1983 it ran 8 four-week
  courses a year (one week of seminars, three mostly flying) with 15 sorties per crew, and every USAFE fighter wing had
  taken part in 1982. [`afmag_1983_usafe`; `tlp_official_history`; 1978-85]
- **nato-06 NATO could re-target between sorties; in-flight retasking of interdiction is not shown for 1985.**
  Cross-servicing from 1978 let a diverted aircraft refuel and rearm at any allied base, and the ATOC "will feed its next
  target to the receiving base" (after landing, not in the air). EIFEL, a high-speed automated system, replaced manual
  tasking procedures. [`afmag_1983_usafe`; 1978-83] A 1987 AFIT thesis designed a decision aid for retasking
  interdiction aircraft in flight (`schoeck_1987_ai_retasking`; 1987, after the period, not re-read): it shows the
  question was studied in 1987, not that the practice existed in 1985 (corrected 15 Nov 2026).
- **nato-07 NATO pilots fired less in training than Pact pilots.** "NATO air forces get in less actual shooting than do
  the Soviets and the Pact" (USAFE, 1983). NATO's minimum-hours guide was an average of 15 a month (Hansard, 1981).
  [`afmag_1983_usafe`; `hansard_1981_nato_hours`; 1981-83]
- **nato-08 Distances are short.** About 15 minutes of flying time separated the opposing forward elements. NATO had 4
  E-3As on station in April 1983, with 18 planned to be operational by 1985 (`afmag_1983_usafe` gives the plan;
  `afmag_1984_nato`, registered, gives the force of 18). NATO in the Central Region was outnumbered about 6 to 1 by the Pact's air forces, in a
  veteran's recollection. [`afmag_1983_usafe`; `rafhs_j20_1999`, Witts; 1983-85]
- **nato-09 JAAT existed in doctrine from 1979.** The first joint air attack team manual was TACP 50-20 / TRADOC TT
  17-50-3, 30 April 1979. Attack helicopters, A-10s and artillery worked as one team. [`tradoc_jaat_1983`, snippet;
  1979-83]
- **nato-10 Israel 1982 is the period's model of a superior air force against SAMs.** RPV decoys went first. An E-2C
  watched 200 miles; a Boeing 707 collected ELINT; jammers cut Syrian voice and datalink to GCI. 17 of 19 SA-6
  batteries were destroyed in a strike that "reportedly took only 10 minutes" (Lambeth, from press and IDF accounts). Lambeth's lesson was the IAF's initiative, leadership, organisation and
  adaptability, more than its equipment. [`lambeth_1984_r3000`; 9-10 June 1982]
- **nato-11 A superior air force still makes rushed retasking errors.** On 7 October 1973 the IDF General Staff stopped
  Tagar against Egypt after its first wave and ordered Dugman 5 against the Syrian SAMs within hours, with little time to
  update targeting. Six F-4s were lost and one SA-3 battery destroyed. Re-planning a large SEAD strike in hours on stale
  intelligence fails. [`wiki_model5` and the Hebrew encyclopedia entry, pointers only; 1973] **(unverified: carried only
  by Wikipedia and an encyclopedia entry, which may not be the cited source; the numbers need an IAF history or a period
  study before they become data.)**
- **nato-12 Vietnam taught both sides' tactics that NATO 1985 inherited.**
  - US flights jettisoned bombs under MiG attack: 21 aircraft on 21 January 1967, 47 in September 1967, 16 F-105s on
    2 October 1967.
  - Operation Bolo (48 F-4Cs posing as F-105s, with six Iron Hand flights) shot down 7 MiGs with no US loss to MiGs
    (2 January 1967).
  - Iron Hand worked a corridor; Wild Weasels searched.

  [`checo_1969_tactics`; 1967-69]
- **nato-13 Awareness was the deciding factor in air combat.** In about 50% of cases US fighter pilots were not aware of
  the enemy aircraft until it closed to within two miles. Radio chatter blocked warnings. [`redbaron_1967_wseg116`; 1967]
  **(unverified: the checker could not fetch Red Baron; its accession and URL are still to confirm.)**

### Warsaw Pact (peer)

- **pact-01 The plan is fixed before launch.** The air-ground coordination procedures worked out by the air and ground
  commanders before launch "often inhibit flexibility in mutual cooperation" and prevent changes once missions are under
  way (the FM's words, about air-ground cooperation in close support, judged from Soviet writings). Aircraft follow the original flight plan through friendly air defence
  unless ground control changes it. [`fm_100_2_1_1984`, ch. 12; adversary view; 1984]
- **pact-02 Fast re-allocation from the top: readiness levels.**

  | Level | State | Time to take-off | Held for |
  |---|---|---|---|
  | 1 | serviced and armed; crews briefed, in the aircraft ready to start engines | 3-5 min | 1-2 h |
  | 2 | serviced and armed; crews briefed, near the aircraft | 15 min | 2-4 h |
  | 3 | refuelled, cannon loaded, external stores not loaded; crews known, briefed before take-off | 1-2 h | 2-4 days |

  The FM gives these for fighter-bomber aircraft and crews ("probably also applicable" to other ground-attack aviation),
  not for fighters. Levels 1 and 2 answer on-call and immediate requests. Immediate requests go up the chain, and a division approves
  within its own allocation. Strict centralisation allows rapid re-allocation. [`fm_100_2_1_1984`; adversary view; 1984]
- **pact-03 The attack itself.** Approach at 50-100 m; 3-6 s to identify the target; clear the area within 10 s. Radio
  "reduced to a minimum or prohibited entirely". Re-attacks come from a different direction or out of the sun. The
  forward air controller confirms the target, then the flight leader assigns targets in the flight.
  [`fm_100_2_1_1984`; 1984]
- **pact-04 Free hunt is restricted.** Planners had reservations about "free hunting" until air superiority was won.
  Armed reconnaissance flew as one recce aircraft with 2-4 attack aircraft. Crews on any mission were "expected to
  immediately report observed enemy activity" (the FM does not say "rather than attack it": that reading is a SAM3D
  inference from the restricted free hunt). Processing the data of a reconnaissance mission took 2-8 h. Plans covered the first 1-2 h,
  or up to 24 h in a static situation. [`fm_100_2_1_1984`; 1984]
- **pact-05 Flexibility rises with rank.** In US analysts' reading of Soviet norms, there was little flexibility below
  regiment. [`fm_100_2_1_1984`, "Norms, Initiative, and Flexibility"; 1984]
- **pact-06 Soviet self-criticism: initiative stressed, then curbed.**
  - Johnson's content analysis of *Aviatsiia i kosmonavtika* found that the emphasis on initiative rose from the
    mid-1970s and dropped in 1983-84.
  - Critics wrote of "fettering initiative", over-simplified exercises and pilots who "fly the same patterns over and over
    again". Under GCI pilots flew "mechanically".
  - "Independent search" (free hunt) was given emphasis.
  - Ground attack was planned in great detail, with a range officer clearing each run. When targets changed before
    take-off, the crews recalculated. Johnson found no example of a target change in the air.

  [`johnson_1986_nps_initiative`; 1975-85] **(unverified: the checker could not fetch Johnson 1986; the trend and quotes
  rest on the author's local copy. FM 100-2-1 independently says there was "probably little tactical flexibility" below
  regiment, which supports the direction.)**
- **pact-07 Western readings of the same evidence differ.**
  - *Soviet Military Power 1983* (p. 43) said that pilot independence and initiative were "now stressed".
  - Gen. Lew Allen (1979) called the Soviets "fairly predictable, doctrinaire".
  - Skinner held that most missions flew under positive ground control and were aborted if contact was lost.
  - Armitage and Mason (1983) said the Soviets see centralised control as a source of flexibility.

  [`lambeth_1984_r3000`; 1979-84]
- **pact-08 Dubrov's lessons from Lebanon (Soviet, 1983).** Combat air patrols detached from the escort, airborne control
  platforms, and the observation that radar control grows less reliable the deeper the operation goes. Lambeth expected
  the "obsession with control" to persist, with an Il-76 AWACS used as an airborne GCI. [`lambeth_1984_r3000`, reviewing
  Dubrov, *Aviatsiia i kosmonavtika*, Sept-Oct 1983; 1983]
- **pact-09 Afghanistan: a set sequence, adapted between missions.**
  - The Su-17 sequence:
    - a target-designation group of the most experienced pilots searches for 3-5 min at 800-1000 m and 850-900 km/h;
    - 1-2 min later, a suppression group of 2-6 aircraft dives from 2000-2500 m;
    - 1-2 min after that, the main group attacks from the march.
  - Pilots had to hand their report to the regiment's staff within 5-10 min of landing; it went at once to the air
    force command post.
  - A Su-17M3 was shot down near Kandahar in summer 1983 on its sixth (!) pass; Markovsky blames stereotyped attacks
    («шаблонность атак») and drawn-out work over the target.
  - Corrected 15 Nov 2026: "the template was punished at once" and intervals of 1200-1500 m are from Markovsky's Mi-24
    chapter (helicopters on the way to the target or on patrol), not a change made after the Su-17 loss.
  - An order of 1981 forbade attacks on unidentified targets; near the airfields, Markovsky says, fire was nonetheless
    opened on anything suspicious in the forbidden zone (in the context of the Stinger years).

  [`markovsky_afghan_sky`; 1981-85]
- **pact-10 Afghanistan: on call and failures of control.**
  - A Su-25 pair stood alert at Bagram, and Su-25s flew airborne alert at 3000-3500 m.
  - A close air support request went through the division command post and then the air force command post. The delays
    caused strikes on own troops (`johnson_1986_nps_initiative`; unverified, not re-read).
  - In summer 1984 near Kandahar a Su-17 group leader refused the FAC's help and by mistake bombed his own infantry
    battalion: 4 killed, 9 wounded. Several Su-17 flights were even sent back to their bases for low accuracy and the
    danger of hitting own troops (confirmed in Markovsky's text).

  [`markovsky_afghan_sky`; 1980-85]
- **pact-11 Afghanistan: tempo and attrition.** In 1985 the Su-25 averaged 224 h per aircraft against 84 h for the
  MiG-23 (Markovsky: the gap came from many fighters standing alert, not from idleness). In 1985 there was one forced
  landing from combat damage per 4,605 flying hours (Su-17 fighter-bombers' chapter; 5.5 times better than 1980, when it
  was one per 830 h). [`markovsky_afghan_sky`; 1980-85] Helicopters grew from 15-20 in January 1980 to 250-300 by
  September 1980, and district commanders were given more latitude [`johnson_1986_nps_initiative`] **(unverified: not in
  Markovsky's text; Johnson not re-read).**
- **pact-12 Client air forces: the lower bound.**
  - **Syria, 9-10 June 1982.** About 90 Israeli and 60 Syrian jets were airborne at once. Jammed off their GCI, Syrian
    pilots broke into pairs and singles and flew figure-eights (a Western attaché's account). 23 Syrian fighters were
    downed with no Israeli loss in that engagement, 15 the next day, and 85 by the end of July, half MiG-21 and half
    MiG-23, "without losing a single aircraft to enemy fighter action" (Israel lost aircraft to ground fire). About 40
    fell to F-15s, 44 to F-16s and 1 to an F-4, and 7% were gun kills, all "according to press accounts of remarks by"
    Gen. Creech of TAC; most were AIM-9L kills, with the E-2C vectoring F-15s and F-16s into beam attacks. "According to
    one report" (Roberts), some Syrian pilots ejected the moment they knew they had been spotted. [`lambeth_1984_r3000`
    from press and IDF accounts; 1982]
  - **Iraq, 1980-87.** Highly centralised control slowed response. Only the leader knew the destination. Pilots kept to
    profiles for fear of punishment and did not share mission information. GCI directed even ground attack. Pilots
    "piled on" the easiest targets and used the order not to lose aircraft as an excuse. In the early years, attacks on
    nearer targets sent too few aircraft, which bombed from high altitude at high speed (from 1985 the F1 with AS-30L
    attacked from low altitude at stand-off range); pilots seldom flew in poor visibility. Nearly 90 aircraft were lost
    in the first six weeks and an estimated 300-325 in seven years. The pilot-to-aircraft ratio was "slightly over 1:1",
    and French flight training was "tougher and more thorough" than Soviet.
    [`cia_1987_iraq_af`; adversary view; 1980-87]
- **pact-13 Vietnam's MiGs: the GCI-run ambush.** After the North's GCI below the DMZ was struck in 1968, MiGs venturing
  south of the 19th parallel did so "for the most part under strict radio and radar silence", with high-speed "hit and
  run" tactics, "usually consisting of a single firing pass", then a retreat north (`checo_1969_tactics`, confirmed;
  1968). Timing by GCI to the escort's fuel state is **unverified** (not in CHECO; Red Baron not re-read).
  [`checo_1969_tactics`; `redbaron_1967_wseg116`; 1967-72]
- **pact-14 Readiness at the start: moved to section 5.** The *Air Force Magazine* article of March 1989
  (`afmag_1989_soviet_readiness`) describes late-1980s Soviet readiness and was published after the period; it is not
  used as 1985 data.
- **pact-15 Peacetime flying hours: not found in a period source.**
  - Forum figures of 110-140 h (16th Air Army) and 70-120 h a year are unsourced.
  - A Polish retrospective gives about 40 h in the 1980s for Poland.
  - A CIA 1969 snippet gives 240-250 h as a Soviet minimum, but the OCR is unreliable.
  - A Czech search found nothing.

  [not found; open item]

### Neutral (near peer)

No neutral air force fought in 1980-85. The cases below are small air forces, not neutrals. They are used for the
pattern of a small professional force with few aircraft and no replacements.

- **neutral-01 Austria in 1985 had a small, defensive, ground-controlled force.**
  - The Goldhaube radar system was planned and activated 1970-83.
  - The Saab 105 was inadequate for air combat; the Draken came only in 1988 (post-period).
  - The faction file's light jets and gun-armed interceptors match this.

  [`goldhaube_refs`; `austrian_af_wiki`; snippets; 1970-85]
- **neutral-02 Sweden: a neutral's C2 in the period.** STRIL 60 and the JA 37's datalink allowed a "silent intercept".
  Bas 60 and Bas 90 dispersed the aircraft to road bases. A neutral could control its fighters closely without voice.
  [`swe_stril_c2`; snippets; 1970s-85]
- **neutral-03 Small-force initiative: Crippa, 21 May 1982.** A lone MB-339 was sent to locate and count the landing
  ships at San Carlos and was allowed to attack. Crippa chose to attack HMS Argonaut with 30 mm and rockets, then noted
  the ships' positions on his kneepad. Five of the ten MB-339s were lost in the war. [`ellitoral_crippa`; tier 4; 1982]
- **neutral-04 Argentina's pilots pressed on at extreme range.** They flew about 300 sorties against ships "with
  determination and bravery". Unrefuelled range left them little time to manoeuvre over the target. Britain flew 1,100
  Sea Harrier air defence sorties and 215 Harrier ground attack sorties, with about 90% availability. The Vulcan raids
  could not close the airfield or put the radars out. [`usn_1983_falklands_lessons`; 1982]
- **neutral-05 A small force husbands its aircraft when the threat rises.** Under the arms embargo the SAAF could buy no
  new fighters after the F1. Its F1CZ operations fell once Cuban MiG-23MLs appeared in 1987, and it lost aircraft on
  27 September 1987, 20 February 1988 and 19 March 1988. [`angola_saaf_1987`; snippets; 1987-88, after the war's year
  but within the decade]
- **neutral-06 Even a capable air force suffers when it meets a surprise.** On 30 July 1970 (Rimon 20) Israel ambushed
  Soviet-flown MiG-21s and downed 5 for one Mirage damaged. A defender that does not know its enemy's quality is easily
  lured. [`rimon20_refs`; snippets; 1970]

### Rebels (inferior)

- **rebels-01 Improvised air arms raid by surprise, low and at night.** Biafra's MiniCOIN (five MFI-9Bs with SNEB rockets)
  hit Port Harcourt (22 May 1969), Benin (24 May) and Enugu (27 May) at tree-top height. [`biafra_minicoin`; tier 4
  snippets; 1969]
- **rebels-02 One aircraft can matter.** In 1961 Katanga's few Fouga Magisters, and at Jadotville a single Fouga,
  harassed the UN until it brought in Canberras. [`katanga_fouga`; tier 4 snippets; 1961]
- **rebels-03 Croatia 1991 is the faction file's model, but it is after the period.** The improvised crop sprayers and
  defected jets of `croatia_1991_migflug` are flagged hindsight (section 5). The pattern also appears in the period cases
  above (Biafra, Katanga). [`croatia_1991_migflug`; 1991, flagged]
- **rebels-04 Rebels attack targets of opportunity.** With no net and no plan beyond the take-off, a rebel pilot attacks
  what he finds. The pattern is the same as Crippa's choice and Biafra's raids. [SAM3D inference from `ellitoral_crippa`
  and `biafra_minicoin`]

### Our side (the defender, Pact-style)

- **our-01 The enemy we meet in 1985 is low.** NATO's attack aircraft come in at 30-75 m (100-250 ft) by day and are
  fastest and lowest where the defence is densest. [`rafhs_j20_1999`; `mt_1984_babich`; 1977-85]
- **our-02 Our fighters depend on GCI.** If the warning net or GCI is jammed or out, our fighters behave as Syria's did
  in 1982 (pairs and singles, little effect), or hold off. One number per front should fall when the enemy jams our C2.
  [`lambeth_1984_r3000`; 1982]
- **our-03 What our crews will see when attackers break.** Bombs jettisoned and aircraft turning away (Vietnam 1967);
  pilots ejecting from undamaged aircraft (Syria 1982); a lone attacker making a second pass (Crippa). These are the sights
  that should reach the battery's records. [`checo_1969_tactics`; `lambeth_1984_r3000`; `ellitoral_crippa`]
- **our-04 Our own air support is slow and can hit us.** In Afghanistan a CAS request went through division and then the
  air force command post, with delays and strikes on own troops. Plan our side's air support with long latencies.
  [`johnson_1986_nps_initiative`; `markovsky_afghan_sky`; 1980-85]

## 3. Numbers usable as data

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| Readiness level 1: crews in aircraft | 3-5 min to take-off, held 1-2 h | Pact fighter-bombers | 1984 | `fm_100_2_1_1984` | medium (adversary view) |
| Readiness level 2: crews near aircraft | 15 min, held 2-4 h | Pact fighter-bombers | 1984 | `fm_100_2_1_1984` | medium |
| Readiness level 3: stores not loaded | 1-2 h, held 2-4 days | Pact fighter-bombers | 1984 | `fm_100_2_1_1984` | medium |
| Target identification time | 3-6 s; clear the area within 10 s | Pact | 1984 | `fm_100_2_1_1984` | medium |
| Attack approach height | 50-100 m | Pact | 1984 | `fm_100_2_1_1984` | medium |
| Armed reconnaissance group | 1 recce + 2-4 attack aircraft | Pact | 1984 | `fm_100_2_1_1984` | medium |
| Reconnaissance processing | 2-8 h | Pact | 1984 | `fm_100_2_1_1984` | medium |
| Detailed plan horizon | 1-2 h (up to 24 h static) | Pact | 1984 | `fm_100_2_1_1984` | medium |
| Exercise safety distance from own troops | 200-700 m | Pact | 1984 | `fm_100_2_1_1984` | medium |
| Target-designation group search | 800-1000 m, 850-900 km/h, 3-5 min | Pact (Afghanistan) | 1980-85 | `markovsky_afghan_sky` | medium |
| Suppression group | 2-6 aircraft, dive from 2000-2500 m, 1-2 min after designation | Pact (Afghanistan) | 1980-85 | `markovsky_afghan_sky` | medium |
| Main group | 1-2 min after suppression | Pact (Afghanistan) | 1980-85 | `markovsky_afghan_sky` | medium |
| Post-mission report | within 5-10 min of landing | Pact (Afghanistan) | 1980-85 | `markovsky_afghan_sky` | medium |
| Interval between Mi-24s on the way to the target or on patrol (corrected: helicopters, not Su-17s after 1983) | 1200-1500 m | Pact (Afghanistan) | 1980s | `markovsky_afghan_sky` | medium |
| Su-25 airborne alert height | 3000-3500 m | Pact (Afghanistan) | 1980s | `markovsky_afghan_sky` | medium |
| Flying hours per aircraft | Su-25 224 h, MiG-23 84 h | Pact (Afghanistan) | 1985 | `markovsky_afghan_sky` | medium |
| Combat-damage forced landings | 1 per 4,605 flying hours | Pact (Afghanistan) | 1985 | `markovsky_afghan_sky` | low |
| Helicopters in Afghanistan | 15-20 (Jan 1980) to 250-300 (Sep 1980) | Pact | 1980 | `johnson_1986_nps_initiative` | low (unverified, not re-read) |
| SA-6 batteries destroyed | 17 of 19 in a strike that "reportedly" took 10 min | NATO-like (Israel) vs Pact client | 1982 | `lambeth_1984_r3000` (from press and IDF accounts) | medium |
| Aircraft airborne at once | about 90 Israeli, 60 Syrian | Israel vs Syria | 1982 | `lambeth_1984_r3000` | medium |
| Syrian aircraft downed | 23 day 1, 15 day 2, 85 by end July; 0 Israeli to enemy fighters (losses to ground fire not counted) | Israel vs Syria | 1982 | `lambeth_1984_r3000` | high |
| Gun kill share | 7% (press account of Gen. Creech's remarks) | Israel | 1982 | `lambeth_1984_r3000` | medium |
| Iraqi losses | nearly 90 in the first six weeks; 300-325 in seven years | Pact client | 1980-87 | `cia_1987_iraq_af` | medium (adversary estimate) |
| Iraqi pilots per aircraft | "slightly over 1:1" | Pact client | 1987 | `cia_1987_iraq_af` | medium (adversary estimate) |
| NATO minimum flying hours (guide) | average 15 h/month | NATO | 1981 | `hansard_1981_nato_hours` | high |
| RAF Tornado crews per aircraft | about 1.3 | NATO | late 1980s (veteran's recollection) | `rafhs_j20_1999` | medium |
| Low-level training height | Tornado GR1: 250 ft daily, 100 ft at Goose Bay (late 1980s); Buccaneer work-up to 100 ft for Red Flag (1977) | NATO | 1977-90 | `rafhs_j20_1999` | high |
| Red Flag package | 60-70 aircraft in 20-30 min, no radio calls | NATO | late 1970s-80s | `rafhs_j20_1999` | medium |
| TAC aircrew with combat time | under 25% | NATO (USAF) | Aug 1978 | `vangilder_1979_cgsc` | high |
| Air combat exchange ratios | Rolling Thunder 2.3:1; USN 12.5:1 from 1971; Korea about 10:1 | US | 1950-73 | `vangilder_1979_cgsc` | medium |
| TLP | 8 courses a year, 4 weeks each (1 week seminars, 3 mostly flying), 15 sorties per crew | NATO | 1983 | `afmag_1983_usafe` | high |
| Flying time between opposing forward elements | about 15 min | NATO/Pact | 1983 | `afmag_1983_usafe` | medium |
| NATO E-3A | 4 on station April 1983; 18 planned operational by 1985 | NATO | 1983-85 | `afmag_1983_usafe`; `afmag_1984_nato` | high |
| Survival by height (US survival corridor) | 60-90 m: over 75%; 30-60 m and 90-200 m: 50-75%; under 30 m or over 200 m: under 50% | NATO, read by the Pact | 1984 | `mt_1984_babich` | medium |
| Speed and vulnerability | 370 to 740 km/h cuts vulnerability fourfold | general | 1984 | `mt_1984_babich` | medium |
| Unaware of the attacker until | within 2 miles, in about 50% of cases | US vs Vietnam | 1967 | `redbaron_1967_wseg116` | low (unverified, not re-read) |
| Jettisons under MiG attack | 21 aircraft (21 Jan 1967); 47 (Sep 1967); 16 F-105s (2 Oct 1967) | US vs Vietnam | 1967 | `checo_1969_tactics` | high |
| Operation Bolo | 48 F-4Cs, 7 MiGs downed | US vs Vietnam | 1967 | `checo_1969_tactics` | high |
| Argentine sorties against ships | about 300 | small air force | 1982 | `usn_1983_falklands_lessons` | medium |
| British Harrier sorties | 1,100 Sea Harrier air defence; 215 Harrier ground attack; about 90% availability | UK | 1982 | `usn_1983_falklands_lessons` | medium |
| MB-339 losses | 5 of 10 | small air force | 1982 | `ellitoral_crippa` | low |
| Rimon 20 | 5 MiG-21s downed, 1 Mirage damaged | Israel vs USSR | 1970 | `rimon20_refs` | medium |
| Biafra MiniCOIN | 5 MFI-9B | rebels | 1969 | `biafra_minicoin` | medium |
| Dugman 5 | 6 F-4s lost, 1 SA-3 battery destroyed | Israel | 1973 | `wiki_model5` (Wikipedia, pointer only) | low (unverified: no citable source; not for data) |

## 4. Disagreements and which to use

1. **Soviet pilot initiative.**
   - *Soviet Military Power 1983* said pilot independence and initiative were "now stressed".
   - Johnson's content analysis shows the emphasis falling back in 1983-84, and Soviet critics wrote of stereotyped
     training.
   - **Use:** low initiative in the air for 1985 (0.25), with the free hunt as an authorised exception on armed-recce
     tasks.
2. **Abort on losing GCI.**
   - Skinner (Western) says missions under positive ground control were aborted if contact was lost.
   - FM 100-2-1 says strike aircraft keep to the original flight plan unless ground control changes it.
   - Soviet writers emphasised "independent search".
   - **Use:** split by mission. Fighters under GCI hold or return when it is lost (turn-back 0.6). Strike groups go on to
     their planned target (turn-back 0.2).
3. **Centralisation: flexible or rigid?**
   - Armitage and Mason, and FM 100-2-1, say centralisation allows rapid re-allocation.
   - The same FM, Johnson and the CIA on Iraq say it prevents changes once missions are under way.
   - **Use:** two latencies. The Pact is fast at launching a new task from alert (3-5 min) and slow or unable to retask an
     airborne flight.
4. **Soviet peacetime flying hours.**
   - No period source. Forum figures of 70-140 h are unsourced; a Polish retrospective gives about 40 h; an unreliable
     CIA OCR gives 240-250 h.
   - **Use:** none as data. Leave currency as a SAM3D estimate (Pact below NATO's 180 h a year) and record it as open.
5. **Bekaa kill tally.** Counts of Syrian losses vary between sources. **Use** Lambeth's: 23, then 15, then 85 by the
   end of July.
6. **Who retasked on 7 October 1973.**
   - It is commonly told as an air force decision.
   - The Hebrew encyclopedia entry says the General Staff stopped Tagar and ordered Dugman 5, with the IAF commander
     recommending.
   - **Use:** the General Staff. The lesson for SAM3D, rushed retasking of a SEAD strike on stale intelligence, holds
     either way.
7. **NATO attack height.**
   - The current NATO commander has `medium_altitude_man`.
   - Period sources show low level in the Central Region (`rafhs_j20_1999`, `mt_1984_babich`). Medium altitude was a
     period option only over broken defences (Vietnam 1972, Bekaa 1982).
   - **Use:** low level by default in 1985. Allow medium altitude after the defence falls below the phase threshold.
     Change the officer's quirk.

## 5. Post-period items excluded, with dates

- The Gulf War switch to medium altitude after early Tornado low-level losses: 1991. The hindsight in `rafhs_j20_1999`
  is not used.
- The rollback-then-interdiction campaign and its percentage gates, kill boxes and killer scouts: 1991.
- Medium-altitude floors as policy (Kosovo, 15,000 ft): 1999.
- LANTIRN night low-level navigation and targeting pods: in service 1987; awaited in 1983 (`afmag_1983_usafe`).
- NVG fast-jet strikes: 1990s.
- The Stinger in Afghanistan, from late 1986, which drove Soviet strikes to high altitude (`markovsky_afghan_sky`).
- TLP at Florennes: 1989.
- The Austrian Draken: 1988.
- Croatia's improvised air arm: 1991 (`croatia_1991_migflug`; the faction file's model, kept only as a pattern).
- AMRAAM: 1991. The R-77: 1990s. JTIDS/Link 16 in fighters: 1990s. Later brevity words.
- The A-50 AWACS: entered Soviet service in the mid-1980s, formally later. Uncertain; not used for 1985.
- SAAF and Angola, 1987-88: within the decade but after the war's year. Used for the pattern only.
- Soviet readiness as *Air Force Magazine* described it in March 1989 (`afmag_1989_soviet_readiness`: 95% of fighters
  ready on day one, much simulator training, first combat sorties about 8 h after dispersal): late 1980s, published
  after the period; moved here from pact-14 and the data table (15 Nov 2026).
- Computer-aided in-flight retasking of interdiction aircraft (`schoeck_1987_ai_retasking`, AFIT 1987): a study of
  1987, not 1985 practice.
- A 1985 role for NATO's E-3A in retasking strike aircraft in the air: not shown in any period source checked; the E-3A
  in 1983-85 is described as surveillance and a force multiplier. Treat AWACS strike retasking as later practice until a
  period source shows it.

## 6. Sources

| Id | Tier | Full citation | URL | Read | Whose view |
|---|---|---|---|---|---|
| `fm_100_2_1_1984` | 1 (registered) | US Army FM 100-2-1, *The Soviet Army: Operations and Tactics*, 16 July 1984 | https://archive.org/details/fm100-2-1 | ch. 12 and section 2 in full | US view of the adversary |
| `lambeth_1984_r3000` | 1 (new; a US Air Force-sponsored RAND study: the adversary's view of Soviet and Syrian practice; its Bekaa figures come from press and IDF accounts) | B. S. Lambeth, *Moscow's Lessons from the 1982 Lebanon Air War*, RAND R-3000-AF, September 1984 | https://www.rand.org/content/dam/rand/pubs/reports/2007/R3000.pdf | full (checker OCR'd the scan, 15 Nov 2026) | US view; reviews Dubrov (Soviet, 1983) |
| `johnson_1986_nps_initiative` | 2 (new) | J. S. Johnson, *Initiative in Soviet Air Force Tactics and Decision Making*, MA thesis, Naval Postgraduate School, Monterey, June 1986 | local copy (DTIC); URL to confirm | key parts | US analysis of Soviet sources |
| `cia_1987_iraq_af` | 1 (new in parallel notes) | CIA, *Iraq's Air Force: Improving Capabilities, Ineffective Strategy*, NESA 87-10046C, October 1987 | https://www.cia.gov/readingroom/document/cia-rdp88t00096r000700910001-5 (text also at archive.org, item cia-readingroom-document-cia-rdp88t00096r000700910001-5) | full | CIA view (adversary's estimate of a Soviet-trained client) |
| `vangilder_1979_cgsc` | 2 (new) | Van Gilder, *Realistic Training: The Key to Success in Aerial Combat*, MMAS thesis, US Army CGSC, 1979, ADA076937 | https://apps.dtic.mil/sti/citations/ADA076937 | relevant parts | US, own side |
| `redbaron_1967_wseg116` | 1 (new) | IDA/WSEG, *Air-to-Air Encounters in Southeast Asia* (Red Baron), WSEG Report 116, vol. I, October 1967, declassified | local copy (DTIC); URL to confirm | relevant parts | US, own side |
| `checo_1969_tactics` | 1 (registered) | Project CHECO, tactics report, 1969, ADA486448 | https://archive.org/details/DTIC_ADA486448 | relevant parts | US, own side |
| `mt_1984_babich` | 1 (registered) | V. Babich, article in *Military Thought*, 1984 | as registered | relevant parts | Soviet, reading the US |
| `rafhs_j20_1999` | 2 (new in parallel notes) | *RAF Historical Society Journal* 20, seminar "Training in Peace for War", 1999 | https://www.rafmuseum.org.uk/documents/Research/RAF-Historical-Society-Journals/Journal-20-Seminar-Training-in-Peace-for-War.pdf | relevant parts | RAF veterans, retrospective |
| `afmag_1983_usafe` | 4 (new) | *Air Force Magazine*, "USAFE: The Dangerous Decade", April 1983 | Air & Space Forces magazine archive; local copy | full | US, own side (press) |
| `afmag_1989_soviet_readiness` | 4 (new) | *Air Force Magazine*, "Readiness, Soviet Style", March 1989 | Air & Space Forces magazine archive | full | US view of the adversary |
| `usn_1983_falklands_lessons` | 1 (new) | US Navy Office of Program Appraisal, *Lessons of the Falklands*, February 1983 | local copy; URL to confirm | relevant parts | US, neutral observer |
| `hansard_1981_nato_hours` | 1 (new) | Hansard, HL Deb 18 February 1981, vol 417 cc679-80 (Viscount Trenchard) | https://api.parliament.uk/historic-hansard/ (1981 Lords) | full (the answer) | UK government |
| `markovsky_afghan_sky` | 4 (new in parallel notes) | В. Марковский, «Жаркое небо Афганистана», 2000 | https://lib.ru/MEMUARY/AFGAN/nebo.txt_with-big-pictures.html | in part | Russian historian, from veterans |
| `schoeck_1987_ai_retasking` | 2 (new in parallel notes) | J. A. Schoeck, AFIT/GST/ENS/87M-15, June 1987, ADA185234 | https://archive.org/details/DTIC_ADA185234 | cited from parallel note | US, own system |
| `tradoc_jaat_1983` | 1 (new in parallel notes) | TACP 50-20 / TRADOC TT 17-50-3, JAAT, 30 April 1979, and later editions | as in parallel note | snippet | US, own side |
| `jsead_1982` | 1 (registered) | JSEAD concept, 1982 | as registered | not reread | US |
| `wiki_model5` | 4 as registered, but Wikipedia: a pointer only, never the cited source | Wikipedia, Operation Model 5 (Dugman 5); with the Hebrew encyclopedia entry (hamichlol.org.il) | https://en.wikipedia.org/wiki/Operation_Model_5 | snippet | pointer only; nato-11 unverified until its sources are read |
| `croatia_1991_migflug` | 4 (registered) | migflug.com on Croatia's air arm, 1991 | as registered | not reread | flagged post-period |
| `austrian_af_wiki` | pointer (registered) | Austrian Air Force pages | as registered | not reread | pointer |
| `tlp_history` | 4 (new) | TLP history pages (scramble.nl, milavia, reccereports) | various | snippets | press and enthusiasts; superseded by `tlp_official_history` |
| `tlp_official_history` | 2 (new; the programme's own history page, retrospective) | Tactical Leadership Programme, "History" | https://www.tlp-info.org/history/ | the founding paragraphs (15 Nov 2026) | NATO programme, own side |
| `afmag_1984_nato` | 4 (registered) | «NATO on the Upbeat», *Air Force Magazine*, September 1984 | https://www.airandspaceforces.com/article/0984nato/ | not reread here | US press |
| `nellis_red_flag` | 1 (new) | Nellis AFB, Red Flag fact sheet and 40-year feature | nellis.af.mil | snippets | US, own side |
| `cia_1984_lebanon_lessons` | 1 (new) | CIA NI IIM 84-10008, *Military Lessons Learned by Israel and Syria From the War in Lebanon*, 1 May 1984 | CIA reading room (fetch refused) | snippet only | CIA view |
| `ellitoral_crippa` | 4 (new) | El Litoral (D. Pignata) on Crippa's sortie of 21 May 1982 | ellitoral.com | full article | Argentine press |
| `biafra_minicoin` | 4 (new) | *Time* (1969) and warhistory pages on Biafra's MFI-9Bs | various | snippets | press |
| `katanga_fouga` | 4 (new) | Katanga air arm pages, 1961 | various | snippets | press and enthusiasts |
| `rimon20_refs` | 4 (new) | Pages on Operation Rimon 20, 30 July 1970 | various | snippets | press |
| `goldhaube_refs` | 4 (new) | Pages on Austria's Goldhaube radar system | various | snippets | Austrian reference |
| `swe_stril_c2` | 4 (new) | Pages on Swedish STRIL 60, the JA 37 datalink, Bas 60 and Bas 90 | various | snippets | Swedish reference |
| `angola_saaf_1987` | 4 (new) | Pages on SAAF operations and losses in Angola, 1987-88 | various | snippets | South African reference |

## 7. Open items and documents to acquire

- **Soviet and Pact peacetime flying hours, 1980-85, from a period or operator source.** Try a CIA SOV paper on frontal
  aviation training, Czechoslovak or Polish regimental histories, or *Vojenské rozhledy* 1984-88.
- **CIA NI IIM 84-10008 (1984)** in full: the reading room refused automated fetch. Ask the project owner to download it.
- **Dubrov, «Авиация в ливанском конфликте»**, *Авиация и космонавтика*, 1983, nos. 9-10: the Soviet text itself.
- **TEABALL (1972)**: the GCI-for-our-fighters case. The PDF was blocked; cite it only from snippets until it is read.
- **Johnson 1986 and Red Baron**: confirm their DTIC accession numbers and URLs.
- **The Argentine decision chain for Bluff Cove (8 June 1982)**: how a sighting became a strike the same day. Not found;
  try the Fuerza Aérea Argentina's official history.
- **MiG-29 and Su-27 service dates in the data**: check the `in_service_year` values against Soviet frontal aviation
  entry (MiG-29 units from about 1983-84; Lambeth 1984 expects both).
- **NATO 1985 SEAD-first or simultaneous**: find a period USAFE or 2 ATAF document saying whether day-1 interdiction
  waited for SEAD.
- **Neutral air forces' rules of engagement in the period**: Austrian or Swiss doctrine on loss aversion and border
  limits.
- **Johnson 1986 and Red Baron** (added by the checker): neither could be fetched on 15 Nov 2026; pact-06, the CAS
  delay in pact-10, the helicopter numbers in pact-11 and nato-13 stay unverified until they are read.

## Verification (15 Nov 2026)

An adversarial check against the period rule (1985) and the sourcing rules. Texts read by the checker: FM 100-2-1
(archive.org full text), Lambeth R-3000 (RAND scan, OCR'd), CIA NESA 87-10046C (archive.org text), Markovsky
(lib.ru text), CHECO 1969 (archive.org text), Babich 1984 (CIA translation, archive.org text), Van Gilder 1979
(archive.org text), RAF HS Journal 20 (RAF Museum PDF), *Air Force Magazine* April 1983 (airandspaceforces.com),
Hansard HL 18 Feb 1981 (historic Hansard), TLP history (tlp-info.org). Not fetched: Johnson 1986, Red Baron,
the Dugman 5 sources. The web search budget ran out during the check, so no new sources were searched for.

| Finding | Verdict | Note |
|---|---|---|
| pact-01 plan fixed before launch | confirmed | FM 100-2-1 ch. 12 (pp. 12-3 to 12-4): "often inhibit flexibility", "follow the original flight plan ... unless changed by ground control"; wording now says it is about air-ground coordination. Adversary view. |
| pact-02 readiness levels | corrected | Numbers right (3-5 min/1-2 h, 15 min/2-4 h, 1-2 h/2-4 days); level 2 crews are near, not in, the aircraft; the chart is for fighter-bombers, not fighters. |
| pact-03 the attack | confirmed | 3-6 s, 10 s, 50-100 m, radio "reduced to a minimum or prohibited entirely", other direction or bright sunlight, FAC confirms then leader assigns: all in FM ch. 12 (pp. 12-4 and 12-7 to 12-8). |
| pact-04 free hunt, armed recce | corrected | Reservations about free hunt, 1 + 2-4, 2-8 h, 1-2 h (24 h) confirmed; "report, not attack" is an inference, not the FM's words. |
| pact-05/06 Johnson's trend | unsupported | Johnson 1986 not fetched; FM's "little tactical flexibility" below regiment supports the direction only. |
| pact-07/08 Western readings, Lambeth | confirmed | SMP 1983 p. 43 "now stressed", Skinner p. 122 abort on lost contact, Armitage and Mason p. 176, Lew Allen, Il-76 as airborne GCI and "obsession with control": all in R-3000. |
| pact-09 Afghan Su-17 sequence | confirmed | 800-1000 m, 850-900 km/h, 3-5 min; 2-6 Su-17 from 2000-2500 m after 1-2 min; report within 5-10 min of landing: Markovsky's text. |
| pact-09 adaptation after the 1983 loss | corrected | Sixth-pass Su-17M3 loss (summer 1983) and the 1981 order confirmed; the 1200-1500 m interval and "template punished at once" are the Mi-24 chapter's, not a change after that loss. |
| pact-10 own-troops strike 1984 | confirmed | Kandahar, summer 1984, leader refused the FAC, 4 killed and 9 wounded, flights sent back for low accuracy; the CAS-chain delay (Johnson) unverified. |
| pact-11 Afghan tempo | corrected | 224 h vs 84 h (1985) and 1 per 4,605 h (1985) confirmed in Markovsky; the helicopter numbers are not in Markovsky and Johnson was not re-read. |
| pact-12 Syria 1982 | corrected | 90/60 airborne, 23/15/85, 40/44/1, 7%, pairs and singles, figure-eights, ejections confirmed in R-3000, but "no Israeli loss" is to enemy fighters only, and the kill split and 7% are press accounts of Gen. Creech. |
| pact-12 Iraq 1980-87 | corrected | CIA text confirms leader-only target, punishment, GCI for ground attack, "pile on", nearly 90 in six weeks, 300-325 in seven years, pilot ratio "slightly over 1:1"; high-altitude bombing is the early years (from 1985 the F1 with AS-30L attacked low). Adversary estimate. |
| pact-13 Vietnam MiGs | corrected | CHECO confirms radio and radar silence, hit-and-run, single pass, after the GCI was struck in 1968 (MiGs going south); timing to the escort's fuel is not in CHECO and is unverified. |
| pact-14 Soviet readiness 1989 | post_period | Published March 1989 about the late 1980s: moved to section 5 and out of the data table. |
| nato-01 low level | confirmed | RAF HS J20: 250 ft daily and 100 ft at Goose Bay (Tornado, described "by the late '80s"), Buccaneers to 100 ft for Red Flag 1977. Retrospective, tier 2. |
| nato-02 Babich survival corridor | confirmed | CIA translation of *Voennaya mysl'* No. 5, 1984: 60-90 m over 75%, 30-60 and 90-200 m 50-75%, below 30 or above 200 m under 50%; 370 to 740 km/h cut vulnerability four times. Read from US Vietnam-era experience. |
| nato-03 Red Flag | confirmed | Van Gilder 1979: Red Flag from November 1975, first ten sorties, under 25% of TAC aircrew with combat time in August 1978; RAF HS J20 agrees on 1975. |
| nato-04 Red Flag packages | confirmed | RAF HS J20 (Pitchfork): FRAG the afternoon before, 60-70 aircraft through corridors in 20-30 min, "No radio calls were required". |
| nato-05 TLP | corrected | AFM 1983: 8 four-week courses a year, 15 sorties per crew, every USAFE fighter wing in 1982. TLP's own history: six nations (BE, CA, DE, NL, UK, US), January 1978 at Fürstenfeldbruck as seminars (not an academic course), Jever September 1979. |
| nato-06 in-flight retasking | corrected | AFM 1983: the ATOC feeds the next target to the receiving base after a diversion, not in the air; Schoeck 1987 is after the period and does not show 1985 practice. Table row and summary fixed. |
| nato-07 NATO 15 h a month | confirmed | Hansard HL 18 Feb 1981 col. 679 (Viscount Trenchard): "the NATO guide on minimum hours is an average 15 hours a month"; AFM 1983 "less actual shooting". |
| nato-08 15 min; E-3A | corrected | 15 min confirmed; AFM 1983 has 4 E-3As on station and 18 *planned* by 1985 (the registered `afmag_1984_nato` has the 18). |
| nato-09 Bekaa SAM strike | confirmed | R-3000: 17 of 19 SA-6, "reportedly took only 10 minutes", E-2C at 200 miles, 707 ELINT, RPV decoys, jamming of voice and data link; Lambeth stresses leadership, organisation, adroitness and adaptability. |
| nato-10/11 Dugman 5 | unsupported | Only Wikipedia and an encyclopedia entry: never the cited source; numbers kept as a pointer, not as data. |
| nato-11/12 Vietnam jettisons, Bolo | confirmed | CHECO 1969: 21 aircraft on 21 January 1967, 47 in September 1967, 16 F-105s on 2 October 1967; Bolo's 48 F-4Cs downed 7 MiGs with no losses to MiGs. |
| nato-12/13 Red Baron awareness | unsupported | Not fetched; accession and URL still to confirm. |
| our-01 GCI dependence | confirmed | R-3000: once jammed, Syrian pilots "lost any semblance of air discipline" and split into pairs and singles. |
| design-01 faction table | confirmed | Stated as SAM3D estimates; corrected the NATO retask cell; the NATO in-flight retask latency (3-10 min) rests only on the FAC route for 1985. |
| scan: post-period items | corrected | No AMRAAM, Link 16, LANTIRN, NVG or killer scouts presented as 1985; added the 1989 readiness article, Schoeck 1987 and AWACS strike retasking to section 5; Wild Weasels dated from late 1965, not 1966; "NATO plans interdiction on day 1" marked as an inference. |
