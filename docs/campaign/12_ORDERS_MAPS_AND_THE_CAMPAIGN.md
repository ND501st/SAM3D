# 12. Orders, maps and the campaign (track O)

Approved by the project owner on 25 Nov 2026, with notes (the review page's answers D1-D12). The campaign becomes the
game's main mode. Battery 21 moves across the country with its division, and everything the player knows reaches them
through the orders, briefings and maps their character holds. Most of the parts exist in simcore; this track joins them
into something playable, in seven slices (section 10), each one something the owner plays.

## 1. The nation (the owner's note on D7, 25 Nov 2026)

The first time this is written down; the campaign's officers act on it.

- A former Warsaw Pact member whose government changed under a charismatic leader. He steered it towards nationalist
  neutrality, lost its old alliances and won no support from the West.
- It sits on the line between East and West, and each neighbour sees its instability as a chance to take it.
- Its forces were trained to Pact standards, but its officers are beginning to take their own liberties. Those
  liberties are what justify the departures from Soviet practice in section 8's redeployments.

## 2. Where the recent tracks stood (25 Nov 2026)

B (the air battle) built in simcore, B13 (Unreal) the owner's. R merged. E0-E7, E9, E10 done, E8 begun. F1-F8 built,
PR #81 open. G0-G5 and G8-G11 in first form, G6-G7 on hold. Phases 14.1-14.3 and 15.2-15.4 in first form. Command P1-P9
in first form. The gap they share: none of it could be reached as a campaign. No game path ran the continuous mission
(`MissionSession`); the campaign screen played each raid as a separate scenario with the battery deployed in place.

## 3. The decisions (D1-D12, approved)

| | Decision | The owner's note |
|---|---|---|
| D1 | Orders come down in three tiers: the regiment's evening briefing (19:00 at its command post in the Lazne woods; urgent tasks at once by wire, radio or courier), the battery commander's oral combat order (on the ground after reconnaissance; at the PU-12M's map when time is short; by radio in short form when urgent), the vehicle commander's to his crew at the vehicle. No corps or brigade level for the player. | |
| D2 | Maps are papers people hold, showing only what reached the holder: the regiment commander's (the division's zone), the battery commander's working map, the vehicle commander's with his sector card. Conscripts hold none; a crew seat may look at its commander's. Every mark has a source and an "as of" hour; ours red, the enemy blue. | Make them look a lot like Eastern Bloc maps: the period's Soviet-pattern sheets at 1:100 000 and 1:50 000, a numbered 1 km grid, sheet number and margin data, brown contours, green woods, blue water, roads by class, settlements as dark blocks; marks as red and blue coloured pencil with handwritten labels and hours; the classification stamped in the margin; the regiment's order on a tracing-paper overlay. Symbols from the period's conventional signs (public tables, R0); lettering in the nation's language. |
| D3 | Commanders decide on their maps, and each decision becomes an existing battery order (march to a place, combat formation, sectors). | The vehicle commander's spot within **1 km** of his given point (a labelled game value; the textbook spaces a pair 200-500 m apart, pairs up to 3 km, pp. 232-239). His spot goes on the battery commander's map only as reported; the review notes a sector left uncovered. |
| D4 | Limits are the order's rules, not walls. NPC commanders keep them; the player may break them. A breach counts only if someone sees it or writes it down, and then the review grades it. | |
| D5 | IFF codes come as a paper handed out at the regiment's briefing. | Confirmed not by radio, and simcore already does it so (B10): the signals chief issues the next day's code schedule as a paper, carried only by means that carry documents; at each change time the crew chief orders the change and the senior operator turns the interrogator's code switch (`kIffCodeControl`) with a callout. A vehicle without the paper stays on its last code, logged. Source: a tier-4 page (12 code filters, changed every 1-6 h, low confidence); R0 looks for an operator-country procedure. |
| D6 | Each day summed up at the next evening briefing, graded on what reached the regiment (5 to 2 with a reason), trust deciding who gets the important task, the truth only at the war's end. No score screen. | |
| D7 | The battery moves with the 7th Motor Rifle Division (its regiment the 15th SAM Regiment): every campaign opens with the division leaving Kesmark for its war plan's area, then the ground war's moves. | Other: the officers may also send the regiment, a battery or a single launcher elsewhere (section 8). Slice O7. |
| D8 | The play area follows the battery: districts generated where the division fights (15.6), a fixed list per adversary first. | |
| D9 | The campaign is the main game mode: the menu opens on Campaign (the adversary, who you are, the helper); Training holds the rest; the old campaign screen becomes the developer's view. | |
| D10 | The war runs continuously and saves at its seams: each task one session, the battery's state carried from task to task, saves at each task's start and each evening briefing. | |
| D11 | The rebels' war comes first (the 7th Division at Ralje, Hollos and Kesmark, on the existing district): O1-O5 need no new ground. | |
| D12 | A lean process (section 11). | |

## 4. How orders reached a battery in 1985

From `voenizdat_osa_1991` pp. 224-245 (`docs/research/OSA_MANUALS_2026-10.md`) and `03_THE_BATTERY_MISSION.md`. A
battery changes position only on the regiment commander's order (p. 238). The battery commander's sequence: understand
the task, work out the time, give preliminary orders, assess the situation, decide, reconnoitre, give the oral combat
order, organise support (pp. 240-245). Items marked *to confirm* get R0's capped check before they are built.

- **The regiment's evening briefing**: at the 15th's command post; the regiment commander (Lt Col Vrana), the chief of
  staff (Maj Sallai, who runs it), the deputies, the chief of signals, the five battery commanders. Summing up the day;
  the enemy; the situation; tasks to each battery traced onto the battery commander's map; coordination (our aviation's
  corridors and hours, the code table and change times, call signs and nets); support; reports. *To confirm*: the order
  of items. Hands over the order with its overlay, the code table, the plan of cooperation, the call-sign table.
- **The battery commander's order**: preferably on the ground after reconnaissance, else at the PU-12M's map table or by
  radio in short form (which the enemy's intercept hears). To the four vehicle commanders, the MANPADS section's, the
  supply section's leader, the maintenance chief and the deputy. The combat order's parts (pp. 240-245). Hands over each
  vehicle's point and sector, its reserve position, the route card and the code change times.
- **The crew's briefing**: at the vehicle on arrival; where we stand and why, the sector and its landmarks, readiness and
  emission, who designates, the code change time, the reserve position, what to do if contact is lost. Then the clutter
  card and the landmark card (pp. 246-258).
- **Maps by echelon**: 1:100 000 for regiments and divisions, 1:50 000 for batteries (*to confirm*, a weak teaching
  table); held by officers, signed for (*to confirm*). The PU-12M's board stays the live air picture; the map shows the
  ground and the plan.

## 5. The maps (O2)

One model in simcore, three views in the game. A mark is a shape anchored to places by id (F1's rule). Each records who
told the holder, by what means, as of when, how sure (reported, confirmed, estimated), and whether it was copied or is
the holder's own decision. Marks are made only from facts that reached the holder through the pipeline.

| Mark | Regiment's | Battery commander's | Vehicle's map and card | From |
|---|---|---|---|---|
| Forward edges, ours and the enemy's, with hours | yes | yes | near his position | the division's summaries, the covered unit |
| Our troops, the covered unit; the enemy's ground forces | yes | yes | the covered unit | the same, the regiment's intelligence |
| Attack directions, helicopter lines, jammer orbits, ARM lines | yes | yes | in his sector | the regiment's estimate |
| Our aviation's corridors and zones, with hours | yes | yes | yes | the plan of cooperation (B10) |
| Neighbours' zones | yes | yes | adjacent ones | coordination |
| Position area; main, reserve and dummy positions | the area | decides | his point, reserve | the order; the decision |
| Sectors | the battery's | decides each launcher's | his own | the decision |
| Route, start point, control points, halts | - | yes | yes | the march plan (G2) |
| Detection limits, launch zones by height | - | yes | his own | the textbook's tables |
| Landmarks, clutter, dead zones | - | - | the crew's cards | the crew's work on arrival |
| Reported events: raids, launches, wrecks, strikes | yes | yes | what he heard | the pipeline |

In the game: M, the map case (O) or the PU-12M's table shows your map full screen with zoom, the legend and each
situation's hour. A commander moves the marks that are his own decisions and gives the order from the map.

**O2's build (26 Nov 2026).**
- **The model: `simcore/command/held_map.hpp`.**
  - A `HeldMap` belongs to a holder (a position of the chain, or a vehicle's commander). Its `HeldMark`s each carry:
    - a kind and a side (ours red, the enemy blue);
    - a shape: points, or a point with an arc for a sector, in the session's frame;
    - a short label in the map's hand;
    - the source (who told the holder) and the means (by what);
    - `as_of_h`, and the certainty (reported, confirmed, estimated);
    - the origin: copied, or the holder's own decision.
  - Marks come from three places:
    - the holder's situation map (F5's `SituationMap`, already built only from facts the post holds); each mark's source and means come from its fact's `Delivery`;
    - the regiment's task as it reached him (its order book's means and hour);
    - his own decisions.
- **Decisions: `BatteryDecision`.**
  - It holds the main position, the reserve, the dummies and each launcher's sector.
  - A position may stand within `decision_leeway_m` of the point the order gives (D3's 1 km, a labelled game value). The sectors must together cover the battery's.
  - It is checked against the task (`CheckDecision` lists what breaks the order, and refuses nothing), then given: the runner's `SetBatteryDecision` makes the combat order use the commander's split and places in place of the task's own.
- **The sector card.**
  - Each launcher commander gets a paper with the combat order: his point, his sector and landmarks bearings, his neighbours' sectors, the reserve position and the code change times.
  - It goes in his map case, as a paper carried with the order.
- **Unreal: M in the cabin's HUD.**
  - The holder's map, drawn as a Soviet-pattern sheet:
    - the numbered 1 km grid, the sheet's number and margin with the classification stamped in it;
    - brown contours from the map's heights, green woods, blue water, roads by class and dark settlement blocks;
    - marks in red and blue pencil with their labels and hours.
  - The legend and a mark's source are shown on hover. The mouse wheel zooms; dragging moves the map.
  - As battery commander: place the main, reserve and dummy positions, draw each launcher's sector, then give the combat order from the map.

## 6. Who knows what, and freedom within the order

| You are | You attend | You hold | You decide | Graded by |
|---|---|---|---|---|
| Battery commander (PU-12M) | the regiment's briefing; you give the battery's order | the order and overlay, the working map, the code table, the communications diagram, the staff list, the order of march | positions inside the area, the sectors' split, dummies, the route, a regime inside the ceiling, the fire control mode, reloads, reports, a move to the reserve if the order delegates it | the regiment |
| Commander of 21-1 | the battery's order; you brief your crew | your map with point, sector and neighbours; the route card; the sector card; the code change times | your spot within 1 km, the search split, engagements when the fire mode allows, the crew's watch | the battery commander |
| A seat of 21-1 | your commander's briefing | nothing of your own (you may look at his map and cards) | your seat's work | your vehicle commander |

Following a unit: the area moves with the covered unit, the battery 1.5-3 km behind the covered companies (pp.
232-239). Fire control: on the battery commander's designation; by permission; independent search and fire in the
sector (`OrderSearchOwn`). *To confirm*: the period's names for the modes.

## 7. Objectives and the review (O5)

Each objective graded 5 to 2 only on what reached the regiment, with a reason as given: the covered object kept working
(its own reports), aircraft downed (claims confirmed by the radar company, a neighbour or a wreck), ready on time (the
ready report in the regiment's journal), records kept (P9's check), limits kept (B10's trace; a breach seen or written
down), the battery kept (its loss report). Consequences: trust (who gets the important task), the career (P8), the next
order's tone. At the war's end the reveal sets the true ledgers beside what the regiment believed.

## 8. Moving with the war (O6, O7)

The chain: the enemy's war, the ground war's formations, the division's order, the regiment's tasking (F4.4), battery
21's task, the march on the roads (G2), the district it fights in. New in O6: the opening march from the garrison to
the war plan's area (`ground_defence.json`), the second echelon sent to a threatened sector, a battery sent to cover a
first-echelon regiment, the regiment's batteries moved by bounds. Districts about 12 km square at 10-20 m ground:
Kesmark (exists), the Rava dam by Dravec (the Pact), the Dombas hub towards Novo Varosd (NATO), the neutral's southern
front. Risk: the divisions seldom move as tuned (F4.4 found no withdrawal in 21 days); the opening march guarantees one
long move. The war's balance is not retuned for this.

**The officers' redeployments (O7, the owner's D7 note).** Each situation is a rule in the regiment's tasking table,
firing only on facts that reached the officer, its source field the precedent; the officers' traits set how readily
they leave the textbook, the adversary's doctrine profile weights each rule. A player's vehicle can be the one detached,
and they are then the senior man on the spot.

| Situation that reaches the officer | What he orders | Precedent | Most against |
|---|---|---|---|
| Another division under heavier air attack | the regiment, or some batteries, sent there for some days | AD massed on the main axis (*to source*); Egypt 1973, armour beyond the SAM umbrella | the Pact, NATO |
| Intelligence names an asset about to be struck | a battery or a launcher sent to cover it for a few days | Vietnam 1965-72; Libya 1986 | all |
| A battery worn down (fatigue, faults, missiles) | relieved by a reserve battery, rested and repaired | rotation in long air campaigns (*to source*); needs a reserve battery | long wars |
| The regiment's radars or post destroyed, contact lost | batteries or launchers fight alone, roving | Syria 1982; flagged hindsight: Iraq 1991, Yugoslavia 1999 | NATO, the Pact |
| A breakthrough or encirclement | withdraw alone to the next line or friendly force | Soviet doctrine for units cut off (*to source*); hindsight: Iraq 2003 | the Pact, NATO |
| A deep-strike campaign on strategic assets | batteries lent to the national air defence | Hanoi and Haiphong 1967-72; the nation's own liberty; hindsight: Iraq 1991 | NATO, the Pact's long-range aviation |
| A special mission: an ambush, a convoy, a bridging | a battery or launcher detached, radars silent until the moment | Egypt 1970; Vietnam's mobile ambushes; hindsight: 1999 | NATO, the Pact; the rebels' helicopters |
| Rebels: few aircraft, many helicopters | launchers split singly over villages, roads, convoys | Angola in the 1980s (*to source*) | the rebels |

The war's year is 1985: cases after it are flagged hindsight and inform the design, never the 1985 doctrine.

## 9. The campaign as the game

The menu opens on Campaign: New (the adversary with its line, who you are, the helper) and Continue. Training holds the
lessons, missions, drills, ranges, history and the test range. A campaign's course: day 0 in garrison (the alert, the
regiment's first order), the march out and occupation, duty and raids at compressed time woken by what reaches you,
19:00 the summing up and new tasks (a save), moves with the division (each task a session, each start a save), the war's
end and the reveal.

## 10. The slices

| Slice | What you can play when it's done | New | Size |
|---|---|---|---|
| O1 | Campaign, rebels, any role: day 0 at Kesmark, the war running, a raid waking you, quit and continue at the last briefing | the battery's state between tasks (15.1), the campaign save (15.7), the menu, the session's path in Unreal (15.8), the director's waits in the pause menu | L |
| O2 | Your map on M: marks with sources and hours; as battery commander, place positions and sectors and give the order from it | the mark model, the map view, decisions to orders, the sector card | M |
| O3 | Drive to the 19:00 briefing and get your order; give your battery's order on the ground; as a vehicle commander, hear it and brief your crew; the 07:00 news | the briefings as scenes with subtitles, marks traced at a briefing, the code table handed out, R0 | M |
| O4 | Choose your spot; follow the covered company; fire within your mode; see your breach in the record | the order's limits as data, NPCs keeping them, the leash to a covered unit | S |
| O5 | The summing up: your grades and reasons; grade your crews; the reveal | the grading rules, trust feeding the tasking, the reveal screen | S |
| O6 | The Pact, NATO and the neutral: the opening march, the moves with the division, new districts | the opening march, regroup and cover-a-regiment rules, bounds, districts (15.6) | L |
| O7 | The officers' redeployments (section 8) | R1, the eight situations as rules, a reserve battery, single launchers detached | M |

A pull request after O3 and after O6.

## 11. How we work this time

One branch (`track-o`), slices committed in order. Done means the owner can play it; each slice ends with a short "try
this" list. Unreal is compiled with UnrealBuildTool. One test per new rule, the area's tests while working, the full
suite once per slice. No new benches, gates, balance passes or replay fingerprints; the air war and the ground war are
not retuned. Research capped: half a day for R0 (in O3), a day for R1 (in O7); anything not found becomes a labelled
game value. Each slice's "As built" in ten lines or fewer.

Not in this track: spoken voices for the officers, the crew's needs (phase 19), G6-G7, the campaign in multiplayer, the
regiment's posts as player roles, saving in the middle of a task, districts generated on demand.

## As built

**O1** (25 Nov 2026). `data/campaign/campaigns.json`: the four adversaries, the rebels playable (the bridges mission,
`pu12m_mission_cover.json`), the others "not yet" until O6; the theatre is the nation's with the fronts; war seed 6 (a
raid near the bridges by 23:00 on day 1). `simcore/campaign/battery_state.hpp`: the battery between its tasks (each
vehicle's place, missiles and stock, the siding, the task held and ready, the hour); `ScenarioDef::task_held` holds a
task from before the session (ready at once, sectors set, the regime said once, the paper in the post, nobody gives it
again). `simcore/campaign/campaign_save.hpp`: a new campaign, the save (the air war's replay save inside it, the task in
its file's format) and the resume; `MissionSession` takes a seam at the evening briefing once the sky is clear and
resumes from a save. Unreal: the menu's Campaign box (1-4 new, C continue; Shift the old screen as the developer's
view), `?SimWar=new:<id>|continue`, `USimWorldSubsystem::StartCampaign` owning the session, the save in
`Saved/Campaign/campaign.json`, the pause menu's W (a wait at `sam3d.WaitScale` until the director wakes the player's
seat or the briefing's save) and R (back to the last save), the campaign's line at the top. Seen in the game by a scripted
run (`-SimMenuTest -SimMenuPick=campaign:rebels:battery_commander`, and `?SimWar=continue`). Left: the Osa's seats in the
campaign (a `BatteryLink` in the session, O3), U2's ground units round the battery (0 made against the rebels), a save
at a task's start, the director following the player to another seat.
