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
| Another division under heavier air attack | the regiment, or some batteries, sent there for some days | AD massed on the main axis (`vr_1981_fanfule_army_ad_defence`); Egypt 1973, armour beyond the SAM umbrella | the Pact, NATO |
| Intelligence names an asset about to be struck | a battery or a launcher sent to cover it for a few days | Vietnam 1965-72; Libya 1986; moving fire units from less important directions (`vr_1982_volny_ad_restoration`) | all |
| A battery worn down (fatigue, faults, missiles) | relieved by a reserve battery, rested and repaired | restoration from depth (`vr_1982_volny_ad_restoration`, which also says SAM reserves were usually not formed: the reserve battery is ours) | long wars |
| The regiment's radars or post destroyed, contact lost | batteries or launchers fight alone, roving | Syria 1982; `pvo_bu_ix_1984`, `vr_1985_plss`; flagged hindsight: Iraq 1991, Yugoslavia 1999 | NATO, the Pact |
| A breakthrough or encirclement | withdraw alone to the next line or friendly force | units cut off (`vr_1980_krizek_encirclement`); hindsight: Iraq 2003 | the Pact, NATO |
| A deep-strike campaign on strategic assets | batteries lent to the national air defence | Hanoi and Haiphong 1967-72; one zone-and-object defence (`vr_1985_jurica_ad_on_the_move`); a troops' battery lent: not found, game values | NATO, the Pact's long-range aviation |
| A special mission: an ambush, a convoy, a bridging | a battery or launcher detached, radars silent until the moment | Egypt 1970; fire ambushes (`vr_1985_jurica_ad_on_the_move`); Vietnam 1966 (`nhandan_2025_tan_bn61_1966`); hindsight: 1999 | NATO, the Pact; the rebels' helicopters |
| Rebels: few aircraft, many helicopters | launchers split singly over villages, roads, convoys | Angola 1987-88, flagged hindsight (`kuito_memoirs`: Strela-10s split over a brigade's area; singly over villages not found, game values) | the rebels |

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

**O2** (26 Nov 2026). `simcore/command/held_map.hpp`: `HeldMark`/`HeldMap` (side, shape, label, source, means, hour,
certainty, copied or decided), `GridSquare`, contours by marching squares, `BatteryDecision` and `CheckDecision` (breaches
listed, nothing refused; `decision_leeway_m` 1 km and the drawing values in `data/gameplay/held_map.json`, game values),
`SectorCard`. `command/held_situation.hpp`: F5's situation map copied only where the holder's `InfoNet` delivery exists,
its sender, means and hour stamped. The runner's `HeldMapOf` (the task from its order book, his decision, a launcher
commander's card), `SetBatteryDecision` (the combat order marches to a "point" anchor at the decided main, the decided
sectors given), sector cards as papers in each launcher commander's map case; `MissionSession::HeldMapFor` adds the
situation. Unreal: M in the cabin HUD (`SimHeldMap.cpp`) draws only that map as a 1:50 000 sheet; the battery commander
drafts on it and gives the order with Enter. `-SimHeldMapShot=<png>` looks once. Left: a handwriting font, reported raids
and launches as marks, routes and launch zones, the regiment's tracing, map case O and the PU-12M's table.

Try this: `?SimWar=new:rebels?SimSeat=battery_commander`, wait for the regiment's task (W), press M. Hover a mark for who
told you and when. Press 1 and click 1.5 km from the order's main: the breach is listed, not refused. S, then two clicks,
draws launcher 21's sector; leave a gap and see it listed. Enter gives the combat order; the battery marches to your point.
(A launcher commander's M shows his sector card once the Osa's seats are in the campaign, O3.)

**O3** (26 Nov 2026). `data/campaign/briefings.json` and `simcore/campaign/briefings.hpp`: the regiment's evening
briefing (seven items, each by a named officer of the chain, and three papers), the combat order's items, the crew's
briefing and the 07:00 news. `MissionSession` (with `briefings`): the battery commander drives to the Lazne woods and
back as a nav mover (his UAZ on the truck class's roads; the NPC leaves through `NpcDoes`, a player by `order_to_briefing`).
Day 1 opens with him there and the task given in person. The lines are subtitled scene lines (`ScenarioRunner::SceneLines`).
The code table, the plan of cooperation and the call-sign table go into his map case. A missed briefing goes into the notes
and the record, with no papers. The runner's combat order goes one of three ways. At the map table or on the ground, the
commanders are called, the items are spoken, and the cards are handed at the end. By radio in short form (`order_battery_combat_order_radio`, or the NPC when away and the ready-by is near), each
commander writes his own notes: no landmarks, no reserve, no codes (D5), and the enemy's intercept hears it as an extra
transmission. The session's Osa (`osa_mission_cover.json`, `osa_mission` in campaigns.json): one sky, its commander walks
to the gathering and gets his card only if he is there; `order_crew_briefing` on the intercom tells each listener (`CrewKnows`).
Unreal: `?SimSeat=osa:commander` plays the Osa, M shows its card, and the campaign line shows the subtitles. R0 was not
carried out (the research agent was stopped); its items are labelled game values.

Try this: `?SimWar=new:rebels?SimSeat=battery_commander`. Watch the briefing's subtitles and the drive back ("You are on
the road..."), then open your map case. Back at the post, give the combat order: the orders sector for the map table, the air sector on the ground, the emission
sector by radio. With `?SimSeat=osa:commander`, walk over when called (the fire sector), press M for your card, and brief
your crew from the search sector.

**O4** (26 Nov 2026). The order's limits as data: a task's `"limits"` (`campaign::OrderLimits`: spot and launcher-spot
leeways, the covered unit, the leash, the fire mode), the leash 3 km and following 1.5 km behind (voenizdat_osa_1991 pp.
232-239), the covered unit's reports every 10 min and a 15 min grace (game values, `held_map.json`). The sector card
carries each launcher's leeway, fire mode and emission ceiling. The runner: the covered unit (a nav mover) says its grid
on its net and the post sees it within the visual range; nothing else tells the battery commander where it is. Beyond
the leash the NPC gives `order_battery_follow` through `NpcDoes`: a march on the roads behind it, then «К бою!». The Osa's
NPC commander on designation fires only at what the post gave him, and reports his spot (`report_spot`); `ChooseSpot` drives
the vehicle to the spot chosen. Nothing refuses the player: the commander's log writes spots, positions, the covered unit's
place, moves, regimes, fire modes and launches; `campaign::CheckLimits` lists the breaches in the debrief. Unreal: the map draws the
leash, the covered unit, the spot area and the fire and emission notes; P then a click picks the Osa commander's spot.

Try this: the missions menu's "PU-12M: cover the company on its move" at the battery commander's seat. Watch the company's
grid reports on the covered net, then stay put after 20:30: the helper shows the follow order and the debrief lists the
leash breach. With `?SimWar=new:rebels?SimSeat=osa:commander`, press M once you have your card, then P and click 2 km
away: your crew drives there, you report it, and the post's record shows your spot outside the leeway.

**O5** (26 Nov 2026). At 19:00, before the seam, the regiment's commander grades the day 5 to 2 from what reached it
(`campaign/grading.hpp`'s `GradeDay` on `ScenarioRunner::DayRecordSince`; bands and lines in `data/campaign/grading.json`):
readiness as reported against the ready-by, journal and board against the radar company's log (P9), the limits' breaches in
the commander's log (O4), kills claimed against that log (`CheckClaims`). Each mark is said with its reason after the
summing-up item; the day's mark is the mean, held to one above the worst; marks and trust live in `RegimentStanding` and the
save. Two tasking rules read them (`fire_mode`): a 5 with trust at least 0.85 puts the launchers on their own; a 2, or
trust under 0.7, on designation. The battery commander grades each crew from his log 1.5 h before the briefing
(`order_grade_crews` through `NpcDoes`; `SetCrewMark` for a player's own, no Unreal UI yet), into his log and the save. The
reveal (`campaign/reveal.hpp`) sets the records beside the ledgers and the enemy's journal. The late battery was two
planning bugs, fixed: the cover task marched to the covered unit, not behind it; an in-person order left out the gathering.

Try this: `?SimWar=new:rebels?SimSeat=battery_commander`. Play to the second evening's briefing and hear the marks and their
reasons. Then press Esc and V for the reveal. After 17:30 at your post the helper shows the grading order in the warning
net sector of the crew menu. Headless: `mission_demo data/scenarios/pu12m_mission_cover.json data --war rebels 1 --hours 26 --reveal`.

**O6** (26 Nov 2026, first form). The Pact, NATO and the neutral are playable (`campaigns.json`). Each war opens at
19:00 with battery 21 in the Kesmark barracks' park, `pu12m_mission_<war>.json` and `osa_mission_<war>.json`, the
division's columns leaving Kesmark (nav movers, for show) and the task given in person at the briefing: march to the
war plan's area and cover its object (`tasks/bty21_cover_dam|dombas|south.json`, ready-by 10, 6 and 15 h, measured).
The commander plans a night march: about 7 h to the Rava dam, 3 h to Dombas and 12 h to the south. The districts
(`maps/kesmark_rava_dam|dombas|south.json`) `"extends"` the test map with one 20 m area, tracks and a village. Their tracks
join the country's roads where they end on them (`RoadGraph::ForCountry`; the ground war's graph is unchanged). A task's
`regiment_cp` moves the regiment's post as a column after the briefing. From its arrival, the post, its radars and the
briefings stand there (`ScenarioRunner::MoveRegiment`; kept in the save). A halt on a long march is not its end: no «к
бою готова» there. Unreal draws the scenario's own map. Left: the second echelon, cover-a-regiment and bounds rules (O7).

Try this: `mission_demo --campaign warsaw_pact data --hours 30` (also `nato`, `neutral`; `rebels` unchanged). In the game,
the menu's Campaign box (2 the neutral, 3 the Pact, 4 NATO) at the battery commander's seat: hear the task at the
briefing, watch «К маршу!» at 19:35, then W through the night's halts to «в назначенный район прибыла» at the dam; the
ground round you is the district's. `?SimWar=new:nato?SimSeat=osa:commander` rides the march in launcher 21.

**O7** (26 Nov 2026, first form). `tasking.json` has twelve new rules: the eight situations, O6's second echelon, the cover of
a first-echelon regiment, bounds, and a loan's end. Each is read by `RegimentTasking` only on facts in the regiment's picture:
the warning net's raids, the ground war's Dispersed, attack and counterattack orders, and another sector raised to defend,
all as `RegimentWatch` had them reach its posts; the battery's own ready report with its missiles; and its radars off the
air. Score: the adversary's weight times (1 - departure x (1 - the commander's willingness)) from his traits (R1's sources in
`SOURCES.md`). A task's `"detached"`: each launcher hears its order on the battery net, drives on the roads (a mover,
`osa_9a33`), deploys and fights on its own, and the player's Osa hears it is the senior man there. The relief: the reserve's
column from the barracks, a fighting neighbour on arrival, the march to rest, then repairs and missiles.

Try this: headless, `simcore_tests "redeploy"`. In a rebels campaign, wait (W) for helicopter raids: by the next briefing
the launchers split over Vrbice, Horka, Dubník and Polány; with `?SimSeat=osa:commander` you drive to Vrbice and are told
you are on your own. Against NATO, wait three days in line for the 25th Battery's column and the march to the rear.
