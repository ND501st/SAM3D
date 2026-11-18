# Air-to-air weapons and sensors in service by 1985: research (November 2026)

Why: the project owner's request of 15 November 2026 for the enemy's air war in its hot phase, including air-to-air
fighting between our aircraft and the enemy's. This note covers the weapons and sensors only: which missiles, guns,
radars, infrared sights, warning receivers and identification sets each side had **in service by 1985**, with dates,
what each could do, and what the period's wars say they achieved. Tactics, communications and pilots' reactions are in
the sibling notes. Read 15 November 2026.

Period rule: the war is 1985. Everything below carries an in-service or doctrine date. Later items are listed in
section 5 and kept out of the 1985 war. Sources from after 1988 are used only for equipment that existed by 1985, and
are flagged.

The web search budget for this session ran out before the Czechoslovak, Polish and East German period journals and the
Soviet *Zarubezhnoye voyennoye obozreniye* could be searched. Section 7 lists what is still to find.

## 1. Summary for SAM3D's design

1. **The 1985 missile is a gameplay probability, not a physics model, and the period gives the calibration.** In
   Vietnam the USAF's own 1967 study found 11.1 per cent hits for the AIM-7 and 18.6 per cent for the AIM-9 per missile
   fired, with 35 and 26 per cent of launches outside the permitted envelope (`checo_1967_a2a`, p. 44). The 1972
   campaign added a reliability problem: 25-30 per cent of AIM-7E-2 and AIM-9E rounds showed guidance failure after
   10-15 captive flights (`checo_1978_linebacker`, p. 30). So a kill per launch is the product of four chances: launched
   inside the envelope (skill, rules), the round working (maintenance), guiding (aspect, sun, ground, flares,
   manoeuvre), and killing (fuze, warhead). Each can be set from period numbers.
2. **Infrared missiles did most of the killing in the period's wars.** A 1986 US Congressional Budget Office paper gives
   about 200 Arab aircraft to heat-seekers against about 7 to Sparrow in 1973, and about 50 Syrian aircraft to
   heat-seekers against 2 to Sparrow over Lebanon in 1982 (`cbo_1986_amraam`, ch. 1 n. 4). In the Falklands the Sea
   Harriers' AIM-9L accounted for 18-19 Argentine aircraft (`harr_1988`, App. E, after Ethell and Price). In Vietnam the
   USAF's 137 MiG kills split 50 AIM-7, 33 AIM-9, 40 guns, 5 AIM-4 and 9 others (`aces_1976`, Table 4). Guns stayed
   important.
3. **Beyond visual range was mostly a NATO capability that NATO's own rules switched off.** NATO's mixed IFF forced a
   visual identification before a radar missile could be fired in central Europe; a 1982 Air Force Institute of
   Technology study found this "reduces the F-15 to the equal of the Soviet first generation aircraft" against a
   capable threat (`afit_1982_vid`, abstract). The F-16 could not carry Sparrow at all (`cbo_1986_amraam`, ch. 2).
   Visual range was about three to four miles (same). SAM3D should model rules of engagement per faction, not just
   missile range.
4. **A semi-active radar missile ties its fighter to the target.** The Sparrow and the Soviet R-23R and R-24R (the
   R-27R is post-period, section 5) home on the launcher's radar, so the fighter must keep illuminating until impact; the mid-1970s AIMVAL trials found
   such fighters "especially vulnerable" (`cbo_1986_amraam`, n. 4). The R-24R could fly 10 s on its own before its
   seeker locked (`mis`, R-24). No 1985 fighter on either side had a launch-and-leave radar missile (AMRAAM came later).
5. **The Pact's fighters depended on ground control.** The MiG-23ML's radar found a fighter at 50-55 km when the pilot
   searched by hand and at 80-85 km when the ground put him onto it, and locked at 30-50 km head-on; Western F-15s and
   F-16s saw the MiG-23 first at 60-70 km (`airwar_ru`, MiG-23ML, a Russian account). The MiG-23ML and MLD carried a
   command datalink from the ground ("Lazur-SML", "Lazur") (`airwar_ru`); that every Pact fighter type had one was not
   checked (unverified). Cut the ground control and the Pact pilot fights at short range.
6. **Aspect is the first gate.** In 1985: all-aspect infrared in NATO (AIM-9L from 1978 production, AIM-9M from 1983)
   and in Soviet frontal units (R-60M close in, adoption date unverified; R-24T; the R-73, adopted 22 June 1984, only
   where MiG-29s were already in Soviet units, a date not established from a usable source); rear-aspect only for the older rounds most
   of our side and the light forces carry (R-3S, R-13M, R-60, AIM-9B/E/J/P, Magic 1). Our side's MiG-21s with R-13M and
   R-60 must get behind; the enemy's F-16 with AIM-9L need not.
7. **Countermeasures and limits are simple rules.** The K-13 (Atoll) could not guide within 30° of the sun and was
   degraded by ground clutter (`checo_1967_a2a`, pp. 19-20, the US view); the AIM-7 was weak in the beam against the
   ground (`afit_1982_vid`, ch. 1); the MiG-23MLD carried 60 flare cartridges (`airwar_ru`, MiG-23MLD); the AIM-9M added
   flare rejection in 1983 (`navy_factfiles`).
8. **Skill decides more than the missile.** In Vietnam the USAF's exchange ratio fell to about 1:1 in 1968 with F-4
   crews flying one air combat sortie a month (`mcallister_1985`, p. 17); in 1972 it went from 1:2 (May) to 3.8:1
   (August-October) with the same aircraft, once a ground and signals-intelligence control centre (Teaball) and IFF
   interrogation (Combat Tree) gave warning and identification (`checo_1978_linebacker`, pp. 52-54). A period US
   compilation found a pilot's chance of being killed in a decisive encounter falling with each one he survived, levelling
   at 0.02 after about ten (`bilikam_1970`, after Weiss).
9. **Soviet design figures are not combat figures.** The R-60's designers gave 0.5 for one missile and 0.7-0.8 for a
   pair, the R-60M 0.9 (`ru_wiki_aam`, `airwar_ru`). The same sources record that about 30 Syrian R-3S failed to guide
   in 1973. Use the design figure as the ceiling a perfect launch could reach, then apply the four chances of point 1.

## 2. Findings by faction

### 2.1 NATO

**aam-nato-01. AIM-9 Sidewinder variants and dates.** AIM-9B in operational use 1956 (`navy_factfiles`). AIM-9J (USAF):
about 10,000 built from 1972, mostly converted AIM-9B/E, with stronger actuators that "doubled the single-plane G
capability" (`parsch_dusrm`, AIM-9). AIM-9L: joint development from 1971, production 1978, all-aspect with an
argon-cooled indium antimonide seeker, laser proximity fuze, 9.4 kg warhead (same). AIM-9M: production 1982
(`parsch_dusrm`), deliveries from 1983 (`navy_factfiles`), with better flare rejection, background discrimination and a
reduced-smoke motor. AIM-9P: USAF export development of the J/N; the P-4 adds an all-aspect seeker (date not found).
[1956-1983]

**aam-nato-02. AIM-9 envelope figures found.** AIM-9B: non-manoeuvring targets only, 900 m to 4.8 km (`parsch_dusrm`).
Parsch's table gives 18 km for later rounds (the column alignment is ambiguous; low confidence). No period launch-zone
diagram for the AIM-9L, J or P was found (the -34 weapon manuals are not public). [1956-1983]

**aam-nato-03. AIM-7 Sparrow variants and dates.** AIM-7E from 1963: head-on up to 35 km in ideal conditions, about
5.5 km from the stern (`parsch_dusrm`, AIM-7); the 1,500 m minimum range is Parsch's figure for the Vietnam-era AIM-7 in
general, not stated for the -7E alone. AIM-7E-2 "dogfight" Sparrow 1969: shorter minimum range, clipped wings (same); the
USAF's 1969 study says the "Dogfight Sparrow" "could be launched as close as 2,000 feet from the target aircraft"
(`checo_1969_a2a`, ch. VI, p. 33; the study does not write "AIM-7E-2"). AIM-7F: production 1975-1981, dual-thrust
motor, 39 kg warhead (`parsch_dusrm`). AIM-7M: first firing of a YAIM-7M in 1980 (not "first flight"), production 1982,
inverse monopulse seeker for low altitude and jamming (same). All are semi-active: the fighter illuminates to
impact (`cbo_1986_amraam`). [1963-1982]

**aam-nato-04. The Sparrow in combat.** 23 April-8 July 1967: 11.1 per cent hits (2.8 of them probables) per AIM-7
fired; 35 per cent of AIM-7s launched outside permissible parameters (`checo_1967_a2a`, p. 44). 1973: about 7 Arab
aircraft to Sparrow; 1982 Lebanon: 2 Syrian aircraft (`cbo_1986_amraam`, n. 4). Earlier versions were "vulnerable to
enemy countermeasures, such as electronic spoofing" (same). In the beam the AIM-7 had "limited capability due to the
degraded ability of the missile to discriminate its target from ground signals" (`afit_1982_vid`, ch. 1). [1965-1982]

**aam-nato-05. The Sidewinder in combat.** 1965-66: one success in eleven in the first year (`mcallister_1985`, p. 17,
citing a 1980s source). 23 April-8 July 1967: 18.6 per cent hits (1.7 probables) per AIM-9 fired, 26 per cent launched
outside parameters (`checo_1967_a2a`, p. 44). MiG pilots out-manoeuvred both missiles "with a rapid turning descent"
(`aces_1976`, p. 10). Falklands 1982: Argentine losses to Sidewinders were 9 Daggers, 6 Skyhawks, 2 Mirage IIIs, 1
Canberra and 1 C-130 (with 30 mm) (`harr_1988`, App. E, after Ethell and Price 1983). After the first day the Argentine
pilots "did not deliberately enter into air fights" with the Sea Harriers (`hezsely_1988`, ch. V). The Argentine
Mirages' Magic was rear-quarter only, the AIM-9L all-aspect (`dtic_1995_falklands`, n., after Woodward; flagged
hindsight). [1965-1982]

**aam-nato-06. Vietnam USAF MiG kills by weapon (all 1965-73).** 137 MiGs (61 MiG-17, 8 MiG-19, 68 MiG-21): AIM-7 50
(F-4C 14, F-4D 26, F-4E 10); AIM-9 33 (F-4C 22, F-4D 5, F-4E 4, F-105D 2); 20 mm guns 40 (F-105D 22, F-105F 2, F-4C 4,
F-4D 6, F-4E 5, F-4D/F-105F 1); AIM-4 5; AIM-9 and gun together 2; manoeuvring (MiG flew into ground) 5; B-52 tail
gunners 2 (`aces_1976`, Table 4). [1965-1973]

**aam-nato-07. Reliability and switchology, 1972.** AIM-7E-2 and AIM-9E rounds had "guidance failure indications of 25
to 30 percent" when downloaded for inspection after 10 to 15 flights; "many missile failures in-flight were the result
of incorrect switch positions", especially when the pilot changed delivery mode after hard manoeuvring
(`checo_1978_linebacker`, pp. 30-31). Standard air-defence load: three tanks, two or four AIM-7E-2, four AIM-9E/J, two
ECM pods (same). [1972]

**aam-nato-08. Guns.** The F-4C had no gun until the SUU-16 pod, first used in combat in May 1967 (`aces_1976`, p. 10);
before it a pilot "would often find himself too close to the MiG, when only a gun could be used" (`checo_1967_a2a`,
p. 45). The F-105's M61 "performs well throughout the flight envelope" (same). Shoot order in 1968 F-4E units: Sparrow
first, then Sidewinder, then close with the gun (`checo_1969_a2a`). A Soviet comparison gives the F-4E's M61 6-7 s of
firing, the Mirage F1's two DEFA more than 10 s (`airwar_ru`, MiG-23M; the Soviet view). Ammunition counts and rates for
the M61, Aden, DEFA 552/553 and Mauser BK-27 in 1985 aircraft were not found in a usable source. [1967-1985]

**aam-nato-09. Visual identification rules in central Europe.** NATO's IFF sets differed by nation and were unreliable,
so a visual identification was required before a radar missile could be fired at an unknown (`afit_1982_vid`, ch. 1).
Tactics to comply: the lead-trail "ident" pass (the leader identifies, the trailer fires) or a bracket; both put the
F-15s "well within the range of the enemy's radar missile capability"; optical aids (Eagle Eye on the F-15, TISEO on the
F-4) extend identification range but not enough (same). The F-15 was expected to be outnumbered about three to one
(same). The Mark XII IFF "has operational problems and is subject to spoofing" (`cbo_1986_amraam`, ch. 2 n. 5). Visual
range is about three to four miles (same, n. 4). [1982-1986]

**aam-nato-10. F-16A had no radar missile.** "F-16s cannot carry Sparrows" (`cbo_1986_amraam`, ch. 2); AMRAAM would
give them beyond-visual-range shots and look-down shoot-down for the first time. In 1985 the F-16A fights with AIM-9 and
its gun only. [1986 statement about the fleet then in service]

**aam-nato-11. The illuminating fighter is vulnerable.** The mid-1970s AIMVAL trials "indicated that fighters forced to
continue to illuminate the target were especially vulnerable to enemy fire" (`cbo_1986_amraam`, n. 4). [mid-1970s]

**aam-nato-12. Combat Tree and Teaball (1972).** Combat Tree interrogated the enemy's IFF: "approximately 17 of our last
20 MIG kills were made possible either directly or indirectly by the use of Combat Tree equipped aircraft"; the North
Vietnamese responded by switching their IFF on only during critical phases of the intercept; US crews faked a Combat
Tree failure to draw MiGs in (`checo_1978_linebacker`, p. 53). Teaball, a ground control centre fusing signals
intelligence with radar, plotted friend and foe once a minute and turned May 1972's 1:0.47 into 19 MiGs for 5 losses
(3.8:1) from early August to mid-October (pp. 52-53). The Navy's radar ship could not see below 10,000 ft over Hanoi
(p. 52). The APX-80/81 designations were not in the documents read. [1972]

**aam-nato-13. NATO fighter radars.** Detection ranges for the APG-63, APG-66, APQ-120 and AWG-10/11/12 were not found
in a usable public source. A Soviet account says the APQ-120 had no look-down mode and was less resistant to jamming
than the MiG-23M's radar (`airwar_ru`, MiG-23M; the Soviet view). A 1982 AFIT simulation used 24 nm detection and track
for the F-15 head-on; these are the study's model inputs, not measured values (`afit_1982_vid`, Fig. 10). Bands and
service years are in `nato_radar_refs`. The RAF's Foxhunter radar still did not meet the RAF's requirements in 1987
(`hansard_1987_foxhunter`, snippet only), so the Tornado air defence variant is not a 1985 fighter. [1974-1987]

### 2.2 Warsaw Pact (the Soviet frontal aviation and the allies)

**aam-wp-01. K-13 family (R-3S, R-13M).** K-13 series production 1960; R-3S (AA-2); R-13M with a freon-cooled
"Inei-70" seeker and a new proximity fuze, developed 1969, adopted 3 January 1974 (`ru_wiki_aam`, K-13; (unverified):
the Wikipedia paragraph has no footnote and Wikipedia may only point to a source; a tier 1-4 source for the date is still
to find). A Soviet account says the K-13A's launch was limited to about 2 g carrier load and its seeker took a long time
to lock with small angles, so the pilot had to point his aircraft at the target and hold it (`airwar_ru`, R-60;
(unverified) on re-reading: the page's text came back mis-encoded and this passage could not be found again). The US view in 1967:
launch under 2 g below 47,500 ft and 1.6 g above; minimum range 3,280 ft (1.0 km); the missile pulls 10 g at sea level
and 4 g at 50,000 ft; no guidance within 30° of the sun and great difficulty from 30 to 50°; ground clutter seriously
degrades it, so a low attacker does not fire at a target below him (`checo_1967_a2a`, pp. 19-20; adversary's view).
[1960-1974]

**aam-wp-02. R-60 and R-60M (AA-8).** R-60 adopted 18 December 1973 (`airwar_ru`, R-60). Uncooled "Komar" seeker,
designation angles to 12°, tracking ±30-35° at up to 35°/s, no front-hemisphere attacks (`ru_wiki_aam`, R-60). Launch at
up to 7 g carrier load; maximum range 7.2 km at about 12 km altitude, "almost three times less" near the ground;
minimum 0.25 km; target load 8 g in the text (12 g in the table) (`airwar_ru`, R-60). Rod warhead 3 kg, damage radius
2.5 m, sure kill only on a direct hit; fuze radius 5 m; motor 3-5 s (`ru_wiki_aam`, `airwar_ru`). Calculated kill
chance 0.5 for one, 0.7-0.8 for a pair (`airwar_ru`, R-60, confirmed on re-reading); R-60M (cooled "Komar-M",
designation angles ±17° against ±12°, 3.5 kg warhead) usable head-on in close combat, calculated 0.9 for one
(`ru_wiki_aam`, which footnotes Markovsky and Perov 2005, pp. 25-30, for the 0.9: that book, not read, is the source to
cite; the 3.5 kg, 2.5 m and 5 m figures carry no footnote there). Carried by MiG-21 (MF, bis), MiG-23, MiG-27, Su-17,
and as self-defence by Su-24 and Su-25 (`airwar_ru`, R-60). R-60M adoption date (unverified): "early 1980s" is the
author's guess; Russian Wikipedia's table lists adoption years 1973 and 1977 without saying which belongs to which
variant, so 1977 may be the R-60M's. Either way it is before 1985. The MiG-23MLD carried it, and one is said to have shot
down a Pakistani F-16A in 1986-87 (`airwar_ru`, MiG-23MLD; a claim, post-period event). [1973; R-60M 1977-early 1980s]

**aam-wp-03. R-23 (AA-7), R-23R and R-23T.** Serial production 1973 (`ru_wiki_aam`, R-23); adopted with the MiG-23M
(`mis`, R-24). The first Soviet missile able to engage a target against the ground; R-23T's seeker locks head-on on the
kinetic heating of the leading edges; rod 23 kg or fragmentation 26 kg warhead (`ru_wiki_aam`). Launch range 25 km head-on,
15 km from the rear (implied by `mis`, R-24, which says the R-24R raised these to 32 and 20 km). A Soviet comparison
rates the R-23R above the AIM-7B and R.530 but slightly below the AIM-7F (`airwar_ru`, MiG-23M). [1973-1974]

**aam-wp-04. R-24 (AA-7), R-24R and R-24T.** Adopted for the MiG-23ML and MiG-23P by a decree of 6 April 1981 (`mis`,
R-24). R-24R: semi-active seeker with an inertial "pseudo-kinematic" mode giving 10 s of autonomous flight, so it could
be fired about 30 per cent beyond the seeker's lock range; R-24T: all-aspect nitrogen-cooled seeker; rod warhead 35 kg,
lethal radius 10 m; Mach 3.5; minimum 0.5 km; targets 40 m to 25 km; controlled flight 45 s (same). Exported with the
MiG-23 to, among others, Bulgaria, Czechoslovakia, East Germany, Hungary, Poland and Romania (same; the page gives no
export dates and its list includes post-1990 states, so whether the Pact allies held the R-24 by 1985 is (unverified)). The US view in July
1982: "range of 10 to 14 NM", associated only with the MiG-23's air superiority versions (`npic_1982_apex`). [1981]

**aam-wp-05. R-73 (AA-11).** Adopted 22 June 1984; maximum 30 km head-on, minimum 0.3 km from the rear; targets
manoeuvring to 12 g; seeker designation angles to 60° (text; 45° in the table), gimbal ±75°, tracking 60°/s; first
exports in 1988 (`airwar_ru`, R-73; on re-reading the 1988 export is there but the GDR as the buyer and the 0.3 km
minimum's row label could not be read in the mis-encoded text). So in 1985 it is in Soviet units only (with the MiG-29). [1984]

**aam-wp-06. R-27 (AA-10).** The family was formally adopted in 1987-1990 (`airwar_ru`, R-27). Brochure figures: R-27R
80 km, R-27T 70 km, launch at up to 5 g, designation angles 50° (radar) and 55° (infrared), targets to 3,500 km/h
between 20 m and 27 km, the R-27R's seeker locking a 3 m² target at 25 km (same). Whether frontal MiG-29 units carried
it before formal adoption was not found. Treat as Soviet-only and doubtful in 1985 (section 5). [1987-1990]

**aam-wp-07. R-40 (AA-6).** The MiG-25P's missile: R-40R and R-40T; maximum 35-60 km head-on against a high target;
35 kg warhead, 7 m effective radius; 475 kg (`ru_wiki_aam`, R-40). National air defence interceptor, not a frontal
weapon. [late 1960s-1970s]

**aam-wp-08. Guns.** GSh-23 adopted in 1965 after development from 1959; projectile 173 g (`ru_wiki_aam`, GSh-23). The
MiG-23MLD's GSh-23L has 200 rounds (`airwar_ru`, MiG-23MLD); the MiG-23M had about 4 s of firing (`airwar_ru`, MiG-23M).
The US view of the older guns in 1967: NR-23 effective range 3,500 ft, 800 rounds a minute, 80 rounds a gun (6 s);
the standard N-37 400 rounds a minute, 40 rounds (6 s) (`checo_1967_a2a`, p. 18; adversary's view). GSh-30-1 data were not
found. [1965-1985]

**aam-wp-09. MiG-23 radars and infrared sights.** MiG-23M: Sapfir-23D, the first Soviet series fighter radar able to
work against targets over the ground, detection 55 km and lock 35 km for a typical target; TP-23 infrared search set,
which saw an F-4 from the rear at about 20 km in simple weather (`airwar_ru`, MiG-23M). MiG-23ML: Sapfir-23ML, 85 km
detection and 55 km lock on paper; in service 50-55 km with the pilot searching by hand and 80-85 km under automatic
ground guidance, lock head-on at 30-50 km; TP-23M detection 35 km from the rear against a clear sky; SRO-2M IFF; the
"Lazur-SML" command link; the SPO-10 radar warning receiver (`airwar_ru`, MiG-23ML). MiG-23ML built for the Soviet air
force 1976-81 and for export to 1985 (same). [1974-1985]

**aam-wp-10. MiG-23MLD (the Pact's escort fighter in SAM3D).** 66 built new in 1984-85 and many earlier aircraft
converted at repair plants; SPO-15L "Beryoza" warning receiver with more accurate bearings, SPS-141 jammer, "Parol" IFF;
BVP-50-60 dispensers with 60 × 50 mm LO-43 flares, added for Afghanistan; Sapfir-23MLA-2 radar, TP-23M, "Lazur" command
link; typical load two R-24R or R-24T and four R-60 (`airwar_ru`, MiG-23MLD; the page's "or two R-73" belongs to the
later variants and is post-period for the 1985 war, section 5). On re-reading, the 66 aircraft, the 60 flares, the 200
rounds, the load and a 1984 service year were legible; the SPO-15L, SPS-141 and "Parol" could not be tied to the MLD in
the mis-encoded text (unverified for the MLD). The variants
with the R-73 and the "Shchel-3UM" helmet sight (MiG-23MLG, MLS, MLDG) were developed from 1984 but "did not receive
further development" (same): section 5. [1982-1985]

**aam-wp-11. The Pact's fighters fought on ground control.** The MiG-23's radar range nearly doubled under ground
guidance; where guidance was jammed or absent (Arab-Israeli wars, Iraq) the F-15 and F-16 saw the MiG-23 first at 60-70
km (`airwar_ru`, MiG-23ML, the Soviet account). In Vietnam, MiG-21s came in low under GCI until abeam the F-4s, outside
the F-4's radar arc of 60° either side of the nose, zoomed to 25,000-28,000 ft, dived through at over Mach 1, fired the
Atoll and zoomed away (`checo_1969_a2a`); the Atoll "had proved to be the best weapon of the MIGs" (same, ch. III, p. 24). In May
1972 they attacked "only when the pilot and GCI controller perceived a clear-cut advantage" (`checo_1978_linebacker`,
p. 52). Over the Bekaa the Syrian MiGs were cut off from ground control (`grant_2002`, as registered). [1967-1982]

**aam-wp-12. Soviet and client combat records (claims).** By minimum estimates North Vietnamese MiG-21s claimed 76 US
aircraft with the R-3S; in 1973 Syria claimed at least 25 R-3S victories and about 30 R-3S failed to guide; in the
Iran-Iraq war Iraqi MiG-21s and MiG-23s claimed at least 13 with the R-13 (`ru_wiki_aam`, K-13; these are claims, and
Wikipedia footnotes them to ACIG victory lists and a T. Cooper article, Western enthusiast compilations of tier 5
quality: (unverified), not usable as data). The
R-60 over Lebanon in 1982 hit "precisely in the engine nozzles" (`airwar_ru`, R-60). Syrian MiG-23MLs claimed three F-15
and one F-4 with the R-24 in 1982 (`mis`, R-24; `airwar_ru`, MiG-23ML); Russian Wikipedia says the claims are not
confirmed (`ru_wiki_aam`, R-23); the CBO's tally gives Israel no such losses (section 4). [1965-1988]

**aam-wp-13. MiG-29 in Soviet frontal units.** Only tier-5 pages were found for the date (first series aircraft to
Kubinka in 1983). The airwar page on the R-73 says the third series MiG-29 joined the K-73's tests in 1981 (`airwar_ru`,
R-73; (unverified) on re-reading: the legible text ties the MiG-29 to the tests in 1979-80 and no 1981 was found). No usable source for the N019 radar, the KOLS infrared sight or the GSh-30-1 was read. Treat the MiG-29 as a
Soviet-only aircraft in 1985 whose weapons beyond the R-60 and R-73 are not established. [1983?]

### 2.3 The neutral (a small air arm on the Austrian pattern)

**aam-neu-01. Guns only.** Austria's air force had a ban on guided missiles until the early 1990s; its Drakens (1988)
carried guns only, and its 1985 combat jets were Saab 105Ö light jets (`austrian_af_wiki`; tier problem: Wikipedia
is the only source carrying this, and may only point to one. The primary document is the 1955 Austrian State Treaty,
Article 13, on "self-propelled or guided missiles"; it could not be fetched in this check, so the ban and its end
date are (unverified) from a citable source). So the neutral of 1985 has no
air-to-air missiles: its light jets and any interceptor fight with guns. The Vietnam record shows guns can kill fighters
(40 of the USAF's 137 MiGs, 22 of them by F-105s: `aces_1976`, Table 4), but only after getting close behind a target
that has not seen them. [1970-1990s]

### 2.4 The rebels

**aam-reb-01. No guided air-to-air weapons.** A defected light jet of the G-2 Galeb class carries guns; helicopters
carry door guns and rockets (the theatre's data). No source was read on helicopter air-to-air combat in the period; a
1978 US Army study title ("Hardware Requirements for Helicopter Air-to-Air Weapons in the 1980s", DTIC ADB029829) was
seen but not read. The R-60 could be carried by the Mi-24P and Mi-24V (`airwar_ru`, R-60, carrier list; date not given;
(unverified): the Mi-24 was not found in the legible carrier list on re-reading).

### 2.5 Our side (a Pact-like small nation)

**aam-our-01. MiG-21MF and MiG-21bis.** The MiG-21MF carries up to four K-13M, K-13, R-60 or R-60M (`airwar_ru`,
MiG-21MF); the R-60 table lists the MiG-21MF and MiG-21bis among its carriers (`airwar_ru`, R-60). Their radars (RP-21
and RP-22 "Sapfir-21") were not found in a usable source. The R-13M and R-60 are rear-aspect; the R-60M gives close
head-on shots. GSh-23L gun (adopted 1965). [1974-1985]

**aam-our-02. MiG-23ML.** R-24R and R-24T adopted for it in April 1981 and exported with it to the Pact allies; R-23R
and R-23T were its first planned missiles; R-60 and R-60M; Sapfir-23ML, TP-23M, SPO-10, SRO-2M, Lazur-SML (`mis`, R-24;
`airwar_ru`, MiG-23ML). Its real search range is 50-55 km alone and 80-85 km under the sector's ground control. [1981]

**aam-our-03. IFF between two MiG-23 forces.** Our MiG-23ML (SRO-2M) and the Pact's MiG-23MLD ("Parol") carry
different IFF generations (`airwar_ru`). A war with the Pact puts the same aircraft type on both sides; the sources do
not say whether SRO-2M would answer "Parol". Identification by ground control and by eye is the realistic rule.

## 3. Numbers usable as data

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| AIM-7 hits per missile fired | 11.1 % (2.8 % probables) | NATO (USAF) | Apr-Jul 1967 | `checo_1967_a2a` p. 44 | high |
| AIM-9 hits per missile fired | 18.6 % (1.7 % probables) | NATO (USAF) | Apr-Jul 1967 | `checo_1967_a2a` p. 44 | high |
| AIM-7 launches outside parameters | 35 % | NATO (USAF) | 1967 | `checo_1967_a2a` p. 44 | high |
| AIM-9 launches outside parameters | 26 % | NATO (USAF) | 1967 | `checo_1967_a2a` p. 44 | high |
| Hit rate per in-envelope launch (derived, all hits assumed in-envelope) | AIM-7 ≈ 0.17, AIM-9 ≈ 0.25 | NATO | 1967 | derived from `checo_1967_a2a` | medium |
| Sidewinder success, first year of combat | 1 in 11 | NATO (USAF) | 1965-66 | `mcallister_1985` p. 17 | medium |
| AIM-7E-2 / AIM-9E guidance failure indications after 10-15 captive flights | 25-30 % | NATO (USAF) | 1972 | `checo_1978_linebacker` p. 30 | high |
| USAF MiG kills 1965-73 | 137: AIM-7 50, AIM-9 33, guns 40, AIM-4 5, AIM-9+gun 2, manoeuvre 5, B-52 guns 2 | NATO (USAF) | 1965-73 | `aces_1976` Table 4 | high |
| USAF exchange ratio | 81:36 (2.25:1) 1965-68; about 1:1 in 1968; Korea 6.2:1 | NATO (USAF) | 1965-68 | `mcallister_1985` p. 17 | medium |
| Exchange ratio, Linebacker | 1:0.47 May 1972; 19:5 (3.8:1) Aug-mid-Oct 1972 | NATO (USAF) | 1972 | `checo_1978_linebacker` pp. 52-53 | high |
| Kills made possible by Combat Tree | about 17 of the last 20 | NATO (USAF) | Sept 1972 | `checo_1978_linebacker` p. 53 | high |
| Teaball position update | one position per minute per track | NATO (USAF) | 1972 | `checo_1978_linebacker` p. 52 | high |
| US radar cover floor over Hanoi (Red Crown) | 10,000 ft | NATO (USN) | 1972 | `checo_1978_linebacker` p. 52 | high |
| MiG share of US combat losses | 1 % 1965, 3 % 1966, 8 % 1967, 22 % Jan-Mar 1968 | Pact client | 1965-68 | `checo_1969_a2a` | high |
| Strike aircraft engaged by MiGs that jettisoned | 107 of 192 (55.7 %); 107 of 3,938 strike sorties (2.7 %) | NATO (USAF) | Sept-Dec 1966 | `aces_1976` p. 10 | high |
| Arab aircraft destroyed by IR missiles vs Sparrow | about 200 vs about 7 | Israel | 1973 | `cbo_1986_amraam` n. 4 | medium |
| Syrian aircraft destroyed by IR missiles vs Sparrow | about 50 vs 2 | Israel | 1982 | `cbo_1986_amraam` n. 4 | medium |
| Argentine aircraft lost to Sidewinders | Dagger 9, Skyhawk 6, Mirage III 2, Canberra 1, C-130 1 (with 30 mm) | NATO (RN) | 1982 | `harr_1988` App. E | medium |
| Visual range (for identification) | about 3-4 miles (4.8-6.4 km) | general | 1986 | `cbo_1986_amraam` n. 4 | medium |
| F-15 to threat ratio expected in central Europe | about 1:3 | NATO | 1982 | `afit_1982_vid` | medium |
| AFIT model inputs: detection / track head-on | F-15 24/24 nm; "Type 1" threat 8/5 nm; "Type 3" threat 24/16 nm | NATO model | 1982 | `afit_1982_vid` Fig. 10 | low (model inputs) |
| AIM-9B envelope | 900 m - 4.8 km, non-manoeuvring targets | NATO | 1956 | `parsch_dusrm` | medium |
| AIM-7E envelope | head-on up to 35 km (ideal), stern about 5.5 km; minimum 1,500 m (Parsch's Vietnam-era AIM-7 figure) | NATO | 1963 | `parsch_dusrm` | medium |
| AIM-7E-2 minimum launch range | about 2,000 ft (610 m) | NATO | 1969 | `checo_1969_a2a` | medium |
| AIM-7F warhead | 39 kg | NATO | 1975 | `parsch_dusrm` | medium |
| AIM-9L/M warhead | 9.4 kg | NATO | 1978/1982 | `parsch_dusrm`, `navy_factfiles` | high |
| AIM-9M deliveries began | 1983 | NATO | 1983 | `navy_factfiles` | high |
| AIM-7M production began | 1982 | NATO | 1982 | `parsch_dusrm` | medium |
| K-13 launch load limit (US view) | under 2 g below 47,500 ft; 1.6 g above | Pact | 1967 | `checo_1967_a2a` p. 20 | medium |
| K-13 minimum range (US view) | 3,280 ft (1.0 km) | Pact | 1967 | `checo_1967_a2a` p. 20 | medium |
| K-13 manoeuvre (US view) | 10 g sea level, 4 g at 50,000 ft | Pact | 1967 | `checo_1967_a2a` p. 20 | medium |
| K-13 sun exclusion (US view) | no guidance within 30° of the sun; great difficulty 30-50° | Pact | 1967 | `checo_1967_a2a` p. 20 | medium |
| R-13M adopted | 3 January 1974 (unverified: Wikipedia only, no footnote) | Pact, our side | 1974 | `ru_wiki_aam` | low |
| R-60 adopted | 18 December 1973 | Pact, our side | 1973 | `airwar_ru` | medium |
| R-60 maximum range | 7.2 km at about 12 km altitude; about one third near the ground (≈2.4 km, derived) | Pact, our side | 1973 | `airwar_ru` | medium |
| R-60 minimum range | 0.25 km | Pact, our side | 1973 | `airwar_ru` | medium |
| R-60 launch load | up to 7 g | Pact, our side | 1973 | `airwar_ru` | high |
| R-60 target load | 8 g (text); 12 g (table) | Pact, our side | 1973 | `airwar_ru` | low |
| R-60 seeker | designation ±12°, tracking ±30-35° at 35°/s; rear hemisphere only | Pact, our side | 1973 | `ru_wiki_aam` | medium |
| R-60 calculated kill chance | 0.5 one missile; 0.7-0.8 a pair | Pact, our side | 1973 | `ru_wiki_aam`, `airwar_ru` | medium (design) |
| R-60M calculated kill chance | 0.9 one missile, close range | Pact, our side | 1977-early 1980s (adoption unverified) | `ru_wiki_aam` (after Markovsky and Perov 2005, not read) | low (design) |
| R-60 warhead / lethal radius / fuze radius | 3 kg rod / 2.5 m / 5 m | Pact, our side | 1973 | `airwar_ru`, `ru_wiki_aam` | medium |
| R-23R launch range | 25 km head-on, 15 km rear | Pact, our side | 1974 | `mis` R-24 (implied) | low |
| R-24 adopted | 6 April 1981 (MiG-23ML, MiG-23P) | Pact, our side | 1981 | `mis` | medium |
| R-24R launch range | 32 km head-on, 20 km rear (text); 50 km (table) | Pact, our side | 1981 | `mis` | low |
| R-24T launch range | 35 km (table) | Pact, our side | 1981 | `mis` | low |
| R-24 minimum range / target heights | 0.5 km / 0.04-25 km | Pact, our side | 1981 | `mis` | medium |
| R-24R autonomous flight | 10 s; launch about 30 % beyond seeker lock range | Pact, our side | 1981 | `mis` | medium |
| R-24 warhead / lethal radius / speed | 35 kg rod / 10 m / Mach 3.5 | Pact, our side | 1981 | `mis` | medium |
| Apex range (US view) | 10-14 nm (18.5-26 km) | Pact | July 1982 | `npic_1982_apex` | medium (adversary's view) |
| R-73 adopted / first export | 22 June 1984 / GDR 1988 | Pact (Soviet only in 1985) | 1984 | `airwar_ru` | medium |
| R-73 envelope | 30 km head-on max; 0.3 km rear min; target to 12 g; designation 45-60°; gimbal ±75°; 60°/s | Pact (Soviet) | 1984 | `airwar_ru` | medium |
| R-27 formal adoption | 1987-1990 | Pact (Soviet) | post-period | `airwar_ru` | medium |
| R-40R/T head-on range, high target | 35-60 km | Pact (national air defence) | 1970s | `ru_wiki_aam` | low |
| GSh-23 adopted / projectile | 1965 / 173 g | Pact, our side | 1965 | `ru_wiki_aam` | medium |
| GSh-23L rounds (MiG-23MLD) | 200 | Pact | 1984-85 | `airwar_ru` | medium |
| Firing time (Soviet comparison) | MiG-23M 4 s; F-4E 6-7 s; Mirage F1 over 10 s | all | 1970s | `airwar_ru` MiG-23M | low (Soviet view) |
| Sapfir-23D detection / lock | 55 km / 35 km | Pact | 1974 | `airwar_ru` MiG-23M | medium |
| TP-23 infrared detection of an F-4 from the rear | about 20 km, simple weather | Pact | 1974 | `airwar_ru` MiG-23M | medium |
| Sapfir-23ML detection | 85 km (specification); 50-55 km by hand, 80-85 km under ground guidance | Pact, our side | 1976-85 | `airwar_ru` MiG-23ML | medium |
| Sapfir-23ML lock head-on | 30-50 km | Pact, our side | 1976-85 | `airwar_ru` MiG-23ML | medium |
| TP-23M detection from the rear | 35 km against a clear sky | Pact, our side | 1976-85 | `airwar_ru` MiG-23ML | medium |
| F-15/F-16 detection of a MiG-23 (Soviet view) | 60-70 km | NATO | 1980s | `airwar_ru` MiG-23ML | low |
| MiG-23MLD flares | 60 × 50 mm LO-43 in BVP-50-60 | Pact | 1984-85 | `airwar_ru` MiG-23MLD | medium |
| Chance of being killed in a decisive encounter, experienced pilot | levels at about 0.02 after about ten encounters | general | WWI-WWII data | `bilikam_1970` (after Weiss) | medium |
| F-4 air combat training sorties | 6 per half-year (about 5 % of sorties); USN up to 20 per half | NATO (US) | 1969 | `mcallister_1985` p. 17 | medium |

## 4. Disagreements between sources and which to use

| Item | Sources | Use |
|---|---|---|
| R-60 target load | 8 g in `airwar_ru`'s text, 12 g in its table | 8 g for the R-60; 12 g for the R-73 (stated for it) |
| R-24R range | 32 km head-on and 20 km rear in `mis`'s text; 50 km in its table; 10-14 nm in the US 1982 note (`npic_1982_apex`) | 32 km as the kinematic maximum head-on at height, 20 km rear; the US figure is the adversary's working number |
| R-73 designation angle | 60° in `airwar_ru`'s text, 45° in its table | 45° for the missile cued by the aircraft's sights; 60° only with a helmet sight, which is post-period |
| R-24 in the June 1982 battles | `mis` and `airwar_ru` say Syrian MiG-23MLs shot down three F-15 and one F-4 with it; `ru_wiki_aam` says the claims are unconfirmed and puts the fight in 1983; the NPIC identified Apex in Syria for the first time on 26 July 1982 (`npic_1982_apex`); the CBO's tally counts no F-15 losses | No confirmed R-24 kill; record the Syrian claim as a claim. A good lesson for SAM3D's claims-versus-records rule |
| Sapfir-23ML detection | 85 km design figure; 50-55 km in service by hand | 50-55 km alone, 80-85 km under ground control |
| Falklands Sidewinder kills | 18 plus a shared C-130 (`harr_1988`, App. E, after Ethell and Price); "32 destroyed by AIM-9L Sidewinders or the 30mm cannon" (`hezsely_1988`, after Ethell and Price p. 72, which may count ground strafing); a popular site gives 27 fired for 19 kills (not registered, not used) | 18-19 Sidewinder kills; the number fired is not established from a usable source |
| Vietnam AIM-7 and AIM-9 kill rates | 1967 hit rates 11.1 and 18.6 % (`checo_1967_a2a`); the Ault report's 9.2 and 18 % are known only from a Wikipedia snippet; secondary pages give 1972 rates of about 13 % for the AIM-9J and 5 % for the AIM-7E-2 | Use the 1967 CHECO figures; the others go to section 7 until read |
| MiG-23 radar vs APQ-120 | The Soviet account rates the Sapfir-23D above the APQ-120 (look-down, jamming) | Recorded as the Soviet view; no Western figure found |
| Combat Tree's share | "17 of our last 20" is a wing's own evaluation (`checo_1978_linebacker`) | Use for the scale of the effect, not as a measured rate |

## 5. Post-period items excluded (with dates)

| Item | Date | Why excluded |
|---|---|---|
| AIM-120 AMRAAM (launch-and-leave radar missile) | requirement 1978; planned fielding 1989 in August 1986 (`cbo_1986_amraam`); in service 1991 | after 1985 |
| AIM-9X, ASRAAM | ASRAAM agreed in 1980 as a successor to Sidewinder (`cbo_1986_amraam`); service 1990s-2000s | after 1985 |
| Mark XV IFF | to replace Mark XII "in the 1990s" (`cbo_1986_amraam`) | after 1985 |
| R-27R/T in Pact allies' service; formal adoption | 1987-1990 (`airwar_ru`) | after 1985; Soviet MiG-29 carriage before adoption not established |
| R-27ER/ET | about 1990 | after 1985 |
| R-73 with Pact allies | first export to the GDR in 1988 (`airwar_ru`) | after 1985 (Soviet units only in 1985) |
| MiG-29 with Pact allies | GDR and others from 1988 | after 1985 |
| R-24M | adopted 1986 (`mis`) | after 1985 |
| MiG-23MLG/MLS/MLDG with R-73 and "Shchel-3UM" helmet sight | developed from 1984, not taken further (`airwar_ru`) | not in service |
| Tornado F3 with Foxhunter as an operational fighter | radar still short of the RAF's requirement in 1987-89 (`hansard_1987_foxhunter`, snippet) | not combat-ready in 1985 |
| Sidewinder's 13 kills in Desert Storm (probably AIM-9M) | 1991 (`parsch_dusrm`) | hindsight only: the AIM-9M existed by 1983 |
| R-60M kill of a Pakistani F-16A by a Soviet MiG-23MLD | 1986-87 (`airwar_ru`) | event after 1985; the weapon existed |
| Magic 2, Super 530D, R-77 | dates not checked in this session | to confirm; assumed later |
| R-27R as a semi-active missile in the 1985 war | family adopted 1987-1990 (`airwar_ru`) | removed from section 1 point 4 in the check |
| MiG-23MLD with two R-73 in place of the R-60s | later variants (`airwar_ru`, MiG-23MLD) | after 1985; the 1985 MLD carries R-24 and R-60(M) |
| Syrian and Iraqi R-3S/R-13 claim tallies via ACIG and T. Cooper | compiled after 2000 | not a period source and tier 5 quality; use only period records |

## 6. Sources

| Id | Tier | Full citation | URL | Read | Whose view |
|---|---|---|---|---|---|
| `checo_1967_a2a` | 1 (US study of its own war) | Project CHECO, HQ PACAF, *Air-to-Air Encounters Over North Vietnam, 1 January - 30 June 1967*, 30 November 1967, DTIC ADA485407 | https://archive.org/details/DTIC_ADA485407 | full text (OCR), searched | USAF on itself; the adversary's view of Soviet weapons |
| `checo_1969_a2a` | 1 | Project CHECO, *Air-to-Air Encounters over North Vietnam, 1 July 1967 - 31 December 1968*, 1969, DTIC ADA485056 | https://archive.org/details/DTIC_ADA485056 | full text (OCR), searched | USAF |
| `checo_1973_lb120` | 1 | Project CHECO, *LINEBACKER: Overview of the First 120 Days*, 1973, DTIC ADA487179 | https://archive.org/details/DTIC_ADA487179 | full text (OCR), searched | USAF |
| `checo_1978_linebacker` | 1 | Project CHECO, *LINEBACKER Operations, September - December 1972*, dated 31 December 1978, declassified 1991, DTIC ADA487182 | https://archive.org/details/DTIC_ADA487182 | full text (OCR), searched | USAF |
| `aces_1976` | 1 | R. F. Futrell, W. H. Greenhalgh, C. Grubb, G. E. Hasselwander, R. F. Jakob, C. A. Ravenstein, *Aces and Aerial Victories: The United States Air Force in Southeast Asia 1965-1973*, Albert F. Simpson Historical Research Center and Office of Air Force History, 1976, DTIC ADA476450 | https://archive.org/details/DTIC_ADA476450 | Table 4 and pp. 10-11 read; rest searched | USAF |
| `wseg_116_1967` | 1 | IDA/WSEG Report 116 (IDA R-125), *Air-to-Air Encounters in Southeast Asia, Vol. I: Account of F-4 and F-8 Events Prior to 1 March 1967* (Project Red Baron), October 1967 | https://archive.org/details/DTICWSEGReport116Volume1RedBaronAirtoAirEncountersSoutheastAsia28October1987 | first 100,000 characters only (events 1-1 to 1-6) | US DoD |
| `afit_1982_vid` | 1 (US Air Force study) | Capt D. R. Rogers, *Effect of the Visual Identification Requirement on Employment of the F-15 in the NATO Theatre*, AFIT/GST/OS/82M-12, Air Force Institute of Technology, March 1982, DTIC ADA115687 | https://archive.org/details/DTIC_ADA115687 | full text (OCR), searched | USAF |
| `cbo_1986_amraam` | 1 (US government) | Congressional Budget Office, *The Advanced Medium-Range Air-to-Air Missile (AMRAAM): Current Plans and Alternatives*, staff working paper, August 1986, DTIC ADA529898 | https://archive.org/details/DTIC_ADA529898 | full text (OCR), searched | US government; its war tallies cite Nordeen 1985 and Fitzpatrick 1985 |
| `npic_1982_apex` | 1 (for what the US believed) | National Photographic Interpretation Center, «APEX (AA-7) Air-to-Air Missile, Sayqal Airfield, Syria», briefing note Z-15787/82, 26 July 1982, CIA-RDP90T00784R000100300002-0 and CIA-RDP82T00709R000102660001-2 | https://archive.org/details/cia-readingroom-document-cia-rdp90t00784r000100300002-0 | full (one page) | US intelligence: the adversary's view |
| `hezsely_1988` | 4 (US officer's study, secondary) | Lt Col C. B. Hezsely, *Argentine Air Power in the Falklands War*, Air War College research report, 1988, DTIC ADA202551 | https://archive.org/details/DTIC_ADA202551 | full text, searched | US analyst after Ethell and Price |
| `harr_1988` | 4 (US officer's study, secondary) | Lt Col J. E. Harr, *War in the Falklands: Perspectives on British Strategy and Use of Air Power*, Air War College, 1988, DTIC ADA202643, Appendix E | https://archive.org/details/DTIC_ADA202643 | full text, searched | US analyst after Ethell and Price |
| `ethell_price_1983` | 4 (period book, not read) | J. Ethell and A. Price, *Air War South Atlantic*, Macmillan, 1983 | — | not read; cited through `harr_1988` and `hezsely_1988` | British and US authors |
| `dtic_1995_falklands` | 4 (flagged hindsight) | *The Falklands Air War: Lessons Revisited*, 1995, DTIC ADA293335 (note citing Woodward) | https://archive.org/details/DTIC_ADA293335 | searched | US study |
| `mcallister_1985` | 2 (an officer's study, open sources) | Maj B. J. McAllister, *Air-to-Air Continuation Training in the Tactical Air Command*, Air Command and Staff College student report 85-1780, 1985, DTIC ADA157151 | https://archive.org/details/DTIC_ADA157151 | full text, searched | USAF officer |
| `bilikam_1970` | 1 (US Air Force study) | J. E. Bilikam and R. K. Frick, *Analysis of Air-to-Air Combat Data and the Effect of Pilot Learning*, Aeronautical Systems Division, Wright-Patterson AFB, August 1970, DTIC ADA083960 | https://archive.org/details/DTIC_ADA083960 | full | USAF |
| `airwar_ru` (registered; pages added) | 4 | «Уголок неба» (airwar.ru): «Р-60», «Р-73», «Р-27» (weapon/avv) and «МиГ-23М», «МиГ-23МЛ», «МиГ-23МЛД», «МиГ-21МФ» (enc/fighter); the R-60 page cites Angelsky (*Авиация и космонавтика*), Markovsky and Perov (*М-Хобби*), Nikiforov (*Аэроплан*) | http://www.airwar.ru/weapon/avv/r60.html etc. | full | Russian compilers of Soviet designers' and pilots' material |
| `mis` (registered; page added) | 4 | missilery.info (English edition of a Russian reference), «Aircraft missile R-24 (K-24)» | https://en.missilery.info/node/13138 | full | Russian compilation |
| `ru_wiki_aam` (new; pointer only: Wikipedia may not be the cited source, so every figure resting on it alone is (unverified) until its source is read: Markovsky and Perov, *Советские авиационные ракеты «воздух-воздух»*, Экспринт, 2005, pp. 25-30, for the R-60M; rbase.new-factoria.ru for the R-60 seeker; Gordon, *Soviet/Russian Aircraft Weapons*, Midland, 2004) | 4 | Russian Wikipedia: «К-13 (авиационная ракета)», «Р-60», «Р-23 (ракета)», «Р-40», «Р-73», «Р-27», «ГШ-23» (read 15 Nov 2026; their literature: Shirokorad 1999, Markovsky and Perov 1995) | https://ru.wikipedia.org/wiki/Р-60 etc. | full | operator-country reference |
| `parsch_dusrm` (registered; pages added) | 4 | A. Parsch, *Directory of U.S. Military Rockets and Missiles*: AIM-9 and AIM-7 | https://designation-systems.net/dusrm/m-9.html, m-7.html | full | Western compilation from Friedman 1983, Gunston 1979 and others |
| `navy_factfiles` (registered; page added) | 1 | US Navy fact file «AIM-9 Sidewinder Missile», archived 2004 | https://webharvest.gov/peth04/20041016160604/http:/www.chinfo.navy.mil/navpalib/factfile/missiles/wep-side.html | full | US Navy on its own weapon |
| `nato_radar_refs` (registered) | 4 | Wikipedia, AN/APG-63 and AN/APG-66 | as registered | not re-read | — |
| `grant_2002` (registered) | 4 | R. Grant, «The Bekaa Valley War», *Air Force Magazine*, June 2002 | as registered | not re-read; cited as registered | Israeli general through press |
| `austrian_af_wiki` (registered) | 4 | Wikipedia, Austrian Air Force | as registered | not re-read; cited as registered | — |
| `hansard_1987_foxhunter` | 1 (UK Parliament) | House of Commons written answers on the Foxhunter radar, 22 January, 2 April and 11 May 1987 | https://api.parliament.uk/historic-hansard/written_answers/1987/jan/22/foxhunter-radar | search snippet only | UK government |

## 7. Open items and documents to acquire

- **The Ault report** (Navy, *Report of the Air-to-Air Missile System Capability Review*, 1 January 1969): the NHHC
  section PDFs returned 404 and DTIC refused (ADA955142, ADA955143). Its firing statistics and causes of failure are the
  Navy's counterpart to `checo_1967_a2a`.
- **Red Baron III** (WSEG/IDA, 1974) summary volume: per-missile firings, kills and reasons for failure over the whole
  war; only volume I's first events were read. Volumes II and III are on archive.org (ADC003628, ADC003629).
- **Soviet flight manuals**: «Инструкция лётчику» and «Пособие лётчику по боевому применению» for the MiG-21bis and
  MiG-23ML: launch zones of the R-13M, R-60, R-24, the RP-22 and Sapfir-23ML's real ranges. Only a forum page was found
  (not used).
- **R-60M and R-13M1 adoption dates; MiG-29's entry into frontal units; R-27 carriage before 1987.** No tier 1-4 source
  found.
- **NATO radars and envelopes**: APG-63, APG-66, APQ-120, AWG-11/12 detection ranges; AIM-9L/P and AIM-7F launch
  envelopes; Sky Flash, Super 530F and Magic 1 dates and envelopes (French and British period sources). Not reached.
- **Gun data**: M61A1, Aden, DEFA 552/553, BK-27 and GSh-30-1 rates and loads in a usable source.
- **1973 and 1982 Israeli records** by weapon (Shafrir 2, Python 3, AIM-9D/G/L, AIM-7), and the Syrian side; Nordeen,
  *Air Warfare in the Missile Age* (1985), cited by the CBO, is the period compilation to read.
- **Iran-Iraq and Angola**: Iraqi and Iranian claims and losses by weapon; the SAAF's Mirage F1 engagements with
  Angolan and Cuban MiG-21s and MiG-23s (1981-88), from SAAF veterans' and Cuban accounts. Only low-quality pages found.
- **Pact allies' period journals**: *Vojenská mysl* and *Vojenské rozhledy* (Czechoslovak), *Przegląd Wojsk Lotniczych*
  (Polish), *Militärwesen* and *Flieger-Revue* (GDR), and the Soviet *Зарубежное военное обозрение* on the AIM-9L,
  AIM-7F and F-15 radar, and on Lebanon 1982: not searched (search budget ran out).
- **Helicopter air-to-air**: DTIC ADB029829 (1978) and Iran-Iraq helicopter duels.
- **Combat Tree in 1985**: whether USAFE's F-4Es kept the APX-81 and whether NATO fighters could read the Pact's SRO-2
  and "Parol" in 1985.

## 8. A gameplay air-to-air model that fits these numbers (proposal)

A SAM3D estimate, built on the numbers above; every calibration value is labelled.

1. **Per weapon** (data): guidance (IR rear, IR all-aspect, SARH, SARH with inertial start); head-on and rear maximum
   range at height; a low-altitude factor (R-60: about one third at the ground); minimum range; launch load limit;
   designation angle; target load limit; reliability (fraction that guide); a design kill chance for a perfect hit;
   warhead lethal radius; smoke (AIM-9M reduced from 1983).
2. **Per launch** the chance of a kill is: in-envelope (the shooter's skill and the rules; calibrated so that a Vietnam
   1967 crew is out of envelope 26-35 per cent of the time) × working (0.70-0.75 for a 1972 USAF round after captive
   wear; higher for a fresh AIM-9L or R-60M, a SAM3D estimate) × guiding (aspect class, sun within 30°, ground behind
   the target for a pulse SARH, flares, the target's break turn, all as multipliers) × lethal (design chance).
3. **Calibration targets**: AIM-7 about 0.11 and AIM-9 about 0.19 per launch for 1967 crews (`checo_1967_a2a`); an
   AIM-9L in the Falklands much higher (fired count not yet established); IR missiles taking the large majority of kills
   in 1973 and 1982 (`cbo_1986_amraam`); guns about 30 per cent of kills where fighters close (`aces_1976`).
4. **Sensors** set who shoots first: radar search range alone and under ground control, IRST range from the rear, a
   visual identification range of about 5-6 km for NATO under the 1982 rules, Combat Tree-style interrogation where a
   faction had it, and SARH launchers tied to their target until impact.
5. **Skill** moves the in-envelope chance and the chance to survive; a pilot's survival per decisive encounter improves
   with each one he lives through, toward 0.98 (`bilikam_1970`).

## Verification (15 Nov 2026)

An adversarial check of the most load-bearing findings against the cited documents, re-fetched on 15 Nov 2026 (DTIC
full texts on archive.org read in the relevant parts; the airwar.ru pages came back mis-encoded, so only their legible
figures count as re-read; the web search budget was exhausted early in the check, so no new sources could be searched for).

| Finding | Verdict | Note |
|---|---|---|
| aam-01 (CHECO 1967 hit and out-of-envelope rates) | confirmed | 11.1 % (2.8 probables), 18.6 % (1.7), 35 % and 26 % outside parameters, 23 Apr-8 Jul 1967, all verbatim; ch. X begins p. 45 by the contents, so cite "pp. 44-45" |
| aam-02 (1972 reliability, switchology) | confirmed | "guidance failure indications of 25 to 30 percent when downloaded" and "incorrect switch positions" on p. 30 of `checo_1978_linebacker` |
| aam-03 (137 MiG kills by weapon) | confirmed | `aces_1976` Table 4: AIM-7 50, AIM-9 33, AIM-4 5, gun 40, AIM-9+gun 2, manoeuvre 5, B-52 .50 cal 2; MiG-17 61, MiG-19 8, MiG-21 68; F-105D 22 by gun |
| aam-04 (IR vs Sparrow, 1973 and 1982) | confirmed | CBO 1986 n. 4: about 200 vs about 7 (1973), about 50 vs 2 (1982); a 1986 paper relying on 1985 compilations: period source |
| aam-05 (VID rule, AFIT 1982) | confirmed | "reduces the F-15 to the equal of the Soviet first generation aircraft", "must allow first shot opportunities", 3 to 1, lead-trail and bracket, AIM-7 beam weakness, Eagle Eye and TISEO |
| aam-06 (F-16 no Sparrow, visual range, Mark XII, AIMVAL) | confirmed | all four in `cbo_1986_amraam`; AMRAAM IOC moved to 1989 by the 1985 restructuring |
| aam-07 (AIM-9 dates) | confirmed | Parsch: 9B 900 m-4.8 km tail-on, 9J 1972 "doubled the single-plane G", 9L 1971/1978 InSb, laser fuze, 9.4 kg; 9M production 1982. The 1983 delivery date (`navy_factfiles`) was not re-read |
| aam-08 (AIM-7 dates) | corrected | 1,500 m minimum is Parsch's general Vietnam-era figure; CHECO 1969 says "Dogfight Sparrow" (not "AIM-7E-2"), p. 33; AIM-7M 1980 is a first firing, not a first flight |
| aam-09 (K-13 in US 1967 intelligence) | confirmed | pp. 19-20 verbatim: <2 g below 47,500 ft, 1.6 g above, 3,280 ft, 10 g / 4 g, sun 30° and 30-50°, ground clutter; adversary's view correctly labelled |
| aam-10 (R-13M date, K-13A 2 g) | unsupported | 3 Jan 1974 rests on an unfootnoted Wikipedia paragraph; the airwar R-60 passage on the K-13A could not be re-read; marked (unverified) |
| aam-11 (R-60 figures) | confirmed | airwar: 18 Dec 1973, 7 g, 7.2 km, about a third near the ground, 0.25 km, 3 kg, 2.5 m, 0.5 / 0.7-0.8; seeker angles via ru-wiki (pointer to rbase); fuze 5 m uncited there |
| aam-12 (R-60M) | corrected | adoption date unverified: ru-wiki table lists 1977 and 1973 without mapping; designation ±17°; 0.9 traced to Markovsky and Perov 2005 (not read); F-16A kill 1986-87 is a post-period claim |
| aam-14 (R-24) | confirmed | missilery: decree 6 Apr 1981 for MiG-23ML and MiG-23P, 10 s autonomous, ~30 % beyond lock, 35 kg, 10 m, M 3.5, 0.5 km, 0.04-25 km, 25→32 and 15→20 km, table 50/35 km; NPIC 26 Jul 1982 "10 to 14 NM" for Flogger B and G. Export dates not given: the Pact allies' holding by 1985 unverified |
| aam-15 (R-73) | confirmed | 22 Jun 1984, 30 km, 12 g, 60°/45°, ±75°, 60°/s, 1988 export legible; GDR as buyer and 0.3 km row label illegible; presence in Soviet frontal units in 1985 depends on the MiG-29's date, not established |
| aam-16 (MiG-23 sensors) | confirmed | legible on airwar MiG-23M and MiG-23ML: 55/35 km, TP-23 20 km, 85 km spec, 50-55 km by hand, 80-85 km under ground guidance, lock 30-50 km, TP-23M 35 km, SRO-2M, Lazur-SML, SPO-10, built 1976-81 and to 1985 for export |
| aam-17 (MiG-23MLD) | corrected | 66 built 1984-85, 60 × 50 mm flares, 200 rounds, 2 R-24 + 4 R-60, service 1984 legible; SPO-15L, SPS-141, Parol not tied to the MLD in the legible text; "or two R-73" moved to post-period |
| aam-19 (Combat Tree, Teaball, Red Crown) | confirmed | p. 53 "17 of our last 20", IFF only in critical phases; p. 52 "1 to .47 in May", 19 MiGs for 5 losses Aug-mid-Oct, one position per minute, Red Crown blind below 10,000 ft over Hanoi, "clear-cut advantage" |
| aam-18 (Pact GCI tactics) | confirmed | CHECO 1969: low until beyond the radar envelope, GCI-advised climb to 25-28,000 ft, "well over Mach 1" diving turn, Atoll "the best weapon of the MIGs" (p. 24, not p. 23); Bekaa via `grant_2002` not re-read |
| aam-20 (Falklands Sidewinder kills) | confirmed | `harr_1988` App. E after Ethell and Price 1983: Dagger 9, A-4 6, Mirage III 2, Canberra 1, C-130 1 (Sidewinder/30 mm) |
| aam-21 (MiG share of losses, jettison) | confirmed | CHECO 1969: 1, 3, 8, 22 % (Q1 1968); `aces_1976` p. 10: 107 of 192 (55.73 %), 107 of 3,938 sorties (2.72 %), Sept-Dec 1966 |
| aam-22 (pilot learning, ACM training) | confirmed | Bilikam and Frick, Aug 1970: levels at 0.02 after about ten encounters (Weiss, WWI/WWII data); McAllister 1985: 81:36, about 1:1 in 1968, 6 ACM sorties semi-annually, Navy up to 20, Sidewinder 1 in 11 |
| aam-23 (guns) | corrected | NR-23 3,500 ft, 800 rpm, 80 rounds, 6 s confirmed; N-37 is 400 rpm, 40 rounds, 6 s (not 400-500, 5-6 s); GSh-23 data rest on Wikipedia only |
| aam-24 (neutral without missiles) | unsupported | rests only on Wikipedia (tier problem); the State Treaty's Art. 13 is the primary document, not fetched |
| aam-25 (claims unconfirmed) | confirmed | Syrian R-24 claims on missilery and airwar; NPIC first identification 26 Jul 1982; the R-3S failure and tally figures trace to ACIG / T. Cooper (tier 5 quality), now marked |

Scan for modern tactics: none presented as 1985 practice. "Beyond visual range" and "launch-and-leave" appear only to say
what the 1985 forces lacked. Two post-period items had slipped into 1985 text and were moved: the R-27R in section 1
point 4, and the MiG-23MLD's R-73 load. The R-73's presence with Soviet frontal units in 1985 stays open.
