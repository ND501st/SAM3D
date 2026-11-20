# The fronts: the air front, the ground war and our air defence strategy (track F)

Written 20 November 2026 (F0) at the project owner's request of 19 November. This folder is the design for track F:
fronts that move by the units on them. The enemy's air front moves forward by sector when its staff judges our air
defence there broken, and back with losses. The ground war's units make their own front line, attack, hold, delay and
withdraw by their officers' logic. Our national air defence sets its sectors' priorities from both, and the regiment's
tasking and the battery's task follow. The research is `docs/research/FRONTS_RESEARCH_2026-11.md` and its four topic
files in `docs/research/fronts_2026-11/`. The tasks are roadmap track F (`IMPLEMENTATION_PLAN.md`).

## The brief (project owner, 19 November 2026)

1. **The map is the only source of truth for where things are.** Every placed object takes its place from the map by
   id. Shifting a factory by 2 km on the map moves everything anchored to it.
2. **The enemy's air front moves.** It starts at the border. When the enemy assesses that our air defence coverage in a
   sector is significantly degraded, and that enough of our mobile ground assets there are destroyed, it may move its
   air front forward in that sector, if it judges the risk low and the reward high. Low-level attack aircraft and
   helicopters matter most.
3. **The ground war moves.** A unit that loses too many of its tactical assets may retreat, dispersed or as a unit. One
   that is not taking many losses attacks and moves the front forward. Its own officers decide, as the air officers
   do. The ground situation feeds our national air defence strategy: where to defend, where to take the offence, and in
   which sectors.

The owner's priority for the track is **efficiency**: whole wars headless at campaign speed, built on what exists, in
tasks small enough for one agent and one review.

## What stays

- **The war's aims** (`docs/campaign/01_WAR_AIMS_AND_TARGETS.md`) and **one enemy at a time**.
- **First person.** The player's character learns where the front is only through reports that reach him
  (`docs/command/01_PRINCIPLES.md`). No map of the truth, no omniscient front line.
- **The written record is the war's memory.** Our side's decisions on the front (the AD commanders', the regiment's)
  are made on the situation reports its posts hold, never on the truth.
- **The player changes the war only a little**: by the assets kept and the enemy's freedom of action denied. A battery
  that survives in a sector is part of the coverage the enemy weighs before it moves its air front forward there.
- **1985.** Doctrine, norms and equipment as of the war's year.
- **The air war as built** (`docs/air_war/`, `docs/air_battle/`): its officers, intelligence picture, planner, routes,
  C2 and balance bands. Track F adds inputs to them and changes three fixed depths into per-sector ones.

This changes one earlier principle: "the strategy layer stays thin ... the front as data" (`docs/campaign/README.md`
B4). The front now moves by the units on it. The rest of B4 stands: no economy, no national strategy game, and the
regiment's tasking a rule table.

## Decisions

| # | Decision | Why |
|---|---|---|
| 1 | **The front is one line per front, kept as offsets along its base line.** Stations every 5 km along the theatre front's line (`FrontDef::line`, anchored to the map by F1). Each station holds how far the line of contact has moved into our country. Sectors are runs of stations, named in the theatre file. At offset zero the line is the base line, exactly | One small array per front; distance and sector lookups stay cheap; nothing changes until something moves it |
| 2 | **The air front is a depth per sector beyond the line of contact**, one for fixed-wing low-level aircraft and one a step behind it for helicopters. It starts at zero: at the border while the line of contact is the border. The planner's fixed depths (close support 40 km, helicopters 25 km, harassment 70 km) become these plus the sector's air front | The brief, point 2; today's numbers are its starting values, so the balance starts where it is |
| 3 | **The enemy's staff moves its air front on its own picture**: believed working radar SAM fire units over a strip, believed mobile air defence destroyed, the believed value beyond, its loss rate in the sector, and its commander's traits. Never the truth | The air war's rule (`docs/air_war/03_INTELLIGENCE.md`); the cases decided locally (`air_ground_cases.md` f-47) |
| 4 | **The ground war is a gameplay-level model with period norms, not a simulation of fire.** Each contested sector is resolved by rule tables: strength by weighted equipment, Dupuy's posture factors, FM 101-10-1's daily losses, HERO's advance rates by opposition, rivers and ground from the country map | The research gives tables, not physics; the result is the same at a fraction of the cost |
| 5 | **Units at the detail their distance needs: U0, U1, U2.** U0: divisions and brigades as actors far from the player. U1: regiments and battalions in the player's sector. U2: battalions and companies as route-service movers in a session within visual range. Up at once, down with a margin and a delay, keeping strength and place | Track B's A0-A2 and track G's M0-M2 |
| 6 | **Officers decide by doctrine tables moved by traits.** Our side and the Pact: withdrawal and counterattack only on the senior's order, effectiveness lines at 2/3 and 1/2. NATO: 3:1 and 6:1 as its decision rule, breakpoints drawn from ranges. Traits move a threshold by at most about ±0.2 of its range, as track B's officers do | `pact_ground.md`, `our_side.md`, `nato_ground.md`; `docs/air_battle/README.md` decision 9 |
| 7 | **The air war follows the units.** A `Troops` theatre object takes its place and working share from its ground unit; the enemy's strikes on it come off the unit's strength; supply lines end where their troops are; NATO's FSCL is drawn from the line of contact | The brief, F3's row: "the air war's targets, close support and interdiction follow the units" |
| 8 | **Our air defence strategy is a rule table run by our AD commanders on the reports they hold.** Sector priority (defend, take the offence, economy), whole-unit moves of national SAM units between prepared positions, fighters by sector, the regiment's new triggers. Orders go through the pipeline; no unit moves on an order its commander did not hear | `our_side.md`; track G's rule; the written record |
| 9 | **The front reaches the player as situation reports.** Units report on their schedule and urgently on a breakthrough; the reports are facts in the pipeline, written into the posts' records, drawn on the situation maps the player's post holds | First person; `docs/command/` |
| 10 | **New behaviour behind save modes**: `air_front.mode`, `ground.mode` and `ad_strategy.mode` in `data/gameplay/air_war.json`, written into the save; a save without them replays at 0, as it was played. The ground war runs only on a theatre with a country map | Tracks B and R's way (`simcore/air_war/save.hpp`); the tuned district and its tests stay as they are |

Decisions 4 and 5 answer the owner's priority. The owner's open choices are listed at the end.

## Efficiency

The ground war must be cheap enough to run a whole nation headless at campaign speed. `air_war_balance` runs 20 seeds
of whole campaigns for every adversary and two battery postures; it must not get noticeably slower.

### The budget

| Part | Budget | Measured by |
|---|---|---|
| The ground war, whole nation, all at U0, one faction's war | **20 ms CPU per campaign day** (mean over 30 days and 5 seeds), the worst day under 60 ms | `fronts_bench data --days 30 --seeds 5` (new, F3.3), per part: combat, decisions, movement, line, picture, reports |
| The player's sector at U1 on top | 15 ms more per campaign day | `fronts_bench --player-sector` |
| The air front (F2) | 1 ms per planning cycle for a front | `fronts_bench --air-front`; the planner's own cycle as before |
| Our AD strategy (F4) | 5 ms per campaign day | `fronts_bench --ad` |
| In a session | the ground war's share of a session step 0.05 ms mean (the `Nav` part of `StepProfile`) | `air_battle_bench data --testbed 600` and `mission_demo --war ... --hours 24` |
| The whole campaign | `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` for each faction: CPU time with every F mode on at most **1.05 times** with them off, same seeds | the tool's new `--cpu` line (F3.3) |
| A save's load | the replay of a 30-day campaign at most 1 s longer | `fronts_bench --replay` (trend, not a gate) |

How the numbers sit: the air war's planner alone took about 35 Gcycles for two days of NATO's war on the nation
(`docs/air_battle/01_ROUTES.md`, the cost of route.mode 1), several seconds a campaign day. Twenty milliseconds is well
under 1 % of that, so the relative gate is met with room; the absolute budget is the one that holds the design to
account. The counts behind it: a faction's ground war on the nation is about 10-15 formations a side at U0 (NATO's
corps of three divisions and a cavalry regiment, the Pact's army of four divisions, our 3rd Army's two divisions and
four brigades) across 7-10 sectors a front. At most 10 contested sectors are resolved an hour and about 5 commanders
decide in it: some hundreds of table lookups an hour.

**How it is measured.** CPU time (`std::clock`), not wall time: track B learnt that wall times move by half with the
machine's other load (`docs/air_battle/01_ROUTES.md`, B2 as built: the cost). Parts are timed with `std::chrono::steady_clock` as
`step_profile.hpp` does. The same seeds before and after. `fronts_bench` prints a table to paste into the as-built
notes; `verify.ps1` runs it as a trend, and `fronts_bench --gate` fails above the budget (F6).

### How the design keeps to it

1. **Aggregate far away.** At U0 the actor is the division or the brigade, holding its regiments as a list with
   strengths. Nothing below it moves or decides. In the player's sector (U1) the regiments and battalions are actors;
   in a session (U2) they are columns on the roads, drawn in Unreal.
2. **Coarse time, driven by events.** The ground war steps with the campaign's hour (`AirCampaign::StepTo`), never
   faster. Each commander decides at his next decision time, from FM 100-2-1's reaction times (division 2-4 h,
   regiment 1-3 h, battalion 25-60 min), or at once on an event (contact, a breakthrough, an order). A queue keyed by
   time holds them, so a quiet hour costs a constant.
3. **The sector is the unit of combat.** One resolution per contested sector per hour, by table. A quiet sector takes
   the "inactive" loss rate in closed form when it is next touched, not hour by hour.
4. **The line is an array.** Station offsets, not polygons. The distance from a point to the line of contact and its
   sector are a projection on the base line and a lookup.
5. **Rule tables, not simulation, where the result is the same**: losses, advance, breakpoints, crossing times.
6. **Routes only on orders, cached.** U0 formations stand by posture relative to the line (their echelon depths) and
   advance with it; only a regrouping between sectors and the reserve's march ask the route service
   (`nav::RoadGraph::Find`), once per order, cached by the pair of anchors and the overlay's version. U1 units ask it
   too, within the sector. Bridges down are the route service's overlays.
7. **Keyed draws.** Every draw is a fork keyed by (sector, day) or (unit, decision time), never the campaign's main
   stream, so the ground war neither costs the air war's sequence nor depends on order.
8. **The pipeline only for what a post holds.** Far formations' internal orders wait out a delay table; only the
   reports and orders a post of our chain would hold become facts (F3.8), a few dozen a day for the nation.
9. **Off costs nothing.** At mode 0, or on a theatre without a country map (the tuned district), nothing is built and
   no draw is made. The air front alone (F2) works on any theatre.

### Deferred for cost

- Fire by direct-fire weapons in a session (tanks and guns shooting): the session shows the units; their combat stays
  the sector's table.
- Encirclement and pockets: a unit bypassed holds or breaks by its breakpoint; no pocket geometry.
- U1 everywhere at once (a "detailed nation"): only the player's sector, and each player's in multiplayer.
- Our own air force's attacks on the enemy's ground units (our side's CAS): out of the track.

## Reuse: what each part extends

| Part | Builds on |
|---|---|
| Sectors and the line of contact | `airwar::FrontDef` (`line`, `entries`), `AirCampaign::DistanceToFront`, `DistanceToLine` (`air_war/types.hpp`), the theatre loader `LoadFront` (`air_war/data.hpp`) |
| The air front | `IntelPicture` and `IntelEntry` (`air_war/intel.hpp`: `IsSamRadar`, `Live`, `BelievedIadsShare`), `AirStaff` and `Officer` (`officers.hpp`), `AirCampaign::Plan` and the phases' `until_iads`, the planner's target filter (`planner.hpp`, the `MissionKind` switch), `RouteRules` and the route planner's gates and helicopter holding areas (`route_planner.hpp`), the c2 game values (`c2.bai_depth_m`) |
| The ground war | `AirCampaign::StepTo` (the hour), `ObjectState` (`state.hpp`: `pos`, `function`, `supply`), `ObjectKind::Troops` objects, `SupplyNetwork` (`logistics.hpp`), `command::OrderOfBattle` (`oob.hpp`, for our units and, in the same format, the enemy's), `airwar::Traits`, `OfficerDef`, `OfficerPoolDef` (`data.hpp`), `CountryMap` (`InForest`, `DistanceToRiver`, `NearestCrossing`, `RegionAt`), `nav::RoadGraph`, `nav::Overlay`, `nav::Movers` and `MoverLevel` |
| Our AD strategy | the chain's positions (`army_ad_chief`, `div_ad_chief`, `sector_cp_duty`, `rgt_commander` in `chain_pannonia.json`), `command::InfoNet` facts, `AirCampaign::Relocate` and `TheatreObject::prepared`, `FrontDef::friendly_fighters` and the fighter regiments of `air_combat.json` `"ours"`, `campaign::RegimentTasking` and `TaskingRules` (`regiment.hpp`), `BatteryTask` |
| What the player sees | `command::CampaignFeed::Pump` (`air_war_feed.hpp`), `FactKind`, `PostView` (`post_view.hpp`), `nav::SessionNav` and its movers, `FSimBasicModels`, `ASimCampaignHUD`, `tools/map_view.py` |
| Balance | `air_war_balance`, `balance_bands.hpp` and `data/air_war/balance_bands.json`, `air_war_bench`, `campaign/outcomes.hpp` |

## Save compatibility

As tracks B and R did (`simcore/air_war/save.hpp`):
- three tuning keys in `data/gameplay/air_war.json`: `air_front.mode` (F2), `ground.mode` (F3) and `ad_strategy.mode`
  (F4), each 1 for new campaigns once its task is merged;
- `SaveCampaign` writes them as `air_front_mode`, `ground_mode` and `ad_strategy_mode`; a save without one loads it
  as 0 and replays to the bit as it was played;
- at 0 nothing is built and no draw is made; the planner's fixed depths and `Relocate`'s timer stay as they are;
- the ground war adds no player action: it is a function of the seed, the data and the recorded actions, so a load
  rebuilds it by the replay;
- a later change to a merged mode's behaviour takes the next number (as `opportunity.mode` went to 3).

## The loop

```
 the enemy's war aims ──► its ground plan (objectives, axes)      our war aims ──► our defence (ČSLA 1985 norms)
              │                                                                 │
              ▼                                                                 ▼
   enemy formations (U0/U1) ── contact by sector, hourly ──► sector resolution (tables) ◄── our formations (U0/U1)
              │                     losses, advance, rivers, ground                            │
              │                                  │                                             │
              │                    line of contact (station offsets) ◄─────────────────────────┤
              │                                  │                                             │
   its commanders decide on its picture   ──────┤──────    our commanders decide on reports (withdraw, counterattack)
              │                                  │                                             │
              ▼                                  ▼                                             ▼
 air staff: the air front by sector (F2) ◄── believed SAM cover, losses        situation reports ──► pipeline (F3.8)
              │                                                                                │
 planner: CAS, helicopters, gates, FSCL ──► packages ──► our troops, columns, SAMs ──► losses ─┤
                                                                                               ▼
                                            our AD commanders: sector priorities, SAM moves, fighters (F4)
                                                                                               │
                                     regiment's tasking ──► battery 21's task ──► the player (F5: maps, units in sight)
```

## Contents

| File | What |
|---|---|
| `01_ANCHORS.md` | F1 in brief: the map as the source of truth (built in parallel; its design is in that branch) |
| `02_AIR_FRONT.md` | F2: sectors and the line of contact, the enemy's air front by sector, what moves it, what it changes in the planner and routes |
| `03_GROUND_WAR.md` | F3: units and levels, the sector's combat, commanders and their decisions, each side's picture, the air war following the units, the situation reports |
| `04_AIR_DEFENCE_STRATEGY.md` | F4: our AD commanders' sector priorities, SAM moves, fighters by sector, the regiment and the battery |
| `05_WHAT_THE_PLAYER_SEES.md` | F5: situation maps, units in sight in Unreal, the campaign screen from our knowledge |
| `06_BALANCE.md` | F6: the fronts against history, the air war's bands kept or argued, the player's bounded effect |
| `IMPLEMENTATION_PLAN.md` | The tasks F2.1-F6.2, what each touches, rests on and is tested by; the budget gates; milestone M-F |

## For the owner to decide

1. **The front's base line.** F2.1 uses each theatre front's own line (three or four points, anchored by F1). The
   country map has the real border with each neighbour (`CountryBorderDef`, many points). Basing the stations on it is
   more faithful and moves the balance a little (distances to the front change). Proposed: the theatre's line for F2-F3,
   the map's border argued in F6.
2. **The rebels' and the neutral's ground war.** The research gives only patterns: rebels without air defence cannot
   hold ground against an air force (`cia_1986_nicaragua_af`); the neutral's doctrine is dissuasion on its own
   territory (`sadauer_schlup_1978`). Proposed (`03_GROUND_WAR.md` §8): the rebels hold a ragged front and raid where
   we are thin, by night; the neutral makes a limited incursion to its political depth (40 km) and then holds. Both
   SAM3D estimates. Or keep both wars' fronts static and model only the Pact's and NATO's.
3. **Who starts the ground war.** NATO's and the Pact's 1985 doctrine was written for their own defence or for an
   offensive in Germany; an invasion of Pannonia is the game's fiction. Proposed: each faction's ground block sets its
   offensive's start day and axes (the Pact with its air operation, NATO after a few days of rollback). The owner may
   prefer a war that opens on the ground and in the air together for both.
4. **Our counterattack across the border.** The ČSLA's 1985 counterstrike aims include restoring the state border
   (`vr_svatos_1985`). Proposed: our side counterattacks to restore the border and stops there; no offensive into the
   enemy's country.
