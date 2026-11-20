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

**Crisis.** Where the enemy's own ground front in the sector is collapsing (its ground picture, F3.5: a formation
below half strength or a breakthrough by our counterattack), the chief may ignore the loss gate there for 1-3 days and
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
attack. Its only effect in F2 is the mission label C2 reads; the BAI depth above stands in until then.

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
zero offsets.

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
takes the theatre's own), the measure timed at every day's end over 200 repetitions on that day's picture, with the
bands kept (`mean_ms`, `worst_ms`) and with every sector's band moved each time (`moved_ms`, every baseline taken afresh:
the worst a cycle can be once F2.3 moves the air front), CPU time (`std::clock`, which MSVC's runtime counts as the
process's elapsed time):

COST_TABLE

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
`fronts_bench` (F3.3); the map's border as the base line (the owner's open decision 1, argued in F6).
