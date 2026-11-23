# Our side on the ground, 1985: the defence, its air defence, and the national PVO (research, November 2026)

Why: track F, "the fronts". The game will simulate the ground war around the battery and our national air defence
strategy. This note answers one question: **how a small Pact-trained national army defended its ground in 1985, how
its troop air defence moved with it, and how its national air defence (PVO) set and changed priorities as the front
moved.** Pannonia's army is modelled on the Czechoslovak People's Army (ČSLA) and the Hungarian People's Army, so ČSLA
texts are the first choice where Soviet texts are silent or general.

This extends `docs/research/air_battle_2026-11/our_side_air.md` and does not repeat it. That note already has: fighter and
SAM cooperation by zones or in one zone (`mitronin_1976`), the army air defence zone and first-echelon SAM positions
behind the front (`mt_turchin_coop`), IFF, flight notices, fratricide, and the national PVO leading at the start of a
war (`mt_rozhdestvenskiy_depth` pp. 8-10). Here those are referred to, not restated.

Period rule: the war is 1985. Every source below is dated. A text written after 1985 is marked post-period and used only
as hindsight about structures that already existed. Read 17-19 November 2026.

Whose view: a Soviet or Pact document (in the original, or in CIA or JPRS translation) is the Pact's own view, tier 1.
US FM 100-2-1 is "what NATO believed", tier 1 as a document but not as a Pact norm. ČSLA journal articles in
*Vojenské rozhledy* (VR) and its *Vojenská mysl* section are the Czechoslovak army's own view, tier 1.

Limits of this pass:
- No Hungarian, Polish (*Myśl Wojskowa*) or East German (*Militärwesen*, NVA handbooks) text was read. The Czechoslovak
  journal was searched instead, because its archive is open and full text.
- No 1980s Soviet combat regulations (BUSV) for the regiment or division in defence were read. Reznichenko 1984 and the
  ČSLA preview of the new regulations (Poltikovič 1984) stand in for them.
- Page numbers are the printed pages of each text. For the Osa textbook the printed page is the PDF page minus one;
  both are given.

## 1. Summary for design

1. **Defence frontages and depths have a written norm, and it changed on 1 January 1985.** The new ČSLA Combat
   Regulations, part 1, copied from the Soviet ones, give a division 30-45 km by 20-25 km, a regiment 10-15 km by up to
   12 km, a battalion up to 5 km by up to 3 km (`vr_poltikovic_1984` pp. 95-96). The old Vševojsk-1-1 gave up to 30 by 20,
   up to 10 by 10, up to 5 by 2. NATO thought 20-30 by 15-20 for a division (`fm_100_2_1_1984` p. 6-5). Use the ČSLA 1985
   numbers. Wider frontages are allowed when defending the state border.
2. **The security zone is 20-40 km deep, fought as a delay by line.** Positions 6-8 km apart. Forward detachments are
   reinforced battalions, sometimes a whole second-echelon regiment. A reinforced battalion delays a brigade 1-2 hours.
   The army commander sets the zone's depth; the division commander runs the fight and orders each withdrawal
   (`vr_krizek_1985` pp. 32-36). Where there is no security zone, a forward position 6-8 km out
   (`vr_poltikovic_1984` p. 97).
3. **Nobody withdraws without the senior commander's order.** Reznichenko says it twice (`reznichenko_1984` pp. 49, 152).
   Units must be ready to fight encircled. The Osa battery changes its start position only on the SAM regiment
   commander's order (`voenizdat_osa_1991` p. 238). This is the "hold" rule for the game: every unit holds until an order
   reaches it through the information pipeline.
4. **The counterattack is the second echelon's job, ordered from above, from the march, into the flank.** Tanks are its
   core. It goes on until the penetration is gone or a set line is reached (`reznichenko_1984` pp. 177-179). Each
   counterattack has main and reserve deployment lines (`vr_poltikovic_1984` p. 96). The counterattack force is a top air
   defence priority (`andersen_1979` p. 53; `reznichenko_1984` p. 179).
5. **The Osa battery sits 3-5 km behind the forward edge in defence and moves by pairs.** Pairs up to 3 km apart; one
   crew of each pair always covers (`voenizdat_osa_1991` pp. 234-237). This is the leapfrog rule for the game.
6. **Long- and medium-range SAM units move whole, not by subunits, two or three times a day at 80-100 km/day.** An army
   makes no more than two moves a day and spends no more than 3 hours on them (`mt_rozhdestvenskiy_depth` pp. 6-7). While
   SAMs move, the divisions' own air defence (firing from the march) and fighters cover the gap (`mt_lavrinovich_mdex`
   p. 17; `andersen_1979` p. 45).
7. **On home ground the national PVO is in charge.** It covers our troops' mobilisation areas, marches and crossings;
   the troops' own air defence complements it (`vr_popelka_1983` p. 53). The PVOS division commander (or the front's air
   force and air defence commander) decides how the sky is shared (`vr_krupicka_1984` p. 63).
8. **National PVO defends areas, places and objects ranked by importance.** It is object or object-zonal defence
   (`vr_lhotsky_1985` p. 38). In 1956 the ČSLA moved assets from less important objects to the important ones. No written
   1980s ranking of objects against troops was found.
9. **Priorities shifted toward the main thrust in the Great Patriotic War, and 1980s texts still cite it.** The 1st
   Baltic Front gave 60% of its air defence to the army making the main thrust (`andersen_1979` p. 40).
10. **The small ally's own plan: cover the borders first, then counterattack.** Czechoslovak war plans of 1964, 1977 and
    1986 all began with covering the border (`polnar_2022` pp. 147-148). In 1987-89 the Pact turned defensive
    (post-period). Hungarian and East German plans were not found.

What SAM3D should build (track F):
- A defence posture per division and regiment from the 1985 norms: frontage, depth, echelons, positions, CP distances.
- A security zone as a sequence of lines with a holding time per line, withdrawn line by line on the division
  commander's order, carried by the pipeline.
- A "hold" flag on every unit: it moves back only on an order that reached it. The order's author is fixed per level.
- A counterattack trigger owned by the senior commander, using the second echelon from the march, with air defence cover
  ordered with it.
- Troop air defence moving by the Osa pair rule; long- and medium-range units moving whole, with a gap that organic air
  defence and fighters fill.
- A national PVO priority list per phase (mobilisation, march, crossing, defence), set by the PVO formation commander and
  passed down as orders, not read from the truth.

## 2. Findings

### 2.1 The division and the regiment in defence

**front-01. Frontage and depth, ČSLA 1985 against the old manual.** Poltikovič previews the new Combat Regulations of the
Ground Forces, part 1, and compares them with Vševojsk-1-1 (`vr_poltikovic_1984` pp. 95-96):

| Level | Frontage, new | Frontage, Vševojsk-1-1 | Depth, new | Depth, Vševojsk-1-1 |
|---|---|---|---|---|
| Division | 30-45 km | up to 30 km | 20-25 km | up to 20 km |
| Regiment | 10-15 km | up to 10 km | up to 12 km | up to 10 km |
| Battalion | up to 5 km | up to 5 km | up to 3 km | up to 2 km |

Frontages may be larger when defending the state border (p. 95). The regulations came into force on 1 January 1985 and
were used at the Shield-84 exercise. The Soviet regulations were taken over almost wholesale (pp. 100-101).
*Design:* these are the 1985 norms. A Pannonian division defending the border can stretch past 45 km.

**front-02. What NATO thought.** FM 100-2-1 gives a division 20-30 km wide and 15-20 km deep, two echelons and a reserve,
at least two regiments forward; a regiment 10-15 km by 7-10 km, its reserve a tank-heavy company; a battalion 3-5 km by
up to 2 km, up to 2 km between its echelons; a company 500-1000 m by 500 m (`fm_100_2_1_1984` pp. 6-5 to 6-7). These match
the old Vševojsk numbers more than the 1985 ones. Front and army second echelons and reserves may be half the force
(p. 6-1).

**front-03. Echelons.** Defence is in one or two echelons, or one echelon plus a combined-arms reserve
(`vr_poltikovic_1984` p. 97). The two-echelon formation comes from the Second World War. A battalion usually has one
company in its second echelon; on adverse terrain one echelon. The battalion reserve is a reinforced platoon. Second-echelon
battalions prepare for wide counterattack manoeuvres and against airborne landings (`reznichenko_1984` pp. 155-156).
The second-echelon division prepares a counterstrike on the main axis. The second-echelon regiment prepares a counterattack
or the relief of first-echelon units (`vr_poltikovic_1984` p. 96).

**front-04. Positions.** The division prepares 3-4 defensive positions, the regiment 2. A second-echelon division 2-3,
a second-echelon regiment 1-2 (`vr_poltikovic_1984` p. 96). There is no continuous front line; units must be ready to
fight encircled and without contact with neighbours (`reznichenko_1984` p. 152).

**front-05. Command posts.** Division main CP 8-10 km from the forward edge (Vševojsk up to 15), rear CP up to 30 km
(20-30). Regiment main CP 4-6 km, rear CP up to 15 km (15-20). Reserve areas are prepared for CPs
(`vr_poltikovic_1984` pp. 99-100). NATO thought a division had four CPs, the forward one as close as 3 km
(`fm_100_2_1_1984` p. 6-5). An army CP is about 2.5 by 2.5 km and moves once per 24 hours; the rear CP 2 by 2 km, also once
per 24 hours; the three CP elements up to 20 km apart (`vr_cerny_horak_1985` p. 57).
*Design:* CP positions and move intervals feed the enemy's target list and the pipeline's delays.

**front-06. Fortified areas.** On the border, permanent works 4-6 km apart; the forward position 4-6 km out; a battalion
area 8-12 km by 4 km; a company 3-4 km by 2 km. The fortified area's commander becomes the deputy division commander
(`vr_poltikovic_1984` p. 100).

### 2.2 The security zone and delaying action

**front-07. Depth and who sets it.** The security zone is 20-40 km deep (Vševojsk: 15 km) (`vr_poltikovic_1984` p. 96;
`vr_krizek_1985` p. 32). It sits at the lower end of the range on an independent axis (`vr_krizek_1985` p. 32). "The
fight in the security zone is run by the division commander. The depth of the zone ... is set by the army commander in
his decision" (p. 34, my translation). NATO thought up to 30 km at army level and 15 km at division (`fm_100_2_1_1984`
p. 6-2).

**front-08. Lines and forces.** Several positions 6-8 km apart. Forward detachments are reinforced motor rifle or tank
battalions, sometimes a whole second-echelon regiment, and in an army defensive operation sometimes a whole second-echelon
division (`vr_krizek_1985` p. 32). A detachment may cover up to twice the normal frontage. A battalion is reinforced with
up to a company, an AA platoon, an artillery battery and an engineer platoon (p. 33).

**front-09. What a detachment can do.** A reinforced battalion can repel up to two enemy battalions and delay a brigade
1-2 hours. To make an enemy division's main body deploy takes two reinforced battalions up to a regiment. Raid
detachments strike the enemy's columns (`vr_krizek_1985` p. 34).

**front-10. Withdrawal from the zone.** From each line on the division commander's order, usually after the senior
commander's fire strikes. The whole echelon withdraws together, never small units alone (`vr_krizek_1985` p. 35). The last
line is at the forward position's level. Detachments pass through the forward edge at set places into the division's
second echelon (p. 36). The order states the expected time to hold each line and how to prevent unauthorised withdrawal
("samovolný odchod") (pp. 36-37). NATO thought the zone withdraws under artillery cover when faced with encirclement
(`fm_100_2_1_1984` p. 6-2).

**front-11. History cited.** At Stalingrad in July 1942, forward detachments (a reinforced rifle regiment from each of
three divisions of the 62nd Army) 40-60 km out delayed the enemy 5-6 days. At Kursk on 4 July 1943, combat outposts held
17 hours against about a regiment, 50 tanks and 100 aircraft (`reznichenko_1984` pp. 170-171).

**front-12. Forward position and outposts.** The forward position is 6-8 km ahead (Vševojsk 3-5 km) where there is no
security zone; combat outposts up to 2 km ahead of first-echelon battalions (`vr_poltikovic_1984` p. 97). The outpost is
beyond enemy direct-fire and observation range of the forward edge; the forward position further out
(`reznichenko_1984` p. 158). Units leave the forward position as "determined by the situation" (p. 170).
*Design:* the security zone is a natural first mission phase for a battery: covering a detachment that will fall back.

### 2.3 The counterattack

**front-13. Who, when, how deep.** In the Second World War counterattacks were on a division and corps scale: the
division's second-echelon regiment and part of the first-echelon regiments (`reznichenko_1984` p. 177). The counterattack
goes into the flank and rear, from the march without halting on a deployment line; tanks are the core; it continues until
the penetration is eliminated or a designated line is reached. First-echelon units join only on the senior commander's
order. The counterattacking units are protected from the air by AA fire (pp. 178-179). If the defence is broken, units
hold their retained positions so the senior commander can counterattack (p. 177). NATO: a counterattack needs the
authority of the next higher commander; the tank regiment is the division's main counterattack force
(`fm_100_2_1_1984` pp. 6-3, 6-5).

**front-14. Deployment lines.** Several counterattack axes, each with main and reserve deployment lines. Tank and BMP
regiments get 1-2 firing lines (`vr_poltikovic_1984` p. 96).

**front-15. Counterstrike aims (Shield-84).** Counterattacks and counterstrikes are the highest form of an active defence.
A counterstrike may aim to weaken the enemy's strike group and stop it, or to restore the situation (the forward edge,
the state border), or at best to destroy the penetration and go over to the offensive. Attack helicopters were used more
widely against armour (`vr_svatos_1985` pp. 27-28).
*Design:* the counterattack is an order event from the senior commander; the battery gets its cover task with it.

### 2.4 Withdrawal and "hold"

**front-16. Who may order it.** Withdrawal "is carried out only with the permission of the senior commander (chief)"
(`reznichenko_1984` p. 49). Defending units "are not authorized to abandon their positions and withdraw without orders
from the senior commander (chief)" (p. 152). The Osa battery commander changes the start position "only on the order of
the SAM regiment commander" (`voenizdat_osa_1991` p. 238 (PDF 239)).

**front-17. How (NATO's reading).** A covering force (a reinforced platoon per forward company) stays; a rear guard (a
reinforced second-echelon battalion) holds; then the main body leaves, rear services first; minimum radio
(`fm_100_2_1_1984` pp. 6-10, 6-11).

**front-18. Mobile defence.** None of the Pact texts read treats "mobile defence" as a separate form. The defence is
positional, made active by the second echelon's manoeuvre, fire and counterattacks (`vr_svatos_1985` p. 27). This is an
open item, not a finding.

### 2.5 A small ally defending its own territory

**nat-01. Czechoslovak war plans.** Polnar (a later study by a Czech University of Defence historian, tier 4) summarises
the plans from the archives. 1964: from initial defence of the border to an offensive at 90-110 km per day. December 1977:
first cover the borders with the FRG and Austria, then attack. October 1986: after repelling the aggression, an
offensive reaching the French border in 15-16 days. July 1989: a defensive campaign, then a counterattack into the FRG
(`polnar_2022` pp. 147-149). Plans were made in Moscow (p. 146). In the 1930s, before the Pact, the plan was a fighting
retreat to successive lines (p. 145).
*Design:* in 1985 a Pact ally's army covers its border, then counterattacks. A fighting retreat is not its doctrine.

**nat-02. State border and fortified areas.** Wider frontages on the state border (`vr_poltikovic_1984` p. 95) and
permanent fortified areas (front-06). Restoring the state border is a written counterstrike aim (`vr_svatos_1985` p. 28).

### 2.6 Troop air defence (PVO SV) with the defence

**tad-01. Which asset covers what.** Air defence units cover the first and second echelons (reserve) in positions,
assembly areas and counterattacks, and the missile unit and artillery. SAM unit positions lie behind natural anti-tank
obstacles or under anti-tank cover; the regiment's own air defence is inside the regiments' formations; some units rove
or lie in ambush (`vr_poltikovic_1984` p. 97). Air defence is organised through the whole depth, main effort on the main
forces, fire coordinated with fighters by tasks, zones, heights and time (p. 99).

**tad-02. The Osa battery's defence tasks.** Cover the first or second echelon and its counterattacks, missile units,
artillery, CPs and the rear; sometimes the security zone, ambushes, roving (`voenizdat_osa_1991` p. 238 (PDF 239)).
The regiment has five batteries; a battery has a PU-12M, four 9A33BM3, three MANPADS launchers and two TZM. It fights
complete, sometimes in pairs or single vehicles (pp. 225-226 (PDF 226-227)).

**tad-03. The Osa battery's defence position.** Two lines or a line of pairs, up to 3 km apart. Start positions inside or
behind the battalion defence area, under anti-tank cover, 3-5 km from the forward edge. One or two reserve positions up to
2 km away. When covering troops in place, 1.5-3 km from their boundaries. Under nuclear threat at least 1,000 m between
launchers. The MANPADS section within 1,000 m of the battery CP (`voenizdat_osa_1991` pp. 235-236 (PDF 236-237)).

**tad-04. NATO's view of the division's air defence in defence.** ZSU with the first-echelon battalions; SA-9 at the rear
of the first-echelon battalions or forward in the second echelon, protecting artillery and CPs; the remaining divisional
batteries protect the division main CP, artillery and rockets; ammunition trucks 1.5-2.5 km behind the ZSU. A first-echelon
motor rifle regiment usually gets one or more divisional batteries (`fm_100_2_1_1984` pp. 11-3 to 11-6). An army has one
or two SA-4 brigades, reaching from the army rear to about 45 km beyond the forward edge; SA-6 and SA-8 several km behind
the line of contact (p. 11-2).

**tad-05. Priorities.** Each source ranks a little differently:
- Pact (Soviet): units going over to the defence or regrouping are the top mission; then artillery and the counterthrust
  groups; site air defence so it covers the counterattack force with only slight regrouping (`andersen_1979` pp. 52-53).
- ČSLA: first and second echelons, the missile unit, artillery (`vr_poltikovic_1984` p. 97).
- ČSLA army level: the army's short-range SAM regiment is fully effective only when it defends the whole missile brigade;
  three Strela-2M teams per CP, at least 500 m from the object; rear objects are covered by the national PVO and fighters
  (`vr_cerny_horak_1985` pp. 60-61). Up to 30% of enemy aviation in the army's zone may go against CPs, missile units,
  artillery and the rear (p. 58).
- NATO's reading: first-echelon regiments, division HQ, artillery and rockets highest (`fm_100_2_1_1984` p. 11-2).
*Design:* one priority list per posture. In defence: counterattack force and second echelon when it moves, then missile
unit and artillery, then CPs, then first-echelon battalions (organic AD covers those).

### 2.7 Troop air defence on the move

**tad-06. The Osa pair rule.** On the move and in the offence the battery stands in a line of pairs; continuity of cover
by at least one crew of each pair. Pairs up to 3 km apart; within a pair at least 200 m and up to 500 m. Launchers
1.5-3 km from the covered troops' forward line; TZM up to 500 m from the launcher. They move behind the companies or by
bounds, 1.5-3 km from the covered units (`voenizdat_osa_1991` pp. 234-235, 237 (PDF 235-236, 238)). In a withdrawal the
battery moves from line to line; positions on the approaches are 1.5-3 km from the covered troops (p. 238).
*Design:* this is the game's leapfrog: one vehicle of each pair stays in "ready" while the other moves.

**tad-07. Osa march.** Column order PU-12M, two launchers, TZM, two launchers, TZM, maintenance. 25-50 m between vehicles,
at least 200 m when air attack is expected; column 200-400 m, up to 700-800 m deep (p. 232 (PDF 233)). Control points
every 3-4 hours; halts up to 1 hour every 3-4 hours, up to 2 hours for a meal; marches at night; one or two launchers take
positions at halts; radio forbidden except to control fire. Speeds: road 60 km/h, dirt 30, cross-country 10, swimming 8
(pp. 239-240 (PDF 240-241)). River crossing: deploy 2-3 km from the water; swim across once a bridgehead 1.6-2 km deep is
held (p. 237 (PDF 238)).

**tad-08. Who goes where in a march (ČSLA).** Medium-range SAM formation by battalions on 2-3 routes ahead of the main
columns. Short-range regiment ahead of the first-echelon columns, or split by batteries. Close-range regiments split by
pairs inside the columns; regimental AA batteries by platoons; MANPADS by squads (`vr_popelka_1983` pp. 53-54). Part may
go ahead to the day-rest area (p. 53).

**tad-09. NATO's view of the march and crossings.** Two radars from the division AD regiment travel with the column, one
moving at a time (`fm_100_2_1_1984` p. 11-9). At a river: ZSU 300-500 m from the water, no more than 1,500 m apart
(p. 11-10). The weakness NATO saw: air defence failing to move forward at the pace of the troops (p. 11-11).
ČSLA: SAMs deploy on the near bank before a crossing, within 500 m of the crossings (`vr_fanfule_1984` p. 35).

**tad-10. Long- and medium-range units: whole moves.** "If the rate of advance is 80 to 100 kilometers per day, long- and
medium-range surface-to-air missile units should be moved two to three times per day, and by entire units, not by
subunits" (`mt_rozhdestvenskiy_depth` p. 6). An army with one brigade or two self-propelled regiments covers its main
grouping with no more than two moves a day and no more than three hours on them. Two regiments (kill radius 35-40 km)
cover 60 km of front by 100-120 km of depth; a low-altitude unit 25 km by up to 50 km (p. 7). From the march to combat
readiness in "five to 15 minutes" (p. 8).

**tad-11. Battery by battery (NATO's version, quoted by the Soviets).** "Foreign specialists believe" area SAM units
change position by order of the higher commander; "the move ... is conducted by battery; ... each subsequent battery should
begin to move from its old position after the previous battery is ready to fire". A gap is covered by a fighter manoeuvre
(`andersen_1979` p. 45). Three relocation methods: by phase lines as a unit; sequentially by small units, keeping the
defence unbroken; together with the defended forces in their formation (p. 46). The 24th AA Artillery Division changed
positions 34 times in two weeks in the Vistula-Oder operation, on some days 6 times (p. 45).

**tad-12. When SAMs move, the kill zones break up.** Gunners trying to keep up "frequently are not held at their firing
(launching) positions". As SAM positions change, cover is chiefly by the divisions' organic air defence firing from the
march, and by fighters; fighters get preference (`mt_lavrinovich_mdex` pp. 16-17).

**tad-13. Depth of cover ahead of the line.** NATO: SA-4 to about 45 km beyond the forward edge (`fm_100_2_1_1984` p. 11-2).
Soviet: continuous cover of a front to 500-600 km from the line of contact needs 3-5 long-range brigades, 9-14 medium-range
and 15-20 short-range regiments plus fighters (`mt_rozhdestvenskiy_depth` p. 5). For the Osa, cover ahead of the line is
its position depth (1.5-5 km) subtracted from its kill zone in `data/systems/` (derived, not a written norm).
`our_side_air.md` has Turchin's first-echelon SAM positions 10-20 km behind the front.

### 2.8 The national PVO

**pvo-01. How the national PVO is built.** The ČSLA's national air defence (PVOS) is run from the central CP through the
formations' (districts') CPs, by defended areas, places and objects arranged by importance. It is object or object-zonal
defence, over the whole territory, continuous along the enemy's whole flight (`vr_lhotsky_1985` p. 38). Since 1976 the
ground forces' modern air defence complexes have been put into the PVOS duty system (p. 37). In the 1970s mixed battle
formations were built and norms for missile stocks set (p. 39). In 1956 forces were moved from less important objects to
strengthen the important ones (p. 38).
*Design:* a national priority list of objects, and army air defence units that can be assigned into the duty system.

**pvo-02. Zones, sectors and who decides.** The cooperation area is split into zones and sectors of at least 60° from the
centre of the SAM formation; effort is divided by target, time, height, sector or line (`vr_krupicka_1984` p. 62).
Division by target is decided by the PVOS division commander or the front's air force and air defence commander;
otherwise the SAM commander destroys everything entering his zone. Authority to change the method is delegated down to
joint CPs (p. 63). One cooperation plan is signed by the PVOS and the allied commanders, with variants for SAM units
knocked out, communications lost and levels of command lost (p. 65).

**pvo-03. Fighters by zone.** Fighters enter a SAM fire zone only by the PVOS formation commander's decision, when the SAMs
cannot destroy all the targets or have lost combat effectiveness; the author says this rule needs revision
(`vr_krupicka_1984` p. 64). Fighters (PVOS and front) defend route sections, loading areas, defiles and crossings from
airfield alert or air alert (`vr_popelka_1983` p. 54). Fighters go to work on the far approaches on long-range radar data
(`vr_strecha_1983` p. 52).

**pvo-04. The national PVO covers our troops on home ground.** The main air defence of front reserves and second echelons
on the move is the PVOS formations on whose territory they move; organic air defence complements the PVOS grouping
(`vr_popelka_1983` p. 53). The choice is zonal air defence or direct cover of march columns (p. 54). Alert units cover the
troops leaving garrisons, then move after them (`vr_fanfule_1984` p. 35). Rear objects are covered by the national PVO
and fighters (`vr_cerny_horak_1985` p. 61).

**pvo-05. When the front moves off home ground.** On home territory full responsibility lies with the national PVO
commanders; the front's air defence chief or deputy goes to their CPs on increased readiness (`mt_rozhdestvenskiy_depth`
p. 9). By day 3-4 responsibility is divided, with operations groups and allied coordination through the Combined Command
(pp. 9-10). Lavrinovich gives two periods: first all air defence from main and alternate positions under national PVO
CPs; second, during the front's operations, troop air defence becomes "the first echelon of the country's air defense",
controlled from the front's air defence CP (`mt_lavrinovich_mdex` pp. 4-5). Cooperation groups are exchanged between the
PVOS main CP and the allies' air defence, and between PVOS and the front's joint CP; they change SAM, radar and fighter
battle formations to fit the air operation (`vr_strecha_1984` pp. 50 ff.).

**pvo-06. A gap between the two.** A gap of 1,000-1,200 km may open between troop air defence and the national PVO as
the front advances; the Supreme High Command keeps mobile air defence reserves (`mt_rozhdestvenskiy_depth` p. 5).
*Design:* for Pannonia, which defends at home, the gap appears only if the front advances into the enemy's country.

**pvo-07. Readiness and redeployment in district exercises.** Exercise stages: 6-8 hours (readiness, repelling the first
massed attack); 10-12 hours (after-effects, partial regrouping to cover ground forces and fleet); one day (later attacks).
Readiness comes by the senior chief's order, or by unit commanders on a surprise attack (`mt_lavrinovich_mdex` p. 9). On
alert, AA artillery regiments go to their divisions' areas; SAM regiments march a short way to assigned sites; sometimes
only the alert battalions fight while the rest march to rail loading (pp. 10-11). The second stage ends about midnight,
to give hours of darkness for moving to alternate positions (p. 13).

**pvo-08. The first massed strike, as the ČSLA expected it.** Up to 70% of theatre tactical aviation; a first echelon of
15-20%, a strike echelon up to 75%, reconnaissance about 10%; up to 3 hours, or as short as 30 minutes; air defence
penetrated in 20-30 km sectors (`vr_fanfule_1984` pp. 33-34). In Lavrinovich's exercise, the first attack lasts 2-3 hours,
at least 50 overflights, 25-30 targets at once for 20-25 minutes, up to half of them low (`mt_lavrinovich_mdex` p. 11).
The enemy isolates the battle area to 600 km depth with up to 15% of its aviation, in groups of 8-16
(`vr_popelka_1983` p. 51).

**pvo-09. Losses and restoration.** Irrecoverable air defence losses in an operation "may reach 25 to 30 percent"
(`mt_rozhdestvenskiy_depth` p. 7). Ammunition: 10-12 units of fire per launcher per operation, 2.5 at the start (pp.
11-12). The cooperation plan has variants for units knocked out (`vr_krupicka_1984` p. 65). A gap while SAMs move is
filled by fighters (`andersen_1979` p. 45). No written time to restore coverage after losses was found.

**pvo-10. Radar field.** A 500 km front zone needs 15-16 radar companies in a first line within 70-100 km of the forward
edge, a second line 50-70 km behind; 65-70 companies in all (`mt_rozhdestvenskiy_depth` p. 6). ČSLA radio-technical troops'
information: primary radar data usable within tens of seconds, secondary within a minute, warnings within minutes
(`vr_strecha_1983` p. 52).

### 2.9 Priorities as the ground war moved: exercises and the GPW

**ex-01. The main thrust gets the air defence (GPW).** Armies had 4-5 AA artillery divisions each in some periods. The 5th
Shock Army had 5 on 14-15 February 1945, and 2 were then regrouped away. In Belorussia in 1944, 60% of the 1st Baltic
Front's air defence went to the 6th Guards Army, which made the main thrust. The 1st Belorussian Front kept 1-2 AA
artillery divisions in reserve. The Magnushevskiy bridgehead had 4 AA divisions and 2 regiments on 24 km
(`andersen_1979` p. 40).

**ex-02. Defence (GPW).** At Kursk by 9 July 1943, 4 AA artillery divisions and 4 army AA regiments covered 36 km of front,
8 km deep; 25% of the attacking aircraft were destroyed. At Prokhorovka, 12 guns per km on 20 km, 7-8 km deep
(`andersen_1979` p. 53).

**ex-03. Shield-84.** The new regulations were tested there (`vr_poltikovic_1984` p. 101). Lessons: the defence must be
active and deep; the initial grouping should allow defence or offence without major regrouping; counterattacks and
counterstrikes are the highest form of activity (`vr_svatos_1985` pp. 26-28). No air defence priority change with numbers
was found for Shield-84.

**ex-04. District exercises.** The priority moves from repelling the first massed strike (stage one) to covering the
ground forces and fleet as they deploy (stage two, partial regrouping), then to subsequent attacks (`mt_lavrinovich_mdex`
p. 9). In a front offensive exercise: two days to regroup and deploy, 10-12 hours of the second stage, a day of successive
SAM moves in the third (pp. 15-16).

**ex-05. Druzhba.** Only the flight notices of Druzhba-76 (in `our_side_air.md`) were found. No priority rule from a
Druzhba exercise was found.

## 3. Disagreements, and which to use

| Topic | Source A | Source B | Use |
|---|---|---|---|
| Division frontage and depth | ČSLA 1985: 30-45 by 20-25 km (`vr_poltikovic_1984` pp. 95-96) | NATO: 20-30 by 15-20 km (`fm_100_2_1_1984` p. 6-5); Vševojsk up to 30 by 20 | ČSLA 1985. It is the operator's own regulation for the war's year. Keep the old numbers for a cramped sector. |
| Regiment depth | ČSLA 1985: up to 12 km | NATO: 7-10 km | ČSLA 1985. |
| Security zone depth | ČSLA 1985: 20-40 km (`vr_poltikovic_1984` p. 96; `vr_krizek_1985` p. 32) | NATO: 15 km at division, 30 km at army (`fm_100_2_1_1984` p. 6-2); Vševojsk 15 km | ČSLA 1985. |
| Forward position | ČSLA 1985: 6-8 km | Vševojsk: 3-5 km | ČSLA 1985. |
| How SAM units leapfrog | Osa: by pairs, one crew of each pair always covers (`voenizdat_osa_1991` p. 234) | "Foreign specialists": battery by battery (`andersen_1979` p. 45); Soviet long/medium range: whole units (`mt_rozhdestvenskiy_depth` p. 6) | Osa: the pair rule. Long and medium range (S-75, S-125, Kub): whole moves, the gap covered by organic AD and fighters. |
| Division CP distance | ČSLA 1985: main CP 8-10 km | NATO: forward CP as close as 3 km | ČSLA for the main CP; NATO's forward CP is a separate element and may be modelled as one. |
| Andersen's NATO frontages | 25-30 km by 30 km for a NATO division (`andersen_1979` p. 52) | (the Pact's own norms) | Use only for the enemy's defence, not ours. |

## 4. Norms for the game

| Norm | Value(s) | Source id, page | Confidence | Recommendation |
|---|---|---|---|---|
| Division defence frontage | 30-45 km (border: wider) | `vr_poltikovic_1984` pp. 95-96 | high | use |
| Division defence depth | 20-25 km | `vr_poltikovic_1984` p. 96 | high | use |
| Regiment frontage, depth | 10-15 km, up to 12 km | `vr_poltikovic_1984` pp. 95-96 | high | use |
| Battalion frontage, depth | up to 5 km, up to 3 km | `vr_poltikovic_1984` pp. 95-96 | high | use |
| Company area | 500-1000 m by 500 m | `fm_100_2_1_1984` p. 6-7 | medium (NATO view) | use until a Pact figure is found |
| Defensive positions prepared | division 3-4, regiment 2; second-echelon division 2-3, regiment 1-2 | `vr_poltikovic_1984` p. 96 | high | use |
| Security zone depth | 20-40 km; lower end on an independent axis | `vr_poltikovic_1984` p. 96; `vr_krizek_1985` p. 32 | high | use |
| Security zone lines | 6-8 km apart; the last at the forward position | `vr_krizek_1985` pp. 32, 36 | high | use |
| Delay by a reinforced battalion | repels up to 2 battalions; delays a brigade 1-2 h | `vr_krizek_1985` p. 34 | medium (a planning estimate) | use as the delay model's anchor |
| Forward position | 6-8 km ahead | `vr_poltikovic_1984` p. 97 | high | use |
| Combat outpost | up to 2 km ahead of first-echelon battalions | `vr_poltikovic_1984` p. 97 | high | use |
| Who sets the security zone, who runs it | army commander sets depth; division commander runs it and orders each withdrawal | `vr_krizek_1985` pp. 34-35 | high | use in the chain of command |
| Withdrawal authority | senior commander only | `reznichenko_1984` pp. 49, 152 | high | use: every unit holds until ordered |
| Osa start-position change | only on the SAM regiment commander's order | `voenizdat_osa_1991` p. 238 | high (1991 text) | use |
| Counterattack authority | the senior (next higher) commander | `reznichenko_1984` p. 179; `fm_100_2_1_1984` p. 6-3 | high | use |
| Counterattack form | from the march, into the flank; until the penetration is gone or a set line is reached | `reznichenko_1984` pp. 178-179 | high | use |
| Counterattack lines | main and reserve deployment lines per axis; 1-2 firing lines | `vr_poltikovic_1984` p. 96 | high | use |
| Reserve movement speed | 20-30 km/h by day, 15-20 km/h at night | `fm_100_2_1_1984` p. 6-7 | medium (NATO view) | use |
| Division CPs | main 8-10 km, rear up to 30 km from the forward edge | `vr_poltikovic_1984` pp. 99-100 | high | use |
| Regiment CPs | main 4-6 km, rear up to 15 km | `vr_poltikovic_1984` pp. 99-100 | high | use |
| Army CP moves | once per 24 h; elements up to 20 km apart | `vr_cerny_horak_1985` p. 57 | high | use |
| Osa battery in defence | 3-5 km from the forward edge; lines or pairs up to 3 km apart; 1-2 reserve positions up to 2 km away | `voenizdat_osa_1991` pp. 235-236 | high (1991 text) | use |
| Osa pair rule | one crew of each pair covers; pairs up to 3 km apart; 200-500 m within a pair | `voenizdat_osa_1991` pp. 234-235, 237 | high | use |
| Osa distance from covered troops | 1.5-3 km | `voenizdat_osa_1991` pp. 234, 237-238 | high | use |
| Osa march spacing | 25-50 m; 200 m or more under air threat | `voenizdat_osa_1991` p. 232 | high | use |
| Osa march halts | up to 1 h every 3-4 h; up to 2 h for a meal | `voenizdat_osa_1991` pp. 239-240 | high | use |
| Long/medium-range SAM moves | whole units, 2-3 a day at 80-100 km/day; an army 2 moves a day, 3 h at most | `mt_rozhdestvenskiy_depth` pp. 6-7 | high (1966 text) | use for S-75, S-125, Kub |
| March to readiness | 5-15 min | `mt_rozhdestvenskiy_depth` p. 8 | medium (no system named) | use for self-propelled units only |
| Coverage of two SAM regiments | 60 km front by 100-120 km depth | `mt_rozhdestvenskiy_depth` p. 7 | medium | use as a check |
| Low-altitude SAM unit coverage | 25 km by up to 50 km | `mt_rozhdestvenskiy_depth` p. 7 | medium | use as a check |
| AD losses in an operation | 25-30% irrecoverable | `mt_rozhdestvenskiy_depth` p. 7 | medium | use to tune the balance |
| Main-thrust share of air defence | 60% (1944 example) | `andersen_1979` p. 40 | medium (historic example) | use as a default skew |
| Sky-sharing sectors | at least 60° each | `vr_krupicka_1984` p. 62 | high | use |
| Who sets the sharing method | PVOS division commander or front air force and AD commander | `vr_krupicka_1984` p. 63 | high | use |
| Fighters into a SAM zone | only by the PVOS formation commander's decision | `vr_krupicka_1984` p. 64 | high | use |
| First massed strike | up to 70% of tactical aviation; up to 3 h; penetration sectors 20-30 km | `vr_fanfule_1984` pp. 33-34 | medium (ČSLA estimate of NATO) | use for the enemy's opening |
| Enemy effort on CPs, missiles, artillery, rear | up to 30% | `vr_cerny_horak_1985` p. 58 | medium | use |
| Strela-2M at a CP | 3 teams, at least 500 m out | `vr_cerny_horak_1985` p. 61 | high | use |
| SAMs at a crossing | near bank, within 500 m of the crossing | `vr_fanfule_1984` p. 35 | high | use |

## 5. Post-period items

| Item | Source | Date | How used |
|---|---|---|---|
| The PVOS formation covering ground forces in mobilisation areas, on the move and in operations; priority to first-echelon formations and missile and nuclear means; fighters on air alert at crossings and deployment; the decision on a 1:500,000 map | `vr_klocok_1987` pp. 63-64 | 1987 | hindsight: confirms `vr_popelka_1983`; no new numbers used |
| Defensive doctrine: hold until full operational deployment, then counterstrike, counterattack and decisive counteroffensive | `vr_podesva_1988` pp. 17-19 | 1988 | hindsight only; 1985 is still the offensive-minded doctrine |
| Destroying an enemy preparing to attack | `vr_roucka_1988` (already registered) | 1988 | not re-read here |
| A single PVO system | `vr_hrbek_1991` (already registered) | 1991 | not re-read here |
| Czechoslovak plans of October 1986 and July 1989 | `polnar_2022` pp. 148-149 | 1986, 1989 | shows the shift to defence after the war's year |
| Osa battery norms | `voenizdat_osa_1991` | 1991 (Osa-AKM) | the battery's formations are assumed unchanged from 1985; flagged |
| PU-12 posts and the radar warning system | `vr_1994_pvov` (already registered) | 1994 | not used here |

## 6. Open items

1. **S-75 and S-125 march and deploy times.** Not found in any text read. Needed for national PVO redeployment.
2. **Hungarian and East German plans for defending their own territory.** Not found. Polish *Myśl Wojskowa* and East German
   *Militärwesen* were not searched.
3. **A written priority table between national objects and our troops.** Only principles were found (`vr_lhotsky_1985`,
   `vr_popelka_1983`).
4. **Druzhba and Shield exercises: air defence priority changes with numbers.** Not found.
5. **The otkhod in the 1980s Soviet combat regulations (BUSV).** Not read; Reznichenko stands in.
6. **Time to restore coverage after losses.** Not found. Only the loss rate and the plan's variants.
7. **Mobile defence as a Pact form.** Not found in the texts read.
8. **Who orders a national PVO SAM battalion to a reserve position, and how long it takes.** Principles only
   (`mt_lavrinovich_mdex` p. 13: moves at night).
9. **Vorlíček, VR 1979 No. 10** (overcoming the security zone in the offence), Liebl and Němec, VR 1981 No. 3 (engineer
   support of the defensive operation) and VR 1984 No. 3 (defence in capitalist armies) were downloaded but not read.

## 7. Sources

| id | tier | citation | notes |
|---|---|---|---|
| `vr_poltikovic_1984` | 1 | Vilijam Poltikovič, preview of the new ČSLA Combat Regulations of the Ground Forces, part 1, against Vševojsk-1-1, *Vojenské rozhledy* (Vojenská mysl section) 1984 No. 5, pp. 95-101, https://vojenskerozhledy.cz/pdfs/vor/1984/05/16.pdf | Used for frontages, depths, security zone, forward position, positions, CP distances, fortified areas, air defence tasks in defence; the regulations' start date and Shield-84. Read: full. Whose view: Czechoslovak army's own (author's name garbled in the OCR, read from the Russian contents). |
| `vr_krizek_1985` | 1 | Křížek, on the fight in the security zone, *Vojenské rozhledy* 1985 No. 8, pp. 32-37, https://www.vojenskerozhledy.cz/pdfs/vor/1985/08/06.pdf | Used for security zone depth, lines, forward detachments, delay estimates, who sets and runs the zone, withdrawal by line. Read: full. Whose view: Czechoslovak army's own. |
| `vr_krupicka_1984` | 1 | Krupička, on cooperation of PVOS SAM troops and fighters, *Vojenské rozhledy* 1984 No. 5, pp. 61-67, https://vojenskerozhledy.cz/pdfs/vor/1984/05/11.pdf | Used for zones and sectors, who decides the sharing method, fighters in SAM zones, the cooperation plan's variants. Read: full. Whose view: Czechoslovak national air defence's own. |
| `vr_cerny_horak_1985` | 1 | Černý and Horák, on the air defence of an army's CPs, missile units and rear, *Vojenské rozhledy* 1985 No. 2, pp. 56-62, https://vojenskerozhledy.cz/pdfs/vor/1985/02/10.pdf | Used for army CP sizes and move intervals, the enemy's share against CPs and missiles, Strela-2M at CPs, rear cover by the PVOS. Read: full. Whose view: Czechoslovak army's own. |
| `vr_popelka_1983` | 1 | Popelka, on the air defence of troops on the march, *Vojenské rozhledy* 1983 No. 10, pp. 51-55, https://www.vojenskerozhledy.cz/pdfs/vor/1983/10/11.pdf | Used for the PVOS covering reserves on the move, the march order of AD units, fighters over routes and crossings, the enemy's isolation effort. Read: full. Whose view: Czechoslovak army's own. |
| `vr_fanfule_1984` | 1 | Fanfule, on air defence when troops leave garrisons and in the first massed strike, *Vojenské rozhledy* 1984 No. 3, pp. 33-36, https://www.vojenskerozhledy.cz/pdfs/vor/1984/03/07.pdf | Used for the first massed strike's size and length, penetration sectors, SAMs at crossings, alert units covering troops leaving garrisons. Read: full. Whose view: Czechoslovak army's own (its estimate of NATO). |
| `vr_svatos_1985` | 1 | Col. Ing. Zdeněk Svatoš, «K některým zkušenostem z operačně strategického cvičení Štít-84», *Vojenské rozhledy* 1985 No. 4, pp. 25-29, https://www.vojenskerozhledy.cz/pdfs/vor/1985/04/05.pdf | Used for Shield-84's lessons on the active defence, counterattacks and counterstrikes, attack helicopters. Read: full. Whose view: Czechoslovak army's own. |
| `vr_lhotsky_1985` | 1 | Col. prof. Ing. Václav Lhotský, «Čtyřicet let výstavby PVOS», *Vojenské rozhledy* 1985 No. 4, pp. 35-40, https://www.vojenskerozhledy.cz/pdfs/vor/1985/04/07.pdf | Used for how the national PVO is built: object and object-zonal defence, defended objects by importance, army AD in the duty system since 1976. Read: chapter (pp. 37-39). Whose view: Czechoslovak national air defence's own. |
| `vr_strecha_1983` | 1 | Maj. Gen. Ing. Josef Střecha, «Vojenskovědecká konference PVOS», *Vojenské rozhledy* 1983 No. 6, pp. 51-57, https://www.vojenskerozhledy.cz/pdfs/vor/1983/06/10.pdf | Used for radar information delays and fighters on the far approaches. Read: chapter. Whose view: Czechoslovak national air defence's own. |
| `vr_strecha_1984` | 1 | Maj. Gen. Ing. Josef Střecha, on cooperation groups between the PVOS and the front, *Vojenské rozhledy* 1984 No. 8, pp. 50 ff., https://vojenskerozhledy.cz/pdfs/vor/1984/08/10.pdf | Used for exchanged cooperation groups and their tasks. Read: chapter. Whose view: Czechoslovak national air defence's own. |
| `vr_klocok_1987` | 1 (post-period) | Klocok, on the PVOS covering ground forces, *Vojenské rozhledy* 1987 No. 4, pp. 63-64, https://www.vojenskerozhledy.cz/pdfs/vor/1987/04/12.pdf | Used as hindsight on PVOS cover of the ground forces. Read: full. Whose view: Czechoslovak national air defence's own. |
| `vr_podesva_1988` | 1 (post-period) | Maj. Gen. Ing. František Podesva, «Vojenská doktrína a další rozvoj operačního umění i přípravy vojsk», *Vojenské rozhledy* (Vojenská mysl) 1988 No. 8, pp. 17-19, https://www.vojenskerozhledy.cz/pdfs/vor/1988/08/04.pdf | Used as hindsight on the 1987 defensive doctrine. Read: snippet (opening). Whose view: Czechoslovak army's own. |
| `polnar_2022` | 4 (later study) | Stanislav Polnar, 'The Evolution of Czechoslovak Defence Planning, 1918-1992', *British Journal for Military History* 8.3 (2022), pp. 128-151, https://journals.gold.ac.uk/index.php/bjmh/article/download/1647/1761/2020 | Used for the Czechoslovak war plans of 1964, 1977, 1986 and 1989. Read: chapter (pp. 145-149). Whose view: a Czech military historian, from the archives. |
| `reznichenko_1984` | 1 | already registered: V. G. Reznichenko (ed.), *Тактика*, Voenizdat 1984; USAF translation *Tactics: A Soviet View* (1987), DTIC ADA183185, https://archive.org/details/DTIC_ADA183185 | Used for echelons, outposts, withdrawal authority, holding, counterattacks, GPW examples. Read: chapter (defence, pp. 49, 152-179). Whose view: Soviet own. |
| `fm_100_2_1_1984` | 1 | already registered: US Army FM 100-2-1, *The Soviet Army: Operations and Tactics*, 16 July 1984, https://archive.org/details/fm100-2-1 | Used for NATO's view of the Soviet defence (ch. 6) and troop air defence (ch. 11). Read: chapter (6, 11). Whose view: NATO's (what NATO believed). |
| `andersen_1979` | 1 | already registered: Yu. A. Andersen et al., *Противовоздушная оборона сухопутных войск*, Voenizdat 1979; JPRS L/9337, 8 Oct 1980, CIA-RDP82-00850R000300040017-6, https://archive.org/details/cia-readingroom-document-cia-rdp82-00850r000300040017-6 | Used for GPW air defence massing, relocation methods, defence priorities. Read: chapter (pp. 40, 45-46, 52-53). Whose view: Soviet own (and its reading of NATO). |
| `mt_rozhdestvenskiy_depth` | 1 | already registered: Rozhdestvenskiy and Bilanonov, *Military Thought* 1966 No. 2 (78); CIA-RDP10-00105R000201690001-8, https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000201690001-8 | Used for SAM moves, coverage, losses, ammunition, home-territory responsibility. Read: full (pp. 5-12). Whose view: Soviet own. |
| `mt_lavrinovich_mdex` | 1 | already registered: Lavrinovich, "Military District Air Defense Exercises", *Military Thought*; CIA-RDP10-00105R000201720001-4, https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000201720001-4 | Used for exercise stages, alert moves, kill zones breaking up on the move. Read: full. Whose view: Soviet own. |
| `voenizdat_osa_1991` | 1 (status as in `docs/SOURCES.md`) | already registered: *Автономный войсковой зенитный ракетный комплекс «Оса»*, Voenizdat 1991; local, `reference/sources/` | Used for the Osa battery's march, formations, positions, pair rule, tasks, withdrawal. Read: chapter (printed pp. 225-240). Whose view: Soviet own. |
| `mitronin_1976` | 1 | already registered (see `our_side_air.md`) | Referred to only. Read: as registered. Whose view: Warsaw Pact's own. |
| `mt_turchin_coop` | 1 | already registered (see `our_side_air.md`) | Referred to only. Read: as registered. Whose view: Soviet own. |
| `vr_roucka_1988`, `vr_hrbek_1991`, `vr_1994_pvov` | 1 | already registered | Listed as post-period only. Read: not re-read. Whose view: Czechoslovak army's own. |

## Verification (19 Nov 2026)

The texts were re-opened and searched for the exact words behind the most important numbers.

| Number | Source, page | Text found | Verdict |
|---|---|---|---|
| Division 30-45 km wide, 20-25 km deep | `vr_poltikovic_1984` pp. 95-96 | "divizi 30—45 km", "u divize 20—25 km" | confirmed |
| Regiment 10-15 km wide, up to 12 km deep | `vr_poltikovic_1984` pp. 95-96 | "pluku 10—15 km", "u pluku do 12 km" | confirmed |
| Security zone 20-40 km | `vr_poltikovic_1984` p. 96 | "hluboké 20 až 40 km" | confirmed |
| Forward position 6-8 km (old 3-5); outpost up to 2 km | `vr_poltikovic_1984` p. 97 | "6—8 km (Vševojsk-1-1 3—5 km)", "do 2 km bojové zajištění" | confirmed |
| Division main CP 8-10 km | `vr_poltikovic_1984` p. 99 | "VS 8—10 km" | confirmed |
| Zone lines 6-8 km apart; delay 1-2 h; army commander sets depth | `vr_krizek_1985` pp. 32, 34 | "6 až 8 km za sebou", "jedné až dvou hodin", "velitel armády" | confirmed |
| Osa defence 3-5 km from the forward edge, pairs up to 3 km | `voenizdat_osa_1991` pp. 235-236 | "3—5 км от", "до 3 км" | confirmed |
| Osa position change only on the regiment commander's order; 1.5-3 km from covered troops | `voenizdat_osa_1991` p. 238 | "только по приказу командира зенитного", "1,5—3 км от прикрываемых" | confirmed |
| SAM moves 2-3 a day; army 3 h at most; 5-15 min to readiness; 25-30% losses | `mt_rozhdestvenskiy_depth` pp. 6-8 | "two to three times per day", "no more than three hours", "five to 15 minutes", "25 to 30 percent" | confirmed |
| Low-altitude unit covers 25 km by 50 km | `mt_rozhdestvenskiy_depth` p. 7 | "25 kilometers ... 50" | confirmed |
| NATO: division 20-30 km; zone 15 km at division; SA-4 to 45 km | `fm_100_2_1_1984` pp. 6-2, 6-5, 11-2 | "20 to 30 kilometers in width", "15 kilometers at division level", "about 45 kilometers beyond" | confirmed |
| 60% to the main thrust; battery-by-battery moves | `andersen_1979` pp. 40, 45 | "60 percent", "each subsequent battery should" | confirmed |
| Army CP moves once per 24 h; 30% against CPs etc. | `vr_cerny_horak_1985` pp. 57-58 | "jednou za 24 h", "až 30 %" | confirmed |
| Sectors at least 60° | `vr_krupicka_1984` p. 62 | "nejméně 60 stupňů" | confirmed |
