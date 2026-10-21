# 4. The planning cycle

Code: `simcore/air_war/planner.hpp` (`Planner::Plan`). Research §2, §6 (D1, D4). Design references: Falcon 4's air tasking
manager (`falcon4_campaign`), DCS Liberation's packages (`dcs_liberation_wiki`), GMT's *Red Storm* (`gmt_red_storm`).

## 1. When

Every `planning_cycle_h` (NATO 24, the Pact 12, the others 24), for times on target from `lead_time_h` to
`lead_time_h + planning_cycle_h` ahead. The opening plan is made before the war, so the first raids come at the start.
A pause from politics skips a cycle.

## 2. Tempo and apportionment (the commander)

Sorties available this cycle: each squadron's serviceable aircraft × sortie rate × the phase's tempo (a surge above 1:
the Pact's air operation flies 1.5 for its two days, then 0.85) × the cycle's share of a day. The packages a cycle may
hold scale with the phase's tempo too.
Tempo: `0.7 + 0.3 boldness + 0.3 pressure − 0.4 stress`, within 0.35..1.3, × the politics' sortie factor, minus the
on-call reserve.

Phases end on a day or on the believed air defences: the share of the radar air defences the enemy believes are there
that it believes still work, each weighed by the belief that it is there and real (a suspected dummy weighs a third).
A site found late counts like one known from the start, so the share falls as fast as known sites are destroyed, not as
fast as new ones are found. A phase that ended on the air defences comes back when they are believed `adapt.reopen_margin`
above its line again: the rollback resumes when the sites return.

The phase's apportionment is bent by character and situation (unless the commander is rigid or "by the book"): DEAD and
hunting up for the methodical and while the believed air defences mostly work; reconnaissance up when the picture is
stale; strikes up for the bold and under pressure; deception for the inventive; helicopters by the helicopter man's or
sceptic's habit. The bend is logged: "Apportionment bent: under pressure from above for results; the picture is stale".

## 3. Candidates (the intelligence officer's picture)

| Kind | Candidates |
|---|---|
| strike | believed targets that are not air defences, troops or towns |
| dead | located radar air defences and warning radars (uncertainty under 4 km) |
| hunter | radar air defences (located or not) |
| deception | radar air defences |
| recce | mobile entries seen more than 8 h ago or poorly located, or doubtful ones |
| bda | entries struck in the last 30 h and not seen since |
| cas | troops within 40 km of the front |
| helo_raid | anything but towns and air bases within 25 km of the front |
| harassment | anything but towns within 70 km of the front |

Banned kinds, the no-strike zone around towns and the depth limit remove candidates.

## 4. The utility of a way to fly it (the planner)

For each candidate × entry point × allowed height × allowed time of day (the next such time in the window):

1. **Build the package** from the template (§5).
2. **Expected losses** from every believed threat on every flight's route: the threat ring widened by half its
   uncertainty and its lethality spread over the wider ring; the share of a pass inside it; low fliers masked from
   long-range radar SAMs, medium fliers above the guns; suppression by the package's SEAD (scaled by the SEAD officer's
   skill), jamming, night and weather for optical defences, the aircraft's evasion; the defender's fighters where there is
   no escort. The same function (`PassKillChance`) the resolver uses with true values.
3. **Expected effect**: worth (the object's value × the faction's weight for its kind, + the grudge) × the belief it is
   there and working × the chance the aim point is right (mobile: `1 / (1 + (sigma / strike.stale_miss_m)²)`) × the chance
   the main flights get through × the strike's effect against its hardness.
4. **Utility** = `value_weight × effect − loss_cost × expected losses × loss aversion` + quirks (time of day, the same
   corridor or a new one, low or medium, weather, the imagery wait, re-strikes, helicopters) − the stale-target penalty
   for the methodical − the losses remembered that way + the success remembered + `exploration × creativity / sqrt(1 +
   uses)` + noise (`noise_at_zero_skill × (1 − planning skill)`). Loss aversion grows as boldness falls, as the faction's
   daily tolerance shrinks (by its square root), and as today's losses pass the tolerance.
5. The best option is kept if its utility passes `utility.min_value` and it is expected to achieve something
   (`utility.min_effect`; recce, BDA, hunting and deception always may).

## 5. The package

| Group | From the template | Timing |
|---|---|---|
| Main flights | `main_flights` × `flight_size` (more for massed blows, fewer for the drip feeder), the preferred types first | on target; successive flights minutes apart unless simultaneous |
| SEAD | `sead_per_threat` × believed radar threats on the route (more for the methodical, the "suppression first", after the SEAD lesson), up to `sead_max`, if ARMs are left above the hoarder's reserve | 3 min ahead |
| Escort | `escort_ratio` × main aircraft × the front's fighter threat | with the main flights, higher |
| Jammers | `jammers`, standing off on the own side of the line or with the package | from 10 min before to after |
| Decoys | `decoys` (doubled by the decoy man) when radar threats are on the route | 4 min ahead |
| Additional reconnaissance | `recce_lead` | 8 min ahead |
| Feint | 2 aircraft from another entry point, turning back short (the feint artist) | 12 min ahead |

Routes: entry → IP (offset) → target → egress (a turn away) → exit; helicopters hide 6 km short; hunters loiter round
the area; jammers orbit. Heights by profile (the aircraft's low or medium height). A squadron leader flies with his
squadron's main or SEAD flight.

## 6. Approval and interference

The commander rejects a package whose expected loss per aircraft exceeds `0.04 + 0.16 boldness + 0.08 pressure + 0.06 risk`,
lowered as today's losses use up the tolerance (an ambitious commander under heavy pressure approves anyway); the
planner stops offering a kind after two rejections. The political officer vetoes strikes near towns. Once a cycle, by the
faction's `interference_per_cycle` (less with a politically skilled commander), higher authority moves the most visible
strike one to three hours earlier: coordination halved, loads wrong or partial, the reasons logged (`hgram_080`).

Punctuality: a centralised doctrine and the "stopwatch" fly to the minute; others drift. Coordination of SEAD and strike:
`0.45 + 0.25 methodical + 0.2 SEAD skill + 0.1 punctuality − 0.2 stress`.

## 7. Reasons

Every package carries its reasons in its officer's words, logged when it is planned:

> dead '956 10' against SA-8 battery near Kesmark, D1 19:30 (dusk), in at N4, low: 4 dead, 2 sead, 1 escort, 1 jammer.
> expects to lose 0.5 of 8 aircraft for an expected effect of 1.6. this one has cost us 3 aircraft: the commander wants
> it gone. SEAD: 2 aircraft for 1 radar threat(s) believed on the route. stand-off jamming. the same way in as last time (N4).
