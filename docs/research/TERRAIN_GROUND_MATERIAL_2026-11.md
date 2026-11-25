# Texturing SAM3D's runtime-generated ground in UE 5.8: technique research

Date: 2026-11-03. Scope: how to texture the ground SAM3D draws at runtime as `UProceduralMeshComponent` tiles (no
Landscape), from simcore's 25 m land-cover raster. The questions are runtime textures, material design, Python
asset building, RVT, natural cover edges, and decals and roads. Every claim carries its source.

Kinds of source:
- **[Epic doc]**: dev.epicgames.com documentation, mostly for 5.8.
- **[Engine 5.8]**: headers and shaders in the local install `D:/UE_5.8` (a launcher build: headers and `.ush` files
  only, no engine `.cpp`). Paths are given in full.
- **[Forum]**: Epic Developer Community forum threads (community, not official, dated).
- **[Paper]**: published papers and talks.
- **[Repo]**: this repository.

Nothing was downloaded except what WebFetch cached itself. No asset files were fetched.

---

## 0. SAM3D facts that drive the design

- The ground is drawn as `UProceduralMeshComponent` tiles with vertex colours. The material is
  `/Game/SAM3D/Map/M_SimMapVertexColor`: the vertex colour goes to base colour, roughness is 0.95, specular 0.2.
  [Repo] `unreal/SAM3D/Source/SAM3D/Private/SimMapActor.cpp:625-628,831-840`, `tools/ue_build_kesmark_range.py:20-41`.
- UV0 of each ground vertex is the map's (E, N) **in kilometres**. [Repo] `SimMapActor.cpp:758,776-777`. This matters
  for precision (section 2.1).
- The land cover is a set of `CoverGrid`s. Each is cells `cell_m` (25 m) across from (west, south), with one byte of
  `Cover` per cell (Field, Forest, Marsh, Town, Military, Road, Water) and one byte of crop index (wheat, maize,
  sunflower, ploughed, pasture). [Repo] `simcore/include/simcore/scenery/map_dressing.hpp:36,74-83,284-303`.
  - The map proper is 40 km across, so 1600 x 1600 cells at 25 m. That fits one texture well under the 16384 limit
    (section 1.6).
  - Field parcels are analytic: rows at a heading drawn per 3 km square, with widths and lengths from a hash.
    [Repo] `map_dressing.hpp:252-263` (`FieldParcel`). The raster is only the 25 m sampling of those shapes.
- The scenario frame (Unreal world) and the map frame differ by an affine horizontal transform, which `MapFrameLink`
  already computes: a 2x2 matrix plus an offset. [Repo] `simcore/include/simcore/terrain_map.hpp:1985-2004`. A mask
  sampled by Unreal world position must use that transform, or be baked in the frame's axes.
- Project rendering: DX12, SM5 and SM6, Lumen, VSM, ray tracing on. `r.VirtualTextures` is **not set** in
  `DefaultEngine.ini`, so it is the engine default. Check Project Settings > Rendering > Virtual Textures before
  relying on RVT or SVT. [Repo] `unreal/SAM3D/Config/DefaultEngine.ini:8-24`. The setting is
  `r.VirtualTextures`, "Enable virtual texture support", and needs an editor restart.
  [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/RendererSettings.h:413-417`.

---

## 1. A UTexture2D from CPU data in a packaged build

### 1.1 The API in 5.8

- `UTexture2D::CreateTransient(SizeX, SizeY, EPixelFormat Format = PF_B8G8R8A8, FName Name = NAME_None,
  TConstArrayView64<uint8> ImageData = {})` is `ENGINE_API static`. The header says it "creates and initializes a new
  Texture2D ... The texture will have 1 mip level of the given size, optionally filled with the provided data."
  `CreateTransientFromImage(const FImage*)` also exists and uses the first slice only.
  [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/Texture2D.h:341-345`.
- `UTexture2D::UpdateTextureRegions(MipIndex, NumRegions, Regions, SrcPitch, SrcBpp, SrcData, DataCleanupFunc)` is
  `ENGINE_API` and **not** inside `WITH_EDITOR`, so it links in packaged games and from plugins. The header says it
  "Asynchronously update[s] a set of regions" and calls `DataCleanupFunc` to free `SrcData` and `Regions` after the
  update. [Engine 5.8] `Texture2D.h:288-298`.
  - Older wikis say it was editor-only and unexported. That was true of old UE4 and is no longer.
    https://unrealcommunity.wiki/dynamic-textures-a5iczbuy
  - `TemporarilyDisableStreaming()` is `WITH_EDITOR` only. [Engine 5.8] `Texture2D.h:300-306`.
- `UTexture2DArray::CreateTransient(SizeX, SizeY, ArraySize, Format, Name)` exists as well.
  [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/Texture2DArray.h:107-108`.

### 1.2 Settings that exist at runtime and settings that do not

- **Runtime properties:** `CompressionSettings`, `Filter` (`TF_Nearest` / `TF_Bilinear` / `TF_Trilinear` / `TF_Default`),
  `LODGroup`, `SRGB`. [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/Texture.h:1512-1573`.
  `AddressX` and `AddressY` are on `UTexture2D` (`Texture2D.h:63-67`). `NeverStream` is on `UStreamableRenderAsset`
  (`StreamableRenderAsset.h:370`). `TF_Nearest` is declared at `TextureDefines.h:487`.
- **Editor only:** `MipGenSettings` sits inside `WITH_EDITORONLY_DATA` (`Texture.h:1427`). Code that sets it will not
  compile in a game target unless it is guarded.
  - The community tutorial sets `MipGenSettings = TMGS_NoMipmaps` unguarded. That is fine in the editor only.
    https://forums.unrealengine.com/t/community-tutorial-creating-a-runtime-editable-texture-in-c/1729786
- **`TEXTUREGROUP_Pixels2D`** has the display name "2D Pixels (unfiltered)". It is the texture group for unfiltered
  data. [Engine 5.8] `TextureDefines.h:64`.
- **sRGB:** `UTexture::SRGB` defaults to on for new textures. A data mask (class indices) must have `SRGB = false`
  **before** `UpdateResource()`. Otherwise the 8-bit index is gamma-decoded on the GPU.
  - The community tutorial's later version switched to `SRGB = 0` and reads the data as linear.
    https://forums.unrealengine.com/t/community-tutorial-creating-a-runtime-editable-texture-in-c/1729786
  - The Python doc for `srgb` says it should be off when channels are masks.
    https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/Texture2DArray

### 1.3 The recipe the community uses

Source: https://forums.unrealengine.com/t/community-tutorial-creating-a-runtime-editable-texture-in-c/1729786
(posts from 2024-2025).

1. `CreateTransient(W, H, PF_B8G8R8A8)`.
2. `CompressionSettings = TC_VectorDisplacementmap` (uncompressed RGBA8), `SRGB = 0`, `Filter = TF_Nearest`.
3. `UpdateResource()`, and `AddToRoot()` (or hold it in a `UPROPERTY`).
4. Then `UpdateTextureRegions` with an `FUpdateTextureRegion2D`, pitch `W*4` and bpp 4.

Two failure modes reported in that thread:
- **Crashes** when a region struct is shared between overlapping updates. The fix is to allocate the region per update
  and free it in the cleanup lambda.
- **Pitch:** `SrcPitch` must be in bytes (width x bytes per pixel). https://forums.unrealengine.com/t/updatetextureregions-pitch/427713

### 1.4 UpdateTextureRegions or locking the bulk data

- **UpdateTextureRegions** copies on the render thread (`RHIUpdateTexture2D`). The source buffer must stay alive until
  the cleanup callback runs, so never free it on the game thread right after the call.
  https://unrealcommunity.wiki/dynamic-textures-a5iczbuy (UE4-era page; its `bFreeData` pattern is the same idea as
  5.8's `DataCleanupFunc`, `Texture2D.h:288-298`).
- **Locking the bulk data:** lock `GetPlatformData()->Mips[0].BulkData`, `Memcpy`, unlock, then `UpdateResource()`. The
  wiki warns this **recreates the RHI texture**, so it is slow and suits only one-off fills.
  https://unrealcommunity.wiki/dynamic-textures-a5iczbuy
  - A UE5 thread shows the `GetPlatformData()->Mips[0].BulkData.Lock/Unlock` then `UpdateResource` route working.
    https://forums.unrealengine.com/t/unresolved-external-symbol-ftextureplatformdata/509022
- **For SAM3D's mask, which is filled once per map load,** pass the bytes straight to `CreateTransient(...,
  ImageData)`. That puts the data in at creation (`Texture2D.h:341`). Use `UpdateTextureRegions` only for later
  edits, for example a crater, a track or a burnt field.
- **Timing caveat (UE 4.18):** `UpdateTextureRegions` had no effect until the resource existed, so `UpdateResource()`
  must have run first. https://answers.unrealengine.com/questions/735308/view.html
- **Packaged-build caveat (Android, old):** a texture edited at runtime worked in the editor but not when packaged on
  Android. The cause was not established; the poster suspected the format or the dynamic flag.
  https://forums.unrealengine.com/t/dynamic-texture-write-texel-broken-on-android/371127
  Not relevant to Windows/D3D12, but keep the format a plain one (BGRA8 / R8).

### 1.5 Mips

- `CreateTransient` makes **one mip** (`Texture2D.h:340-342`). That is right for a class-index mask: mipping indices
  is meaningless, and the shader must read them with point sampling.
- A transient texture with a full mip chain can be built by hand: add `FTexture2DMipMap`s to the platform data before
  `UpdateResource`. It is fiddly and poorly documented.
  https://forums.unrealengine.com/t/how-do-you-generate-mips-at-runtime/395354 and
  https://forums.unrealengine.com/t/ue5-2-how-to-save-custom-texture-mips/1184851
- **Alternative:** a `UTextureRenderTarget2D` has `bAutoGenerateMips` and `MipsSamplerFilter`.
  [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h:157-168`.
- **For SAM3D, a mipped "macro colour" texture is not needed at first.** The ground already carries the land cover's
  colour per vertex ([Repo] `SimMapActor.cpp` `Colour` lambda), and that is the far LOD for free.

### 1.6 Size limits and memory

- **D3D11/12 feature level 11:** Texture2D U/V up to **16384**, Texture2DArray up to **2048 slices**, **16 sampler
  slots** and **128 input-resource slots** per shader stage. Resource size is guaranteed only up to
  min(max(128 MB, 0.25 x VRAM), 2048 MB).
  https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-limits
- **UE:** `MAX_TEXTURE_MIP_COUNT = 15`, so at most 2^14 = 16384 per side.
  [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/RHI/Public/RHIDefinitions.h:261`. The current RHI's limit is
  `GetMax2DTextureDimension()` (`RHI/Public/RHIGlobals.h:990-993`). `UTexture::GetMaximumDimensionOfNonVT()` gives the
  general non-VT limit (`Texture.h:2248-2252`). `UTexture2DArray` caps at `MAX_ARRAY_SLICES 512`
  (`Texture2DArray.h:12`).
- **SAM3D numbers:**
  - The map proper at 25 m is 1600^2 cells: 2.56 MB as R8, 10.24 MB as RGBA8.
  - Even the whole 150 km far ground at 25 m (6000^2 x 4 B, 144 MB) would fit the limits, but it is not needed. The far
    ground has no cover raster and uses vertex colour.
  - Detail areas (10 m and 20 m ground) are separate `CoverGrid`s ([Repo] `map_dressing.hpp:284-312`). Give each its
    own small mask, or composite them into the map's mask at the finest cell size.
- **GC:** a transient texture referenced only from a dynamic material instance must also be held by a `UPROPERTY` (or
  rooted), or it can be collected. (The tutorial uses `AddToRoot`, thread above.)

---

## 2. Material design for a land-cover mask

### 2.1 Where the mask UV comes from: not the PMC's UVs

- **PMC UVs reach the GPU as 16-bit half floats.** The CPU stores `FVector2D`, but the scene proxy builds the vertex
  buffer with `bUseFullPrecisionUVs = false`. Reported in 2019 and still true in 2022; changing it needs a custom scene
  proxy. https://forums.unrealengine.com/t/can-someone-confirm-me-that-the-uv-on-the-uproceduralmeshcomponent-are-32-bits/449523
  - The PMC builds its buffers through `FStaticMeshVertexBuffers::InitFromDynamicVertex`, and the vertex buffer has a
    `bUseFullPrecisionUVs` flag. [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/Engine/Public/StaticMeshResources.h:337-342`,
    `Engine/Public/Rendering/StaticMeshVertexBuffer.h:418-423,528`.
  - The proxy's `.cpp` is not in the launcher install, so 5.8's behaviour could not be checked in source.
    **Verify in game.**
- **Half precision has 11 significant bits:** spacing 2^-6 between 16 and 32, and 1.0 between 1024 and 2048.
  https://en.wikipedia.org/wiki/Half-precision_floating-point_format
  - SAM3D's UV0 is (E, N) in km. At 16-32 km from the map origin it resolves only ~15.6 m, and at 128-256 km only
    ~125 m. **It is useless for a 25 m mask and for tiling.**
- **So sample by world position** (or ship a custom PMC proxy with full-precision UVs).

### 2.2 World position, LWC and precision over 40 to 150 km

- **The node's options:** the World Position node offers Absolute World Position with or without shader offsets, and
  Camera Relative World Position with or without shader offsets.
  [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/Engine/Public/Materials/MaterialExpressionWorldPosition.h:13-27`,
  and the `EPositionOrigin` enum in `Materials/MaterialExpression.h:655-662`.
- **Epic's LWC rendering doc:**
  - Coordinate nodes (WorldPosition, Object, Camera, Actor, TransformPosition) output LWC values. Most math nodes keep
    LWC types when fed one.
  - "Absolute worldspace works at LWC scale but costs performance."
  - Camera-relative (translated world) space is precise and cheaper: "it is safe to use floats when working in
    TranslatedWorldSpace".
  - Custom HLSL has `FLWCVector3` / `FDFVector3` and `DF*Demote` functions, "the preferred method of truncation".
  - Before 5.4 there was "oscillating precision as objects moved through the world".
  https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-rendering-in-unreal-engine-5
  and https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-in-unreal-engine-5
- **The shader library has precise wrap functions:** `DFFrac`, `DFFracDemote`, `DFFmodDemote`, `DFFmodByPow2Demote`
  (double-float), and `LWCFrac` / `LWCFmod`.
  [Engine 5.8] `D:/UE_5.8/Engine/Shaders/Private/DoubleFloatOperations.ush:883-933`,
  `D:/UE_5.8/Engine/Shaders/Private/LWCOperations.ush:97-135`.
  - The pixel's absolute world position is `Parameters.LWCData.AbsoluteWorldPosition` (an `FWSVector3`), read by
    `GetWorldPosition(Parameters)`. [Engine 5.8] `D:/UE_5.8/Engine/Shaders/Private/MaterialTemplate.ush:491-508,979-997`.
- **Precision arithmetic.** float32 has 24 significant bits (https://en.wikipedia.org/wiki/Single-precision_floating-point_format).
  - **Mask lookup:** 40 km = 4e6 cm, where the float spacing is 0.25-0.5 cm. At 150 km (1.5e7 cm) it is 1 cm. A plain
    float world position is good enough to find a 25 m cell anywhere on the map.
  - **High-frequency tiling is the problem.** UV = position / 2 m at 20 km is 10000, where the float spacing is
    ~0.001 UV, or ~2 texels of a 2048 texture. At 40 km it is ~4 texels.
  - **So wrap in double first:** UV = DFFracDemote(WorldPos / Period) (or the Frac node on the LWC value), then do
    float math on the wrapped value.
  - This is my calculation from the float format. The engine function `HighPrecisionWorldPosTextureSampling` exists
    for the same purpose ([Engine 5.8] `D:/UE_5.8/Engine/Content/Functions/Engine_MaterialFunctions02/HighPrecisionWorldPosTextureSampling.uasset`),
    but no documentation for it was found.
- **A mask origin as a double:** `UMaterialExpressionDoubleVectorParameter` (an `FVector4d` default) exists, and a MID
  can set it with `UMaterialInstanceDynamic::SetDoubleVectorParameterValue(FName, FVector4)`.
  [Engine 5.8] `Public/Materials/MaterialExpressionDoubleVectorParameter.h:12-50`,
  `Public/Materials/MaterialInstanceDynamic.h:117`.
  - So: mask UV = (AbsoluteWorldPos - MaskOriginD) x InvSizeCm. The LWC subtraction is exact and the result is a small
    float.
- **A community report (UE 5.4):** shaders using world position far from the origin show precision problems. The
  advice is to subtract a chunk origin.
  https://forums.unrealengine.com/t/shader-issues-with-large-world-coordinates/2227783
- **SAM3D specifics:**
  - The scenario frame differs from the map frame by `MapFrameLink`'s affine ([Repo] `terrain_map.hpp:1985-2004`).
  - **Either** (a) bake the mask on the CPU in **scenario-frame axes**: for each texel centre in the Unreal frame, call
    `FrameToMap(e, n)` and then `Cover->At()` and `CropAt()`. **Or** (b) pass the 2x2 and the offset as material
    parameters and transform in the shader.
  - (a) keeps the shader trivial. Its resampling error is at most a cell.

### 2.3 Blending 8-16 ground layers cheaply

- **Texture arrays in UE materials:** a `Texture2DArray` is sampled with a **float3 UV whose z is the slice index**:
  `Texture2DArraySample(Texture2DArray Tex, SamplerState S, float3 UV)`, plus `...SampleLevel`, `...SampleBias` and
  `...SampleGrad(Tex, S, float3 UV, float2 DDX, float2 DDY)`.
  [Engine 5.8] `D:/UE_5.8/Engine/Shaders/Private/Common.ush:336,380,529,537`.
  - The material node is `UMaterialExpressionTextureSampleParameter2DArray`.
    [Engine 5.8] `Public/Materials/MaterialExpressionTextureSampleParameter2DArray.h`.
  - **All slices must share size, format and features.** https://forums.unrealengine.com/t/texture-2d-array-support/91266?page=2
  - **The gain comes from picking slices dynamically,** not from swapping N fetches for N array fetches ("Simply
    replacing fetches of individual textures with fetches from arrays would not do anything good"). Three weight maps
    can drive a 16-slice array for about three fetches. Same thread.
  - **Texture2DArray does not use virtual texture streaming** (a UE 5.7 user report; "Convert to Virtual Texture"
    does nothing). https://forums.unrealengine.com/t/2d-texture-arrays-streaming-textures/2746771
    Ordinary array mip streaming is gated on `GSupportsTexture2DArrayStreaming` (`Texture2DArray.h:10`).
- **Sampler slots:** "Shared: Wrap" and "Shared: Clamp" sampler sources consume no sampler slot, but they take their
  filter from the World texture group. "From texture asset" uses the texture's own filter.
  [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/EngineTypes.h:283-293`.
  - D3D limits are 16 samplers and 128 SRVs per stage (MS link above).
  - Epic's landscape doc: shared samplers mean "no hard limit" on layers for most RHIs.
    https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine
  - **Consequence for the index mask:** it needs a "From texture asset" sampler with `TF_Nearest`, or a `Load()` or
    `Gather` in a Custom node. A shared sampler would bilinear-filter the indices.
  - `ETextureGatherMode` (Red/Green/Blue/Alpha) exists on the Texture Sample node.
    [Engine 5.8] `Public/Materials/MaterialExpressionTextureSample.h:18-27`.
- **The index-map approach** (Terrain3D, a Godot terrain, influenced by Witcher 3's talk):
  https://github.com/TokisanGames/Terrain3D/blob/main/doc/docs/shader_design.md
  - Each pixel reads the **4 surrounding control-map samples** and bilinearly interpolates **their weights**, with
    GPU filtering off. Weights are summed per texture ID.
  - Textures are in **albedo and normal arrays** indexed by ID.
  - About **5-20 lookups per pixel by distance** (1-4 control reads, 4-16 albedo/normal), against 12-16 for splat maps.
  - It **skips the bilinear blend when it would span one screen pixel**.
  - Because detiling breaks UV continuity, it uses `textureGrad()` with shared derivatives.
  - The talk itself is Witcher 3's "Landscape Creation and Rendering in REDengine 3" (Gollent, GDC 2014).
    https://gdcvault.com/play/1020394/contactUs
    REDengine is said to blend linearly where Terrain3D height-blends: https://github.com/TokisanGames/Terrain3D/discussions/64
- **Height blend:** blend layers by (height + weight) above (max - depth), with a band such as 0.2. Sand "fills cracks
  between them, leaving tops of stones pure." Mishkinis, 2013.
  https://www.gamedeveloper.com/programming/advanced-terrain-texture-splatting
  - UE's own versions are `HeightLerp` (A, B, Transition Phase, Height Texture, Contrast).
    https://dev.epicgames.com/documentation/en-us/unreal-engine/texturing-material-functions-in-unreal-engine
    Landscape's `LB_HeightBlend`, and its black-spot pitfall when all heights are 0:
    https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine
- **Procedural shader splatting:** Frostbite combined coarse masks with procedural shader rules instead of stored
  terrain textures. Andersson, SIGGRAPH 2007 course.
  https://www.advances.realtimerendering.com/s2007/Andersson-TerrainRendering(Siggraph07)-CourseNotes.pdf
  (bibliographic record: https://dblp.dagstuhl.de/rec/conf/siggraph/Andersson07.html). Only the abstract-level
  description was read; the PDF did not extract.
- **The simulator precedent (Arma 3):** a colour-coded surface mask per tile, matched to `layers.cfg`, with 4-6 surfaces
  per satellite/mask tile and a satellite image for distance. Users advise not blurring the mask, which creates extra
  colours at borders. https://forums.bohemia.net/forums/topic/183029-mask-image-and-layers ,
  https://community.bistudio.com/wiki/Arma_3:_Common_Terrain_Creation_Problems (2015-era forum statements).

### 2.4 Anti-tiling

- **TextureVariation** (since 4.26), at `/Engine/Functions/Engine_MaterialFunctions03/Texturing/TextureVariation`.
  - Inputs: Heightmap, UVs, Variation Scale and Levels, Heightmap Influence, Random Rotation and Scale, HQ Edge.
  - Outputs: Shifted UVs, DDX, DDY. **The samples must use MipValueMode = Derivative.**
  - It shifts UVs rather than sampling many times, so its cost is fixed. Some tiling still shows on very large surfaces.
  - https://forums.unrealengine.com/t/new-texturevariation-node-in-4-26-simple-non-tiling-materials/152671 ,
    https://forums.unrealengine.com/t/material-randomize/154369
  - Present in 5.8: `D:/UE_5.8/Engine/Content/Functions/Engine_MaterialFunctions03/Texturing/TextureVariation.uasset`
    and `TextureVariation_RotateNormals.uasset`.
- **Texture bombing:** `Texture_Bombing`, `TextureBomb_SingleSample` and their POM variants are in
  `D:/UE_5.8/Engine/Content/Functions/Engine_MaterialFunctions01/Texturing/`. No 5.x documentation was found.
- **Hex tiling:** Mikkelsen, "Practical Real-Time Hex-Tiling", JCGT 11(3), 2022. https://jcgt.org/published/0011/03/05/
  - It is a version of Heitz-Neyret without the histogram-preserving step, so it **samples the original texture**
    with no precomputation.
  - MIT demo code includes `hextiling_rws.h` "for large-scale planar or triplanar projection far from world origin".
    https://github.com/mmikk/hextile-demo
  - Normal maps should average to +Z.
  - An Unreal HLSL port is MIT (Tabasi, Ubisoft). https://www.cgchannel.com/2023/02/free-tool-hextile-for-houdini-and-unreal-engine/
    and https://github.com/proceduralit/PI_Sandbox
  - **Cost (my reading of the method):** three hex-cell samples per texture per lookup.
- **Histogram-preserving by-example noise:** Heitz and Neyret, HPG 2018. More than 20x faster than procedural-noise
  equivalents, but needs a precomputed transform texture. https://hal.archives-ouvertes.fr/LJK/hal-01824773v1
  Burley 2019 notes clipping artefacts from its Gaussian assumption. https://www.jcgt.org/published/0008/04/02/paper.pdf
- **Macro variation and distance tiling:**
  - Multiply albedo by one light-to-dark tileable texture at 3 different scales.
  - Switch to a larger tiling scale with distance. Together these "almost completely eliminate" visible tiling.
  - https://worldofleveldesign.com/categories/ue4/landscape-macro-tiling-variation.php
  - Epic's landscape doc mentions "transitioning from detail textures to larger macro textures depending on the
    distance". https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine

### 2.5 Parallax and displacement

- **POM** (the engine's ParallaxOcclusionMapping function): cost scales with Min/Max steps, roughly linearly ("16
  steps would cost approximately 16x" one step), and the height texture read dominates. UE4-era threads.
  https://forums.unrealengine.com/t/boost-your-parallax-occlusion-mapping-performance/215713 ,
  https://unrealcommunity.wiki/parallax-occlusion-mapping-kj0xwja3
- **Nanite tessellation:** it modifies **Nanite meshes** at runtime with a displacement map, including Nanite landscapes.
  Nanite is enabled on Static Meshes and Geometry Collections; procedural or runtime meshes are not listed.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine
  - The Nanite builder is a **Developer** module in 5.8 (`D:/UE_5.8/Engine/Source/Developer/NaniteBuilder`), so it is
    not in a packaged game. That is my inference from where the module lives.
  - A third-party plugin claims runtime Nanite building in its paid edition.
    https://www.fab.com/listings/bb2e4fbb-617c-41d3-aac6-e181eddf8b3b
  - **So no tessellated displacement on the PMC ground.**
- **TextureVariation with displacement or WPO** fails because DDX/DDY are invalid in the vertex shader.
  https://forums.unrealengine.com/t/new-texturevariation-node-in-4-26-simple-non-tiling-materials/152671

### 2.6 Shader cost notes

- **Custom HLSL nodes** prevent constant folding and "may use significantly more instructions" than built-in nodes.
  - A Texture Object input named `Tex` is sampled as `Texture2DSample(Tex, TexSampler, UV)`.
  - Include File Paths and Additional Defines are available.
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/custom-material-expressions-in-unreal-engine ,
    https://forums.unrealengine.com/t/unreal-hlsl-custom-material-node-texsampler-no-longer-valid/415393
- **Project `.ush` includes** need `AddShaderSourceDirectoryMapping("/Project", <ProjectDir>/Shaders)` in a module that
  loads at `PostConfigInit`. https://forums.unrealengine.com/t/virtual-shader-source-path-link-custom-shaders-shadertoy-demo-download/119996/7 ,
  https://docs.unrealengine.com/4.26/API/Runtime/RenderCore/AddShaderSourceDirectoryMapping/index.html
  - A 5.2 report says the mapping went missing during cook.
    https://forums.unrealengine.com/t/shader-source-directory-mapping-not-working-correctly-when-packaging/1829300
  - **Simplest is to keep the HLSL inline in the Custom node's `code`** (no include) until it needs sharing.
- **Stochastic (dithered) layer choice with TAA/TSR** is viable once it collapses 6-8 lookups into one; TAA hides the
  noise. https://forums.unrealengine.com/t/texture-2d-array-support/91266?page=2

---

## 3. Texture2DArray and material assets from Python in a commandlet

- **The pattern already works in this repo:**
  `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<py> -unattended -nullrhi`, with
  `unreal.MaterialEditingLibrary` and `unreal.AssetImportTask`.
  [Repo] `tools/ue_build_kesmark_range.py:1-10`, `tools/ue_make_fx_assets.py:150-200`.
- **MaterialEditingLibrary (Python 5.8):** `create_material_expression(material, class, x, y)`,
  `connect_material_expressions(from, out_name, to, in_name)`, `connect_material_property(from, out_name, property)`,
  `recompile_material`, `layout_material_expressions`, `create_material_expression_in_function`,
  `set_material_instance_texture_parameter_value`, `update_material_function`.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialEditingLibrary
- **Custom node from Python:** `unreal.MaterialExpressionCustom` has `code`, `output_type`, `inputs`
  (`Array[CustomInput]`), `additional_outputs`, `additional_defines` and `include_file_paths`.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/MaterialExpressionCustom
  `unreal.CustomInput` has `input_name`. https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/CustomInput
  Wire the inputs with `connect_material_expressions(src, '', custom, 'InputName')`.
- **Texture2DArray from Python:**
  - `unreal.Texture2DArray` exposes `source_textures` (`Array[Texture2D]`, read-write), `address_x/y/z`,
    `compression_settings`, `srgb`, `mip_gen_settings`, `virtual_texture_streaming` and `filter`.
    https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/Texture2DArray
  - `unreal.Texture2DArrayFactory` exists. https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/Texture2DArrayFactory
  - In C++, `SourceTextures` is `EditAnywhere` and editor-only. `PostEditChangeProperty` and
    `UpdateSourceFromSourceTextures()` / `CheckArrayTexturesCompatibility()` exist but are **not UFUNCTIONs**, so Python
    cannot call them. [Engine 5.8] `Texture2DArray.h:62-104`.
  - The factory's `InitialTextures` is a bare `UPROPERTY()`, not editable, so Python cannot set it.
    [Engine 5.8] `D:/UE_5.8/Engine/Source/Editor/UnrealEd/Classes/Factories/Texture2DArrayFactory.h:18-19`.
  - **The route:** create the asset with `AssetTools.create_asset(name, path, unreal.Texture2DArray,
    unreal.Texture2DArrayFactory())`, then `arr.set_editor_property('source_textures', [...])`. `set_editor_property`
    fires the pre/post change notifications ("ensuring that the pre/post change notifications are called"; notify mode
    DEFAULT, NEVER or ALWAYS), so `PostEditChangeProperty` should rebuild the array from its sources.
    https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/StructBase?application_version=5.0 ,
    https://docs.unrealengine.com/5.0/en-US/PythonAPI/class/PropertyAccessChangeNotifyMode.html
  - **Verify:** whether `FactoryCreateNew` accepts an empty `InitialTextures`, and that the rebuild runs in the
    commandlet. Check in the first script run; there is no `.cpp` locally.
- **Import settings:** setting compression on the factory reportedly does not stick. Import with `AssetImportTask`,
  then set on the texture (`set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)`)
  and save. https://forums.unrealengine.com/t/setting-compression-settings-on-newly-imported-textures/152924
  This repo's FX script does exactly that ([Repo] `tools/ue_make_fx_assets.py:185-200`).
- **Compression enums (Python 5.8):**
  - TC_MASKS "Uses the same compression as Default" (the difference is sRGB off).
  - TC_GRAYSCALE becomes uncompressed if sRGB; "Prefer BC4".
  - TC_ALPHA is single-channel compressed.
  - TC_BC7 is "High quality compressed texture, same size as BC3".
  - TC_NORMALMAP.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/TextureCompressionSettings
  - Normal maps set to TC_Normalmap have sRGB disabled. https://answers.unrealengine.com/questions/383499/view.html
  - BC5 is used for normal maps unless "Use DXT5 Normal Maps" is set.
    [Engine 5.8] `RendererSettings.h:404-408`.
- **Interchange's texture pipeline** can detect normal maps and set sRGB, compression and LOD group itself.
  https://api.unrealengine.com/INT/PythonAPI/class/InterchangeGenericTexturePipeline.html
  Setting the properties after import is the deterministic route.
- **Virtual texture streaming:**
  - Per texture: the "Virtual Texture Streaming" flag, or "Convert to Virtual Texture". Samplers must then be the
    "Virtual" types (Virtual Color, and so on).
  - VT samples cost more: an extra lookup and at least two fetches plus math.
  - Sizes must be powers of two. Anisotropic filtering is limited by Tile Border.
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/streaming-virtual-texturing-in-unreal-engine
  - The sampler type list includes the Virtual* types. [Engine 5.8] `Classes/Engine/EngineTypes.h:946-975`.
  - Arrays do not stream as VT (section 2.3), so **detail arrays use ordinary mip streaming**.

---

## 4. Runtime Virtual Texturing on non-Landscape meshes

- **What writes to an RVT:**
  - **Well suited:** Landscape and Landscape Splines, Static Meshes, Instanced Static Meshes, Foliage.
  - **Not good candidates:** skeletal, movable and animated meshes.
  - Objects render into the RVT by "an Orthographic projection in the Negative-Z direction of the RVT Volume".
  - Writers' materials use the Virtual Texture Output.
  - RVT texels are created "on demand using the GPU at runtime" and cached.
  - Streaming low mips is optional.
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine
- **ProceduralMeshComponent: no.** The PMC draws through the **dynamic** path (`GetDynamicMeshElements`), and RVT
  pages are drawn from **static** mesh batches.
  - Fixes: add `DrawStaticElements` with `bRenderToVirtualTexture` to a copied scene proxy, **or convert to a runtime
    `UStaticMesh`**.
  - The conversion worked in play and in **packaged** builds once the batch's RVT material type matched the scene's
    (Mar 2023).
  - https://forums.unrealengine.com/t/virtual-texture-and-procedural-mesh/480227 ,
    https://forums.unrealengine.com/t/can-you-write-to-rvt-from-a-procedural-mesh-component/585991
- **DynamicMeshComponent: no write** (it can read). The same RVT setup works on a StaticMeshComponent (June 2025).
  https://forums.unrealengine.com/t/can-dynamicmeshcomponent-write-to-runtime-virtual-texture/2585556
- **Nanite static meshes:** "nanite meshes cannot write to virtual textures" (5.1). Reports of failure continue to 5.6;
  one user saw it fail even with Nanite off (5.3). Unresolved.
  https://forums.unrealengine.com/t/ue5-unable-to-write-to-runtime-virtual-textures-with-material-on-static-mesh/686869
- **Runtime static meshes are supported in 5.8:**
  - `UStaticMesh::BuildFromMeshDescriptions(...)` with `FBuildMeshDescriptionsParams::bFastBuild`. The header says the
    fast path is "mandatory in non-editor builds".
    [Engine 5.8] `D:/UE_5.8/Engine/Source/Runtime/Engine/Classes/Engine/StaticMesh.h:1837-1891`.
  - The PMC plugin exports `BuildMeshDescription(UProceduralMeshComponent*)`.
    [Engine 5.8] `D:/UE_5.8/Engine/Plugins/Runtime/ProceduralMeshComponent/Source/ProceduralMeshComponent/Public/ProceduralMeshConversion.h`.
  - **So:** PMC tile, then FMeshDescription, then a non-Nanite UStaticMesh. That is the documented-by-community way into
    an RVT.
- **Runtime-added primitives and invalidation:**
  - `URuntimeVirtualTextureComponent::Invalidate(WorldBounds[, Priority])` marks pages dirty.
    https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Components/URuntimeVirtualTextureComponent/Invalidate?application_version=5.5
  - `FSceneInterface::InvalidateRuntimeVirtualTexture`.
    https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FSceneInterface/InvalidateRuntimeVirtualTexture
  - The RVT properties on any primitive are `RuntimeVirtualTextures`, `VirtualTextureRenderPassType` and the LOD and
    cull settings. [Engine 5.8] `Classes/Components/PrimitiveComponent.h:909-937`.
- **Sizing:**
  - An RVT is at most **4096 tiles** x tile size (tiles up to 1024 texels; 2 border texels against seams).
  - A Base Color/Normal/Roughness/Specular RVT uses two DXT5 layers. Pools are set under Engine > Virtual Texture
    Pool.
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-texture-memory-pools-in-unreal-engine ,
    https://dev.epicgames.com/documentation/unreal-engine/virtual-texturing-settings-and-properties-in-unreal-engine
  - Uploads are capped by `r.VT.MaxUploadsPerFrame` (8 in game). RVT does no instance culling or LOD for ISMs.
    https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine
- **Verdict for SAM3D:** RVT is an **optimisation for later**, not the first step.
  - It needs the tiles converted to non-Nanite static meshes, VT turned on, and a near-camera detail layer on top. RVT
    texel density over 40 km is limited (at 4096 x 256 texels about 4 cm/texel, but pool memory decides the real
    density).
  - The win is that the expensive 4-neighbour/height-blend shading runs once per RVT page, not every frame.

---

## 5. Natural edges between 25 m land-cover cells

Options, cheapest first:

1. **Manual bilinear of the 4 neighbouring indices with sharpened weights** (Terrain3D style: weights summed per ID,
   4 reads, interpolated by the pixel's place in the cell).
   https://github.com/TokisanGames/Terrain3D/blob/main/doc/docs/shader_design.md
   - This turns 25 m stairs into rounded steps.
   - **Height blend** then turns the soft band into material-shaped edges (soil in the cracks first).
     https://www.gamedeveloper.com/programming/advanced-terrain-texture-splatting
2. **Domain-warped lookup:** offset the mask lookup position by low-frequency noise before reading the 4 indices,
   f(p + h(p)). https://iquilezles.org/articles/warp
   - With an amplitude of ~0.3-0.5 cell (8-12 m) and a wavelength of ~50-150 m, forest, marsh and garden edges
     meander instead of following the grid. (The amplitudes are my suggestion.)
3. **Blurred weights:** per-class weight textures, blurred on the CPU and bilinear filtered.
   - Smooth, but it costs a channel per class and blurs straight field edges.
   - Arma users advise against blurring index masks (extra colours at borders).
     https://forums.bohemia.net/forums/topic/183029-mask-image-and-layers
4. **Distance-field edges:** store a signed distance to the class boundary in a low-resolution texture and threshold
   it. Edges stay crisp under heavy magnification: a 64x64 field reproduced a 4096x4096 shape.
   https://cdn.akamai.steamstatic.com/apps/valve/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf
   - simcore knows the field parcels **analytically** ([Repo] `map_dressing.hpp:252-263`). The CPU can therefore write,
     per 25 m texel, "class A, class B, signed metres to the A|B line".
   - The shader then draws **straight field edges at any heading**. That matters on the Alföld, whose collective-farm
     fields have straight, tractor-cut edges.
   - Corners where three classes meet need care.
5. **Per-class-pair edge style:** field|field straight (with a 1-3 m weedy balk at the line), field|forest warped with
   a shrub band, water|anything with a mud or reed band. A small 2D LUT texture (class x class) can select this
   cheaply. (Design suggestion.)

---

## 6. Decals and roads

- **DBuffer decals** are "enabled by default in the Project Settings for all Unreal Engine 5 projects".
  - They need the depth prepass, which costs nothing extra with VT, Nanite or Lumen.
  - Deferred Decal materials use Translucent or Alpha Composite blending. Modulate falls back. They write base colour,
    normal and roughness.
  - The receiving material's **Decal Response** picks the channels.
  - Overlapping DBuffer decals do not blend and need sorting.
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/decal-materials-in-unreal-engine
- **PMC tiles can receive decals:** `bReceivesDecals` is a `UPrimitiveComponent` property, not limited to static
  meshes. [Engine 5.8] `Classes/Components/PrimitiveComponent.h:489,2007`.
- **Mesh decals:**
  - "A mix of translucent blend mode and deferred decals": they do not write depth, and they update the G/DBuffer
    after the opaques.
  - The geometry should "hug" the surface. There is "no easily adjustable DepthBias", so push the mesh up (or use WPO).
  - Feathered edge geometry gives soft transitions.
  - They work on curving geometry and splines without projection shear. No shadows.
  - https://dev.epicgames.com/documentation/en-us/unreal-engine/mesh-decals?application_version=4.27
  - Path-tracer cost noted in the 5.8 decal page above.
- **Roads as separate strip meshes (SAM3D draws them as PMC strips with `ColourMaterial`, [Repo] `SimMapActor.cpp:948,1401`):**
  - **(a)** Keep the road strip opaque for the carriageway. Mark the **road and verge class in the ground mask** (simcore
    already has `Cover::Road`) so the ground under and beside the strip is already dirt or gravel. The edge where the
    strip meets the ground then joins like to like.
  - **(b)** For worn verges, dust and tyre tracks, use a mesh-decal strip, slightly wider than the road with feathered
    alpha, over the ground's DBuffer.
  - **(c)** The road material's UVs along a long strip hit the same **half-float UV** limit as the ground (section 2.1).
    Use world-aligned coordinates, or wrap the U per segment on the CPU.
- **Decal limits:** Mobile has no DBuffer (irrelevant here). Normal reprojection is off by default
  (`r.Decal.NormalProjectionEnabled`). https://dev.epicgames.com/documentation/en-us/unreal-engine/decal-materials-in-unreal-engine

---

## 7. Recommendations: a first ground material for SAM3D (G-mat v1)

1. **The mask texture (C++, `ASimMapActor`):**
   - One transient `UTexture2D` per cover grid (map proper and detail areas), baked **in scenario-frame axes**: texel
     centres go through `MapFrameLink::FrameToMap`, then `CoverRaster::At` and `CropAt`.
   - Format `PF_B8G8R8A8`, bytes passed to `CreateTransient(..., ImageData)`.
   - `SRGB = false`, `Filter = TF_Nearest`, `LODGroup = TEXTUREGROUP_Pixels2D`, clamp addressing, `NeverStream`, 1 mip,
     then `UpdateResource()`. Hold it in a `UPROPERTY`.
   - **R:** a ground class 0..15. Field crops are split out (wheat stubble, maize standing, maize stubble, sunflower
     stubble, ploughed, pasture), then forest floor, marsh, town garden, military hard-standing, road/verge, water bed.
   - **G:** a parcel hash for per-parcel tint and roughness.
   - **B:** furrow heading 0..255 for 0..pi, from `FieldParcel`'s square.
   - **A:** reserved for the v2 signed edge distance.
   - The map proper is 1600^2 x 4 B = 10 MB.
2. **Mask sampling (material):**
   - Pass `MaskOrigin` as a **DoubleVectorParameter** and `InvCellCm` and `SizeCells` as scalars. Set them with
     `SetDoubleVectorParameterValue` on the tile MID.
   - UV comes from **Absolute World Position minus origin** (LWC), never from the PMC's UVs.
   - Read the 4 neighbours in a Custom node with `Tex.Load()` or point samples ("From texture asset" sampler). Bilinear
     weights come from frac(cell - 0.5), after a **domain warp** of 8-12 m from a tiling noise texture.
3. **Layer textures:**
   - Three `Texture2DArray`s built by a commandlet script (`tools/ue_make_ground_arrays.py`, same pattern as
     `ue_make_fx_assets.py`):
     - `T_Ground_BaseH`: albedo RGB plus height A, BC7, sRGB on.
     - `T_Ground_N`: normal, TC_Normalmap/BC5, sRGB off.
     - `T_Ground_RAO`: roughness and AO, TC_Masks, sRGB off.
   - 2048^2 per slice, about 12 slices. Every slice of an array is the same size and format.
   - All are CC0 sources, registered in `data/scenery/assets.json` with their licence.
   - About 12 x (5.3 + 5.3 + 2.7) MB = 160 MB with mips. (My estimate: BC7 and BC5 are 1 B/texel, BC1 0.5 B/texel, and
     mips add a third.)
4. **Per-pixel flow:**
   - If all 4 indices are equal (most pixels), take the single-class path. Otherwise visit the distinct classes (at
     most 4), sample each, **height-blend** the weights, and normalise.
   - Use `Texture2DArraySampleGrad` with derivatives computed **outside** the branch.
   - Field classes rotate their UVs by the B heading, so furrows and stubble rows follow the parcel, and the normal is
     rotated to match (as `TextureVariation_RotateNormals` does).
5. **Anti-tiling, near:**
   - Wrap UVs in double first (Frac on the LWC position over a power-of-two period), then apply **TextureVariation**
     (fixed cost) to albedo, normal and RAO, with **macro variation**: a CC0 noise at ~1/40 m and ~1/400 m multiplying
     albedo by ±10-15%.
   - Upgrade to hex tiling (MIT code) only if TextureVariation still shows repeats. It costs 3x the samples.
6. **Distance:**
   - Lerp the detail tiling from ~2-4 m (near) to ~16-32 m (far) by camera distance.
   - Beyond ~1.5-2 km, take base colour from the **vertex colour** that already carries the cover's colour, and skip
     the arrays with a branch. This keeps the far ground at vertex-colour cost.
   - The far ground (500 m, 1 km and 4 km levels) keeps the vertex-colour material it has now.
7. **No POM, no tessellation in v1.** Furrows and stubble come from normal maps.
   - POM can come later, only for the ploughed class within ~30 m, with a small step count.
8. **Roads:** paint the road and verge class into the mask, keep the carriageway strips, and add a feathered
   **mesh-decal** verge and wear strip in v1.1. `bReceivesDecals` is on by default for primitives; check it in the
   tiles.
9. **Season:**
   - Swap array slices (or material-instance parameters) for 15 October 1985 (stubble, ploughed, maize standing or cut,
     trees turning) against summer.
   - The class-to-slice mapping should be data in `data/scenery/map_dressing.json`, so simcore stays the authority on
     the classes.
10. **Later (v2):**
    - Analytic straight field edges (A-channel signed distance from simcore's parcels).
    - RVT through runtime non-Nanite static meshes if GPU cost bites.
    - Optional stochastic single-class sampling under TSR.
11. **Tests:**
    - A simcore test that the baked mask equals `CoverRaster::At` at texel centres, through `MapFrameLink`.
    - A UE check script that samples the ground colour at known places against simcore's class.
    - A precision check far from the origin: a tiling texture viewed at 18-20 km from the scenario origin should show no
      swimming.

---

## 8. Gaps and things to verify in 5.8

- **Not verifiable without the engine `.cpp`:**
  - Whether 5.8's PMC proxy still forces 16-bit UVs. A full-precision proxy may have landed; test by putting E/N in UV
    and looking for banding.
  - Whether `Texture2DArrayFactory` creates an array with empty `InitialTextures`, and whether `set_editor_property`
    triggers `UpdateSourceFromSourceTextures` in a `-nullrhi` commandlet.
  - The exact HLSL the Frac node emits on an LWC input. Check with Window > HLSL Code. `LWCFrac` and `DFFracDemote`
    exist in the shader library.
- **No official 5.x pages found for:** TextureVariation's internals, HighPrecisionWorldPosTextureSampling, or Texture
  2D Array sampling in the material editor.
- **RVT and Nanite writing:** still unresolved per forum (5.1-5.6). No release-note statement found.
- **The Frostbite course PDF** did not extract as text, so only its citation is used.
