# The dynamic campaign test

A testbed for the game's look, sound and play: the missions menu's **Test range → Dynamic campaign test**. Battery 21
stands deployed in the capital and air defence of every kind stands round it. Waves of aircraft come over the city
without end, every adversary's and our own, for four hours. It is not the air war (`docs/air_war/`): no officer
plans these raids, and nothing in them is sourced. It exists to put everything in the sky and on the ground at once.

## What is where

The scenario is `data/scenarios/dynamic_campaign_test.json` on the Kesmark map (`/Game/Maps/KesmarkRange`). Its origin
is the capital's middle (`data/maps/pannonia.json` `settlements.capital`, 36 km east and 58 km north of the theatre's
origin), and its heights come from the map's ground.

- **Battery 21**, deployed in the eastern districts. Your Osa is at 1.2 km east and 0.9 km north. With it are the
  launchers 22-24, the TZM, the ATZ and the PU-12. The crew and the battery commander are NPCs; take any seat.
- **Our air defence within about 7 km.** None of it is ours to command:
  - an Osa battery in the city park, which fights;
  - a Kub, an S-75M2 and an S-125M1;
  - Strela-10 and Strela-1 platoons;
  - two Shilka platoons;
  - **ten S-60 batteries** in two rings, at 3.3 km and 5.3 km;
  - two KS-30 batteries;
  - three ZU-23 sites, AA machine guns and two Strela-2 teams.
- **Radars:** a P-14 with its PRV-11, a P-15, a P-18, a P-19, a P-40 and the regiment's PU-12. The P-19 and the P-40
  also feed the regiment's picture (`c2.regiment`).

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
- **Our guns.** The S-60s, KS-30s, Shilkas, ZU-23s and machine guns fire gun passes of a few seconds at the nearest
  enemy aircraft within their reach, then pause. They are drawn and heard, but they hit nothing.
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

- **Only the Osa batteries shoot anything down.** The guns' passes are for show (above). The Kub, the S-75M2, the
  S-125M1, the Strelas and the MANPADS are drawn and plotted on the post's board, but they do not fire: the battery fire
  model has only the Osa.
- **Our side's aircraft are placeholders.** Their types, speeds and heights are game numbers. The waves' weapons do no
  damage.
