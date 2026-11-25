# How AA/AAA games build realistic Eastern European open terrain: lessons for SAM3D

Research notes, 3 November 2026. Scope: S.T.A.L.K.E.R. 2, Arma 3 / DayZ (Chernarus, Livonia), Arma Reforger,
Squad, Escape from Tarkov, Gray Zone Warfare, Hell Let Loose, Enlisted (Dagor), Post Scriptum, Microsoft Flight
Simulator and DCS (plus IL-2 Great Battles) for the far field, with a few cross-references (Far Cry 5, Horizon Zero
Dawn, Epic's own documentation) where they explain a technique the games above use. The purpose is SAM3D's art
direction and budgets for a data-generated map (simcore) drawn in UE 5.8 (procedural-mesh ground at 10-50 m spacing
near the player, vertex colour today, HISM trees and buildings).

## How to read these notes

- Every claim carries its URL. Tags say how it was checked:
  - **[P]** read on the page itself.
  - **[S]** seen only in a search engine's summary of the page (the page blocked automated reading: Bohemia's wiki,
    forums and feedback tracker return 403; Fandom returns 402; PC Games Hardware, TechPowerUp, ArtStation and the
    Unreal Engine developer-interview pages return 403). Treat [S] as lower confidence.
  - **[SAM3D]** our own data or an inference of ours, not a source claim.
- No asset files were downloaded. One public conference talk PDF (Asobo's terrain talk) was fetched as a document
  and read as text because it was too large for the page reader.
- Numbers from game pages are for comparison and art direction only; none of them is system data, so none goes into
  `data/systems/`.

---

## 1. S.T.A.L.K.E.R. 2: Heart of Chornobyl (GSC Game World, UE5)

**Map size and structure**
- Seamless map of about 60-64 km², split into about 20 zones; sources disagree on 60 vs 64. [S]
  https://www.gfinityesports.com/article/stalker-2-map-size-explained
- The studio says its locations are "almost entirely hand-crafted" with no procedural generation (technical producer
  Evgeniy Kulik). Headline [P], body [S].
  https://pcgamer.com/games/fps/despite-being-one-of-the-biggest-unreal-engine-5-games-ever-stalker-2-s-locations-are-almost-entirely-hand-crafted-it-took-a-lot-of-time-took-a-lot-of-effort-but-we-re-happy-with-the-result
- Asked which UE5 features most affected development, GSC named World Partition (Epic developer interview, 22 Oct
  2024). [S]
  https://www.unrealengine.com/developer-interviews/balancing-nostalgia-with-innovation-in-s-t-a-l-k-e-r-2-heart-of-chornobyl
- The official Zone Kit modding guide warns its level tools "do not work with World Partition yet" (the map is a
  World Partition level). [P, line read from the PDF text]
  https://cdn.stalker2.com/Mod.io/S2_HoC-Zone_Kit_Full_Modding_Guide.pdf

**Reference gathering**
- The former lead of level designers says the studio "organized regular trips" to the real Exclusion Zone to collect
  references that could not be found online; the map merges the real Zone with the original series' locations; the
  whole map was first blocked out, then its metrics and paths tested. [P]
  https://novyny.live/en/tehnologii/how-locations-in-stalker-2-were-created-developer-219358.html

**Vegetation and rendering**
- PC Games Hardware reports GSC switched vegetation to Nanite-compatible polygon models in 2024, reusing the existing
  textures and materials (converted alpha-tested cards), and that LOD switching is still visible at long range. [S]
  https://www.pcgameshardware.de/Stalker-2-Spiel-34596/Specials/Release-Test-Review-Steam-Benchmarks-Day-0-Patch-1459736/4/
- Lumen runs in its software mode, and the foliage quality setting "greatly changes how much grass you'll see" (PC
  Gamer's settings guide). [S]
  https://pcgamer.com/hardware/stalker-2-heart-of-chornobyl-best-graphics-settings-guide
- Another settings guide in the same search calls Foliage Quality the most taxing setting (which guide was not
  confirmed: PC Gamer's or Shazoo's). [S]
  https://shazoo.ru/2024/11/20/161802/rukovodstvo-po-nastroike-grafiki-v-stalker-2-kak-vyzat-maksimum-iz-sistemy
- Players report that the console variable `fg.CullDistanceScale.Trees` sets the share of full trees to 2D sprite
  trees (user finding, not developer documentation; low confidence). [S]
  https://cal1.lr.ggtyler.dev/r/FuckTAA/comments/1gwsskq/stalker_2_foliage_optimization_trees_and_flowers/lynb7rq
- Update 2.0 (20 Aug 2026): engine to UE 5.5.4, "Reworked the landscape and foliage to look more natural", lighting
  "changed significantly", a fog mechanic in which "you and your enemies will see less". [P]
  https://mp1st.com/title-updates-and-patches/stalker-2-version-2-0-alongside-cost-of-hope-dlc-update-1-016
- Patch 2.0.5: "Adjusted the distance of foliage appearance around the player character." [P]
  https://www.dsogaming.com/patches/stalker-2-patch-2-0-5-released-with-over-200-fixes/

**Not public:** heightmap resolution, landscape layer count, grass density, impostor ranges, frame budgets.

**Take-away:** a 64 km² hand-made Ukrainian landscape in UE5 still needed a two-year foliage and landscape rework after
launch; foliage density/distance is the setting that costs most and players notice most. Hand-crafting at that scale
is a studio-sized job; SAM3D must get its look from rules and data instead.

---

## 2. Arma 3 and DayZ (Bohemia; Real Virtuality / Enfusion; Chernarus, Livonia)

**Chernarus (Arma 2, carried into DayZ): a real Czech landscape**
- Ivan Buchta: "We were looking for a suitable real-life terrain rather than attempting to create a fictional
  environment"; the area between Ústí nad Labem and Děčín (Central Bohemian Uplands): "deep valleys, rolling hills,
  and plains"; a "super-texture" technique put "a terrain mesh covered by aerial orthophotos" in place of generic
  surface tiles; for play they "added airstrips, reduced the size of the settlements, and simplified the trees and
  vegetation"; the ground beyond the playable area is procedurally generated bare terrain; the setting was changed to
  post-Soviet Russia with Russian road signs and village houses. [P]
  https://www.killscreen.com/we-interviewed-dayz-developer-right-he-was-arrested-greece/
- ChernarusPlus is 15.36 km a side, about 225 km² (sources give 225-236 km²). [S]
  https://xgamingserver.com/blog/dayz-chernarus-map-guide/
- Livonia (Arma 3 Contact / DayZ): 163 km² of forest, lake and river; a fictional Polish-speaking region. [S]
  https://pcgamer.com/dayz-is-getting-a-new-map/ and https://community.bistudio.com/wiki/Livonia
- Altis is 270 km², Stratis 20 km². [P] https://arma3.com/features/terrain

**How the ground is textured (Real Virtuality)**
- Two parts: a satellite (colour) texture over the whole terrain plus a surface mask; each mask colour maps to a
  surface (detail texture + material) in `layers.cfg`; clutter comes from the surface. Surfaces per terrain tile:
  4 in Arma 1, 6 in Arma 2 (alpha in the mask tiles), 5 in Arma 3 when a normal map is used (it takes one slot),
  limited by "up to 16 textures sent to the pixel shader" per draw. [P]
  https://gist.github.com/pennyworth12345/27786cf99652e0018b9b2f0becaa0638
- Example CfgWorlds values (Arma 2 Chernarus style): `clutterGrid = 1.0` (1.5 in arid Takistan), `clutterDist = 125`
  (m), `fullDetailDist = 15`, `noDetailDist = 40`: the ground detail textures are seen only close; "At longer ranges
  the satellite texture is seen instead", and the satellite colour is mixed into the detail texture "to produce colour
  variations right up to the player's feet". [P]
  https://sites.google.com/site/islandconfigs/cfgworlds-overview/cfgworlds-clutter
- DayZ's wiki: `clutterGrid` is the size of the square (m) holding one clutter model; `clutterDist` the base
  distance (m); high `clutterDist` (150 and more) is "very performance-heavy"; avoid `clutterGrid` below 1.0. [S]
  https://community.bistudio.com/wiki/DayZ:Grass-clutter_configuration
- `CfgSurfaceCharacters` probabilities per surface must sum to 1 or less. [P]
  https://gist.github.com/pennyworth12345/27786cf99652e0018b9b2f0becaa0638
- Client terrain grid by setting (Arma 3 1.34): Low 50 m (no grass), Standard 25, High 12.5, Very High 6.25,
  Ultra 3.125 (forum post; old). [S] https://community.bistudio.com/wiki/setTerrainGrid
- A tracker report describes the visual gap where clutter ends before the satellite texture looks photographic. [S]
  https://feedback.bistudio.com/T82654
- Bohemia's own guide gives Sahrani's satellite texture as 1 m/px; a terrain guide asks for 1 m/px mapframes. [S]
  https://community.bistudio.com/wiki/Making_Satellite_Texture_and_Mask and
  https://community.bistudio.com/wiki/User:RoF/Sandbox
- A tutorial example: 10 m cell, 5120 m terrain, satellite and mask tiles 512 px with 16 px overlap, texture layer
  40 x 40 m. [S] https://www.instructables.com/How-to-Make-an-Arma-3-Map-With-Terrain-Builder

**Real-world Eastern European terrains by the PMC modding group (comparison for a data-generated map)**
- PMC Poland Krynki: 65.5 x 65.5 km; heightmap grid 4096 x 4096 at 16 m cells; texture layer 64 x 64 m; DEM from
  SRTM 1-arc-second (30 m); colour from ArcGIS World Imagery at 1 m/px, 65,536 x 65,536 px; surface mask 2048 px
  tiles "generated from OpenStreetMap (OSM) landuse and natural areas data"; roads from OSM; 18,245,250 objects. [P]
  https://www.pmctactical.org/arma3/pmc-poland-krynki.php
- PMC Ukraine Donetsk: a highly detailed mask produced 42,241 layer RVMATs and a crash on load, fixed by 2048 px
  mask tiles; road features cut from over 9,000 to 1,200 by dropping "track" roads. [P]
  https://www.pmctactical.org/arma3/dev-diary-pmc-ukraine-donetsk-002.php
- PMC Poland Hulskie: forests from land-use shapes split into three forest types; tree density raised to 125 per
  hectare using Livonia's Terrain Processor settings; under 10 million objects per layer; 6.85 million objects in
  all; trees kept 15 m clear of roads. [P]
  https://www.pmctactical.org/arma3/dev-diary-pmc-poland-hulskie-002.php

**Production notes from Bohemia (Tanoa)**
- The environment department was three people; their work included "retouches of the satellite imagery and surface
  mask", placing objects, sculpting terrain, building roads; benches and bins come only at Alpha; reference trips to
  Fiji produced "more than 60 GB of reference data"; rainforests "need more manual work and placement". [P]
  https://arma3.com/news/report-in-radim-vitek-petr-sedlacek-environment

**Lighting**
- Arma 3's 2016 visual upgrade: a new lighting setup and HDR handling aiming at "natural contrast and saturation";
  clouds "properly affected by the light conditions", cloud light sets "multiplied four-times"; vegetation, building
  and island surface materials and textures re-tuned to the new light. [P]
  https://dev.arma3.com/post/oprep-visual-upgrade

---

## 3. Arma Reforger (Bohemia, Enfusion; Everon 1989, Arland)

- Everon: "a 52km² microstate" recreated "for a 1989 setting". [P] https://arma3.com/news/arma-reforger-out-now
- Field trips: the environment designers "were gradually exposed to Bohemian landscapes on dedicated field trips";
  architecture and signage inspired by 1980s Czechoslovakia; new environment tech includes "rain and wetness,
  streams and lakes, vegetation movement in the wind". [P]
  https://reforger.armaplatform.com/news/interview-with-jan-buchta
- Official terrain tutorial (Bohemia, Reforger site) [P]
  https://reforger.armaplatform.com/news/terrain-tutorial :
  - The satellite map is used for base land colour (albedo); "There should be no actual satellite images used on
    your final terrain"; the final satellite map is made last, after placement (a quick one: grass and rock masks
    filled with the average colour of their `middle_BCR` textures).
  - Surface masks are greyscale 0-1 coverage; at most **5 surfaces per block**; start with 3 basic surfaces.
  - Sample settings "similar to the ones used on Everon": 1.5 m planar resolution; block 33 x 33 vertices (48 x 48
    m); tile 129 x 129 vertices (192 x 192 m); 8193 x 8193 vertices (12,288 m); height resolution 3.125 cm. For own
    data, 2 m is recommended ("what Bohemia uses"); at 4 m "structures can't be placed without holes".
  - Normal maps: "You only need one texel per vertice in the terrain mesh."
  - Generators place sidewalks, verge posts, street lights and shoreline objects; keep hand-placed objects to a
    minimum. Forest generators on polylines; "A few hundred meters in each direction per shape" is a good size; "A
    very large flat area (Kilometers) covered in a forest won't run very well." Undergrowth on forest edges per
    vertex. Manual occluders on ridgelines help.
  - Re-importing the satellite map after road work lets roads show in the distance "without clutter on them".
- The World Editor's Terrain Creation Tool imports the satellite image as the source of long-distance terrain
  texturing. [S] https://community.bistudio.com/wiki/Arma_Reforger:World_Editor:_Terrain_Creation_Tool
- Forest generators fill a polyline or spline and avoid roads, rivers, objects and power lines. [S]
  https://community.bistudio.com/wiki/Arma_Reforger:World_Editor:_Forest_Generator

---

## 4. Squad (Offworld Industries; UE4, UE 5.5 since 9.0)

- Squad began in 2014 as a commercial UE4 successor to the Project Reality mod. [S]
  https://pcgamer.com/how-modders-and-veterans-created-squad-a-military-sim-like-no-other
- Map sizes (community wiki): Gorodok 4340 x 4340 m (18.8 km², "large expansive hilly terrain with some forest
  coverage" near the Dnieper, Smolensk Oblast); Yehorivka 4.2 km (later 5 x 5 km); Narva 2.3 km; Belaya 3.9 km;
  Black Coast size not listed. [S] https://squad.fandom.com/wiki/Gorodok ,
  https://squad.fandom.com/wiki/Yehorivka/fr?oldid=9042 , https://squad.gamepedia.com/Maps?cb=2143& ,
  https://squad.fandom.com/wiki/Black_Coast
- Update 9.0 (3 Sep 2025) moved Squad to UE 5.5 with "a new lighting pipeline", Chaos and MetaSounds; early players
  reported frame drops and stutter, and new system requirements followed. [S]
  https://consolepcgaming.com/squad-update-9-0-arrives-with-unreal-engine-5-5-and-free-weekend-on-steam and
  https://www.gamepressure.com/newsroom/players-struggle-after-squad-unreal-5-update-as-devs-reveal-new-s/z4854c
- **Not public (not found):** Squad SDK landscape settings, layer counts, grass distance, foliage density.

**Take-away:** Squad's Eastern European maps are 2-5 km squares: small enough for hand-painted UE Landscape layers and
foliage. SAM3D's 40 km range (and 150 km far ground) is closer to Arma or a flight sim and cannot be dressed the
Squad way.

---

## 5. Escape from Tarkov (Battlestate Games; Unity; Woods, Customs, Shoreline)

- No developer talk or blog on terrain technology was found. Patch notes show the trade between detail and frame
  rate: 0.16.6.0 "returned some of the previously deleted location detailing on Customs" and optimised Lighthouse;
  1.0.4.5 added an updated player culling system on Lighthouse, Shoreline and Woods. [S]
  https://consolepcgaming.com/escape-from-tarkov-0-16-6-0-patch-notes-revealed and
  https://www.altchar.com/game-news/escape-from-tarkov-patch-1.0.4.5-improves-performance-reworks-weapon-balance-and-fixes-key-issues-aPJVC8b37l9m
- A bot fix (0.13.5) corrected "incorrect calculation of visibility for targets through bushes and trees" (the AI's
  sight and the drawn foliage disagreed). [S]
  https://mein-mmo.de/en/escape-from-tarkov-today-is-the-big-wipe-with-update-0-13-5-patch-notes,1019682

---

## 6. Gray Zone Warfare (MADFINGER Games; UE5; Lamang, 42 km², Laos-inspired jungle)

- 42 km² map. [S] https://www.pcgamesn.com/gray-zone-warfare/early-access-launch-date
- UE5 with Nanite, Lumen and virtual textures. [S]
  https://stopgame.ru/newsdata/60529/gray_zone_warfare_tehnologichnyy_shuter_s_uporom_na_maksimalnyy_realizm
- Hardware ray-traced Lumen added as an option. [S]
  https://wccftech.com/gray-zone-warfare-to-support-nvidia-dlss-3-and-ray-traced-enhanced-lumen/amp/
- MADFINGER on optimisation [P]
  https://gamewave.fr/gray-zone-warfare/gray-zone-warfare-madfinger-nous-detaille-sa-bataille-constante-pour-l-optimisation/ :
  - Every PC renders **the same vegetation**: no compromise on vegetation density or distance, so cover stays the same
    on low settings ("Nous voulons que les PC, quelles que soient leurs spécifications, rendent la même végétation").
  - Lumen is "extrêmement gourmande", too heavy for older GPUs, so they keep two lighting variants and call keeping
    them fair "une bataille constante".

**Take-away (most important for SAM3D):** when sight decides the game, vegetation that blocks sight is gameplay and must
not scale with graphics quality. Only non-blocking detail (grass, small clutter) scales.

---

## 7. Hell Let Loose (Black Matter; UE4; eastern front Kursk, Stalingrad, Kharkov, Smolensk)

- Maps are to-scale real battlefields "recreated using archival aerial photography and satellite imagery" in UE4. [S]
  https://gematsu.com/2021/03/hell-let-loose-coming-to-ps5-xbox-series-in-2021
- The eastern front (Kursk, Stalingrad) came with 1.0 on 27 July 2021. [S]
  https://www.team17.com/news/hell-let-loose-launches-out-of-early-access-with-the-eastern-front
- Mortain dev brief [P] https://devtrackers.gg/hell-let-loose/p/b9e40f81-dev-brief-mortain-weekend-pte-now-live :
  - Terrain from "real-world DEM/height data", compared with period maps, adjusted for play.
  - Earlier maps spawned foliage procedurally from landscape materials, which hurt performance; on Mortain "Every
    tree, bush and blade of grass has been hand-painted" for control of density, variety and species.
  - Buildings reworked with new textures, geometry and shaders, some with up to 100x fewer draw calls; detail normal
    maps on buildings and props.
  - Order of work: capture points (layout), transitional areas, then the art pass over the blockout, then playtests.
- Dev Brief #203 (30 May 2025): Kharkov, Stalingrad and five western maps "were found to be excessively foggy" on
  night variants and "have all been updated to leave longer sightlines"; El Alamein's fog was praised for cutting
  over-long sightlines. [P, via a Steam news repost]
  https://www.eprison.de/spiele/hell-let-loose/steam-news/1800991756345124/5385/96682.html

---

## 8. Enlisted (Darkflow / Gaijin; Dagor Engine; Moscow 1941, Stalingrad, Berlin)

- Enlisted runs on the same Dagor engine as War Thunder. [S] https://gaijinent.com/news/174
- Dagor 5.0 landscape: displacement mapping with tessellation for landscape detail; added roughness, AO and a
  ground-type map (grass, sand, dirt) to the normal, translucency and albedo maps. [S]
  https://warthunder.com/en/devblog/current/5341?page=3
- Dagor's open documentation, Impostor Baker [P]
  https://gaijinentertainment.github.io/DagorEngine/dagor-tools/impostor-baker/impostor_baker.html :
  - A tree impostor is one texture of **9 view slices**; trees may be rotated only to a palette of angles (3 gives
    0/120/240°), with a shadow texture per rotation.
  - Slice height set by the tree's height per quality level: 32 px (any tree), 64 px at 4 m, 128 px at 8 m, 256 px
    at 16 m; the whole texture example is 4096 px wide.
  - The examples switch to the impostor at `range:r=1000` and `range:r=1500` (m).
  - Baked normals and AO, preshadows on by default, transmittance off outside cascade 0.
- **Not public (not found):** Enlisted's own terrain sources, clutter distance or tree density.

---

## 9. Post Scriptum / Squad 44 (Periscope Games; UE4)

- Began as a Squad mod; UE4; later moved to UE 4.27. [S] https://www.moddb.com/news/ps-427-engine-update and
  https://handwiki.org/wiki/Software:Post_Scriptum_(video_game)
- **Not public (not found):** any terrain or foliage technique. Treat as Squad-like (small hand-made UE4 maps).

---

## 10. Microsoft Flight Simulator (Asobo; the far field)

From Lionel Fuentes (Asobo technical director), "Designing the Terrain System of Flight Simulator: Representing the
Earth" (GDC talk slides). [P]
https://www.asobostudio.com/files/inline-images/Designing_Terrain_System_Fuentes_Lionel.pdf
- Inputs: aerial images, DEM (ground height), TIN (photogrammetry), vector data (water, roads, building footprints,
  individual trees). Problems: baked-in clouds and shadows, missing data, photos from different seasons and times.
- Quadtree tiles; requested LoD from estimated screen size and bandwidth; tiles draw with the best data available,
  using a parent's aerial texture until their own arrives; skirts hide cracks, skirt size set by slope.
- Each tile is a set of data "markers" with dependencies (DEM, aerial, water mask, land class, vegetation mask,
  vegetation mesh, synthesized aerial...).
- **Synthesized aerials** where imagery is missing or bad; "Synthesized aerials should be affected by altitude and
  slope" and "should reflect the presence of trees".
- **Vector data rasterized to per-tile textures**, projected on the terrain, with GPU distance fields "for smooth
  fade-outs": used for drawing over the aerial, terraforming, water, and flags for trees/grass/no-buildings.
- Terraforming edits vector shapes with heights and an influence radius, merged with the DEM through a distance field
  (never editing the source DEM); water flattened the same way.
- Per-tile GameRender textures at LoD 15 store surface type, biome ID, vegetation flags; read back to the CPU for
  collisions and tree/grass generation.
- **Trees:** per-tile tree masks (machine learning, precomputed); **Halton sampling** so density can be changed at
  draw time and stays uniform; species by biome rules; height from the DEM; "sample aerial -> set matching tree
  colour. Trees fade out naturally to the aerial!"; drawn as **3D impostors** (octahedral, ref. shaderbits.com),
  "1 draw call per tree specie per tile", 1 vertex per tree; a sample frame had 23 M generated trees, 6.3 M in the
  frustum.
- **Shadows:** cascaded shadow maps near; a top-down heightmap of the terrain ray-marched for big terrain shadows
  over **300 km x 300 km**; screen-space ray-marched shadows for small detail.
- **Ground details:** near, mid and far detail maps (dirt, asphalt, grass) detected in the pixel shader; **grass
  spawned by surface type and coloured with the aerial**.
- Large coordinates: doubles on the CPU, an "anchor space" at the camera on the GPU, inverted depth.
- MSFS 2024: full 3D ground with procedural vegetation in about 30 biomes, 3D tessellated ground materials, about 20
  3D crops. [S] https://news.flightsim.to/fsexpo-2024-key-takeaways-for-microsoft-flight-simulator-2024

---

## 11. DCS World and IL-2 Great Battles (the far field over Europe)

- DCS Cold War Germany (Ugra Media): 980 x 800 km of 1980s central Europe (both Germanys, Poland, Czechoslovakia, the
  Low Countries), "accurate reproduction of the road and rail network", power plants and grid; first release with
  "summer textures", "Spring, Autumn, and Winter textures" later. [P]
  https://www.digitalcombatsimulator.com/en/products/terrains/cold_war_germany_terrain/
- Its storage: 205 GB installed, 32 GB RAM and 8 GB VRAM recommended (phase 1: 110 GB). [S]
  https://stormbirds.blog/2025/04/15/dcs-cold-war-germany-faq-confirms-large-storage-footprint-and-post-launch-details/
- DCS Kola (Orbx): winter textures applied automatically by the mission's date. [S]
  https://fselite.net/content/orbx-updates-dcs-kola-map-with-dynamic-winter-textures/ ; one year on: VRAM and
  performance optimisation, "Enhanced ground textures for low-level flight" planned. [P]
  https://www.digitalcombatsimulator.com/en/news/2025-06-13/
- DCS newer maps: ground textures between 1 and 64 m per pixel depending on the area and clipmap level (forum repost
  of a development update; unverified). [S] https://forum.dcs.world/topic/113346-dcs-world-15-and-2-discussion/page/25/
- DCS Afghanistan: high-resolution satellite imagery integrated for terrain textures. [S]
  https://www.amvi.it/news5541-dcs-world-newsletter-29032024.html
- DCS Normandy 2.0: 2.4 times Normandy 1, built with the latest Terrain Development Kit so performance stays
  comparable. [S] https://key.aero/article/dcs-normandy-2-enters-early-access
- IL-2 Great Battles, Moscow 1941: "forest types reflect the real forestation in the area" (pine, spruce, leaf,
  mixed drawn as separate forest-type maps). [P] https://il2sturmovik.com/news/232/dev-blog-128/
- IL-2 Moscow city: German wartime aerial photos used because Soviet data on the period layout was limited; "scenery
  objects are designed to be viewed from a distance of 5-15 km"; unreachable districts at a "middle level" for 1-3 km
  with zoom. [P] https://il2sturmovik.com/news/228/dev-blog-127/

---

## 12. Cross-references: technique sources

- **Far Cry 5** (Ubisoft, Houdini): "biome recipes that react to the physical features of the land"; forests denser
  on north faces (humidity, sun exposure), grassland on exposed hills, species "fight for the ground"; grass tinted by
  terrain humidity and oriented by a wind vector map; tools for freshwater, cliffs, power lines. [P]
  https://blog.playstation.com/2018/03/22/the-procedural-world-generation-of-far-cry-5/ and
  https://80.lv/articles/houdini-procedural-world-generation-of-far-cry-5/
- **Horizon Zero Dawn** (Guerrilla): GPU placement rules authored in a graph; the dense world built around the player
  at runtime, so artists can repaint a tree line or move a road. [P]
  https://www.guerrilla-games.com/read/gpu-based-procedural-placement-in-horizon-zero-dawn
- **UE foliage culling** [P] https://dev.epicgames.com/documentation/en-us/unreal-engine/foliage-mode-in-unreal-engine :
  instances render as clusters in one draw call and change LOD per cluster; an end cull distance alone makes groups
  vanish abruptly; a start distance plus `PerInstanceFadeAmount` fades per instance; "Nanite-enabled meshes are not
  affected by culling distance and instance fading"; density can scale with `foliage.DensityScale`.
- **World Partition HLOD** "Instancing" layers replace meshes by ISMs at their lowest LOD, "ideal for imposter
  meshes, such as trees and foliage". [S]
  https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine
- **Runtime Virtual Texturing**: complex landscape materials cache their shading; splines, decals and meshes blend
  into the same RVT. [S] https://dev.epicgames.com/documentation/unreal-engine/runtimevirtual-texturing-quick-start-in-unreal-engine ;
  any primitive can draw into an RVT through `UPrimitiveComponent::RuntimeVirtualTextures` ("Draw in Virtual
  Textures") if its material has an RVT output. [S]
  https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Components/UPrimitiveComponent/RuntimeVirtualTextures?application_version=5.5
- **Landscape grass** spawns only on Landscape actors (Epic staff reply; old). [S]
  https://forums.unrealengine.com/t/help-with-landscape-grass-output-on-static-mesh/147648/2
- **UE 5.7**: Nanite Foliage (Nanite Assemblies, Skinning, Voxels) is Experimental with "incomplete support for
  physics, collisions, and wind animation"; the Procedural Vegetation Editor is Experimental; PCG is production-ready.
  [P] https://digitalproduction.com/2025/11/12/unreal-engine-5-7-foliage-pcg-and-in-editor-ai/
- **Sky Atmosphere**: aerial perspective "View Distance Scale"; Mie scattering is the haze ("causing the clarity of
  the sky to appear hazy"); height fog adds on top (set its inscattering colours to black to let the atmosphere drive
  the colour); the aerial-perspective LUT is a froxel 3D texture whose size costs memory. [P]
  https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine

---

## 13. Comparison table

| Game | Map | Height spacing | Ground colour far | Detail near | Clutter / grass | Trees far |
|---|---|---|---|---|---|---|
| Arma 2/DayZ Chernarus | 15.36 km, ~225 km² | n/f | aerial orthophoto "super-texture" (P) | detail textures to 15-40 m (P) | 1 per 1 m² cell, 125 m (P) | simplified trees (P) |
| Arma 3 (PMC real data) | 65.5 km | 16 m (P) | 1 m/px imagery (P) | 5 surfaces per tile (P) | per surface | objects, 125 trees/ha (P) |
| Arma Reforger Everon | 52 km² | 2 m recommended (P) | painted albedo map, no photos (P) | 5 surfaces per block (P) | per surface | forest generators (P) |
| Squad | 2.3-5 km | n/f | UE Landscape | n/f | n/f | n/f |
| STALKER 2 | ~64 km² | n/f | hand-made UE5 | n/f | costliest setting (S) | sprite trees (S, user) |
| Gray Zone Warfare | 42 km² | n/f | UE5 virtual textures (S) | n/f | same on all settings (P) | same on all settings (P) |
| Hell Let Loose | ~2 km class | real DEM (P) | UE4 Landscape | detail normals (P) | hand-painted (P) | hand-painted (P) |
| Enlisted (Dagor) | n/f | n/f | tessellated displacement (S) | ground-type map (S) | n/f | 9-view impostors from 1-1.5 km (P) |
| MSFS | Earth | DEM / TIN | aerials, synthesized where bad (P) | near/mid/far detail maps (P) | by surface type, aerial-coloured (P) | octahedral impostors, aerial-coloured (P) |
| DCS | to 980 x 800 km | n/f | 1-64 m/px clipmap (S) | n/f | n/f | n/f |

(n/f: not found in public sources.)

---

## 14. Lessons for SAM3D (a data-generated map drawn in UE 5.8)

SAM3D facts used below [SAM3D]: simcore makes the ground (10 m and 20 m detail areas, 50 m map proper, far ground to
150 km), a 25 m land-cover raster with crop parcels of 250-700 x 80-250 m and a furrow heading per 3 km square,
planted trees (7 m spacing in detail areas, rows on wood edges, masses beyond; Kesmark about 150,000 trees under a
400,000 budget, `data/scenery/map_dressing.json`), buildings, and line of sight against the same ground and obstacle
heights; Unreal draws the ground as ProceduralMeshComponent tiles coloured per vertex, trees and buildings as HISM.

1. **One colour source for the whole view.** Every game that looks right far away drapes one colour image over
   the terrain and makes everything else agree with it: Chernarus' aerial super-texture (killscreen), Reforger's albedo
   map (terrain tutorial), Arma's satellite texture mixed into near detail "right up to the player's feet"
   (islandconfigs), MSFS' aerials with trees and grass coloured from them (Asobo). SAM3D has no photographs of 1985
   Pannonia, so do what MSFS does where aerials are missing and Reforger does by policy: **synthesize the colour map**
   from simcore's data (cover class, crop, parcel edges, furrow heading, woods, gardens, roads, water), with slope,
   height and tree presence shading it. Per-vertex colour cannot carry it: at 50 m spacing a 250 x 80 m parcel is a
   handful of vertices [SAM3D]. Bake it per ground tile into textures (or a Runtime Virtual Texture, which any
   primitive can draw into; UE RVT docs).

2. **Vector data, not a staircase raster.** MSFS rasterizes roads, water and masks per tile from vectors with
   distance fields for smooth edges (Asobo). Field edges, road verges and wood margins in the colour map should be
   drawn from simcore's polygons and polylines at the colour map's resolution, not from the 25 m cover cells.

3. **Few blended surfaces per pixel, chosen by data.** Arma 3 allows 5 surfaces per tile, Arma 2 six (gist);
   Reforger 5 per block and advises starting with 3 (terrain tutorial); Dagor adds a ground-type map (War Thunder
   devblog); MSFS stores surface type and biome ID per tile (Asobo). For SAM3D: a material-ID (surface) texture from
   simcore's cover and crop classes, plus at most 4-5 detail sets per tile blended near the camera: stubble, ploughed
   soil (normal map turned to the parcel's furrow heading), standing or cut maize, pasture/meadow, forest floor,
   marsh, garden soil, asphalt, dirt track, gravel.

4. **Detail near, colour far, and a mid layer for the gap.** Arma fades its detail textures out between about 15 and
   40 m and lets the satellite colour take over (islandconfigs); Bohemia's tracker records the visible gap where
   clutter ends before the colour map looks detailed (T82654); MSFS uses near, mid and far detail maps (Asobo). Use
   three ranges in the ground material: near (0-50 m) full PBR detail sets, mid (50 m-2 km) a macro-variation layer
   (soil moisture, tractor tracks, crop-row stripes at 10-100 m scale) over the colour map, far (2 km and beyond) the
   colour map alone. [SAM3D proposal]

5. **Height spacing: keep simcore's grid, seat things with pads.** Reforger uses about 2 m and warns that at 4 m
   buildings cannot be placed without holes (terrain tutorial); PMC's real-data Arma terrains run 16 m cells under 1 m
   imagery (Krynki). SAM3D's 10 m near spacing is coarse for seating houses, revetments and launchers, but a render-only
   finer mesh would make the drawn ground disagree with the line of sight simcore computes. Do what MSFS does:
   terraform by vector shapes (pads, road beds, ditches, revetments) merged with distance fields (Asobo), and do it in
   simcore so the sight lines see the same ground [SAM3D]. Detail displacement in the material, if any, stays a few
   decimetres.

6. **Clutter radius about 100 m, 1 per m², coloured by the map.** Arma/DayZ: one clutter model per 1 m² cell to 125
   m, with 150 m and above "very performance-heavy" (islandconfigs, DayZ wiki). MSFS spawns grass by surface type and
   colours it from the aerial (Asobo). For 15 October most of the plain is stubble, ploughed soil or cut maize, so the
   clutter is short and cheap; standing maize is better as field-scale crop blocks (MSFS 2024 lists about 20 3D crops,
   flightsim.to) than as clutter. Generate clutter from simcore's cover raster on the CPU or GPU, because UE's
   Landscape Grass only spawns on Landscape actors (Epic forum reply) and SAM3D's ground is a procedural mesh.

7. **Sight-blocking vegetation never scales with quality.** Gray Zone Warfare renders the same vegetation on every PC
   so cover is fair (gamewave); Tarkov had to fix bots seeing through bushes and trees (mein-mmo); Hell Let Loose
   retuned fog for sightlines (Dev Brief #203). In SAM3D the sight lines come from simcore's obstacle heights, so every
   tree row, wood mass and building that stands in a sight line must be drawn at every quality setting out to the
   crew's visual range; only grass, small clutter, shadows and lighting quality may scale. A test can compare the
   drawn obstacles with simcore's obstacle raster [SAM3D].

8. **Four distance rings with explicit budgets.** Dagor switches trees to 9-view impostors at 1-1.5 km (Impostor
   Baker); MSFS draws millions of octahedral impostors at one vertex each and one draw call per species per tile, 6.3 M
   in the frustum in a sample frame (Asobo); IL-2 builds scenery to be seen from 5-15 km and middle detail for 1-3 km
   (IL-2 dev blog 127). Proposed SAM3D rings [SAM3D proposal]:
   - near 0-1.5 km: full tree meshes (HISM with LODs, per-instance fade; or Nanite foliage once it leaves
     Experimental), clutter to about 100 m, buildings full;
   - mid 1.5-6 km: octahedral impostors per species and autumn tint, buildings at a low LOD;
   - far 6-40 km: wood masses and building blocks shaded to match the colour map;
   - horizon 40-150 km: far ground with its colour map and the atmosphere's aerial perspective.
   UE notes: an end cull distance alone pops whole clusters, so use a start distance and `PerInstanceFadeAmount`; Nanite
   meshes ignore cull distance (UE foliage docs); World Partition HLOD "Instancing" layers suit impostor trees (UE HLOD
   docs).

9. **Trees take their colour from the same map.** MSFS samples the aerial for each tree so "trees fade out naturally
   to the aerial" (Asobo). Give each species' autumn tint (oak, poplar, black locust, pine) a value the colour map's
   forest and tree-row cells average to, and tint impostors and masses from the colour map at their footprint, so
   there is no seam where meshes become impostors and impostors become masses.

10. **Tree density from rules, checked against references.** IL-2 drew forest-type maps from real forestation (dev
    blog 128); PMC used 125 trees per hectare from Livonia's Terrain Processor settings (Hulskie); Far Cry 5's biome
    recipes read slope, humidity and sun exposure (PlayStation blog). SAM3D's 7 m spacing is about 204 trees per
    hectare [SAM3D arithmetic: 10,000 m² / 49 m²]; check it against Hungarian forestry stand data in the terrain
    research track before raising budgets, and let species follow soil, water and planting history (poplar on river
    banks, black locust on sand, planted pine) as the dressing rules already begin to do.

11. **Generate the ordinary, hand-place the special, as data.** Reforger keeps hand placement minimal and lets
    generators place sidewalks, verge posts, street lights and shoreline objects (terrain tutorial); STALKER 2 hand-made
    64 km² at studio scale (PC Gamer); HLL went from procedural to hand-painted foliage on a 2 km map for control
    (Mortain brief); Tanoa's rainforest needed manual placement (Arma 3 report). SAM3D: rules for the whole country,
    plus hand overrides stored as data (a hedge, a haystack, a field shed, a well) only at battery positions and
    detail areas, so a changed map needs no hand edits elsewhere.

12. **Roadsides and villages as kits with rules.** Reforger's road generators (terrain tutorial), Chernarus' smaller
    settlements and simplified trees for play (killscreen), HLL's buildings at up to 100x fewer draw calls with detail
    normal maps (Mortain brief). For SAM3D: a roadside kit placed by simcore along each road class (kilometre stones,
    tree rows, ditches, telephone and power poles, bus shelters, signs) and a village kit (house variants, fences,
    gardens, fruit trees, outbuildings); each building one or two draw calls with a shared trim/detail-normal material.
    Which objects belong on a 1985 Hungarian roadside is a question for the reference-image research track.

13. **Field work and period imagery come first.** STALKER 2's team visited the Zone repeatedly (novyny.live); Bohemia
    sent designers on field trips (Reforger interview) and gathered 60 GB of references for Tanoa (Arma 3 report);
    Chernarus is its designer's home region (killscreen); IL-2 used wartime aerial photographs for the period layout
    (dev blog 127). SAM3D's equivalents: period aerial and ground photographs of the Great Plain in the 1980s and
    present-day photographs of the same land, catalogued by cover class and season, before tuning colours.

14. **Season from the scenario's date.** DCS Kola applies its winter textures from the mission's date (FSElite); DCS
    Cold War Germany ships summer first, other seasons later (DCS product page). SAM3D already has a date per scenario:
    drive crop state (stubble, ploughed, maize standing or cut, sunflower cut), leaf tint and leaf fall from it, with an
    autumn set (15 October 1985) and a summer set first [SAM3D].

15. **Haze is physical, and it must not hide what the optics see.** UE's Sky Atmosphere gives aerial perspective in
    kilometres with Mie scattering as the haze, height fog on top (UE docs); Arma 3's light upgrade aimed at "natural
    contrast and saturation" and four times the cloud light sets (OPREP); HLL found its fog too heavy for sightlines
    (Dev Brief #203). Drive the Mie density and height fog from simcore's weather visibility, so the haze the player
    sees matches the visual range simcore uses (`data/gameplay/visual_range.json`) [SAM3D].

16. **Distant terrain shadows matter in low sun.** MSFS ray-marches a top-down heightmap for terrain shadows over 300
    x 300 km, with cascades near and screen-space shadows for small detail (Asobo). For October dawn and dusk over the
    northern hills, check that virtual shadow maps and Lumen give the far ground and wood masses their shadows; if not,
    a heightmap shadow pass from simcore's far ground is a known technique [SAM3D].

17. **Budgets, and the cost of Lumen.** GZW keeps a second lighting path because Lumen is too heavy for older GPUs
    (gamewave); STALKER 2's foliage setting is reported as the most expensive (settings guides, S); Squad's UE5 move brought stutter and
    new requirements (gamepressure, S); DCS Cold War Germany asks 32 GB RAM and 8 GB VRAM (stormbirds, S). Set SAM3D's
    budgets in milliseconds per pass (ground, trees, clutter, shadows, Lumen, atmosphere) on a named target GPU and
    measure them with `-SimMapShots`-style captures before adding detail; keep a non-Lumen fallback in mind [SAM3D].

18. **Do not depend on Landscape-only systems; prefer stable UE features.** Landscape Grass spawns only on Landscape
    actors (Epic forum); Nanite Foliage and the Procedural Vegetation Editor are Experimental in 5.7 while PCG is
    production-ready (digitalproduction). SAM3D's placement belongs in simcore anyway (line of sight), so Unreal should
    take simcore's instance lists into HISM/ISM and HLOD-style impostors, and use RVT and PCG only as drawing aids.

---

## 15. Gaps

- No public figures for STALKER 2's landscape resolution, layers, grass or impostor distances; Squad SDK landscape
  settings; Tarkov's terrain; Enlisted's terrain; Post Scriptum's terrain; Gray Zone Warfare's landscape and foliage
  budgets; Hell Let Loose's eastern-front map method; DCS Cold War Germany's texture resolution.
- Bohemia's wiki, forums and tracker block automated reading, so their numbers here are [S] (search summaries); a person
  can confirm them in a browser: Common Terrain Creation Problems, DayZ grass-clutter configuration, setTerrainGrid,
  Reforger Terrain Creation Tool and Forest Generator.
- The GDC slides for Far Cry 5 and Horizon Zero Dawn (density rules, budgets in ms) were not read; the vault and PDF
  links exist (GDC Vault 1025557; guerrilla-games.com).
- Real-world tree densities and Hungarian roadside and village furniture of 1985 are for the reference-image track.
