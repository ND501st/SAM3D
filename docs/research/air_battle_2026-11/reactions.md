# In-flight threat reactions, 1965-1985: how aircrews saw and answered SAMs, flak and fighters

Research note for SAM3D's 1985 air war (`simcore::airwar`, track R4: pilots who break, dive and jettison).
Written 15 Nov 2026. Period rule: only what was in service or in doctrine by 1985. Later items are listed in
section 5. 1991 and later appear only as flagged hindsight.

This note extends `docs/research/AAA_DAMAGE_2026-11.md` (sections 5-7: SAM-forced jettisoning, the 47 aircraft of
September 1967, the 16 F-105s of 2 October 1967, the `mt_1973_nvn` dive and turn, the 30-50% avoidance, Babich's turn to
the zone boundary, the jink rule, minimum heights and the estimated jettison probabilities). Those are not repeated
here as new findings. What this note adds: how the threat was first noticed, by eye or by receiver, and how late;
what each receiver could tell the crew, by date; the calls in use; the manoeuvre by threat, with the timing that
decided it; chaff and flares as carried before 1985; mutual support; and counts of jettison, press-on and abort per
attacked flight.

The web search budget for this task was used up before the end (200 searches). Some leads are therefore open items.

---

## 1. Summary for SAM3D's design

1. **The eye came first.** In every period source read, most threats were first seen, not received. In 16 MiG
   attacks on F-105 flights in 1965-67, 13 were first detected by eye with no prior warning (`red_baron_vol2`).
   SA-2 launches were seen by the booster's dust cloud and the climbing missile, and USAF crews in 1972 judged the
   eye more reliable than the receivers, which were swamped (`nalty_1977`). Wild Weasels kept 8,000 ft above a cloud
   deck so they could see the missiles. Below cloud, at night or with no one looking, the SAM came unseen.
2. **Receivers in 1985 told the crew less than a modern one.** The period RWR showed the type of radar, a rough
   range or power and a bearing, and lit a launch light when the guidance signal appeared (`gao_masad_82_23`,
   `checo_1967_tactics` p. 19, `nalty_1977`). The fielded ALR-69 had known faults in bearing, ambiguity, false alarms and
   detection range (GAO 1982). No source read shows a missile approach warner on fast jets by 1985. A receiver could not
   warn of an IR missile (`babich_1988`). SAM3D's NATO RWR model should therefore give type and rough bearing, a launch
   cue only for command- or radar-guided SAMs whose guidance link it knows, false alarms, and nothing at all for
   an IR shot or a radar it does not know.
3. **Timing decided survival.** Hits fell mostly on aircraft that "had taken no evasive action or had only initiated such
   action" (`checo_1967_tactics` p. 30). The SA-2 doctrine was to wait until the missile could no longer follow, then
   pull hard (`werrell_2005` p. 121, `checo_1967_tactics` p. 31). Soviet writers say the same from the other side: the
   manoeuvre's effect depended on how accurately its start was timed (`mt_1984_babich`). Reaction delay and its spread
   by skill are therefore the main lever, more than the manoeuvre's shape.
4. **Against low-level SAMs of the Osa's class**, the 1973, 1982 and Falklands evidence shows: low and fast run-ins;
   chaff (often improvised in the airbrake well before proper dispensers) and a break on a tracking call; leaving the
   zone low behind terrain; turning toward the missile to show it a "cold side"; steep dives onto SA-6 sites
   (`werrell_2005` p. 153, `pook_1985`). Rolands fired at retreating Harriers all missed (`pook_1985`). This is the
   nearest analogue to an Osa engagement in the sources.
5. **The Pact's own doctrine was a preplanned programme.** The Soviet Military Encyclopedia's
   «противоракетный манёвр» is a change of course, speed and height together, planned before the flight from the known
   sites, flown "at once by a preset programme" under sudden fire, to the zone's nearest boundary, at full speed and
   with jamming (`sve_prm`). Soviet aircraft had the SPO-10 receiver; flare dispensers on the Su-25 in Afghanistan were
   rarely used before about 1985 because of workload and a one-minute supply (`markovsky_afghan`).
6. **Jettison and abort are common and are a win for the defence.** In the F-105 MiG events, about half the attacked
   flights jettisoned something (tanks more often than bombs), about a third pressed on and a fifth to a third
   aborted (`red_baron_vol2`). One MiG attempt on 21 January 1967 caused 21 aircraft to jettison (`checo_1969_tactics`
   p. 4). "MiGs would usually depart anytime you jettisoned" (a pilot's comment in `red_baron_vol2`). This supports
   the project rule that the battery's ledger counts freedom of action denied, not just kills.
7. **Calls were plain language.** The period calls attested are plain words with clock codes and callsigns ("MIGs
   behind you, break right", "MIGs at 12 o'clock"), code words for warnings that were changed daily, and radio
   congestion that drowned warnings (`red_baron_vol2`, `nalty_1977`). The multi-service brevity list (FM 90-38) is from
   1997. SAM3D should not use post-1990 brevity words ("Magnum", "Spike", "Mud", "Naked", "Defending", "Notch") for 1985
   crews. See section 5.
8. **Skill shows in the records.** Flights that did not react to warnings were hit (`red_baron_vol2` II-1). One pilot
   rolled inverted and was hit; another flew straight on (`checo_1967_tactics` p. 30). Syrian pilots in 1982 popped up to
   reorient, zoomed into the CAP, or levelled off and flew home (`nwc_83_027`). Argentine Dagger pilots wasted their
   airbrake chaff by opening the brakes to keep formation (`twz_2025_argentine_chaff`). The skill model should change
   the reaction delay, the chance of no reaction, the choice of manoeuvre and countermeasure misuse.
9. **Design rules per faction are in section 2.6.** They are anchored on these counts, but the probabilities are
   design estimates and are marked so.

---

## 2. Findings by faction

Each finding has an id, the date it holds for, and [source, page]. "Lineage" means the faction's 1985 doctrine
descends from that experience: NATO from the USAF, USN and IAF record; the Warsaw Pact from Soviet doctrine and
Afghanistan; the neutral neighbour from a non-aligned air force without RWRs (Argentina 1982 as analogue); the rebels
from irregular air arms with no electronic warning; our side from the VPAF and the Syrian Air Force.

### 2.1 NATO (USAF, USN and IAF lineage)

**Detection by eye and by receiver**

- **reactions-01. The eye was the main sensor.** In events II-30 to II-47 of the F-105 MiG record (16 events),
  first detection was visual with no prior warning in 13, by radio or electronic warning in 2 (an X-band vector warning;
  ETHAN BRAVO), and by a friendly's call in 1. About 6 of the 12 attacked crews saw the attacker before being fired on.
  In events II-9 to II-29, almost all first sightings were also visual. Date: 1965-Feb 1967.
  [`red_baron_vol2`, events II-9 to II-47]
- **reactions-02. Visual ranges of fighters.** First sightings of MiGs were at 600 yd (II-41), 2,000-3,000 ft (II-39),
  about 4,000 ft (II-45), about 5,000 ft (II-1), 1-1.5 mi (II-37, II-46), 2 nm (II-32) and 4-5 mi (II-30). A bogey was seen
  at about 15 nm (II-40) and three MiGs at about 10 mi (II-47). The "7 mi", "against terrain" and "out of the sun"
  details were not found on re-reading events II-1 and II-30 to II-47 (unverified; they may be in II-9 to II-29).
  Date: 1965-Sep 1966. [`red_baron_vol2`, events II-1, II-30 to II-47; corrected 15 Nov 2026]
- **reactions-03. Visual cues of an SA-2 launch.** The first-stage booster raised a dust cloud on the ground, and
  launches were seen through it (`checo_1967_tactics` p. 31). The missile looked like "a flying telephone pole"
  (`werrell_2005` p. 121). A pilot's memoir says the booster "creates a good-sized dust storm on the ground" and "if you
  can see Sam, you can usually escape" (`broughton_1969`, snippet only). No source gives a launch-detection range as a
  number. Date: 1965-72.
- **reactions-04. Cloud and the eye.** Wild Weasels needed "a safety margin of 8000 feet above the cloud deck in order
  to react to SAM launchings", because they "had to see missiles to be sure of eluding them". Flying close to clouds
  was prohibited. Nalty puts the 8,000 ft margin down to "Linebacker experience", so the figure holds for 1972; the
  rule against flying too close to clouds between the aircraft and the ground is earlier (`werrell_2005` p. 120).
  Date: 1972 (the margin); 1966-72 (the cloud rule). [`nalty_1977`, Linebacker chapters; `werrell_2005` p. 120]
- **reactions-05. The receivers were swamped.** Warning gear "often was inundated by radar signals" in 1972, and "the
  human eye was more reliable than electronics". Date: 1972. [`nalty_1977`, Linebacker chapters]
- **reactions-06. The launch light.** The Fan Song's PRF doubled 30-40 s before a launch. The guidance signal began
  no later than 4 s after the launch and lit the launch light. The early WR-300 lit a red light when a launch was
  imminent. RHAW gave visual and aural warning. Date: 1965-67. [`nalty_1977`; `checo_1967_tactics` p. 19;
  `werrell_2005` p. 121]
- **reactions-07. What a period RWR displayed.** An RWR intercepted radar pulses so that "the type of radar that
  transmitted it, the radar's range, and the pulses' direction of arrival are displayed (via a TV screen) to the pilot".
  It "helps a pilot choose among the options of jamming, dispensing chaff, taking evasive action, or avoiding the
  threat". The ALR-69 on the F-16 and A-10 had deficiencies in "azimuth accuracy, threat ambiguities, false alarms, and
  detection ranges". Date: GAO report of 10 March 1982, on sets then in service. [`gao_masad_82_23` pp. 1-2]
- **reactions-08. RWR dates.** Vector IV became the APR-25 (500 contracted, November 1965). The APR-26 had its
  first combat success in 1966, with production at 250 a month in 1966-67. The APR-35/36/37 followed. The ALR-45 went
  to the Fleet 1970-74. The USAF ALR-46 was the first digital, software-controlled RWR. The ALR-67 contract was
  awarded in 1975; the ALR-69 was under way at the same time. By 1987 the ALR-69 was in "all F-16 aircraft"; the
  ALR-74 upgrade was in development in 1982 and not fielded by 1985. (Partly unverified: the Litton brochure and the
  1987 GAO report's body could not be re-read as text on 15 Nov 2026; GAO's own page confirms only the 1987 report's
  title and date. The APR-25/26 numbers and the "first digital" claim rest on a maker's brochure, tier 4.)
  Date: 1965-1987.
  [`litton_rwr_history` (company history, tier 4); `gao_masad_82_23` p. 1; `gao_nsiad_87_167` p. 1]
- **reactions-09. An RWR cannot warn of an IR missile.** Soviet analysts noted that receivers "did not help if the
  attack was carried out with IR missiles". IR seekers of SAMs detected the radiation of the airframe's skin, heated at
  near- and supersonic speeds, at 8-16 km (Babich: from the exhaust only at subsonic speeds; corrected 15 Nov 2026, the
  8-16 km is skin heating, not exhaust). In 1982 the Israeli F-15s and F-16s over Lebanon carried receivers, jammers and
  containers of IR and radar decoys. Date: 1982 (written 1988).
  [`babich_1988`, chapter «Развитие способов преодоления ПВО»; adversary's view of NATO]
- **reactions-10. Launch warning from other aircraft.** In 1973, "spotters in helicopters warned pilots of missile
  launches" (`werrell_2005` p. 153). Soviet analysts noted launch warning by radio from reconnaissance aircraft
  (`babich_1988`). Big Eye's first MiG warnings (16 April 1965) were sent "in the blind" (`nalty_1977`). Date: 1965-1973.

**Calls and radio**

- **reactions-11. Calls attested in the period.** "MIGs behind you, break right"; "BLUE 1 called MIGs at 12 o'clock"
  (`red_baron_vol2`, events II-30 to II-47). "MIG RED" warnings from Big Eye (24 June 1965, `red_baron_vol2` II-2).
  Border-warning code words were changed daily, and some pilots turned off their transponders near the border
  (`nalty_1977`). The callsigns Red Crown and Disco were in use in 1972; Teaball from 30 July 1972; Red Crown as primary
  control from 14 September 1972 (`nalty_1977`). Bingo fuel was a planning term in 1967 (`checo_1967_tactics`: pods
  added 500 lb on bingo). Date: 1965-72.
- **reactions-12. Radio congestion killed warnings.** On 29 September 1972, "repetitive SAM alerts choked the radio
  channels and prevented the Weasel crew from hearing the warning". EC-121 warnings were "not received" in 1966, and
  Big Eye "did little more than garble up Guard Channel" (unverified: not found in event II-1 or II-2 on re-reading).
  The 4 April 1965 loss is put down to a "communications
  'breakdown' between flight elements". Date: 1965-72. [`nalty_1977`; `red_baron_vol2` II-1, II-9 to II-29]
- **reactions-13. Radios.** The ARC-27 was obsolete; the ARC-109 arrived in September 1968; voice scramblers from
  December 1966. RIVET TOP gained direct communication with the strike force on 6 October 1967 and helped to kill 10
  MiGs. Date: 1966-68. [`nalty_1977`; `checo_1969_tactics` pp. 24-25]

**Manoeuvre against SAMs**

- **reactions-14. Against the SA-2: see it, wait, then pull.** Pilots dove toward the missile, then pulled up "as
  abruptly and as sharply as possible" once the missile changed direction (`werrell_2005` p. 120). An F-105F waited until the missile "would not be able
  to maneuver" and pulled up; the missile burst 500 ft below (`checo_1967_tactics` p. 31). Hits came mostly to aircraft
  that "had taken no evasive action or had only initiated such action" (`checo_1967_tactics` p. 30). SUPERHEAT 3 flew
  straight on and was hit; DAGGER 2 rolled inverted and was hit (ibid.). Date: 1965-67.
- **reactions-15. SA-2 timing figures.** The warhead armed about 1,000 ft from intercept, about 23 s after launch;
  self-destruct came at 57-63 s. Optical guidance was introduced in 1968, which removed the radar cue for those shots.
  CHECO puts the SA-2's effective envelope "above 1500 feet" and gives single hits at 4,500, 5,000 and 6,000 ft; it does
  not say that most hits were at 4,500-6,000 ft (corrected 15 Nov 2026: examples, not a distribution). Date: 1965-68.
  [`nalty_1977`; `checo_1967_tactics` pp. 30-31]
- **reactions-16. Dive into flak, climb out.** Taksan 1 (8 October 1966) had already jettisoned its stores, except its
  ECM pods, to evade a MiG; after an engine restart at 10,000 ft it saw an SA-2 launched at it, dove to 2,000 ft to evade
  it, then climbed above 4,000 ft; the report says it "did not take any hits" from the SAM or the guns. Battle damage
  came earlier, from radar-directed 85 mm bursting close. (Corrected 15 Nov 2026: the jettison was a MiG reaction, not
  a reaction to the 85 mm.) Date: 1966. [`checo_1967_tactics`]
- **reactions-17. Pod formation replaced manoeuvre at height.** From late 1966 strike flights flew pod formation: the
  wingman 1,500-2,000 ft out and 500-1,000 ft up; the element 2,000-3,000 ft back, stepped 1,000-2,000 ft higher ("a
  very loose fingertip"). The quote "Close formation over North Vietnam was finished" was not found in
  `checo_1967_tactics` (unverified). Pods cost 3-4% more fuel and about 500 lb more on bingo. Flights closed up about 60 s
  before release ("Sixty seconds before bomb release time, close formation assumed"). In 1968 the 355th TFW flew 3,000-4,000
  ft across and 750 ft deep, the 388th 3,500 ft across and 1,500 ft deep; in 1972 the manoeuvre formation had elements up
  to 6,000 ft apart laterally with about 1,500 ft between the aircraft of an element (corrected 15 Nov 2026: the
  355th/388th figures are 1968, not 1972). Pods "permitted operations from 10,000 to 17,000 feet"; the tighter pod
  formations were "more vulnerable to MiG attack" (`werrell_2005` p. 126). Date: 1966-72. [`checo_1967_tactics`; `checo_1969_tactics` p. 18; `nalty_1977`; `werrell_2005` p. 126]
- **reactions-18. Pods and loss rates.** The loss rate fell from 0.0037 to 0.0021 with pods (`checo_1969_tactics`
  p. 20). Without pods the loss rate was "four time[s]" that with pods (`nalty_1977`). Electronic countermeasures "came
  to take the place of surprise" (`nalty_1977`). Date: 1966-72.
- **reactions-19. Turns off target were the weak point.** In Linebacker II, B-52 post-target turns of 113-160° were
  vulnerable; from 24 December chaff was released on Fan Song tracking in turns sharper than 45° at 30° of bank or
  more. Cells flew 2 min apart, later 4. On 18 December 164 SAMs downed 3 B-52s and damaged 2; on 20 December 221 SAMs
  downed 6. Over the campaign 882 SAMs destroyed 15 B-52s and damaged 9. Date: December 1972. [`nalty_1977`]
- **reactions-20. Recce "slice" off target.** A hard 5-6 g turn of 120-135°, slightly descending, then egress at
  5,000-8,000 ft. Crews had become "stereotyped" in routes and heights. Date: 1967-68. [`checo_1969_tactics` pp. 27-28]
- **reactions-21. Against low-level SAMs and MANPADS (1972-73).** SA-7: 528 sighted and 45 aircraft destroyed and
  6 damaged, 1972 to January 1973; aircraft "above 9000 feet and at speeds faster than 500 knots were generally safe";
  flares were used as decoys (`nalty_1977`, "A Dangerous Latecomer: The SA-7"). From 29 April to 1 September 1972, 351
  SA-7s were fired in 221 incidents, downing 17 fixed-wing and 9 rotary-wing aircraft: 1.8 missiles per helicopter
  kill, 10 per slow fixed-wing kill, 135 per F-4 kill; crews answered with flares and more speed and height
  (`werrell_2005` p. 128).
- **reactions-22. Israel 1973.** The IAF had stripped its ECM before the war and stopped flying within 10-15 mi of
  the canal (`werrell_2005` p. 152). It then carried chaff first in the speed brakes, then improved US pods; used flares
  and ignited jettisoned fuel as decoys; turned toward the missile to show it a "cold side", and crossed flight paths to
  make a "hot spot"; dove steeply on SA-6 sites and fired Shrikes. The SA-7 hit tails; aircraft could outrun it
  (`werrell_2005` pp. 152-153). Date: October 1973.
- **reactions-23. Israel 1982.** ECM pods, chaff rockets, stand-off jammers and drones as decoys (`werrell_2005`
  p. 156). E-2C pictures were downlinked to the command post; the IAF jammed Syrian fighter communications
  (`grant_2002`). The IAF had good anti-jamming radio procedures, pre-briefed frequencies and dual radios
  (`nwc_83_027`). Date: June 1982.
- **reactions-24. Falklands, British Harrier GR3 against radar SAMs and radar flak.** GR3 chaff was at first a
  temporary fitting inside the airbrake; proper dispensers came later. When No. 3 called radar tracking by Super
  Fledermaus/Roland, the leader dropped chaff and No. 3 broke away, also dropping chaff. A Roland fired after an
  aircraft that dropped behind a hill; "a Roland was launched after each retreating aircraft", and none hit. Run-ins
  were low and fast, egress at ultra-low level and full speed. Radar-laid twin 35 mm flak was "far more effective" than
  the SAMs. Hit by small arms, the author lost fuel and ejected after 9-10 min. Date: May-June 1982.
  [`pook_1985`, read as the web page's summary of the 1985 article]
- **reactions-25. Wild Weasel ARM results.** The Wild Weasel ARM kill probability was "just 5 percent", by a Security
  Service assessment cited in Corona Harvest (Nalty's conclusions). The first 107 Shrikes (18 April-15 July 1966) gave
  1 confirmed and 38 probable hits; 908 Standard ARMs gave 3 confirmed and 96 possible hits. Date: 1966-72. [`nalty_1977`]

**Chaff, flares and countermeasure loads (before 1985)**

- **reactions-26. Improvised chaff.** Nalty writes that "as early as spring 1967" F-4E squadrons taped chaff bundles
  in the speed-brake well and opened the brake on a SAM sighting or a missile warning, then evaded. The type is doubtful:
  the F-4E reached Southeast Asia only in late 1968, so Nalty probably means F-4C/D units (noted 15 Nov 2026; the practice
  and its date are as written, the type is unverified). An RF-4C pilot used his photoflash cartridge dispenser to drop two
  of three chaff containers. Date: 1967. [`nalty_1977`]
- **reactions-27. Chaff corridors.** EB-66 hoppers held 348 bundles each, released at about 12 a minute per hopper.
  An EB-66 might begin dropping 3 h or more before a strike. The Paul Doumer corridor of 10 May 1972 was 2 nm wide,
  4,000 ft deep and 34 nm long, laid by two four-plane F-4 flights about 15 min ahead. "Corridors needed 20-30 min to
  form" was not found in Nalty on re-reading (unverified). Miss distance against Navy aircraft was 1,586 ft with chaff and 1,162 ft without; the JCS
  found 22.5% of misses outside a corridor came within 200 ft, against 6.5% inside. Date: 1972. [`nalty_1977`]
- **reactions-28. ALE-40.** The ALE-40 dispenser (about 1978 onward) is in the task's scope, but no tier 1-4 source
  for its period load was reached. The only figures found were on tier-5 pages and are not used. See open items.

**Fighters: jettison, press on, abort**

- **reactions-29. Jettison and break into the attack.** On 4 April 1965 B3 and B4 jettisoned stores and broke into
  the MiGs; "Neither B1 nor B2 reacted to the warnings" and both were hit by cannon fire (`red_baron_vol2` II-1). Zinc
  flight jettisoned its tanks and its bombs (eight 750 lb bombs per F-105) to turn against the MiGs (`nalty_1977`). On 24 June 1965, under "MIG RED" warnings all mission, all aircraft
  jettisoned tanks, and BLUE 4 went to Mach 1.1 and the deck (`red_baron_vol2` II-2). Date: 1965.
- **reactions-30. Counts per attacked flight.** Events II-9 to II-29: about 8 jettisoned (often tanks only, keeping
  the bombs), about 6 pressed on, about 6 disengaged or aborted, often for fuel. Events II-30 to II-47 (16 events):
  jettisoned all stores 3, tanks only 2, mixed 2, ordnance only 1; pressed on 6; aborted 3. "MIGs would usually depart
  anytime you Jettisoned ordnance or tanks" (BLUE 1, event II-28); "As soon as we dropped ordnance, they took off, which
  is what they always did" (BLUE 2, event II-46). Events II-30 to II-47 are July-September 1966. Date: 1965-Feb 1967.
  [`red_baron_vol2`]
- **reactions-31. One attempt, 21 jettisons.** On 21 January 1967 one MiG attempt "caused 21 U.S. aircraft to
  jettison ordnance". Date: 1967. [`checo_1969_tactics` p. 4]
- **reactions-32. Training for the fight.** One F-105 pilot "Had no formal air-to-air training for 13 years" (event
  II-15); another had "Very little ACM training" (II-17). Pilots complained of the "complicated switch settings" from
  ground attack to air-to-air (II-21, II-23), and of canopy condensation and vapour (II-19, II-21). (Corrected 15 Nov
  2026: individual pilots' comments, not a statement about all F-105 pilots.) Date: 1965-66. [`red_baron_vol2`]
- **reactions-33. Exit rule on MiG warning.** After the 23 May 1968 MiG foray south, whenever MiGs were detected below
  19°N "all tactical aircraft would immediately exit below 18 degrees North", to give the Navy's ship-borne SAMs clear
  targets (a "clear fire" area). Date: 1968. [`checo_1969_tactics` p. 40]

### 2.2 Warsaw Pact (Soviet doctrine and Afghanistan)

- **reactions-34. The doctrine of the counter-SAM manoeuvre.** The Soviet Military Encyclopedia defines
  «противоракетный манёвр» as a coordinated change of course, speed and height. Speed goes to the maximum to cut the
  number of possible launches; height is changed to leave the zone through its lower or upper boundary. The
  manoeuvre's start and end points are planned before the flight from the sites' positions, types and zones. Under
  sudden fire it is flown at once by a preset programme. It is combined with jamming. «Противозенитный манёвр»
  (against guns) follows the same logic. Date: Soviet doctrine in the encyclopedia, believed vol. 6 (1978), unconfirmed.
  [`sve_prm`, both entries]
- **reactions-35. Soviet reading of the counter-SAM manoeuvre (adversary's view of NATO, used as the Pact's
  lesson).** Babich describes a US pair over the DRV: one aircraft, the feint, stayed in the "danger zone" at
  1,500-3,000 m so that its pilot could note the moment of launch, then went into a steep spiral toward "the closest
  boundary" of the lethal zone; meanwhile the second aircraft, the striker, increased speed and pressed to the target at
  500-800 m. (Corrected 15 Nov 2026: the 500-800 m aircraft was the striker, not the decoy.) A lead of up to 15 km "did not
  result in frustrating guidance"; the manoeuvre's effect "depended on the accuracy of the determination of the moment for
  its initiation". Against guns: snaking, scissoring, slipping; and the star raid from several directions at once.
  Date: written May 1984 on Vietnam experience. [`mt_1984_babich`, «Военная мысль» No. 5, 1984, pp. 34-43]
- **reactions-36. The low-level survival corridor.** At 60-90 m the survival probability was above 0.75; doubling
  speed from 370 to 740 km/h cut vulnerability fourfold (Babich citing US military experts: a Western estimate relayed
  by a Soviet author, not a Soviet measurement). The anti-fighter turn into the attacker at
  the maximum angular rate needed detection near the limit of sight. Date: written 1988 on the period to 1982.
  [`babich_1988`]
- **reactions-37. Soviet receivers.** On the An-26 the SPO-10 sent an intermittent 800 Hz tone to all the crew's
  headphones and a light signal to the commander's S3M indicator, from which he judged the direction of an attacking
  fighter; the character of the signal showed the attacker's radar mode. It had four antenna-detector units (S3M-1) and
  could switch on the chaff release automatically. (Corrected 15 Nov 2026: the page speaks of the direction of attack,
  not of bearing-sector lamps, and gives no date; the 1976 date rests on a secondary pointer only.) Date: in service by
  the 1970s (low confidence). [`spo10_tulin`, tier 4] The SPO-15's in-service date was not found; see open items.
- **reactions-38. Flares in Afghanistan, before 1985.** The Su-25 carried four ASO-2V with PPI-26 (LO-56) flares.
  Pilots used them rarely: the control panel was at the pilot's side and the supply lasted about a minute. When a
  launch was seen it was too late to dispense. The first Su-25 was probably lost to a MANPADS on 16 January 1984. MANPADS
  in 1984 were the Strela-2M and the Redeye, rarely the Blowpipe. Date: 1981-84. [`markovsky_afghan`]
- **reactions-39. Attack heights in Afghanistan.** Su-25 attacks from 600-1,000 m; Su-17s and fighters from
  2,000-2,500 m; waiting zones 3,000-3,500 m. Low-level flying at 50-150 m (on hunting missions) "finally outlived
  itself" as the flak grew. Markovsky dates none of these heights; placing them before the Stinger (late 1986) is this
  note's inference, so they may straddle 1985 (marked 15 Nov 2026). Date: about 1984-86 (inferred). [`markovsky_afghan`]
- **reactions-40. Chaff curtains (Soviet account).** The 16 April 1972 Haiphong chaff curtain was laid about 30 min
  ahead and lasted more than 3 h. About 8,000 shells per kill in Korea and Vietnam (Babich citing *International
  Defense Review*: a Western journal's figure relayed, not a Soviet count). Date: written 1988.
  [`babich_1988`; adversary's view of the US]

### 2.3 Neutral neighbour (non-aligned air force without RWRs; Argentina 1982 as analogue)

- **reactions-41. No warning receivers.** "Our planes had no radar warning receiver": the A-4B/C, Mirage III, Dagger
  and Canberra flew "unprotected". Thomson-CSF had proposed Sherloc and Remora for the Mirage IIIEA in November 1980;
  nothing was fitted. Argentina "flew and fought without ECM". Date: 1982. [`twz_2025_argentine_chaff` (veterans'
  words); `werrell_2005` p. 161]
- **reactions-42. Improvised chaff and flares.** 140 kg of chaff, cut on a pasta machine, by early May. Taped in the
  airbrakes of the Mirage IIIEA and Dagger; Dagger pilots wasted it by opening the brakes to keep formation. The
  Canberra had 7 horizontal tail launchers, operational by 1 May; each flare burned 15 s. A separate CITEFA launcher for
  the A-4C allowed two launches every 5 s, but its 2 June test gave "no positive results". (Corrected 15 Nov 2026: the
  2-per-5-s rate is the A-4C launcher's, not the Canberra's.) Date: 1982. [`twz_2025_argentine_chaff`]
- **reactions-43. Seeing an AIM-9L launch.** Capt. García Puebla, leading Canberras, saw "a small whitish flash" come
  from a cloud "with astonishing speed" and called a break; the AIM-9L hit his No. 2 Canberra, which had not launched its
  flares. Date: 1982. [`twz_2025_argentine_chaff`]
- **reactions-44. Evasion by eye.** A Dagger pilot on 21 May: "Virage, zigzag, evasión"; he climbed to 4,000 m and
  descended to 1,000 m, and saw "el misil que viene por el flanco". Date: 1982. [`infobae_2017_piuma`]
- **reactions-45. Against fighters: jettison and run.** A-4s jettisoned drop tanks and climbed to outrun Sea
  Harriers, which broke off. Sea Darts passed close to Daggers. Flights left under radio silence. Date: 1982.
  [`fundacion_malvinas_24may`]
- **reactions-46. What the British SAMs achieved against them.** Kills by system are disputed (section 4). Sea Dart
  forced the attackers low and engaged one target at a time. Argentine sources put 18 of their losses to SAMs and 21 to
  Harriers, out of 44-55 lost. Date: 1982. [`werrell_2005` pp. 164-167]

### 2.4 Rebels (irregular air arm, eyes only)

No source on a rebel air arm's reactions was read. The evidence that applies is about the aircraft such a force would
fly (helicopters, trainers, light attack) and about crews with no electronic warning.

- **reactions-47. Slow aircraft are easy targets.** SA-7s needed 1.8 missiles per helicopter kill and 10 per slow
  fixed-wing kill, against 135 per F-4 kill (1972). Above 9,000 ft and over 500 kt was generally safe. Date: 1972.
  [`werrell_2005` p. 128; `nalty_1977`]
- **reactions-48. With no receiver, the launch is seen late or not at all.** Argentine and Su-25 evidence: the
  flash or the trail is the only cue, and when it is seen it is often too late to dispense. Date: 1982-84.
  [`twz_2025_argentine_chaff`; `markovsky_afghan`]

### 2.5 Our side (VPAF and Syrian lineage, and our pilots under the enemy's jamming)

- **reactions-49. Hit-and-run as the fighter's answer.** VPAF MiGs used single-pass hit-and-run attacks under strict
  radio and radar silence. Date: 1967-68. [`checo_1969_tactics` p. 39]
- **reactions-50. Syrian pilots in 1982 under jamming.** Syrian pilots "lacked the training, experience,
  organization, and leadership needed". Some popped up to reorient and were shot down; some "zoomed up" into the CAP;
  confused pilots "simply leveled off and headed straight back to Syria", where Israeli fighters shot them down. Israeli
  communications jamming and deception were used against them; the report gives 85 Syrian aircraft lost (executive
  summary, ch. III) and, in ch. V, 81 Israeli fighter kills, unreconciled. Early AIM-9L shots were taken from rear-aspect approaches. Date: June
  1982. [`nwc_83_027`, a Western observer's view; `grant_2002`]
- **reactions-51. Our side's warnings came by GCI.** The Syrian fighters depended on ground control, which the IAF
  jammed ("Selective airborne communications jamming frazzled the airwaves for the Syrian MiG-21s"). Date: 1982.
  [`grant_2002`]

### 2.6 Design rules per faction and skill (design estimates)

Everything in this section is a **design estimate**, anchored on the findings above. It is not a sourced figure. Each
rule names its anchors.

**Cues a crew can act on (all factions)**

| Cue | Who gets it | Design estimate | Anchor |
|---|---|---|---|
| Visual: launch dust or smoke, large booster (SA-2 class) | anyone looking that way, day, below cloud, clear air | seen in the forward hemisphere out to about 10 km; behind the beam only if a wingman calls it | reactions-03, -04 (no number in any source) |
| Visual: small SAM's smoke trail (Osa, Roland, Rapier class) | as above | 3-6 km in day VMC; not at night; not from above cloud | none found; open item (9M33 trail) |
| Visual: MANPADS flash or trail | as above | 1-3 km; often too late to act | reactions-38, -43 |
| Visual: fighter | as above | 2-7 km typical; up to 15 nm against sky at best; out of the sun: late | reactions-02 |
| RWR search or track tone | NATO; Pact SPO-10 class | type and rough bearing only; known radars only; false alarms | reactions-06, -07, -37 |
| RWR launch light | NATO receivers that know the SAM's guidance signal | a few seconds after launch; none for IR shots or unknown radars | reactions-06, -09 |
| Radio warning (AWACS, GCI, wingman, helicopter spotter) | all with working radios | delayed and lost in congestion; may be jammed | reactions-10, -12, -51 |

**Reaction delay (from the cue to the manoeuvre's start)**, design estimates:

| Skill | Delay | Chance of no reaction to a warning | Anchor |
|---|---|---|---|
| Veteran | 1-3 s | 2% | reactions-14, -35 |
| Trained | 2-5 s | 10% | reactions-14 |
| Green | 4-10 s | 25% | reactions-29 (B1 and B2), -32, -50 |

**Manoeuvre by threat**, design rules:

- Radar or command-guided SAM seen or warned, NATO: chaff on the tracking cue if carried; wait while the missile
  closes; then a hard pull and a turn to the zone's nearest edge, descending if already low; at height, a dive toward
  the missile, then a pull. A late or mistimed start loses most of the effect. (reactions-14, -24, -26, -35)
- Same, Warsaw Pact: the preplanned programme from the briefing, flown at once; full speed; out by the nearest
  boundary; jamming on. Little improvisation. (reactions-34, -35)
- Same, neutral: no receiver, so react only to a sighting or a call; break and zigzag; climb or descend by eye.
  (reactions-41 to -44)
- IR SAM or MANPADS: no receiver warning; flares only if seen in time; NATO flares by reflex at low level over known
  MANPADS belts (1973, 1982 practice); Pact flares rarely before 1985; speed and height as the main defence.
  (reactions-21, -22, -38)
- Flak: jink; climb above the guns' effective height if the mission allows (see AAA_DAMAGE §6).
- Fighter: break into the attacker if seen; jettison tanks, then bombs if the fight is close; run for the deck at
  full speed; the attacker often leaves once the bombs are gone. (reactions-29, -30)

**Jettison, press on, abort**, per attacked flight, design estimates refining AAA_DAMAGE §7:

| Situation | Jettison anything | Of those, tanks only | Press on | Abort | Anchor |
|---|---|---|---|---|---|
| Fighters attack the flight, NATO trained | 50% | 40% | 35% | 20% | reactions-30 |
| Fighters attack, neutral | 60% (tanks first) | 60% | 25% | 30% | reactions-45 |
| SAM launch seen against the flight, NATO | 40% (as AAA_DAMAGE) | n/a | 60% | 10% | AAA_DAMAGE §7, reactions-14 |
| SAM launch seen, Pact | 20% | n/a | 70% (programme) | 10% | reactions-34 |
| Our side's fighters under jamming, green | n/a | n/a | n/a | 30% go home | reactions-50 |

**Report.** Every crew reports what it saw, not what happened. Rules for the enemy's after-action records follow
`docs/air_war/`. For our side's records, this research supports no reported launch where no one looked or no receiver
alarmed (reactions-01, -05).

---

## 3. Numbers table

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| First detection of a fighter attack: visual, no warning | 13 of 16 events | NATO | 1965-67 | `red_baron_vol2` II-30 to II-47 | high |
| Attacked crews who saw the attacker before being fired on | about 6 of 12 | NATO | 1965-67 | `red_baron_vol2` | medium |
| Visual first-sighting range of MiGs | 600 yd to 4-5 mi typical; about 10 mi once; a bogey at about 15 nm | NATO | 1965-66 | `red_baron_vol2` II-1, II-30 to II-47 | high (corrected) |
| Jettison per attacked flight (16 events) | 8 of 16 (all 3, tanks 2, mixed 2, ordnance 1) | NATO | 1965-67 | `red_baron_vol2` | high |
| Pressed on / aborted (16 events) | 6 / 3 | NATO | 1965-67 | `red_baron_vol2` | high |
| Jettison / press on / abort, events II-9 to II-29 | about 8 / 6 / 6 | NATO | 1965-67 | `red_baron_vol2` | medium (counted from narrative) |
| Aircraft jettisoning after one MiG attempt | 21 | NATO | 21 Jan 1967 | `checo_1969_tactics` p. 4 | high |
| Fan Song PRF change before launch | 30-40 s | our side (SA-2) | 1965-67 | `nalty_1977` | high |
| Guidance signal after launch | no later than 4 s | our side (SA-2) | 1965-67 | `nalty_1977` | high |
| SA-2 warhead arming | about 1,000 ft from intercept, about 23 s after launch | our side (SA-2) | 1965-67 | `nalty_1977` | high |
| SA-2 self-destruct | 57-63 s | our side (SA-2) | 1965-67 | `nalty_1977` | high |
| Height above cloud deck to react to SAMs | 8,000 ft | NATO | 1972 (Linebacker experience) | `nalty_1977` | high |
| SA-2 hits | envelope "above 1500 feet"; single hits at 4,500, 5,000, 6,000 ft (examples, not a distribution) | NATO | 1965-66 | `checo_1967_tactics` pp. 30-31 | medium (corrected) |
| SAMs per kill | 11 (1965-Apr 1966); 13 (20 Jul-2 Aug 1966) | NATO losses | 1965-66 | `checo_1967_tactics` | high |
| SAMs per kill, unpodded 1966 / podded 1972 | 37 / 65 | NATO losses | 1966, 1972 | `nalty_1977` | high |
| Loss rate before / after pods | 0.0037 / 0.0021 | NATO | 1966-68 | `checo_1969_tactics` p. 20 | high |
| SAMs fired per month | 431 (Mar 1967); 582 (Oct 1967) | our side (SA-2) | 1967 | `checo_1969_tactics` p. 20 | high |
| Pod formation spacing | wingman 1,500-2,000 ft out, 500-1,000 ft up; element 2,000-3,000 ft back | NATO | 1966-67 | `checo_1967_tactics` | high |
| Pod formation size | 3,000-4,000 x 750 ft (355th), 3,500 x 1,500 ft (388th) in 1968; elements to 6,000 ft apart, ~1,500 ft within an element in 1972 | NATO | 1968, 1972 | `nalty_1977` | high (dates corrected) |
| Pod fuel cost | +3-4%; +500 lb on bingo | NATO | 1967 | `checo_1967_tactics` | high |
| Close-up before release | about 60 s | NATO | 1967-68 | `checo_1969_tactics` p. 18 | high |
| Recce slice | 5-6 g, 120-135°, egress 5,000-8,000 ft | NATO | 1967-68 | `checo_1969_tactics` pp. 27-28 | high |
| Linebacker II turns that were vulnerable | 113-160° | NATO | Dec 1972 | `nalty_1977` | high |
| Linebacker II chaff rule | turns over 45° at 30° bank or more | NATO | from 24 Dec 1972 | `nalty_1977` | high |
| Linebacker II | 882 SAMs; 15 B-52s destroyed, 9 damaged | NATO | Dec 1972 | `nalty_1977` | high |
| EB-66 chaff hopper | 348 bundles; about 12 per min | NATO | 1972 | `nalty_1977` | high |
| Chaff corridor | 2 nm x 4,000 ft x 34 nm, about 15 min ahead ("20-30 min to form" unverified) | NATO | 10 May 1972 | `nalty_1977` | high (size); unverified (formation time) |
| Miss distance with / without chaff | 1,586 / 1,162 ft | NATO (USN) | 1972 | `nalty_1977` | high |
| Misses within 200 ft, outside / inside corridor | 22.5% / 6.5% | NATO | 1972 | `nalty_1977` | high |
| Wild Weasel ARM kill probability | about 5% | NATO | 1966-72 | `nalty_1977` | medium |
| SA-7 1972 to Jan 1973 | 528 sighted; 45 destroyed, 6 damaged | NATO losses | 1972-73 | `nalty_1977` | high |
| SA-7 safe envelope | above 9,000 ft and over 500 kt | NATO | 1972 | `nalty_1977` | high |
| SA-7, 29 Apr-1 Sep 1972 | 351 fired, 221 incidents, 26 down (17 fixed-wing, 9 rotary) | NATO losses | 1972 | `werrell_2005` p. 128 | high |
| SA-7 missiles per kill | 1.8 helicopter; 10 slow fixed-wing; 135 F-4 | NATO losses | 1972 | `werrell_2005` p. 128 | high |
| IAF stand-off from the canal | 10-15 mi | NATO (IAF) | Oct 1973 | `werrell_2005` p. 152 | high |
| APR-25 first order | 500 sets, Nov 1965 | NATO | 1965 | `litton_rwr_history` | medium |
| APR-26 production | 250 a month | NATO | 1966-67 | `litton_rwr_history` | medium |
| ALR-74 plan (not fielded by 1985) | 2,607 sets (1,388 F-16, 707 A-10, 512 F-4E) | NATO | 1982 | `gao_masad_82_23` | high |
| US RWR programmes | 9, over $6.6 billion | NATO | 1987 | `gao_nsiad_87_167` | high |
| Low-level survival corridor | 60-90 m, survival over 0.75 | general | to 1982 | `babich_1988` | medium |
| Vulnerability with speed doubled 370 to 740 km/h | cut fourfold | general | to 1982 | `babich_1988` | medium |
| IR SAM seeker detection of skin heating (near- and supersonic) | 8-16 km | general | to 1982 | `babich_1988` | medium (corrected: not exhaust) |
| Feint aircraft's height in the danger zone before its spiral | 1,500-3,000 m | NATO (Soviet view) | Vietnam, written 1984 | `mt_1984_babich` | high |
| Lead that did not defeat guidance | up to 15 km | NATO (Soviet view) | Vietnam, written 1984 | `mt_1984_babich` | high |
| Striker's height while the feint drew the missile | 500-800 m | NATO (Soviet view) | Vietnam, written 1984 | `mt_1984_babich` | high (corrected: striker, not feint) |
| Haiphong chaff curtain | about 30 min ahead, over 3 h | NATO (Soviet view) | 16 Apr 1972 | `babich_1988` | medium |
| Su-25 flare dispensers | 4 ASO-2V; supply about 1 min | Warsaw Pact | 1981-84 | `markovsky_afghan` | medium |
| Afghan attack heights | Su-25 600-1,000 m; Su-17 2,000-2,500 m; waiting 3,000-3,500 m | Warsaw Pact | about 1984-86 | `markovsky_afghan` | medium |
| SPO-10 tone | 800 Hz, intermittent | Warsaw Pact | 1970s | `spo10_tulin` | low |
| Argentine improvised chaff | 140 kg by early May | neutral | 1982 | `twz_2025_argentine_chaff` | medium |
| Canberra flares | 7 launchers; 15 s burn (the 2 per 5 s rate is the A-4C's CITEFA launcher, which failed its test) | neutral | 1982 | `twz_2025_argentine_chaff` | medium (corrected) |
| Argentine losses | 44-55; 18 to SAMs and 21 to Harriers by Argentine sources | neutral | 1982 | `werrell_2005` pp. 164-167 | medium |
| Time from small-arms hit to ejection (GR3) | 9-10 min | NATO | 1982 | `pook_1985` | medium |
| Syrian losses, June 1982 | 85 (exec. summary, ch. III); 81 Israeli fighter kills (ch. V), unreconciled | our side | 1982 | `nwc_83_027` | medium |

---

## 4. Disagreements

- **SA-7 in 1972.** `nalty_1977`: 528 sighted, 45 destroyed and 6 damaged, 1972 to January 1973. `werrell_2005` p. 128:
  351 fired in 221 incidents, 26 downed, 29 April to 1 September 1972. The periods differ, and "sighted" is not
  "fired". Both are kept; Werrell's per-kill ratios are used for slow aircraft and helicopters.
- **SA-2 missiles per kill.** CHECO: 11 to 25 April 1966, 13 for 20 July to 2 August 1966 (`checo_1967_tactics`).
  Nalty: 37 in 1966 against unpodded aircraft, 65 in 1972 against podded aircraft. The Soviet count: 3 to 15 May 1967
  (`mt_1973_nvn` p. 7, already in AAA_DAMAGE). The US figures count missiles seen; the Soviet figure counts claims.
  SAM3D uses outcomes from the gameplay model; these are calibration bands only.
- **Falklands SAM kills.** British claims (Rapier 13, later 20; Blowpipe 9) against lower estimates (Rapier 1-3 by
  Argentine-source authors; Blowpipe 2-4); Sea Dart 5-8, Sea Wolf 3-5, Seacat 1-8 (`werrell_2005` pp. 164-167). Recorded,
  not resolved. The reaction findings do not depend on them.
- **SPO-10 naming and date.** The tier-4 page and a secondary pointer give a 1976 technical description; the
  receiver's in-service date and its family name were not confirmed from an operator document.
- **`babich_1988` imprint.** The chapter was read on a reprint site that did not show the imprint; believed Voenizdat
  1988, unconfirmed.
- **The Encyclopedia's volume.** `sve_prm` is believed to be vol. 6 of the Soviet Military Encyclopedia (1978); the
  web page did not show the volume.
- **Chaff in the airbrake: help or harm.** The USAF (1967), the IAF (1973) and the RAF (1982) used it to good effect
  (reactions-22, -24, -26); Argentine Dagger pilots wasted it by opening the brakes to keep formation (reactions-42). The
  same fit is skill-dependent, not a disagreement of fact.

---

## 5. Post-period exclusions

| Item | Date | Why excluded |
|---|---|---|
| Stinger in Afghanistan | September (late) 1986 | after 1985; forced Soviet aircraft higher (`werrell_2005` pp. 170-172) |
| Soviet 4,500 m floor in Afghanistan | late 1986 | after 1985 (`markovsky_afghan`) |
| Su-25 extra armour | August 1987 | after 1985 (`markovsky_afghan`) |
| Su-25 extra flare booms, 256 flares, automatic 30 s release from the combat button | undated in the source; likely 1986-87 | treat as post-period until dated (`markovsky_afghan`) |
| ALR-74 RWR | in development 1982; not fielded by 1985 | (`gao_masad_82_23`, `gao_nsiad_87_167`) |
| Multi-service brevity codes (FM 90-38) | April 1997 | after 1985; words like "Magnum", "Spike", "Mud", "Naked", "Defending", "Notch" are not attested in the period sources read |
| Desert Storm figures and practice | 1991 | hindsight only (`gwaps_vol5`, `rjlee_ds_losses` are in AAA_DAMAGE) |
| Kosovo practice | 1999 | hindsight only |
| Missile approach warners on fast jets | no source read places one in service by 1985 | treat as absent for 1985 until a period source shows otherwise |

In period and kept: Teaball (30 July 1972), Red Crown as primary control (14 September 1972), Disco, the APR-25/26
(1965-66), the ALR-45/46 (1970s), the ALR-67 and ALR-69 (contracts from 1975), the SPO-10 (1970s), the ASO-2V on the
Su-25 (1981-84), the ALE-40 (from about 1978, load unsourced).

---

## 6. Sources

| Id | Tier | Citation | URL | Read | Whose view |
|---|---|---|---|---|---|
| `nalty_1977` | 1 | B. C. Nalty, *Tactics and Techniques of Electronic Warfare: Electronic Countermeasures in the Air War Against North Vietnam 1965-1973*, Office of Air Force History, 1977 | https://allworldwars.com/Tactics-and-Techniques-of-Electronic-Warfare-by-Bernard-Nalty.html (mirror) | about 300k of 344k characters | NATO (USAF) own |
| `checo_1967_tactics` | 1 | Project CHECO, *Air Tactics Against NVN Air/Ground Defenses*, HQ PACAF, Feb 1967, DTIC ADA586117 (registered) | https://archive.org/stream/DTIC_ADA586117/DTIC_ADA586117_djvu.txt | full | NATO (USAF) own |
| `checo_1969_tactics` | 1 | Project CHECO, *Air Tactics Against NVN Air/Ground Defenses, Dec 1966 - 1 Nov 1968*, 30 Aug 1969, DTIC ADA486448 (registered) | https://archive.org/stream/projectcheco-ADA486448/ADA486448_djvu.txt | full | NATO (USAF) own |
| `red_baron_vol2` | 1 | WSEG, *Air-to-Air Encounters in Southeast Asia (Red Baron)*, Vol. II, F-105 events before 1 Mar 1967, DTIC ADC003628 | https://archive.org/stream/DTIC_ADC003628/DTIC_ADC003628_djvu.txt | about 300k of 840k characters | NATO (USAF) own |
| `werrell_2005` | 1 | K. P. Werrell, *Archie to SAM*, Air University Press, 2005, DTIC ADA439255 (registered) | https://archive.org/details/DTIC_ADA439255 | the pages cited | NATO (USAF) own; later history of the period |
| `gao_masad_82_23` | 1 | US GAO, *Need to Examine ALR-74 Radar Warning Receiver Program Schedule*, MASAD-82-23, B-206548, 10 Mar 1982 | https://www.gao.gov/assets/masad-82-23.pdf | pp. 1-2 (page images) | NATO (US) own |
| `gao_nsiad_87_167` | 1 | US GAO, NSIAD-87-167, on US radar warning receiver programmes, 1 Jul 1987 | https://www.gao.gov/assets/nsiad-87-167.pdf | p. 1 only | NATO (US) own |
| `nwc_83_027` | 1 | National War College, NDU/NWC 83-027, April 1983, DTIC ADA130558 (on the 1982 Lebanon air war) | https://archive.org/stream/DTIC_ADA130558/DTIC_ADA130558_djvu.txt | about 100k of 125k characters | Western observer's view of Syria and Israel |
| `grant_2002` | 4 (Ivry's words: tier 2 through press) | R. Grant, "The Bekaa Valley War", *Air Force Magazine*, June 2002 (registered) | https://www.airandspaceforces.com/article/0602bekaa/ | full | Western, with Israeli commanders' words |
| `mt_1973_nvn` | 1 | «Военная мысль», on the DRV's air defence 1965-67, CIA translation 18 Sep 1973 (registered) | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000100260001-7 | not re-read; cited through AAA_DAMAGE | Soviet own; CIA translation |
| `mt_1984_babich` | 1 | В. К. Бабич, «Военная мысль» No. 5, 1984, pp. 34-43, CIA translation (registered) | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000403350001-1 | full | Soviet own, on NATO |
| `sve_prm` | 1 | Советская военная энциклопедия, entries «Противоракетный маневр» and «Противозенитный маневр» (volume believed 6, 1978) | https://военная-энциклопедия.рф (entries under П) | both entries in full | Soviet own |
| `babich_1988` | 2 | В. К. Бабич, «Авиация в локальных войнах», chapter «Развитие способов преодоления ПВО» (imprint believed Voenizdat 1988) | https://tech.wikireading.ru/10867 | the chapter in full | Soviet own, on NATO and Israel |
| `markovsky_afghan` | 2 | В. Марковский, «Жаркое небо Афганистана», chapter «Штурмовики» (serialised in «Авиация и время» 1995 No 5) | http://militera.lib.ru/h/markovsky/03.html | the chapter in full | Soviet participants' accounts, post-Soviet |
| `spo10_tulin` | 4 | O. Tulin, An-26 avionics pages (SPO-10) | https://oleg-tulin.narod.ru/index/0-28 | the page in full | operator-country reference page |
| `litton_rwr_history` | 4 | Litton Applied Technology, "The Radar Warning Story", company brochure (1990s) | https://www.aef.se/Avionik/Artiklar/Motmedel/Nya_hotbilder/RadarWarnStory.pdf | full (text extracted) | maker's own |
| `pook_1985` | 2 | J. Pook, "Air War in the South Atlantic", *Australian Aviation* No. 26, March 1985 | https://australianaviation.com.au/1985/03/__trashed-23/ | the web page's summary, not the full article | British pilot's own |
| `twz_2025_argentine_chaff` | 4 | T. Newdick and M. Sciaroni, "Argentine Air Force went to war with chaff made by pasta machine", *The War Zone*, 14 Mar 2025 | https://www.twz.com/air/argentine-air-force-went-to-war-with-chaff-made-by-pasta-machine | full | Argentine veterans' words via a Western site |
| `infobae_2017_piuma` | 4 | *Infobae*, 1 May 2017, on a Dagger pilot's combat and ejection, 21 May 1982 | https://www.infobae.com/sociedad/2017/05/01/el-dramatico-combate-aereo-y-la-caida-de-un-piloto-argentino-que-se-eyecto-en-malvinas-al-borde-de-la-muerte/ | full | Argentine pilot's own |
| `fundacion_malvinas_24may` | 4 | Fundación Malvinas, "Ataque aéreo al desembarco británico" | https://fundacionmalvinas.org/ataque-aereo-al-desembarco-britanico/ | full | Argentine own |
| `broughton_1969` | 2 | J. Broughton, *Thud Ridge*, 1969 | none (search snippet) | snippet only | NATO (USAF) pilot's own |

Not used: tier-5 pages (FAS, globalsecurity, militaryperiscope) that give an ALE-40 load. Wikipedia was used only as a
pointer. FM 100-2 and CIA estimates were not needed for this topic.

---

## 7. Open items

- TACM 3-1 / MCM 3-1 (USAF tactics manuals of the period): not public in what was reached.
- *USAF Fighter Weapons Review*, 1980s issues: the period's own defensive-manoeuvre and brevity articles.
- «Зарубежное военное обозрение» 1982 No 2: Kirillov pp. 47-53, «Использование опыта локальных войн в тактике ВВС
  НАТО», and Dmitriev pp. 58-61, «Авиационные обнаружительные приёмники». The PDF reached was broken (blank pages); a
  clean copy is needed.
- The SPO-15's in-service date and manual; the ASO-2V's in-service date and cartridge count per unit.
- The ALE-40's period load per aircraft type, from a T.O. or a period USAF source.
- Rubén Moro, *La guerra inaudita* (1985): the Argentine side's reactions and warnings.
- RAF Historical Society Journal 30 (Falklands): GR3 and Sea Harrier countermeasures.
- Red Baron vol. II beyond 300k characters, and Red Baron III (F-4 events 1967-72): more counts of jettison and abort.
- NSWC flare history (Douda, 2009): flare dates and loads before 1985, read only as a pointer.
- The Osa 9M33's smoke trail and its visibility: needed to set the visual launch cue for SAM3D's own missile.
- A launch-detection range by eye, as a number, from any period source.
- Iran-Iraq War reactions: nothing usable was found.
- Israel 1973: the "36 aircraft downed by missiles" figure (Tovy) was seen only as a snippet.
- The web search budget was used up; these leads were not followed further.

---

## Verification (15 Nov 2026)

Adversarial check against the cited texts (WebFetch of the full-text mirrors; no new web searches were possible, the
shared search budget was spent). "Confirmed" means the claim and its numbers were found in the cited source;
"corrected" means the source says something different and the finding was edited above; "unsupported" means it could
not be found or the source could not be read as text.

| Finding | Verdict | Note |
|---|---|---|
| reactions-01 | confirmed | II-30 to II-47 re-read (16 events, Jul-Sep 1966): all first sightings visual except II-44 (X-band on the vector box) and II-46 (ETHAN BRAVO radio warning). |
| reactions-02 | corrected | Ranges re-listed by event (600 yd to 4-5 mi, ~10 mi once, bogey ~15 nm); "7 mi", "against terrain", "out of the sun" not found. |
| reactions-04 | corrected | Nalty quote confirmed; the 8,000 ft margin is "Linebacker experience" (1972), not 1966-72; Werrell's cloud rule is p. 120. |
| reactions-05 | confirmed | Nalty: warning gear "inundated by radar signals"; the eye "more reliable than electronics". |
| reactions-06 | confirmed | Nalty: PRF doubled 30-40 s before launch, guidance no more than 4 s after; CHECO 1967: red light on "launch imminent". |
| reactions-07 | unsupported (partly) | GAO page confirms title and date (10 Mar 1982); the scanned body could not be read as text here, so the quoted display and ALR-69 deficiencies were not re-checked. |
| reactions-08 | unsupported (partly) | Litton PDF and GAO-87-167 body not readable as text; GAO page confirms only title and date (1 Jul 1987). Dates kept with tier-4 caveat. |
| reactions-09 | corrected | Babich: 8-16 km is IR detection of skin heating at near/supersonic speed, not exhaust; the F-15/F-16 were Israeli, over Lebanon. |
| reactions-14 | confirmed | Werrell p. 120 (not 121): telephone pole, dive toward the missile, pull up; CHECO pp. 30-31: no evasive action, SUPERHEAT 3, DAGGER 2, burst 500 ft below. |
| reactions-15 | corrected | Arming ~23 s and ~1,000 ft, self-destruct 57-63 s, optical 1968 confirmed (Nalty); "most hits at 4,500-6,000 ft" is not in CHECO (only examples). |
| reactions-16 | corrected | Taksan 1 jettisoned (keeping pods) against a MiG; the SAM dive and climb came after; "did not take any hits" from SAM or AAA. |
| reactions-17 | corrected | CHECO 1967 spacing and 60 s close-up confirmed; 355th/388th dimensions are 1968, not 1972; "close formation ... finished" quote not found; 10,000-17,000 ft confirmed (Werrell p. 126). |
| reactions-18 | confirmed | CHECO 1969: .0037 (Jul-Sep 1966) to .0021 (Sep 1966-Jun 1967), all route packages; Nalty "four time". |
| reactions-19 | confirmed | Nalty: 113-160 deg turns; chaff on Fan Song in turns >45 deg at >=30 deg bank, effective 24 Dec; 882 SAMs; 15 destroyed, 9 damaged. |
| reactions-21 | confirmed | Werrell p. 128: 351 in 221 incidents, 17 + 9 down, 1.8/10/135; Nalty: 528 sighted, 45 + 6, safe above 9,000 ft and 500 kt. |
| reactions-25 | confirmed | Nalty's conclusions: "kill probability of just 5 percent" (Security Service via Corona Harvest). |
| reactions-26 | corrected | Practice and date as Nalty writes; the type "F-4E" is doubtful for 1967 (F-4E in SEA from late 1968). |
| reactions-27 | corrected | Hoppers, rate, corridor size, miss distances, 22.5%/6.5% confirmed; "20-30 min to form" not found. |
| reactions-29 | confirmed | II-1 "Neither B1 nor B2 reacted to the warnings"; II-2 MIG RED, tanks, Mach 1.1; Zinc: eight 750 lb bombs per aircraft (clarified). |
| reactions-30 | confirmed | II-28 and II-46 quotes found; counts consistent with the II-30 to II-47 re-read. |
| reactions-31 | confirmed | CHECO 1969: "one on 21 January which caused 21 U.S. aircraft to jettison ordnance". |
| reactions-32 | corrected | Comments of individual pilots (II-15, II-17, II-19, II-21, II-23), not a statement about all F-105 pilots. |
| reactions-33 | confirmed | CHECO 1969; added the reason (clear fire for the Navy's SAMs). |
| reactions-35 | corrected | Babich 1984: the 1,500-3,000 m aircraft was the feint that spiralled at launch; the 500-800 m aircraft was the striker. |
| reactions-36 | confirmed | Babich 1988: 60-90 m, >0.75; 370 to 740 km/h fourfold, flagged as US experts' estimate relayed. |
| reactions-37 | corrected | Tulin page: 800 Hz to all phones, S3M indicator gives direction of attack, four S3M-1 units, chaff auto-start; no date on the page. |
| reactions-38 | confirmed | Markovsky: four ASO-2V, PPI-26 (LO-56), panel at the side, about one minute, too late; Ruban 16 Jan 1984 "probably" first. |
| reactions-39 | corrected | Heights confirmed but undated in Markovsky; the 1984-86 date is this note's inference and may straddle 1985. |
| reactions-40 | confirmed | Babich: curtain ~30 min ahead, >3 h; 8,000 shells per kill cited from International Defense Review (flagged). |
| reactions-42 | corrected | 140 kg, pasta machine, airbrakes, Dagger misuse confirmed; 2 per 5 s is the A-4C launcher, not the Canberra. |
| reactions-43 | corrected | García Puebla led Canberras; his No. 2 Canberra, which had not fired flares, was hit. |
| reactions-46 | confirmed | Werrell p. 161 (no ECM), p. 164 (44-55; 21 Harriers, 18 SAMs), p. 166 (Sea Dart forced low level). |
| reactions-50 | confirmed | NWC 83-027 quotes found; 85 vs 81 explained (losses vs Israeli fighter kills). |
| reactions-34 | unsupported (not re-checked) | The encyclopedia site could not be reached; the author's full reading stands, volume and year still unconfirmed. |

**Period scan.** No post-1985 tactic, term or equipment is presented as 1985 practice. The terms used for NATO in 1985
(RWR, launch light, chaff and flares by hand or ALE-40 class dispensers, AWACS/GCI warnings, E-2C downlinks in 1982)
were in service by 1985. Post-1985 items stay in section 5 (Stinger and the 4,500 m floor, late 1986; Su-25 armour,
1987; ALR-74; FM 90-38 brevity, 1997; missile approach warners on fast jets). The Su-25's 256 flares with automatic
release are undated in Markovsky and may fall in 1984-86: they stay excluded until dated. Tier note: the 370-740 km/h
and 8,000-shells figures are Western numbers relayed by a Soviet author, and `mt_1984_babich`, `babich_1988` and
`nwc_83_027` are each one side's view of the other, as labelled.
