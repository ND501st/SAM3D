# Scenery: ultra-basic models of every asset

Written 18 October 2026, after the project owner asked for ultra-basic models of the enemy and friendly assets, and for
scenarios to be tried on the map once it is built in Unreal. The map itself (the ground, towns, woods, roads, the river,
the sites) is `simcore/terrain_map.hpp` and `data/maps/` (another workstream); this folder is the models that stand and fly
on it.

## What there is

`data/scenery/basic_models.json` (79 models), loaded and expanded by `simcore/scenery/basic_models.hpp`
(`simcore::scenery::ModelLibrary`); drawn in Unreal by `FSimBasicModels` (`SimBasicModels.h/.cpp`).

| Group | Models |
|---|---|
| NATO aircraft | F-4G, F-4E, RF-4C, F-16A, F-111F, EF-111A, Tornado IDS, A-10A, F-15C, AH-1S, BQM-74 class decoy |
| Warsaw Pact aircraft | MiG-23MLD, MiG-27, Su-24, Su-25, Su-17M3, MiG-25RB, MiG-25BM, Yak-28PP, Mi-24 |
| The neutral neighbour's | Saab 105 class, Draken class, AB 212 class, Alouette III class |
| The rebels' | An-2, UTVA 75 class, Mi-8, G-2 Galeb class |
| Our side's aircraft | MiG-21 |
| Generic (a target without a model: by its class) | fighter, bomber, helicopter, drone, light aircraft |
| Missiles | 9M33, an anti-radiation missile |
| Our battery | 9A33 Osa-AK combat vehicle, its dummy, PU-12M, 9T217 reload vehicle, fuel truck, Ural, UAZ-469 |
| Our air defence and army | Kub launcher and radar, S-125 launcher and radar, S-75 launcher and radar, Strela-10, ZU-23, P-19, P-40, T-55, BMP-1, BTR-60 |
| Buildings and nature | house, large house, barn, apartment block, industrial hall, shed, barracks, town hall, hospital, station, hangar, bunker, church, chimney, cooling tower, fuel tank, crates, tent, command post tent, antenna mast, conifer, broadleaf tree, road bridge |

Each model is an archetype and its overall dimensions (approximate public figures, for scale only) and colour; the code
builds it from boxes, cylinders, cones and spheres in the model's frame (x forward, y right, z up, metres; the origin on
the ground under the middle, an aircraft's at the middle of its fuselage):

| Archetype | Built from |
|---|---|
| jet | fuselage, nose cone, canopy, wings (swept, delta, straight, or a variable sweep half spread), tailplane (none for a delta), one or two fins, nozzles |
| prop | box fuselage, propeller disc, wing (or two for a biplane), tail |
| helicopter | cabin and glazed nose, tail boom, mast, main and tail rotor discs, fin, stub wings for gunships, skids |
| drone, missile | body, nose, wings or fins |
| wheeled, tracked | hull, wheels or tracks, cab, turret and gun, missiles on their launchers, a radar dish or array on a mast, a tall mast (the PU-12M's) |
| truck | cab, cargo body, tank or box body, wheels, a radar array (the P-19) |
| launcher, gun | base, turntable, rails with missiles at their elevation; barrels |
| building, church, tower, tent | walls and a 45-degree pitched roof (or flat), a tower and spire; cylinders for chimneys, cooling towers and tanks |
| tree, mast, crates, bridge | trunk and crown; pole and crossbar; stacked boxes; a deck on piers |

## How the game uses them

- **Aircraft**: a scenario target names its model (`"model": "su24"`; the air war's raids write each aircraft's type),
  else its class picks one (`fallbacks`: fighter, bomber, helicopter, drone, default), else its Tacview tags (a
  rotorcraft is a helicopter). `USimWorldSubsystem::GetEntityStates` puts it in `FSimEntityState.Model`; the director
  draws the entity with it at its real size.
- **Missiles**: our 9M33.
- **The battery's vehicles**: the real models where they exist (the TZM, the fuel truck, the PU-12M:
  `ASimEntityActor::UseVehicleModel`), else the basic model of their kind (`ground`: launcher → the 9A33, dummy → the
  dummy), else a box.
- **The map**: its sites can use the same library for their vehicles and buildings.

## Checks

- `simcore/tests/test_scenery.cpp`: every aircraft any adversary flies has a model (helicopters as helicopters); every
  battery vehicle kind has one; a target's model, class and tags pick the right one; every model is built and is its
  stated size (parts within 1.6 times its length and span; masts may stand higher); the loader refuses unknown archetypes
  and colours; a generated raid's aircraft carry their models through the scenario loader.
- **The gallery** (Unreal, `ASimModelGallery`): `UnrealEditor.exe SAM3D.uproject /Game/Maps/OsaPractice -game
  -SimModelGallery=<folder>` lays every model out in rows on a flat ground high above the terrain, labelled, and
  photographs the rows (`gallery_<n>.png`).
- **Scenario photographs**: add `-SimShots=<folder> -SimShotTimes=20,60,120` (and `-SimTimeScale=6` to run the sim
  faster between them) to any scenario run: at each time the sim pauses, a chase camera behind the nearest aircraft and a
  view over the battery are photographed (`shot_<t>_air.png`, `shot_<t>_battery.png`), the log lists every entity with
  its model, and the sim goes on; after the last the game quits. In Git Bash, set `MSYS_NO_PATHCONV=1` so `/Game/...`
  isn't turned into a Windows path.

## On the map

The map (`/Game/Maps/KesmarkRange`, `data/maps/kesmark_range.json`) draws the district; a scenario plays on its ground
with `"terrain": {"type": "map", "map": "maps/kesmark_range.json"}`. The campaign's raids are written that way
(`USimAirCampaignSubsystem::WriteRaidScenario` sets `ScenarioFormat::map_file`) and fought in that level when it
exists. A raid run on the map from the command line:

```bash
MSYS_NO_PATHCONV=1 UnrealEditor.exe SAM3D.uproject /Game/Maps/KesmarkRange -game -SimScenario=scenarios/air_war_pact_dead.json -SimShots=<folder> -SimShotTimes=180,248,278,338 -SimTimeScale=6 -RenderOffscreen
```

(the sample raids in `data/scenarios/` are on flat ground; copies with the map's terrain were used for the first test,
18 October 2026: `unreal/SAM3D/Saved/MapTests/`).

### The first test on the map (18 October 2026)

Five sample raids (NATO strike, DEAD and decoys; the Pact's massed strike and DEAD) and one generated on the map were
played on `/Game/Maps/KesmarkRange` with the photographer: every aircraft drawn with its own model at its real size
(the chase shots: F-16A, A-10A, MiG-25RB, MiG-25BM, F-4E, BQM-74 class decoys), the battery's launchers as the 9A33, its
dummies, the TZM, fuel truck and PU-12M as their vehicle models. The NPC crews fought: in the decoy raid a 9M33 hit a
decoy at 485 s; in the Pact DEAD raid launch was permitted at 322 s; the generated NATO raid's crew searched in bursts
while its F-4Es passed 7.6 km out. What the test found, and what was done:

- **The battery stood in a wood.** In the decoy raid the player's Osa and launcher 22 stood inside a wood with a 17 m
  canopy (the campaign moves the battery without seeing the map). Raids written for the map now set the battery and
  its dummies on the nearest open ground (`docs/air_war/07_INTEGRATION.md`, "On the map").
- **The photographer looked into the wood.** The battery view now picks the first of eight sides (then higher) whose
  line of sight clears the map's woods and buildings, tested against the map's obstacle heights (the map's blocks have
  no collision).
- **A sphere floated over the battery** in every mission: the game mode's stand-in pawn, left behind when the director
  puts the player in the vehicle. The director now removes it. The photographer names anything drawn 7-30 m over the
  vehicle in the log, so a part placed outside it shows up.

Not yet: the player's own Osa is still the drivable vehicle's own boxes (hull, turret, antenna, animated by the sim),
not the 9A33 basic model the other launchers use; the sim's line of sight ignores the map's woods (`ObstacleHeightAt` is
there for E9).
