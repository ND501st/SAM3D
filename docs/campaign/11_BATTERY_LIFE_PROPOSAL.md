# 11. The battery's life on the ground: movement, pathing, orders and the visible chain of command (track G)

Proposed 28 October 2026 and reviewed by the project owner the same day (§1, §8). It lays out the features and
systems a roadmap needs for the battery's accompanying elements (logistics, planning, maintenance, operations and
recreation between raids), the movements of a vehicle, the battery and the regiment's units, a basic pathing of the
map that keeps working as the map changes, a chain of command the player can see, and battery orders that travel
by real means to people who carry them out. It names the track **G** (the ground) and its phases G0-G11 (§6), entered
in `docs/DEVELOPMENT_ROADMAP.md`. When this document and a built system's document disagree, the built system's
document wins.

## 1. The brief (project owner, 28 October 2026)

1. Work on **all the battery's accompanying elements**, so the player's experience includes logistics, planning,
   maintenance, operations and recreation between raids.
2. It must **fit neatly within the dynamic campaign**.
3. Critical: **unit-wide and battery-wide movements**, developed and coordinated.
4. Critical: **basic pathing for the map** that works seamlessly as the map is altered and updated in the future.
5. Critical: **a visible chain of command**.
6. It should bring in **elements already built in simcore that the player cannot yet reach**.

**The owner's review (28 October 2026).**

1. **The emphasis**: the command structure, how battery-wide orders are communicated and carried out, and movement.
   First come G1-G3 (movement), G8 (the visible chain) and G10 (battery orders, added for this), with G11 drawing
   them (§6, "Priority").
2. **G6 (wear and maintenance) and G7 (the battery's day) are on hold.** Their design stays here for later.
3. Open questions 1-4 as recommended (§8).
4. **The TZM and its crew are not playable.** No support role is playable in track G.
5. **Recreation**: minigames later, not now.
6. **Unreal**: every element of the battery moves in Unreal, and every element within the player's visual range
   appears in Unreal (G-14, §5.12, G11). Whether Unreal's navigation mesh has any part is still open (§8, question 7).

## 2. What the player lives: a day at battery 21

The target experience, step by step, with the system that serves each step (§5). Every step is an existing or planned
thing done by a person at a place; nothing is a menu between raids.

| Time | What happens | Served by |
|---|---|---|
| 18:20 | The battery commander leaves the PU-12M for the regiment's evening briefing; the deputy takes the battery | Movement (§5.2), the chain's routine (built), handover (20.6) |
| 18:30 | He goes by road to the regiment's CP in the Lazne woods (in what vehicle: G0); the drive takes the road's time | Route service (§5.1) |
| 19:00 | The briefing: the regiment's order to cover the Kesmark bridges from a new area, with its reasons, in person, on paper | P1, 14.1-14.2 |
| 20:10 | Back at the battery, on the working map: the route, the start point, the control points and their times, the positions | Planner and working map (§5.5, 14.3-14.4) |
| 20:40 | The oral combat order to the vehicle commanders gathered at the PU-12M; each repeats his task back; route cards in their map cases; 21-3's commander, at the far position, gets it by runner | Battery orders (§5.5, G10), papers (§5.9) |
| 22:30 | «К маршу!» on the battery net: the vehicle commanders mount, start up and climb out of the emplacements; each reports «К маршу готов» (`voenizdat_osa_1991` p. 246, in the crew data), and 21-3, whose set is off, is fetched by the runner | Battery orders (G10) |
| 23:00 | The night march: every vehicle of the column moving on the roads in Unreal, flags and lamps, the regulator at the bridge holding it for the division's column; the radiotelephonist logs the control points | Movement and coordination (§5.2-5.3), what Unreal draws (§5.12) |
| 02:30 | Occupation: positions, nets up, wire to the launchers, each launcher's «к бою готов» to the battery commander and the battery's to the regiment | Battery area (§5.4), built drills (7.5, 7.6, 8.14), battery orders (G10) |
| 05:20 | The dawn raid, fought as the missions fight today | Built |
| 06:30 | The regiment orders the battery to its reserve area by 08:00: the order on the net, the march replanned, the column moves before the enemy's next strike on the old fix | Battery orders (G10), movement (G2) |
| 07:00 | The TZM to the technical battery at the Horka siding and back, its NPC crew driving; the road bridge is down, so it goes round | Logistics (§5.6), overlays (§5.1) |
| 09:00 | The sergeant major's run to the rear: food, fuel, the post, the rear's news | Logistics, the routine (built) |
| 18:20 | The next briefing; tomorrow he plays 21-2's commander, and the order reaches him on the battery net | Whom the player plays (19.5), battery orders (G10) |

On hold with G6 and G7 (the owner, 28 Oct 2026), and back in this day when they resume: 21-3 breaking down on the
march and the maintenance vehicle staying with it; «Воздух» at 05:10 finding 21-3's operator asleep in the tent, the
readiness waiting for his walk; maintenance on 21-2's search radar at 11:00; meals, letters and sleep by shifts in
compressed time.

## 3. Decisions (G-13 and G-14 added on the owner's review)

| # | Decision | Why |
|---|---|---|
| G-1 | **One movement service for everything that travels**: a vehicle, a column, a person on foot, a courier or despatch rider, a supply convoy or train, a rebuild team. All ask one route service and move at their mobility class's pace. Nothing jumps | Extends the crew's no-teleport rule (`docs/crew/13_OUT_OF_THE_VEHICLE.md`) from bodies to units; the campaign's moves, the pipeline's carriers and the supply lines stop using straight lines |
| G-2 | **Navigation data is derived, never authored.** It is built at load from the map's data (roads, rivers, bridges, woods, towns, slopes, sites). Nothing refers to its node numbers. Routes are asked between **anchors** (places, prepared positions, theatre objects' sites, battery-area places, by id) and planned again when the map changes | The map can be edited, extended or regenerated (15.6) with no nav edits and no broken saves |
| G-3 | **Changes in play are overlays, and they are facts.** A bridge down, a crater, a road closed by the commandant, a pontoon bridge laid, a ford marked, rubble in a town. A unit plans on the overlay it **believes**; its movers drive on the **truth**. A column that meets a cut bridge it was not told of stops, reports it, and plans again | Crew pillar 3 and B8: nobody acts on the truth; the record and the pipeline carry the news |
| G-4 | **Basic, not clever.** Roads first; off-road only to link a place to the road net or when a cross-country move is ordered. A* with integer costs and index tie-breaks so MSVC and GCC agree. The country at network level; a district with a trafficability grid in tiles built on first use | Cheap, testable, deterministic; enough for a battery and a regiment |
| G-5 | **Simcore decides, Unreal draws.** Where anything goes is decided in simcore; Unreal draws it on the simulation's path and its ground. Art placed by hand in a level must come from map data or go back into it, and a check flags meshes standing on the simulation's roads and places. Unreal's navigation mesh: §8, question 7 | `CLAUDE.md`: game logic in simcore; headless tests, multiplayer and the nation's map all need it |
| G-6 | **The march is a document.** The march plan (route, start point, control points with times, halts, speeds, spacing, order of march) is made as the textbook has the battery commander make it, by the NPC planner or the player, and given out as route cards. The radiotelephonist logs the control points passed | The record principle (B7): the regiment learns where its batteries are from their reports |
| G-7 | **Movements are coordinated by a movement table.** Crossings and narrow stretches of road are reserved by time and priority; the commandant's regulators hold a column that comes out of its slot | Unit-wide and battery-wide moves share the same bridges with supply, troops and rebuilds |
| G-8 | **The battery area is a set of places** generated from the task and the ground by sourced rules: positions, reserve and dummy positions, the TZM's places, the PU-12M, the camp, the kitchen, the vehicle park, guard posts, the observation post, wire runs. Every activity happens at a place | Activities as jobs at places (`07_BETWEEN_FIGHTS.md` §3) need the places |
| G-9 | **Stocks are things in places**, known to the battery commander only from reports and his own papers | B7, B8 |
| G-10 | **The chain of command is visible the way it was to the people in it**: people at their places, voices with call signs, papers in the map case (the communications diagram, the call-sign table, the staff list, the order), and the record. Never an omniscient chart; the character's papers can be wrong or out of date | First person (`docs/command/01_PRINCIPLES.md`) |
| G-11 | **Every built system the player cannot reach gets a surface owned by a person**: an order in a menu, a paper, a control, a job at a place, a means of signal | Crew pillar 3: every action has an owner seat |
| G-12 | **Time between raids is lived, compressed.** The director wakes the player; sleep, meals and work pass at speed. The continuous mission has no "next raid" button. (With G7, on hold) | B11 and `05_THE_SYSTEMS.md` §8 |
| G-13 | **A battery order is words carried by a means to a person who carries it out.** The regiment or the battery commander gives it on a net, on a wire, in person or by runner. Only those whose means reach them hear it; each repeats it back or acknowledges; each vehicle's commander and driver carry it out as acts at their seats; the results are reported and logged. No vehicle acts on an order its commander did not hear. The emission regime (8.16) already travels this way; every battery order follows it | Crew pillar 3 and first person. Today `Battery::Order` moves every vehicle the moment an order is given, and no commander of theirs hears it (§4) |
| G-14 | **What the player could see is in Unreal.** Every element of the battery (each vehicle, each person out of his vehicle, the support trucks) is drawn and moves in Unreal throughout a session. Every element the simulation holds within the player's visual range is drawn too: the regiment's columns, convoys, couriers, the regulator at the crossroads, the people at a place. Inside that range movers run at full detail (M2), so what is drawn is where the simulation has it | The owner, 28 Oct 2026. First person works both ways: nothing the character could not know is shown, and nothing he could see is missing |

## 4. Built in simcore, not yet in the player's hands

Found by reading the code on 28 Oct 2026 (the Unreal module's calls into simcore checked by name).

| System | Where | The player today | What this proposal gives them |
|---|---|---|---|
| The battery's orders: march, short halt, combat formation, ambush (roving), river crossing, relocate, reload, refuel | `battery.hpp`, `ScenarioRunner::OrderBattery` | Nothing: scenario triggers give them. No Unreal code calls `OrderBattery`. Routes are hand-written polylines, and vehicles drive straight at their goals (`Battery::Move`) | Ordered by the battery commander (player or NPC) from the working map and the order menu, on routes from the route service (G1, G2, G9) |
| How the battery's other vehicles get an order | `Battery::Order`; the emission regime's path (`OrderBatteryEmission`, `CrewRegime`, `CommanderRegime`) | Every vehicle obeys at once. Only the emission regime and the net orders (spare frequency, duty, simplex) are said on the battery net, heard by the crews modelled seat by seat (the player's vehicle, the PU-12M in a linked battery: `battery_link.hpp`); the battery's other vehicles take the regime at once (`BatteryOrderRegime`) | Each vehicle's commander hears the order by his means, repeats it back and carries it out with his driver; a vehicle not reached waits until someone reaches it (G10) |
| The battery's vehicles and movers in Unreal | `ASimScenarioDirector::SpawnMissingEntityActors`, `ASimMapActor` | Every entity of a running scenario is spawned and moves by its state (a real model for the TZM, the fuel truck and the PU-12M, else its basic model, else a stand-in); the nation's assets stand still where they are; no movers outside the scenario, no rule tying what is drawn to what the player can see | Every battery element always drawn and moving; every mover and place within the player's visual range drawn (G11) |
| Service runs (the TZM, the fuel truck) | `Battery::RequestService`, the runner's service drills | Called automatically when a launcher is empty or its tank half full; the truck drives straight to its pair | Requested by radio or telephone, driven over the roads from the TZM's place or the technical battery (G5) |
| Prepared positions, nets, dummies | `position_prep.hpp`, `prepared_positions.json` | Fixed by the scenario or the campaign's posture; drawn (`ASimPositionWork`) | Dug, netted and placed as jobs with hours at places of the battery area (G4) |
| Our supply lines | `air_war/logistics.hpp` (`SupplyNetwork`) | Headless; not drawn | Convoys and trains as movers on the roads; the battery's own resupply one of them (G2, G5) |
| The country's routes and bridges | `country_map.hpp` (`CountryNetwork::Route`) | Headless (`country_dump --route`, `map_view.py`) | The route service's long legs (G1) |
| Our order of battle | `command/oob.hpp`, `oob_pannonia.json` | Not shown anywhere | The staff list and the communications diagram on paper; neighbours on the working map as they are told (G8) |
| The chain's posts, places and routines | `command/data.hpp`, `chain_pannonia.json`, `places_pannonia.json` | The campaign screen's list of coming contacts | People at their places, moving by their routines; the briefing a place the commander drives to (G8, P1) |
| The pipeline | `command/info_net.hpp` | The campaign screen's received facts | Deliveries as scenes (P1), the player's means (P2); a carrier's time from its route, not a straight line times `road_factor` (G2) |
| The battery's relocation in the campaign | `AirCampaign::RelocateBattery` | A click on the map and M: the battery is there at once (30 km at most) | A relocation order: a march with its time, its exposure on the road and its arrival reported (G2, G9) |
| The battery's posture | `airwar::BatteryPosture` | Sliders on the campaign screen | Orders and the battery's real state (15.1); the sliders retire |
| The crew's bodies, walking, jobs at places | `crew_mobility.hpp` | Out through a hatch and back (F); NPCs walked to the vehicle's jobs | People walk the battery area by the foot layer to their places and jobs (G2, G4; their activities with G7, on hold) |
| Faults and their recovery | `equipment.hpp`, `drill_master.hpp` | Faults in missions, recovered by drill | Wear, maintenance jobs at the vehicle, breakdowns on the march (G6, on hold) |
| Fuel | `fuel.hpp` | The gauge; the half-tank call | Burned by the battery's kilometres and hours; stocks and requests (G5) |
| The instructor and the lessons | `instructor.hpp`, `tutorial.hpp` | F9; the missions menu | Training as an activity of the battery's day (G7, on hold) |

## 5. The systems

```
 map data (data/maps/*.json)                        mobility (data/vehicles/mobility.json)
        │ built at load, keyed by the map's content hash        │
        ▼                                                       ▼
 NAV: road graphs (district + country, joined) · trafficability tiles · anchors · overlays (truth and believed)
        │ RouteRequest(from anchor, to anchor, mobility class, constraints, believed overlay)
        ▼
 MOVEMENT: movers (vehicle, column, person, carrier, convoy) · march plans · movement table · levels of detail
        │ positions, control points passed, halts, breakdowns, sightings by the enemy
        ▼
 THE BATTERY'S LIFE: area and places · stocks and requests · (on hold: wear and maintenance · the day, readiness)
        │                                                                      ▲
        ▼                                                                      │ the record → reports (P9)
 THE CHAIN OF COMMAND: people at places · means of signal · papers · NPC officers' orders (14.2) ──┘
        │ BATTERY ORDERS: given · carried by a means · heard · repeated back · carried out at seats · reported
        ▼
 UNREAL: every battery element moving · everything in the player's visual range · the working map · papers ·
         telephone · menus · the director
```

Planned code: `simcore/include/simcore/nav/` (namespace `simcore::nav`), `simcore/include/simcore/movement/`
(`simcore::movement`; not `move`, which reads as `std::move`), and the campaign's `simcore/campaign/` as
`05_THE_SYSTEMS.md` plans it. Header-only, standard library only, every model with a test (`CLAUDE.md`).

### 5.1 The route service (pathing)

**Layers.**

| Layer | What | Exists | Change |
|---|---|---|---|
| Country network | Main roads, railways, pipelines as graphs; river crossings as named bridges that can be cut | `CountryNetwork::Route` (Dijkstra by length) | Road classes and speeds per mobility class; ferries and fords as crossings; integer time costs |
| District road graph | The district's roads (main, secondary, track) and bridges (`MapCrossing`) | Strips for drawing only | Built as the country's network is, by one generalised builder; joined to the country's at **portals** where the country's lines cross the district's edge (they agree point for point: `test_country_map.cpp`) |
| Trafficability tiles | A cost grid per mobility class: slope from the ground, woods and buildings from the obstacle grid and the occupancy, water (only for swimmers, at a bank they can climb), lakes and marshes from the country | The obstacle grid (`MapData::ObstacleGrid`), the ground function | New. 1 km tiles at the obstacle grid's spacing, built on first use and cached by (map hash, tile, class) |
| Foot | The same tiles with a man's pace | `GroundSpeed` (Tobler, the woods factor), but a straight line | A crewman's walk goes round the pond and through the gap in the hedge |
| Final approach | Into an emplacement by its open back, 8 m alongside a launcher, into a column slot | `battery.hpp`'s slots | Fed by the path's end |

**A request** names two anchors, a mobility class, constraints (night, blackout, avoid towns, avoid open ground by day,
a bridge's load later), and the overlay to plan on. The search links each anchor to the nearest road nodes by an
off-road search bounded to a few kilometres, routes the rest on the joined graph, and runs a tile search only for an
ordered cross-country move or where no road serves. The result: the path, its legs (road class, crossing, off road),
the time for the class, the bridges used, and the overlay entries it depends on.

**Mobility classes** (`data/vehicles/mobility.json`, every value with its source or marked an estimate): the 9A33
launcher, the TZM, the PU-12M (BTR-60PB), the maintenance vehicle and the ZIP vehicle, the fuel truck, the sergeant
major's truck, the commander's car (if he had one: G0), the despatch rider's motorcycle, a man on foot. Fields: speed
by road class by day, night and blackout; cross-country speed; the steepest slope; fording depth or swimming (its speed
and the bank it can climb); fuel per kilometre. Which of the battery's vehicles swim is a sourced fact, not a guess:
the river-crossing drill (7.5) already swims the launchers at 8 km/h, and a fuel truck that cannot follow changes the
crossing plan.

**Anchors.** Places (`places_pannonia.json`), prepared positions (the theatre's `positions`), theatre objects' sites,
the battery area's places (§5.4), and a map point only as a last resort (the player's click, snapped to the nearest
point the class can reach). Saved routes keep anchors and the map's hash, never node numbers.

**Overlays.** An entry: id, kind (cut, slowed, closed, opened: a pontoon bridge, a ferry, a ford; a crater; rubble),
where (a crossing id, a stretch, an area), from and until, the cost it adds, and the fact it came from. The **truth**
overlay is what is; each unit's **believed** overlay is built from the facts that reached it. A route records the
entries it depends on, and when a unit learns an entry has changed, only its routes that touch it are planned again.
On the truth side, a mover that meets a blocked leg stops and its commander becomes the fact's first knower.

**When the map changes.**

| Change | What happens |
|---|---|
| A map file edited (a road moved, a village added, woods redrawn) | Rebuilt at load; nothing else to edit. `nav_check` lists anchors that became unreachable or no longer stand on open ground |
| A district generated where the battery fights (15.6) | The same builder |
| Changes in play (bombs, rebuilds, the engineers' bridges) | Overlays |
| Art placed by hand in the Unreal level | The simulation never reads Unreal's geometry; a level check flags static meshes on the simulation's roads and battery-area places |
| A save made on an older map | The replay saves (`air_war/save.hpp`) assume the same data; a changed hash loads the battery's state snapshot (15.7) instead of replaying (open question 4) |

**Determinism and cost.** Costs in integer tenths of a second from lengths rounded to the centimetre; ties broken by
node index; no unordered container in the search's order; a test compares route hashes on MSVC and GCC (CI has seen
the two libms disagree in the air war's planning). Targets to measure, not promises: the nav built in under 200 ms at
load, a road route in under 5 ms, a 1 km tile in under 20 ms; `nav_bench` measures them as `em_bench` does the EM layer.

### 5.2 Movers and the march

**A mover** has an identity (a unit or a person), a mobility class, a route, a place along it in metres, a speed
profile, and a state: moving, halted, waiting at a control point, blocked, broken down, arrived. Kinds: one vehicle;
a column (the head on the route, each vehicle by its distance behind, as `battery.hpp` does today); a person on foot
(`CrewMover`'s ground legs on a foot route); a carrier (the pipeline's courier or despatch rider); a convoy or train
(the supply lines' columns).

**Levels of detail.**

| Level | Where | How it moves |
|---|---|---|
| M0 | In the campaign, no session; the regiment's other units; the supply lines | Its place along the route by its plan, at the campaign's step; exposure per hour to the enemy's armed reconnaissance (built) |
| M1 | In a session, out of every player's visual range | The column's head and tail along the path; each vehicle by its distance behind; no ground contact |
| M2 | In a session, within any player's visual range (§5.12), and always the battery's own elements | Each vehicle on the path and the ground with its slot behaviour, short halts off the road, the emplacement approach; the player's own vehicle driven by the player or its NPC driver |

A hand-over between levels keeps each mover's place along its route, so the column seen from the PU-12M and the column
driven past are the same column. The level follows the player's visual range, not a fixed radius. An NPC driver following the route is an act at his seat, so it goes through
`ScenarioRunner::NpcDoes` and the helper shows a player driver the next turn.

**The march plan** is the battery commander's document (`voenizdat_osa_1991` pp. 232-239; the march's organisation in
G0): the route; the start point and the time the head passes it; control points and their times; the halts (as
`battery_tactics.json` has them); speeds by leg; spacing by threat (built: 40 m, 400 m when an attack is expected); the
order of march (PU-12M, two launchers, TZM, two launchers, TZM: `03_THE_BATTERY_MISSION.md` §4); the column's length
and the time it takes to pass a point; radio discipline (built: flags and lamps). Each vehicle commander gets his route
card. The radiotelephonist logs the control points passed; the arrival report goes up by the post's means.

**`battery.hpp` on the route service**: `March`, `Relocate`, roving `Ambush` and the service runs take their routes
from it. Hand-written polylines still load, so today's scenarios play unchanged.

**Carriers and routines on routes**: the courier's round, the despatch rider, the sergeant major's 09:00 run, the
regiment commander's visits and the battery commander's drive to the briefing become movers. A delivery's time is its
route's time, and a carrier on a road where bombs fall is at risk where he actually is (the pipeline's carrier loss,
built).

### 5.3 Coordination

**Inside the battery**: the order of march and spacing; pairs bounding (built); each TZM with its pair; the
maintenance vehicle at the tail, stopping with a breakdown; short halts on the route's lay-bys; the camp moving with
the sergeant major behind the fighting vehicles.

**Between units: the movement table.** Reservations of a crossing or a narrow stretch: unit, from, until, priority. A
march plan asks for its slots; the earlier asker or the higher priority holds them; the later column's plan moves its
times or its route. The military commandant of Kesmark (a post in the chain) has traffic regulators, placed today at
the Vrbice crossroads (`docs/command/05_PLACES.md`); they and the bridges' regulators hold a column that arrives
outside its slot. On the table: the regiment's batteries (their moves at M0 until 17.5 makes them
units), the technical battery's runs, the rear's convoys (the supply lines), the covered troops' marches (the
campaign's front data), the rebuild teams and the pontoon park (13.5).

**Under the air threat**: the spacing and short halts (built); a move by day under the enemy's air superiority may be
seen (21.1's hook); the enemy's armed reconnaissance finds columns on the roads (`logistics.hpp`'s search, reading the
movers); a relocation leaves an empty position for the enemy's next strike on the old fix, which the freedom-of-action
ledger counts (13.6).

**Breakdowns**: a fault on the march (at a fixed chance while §5.7 is on hold) stops the vehicle; the column goes on or waits as its commander decides; the
maintenance vehicle stays, repairs or tows; the report goes up.

### 5.4 The battery area and its places

Generated from the task and the ground by rules held as data (`data/campaign/battery_area.json`, sourced in G0) in
`simcore/campaign/battery_area.hpp`, the same every time for the same task and ground:

| Place | Rule (source or G0) |
|---|---|
| Main positions | Pairs 200-500 m apart within, up to 3 km between pairs, open ground with sight along the attack directions, nothing tall within 20 m (`voenizdat_osa_1991` pp. 232-239; `03` §3-4) |
| Reserve and dummy positions | Reserve within 2 km (pp. 232-239); dummies as the order places them (7.6) |
| TZM places | Within 500 m of their pair (pp. 232-239) |
| The PU-12M and the aiming circles | The PU-12M behind the pairs (`battery_tactics.json`); its aiming circle 30-40 m ahead of it, a launcher's 20-50 m ahead (pp. 249-250) |
| Maintenance vehicle, ZIP vehicle | G0 (the maintenance vehicle serves four launchers and deploys in 40 min: p. 189) |
| The camp: tents or dugouts, kitchen, water, latrine; the vehicle park | G0 (`pvo_bu_ix_1984`, the period's internal service regulation) |
| Guard posts, the observation post | The chain's observation post (built); guard posts: G0 |
| Wire runs | The PU-12M to each launcher (P-274: four lines at 500 m in 25-30 min, `ru_pu12_text`) |

Every place is an anchor with what stands there, who works there, its cover (woods, nets), its echo
(`position_prep.hpp`), and foot routes to the others. The NPC planner lays it out; a player battery commander may move
any place on the working map within the rules, and the helper shows the NPC's layout. Position work, wire and the camp
are jobs at places with hours, people and what they leave half done when dropped (`07` §3).

### 5.5 Planning and battery orders

**Planning.** The battery commander's planner (14.3) produces the march plan, the battery area, the sectors and
regime, and the support requests (the day's schedule, 20.4, waits with G7). The **working map** in the PU-12M (14.4) is
where a player sees and changes them: pick the roads (the planner fills in the rest), place the positions, set the
control points and times. The **order composer** turns the plan into the oral combat order and the written papers
(§5.9). Planning takes the commander's time at the map, during which he is not elsewhere. The helper's shadow shows the
NPC's plan at each step.

**A battery order** (G10; planned in `simcore/command/orders.hpp`) is a record: who gives it, to whom (a vehicle, a
pair, the battery), what (a `BatteryOrder` kind with its parameters, a march plan, a position, a readiness, a regime, a
sector, the regiment's task of 14.1), by what means and in what words (the crew data's callouts, the talk tables on
the net), and for each recipient its state and times:

```
given ──> reached ──> heard ──> repeated back ──> carrying out ──> done ──> reported ──> logged
              │                                        │
              └─ not reached (set off, jammed, wire    └─ cannot (the bridge is down, broken down):
                 cut, out of earshot): waits                reported with its reason
```

**The means** are the built ones. In person at the orders group: the oral combat order with the vehicle commanders
gathered at the PU-12M, each repeating his task. The battery net: the sets, call signs, talk tables and jamming
(`radio_nets.hpp`). The wire from the PU-12M to the launchers once it is laid (`voice_comms.hpp`). Flags and lamps on
the march under radio silence (`battery_tactics.json`). A runner, or the commander driving over, both movers (G2). A
recipient hears an order only when his means reaches him: his set on, warm, on the net's frequency and not jammed out,
his headset on its box, or within earshot.

**The giver** knows who repeated back. A silence past its time makes him say it again, try the next means, or send a
runner; the rule is data, and his skill tier sets how soon he notices. His picture of the battery on the working map
is what was reported, not where the vehicles are (B7).

**The receiver.** Every vehicle of the battery has a commander and a driver as people (the registry, G8.1), at one of
two levels of crew detail:

| Level | Which vehicles | How an order is carried out |
|---|---|---|
| C1 | Every vehicle of the battery not modelled seat by seat (the other launchers, the TZMs, the support trucks) | The vehicle commander and the driver hear, repeat back, decide and act, with their skill tier's times; the vehicle's acts (crew mounted, started, out of the emplacement, on the route card, in position, «к бою готов») follow from his |
| C2 | The player's vehicle; the PU-12M in a linked battery (`battery_link.hpp`) | The whole crew seat by seat, as built |

Every act at a seat, C1 or C2, goes through `ScenarioRunner::NpcDoes`, so the helper shows a player in that seat the
same move (`docs/crew/14_HELPER.md`). `Battery::Order` then applies an order to each vehicle only once its commander
has it and acts; a vehicle not reached stays where it is until someone reaches it, and catches up by its route card.

**Reports and the record.** Ready to march, control points passed, arrived, ready for battle, and cannot (with the
reason) go up on the net or by the next means. The radiotelephonist logs them (`radio_journal.hpp`), and the battery
commander reports up by the post's means. The debrief shows each order's times for each recipient and its why (14.1's
reasons, B2).

**From the regiment down.** The regiment's orders (14.2's NPC officers) reach the battery commander by the same path:
the pipeline for the routine (P1), the net or the wire for the urgent. He passes them on in his own words and at his
own pace. The emission regime (8.16) is the first built case: the regiment's order or his decision, said on the nets,
told to each crew by its commander, each crew keeping the regime it was told. Its pattern becomes the general one.

**The player.** In command, he gives orders from the radial menu (8.15; each order needs its place in the crew data's
`"menu"`) and the working map, and sees only the acknowledgements and the reports. In a vehicle commander's or a
driver's seat, he hears the order in his headset, repeats it back with the replies (U, I, P), and carries it out with
the helper's next move.

### 5.6 Stocks and logistics

| Stock | Where it is | Used by |
|---|---|---|
| Missiles | In the launchers' containers (`launcher.hpp`), on the TZMs (built), at the technical battery by state (tested, ready) | Firing; the technical battery's tests (the 9V242-1 station, 6-7 an hour: pp. 188-189) |
| Fuel | Each vehicle's tank (`fuel.hpp`), the fuel truck (built), the rear's point | Hours running, kilometres, the power units |
| Food and water | The kitchen, days per person | People, by day |
| Spares | The ZIP sets | Repairs (§5.7) |

Norms in data with sources, or game values in `data/gameplay/`. **Requests** (a written or spoken request up the chain)
go to the deputy for armament for missiles and the deputy for the rear for fuel and food, by a means (P2); the NPC
battery commander requests by rule (14.2's "missiles under two a launcher" exists in the plan). The answer is an order:
a TZM run at a time. **Deliveries** are movers: the TZM to the Horka siding and back, the fuel truck, the sergeant
major's run, the field post; a cut bridge lengthens a run; armed reconnaissance can find them. Their crews are NPCs:
the TZM and its crew are not playable (the owner, 28 Oct 2026), and no other support role is in track G. **What the battery
commander knows** is what the sergeant major, the vehicle commanders and the TZM's commander report, and what he wrote
in his ledger; the NPC decides on those, and a launch the radiotelephonist logged wrongly makes his ledger wrong (B7).

### 5.7 Wear and maintenance (on hold with G6)

On hold (the owner, 28 Oct 2026); the design is kept for when it resumes. Wear clocks per vehicle and subsystem: hours radiating, the turbine's hours, kilometres, missiles held prepared,
power-up cycles. A fault's chance grows with the hours since its maintenance (game values until 20.0 reads pp.
206-224). The kinds of maintenance and their contents come from the textbook; each is a job at the vehicle's places
(8.14) with its people, its time, the state it leaves the equipment in (covers off, power off) and its drop time. The
maintenance vehicle's capacity limits how much is done at once. Repairs: a fault found in the function check or a
fight is recovered by drill (8.3), by the maintenance crew with a ZIP spare, or by evacuation on tow to the technical
battery (a mover). **Playable**: a crewman or the maintenance crew walks to the job's place, opens the covers, finds the
fault on the console's own fault model, replaces the block, closes up and runs the function check, with the helper's
steps from the drill master.

### 5.8 Operations, the day, needs and recreation (on hold with G7)

On hold (the owner, 28 Oct 2026), except what battery orders need: the readiness and emission regimes as orders (G10)
and the guard and observation post as manned places (G4). Mostly built or planned in phases 19 and 20; when G7 resumes
it joins them to places and movers.

- **Operations**: readiness and emission regimes, the board, the warning net, the duty launchers (built); readiness
  **computed** from where every needed person and piece of equipment is (20.2); the duty roster (19.3); guard and
  the observation post as manned places; the report routine (P9); interruptions (20.5).
- **The day**: activities as data at places (20.1), the schedule as the NPC battery commander's plan given out at the
  morning orders and on a board in the PU-12M (20.4), on the movement service: people walk to their activities and
  back by their real routes.
- **Needs, a first slice of 19.2**: sleep, food and morale as multipliers on the skill tiers, heard not shown (19.6).
- **Recreation**: meals, rest, letters and the field post (the sergeant major brings it), the newspapers and the news
  (the political deputy; the broadcast routine is built), a film or football where the battery is in a quiet area, a
  walk to Vrbice, where talk in the pub reaches the enemy's agents (`07` §2). In play: compressed time while the
  player's person sleeps or eats, short scenes with the battery's people (where the player hears its mood), and the
  director's wake events. Minigames later (the owner, 28 Oct 2026), not in track G now.

### 5.9 The visible chain of command

Three ways the character sees the chain, each built from data that exists:

1. **People at their places.** Every post holder is a person with a body, a rank and a name at their place
   (`docs/command/05_PLACES.md`), moving by the movement service on their routines: the regiment commander's visit
   arrives by road; the regiment's CP in the Lazne woods is a place the battery commander drives to and walks into;
   the sergeant major leaves at 09:00. **One people registry**: the chain's holders (`command::Position::holder`) and
   the battery's roster (19.1) become one list of people, so the vehicle commander who is a chain post is the same man
   in 21-1's seat.
2. **Papers the character holds**, in the map case, dated, written from what reached him:
   - the **communications diagram**: who is on which net and line, with call signs and frequencies (from
     `chain_pannonia.json`, `signal_means.json` and the radio nets' data);
   - the **call-sign table** and the talk tables (built);
   - the **staff list**: who holds each post of the battery, with their class (from the registry; a wounded man's
     replacement appears only when the battery commander is told, P8);
   - the **order**, with whom it came from and by what means; the **ledger** (§5.6); the **daily routine** (§5.8).

   A change reaches the character as a fact through the pipeline, and his paper is corrected then, or not.
3. **Voices and the record.** Call signs on the nets, the telephone's exchange, the courier's "from the chief of
   staff"; the radio journal and the commander's log record who ordered what. The debrief shows the whole why chain
   (B2).

**What the player does with it** (P2): picks a person on the communications diagram and a means the diagram lists for
them (the telephone while the wire is whole, the net with its call sign and talk table, a written report by courier,
or going in person, which is a move with its time); reports up, requests support, asks for orders. In command, his
subordinates' places on the working map are where they reported being. When command breaks (21.4-21.6), the diagram
shows the line that does not answer and the runner he sends; the alternate CP's call sign goes on the paper when he is
told of it.

**Never**: a chart that shows the truth (who is alive, where everyone is now, what every post knows).

### 5.10 The player's surfaces

| Surface | Where | Calls |
|---|---|---|
| The working map table | The PU-12M, beside the board (14.4) | The planner; the march plan; `OrderBattery` |
| Battery orders and requests | The radial menu (8.15): the battery commander's and the vehicle commanders' sectors | March, halt, combat formation, relocate, ambush, readiness, reload, refuel; requests for the TZM, fuel, repair; the acknowledgements and reports as they come back (§5.5) |
| Receiving an order | A vehicle commander's or a driver's headset; the replies (U, I, P) | Repeat back, report ready, report cannot; the helper's next move (G10) |
| The route card | The vehicle commander's map case; the driver | The march plan's legs and control points; the helper's next turn for a driver |
| Papers | Every commander's map case | §5.9 |
| The telephone | The PU-12M's wire lines (built in `voice_comms.hpp`) | P2's calls by wire |
| The schedule board | The PU-12M | 20.4 (with G7, on hold) |
| The battery area | The level | Walking to places and working jobs there (8.14) |
| The world in view | The level | Every battery element moving; everything within visual range (§5.12) |
| Driving | Any vehicle with a driver's seat | The player drives; the column follows (`Battery::SetOwnPosition`, built) |
| The director | The session | Wake events and compression (15.4) |
| The campaign screen | The menus | Until the continuous mission (15.8) exists, its relocation becomes a march with a time; after it, the screen keeps only what is outside the character: saves, settings, the reveal |

### 5.11 Multiplayer, saves, tools and tests

- **Multiplayer**: the server plans routes and moves the movers, at M2 whatever any player can see; the knowledge
  stream sends each client the movers within its player's visual range; each player's papers are his own (P6).
- **Saves**: movers and overlays are part of the battery's state (15.1, 15.7); routes are planned again on load from
  their anchors.
- **Tools**: `nav_check data maps/<map>.json` (every anchor reachable by the classes that need it, the route table,
  JSON for `map_view.py --routes`), run by `verify.ps1` and `verify.sh`; `nav_bench`; `battery_day_demo` (a headless night
  and dawn from the regiment's order to the relocation, printing every order's times and the record); the Unreal level check of §5.1;
  `helper_coverage` gains the driver's route.
- **Tests**: routes against hand-worked answers on a small fixture map; a cut bridge believed and not believed; a road
  moved in the fixture changes the routes with no other edit; route hashes equal on both compilers; column spacing and
  march times; the movement table's conflicts; an order not heard is not carried out, and a repeat or a runner gets
  it through; every order's times in the journal; the visual range's set of movers and its hysteresis; the battery
  area's rules and reachability; stocks and requests (wear, maintenance and readiness from people's places when G6 and
  G7 resume).

### 5.12 What Unreal draws (the owner, 28 Oct 2026)

**Today.** Every entity of a running scenario is spawned as an actor and moves by its simulation state
(`ASimScenarioDirector::SpawnMissingEntityActors`): a real model for the TZM, the fuel truck and the PU-12M, else its
basic model, else a stand-in. The neighbours' launchers are spawned at their sites and set on the ground. The nation's
assets are drawn as basic models where they stand (`ASimMapActor`). The player's vehicle's crew have bodies. Nothing
outside the scenario moves (the supply lines' convoys, the pipeline's couriers, the regiment's other units), and
nothing ties what is drawn to what the player can see.

**The rules.**

1. **Every element of the battery is drawn and moves, wherever the player is**: each launcher, the PU-12M, the TZMs,
   the support trucks, and each person out of his vehicle, on the simulation's path. The battery is always at M2.
2. **Everything within the player's visual range is drawn**: any mover or place the simulation holds (the regiment's
   columns, a convoy, the courier's motorcycle, the regulator at the crossroads, a neighbour battery, the people and
   tents of a place). The visual range runs from the player's eye, or from the optic he looks through (the TV-optical
   sight, binoculars), out to what the weather's visibility (`visibility_km` in the weather presets) and the light
   allow. Unreal's rendering hides what hills and woods hide.
3. **Inside the range a mover runs at M2**, so what is drawn is where the simulation has it, on the ground with its
   heading.
4. **Nothing pops or jumps.** A mover enters the range at its place along its route and leaves it a margin further out
   (hysteresis); far movers are basic models (instanced), near ones real models where they exist (the chain above).
5. **Unreal adds presentation only**: wheels and tracks on its rendered ground, the body's pitch and roll, people's
   steps, dust and lights, and setting things on the ground by trace as the neighbour sites are today. Nothing it
   computes goes back to the simulation.
6. **Multiplayer**: each client draws its own player's range (§5.11).

The cost (many movers at M2 when the player stands on a hill above a road) is measured from G11 with the battery and a
regiment's column in view, and any cap on the range is set from those measurements.

## 6. Track G: the phases

**Priority (the owner, 28 Oct 2026): command, battery orders and movement.** First G0's research for them, then G1-G3
(the route service, movers and the march, coordination), G8 (the visible chain) and G10 (battery orders), with G11
drawing them in Unreal; G4 as far as orders and marches need places to go to; then G9's surfaces for them; then G5.
**G6 and G7 are on hold.** The phase numbers are names, not the order.

### G0: research

First, for the priority: the march's organisation (the march plan, start point, control points, regulators, halts):
`voenizdat_osa_1991` pp. 232-239 first, then a Soviet regulation or textbook on the march of the period, to be found.
How orders went to the battery's vehicles and came back: the oral combat order and its form, the orders on the net
(the talk tables, built), repeating back, the reports a vehicle commander makes, the runner (pp. 232-246, `utin_2018`,
`pvo_bu_ix_1984`). Every battery vehicle's mobility: speeds by road and across country, swimming, slopes. Later: the
battery's field camp and daily routine (`pvo_bu_ix_1984`, the period's internal service regulation; `uvs_rf_2007` is
its later lineage); ration, water and fuel norms; the regiment's supply organisation; maintenance (pp. 206-224, with
20.0, for G6). Each value into data with its source and confidence; estimates marked as such.

### G1: the route service (simcore; not gated)

| # | Task |
|---|---|
| G1.1 | Mobility classes (`data/vehicles/mobility.json`) and their loader |
| G1.2 | Road graphs: the country's builder generalised to a district's roads and bridges; portals joining the two; road classes and speeds; integer costs |
| G1.3 | Trafficability tiles per class, built on first use and cached by the map's hash; the foot class |
| G1.4 | Requests between anchors: road first, bounded off-road links, cross-country on order, constraints |
| G1.5 | Overlays, truth and believed; the routes' dependencies; planning again |
| G1.6 | `nav_check`, `nav_bench`, `map_view.py --routes`; verify; tests |

**Done when:** routes from battery 21's home position to each of its prepared positions, to the regiment's CP in the
Lazne woods and to the Horka siding are found for each of its vehicles, with times. With the Kesmark road bridge down
they go round, and a unit not told of it still plans over it. Moving a road in a fixture map changes the routes with no
other edit. `nav_check` passes every prepared position and place of the nation's theatre, or lists it. MSVC and GCC give
the same routes.

**Built, first form (29 Oct 2026).** `simcore/nav/routes.hpp`, `nav/anchors.hpp`, `data/vehicles/mobility.json`,
`tests/test_nav.cpp`:

- **G1.1** six mobility classes (the 9A33 and the TZM from `mon_2112_80` p11, the PU-12M from `valka_pu12`, a man on
  foot from `stroevoy_ustav_1993`; the trucks and the motorcycle estimates), each speed by road class, cross-country,
  in water, and by night (`korovin_tiv_2010`).
- **G1.2** one road graph from the country's roads and the district's tracks (95 nodes, 100 edges), joined where their
  vertices meet, with the country map's crossings under their own names; the district's main roads are the country's,
  so its own tracks to the sites are the only lines added. Lengths to the centimetre, times in tenths of a second.
- **G1.4** routes between anchors (the chain's places, the theatre's objects and prepared positions, the country's towns
  and landmarks): each end off the road to the nearest point of the net, straight across country between ends near each
  other, Dijkstra with ties by node index. Battery 21's PU-12M to the regiment's CP: 29 min for a launcher, 33 for a
  truck, 4.5 h on foot.
- **G1.5** overlays: the Kesmark road bridge (`b_ralje@rava`) down, the truck to the Horka siding goes round by the
  capital road (209 min instead of 31), a unit not told still plans over it, and the route lists the entry it was planned
  round; a launcher swims the Rava where the road meets it (41 min) unless told to keep dry.
- A fixture map whose road is moved changes the route with no other edit; the fixture route's digest is pinned (MSVC's;
  CI checks GCC's).

Left: the trafficability tiles and the foot layer round the pond (G1.3), `map_view.py --routes` (G1.6). The country's
road net is sparse: 225 of its 329 other anchors (border posts, radar companies, the dam) are more than 3 km from a road,
and the last kilometres to them are a straight line across country until the tiles come.

**Built: `nav_check` (29 Oct 2026, G1.6; `simcore/examples/nav_check.cpp`, a ctest).** It builds the graph from the
map's data, lists every place and prepared position more than 3 km from a road or by a piece of the road net cut off
from the rest, routes battery 21 to every anchor, and times the build and 200 routes (`nav_bench`'s first form: the
graph in 0.1 ms, a route in 0.013 ms on today's 95 nodes). Today: one road net, no anchor without a route, 83 of the
theatre's 111 prepared positions more than 3 km from a road (the tiles' work).

### G2: movers and the march (simcore; not gated)

| # | Task |
|---|---|
| G2.1 | Movers (vehicle, column, person, carrier, convoy), levels M0-M2 and their hand-over |
| G2.2 | The march plan as a document; the planner's first form |
| G2.3 | `battery.hpp`'s march, relocation, roving ambush and service runs on routes (polylines still load) |
| G2.4 | `CrewMover`'s ground legs on foot routes |
| G2.5 | The pipeline's carriers and the chain's routines on routes |
| G2.6 | The supply lines' columns as M0 movers on the same roads |
| G2.7 | The blocked leg: stop, the fact at its first knower, a new plan once told |

**Done when:** battery 21 marches from its home position to its position by the Rava dam on the roads in its march
plan's times, the radiotelephonist logging the control points. A courier's delivery takes its route's time. A column
that meets a cut bridge stops, reports and moves on once the new route is ordered. Every mission plays as before, and
`helper_coverage`'s rows are unchanged.

**Built, first form (29 Oct 2026): G2.2, G2.3's march, G2.7, the column of G2.1** (`simcore/nav/march.hpp`,
`battery.hpp`, `tests/test_nav.cpp`):

- **The march plan** (`PlanMarch`), as the 1991 textbook has the battery commander organise the march
  (`voenizdat_osa_1991` p239, 6.3.3): the route for the column's slowest in every respect (the maintenance vehicle,
  which does not swim, keeps the column dry); a start point at the first landmark beyond the column's length, so that it
  has stretched out of its area; a halt of 1 h (by night `night_halt_s`) after every 3.5 h of movement while more than
  4 h is left (`halt_interval_max_s`, new in `battery_tactics.json`); a regulation point at the first well-seen
  landmark after each halt (a bridge, a crossroads, a village the road runs through, else the turn off the road); a
  river swum as a stop at its bank. The plan's papers: the route, the order of march (the textbook's), the spacing, the
  column's length and the time it takes to pass a point, the move-off time, each point with the head's and the tail's
  times; each vehicle commander's route card with his own times.
- **The column on the plan** (`battery.hpp`): a march order carries the plan's speed for each stretch, its points and
  the order of march; the head is held at each point until its time and stops at its halts (they replace the scheduled
  halt), and the head and the tail passing each point are noted. A march without them (every scenario's) moves as
  before.
- **The march run** (`ColumnMarch`): the battery commander at the head says each point in the cabin (radio is forbidden
  on the march, p240) and the radiotelephonist writes it in the journal on the net "march"; the record set against the
  plan is read from the journal. Battery 21 from its position to the Rava dam (132.8 km, by day): moves off 22:50, the
  start point where the battery's track meets the Vrbice road 23:00, the halt at km 108 02:30-03:30, the regulation point at the turn off the
  Hradec road 03:55, the dam 05:08, each written within a second of the plan's time.
- **The blocked leg** (G2.7): its commander not told, the column plans over the Kesmark road bridge to the Horka siding;
  its head finds the bridge down, the column stops, the commander has it to report and believes it from then; after 10
  minutes at his map (a game value) he orders the march round by the capital road, the column turned about with the
  maintenance vehicle leading, and it arrives 7.6 h after its first plan's time. A column of swimmers told of the bridge
  beforehand swims the Rava instead.

Left: the movers' levels M0-M2 and their hand-over (G2.1), relocation, the roving ambush and the service runs on
routes (G2.3), the crew's walks, the pipeline's carriers and the supply lines on routes (G2.4-G2.6, so a courier's
delivery takes its route's time), the report going up by a means (G10), and missions ordering marches by anchors.

### G3: coordination

| # | Task |
|---|---|
| G3.1 | The movement table: reservations, priorities, the commandant's regulators |
| G3.2 | The battery's own: pairs, TZMs with their pairs, the maintenance vehicle at the tail, lay-bys for short halts |
| G3.3 | The regiment's units, the supply lines, the covered troops and rebuild teams on the table |
| G3.4 | Exposure: moves by day under the enemy's reach (21.1's hook); armed reconnaissance reading the movers |
| G3.5 | Breakdowns on the march, at a fixed chance (G6's wear is on hold) |

**Done when:** two columns given the Kesmark road bridge in the same hour cross in their priority's order with the
table's gap; a TZM run waits for its battery's move; against NATO after the rollback, a column moving by day is found
more often than one moving by night.

### G4: the battery area

| # | Task |
|---|---|
| G4.1 | The layout's rules as sourced data; the generator; places as anchors |
| G4.2 | Position work and wire as jobs at places with hours (7.6, 8.14) |
| G4.3 | The battery area on the working map (14.4's first form) |

**Done when:** at every prepared position of the theatre, the battery area lays out on open ground by the textbook's
distances, every place reachable on foot and by its vehicles; a half-dug emplacement keeps what was dug.

### G5: stocks and logistics (with 15.1)

| # | Task |
|---|---|
| G5.1 | Stocks as things in places; consumption |
| G5.2 | Requests up the chain and their answers as orders (P2's means; the NPC by rule) |
| G5.3 | Deliveries as movers: the TZM to Horka and back, the fuel truck, the sergeant major's run, the field post |
| G5.4 | What the commander knows: reports and his ledger |
| G5.5 | The runs ordered and reported as battery orders (G10); their crews NPC (the TZM and its crew not playable: the owner) |

**Done when:** after twelve missiles fired at dawn, the TZMs are back loaded at a time the road to Horka and the
technical battery's test rate decide; with the rail bridge down the technical battery's own resupply comes late; the
NPC battery commander's request follows his ledger, which is wrong when a launch was logged wrongly.

### G6: wear and maintenance (20.0 and 20.3 brought forward): on hold

On hold (the owner, 28 Oct 2026). Kept as designed for when it resumes.

| # | Task |
|---|---|
| G6.1 | Wear clocks and fault chances |
| G6.2 | The kinds of maintenance as jobs at the vehicle's places; the maintenance vehicle's capacity; ZIP spares |
| G6.3 | Repairs: by drill, by the maintenance crew, by evacuation on tow |
| G6.4 | Playable maintenance with the helper's steps |

**Done when:** a battery that skips maintenance for three days has more faults than one that does not; a radar open for
maintenance when «Воздух» comes takes its drop time; a player crewman replaces a failed block by the helper's steps and
the function check passes.

### G7: the battery's day (with 15.4, 19 and 20): on hold

On hold (the owner, 28 Oct 2026). Kept as designed for when it resumes; recreation's minigames come later still.

| # | Task |
|---|---|
| G7.1 | Activities as data at places, on the movement service (20.1) |
| G7.2 | Readiness computed (20.2); the duty roster (19.3) |
| G7.3 | The schedule: the NPC planner and the board (20.4) |
| G7.4 | Interruptions (20.5) |
| G7.5 | Needs: sleep, food, morale (19.2's first slice); recreation's effects |
| G7.6 | The director's wake events and compression through the day (15.4) |

**Done when:** «Воздух» at 05:10 finds 21-3's operator asleep and its radar under maintenance; readiness comes when both
are back, and the debrief says so. A battery on shifts is fresher on day three than one always at its posts.

### G8: the visible chain of command (with P1, P2, P8)

| # | Task |
|---|---|
| G8.1 | One people registry: the chain's holders and the battery's roster |
| G8.2 | People at their places, moving by their routines |
| G8.3 | Papers: the communications diagram, the call-sign table, the staff list, the order, the ledger; dated, corrected by facts |
| G8.4 | Contact by means from the diagram (P2): telephone, radio, courier, in person |
| G8.5 | The evening briefing at the regiment's CP as a place driven to (P1's scene) |
| G8.6 | The chain changing where the player can see it: a holder replaced, the alternate CP (P8, 21.4) |

**Done when:** the player as battery commander finds the regiment's duty officer on the diagram and telephones him on
the wire; with the wire cut he sends the sergeant major with a written report, who arrives by road in his route's time;
a vehicle commander replaced after a raid is a new name on the staff list only once the battery commander is told.

### G9: the player's surfaces

| # | Task |
|---|---|
| G9.1 | The working map table in the PU-12M (with 14.4) |
| G9.2 | Battery orders and requests in the radial menu; their places in the crew data's `"menu"` |
| G9.3 | The route card and the driver's helper |
| G9.4 | Papers in the map case |
| G9.5 | The campaign screen's relocation as a march with a time; the screen kept for what is outside the character once 15.8 exists |
| G9.6 | Whom the player plays, first form (19.5): the battery commander, a vehicle commander, a driver (no support role: the TZM and its crew are not playable) |

**Done when:** a player battery commander orders the march, the halt, the occupation and a relocation from the radial
menu and the working map, and requests the TZM; a player driver follows the route card with the helper's next turn;
the papers are in the map case; the campaign screen's relocation is a march with its time.

### G10: battery orders, given and carried out (simcore; not gated)

| # | Task |
|---|---|
| G10.1 | The order as a record (`simcore/command/orders.hpp`): giver, recipients, content, means, words; each recipient's state and times |
| G10.2 | Each vehicle's commander and driver as people at level C1 (§5.5), with their skill tiers; the levels C1 and C2 and the hand-over when the player takes a seat |
| G10.3 | The means: the orders group in person, the battery net with call signs and talk tables, the wire, flags and lamps, the runner; who hears by `voice_comms.hpp` and `radio_nets.hpp` |
| G10.4 | The giver: acknowledgements, a silence past its time, the repeat, the next means, the runner; his picture from the reports |
| G10.5 | Carried out: `Battery::Order` applied per vehicle once its commander has it; each act through `NpcDoes`; a vehicle not reached waits and catches up by its route card |
| G10.6 | Reports (ready to march, control points, arrived, ready for battle, cannot) on the net, logged in the radio journal; the debrief's times per order and recipient |
| G10.7 | The regiment's orders down the same path; the emission regime's (8.16) moved onto it |
| G10.8 | The player on both sides: giving from the radial menu and the working map; receiving in a vehicle commander's or driver's seat with the replies and the helper; each order and reply a place in the crew data's `"menu"` |

**Done when:** the battery commander gives «К маршу!» on the battery net; 21-3, whose set is off, stays in its
emplacement until the runner reaches it, then follows by its route card; every vehicle's «К маршу готов», control
points and «к бою готов» are in the radio journal; the debrief shows each order's times for each vehicle; the player
as 21-2's commander hears the order, repeats it back and has his driver follow the route card, with the helper's
moves; the emission regime travels the same path as before; `helper_coverage` shows the vehicle commanders' new
duties with no new gaps.

**Built: G10.1 (29 Oct 2026).** The order as a record (`simcore/command/orders.hpp`, `OrderBook`): who gave it, what,
in what words and by what means; for each recipient the hour of every step from given to logged (each step needs the one
before it, and carrying out needs only "heard", so nobody acts on an order he did not hear), a "cannot" with its reason,
and every sending. The giver's rule for a silence (`ChaseRule`, game values: 30 s, twice more on the same means, then the
telephone, then a runner) says whom to send to again, when and how. The test gives «К маршу!» on the battery net with
21-3's set off: said again twice, the telephone (no wire yet), then a runner who reaches 21-3 in his foot route's time
from the route service (14 min 49 s), and the debrief lists each vehicle's times to the second.

**Built: G10.2-G10.5, first form (29 Oct 2026).** `simcore/command/battery_orders.hpp` (`BatteryOrderNet`) carries an
order by a means to each vehicle commander, a person from the chain with his skill tier's times (`CommanderFromChain`,
level C1). The battery net reaches him while his R-123M is on, warm (10 min), on the net and not jammed, and it is said
once; flags by day and lamps by night reach a vehicle in sight, each commander passing the signal on; the field
telephone where a line is laid; a shout in earshot; a runner walking the route service's way on foot. He hears it, acts
after his reaction, and repeats back by the first way back he has (the net once his transmitter is ready, the signal
back, the telephone, the runner returning). The giver's silence rule, now data (`data/command/battery_orders.json`),
skips the telephone where no wire is laid, and a commander who hears with his set off switches it on. `Battery` keeps a
vehicle whose commander has not heard where it is (`SetAbsent`) and lets it go when he acts (`Join`): out of its
emplacement in its own time, and a column already gone it catches up along the route at its own class's speeds, to its
place. `ColumnMarch::UseOrders` runs the march on it: «К маршу!» on the battery net at the move-off, each «К маршу
готов» on the net once the vehicle is out (radio stays forbidden in the column on the march, p240, so only a vehicle out
of it calls), «К бою!» by flags down the column at the end, each «К бою готов» on the net, every report written in the
radio journal and logged in the book. The test (`test_nav.cpp`): 21-3's set is off; said twice more on the net, no wire,
a runner reaches it 18 min after the order; it switches on, repeats back on the net 13 min later (the warm-up and the
transmitter), leaves its emplacement and falls in 42 min after hearing, before the halt; every vehicle's two reports and
every point are on the page, and the debrief lists each order's times for each vehicle. Left: the session
(`ScenarioRunner`'s battery orders through the net, each NPC act through `NpcDoes`, the player's own vehicle at C2), the
driver as a person, the orders group in person and the talk tables' words on the net (G10.3), the giver's picture from
the reports on the working map (G10.4), "cannot" (a vehicle that cannot catch up), the regiment's orders and the
emission regime on the same path (G10.7), the player's side (G10.8).

### G11: Unreal, everything the player could see

| # | Task |
|---|---|
| G11.1 | Movers in the entity stream Unreal reads (`USimWorldSubsystem::GetEntityStates`): the battery's vehicles and people, other units' columns, convoys, carriers, each with its model |
| G11.2 | The visual range from the player's eye or optic, the weather's visibility and the light; hysteresis; M2 inside it (§5.12) |
| G11.3 | The battery's places drawn as basic models: emplacements, the PU-12M's mast and wire runs, the regulator's post (the camp's tents with G7) |
| G11.4 | Presentation only: grounding, wheels and tracks, pitch and roll, people's steps; instancing; a frame-time budget measured |
| G11.5 | A scripted check in the game (as the missions menu check runs): the column forming and marching, every vehicle counted moving; a convoy appearing at the range and leaving it |

**Done when:** from the PU-12M's roof the player watches the whole column form up and move off, every vehicle on the
road; from a hill the regiment's convoy appears on the far road at the weather's visibility and is gone behind the
ridge; nothing appears or vanishes inside the range or jumps on entering it; the frame time with the battery and a
regiment's column in view is within the budget measured.

### Milestone M-G: a night and a dawn at battery 21

A person plays §2 as the battery commander: drives to the 19:00 briefing and gets the order, plans the night march on
the working map, gives the oral combat order at the PU-12M, orders «К маршу!» and gets 21-3 moving by runner, watches
every vehicle form up and march in Unreal, is held by the regulator at the bridge, occupies and reports ready, fights
the dawn raid, and takes the regiment's order to relocate; the TZM's run to Horka goes on around him, its crew NPC.
Then the same night from 21-2's commander's seat: the order heard on the net, repeated back, carried out with the
helper.

### Order

```
G0 research ─> G1 route service ─> G2 movers and the march ─┬─> G3 coordination ────────────────────────┐
                                                            ├─> G10 battery orders <── G8 the chain ────┤ <── P1, P2
                                                            ├─> G11 Unreal: what the player could see ──┤
                                                            └─> G4 battery area ────────────────────────┤
                                      14.3, 14.4 ──────────> G9 the player's surfaces ──────────────────┴─> M-G
                 15.1-15.2 the battery's state, the session ──> G5 stocks and logistics
 On hold: G6 wear and maintenance, G7 the battery's day
```

G1-G4 and G10 are simcore and headless and need no gate, as the nation on the map did not. G8's registry and papers
start headless, its scenes with P1 and P2. G11 needs only today's Unreal entity stream to start. G9 needs the working
map (14.4). G5 needs the battery's state in a running session (15.1-15.2) for anything beyond `battery_day_demo`.

## 7. How it fits the existing plan

| This proposal | Already planned or built | How they meet |
|---|---|---|
| The route service (G1) | `country_map.hpp`'s routes; `COUNTRY_MAP.md`'s "what comes next": road classes and land cover off the road | New; takes those two items |
| Movers and the march (G2) | 7.5 (`battery.hpp`), 14.5 (the mission kinds), 15.6 (districts), 17.4 (supply over the real roads) | New foundation; 17.4 becomes a use of it |
| Coordination (G3) | Nothing | New |
| The battery area (G4) | 14.3 (positions), 7.6, 8.14 (jobs at places) | Adds the camp and its places to 14.3 |
| Stocks and logistics (G5) | 13.4 (the depots' consequences), 17.4, 20.1 (the sergeant major's run) | Brought forward; NPC crews |
| Maintenance (G6, on hold) | 20.0, 20.3 | Would bring them forward with a playable part |
| The day (G7, on hold) | 15.4, 19, 20 | Would use their models with a first slice of needs |
| The visible chain (G8) | C5, P1, P2, P8, 21.4-21.6 | Joins them with papers and people |
| The surfaces (G9) | 14.4 (the working map), 8.15 (the menu), 19.5 | Extends them |
| Battery orders (G10) | 8.8 (the order menu), 8.16 (the emission regime's path), 14.3's oral combat order, 14.4, P1 | Generalises 8.16's path to every battery order; 14's done-when (NPC vehicle commanders who carry the order out) rests on it |
| What Unreal draws (G11) | N5 (the nation's assets drawn), the entity actors, 15.8 | New: the movers and the visual range |

## 8. Open questions and the owner's answers (28 Oct 2026)

| # | Question | Answer |
|---|---|---|
| 1 | Does track G run now and ungated, as the nation on the map did, or wait for the gates of phases 13-21? | **As recommended**: G0-G4 now, headless and ungated; G10 and G11, added after the review, likewise; G5 and G9 joined to 15.1-15.2, 14.4 and P1-P2 |
| 2 | Does the roadmap get a "track G" section, with phases 14, 15, 17, 19 and 20 pointing to it, or are G's tasks folded into those phases? | **As recommended**: a track G section in `docs/DEVELOPMENT_ROADMAP.md`; those phases point to it, so nothing is planned twice |
| 3 | How fine is the off-road layer, and where? | **As recommended**: the obstacle grid's spacing in districts; networks only in the country |
| 4 | What does a save do when the map has changed? | **As recommended**: record the map's hash; on a mismatch load the battery's state snapshot instead of replaying |
| 5 | Which support roles become playable first? | **None in track G**: the TZM and its crew are not playable |
| 6 | How deep does recreation go? | **Minigames later, not now** (G7 is on hold) |
| 7 | Unreal's navigation mesh? | **Open.** The owner's requirement: every battery element moves in Unreal, and every element within the player's visual range appears (G-14, §5.12, G11). The recommendation stands: no navigation mesh decides where anything the simulation moves goes, because the server must move things for every client, the headless tests and the campaign's M0 movers have no Unreal, and a level's navigation mesh covers only the loaded level, not the country. It is not needed to meet the requirement: Unreal draws what simcore moves. If one is ever wanted, it is for presentation within a few metres (a man stepping round another in the camp), never for where anyone goes |

## 9. Risks

- **Scope.** It touches every layer. The order (headless first, one playable day as the milestone) keeps each phase
  testable alone.
- **Performance** of tiles and many movers: measured by `nav_bench` from G1; M0 or M1 for everything out of view. In
  Unreal, many movers in view from high ground: measured in G11, instanced basic models when far.
- **Orders that never arrive**: an order not heard stalls a vehicle by design, so the giver's repeat and runner rules
  (G10.4) must be in place before any mission depends on them, or a mission can stall with no one noticing.
- **Determinism across compilers**: integer costs, index tie-breaks, a route-hash test in CI.
- **First person against "visible"**: the papers can be wrong and out of date by design; no chart of the truth.
- **Old scenarios' hand-written routes**: they keep loading; `helper_coverage` must not change.
- **Unreal art drifting from the simulation's map**: the level check of §5.1.
