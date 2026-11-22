# The light: one sun for a session

Written 2 November 2026 (roadmap R7, "the time of day in the missions menu"). Code: `simcore/solar.hpp` (the sun's
place, dates, the sun's crossings), `simcore/light.hpp` (the sky clock, the light and its phases, the menu's presets),
`simcore/scenario.hpp` (a scenario's clock, `ApplyTimeChoice`), `simcore/scenario_runner.hpp` (`UpdateLight`,
`LightNow`, `Clock`), `simcore/missions.hpp` (the menu's time options), `simcore/air_war/data.hpp` (the theatre's date);
Unreal: `ASimMissionMenuHUD`, `USimWorldSubsystem::LoadScenario`, `GetSunNow`, `GetSkyClock`, `ASimSkyWeatherActor`,
`ASimScenarioDirector`. Data: `data/gameplay/light.json`. Tests: `simcore/tests/test_light.cpp`,
`SAM3D.Missions.TimeOfDay`.

Extended 19 November 2026 (roadmap S8, "light and the sample raids"): the light on the ground in lux and the moon
(`MoonAt` in `solar.hpp`; `SkyLuxOfSun`, `MoonLux`, `TvThresholdFactor`, `AddMoon` in `light.hpp`), the TV's contrast
threshold through the twilight (`em/optical.hpp`), the night's visual range by the moon (`nav::VisualRangeM`), the tuned
theatre dated and the air war reading `light.json` under a save mode (`airwar::ApplyLightMode`, §5), the seven sample
raids regenerated (§5), and a network client's sky the server's (`USimWorldSubsystem::ApplyServerSky`, §6).

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

| `tv_full_lux` | 400 (the code: 0) | the TV's contrast threshold is its full-light one in this much light or more (S8) |
| `moonless_lux` | 0.001 | a moonless night's light: the moon's share 0 at or below it (S8) |
| `full_moon_lux` | 0.25 | a full moon high: the moon's share 1 (S8) |

`night_share` is 0 by day and 1 by night, a smoothstep from +6 to -6 degrees (the same curve the fx's tracers used).
The visual range keeps its three steps (day, dusk or dawn, night), the battery's dark its yes or no at -6 degrees.

### 2.1 The light in lux, the TV's twilight and the moon (roadmap S8)

The light on level ground under a clear sky (`SkyLuxOfSun`) is log-interpolated by the sun's elevation between the
natural illumination tables' points (`brown_1952`): about 400 lux at sunset (-0.8 degrees), 3.4 at the end of civil
twilight, 0.008 at the end of nautical twilight, and the moonless night sky's own 0.001 below; above the horizon it
climbs to about 100,000 lux. The moon adds its own (`MoonLux`, `krisciunas_schaefer_1991`): its magnitude by the phase
angle, -12.73 + 0.026 |a| + 4e-9 a^4, 2.54e-6 lux for a magnitude-0 source, by the sine of its height on level ground,
dimmed by 0.2 magnitudes an air mass (an estimate for a clear lowland sky). A full moon overhead gives 0.26 lux, a
half moon 0.03. The moon's place and phase are the Almanac's low-precision lunar formulas (`MoonAt`, `meeus_1998`:
about 0.3 degrees, the parallax taken off) in the date's own year, unlike the sun's: a scenario's `"date"` gives the
year (1985 without one), and the sky clock carries it (`SkyClock::year`).

**The TV's twilight.** A television camera, like the eye, is photon-limited in poor light: its threshold contrast goes
as one over the square root of the light (`rose_1948`). The TV's contrast threshold is multiplied by
`sqrt(tv_full_lux / lux)` when the light is less than `tv_full_lux` (`TvThresholdFactor`; the runner tells the
engagement each second as it moves, `OpticalConditions::threshold_factor`). By Koschmieder the factor k takes
ln(k) V / 3.912 off the TV's range: at the reference 40 km visibility the TV's 20 km holds to sunset, is about 10 km
at 3 degrees down (53 lux, k 2.7) and gone a little past 5 degrees down; `daylight` still ends it outright at -6 degrees. A
target lost to the light says "too dark for the TV". A moon changes nothing here: a full one is a thousandth of the
sunset's light.

**The moon at night.** `moon_share` is 0 with no moon (down, new, or below `moonless_lux`) and 1 with a full moon high
(`full_moon_lux`), logarithmic between (the eye's sense of brightness). The night's visual range goes from
`visual_range.json`'s `night_m` (1.5 km, now a moonless night) towards its `moonlit_m` (3 km) by it. The debrief's line
for the night says the moon: "night: the sun 6.0 deg below the horizon, dark; the moon 63 % lit, 21 deg up" or "; no
moon". The war's date is a dark one: on 15 October 1985 the moon is new (the total lunar eclipse of 28 October 1985 is
its full moon), so the nation's and the tuned theatre's first nights have no moon; a campaign's later days and the
battles on their own dates have theirs.

Over the Kesmark range on 15 October (UTC+1): 33 degrees at 10:30; sunset 16:49; the sun 3 degrees down at 17:08 and
6 down (dark) at 17:25; in the morning 6 down at 05:28 and 3 down at 05:47.

## 3. What reads it

The runner evaluates the sun at most once a simulated second (`UpdateLight`); when the daylight or the dark changes it
tells everything below at once, and the debrief gets one line ("dusk: the sun 3.0 deg below the horizon"). It is a
world fact for the debrief, never a crewman's knowledge.

| Consumer | What it reads |
|---|---|
| The TV sight (`em/optical.hpp`, the engagement model) | `daylight`, and `tv_threshold_factor` through the twilight (S8, told each second as it moves) |
| The battery's night halts (`battery.hpp`) | `dark` |
| Flags or lamps on the march, the order net's night (`command/battery_orders.hpp`) | `dark` |
| The march planned to a place or for the task (`nav/session.hpp` `PlanTo`, night speeds and gaps) | `dark` at the move off |
| The TZM's run and its route (`campaign/stocks.hpp`) | `dark` at its start |
| The visual range (`nav::VisualRangeM`) | the sun's elevation; at night `moon_share` (S8) |
| The AAA's fire control (`aaa.hpp` `DrawPassErrors`: optical sights blind) | `dark` |
| The dynamic campaign's waves (`air_war/test_waves.hpp` `WavePassKillChance`) | `dark`: a defence without radar takes `attrition.night`, as the abstract raids do (`mission_bridge.hpp`) |
| The couriers' night speed (`command/info_net.hpp`) | `dark` once given a sky (`InfoNet::SetSky`; the campaign's net gets its dated theatre's through `command::GiveTheatreSky`); without one the old 20:00-06:00 |
| Unreal's sky (`ASimSkyWeatherActor`, or the level's own atmosphere sun: OsaPractice's CesiumSunSky, made Movable and set to the project's 10 lux by `ASimScenarioDirector::FindLevelSun`) | the sun's elevation and azimuth; on a network client the server's clock and time (§6) |
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
February 1988 at 10:30; the Angola night raid at 23:00, its date a game value). The air war's sample raids offer their
planned hour first and take any other (3 Nov 2026: the owner tried to change one's hour). The
hours of the battles are game values, recorded in each file's notes. A command line's `-SimTime=` still overrides a held
mission (a developer's tool).

## 5. The air war

A theatre with a `"date"` (its year must be the war's `"year"`) and `"utc_offset_h"` decides the parts of the day by the
sun at its origin (`DayPartBySun`: the same phases), and the planner aims a part at its middle by the sun
(`DayPartHourBySun`: sunrise, noon, sunset, midnight) instead of the buckets' 05:30, 12:00, 19:30 and 01:00. An undated
theatre keeps the fixed buckets (night 21-04, dawn 04-07, day 07-18, dusk 18-21) the first balance pass was tuned on.
The nation's theatre (`theatre_pannonia_nation.json`, the game's new campaigns) is dated 15 October 1985, UTC+1: dawn
05:29-06:41, day to 16:13, dusk to 17:25, night after. The shift it made, small and with no rebalance, is
`docs/air_war/09_BALANCE.md` §12.

**Roadmap S8: the tuned theatre dated, the air war reading `light.json`.** `theatre_pannonia.json` (the theatre the
tests and the first balance pass were tuned on, and the demo's) now has the war's date too: 15 October 1985, UTC+1, the
same parts of the day as the nation's. Its date carries `"dated_from_light_mode": 1`: it is read only under the air war's
save mode `light.mode` 1 (`data/gameplay/air_war.json`'s `"light"` block, the way `own_air.mode` and `c2.live_mode`
are gated). A campaign saved before S8 has no `"light_mode"` in its file and replays with `light.mode` 0: its tuned
theatre undated, the buckets as it was played, so its replay is unchanged (`test_routes.cpp`'s fingerprint of the
version 1 save, journal 107, FNV 1375944258174313379, 41 packages, holds). The nation's theatre has its own date with no
such key, read under either mode. Every theatre's light thresholds (`TheatreDef::light`) are `light.json`'s under mode
1 and the code's defaults under 0 (`airwar::ApplyLightMode`, called by `LoadAirWarData` and `LoadCampaignJson`): the
parts of the day (`DayPartBySun`) and the campaign's couriers' night (`command::GiveTheatreSky`) read them, never a rule
of the air war's own. The values are equal today, so this changes no outcome; it keeps the two from parting when
`light.json` is tuned. The shift the date made on the tuned theatre is `docs/air_war/09_BALANCE.md` §19.

The menu's seven sample raids (`data/scenarios/air_war_*.json`) were regenerated from the dated tuned theatre with
`air_war_demo --build-only` (it writes each raid's scenario without playing it; the faction, days and seed of each are in
its notes). Each carries its planner's clock, the date and the war's year, and its aircraft their types' flight limits
(R3's g and roll rate). The dawn raids start with the sun a degree or two down and end with it up; the dusk raids start
with it a few degrees up and end around sunset, the rebels' (75 minutes long) in the dusk's dark. `test_light.cpp`
checks the part of the day at second 0, the daylight at second 0, a dawn raid's daylight at its end, and every
aircraft's limits.

A raid from a dated theatre carries its `"clock_start"` (the theatre's local time at its second 0), its `"date"` and its
`"utc_offset_h"`, never the legacy keys: its light is the sun's, as its planner's part of the day was. A raid from an
undated theatre (the tuned `theatre_pannonia.json` in a campaign saved before S8) was planned by the buckets, so it carries no clock and keeps the
legacy keys from the planner's part (`battery.night` for the night, `environment.daylight` otherwise): it plays in the
light it was planned in, as before R7, and the bridge's night attrition (`mission_bridge.hpp`, the planner's part) agrees.

## 6. Multiplayer: the client's sky the server's (roadmap S8)

The server owns the session's clock. With every console snapshot it sends each seat's client its sky: the start hour,
the day of the year, the year and the session's second (`USimCrewSeatComponent::ClientReceiveSky`, unreliable, like the
snapshot). The client keeps them (`USimWorldSubsystem::ApplyServerSky`, logged once) and reads its light from them:
`GetSunNow` is `LightAt` on the server's clock at the server's second through the scenario's own `light_values`,
`GetSkyClock` returns the server's clock and `GetSkyTimeSeconds` its second, which `ASimSkyWeatherActor` turns the sun
by. A client's own `-SimTime` therefore no longer matters once the first snapshot arrives; before it, and on a listen
server or alone, the local runner's clock is the sky's. A new scenario forgets the server's sky until the next snapshot.

## 7. Left

- The battery's mission session (`campaign::MissionSession`, `mission_demo --war`) runs its war undated by default
  (`MissionSessionDef::light_mode` 0): the bridges mission starts at 19:00, and its package on the march was the
  buckets' dusk (18-21); by the sun the dusk is 16:13-17:25 and the package would come before the session. Dating it
  needs the mission's clock moved into the sun's dusk, and its tests' times with it. Three air-war tests written on
  the buckets' hours (`test_c2.cpp`'s alert pools and on-call package, `test_c2_live.cpp`'s nomination) pin the tuned
  theatre undated the same way (`Buckets`).
- The moon is in simcore only: Unreal draws no moon and its night light is not raised by it.
- The Unreal side (S8's `ClientReceiveSky`) is compiled but not yet looked at in a session with a client.
- `brown_1952`'s twilight illuminance table is from memory, not a fetched copy; its values should be checked against
  the text (`docs/SOURCES.md`).
- The TV's twilight gradient is a game value (`tv_full_lux`, Rose's square root below it), not a measured curve for the
  9Ш33's camera.
- The Pact's massed-strike sample carries no ARM (no Pact strike package in the seeds searched had one); the ARM drill is
  shown by the Pact's DEAD sample. The rebels' dusk raid runs on into the dark by its end, as its planner's clock had it.
