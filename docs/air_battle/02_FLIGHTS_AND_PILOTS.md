# 2. Flights and pilots

Code: `simcore/include/simcore/aircraft.hpp`, `world.hpp`, `target_behaviour.hpp`, `scenario.hpp`, `scenario_runner.hpp`
(`UpdateTargets`, `Instruct`, `AddTargets`), new `simcore/flights.hpp`; `air_war/raid_scenario.hpp`, `test_waves.hpp`,
`mission_bridge.hpp`, `package_injector.hpp`, `state.hpp`, `data.hpp`, `officers.hpp`, new `air_war/pilots.hpp`;
`prototype/samproto/engagement.py`; Unreal's `FSimEntityState`. Research: `docs/research/air_battle_2026-11/`
`air_combat.md` (formations), `factions_cases.md` and `perception.md` (hours, experience, human factors), `routing.md`
(formations through corridors), `reactions.md` and `c2_retasking.md` (who decides in the air); code maps `map/flight.md`,
`map/routes.md`, `map/c2_people.md`, `map/scale.md`.

The war's year is 1985. A raid in a session today is a set of lone aircraft, each a constant-speed point that turns at a
fixed rate, with only a squadron leader as a person. This document keeps the flight and the package alive in the
session, puts a command layer between the pilot and the aircraft, gives the aircraft energy and turn limits, makes the
pilots people, and sets the levels of detail that let a hundred of them fly within the step's budget. Perception,
reactions, radio, retasking and air combat are their own documents in this folder; they act through what this one
builds.

## 1. The flight and the package in the session

### 1.1 Today

`RaidScenarioBuilder::Build` writes one `ScenarioTarget` per aircraft and shifts each one's whole route sideways by
`(k - (n-1)/2) x 800 m` (`scenario.flight_spacing_m`) at right angles to the first clipped leg, so the line abreast is
lost in the first turn (raid_scenario.hpp:441-448). The testbed does the same with `Spread` (test_waves.hpp:343-350).
Flight membership survives only in the bridge's manifest (`RaidAircraft::flight`, `index`; raid_scenario.hpp:205-236);
the runner has no leader, wingman, package commander or shared decision. The leader's nerve exists only in the abstract
resolver (resolve.hpp:117-122, 250-255). `MissionBridge::Finish` calls a flight "aborted" when it lived and did not
release: geometry, not a decision (mission_bridge.hpp:297).

### 1.2 Who is who

| Slot | NATO | Warsaw Pact | Neutral | Rebels |
|---|---|---|---|---|
| Package commander | a package leader, trained as such: the Tactical Leadership Programme from 1978, at Jever from 1979, 8 courses a year by 1983, every USAFE fighter wing through it in 1982 (`afmag_1983_usafe`, `tlp_official_history`) | the leader of the group, under the KP's control; the plan, the route and the coordination are fixed before take-off (`fm_100_2_1_1984` ch. 12) | a leader, no package structure found (SAM3D assumption) | none: each sortie its own (`c2_retasking.md` c2-41, SAM3D assumption) |
| Flight leader | decides within his fragged mission: attacks, takes the frag's alternate target, aborts, jettisons, evades; no new target outside the frag (`hoak_1987_retasking`, `schoeck_1987_ai_retasking`, c2-05) | flies the plan; assigns individual targets in the flight once the FAC confirms (`fm_100_2_1_1984` pp. 12-4, 12-7 to 12-8) | as NATO (assumption) | the pilot is his own retasker |
| Element lead (No. 3 of four) | leads the second pair; takes the flight if the leader is lost (SAM3D rule) | leads the second pair; the same rule | as NATO | none |
| Wingman | in 1985 a fighting partner, not a "welded wing" (`anderegg_2001` pp. 20-21, 59, 82-84) | the shield; from the 1982 debate "every pilot both a shield and a sword", the strike to whoever is better placed (`johnson_1986`, `pennington_1985`) | as NATO | alone or a loose second |

The package commander is `Package::leader` where an officer flies (planner.hpp:1342-1349); otherwise the first main
flight's leader. He decides for the package only what his faction lets a flight leader decide (turn back, the
alternate, a time to press on); everything else comes by voice from a ground node, per `05_C2_AND_RETASKING.md`.

### 1.3 Formations by faction and phase

| Faction, phase | Formation | Source |
|---|---|---|
| NATO fighters, sweep and escort | pairs line abreast about a mile (1.6 km) apart, stacked "a couple of thousand feet" (about 600 m); engaged and free fighter swap roles; turns by "delayed 90s" without calls | `anderegg_2001` pp. 59-60, 82-84 |
| NATO strike, low-level transit | very loose twos or fours, no R/T except to call a fighter threat, precise timings; four-ships at 100 ft in work-ups | `rafhs_j20_1999` (Pitchfork; RAF crews at Red Flag from 1977) |
| NATO, low under the weather | spread line abreast so each aircraft watches for MiGs and SAM launches; radio assumed jammed | `anderegg_2001` pp. 60-61 |
| NATO-like pair search (Israel 1982, Soviet reading) | about 1,000 m line abreast, lead searching 60° up, wingman 60° down | `dubrov_1983` |
| NATO strike group to the target | close, split only before the target (Israel 1982, Soviet reading) | `dubrov_1983` |
| NATO at medium altitude (only where the defence is broken) | Vietnam's pod formation as lineage: wingman 1,500-2,000 ft out and 500-1,000 ft up, element 2,000-3,000 ft back; close up 60 s before release | `checo_1967_tactics`; pre-period, used only for a medium-altitude phase |
| Pact fighters | pairs inside groups of four to eight under ground control; initiative to the flight leader at contact (best units, after 1984) | `johnson_1986`, `pennington_1985`; the 4-8 figure in `mt_turchin_coop` is the enemy's first echelon as the Pact saw it, so for the Pact's own groups it is a SAM3D reading |
| Pact strike | approach at 50-100 m, radio "reduced to a minimum or prohibited", re-attack from another direction or out of the sun; the FAC confirms, then the leader assigns targets | `fm_100_2_1_1984` p. 12-8 |
| Pact strike, set sequence (Afghanistan) | a designation pair of the best pilots searching for 3-5 min at 800-1,000 m; a suppression group of 2-6 1-2 min after the marking; the main group 1-2 min after that | `markovsky_afghan_sky` (local war: a pattern, not a European rule) |
| Pact at night | a column of pairs at different heights | `mai_tactics_vvs` §3.3.3 (via `map/routes.md` §10.1) |
| Pact helicopters | in Europe under the air defence umbrella, through corridors between artillery concentrations; in Afghanistan pairs in echelon at 15-20°, 600-800 m apart on armed reconnaissance, 1,200-1,500 m between helicopters on the way against ambush, night pairs 80-100 m apart in height | `fm_100_2_1_1984` pp. 12-5, 12-8; `airwar_mi24_markovsky` |
| NATO helicopters | low level, contour, and NOE weaving along a broad corridor of terrain features; NOE seldom at night | `fm_17_50_1977` ch. 3; no pair spacing found |
| Neutral | NATO-style pairs, short low routes by day | design inference (`routing.md` routing-29), low confidence |
| Rebels | single aircraft or a loose pair, low, at night | design inference from `biafra_minicoin`, `katanga_fouga` |
| Our side | the Pact pattern: pairs under GCI, falling back to independent search in pairs or abort when control is lost | `mt_turchin_coop`, `pennington_1985`, `lambeth_1984` (our-01) |

Where a spacing is not in a source (Pact fast-jet pairs, NATO helicopter teams, the neutral, the rebels, the low-level
line abreast) it is a game value and says so in the data. Fluid Four's "welded wing" (`anderegg_2001` p. 20) is
pre-1975 and is not a 1985 formation.

**The phases** a formation is given for: `transit` (own side, to the gate or checkpoint), `ingress` (past it, to the IP),
`attack` (IP to release: pairs or singles by the delivery and the leader's assignment), `egress`, `defensive` (under
attack), `orbit` (CAP, jammer, hunter). The phase is the route leg's tag (`RoutePoint::tag`) or the flight's order.

### 1.4 How a flight decides together

One decision-maker per flight: the leader. A wingman sees, hears and calls; the leader decides; the flight follows.
Three kinds of act:

- **Flight acts**, by the leader only: press on, turn back, take the alternate, jettison (whole flight or one element),
  re-attack, split for the attack, rejoin. Each is said on the flight's net (`09_COMMS_AND_INTERCEPT.md` makes it an emission)
  and reaches each member after the call's time; a member who did not hear it (jammed, out of range) keeps the last
  order. Silent packages (NATO comm-out, Pact radio forbidden at low level) act on the briefed plan and visual signals:
  a wing flash, a turn seen (`anderegg_2001` pp. 59-60).
- **Pilot reflexes**, by each pilot: a defensive break on his own warning or sight, jettison when hit. These do not
  wait for the leader (`c2_retasking.md` §1.2 point 4: "threat warnings are not retasking").
- **Package acts**, by the package commander: the package turns back once its losses reach his nerve. The session uses
  the abstract resolver's rule, `0.25 + 0.3 x boldness` plus quirks (resolve.hpp:117-122), counted on the losses the
  commander **knows** of (seen or called), so the abstract and played raids keep one rule.

### 1.5 When a flight splits

| Cause | What happens | Source |
|---|---|---|
| The attack | elements or pairs take their assigned aim points and directions; the flight rejoins at the egress point | `fm_100_2_1_1984` p. 12-8 (Pact); `dubrov_1983` (split only before the target) |
| A threat to one aircraft | that aircraft breaks; the rest hold or cover by doctrine (NATO: the leader drops chaff, No. 3 breaks, Falklands 1982) | `pook_1985` |
| Fighters on the flight | NATO strike: jettison and turn into them or press on by fuel and escort; Pact fighters jammed off GCI: pairs and singles in figure-eights | `checo_1969_tactics`, `usn_falklands_1983`, `lambeth_1984` |
| The leader lost | the element lead takes the flight; a lone wingman joins the element or goes home | SAM3D rule (no period number) |
| Contact lost (cloud, terrain, a break) | each goes on to the next turn point and rejoins there, or alone by the plan | SAM3D rule; game value for the distance at which "lost" is called |
| Damage | the damaged aircraft goes home; whether a wingman escorts it is the leader's call, a game value per faction | not found |

### 1.6 The data and the run-time state

Formations per faction go in the faction files as a `"flights"` block: per type class (fighter, strike, helicopter)
and phase, the slots as `[right_m, back_m, up_m]` from the leader, with `source` and `confidence`; game values say so.
The loader rejects unknown keys as `data.hpp` does.

In the session (`simcore/flights.hpp`, header-only, held by the runner by `std::unique_ptr` as the devloop branch puts
pieces with their own state):

```cpp
struct FlightSlot { int target = -1; int number = 1; Vec3 offset_m; bool alive = true; };
struct FlightState {
    std::string id, callsign, package, side;      // side: "enemy" or "ours" (08_OUR_FIGHTERS.md's OwnFlight flies as one)
    std::vector<FlightSlot> slots;                // 1 = leader, 3 = element lead
    int leader_slot = 0;
    FlightOrder order;                            // section 2
    RoutePlan plan;                               // legs with speeds and heights, the place along them
    std::string phase;                            // transit, ingress, attack, egress, defensive, orbit
};
struct PackageState { std::string id; int commander_flight = -1; std::vector<int> flights; int losses_known = 0; };
```

`ScenarioTarget` gains `flight` and `slot`; the scenario gains `"flights"` and `"packages"` lists; a target with no
flight is a flight of one, so every hand-written scenario still loads.

## 2. A command layer between the pilot and the aircraft

### 2.1 Today

Every decision overwrites `Aircraft::waypoints`: a break or dive leaves a 30 km beam point as the only waypoint and the
aircraft never returns (target_behaviour.hpp:209-214), an ARM carrier's turn-away writes a 40 km leg
(scenario_runner.hpp:2892-2900), an abort jumps to the last waypoint (:2660), the instructor saves and restores a route
for at most two held targets (`kInstructorTargets`, :11251-11257). Only the jink and the instructor go back to a route
(`TargetPilot::Route`, target_behaviour.hpp:134-137).

### 2.2 The layers

```
RoutePlan      the briefed route: legs with speed, height (agl or altitude), tag; never overwritten
FlightOrder    what the flight is doing now, and who ordered it
SlotKeeping    a wingman's station on the leader, by the formation and the phase
PilotOverride  a pilot's own manoeuvre (break, dive, jink, attack run) with an end condition
AircraftDemand heading or turn, height or path angle, speed, the load factor allowed
FlightModel    section 3
```

```cpp
enum class OrderKind { FollowRoute, Hold, Orbit, Attack, Rejoin, ReturnHome, Divert, Evade };
struct FlightOrder {
    OrderKind kind = OrderKind::FollowRoute;
    int from_leg = 0;             // FollowRoute: the leg to resume
    std::vector<RouteLeg> legs;   // Divert: a new route from here (the runner's API)
    Vec3 aim;                     // Attack, Orbit
    double until_s = -1.0;        // Hold, Orbit
    std::string by;               // "leader", "package", a ground node's id (05_C2_AND_RETASKING.md's authority)
    double at_s = 0.0;            // when it reached the flight
};
struct AircraftDemand { double heading_rad = 0, path_angle_rad = 0, speed_mps = 0, max_load_g = 0; bool level = true; };
```

A pilot override ends by its own rule (the jink's `evade_s`, a break when the missile is past or the warning stops, an
attack run at its egress point) and the aircraft then rejoins its flight's slot or, alone, its order's route. Today's
"break ends the mission" goes; `test_threats.cpp`'s break and dive cases (:375-624) change on purpose.

### 2.3 The runner's API

```cpp
bool ScenarioRunner::OrderFlight(const std::string& flight, const FlightOrder& o);   // false: unknown flight, refused
bool ScenarioRunner::RerouteFlight(const std::string& flight, std::vector<RouteLeg> legs, const Vec3& aim,
                                   const std::string& by);
const FlightState* ScenarioRunner::Flight(const std::string& id) const;
std::vector<std::string> ScenarioRunner::FlightsOf(const std::string& package) const;
```

No cap on how many flights are steered. Each call is logged with its `by`; the runner refuses a `by` the faction's
authority table does not allow for that flight's mission (`C2Authority::May`, `05_C2_AND_RETASKING.md` §4.2). These two
calls are the only way a C2 post (`05_C2_AND_RETASKING.md` §4.5) or our GCI (`08_OUR_FIGHTERS.md` §4) steers a flight.
A reroute moves the release point in the manifest, so `MissionBridge::Engage`'s neighbour passes, fixed at zone entry on the present
heading (mission_bridge.hpp:539), are re-decided on the new legs. The instructor's Take, Turn, Height, Speed and Release
become orders on a flight of one, so the trainer keeps its behaviour and its two-target cap as a trainer rule only.

### 2.4 What changes where

- **`ScenarioTarget`**: `flight`, `slot`, and `legs` (each with `speed_mps` and `agl_m` or `alt_m`) in place of one
  `speed_mps` for the whole route. `waypoints` stay for a flight of one and as the plan a wingman falls back on.
- **`TargetPilot`**: keeps the height layer (`HoldHeight`, `GroundFloor`, pop-up) and gives up steering. `Evade` returns
  a `PilotOverride`, not waypoints; `OnLaunch`, `OnLocked` and `EvadeNow` become one entry point,
  `Notice(const ThreatCue&)` (`04_REACTIONS.md` §12.2), fed by the sightings of `03_PERCEPTION.md`.
- **`RaidScenarioBuilder`**: writes `"flights"` and `"packages"` from the planner's `Flight` and `Package`; slots from
  the faction's `"flights"` block for the type class and phase; no `scenario.flight_spacing_m`; per-leg speeds from the
  route's own times (cruise on transit, `attack_mps` from the IP, as `01_ROUTES.md` §6.6 gives `TimeRoute` per leg)
  instead of the clipped length over the clipped time (raid_scenario.hpp:475-478). The manifest's `RaidAircraft` keeps
  its fields and adds the pilot's id (section 4).
- **`TestWaves`**: `EnemyFlight` and `FriendlyFlight` write flights; `Spread` goes.
- **`MissionBridge::Finish`**: `aborted_flights` from the flight's own order (`ReturnHome` before release), and new
  outcome lists: flights that jettisoned (`jettisoned_flights`, `04_REACTIONS.md` §13), took the alternate, were
  diverted (`retasks`, with the order's `by`, `05_C2_AND_RETASKING.md` §4.5), the route each flight actually flew
  (`01_ROUTES.md` §9), and each pilot's fate (section 4).
- **`test_packages_live.cpp:159`** (a raid injected flies as the same raid played alone) must still pass.

### 2.5 As built (B1, 15 Nov 2026)

B1 is built in `simcore/flights.hpp` (the session's flights and packages, the orders, slot keeping and the pilot's
override), `AircraftDemand` in `simcore/aircraft.hpp` (mirrored in `prototype/samproto/engagement.py`), the runner, the
loader, the raid builder, the testbed's waves and the mission bridge (tests `tests/test_flights.cpp`). Where it differs
from 1.6 and 2.2-2.4:

- **The step.** Before the world moves, `Flights::Steer` sets each aircraft's demand: a pilot overriding flies his
  override's point at its turn rate; a wingman in contact flies his station; the leader, or one on his own, flies his
  order (FollowRoute, Rejoin and ReturnHome on his own waypoints, with no demand). After it moves, `AfterMove` ends an
  override, keeps a wingman's place on the plan (`next_waypoint`) with his leader's and sets the leg's speed from the
  target's `"legs"`; after the step, `After` delivers the calls and the packages' losses. An aircraft with no demand
  flies exactly as before, so a flight of one with no order is the old target to the bit.
- **Slot keeping.** The slot is the faction's (`FormationSlot`: the phase's table, else the transit's, else the first;
  a place past the table repeats it further back by its depth and 1.5 km; with no table at all an echelon right 800 m
  apart, a game value), turned to the leader's track. The phase comes from the leader's leg tag (`PhaseOfTag`), his
  order or his override. A wingman flies for a point 4 s of flight (600 m at least) ahead of his station, at the
  leader's speed plus 0.05 m/s for every metre he is behind it, within 30 % of the leader's speed. Farther than 10 km
  from it he has lost the leader and flies his own plan until within 6 km. All game values. After a 90 degree turn a
  wingman 1-2 km out is back on station within about a minute (the station swings with the leader's track; tactical
  turns are B8's). **In the attack** (the leader on a leg tagged for it: pull-up, apex, roll-in, target, dive,
  launch) each flies his own run on his plan's attack points, in trail on the leader's track, and takes his slot again
  after it: the height layer's loft, pop-up and dive are flown by each aircraft, and each releases at his own point.
- **A wingman's plan** (the builder's and the waves'): his start is set off by his whole slot, his turn points and his
  release point only to the side: a man in trail passes the leader's turn point when he gets there (set back by the
  slot's depth, he would turn inside the leader and release short).
- **Orders and calls.** `Flights::Order` gives the leader the order now and each member who hears the flight's net
  after `call_s` (3 s, a game value; `hears` is B9's to clear); one who did not hear keeps the last order he heard.
  `OrderMember` orders one aircraft (the instructor's, a damaged one's turn for home) and may detach it from the
  formation. Every order is recorded with its `by` (`FlightState::orders`). `Flights::May` accepts every order until
  B5.
- **Succession.** The leader lost, gone or detached: No. 3 takes the flight, else the lowest number left, and the rest
  close up behind him in their numbers' order; the debrief says why ("lead lost", "the lead has left", "the lead turns
  for home").
- **The package's nerve.** A package's commander hears of a loss in his own flight at once, of one in another flight
  after its call from a survivor (a flight lost whole is not heard of), and counts those lost before it came in
  (`lost_before`). When the losses he knows reach `turn_back_share` of its size (the resolver's `PackageNerve`,
  0.25 + 0.3 x boldness, factored out of `resolve.hpp` so both use it) every flight short of its release (not in its
  egress) is ordered home `by "package"`. A flight with no crew (cruise missiles, decoy drones: a scenario flight's
  `"crewed": false`) never turns back and is no loss to its package.
- **The pilot's override.** `TargetPilot`'s `OnLaunch`, `OnLocked` and `EvadeNow` return an `Evasion` (a
  `PilotOverride` and the debrief's text): break and dive for a beam point, the jink's flips, each until `evade_s`,
  then back to the slot ("rejoins its flight") or the route ("back on its route"). The ARM carrier's turn-away is an
  override to a point 40 km away that ends where the point is passed as a waypoint is (`AircraftDemand::capture`) and
  takes the aircraft out of the session there, as its old one-waypoint route did; a leader's flight follows him home.
  `Notice` and perception are B3's and B4's.
- **The overwrites replaced.** Break and dive (`TargetPilot::Evade`'s waypoints), the ARM turn-away's 40 km leg, the
  damaged aircraft's jump to its last waypoint (now `OrderMember` ReturnHome `by "pilot"`, detached), and the
  instructor's saved routes (`route_waypoints`, `held_heading_rad`: his Take, Turn and Speed are a heading order on a
  flight of one `by "instructor"`, Release rejoins the flight). The two-target cap stays as a trainer rule.
  `engagement.hpp`'s `evade_on_launch` (the gameplay engagement model's own target, not a session's) is untouched.
  On purpose, the hand-made scenarios with an `evade` fly differently: a break or dive now ends after `evade_s` and the
  aircraft flies its route again ("back on its route"), and through a jink, break or dive the height layer holds the
  height of the leg it is on (the old overwrite put it on its first waypoint's, so a terrain follower dived to it).
- **Talk.** An engagement order of the commander's still waiting its turn on the intercom when he orders the
  transmitters off against an ARM is taken back (`CalloutBus::Withdraw`): the changed timings had him say «Цель —
  взять!» after «Излучение выключить!».
- **The scenario.** `"flights"` (`id`, `callsign`, `package`, `side`, `keep_slots`, `crewed`, `call_s`, `formations` by phase),
  `"packages"` (`id`, `commander`, `size`, `lost_before`, `turn_back_share`), and a target's `"flight"`, `"slot"` (1-16)
  and `"legs"` (one per waypoint: `speed_mps`, `agl_m` or `alt_m`, `tag`). Strict keys. A target with no flight is a
  flight of one named by its id. Flights added in play (`ScenarioRunner::AddFlights`, the package injector and the
  waves) register with their targets.
- **The runner's API** is 2.3's with an optional `why` for a refusal, and `AllFlights()`. `RerouteFlight` sends a
  Divert on the new legs with the aim; the bridge's `MissionBridge::Reroute` moves the members' release and aim in the
  manifest and re-decides the neighbours' passes on the next step.
- **The raid builder** writes flights and packages (the planner's flights by callsign, one package with the
  commander's flight and the nerve), each member's start and waypoints placed on its slot by the leg's phase, and a
  speed per leg from the leg's planned time (between 20 m/s and the larger of 600 m/s and 1.2 x cruise).
  `scenario.flight_spacing_m` is gone. The testbed's waves write flights the same way (`Spread` is gone; our own
  flights an echelon right 100 m and 300 m deep, a game value).
- **The bridge** takes `aborted_flights` from a flight's ReturnHome order before its release (or every live member
  sent home on its own), and lists `retasks`: orders from anyone but the leader, the package or the pilot, with their
  `by`, kept in the save.
- **Cost.** The layer (`Steer`, `AfterMove`, `After`) takes about 0.01 ms a 0.05 s step for 100 aircraft in 25 flights
  of four (`test_flights.cpp`'s cost test prints it; one core, Release), well inside 5.2's 0.5 ms.
- **Left.** B2: `TimeRoute` per leg in the planner (the builder takes speeds from the clipped route's times), the
  heights per leg as demands. B3/B4: `Notice`, the cue chain, the press-on-or-abort decision for the flight, the
  break's end on the missile passing, jettison and `jettisoned_flights`, the alternate, escorts staying with a damaged
  wingman. B5: `C2Authority::May` and who may order what. B8: tactical turns, splits for the attack against several
  aim points (each element its own target), the fighters' formations in combat. B9: the calls as transmissions (`hears`). Pilots' ids in the
  manifest and their fates (section 4). The route each flight actually flew in the outcome.

## 3. The flight model beyond R3

### 3.1 State and equations

R3 (`docs/DEVELOPMENT_ROADMAP.md` R3) is a point mass flown by bank-to-turn. This adds energy. `Aircraft` gains
`heading_rad`, `bank_rad`, `pitch_rad`, `path_angle_rad`, `load_g`, `throttle` and a variable airspeed `speed_mps`;
`velocity` stays the ground velocity (wind included). With g = 9.81 m/s2:

- **Turn**: a coordinated turn at bank phi gives omega = g tan(phi) / v and n = 1 / cos(phi) (R3's
  bank = atan(v omega / g)).
- **Limits**: n <= `max_load_g` (lower with stores and with damage); omega <= g sqrt(n_max^2 - 1) / v; omega <= the
  type's instantaneous turn rate; |d phi / dt| <= `roll_rate_deg_s`. Roll-in and roll-out at turn points follow.
- **Speed**: dv/dt = A(throttle) (1 - (v / v_max)^2) - K (n^2 - 1) - g sin(gamma), where A is the type's level
  acceleration at full throttle (`accel_mps2`), v_max its top speed at low level (`max_mps`), gamma the path angle, and
  K set so that the type's **sustained** turn holds speed at its corner speed:
  K = A (1 - (v_c / v_max)^2) / (n_s^2 - 1), n_s = sqrt(1 + (v_c omega_s / g)^2).
- **Corner speed** comes from the type's limits: v_c = g sqrt(n_max^2 - 1) / omega_inst. Above it the turn is
  g-limited, below it lift-limited; harder than sustained bleeds speed.
- **Climb**: the height layer asks for a path angle within `max_climb_mps`, not a vertical speed added on top of the
  airspeed (target_behaviour.hpp:295-336 adds it today); a climb now costs speed through -g sin(gamma). `GroundFloor`
  stays absolute (R5).
- **Helicopters**: speed by nose-down pitch, a bank limit, hover with a yaw-rate limit; R3's lean.

The maths of a banked lift vector is already in `fx::WreckFall::Accel` (fx/trajectories.hpp:442-455) and is reused.
Unreal draws the nose, not the track: `MakeState` takes yaw from `heading_rad`, pitch, and roll from `bank_rad`
(SimWorldSubsystem.cpp:2040-2051 today takes the ground velocity and roll 0); `AcmiWriter` writes the roll.

### 3.2 Type data

`AircraftType` (data.hpp:318-385) keeps `in_service_year` and `max_turn_deg_s` (now the instantaneous rate at corner
speed) and gains `max_load_g`, `sustained_turn_deg_s`, `roll_rate_deg_s`, `max_mps`, `accel_mps2`, `climb_mps`,
`seats` (section 4) and, for stores, `loaded_load_g` and `loaded_drag` (the factor on K and on A with a load; jettison
restores the clean values). Each with `source` and `confidence`.

**Fuel** is a state per aircraft: `Aircraft` gains `fuel_kg`, burnt at a rate per type by phase and throttle
(`fuel_kg_per_s` at cruise and at full throttle, game values), with the drop tanks' fuel (`04_REACTIONS.md` §8) used
first. Jettisoning tanks leaves the internal fuel. Joker and bingo are read against it (`07_AIR_TO_AIR.md` §4.5); no
period figure for either was found (`air_combat.md` nato-16), so they are game values per type and profile.

**No source read gives these numbers.** `air_combat.md` gen-03 found the words (merge, one and two circle fights, E-M)
but no turn performance; Shaw's *Fighter Combat* (1985) was lending-only; the Soviet practical aerodynamics manuals for
the MiG-21 and MiG-23 are open items (`air_combat.md` §7 items 6, 12, 15). They are **game values**, held to the one
ordering the sources give: the MiG-23 turns worse than a hard-wing F-4 and out-accelerates and outruns the F-5
(`lambeth_1985`). Speeds on legs come from the routes: the F-111F at 540 kt preferred, never under 480 (`crandall_twz`,
a veteran's account), the Tornado's TFR at 420 kt in cloud (`napier_hushkit_2022`), the A-10 at 200-300 kt below 100 ft
(`agard_cp267_1979`, DeHart), the Pact's designation pair at 850-900 km/h (`markovsky_afghan_sky`).

### 3.3 Effects to watch

Turn rate now falls with speed, so fast aircraft manoeuvre less and the engagement model's measured turn rate
(engagement.hpp:1263, the `manoeuvre_penalty_per_deg_s` factor) moves the Osa's hit chances; `crew_balance`,
`air_war_bench` and `em_demo` are reported byte-identical or with every moved cell explained, as track R does. A
damaged aircraft (R1's moderate and heavy) loses load factor and acceleration by game values.

### 3.4 The Python mirror

`prototype/samproto/engagement.py`'s `Aircraft` (:18-60) gets the same step, wind included (it has none today), and the
same tests. Flights, pilots and the runner are not mirrored today and stay unmirrored.

### 3.5 As built (R3, 15 Nov 2026)

R3 is built in `simcore/aircraft.hpp` (tests `tests/test_flight.cpp`, mirrored in `prototype/tests/test_flight.py`).
Where it differs from 3.1-3.4:

- **Two ways of flying.** An aircraft given any of its type's limits (a roll rate, a g limit, or an energy model's
  acceleration and top speed) flies by bank-to-turn. One given none, as every hand-made scenario's target, flies the
  old way: constant airspeed, its velocity turned toward the waypoint at its turn rate in 3D. Its heading, path angle
  and bank are read off the path it flew (the bank for the turn rate, rolled into at 90 deg/s), for the drawing only.
  So the crew scenarios fly as before. The air war's types give their limits, so the testbed's waves and the raids
  generated from now on fly by bank-to-turn. The seven sample raids saved in `data/scenarios/air_war_*.json` were
  generated before R3 and fly the old way until they are generated again.
- **Speeds.** `speed_mps` stays the speed the aircraft is told to fly. The airspeed it has is the new `airspeed_mps`.
  The energy model is dv/dt = A (throttle - (v / v_max)^2) - K (n^2 - 1) - g sin(gamma): at full throttle it is
  3.1's formula. The pilot sets the throttle (-0.5, idle and air brakes, to 1) to hold the speed told within 4 s; when
  that asks for more than full throttle, the climb or the turn costs speed.
- **The speed he holds.** A pilot does not bleed his speed away. The speed he keeps is the speed he is told (at most
  90 % of his top speed). As a turn slows him below it he eases the turn, from the full rate at that speed down to the
  sustained turn at 85 % of it, K (n^2 - 1) = A (1 - (v / v_max)^2), and no harder below. There he also climbs only
  on the thrust the turn leaves. The eased limit changes with the speed, so the bank follows it at the roll rate; only
  the g limit's bank is a hard stop. Without this a MiG-25 hunting radars, its turn rate raised to the ARM
  turn-away's 12 deg/s, circled down to 48 m/s at 12 km. 85 % is a game value.
- **K.** The data's instantaneous rates (`max_turn_deg_s`) are low, so most jets' corner speeds lie above their top
  speeds. K is set at the corner speed or at 90 % of the top speed, whichever is lower.
- **The pilot's turn.** He asks for a turn rate of the heading error over 1 s (or over his roll time to the bank
  limit, if longer), within the limits, and no more bank than he can roll out of before the heading he wants:
  cos(phi) >= exp(-error v p / g), the heading the roll-out turns at roll rate p.
- **Climb.** With no height layer the step flies a path angle to the waypoint, pulled at the turn rate limit, its
  climb capped at `climb_mps`. Terrain following (`TargetPilot::HoldHeight`) still sets the height after the step, as
  before; the step then flies the horizontal at v cos(gamma), and the energy model takes -g sin(gamma) from the
  vertical speed flown. The type's `climb_mps` caps the height layer's climbs. So a terrain-following climb costs
  speed, but its rate is not yet asked of the aircraft as a path angle.
- **Pitch.** The path angle and an angle of attack of 3 deg at 1 g, times n and (the speed told / the airspeed)^2,
  at most 15 deg. A helicopter's pitch is its lean, -atan((v / v_max)^2 tan 10 deg + (dv/dt) / g).
- **Helicopters** have no energy model yet: they fly their told speed, as before. A pop-up helicopter stops at its
  hide on the spot; with an energy model it would slide several hundred metres past it until the pilot plans his stop. Their bank limit
  is acos(1 / max_load_g); in the hover (under 2 m/s) they turn on the spot at `max_turn_deg_s`, level.
- **Type data.** The new values are in each aircraft's `"flight"` object, with their own `source` and `confidence`
  (the type's own source is about its role and numbers, not its flight). All are game values, `estimate`. The
  ordering held: the MiG-23MLD sustains 6.5 deg/s against the F-4E's 7.5, and has the higher acceleration (7.5 against
  6 m/s2). No F-5 is in the data; one added must take less acceleration and top speed than the MiG-23MLD's (7.5,
  375 m/s). `max_turn_deg_s` is unchanged. A helicopter's `"flight"` has no `accel_mps2`. Fuel, stores, seats and
  jettison are not in R3 (B4).
- **Not reused.** `fx::WreckFall::Accel`'s banked lift was not needed: the coordinated turn is the two lines of 3.1.
- **Drawn.** Unreal takes yaw and pitch from the nose (`heading_rad`, `pitch_rad`) and the roll from `bank_rad`, for
  every aircraft but a weapon (`USimWorldSubsystem::BuildEntityList`). Tacview gets the same attitude.
- **Left.** Damage (R1's moderate and heavy) does not lower the g limit or the acceleration yet. Helicopters' energy.
  The terrain-following climb as a path angle. The flight model at altitude (v_max is one number per type, the top
  speed at the heights it flies).

## 4. Pilots as people

### 4.1 Today

Only officers are people: one squadron leader per squadron and staff officers with `flies_himself`
(officers.hpp:278-282). A package has at most one flying officer. `SquadronState` counts airframes (state.hpp:190-209).
Crew quality is the type's `evasion` plus the leader's airmanship (`PackageAirmanship`, resolve.hpp:59-62).

### 4.2 What the period gives

| Quantity | Value | Faction | Source | Confidence |
|---|---|---|---|---|
| TAC flying per pilot | FY1980 11.4 sorties and 16.3 h a month; FY1984-85 13.1 sorties (an estimate) and 18.3 h; about 196 and 220 h a year (x12) | NATO (USAF) | `gao_1986_flying_hours` | high |
| 1st TFW sorties, Jan-Jun 1985 | inexperienced 92, experienced 87, instructors 103 | NATO (USAF) | `gao_1986_flying_hours` | high |
| NATO guide | an average of 15 h a month; the RAF "more than the minimum" (no number) | NATO | `hansard_1981_nato_hours`, `hansard_1981_raf` | high |
| Soviet tactical pilots | minimum 100-110 h a year, probably 125-160 on average; one regiment about 12 h a month; the US minimum then 240-250 | Pact (US view, 1969, MiG-17 era) | `cia_1969_soviet_tac_training` | medium; for 1985 a SAM3D extrapolation |
| Soviet ground-attack training | preplanned, unopposed, targets reconnoitred 2-3 days ahead; qualification CEP 410 ft against the US 165 ft | Pact (US view, 1969) | `cia_1969_soviet_tac_training` | medium |
| Afghanistan, 1985 | 224 h per Su-25 against 84 per MiG-23 (per aircraft, not per pilot) | Pact | `markovsky_afghan_sky` | medium |
| Combat experience | under 25% of TAC aircrew had combat time in August 1978 | NATO (USAF) | `vangilder_1979` | high |
| The first ten missions | most combat losses in a pilot's first ten combat missions, then "nearly zero" (Suter's 1975 argument for Red Flag); the study behind it not found | NATO (US) | `afmag_2000_redflag` (2000, hindsight), `vangilder_1979` | **low** |
| Training over the aircraft | Cunningham's 150 ACM flights before his first MiG; the Navy's kills per engagement 1.04 after Top Gun | NATO (US) | `vangilder_1979` | high |
| Low-level workload | high-speed low flight "results in loss of lookout"; weather in 10 of 19 European low-level accidents; white-out, illusions, "mission completion syndrome", fatigue, stress, task overload | NATO | `agard_cp267_1979` (Lee B3; Rud and Leben B22) | high |
| Two seats | low-level penetration "at minimum a two-man job" (FB-111 crews) | NATO | `agard_cp267_1979` (DeHart B4) | medium |
| Tempo | a surge of six missions a day for three days (A-10); Soviet 2-3 sorties per aircraft a day realistic | NATO, Pact | `agard_cp267_1979`; `cia_1969_soviet_tac_training` | medium |
| Attention on alert | flags after 30 min of low-level airborne alert (US tests, Soviet reading) | NATO | `mt_1986_dobedin` (1986, just after the period) | medium |
| Crews per aircraft | Iraq "slightly over 1:1"; RAF Tornado about 1.3 (late 1980s recollection) | Pact client, NATO | `cia_1987_iraq_af`, `rafhs_j20_1999` | medium |
| Nerve | 47 US aircraft jettisoned under MiG attack in September 1967; Syrian pilots ejected when they knew they were seen (1982, one report); an Afghan Su-17 leader refused the FAC and bombed his own battalion (1984) | NATO, Pact client, Pact | `checo_1969_tactics`, `lambeth_1984`, `markovsky_afghan_sky` | high, low, high |

**Not found**: Pact frontal aviation hours in the 1980s from a period source (forum figures of 70-220 h are tier 5 and
not used), the hours of the Pact's allies, Syria, a small neutral and the rebels (`perception.md` perc-22), NATO and
Pact crew ratios for 1985, and the loss study behind "the first ten missions".

### 4.3 The pilot record

`air_war/pilots.hpp`:

```cpp
enum class PilotFate { Fit, Wounded, Killed, Captured, Missing };
struct Pilot {
    int id = -1;
    std::string name, rank, squadron;
    int seat = 0;                  // 0 pilot; 1 WSO or navigator of a two-seat crew
    int officer = -1;              // an Officer flying as this pilot (the squadron leader)
    double hours_per_year = 0;     // currency: the training he had before the war
    int combat_sorties = 0;        // experience
    double nerve = 0.5;            // 0..1, drawn once
    double stress = 0;             // 0..1, carried between sorties, decays with rest
    int sorties_today = 0; double last_rest_h = 0;   // fatigue
    PilotFate fate = PilotFate::Fit; double back_h = 0;  // wounded until
    std::vector<std::string> claims;                     // as he claimed them (the enemy's records)
};
```

**Skill** (game formula, coefficients in `data/gameplay/air_war.json` `pilots.*`, every key in `TuningTable::Defaults`):

- base = s_min + (1 - s_min) x min(1, hours_per_year / hours_ref);
- green = max(0, 1 - combat_sorties / 10) (ten from Suter's argument, low confidence, so a game value with that note);
- effective = base x (1 - g_green x green) x (1 - workload) x (1 - 0.5 x stress x (1 - nerve)), the last factor the
  same shape as `Officer::Effective` (officers.hpp:73).

**Workload** adds by game values for low level, speed at low level, poor visibility, night, a long low-level alert past
30 min (`mt_1986_dobedin`), and a single seat at low level (a two-seat crew takes half the low-level term: DeHart's remark
gives the direction, not the number). **Fatigue** adds past the faction's normal sorties a day (Pact 2-3, the A-10 surge's
six as the upper bound). **Stress** rises in the sortie on a threat seen or called, a launch at the flight, a wingman
lost, damage; it decays on the ground. Effective skill is what the other documents read: the reaction delay and the
chance of no reaction (`reactions.md` §1 point 3), lookout (perception), formation keeping (slot error and the chance of
losing contact), release accuracy (a CEP factor; the 1969 qualification CEPs give the Pact-to-US ratio), shots out of
parameters and countermeasure misuse (air combat).

**Faction, officer, pilot.** The faction's hot-phase parameters (the `hot_phase` block of `10_FACTIONS.md` §7:
`initiative`, `plan_adherence`, `press_on`, `jettison_readiness`, `radio_discipline`, `emission_discipline`,
`air_combat_skill`, `opportunity`, `turn_back`, `reaction_delay_s`, `husbanding`; after `factions_cases.md` §1) are
doctrine, SAM3D estimates bounded by the sources. Each flight's working value is

  P = clamp01(P_faction + d_officer + d_pilot), |d_officer| <= 0.2, |d_pilot| <= 0.15,

d_officer from the flying officer's traits (rigidity and methodical up on adherence and retask latency, creativity
down; boldness up on press-on and opportunity; composure down on turn-back; leadership holds the flight on a jammed
net), d_pilot from the leader's experience, currency and nerve (nerve on press-on, turn-back and jettison; experience on
reaction and air combat; currency on skill). The weights are `10_FACTIONS.md` §8's, computed by its `HotPhase`
function. A Pact leader with high boldness is still a Pact leader: he may take a free
hunt he was given, not invent a target change (`factions_cases.md`, Johnson found no example of one).

### 4.4 Where pilots live, and their fates

- **The roster**: `SquadronState` gains `pilots` (ids into the campaign's `std::vector<Pilot>`), generated at `Init`
  from a new faction `"pilot_pool"`: `crew_ratio`, `hours_mean`, `hours_sd`, `combat_share`, `nerve_mean`,
  `replacement_per_week`, `replacement_hours`, each with `source` and `confidence`. NATO's hours from
  `gao_1986_flying_hours` (USAF TAC; other NATO air forces at the NATO guide's 15 h a month, `hansard_1981_nato_hours`);
  the Pact's from `cia_1969_soviet_tac_training` marked low for 1985; the rest SAM3D estimates.
- **Names** from the faction's `officer_pool` name lists (faction_nato.json:187-188), ranks by seat and service. Our
  side learns a name only from a prisoner or an intercept (`docs/air_war/02_AIR_STAFF.md` §6).
- **Crewing**: `Planner::Commit` puts crews in each flight: the most experienced fit pilot leads, a greener one flies
  his wing; the squadron leader stays in the first eligible flight (planner.hpp:1342-1349). Sorties a squadron can fly
  become the lesser of serviceable airframes and fit crews.
- **Fates**: a loss draws ejected or not; over our ground an ejected pilot is captured, over his own rescued after a game
  delay; the bridge's flat 50% capture goes. A damaged aircraft home may bring a wounded pilot (game chance, days off).
  `LossRecord` and the outcome carry pilot ids and fates; a capture is a `FactKind::Prisoner` at `div_intel`
  (`command/air_war_feed.hpp`) and reaches the player only down the pipeline, with what the prisoner says (name, unit,
  type, perhaps his hours) as he says it.
- **Replacement**: new pilots arrive by `replacement_per_week`, green and at `replacement_hours`; an air force short of
  crews flies less, as Iraq's "slightly over 1:1" shows (`cia_1987_iraq_af`).
- **Our fighters' pilots** use the same record from our order of battle (`command/oob.hpp`), under our GCI.

**Determinism and balance.** The roster is drawn from `rng_.Fork` keyed by squadron (types.hpp:433-437), so the
campaign's main stream does not move. With `pilots.enabled` false the campaign is today's, so `air_war_balance` and
`air_war_bench` stay byte-identical until a balance pass turns it on. Fates and stress come back in the
`PackageOutcome` the `Complete` action saves; a played session is never replayed. Pilots' claims go into the enemy's
records as claimed (`BeliefsFrom`'s overclaim, campaign.hpp:987-988); the truth stays for the debrief.

**In the session** the raid builder writes each target's `"pilot"` (`id`, `name`, `skill`, `experience`, `nerve`,
`stress`, `fatigue`, `seats`); the runner keeps stress live and hands every pilot's sortie back through the bridge.

## 5. Levels of detail, budget and multiplayer

### 5.1 The levels

| Level | Where | Flown | Thinks |
|---|---|---|---|
| A2, full | inside the play circle and near any of our units or engaged | every aircraft by section 3, its own pilot override | perception, decisions, radio, air combat at 1-5 Hz |
| A1, coarse | in the session but far | the leader by section 3; wingmen placed on their slots from the leader each step | the leader at 1 Hz on simple cues: an order heard, his RWR, a member lost |
| A0, abstract | outside the session | the resolver, made time-stepped so a package has a place at each `RoutePoint::t_h` and can report and be retasked (`05_C2_AND_RETASKING.md` §4.6) | the abstract rules (`01_ROUTES.md`, `05_C2_AND_RETASKING.md`) |

**Near** means within reach of a sensor or eye of ours in the session (the battery's and the PU-12M's radars at their
instrumented range, the visual range by the sun and the weather as `StepNav` uses it, our fighters' radar when they are
in the session) plus a margin, or engaged: a missile in flight at it, its flight in a fight, a launch or a burst it saw in
the last minute. Whole flights change level together, at their most detailed member's level. Levels follow simulation
facts, never Unreal's camera, so headless tools, the game and a server agree (`nav/movers.hpp` keys its M0-M2 the same
way). **Hysteresis**: down a level only past 1.2 x the near range and after 30 s at it (game values), as the movers'
margin does (nav/movers.hpp:160-171). **State kept across a hand-over**: route leg and the distance along it, slot,
order, stores, fuel, damage, the pilot's stress and what he knows; EM emitters radiate at A1 exactly as at A2, since what
the enemy emits is what our ESM hears.

### 5.2 The budget

At most about **0.5 ms added per 0.05 s step for about 100 aircraft** (the testbed's ceiling, `map/scale.md` §1.3, §2):
about 5 us per aircraft for work done every step. Every step: the flight model, slot keeping, the ground floor read from a
cached profile. Staggered: perception 2-5 Hz, decisions 1-2 Hz, the terrain look-ahead 2 Hz (it is 17 ground and
obstacle samples per aircraft per step today, target_behaviour.hpp:295-334), airborne radar looks 1-5 Hz in the EM
layer's own rates. **The stagger key** is a fixed serial given at `EnterTarget`, not the index or the id string: a task at
rate f runs when floor(t f + phase) steps, phase from the serial's hash, so it fires at the same simulation times at
0.05 s in the game and 0.1 s headless. Two traps go first: `World::FindAircraft`'s linear scan inside loops over targets
(`IndexOfAircraft`, world.hpp:147-150) gets an index, and the target lists that never shrink (scenario_runner.hpp:1940-1954) get a live
list. A new `air_battle_bench` (`simcore/examples/`) flies 100 aircraft at A2 and at A1 and reports the mean and worst
step; `verify.ps1` runs it as a trend, not a timed ctest. Its shares inside the budget: perception 0.15 ms
(`03_PERCEPTION.md` §5.7); reactions about 0.05 ms (`04_REACTIONS.md` §12.2); the rest for the flight model, slot
keeping, air combat and the radio (SAM3D budgets, to be measured).

**Randomness** draws from a generator of its own (`seed + kFlightSeed`) or keyed draws f(seed, serial, event) as the EM
layer does, never from the crew's or the engagement's stream and never from the fx feed (`test_fx_events.cpp:331-345`).

### 5.3 Multiplayer and the battery link

The server simulates; clients mirror and never step (`docs/MULTIPLAYER.md`). Aircraft are not replicated yet: "entity
positions need a stream of their own" (:104-105). Attitude, stores falling and the formation on a client wait for that
stream, which carries position, heading, pitch, bank and a few presentation flags, never a pilot's state or knowledge
(em::BuildView's views have no truth fields, `test_em_replication.cpp`). In a battery link the aircraft fly in the Osa's
runner and `MirrorTargets` (scenario_runner.hpp:10893) copies positions to the post's; it copies the attitude too.

### 5.4 As built (B12, 18 Nov 2026)

**On.** `data/gameplay/air_levels.json` ships with `mode` 1 since B12's second pass (below); `mode` 0 puts every
aircraft at A2, as before B12. The savings that change nothing (below) are always on.

**The levels.** `simcore/air_levels.hpp` holds them and `data/gameplay/air_levels.json` their values (game values but
the Osa's 45 km search radar range, `voenizdat_osa_1991`). The runner looks at each flight when it comes into the sky and
then once a second, at a moment of its own: `StaggerDue` fires when floor(t f + phase) moves on, the phase drawn from a
hash of the flight's id (`em::Mix64`, the same on every machine and at every step length; no random stream is used). A
flight is **near** when any member is within the reach of one of our units plus the 10 km margin: the session's own
vehicle, the battery's vehicles and the battery fire model's launchers (45 km each), the neighbours (their own reach, at
least 45 km), our fighters in the session (30 km), and our ground objects a pilot may find, the session's movers on the
roads and the dummies (40 km, the pilots' ground search cull in `perception.json`, so B6's opportunities are found as
before). A flight is **engaged** when it is in its attack, a member flies an override (a break, an air fight's steer) or
evades or is held by the instructor, a missile of ours is at a member (the engagement model's track, a launcher of the
battery fire model, a neighbour's shot in the testbed), or a member's picture holds a launch, a trail, a burst, tracer, or
a lock or launch on his receiver from the last 60 s: what the pilots know, never the truth. Near or engaged, the flight
is at A2 at once; beyond 1.2 times the near range for 30 s, it goes down to A1; a flight that comes into the sky far
away starts at A1. Our own flights are always at A2.

**A1 as built.** Every aircraft of a flight at A1 is flown, looks and thinks as at A2: the flight model, the height
layer, perception at its own rates, reactions, the radio. The one thing A1 changes is the wingmen's warning receivers.
Each wingman who keeps his slot (`FlightSlot::keeping`, alive and in the sky) has his own receiver switched off
(`EmSensorLink::SetRwrOn`; switched off it holds nothing, so back on it starts from what it hears) and hears on his
leader's: the searches and the missiles' guidance the leader's receiver hears go into his picture, marked `borrowed`
(`RwrHeard::borrowed`, `Sighting::borrowed`, keyed apart from his own receiver's contacts). A lock is not lent: a
tracking radar holding the leader is not holding him. A missile's guidance heard on the leader's receiver is also his
warning (`CueFromWarnings`). The leader's receiver must be the same type as the wingman's, and on; if the leader is
dead or gone, or a wingman leaves his slot (lost from sight, detached, in the attack, breaking), the wingman's own
receiver is on at once. When the flight comes back to A2 (or the wingman stops being lent), his own receiver comes on
and for 1 s he still hears the leader's searches and guidance while his own takes hold (a contact counts as heard for
1 s); his own locks and launches count from the first moment. A sighting that moves between the leader's receiver and
his own (the same kind of emitter on the same bearing, within three bearing errors or 10 degrees) keeps its place in
his picture (`PerceptionHub`'s `Adopt`): it is not news, so it raises no stress and cues no reaction a second time.

**The cause of the first pass's divergence.** The first pass (B12, branch `tb-b12`) also stopped flying the wingmen at
A1 and shut their eyes. Traced step by step against the levels off (`air_battle_bench --trace`), its first difference
in the testbed's hour came from two things at the hand-over back to A2. A placed wingman was up to 191 m from where he
would have flown (4 m at the hand-over, gone by 415 s), and waking his eyes set his next perception tick to the moment of
the hand-over (`PerceptionHub::Wake`). The second moved every keyed draw of his perception afterwards, since the draws
are keyed on the tick's index: the first wingman to part (62320-2) saw a missile's trail at 536.95 s on a different tick,
with his receiver's display and picture otherwise the same, and the hour went its own way from there. Nothing in B3's
or B4's streams shifted (both are keyed hashes, not sequential streams); the state they were keyed on did. The second
pass flies the wingmen and keeps their eyes open, so their ticks and draws are the ones they would have had at A2.

**What the levels change.** Near our units nothing is coarse: every flight there is at A2, flown and looking as
with the levels off. A flight that was far comes back with every aircraft in the place it would have had, its stores, fuel,
damage, orders and route untouched, and with the pictures it would have built, but for one thing: a wingman heard on
his leader's receiver, 1-2 km away, which hears an emitter at slightly different moments than his own would have (the
ground masks the two differently, and each sits at its own margin of sensitivity). In the testbed's hour, seed 12, two
wingmen of Viper19, at A1, would have heard an SA-8's search on their own receivers at 170.3 s and 173.7 s, before
their leader's receiver heard it, and a third heard it on the leader's at 175.95 s, when his own would not yet have. A
search paint adds 0.05 to a pilot's stress, and that moves the delay of his next reaction by a few hundredths of a
second. From such a difference the hour goes its own way, as it does from any small change. So the debriefs are often
not identical, and the test is whether the levels move the outcomes the way a new seed does or push them one way. The
testbed's hour over 24 seeds, with the levels off and on (`air_battle_bench data --compare 3600 --seeds N --first K`):

| The testbed's hour, 24 seeds | Levels off, mean (spread) | Levels on, mean (spread) | On less off, mean (its standard error) |
|---|---|---|---|
| Our launches | 6.00 (3.01) | 5.71 (2.73) | -0.29 (0.49) |
| The neighbours' launches | 206.5 (16.7) | 206.5 (18.8) | 0.0 (2.3) |
| Aircraft down | 38.0 (6.0) | 38.1 (6.3) | +0.08 (1.09) |

Seven seeds of the 24 give identical debriefs; in the others the first different line is between line 426 and line
41,199. No outcome moves one way: each mean difference is well inside its standard error, and the spreads are alike.
Seed by seed the runs with the levels off and on stay closer than two different seeds do (the spread of the paired
difference is 0.5 to 0.6 of what two independent seeds give), but one hour can differ a lot (seed 12: our launches 14
against 6; seed 13: 6 against 11). That is the kind of change a new seed makes, so the levels are on.
`helper_coverage` (the testbed's rows, 900 s, included) and `crew_balance data 10` are identical with the levels on
and off, line for line, and `ground_check data 3600` lists nothing with them on. The sessions in `test_levels.cpp`
whose flights go far and come back near give the same places at every step, the same launches and the same debrief
but for a wingman's warning-receiver line, which may come at another moment (see Tests).

**A0** is not built here: the time-stepped resolver is B5's (`05_C2_AND_RETASKING.md` §4.6), and making the resolver
place packages each step changes every campaign's outcome; it stays with B5.

**Savings that change nothing.** The step was made cheaper where the result is exact. The engagement model's geometry
tested the line of sight (ground samples every 100 m) and the clouds for every aircraft at every step; they are now
worked out only when a reader asks, and the readers ask after the range and the sector, so an aircraft beyond the
radar's reach costs a distance (`engagement.hpp`, `Geometry::Los()`; with 100 aircraft this was 7 to 10 ms of a step
before). `World::IndexOfAircraft` searches the sorted list instead of scanning it; the EM link keeps a map from a target
to its receiver and its platform; the runner a map from a target's id to its index. The route planner's lattice (B2)
keeps each edge's heading and its statistics with the edge, so a search no longer works out a bearing and finds a
cached leg at every relaxation.

**What it costs.** Timings are wall time on one core of a machine shared with other builds, so means move by a fifth
from run to run; the first pass and the second were run alternately, back to back.

`air_battle_bench data`: 25 flights of four of the testbed's waves' types (placeholders) entered at once into the
testbed (its battery, its neighbours and their radars, no waves of its own), measured for 120 s after 45 s, aimed at the
battery (near) or at a point 220 km away (far). Two rounds each, ms a step:

| Case | Aircraft | At A1 | Step (mean) | Step (worst) | Air battle | Pilots' height | Perception | Engagement and EM | of it receivers |
|---|---|---|---|---|---|---|---|---|---|
| near, A2 | 104 | 0 % | 3.72-3.73 | 16.7-16.8 | 0.72 | 0.33 | 0.27 | 2.71-2.72 | 1.77 |
| far, A2 | 104 | 0 % | 2.24-2.29 | 12.9-13.0 | 0.53-0.54 | 0.30-0.31 | 0.10 | 1.45-1.49 | 1.29-1.33 |
| far, A1, first pass (wingmen placed) | 104 | 53 % | 1.33 | 8.6-10.4 | 0.39 | 0.22 | 0.09 | 0.69 | 0.53 |
| far, A1, second pass (as built) | 104 | 53 % | 1.47-1.53 | 9.1-9.7 | 0.52-0.54 | 0.29-0.30 | 0.10 | 0.69-0.72 | 0.54-0.57 |

The second pass keeps most of the saving: far away, 2.3 ms a step at A2 and 1.5 ms at A1, against the first pass's
1.3 ms. What it gives up is the wingmen's flight model, height layer and perception at A1, about 0.15 ms with 100 aircraft.
In the testbed's hour (`air_battle_bench data --testbed 3600`) the flights are near nearly all the time (0 to 1 % of the
aircraft-steps at A1), so the levels change little there: a mean step of 1.89-1.93 ms with them off and 1.90-1.92 ms
on, two rounds of each pass (the first pass's runs with them on, at 1.71-1.74 ms, played a different hour).

The air battle's own parts (the levels, the flights and the flight model, the pilots' height layer, the radio,
perception, reactions, the air fight) are 0.52 to 0.72 ms with 100 aircraft, the same at A1 as at A2 now that every
aircraft flies and looks, a little over the budget of 0.5 ms; the pilots' height layer (the terrain look-ahead) is half
of them. The levels themselves cost 0.004 to 0.009 ms. What dominates the step is the EM layer's warning
receivers: every aircraft's receiver hears every radar every 0.2 s, each a path profile over the ground, and at A1
three of each four are off. Near the battery, where everything is A2 whatever the levels, the step is the receivers'
and the engagement model's, and that is B4's ESM period below. The pilots' height layer at a lower rate was not taken:
an exact skip needs a ceiling on the ground ahead, and the map's analytic ground beyond its lattices has none, so any
lower rate changes where the pilots fly inside the play circle.

**B4's savings that change outcomes** (`04_REACTIONS.md` §15: the defensive perception rate capped at the threat rate,
about 7 s in the testbed's hour; the ESM period at 0.4 s instead of 0.2 s, about 25 s) are not taken: both act inside the
play circle and change what a pilot knows and when. They stay as data (`perception.json`'s `defensive` rate, the EM
library's ESM period) for a balance decision.

**B2's district planner.** Profiled over three seeds: 60 s of the 86 s run is `RoutePlanner::Plan`, of which 20 s builds
the lattice's edges, 29 s searches (87 million relaxations, 82 million of them a cached leg) and 2 s works out new legs.
The cause: the district has no map, so the 15 km lattice is never thinned by the ground and fills the 120-feature cap,
and every plan searches about 117 nodes with every heading. Routes are not reusable across cycles (the believed picture
moves: 3,936 of 71,528 plans repeat one). The edge memo cuts the searches by about 38 % with the same output, line for
line; building the edges is left as it is. A coarser lattice on a theatre with no map would cut the rest but changes the
routes, so it stays on 01's Left list.

**Through the other tasks.** B3's perception runs for every pilot at every level; at A1 a wingman's receiver sightings
come from his leader's receiver, marked borrowed, and `Adopt` keeps a sighting in place when it moves between the two.
B4's reactions are keyed on the pilot and his count of cues (`ReactionU`), so they shift only if what he heard shifts.
B6's ground opportunities are found as at A2, and the near range covers our ground objects a pilot may find (40 km, the
ground law's cull). B9's radio runs for every member at A1 as at A2. B8's live air fight counts as engaged, so it is
always at A2: there is no "A1 far fight", and `air_combat_bench` is not written (B8's fights are measured as
`air_battle_bench`'s "air fight" part). A0 is B5's.

**Tests** (`tests/test_levels.cpp`): the stagger fires at the same simulation times at 0.05 s and 0.1 s and spreads the
flights; the hysteresis (up at once, down past 1.2 times after 30 s, a route along the edge never changes level, a flight
first seen far starts at A1, mode 0 keeps everything at A2); the data loads, and a data folder without the file keeps
the levels off (old saves and old data folders load as before); a flight out to 160 km and back, the levels off and on
side by side: it goes to A1 and back, its wingmen's receivers are off at A1 while the leader's is on, the Osa's search
heard on the leader's receiver reaches the wingmen's pictures as borrowed, none is borrowed 2 s after the return, every
aircraft is in the same place at every step and the debrief is the same; four cases of flights that go far and come
back near the battery (routes north and south, east of the unit by -30 to 20 km, four seeds), each compared with the
levels off when a flight went to A1, came back within 55 km and its wingmen heard on the leader's receiver: the same
places at every step, the same launches, and the same debrief within the stated tolerance (the warning receivers' lines
the same by who and what; only a wingman's may come at another moment; every other line identical). Three of the four
are identical; in the fourth, while his flight was far south of the unit, a wingman's own receiver would have heard an
SA-8's track 8 s before his leader's did, and with the levels on he hears it 14 s later, once the leader's lock has
brought the flight back to A2. Three flights near the battery give
the same debrief and launches with the levels on and off; a session with the levels on run twice gives the same
debrief; 100 aircraft far away at A1 have 25 receivers on against 100 at A2 and the same perception ticks.

**The balance (the first pass, levels off, against the second, levels on).** `crew_balance data 10`,
`air_war_balance data 20 7`, `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` and `air_war_bench data
400` are identical, line for line (the air war's runs play no session with far flights). `em_bench data 500 60 24`
gives the same checksum (it drives the EM world directly). `ground_check data 3600` lists nothing, 0 of 15, with the
levels off and on. `helper_coverage` is identical with the levels off and on, the testbed's rows included, and the same
as the first pass's with them off. With the levels off the second pass's `ground_check`, `helper_coverage` and
`crew_balance` are the first pass's, line for line. `python -m pytest prototype tools -q`: 186 passed, the one failure
`test_voice_lines` as before; `check_citations`: 0 problems.

**Shortcuts.** A wingman at A1 hears what his leader's receiver hears, not what his own would have: an emitter at the
edge of hearing is heard earlier or later, and a lock on him that is not on his leader is missed until the flight comes
back to A2. The stagger key is a hash of the flight's id, not a serial given at `EnterTarget`: the same on every machine,
but it changes if a flight is renamed. The post's radar does not make a flight near by default (`post_radar_m` 0): the
PU-12M's P-19 reaches 160 km and would keep a whole battery mission at A2; set it to count it.

## Done when

1. `test_flight_model.cpp`: at 200 m/s and 60 deg of bank the turn is 4.87 deg/s, 2.0 g and a 2,354 m radius (hand
   calculation); a sustained turn at corner speed holds speed within 1%; an instantaneous turn bleeds it; a climb trades
   speed for height; roll-in takes bank / roll rate; runs at 0.05 s and 0.1 s agree within a stated tolerance; the
   placeholder MiG-23 class turns worse than the F-4 class and out-accelerates the F-5 class.
2. The Python mirror passes the same cases (`python -m pytest prototype -q`).
3. `test_flights.cpp`: a four-ship keeps its slots through a 90 deg turn within a tolerance (today's shifted routes do
   not); a break ends and the wingman rejoins; the leader shot down hands the flight to No. 3; a flight that misses an
   order on a jammed net keeps the last one; `OrderFlight` from a node the faction's table forbids is refused.
4. A raid built from a NATO and a Pact package has slots from the faction files and per-leg speeds, and no
   `flight_spacing_m`; `test_packages_live.cpp:159` still passes; every hand-written scenario loads unchanged.
5. A played raid's outcome lists jettisons, diversions with their `by`, aborts by order, and each pilot's fate; a save
   with them loads to the same campaign (`test_air_war.cpp:943`).
6. `test_pilots.cpp`: rosters are the same for a seed and differ for another; with `pilots.enabled` false
   `air_war_balance` and `air_war_bench` are byte-identical to the base; every working parameter stays within +-0.2 of the
   faction for the officer and +-0.15 more for the pilot; a captured pilot reaches the player only as a delivered
   `Prisoner` fact.
7. A route that straddles the near range does not flicker between A1 and A2; A2 to A1 and back keeps route place, stores,
   damage and stress; a session run twice with one seed writes the same log.
8. `air_battle_bench` shows at most about 0.5 ms added per 0.05 s step with 100 aircraft; `ground_check data 3600` lists
   nothing; `crew_balance`, `air_war_bench` and `em_demo` are byte-identical or every moved cell is explained.
9. The testbed's waves bank, pull and roll visibly in Unreal (R3's own check; the user runs the editor).

## Left open

- Every type's sustained and instantaneous turn rates, load limits, roll rates and acceleration: no source read
  (Soviet practical aerodynamics manuals, Shaw 1985, Spick as pointers only); game values until found.
- Pair and element spacing for Pact fast jets, NATO helicopter teams, the neutral and the rebels; RAF, Luftwaffe and
  Dutch formations (`air_combat.md` nato-17).
- How flights rejoined after an attack, and whether a wingman escorted a damaged leader home, in either bloc.
- Pact frontal aviation's annual hours in 1980-85 from a period source; the Pact's allies, Syria, a neutral, rebels.
- The loss study behind "the first ten missions"; Red Baron's awareness shares (`perception.md` §4 item 5).
- Crews per aircraft for NATO and the Pact in 1985; joker and bingo fuel as numbers (`air_combat.md` nato-16).
- Two-seat crew coordination beyond DeHart's remark; how fatigue and stress curves should be shaped (game values).
- The A0 resolver made time-stepped (`05_C2_AND_RETASKING.md` §4.6, with `01_ROUTES.md`'s routes): its cost on the
  nation's theatre is not yet measured.
- B12's A1 (§5.4): a wingman hears on his leader's receiver, so an emitter at the edge of hearing comes to him a
  little earlier or later, and a lock on him alone is missed until his flight is back at A2.
