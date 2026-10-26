# Air war missions, weapons, support, forces and logistics: research (23 October 2026)

The research behind the air war's weapons, attacks, support aircraft, force build-ups, our supply lines and the
benchmarks against history (`docs/air_war/10_WEAPONS.md` to `14_LOGISTICS.md`, `docs/air_war/09_BALANCE.md` §9-10,
`simcore/examples/air_war_bench.cpp`). It extends `ENEMY_AIR_RESEARCH_2026-10.md` (the doctrines, staffs and planning)
with how the period's air forces actually flew and what their attacks achieved. Sources are registered in
`docs/SOURCES.md` ("Weapons, attacks, support, forces and logistics").

**What is researched and what is not.** What a weapon *is* (its mass, guidance, submunitions, range, the year it entered
service, how it was delivered) comes from encyclopaedic pages. What it *does* to a target is never taken from weapon
effects or lethality data: the model's footprints and effects are game values, calibrated until whole attacks and
campaigns come out as history recorded them (a bridge that stood through hundreds of sorties, a runway closed by a
flight of four, a SAM belt's toll). The calibration is the research here; the numbers are the model's.

## 1. Sources read

| Source | Tier | What it gave |
|---|---|---|
| `fm100_2_3` (FM 100-2-3, 1991, local copy) | 1 (what NATO believed) | The air forces of a Soviet front (two or three divisions; Western Group of Forces over 700 combat aircraft and 350 attack helicopters, p. 4-127); a fighter-bomber regiment of 45 aircraft in three squadrons (p. 4-134); the division's materiel support battalion: an ammunition company of 60 Ural-375s with trailers, a fuel company of 80 fuel trucks of 5,200 l (p. 4-90); the railroad troops between the front and the interior (p. 1-3) |
| `gwaps_vol3` (Gulf War Air Power Survey Vol. III, 1993) | 1 | Tankers: in Desert Storm 215 KC-135 and 35 KC-10 sorties a day refuelling 839 and 222 aircraft (Table 16), about 4.2 receivers a tanker sortie; Phase I's squadrons in place within four weeks, the whole force in five and a half months (pp. 298-299); the 1991 munitions (Table 18) |
| `rjlee_ds_losses` (from GWAPS Vol. V) | 4 | Desert Storm's fixed-wing combat losses by cause: guns 11, IR SAMs 12 and 2 SA-13, radar SAMs 8 and 2 SA-2, a fighter 1; 18 in the low-level first week, 22 in the five weeks at medium altitude after |
| `rand_mr1365` (Lambeth, *NATO's Air War for Kosovo*, 2001) | 1 | 430 allied aircraft at the start, nearly 1,000 committed; about 400 sorties the first night, 120 of them strikes; over 800 SAMs fired for two aircraft lost; at least 743 HARMs for 3 of some 25 SA-6 batteries confirmed destroyed; radars that radiated 20 s and shut down; the 15,000 ft floor kept against guns and MANPADS |
| `afmag_2003_bombs`, `wiki_lgb`, `wiki_thanh_hoa` | 4 | 1972-73: 48 % direct hits for laser-guided bombs against 5.5 % unguided, average misses of 23 ft and 447 ft; the Paveway I's 20 ft combat CEP; the Thanh Hoa bridge's 871 sorties and 11 aircraft lost without dropping it, then a span down to 24 laser-guided bombs from 14 F-4s (13 May 1972), after a strike that damaged it on 27 April |
| `wiki_focus` | 4 | Operation Focus, 5 June 1967: 196 combat aircraft, flights of four with runway-piercing bombs over some ten airfields, about 200 aircraft destroyed on the ground |
| `jvl_1973_losses`, `wiki_model5` | 4 | Yom Kippur: 50 Israeli aircraft lost in about 1,220 sorties in the first three days (4 %); Dugman 5 (7 October): six F-4s lost for one SAM battery |
| `bekaa_popular` | 4 | Mole Cricket 19 (9 June 1982): decoys drew the radars on, ARMs and bombs followed, 17 of 19 batteries destroyed, none of the aircraft lost to them |
| `navalgazing_falklands` | 4 | Argentine bombs dropped too low to arm: 25 hit ships, 11 went off |
| `saaf_toss`, `mig27_book` | 2, 4 | The toss: in at 100 ft, pull-up 4-5 nm out, the release climbing; the MiG-27's level, offset, dive and toss (to 45°) deliveries |
| `afmag_1984_nato`, `wiki_support_aircraft` | 4 | NATO's AEW force of 18 E-3As from February 1982, an E-3's coverage of 312,000 km²; the KC-135, the RC-135, the A-50 (in service 1984-85), the Il-20M |
| `wiki_fofa` | 4 | NATO's follow-on forces attack (adopted November 1984): the second echelon's columns, trains, bridges and chokepoints out to some 150 km behind the line; OTA's 1987 study |
| `croatia_1991_migflug` | 4 | An improvised air arm's growth: An-2s and crop sprayers in October 1991, a founded service by December |
| `wiki_weapons` | 4 | Each weapon's facts (the Mk 80 series, cluster and runway weapons, Paveway, AGM-65, the Soviet FAB, OFAB, RBK, BetAB, KAB-500L, Kh-25, Kh-29, rockets, guns) |
| `cia_1985_air_op`, `cia_1972_wp_air`, `mai_tactics_vvs` | 1 | (already registered) The Pact's air operation in massed blows; the tactical groups of a strike (strike, suppression, cover, reconnaissance and marking); the free hunt and the strike on call |

Official US and NATO documents on FAS, DTIC and media.defense.gov were not reachable from this session (blocked or served
as downloads); GWAPS Vol. V is read through `rjlee_ds_losses` and stays a research item in the original.

## 2. A library of the period's missions

What each side flew, as the model now has it (`data/air_war/faction_*.json` "packages", `docs/air_war/13_PACKAGES.md`).

| Mission | NATO (1980s) | Warsaw Pact (1980s) | In the model |
|---|---|---|---|
| Counter-air on airfields | Tornados laying JP233 and F-111s with Durandal and laser-guided bombs at night, low; later medium altitude | The air operation's opening massed blows: regiments of Su-24s and MiG-27s with BetAB and FAB, in waves (`cia_1985_air_op`) | `strike` on `airfield` (runway class): JP233 laydown, BetAB in dives, retarded FAB-500ShP laid down; the Pact sends 10-11 attackers |
| Suppression and destruction of air defences | F-4G Wild Weasels with HARMs and Shrikes in hunter-killer teams with F-4Es, the EF-111A standing off, decoys ahead (`afmag_1989_slam`, `jsead_1982`) | Kh-58 and Kh-25MP carriers, the suppression group of a strike, jamming aircraft | `dead`, `hunter`, `deception` packages; SEAD flights with every strike that meets a known radar |
| Interdiction of fixed targets | Bridges, rail junctions, depots, command posts by F-111, Tornado, F-16, F-4 | Bridges, junctions, depots, command posts by Su-24, MiG-27, Su-17 | `strike`; weapons chosen by the target's class (`10_WEAPONS.md`) |
| Armed reconnaissance | FOFA: A-10s and F-16s searching roads and railways behind the line | The free hunt (свободная охота): Su-25, MiG-27, Su-17 pairs | `armed_recce` on our supply lines (`14_LOGISTICS.md`) |
| Close air support | A-10s and AH-1s on call at the line | Su-25s and Mi-24s, the strike on call | `cas`, `helo_raid` |
| Reconnaissance and damage assessment | RF-4C, Tornado, the RC-135 standing off | MiG-25RB, Su-17M3 with pods, the Il-20M | `recce`, `bda`, the ELINT orbit (`11_SUPPORT.md`) |
| Cruise missiles | Tomahawk from 1986, CALCM from 1988 | Kh-22M from the long-range aviation | `missile` (built before this pass) |
| Support | E-3A orbits, KC-135 tankers, RC-135 | A-50 (from 1984-85), Il-20M | support orbits by the day (`11_SUPPORT.md`) |
| A light air arm's raids | (none) | (none) | the rebels' night harassment by crop sprayers and the neutral's pairs of light jets along the line |

**Tactics and attack profiles.** Low-level penetration under the radar with terrain masking, the attack itself by
laydown (over the target, for runway and cluster weapons and retarded bombs), pop-up (a pull-up some 6 km out to an
apex near 1,200 m and a dive onto the target), toss (a pull-up 4-5 nm out, the release climbing, the turn away short of
the target's guns), dive from medium altitude (the roll-in a few km out), level bombing from medium altitude above the
guns, and stand-off release of guided missiles (Maverick, Kh-29L) or glide bombs (GBU-15); helicopters pop up from a
hide and fire from the hover. Desert Storm's switch from low to medium altitude after its first week (`rjlee_ds_losses`)
and Allied Force's 15,000 ft floor (`rand_mr1365`) are the period's lesson on what the target's own guns cost.

**Exercises.** The period's air forces trained these packages in large exercises (NATO's in Central Europe and the
United States, the Pact's in its western military districts); this pass took no numbers from exercises, only from
operations, whose results are recorded. Exercise data on simulated kills stays a research item.

## 3. Benchmarks against history

`air_war_bench` flies each analogue through the abstract resolver with the theatre's own defence values and prints the
model beside the record. After calibration (23 October 2026, 400 runs each):

| Benchmark | The model | History |
|---|---|---|
| A massive bridge, F-4Es with Mk 84s from medium altitude, up to 871 sorties | a span down in 29 % of campaigns (after some 470 sorties when it fell) | the Thanh Hoa bridge stood through 871 sorties |
| The same bridge, one strike of 24 GBU-10s | a span down in 48 % of strikes | damaged on 27 April 1972, dropped on 13 May |
| Average miss, unguided from medium altitude and laser-guided | 472 ft and 21 ft | 447 ft and 23 ft |
| An airfield at night, low (JP233) against medium altitude (Mk 84), losses a 100 sorties | 6.2 against 0.4 | about 4 times a sortie in Desert Storm |
| An airfield's runways cut: two Tornados with JP233; four Su-24s with BetAB in dives; four F-16s with Mk 84s from medium altitude | 54 %; 96 %; 5 % | Focus's flights of four closed runways; medium-altitude bombing cratered few |
| ARMs for each radar destroyed: radiating crews against crews that switch off | 3.6 against 11.7 | the Bekaa against Allied Force |
| Unretarded bombs from low level that go off | 45 % | the Falklands, 44 % |
| Four F-4Es popping up behind four Kub batteries and guns, no suppression | 25 lost a 100 sorties | 4 a 100 over Yom Kippur's first three days, most sorties not into a belt; Dugman 5 lost six aircraft |
| A full suppression package against a radiating Kub battery | destroyed in 66 %, 4.5 lost a 100 sorties | the Bekaa: 17 of 19 batteries, none lost to them |

Where the model and the record still differ: the direct-hit share of unguided bombs (the model's miss has no tail of gross
errors), the low-to-medium ratio (this airfield's guns are thicker than Desert Storm's average target), and the belt
without suppression (worse than the average of the first three days, as it should be, and in Dugman 5's range). The
calibration changed five things, each in `data/gameplay/air_war.json` with its reason: the pass chance's scale (0.5),
SEAD's suppression (0.6), decoys exposing the radars that fire at them (0.6), an ARM with memory against a radar that
switched off (0.08), and a runway cut across its width (25 m) and blast reaching further against soft and light targets.

## 4. Support aircraft

NATO's E-3As kept AEW orbits over Central Europe from 1982 (`afmag_1984_nato`); the model counts three sorties of eight
hours for an orbit round the clock (a game value). Desert Storm's tankers refuelled about 4.2 receivers a sortie (`gwaps_vol3`). The RC-135 stood off
the border listening; the Pact's A-50 came into service in 1984-85 and its Il-20M flew ELINT. In the model an AWACS on
station cuts the defender's fighter threat by half, tankers extend a strike's radius by 1.6 times, and an ELINT aircraft
widens and sharpens the enemy's ELINT picture (`11_SUPPORT.md`).

## 5. Force build-ups

| Faction | The period | The model (`logistics.buildup_*`) |
|---|---|---|
| NATO | Reinforcement from the United States over weeks (Desert Shield's first squadrons in four weeks, the whole force in five and a half months; Kosovo from 430 to nearly 1,000 aircraft in weeks) | 45 % on day 1, all by day 28 |
| Warsaw Pact | The front air forces in place, the second echelon and the long-range aviation within days (`fm100_2_3`) | 70 % on day 1, all by day 7 |
| Neutral | Its air force at home | all from day 1 |
| Rebels | An improvised wing that grows as aircraft are found, captured or defect (`croatia_1991_migflug`) | 80 % on day 1, all by day 30 |

## 6. Logistics

A Soviet-model division is fed by its materiel support battalion's trucks (60 ammunition, 80 fuel: `fm100_2_3`) from
the army's and the front's depots, which the railways fill from the interior. NATO's follow-on forces attack aimed at
exactly this: columns on the march, trains, bridges and chokepoints behind the line (`wiki_fofa`); the Soviet air forces
called the same search the free hunt. Our supply lines in the model are columns of 18-22 trucks, trains of 32-36 wagons
and an armoured march column, each going round from depot to troops; the troops hold three days' stocks and lose
strength when they run out (`14_LOGISTICS.md`).

## 7. Open

- Exercise records (simulated kills and losses) as a second check on the calibration.
- GWAPS Vol. V and the US Army's FM 100-2-1 (march rates, column lengths) in the original.
- The speed of a column by road class and by night, the effect of air attack on march discipline.
- The rebuild of a bridge by engineers (pontoons from the pontoon park) as its own logistics.
