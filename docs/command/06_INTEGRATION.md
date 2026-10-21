# 6. Integration: the air war, the crew, the warning sources, the EM layer, multiplayer, and the screen

## 1. The air war (`simcore/command/air_war_feed.hpp`)

The air war decides what our side's organisation learns; the feed gives each piece an origin in the chain:

| The air war's news | Fact | Origin | For | Precedence |
|---|---|---|---|---|
| The war begins | war (also on the news) | the army's chief of air defence | everyone | priority |
| A raid comes through the battery's sector (the pacing's player mission) | air_warning, "Воздух!" | the national sector's duty officer | everyone, down to the crews | urgent; worthless after the raid |
| The raids the warning net saw | raid_seen | the division air defence CP's duty officer; the battery's own people saw the ones through its sector | the regiment, down to battery commanders | routine |
| The player's fight | raid_result | battery 21's commander; the battery saw it | the regiment's staff | priority (the combat report) |
| The battery moved; the crew's rating | battery_news | battery 21's commander; the battery knows | the regiment's staff | routine |
| An airman picked up | prisoner | the police | the regiment, down to battery commanders (the commandant tells the battery) | routine |
| What a prisoner said; intercepts; the analysts' notes on the enemy's officers | enemy_officer (a paper) | the division's or the army's intelligence | down to battery commanders | routine |
| The regiment's estimate of what the enemy knows of the battery | enemy_knows_battery | the regiment's chief of staff | the battery commander | priority if "consider moving" |
| The foreign press on civilian casualties | press (the news) | the political deputy | everyone | routine |
| An object of ours wrecked | own_damage | the nearest post (within 10 km), seen by everyone within 3 km | the regiment, down to battery commanders | priority for a bridge or the battery |

And the air war shapes the pipeline: every raid is a **danger zone** on the roads around its target while it is over it
(carriers lost: 6 km, 25 %), its bombs **cut the wire** of posts within 1.5 km for 6 hours, and the battery's post and
vehicles **move with the battery** (its position history: `AirCampaign::BatteryTrack`). Game values, in `FeedRoles`.

The air war's own model of what the enemy learns from our radio traffic is still the posture's radio discipline; the
pipeline's radio calls (`InfoNet::RadioCalls`) are the evidence to feed it next (`07` P3).

## 2. The crew (`simcore/crew_knowledge.hpp`, `docs/crew/`)

The crew's knowledge model (what each crewman knows, from their instruments or by voice) is this pipeline at the scale
of one vehicle and one fight; the pipeline is the same rule at the scale of the army and the campaign. The vehicle's
people are posts of the chain (vehicle 21-1's crew); the intercom is "in person" inside a vehicle. Joining them (a
pipeline fact becoming a crew picture entry, the commander's callouts becoming facts) is `07` P4.

## 3. The early-warning sources (`simcore/c2_sources.hpp`)

The battery's air picture in a fight (the PU-12M, the warning radars' voice nets, the division's net, the observation
post, the data link) stays with c2_sources: seconds-scale designations. The pipeline carries the campaign-scale warning
("a raid is coming") and everything else. The same people appear in both: the regiment's CP duty officer, the division's
warning net, the observation post on the Vrbice tower.

## 4. The EM layer (`simcore/em/comms_system.hpp`)

With the EM layer on, a radio hop's delivery, jamming and interception should come from the net model (link budgets,
J/N, ESM). Today the pipeline's radio is a means with a range, a garble rate, jamming windows and an intercept log (`07` P5).

## 5. Multiplayer (`docs/MULTIPLAYER.md`)

Each player's character is a post; the server runs one net and sends each client only its own character's deliveries.
Two players can tell each other things only by the means their posts share (`07` P6).

## 6. The screen, and the world

**Now (interim).** The campaign screen (`ASimCampaignHUD`) shows only the player's character's knowledge:

- "What you know": each delivery with when, who, by what means and where, the full text or the summary ("the paper has
  not reached you yet");
- "Your chain": the player's post, superior, contacts and people, where each is now, the coming briefings, visits and
  courier runs;
- the raids panel and the map's raid tracks: only raids reported to the player; the map's objects without their true
  state, damage only as reported; the district's villages and posts;
- the raid warning: "'Vozdukh!' D1 05:14, Captain Pavel Liska (duty officer, regiment command post), on the radio: a raid
  of ... expected over the sector about 05:24", or no warning at all;
- the header: the battery's own kills, and the regiment's last word on what the enemy knows.

**Next (in the world).** Each delivery happens as a scene the player is in (`07` P1, P2):

| Means | In the world |
|---|---|
| In person | The NPC walks up, salutes, speaks (the crew's voices, `data/crew/voices.json`), hands over a paper |
| Briefing | The regiment's KP tent at 19:00: the chief of staff at the map board reads the summary; the player drives there in the UAZ |
| Runner, despatch rider, courier | A soldier runs up, a motorcycle arrives at the battery; a sealed envelope the player opens in the cabin |
| Telephone | The TA-57 in the PU-12M rings; the player answers |
| Radio | The voice on the R-123M's headset, in Russian with the brevity words |
| Early-warning net, sirens | The R-311's speaker; the town's sirens heard across the fields |
| Post, news | Letters and the newspaper at the sergeant major's run; the state radio in the cabin at 19:00 |
| Papers | Kept in the player's map case: the order, the overlay, the summary; the working map the player marks |

And the other way: the player tells people (talks to an NPC, picks up the handset, calls on the radio, writes a report
for the courier), choosing the means and paying its cost.
