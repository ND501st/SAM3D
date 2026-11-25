# Air combat tactics, 1980-85: research for the air battle

Research note, November 2026. The question: how did each faction's fighters fight each other in the early 1980s, and how
should SAM3D's air war decide when a fighter commits, how it flies the fight and when it leaves? The rule for this note is
the project owner's: **Cold War, not modern tactics**. Only what was in service or in doctrine by 1985 is used. Anything
later is listed in section 5 and kept out. The Gulf War of 1991 appears only as flagged hindsight, and nowhere as data.

Sources were read in full unless marked "snippet". Tier numbers follow `docs/SOURCES.md`. Where a US document describes
the Soviet side, it is the adversary's view and is marked so. Where a Soviet text describes the Israelis or the
Americans, the same applies the other way round.

Sources read for this note (details in section 6):

| Id | What | How read |
|---|---|---|
| `lambeth_1984` | RAND R-3000-AF, *Moscow's Lessons from the 1982 Lebanon Air War*, with a full translation of Dubrov 1983 | full |
| `dubrov_1983` | Col. V. Dubrov, «Авиация в ливанском конфликте», *Авиация и космонавтика* 9-10/1983 | full, in Lambeth's translation |
| `turchin_mt` | Gen.-Lt. I. Turchin, «Cooperation of fighter aviation and SAM troops», *Военная мысль* (late 1960s), CIA translation 1976 | full |
| `johnson_1986` | NPS thesis, *Initiative in Soviet Air Force Tactics and Decision Making*, June 1986 | most |
| `pennington_1985` | «Another Look at the Soviet Pilot», *Air Force Magazine*, April 1985 | full |
| `anderegg_2001` | *Sierra Hotel: Flying Air Force Fighters in the Decade after Vietnam*, USAF history, 2001 | chapters 2, 3, 6, 8, 15 |
| `lambeth_1985` | RAND P-7064, *Pitfalls in Fighter Force Planning*, Feb 1985 | parts |
| `vangilder_1979` | CGSC thesis, *Realistic Training: the Key to Success in Aerial Combat*, 1979 | Vietnam and training sections |
| `usn_falklands_1983` | US Navy, *Lessons of the Falklands: Summary Report*, Feb 1983 | full |
| `cordesman_1982` | «The Sixth Arab-Israeli Conflict», *Armed Forces Journal International*, Aug 1982 | full (OCR) |
| `gao_1985_id` | GAO testimony, *Aircraft Identification Programs*, 11 June 1985 | abstract only |
| `ru_wiki_mig23` | Russian Wikipedia, МиГ-23 | parts; pointer and tier 4 only |
| `combat_tree_refs` | forum and blog notes on Combat Tree | snippets |
| `riccioni_double_attack` | E. Riccioni, «The Double Attack System», c. 1964 | snippet |

Registered sources reused: `checo_1969_tactics`, `werrell_2005`, `grant_2002`, `paprcka_1984`, `soviet_iff`,
`afmag_1984_nato`, `mai_tactics_vvs`, `cia_1985_air_op`, `fm_100_2_1_1984`, `austrian_af_wiki`, `croatia_1991_migflug`.

---

## 1. Summary for SAM3D's design

1. **In 1980-85 the two blocs fought air battles in different ways, and the difference is the heart of the model.**
   NATO's fighters fought in pairs that each searched with their own radar, were helped by AWACS or ground radar, and
   decided for themselves when to commit. The Warsaw Pact's fighters were brought to the target by a controller on the
   ground (наведение по командам с земли), often by datalink to the autopilot on the MiG-23. The pilot's own initiative
   began only at contact, and the Soviet reforms that widened it date from 1978-84 and were patchy
   (`lambeth_1984`; `pennington_1985` pp. 83-90; `johnson_1986`).

2. **Beyond visual range shots were the exception in Central Europe, not the rule.** Without reliable IFF, NATO
   expected to stay bound to visual identification rules in most cases (`lambeth_1985`). The Mark XII was the only
   secure IFF and NATO had not agreed on the Mark XV (`gao_1985_id`). The F-15 pilots of the late 1970s often saw the
   target as a dot in the HUD box and could not identify it before Sparrow minimum range (`anderegg_2001` p. 161). So
   the default ROE in SAM3D for 1985 is VID, with BVR allowed only where the identification chain allows it: no friendly
   aircraft forward of the line, or a hostile declared by AWACS or ground control.

3. **All-aspect missiles were in service and they changed the fight.** The AIM-9L (chosen after AIMVAL-ACEVAL, 1977;
   in Israeli and British hands in 1982) and the AIM-7F made the head-on shot real. In the Bekaa most kills were by
   AIM-9L and only 7% were by guns (`lambeth_1984`). The fight "did not start at 1,000 feet" any more but thirty
   miles apart (`anderegg_2001` p. 163). The Pact's equivalents in 1985 were the R-23 (MiG-23M/ML) and the R-60.
   Export MiGs often had less (`lambeth_1984`, `cordesman_1982`).

4. **The basic NATO unit was the pair, flying line abreast.** By the mid-1970s the USAF had dropped Fluid Four and its
   fighting wing for Fluid Attack. In this formation the pair fly line abreast about a mile apart with an altitude stack
   of a couple of thousand feet. One fighter is "engaged" and the other "free", and the two swap roles to keep their
   energy (`anderegg_2001` pp. 59, 83-84). The US Navy's Loose Deuce and the F-104 community's Double Attack were the same
   idea. Radar targets were sorted by "left, low, lead" (`anderegg_2001` p. 160).

5. **The basic Pact unit was the pair, inside a group of four to eight.** The leader struck and the wingman covered:
   the "sword and shield". In 1982 Soviet writers wanted "every pilot both a shield and a sword", and the strike went to
   whichever pilot was better placed (`johnson_1986`). Turchin expected the enemy's first echelon in groups of 4-8
   and planned the rear-hemisphere attack: the fighter is brought to a departure position no closer than 6-8 km and
   launches at 4-6 km (`turchin_mt`).

6. **The Bekaa, June 1982, is the period's decisive lesson, and both sides drew it.** E-2Cs saw the Syrian MiGs leave
   their runways. The Syrian ground control was jammed out. The Israeli F-15s and F-16s were vectored onto the MiGs'
   beams, and the MiGs flew in pairs and singles in figure-eights (a Western attaché's account, quoted by Lambeth). Syria lost 23 (Lambeth) or 29 (Cordesman) on the
   first day and about 85 by the end of July. Israel lost none in air combat (`lambeth_1984`, `cordesman_1982`,
   `paprcka_1984`). Dubrov's Soviet reading: fighters freed from escort, radar control at depth and good training won it.
   He did not admit that the export MiGs were stripped down.

7. **Exchange ratios fall as numbers rise and as identification gets harder.** ACEVAL found that the exchange ratio "tends
   to diminish rapidly as the number of independently maneuvering participants goes up — regardless of qualitative
   differences" (`lambeth_1985`, after Watts 1979). In the knife fight, F-15s against six F-5s scored "distinctly less than
   1:1". The unseen adversary scores most. SAM3D should not let quality alone decide a large fight.

8. **Attackers answered interceptors by jettisoning, breaking or pressing on.** In Vietnam, 47 aircraft jettisoned their
   loads after MiG attacks in September 1967 (`checo_1969_tactics`). Argentine attackers over the Falklands, at the
   extreme of their range, "could not afford to maneuver" (`usn_falklands_1983`). Israeli strike groups flew close and
   split only before the target, with CAPs held apart from them (`dubrov_1983`). A strike flight's answer in SAM3D
   depends on its fuel state, its distance to the target and whether it has escort.

9. **For the design.** Each faction gets decision rules as data:
   - a commit rule: who decides, GCI or the flight leader;
   - an ROE: VID, or BVR when cleared;
   - a formation: line abreast pair, or pair and four under control;
   - a sort and shot discipline;
   - a disengage rule: fuel, weapons, numbers, loss of control.

   Skill sets how well each rule is kept (sort errors, shots out of parameters, the wingman lost). Stress sets how
   readily a pilot jettisons, drags or ejects. The fight itself runs on a point-mass energy and turn model. The type's
   numbers are an open item (section 7), and until they are found they are game values.

10. **Our side's fighters are the Soviet model, and their worst problem is our own SAMs.** In a Baltic Military
    District exercise, up to 25% of the targets "fired upon" by SAMs and AAA were our own aircraft (`turchin_mt`).
    Turchin argued for a single zone with colocated command posts, and rejected corridors as predictable. The battery's
    fire and our interceptors must share one picture through the regiment, or the battery will fire on friends.

---

## 2. Findings by faction

### 2.1 NATO

**nato-01 Pair line abreast; Fluid Four gone (USAF from about 1973-77).** The USAF's Fluid Four was two elements, the
element leaders 1-3 miles apart. Each wingman flew 1,500-2,500 ft behind his leader in a 45-60° cone ("fighting wing",
"welded wing"), and only the flight leader made the decisions and did most of the shooting (`anderegg_2001` pp. 20-21).
After the Aggressors arrived (64th Aggressor Squadron from late 1972), "Fluid Four ... died a quick and timely death".
The Fighter Weapons School adopted pairs line abreast about a mile apart with a stack of a couple of thousand feet. It
drew on the Navy's Loose Deuce, the F-104 community's Double Attack and the Six Pack (`anderegg_2001` pp. 82-84; the
OCR's page headers put these lines on pp. 82-83).
The pair turns without radio calls in "delayed 90s" and signals with a wing flash (`anderegg_2001` pp. 59-60).
Riccioni's Double Attack (c. 1964) had both members of the pair attack (`riccioni_double_attack`, snippet). Confidence
high (US view of its own forces).

**nato-02 Engaged and free fighter.** One fighter presses the bandit and the other manoeuvres away to pick an entry.
The engaged F-4 stays in until its speed bleeds, then uses its thrust to separate, and the free fighter, who has kept
his energy, comes in. "The two continuously switched roles so they did not have to slow down into the more agile
bandit's area of advantage" (`anderegg_2001` p. 84). Confidence high.

**nato-03 The sort and shot discipline (late 1970s, F-15).** If the targets were side by side the leader took the left
one, if stacked the low one, and if in trail the lead one: "left, low, lead". The wingman took the other
(`anderegg_2001` p. 160). The keys were "discipline, discipline, and discipline": in formation, radio, radar and shots.
Two radars on one target leave the other target free. Confidence high.

**nato-04 Visual identification bound the BVR shot (Vietnam to 1985).**
- *Vietnam.* During most periods, rules required Phantom crews to identify MiGs visually before shooting. The F-4 could
  fire a Sparrow at over ten miles head-on, but its head-on minimum range was nearly three miles. At 500 kt each, the
  closure is a mile every three seconds, so the MiG was inside minimum range before it could be identified
  (`anderegg_2001` p. 32).
- *Late 1970s.* TISEO gave the F-4E a 4× and 10× telescope slaved to the radar. The F-15 had no such telescope, so its
  pilots fitted rifle scopes ("Eagle Eye") beside the HUD. They were seen on F-15s at Langley and Bitburg
  (`anderegg_2001` pp. 33, 161).
- *1985.* "Without reliable IFF we will, in most cases, remain bound to visual rules of engagement" (`lambeth_1985`).

Confidence high.

**nato-05 IFF in NATO, 1980-85.**
- GAO (1985): identification beyond visual range is "a critical requirement". The Mark XII was the only secure system and
  was not totally accepted by NATO. There was only a tentative US-NATO agreement on the Mark XV frequency, and a joint
  office had been set up in 1980 (`gao_1985_id`, abstract only).
- GAO's 1982 report adds that the Mark X and XII were in NATO use with no agreement on the Mark XV band (snippet).

Confidence medium (abstract and snippet).

**nato-06 Combat Tree (1971-72, a few F-4D/E) (unverified: no period or official source found; the details rest on forum
and blog snippets only, and `anderegg_2001` mentions TISEO but not Combat Tree).** Combat Tree read the MiGs' own IFF
transponders. It allowed a BVR shot when no friendly aircraft were ahead. Only a few aircraft had it. Once a Tree "MiG"
turned out on TISEO to be an F-4 (`combat_tree_refs`, snippets, tier 4, low). Its status in Europe in 1985 is not known from the sources read. Design use:
a NATO pair may be given a "cooperative ID of the enemy" flag that allows a BVR shot. It is off by default.

**nato-07 AIMVAL-ACEVAL (Nellis, 1977): all-aspect missiles and numbers.**
- *The missiles.* AIMVAL chose the AIM-9L. Its cooled seeker and fuze made the head-on shot real: "shoot him in the face".
  The AIM-9Ls went first to the F-15s, and only a few F-4s were modified (`anderegg_2001` pp. 158-159).
- *The Red Air answer.* ACEVAL tested numbers. The F-5s, warned of a lock, turned away ("drag"). A pair flew close to look
  like one return: one dragged and the other pressed on unseen ("drag and bag") (`anderegg_2001` p. 160).
- *The dates.* Lambeth dates ACEVAL to 1976. Anderegg says 1977 (section 4).

Confidence high.

**nato-08 AIM-7F in the F-15 (from 1976-77).** In Langley's live firings over the Gulf of Mexico, the AIM-7F's success
rate was 4-5 times the F-4's Sparrow rate. The AIM-9L was close to "one shot, one kill" (`anderegg_2001` p. 163).
Dubrov gives the AIM-7F 5-50 km and says it breaks lock against violent manoeuvre (`dubrov_1983`). Confidence medium
(the success rates are not given as numbers).

**nato-09 The F-15's radar and search.** The F-15's pulse-Doppler radar could look down. Two Eagles in tactical formation
could search from the ground to 40,000 ft by overlapping their scans (`anderegg_2001` chapter 15, fig. 15.1). An F-15 squadron had 22
F-15As and 2 F-15Bs (chapter 15). The F-15 could lock on the nose to 50 nm and engage head-on, ROE permitting
(`lambeth_1984`). Dubrov gives the F-15's radar about 50 km against ground clutter and more than 80 km looking up, and
says the look-down range was half (`dubrov_1983`). Confidence medium (two views, consistent).

**nato-10 Detached CAP, not close escort (Israel 1982; NATO doctrine).**
- *The CAPs.* The F-15 and F-16 CAPs stood west of the Bekaa, away from the strike groups. An E-2C vectored them onto
  MiGs picked up as they took off. MiGs crossing a line set beforehand were attacked; Eitan said the line was the range
  of the missile batteries in Syria (`lambeth_1984`).
- *The layers.* The F-16s flew low and took target designation from the F-15s ("for the first time"). The E-2C and F-15s
  flew at medium height, and the 707 highest and farthest out to sea (`dubrov_1983`).
- *Cordesman's view.* An F-15 at the rear acted as a "mini-AWACS" (`cordesman_1982`).
- *Dubrov's judgement of the CAP.* It gave freedom of action but needed radar control beyond the strike zone. It was less
  reliable than escort, and it was exposed to SAMs.

Confidence high for the events, medium for the layering (Soviet reading).

**nato-11 Beam attacks, not BVR shots, in the Bekaa.** The IAF says it took no BVR shots. It used beam attacks set up by
the E-2C (`lambeth_1984`). The fights broke into small independent engagements, where speed, few and unpredictable
manoeuvres, and snap shots won. Creech gives about 40 kills to F-15s, 44 to F-16s and 1 to an F-4, most of them by
AIM-9L, with 7% by guns. Confidence high for the totals; the shares are Creech's, as cited by Lambeth.

**nato-12 The cardinal rules of a many-aircraft fight.** Keep speed up, avoid turning fights, stay unpredictable
(`lambeth_1984`). ACEVAL: "engagement exchange ratios tend to diminish rapidly as the number of independently maneuvering
participants goes up — regardless of qualitative differences" (`lambeth_1985`, after Watts, *TOPGUN Journal*, Fall/Winter
1979, p. 24). Aggressor Maj. Lee Harrell flew six F-5s against four to six F-15s. In the knife fight the F-15s did
"distinctly less than 1:1". With no VID required they did "slightly better than 1:1"; "the Eagle died like everyone else"
(`lambeth_1985`). At TOPGUN in August 1980, about two shots were called for every shot observed: the unseen adversary
scores most. Confidence high (US training data).

**nato-13 Low level under the weather, line abreast (USAFE, mid-1970s to 1985).** USAF tacticians expected Europe's
weather and the Pact's medium-altitude SAMs to put them low. The ceiling was below 3,000 ft, and visibility under 3 miles,
for more than half the year. Low formations spread out line abreast so that each aircraft could watch for MiGs and SAM
launches. Radio was assumed jammed by Soviet jammers travelling with the troops (`anderegg_2001` pp. 60-61). The same
source puts the Pact's fighters at two to one against NATO's, in the US view. Confidence high (US view).

**nato-14 The Aggressors and Red Flag (from 1972-75).** These were dissimilar air combat training squadrons, first on
T-38s and then on F-5Es. They "flew enemy tactics they knew the Soviets used" and brought a GCI controller who
"controlled Aggressor aircraft in the same way the Soviets relied on their GCI" (`anderegg_2001` pp. 78-79). F-4 crews
learned to have the back-seater call the defence ("check left and extend", "break", "guns break"). Against the small,
agile T-38, the check-and-extend often failed: the attacker came in fast and then slowed to turn inside for a gun shot
(pp. 84-85). Confidence high. Design use: NATO crews in 1985 are trained against GCI-led Pact tactics and expect them.

**nato-15 Falklands, 1982: the outer CAP and its limits.**
- *The force.* 28 Sea Harriers flew more than 1,200 sorties in 44 days at about 90% availability, 1,100 of them air
  defence. The outer defence was rarely more than four aircraft. Each carried 2 AIM-9L and 2 Aden 30 mm for a 90-minute
  mission with about 20 minutes on station.
- *The results.* At least 20 Argentine aircraft were destroyed, 16 by Sidewinder, and no Harrier was lost in air combat.
- *The limit.* There was no radar-controlled cueing and vectoring.

(`usn_falklands_1983`.) Confidence high (US official view of the UK).

**nato-16 Bingo and joker.** No number for fuel states (joker, bingo) was found in the period sources read. The
Falklands figures (90 minutes in all, about 20 minutes on station) are the only fuel-bound engagement window
found. Game values are needed (section 7).

**nato-17 RAF and Luftwaffe practice.** Not found in the sources read. RAF Germany's Phantoms and Lightnings, the
Luftwaffe's F-4Fs and the Dutch F-16s are open items. Until they are found, use the USAF pair rules (nato-01 to nato-03)
for all NATO air forces. The F-15s at Bitburg are confirmed in `anderegg_2001`, as is a Bitburg F-15 squadron commander.

### 2.2 Warsaw Pact

**wp-01 GCI close control, and the mission aborted when it was lost (Western view, to 1983).** "Soviet doctrine requires
most missions under positive ground control, and the mission is aborted if contact is lost" (Skinner 1983, in
`lambeth_1984`). In Lambeth's reading the KP's flight director (руководитель полётов) does the thinking that a Western
pilot does in the cockpit. Lew Allen (1979) called the Soviets "fairly predictable". Confidence medium: this is the
adversary's view, but Soviet critics say the same (wp-03).

**wp-02 The Lazur datalink and Vozdukh-1 (MiG-23S/M/ML; MiG-21 variants).** The MiG-23S, M and ML carried the Lazur-S,
-SM and -SML command link to the ground's automated guidance system (named «Воздух-1» by `ru_wiki_mig23`, a pointer
only). With the autopilot (САУ) it brought the fighter to the target on the ground system's commands (`airwar_ru`,
MiG-23ML page). Radars: the Sapfir-23D (MiG-23M) detects a typical target at 55 km and locks at 35 km; the Sapfir-23ML
(MiG-23ML) at 85 and 55 km (`airwar_ru`, MiG-23M and MiG-23ML pages; "typical target", not "MiG-21-sized"). **Corrected
(15 Nov 2026):** the MiG-23ML was built for the Soviet air force in 1976-81 and for export until 1985; the MLD's series
production began in 1984 (`airwar_ru`, MiG-23ML and MiG-23MLD pages), not 1978. Other accounts give 1982 for the MLD
(unverified). For 1985, the Pact's front-line MiG-23s are ML, with MLD just entering units. The MLD's wing sweep
positions of 16, 32, 45 and 72° (unverified: Wikipedia only). Confidence medium (tier 4; the Lazur's technical
description is an open item).

**wp-03 Initiative: written up from 1978, and only partly lived (1978-85).**
- *Dubrov's 1978 series.* Dubrov's series "How Has Air Combat Changed?" (*Авиация и космонавтика*, 1978) went through
  the search, closing, attack, manoeuvre and disengagement phases, analysed decoy, reserve and strike groups, and put
  "heavy stress on the importance of GCI directives" (`lambeth_1984`). Pennington says the Soviets had written of the
  re-emergence of manoeuvring air combat since the early 1970s (`pennington_1985`). **Corrected (15 Nov 2026):** the
  series did not by itself "restore" manoeuvring combat, and "seven-part" is unverified.
- *Two-sided training.* In 1983 the chief of combat training wanted two-sided exercises, "fighters against fighters"
  (`pennington_1985`).
- *The PVO's 1984 reform.* In May 1984 Moskvitelev, the commander of PVO aviation, announced new programmes for
  manoeuvring air combat, the interception of cruise missiles and EW. He called it a "radical overhaul", and complained of
  commanders waiting for "prompting from the top". Squadrons were to train against "specially trained aircrews flying in
  the role of adversary ... in flights of up to four aircraft" (`pennington_1985`, quoting an internal PVO publication).
- *The ebb.* Johnson's count of Soviet journals shows initiative peaking in print in 1973 and 1982 and falling in 1983-84.
  A pilot was put on probation in 1979 for overstressing his aircraft in a dogfight he won. A cadet was grounded in 1983
  for two rolls more than the plan, caught by the SARPP flight recorder. By 1984 the free-hunt articles no longer
  mentioned initiative (`johnson_1986`).

Confidence high for the existence of the reform, medium for how far it reached in 1985.

**wp-04 Initiative passes to the flight leader at contact (1984).** The KP leads the search for the target. "When the
aircraft come in direct contact ... initiative ... [is] transferred mainly to the flight leaders" (`pennington_1985`).
Controllers who attended the pilots' ground training halved the commands they had to repeat by radio. Pilots trained in
heavy jamming, and kept their radars off until close for surprise. Pilots who failed while waiting for GCI were rebuked.
Confidence medium.

**wp-05 The pair: sword and shield, then both (1982).** The year-long debate in the press on pair against single ended
with the majority keeping the pair, and with wingmen equal to leaders (`pennington_1985`). "Every pilot is both a shield
and a sword" is from a Soviet article Johnson cites (his bibliography has «Щит и меч», *А и К*, 1982); the strike made "by
the pilot in the more advantageous position" is Golubev's, chief of VVS combat training (`johnson_1986`). Corrected
attribution (15 Nov 2026): the report had given both to Golubev. Training ran from theory to solo aerobatics, solo combat manoeuvring, the pair, the
four and the tactical exercise. Training attackers struck one target as a pair and rarely split. The scenario was
arranged beforehand, and targets were changed after take-off only from the 1980s (`johnson_1986`, `pennington_1985`).
Keep the height advantage and cover a group's attack in the vertical (Babich and Dubovitskiy, A&K 1977, in
`johnson_1986`). Confidence medium (US reading of Soviet journals).

**wp-06 Fighter echelons and attack geometry (Turchin, 1967-68, a reply to Levchenko in *Военная мысль* No. 3 (82),
1967; whether it was still the frame in the 1980s is an inference, not in the source).**
- *The enemy's first echelon.* Groups of 4-8.
- *Our three echelons.*
  - The first is forward of the front, fighters only, on independent search. They break off 25-30 km into enemy
    territory and report by radio.
  - The second runs to 80-100 km depth: SAMs, plus airborne-alert zones at 1,500-2,000 m (for low targets) and 10-12 km
    (for high targets), centred 50-60 km inside our own territory beyond the Nike-Hercules' reach.
  - The third is alert on the airfield.
- *The attack.* In a rear-hemisphere attack the fighter is brought to a departure position no closer than 6-8 km and
  launches at 4-6 km. Against low targets, control is only "assistance in maintaining the general combat formation".
- *The figure.* Its key gives the reserve in the air at 5-6 km, the jammers at 5-10 km, tactical aviation's overflight
  zone at 50-100 m, the MiG-21 at 1.5-2 km and 10-11 km, and the SAM kill zone at 3-23 km.

(`turchin_mt`, tier 1 Soviet.) Confidence high for the doctrine of its date. Its weapons are the R-3S and R-13 era.

**wp-07 Fighters and SAMs: one zone, and fratricide.** Turchin argues for a single zone, against cooperation by zones,
with the fighter division's guidance post colocated with the SAM regiment's or brigade's command post, under the air
army. He rejects corridors as predictable. In a Baltic MD exercise, up to 25% of the targets "fired upon" by SAMs and AAA
were our own aircraft. He wants interrogators on every fire control radar and blocking within a 600 m radius. All-aspect
weapons would need even closer coordination (`turchin_mt`). The Soviet interrogators of the period were Kremniy-2 and
Parol (`soviet_iff`). Confidence high.

**wp-08 The Soviet reading of the Bekaa (Dubrov, 1983).** This is a Soviet view of the Israeli side.
- *The E-2C.* It sat about 100 km offshore and saw more than 300 km looking down, with a pair of F-15s as direct cover.
- *Search.* Pairs searched with the lead looking into the upper hemisphere and the wingman the lower, line abreast
  about 1,000 m apart, "the overall sectors of search being 60 degrees up and down" (corrected 15 Nov 2026: elevation
  sectors, not ±60° in azimuth). The fighters stayed under the E-2C's radar control. Radio discipline was kept.
- *The attack.* It came from below and head-on, with decoy and strike groups. Dubrov called the all-aspect attack "the
  wave of the future". Attacks from the forward hemisphere without a turn into a manoeuvring fight were not typical.
- *Control at depth.* "Radar control over fighters weakens with increasing depth into enemy territory", and air
  superiority is won by taking the battle to the enemy.
- *Hunting and defence.* "It is deemed unwise" for an aircraft with expensive external guided weapons to search
  independently where air defences have not been suppressed; target hunting was left to RPVs. Corrected (15 Nov 2026):
  this is about strike aircraft hunting ground targets, not about fighters' free hunt. RWR, automatic flares and chaff
  were used.
- *His lesson.* The pilot's preparation is what is left. There is nothing "new in principle" in the training.

(`dubrov_1983` in `lambeth_1984`.) Dubrov wrongly treated the E-2C as an airborne KP giving "directives"
(`lambeth_1984`). Confidence high that this is what the Soviet press taught in 1983.

**wp-09 Syrian (Pact-trained client) behaviour in 1982.**
- *The aircraft.* The Syrians flew export MiG-23s without the R-23 (AA-7) or the High Lark radar, and MiG-21s, with at
  best the R-60 (`lambeth_1984`, `cordesman_1982`).
- *Communications.* Their communications were massively jammed and they lost contact with GCI. The Israelis used
  jam-resistant radios and a datalink (`lambeth_1984`). Czechoslovak analysis also notes the voice nets jammed
  (`paprcka_1984`).
- *Behaviour.* Under attack they split into pairs and singles and flew figure-eights (a Western military attaché, quoted
  in `lambeth_1984`). Some "simply ejected the moment they knew that the Israelis had spotted them" (a footnote in
  `lambeth_1984`; the attribution to Roberts 1983 was not checked). They flew with more courage than in 1973 (`cordesman_1982`).

Confidence high for the pattern. Design use: a Pact-trained force that loses GCI degrades into predictable pairs.

**wp-10 Afghanistan (1979-85): decentralisation, then control again.** In the mountains the pair was the unit, and
decisions moved down to it. Controllers were then brought back (`johnson_1986`). Confidence low (brief mention).

**wp-11 MiG-23 against Western fighters.**
- *Turning and speed.* The MiG-23 turns worse than a hard-wing F-4, but exceeds the F-5 in acceleration and speed
  (`lambeth_1985`).
- *Energy (post-period hindsight).* The Israeli evaluation was of the Syrian MiG-23ML flown to Israel in **1989**, tested
  against the F-16C; it found the MiG-23ML accelerated better (`airwar_ru`, MiG-23ML page). The aircraft existed by 1985,
  so this is hindsight about equipment only, and it is moved to section 5. The "above 900 km/h" and "small edge in
  energy manoeuvrability" figures are unverified (Wikipedia only).

Confidence medium. Design use: the MiG-23 is a slashing, one-pass fighter. Turning is the Western fighter's game.

### 2.3 Neutral

**neu-01 The neutral neighbour in 1985 has no missile-armed interceptor.** On the Austrian analogue, the 1980s air force
was 40 Saab 105Ö light jets and some two dozen AB 212 helicopters. The Drakens arrived only in 1988 and carried guns only
under a guided-missile ban (`austrian_af_wiki`, via `ENEMY_AIR_RESEARCH_2026-10.md` section 2.2). In 1985 its "fighters"
are light jets with guns, so they fight WVR only, by day, always with visual identification, and in pairs or single
aircraft under ground control. No period source on their air combat tactics was read. Confidence low. Design use: the
neutral avoids air combat with modern fighters and breaks off on any threat.

### 2.4 Rebels

**reb-01 Rebels have no fighters.** On the Croatian 1991 analogue, the rebels' air arm is crop sprayers and light
aircraft (`croatia_1991_migflug`, low). The analogue is itself post-period (1991) and is used only for the faction's
shape, not its tactics. Against fighters a rebel aircraft can only hide low, run, or land. Confidence low.

### 2.5 Our side (a Pact-equipped national air defence)

**our-01 Our fighters follow Soviet control practice.** Our side's interceptors are brought in by GCI or Lazur from the
regiment's or sector's KP, and their own initiative begins only at contact. The 1984 reforms are the best case
(`pennington_1985`, `turchin_mt`). Lose the KP, or have the voice nets jammed, and they fall back to independent search
in pairs, or abort (wp-01, wp-09). Confidence medium.

**our-02 Our SAMs and our fighters share a zone at the risk of fratricide.** See wp-07: up to 25% of targets engaged in
an exercise were our own. The battery must know where our fighters are, through the regiment and the PU-12M, before it
fires. Interrogation (Parol) is one means and not enough on its own (`turchin_mt`, `soviet_iff`). Confidence high.

**our-03 Our fighters' kills reach the battery only through the reports.** This follows the project's first-person rule.
Air combat in the war is resolved headless, and its result reaches the battery only through the pipeline. Not a period
finding: a design note.

### 2.6 General (both sides, background)

**gen-01 Exchange ratios in the period's wars.**

| War | Ratio | Source |
|---|---|---|
| Korea | about 10:1 (his source says 14:1 and "not too accurate") | `vangilder_1979` |
| Rolling Thunder, 1965-68 | about 2.3:1 for both services | `vangilder_1979` |
| 1970-73 | 72 MiGs for 28 US losses: USAF 1.88:1, USN 12.5:1 | `vangilder_1979`, after de Leon, RAND R-2070, 1977, pp. 10-12 |
| Linebacker I (9 May to 23 Oct 1972) | 69 MiGs shot down | `vangilder_1979` |
| Linebacker II | 8 of the 32 MiGs launched were shot down | `vangilder_1979` |
| Bekaa 1982 | about 85 to 0 by the end of July | `lambeth_1984` |
| Falklands 1982 | at least 20 to 0 | `usn_falklands_1983` |

MiGs caused 3% of US losses in Vietnam overall and 8% over the North (`werrell_2005`). Confidence high for the US
figures, low for Korea.

**gen-02 Training beats the aircraft.** Ritchie: "a superior pilot in an inferior aircraft will defeat an inferior pilot
in a superior aircraft". Cunningham had 150 ACM flights before his first MiG, and the Navy's kills per engagement rose to
1.04 after Top Gun's DACT against A-4s (`vangilder_1979`). This is the base for making skill weigh more than the type.

**gen-03 One-circle and two-circle fights, the merge, E-M.** The sources read use the words (the merge, pre-merge,
post-merge, `anderegg_2001` p. 162) but give no numbers for turn performance. Shaw's *Fighter Combat* (1985) is the
period text and was not readable (lending only). The point-mass model must use turn rates and specific excess power as
game values until type data are found (section 7).

**gen-04 Helicopters against fighters.** Nothing was found in the sources read. J-CATCH (1978-79) is the period test and
an open item.

---

## 3. Numbers usable as data

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| Fluid Four: element leaders apart | 1-3 miles (1.6-4.8 km) | NATO (USAF) | to c. 1975 | `anderegg_2001` pp. 20-21 | high |
| Fluid Four: wingman behind leader | 1,500-2,500 ft (460-760 m), in a 45-60° cone | NATO (USAF) | to c. 1975 | `anderegg_2001` p. 21 | high |
| Fluid Attack / line abreast: spacing | about 1 mile (1.6 km) | NATO | from mid-1970s | `anderegg_2001` pp. 59, 83 | high |
| Line abreast: altitude stack | "a couple of thousand feet" (about 600 m) | NATO | from mid-1970s | `anderegg_2001` p. 84 | medium |
| Pair search (Israeli, Soviet reading): spacing and sectors | about 1,000 m line abreast; search sectors 60° up (lead) and 60° down (wingman) | NATO-like (Israel) | 1982 | `dubrov_1983` | medium |
| Sparrow (F-4 era) head-on max range | over 10 miles (16 km) | NATO | Vietnam | `anderegg_2001` p. 32 | medium |
| Sparrow head-on min range | nearly 3 miles (4.8 km) | NATO | Vietnam | `anderegg_2001` p. 32 | medium |
| Head-on closure, both at 500 kt | 1 mile per 3 s (about 515 m/s) | general | — | `anderegg_2001` p. 32 | high |
| AIM-7F envelope | 5-50 km | NATO | 1982 | `dubrov_1983` | medium |
| F-15 radar against clutter | about 50 km | NATO | 1982 | `dubrov_1983` | medium |
| F-15 radar looking up | more than 80 km | NATO | 1982 | `dubrov_1983` | medium |
| F-15 lock on the nose | to 50 nm (93 km) | NATO | 1984 | `lambeth_1984` | medium |
| F-15 squadron | 22 F-15A + 2 F-15B | NATO (USAF) | late 1970s | `anderegg_2001` ch. 15 | high |
| E-2C stand-off and look-down range | about 100 km offshore; more than 300 km | Israel | 1982 | `dubrov_1983` | medium |
| E-2C surveillance range | 200 miles | Israel | 1982 | `lambeth_1984` | medium |
| Aircraft airborne at once, Bekaa | about 90 Israeli, 60 Syrian | Israel, Syria | 9 June 1982 | `lambeth_1984` | medium |
| Syrian losses, first day | 23 (Lambeth) or 29 (Cordesman) | Syria | 9 June 1982 | `lambeth_1984`, `cordesman_1982` | medium |
| Syrian losses to end of July 1982 | about 85; Israeli air-to-air losses 0 | Syria, Israel | 1982 | `lambeth_1984` | high |
| Bekaa kills by type | F-15 about 40, F-16 44, F-4 1 | Israel | 1982 | `lambeth_1984` (Creech) | medium |
| Bekaa gun kills | 7% | Israel | 1982 | `lambeth_1984` | medium |
| Israeli strike dive | entry 2,500-3,000 m, pull-out 1,500 m, single pass | Israel | 1982 | `dubrov_1983` | medium |
| Sea Harrier outer CAP | rarely more than 4 aircraft | UK | 1982 | `usn_falklands_1983` | high |
| Sea Harrier load | 2 AIM-9L + 2 Aden 30 mm | UK | 1982 | `usn_falklands_1983` | high |
| Sea Harrier mission / time on station | 90 min / about 20 min | UK | 1982 | `usn_falklands_1983` | high |
| Falklands air-to-air | at least 20 Argentine down, 16 by AIM-9L; 0 Harriers | UK, Argentina | 1982 | `usn_falklands_1983` | high |
| Argentine attack window | daylight only, about 8 h in 24 | Argentina | 1982 | `usn_falklands_1983` | high |
| Enemy first echelon group | 4-8 aircraft | (as Pact saw NATO) | 1967-68 | `turchin_mt` | high |
| Independent search break-off | 25-30 km into enemy territory | Warsaw Pact | late 1960s | `turchin_mt` | high |
| Second echelon depth | 80-100 km | Warsaw Pact | late 1960s | `turchin_mt` | high |
| Airborne alert zones | 1,500-2,000 m and 10-12 km altitude; centre 50-60 km inside own territory | Warsaw Pact | late 1960s | `turchin_mt` | high |
| Rear-hemisphere attack: departure / launch | no less than 6-8 km / 4-6 km | Warsaw Pact | late 1960s | `turchin_mt` | high |
| Fratricide in exercise | up to 25% of targets engaged were own aircraft | Warsaw Pact | late 1960s | `turchin_mt` | high |
| Proposed blocking radius | 600 m | Warsaw Pact | late 1960s | `turchin_mt` | medium |
| Sapfir-23D detect / lock (typical target) | 55 km / 35 km | Warsaw Pact | MiG-23M, 1970s | `airwar_ru` (MiG-23M) | low |
| Sapfir-23ML detect / lock (typical target) | 85 km / 55 km | Warsaw Pact | MiG-23ML, built 1976-81 (USSR), to 1985 (export) | `airwar_ru` (MiG-23ML) | low |
| MiG-23MLD series production | from 1984 (other accounts 1982, unverified) | Warsaw Pact | 1984 | `airwar_ru` (MiG-23MLD) | low |
| MiG-23MLD sweep positions | 16, 32, 45, 72° (unverified: Wikipedia only) | Warsaw Pact | 1980s | `ru_wiki_mig23` (pointer) | low |
| Pact fighters against NATO fighters | about 2:1 (US view) | both | 1975-89 | `anderegg_2001` p. 61 | medium |
| European weather | ceiling below 3,000 ft and visibility under 3 miles more than 50% of the year | general | 1970s-80s | `anderegg_2001` p. 61 | medium |
| Shots called for each shot observed | about 2 | NATO (US Navy) | Aug 1980 | `lambeth_1985` | medium |
| F-15s against 6 F-5s, knife fight | less than 1:1 | NATO training | c. 1980 | `lambeth_1985` | medium |
| Korea exchange ratio | about 10:1 (14:1 disputed) | general | 1950-53 | `vangilder_1979` | low |
| Rolling Thunder exchange ratio | about 2.3:1 | US | 1965-68 | `vangilder_1979` | medium |
| 1970-73 exchange ratios | USAF 1.88:1, USN 12.5:1 (72 MiGs for 28) | US | 1970-73 | `vangilder_1979` | high |
| Linebacker II | 8 of 32 MiGs launched shot down | US, NVN | Dec 1972 | `vangilder_1979` | medium |
| Jettisons after MiG attacks | 47 aircraft | US | Sept 1967 | `checo_1969_tactics` | high |
| MiGs' share of US losses | 3% overall, 8% over NVN | US | 1965-73 | `werrell_2005` | high |

---

## 4. Disagreements and which to use

1. **The Bekaa's first day: 23 or 29 Syrian aircraft.** Lambeth gives 23 on 9 June and 15 the next day. Cordesman gives
   "29 Syrian MiGs without a single loss" in the first attack. Use Lambeth (later and more careful). The total of about 85
   by the end of July agrees in both.
2. **The Syrian claims against the Israeli record.** Red Star claimed 67 Israeli aircraft, Zamyatin "up to 40", and
   Syrian MiG-23 claims say 7 for 10. Israel admits no air-to-air losses (a drone only). Use the Israeli figure for what
   happened. Keep the claims as the kind of thing a Pact side's records say (the written record rule).
3. **How bound the Soviets were to GCI in 1985.** Western writers (Skinner 1983, Allen 1979) say bound and predictable.
   Pennington (1985) shows a reform under way. Johnson (1986) shows its talk ebbing by 1984. Use a skill-dependent rule:
   the average Pact unit in 1985 is GCI-bound, aborting or reverting to rote pairs when it loses control, and the best
   units (PVO, after 1984) pass initiative to the flight leader at contact.
4. **What the E-2C was.** Dubrov saw it as an airborne KP giving directives. Lambeth says the F-15 could search on its own
   and that the E-2C advised. Use Lambeth for NATO's actual practice. Use Dubrov for what a Pact staff believes, which
   matters for the enemy's and our own intelligence.
5. **The date of AIMVAL-ACEVAL.** Lambeth (1985) cites ACEVAL as 1976. Anderegg (2001) says 1977. Both are before the
   period's start. Use 1977 for the conclusions and note the gap.
6. **When the MiG-23MLD entered series.** The report first said 1978 (from Wikipedia). `airwar_ru` gives the ML's
   Soviet production as 1976-81 and the MLD's series from 1984; other accounts say 1982. Use the ML as the Pact's
   standard MiG-23 in 1985, with the MLD only in a few units.
7. **Iran-Iraq MiG-23 losses.** Iran claimed 78 MiG-23s. Iraq admitted 43 lost from all causes (`ru_wiki_mig23`, tier 4).
   There is no period source. Use neither as data.
8. **Korea's exchange ratio: about 10:1 or 14:1.** Van Gilder's own source says 14:1 is "not too accurate". Background
   only.

---

## 5. Post-period items excluded

| Item | Date | Why excluded |
|---|---|---|
| AIM-120 AMRAAM (fire and forget) | IOC 1991 | after 1985 |
| JTIDS / Link 16 in fighters | late 1980s onwards | after 1985 |
| LANTIRN, night low-level attack with FLIR | from 1987 | after 1985 |
| NVG strikes | late 1980s onwards | after 1985 |
| "Killer scouts" | 1991 | after 1985 (Gulf War, hindsight only) |
| SEAD/DEAD as practised after 1991 | 1991 onwards | after 1985 |
| R-77 | 1990s | after 1985 |
| R-73 with helmet sight in large numbers; MiG-29 and Su-27 tactics | MiG-29 units from 1983-84 (to be checked); Su-27 from 1985 | at or past the edge; not used as typical of 1985 |
| A-50 Mainstay in service | 1984-85 (edge) | not used for 1985 Pact control |
| "Fox three" for an active radar missile | with AMRAAM, 1990s | brevity of 1985 not verified |
| F-16 against MiG-23 incidents on the Afghan-Pakistan border | 1986-88 | after 1985 |
| Angola air combats (Mirage F1 against MiG-23ML) | 1987-88 | after 1985 (kept only as a validation case elsewhere) |
| Israeli evaluation of a defected Syrian MiG-23ML against the F-16C | 1989 | after 1985: hindsight about an aircraft that existed in 1985, not data |
| MiG-23MLD as the Pact's standard MiG-23 | series from 1984 | at the edge: the ML is the 1985 standard |
| Austrian Drakens | 1988 | after 1985: the neutral has none in 1985 |
| Croatian air arm (the rebels' analogue) | 1991 | after 1985: its shape is used, not its tactics |
| Gulf War air combat | 1991 | hindsight only |
| Kosovo | 1999 | after 1985 |

---

## 6. Sources

| Id | Tier | Citation | URL | Full or snippet | Whose view |
|---|---|---|---|---|---|
| `lambeth_1984` | 4 | B. S. Lambeth, *Moscow's Lessons from the 1982 Lebanon Air War*, RAND R-3000-AF, September 1984 (DTIC ADA148310) | https://archive.org/details/DTIC_ADA148310 | full | US analyst, on the Soviet reading |
| `dubrov_1983` | 1 | Col. V. Dubrov, «Авиация в ливанском конфликте», *Авиация и космонавтика* No. 9-10, 1983, pp. 46-47, translated in `lambeth_1984` | (in `lambeth_1984`) | full, in translation | Soviet, on Israel |
| `turchin_mt` | 1 | Gen.-Lt. of Aviation I. Turchin, «Cooperation of Fighter Aviation and Surface-to-Air Missile Troops», *Военная мысль*, special collection, 1967-68 (a reply to Levchenko in No. 3 (82), 1967); CIA translation 11 Nov 1976 (CIA-RDP10-00105R000302550001-1) | https://www.cia.gov/readingroom/document/cia-rdp10-00105r000302550001-1 | full | Soviet |
| `johnson_1986` | 4 | J. S. Johnson, *Initiative in Soviet Air Force Tactics and Decision Making*, Naval Postgraduate School thesis, June 1986 (DTIC ADA171772) | https://archive.org/details/DTIC_ADA171772 | most | US reading of Soviet journals |
| `pennington_1985` | 4 | Capt. Rana J. Pennington, USAF, «Another Look at the Soviet Pilot», *Air Force Magazine*, April 1985, pp. 83-90 | https://www.airandspaceforces.com (April 1985 issue PDF) | full | US, quoting Soviet sources |
| `anderegg_2001` | 2 | C. R. Anderegg, *Sierra Hotel: Flying Air Force Fighters in the Decade after Vietnam*, Air Force History and Museums Program, 2001 (DTIC ADA476975) | https://archive.org/details/DTIC_ADA476975 | chapters 2, 3, 6, 8, 15 | USAF veteran, written as history |
| `lambeth_1985` | 4 | B. S. Lambeth, *Pitfalls in Fighter Force Planning*, RAND P-7064, February 1985 (DTIC ADA156809) | https://archive.org/details/DTIC_ADA156809 | parts | US |
| `vangilder_1979` | 4 | Maj. M. L. Van Gilder, *Realistic Training: The Key to Success in Aerial Combat*, CGSC MMAS thesis, June 1979 (DTIC ADA076937) | https://archive.org/details/DTIC_ADA076937 | Vietnam and training sections | US |
| `usn_falklands_1983` | 1 (a declassified official document, but a US observer's view of British practice, not the operator's) | Department of the Navy, Office of Program Appraisal, *Lessons of the Falklands: Summary Report*, February 1983 (DTIC ADA133333) | https://archive.org/details/DTIC_ADA133333 | full | US official, on the UK |
| `cordesman_1982` | 4 | A. H. Cordesman, «The Sixth Arab-Israeli Conflict: Military Lessons for American Defense Planning», *Armed Forces Journal International*, August 1982 (CIA-RDP93B01478R000300040006-7) | https://www.cia.gov/readingroom/document/cia-rdp93b01478r000300040006-7 | full (OCR) | US press |
| `gao_1985_id` | 1 | US GAO, testimony «Aircraft Identification Programs», 11 June 1985, product 127114 | https://www.gao.gov/products/127114 | abstract only | US official |
| `ru_wiki_mig23` | 4 | Russian Wikipedia, «МиГ-23» | https://ru.wikipedia.org/wiki/МиГ-23 | parts | Russian reference; pointer only, never the cited source (its figures now cited to `airwar_ru` where found there) |
| `airwar_ru` | 4 | (registered) «Уголок неба» (airwar.ru), pages МиГ-23М, МиГ-23МЛ, МиГ-23МЛД (read 15 Nov 2026 in saved copies) | http://www.airwar.ru | the three pages | Russian reference |
| `combat_tree_refs` | 4 | forum and blog notes on the APX-81 Combat Tree | — | snippets | US enthusiasts; low |
| `riccioni_double_attack` | 2 | Capt. E. Riccioni, «The Double Attack System», c. 1964, Boyd collection, USMC University | — | snippet | USAF |
| `checo_1969_tactics` | 1 | (registered) | see SOURCES.md | as registered | US |
| `werrell_2005` | 4 | (registered) | see SOURCES.md | as registered | US |
| `paprcka_1984` | 1 | (registered) *Vojenská mysl* 7/1984 on the Bekaa | see SOURCES.md | as registered | Czechoslovak |
| `soviet_iff` | 4 | (registered) Kremniy-2, Parol | see SOURCES.md | as registered | — |
| `austrian_af_wiki` | 4 | (registered) | see SOURCES.md | as registered | — |
| `croatia_1991_migflug` | 4 | (registered) | see SOURCES.md | as registered | — |

---

## 7. Open items and documents to acquire

1. MAI, *Тактика ВВС*, section 3.1.3 on fighters (`mai_tactics_vvs`). The site's TLS certificate fails and was not
   bypassed. It is also post-Soviet: use only for the doctrine it says is Soviet.
2. *Военная мысль*, 1975, «Fighter aviation in the front air defense» (CIA-RDP10-00105R000201080001-5). The PDF is
   damaged and has no text. Ask for another copy.
3. Dubrov's 1978 series «Как изменился воздушный бой?» in full: in the original *Авиация и космонавтика*, or in
   translation in the USAF Intelligence Service's *Soviet Awareness Red Eagle Reader* and *Fighter Weapons Review*
   (Spring 1981, pp. 23-27).
4. Pennington, «Closing the Tactics Gap», *Air Force Magazine*, March 1984, p. 83.
5. Moskvitelev's May 1984 PVO publication, if any translation exists.
6. R. L. Shaw, *Fighter Combat: Tactics and Maneuvering* (1985). This is the period text for one-circle and two-circle
   fights, the merge, E-M and section tactics. It is lending-only on archive.org.
7. de Leon, RAND R-2070 (1977), the Vietnam exchange ratios at source.
8. Red Baron III (WSEG Report 116, IDA), downloaded but not analysed: the causes of losses in Vietnam air combat.
9. AIMVAL and ACEVAL final reports, or their public summaries.
10. GAO 1985 in full, and GAO C-MASAD-82-6 (1982), on NATO IFF.
11. FM 100-2-1 (1984) on the Pact's air operations (`fm_100_2_1_1984`), to set wp-01 beside the US Army's view.
12. M. Spick, *The Ace Factor* (1983) / *Modern Fighting Aircraft* (period), for turn and energy data, used only as
    pointers.
13. Skinner, *USAFE: A Primer of Modern Air Combat in Europe* (1983).
14. The Lazur and Vozdukh-1 technical description: the datalink's ranges, update rates and the commands it carries.
15. Soviet practical aerodynamics manuals (Практическая аэродинамика) for the MiG-21 and MiG-23. These give turn rates,
    sustained g and specific excess power, which the point-mass model needs for each type.
16. J-CATCH (1978-79): helicopters against fighters.
17. RAF Germany (Phantom, Lightning, Harrier), Luftwaffe F-4F and Dutch F-16 tactics, 1980-85.
18. A period source on Iran-Iraq air combat (1980-85).
19. Joker and bingo fuel practice in NATO and the Pact, as numbers.
20. NATO brevity words in use in 1985 (Fox one and two; whether "Fox three" then meant the gun).

---

## Verification (15 Nov 2026)

An adversarial check of the most load-bearing findings against the sources' own texts: `anderegg_2001`, `lambeth_1984`
(with Dubrov's translation) and `lambeth_1985` read through archive.org's full text and saved copies; `turchin_mt`,
`usn_falklands_1983`, `vangilder_1979`, `johnson_1986`, `pennington_1985` and `cordesman_1982` in the saved OCR texts;
`gao_1985_id` on GAO's product page; `airwar_ru`'s MiG-23M, ML and MLD pages in saved copies. Web search was not
available for this check (its budget was used up), so nothing new was found by searching; Combat Tree and the Austrian
analogue were not re-sourced.

| Check | Verdict | Note |
|---|---|---|
| nato-01 Fluid Four to Fluid Attack | confirmed | Anderegg p. 20 (wingman 1,500-2,500 ft, 45-60° cone, "fighting wing"); p. 59 (line abreast about a mile apart, "died a timely death"); pp. 82-83 ("quick and timely death", stack of a couple of thousand feet, Loose Deuce, Double Attack, Six Pack); p. 60 (delayed 90s, wing flash). Page numbers off by one in places |
| nato-02 Engaged and free fighter | confirmed | Anderegg pp. 59, 83: "engaged" presses, "free" manoeuvres away; they "continuously switched roles" |
| nato-03 "Left, low, lead" | confirmed | Anderegg p. 160, with "discipline, discipline, and discipline" |
| roe-01 VID bound the BVR shot | confirmed | Lambeth 1985 note 5: "without reliable IFF we will, in most cases, remain bound to visual rules of engagement"; Eagle Eye, Anderegg p. 161 |
| iff-01 NATO IFF in 1985 | confirmed | GAO 127114 (11 June 1985): Mark XII "the only secure identification system available", "not totally accepted by NATO"; tentative Mark XV frequency agreement; joint office 1980; BVR ID "a critical requirement" |
| iff-02 Combat Tree | unsupported | Forum and blog snippets only; no official or period source found; marked unverified, default off in design |
| bvr-01 AIMVAL-ACEVAL, AIM-9L, AIM-7F | confirmed | Anderegg pp. 158-163: Nellis 1977, AIM-9L chosen, AIM-7F "four to five times higher than it had been on the F-4", "thirty miles apart". Lambeth 1985 dates ACEVAL 1976 (disagreement kept) |
| bvr-02 Drag and drag-and-bag | confirmed | Anderegg p. 160 |
| num-01 Numbers and exchange ratios | confirmed | Lambeth 1985: ACEVAL 1976, Watts *TOPGUN Journal* Fall/Winter 1979 p. 24, "regardless of qualitative differences"; Harrell "distinctly less than 1:1", "slightly better than 1:1" without VID; TOPGUN Aug 1980 "roughly two shots called for every shot observed" (Lambeth's own observation, not Watts's) |
| nato-04 F-15 radar and squadron | confirmed | Anderegg p. 155 note (22 F-15A + 2 F-15B), p. 156 (ground to 40,000 ft); Lambeth 1984 "as far out as 50 n mi"; Dubrov about 50 km against clutter, more than 80 km upper hemisphere |
| bekaa-01 Bekaa numbers | confirmed | Lambeth 1984: 23 on the first engagement, 15 the next day, 85 by end of July, about 40 F-15 / 44 F-16 / 1 F-4 ("a variety of conflicting accounts"), 7% guns, mostly AIM-9L, no BVR shots, beam attacks, Eitan's line. Cordesman: "29 Syrian MiGs without a single loss" in the initial attack |
| bekaa-02 Dubrov's reading | corrected | Search sectors are "60 degrees up and down" (elevation), not ±60°; the "free hunt impossible" line is about strike aircraft with guided weapons hunting ground targets ("deemed unwise"), not fighters. E-2C 100 km offshore, over 300 km look-down, F-16s' designation from F-15s, control weakening with depth: confirmed |
| gci-01 Positive ground control | confirmed | Lambeth 1984 note quoting Skinner: "most missions be flown under positive ground control", aborted if contact is lost; Lew Allen "fairly predictable, doctrinaire". Adversary's view, labelled |
| gci-02 Lazur and Sapfir-23 | corrected | Ranges confirmed on airwar.ru ("typical target", not "MiG-21-sized"), now cited there instead of Wikipedia; MLD in series from 1984 (airwar.ru), not 1978; the ML (1976-81) is the 1985 standard |
| init-01 Soviet reform | corrected | Moskvitelev May 1984, "radical overhaul", "prompting from the top", adversary flights "of up to four aircraft", 1983 two-sided training, initiative "transferred mainly to the flight leaders", commands to clarify halved: confirmed (Pennington). Dubrov's 1978 series stressed GCI directives (Lambeth); it did not "restore" manoeuvring combat; "seven-part" unverified |
| init-02 Initiative ebbing | confirmed | Johnson: peaks 1973 and 1982, drop in 1983-84; two-week probation for overstressing in a won dogfight; a student grounded for two rolls too many (SARPP); attackers "attack as a pair only one target", scenario "prearranged". The years 1979 and 1983 for the two cases follow Johnson's chronology and were not checked against his reference list |
| pair-01 Sword and shield | corrected | Pennington confirms the year-long debate and the "leader as sword and wingman as shield" idea dying; Johnson gives "every pilot is both a shield and a sword" from a 1982 article and the "more advantageous position" line to Golubev; the report had joined them under Golubev |
| turchin-01 Echelons and attack geometry | confirmed | CIA translation: groups of four to eight; 80-100 km; 1,500-2,000 m and 10-12 km; centre 50-60 km; break-off 25-30 km; departure no less than 6-8 km, launch 4-6 km. Date sharpened to 1967-68 |
| frat-01 Fratricide and one zone | confirmed | Baltic MD exercise, "up to 25 percent of our own aircraft"; interrogators on all guidance and fire control radars; 600 m radius blocking; corridors rejected |
| falk-01 Falklands CAP | confirmed | US Navy report: 28 Sea Harriers, more than 1,200 sorties in 44 days, almost 90%, 1,100 air defence sorties, outer defence rarely more than four, about 20 minutes on station, two Sidewinders and two Aden for a 90-minute mission, at least 20 destroyed (16 AIM-9L, 4 guns), none lost in air combat, lack of radar-controlled cueing and vectoring. Tier note added (US view of the UK) |
| usafe-01 Low level in Europe, 2:1 | confirmed | Anderegg pp. 60-61: ceiling below 3,000 ft and visibility under three miles, MiGs outnumbering NATO two to one "by most counts", radios jammed |
| aggr-01 Aggressors' GCI | confirmed | Anderegg p. 78 ("in the same way the Soviets relied on their GCI"); check and extend, guns break pp. 82-83 |
| mig23-01 MiG-23 against Western fighters | corrected | Lambeth 1985 note 31 confirmed (turns worse than a hard-wing F-4; exceeds the F-5 "by a considerable margin" in acceleration and top speed). The Israeli evaluation was of a MiG-23ML defected in 1989, against the F-16C: post-period hindsight, moved to section 5; "above 900 km/h" unverified |
| ratio-01 Exchange ratios | confirmed | Van Gilder: Korea about 10:1 ("Our 14 to 1 ratio ... is not too accurate"); Rolling Thunder about 2.3:1 for both; 1970-73 72 MiGs for 28, USAF 1.88, USN 12.5 (after de Leon p. 11); Linebacker I 69 MiGs; Linebacker II 8 of 32 launched. Werrell's 3% and 8% not re-read |
| skill-01 Training over aircraft | confirmed | Van Gilder: Cunningham's 150 ACM flights, Navy 1.04 kills per engagement, Ritchie's quote |
| syria-01 Syrian MiGs and behaviour | corrected | Lambeth: MiG-23s lacked the APEX (R-23) and High Lark; Cordesman: "stripped-down export versions", "at best ... AA-8", more courage than in 1973. Figure-eights are a Western attaché's account; the ejections a footnote; attribution to Roberts not checked |
| neu-01 Neutral (Austrian analogue) | unsupported | Rests on Wikipedia (`austrian_af_wiki`, registered tier 4); tier problem: Wikipedia is the cited source. Not re-sourced in this check |

Period scan: no post-1985 equipment or tactic is presented as 1985 practice. The terms used (Fluid Attack, engaged and
free, delayed 90s, "left, low, lead", drag, check and extend, VID, BVR, GCI, bingo and joker as open items) are all in the
period sources. The 1989 Israeli MiG-23ML evaluation was the one post-period item used as data; it is now excluded. The
MiG-23MLD is at the edge (series from 1984). "Fox three" and the A-50 and MiG-29 dates stay open items in section 5.
