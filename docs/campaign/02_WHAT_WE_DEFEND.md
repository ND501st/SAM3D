# 2. Our side: what we defend, and why

Every object in the theatre (`data/air_war/theatre_pannonia.json`) exists for what it supports. That is what makes it
worth striking (`01` §3) and worth covering (`03` §1). This document gives each object its role and the consequence of
its loss, kept thin: links between objects, not an economy (decision B4).

## 1. Roles and consequences

Roles are design readings for the fictional country (game values, marked `"confidence": "estimate"` in the data).
The consequences act on systems that are already built.

| Object (theatre ids) | What it supports | If it is wrecked | Felt in the systems built |
|---|---|---|---|
| **The Kesmark road and rail bridges** (`kesmark_road_bridge`, `kesmark_rail_bridge`) | The southern group's supply over the Rava; trains to Kesmark station and the Horka siding (rail); the battery's own road between its area, the depot and the technical battery (road) | The supply goes over the other bridge, or by a ferry, slower; the TZM's and the ATZ's runs take longer; the regiment re-tasks a battery to the remaining crossing | The TZM and ATZ runs (`battery.hpp` `RequestService`); the pipeline's despatch riders and couriers on the roads |
| **The depots** (`kesmark_depot`, `southern_depot`, `sw_depot`) | Fuel and ammunition for the group in front of them; the ATZ fills there | The ATZ's run is longer or comes back with less; the battery's half-tank rule bites sooner | Fuel (7.4: 300 l, the half-tank rule) |
| **The technical battery** (Horka siding; a new theatre object) | The regiment's missiles: the TZMs reload from its stock | Reloads stop; the batteries fight with what is in their containers | The TZM's stock and the six containers (7.3, 6.3) |
| **The regiment's radars** (`kesmark_p40`, `ludas_p19`) | The regiment's air picture: what the PU-12M's board shows and what the battery is designated (`data/command/regiment_radars.json`) | The board holds only what the remaining radar covers; designations come late or not at all; the battery searches with its own radar, and radiates | `pu12.hpp`; the raid builder's `c2_fail` when no regiment radar stands |
| **The warning network** (`bugac_rtb`, `div_ad_cp`) | The division's early-warning net to the R-311 receivers; "Воздух" | Warnings come late or not at all; crews are caught at a lower readiness | `c2_sources.hpp`; the pipeline's air warnings (`docs/command/06_INTEGRATION.md` §1) |
| **The air bases** (`kecel_airbase`, `tiszavar_airbase`) | Our fighters, which the enemy must escort against (`friendly_fighters`) | Fewer of our fighters up: the enemy brings fewer escorts and more bombers | The planner's escort need (`planner.hpp`) |
| **The other air defences** (`kecel_s125`, `tiszavar_kub`, `varosd_s75`, `osa_22`, `strela_west`, `strela_south`, the ZU-23s at the bridges and Kecel) | Layered cover (§2) | A gap: corridors open where the enemy believed a threat. The enemy's believed share of working air defences falls, which ends NATO's rollback at 45 % | The planner's expected losses on a route; the phase change (`docs/air_war/04_PLANNING.md` §2) |
| **Command** (`army_hq`, `div_ad_cp`, `sw_brigade_cp`, the regiment's command post) | Orders and reports; the pipeline's routes; the evening briefing | Orders come later, by other routes; a cut wire sends calls to the radio, which the enemy hears | `command/info_net.hpp` (posts, wire, routes) |
| **Power** (`paksany_power`) | The grid: the works, the towns, and the posts that run on grid power | Posts go over to their diesels, which burn fuel; the works stop; towns go dark | A position's `position_power` (7.6); the news |
| **The rail junction** (`cegled_junction`) | The trains: missiles to the technical battery, fuel to the depots, replacements | Resupply slows for every unit behind it | The technical battery's and the depots' stocks |
| **The works** (`csepely_works`) | Repair of vehicles and radars | Wrecked units come back later | The replacement time (`repair_h`) |
| **The troops** (`group_south` and the other groups, `sw_border_battalion`, `sw_artillery`) | Holding the line | Losses reported up; the front moves only as the campaign's front data says (§3) | The enemy's close-support and helicopter candidates (within 40 and 25 km of the front) |
| **Towns** (`kesmark`, `novo_varosd` and the others) | The people | Civilian casualties: the enemy's own politics react (its `collateral_sensitivity`); our news reports them | The pipeline's news |
| **The rebuild means**: railway troops, engineers and their pontoon park, power line crews (new units); materials depots for spans, rails, cable and spares (new objects) | Rebuilding everything above (§3) | Rebuilds slow or stop where the teams, the materials or the way to the site are gone | Every object's repair; a battery's task to cover a rebuild (`03` §4) |

## 2. The layered defence battery 21 is part of

The air war's objects already carry their defences: radius, lethality, low and medium cover, radar or optical, rounds.
They form layers that work together. That is why each one matters beyond its own ring:

- **The high and medium layer** (the S-75 at Novo Varosd, the S-125 at Kecel, the Kub at Tiszavar) makes medium
  altitude expensive, so the enemy's planner, pricing the threats on each route, prefers to come in low.
- **The low layer** (the Osa batteries, the Strela-10 platoons, the guns at the bridges and the air base) is where low
  raids end up. An Osa battery covers the low approaches of what it defends, inside the high layer's umbrella.
- **The warning network and the regiment's radars** let every layer start warm, and let the PU-12M designate targets
  to launchers whose radars stay silent.

So a battery's task is never only "the bridges". It is the bridges' low approaches, in the sectors the other layers
leave open, with the warning the radars give. The order says so (`03` §2).

## 3. Rebuilding, and what stays thin

**Much can be rebuilt, at a price the enemy can strike too.** Industrial, civilian and logistic targets come back, but a
rebuild takes time, resources, logistics and manpower, and each of them is a target:

| A rebuild needs | In the model | How the enemy can stop it |
|---|---|---|
| **Manpower** | Teams of the right kind: railway troops for rail lines and rail bridges, engineers for road bridges and pontoons, power line crews for the grid, the works' fitters for equipment. Few, based somewhere, sent by priority | Strikes on the site while they work (the people there are hit, as people at any place are); strikes on their base |
| **Resources** | Materials of the right kind in a depot: bridge spans and pontoons, rails and sleepers, cable and transformers, spares | Strikes on the materials depot |
| **Logistics** | The way from the depot and the teams' base to the site over the transport links: roads, bridges, the rail line | Strikes on the bridge, the junction or the line in between: the rebuild waits for its own supply |
| **Time** | The job's hours with full teams and materials (today's `repair_h`); longer with less | Re-strikes "to prevent restoration" (`mai_tactics_vvs`) as soon as its reconnaissance sees the work progress |

- **Temporary fixes come first**: a ferry or a pontoon bridge in hours or days while a span is rebuilt in weeks; diesel
  sets while the grid is down; a bypass line round a wrecked junction. Each is an object that can be struck.
- **The priorities are set above the battery**: the division and the army decide which rebuild comes first (data per
  campaign), because teams and materials are short. The battery's part is to cover a rebuild when ordered (`03` §4).
- **Military units are replaced, not rebuilt**: a wrecked battery comes back from the reserve after its replacement
  time, as the air war already does it (`docs/air_war/06_ADAPTATION_POLITICS_LOGISTICS.md` §3). The rebuild model is for
  objects.
- **The air war's repair rule** (field repair at `1 / repair_h` an hour; a wreck out until its replacement comes, then
  back at `repair.restored_function`) becomes a job's progress with full means, slower or stopped with less.

**What stays thin** is everything else (decision B4):

- **No economy.** Each dependant has a supply share, the best of its supports' working shares along its alternatives
  (the southern group: the road bridge, or the rail bridge, or the ferry at a third). It is recomputed when an object
  changes, and the consequences in the table follow from it. Stocks exist only where a built system already counts
  them (the TZM's missiles, the battery's fuel, the technical battery's stock), where a rebuild needs them (the
  materials depots), and in one national stock: fuel, with its daily inflow from the pipelines and the refinery, because
  the oil decision makes it decisive in a war with the Pact (`10_BRIEFING_FORCES_AND_NATION.md` §3.1).
- **No ground war simulation.** The front is data per campaign: where the line is, when it moves, which troops are where
  (`data/campaign/<faction>.json`, read from the faction's phases: the neutral's three days of border defence, the Pact's
  offensive, NATO's land battle). A covered formation's moves come from it, and the battery follows them (`03` §4).
- **No national strategy.** The division's priority list per campaign is data. The regiment's tasking is a rule table
  (`03` §1). Neither is a game the player plays.

## 4. How the division and the regiment rank what to cover

The division gives the regiment a **priority list of objects to cover** for the campaign's front. It is data per
campaign: the battery position's note already says one ("Battery 21 covers the southern group's forward troops at the
front"). The regiment's staff then **bend it by what they see the enemy doing**. They read the enemy's aims from its
raids, as our side's records report them (`docs/command/01_PRINCIPLES.md` 9):

| What our side sees (from the reports) | What the regiment concludes | What it does with its batteries |
|---|---|---|
| DEAD on radars, hunters, decoy waves | The enemy is rolling back the air defences | Keep batteries alive and hidden: emission regimes, moves, dummies; cover the regiment's radars |
| Strikes on airfields, command posts and radars in massed blows | The Pact's air operation | The same, and cover the command posts |
| Strikes on bridges, junctions and depots | Interdiction | Cover the crossings and the technical battery's route |
| Close support and helicopters over the troops | Support of the front | Batteries forward behind the companies; duty launchers on the helicopter lines |
| Night harassment of headquarters and troops | The rebels' harassment | Cover the headquarters at night; ambushes on the light aircraft's routes |
| The same corridor four times (the analysts' pattern) | A habit | An ambush on it |

This is the "why" of every battery task. The order carries it in its reasons (`03` §2).

## 5. What the player changes

The player cannot change the course of the war: the enemy's aims and phases, the rest of the country's defence, and
which rebuilds come first are beyond one battery. Battery 21 changes two things, and the game measures both (`05` §14):

1. **The survival of what it covers**, and so the nation's ability to keep up the fight. A bridge kept standing keeps
   the southern group supplied; a rebuild covered to its end brings a crossing back; covered troops keep their
   strength. Measured as the working hours of the covered objects, the hits and releases prevented, the consequences
   that did not follow (§1), and the rebuilds finished under the battery's cover.
2. **The enemy's freedom of action**, limited by the battery surviving and threatening aircraft: the SEAD, decoys and
   jammers the enemy had to send, the ARMs it fired, the sorties it spent hunting the battery, the packages it did not
   fly or flew elsewhere, the aircraft it lost or had damaged, and the time it stayed in its suppression phase
   (`01` §6).

Both are told in the debrief without points, at the battery's own scale: what this battery kept standing, and what it
cost the enemy to work near it.
