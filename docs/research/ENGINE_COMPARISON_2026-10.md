# Engine and technology comparison for the EM layer (October 2026)

Question: is Unreal Engine the right platform for a simulation-heavy game whose core is a radar,
signals, EW and command environment — or would another engine, or a custom approach, serve better?

**Short answer:** keep the hybrid the project already has — an **engine-agnostic C++ simulation core
(simcore) with Unreal Engine 5 as the presentation front end**. The electromagnetic layer added in
October 2026 runs entirely in simcore (tests, tools and the MVP scenario run headless in under a
second), so the engine question only concerns presentation, and for a first-person, cabin-centred,
real-terrain military simulation Unreal is the strongest option available to a solo developer.

## 1. What the EM layer needs from an engine

| Need | Where it is met |
|---|---|
| Deterministic fixed-step simulation of hundreds of entities, sensors and links | simcore (C++17, header-only, no engine types) |
| Double precision over 100–300 km theatres | simcore (doubles, ENU, geodesy); engine: large-world coordinates for rendering |
| Real terrain heights for line of sight | simcore height grid sampled once from the engine's terrain (Cesium) — never the streamed render mesh |
| Debug overlays (beams, strobes, ellipses, links) | simcore produces primitives; engine draws lines/text |
| First-person cabin, crew animation, lighting | Engine |
| Networking (later) | Engine transport; simcore authority and per-player knowledge |
| Tooling, profiling | Engine profiler for presentation; simcore benchmarks and tests outside the engine |

The simulation's demands are met independently of the engine. The engine is chosen for what only an
engine does well here: the 3D cabin and vehicles, real-world terrain, animation, audio, UI, and
eventually multiplayer transport.

## 2. Options compared

Scores: ●●● strong, ●● adequate, ● weak for *this* project.

| Criterion | Unreal Engine 5 | Unity 6 | Godot 4 | O3DE / Flax / Bevy | Custom C++ + thin renderer |
|---|---|---|---|---|---|
| Cost | Free to use; 5 % royalty above US$1 M gross per product (Epic EULA) | Personal free under US$200 k; Pro ~US$2,200/seat/yr; runtime fee cancelled Sept 2024 | Free, MIT | Free (Apache/MIT); Flax source-available with royalty | Free; your time |
| C++ simulation core | ●●● native: simcore compiles into the module (done) | ● C# engine; C++ via native plugin and P/Invoke marshalling | ●● GDExtension C++ | ●●/● (Bevy needs Rust or FFI) | ●●● |
| Large worlds | ●●● LWC doubles, World Partition | ●● floating origin; camera-relative rendering | ●● double-precision build option (`precision=double`) | ●/●● | depends on renderer |
| Real-world terrain | ●●● Cesium for Unreal (already used) | ●●● Cesium for Unity | ● no official Cesium plugin; community terrain add-ons | ● | ●● osgEarth/VSG (no game tooling) |
| Cabin realism (lighting, materials, animation) | ●●● Lumen/Nanite, Control Rig, MetaHumans | ●● HDRP | ● | ●/●● | ● |
| AI tooling | ●●● StateTree, Behavior Trees, EQS, Smart Objects | ●● | ● | ● | ● |
| Networking | ●●● replication, Iris, dedicated servers | ●● Netcode for GameObjects/Entities | ●● high-level multiplayer | ● | ● |
| Profiling and debugging | ●●● Unreal Insights, DrawDebug, Visual Logger, Gameplay Debugger | ●●● Profiler, Gizmos | ●● | ●/●● | build your own |
| Ecosystem and documentation | ●●● | ●●● | ●● growing | ● | ● |
| Solo developer friction | ●● heavy editor, long C++ builds | ●● | ●●● light, fast iteration | ● | ● (everything is yours) |
| Long-term maintainability | ●●● with simcore isolated | ●● | ●● | ● | ●● |
| Fit to the existing project | ●●● cabin, scenario mode, Cesium, scripts | ● rewrite presentation | ● rewrite presentation | ● | ● |

Notes:

- **Unity** is a credible alternative: Sea Power (2025) is a physically modelled naval sensor game on
  Unity, and Cesium for Unity exists. But simcore would sit behind a C-ABI shim, every type crossing
  it marshalled, and the cabin work done in Unreal would be redone. Licence history (the 2023 runtime
  fee, cancelled in September 2024) is a reason for caution, not a disqualifier.
- **Godot 4** is attractive for cost and iteration speed and supports C++ via GDExtension and
  double-precision builds, but real-world terrain streaming, a first-person cabin at the fidelity
  already reached, and multiplayer tooling would all be bigger jobs.
- **O3DE, Flax, Bevy**: smaller ecosystems; Bevy's ECS is elegant but would mean Rust or FFI.
- **Custom engine**: simcore *is* the custom part. A bespoke renderer would spend the project's time
  on what engines already do. Lightweight custom front ends remain valuable for development: the
  browser viewer (`tools/em_view.py`) and Tacview replays (`.acmi`) are exactly that.
- **Simulation renderers** (Unigine Sim, osgEarth/VSG, CesiumJS) suit planning maps and training
  systems; CesiumJS could host a future web planning/debrief map fed by simcore output.

## 3. Recommendation

Keep **Unreal Engine 5 for presentation** and **simcore for everything that decides an outcome**:

1. The simulation stays testable, deterministic and portable (Mac, CI, a dedicated server, tools),
   and the engine decision stays reversible — the expensive asset is simcore, not the engine glue.
2. Unreal gives the project things it already uses and cannot easily replace: Cesium terrain, the
   3D cabin pipeline, large-world rendering, and a path to multiplayer.
3. The remaining risks (heavy editor, compile times) are managed by doing all simulation work in
   simcore, building Unreal from the command line, and keeping Unreal code thin.

## 4. How the EM layer maps onto Unreal

| Unreal feature | Use | Do not use for |
|---|---|---|
| **World subsystem** (`USimWorldSubsystem`) | Owns simcore; steps the scenario and the EM layer at fixed rates; converts ENU ↔ Unreal (through the Cesium georeference) | Per-actor ticking of sim logic |
| **C++ vs Blueprint** | C++: adapters (`FSimEmLayer`, components, subsystem API). Blueprint: HUD/UMG scopes and status panels reading knowledge, level scripting | Any rule that changes an outcome |
| **Actors/components** | Visual proxies (vehicles, aircraft), antenna rotation from `EmitterState::pointing_az`, cabin scopes from radar plots and strobes | Sensors (no `UAIPerception`: it would see truth) |
| **Mass Entity** | Many visual proxies (instancing, LOD) if the theatre grows to hundreds of moving objects | The simulation itself (simcore keeps its own component arrays so it stays engine-free) |
| **Gameplay Ability System** | — | Sensors, EW, C2 (it models abilities and attributes, not signals) |
| **Data Assets / Data Tables** | Optional read-only views of `data/em/*.json` for editor browsing | A second source of truth (the JSON in `data/` is loaded by simcore, tests and tools) |
| **World Partition / Cesium streaming** | Rendering | Line of sight: LOS uses a height grid sampled once (`RequestCesiumTerrain`), so results never depend on what the camera has streamed in |
| **Large World Coordinates** | Rendering at 100+ km | — (simcore is double precision in ENU) |
| **StateTree / Behavior Trees** | Crew animation, vehicle driving, presentation behaviour; later, higher-level AI that issues orders | Detection or identification decisions (those are doctrine on knowledge, in simcore) |
| **Networking / Iris** | Server-authoritative simcore; replicate each player's *knowledge* and their own vehicle's state, never global truth — this is also anti-cheat | Client-side simulation of sensors |
| **Multithreading** (`UE::Tasks`, `ParallelFor`) | Later: run the EM step off the game thread with double-buffered outputs; per-sensor parallelism inside simcore (independent sensors, counter-based RNG) | Touching UObjects from worker threads |
| **Profiling** (Unreal Insights, `stat`) | Frame cost of the EM step and the overlays (`TRACE_CPUPROFILER_EVENT_SCOPE` around `FSimEmLayer::Advance`) | — (algorithmic profiling is done with `em_bench`) |
| **Debugging** | `DrawDebug*` overlays (`sam3d.em.Debug`), later a Gameplay Debugger category and Visual Logger entries for doctrine decisions | — |

## 5. Sources

- Unreal Engine documentation: [Large World Coordinates](https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-in-unreal-engine-5), [Mass Entity / Mass Gameplay](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-mass-gameplay-in-unreal-engine).
- Unity: ["Unity is canceling the Runtime Fee"](https://unity.com/blog/unity-is-canceling-the-runtime-fee) (12 Sep 2024).
- Godot: [Large world coordinates](https://docs.godotengine.org/en/4.2/tutorials/physics/large_world_coordinates.html) (`precision=double`, GDExtension `REAL_T_IS_DOUBLE`).
- Sea Power: [Steam / Triassic Games notes](https://sea-power.pmctactical.org/).
- Cesium: [Cesium for Unreal](https://cesium.com/platform/cesium-for-unreal/).
