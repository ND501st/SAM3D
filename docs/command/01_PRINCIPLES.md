# 1. Principles: the player's character knows only what reaches them

1. **First person, one character.** The player plays one person: a crewman, a vehicle commander, the battery commander.
   Everything the game shows is what that person has seen with their own eyes, read on their own instruments, or been
   told. There is no omniscient screen: no enemy order of battle, no list of every raid, no true state of our side's
   objects, no "intelligence estimate" that nobody in the world gave them.
2. **Every fact has an origin and an owner.** A raid is seen coming first by the national air defence sector's radars; a
   bridge is reported down by the garrison commandant who saw it; a prisoner's story comes from the division's
   intelligence officer who questioned the airman. The simulation's truth decides what happened; a person in the chain is the
   first to know it.
3. **Facts travel only through people and the means they have.** In person, a runner, a despatch rider, the courier's
   round, the field telephone, the town telephone, the radio, the division's early-warning net, the sirens, the field
   post, the state radio's news (`03_MEANS.md`). Each has its speed, its reach, its losses and what it costs: a radio
   call is fast and the enemy's intercept service hears it; a courier is slow and covert, and the road may be bombed; the
   telephone needs the wire to be whole (`utin_2018`).
4. **Two rhythms.** Urgent news ("Воздух", an order to move now) goes at once by the fastest means. Routine news waits
   for the next contact: the regiment's evening briefing, the battery's morning orders, the commander's visit, the
   courier's round, the post. So the player learns the day's intelligence at the briefing, as a real officer would, and
   a vehicle commander learns it at the next morning's orders, or never if nobody passes it on.
5. **The chain filters.** A fact is meant for a tree of the chain down to a level: the army's notes on the enemy's
   officers go down to battery commanders; an air-raid warning to every crewman; the regiment's estimate of what the
   enemy knows of the battery to the battery commander. Voice carries a summary; the paper (the full intelligence
   summary, an order with its overlay) comes by hand.
6. **The player's character passes nothing on by itself.** NPCs relay what they are told by their own drills, at their
   own speed (their skill tier). What reaches the player stops with the player: if the player commands the battery and
   does not tell the vehicle commanders, they do not know. Telling them is the player's action (`06_INTEGRATION.md` §6).
7. **Senior officers are people on the map.** The army's chief of air defence works in the city's barracks; the
   division's in a command post in the forest; the regiment's officers at its command post and rear; the garrison
   commandant in the old customs house in town. They hold briefings, visit, call, send couriers, and can be met
   (`05_PLACES.md`).
8. **The HUD is only for the interim.** The campaign screen's panels now show only what reached the player, with who told
   them and how. The aim is for each delivery to happen in the world: the duty officer's voice on the radio, the
   courier's motorcycle at the battery, the chief of staff reading out the summary at the evening briefing, a paper in
   the cabin (`06_INTEGRATION.md` §6).
9. **The written record is the war's memory** (project owner, 22 Oct 2026). What a unit reports after a fight, and what
   its superiors plan on, comes from what its people wrote down at the time, never from the truth: above all the
   radiotelephonist's journal, every message with the time he took it (`pvo_bu_x_1984` art. 24;
   `simcore/radio_journal.hpp`, `docs/crew/12_COMMUNICATIONS.md` §2.9a), then the plotter's board and the commander's own
   notes. The after-action report is compiled from those records, so a message lost unwritten, a short-form entry, a digit
   written wrong or a ghost track on the board goes up the chain as fact: the wrong time of a raid, the wrong square of
   its route, a raid that never was, a corridor missed, a kill claimed against the wrong target. The battery's and the
   regiment's analysis and planning (the corridors and timings the analysts name, the ambushes, the positions, the
   emission policy, our estimate of what the enemy knows of us) are built on those reports. So the radiotelephonist's
   accuracy decides how well his side learns and how well it meets the next raid: it is the core of the after-action
   loop, **the fight, the record, the report, the staff's picture, the plan, the next fight**. The staff check records
   against one another (the journal against the board and the radar company's own log; a 2010 Belarusian textbook of the
   Soviet method, Dedyaev's, checks an operator by setting a live plot against a written record:
   `docs/research/PLOTTING_BOARD_2026-10.md`, "How the plotter got the picture"); a unit whose records prove wrong is believed less, and its people's ratings and careers carry it.
