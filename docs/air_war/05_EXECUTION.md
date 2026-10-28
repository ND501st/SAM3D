# 5. Flying the raids

Code: `simcore/air_war/resolve.hpp` (abstract), `simcore/air_war/raid_scenario.hpp` (the player's mission),
`simcore/air_war/mission_bridge.hpp` (its outcome). Balance numbers: `data/gameplay/air_war.json`.

## 1. Which raids the player flies

Each planned package that comes within `pacing.sector_m` (10 km) of the battery, or is aimed at the battery, its
dummies or anything within that distance (what the battery defends), is a candidate. When its first aircraft takes off,
`AirCampaign::Advance` stops and offers it, at most `pacing.max_per_day` a day and `pacing.min_gap_h` apart; the rest are
resolved abstractly. The pacing only chooses among the enemy's own raids (`l4d_director_2007`, used for that alone).

**Planned** (the battery's war, `docs/campaign/05_THE_SYSTEMS.md` §6-8, roadmap phase 15): the battery's task runs as one
continuous mission, and these packages enter it live as they come instead of as separate scenarios with the battery
deployed in place. The battery's state (positions, missiles, fuel, damage, dummies, the board, the journal) carries
from one raid to the next.

## 2. The abstract resolution

Flights in the order they reach their target (decoys and SEAD first). Each route segment through each real air defence
that works and has rounds:

- a radar defence radiates while it engages (not under the player's "fight silent" posture): the seconds go to the
  outcome for the enemy's ELINT; SEAD and hunter flights in ARM range fire up to two ARMs a segment: a fire-control
  radar is destroyed by `attrition.arm_kill`, or the crew switches off in time by its EMCON discipline (the player's
  posture, 0.5 for others) and an ARM without memory then kills only by `attrition.arm_kill_no_memory_off`; a hit takes
  out one of the defence's `radars` (an Osa battery has four combat vehicles with a radar each; a Kub, S-125 or S-75
  battery one), and the battery is out of the fight with the last; a switched-off radar is suppressed for the rest of
  the package;
- decoy drones draw a launch or two and show where the defence is;
- a defence engages nothing faster than its `max_target_mps` (the Osa-AK 500 m/s); a cruise missile (a `missile` flight)
  is engaged like an aircraft, but one shot down is the defence's (`missiles_downed`), never a crew lost, and those that
  get through strike with their own `hit_chance`, day or night (`09_BALANCE.md` §8, cruise missiles);
- the kill chance per aircraft (`PassKillChance`: lethality × share of the pass × height cover × (1 − suppression) × (1 −
  jamming) × night and weather for optical defences × EMCON × (1 − evasion)), drawn per aircraft it engages; damaged
  aircraft too; rounds spent. A defence engages as many aircraft of a passing flight as it has fire channels
  (`defence.channels`: an S-75's or S-125's guidance radar one, an Osa battery four, a platoon's launchers four, scaled by
  how much of it works: `EngagedAircraft`, which the planner's estimate uses too); without channels, every aircraft in
  reach (the theatre the balance pass tuned has none; the nation's theatre sets them for every system);
- the defender's fighters take their chance on unescorted packages;
- the leader's nerve: once the losses reach his share (boldness, quirks) the main flights turn back.

At the release point the light guns and shoulder-fired missiles around a military target without an air defence of
its own take their chance on each aircraft attacking, by the share of them its attack meets (`FlightExposure`: a laydown
all, a pop-up two thirds, a toss or a dive a third or less, medium altitude and stand-off none; `10_WEAPONS.md`) (`TargetGunsChance`: troops
`attrition.target_guns_troops`, other military objects `attrition.target_guns_site`, towns none; night and weather as
for optical defences; medium altitude is out of their reach: Dugman 5's lesson, `wiki_model5`). The planner prices the
same guns in its expected losses. Then the surviving main flights' bombs fall on what is really near the aim point: the target if it is
within `strike.stale_miss_m`, a dummy within `strike.decoy_area_m`, a different object, or empty ground. Each weapon falls
on one of the target's aim points still working and puts it out of action by its CEP, footprint and effect on the
target's class (`10_WEAPONS.md`); a type flying without weapons in the data strikes by its old strike power (hits by
`strike.hit_chance` × load × night, weather, toss accuracy; damage = strike power × (1 − 0.7 hardness)). An armed
reconnaissance first searches its stretch of road or railway and attacks the first column it sees on the move, or
brings its weapons home (`14_LOGISTICS.md`). Every defence's lethality is a relative one, times `attrition.pass_scale`;
a radar that fires at decoys is suppressed by the SEAD behind them (`09_BALANCE.md` §9). Bombs near a town's edge risk
civilian casualties. Reconnaissance photographs what lies along its route.

## 3. The raid as the player's scenario

`RaidScenarioBuilder::Build(campaign, package)` writes scenario JSON that `LoadScenario` (the same strict loader as every
hand-written scenario) accepts, and a manifest for the bridge:

| Scenario part | From |
|---|---|
| origin, terrain | the battery's place (theatre ENU → geodetic → scenario ENU), the theatre's ground |
| targets | every surviving aircraft of every flight inside `scenario.clip_m`, one target each, spread in formation, entering where the route crosses the circle, at its time; type, gameplay class, RCS, turn rate, speed; terrain following for low flights (waypoint heights), the pop-up behaviour for helicopters with a hide; evasion on the lock with an RWR, on the launch without; the RWR (EM); `decoy` for decoy drones; board type 0 for jammers, 9 otherwise; formation |
| own_unit.em.jammers | stand-off and self-protection jammers on their aircraft, the faction's EM libraries |
| battery | the launchers it still has (fewer as the battery is hurt), TZM, fuel truck, PU-12, deployed; night; "emcon" order when the posture fights silent |
| prepared_position | emplacement and nets by the posture; the dummies where they stand, with decoy emitters by the posture |
| c2 | the theatre's warning nets near the battery: of each kind (`c2_source`) the nearest that still works within 150 km, else the nearest with its outage (the radar company the battery is nearest, the neighbouring battery on the radio); the regiment's radars that still stand feed the PU-12; if none stands, the PU-12 is blind (a c2_fail trigger) |
| triggers | the air-raid warning (`air_threat`) `own_intel.warning_s` before the first aircraft (15 min for a planner with loose lips) |
| doctrine | the TV check when our side has seen decoys; search-radar bursts when the battery keeps emission discipline |
| crew | five seats at the campaign's crew tier; NPC vehicle and battery commanders |
| environment | visibility by the weather, daylight by the time of day |
| readiness, empty_containers | deployed; the missiles left in the vehicle |

## 4. The mission bridge

Stepped after every `ScenarioRunner::Step`, from the runner's public state only:

- **release**: a strike, DEAD, helicopter or recce aircraft alive within `scenario.release_capture_m` of its release
  point drops its load (photographs, for recce); it lands `scenario.weapon_fall_s` later where it was aimed, scattered by
  a couple of hundred metres; our vehicle, the battery's other vehicles, dummies and the objects inside the scenario take
  their chances (`scenario.bomb_lethal_m`, `bomb_pk_vehicle`, `rocket_pk_vehicle`, `strike.hit_chance`);
- **ARMs** (A11, 20 Oct 2026): with the EM layer a SEAD or hunter aircraft with a warning receiver carries EM ARMs, the
  target's `"arms"` written by the generator (its type's `arm_em`, or by the war's year its `arm_em_by_year`, `data/air_war/faction_*.json`: the F-4G's AGM-78B Standard ARM class in a war before 1987, its HARM class after,
  the F-4E's Shrike class, the MiG-25BM's Kh-58 class; default the HARM class with `arm_memory`, else the Shrike class),
  aimed where the enemy's intelligence has the battery, within the type's `arm_range_km`. The runner fires and flies
  them (`docs/EM_ARCHITECTURE.md` §35); the bridge reads their ends from `ScenarioRunner::ArmResults`: one on our vehicle
  kills the radar and puts the vehicle out of action, one on a dummy destroys it, a miss counts the radar suppressed.
  For an aircraft without (no EM layer, no warning receiver) the gameplay model stays: in ARM range while our search or
  tracking transmitter radiates it fires (one per 10 s); the ARM flies at `scenario.arm_speed_mps`; it hits by
  `scenario.arm_pk` if the radar radiates when it arrives, or radiated within `scenario.arm_memory_s` and the seeker has
  memory; otherwise it is logged as lost ("the radar went silent");
- **the neighbours** (the nation on the map N4, 17.1's first form, 23 Oct 2026): our other air defences whose zones
  reach into the scenario (`RaidManifest::neighbours`, with their state: working share, rounds) take their chances at
  the aircraft passing through, by the abstract resolver's rule: as an aircraft enters a zone its pass is rolled
  (`PassKillChance` on the chord it will fly on its heading, the package's SEAD suppression and jamming against a radar
  one, night and weather against an optical one, the defence's fire channels and rounds), and an aircraft hit goes down
  in the scenario half way through the zone (`ScenarioRunner::DestroyTarget`, "destroyed by" the defence; a decoy drone
  draws rounds and is taken uncounted). Their kills, rounds, launches and radar time go to the outcome as theirs. The
  raid scenario lists them (`"neighbours"`: name, system, place, reach): the game draws each as its system's site of
  basic models on the scenario's ground (`ASimScenarioDirector::SpawnNeighbourSites`, the site layouts in
  `data/scenery/basic_models.json` `"systems"`), and the PU-12M's plotting board marks them in blue pencil with their
  reach and our name for them (Оса, Куб, С-125, ЗУ-23...), as the regiment's order gives a battery its neighbours.
  The regiment's other **Osa batteries fight** instead of rolling (the nation on the map N4, 23 Oct 2026): the scenario
  marks them `"fights"` with their working launchers and missiles, the runner sets those launchers up in the battery's
  combat formation about their place as shooters of the battery fire model (`battery_fire.hpp`, each taking the nearest
  hostile in reach: their post is not ours), and the bridge reads their kills ("destroyed by" the battery), missiles and
  radar time from it (`ScenarioRunner::NeighbourFights`, `NeighbourKills`, `NeighbourLaunches`); the game draws them
  where they fire from;
- **what the enemy learns**: every second our radars radiate; every launch seen by an enemy aircraft within
  `scenario.revealed_by_launch_m`.

Every event goes into the runner's log, so the debrief shows it. `Finish()` returns a `PackageOutcome`: aircraft shot
down (the engagement model's HIT, the battery's other launchers' "destroyed by", the neighbours' kills credited to
them), which flights released, which broke away, damage, dummies destroyed, missiles spent (the neighbours' as
`missiles_by`), ARMs fired, emissions, sightings, radar killed or silenced. `AirCampaign::CompletePlayerMission` applies
it and resolves the rest of the raid abstractly without the battery: every part of every route except what the
scenario flew (inside its circle while it ran: `ResolveContext::played_*`, `ResolvedPieces`), the way in before the
scenario as much as the way out after it. (Until 23 Oct 2026 it resolved only from the scenario's end, so a played
raid's approach met no defence at all.)

The crew fights the ARMs by its drill (roadmap E6): a senior operator of class2 or better calls the signs of a launch on
his scope and the commander has every transmitter off once the missile in the air has burst. Once our radars have come
under ARM attack anywhere in the war (`RaidPatterns::arm_attacks`, from each raid's `radars_killed` and
`radars_suppressed`: their sites report it), the regiment orders its batteries to the ARM drill (a journal line from
"regiment", an order down the chain to the vehicle commanders, `command/air_war_feed.hpp`) and every generated raid's
combat order gives the regiment's least emission (doctrine `emission`: `arm` from the regiment); otherwise the battery's
posture gives the limited regime's bursts as before (`docs/crew/16_EMISSION.md`).
`relocate_after_arm` is not set: it needs a battery that drives the vehicle, and a raid's battery is deployed in place.
The player can switch off too.
