# 4. The pipeline: how a fact travels

Code: `simcore/command/info_net.hpp` (`InfoNet`). Tests: `simcore/tests/test_command.cpp`.

## 1. A fact

| Field | Meaning |
|---|---|
| key | Unique and stable: where it came from ("journal:12", "raid:3", "wreck:kesmark_road_bridge:3150") |
| kind | war, air_warning, order, intsum, raid_seen, raid_result, own_damage, prisoner, enemy_officer, enemy_knows_battery, battery_news, press, rumour, mail |
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
