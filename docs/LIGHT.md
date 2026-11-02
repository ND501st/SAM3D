# The light: one sun for a session

Written 2 November 2026 (roadmap R7, "the time of day in the missions menu"). Code: `simcore/solar.hpp` (the sun's
place, dates, the sun's crossings), `simcore/light.hpp` (the sky clock, the light and its phases, the menu's presets),
`simcore/scenario.hpp` (a scenario's clock, `ApplyTimeChoice`), `simcore/scenario_runner.hpp` (`UpdateLight`,
`LightNow`, `Clock`), `simcore/missions.hpp` (the menu's time options), `simcore/air_war/data.hpp` (the theatre's date);
Unreal: `ASimMissionMenuHUD`, `USimWorldSubsystem::LoadScenario`, `GetSunNow`, `GetSkyClock`, `ASimSkyWeatherActor`,
`ASimScenarioDirector`. Data: `data/gameplay/light.json`. Tests: `simcore/tests/test_light.cpp`,
`SAM3D.Missions.TimeOfDay`.

## 1. The sky clock

Every scenario has a place, a date and a start hour:

| What | From | Without it |
|---|---|---|
| The place | the scenario's `origin` | (required) |
| The date | `"date": "1985-10-15"` | 15 October (day 288) |
| The clock | `"utc_offset_h": 1` | the longitude's hour, `round(lon / 15)` |
| The hour of second 0 | `"clock_start": "19:00"` | 23:00 if the legacy keys say night (`environment.daylight: false` or `battery.night: true`), else 10:30 |

The legacy keys stay valid for good: saved raid files from earlier campaigns (`Saved/AirWar/raid_<id>.json`) and a
few tests still use them. They only choose the start hour of a scenario without a clock. A scenario with a clock and a
legacy key that the sun contradicts (10:00 and `"daylight": false`) is refused.

The sun is the Astronomical Almanac's low-precision solar position (`meeus_1998`), with refraction ignored and the year
taken as 2001: a 1982 or 1988 date is off by a few hundredths of a degree. The clock offset only names the hours; the
light depends on the longitude.

## 2. The light and its thresholds

`LightAt(clock, t)` is the sun at session second `t` (the days wrap; the year after the last day is a common year) and
what it means, by `data/gameplay/light.json`:

| Value | Default | Meaning |
|---|---|---|
| `low_sun_deg` | 6 | the sun this high or higher: **day** |
| `twilight_deg` | 6 | more than this below the horizon: **night** (civil twilight; equal to `visual_range.json`'s, tested) |
| (between) | | **dawn** if the sun is rising, else **dusk** |
| `tv_min_elevation_deg` | -6 | the TV sight has **daylight** at or above |
| `optical_night_deg` | -6 | below it the battery is **dark** |

`night_share` is 0 by day and 1 by night, a smoothstep from +6 to -6 degrees (the same curve the fx's tracers used).
Twilight is a step for everything but the fx: the TV keeps a yes or no at -6 degrees and the visual range three steps.

Over the Kesmark range on 15 October (UTC+1): 33 degrees at 10:30; sunset 16:49; the sun 3 degrees down at 17:08 and
6 down (dark) at 17:25; in the morning 6 down at 05:28 and 3 down at 05:47.

## 3. What reads it

The runner evaluates the sun at most once a simulated second (`UpdateLight`); when the daylight or the dark changes it
tells everything below at once, and the debrief gets one line ("dusk: the sun 3.0 deg below the horizon"). It is a
world fact for the debrief, never a crewman's knowledge.

| Consumer | What it reads |
|---|---|
| The TV sight (`em/optical.hpp`, the engagement model) | `daylight` |
| The battery's night halts (`battery.hpp`) | `dark` |
| Flags or lamps on the march, the order net's night (`command/battery_orders.hpp`) | `dark` |
| The march planned to a place or for the task (`nav/session.hpp` `PlanTo`, night speeds and gaps) | `dark` at the move off |
| The TZM's run and its route (`campaign/stocks.hpp`) | `dark` at its start |
| The visual range (`nav::VisualRangeM`) | the sun's elevation |
| The AAA's fire control (`aaa.hpp` `DrawPassErrors`: optical sights blind) | `dark` |
| The dynamic campaign's waves (`air_war/test_waves.hpp` `WavePassKillChance`) | `dark`: a defence without radar takes `attrition.night`, as the abstract raids do (`mission_bridge.hpp`) |
| The couriers' night speed (`command/info_net.hpp`) | `dark` once given a sky (`InfoNet::SetSky`; the campaign's net gets its dated theatre's through `command::GiveTheatreSky`); without one the old 20:00-06:00 |
| Unreal's sky (`ASimSkyWeatherActor`, or the level's own Movable sun) | the sun's elevation and azimuth |
| Unreal's fx (`SimFxLogic::NightShare`) | `simcore::NightShare` of the sun's elevation |

## 4. The menu's time of day

`data/scenarios/missions.json` gives each entry `"time": "fixed"` or `"choice"` (the default) and, optionally,
`"time_options"`, a list starting with `"mission"`. The menu offers the mission's own time, dawn (the sun 3 degrees down,
rising), day (an hour and a half before local noon), dusk (3 degrees down, setting), night (solar midnight) or an hour
in half hours, labelled by the scenario's sun ("Dusk 17:08"); without a crossing (the polar day or night) dawn and dusk
fall back to 05:30 and 18:30. The choice reaches the level as `?SimTime=<preset>[?SimHour=<h>]` and `LoadScenario` applies it
(`ApplyTimeChoice`: the start hour, and the paper clock when the scenario has one), to both halves of a battery link,
which must keep one clock.

Held: the lessons, the battles on their dates (the Bekaa on 24 July 1982 at 11:00, UTC+3; Cuito Cuanavale on 20
February 1988 at 10:30; the Angola night raid at 23:00, its date a game value) and the air war's sample raids. The
hours of the battles are game values, recorded in each file's notes. A command line's `-SimTime=` still overrides a held
mission (a developer's tool).

## 5. The air war

A theatre with a `"date"` (its year must be the war's `"year"`) and `"utc_offset_h"` decides the parts of the day by the
sun at its origin (`DayPartBySun`: the same phases), and the planner aims a part at its middle by the sun
(`DayPartHourBySun`: sunrise, noon, sunset, midnight) instead of the buckets' 05:30, 12:00, 19:30 and 01:00. An undated
theatre keeps the fixed buckets (night 21-04, dawn 04-07, day 07-18, dusk 18-21) the first balance pass was tuned on.
The nation's theatre (`theatre_pannonia_nation.json`, the game's new campaigns) is dated 15 October 1985, UTC+1: dawn
05:29-06:41, day to 16:13, dusk to 17:25, night after; the tuned theatre (`theatre_pannonia.json`) stays undated. The
shift it made, small and with no rebalance, is `docs/air_war/09_BALANCE.md` §12.
The light values there are the code's defaults, equal to `light.json`'s.

A raid from a dated theatre carries its `"clock_start"` (the theatre's local time at its second 0), its `"date"` and its
`"utc_offset_h"`, never the legacy keys: its light is the sun's, as its planner's part of the day was. A raid from an
undated theatre (the tuned `theatre_pannonia.json`) was planned by the buckets, so it carries no clock and keeps the
legacy keys from the planner's part (`battery.night` for the night, `environment.daylight` otherwise): it plays in the
light it was planned in, as before R7, and the bridge's night attrition (`mission_bridge.hpp`, the planner's part) agrees.

## 6. Left

- Twilight as a gradient for the TV (the contrast threshold by `night_share`), and a moon.
- Multiplayer: the chosen hour is not replicated; each machine applies its own `-SimTime` (docs/MULTIPLAYER.md), and a
  client's sky stands at the start hour.
- OsaPractice's sun is the level's: it follows the scenario only if it is Movable (not checked without the editor).
- The air war reads the code's light defaults, not `light.json`.
- The air war's seven sample raids (`data/scenarios/air_war_*.json`, from the undated theatre) have estimated clocks:
  the sun's rising or setting at each origin on 15 October less half the time limit, so each dawn or dusk raid plays in
  its twilight (each file's notes; `test_light.cpp` checks the phase at second 0). A regenerated sample from the undated
  theatre would carry the legacy keys instead.
