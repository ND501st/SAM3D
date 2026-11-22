# Targets of opportunity and spontaneous attacks, 1965-1985

Research for SAM3D's enemy air command (`simcore::airwar`): what a flight does when it sees a target it was not sent
for, and what decides it. The evidence is from 1965-1985, for a war set in 1985. Each item carries a date. Items from
after 1985 are listed in section 5 only. The Gulf War of 1991 and later are cited only as flagged hindsight.

The existing work this adds to: `docs/research/ENEMY_AIR_RESEARCH_2026-10.md` (the MAI teaching aid's four methods,
the free hunt by up to a flight), `docs/air_war/14_LOGISTICS.md` (`MissionKind::ArmedRecce`, with find chances of 0.7
by day, 0.5 at dawn or dusk and 0.15 at night, flown as pairs) and `docs/research/AAA_DAMAGE_2026-11.md` (the jettison
and defensive reactions under fire).

## 1. Summary for SAM3D's design

1. **Most flights never see the target they were not sent for.** In the early 1980s, F-4 crews flying close air
   support in Germany "usually never even saw a target", and when they did they had already passed it. Their
   targets were one or two parked trucks or APCs, dark on a dark background. At 450-500 kt the pop-up began about
   3 nm out, where a tank looks the size of the pipper's dot [bush_a10_simhq] (unverified: the archived interview
   could not be re-read in the 15 Nov 2026 check; every bush_a10_simhq item rests on the first reading only). Misty FACs in 1968 judged that strike pilots on armed
   reconnaissance were "unable to see and/or identify targets". A Misty pilot needed 20 missions to train and about 40
   to be highly qualified [checo_rp1_1969, section on night armed reconnaissance]. NATO's own reading of Soviet practice gave the low, fast attacker 3-6
   seconds to identify its target [fm_100_2_1_1984, p. 12-8]. The find chance has to depend on the aircraft
   type, the crew, the speed and whether the target moves. The aircraft type matters most: slow attack aircraft and
   two-seaters see more than fast single-seaters. A target that moves, raises dust or makes smoke is seen far more
   often than one that is parked.
2. **Authority to attack is set before take-off, by the mission and the area, not by the pilot.**
   - The US Army's terms of 1980 (FM 101-5-1: US Army doctrine, not a NATO publication) define armed reconnaissance
     as a mission to find and attack targets of opportunity in assigned areas or along assigned routes. Beyond the
     FSCL (normally set by corps or an independent division), or in a free fire area (usually set by a division or
     higher), any target may be attacked without further coordination [fm_101_5_1_1980].
   - The 81st TFW's A-10 plan, from the late 1970s to about 1984, made each 5 x 10 nm border area a box of its own once
     the friendly forces had left it. From then on any military object in the box was a target [bush_a10_simhq]
     (unverified, see item 1; and "kill box" is a later word, used in a later interview and absent from the 1980 and
     1984 manuals: do not use it as the 1985 term).
   - In Vietnam the rules allowed free armed reconnaissance only within 200 yards (later 200 metres) of the roads.
     Anything further off needed validation or a FAC, unless it was firing at the aircraft
     [checo_interdiction_1969, ch. IV; checo_roe_1973, ch. III].
   - Flights on other missions might hit back at guns that fired on them. In North Vietnam in 1970, attacks on "other
     unfriendly forces or installations encountered" were prohibited [checo_roe_1973, ch. V].
3. **The Pact's default is to report, not to hunt.**
   - Soviet doctrine as written in 1970 names three methods: strikes on assigned targets, strikes on request, and
     hunting. Strikes on request are named as the principal method [mt_1970_bukhantsev]. The text says this for
     front aviation's missions against enemy naval vessels and landings on a maritime front (Odessa MD exercises),
     not for every mission; reading it as the Pact's general default is an extrapolation.
   - The US Army's 1984 handbook says the Soviets showed "some reservations" about free-hunting flights until air
     superiority was established. A Soviet armed reconnaissance was a reconnaissance aircraft with two to four attack
     aircraft [fm_100_2_1_1984, ch. 12].
   - Every crew on any mission was expected to report enemy activity at once by radio [fm_100_2_1_1984, ch. 12].
   - On-call strikes came from aircraft at readiness one, 3-5 minutes to take-off, or readiness two, 15 minutes
     [fm_100_2_1_1984, ch. 12].
4. **Night hunting worked when a ground post with the movement plan cleared each strike.** In Afghanistan, pairs or
   sections of the most experienced helicopter crews found moving vehicles by their headlights. They reported the
   coordinates to the nearest outpost and struck only once its commander had cleared them [rgs_afghan_2002, pp. 219-220].
   This is the period's clearest rule against fratricide. It needs a curfew or a movement plan to work.
5. **A re-attack is allowed, but it costs.**
   - Soviet doctrine as NATO read it in 1984: if more than one pass is needed, come again from a different direction or
     out of the sun [fm_100_2_1_1984, p. 12-8].
   - Misty FACs pulled away after the first look rather than circle, then came back once, low and fast
     [afmag_2006_misty].
   - In the Falklands in 1982, two of the three Harrier GR3s shot down were re-attacking. This comes from the official
     historian, quoted second-hand [rafm_falklands].
   - In SAM3D a second pass on a defended target should be rarer than the first, should come from a new heading after a
     separation, and should carry a higher loss chance.
6. **Gun and rocket runs were the weapons left over, and the A-10's gun was the strongest of them.** In a 1979 test,
   two A-10s made ten low passes at a mean 3.3 degrees on ten stationary, undefended tanks and immobilised 8 of them,
   about 0.8 a pass [cdat_array20_1980]. A-10 pilots in Europe carried Mavericks, the gun and CBUs, and no iron bombs.
   They meant to shoot from stand-off and "mop up later with the gun" [bush_a10_simhq] (unverified, see item 1). Soviet conventional attacks on landing forces
   used cannon and rockets from a gentle 10-25 degree dive [mt_1970_bukhantsev].
7. **Pilots' reports of what they hit were badly inflated, and they should go into the enemy's records as reported.**
   - Early-morning photo flights in 1969-70 confirmed only 7 of 103 reported truck kills, and the totals were never
     revised [nalty_2005, p. 89].
   - SAM3D's enemy journal should carry the claims. The truth should stay separate. This matches the project's rule
     that the written record is the war's memory.
8. **By faction:**

   | Faction | Habit | Sources |
   |---|---|---|
   | NATO | Attacks freely in pre-assigned boxes (unverified) and beyond the FSCL. Uses FACs and Fast FACs elsewhere. Hits back at guns that fire. | as above |
   | Pact | Reports and calls a strike from readiness. Hunts only in zones it has been given and once it holds the air. | as above |
   | Neutral | Flies planned low runs, approaching "as unseen as possible" (Swedish exercises, early 1980s). No period source on a neutral's emission rules for the run (the "ABBA" item is an exercise convention, see opp-neu-01). | [ericsson_f17_recce] |
   | Rebels | Plan single low passes on infrastructure, but their impulsive shoot-ups cost aircraft. The only account is of low confidence. | [garrison_biafra] |

## 2. Findings by faction

Dates are when the practice was in force, or the period the evidence covers. "Whose view" matters: FM 100-2-1 is the
US Army's view of the Soviet Army, and CIA translations give a Soviet text through US intelligence.

### NATO, with US practice in Vietnam and the Israeli and British wars as analogues

**opp-nato-01 (1980). Terms.**
- Armed reconnaissance is "an air mission flown with the primary purpose of locating and attacking targets of
  opportunity ... in assigned general areas or along assigned ground communications routes, and not for the purpose of
  attacking specific briefed targets".
- A target of opportunity is a target "that has not been planned".
- An immediate mission request is one that could not be identified far enough ahead for detailed planning.
- BAI is planned and coordinated jointly, but "may not require continuous coordination during the execution stage".
- [fm_101_5_1_1980, pp. 1-6, 1-15, 1-111 and the "Immediate mission request" entry]

**opp-nato-02 (1980). Where an attack needs no clearance.**
- The FSCL is a line "beyond which all targets may be attacked by any weapon system ... without endangering friendly
  troops or requiring additional coordination". It is normally set by corps.
- A free fire area is an area into which any weapon may fire without further coordination.
- [fm_101_5_1_1980, FSCL and FFA entries]

**opp-nato-03 (late 1970s to about 1984). The 81st TFW's A-10 squadron areas (unverified).**
- Each pre-planned area along the inner German border was about 5 x 10 nm and assigned to a squadron.
- Once the friendly forces had withdrawn west of it, the box was declared hostile, and "any military object in it
  would, by definition, be a target".
- The aim was to stall Pact columns at choke points (bridges and road junctions), not to destroy them.
- Squadrons trained to fight autonomously if communication with higher headquarters was lost.
- [bush_a10_simhq; Bush flew A-10s in England from 1982]
- (unverified: the archived interview could not be re-read on 15 Nov 2026. "Kill box" is the interview's later
  word, absent from FM 101-5-1 (1980) and FM 100-2-1 (1984); it must not be used as 1985 vocabulary.)

**opp-nato-04 (from about 1984; evaluated in spring 1986). The move to airborne holding.**
- A-10 flights orbited at airborne holding points while waiting for tasking.
- A squadron "Mission Director" received NATO requests by teletype and radioed assignments to pilots in their cockpits
  or in holding.
- Bush's squadron demonstrated this in a NATO evaluation in spring 1986, as the change from the kill boxes to a mobile
  defence.
- [bush_a10_simhq] For 1985 this is transitional (section 4). (unverified, source not re-read on 15 Nov 2026.)

**opp-nato-05 (mid-1980s). A-10 weapons for the role.**
- In Europe: the Maverick and the gun. The only free-fall weapons stocked were CBUs, and there were no iron bombs.
- The aim was not to overfly the enemy: "shoot them from a safe distance, and then mop up later with the gun".
- [bush_a10_simhq] (unverified, source not re-read on 15 Nov 2026.)

**opp-nato-06 (early 1980s). Fast jets rarely find unbriefed targets.**
- An F-4 squadron in Holland flew close air support as a back-up role. Brief: an IP, then a heading and distance; run
  in at 450-500 KIAS; pop up.
- "Usually, we never even saw a target ... or if we did see it, it was too late to attack it."
- The targets were one or two parked vehicles, with no movement, smoke or dust.
- A-10 pilots did better because CAS was their main mission. They flew over the expected battle area routinely, had
  three radios against the F-4's one, and had better maps.
- [bush_a10_simhq] (unverified, source not re-read on 15 Nov 2026; the speeds and the 3 nm pop-up stay medium
  confidence at best until it is.)

**opp-nato-07 (19 September 1979). GAU-8 against a simulated tank company (Array 20).**
- The setup: two A-10s, ten passes, one primary tank each pass. The tanks were stationary, combat-loaded M-47s,
  simulating a Soviet advance-guard company.
- Each pass: open fire at a mean dive angle of 3.3 degrees (the executive summary rounds it to three) and a mean
  slant range of 3,658 ft (1,115 m). Bursts averaged 2.01 s and 135 rounds.
- Hits: 243 of 1,349 rounds hit (192 direct, 0.14; 51 ground ricochets, 0.18 in all), and 42 perforated the armour.
- Results: 3 catastrophic kills and 7 tanks immobilised at once (the abstract's count, which includes the K-kills),
  with an 8th immobilised after about 500 m more movement. The report is not fully consistent: its summary table
  lists 3 K-kills, 2 M+F-kills, 3 M-kills and 1 light-damage tank. Only one tank kept significant combat capability.
- There was no air defence, and the targets were known and stationary.
- [cdat_array20_1980, summary and Section 1]

**opp-nato-08 (1965-1972). The road corridor and validation (Vietnam and Laos).**
- In Tiger Hound (from December 1965), armed reconnaissance was unlimited within 200 yards of all motorable roads.
  Beyond that the rules stayed restrictive [checo_interdiction_1969, Tiger Hound section].
- In Steel Tiger, 1966-67, a target of opportunity more than 200 yards off a road could be struck only if one of these
  held:
  - it was a validated target;
  - AA guns were firing on the aircraft;
  - the air attaché approved;
  - it was a military boat under FAC control.
  [checo_interdiction_1969, ch. IV, p. 56]
- Also forbidden: night attacks on fixed targets, and attacks on camp fires and civilian houses
  [checo_interdiction_1969, ch. IV, p. 56].
- By 1969-72 the corridor was 200 m. Strikes further off needed validation and a FAC, and none could be made within
  500 m of an active village unless fire came from it [checo_roe_1973, ch. III].

**opp-nato-09 (1964-1972). Self-defence and protective reaction, but nothing else.**
- 1964: escorts and reconnaissance aircraft fired on at low level could retaliate "on the first pass or on subsequent
  circling passes". Suppressing known guns needed JCS authorisation for each mission. Night interdiction missions had
  no alternate targets. [checo_roe_1966, pp. 32, 43]
- 1970-72 over North Vietnam: SAM and AAA sites could be struck right after they fired, or when "activated against"
  friendly forces. Attacks on "other unfriendly forces or installations encountered were prohibited"
  [checo_roe_1973, ch. V].

**opp-nato-10 (1967-1968). The Fast FAC's visual reconnaissance and the strike pilot's blindness.**
- Strike pilots on armed reconnaissance were "unable to see and/or identify targets". Misty training took 20 missions,
  and a pilot was highly qualified after about 40.
- A flight lead with over 20 sorties in Route Package One saw a gun site and a truck for the first time on a Misty
  orientation ride, though he had "struck" both many times.
- [checo_rp1_1969, section on night armed reconnaissance, quoting a 7AF report]
- Misty sorties lasted four hours or more. On 8 November 1967 the crew looked over the trail from about 4,500 ft (the
  article gives the height but not clearly as the first look's), flew on rather than circle, and came back once, low
  and fast. They marked with two 2.75-inch white phosphorus rockets. They reported the find to a command and control
  aircraft and asked it to divert fighters; four F-4s came, and the FAC also briefed them directly. Their F-100 was
  hit, probably by a 37 mm gun, and the crew ejected. [afmag_2006_misty]

**opp-nato-11 (June-July 1968). Day against night.**
- In Route Package One, 20% of trucks sighted were seen by day, and they gave 60% of the trucks destroyed or damaged.
  The 80% seen at night gave 40%.
- FACs improved daylight strike effectiveness by about 30%. FACs assisted 83% of day strikes and 26% of night strikes.
- [checo_rp1_1969, section on night armed reconnaissance]

**opp-nato-12 (1969-1971). Pilots' damage reports cannot be trusted.**
- First-light RF-4C photo coverage over six weeks in 1969-70 confirmed 7 of 103 reported truck kills. Inaccurate
  coordinates and delays were possible reasons, and the totals were not revised down [nalty_2005, p. 89].
- Commando Hunt III claimed 6,428 vehicles destroyed and 3,604 damaged. It counted only southbound trucks as loaded,
  at one ton each [nalty_2005, pp. 128-129].
- This is hindsight written in 2005, about period records.

**opp-nato-13 (studies 1962-1974). Finding a target of opportunity is a different, harder problem.**
- A 1974 survey for the Office of Naval Research names five visual acquisition problems. Attack on targets of
  opportunity is separate from attack on briefed targets, and ranges for one should not be expected for another.
- Targets "expected" where the briefing put them were found with a higher probability and sooner.
- In one simulation (Schohan 1965), pilots found 61% of targets and observers with fewer tasks found 73%.
- In a 1962 study of contour flight below 100 ft with minimal briefing, the longest tank detection was 1,400 yd
  (1,280 m) and the median 300 yd (274 m). The survey notes the study gave no masking data and "its results are
  often discounted".
- [jones_atg_1974, ch. 5 and sections on task loading and helicopters]

**opp-nato-14 (October 1973). Israeli BAI on the Golan.**
- Syrian columns moved on the roads three or four vehicles abreast. After the breakthrough, "no clear distinction
  between friendly and enemy positions" made close support less workable than attacks deeper.
- Pairs of fighters attacked armour late on the first day, in a hurry to use the light. Convoys were attacked at night
  under flares, but day was preferred.
- 80 IAF aircraft were lost over the Golan: about 50% to AAA, 10% to air combat and an estimated 40% (about 30) to the
  SA-6. A Dutch UN observer estimated that three of five aircraft over the battlefield were hit.
- [entwistle_1988, pp. 17, 28-29 and the chapter on the threat]
- Losses in the first three days: 50 aircraft in about 1,220 sorties, 4% [jvl_1973_losses].

**opp-nato-15 (May-June 1982). Harrier GR3s in the Falklands.**
- Many GR3 sorties "were risky armed reconnaissance" until tactical air control, communications and laser-guided bombs
  came together near the end.
- "Two of the three GR3s shot down were re-attacking." This is quoted from the official historian through a museum
  lecture summary.
- Two GR3s a day were detached to a forward strip for quick reaction. Weapons: cluster bombs, 2-inch rockets, 1,000 lb
  bombs, and finally laser-guided bombs.
- On 23 May a four-aircraft attack on Dunnose Head airstrip found no aircraft.
- [rafm_falklands]

**opp-nato-16 (1973-74). Pre-planned and pop-up targets under a FAC, at medium altitude.**
- Bush's F-4 day missions over Cambodia took both pre-planned targets and targets of opportunity ("pop-up targets").
  The ABCCC passed the flights to OV-10 or Cambodian ground FACs.
- The minimum was 4,500 ft AGL, above small arms. Bombs were released at 6,000-8,000 ft from a 30 degree dive, with no
  low strafing. The usual load was six Mk-82s or CBUs.
- [bush_a10_simhq] (unverified, source not re-read on 15 Nov 2026. The 4,500 ft floor agrees with the 7th AF
  minimum already registered from checo_1970_tactics.)

### Warsaw Pact

**opp-wp-01 (1970, Soviet Ministry of Defence journal; translated 1973). Three methods, with "on request" the
principal.**
- "In carrying out combat missions to destroy enemy naval vessels and landings", front aviation may use "strikes
  against previously assigned targets, strikes upon request, and independent search and destruction of targets
  ('hunting')". The principal method for fighters, fighter-bombers and bombers "must be strikes upon request", as
  best suited to a fast-changing situation. (Context checked 15 Nov 2026: the rule is stated for the anti-landing
  battle on a maritime front, not for front aviation in general.)
- Fighters hunted low-flying aircraft up to 300 m in zones designated for each fighter regiment, since front radars
  might not detect them in time.
- Conventional attacks on landing ships and forces used cannon and rockets from a gentle 10-25 degree dive.
- 20-30% of the planned reconnaissance flights were held back for requests from the air army's command post, from
  readiness No. 1 and No. 2.
- [mt_1970_bukhantsev; Odessa Military District exercises]

**opp-wp-02 (1984, NATO's view). Free hunting only once the air is won, and every crew reports.**
- "Aircraft crews on any mission are expected to immediately report observed enemy activity." Perishable target data
  went by radio to ground command posts. Processing a reconnaissance mission's data took 2-8 hours.
- In exercises the Soviets showed "some reservations" about armed reconnaissance ("free hunting") until air superiority
  was established. Such missions were meant to strike at once on what the reconnaissance found. The flight was one
  reconnaissance aircraft and two to four attack aircraft.
- [fm_100_2_1_1984, ch. 12, p. 12-6]

**opp-wp-03 (1984, NATO's view). Readiness for on-call strikes.**

| Readiness | Time to take-off | Held for |
|---|---|---|
| One | 3-5 min | 1-2 h |
| Two | 15 min | 2-4 h |
| Three | 1-2 h | 2-4 days |

- Readiness one and two answer on-call missions. Aircraft and helicopters for on-call missions may hold airborne or
  wait at forward airfields.
- [fm_100_2_1_1984, p. 12-7]

**opp-wp-04 (1984, NATO's view). Target classes and how each is attacked.**
- A line target, a march column usually 1 km or longer, is attacked by a single aircraft or small group along its
  long axis. Helicopters attack a column from the flank.
- A multiple target, 10-20 targets in 1-1.5 km, is attacked by 2-8 aircraft.
- A single target is attacked by one aircraft at low level or in a dive, or by one helicopter with ATGM or rockets.
- [fm_100_2_1_1984, p. 12-7, chart]

**opp-wp-05 (1984, NATO's view). Time to identify, and the re-attack.**
- After a low, fast approach the pilot "has only 3 to 6 seconds to identify his target" and should leave the target
  area within 10 seconds.
- The approach is at 50-100 m, with radio "reduced to a minimum or prohibited entirely".
- If more than one pass is needed, flights come from different directions, or out of bright sunlight.
- [fm_100_2_1_1984, p. 12-8]

**opp-wp-06 (1984, NATO's view). Helicopters for time-sensitive strikes near own troops.**
- Helicopters were preferred for immediate strikes close to friendly forces, but only under the air defence umbrella.
- Fixed-wing aircraft struck previously reconnoitred targets.
- [fm_100_2_1_1984, p. 12-8]

**opp-wp-07 (Afghanistan, 1980s). Night "free search and destroy", cleared by the ground.**
- More experienced crews flew in pairs or sections at arbitrary heights. They found vehicles moving against the
  curfew by their headlights and reported the coordinates to the nearest combat outpost.
- The outpost commander had his region's convoy movement plan, and the helicopters struck only once he cleared the
  strike.
- [rgs_afghan_2002, pp. 219-220]

**opp-wp-08 (Afghanistan, 1980s). Inspecting before attacking.**
- Helicopters circled a caravan at 1,500-2,000 m and judged its size and nature.
- If the escort was aggressive or used weapons, the caravan was destroyed. Otherwise lift helicopters landed search
  groups ahead of and behind it while gunships circled.
- To stop a caravan, the helicopters fired the door guns, then landed 200-300 m ahead.
- Time on station was usually not more than 30-35 minutes.
- [rgs_afghan_2002, pp. 218-219, 231-232]

**opp-wp-09 (Soviet teaching text, WWII examples; publication year not confirmed). The hunter's craft.**
- Free hunting by single aircraft and small groups was used widely in 1943-44, mainly on railways and roads.
- Volunteers were taken from the best-trained crews. They went up to about 150 km deep and chose their own route,
  height, attack and weapons.
- Crews circling to attack often found several targets and made 2-3 attacks a circle. When unsure, they made 1-2 dummy
  passes to find the target first.
- Columns were hit at the head or in a defile. A pair photographed the results.
- Strikes flown straight onto reconnaissance data were judged the most effective method.
- [skomorokhov_1985, ch. 2]
- This is background for the Soviet idea of the hunt, not 1980s tactics.

**opp-wp-10 (registered; an undated, post-Soviet teaching aid describing Soviet-lineage methods, not a 1980s text).
Free search by up to a flight** when the target's location is not known precisely [mai_tactics_vvs, §3.3.3]. Reused
from `ENEMY_AIR_RESEARCH_2026-10.md`; not re-read here. docs/SOURCES.md registers it as undated and post-Soviet, so
it supports the 1985 Pact only where period texts (Bukhantsev 1970, FM 100-2-1 1984) agree with it.

### Neutral

**opp-neu-01 (early 1980s, Swedish reconnaissance). Exercise runs against Pact-style air defence.**
- On "ABBA" missions in the large exercises the flight left its base on the A side. At a point, IK (the Swedish IFF)
  was switched off and the radio changed to the B side, and the run to the target began. After the target it
  switched back to the A side for the return. (Corrected 15 Nov 2026: A and B are the exercise's two sides, so this
  is an exercise convention, the aircraft changing sides to play the attacker for the run. It is not evidence of a
  Swedish wartime emission rule.)
- In the FOCUS exercises from the early 1980s, reconnaissance flew low against army air defence playing Pact tactics,
  to approach the target area "as unseen as possible".
- J 35 fighters at 1,500 m found the low flyers over the sea by their wake.
- [ericsson_f17_recce]

**opp-neu-02 (moved to section 5 as post-period, 15 Nov 2026).** The source places the staff's 20-60 s remark after
the JAS 39's delivery was delayed and with weapons such as the Rb 15F already developed: about 1989 or later, not a
1985 practice.

**opp-neu-03.** No period source was found on Austrian or Swedish attack-pair rules for targets of opportunity (section
7).

### Rebels

**opp-reb-01 (1969, Biafra; one veteran's account through a website; low confidence). Planned single passes.**
- Lynn Garrison's MFI-9 strikes were planned on known targets. He was advised to attack infrastructure, not troops.
- The aircraft flew below 300 ft, made one run with rockets and left.
- Others were "caught up in the adrenaline game of shooting rockets": two fired early at empty ground, and "2 MFI-9s
  with pilots were lost to ground fire" in such shoot-ups.
- [garrison_biafra]

### Our side (what our columns and the battery meet)

**opp-our-01 (1984, NATO's view of Soviet practice). A march column under air attack.**
- On the commander's signal the column's air defence opens fire. The column speeds up and opens to up to 100 m
  between vehicles.
- Against a large group it may disperse or take cover off the road.
- [fm_100_2_1_1984, ch. 5, the march section, about p. 5-10] (reference corrected 15 Nov 2026: the passage is in the
  chapter on offensive tactics, not ch. 11, Air Defense)

**opp-our-02 (1973, 1982). Lessons the defended ground can read.**
- Low-level AAA took over half the Israeli losses on the Golan [entwistle_1988].
- Two of three Harriers lost were re-attacking [rafm_falklands].
- A battery that sees a flight turn back toward it is seeing the riskiest moment of the attacker's sortie.

### General (synthesis for the design)

**opp-gen-01. The flight's choice, in priority order.** Section 3 gives the numbers. This is a synthesis of the
findings above. The order of the steps is a design choice, not a sourced rule.

1. **Seen?** Draw a find chance for the aircraft type, crew skill, light and target state.
   - Parked and camouflaged targets are seldom seen by fast jets (opp-nato-06).
   - Moving targets with dust, smoke or lights are seen far more often (opp-nato-06, opp-wp-07).
   - Two-seat crews and slow attack aircraft see more (opp-nato-13).
2. **Identified?** The attacker has 3-6 s at low level (opp-wp-05). If friendly troops are near, or the forces are
   intermingled, the flight does not attack: it reports, or makes a recce pass (opp-nato-14, opp-wp-07).
3. **Authorised?** Check the mission and the area.
   - Armed reconnaissance, a pre-assigned area cleared of friendly forces (opp-nato-03, unverified), beyond the FSCL
     or a free fire area: attack (opp-nato-01..03).
   - Inside the road corridor of a restrictive ROE: attack. Outside it: report, unless fired upon (opp-nato-08).
   - A strike or escort on another task: report only, and hit back only at whatever fires (opp-nato-09).
   - Pact: report and request, and hunt only where told and once the air is won (opp-wp-01, 02).
   - Night helicopter hunts: only after a ground post clears the strike (opp-wp-07).
4. **Able?** The weapons left decide the attack.
   - Ordnance kept: an impromptu attack.
   - Only the gun or rockets left after the primary: a gun or rocket run (opp-nato-05, 07; opp-wp-01).
   - Low fuel or a strict time slot: report only.
5. **Worth the threat?** If guns or SAMs are seen or firing, make one pass and leave, or none at all.
   - A re-attack comes from a new heading after a separation, and carries a higher loss chance (opp-wp-05,
     opp-nato-10, 15).
   - The Pact and NATO superior forces may come back. Neutrals and rebels rarely do.
6. **Report in every case.** The report carries the pilot's error of position and damage
   (opp-wp-02, opp-nato-12).

## 3. Numbers usable as data

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| Free armed recce corridor either side of motorable roads | 200 yards (183 m) | NATO (US) | 1965-67 | checo_interdiction_1969, Tiger Hound section and p. 56 | high |
| Same corridor, later rules | 200 m | NATO (US) | 1969-72 | checo_roe_1973, ch. III | high |
| No strike within this distance of an active village unless fired from | 500 m | NATO (US) | 1969-72 | checo_roe_1973, ch. III | high |
| A-10 squadron's pre-assigned area ("kill box" in the later interview) | about 5 x 10 nm (9 x 18 km) | NATO (USAF A-10) | late 1970s-1984 | bush_a10_simhq | low (unverified: not re-read 15 Nov 2026) |
| F-4 run-in speed for CAS | 450-500 KIAS | NATO | early 1980s | bush_a10_simhq | low (unverified: not re-read 15 Nov 2026) |
| Range where the fast mover begins its pop-up | about 3 nm (5.6 km) | NATO | early 1980s | bush_a10_simhq | low (unverified: not re-read 15 Nov 2026) |
| Time to identify the target in a low, fast attack | 3-6 s | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-8 | medium |
| Time to leave the target area after attack | within 10 s | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-8 | medium |
| Approach height over enemy territory | 50-100 m | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-8 | medium |
| Gentle dive for cannon and rocket attack (stated for attacks on landing ships and forces) | 10-25 degrees | Pact | 1970 | mt_1970_bukhantsev | high |
| Readiness one: time to take-off; held for | 3-5 min; 1-2 h | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-7 | medium |
| Readiness two: time to take-off; held for | 15 min; 2-4 h | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-7 | medium |
| Readiness three: time to take-off; held for | 1-2 h; 2-4 days | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-7 | medium |
| Soviet armed recce flight | 1 recce aircraft + 2-4 attack aircraft | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-6 | medium |
| Share of recce flights kept for requests | 20-30 % | Pact | 1970 | mt_1970_bukhantsev | high |
| Processing time of air recce data | 2-8 h | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-6 | medium |
| Line target (march column) length | usually 1 km or longer | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-7 | medium |
| Multiple target: count and spread; attackers | 10-20 in 1-1.5 km; 2-8 aircraft | Pact (NATO's view) | 1984 | fm_100_2_1_1984, p. 12-7 | medium |
| Column spacing on air attack | up to 100 m | our side (Soviet practice, NATO's view) | 1984 | fm_100_2_1_1984, ch. 5 (the march) | medium |
| Caravan inspection: helicopter circling height | 1,500-2,000 m | Pact | 1980s | rgs_afghan_2002, p. 219 | high |
| Inspection landing distance ahead of a caravan | 200-300 m | Pact | 1980s | rgs_afghan_2002, p. 231 | high |
| Helicopter time on station over an inspection | not more than 30-35 min | Pact | 1980s | rgs_afghan_2002, p. 232 | high |
| GAU-8 open-fire slant range, low-angle pass | mean 3,658 ft (1,115 m) | NATO | 1979 | cdat_array20_1980 | high |
| GAU-8 dive angle at open fire | mean 3.3 degrees | NATO | 1979 | cdat_array20_1980 | high |
| GAU-8 burst | mean 2.01 s, 135 rounds | NATO | 1979 | cdat_array20_1980 | high |
| GAU-8 hits / rounds | 0.14 direct; 0.18 with ricochets | NATO | 1979 | cdat_array20_1980 | high |
| Tanks immobilised per GAU-8 pass, stationary and undefended | 0.8 (8 in 10 passes; 3 catastrophic) | NATO | 1979 | cdat_array20_1980 | high (for the test), low for war |
| Missions to train a Fast FAC; to be highly qualified | 20; about 40 | NATO (US) | 1968 | checo_rp1_1969, section on night armed reconnaissance | high |
| Share of trucks sighted by day; share of kills from them | 20 %; 60 % | NATO (US) | Jun-Jul 1968 | checo_rp1_1969, section on night armed reconnaissance | high |
| FAC improvement of day strike effectiveness | about 30 % | NATO (US) | 1968 | checo_rp1_1969, section on night armed reconnaissance | medium |
| Day / night strikes assisted by a FAC | 83 % / 26 % | NATO (US) | Jul 1968 | checo_rp1_1969, section on night armed reconnaissance | high |
| Reported truck kills confirmed by photo | 7 of 103 (7 %) | NATO (US) | 1969-70 | nalty_2005, p. 89 | high |
| Targets found: pilot / observer with lighter load | 61 % / 73 % | general | 1965 study | jones_atg_1974 | medium |
| Contour flight below 100 ft, minimal brief: tank detection max / median | 1,280 m / 274 m | general | 1962 study | jones_atg_1974 | low (the survey says the study is often discounted) |
| IAF aircraft lost on the Golan; share to AAA; to SA-6 (estimate) | 80; about 50 %; about 40 % | NATO analogue | Oct 1973 | entwistle_1988 | medium |
| IAF losses, first three days | 50 in about 1,220 sorties (4 %) | NATO analogue | Oct 1973 | jvl_1973_losses | medium |
| GR3s shot down that were re-attacking | 2 of 3 | NATO (UK) | May-Jun 1982 | rafm_falklands | medium |
| Medium-altitude floor under a FAC | 4,500 ft AGL; release 6,000-8,000 ft, 30 degree dive | NATO (US) | 1973-74 | bush_a10_simhq (the 4,500 ft floor also in checo_1970_tactics) | low-medium (unverified: not re-read 15 Nov 2026) |
| Hunter penetration depth | up to about 150 km | Pact (WWII teaching) | 1943-44 | skomorokhov_1985 | low for 1985 |
| Attacks per circle by circling attackers | 2-3 | Pact (WWII teaching) | 1943-44 | skomorokhov_1985 | low for 1985 |
| Rebel strike height | below 300 ft | rebels | 1969 | garrison_biafra | low |

**Not sourced. These are game values to be set in `data/gameplay/air_war.json` and labelled as such:**
- the probability of each reaction;
- the share of flights that keep ordnance for an alternate target;
- the added loss chance on a re-attack;
- the error in a pilot's reported position.

## 4. Disagreements and which to use

1. **How much the Pact hunts.**
   - Bukhantsev (1970) lists hunting as a method, with strikes on request the principal one.
   - The MAI teaching aid gives free search by up to a flight when a target's place is not known.
   - FM 100-2-1 (1984) says the Soviets were reluctant until they held air superiority.
   - **Use:** report and request is the Pact's default. Hunting is flown in zones given by the plan and once the
     enemy's air picture shows air superiority. A flight on another task attacks a target of opportunity only with its
     leftover weapons. FM 100-2-1 is NATO's view, and Bukhantsev is a Soviet text, so Bukhantsev wins where they
     differ. On the reluctance they agree.
2. **The size of a hunting flight.**
   - FM 100-2-1: one reconnaissance aircraft and 2-4 attack aircraft.
   - MAI: up to a flight.
   - Afghanistan: helicopter pairs or sections.
   - Skomorokhov (WWII): single aircraft and pairs.
   - **Use:** keep the existing pairs. Add an optional Pact flight of four led by a reconnaissance aircraft for a
     zone hunt, where FM 100-2-1 and MAI agree.
3. **The A-10's gun against reality.**
   - Array 20 gives 0.8 tanks immobilised a pass, but against stationary, known, undefended tanks.
   - Bush and the Misty report say finding the target is the real limit.
   - The Golan shows the cost under AAA.
   - **Use:** apply the per-pass effect only once a target is found and identified. Draw finding and identifying
     separately, and let the defences shorten the pass and lower the effect.
4. **One pass or several.**
   - FM 100-2-1 allows more passes from new directions.
   - Skomorokhov's WWII hunters made 4-8 passes on trains.
   - The Vietnam rules allowed circling passes for retaliation.
   - Misty came back once, and the Falklands losses came on re-attacks.
   - **Use:** where defences are seen, one pass by default. A second pass from a new heading is allowed for NATO and
     the Pact under the conditions in section 2 (opp-gen-01), with a higher loss chance. Neutrals and rebels rarely
     re-attack.
5. **The A-10's European concept in 1985.**
   - Kill boxes ran from the late 1970s to about 1984.
   - Airborne holding with pop-up tasking was demonstrated in spring 1986.
   - **Use:** pre-assigned squadron areas for 1985 (in game data name them by area, not "kill box", a later term).
     Airborne holding is an option a NATO officer may adopt (a quirk), not the default. Both rest on one interview
     that could not be re-read on 15 Nov 2026 (unverified).
6. **Day against night.**
   - The 1968 figures make night hunting poor for fixed-wing aircraft.
   - Afghanistan's helicopter night hunts were effective, but only against a curfew with lights showing.
   - **Use:** keep the night find chance low (0.15) for fixed-wing. Raise it only for a target showing lights or moving
     on a road under a known ban.
7. **Damage claims.**
   - The official totals and the photo checks disagree by an order of magnitude (nalty_2005).
   - **Use:** the enemy's journal records the claim. Its intelligence picture discounts claims only when photo control
     is flown. The truth stays in the debrief ledger.

## 5. Post-period items excluded, with dates

- The A-10 squadron's NATO evaluation of airborne holding and pop-up tasking, spring 1986 (bush_a10_simhq). Used only as
  an option, see section 4.
- The A-10 as an airborne FAC attacking the targets it finds (OA-10 role, from 1987), and its computer-aided weapons
  delivery and AIM-9s (later than Bush's service, which ended in 1988).
- The Desert Storm "killer scout" F-16s, medium-altitude A-10 work and Scud hunting (1991). Hindsight only.
- FASA, the Swedish computer-based planning and evaluation system (delivered 1992), and the AJS 37 modification
  (1991-94) (ericsson_f17_recce).
- The Swedish staff's remark that attack and reconnaissance missions differed by only about 20-60 s in the target area,
  tried on a weapons course with the Rb 04E on the SH 37 (formerly opp-neu-02). The source places it after the JAS 39's
  delay, with the Rb 15F (in service 1989) already developed: about 1989 or later (ericsson_f17_recce).
- The term "kill box" for a pre-assigned attack area: found here only in a later interview about the late 1970s and
  early 1980s (bush_a10_simhq); it is not in FM 101-5-1 (1980) or FM 100-2-1 (1984), and its use as a joint term is
  believed to date from 1991 and after (not sourced here). The 1985 arrangement is kept; the word is not.
- Afghanistan helicopter tactics after the Stinger appeared (from late 1986). Only the pre-1986 rules are used here, and
  rgs_afghan_2002 does not date each rule.
- The free hunt in the Chechen wars (1994-96, 1999-2000), mentioned in Russian reference pages.
- Croatia's improvised air arm (1991; already registered as croatia_1991_migflug).
- LANTIRN night attack (1987 onward), NVG-equipped night attack by fixed-wing aircraft, and JTIDS and AMRAAM (late
  1980s and 1990s).
- Kosovo 1999 kill boxes and the FAC-A procedures in the 1990s joint publications (JP 3-09.3 and successors).

## 6. Sources

| Id | Tier | Full citation | URL | Read | Whose view |
|---|---|---|---|---|---|
| fm_101_5_1_1980 | 1 | HQ Department of the Army, FM 101-5-1, *Operational Terms and Graphics*, 31 March 1980 | https://archive.org/details/operationalterms00unse | entries read in full (armed reconnaissance, target of opportunity, BAI, FSCL, FFA, immediate mission request, on-call target) | US Army, own doctrine |
| fm_100_2_1_1984 | 1 (registered; for what NATO believed) | US Army FM 100-2-1, *The Soviet Army: Operations and Tactics*, 16 July 1984, ch. 5 (the march) and ch. 12 | https://archive.org/details/fm100-2-1 | chapter 12 (air support) and the march section read in full; re-read 15 Nov 2026 | NATO's view of the Soviet Army |
| mt_1970_bukhantsev | 1 | Gen.-Maj. of Aviation A. Bukhantsev, «Tactical Air Operations in Conventional Warfare on a Maritime Front», *Military Thought* (secret collection) No. 3 (91), 1970; CIA translation, memorandum of 22 June 1973, CIA-RDP10-00105R000100190001-5 | https://www.cia.gov/readingroom/document/cia-rdp10-00105r000100190001-5 (read through the archive.org mirror) | full | Soviet MoD text through US intelligence |
| mai_tactics_vvs | 1 (registered; status per docs/SOURCES.md) | MAI military institute teaching aid «Тактика военно-воздушных сил», §3.3.3 | (see docs/SOURCES.md) | not re-read here; used through ENEMY_AIR_RESEARCH_2026-10.md | Russian teaching text, undated and post-Soviet (Soviet-lineage methods) |
| skomorokhov_1985 | 2 (a Soviet text of WWII examples) | N. M. Skomorokhov, V. N. Chernetsky, «Тактика в боевых примерах: Авиационный полк», Moscow: Voenizdat, 1985; ch. 2 (year confirmed on militera's index page, 15 Nov 2026) | http://militera.lib.ru/science/skomorohov_chernetsky/02.html | chapter 2 read in full | Soviet teaching, WWII examples |
| rgs_afghan_2002 | 1 | Russian General Staff, *The Soviet-Afghan War: How a Superpower Fought and Lost*, transl. and ed. L. W. Grau and M. A. Gress, University Press of Kansas, 2002 | https://almanaquemilitar.com.br/wp-content/uploads/2021/10/The-Soviet-Afghan-War.pdf (copy used) | army aviation and reconnaissance sections read (pp. ~217-232) | Soviet General Staff, translated; written after the period about it |
| checo_roe_1966 | 1 | L. E. Paterson, Project CHECO, *Evolution of the Rules of Engagement for Southeast Asia, 1960-1965*, HQ PACAF, 30 September 1966, DTIC ADA486920 | https://archive.org/details/DTIC_ADA486920 | first two-thirds read | US study of its own rules |
| checo_roe_1973 | 1 | P. W. Elder, P. J. Melly, Project CHECO, *Rules of Engagement, November 1969 - September 1972*, HQ PACAF, 1 March 1973, DTIC ADA486569 | https://archive.org/details/DTIC_ADA486569 | most read (the end of the LINEBACKER section through its appendices) | US study of its own rules |
| checo_rp1_1969 | 1 | Project CHECO, *Interdiction in Route Package One, 1968*, HQ PACAF, 30 June 1969, DTIC ADA486744 | https://archive.org/details/DTIC_ADA486744 | relevant parts read | US study of its own campaign |
| checo_interdiction_1969 | 1 | Project CHECO, *Interdiction in SEASIA, November 1966 - October 1968*, HQ PACAF, 30 June 1969, DTIC ADA486781 | https://archive.org/details/DTIC_ADA486781 | relevant parts read (chapters on Laos and the ROE) | US study of its own campaign |
| nalty_2005 | 1 (USAF history office; hindsight) | B. C. Nalty, *The War Against Trucks: Aerial Interdiction in Southern Laos 1968-1972*, Air Force History and Museums Program, 2005, DTIC ADA476352 | https://archive.org/details/DTIC_ADA476352 | relevant parts read | USAF official history, written after the period |
| cdat_array20_1980 | 1 | R. H. S. Stolfi, R. R. McEachin, *Combat Damage Assessment Team: A-10/GAU-8 Low Angle Firings versus Simulated Soviet Tank Company (Array 20), 19 September 1979*, Naval Postgraduate School NPS-56-80-006, May 1980 | https://archive.org/details/combatdamageasse80006stol | full | US test report |
| jones_atg_1974 | 1 | D. B. Jones, M. Freitag, S. C. Collyer, *Air-to-Ground Target Acquisition Source Book: A Review of the Literature*, Martin Marietta for the Office of Naval Research, 30 September 1974, DTIC ADA015079 | https://archive.org/details/DTIC_ADA015079 | relevant sections read (OCR poor in tables) | US research survey |
| bush_a10_simhq | 2 | Lt Col Andy Bush (USAF, ret.), interview by J. Sponauer, «The A-10 in the 1980s», SimHQ (archived) | https://web.archive.org/web/2012id_/http://www.simhq.com/_air/air_048a.html | full at first reading; could not be re-read on 15 Nov 2026 (the Wayback Machine refused the check), so its items are marked unverified | operator account (F-4 1973-74; A-10 from 1982; the report gives his service's end as 1986 here and 1988 in section 5: unresolved) |
| afmag_2006_misty | 4 (operator accounts through press) | R. J. Newman, D. Shepperd, «A Day in the Life of the Misty FACs», *Air Force Magazine*, June 2006 | https://www.airandspaceforces.com/article/0606misty/ | full | operators' recollections through a magazine |
| entwistle_1988 | 4 (a staff college thesis on secondary sources) | Maj. W. Entwistle, *Lessons from Israeli Battlefield Air Interdiction during the Battle for Golan, October 1973*, US Army CGSC, 1988, DTIC ADA199060 | https://archive.org/details/DTIC_ADA199060 | relevant parts read | US officer's analysis of Israeli practice |
| jvl_1973_losses | 4 (registered) | Jewish Virtual Library, the IAF in the Yom Kippur War | (see docs/SOURCES.md) | not re-read | secondary |
| rafm_falklands | 4 | RAF Museum, lunchtime lecture summary «Suddenly like cavalry: British close air support in the Falklands campaign» (J. Alexander, 2025, quoting the official historian), and blog «Harriers over the Falklands» (2022) | https://rafmuseum.org.uk/london/whats-going-on/lunchtime-lecture-suddenly-like-cavalry-british-close-air-support-in-the-falklands-campaign ; https://www.rafmuseum.org.uk/blog/harriers-over-falklands/ | both pages read in full; the official history itself not read | museum summaries of the official history and pilots' words |
| ericsson_f17_recce | 2 | P.-O. Ericsson, «Flygspaning. Teknik- och taktikutveckling» (reconnaissance at F 17, part 3), F 11 museum | https://www.f11museum.se/spaning/s37/pdf/flygspaning_teknik_o_taktik_f_17.pdf | full | Swedish operator's account |
| garrison_biafra | 4 (a veteran's account through a website; low confidence) | E. Lionel, Lynn Garrison's account of MFI-9 strikes in the Biafran war, military.africa | https://military.africa/?p=11 (address as fetched) | full | a mercenary pilot's recollection |
| ijaf_jvl_1982 | 4 | Jewish Virtual Library, «Israel Air Force: In Operation Peace for the Galilee» (after the IDF website) | https://www.jewishvirtuallibrary.org/jsource/Society_&_Culture/82iaf.html | full | secondary; IAF's own summary |

The last row (ijaf_jvl_1982) is not cited in the findings. It says only that 1982 was the first wide use of Cobra and
Defender helicopters, with no detail on targets of opportunity. It is listed so that the search is on record.

## 7. Open items and documents to acquire

- **The in-flight report formats.** The NATO INFLTREP and ATP-27(B) texts, and the Soviet in-flight report rules
  (доклад с воздуха), were not found. They set what a pilot says, and how fast.
- **NATO and US manuals.**
  - FM 90-21, *Joint Air Attack Team Operations* (1983).
  - TACP 50-20 (1979) and the JAAT and BAI manuals of 1983-85.
  - The CALL November 1985 note on attacking columns (globalsecurity refused access).
- **USAFE and RAF Germany attack rules.** Their armed reconnaissance rules in Central Europe, 1980-85, were not
  found. The only European evidence here is one A-10 pilot's account.
- **The US Navy's road reconnaissance tactics for two-aircraft sections in Vietnam.** The USNI 1972 article was
  refused, and no primary source was found.
- **Soviet sources.**
  - Soviet rules for photo control of strike results (фотоконтроль, ФКП) and how far claims were trusted: not found.
  - Markovsky's books on Su-25 and Mi-24 hunting in Afghanistan («Жаркое небо Афганистана»): not reachable.
  - The redov.ru page on attack aircraft failed (TLS). The MAI page was not re-read: the request was refused because it
    needed an insecure connection.
  - Soviet gun pods (SPPU-22) and S-8 rockets: their service dates and how they were used against columns are not
    sourced here.
- **Israel, 1982.** Helicopter attacks on Syrian armour in Lebanon: only a press claim of "dozens" of tanks was seen,
  not registered. A 1987 study was suggested by search results but not found.
- **Southern Africa.** SAAF Impala attacks on convoys in Angola in 1987: not found. Use `saaf_toss` and
  `oosthuizen_2014` for the attack profiles meanwhile.
- **Neutrals.** Swedish attack-pair rules and Austrian Saab 105 attack tactics: no public period manual found. A
  Swedish attack tactics handbook of the early 1980s is said to be under a declassification request; this is
  unconfirmed.
- **Books to acquire.** RAND R-3000 (Lambeth, 1984) on Soviet tactical air; Cordesman and Wagner, *The Lessons of
  Modern War*, vol. I (1990), on 1973 and 1982; L. Freedman's *Official History of the Falklands Campaign* (2005), to
  confirm the re-attack losses at first hand.
- **The Skomorokhov book.** Publication year confirmed as 1985 (Voenizdat), 15 Nov 2026.
- **The A-10 interview.** Re-read bush_a10_simhq (or find a period USAFE/81st TFW document) to confirm opp-nato-03 to
  06 and 16 and the dates of Bush's service.

## Verification (15 Nov 2026)

An adversarial check against the cited texts. Full texts were downloaded and searched for FM 100-2-1, FM 101-5-1,
the three CHECO reports, Nalty, Jones, Entwistle, the Array 20 report, the CIA translation of Bukhantsev (archive.org
mirror), the Grau-Gress translation and the F 17 PDF; the Misty, Garrison and RAF Museum pages were fetched. The
archived SimHQ interview (bush_a10_simhq) could not be reached (the Wayback Machine refused the request) and the web
search budget was used up, so nothing resting on it alone could be confirmed.

| Finding | Verdict | Note |
|---|---|---|
| opp-nato-01 | confirmed | FM 101-5-1 (31 Mar 1980) entries read word for word. They are US Army terms, not NATO's: summary corrected. |
| opp-nato-02 | confirmed | FSCL "normally established by corps or independent division"; FFA usually set by a division or higher. Added. |
| opp-nato-03 | unsupported | Source not re-readable. "Kill box" is absent from the 1980 and 1984 manuals: the word moved to section 5, the arrangement marked unverified. |
| opp-nato-04 | unsupported | Source not re-readable; already flagged transitional (1986 evaluation). |
| opp-nato-05 | unsupported | Source not re-readable. |
| opp-nato-06 | unsupported | Source not re-readable; speed and pop-up numbers lowered to low confidence. |
| opp-nato-07 | corrected | Array 20 numbers confirmed (19 Sep 1979, 1,349 rounds, 243 impacts = 192 direct + 51 ricochets, 42 perforations, 3 K-kills, 7 + 1 immobilised). Mean dive angle is 3.3 degrees, not 3; the report's own tables are not fully consistent. |
| opp-nato-08 | confirmed | Tiger Hound 200 yd, Steel Tiger's four conditions, no night fixed targets or camp fires, 200 m and 500 m village rules all found in ADA486781 and ADA486569. |
| opp-nato-09 | confirmed | 1964 "first pass ... circling back" and no night alternates in ADA486920; 1970-72 protective reaction and "other unfriendly forces or installations encountered" in ADA486569. The JCS-per-mission point was only partly seen. |
| opp-nato-10 | corrected | CHECO quote, 20 and about 40 missions, the flight lead's first sighting confirmed. Misty article: 4,500 ft not clearly the first look; the F-100 was hit and the crew ejected; the C2 aircraft was asked to divert fighters. |
| opp-nato-11 | confirmed | 20 %/60 %, 80 %/40 %, about 30 %, 83 %/26 % all in ADA486744. |
| opp-nato-12 | confirmed | 7 of 103 (p. 89), no downward revision, 6,428 and 3,604, one ton southbound only (Nalty 2005; hindsight, flagged). |
| opp-nato-13 | corrected | 61 %/73 % (Schohan 1965) confirmed; the 1,280 m/274 m study was contour flight below 100 ft and "often discounted" (no masking data): confidence lowered. |
| opp-nato-14 | confirmed | Three or four abreast, intermingled forces, 80 lost, 50 % AAA, about 40 % SA-6 as the author's estimate by elimination, Dutch observer's three of five, all in Entwistle 1988 (tier 4, a thesis on press sources). |
| opp-nato-15 | confirmed | "Risky armed reconnaissance" and "two of the three GR3s shot down were re-attacking" (from the official historian via the 2025 lecture page); two GR3s detached daily, Dunnose Head 23 May. "2-inch rockets" not seen in the fetched text. |
| opp-nato-16 | unsupported | Source not re-readable; the 4,500 ft floor agrees with checo_1970_tactics. |
| opp-wp-01 | corrected | All quotes and numbers found (CIA memo 22 Jun 1973, Military Thought 1970). The methods and "principal method" are stated for the battle against enemy ships and landings on a maritime front, and the 10-25 degree dive for that battle: context added, the general Pact default marked as extrapolation. |
| opp-wp-02 | confirmed | "Immediately report", 2 to 8 hours, "some reservations ... ('free hunting')", one recce + 2-4 attack aircraft: FM 100-2-1, p. 12-6 (NATO's view). |
| opp-wp-03 | confirmed | Readiness table on p. 12-7 as given (NATO's view). |
| opp-wp-04 | confirmed | Target classification chart, p. 12-7 (NATO's view). |
| opp-wp-05 | confirmed | 3 to 6 s, 10 s, 50-100 m, radio minimum or prohibited, other directions or bright sunlight, p. 12-8 (NATO's view). |
| opp-wp-06 | confirmed | Helicopters for immediate strikes only to the air defence umbrella; fixed wing on reconnoitred targets, p. 12-8. |
| opp-wp-07 | confirmed | Night free search and destroy, headlights, outpost commander with the convoy plan clears the strike (Grau-Gress, army aviation section). The text gives no year; pre-1986 dating stays an assumption. |
| opp-wp-08 | confirmed | 1,500-2,000 m circling height (p. 219), 200-300 m landing, 30-35 min on station all found. |
| opp-wp-09 | confirmed | Book year confirmed: Voenizdat, 1985 (militera index). WWII content, background only. |
| opp-wp-10 | corrected | mai_tactics_vvs is registered as undated and post-Soviet, not a 1980s teaching aid: date label fixed. |
| opp-neu-01 | corrected | Text found, but A and B are the exercise's two sides: ABBA is the aircraft changing to the attacking side for the run, an exercise convention, not a wartime emission rule. Faction table fixed. |
| opp-neu-02 | post_period | The source puts it after the JAS 39's delay with the Rb 15F (1989) already built: moved to section 5. |
| opp-reb-01 | confirmed | Garrison's words found (below 300 ft, infrastructure, "adrenaline game", two MFI-9s lost); one strike of twelve rockets rather than "one rocket run". Tier 4, low confidence kept. |
| opp-our-01 | corrected | Text confirmed, but it is in FM 100-2-1 ch. 5 (the march), not ch. 11. |
| opp-our-02 | confirmed | Rests on Entwistle and the RAF Museum page, both checked above. |
| opp-gen-01 | corrected | Synthesis kept; "kill box" replaced by pre-assigned areas (unverified), the neutral emission habit dropped. |

Modern-term scan: no AMRAAM, JTIDS, LANTIRN, NVG, SEAD/DEAD or post-1991 brevity is presented as 1985 practice.
The only later item used as 1985 vocabulary was "kill box", now excluded. Tier check: no tier 5 page carries a claim;
FM 100-2-1 and the CIA translation are labelled as NATO's or US intelligence's view; Wikipedia is not cited.
