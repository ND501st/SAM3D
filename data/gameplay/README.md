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
helicopter 0.10 at 10 m with the class chances in the file). "N" (against jammers) is not modelled, and neither is
the NVA's "parameter" limit on crossing offset.

Losing the track during the flight loses the missile (command guidance), and a shot whose flight time would exceed
`missile.max_flight_time_s` runs out. The debrief prints every factor and the random draw for each shot, and the
draw comes from the scenario seed, so a run can be repeated exactly.
