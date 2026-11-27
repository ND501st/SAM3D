# The fronts: research synthesis (track F, F0; November 2026)

Track F gives the war fronts that move: the enemy's air front by sector, the ground war by its units and officers, and
our national air defence's strategy on both (`docs/DEVELOPMENT_ROADMAP.md`, track F; design in `docs/fronts/`). This
note points into the four topic files that hold the research, lists the norms the design uses, and records where the
sources disagree and which was used. It adds no new reading: every number below is in a topic file, with its page.

**The war's year is 1985.** Later texts are hindsight about period events, or give only the shape of a distribution.
Sources are registered in `docs/SOURCES.md` ("Fronts sources (track F, 20 Nov 2026)"). Each topic file ends in a
verification of 19 November 2026: the key numbers re-opened in the texts and marked confirmed, corrected or unsupported.

## 1. The four topic files

| File | The question | Its main sources | Verified |
|---|---|---|---|
| `fronts_2026-11/pact_ground.md` | How a Soviet-style force attacked and defended in 1985: echelons, objectives in depth, rates of advance, frontages, the correlation of forces, losses and effectiveness, the march under air threat, frontal aviation's support, supply | FM 100-2-1 and 100-2-2 (1984, NATO's reading); Reznichenko's *Тактика* (1984); eight *Военная мысль* articles in CIA translation (1965-69) | 16 numbers; 2 corrected; Reznichenko's page numbers to recheck |
| `fronts_2026-11/nato_ground.md` | How NATO fought on the ground and in the air over it: the layered battlefield, the FLOT, FEBA and FSCL, CAS, BAI and AI, FOFA, J-SEAD, losses, rates of advance, the march, its own air defence moving | FM 100-5 (1976, 1982), FM 71-100, 71-2, 101-10-1, 6-20; OTA's FOFA report; Davis's *31 Initiatives*; HERO and Dupuy (analysis) | 12 numbers; 2 corrected; US frontages unsupported (none given) |
| `fronts_2026-11/air_ground_cases.md` | When air forces moved close support and helicopters forward over broken air defence, what pulled them back, how fast air defence came back; the small air forces' and the neutral's appetite | Werrell (2005), Brant (1986), the NWC's Lebanon study (1983), NPIC and FRUS on Egypt and Syria, Lambeth (1984), *Военная мысль* on Vietnam, Bobrov and Babich (1984), CIA on Angola and Nicaragua | 13 numbers; 1 corrected |
| `fronts_2026-11/our_side.md` | How a small Pact-trained army defended its ground in 1985, how its troop air defence moved with it, how its national PVO set priorities | The ČSLA's *Vojenské rozhledy* (1983-85), including Poltikovič's preview of the 1985 combat regulations; Reznichenko; the Osa textbook (`voenizdat_osa_1991`); *Военная мысль* (Rozhdestvenskiy, Lavrinovich); Andersen's *ПВО сухопутных войск* (1979) | 14 numbers, all confirmed |

What each file found, in a line each:
- **pact_ground.** The Pact attacks at about 3:1 aggregate, in two echelons against a deep defence, feeding success,
  25-30 km a day through a defence and 40-50 in depth. A formation under 40-50 % losses is still effective, and none
  withdraws without its senior's leave.
- **nato_ground.** NATO's 1985 manuals give depth in hours, not kilometres, and no frontage for its own units. The 1976
  ratios (defend against 3:1, attack at 6:1) are the planners' rule of thumb. FOFA bands the deep battle at 5-30,
  30-80 and 80-150 km. No doctrine sets a combat-ineffective percentage.
- **air_ground_cases.** Air forces kept off an intact, dense, mobile SAM belt. They came over it within a day of a
  local hole about 24 km wide. They pulled back on a loss rate of about 4 % a sortie. Helicopters crossed only where
  their side held the air. Defenders came back overnight into prepared positions when the attacker let them, and new
  mobile units took about three weeks.
- **our_side.** The ČSLA's 1985 regulations give a division 30-45 km by 20-25 km in defence, a security zone 20-40 km
  deep fought line by line, and withdrawal and counterattack only on the senior's order. The Osa battery stands 3-5 km
  behind the forward edge and moves by pairs. Long- and medium-range SAM units move whole, two or three times a day at
  most. The national PVO covers our troops on home ground, by objects ranked by importance.

## 2. The norms the design uses

The value used, the source and the design file that uses it. Confidence and the alternatives are in the topic files.

### 2.1 The front and its measures

| Norm | Used | Source | Where |
|---|---|---|---|
| FLOT, FEBA, line of contact | FLOT: our most forward positions; FEBA: the forward limit of the main battle area; line of contact: where the two sides are engaged | `fm_101_5_1_1980` pp. 1-52, 1-53, 1-62 | `03_GROUND_WAR.md` §2 |
| FSCL | set by corps on a terrain line; 20-30 km beyond the FLOT in defence, the next terrain line beyond the day's objective in attack | `fm_101_5_1_1980` p. 1-49; `fm_6_20_1977` p. D-5; `ota_1987_fofa` pp. 78-79 | `03` §6, `02` §4 |
| NATO's air support by depth | CAS short of the FSCL with detailed integration; BAI either side of it inside the area of influence; AI beyond | `fm_101_5_1_1980` pp. 1-3, 1-15; `fm_100_5_1982` p. 7-11 | `02` §4 |
| J-SEAD by depth | the Army to the limit of observed fire, the Air Force beyond | `davis_1987_31_initiatives` p. 31 | `02` §3 |
| Our division in defence | 30-45 km wide, 20-25 km deep; regiment 10-15 by up to 12 km; battalion up to 5 by up to 3 km | `vr_poltikovic_1984` pp. 95-96 | `03` §3 |
| A Pact division attacking | zone 15-25 km, breakthrough on 6-10 km; regiment 3-8 km (typically 4-5) | `fm_100_2_1_1984` pp. 5-18 to 5-21 | `03` §3 |
| A Pact army | frontage 60-100 km; 2-4 motor rifle and 1-2 tank divisions | `fm_100_2_1_1984` pp. 4-4 to 4-7 | `03` §3 |
| NATO's frontages | not in the period manuals; SAM3D estimate: defence division 25-40 km, brigade 8-15 km, battalion 3-6 km; attack division 10-20 km | estimate bounded by `fm_100_5_1976` p. 5-3 | `03` §3 |

### 2.2 Attack, defence, delay and withdrawal

| Norm | Used | Source | Where |
|---|---|---|---|
| The Pact attacks at | about 3:1 by weighted strength over the axis; 3-5:1 locally at a breakthrough | `fm_100_2_1_1984` p. 2-11; `reznichenko_1984` p. 79 | `03` §5 |
| NATO attacks at; defends against | seeks 6:1 at the point of decision, 3:1 or better in the sector; holds while under 3:1, briefly to 5:1 with air and artillery on good ground, then delays | `fm_100_5_1976` pp. 3-4, 3-5, 5-3 | `03` §5 |
| Echelons | two against a deep defence, one and a reserve against a shallow one; the second echelon committed to success | `fm_100_2_1_1984` pp. 2-10, 2-11 | `03` §5 |
| Objectives in depth (the Pact's plan) | division's day 25-30 km; army 100-150 km in 3-4 days, 250-350 km in 6-7 | `fm_100_2_1_1984` fig. p. 4-4 | `03` §5 |
| Our security zone | 20-40 km deep, lines 6-8 km apart; a reinforced battalion delays a brigade 1-2 h a line; the army sets its depth, the division runs it and orders each withdrawal | `vr_poltikovic_1984` p. 96; `vr_krizek_1985` pp. 32-36 | `03` §5 |
| NATO's covering force | told in time (72 h, 7 days, until the main body is ready); 30-50 % of a division's manoeuvre units | `fm_100_5_1976` p. 5-12; `fm_71_100_1978` p. 5-13; `fm_101_10_1_1976` p. 7-16 | `03` §5 |
| Withdrawal (the Pact and our side) | only with the senior commander's permission | `reznichenko_1984` pp. 49, 152; `voenizdat_osa_1991` p. 238 | `03` §5, `04` §4 |
| Counterattack (the Pact and our side) | the next higher commander's; the second echelon from the march into the flank, tanks leading, to a set line | `reznichenko_1984` pp. 177-179; `vr_poltikovic_1984` p. 96 | `03` §5 |
| Reaction and planning times | division 2-4 h and 1-3 h; regiment 1-3 h and 0.5-2.5 h; battalion 25-60 min and 20-45 min | `fm_100_2_1_1984` p. 5-14 | `03` §5, README efficiency |
| Posture multipliers in the combat result | attack 1.0, hasty defence 1.3, prepared 1.5, fortified 1.6, delay 1.2, withdrawal 1.15 | `ciano_1988_qjm` fig. 24 (Dupuy) | `03` §4 |

### 2.3 Losses, breakpoints and rates of advance

| Norm | Used | Source | Where |
|---|---|---|---|
| Daily personnel losses by action (division in contact) | 1.0-6.6 % a day: attack of a position 4.1 then 2.2, defence of a position 2.2 then 1.3, delay 1.0, inactive 1.0 | `fm_101_10_1_1976` table 5-8, p. 5-8 | `03` §4, `06` §2 |
| Tank losses by commitment | day 1: covering force heavily committed 73 %, MBA heavy 54 %; up to 80 % reparable | `fm_101_10_1_1976` table 7-7, p. 7-16 | `03` §4 |
| WWII daily losses (calibration band) | army 0.6-1.2 %, division 3-6 %, regiment 10-16 % | `mt_novoselov_replacement` p. 4 | `06` §2 |
| Combat effectiveness (the Pact, our side) | over 2/3 full; 1/2-2/3 degraded and replaced first; under 1/2 (or about 25 % with command and artillery hit) ineffective | `mt_stashek_losses` p. 7; `mt_novoselov_replacement` p. 9 | `03` §5 |
| Breakpoints (NATO) | ranges by mission: an attack stops at 15-35 % cumulative losses, a defence breaks at 25-50 % (SAM3D estimate within the folklore's 20-30 % and Clark's ranges) | `clark_1954_oro`; `hero_1982_termination` | `03` §5 |
| Advance rate by opposition (analysis) | 2.8 km/day intense, 10.1 moderate, 23.3 slight, 54.8 negligible, about 70 unopposed | `hero_1972_oralfore` p. IV-2 | `03` §4 |
| The Pact's rates | 25-30 km/day breaking through, 40-50 in depth (50-70 against a broken enemy) | `mt_kurkov_tank_army` p. 9; `fm_100_2_1_1984` p. 4-4 | `03` §4 |
| NATO's exploitation | 50-80 km a day for corps and divisions | `fm_100_5_1982` p. 8-7 | `03` §4 |
| No sustained advance below a threshold ratio | HERO's conclusion; the threshold is a SAM3D game value | `hero_1972_oralfore` p. IV-2 | `03` §4 |

### 2.4 The march and dispersal

| Norm | Used | Source | Where |
|---|---|---|---|
| Columns | day 20-30, night 15-20, cross-country 5-15 km/h; 25-50 m between vehicles, up to 100 m under air attack | `fm_100_2_1_1984` pp. 5-2, 5-5 | `03` §7 |
| Intervals on a route | battalions 3-5 km, regiments 5-10 km | `fm_100_2_1_1984` p. 5-2 | `03` §7 |
| NATO's columns | open column 50-100 m by day, close 25 m at night; infiltration against air attack | `fm_71_2_1977` p. H-4 | `03` §7 |
| Against a large raid | the column leaves the road | `fm_100_2_1_1984` p. 5-5 | `03` §7 |

### 2.5 The air front

| Norm | Used | Source | Where |
|---|---|---|---|
| Default | at the border; after heavy losses an air force kept 16-24 km back from the belt | `werrell_2005` p. 151 | `02` §2 |
| What opens it | believed working radar SAM fire units over a strip at least 20 km wide down to 30 % of their pre-war number (SAM3D estimate bounded by the Suez hole and the Bekaa); for the Pact, Krasovskiy's 60-80 % of the SAM fire system neutralised | `brant_1986_bai_1973` ch. 4; `werrell_2005` pp. 153, 155; `mt_1965_krasovskiy_conventional` p. 8 | `02` §3 |
| How fast | under 24 h from the first sites destroyed to full close support | `werrell_2005` p. 153 | `02` §3 |
| What pulls it back | a loss rate over 1-3 days: about 4 % a sortie at Suez against a normal 1 % | `werrell_2005` pp. 151, 154; `jvl_1973_losses` | `02` §3 |
| A crisis overrides | the IAF flew a maximum effort at the Golan through about 80 losses in 19 days | `entwistle_1988` ch. 4-5; `brant_1986_bai_1973` ch. 4 | `02` §3 |
| Helicopters | only under their own side's air defence umbrella or local air superiority; a step behind the fixed-wing front; airmobile raids 20-30 km beyond the FEBA | `fm_100_2_1_1984` pp. 7-1, 12-7; `fm_17_50_1977` p. 1-1; `barbeau_1985_sams` pp. 14, 23-24 | `02` §3 |
| Helicopters against an intact defence | 20-35 of about 50 Mi-8s lost on 6 Oct 1973 | `werrell_2005` p. 153 | `02` §3, `06` §2 |
| The neutral | its own territory; no "untimely attrition" | `sadauer_schlup_1978` pp. 54, 88-90 | `02` §3 |
| A small air force | surges forward for days where the enemy's troops have no SAM cover, then cuts back | `cia_1985_angola_offensive`; `cia_1986_angola_saaf` | `02` §3 |

### 2.6 Air defence redeployed and rebuilt

| Norm | Used | Source | Where |
|---|---|---|---|
| Long- and medium-range SAM units | move whole, 2-3 times a day at most at 80-100 km/day; an army 2 moves a day, 3 h at most | `mt_rozhdestvenskiy_depth` pp. 6-7 | `04` §3 |
| A fixed SAM battalion's move (S-75 class) | 30-40 km in 10-15 h at night: 2.5 h to pack, 3 h to set up, 2-2.5 h of checks | `mt_1973_nvn` pp. 10-11 | `04` §3 |
| Mobile batteries | leapfrog; at least one battery stays in its firing position | `fm_100_2_1_1984` p. 11-9; `andersen_1979` p. 45 | `04` §3 |
| The Osa battery in defence | 3-5 km behind the forward edge; pairs up to 3 km apart; 1.5-3 km from covered troops; moves only on the SAM regiment commander's order | `voenizdat_osa_1991` pp. 234-238 | `04` §4 |
| The gap while SAMs move | covered by organic air defence and fighters | `mt_lavrinovich_mdex` pp. 16-17; `andersen_1979` p. 45 | `04` §3 |
| The main thrust's share | 60 % of a front's air defence to the army of the main thrust (1944) | `andersen_1979` p. 40 | `04` §2 |
| Who decides the sky's sharing | the PVOS division commander or the front's air force and air defence commander; fighters into a SAM zone only by the PVOS formation commander's decision | `vr_krupicka_1984` pp. 63-64 | `04` §5 |
| The national PVO | object and object-zonal defence by importance; it covers our troops on home ground | `vr_lhotsky_1985` p. 38; `vr_popelka_1983` p. 53 | `04` §2 |
| Unopposed rebuild | 15-25 to 58-61 operational sites in about 8 weeks, mostly into prepared sites | `frus_1969_76_v23_d179` p. 615 | `04` §6 |
| New mobile units into a lost area | about 3 weeks (new SA-6 units in the Bekaa, July 1982) | `cordesman_1982`; `werrell_2005` p. 155 | `04` §6 |
| Prepared positions per unit | about two | `npic_1973_sam_egypt` | `04` §3 |
| Air defence losses in an operation | 25-30 % irrecoverable | `mt_rozhdestvenskiy_depth` p. 7 | `06` §2 |

## 3. Where sources disagree, and which was used

| Topic | One says | Another says | Used |
|---|---|---|---|
| Our division's frontage | ČSLA 1985: 30-45 by 20-25 km (`vr_poltikovic_1984`) | NATO: 20-30 by 15-20 km (`fm_100_2_1_1984` p. 6-5); the old Vševojsk: up to 30 by 20 | ČSLA 1985, the operator's own regulation of the war's year |
| Our security zone and forward position | ČSLA 1985: zone 20-40 km, forward position 6-8 km | NATO: 15 km at division, 30 at army; Vševojsk: 15 km and 3-5 km | ČSLA 1985 |
| The Pact's division frontage | FM: 15-25 km, breakthrough 6-10 | Kurkov: about 10 km at the breakthrough for a tank division; Reznichenko's 20-30 km is NATO's | FM's 15-25 for the zone, 6-10 for the breakthrough |
| The Pact's rate in depth | FM 50 km/day | Kurkov 40-50; Volkov 50-70 | 25-30 breaking through, 40-50 in depth, 50-70 only against a broken enemy |
| Force ratios | FM 100-5 (1976): 3:1 and 6:1 | FM 100-5 (1982) drops numbers; Dupuy: a prepared defence is worth 1.5 times | the 1976 ratios as the commanders' decision rule; Dupuy's factors in the combat result (they answer different questions) |
| Combat effectiveness | Stashek: effective under 40-50 % losses | Novoselov: effective status kept at 2/3 of strength | both: 2/3 the line below which a unit is degraded and replaced first, 1/2 the line below which it is ineffective |
| A fixed breakpoint | officers' folklore 20-30 % | Clark: "not supported by combat data"; HERO: battles end on the mission | ranges by mission, drawn per unit |
| Where the FSCL goes | FM 6-20 (1977): as close as possible (3 km in its example) | OTA on FOFA: about 30 km | 20-30 km in defence; closer in a fast attack |
| Helicopters' reach | FM 100-2-1: only under own air defence | Reznichenko: the whole tactical depth | FM's rule near the line; raids into the tactical depth only through corridors SEAD made |
| Aircraft held back for nuclear use | Krasovskiy: up to 30 % | Volkov: about 50 % of fighter-bombers | not modelled: the conventional war of 1985 keeps the faction files' sortie rates (noted) |
| How SAM units leapfrog | Osa: by pairs, one crew of each pair covering | "foreign specialists": battery by battery; Soviet long and medium range: whole units | the Osa by pairs; S-75, S-125 and Kub whole units, the gap covered by organic air defence and fighters |
| The 1970 standstill's date | Werrell: 8 July 1970 | FRUS: 7 August 1970 | FRUS |
| Bekaa batteries destroyed | Werrell 17 + 11; Brant 23; Lambeth 17 of 19 | Werrell's range 19-36 | Werrell's day-by-day figures |
| The SA-8's first Bekaa success | 24 July 1982 (`wapo_1982`) | August 1982 (`lambeth_1984`) | late July 1982 |

## 4. SAM3D estimates the design needs

No period source gives these numbers. Each is labelled as an estimate in the data and checked in F6 against the
cases that bound it.

| Estimate | Value | Bounded by |
|---|---|---|
| NATO's own frontages | defence: division 25-40 km, brigade 8-15, battalion 3-6; attack: division 10-20 | `fm_100_5_1976` p. 5-3 (6-8 battalions on one-fifth of a division's front) |
| The air front's step and its gates | steps of 20 km; open at 30 % believed working over a 20 km strip; pull back over 3 % a sortie (NATO 2 %, the Pact 4 %, the rebels and the neutral 1-2 %) | the Suez hole, the Bekaa, the 1973 loss rates (`air_ground_cases.md` §7) |
| NATO's breakpoints | attack stops at 15-35 %, defence breaks at 25-50 % | `clark_1954_oro`; `hero_1982_termination` |
| Weights of strength by equipment, 1985 | per type of tank, IFV, gun and launcher | Perventsev and Sokolov's method (`mt_perventsev_balance`), not their 1968 numbers |
| The air's share of ground losses | a band to check, not a rule: about 5-25 % of a sector's losses | aircraft "did not unequivocally damage or destroy one tank in the first three days" of 1973 (`werrell_2005` p. 154); the Bekaa's close support after air superiority (`nwc_83_027` pp. 20-21) |
| Restoring coverage after losses | a third of the lost fire units back per 2-3 weeks when unopposed; refills overnight are useless under daily SEAD | `frus_1969_76_v23_d179`; `lambeth_1984` p. 8 |
| The rebels' and the neutral's ground war | see `docs/fronts/03_GROUND_WAR.md` §8 | `cia_1986_nicaragua_af`; `sadauer_schlup_1978` |

## 5. Open items the design works around

None blocks the design. Each topic file lists its own; these are the ones that would change a number in `docs/fronts/`.
- A loss fraction at which a Pact attack is called off (pact_ground 6.3). The design commits the second echelon
  where the first is stopped, as the doctrine says, and stops on the effectiveness lines.
- A Soviet norm for when frontal aviation may work over the forward edge (air_ground_cases §9). Krasovskiy's
  penetration rule stands in.
- S-75 and S-125 march and deploy times from an operator's text (our_side 6.1). The Vietnam-era move from
  `mt_1973_nvn` stands in.
- A written priority table between national objects and our troops (our_side 6.3). The design keeps the national
  PVO's object list and gives our troops the share the main thrust gets.
- Hungarian and East German plans for defending their own territory (our_side 6.2).
- US unit frontages for 1985 (nato_ground §6): estimates until a CGSC student text is found.
- Swedish or Finnish neutral doctrine in a tier 1-2 text (air_ground_cases §9).

## 6. Post-period material kept out

Glantz and the Voroshilov lectures (1989-92); FM 100-2-3 (1991) for organisation only, as registered; FM 100-5 of 1986;
FM 1-112 (1997); Helmbold (1990) only for the shape of advance rates; Jauron (1993) only as a pointer to FM 6-20;
Afghanistan after the Stinger; the Gulf War of 1991; Kosovo 1999; the ČSLA's defensive doctrine of 1987-89. Each
topic file's section on post-period items gives the details.
