# 02. Interdependence: who needs whom

Pillar 7: for one crewmate to act, others must have acted or told him something. This file is the map. Its control-level
part is generated from the data (`generated/osa_ak_action_map.md`: every control, the subsystems it feeds, the
procedures that move it, the inputs the simulation sets); this file adds what the data can't say: knowledge hand-offs,
the world, decisions and alternatives.

## 1. Two chains

Everything the crew does serves one of two chains. Both run at once and meet at "the target is within reach of a
ready vehicle".

```
MOBILITY CHAIN                                   KILL CHAIN
commander: choose position / route              (sources) detect
driver: drive, halt                    ─┐        commander/senior op: designate
driver: power (GTA, generator)          │        senior op + operator + deputy: acquire / capture
operator: equipment, drives, АПУ        ├──▶     senior op + commander: identify (IFF)
deputy: orient                          │        commander: decide (permission)
crew: "к бою готов"                    ─┘        deputy: prepare, launch
                                                 operator/senior op/deputy: guide (hold the track)
                                                 deputy (TV), operator (range): assess; re-engage
```

## 2. The kill chain, link by link

Each link: who does it, what they need (from whom), what they hand on, and the ways it can be done.

| # | Link | Done by | Needs | Hands on (callout) | Alternatives |
|---|---|---|---|---|---|
| K1 | **Detect** | Senior operator (SOC); commander (PU-12, EWR, neighbour, observation post on the radio); deputy (TV search, `emcon_tv_*`); anyone looking out | SOC: power, `search_antenna`, `soc_antenna`, `soc_transmitter` ready (operator, driver, senior op); a beam programme that covers the target; MTI for low targets. Radio: the right R-123M on, the right preset, the commander on the radio (`crew_comms.hpp`) | Contact: azimuth, range, beam (`contact`); or the PU-12's designation passed on (`designation`) | SOC; PU-12 board; EWR voice; neighbour's radar; TV sector search with the board's azimuth (EMCON); visual |
| K2 | **Designate** (put the tracking radar on it) | Senior operator: azimuth wheel onto the mark; operator: range wheel onto the range; deputy: elevation (wheel, or the elevation scan in ЦУ) | K1's azimuth and range (from the scope or the callout); `antenna_drives` ready (operator), `apu_free` (operator, needs the vehicle stopped: driver); the azimuth lock open (senior op) | "On the target" (`az_on`, `range_on`) | Senior op from his own scope; from the commander's designation (board azimuth with the vehicle oriented, `orient_vehicle`: deputy) |
| K3 | **Acquire / capture** | Deputy: ЦУ (or ТСЦ by the operator); operator: tracking transmitter on | K2 done; `ssc_transmitter` (operator: `hv_ready`, needs `pwr.equipment` and 3 min warm-up); `capture_mode` (deputy) | Capture (`captured`), or TV track (`tv_track`), or radar on a jammer (`on_jammer`) | Radar auto-capture (ЦУ АЗ); manual capture (ТСЦ); TV (ПА: deputy; needs `tov_camera`); range-only radar with TV angles (ПОЛУАВТОМАТ: operator); radar on a jammer's strobe (9.1); low target: TV angles, radar range (НЛЦ) |
| K4 | **Identify** | Senior operator interrogates (ЗАПРОС); commander decides | SOC transmitter, the IFF ready; the target on the scope (gated mode) | `iff_friend` / `iff_no_reply` | The PU-12's code on the board (friend 1x); the commander's orders ("свой") |
| K5 | **Decide** | Commander (vehicle) or the battery commander | K3 (a track), K4, the zone (deputy's `target_near_zone`, `target_in_zone`), orders from above (permission, sectors, priorities) | Fire permission (`fire_permitted`) or hold (`hold_fire`) | Standing permission in a free-fire sector (doctrine) |
| K6 | **Prepare** | Deputy: БР mode, key in, missile preparation (automatic at ЦЕЛЬ В ЗОНЕ, or БОРТ СПК by hand) | `hv_ready`, `fire_control_computer` (operator's equipment switch); a range for the zone (K3 radar, or the range by hand: operator from the SOC or the board) | ПУСК ГОТОВ (`missile_ready`) | Automatic at ЦЕЛЬ В ЗОНЕ; by hand (БОРТ СПК) when there is no range (TV, jammer) |
| K7 | **Launch** | Deputy: key to ПУСК, ПУСК | K5 permission, K6, hatches closed (commander), РОД РАБОТЫ at БР; the launch hold off; a loaded container (TZM, `launcher.hpp`) | Missile away (`missile_away`); a second missile ≥ 4 s later (salvo) | One missile; a salvo of two on both channels (КАНАЛЫ) |
| K8 | **Guide** | Whoever holds the track: radar (automatic), TV (deputy, by hand; the senior operator and deputy turn the wheels in TV), operator (range) | The track held to the end (`ResolveShots`: track lost = missile lost); the transmitter on; the antenna free; for TV, the target in the lens | (silence; "сопровождаю") | Radar; TV; mixed (TV angles, radar or hand range); radar on the jammer with ПОМЕХА (method Н) |
| K9 | **Assess** | Deputy (TV, the missile lamps), operator (the echo on the range indicator), senior op (the scope) | K8 to impact | Hit / miss (`hit`, `miss`) | Re-engage while ЦЕЛЬ В ЗОНЕ stays lit (`voenizdat_osa_1991` p257); next target (40 s for an excellent crew, 120 s for a poor one: `video_osa`, unverified) |

## 3. The mobility chain, link by link

| # | Link | Done by | Needs | Hands on |
|---|---|---|---|---|
| M1 | Choose a position or route | Commander (vehicle); battery commander (battery formation, `battery.hpp`) | The air picture (where the threat comes from), the covered unit, the ground (site rules: level, ≥ 10 m radius, ≥ 500 m from large objects, ≥ 200 m between vehicles; `mon_2112_80` p63–68) | Order: march to, halt at (`march`, `halt`) |
| M2 | Drive | Driver | Power for the engine; the route; the crew's equipment stowed for the march (the АПУ locked: operator; the antenna up or down for bridges: commander) | "Halted" (`halted`); speed and fuel |
| M3 | Halt | Driver, on the commander's order or by drill: target found beyond 20 km → pull 30–50 m off the road; within 20 km → stop at once (`mon_2112_80` p63–68) | — | `halted` (sets `vehicle_stopped`) |
| M4 | Power | Driver: the gas-turbine unit (moving or halted), the take-off generator (halted only), external power (prepared position) | Fuel (`fuel.hpp`); the vehicle stopped for the generator | `power_on` |
| M5 | Deploy | Operator: АПУ unlock, drives; commander: antenna raised | `vehicle_stopped` (driver); power; `equipment_warm` | `apu_free` |
| M6 | Orient | Deputy (ЗК-4, СОГЛАСОВАНИЕ), commander (the measured course angle) | Power, the gyro erected (driver's arrest, 40 s) | `oriented`; without it the board's azimuths point wrong |
| M7 | Get within reach | Commander decides, driver drives | The target's track and the zone; for a battery, the formation and the covered unit | "In position" |

## 4. Who waits for whom (the critical hand-offs)

Read "row waits for column". These are the hand-offs that most often stall a crew, and therefore where the NPCs
remind each other (`04` §5) and where the drill master puts its overlay first.

| Waits → for | Commander | Deputy | Senior op | Operator | Driver |
|---|---|---|---|---|---|
| **Commander** | – | target in zone, missile ready, result | contacts, IFF | capture, range | power, halted, fuel |
| **Deputy** | permission, designation, hatches closed | – | azimuth on the target | capture, transmitter, equipment (`hv_ready`, computer), drives | power; halted (orientation, deploy) |
| **Senior op** | designation (board), orders (beam, range scale, IFF) | orientation (board azimuths) | – | stabilisation and equipment (search antenna), drives | power |
| **Operator** | orders | ЦУ / mode orders ("Расстопорить АПУ", "Включить приводы": `voenizdat_osa_1991` p246) | azimuth on the target (to find range) | – | power, halted (АПУ) |
| **Driver** | route, halt, march order | – | – | АПУ locked before moving | – |

## 5. The control-level dependency trees (summary)

The full trees are generated (`generated/osa_ak_action_map.md`); the main ones, with the seat owning each leaf:

```
launch_armed  ← fo.mode=БР (deputy) + veh.hatches closed (commander) + fo.key=ПУСК (deputy)
               + missile_prep ← hv_ready ← pwr.equipment (operator) + power
                              + fire_control_computer ← pwr.equipment (operator) + power   [100 s]
                              + (fo.bort_spk (deputy) | target_in_zone ← range + zone)       [13 s]
               + launch_hold off (launcher: pair interval)
power          ← gta (driver: pwr.gta, fuel) | pto_generator (driver: pwr.pto, halted) | external supply (position)
soc_transmitter← soc.hv (senior op, under its cover) + hv_ready                                 [180 s]
soc_antenna    ← soc.search + soc.rotation_lock (operator) + search_antenna ← stabilisation ← pwr.equipment +
                 stab.power (operator) [180 s] + apu.stow up (commander) [10 s]
ssc_transmitter← ssc.tx (operator, under its cover) + hv_ready
antenna_drives ← drv.az + drv.el (operator) + apu_free ← apu.lock (operator) while halted (driver) + equipment_warm
capture_mode   ← ЦУ / АЗ (deputy) | ТСЦ (deputy)
auto_tracking  ← capture_mode + ssc_transmitter + antenna_drives + target_captured (the radar's lock: K2, K3)
tv_angle_track ← fo.pa (deputy) + tov_camera ← tov.supply (operator) + tov.power (deputy) + antenna_drives
```

The interdependence is visible in the leaves: **the deputy can't capture before the operator has the transmitter and
the drives on, the operator can't free the АПУ before the driver halts, nobody has power before the driver starts the
turbine, and the senior operator's search antenna needs the operator's stabilisation and the commander's mast.**

## 6. Alternatives (the combinations)

Every link with more than one way is a branch the behaviour paths (`03`) must cover and the drill master (`06`) must
choose between:

| Need | Ways | Chosen by | Rule |
|---|---|---|---|
| Power | GTA; take-off generator; external | Commander (driver carries out) | Moving: GTA only. Halted long: generator (saves fuel). Prepared position: external (`position_power`). |
| Target position | SOC; PU-12 board; EWR; neighbour; observation post; TV search | Commander | SOC when radiating is allowed; board when silent (EMCON) or the SOC is jammed or masked; switch source on silence (`c2_sources.hpp`) |
| Tracking | Radar (ЦУ АЗ / ТСЦ); TV (ПА); TV + radar range (ПОЛУАВТОМАТ); radar on a jammer | Deputy proposes, commander orders | Radar by default; TV when the target is too low (НЛЦ), when silent, or when the radar is jammed and the target visible; jammer track when only the strobe is left (9.1) |
| Range | Radar; range-only radar; by hand from the SOC; by hand from the board; none | Operator | In that order of preference |
| Guidance method | T/T, Горка, Ф (automatic by geometry); Н (ПОМЕХА, by hand) | Deputy | ПОМЕХА on with no range or against a jammer |
| Missiles | One; salvo of two (both channels) | Commander | Salvo against high-value or manoeuvring targets (doctrine, sam3d) |
| Engage or not | Fire; hold; let a neighbour take it | Commander / battery commander | IFF, orders, sectors, priorities (`07`) |
| Stay or move | Fire from here; move within reach; leave (relocate) | Commander | Zone geometry; threat to the vehicle (radiating time, a strike on the position) |

## 7. Failure of a link and what the others do

| Link fails | Seen by | Who reacts, how |
|---|---|---|
| Power lost (fuel, GTA fault) | Driver (lamps), everyone (equipment drops) | Driver: restart or switch to the generator; commander: report to the battery; all: equipment on again in order; orientation is lost (`mon_2112_80` p63–68) |
| Contact lost | Senior operator | Calls it; commander asks the board; deputy TV searches the last azimuth |
| Capture fails | Operator, deputy | Operator re-sets the range; deputy re-presses ЦУ; switch to TV / ТСЦ; for a low target НЛЦ → TV |
| Track lost in flight | Operator, deputy | The missile is lost (`ResolveShots`); report; re-acquire; second missile if in zone |
| Jammed (range lost) | Operator (range indicator), senior op (strobe) | `fire_at_jammer` path (9.1): range by hand, ПОМЕХА |
| Radio lost (PU-12 silent) | Commander | Switch source (`c2_sources.hpp`), autonomous search |
| A seat can't act (player idle, NPC busy) | The seats that wait | Remind (`04` §5): the waiting seat calls it, then the commander orders it by name. Nobody works another seat's controls (`CrewMember` refuses), so the task stays with its seat; in training the overlay shows the player what is due (`08`), and in command the player sees which seat the crew is waiting for |
