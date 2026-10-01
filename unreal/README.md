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

The level is in the repository (`Content/Maps/OsaPractice`, stored with Git LFS: run `git lfs install` once per
clone) and the editor opens it on startup. It holds a `CesiumGeoreference`, Cesium World Terrain with Bing Maps
aerial imagery, a `CesiumSunSky` and a **Sim Scenario Director** (scenario `scenarios/osa_practice.json`, terrain
tileset set). Don't place a Sim Osa Vehicle in it: the director spawns the player's.

1. Open `SAM3D.uproject`.
2. **Window → Cesium** and sign in to Cesium ion once per machine (or paste your own token). The token is saved under
   `Content/CesiumSettings/`, which git ignores; the level itself holds none.
3. Press **Play**. The director loads the scenario, moves the georeference origin onto the scenario origin
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
| C | Enter or leave the 3D cabin |

In the cabin (placeholder shapes from `data/cabins/osa_ak.json`):

| Input | Action |
|---|---|
| Mouse | Look around the seat |
| Tab | Next seat: driver, commander, range officer, firing officer |
| Left click | Flip a switch or cover, step a selector or key forward, press a button |
| Right click | Step a selector or key back |
| Left drag / scroll on a hand wheel | Turn it (a wheel lock must be released first) |

The instruments work: the SOC scope (sweep, fading echoes, the SSC azimuth line and range gate), the SSC range
indicator (two sweeps and the range counter), the βс and εс dials (lit by their ПОДСВЕТКА switches), the ДЦ
voltmeter, and the TOV monitor, which films the world along the SSC wheels in black and white.

The crosshair names the control under it and its position. Clicks go through the player's procedure tracker
(`USimWorldSubsystem::PlayerSetControl`), so mistakes reach the debrief; only physical refusals (a closed cover,
a key jumping a position) show on screen. The crew keeps working the same switches, so you see their actions too.

The practice scenario also gives the crew these orders itself as the drone approaches (its triggers), so you can
just watch. The status line at the top shows power, the АПУ, the search antenna (it folds for transport), the
radars, missile preparation, the track and the lamps; debrief lines scroll below it (hits green, misses and
faults red) and go to the Output Log (`LogSAM3D`). Driving off with the АПУ unlocked faults it, as in the sim.

**Your Cesium ion token** is saved by the plugin under `Content/CesiumSettings/`, which is git-ignored so it
never reaches the repository.

## Hands-free test run

Tick **Cabin Tour** on the Sim Scenario Director, or start the editor with the environment variable
`SAM3D_CABIN_TOUR=<seconds per view>`. On Play the camera enters the cabin and steps through every seat and a
close-up of every instrument while the scenario plays itself, logging each view (`LogSAM3D: Cabin tour view 4/10 at
t=78 s: Range officer - SOC indicator`). A run can then be checked from screenshots and the log without the
keyboard. C stops it.

```
set SAM3D_CABIN_TOUR=6
D:\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe C:\dev\SAM3D\unreal\SAM3D\SAM3D.uproject
```

## Scripting the open editor

Python remote execution is on (`Config/DefaultEngine.ini`), so scripts can drive the open editor: start and stop
Play, list, add or delete actors, set properties, save levels, take screenshots.

```
python tools/ue_remote.py "print(unreal.SystemLibrary.get_engine_version())"
python tools/ue_remote.py -f my_script.py
```

`tools/ue_cabin_check.py` uses it to check the cabin with no hands: it restarts Play with the cabin tour (the editor
must run with the tour on), records sim time, equipment, lamps, the track and a screenshot at every view, and writes
`out/cabin_check/<time>/report.md` and the debrief. `--at 70:tov.power=1` moves a control at a sim time.

## Code map

| File | Purpose |
|---|---|
| `Source/SAM3D/Public/SimCoreIncludes.h` | The only place Unreal code includes `simcore` (wrapped in `THIRD_PARTY_INCLUDES_*`). |
| `SimWorldSubsystem` | Runs the simulation at a fixed step: a `simcore::ScenarioRunner` (scenario mode) or the older demo `World`. Console API for Blueprints and C++ (controls, wheels, equipment states and progress, inputs, crew orders), debrief events, Cesium terrain sampling, coordinate conversion. |
| `SimScenarioDirector` | Level actor: loads the scenario, syncs the Cesium georeference, requests terrain, spawns and possesses the player's vehicle, spawns proxies, prints status and debrief. |
| `SimOsaVehicle` | Placeholder drivable Osa (engine box shapes): kinematic driving over the ground, turret follows the tracking antenna's azimuth, search antenna folds with the sim's stowage progress, number keys give crew orders, C enters the cabin. |
| `SimCabinComponent` | Builds the cabin's panels, controls and lamps from the loaded cabin layout and shows the console state on them every frame; tracing, hover text, clicks and wheel turns for the vehicle. |
| `SimCabinDisplayComponent` | One instrument: draws the scope, range indicator, dial or meter into a render target every frame (or films the TV picture with a scene capture) and shows it on a quad on the panel. Uses the ProceduralMeshComponent plugin and the engine's `Widget3DPassThrough_Opaque` material. |
| `SimCabinHUD` | Crosshair, hover text, feedback and key help while in the cabin. |
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
- Cabin: audio cues, a real interior (the cabin is floating placeholder panels inside the hull box, which is
  see-through from inside), real instrument faces.
- Corrections from the October 2026 research (roadmap phase 6): a fifth seat (the cabin has four; the real vehicle
  has commander, deputy commander, senior operator, operator and driver), missile containers and per-channel guidance
  lamps, the elevation display, a TV channel tracked by hand, and the vehicle's heading instead of "hull faces north".
- The PU-12 command post and plotting board (phase 7), and the crew order menu (phase 8); crew orders are number keys
  for now.
