# The Kesmark test range: a 40 km map, the airspace and the weather

A test map for the Osa, the battery's other vehicles and enemy and friendly aircraft (18 October 2026): the Kesmark
district of the Pannonian theatre (`data/air_war/theatre_pannonia.json`, `data/command/places_pannonia.json`) as
40 by 40 km of ground at the least detail that reads, ground beyond it out to 150 km, and weather over the whole
airspace. The sim and the picture share both: the sim's line of sight runs over the same ground the game draws, and
the cloud the game shows is the cloud the TV sight cannot see through.

## What is on it

`data/maps/kesmark_range.json`, built by `simcore/terrain_map.hpp` (deterministic for its seed; a few seconds with
its detail areas):

| Thing | How it is made |
|---|---|
| Ground | A plain at 110 m with three octaves of gentle relief, hills (Polany hill under the P-40, Ludas, the Kesmark heights, Varos hill, the neutral neighbour's hills beyond the border), the Rava's valley carved along the river, sites levelled. Baked at 50 m inside the map, 500 m out to 150 km, finer in the two detail areas (below) |
| River | The Rava through both Kesmark bridges (12, -6) and (14, -4) and the Rava marshes |
| Roads, railway | Main and secondary roads and tracks between the places; the railway through Kesmark station, the rail bridge and the Horka siding. A bridge wherever a road or the railway crosses the river |
| Towns | Kesmark (1400 buildings), five villages (90-110), Novo Varosd and four towns beyond the map; houses end on to the streets, flats in a town's centre; 70 lone farmsteads |
| Woods | The Lazne woods (regiment command post), the Dubnik forest (division HQ), the woods on Polany hill and at Horka, 45 copses, trees along the river, big woods beyond the map: canopy blocks 100 m (400 m far out), 14-24 m high |
| Military sites | Battery 21's position (four launcher revetments, the PU-12M's pad), its two dummy positions, the ZU-23 at the bridges, the fuel depot, Kecel air base (runway, taxiway, 12 shelters, apron, hangars, tower), its guns, the S-125 at Kecel, the regiment's and division's posts, the missile technical battery, the P-40 and P-19 posts, the garrison barracks, the sector bunker, the church tower, the sugar factory, the station, Tiszavar air base and the Paksany power station far out |

Every one is a box (cylinders for tanks, chimneys, cooling towers). Positions come from the theatre and places files;
a test checks they agree (`test_world_map.cpp`). The woods' and buildings' heights are also kept as obstacle grids
(`MapData::ObstacleHeight`): the line of sight and the radio paths carry them (E9, `docs/EM_ARCHITECTURE.md` §27).

## Detail areas (roadmap R6)

Where play happens close up the ground is finer: two **detail areas** (`terrain.detail`), each a level of the drawn
ground of its own (`MapData::DrawnLevels`, finest first; `docs/ARCHITECTURE.md`, "One ground, one vertical"), so the
sim and the picture still share one ground:

| Area | Where | Ground | Obstacles | What is in it |
|---|---|---|---|---|
| `bty21` | 9.5 km east, 6.5 km south, 12 km square (inside the map proper) | 10 m, micro relief 0.8 m | 25 m | Battery 21's position, both Kesmark bridges, Vrbice, the station |
| `capital` | 36 km east, 58 km north, 15 km square (on the far ground) | 20 m, micro relief 0.6 m | 25 m | The capital, laid out (12000 buildings), and the dynamic campaign test round it |

* **Edges on the coarse nodes.** An area's edges fall on the nodes of the ground it lies in (the map proper's 50 m, the
  far ground's 500 m) and its spacing divides that ground's, so a coarse cell is wholly inside it (not drawn) or wholly
  outside (`NoGroundDrawnTwice`); the loader refuses an area that does not.
* **Its ground** is the coarse level's triangles sampled at its nodes, so at its edge it is the coarse ground exactly,
  plus micro relief (`noise::Fbm`, 400 m, three octaves) faded to nothing over the last 300 m to the edge, within 15-60 m
  of every road, railway and river, on every levelled site and in every town (50-100 m beyond its edge).
* **The capital's lines.** The country's roads, railways and the Rava through the capital (`data/maps/pannonia.json`)
  are imported, clipped to the area and a kilometre round it, and laid as the map's own strips and bridges
  (`MapData::ImportedLines`); not in the map file's lines, so the route service and `NearestRoad` are unchanged.
* **The capital's town** (`detail_settlements`) is laid after everything else on its own random stream: houses along
  the imported roads, blocks of flats within 0.45 of its 7 km radius, nothing within 30 m of the Rava. The map proper's
  layout is unchanged by it: the core's boxes per part are recorded in `test_world_map.cpp` (12038, from r-track before
  R6) and checked. The far ground's woods within 100 m of its radius give way.
* **The dynamic campaign test's positions are map sites** (`capital_*`, 32 of them: the Osa positions of battery 21
  and of battery 22, the Kub, S-75 and S-125, the gun sites, the radars), so the town keeps clear of them and their
  ground is levelled. Three on the Rava's bank are left out (the ZU-23 at the road bridge and at the river port, S-60
  battery 6), because a levelled site would fill the river's bed.
* **Obstacles at 25 m** in each area, from the same boxes (`MapData::DetailObstacleGrids`).

Unreal draws every level it is given (`ASimMapActor::BuildGround`): the area inside the map proper as the map proper
(collision, shadows, its colours), the capital's as the far ground near the scenario; the country's hashed town blocks
and strategic ribbons are not drawn where the map lays its own (the capital, the detail areas). Trees, roofs, field
parcels and real assets are the next step (R6's dressing).

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

* `ASimMapActor` draws the map at play time in the running scenario's frame.
  * The ground: every drawn level (`MapData::DrawnLevels`) is a procedural mesh with vertex-coloured fields. The
    levels are the map at 50 m, the far ground at 500 m, the country at 1 km and the land beyond at 4 km. Each mesh's
    vertices are the sim's own ground points (one ground, roadmap R5; `docs/ARCHITECTURE.md`).
  * Collision: inside the map, and on far tiles within 15 km.
  * The roads, railway and river are ribbons. Every box is an instanced engine cube or cylinder, one component and
    colour per kind.
  * "Build Preview" in the editor draws it at the map's own origin.
* Ground items stand on that ground through `USimWorldSubsystem::GroundAt`, from the map's own link, with no trace:
  your vehicle, the battery, the works, the visitors, the movers and the neighbours' sites.
* **Nothing under the map** (R5):
  * `ground_check data 3600` plays the dynamic campaign testbed for an hour and every map mission for ten minutes. It
    lists anything under the ground in simcore.
  * `python tools/ue_ground_check.py` does the same in the game, with `-SimGroundCheck=<s>`, from the drawn actors.
  * `sam3d.ground.check [s]` starts the check in a running game.
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
* On a Cesium level, ground items are put on the tiles by a ground-only trace from 3 km up; the tiles are not the
  sim's ground (`docs/ARCHITECTURE.md`, known limitations). Woods and buildings are not part of the drawn ground that
  aircraft are held over.
* Obstacles are boxes widened to whole cells (50 m on the map proper, 25 m in the detail areas), sampled bilinearly:
  a wood's edge is soft by half a cell. Beyond them, far woods are whole 400 m blocks and the far ground's other towns
  two clutter heights.
* The detail areas' ground is drawn at 10 m and 20 m with collision: about 2 million vertices more to cook when the
  map loads.
