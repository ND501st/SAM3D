# Gameplay tuning

One file per system with the game-balance numbers used by `simcore/engagement.hpp`: when zone lamps light, how close
an operator must aim for the tracking radar to capture, how fast NPC crew turn the wheels, how long a missile takes
to arrive, and the chance of a hit.

**These are game values, not performance data for any real system.** A value taken from a document says so in its
group's `source`; those only decide when a lamp lights (e.g. the zone the SAM Simulator manual gives). Everything
else is there to make the game play well, and can be changed freely.

**Calibration targets (roadmap 6.7).** The NVA's published single-missile kill-probability ranges for the Osa-AK
(`flak11`, `docs/research/OSA_RESEARCH_2026-10.md` §5) are the yardstick for `hit_chance`: manoeuvring targets
0.55–0.85, strike aircraft at 10 m 0.12–0.55, helicopters at 10 m 0.07–0.12, cruise missiles at 10 m 0.12–0.38. They
calibrate the game; they don't turn it into a weapon-physics model.

The chance of a hit is a product of plain factors, all in the `outcome` group:

```
chance = hit_chance[target class]
       x (TV track ? tv_factor : 1)
       x (in the zone at launch ? 1 : outside_zone_factor)
       x max(manoeuvre_factor_min, 1 - manoeuvre_penalty_per_deg_s x target turn rate)
       x max(deviation_factor_min, 1 - deviation_penalty x crew mistakes)
       x low_altitude[target class](height above the ground at impact)
       x (TV track ? max(error_factor_min, 1 - error_penalty_per_reticle x mean aim error / reticle) : 1)
       x method_factor[guidance method]                  (N: 0.75; a method without one: 1)
       x (no range at launch and ПОМЕХА off ? no_range_fuse_factor : 1)
```

**Zones and methods (roadmap 6.7).** `zones` gives the NVA's firing zones (`flak11`): head-on for an approaching target
(the `zone` box's range limits, with an altitude band that depends on the target's speed: 25-5000 m up to 300 m/s,
100-5000 m up to 500 m/s), pursuit for a receding one (to 6.5 km, 100-5000 m, up to 300 m/s), and the optical "F"
zone for slow or hovering targets near the ground (up to 100 m/s, 2-6.5 km, 10-25 m). The ЦЕЛЬ В ЗОНЕ lamp and the
zone factor use them; without a `zones` block the one `zone` box applies to every target. `methods` picks the guidance
method the way the creator describes it: "F" for slow or hovering targets in the optical zone, "Gorka" (zoom climb) for
a low target (at or below `gorka_max_agl_m`) under `low_target_elevation_deg` (00-30 thousandths = 1.8 degrees),
"T/T" otherwise. The method is shown in the debrief; its effect on the chance is in `low_altitude`, which is calibrated
to the published single-missile kill probabilities listed above (fighter 0.39 at 50 m, strike aircraft 0.32 and
helicopter 0.10 at 10 m with the class chances in the file). "N" is used whenever ПОМЕХА is up (against a jammer,
or with the range by hand): `method_factor` gives its cost (three-point guidance on angles alone, the fuse armed early
with the medium beam; 0.75 is a game value). A missile launched with no range at all and ПОМЕХА off is never fused
(`no_range_fuse_factor` 0): the fuse is armed by range and there is none. Each head-on speed band may carry
`max_parameter_m`, the crossing parameter (how far the target's course passes from the unit over the ground): 6 km
at 300 m/s and 4 km at 500 m/s (`mon_2112_80` p10-12, `flak11`); beyond it the target is outside the zone (roadmap 9.1).

**Manual TV tracking (roadmap 6.6).** With a `tv` group the TV channel follows the wheels instead of driving them
(`ePap`: "only manual tracking"). A TV track holds while the target stays inside the lens field around where the
azimuth and elevation wheels point (the console's `fov_deg`, with the height `fov_height_ratio` of the width: a 4:3
raster); outside it the track is lost ("target left the TV field"). An NPC in a wheel's seat follows the target
`npc_delay_s` behind it, no faster than the aim rates; a wheel in the player's hands stays where the player puts it. A
target hidden by terrain is carried on at the wheels' last angular rates for `memory_s` ("memory tracking"), then lost.
The mean distance of the target from the crosshair (the 3-mil reticle, 0.18 degrees) over a missile's flight costs hit
chance, and the debrief prints it ("TV aim"). Close crossing targets are the hard case: at 1.5 km abeam a 150 m/s
target outruns the operators' lag. `track.clutter_altitude_m` (optional, 100 m, an estimate): a target lower than this
can't be held by the tracking radar unless the operator has its СДЦ on (`ssc.mti`; the capture drill switches it on).
Not done: drawing the reticle, and the range read from the target's size in it, in
the TV view (Unreal). Without the group the old behaviour stays: the servo drives the wheels onto the target.

Losing the track during the flight loses the missile (command guidance), and a shot whose flight time would exceed
`missile.max_flight_time_s` runs out. The debrief prints every factor and the random draw for each shot, and the
draw comes from the scenario seed, so a run can be repeated exactly.

## air_war.json (the enemy air command)

Game-balance numbers of the enemy air command (`simcore/air_war/`, `docs/air_war/`): the planner's utility weights, the
abstract attrition and strike model (with the guns around targets), the intelligence's ageing and deception, adaptation,
politics, the repair of what is wrecked, the day's weather, the pacing of the player's missions, the raid scenarios and
the mission bridge's weapon effects, and what our side learns. Tuned against the period's shapes in the balance pass
(`docs/air_war/09_BALANCE.md`, `simcore/examples/air_war_balance.cpp`).
Every key is listed with its default in `TuningTable::Defaults` (`simcore/air_war/data.hpp`); an unknown key fails the
load and `test_air_war` checks the file names them all.
