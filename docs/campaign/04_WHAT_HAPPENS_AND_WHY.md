# 4. Around the battery: what happens, and why

Everything that happens around battery 21 has a cause in someone's plan: an enemy officer's (`01`) or our regiment's
(`03` §1). Each table here gives who does it, why (traced to the doctrine and the officers' logic as built), what the
crew sees on its instruments, and what the battery can do about it. The package compositions are the faction files'
templates (`data/air_war/faction_*.json`, `doctrine.packages`). The enemy's methods are as built
(`docs/air_war/04_PLANNING.md` §5, `05_EXECUTION.md`).

## 1. Attacked

| What the enemy attacks | With | Why | What the crew sees | What the battery can do |
|---|---|---|---|---|
| **The object the battery covers** | Strikes: the Pact's 2 × 4 with SEAD (one per believed radar threat on the route), escorts, a jammer and a reconnaissance lead; NATO's 2 × 4 with SEAD, escorts, a jammer and chaff; the neutral's four with an escort; the rebels' pair at night | Its worth: the kind's weight in the faction's phase, what its loss would cut, the grudge (`01` §3) | The warning; the PU-12M's designations; the formation on the scope; chaff and jamming | Fight it in the sectors the order gave; meet the low ones in the launch zone (Table 7.1); the SEAD first if it comes first |
| **The battery itself** | DEAD: four aircraft with SEAD, a jammer, and for NATO four decoy drones ahead; hunter pairs at medium height, loitering | It is located within 4 km (ELINT, photographs, launch sightings); NATO's rollback until it believes under 45 % of our air defences work; the Pact's weight on radars; a grudge after its losses | Decoys first (4 min ahead), SEAD 3 min ahead, the signs of an ARM launch on the scope, then the strike | The TV check against decoys (E6); the ARM drill; fight from the PU-12M's designations with the radars silent; dummies; relocate after the raid |
| **The warning radars and the PU-12M** | DEAD on warning radars; strikes on command posts | DEAD candidates include warning radars; the Pact weights command posts 1.3 and radars 1.2; blinding the board and the warning helps every later package | The board falls silent where a radar went down; "Воздух" comes late | Search with the battery's own radar, and radiate; cover the regiment's radars if the order says so |
| **The covered troops** | Close support (the Pact's Su-25s and NATO's A-10s with some SEAD; the neutral's pairs); helicopter raids (pop-up from a hide 6 km short) | The phase's support of the front; troops within 40 km of the front (close support) and 25 km (helicopters) | Low, slow marks in the clutter; helicopters popping up and dropping back | Duty launchers on the helicopter lines (45-60 deg sectors); the TV search with the screen's bottom edge on the crests; the first missile within 6.5 km (p262-266) |
| **Headquarters and troops at night** (the rebels) | Pairs of An-2s and light aircraft, low | Harassment, supply and propaganda; headquarters weight 1.4, troops 1.3 | Slow low marks at night; no jamming, no SEAD | Cover the headquarters at night; ambush the light aircraft's routes |
| **A rebuild and its means** | Strikes on a repair site, a pontoon bridge, a materials depot, the line that brings them | Re-strikes "to prevent restoration" once its reconnaissance sees the work (`mai_tactics_vvs`); the rebuild means' worth by the faction's weights for their kinds | People working in the open; a raid on a target wrecked days ago | Cover the rebuild when ordered (`03` §4) |

## 2. Defended

What we defend, and why, is `02`. The battery's part is the task in its order (`03` §2): the object, its low
approaches in the sectors the other layers leave open, and the battery's own survival, since a battery lost leaves the
object uncovered and lowers the share of working air defences the enemy believes in.

By surviving, the battery also limits the enemy's freedom of action (`01` §6). A battery that lives and threatens
costs the enemy SEAD, ARMs, sorties and routes, whether or not it fires.

## 3. Ambushed

| Who ambushes whom | How | Why it works, and when it stops working |
|---|---|---|
| **Our battery ambushes a corridor** | On the regiment's order: prepared points on a corridor the analysts named, radars silent until the PU-12M designates, fire, then leave or rove (`03` §4) | The enemy's planner prices only the threats it believes are on a route, so a silent battery it doesn't know of is not in its price. Its tactic memory makes a way that worked attractive again ("the same way in as last time"). A loss with no launch seen is put down to an "unidentified air defence" within 4 km; a launch seen locates the battery within 700 m. Then the memory makes that way costly and the grudge points at the battery. A rigid planner keeps coming (Linebacker II); a creature of habit is the analysts' best find; a pattern breaker never forms a pattern to ambush |
| **The enemy's hunters wait for our radars** | Hunter pairs loiter at medium height where a battery is suspected, with ARMs, firing on their warning receivers | Hunters are candidates against any radar air defence, located or not; methodical officers and the rollback raise their share |
| **The enemy provokes our radars** | Decoy waves (NATO's four drones ahead of its DEAD; doubled by a "decoy man"); demonstration groups (the Pact) | Deception against radar air defences: a radar that switches on to fight a decoy is located and attacked |
| **Feints and second strikes** | A pair from another entry that turns back short, 12 min ahead (the "feint artist"); a smaller second strike half an hour later (the "double tap") | Quirks of the planner (`docs/air_war/02_AIR_STAFF.md` §3): the first draws the warning and the battery's attention the wrong way; the second catches the repair, the reload, or a battery that has stood down |

## 4. Jammed

| What is jammed | By | Why | What the crew sees | What the crew does (built) |
|---|---|---|---|---|
| **The search radar** | A stand-off jammer orbiting on its own side, from 10 min before the strike to after: NATO's EF-111 class (`nato_alq99_band8`, in the search radar's band), the Pact's Yak-28PP class (`generic_soj_barrage_c`) | One jammer in each Pact and NATO strike and DEAD package, to hide the package from the radar threats believed on its route | Strobes on the PPI along the jammer's bearing; echoes lost in it | Burn-through at shorter range; the strongest beam and the gain down to narrow the strobe; the PU-12M's designations; the TV (9.1, E3) |
| **The tracking radar** | Self-protection noise pods (`generic_selfscreen_noise_ku`: the Pact's Su-24s and MiG-27s, NATO's strike aircraft and Weasels); range-gate pull-off repeaters (`generic_rgpo_leading_ku`, in the EM library and `osa_rgpo_em.json`, on no faction's aircraft yet) | The aircraft's own pods | No range in the gate; the gate walked off the echo | Radar on the jammer with the range by hand, ПОМЕХА and method Н (9.1); УВ. ПОМЕХА against a pull-off (9.4); the TV and ПОЛУАВТОМАТ |
| **The echo** | Chaff (NATO's strike packages) | The template's chaff | Clutter that drifts with the wind | СДЦ, external coherence (E0) |
| **The voice nets** (the battery net, the regiment's net, the PU-12M's designations) | A barrage jammer on the nets' band. Built as a hand-written scenario (`osa_vhf_jamming_em.json`); **not generated by the air war yet** | To cut designations and warnings: the board's picture reaches the launchers late or not at all | The headset's noise; the set's carrier quieting gone; read-backs missing | The spare frequency, an ordered change of frequency, another net (the EWR's, below the band), a runner (`radio_nets.hpp`, 7.8, 8.13) |

The rebels and the neutral neighbour have no jammers and no SEAD (their files). Our side does not jam. Its answer is
deception (§6).

## 5. Reconnoitred

| Who looks at whom | How | Why | What the battery can do |
|---|---|---|---|
| **The enemy's reconnaissance aircraft** | The Pact's MiG-25RB and Su-17 at any height; NATO's RF-4C with some SEAD; the neutral's Saab 105 and Alouette III; the rebels' single low aircraft by day; BDA aircraft after strikes | Mobile entries seen more than 8 h ago or poorly located; strikes in the last 30 h; an "imagery man" waits for photographs before he strikes a mobile target; the Pact sends a reconnaissance lead 8 min ahead of its strikes | Shoot it down before it gets home: the next strike is planned on old photographs, or not at all; nets halve the chance it sees the battery; move after it has passed (the regiment's order) |
| **The enemy's ELINT** | Every second a radar radiates is a fix: accuracy `elint_best_m + elint_k_m / sqrt(seconds)`, within its reach (the neutral's border station 40 km, the Pact's 150 km, NATO's 250 km) | The intelligence picture for DEAD and hunters | Emission regimes: bursts, silence on the PU-12M's designations; decoy emitters on the dummies |
| **The enemy's radio intercept** | Every radio call the battery makes (P3: the pipeline's `RadioCalls`) | Locating the battery by its traffic | Radio discipline: the march's flags and lamps, the wire, runners; dummy traffic |
| **The enemy's satellites** | The Pact's pass at 10:30 (film back in 36 h); NATO's at 09:30 and 21:00 (2 h); cloud blocks them, and poor weather stops them | Fixed objects, and mobile ones by chance (halved under nets) | Nets; not standing in the open at pass times (a design choice: no source yet for units warned of passes, `docs/research/ENEMY_AIR_RESEARCH_2026-10.md` §7) |
| **The enemy's agents** | Reports on what is near the front, late and sometimes false (the rebels' sympathisers reach everywhere) | | Little: moving makes their reports stale |
| **Our observation** | The regiment's radars, the warning net, the observation post on the Vrbice church tower, the villages and the police | The board's picture; warnings; parachutes and wrecks | Keep them working; the observation post when the radars are silent |
| **Our battery reconnoitres its positions** | The battery commander's reconnaissance step (`03` §3) | Ground with a field of view, nothing tall near a launcher, reserve positions | |
| **Our analysts** | The regiment's and the division's, on the batteries' reports (compiled from their records, P9) | The enemy's habits, corridors and timings; what the enemy knows of each battery | Keep the records well: the radiotelephonist's journal is what they read |

## 6. Deceived

| Who deceives whom | How | What follows |
|---|---|---|
| **Our dummies** | Dummy positions with decoy emitters on the search radar's parameters, in the posture's mode (`when_radar_off`, `with_radar`, `always`) (7.6, E7) | An ELINT fix may land on a dummy, or between it and the battery (`intel.ghost_fix_chance`). A DEAD strike on a dummy is claimed as a kill. The enemy then believes our air defences weaker than they are. NATO leaves its rollback when it believes under 45 % still work, so its interdiction may come to the bridges sooner. A battery that survives by deception changes what its object faces |
| **Switching off under ARM attack** | The ARM drill: every transmitter off once the missile in the air has burst | An ARM without memory misses; its pilots claim the radar destroyed; the "destroyed" battery fights again; the enemy's staff lose face and its intelligence officer grows sceptical (`docs/air_war/03_INTELLIGENCE.md` §5) |
| **Dummy radio traffic** | Empty messages on the net (`em/deception.hpp`) | Ghost fixes for the enemy's intercept service |
| **The enemy's decoys and feints** | §3 | Our missiles spent on drones; the warning and attention drawn the wrong way |

## 7. What battery 21 meets, adversary by adversary

| Adversary | What comes to the battery | What the battery's tasks are most often |
|---|---|---|
| The rebels | Pairs of light aircraft and helicopters at night, dawn and dusk, low; a single reconnaissance aircraft by day; no SEAD, no jamming; they keep away from known missiles (`docs/air_war/09_BALANCE.md` §5) | Cover headquarters and troops at night; ambushes on their routes, since a known battery only pushes them elsewhere |
| The Noric Republic | Low strikes of four with an escort at first and last light; close support and helicopter raids along the line; a costly first contact, then caution | Cover the troops and the crossings near the border; ambushes once its habits show |
| The Warsaw Pact | Two days of massed, timed blows with suppression groups, jammers and reconnaissance leads; DEAD on located batteries; hunters; then Su-25s over the troops | Survive the air operation: silence, moves, dummies; cover the command posts and radars; then cover the troops |
| NATO | DEAD with decoys first, hunter pairs, a stand-off jammer in the search band, RF-4Cs; then interdiction of the bridges and junctions; then A-10s over the land battle | Survive the rollback by deception and discipline; then cover the crossings and the technical battery's route; then the troops |

Each adversary's reach (`08` §1) decides when the battery can rest and maintain (`07` §7), and our doctrine against it
(`08` §2) decides how it fights and hides.

## 8. Rebuilt

| What | Who rebuilds it, with what | What can stop it |
|---|---|---|
| Bridges | Engineers and railway troops with spans and pontoons; a ferry or a pontoon bridge first | Strikes on the site, the pontoon park, the materials depot, the road in |
| Rail lines and junctions | Railway troops with rails and sleepers; a bypass line first | The same, and the line that brings the materials |
| The grid | Power line crews with cable and transformers; diesel sets meanwhile | Strikes on the substation being rebuilt, the crews' base, the stores |
| Depots, works, command posts | The works' fitters and the engineers, with their spares | Re-strikes once the enemy sees them working |

Each rebuild is a job with its teams, materials, transport and time (`02` §3). The enemy sees it through its
reconnaissance and strikes it, or the means behind it, by its doctrine. The battery's part is the order to cover one
(`03` §4).
