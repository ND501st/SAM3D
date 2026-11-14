# 7. Between fights: the battery's day, its schedule, and the war that interrupts it

Planned code: `simcore/campaign/activities.hpp` (the activities and the schedule), `simcore/campaign/equipment_wear.hpp`
(wear and faults). Data: `data/campaign/activities.json`, wear and fault rates in `data/gameplay/`. Roadmap phase 20.

## 1. Time is continuous

There is no gap between missions. The battery lives through days. Its task (`03`) frames them, its activities fill
them, and the war interrupts them whenever the enemy's plan says, not on a schedule. Maintenance, briefings, meals,
sleep, digging and training compete for the same people and the same hours as readiness. Planning, running and
rescheduling them is part of the player's strategy, with the battery's readiness, its people (`06`) and its equipment
as the stakes.

## 2. The activities

| Activity | Who, where | Takes | Gives | Built on, or sourced |
|---|---|---|---|---|
| **Watch** at the ordered readiness | The duty crews at their posts | People, awake | Readiness; the board; the warning heard | Readiness Nos. 1-3 (`pvo_bu_ix_1984`); `06` §4 |
| **Sleep, meals, hygiene, letters** | In the tent, by the vehicle, at the PU-12M | Time off post | Needs eased, morale (`06` §3) | The rest places (`docs/crew/13_OUT_OF_THE_VEHICLE.md` §4); the 2-hour meal halt on the march (p. 236) |
| **Daily and periodic maintenance** | A vehicle's crew at the vehicle; the maintenance crew with the 9V210M3 | The vehicle partly out: covers off, blocks open, power off | Faults prevented (§4); the function check after (7.1 min from cold) | The textbook's maintenance pages (pp. 206-224; to read in 20.0); the 9V210M3 serves four launchers and deploys in 40 min (p. 189) |
| **Repairs** of faults and damage | The maintenance crew; the regiment's technical battery; the works for heavy repairs | Hours to days; spares (ZIP) | The vehicle back | The drill master's fault recovery (8.3); the air war's repair and replacement |
| **Reload and refuel** | The TZM and the ATZ alongside | The launcher not ready while loading | Missiles, fuel | 5-8 min a launcher (p. 188; 7.3); the ATZ (7.4) |
| **The sergeant major's run** | The sergeant major to the rear and back, 09:00 | A truck on the roads | Food, fuel, the post, the rear's news | The routine in `places_pannonia.json` |
| **Position work** | Crews and the supply section | Hours of digging | Emplacements, nets, dummies, reserve positions: the battery's echo smaller (7.6) | `position_prep.hpp`; "a dig job with hours of time" (`13_OUT_OF_THE_VEHICLE.md` §4) |
| **Wire** | The PU-12M's crew with a reel | A man walking the line | A telephone line to a launcher or the regiment: covert against intercept | P-274: four lines at 500 m in 25-30 min (`ru_pu12_text`); 8.14's next step |
| **Orientation** | The vehicle commander with the aiming circle | 180 s at a halt | Qc set, the gyro checked | 6.5 |
| **Briefings and orders** | The regiment's evening briefing (the battery commander drives there); the battery's morning orders; the crews told | The battery commander away for an hour and the drive | Orders, the day's intelligence, the schedule given out | The routines (`docs/command/05_PLACES.md` §3) |
| **Visits** | The regiment commander, the political deputy, the garrison commandant | People's time | Orders, morale, the town's news | The routines |
| **Training** | Drills on the vehicle, exams | The vehicle at drill | Experience, classes (`06` §5) | The lessons and exams (8.7, the tutorials) |
| **Guard and observation** | Sentries; the observation post on the Vrbice tower | People | Security; the battery's eyes when its radars are silent | The chain's observation post |
| **Reports** | The battery commander, the radiotelephonist | Time at the PU-12M | The day's report from the records (P9) | `docs/command/01_PRINCIPLES.md` 9 |
| **Recreation** | A film, football, a walk to Vrbice | Time off post | Morale | Risk: talk in the village's pub reaches the enemy's agents (its HUMINT, `docs/air_war/03_INTELLIGENCE.md` §2) |

## 3. What an activity is, in the model

Each activity is data, and runs as jobs at places (the form `docs/crew/13_OUT_OF_THE_VEHICLE.md` §4 already set out):

| Field | Meaning |
|---|---|
| `who` | How many people, which qualifications |
| `where` | The places, so the people walk there and back (`crew_mobility.hpp`) |
| `equipment_state` | What it leaves open, off or away: a radar's covers off, the antenna stowed, the vehicle's power off, the TZM at the siding |
| `duration` | Minutes or hours, scaled by skill and condition (`06` §3) |
| `gives` | Stocks, faults cleared, position improved, needs eased, morale, experience |
| `drop_time` | How long it takes to abandon it and make the equipment fightable: close the covers, power up (the 4.0-minute cold switch-on and the 7.1-minute function check exist, 6.2), the people back to their seats |
| `if_dropped` | What is lost (the half-dug emplacement keeps what is dug; an interrupted maintenance may leave a fault; sleep cut becomes fatigue) |

**Readiness is computed, not declared.** The battery's time to readiness No. 1 at any moment is the slowest of what
every needed person and piece of equipment must do from where it is: wake, walk, close up, power up. The debrief can
say: "Readiness No. 1 four minutes after «Воздух»: 21-3's search radar was open for maintenance, its operator asleep in
the tent."

## 4. Wear and faults

Equipment wears with use: hours radiating, the turbine's hours, kilometres driven, missiles held prepared (the gyros'
10-minute hold and rest are already modelled, 6.3). The chance of a fault grows with the hours since its maintenance.
Faults use the consoles' fault model and the drill master's recovery (8.3). Maintenance resets the clock. Skipping it
buys readiness now and costs a fault later. The rates are game values until 20.0 reads the textbook's maintenance
pages.

## 5. The schedule

The battery's daily routine (распорядок дня) is the battery commander's plan:

- **Fixed points** come from outside: the regiment's routines (the 19:00 briefing, the courier rounds, the visits),
  and the readiness the regiment orders for each part of the day (`03` §2, by doctrine, `08` §2).
- **The battery commander places the rest**: the watch roster, sleep shifts, meals, which vehicle is maintained when,
  the digging, the wire, training. They place it in the windows the threat estimate allows (`08` §1), using what the
  regiment knows of the enemy's habits ("the dawn raider", "a night owl").
- **The vehicle commanders** run their own crews within it: who sleeps when, the vehicle's maintenance.
- **It goes out as orders**: the morning orders at 06:00 give the day's plan. A change goes by order.
- **NPCs plan it, the player may take it over.** The NPC battery commander builds the schedule as a planner, like the
  sequence in `03` §3. The helper shows its choice to a player battery commander. A player in a seat lives under the
  schedule their commander set.

## 6. Interruptions

War is not scheduled. At any moment an alarm («Тревога»), an air-raid warning («Воздух»), a designation on the board,
a raid seen by the battery's own radar, an order to move, or a call to the regiment can break into whatever is
happening:

1. People drop what they are doing by its `drop_time` rules and go to their posts by their real legs.
2. Readiness comes when the slowest needed piece is ready (§3).
3. When it passes, activities resume where they stopped, or are rescheduled. Lost sleep becomes fatigue; a half-done
   maintenance may leave a fault.

The interruptions come from the enemy's real plans, at the hours its officers chose for their reasons (`01` §3; their
quirks: the dawn raider, the night owl, the double tap). Orders interrupt too: a relocation in the middle of the
digging, the briefing called during a raid. **Learning the enemy's habits is how the player schedules well.** A
battery that maintains its radars at dawn against a dawn raider pays for it.

## 7. Scheduling as strategy

| Trade-off | One way | The other way |
|---|---|---|
| Readiness against rest | Everyone at posts: fast to fight, tired by day three | Shifts: slower to full readiness, fresh for longer |
| Readiness against maintenance | Never open the equipment: no gaps now, faults later | Maintain in the quiet windows: a vehicle down for an hour, fewer faults |
| Position work against rest | Deeper emplacements and better nets, tired crews | Rest, and a battery easier to find |
| Moving against staying | A new position the enemy has not seen; a night's sleep lost | A known position, dug in |
| Training against everything | Better classes over the campaign | Hours taken now |

The trade-offs shift with the adversary's reach (`08` §1), as the data has it:

| Adversary | The safe windows, as the data has them |
|---|---|
| The rebels | Nights belong to them (An-2s at night; strikes at night, dawn and dusk); days are quieter; beyond 80 km nothing comes |
| The Noric Republic | No night-capable aircraft; raids at first and last light and by day: nights for maintenance and sleep, readiness No. 1 at dawn and dusk; beyond 40 km (60 if allowed) nothing comes |
| The Warsaw Pact | No rest in the two days of the air operation; after it, a lower tempo; Su-24s and MiG-25s by night |
| NATO | Day and night, all weather, no depth limit: no safe window, only less likely ones; rest in shifts, maintenance in short windows under nets |

## 8. The regiment's side

The regiment has its own day: the technical battery tests missiles (the 9V242-1 station, 6-7 an hour, p. 188-189) and
sends the TZMs' loads; the rear sends food, fuel and the post; the command post runs the briefing and the courier
rounds. When the battery commander is at the evening briefing, the battery is run by the deputy, a handover of command
by routine (`08` §3).
