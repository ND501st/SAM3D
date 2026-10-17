# Unreal project (`unreal/SAM3D`)

A C++ Unreal Engine 5.8 project that links the header-only `simcore` library from the repo root. It uses the
Cesium for Unreal plugin (the 5.8 build from Fab) for real terrain.

## Build

The module builds from the command line without opening the editor (close any running editor first; Live Coding
blocks command-line builds):

```
D:\UE_5.8\Engine\Build\BatchFiles\Build.bat SAM3DEditor Win64 Development -Project=C:\dev\SAM3D\unreal\SAM3D\SAM3D.uproject -WaitMutex -NoHotReload
```

Or double-click `SAM3D.uproject` and let Unreal compile it.

## Unreal MCP (for Claude Code)

The project enables Epic's **Unreal MCP** (`ModelContextProtocol`, experimental, UE 5.8+) and **All Toolsets**
plugins. With the editor open, the MCP server listens on `http://127.0.0.1:8000/mcp` (this PC only, no
authentication) and Claude Code connects to it through `.mcp.json` at the repo root. Python remote execution
(`tools/ue_remote.py`) stays on alongside it.

The server starts with the editor: `Config/DefaultEditorPerProjectUserSettings.ini` (in git) sets
`bAutoStartServer=True`, so every checkout and worktree has it. (The same switch is **Edit → Editor Preferences →
General → Model Context Protocol → Auto Start Server**; a user's own choice there overrides the default. It is
read once at editor start-up, so change it, then restart the editor.) If the server is ever off, run
`ModelContextProtocol.StartServer` in the editor console.

Order matters for Claude Code: it reads `.mcp.json` and connects when the session starts, and does not retry. Open
the editor first, then start Claude Code from the repo root. If the session came first, or the editor was restarted,
type `/mcp` in the session and reconnect `unreal-mcp`. Only one editor can hold port 8000, and a session talks to
whichever project's editor owns it: check the editor is the checkout you mean (a worktree has its own `Binaries/`
and `Saved/`; the Cesium ion token is also per checkout and not in git).

## Time compression

`sam3d.TimeScale <x>` (console variable; also `USimWorldSubsystem::SetTimeScale`) runs sim time faster or slower than
real time, from 0.25 to 32. A cold start takes minutes (roadmap 6.2: 3.8 min switch-on, 3.0 min function check), so
`sam3d.TimeScale 8` gets through it in under a minute. The sim stays on its fixed step; only how many steps a frame
runs changes.

## First level: Osa practice scenario on Cesium terrain

The level is in the repository (`Content/Maps/OsaPractice`, stored with Git LFS: run `git lfs install` once per
clone) and the editor opens it on startup. It holds a `CesiumGeoreference`, Cesium World Terrain with Bing Maps
aerial imagery, a `CesiumSunSky` and a **Sim Scenario Director** (scenario `scenarios/osa_practice_em.json`: the practice shoot with the EM layer behind the radars, since 16 Oct 2026; terrain
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
| 1 | Power up: driver starts the GTA (`power_up`), operator switches on the equipment (`equipment_on`) |
| 2 | Senior operator: search on the march (waits for power) |
| 3 | Come into action at a halt: operator unlocks the АПУ and switches on the drives, driver puts the generator on line, deputy commander orients the vehicle (gyro to the crew chief's Qc, `orient_vehicle`) |
| 4 | Acquire: senior operator with the SOC, deputy commander readies ЦУ АЗ (queued after orienting), operator switches on the SSC |
| 5, 6 | Deputy commander: prepare for firing; launch (both queued) |
| 7 | TV-only engagement: senior operator turns the azimuth, deputy commander does the rest |
| 8 | Stow for the march: operator (SSC off, drives off, АПУ locked), deputy commander, driver |
| 9 | Shut down: senior operator stops the SOC, operator switches the equipment off, driver shuts down the GTA (queued) |
| C | Enter or leave the 3D cabin |

In the cabin (placeholder shapes from `data/cabins/osa_ak.json`):

| Input | Action |
|---|---|
| Mouse | Look around the seat |
| Tab | Next seat: driver, commander, senior operator, operator, deputy commander |
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
t=78 s: Senior operator - SOC indicator`). A run can then be checked from screenshots and the log without the
keyboard. C stops it.

```
set SAM3D_CABIN_TOUR=6
D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe C:\dev\SAM3D\unreal\SAM3D\SAM3D.uproject
```

## Cabin parts library

The cabin's knobs, toggles, lamps, buttons, jacks, sockets, latches and hand wheels are meshes built from code in
Blender: `tools/blender/build_cabin_parts.py` writes `art/cabin_parts/*.fbx` and `cabin_parts.blend`, and
`tools/ue_import_cabin_parts.py` imports them into the open editor as `/Game/SAM3D/Cabin/Parts/SM_*` (all through Git
LFS). Each part is 1 m across at unit scale; the cabin scales and tints it from `data/cabins/<system>.json`, and falls
back to engine basic shapes for any part that isn't imported.

```
"C:\Program Files\Blender Foundation\Blender 4.2\blender.exe" --background --python tools/blender/build_cabin_parts.py -- --preview out/parts_preview.png
python tools/ue_import_cabin_parts.py
```

## Battery support vehicles

Placeholder exteriors (no interiors) for the battery's other vehicles: the TZM 9T217 transloader, the ATZ-3.8-131 fuel
truck and the PU-12M command post, at their published sizes. `tools/blender/build_support_vehicles.py` writes
`art/support_vehicles/SM_*.fbx` (Git LFS); `tools/ue_import_support_vehicles.py` imports them as
`/Game/SAM3D/Vehicles/SM_TZM_9T217`, `SM_ATZ_38_131` and `SM_PU12M` (editor closed, through the commandlet).
`ASimEntityActor::UseVehicleModel` gives battery entities of kind `reload`, `fuel` and `command_post` these meshes and
falls back to a box when one isn't imported. The PU-12M at full detail, with its interior: `docs/PU12_DEVELOPMENT.md`.

```
"C:\Program Files\Blender Foundation\Blender 4.2\blender.exe" --background --python tools/blender/build_support_vehicles.py -- --preview out/support_vehicles
D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject -run=pythonscript -script="C:/dev/SAM3D/tools/ue_import_support_vehicles.py" -unattended -nullrhi
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

## Electromagnetic layer (radar, ESM, jamming, C2)

`FSimEmLayer` (`Public/SimEmLayer.h`) runs simcore's electromagnetic layer inside `USimWorldSubsystem::Tick` at a
fixed 10 Hz and draws its debug overlays with `DrawDebug*`. For now it runs the MVP demo scenario
(`simcore/em/demo_scenario.hpp`, generic placeholder equipment) at the current sim origin and terrain; linking it to
the Osa scenario's console and scopes is roadmap phase E2/E3. Console (PIE or game):

| Command | Effect |
|---|---|
| `sam3d.em.Start` / `sam3d.em.Stop` | start / stop the EM demo |
| `sam3d.em.Debug <mask>` | overlays: 0 none, -1 all; 1 truth, 2 emitters, 4 beams, 8 coverage, 16 line of sight, 32 signal paths, 64 bearings, 128 fixes, 256 uncertainty, 512 tracks, 1024 C2 links, 2048 comms traffic, 4096 jamming, 8192 status, 16384 detection |
| `sam3d.em.Owner <node>` | whose knowledge the overlays show: 0 command posts, -1 every node |

Blueprint/C++: `StartEmDemo`, `StopEmDemo`, `IsEmDemoRunning`, `GetEmTimeSeconds`, `GetEmStatusText` on
`USimWorldSubsystem`. The layer is stopped when the scenario, the demo world or the sampled Cesium terrain is
replaced (it holds a pointer to the active world's terrain). Overlays are rebuilt after each EM step and redrawn
every frame. Design: `docs/EM_ARCHITECTURE.md`; handover: `docs/EM_HANDOFF.md`.

## Coordinates

`simcore` works in ENU metres (x=East, y=North, z=Up) at the scenario origin. Convert only through
`USimWorldSubsystem::ToUnreal` / `ToEnu`:

- **With a Cesium georeference** (the director sets it): ENU → latitude/longitude/height → Unreal through the
  georeference. This matches Cesium's frame (at the origin, +X east, +Y south) and handles Earth curvature.
- **Without one**: a flat mapping, Unreal X = North, Y = East, Z = Up (cm) (`EnuToUnreal` / `UnrealToEnu`).

Headings go through `YawToHeadingDeg` / `HeadingToYawDeg`: Unreal yaw is clockwise from north only in the flat
mapping; with the Cesium georeference Unreal X points east at the origin. The vehicle reports its true heading to the
sim each tick, the director spawns it facing the scenario's heading, and the turret turns by the wheel's true azimuth
minus the hull's true heading.

## Crew and teamwork (phase 8)

The crew pillars (`docs/crew/README.md`) decide what Unreal shows of the crew: in the crew, the player in one seat
with the drill master's assistance overlay (the best next control highlighted, with why and when: `docs/crew/08_*`);
in command, the order menu (`docs/crew/07_*`) with honest greying, the crew's callouts as voice and HUD lines, and the
crew's picture as they reported it. Built so far: `USimWorldSubsystem::OrderCrew` (orders by procedure id), the
order hotkeys of `ASimOsaVehicle`, `GiveCrewOrder` (a whole order of `data/crew/<system>_crew.json`, e.g.
`order_k_boyu`: the order menu's backend) and `GetDrillAdvice` (a seat's best next move toward a task, with why and
when: the overlay's backend). The crew's callouts reach the HUD as radio lines (net "intercom").

In the cabin:

- **H** cycles the training overlay: guided (the drill master's next control for this seat pulses green, a ghost hand
  shows the move, the HUD says what, why and when), hint (the same once you hesitate), demonstrate (the seat's NPC does
  it while the overlay names his move), off. A move off the drill shows what it cost. An exam scenario keeps it off.
- **O** opens the crew menu: at the commander's seat the orders of the crew data, at any other seat that seat's
  reports. 1-9 choose, 0 next page, Esc close. Orders that can't be carried out now are greyed with the reason, and the
  crew answers "Не могу" if you give one anyway.
- **T** takes this seat from its NPC (his orders become yours, the crew waits for you and reminds you) or gives it back.

With the board (B) open, the battery's other launchers are listed under it (missiles, target, phase). To play the crew
half, set the director's ScenarioFile to `scenarios/osa_crew_practice.json` (NPC commander) or
`scenarios/osa_mission_march.json`.

**Bodies and voices** (roadmap 8.10, `docs/crew/10_BODIES_AND_VOICES.md`):

- `USimCrewBodiesComponent` (on the vehicle, in the cabin's frame) draws each NPC crewmate working his controls. Its
  poses come from simcore's `BodyAnimator`. The body at the camera's seat is hidden.
- The meshes are built from code and imported with the editor closed. Engine shapes stand in for any not imported.

  ```
  "C:\Program Files\Blender Foundation\Blender 4.2\blender.exe" --background --python tools/blender/build_crew_body.py
  D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe C:/dev/SAM3D/unreal/SAM3D/SAM3D.uproject -run=pythonscript -script="C:/dev/SAM3D/tools/ue_import_crew_body.py" -unattended -nullrhi
  ```
- `USimCrewVoiceComponent` speaks every callout in its seat's voice through Windows' speech engine (`FSimSpeech`,
  SAPI). It speaks Russian when a Russian voice is installed (Settings > Time & language > Speech > Add voices),
  English otherwise. Recorded lines in `art/crew_voices/<language>/[<seat>/]<callout>.wav` take precedence.
- Console: `sam3d.Voice 0/1`, `sam3d.VoiceLanguage auto|ru|en`, `sam3d.VoiceVolume`.
- Headless checks:

  ```
  UnrealEditor-Cmd.exe SAM3D.uproject -ExecCmds="Automation RunTests SAM3D.Crew;Quit" -unattended -nullrhi
  ```

## Not done yet

- Chaos Vehicles physics for driving (needs a vehicle mesh with wheel bones); driving is kinematic.
- Real Osa model and deploy/stow animation beyond the placeholder antenna fold.
- Cabin: audio cues, a real interior (the cabin is floating placeholder panels inside the hull box, which is
  see-through from inside), real instrument faces.
- Corrections from the October 2026 research (roadmap phase 6): a fifth seat (the cabin has four; the real vehicle
  has commander, deputy commander, senior operator, operator and driver), missile containers and per-channel guidance
  lamps, the elevation display and a TV channel tracked by hand (simcore has these now; the cabin shows some).
- The PU-12 command post and plotting board (phase 7), and the crew order menu (phase 8); crew orders are number keys
  for now.
- The electromagnetic layer drives no scope or lamp yet; it runs as a separate demo with overlays (roadmap E2/E3).

Early-warning source (roadmap 7.8): `USimWorldSubsystem::RequestC2Source`, `GetActiveC2Source`, `GetC2SourceIds` (Blueprint, category SAM3D|Command) written 11 Oct 2026, NOT YET BUILT (editor was open). Still to do: a UI/cabin control for it.

Multiplayer knowledge (roadmap E10): add a `SimKnowledge` component (`USimKnowledgeComponent`) to the PlayerController. The server sends that player its node's view (NodeId 0 = the scenario unit's own node) twice a second; read it with `GetPictureTracks`, `GetEsmContacts`, `GetSensorTracks` or bind `OnKnowledgeUpdated`. Headless test: `UnrealEditor-Cmd.exe SAM3D.uproject -ExecCmds="Automation RunTests SAM3D.Knowledge;Quit" -unattended -nullrhi`.

Battery, board and radio (roadmap 7.7): the practice map plays `scenarios/osa_battery_practice.json` (the whole battery in a prepared position). The battery's other vehicles and the dummy positions appear as box stand-ins (`ASimScenarioDirector::GroundActorClass` / `GroundScale`); B shows the PU-12 plotting board; radio traffic runs as lines at the bottom left (`ASimCabinHUD`).

Multiplayer crew: `ASimCrewGameMode` gives every player a crew seat and a knowledge stream; a network client mirrors the server's simulation and never steps its own. Details and checks in `docs/MULTIPLAYER.md`; networked smoke test `tools/ue_crew_smoke.ps1` (dedicated server and two headless clients; `-Pu12m` for the command post).

The PU-12M (`docs/PU12_DEVELOPMENT.md`): when the scenario's own vehicle is the command post (`scenarios/pu12m_deploy.json`, `pu12m_battery.json`), `ASimOsaVehicle` wears `SM_PU12M` and its mast (`USimPu12MastComponent`) instead of the Osa's boxes, and the director loads `cabins/pu12m.json` (an empty `CabinFile` loads the scenario's own vehicle's cabin; "none" loads none). In the cabin: at the battery commander's seat `[` `]` pick a target on the post's board and 1-5 give it to that launcher on the battery net; at the operator-plotter's seat `[` `]` pick a target on the IT-45 and P writes it on the board (take the seat with T first). Art: `tools/blender/pu12m_model.py` (exterior), `tools/blender/build_pu12m_interior.py` (interior), imported headless with the editor closed by `UnrealEditor-Cmd.exe SAM3D.uproject -run=pythonscript -script=C:/dev/SAM3D/tools/ue_import_pu12m.py -unattended -nullrhi` (`SAM3D_PU12M_STEPS=interior` for the interior alone). A battery link (`-SimScenario=scenarios/battery_link_practice.json`, docs/PU12_DEVELOPMENT.md P8) puts the Osa and the PU-12M in one battery: the player starts in the Osa (`-SimVehicle=post` for the post) and **V** moves him to the other vehicle, whose crew he leaves behind fighting on as NPCs. Networked, each player joins in his vehicle (`-SimVehicle=post` or `osa`; docs/MULTIPLAYER.md). The interior is imported without Nanite (its fallback mesh made the round parts polygons) and with finer distance fields, set through `USimEditorScripting` (a commandlet has no static mesh editor subsystem, and the import pipeline's build settings don't reach the mesh). The compartment is closed: while the camera is in it the level's sky lights are put out (`ASimOsaVehicle::SetDaylightShut`; Lumen otherwise tints the cream lining mauve), and from outside the cabin is hidden (`USimCabinComponent::SetCabinHidden`). At the battery commander's seat the instruments work too: the DT-2's two wheels put the IT-45's marker on a target, the ДХ's digits pick a number, ЦУ and a digit give it to that launcher, СБР clears.

Command-line options for scripted runs: `-SimScenario=scenarios/<file>.json` plays that scenario whatever the map's director says (server and clients alike). `-SimCabinShots=<folder> [-SimShotDelay=<s>]` photographs every seat and instrument of the cabin after the delay (default 20 s), one PNG each, then quits: with `UnrealEditor.exe SAM3D.uproject -game -RenderOffscreen -ResX=1280 -ResY=720` a run is checked from its pictures alone, no window and no editor (the PU-12M's: `-SimScenario=scenarios/pu12m_battery.json -SimCabinShots=C:/dev/SAM3D/out/pu12m_cabin_shots -SimShotDelay=40`). `-SimShotsAround` adds five more views from every seat (left, right, behind, down, up) to check the cabin all round; `-SimVehicle=post` photographs the post in a battery link.

