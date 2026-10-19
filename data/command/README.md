# Command posts

Data for the battery and regiment command posts (roadmap phase 7), loaded by `simcore/pu12.hpp`. A scenario names one
with its top-level `"command_post"` (e.g. `"command/pu12.json"`).

| File | What |
|---|---|
| `regiment_radars.json` | Library of the regiment's radar types (P-19, P-40) that feed the PU-12M: range and antenna height, cited. A scenario's `"c2.regiment"` block places them; the PU-12's board then holds only the targets some radar covers (within range, above its horizon) |
| `pu12.json` | PU-12M battery command post: the plotting board's update period, range rings, the radar battery's tactical number and the post's navigation errors |
| `aspd_u.json` | The PU-12M's ASPD-U when the player's vehicle is the post (`simcore/command_post.hpp`): 100 targets, 12 semi-automatic tracks at 2-3 s each, heights in 0.2 km steps and three zones, speeds in 12.5 m/s steps, the IT-45's three scales, the telecode's cycle and the R-407's reach, the plotter's time per mark, and the navigation errors oriented, not oriented and driving (the KP-4's 1 %) |

| `battery_tactics.json` | The battery's march spacing, threat spacing, short-halt offset, combat formation (pairs), halts and speeds; the drills' numbers (river standoff, speed afloat, ambush dwell), the TZM's 12 missiles and the fuel truck's 3.8 m³ (roadmap 7.3, 7.5; `video_osa`, unverified, and estimates) |
| `prepared_positions.json` | A prepared position's echo factors (emplacement, nets), the dummies' echo and the time to leave the emplacement (roadmap 7.6; estimates) |
| `chain_pannonia.json` | Our side's chain of command around battery 21 (`docs/command/02_CHAIN_OF_COMMAND.md`): 38 posts from the national air defence sector and the army's chief of air defence to vehicle 21-1's driver, the Kesmark garrison commandant, civil defence, the police, the militia and the village council, each with a holder, a home place, a superior, links and the means of signal it has; the routines (briefings, visits, courier runs, the news). Loaded by `simcore/command/data.hpp` |
| `places_pannonia.json` | Where they sit (`docs/command/05_PLACES.md`): the small city of Novo Varosd, the town of Kesmark, five villages, the works, the regiment's and the division's posts, the radar posts, the air base; on the air war's theatre, the battery's places moving with it |
| `signal_means.json` | The means of signal between people (`docs/command/03_MEANS.md`): in person, runners, despatch riders, courier runs, field and town telephones, radio, the early-warning net, the teleprinter, sirens, the field post, the state radio; and how an NPC relays (`utin_2018`; speeds and times estimates) |
| `c2_sources.json` | Library of other early-warning and command sources (EWR voice net, division early-warning net, neighbouring battery, observation post, data link) the crew can switch to; the PU-12 stays the main source |

A scenario's `"c2"` block makes instances (`id`, library `type`, `position`, `net`, `link_range_m`, `outages`) and sets the commander's policy (`primary`, `active`, `auto_fallback`, `fallback_order`, `presets`, `select_s`, `retune_s`, `primary_stable_s`, `heard_life_s`). Triggers: `c2_source` (change source, or `"none"`), `c2_fail`, `c2_restore`. A player changes source with `ScenarioRunner::RequestC2Source`. See `docs/research/EM_C2_SOURCES_2026-10.md`.

When the player's vehicle is the PU-12M itself, the scenario has a `"post"` block instead of `"command_post"`:
`{ "board": "command/pu12.json", "aspd": "command/aspd_u.json", "library": "command/regiment_radars.json", "regiment":
{ "position", "radars": [ { "id", "type", "position" } ], "outages": [ { "from_s", "to_s", "reason" } ] }, "radar": { "type",
"position" } }`. The regiment's command post sends its radars' picture by telecode while the post is within the R-407's
reach of it and its net is on the air; `radar` is one cabled to the post (the primary picture). The regiment's radar
battery also reads its picture out on the air-picture voice net (FuG-1, `aspd_u.json` `air_net_*`), out to the board's
200 km. The post's board is written by its own plotter, what he hears on that net first, then the IT-45, one mark at a
time (`ScenarioRunner::Post`, `PostScope`, `PlayerPlot`; `docs/research/PLOTTING_BOARD_2026-10.md`).

Every value is `{"value": ..., "source": "...", "confidence": "..."}`, cited like `data/systems/` (`docs/SOURCES.md`).

The board (`samsim_osa` p20) writes each target once a minute as a four-digit number (the radar battery's tactical
number, then a sequence number) and its height in hectometres over a two-digit code: the tens digit is the type
(0 jamming, 1 friend, 2 identified, 3 border violator, 4 supervisor, 5 own target, 6 rule violator, 7 practice,
8 enemy, 9 no IFF), the ones digit the formation size. A scenario target gives its type with `"board_type"` (default:
1 for a friend, else 9) and its formation with `"formation"` (default 1). With the radars silent, the vehicle crews
aim by the designations the battery commander has given them (roadmap 7.2) instead of the truth: each target inside `designation_range_m` is read out by voice on every plot, one message at a time (`operator_delay_s`, `message_s`), and the vehicle commander passes it on (`pass_on_s`). A designation is a minute old at worst, off by the post's navigation error, and the crew only has it once it is passed on. The debrief logs each one ("Commander ... designation 6201: azimuth 00-71, range 45 km, height 12 hm").
