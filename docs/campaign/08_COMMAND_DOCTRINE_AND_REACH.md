# 8. The enemy's reach, doctrine by opponent, and command under strain

Planned code: `simcore/campaign/air_situation.hpp` (the reach and the threat estimate), our doctrine profiles in
`data/campaign/doctrine.json`, `simcore/campaign/command_state.hpp` (posts and links intact or not, improvisation,
reconstitution). Roadmap phase 21; the tasking (14.2) runs on one default doctrine profile until 21.3.

## 1. The enemy's reach

Each adversary reaches over our country as far, as often and at the hours its data allows. From limited reach to near
air superiority:

| Adversary | Aircraft (placeholders) | Sorties a week (`docs/air_war/09_BALANCE.md`) | How deep | When | Suppression, jamming | What it sees |
|---|---|---|---|---|---|---|
| The rebels | 11 | About 25 | 80 km | Night, dawn, dusk; reconnaissance by day | None | Sympathisers everywhere; captured radio sets |
| The Noric Republic | 52 | About 40 | 40 km (60 if pressure allows) | Dawn, day, dusk: no night-capable types | None; escorts | Border observers; a border ELINT station (40 km) |
| The Warsaw Pact | 130 | About 620, two days of surge first | No limit | Day and night (Su-24, MiG-25) | Suppression groups, ARMs, stand-off jammers | Satellites (36 h), ELINT aircraft (150 km), agents |
| NATO | 176 | About 700 | No limit | Day and night, all weather | DEAD with decoys, hunters, HARM, EF-111 | Satellites twice a day (2 h), TDOA ELINT (250 km), reconnaissance |

**Air superiority** is a state the campaign already moves through. The Pact's air operation and NATO's rollback end
when the enemy believes our air defences are broken (`docs/air_war/04_PLANNING.md` §2). Then it flies at medium height
above the light flak, sends reconnaissance where it likes, and turns to the bridges and the troops.

**In the game logic** the reach is read two ways:

- **The truth** decides what happens. Beyond its depth limit nothing of the enemy's comes. At hours its aircraft
  cannot fly, nothing comes. Under its air superiority, a battery **moving by day is seen**: each hour on the road it may
  be sighted by the packages crossing the area, by reconnaissance and by hunters, as a pilot's report with its error.
  That is a new source in the enemy's picture, by its activity over the area and by its night-capable types after
  dark.
- **Our estimate** decides what we do. The regiment keeps a threat estimate by area and hour (`air_situation.hpp`):
  where and when raids have come, how deep, at what height, built from the reports (`05` §11). Doctrine (§2) and the
  battery's schedule (`07` §5) are planned on it, not on the truth.

So against the rebels there is a safe rear beyond 80 km and the days are quiet. Against the neutral the nights are.
Against NATO after the rollback nothing is safe: moves are made at night and under nets, rest is taken in shifts, and
maintenance is done in short windows.

## 2. Doctrine by opponent

Our side changes its doctrine to fit the enemy. A **doctrine profile** is data (`data/campaign/doctrine.json`), one per
adversary:

| Item | What it sets | Used by |
|---|---|---|
| Readiness by hour | Readiness No. 1 / 2 / 3 for each part of the day | The order (`03` §2), the roster (`06` §4) |
| Emission | When the search radar may radiate; bursts; silence on the PU-12M's designations; the ARM drill (E6) | The order, the crew's drills |
| Moves | How often; by day or only at night; to reserve positions or new areas | The regiment's tasking (`03` §1) |
| Deception | Dummies and their decoy emitters' mode; dummy radio traffic | The battery commander's planner |
| What to cover first | Headquarters and troops at night, bridges, radars, the battery's own survival | The division's priorities (`02` §4) |
| Ambushes | How much of the regiment goes into ambushes, and on what evidence | The tasking rules |
| Rest and maintenance windows | Which hours are for what | The battery's schedule (`07` §5) |
| Radio and road discipline | Wire and runners first, radio only in a fight; columns by night | The orders, the march |

**Where each profile comes from.** Research step 21.0 looks for the period's operator documents on fighting each kind
of enemy: emission set by readiness level (`pvo_bu_ix_1984`); firing against ARMs with least emission
(`voenizdat_osa_1991` pp. 262-264); concealment against TDOA location (`vr_1985_plss`); duty launchers and TV search
against helicopters (pp. 262-266). The profiles are filled from what is found, as design readings marked as such:

| Adversary | The profile in outline |
|---|---|
| The rebels | Radiate freely (no ARMs, no ELINT); readiness No. 1 at night; cover headquarters and troops; ambush the light aircraft's routes; little need to move |
| The Noric Republic | Readiness No. 1 at dawn and dusk; nights for rest and maintenance; cover the troops and crossings near the border; ambushes once its habits show |
| The Warsaw Pact | Survive the air operation: silence, moves, dummies, the PU-12M's designations; cover the command posts and radars; then the troops |
| NATO | Strict emission control from the start; frequent moves by night; decoy emitters; fight silent; protect the warning radars; ambushes on the corridors the analysts name; above all survive, since a battery in being keeps NATO's SEAD busy and its rollback going (`01` §6) |

**Doctrine changes during a campaign** as the regiment's picture shows the enemy changing: the first ARM attack brings
the ARM drill (built, `docs/air_war/05_EXECUTION.md` §4); night raids bring readiness at night; a battery photographed
and struck brings more moves. How fast our officers change depends on their characters (`03` §1). A rigid chief of
staff keeps yesterday's doctrine one raid too long, as the enemy's rigid planners do.

## 3. Command under strain

Command between the layers (the division, the regiment, the battery, the vehicle) may or may not be intact in a fight.
When it is not, people improvise. When it can be, they rebuild it.

### 3.1 What breaks

| What | How, in the systems built or planned | What it stops |
|---|---|---|
| A post's holder | Killed or wounded where bombs fall on their place (the pipeline's places stand at theatre objects), or a vehicle hit | That person's decisions, and what only they knew |
| A command post | The division's CP and the army's HQ are theatre objects; the regiment's CP becomes one (13.2) | Orders, the briefing, the picture |
| The PU-12M | Hit, or its radios out | Designations to the launchers; the battery net |
| Wire | Cut for 6 h within 1.5 km of a raid (built, `docs/command/06_INTEGRATION.md` §1); a new position has none until it is laid (`07` §2) | Covert telephone traffic |
| Radio | Jammed (built for the nets; generated by packages in 16.2) | Fast traffic |
| Couriers and despatch riders | Lost on the roads in a raid's danger zone (built) | Papers, orders with overlays |
| A commander away | At the regiment's briefing when the raid comes (a routine) | Their presence; the deputy commands |

The pipeline already knows when a route fails (`docs/command/04_PIPELINE.md` §3). What is new is what people do about
it.

### 3.2 Improvisation

| Rule | Source |
|---|---|
| A battery takes readiness No. 1 on its own initiative when a raid is detected or ground combat begins | `pvo_bu_ix_1984` (`docs/research/CREW_COMMS_2026-10.md`) |
| Independent search and fire: with no designations, each launcher searches with its own radar in its sector of responsibility and fires | `pvo_bu_ix_1984`; the sectors in the order (`03` §2) |
| Act on the last order and its intent; report when contact returns | Design rule |
| The next means: wire, then radio, then a runner or a despatch rider; another net (the early-warning sources' fallback is built, 7.8) | `docs/command/03_MEANS.md`; `c2_sources.hpp` |
| The next senior takes the post: in a battery the first vehicle's commander, who is also the deputy battery commander; at the regiment, the alternate command post (ЗКП) under a deputy commander | `wrk` (the first vehicle's commander as deputy battery commander); `utin_2018` (the alternate post) |
| The observation post and the TV when the radars must stay silent | The chain's observation post; the TV (6.6, E5) |

**For NPCs** each commander keeps a contact clock for its superior and its subordinates. When nothing has come for the
doctrine's time, it acts by these rules in order, tries the next means, and says so. That makes improvisation part of
the action map like every other act (pillar 3). **For the player**, the helper shows what the post's NPC would do
(`docs/crew/14_HELPER.md`). The after-action loop judges it: the regiment's trust in the battery, and the career of the
person who decided (`05` §11).

### 3.3 Reconstitution

1. **Communications back**: wire relaid, the spare frequency or a new one, new call signs, the spare net.
2. **Posts refilled**: by the succession in §3.2, then by replacements from the reserve (P8; the air war's
   replacement after a wreck). A person who took over may keep the post.
3. **The alternate post takes over**, and the main post is rebuilt or moved.
4. **Units merged**: two damaged batteries become one; vehicles and crews are reassigned; the regiment re-tasks.
5. **New orders** confirm or replace what was improvised.

### 3.4 The player in it

The player's post may lose its superior (no orders: act on the last one), a subordinate (a launcher silent: send a
runner), or its holder. If the player's character is wounded, they go to the medical post and the player goes on as
another member of the battery (`06` §6). The player may also become the acting holder of a post above their own.

## 4. How they fit together

The enemy's reach sets the threat. Our estimate of it, from the reports, sets the doctrine. The doctrine sets the
orders and the battery's schedule. Command carries both down. When it breaks, people improvise by the same doctrine,
and rebuild.
