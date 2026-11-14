# 10. Weapons, attacks and damage

Written 23 October 2026. How the enemy's aircraft attack: what they carry, how the planner chooses the weapon and the
attack, how the attack is flown, and what it does to the target. Code: `simcore/include/simcore/air_war/planner.hpp`
(`CanDeliver`, `DeliveryCep`, `WeaponKill`, `ExpectedShare`, `Weaponeer`, `MainRoute`), `resolve.hpp` (`Deliver`); data:
`data/air_war/weapons.json`, each aircraft's `loads` in `data/air_war/faction_*.json`, the `delivery` group of
`data/gameplay/air_war.json`. Research and the benchmarks: `docs/research/AIR_WAR_MISSIONS_2026-10.md`, `09_BALANCE.md` §9.

## What a weapon is, and what it does

`weapons.json` lists 37 weapons of the period, NATO's and the Pact's and the light air arms': general-purpose and
retarded bombs, cluster bombs, runway weapons (JP233, Durandal, BetAB-500), laser-, TV- and IR-guided bombs and missiles
(Paveway, GBU-15, AGM-65, KAB-500L, Kh-25ML, Kh-29L), rockets, anti-tank missiles, guns, and the rebels' improvised
bombs. Each has its facts (mass, guidance, deliveries, a stand-off range, the year it entered service; `wiki_weapons`)
and two game values:

- **footprint_m**: how near an aim point it must fall to put it out of action;
- **effect** by target class (soft, light, armour, hard, very hard, runway): the chance it does, falling inside.

Never a weapon-effects table: the two values are calibrated until whole attacks come out as history recorded them
(§ "Calibration"). `reliability` is the chance it works; `laydown_reliability` the chance an unretarded bomb dropped from
low level has time to arm (the Falklands: 11 of 25).

**Targets.** Every object kind is a target of a class with aim points (`weapons.json` "targets"): a SAM site is light
with six (radars and launchers), an airfield runway class with four (runways and taxiways), a bridge hard with two
spans, troops armour with twelve, a convoy light with eight, a train light with six, an armoured column armour with ten.
An object may set its own (`target_class`, `aim_points` in the theatre: a border battalion and the artillery are light).
A target's function is the share of its aim points working.

## Choosing the attack: the weaponeer

For each aircraft type, target class, profile, part of the day and weather, `Weaponeer` tries every weapon it carries in
every delivery it allows and keeps the one with the most `share × (1 − delivery_risk × exposure)`: the expected share of
the target's aim points its load puts out of action, against how much of the target's own guns the attack meets. The
doctrine's `delivery_risk` (rebels 0.8, neutral 0.7, NATO 0.4, the Pact 0.3) is how much the planners fear those guns.

`CanDeliver` says what is possible:

| Rule | Why |
|---|---|
| Only weapons in service in the war's year | 1985: no CALCM, no Tomahawk |
| A helicopter fires from the hover or pops up; a jet never hovers | |
| Laydown, toss, pop-up and the hover need the low profile; dive and level the medium; stand-off either | the profile is the flight's height on the way |
| A laser-guided weapon needs a designator, not low cloud, not night without a night attack system | `designator`, `night` per type |
| A TV seeker needs daylight and no low cloud; an IR seeker no low cloud | |
| Nothing unguided by eye (pop-up, dive, hover) under low cloud; level bombing in it only by an all-weather type | |

## How far it falls from the aim

`DeliveryCep`: a guided weapon's CEP is its guidance's (laser 6 m, TV and IR 3 m, wire 2 m); an unguided one's is its
delivery's (laydown 30 m, toss 170 m, pop-up 50 m, dive 45 m, level from medium altitude 130 m, the hover 25 m), times
`1.4 − 0.8 × computing` for the type's sight (an F-111F's 0.8, an F-4E's 0.45), times 2.5 for a dive or pop-up by eye at
night without a night attack system, times 1.5 under low cloud, and for every weapon times `1.25 − 0.5 × airmanship`.
The level figure is the Vietnam-era average miss (447 ft); the laser's the early Paveway's (20 ft).

## What it does

`WeaponKill`: the chance one weapon puts an aim point out of action is the chance it falls within its footprint (a
circular normal miss with that CEP: `1 − 0.5^((footprint/CEP)²)`), times the chance it goes off, times its effect on
the class. A bomb's footprint is three times as wide against soft targets and 1.8 times against light ones (blast and
fragments reach further than against armour or concrete); a runway is cut by a crater anywhere across its width, so a
runway weapon's footprint is at least 25 m. `ExpectedShare`: spread over the aim points, `1 − (1 − kill)^(weapons /
aim_points)`. In the resolver each weapon falls on one of the aim points still working; each one it kills is gone. Under
the fire of a working defence and without SEAD the CEP is 1.2 times worse. A laser-guided weapon whose spot is lost
(cloud, darkness without a night system) or a TV seeker in the dark falls like a bomb from height.

## How it is flown

The planner's route (`MainRoute`) flies the delivery between the initial point and the release, and the played raid
flies the same waypoints with their heights (terrain following below the apex, `raid_scenario.hpp`):

| Delivery | Route | Heights (`delivery.*`) |
|---|---|---|
| Laydown | low over the target, the release on top | the type's low height |
| Toss | low to the pull-up 8 km out, the release 5 km out climbing, the turn away | pull-up low, release 600 m |
| Pop-up | low to the pull-up 6 km out and 1.5 km to the side, the apex, the dive, the release 1.5 km out | apex 1,200 m, release 700 m |
| Dive | from medium altitude to the roll-in 5 km out, the release 2.5 km out | release 1,500 m |
| Level | from medium altitude over the target | the type's medium height |
| Stand-off | to the weapon's range and away | the profile's |
| Hover (helicopters) | from the hide to the firing point 4 km out | 30 m |

`delivery.exposure_*` is the share of the target's own guns each attack meets (a laydown all of them, a pop-up two
thirds, a dive a third, a toss less than a third, level and stand-off none: medium altitude is above them),
`delivery.release_*_m` how far short of the target the weapons go. A jet in a played raid may climb at 120 m/s so it
flies its pop-up.

## Calibration

`simcore/examples/air_war_bench.cpp` (`air_war_bench [data] [runs] [--why] [--set key=value]`) flies historical
analogues and prints the model beside the record (`09_BALANCE.md` §9): a massive bridge against dumb and laser-guided
bombs, the misses, low against medium altitude, runways cut, ARMs against disciplined crews, low-level fuzing, a SAM belt,
a suppression package. `--set` tries a tuning value; `--why` prints the losses by role and defence.

## Tests

`tests/test_weapons.cpp`: every load is a known weapon and every attack type carries one in service; the kill model's
misses against the 1972-73 averages, a better sight and crew aiming better, night by eye worse, blast reaching further
against soft targets, the runway strip, low-level fuzing; the weaponeer's choices (a bridge by day: laser-guided bombs;
an F-4E at night: no laser; low cloud: nothing by eye; an airfield low: JP233 laid down; the Su-24 at a runway: retarded
bombs low, BetAB or laser-guided from height; a wary doctrine never more exposed); each attack's shape in two campaigns'
packages; retarded against unretarded bombs; the Thanh Hoa bridge against dumb and laser-guided bombs.
