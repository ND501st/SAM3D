# 1. Routes as they were flown

Written 15 November 2026 (roadmap track B, task B2). Code it will touch: `simcore/include/simcore/air_war/planner.hpp`
(`Planner::Build`, `BestPackage`, `Option`, `MainRoute`, `OrbitRoute`, `TimeRoute`, `ExpectedLosses`), a new
`air_war/route_planner.hpp`, `air_war/officers.hpp` (`TacticKey`), `air_war/state.hpp` (`RoutePoint`, `Flight`),
`air_war/resolve.hpp`, `air_war/raid_scenario.hpp`, `air_war/mission_bridge.hpp`, `air_war/save.hpp`; data
`data/air_war/faction_*.json` and the `route` group of `data/gameplay/air_war.json`. Research it rests on:
`docs/research/air_battle_2026-11/routing.md` (main), `reactions.md` (low-level evasion), `factions_cases.md`; code
maps `map/routes.md` (main) and `map/scale.md`. Source ids are those of the research files.

## 1. What a period route was

A route of 1985 was a line of turn points planned on paper maps before take-off and flown by map, stopwatch and the
navigation set, low over the ground the planners chose. Neither side flew a straight line to the target, and neither
side expected to fly round every defence.

### 1.1 The parts of a route

| Part | What it was | Source |
|---|---|---|
| Turn points | Over or close to features a crew can recognise: land and water contrast, a good radar return. The route as direct as possible to cut the time in reach of the defences, a procedure turn worked out for each point | `afm_51_40_1973` ch. 26 |
| Planned on the threats | Canadian CF-104s in the tactical air support role planned each route on the forecast weather and the known threats, SAM sites among them | `agard_cp267_1979` paper 23 |
| The IP | A fixed point chosen before the flight, from which the run on the target begins | `afm_51_40_1973` glossary |
| The let-down | Where a flight leaves its transit height for low level, in its own territory. Bombers flew low-level legs of 1-2.5 h, an A-10 40-60% of its mission under 500 ft. No source gives the let-down's distance from the front | `agard_cp267_1979` paper 4; a game value |
| The crossing | NATO: through gates and corridors at set times. The Pact: a checkpoint behind its own lines and a strict path and time through its own air defence | §1.2, §1.3 |
| The last leg | Low and fast; the Pact's ideal over enemy territory 50-100 m, 3-6 s to find the target, 10 s to clear it | `fm_100_2_1_1984` p. 12-8 |
| The way home | NATO: back under the established air defence procedures, through a gate. The Pact: the planned departure route | `fm_100_42_1976` §4-6d(5); `hines_1981` pp. 10-12 |

### 1.2 NATO: gates, corridors and time windows

- **Minimum Risk Routes.** The airspace control centre set up temporary routes through the friendly area for fast
  low fliers, built on the corps' advice from the friendly and enemy air defences, the fire plans and the terrain. The
  aircrew decided whether to use one. CAS sorties did not normally use them (`fm_100_42_1976` §4-6d, glossary).
- **Corridor suppression.** Where a mission crossed the FLOT, the Air Force asked the corps to suppress the enemy's air
  defence along its route and time, "usually disruptive", and passed any deviation at once (`jsead_1982` ch. 4).
- **Time windows.** At Red Flag (RAF Buccaneers there from 1977) 60-70 aircraft went through a series of corridors in
  20-30 minutes, deconflicted by precise timings, with no radio except a fighter warning. An RAF Buccaneer leader likened
  it to the way they would have crossed the inner-German border in war (`rafhs_j20_1999`, Pitchfork). This is an
  exercise in Nevada, not RAF Germany's own planning, and is used for the shape only.
- **The leader's freedom.** NATO crews chose whether to use an MRR and had a free choice of attack mode, direction, speed
  and release height (`fm_100_42_1976`; `rafhs_j20_1999`, a range freedom).
- **Why low.** The US judged medium-height penetration of Pact defences too costly and chose very low penetration, 200 ft
  or less (`crawford_1977_rand`). The FRG government said in 1987 that low flight cut the effect of the Pact's ground
  defences (`btd_11_558_1987`; its look-down argument names post-1985 fighters and is not used).

### 1.3 The Pact, as FM 100-2-1 sees it

All of this is NATO's view of the Soviet side, from 1984:

- The aviation commander's order gives the targets, sorties, approach corridors, codes and timing. Routes are chosen on
  large-scale maps and terrain models (`fm_100_2_1_1984` p. 12-3).
- Airborne aircraft go to a **checkpoint behind friendly lines and confirm the target with ground control**. They keep
  the original plan through friendly air defence unless ground control changes it (p. 12-4).
- The emphasis on strict timing and paths "indicates the probable use" of **safe corridors** through their own air
  defence. That is the US authors' inference, and the FM calls its division of the airspace by geography, height and
  time hypothetical (pp. 12-4, 11-11).
- Over enemy territory: 50-100 m ideally, radio minimal or forbidden, radar gaps used, decoy flights ahead, re-attacks
  from another direction or out of the sun (p. 12-8).
- Soviet open press of 1980, on airfield strikes: surprise by time, a hidden approach and the direction the enemy thinks
  least dangerous; approach and departure routes in the plan, rehearsed on sand tables and models "to the point of
  automatism" (`hines_1981` pp. 10-12, 17-18, after Tomilin 1980). The Soviet rules of 1985 class 0-200 m above the
  ground as extremely low, ПМВ (`opp_85` §45).

### 1.4 Low exposure, not clean avoidance

Both sides wrote that a route keeps exposure low; neither believed it could keep exposure at nothing.

- **Babich (1984):** a free bypass of the defences works "only in map exercises for staffs". One can count only on a
  route of minimum air defence activity, which needs real-time ELINT, radar cover and warning receivers, and every
  bypass ended inside the firing zone on the route's last stage (`mt_1984_babich`). A Russian teaching aid says the route
  should be the shortest and bypass the defences (`mai_tactics_vvs`, not a period document); Babich is used where they
  differ.
- **RAND (1977):** choosing the path past one missile site cut its intercepts by up to a factor of three, and terrain
  that held exposure to 20 s or less badly degraded mobile defences, if the sites' positions were known
  (`crawford_1977_rand` pp. 7-8).
- **MICOM (1980):** a US Army model chose a minimum-exposure, minimum-height route from line of sight to each sensor; the
  best route generally followed the lowest ground, and small changes in one weight gave an entirely different route.
  Its case is a helicopter force over a 35 x 35 km area, an air defence wargaming tool, not fixed-wing doctrine
  (`dorsett_1980`).

So SAM3D's routes minimise believed exposure weighted by time in each believed site's sight, prefer low ground, and
accept the target's own defence on the last leg. A battery sited on the target's last leg sees the raid whatever the
route did before it (routing-31).

### 1.5 A group ahead to blast a corridor

Where the route cannot avoid the defence, a group goes 1-2 minutes ahead to "blast a corridor" (`hines_1981`, after
Tomilin 1980, on airfield strikes). Babich gives the same as "overpowering": a suppression group forms a corridor and
the bombers follow in a column of flights at minimum intervals (`mt_1984_babich`). Shesterin wrote that strong missile
defences call for small echeloned groups at the lowest height with heavy jamming (`hines_1981`, after ВИЖ 1977). The
Soviet view of a NATO massed strike was a first echelon suppressing the air defence over a zone up to 120 km wide and
strike groups using the corridors it opened (`mt_1984_bobrov`).

### 1.6 The way home is another way

NATO's returning aircraft came back under established air defence procedures, through the friendly area's gates
(`fm_100_42_1976` §4-6d(5)). The Pact's plans gave departure routes as well as approach routes (`hines_1981`). Turns
off the target were the weak point (`nalty_1977`, Linebacker II; reactions-19), and Pact crews were to clear the target
area within 10 s (`fm_100_2_1_1984` p. 12-8). Linebacker II's predictable, repeated routes cost B-52s until routes were
varied (`dtic_linebacker`, a search excerpt only, low confidence). SAM3D: the exit is chosen on its own exposure and is
not the entry by rule.

## 2. Heights and speeds by faction, type and phase

Heights in feet and knots are as printed, converted (1 ft = 0.3048 m, 1 kt = 0.5144 m/s). "Sourced" means the
range is from the source; the value a type takes inside it is a game value. Peacetime floors (UK 250 ft, FRG band
150-450 m with seven 75 m areas: `hansard_lowflying`, `btd_10_2980_1985`, `btd_11_558_1987`) are not the war: the
period expected war heights to be lower than training (`agard_cp267_1979` paper 4).

| Faction | Types (data ids) | Phase | Height | Speed | Source | Kind |
|---|---|---|---|---|---|---|
| NATO | fast jets | own side to the let-down | the type's `medium_alt_m` | `cruise_mps` | none | game value |
| NATO | `tornado_ids`, `f16a`, `f4e`, `f111f` | ingress by day | 30-75 m (100-250 ft) | 216-247 m/s (420-480 kt) | `rafhs_j20_1999` (100 ft four-ships), `afm_51_40_1973` (200-500 ft normal), `agard_cp267_1979` p. 4-3 (FB-111 0.63-0.73 M) | sourced range |
| NATO | `f111f` | ingress at night or in cloud | 61 m (200 ft) on TFR | 247-278 m/s (480-540 kt) | `crandall_twz` | sourced |
| NATO | `tornado_ids` | ingress at night or in cloud | 152 m (500 ft) on TFR | 216 m/s (420 kt) | `napier_hushkit_2022` (undated, maybe after 1985) | low confidence |
| NATO | `a10a` | ingress and attack | under 30 m (100 ft) | 103-154 m/s (200-300 kt) | `agard_cp267_1979` p. 4-3 | sourced |
| NATO | fighter-bombers | last leg, laydown | 15 m (50 ft) | 283 m/s (550 kt) | `agard_cp267_1979` p. 23-1, the CF-104 strike role of about 1963-72 | lineage only: game value |
| NATO | `rf4c` | the recce run | 152 m (500 ft) | 216 m/s (420 kt) | the CF-104 reconnaissance role, pre-period | lineage only: game value |
| NATO | all | egress | as low as ingress | full speed | `pook_1985` (Harriers, 1982: ultra-low, full speed) | sourced shape, game values |
| NATO | `f4g`, `f4e` hunters, `ef111a` | SEAD and jamming | the template's profile (medium, low) | `cruise_mps` | none for 1985 | game value |
| Pact | all jets | own side, checkpoint, corridor | strict path and time | strict time | `fm_100_2_1_1984` p. 12-4 (height not given) | game values |
| Pact | `su24`, `mig27`, `su17m3`, `su25`, `mig23mld` | over enemy territory | 50-100 m | "low, fast", no figure | `fm_100_2_1_1984` p. 12-8; inside ПМВ 0-200 m, `opp_85` §45 | sourced height, game speed |
| Pact | strike | the target | 3-6 s to identify, out of the area in 10 s | `attack_mps` | `fm_100_2_1_1984` p. 12-8 | sourced times |
| Pact | suppression group | ahead of the strike | the route's height | — | 1-2 min ahead, `hines_1981` | sourced lead |
| general | low fliers | — | 60-90 m gave over 75% survival; under 30 m or over 200 m under 50% | — | `mt_1984_babich` (a Soviet reading of US Vietnam data) | supports the bands above |

Not used for this war: the Su-25's and Su-17's Afghan attack heights of 600-2,500 m and the Su-17 designation group at
800-1,000 m and 850-900 km/h (`markovsky_afghan_sky`), which answered guns alone and may straddle
1985; Vulcan and B-52 profiles (no such types in the theatres). No period source gives a Pact low-level speed, a
let-down distance, a leg length, a turn-point count, an IP distance (the SAAF's 4-5 nm pull-up is 1987-88) or a fixed
stand-off from a SAM ring (routing §3). Those stay game values in `data/gameplay/air_war.json` `route.*`, labelled so.

**In the data.** Today each type has one `low_agl_m`, and most NATO and Pact values are marked placeholders in
`faction_*.json`. B2 sets them from this table with `source` and `confidence`, and adds per type `low_agl_night_m`
(TFR types) and `low_mps` (the ingress speed, default `attack_mps`). The routing block of each faction (§6.7) holds the
phase rules.

## 3. Helicopters

- **NATO** (`fm_17_50_1977` ch. 3): low level in the rear where radar cannot acquire, contour forward of the division's
  rear boundary, NOE near the enemy. NOE is a broad corridor planned on terrain features and flown as a weaving route
  from a holding area to a battle position. NOE is seldom flown at night, when contour or low level is flown. The
  enemy's air defence ambushes NOE routes. The scan's "35 s or less" unmasked against the ZSU-23-4 is garbled; SAM3D
  uses RAND's 20 s (routing §4, item 8).
- **The Pact** (`fm_100_2_1_1984` pp. 12-5, 12-8, NATO's view): the Soviets prefer to use helicopters in ground support
  only to the range of their own air defence umbrella, a preference, not a rule. Helicopters, aircraft and artillery
  normally avoid the same fire zone at the same time; when helicopters and artillery work together, the helicopters get
  entrance and exit corridors between the artillery concentrations. The Afghan heights (armed reconnaissance at
  1,500-1,700 m, `airwar_mi24_markovsky`) answered guns, not a SAM defence, and are not used against our side.
- **SAM3D:** a helicopter route is a holding area behind the front, a weaving corridor scored on low ground, woods and
  masking, and a battle position (today's `hide` point) chosen where the believed threats cannot see it at hover height
  and the target can be seen at pop-up height. The holding area's distance and the umbrella's reach are game values
  (`route.helo_holding_m`, `route.helo_umbrella_m`); the Pact's planner scores a helicopter target beyond the umbrella
  down rather than forbidding it. At night a helicopter flies the contour height (`route.helo_night_agl_m`).

## 4. The neutral and the rebels

No period source on either air arm's routing was found (routing-29, -30). What follows is design inference, low
confidence, and every number is a game value.

- **The neutral** (Saab 105Ö, AB 212, Alouette III): short, simple low-level routes inside its own airspace up to the
  border, two turn points at most, by day, NATO-style map and stopwatch navigation. The crossing is an entry point with
  no window. It comes home the shortest covered way.
- **The rebels** (An-2, UTVA, Galeb, armed Mi-8): at night, low, navigating by roads, rivers and villages, keeping clear
  of known guns, one target per sortie, no gate and no corridor, home the way they came.
- The lower bound of a Soviet-trained client is in the record: Iraq kept rigid profiles under tight ground control,
  only the leader knowing the destination (`cia_1987_iraq_af`). It informs the Pact's strict deviation rule (§9), not a
  faction of its own.

## 5. What the code does today

From `map/routes.md` §3: every route is one of four shapes in `planner.hpp`. `MainRoute` (planner.hpp:863) is entry, an
IP 20 km short and a quarter of that to the left, the attack's points, the target, an egress 12 km to the left, and an
exit that is the entry (`o.exit = ei`, planner.hpp:1083). It is timed at `attack_mps` throughout from a hard-coded
anchor (`TimeRoute(r, 2 + attack.size(), …)`). `LoiterRoute`, `OrbitRoute` and `MissileRoute` are as simple. Believed
threats price a route (`ExpectedLosses`, `RouteChord`, `PassKillChance`) and size its SEAD, but never shape it.
`PlanContext` has no ground; `AirWarData::country` (data.hpp:2079) is loaded on the nation's theatre and read only by the
supply lines. The only lateral choice is which of a front's 3-4 entries to use.

## 6. The design: a route planner

### 6.1 What it is

`RoutePlanner` (new, `air_war/route_planner.hpp`) builds one flight's route as a list of `RoutePoint`s from the
believed picture and the ground. It is a pure function: it reads `IntelPicture::Threats()` (intel.hpp:174), the country
map and the request, and draws no random number.

```cpp
struct RouteRequest {
    const EntryPoint* gate_in = nullptr;    // the front's entry the flight crosses by
    const EntryPoint* gate_out = nullptr;   // where it comes back (may differ)
    Vec3 target;                            // the believed aim
    double release_short_m = 0.0;
    Delivery delivery = Delivery::Count;
    const AircraftType* type = nullptr;
    Profile profile = Profile::Low;
    DayPart part = DayPart::Day;            // night heights and the helicopter's contour
    FlightRole role = FlightRole::Strike;
    int variant = 0;                        // 0 covered, 1 other flank, 2 direct (§6.4)
    double ip_dist_m = 20000.0;             // game value as today (6 km for helicopters)
};
struct PlannedRoute {
    std::vector<RoutePoint> points;         // tagged, untimed (TimeRoute times them)
    std::string corridor;                   // a stable name for the officers' memory: "S2 via <feature id>"
    double exposure_s = 0.0;                // believed seconds in sight of a believed threat, weighted by lethality
    double length_m = 0.0;                  // from the front's bases and back, for the reach check
    std::vector<ZoneEscape> escapes;        // the Pact's planned counter-SAM programme per zone crossed (§9)
};
PlannedRoute RoutePlanner::Plan(const RouteRequest&, const RouteRules&, const PlanContext&) const;
```

`RoutePoint` (state.hpp:18) gains `leg_mps` (the speed on the leg from this point to the next) and `feature` (the
feature's id, empty for a computed point). The tags the rest of the code reads are kept: `entry` first and `exit` last,
`ip` (or `hide` for a helicopter) before the attack's `pull_up`/`apex`/`roll_in`, exactly one `target`, then `egress`.
New tags: `letdown`, `checkpoint` (the Pact), `gate` (the front crossed, out and back), `turn` (a turn point on a
feature) and `climb` (back to transit height on the own side). A route's order:

```
entry ─ letdown ─ [checkpoint] ─ gate ─ turn … turn ─ ip ─ [pull_up, apex | roll_in] ─ target ─ egress ─ turn … ─ gate ─ climb ─ exit
```

`entry` is on the enemy's own side, `route.start_before_gate_m` back along the line from the front's `bases` to the gate
(a game value); `letdown` is `route.letdown_m` before the gate (a game value per faction).

### 6.2 The features

Turn points and IPs sit on features of the country map (`country_map.hpp`), in the theatre's ENU frame, which is the
country map's frame as `logistics.hpp` already uses it:

| Feature | From | Id |
|---|---|---|
| Villages, towns, cities | `CountryDef::settlements` | `settlement:<id>` |
| Bridges | `CountryMap::Crossings()` | `crossing:<id>` |
| Road and rail junctions | nodes of `Network(Road)` and `Network(Rail)` with three or more edges | `junction:<net>:<nearest settlement>:<n>` |
| River bends | vertices of `Def().ground.rivers` where the line turns more than `route.bend_deg` | `river:<id>:<k>` |
| Lake ends | the two shore points of `ground.lakes` farthest apart | `lake:<id>:<a or b>` |
| Landmarks | `CountryDef::landmarks` (dams, refineries, crossings) | `landmark:<id>` |

Features are built once per map at load into a uniform 10 km bucket grid and sorted by id, so iteration never depends
on container order. Ids come from the map's data, never from node numbers saved anywhere: a changed map needs no route
edits, the rule track G keeps for the roads.

**The tuned theatre** (`theatre_pannonia.json`) has no country map. There the features are the theatre's own objects that
are not believed threats (towns, bridges, depots: `TheatreObject::pos`) plus a lattice at `route.fallback_grid_m`, the
ground is flat, and masking is off. Routes still bend round the believed rings; they are not claimed to be on features.

### 6.3 What a leg costs

A leg from feature a to feature b costs its flying time plus its believed exposure:

```
cost = t_leg_s × (1 + w_heading)                                       // dorsett_1980's heading window, a game method
     + w_exp × Σ_threats  lethality × Live() × seen_s(leg, threat)     // believed seconds in sight
     + w_cont × Σ_threats  max(0, longest_continuous_s − 20)            // crawford_1977_rand: 20 s or less
     + w_low × mean(ground under the leg − corridor's lowest ground)   // Low only: the lowest ground (dorsett_1980)
```

- `seen_s` is the time the leg spends inside the threat's ring (`threat_radius_m + 0.5 σ`, as `ExpectedLosses` widens
  it) **and** in its line of sight at the profile's height. For a Low flight on a map with ground, sight comes from a
  viewshed of each believed threat at a few height classes (`route.mask_agl_classes_m`), on the country grid, cached by
  (threat key, believed position, σ, class). Medium and High flights, and every flight on the tuned theatre, are seen
  throughout the ring, as today.
- **The target's own defence is not avoided.** Threats whose ring holds the target cost nothing on the legs from the IP
  on: every route ends in the target's firing zone (`mt_1984_babich`). They are still priced in `ExpectedLosses`.
- `w_heading` is 0 for a leg within 90° of the bearing to the goal and 1 behind it, 45° inside `route.final_cone_m`
  (`dorsett_1980` §4.5-4.6).
- `w_exp`, `w_cont` and `w_low` are game values per faction (`RouteRules`), moved by the planner's traits (§7.3).

### 6.4 The search and its variants

- **The graph.** Features inside an ellipse about the gate and the target (half width `route.corridor_share` of their
  distance, at least `route.corridor_min_m`), at most `route.max_features`, each joined to its `route.neighbours`
  nearest. A leg is allowed if it is at least `route.min_leg_turns` turn radii long (textbook: 480 kt at 60° bank is a
  radius of about 3.6 km, routing §3) and at most `route.max_leg_m`; the turn at each point is at most
  `route.max_turn_deg`. A route has at most `rules.max_turn_points` turns. All game values.
- **Ingress:** Dijkstra from the gate to the IP. The IP is the feature nearest the point `ip_dist_m` short of the
  target on the run-in heading, within `route.ip_snap_m`; the run-in heading is the one whose last leg costs least among
  `route.run_in_headings` tried round the target. With no feature in reach the IP is the computed point (`feature`
  empty).
- **Attack:** the attack's points are laid out as `MainRoute` lays them today, but along the IP's run-in heading
  instead of the entry's bearing, and the side of the pull-up offset is the side away from the cheaper egress.
- **Egress:** the `egress` point is ahead on the run-in at least 10 s of flight past the release
  (`fm_100_2_1_1984` p. 12-8 for the Pact; the same for all as a game value), turned to the cheaper side; then Dijkstra
  to the exit gate.
- **Variants** (the option loop's new dimension): 0, **covered**, the least cost; 1, **other flank**, the least cost
  with variant 0's turn points charged `route.reuse_penalty` (a different corridor); 2, **direct**, with `w_exp` cut to
  `route.direct_exposure_share` ("as direct as possible", `afm_51_40_1973`). The return search charges the ingress's
  turn points the same penalty, so the way home is a different way when one is cheaper.
- **Ties** are broken by feature id, never by order of discovery.

### 6.5 Gates, windows and the group ahead

- **The gate book.** Each planning cycle keeps, per front and gate, the crossing times already committed
  (`GateBook` in `PlanContext`). `Build` times the route, reads the crossing time at the `gate` point and, if another
  package holds the gate within `rules.gate_separation_s`, moves this option's ToT later to the first free slot inside
  the planning window or drops the option. Packages on different axes are kept apart by gate and time, not by height
  (routing §1). `Commit` writes the reservation. Missions fill in the planner's fixed order (planner.hpp:338-341), so the
  book is deterministic.
- **NATO** uses the gate for every package except CAS (`fm_100_42_1976`: CAS does not normally use MRRs). The package
  carries its `gate_window` (the crossing time ± `rules.gate_window_s`). Corridor suppression (`jsead_1982`) is written
  as a request on the package; in the abstract resolver our defences within `route.corridor_halfwidth_m` of a gate during
  its window take `route.corridor_suppression` against their radars, NATO only and only on a front where land forces
  face each other. That is a game value on a doctrine source, and B5 owns whether it reaches a played session.
- **The Pact** adds a `checkpoint` `route.checkpoint_before_gate_m` short of the gate, where the leader confirms the
  target with ground control (`fm_100_2_1_1984` p. 12-4; the call and its delay are `05_C2_AND_RETASKING.md`'s and
  `09_COMMS_AND_INTERCEPT.md`'s). The gate is the corridor through its own air defence, kept to time
  (`centralisation` 0.9 already holds the jitter small).
- **The group ahead.** When the covered variant's believed exposure outside the target's own zone exceeds
  `rules.blast_exposure_s`, `Build` routes the package's suppression flights (the code's `Sead` and `Dead` roles) on the
  same corridor, `rules.blast_lead_s` ahead of the main flights (the Pact 60-120 s, `hines_1981`; NATO a game value on
  `mt_1984_bobrov`'s view), each aimed at the believed threat costing most on the corridor (`CorridorThreat`), not at
  the target. Today SEAD flies the target's route 0.05 h (3 min) early whatever the route (planner.hpp:721).

### 6.6 Timing: `TimeRoute`

`TimeRoute(r, anchor_h)` anchors at `ReleaseIndex(r)`, the `target` tag, for any shape, and times every leg at its own
`leg_mps`: `cruise_mps` from `entry` to `letdown` and from `climb` to `exit`, `low_mps` from `letdown` to the IP (the
type's `attack_mps` when it has no `low_mps`), `attack_mps` from the IP to `egress`. A helicopter flies `cruise_mps`
throughout as today. The old signature stays for `LoiterRoute` and `OrbitRoute` until they are planned too.

### 6.7 The rules per faction (data)

Each faction's doctrine gains a `routing` block with `source` and `confidence`. The kinds come from §1-4; the numbers
are game values unless a source is named.

| Key | NATO | Warsaw Pact | Neutral | Rebels |
|---|---|---|---|---|
| `crossing` | `gate` (CAS free) | `checkpoint_gate` | `entry` | `none` |
| `gate_window_s` | 300 | 120 | — | — |
| `gate_separation_s` | 120 (a game value on Red Flag's 60-70 aircraft in 20-30 min, `rafhs_j20_1999`) | 120 | — | — |
| `letdown_m` | 60 km | 40 km | 20 km | 10 km |
| `max_turn_points` | 6 | 6 | 2 | 4 |
| `variants` | 0, 1, 2 | 0, 1, 2 | 0, 2 | 0 |
| `w_exp` (exposure against time) | high | high | middle | high for guns too |
| `lines_bonus` (legs along roads and rivers cost less) | 0 | 0 | 0 | 0.5 |
| `return` | `other_gate` (`fm_100_42_1976`) | `planned_departure` (`hines_1981`) | `same_entry` | `same_entry` |
| `blast_lead_s` | 120-300 | 60-120 (`hines_1981`) | — | — |
| `blast_exposure_s` | 20 | 20 | — | — |
| `deviation` | §9 | §9 | §9 | §9 |

Day parts: the rebels' strike and harassment already fly at night by their templates; the neutral's by day.

## 7. Into the planner

### 7.1 `Planner::Build`

`Build` calls the route planner where it calls `MainRoute` today (planner.hpp:676, 721, 749, 768, 790, 808, 830). The
main flights fly the option's variant; escort, decoy and recce flights the main flights' corridor at their own heights
and leads (as now, the escort a close escort); SEAD as §6.5; the feint from the next gate to 75% of the way and back by
the same planner. `OrbitRoute`, `LoiterRoute` and `MissileRoute` keep their shapes, timed by the new `TimeRoute`; a hunter's
way in and out of its loiter circle is planned. Within one `Build` the planner's results are cached by
(gate, exit, target key, type class, profile, part's height class, variant), so several flights of a package share one
search.

### 7.2 The option loop

`Option` (planner.hpp:571) gains `variant`, and the loop (planner.hpp:1072-1180) tries each variant the faction allows
and, for each, the exits `rules.return` allows: the entry and the gate with the cheapest return, so the exit is no longer
the entry by rule. Options grow at most six-fold; before `Build`, an option whose route's believed cost is more than
`route.prune_ratio` times the best of its (entry, profile) is dropped. `ExpectedLosses` and `RouteChord` already price
any polyline.

### 7.3 `TacticKey`: officers remember corridors

`TacticKey` (officers.hpp:35) becomes `gate|corridor|exit|profile|part|kind`, where `corridor` is `PlannedRoute::corridor`,
named after the gate and the feature of greatest lateral offset (`S2 via settlement:kovar`). So `pln.tactics` remembers
losses and success per corridor, `axis_repeat` and `axis_vary` (planner.hpp:1133, 1167) compare corridors against
`pln.last_corridor`, and the exploration term favours untried corridors. The planner's traits weight the route's
choice through the cost weights: methodical raises `w_exp`, boldness lowers it, rigidity raises the bonus for a
remembered corridor, creativity the exploration term; each by at most about ±0.2 of the faction's value
(`factions_cases.md`, design proposal). A loaded `tactics` map with old keys is read as unknown corridors.

### 7.4 The reach check

The reach check (planner.hpp:838-856) measures `PlannedRoute::length_m` from the front's `bases` to the release and back
to `bases` (a helicopter from its holding area), not `Dist2d(entry, target) + Dist2d(bases, entry)`. A long covered route
can need a tanker the direct one does not, which is the trade the period made.

## 8. Couplings to `MainRoute`'s shape that must be fixed

From `map/routes.md` §11.3, each fixed with a test:

| Where | Assumes | Fix |
|---|---|---|
| `TimeRoute(r, 2 + attack.size(), …)` (planner.hpp:912) | the target's index | anchor at `ReleaseIndex`; per-leg `leg_mps` (§6.6) |
| The resolver's fighter roll at `si == 1` (resolve.hpp:232) | leg 1 runs from the IP | roll on the leg that starts at the `ip` (or `hide`) point; B8 replaces the roll |
| `ExpectedLosses`' stand-off jammer test `route[1].tag == "orbit"` (planner.hpp:990) | the orbit is point 1 | any point tagged `orbit` |
| `radar_threats` reads the first main flight only (planner.hpp:691) | one shared path | the union over the main flights, each threat once |
| The reach check (planner.hpp:840-850) | straight lines | the route's length (§7.4) |
| The raid builder's one speed per clipped route (raid_scenario.hpp:475-478) | constant speed | a speed per waypoint from the `t_h` differences; the scenario's waypoints gain an optional `speed_mps` |
| The raid builder's sideways offset from the first clipped leg (raid_scenario.hpp:441-448) | a straight line | until B1 flies formations, each waypoint's offset at right angles to the bisector of its two legs |
| `MissionBridge::Engage`'s pass chance on the present heading (mission_bridge.hpp:539) | straight on through the zone | the chord of the remaining planned polyline inside the zone |
| The bridge's release window (mission_bridge.hpp:424-459) | the run along the entry's bearing | unchanged, since the attack's points now lie on the IP's run-in (§6.4); the test checks it |
| The PU-12's raid azimuth from the first clipped point (raid_scenario.hpp:851-911) | none | unchanged; the warning's bearing is where the raid enters the circle |

`test_packages_live.cpp:159` (an injected raid flies as the same raid played alone) must hold unchanged.

## 9. Leaving the route in flight (handed to the command layer of `02_FLIGHTS_AND_PILOTS.md`)

The planner writes the rules; the flight's command layer (`02_FLIGHTS_AND_PILOTS.md`) applies them when a threat the
plan did not know appears, and `04_REACTIONS.md` flies the defensive manoeuvre itself. A deviation for a pop-up threat
is not a retasking: a change of target or of the route's end is `05_C2_AND_RETASKING.md`'s.

| | Who may | How far | Back to | Source |
|---|---|---|---|---|
| NATO | the flight leader | off the leg by up to `deviation.lateral_m` (game value, 10 km), keeping the ToT within `deviation.tot_slip_s` (60 s), never into another package's gate window | the next turn point, or straight to the IP | the leader's choice of MRR and attack (`fm_100_42_1976`, `rafhs_j20_1999`); precise timing (`rafhs_j20_1999`) |
| Warsaw Pact | nobody, except the planned counter-SAM programme; any other change only on ground control's order | the programme: full speed to the zone's nearest boundary, its start and end planned before the flight from the believed sites | the planned route at the programme's end | `fm_100_2_1_1984` p. 12-4; `sve_prm` (reactions-34) |
| Neutral | the leader, by eye | a break and a zigzag, `deviation.lateral_m` 3 km | the planned route | design inference (reactions-41 to -44) |
| Rebels | each pilot | anything, the route may be abandoned for the nearest road or river home | none | design inference (routing-30) |

For the Pact the route planner writes the programme into the flight: one `ZoneEscape {threat key, first and last route
index inside the ring, heading to the nearest boundary, height change, full speed, jamming on, rejoin index}` per
believed zone the route crosses; `04_REACTIONS.md` §6.2 flies it. A threat not in the plan has no programme, and the
Pact flight holds its route until ground control speaks or the reaction's own break (`04_REACTIONS.md`) is over.
Officers' and pilots' traits move `lateral_m` and the chance of using it by the bounds of `10_FACTIONS.md` §8; a rigid
Pact leader stays rigid. Every deviation flown in a session returns to the campaign as the route actually flown (the
outcome of `02_FLIGHTS_AND_PILOTS.md` §2.4), so the abstract rest of the raid is resolved on it, not on the plan.

## 10. Determinism and cost

- **Pure.** `RoutePlanner::Plan` reads the data, the intel picture as it stands, the map and the request, and draws
  nothing. Features and threats are iterated in id order; no `unordered_` container decides an order. The caches (the
  features' grid, the viewsheds, the per-cycle route cache) are caches only: a test plans a cycle cold and warm and
  compares every point.
- **The option noise.** Today `BestPackage` takes one `rng->Normal` per option from the campaign's stream
  (planner.hpp:1152), so more options would shift every later draw. Each option's noise is taken instead from
  `c_->rng->Fork(OptionKey(...))`, the key an FNV-1a hash of the candidate's key, gate, exit, variant, profile, part and
  size (never `std::hash`, which differs between standard libraries). `Fork` is const (types.hpp:433), so the main
  stream advances by nothing in the option loop; `Commit`'s jitter and `Interference` keep their draws.
- **Saves.** A changed planner makes old saves meet packages they were not made with (`map/routes.md` §12). The tuning
  key `route.mode` (0 the old shapes, 1 the route planner) is written into the save; a save without it loads with mode 0
  and replays exactly as before; `save_version` becomes 2. Mode 0 stays for one release and for A/B balance runs.
- **Sessions.** A route is waypoints by the time it reaches a session; nothing here runs in `ScenarioRunner::Step`.
  The levels of detail (A0-A2) and the in-flight deviation are `02_FLIGHTS_AND_PILOTS.md`'s and cost nothing here.
- **Cost** (SAM3D estimates, to be measured): the features' grid once per map; viewsheds for Low flights, cached across
  cycles while a belief does not move, at most about 50 ms per planning cycle on the nation's theatre; one search at
  most about 1 ms with 400 features and 8 neighbours each; searches shared within a package. `air_war_balance` on the
  nation's theatre may take at most 1.5 times its present wall time, measured before and after.

**Balance.** Masked exposure prices a Low route below its open chord. To keep the planner's estimate and the resolver's
truth one rule (`map/scale.md` §6), `PassKillChance`'s exposure for a Low flight on a map with ground becomes the chord
seen in the defence's line of sight (the resolver with true positions, the planner with believed ones), and
`attrition.low_vs_long_range_sam` applies only where there is no ground. That moves `air_war_balance`, `air_war_bench`
and `docs/air_war/09_BALANCE.md` §8-10; every moved row is explained in the PR.

## 11. Data and tests

- **Data.** `faction_*.json`: `doctrine.routing` (§6.7, §9) and per type `low_agl_m`, `low_agl_night_m`, `low_mps` from
  §2 with sources. `air_war.json`: the group `route` (`mode`, `start_before_gate_m`, `bend_deg`, `fallback_grid_m`,
  `mask_agl_classes_m`, `final_cone_m`, `corridor_share`, `corridor_min_m`, `max_features`, `neighbours`,
  `min_leg_turns`, `max_leg_m`, `max_turn_deg`, `ip_snap_m`, `run_in_headings`, `reuse_penalty`,
  `direct_exposure_share`, `direct_slack`, `prune_ratio`, `checkpoint_before_gate_m`, `corridor_halfwidth_m`,
  `corridor_suppression`, `helo_holding_m`, `helo_umbrella_m`, `helo_night_agl_m`), every one labelled a game value. New sources in
  `docs/SOURCES.md` from routing §6 (`afm_51_40_1973`, `agard_cp267_1979`, `crawford_1977_rand`, `dorsett_1980`,
  `fm_100_42_1976`, `fm_17_50_1977`, `rafhs_j20_1999`, `crandall_twz`, `napier_hushkit_2022`, `hines_1981`,
  `mt_1984_bobrov`, `opp_85`, the Bundestag and Hansard answers); `tools/check_citations.py` passes.
- **Tests** (`simcore/tests/test_routes.cpp`, new, fast, on small synthetic country maps; on devloop in the `air_war`
  area): see Done when.

## Done when

1. On a synthetic map with a valley and a believed Osa on the ridge beside it, a Low route takes the valley, its
   believed exposure is below the straight line's, and no continuous exposure to the Osa exceeds 20 s.
2. With no believed threats a route's length is within `route.direct_slack` of the straight line, and every `turn` and
   the `ip` carry a feature id on the nation's map.
3. Every planned route has `entry` first, `exit` last, one `target`, the IP before the attack's points; legs are at
   least the minimum length and turns within the limit; the attack's points lie on the IP's run-in line.
4. A target inside a believed ring is attacked from inside that ring: the last leg is not bent out of it.
5. A NATO package returns through another gate when its egress is cheaper there; two NATO packages in one cycle cross
   one gate at least `gate_separation_s` apart. A Pact route has a `checkpoint` behind the line before its gate.
6. A Pact package whose covered corridor is exposed beyond `blast_exposure_s` sends its `Sead` or `Dead` flights 60-120 s
   ahead on the same corridor, aimed at the corridor's worst believed threat; every Pact flight carries a `ZoneEscape`
   for each believed zone its route crosses.
7. A helicopter's battle position is unseen from every believed threat at hover height and sees the target at pop-up
   height; a Pact helicopter target beyond the umbrella scores lower than the same target inside it.
8. An officer with `axis_vary` takes a different corridor the cycle after one, and `pln.tactics` holds corridor keys.
9. The same seed gives byte-identical packages twice; a cycle planned with cold and warm caches gives identical routes;
   the main `Rng`'s state after the option loop does not depend on how many options were tried.
10. A save made before B2 loads with `route.mode` 0 and replays exactly (`test_air_war.cpp:943` with a fixture); a
    mode 1 save replays exactly.
11. `TimeRoute` puts the `target` at the ToT for every shape, and per-leg speeds give the hand-computed times.
12. The fixes of §8 each have a test: fighter roll leg by tag, stand-off jammer by tag, reach by route length (a
    dogleg beyond the radius needs a tanker), `radar_threats` over all main flights, per-waypoint speeds and offsets in
    the raid scenario; `test_packages_live.cpp:159` still passes.
13. `air_war_balance` (both theatres) and `air_war_bench` re-run with every moved row explained, within the cost bound
    of §10; `ground_check data 3600` lists nothing; `crew_balance` and `em_demo` unchanged or explained.
14. `python tools/map_view.py` (or `air_war_view.py`) draws a NATO and a Pact package's routes on the nation's map, and
    they follow valleys, cross at their gates and come home another way: the first item of milestone M-B.

## As built (B2, 17 Nov 2026, branch `tb-b2`)

**Files.** `simcore/air_war/route_planner.hpp` (new): `TimeRoute`, the heights and speeds (`RouteLowAgl`,
`RouteProfileAgl`, `RouteLowMps`), `RouteGround`, `RoutePlanner`, `BelievedRouteThreats`. Changed: `state.hpp`
(`RoutePoint::leg_mps` and `feature`, `ZoneEscape`, `Flight::escapes`, the package's `corridor`, `variant`, `gate_h`,
`gate_window_s`, `corridor_suppression`), `data.hpp` (`RouteRules`, the type's `low_level`, the `route.*` defaults),
`officers.hpp` (`TacticKey`, `last_corridor`), `planner.hpp`, `resolve.hpp`, `campaign.hpp`, `save.hpp`,
`mission_bridge.hpp`, `raid_scenario.hpp`, `flights.hpp` (`PhaseOfTag`), `examples/air_war_demo.cpp` (the routes as
`map_view.py --routes` draws them). Tests: `simcore/tests/test_routes.cpp` and the fixture
`tests/fixtures/air_war_save_v1.json`.

**The mode.** The tuning's `route.mode`: 1 (the default) plans routes; 0 keeps every pre-B2 path to the bit. A save is
`save_version` 2 and writes its `route_mode`; a version 1 save loads with mode 0 and replays exactly (the fixture test).

**The ground.** `RouteGround` takes the country map's features when the theatre names one: settlements, crossings,
landmarks, and road and rail junctions of three ways or more (named after the nearest settlement). Without a map it
takes the theatre's fixed objects that are no threat and a lattice (`route.fallback_grid_m`). A site's viewshed is the
least height it sees per ray and range step (`route.view_step_m`, the earth's curvature at 4/3, the antenna
`route.mask_antenna_m` up), cached by site and ring. Masking counts only for Low flights and helicopters on ground.

**The search.** The graph is the features near the lines gate-target and target-exits (`route.corridor_share`, at least
`route.corridor_min_m`, at most `route.max_features`). Each joins its `route.neighbours` nearest between
`route.min_leg_turns` turn radii and `route.max_leg_m`. A leg costs
`t (1 + w_heading) (1 - lines_bonus if on a road or river) + w_exp Σ weight seen/v + w_cont Σ max(0, run/v - route.continuous_s) + w_low t (ground - lowest)/100 m`.
`w_heading` is 1 when the leg turns more than 90° off the goal (45° inside `route.final_cone_m`). Dijkstra runs with
`route.max_turn_deg` and the faction's `max_turn_points`, ties by node index. IP candidates sit at
`route.run_in_headings` headings round the target, snapped to a feature within `route.ip_snap_m`. The run-in from the
IP is straight and the target's own zone is not charged from the IP on. The egress turns `route.egress_turn_deg` to
the cheaper side for `route.egress_m`. Home is a second search from there to the cheapest exit the faction's `return`
allows, with the way in's features charged `route.reuse_penalty_s`. A helicopter has a holding area
(`route.helo_holding_m`) and a battle position hidden at `route.helo_hide_agl_m` that sees the target at
`route.helo_popup_agl_m` (`route.hide_penalty_s` if not). A hunter's loiter circle keeps its shape; its way in is
planned and its way out is the way in reversed. The planner officer's traits scale `w_exp` (methodical up, bold down).
A feature flown over within `route.straight_deg` (20°) of straight on is no turn point: it does not count against
`max_turn_points` and is not listed as a turn point, but the route still flies over it. Only features within
`route.simplify_m` (500 m) of a straight line between the kept points are left out of the points, so the shape flown
is the one searched to that much. Without this, over the country's
dense villages every feature passed counted as a turn, far targets (most SAM sites) could not be reached, and DEAD
packages lost their SEAD.

**Variants.** 0 covered. 1 the other flank: variant 0's turn points charged the reuse penalty; dropped if it costs more
than `route.prune_ratio` times variant 0 or comes out the same. 2 direct: `w_exp` times `route.direct_exposure_share`.
Each variant the faction allows is an option in `BestPackage`; its noise is drawn from its own forked stream, so the
main stream does not move with the number of options. `TacticKey` is `gate|corridor|exit|profile|part|kind`; `axis_repeat`
and `axis_vary` compare corridors. To keep the cost down, variants 1 and 2 are weighed only on each target's best
variant 0 option (gate, profile, part of day, size). A route is cached per cycle by its flown height, not the part of
day, so two parts of day at one height share one plan.

**In the planner.** The main flights fly the option's route, timed by `TimeRoute` at the ToT. Escorts, jammers, decoys
and recce follow it at their own heights and speeds. A feint is planned like a main route. A Pact or NATO group ahead
(`blast_lead_s`) aims at the worst believed threat on the route when the covered route is still exposed beyond
`blast_exposure_s`. Every Pact flight carries a `ZoneEscape` per believed zone its route crosses. The gate book keeps
crossings of one gate `gate_separation_s` apart in a cycle (NATO's CAS free of it). NATO's corridor suppression thins
our radars within `route.corridor_halfwidth_m` of its gate where our troops stand within `route.land_front_m`. Reach is
half the route's length from the bases and back.

**In the resolver and the bridge.** With mode 1 the resolver merges consecutive legs through one zone into one pass,
uses the ground-masked chord for Low flights on ground (`PassKillChance`'s masked flag drops
`attrition.low_vs_long_range_sam`), applies the corridor suppression, and rolls the fighters on the leg from the `ip`
or `hide` point (`FighterLeg`). `MissionBridge::Engage` takes the chord of the remaining planned waypoints inside the
zone. The raid builder tags the legs after the target as egress and follows the terrain at the first low point's height.

**Section 8's fixes.** All done under mode 1: `TimeRoute` anchored on the release, the fighter roll by tag, the
stand-off jammer by tag, `radar_threats` over all main flights, reach by the route's length, the bridge's chord. The
raid builder's per-waypoint speeds and offsets came with B1.

**The cost (§10).** Measured as the planning thread's CPU cycles over two days of a campaign (seed 5), because other
work loaded the machine and wall times moved by half. On the nation's theatre mode 1 costs 1.30 times mode 0 for NATO
(35.4 against 27.3 Gcycles) and 1.44 times for the Pact (52.9 against 36.6): within the budget of 1.5. What got it
there: variants weighed on the best shape only; the routes cached by height; the leg cache's hash mixed to the last bit
(the standard library took its bucket from the low bits, where every leg of one flight has the same height, so the
lookups ran down long chains); the masked chord sampled only inside the ring and cached by route and site; the features
near a corridor cached; the expected losses skipping a site whose ring the route's box misses. On the tuned district's
theatre, with no map, the planner was cheap before and is not now: 6 to 8 times mode 0 (4.5 against 0.7 Gcycles), most
of it the Dijkstra searches over the lattice. Its balance run took 92 s before B2 and 1396 s after, on a loaded
machine. The nation's balance run took 1978 s before and 1405 s after, under loads too different to compare.

**The balance (base against B2, `air_war_balance` on the nation over 10 seeds and on the district over 20).**
`crew_balance` is identical. `ground_check` passes, 0 of 15. `em_bench` changed only in its timings. The helper's
coverage is the same with and without B2 on every mission whose rows differ from the committed table (that table is
older than this branch). `air_war_bench` with `route.mode` 0 is identical to the base, line for line.

- **The bench in mode 1.** One engagement a pass means one ARM salvo (two missiles) a pass, where the old resolver fired
  one a leg. Suppression is thinner. Medium-altitude F-16s over the airfield lose 1.8 a 100 sorties (0.4 before). Low
  against medium is now 3.0 times, nearer the history's 4. The Bekaa package loses 7.6 a 100 (4.5 before; history:
  none) and kills its battery in 62 % (66 %). Yom Kippur's pop-up: 25.8 (25.0).
- **Rebels on the nation**: losses 1.0 to 1.9 aircraft a campaign careless (6 % to 14 %), 1.3 to 2.4 careful (8 % to
  20 %); raids through the battery's sector 1.5 to 5.7 and 0.5 to 2.5; more SAM sites among the targets (1.3 to 2.2).
  **Out of the band** ("a few per cent, few in number, raids that keep away from the battery"). The likely cause: the
  planner now prices the route it would fly, which it believes covered, so targets near defences look cheaper than the
  truth makes them. Small numbers, but to tune (Left below). On the district the rebels are in band (2 %).
- **Neutral on the nation**: losses 4.9 % to 3.0 % careless, 4.4 % to 6.3 % careful; more helicopter raids (45 % to 56 %
  of its packages careless). In band.
- **The Pact on the nation**: losses 2.0 % to 2.9 % and 1.7 % to 2.9 %; the same surge on days 1-2 (the air defences at
  42 % working after day 2, 40 % before); more dive attacks (34 to 56 flights a campaign) and fewer pop-ups (28 to 12).
  Our battery wrecked in 80 % of careless runs (50 %) and 20 % of careful ones (10 %). In band.
- **NATO on the nation**: losses 1.8 % to 1.6 % and 1.6 % to 1.8 %; the rollback a little slower (66 % working after day
  4, 60 % before). The careless battery is wrecked in 90 % of runs (10 % before), as on the district (90 % before and
  after); the careful one in none. The likely cause: SEAD now goes with every DEAD whose route crosses a believed radar
  (§8's `radar_threats` over all main flights), and DEAD reaches sites the old straight lines did not. In band: the
  shape is a methodical rollback that takes a careless battery.
- **The district** moves less: the neutral flies more (16.6 to 20.9 packages careless) and loses less (4.4 % to 2.4 %);
  the Pact wrecks the careless battery in 75 % (90 %) and the careful in 40 % (25 %); NATO 95 % and 10 % (90 %, 5 %).

## As built (S7, 20 Nov 2026, branch `tbr-s7`)

Roadmap S7 takes B2's "Left open" on the routes. All of it is behind the tuning's `route.mode` 2 (the default now,
`data/gameplay/air_war.json`). A save writes its `route_mode`, so a save made before S7 (which wrote 1) replays B2's
routes exactly; the replay fingerprint (`test_routes.cpp`) is unchanged.

- **A gate with no feature.** `RouteGround::LinkFar`: a gate, an exit or a target that no feature joins within
  `route.max_leg_m` is joined to its nearest features however far, so the search always has a first leg. Before, such
  a route fell back to the straight line with no turn points.
- **The believed fighter picture.** `BelievedFighterThreats`: our airfields in the enemy's intel picture (not its
  ghosts, and only while it believes them at least 15 % working), each a ring of `route.fighter_reach_m` (40 km, plus
  half the entry's uncertainty) whose second inside costs `route.fighter_weight` (0.15) times the front's
  `friendly_fighters` times the base's believed working share, times `route.fighter_low_share` (0.3) for a Low flight
  or a helicopter (the period's radars looked down poorly). A fighter
  ring is not a SAM's: it is not masked, it does not make an escape (`ZoneEscape`) and it is not a continuous run nor
  the worst threat on the route (`WorstOnRoute`). Believed CAP stations are not in the picture yet (Left).
- **Navigation error.** `NavigationDrift`: between fixes a flight drifts along one direction, drawn once per stretch
  from a key of the package and the flight (no draw on a campaign stream), by the faction's `routing.navigation.drift_share`
  of the distance flown since the fix, at most `route.nav_fix_m` (3 km). A fix is the first point, any point on a map
  feature, the IP and the target; the drift there is nil. The raid builder adds it to the flown waypoints only: the
  plan, the timing and the resolver keep the planned route. Drift shares are estimates: NATO 0.5 % (INS and the
  Pave Tack era's updates), the Pact 1 % (Doppler and a navigator's dead reckoning), the neutral 2 %, the rebels 3 %
  (map and stopwatch). A research pass is left (below).
- **The testbed's waves on planned routes, in first form.** The waves have no believed picture to plan on, so they
  do not get the RoutePlanner. A low strike or DEAD (not a helicopter) flies round our rings on its way to the IP
  (`TestWaves::AroundRings`): the session's own unit and the battery's vehicles at `route_own_ring_km` (10 km), a
  neighbour at its own reach, `route_margin_km` (3 km) outside each; the ring cut deepest first, its turn point 1.2
  times the ring and margin out at the place nearest the straight line, then each half again (two levels). A ring
  round the entry or the IP is flown through. No draws, so the waves' streams do not move.
- **Routes as flown fed back.** A played raid's outcome now carries where each flight's last seen aircraft was and when
  (`PackageOutcome::flown`, saved as `"flown"` only when there is one). `AirCampaign::CompletePlayed` inserts that
  point into the flight's route (`FlownInto`) and moves the later points' times to when it can reach them; the release
  index and the escapes' indices move with it. What the campaign then resolves and records for the rest of the flight
  starts where the session left it.
- **Tests of the umbrella and `axis_vary`.** The Pact's helicopters beyond their fighter umbrella
  (`helo_beyond_umbrella` 0 against 1) and a planner officer with `axis_vary` against one without, over three days.
- **B2's lattice without a map.** With no country map the lattice is `route.fallback_grid2_m` (25 km, was 15 km)
  and the IP has `route.run_in_headings_nomap` (4, was 8) headings, to pay for the new fighter term and the joined gates: the district's balance run costs what it did before
  S7, not less (`docs/air_war/09_BALANCE.md` section 22).
- **Learning (B14's leftovers), behind the tuning's `learning.mode` 1** (saved as `"learning_mode"`; a save without it
  replays with 0):
  - *Losses it cannot explain.* After a raid, a loss no fighter claimed and no known site's fire explains (no shooter
    reported, or the shooter it believes in fired and no survivor came home to say from where) becomes a suspected
    air defence site in its picture: a ghost report where the loss was, `learning.unexplained_sigma_m` (8 km) off by a
    keyed draw, believed to exist at `learning.unexplained_exists` (0.4), with a ring of
    `learning.unexplained_radius_m` (10 km) and a lethality of `learning.unexplained_lethality` (0.15), labelled "a loss
    nobody saw". Its routes then go round the place. A played raid's losses are now placed in the theatre's frame
    (they were in the scenario's), under this mode only.
  - *The rebels finding the mobile batteries.* A faction's `intelligence.humint_air_defence_per_day` (the rebels 2,
    the others none) is a sympathisers' rate: each step's draw (keyed on the minute, not on the campaign's stream) pick a
    mobile air defence unit of ours, or the player's battery, within the faction's humint reach of the front, weighted 1 near a town
    (within `learning.watch_m`, 15 km) and `learning.countryside_share` (0.3) in the country, halved for the player's
    battery under its nets. The report is a humint one by the faction's own rules (its delay and its errors); its false
    share is a ghost 8 km off. All values are game values.

## Left open

- **Numbers no period source gives**: leg lengths, turn-point counts, the IP's distance, the let-down's distance, a
  stand-off from a ring, the Pact's low-level speeds. Game values until a source is found (routing §7).
- **Who controls a Pact group between the checkpoint and the target** (routing §7); the НПП and КБП limits on ПМВ by
  skill class are not public, so the Pact's lowest height does not yet vary by pilot class.
- **The believed fighter picture** (S7 did the air bases): our believed CAP stations are not in the intel picture
  yet; when B8's and B10's CAPs reach it, they take the same fighter term.
- **Navigation error** (S7 in first form): the drift shares and the fix's cap are estimates; the period's INS and
  Doppler drift rates and how crews took their fixes need a research pass (`map/routes.md` §10.3).
- **Corridor suppression in a played session**: whether the land battle's artillery suppressing our forward defences
  is shown or only resolved abstractly (B5).
- **The testbed's waves** (S7 in first form): low strikes go round our rings, but the waves still have no believed
  picture, so they do not get the RoutePlanner's corridors, gates and features.
- **Masking in `PassKillChance`**: whether the balance holds without `low_vs_long_range_sam` on maps with ground, and
  whether the long-range SAMs' masking then matches `docs/air_war/09_BALANCE.md` §8, is for the balance pass to show.
- **The Tornado GR1's night height** rests on an undated account that may be after 1985 (`napier_hushkit_2022`).
- **The 35 s helicopter rule** (`fm_17_50_1977`) is garbled in the scan; 20 s (`crawford_1977_rand`) is used for all.
- **The rebels' losses on the nation** (14-20 %, see "As built"): their exposure weight or their target choice against
  believed defences needs a tuning pass.
- **The cost on a theatre without a map**: S7's coarser lattice and fewer IP headings pay for S7's own additions but
  do not bring the district's balance run back to its pre-B2 time (09_BALANCE section 22); a cheaper search there is
  still wanted, and whether the coarser routes suit its small theatre is to be seen in play.
- **Weights.** Small changes to a leg's weight can give an entirely different route (`dorsett_1980`). That is true to
  the period's planning, but it means the routes move with any tuning; the tests pin them only on synthetic maps.
