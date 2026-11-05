# Unreal's open-world terrain, vegetation and procedural tools (UE 5.6-5.8), and what they mean for SAM3D

Research pass of 5 Nov 2026 for the terrain rebuild. Reading only: nothing was downloaded, installed or signed up for.

This note does not repeat `TERRAIN_GEOMETRY_2026-11.md` (PMC and its alternatives, baking Nanite tiles, HLOD
commandlets, foliage culling, impostors, haze, measurement) or `TERRAIN_GROUND_MATERIAL_2026-11.md` (runtime masks, RVT
on non-Landscape meshes, texture arrays) or `TERRAIN_VEGETATION_2026-11.md` (Megaplants, Megascans tree packs, licences).
It points back to them where they already answer something.

How claims are marked:

- **[Local 5.8.3]** is a file of the installed engine `D:/UE_5.8` (5.8.3). This is the strongest evidence for 5.8.
- **[Epic doc]** is dev.epicgames.com, for the version the page states.
- **[Epic staff]** is a forum reply by Epic staff. **[Forum]** is a community post: weaker.
- **[Repo]** is this repository. **(design)** and **(estimate)** are my own reasoning.
- Note: 5.8.4 is out (20 Oct 2026, https://forums.unrealengine.com/t/5-8-4-hotfix-released/2842775); this PC has 5.8.3.

---------------------------------------------------------------------------------------------------------------------------

## 0. Answers in short

| Question | Answer |
|---|---|
| Can a Landscape be built from external data by script? | **Yes, but only from editor C++.** There is no Python `LandscapeEditorSubsystem` (the Python doc page is a 404; no such header in 5.8.3). Python can only push a render target into an existing landscape. A C++ commandlet calling `ALandscapeProxy::Import` (editor-only) is the route, as Datasmith, Houdini Engine and community tools do. |
| Size for 40 km | **10 m spacing fits Epic's 4033 x 4033 preset exactly (40.32 km). 5 m fits 8129 x 8129 (40.64 km).** 2.5 m needs a 16k World Partition landscape, which the community reports as the practical import limit with edit layers. |
| Can simcore sample the exact landscape triangles? | **Only at LOD0, and only after matching three things**: one uniform grid spacing (Landscape has one resolution; simcore has 50 m plus 10/20 m detail areas), 16-bit heights, and the split diagonal. The collision heightfield splits each cell along a fixed diagonal **[Local 5.8.3]**, which looks opposite to simcore's split (to verify). Rendered LODs and Nanite landscape are simplifications of LOD0. |
| What does only a Landscape unlock? | Landscape Grass (GPU grass from layer weights), Water bodies carving the ground, Landmass brushes, landscape splines deforming it, PCG's landscape nodes. **Everything else (RVT, Nanite, Lumen cards, distance fields, PCG scattering, auto-materials) also works on static-mesh ground.** |
| PCG status | Production-ready since 5.7. GPU execution is **Beta**, GPU instancing **Experimental** in the 5.8 docs. Runtime hierarchical generation around generation sources works. Biome Core is **Experimental**. Custom C++ nodes and Python graph building both exist. |
| PCG and multiplayer | Not replicated. Epic staff (May 2026): small hardware differences are acceptable for static objects; anything gameplay-relevant must be replicated and generated on the server. |
| Megaplants as instances | They are skeletal Nanite Foliage. 5.8 has `UInstancedSkinnedMeshComponent::AddInstances` (BlueprintCallable) and a PCG Skinned Mesh Spawner **[Local 5.8.3]**. So they can be placed from C++ at simcore's positions without exporting them. Needs the Nanite Foliage project setting; packaged-build crash UE-386321 not confirmed fixed in any hotfix notes. |
| Recommendation | **(a) Keep the sim-owned mesh ground** (moved to static-mesh tiles as `TERRAIN_GEOMETRY` recommends), add RVT, PCG fed by simcore through a custom C++ node, and Megaplants through instanced skinned meshes. Landscape costs more than it unlocks for a data-driven, script-only, sim-owned map. A two-part spike is in section 8. |

---------------------------------------------------------------------------------------------------------------------------

## 1. Landscape built by script from external data

### 1.1 The APIs

| API | What it does | Where it runs | Source |
|---|---|---|---|
| `ALandscapeProxy::Import(Guid, MinX, MinY, MaxX, MaxY, NumSubsections, SubsectionSizeQuads, TMap<FGuid, TArray<uint16>> HeightData, HeightmapFileName, TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayers, AlphamapType, EditLayers)` | Imports heights and weight layers into a new landscape. | Inside `#if WITH_EDITOR`: editor and commandlets only. | [Local 5.8.3] `Engine/Source/Runtime/Landscape/Classes/LandscapeProxy.h:1421-1423` (block 1326-1679 is `WITH_EDITOR`); API page https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Landscape/ALandscapeProxy/Import?application_version=5.0 |
| `FLandscapeImportHelper` | Reads heightmap and weightmap files and descriptors; picks import sizes. | Editor module `LandscapeEditor`. | [Local 5.8.3] `Engine/Source/Editor/LandscapeEditor/Public/LandscapeImportHelper.h`; https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/LandscapeEditor/FLandscapeImportHelper |
| `LandscapeImportHeightmapFromRenderTarget`, `LandscapeImportWeightmapFromRenderTarget` | BlueprintCallable, so Python can call them: overwrite an **existing** landscape's heights or a layer from a render target. | Editor only. | [Local 5.8.3] `LandscapeProxy.h:1574-1582`; https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Rendering/LandscapeImportHeightmapfromRend- |
| `EditorApplySpline(USplineComponent*, widths, falloffs, ..., PaintLayer, EditLayerName)` | BlueprintCallable: stamps a spline (raise/lower, paint a layer) into an edit layer. | Editor. | [Local 5.8.3] `LandscapeProxy.h:1039-1040` |
| `FLandscapeConfigHelper::PartitionLandscape(World, Info, GridSizeInComponents)`, `ChangeGridSize`; `ULandscapeSubsystem::ChangeGridSize` | Split a landscape into World Partition streaming proxies. | C++. | [Local 5.8.3] `Landscape/Public/LandscapeConfigHelper.h:71-72`, `LandscapeSubsystem.h:153` |
| Python `LandscapeEditorSubsystem` | **Does not exist.** The 5.7 Python doc URL redirects to a 404, and no such class is in the 5.8.3 headers. `ULandscapeSubsystem` has no UFUNCTIONs. | n/a | https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/LandscapeEditorSubsystem?application_version=5.7 (302 to 404); [Local 5.8.3] `LandscapeSubsystem.h` |
| `ULandscapeSplinesComponent` | Only `GetSplineMeshComponents` is BlueprintCallable. Building landscape splines needs C++ (control points and segments are UObjects). Houdini Engine does this (Experimental in its docs; segments "don't exactly match"). | C++ | [Local 5.8.3] `LandscapeSplinesComponent.h:156`; https://sidefx.com/docs/houdini/unreal/landscape/splineoutput.html |

- Spawning a Landscape from Python gives a `LandscapePlaceholder` with no heightmap support; the working community route
  is C++ that spawns `ALandscape`, calls `Import` with a new GUID, sets layer infos and registers components, copied from
  the New Landscape tool and Datasmith (UE 4.27-5 era, no Epic reply).
  https://forums.unrealengine.com/t/creating-a-lanscape-using-c-or-the-python-api/504997 . A weight layer also needs a
  `ULandscapeLayerInfoObject` or it does not show "and may crash" (same thread, Dec 2022).
- **Edit layers are mandatory since 5.7**: "Non-edit layer landscapes are deprecated, all landscapes use the edit layer
  system now", and merges run through "BatchedMerge" **[Local 5.8.3]** `Landscape/Classes/Landscape.h:215,413,662`.
  The merge runs on the GPU with temporary render targets. A commandlet run of the PCG World Partition builder with
  `-AllowCommandletRendering` crashed on a big landscape for that reason; a hidden `bEnableEditorLayersTick` bypasses the
  merge (UE 5.5 thread, community answer).
  https://forums.unrealengine.com/t/pcgworldpartitionbuilder-crash-trying-to-load-big-landscape-chunk/2692004 .
  So a landscape commandlet will need `-AllowCommandletRendering` (not `-nullrhi`), and batch sizes must be watched.
  **Not verified** whether `Import` alone works under `-nullrhi`.

### 1.2 Sizes, limits and streaming

Epic's 5.8 Landscape Technical Guide: heights are 16-bit, -256 to 255.992 times the Z scale (Z scale 100 gives about
±256 m); quads per component 63 or 126 (or 127/254 with 2x2 sections); a recommended maximum of **1024 components**,
"each component has a render-thread CPU cost and each section is a draw call"; heightmap size = component count x
quads + 1. https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-technical-guide-in-unreal-engine

Height precision (my calculation from the guide): one height step is Z scale / 128 cm. The Kesmark map's relief is a
few tens of metres, so Z scale 25 (±64 m about a chosen base) gives 0.2 cm steps; Z scale 100 gives 0.78 cm.

| Spacing | Quads per side | Preset that fits | Components | Heights (uint16) | Fit for SAM3D |
|---|---|---|---|---|---|
| 50 m (simcore's map proper today) | 800 | 1009 x 1009 (63 quads, 16x16) = 50.4 km | 256 | 2 MB | Equals simcore's grid, but weightmaps and grass at 50 m are too coarse, and the 10 m battery area cannot be in it. |
| **10 m** (simcore's bty21 detail) | 4000 | **4033 x 4033** (126 quads, 32x32) = **40.32 km** | 1024 | 32 MB | Best fit: one uniform 10 m ground. simcore would need a uniform 10 m map proper. |
| 5 m | 8000 | **8129 x 8129** (254 quads, 32x32) = 40.64 km | 1024 | 132 MB | Fits the largest preset. Four times simcore's LOS sampling cost of 10 m (estimate). |
| 2.5 m | 16000 | none; a World Partition landscape about 16k | 4096+ | 512 MB | Community: "World Partition importing with Edit Layers enabled is limited to 16k x 16k"; 32k imports need "64GB+" (UE 5.1-5.3 reports). https://forums.unrealengine.com/t/landscape-max-size-with-world-partition/232989 |
| 1 m | 40000 | none | | 3.2 GB | Out of reach for import and for simcore's memory. |

- World Partition: the landscape is split into streaming proxies of "grid size" components per side, and regions group
  proxies (community explanations; the API calls it the streaming proxy cell size).
  https://forums.unrealengine.com/t/world-partition-grid-size-and-region-size/1350946 ;
  https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Landscape/ULandscapeInfo/GetGridSize?application_version=5.4 .
  A 40 km landscape at 10 m does not need World Partition to stream (32 MB of heights); SAM3D's current levels do not use
  World Partition (`TERRAIN_GEOMETRY` 2.3).
- **The capital's 20 m detail area lies outside the 40 km map proper** (centre 36, 58 km) [Repo]
  `data/maps/kesmark_range.json:53`. A landscape covering both would be about 70 km across, or the capital needs its own
  landscape (design).
- **Nanite landscape** (5.8 doc): enabled per landscape, built with Build Data or at cook; "Nanite data streams on top of
  the existing non-Nanite data", both stay in memory, and non-Nanite data is still needed for RVT and water; Nanite
  "won't keep a regular grid unless the camera is close"; a skirt hides seams between proxies.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/using-nanite-with-landscapes-in-unreal-engine

### 1.3 The triangulation, and whether simcore can sample the same surface

- **Collision (verified in source):** Chaos's heightfield makes each cell's two triangles from corners
  V0=(row,col), V1=(row,col+1), V2=(row+1,col), V3=(row+1,col+1) as (V0,V1,V3) and (V0,V3,V2): a fixed V0-V3 diagonal,
  the same in every cell. **[Local 5.8.3]** `Engine/Source/Runtime/Experimental/Chaos/Public/Chaos/HeightField.h:117-157`.
  The landscape collision component still has a per-quad `QF_EdgeTurned` flag ("This quad's diagonal has been turned")
  **[Local 5.8.3]** `LandscapeHeightfieldCollisionComponent.h:191`; whether anything sets it in 5.8 could not be checked
  (no `.cpp` in the launcher install).
- **simcore's split** is "along its (c+1, r)-(c, r+1) diagonal" [Repo] `simcore/include/simcore/terrain_map.hpp:22,377-382`.
  Unreal's frame is X = north, Y = east (CLAUDE.md). If the heightfield's columns run along Unreal X, the Chaos diagonal
  is (e,n)-(e+1,n+1) in map terms, **the opposite of simcore's**. Inferred, not tested: settle it with a trace test
  (section 8). Either simcore changes its split or the landscape is mirrored/rotated.
- **Rendering:** the landscape's render index buffer is built in `LandscapeRender.cpp`, not in the launcher install, so
  its diagonal is unverified. Rendered LODs blend between levels, so "the rendered surface can sit higher or lower than
  the LOD 0 height", and traces hit collision, not the blend (community thread, Sept 2025, linked to 5.6 docs).
  https://forums.unrealengine.com/t/landscape-mesh-reduction-and-line-traces-against-landscape/2655487
- `GetHeightAtLocation` once interpolated "from the wrong triangle"; UE-264921 is listed as fixed for 5.6 (search
  snippet; the issue page answered 403). https://issues.unrealengine.com/issue/UE-264921
- **What exact equality would take (design):**
  1. One uniform spacing for the whole landscape, so simcore's nested grids become one grid at that spacing.
  2. simcore reads the same uint16 heightmap the bake wrote (heights quantised to Z scale / 128 cm), not its doubles.
  3. The same diagonal.
  4. Equality defined as LOD0 plus screen-space error, as `TERRAIN_GEOMETRY` 2.1 already does for Nanite.
  This is the same contract SAM3D keeps today, moved from simcore's grid to the landscape's grid.

### 1.4 Things that make it hard or impossible

- **No runtime creation in a packaged game**: `Import` and the render-target imports are editor-only (above). Every map
  must be baked in the editor before play. Today simcore builds the ground at load from the map JSON; a changed map
  would need a re-bake (a hash manifest, `TERRAIN_GEOMETRY` 2.4).
- **One resolution per landscape**: simcore's 10 m and 20 m detail areas cannot be finer inside a coarser landscape.
- **The 150 km far ground** stays a mesh either way (a 150 km landscape at a useful spacing is beyond the sizes above).
- **Runtime height changes** (craters, earthworks): "landscapes aren't designed for runtime changes" (community, same
  2025 thread). Not needed today.
- Nothing found makes it **impossible**. The costs are the editor-only bake, the GPU edit-layer merge in commandlets,
  and matching simcore to the landscape's grid.

---------------------------------------------------------------------------------------------------------------------------

## 2. What a Landscape unlocks, and what also works on a mesh ground

| Feature | On Landscape | On a non-Landscape mesh ground | Source |
|---|---|---|---|
| **Landscape Grass Type** (GPU grass from layer weights) | Yes. Density is "Instances per 10 square meters" (header comment), jittered grid or Halton, start/end cull distances, `bCastDynamicShadow`. | **No**: "Currently, the Grass system will only work with the Landscape Terrain Actor." | [Local 5.8.3] `Landscape/Classes/LandscapeGrassType.h:43-72,140-142`; https://dev.epicgames.com/documentation/en-us/unreal-engine/grass-quick-start-in-unreal-engine |
| **Grass by PCG** | GPU grass-map sampling of a landscape. | **Yes**, through PCG points and the GPU Static Mesh Spawner. 5.8: "PCG GPU runtime scatter is now within the same performance range as the landscape GPU grass system, and was stress-tested". | https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes ; GPU doc below |
| **Runtime Virtual Texturing** | Native writer. | Static meshes and ISMs write; PMC and DynamicMeshComponent do not; Nanite writing unresolved. | `TERRAIN_GROUND_MATERIAL` section 4 |
| **Auto-materials** (slope, height, layer rules) | Usually built with landscape layer-blend nodes. | Same maths on any material from world position and normal; SAM3D's mask design already does this. | `TERRAIN_GROUND_MATERIAL` section 7 |
| **PCG sampling** | Get Landscape Data; height by collision upload, RVT or the Generate Landscape Textures node. | World Ray Hit / Surface Sampler need collision (reported broken on runtime geometry in 5.7.3); the **Mesh Sampler** needs a Static or Skeletal Mesh **asset** ("Soft Object Path to the mesh to sample from"); **texture data** can be initialised from any `UTexture` or a pooled render target in C++, with deferred GPU readback in 5.8; or a **custom C++ node** emits points from any source. | https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine ; [Local 5.8.3] `Plugins/PCGInterops/PCGGeometryScriptInterop/.../PCGMeshSampler.h`, `Plugins/PCG/Source/PCG/Public/Data/PCGTextureData.h:189-221`; 5.7.3 report in `TERRAIN_GEOMETRY` section 4 |
| **Water plugin** (rivers and lakes carving) | Water bodies carve "via a Landscape Brush, which only edits the layer when Edit Layers is on"; needs a Water Brush Manager. Plugin is **Experimental**. | Water bodies render, but carving is a landscape feature. SAM3D carves its rivers in simcore already. | https://dev.epicgames.com/documentation/unreal-engine/water-body-actors-in-unreal-engine ; [Local 5.8.3] `Plugins/Experimental/Water/Water.uplugin` |
| **Landscape splines** (roads deform and paint) | Yes; C++ only to build (1.1). | SAM3D's roads are simcore strips already. | above |
| **Landmass brushes** | Blueprint brushes on edit layers. Plugin is **Beta** in 5.8. | No. | [Local 5.8.3] `Plugins/Experimental/Landmass/Landmass.uplugin` |
| **Landscape Patch** | "Components that can be attached to meshes to affect the landscape" (e.g. flatten under a site). Editor plugin, no maturity flag. 5.8.3 fixed its reinitialise functions. | No. | [Local 5.8.3] `Plugins/Editor/LandscapePatch/LandscapePatch.uplugin`; https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315 |
| **Nanite, Lumen cards, distance fields, VSM caching** | Yes (Nanite landscape above). | Yes for a **baked** static-mesh tile; not for PMC or runtime fast-built meshes. | `TERRAIN_GEOMETRY` 1.6 |

So a Landscape's unique gains are Landscape Grass and the hand-authoring tools (water carving, Landmass, Patch,
splines). SAM3D authors nothing by hand: its rivers, roads and sites come from simcore.

---------------------------------------------------------------------------------------------------------------------------

## 3. PCG in 5.6-5.8

### 3.1 Status

| Part | Status | Source |
|---|---|---|
| PCG framework | **Production-ready since 5.7** ("Epic now considers the PCG framework suitable for use in final projects"). Not marked Beta or Experimental in 5.8.3. | https://digitalproduction.com/2025/11/12/unreal-engine-5-7-foliage-pcg-and-in-editor-ai/ ; [Local 5.8.3] `Plugins/PCG/PCG.uplugin` |
| GPU execution | **Beta** in the 5.8 docs; GPU nodes: Attribute Partition, Copy Points, Cull Points Outside Actor Bounds, Custom HLSL, Data Count, Normal to Density, Static Mesh Spawner, Transform Points. | https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine |
| GPU Static Mesh Spawner | "The GPU implementation is Experimental"; instances live only in GPU memory; no collision, physics, navigation, ray tracing, distance-field lighting, baked lighting or HLOD. Readback to the CPU exists (yellow badge, CPU cost). | same page |
| Runtime generation | Generate at Runtime; cells within each grid level's Generation Radius of a generation source (a `PCGGenerationSourceComponent` on any actor, or streaming sources) are generated, and cleaned past the radius times the Cleanup Radius Multiplier. Hierarchical generation needs partitioned generation. | https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine ; https://dev.epicgames.com/documentation/unreal-engine/runtime-hierarchical-generation |
| Actor-less output | 5.8: "Actor/component-less runtime generation"; FastGeo interop plugin is **Experimental**. | 5.8 release notes; [Local 5.8.3] `Plugins/Experimental/PCGInterops/PCGFastGeoInterop` |
| Builder commandlet | `-run=WorldPartitionBuilderCommandlet -Builder=PCGWorldPartitionBuilder`; 5.8 adds an "Editor only actor used to limit the bounds of generation when using the PCG Builder commandlet". | `TERRAIN_GEOMETRY` 2.3; 5.8 release notes (offset 200000) |
| Biome Core / Biome Sample | **Experimental** (both plugins; v0.2). | [Local 5.8.3] `Plugins/Experimental/PCGBiomeCore/PCGBiomeCore.uplugin`, `PCGBiomeSample.uplugin` |
| Python Interop, Geometry Script Interop, External Data Interop | **Beta**. External Data Interop's only loader is Alembic. | [Local 5.8.3] `Plugins/PCGInterops/*/*.uplugin`, `PCGExternalDataInterop/.../PCGLoadAlembicElement.h` |
| PCG Toolset | **Experimental**, editor-only: "A collection of tools allowing the assistant to create and modify PCG graphs". | [Local 5.8.3] `Plugins/Experimental/Toolsets/PCGToolset/PCGToolset.uplugin` |

### 3.2 Biome Core

- "A data-driven biome creation tool made of native PCG Framework nodes, graphs"; biome asset properties are held in
  "a local attribute set table ... built from all data assets references in the biome actor"; "Biomes can be spatially
  defined from volumes, splines, and textures"; "Multiple Biome layering and priority sorting"; the Biome Setup actor
  "has been deprecated and must be replaced with Biome Texture volumes"; it spawns "on landscape and meshes" with GPU
  hierarchical generation; Epic advises copying the plugin before production use. The page is labelled 5.8 but says the
  major changes came in 5.6.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-overview-guide-in-unreal-engine
- The biome cache is a "3D point grid structure used to store the biomes in the world".
  https://dev.epicgames.com/documentation/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-glossary-in-unreal-engine?application_version=5.5
- For SAM3D (design): a Biome Texture volume driven by a texture written from simcore's 25 m cover raster could choose
  the ground clutter per class. But Biome Core places its own points; anything that blocks sight must still come from
  simcore (section 7).

### 3.3 Reading external data

- **Data Tables**: a PCG node loads them (async by default) **[Local 5.8.3]** `Plugins/PCG/Source/PCG/Public/Elements/IO/PCGDataTableElement.h`,
  `PCGDataTableRowToParamData.h`. Data Tables import from CSV or JSON in the editor (5.8 fixed JSON import losing
  instanced-struct data: 5.8 release notes, offset 200000).
- **PCG Data Assets**: Load and Save nodes **[Local 5.8.3]** `Elements/IO/PCGLoadAssetElement.h`, `PCGSaveAssetElement.h`.
- **JSON/CSV files directly**: PCG 5.8 **exports** to JSON, CSV and binary (`PCGSaveDataViewElement.h`,
  `PCGExportSelectedAttributes.h`, `Data/DataView/PCGDataViewCSVConverter.h`); no built-in node that reads an arbitrary
  JSON or CSV file at runtime was found.
- **Textures**: Get Texture Data / Texture Sampler, Texture2DArray data **[Local 5.8.3]** `Elements/PCGTextureSampler.h`,
  `PCGGetTexture2DArrayData.h`; `UPCGTextureData::Initialize(UTexture*, ...)` and an overload taking a pooled render
  target; 5.8: "CPU readback is now always deferred" (`Data/PCGTextureData.h:189-221`). This answers part of
  `TERRAIN_GEOMETRY` gap "whether PCG can take a transient runtime texture": the C++ API takes any `UTexture`.
  **Untested.**
- **Custom C++ nodes**: subclass `UPCGSettings` and an `IPCGElement` whose `ExecuteInternal` "will be called until it
  returns true". https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PCG/IPCGElement . A node in the
  SAM3D module can call simcore directly and emit points (design).
- **Graphs by script**: `UPCGGraph::AddNodeOfType`, `AddNodeInstance`, `AddNodeCopy`, `AddEdge`, `RemoveNode` are
  BlueprintCallable, so Python in a commandlet can build graphs **[Local 5.8.3]** `Plugins/PCG/Source/PCG/Public/PCGGraph.h:453-477`.

### 3.4 Spawning at scale

- Static Mesh Spawner (CPU, ISM/HISM components; 5.8 "Improved ISM re-use logic when spawning large sets of static
  meshes") and the GPU spawner (above). Skinned Mesh Spawner for anim-bank skinned meshes; its GPU path is commented
  "Disabled pending testing" **[Local 5.8.3]** `Elements/PCGSkinnedMeshSpawner.h:32`.
- Nanite scene limit 16 million instances, ISM vs HISM advice: `TERRAIN_GEOMETRY` 3.1.
- **Performance numbers**: Epic gives none on the GPU page ("qualitative guidance" only). The one public claim is the 5.8
  note that GPU scatter is in the landscape grass range. No Unreal Fest PCG talk with figures was found (the Orlando 2025
  urban PCG session recap has none: https://www.pugetsystems.com/blog/2025/06/13/unreal-fest-2025-highlights-from-orlando/).
  Measure it ourselves (section 8).

### 3.5 Determinism and networking

- Same seed, same graph, same inputs: same points on one machine (community, 2023:
  https://forums.unrealengine.com/t/procedural-content-generation-multiplayer-and-saving-a-proc-gen-layout/1163951).
- **[Epic staff] Julien_LHeureux, 14 May 2026**: hardware differences may cause minor variation, generally acceptable for
  static objects; dynamic or gameplay actors must be replicated and runtime generation must also run on the server; the
  Get Execution Context node detects the server. Also: no native read across partitions at the same grid level
  ("it would entail an ordering in generation"); "seed large, decide small" and own each object by its centre; a cell
  should be at least twice the size of what is placed in it.
  https://forums.unrealengine.com/t/cross-partition-coherent-generation-in-pcg-with-hierarchical-runtime-generation/2728364
- For SAM3D (design): PCG output must be presentation only (grass, stubble, low shrubs below any sight-line obstacle
  height). If PCG points come from simcore through a custom node, they are as deterministic as simcore itself.

### 3.6 The Electric Dreams sample

- "A resource intensive, 4 kilometer by 4 kilometer World Partition level with streaming disabled", built on a
  landscape, with PCG forests, a procedural river bed and assemblies; UE 5.2+; DX12.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/electric-dreams-environment-in-unreal-engine . The Fab
  listing restricts use to Unreal-based products.
  https://www.fab.com/listings/d79688f5-29be-4fb2-a650-2d4a813f5306 . Useful to read graphs; its content cannot go in the
  public repo (`TERRAIN_VEGETATION` section 4).

### 3.7 Known limits (collected)

- GPU instances: no collision, RT, distance fields, navigation (above).
- Surface Sampler on runtime-created geometry generated 0 points in 5.7.3 (`TERRAIN_GEOMETRY` section 4).
- No cross-partition reads (Epic staff, above).
- 5.8.2 and 5.8.3 fixed PCG race conditions and crashes (grid linkage race, BP point processor race, attribute drift):
  https://forums.unrealengine.com/t/5-8-2-hotfix-released/2746335 , https://forums.unrealengine.com/t/5-8-3-hotfix-released/2833315 .
  5.8.4 lists a PCG Editor Mode paint tool known issue (UE-401568): https://forums.unrealengine.com/t/5-8-4-hotfix-released/2842775 .

---------------------------------------------------------------------------------------------------------------------------

## 4. Vegetation

### 4.1 Nanite Foliage and Megaplants (5.7-5.8, Experimental)

- Enable "Project Settings > Rendering ... Nanite Foliage" (restart); the Dynamic Wind plugin; wind drives an "Instanced
  Skinned Mesh Component" through its Transform Provider; assemblies hold "up to 65k instances of other meshes" and "work
  with both static and skeletal mesh assets"; disabled skinned instances render "like a fixed function static mesh (in
  bind pose)"; `r.Skinning.DefaultAnimationMinScreenSize` (default 0.1) stops skinning by screen size; labelled
  Experimental. https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-foliage (5.8)
- **Placing Megaplants without exporting them (new finding):** `UInstancedSkinnedMeshComponent::AddInstance(Transform,
  AnimationIndex, bWorldSpace)` and `AddInstances(Transforms, AnimationIndices, ...)` are BlueprintCallable
  **[Local 5.8.3]** `Engine/Source/Runtime/Engine/Classes/Components/InstancedSkinnedMeshComponent.h:212-217`. The PCG
  Skinned Mesh Spawner uses anim banks **[Local 5.8.3]** `PCGSkinnedMeshSpawner.h`. So SAM3D could put Megaplants at
  simcore's tree positions from C++, as it does with HISM today. **Untested**: whether a Megaplants asset drops straight
  into that component, and the per-instance custom data path for autumn colour.
- Epic staff (Petrockets, Jan 2026): character interaction "Not yet"; simple capsule collision works but does not move
  with the wind; placement is mainly by PCG in practice.
  https://forums.unrealengine.com/t/ue5-7-nanite-foliage-character-interaction-support-and-future-direction/2698270
- Packaged-build crash with PVE/Megaplants meshes (UE-386321, marked fixed for 5.8.2): **not in the 5.8.2, 5.8.3 or 5.8.4
  hotfix notes** (5.8.2 only lists "Fixed issue where Nanite Voxel Foliage wouldn't render on some platforms").
  https://forums.unrealengine.com/t/5-8-2-hotfix-released/2746335 . A Shipping test stays mandatory
  (`TERRAIN_VEGETATION` 3.1).

### 4.2 Procedural Vegetation Editor (PVE)

- Experimental; 5.8 adds a Growth node, seed generation, skeleton extraction from a mesh or image, grafting, carving,
  trimming and a trunk material compositor (5.8 release notes). Export as static or skeletal mesh, Nanite Foliage on by
  default, no LOD option (`TERRAIN_VEGETATION` 2.3).

### 4.3 Impostors, the Procedural Foliage Spawner, instancing

- Impostor Baker (Experimental folder, no maturity flag in its descriptor): `TERRAIN_GEOMETRY` 3.3.
- Procedural Foliage Spawner: still enabled under Editor Preferences > **Experimental**; the 5.8 page uses a landscape
  but does not say one is required; every step is in the editor.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-foliage-tool-in-unreal-engine . PCG supersedes it
  for new work (my reading: it gets no 5.6-5.8 release-note entries).
- ISM/HISM, cull distances, Nanite instance limits, WPO and VSM costs: `TERRAIN_GEOMETRY` 3.1-3.6.
- Grass density practice: Landscape Grass counts "instances per 10 square meters" up to 1000 per variety, with start/end
  cull distances per quality level **[Local 5.8.3]** `LandscapeGrassType.h:43-72`; grass usually without dynamic
  shadows (Contact Shadows suffice, `TERRAIN_GEOMETRY` section 4). The same parameters suit a PCG or hand-written ring.

---------------------------------------------------------------------------------------------------------------------------

## 5. Other terrain tools

| Tool | 5.8 status | Use for SAM3D | Source |
|---|---|---|---|
| Landmass | **Beta** plugin; landscape edit-layer brushes | No (needs a Landscape; hand-authoring tool). | [Local 5.8.3] `Plugins/Experimental/Landmass/Landmass.uplugin` |
| Mesh Terrain / Mesh Partition | **Experimental**; editor-built, PCG interop; reported not to work in packaged builds in the preview | Not now (`TERRAIN_GEOMETRY` 1.5). | [Local 5.8.3] `MeshPartition.uplugin`; https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes |
| Geometry Script / DynamicMeshComponent | Geometry Script not flagged Beta or Experimental | Covered in `TERRAIN_GEOMETRY` 1.2 and 2.1 (baking Nanite tiles from Python). | [Local 5.8.3] `Plugins/Runtime/GeometryScripting/GeometryScripting.uplugin` |
| Voxel Plugin 2 | Paid ($349, sold on its own site); targets UE 5.6-5.7; the free edition found is for UE 4.22-4.24 | Excluded (free-only rule). | https://voxelpluginvitedocs.pages.dev/resources/licensing ; https://voxelplugin.gumroad.com/l/voxelpluginfree |
| World Partition + HLOD for vegetation | Instancing HLOD layers suit "imposter meshes, such as trees and foliage" | Only if SAM3D moves to World Partition (`TERRAIN_GEOMETRY` 2.3). | `TERRAIN_GEOMETRY` 2.3 |
| Virtual Heightfield Mesh | **Experimental**; renders a heightfield from an RVT heightmap; collision hidden; forum reports of blur, shadow artifacts, flicker | No: it is a rendering of a heightfield with no collision, and SAM3D's ground must be the sim's. | [Local 5.8.3] `Plugins/Experimental/VirtualHeightfieldMesh`; https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/VirtualHeightfieldMesh/UVirtualHeightfieldMeshComponent ; https://forums.unrealengine.com/t/rvt-virtual-heightfield-mesh-multiple-questions-issues/548820 |
| FastGeo Streaming | **Experimental** | Only with World Partition. | `TERRAIN_GEOMETRY` 2.3 |

---------------------------------------------------------------------------------------------------------------------------

## 6. Fab and Quixel in 2026

- **How assets arrive in 5.8**: through the Fab plugin inside the editor (the Fab plugin is in `Engine/Plugins/Fab`,
  **[Local 5.8.3]**). Quixel Bridge content moved to Fab; a forum user says the Bridge plugin "has been retired"
  (anecdotal). https://cgpress.org/archives/epic-games-adds-fab-to-its-launcher-with-quixel-bridge-features.html ;
  https://forums.unrealengine.com/t/quixel-bridge-plugin-for-ue-5-1-1/2081792
- **A 5.8 bug**: with Virtual Texturing on, Fab sets up the **non-VT** version of a Megascans asset although it downloads
  the VT base materials (15 Jul 2026, no Epic answer).
  https://forums.unrealengine.com/t/fab-will-download-non-vt-versions-of-the-quixel-megascans-assets-into-project-with-vt-enabled/2735858
- **Sign-in**: the Fab plugin needs the owner's Epic account. Claude cannot do that step; the owner adds the assets, then
  scripts take over (import settings, register entries).
- **Licence**: the Fab Standard License has Personal and Professional tiers by the buyer's revenue (over $100,000 in 12
  months is Professional); commercial use is allowed; standalone redistribution is not.
  https://dev.epicgames.com/documentation/en-us/fab/licenses-and-pricing-in-fab . Free Megascans and Megaplants are
  Fab-Standard; the consequences for the public repo (`Content/ThirdParty/`, not committed) are in `TERRAIN_VEGETATION`
  section 4. Old Bridge downloads carry the old Unreal-only licence (search summary of
  https://www.strayspark.studio/blog/quixel-to-fab-migration-indie-developer-survival-guide-2026 , not re-read).

---------------------------------------------------------------------------------------------------------------------------

## 7. Options for SAM3D

The constraint that decides everything: **the sim owns the ground and every sight-blocking object**, the sim samples
the exact drawn triangles, and all editor work is scripted.

### 7.1 The three options

| | (a) Keep the sim-owned mesh ground, add PCG/RVT/grass | (b) Bake a Landscape per map from simcore | (c) Hybrid: Landscape in the 40 km play area, mesh far ground |
|---|---|---|---|
| Ground | simcore's triangles, as now; moved from PMC to static-mesh tiles (runtime, later baked Nanite) per `TERRAIN_GEOMETRY` section 7 | Landscape at one spacing (10 m: 4033 x 4033); simcore reads the baked uint16 heights | Landscape inside, simcore mesh outside; a seam to hide at 20 km |
| Unlocks | RVT (static-mesh tiles), Nanite/DF/Lumen (baked tiles), PCG via a custom simcore node or textures, GPU grass via PCG, Megaplants via instanced skinned meshes | All of (a) plus Landscape Grass, Water carving, Landmass, Patch, landscape splines, PCG landscape nodes, Nanite landscape | (b)'s gains inside 20 km |
| Effort (estimate) | Small to medium: builds on code that exists (`FSimRuntimeMesh`, HISM path, the mask) | Large: C++ editor commandlet (spawn, `Import`, layer infos, World Partition split, edit-layer merge under `-AllowCommandletRendering`), simcore moved to one uniform grid and to quantised heights, diagonal matched, every map baked, a re-bake on every map change | Largest: (b) plus a seam and two ground systems kept consistent |
| Sim fit | Exact today; stays exact | Exact at LOD0 if the four conditions in 1.3 hold; simcore's 50 m grid and nested detail areas give way to the landscape's grid | As (b) inside; two ground contracts to test |
| Script-only fit | Good: runtime C++ plus Python-built PCG graphs and material assets | Workable but only from C++ (no Python landscape creation), GPU-dependent commandlets, a reported builder crash on big landscapes | As (b) |
| Risks | PCG GPU spawner is Experimental; Surface Sampler on runtime geometry broken in 5.7.3 (avoided by a custom node); Megaplants packaged crash unconfirmed fixed | Every map must be pre-baked (the whole-country map, the capital outside the 40 km square); LOD and Nanite landscape are simplifications; diagonal mismatch; memory of 5 m or finer for simcore | Seams, two pipelines, the capital still outside |
| What it loses | Landscape Grass and the hand tools, which SAM3D does not use | simcore's freedom to build ground from data at load; detail areas finer than the landscape | Same as (b) in the middle |

### 7.2 Recommendation

**Choose (a).** Landscape's unique gains are Landscape Grass and hand-authoring tools. 5.8's PCG GPU scatter now matches
landscape grass for performance (Epic's claim), and SAM3D authors rivers, roads and sites in simcore, not by hand. A
Landscape would make the sim follow the engine's grid, add an editor-only bake for every map, and move ground work to
editor C++ with GPU commandlets. That breaks more of "the sim owns the ground" than it buys.

Keep (b) as a fallback, to revisit if Landscape Grass or Nanite landscape tessellation become needed, or when Mesh
Terrain leaves Experimental (`TERRAIN_GEOMETRY` 1.5).

Within (a):

1. Ground tiles: as `TERRAIN_GEOMETRY` section 7 steps 3-4 (runtime static-mesh tiles now, baked Nanite tiles later).
   This also makes RVT possible (`TERRAIN_GROUND_MATERIAL` section 4).
2. Clutter (grass, stubble, low weeds): PCG with runtime hierarchical generation around the player's camera, fed by a
   custom C++ PCG node that reads simcore's cover raster and ground triangles, spawning on the GPU. Nothing PCG places may
   block a sight line.
3. Trees: positions stay simcore's. Try Megaplants through `UInstancedSkinnedMeshComponent` near the camera, with the
   static HISM path (Megascans packs or CC-BY trees) as the fallback and for distance.
4. Biome Core: only as a reference for graph layout; it is Experimental and places its own points.

---------------------------------------------------------------------------------------------------------------------------

## 8. The first spike (small, proves (a); one optional check for (b))

Part 1, PCG clutter on the sim's ground (about two days of work, estimate):

1. In the SAM3D module, a `UPCGSettings` node "Sim Cover Points": for its cell bounds, asks simcore for jittered points
   on the drawn ground (`MapGrid::SampleTri`) with the cover class and crop as attributes.
2. A Python commandlet script builds the graph (`AddNodeOfType`, `AddEdge`): Sim Cover Points, then filter by class,
   then the GPU Static Mesh Spawner with two placeholder grass meshes. Runtime hierarchical generation, a 64 m and a
   256 m grid, a generation source on the player pawn.
3. Measure with the harness in `TERRAIN_GEOMETRY` section 6 at the battery position: game-thread and GPU ms,
   instance count, generation time per cell (`pcg.RuntimeGeneration.EnableDebugOverlay 1`).
4. Pass if: every point lies on the drawn ground within 1 cm (trace test), two clients started from the same map give
   the same point set (dump and diff), and the frame cost is within budget.

Part 2, Megaplants as instances (about one day):

1. With Nanite Foliage and Dynamic Wind enabled, put 1000 English Oak instances at simcore's tree positions in the 10 m
   area through `UInstancedSkinnedMeshComponent::AddInstances`.
2. Package a Shipping build on 5.8.3 (or 5.8.4) and load the map. Pass if it neither crashes (UE-386321) nor costs more
   than the HISM trees it replaces.

Optional check for (b), only if the owner wants Landscape kept open (about one day):

- A C++ commandlet builds a 4033 x 4033, 10 m landscape from simcore heights (quantised to uint16), saves it, and
  traces 10 000 random points against simcore's `SampleTri` on the same uint16 grid. It answers the diagonal question and
  the commandlet question (`-nullrhi` or `-AllowCommandletRendering`) with numbers.

---------------------------------------------------------------------------------------------------------------------------

## 9. Could not be confirmed

- The render index buffer's diagonal for Landscape (the `.cpp` is not in the launcher install); whether `QF_EdgeTurned`
  is ever set in 5.8; the mapping of Chaos heightfield rows and columns to Unreal X and Y.
- Whether `ALandscapeProxy::Import` runs under `-nullrhi`.
- An official size limit for World Partition landscapes (only community reports from 5.1-5.3).
- Whether UE-386321 (packaged PVE/Megaplants crash) is fixed in a shipped hotfix; whether Megaplants work directly in
  `UInstancedSkinnedMeshComponent`.
- Any published PCG performance figures beyond Epic's one-line 5.8 claim.
- Whether a Biome Texture volume can take a runtime transient texture.
- Water plugin maturity beyond its descriptor (Experimental) and whether water bodies are useful without a landscape.
- The Fab EULA text itself was not re-read this pass (`TERRAIN_VEGETATION` section 4 read it).
