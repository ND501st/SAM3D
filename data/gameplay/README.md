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
```

Losing the track during the flight loses the missile (command guidance), and a shot whose flight time would exceed
`missile.max_flight_time_s` runs out. The debrief prints every factor and the random draw for each shot, and the
draw comes from the scenario seed, so a run can be repeated exactly.
