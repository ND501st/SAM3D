# Drawing a large, data-generated map in UE 5.8: geometry, foliage, haze, measurement

Research notes for the terrain roadmap, 2026-11-03. Engine on this PC: UE **5.8.3** (`D:/UE_5.8/Engine/Build/Build.version`,
changelist 58210709). Every claim carries its source. "Local:" means a file of the installed engine (headers, plugin
descriptors, binaries' strings), read directly; those are the strongest evidence for 5.8 behaviour. Forum posts are marked
as such and are weaker. Nothing was downloaded; only pages and local files were read.

The problem: a 40 km play map (detail areas at 10 m and 20 m, the map proper at 50 m spacing, far ground at 500 m out to
150 km: `data/maps/kesmark_range.json` `"core_spacing_m": 50, "far_spacing_m": 500, "far_km": 150`), about 240 000 trees,
12 000 buildings, every position from simcore, and the drawn ground equal to the simulation's triangles (line of sight is
computed from the same ground).

---------------------------------------------------------------------------------------------------------------------------

## 0. Where SAM3D stands now (read from the code)

- The ground is `UProceduralMeshComponent` tiles, one component per 50 x 50 cells, collision cooked async, shadows only on
  the core levels (`unreal/SAM3D/Source/SAM3D/Private/SimMapActor.cpp` around lines 825-840: `NewObject<UProceduralMeshComponent>`,
  `bUseAsyncCooking = true`, `CreateMeshSection_LinearColor`). The split of each quad follows simcore's `MapGrid::SampleTri`
  (comment in the same function): the contract that keeps the drawn ground equal to the simulated one.
- Trees, wood masses and buildings are `UHierarchicalInstancedStaticMeshComponent`s of meshes built at runtime by
  `FSimRuntimeMesh::Build` (`Private/SimRuntimeMesh.cpp`): `UStaticMesh::BuildFromMeshDescriptions` with
  `Params.bFastBuild = true`, `bCommitMeshDescription = false`, transient package. Trees are culled at
  `TreeCullDistanceM = 8000` (`Public/SimMapActor.h`).
- The Osa's TV sight is drawn by a scene capture (`SimCabinDisplayComponent.cpp` uses `SceneCapture`), with fields of
  view 4.8 deg and 1.44 deg (`data/procedures/osa_ak.json` line 255, `"fov_deg": [4.8, 1.44]`). This matters for every
  distance-based cull below (section 3.6).

Consequences, from the sections below: the PMC ground has no LODs, no mesh distance field, no Lumen cards, no Nanite, and
is drawn on the dynamic path; the fast-built runtime meshes have no Nanite, no distance field and (inferred) no Lumen
cards, so the trees and buildings are not in Lumen's surface cache and render into virtual shadow maps the expensive
non-Nanite way.

---------------------------------------------------------------------------------------------------------------------------

## 1. Runtime geometry: ProceduralMeshComponent and the alternatives

### 1.1 ProceduralMeshComponent (PMC)

| Feature | PMC in 5.8 | Source |
|---|---|---|
| Mesh distance field | **No.** Distance fields are built offline; a PMC cannot generate them. | Forum, Sam_Swain 14 Jun 2022: "Unfortunately distance fields are built offline and not available at runtime, largely due to the high cost of building them." https://forums.unrealengine.com/t/uproceduralmeshcomponent-with-distance-fields/354509 ; forum, 2022: PMC "not fully supported by Lumen", "can't automatically generate" mesh distance field data https://forums.unrealengine.com/t/the-type-dynamic-mesh-that-are-best-supported-by-lumen-in-ue5-preview/501937 |
| Lumen software RT | **No** (SWRT represents only static meshes, ISM, HISM and landscape). | Epic, Lumen Technical Details: "Only Static Meshes, Instanced Static Meshes, Hierarchical Instanced Static Meshes, and Landscape terrain" are in the Lumen Scene for SWRT. https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine |
| Hardware RT | **Yes**, through `GetDynamicRayTracingInstances`, gated by `r.RayTracing.Geometry.ProceduralMeshes` (default 1). | Code excerpt of `ProceduralMeshComponent.cpp` in the cvar wiki: https://indxzero.github.io/ue544cvarwiki/articles/r.raytracing.geometry.proceduralmeshes |
| Lumen surface cache (cards) | Not documented; no card API in the PMC header (local: `Engine/Plugins/Runtime/ProceduralMeshComponent/Source/ProceduralMeshComponent/Public/ProceduralMeshComponent.h` has no Lumen, distance field or card members). Meshes without cards "don't bounce light and appear black in reflections" (Lumen Technical Details, above). | local header; Lumen Technical Details |
| Nanite | **No.** Nanite's supported component list is static, skeletal, ISM, HISM, spline, geometry collection, foliage and landscape grass; PMC is not on it. | https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine |
| LODs | **No.** The class hides the LOD category: `UCLASS(MinimalAPI, hidecategories = (Object, LOD), ...)` (local header line 148). | local |
| Draw path | Dynamic relevance only: mesh batches gathered every frame, no cached static draw commands. Koderz 2016: "The ProceduralMeshComponent only implements dynamic relevance currently"; reply: "PMC is currently set up to be entirely dynamic and not optimized for the static case." (old thread; no newer contradiction found, header shows no static draw option). | https://forums.unrealengine.com/t/procedural-mesh-component-rendering-performance-concerns/355859 |
| VSM caching | Epic: "if a mesh is a static mesh and is set to Static mobility, it will be cached as static"; a PMC is not a static mesh. "Non-Nanite geometry is much more expensive to render into VSMs than Nanite geometry." | https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine |
| Collision | Yes, async cooking (`bUseAsyncCooking`, local header line 285). | local |

### 1.2 DynamicMeshComponent (Geometry Framework, engine runtime module)

Read from the 5.8.3 headers (local: `Engine/Source/Runtime/GeometryFramework/Public/Components/BaseDynamicMeshComponent.h`,
`BaseDynamicMeshSceneProxy.h`, `Private/Components/DynamicMeshSceneProxy.h`):

- **Draw path selectable**: `EDynamicMeshDrawPath { DynamicDraw = 0 /* meshes that update on most frames */, StaticDraw = 1 /* meshes that do not change on most frames */ }`,
  property `DrawPath` (default DynamicDraw). The proxy sets `bStaticRelevance` when the static path is used
  (`DynamicMeshSceneProxy.h` lines 660-662). Note: "fast update is not compatible with static draw path".
- **Hardware ray tracing**: `bEnableRaytracing = true` by default; the header warns that in truly dynamic use it "has
  additional renderthread performance costs and does not currently support partial updates in the SceneProxy".
- **Lumen cards**: the proxy implements `GetMeshCardRepresentation()` and `UpdateLumenCardsFromBounds()` ("set (or clear)
  simple Lumen cards via the bounding box"; "TODO: Implement a better method"). A cvar `geometry.DynamicMesh.AllowMeshCardGeneration`
  exists in the 5.8 binary (local: strings of `UnrealEditor-GeometryFramework.dll`). Box cards suit a heightfield tile
  seen from above.
- **Distance fields: deprecated.** `UE_DEPRECATED(5.6, "Dynamic Mesh distance field support has been deprecated")` on
  `DistanceFieldMode`, `SetDistanceFieldMode` (now an empty body), `ComputeDistanceFieldForMesh`. The 2024 forum report of
  "Initial Lumen support for DynamicMeshComponent" with an async CPU SDF (https://forums.unrealengine.com/t/looks-like-lumen-support-for-dynamic-meshes-is-coming-possibly-in-ue-5-5/1951349)
  is therefore out of date: the SDF part was withdrawn in 5.6; Lumen on a DMC is HWRT plus bounding-box cards. The same
  thread says with Dynamic Draw Lumen worked only with HWRT hit lighting (community test on ue5-main, mid-2024).
  The Python enum page still documents the values (https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/DynamicMeshComponentDistanceFieldMode?application_version=5.7).
- **Nanite**: not on Nanite's component list (Nanite page above).
- **Collision**: complex collision silently capped by `geometry.DynamicMesh.MaxComplexCollisionTriCount` (local: cvar in
  the binary; forum: default 250 000 triangles, https://forums.unrealengine.com/t/udynamicmeshcomponent-max-triangles-for-complex-collision/2500212).
- **Costs**: keeps an `FDynamicMesh3` on the CPU per component (it is an editable mesh); forum users report steady slowdown
  from repeated edits (https://forums.unrealengine.com/t/dynamic-mesh-components-and-loss-performances/2079885). For a
  ground that never changes after load the static path avoids per-frame gathering.

### 1.3 RealtimeMeshComponent (TriAxis Games)

- **Licence: MIT** for the free Core edition, copyright TriAxis Games 2015-2026: "to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell", keep the notice.
  https://raw.githubusercontent.com/TriAxis-Games/RealtimeMeshComponent/master/LICENSE.txt ; repo https://github.com/TriAxis-Games/RealtimeMeshComponent
  ("UE 5.5 – 5.8").
- Core: "Up to 8 LODs with dithering", "Full collision (static + dynamic) & NavMesh", "Async collision updates"; static
  and dynamic section draw modes (tier not stated). https://triaxis.games/realtime-mesh
- Pro ($49 on Fab, Studio $249.99 above $100K revenue): "GPU + CPU mesh→SDF generation with Lumen card support" and "a
  runtime Nanite builder" (repo README; triaxis page). **Paid: excluded by the project's free-only rule.** Vendor claims,
  not verified.

### 1.4 A UStaticMesh built at runtime (`BuildFromMeshDescriptions`), as SAM3D's `FSimRuntimeMesh` does

- `FBuildMeshDescriptionsParams::bFastBuild`: "Specifies that the mesh will be built by the fast path (**mandatory in
  non-editor builds**)". `bUseHashAsGuid`: "Uses a hash as the GUID, useful to prevent recomputing content already in
  cache." (local: `Engine/Source/Runtime/Engine/Classes/Engine/StaticMesh.h` lines 1838-1891). Epic support (Stephen
  Kelly, Sep 2025): `bDoFastBuild` "is intended to mark runtime-created Static Meshes for fast build"
  https://forums.unrealengine.com/t/failed-to-generate-nanite-data-for-this-mesh-we-want-to-know-the-reason/2670678
- **No Nanite at runtime**: the Nanite builder lives in `Engine/Source/Developer/NaniteBuilder` and the distance field and
  Lumen card generators in `Engine/Source/Developer/MeshUtilities/Public/MeshUtilities.h`
  (`GenerateSignedDistanceFieldVolumeData`, `GenerateCardRepresentationData`) (local). Epic's API index: Developer modules
  are compiled "only in non-shipping build configurations" and "are not included in Shipping builds"
  https://dev.epicgames.com/documentation/unreal-engine/API/Developer . Cesium (azrogers, May 2025): "Nanite meshes cannot
  yet be created at runtime" https://community.cesium.com/t/cesium-for-unreal-5-5-and-nanites-when/40713 . So a
  runtime-built static mesh gets LODs (one MeshDescription per LOD), the static draw path and, with
  `bSupportRayTracing` (default true: "a ray tracing acceleration structure will be built for this mesh", local StaticMesh.h
  line 1383), HWRT geometry; but **no Nanite, no distance field and (inferred from the above) no Lumen cards**.
- Costs: CPU build time at load (render buffers only, no Nanite or DF build), memory of LOD buffers; drawn through cached
  mesh draw commands like any static mesh.

### 1.5 Mesh Terrain / Mesh Partition (new in 5.8, Experimental)

- "A next-generation mesh-based terrain solution to support large worlds using 3D modeling, layering, virtual texture"
  (5.8 release notes https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes); plugin
  descriptor `"IsExperimentalVersion": true` (local `Engine/Plugins/Experimental/MeshPartition/MeshPartition.uplugin`).
- Compiled sections are built in the editor and "serialized for runtime"; the Static Mesh Transformer enables Nanite and
  runs a simplifier per LOD (LOD0 is source geometry only with `bReduceMesh = false`).
  https://dev.epicgames.com/documentation/unreal-engine/mesh-partition-definition-in-unreal-engine?lang=en-US
- Not creatable at runtime in a packaged build (community answer, Jun 2026, no Epic reply)
  https://forums.unrealengine.com/t/does-5-8-mesh-terrain-support-creation-of-a-terrain-at-runtime-in-a-packaged-build/2730244/1 ;
  missing from software Lumen until a distance field is generated by hand (bug report Jun-Aug 2026)
  https://forums.unrealengine.com/t/mesh-terrain-doesnt-work-with-software-lumen/2730678 . Works natively with PCG
  (https://80.lv/articles/unreal-engine-5-8-is-out-today-with-big-optimization-improvements-and-mesh-terrain).
- Verdict for SAM3D: not now (experimental, editor-authored, its own triangulation); re-evaluate when it leaves
  Experimental, since it is Epic's intended non-heightfield terrain.

### 1.6 Capability matrix (the ground's needs)

| | Exact sim triangles | LODs | Static draw | HWRT | Lumen cards | Mesh DF | Nanite | Static VSM cache | Shipping-safe |
|---|---|---|---|---|---|---|---|---|---|
| PMC (now) | yes | no | no | yes | no (none documented) | no | no | no (not a static mesh) | yes |
| DynamicMeshComponent, StaticDraw | yes | no (one mesh) | yes | yes | bounding-box cards | deprecated | no | unverified | yes |
| RMC Core (MIT) | yes | up to 8 | yes (mode) | claimed | Pro only | Pro only | Pro only | unverified | yes |
| Runtime UStaticMesh (fast build) | yes (LOD0) | yes | yes | yes | no (inferred) | no | no | yes if Static mobility | yes |
| **Baked UStaticMesh asset (editor)** | yes at full detail; Nanite simplifies with distance | Nanite or LODs | yes | yes (Nanite fallback mesh) | yes (12 by default) | yes | **yes** | **yes** | yes |
| Mesh Terrain (5.8) | not guaranteed | yes | yes | yes | yes | needs FarField transformer | yes | yes | experimental |

---------------------------------------------------------------------------------------------------------------------------

## 2. Baking in the editor: assets with Nanite and distance fields, World Partition, HLODs, deterministic rebuild

### 2.1 Creating static mesh assets with Nanite from code

- Geometry Script (editor module `GeometryScriptingEditor`): `CreateNewStaticMeshAssetFromMesh` / `...FromMeshLODs` with
  `FGeometryScriptCreateNewStaticMeshAssetOptions { bEnableRecomputeNormals, bEnableNanite, FMeshNaniteSettings NaniteSettings, bEnableCollision, bUseOriginalVertexOrder }`
  (local: `Engine/Plugins/Runtime/GeometryScripting/Source/GeometryScriptingEditor/Public/GeometryScript/CreateNewAssetUtilityFunctions.h`
  lines 48-190). Python: `unreal.GeometryScriptCreateNewStaticMeshAssetOptions` with `enable_nanite`, `nanite_settings`,
  `use_original_vertex_order` ("Unreal Python 5.8 (Experimental)")
  https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/GeometryScriptCreateNewStaticMeshAssetOptions
- Or C++ in an editor module/commandlet: create a `UStaticMesh` in a real package, add source models from `FMeshDescription`
  (no `bFastBuild`), set `NaniteSettings.bEnabled`, `bGenerateMeshDistanceField` (local StaticMesh.h line 1295; ignored when
  the project setting "Generate Mesh Distance Fields" is on), `Build()`, save. Existing editor subsystem:
  `UStaticMeshEditorSubsystem::SetNaniteSettings(StaticMesh, FMeshNaniteSettings, bApplyChanges)`
  https://dev.epicgames.com/documentation/unreal-engine/API/Editor/StaticMeshEditor/UStaticMeshEditorSubsystem/SetNaniteSettings?application_version=5.1
- **Keeping the sim's triangles under Nanite**:
  - Nanite "quantizes mesh vertex positions"; the step is a power of two set by Position Precision, Auto chooses from mesh
    size (https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-technical-details); Python docs: step size is
    2^-PositionPrecision cm (https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MeshNaniteSettings?application_version=5.1).
    Set it explicitly (e.g. 0 = 1 cm, or 1 = 0.5 cm) so the error is bounded and known; LOS works in metres.
  - Keep Triangle Percent = 1.0 (no reduction) (same Python page).
  - Nanite picks detail per cluster from the view: "swapped on the fly at varying levels of detail based on the camera
    view", "Loss of quality is rare or non-existent" (Nanite overview page). So the full-detail mesh is the sim's mesh and
    distance simplification is sub-pixel by design; define "equal" as LOD0 identical plus screen-space error.
  - Ray tracing uses the **fallback mesh** ("The Fallback Mesh is used for Nanite-enabled meshes by default", lower
    "Fallback Relative Error" to keep more source triangles) (Nanite overview page). For ground, set Fallback Relative Error
    near 0 so Lumen HWRT occlusion matches the drawn ground.
- Lumen cards: 12 per static mesh by default ("Max Lumen Mesh Cards") (Lumen Technical Details). Software Lumen and large
  single meshes: "Large single meshes ... get poor fields that can cause self-occlusion" (same page): keep ground tiles
  modest (e.g. 1-2 km) rather than one 40 km mesh.

### 2.2 Running it headless

- Python: `UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<file>`: "very fast to execute, and can even run your
  scripts in headless mode"; "This commandlet does not automatically load levels"; or `-ExecutePythonScript` (full editor).
  "In both approaches, the Editor shuts down immediately after running your Python script."
  https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python . Community reports
  of actor spawning failing under `-run=pythonscript` (search results; not verified): spawn levels/actors with the full
  editor path or a C++ commandlet.
- C++ commandlet (`UCommandlet` subclass in an editor module) is the robust route: links simcore directly (header-only),
  writes assets with the same code the runtime uses, no Python round trip. (General UE practice; the World Partition
  builders are themselves commandlets, below.)

### 2.3 World Partition, HLODs and their commandlets

- HLODs from the command line: `-run=WorldPartitionBuilderCommandlet -AllowCommandletRendering -Builder=WorldPartitionHLODsBuilder`,
  with `-SetupHLODs`, `-BuildHLODs`, `-DeleteHLODs`; "HLOD Actors will not be recreated or built if no changes have been
  made to their source Actors"; layer types Instancing ("ideal for imposter meshes, such as trees and foliage"), Merged,
  Simplified (with a Nanite option); per-layer runtime Cell Size and Loading Range.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine
- Builders: HLODs, MiniMap, ResaveActors (`-ActorClass=`), Foliage (`-NewGridSize`, default instanced foliage grid 256 m),
  Navigation, PCG (`-Builder=PCGWorldPartitionBuilder`, `-IterativeCellLoading`, `-IterativeCellSize` default 25600,
  `-AutoSubmit`), `-SCCProvider=None` to run without source control.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-builder-commandlet-reference
- 5.8: "perceptual difference heuristic option to update HLODs only when needed", "Build HLOD for Selected Region",
  standalone HLODs for repeated sub-levels; PCG Builder Volume to bound the PCG builder commandlet (5.8 release notes,
  above). 5.8.3 fixed "the HLOD commandlet running out of GPU virtual address space" (https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315 , via the 5.8 thread).
- Custom HLOD actors (`WorldPartitionCustomHLODActor`) since 5.7 (Tom Looman, https://tomlooman.com/unreal-engine-5-7-performance-highlights/).
- **Why HLODs matter here**: Lumen's Far Field (HWRT) "requires the use of HLOD1 to be built"; it traces from the Max
  Trace Distance (default 200 m) to `r.LumenScene.FarField.MaxTraceDistance` (default 1 km) (Lumen Technical Details).
  Without World Partition HLOD1, Lumen GI/reflections stop at about 200 m (SWRT scene: "only covers 200 meters", up to
  800 m with Lumen Scene View Distance; beyond, only screen traces). An always-loaded mesh cannot simply be forced into the
  far field (open Epic ticket, Nov 2025): https://forums.unrealengine.com/t/include-huge-always-loaded-distant-meshes-in-the-far-field-for-lumen-reflections/2687349
- FastGeo Streaming (Experimental since 5.6; `Engine/Plugins/Experimental/FastGeoStreaming`, descriptor: "extracts and
  converts a partitioned world's geometry to optimize world streaming performance", local) converts World Partition cells'
  static geometry to actor-less primitives (Tom Looman: "FastGeo is Epic's new 5.6+ static geometry streaming solution");
  only relevant if SAM3D moves to World Partition. API: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/FastGeoStreaming

### 2.4 Deterministic rebuild when the data changes

- Engine help: `bUseHashAsGuid` (above) and the World Partition HLOD skip-if-unchanged rule (above); 5.8 adds a perceptual
  heuristic. The static mesh build's derived data is cached; identical inputs give identical keys (implied by the
  `bUseHashAsGuid` comment).
- Recommended manifest (design, not a source): key each baked asset by a hash of (the map JSON, the simcore build's map
  code version, the bake tool version, the Nanite settings); store it in a manifest next to the assets; the runtime map
  actor uses the baked asset only when the manifest hash equals the hash of the data it loaded, else falls back to the
  runtime path. That keeps "routes and maps derived from data at load" true (CLAUDE.md track G) while letting Nanite and
  distance fields exist.

### 2.5 How others build data-generated worlds in UE

- **City Sample (Epic)**: city generated in Houdini, imported with a Rule Processor ("rules that tell Unreal Engine how to
  use that data to populate the world") into a World Partition world; 5.8 update adds PCG-built levels.
  https://dev.epicgames.com/documentation/unreal-engine/city-sample-quick-start-for-generating-a-city-and-freeway-in-unreal-engine-5 ;
  https://www.unrealengine.com/learning/city-sample-gets-a-major-update-with-pcg-and-unreal-mcp-workflows
- **Cesium for Unreal**: streams tiles at runtime as one StaticMeshComponent per tile; no Nanite because runtime Nanite
  builds are not possible (Cesium forum above; tiles are static meshes per Cesium community answers in search results).
- **PCG (production-ready since 5.7)** with a builder commandlet to bake PCG output into World Partition (builder reference
  above; status: https://digitalproduction.com/2025/11/12/unreal-engine-5-7-foliage-pcg-and-in-editor-ai/ ,
  https://www.tweaktown.com/news/107858/unreal-engine-5-7-preview-now-out-with-production-ready-procedural-content-generation-framework/index.html).

---------------------------------------------------------------------------------------------------------------------------

## 3. Foliage at scale (240 000 trees, woods' masses)

### 3.1 ISM or HISM

- Epic docs: "If your project only uses Nanite then ISM is always the choice since Nanite has its own culling and LOD
  system"; "HISMs can be better if you have 1000s of instances that don't move"; "ISMs does not have the static hierarchy
  and so has to cull and LOD each instance on the GPU"; projects created in 5.4+ can use per-instance LODs on ISM.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine
- Community (2024): "Using ISMs, if you are always enforcing nanite, is better than HISM" https://forums.unrealengine.com/t/sm-ism-hism-lightweight-instances-and-auto-instancing-what-are-the-differences/609954/7
- ISM properties in 5.8 (local `Engine/Source/Runtime/Engine/Classes/Components/InstancedStaticMeshComponent.h`):
  `NumCustomDataFloats`, `PerInstanceSMCustomData` ("Can be read in a material"), `SetCustomDataValue`,
  `InstanceStartCullDistance`/`InstanceEndCullDistance` ("Distance from camera at which each instance begins to fade
  out"/"completely fades out"), `InstanceLODDistanceScale`, `bUseGpuLodSelection`.
- Nanite instance cap: the scene is "hard-locked to 16 million instances, which includes all instances that are streamed
  in" (Nanite overview page). 240 000 trees + 12 000 buildings are far below.

### 3.2 Nanite for trees and the 5.7-5.8 Nanite Foliage

- Classic Nanite foliage: enable **Preserve Area** on foliage meshes ("should be enabled on all foliage meshes and nothing
  else") (Nanite technical details). Nanite with WPO: clamp displacement; "Foliage using WPO is less problematic because
  the foliage is filled with holes" (Nanite overview).
- **Nanite Foliage (Experimental, 5.7+, still Experimental in the 5.8 docs)**: Assemblies (up to 65k instanced parts),
  Skinning (bone-driven wind through the Dynamic Wind plugin instead of WPO), Voxels (Shape Preservation = Voxelize; the
  switch happens when voxels give lower error). "Learn to use this Experimental feature, but use caution when shipping";
  "Virtual Shadow Map (VSM) performance requires you to disable animation in the distance"; Dynamic Wind "Only has support
  for global wind direction", "No collision with players or objects"; ~100 000 bones updated in ~0.1 ms GPU.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-foliage ; Dynamic Wind descriptor: "Extremely
  experimental dynamic wind support for Nanite foliage" (local `Engine/Plugins/Experimental/DynamicWind/DynamicWind.uplugin`).
  Epic forum (Chris.Murphy): wind "much cheaper than WPO as it's done per bone instead of per vert"; voxelization "is how
  we see the path forward for most platforms" but "might have more overhead than if it was just a nanite mesh"
  https://forums.unrealengine.com/t/foliage-in-current-version-and-forward-looking/2685424 . Ray-traced shadows of
  voxelised Nanite foliage use the fallback mesh (forum report https://forums.unrealengine.com/t/bad-raytraced-shadows-with-voxelised-nanite-foliage-in-5-7/2696233).
  5.8.2 fixed "Nanite Voxel Foliage didn't render on some platforms" (5.8 release thread).
- **Procedural Vegetation Editor** (Experimental, 5.7+; descriptor `IsExperimentalVersion: true`, local) makes Nanite
  Foliage-ready trees from species presets; can bake to static or skeletal mesh (forum above). Usable later to author
  oak, poplar, pine and black locust in-engine (output is Epic-content/our own; check licence of the presets).

### 3.3 Impostors

- Impostor Baker ships with 5.8 at `Engine/Plugins/Experimental/ImpostorBaker` (local; descriptor "Generates a variety of
  Impostors for use as distant mesh LODs", category Mesh, depends on GeometryScripting in Editor). Its material functions
  include `UnitVectorToHemiOctahedra` and `HemiOctahedronToUnitVector` (local Content/MaterialFunctions), i.e.
  (hemi-)octahedral frame layouts.
- Docs: Full Sphere, Upper Hemisphere and Billboard types; FramesXY 16 gives 256 frames; "Epic uses 12 XY frames (144
  total) for distant foliage in Fortnite Battle Royale" at 2048 texture resolution; most impostors need only BaseColor
  and Normal; 5.5+ batch renderer and GBuffer capture; Fortnite uses Upper Hemisphere impostors for trees "when Nanite is
  disabled"; "replace the last LOD, add a new LOD". https://dev.epicgames.com/documentation/en-us/unreal-engine/impostor-baker-plugin-in-unreal-engine
- With Nanite trees, impostors are a fallback for non-Nanite paths (scene captures without Nanite, low scalability).

### 3.4 Cull distances, shadows, wind, Lumen and ray tracing with 240 000 trees

- VSM: WPO and skeletal animation "always invalidates cached pages every frame"; "Shadow Cache Invalidation Behavior"
  Auto/Always/Rigid/Static; "Non-Nanite meshes need LODs, or they become extremely expensive to render into small pages";
  non-Nanite "Instances are culled separately for each virtual shadow map mip/clipmap level"; use `r.Shadow.Virtual.Stats`
  and keep invalidated static pages near zero; broadest clipmap covers about 40 km; Nanite "always renders to the Virtual
  Shadow Map regardless of distance". https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine
- `WorldPositionOffsetDisableDistance` on static mesh components (local `Components/StaticMeshComponent.h` line 158) and
  `ShadowCacheInvalidationBehavior` on primitives (local `PrimitiveComponent.h` line 372: "Control shadow invalidation
  behavior, in particular with respect to Virtual Shadow Maps and material effects like World Position Offset").
- Lumen: "Foliage and Instanced Static Mesh Components can only be supported if the mesh is using Nanite" (surface cache);
  SWRT foliage needs "Affect Distance Field Lighting" (Lumen Technical Details). HWRT: "fewer than 100,000 instances" after
  culling for consoles; "Disable Visible In Ray Tracing" per actor; `r.Lumen.Reflections.MaxRoughnessToTraceForFoliage 0`
  for a large saving; `stat SceneRendering` shows ray tracing active instances.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine
- Lumen direction: "SWRT detail traces (mesh SDF tracing) is now a deprecated path"; "Lumen Continues to move toward a
  single rendering path with HWRT" (Tom Looman on 5.7). 5.8: Lumen Lite (Beta) "about twice as fast" (5.8 release notes).

### 3.5 Per-instance custom data for autumn colour

- `UInstancedStaticMeshComponent::SetNumCustomDataFloats`, `SetCustomDataValue(InstanceIndex, CustomDataIndex, Value)`
  (local header, lines 300-327); material side `PerInstanceCustomData` (one float, `DataIndex`, default) and
  `PerInstanceCustomData3Vector` (three floats, FLinearColor default)
  https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UMaterialExpressionPerInstanceCu-_1 ;
  the ISM docs: custom data reduces draw calls by "passing data without generating a new" material instance per mesh.
- Use: simcore computes, per tree, species, a turn-of-colour phase from the date (15 Oct 1985) and a random seed; the
  material maps (phase, species) to a palette. Deterministic and testable in simcore.

### 3.6 Recommended ranges (derived; the TV sight changes them)

Projected height of a 15 m tree (computed: px = width_px x (h/d) / (2 tan(hfov/2))):

| View | 300 m | 1 km | 3 km | 8 km | 15 km | 30 km |
|---|---|---|---|---|---|---|
| Main view, 90 deg, 1920 px | 48 | 14 | 4.8 | 1.8 | 1.0 | 0.5 |
| TV wide 4.8 deg, 768 px (assumed) | 458 | 137 | 46 | 17 | 9 | 4.6 |
| TV narrow 1.44 deg, 768 px (assumed) | 1528 | 458 | 153 | 57 | 31 | 15 |

- In the main view, single trees are under 5 px beyond ~3 km: full Nanite trees to ~3 km, wood masses (already in
  simcore) beyond. In the TV's narrow field a tree at 8 km is ~57 px tall, so a fixed 8 km cull removes visible trees.
- Cull by screen size or scale distance by field of view: the 5.8 engine has `r.ViewDistanceScale.FieldOfViewMinAngle`,
  `...MinAngleScale`, `...MaxAngle`, `...MaxAngleScale`, `...FieldOfViewAffectsHLOD` (local: strings in
  `UnrealEditor-Engine.dll`), implemented by `FCachedSystemScalabilityCVars::CalculateFieldOfViewDistanceScale`, a lerp of
  the scale between the two angles (local `Engine/Source/Runtime/Engine/Public/UnrealEngine.h` lines 414-438). Scene
  captures have `LODDistanceFactor` and `MaxViewDistanceOverride` (local `Components/SceneCaptureComponent.h` lines
  150-156). Whether the FOV scale applies to a scene capture's view must be tested.
- Proposed bands (to tune by measurement): full Nanite tree with wind to ~150-300 m (WPO disable distance), full Nanite
  tree without animation to ~3 km in the main view (further in the TV, by FOV scale), wood masses or impostor/voxel
  representation beyond; hedgerow and single trees outside woods kept to the cull limit since they are landmarks.

---------------------------------------------------------------------------------------------------------------------------

## 4. Ground clutter (grass, stubble) on a non-Landscape surface

- **Landscape Grass**: "Currently, the Grass system will only work with the Landscape Terrain Actor"; on any other actor
  it "will not work". https://dev.epicgames.com/documentation/en-us/unreal-engine/grass-quick-start-in-unreal-engine
- **PCG GPU** (Beta in the 5.8 docs: "use caution when shipping"): GPU nodes are few (Copy Points, Data Count, Transform
  Points, Static Mesh Spawner, Custom HLSL); the GPU Static Mesh Spawner "sets up mesh instances entirely on the GPU";
  instances "exist only at runtime in GPU memory" and do not support collisions/physics, navigation, ray tracing or
  Distance Field lighting; "The GPU implementation is Experimental"; landscape height and grass-map sampling paths are
  documented (landscape-only).
  https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine
- 5.8: "PCG GPU runtime scatter is now within the same performance range as the landscape GPU grass system";
  "Actor/component-less runtime generation" (5.8 release notes). 5.7: PCG GPU with FastGeo components and componentless
  primitives (`pcg.RuntimeGeneration.ISM.ComponentlessPrimitives 1`, PCG FastGeo Interop plugin, Experimental) (Tom
  Looman; local descriptor `Engine/Plugins/Experimental/PCGInterops/PCGFastGeoInterop/PCGFastGeoInterop.uplugin`).
- Runtime generation: cells around generation sources, `pcg.FrameTime` budget, `pcg.RuntimeGeneration.EnableDebugOverlay 1`,
  partition-actor pool doubling is expensive.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-runtime-generation-debugging-in-unreal-engine
- On non-Landscape ground PCG needs a surface: the usual pattern is World Ray Hit Query into the Surface Sampler (needs
  collision), and on runtime-created geometry it is reported broken in 5.7.3 ("Generated 0 points in 122,500 cells", no
  answer) https://forums.unrealengine.com/t/pcg-surface-sampler-generates-0-points-on-runtime-created-geometry-5-7-3/2754135 .
  An untested alternative: a Custom HLSL point generator sampling a height texture and a land-cover texture written from
  simcore (GPU Texture Sampler exists; "reading from CPU memory is faster and more accurate than reading from GPU memory",
  Python docs for `PCGTextureSamplerSettings` via search). Whether a transient runtime texture can feed PCG is unverified.
- **Hand-written camera-centred ring**: ISM chunks (e.g. 32 m cells) in a ring around the camera, positions from simcore's
  land-cover raster and exact ground triangles (so blades sit on the drawn ground), refilled as the camera moves; no
  shadows (VSM doc: Contact Shadows "is a sufficient substitute" for grass), no ray tracing, WPO only near. Same structure
  as Landscape grass (HISM per cell, cull distances: `FGrassVariety` start/end cull distance, WPO disable distance,
  https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Landscape/FGrassVariety?lang=en-US ). Costs: game
  thread time to fill cells (time-slice it), GPU vertex cost of dense instances; deterministic and unit-testable in simcore.
- Note: 5.8 time-sliced landscape grass was reported to fill distant cells first (forum bug)
  https://forums.unrealengine.com/t/ue-5-8-time-sliced-landscape-grass-generation-does-not-prioritize-the-active-camera-and-starts-populating-distant-landscape-cells-first/2746085

---------------------------------------------------------------------------------------------------------------------------

## 5. Far terrain realism: aerial perspective, fog, sky atmosphere, far colour maps

- **Aerial perspective** is a froxel LUT: "Stores the transmittance and scattered luminance into froxel (camera frustum
  voxel)" applied to opaque and translucent; `r.SkyAtmosphere.AerialPerspectiveLUT.Width`, `.DepthResolution`; cinematic:
  `r.SkyAtmosphere.FastSkyLUT 0`, `r.SkyAtmosphere.AerialPerspectiveLUT.FastApplyOnOpaque 0`; Mie Scattering Scale for
  haze, Mie Anisotropy (Earth ~0.8), Mie Exponential Distribution (km where Mie falls to 40%); Height Fog Contribution;
  "Support Sky Atmosphere Affecting Height Fog" project setting.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine
- **LUT depth**: `r.SkyAtmosphere.AerialPerspectiveLUT.Depth` "The length of the LUT in kilometers (default is 96
  kilometers ...)"; beyond it the last slice is used (zh-CN docs per search; English properties page truncates);
  `r.SkyAtmosphere.AerialPerspective.StartDepth` default 100 m.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-properties-in-unreal-engine .
  Slice counts by scalability: 8, 16, 16, 16, 32 (local `Engine/Config/BaseScalability.ini` lines 814-939). For haze
  growing out to 150 km raise the depth to ~150-160 km and the depth resolution with it, or accept constant haze past
  96 km. Community advice for near precision: shrink the depth (e.g. 5 km)
  https://forums.unrealengine.com/t/new-sky-atmosphere-model-in-4-24/133086?page=3 : a trade-off, test both.
- **Calibrating haze to visibility** (Koschmieder): visual range = 3.912 / extinction; clean Rayleigh air at sea level
  ~296 km; haze 2-5 km, mist 1-2 km, fog < 1 km. https://en.wikipedia.org/wiki/Visibility . So for a weather preset's
  visibility V (km), ground-level Mie extinction ~ 3.912/V minus Rayleigh (~0.0132 /km at 520 nm); set Mie scattering
  plus absorption to that at 0 km with Mie Exponential Distribution ~1-1.5 km for a boundary layer. An autumn Alfold day
  with 20 km visibility needs ~0.18 /km; units are km^-1 in the component (properties page: coefficients "at an altitude
  of 0 kilometers").
- **Exponential Height Fog**: Fog Density, Height Falloff, second fog layer, Start Distance, Cutoff Distance, Max Opacity,
  Directional Inscattering; volumetric fog ignores Start Distance, Max Opacity, Cutoff; a far Start Distance cuts cost
  ("can be 50% or less"). https://dev.epicgames.com/documentation/en-us/unreal-engine/exponential-height-fog-in-unreal-engine .
  5.8 adds Fog Screen Space Scattering (Experimental) (5.8 release notes; cvar `r.Fog.ScreenSpaceScattering` in the 5.8
  renderer binary, local). Use the second layer for morning river mist (low falloff height), the atmosphere for haze.
- **Far colour maps**: the far ground at 500 m spacing coloured per vertex gives 500 m colour cells; a land-cover colour
  texture sampled by world position (mip-mapped, so distance averages correctly) gives parcel-scale colour at any
  distance. Runtime textures: create transient, update regions (forum and wiki caveats on resource timing and pitch:
  https://forums.unrealengine.com/t/community-tutorial-creating-a-runtime-editable-texture-in-c/1729786 ,
  https://unrealcommunity.wiki/dynamic-textures-a5iczbuy ) or bake it as an asset in the commandlet.
- **Runtime Virtual Texturing**: "Any component placed in the scene can render to an RVT"; good for blending roads and
  for letting grass and trees read the ground colour; ISM instances are not culled in RVT rendering; low mips can be
  baked and streamed. https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine
- Far Cry 5's GPU terrain pipeline (LOD, culling, stitching, all distances) is the reference talk; slides are behind GDC
  Vault login, not read. https://gdcvault.com/play/1025480/Terrain-Rendering-in-Far-Cry
- Shadows far out: VSM's broadest clipmap is ~40 km; beyond, terrain self-shadowing from 150 km hills is negligible on a
  plain (VSM page).

---------------------------------------------------------------------------------------------------------------------------

## 6. Measuring performance from the command line

- **Stats**: `stat unit`, `stat gpu`, `ProfileGPU`, `stat SceneRendering` (Lumen performance guide above). 5.8: stat unit
  shows FrameNum and UTC time with `stats.UnitTimestamp`; `snapshothitches -start` writes a trace snapshot and screenshot
  to `Saved/Profiling/Hitches` when a hitch is detected (needs a stat group like `stat default`) (5.8 release notes, offset
  300000). Nanite: `NaniteStats`, `r.Nanite.ShowStats`; VSM: `r.Shadow.Virtual.Stats`; Lumen: `r.LumenScene.DumpStats`
  (all present in the 5.8 `UnrealEditor-Renderer.dll`, local).
- **CSV profiler**: `csvprofile start|stop|frames=N`, `-csvCaptureFrames=N`, `-csvGpuStats`, `-csvcategories=`; output in
  `Saved/Profiling/CSV`; graph with CSVToSVG (`-showevents`, `-stats Category/*`).
  https://dev.epicgames.com/documentation/unreal-engine/csv-profiler?application_version=4.27 . Tools shipped with 5.8:
  `PerfreportTool.exe`, `CSVToSVG.exe`, `CSVCollate.exe`, `csvinfo.exe`, `RegressionsReport.exe` in
  `D:/UE_5.8/Engine/Binaries/DotNET/CsvTools/` (local).
- **Unreal Insights**: `-trace=cpu,gpu,frame,bookmark,counters`, `-tracefile`, `-tracehost`, `-statnamedevents`;
  `Trace.File`, `Trace.Start`, `Trace.Stop`; output to `Saved/Profiling`.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-reference-in-unreal-engine-5 . 5.8 adds
  TraceQuery, "a standalone UBT CLI that reads .utrace files and outputs JSONL" (frames, counters, CPU scopes) (5.8
  release notes, offset 300000), the bridge from traces to automated checks. `UnrealInsights.exe` and
  `UnrealTraceServer.exe` are in `Engine/Binaries/Win64` (local).
- **Launch flags**: `-BENCHMARK` "Run game at fixed-step in order to process each frame without skipping any frames",
  `-FPS=` "Set the frames per second for benchmarking", `-UNATTENDED`, `-NOSOUND`, `-WINDOWED`, `-ResX/-ResY`
  https://dev.epicgames.com/documentation/unreal-engine/command-line-arguments-in-unreal-engine?application_version=5.0 ;
  `-ExecCmds="..."` runs console commands at start (community guide https://ikrima.dev/ue4guide/wip/useful-command-line-arguments/).
- **Automated Performance Testing plugin** (Experimental, `Engine/Plugins/Performance/AutomatedPerfTesting`, "Gauntlet
  Test Controllers to facilitate automatic performance testing", local): static camera tests
  (`UAutomatedStaticCameraPerfTestBase`, `UAutomatedPlacedStaticCameraPerfTest`: per camera set up, mark start/end,
  screenshot, evaluate), sequence, replay, material and ProfileGo tests; the controller drives the CSV profiler.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/AutomatedPerfTesting/UAutomatedStaticCameraPerfTestBa-
- For SAM3D: a fixed set of placed cameras (battery position in the 10 m area, map proper, 3 km altitude, the TV's narrow
  field at 10 km), run `-game -BENCHMARK -FPS=60 -csvCaptureFrames=600 -csvGpuStats -trace=cpu,gpu,frame` from a script
  like `tools/ue_ground_check.py`, reduce with PerfreportTool or TraceQuery JSONL, and fail on budgets (game thread, GPU
  ms, draw calls, RT instances, invalidated VSM pages).

---------------------------------------------------------------------------------------------------------------------------

## 7. Recommendations and order of work

1. **Measure first.** Build the camera-shot harness (section 6) and record the current PMC/HISM baseline, including the
   TV view. Without it no later step can be judged.
2. **Split "simulation geometry" from "presentation meshes".** Only the ground (and anything line of sight reads) must be
   the sim's triangles. Tree and building shapes are presentation (LOS uses obstacle heights), so they can be baked or
   third-party Nanite assets placed at simcore's positions.
3. **Ground, step A (runtime, cheap)**: replace PMC tiles with runtime-built `UStaticMesh` tiles (the existing
   `FSimRuntimeMesh`), Static mobility, with LOD chains from simcore's coarser grids for far tiles; or a
   DynamicMeshComponent with `StaticDraw` to get bounding-box Lumen cards. A/B both against the harness (static draw, VSM
   caching, Lumen bounce). Keep `tools/ue_ground_check.py` as the equality test.
4. **Ground, step B (bake)**: an editor C++ commandlet (links simcore) writes Nanite ground tiles (~1-2 km, Keep Triangle
   Percent 1.0, explicit Position Precision, low Fallback Relative Error), with distance fields and Lumen cards, plus a
   manifest of input hashes; the map actor uses baked tiles when the hash matches and falls back to step A otherwise. Add
   a test that reads back the baked LOD0/fallback and compares to simcore's triangles.
5. **Trees and buildings**: Nanite static meshes (baked from simcore's `TreeMesh`/building generators, or registered
   CC0/Fab assets), Preserve Area on foliage, ISM per kind per chunk, per-instance custom data for species colour and
   autumn phase, WPO only near (disable distance), Shadow Cache Invalidation Rigid beyond, ray tracing visibility limited
   by distance, FOV-scaled culling so the TV sees far trees; wood masses beyond ~3 km. Impostor Baker only as the non-Nanite
   fallback.
6. **Grass and stubble**: a camera-centred ISM ring from simcore's land cover and exact ground (no shadows, no RT, contact
   shadows); a spike on PCG GPU with Custom HLSL and simcore textures later.
7. **Far terrain**: land-cover colour texture for the far ground; Sky Atmosphere Mie per weather preset from Koschmieder;
   AP LUT depth and resolution tuned for 150 km; height fog second layer for mist; check in the TV's narrow field.
8. **Later, when out of Experimental**: Nanite Foliage (assemblies, voxels, dynamic wind), Procedural Vegetation Editor
   species, Mesh Terrain; World Partition plus HLOD1 only if Lumen Far Field (GI/reflections beyond 200 m) proves worth it.

---------------------------------------------------------------------------------------------------------------------------

## 8. Gaps (not established)

- Whether PMC proxies supply any Lumen card representation in 5.8 (the .cpp is not in the binary install).
- Whether runtime fast-built static meshes get Lumen cards (inferred no: the generator is in a Developer module).
- Whether VSM static caching applies to a DynamicMeshComponent on the static draw path.
- Whether `r.ViewDistanceScale.FieldOfView*` affects scene-capture views (the TV sight).
- Whether PCG can take a transient runtime texture as Texture Data, and the open 5.7.3 runtime Surface Sampler issue.
- Exact default for `r.SkyAtmosphere.AerialPerspectiveLUT.DepthResolution` beyond the scalability values, and the
  zh-CN-only wording for "last slice".
- The Fast Geometry Streaming guide's body could not be read (https://dev.epicgames.com/community/learning/knowledge-base/zqzP/unreal-engine-fast-geometry-streaming-guide).
- RMC Pro's runtime Nanite and SDF claims (vendor; paid, so excluded anyway).
- Far Cry 5 terrain talk content (GDC Vault login).
# Tech gaps filled: the ground mask, mask sampling in 5.8, and building the arrays by commandlet

Research pass of 3 Nov 2026 for the terrain roadmap. It closes three gaps that `tech_material.md` section 8 left
open:

- **Gap 1:** what the runtime mask holds per 25 m cell, and how a cell's content becomes a ground layer.
- **Gap 2:** how a mask is sampled by world position in UE 5.8, and how precise that is.
- **Gap 3:** whether a `-nullrhi` pythonscript commandlet can build the Texture2DArrays and the master material.

How claims are marked:

- **[Repo]** is a file in this repository, given with line numbers.
- **[Engine 5.8]** is a file in the local install `D:/UE_5.8` (5.8.3-58210709).
- **[Probe]** is a measurement made this session in a throwaway content-only project (5.8.3) under the scratchpad.
  - Project: `C:\Users\Nick\AppData\Local\Temp\claude\C--dev-SAM3D\ce1ed19e-5ce2-477c-808d-6c3e688a22c5\scratchpad\ue_probe\ProbeProj\ProbeProj.uproject`.
  - Scripts are in `...\scratchpad\ue_probe\scripts\`. Logs are `...\scratchpad\ue_probe\probe1.log` to `probe8.log`.
  - Nothing was written into the SAM3D project or its Content.
- **URLs** are web pages read this session or in the earlier passes, as cited there.
- My own calculations and design choices are marked **(estimate)** or **(design)**.

No asset files were downloaded. Only pages and API JSON were read. No new asset picks are made here; the layer picks
are the ones in `pbr_textures.md` and `fill_textures.md`.

---

## 0. Summary of answers

| Question | Answer | Evidence |
|---|---|---|
| Are the PMC's UVs 16-bit on the GPU in 5.8? | **Yes, measured.** UV0 = 40.0123 reached the pixel shader as exactly 40.0. UV1 = 0.5123 reached it as 0.51220703, the half-float neighbour. | [Probe] probe7.log, probe8.log (section 2.1) |
| What do Frac and Fmod on an LWC world position emit in 5.8? | `LWCFrac(LWCDivide(WP, d))` and `LWCFmod(WP, d)`. A DoubleVectorParameter subtraction emits `LWCSubtract` followed by `LWCToFloat`. | [Probe] the generated `BasePassPixelShader.usf` (section 2.2) |
| How precise is world-position wrapping 140 km out? | It agrees with a double-precision computation to about 1 cm. Neighbouring pixels step consistently, to within 0.15 cm. | [Probe] probe7.log (section 2.3) |
| Is HighPrecisionWorldPosTextureSampling needed? | **Not for the mask.** For tiling detail textures, **use its form** (camera-relative position / T + frac(camera origin / T)). It is smooth across the screen, so mips and derivatives work. The Frac node wraps per pixel. | [Probe] HLSL dump; section 2.4 |
| RVT: "any component can write" against "PMC cannot"? | **Both are true at different levels.** Every primitive has the property. Only proxies that set `bSupportsRuntimeVirtualTexture` write. The 5.8 PMC proxy has only a dynamic draw path. | [Engine 5.8] headers and PDB symbols (section 2.5) |
| `r.VirtualTextures` default in 5.8 | **1 (on).** Seen in a blank project with no config, and in SAM3D's own editor. | [Probe] probe1.log (section 2.6) |
| Does `Texture2DArrayFactory` work with an empty `InitialTextures`? | **Yes.** `create_asset` returns a 0-slice array. | [Probe] probe1/2.log (section 3.1) |
| Does setting `source_textures` rebuild under `-nullrhi`? | **Yes**, if the set notifies. Source went from 0 to 524288 B. Built size became 256x256x2. Settings survive save and reload. | [Probe] probe2/3.log |
| Can `-nullrhi` validate the master material? | **No.** Materials are created and saved, but nothing is compiled. A broken graph returns no errors and zero statistics. **`-AllowCommandletRendering` compiles for real** and returns the errors. | [Probe] probe3/4.log (section 3.3) |
| ambientCG normal convention | **Not stated on any page read.** The site's own previews are `_NormalDX`. Decide per file at import with a test against the height map. | section 3.5 |
| Scan sizes of Ground077, Ground103, Ground104, Plaster007 | **Not published.** The API returns 0 for all four. Visual estimates are 1-3 m; start at 2 m and calibrate. | section 3.6 |

---

## 1. Gap 1: the ground mask (one texel per 25 m cell) and the layer table

### 1.1 What simcore has, and what the mask must reproduce

- **Cover classes:** Field, Forest, Marsh, Town, Military, Road, Water. Each `CoverGrid` cell holds one `Cover` byte
  and one crop byte. [Repo] `simcore/include/simcore/scenery/map_dressing.hpp:36,284-303`.
- **Crops:** wheat 0.3, maize 0.25, sunflower 0.1, ploughed 0.15, pasture 0.2. [Repo] `data/scenery/map_dressing.json`.
  - The crop of a cell is `CropOf(FieldParcel(rules, seed, cell centre))`, where
    `seed = Mix64(def.seed ^ 0xF1E1D5)`. [Repo] `map_dressing.hpp:271-283,400-424`.
- **Grid geometry:**
  - The cell is 25 m (`cover_cell_m`). [Repo] `map_dressing.hpp:74`.
  - There is one grid for the map proper and one per detail area. The detail grids are added first, so
    `CoverRaster::GridAt` prefers them. [Repo] `map_dressing.hpp:309-323,419-422`.
  - **On the Kesmark map every grid lies on one 25 m lattice anchored at the map origin:**
    - The map proper is -20..20 km, so 1600x1600 cells.
    - bty21 is centred at (9.5, -6.5) km with half 6 km. Its west edge is 3500 m = 140 cells, its south edge
      -12500 m = -500 cells, and it is 480x480 cells.
    - The capital is centred at (36, 58) km with half 7.5 km. Its west edge is 28500 m = 1140 cells, its south edge
      50500 m = 2020 cells, and it is 600x600 cells. **It lies outside the map proper.**
    - Source: [Repo] `data/maps/kesmark_range.json` (`extent_km`, `terrain.detail`).
  - BakeCover stamps every grid from the same boxes and strips by cell centre. So bty21's cells equal the map
    proper's cells where they overlap (estimate from reading `BakeCover`, [Repo] `map_dressing.hpp:400-447`).
  - **So two masks are enough:** the map proper and the capital. bty21 needs its own mask only if it later gets a finer
    cell (section 1.6).
- **Stamp order** (last wins):
  - Fields with crops.
  - The levelled sites (`map.Flats()`), stamped as Military discs.
  - Then four passes over the boxes:
    - Forest: `Canopy` boxes.
    - Marsh.
    - Town: buildings `House`, `Flats`, `Farm` and `Industrial`, each grown by `garden_m` = 15 m.
    - Military: `Military`, `Hangar`, `Shelter`, `Pad`, `Runway`, `Revetment`, `Bunker`.
  - Then road strips (`MainRoad`, `Road`, `Track`, `Rail`) as Road.
  - Then water strips as Water.
  - Source: [Repo] `map_dressing.hpp:388-447`; `MapSurface` and `MapPart` in
    `simcore/include/simcore/terrain_map.hpp:222-225,272`.
- **What the raster loses** (and the mask baker must recompute from `MapData`, because the 1-byte cover cannot hold
  it):
  - Which part made a Town or Military cell.
  - Which strip made a Road cell.
  - A field's parcel identity and furrow heading.
- **The parcel hash cannot run in the shader.** `FieldParcel` chains `noise::Mix64` (64-bit multiply and xor-shift) on
  64-bit integers. [Repo] `map_dressing.hpp:253-269`, `simcore/include/simcore/noise.hpp:14`.
  - SAM3D targets PCD3D_SM5 as well as SM6. [Repo] `unreal/SAM3D/Config/DefaultEngine.ini:18-22`.
  - In D3D12's HLSL, `uint64_t` appears only as a type "present in the target shader model" from SM 6.0 on.
    https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/hlsl-shader-model-6-0-features-for-direct3d-12
  - **So the heading and a parcel seed are baked into the mask's channels on the CPU (design).**
- **Tree species are white noise per tree.** Each tree's species is a point hash at decimetre resolution
  (`At(seed ^ 0x5F, e, n)`, a lattice hash on `llround(e*10)`). The only exception is that poplar is forced within
  `poplar_river_m` = 60 m of water. [Repo] `map_dressing.hpp:108-110,505-520`.
  - So a wood has no dominant species today, and the cover raster cannot say whose litter lies under a cell
    (section 1.4).

### 1.2 The mask texture

**Format (design, checked against the 5.8 API):**

- Create it with `UTexture2D::CreateTransient(W, H, PF_B8G8R8A8, Name, TConstArrayView64<uint8> ImageData)`. In 5.8
  `CreateTransient` takes the bytes directly and makes **one mip**. [Engine 5.8]
  `Engine/Source/Runtime/Engine/Classes/Engine/Texture2D.h:340-342`.
- **Byte order:** `PF_B8G8R8A8` is B, G, R, A in memory. Build the bytes as a `TArray<FColor>`. On little-endian
  platforms `FColor` is laid out exactly `uint8 B,G,R,A`. [Engine 5.8]
  `Engine/Source/Runtime/Core/Public/Math/Color.h:546-547`.
  - Fill `FColor(R, G, B, A)` by channel name. The shader's `.r .g .b .a` then read R, G, B, A; the format does the
    swizzle.
- **Settings before `UpdateResource()`:**
  - `SRGB = false`.
  - `Filter = TF_Nearest`.
  - `LODGroup = TEXTUREGROUP_Pixels2D`.
  - `AddressX = AddressY = TA_Clamp`.
  - `NeverStream = true`.
  - Hold the texture in a `UPROPERTY`.
  - This is the recipe in `tech_material.md` section 1.3. `UTexture` defaults to sRGB on, and a class mask must turn
    it off (same section).
- **Row order:** texture memory row r = grid row r, with row 0 the **south** row (`CoverGrid::Index(c, r)`; [Repo]
  `map_dressing.hpp:296-302`).
  - The material reads cells with `Tex.Load(int3(c, r, 0))` (section 2.2), so there is no flip.
  - If a sampler is ever used instead, v = (r + 0.5) / rows. That is still no flip, because both sides count rows
    from memory row 0.
- **Size:**
  - Map proper: 1600 x 1600 x 4 B = 10.24 MB.
  - Capital: 600 x 600 x 4 B = 1.44 MB.
  - Both are far under the 16384 limit (`tech_material.md` section 1.6).
- **No mips.** Class indices must not be averaged. Distance falls back to the vertex colour beyond about 1.5-2 km
  (`tech_material.md` section 7.6).
- **Integer formats rejected:** `PF_R8G8B8A8_UINT` would need a `Texture2D<uint4>` declaration. The material
  editor's Texture Object input is a float texture, and Load on a UNORM8 texture already returns k/255 exactly.
  Decode with `(uint)round(t.r * 255)` (design).

**Channels (design):**

| Channel | Content | Encoding | Who reads it |
|---|---|---|---|
| **R** | **Ground layer index** 0-255 (section 1.3) | layer id; 0 = unknown (draw magenta in a debug view) | material: index into the layer table |
| **G** | **Unit seed:** a per-parcel, per-stand or per-building random byte | Fields: `(Mix64(parcel ^ 0x5EED) >> 56)`. Forest: the same from the stand's (Canopy box's) centre. Town: from the nearest building's box. Others: from the cell index. | material: tint and roughness jitter, row phase, stubble density, rut offset |
| **B** | **Direction:** the furrow heading for fields, the road's local direction for road cells, the building's heading for town cells | `round(heading / pi * 256) & 255`, heading in [0, pi). The heading is a bearing from north: `FieldParcel`'s along-row axis is u = e sin h + n cos h ([Repo] `map_dressing.hpp:260`). One step is 0.70 degrees. | material: rotate the layer UVs and their normals |
| **A** | **v1:** the field-state sub-parameter (sowing age in days 0-60 for drilled wheat, 255 = n/a). **v2:** the signed distance to the parcel edge (section 1.7). | v1: days; v2: `128 + d/0.25 m`, clamped | material: seedling-row cover and the headland |

**Rotation between the map and the frame.**

- The mask is in **map axes**. The scenario frame (Unreal world) differs from them by `MapFrameLink`'s exact affine.
  [Repo] `simcore/include/simcore/terrain_map.hpp:1985-2070`.
- A heading taken from the mask must therefore be turned into the frame by the same 2x2.
- About 0.06 degrees of meridian convergence for an origin 6 km from the map's (estimate, from the 2x2).
- Pass the 2x2 to the material (section 2.2) and rotate the direction vector, not the angle.

### 1.3 Cell content to ground layer (the table)

**Where the rule lives (design):**

- Write a header-only function in simcore, `scenery/ground_layers.hpp`:
  `GroundLayerAt(map, rules, raster, date, e, n) -> {layer, seed, heading, aux}`.
- Its rules go in `data/scenery/map_dressing.json` under `"ground_layers"` and `"ground_seasons"`.
- Unreal only copies the bytes into the texture. This keeps simcore the authority on classes, as
  `tech_material.md` 7.9 asked.
- The session's date is the scenario's `"date"`, its sky clock ([Repo] `CLAUDE.md`, `data/scenarios/`). The mask is
  baked at map load. A 24-hour session keeps its states, since a day's harvesting progress is not modelled.

**Layers.** The slice names refer to section 1.5's array order. Picks are from `pbr_textures.md` and
`fill_textures.md`.

| R | Layer | Cell rule | Slice (primary) | Rotated by B | Notes |
|---|---|---|---|---|---|
| 0 | unknown | none matched | none | no | debug magenta |
| 1 | stubble_standing | Field, crop wheat, state "stubble" | 4 withered_grass | yes | plus instanced stalk rows near the camera (`pbr_textures.md`) |
| 2 | stubble_disced | wheat, "disced" | 3 Ground071 | yes | straw pieces worked into soil |
| 3 | ploughed_autumn | any crop, "ploughed" | 0 farm_furrows | yes | deep autumn ploughing, big clods |
| 4 | seedbed_harrowed | any crop, "harrowed" | 1 farm_soil | yes | fine, rolled, darker |
| 5 | wheat_drilled | "sown", age < emergence | 2 raked_dirt | yes | drill lines along B (`fill_textures.md` 4) |
| 6 | wheat_emerged | "sown", age >= emergence | 2 raked_dirt | yes | plus procedural green rows 0.10-0.15 m apart; cover grows with A (age) |
| 7 | maize_standing | maize, "standing" | 1 farm_soil | yes | plants are geometry; far colour from the vertex colour |
| 8 | maize_stover | maize, "harvested" | 5 Ground072 | yes | chopped stalks; stumps as meshes |
| 9 | sunflower_stalks | sunflower, "harvested" | 6 Ground103 | yes | cut stalks as meshes |
| 10 | pasture_autumn | pasture | 7 leafy_grass | no | tint toward brown-green |
| 11 | fallow_bare | summer "ploughed" crop | 8 dry_mud_field_001 | yes | summer only |
| 12 | litter_oak | Forest, stand species oak | 9 forest_leaves_03 | no | |
| 13 | litter_acacia | Forest, black locust | 10 dry_decay_leaves | no | thinner litter, more green (estimate; see 1.4) |
| 14 | litter_pine | Forest, pine | 11 forrest_ground_03 | no | Ground077 as the alternate |
| 15 | litter_poplar | Forest within 60 m of water | 12 sparse_grass + 9 at low weight | no | riparian, grassy floor (estimate) |
| 16 | marsh_mud | Marsh, low-frequency noise < 0.4 | 13 mud_forest | no | wetness parameter from the weather |
| 17 | marsh_grass | Marsh, otherwise | 12 sparse_grass | no | reeds are foliage |
| 18 | town_yard | Town (House), distance to the footprint <= `yard_m` | 14 dirt | by the building | packed earth |
| 19 | town_garden | Town (House), otherwise | 1 farm_soil + 7 leafy_grass by noise | by the building | October: dug beds and grass (estimate) |
| 20 | farmyard | Town (Farm), <= `yard_m` | 18 aerial_mud_1 near-detailed by 14 dirt | no | trampled mud |
| 21 | lawn_estate | Town (Flats) | 16 grass_ground | no | housing-estate lawns |
| 22 | works_yard | Town (Industrial) | 19 concrete_floor_damaged_01 | by the building | |
| 23 | village_street | Road with a Town cell among its 8 neighbours | 15 grass_path_2 | by the road | unpaved verge, packed earth and grass |
| 24 | road_verge | Road (MainRoad, Road) | 16 grass_ground | by the road | the carriageway is the strip mesh |
| 25 | farm_track | Road (Track) | 17 muddy_tracks | by the road | ruts along B |
| 26 | rail_ballast | Road (Rail) | 23 gravel_road | by the road | |
| 27 | military_trampled | Military (site discs, Military, Hangar, Shelter, Revetment, Bunker) | 18 aerial_mud_1 | no | detail layer `dirt` near the camera |
| 28 | military_hardstand | Military (Pad, Runway) | 19 concrete_floor_damaged_01 | by the box | slab joints from the box's frame |
| 29 | water_bed | Water | 20 Ground054 | no | under the water surface; shows at the banks |

**Notes on the table:**

- **Slope layers are not in the mask.** Loess (slice 21 `excavated_soil_wall`) and rock (slice 22 `rock_face_04`)
  are chosen in the shader by slope, with world-aligned mapping on steep faces (`pbr_textures.md`). No cell rule
  needs them.
- **`yard_m` is a game value.** Start at 6 m (estimate). Its distance is from the cell centre to the nearest building
  footprint rectangle, 0 inside it.

**October states per parcel (design). Shares are game values except where marked sourced.**

- Each Field cell's state is drawn from its parcel: `Unit(Mix64(parcel ^ 0x5747E))` against the crop's state
  shares. The draw is per parcel, so a whole field shares one state.
- Sowing age: drilled parcels draw a sowing day uniformly between 25 Sep and 25 Oct (estimate). Their age on the
  scenario's date goes into A.

| Crop (raster) | Mid-October states and shares | Basis |
|---|---|---|
| maize | standing 0.13, harvested stover 0.57, ploughed 0.30 | **Sourced anchor:** "about 87% was in by 16 October" in the reported year (2018), so 0.13 standing. The split of harvested fields between stover and ploughed is an estimate. https://agroberichtenbuitenland.nl/actueel/nieuws/2018/10/29/harvest-in-hungary , via `references.md` T11 |
| wheat (harvested in summer) | stubble 0.10, disced 0.30, ploughed 0.45, harrowed 0.05, sown 0.10 | Winter wheat is sown in October; the best week is the one with 4 October (`fill_textures.md` 4: https://trademagazin.hu/en/keszul-a-vetomagszektor-az-oszi-kalaszosok-szeptemberben-kezdodo-vetesere , https://hu.wikipedia.org/wiki/Vet%C3%A9s ). The shares are estimates. |
| sunflower | harvested stalks 0.40, disced 0.20, sown (winter wheat) 0.40 | The sunflower harvest starts in the second week of September (`references.md` T11, trademagazin above), so it is mostly done by mid-October. The shares are estimates. |
| ploughed | ploughed 0.70, harrowed 0.30 | estimate |
| pasture | pasture_autumn 1.0 | none needed |

**Emergence age.** Wheat counts as emerged at 10 days or more from sowing (estimate). Days to emergence were not found
on a page read (`fill_textures.md` 4).

**Summer (a July scenario).** The map changes by data only:

- Wheat is standing ripe (gold) before an early-July harvest and stubble after it.
- Maize is standing green.
- Sunflower is standing.
- "Ploughed" becomes `fallow_bare`.
- Pasture is green.
- **None of these summer dates is sourced for 1985** (estimate).
- Standing crops are cards or meshes, plus the vertex colour at distance. Their ground slice is `farm_soil`.

**Rule shape in data (design):**
`"ground_seasons": [{"from": "09-20", "to": "11-15", "states": {"maize": [["standing",0.13],["stover",0.57],["ploughed",0.30]], ...}}]`,
`"ground_layers": [{"id": "stubble_standing", "slice": "withered_grass", "rotate": true, "tint": [..], "rough_bias": 0}, ...]`.
The R value is the layer's position in the list.

### 1.4 Forest litter by species

- **The problem:** species are drawn per tree as white noise (section 1.1). A 25 m cell of an oak-acacia-pine wood
  holds a mix, so any per-cell litter choice disagrees with the trees above it.
- **The written basis:**
  - Black locust is about 22.5% of Hungary's forest. Its small woods on the Nyírség are 1-3 ha fragments, mostly
    black locust. The Alföld's sand was afforested with black locust and black pine.
  - Source: `references.md` T4, https://hu.wikipedia.org/wiki/Feh%C3%A9r_ak%C3%A1c ,
    https://epa.hu/01600/01614/00019/pdf/EPA01614_jame_2010_375-386.pdf
  - Real stands are single-species blocks, not salt-and-pepper mixes.
- **Option (b), recommended:** simcore gives each wood block one dominant species, so the ground matches the trees.
  - A stand is the `Canopy` box the trees are planted in (`CanopyMass`, keyed per block; [Repo]
    `map_dressing.hpp:460-490`).
  - Draw the stand's species once by `species_shares` from the box's centre.
  - Each tree takes the stand species with probability `stand_purity` (game value, about 0.8), and the per-point
    draw otherwise. The poplar-near-water rule stays first.
  - The mask's R for a Forest cell is then the litter of its stand (the box whose stamp won the cell). G is the stand
    seed.
  - This changes the planted trees, so `tests/` that pin species counts need updating. It is a behaviour change for
    the project to approve (design).
- **Option (a), no simcore change:** a low-frequency noise (period about 300-500 m) over Forest cells picks the
  litter with the same shares, and poplar within 60 m of water. Patches look plausible, but they will not match the
  trees overhead (design).
- **Litter looks:**
  - Oak: dense curled leaves (`forest_leaves_03`).
  - Pine: needles (`forrest_ground_03` or `Ground077`).
  - Black locust's thin leaflets and grassier floor are **not sourced on a page read**. `dry_decay_leaves` at lower
    density, with a green tint, is an estimate.

### 1.5 The Texture2DArray slice order

- **One ordered list, `"ground_slices"`, in `data/scenery/map_dressing.json`,** read by both the build script and the
  runtime (design).
  - The build script (`tools/ue_make_ground_arrays.py`) sets `source_textures` in that order. A slice's index is its
    position in the list ([Engine 5.8] `Engine/Classes/Engine/Texture2DArray.h:62-104`: `SourceTextures` is an ordered
    editor-only array).
  - At startup the runtime compares the array's slice count with the list's length and logs an error if they differ.
- **Proposed order.** Fields first, as the most used; the 2 m and larger scans are noted in `pbr_textures.md`.

| Slice | Texture (source) | Licence |
|---|---|---|
| 0 | farm_furrows (Poly Haven) | CC0 |
| 1 | farm_soil (Poly Haven) | CC0 |
| 2 | raked_dirt (Poly Haven) | CC0 |
| 3 | Ground071 (ambientCG) | CC0 |
| 4 | withered_grass (Poly Haven) | CC0 |
| 5 | Ground072 (ambientCG) | CC0 |
| 6 | Ground103 (ambientCG) | CC0 |
| 7 | leafy_grass (Poly Haven) | CC0 |
| 8 | dry_mud_field_001 (Poly Haven) | CC0 |
| 9 | forest_leaves_03 (Poly Haven) | CC0 |
| 10 | dry_decay_leaves (Poly Haven) | CC0 |
| 11 | forrest_ground_03 (Poly Haven) | CC0 |
| 12 | sparse_grass (Poly Haven) | CC0 |
| 13 | mud_forest (Poly Haven) | CC0 |
| 14 | dirt (Poly Haven) | CC0 |
| 15 | grass_path_2 (Poly Haven) | CC0 |
| 16 | grass_ground (Poly Haven) | CC0 |
| 17 | muddy_tracks (Poly Haven) | CC0 |
| 18 | aerial_mud_1 (Poly Haven) | CC0 |
| 19 | concrete_floor_damaged_01 (Poly Haven) | CC0 |
| 20 | Ground054 (ambientCG) | CC0 |
| 21 | excavated_soil_wall (Poly Haven) | CC0 |
| 22 | rock_face_04 (Poly Haven) | CC0 |
| 23 | gravel_road (Poly Haven) | CC0 |

- **All CC0, so all redistributable.** The Fab alternates in `pbr_textures.md` (Fab-Standard, not redistributable)
  stay out of the arrays until their files can live in `Content/ThirdParty/` (git-ignored). Mixing them into one
  array would make the array itself non-redistributable (design; licence rules in `pbr_textures.md`).
- **Three arrays share the order:**
  - `T_Ground_BaseH`: albedo plus height, BC7, sRGB on.
  - `T_Ground_N`: BC5, sRGB off.
  - `T_Ground_RAO`: roughness and AO, sRGB off.
  - Source: `tech_material.md` 7.3.
- **Memory (estimate):** BC7 and BC5 are 1 B/texel, and mips add 1/3.
  - At 2048^2 a slice is 5.6 MB per array, so 16.8 MB per slice across three arrays and **about 400 MB for 24
    slices**.
  - At 1024^2 it is **about 100 MB**.
  - Start at 1024^2. Over a 2 m scan that is about 2 mm per texel, roughly one texel per screen pixel at 2 m on a
    1080p, 90-degree view (estimate). Go to 2048^2 only for the field slices if they look soft.
- **Lookup from R to slice (design):**
  - A runtime **layer table** texture, `CreateTransient(256, 2, PF_FloatRGBA)`.
  - Row 0: slice, secondary slice, rotate flag, tiling scale.
  - Row 1: linear tint RGB and roughness bias.
  - The material reads it with `Load`, so a change of layers or season needs no material recompile.

### 1.6 Town at a finer cell (later)

- At 25 m, a house of about 10 x 12 m with its 15 m garden spans 2-4 cells, so yard and garden are coarse.
- **v1.1 (design):** bake a 5 m town mask for the detail areas only.
  - bty21 would be 12 km / 5 m = 2400^2 texels, 23 MB as RGBA8.
  - The shader prefers it inside its rectangle.
  - Its content is the same `GroundLayerAt` evaluated at 5 m.
  - It needs no simcore raster change, because the rule reads the buildings directly.

### 1.7 Straight field edges (v2, design only)

- **Long edges can be exact.** Rows run straight along the heading over the whole 3 km square, at a width drawn per
  square (`FieldParcel`, [Repo] `map_dressing.hpp:256-263`).
  - A small float table per square (heading, width; 14 x 14 squares for the map proper) lets the shader compute the
    long edges exactly.
- **Short edges need per-row data.** They depend on each row's length and offset (`rs`).
  - A per-row table (about 196 squares x 64 rows x 2 floats, about 100 KB) makes them exact too.
- **Exact crop by the inward pull (design):**
  - Read the mask at the pixel's point pulled 18 m inside its analytic parcel. 18 m is more than the cell's
    half-diagonal of 17.7 m.
  - Parcels are at least 80 m wide, so the pulled point's cell centre is always in the same parcel, and the crop
    read is exact.
  - The A-channel distance in 1.2 is the cheaper approximation.
- All of this is v2. v1 lives with 25 m stair steps softened by the domain warp (`tech_material.md` 5).

---

## 2. Gap 2: sampling the mask by world position in UE 5.8, and its precision

### 2.1 PMC UVs are 16-bit on the GPU (measured)

**Test** ([Probe] `scripts\probe_pmc_uv.py`, then `probe_pmc_uv2.py` as the control):

- Run as `UnrealEditor-Cmd ProbeProj.uproject -run=pythonscript -script=... -unattended -nosplash
  -AllowCommandletRendering`.
- The ProceduralMeshComponent plugin is enabled in the probe project.
- An actor is spawned at world (14,001,234.5, 3,000,777, 0) cm, about 140 km north and 30 km east.
  - A PMC was added to the instance through `SubobjectDataSubsystem.k2_gather_subobject_data_for_instance` and
    `add_new_subobject`. `AActor::AddComponentByClass` is `ScriptNoExport` ([Engine 5.8]
    `Engine/Classes/GameFramework/Actor.h:1974`).
- One 10 m quad was made with `create_mesh_section_linear_color`:
  - UV0 = (40.0123, 140.0456).
  - UV1 = (0.5123, 0.25) in the control run.
- An unlit material wrote:
  - R = (UV0.x - 40) x 100.
  - G: run 1 used frac(WorldPos.x / 2500); run 2 used (UV1.x - 0.5) x 100.
  - B = 1.
- An orthographic `SceneCapture2D` (100 cm wide, 16x16, `RTF_RGBA32F`, `SCS_SCENE_COLOR_HDR`) captured once.
  `RenderingLibrary.read_render_target_raw_pixel` read three pixels.

**Result** (log lines quoted from probe7.log and probe8.log):

- `PROBE4 RESULT R/B (UV0.x-40)*100 = 0.0`. A 32-bit UV would give 1.23. Half floats between 32 and 64 are spaced
  1/32, so 40.0123 rounds to 40.0.
- `PROBE5 RESULT ... | G/B (UV1.x-0.5)*100 = 1.2207`.
  - 0.5123 in half precision is 0.51220703 (spacing 2^-11 between 0.5 and 1), so the expected value is 1.2207.
  - A 32-bit UV would give 1.2300.
- **Both channels match half-float rounding exactly.** In 5.8.3 the PMC proxy uploads UVs as `FVector2DHalf`.
  - The CPU side keeps `FVector2D`.
  - `FStaticMeshVertexBuffer::SetVertexUV` writes halves unless `GetUseFullPrecisionUVs()`. [Engine 5.8]
    `Engine/Public/Rendering/StaticMeshVertexBuffer.h:330-528`.
  - This matches the 2019-2022 forum reports cited in `tech_material.md` 2.1:
    https://forums.unrealengine.com/t/can-someone-confirm-me-that-the-uv-on-the-uproceduralmeshcomponent-are-32-bits/449523
- **Consequence for SAM3D.** `SimMapActor` puts the map's (E, N) in km into UV0. [Repo]
  `unreal/SAM3D/Source/SAM3D/Private/SimMapActor.cpp:740-790`, `Uv.Add(FVector2D(E / 1000.0, N / 1000.0))`.
  - On the GPU those UVs step 15.6 m at 16-32 km and 125 m at 128-256 km.
  - Half spacing: https://en.wikipedia.org/wiki/Half-precision_floating-point_format
  - **They must not be used for the mask or for tiling.** The current vertex-colour material does not use them, so
    nothing is broken today.
- **Fixes, in order of preference:**
  1. Sample by world position (2.2). This needs no mesh change.
  2. If a per-vertex coordinate is ever needed, put a **tile-local** UV in [0, tile size in km), where halves are
     still coarse, or store it in vertex colour.
  3. A custom scene proxy with full-precision UVs (the PMC source is not in the launcher install).
- **The test pattern itself works in a commandlet.**
  - In a fresh project, the first run took 9 min. Most of it was compiling engine debug materials that the capture
    needs (`WireframeMaterial`, `MAT_LevelColorationLitLightmapUV`).
  - The cached second run took 36 s from launch to exit.
  - A small CPU detail: `unreal.KismetProceduralMeshLibrary` is not exposed to Python under that name (probe7.log).
- [Probe] The logs are `...\scratchpad\ue_probe\probe7.log` and `probe8.log`.

### 2.2 What the material translator emits for LWC world position in 5.8

- **Setup** ([Probe] `scripts\probe_hlsl.py`, log probe6.log):
  - The material is `M_ProbeLWC`.
  - It was run with `-AllowCommandletRendering -dpcvars=r.DumpShaderDebugInfo=1,r.DumpShaderDebugShortNames=1`.
  - The generated pixel shader is in
    `...\ProbeProj\Saved\ShaderDebugInfo\PCD3D_SM5\M_ProbeLWC_7486ec54dd13270\Default\LocalVF\BPPSFNoLMPol\0_b18579dd4561dbf8\BasePassPixelShader.usf`.
- **Defines:** `MATERIAL_LWC_ENABLED 1`, `WSVECTOR_IS_TILEOFFSET 1`, and `UE_LWC_RENDER_TILE_SIZE 2097152.00f`.
  - So world position is an `FLWCVector3`: a tile plus a float offset, with 20.97 km tiles.
  - If `r.MaterialEditor.LWCEnabled` were 0, the base pass would define `UE_DF_FORCE_FP32_OPS 1` and do plain FP32
    math. [Engine 5.8] `Engine/Shaders/Private/BasePassPixelShader.usf`, `MaterialTemplate.ush` (the
    `#if !MATERIAL_LWC_ENABLED` block).
- **Emitted code:**
  - **World Position:** `FLWCVector3 Local0 = GetWorldPosition(Parameters);`
  - **Divide, then Frac:** `LWCDivide(Local1, 200.0)`, then `float3 Local3 = LWCFrac(Local2);`
    - In the library, `LWCDivide` multiplies both the tile and the offset by `rcp(d)`.
    - `LWCFrac` is `frac(frac(Tile * TILE_SIZE) + Offset)`.
    - [Engine 5.8] `Engine/Shaders/Private/LWCOperations.ush:97-145`.
  - **Fmod:** `float3 Local5 = LWCFmod(Local1, 200.0);`, defined as `LWCToFloat(Lhs - trunc(Lhs/Rhs)*Rhs)` (same
    file).
  - **DoubleVectorParameter subtraction:**
    `LWCSubtract(Local1, DFToWS(MakeDFVector3(PreshaderBuffer[0].xyz, ...)))`, then `LWCToFloat`.
    - The double parameter arrives as a high/low float pair. The difference is exact to the float of the result.
    - Over the 40 km map the result is at most 4e6 cm, where the float spacing is 0.25-0.5 cm (estimate from the
      float format).
  - **Custom node with a Texture Object input:** `Tex.Load(int3(c, 0))` compiles. The texture and its sampler are
    passed as `Material_Texture2D_0, Material_Texture2D_0Sampler`.
  - **Custom node fed the LWC position:** it receives **both** a demoted `float3 P` and an `FLWCVector3 LWCP`.
    - So exact LWC math, such as a rotated furrow UV wrapped by period, can be written in a Custom node.
  - **Texture2DArray sample:** `Texture2DArraySample(..., float3(uv, slice))`.
  - **HighPrecisionWorldPosTextureSampling:** `GetTranslatedWorldPosition / T + LWCFrac(GetWorldCameraOrigin / T)`,
    then a dot product with the U and V vectors.
    - Its inputs are "Texture Object", "TilingSize", "Texture Coord U Vector" and "Texture Coord V Vector".
    - Its outputs are "Texture (F4)" and "High Precision World Position" (probe5.log).
    - The asset was saved in 5.0. [Engine 5.8]
      `Engine/Content/Functions/Engine_MaterialFunctions02/HighPrecisionWorldPosTextureSampling.uasset`.
- **Instruction counts with rendering:** `M_ProbeLWC` was 113 PS and 136 VS instructions, with 5 pixel texture
  samples (probe5/6.log).

### 2.3 Precision far out (measured)

- **Run 1** of the PMC probe also wrote frac(WorldPos.x / 2500 cm) through the Frac node, at x = 14,001,234.5 cm
  (140 km out, tile index 6).
- **The capture's geometry:** 16 px over 100 cm puts pixel centres at ±3.125 cm from the centre. Image y runs along
  world -X (looking down at pitch -90, yaw 0).
- **Expected (double precision):** 0.49255 for pixel (8, 8) and 0.49505 for row 7.
- **Measured:** 0.49219 and 0.49463.
  - Both are about 0.9-1.0 cm low as a constant offset. The two rows differ by 6.11 cm against 6.25 expected, so the
    pixel-to-pixel step is good to 0.15 cm.
- **Likely cause of the offset (estimate):** `LWCDivide` multiplies `Tile * TILE_SIZE` by a float `rcp(2500)`.
  - 6 x 2,097,152 / 2500 = 5033.2. Its float spacing is about 5e-4, which is about 1.2 cm.
- **What this means:**
  - A 25 m mask cell is located to about 1 cm anywhere within 150 km. With the origin-subtraction path of 2.2 it is
    better still.
  - A 2 m tiling texture located through LWC Frac is offset by a constant of up to about 1 cm. That is not visible.
    It does not swim, because it is the same for neighbouring pixels.
  - It may jump by about 1 cm when the camera crosses a 21 km LWC tile (estimate). Not visible either.

### 2.4 Is HighPrecisionWorldPosTextureSampling needed?

- **For the mask: no.**
  - Use `(AbsoluteWorldPos - MaskOriginWorld)` with `MaskOriginWorld` a **DoubleVectorParameter**. The emitted code
    is `LWCSubtract` then `LWCToFloat` (2.2).
  - Then texel = `M x d` in cm, with M = `MapFrameLink`'s 2x2 / (100 x cell). Pass M as four scalars (or two vector
    parameters). Unreal X is north and Y is east: [Repo]
    `unreal/SAM3D/Source/SAM3D/Private/SimWorldSubsystem.cpp:84-93`.
  - Then read the four neighbours with `Load` in a Custom node.
  - `MaskOriginWorld` is the Unreal world position of the grid's (west, south) corner, mapped through
    `MapToFrame`. Since FrameToMap is exactly affine ([Repo] `terrain_map.hpp:1985-2070`), this is exact.
- **For the tiling detail layers: use the HighPrecision form, not the Frac node.**
  - The Frac node wraps **per pixel**, so the UV jumps by 1 inside a quad. Hardware derivatives then blow up and pick
    the smallest mip, which draws a one-pixel seam line every period.
    - Estimate from how `ddx`/`ddy` differences work. The usual cure is `SampleGrad` with derivatives of the unwrapped
      value.
  - `TranslatedWP / T + LWCFrac(CameraOrigin / T)` is continuous across the screen. It differs from WP/T by an integer
    that changes only when the camera crosses a period.
  - It is precise near the camera, where precision matters. At 2 km from the camera, with T = 2 m, the value is about
    1000 and its float spacing is 6e-5, about 0.1 texel of a 2048 texture (estimate).
  - Rotated field UVs: rotate the camera-relative part by the parcel's heading. The constant per-frame part is
    `frac(CameraOrigin / T)` rotated, which can be computed in a Custom node from `LWCP`, which the Custom node
    receives (2.2).
- **`DFFracDemote` and `LWCFrac` stay useful** for anything that must be periodic in absolute space. [Engine 5.8]
  `Engine/Shaders/Private/DoubleFloatOperations.ush:896-933`.

### 2.5 RVT: the disagreement resolved

- **The two claims:**
  - `tech_geometry.md` and `aaa_lessons.md` quote Epic's page: "Any component placed in the scene can render to an
    RVT". https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine
  - `tech_material.md` 4 cites forum threads saying a PMC cannot:
    https://forums.unrealengine.com/t/virtual-texture-and-procedural-mesh/480227 ,
    https://forums.unrealengine.com/t/can-you-write-to-rvt-from-a-procedural-mesh-component/585991
- **Both are true at different levels (5.8 source):**
  1. Every primitive has the property: `TArray<TObjectPtr<URuntimeVirtualTexture>> RuntimeVirtualTextures` on
     `UPrimitiveComponent`. [Engine 5.8] `Engine/Classes/Components/PrimitiveComponent.h:909,935`.
  2. A proxy writes only if its type opts in. `WritesVirtualTexture()` is
     `bSupportsRuntimeVirtualTexture && RuntimeVirtualTextures.Num() > 0`. The flag's comment reads "This is only set
     true by the constructors of proxy types which support writing to runtime virtual texture." [Engine 5.8]
     `Engine/Public/PrimitiveSceneProxy.h:817,1639-1640`.
  3. The 5.8 PMC proxy has no static draw path. The plugin's PDB names these `FProceduralMeshSceneProxy` methods:
     `CanBeOccluded`, `GetDynamicMeshElements`, `GetDynamicRayTracingInstances`, `GetMemoryFootprint`,
     `GetTypeHash`, `GetViewRelevance`, `HasRayTracingRepresentation`, `IsRayTracingRelevant`,
     `UpdateSection_RenderThread`. There is **no `DrawStaticElements`**. [Engine 5.8]
     `Engine/Plugins/Runtime/ProceduralMeshComponent/Binaries/Win64/UnrealEditor-ProceduralMeshComponent.pdb`
     (strings).
     - This agrees with the forum's finding that RVT pages draw static mesh batches.
     - The proxy `.cpp` is not shipped, so the constructor's flag cannot be read directly. With no static batches,
       nothing would be drawn even if the flag were set (estimate).
- **Verdict:** in 5.8 the PMC ground tiles cannot write an RVT. RVT stays a later optimisation that needs the tiles
  converted to non-Nanite `UStaticMesh`. The route is in `tech_material.md` 4: `BuildMeshDescription(PMC)` →
  `UStaticMesh::BuildFromMeshDescriptions(bFastBuild)`.

### 2.6 `r.VirtualTextures` in 5.8 (and other material cvars)

- **[Probe] probe1.log**, in a blank content-only project with no config:
  - `r.VirtualTextures = 1`.
  - `r.MaterialEditor.LWCEnabled = 1`.
  - `r.MaterialEditor.LWCTruncateMode = 2`.
  - `r.VT.EnableAutoImport = 0`.
- **SAM3D's open editor** gave the same `r.VirtualTextures = 1`. It also showed `r.VT.MaxUploadsPerFrame = 2`,
  `r.VirtualTexturedLightmaps = 0` and `r.VT.RVT.StreamingMips = 1` (read in the earlier part of this pass).
- **Nothing sets it in config.** `BaseEngine.ini` does not set `r.VirtualTextures`, so 1 is the cvar's compiled
  default. [Engine 5.8] `Engine/Config/BaseEngine.ini` (grep).
- **SAM3D does not override it.** [Repo] `unreal/SAM3D/Config/DefaultEngine.ini`.
- The setting is `URendererSettings::bVirtualTextures`, "Enable virtual texture support", and needs a restart.
  [Engine 5.8] `Engine/Classes/Engine/RendererSettings.h:413-417`.
- **So VT is already on.** RVT can be tried later without a config change. SVT import is off by default, so imported
  textures stay ordinary unless marked VT.
- **`LWCTruncateMode = 2` is not explained.** Its help text was not found in the shipped headers or near the cvar name
  in `UnrealEditor-Engine.dll`. It is unknown, and it does not affect the plan above.

### 2.7 The in-game check (spec for `-SimMaskCheck`, design)

It follows the existing `-SimGroundCheck` and `tools/ue_ground_check.py` pattern ([Repo] `CLAUDE.md` commands).

1. **simcore test, `simcore/tests/test_ground_mask.cpp`:**
   - Every grid is on the 25 m lattice: `west_m / cell_m` and `south_m / cell_m` are integers.
   - For every texel of a baked mask: R equals `GroundLayerAt` from `CoverRaster::At` and `CropAt` plus the state
     rule at the cell centre.
   - B equals `round(heading / pi * 256) & 255` from `FieldParcel`'s square.
   - G is stable across two bakes.
   - Bytes are in B, G, R, A order.
   - The parcel's state is constant within a parcel.
   - Tests use the test map and generic numbers.
2. **Unreal, `-SimMaskCheck` (after the map is built):**
   - simcore supplies 200 sample points:
     - Stratified by layer.
     - At least 2 m from cell edges.
     - Plus points in the capital grid and at the four corners of the map proper.
   - For each point:
     - Set the ground MID's `DebugMode` to 1. The emissive is the decoded R/255, G/255 and B/255, unlit.
     - Capture a 4x4 `RTF_RGBA32F` orthographic patch 1 m wide over the point.
     - Read it back.
     - Compare R and B exactly with simcore's bytes.
   - Log `SimMaskCheck: ok N/N`, or each mismatch with (e, n), the expected bytes and the read bytes.
   - `tools/ue_mask_check.py` parses the log.
   - The probe in 2.1 is a working template: SceneCapture2D, `read_render_target_raw_pixel`, 36 s when cached.
3. **Precision probe, same mechanism:**
   - Points 20 km, 28 km (map corner) and 150 km (far ground edge) from the scenario origin.
   - DebugMode 2 outputs `frac(HPWorldPos / 2 m)` and the mask's sub-cell `frac(texel)`.
   - Compare with double values from simcore.
   - **Pass:** absolute error < 1 cm, and neighbouring-pixel steps within 0.2 cm of expected (thresholds from 2.3).
4. **Regression for half UVs (optional):** assert that no ground material reads `TexCoord[0]` (a Python check over the
   material's expressions in the commandlet).

---

## 3. Gap 3: building the arrays and the master material by commandlet

### 3.1 Texture2DArray creation from Python (measured, 5.8.3)

Steps from [Probe] `scripts\probe_array.py`, logs probe1.log and probe2.log, with `-nullrhi`:

1. **Import.** `AssetImportTask` with `automated=True` and `replace_existing=True` imported three PNGs as
   `Texture2D`: two at 256 px and one at 128 px.
2. **Create an empty array.** `tools.create_asset('T2DA_Probe', '/Game/Probe', unreal.Texture2DArray,
   unreal.Texture2DArrayFactory())` returned a `Texture2DArray` with 0 source textures, source (0, 0) and built size
   0x0x0. **An empty `InitialTextures` is accepted.**
   - The factory has `UPROPERTY() InitialTextures` and overrides `CanCreateNew`, `ConfigureProperties` and
     `FactoryCreateNew`. [Engine 5.8] `Editor/UnrealEd/Classes/Factories/Texture2DArrayFactory.h`.
3. **Fill it.** `arr.set_editor_property('source_textures', [A, B])` rebuilt the source **under `-nullrhi`**:
   - Source went from (0, 0) to (524288, 524288) bytes, which is 2 x 256 x 256 x 4 B.
   - Built size became 256x256x2.
   - Channel min and max were computed.
4. **Mismatched size.** Adding the 128 px texture logged "Mismatched sizes of the source textures, resizing all the
   array elements to 256x256". The array became 256x256x3. **It is resized, not rejected.**
   - So the build script must check sizes itself.
5. **The notification is what triggers the rebuild.**
   - Setting `[A, B]` on a second new array with `PropertyAccessChangeNotifyMode.NEVER` left its source at (0, 0).
   - A later **notifying** set with the **same** list also left it at (0, 0). The change detection sees no change.
   - `Texture2DArray` has no `post_edit_change` in Python (AttributeError).
   - `UpdateSourceFromSourceTextures` is not a UFUNCTION. [Engine 5.8] `Engine/Classes/Engine/Texture2DArray.h:62-104`.
6. **Settings.** `compression_settings = TC_BC7`, `srgb`, `address_x` and `address_y` were set, saved, and **survived
   reload**. probe3.log shows "slices 2 compression TC_BC7 srgb True".

**Pattern for `tools/ue_make_ground_arrays.py` (design):**

1. Import every slice with an explicit size check. Resample the sources to one size before import, outside Unreal.
2. Set each texture's compression and sRGB (normals: `TC_NORMALMAP`; masks: `TC_MASKS`; `tech_material.md` 3).
3. **Delete and re-create** the array on every build.
4. Set `source_textures` **once**, with the default notification.
5. Set compression, sRGB and address modes.
6. Save.
7. Check `blueprint_get_built_texture_size()` == (S, S, N), where N is the length of `ground_slices`.

**Alternative.** An array can also come from a multi-slice DDS (`bSourceGeneratedFromSourceTexturesArray` false).
[Engine 5.8] `Texture2DArray.h:62-104`. Avoid it while the Python route works.

### 3.2 The master material from Python

- Already used in this repo: `MaterialFactoryNew` → `create_material_expression` → `connect_material_expressions` →
  `recompile_material` → `save_asset`. [Repo] `tools/ue_build_kesmark_range.py`.
- **Confirmed in 5.8.3** ([Probe] probe2.log, probe5/6.log):
  - `MaterialExpressionTextureSampleParameter2DArray` takes an array and a float3 UV.
  - `MaterialExpressionDoubleVectorParameter` can be created and connected.
  - `MaterialExpressionCustom` takes `CustomInput`s, including a Texture Object input.
  - `MaterialExpressionMaterialFunctionCall` with HighPrecisionWorldPosTextureSampling connects by pin name.
  - `get_material_expression_input_names` and `get_material_expression_output_names` list the pins.
- **In 5.8 `RecompileMaterial` returns the compiler's error strings.** `GetStatistics`, `ListShaders` and
  `GetNumShaderTypes` also exist. [Engine 5.8] `Editor/MaterialEditor/Public/MaterialEditingLibrary.h`.

### 3.3 `-nullrhi` cannot validate the material. `-AllowCommandletRendering` can.

- **Under `-nullrhi`** (probe2/3.log):
  - `recompile_material` returned `[]` for a **broken** graph: an Add of float4 and float3.
  - `get_statistics` returned 0 instructions.
  - `get_num_shader_types` returned 12.
  - The graph is saved but **not compiled**.
- **With `-AllowCommandletRendering` and no `-nullrhi`** (probe4.log):
  - The same broken material returned `'(Node Add) Arithmetic between types float4 and float3 are undefined',
    '(Node Add) Invalid input types: float4, float4'`.
  - The array material compiled to 270 PS and 140 VS instructions, with 4 samplers and 1 pixel texture sample.
- **Cost:**
  - The first such start in a fresh project took about 2 minutes.
  - A first capture-using run took 9 minutes, mostly compiling engine debug materials.
  - Cached runs took 0.5-2 minutes ([Probe] log timestamps).
- **Recommendation (design):**
  - Build with `-nullrhi`, which is fast and creates and saves everything.
  - Then run a **second validation pass** with `-AllowCommandletRendering` that reloads the material, recompiles,
    fails on any error string, and fails on a 0-instruction result.
  - Both are run from PowerShell (memory note: Git Bash mangles /Game paths).

### 3.4 The runtime side (no commandlet)

- The mask and layer table textures are transient and made at map load (1.2, 1.5). They need no assets.
- Only the three arrays and the master material are assets.
- `UTexture2DArray::CreateTransient(SizeX, SizeY, ArraySize, Format, Name)` also exists. [Engine 5.8]
  `Texture2DArray.h:107`. Its slices would have to be filled from decoded images at runtime, so it is not the route
  for BC-compressed CC0 sets (design).

### 3.5 ambientCG normal convention

- **No page read states GL or DX.** That covers the asset pages, the docs index, the asset-types page, the updates
  page, and the v2 and v3 API docs and responses for Ground077, Ground103, Ground104 and Plaster007.
  - https://docs.ambientcg.com/
  - https://docs.ambientcg.com/asset-types/
  - https://docs.ambientcg.com/updates/
  - https://docs.ambientcg.com/api/v2/full_json/
  - https://docs.ambientcg.com/api/v3/assets/
  - https://ambientcg.com/api/v3/assets?id=Ground077,Ground103,Ground104,Plaster007&include=maps,downloads,dimensions,tags
    (`zipContent` came back empty)
- **The site's own previews are DirectX-convention:**
  - The preview file is `Ground077_SQ_NormalDX.jpg`
    (https://f003.backblazeb2.com/file/ambientCG-Web/media/surface-preview/Ground077/Ground077_SQ_NormalDX.jpg).
  - The 3D preview URL carries `normal_type=directx`.
  - Source: https://ambientcg.com/api/v2/full_json?id=Ground077&include=previewData,usdData,imageData,mapData
- **In-zip naming.** The 29 Aug 2023 update notice gives `<id>_<res>-<fmt>_<Map>.<ext>`.
  https://docs.ambientcg.com/updates/
  - Whether `<Map>` includes both `NormalGL` and `NormalDX` is customary knowledge, **not verified**.
- **Rule for the import script (design):**
  - Prefer a file whose name ends `_NormalDX`. Unreal is DirectX-convention (`pbr_textures.md`).
  - If only `_NormalGL` (or an unlabelled normal) exists, flip green.
  - **Verify every set automatically:**
    - Compute the correlation between the normal's green channel (decoded to [-1, 1]) and the vertical gradient of
      the displacement map, `-dH/dv` in image space.
    - The sign tells GL from DX: green points up the image in GL.
    - Fail the import if the sign disagrees with the file name.
  - Poly Haven sets ship both `nor_gl` and `nor_dx` (`pbr_textures.md`), and the same test guards them.

### 3.6 Scan sizes of the four ambientCG sets

- **Published sizes:** the v2 API's `dimensionsData` returned **0** for Ground077, Ground103, Ground104 and
  Plaster007.
  https://ambientcg.com/api/v2/full_json?id=Ground077,Ground103,Ground104,Plaster007,Ground071,Ground072&include=dimensionsData,downloadData,mapData,displayData
- **The same call gives sizes for others:** Ground071 175 cm and Ground072 175 cm. A sibling call gives Ground037
  210 cm, Ground054 350 cm, Ground106 250 cm and Gravel043 160 cm.
  https://ambientcg.com/api/v2/full_json?id=Ground037,Ground054,Gravel043,Ground106&include=downloadData,dimensionsData
- **Visual estimates** from the square colour previews. **Low confidence.**

| Set | Preview | What it shows | Size estimate |
|---|---|---|---|
| Ground077 | https://f003.backblazeb2.com/file/ambientCG-Web/media/surface-preview/Ground077/Ground077_SQ_Color.jpg | Orange-brown crumbly pine-needle litter with twigs. The longest twigs span about 10% of the width. | 1.0-1.5 m |
| Ground103 | https://f003.backblazeb2.com/file/ambientCG-Web/media/surface-preview/Ground103/Ground103_SQ_Color.jpg | Brown soil with pebbles and small green tufts | about 2 m |
| Ground104 | https://f003.backblazeb2.com/file/ambientCG-Web/media/surface-preview/Ground104/Ground104_SQ_Color.jpg | Yellowish gravelly loess with faint horizontal layering | 1.5-2.5 m |
| Plaster007 | https://f003.backblazeb2.com/file/ambientCG-Web/media/surface-preview/Plaster007/Plaster007_SQ_Color.jpg | Grey-white plaster with green weathering. No scale cue. | 2-3 m |

- **Plan (design):**
  - Enter 2.0 m as each layer's provisional `size_m` in `ground_layers`, with `"confidence": "estimate"`.
  - Calibrate by eye in the game against a 1 m reference, such as a crewman's boot or a 25 cm stalk spacing.
  - Record the result in `data/scenery/assets.json`'s entry for the set.
  - The ambientCG sets with published sizes (Ground071, Ground072, Ground054, Ground106) need no calibration.

---

## 4. What stays open

1. **ambientCG GL or DX** inside the zips is not stated on any page. It is guarded by the import-time test (3.5).
2. **The scan sizes** of Ground077, Ground103, Ground104 and Plaster007 are visual estimates only (3.6).
3. **The October state shares** are estimates, except the maize anchor (87% harvested by 16 October), which comes from
   2018, not 1985. Days to emergence and the 1985 harvest dates were not found (1.3).
4. **Black locust litter and the poplar floor:** no written description was read (1.4).
5. **Per-stand species** needs a simcore behaviour change for the project to approve (1.4, option b).
6. **`r.MaterialEditor.LWCTruncateMode = 2`**: meaning unknown (2.6).
7. **The roughly 1 cm constant offset** of LWC Frac at 140 km is attributed to `rcp` rounding by estimate. It was not
   isolated by a second test (2.3).
8. **The derivative seam** at Frac wraps is reasoned, not captured in a probe (2.4).
9. **The web search budget was used up** during this pass. Only known URLs were fetched afterwards.
