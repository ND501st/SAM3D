# 4. The pipeline: how a fact travels

Code: `simcore/command/info_net.hpp` (`InfoNet`). Tests: `simcore/tests/test_command.cpp`.

## 1. A fact

| Field | Meaning |
|---|---|
| key | Unique and stable: where it came from ("journal:12", "raid:3", "wreck:kesmark_road_bridge:3150") |
| kind | war, air_warning, order, intsum, raid_seen, raid_result, own_damage, prisoner, enemy_officer, enemy_knows_battery, battery_news, press, rumour, mail, intercept, own_air_notice, air_front, situation |
| h | When its origin knew it (campaign hours) |
| origin, witnesses | The post that first knew it; others who saw it themselves at that moment |
| text, summary | The full text (the paper), and what voice carries (if different) |
| audience, need level | The tree of the chain it is for (a post; empty: everyone) and how far down (`02` §1) |
| precedence | 0 routine, 1 priority, 2 air raid |
| document | A paper: its full text goes only by hand or teleprinter; voice carries the summary and the paper follows |
| broadcast | Goes out at the next news bulletin to everyone it is for |
| expires | Nobody passes it on after this (a warning after the raid) |

## 2. The route

Who it is for: every post in the audience's tree at or above the need level. The route is the shortest way along the
chain from the origin and the witnesses to each of them: superiors cost one hop; the people a post deals with directly
(its links) cost one hop for urgent and priority facts and three for routine ones (routine news goes through the staff).
Every post on the way learns it as it passes. **Nothing passes through the player's post on its own**: the player is a
leaf unless they saw it themselves.

## 3. A hop

At the moment a post has the fact, it takes its own time to pass it on (30 s urgent, 3 min priority, 10 min routine,
times its holder's skill: master 0.7 .. recruit 1.8), then:

1. **Together**: the two are at the same place at that moment (`05` §3): in person.
2. **Urgent**: the earliest of every means the pair shares (`03`).
3. **Priority**: the wire if whole, else the radio, else a despatch rider.
4. **Routine**: the earliest **contact** within 30 h:
   - a meeting: a briefing or a visit that puts both at the same place (or both back at their posts after one);
   - a courier run that stops at the sender's place and later at the receiver's;
   otherwise as priority.

**Losses.** A garbled radio or telephone message is repeated (6 min later). A carrier on the road (runner, despatch
rider, courier, post) is checked when it arrives: if its route passed through a raid's danger zone while the raid was
over it, it may be lost, and the message goes again an hour later (three tries). The telephone is out while either end's
wire is cut; the radio while jammed.

**Papers and summaries.** Voice and telephones carry the summary; a document's paper follows by the next contact (the
receiver's knowledge is marked full when it arrives).

## 4. Time and determinism

The net is a pure function of its time-stamped inputs: the facts, the danger zones (known from the raid's take-off),
cut wire (from the bombs' time), jamming, and the battery post's moves. Each fact runs on its own; every decision at an
hour uses only the inputs known by that hour; randomness is a hash of the fact, the hop and the attempt. So advancing in
steps or at once gives the same deliveries (tested), and a saved campaign rebuilds its net exactly from the campaign's
replay: nothing about the net needs saving.

## 5. What the game asks it

| Query | Use |
|---|---|
| `KnownBy(post)` | What a person has learned, in order, each with when, from whom, by what means, where, and whether they have the paper |
| `Known(post, fact)` | Does this person know this? (the map's raids and damage, the raids panel) |
| `PreviewDelivery` / `WhenKnown` | When and how a person will learn a fact if nothing changes (the raid warning's lead time) |
| `PlaceOf(post, h)` | Where a person is (a briefing or a visit takes them away from their post) |
| `NextContacts(post, h, horizon)` | The coming briefings, visits, courier runs and bulletins a person is part of |
| `RadioCalls()` | Every radio call made: what the enemy's intercept service heard |
| `Describe(delivery)` | "D2 19:20, Major Imre Sallai (Chief of staff ...), in person at the regiment command post" |

## 6. What our radio intercept service puts in (track B, B9)

A new kind of fact, `FactKind::Intercept`, is what our posts heard of the enemy's air radio and read
(`simcore/command/intercept.hpp`, `docs/air_battle/09_COMMS_AND_INTERCEPT.md` section 5). Its origin is the post's air
defence liaison (`ad_liaison_1`, `ad_liaison_2`) or, for the hours warning drawn from the enemy staff's opsec, the
intercept centre (`intercept_centre`, under `army_intel`). A call that names a time and a place is a fact of precedence 2
when the place is within `area_m` of the battery, else 1; it is meant for the division's air defence chief's tree down to
the battery commander (`need_level` 3), carried by the liaison's direct telephone to `div_ad_cp_duty` and from there by the
warning net or the regiment's command net. Its text is what the linguist wrote on the form, slips and all; the truth of the
call is in the call log for the debrief only. A heard report that names the battery's square is also an
`EnemyKnowsBattery` fact for `rgt_chief_of_staff`. Everything the posts logged, read or not, goes into the day's summary
(`FactKind::Intsum`, a document from `army_intel`, routine, down to the battery commanders). In a session
(`campaign/mission_session.hpp`) the fast road's arrival is said on the battery's net (`ScenarioRunner::RegimentSays`) and
written in the radiotelephonist's journal as heard; the summary comes as a paper by the means the pipeline picks
(`ScenarioRunner::AddDeliveries`). Nothing appears on a panel.

## 7. What our side learns of the enemy's air front (track F, F2.5)

The enemy moves his air front by sector in his own mind (`docs/fronts/02_AIR_FRONT.md`); our side sees only its effects.
Where the air war runs it (`air_front.mode` 1) the feed (`CampaignFeed`) sends the army's air defence staff
(`army_ad_staff`, under `army_ad_chief` at the army's headquarters) its signs, each a fact for that post alone
(`need_level` 0): the division's air defence CP's report of a raid the warning net saw attack our troops at low level by
day, or with helicopters, deeper beyond the line of contact than his close support or helicopters usually work
(`RaidSeen`, priority, key `front_watch:raid:<n>`); the intercept centre's form of a close support request heard on his
request net naming a square that deep (`Intercept`, routine, `front_watch:intercept:<n>`); our troops' reports of air
attack (`Troops`, `CampaignFeed::ReportAirAttack`, for F3.8). The staff counts each sign by sector and day when it reaches
it (`command/air_front_watch.hpp`), and once enough fall in a sector within a few days it states, after its working
delay, a new kind of fact, `FactKind::AirFront` (`air_front_estimate:<n>`, subject `air_front:<sector>`): a document of
priority from `army_ad_staff` for the division's chief of air defence's tree down to the battery commanders (`need_level`
3), saying how much deeper the enemy's close support and helicopters now work there. `CampaignFeed::AirFrontKnownBy(post,
sector)` gives the estimate a post holds, from the latest conclusion that has reached it: what the regiment's tasking and
F4's sector priorities read. Nothing reads the enemy's depths.

## 8. What our side learns of the ground war (track F, F3.8)

Where the ground war runs (`ground.mode` 1) our units' reports are facts (`command/ground_feed.hpp`, `GroundFeed`, a member
of `CampaignFeed`, `docs/fronts/03_GROUND_WAR.md` section 10 and "As built: F3.8"). A formation's staff compiles its combat
report twice a day (06:00 and 18:00 local) and at once when it reports, asks to be relieved, stops its attack, withdraws,
breaks or re-forms: `FactKind::Situation`, priority, key `sitrep:<n>`, subject `situation:<unit>`, from the unit's duty
officer (`div_chief_of_staff`, `div9_staff_duty`, `swbde_staff_duty`, `tankbde_staff_duty`; the army's own brigades report
from the army's operations section) for `army_ops` and, for the SAM regiment's division, `rgt_chief_of_staff` and
`bty21_commander` on the way down. A new field, `Fact::to`, names a fact's addressees when it is for posts that are not a
tree (`audience` and `need_level` are then not read); the posts along the pipeline's way only pass it on, and so hold it
(the garrison commandant, the regiment's commander). The report says what the unit believed: its strength and supplies as
its staff counted them, the axes it reports (depth and pace as it knows them), the enemy it believes it faces, and the air
attacks it counted since its last return (the picture's own errors, `ground_war/picture.hpp`). The army's operations
section sums the latest return of each unit that has reached it, twice a day (09:00 and 21:00 local), for the national
sector's post and the chief of air defence (`Situation`, `sitsummary:<n>`, made only of what the section held by then);
the army's intelligence summary of the enemy it does not touch is an `Intsum` from `army_intel` (`ground_intsum:<n>`);
an order our commander gives a unit (withdraw, relieve, counterattack) is an `Order` from his staff for the unit, the
chief of air defence and, for the regiment's division, the regiment and the battery (`sitorder:<n>`). A struck unit's
air attack is passed to `CampaignFeed::ReportAirAttack`, so the army's air defence staff's watch (section 7) sees it as
a sign when it lies deeper than the enemy's close support usually works. F4's commanders and the regiment's tasking read
what a post holds (`GroundFeed::LatestKnownBy`, `LatestAxisKnownBy`), never the ground war's state.

