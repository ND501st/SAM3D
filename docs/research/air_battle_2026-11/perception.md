# Pilots' perception and human factors: period evidence (to 1985)

Research note for SAM3D's enemy air war (`simcore::airwar`): what a pilot of each faction could see, find and do in
the cockpit by 1985, and how skill, stress and experience changed it. Everything here was in service, in doctrine or
in study by 1985 unless it is flagged. Later items are in section 5. The 1991 Gulf War is not used.

Web searching ran out part of the way through this pass (the shared budget for the turn was used up). The sensor
in-service dates (Pave Spike, Pave Tack, Pave Penny, Kaira-23, Klen-PS, helicopter NVGs) are therefore **not sourced
here**. They are listed in section 7 to be confirmed. No date below comes from memory.

## 1. Summary for SAM3D's design

**The core finding.** By 1985, every faction's attack pilots found ground targets with their eyes. This includes the
crews of NATO's best two-seaters. The evidence is consistent across US, NATO and Soviet material:

- US test series of the 1960s and 1970s (JTF-2, the China Lake models);
- NATO's AGARD meetings of 1972 and 1979;
- the CIA's 1969 view of Soviet training;
- Soviet Afghanistan practice, 1980-85.

Pilots found targets by eye on a short, busy run in. The odds were set mostly by:

- the target and its background (62 % of the variance in JTF-2);
- the visibility;
- terrain masking;
- whether the crew had been briefed and cued.

Speed mattered much less. In Dyer's 1964 flight trials acquisition probability did not change significantly from 350
to 700 kt. Mean acquisition range was about 7 % shorter at 550 kt than at 350 kt and shorter again at 700 kt, but
Jones (1974) reports these range differences as not statistically significant (checked 15 Nov 2026).

Sensors helped only a few crews by 1985: laser spot trackers, TV and infrared pods, TV Maverick and radar mapping.
Even those needed the eye or a cue to put the target in the narrow field of view. RAND (1977) put reliable
recognition of a vehicle-sized target by "sensors which operate at either infrared or visible wavelengths" at about
3 km in relatively good weather and possibly 1 km in bad. RAND does not name the unaided eye; applying the limit to the
eye is SAM3D's inference.

**What this means for the Osa battery.** A deployed, camouflaged Osa battery is hard to find. US pilots back from
Southeast Asia reported that AA sites "are seldom detected unless they are firing" (AGARD 1972). In February 1967 an
F-105F flight acquired an SA-2 site by "the cloud of dust caused by the launch" (CHECO 1967). The model should
therefore treat **the Osa's own launch, its dust and its smoke trail** as the main way a strike pilot finds the
battery. Radar warning and ELINT are the other ways, and they belong to the EM layer.

**What this means for skill and stress.** The period evidence supports a model with these parts:

- **Annual flying hours as the base of skill.** USAF tactical pilots flew about 16-18 hours a month in 1980-85.
  Soviet pilots flew 125-160 hours a year on average in 1969, in the US view. The 1969 Soviet training was
  preplanned, unopposed and on reconnoitred targets.
- **A combat-experience term.** Losses fall sharply after a pilot's first ten combat missions. This was Suter's
  argument for Red Flag in 1975, reported in hindsight.
- **A workload term.** High-speed, low-level flight "results in loss of lookout". Weather, fatigue, "mission
  completion syndrome" and task overload led NATO's 1979 list of accident factors at low level.
- **A crew term for two-seaters.** FB-111 crews held that low-level penetration was "at minimum a two-man job".

**How to model it.** Reuse the Osa TV sight's optics (`em/optical.hpp`: Koschmieder's contrast loss with distance,
Johnson's line criteria) for the **physics of seeing**. Then add the two China Lake models from the period:

- for an aircraft seeing an aircraft or a missile: the visual detection lobe and glimpse model (NWC TP 5709, 1974);
- for a pilot finding a ground target on an attack run: the launch-opportunity model (NWC TP 6005, 1978). It gives
  the chance of acquisition from three inputs: target contrastiness, target pattern, and the shorter of the unmask
  range and the visibility.

Both models are US Navy studies of US crews. Applying them to Soviet, Pact or rebel pilots is a SAM3D assumption.

## 2. Findings by faction

### NATO and the United States (US and NATO documents about their own forces unless marked)

**perc-01: Ground targets are found by eye; target and background dominate.** [jones_1974_ta_sourcebook, JTF-2
summary; agard_cp100_1972, Bailey B10]

- **Period:** JTF-2 tests 1965-68; reviewed 1974.
- **Variance explained:** target and background explained 62 % of acquisition variance in JTF-2.
- **JTF-2 Test 4.1:** 474 sorties, 232 crews, 175-550 kt, in bands below 400 ft and at 500-900 ft. Its results stay
  classified.
- **Bailey's RAF-style checklist (AGARD 1972), what helps acquisition:**
  - a unique, uncamouflaged target;
  - a line feature;
  - movement;
  - IP features near the target;
  - a good approach aspect.
- **What hurts it:** a well camouflaged pin-point target, poor contrast, obscured approach, confusable features.

**perc-02: Speed has little effect on acquisition probability between 350 and 550 kt; range shortens a little.**
[jones_1974_ta_sourcebook, Dyer 1964 and Valentine 1972 as summarised]

- **Dyer (1964), at 500 ft:** mean acquisition range
  - 16,300 ft at 350 kt;
  - 15,200 ft at 550 kt;
  - 11,250 ft at 700 kt.
  Probability was not significantly affected anywhere from 350 to 700 kt. Jones (1974) adds that the range differences
  "were not statistically significant" (checked 15 Nov 2026). Jones's conclusion: speed is not a significant variable
  in the regimes of the period's aircraft, but higher speed reduces the time to search.
- **Valentine (1972), F-105 pilots:** no significant difference between 300 and 450 kt.
- **Moving vehicles:** 20 mph vehicles were acquired at slightly greater range.
- **Altitude:** above about 1,500 ft, vision no longer limits search.

**perc-03: The launch-opportunity model.** [nwc_tp6005_1978]

- **Date:** January 1978.
- **P_max** depends on contrastiness and pattern:

  | Pattern | High contrastiness | Medium | Low |
  |---|---|---|---|
  | Large | 1.00 | 0.75 | 0.50 |
  | Medium | 0.75 | 0.56 | 0.37 |
  | Small | 0.50 | 0.37 | 0.25 |

- **Range rule:** let R* = min(unmask range, visibility).
  - P = 0.5·P_max at 0.5 R* (the median acquisition range);
  - P = 0.2·P_max at 0.625 R*;
  - P = 0.8·P_max at 0.375 R*.
- **Pattern and contrastiness examples:**
  - an SA-2 site's symmetrical pads and roads score high on pattern;
  - AA guns sit on graded areas;
  - a fresh vehicle park scores low.
- **Sample inputs:** decision 1 s, roll-in 0.5 s, 450 kt, minimum release range 3,000 ft, 3 g, roll-out 1 s.
- **Results with P_max 0.75:**
  - flat open terrain: launch probability about 0.50;
  - sharply rolling terrain at low altitude: 0.05;
  - sharply rolling terrain at 4,000 ft: 0.25.
- **Operating time:** raising it from 2 to 10 s drops the launch probability from 0.5 to 0.35.
- **Off-angle:** about 1 % is lost per 3° off the target line.
- **Validity:** 500-2,500 ft and 350-550 kt.

**perc-04: Search and recognition times.** [jones_1974_ta_sourcebook; nwc_tp6005_1978]

- **Search time:** exponentially distributed.
- **Briefed large targets:** mean time to detect 3.7-8 s. This is Freitag and Jones (1972): pre-briefed observers
  searching a TV display of a scale terrain model for bridges and road junctions. It is a simulation, not flight.
- **Helicopter pop-up:** 50 % of armoured targets found in 20 s, none after 48 s.
- **Fixation:** 0.33 s on large fields, 0.6 s on small displays.
- **Recognition after detection:** 2.7 s (Duntley 1957). Correction: this is an assumed interval in a 1970 model,
  taken from a study of small models seen from a passing automobile, and Jones reports the modelling as "very
  optimistic" against flight tests. It is a model input, not a measured pilot time.
- **Bomb-sight tracking:** about 2 s. An A-4 needs 3 s for a 17 mrad correction and 5 s for 60 mrad.
- **TV missile lock:** 5-6 s, plus 2-3 s to call up the picture, about 9 s in all. The spread runs from 2 to over
  12 s, with a peak near 5 s.
- **Model optimism:** recognition models of 1970 were optimistic against A-4 trials at 270 kt from 4,000 ft
  (Erickson and Gordon).

**perc-05: The visual detection lobe for air-to-air and missile sighting.** [nwc_tp5709_1974]

- **Date:** October 1974.
- **Foveal maximum range:** R_M = 0.1655·√((C0 − 1.565)·A), with R_M in nmi, A in ft² and C0 in %.
  - **SI form (derived here):** R_M ≈ 1006·√((C0 − 1.565)·A) m, with A in m².
  - **Example:** 55 ft² at 30 % contrast gives about 6.5 nmi with no haze.
- **Empty-field myopia:** halves that range (rest focus about 5.5 ft).
- **Taylor's field factors** raise the threshold contrast:
  - 1.64 for confidence;
  - 1.31 for an unknown location;
  - 1.19 for vigilance;
  - 2.00 for an untrained observer;
  - 5.12 for all four together, about 1.2 more for ordinary seeing.
- **Glimpse model:**
  - one glimpse is about 1.65 s;
  - glimpse probability = lobe solid angle / search solid angle (±90° azimuth, ±30° elevation in the example);
  - cumulative P = 1 − Π(1 − g_i).
- **What drives the curve:** knowing where to look ("pilot's expectation").

**perc-06: Air-to-air first sightings in Vietnam were short.** [red_baron_1967, vol. 1]

- **Date:** events before 1 March 1967; report October 1967; declassified 1987.
- **Range:** first sighting of MiGs typically at 1-5 nm. Single events range from 1 to 10 mi.
- **Contrails:** seen at about 20 nm. Vol. 1's summary (checked 15 Nov 2026): radar acquisitions were all beyond
  10 miles and visual acquisitions under 10 miles, "except for several detections of contrails at 20 miles and over".
  Single events run from about 1 to 10-12 miles.
- **Misses:** haze and cloud often caused them.
- **Not verified:** the often-quoted "80 % never saw the attacker". It was met only in a secondary briefing quote of
  Red Baron III, which was not read.

**perc-07: SAM sites are seldom seen unless they fire; launch dust and smoke give them away.**
[agard_cp100_1972, Hilgendorf B9, p. B9-9; checo_1967_tactics; checo_1969_tactics]

- **AGARD 1972:** in a simulated search no subject detected the AA site, "since Southeast Asia returnees reported
  that these sites are seldom detected unless they are firing".
- **CHECO, February 1967:** an F-105F flight "visually acquired the site because of the cloud of dust caused by the
  launch". The pilot then dived to 200 ft, watched the missile and pulled up; it burst about 500 ft below. SAM sites
  were hard to find "under superb camouflage".
- **7AF OPlan 512-8 (15 September 1967):** let the force commander divert strike flights onto SAM sites "visually
  acquired".
- **Egress height:** 5,000-8,000 ft was chosen partly to "increase visual acquisition" of SAMs.

**perc-08: Ground observers usually see the aircraft before the pilot finds his target.** [humrro_1968_ground_observer]

- **Test:** JTF-2 Test 3.1/3.5, October 1967, hilly Oklahoma and Arkansas terrain.
- **Detection range:** mean slant range 6,200 m (5,300-7,700 m by site).
- **Background:** 68 % of detections were against sky or cloud.
- **Seen before heard:** 83 %.
- **Smoke:** the F-4C's smoke made it the easiest aircraft to see.
- **Race to see first:** observers saw the aircraft before the pilot acquired his ground target in 60 % of runs.
- **Warning:** earlier tests gave about 10,000 m with 1-5 min warning. Binoculars did not help.
- **Paint (found in the check, 15 Nov 2026):** pilots acquired a SAM site with white missiles on a hillside at about
  twice the range of an olive-drab radar site on low ground, so observers at the radar site saw the aircraft first far
  more often. Relevant to an Osa battery's camouflage.
- **Not re-checked:** the 5,300-7,700 m range by site.

**perc-09: Low-level workload costs lookout; weather is the main limit.** [agard_cp267_1979, Lee B3 pp. 3-2; Rud and
Leben B22 (corrected from B23) as summarised in the technical evaluation; DeHart B4]

- **Date:** October 1979.
- **Lee (BAe test pilot):**
  - high-speed, low-level flight "requires high concentration and intensive training";
  - that concentration "results in loss of lookout", less time to navigate and to keep formation;
  - a Hunter needed VMC of 5 nm visibility and 1,500 ft cloud separation;
  - with practice a Hunter pilot could acquire a target in about 5 km visibility if he held 360 kt (the OCR of the
    visibility figure is damaged);
  - a HUD and the Tornado's terrain-following radar and autopilot widen these limits.
- **Rud and Leben:**
  - weather played a part in 10 of 19 European high-speed, low-level accidents;
  - the human factors were white-out, visual illusions, "mission completion syndrome", fatigue, stress and task
    overload.

**perc-10: Two-seat and single-seat workload at low level.** [agard_cp267_1979, DeHart B4]

- **FB-111 crews:** the mission is "at minimum a two-man job".
- **F-111 cockpit:** poor outside visibility, especially right-to-left and downward.
- **Distraction:** master caution lights from terrain-following limits pull the pilot's eyes inside.
- **A-10:**
  - frequently below 100 ft at 200-300 kt;
  - 40-60 % of a 1.5-2 h mission below 500 ft;
  - the pilot must keep "eyes-out-of-the-cockpit";
  - a surge exercise flew six missions a day for three days.

**perc-11: Central European weather forces visual attackers up.** [rand_p5962_1977]

- **Date:** August 1977.
- **Weather:** below 5,000 ft ceiling and 5 nmi visibility about 85 % of the time in winter and about 50 % in summer.
- **Recognition range:** reliable recognition of vehicle-sized targets by "sensors which operate at either infrared
  or visible wavelengths" at no more than about 3 km in relatively good weather, and possibly 1 km in poor
  visibility. The text is about sensors; it does not name the unaided eye.
- **Run-in direction:** crossing a column gives a short exposure but little time to acquire. Running down the road
  gives more time and more exposure.
- **Exposure:** about 20 s or less degrades mobile defences severely.

**perc-12: Night attack meant flares by eye in the early 1970s.** [agard_cp100_1972, Hilgendorf B9]

- **Date:** 1972.
- **Flares:** "the majority of night-time aerial activity is carried on under air-dropped, parachute illumination
  flares".
- **Flare count:** no gain from more than two flares over about 1.5 × 5 km.
- **Best height:** 610 m was best for search at slant ranges of 1,029-1,587 m in simulation.
- **Sensors:** LLTV and FLIR were then under evaluation (Jones 1974 lists them as studies, not fleet kit).

**perc-13: USAF tactical flying hours, 1980-86.** [gao_1986_flying_hours]

- **TAC average per pilot per month:**
  - FY1980: 11.4 sorties and 16.3 h;
  - FY1984-85: 13.1 sorties and 18.3 h (GAO marks the FY1985 sorties figure as an estimate, "actual data not
    available");
  - FY1986 (programmed): 13.2 sorties and 18.5 h.
- **Annual equivalents:** about 196, 220 and 222 h a year (×12, derived here).
- **1st TFW, January-June 1985, six-month sorties:**
  - inexperienced pilots 92 (standard 61, goal 116);
  - experienced pilots 87 (standard 53, goal 103);
  - instructors 103.
- **Cost per flying hour, FY1985:** A-10 $1,600, F-111 $8,300, F-15 about $5,000.

**perc-14: RAF and Luftwaffe hours, period and near-period.** [hansard_1981_raf; bt_11_4666_1989]

- **Hansard, 23 June 1981:** the Minister of State (Peter Blaker) said RAF crews "will still fly more hours than the
  minimum recommended by NATO" despite that year's flying restrictions. No number was given. Correction (15 Nov 2026):
  the remark that the USAF favours medium level and the RAF low level was made by an opposition MP (Peter Snape) in
  the debate, not by the government; it is a debating point, not an official statement.
- **Hansard, 22 March 1985 and 4 November 1986:** restrictions on RAF flying time in 1985-86 were lifted in July 1985
  (not re-checked: search budget exhausted).
- **Bundestag, 1 June 1989:** for the Tornado ECR squadron planned for Jever (deliveries to be complete at the end of
  1990), the government expected 180 flying hours a year per pilot, about 60 of them low-flying training in roughly
  equal shares at home and abroad. Correction (15 Nov 2026): a planning figure for a type not yet in service, not
  hours flown in 1989. Post-period: a check only.
- **Not found in a period source:** a NATO minimum of 180 h.

**perc-15: "The first ten missions" behind Red Flag.** [afmag_2000_redflag, Boyne, pp. 46-50]

- **Suter's argument (1975):** "the majority of combat losses occur during a pilot's first 10 combat missions";
  after that, losses "dropped nearly to zero".
- **Red Flag's answer:** let a new pilot log those ten "combat" missions in a controlled environment. The first Red
  Flag ran on 29 November 1975.
- **Red Baron, as Boyne reports it:** crews were caught by surprise, inadequately trained and not fully informed
  about the enemy.
- **Status:** this is a 2000 press account (tier 4, hindsight). Boyne writes that Suter "knew of studies"; the
  underlying loss study is not named and was not found.

### Warsaw Pact and the Soviet Union

**perc-16: Soviet tactical pilots flew fewer hours and trained on preplanned, unopposed, reconnoitred targets.**
[cia_1969_soviet_tac_training]

- **Source and view:** CIA, October 1969. This is the adversary's view.
- **Annual hours:** minimum 100-110 h; probably 125-160 h on average; one regiment flew about 12 h a month. The US
  minimum was then 240-250 h.
- **Training content:**
  - about 50 % of training was weapons training;
  - ground-attack regiments gave about 10 % to air-to-air;
  - cross-training ran 10-15 h a year.
- **Ground-attack training:**
  - preplanned and unopposed;
  - targets reconnoitred 2-3 days ahead;
  - large, well-defined targets, rocket runs with high entry and recovery altitudes, and little training in
    low-altitude fire and manoeuvre (criticised in a Red Star editorial of 28 November 1968).
- **Date caveat:** the accuracy data are from a MiG-17 regiment of the late 1960s. The CIA itself says the one
  regiment may not be representative. Using it for 1985 Su-17, MiG-27 or Su-25 units is a SAM3D extrapolation.
- **Accuracy:**
  - qualification CEP was 410 ft against the US 165 ft for a 30° dive;
  - a MiG-17 regiment averaged about 150-160 ft, about 50 % worse than US units.
- **Sorties:** the goal was 200 a day per regiment; 2-3 per aircraft per day was realistic.

**perc-17: New Soviet fighter-bombers carried laser ranging and designation, and the Hind D was thought to have
low-light TV.** [dia_1977_frontal_aviation]

- **Source and view:** DIA, August 1977. This is the adversary's view.
- **Fighter-bombers:** the new types had a laser rangefinder, a laser designator, Doppler, HUD, RWR and a computer for
  visual deliveries.
- **Hind D:** assessed to have low-light TV and a laser rangefinder.
- **Status:** these are assessments, not Soviet dates. **(unverified)**: the document could not be fetched in the
  check of 15 Nov 2026 (CIA reading room refused; no URL recorded), so neither the claims nor the date were confirmed.
  Do not put them into data until read.

**perc-18: Afghanistan, 1980-85: experienced pathfinders found targets, others struck marks.**
[markovsky_2000_afghan]

- **Pathfinders:** the most experienced Su-17 pilots, skilled at finding hard-to-see objects, flew a pair ahead.
  - They searched at 800-1,000 m and 850-900 km/h for about 3-5 minutes.
  - They marked the target with illumination and smoke bombs.
  - Correction (15 Nov 2026): 1-2 minutes after the marking an air-defence suppression group of 2-6 Su-17s arrived,
    found the DShK and ZGU positions from 2,000-2,500 m and attacked them in dives; the main strike group came
    over the target 1-2 minutes after that.
- **Other cues:** coloured smoke marked own troops, and pilots aimed at the dust clouds of artillery bursts.
- **Mi-24 crews:** relied on eye and skill more than the ASP-17V sight and its computer.
- **Status:** a Russian historian's book from 2000, built on unit material. It is operator-country, hindsight.

**perc-19: Soviet pilots were ordered to watch for MANPADS launches and evade by eye.** [markovsky_2000_afghan]

- **Orders:** in dangerous areas, pilots were told to watch for launches of "Strelas" and break hard towards the sun
  or dense cloud.
- **Helicopter drill:** on a spotted launch, a sharp turn with a salvo of flares.
- **Count, 1984:** about 50 launches, 5 hits (two aircraft, three helicopters).
- **Misreadings:** RPG airbursts were mistaken for missile bursts.
- **Identification:** "almost impossible" to tell a Stinger gunner from a peasant from the air. This is post-1985 for
  Stinger (Markovsky dates Stinger's arrival to the end of 1986); moved to section 5.
- **Late warning (found in the check):** Su-25 pilots seldom used their ASO-2V flares; by the time they noticed a
  launch it was too late, as the seeker had locked on.
- **Experience:** in 1985 a young pilot who dived to 900 m was hit by DShK fire.
- **Damage rate:** in 1985 combat damage forced one landing per 4,605 flying hours, 5.5 times better than 1980.

**perc-20: Night work in Soviet helicopters used active IR goggles and flares.** [markovsky_2000_afghan]

- **Mi-8 airfield-defence patrols:** hunted at night with PNV-57E IR goggles, lit the target with a flare, then fired.
- **Mi-24 night attacks:** required "excellent training". The example given is April 1986: flagged.
  **(unverified)**: not found in the lib.ru text in the check of 15 Nov 2026; the PNV-57E patrols were found.
- **Status:** not dated more closely than the war years.

**perc-21: The Soviet view in 1986 was that NATO had not solved night and bad-weather close support.** [mt_1986_dobedin]

- **Source and view:** Military Thought, July 1986. This is the adversary's view of NATO, just after the period.
- **A-10 variant:** Western specialists proposed one with radar, low-altitude TV, INS, infrared search and a second
  crewman "to relieve the pilot of part of the burden of target search".
- **Main night weapon:** the TV Maverick was assumed to be the principal night and bad-weather weapon.
- **(unverified):** the translation could not be fetched in the check of 15 Nov 2026 (CIA reading room refused; no URL
  recorded). Kept as context only, not data.

### Neutral, rebels, Syria, Pact allies

**perc-22: No period figures found.** None was found in this pass for:

- annual hours of Pact allies (NVA, Polish, Czechoslovak);
- Syrian annual hours;
- a small neutral's annual hours;
- rebels' annual hours.

A design suggestion is in section 3, marked as a game value.

## 3. Numbers usable as data

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| Foveal max detection range | R_M[nmi] = 0.1655·√((C0[%] − 1.565)·A[ft²]); SI ≈ 1006·√((C0 − 1.565)·A[m²]) m | general (US model) | 1974 | nwc_tp5709_1974 | high (formula), medium (use for other air forces) |
| Empty-field myopia | range ×0.5 | general | 1974 | nwc_tp5709_1974 | medium |
| Field factors on threshold contrast | 1.64 × 1.31 × 1.19 × 2.00 = 5.12 (untrained); ~1.2 more for ordinary seeing | general | 1974 | nwc_tp5709_1974 (Taylor) | medium |
| Glimpse duration | ~1.65 s | general | 1974 | nwc_tp5709_1974 | medium |
| Air-to-ground P_max | table in perc-03 (0.25 to 1.00) | NATO (US study) | 1978 | nwc_tp6005_1978 | medium |
| Acquisition range rule | P = 0.2/0.5/0.8·P_max at 0.625/0.5/0.375 × min(unmask, visibility) | NATO | 1978 | nwc_tp6005_1978 | medium |
| Launch probability, flat open / rolling low / rolling 4,000 ft | 0.50 / 0.05 / 0.25 (P_max 0.75) | NATO | 1978 | nwc_tp6005_1978 | medium |
| Operating time effect | 2 → 10 s: 0.50 → 0.35 | NATO | 1978 | nwc_tp6005_1978 | medium |
| Off-angle loss | ~1 % per 3° | NATO | 1978 | nwc_tp6005_1978 | low |
| TV missile lock time | ~9 s total (2 to >12 s, peak ~5 s) | NATO | 1978 | nwc_tp6005_1978 | medium |
| Acquisition range vs speed, 500 ft | 16,300 ft @350 kt; 15,200 @550; 11,250 @700 (differences not statistically significant) | NATO | 1964 | jones_1974_ta_sourcebook (Dyer) | low |
| Mean detect time, briefed large targets (TV display of a terrain model) | 3.7-8 s | NATO | 1972 | jones_1974_ta_sourcebook (Freitag and Jones) | medium |
| Helicopter pop-up acquisition | 50 % in 20 s; none after 48 s | NATO | ≤1974 | jones_1974_ta_sourcebook | medium |
| Recognition after detection (a 1970 model's assumption, judged optimistic) | 2.7 s | general | 1957 | jones_1974_ta_sourcebook (Duntley) | low |
| Fixation duration | 0.33 s (large field), 0.6 s (display) | general | ≤1974 | jones_1974_ta_sourcebook | medium |
| Vehicle recognition range, visible/IR sensors (eye by SAM3D inference) | ≤ ~3 km good weather, ~1 km poor | NATO | 1977 | rand_p5962_1977 | medium |
| Central Europe <5,000 ft ceiling and <5 nmi vis | ~85 % winter, ~50 % summer | NATO | 1977 | rand_p5962_1977 | medium |
| Exposure that degrades mobile SAMs | ≤ ~20 s | NATO | 1977 | rand_p5962_1977 | low |
| First sighting of MiGs | typically 1-5 nm; contrail ~20 nm | NATO (US) | 1965-67 | red_baron_1967 | medium |
| Ground observer detection of jets | mean 6,200 m slant (5,300-7,700) | general | 1967 | humrro_1968_ground_observer | medium |
| Observer sees aircraft before pilot acquires target | 60 % of runs | general | 1967 | humrro_1968_ground_observer | medium |
| Hunter VMC | 5 nm vis, 1,500 ft from cloud; ~5 km at 360 kt to acquire | NATO (RAF) | 1960s-79 | agard_cp267_1979 (Lee) | medium (OCR damaged) |
| Weather in European HSLL accidents | 10 of 19 | NATO | ≤1979 | agard_cp267_1979 (Rud and Leben) | medium |
| A-10 low-level share | 40-60 % of 1.5-2 h mission below 500 ft; often <100 ft at 200-300 kt | NATO (US) | 1979 | agard_cp267_1979 (DeHart) | medium |
| Surge rate | 6 missions/day for 3 days (A-10 exercise) | NATO (US) | ≤1979 | agard_cp267_1979 (DeHart) | medium |
| Flares for night search | no gain beyond 2 over 1.5 × 5 km; best search height 610 m | NATO (US) | 1972 | agard_cp100_1972 (Hilgendorf) | low |
| TAC flying per pilot per month | 16.3 h (FY80), 18.3 h (FY84-85), 18.5 h (FY86 prog.) | NATO (USAF) | 1980-86 | gao_1986_flying_hours | high |
| 1st TFW six-month sorties | inexp. 92, exp. 87, IP 103 | NATO (USAF) | Jan-Jun 1985 | gao_1986_flying_hours | high |
| Tornado ECR annual hours (planned, post-period: check only, not 1985 data) | 180 h, ~60 low-flying | NATO (Luftwaffe) | planned in 1989 for 1990+ | bt_11_4666_1989 | high as a plan; not for the 1985 war |
| Soviet tactical annual hours | min 100-110; avg probably 125-160 | Warsaw Pact (US view) | 1969 | cia_1969_soviet_tac_training | medium |
| Soviet vs US dive CEP | 410 ft vs 165 ft (qualification); regiment ~150-160 ft, ~50 % worse | Warsaw Pact (US view) | 1969 | cia_1969_soviet_tac_training | medium |
| Soviet pathfinder search | 800-1,000 m, 850-900 km/h, 3-5 min; AD suppression (2-6 Su-17) 1-2 min after marking, main strike 1-2 min after that | Warsaw Pact | 1980-85 | markovsky_2000_afghan | medium |
| MANPADS launches and hits, Afghanistan | ~50 launches, 5 hits | Warsaw Pact (Soviet) | 1984 | markovsky_2000_afghan | medium |
| Combat-damage forced landings | 1 per 4,605 h | Warsaw Pact (Soviet) | 1985 | markovsky_2000_afghan | medium |
| Losses concentrated in first combat missions | first 10 missions; then "nearly zero" | NATO (US) | argued 1975 | afmag_2000_redflag | low (no primary data) |

**Game values, not sourced.** These are suggestions only, so the model has a place for each faction:

- **Hours as a skill input:** NATO about 180-220 h a year; Soviet frontal aviation lower (the 1969 figure of 125-160 h
  is the only period figure).
- **Pact allies, Syria and the small neutral:** set by analogy and marked as SAM3D estimates.
- **Rebels:** few hours and no combat experience.

## 4. Disagreements

1. **Speed.** Corrected 15 Nov 2026: this is not really a disagreement. Dyer (1964) found mean acquisition range
   lower at higher speed, most at 700 kt, but Jones reports the differences as not statistically significant, and
   probability unaffected from 350 to 700 kt. Valentine (1972) found no significant effect between 300 and 450 kt.
   **Used:** probability flat from 350 to 550 kt; Dyer's range figures only as a weak trend. Speed acts mainly
   through less time to search. NWC TP 6005 is valid only 350-550 kt.
2. **SAM site visibility.** NWC TP 6005 counts an SA-2 site's pads and roads as a strong pattern (high P_max). CHECO
   1967 and AGARD 1972 say sites under camouflage were seldom seen unless firing. An Osa battery has no pads, so
   **used:** a small pattern and low contrastiness until it fires; firing raises it to large and high.
3. **Models against trials.** The 1970 recognition models were optimistic against A-4 trials (Erickson and Gordon).
   **Used:** the TP 6005 empirical curve over pure Johnson criteria for ground targets.
4. **Soviet hours.** The CIA gives 125-160 h a year (1969). Russian forum claims of 100-220 h for the 1980s are
   tier 5 and not used. There is no 1980s period figure.
5. **"80 % never saw the attacker" and "first ten missions."** Both are widely quoted. Neither was found in a
   primary document read here. Red Baron vol. 1, as read, gives sighting ranges, not these shares. Both are kept as
   low confidence.
6. **NATO minimum hours.** A 180 h NATO minimum appears in press and forums. The period Hansard answer (1981) says
   only that the RAF flew "more than the NATO minimum". Not used as data.

## 5. Post-period items excluded

- Bundestag Drucksache 11/4666 (1 June 1989): Tornado ECR 180 h a year per pilot, a planning figure for a squadron
  still to be delivered (end of 1990). Post-period; used only as a check.
- Stinger's identification problem ("a Stinger gunner from a peasant", Markovsky): Stinger arrived at the end of
  1986 by Markovsky's account; not part of the 1985 war.
- Hansard 1998 written answers: Tornado GR1 210, Harrier 206, Jaguar 197 h a year.
- Stinger in Afghanistan (from 1986) and the Mi-24 night attack of April 1986 (Markovsky).
- Military Thought, July 1986 (Dobedin): a Soviet view just after the period; used only for the state of NATO
  night CAS.
- LANTIRN and NVG-aided night low-level strike in fast jets: after 1985. No source read here; excluded.
- FAA "See and Avoid / Cockpit Visibility" (October 1989, DTIC ADA214214): downloaded, not used.
- Air Force Magazine, November 2000 (Boyne): a hindsight account of a 1975 argument.
- The 1991 Gulf War: not used.

## 6. Sources

| Id | Tier | Citation | URL | Read | Whose view |
|---|---|---|---|---|---|
| `red_baron_1967` | 1 | IDA, WSEG Report 116, *Red Baron: Air-to-Air Encounters in Southeast Asia*, vol. 1, J. S. Attinello (project leader), October 1967; declassified 28 October 1987 | archive.org/download/DTICWSEGReport116Volume1RedBaronAirtoAirEncountersSoutheastAsia28October1987 | in part (sighting sections) | US of its own crews |
| `humrro_1968_ground_observer` | 1 | HumRRO consulting report on ground observers' detection of jets, JTF-2 Test 3.1/3.5 (October 1967 test), about 1968, DTIC ADA020657 | apps.dtic.mil/sti/pdfs/ADA020657.pdf | full (OCR) | US |
| `nwc_tp5709_1974` | 1 | J. R. Erickson, C. G. Burge, *Modeling Air-to-Air Visual Search*, NWC TP 5709, Naval Weapons Center, October 1974, DTIC ADB001148 | apps.dtic.mil/sti/tr/pdf/ADB001148.pdf | full (OCR) | US |
| `jones_1974_ta_sourcebook` | 1 | D. B. Jones et al., *Air-to-Ground Target Acquisition Source Book: A Review of the Literature*, Martin Marietta for ONR, September 1974, DTIC ADA015079 | apps.dtic.mil/sti/tr/pdf/ADA015079.pdf | in part (summaries and chapters searched) | US |
| `nwc_tp6005_1978` | 1 | J. R. Erickson, C. G. Burge, *Launch Opportunity for Air-to-Ground, Visually Delivered Weapons*, NWC TP 6005, January 1978, DTIC ADB026467 | apps.dtic.mil/sti/pdfs/ADB026467.pdf | full (OCR) | US |
| `agard_cp100_1972` | 1 | AGARD CP-100, *Air to Ground Target Acquisition*, Aerospace Medical Panel, Brussels, 31 May-1 June 1972 (Hilgendorf B9; Bailey B10), DTIC AD-755082 | apps.dtic.mil/sti/tr/pdf/AD0755082.pdf | in part (B9, B10) | NATO |
| `agard_cp267_1979` | 1 | AGARD CP-267, *High-Speed, Low-Level Flight: Aircrew Factors*, Lisbon, 24-26 October 1979 (Lee B3; DeHart B4; technical evaluation), DTIC ADA087592 | archive.org DTIC_ADA087592 | in part (evaluation, B3, B4) | NATO |
| `rand_p5962_1977` | 1 | N. W. Crawford, *Low Level Attack of Armored Targets*, RAND P-5962, August 1977, DTIC ADA054186 | archive.org DTIC_ADA054186 | full | US |
| `checo_1967_tactics` | 1 | already registered (CHECO, February 1967, DTIC ADA586117) | archive.org/details/DTIC_ADA586117 | in part | US |
| `checo_1969_tactics` | 1 | already registered (CHECO 1969, DTIC ADA486448) | archive.org/details/DTIC_ADA486448 | in part | US |
| `gao_1986_flying_hours` | 1 | GAO, *Aircrew Training: Tactical Air Command...*, GAO/NSIAD-86-192BR, September 1986 | www.gao.gov/assets/nsiad-86-192br.pdf | full | US of its own forces |
| `hansard_1981_raf` | 1 | House of Commons debate, Royal Air Force, 23 June 1981; written answers 22 March 1985 (RAF flying time) and 4 November 1986 (RAF flying hours restriction) | api.parliament.uk/historic-hansard/commons/1981/jun/23/royal-air-force (and the two written answers) | full (those pages) | UK of its own forces |
| `bt_11_4666_1989` | 1 | Deutscher Bundestag, Drucksache 11/4666, 1 June 1989, answer to a minor interpellation on the future use of the Upjever (Jever) air base, including the planned Tornado ECR squadron's hours | dserver.bundestag.de/btd/11/046/1104666.pdf | full | FRG of its own forces |
| `cia_1969_soviet_tac_training` | 1 | CIA Directorate of Intelligence, Intelligence Report SR IR 69-20, *Operational Training and Ground Attack Proficiency of Soviet Tactical Fighter Units*, October 1969 | archive.org/details/cia-readingroom-document-5166d4f999326091c6a608f3 | full | adversary's view (US of Soviet) |
| `dia_1977_frontal_aviation` | 1 | DIA, *Soviet Frontal Aviation Support to Ground Forces Operations*, August 1977, CIA-RDP83M00171R001100080001-7 | CIA reading room record (URL not recorded) | in part | adversary's view (US of Soviet) |
| `mt_1986_dobedin` | 1 | Col. V. I. Dobedin, "Problems of Air Support for Troops", *Voyennaya Mysl'* No. 7, July 1986, pp. 31-40 (CIA translation, CIA-RDP10-00105R000403380001-8) | CIA reading room record (URL not recorded) | in part | adversary's view (Soviet of NATO) |
| `markovsky_2000_afghan` | 4 | В. Марковский, *Жаркое небо Афганистана*, «Техника — молодёжи», 2000 | lib.ru, MEMUARY/AFGAN/nebo.txt | in part (searched) | operator country (Soviet) |
| `afmag_2000_redflag` | 4 | W. J. Boyne, "Red Flag", *Air Force Magazine*, November 2000, pp. 46-50 | airandspaceforces.com magazine archive (URL not recorded) | full | US press, hindsight |

All new ids above are proposals. Only `checo_1967_tactics` and `checo_1969_tactics` are already in `docs/SOURCES.md`.

## 7. Open items

- **Sensor in-service dates.** These are not sourced in this pass, because the search budget ran out:
  - Pave Spike (F-4);
  - Pave Tack (F-111F, about 1982 per the task);
  - Pave Penny (A-10);
  - AGM-65A as a finder;
  - F-111, Tornado and F-4E radar ground mapping;
  - Kaira-23 (MiG-27K);
  - Klen-PS (Su-25);
  - Mi-24 sights;
  - AN/PVS-5 and ANVIS in US Army helicopters.

  Acquire a period or official source for each, such as USAF and Army histories and Soviet design-bureau histories.
- **9M33 smoke trail as seen by pilots.** No source found. `data/scenery/munitions.json` keeps "dense" as an
  estimate. Osa doctrine mentions "launch smoke on the TV" only indirectly.
- **AAA tracer and bursts by night and day as seen from the cockpit.** No period number found beyond CHECO
  narratives.
- **Red Baron III executive summary.** Source of the "never saw the attacker" share.
- **JTF-2 Test 4.1 results** (classified) and AFATL-TR-73-140 (Thornton, Erickson, Bruns 1973, confidential).
- **Hours for the RAF and Luftwaffe in 1985**, Soviet frontal aviation in the 1980s (a period source), Pact allies,
  Syria, and a small neutral.
- **The primary loss study** behind "first ten combat missions".
- **Target fixation.** Period evidence was not found beyond the CHECO dive and the Afghan dive to 900 m.
- **Two-seat crew coordination data** beyond DeHart's FB-111 remark.
- **FAA 1989 See and Avoid** (ADA214214): may hold see-and-avoid reaction-time figures. Post-period, but a check.

## Verification (15 Nov 2026)

An adversarial check of the most load-bearing findings against the sources themselves. Full texts were read from the
archive.org OCR of the DTIC reports (ADB001148, ADB026467, ADA020657, ADA015079, ADA054186, ADA087592, AD0755082),
the CIA's 1969 report (archive.org OCR), the GAO report (PDF text), Red Baron vol. 1 (archive.org OCR), Markovsky
(lib.ru text), Boyne (Air Force Magazine PDF), the Bundestag answer (PDF) and the 1981 Hansard debate (first third of
the page). The CIA reading room refused the DIA 1977 and Military Thought 1986 documents, and the search budget ran
out before the 1985-86 Hansard written answers could be found.

| Finding | Verdict | Note |
|---|---|---|
| perc-01 | confirmed | Jones 1974: "major source of variance (62 percent)" is target and background (Wyman et al. 1968), "for the most part, unexplained"; Bailey's checklist items are in CP-100. |
| perc-02 | corrected | Dyer's numbers are right, but Jones says the range differences were not statistically significant and probability was unaffected from 350 to 700 kt; "sharply above 550 kt" removed. |
| perc-03 | confirmed | NWC TP 6005 (January 1978): Table 2, the 0.2/0.5/0.8 at 0.625/0.5/0.375 rule, 0.50/0.05/0.25, 2 to 10 s gives 0.5 to 0.35, about 1 % per 3 deg, 500-2,500 ft and 350-550 kt all found. The text says "SAM site", not SA-2. |
| perc-04 | corrected | 9 s TV lock and 2 to over 12 s found in TP 6005; 3.7-8 s is a TV-display terrain-model simulation (Freitag and Jones 1972); 2.7 s is a model assumption Jones calls very optimistic. |
| perc-05 | confirmed | NWC TP 5709: Eq. 7, R_M = 0.1655 sqrt((C0 - 1.565)A), factors 1.64, 1.31, 1.19, 2.00 = 5.12, about 1.2 more, glimpse 1.65 s, empty-field halving, 55 ft2 at 30 % example; SI constant 1006 m rechecked. |
| perc-06 | confirmed | Red Baron vol. 1: visual acquisitions under 10 miles except contrails at 20 miles and over; event narratives at 1-12 miles; haze losses found. |
| perc-07 | confirmed | AGARD CP-100 p. B9-9 quote found (in a night flare-search simulation). CHECO items not re-read (already registered sources). |
| perc-08 | confirmed | HumRRO: about 6,200 m, 68 % sky or cloud, 83 % seen before heard, F-4C smoke, 60 % before pilot acquisition, about 10,000 m earlier, binoculars no help. By-site range not re-checked. Added: white missiles drew pilots' eyes. |
| perc-09 | corrected | Lee's "loss of lookout" and Rud and Leben's 10 of 19 found; their paper is B22, not B23. |
| perc-10 | confirmed | DeHart: "at minimum a two-man job", A-10 frequently below 100 ft at 200-300 kt, eyes-out-of-the-cockpit. |
| perc-11 | corrected | RAND P-5962: 85 % / 50 %, 3 km / 1 km and 20 s found; the 3 km limit is stated for IR and visible sensors, not explicitly for the eye. |
| perc-13 | confirmed | GAO Table IV.2 and Table I.1 match; FY1985 sorties are a GAO estimate; cost per hour figures match. |
| perc-14 | corrected | 1981: the low-level vs medium-level remark is an opposition MP's (Snape), not the government's; 1989: 180 h is a plan for an ECR squadron not yet delivered (post-period). 1985-86 Hansard not re-checked. |
| perc-15 | confirmed | Boyne 2000 says Suter "knew of studies" on the first 10 missions; first Red Flag 29 Nov 1975. Hindsight press (tier 4); the study is not named. |
| perc-16 | confirmed | CIA SR IR 69-20, 14 Oct 1969: 100-110 h minimum, 125-160 h average, about 12 h a month, US 240-250 h, CEP 410 vs 165 ft, 150/160 ft, 200 sorties a day, 2-3 realistic, Red Star 28 Nov 1968. Adversary's view, late-1960s MiG-17 data: date caveat added. |
| perc-17 | unsupported | DIA 1977 not retrievable (reading room refused; no URL recorded). Marked (unverified). |
| perc-18 | corrected | Pathfinder figures confirmed; the timeline was compressed: AD suppression group 1-2 min after marking, main strike 1-2 min after that. ASP-17V passage confirmed. |
| perc-19 | confirmed | Orders to watch for "Strelas" and break toward the sun or dense cloud; about 50 launches and 5 hits in 1984; RPG airbursts; 1 forced landing per 4,605 h in 1985, 5.5 times better than 1980; the 900 m dive. Stinger (end of 1986) moved to section 5. |
| perc-20 | corrected | PNV-57E night patrols with flares confirmed; the Mi-24 "excellent training" and April 1986 example not found: marked (unverified). |
| perc-21 | unsupported | Military Thought July 1986 not retrievable. Marked (unverified); post-period by date in any case. |

**Period scan.** Nothing after 1985 is presented as 1985 practice in the body except the Tornado ECR hours (now
labelled a 1989 plan for 1990 and later) and Stinger (moved to section 5). The Tornado's terrain-following radar
(Lee, 1979) and the TV Maverick were in service by 1985. The sensor in-service dates in section 7 remain unsourced and
must not become data until a period source is found.

**Tier scan.** No tier-5 page carries any finding. The CIA, DIA and Military Thought items are labelled as the
adversary's view. Air Force Magazine (2000) and Markovsky (2000) are tier 4 hindsight as stated.
