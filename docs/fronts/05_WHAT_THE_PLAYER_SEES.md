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

## As built: F8.3 (25 Nov 2026)

Section 2, units in sight, built as task F8.3. Simcore gained one new header; the Unreal side is written and compile-checked by reading only
(the owner builds and looks). Nothing in the war or the session's fight reads any of it: `air_war_balance`'s two runs are identical to the
base, the replay's fingerprint holds (journal 107, FNV 1375944258174313379, 41 packages), the four `fronts_bench` gates pass.

**U2** (`ground_war/session_units.hpp`, new). `U2Near` takes every regiment at U1 (F3.6) of a formation not spent, within a reach of the
battery, down to its battalions and companies: each equipment type of the regiment's own tree with a weight in the ground war's tables and at
least 6 of it, in battalions of about 40 (a Pact tank battalion 31-40, a motor rifle battalion's BMPs 30-43, a NATO company team 14-17:
`fm_100_2_3_1984`, `fm_71_2_1977`, rounded; under 20 vehicles a company), each with its vehicles now (its share of the regiment's
establishment times `RegimentFraction`), spread across at most 6 km of the regiment's run of the line at its echelon's depth behind the line
of contact as the war has it now (ours behind the line pushed in, the enemy's behind its axis's tip: `AssignStations`' rule). `SessionUnits`
makes each one the session's mover (`nav::SessionNav::AddMover`, kind `column`, flagged `enemy` and `ground_war`), deployed in line along
the line of contact at 50 m spacing; when the war moves its place more than 1 km, it marches there as a column at the ground war's own march
speeds (the march table's day and cross-country middles, its night factor), on the roads when the move is 3 km or more and the session has
roads, else straight across country; a unit no longer near drops to M0 (not listed, not stepped finely) and comes back when near again, so
the movers' order, and Unreal's ids, never shift. `MissionSession` runs it with the fronts on (`MissionSessionDef::fronts`, a theatre with a
country map), every 60 s of the session, the reach `MissionSessionDef::u2_radius_m` (20 km, a game value; `mission_demo --u2-km`).
Nothing is made outside a session, nor with the fronts off.

**What the player sees.** The session's rule for every mover: `SessionNav::Visible` lists only those within `nav::VisualRangeM` of the
player's vehicle (8 km by day, 3 km at dusk, 1.5 km at night, the weather's visibility the cap), so an enemy's battalion out of sight has no
mark anywhere. `Seen` and `ScenarioRunner::WorldMover` carry `enemy`. **Ours are drawn by our equipment's models, the enemy's by its own**
(`U2Model`): 26 new basic models in `data/scenery/basic_models.json` for every enemy ground type of the four adversaries (NATO's M1, M60A3,
Leopards, M2/M3, Marder, M113, M109, M110, MLRS; the Pact's T-72, T-64, BMP-2, BTR-70, 2S1, 2S3, BM-21; the neutral's SK-105 and
Saurer 4K4F; the rebels' technicals, 82 mm mortars and SPG-9), and where an enemy type is also ours (the Pact's BMP-1, the rebels' T-55
and BTR-60) its faction's variant, `<type>_<faction>`, in its faction's colours: never one of ours.

**Kept out of the fight.** A U2 mover is not in the session's air battle: the runner's detail levels (`near_points_`), the pilots'
perception and the mission bridge's targets of opportunity skip `nav::Mover::ground_war`. The war's strength is never read back from
them. (An enemy pilot attacking a column he sees is the ground war's own business at U1, not the session's.)

**The front from afar** (`DistantFire`, `FireGlows`; `fx::EventKind::DistantFire`, `FireGlow`, new). Every 20 s each attacking or
counterattacked axis's tip is a salvo of 6 rounds over 20 s; its level at the battery is the burst's peak (`fx/explosions.hpp`, 6 kg of
charge, a surface burst) less the air's absorption of its 125 Hz thud (`fx/acoustics.hpp`, ISO 9613-1, the scenario's weather), and it
carries its retarded time (`end_t_s`, when the sound reaches the battery at the speed of sound in that air) and its level (`level_db`).
One under 70 dB or past 50 km is not pushed. At night, every 5 min, a fire's glow (15 min) at a tip within 30 km and the weather's
visibility. Game values in `FrontFxValues`. They go into the runner's fx feed; nothing in the simulation reads it.

**Unreal** (written, unbuilt):
- `SimWorldSubsystem.cpp`: the movers in view set `bFriendly` from `enemy`, so U2's movers are drawn by the track G mover code that already
  draws every listed mover as its basic model (`FSimBasicModels`), ours as friendly.
- `SimFxAudio.cpp`: `DistantFire` plays its rounds as blasts spread over the salvo's span from its emission time (the audio scene adds its
  own delay and absorption from the distance), jittered 150 m about the tip.
- `SimFxEffects.cpp`: `FireGlow` is a fire at the ground under the tip, four times a wreck's fire, burning to its end and without smoke;
  `DistantFire` draws nothing.

**What the owner should look at.** The mover models' colours and sizes for the new types; that ours and the enemy's are told apart (friendly
vs hostile) as the other movers are; the salvos' loudness and spread at 20-50 km (the audio scene's own range limit for one-shots is 60 km);
whether the glow reads as a distant fire at night or needs its own effect. **No game path runs a `MissionSession` yet** (the missions menu runs
the runner straight; the campaign's raids have no roads), so nothing of this appears in the game until one does; and **battery 21 stands
about 100 km from the nearest regiment** in every war on its default days (the Pact, NATO, the neutral and the rebels on days 4 and 8, the Pact also on day 3:
no unit within 20 km, no axis's tip within 50 km), so even then a mission on an axis is needed to see or hear them. `mission_demo --war
warsaw_pact 1 3 --theatre theatre_pannonia_nation.json --fronts --u2-km 150` shows them made (52: 28 ours, 24 the enemy's).

**Cost.** A 2 h Pact session on day 3: 24.7-25.1 s without U2, 25.0-25.3 s with 52 U2 movers held (about 1 %).

**Tests.** `test_ground_levels.cpp` (+4): U2 near the battery from a hand war (ours by our models, the enemy's by its variant, vehicles by
the regiment's fraction; none far off, none once the levels come down to U0); the movers deployed in line, the enemy's listed only within
sight by day and by night, none out of need, a moved place marched; the front from afar (retarded time 20 km / c, the level's formula,
farther quieter and later, the deaf gate, deterministic per step and seed, glows only at night and within the visibility); a played mission
with the fronts makes them, without the fronts none. `test_scenery.cpp` (+1): every enemy ground type of every adversary has its model, never
one of ours.

**Decisions.**
- No crew sighting reports: no seam exists today for a crew's sighting of a ground unit into the records (the crews report raids through the
  radiotelephonist's journal only); left.
- U2 is presentation and place only: it neither fights nor feeds the war, so the air war's and the ground war's outcomes are unchanged.
- A mover out of need drops to M0 rather than being removed, so ids stay stable for Unreal.

**Left:**
- Unreal is unbuilt and unseen; a game path that runs a `MissionSession` with the fronts (the campaign's mission, M-F's playthrough).
- A mission in a sector with an axis (battery 21's has none on any default day).
- The crews' sightings of a column or the enemy's tanks into the records as they saw them (section 2's fourth point).
- U2's units do not take cover, dig in or disperse; they stand in line or march as a column. Ours and the enemy's never meet at U2: the
  contact is fought at U1.

## As built: F5.3 (23 Nov 2026)

The campaign screen of section 3 and the developer's view of section 4. Simcore gained one new header and a bench option; the Unreal side is written and
compile-checked by reading only (the owner builds and looks). It changes no simulation: nothing in the war reads any of it.

**The screen's source** (`command/campaign_situation.hpp`, new; `CampaignSituation`): the layout (`SituationLayoutOf`, the war's static data) and the cached
maps (`SituationMaps`) for a caller that holds the campaign, the net and the feed. `RegimentMap` is the map of the feed's regiment post
(`feed.Ground().Roles().regiment`, as `MissionSession::FrontsDebrief` finds it); `MapFor` any post. **With the fronts off** (`feed.Ground().On()` false: no ground
war) both give null, and nothing is built. The layout is rebuilt once when the strategy turns on at the first pump. Nothing here reads the campaign's state.

**Unreal.** `USimAirCampaignSubsystem` already kept the net and the feed (it pumps them after every step); it gains `GetRegimentSituation()` (a `CampaignSituation`,
reset with the net in `StartInfoNet`). `ASimCampaignHUD::DrawMap`, where that is non-null (any front mode on):
- the enemy's entry points and the other fronts are not drawn (they are the enemy's air plan); the base line is drawn faintly and over it **the line of
  contact as the regiment holds it** (`SituationMap::line`), red where its sector's report is current and dim where every axis report of the sector is stale, each
  axis labelled with its depth and the hour of the report ("(stale)" past 14 h);
- our SAM units that move on orders are drawn where the regiment holds them (the plan faint, a move ordered or reported ready filled and labelled with the hour),
  not where the war has them (`Truth()`); the player's own battery, the towns and the fixed sites are as before;
- a paper in the map's lower left: our units' strength and the enemy as reported, each with its hour, its age and "stale", then the air defence staff's conclusion
  on the enemy's air front by sector if one reached the regiment ("No staff conclusion ... has reached the regiment" if none). The enemy's air front is never drawn.
With the fronts off, `Sit` is null and every branch is skipped: the screen is as it was.

**`tools/map_view.py --front fronts_map.json`** (with the country's dump): two country drawings side by side, the truth (the line of contact on each day, paler for
older days; the air front by sector as the line plus its depth; the formations where they are) and the belief of a post (`--post`, default the national sector's: the
line as reported, dashed and pale where stale, each axis with its hour and age, the staff's air front conclusion, our SAM units as it has them, its units and the
enemy as a list, and the mean and worst distance of its line from the truth's). `--run`, `--day`, `--png` (two files). `fronts_bench ... --map post,... --map-json
out.json` writes the file: per run the base line, sectors and stations, the truth at the end of every day (when this option is given the campaign is stopped at each day's
end to read it; the bench's printed numbers are the same) and each post's map (`SituationMapValue`).

**Tests.** `test_campaign_situation.cpp` (2): the regiment's map is the one the regiment's post builds from its facts, byte for byte, every mark's fact held by the post,
the air front only as a staff's conclusion with its key and hour, cached between asks, the army's map different, an unknown post null, an hour on the marks as old
or older; with the fronts off, no map and no build. `tools/tests/test_map_view.py` (+3): the truth's drawing (last day red, earlier days paler, the air front, the
formations, a spent one not drawn, an unknown day refused), the belief's (stale dashed, no truth in it, the plan's mark not a move, the error against the truth
exact on a small fixture, an unknown post lists the posts), the page and the CLI.

**Left:**
- Unreal is unbuilt and unseen: the owner builds and looks at the screen with a campaign under the fronts and with them off.
- A ground unit has no place on the regiment's map but its sector, so the screen lists the units and the enemy instead of marking them; the line by station and the
  enemy's units in contact as places are F3.8's and F5.1's left.
- The paper does not scroll and shows 14 lines; the sector priorities, orders held and radar and warning posts' marks are not on the screen (they are the PU-12M working
  map's, 14.4).
- The sky's sharing is not given to the screen's layout (no own-air definition is loaded by the campaign subsystem); the screen does not show it.
- The campaign screen reads the regiment's map; the player's own post (`GetPlayerPost`) may differ and its map is not used: the regiment is the highest post the
  player's character could have learnt it from (section 3).
- `map_view --front` draws the end of a headless run, not a live campaign; nothing sends the live campaign's maps to the developer's tool.
