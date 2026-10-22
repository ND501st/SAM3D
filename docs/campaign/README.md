# The battery's war: missions, objectives, and the systems around the Osa and the PU-12M

Written 22 October 2026 and reworked the same day after the project owner's review. This folder plans the game logic of
the wider war that the Osa's and the PU-12M's crews, vehicles and battery fit into: how a mission starts, what its
objectives are and why, what is attacked, defended, ambushed, jammed and reconnoitred around the battery and why, and
the game systems that make it run seamlessly. It is a plan, not yet code: the roadmap's phases 13-21
(`docs/DEVELOPMENT_ROADMAP.md`), to start once the open items before them are done (`09_IMPLEMENTATION_PLAN.md` §1).

It builds on the enemy air command (`docs/air_war/`), our side's chain of command and information pipeline
(`docs/command/`), the battery and its tactics (`simcore/battery.hpp`, `data/command/battery_tactics.json`), the PU-12M
(`docs/PU12_DEVELOPMENT.md`), the crew (`docs/crew/`) and the EM layer (`docs/EM_ARCHITECTURE.md`). When this plan and
those documents disagree, the built system's document wins until this plan's task changes it.

## The brief (project owner, 22 October 2026)

1. Our side and **one enemy at a time** fight over the fictional nation: the air war's four adversaries
   (`docs/air_war/01_FACTIONS.md`), one per campaign, as the campaign box starts them today.
2. The enemy's aim is to grind the nation down: military forces, logistics, critical infrastructure, sovereignty and
   production. Its officers use their intelligence to wear the defences down and strike towards their targets.
3. The strategy layer stays simple. The point is the game logic and the implementation of the wider systems the Osa and
   PU-12M crews, vehicles and battery fit into. How does a mission start? What are the objectives? **Why** are they the
   objectives? What is being attacked, defended, ambushed, jammed and reconnoitred, and why? Which game systems make
   it work seamlessly?
4. Kept from the first drafts at the owner's word: **the war aims and the target picking** (`01_WAR_AIMS_AND_TARGETS.md`).
5. **Crew management** comes into play: fatigue and sleep, eating, morale, progression. It lets the player take
   different crew members across missions for a rounded experience (`06_THE_CREW_AS_PEOPLE.md`).
6. **Between missions** the battery and the regiment run maintenance, briefings, recreation and their other
   activities. War is not scheduled, so any of them can be interrupted at any time, and so can a mission. Planning,
   running and scheduling them is part of the player's strategy (`07_BETWEEN_FIGHTS.md`).
7. **Command between the layers may or may not be intact** in a fight, which calls for improvisation and
   reconstitution. **Command changes its doctrine by opponent.** Some enemies have limited reach over our airspace,
   others near air superiority. All of it is in the game logic and systems (`08_COMMAND_DOCTRINE_AND_REACH.md`).
8. **The player cannot significantly change the wider war.** They change two outcomes: the survival of the critical
   assets and forces they protect (the nation's ability to keep up the fight), and the enemy's freedom of action,
   limited simply by the battery surviving and threatening aircraft. That forces the enemy to spend aircraft and
   munitions, and limits its air operations and sortie rate (`02` §5, `01` §6).
9. **Many industrial, civilian and logistic targets can be rebuilt**, but a rebuild takes time, resources, logistics
   and manpower, and each of those can be targeted too (`02` §3).

## What exists, and the seams

| Built | Where | The seam this plan closes |
|---|---|---|
| The enemy air command: officers who plan raids on their picture, by their faction's doctrine | `simcore/air_war/`, `docs/air_war/` | Its targets have a value and a kind, not a role, so nothing follows from a hit beyond the object's own working share |
| Raids through the battery's sector as scenarios | `raid_scenario.hpp`, `mission_bridge.hpp` | Each raid is a separate scenario with the battery "deployed in place" (`docs/air_war/05_EXECUTION.md` §4). Nothing carries over between raids, and the battery never marches, moves or reloads in the campaign |
| The battery's posture (emission, dummies, nets, moves) | `airwar::BatteryPosture`, the campaign screen | It is a set of sliders. No order gives the battery its task, and no task says why |
| The battery's orders and drills: march, short halt, combat formation, ambush (also roving), river crossing, reload, refuel, emission, relocation | `simcore/battery.hpp` | Used in hand-written scenarios. The campaign never issues them |
| Our chain of command and the pipeline | `simcore/command/`, `docs/command/` | It carries news. Nobody on our side decides a battery's task, and no order reaches the battery through it yet (P1, P2) |
| The radio journal and the board | `simcore/radio_journal.hpp`, `simcore/pu12.hpp` | The after-action report is not yet compiled from them (`docs/command/01_PRINCIPLES.md` 9, P9) |

## Decisions

| # | Decision | Where |
|---|---|---|
| B1 | **One enemy per campaign**, as built. Nothing in this plan adds a second | `01` §1 |
| B2 | **Every objective has a traceable why.** The enemy's war aims set its phases; its officers pick targets by worth and utility; its packages come at what they picked. Our objects have roles; the regiment's staff gives each battery a task to cover what matters most against what the enemy is seen to do. The debrief can show the whole chain for any raid and any order | `01`, `02`, `04` |
| B3 | **The battery's mission is the unit of play.** Every hour of a campaign at the battery belongs to a task the regiment gave by an order: cover an object, cover troops, cover a march or a crossing, an ambush, a relocation. The battery commander turns it into the battery's orders by the textbook's sequence, on a working map (`voenizdat_osa_1991` pp. 232-245) | `03` |
| B4 | **The strategy layer stays thin.** Objects have roles, consequences and rebuilds, not an economy. The front is data, not a simulation. The regiment's tasking is a small rule table with reasons, run by its NPC officers. There is no national strategy game | `02` §3, `05` §3 |
| B5 | **One continuous mission.** The battery's state persists from the order to the end of the task: positions, missiles, fuel, damage, dummies, the board, the journal. The enemy's raids, reconnaissance flights, hunters and jammers enter the running mission when they come, rather than as separate scenarios. Time compresses between events | `05` §5-7 |
| B6 | **Everything the enemy does near the battery is a real thing** the crew can see, hear, shoot or hide from, and each has its reason in an enemy officer's plan | `04` |
| B7 | **The written record is the war's memory** (`docs/command/01_PRINCIPLES.md` 9). The battery's report comes from its records, never from the truth. The regiment's picture comes from the reports, weighing each unit by how its records have checked out, and the next order comes from the regiment's picture. The radiotelephonist's accuracy is critical to the loop: the fight, the record, the report, the picture, the plan, the next fight | `05` §10 |
| B8 | **First person as before.** Orders, warnings and reports travel through people and means of signal (`docs/command/01_PRINCIPLES.md`). The working map shows what reached the battery commander, never the truth | `03` §3 |
| B9 | **Sources as before.** Tactics come from the textbook and operator documents. The country is fiction. Balance numbers live in `data/gameplay/` | each phase's step 0 |
| B10 | **The battery's people are people.** Each has qualifications, experience, needs (sleep, food, morale, health), a place and a duty. Needs change the effective skill tier hour by hour. The player may take any fit member of the battery for a mission or an activity, and knows only what that person knows | `06` |
| B11 | **Time is continuous, and war interrupts it.** Between fights the battery runs activities (maintenance, rest, meals, digging, wire, briefings, training) that hold people and equipment away from readiness. Readiness is computed from where everyone and everything is. The enemy's real raids and our orders break in at any time | `07` |
| B12 | **The enemy's reach is game logic.** Depth limits, sorties, the hours its aircraft can fly and air superiority decide where and when anything can come, and whether a battery moving by day is seen. Our side plans on its own estimate of that reach | `08` §1 |
| B13 | **Doctrine by opponent, and command that can break.** Our doctrine is a profile per adversary, changed as the enemy is seen to change. Posts, command posts and links can be lost; NPCs and the player improvise by sourced rules, then reconstitute | `08` §2-3 |
| B14 | **The player changes two outcomes, not the war.** The enemy's aims and phases, the rest of the defence and the rebuilds' priorities are beyond one battery. What battery 21 changes is the survival of what it covers, and the enemy's freedom of action: by surviving and threatening it makes the enemy add SEAD, decoys and jammers, fire ARMs, spend sorties hunting it, reject or re-route packages and lose aircraft, and it holds the enemy in its suppression phase. Both are measured in ledgers and told in the debrief | `01` §6, `02` §5, `05` §14 |
| B15 | **Rebuilding is a chain the enemy can cut.** A rebuild needs teams, materials, transport and time, and the repair site, the materials depot, the pontoon park and the line between are all targets. Temporary fixes come first. Military units are replaced from the reserve, as built | `02` §3, `04` §8 |

## The why chain

```
  the enemy's war aims (its faction file)              our objects and their roles (what each supports)
        │ its phases and apportionment                        │ the division's priorities
        ▼                                                     ▼
  its officers pick targets: worth, utility,           the regiment's staff picks each battery's task:
  approval (docs/air_war/04_PLANNING.md)                rules with reasons, on the regiment's picture
        │ packages: strike, DEAD, hunter, recce,              │ the combat order, by a means of signal
        │ BDA, deception, CAS, helicopters,                   ▼
        │ harassment; SEAD, jammers, decoys             battery 21's task: cover, ambush, march, move
        ▼                                                     │ the battery commander's sequence, the working map
  what comes through the battery's area ───────────►  the continuous mission: the crews, the vehicles, the fight
        ▲                                                     │ the records: the journal, the board, the log
        │ what the enemy learns: emissions, photographs,      ▼
        │ claims, launch sightings, radio calls ◄──── the battery's report ──► the regiment's picture ──► the next order
```

## Contents

| File | What |
|---|---|
| `01_WAR_AIMS_AND_TARGETS.md` | The enemy: each adversary's war aims, how its officers pick targets, why raids come to battery 21, and what the battery costs the enemy in freedom of action |
| `02_WHAT_WE_DEFEND.md` | Our side: what each object is for, what its loss does, the layered defence battery 21 is part of, rebuilding and its means, how the division and the regiment rank what to cover, and the two outcomes the player changes |
| `03_THE_BATTERY_MISSION.md` | How a mission starts: the regiment's tasking, the combat order, the battery commander's sequence and working map, the mission kinds, the mission's life, objectives and their measures, the player at each seat |
| `04_WHAT_HAPPENS_AND_WHY.md` | Around the battery: what is attacked, defended, ambushed, jammed, reconnoitred and deceived, by whom, why, what the crew sees, and what the battery can do |
| `05_THE_SYSTEMS.md` | The game systems that make it seamless: object roles, the regiment's tasking, the battery task, the battery commander's planner, the continuous mission, raids injected live, the director, the enemy's activity in the scenario, our side around the battery, the after-action loop, the debrief, tools and tests |
| `06_THE_CREW_AS_PEOPLE.md` | The battery's people: qualifications, experience, needs and what they do to performance, the duty roster and readiness as a computed time, progression, and whom the player plays |
| `07_BETWEEN_FIGHTS.md` | The battery's day: the activities, what each holds away from readiness, wear and faults, the schedule, interruptions, and scheduling as strategy against each adversary |
| `08_COMMAND_DOCTRINE_AND_REACH.md` | The enemy's reach from limited to near air superiority, our doctrine profile per adversary, and command under strain: what breaks, improvisation, reconstitution |
| `10_BRIEFING_FORCES_AND_NATION.md` | **Briefing (decisions taken; the capital's name to come)**: the country's map, its industry and infrastructure with the oil (mainly from the Pact neighbour: a war with the Pact is a war short of fuel), the command structure (two services beside the Ground Forces), the National Air Defence (about 6 S-75M2 and 10 S-125M1 fire units, the KS-30s, the radio-technical troops, MiG-21s and a squadron of MiG-23MLs), the Air Force, the Ground Forces (two motor rifle divisions of three regiments and a tank brigade) with the troop air defence (five Osa batteries, the tank brigade's Kub battery, Strela-1s, Shilkas, MANPADS, ZU-23s, S-60s), personnel, and support equipment and assets |
| `09_IMPLEMENTATION_PLAN.md` | What must be done first, phases 13-21 with their tasks and done-when, the order, the milestones |
