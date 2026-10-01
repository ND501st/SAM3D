# Unreal project (`unreal/SAM3D`)

A C++ Unreal Engine 5.7 project that links the header-only `simcore` library from the repo root. It uses the
Cesium for Unreal plugin (2.29+) for real terrain.

## Build

The module builds from the command line without opening the editor (close any running editor first; Live Coding
blocks command-line builds):

```
D:\UE_5.7\Engine\Build\BatchFiles\Build.bat SAM3DEditor Win64 Development -Project=C:\dev\SAM3D\unreal\SAM3D\SAM3D.uproject -WaitMutex -NoHotReload
```

Or double-click `SAM3D.uproject` and let Unreal compile it.

## First level: Osa practice scenario on Cesium terrain

1. Open `SAM3D.uproject`. **File → New Level → Empty Level**, save it as `Content/Maps/OsaPractice`.
2. **Window → Cesium**. Sign in to Cesium ion (or paste your own token in the Cesium panel yourself), then use
   **Quick Add → Cesium World Terrain + Bing Maps Aerial imagery**. This adds a `CesiumGeoreference`, the terrain
   tileset, and `CesiumSunSky` (lighting).
3. From Place Actors, drag in **Sim Scenario Director**. In its details:
   - **Scenario File**: `scenarios/osa_practice.json` (the default).
   - **Terrain Tileset**: pick the `Cesium World Terrain` actor.
4. Press **Play**. The director loads the scenario, moves the georeference origin onto the scenario origin
   (46.9° N, 19.7° E), samples terrain heights for the simulation after a few seconds, spawns the Osa placeholder
   at the unit's position and gives you control of it, and spawns a cone for the target drone.

In play:

| Keys | Action |
|---|---|
| W / S, A / D, Space | Drive, steer, brake. Stopping tells the sim the vehicle is at a halt. |
| Right mouse + drag | Orbit the camera |
| 1 | Commander: start the GTA (`power_up`) |
| 2 | Commander: search on the march (queued) |
| 3 | Commander: come into action at a halt (unlock the АПУ) |
| 4 | Range officer: acquire with the SOC |
| 5, 6 | Firing officer: prepare for firing; launch (queued) |
| 7 | Firing officer: TV-only engagement |
| 8, 9 | Commander: stow for the march; stow for transport |

The practice scenario also gives the crew these orders itself as the drone approaches (its triggers), so you can
just watch. The status line at the top shows power, the АПУ, the search antenna (it folds for transport), the
radars, missile preparation, the track and the lamps; debrief lines scroll below it (hits green, misses and
faults red) and go to the Output Log (`LogSAM3D`). Driving off with the АПУ unlocked faults it, as in the sim.

**Your Cesium ion token** is saved by the plugin under `Content/CesiumSettings/`, which is git-ignored so it
never reaches the repository.

## Code map

| File | Purpose |
|---|---|
| `Source/SAM3D/Public/SimCoreIncludes.h` | The only place Unreal code includes `simcore` (wrapped in `THIRD_PARTY_INCLUDES_*`). |
| `SimWorldSubsystem` | Runs the simulation at a fixed step: a `simcore::ScenarioRunner` (scenario mode) or the older demo `World`. Console API for Blueprints and C++ (controls, wheels, equipment states and progress, inputs, crew orders), debrief events, Cesium terrain sampling, coordinate conversion. |
| `SimScenarioDirector` | Level actor: loads the scenario, syncs the Cesium georeference, requests terrain, spawns and possesses the player's vehicle, spawns proxies, prints status and debrief. |
| `SimOsaVehicle` | Placeholder drivable Osa (engine box shapes): kinematic driving over the ground, turret follows the tracking antenna's azimuth, search antenna folds with the sim's stowage progress, number keys give crew orders. |
| `SimEntityActor` | Visual proxy for one target or missile. Subclass in Blueprint for real meshes and effects. |

## Coordinates

`simcore` works in ENU metres (x=East, y=North, z=Up) at the scenario origin. Convert only through
`USimWorldSubsystem::ToUnreal` / `ToEnu`:

- **With a Cesium georeference** (the director sets it): ENU → latitude/longitude/height → Unreal through the
  georeference. This matches Cesium's frame (at the origin, +X east, +Y south) and handles Earth curvature.
- **Without one**: a flat mapping, Unreal X = North, Y = East, Z = Up (cm) (`EnuToUnreal` / `UnrealToEnu`).

The sim treats the vehicle's hull as facing north when it measures azimuths; the placeholder turret subtracts the
hull heading so it still points the right way.

## Not done yet

- Chaos Vehicles physics for driving (needs a vehicle mesh with wheel bones); driving is kinematic.
- Real Osa model and deploy/stow animation beyond the placeholder antenna fold.
- The 3D cabin (phase 5) and the crew order menu (phase 6); crew orders are number keys for now.
