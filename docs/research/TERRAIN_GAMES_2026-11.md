# How shipped games make countryside and towns read as real: lessons for SAM3D's terrain rework

Research notes, 5 November 2026. This note continues `TERRAIN_AAA_MAPS_2026-11.md` (military sims and flight sims:
STALKER 2, Arma/DayZ, Reforger, Squad, Tarkov, Gray Zone Warfare, Hell Let Loose, Enlisted, MSFS 2020, DCS, IL-2).
It does not repeat that note. Here: Kingdom Come: Deliverance 1 and 2, Farming Simulator, Horizon Zero Dawn, Far Cry 5,
Ghost Recon Wildlands, Ghost of Tsushima and Yotei, Red Dead Redemption 2, The Witcher 3, Forza Horizon 5, MSFS 2024,
theHunter: Call of the Wild, Hunt: Showdown, Euro Truck Simulator 2, SnowRunner, and a few Central/Eastern European
games (Chernobylite, Pathologic 2, Atomic Heart, Manor Lords).

The goal is the Kesmark range (40 x 40 km of the Great Hungarian Plain on 15 October 1985) in UE 5.8, where the ground
is a simcore-generated ProceduralMeshComponent coloured per vertex, trees and buildings are HISM, and line of sight
comes from simcore's own ground and obstacle heights.

## How to read these notes

- Every claim carries its URL. Tags, as in the earlier note:
  - **[P]** read on the page itself.
  - **[S]** seen only in a search engine's summary (the page blocked automated reading or was not opened). Lower
    confidence.
  - **[SAM3D]** our inference or proposal, not a source claim.
- Nothing was downloaded, installed or signed up for. Large PDFs that the page reader refused (Guerrilla's GDC 2017
  slides) were read through SlideShare's text transcript of the same deck instead.
- Many game studios publish little about terrain. Where a source could not be found the note says so.

---

## 1. Kingdom Come: Deliverance (2018) and KCD2 (2025): Warhorse, CryEngine

**What is public is thin.** No Warhorse talk on terrain or vegetation was found (their GDC talks are on NPCs and
clothing: [S] https://gdcvault.com/play/1035718/Supporting-Thousands-of-NPCs-in). What exists:

**Terrain and references**
- Daniel Vávra (80.lv): "Our world is built around digital satellite heat map ... we bought the terrain and we are
  building on it" (probably "height map"; the page does not say); field trips "with a mobile bluescreen and taking
  pictures of herbs, plants, and flora"; team "about 70 people" at the time. [P]
  https://80.lv/articles/kingdom-come-deliverance-an-interview-with-daniel-vavra
- KCD's map is about 16 km²; KCD2's is about twice that, split into two maps. [S]
  https://www.dexerto.fr/jeux-video/taille-carte-kingdom-come-deliverance-2-1602811/
- KCD2 (Xbox Wire, senior designer Ondřej Bittner): start from real geography, since hills, valleys and rivers changed
  little; settlement layouts from **mid-19th-century military maps**, which "usually have the layout of fairly static
  villages" with individual buildings, roads and logistic points; then castle and church custodians' records; forests in
  two kinds: clearly planted areas and "more natural forests based on research about biodiversity". Fields,
  photogrammetry, tools and procedural vs hand placement are not mentioned. [P]
  https://news.xbox.com/en-us/2025/02/04/building-an-authentic-bohemia-in-kingdom-come-deliverance-2/
- A Czech university note says a student worked on "castles, chicken coops and entire villages", reconstructing how
  chicken coops and watercourses in courtyards were laid out. [S]
  https://info.zcu.cz/Jiri-Frei-is-the-new-candidate-for-Dean-of-the-Faculty-of-Health-Care-Studies/clanek.jsp?id=8016

**Vegetation and engine**
- Tech interview (lead environment artist Jakub Holík, lead programmer Tomáš Blaho): PC "could potentially handle denser
  vegetation, but we can keep only one" vegetation map across all platforms; PC gets "more objects, details, clutter"
  in level layers not exported to consoles; PC textures generated up to 4x the console resolution; Warhorse is "quite
  small" for an AAA open world. [P] http://www.gamebanshee.com/k4uw6
- Warhorse PR: CryEngine "allowed us to have stunning simulated vegetation"; people told them "we might have the
  best-looking forests ever seen in a videogame". [P]
  https://www.cryengine.com/news/why-developers-choose-cryengine-part-5
- A forum repost says the planned dense, grassy forests made CryEngine the choice and it ran test scenes faster. [S]
  https://forum.kingdomcomerpg.com/t/warhorse-studios-weekly-torch/29593/145 . Vávra later said, in a podcast reported
  in translation, that Unreal suits rocky landscapes but "when it comes to dense forests and vegetation, it struggles".
  [S] https://windowscentral.com/gaming/kingdom-come-deliverance-2-dev-witcher-4-has-a-long-road-ahead-on-unreal-engine-5
- KCD2 settings test: lowering Vegetation Quality brings "a reduction in the amount of distant vegetation" and "in
  the amount of shadows within the vegetation itself"; Object Detail "controls the rendering of plants and buildings
  in the distance". [P]
  https://en.gamegpu.com/test-gpu/rpgrolevye/kingdom-come-deliverance-2-obzor-i-sravnenie-graficheskikh-nastroek
- A KCD2 tech review: forests "with even more detail", "a much greater emphasis on shading and the play of light";
  the experimental tier's extra draw range is "barely noticeable". [P]
  https://www.gamepressure.com/editorials/kingdom-come-deliverance-2-tech-review-great-optimization-surpris/zd751
- Warhorse's material team reworked KCD1 materials in Substance Designer/Painter (an artist's ArtStation). [S]
  https://jouke.artstation.com/projects/AZXxmz

**The engine's own grass tools (CryEngine 3 docs, the base KCD started from)** [P]
https://www.cryengine.com/docs/static/engines/cryengine-3/categories/1114113/pages/1048735
- "Use On Terrain Layers": a grass asset tied to a painted terrain texture layer is scattered wherever that layer is
  painted, "a quick and dirty way to automatically cover a huge area with grass".
- Procedural vegetation "needs a memory cache of 8 MB"; use only small objects, since "big objects might appear with
  very obvious popping effects"; placement uses the pivot, so big objects overlap neighbouring layers (grass into mud).
- "Fit to Terrain" moves a patch's polygons down to the ground; "Align to Terrain" rotates the whole object to the
  normal and is wrong for big patches.
- A "Use Terrain Color" option blends grass with the ground colour under it. [S]
  https://www.cryengine.com/docs/static/engines/cryengine-5/categories/23756816/pages/24285892

**Not found:** KCD's heightmap resolution, terrain layer count, grass density, tree counts or impostor distances; how
fields and crops were made; whether vegetation was hand-painted or rule-placed.

**Take-away [SAM3D]:** KCD's realism comes from real geography plus period maps for where villages, roads and woods are,
species from field research, and dense, well-shaded forests. Its one vegetation map for every platform agrees with
Gray Zone Warfare's rule in the earlier note: sight-blocking vegetation is the same for everyone.

---

## 2. Farming Simulator 22/25 (GIANTS Engine 9/10)

Farming Simulator is the one game whose whole subject is fields at every growth state, drawn cheaply over many km².
GIANTS' own documentation pages on terrain were not found (the GDN lists only scripting, exporter, i3d and an artwork
guide: [P] https://gdn.giants-software.com/documentation.php); the concrete mechanics below come from modding forums.

**Terrain**
- FS25 maps are built at **1 m** terrain resolution, down from 2 m. [P]
  https://en.number13.de/ls25-major-graphics-and-performance-improvements-with-giants-engine-10/
- GIANTS Engine 10 adds ground deformation (the ground compresses under wheels) and denser fog. [P]
  https://farmingsimulator.wiki.gg/wiki/GIANTS_Engine/Version_10.0/Farming_Simulator_25 ; a virtual-texture system
  and projected decals were presented at FarmCon 2024 (fan notes). [S]
  https://www.modland.net/fs25-mods/news/giants-engine-10-the-new-engine-for-farming-simulator-25.html
- A blank 4x map template: weight (texture layer) files at 4096 px, GDM density maps at 8192 px. [S]
  https://pwmmapping.itch.io/fs25-blank-4x-map-for-mappers
- Map generator maps4fs (builds FS maps from OpenStreetMap): each texture layer is weight maps named
  `{texture}{n}_weight.png`; in FS25 most textures use **two** weight files (e.g. grass01, grass02) "and the game
  blends them to avoid visible repetition"; layers are produced by matching OSM tags. [S]
  https://maps4fs.gitbook.io/docs/understanding-the-basics/texture_schema
- GIANTS' artwork guide: textures 2048 px or smaller; normal maps as big as or twice the diffuse. [P]
  https://gdn.giants-software.com/documentation_content.php

**Fields: everything is a density map read by the shader or the foliage system** [P unless marked]
- The ploughing/cultivator state is a terrain detail layer with a density map of 11 channels by default. One part
  is the **field's furrow angle**, quantised to **4 directions 45° apart** and turned in the ground shader
  (`angle = ceil(angle*3-0.0001)/4*3.14159`); a mod raises it to 8 directions with one more channel.
  https://forum.giants-software.com/viewtopic.php?p=800581
- Crops and grass are a `FoliageMultiLayer` over one density map: in a posted FS19 map, `numChannels="10"`,
  `numTypeIndexChannels="5"` (the crop type), the rest the growth state, 16 foliage types (wheat, grass, bushes, cotton
  ...). A second multilayer holds weeds. Ground paint can be linked to paintable foliage (grass on a ground type).
  https://forum.giants-software.com/viewtopic.php?p=1237757
- In the same map the terrain detail layers have `viewDistance="75"` and `blendOutDistance="5"` (m). (same link)
- FS22's fruit density map grows from "10 5 5 to 12 6 6 channels" to hold more crops. [P]
  https://itch.io/blog/1045682/quick-tutorial-for-gdm-expansion-for-multifruit ; 5 type channels hold 31 crops, 6
  hold 63; every crop has 15 growth states available (many unused); wheat uses 8 plus a withered state; 6 fruit
  channels exceed the default limit of 32 foliages. [S] https://itch.io/post/13838861 ,
  https://forum.giants-software.com/viewtopic.php?p=1605478
- A foliage type defines states (`foliageState`: name such as "green middle", "harvest ready 2", height, height and
  width variance, `numBlocksPerUnit`), per-layer `cellSize`, LODs per block shape, `viewDistance`, `blendOutDistance`,
  and a `distanceTexturePath` (the crop drawn as a texture far away). The page lists the attributes but documents few.
  https://farming-simulator-mod-doc.readthedocs.io/en/latest/FS2019/XML/foliageType.html
- FS25 is said to allow "denser foliage and crops" from smaller data and more efficient rendering, and adds tyre furrows
  in fields. [P] https://en.number13.de/ls25-major-graphics-and-performance-improvements-with-giants-engine-10/

**Not found:** official numbers for density-map resolution per map size, foliage instances per m², or crop view
distances (users report foliage draw distance changing between patches: [S]
https://steamcommunity.com/app/2300320/discussions/0/505068966120561323).

**Take-away [SAM3D]:** a field is not a texture with nothing on it. It is a few bits per pixel (crop type, growth
state, furrow angle, tillage) that the ground shader and a foliage instancer both read: near, crop blocks of instanced
geometry; past a view distance, a distance texture of the same crop. That is cheap and exactly what simcore's crop
parcels can feed.

---

## 3. Horizon Zero Dawn / Forbidden West (Guerrilla, Decima): GPU procedural placement

From Jaap van Muijden's GDC 2017 deck, read as SlideShare's transcript. [P]
https://www.slideshare.net/guerrillagames/gpubased-procedural-placement-in-horizon-zero-dawn (page:
https://www.guerrilla-games.com/read/gpu-based-procedural-placement-in-horizon-zero-dawn)

- Scale: "500+ asset types", "100.000+ objects in scene", "~250µs avg busy load on GPU"; one GENERATE shader about 10
  µs, one PLACEMENT shader about 7 µs.
- **World data** is "a collection of 2D maps", "streamed in sections", all generated and all paintable, about **4
  MB/km²** (about 32 bits per m²). Examples: terrain height at 0.5 m (16 bit), tree placement map at 1 m (BC7),
  ecotope maps A-H at 2 m (BC7); layers named Topo_Roads, Topo_Objects, Topo_Water. (The speaker's notes say 1-4 m; the
  table says 0.5-2 m.)
- An **ecotope** decides "asset types, asset distribution, colorization, weather, effects, sound, wildlife".
- Placement logic is a node graph whose output is **one density map per layer**, discretised into points by a
  precomputed dither pattern scaled to the asset's **footprint**: "large trees six meters apart, undergrowth one meter
  apart". Density maps are 64 x 64 px per block; trees work in 128 x 128 m blocks, grass in 32 x 32 m blocks.
- **Layered dithering**: layers with the same footprint stack as ranges of one density, so they never collide; more
  than 20 such layers in one production ecotope.
- Layer flattening cut active layers near the player "from several thousands to several hundreds".
- Team: nature assets by **3 people**, ecotopes by **1**.
- Artists repaint a tree line, redirect a road or turn a desert into a swamp equally easily (page summary).

Vegetation art talk, GDC 2018 (Gilbert Sanders): grass tilts when the camera looks down, so the player never sees
the empty ground between grass meshes. [S]
https://gamedeveloper.com/art/video-how-guerrilla-crafted-the-lush-vegetation-of-i-horizon-zero-dawn-i-

---

## 4. Far Cry 5 (Ubisoft Montreal, Dunia, Houdini)

The earlier note has the biome-recipe summary. New here:
- The GDC 2018 talk's brief: fill "100 square km of wilderness"; tools to "generate biomes, texture the terrain, set up
  freshwater networks, generate cliff rocks". [P] https://80.lv/articles/houdini-procedural-world-generation-of-far-cry-5/
  and [S] https://gdcvault.com/play/1025557/Procedural-World-Generation-of-Far
- Mountains channel rain into streams and lakes that "erode the land and affect nearby vegetation"; grasslands on
  exposed hills, ponderosa forest in valleys, denser forest on shaded humid north faces; where conditions overlap
  "several species will fight for the ground"; grass reflects terrain humidity and is oriented by a wind vector map;
  each plant appears only in its own environment type. [P]
  https://blog.playstation.com/2018/03/22/the-procedural-world-generation-of-far-cry-5/
- **Not found:** fences, power lines, terrain resolution, densities, or the hand-edited share. The talk video is on GDC
  Vault and Vimeo (https://vimeo.com/273986776), not read.

---

## 5. Ghost Recon Wildlands (Ubisoft Paris, AnvilNext, Houdini)

Three 80.lv articles by the team. [P]

**Ground** https://80.lv/articles/landscape-and-material-pipeline-of-ghost-recon-wildlands/
- 11 ecosystems, each with rules, from reference gathered in Bolivia.
- Each terrain material is a "Terrain Procedural Set" built from a "Terrain Procedural Material" with stacked layers,
  covering near ground and distant "vista" textures; placement driven by **slope, height, flow, density, deposition and
  curvature maps**; each material blends **4 foreground textures** with a random tile; foreground supports
  tessellation and displacement matched to the far vista.
- A virtual texture layers decals (albedo, normal, height) over the terrain, spread by Houdini or by hand. Puddles flatten
  the normal and override albedo in their mask.
- Rocks scattered by terrain material, curvature, slope, cliffs and road detection, kept out of camps, villages, roads
  and footpaths.

**Vegetation** https://80.lv/articles/vegetation-generation-in-ghost-recon-wildlands/
- Grass spawns patterns by the terrain splatting (the ground's material decides the grass).
- Scatter rules tune density, scale, slope and spacing, then sun direction and material inclusion/exclusion (flow maps,
  river beds, cliffs); rules combine with a delete threshold.
- **A cascade**: big trees spawn medium trees, which spawn small trees and bushes. Few species per rule, grouped by
  species, for natural results and memory.
- Roads, railways, water and footpaths are cleared automatically.
- **LOD distance scales with local density**, not fixed per asset: denser stands switch earlier to the last LOD.
- That last LOD is an in-engine **dynamic impostor** (several side views by camera angle) with channels for light
  direction, fake shadowing and translucency.

**Roads and settlements** https://80.lv/articles/procedural-world-building-in-ghost-recon-wildlands/
- Roads by a cost-based pathfinder: steep slopes and tight turns cost more, existing roads are cheap, so new roads branch
  off old ones; "thousands of kilometers of roads". Terraforming turns the network into height maps, materials and masks
  stamped on the terrain (render farm, minutes).
- Railways take shallower sweeping paths with tunnels and bridges; rivers only flow downhill to the nearest water.
- The settlement builder packs buildings with **more than 70 rules**, e.g. no second religious building "within
  approximately 150 meters of that church"; v2 added grid roads, sidewalks and rows. Big towns took up to 20 minutes to
  cook; results stored as files and placed by a light "settlement instancer" so tweaks need no re-cook.
- The road and village methods came from CNRS research adapted by Ubisoft. [S]
  https://perso.liris.cnrs.fr/egalin/2014-2010.html

---

## 6. Ghost of Tsushima (2020) and Ghost of Yotei (2025) (Sucker Punch)

- GDC 2021 talks: "Procedural Grass in Ghost of Tsushima" (Eric Wohllaib) and "Samurai Landscapes: Building and
  Rendering Tsushima Island on PS4" (Matt Pohlmann). Slides not read; vault pages hold only titles. [S]
  https://gdcvault.com/play/1027214/Advanced-Graphics-Summit-Procedural-Grass ,
  https://gdcvault.com/play/1027352/Samurai-Landscapes-Building-and-Rendering
- A detailed third-party summary of the grass talk [P, secondary]
  https://threadreaderapp.com/thread/1688939411575119872 :
  - World in tiles; each tile has textures for terrain height, terrain material and **grass type**; a compute shader
    places each blade and jitters it; blade type and height come from the tile textures at its position.
  - Each blade is a Bézier curve from its type's parameters (height, width, tilt, colour); **15 vertices** near, **7**
    far; high LOD morphs into low LOD near the switch to avoid pops.
  - Noise groups blades into **clumps** that share height, colour and direction, "imitating how real grass grows in
    patches".
  - Wind: a scrolled 2D Perlin texture sets strength and bend, mixed with a sine bob; each blade moves a little
    differently.
  - Short blades are **folded** into two, so short grass looks twice as dense for free; blades edge-on to the camera are
    turned slightly towards it.
  - **Far away, grass and foliage are removed and replaced by an artist-authored texture** in the ground material.
- Ghost of Yotei (PlayStation Blog tech deep dive) [P] https://blog.playstation.com/?p=411201 :
  - "Longer sight lines" needed "better rendering for grass, terrain and mountains into the distance"; distant mountains'
    models and detailed materials **baked into textures**.
  - The GPU compute renderer "doubled the amount of grass and renderable items".
  - Distant mountains with "over one million trees, rocks, and bushes" culled to "about sixty thousand individual items"
    for the G-buffer, by GPU occlusion culling.
  - Procedurally assisted authoring; broad flower fields generated at runtime on the GPU; terrain tessellation for detail
    and deformation; the goal that "everything onscreen" moves with the wind.

---

## 7. Red Dead Redemption 2 (Rockstar, RAGE)

Rockstar has published no terrain or vegetation talk that was found; its SIGGRAPH 2019 talk is on sky, clouds and
volumetrics (Fabian Bauer). [S] https://history.siggraph.org/?p=76936
- A RenderDoc frame study: footprints and wagon trails accumulate into a mud texture "used for Parallax Occlusion
  Mapping when rendering terrain"; terrain is drawn in a depth prepass. [P]
  https://imgeself.github.io/posts/2020-06-19-graphics-study-rdr2
- GamingBolt's tech analysis: grass and tree density higher than GTA V; trees "just look right" from "physically
  accurate bark textures"; snow displaced in real time. [P]
  https://gamingbolt.com/red-dead-redemption-2-tech-analysis-key-improvements-from-gta-5-world-building-and-more
- A modder's HD pack raises vegetation textures from 512 to 1024 px, suggesting the shipped size. [S]
  https://www.dsogaming.com/mods/red-dead-redemption-2-gets-a-new-hd-texture-pack-that-improves-its-grass/
- **Not found:** RDR2's terrain resolution, layer counts or grass numbers. Claims of "8-12 foliage entities per m²"
  attributed to Digital Foundry could not be traced to Digital Foundry and are not used.

---

## 8. The Witcher 3 (CD Projekt RED, REDengine 3)

- GDC 2014, Marcin Gollent, "Landscape Creation and Rendering in REDengine 3": "moving away from the conventional
  terrain material layers and linear material blends" improved quality and cut work; vegetation "procedurally generated
  both offline and in real-time"; terrain shadows and LOD. [P]
  https://www.gdcvault.com/play/1020394/Landscape-Creation-and-Rendering-in
- An unofficial copy of the slides: a clipmap in a texture array, Novigrad 46 x 46 tiles of 512 x 512, 5 clipmap
  levels; about 30 MB for elevation, normal and control maps (5 levels, 1024 window); terrain shadows store the highest
  shadowed height. [S] https://es.scribd.com/document/369764097/Gollent-Marcin-Landscape-Creation-And
- Heightmap sizes from the shipped files: White Orchard 2000 m at 4096 px (about 0.49 m), Novigrad+Velen 8625 m at 23,552
  px (about 0.37 m), Skellige 7472 m at 16,384 px (about 0.46 m), Toussaint 8000 m at 16,384 (about 0.49 m). The
  **control map** holds "texture indices, UV scale, slope threshold" per pixel. [P]
  https://github.com/Til-Weimann/witcher3-heightmaps
- A former level designer: terrain textures hand-painted; generation limited to a basic grass layer by terrain texture
  type; trees, bushes and flowers placed by hand. [S]
  https://stevivor.com/news/cd-projekt-red-the-world-of-the-witcher-3-is-not-procedurally-generated

**Take-away [SAM3D]:** the Witcher's ground is a control map (which two materials, at what scale, where slope switches
them) under sub-half-metre height, not a stack of painted weight layers. A control map is a natural output of
simcore's cover raster.

---

## 9. Forza Horizon 5 and MSFS 2024 (photogrammetry plus procedural vegetation)

**Forza Horizon 5** (Playground)
- Mexico in 11 biomes, each researched and rebuilt with photogrammetry (art director Don Arceta). [S]
  https://forza.net/news/exploring-forza-horizon-5-biomes-and-seasons
- A Mexico-based photographer supplied "countless photos" for texture and reference captures. [S]
  https://www.benq.com/en-us/knowledge-center/knowledge/photography-in-the-making-of-forza-horizon-5-with-nicole-francine-holmes.html
- Sky: 48,000 8K captures, 75 TB. [S]
  https://www.tweaktown.com/news/80373/forza-horizon-5s-skies-48-000-8k-image-captures-worth-75tb-of-data/index.html
- **Not found:** FH5's vegetation pipeline.

**MSFS 2024** (Asobo). The SDK's vegetation docs are concrete. [P]
- Biomes and species are two XML files; biomes placed by polygons in the scenery editor; biomes may be limited to WWF
  eco regions and "potential natural vegetation" types (e.g. "Temperate Deciduous Broadleaf Forest", "Steppe").
  https://docs.flightsimulator.com/msfs2024/retail/content-configuration/environment/vegetation/biomes-and-vegetation/index.md
- A biome rule's density is `speciesInstancesPerHectar` (required); each species has a `spawnRatio` and an optional
  `<Color r g b>` **multiplier**; `onlyArtificialAreas` and `artificialSurfacesScaleFactor` set vegetation in built-up
  areas.
  https://docs.flightsimulator.com/msfs2024/retail/content-configuration/environment/vegetation/biome-xml-properties/index.md
- A species has variations, each with a spawn ratio, a size range in m, an impostor (wind `stiffness` 0-1, `frames`
  per atlas edge) and a `MaskColor` that keeps aerial colouring off parts such as trunks; one material per species; an
  optional ground "grass" material for the biome.
  https://docs.flightsimulator.com/msfs2024/retail/content-configuration/environment/vegetation/species-xml-properties/index.md
- The SDK examples use **200 instances per hectare**; conifers 15-50 m tall in four variations; impostors with 8 or 10
  frames per edge.
  https://docs.flightsimulator.com/msfs2024/retail/content-configuration/environment/vegetation/biome-and-species-examples/index.md
- Press: full 3D ground with procedural vegetation in 30 biomes; tessellated ground materials; about 20 3D crops. [S]
  https://pcgamer.com/games/sim/more-details-on-ms-flight-simulator-2024-including-full-3d-landscapes-in-30-biomes and
  https://news.flightsim.to/fsexpo-2024-key-takeaways-for-microsoft-flight-simulator-2024

---

## 10. Other games

| Game | What is public | Source |
|---|---|---|
| theHunter: Call of the Wild (Expansive Worlds, Apex) | "the majority of the vegetation is placed procedurally in our tools", part of Apex [S]. No numbers found. A user believes grass distance is an engine limit [S, user]. | https://gamingbolt.com/thehunter-call-of-the-wild-interview-hunting-beasts , https://steamcommunity.com/app/518790/discussions/0/3830918083252469724 |
| Hunt: Showdown (Crytek, CryEngine) | "we mostly use CRYENGINE tools", which "partly offer procedural tools"; natural materials (forest ground, bark, soil) "usually based on scanned data" as the base of tileable materials; man-made materials procedural; few shadow casters and aggressive view-distance culling; villages or farms sometimes swapped for rivers, forests, hills for play [P]. The 2024 Stillwater Bayou rework: "improved vegetation and ground details", new water [S]. | https://80.lv/articles/the-development-of-hunt-showdown , https://press.crytek.com/crytek-brings-the-beloved-stillwater-bayou-map-back-to-hunt-showdown-1896-in-a-stunning-visual-and-gameplay-upgrade |
| Euro Truck Simulator 2 (SCS) | No public write-up on roadside or terrain method found. Blog posts show regions' forests as screenshots only [P]; v1.18 added prefab vegetation distances and rebalanced vegetation LODs [S]. | https://blog.scssoft.com/2019/07/road-to-black-sea-landscapes.html , https://www.gamepressure.com/download/euro-truck-simulator-2-v1171-1181-patch/z5e1f9 |
| SnowRunner (Saber, Swarm) | Deforming ground from MudRunner's physics engine, changed for larger maps and snow [S]. No vegetation or terrain-art source found. | https://en.wikipedia.org/wiki/SnowRunner |
| Chernobylite (The Farm 51, UE4) | About 20 people; locations from photogrammetry and laser scans of the Exclusion Zone; artists doubled as photographers; dynamic lighting after baked lighting took too long [P]. | https://80.lv/articles/chernobolyte-game-based-on-3d-scanned-chernobyl/ |
| Pathologic 2 (Ice-Pick Lodge, Unity) | Steppe town; no environment-tech source found [S]. | https://en.wikipedia.org/wiki/Pathologic_2 |
| Atomic Heart (Mundfish, UE4) | Reviews call the open areas "lush Soviet nature" but also "largely empty" [S]. No tech source. | https://www.thesixthaxis.com/?p=372907 , https://dotesports.com/?p=802630 |
| Manor Lords (Slavic Magic, one developer, UE) | Burgage plots "dynamically resize and reshape" to the space roads leave, roads curve round the terrain [S]. A third-party UE 5.4 PCG village generator copies the idea [S]. | https://jeu.video/en/games/manor-lords , https://80.lv/articles/grab-a-new-manor-lords-inspired-procedural-village-generator-for-ue5 |

**UE note.** PCG's generation modes: partitioned (per grid cell, streamable), hierarchical (per-node grid sizes) and
runtime (generated and cleaned up around generation sources, in builds too). [S]
https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine . Users report
GPU static-mesh spawners losing meshes in runtime-partitioned actors in 5.6-5.7. [S, user]
https://forums.unrealengine.com/t/pcgruntimepartitionedactors-with-gpu-static-mesh-spawners-stop-rendering-meshes-and-need-to-be-refreshed-reinitialization-manually/2712334

---

## 11. Comparison

| Game | Height spacing | Ground blending | Vegetation rules | Far field | Team / tools |
|---|---|---|---|---|---|
| KCD 1/2 | bought real terrain (P); n/f | n/f | n/f; one map for all platforms (P) | Object Detail setting (P) | ~70 (P); CryEngine |
| Farming Sim 25 | 1 m (P) | weight maps, 2 per texture (S); furrow angle in density bits (P) | density maps: type + growth state (P) | foliage distance texture (P, attribute) | GIANTS Editor |
| Horizon ZD | 0.5 m height (P) | n/f (ecotope colourisation, P) | density map per layer, footprints 6 m / 1 m, >20 stacked layers (P) | n/f | 3 nature artists + 1 ecotopes (P) |
| Far Cry 5 | n/f | humidity tint (P) | species compete by slope, sun, humidity (P) | n/f | Houdini |
| Wildlands | n/f | 4 textures per material, 6 driver maps, VT decals (P) | cascade big→small, density-scaled LOD (P) | dynamic impostors with light channels (P) | Houdini, ZBrush, SD |
| Tsushima / Yotei | n/f | tile grass-type texture (P*) | compute blades, clumps, folding (P*) | grass → texture far (P*); 1 M items → 60 k (P) | in-house |
| Witcher 3 | 0.33-0.78 m (P) | control map: indices, UV scale, slope (P) | grass by texture type, rest hand-placed (S) | clipmap 5 levels (S) | REDengine |
| MSFS 2024 | DEM | aerial + 3D ground materials (S) | instances/ha, spawn ratio, colour multiplier (P) | impostors 8-10 frames/edge (P) | SDK XML |

(n/f: not found. P*: from a third-party summary of the talk.)

---

## 12. Lessons for SAM3D

SAM3D facts used [SAM3D]: simcore's ground (10/20 m detail areas, 50 m range, far ground to 150 km), a 25 m cover raster
with crop parcels (250-700 x 80-250 m) and a furrow heading per 3 km square, about 150,000 planted trees, buildings,
obstacle heights for the sight lines; Unreal draws ground tiles as ProceduralMeshComponent with vertex colour, trees and
buildings as HISM. **L** = needs a UE Landscape. **M** = works on our custom mesh ground. Most lessons are **M**: the
ones that are Landscape-only in UE (Landscape Grass, layer painting) have a mesh equivalent through a material reading
our own textures and an instancer fed by simcore.

1. **Every field carries data, not a picture.** Farming Simulator stores crop type, growth state, tillage and the
   furrow angle as bits in density maps that the ground shader and the foliage system both read (GIANTS forum). Bake per
   ground tile a small "field state" texture from simcore's parcels: crop (stubble wheat, stubble maize, standing maize,
   cut sunflower, ploughed, harrowed, emerging winter wheat, pasture, fallow), state, furrow angle. Start at 2 m per
   pixel in detail areas and 8 m over the range, 1 byte each channel [SAM3D proposal]. **M**.

2. **Furrows from an angle, not a mesh.** FS quantises furrow direction to 4 headings 45° apart (8 with one more bit)
   and rotates the ploughed normal map in the shader (GIANTS forum). Use simcore's parcel heading at full precision (one
   byte, 1.4° steps) to rotate the ploughed-soil and stubble-row normal maps per pixel. This alone turns the flat brown
   fields into ploughland at 0-300 m [SAM3D]. **M**.

3. **A control map, not many painted layers.** The Witcher 3 stores per pixel texture indices, UV scale and a slope
   threshold, and dropped linear multi-layer blends for quality and less work (Gollent GDC; heightmap repo). Wildlands
   blends 4 textures per material driven by slope, height, flow, density, deposition and curvature maps (80.lv). For
   our mesh: a control texture holding material A, material B, blend, and a slope/wetness term, made by simcore from
   cover, crop and relief; the material samples only those two sets near the camera [SAM3D proposal]. Start with 8-12
   ground materials in a texture array. **M** (a Landscape would force layer weights instead).

4. **Break tiling with two variants and macro noise.** FS25 uses two weight files per texture so the game can blend
   them against repetition (maps4fs docs); Witcher keeps a per-pixel UV scale (control map); Wildlands adds a random
   tile (80.lv). Give each ground material two variants, blend them by a 20-50 m noise, and vary UV scale per parcel
   [SAM3D proposal]. **M**.

5. **Crops near as geometry, far as a texture, at one view distance.** FS foliage types have `viewDistance`,
   `blendOutDistance` and a `distanceTexturePath` (FS doc); Tsushima removes far grass and draws an artist texture in
   the ground material (talk summary); FS terrain detail fades at 75 m with 5 m blend-out (GIANTS forum). Start with
   standing maize and stubble as instanced blocks to 150 m with a 20 m fade, then the crop's distance texture through
   the field-state map; reeds the same [SAM3D proposal]. For 15 October most parcels are stubble, ploughed or cut, so
   only maize, reeds and sunflower stalks need height. **M**.

6. **Grass that reads dense for few triangles.** Tsushima: compute-generated Bézier blades at 15/7 vertices, LOD morph,
   short blades folded in two, edge-on blades turned to the camera (talk summary); Horizon tilts grass when the camera
   looks down so no bare ground shows (GDC 2018 summary). In UE, start with mesh clumps of 30-60 blades on HISM at about
   one clump per m² to 60-100 m on verges, pastures and wood edges, fading over 15 m, with Tsushima's folding and
   camera-tilt tricks in the vertex shader [SAM3D proposal]. Since Landscape Grass needs a Landscape (earlier note),
   feed instances from simcore's cover raster or PCG on our mesh. **M**.

7. **Clumps, not uniform noise.** Tsushima groups blades into clumps sharing height, colour and direction "as real grass
   grows in patches" (talk summary); Horizon's ecotopes drive "colorization" (GDC 2017); MSFS gives every species a
   colour multiplier per biome (SDK); Far Cry 5 tints grass by humidity (PlayStation blog). Drive grass and crop colour
   by two noises (2-5 m clumps, 30-100 m patches) plus wetness from simcore's ground (river floodplain greener, sand
   ridges drier) [SAM3D proposal]. **M**.

8. **Trees by footprint and layered density, not by a single spacing.** Horizon places each layer by a density map
   discretised at its footprint: large trees 6 m apart, undergrowth 1 m, with 20+ same-footprint layers stacked so they
   never collide (GDC 2017). MSFS biomes are instances per hectare with spawn ratios per species (SDK; examples at
   200/ha). Move simcore's planting from one 7 m spacing to layers: canopy (oak, hornbeam, poplar, black locust) at 6-8
   m footprint, sub-canopy at 3-4 m, shrubs at 1-2 m only where light reaches (edges, clearings, rides) [SAM3D
   proposal; check against Hungarian stand data first]. **M** (simcore).

9. **Edges by a cascade.** Wildlands spawns medium trees from big ones and small trees and bushes from those, and
   clears roads, railways, water and paths automatically (80.lv). A wood edge then grades from big trees through small
   trees to a shrub mantle without hand work. In simcore: a 10-20 m edge band where shrub and small-tree density rises
   and canopy density falls, on every wood, tree row, river bank and village garden edge; a 3-5 m mown or ploughed
   strip next to fields [SAM3D proposal]. This is what turns the flat green wall into a forest. **M**.

10. **Species fight for the ground by rule.** Far Cry 5's species compete where conditions overlap, with denser forest
    on shaded humid slopes (PlayStation blog); KCD2 split planted forest from natural forest "based on research about
    biodiversity" (Xbox Wire). Kesmark equivalents: planted black locust and pine in straight rows on sand, poplar
    plantations in grids on floodplains, oak/hornbeam irregular on loess and the northern hills, willow and reed at
    water [SAM3D]. Row planting is itself a realism cue: plantations are rows. **M**.

11. **LOD by density, impostors that keep the light.** Wildlands switches denser stands earlier to impostors, and its
    impostors carry light direction, fake shadow and translucency channels (80.lv); MSFS impostors use 8-10 frames per
    atlas edge with a wind stiffness (SDK). Bake octahedral impostors per species with normal and depth so wood masses
    light correctly at low October sun; switch at 600-1500 m scaled by stand density [SAM3D proposal]. **M**.

12. **Far field: cull hard, bake the rest.** Ghost of Yotei culls over a million distant trees, rocks and bushes to
    about 60,000 drawn items and bakes distant mountains' detail into textures (PlayStation Blog). Beyond about 6 km draw
    woods as shaded masses with the impostors' average colour baked into the ground colour, and stop drawing single
    trees except along the skyline and sight lines that simcore says matter [SAM3D proposal]. **M**.

13. **One vegetation map for every setting.** KCD keeps one vegetation map for all platforms and puts only extra
    clutter in PC layers (gamebanshee interview); Gray Zone Warfare does the same for fairness (earlier note). Every
    obstacle simcore uses for sight is drawn at every quality level; only grass, crop clutter and shadows scale [SAM3D].
    **M**.

14. **Relief at three scales, with sight kept honest.** Witcher 3 heightmaps are 0.33-0.49 m (heightmap repo); FS25
    moved to 1 m (number13); Horizon's height is 0.5 m (GDC 2017). Our 10 m simcore grid cannot show ditches, field
    edges, cart-track ruts or levees. Keep simcore's ground for sight lines and give it the large forms (the plain's 2-5
    m undulations over 200-500 m, dunes, levees, the northern hills); add metre-scale features that matter for sight
    (dykes, embankments, revetments) as simcore vector shapes; leave sub-metre forms (furrows, ruts, ridges between
    parcels) to normal and parallax maps under 0.3 m [SAM3D proposal]. **M**; a Landscape at 1-2 m would carry the
    middle scale, but would disagree with simcore unless simcore reads it back.

15. **Ruts and tracks tell the story.** RDR2 accumulates footprints and wagon trails into a mud map used for parallax
    occlusion on the terrain (frame study); FS25 draws tyre furrows in fields (number13); Wildlands lays height decals
    through a virtual texture (80.lv). Paint farm-track ruts, tractor tramlines in winter wheat, field-entrance mud and
    the battery's own vehicle tracks into a decal/RVT layer; tramlines at 12-24 m spacing along the parcel heading
    [SAM3D proposal, spacing to verify]. **M** (RVT takes any primitive, earlier note).

16. **Roads stamped into the ground with their verges.** Wildlands' roads are terraformed into heights, materials and
    masks, and vegetation and rocks are cleared from them automatically (80.lv). Simcore should cut each road's bed,
    camber and ditches into its ground and write the verge, ditch and gravel shoulder into the control map; the drawn
    road mesh then sits on a matching bed [SAM3D]. **M**.

17. **Villages from rules plus period maps.** KCD2 took village layouts from mid-19th-century military maps (Xbox Wire);
    Wildlands packs buildings by more than 70 rules such as church spacing, and stores results for a light instancer
    (80.lv); Manor Lords subdivides plots by the roads (jeu.video, S). For a Hungarian street village: long narrow plots
    perpendicular to the street, the house gable-end to the street, outbuildings behind, a garden and orchard, then the
    plot's field strip; fences and gates on the street line; church and shop at the centre [SAM3D, check against period
    photographs and the Habsburg survey sheets for Hungary, not yet verified here]. Simcore generates; Unreal instances.
    **M**.

18. **Scanned natural materials, procedural man-made ones.** Hunt bases forest floor, bark and soil on scans and makes
    metal, wood and fabric procedurally (80.lv); Chernobylite's 20 people scanned the real Zone (80.lv); Warhorse reworked
    materials in Substance (ArtStation, S). For SAM3D: ground, bark and plaster from scanned libraries with their licences
    recorded in `data/scenery/assets.json`; whitewash, roof tile, brick and fence wood from Substance-style generators
    [SAM3D]. **M**.

19. **Everything moves.** Tsushima drives grass by a scrolled Perlin wind field mixed with a bob (talk summary); Far
    Cry 5 orients grass by a wind vector map (PlayStation blog); Yotei wants "everything onscreen" to move (PlayStation
    Blog). Drive grass, maize, reeds and poplar crowns from simcore's weather wind, with one shared gust texture, so a
    still frame and a moving one tell the same weather [SAM3D]. **M**.

20. **Rules as data, editable by one person.** Horizon's nature was 3 artists plus 1 for ecotopes (GDC 2017); MSFS
    biomes are two small XML files (SDK); Chernobylite was 20 people (80.lv). Put densities, footprints, spawn ratios,
    colour multipliers and edge bands into `data/scenery/map_dressing.json` per cover class, in MSFS's shape (instances
    per hectare, spawn ratio, colour), so tuning is data, not code [SAM3D]. **M**.

21. **Reference photos per cover class before tuning.** KCD shot plants on location with a mobile bluescreen (80.lv);
    FH5 hired a local photographer for textures and references (BenQ, S); KCD2 used surviving buildings' custodians and
    archaeology (Xbox Wire). Catalogue Great Plain photos per cover class and season (stubble, ploughland, maize, poplar
    rows, locust groves, street villages, reed beds) and compare the game's view from the same height and focal length
    [SAM3D]. **M**.

### What our screenshots lack → which game shows how to fix it

| Lacks | Fix shown by | Lesson |
|---|---|---|
| Bare textured fields with nothing on them | Farming Simulator (crop type and growth state per pixel, near geometry, far distance texture); MSFS 2024 3D crops | 1, 5 |
| Ploughland looks like flat brown paint | Farming Simulator (furrow angle in the density map, rotated in the shader) | 2 |
| Ground almost perfectly flat | Witcher 3 (0.3-0.5 m heights), Horizon (0.5 m), RDR2 (mud POM), Wildlands (height decals, tessellation) | 14, 15 |
| One colour per field, tiling | Witcher 3 control map, FS25 two variants per texture, Tsushima clumps, MSFS colour multipliers, Far Cry 5 humidity tint | 3, 4, 7 |
| Trees sparse | Horizon footprints and stacked layers; MSFS instances per hectare | 8, 10 |
| Trees low-poly, wood masses as flat green walls | Wildlands cascade edges and lit impostors; Yotei baked far detail | 9, 11, 12 |
| No grass, no clutter near the camera | Tsushima blades and folding; Horizon camera tilt; CryEngine grass by terrain layer | 6 |
| Roads and verges are stripes on the ground | Wildlands road terraforming and automatic clearing | 16 |
| Buildings as dark boxes, villages unreadable | KCD2 period maps; Wildlands settlement rules; Hunt scanned and procedural materials | 17, 18 |
| A still, dead landscape | Tsushima wind, Far Cry 5 wind vector map, Yotei | 19 |

---

## 13. Gaps

- **KCD 1/2:** no public figures for terrain resolution, ground layers, grass or tree density, impostor distances, or
  how fields were made. PC Games Hardware's studio-visit report and Autodesk's Warhorse story blocked reading.
- **Farming Simulator:** GIANTS publishes no terrain/foliage reference that was found; the mechanics come from modders'
  forum posts and a third-party generator's docs.
- **Horizon:** the deck was read through SlideShare's transcript, not the PDF (too large for the reader); the GDC 2018
  vegetation talk (Sanders) was not read.
- **Tsushima:** both GDC 2021 talks unread; the grass facts come from a third-party summary.
- **Not found at all:** RDR2's terrain and vegetation method; Forza Horizon 5's vegetation pipeline; theHunter's numbers;
  ETS2's roadside method; SnowRunner's, Pathologic 2's and Atomic Heart's environment tech; Far Cry 5's fences and power
  lines.
- Hungarian specifics proposed above (tramline spacing, street-village plot pattern, the Habsburg survey sheets) are
  [SAM3D] leads for the reference track, not checked here.
