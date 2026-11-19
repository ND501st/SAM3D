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

**As built (B7, 15 Nov 2026).** The file has the five sections above, with these differences from the table:
- `doctrine` is a list with one entry per faction (`nato`, `warsaw_pact`, `neutral`, `rebels`, `ours`); `strikers_answer` is
  `{jettison, press_on, abort}`.
- Each missile and gun names its game values in `game_values` (keys of the entry that are SAM3D's, not the source's); the loader
  refuses a name that is not a key. The `munition` key names the missile's shape in `data/scenery/munitions.json` (B13, 19 Nov
  2026: all 18, each with its dimensions, fins, colours and motor from `parsch_dusrm`, `airwar_ru` and `mis`).
- `loads_by_year` is a list of eras, `{"from": year, "aams": [[id, count], ...], "gun": id}`: the latest era at or before the
  war's year is the fit, each missile checked against the year for the shooter's side (`AirCombatDef::LoadOutFor(type, year,
  allied)`; `allied` is true for the Pact's allies and our side). `aliases` let the order of battle's `mig21` find
  `mig21mf`'s fit; `export_fit` names another fit (`mig23ml_export`, the Bekaa's lower bound).
- `calibration` has `chain` (the numbers of section 3: judgement spread, switch error, guidance bases, the sun, the beam
  against the ground, the break, the miss), `mix` (the 1967 engagement: where shots were taken from and what the target did)
  and `targets` (each rate with its tolerance, source and confidence). All of `chain` and `mix` are game values.
- Types are in `air_war/air_combat_types.hpp` (included first by `data.hpp`), loaders in `air_combat_data.hpp`, the shot in
  `air_war/air_combat.hpp`; `AirWarData` holds `air_combat` and `sensors`.
- Left out of the data: the AMRAAM (no guidance class: its slot is in `sensors.json`), the AIM-9P, Sky Flash, Magic 1 and
  Super 530F (no date or envelope found). The R-27 and R-73 are present with their real years and carried by no 1985 fit.

**The chain as built** (`air_combat.hpp`). `ShotOdds` gives the four links for a truth geometry; `ResolveShot` draws each from
its own keyed stream (`em::Stream::AirCombat`, link number) so the answer is independent of order. In envelope: `CheckEnvelope`
on the truth (reasons: too close, too far, aspect, off boresight, launch g); the pilot's belief is `Believe` (a lognormal
range error, normal errors in aspect, angle and load, all scaled by `JudgementScale`: (1 - 0.8 skill) x (1 + 0.5 stress),
larger for a radar round) and he fires on `PilotFires`; `EnvelopeProbability` is the expectation for the planner. Works:
reliability x (1 - switch error), the error 0.12 x (1 - 0.7 skill) x (1 + stress), a fifth of it with no change of mode.
Guides: the class's base (infrared rear 0.85, all-aspect 0.8, semi-active 0.6) x aspect x sun (zero inside the exclusion,
a soft edge to 1.667 times it) x ground (`clutter_below`, and a half more in the beam for a radar round) x countermeasure
(1 - strength x (1 - rejection)) x break (1 - 0.6 x timing x load/limit) x lock (a radar round needs the lock to impact,
or within `autonomous_s` of it). Lethal: `design_pk` x the ratio of the chance to put the miss inside the lethal radius with
the break's added miss (rms 6 m x effectiveness, on a nominal 1.5 m) to that with none. A miss carries its reason; guidance
failures are attributed in proportion to the losses each cause made. A gun pass is `1 - exp(-kill_per_s x seconds x aim)`,
the seconds capped by the magazine, the aim by skill, range and the target's load. `SimulateLaunches` plays launches at the
mix and is what holds the chain to the targets: 1967 crews (skill 0.3, stress 0.4) hit 11.0 % with the AIM-7E and 18.7 % with the AIM-9B per launch and launch 35.0 % and 26.8 % outside the parameters, against 11.1 %, 18.6 %, 35 % and 26 % (`test_air_combat.cpp`).

### 5.2 Airborne radar, infrared and the warning receiver in the EM layer

- **Radars as `em::RadarDef`.** Each fighter radar is a `RadarDef` placed on the aircraft's platform by
  `EmWorld::AddRadar(platform_id, def_id)` from `EmSensorLink::SyncTarget`, scanning a `Sector` about the heading as
  `scene_ops.hpp` already does, calibrated to the published figures: `sapfir_23ml` (50-55 km by hand), `sapfir_23d`
  (55 km detection, 35 km lock), `apg63_class` (about 50 km against clutter, over 80 km looking up, the Soviet view),
  `apq120_class` (no look-down). RP-21/RP-22 and APG-66 stay placeholders, labelled, until a source is found. Two new
  fields: `look_down` (pulse-Doppler clutter rejection in the main lobe, a game value per radar until sourced) and an
  airborne anchor form, since `AnchorLook` assumes an antenna over flat ground (`radar_system.hpp:77`).
- **As built (B7).** `data/em/fighter_radars_1985.json` (per value) holds seven `RadarDef`s with `role: search`, a sector scan
  and `airborne: true`: `sapfir_23ml` (anchor 52.5 km, 85 km on paper), `sapfir_23d` (55 km), `apg63_class` (80 km looking up,
  the Soviet view; lock to 92.6 km), `apq120_class` (an estimated anchor, no look-down) and the labelled placeholders
  `apg66_placeholder`, `rp21_placeholder` and `rp22_placeholder`. The two new fields are `RadarDef::airborne` (the antenna
  height is the carrier's, and `AnchorLook` leaves out the ground reflection and diffraction: the published figures are
  free-space) and `look_down_db` (called `look_down` above: the clutter cut in the main lobe, a game value until sourced,
  read by the air combat layer in B8, not by the radar equation). `sensors.json` names each radar's `RadarDef` and gives the
  by-hand, cued, lock and clutter ranges; `test_air_combat.cpp` checks the radar equation gives each anchor back. Placing the
  radars on aircraft platforms in a session (`EmSensorLink::SyncTarget`), the cue narrowing the sector and the RWR libraries'
  new entries are B8's and B3's.
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

## 8. As built (B8, 18 November 2026, branch `tb-b8`)

**The abstract fight (A0), `simcore/air_war/air_fight.hpp`.** `AbstractAirFight` replaces the resolver's flat fighter
roll in both the time-stepped `Begin`/`StepTo` and the atomic `Resolve` (they share the leg code). It runs once a
package, when its first strike flight reaches the leg our fighters meet it on (B2's IP leg):

- **Sent and found.** The front's fighter threat times `fight.a0_intercept` gives the chance a group is sent. Our
  ground radar finds the package by day (0.85), by night (0.45), in poor weather (×0.6) and low (×0.6).
- **Our group.** A pair or a four from one of our three regiments (`air_combat.json`'s `ours`, from
  `oob_pannonia.json`: two of MiG-21MFs, one of MiG-21bis and MiG-23MLs), drawn by strength.
- **Our GCI jammed.** A package with a jammer jams our nets half the time. Our green pilots then go home (30 %) and the
  surprise is lost (0.6 → 0.2).
- **Their cover first.** Sweep, then CAP, then close escort; each meets our group by its chance (0.6, 0.4, 0.5), ×1.5
  under an E-3A that controls fighters. A leader who commits himself (NATO) needs the odds of his doctrine. A met group
  fights them; it reaches the strikers only if the cover is down or broke off.
- **The strikers.** Those shot at or that saw the attack answer by their doctrine: jettison, then press on or abort.
  Armed strikers defend themselves when they saw ours first. An aborted flight goes home in the resolver; a jettisoned
  one delivers nothing.
- **Rounds** (`a0_rounds` 2). The side with the first sight fires first. Each shooter tries his weapons in his
  doctrine's order at the round's range (lognormal about 15 km, 5 km, 3 km and 600 m for a radar missile beyond sight,
  a radar missile at sight, an infrared missile and a gun). A radar missile beyond sight goes only where his rules
  clear it: the controller's word for ours and the Pact, the E-3A for NATO. Each shot is B7's chain: `Believe`,
  `PilotFires`, then `ResolveShot` or `ResolveGunPass`.
- **The record.** A loss is written `LossRecord::by` = the regiment's id with the new `fighter` flag; ours lost go to
  `PackageOutcome::our_fighters_lost`; `air_fights` counts the fights. Both are saved. The debrief's ledger
  (`campaign/outcomes.hpp`) counts `downed_by_fighters` apart, with ours lost beside it.
- **The planner.** `ExpectedLosses` uses `AbstractAirFight::ExpectedMainLoss`, the same model in closed form: the
  chance of finding, times the chance no cover meets our group, times what our mean group kills with its nominal shot.

**The live fight, `simcore/air_war/air_fight_session.hpp`.** A scenario's `"air_fight"` (`air_fight_def.hpp`) names
the targets that fight, their type, faction, role, regiment, controller and skill, and the controllers (a GCI post or
an AWACS, ours or the enemy's, with a range and an `air_radio` post). The runner steps it after the reactions
(`UpdateAirFight`, one small block in `scenario_runner.hpp`) and hands it callbacks: the aircraft, each pilot's B3
picture, height and height above the ground, steering through B1's `PilotOverride`, `DestroyTarget`, events and B9's
radio.

- **What a fighter knows.** His pilot's picture (a type recognised inside `vid_m`, 5.5 km, is friend or foe), his own
  radar (`sensors.json`: by hand, or cued after a vector; the clutter figure looking down below 3 km; a 60° sector;
  range scaled by the fourth root of the RCS) and his controller's vectors. Decisions use these. The truth is read
  only to resolve a shot and to match a believed place to an aircraft.
- **Rules of engagement.** A `vid` doctrine (NATO, the neutral) fires only on a type recognised by eye. Under an E-3A
  that controls it in the war's year, a radar contact with no friendly within 20 km is cleared. A `controller`
  doctrine (the Pact) fires on what its controller called hostile; ours the same.
- **GCI and its loss.** A controller vectors his fighters every 20 s to the nearest opposed aircraft in his range, with
  1 km of error, said on his post's net (`AirRadio::SayFrom`, B9) and heard or not (`Reaches`): jammed, out of range or
  the net busy. A bound fighter that hears nothing for 60 s has lost control. By his doctrine and skill he aborts (the
  Pact's and ours when green, the neutral's always), goes on in pairs with his own radar (the Pact's and ours when
  skilled), or continues (NATO).
- **Commit and disengage.** Bingo after 900 s of fight, joker after 600 s unless engaged, Winchester when empty, and a
  leader who commits himself goes home when his believed odds fall below his doctrine's.
- **Modes.** Patrol (his own route), Vectored, Search (free after control is lost), Identify (closes on an unknown),
  Engage (leads the hostile and fires), Home.
- **The shot.** One missile in the air at a time per target. A radar missile needs a lock (`lock_m`, else 0.8 of the
  range by hand) held to impact. The missile flies its time and is resolved at arrival: the target breaks if its
  pilot knew, and a lost lock fails it. A gun fires in its cone (10°), its range and an aspect within 60° of the tail.
- **Strikers** fire back only in self-defence, close in. Their break and jettison on seeing a fighter come from B4's
  reaction chain, which already reads a recognised fighter in the picture.
- **Kills and claims.** A kill goes to `DestroyTarget` with the regiment's id for ours and the type for the enemy's. A
  miss is claimed 30 % of the time (`claims`). The mission bridge writes our fighters' kills with the regiment and the
  `fighter` flag, and our fighters lost into the outcome.

**Data.** `air_combat.json` gains `fight` (the game values above, each anchored in its notes), `ours`, and per
doctrine `skill`, `on_control_lost` (green and skilled), `commit_odds` and `fighter_plan` (the shares of close escort,
sweep and CAP and the sweep's lead). NATO plans sweep and CAP at half each with a 5-minute lead; the Pact close escort
and its clearing group (a sweep 3 minutes ahead) at half each; the neutral close escort only; the rebels have no
fighters to send. `FlightRole` gains `Sweep` and `Cap`. The planner splits a
package's fighters into one flight, or two pairs in the two likeliest roles. A sweep flies the strike's route ahead by
the lead; a CAP holds an orbit off the target (`OrbitRoute`).

**Keyed draws.** No new random stream. The fight reuses `Stream::AirCombat` (15) with keys apart from B7's:
`0xB8000000+n` for the abstract fight's shooters, `0xF1600000+i` for the session's fighters, `0xF16C0000+i` for its
controllers, links 31-32 (radar noise) and 41 (claims). The resolver's fight seeds from a fork of the package's stream,
so it takes no draw from it. The prototype has no air combat, so nothing there changes.

**B7's data list for B11, settled here.** B3 and B5 had done none of it.

- The Draken from 1988 (`draken_gun` in service 1988, its `air_to_air` gone); the neutral's 1985 escort is the
  gun-armed Saab 105 (`escort` added to its roles).
- The MiG-23ML as the Pact's 1985 standard: a new type `mig23ml_wp` (SPO-10, its own sensors), the MLD from 1984 with
  the SPO-15L. The regiments are 20 MLs and 10 MLDs.
- The F-16A's escort value with no radar missile: its fit has AIM-9s and the gun only. In the abstract fight it can
  fire beyond sight only with a radar missile, so it fights at sight.
- The A-50 controls no fighters in 1985 (`AwacsControlsFighters`: the faction's airborne warning post's year). The
  planner's AWACS cut and the abstract fight's E-3A factors apply only where it does.
- `AircraftType::air_to_air` is retired (the loader and the struct).
- Our order of battle's `mig21` is `mig21mf` or `mig21bis` by regiment, with basic models, sounds and the reaction
  table's fighter models.
- Fits for f111f, tornado_ids, a10a, ah1s, mig25bm, mi24 and the helicopters, and an `unarmed` fit for the support,
  jamming, reconnaissance and light types. Each is sourced or a labelled SAM3D assumption.
- `rwr_by_year`: not needed. A type's receiver is its `rwr_em`, and a later receiver is a later type (the MLD).
- Guns for the MiG-27 (GSh-6-30, `hushkit_2020_mig27`), Su-24 (GSh-6-23), Su-25 (GSh-30-2) and Su-17M3 (two NR-30s),
  with their R-60s, from `airwar_ru`.

**Tests**, `simcore/tests/test_air_fight.cpp`: the data and doctrines; the A-50 and E-3A year gate; the abstract fight
deterministic by seed with its kills named by regiment; how often a package is found; exchange ratios (NATO's F-15
sweep at least 2:1 against our fighters and twice the Pact's; the Pact's MiG-23MLs near even); a sweep and an escort
cutting the strikers' losses, an E-3A more; the strikers answering; the planner's expectation within a factor of three
of the fight; night, weather and height hiding a package; the resolver writing fighter losses with the regiment. Live:
a NATO pilot without an E-3A holds his fire until he recognises the type; under an E-3A he fires an AIM-7 beyond sight;
the same seed gives the same fight; a Pact pilot whose vectors are jammed goes home if green, on in pairs if skilled; in
a session our MiG-21 under GCI closes on an enemy jet and fires, and the scenario loader refuses bad fighters. `test_perception_em.cpp` now holds the MLD to its SPO-15L from its year, and `test_air_combat.cpp` gives every type a fit.

**Balance** (`docs/air_war/09_BALANCE.md` §15). Our fighters now shoot down 4 of the Pact's and 4 of NATO's aircraft a week on the district (8-10 and 5-6 a campaign on the nation) and lose 2.5-3 and 4.3-4.8. The Pact's and NATO's loss rates rise by 0.3-0.8 points; the district stays at or under the period's 2 % a sortie. Against the Pact our side comes out ahead (1.2-1.8:1, mostly strikers caught); against NATO's sweeps and E-3A it loses more than it kills (0.7-0.9:1), as the period suggests. The Pact, its A-50 no longer counted, escorts more and flies bigger strike groups. Old saves (version 1 and 2) still load and replay to the bit on a reload, but to a new war: the Pact's order of battle and the fight changed (`test_routes.cpp`'s fixture fingerprint is now B8's).

**Left.**

- For B10: our fighters as live actors in raids (the raid builder writes no `"air_fight"` yet, so a played raid has no
  live air fight; the testbed has none either), our GCI on our picture, alert and scrambles, corridors, IFF. Until
  then our side in the session is the minimal fighter of a hand-written scenario, heard at once with no radio.
- Fighter radars as EM emitters on aircraft (`EmSensorLink::SyncTarget`); the session uses `sensors.json`'s ranges and
  its clutter figure, not `look_down_db`. (S3, §9.)
- Authority lines for vectors (B5's `C2Authority`): the controllers' vectors are the session's own calls on B9's nets. (S3, §9.)
- The doctrine's tactics (beam, slash, one or two circles, drag) are not told apart; the live fight steers by lead
  pursuit and the abstract one by range bands. (S3, §9; the circles left.)
- The strikers' chaff and flares and the escort's station on its strikers (B1's formation layer) in the fight. (S3, §9.)
- `air_combat_bench` (§5.10) against Vietnam, Linebacker, the Bekaa, the Falklands and ACEVAL; the A1 far fight. (S3, §9:
  the bench and A1; the comparison with history left.)
- Claims into the enemy's records and ours through the pipeline. (AAM shapes, their smoke and the fight drawn in Unreal: B13,
  done 19 Nov 2026, `11_IMPLEMENTATION_PLAN.md`.)

## 9. As built (S3, 19 November 2026, branch `tbr-s3`)

Roadmap S3 takes B7's and B8's "Left:" on the fight itself: the radars, who may vector whom, the doctrines' attacks, the
strikers' answer to a missile and the escort's station, and the measures. Everything is of 1985.

**Two switches.** The session's fight has its own: a scenario's `"air_fight"` takes `"mode"` (1, the default: S3's fight;
0: B8's as it was) and `"far_fight"` (-1 with the mode, 0 off, 1 on). A played session's outcome is saved with the
campaign, so the session's changes never change an old save's replay. The abstract fight (A0) has the tuning's
`air_fight.mode` (`data/gameplay/air_war.json`, 1), saved as `"air_fight_mode"`; a save without it replays with 0 (B8's
A0). The raid builder writes the scenario's mode from the tuning. B8's tests run with mode 0; the replay fingerprint
(`test_routes.cpp`) is unchanged. The shot check's scenario (`air_battle_shots.json`) keeps B8's fight (`"mode": 0`): with S3's, our pair
slashes, survives the Eagle and drives the F-4Es off before they meet the Osa, and the chaff -SimAirShots photographs
never comes.

**Radars.**

- *As emitters.* A fighter's radar is an emitter on his EM platform (`EmSensorLink::SetFighterRadar`, roadmap S3): the
  RadarDef its `sensors.json` entry names, from `data/em/fighter_radars_1985.json`, merged into the session's library
  when the fight starts (`AirFightDef::radar_library`). Searching, it sweeps its sector about the nose; locked, it is a
  steady track pointed at its target each step (`PointFighterRadars`), so the target's warning receiver hears a lock by
  its own library (`pact_fighter_fcr_x`, `nato_fighter_fcr_x`: B3's rule, a steady main beam from a fire-control
  entry). The fight sets the state (`AirFightWorld::radar`): off for a striker, at home and in a drag, and for a fighter
  bound to his controller until the nearest aircraft he was told of (else his vector's point) is within his radar's cued
  range (radar silence until close, `pennington_1985`); locked on the hostile he engages within the lock range in his cone, or on the target of his radar
  round in flight; else searching. A radar that is off sees nothing: its look is skipped.
- *Looking down.* Each radar's `look_down_db` is read from its EM entry (`FillLookDown`). Looking down at a target
  below him and under 3 km above the ground, his range is his range up times 10^(-(C - look_down_db)/40), the fourth
  root of what the clutter the radar does not reject leaves of the signal; C, `fight.look_down_clutter_db`, is 48 dB, so
  the APG-63's 40 dB gives `sensors.json`'s 50 of 80 km. The Sapfir-23ML (25 dB) keeps about a quarter, the APQ-120 and
  the RP-21 (0 dB) about a fifteenth. A radar with its own published clutter range keeps it.

**Control.** A faction's c2 authority table has a new act, `vector` (`c2_def.hpp`), and the fight reads its lines
(`AirFightDef::vector_lines`, `MayVector`): a controller of a post kind vectors a fighter of a role only where a line
lets it in the war's year. NATO's CRC vectors its CAP and interceptors, not its sweep or close escort (`smart_1985_tacs`);
its E-3A any fighter from 1982 (`afmag_1984_nato`, `anderegg_2001`). The Pact's GCI vectors all of its own
(`mt_1962_front_aviation`, `lambeth_1984`); its A-50 none (from 1989). The neutral's GCI its own (a labelled
assumption). A faction with no line (the rebels) and our side keep B8's and B10's rule. A fighter his controller may not
vector is never bound to him: he flies on his own radar and his picture, and never loses control.

**Tactics.** Each doctrine's `tactic` (`air_combat.json`) now steers:

- *Beam* (NATO): beyond `beam_turn_in_m` (15 km), unless already near the target's beam, he flies to a point abeam its
  track on his own side, `beam_offset_m` (8 km) out and ahead by half the time to get there (`Setup`), then turns in by
  lead pursuit (`lambeth_1984`, the Bekaa).
- *Slash* (the Pact, ours): beyond `slash_turn_in_m` (6 km), unless already behind it, he makes for a point
  `slash_offset_m` (3 km) behind the target (`Setup`); after his shot, once no radar round of his needs his lock, or once
  he has passed inside `pass_m` (1.5 km), he extends straight ahead at full speed for `extend_s` (`Extend`;
  `checo_1969_a2a`, the MiG-21's hit-and-run under GCI; `mt_turchin_coop`).
- *Turn* (the neutral, the rebels): straight in by lead pursuit and stays in the fight.
- *Drag* (all): on a lock or a launch on his receiver, or a launch or trail seen within 10 km, within `drag_warning_s`,
  a pilot who cannot shoot first turns tail and runs at full speed for `drag_s` (`Drag`, radar off). A green pilot
  always drags; a skilled one only if no hostile he knows of is inside the reach of what he carries and he has no
  missile of his own in the air.
- *Flown rounds.* A missile is now a point flying lead pursuit at its mean speed for as long as its maximum ranges
  allow against a target of `missile_ref_target_mps` (250 m/s): its head-on range over its speed plus that, or its
  range from astern over its speed less it, whichever is longer. A target that drags runs it out of energy
  (`MissReason::Energy`, new). At its end the shot is resolved on the end geometry's beam (`ShotConditions::end_notch`:
  a radar round ending with its target in the beam against the ground suffers the chain's `beam_notch`).

**In the fight.**

- *The strikers' chaff and flares.* A target who knows of a round flying at him (a launch on his receiver, a lock for a
  radar round, a launch or trail seen) answers it once, about 4 s before it arrives (`AirFightWorld::dispense`): the
  runner's reaction chain (`ReactionChain::AnswerAam`) dispenses flares at an infrared round and chaff at a radar one,
  by his profile's habit against the like SAM and from what his type carries that year (`countermeasures.json`). The
  salvo is the chain's own (its count, its event, the chaff cloud in the EM world, the fx feed). At the round's end its
  strength goes into the shot (`countermeasure`): flares out within `flare_window_s`, `flare_decoy`; chaff within
  `chaff_window_s`, 1 - `chaff_factor`.
- *The escort's station.* An escort (`"escorts"`: a striker's target id, else the nearest of his side's within 40 km)
  with no contact flies his station on the strikers' leader: `escort_back_m` behind, `escort_side_m` abeam (the side by
  his key) and `escort_up_m` above, at a speed that closes on it (`Station`); he fights only what comes within
  `escort_tether_m` (15 km) of his strikers. The raid builder now writes a package's sweep and CAP as `sweep` and `cap`
  (before, as strikers).

**The far fight (A1).** A fighter whose flight is beyond every reach of our ground units (the runner's near points
without our fighters', `FarFromOurGround`) thinks at `a1_think_s` (1 s) and, a wingman whose leader is far too, fights
on his leader's picture: no look of his own, his leader's mode, and his shots judged on his leader's geometry. A level
never changes while a missile of his or at him is in the air. It is the fight's own notion, not B12's levels, so B12's
level equality stays as it was.

**A0.** With `air_fight.mode` 1: a beam attacker's shots come from the target's beam (±30°); a slash by the side that
fired first is one pass, after which only `a0_slash_return` (0.4) of it comes back for another round (fights between
fighters only: strikers cannot chase); a turning doctrine on either side keeps a fight between fighters
`a0_turn_rounds` (1) longer; a fighter shot at by a radar round from beyond sight drags it at `a0_drag` (0.6) times his
skill; strikers that saw the shot answer it with flares or chaff if their type carries them in 1985, at the reaction
table's strengths (`AirWarData::reactions`, loaded from `gameplay/reactions.json`).

**Measures.** `air_combat_bench data [seeds]` flies four cases in a small world of its own (aircraft that turn at their
rate, eyes to 9 km, a receiver that hears locks and launches), B8's fight beside S3's, then the far fight off and on.
Each counts the losses, shots, drags, extensions, set-ups, dispenses, rounds out of energy and in the notch, the
thoughts and looks and the wall time. With 50 seeds a case (19 November 2026):

```
air_combat_bench: 50 seeds a case; A is the first faction, B ours
             A lost B lost  shots  drags extends setups  disp energy notch  thinks    far senses       ms
nato 2 f15c (beam, E-3A) vs our 2 mig23ml (slash, GCI)
  B8             17     81   1021      0       0      0     0      0     0  160280      0 151290    277.6
  S3              5     51    494   1266      12   2161    33    159     0  195620      0 194664    559.9
warsaw_pact 2 mig23ml (slash, GCI) vs our 2 mig21bis (slash, GCI)
  B8             25     26   1268      0       0      0     0      0     0  207777      0 198718    671.1
  S3              8     37    569   1334     551   1344     0    144     0  205613      0 204577    384.3
neutral 2 draken_gun (turn) vs our 2 mig21bis (slash, GCI)
  B8             32      0    440      0       0      0     0      0     0  216564      0 112793    336.8
  S3              9      0    303     13     558   1212     0     13     0  236103      0 120100    344.6
nato 4 tornado_ids low + 2 f15c escorts vs our 2 mig23ml
  B8              2     49    766      0       0      0     0      0     0  435943      0 433911   1591.2
  S3             10     59    616   1122      51    956   167     49     0  418947      0 418947   1547.2
far: nato 4 f15c (E-3A) vs our 4 mig23ml (GCI), far from our ground units
  S3 A2           8    158   1016   1372       4   3219    42    145     0  341984      0 341984   1292.7
  S3 A1          10    154   1343   1167      47   2034   215    510     0  172688 172688  98460    825.3
```

S3 fights fewer, longer engagements: the drag, the set-ups and the slash's extension halve the shots, and the drags
make between one round in twenty-five (the Drakens' targets) and one in three (the Eagles') fall short (`energy`). The
Eagles lose 5 instead of 17 against the MiG-23s and kill 51 instead of 81: both sides drag what they cannot outshoot.
The Pact's slash does better against our MiG-21s: 8 lost to our 37, from 25 to 26 under B8's pursuit; its MiG-23s
shoot and extend before the MiG-21s close to their R-60s. The neutral's Drakens still never kill a MiG-21 with their
guns, and lose under a third as many. Against the escorted Tornados the strikers dispense 167 times in 50 runs. No
round ends in the notch: the bench's fights are over 3 km above the ground, where no radar looks down into clutter.
Far from our ground units (A1) the fight thinks half as often and looks under a third as often, at two thirds of the
wall time, and its outcome stays near A2's (10 and 154 lost against 8 and 158); the wingmen on their leaders' picture
fire more rounds that fall short (510 against 145).

**Keyed draws.** No new stream and no new draw in the session: S3 decides by geometry and the chain's existing draws.
The abstract fight's new draws come from its own generator (seeded from the package's fork), only with mode 1.

**Tests**, `test_air_fight.cpp` (S3): the radars' look-down from their EM entries; who may vector whom, and a NATO
sweep under a CRC hearing no vector; a radar silent while vectored in, then searching, then locked, a striker's never;
the three attacks told apart; a drag on a lock and its end; a striker dispensing once before the round arrives and the
decoy read at its end; an escort on his station; the far fight's pair on its leader's picture, cheaper and still
fighting; a round run out of energy; A0's drags, extensions and decoys only with mode 1; the save's key; the scenario's
keys. `test_perception_em.cpp`: a fighter radar's search sweeps past a NATO receiver, its lock is called a lock, and
off is silent.

**Left.**

- The bench's cases are not yet set against Vietnam, Linebacker, the Bekaa, the Falklands and ACEVAL (`air_war_bench`
  does that for the abstract fight); the live fight's exchange is not banded.
- One or two circles of a turning fight are not flown: the turning doctrine closes by lead pursuit and stays.
- A fighter's radar in the EM world is seen by every receiver, but his own search is still `sensors.json`'s ranges,
  not the radar equation through the EM world; his own warning receiver is the session's picture, not his RWR's.
- The A1 wingman borrows his leader's picture only when both are far; a flight split across the line is two levels.
- The escort's tether and station are game values; no period source gives NATO's close-escort geometry in 1985.
- `look_down_clutter_db` is one figure for every ground; the sea, towns and forest are not told apart.
- The bench's fights are all above 3 km over the ground, so no radar round ends in the notch there; a low case is left.
