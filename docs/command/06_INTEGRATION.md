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

**From the records (principle 9; planned, `07` P9).** Today `raid_result` and `battery_news` start from the fight as the
simulation saw it. They are to be compiled from the battery's records instead: the radiotelephonist's journal
(`simcore/radio_journal.hpp`: what was said, what he wrote, what he lost), the board's marks and the commander's log. A
raid's time, route and size in the combat report are the journal's; a ghost track on the board is a raid reported; a raid
nobody wrote down is not in the report at all. `raid_seen` likewise comes from the regiment's own records of the warning
net. What the regiment and the division then believe and plan (`docs/campaign/05_THE_SYSTEMS.md` §11: the regiment's picture and its next order) follows.

**What the enemy hears (P3, built 29 Oct 2026).** The air war's radio intercept hears the calls the battery's people
actually made on the radio: every relay the pipeline sent by radio and every call the player made
(`CampaignFeed::BatteryCalls`), handed to the campaign after each step (`AirCampaign::NoteTransmissions`, a recorded
action, so a save replays them). One call on the battery net is one transmission however many hear it; each gives the
enemy's direction finders a fix with chance `intel.comint_per_call` (0.3) times the means' intercept share. From the
first note the posture's radio discipline no longer stands for the traffic (headless runs without the pipeline keep it).
A silent battery gives the intercept service nothing; one that radios every piece of news is heard
(`test_command.cpp`). Left: the session's own radio log (the crews' talk in a played raid) as calls too.

## 2. The crew (`simcore/crew_knowledge.hpp`, `docs/crew/`)

The crew's knowledge model (what each crewman knows, from their instruments or by voice) is this pipeline at the scale
of one vehicle and one fight; the pipeline is the same rule at the scale of the army and the campaign. The vehicle's
people are posts of the chain (vehicle 21-1's crew); the intercom is "in person" inside a vehicle.

**Built (P4, 29 Oct 2026).** The two are joined at the vehicle's edge, both ways:

- *In.* A delivery that gives a raid's direction (`"azimuth_deg"`, which the campaign's raid builder writes on the
  pipeline's air warning) becomes an entry in the picture of each crewman who heard it (`KnownTarget` `warning:<id>`,
  a true bearing, no range, "told by" its sender). It is known as told: the commander turns the turret to it because
  the regiment said so, not because the radar has it.
- *Out.* The crew's calls in a fight (each launch, and each result its judge called: destroyed, missed, not seen;
  `ScenarioRunner::CrewCalls`) become its vehicle commander's report (`command/session_reports.hpp` `CommanderReport`), a
  fact that starts with him at the hour of his last call and goes up the chain like anyone's. It is the crew's word,
  right or wrong, never the truth (`01_PRINCIPLES.md` 9). When the player's character is the vehicle commander, the
  report is his own and goes where he tells it; it is saved with the player's inputs either way, since the campaign
  cannot make it again.

## 3. The early-warning sources (`simcore/c2_sources.hpp`)

The battery's air picture in a fight (the PU-12M, the warning radars' voice nets, the division's net, the observation
post, the data link) stays with c2_sources: seconds-scale designations. The pipeline carries the campaign-scale warning
("a raid is coming") and everything else. The same people appear in both: the regiment's CP duty officer, the division's
warning net, the observation post on the Vrbice tower.

## 4. The EM layer (`simcore/em/comms_system.hpp`)

Without a radio model the pipeline's radio is a means with a range, a garble rate, jamming windows and an intercept log.

**Built (P5, 29 Oct 2026).** With one (`InfoNet::SetRadioModel`; `command/em_radio.hpp` `EmRadio`), each radio hop is
worked out the way the EM layer's nets are:

- *The sets.* Each post works a set from the EM library (`data/em/osa_ak.json`): the vehicles the R-123M (15 km over
  open ground, `mon_2112_80`), the battery's command post and everyone above it the R-111 (35 km whip to whip,
  `r111_to`), each calibrated to its published range (`CalibrateRadio`). A mixed pair takes half of each set's
  fitted system loss.
- *The ground.* The path's loss over the country's ground (`CountryGround` over `maps/pannonia.json`): the plane-earth
  loss below 1 GHz, or the diffraction over a ridge when it is larger, as the EM layer's `LinkLossCore` gives it.
- *Jamming.* A communications jammer (`RadioJammer`: where, how high, its power and band, from and until) raises
  the receiver's floor by its J/N.
- *The margin.* Below -3 dB the call does not get through: the pipeline finds another way, and a telling by radio is
  refused with why ("out of range", "the ground is in the way", "the net is jammed"). Above it the call is said again
  with chance 1 - 1/(1 + exp(-margin/2)), the EM layer's delivery chance.
- *The enemy listening.* Each `RadioListener` hears a call by its own margin; a call's `heard` share scales what the
  enemy's radio intercept gets from it (P3's `Transmission::intercept`). With none placed the intercept service is the
  campaign's abstract one, which hears every call.

The game's campaigns on the nation's theatre (which has the country's map) run with it. Left: the air war's own
communications jammers and listening aircraft placed as jammers and listeners (it has none yet), masts at the command
posts (the R-111 reaches 45-60 km on them), and the PU-12M's two sets and the nets' frequencies (`radio_sets.hpp`).

## 5. Multiplayer (`docs/MULTIPLAYER.md`)

Each player's character is a post; the server runs one net and sends each client only its own character's deliveries.
Two players can tell each other things only by the means their posts share.

**Built in simcore (P6, 29 Oct 2026).** `InfoNet::SetPlayers` makes several posts players' characters: none of them
passes anything on by himself, each reports from his own post (`Report(f, by)`), and a vehicle commander played by
someone else does not tell his crew what his battery commander told him until he does. A client never holds the net:
`command/post_view.hpp` gives each post's view as the server sends it (`PostViewJson`, with what is new since the last
view), the deliveries with the words as the holder has them (the short form until the paper comes), who told him and
how, and the people he can tell; the client merges each update (`MergePostView`). A telling from a client comes back as
a request (`TellRequestJson`) that the server applies at its own hour and only from that client's own post
(`ApplyTellRequest`). Left: the campaign as a networked session in Unreal (the campaign screen is single-player; its
views would ride on the player controller as the knowledge stream's do, `docs/MULTIPLAYER.md`).

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

**Built (P2, 29 Oct 2026): the player tells people.** A **telling** (`InfoNet::Tell`) is a time-stamped input like the
others: who tells whom what, by which means, when. The pipeline plays it as it plays everyone's relays, so it is saved
and replayed like them (`command/player_inputs.hpp`, kept beside the campaign in `Saved/AirWar/pipeline.json`). He can
tell only what he knows by then, and only by a means that reaches the other then. `InfoNet::TellOptions` gives every means
with what it costs: when the other has it (handling, a telephone connection, a runner's walk, the next meeting or the
courier's next run), whether the paper goes with it, how much the enemy's intercept service learns (the radio:
everything), and whether a carrier is on the road (he may be lost: then the player must send again). Someone the player
told passes it on to those under him it is for, as he would have from anyone. The player's **own report or order**
(`InfoNet::Report`) starts with him and goes only where he tells it.

On the campaign screen each piece of news has "tell": the tell box picks whom (the player's superior, his people and
those he deals with), the means with its arrival and cost, and tells him. The posture panel gives the posture as **the
battery commander's order** (O): a paper of his own that the vehicle commanders have only once he tells each of them.
Left: what the vehicles do on an order that reached them (track G, `docs/campaign/11_BATTERY_LIFE_PROPOSAL.md` G10); a
report written from the records (P9); telling in the world (walking up, the handset) rather than from the screen.

**Built (P1, 29 Oct 2026).** A delivery is a **scene** (`simcore/command/scenes.hpp`): its kind from the means and the
places (in person, a briefing, a runner, a despatch rider, a courier, the telephone, the radio, the warning net, the
sirens, the teleprinter, the field post, the news), who tells, where each of them is, the words said (an air warning
opens with "Vozdukh!"), and the paper handed over with it. A briefing the person sat in is **one scene**
(`Briefings`): the routine, the place, who held it and what was told in order. The **map case** (`MapCase`) holds every
paper the person has in full, dated and with who gave it.

A post that sits in a session has a `"seat"` in the chain's data (`"osa:commander"`, `"post:battery_commander"`). A
scenario's **`"deliveries"`** (`simcore/session_deliveries.hpp`) are played by the runner at their time: on the radio
net, the telephone, the intercom, the warning net's speaker, the sirens, the state radio, or at the hatch, each heard by
the seats that channel reaches; the paper goes into the map case of the seat it was for; an air warning sets the
battery's air threat. A carrier (someone walking over, a runner, a despatch rider, the courier's car) is a **visitor**:
he sets out to arrive at the delivery's time from its bearing, waits a minute at the vehicle and goes back; the game
draws him as a basic model (`soldier`, `motorcycle`, `uaz469`).

When a campaign's raid is written as a scenario (`RaidScenarioBuilder`), a **`SessionSource`** gives it the pipeline's
deliveries to the Osa's seats in the raid's window, with the player's character at the seat they fight from
(`PlayFromSeat`: then the commander relays to his crew, and the player passes nothing on), and the raid's warning comes
when the pipeline brings it to the vehicle: before the session, at its start; inside it, at that second; never, not at
all.

Unreal: the carriers are entities (`USimWorldSubsystem::GetEntityStates`, kind `visitor`); the seat's map case opens on
**O** in the cabin (a paper handed in is pointed out until it is read); the campaign screen's "What you know" opens with
the last briefing as its scene and the papers in the map case.

**Built (P7, 29 Oct 2026): the district in the world.** The chain's places that are not drawn otherwise (the command
posts, the komendatura, the offices, the police station, the railway stations, the works, the observation post, the
villages; `data/command/places_pannonia.json`) are basic models where they stand, from the scenery's `"places"` and
`"place_kinds"` (`data/scenery/basic_models.json`, placeholders): the regiment's KP its tents, masts, trucks and cars,
an office its building, a village its church, houses and barns (`command/district.hpp` `DistrictPlaces`). A place that
is a theatre object is the nation's asset (the battery, the radar companies, the airfield), a city or a town is the
country map's. With a campaign running, each post's holder stands at the place he is at at its hour, as his routines
take him (`DistrictPeople`: the regiment's commander at his KP, or not there while he visits the battery); the crews are
in their vehicles. Unreal draws both with the nation's assets (`ASimMapActor`). Left: buildings of their own (the
places are boxes of the kinds the library has), the people moving between builds of the map, and the district of
wherever the battery fights (`docs/campaign/` 15.6).
