# 3. The battery's mission: from the regiment's order to the report

A campaign at the battery is a chain of missions. Each starts with an order from the regiment and ends with a report
compiled from the battery's records. The tactics come from the 1991 textbook's chapters on the battery
(`voenizdat_osa_1991` pp. 224-245, summarised in `docs/research/OSA_MANUALS_2026-10.md`, "Chapters 6-7"), and the
battery's orders and drills from what `simcore/battery.hpp` already does.

## 1. Where a mission comes from: the regiment's tasking

A battery changes position only on the regiment commander's order (pp. 232-239). So every mission starts at the 15th
SAM Regiment's command post in the Lazne woods. The commander, Lt Col Vrana, decides. The chief of staff, Maj Sallai,
works it out and runs the evening briefing (`docs/command/02_CHAIN_OF_COMMAND.md`). Routine tasks are set at the 19:00
briefing (`docs/command/05_PLACES.md` §3). Urgent ones go at once.

The tasking is a rule table, not a strategy game (decision B4). Each rule has a trigger the regiment can know (from the
division's priorities, its own picture built from the batteries' reports, and the regiment's estimate of what the enemy
knows), a task, and the reason written into the order:

| Trigger (what the regiment knows) | Task | The reason the order gives |
|---|---|---|
| An object on the division's priority list has no low cover (`02` §4) | Cover the object | "The bridges carry the group's supply; the enemy has struck crossings twice since D2" |
| The covered troops are to move (the campaign's front data, `02` §3) | Cover the march, the crossing, or the troops in their new line | "The 2nd battalion crosses at Kesmark at 04:00" |
| The analysts name a corridor: the same way in four times or more (the reports' pattern) | An ambush on it, or a roving ambush between prepared points | "Raids from the south-west at dawn came up the Rava valley four times" |
| The regiment estimates the enemy knows the battery ("consider moving it") | Relocate to a reserve position, or to a new area | "The enemy's reconnaissance photographed your position at 11:20" |
| ARM attacks seen anywhere in the war | Emission regime tightened: bursts, silence on the PU-12M's designations, the ARM drill (built: `docs/air_war/05_EXECUTION.md` §4) | "Our radars have come under anti-radiation attack" |
| Missiles under two a launcher, or fuel under half | The technical battery's run, the TZM and the ATZ | "Reload before the dawn raids" |
| The enemy's phase seen to change (`02` §4's table) | The priorities re-ranked, and tasks changed to match | "The enemy has turned to the bridges" |
| A battery out of action | A neighbour takes its task, or the object goes uncovered | "Battery 23 is out: take over its sector" |

The regiment's officers bend the rules by their characters, using the enemy officers' machinery (`airwar::Officer`'s
traits and quirks). A methodical chief of staff moves batteries on schedule. A bold commander sets ambushes forward. A
rigid one keeps the same prepared positions until a battery is lost. The reasons say so, as the enemy's packages carry
their officers' reasons (`docs/air_war/04_PLANNING.md` §7).

The rules are read through **our doctrine profile for the adversary** (`08` §2): against the rebels a battery seldom
moves; against NATO it moves by night and fights silent. And **an order may never arrive**: when command between the
regiment and the battery is cut, the battery acts on its last order and the improvisation rules (`08` §3.2) until
contact is back.

## 2. The combat order

The order is a document: a paper with an overlay (`docs/command/04_PIPELINE.md`: a fact with `document`). Its contents
follow what the textbook has the battery commander put on the working map (pp. 240-245):

| Part | What | Example |
|---|---|---|
| **Task and reason** | What to cover, and why (§1) | Cover the Kesmark bridges from low-flying attack; the enemy's interdiction |
| **The situation** | The enemy's expected attack directions, helicopter lines, jammer orbit lines, ARM launch lines; what the regiment knows of the enemy's habits | Attack from the west and south-west at dawn and dusk; a jammer orbit 40 km west |
| **Position area** | The main positions, reserve positions within 2 km (p232-239), dummy positions, the TZM's place within 500 m | Three prepared positions east of Vrbice; dummies at the crossroads |
| **Sectors** | Each launcher's sector of responsibility and the MANPADS section's; duty launchers with 45-60 deg sectors on the helicopter lines (p262-266) | 21-1 and 21-2 west, 21-3 and 21-4 south-west |
| **Readiness and emission** | The readiness to hold, and when radars may radiate: bursts, silence on the PU-12M's designations, the ARM drill | Readiness No. 1 at dawn and dusk, No. 2 otherwise; search in bursts |
| **Time** | Ready by; how long the task lasts | Ready by 05:00; until relieved |
| **Coordination** | The neighbours' sectors (the ZU-23 at the bridges, battery 22), the friendly air corridor and its times, IFF codes, call signs and signals, the nets and frequencies (`simcore/radio_nets.hpp`) | Our fighters' corridor north of Kesmark, 06:00-08:00 |
| **Reports** | What to report, when and how (the readiness report «к бою готов», each engagement, the day's report) | By wire to the regiment's CP; by radio only in a fight |
| **Support** | The technical battery's run, fuel, the medical post, repairs | TZM at 03:00 at the Horka siding |

**How it reaches the battery commander** (`docs/command/03_MEANS.md`; P1 and P2): in person at the evening briefing,
which they drive to and from; by the courier's round with the overlay; by telephone if the wire is whole; by radio in
short form, which the enemy's intercept service hears (P3). A relocation order after a reconnaissance photograph is
priority, so it goes by wire, or else by radio, or else by despatch rider. The time it takes is time the battery spends
where the enemy has seen it.

## 3. The battery commander's sequence and working map

The textbook's sequence for the battery commander (pp. 240-245):

1. **Understand the task**: what is covered, from what, by when, with whom.
2. **Work out the time**: ready-by, march time, the position's preparation.
3. **Preliminary orders** to the vehicle commanders: prepare to march; the TZM to the siding.
4. **Assess the situation** on the working map.
5. **Decide**: which positions, the sectors, the emission regime, where the dummies go.
6. **Reconnoitre** the position: open ground with a field of view along the attack directions, nothing tall within 20 m
   of a launcher (the raid builder already finds such ground, `docs/air_war/07_INTEGRATION.md` "On the map").
7. **The oral combat order** to the vehicle commanders, in person or on the battery net.
8. **Organise support**: reloads, fuel, communications (nets, frequencies, the talk tables), the observation post.
9. Political work (in the textbook's list; a line in the debrief only).

**The working map** (p. 240-245) carries the attack directions, the helicopter and jammer lines, the ARM launch lines,
the search radar's detection limits, the launch zones' far edges for the likeliest heights, the friendly air corridor,
and the call signs and signals. In the game it is the battery commander's own map in the PU-12M. It is drawn from the
order and from what has reached the battery since (B8), and kept beside the board and the battery's logs.

When the battery commander is an NPC, it runs the sequence as a planner with an ideal timeline, as the drill master
does for a vehicle (`docs/crew/06_REFERENCE_PERFORMER.md`). When the player holds the post, the helper's shadow shows
the NPC's choice at each step (`docs/crew/14_HELPER.md`).

## 4. The mission kinds

| Kind | Given when (why) | What the battery does (`battery.hpp`) | Primary objective | Numbers |
|---|---|---|---|---|
| **Cover an object** | An object on the priority list needs low cover | `March` to the area; `CombatFormation` round the object; dummies, nets and emplacement (7.6); duty at the ordered readiness | The object keeps working | Pairs up to 3 km apart, 200-500 m within a pair, TZMs within 500 m, reserve positions within 2 km (pp. 232-239) |
| **Cover troops in the field** | The covered companies are in a line, or advancing | `CombatFormation` behind the covered unit, on its axis; `bounding` when it advances | No losses to air attack among the covered troops | 1.5-3 km behind the covered companies; the battery 3-5 km from the forward edge (pp. 232-239) |
| **Cover a march** | The covered unit, or the battery itself, moves | `March` in the column; some launchers search; a `ShortHalt` to fire (in the column within 20 km of the target, off the road beyond); flags and lamps, no radio but for fire control | The column arrives on time, unhurt | Column PU-12M, two launchers, TZM, two launchers, TZM; 25-50 m gaps, at least 200 m when an attack is expected; halts of up to 1 h every 3-4 h (pp. 232-239) |
| **Cover a crossing** | The covered unit crosses the river | `RiverCrossing`: 2 km short, then across in pairs once the far bank is held (`FarBankHeld`) | The crossing is not hit while the troops are on it | `battery_tactics.json` (estimates) |
| **Ambush** | The analysts name a corridor | `Ambush` on prepared points along it, radars silent until the designation; the `roving` variant moves between points every `ambush_dwell_s` | Aircraft downed on the corridor; the battery not located | `battery_tactics.json` (`video_osa`, estimates) |
| **Relocate** | The enemy knows the position; or the task moves | Leave the emplacement (60 s), `Relocate` with the dummies, prepare the new position | The battery in its new position, ready, before the enemy's next strike | Leaving the emplacement 60 s (7.6); into combat order 35-50 s, back to march order 15-30 s (p231) |
| **Cover a rebuild** | A priority object is being rebuilt, or a temporary crossing is put in (`02` §3); the enemy re-strikes repair its reconnaissance sees (`mai_tactics_vvs`) | `CombatFormation` round the site, the pontoon bridge or the materials' route; duty at readiness through the working hours | The rebuild finishes | As for covering an object |

Readiness and emission regimes run inside every kind (`Emcon`, the ARM drill). Reloads and refuelling (`Reload`,
`Refuel`) come when the stocks run down.

## 5. The life of a mission

| Step | What happens | Built on |
|---|---|---|
| 1. The order | Received in person, by courier, telephone or radio | `info_net.hpp`, P1/P2 |
| 2. The sequence | Preliminary orders, the decision, the working map, reconnaissance, the oral combat order | New (§3) |
| 3. The march | The column; flags and lamps; short halts against a raid on the march | `battery.hpp`, `MarchRadioBan` |
| 4. Occupation | Combat formation; the PU-12M deployed (18 min); orientation (the compass survey, 180 s); emplacement, nets and dummies; the nets checked; «к бою готов» reported | 7.5, 7.6, 6.5, 7.9, 8.13 |
| 5. Duty | The ordered readiness and emission; the duty launchers; the board; the warning net; time passes quickly until something comes | `c2_sources.hpp`, `pu12.hpp`, the director (`05` §7) |
| 6. Raids | Warnings, designations, engagements, kill assessment, the ARM drill; the journal and the board record it | The crews, the runner, E6 |
| 7. Service | The TZM and the ATZ when the stocks run down, over the real roads and bridges | 7.3, 7.4 |
| 8. Moves | On the regiment's order, or the covered unit's move | `Relocate`, `March` |
| 9. The end | The task's time ends, a relief arrives, the covered unit's move ends it, or the battery is out of action | New |
| 10. The report | Compiled from the journal, the board and the commander's log; sent up by the means the post has | P9 |

Through all of these steps, and between one task and the next, **the battery's day goes on**: the watch roster,
sleep, meals, maintenance, digging, wire, briefings and training (`07`). A raid or an order can break into any step,
and into any activity. Readiness is then reached as fast as the people and the equipment can get back from what they
were doing (`07` §3).

## 6. Objectives, their measures, and the debrief

The player cannot change the war's course (`02` §5). Every task's objectives serve the two things the battery can
change. The debrief tells them without points (as in SAM Simulator and in reality), each with its reason and its
consequence:

| Outcome | Objective | Measured by | Told as |
|---|---|---|---|
| **Assets kept**: the nation keeps up the fight | The covered object, troops, column or rebuild | Its working hours kept; hits on it; the column's arrival; the rebuild finished | "The bridges stood: the trains of the 23rd went through at 02:00" (a consequence from `02` §1, delivered as a fact through the pipeline) |
| | Releases prevented | Aircraft downed or turned back before their release point | "The dawn strike's second flight turned back short of the bridges" |
| **Freedom of action denied**: survive and threaten | The battery kept | Vehicles and radars intact; how well the enemy knows the battery (`EnemyKnowledgeOfBattery`) | "The enemy believes battery 21 is 3 km east of where it is" |
| | The enemy made to pay to work near the battery | The ledger (`05` §14): SEAD, decoys and jammers added for it; ARMs fired at it; DEAD and hunter sorties against it; packages rejected or re-routed; aircraft downed and damaged; days the suppression phase was held open | At the reveal, in the enemy's own reasons: "SEAD: 2 aircraft for 1 radar threat(s) believed on the route" |
| | Reconnaissance denied | Reconnaissance and BDA aircraft downed before they got home | "The RF-4C did not get home: the next strike was planned on yesterday's photographs" |

Supporting measures: missiles spent per aircraft downed and the containers left; the record, the journal and the board
against what was said and seen (built: `docs/crew/12_COMMUNICATIONS.md` §2.9a): "Two designations not written down: the
regiment's picture is short of the 05:12 raid". The crew's grades and the drill master's reference stay as built
(`docs/crew/08_TRAINING_AND_ASSISTANCE.md`).

## 7. The player at each seat

| The player's post | How the mission reaches them | What they do |
|---|---|---|
| A seat of 21-1 | The vehicle commander's orders; the morning orders; the intercom | The seat's work, through every step of §5 |
| Vehicle 21-1's commander | The battery commander's oral combat order; the battery net | The vehicle's position, its sector of responsibility, readiness and emission, reports |
| Battery 21's commander (the PU-12M) | The regiment's order, and everything after it through the pipeline | The sequence and the working map, the oral combat order, the board, fire distribution, logistics, reports, the battery's daily schedule (`07` §5) |

Across missions the player may take **any fit member of the battery**, not only the post their main character holds:
the driver for a march, the radiotelephonist for a night on the journal, a TZM's crew for a reload (`06` §6).

The regiment's posts are not planned for the player (`09` §4, "Later"). Progression stays as the crew documents have
it: the seat exams open the vehicle commander's post, and the commander's exams open the battery's
(`docs/crew/08_TRAINING_AND_ASSISTANCE.md` §4).
