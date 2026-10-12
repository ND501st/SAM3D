# Command posts

Data for the battery and regiment command posts (roadmap phase 7), loaded by `simcore/pu12.hpp`. A scenario names one
with its top-level `"command_post"` (e.g. `"command/pu12.json"`).

| File | What |
|---|---|
| `pu12.json` | PU-12M battery command post: the plotting board's update period, range rings, the radar battery's tactical number and the post's navigation errors |

| `c2_sources.json` | Library of other early-warning and command sources (EWR voice net, division early-warning net, neighbouring battery, observation post, data link) the crew can switch to; the PU-12 stays the main source |

A scenario's `"c2"` block makes instances (`id`, library `type`, `position`, `net`, `link_range_m`, `outages`) and sets the commander's policy (`primary`, `active`, `auto_fallback`, `fallback_order`, `presets`, `select_s`, `retune_s`, `primary_stable_s`, `heard_life_s`). Triggers: `c2_source` (change source, or `"none"`), `c2_fail`, `c2_restore`. A player changes source with `ScenarioRunner::RequestC2Source`. See `docs/research/EM_C2_SOURCES_2026-10.md`.

Every value is `{"value": ..., "source": "...", "confidence": "..."}`, cited like `data/systems/` (`docs/SOURCES.md`).

The board (`samsim_osa` p20) writes each target once a minute as a four-digit number (the radar battery's tactical
number, then a sequence number) and its height in hectometres over a two-digit code: the tens digit is the type
(0 jamming, 1 friend, 2 identified, 3 border violator, 4 supervisor, 5 own target, 6 rule violator, 7 practice,
8 enemy, 9 no IFF), the ones digit the formation size. A scenario target gives its type with `"board_type"` (default:
1 for a friend, else 9) and its formation with `"formation"` (default 1). With the radars silent, the vehicle crews
aim by the designations the battery commander has given them (roadmap 7.2) instead of the truth: each target inside `designation_range_m` is read out by voice on every plot, one message at a time (`operator_delay_s`, `message_s`), and the vehicle commander passes it on (`pass_on_s`). A designation is a minute old at worst, off by the post's navigation error, and the crew only has it once it is passed on. The debrief logs each one ("Commander ... designation 6201: azimuth 00-71, range 45 km, height 12 hm").
