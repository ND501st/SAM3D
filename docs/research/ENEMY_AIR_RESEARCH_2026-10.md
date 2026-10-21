# The enemy's air forces, 1980–85: research for the enemy air command

Written 18 October 2026 for the enemy air command (`docs/air_war/`, `simcore/air_war/`). The player's country is
fictional and resembles Czechoslovakia, Hungary, Romania and Yugoslavia: Soviet-equipped, but able to find itself at war
with a rebel faction inside, with a small neutral neighbour, with the Warsaw Pact, or with NATO. The question this file
answers: **how did each kind of air force of the period plan and fly its air war, what did it know about the defender,
how did it learn and how did politics bend it**, so that the people who plan the raids the player faces behave like
the real staffs did. It also surveys the games that have tried to give an unseen enemy staff a mind of its own.

Sources are registered in `docs/SOURCES.md` ("Enemy air command sources"). Tiers follow that file. For an adversary's
doctrine the best tier is the adversary's own documents (Soviet and Russian for the Warsaw Pact, US and NATO for NATO);
the other side's assessments are recorded as what that side believed. Every number that goes into `data/` from here
names its source; the rest are game values and say so.

## 1. Summary: what the research changed in the design

1. **Staleness is the defender's best weapon.** The most expensive SEAD failure of the era (the Israeli Dugman 5 raid
   on the Golan, October 1973) was planned on imagery of SAM batteries that had moved (`wiki_model5`). The most famous
   success (the Bekaa, June 1982) came from live drone surveillance, decoys that made the radars switch on, ELINT that
   recorded them, then anti-radiation missiles and bombs inside minutes (`decoys`, `bekaa_popular`). So the enemy's
   picture of a mobile battery must age fast, and the enemy must have ways to make it radiate.
2. **Switching off beats an anti-radiation missile without memory.** In the Falklands the Argentine operators knew the
   Vulcans carried Shrikes and switched their radars off or to low power as they came; the main surveillance radar
   survived the war and the radars that were hit were quickly replaced (`wiki_black_buck`). The Czechoslovak army taught
   the same discipline against TDOA location (`vr_1985_plss`). EMCON is a real player choice with a real price.
3. **Pilots claim more than they destroy.** The best-documented case is later than our period (Kosovo, 1999: claims fell
   from 120 tanks to 93 on reassessment, against a Serbian figure of 13, with decoys and double counting blamed,
   `afmag_2000_kosovo`); it is used as a design reference only. Every enemy has a bias in its battle damage assessment
   and the planners re-strike, or don't, by what they believe.
4. **Politics and leadership reach down to the time on target.** On 4 December 1983 "higher authority" moved a US Navy
   strike on Syrian positions in Lebanon to an earlier time over the on-scene commander's objection: aircraft launched
   with the wrong or partial loads, crews under-briefed, flying into the sun through haze, with no time for deception;
   two aircraft were lost to SAMs (`hgram_080`, `csm_1983_lebanon`). Rolling Thunder's target lists were approved in
   Washington and whole zones were off limits (`nmusaf_sanctuaries`). Linebacker II's centrally planned, predictable B-52
   streams were changed only after the losses of the first nights became unacceptable (`dtic_linebacker`).
5. **Each adversary is a different kind of mind.** A Soviet-style air army plans centrally, strikes in massed, timed
   blows through corridors cleared by suppression groups, approaches low (`mai_tactics_vvs`, `cia_1985_air_op`). A NATO
   air force runs a daily tasking order, builds packages around strike flights with SEAD, jamming and escort, and runs
   long, methodical suppression campaigns (`jsead_1982`, `afmag_1989_slam`, `usaf_ato_doctrine`). A small neutral air arm
   has a handful of light jets and helicopters and cannot afford to lose them (`austrian_af_wiki`). A rebel faction
   bolts bombs to crop sprayers (`croatia_1991_migflug`).
6. **The officers are the interesting part.** Games that made an unseen enemy feel alive did it with named
   individuals who remember the player (Shadow of Mordor's Nemesis, `nemesis_gdc_2018`), with an enemy that learns about
   the player until it can strike at the base (XCOM 2's Chosen, `xcom2_wotc`), and with a director that paces pressure
   and relief (Left 4 Dead, `l4d_director_2007`). SAM3D takes the memory, the learning and the habits, and leaves the
   director to choose only which of the enemy's real raids the player plays.

## 2. The four adversaries

### 2.1 Inferior: a rebel faction in a civil war (light air power)

**Analogue.** Croatia in 1991 had no air force when the war began. Its first air arm was improvised: An-2 biplanes and
other agricultural and sport aircraft (UTVA, even ultralights) dropping home-made bombs on positions around Vukovar; an
air platoon at Osijek was formed on 8 October 1991 to supply the besieged town and bomb, and the service was founded
formally on 12 December 1991 (`croatia_1991_migflug`, low confidence: a popular site, but consistent with the Croatian
general's own quote that they "installed weapons in agricultural aircraft ourselves"). The period's other improvised
air arms (night harassment by light biplanes in Korea, small civil aircraft with rockets in Biafra) are background,
not cited in data.

**What it means for the game.**
- A dozen airframes at most: crop sprayers and light aircraft with improvised bombs, a few transport and utility
  helicopters with door guns or rocket pods, maybe a defected light jet or two.
- Planning is ad hoc, by a few pilots, the night before. No SEAD at all: the only defence is to avoid known guns and
  missiles, to fly at night and low.
- Intelligence is local and human: sympathisers, villagers, deserters. Good at knowing where things are near home, slow,
  sometimes planted. No ELINT, no imagery beyond a pilot's eyes.
- Targets chosen for effect: symbolic, propaganda, supply of a besieged town, the unit that killed a friend.
- Every airframe is irreplaceable; one loss is a disaster. Leaders are charismatic amateurs: ex-reserve, aero-club and
  airline pilots.

### 2.2 Near peer: a small neutral nation (limited air power, helicopters, light jets, front-line raids)

**Analogue.** Austria's air force of the 1980s: 40 Saab 105Ö light jets delivered from 1970 (trainer and light attack),
some two dozen Agusta-Bell 212 utility helicopters from 1980, Alouette III for rescue; the Draken interceptors arrived
only in 1988 (sources differ: 24 or 28); a ban on guided missiles lasted until the early 1990s, so the Drakens carried
guns only (`austrian_af_wiki`). Switzerland and Albania are similar in scale. Nothing here is used as Austrian data: the
neutral neighbour is fictional, and the numbers are game values shaped like this.

**What it means for the game.**
- Light jets with rockets, gun pods and small bombs; armed utility helicopters; a few gun-armed interceptors.
- A defensive doctrine: support its own troops near the line, raid what is close to the front, rarely deep.
- Some recce (camera pods on the light jets), border observation posts, a ground ELINT station or two; no satellites.
- Very loss-averse (a small fleet, no replacements) and politically constrained: neutrality, international opinion,
  a limited war aim. Raids are short, low and fast, often at dawn or dusk, with the helicopters popping up from cover.

### 2.3 Peer: the Warsaw Pact (deep strikes, mixed packages, basic SEAD)

**Organisation.** The US Army's handbook of the Soviet Army (1991) describes the Air Forces of the Front as having no
fixed organisation: typically two or three divisions of fighters and fighter-bombers, one or more reconnaissance
regiments, and helicopter units; each fighter or fighter-bomber regiment had 45 aircraft in three squadrons of 15
(including up to five trainers), an attack helicopter regiment 40 Mi-24 and 20 Mi-8; the Air Forces of the Western
Group of Forces alone had more than 700 combat aircraft and over 350 attack helicopters (`fm100_2_3` pp. 4-127 to 4-134;
what NATO believed). The reconnaissance regiment's chart lists MiG-25 reconnaissance aircraft and Yak-28 jammers.

**The air operation.** A 1985 CIA assessment summarises Soviet military writings: in the mid-1970s the Pact planned to
defeat NATO's air forces in an intense battle of attrition in the first two days of a war, by an offensive air
operation relying on surprise and overwhelming force to deliver three crushing blows against NATO's airbases in
Central Europe; by the late 1970s Soviet writers judged this unworkable, because surprise would be hard and most NATO
aircraft would survive on the ground (`cia_1985_air_op`, read from the search excerpt; the full text could not be
fetched). A 1972 CIA report on Pact concepts for conventional air operations against NATO says the air forces would be
the principal means of destroying critical targets throughout the theatre in a conventional phase (`cia_1972_wp_air`).

**Tactics (operator country).** A Russian Air Force tactics textbook of the MAI military institute (`mai_tactics_vvs`,
§3.3.3, bomber and ground-attack aviation) gives the Soviet-lineage method in detail:
- The commander chooses the **method of action** from the number of targets and how complete the information on them
  is, the forces available, the urgency, the required distribution of effort in time and the state of the forces.
- Four methods: a **simultaneous strike** by the whole force or most of it at a set time (when a dense blow is needed;
  the best conditions to suppress and get through the air defence); **successive strikes** by squadrons, flights, pairs
  at set times (when forces are short, or to act on a target for a long time **and to prevent restoration work**);
  **successive strikes on call** from ground or airborne alert against targets found during the fighting (missile
  launchers in position, troops on the march; mostly in support of the ground forces); and **free search, "the hunt"**,
  by up to a flight, when the target's location is not known precisely.
- Units fly to the target in **tactical groups**: the strike group (most of the force) and support groups: electronic
  warfare; additional reconnaissance, search and target marking; destruction or suppression of air defences; weather
  reconnaissance; clearing the airspace; escort; demonstration (diversion).
- Strike groups approach at **low and extremely low altitude** using the terrain; support aircraft may fly higher to find
  the target, and may fly diversions. The route is the shortest, with a variable profile, bypassing air defences; the
  strike itself is short. Control passes to forward air controllers with the ground troops at set lines. Attacks are
  from a dive, from level flight or from a pitch-up (toss).
- At night the strike group flies a column of pairs at different heights, dispersed.

A Russian journal article on precision attacks on airfields describes the same picture from the other side: strike
groups with fighter escort in dense formations pass in the shortest possible time through **corridors in which the air
defences have been suppressed** (`cyberleninka_airfields`).

**SEAD means.** The MiG-25BM with the Kh-58 anti-radiation missile was the Soviet "wild weasel", accepted in 1981 and
built 1982–85 (40 aircraft); the Kh-58 could also be carried by the Su-24 (at speed limits), and superseded the Kh-28 in
the early 1980s (`mig25bm_refs`, tier 4, low confidence). Stand-off jamming came from Yak-28PP and Mi-8 jammer variants
(the reconnaissance regiment chart in `fm100_2_3`).

**What it means for the game.** Centralised, punctual, massed: a plan made at army level, timed to the minute, flown in
waves through cleared corridors, low; on-call strikes against what the reconnaissance finds; hunting flights over areas
where a battery is suspected; ARMs and jamming as the "basic SEAD". Rigid when things go wrong; strong when things go
to plan. A political officer at every level.

### 2.4 Superior: NATO (complex packages, long SEAD and DEAD campaigns, EW and deception)

**The tasking cycle.** The air tasking order (historically the "frag order") is a 24-hour execution document listing
every sortie, its call sign, type and mission; strategic guidance becomes an air operations directive and a master air
attack plan, and the cycle runs objectives and guidance, target development, weaponeering and allocation, ATO
production, execution planning and assessment (`usaf_ato_doctrine`; `wiki_ato`). The 1980s ATAF details are not in
these sources: the 24-hour order with a planning horizon of a few days is used, and recorded as the modern description.

**SEAD doctrine.** The joint US Army–Air Force *J-SEAD* training text of 11 June 1982 (TRADOC TT 100-44-1, also TACP
50-23) describes the joint concept, planning and coordination and a sequence for developing a SEAD plan (`jsead_1982`;
only the abstract could be read). The later joint publication (JP 3-01.4, 1995) sorts suppression into theatre-wide
campaigns, **localized suppression** for particular operations and **opportune suppression** of what appears
(`jp3_01_4_1995`; later than the period, used for the categories).

**The electronic combat force, as described in 1989** (`afmag_1989_slam`): 42 EF-111A Ravens and fewer than 130 F-4G
Wild Weasels; the 52nd TFW at Spangdahlem was the only all-SEAD wing outside the US. Weasels fly in two-ship
**hunter–killer teams**: the F-4G finds the emitter and fires, or passes the data to the killer (an F-4E, at Spangdahlem
an F-16C), which cannot find emitters itself. Ravens jam in three ways: **stand-off** (many emitters masked from far
away, out of the densest defences), **close-in** (a corridor opened in one area, at more risk) and **escort** (with the
package all the way, as against Libya in 1986). The aim is to force the radars to go autonomous, then destroy, jam or
shut them down to open holes for the strike. HARM (more than ten miles) was preferred and **in short supply**; Shrike
(about three miles, lofted for more) was older and less reliable; then Maverick, bombs and overflight. The Weasels
should be concentrated in one area at the right time rather than spread along the front. The EC-130H Compass Call
jammed communications.

**What it means for the game.** A daily cycle; packages built around strike flights with SEAD, stand-off and escort
jamming, sweeps and escorts; recce and BDA sorties; a long campaign to roll back the defences methodically (hunter–killer
patrols, DEAD strikes on located sites, decoys to make radars radiate), then interdiction and support of the ground war.
Scarce ARMs. Strong ELINT, satellite imagery, coalition politics and a low tolerance for losses and prisoners.

## 3. Lessons from the period's air campaigns

| Event | What happened | Mechanic it supports |
|---|---|---|
| Dugman 5, Golan, 7 Oct 1973 | Planned on outdated imagery; the SAM batteries had moved; weak ELINT; six F-4s lost, one battery destroyed (sources differ on a second damaged), most aircraft hit by ZSU-23-4 at the low approach height (`wiki_model5`) | Intel ages; mobile targets decay fast; a strike on a stale position hits empty ground; low ingress trades SAM exposure for gun exposure |
| Linebacker II, Dec 1972 | Centrally controlled from SAC headquarters, predictable and inflexible; losses after three days judged unacceptable; decentralised control of mission preparation and tactics and varied routes cut losses (`dtic_linebacker`) | Rigid officers repeat a plan until losses force a change or higher command intervenes; decentralisation after failure |
| Rolling Thunder, 1965–68 | Targets approved by the White House; in 1966 bombing within 30 miles of Hanoi and 10 nm of Haiphong prohibited, a buffer zone along the Chinese border (`nmusaf_sanctuaries`) | Political no-strike zones and target-category bans that the planners must obey |
| Bekaa (Mole Cricket 19), 9 June 1982 | Drones kept the valley under watch; decoys drew the radars on and missiles off; EW aircraft recorded the radars; ARMs, then bombs (`decoys`, `bekaa_popular`; numbers disputed: 17 of 19 batteries in one account) | Provoke, record, kill: decoy waves and hunters that wait for emissions; the faster the chain, the less time to move |
| Lebanon, 24 July 1982 | A Syrian SA-8 shot down an Israeli F-4 (`wapo_1982`) | The Osa is a real threat even to a superior air force |
| Black Buck, May–June 1982 | Operators warned of ARMs switched the radars off or to low power; the main radar survived; the radars hit were replaced quickly (`wiki_black_buck`) | EMCON defeats ARMs without memory; ARM claims of "radar destroyed" are often just "radar off" |
| Lebanon, 4 Dec 1983 | 28 A-6 and A-7; launch moved earlier by higher authority over the on-scene commander's objection; wrong or partial loads, under-briefed crews, into the sun through haze, no time for deception; one A-6E and one A-7E lost to SAMs; led to the Naval Strike Warfare Center (`hgram_080`, `csm_1983_lebanon`) | Political/leadership interference degrades planning; poor timing and lost deception raise losses; a failure leads to reform |
| Angola, 1987–88 | SAAF Mirage F1 and Buccaneer long-toss attacks: ingress about 100 ft, pull-up 4–5 nm out (`saaf_toss`, a first-hand forum account); the targets far away, only minutes over them; Mirages lost to SAMs in 1988 | Loss-averse air arms deliver from outside the point defences; an Osa sees a brief pop-up |
| Kosovo, 1999 (out of period; design reference only) | BDA fell from 120 to 93 tanks on NATO's own reassessment; the Serbian figure was 13; decoys and double counting blamed (`afmag_2000_kosovo`) | Claims are inflated; decoys absorb strikes; the enemy's picture of its own success is wrong |

## 4. What the enemy can know: intelligence in 1980–85

| Discipline | NATO (superior) | Warsaw Pact (peer) | Neutral (near peer) | Rebels (inferior) | Defeated by |
|---|---|---|---|---|---|
| Satellite imagery | Near-real-time digital imagery existed from 1976 (KH-11 class, background knowledge); passes at predictable times; cloud blocks it | Film-return satellites, days of latency (background knowledge) | none | none | Moving between passes, nets, dummies, cloud |
| Air reconnaissance | RF-4C, Jaguar, Mirage F1CR class; film needs hours to process; the aircraft must survive | MiG-25R, MiG-21R, Su-17 class; the reconnaissance regiments of `fm100_2_3` | Light jets with camera pods | A pilot's eyes; helicopters | Shooting it down; camouflage; dummies |
| ELINT | Stand-off collectors; TDOA location in seconds once a source switches on (`vr_1985_plss`: the Czechoslovak army's reading of PLSS on the TR-1) | Il-20 and ground stations | A border station or two | none | EMCON: transmitters off, dummy loads, brief bursts, decoy emitters on the same parameters (`vr_1985_plss`) |
| COMINT | Voice nets intercepted and direction-found | Same | Little | Listening on captured sets | Radio discipline, dummy traffic (`em/deception.hpp`) |
| HUMINT | Agents, refugees, line-crossers: coarse and slow | Agents, special forces: good | Border observers | Sympathisers and villagers: good locally, slow, sometimes planted | Counter-intelligence; disinformation |
| Pilot reports and BDA | Biased upward; post-strike recce corrects it later | Same | Same | Same | Decoys, dummies, switching off under ARM attack |

The EM layer already models what ELINT and COMINT can measure (TDOA, bearings, triangulation, decoys, dummy traffic:
`docs/EM_ARCHITECTURE.md`). The enemy air command consumes the same quantities at campaign scale: how long the battery
radiated, from where, how much it talked, and how many decoys radiated with it.

## 5. Games that gave an unseen enemy a mind

| Game | What it does | What SAM3D takes | What SAM3D avoids |
|---|---|---|---|
| **Falcon 4.0** (1998) and Falcon BMS: the dynamic campaign | Units and the engine raise mission requests (against factories, airbases, ground units); each is prioritised; the air tasking manager frags packages from the requests; players can move priority sliders (`falcon4_campaign`) | The request → priority → package pipeline; the ATO as the planning product | Opaque priorities nobody can explain |
| **DCS Liberation / Retribution** | Turn-based theatre; each turn's packages form the ATO; SEAD and escort flights attached to strike flights; hold and join points; SEAD attacks defences near the mission area (`dcs_liberation_wiki`) | Packages of flights with roles; routing with join points and threat zones | Perfect knowledge of the defender's sites |
| **Command: Modern Operations** | Missions (strike, patrol, support) with doctrine, rules of engagement, EMCON and weapon release authority settings per side, unit and mission (general knowledge; design reference) | Doctrine as data at several levels, overridden by the officer | — |
| **GMT Downtown / Red Storm** (2004 / 2019) | Board wargames of the air war over North Vietnam and over central Germany in 1987: players build strike packages and CAPs; heavy SEAD and jamming raise complexity; a two-tier solitaire system (`gmt_red_storm`) | Package-building as the core decision; raid planning seen from the attacker's chair | — |
| **Middle-earth: Shadow of Mordor / War** (2014 / 2017): the Nemesis system | Named captains with strengths and weaknesses who remember encounters, are promoted when they beat the player, and come back (`nemesis_gdc_2018`) | Named officers with memory, grudges, promotion and replacement; the player learns their weaknesses | The cartoon: our officers never meet the player |
| **XCOM 2: War of the Chosen** (2017) | Each Chosen fills a knowledge bar about the player; as it grows it gains strengths and harasses the campaign; full, it assaults the base; strengths and weaknesses are randomised per campaign (`xcom2_wotc`) | The enemy's knowledge of the player's battery as a visible threat that grows until a DEAD strike comes | — |
| **Left 4 Dead**: the AI Director (2008) | Paces the threat: schedules attacks when the game goes quiet, lets it settle after long intense combat (`l4d_director_2007`) | A pacing layer that only chooses which of the enemy's real raids the player plays and when the campaign gives respite | Spawning threats the enemy doesn't have |
| **RimWorld** storytellers, **Hearts of Iron** and **War in the East** generals, **Highfleet**'s intercepted radio (general knowledge) | Pacing personalities; leaders with traits that change outcomes and can be fired; the enemy perceived through intercepts | Traits as numbers in decisions; relief and replacement; the player learns of the enemy through intercepts and reports | — |

## 6. Design decisions taken from this research

| # | Decision | From |
|---|---|---|
| D1 | Four adversary doctrines as data: planning cycle, methods, package templates, capabilities, politics, logistics. Not four AIs: one planner with four doctrines and many officers | §2 |
| D2 | The enemy never reads the truth: it plans on its **intel picture**, fed by collection disciplines with latency, error and bias, and aged by mobility | §1.1, §4 |
| D3 | The air staff are **people**: a commander, a planner, an intelligence officer, an EW/SEAD officer, squadron leaders (and a political officer in the Pact), each with traits, skills, quirks, weaknesses, memory, stress, reputation, a career | §1.6, §5 |
| D4 | Decisions by utility with explanations: every package carries the reasons its officer chose it, for the debrief and for the player's intelligence reports | §5 (Falcon's opacity avoided) |
| D5 | Learning from perceived results: tactics are remembered with their believed success and losses; rigid officers learn slowly; failure brings intervention or relief | §3 (Linebacker II) |
| D6 | Politics as restrictions, pressure and interference events: no-strike zones, banned categories, loss tolerance, higher authority moving times on target | §3 (Rolling Thunder, Lebanon 1983) |
| D7 | SEAD as a chain: find (recce, ELINT), provoke (decoys, hunters), kill (ARMs, DEAD strikes); EMCON and moving break it; ARMs without memory miss a radar that switched off | §3 (Bekaa, Black Buck) |
| D8 | BDA is biased; decoys and dummies absorb strikes and are claimed as kills; re-strikes follow belief, and the repair of real targets is struck again (the MAI textbook's successive strikes "to prevent restoration") | §2.3, §3 (Kosovo) |
| D9 | Raids through the player's sector become scenarios the existing runner plays (targets, behaviours, jammers, C2 losses, warnings); the rest are resolved abstractly by the same rules | integration |
| D10 | The director only selects and paces; it never invents forces | §5 (L4D) |

## 7. Open items and documents to acquire

- The 1985 and 1972 CIA assessments in full (the reading room refuses automated downloads): what replaced the three
  blows, sortie rates, expected losses.
- FM 100-2-1 (1984), *The Soviet Army: Operations and Tactics*: its air support and radioelectronic combat chapters (the
  FAS copy is behind a browser check).
- TRADOC TT 100-44-1 (1982), *J-SEAD*, full text (DTIC refused the download): the planning sequence and the categories
  as written in 1982.
- Warsaw Pact exercise documents (Parallel History Project), Czechoslovak and Hungarian air force exercise records, NVA
  LSK/LV documents: the Pact's air plan as its own officers wrote it.
- Yugoslav *Total National Defence* writings: the defender's view of an attack by either bloc.
- A period source on Western satellite-pass warning to ground units (both sides hid from overflights; no source found
  yet: the mechanic stays a design choice).
