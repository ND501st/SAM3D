# Command posts

Data for the battery and regiment command posts (roadmap phase 7), loaded by `simcore/pu12.hpp`. A scenario names one
with its top-level `"command_post"` (e.g. `"command/pu12.json"`).

| File | What |
|---|---|
| `pu12.json` | PU-12M battery command post: the plotting board's update period, range rings, the radar battery's tactical number and the post's navigation errors |

Every value is `{"value": ..., "source": "...", "confidence": "..."}`, cited like `data/systems/` (`docs/SOURCES.md`).

The board (`samsim_osa` p20) writes each target once a minute as a four-digit number (the radar battery's tactical
number, then a sequence number) and its height in hectometres over a two-digit code: the tens digit is the type
(0 jamming, 1 friend, 2 identified, 3 border violator, 4 supervisor, 5 own target, 6 rule violator, 7 practice,
8 enemy, 9 no IFF), the ones digit the formation size. A scenario target gives its type with `"board_type"` (default:
1 for a friend, else 9) and its formation with `"formation"` (default 1). With the radars silent, the vehicle crews
aim by the board's last marks (a minute old at worst, and off by the post's navigation error) instead of the truth.
