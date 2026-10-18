# The Kesmark test range: a 40 km map, the airspace and the weather

A test map for the Osa, the battery's other vehicles and enemy and friendly aircraft (18 October 2026): the Kesmark
district of the Pannonian theatre (`data/air_war/theatre_pannonia.json`, `data/command/places_pannonia.json`) as
40 by 40 km of ground at the least detail that reads, ground beyond it out to 150 km, and weather over the whole
airspace. The sim and the picture share both: the sim's line of sight runs over the same ground the game draws, and
the cloud the game shows is the cloud the TV sight cannot see through.

## What is on it

`data/maps/kesmark_range.json`, built by `simcore/terrain_map.hpp` (deterministic for its seed; about 0.7 s):

| Thing | How it is made |
|---|---|
| Ground | A plain at 110 m with three octaves of gentle relief, hills (Polany hill under the P-40, Ludas, the Kesmark heights, Varos hill, the neutral neighbour's hills beyond the border), the Rava's valley carved along the river, sites levelled. Baked at 50 m inside the map, 500 m out to 150 km |
| River | The Rava through both Kesmark bridges (12, -6) and (14, -4) and the Rava marshes |
| Roads, railway | Main and secondary roads and tracks between the places; the railway through Kesmark station, the rail bridge and the Horka siding. A bridge wherever a road or the railway crosses the river |
| Towns | Kesmark (1400 buildings), five villages (90-110), Novo Varosd and four towns beyond the map; houses end on to the streets, flats in a town's centre; 70 lone farmsteads |
| Woods | The Lazne woods (regiment command post), the Dubnik forest (division HQ), the woods on Polany hill and at Horka, 45 copses, trees along the river, big woods beyond the map: canopy blocks 100 m (400 m far out), 14-24 m high |
| Military sites | Battery 21's position (four launcher revetments, the PU-12M's pad), its two dummy positions, the ZU-23 at the bridges, the fuel depot, Kecel air base (runway, taxiway, 12 shelters, apron, hangars, tower), its guns, the S-125 at Kecel, the regiment's and division's posts, the missile technical battery, the P-40 and P-19 posts, the garrison barracks, the sector bunker, the church tower, the sugar factory, the station, Tiszavar air base and the Paksany power station far out |

Every one is a box (cylinders for tanks, chimneys, cooling towers). Positions come from the theatre and places files;
a test checks they agree (`test_world_map.cpp`). The woods' and buildings' heights are also kept as an obstacle grid
(`MapData::ObstacleHeight`) for E9's forest and urban obstacles; the line of sight does not use it yet.

## The airspace and the weather

The map's ground reaches 150 km out; past it the sky atmosphere's planet carries on. Aircraft may fly anywhere (the
air war's border entries are 100-185 km out). The scenario's frame is a true tangent plane: the map is drawn through
the Earth-centred frame, so its ground falls away with the Earth's curvature (1.8 km at 150 km), as the sim's does.

`data/weather/presets.json` (`simcore/weather.hpp`): clear, fair_cumulus, cloudy, poor, low_stratus; the air war's
clear/cloudy/poor map to the presets of the same name.

* **The air**: ISA shifted by the day's surface temperature and QNH (hydrostatic, standard lapse).
* **The wind**: a log profile over the ground's roughness (z0 0.1 m, farmland) to the surface layer's top, then a blend
  and veer to the winds aloft at the boundary layer's top, then the winds-aloft table. Aircraft fly at their airspeed in
  it and crab to hold their track to the next waypoint (`StepAircraft`); their velocity is over the ground. Missiles
  are not blown about (yet).
* **The cloud**: layers with flat bases (cumulus at the condensation level), their cover in oktas as a repeatable noise
  field sized by the layer's cells, cumulus tops rising where the field is strongest; the field drifts with the wind at
  the layer's height. The TV sight's contrast falls with the light the cloud lets through (Kunkel's extinction from the
  liquid water); behind a cloud it says "in cloud". Radar sees through cloud; rain (`rain_mmh`) still attenuates it.
* **The sun**: `simcore/solar.hpp` (Meeus) for the scenario's place, day and hour.

## Playing on it

A scenario opts in with `"terrain": {"type": "map", "map": "maps/kesmark_range.json"}` (any origin: the map is looked
up through the Earth-centred frame) and `"weather": "<preset>"`. `data/scenarios/kesmark_range_test.json` puts battery
21 in its position with a strike pair coming in low from the western border, a reconnaissance jet high through the
cloud and a friendly MiG-21 pair on patrol, in the `cloudy` weather. The missions menu lists it under "Test range" and
opens the level `/Game/Maps/KesmarkRange` (missions.json `"level"`). The air war's raid writer adds the day's weather
(`ScenarioFormat::weather`) and can put a raid on the map (`ScenarioFormat::map_file`).

```bash
./simcore/build/scenario_demo data/scenarios/kesmark_range_test.json data out/kesmark.acmi   # headless
./simcore/build/map_dump data maps/kesmark_range.json out/kesmark_range_map.json            # the layout as JSON
python tools/map_view.py out/kesmark_range_map.json --png                                    # from above: out/map_view/
```

## In Unreal

`Content/Maps/KesmarkRange` (made by `tools/ue_build_kesmark_range.py` through the editor commandlet) holds no geometry:

* `ASimMapActor` draws the map at play time in the running scenario's frame: the ground as a procedural mesh with
  vertex-coloured fields and collision inside the map (the vehicle traces for the ground), the roads, railway and river
  as ribbons, every box as instanced engine cubes and cylinders, one component and colour per kind. "Build Preview" in
  the editor draws it at the map's own origin.
* `ASimSkyWeatherActor`: the sun (a directional light at the sun's place), a physically based sky atmosphere whose haze
  is set from the day's visibility (Koschmieder: 3.912 / visibility, less Rayleigh), the engine's volumetric cloud on
  the main layer, and with `-SimCloudCells` the sim's own cloud cells as boxes, drifting.
* A Sim Scenario Director with Cesium off plays `scenarios/kesmark_range_test.json` in the editor's Play.

```
UnrealEditor.exe SAM3D.uproject /Game/Maps/KesmarkRange -game -SimScenario=scenarios/kesmark_range_test.json
UnrealEditor.exe SAM3D.uproject /Game/Maps/KesmarkRange -game -RenderOffscreen -SimScenario=scenarios/kesmark_range_test.json -SimMapShots=C:/dev/SAM3D/out/map_shots
```

`-SimMapShots` photographs six views (the battery's position, Kesmark, the bridges, the woods, from 3 km, from 9 km)
and quits.

## Known limits

* The volumetric cloud is the engine's sample material on one layer, set to the main layer's base, depth and cover; its
  clouds stand where its own noise puts them, not exactly where the sim's do. The cells (`-SimCloudCells`) are exact.
* Buildings scatter on lots and a grid rather than real streets; no roofs, no detail, by design.
* Ground colour is by patches of farmland tint, not a land-use map.
* A scenario played in the level on its own flat ground gets a warning: its line of sight is not the drawn ground's.
* The obstacle grid is not yet used by line of sight or propagation (E9).
