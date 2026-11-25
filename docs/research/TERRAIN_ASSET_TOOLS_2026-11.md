# Tools for making terrain and vegetation assets outside Unreal (UE 5.8)

Research for the terrain roadmap (R6, "a more realistic map"). Date: 2026-11-05. Researcher: Claude (subagent).
Question: which tools outside Unreal (Blender, tree, terrain and texture generators, scanning, AI 3D) can an AI agent
run by script to make many good-looking trees, shrubs, crops, ground textures and simple rural buildings cheaply, for the
Great Hungarian Plain in October 1985. Licences allowed: CC0, CC-BY-4.0, Fab Standard (not redistributable), own work.

Read first, not repeated here: `TERRAIN_VEGETATION_2026-11.md` (free models: Megaplants, Megascans Trees, PlantCatalog,
CC-BY picks, PVE samples, the packaged crash UE-386321) and `TERRAIN_TEXTURES_2026-11.md` (CC0 ground and building
textures, soil colour targets, water, sand, winter wheat).

How the research was done:
- Web pages, release notes, licence pages and GitHub READMEs, read on 2026-11-05. **Nothing was downloaded, installed or
  signed up for.** Prices are as the pages showed them that day, before tax.
- Local checks only (no installs): the Blender versions already on this PC (`C:\Program Files\Blender Foundation\`:
  4.0, 4.2, 5.2); a headless run of the Sapling add-on that ships bundled inside Blender 4.0 (section 1.2); the engine's
  own PVE and ImpostorBaker plugin folders under `D:/UE_5.8` (sections 1.10, 7.2); the GPU (`nvidia-smi`: RTX 3060 Ti,
  **8 GB**), which decides what AI models can run here; the project's own card tools (`tools/make_cards.py`,
  `tools/blender/build_cards.py`, `tools/blender/card_tree.py`).
- Where a page did not say something (headless mode, output licence, CLI flags), it is marked **not confirmed**.

---

## 0. The answer in brief

1. **No free tool makes a finished, game-ready, species-correct tree by script.** The free generators make good
   skeletons (trunk and branches). The leaves, the LODs and the far impostors are where quality is won, and those are
   our own work in any free route. The paid exception is SpeedTree Indie ($19/month), whose LOD and billboard tools are
   the industry's reference, but it is a GUI tool.
2. **Best free tree route for an agent:** a branch skeleton from a Blender generator run headless (Sapling Tree Gen,
   tree-gen or Modular Tree), then our own Python step that hangs **twig cards on the terminal branches** (not in a
   random crown envelope), transfers **crown-hull normals** to the cards, thins cards for LODs **with scale-up**, and an
   **impostor** baked in Unreal for the far rings. This is the current `card_tree.py` approach, made with a real branch
   structure and proper card shading.
3. **Grass, stubble, reeds and crops are cards**, made by our own scripts from CC0 atlases (ambientCG `Foliage001-008`,
   `LeafSet001-030`) or our own photographs. No scan library has October maize, sunflower or reed; we draw or photograph
   them.
4. **Terrain:** the relief must live in simcore (`terrain_map.hpp` builds the ground from noise and the map's data, and
   the line of sight reads it). External terrain tools are either non-commercial at 1K (Gaea Community, World Machine
   Basic), paid with scripting only in higher tiers, or GUI-bound. The best route is our own detail layers plus a small
   droplet-erosion pass in simcore and the Python prototype, applied to the detail only. xarray-spatial's MIT erosion
   code is a good, deterministic reference.
5. **Textures:** Material Maker (MIT, command-line export since 1.5, Jan 2026) and Blender's own procedural bakes cover
   what no scan has: drilled seedbeds, crop rows, beaver-tail roof tiles, render. AI texture models are usable for
   colour only, with care about the model's licence.
6. **Scanning:** RealityScan (free under $1M revenue, has a command line) and Meshroom (MPL-2.0) make bark and ground
   patches cheaply. Leaves are photographed flat, not scanned in 3D.
7. **AI 3D generation is poor for trees and foliage** (thin parts, no alpha cards, no wind or LOD structure) and only
   fair for buildings. It is acceptable for small props. Free tiers carry attribution or restrictions. The open models
   that make good meshes (TRELLIS.2) need 24 GB of VRAM; this PC has 8 GB.

---

## 1. Procedural tree and plant generators

### 1.1 Summary table

| Tool | Version, date | Price | Licence (tool / output) | Runs by script, headless | Leaves as alpha cards | LODs / billboards | Realism for our species |
|---|---|---|---|---|---|---|---|
| **Sapling Tree Gen** (Blender extension) | 1.0.0, 4 Nov 2026, "all new code", Blender 5.2+; 0.3.7 (Dec 2025) for 4.4-5.0 | Free | GPL-3.0 code; output is your own model | 0.3.x: **yes** (tested locally, sec. 1.2); 1.0.0: not confirmed | Yes: leaf quads (rect, hex) with UVs; you supply the texture | No | Fair: Weber-Penn style parameters, 29 presets in 1.0.0 |
| **tree-gen** (Charlie Hewitt) | GitHub, date not shown | Free | GPL-3.0 code; output "free for use without restriction in any context apart from direct sale as assets" | Python add-on; headless not confirmed | Yes (leaf shapes) | No | Fair to good: species presets (Sapling 1.0.0 reuses them) |
| **Modular Tree (MTree)** | 5.6.0, 20 Oct 2026, Blender 4.5+ | Free | Add-on GPL-3.0, core library MIT | Own node tree type; headless not confirmed | Procedural leaves with veins | No; has **Pivot Painter 2.0 export for UE5** | Good branching; leaves generic |
| **Easy Tree** (geometry nodes) | extensions.blender.org, Blender 4.5+ | Free | GPL-3.0 | Geometry-nodes modifier: scriptable in principle | Leaves not editable (review) | No | Simple |
| **EZ-Tree** (Dan Greenheck) | npm 1.1.0; article 13 Jan 2026 | Free | MIT code; bark textures CC0 (ambientCG); leaf texture licence not stated | JS library on Three.js; web app exports GLB; Node headless not confirmed | Yes: one quad or two crossed quads per leaf, alpha test | Yes: `generateLODs()` at ~40% and ~20% | Fair: ~15 presets (oak, ash, pine, aspen, bushes); 10-40k tris |
| **Proctree** (C++ port of proctree.js) | binaries 2015 | Free | BSD-3 | Yes: a dependency-free C++ library | Twig mesh with one twig texture (SnappyTree style) | No | Low: stylised |
| **Tree It** (Evolved Software) | last release July 2017 | Free | Output usable commercially (per its T&C) | Windows GUI only | Not stated | Polygon reduction slider | Dated |
| **The Grove 2.3** (F12) | 27 Feb 2026 | Starter EUR 99, Indie EUR 199, Studio EUR 799; twigs EUR 9.69 each | Paid; output licence not stated in coverage | Standalone library with Blender/Houdini add-ons; headless not confirmed | Twigs (instanced geometry) | Game LOD reduction (2.2) | **Best realism** of the cheap tools: grown, not shaped |
| **SpeedTree 10** (Unity) | Modeler 10.2.0 | Indie $19/month (revenue under $100k), Pro $899/year (under $1M); Library add-on $999/year | Commercial; "No engine-specific modeler or license is needed" | GUI; scripting not confirmed | Yes, the industry's reference | **Yes: leaf LOD with scale-up, billboards** | Very good with skill; species library is extra |
| **Houdini SideFX Labs tree tools** | Labs (free add-on to Houdini) | Houdini Indie about $299/year (third-party, Sept 2026; not confirmed on SideFX's page); Apprentice free, non-commercial | Indie: limited commercial (under $100k) | Yes: hython batch with Indie or the free Engine Indie licence | Leaf generator; Labs maps baker | **Labs Impostor Texture / Impostor Baker** | Good, with effort |
| **UE 5.8 PVE** (engine plugin) | experimental, in 5.8.3 | Free with UE | Engine content (see the earlier note) | **Export is UI-only** (sec. 1.10) | Foliage meshes | No LOD option; Nanite foliage | Good with Megaplants recipes |

### 1.2 Blender's Sapling Tree Gen

- Listing and versions: https://extensions.blender.org/add-ons/sapling-tree-gen/ and
  https://extensions.blender.org/add-ons/sapling-tree-gen/versions/ (read 2026-11-05).
  - **1.0.0, 4 Nov 2026**, Blender 5.2 LTS and newer, GPL-3.0, developer Brad-Chamberlain. The changelog says it
    "works on Blender 5.2 and later, with all new code"; trees are "live Geometry Nodes objects"; wind with or without
    bones, exportable as a skinned mesh; "Translucent Thin Wall leaves"; 29 presets, 20 new, "including tree-gen species";
    pruning to an envelope; up to twice as fast. No LOD feature is mentioned.
  - 0.3.7 (25 Dec 2025) for Blender 4.4 to 5.0; 0.3.6 (May 2024) for 4.2. Reviews report breakage on 5.0.x.
- **Local test (2026-11-05):** the Sapling that ships bundled in Blender 4.0 on this PC ran headless:
  `blender --background --factory-startup --python sap_test.py`, with `addon_utils.enable("add_curve_sapling")` and
  `bpy.ops.curve.tree_add(do_update=True, bevel=True, showLeaves=True, leafShape='rect', leaves=25, levels=3)`, returned
  `FINISHED` in 0.07 s and made a `tree` curve object (153 splines) and a `leaves` mesh (1,271 quads). So the old operator
  works without a UI. The 1.0.0 rewrite was not installed (instruction: no installs), so **whether 1.0.0's operator runs
  headless is not confirmed**; a geometry-nodes object should, but its operator may expect a 3D viewport.
- What it gives us: a branch hierarchy we can read (curve splines per level), seeded and repeatable. What it does not
  give: twig cards, a leaf atlas, LODs, game-grade topology. The bark is a beveled curve; convert to mesh with low ring
  counts.

### 1.3 tree-gen and Modular Tree

- tree-gen: https://github.com/friggog/tree-gen. "The code is under GPL-3.0 License"; "any models generated using the tool
  are free for use without restriction in any context apart from direct sale as assets". Its species presets now ship in
  Sapling 1.0.0. Blender version and last update not shown on the page.
- Modular Tree: https://extensions.blender.org/add-ons/modular-tree/ (5.6.0, 20 Oct 2026; Blender 4.5 LTS+; add-on
  GPL-3.0, core MIT; maintained fork by GoodPie of Maxime Herpin's Mtree). It has **Pivot Painter 2.0 export for UE5**
  ("only UE5 export is fully tested"), which matters for wind on static meshes. It uses its own node tree type, so a
  script must build or load a node tree; headless use not confirmed.

### 1.4 EZ-Tree

- Repo: https://github.com/dgreenheck/ez-tree (MIT, copyright 2024 Daniel Greenheck; npm 1.1.0; peer dependency
  `three >= 0.167`, no runtime dependencies). Coverage: https://www.cgchannel.com/2025/04/ez-tree-is-a-lightweight-free-open-source-tree-generator
  (28 Apr 2025), https://digitalproduction.com/2026/01/13/free-browser-based-procedural-tree-generator-released/ (13 Jan 2026).
- Leaves: each leaf a textured quad; `billboard` option for "single" or "double" (two crossed quads); `alphaTest`
  threshold. LODs: `generateLODs()` builds a `THREE.LOD` with default switches at 100 and 250 units, about 40% and 20%
  of full detail.
- Size: "typically 10k to 40k triangles"; the author says the models are "not particularly optimized" for real-time use.
- Licence of textures: bark from ambientCG (CC0); the leaf textures' licence is not stated in the LICENSE file. Replace
  them with our own.
- Script use: it is a Three.js library. Running it in Node with three's GLTFExporter is plausible but **not confirmed**
  (texture loading in Three.js normally expects a browser). Its value to us is mainly as readable MIT source for a
  branch generator and a LOD scheme, both easy to port to Python.

### 1.5 Proctree and Tree It

- Proctree C++ port: https://github.com/jarikomppa/proctree. "The generator is licensed under 3-clause BSD license";
  "has no dependencies, and was designed to be easily plugged into whatever"; editor binaries dated 2015-01-23. The
  original proctree.js (SnappyTree) draws leaves as a twig mesh with one twig texture: the cheapest card tree there is,
  and stylised.
- Tree It: https://www.cgchannel.com/2018/01/download-free-tree-generation-tool-tree-it/ (free, Windows, last release
  July 2017, OBJ export; the reviewers "could not get Tree It to export an OBJ file in their own tests"). Not
  scriptable. Skip.

### 1.6 The Grove (paid)

- 2.3: https://www.cgchannel.com/?p=174250 (27 Feb 2026): Starter EUR 99, Indie EUR 199, Studio EUR 799 (Houdini add-on
  only in Studio); twigs EUR 9.69 each; Blender 4.2+ (supports 5.0), Houdini 19.0+; exports FBX, OBJ, USD, Alembic; splits
  meshes along branches "for real-time engines". 2.2 (Apr 2025, https://www.cgchannel.com/2025/04/f12-releases-the-grove-2-2-for-blender-and-houdini/)
  added a skeleton tool and polygon reduction for game LODs.
- It grows trees from light and gravity rather than shaping them, which is why its oaks and willows look right. Output
  licence and a headless mode are **not confirmed** from the coverage read. A UESReseach robinia made with The Grove
  is already noted in the earlier vegetation note (1.67M faces, no textures): it shows The Grove's default output is far
  too heavy for us without its LOD tools.

### 1.7 SpeedTree (Unity) in 2026

- https://unity.com/products/speedtree (speedtree.com now redirects there): Indie **$19/month** for funding or revenue
  under $100k (Modeler and all games and cinema exports); Pro **$899/year** under $1M; Library $999/year add-on;
  Enterprise custom. No free tier, a 30-day trial with limited export. Version: Modeler 10.2.0. "No engine-specific
  modeler or license is needed": the old $19 UE4 edition is gone and the general Indie plan covers Unreal. UE forum
  threads in Dec 2025 and Mar 2026 discuss SpeedTree 10.1 with Nanite voxelize in 5.7 and leaf-cluster pivots on import
  (https://forums.unrealengine.com/tag/speedtree, titles only).
- LOD method (https://docs.speedtree.com/doku.php?id=lod): "reducing the number of leaves at fixed distances from the
  camera, while increasing the size of surviving leaves"; survivors "are scaled up to fill in the empty space". This is
  the method our own card LODs should copy (section 7.3).
- For SAM3D: the cheapest way to *good* trees is one or two months of SpeedTree Indie ($19-38), but each tree is made by
  hand in the GUI. An agent cannot drive it (scripting not confirmed). It is the owner's call whether to spend that.

### 1.8 Houdini and SideFX Labs

- Editions (https://www.sidefx.com/buy/, https://www.sidefx.com/faq/indie-new/): Indie is "Limited Commercial", for
  revenue under $100k and funding under $1M over 24 months; **Engine Indie** is a free non-graphical licence (up to 3)
  that runs Indie "in batch mode" (hython) or loads HDAs into Unreal. Apprentice is free, **non-commercial**, renders with
  a watermark. Indie price: $299/year or $449/two years per third parties
  (https://shade.inc/blog/sidefx-houdini-pricing-2026-fx-core-indie-license-costs), not shown on SideFX's own page.
- Labs tree tools (controller, trunk, branch generator, branch placer, leaf generator) are free with Labs
  (https://sidefxlabs.artstation.com/projects/PoLJlo, https://www.sidefx.com/tutorials/tree-generator/).
- Labs Impostor Texture ROP "supports full octahedral, 3D, fixed axis and animated impostors"
  (https://www.sidefx.com/docs/houdini/nodes/out/labs--impostor_texture.html); a Labs Impostor Baker replaces it, the old
  chain to be removed in Houdini 24.0 (https://www.sidefx.com/community/sidefx-labs-september-update/, 25 Sep 2026; the
  page was only partly readable).
- For SAM3D: Houdini Indie is the strongest scriptable pipeline (trees, impostors, terrain in one tool) at about
  $300/year, plus a steep learning curve. Apprentice output must not go into a project that might ever be commercial,
  and its files cannot move to Indie. Not recommended unless the owner wants Houdini anyway.

### 1.9 Blender geometry-nodes setups and scatter tools

- Easy Tree: https://extensions.blender.org/add-ons/easy-tree/ (GPL-3.0, Blender 4.5+, "Simple Tree Generator" node
  group; a review says the leaves are not modifiable).
- RC12's free "Stylized Fantasy Tree Generator" (CC0, 2022, https://www.cgchannel.com/2022/01/get-rc12s-free-stylized-fantasy-tree-generator-for-blender/):
  stylised, wrong look for us.
- GScatter (Graswald): a free geometry-nodes scatter add-on; an unofficial v0.12.1 patch for Blender 5.2 exists
  (https://blenderartists.org/t/graswald-gscatter-v0-12-1-fixes-for-blender-5-2/1652019). Graswald's plant library is free
  with an account under its own EULA (https://graswald.notion.site/End-User-License-Agreement-e8cac35ea7dc4240a57878ff5f9bc4a2,
  updated 18 Mar 2024; terms not read in full). A scatter tool is not needed: simcore places every instance.

### 1.10 UE 5.8 Procedural Vegetation Editor: export by script?

- The Python API exposes `unreal.PVExportParams` and `PVExportMeshType` (STATIC_MESH, SKELETAL_MESH)
  (https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/PVExportParams?application_version=5.7).
- In the local 5.8.3 plugin (`D:/UE_5.8/Engine/Plugins/Experimental/ProceduralVegetationEditor/Source`), the exporter is
  in the editor module's **Private** folder (`Private/Exporter/PVExporter.h`, `PVEditor::OnExport()`,
  `SPVExportSelectionDialog`), and no `BlueprintCallable` export function was found. So **an agent cannot export a PVE
  graph by Python** without our own editor C++ module (which cannot include private headers) or a human click.
- A 5.8 forum report: exporting a tree with custom Nanite Assemblies crashes; exporting as a Static Mesh avoids the crash
  but the assembly instances go invisible
  (https://forums.unrealengine.com/t/crash-when-exporting-a-tree-with-custom-nanite-assemblies-in-procedural-vegetation-editor/2730267).
- Conclusion: PVE stays a hand-driven source (and the earlier note's licence caution applies), not part of an agent
  pipeline.

---

## 2. Grass, crops and ground clutter

### 2.1 How good free grass and crop cards are made

The method used in shipped games, from practitioners:
- **One atlas first**, then cards unwrapped onto it; start each card from a plane of about 4 x 4 segments and pull the
  vertices so blades bend and tips curl (https://www.gormandev.com/post/camp-hangar-03-focusing-detail).
- **Edit the vertex normals** of a clump so they point out of a dome or hull over it; otherwise each card lights
  differently and flickers as it turns (same source; https://polycount.com/discussion/comment/2677665). In Blender this
  is the Data Transfer modifier's custom normals from a smooth hull (https://docs.blender.org/manual/en/latest/modeling/modifiers/edit/data_transfer.html),
  which a headless script can do; the paid "Foliage Normals" add-on automates the same idea
  (https://superhivemarket.com/products/foliage-normals, price not checked).
- **Ambient occlusion in vertex colour** (dark at the foot): almost as good as distance-field AO at less cost
  (https://polycount.com/discussion/comment/2696460).
- **Tight cut-outs**: a few extra vertices round each blade group cut overdraw, which is what costs most on dense grass
  (https://polycount.com/discussion/comment/2605313/).
- The project's `tools/make_cards.py` and `tools/blender/build_cards.py` already draw grass, straw and reed textures and
  crossed-quad meshes. What they lack is the bent multi-segment cards, hull normals and vertex-colour AO above.

### 2.2 Sources of CC0 leaf and grass images

| Source | What | Licence | Notes |
|---|---|---|---|
| **ambientCG** `Foliage001-008` | Grass and weed atlases (2019-2023), e.g. Foliage006 "strands", Foliage008 "blades, lawn" | CC0 | Found with `https://ambientcg.com/api/v2/full_json?type=Atlas&q=grass` (2026-11-05). |
| **ambientCG** `LeafSet001-030` | Leaf atlases: oak (LeafSet016 spring, LeafSet012 autumn), maple (027), autumn and dried (021, 028, 030, Dec 2025), needles (019) | CC0 | Species mostly unstated. No reed, wheat or maize atlas. |
| **Poly Haven** models | `grass_medium_01` (1.6M polys), `grass_medium_02` (1.0M), `grass_bermuda_01` (224k), `nettle_plant`, `dandelion_01`, `moss_01` | CC0 | Real geometry, not cards (API `https://api.polyhaven.com/assets?t=models&c=nature`). Useful as a source to **render our own cards from** (orthographic render to an atlas in Blender), not to place. |
| **Wikimedia Commons** CC0 photos | Robinia leaves and bark (listed in the earlier note); other species by category | CC0 / PD (check each file) | Most Commons files are CC BY-SA: excluded. |
| **Megascans on Fab** (free selection) | Wild grass, dry grass, wheat grass and other atlases | Fab Standard | Unreal only, `ThirdParty/`; not committable. |
| **Textures.com** | Free daily credits, small sizes | Own terms: no redistribution "as a competing product", account not shareable | The free credit amount is not stated in the terms (https://www.textures.com/about/terms-of-use); a 2023 thread says free credits ended (https://community.gamedev.tv/t/textures-com-no-longer-offers-free-credits/219803). Not suitable for a public repo. |

### 2.3 Blender scripts that build clumps

- Our own `build_cards.py` is the base. Extend it (no new tool needed): per card, a 4 x 4 plane bent along a curve; 3-7
  cards per clump at random yaw and lean; normals from a dome via Data Transfer; vertex colour AO by height; UVs onto
  atlas tiles chosen at random; export GLB. All plain `bpy`, runs with `blender --background`.
- No maintained free "grass clump generator" add-on worth adopting was found.

### 2.4 Crops for 15 October

- **Standing dry maize** has no free model or atlas (earlier note, section 6). Make it: a stalk (6-sided tube), 8-12
  strap leaves as bent ribbons of 3-4 segments drooping and torn, a husked ear, all on one atlas painted or photographed;
  then **row cards** for the middle distance: render a 3 m row segment from the side into an RGBA card, and place long
  strips of crossed row cards.
- **Dry sunflower**: stalk, a drooping blackened head (a shallow disc), withered leaves as cards. Same row-card trick.
- **Wheat stubble** and the **drilled winter wheat** are cards and textures (earlier texture note, section 4).

---

## 3. Terrain generators

### 3.1 What SAM3D needs, and why it limits tool choice

simcore builds the ground itself (`simcore/terrain_map.hpp`: noise layers, river beds, lakes, a `micro_m` fBm detail per
area, all seeded), and the battery's line of sight reads the same ground. So relief added only in Unreal would make the
radar see a different ground from the one drawn. Any external tool must therefore produce **data simcore loads**
(a raster per area) or be a **reference** for rules we write into simcore. Rasters would also have to be regenerated
whenever the map's data changes, which the project's "derived from the map's data at load" principle avoids.

### 3.2 Tools

| Tool | Version, date | Price, licence | Free tier limits | Script / CLI | Input heightmap, repeatable |
|---|---|---|---|---|---|
| **Gaea** (QuadSpinner) | 2.3 (May 2026); Gaea 3.0 early access due July 2026, stable "around September" (https://www.cgchannel.com/?p=176494) | Indie $99 (8K cap, revenue under $100k), Professional $199, Enterprise $299, perpetual (same source; https://quadspinner.com/FAQ) | Community: "free for non-commercial use", **export capped at 1K** | **Build Swarm** `Gaea.Build.exe`: `--resolution` 512-8192, `var:value` for exposed properties after switches, `--silent`, `--mutate##` for seed variants (https://docs.quadspinner.com/Guide/Build/Swarm.html). Which editions include it: **not confirmed** | Yes: a `filein:` file node; seeds as exposed variables |
| **World Machine** | "Dragontail Peak" | Basic free; Indie $119; Professional $299; Studio $1,999 (https://www.world-machine.com/purchase.php) | Basic: non-commercial, **1025 x 1025 max**, 4 threads | "Automation to script builds" only in **Professional** and Studio | Yes (file input); deterministic |
| **World Creator** (BiteTheBytes) | 2025.1 (23 Sep 2025); 2026.3 changed prices; **WC4 early access** with "MCP for AI developers" (https://www.bitethebytes.com/docs/world-creator/faq/purchase-and-license) | 2025.1: Indie $149 (under $50k, no scattering, 8K cap), Professional $249, Studio $499 (https://www.cgchannel.com/?p=170365); 2026.3 "cuts prices and lifts the revenue cap" (not read in full) | No free tier found | WC4's built-in MCP server lets an AI client drive the running editor; not a headless CLI | GIS import; heightmap import not confirmed |
| **Instant Terra** (Wysilab) | undated page | $149 (8K) / $199 (unlimited), revenue under $500k, both shown "Not available"; PRO on request (https://www.wysilab.com/Editions_Pricing.html) | Education only | "Automation (Python & C# API)", apparently PRO | Not checked |
| **Houdini heightfields** | Houdini 21 | Indie about $299/year; Apprentice non-commercial (sec. 1.8) | Apprentice: watermark, non-commercial | hython batch with Indie / Engine Indie | Yes, fully procedural and seeded |
| **Blender A.N.T. Landscape** | 0.2.0 (14 May 2024), Blender 4.2+, GPL-2.0+, "limited support" (https://extensions.blender.org/add-ons/antlandscape/) | Free | none | Python operators, scriptable | Noise-based mesh; it has historically shipped an erosion operator (not re-checked for 0.2.0); mesh-based, slow at map scale |
| **xarray-spatial `erode`** | stable docs (https://xarray-spatial.readthedocs.io/en/stable/_modules/xrspatial/erosion.html) | Free; the xarray-spatial project is MIT (licence file not re-read) | none | Python function: `erode(agg, iterations=50000, seed=42, params=None)`; keys inertia, capacity, deposition, erosion, evaporation, gravity, min_slope, radius, max_lifetime | Input heightmap; **deterministic on CPU** for a given seed (droplets run in sequence); the CUDA path may differ run to run |
| Hesiod, CS2 MapCreator | GitHub | open source | | Hesiod is a node GUI; CS2 MapCreator has CLI scripts but targets Cities: Skylines 2 (https://github.com/gregspruce/CS2_MapCreator) | |

### 3.3 Relevance to a flat plain

- The relief the Alföld needs is small: micro-swells of 0.5-2 m over hundreds of metres, old river channels (meander
  scars and oxbow hollows 1-3 m deep), wind-blown sand dunes 3-10 m in the Kiskunság and Nyírség, loess bluffs a few metres
  to tens of metres at terrace edges. These are **shaped features**, not the ridges and valleys that Gaea and World
  Machine are built for; their erosion nodes are tuned for mountains.
- The free tiers are useless at our scale: a 1K export over 40 km is 40 m per pixel, coarser than simcore's own detail
  levels. The paid tiers work but put a GUI-made, owner-licensed raster into the data, outside simcore's rules.
- **Recommended route (own work):**
  1. Keep the big-scale field as given. Add **band-limited detail** only (wavelengths 50-2,000 m) as rule-driven layers in
     `terrain_map.hpp`: swells as low-amplitude fBm masked by soil type; palaeochannels as ribbons from the river data's
     old courses (or a seeded meander walk near today's rivers), lowered by a smooth profile; dunes as elongated ridges
     aligned with the prevailing north-westerly wind on sand areas; bluffs as a steep step along terrace lines.
  2. If erosion is wanted, run a **droplet erosion on the detail residual only** (port the xarray-spatial algorithm,
     about 100 lines, to header-only C++ and the Python prototype; fixed seed, CPU order), then re-impose the original
     low-pass so the big scale cannot drift. Clamp the change to a few metres.
  3. Use Gaea or World Machine (if ever bought) only as a reference to tune the look, never as the source of map data.
- Not confirmed: how the paid tools' erosion behaves at sub-metre amplitudes; whether Gaea's Build Swarm is in the Indie
  edition.

---

## 4. Texture tools

### 4.1 Tools

| Tool | Version, date | Price, licence | Script use | Use for SAM3D |
|---|---|---|---|---|
| **Material Maker** (RodZill4) | 1.5 (18 Jan 2026) brought back the CLI; 1.8 (25 Oct, year not shown; after 1.7 of 14 Jul) "fixed the `--size` parameter in command-line export" (https://github.com/RodZill4/material-maker/releases, https://www.cgchannel.com/2026/01/check-out-free-material-authoring-software-material-maker-1-5/) | Free, **MIT**; built on Godot 4.5 | Command-line export of `.ptex` materials (full flag list **not confirmed** from the release notes) | Procedural, tileable PBR sets that are our own work: beaver-tail roof tiles, render and plaster, brick, drilled seedbed furrows, crop-row masks |
| **Blender procedural bake** | Blender 5.2 (installed) | Free, GPL; output is our own | Fully headless: shader nodes baked with Cycles to image textures | Same as above, and bakes from high-poly scans or Poly Haven grass to cards |
| **ArmorPaint 1.0** | 4 Sep 2026, after eight years of early access (https://www.cgchannel.com/2026/09/armorpaint-1-0-is-out-after-eight-years-in-early-access/) | Source free under zlib; binaries $19 (itch.io) | GUI; CLI not mentioned | Hand painting; its "neural nodes" (upscale, photo to PBR maps, text to image) run locally on open models |
| **ArmorLab** | listing at 0.1 (Sep 2023) | Unclear: one site says $19, others "open source" | GUI | Photo to PBR; status not confirmed. Skip. |
| **Substance 3D** (Adobe) | 2026 releases (Painter and Designer on Steam from 18 Mar 2026, ¥22,000 each perpetual) | Indie subscription about $24.99/month (retailer price, not confirmed on Adobe's page) | Designer ships command-line tools (not re-checked here); Sampler 4.4+ "Text to Texture" (Firefly) is subscription-only | The industry standard; not needed for our list |

### 4.2 AI texture generators

- **Substance Sampler "Text to Texture"** (Firefly): tileable square images, then Image to Material for normal, height,
  roughness; subscription only; Content Credentials mark the files as AI-made
  (https://www.cgchannel.com/2024/05/adobe-releases-substance-3d-sampler-4-4).
- **Local diffusion** (ComfyUI with a seamless-tiling node, https://www.runcomfy.com/comfyui-nodes/ComfyUI-seamless-tiling/SeamlessTile):
  colour textures only; normals and roughness need another step (Material Maker or Blender from the colour's
  luminance). Model licences: FLUX.1 [schnell] is Apache-2.0 (commercial use allowed); FLUX.1 [dev]'s licence forbids
  commercial use *of the model* while saying outputs may be used "for any purpose" (contested;
  https://www.promptzone.com/zuzanna_choi/flux-ai-license-key-insights-2g8d). SDXL-class models run in 8 GB; FLUX needs a
  quantised build to fit 8 GB (not tested).
- **Ubisoft CHORD** (photo to PBR maps, SIGGRAPH Asia 2025) is **research-only** ("ubisoft-machine-learning-license-research-only-copyleft",
  https://huggingface.co/Ubisoft/ubisoft-laforge-chord): excluded.
- **Copyright of AI images:** the US Copyright Office's Part 2 report (29 Jan 2025) found that "prompts alone do not
  provide sufficient human control" for authorship (https://ipwatchdog.com/2025/01/29/part-two-copyright-office-ai-report-says-creative-prompting-doesnt-constitute-authorship/).
  Such textures are usable but nobody owns them; record them in `data/scenery/assets.json` as own work "AI-assisted",
  with the model and its licence.
- **Candid view:** AI textures look plausible and are regionally wrong in detail (a "Hungarian roof" prompt gives
  generic terracotta). For roof tiles, render and seedbeds, a procedural Material Maker or Blender graph tuned against
  period photos (`TERRAIN_REFERENCES_2026-11.md`) is more controllable and is clearly our own work.

---

## 5. Photogrammetry and scanning

| Tool | Version, terms | Script use | Notes |
|---|---|---|---|
| **RealityScan** (Epic; was RealityCapture) | 2.0 (Jun 2025), 2.1 (Nov 2025, https://www.cgchannel.com/2025/11/epic-games-releases-realityscan-2-1/). Free for "individuals and small businesses that made less than $1 million USD in revenue in the past 12 months"; else $1,250/seat/year (https://www.realityscan.com/license) | **Yes**: command line with `-headless`, `-newScene`, `-addFolder`, `-align`, `-calculateNormalModel`, `-exportSelectedModel`, and `.rscmd` scripts (https://dev.epicgames.com/documentation/en-us/realityscan/command-line-operations, https://rshelp.capturingreality.com/en-US/tutorials/commandline.htm) | Needs an NVIDIA GPU for meshing; the RTX 3060 Ti qualifies. Output rights are in the EULA (not read). Installed through the Epic launcher. |
| **Meshroom** (AliceVision) | 2025.1.0 (18 Aug 2025): 24 pipeline templates, plugins, **photometric stereo** (https://github.com/alicevision/Meshroom/releases/tag/v2025.1.0, https://digitalproduction.com/2025/08/20/meshroom-2025-1-templates-plugins-and-photometric-stereo/) | Open source, MPL-2.0; has a batch command line | Slower than RealityScan; photometric stereo is useful for leaf and bark normals from a fixed camera and moving light. Users report trouble running it from `Program Files`. |
| **Polycam** | Free plan: 10 photogrammetry captures, up to 150 images each, **glTF export only**; Basic $150/year (https://poly.cam/pricing) | Cloud app; no CLI | Commercial terms are in its terms of service (not read). The earlier note's walnut scan was made in Polycam. |

Can a hobbyist scan our needs cheaply? With a phone and the PC:
- **Bark: yes.** 30-60 photos round a trunk at chest height in flat light, process in RealityScan or Meshroom, bake a
  strip to a tiling texture in Blender (colour, normal, AO). Black locust, oak, poplar and willow bark differ strongly
  and are worth doing.
- **Ground: yes.** A 2 x 2 m patch shot in a grid, as Poly Haven and ambientCG do: stubble, disced maize land, a
  drilled seedbed, a sandy track. October is the right month to shoot now (the date of this note is 5 Nov).
- **Leaves and twigs: photograph flat, not in 3D.** A backlit sheet or a flatbed scanner gives colour and alpha
  directly; a second backlit shot gives translucency; normals from photometric stereo (Meshroom) or from luminance.
  Thin, moving leaves defeat photogrammetry.
- **Whole trees, crops in rows, reeds: no.** Wind and thin parts break reconstruction; they are cards.
- Scans of our own are our own work and committable.

---

## 6. AI 3D generation in 2026

### 6.1 Services and models

| Name | Free tier and output licence | Paid | API | Runs on this PC (8 GB)? |
|---|---|---|---|---|
| **Meshy** | 100 credits/month; Free outputs **CC BY 4.0** ("commercial use allowed with attribution, you own the output"); image to 3D 20 credits (Meshy 7.1 or 6) (https://docs.meshy.ai/en/webapp/pricing) | Pro and up: private licence, full commercial use (prices on meshy.ai, not read) | Yes (API docs; plan needed not confirmed) | Cloud |
| **Tripo** | About 300 credits/month (third parties). **Licence of free output disputed**: Tripo's blog says CC BY 4.0; its help page says under ToS 5.2.1 "Tripo retains all rights" for Free users and commercial use needs a paid plan (https://www.tripo3d.ai/help/privacy-policy/how-to-use-tripo-models-commercially, 403 to our fetch; quoted from search). Treat free output as **not usable**. | About $19.90/month (third party) | Yes | Cloud |
| **Rodin / Hyper3D** (Deemos) | Free: generate and preview, pay $1.50 per download credit; commercial terms for Free not stated (https://hyper3d.ai/pricing) | Creator $30/month ($24 yearly) "unlimited export and any use"; Business $120/month with API | Business | Cloud |
| **Hunyuan3D 2.1** (Tencent) | Open weights, Tencent community licence of 13 Jun 2025 that **"does not apply in the European Union, United Kingdom and South Korea"** (mirror of the LICENSE: https://gitserver.onethingai.com/ai-models/Hunyuan3D-2.1/raw/branch/main/LICENSE) | | Self-host | Shape about 10 GB, texture about 21 GB: **no**. 2mini shape-only about 5-6 GB: yes, untextured. **Unusable if the owner lives in the EU or UK.** |
| **TRELLIS.2** (Microsoft) | **MIT**; mesh with PBR including opacity; paper 16 Dec 2025 (https://huggingface.co/microsoft/TRELLIS.2-4B) | | Self-host | **No**: "at least 24GB", "tested only on Linux". Cloud GPU rental would work. |
| **Stable Fast 3D / SPAR3D** (Stability) | Stability Community Licence: free including commercial use under $1M annual revenue (https://stability.ai/license, https://huggingface.co/stabilityai/stable-fast-3d) | Enterprise above $1M | Self-host | SF3D about 6 GB: **yes**. SPAR3D 10.5 GB, about 7 GB in low-VRAM mode: borderline. Fast, low detail. |

Reviews are mostly written by vendors or affiliates; independent tests are scarce. A common reading: Rodin and Tripo
give the cleanest topology, Meshy the best plug-ins, Hunyuan3D and TRELLIS the best raw shape among open models
(https://help.scenario.com/en/articles/comparing-generative-3d-models/, https://www.pixazo.ai/leaderboard/ai-3d-model-generation).
No benchmark for buildings was found.

### 6.2 Quality by asset kind

- **(a) Trees and foliage: poor.** Generators output one closed, textured surface. Leaves come out as lumps or a
  shrink-wrapped crown with leaves painted on; thin stems break; there are no alpha cards, no branch hierarchy for wind,
  no LOD chain. Research on image-to-3D reports that a potted plant's leaves came out well but "the thin stems fail to
  generate properly" because multi-view images disagree on thin parts, and that score-distillation methods oversmooth
  irregular structures such as bonsai (https://arxiv.org/pdf/2405.14580, https://arxiv.org/pdf/2504.01619; which paper
  holds which sentence was not re-checked); a single view also hides the branching under the leaves. TRELLIS.2's opacity channel does not change this: it is still a volume, not cards.
- **(b) Rural buildings and props: fair for props, weak for buildings.** A haystack, a cart, a well sweep, drums, a
  feeding trough come out usable at middle distance after cleanup (remesh, decimate, re-UV, re-bake). Buildings come out
  lumpy, with lighting baked into the colour, non-straight walls, wrong regional details (window proportions, the
  street-facing gable and porch of a Hungarian peasant house, the hipped "Kádár cube"), and no modular parts or LODs.
  A procedural Blender generator does these better.
- **(c) Textures:** the services' "AI texturing" re-paints a mesh; for tiling ground and wall textures see section 4.2.
- **Output hygiene:** expect triangle soup at 50k-500k triangles, holes ("Raw meshes may contain small holes", TRELLIS.2
  card), baked shadows. Budget a cleanup script per asset (Blender: voxel remesh, decimate, smart UV, bake from the
  original).

---

## 7. Impostors and LODs for foliage

### 7.1 Why decimation fails on leaves

- Foliage is many **disconnected** small quads whose outline lives in the **alpha texture**, not the geometry. An
  edge-collapse decimator cannot merge separate leaves, so it deletes them or collapses them to slivers, and its error
  measure does not see the loss of covered area. The crown thins and goes see-through at distance; with heavy targets
  the leaves weld into solid blobs, as seen with GV Free Shrubs.
- Simplygon, the commercial reducer, says the same: reduction to about 400 triangles "does not look that good... the
  main issue is that we are loosing density"; it recommends triangle reduction for the near LODs and **billboard clouds**
  for the far ones (https://simplygon.com/posts/04a89922-2cd3-4ea6-84af-19cf453c05c2,
  https://www.simplygon.com/features/vegetationoptimization).
- Epic's own fix history: Nanite first added "surface area back into surviving triangles" (Preserve Area), which users
  found unreliable at some angles (https://forums.unrealengine.com/t/nanite-foliage-disappear-on-certain-angles/761590),
  and 5.8's Nanite Foliage moves to **voxels** at distance, still experimental
  (https://dev.epicgames.com/documentation/unreal-engine/nanite-foliage).

### 7.2 What works

1. **Card thinning with scale-up** (SpeedTree's method, sec. 1.7): each LOD keeps a seeded subset of cards and scales the
   survivors so total card area stays about constant. Trivial to script in Blender on our own card trees.
2. **Cluster cards** for the middle LOD: render groups of twigs (or a whole branch) from the full tree into a new
   atlas, and replace each group by one or two larger cards: a home-made billboard cloud.
3. **Impostors** for the far rings, baked from the full tree:
   - **UE ImpostorBaker** (on this PC: `D:/UE_5.8/Engine/Plugins/Experimental/ImpostorBaker`, Blueprint and Editor
     Utility Widget content, `BP_Generate_ImpostorSprites`, `EUW_Generate_Impostor_using_Preset`, `HemiOctahedra`).
     Epic: a batch renderer since 5.5; Fortnite uses upper-hemisphere impostors for trees when Nanite is off; foliage
     example with 12 x 12 = 144 frames (https://dev.epicgames.com/documentation/en-us/unreal-engine/impostor-baker-plugin-in-unreal-engine).
     Driving it from Python (spawn the Blueprint, call its functions) is **not confirmed**; it needs a rendering editor,
     not `-nullrhi`. Forum users on 5.5 reported a broken generate Blueprint (https://forums.unrealengine.com/t/impostor-baker-plugin-broken/2355111).
   - **Blender**: no maintained free octahedral baker found. Pandrodor's "Blender-Impostor-Baker" (GPL-3.0, two commits,
     https://github.com/Pandrodor/Blender-Impostor-Baker) and "Instant Impostors" (paid, EEVEE-only, "cannot export
     impostors to other game engines", https://www.blendermarket.com/products/instant-impostors--one-click-impostor-generation-eevee)
     do not fit. Writing our own hemi-octahedral render in Blender is easy; matching the layout that Unreal's impostor
     material expects is the hard part. Bake in Unreal instead.
   - **Houdini Labs** Impostor Texture / Impostor Baker (sec. 1.8), if Houdini is ever bought.
4. Keep **Nanite with voxels** as the near path only once Nanite Foliage leaves experimental status and the packaged
   build is proven (earlier note, 3.1).

---

## 8. A recommended asset pipeline for SAM3D

Principles: every step an agent can run (`blender --background --python`, Python, Unreal Python/commandlets); every
output our own work or CC0/CC-BY so it can be committed; Fab-Standard only as an optional overlay in `ThirdParty/`.
The common back end for all vegetation: `tools/prepare_model.py` (unit frame), LODs by card thinning, impostors baked in
Unreal, HISM drawing as today.

| Asset kind | Best free scripted route | Quality to expect | First thing to try |
|---|---|---|---|
| **Broadleaf tree** (oak, hornbeam, field maple, black locust, willow) | Skeleton from **Sapling 1.0.0 or tree-gen presets** in Blender 5.2 headless (fall back to the bundled Sapling 0.3.x path proven on 4.0). Our script converts levels 0-2 to low-ring tubes, hangs **twig cards on terminal branches** facing outward, UVs onto a **twig atlas** cut from CC0 photos (Commons robinia, ambientCG LeafSet016/012 oak, LeafSet027 maple) or our own flat photos; hull normals by Data Transfer; vertex-colour AO; October tint in the material. LOD1-2 by card thinning with scale-up; far by UE impostor. | Good from 50 m, fair at 10-20 m, never Megascans-close. Willow and robinia (pinnate, airy) are the hardest to read right. | One oak: Sapling's oak-like preset, 2,000-4,000 tris, beside the current `card_tree.py` oak and a photo, from the battery's eye height. |
| **Conifer** (Scots and Austrian black pine on the sand) | Same skeleton route with needle-tuft cards (ambientCG LeafSet019 "needles" or own photos); or the engine's `PVE_Conifer_01` exported by hand (it packaged on 5.8.1). | Good: conifers suit cards well; plantations are seen as rows from afar. | A black pine with clumped crown top from Sapling's pine preset. |
| **Poplar** (Lombardy; hybrid plantations) | Lombardy: skeleton with near-vertical branches and a narrow pruning envelope (Sapling 1.0.0 has pruning to an envelope); hybrid: open crown in straight rows, where the **impostor** matters most. CC-BY intice184 Lombardy poplar (earlier note) as a check. | Good at distance; rows of identical trees need 3-4 variants and per-instance tint. | A Lombardy poplar with 3 variants and its impostor, in a row of 50. |
| **Shrub** (hazel, elder, blackthorn, dog rose; hedges) | `card_tree.py`'s multi-stem shrubs with denser, smaller twig cards; for field-edge **hedges, a ribbon generator** that lays crossed cards along the hedge line rather than separate bushes. Sloes (blue-black) and hips (red) as a few tinted dots in the atlas for October. | Fair near, good as hedge bands at distance. Species are told apart mostly by colour and size. | A 50 m blackthorn-rose hedge strip along a field edge. |
| **Grass and clutter** | Extend `make_cards.py` / `build_cards.py`: bent 4 x 4 cards, 3-7 per clump, dome normals, AO in vertex colour, atlases from ambientCG `Foliage001-008` and our drawn October blades; Poly Haven grass rendered to cards for variety. | Good: this is what most games do. | Swap one dry-grass card for a bent, dome-normal clump and compare at grazing light. |
| **Crop** (dry maize, dry sunflower, stubble, drilled wheat) | Own procedural plants in Blender (stalk + ribbon leaves + ear/head), then **row cards** rendered from them for 20-300 m; stubble cards exist; drilled wheat and seedbed as textures (Material Maker furrow graph over CC0 soil). | Maize near: fair; maize and sunflower fields at distance: good. | A 10 x 10 m dry maize block: 3 plant variants, then its row card. |
| **Reed** (Phragmites in marsh and canal edges) | Crossed cards with a plume, drawn or from CC0 photos; dense stands as clumps with a darker base; edge bands along water. | Good at distance; fair near. | Replace `reed_card` with a 5-card clump with plumes in October beige. |
| **Ground texture** | CC0 scans (earlier texture note) first; **own scans** (RealityScan or Meshroom, 2 x 2 m patches) for the soils where colour targets matter; **Material Maker or Blender procedural** for rows, furrows and tracks; AI colour only as a last resort (SDXL or FLUX.1 schnell, recorded as AI-assisted). | Good: textures are the easiest win. | Scan one October stubble patch and one ploughed field with a phone; process headless with RealityScan's CLI. |
| **Building** (village house, Kádár cube, barn, water tower) | **Own Blender Python generator** by type and dimensions (walls, hipped or gabled roof, porch columns, window grid, chimneys), CC0 textures (ambientCG roof tiles, plaster) plus Material Maker for beaver-tail tiles and render colours; low-poly by construction, LODs by removing trims. | Good from 100 m; fair near. Far better than AI output. | The "Kádár cube" (about 10 x 10 m, pyramid roof, three street windows) with 4 render colours. |
| **Prop** (bales, fences, wells, carts, drums, troughs) | Simple shapes procedurally (bales, posts, fences, well sweep); complex ones by **AI 3D**: Meshy free (CC BY 4.0, credit "Meshy") or Stable Fast 3D locally (Community Licence), then a Blender cleanup script; or CC-BY models. | Fair at middle distance. | One cart from a period photo through SF3D, cleaned to 3k tris. |

### 8.1 What will not look good, candidly

- **Close trees.** No free scripted route will match Megascans or SpeedTree at under 10 m. If the player stands under
  trees often, the Megascans Trees packs (via a 5.6 project) or SpeedTree Indie for a few hero trees are the honest
  answer.
- **Willow and black locust crowns.** Their fine, drooping or pinnate foliage reads as generic blobs with coarse cards.
- **AI-made trees, shrubs, crops and buildings.** Not worth trying for these.
- **Repetition.** Plantations, orchards and maize fields are regular in life; identical instances still look fake.
  Every kind needs 3-4 variants and per-instance tint and scale from simcore's seed.
- **Wind.** Static cards do not move. Vertex-colour or Pivot Painter wind (MTree exports Pivot Painter 2.0) is a
  separate task; without it, near trees look dead.
- **Terrain tools' erosion on a plain.** Mountain-tuned erosion makes gullies the Alföld does not have; keep any erosion
  to a clamped detail pass.

### 8.2 Costs in one place

| Item | Cost | Needed? |
|---|---|---|
| Blender 5.2, Sapling, tree-gen, MTree, Material Maker, Meshroom, xarray-spatial | Free | Yes |
| RealityScan | Free under $1M revenue | Useful |
| SpeedTree Indie | $19/month | Optional, for hero trees by hand |
| The Grove 2.3 | EUR 99-199 one-off | Optional, best cheap realism; not scriptable as far as confirmed |
| Gaea Indie / World Machine Pro | $99 / $299 | No (own detail layers instead) |
| Houdini Indie | about $299/year | No, unless wanted for everything |
| Meshy paid / Rodin Creator | about $20 / $30 a month | No; free tiers are enough for a few props |
| Cloud GPU for TRELLIS.2 | pay per hour | No |

### 8.3 Gaps (could not be confirmed)

- Whether Sapling 1.0.0, tree-gen and MTree run under `--background` (0.3.x Sapling does).
- EZ-Tree in Node without a browser; its leaf texture licence.
- The Grove's output licence and any headless mode; SpeedTree scripting.
- Which Gaea editions include Build Swarm; World Creator 2026.3 prices.
- Material Maker's full command-line flags.
- RealityScan's EULA on output rights; Polycam's commercial terms.
- Tripo's free-tier licence (its own pages disagree); Rodin's free-tier commercial rights.
- Driving UE ImpostorBaker from Python.
- The owner's country (it decides whether Hunyuan3D may be used at all).
