# 2. The enemy's air front (F2)

The project owner's second point: the enemy's air front starts at the border. When the enemy assesses that our air
defence coverage in a sector is significantly degraded, and that enough of our mobile ground assets there are
destroyed, it may move its air front forward in that sector, if it judges the risk low and the reward high. Low-level
attack aircraft and helicopters matter most.

The research is `docs/research/fronts_2026-11/air_ground_cases.md` (the cases: the War of Attrition, Suez and the
Golan in 1973, the Bekaa in 1982, Iran-Iraq; the rebels' and the neutral's appetite) and the synthesis's §2.5.

## 1. What exists

- **One front line per front**, `FrontDef::line` (a few points along the border), with the raids' entry points
  (`entries`), the enemy's bases and the battery's position (`air_war/data.hpp`). The campaign measures depth from it:
  `AirCampaign::DistanceToFront` (HUMINT's reach, where a relocating unit may go), `DistanceToLine` in the planner.
- **Three fixed depths** in the planner's target filter (`planner.hpp`, the `MissionKind` switch): close air support
  only on `Troops` within 40 km of the line, helicopter raids within 25 km, harassment within 70 km; the politics'
  `max_depth_m` on top. A strike within `c2.bai_depth_m` (30 km) is battlefield interdiction to the C2 layer
  (`c2.hpp`), and `NominationWaits` holds such targets for the corps' nomination.
- **Whole-country gates.** A phase ends on `until_iads`, the believed working share of all our radar air defences
  (`IntelPicture::BelievedIadsShare`). NATO's rollback ends at 45 %.
- **Routes** (track B2, `route_planner.hpp`): gates and corridors where the raids cross, helicopters' holding areas
  `route.helo_holding_m` behind the line and the Pact's helicopters under their umbrella (`route.helo_umbrella_m`
  from an entry point).

So today the enemy flies low-level work to fixed depths from the border, everywhere, from the first hour, and the only
thing that changes with our losses is the whole country's phase. The cases say the period's decision was local, by
sector (`air_ground_cases.md` f-47).

## 2. Sectors, the line of contact and the default

**Sectors.** Each front is cut into sectors in the theatre's data: named runs of the front line's length, 20-60 km
wide (a sector narrower than 20 km cannot hold the 20 km strip the gate needs, §3). About 4-8 sectors a front. A
sector is given as distances along the line, so it stays valid when F1 anchors the line or the owner moves it.

```json
"sectors": [
  { "id": "north_hills", "name": "Northern hills", "from_km": 0, "to_km": 35 },
  { "id": "kesmark_gap", "name": "Kesmark gap", "from_km": 35, "to_km": 70 }
]
```

**The line of contact.** Stations every 5 km along the front's line. Each holds an offset: how far the line of contact
has moved into our country at that station, along the line's inward normal. F2 has only zero offsets; the ground war
(F3) moves them. At zero the line of contact is the front's line exactly, so every distance is bit-identical to today's.
Two lookups on it, each a projection on the base line and an interpolation of the offsets:
- `LineOfContact::DistanceTo(p)`: how far `p` is beyond the line of contact (negative on the enemy's side);
- `LineOfContact::SectorOf(p)`: the sector `p` is in, by its projection on the line.

**The air front.** Each sector holds two depths beyond its line of contact:
- `depth_m`, the fixed-wing front: how deep low-level attack aircraft work in the sector;
- `helo_depth_m`, the helicopters' front: always one step behind the fixed-wing front, never ahead of it.

Both start at 0: at the border while the line of contact is the border. That is the period's default, the air front
outside the belt (`werrell_2005` p. 151). When pulled back after heavy losses it goes back to 0 at most: we model no
"keep 16-24 km back from the belt" until the ground war gives a belt to keep back from.

## 3. What moves it: the decision

The enemy's air staff decides at its planning cycle (`AirCampaign::Plan`, every `planning_cycle_h`), sector by sector,
on its own picture (`IntelPicture`), never the truth. The officer who decides is the faction's air operations chief
(the `StaffRole` that plans, `officers.hpp`); his traits move the gates as track B's officers' do, by at most about
±0.2 of a gate's range.

**Risk: our cover over the next band.** The band is the sector's width by one step (20 km) beyond its air front. The
cover is the believed working share of the radar SAM fire units whose believed engagement rings
(`IntelPicture::Threats`) reach the band:

```
cover = sum over those fire units of  exists × (1 - 0.7 × suspected_decoy) × function
        ─────────────────────────────────────────────────────────────────────────────
        max(the same without function, the sector's pre-war baseline)
```

the formula of `BelievedIadsShare` restricted to the band, over a baseline: the sector's believed count at the war's
start, raised when more are found. A unit that pulls out of the sector lowers the cover as a destroyed one does. That
is the brief's "coverage significantly degraded".

**Risk: our mobile ground assets there.** The brief adds "enough of our mobile ground assets there destroyed". Read as:
the believed share of our mobile air defence in the band destroyed or out of action: the Osa, Kub and Strela batteries
and the AAA the enemy believes are there (`IntelEntry` of kinds `SamSite` with `mobile`, `Aaa`), the same formula
over their entries. The other reading, our troops' strength in the sector, is the ground war's (F3) and enters through
the reward and the crisis, not the gate. The owner may prefer otherwise (README, "For the owner to decide").

**The gates.**

| Faction | Opens when the cover is at most | and mobile air defence destroyed at least | Pulls back on a loss rate over 1-3 days above |
|---|---|---|---|
| NATO (superior) | 30 % | 50 % | 2 % a sortie |
| The Pact (peer) | 20 % (Krasovskiy's 60-80 % of the fire system neutralised; "destroyed" at 50 % of elements) | 50 % | 4 % a sortie |
| The rebels | where our mobile SAMs are believed absent (cover 0 %, mobile 100 %) | | 1 % a sortie |
| The neutral | never on its own: its air front stays at its border unless an event moves it | | 1 % |

All SAM3D estimates bounded by the cases (`FRONTS_RESEARCH_2026-11.md` §4), labelled so in the data and checked in F6.

**Reward.** The believed value of our targets in the next band that the sector's low-level work would reach: troops
(`Troops`), columns, bridges, command posts and supply nodes, each by its `value` and the faction's mission weights. A
sector with nothing worth flying for does not advance even when its cover is gone. The gate is a minimum reward per
step: a game value per faction.

**Delay and steps.** When the gates hold, the front advances one step (20 km) after a delay of 6-24 h from the
planning cycle that first saw them hold (`werrell_2005` p. 153: under 24 h from the first sites destroyed to full close
support). A methodical chief takes the long end; a bold one the short. It never advances more than one step a cycle. Its
greatest depth: NATO and the Pact 60 km, the rebels 40 km, and never beyond the politics' `max_depth_m` measured from the
border.

**Pull-back.** When the sector's loss rate over the last 1-3 days (aircraft lost to ground fire per low-level sortie
flown in the sector) is above the faction's line, the front goes back one step at the next cycle. A chief with high
composure waits a cycle longer; one under stress (`Officer::stress`) pulls back at once.

**Crisis.** Where the enemy's own ground front in the sector is collapsing (its ground picture, F3.5: a formation of
its own on an axis there believed below half strength, or our counterattack on an axis there, both as its units' reports
reached its staff), the chief may ignore the loss gate there for 1-3 days and
advance by a step, as the IAF did on the Golan in 1973 (`entwistle_1988`). Only for the Pact and NATO; the rebels and
the neutral break the helicopter rule only, once, in a crisis.

**Helicopters.** `helo_depth_m` follows the fixed-wing front a step behind, and only after a day of fixed-wing
low-level work in the sector without losses above the gate (`fm_100_2_1_1984` p. 7-1). The Pact's airmobile raids may
then go 20-30 km beyond the line of contact (`barbeau_1985_sams` p. 24). The holding areas stay at least 15-20 km behind
the enemy's own side of the line (artillery range).

**The rebels' and a small air force's surge.** The rebels advance only in a sector where our mobile SAMs are believed
absent, for 1-3 days at most, then cut back to 0 (`cia_1985_angola_offensive`, `cia_1986_angola_saaf`).

**Phases stay whole-country.** `until_iads` keeps ending NATO's rollback on the whole country's share. A rollback that
has not ended can still open sectors: the cases' "local first" (Suez, the Bekaa).

## 4. What it changes: the planner, the routes and C2

All measured from the sector's line of contact (zero offsets in F2: the border).

| Today | With `air_front.mode` 1 |
|---|---|
| Close support on troops within 40 km | within 40 km + the sector's `depth_m` |
| Helicopter raids within 25 km | within 25 km + the sector's `helo_depth_m` |
| Harassment within 70 km | within 70 km + the sector's `depth_m` |
| Gates and corridors at the front line, the routes' let-down to low level where they cross | the let-down where the route crosses the sector's air front; corridors still cross the line of contact at the entries |
| Helicopters' holding areas `route.helo_holding_m` behind the line | behind the sector's helicopter front, never closer than 15 km to our side of the line of contact |
| The Pact's helicopters within `route.helo_umbrella_m` of an entry point | within it of the sector's air front |
| BAI within `c2.bai_depth_m` of the line, nominations waiting | within it plus the sector's `depth_m` |
| The politics' `max_depth_m` from the line | unchanged: from the border, which is political |

The 40, 25 and 70 km stay what they are, so at depth 0 the planner is as balanced today. The step moves the
whole band. That is the brief's "low-level attack aircraft and helicopters matter most": high-level strikes and SEAD
are not gated by the air front, only by the threat their routes weigh (B2).

**NATO's FSCL** (`fm_101_5_1_1980` p. 1-49; `fm_6_20_1977` p. D-5; `ota_1987_fofa` pp. 78-79): close air support short of
it with detailed integration, battlefield interdiction either side, air interdiction beyond. With the ground war (F3.7)
the FSCL is drawn 20-30 km beyond the line of contact in defence, the next terrain line beyond the day's objective in
attack. Its only effect in F2 is the mission label C2 reads; the BAI depth above stands in until then. As built (F3.7,
`03_GROUND_WAR.md`, As built: F3.7): NATO's corps draws it by sector (`GroundWar::Fscl`, the doctrine's `"fscl"` in
`ground_nato.json`), and the BAI area's depth beyond the line of contact is the FSCL's there in place of `c2.bai_depth_m`
(`AirFrontView::BaiDepth`, read by `InBaiArea` and `NominationWaits`); the other factions draw none and keep the fixed
depth.

**J-SEAD by depth** (`davis_1987_31_initiatives` p. 31): the Army's guns suppress to the limit of observed fire, the
Air Force beyond. Not modelled until the ground war has artillery: the planner's SEAD stays the air force's.

## 5. What our side learns of it

The front moves in the enemy's mind; we see only its effects. Our side's posts learn it by indicators:
- low-level attacks and helicopters seen deeper in a sector than before (the warning posts' and the batteries'
  reports, `FactKind::RaidSeen`), counted per sector and day by the army's air defence staff;
- our intercept service's day summary when it heard close support control nets deeper (`FactKind::Intercept`);
- our troops' reports of air attack (F3.8).

The army's air defence chief's staff draws a conclusion ("the enemy's close support now works to 20 km in the Kesmark
gap") as a fact once the indicators pass a count, a game value, after the staff's working delay. That fact travels the
pipeline to the regiment and is what F4's sector priorities and the regiment's tasking read. Never the enemy's
`depth_m`.

## 6. Data

- The theatre's front: `"sectors"` (§2), one list per front; a theatre without it has one sector the length of the line,
  so the old theatres and the tests need no change.
- A faction's `"air_front"` block (`data/air_war/faction_<id>.json`): the gates, the loss line, the delay range, the
  greatest depth, whether the crisis override and the surge apply. Values labelled as SAM3D estimates with the cases
  that bound them.
- `data/gameplay/air_war.json`: `air_front.mode`, `air_front.step_m` (20 000), `air_front.station_m` (5 000), the reward
  gate per faction, our staff's indicator count.

## 7. Code

- `simcore/air_war/front_line.hpp` (new, F2.1): `SectorDef`, `LineOfContact` (stations, offsets, `DistanceTo`,
  `SectorOf`, `Offset`), built from a `FrontDef` and its sectors.
- `simcore/air_war/air_front.hpp` (new, F2.2-F2.3): `AirFrontDef` (the faction block), `AirFrontState` (per sector:
  `depth_m`, `helo_depth_m`, the baseline, the pending decision and its time, the loss window), `SectorCover` (the
  believed cover and mobile share over a band, from `IntelPicture`), `DecideAirFront` (the rules above, pure: picture,
  state, officer, time in; state out).
- `AirCampaign`: holds a `LineOfContact` and an `AirFrontState` per front; `Plan` calls `DecideAirFront` before the
  planner; the planner, the route planner and C2 read `AirCampaign::AirFrontAt(pos)` in place of the fixed depths when
  the mode is on. `DistanceToFront` reads the line of contact.
- The campaign's journal records each move (sector, depths, why, the officer) for the enemy's side of the debrief and
  `tools/air_war_view.py`.

## 8. Cost

The decision is per sector per planning cycle: one pass over the believed SAM entries (a few hundred on the nation) per
front, a loss window per sector, and table lookups. Budget 1 ms per planning cycle for a front (README). The planner's
reads are a projection and an interpolation per target: cheaper than today's `DistanceToLine` over the line's segments.

## 9. Tests

- **The default holds.** With `air_front.mode` 0, and with 1 and no sector moved, the planner's choices and a 7-day
  campaign's journal are bit-identical to before (an identity case in `test_air_war.cpp`).
- **A hole opens a sector.** A theatre with two sectors; every believed SAM in one destroyed: that sector advances one
  step after the delay, the other does not; close support appears deeper there only.
- **Losses pull it back.** A sector with losses above the line over two days steps back one step.
- **Helicopters follow.** The helicopter front never passes the fixed-wing one and lags by a day of clean work.
- **On belief, not truth.** A decoy believed working holds the gate; a real unit believed destroyed opens it.
- **Our side learns it late.** The army's staff states the deeper front only after enough raids are seen, through the
  pipeline.
- **Save.** A save without `air_front_mode` replays at 0.

## Done when

The enemy's air front moves by sector on its picture, the planner, routes and C2 read it, our staff learns of it by its
signs, it costs under its budget, and old saves replay unchanged. Its balance is F6's.

## As built (F2.1, F2.2; 20 November 2026)

**F2.1, the sectors and the line of contact** (`simcore/air_war/front_line.hpp`, `air_front_def.hpp`). A theatre front
takes `"sectors"`: `[{"id", "name", "from_km", "to_km"}]`, in order along its line from its first point, each starting
where the one before ends (`AirWarLoader::LoadSectors`, a separate function beside `LoadFront`; a gap, an overlap, a
repeated id or an empty list fails). The last may end short of the line or past it: the line's ends belong to the end
sectors. A front without them is one sector, `"front"`, the line's length. `LineOfContact::Build(front, station_m)`
keeps the base line (the theatre front's own line, the owner's decision 1 as proposed), its sectors, stations every
`air_front.station_m` (5 km; the last at the line's end) and an offset per station, all zero. Our side is the side away
from the front's `bases`. The lookups: `Place(p)` (the nearest point on the base line, found as `DistanceToLine` finds it,
its distance along the line and the side), `DistanceTo(p)` (signed, positive beyond the line of contact into our
country: the side times the distance, less the offset interpolated there), `AbsDistance(p)` (either side; with no
station moved it is `DistanceToLine(base line, p)` itself), `SectorOf(p)`, `SectorAt(along)`, `Offset(along)`,
`ContactPoint(station)`, `InwardNormal(along)`, and `SetStationOffset`, `Moved` and `Version` for F3.3. With offsets the
signed distance is the projection's less the offset at it: exact on a straight line, close on a front's gentle bends.
`AirCampaign` builds its front's line at `Init` and holds it (`FrontLine()`); `DistanceToFront` (HUMINT's reach, where a
relocating unit may go) reads `AbsDistance`, and the planner's target filter reads it through
`PlanContext::front_line` (the campaign's plan and its on-call builder; a context without one reads the base line).

**The theatres' sectors.** Every theatre front has its sectors, named on the country map's features along its line (both
theatres, the same on the nation's Pact and NATO fronts; the line's points are F1's places, read through the loaded
`FrontDef::line`, never from the file's coordinates):

| Front | Line | Sectors (km along the line from its first point) |
|---|---|---|
| The Pact (north-east) | 214.5 km | the Hradec Dolni approach 0-50, the northern forest 50-100, the Rava reservoir 100-140, the Tisava valley 140-180, the north-eastern crossings 180-215 |
| NATO (west) | 180.7 km | the western uplands 0-50, the Zlatno gap 50-90, the western crossings 90-125, the upper Lenta 125-181 |
| The rebels (south) | 150.8 km | the Hollos approach 0-45, the Ralje approach 45-80, the Rava crossing 80-110, the rebels' forest 110-151 |
| The neutral (south-west) | 148.8 km | the Lenta valley 0-35, the border forest 35-70, the lower Lenta's marshes 70-110, the southern border 110-149 |

Game values (SAM3D's cut, 30-50 km each so a sector holds the 20 km strip the gate needs); nothing reads them at
`air_front.mode` 0.

So far only these two read it. `c2.hpp`'s four reads (`C2MissionOf`'s BAI depth, `FreeHunt`, the post-fixing fires'
reach, `MissionName`) are free functions or the C2 layer's own and reach the front through `AirWarData`; they move to the
line of contact with F2.4, which changes that depth by the sector's air front. `resolve.hpp`, `raid_scenario.hpp` and
`logistics.hpp` read the base line for a depth in a report or a supply line's front end: F3.3 and F3.7. All equal it at
zero offsets. (Done: `resolve.hpp`'s with F3.3; `raid_scenario.hpp`'s air-fight front point and `logistics.hpp`'s patrol
point read the line of contact with F3.7.)

**F2.2, what the enemy weighs by sector** (`simcore/air_war/air_front.hpp`). `SectorCoverBook` keeps the enemy's picture
at the war's start (every radar SAM fire unit and every piece of mobile air defence it believed in, by place, believed
ring and `IntelEntry::BelievedThere`, the weight `BelievedIadsShare` gives a site), and `Measure(picture, line, front
depths, band)` gives a `SectorCover` per sector over its band, from its air front to `air_front.step_m` (20 km) beyond: the
cover (§3's formula), the counts, the baseline, and the mobile air defence's share destroyed or out of action. A ring
reaches a band when its centre, placed by its distance along the line and its depth beyond the line of contact, is
within its radius of the sector's run by the band: one placement per entry a measure, tested against every sector. A
band's baseline is the pre-war picture's sum over it, raised when more are found there; a new band (the air front
moved) or a moved line takes it afresh from the pre-war picture; the pre-war units are placed on the line once for
every sector and kept until the line changes (`LineOfContact::Version`, unique among lines: a copy that moves gets its
own). A measure allocates nothing once the campaign's vectors are sized. Nothing believed there, ever: cover 0 and mobile air
defence all gone, the rebels' gate's reading (§3). The mobile air defence is the guns and the SAMs the enemy believes are
an Osa, Kub, Strela-1 or Strela-10 (`IsMobileAirDefence`), or a SAM of no known system (an unexplained launch): for
that the picture now keeps the system a report believed (`IntelReport::system`, `IntelEntry::system`, from the
object's defence in `ReportAbout`, a dummy's the battery's, an in-flight report's from the believed entry). It changes
nothing else: `IntelEntry::mobile` is true of every SAM site, so it could not tell an Osa from an S-125.

`SectorLossWindow` counts by sector and day (the hour over 24) the sorties of low-level packages (`Profile::Low`), on
their time on target's day in their target's sector, and their aircraft lost to ground fire (not to our fighters, not to
their own side's belt): `Rate(sector, day, days)` over the last one to three days. `AirCampaign` keeps both under
`air_front.mode` 1 only: the pre-war picture after the prior intelligence (`StartAirFront`), a measure at every plan
before the planner (`SectorCovers()`), and the losses in `ApplyOutcome` (the sorties once, when a raid is first
resolved; `SectorLossRate(sector, days)`). At 0, the default and the data's until F2.3, nothing of it is built,
measured or drawn. Nothing reads it: F2.3's decision will. The mode has no save key yet: it changes no outcome, so a
save replays the same either way; F2.3 adds `air_front_mode` with the behaviour.

**The cost.** `fronts_bench data --air-front` (new, the first part of F3.3's bench): each faction's campaign on the
nation with air_front.mode 1, its front cut into eight equal sectors (the most the design gives a front; `--sectors 0`
takes the theatre's own), the measure timed at every day's end over 2000 repetitions on that day's picture, with the
bands kept (`mean_ms`, `worst_ms`) and with every sector's band moved each time (`moved_ms`, every baseline taken afresh:
the worst a cycle can be once F2.3 moves the air front), CPU time (`std::clock`, which MSVC's runtime counts as the
process's elapsed time):

| Faction | Sectors | Picture's entries | Measure, mean | Worst day | Every band moved, worst | Pre-war units kept | Campaign CPU s, mode 0 / 1 | Same war |
|---|---|---|---|---|---|---|---|---|
| The Pact | 8 | 204 | 0.0050 ms | 0.0060 ms | 0.0090 ms | 39 | 94.2 / 95.5 | yes |
| NATO | 8 | 215 | 0.0053 ms | 0.0075 ms | 0.0080 ms | 39 | 61.9 / 62.3 | yes |

Seven days, seeds 7-9, 2000 repetitions a day (`std::clock` ticks in milliseconds), run beside two balance runs and the
test suite: the campaigns' totals move by a percent or two with that load (the measure itself is some microseconds a
plan). On the theatre's own sectors (`--sectors 0`, one seed) the Pact's five and NATO's four cost 0.004 and 0.003 ms.
So a cycle's measure is about 1/100 of the budget; F2.3's decision on top reads its results, a sector at a time.

The budget is 1 ms a planning cycle for a front. The line of contact's reads cost what `DistanceToLine` did: the same
projection, and `DistanceToFront` no longer looks the front up by its faction's name each call.

**Identity.** `air_war_balance data 20 7` and `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` print
byte for byte what they printed before F2.1 (`track-f`'s tip with F1, built and run on its own data, against this
build on this data, the sectors in it), and `test_routes.cpp`'s replay fingerprint (pact, the district, seed 21: 107 entries, FNV 1375944258174313379,
41 packages) holds. `test_front_line.cpp` checks every theatre front's distances to the bit against `DistanceToLine` and a
two-day war at air_front.mode 0 and 1 to the same journal; `fronts_bench` checks it for a week of each faction's war on the
nation.

**Tests.** `test_front_line.cpp` (7): distances, sides and places on a bent line by hand; sectors, the ends and the
default; offsets and their interpolation, the contact point; the identity on both theatres' fronts; the campaign's reads
and the mode's identity; the loader's sectors and its refusals (not from 0, a gap, an overlap, a repeat, an empty run or
list); every theatre front's sectors (4-8, 20-60 km, to the line's end). `test_air_front.cpp` (4): the cover by hand on
a small picture (a decoy half suspected, a unit believed destroyed, one that pulled out, one found later raising the
baseline); a band further in, an empty band, the guns and an unknown SAM, a moved line, back on the first line and a
copy moved in place (the cached places taken again); the loss window by hand; the campaign's measures on the Pact's five
sectors and its losses counted against its packages.

**Left:** `c2.hpp`'s, `resolve.hpp`'s, `raid_scenario.hpp`'s and `logistics.hpp`'s reads of the base line (F2.4, F3.3,
F3.7); the air front's decision, its depths per sector, its journal entries and the save key `air_front_mode` (F2.3:
`SectorCovers()` and `SectorLossRate` are its inputs, `air_front_depth_` the depths it will move); the rest of
`fronts_bench` (F3.3); the map's border as the base line (the owner's open decision 1, argued in F6). A note for F2.3: on
the tuned district theatre our radar SAMs stand 94 km or more behind the Pact's line, so every sector's band there has no
cover and never had (cover 0, mobile air defence all gone): its gates must not open on an empty baseline alone (the
reward gate, or a minimum baseline), or the district's wars change at mode 1.

## As built (F2.3; 21 November 2026)

**The rules as data** (`air_front_def.hpp` `AirFrontDef`; each faction's `"air_front"` block, loaded strictly by
`AirWarLoader::LoadAirFrontBlock`; a faction without it never moves its air front). `kind`: `advance` (NATO, the Pact),
`surge` (the rebels), `border` (the neutral, with its `after_event`, a political event its politics must have); the gates
`cover_at_most` and `mobile_destroyed_at_least`; `loss_line` and `loss_days`; `delay_h` [lo, hi]; `max_depth_km`;
`min_reward`; `crisis` (`advance`, `helicopters` or `none`) and `crisis_h`; a surge's `surge_h` and `surge_rest_h`. The
values are §3's table, SAM3D estimates cited to the cases:

| Faction | Kind | Cover at most | Mobile destroyed at least | Loss line, window | Delay | Greatest depth | Crisis |
|---|---|---|---|---|---|---|---|
| NATO | advance | 30 % | 50 % | 2 % a sortie, 2 days | 6-24 h | 60 km | advance, 24-72 h |
| The Pact | advance | 20 % | 50 % | 4 %, 2 days | 6-24 h | 60 km | advance, 24-72 h |
| The rebels | surge (24-72 h, then 72 h rest) | 0 % | 100 % | 1 %, 2 days | 6-24 h | 40 km | helicopters once |
| The neutral | border until `deeper_raids` | 20 % | 50 % | 1 %, 3 days | 12-24 h | 40 km (and its politics' 40, 60 after the event) | helicopters once |

The reward gate (`min_reward`, 0.3 for every faction: about one believed troop position or column at the nation's
values, 0.5 each at a belief of 0.6) is in the faction's block rather than `air_war.json` (§6 put it there): it is a
per-faction value, read beside the faction's other gates. The game values in `air_war.json` `"air_front"`:
`min_baseline` (1.0), `min_sorties` (6), `helo_lag_h` (24), `trait_shift` (0.2), `reward_reach_m` (40 km).

**The decision** (`air_front.hpp` `DecideAirFront`, pure: the rules, the game values, the enemy's inputs and its chief
in; the state and the moves out; no draw, so mode 1 keeps the war's sequence). `AirCampaign::Plan` measures
(`MeasureAirFront`, F2.2) and then decides (`DecideAirFrontNow`) before the planner, every planning cycle (the Pact's 12
h, the others' 24 h). The chief is the planner's post (the air operations chief), the commander's while it is empty.
Its inputs are all the enemy's own (`AirFrontInputs`, built by `AirCampaign::AirFrontDecisionInputs`): the picture, this
cycle's `SectorCover`s, the `SectorLossWindow`, its target weights and banned kinds, the politics' depth now and whether
the neutral's event has fired; never the truth. Sector by sector, at most one move a cycle, in this order:

1. **The politics hold it** (the depth over its cap: back to the deepest whole step allowed). The cap is the block's
   greatest depth and the politics' `max_depth_m`, the latter measured from the border (the line of contact's offset
   at the sector's middle taken off), 0 for the neutral until its event.
2. **A crisis** (F3.5's ground picture: `in.crisis` is the enemy picture's `GroundWar::EnemyCrisis()` while the ground
   war runs, `ground.mode` 1 on a theatre with a ground file, and null otherwise, so at mode 0 nothing changes): NATO and the
   Pact step forward at once and ignore the loss gate for `crisis_h` (a bold chief the long end); the rebels and the
   neutral send their helicopters a step ahead of the fixed-wing front, once a war, back behind it when the crisis ends.
3. **The surge's end** (the rebels): after `surge_h` (a bold chief the long end) the sector goes back to the border, both
   depths, and may not surge again for `surge_rest_h`.
4. **The pull-back**: with at least `air_front.min_sorties` low-level sorties in the window and a loss rate over the
   line, one step back (the helicopters with it); a chief of composure 0.7 or more and stress under 0.6 waits for a
   second cycle over the line.
5. **The gates**: the sector **contested** (below), the cover at most the gate, the mobile air defence destroyed at
   least the gate, the next band's believed worth at least `min_reward`, no losses over the line (unless a crisis
   overrides them), room for a whole step, and no surge rest. The first cycle that sees them hold notes it (a "Seen"
   entry with the delay); the step comes at the first cycle at or after the delay, if they still hold; one that sees
   them fail forgets it. The delay is `lo + (hi - lo) x (0.5 + 0.5 (methodical - boldness))`, clamped.
6. **The helicopters** follow the fixed-wing front a step at a time, each step after `air_front.helo_lag_h` since either
   front last moved, with at least one low-level sortie flown in the sector over the last two days and their loss rate
   within the line. They never pass the fixed-wing front but in the crisis's one exception. §2's "one step behind" is
   read as behind in time: at 20 km the helicopters reach 20 km after a day of clean work, not at the next step.

Traits move the gates by at most `trait_shift` (0.2) of their value, as track B's officers do: boldness raises the cover
gate and the loss line and lowers the mobile gate, ambition lowers the reward gate.

**The reward** (`SectorRewards`): what a step brings into the low-level work's reach. The enemy's entries of kinds
troops, column, convoy, train, bridge, command post, depot and rail junction in the sector from `reward_reach_m` (40 km,
the planner's close support depth) beyond its air front to a step further, each `Live() x value x` the faction's target
weight, banned kinds left out; one pass over the picture a cycle. §3 says "the next band"; read literally (the band the
cover is weighed over, 0-20 km) it finds nothing on the nation, whose front-line bands hold almost no believed targets
(the ground war's units come with F3), and it would weigh what close support reaches from the border already. Measured
with `fronts_bench --trace`: the Pact's five sectors held one such target within 20 km of the line (worth 0.36), and the
best band's worth was 0.4 against the gate's 1.0 first tried; so the reach, and the gate at 0.3. The risk is still
weighed where the aircraft fly low (the next band), the worth where the step lets them strike.

**The empty baseline.** F2.2's note: on the tuned district our radar SAMs stand 94 km or more behind the Pact's line, so
every sector's band measures cover 0 and mobile air defence all gone. A sector's gates open only once it is
**contested**: once some band of it was believed covered by at least `air_front.min_baseline` (1.0, the sum of
`BelievedThere`: one fire unit fully believed, or two half) of radar SAMs or mobile air defence. No cover where nothing
was ever believed is not a hole the staff has made; a cover broken below the gate in a contested sector is observed
destruction (at least 1 - gate of a baseline of at least one unit). Once contested a sector stays so: the band beyond a
broken one may be empty and still be stepped into, the reward gate and the greatest depth bounding it. Tested both ways
(`test_air_front.cpp`): the far-back picture never opens, the same with `min_baseline` 0 opens both sectors, and on the
district the Pact's campaign at mode 1 makes no move in three days (and with both guards off it moves and still plans
the same packages).

**The campaign** keeps `AirFrontState` (per sector: both depths, when each reached it, the pending decision and its
delay, the cycles over the loss line, contested, the surge's and crisis's times, the counts of steps) under
`air_front.mode` 1, writes each sector's fixed-wing depth into `air_front_depth_` (the band the next measure weighs, and
what F2.4's planner reads), and logs every move in the journal, kind `"air_front"`, by the chief, not visible to our side
(our side learns it in F2.5): e.g. "s3: the air front moves forward to 20 km (cover 0%, mobile air defence 100%
destroyed, worth 4.2)". Accessors: `AirFront()`, `AirFrontDepths()`, `AirFrontAt(p)`, `HeloFrontAt(p)` (0 at mode 0).
Nothing outside the decision reads the depths yet, so at mode 1 the war's packages are those of mode 0 (fronts_bench's
"same", the district test). `air_front.mode` stays 0 in the data until F2.4 makes the depths matter.

**Save.** `SaveCampaignJson` writes `air_front_mode`; a save without it loads with `air_front.mode` 0 and replays as
played (`test_air_front.cpp`).

**How it moved** (`fronts_bench data --air-front --sectors 0 --days 10 --seeds 2 --moves warsaw_pact nato rebels
neutral`: each faction's war on the nation at mode 1, the theatre's own sectors, seeds 7 and 8, ten days):

| Faction | Steps forward, back | What moved |
|---|---|---|
| The Pact (5 sectors) | 1, 0 | Seed 7: the Tisava valley, its one believed fire unit there believed destroyed (cover 0, mobile air defence all gone) and the Mezovar road bridge in the reach beyond (worth 0.5): seen on day 2 at 13:00, the front to 20 km 24 h later (its planner, Colonel Belyaev, methodical 0.8 and boldness 0.5: a 17.7 h delay, so the second 12 h cycle after it), the helicopters to 20 km a day later after 8 clean low-level sorties. Seed 8: nothing. Its other contested sectors (the northern forest, the Rava reservoir) kept a believed cover of 0.4-0.75 all ten days; the Hradec and north-eastern sectors were never contested |
| NATO (4 sectors) | 0, 0 | No contested sector's believed cover fell under its 30 % (the rollback works on the whole country first) |
| The rebels, the neutral (4 each) | 0, 0 | Their wars on the nation are small (under a second of CPU for ten days), no sector of theirs contested and broken; the neutral's `deeper_raids` did not fire |

Sensible, not twitchy: one sector of one war moved, once, a step, where the picture says the hole is, with something to
strike beyond it; nothing pulled back because nothing advanced into fire. The tests drive the rest (pull-backs, surges,
the border, the crisis) by hand. Whether this is too timid is F6's balance, once F2.4 makes the depths matter.

**The cost.** `fronts_bench data --air-front` (seven days, seeds 7-9, each front cut into eight equal sectors, 2000
repetitions a day on that day's picture, CPU time; run alone): `cycle_ms` is the planning cycle's whole air front work,
the measure at the day's depths and the decision on a copy of the day's state, as the campaign runs it.

| Faction | Sectors | Picture's entries | Measure, mean | Worst day | Every band moved, worst | Measure and decision, mean | Worst day | Campaign CPU s, mode 0 / 1 | Same war |
|---|---|---|---|---|---|---|---|---|---|
| The Pact | 8 | 204 | 0.0044 ms | 0.0050 ms | 0.0060 ms | 0.0076 ms | 0.0085 ms | 83.7 / 82.1 | yes |
| NATO | 8 | 215 | 0.0047 ms | 0.0050 ms | 0.0060 ms | 0.0087 ms | 0.0095 ms | 54.6 / 55.2 | yes |

A cycle costs under 1/100 of the 1 ms budget; the decision adds about as much as the measure (the reward's pass over the
picture). The campaigns' totals with the mode on and off are within their noise. "Same war": the same packages (ids,
sizes, times on target, targets) and the same journal but for the air front's own entries. In this run the Pact's eight
equal sectors moved twice (one at the end of seed 7: its sixth sector at 20 km, helicopters with it), NATO's none.

**Identity.** At `air_front.mode` 0 (the default and the data's) nothing is built, measured or decided:
`air_war_balance data 20 7` and `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` print byte for byte
what `track-f`'s tip (717c1e6, built and run on its own data) prints, and `test_routes.cpp`'s replay fingerprint (pact,
the district, seed 21: 107 entries, FNV 1375944258174313379, 41 packages) holds. At mode 1 the packages are the same
(`fronts_bench`'s "same" on the nation, `test_air_front.cpp` on the district with and without the guards).

**Tests.** `test_air_front.cpp` (+8, 12 in all): a hole opens one sector only, after the chief's delay, a step at a
time, while it is worth it (a bold and a methodical chief's gates and delays); on belief, not truth, and never on a band
nothing was ever believed to cover (the district's case, the guard off, a contested sector stepping into an empty band
to its greatest depth); losses pull it back (the minimum sorties, a composed chief's extra cycle, a stressed one's none,
the window before it opens again); the helicopters lag by a day of clean work and never pass; the rebels' surge and
rest, the neutral's border until its event, the politics' cap; the crisis stub (the Pact's step and override, the
rebels' helicopters once); every faction's block and the loader's refusals, the campaign at mode 1 on the district;
the save key.

**Left:** the planner, routes and C2 reading the depths (F2.4: `AirFrontAt`, `HeloFrontAt`, `AirFrontDepths`), and
`air_front.mode` 1 in the data with it; our side learning it (F2.5); the crisis's input from the ground picture (F3.5:
`AirFrontInputs::crisis`; done, `03_GROUND_WAR.md` "As built: F3.5"); the pull-back's "keep 16-24 km back from the belt" once the ground war gives a belt (§2); the
gates' values against history (F6).

## As built (F2.4; 21 November 2026)

**What reads it** (`front_line.hpp` `AirFrontView`): the line of contact and each sector's fixed-wing and helicopter
depths, pointers to the campaign's own vectors (`AirCampaign::FrontView()`, the helicopters' now kept beside the
fixed-wing ones, `HeloFrontDepths()`), handed to the planner (`PlanContext::air_front`), the route planner
(`RouteRequest::air_front`) and C2 (`AirC2::Bind`, `C2MissionOf`'s third argument). Off at `air_front.mode` 0: the
vectors are empty, every depth reads 0, and each reader keeps its fixed depth to the bit (40 km + 0.0 is 40 km). A
sector's depth is looked up by the target's projection on the base line, once per candidate and only for the kinds that
read it.

| §4's row | As built |
|---|---|
| Close support within 40 km | within 40 km + the target's sector's `depth_m` (`Planner::Candidates`, the `MissionKind` switch) |
| Helicopter raids within 25 km | within 25 km + the sector's `helo_depth_m`: the helicopters' own front, a day behind (F2.3) |
| Harassment within 70 km | within 70 km + the sector's `depth_m` |
| BAI within `c2.bai_depth_m`, nominations waiting | within it + the sector's `depth_m`: `NominationWaits`' new `air_front_m`, and C2's mission label (`InBaiArea`, read by `C2MissionOf` and `AirC2::MissionName`) |
| The let-down where the route crosses the line | `RoutePlanner::LetDownAtAirFront`: where the gate is inside its sector's air front, a low fixed-wing flight crosses the line of contact at its transit height (the entry, the checkpoint, the gate and the turns inside at transit height and cruise), lets down where its way in leaves the air front (the first leg ending past it; the crossing found by halving the leg, 24 times) and climbs where its way home comes back inside it; a target inside the air front: the let-down a leg short of the IP (`letdown_m` at most), the climb as far past the egress. The way in and home are planned as before: only the heights change, so the corridor, the turns and the length are those of mode 0 |
| Helicopters' holding areas `route.helo_holding_m` behind the line | `route.helo_holding_m` behind the gate's sector's helicopter front, never nearer the line than the new `route.helo_holding_floor_m` (15 km, §3's artillery range, a SAM3D estimate) |
| The Pact's helicopters within `route.helo_umbrella_m` of an entry point | within it of the entry point plus the target's sector's air front (`BeyondHeloUmbrella`) |
| The politics' `max_depth_m` from the line | from the border: the base line once the line of contact has moved (F3.3), the same distance until then |

Escorts and other flights following a route (`FollowRoute`) fly high from the entry to the let-down and from the
climb on, wherever those now are; a played raid's flights follow the ground from the let-down (`raid_scenario.hpp`), so a
gate crossed high is not taken for the low leg. All four identical to before on a route whose let-down is short of the
gate.

**C2 and the line of contact.** `c2.hpp`'s four reads of the base line now read the line of contact (`AirC2::ToFront`, the
bound view's `AbsDistance`; with none bound, the base line, as before): the BAI area (with the air front), the free hunt
zone and our fires' reach on a fixed post (the line of contact only: neither is low-level work the air front carries).
The same distances to the bit while no station has moved. The campaign binds its view wherever it binds the posts (its
plan, its C2 context, `C2()`), and the package injector passes it to `C2MissionOf`.

**Not moved**: the free hunt zone, high-level strikes, SEAD and DEAD (weighed by their routes' believed threats, B2), the
neutral's and the politics' depth from the border.

**At data values the holding area does not move**: `route.helo_holding_m` and the floor are both 15 km, so the holding
area stays 15 km behind the line whatever the helicopters' front (`test_routes.cpp` moves it with a 30 km holding
distance). The floor is there for F6's balance, should the holding distance grow.

**Mode 1, the nation** (`air_war_balance --theatre theatre_pannonia_nation.json --set air_front.mode=1 data 10 7`
against the same at mode 0; ten seeds, two battery postures, the theatre's own sectors): the rebels', the neutral's and
NATO's wars print byte for byte as at mode 0 (no sector of theirs moved, F2.3). The Pact's change from day 3, where its
front first steps forward:

| The Pact | Packages | Sorties | Lost | Lost a sortie | Air battle, theirs for ours | On-call latency |
|---|---|---|---|---|---|---|
| Careless battery, mode 0 / 1 | 156.4 / 157.0 | 925.9 / 929.1 | 33.6 / 34.0 | 3.63 / 3.66 % | 1.66 / 1.61 | 29.1 / 29.2 min |
| Careful battery, mode 0 / 1 | 163.3 / 162.8 | 945.0 / 943.0 | 31.0 / 29.4 | 3.28 / 3.12 % | 1.44 / 1.34 | 27.9 / 27.8 min |

The mission mix is unchanged to the per cent; the targets shift by a few tenths of a package (depots, guns and SAM sites
a little more, troops and convoys a little less), losses to the guns rise and to our fighters fall slightly, and every
band still passes. Nothing broken: a step of 20 km in one sector at a time moves a handful of packages. Whether the mode
goes on, and with which gates, is F6's balance.

**The cost and the moves** (`fronts_bench data --air-front --days 10 --seeds 2 warsaw_pact nato rebels neutral`: eight
equal sectors, seeds 7-8, run alone): `deeper` counts the mode-1 packages flying close support, helicopter raids or
harassment past their kind's fixed depth.

| Faction | Steps forward, back | Packages mode 0 / 1 | Deeper | Campaign CPU s, mode 0 / 1 | Cycle, mean |
|---|---|---|---|---|---|
| The Pact | 2, 1 | 452 / 451 | 1 | 79.7 / 78.8 | 0.0087 ms |
| NATO | 0, 0 | 441 / 441 | 0 | 54.1 / 55.1 | 0.0097 ms |
| The rebels | 0, 0 | 28 / 28 | 0 | 0.98 / 0.98 | 0.0066 ms |
| The neutral | 0, 0 | 29 / 29 | 0 | 0.30 / 0.30 | 0.0069 ms |

The readers cost nothing measurable: a sector lookup (the target's projection on the base line, about a dozen stations)
per close support, helicopter or harassment candidate, and a few bisection steps per route whose gate is inside the air
front; no allocation (the view holds pointers to the campaign's vectors). The campaigns' CPU with the mode on and off
is within its noise (`std::clock` is wall time on MSVC).

**Identity.** At `air_front.mode` 0 every depth reads 0.0 and each reader computes what it did: `air_war_balance data
20 7` and `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` print byte for byte what `track-f`'s tip
(d016c8a, built and run on its own data) prints, and `test_routes.cpp`'s replay fingerprint (107 entries, FNV
1375944258174313379, 41 packages) holds. At mode 1 the war now parts once a sector moves: F2.3's campaign test keeps
"the same packages" only for the run where nothing moves, and prints the other.

**Tests.** `test_air_war.cpp` (+1, 28): close support, helicopter raids and harassment reach their fixed depth plus the
sector's, in that sector only; BAI's nominations wait to the deeper line; the Pact's helicopter umbrella from the entry
plus the air front. `test_routes.cpp` (+1, 27): at depth 0, at 0.0 and in another sector the route is the same point
for point; at 20 km a low flight crosses high, lets down where its way in leaves the air front (within 5 m) and climbs
where its way home re-enters it; a target inside the air front; the helicopters' holding area behind their own front
and never nearer than the floor. `test_c2.cpp` (+1, 13): the BAI label reaches the deeper line in its sector, the free
hunt zone and our fires read the line of contact. `test_air_front.cpp`: the F2.3 campaign test as above.

**Left:** `air_front.mode` 1 in the data (F6, with the gates' values); the deep-strike line beyond the FSCL (the corps'
area of interest; F3.7 made the FSCL the BAI area's depth, not yet a separate inner edge); our side learning the air
front (F2.5). Done since: the FSCL (F3.7); the holding area measured from the moved line of contact (F3.7: the gate's
offset taken off its distance behind the helicopter front); `resolve.hpp`'s (F3.3), `raid_scenario.hpp`'s and
`logistics.hpp`'s (F3.7) base-line reads.

## As built (F2.5; 21 November 2026)

**Who learns it.** A new post in `data/command/chain_pannonia.json`: `army_ad_staff`, the senior officer of the operations
section of the army's air defence staff (Major Pavel Kratochvil, under the army's chief of air defence at the army's
headquarters; `andersen_1979` for the troop air defence staffs and the air situation they keep, the sector-by-sector
estimate SAM3D's reading). He keeps the signs, sector by sector and day by day, and states the conclusion.

**The signs** (`command/air_front_watch.hpp` `FrontSign`; sent by the feed, `CampaignFeed`, only from what our posts saw,
heard or wrote, each a fact for the staff alone in the pipeline, `need_level` 0):

| Sign | From | Fact | When it is one |
|---|---|---|---|
| A low-level attack on our troops by day | the division's air defence CP (`div_ad_cp_duty`), on the warning net's record (`ObservedRaid`) | `RaidSeen`, priority, `front_watch:raid:<n>` | the raid came at low level, with no helicopters, at an object of ours of kind troops, not at night, and attacked (delivered); its target more than `watch_cas_reach_m` + `watch_margin_m` (45 km) beyond the line of contact |
| Helicopters attacking | the same | the same | the raid had helicopters and attacked; beyond `watch_helo_reach_m` + the margin (30 km) |
| A close support request heard | the intercept centre (`intercept_centre`), on our desk's form (`Logged`, the abstract calls' write-up, radio.mode 1 or more with `UseIntercept`) | `Intercept`, routine, `front_watch:intercept:<n>` | understood, a request (the HF request net), naming a square: the square as the linguist wrote it, slips and all, beyond 45 km |
| Our troops report an air attack | the post that reports it | `RaidSeen`, priority, `front_watch:troops:<n>` | `CampaignFeed::ReportAirAttack` (F3.8's `GroundFeed` calls it for every strike on our troops a unit counts, `04_PIPELINE.md` §8) |

The warning net's record gained what it saw of how a raid came (`ObservedRaid::low`, `helicopters`, `target_object`:
our own object it went for). The depth is measured from our line of contact (`AirCampaign::FrontLine()`, our own front)
to where our side saw the raid go, the sector by the same line's lookup. Night raids are left out: they are the
enemy's harassment (70 km at the border), and low-level strikes on anything but troops go everywhere whatever his
air front, so neither tells it.

**The staff's rule** (`AirFrontWatch::Take`, pure, in the order the signs reached the staff; game values in
`data/gameplay/air_war.json` `"air_front"`, the `watch_` keys): a sign counts in its sector's campaign day when it lies
beyond what the staff expects, the reach (close support 40 km, helicopters 25 km, `barbeau_1985_sams` p. 24) plus its
own estimate of the sector's air front, by more than `watch_margin_m` (5 km) and no more than `watch_band_m` (40 km, two of
his steps: deeper is interdiction, not the close support's front; the NATO run below heard requests naming squares
250 km deep). `watch_count` (3) such signs in a sector within `watch_days` (2: that day and the one before) and the staff
concludes: the estimate moves by the count-th deepest sign's excess, rounded down to `watch_round_m` (5 km), the depth at
least that many signs reached, and the window starts again. The conclusion is stated `watch_delay_h` (6 h) after the last
sign reached it. The staff has no pull-back: a front that went back stays where it was last concluded.

**The conclusion travels** as a new kind of fact, `FactKind::AirFront` (`air_front_estimate:<n>`, subject
`air_front:<sector>`; `PaperTitle` "The army's air defence staff's estimate of the enemy's air front"): a document of
priority from `army_ad_staff` for the division's chief of air defence's tree down to the battery commanders (`need_level`
3): the division's air defence chief and CP, the regiment's commander and staff, battery 21's commander. Its text says
how deep his close support and helicopters now work there and on how many signs since which day; the voice carries a
summary and the paper follows (`document`). `CampaignFeed::AirFrontKnownBy(post, sector)` is the estimate a post holds,
from the latest conclusion that has reached it: what the regiment's tasking (14.2) and F4's sector priorities read.
Nothing in it reads `depth_m`, `AirFrontDepths()` or the enemy's journal.

**Deterministic.** The staff takes a sign only once it has reached it and its hour is below the watermark (the earliest
hour a package not yet resolved could still add one, as the intercept desk's), in the order of arrival: a campaign pumped
step by step and one pumped once after its load take the same signs in the same order and state the same conclusions
(tested). The conclusion is added with its hour and the net advanced again, so it reaches people at the same times either
way.

**Cost.** Per sign a fact and a lookup on the line; per conclusion a fact; per pump one pass over the signs not yet taken.
Per sector a vector of the window's few signs. Nothing per step, nothing at `air_front.mode` 0.

**Gating.** Only where the air war runs its air front: `air_front.mode` 1, the chain has `army_ad_staff` and the
division's air defence chief, the front has a line. At the data's mode 0 the feed sends nothing new (tested), and the air
war's own runs never build the feed.

**When it learned, the nation at mode 1** (`fronts_bench data --watch --days 10`: the pipeline pumped after every step,
the intercept service on):

| War, sectors, seed | The enemy's air front moved (his journal) | Signs that reached our staff | Our staff concluded |
|---|---|---|---|
| The Pact, its 5, seed 7 | the Tisava valley to 20 km D3 13:00, its helicopters to 20 km D4 13:00 | none | nothing |
| The Pact, its 5, seed 8 | no move | none | nothing |
| The Pact, 8 equal, seed 7 | s6 to 20 km D6 01:00, helicopters D7 01:00, back to 0 D7 13:00 | one helicopter attack in s6 | nothing (one of the three) |
| The Pact, 8 equal, seed 8 | s6 to 20 km D3 01:00, helicopters D4 01:00 | none | nothing |
| NATO, its 4, seeds 7 and 8 | no move | 7 and 12 intercepted requests in the western crossings, all naming squares about 290 km deep | nothing: beyond the band (before the band, it concluded the front 250 km deep, D3 00:42 and D4 22:43) |

So the staff never concluded, and rightly by its evidence: where the enemy's air front moved, his planner sent almost
nothing deeper (F2.4's `deeper` column: one package in ten days), so the warning net saw no low-level attack on troops
and only one helicopter attack beyond the usual reach. The moved sector is known to the enemy and not to us; the
signs will come once F6's balance (and F3's troops near the line) makes the step worth flying. The pipeline's CPU with
the watch on is that of the feed (24-40 s for ten days, most of it the war itself).

**Identity.** At the data's `air_front.mode` 0 the watch is never built and the air war is untouched (the
warning net's record only gained three fields nothing else reads): `air_war_balance data 20 7` and `air_war_balance
--theatre theatre_pannonia_nation.json data 10 7` print byte for byte what `track-f`'s tip (306a637, built and run on its
own data) prints, and `test_routes.cpp`'s replay fingerprint (107 entries, FNV 1375944258174313379, 41 packages) holds.

**Tests.** `test_command.cpp` (+2, 20): the staff's rule by hand (the reach, the margin and the band; the window of two
days; the count-th deepest sign rounded down; the estimate moving the next signs' bar; a sign not yet arrived not taken);
the Pact's war on the district at mode 1: with the enemy's front moved to 40 km and the staff expecting nothing so deep, no
sign and no conclusion (never from his depths); with his front never moving and a credulous rule (reach 0, one sign), a
false conclusion from an ordinary helicopter raid, through the pipeline to the regiment's commander after its hour, who
then holds its estimate; the same pumped once at the end; at the data's rule only on three signs or more; at mode 0
nothing.

**Left:** our troops' reports (done, F3.8: `GroundFeed` calls `ReportAirAttack` for each strike on our troops a unit counts); the played
sessions' heard requests (`PumpHeard` makes their facts but not their signs: the records carry no square); the staff's
pull-back (a quiet sector after a conclusion); the regiment's tasking and F4's priorities reading `AirFrontKnownBy`
(14.2, F4.1); the thresholds' balance against the enemy's real moves (F6: on the nation the moved sectors brought almost
no deeper raids, so the evidence is thin).
