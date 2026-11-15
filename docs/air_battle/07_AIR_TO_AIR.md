# 7. Air combat

Written 15 November 2026. Design, nothing built. How our fighters and the enemy's fight each other in the war of 1985:
the weapons and sensors each side had, the shot as a chain of chances, each faction's tactics, and how SAM3D builds it
at the gameplay level. Code it touches: `simcore/include/simcore/air_war/data.hpp` (`AircraftType`, the loader),
`types.hpp` (`FlightRole`), `planner.hpp` (escort sizing, `ExpectedLosses`), `resolve.hpp` (the fighter roll),
`raid_scenario.hpp`, `package_injector.hpp`, `mission_bridge.hpp`, `state.hpp` (`LossRecord`, `PackageOutcome`),
`campaign/outcomes.hpp`, `em/model.hpp` and `em_link.hpp` (airborne radars, RWRs), `scenario_runner.hpp`; new
`air_war/air_combat.hpp`, `air_fight.hpp` and `data/air_war/air_combat.json`. Research it rests on:
`docs/research/air_battle_2026-11/aam_weapons.md` and `air_combat.md` (main), `perception.md`, `reactions.md`,
`factions_cases.md`, `our_side_air.md`; code maps `map/air_to_air.md` (main), `map/sensing.md`, `map/flight.md`,
`map/scale.md`.

## 1. What there is today

No aircraft fights another anywhere in SAM3D (`map/air_to_air.md`). Our fighters are one number per front,
`FrontDef::friendly_fighters` (0..1, `data.hpp:158`). The planner sizes escorts from it (`planner.hpp:733-753`) and the
resolver turns it into a flat loss chance on a package's main flights, once, on the second route leg, times 0.3 with an
escort and 0.5 at night (`resolve.hpp:231-247`, `attrition.escort_fighter_kill` 0.03). Escorts fly the strike's
route and fight nothing; they are never lost and are the first flights cut from a played raid. `AircraftType::air_to_air`
is loaded and never read. No type carries a radar, an air-to-air missile or a gun load; no AAM exists in
`weapons.json` or `munitions.json`. Airborne fire-control radars exist only as EM emitters for tests
(`nato_fighter_fcr_x`). Our flights in the testbed fly CAP and intercept tracks and engage nothing. A loss to fighters
is written `LossRecord::by = ""`, "fighters or unknown".

## 2. Weapons and sensors in service by 1985

### 2.1 The rule

Every missile, gun, radar, infrared sight, warning receiver, flare dispenser and IFF set enters data with its
`in_service_year` and is gated by `TheatreDef::year`, as aircraft and ARMs are. Soviet equipment gets a second year,
`allies_year`, for when the Pact allies and our side held it: the R-73 was adopted in 1984 but first exported in 1988
(`airwar_ru`, R-73). Where a date is not established the entry carries the research's "(unverified)" and the earliest
year that is safe for 1985 is used only if the research says the item existed by then.

### 2.2 NATO

| Item | In service | What it gives the fight | Source |
|---|---|---|---|
| AIM-9J | from 1972 (USAF, mostly rebuilt B/E) | rear aspect; actuators "doubled the single-plane G" | `parsch_dusrm` |
| AIM-9L | production 1978 | all-aspect, cooled InSb seeker, laser fuze, 9.4 kg warhead | `parsch_dusrm` |
| AIM-9M | production 1982, deliveries 1983 | as 9L, better flare rejection and background discrimination, reduced smoke | `parsch_dusrm`, `navy_factfiles` |
| AIM-9P | date not found; P-4 all-aspect | export rear-aspect round | `parsch_dusrm` |
| AIM-7F | production 1975-81 | semi-active, dual-thrust motor, 39 kg; 5-50 km in the Soviet reading | `parsch_dusrm`, `dubrov_1983` |
| AIM-7M | production 1982 | inverse monopulse seeker for low altitude and jamming | `parsch_dusrm` |
| Sky Flash, Magic 1, Super 530F | dates and envelopes not found | none until sourced | `aam_weapons.md` §7 |
| M61, Aden, DEFA | loads and rates not found | guns at visual range | `aam_weapons.md` §7 |
| F-15 radar (APG-63 class) | with the F-15 | lock on the nose to 50 nm; about 50 km against clutter and over 80 km looking up in the Soviet view; two Eagles search from the ground to 40,000 ft | `lambeth_1984`, `dubrov_1983`, `anderegg_2001` ch. 15 |
| F-4 radar (APQ-120 class) | with the F-4E | no look-down mode, in the Soviet view | `airwar_ru`, MiG-23M |
| Optical aids | TISEO on the F-4E; rifle scopes ("Eagle Eye") on F-15s | identification a little further out | `anderegg_2001` pp. 33, 161 |
| IFF | Mark X and XII; the Mark XII "subject to spoofing"; no agreed Mark XV | no reliable identification beyond visual range | `gao_1985_id`, `cbo_1986_amraam` |
| E-3A | 18 planned by mid-1985, 12 delivered by late 1984 | warns and vectors fighters by voice | `afmag_1984_nato`, `rippe_1985_airland` (`c2_retasking.md` c2-13) |

Two rules follow. **The F-16A carries no radar missile**: "F-16s cannot carry Sparrows" (`cbo_1986_amraam` ch. 2), so in
1985 it fights with the AIM-9 and its gun. **A semi-active shooter illuminates to impact**: the Sparrow homes on the
launcher's radar, and the AIMVAL trials found fighters "forced to continue to illuminate the target were especially
vulnerable" (`cbo_1986_amraam` n. 4). No NATO fighter of 1985 had a launch-and-leave missile. The Tornado F3 is not a
1985 fighter: its Foxhunter radar still fell short of the RAF's requirement in 1987 (`hansard_1987_foxhunter`, snippet).

### 2.3 Warsaw Pact

| Item | In service | What it gives the fight | Source |
|---|---|---|---|
| MiG-23ML | built for the Soviet air force 1976-81, export to 1985; the Pact's standard MiG-23 in 1985 | | `airwar_ru` MiG-23ML; `air_combat.md` §4.6 |
| MiG-23MLD | series from 1984, 66 new in 1984-85 and conversions; a few units | | `airwar_ru` MiG-23MLD |
| Sapfir-23ML radar | with the ML | 85 km on paper; 50-55 km with the pilot searching by hand, 80-85 km under ground guidance; lock head-on at 30-50 km | `airwar_ru` MiG-23ML |
| TP-23M infrared search set | with the ML | 35 km from the rear against a clear sky | `airwar_ru` MiG-23ML |
| Lazur-SML / Lazur command link | ML, MLD | the ground's commands to the autopilot | `airwar_ru` |
| SPO-10 warning receiver | ML | type and rough direction of a radar | `airwar_ru`; `spo10_tulin` |
| SPO-15L, SPS-141 jammer, Parol IFF | MLD (not tied to the MLD in the legible text: unverified) | | `airwar_ru` MiG-23MLD |
| BVP-50-60 dispensers, 60 × 50 mm LO-43 flares | MLD, 1984-85 | defeats some IR shots | `airwar_ru` MiG-23MLD |
| R-24R, R-24T | adopted for the MiG-23ML by decree of 6 April 1981 | R-24R semi-active with 10 s of inertial flight, fired about 30 % beyond seeker lock; R-24T all-aspect IR; 32 km head-on and 20 km rear (text); minimum 0.5 km; 35 kg rod warhead, 10 m | `mis` R-24 |
| R-23R, R-23T | serial production 1973 | 25 km head-on, 15 km rear (implied); the first Soviet missile able to engage a target against the ground | `mis`, `ru_wiki_aam` |
| R-60 | adopted 18 December 1973 | rear hemisphere only; 7.2 km at about 12 km height, about a third near the ground; minimum 0.25 km; launch at up to 7 g; seeker designation ±12° | `airwar_ru`, `ru_wiki_aam` |
| R-60M | adoption 1977 or early 1980s (unverified) | cooled seeker, ±17°, head-on in close combat | `ru_wiki_aam` |
| R-73 | adopted 22 June 1984; first export 1988 | 30 km head-on; 45° with the aircraft's sights | `airwar_ru` R-73 |
| GSh-23L | adopted 1965 (Wikipedia only: unverified); 200 rounds on the MLD | about 4 s of firing on the MiG-23M (Soviet comparison) | `ru_wiki_aam`, `airwar_ru` |

The R-73 came with the MiG-29, whose entry into frontal units is not established from a usable source
(`aam_weapons.md` aam-wp-13). It enters data with 1984 and `allies_year` 1988, and no aircraft of the Pact faction in a
1985 war carries it. The R-27 family was adopted in 1987-90 and is a later war's weapon. The R-24's export to the Pact
allies is undated (unverified); the faction's own MiG-23s are Soviet frontal aviation and carry it. Calculated kill
chances are design figures, not combat rates: the R-60's designers gave 0.5 for one missile and 0.7-0.8 for a pair, the
R-60M 0.9 (`airwar_ru`; `ru_wiki_aam` after Markovsky and Perov 2005, not read), while about 30 Syrian R-3S are said to
have failed to guide in 1973 (`ru_wiki_aam`, tier 5 trace: not data). SAM3D uses a design figure only as the ceiling of
the last link in the chain (§3).

### 2.4 Our side

| Type | Weapons | Sensors | Source |
|---|---|---|---|
| MiG-21MF, MiG-21bis (fighter regiments 1-3) | up to four R-13M, K-13, R-60 or R-60M; GSh-23L | RP-21, RP-22 radars not found in a usable source; IFF Kremniy-2M | `airwar_ru` MiG-21MF, R-60; `our_side_air.md` (`soviet_iff`) |
| MiG-23ML (twelve, fighter regiment 3) | R-24R/T, R-23R/T, R-60, R-60M; GSh-23L | Sapfir-23ML, TP-23M, SPO-10, SRO-2M, Lazur-SML | `mis` R-24; `airwar_ru` MiG-23ML |

The R-13M's adoption on 3 January 1974 rests on an unfootnoted Wikipedia paragraph (unverified). The R-24 was exported
with the MiG-23 to the Pact allies, but the source gives no dates (`mis`, unverified for 1985): SAM3D's MiG-23ML carries it,
with `allies_year` 1981 (a SAM3D estimate, low confidence) until a date is found. The R-13M and R-60 are
rear-aspect: our MiG-21s must get behind, the enemy's F-16 with the AIM-9L need not (`aam_weapons.md` summary 6). A war
with the Pact puts MiG-23s on both sides with different IFF generations (SRO-2M against "Parol"); the sources do not say
whether one answers the other, so identification there is by ground control and by eye (`aam_weapons.md` aam-our-03).

### 2.5 The neutral and the rebels

The neutral of 1985 has no air-to-air missiles: on the Austrian analogue its combat jets were Saab 105Ö light jets with
guns, and its Drakens came in 1988, guns only (`austrian_af_wiki`; tier problem, the State Treaty's Article 13 not
fetched: unverified). The rebels have no guided air-to-air weapon: a defected G-2 Galeb's guns, door guns and rockets on
helicopters (the theatre's data). Whether a Mi-24 could carry the R-60 is unverified.

### 2.6 Later wars' options, kept out

AIM-120 AMRAAM (1991), AIM-9X and ASRAAM, the Mark XV IFF, the R-27 family (1987-90), the R-24M (1986), the R-73 and
MiG-29 with the Pact allies (1988), the MiG-23MLG/MLS/MLDG with the R-73 and a helmet sight (never taken further),
the Tornado F3 as a combat-ready fighter, JTIDS in fighters, "Fox three" for an active missile, the A-50 as a 1985
controller (`aam_weapons.md` §5, `air_combat.md` §5). They may appear only as a later war's data with their own years.

### 2.7 Today's data that this changes

- `faction_neutral.json`'s `draken_gun` interceptor is a 1988 aircraft: give it `in_service_year` 1988 and let the
  Saab 105Ö fly the neutral's 1985 escort and interception, guns only.
- The Pact's `iap_mig23` squadron flies the MiG-23MLD; add the MiG-23ML as the 1985 standard and keep the MLD in a few
  squadrons (`air_combat.md` §4.6).
- The F-16A's `escort` role and `air_to_air` 0.75 stand for a radar-missile fighter it was not: it escorts, as a
  visual-range fighter with AIM-9 and gun.
- The A-50's `in_service_year` 1985 lets it control in 1985; the research does not use it for 1985 Pact control
  (`air_combat.md` §5, `factions_cases.md` §5): set 1986 or keep it but give it no fighter control role.
- `AircraftType::air_to_air` is retired once every fighter has a fit (§6.1).

## 3. The shot: a chain of chances

### 3.1 The chain

A missile kills when four things hold, each a chance set from period numbers (`aam_weapons.md` summary 1 and §8):

`P(kill) = P(in envelope) × P(works) × P(guides) × P(lethal)`

| Link | What sets it | Period anchor |
|---|---|---|
| In envelope | the pilot's judgement of range, aspect, load and closure against the missile's real zone; skill, stress, sensor, the rules | 35 % of AIM-7 and 26 % of AIM-9 launches outside permissible parameters, 23 April-8 July 1967 (`checo_1967_a2a` pp. 44-45) |
| Works | the round after carriage, and the switches | 25-30 % of AIM-7E-2 and AIM-9E showed guidance failure after 10-15 captive flights; "many missile failures in-flight were the result of incorrect switch positions" (`checo_1978_linebacker` pp. 30-31) |
| Guides | aspect against the seeker's class; the sun; the ground behind the target; flares, chaff, jamming; the target's break and its timing; for a semi-active round, the shooter's lock held to impact | K-13: no guidance within 30° of the sun, great difficulty to 50°, ground clutter serious (`checo_1967_a2a` pp. 19-20, the US view); AIM-7 weak in the beam against the ground (`afit_1982_vid` ch. 1); MiG pilots defeated both US missiles "with a rapid turning descent" (`aces_1976` p. 10) |
| Lethal | fuze and warhead against the miss | the designers' kill chances as ceilings: R-60 0.5, R-60M 0.9 |

The overall per-launch rates the chain must reproduce for 1967 crews are 11.1 % for the AIM-7 and 18.6 % for the AIM-9,
probables included (`checo_1967_a2a` p. 44). Taking every hit as in-envelope gives about 0.17 and 0.25 per in-envelope
launch (`aam_weapons.md` §3, derived). With the 1972 reliability of 0.70-0.75 applied to 1967 rounds (a SAM3D
assumption), `P(guides) × P(lethal)` for an in-envelope 1967 round is about 0.23 for the AIM-7 and 0.34 for the AIM-9.

### 3.2 Each link as SAM3D computes it

- **In envelope.** The pilot fires when he believes he is inside the zone. His belief of range, aspect and closure
  carries an error that shrinks with skill and grows with stress and workload; a launch outside the true zone fails.
  So out-of-envelope shots come from judgement, not from a separate roll, and the error's spread by skill is the game
  value calibrated until a 1967 crew misjudges 35 % (AIM-7) and 26 % (AIM-9) of its shots. The zone itself is data:
  head-on and rear maximum at height, a low-level factor (the R-60's third near the ground), minimum range, the
  carrier's load at launch (K-13 under 2 g below 47,500 ft in the US view; R-60 up to 7 g), and the seeker's angle off
  the nose (R-60 ±12°, R-60M ±17°, R-73 45°). A rear-aspect seeker has no head-on zone at all.
- **Works.** `reliability` per missile, a SAM3D estimate above 0.70-0.75 for a fresh AIM-9L or R-60M; a switch error
  added by skill and stress when the pilot changes mode after hard manoeuvring or from ground attack to air-to-air (the
  F-105 pilots' complaint of "complicated switch settings", `red_baron_vol2` II-21, II-23).
- **Guides.** A base by guidance class, then multipliers: aspect within the class's cone; the sun inside the seeker's
  exclusion (sourced only for the K-13 family); the target below the shooter against the ground for a K-13 or a pulse
  semi-active round; flares in the cone for IR rounds (AIM-9M and cooled seekers resist better, a game value); chaff or
  a self-protection jammer for a semi-active round (the EM layer's J/S on the shooter's radar, §6.2); the shooter's
  lock lost before impact for a semi-active round, after the R-24R's 10 s of inertial flight; the target's break,
  which counts most when it starts at the right moment (`reactions.md` summary 3: timing decided survival).
- **Lethal.** The design figure as the ceiling, where one exists; elsewhere a game value. The warhead's lethal radius
  (R-60 2.5 m, R-24 10 m) sets how much a miss distance from a late break costs.

### 3.3 Guns

Guns stayed important: 40 of the USAF's 137 MiG kills in 1965-73 were by gun and 2 by missile and gun together
(`aces_1976` Table 4); 7 % of Israeli kills in 1982 (`lambeth_1984`); 4 of at least 20 Sea Harrier kills
(`usn_falklands_1983`). A gun pass needs the shooter behind and close; before the SUU-16 pod the F-4C pilot "would often
find himself too close to the MiG, when only a gun could be used" (`checo_1967_a2a` p. 45). SAM3D's gun is a pass, not
rounds: a firing window in seconds from the type's ammunition (MiG-23M about 4 s, F-4E 6-7 s, Mirage F1 over 10 s in a
Soviet comparison, low confidence; GSh-23L 200 rounds), an effective range (the US view of the NR-23: 3,500 ft) and a
kill chance per second inside range and angle, a game value calibrated to the gun shares above.

### 3.4 Skill

Skill decides more than the missile. The USAF's exchange ratio was 2.25:1 in 1965-68 but fell to about 1:1 in 1968,
with F-4 crews flying six air combat training sorties a half-year against the Navy's up to 20 (`mcallister_1985` p. 17).
The Navy's kills per engagement rose to 1.04 after Top Gun (`vangilder_1979`). A pilot's chance of being killed in a
decisive encounter fell with each one he survived, levelling at about 0.02 after ten (`bilikam_1970`, after Weiss, World
War data). Skill moves the envelope judgement, the switch error, the reaction delay to a shot (`reactions.md` §2.6:
veteran 1-3 s, trained 2-5 s, green 4-10 s, design estimates), the sort error and how often the wingman is lost. The
faction means are `hot_phase.air_combat_skill` (`10_FACTIONS.md` §7.2): NATO 0.8, the Pact 0.5 (clients 0.3), the
neutral 0.5, the rebels 0.2, our side 0.45 (`factions_cases.md` §1, SAM3D estimates).

## 4. Tactics, faction by faction

### 4.1 NATO

- **Rules of engagement.** NATO's IFF was mixed and unreliable, so a visual identification was required before a radar
  missile could be fired at an unknown in central Europe (`afit_1982_vid` ch. 1). "Without reliable IFF we will, in most
  cases, remain bound to visual rules of engagement" (`lambeth_1985`). Visual range was about three to four miles
  (`cbo_1986_amraam` n. 4). The ident pass (the leader identifies, the trailer fires) or a bracket put the F-15s "well
  within the range of the enemy's radar missile capability", and the AFIT study found the rule "reduces the F-15 to the
  equal of the Soviet first generation aircraft" against a capable threat that outnumbered it about three to one
  (`afit_1982_vid`). Late-1970s F-15 pilots saw the target as a dot in the HUD box and could not identify it before
  Sparrow minimum range (`anderegg_2001` p. 161). SAM3D's NATO default is VID; a BVR shot is cleared only where the
  identification chain allows it: no friendly aircraft forward, or a hostile declared by the E-3A or the CRC
  (`air_combat.md` summary 2, a design rule). A cooperative identification of the enemy's own IFF (Combat Tree in 1972)
  is a flag, off by default: its 1985 status in Europe is not known (`air_combat.md` nato-06, unverified).
- **The pair.** Fluid Four was gone by the mid-1970s. Pairs fly line abreast about a mile apart with a stack of a couple
  of thousand feet; one fighter is engaged, the other free, and they "continuously switched roles so they did not have
  to slow down into the more agile bandit's area of advantage" (`anderegg_2001` pp. 59, 82-84). Targets are sorted "left,
  low, lead", and "discipline, discipline, and discipline" kept two radars off one target (p. 160). The shot order in
  1968 F-4E units was Sparrow, then Sidewinder, then the gun (`checo_1969_a2a`).
- **CAP, sweep and escort.** Over the Bekaa the F-15 and F-16 CAPs stood away from the strike groups; an E-2C vectored
  them onto MiGs from take-off, and MiGs crossing a line set beforehand (the range of the Syrian missile batteries)
  were attacked (`lambeth_1984`). The attacks were beam attacks set up by the E-2C, not BVR shots (same). Dubrov's
  Soviet reading: the detached CAP gave freedom of action but needed radar control beyond the strike zone and was less
  reliable than escort (`dubrov_1983`). The Falklands' outer CAP was rarely more than four Sea Harriers, each with two
  AIM-9L and two Aden on a 90-minute mission with about 20 minutes on station, and lacked radar-controlled cueing
  (`usn_falklands_1983`). An F-15 squadron had 22 F-15A and 2 F-15B (`anderegg_2001` ch. 15).
- **In the many-aircraft fight**: keep speed up, avoid turning fights, stay unpredictable (`lambeth_1984`). Crews were
  trained against Aggressors flying GCI-led Pact tactics (`anderegg_2001` pp. 78-79).
- **Calls.** Plain language with callsigns and clock codes ("MIGs at 12 o'clock", `red_baron_vol2`), plus the words
  TRADOC TT 17-50-3 (1983) defines for US close support: Bandit, Bingo, Blind, No Joy, Tally-ho, Visual, Winchester
  (`tradoc_jaat_1983`, glossary pp. 49-50; `c2_retasking.md` §7).
  Whether "Fox" calls were in use is open.

### 4.2 Warsaw Pact

- **Ground control.** The Pact's fighters were brought to the target by a controller on the ground, on the MiG-23 by
  the Lazur link to the autopilot; Western writers held that "the mission is aborted if contact is lost" (Skinner 1983
  in `lambeth_1984`, the adversary's view). Initiative "is transferred mainly to the flight leaders" only at contact,
  in the 1984 reform's words (`pennington_1985`), and Johnson's count of Soviet journals shows the talk of initiative
  falling back in 1983-84 (`johnson_1986`). SAM3D's rule (`air_combat.md` §4.3): the average Pact unit of 1985 is
  GCI-bound, aborting or falling back to rote pairs when control is lost; the best units pass initiative to the flight
  leader at contact.
- **The radar under control.** The Sapfir-23ML's 50-55 km by hand becomes 80-85 km when the ground puts the pilot onto
  the target; where guidance was jammed or absent the F-15 and F-16 saw the MiG-23 first at 60-70 km (`airwar_ru`
  MiG-23ML, the Soviet account). 1984 pilots kept their radars off until close for surprise (`pennington_1985`).
- **Geometry.** The rear-hemisphere attack: the fighter is brought to a departure position no closer than 6-8 km and
  launches at 4-6 km; the enemy's first echelon comes in groups of 4-8; independent-search fighters break off 25-30 km
  into enemy territory; airborne-alert zones sit at 1,500-2,000 m and 10-12 km, 50-60 km inside our own territory
  (`mt_turchin_coop`, 1967-68; that it still framed the 1980s is an inference). The VPAF MiG-21's hit-and-run: low under GCI
  until abeam the F-4s outside their 60° radar arc, a zoom to 25,000-28,000 ft, a dive at over Mach 1, the Atoll, and
  away (`checo_1969_a2a`); in May 1972 MiGs attacked "only when the pilot and GCI controller perceived a clear-cut
  advantage" (`checo_1978_linebacker` p. 52). The MiG-23 turns worse than a hard-wing F-4 and out-accelerates the F-5
  (`lambeth_1985`): a slashing, one-pass fighter.
- **The pair.** Sword and shield inside a group of four to eight; by 1982 Soviet writers wanted "every pilot both a
  shield and a sword", the strike made by "the pilot in the more advantageous position" (`johnson_1986`).
- **When the ground control is jammed.** Over the Bekaa the Syrian GCI was jammed out; the MiGs split into pairs and
  singles and flew figure-eights (a Western attaché, in `lambeth_1984`); some "simply ejected the moment they knew that
  the Israelis had spotted them" (a footnote in `lambeth_1984`); others popped up to reorient, zoomed into the CAP, or
  "simply leveled off and headed straight back to Syria" (`nwc_83_027`). Those were export MiG-23s without the R-23
  or the High Lark radar, and MiG-21s with at best the R-60 (`lambeth_1984`, `cordesman_1982`): the lower bound.

### 4.3 Our side

Our interceptors follow Soviet control practice: the national air defence's GCI brings them in, voice and Lazur where
fitted, and their own initiative begins at contact (`air_combat.md` our-01). Lose the command post, or have the voice
nets jammed, and they fall back to independent search in pairs or go home (`reactions.md` §2.6: 30 % of green crews go
home under jamming, a design estimate). How they are scrambled, controlled and told to the battery is the subject of
`08_OUR_FIGHTERS.md`; this doc gives them their fight. One danger is ours alone: in a Baltic Military District
exercise up to 25 % of the targets "fired upon" by SAMs and AAA were our own aircraft (`mt_turchin_coop`), and umpires
counted a fighter attacking a target at the moment missiles were launched at it as destroyed by its own SAMs
(`mt_lavrinovich_mdex`, in `our_side_air.md`). An air fight over the battery is that case.

### 4.4 The neutral and the rebels

The neutral fights within visual range, by day, always with visual identification, in pairs or singles under ground
control, with guns; it avoids modern fighters and breaks off on any threat (`air_combat.md` neu-01, low confidence; no
period source on its tactics). The rebels have no fighters: against one, a rebel aircraft hides low, runs or lands
(`air_combat.md` reb-01).

### 4.5 Engage and disengage

- **Who commits.** NATO: the flight leader, or the E-3A or CRC vectoring air defence fighters. The Pact and our side:
  the controller; the leader only at contact. The neutral: the controller.
- **Numbers.** ACEVAL found exchange ratios "tend to diminish rapidly as the number of independently maneuvering
  participants goes up — regardless of qualitative differences"; six F-5s against four to six F-15s came out
  "distinctly less than 1:1" in the knife fight and "slightly better than 1:1" without the VID rule (`lambeth_1985`).
  The F-5s, warned of a lock, turned away ("drag"), and a close pair showing as one return let one drag while the other
  pressed on unseen (`anderegg_2001` p. 160). A leader weighs the count he believes on each side before committing.
- **Fuel.** No period number for joker or bingo was found (`air_combat.md` nato-16). The Falklands' 90 minutes with about
  20 on station is the one fuel-bound window; pods cost about 500 lb more on bingo (`checo_1967_tactics`, in
  `reactions.md`). Joker and bingo are game values per type and profile, read against the fuel state of
`02_FLIGHTS_AND_PILOTS.md` §3.2.
- **Weapons state.** Out of missiles a NATO fighter calls Winchester and leaves unless a gun fight is forced on it.
- **The mission's priority.** An escort stays with its strikers unless released; a sweep or CAP is free to fight; a
  Pact fighter that loses its controller aborts or goes to rote pairs by skill (§4.2). The Argentine attackers at the
  extreme of their range "could not afford to maneuver" (`usn_falklands_1983`).

### 4.6 Strikers against interceptors

The eye found most fighters: in 16 MiG attacks on F-105 flights in 1965-67, 13 were first seen with no warning; first
sightings ran from 600 yd to 4-5 mi, a bogey once at about 15 nm (`red_baron_vol2`). The answers were to jettison and
fight, to press on low, or to abort. In those flights about half jettisoned something (tanks more often than bombs),
about a third pressed on, a fifth to a third aborted (`red_baron_vol2`); "MIGs would usually depart anytime you
jettisoned" (same). Of 192 strike aircraft engaged by MiGs in September-December 1966, 107 jettisoned (55.7 %), 2.7 %
of all strike sorties (`aces_1976` p. 10). One MiG attempt on 21 January 1967 made 21 aircraft jettison
(`checo_1969_tactics` p. 4). SAM3D takes `04_REACTIONS.md`'s per-flight table (§8; NATO trained: jettison 50 %, press on
35 %, abort 20 %; neutral: 60 %, 25 %, 30 %; design estimates). Self-escort is the F-16A and F-4E with AIM-9s on a
strike; Israeli strike groups flew close and split only before the target, with CAPs held apart (`dubrov_1983`). Pact
attackers carry the R-60 for self-defence on the Su-24 and Su-25 (`airwar_ru` R-60) and keep to the plan. A jettison
or abort counts in the enemy's freedom-of-action ledger (14.6) whoever caused it.

### 4.7 Helicopters against fighters

Nothing was found (`air_combat.md` gen-04); J-CATCH (1978-79) is the period test still to read. Until then a
helicopter seen by a fighter is a target like any slow aircraft, and its defence is to stay low behind cover where the
fighter's look-down and eye fail (`03_PERCEPTION.md`'s sight rules). Game values only.

### 4.8 What decided the period's exchange ratios

| Case | Ratio | Source |
|---|---|---|
| USAF 1965-68; 1968 | 81:36 (2.25:1); about 1:1 | `mcallister_1985` p. 17 |
| 1970-73 | USAF 1.88:1, USN 12.5:1 (72 MiGs for 28) | `vangilder_1979` after de Leon |
| Linebacker, May 1972 to Aug-Oct 1972 | from 1:0.47 to 19 MiGs for 5 losses (3.8:1) once Teaball and Combat Tree gave warning and identification | `checo_1978_linebacker` pp. 52-53 |
| Bekaa, June-July 1982 | about 85 Syrian, none Israeli in air combat; 23 on the first engagement | `lambeth_1984` |
| Falklands 1982 | at least 20 Argentine (16 AIM-9L, 4 guns), no Sea Harrier | `usn_falklands_1983` |
| Kills by IR against Sparrow | about 200 against 7 (1973); about 50 against 2 (1982) | `cbo_1986_amraam` n. 4 |

The swings come from warning, identification, ground control and training, not the aircraft: the same F-4s went from
losing to winning in 1972. SAM3D's balance is held to those causes (§5.10), not to the type.

## 5. The design

### 5.1 Data: `data/air_war/air_combat.json`

One file, loaded into a new `AirCombatDef` on `AirWarData` by `LoadAirCombat` (strict keys, as `LoadAircraft`), every
value with `source` and `confidence` or labelled a game value:

| Section | Keys |
|---|---|
| `aams` | `id`, `name`, `guidance` (`ir_rear`, `ir_all_aspect`, `sarh`, `sarh_inertial`), `in_service_year`, `allies_year`, `max_head_m`, `max_rear_m`, `low_factor`, `min_m`, `launch_g`, `seeker_cone_deg`, `target_g`, `sun_exclusion_deg`, `clutter_below`, `autonomous_s`, `flare_rejection`, `reliability`, `design_pk`, `lethal_m`, `mean_mps`, `munition` (the shape in `data/scenery/munitions.json`) |
| `guns` | `id`, `firing_s`, `effective_m`, `kill_per_s` (game value), `in_service_year` |
| `fits` | per aircraft id, ours included (`mig21mf`, `mig21bis`, `mig23ml`): `iff` (`mk12`, `sro2m`, `parol`, `kremniy2m`, `none`), `command_link`, `vid_aid_m`, `loads_by_year` (missiles and gun by year), `export_fit` (the stripped client variant). Not here: the radar and IR set are the type's entries in `data/air_war/sensors.json` (`03_PERCEPTION.md` §4; a radar entry names its EM `RadarDef`), the warning receiver its `rwr_by_year` (`03_PERCEPTION.md` §3.2), flares and chaff its `countermeasures` (`04_REACTIONS.md` §7, §12.1) |
| `doctrine` | per faction and ours: `roe` (`vid`, `cleared_bvr`, `controller`), `commit_by` (`leader`, `controller`), `initiative_at_contact`, `formation`, `sort`, `shot_order`, `tactic` (`turn`, `slash`, `beam`), `cooperative_id`, the strikers' answer table |
| `calibration` | the targets of §3.1 and §4.8, read by the bench (§5.10) |

What a flight does when it loses its controller (`abort`, `pairs` or `continue`, by skill) is the faction's
`hot_phase.on_control_lost` with `hot_phase.turn_back`, and the mean skill is `hot_phase.air_combat_skill`
(`10_FACTIONS.md` §7.2, §7.5); this file does not repeat them. `AamDef::InService(year, allied)` checks the right year. A type's `loads_by_year` is the AAM counterpart of
`arm_em_by_year`. The pilot's air combat skill comes from the people model (decision 9): annual hours, air combat
sorties, encounters survived, nerve; officers' traits move the doctrine values by up to ±0.2 (boldness the commit and
press-on, rigidity the dependence on the controller, composure the ejection and turn-back when spotted, leadership the
flight's cohesion when its net is jammed, airmanship the skill; the weights are `10_FACTIONS.md` §8.2's), pilots' state
by ±0.15 (`factions_cases.md` §1).

### 5.2 Airborne radar, infrared and the warning receiver in the EM layer

- **Radars as `em::RadarDef`.** Each fighter radar is a `RadarDef` placed on the aircraft's platform by
  `EmWorld::AddRadar(platform_id, def_id)` from `EmSensorLink::SyncTarget`, scanning a `Sector` about the heading as
  `scene_ops.hpp` already does, calibrated to the published figures: `sapfir_23ml` (50-55 km by hand), `sapfir_23d`
  (55 km detection, 35 km lock), `apg63_class` (about 50 km against clutter, over 80 km looking up, the Soviet view),
  `apq120_class` (no look-down). RP-21/RP-22 and APG-66 stay placeholders, labelled, until a source is found. Two new
  fields: `look_down` (pulse-Doppler clutter rejection in the main lobe, a game value per radar until sourced) and an
  airborne anchor form, since `AnchorLook` assumes an antenna over flat ground (`radar_system.hpp:77`).
- **Under ground control** the controller's cue narrows the search to a sector and a height band: the radar's anchor
  becomes its cued figure (Sapfir-23ML 80-85 km), with no change to the radar equation's code.
- **The lock is heard.** A single-target track is a steady main-beam illumination, which is exactly what sets the
  target's RWR `lock` flag in `EmSensorLink::WarningOf` (`em_link.hpp:771`): no new RWR code. The libraries need
  entries: the NATO RWR's `blue_rwr_nato_view` gains the MiG-23's radar as a `fire_control` entry; the Pact's
  `generic_rwr` placeholder is replaced by `03_PERCEPTION.md` §3.3's `pact_spo10_class_rwr` and `pact_spo15l_class_rwr`
  (the latter low confidence), whose library `blue_rwr_pact_view` gains the NATO fighter radars as `fire_control`. No launch light for an AAM: none is sourced. An IR shot gives no warning (`babich_1988` in
  `reactions.md`).
- **Jamming and chaff** act on the shooter's radar as on a ground radar (J/S, `AddChaff`), so a self-protection jammer
  can break a semi-active round's lock. The AIM-7M's monopulse seeker resists better (`parsch_dusrm`): an ECCM value
  on its `RadarDef` link.
- **Infrared search.** The TP-23M is not an EM sensor: a range from the rear against a clear sky (35 km), shortened by
  cloud between (`Weather::CloudTransmission`) and against the ground (a game factor). The IR missile's seeker uses the
  same aspect rule.
- **Identification.** VID uses `03_PERCEPTION.md` §2.1's recognition of an aircraft's type (`eye.recognise_lines`, set
  so a fighter is recognised at about 5-6 km by day in clear air, `aam_weapons.md` §8; further inside TISEO's or Eagle
  Eye's field); IFF replies ride on the radar as for ground radars (`RadarDef::has_iff`), with each side's sets and the open
  question of SRO-2M against Parol (§2.4).

### 5.3 The fight in the session

The fight is pilots' demands on `02_FLIGHTS_AND_PILOTS.md`'s point masses, nothing more. Each aircraft flies by its
type's energy and turn limits through that doc's command layer (`AircraftDemand`: heading or turn, height, speed); the air
combat layer chooses the demand. Geometry is real (the aircraft's positions and velocities), so angles gained in a
turning fight follow from the turn rates and energy `02_FLIGHTS_AND_PILOTS.md` §3.2 gives each type, and type numbers stay game values until the Soviet practical aerodynamics
manuals or Shaw (1985) are read. No 6-DoF, no seeker physics.

Each fighter flight in contact is in one mode, chosen by its doctrine at 1-5 Hz, staggered by key:

| Mode | Demands | Who |
|---|---|---|
| Vectored | the controller's heading, height and speed; radar on or off by doctrine | Pact, ours, NATO under E-3A or CRC |
| Search | own radar sector; the pair stacked | NATO pairs; Pact after control is lost |
| Ident pass | the leader closes to identify, the trailer holds a firing position | NATO under VID |
| Beam or slash | one pass from the side or below, a shot, extend away | NATO beam attacks (Bekaa); Pact rear-hemisphere and hit-and-run |
| Turn (one circle or two) | nose-to-nose or nose-to-tail at the merge, pursuit lead, pure or lag | NATO engaged and free fighters; the neutral |
| Extend, drag | unload and run; turn away from a lock | everyone; ACEVAL's drag |
| Disengage | home at best speed, low | on fuel, weapons, numbers, lost control |

A shot check runs at 2-5 Hz for each armed aircraft against its sorted target: the pilot's believed envelope (§3.2),
the doctrine's shot order, the ROE gate (VID done, cleared, or the controller's word), and for a semi-active round the
radar's lock. Defensive answers (break, jettison, flares) use `04_REACTIONS.md`'s rules for a fighter threat (break
into the attacker if seen; tanks, then bombs; the deck), with the same reaction delays.

### 5.4 Missiles in flight

An AAM is a session object, like an ARM: shooter, target, launch time and a flight time from the range and the round's
mean speed. It flies the line of sight as a point, for the eye and for Unreal (shapes for the AIM-7F/M, AIM-9L/M/P,
R-60, R-24R/T and R-13M are added to `munitions.json` with their smoke; the AIM-9M's reduced smoke from 1983). The
links in envelope and works are drawn at launch; guides and lethal at arrival, after what happened in flight: the
target's break and when it began, flares, the shooter's lock held or lost. A target sees the trail only by
`03_PERCEPTION.md`'s rules, and its RWR warns only of a lock. A new `fx::Feed` event shows the launch and the burst; nothing
reads it.

### 5.5 Escort, CAP and sweep

- **Roles.** `FlightRole` gains `Sweep` and `Cap`; `Escort` becomes close escort. Every switch on it follows
  (`raid_scenario.hpp:408-420`, `test_waves.hpp:428`, `outcomes.hpp:126`).
- **The planner.** Escort sizing (`planner.hpp:733-753`) reads the believed threat of our fighters from the enemy's
  intelligence picture (`08_OUR_FIGHTERS.md` §11's derivation replaces `FrontDef::friendly_fighters`) and the doctrine:
  NATO flies detached CAPs on a line outside the believed SAM zones and sweeps ahead of the strike by a game-value lead
  time; the Pact flies the clearing group ahead and close escort; the neutral a few gun-armed escorts; the rebels none.
  A sweep and a CAP get their own routes (`OrbitRoute` for a CAP station), not the strike's `MainRoute`.
- **The session.** `RaidScenarioBuilder::Build` no longer cuts fighters first when our fighters are up within the
  played circle; `scenario.max_targets` rises from 24 toward 40 for such raids (a tuning value, `map/scale.md` §2).
  Escorts hold a station on their strikers through `02_FLIGHTS_AND_PILOTS.md`'s formation layer (§1.3, §2.2); a CAP orbits; a sweep flies ahead.

### 5.6 The abstract fight (A0) and the far fight (A1)

The resolver's flat roll (`resolve.hpp:231-247`) is replaced by `AbstractAirFight` in `air_war/air_combat.hpp`, under
the same rules: where the time-stepped package's route crosses a zone our fighters hold at that hour (`08_OUR_FIGHTERS.md`
§3's duty), a detection by our ground control (night and weather cut it), a commit by our doctrine, then a few rounds
in which each armed fighter on each side gets shot chances drawn from its doctrine's geometry (the Pact's and ours from
the rear hemisphere, NATO's all-aspect where its missiles allow; game-value distributions), each resolved with the same
`ShotOdds` the session uses. Escorts and sweeps meet our fighters first by share; strikers jettison or abort by the
§4.6 table; our losses come back in a new `PackageOutcome::our_fighters_lost`. A loss to fighters is written
`LossRecord::by` = the regiment's id from `oob_pannonia.json` (`fighter_rgt_1`), so `outcomes.hpp:127` can count
`downed_by_fighters` apart from the other defences. The planner's `ExpectedLosses` uses the expectation of the same
function, as `PassKillChance` serves both today. At A1 (flown but far), two flights in contact fight flight against
flight at 1 Hz with the same chain on the leaders' geometry. A fight never changes level while a missile is in the air.

### 5.7 Records, claims and the first person

A pilot claims what he believed: a burst near a target he then lost sight of is a kill. Claims go into the enemy's
records as claimed and into ours through the pipeline as our regiment reports them; the truth stays for the debrief.
The period shows the gap: Syrian MiG-23ML pilots claimed three F-15s and an F-4 with the R-24 in 1982, unconfirmed
(`mis`, `ru_wiki_aam`), and at TOPGUN in 1980 about two shots were called for every shot observed (`lambeth_1985`).
The battery sees an air fight only as its crews see it (trails, a fireball, a parachute), hears it on the regiment's
nets, or is told it by notice; no panel shows it. The enemy's GCI and flight calls are EM emissions our intercept
posts may hear (`09_COMMS_AND_INTERCEPT.md`).

### 5.8 Determinism and cost

Every draw is keyed: `DrawHash(seed, Stream::AirCombat, shooter, shot, link)` (a new `em::Stream` value), independent of evaluation order; nothing
reads the fx feed. A fight's results that change the campaign return in `PackageOutcome`. Pair work runs only between
flights in contact (a radar track, a sighting, a controller's vector); detection is the EM layer's, with its grid and
culling. Shot checks and mode choices run at 2-5 Hz staggered by a fixed key, within the track's budget of about 0.5 ms
per 0.05 s step for about 100 aircraft.

### 5.9 Interfaces with the other docs

`02_FLIGHTS_AND_PILOTS.md` gives the flight model, its type limits and the command layer; `03_PERCEPTION.md` gives
sight of aircraft and trails and the sensors' entries; `04_REACTIONS.md` the defensive answers, delays and
countermeasures; `09_COMMS_AND_INTERCEPT.md` the GCI, E-3A and flight nets as emissions; `08_OUR_FIGHTERS.md` the
scrambles, our GCI, the shared sky with the SAMs and the notices to the battery; `10_FACTIONS.md` the faction values. This doc adds
the fight between them.

### 5.10 Balance against the period

A new `simcore/examples/air_combat_bench.cpp`, like `air_war_bench`'s analogues, plays the period's cases with analogue
fits: Vietnam 1967 (F-4 with AIM-7E and AIM-9B/E against MiG-21s with R-3S under GCI), Linebacker May and
August-October 1972 (the same with a Teaball-like warning and the cooperative-ID flag), the Bekaa (F-15 and F-16 with
AIM-9L under an E-2C against stripped MiG-21s and MiG-23s with GCI jammed), the Falklands (Sea Harriers with AIM-9L and
no cueing against attackers at the end of their range) and ACEVAL (four F-15s against six F-5s, with and without VID).
It prints per-launch rates, out-of-envelope shares, IR and gun shares and exchange ratios against the `calibration`
section. `air_war_balance` and `air_war_bench` are rerun and their moved rows explained in `docs/air_war/09_BALANCE.md`.

### 5.11 Tests

`test_air_combat.cpp`: the chain's product equals a hand calculation; a rear-aspect IR round refuses a head-on shot; a
K-13 within 30° of the sun does not guide; a semi-active round fails when the shooter breaks lock and an R-24R survives
a lock lost inside its 10 s; a 1967-skill shooter's out-of-envelope share is within tolerance of 35 % and 26 %; the
year gate (no R-73 on a Pact allied or our aircraft, no R-27 or AMRAAM in 1985, no AIM-7 on the F-16A); a NATO pair
under VID holds its AIM-7 at an unidentified track with friendlies ahead; a Pact flight that loses its controller
aborts or pairs by skill; the abstract and the session fight agree in expectation over seeds; a replay by seed is
exact. EM: a fighter's track radar sets its target's RWR `lock`; the Sapfir-23ML's range by hand and under control
match the anchors.

## 6. Done when

- `data/air_war/air_combat.json` loads with every value sourced or labelled, and `python tools/check_citations.py`
  passes; a 1985 war fields nothing from §2.6.
- The §5.11 tests pass, and `air_combat_bench` reproduces, within tolerances set in the `calibration` section, the 1967
  per-launch rates (11.1 % and 18.6 %), IR rounds taking most kills where both kinds are carried, a gun share between
  the Bekaa's 7 % and Vietnam's 29 %, a swing of the 1972 kind when warning and identification are added, and an
  ACEVAL fight near 1:1.
- In the testbed, our MiG-23MLs and MiG-21s under GCI meet the enemy's escorts and sweeps; losses fall on both sides;
  strikers jettison and abort; the battery learns of it only by sight, nets and reports.
- The resolver's flat fighter roll is gone; fighter losses carry the regiment's id; `air_war_balance` rows are within
  the bands `09_BALANCE.md` gives after the rerun.
- `AircraftType::air_to_air` is no longer loaded.

## 7. Left open

- NATO radar ranges (APG-63, APG-66, APQ-120) and launch zones for the AIM-9L/P and AIM-7F from a usable source; the
  Sky Flash, Magic 1 and Super 530F; gun loads and rates.
- The MiG-21's radars; the R-60M's and R-13M's adoption dates; the R-24 with the Pact allies; the SPO-15L, SPS-141 and
  Parol on the MLD; whether SRO-2M answers Parol.
- The MiG-29's and so the R-73's place in Soviet frontal units in 1985.
- Turn rates and specific excess power per type (the Soviet practical aerodynamics manuals; Shaw 1985).
- Joker and bingo as numbers; the 1985 brevity beyond TRADOC's list.
- The AIM-9L's launches in the Falklands (only the 16-19 kills are established); the Ault report and Red Baron III.
- Helicopters against fighters (J-CATCH); RAF and Luftwaffe air combat practice of 1980-85.
- How far the Pact's 1984 reform reached the average unit; the Lazur's ranges and commands.
- The Pact's own identification rule before a radar shot (the controller's word is SAM3D's assumption).
