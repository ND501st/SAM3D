# The dynamic campaign test

A testbed for the game's look, sound and play: the missions menu's **Test range → Dynamic campaign test**. Battery 21
stands deployed in the capital and air defence of every kind stands round it. Waves of aircraft come over the city
without end, every adversary's and our own, for four hours. It is not the air war (`docs/air_war/`): no officer
plans these raids, and nothing in them is sourced. It exists to put everything in the sky and on the ground at once.

## What is where

The scenario is `data/scenarios/dynamic_campaign_test.json` on the Kesmark map (`/Game/Maps/KesmarkRange`). Its origin
is the capital's middle (`data/maps/pannonia.json` `settlements.capital`, 36 km east and 58 km north of the theatre's
origin), and its heights come from the map's ground.

- **Battery 21**, deployed on open ground at the capital's eastern edge. Your Osa is at 6.2 km east and 3.5 km north.
  With it are the launchers 22-24, the TZM, the ATZ and the PU-12. The crew and the battery commander are NPCs; take
  any seat. An Osa's position is open ground with sight along the attack directions (`voenizdat_osa_1991`
  pp. 232-239). Until 3 Nov 2026 the battery stood 1.2 km east and 0.9 km north, among the blocks of flats, where R6's
  buildings 285-400 m off hid anything below as much as 4.1 degrees (the worst over its four Osas, with the obstacle grid of
  the time, which stood each building 12.5 m wider): a cruise missile 60 m up was not seen until it was inside the
  ring of flats. From the edge, each Osa's mask over the roofs within 6 km is under 0.3 degrees
  (`test_dynamic_campaign.cpp`).
- **Our air defence within about 7 km.** None of it is ours to command:
  - an Osa battery in the city park, which fights;
  - a Kub, an S-75M2 and an S-125M1;
  - Strela-10 and Strela-1 platoons;
  - two Shilka platoons;
  - **ten S-60 batteries** in two rings, at 3.3 km and 5.3 km;
  - two KS-30 batteries;
  - three ZU-23 sites, AA machine guns and two Strela-2 teams.
- **Radars:** a P-14 with its PRV-11, a P-15, a P-18, a P-19, a P-40 and the regiment's PU-12. The search radars'
  antennas turn (`docs/scenery/README.md`). The P-15, the P-18, the P-19 and the P-40 feed the regiment's picture
  (`c2.regiment`, `data/command/regiment_radars.json`): the PU-12M's board holds what they cover.

## The waves

The waves come from `data/gameplay/test_waves.json`, made by `simcore/air_war/test_waves.hpp` and flown by the
`ScenarioRunner` (a scenario's `"waves"`):

- **Timing.** The first wave comes at 30 s, then one every 1.5 to 3 minutes. A wave holds off while 60 of the waves'
  aircraft are already up.
- **Flights.** Each wave has 3 to 6 flights, and about a third of them are ours.
- **Enemy types.** The types are dealt from a shuffled deck of every type of all four adversaries
  (`data/air_war/faction_*.json`), so every type comes round, whatever its year.
- **Enemy routes.** Each comes in from its front's side and flies its role:
  - strikes and DEAD, low over the ground or at medium height, through an IP to an aim (one of the sites, or a point in
    the city);
  - SEAD and hunters, standing off with their ARMs and warning receivers;
  - escorts, sweeping through;
  - recce, passing high or low;
  - decoys;
  - helicopters, which hold short and then pass;
  - cruise missiles;
  - stand-off jammers on an orbit 45 km out, and the AWACS, tankers and ELINT orbits 105 km out.
- **Our side.** Patrols, interceptors, Su-22 and L-39 attack runs, transports and helicopters. Their numbers are
  placeholders.
- **Weapons.** A strike aircraft within release range of its aim lets its load go: bombs, cluster bombs, guided bombs,
  air-to-ground missiles, rockets, anti-tank missiles and gun runs. The runner gives each to the presentation feed
  (`simcore/fx/events.hpp`, `docs/fx/`) as a release, by a delivery its weapon allows, with its landing time from the
  raids' weapon flight model (`air_war/weapon_flight.hpp`), and then as an impact. The game flies, bursts and sounds the
  munitions from the munitions library. A cruise missile is an aircraft of the session, and its warhead goes off where
  it ends. These weapons do no damage; ARMs do, through the EM layer.
- **Our air defence.** Every site but the Osa batteries (which fight in the battery fire model) engages the enemy's
  aircraft in its reach (`ScenarioRunner::FireNeighbourDefences`):
  - each takes an aircraft on once as it comes into its reach. Whether it is shot down on that pass is the raids' own
    chance per pass (`air_war/test_waves.hpp` `WavePassKillChance`), with the system's reach, lethality, cover low
    and medium, fire channels and fastest target from the nation's theatre (`theatre_pannonia_nation.json`, else the
    tuned theatre it extends; `defences` in `test_waves.json`) and the raids' tuning (`gameplay/air_war.json`). Nothing
    suppresses or jams them, and the pilots do not evade. That chance is the raids' calibrated loss per pass, so most
    missiles miss;
  - with `"damage"` and `"aaa"` in `test_waves.json` (as now), what a hit or a burst does is drawn by chance
    (`docs/DAMAGE.md`, roadmap R1-R2): a missile's kill destroys the aircraft or leaves it heavily damaged to fall
    10 s to 5 min later; a miss may burst near and damage it; damage short of destroyed may bring smoke, fire, a
    jettisoned load and a turn for home. In a test half hour some 35 damage levels rose and 8 loads were jettisoned;
  - the Kub, the S-75M2, the S-125M1, the Strelas and the MANPADS launch, as many at once as their fire channels and a
    few seconds apart. The missile bursts on the aircraft at the meeting if it hit, or flies on and destroys itself.
    An empty site reloads in ten minutes;
  - the S-60s, KS-30s, Shilkas, ZU-23s and machine guns fire passes of a few seconds at the nearest aircraft in reach,
    then pause. With `"aaa"` every round is fired (`simcore/aaa.hpp`): the fire control's errors grow with range, a
    contact gun's rounds are judged where the aircraft is when they arrive, the KS-30's time-fuzed shells burst one by
    one, mostly well clear. Without it, the pass that took on a new aircraft brings it down at its end if it hit;
  - a kill is in the debrief ("destroyed by" the site) and its wreck falls.
- **Clean-up.** An aircraft that has finished its route leaves the world a minute later, so four hours do not fill it.

An ARM that knocks out your vehicle is logged and the session runs on (`"end": {"on_unit_hit": false}`).

## Looking at it

**F12** visits every asset in turn, ours and the enemy's. Press it again for the next one and Shift+F12 for the one
before. The order is:

1. your vehicle;
2. everything the simulation moves: aircraft, their weapons, our missiles, the battery's vehicles;
3. the places that stand: the scenario's air defence sites and radars, then the map's theatre objects.

The label says whose it is and which one of how many. The right mouse turns the camera and the wheel brings it closer.
Ctrl+Z speeds time up to x32. The other views are in `docs/PLAYING.md`.

## Known limits

The project owner's review of 2 Nov 2026 turned several of these into the roadmap's track R
(`docs/DEVELOPMENT_ROADMAP.md`): damage short of a kill and the guns' accuracy at range (R1-R2, done in first form,
`docs/DAMAGE.md`), assets drawn under the map (R5, done in first form: one ground and the checks below), aircraft
attitude, the map's realism, the time of day and the pilots' defensive reactions.

**Nothing under the ground (R5).**

* The test "dynamic campaign: nothing is under the ground" plays ten minutes of the waves and judges everything every
  five seconds against the drawn ground (`simcore/ground_check.hpp`).
* `ground_check data 3600` plays the whole testbed hour and the missions menu's maps. It is in `tools/verify.ps1` and
  `tools/verify.sh`.
* `python tools/ue_ground_check.py` checks the same in the game.
* What keeps the waves over the ground:
  * The pilots hold the belly a metre over the ground 3 s ahead.
  * The reconnaissance helicopters fly low.
  * A low weapon ends on its aim.
  * A diving weapon ends where its line meets the ground.
* Against the base (da0038a), R5 changes only the testbed's rows: `crew_balance data 10` and
  `air_war_balance data 20 7` are byte-identical. In `helper_coverage` the testbed's Kills fall (NPC 25 to 23,
  commander 29 to 22, deputy 29 to 22), and two gaps appear, "fewer kills (22 vs 23)" for the commander and deputy.
  * The Kills column counts every enemy target dead in the scenario, by any site. In the commander's 900 s the base
    has 32 "destroyed by" events and R5 23, nearly all by the neighbours' abstract fire (the city park's Osa battery,
    the S-60s, the ZSUs). The player's own Osa launches the same in both: 2 for the commander, 3 for the NPC crew.
  * The waves are the same to wave 3. From wave 4 the generator, which reads the war's state, draws another war:
    other types and sizes (the base's wave 4 has 18 aircraft, R5's 11). The aircraft fly differently (held over the
    ground, weapons ending on their aim), so the state it reads differs.
  * So the drop and the two gaps are the testbed's random war, not a helper path: the gaps compare a seat with the NPC
    row of the same build, within one or two of the 150-odd targets.

- **The other sites are not the Osa's model.** They fire by the raids' abstract chance per pass, not by a guidance
  model, and their missiles' speed (700 m/s), the launch interval and the reload are game values. Their radars do not
  radiate in the EM layer, so the enemy's ARMs and warning receivers do not see them.
- **The PRV-11 and the P-14 are drawn but feed nothing.** The regiment's radar list has no sourced range for them, and
  a height finder is not a search radar.
- **Our side's aircraft are placeholders.** Their types, speeds and heights are game numbers. The waves' weapons do no
  damage.
