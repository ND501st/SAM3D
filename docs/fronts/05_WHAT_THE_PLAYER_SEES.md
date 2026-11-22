# 5. What the player sees (F5)

First person: the player's character learns where the front is only through reports that reach him
(`docs/command/01_PRINCIPLES.md`), and sees only what is within his sight. This file designs the three surfaces the
fronts reach him by: the situation map his post holds, the units in sight in Unreal, and the campaign screen.

## 1. The situation map

Every post of our chain that receives situation reports (`FactKind::Situation`, F3.8) and orders keeps a **situation
map**: the line of contact by sector as last reported, our units' places and our knowledge of the enemy's, the
priorities and orders it holds, each mark with its source and time, as the PU-12M's working map (roadmap 14.4) keeps
its marks. It is built from the post's facts, never from the ground war's state: two posts holding different reports
draw different lines, and a stale report draws a stale line.

- simcore: `command/situation_map.hpp` (new): `SituationMap` per post, rebuilt from the post's held facts
  (`InfoNet`'s holdings), marks with `source`, `h` and `age`. It is the working map's first form (14.4, G9.1) for the
  front's marks; the working map's other marks join it there.
- What the player's post holds is in `PostView` (`post_view.hpp`), so `PostViewJson` gets the map, and the debrief its
  last state.
- In Unreal the map is a paper on the post's table (the PU-12M's working map, the regiment's map in a session at its
  CP): drawn from `PostViewJson`, marked in the period's colours (ours red, the enemy's blue, as Soviet-pattern maps
  were drawn), never more than the post holds.

## 2. Units in sight in Unreal

Track G's rule: everything within the player's visual range appears in Unreal where simcore has it.
- In a session, the ground war's units near the battery are at U2 (`03_GROUND_WAR.md` §3.3): battalions and
  companies as the session's movers (`nav::SessionNav::AddMover`) on the roads and at their places.
- `nav::VisualRangeM` decides what the player's character sees (day, dusk, night); Unreal draws those movers as the
  basic models (`FSimBasicModels`, `data/scenery/basic_models.json`: tanks, BMPs, trucks, guns), ours never as the
  enemy's.
- The enemy's units are drawn only when within sight; there is no marker for an unseen one.
- What the crew sees of them can be reported (a column passing, the enemy's tanks seen) as the crews report raids now:
  into the records as they saw it.
- The front's sounds at a distance (artillery, the glow of fires at night) are presentation, from the sector's combat
  as the fx feed is from the session's (`simcore::fx`), and tell the player nothing the crew could not hear.

Out of a session nothing is drawn.

## 3. The campaign screen

The campaign screen (`ASimCampaignHUD`, `SimCampaignHUD.cpp`) shows the nation and the war between missions. Today it
draws the theatre from the campaign's state. Under the fronts it draws **our side's knowledge only**:
- the line of contact as our side's highest post the player's character could have learnt it from has it (the
  battery's regiment's map, as a newspaper and the regiment's briefing give it), with its date;
- our units' places from the same; the enemy's only as reported;
- the enemy's air front never: only our staff's conclusion of it (F2.5), if one reached the regiment.

The truth's line goes only into the debrief after the war (the outcomes' ledgers are from the truth for the debrief
only, `campaign/outcomes.hpp`), and into the developer's tools.

## 4. The developer's view

`tools/map_view.py` (the country with the theatre's objects and fronts) draws the truth's line of contact by day, the
air front by sector, the formations, and, beside it, our side's belief as the national sector's post held it, from
`air_war_demo`'s and `fronts_bench`'s JSON. `tools/air_war_view.py` gets the enemy's air front moves in its journal.
These are for the developer and the owner, not the player.

## 5. Data and code

- `command/situation_map.hpp` (simcore); `PostView` and `PostViewJson` extended.
- Unreal: the map paper's widget from `PostViewJson`; the movers drawn by the existing mover and basic model actors;
  `ASimCampaignHUD` reading the player's regiment's situation map instead of the truth.
- `tools/map_view.py`: `--front` layers.

Unreal is the owner's to build and run (`CLAUDE.md`); the simcore side comes first, with its tests.

## 6. Tests

- **Two posts, two maps.** A report that reached the army but not the regiment is on the army's map only.
- **Stale is stale.** With no new report the map keeps the old line and its time, while the truth moves.
- **No truth in the view.** `PostViewJson` for the battery holds nothing the battery's facts do not.
- **In sight.** A U2 mover beyond visual range is not in the session's drawn list; within it, it is.

## Done when

The player's character sees the front only on the maps his post holds and the units within his sight, the campaign
screen shows our side's knowledge, and the developer's tools show the truth beside the belief.

## As built: F5.1 (23 Nov 2026)

The situation map of section 1, per post, simcore only (`command/situation_map.hpp`; the Unreal paper and the campaign screen are F5.3, the
owner's). It changes no simulation: nothing in the war reads it.

**What a map is.** `SituationMap` for one post at the net's hour: each mark with `source` (the post that compiled, ordered or stated it),
`key` (the fact it rests on), `h` (compiled, ordered or stated), `got_h` (when it reached this post), `age_h` and `stale` (older than the
layout's `stale_h`, 14 h: the units' returns come at 06:00 and 18:00 and a few hours late; a game value). The marks:
- **the believed line by axis** (`MapAxis`): the depth and pace of each axis in the latest report that reached the post, whether a unit's return
  or the army's summary (`by_summary`), the enemy in contact as the unit believed him (formations, strength), our counterattack on it; and the
  line itself (`line`, one point per station of the front's base line): the base line with each held axis's depth in the ground war's shape of a
  penetration (frontage, taper), never carried on at the reported pace. A post holding no report of an axis has no mark for it;
- **our units as reported** (`MapUnit`): the latest return per formation that reached the post (its own report, or the army's summary of it): strength,
  supplies, out of the fight, sector, why it reported; the enemy not in contact as the army's latest intelligence summary gave it (`MapEnemy`);
- **orders held** (`MapOrder`): the ground orders that reached the post, newest first, 12 at most; **sector priorities** (`MapPriority`) of each air
  defence commander the post is for or holds an order from (`PriorityKnownBy`), by the order's fact, else the pre-war plan (`source` "plan", `h` -1),
  with the sky's sharing the priority implies (`SharingFor` on the layout's `sky`); the air defence staff's conclusions on the enemy's air front
  (`MapAirFront`, the latest that reached the post); **our SAM units and radar companies** (`MapSam`): the plan for the posts that command or are the
  unit's own, then the last move ordered (`basis` "ordered") or reported ready (`basis` "reported") that reached the post. A post that is neither
  holds no plan for it and shows only the units it has an order on.

**Nothing from the truth, structurally.** `BuildSituationMap(net, feed, layout, post, out)` takes the pipeline (`InfoNet`), the campaign's feed (the
records it made of the facts it put in the net: `GroundFeed`, `AdStrategy`, the watch's conclusions) and the layout, never the campaign, the ground
war or the air war; a record is used only where `InfoNet::Known(post, fact)` says the post holds its fact. `GroundFeed` gained `Intels()` (the
intelligence summary's lines, which were text only) and `AdStrategy` the accessors of which post commands a unit. The layout
(`SituationLayoutOf(campaign, ad, own_air)`, the one function that reads a campaign) takes only what the war does not change: sector, axis and
formation names, the axes' places on the base line, the base line's stations, the order of battle's SAM units' plan places (the theatre's data as loaded,
not where the war has them).

**Cheap.** On demand and cached per post (`SituationMaps::For`): rebuilt only when `InfoNet::Revision()` (new: bumped by every fact added, advance and
replay) or the feed's records change, never per step; a reference stays valid across other posts' builds. A map takes 0.005-0.015 ms to build (2 to 46
marks, `fronts_bench --map`).

**Where it goes.** `PostViewJson(net, post, map, since_h)` (or the older form's new `situation` pointer) adds a `"situation"` object, whole each
time (a mark that aged is a change); `ReadPostView` reads it into `PostView::situation` and `MergePostView` keeps the newer. `SituationMapText` writes it
as a page: `MissionSession::Debrief()` ends with the battery commander's and the regiment's staff's maps (fronts sessions only), and the bench prints any
post's. The truth's line is not in the debrief: it stays the outcomes' and the developer's.

**The nation, every F mode at 1, 10 days, seed 7** (`fronts_bench data --map army_ops,sector_cp_duty,rgt_chief_of_staff,bty21_commander`; the truth is read
by the bench only, for the comparison):

| War, post | Axes held, reported / true depth (km), age | The believed line against the true over 38-44 stations (mean, worst) | Marks |
|---|---|---|---|
| Pact, army operations duty officer | north-eastern road 60.9 / 60.9, Rava gorge 20.0 / 20.0, 10.0 h | 0.00, 0.0 km | 9 (5 units, 2 axes, 1 order, 1 intelligence line) |
| Pact, SAM regiment's chief of staff | Rava gorge 20.0 / 20.0, 10.0 h; the north-eastern road not reported to it | 5.64, 60.9 km | 2 |
| Pact, battery commander | the same as the regiment's (the 7th Division's report) | 5.64, 60.9 km | 2 |
| NATO, army operations | western road 41.4 / 41.7, south of the Lenta 74.9 / 74.9, 10.0 h | 0.03, 0.3 km | 5 units, 2 axes |
| NATO, regiment and battery | western road 40.5 / 41.7, 29.0 h, stale; the Lenta axis not reported | 8.16, 74.9 km | 1 unit, 1 axis |

The posts differ (four posts, four different maps in every campaign): the national sector's post holds the army's summaries only (every axis and unit
`by_summary`; 46 marks with the sector priorities and the SAM units' plans), the regiment and the battery only the division the regiment covers. Where the
unit reported at its last return the line is right to a kilometre; what a post was never told is not on its map (the battery's line is 61-75 km short in
the axis its division does not touch), and what it was told long ago stays old (the NATO division's 29 h old report; the army's one intelligence summary of the
enemy's 9th Tank Division, 214 h old, shown as stale).

**Tests** (`test_situation_map.cpp`, 5): two posts, two maps (the army's, the sector's, the regiment's and the battery's: by the reports and summaries each
holds, a report that reached the army but not the regiment on the army's map only, four different maps, every mark's fact held by its post at the hour the
mark says, the regiment's commander's priorities with the plan's marks and the sharing, no SAM plan at the battery, the move orders' units where they were
sent); stale stays stale (the Pact, hourly for 48 h: 86 marks unchanged an hour on with their depth and hour exactly the same and their age an hour more, the
truth having moved under 41 of them; 8 replaced by a later report only; the stale flag exactly past the threshold; no point of the line deeper than the
deepest reported); the view holds nothing its post's facts do not (the view's JSON read back is the map, every mark's key among the view's items, the truth
run on six hours without the net changing leaves the map byte for byte, a client merges it, the cache builds once for 100 asks and again after a fact);
no fronts, no maps (an empty map, an unknown post); a fronts session's debrief ends with the battery commander's and the regiment's maps.

**Identity.** Nothing in the war reads the maps, and the feed's two additions (the intelligence summaries' lines, the units' accessors) and the net's revision counter change no output: `air_war_balance data 20 7` and `air_war_balance --theatre theatre_pannonia_nation.json data 10 7` are byte-identical (`cmp`) to track F's tip before it (7cab836, built the same way in a detached worktree); the replay fingerprint (`tests/test_routes.cpp`: journal 107, FNV 1375944258174313379, 41 packages) holds; the gates pass (`fronts_bench data --ground --gate`, `--ground --gate --player-sector battery`, `--feed --gate`); all 15 ctest targets pass (`simcore_tests`, 1405 tests, 5 new; the full run 68 min on this machine under load); `python -m pytest prototype tools -q`, 203 passed (one proxy timeout test failed once under that load and passes alone).

**Left:**
- The Unreal paper and the campaign screen (F5.3) are to read `PostViewJson`'s `"situation"`; nothing sends it to a client yet (the view is built by the
  caller that has the feed: a session, the server).
- Our radar companies' and warning posts' own reports of themselves, our wrecks (`OwnDamage`) and the raids as the warning net saw them are not marks
  (the PU-12M's working map, 14.4/G9.1, takes them).
- The believed line is by axis (the base line less the reported depth, in the ground war's shape of a penetration): the line by station is not a fact
  (F3.8's left). A post sees the enemy only as the units believed him per axis and in the army's intelligence summary; no enemy unit has a place.
- A commander's sector priorities are shown to the posts he is for or whose orders reached them; the air defence staff's conclusions only where the post is in
  their audience (F2.5).
- The debrief prints the maps as text and does not set the truth's line beside the belief (that is `fronts_bench --map`'s and F5.3's `tools/map_view.py --front`).
