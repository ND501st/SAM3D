# Unreal project (`unreal/SAM3D`)

A C++ Unreal Engine 5 project that links the header-only `simcore` library from the repo root.

## First open (Windows PC)

1. Install Unreal Engine 5 (latest 5.x) from the Epic Games Launcher, and Visual Studio 2022 with **Game development with C++** (`tools/setup-pc.ps1` does both).
2. Right-click `SAM3D.uproject` → **Switch Unreal Engine version…** → pick your installed version. This writes `EngineAssociation`.
3. Right-click `SAM3D.uproject` → **Generate Visual Studio project files**.
4. Open `SAM3D.sln`, set configuration **Development Editor | Win64**, and build (Ctrl+Shift+B). Or just double-click the `.uproject` and let Unreal compile the module.
5. In the editor: **File → New Level → Basic**, save it as `Content/Maps/Sandbox`.
6. Drag **Sim Scenario Director** (search the Place Actors panel) into the level. Press **Play**.
   - The director resets the sim at its origin (default 45°N 25°E, 300 m), starts the demo, and spawns a cone for each aircraft/missile.
   - Sim events print on screen (yellow) and to the Output Log (`LogSAM3D`).
   - The target starts about 40 km north (+X), so eject (F8) and fly the camera north to watch.

## Code map

| File | Purpose |
|---|---|
| `Source/SAM3D/Public/SimCoreIncludes.h` | The only place Unreal code includes `simcore` (wrapped in `THIRD_PARTY_INCLUDES_*`). |
| `SimWorldSubsystem` | Owns `simcore::World`, runs it at a fixed 100 Hz, converts coordinates, broadcasts events. |
| `SimEntityActor` | Visual proxy for one sim entity. Subclass in Blueprint for real meshes and effects. |
| `SimScenarioDirector` | Level actor that sets the origin, starts scenarios, and spawns proxies. |

## Coordinates

`simcore` works in ENU metres (x=East, y=North, z=Up). Unreal uses cm with X=North, Y=East, Z=Up (left-handed).
Convert only through `USimWorldSubsystem::EnuToUnreal` / `UnrealToEnu`.

## Adding Cesium for Unreal (next step)

1. Install **Cesium for Unreal** from Fab (free) and enable the plugin.
2. Add a `CesiumGeoreference` whose origin matches the director's origin lat/lon/alt.
3. Implement a `simcore::Terrain` subclass that samples Cesium terrain height (line traces or Cesium's height query API), and pass it to `simcore::World` in place of the flat demo ground.
4. Once the georeference is set up, `EnuToUnreal` stays valid near the origin. For large areas, convert via ECEF using the Cesium georeference instead.
