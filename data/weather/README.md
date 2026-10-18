# Weather

`presets.json`: the weather a scenario names in its `"weather"` (`simcore/weather_data.hpp`, `simcore/weather.hpp`,
`docs/TEST_RANGE.md`): the surface temperature, dewpoint, QNH, visibility and rain, the surface wind and the winds
aloft, the boundary layer, and the cloud layers (kind, base or "lcl", depth, cover in oktas, cell size, liquid water
content or extinction). Typical days chosen inside textbook ranges (`wmo_cloud_atlas`, `stull_1988`, `kunkel_1984`;
`docs/SOURCES.md`), each with a source and confidence. A preset's `air_war` names the air war's weather it plays
(clear, cloudy, poor).
