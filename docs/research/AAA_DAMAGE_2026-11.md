# Air defence guns, battle damage and pilots' reactions: research (November 2026)

Why: track R, step R0 (`docs/DEVELOPMENT_ROADMAP.md`, "Track R"). R1 draws a damage state per burst by miss distance,
R2 gives the guns an accuracy that falls with range, R4 makes pilots react. This note gathers the public numbers to set
and calibrate them. Read 2 November 2026. Every source is registered in `docs/SOURCES.md` under "Air defence guns and
battle damage (track R)"; tiers as there. Where nothing public gives a number, the last section proposes a SAM3D
estimate and says so.

## Sources read

| Id | Tier | What it is | What it gave |
|---|---|---|---|
| `zu23_rs_1977` | 1 | «23-мм спаренная установка ЗУ-23. Руководство службы, часть I», Voenizdat, 3rd ed., 1977 (240 pp., posted by tksu.ru) | ZU-23 data (§1.2), the ammunition (§13.1-13.2), the V-19U fuze and its self-destruct (§13.3), the ZAP-23 sight's inputs and scales |
| `westermann_2000` | 1 (archival study) | E. B. Westermann, *Defending Hitler's Reich: German Ground-Based Air Defenses, 1914-1945*, PhD dissertation, UNC 2000, DTIC ADA380153 (the basis of his book *Flak*, 2001); read from the archive.org text | Rounds per shootdown by year and calibre; Eighth and Fifteenth Air Force aircraft lost and damaged by flak; seriously damaged share; the 88 mm's lethal distance; delayed crashes |
| `werrell_2005` | 1 (the USAF's own press) | K. P. Werrell, *Archie to SAM: A Short Operational History of Ground-Based Air Defense*, Air University Press, 2005, DTIC ADA439255 | WWI rounds per claim; Korea's flak shares of losses and damage; Vietnam's losses by cause and SAMs per kill; 1973; Desert Storm damaged and lost by cause; fuze factors |
| `affdl_tr_77_115` | 1 | AFFDL-TR-77-115, *A Comparative Analysis of USAF Fixed-Wing Aircraft Losses in Southeast Asia Combat*, Air Force Flight Dynamics Laboratory, Dec 1977 (declassified), DTIC ADC016682 | Kill severity (how far a fatally hit aircraft flew), the reasons for crash, crew survival, one round per hit, F-105s hit vs lost |
| `gwaps_vol5` | 1 | *Gulf War Air Power Survey*, vol. V, *A Statistical Compendium and Chronology*, 1993, DTIC ADA279745, Table 205 | Coalition aircraft damaged and lost per 1,000 sorties by type and service |
| `usaf_tacops_1962` | 1 | USAF Historical Division Liaison Office, *USAF Tactical Operations, World War II and Korean War, with Statistical Tables*, May 1962 (archive.org) | Korea: USAF combat losses by cause, Table 111 |
| `checo_1967_tactics` | 1 | Project CHECO, *Air Tactics Against NVN Air/Ground Defenses*, HQ PACAF, Feb 1967, DTIC ADA586117 | SAM-forced jettisoning, the 1966 end-of-tour report, an F-105 hit by 85 mm |
| `checo_1969_tactics` | 1 | Project CHECO, *Air Tactics Against NVN Air/Ground Defenses, Dec 1966 - 1 Nov 1968*, 30 Aug 1969, DTIC ADA486448 | Jettisoning under MiG attack (Sept-Oct 1967), losses by place with ECM pods |
| `checo_1970_tactics` | 1 | Project CHECO, *USAF Tactics Against Air & Ground Defenses in SEA, Nov 1968 - May 1970*, DTIC ADA487052 | Jinking rule, minimum altitudes, gun ranges with optical and radar laying |
| `mt_1973_nvn` | 1 (Soviet MoD journal) | «Военная мысль» (Military Thought, secret collection), article on the DRV's air defence against US aircraft 1965-67, CIA translation of 18 Sept 1973, CIA-RDP10-00105R000100260001-7 | The gun's share of kills, missiles per kill (Soviet figures), the US anti-SAM manoeuvre, jettisoning under fighter attack, avoidance |
| `mt_1984_babich` | 1 (Soviet MoD journal) | V. K. Babich, «Развитие способов преодоления ПВО авиацией в локальных войнах», «Военная мысль» No. 5, 1984, pp. 34-43, CIA translation, CIA-RDP10-00105R000403350001-1 | Penetration methods, the anti-missile manoeuvre, manoeuvres against guns, the guns' optical laying |
| `nhandan_2013` | 4 | «60 năm Ngày truyền thống Bộ đội Pháo cao xạ», *Nhân Dân*, 1 Apr 2013 | Vietnam's AAA claims over the North |
| `ru_wiki_aaa` | 4 | Russian Wikipedia: «С-60», «100-мм зенитная пушка КС-19», «85-мм зенитная пушка образца 1939 года (52-К)», «Шилка (ЗСУ)», «СОН-9», «Дистанционный взрыватель» (read 2 Nov 2026) | Shells, fuzes, the 52-K's 1952 service manual table, the RPK-2's and SON-9's tracking errors, fuze timing spread |
| `wiki_guns` (existing) | 4 | English Wikipedia: AZP S-60, ZU-23-2, ZSU-23-4 (their tables after russianammo.org) | The UOR-281's 2.85 kg and 154 g; the 23 mm's charges |
| `ru_wiki_guns` (existing) | 4 | Russian Wikipedia: KS-30, 61-K | KS-30's 33.4 kg, VM-45; UOR-167's 37 g, self-destruct |
| `yugoimport_ammo` (existing) | 1 | Yugoimport sheets, 23 mm and 57 mm | 57 mm self-destruct 13-17 s, arming 250 m |
| `rjlee_ds_losses` (existing) | 4 | R. J. Lee after GWAPS vol. V | Desert Storm losses by cause |
| `jvl_1973_losses` (existing) | 4 | Jewish Virtual Library | IAF 102 lost |

Not reached: Ball's *Fundamentals of Aircraft Combat Survivability* (no public copy); the Soviet anti-aircraft firing
textbooks and the S-60, KS-19 and ZSU-23-4 service manuals (no public copy found); DTIC's own PDF server (it refuses
automated downloads: the documents above were read from their archive.org copies).

## 1. The guns and their shells

| Gun | Shell (round) | Shell mass | HE charge | Muzzle velocity | Fuze | Self-destruct | Sources |
|---|---|---|---|---|---|---|---|
| ZU-23-2 (ZU-23) | ОФЗ / ОФЗТ (3УОФ1 etc.) | 183 g / 188.5 g (BZT 190 g) | A-IX-2 / A-IX-20; mass not stated in the manual. 18.5 g (3UOF1, ru wiki); 19 g OFZ, 13 g OFZT (en wiki after ORDATA); 13-20 g (bigenc table) | 980 m/s OFZ, 970 m/s OFZT/BZT | V-19U (earlier MG-25): **contact, impact-delay, with self-destruct**; armed 3-80 m from the muzzle (as printed) | **5-11 s** after firing (V-19U); **5-8 s** (V-19UK) | `zu23_rs_1977` §1.2, §13.1-13.3; `ru_wiki_aaa`; `wiki_guns` |
| ZSU-23-4 | the same 23x152B rounds, belts of 40 OFZT + 10 BZT | as above | as above | as above | as above | as above | `wiki_guns`; `zu23_rs_1977` |
| 61-K | UOR-167 | 0.732 kg | 37 g TNT | 880 m/s | contact with self-destruct | about 4,000 m | `ru_wiki_guns` |
| S-60 | UOR-281 / UOR-281U | 2.85 kg | 154 g (`wiki_guns` after russianammo.org); Jane's 168 g for the 281U (tier 5, recorded only) | 1,000 m/s | **contact (nose), with self-destruct and tracer** | **13-17 s**; arming at 250 m | `ru_wiki_aaa` («С-60»: «зенитный осколочный снаряд с ударным взрывателем»); `wiki_guns`; `yugoimport_ammo` |
| S-60 (later) | 3UO6 | | 154 g | 1,000 m/s | AR-51 **proximity**, worked with the RPK-1 | | `wiki_guns` (no cited source; low confidence) |
| 52-K | 85 mm fragmentation grenade with T-5 | 9.2 kg with fuze | 0.660 kg (+0.120 kg TNT-smoke booster, as printed) | 800 m/s | **T-5 pyrotechnic time fuze** (later mechanical) | (time fuze) | `ru_wiki_aaa` (the table of «85-мм зенитная пушка обр. 1939 г. Руководство службы», 1952) |
| KS-19 | UO-415 (O-415), also UOF-412 | 15.6 kg | 1.4 kg TNT | 900 m/s | **VM-30, VM-30L, VM-45 mechanical time fuzes**; radio-fuzed UO-415R also listed | ceiling by fuze: 12,700 m with VM-30, 14,900 m with VM-45 (at +85°) | `ru_wiki_aaa` (KS-19 table after Shirokorad) |
| KS-30 | O-481 | 33.4 kg | not found | 970 m/s | **VM-45 mechanical time fuze** | reach by self-destruct 19,500 m (19,453 m at +88° in the 1961 firing table) | `ru_wiki_guns`; `ru_wiki_aaa` |

So the belief to check is **confirmed**: the S-60's UOR-281 is contact-fuzed with self-destruct, the KS-30's is
time-fuzed (VM-45). The light guns (23, 37, 57 mm) kill only by **hitting**: a round that misses goes on and
self-destructs seconds later, far from the aircraft (5-11 s for the 23 mm is 2-4 km downrange). The medium and heavy
guns (85, 100, 130 mm) burst near the aircraft on a set time.

Fragments and lethal radius:
- No public operator-country source found gives a fragment count or a «радиус поражения» for any of these shells.
  The only count is the KS-19's shrapnel round 3USh1, 1,800 elements (`ru_wiki_aaa`), not an anti-aircraft fragmentation shell.
- The 23 mm manual says the damage is done by the fragments and the blast, and the charge is also incendiary
  (`zu23_rs_1977` §13.2). The fuze's delay bursts the shell behind the skin.
- The nearest historic figure is the German 88 mm (`westermann_2000` p. 10): pre-war flak studies held that one 88 mm
  shell bursting **within 33 yards (30 m)** would bring a bomber down. By October 1943 Göring reminded the flak
  commanders that the estimate had "plummeted to a mere **13 yards (12 m)**". Even there it was not sure to bring down a
  four-engine bomber.
- The 88 mm's charge is close to the 52-K's 0.66 kg, so this is the anchor for the time-fuzed calibres in section 7.

## 2. Accuracy and fire control

Fire-control errors (radar):
- **SON-9 "Grom-2"** (S-60 and KS-19 batteries, with the PUAZO-6 director, later the RPK-1 "Vaza"): automatic-tracking
  errors **20 m in range** and **"0,2 д. у." in azimuth and elevation** (`ru_wiki_aaa`, «СОН-9», after Shirokorad 2000).
  As printed, 0.2 mil is implausibly fine for a 10 cm set. It most likely means 0-02, i.e. 2 thousandths (about 0.12°).
  Resolution 125 m by hand, 200 m automatic. Detection at least 55 km.
- **ZSU-23-4's RPK-2 "Tobol"** (radar 1RL33): automatic-tracking accuracy **10 m in range** and **"00-6" in angle**
  (`ru_wiki_aaa`, «Шилка (ЗСУ)»: read as 0-06, i.e. 6 thousandths, about 0.36°, or as 0.6 thousandths; ambiguous).
  Detection of a MiG-17-sized target at least 12 km, automatic tracking at least 10 km, dead zone 200 m.
  The third laying mode, "by remembered settings", is used when the lock is lost, under jamming or at high angular rates.
- **SON-30** (KS-30): nothing public found.

Fire-control errors (optical):
- **ZAP-23** (ZU-23), from `zu23_rs_1977` (§1.2 and the sight's description):
  - Three inputs: the target's course, speed and range. Range is estimated **by eye or with a stereo rangefinder**,
    and "the other data are determined by eye".
  - The sight's scales are graduated in 100 m (range), 5 m/s (speed), 1° (course) and 5° (dive or climb).
  - It solves the meeting point by successive approximation, assuming the target flies **straight and level at
    constant speed** over the shell's time of flight.
  - It takes target speeds 0-300 m/s, and solves the meeting point out to 2,000 m (current range to 3,000 m).
  - Collimator ring radius 0-50.
  - This is the source for an optical aim error dominated by estimated speed, course and range, and for a
    prediction error that grows with time of flight and manoeuvre.
- `mt_1984_babich`: the guns' crews in the local wars "rarely used fire control radar, and more often used optical
  sights", which is why jamming did little against them. Manoeuvres against guns ("snaking", "scissoring",
  "slipping") made the gunners' aiming difficult.
- `checo_1970_tactics` (Laos, 1968-70) gives the S-60's maximum effective slant range as **13,100 ft (4.0 km) with
  optical sighting, 19,700 ft (6.0 km) with gun-laying radar**; 85 mm 27,500 ft, 100 mm 39,000 ft; 15 rounds a minute.

Fuzes:
- Pyrotechnic time fuzes (T-5 type) have a **running-time spread of up to 5%**, and depend on pressure and temperature.
  Mechanical (clockwork) fuzes such as the VM-30 are more accurate (`ru_wiki_aaa`, «Дистанционный взрыватель»).
  No public figure was found for the VM-30's or VM-45's own error.
- The fuze's share of the miss shows in what better fuzes bought. The proximity fuze was about **five times** more
  effective than time or contact fuzes against the V-1 (`werrell_2005` p. 18). The US Navy estimated it raised AAA
  effectiveness **three to four times** (p. 53). German double fuzes (contact plus time, late 1944) raised the
  effectiveness of 88 mm guns five times, 105 mm three, 128 mm twice (p. 34). A US study found proximity fuzes
  would have raised German flak efficiency by a factor of 3.4 (p. 34).
- The 128 mm's factor is smaller than the 88 mm's, so the newer, better-directed guns lost less to fuze error.

Ballistic dispersion (срединные отклонения) and hit probability per round versus range: **no public source found**
(the firing tables and the «Правила стрельбы» are not online). Section 7 gives estimates.

## 3. Rounds fired per aircraft brought down

German flak, from `westermann_2000` (German records):

| Period | Heavy flak, rounds per kill | Light flak, rounds per kill | Page |
|---|---|---|---|
| 1940, average | 2,412 | 4,598 | 194 |
| December 1940 | 7,058 | 20,604 | 194 |
| First 20 months of the war | 2,805 | 5,354 | 537-538 |
| Nov-Dec 1943 (jamming, weather) | 4,000 | 6,500 | 538 |
| 1944, 88 mm/36-37 | **16,000** | | 535 |
| 1944, 128 mm (regular crews, best equipment) | 3,000 | | 293, 537 |
| 1943 forecast (von Renz): 88 mm/36 and /41, 105 mm/39 | 3,000 each | | 381 |
| April 1945, Munich, double-fuzed rounds (13 kills) | **370** | | 557 |
| Average at that time | about 4,500 | | 557 |
| Whole war (one estimate) | **3,343** | **4,940** | 538 |

Westermann explains the 16,000 of 1944 by four things (pp. 535-537):
- the 88 mm/36-37 fired at B-17s at or beyond its effective ceiling;
- worn barrels (380 guns a month lost to wear or destruction);
- 262 Home Guard batteries without good fire control, firing barrage;
- chaff, and auxiliaries replacing trained gunners.

Other periods (`werrell_2005`):
- **WWI**, rounds per claim (p. 3): German 11,600 (1915) falling to 5,000 (1918); French 11,000 to 7,000; Russian
  11,000 (1916) to 3,000 (1917); British 8,000 to 4,550; US 605 (17 kills in three months; another source gives
  US 1,055, British 1,800, French 3,225).
- **WWII**: the same German figures (p. 41).
- **Korea**: no rounds-per-kill figure found. The USAF credited **light flak with 79% of the aircraft downed and
  45% of those damaged**, small arms with 7% and 52%, **heavy flak with 14% and 3%** (p. 76).
- **Vietnam**: no gun rounds-per-kill figure found. Of the US fixed-wing aircraft lost in flight to enemy defences
  (just over 2,400), **gunfire caused 89%** of known causes, SAMs 8%, MiGs 3%. Over North Vietnam, of about 1,100
  lost, 72% fell to gunfire, 19% to SAMs, 8% to MiGs (p. 118).
- SAMs fired per aircraft downed, US count: about 18 (1965), 35 (1966), 57 (1967), 107 (1968) (p. 121).

**Disagreement (Vietnam):**
- The Soviet specialists' count gives **three missiles per kill** to 15 May 1967 (`mt_1973_nvn` p. 7). The same
  article credits the guns with **76%** of US aircraft destroyed, and says that of 1,302 aircraft entering the SAM
  zone, 382 (about 30%) were destroyed (VPA claims).
- The Vietnamese claim nearly **3,100** aircraft to the guns of the three forces over the North, about 75% of their
  claims there; the regular AAA troops claimed 1,052 (`nhandan_2013`).
- The claims run several times the US losses. Calibrate against the losses; keep the claims for what our side's
  record would believe (the roadmap's "first person and the record").

**1973 and 1982**: no rounds-per-kill figure found.
- Western sources put **30-43%** of Israeli losses on the ZSU-23-4 (Mearsheimer 1985 p. 270, via «Шилка (ЗСУ)»,
  `ru_wiki_aaa`). Russian data credit the Shilkas with **27 aircraft**.
- ZSU-23-4 gunners were trained to fire **short bursts of about 75 rounds** (`werrell_2005` p. 151).

## 4. Battle damage: damaged against lost

| War, force, threat | Damaged | Lost | Damaged : lost | Source |
|---|---|---|---|---|
| WWII, 8th + 15th AF heavy bombers, flak, Jan-Apr 1944 | 10,563 | 314.5 | **34 : 1** | `westermann_2000` Table 8.3, p. 455 |
| WWII, 8th AF, flak, Jun-Aug 1944 | 10,972 | 341.5 | 32 : 1 | `westermann_2000` Table 9.1, p. 490 |
| WWII, 15th AF, flak, Jun-Aug 1944 | 3,357 | 313 | 11 : 1 | same |
| WWII, Berlin 6 Mar 1944, flak | 318 of 672 (48%) | 14 (+5 flak-then-fighter) | about 20 : 1 | `westermann_2000` pp. 456-458 |
| WWII, Berlin raids May 1944, 1st Bomb Division, flak | 553 of 938 (59%) | 11 | 50 : 1 | `westermann_2000` p. 473 |
| WWII, Merseburg 12 May 1944, 1st BD | 208 of 316 (66%), 26 seriously | | | `westermann_2000` p. 473 |
| Korea, Fifth AF, August 1952, flak | 153 | 14 | **11 : 1** | `werrell_2005` p. 76 |
| Vietnam, F-105, all ground fire | "one out of every four F-105s hit will crash; the other three usually sustain only minor damage" | | **3 : 1** | `affdl_tr_77_115` p. 25 (after its ref. 1) |
| 1973, SA-7 against the IAF | 30 | 2 | 15 : 1 | `werrell_2005` p. 152 |
| Desert Storm, all coalition | 48 | 38 | **1.3 : 1** | `werrell_2005` p. 228; `gwaps_vol5` Table 205, pp. 650-651 (USAF 28/14, USN 4/6, USMC 11/7, allies about 5/11) |
| Desert Storm, by cause | AAA 24, IR SAM 15, radar SAM 4 | guns 11, IR SAM 14, radar SAM 10 | AAA **2.2 : 1**, IR SAM 1.1 : 1, radar SAM **0.4 : 1** | `werrell_2005` p. 228 (damaged); `rjlee_ds_losses` (lost) |

More on WWII damage:
- **8th AF**: flak damaged **54,539** aircraft from December 1942 to April 1945, slightly more than **20% of sorties
  dispatched**. The 15th AF lost 11,954 damaged, 8.5% of sorties (`westermann_2000` p. 525).
- **Severity**: of 15,042 1st Bomb Division aircraft damaged by flak (May 1944 - March 1945), 4,115 (**27%**) were
  "seriously damaged" (p. 525). Werrell: the 8th AF suffered 20% damage per sortie and wrote off 1,600 bombers as
  damaged beyond economic repair (p. 43).
- **Overall**: flak destroyed at least half of American combat losses and damaged more than 66,000 US and 9,000
  British bombers (`westermann_2000` abstract).

Other figures:
- **Korea**: ground fire destroyed or damaged **21.6 aircraft per 1,000 sorties** in early 1952, and 11.1 after
  minimum altitudes were imposed (`werrell_2005` pp. 77-78). USAF combat losses: 501 of 708 to ground fire, 139 air
  to air, 68 unknown (`usaf_tacops_1962` Table 111).
- **1973**: the IAF lost 102 aircraft (`jvl_1973_losses`); no public total of aircraft damaged was found.
- **Disagreement (1973)**: some sources say 50 more were hit by flak; the "236 damaged" sometimes quoted is unsourced.

The ratio falls steeply from WWII to the jet age. Big four-engine bombers took many small hits at their ceiling, and
time-fuzed bursts were mostly distant. Fast jets are hit by one round at low level, and the hit is usually decisive.
`affdl_tr_77_115` p. 74: "statistically speaking, if an aircraft is hit, only one gun round or missile is involved",
and about 90% of hits came while the aircraft was engaged with its target.

How long a fatally hit aircraft flew on (`affdl_tr_77_115`):
- Kill severity is graded by distance flown after the hit: **K < 5 nm**, **A 5-50 nm**, **B > 50 nm**.
- Where severity is known, the K share of losses was:
  - F-105: **31.9%** (p. 25);
  - F-4: about **50%** for every threat class, 63.4% for small arms (pp. 11-12);
  - RF-4C: 42.9% overall, 64% to small arms and automatic weapons, **37.5% to AAA**, 28.6% to SAMs (p. 10).
- At 450-550 kt, 5 nm is about 35-40 s and 50 nm about 6 minutes.
- Crew survival rose with distance flown: F-105 K 52.9%, A 67.6%, B 94%. F-4 K 65.3%, A 94.2%, B 81.8%.
- Reasons for crash, F-105 to ground fire: **fire/explosion 44.2%**, flight controls 18.4%, engine damage/fire 16.8%.
  F-4 and others: fire/explosion, engine damage/fire and flight controls were the biggest contributors (p. 76).
- WWII: some flak-damaged bombers flew home on slow oil or fuel leaks; "numerous damaged bombers were able to fly on
  for only 10, 20, or 100 miles before succumbing to flak damage". Others made for Sweden or Switzerland, or ditched
  (`westermann_2000` p. 530).
- No public figure was found for how often a hit aircraft trailed smoke. Fire as the leading cause of loss is the
  closest proxy.

## 5. Jettisoning, abort and diversion

- **SAM threat, 1966**: an end-of-tour report: "Many times the strike force must jettison their ordnance and take hard
  evasive maneuvers to evade launched SAM's"; SAMs forced strikes lower into the guns and spoiled target acquisition
  (`checo_1967_tactics`, the SA-2 section, at its note 78). The 1969 report: SAMs "caused aircrews to jettison ordnance;
  their bombing accuracy was also decreased" (`checo_1969_tactics` p. 3).
- **Fighter attack**: CINCPAC noted that **47 aircraft were forced to jettison ordnance in September 1967** after
  MiG attacks, and **on 2 October 1967 alone 16 F-105s jettisoned all bombs** (`checo_1969_tactics` pp. 14-15).
  - The Soviet view agrees: with no reliable cover, the VPA's fighters "would often frustrate the accomplishment of
    tasks; American pilots would be forced to jettison their bombs ahead of time" (`mt_1973_nvn`). The article also
    gives an example of F-105s "jettisoning their bombs and in disorder" returning after one was shot down.
  - The share of sorties is not given. A secondary page's "107 (56 percent) forced to drop their bombs" (1966-67)
    could not be traced to its source and is not used.
- **Damage**: an F-105 damaged by three close 85 mm bursts jettisoned its stores, keeping its ECM pods, and then
  evaded a MiG and an SA-2 (`checo_1967_tactics` pp. 47-48).
- **Avoidance rather than abort**: "in certain months thirty to fifty percent of the aircraft did not enter SAM
  battalion zones" (`mt_1973_nvn`).
- **Disagreement (Gulf War)**: according to V. Ilyin, after KS-19s shot down the lead of eight Tornados at 6,700 m
  (22 January 1991), "the rest, having rid themselves of their bombs, dived" (`ru_wiki_aaa`, KS-19). Khaustov and
  Kotlobovsky credit that kill to a Roland.
- **1973 and the Bekaa 1982**: no public source found for jettisoning under SAM or AAA fire, or for the share of
  strikes aborted. The IAF stopped flying within 10-15 miles of the canal after its first days' losses
  (`werrell_2005` p. 152), and the dumped bombs of 6 October 1973 were jettisoned on scramble, not under fire
  (`jvl_1973_losses`).

## 6. Pilots' defensive reactions (for R4)

Against SAMs:
- US pilots spotted SA-2 launches on clear days (the missile's smoke trail), dived toward the missile, then pulled up
  as abruptly as possible when it turned to follow; this needed warning, timing and nerve. Flying near clouds between
  them and the ground was forbidden, so as to keep the warning time. Later the RHAW gear gave a light and a tone when
  SAM radar tracked them (`werrell_2005` p. 121).
- The Soviet description (`mt_1973_nvn`), from 1.5-2 km: on a launch warning (radio from observation aircraft, or a
  light on the panel), dive to 300-400 m and change heading **30 to 180°** (or 90° and more), leaving the kill zone.
- Babich (`mt_1984_babich`): on a launch warning, turn toward the nearest boundary of the lethal zone and cross it as
  quickly as possible, in a steep spiral from 1,500-3,000 m. Feint attacks drew launches while others ran in at
  500-800 m. A missile seen close was beaten by timing a hard turn; a start at up to 15 km was too early.
- SA-2 kills came almost only when the aircraft was above 1,500 ft **and failed to take high-G evasive action**
  (`checo_1967_tactics`, 1966).

Against guns:
- Jink, changing heading and altitude abruptly and at random. The rule of thumb was a jinking interval in seconds of
  two-thirds the altitude in thousands of feet (10 s at 15,000 ft, 2 s at 3,000 ft). Against barrage fire, speed
  rather than jinking (`checo_1970_tactics` p. 21).
- Fly above light flak: a minimum of 4,500 ft AGL in high-threat areas (`checo_1970_tactics`); 3,000 ft pull-out in
  Korea, which cut losses (`werrell_2005` p. 77). ECM pods let 1967 strikes fly at 10,000-17,000 ft above light and
  medium flak (`werrell_2005` p. 126).
- Fly the target area in a single pass; strike from several directions (the "star raid") to scatter the guns
  (`mt_1973_nvn`; `mt_1984_babich`).

## 7. What SAM3D uses

"Sourced" means a public number above sets it; "estimate" means a SAM3D game value, anchored as stated, to be tuned in
`data/gameplay/damage.json` and checked against the bands with `air_war_bench`.

| Parameter | Value | Basis |
|---|---|---|
| 23 mm (ZU-23-2, ZSU-23-4) fuze | Contact with delay. Bursts only on a hit; else self-destructs 5-11 s (V-19U) or 5-8 s (V-19UK) after firing | sourced: `zu23_rs_1977` §13.3 |
| 23 mm round | 183-188.5 g, 970-980 m/s; charge 13-19 g | sourced: `zu23_rs_1977`; charge `wiki_guns`, `ru_wiki_aaa` |
| 37 mm (61-K) | Contact, self-destruct about 4,000 m; 37 g; 880 m/s | sourced: `ru_wiki_guns` |
| 57 mm (S-60) fuze | **Contact** with self-destruct 13-17 s, arming 250 m; 2.85 kg, 154 g, 1,000 m/s | sourced: `ru_wiki_aaa`, `wiki_guns`, `yugoimport_ammo` |
| 85 mm (52-K) | Time fuze (T-5 pyrotechnic); 9.2 kg, 0.66 kg charge, 800 m/s | sourced: `ru_wiki_aaa` (1952 service manual) |
| 100 mm (KS-19) | Time fuze VM-30/VM-45; 15.6 kg, 1.4 kg TNT, 900 m/s | sourced: `ru_wiki_aaa` |
| 130 mm (KS-30) | Time fuze VM-45; 33.4 kg, 970 m/s; charge about 3 kg | fuze and mass sourced (`ru_wiki_guns`); **charge an estimate** (KS-19's 9% of shell mass) |
| Light guns' damage model | A hit is decided per round from the pattern against the presented area. **P(state given one hit)**, 23 mm: destroyed 0.10, heavy 0.15, moderate 0.25, light 0.50. 37 mm: 0.20 / 0.20 / 0.25 / 0.35. 57 mm: 0.30 / 0.25 / 0.25 / 0.20 | **estimate**, calibrated on: one round per hit (`affdl_tr_77_115`); about 1 in 4 F-105s hit crash (`affdl_tr_77_115`); Desert Storm AAA damaged : lost 2.2 (`werrell_2005`) |
| Lethal radius, time-fuzed bursts against a fighter (destroyed / heavy / moderate / light) | 85 mm 8 / 12 / 20 / 35 m; 100 mm 10 / 15 / 25 / 45 m; 130 mm 13 / 20 / 32 / 55 m | **estimate**. Anchored on the 88 mm's 12 m "not guaranteed" lethal distance against a four-engine bomber in 1943 (`westermann_2000` p. 10) and the pre-war 30 m; scaled by the cube root of the charge (0.66, 1.4, about 3 kg). Bursts at 5-50 m mostly do nothing, as the roadmap asks |
| Fragment counts | Not modelled per fragment. If needed, 85 mm about 600-700, 100 mm about 900, 130 mm about 1,300 | **estimate**; no public count found (the 88 mm's "680-750" is from an unsourced page, not used) |
| Ballistic dispersion (1σ) | Light automatic guns 1.5 mils; 85-130 mm 0.8 mils | **estimate**; no firing table found |
| Time-fuze error (1σ) | Mechanical VM-30/VM-45: 0.4% of running time + 0.03 s; pyrotechnic T-5: 1.5% (spread "up to 5%") | T-5 spread sourced (`ru_wiki_aaa`, read as about ±3σ); **mechanical an estimate**. Check: removing the fuze error (a proximity fuze) should raise the kills 3-5 times (`werrell_2005` pp. 18, 34, 53) |
| Radar fire-control error (1σ) | SON-9: 20 m range, 2 mils angle. RPK-2: 10 m range, 1-2 mils angle | sourced figures (`ru_wiki_aaa`); the angle readings are ambiguous ("0,2 д.у.", "00-6"), the 1σ values chosen an estimate |
| Optical fire control error (ZAP-23, S-60 by eye) | Range by eye σ 15% (stereo rangefinder 5%); target speed σ 10% (5 m/s scale steps); course σ 10°. Day only | the inputs by eye sourced (`zu23_rs_1977`); **the sizes an estimate** |
| Prediction error | The sight assumes straight, steady flight over the time of flight. Error = the target's own acceleration over that time (jinking 2-4 g); jink interval from the CHECO rule | method sourced (`zu23_rs_1977`; `checo_1970_tactics` p. 21) |
| Effective slant range | S-60: 4.0 km optical, 6.0 km radar. ZU-23: 2.5 km slant, 1.5 km high, meeting point solved to 2 km | sourced: `checo_1970_tactics`; `zu23_rs_1977` |
| ZSU-23-4 burst | About 75 rounds | sourced: `werrell_2005` p. 151 |
| Rounds per kill, bands to calibrate against | Heavy time-fuzed, radar-directed, good crews: **2,500-4,500** (worst 16,000; best 3,000 for the 128 mm). Light flak: **4,500-6,500** (worst about 20,000). Double or proximity fuze: **300-1,000** | sourced: `westermann_2000` pp. 194, 293, 535-538, 557. Applied to 1980s guns against jets as an **estimate** of the same order: no Korea, Vietnam or 1973 figure found |
| Damaged : lost, to calibrate R1 | Guns against jet fighters at low level **2-3 : 1** (Desert Storm AAA 2.2; F-105 3; Korea flak 11 with 1950s piston and jet types). IR SAM about **1 : 1**; radar SAM about **0.4 : 1**. WWII heavy bombers against heavy flak 11-34 : 1, about 27% of them seriously | sourced: `werrell_2005`, `gwaps_vol5`, `rjlee_ds_losses`, `affdl_tr_77_115`, `westermann_2000` |
| Time to crash after a fatal hit | Gun kills: about 40% "K" (under about 40 s), the rest 40 s to 6 min ("A"), a few over 6 min ("B": reaches home or the crew ejects over friendly ground). Heavy damage crashes after 10 s - 5 min | K shares sourced (`affdl_tr_77_115`: 31.9-50%, 37.5% for RF-4C to AAA); **the A/B split an estimate** (the tables are unreadable in the copy) |
| Fire and smoke | Fire in about 45% of fatal gun hits (smoke and flame); moderate damage trails smoke in about 30% | fire/explosion 44.2% of F-105 ground-fire losses (`affdl_tr_77_115`); **the smoke share an estimate** |
| Jettison | Moderate or heavy damage: jettison stores 80% (keep pods). A SAM launch seen or warned against the flight: 40% of the flight's strikers jettison. Fighters attacking the strike group: 50%. Flak alone, no damage: 5%. A threatened flight avoiding a SAM zone it can bypass: 30-50% | **estimate**. Anchored on CHECO's 47 jettisoning in September 1967 and 16 F-105s on 2 October 1967 (`checo_1969_tactics` pp. 14-15); SAM-forced jettisoning "many times" (`checo_1967_tactics`); avoidance 30-50% (`mt_1973_nvn`) |
| Defensive reactions | SAM warning: dive 30-180° off course to 300-400 m, or turn to the nearest zone boundary, high-g at the end. Guns: jink at 2/3 × altitude (thousand ft) seconds; barrage: speed. Minimum altitude rules 3,000-4,500 ft | sourced: `mt_1973_nvn`, `mt_1984_babich`, `werrell_2005` p. 121, `checo_1970_tactics` |

**Not sourced** (to look for later):
- the guns' ballistic dispersion;
- the VM-30's and VM-45's error;
- the SON-30;
- hit probability per round or per burst against range;
- fragment counts and the «радиус поражения» per calibre;
- rounds per kill in Korea, Vietnam and 1973;
- the IAF's damaged total in 1973;
- jettison and abort shares as fractions of sorties;
- how often a hit aircraft trailed smoke.

The best candidate sources are the Soviet «Правила стрельбы зенитной артиллерии», the S-60 and KS-19 firing tables and
service manuals, and Ball's textbook.
