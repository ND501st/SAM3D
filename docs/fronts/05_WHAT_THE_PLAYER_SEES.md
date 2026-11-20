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
