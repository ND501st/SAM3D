# 13 — Out of the vehicle: hatches, the hull, the ground, and back (roadmap 8.14)

The plan and the design for the crews getting out of their vehicles and back in, standing on the hull, walking the map
and doing jobs outside. The research is `docs/research/MOUNTING_2026-10.md` (20 Oct 2026). It rests on the crew pillars
(`README.md`): nothing is done by "the vehicle", every action has an owner seat, and knowledge moves only through
instruments and voice, which now includes being out of earshot and off the headset.

**The rule:** a crewman's body is always somewhere, and he gets from one place to the next only by the ways the vehicle
has: up from his seat, along the compartment to a hatch, the lid opened, up and out onto the roof, across it to where a
wheel takes him down, and over the ground; back the same way. Every leg takes its time. Nothing teleports, neither the
NPCs nor the player.

## 1. What the sources settle

| Fact | Source | Tier |
|---|---|---|
| The Osa: an entry hatch in the roof; two windscreens, one opened by hand as the crew's emergency exit; an emergency hatch on the engine compartment's roof; a door in the bulkhead | `voenizdat_osa_1991` pp. 173-174 | 1 (owner's exception) |
| The Osa's hatches must not be opened in БР; leave the vehicle only once the readiness to launch is cancelled; not within 10 m of the turbine's exhaust | `voenizdat_osa_1991` p. 207 | 1 |
| The aiming circle 20-50 m ahead of the Osa, 30-40 m ahead of the PU-12M, sighted mutually on the vehicle | `voenizdat_osa_1991` pp. 249-250 | 1 |
| The BTR-60PB's troops get in and out by the roof hatches; two side hatches high near the turret | `fair_btr_compare`, `vk_btr60` | 4 |
| К МАШИНАМ: get out quickly and line up beside the vehicles; ПО МЕСТАМ: get in quickly by the wheels and steps; BTR crews by their hatches, shut once in; a crew in one rank in front of its vehicle, the commander two steps ahead of the right track | `stroevoy_ustav_1993` arts. 145-154 | 1 |
| The pace: 110-120 steps a minute of 70-80 cm; running 165-180 of 85-90 cm | `stroevoy_ustav_1993` art. 31 | 1 |
| The slope: Tobler's hiking function | `tobler_1993` | textbook |
| Vocal effort at 1 m: normal 60, raised 66, loud 72, very loud 78 dB(A) | `iso_9921` | textbook |

Not found and estimated (`data/vehicles/*.json` marks each): the climbing times, the axles' places on the Osa, which
hatch each seat uses by drill, the noise of the machinery, the hull's and an open hatch's loss of a voice.

## 2. The model (simcore)

### 2.1 The vehicle as a body to climb: `data/vehicles/<system>.json`

Loaded with the scenario when the system has one (`ScenarioDef::vehicle`, `simcore/crew_mobility.hpp` `VehicleDef`); the
body frame is the cabin's (x right, y forward, z up, metres, origin on the ground under the reference point).

| Block | What |
|---|---|
| `hull` | The roof's height, the rectangles a man can stand on, the footprint men on the ground walk round |
| `keep_out` | Where he must not stand up top: the Osa's antenna-launcher unit (it turns ±342°, and its radar beam), the PU-12M's mast and roof box |
| `seats` | Where each seat's crewman stands up (the cabin's walk floor) and the points he passes to his hatch (`via_m`: the PU-12M's bench crew squeeze forward past the R-111) |
| `exits` | Each hatch or window: the opening, where he stands under it, where he stands once out (or the ground, `to_ground`), the lid's and the climb's times, the seats that use it, `shut_when` (the conditions under which the drill keeps it shut, with the reason) and `emergency` |
| `ways_down` | Between the hull and the ground: a wheel and a handhold, the top and the bottom, the times down and up |
| `places` | Places round the vehicle: where he stands (`at_m`) and what he faces (`face_m`, the instrument or the set) |
| `jobs` | Console controls worked at a place (and `stay_until`: what holds him there while the work goes on): `nav.pab2` at the PAB-2 35 m ahead, `pwr.ab1_set` and `pwr.ab1_start` at the AB-1 behind, the Osa's `nav.aiming_circle` 35 m ahead |
| `noise` | Machinery that drowns voices while its subsystem runs (the Osa's turbine and diesel, the PU-12M's engines and its AB-1) |
| `lineup` | Where each seat stands for К машинам |

`data/crew/walking.json` holds the man: the pace (walking, running, inside, on the hull, through a wood, the steepest
slope) and how far his voice carries (talking, shouting, the background, the needed ratio, the hull's and a hatch's loss).

### 2.2 A crewman's body over time: `CrewMover`

His body is in one of five places (`BodyPlace`): **seated**, **on his feet inside**, **in a hatch**, **on the hull**, **on the
ground**. A goal (`MoveGoal`: his seat, a place, the line-up, a point on the map, a point on the hull or inside) becomes a
list of legs from where he is:

| Leg | Takes | Moves him |
|---|---|---|
| StandUp | the crew file's `stand_s` | seated → inside |
| CabinWalk | distance at 0.9 m/s | along the compartment, by the seat's `via_m` |
| OpenHatch | the exit's `open_s` (none if it stands open) | — |
| ClimbOut | `climb_out_s` | inside → (in the hatch) → hull, or the ground by a window |
| HullWalk | distance at 0.8 m/s, round a keep-out | across the roof |
| ClimbDown | the way's `down_s` | hull → ground |
| GroundWalk | over the terrain at the pace, the slope and the woods; round the hull | on the map |
| ClimbUp, ClimbIn | `up_s`, `climb_in_s` | ground → hull → inside |
| ShutHatch | `open_s` | the last man in shuts it behind him |
| SitDown | `sit_s` | inside → seated |

Positions on the vehicle are kept in its body frame, so a man on the roof moves with it; on the ground on the map (ENU,
`VehiclePose` converts). A tier's slower hands climb slower: the mover's pace is the tier's reaction time against a
trained man's 1 s (0.7-1.8). He is "inside" (on the intercom, at his instruments) until he is half out of a hatch.

**Exits by the drill.** A crewman leaves by his seat's exit (`stations`). If every exit he may use is shut by the drill
(the Osa in БР with the equipment on), he can't go out, and the runner logs why (p. 207) and tries again every 2 s. The
emergency exits (the Osa's opening windscreen) are used only for an emergency goal.

### 2.3 Jobs outside: a step needs him where it is done

`CrewMember` (procedures.hpp) knows each control's place (`SetOutsideJobs`). When a step's conditions hold and the step
is on a control at a place he isn't at, he goes there first; his reaction time counts from arriving. A step inside needs
him at his seat. He goes out only once the step's conditions hold (not on the march before the halt). **After a job he
stays at its place while its work goes on** (the job's `stay_until`: the squad leader at the aiming circle until the
measurement is done, the driver at the AB-1 until it is lifted down, earthed and cabled); then, if his next step is
inside and still waiting (the GAK running up, the set running up), he walks back and waits at his seat. With nothing to
do he goes back to his seat, unless told to stay (the line-up). The runner (`ScenarioRunner::UpdateMovers`) walks every NPC's body to where his
drill wants it and tells each crew member where his body is.

The first jobs:

| Vehicle | Seat | Job | Place |
|---|---|---|---|
| PU-12M | driver | `pwr.ab1_set`, `pwr.ab1_start` (deploy_power), and back | the AB-1 set, 9 m behind (`csla_pu12_manual`) |
| PU-12M | squad leader | `nav.pab2`, set up and stowed (deploy_orient) | 35 m ahead (p. 249) |
| Osa | commander (crew chief) | `nav.aiming_circle`: `measure_qc` (situational) | 35 m ahead (p. 250) |

The switches stay where they were in the cabins (the cases): for the player they are where the job is started from
(§3). The deputy's `orient_vehicle` still takes the crew chief's Qc as given.

### 2.4 The voice outside

A man out of the vehicle is off his box (the headset's lead doesn't reach): he hears no intercom, no radio, no call.
What he says goes aloud. `ScenarioRunner::InEarshot(from, to)`: both inside, always (the compartment, as before);
otherwise the speech level at the listener (shouted, 78 dB at 1 m, less 6 dB a doubling of distance, less 25 dB through
the shut hull or 8 dB through an open hatch near the man outside) against the noise there (45 dB in a quiet field, the
machinery running near adding its own) must clear 3 dB. So a shout carries about 30 m in a quiet field, and nobody in
the line-up hears the shut compartment. `CalloutHearers`, `Hear` (what an NPC takes in) and the order path all use it:
an order to a man out of earshot waits until he can hear it.

### 2.5 К машинам and По местам

`ScenarioRunner::OrderDismount` ("К машине!") sends the NPC crew out to their places in the line-up (one rank two steps
ahead of the vehicle, the commander on the right); `OrderMount` ("По местам!") brings them back to their seats, the last
man in shutting his hatch. The line-up hears the commander standing with it; an order from inside the shut vehicle does
not reach it.

Both are orders of the crew data (`order_dismount`, `order_mount` in `data/crew/osa_ak_crew.json` and `pu12m_crew.json`,
actions `dismount` and `mount`), so the order menu offers them: said once on the intercom, then the same work
(`LineUpCrew`, `MountCrew`). The menu greys К машине on a vehicle without a line-up or while the crew is lined up or on
its way there, and По местам while every NPC is in his seat ("уже выполнено").

## 3. The player (Unreal)

The player's body goes through the same legs; nothing is a teleport.

- **Inside**, X gets him up and walks him (as before). Under a hatch, looking up at it, **F** opens it and climbs out: the
  camera rises through the hatch along the ClimbOut leg (simcore's `PlayerGoTo`, the hull point beside it) and he
  stands on the roof.
- **On the hull**, W A S D walk him on the roof's walkable part (not past its edges, not onto the antenna-launcher unit
  or the mast). At a way down, **F** climbs him down (the ClimbDown leg).
- **On the ground**, W A S D walk him over the terrain at the drill pace, Shift runs; the ground's height under him from
  the world. Near a way down, **F** climbs him up onto the hull; on the hull beside a hatch, **F** climbs him in.
- **A job's switch clicked inside** (the AB-1's, the PAB-2's case) walks him out to its place along the legs, in first
  person, and works it when he is there. Once out, he can also work it there.
- Back inside, X sits him down at the seat he stands by.
- The engine reports his body to simcore every frame (`SetPlayerBody`); from then on a job outside needs him at its
  place (`PlayerJobRefusal`), the intercom doesn't reach him outside, and the NPCs hear him only in earshot.
- The NPC crewmen out of their seats are drawn walking, climbing and working (`USimCrewBodiesComponent`, from
  `ScenarioRunner::CrewBodies` and `simcore::WalkingPose`).
- **Multiplayer**: the server simulates; the client's body goes to the server with each move (the pawn's server call)
  and the bodies mirror.

## 4. Hooks for the later tasks (not built)

An outside task is data: a **place** (where, what he faces) and a **job** (a console control worked there, with its time
as a subsystem's `start_time_s`, and the drill's procedure). The later tasks fit the same form:

| Task | Place(s) | Job | Needs |
|---|---|---|---|
| Laying a field telephone line (P-274) to a launcher or the regiment (`ru_pu12_text`: four lines at 500 m in 25-30 min) | a reel's walk from the vehicle to the far end: a moving place along a line | `line<n>.laid` on the RTK-1's lines (`radio_nets.hpp` LineDef, today taken as laid) | a place that moves with the man (the reel unrolling), the line's length as distance walked |
| Servicing the AB-1, changing a wheel, the engine | the set, a wheel, the engine deck | maintenance switches | the fault model's repairs |
| A dugout, a small command post, camouflage | a spot near the vehicle | a dig job with hours of time | shifts of men; tools |
| Rest, meals, guard | the vehicle's side, a tent | the battery's routine (halts every 3-4 h, `voenizdat_osa_1991` p. 236) | the time of day |
| Signals on the march by flags and lamps (pp. 232-239) | the commander in his hatch | a signal control | the hatch as a place to stand |

**Planned** (the battery's war, `docs/campaign/07_BETWEEN_FIGHTS.md`, roadmap phase 20): the battery's activities between
fights (maintenance, rest, meals, digging, wire, guard, briefings) run as jobs at places in this form, and the time to
readiness is computed from where every person is and the legs to their seat.

## 5. Status and what is next

| Step | Status |
|---|---|
| Research (`docs/research/MOUNTING_2026-10.md`) | done |
| `simcore/crew_mobility.hpp`: the data, the pace, the voice, the mover; `CrewMember` jobs; the runner's movers, earshot, К машинам / По местам | done, `simcore/tests/test_crew_mobility.cpp` |
| The vehicles' files (Osa, PU-12M) and the first jobs | done (times estimated) |
| Unreal: the player through the hatch, on the hull, down, over the ground, back; NPCs drawn walking; the job switches walk him out | done, to be tested in the game |
| The order menu's К машине / По местам | done |
| The drill master and the lessons knowing the walk's time; the dismount on the safe side of a road | next |
| Wire laying as the first new outside task | next |
