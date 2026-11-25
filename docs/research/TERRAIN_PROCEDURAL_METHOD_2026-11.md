# Test plots first, then the whole map by rules: a method for SAM3D's rural world

Research notes, 5 November 2026. The question: how to build a large, realistic rural world from data by perfecting
small test plots (about 50 x 50 m per land type) against photographs and then applying each plot's rules to every
matching part of the map. SAM3D's map is a 40 x 40 km generated piece of the Great Hungarian Plain on 15 October
1985. simcore already owns the data (25 m land-cover raster, crops and furrow headings, vector roads, rivers and
railways, settlements, woods with stand types, every tree and building, heights on a 50 m grid and 10-20 m in detail
areas). Unreal draws it.

Not repeated here (read them first): the AA/AAA map survey (`TERRAIN_AAA_MAPS_2026-11.md`), the reference photos,
traits, field sizes, the 1985 crop mix and the October 1985 crop states (`TERRAIN_REFERENCES_2026-11.md`), soil colour
targets and the sown-wheat look (`TERRAIN_TEXTURES_2026-11.md`), the ground material's blending, anti-tiling and RVT
(`TERRAIN_GROUND_MATERIAL_2026-11.md`, `TERRAIN_GEOMETRY_2026-11.md`), and the stand, edge and species numbers already
in `docs/scenery/VEGETATION.md` section 1.

## How to read these notes

- Every claim carries its link. Tags say how it was checked:
  - **[P]** read on the page or in the document's own text.
  - **[S]** seen only in a search engine's summary of the page (the page blocked reading, or only its abstract was
    reached). Lower confidence.
  - **[SAM3D]** our own data, arithmetic or proposal, not a source claim.
- Nothing was downloaded on purpose, installed or signed up for. Four PDFs that the page reader could not parse were
  kept by the reader in its own cache and read as text there (Kevey & Riezing 2023, Erdészeti Lapok 1961/4, Andersson
  2007, Poór et al. 2009).
- Real-world numbers are for judging the look. Pannonia is fictional, so they are targets for the eye and ranges for
  tests, not system data for `data/systems/`.

---

## 0. Answers in short

1. **Everyone defines a land type once as data and applies it by rule; nobody hand-places a whole open world.**
   Horizon Zero Dawn's "ecotopes" are density graphs per asset over a stack of 2D world maps, discretised by an ordered
   dither whose cell is the asset's footprint (section 1.1). Ghost Recon Wildlands defined 11 ecosystems after a
   15,000-photo field trip and built its world in layers, each the base for the next: terrain, rivers, roads, railways,
   forests, settlements (1.2). Arma Reforger's forest generator has edge ("outline") levels set by distance from the
   polygon's edge (1.4). UE's PCG Biome Core and MSFS 2024 store a biome as a definition plus weighted assets with
   density per hectare (1.5, 1.8).
2. **The vertical slice is a small area made perfect and then generalised**, and validation is field photographs,
   mood boards per environment, early reviews on target hardware, and screenshot comparison against approved images
   (1.11). Academic work makes the same idea exact: learn the point statistics of an example region and synthesise
   them elsewhere (WorldBrush 2015, Ecormier-Nocca 2019; section 2.4). That is the plot method in mathematical form.
3. **Scatter:** blue noise (Poisson-disk) for single trees, lattices with jitter for plantations, Thomas or Matérn
   clusters for shrubs, regeneration and coppice stools, rows as 1D jittered spacing with gaps. Change density by
   thresholding a fixed, ordered point set so the trees that stay do not move (HZD's dither thresholds, Kopf's
   progressive Wang tiles, MSFS's Halton sampling). Seed every cell from an integer hash of its position, never from
   generation order (2.1-2.7).
4. **Real numbers exist for most plots** (section 3): oak-hornbeam 22-28 m with a 15-20 m lower canopy; black locust
   2000-4000 stems/ha planted, 900-1000 at 25-30 years; hybrid poplar 6 x 6 m (278/ha) in a Hungarian plantation;
   white willow at most 20-25 m; forest edge herb fringe at least 3 m, shrub belt at least 5 m, mantle at least 10 m
   (15-20 optimal); a 1950s plain poplar row at 5 m in the row; hedges 5-10 m wide for 3-5 rows; reed fringes of an
   oxbow only 0.5-1 m (Szarvas, 1980) and 1-5 m on canals; flood dykes with a 4 m crown and 1:3 / 1:2.5 slopes;
   kunhalom mounds 20-90 m across and 0.5-12 m high; sand dunes 6-8 m high and about 150 m long; floodplain relief
   0-3 m per km². **Not found:** canal and roadside ditch dimensions, poplar plantation heights, willow stems/ha,
   headland widths of 1985, mid-October wheat seedling height (section 7).
5. **Relief:** keep the data's large scale fixed by adding only band-limited, zero-mean meso relief and shape stamps
   (old channels, levees, kunhalmok, dykes, road embankments and ditches, canals) merged with distance fields, and put
   anything taller than about 0.5 m into simcore, because the sight lines read simcore's ground (section 4).
6. **Texturing:** a low-frequency colour map synthesised from simcore's vectors plus at most four tiled detail layers
   per pixel, chosen per pixel by rules on cover, crop state, distance to edges and wetness; the recipe that sets a
   plot's look is the only place its colours and layers are written, so tuning a plot retunes the map (section 5).
7. **For SAM3D** (section 6): 17 plots and 7 transition plots, each a recipe file in `data/scenery/recipes/`, built in
   a synthetic plot world by the same code as the map, rendered from 1.7 m, 25 m and 150 m oblique beside its
   photographs, measured by simcore tests, and also found on the Kesmark map by anchor so the same recipe is judged in
   place. Start with ploughland, maize stover and standing maize, then the forest edge and the tree row.

---

## 1. Industry practice: define a land type once, apply it everywhere

### 1.1 Horizon Zero Dawn (Guerrilla, Decima): ecotopes, density maps and dithering

From Jaap van Muijden's GDC 2017 slides, read through their SlideShare transcript [P]
(https://www.slideshare.net/slideshow/gpubased-procedural-placement-in-horizon-zero-dawn/72897079; deck PDF
https://www.guerrilla-games.com/media/News/Files/GDC2017_VanMuijden_GPUBasedProceduralPlacementInHorizonZeroDawn.pdf;
abstract [P] https://www.guerrilla-games.com/read/gpu-based-procedural-placement-in-horizon-zero-dawn):

- **World data is a stack of 2D maps** streamed in sections, about 4 MB per km² (about 32 bits per m²): placement maps
  for trees (1.0 m), block bushes and undergrowth (1.0 m), pickups and resources (2.0 m); topology maps for roads,
  water and objects (0.5 m); ecotope maps A-H (2.0 m); terrain, object and water heights (0.5 m); erosion, variance and
  cavity maps (0.5-2.0 m). Many are seeded by baking in World Machine or Houdini and then hand-painted in the in-game
  editor.
- **Logic is a node graph per asset or asset group** that outputs a density. Example: tree placement multiplied by
  the object, road and water masks removes trees from rocks, roads and water; an inverted "clearing" map thins trees
  inside a forest. **Edge trees and inner trees are separate assets**, edge trees taking densities near zero (the
  forest's rim) and inner trees near one.
- **Each asset has a footprint** (effective diameter) that sets spacing: about 6 m for trees, about 1 m for
  undergrowth (speaker notes).
- **Density becomes points by ordered dithering** in blocks that scale with footprint, from 128 x 128 m for trees to
  32 x 32 m for grass, each block a fixed 64 x 64 density map. The thresholds are spread evenly between 0 and 1 and
  ordered to keep consecutive thresholds far apart; the pattern is scaled so its spacing equals the footprint; a
  user noise offset breaks visible lines at high density.
- **Collision between layers without read-back**: layers with the same footprint split the density range into
  min/max slices so a sample point lands in only one layer. Read-back collision for different footprints was a backup
  and did not ship.
- **Ecotopes** decide assets and their placement, colourisation, weather, effects, sound and wildlife; area logic is
  built once and shared between ecotopes.
- **Deterministic and "locally stable"**: random numbers come from the pattern index and an ID; the only
  non-deterministic step (GPU output order) is fixed by passing a stencil point and tile ID to each placement.
- **Scale and team**: 500+ asset types, 100,000+ objects in the scene, about 250 µs of GPU time on average; nature
  assets by three people, ecotopes by one.
- **Iteration**: an in-game GPU interpreter lets artists step through density calculations and inspect layers; used
  "throughout the development of horizon".

**Lesson for SAM3D [SAM3D]:** separate edge assets from inner assets and drive them by distance to the edge; give
every recipe a footprint; make density a threshold on a fixed ordered point set; debug by showing each rule's
density as an overlay.

### 1.2 Ghost Recon Wildlands (Ubisoft Paris): ecosystems, layered tools, rules matched to materials

- **Reference first**: four teams went to Bolivia and brought back about 15,000 photos and 15 hours of video
  [P] https://80.lv/articles/procedural-technology-in-ghost-recon-wildlands/ . The team analysed heat, drought,
  wetness, erosion, rocks, vegetation and colour, extracted biomes and **defined 11 ecosystems**; mood boards per
  environment covered landscape, long and close range, landmarks, ground, vegetation, rocks and a colour key [P]
  https://80.lv/articles/landscape-and-material-pipeline-of-ghost-recon-wildlands/ .
- **Layer order**: terrain, rivers, roads, railways, forests, settlements, mines; "Each layer gave us the base for the
  next layer". Tools split by task with clear inputs and outputs (the railway tool calls the bridge tool). Procedural
  data must be **locked before manual edits**, or the next update loses them. Tiles integrated early into an Xbox One
  demo for console reviews [P] (procedural technology article above).
- **Terrain materials by rule**: "We knew we would not be able to paint manually all materials on the entire world."
  Placement read slope, height, flow, density, deposition and curvature maps; each Terrain Procedural Set holds a
  material that blends **four** foreground textures plus a distant "vista" texture made to match [P] (landscape
  article above).
- **Vegetation**: grass follows the terrain's splatting; a Houdini tool spreads trees and bushes. Base rules set
  density, scale, slope and spacing; a second layer adds sun direction, species per material splat, and
  inclusion/exclusion (flow maps, river beds, cliffs); rules combine with a delete threshold; roads, railways, water
  and footpaths are removed automatically. **Large trees spawn medium trees, which spawn small trees and bushes**
  (a cascade). Each species has a few variants and "avoid having too many different species per rule". Terrain
  artists matched material distribution rules to vegetation rules. LOD distances shorten where vegetation is dense
  [P] https://80.lv/articles/vegetation-generation-in-ghost-recon-wildlands/ .
- **Determinism caveat**: World Machine erosion at 64k x 64k ran on 80 machines; a low-resolution pre-erosion on one
  machine fixed about 90% of tile seams, but generation was "not fully deterministic" [P] (procedural technology
  article).

### 1.3 Far Cry 5 (Ubisoft Montreal): biome recipes reacting to the land

- Tools for biomes, terrain texturing, freshwater networks, cliffs, power lines, fog density and the world map; the
  talk ends with a map of biomes "layered on top of each other" [S]
  https://shazoo.ru/2020/11/26/102635/gejmdev-procedurnaya-generaciya-mira-far-cry-5 ; [P]
  https://80.lv/articles/houdini-procedural-world-generation-of-far-cry-5/ .
- Forests denser on north faces where soil humidity is shaded from the sun; grass tinted by humidity and oriented by a
  wind vector map [P via the AAA notes] https://blog.playstation.com/2018/03/22/the-procedural-world-generation-of-far-cry-5/ .
- The GDC talk itself (https://gdcvault.com/play/1025557/Procedural-World-Generation-of-Far) was not read; its rule
  details are a gap.

### 1.4 Arma Reforger (Bohemia, Enfusion): forest generator with edge levels

- The generator fills a polyline or spline (wiki page blocked, 403; [S]
  https://community.bistudio.com/wiki/Arma_Reforger:World_Editor:_Forest_Generator ). Density is per hectare
  ("CDENSHA") [S].
- The script API names the parts [P]
  https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceForestGeneratorOutline.html :
  an **outline** level has an outline type, **minimum and maximum distance** (from the polygon's edge), cluster
  strength and radius; every level has tree groups, density, group probabilities and an **outline scale curve over a
  distance** (trees shrink toward the edge). Top and bottom levels and **clusters** (which trees, at which density)
  complete the hierarchy (cluster class page [S]).
- Bohemia's tutorial: generators per shape of "a few hundred meters in each direction"; undergrowth on forest edges
  per vertex [P via the AAA notes] https://reforger.armaplatform.com/news/terrain-tutorial .
- A community tool, "Forest Featherer", finds edges by neighbour density and thins and shrinks edge objects [S]
  https://reforger.armaplatform.com/workshop/684876344DA83476 .

### 1.5 Unreal Engine PCG Biome Core (Experimental)

From Epic's reference [P]
https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-reference-guide-in-unreal-engine
and overview [P]
https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-overview-guide-in-unreal-engine :

- A biome is a **Biome Definition** (name, unique colour, priority) plus **Biome Assets**; generators (with their own
  priority) make root points, filters keep or drop them (density min/max, height min/max, distance to water), and
  assets are chosen by weight. Biomes are placed by volumes, splines or textures.
- **Overlap** is resolved by priority (biome first, then generator; lower value wins). **Transitions** have a blending
  range that widens the biome's bounds, with "Biome Blending Noise and Density" deciding how many points survive in
  that band.
- **Child assets** are spawned around a parent by its transform graph, recursively to a maximum depth, with a rate
  multiplier and weights (the Wildlands cascade as data).
- Landscape paint layers can choose generator subtypes. The plugin is labelled Experimental.
- PCG's hierarchical generation runs coarse and fine grids (small meshes on smaller grids), and runtime generation
  generates and cleans up around sources with radii per grid size [P]
  https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine . The docs
  read say nothing on seeds; a forum answer warns of "minor differences based on hardware/platform" [S]
  https://forums.unrealengine.com/t/cross-partition-coherent-generation-in-pcg-with-hierarchical-runtime-generation/2728364 .

**For SAM3D [SAM3D]:** the data model (definition, weighted assets, priorities, blending band, children) is worth
copying. The plugin is not: trees must come from simcore for the sight lines, and multiplayer clients must agree.

### 1.6 Houdini pipelines

- Heightfield nodes build masks by feature (slope, height, curvature, direction, occlusion) [S]
  https://www.sidefx.com/ja/docs/houdini/unreal/landscape/index.html ; erosion is run "in multiple iterations with
  different nodes", with a feature size for channels [P] https://www.sidefx.com/docs/houdini/heightfields_cop/erosion.html ;
  Heightfield Scatter places points by mask with a hierarchical option against overlap between scales [S]
  https://www.sidefx.com/docs/houdini/heightfields/creation.html .
- Wildlands and Far Cry 5 both built their rule tools in Houdini (1.2, 1.3). The pattern is the same as HZD's: masks
  in, densities out, points by rule.

### 1.7 CityEngine (Esri): rules on shapes, attributes from data, seeds per shape

- A CGA rule file has a start rule applied to every shape of a kind; street-network shapes get default start rules
  (Lot, Street, Sidewalk) at creation; parameters marked `attr` are attributes whose CGA values are defaults, editable
  per shape [S] https://doc.arcgis.com/en/cityengine/latest/help/help-working-with-rules.htm .
- `seedian` is a per-shape seed that rules read and set, so randomness is tied to the shape, not the run [S]
  https://doc.arcgis.com/en/cityengine/latest/cga/cga-seedian-attribute.htm .
- **Lesson [SAM3D]:** a village garden, a street or a road verge is a rule on a shape (a plot, a street segment)
  whose attributes come from simcore's data, with its seed from the shape's id.

### 1.8 Microsoft Flight Simulator 2024: species and biomes as XML

- About 30 biomes with procedural 3D vegetation [S]
  https://pcgamer.com/games/sim/more-details-on-ms-flight-simulator-2024-including-full-3d-landscapes-in-30-biomes .
- A **species** file lists variations with a spawn ratio and a size range in metres, an impostor setup and a colour
  mask for tinting from the aerial [P]
  https://docs.flightsimulator.com/msfs2024/retail/content-configuration/environment/vegetation/species-xml-properties/index.md .
- A **biome** file gives **`speciesInstancesPerHectar`**, species with `spawnRatio` and a colour multiplier, whether
  the biome is for artificial areas only, and where it applies (eco regions, potential natural vegetation) [P]
  https://docs.flightsimulator.com/msfs2024/retail/content-configuration/environment/vegetation/biome-xml-properties/index.md .
  Polygons in the scenery editor can override biomes locally [S]
  https://docs.flightsimulator.com/msfs2024/retail/samples-tutorials/samples/sceneries/simplebiomes/index.md .
- A third-party add-on's complaint: stock vegetation "too uniform", the same heights and similar densities [S]
  https://orbxdirect.com/product/burningbluedesign-global-vegetation . Size and density need sourced ranges, not one
  value.

### 1.9 Frostbite: the texturing lineage

- Andersson (SIGGRAPH 2007 course notes) [P]
  https://www.advances.realtimerendering.com/s2007/Andersson-TerrainRendering(Siggraph07)-CourseNotes.pdf : the
  common approach combines "low-resolution unique color maps" for low frequencies with "multiple tiled detail maps"
  for high frequencies; in Battlefield 2 a terrain patch could have up to six tiling detail maps blended by one or two
  three-channel detail masks. Frostbite exposed three per-pixel parameters to graph-based terrain shaders: **height,
  slope and world-space normal**, and shaders decide both how a material looks and where it appears (masking).
- Widmark (GDC 2012, Battlefield 3) [P]
  https://www.slideshare.net/slideshow/terrain-in-battlefield-3-a-modern-complete-and-scalable-system/11989404 :
  shaders splatted by painted masks with a colour map overlay; decoration (trees, rocks, grass) distributed
  automatically by the masks; procedural virtual texture at about 32 samples per metre, with detail splatting to
  500-1000 samples per metre only within 50-100 m; real-time editing in the editor, external tools (World Machine)
  for large operations.

### 1.10 The common pattern

| Question | What the studies do |
|---|---|
| What a rule reads | 2D maps: cover/material masks, roads, water, objects, height, slope, curvature, flow, erosion, wetness, sun direction, painted overrides (HZD, Wildlands, Frostbite, Houdini). Vector shapes and their attributes (CityEngine, PCG splines, Reforger polygons). |
| How a rule is stored | A graph or table per asset or asset group: density, footprint, scale, filters, weights (HZD, PCG Biome, MSFS XML). |
| Layering order | Ground first, then water, roads and rails, then forests, then settlements (Wildlands). Within vegetation, big things first and small things around them (Wildlands cascade, PCG child assets). Priorities resolve overlaps (PCG). |
| Variation | Several variants per species with ratios; few species per rule; noise offsets in the dither; size ranges; colour from the colour map (HZD, Wildlands, MSFS). |
| Edges | Separate edge assets or levels by distance to the edge (HZD edge trees, Reforger outline min/max distance and scale curve); a blending band with noise and density (PCG); edge thinning (Reforger community tool). |
| Reference | Field trips and photo libraries (Wildlands 15,000 photos; earlier notes: Arma, STALKER 2), mood boards per environment with long and close range and a colour key (Wildlands). |
| Iteration | One small team owns the ecotope data (HZD: one person); in-editor debugging of densities (HZD); procedural data locked before hand edits (Wildlands). |

### 1.11 How teams validate

- **Field reference and mood boards** per environment, covering long range, close range, ground, vegetation and a
  colour key (Wildlands, 1.2).
- **Calibrated colour**: DICE's Battlefront photogrammetry ran "batch calibration to a colour chart" before RAW
  processing [S] https://www.ea.com/frostbite/news/photogrammetry-and-star-wars-battlefront .
- **Early reviews on target hardware** (Wildlands' Xbox One demo, 1.2).
- **Screenshot comparison against approved images**: Unreal's Screenshot Comparison tool captures from functional
  screenshot test actors, asks the team to accept the first capture as "Ground Truth", and compares later captures
  with a slider and a tolerance [S]
  https://dev.epicgames.com/documentation/en-us/unreal-engine/screenshot-comparison-tool-in-unreal-engine .
- SAM3D already has fixed views anchored by map ids and kinds (`data/scenery/views.json`, rendered by
  `-SimMapShots=<folder> -SimShotSet=views`, set beside photos by `tools/terrain_views.py`) [SAM3D].

---

## 2. Scatter mathematics

### 2.1 Blue noise for single plants

- Poisson-disk sampling: no two points closer than r, otherwise random. Bridson's dart throwing runs in linear time
  in any dimension [S] https://www.cs.ubc.ca/~rbridson/docs/bridson-siggraph07-poissondisk.pdf ; SciPy implements it
  (`scipy.stats.qmc.PoissonDisk`) [S]
  https://docs.scipy.org/doc/scipy-1.12.0/reference/generated/scipy.stats.qmc.PoissonDisk.html .
- Infinite, non-periodic blue noise from **Wang tiles**, each holding a precomputed point set; points in a tile are a
  **progressive** sequence so density can vary in space; recursion keeps apparent density constant when zooming;
  suited to "object placement" [S] Kopf, Cohen-Or, Deussen, Lischinski 2006,
  https://www.johanneskopf.de/publications/blue_noise .
- In ecology, trees in even-aged stands are regular at short range because of competition and closer to random
  beyond. The pair correlation function g(r) shows it: g = 1 random, g > 1 clustered, g < 1 regular at distance r
  [S] (Nguyen et al. 2014,
  https://alumni.uni-goettingen.de/de/document/download/7400f738df154eb3de30d88696efb975.pdf/Nguyen_et_al_2014a.pdf ;
  an even-aged Abies stand regular from a size hierarchy, a holm oak stand regular only at small scales [S]
  https://hal.inrae.fr/hal-05142943v1 ). So a Poisson-disk canopy layer, with r near the crown radius, is the right
  null model for a high forest.

### 2.2 Density from rules without reshuffling

When a rule changes density, the trees that stay should not move. Three proven ways:

1. **Ordered dither thresholds** (HZD, 1.1): each candidate point carries a fixed threshold in [0, 1); a point exists
   where density > threshold.
2. **Progressive point sets** (Kopf 2006, 2.1): the first k points of a tile are a good blue-noise set for every k.
3. **Low-discrepancy sequences**: MSFS uses Halton sampling so tree density can change at draw time and stays uniform
   [P via the AAA notes] https://www.asobostudio.com/files/inline-images/Designing_Terrain_System_Fuentes_Lionel.pdf .

A hobby system on the same idea stores about 2 bytes per m², spawns on 64 x 64 m CPU tiles, gives every precomputed
spawn point a dither weight so lowering density keeps the pattern, applies ground textures bottom-up and objects
top-down (large before small), and rasterises footprints into a 2D bitmap for collision [P]
https://habr.com/en/articles/343234 .

### 2.3 Clusters: Thomas and Matérn processes

- **Matérn cluster**: parents are a Poisson process of intensity κ; each parent has a Poisson(μ) number of offspring
  placed uniformly in a disc of radius R; overall intensity κμ [P]
  https://search.r-project.org/CRAN/refmans/spatstat.random/html/rMatClust.html .
- **Thomas process**: the same, but offspring are displaced from the parent with a Gaussian of standard deviation σ
  per axis [P] https://search.r-project.org/CRAN/refmans/spatstat.random/html/rThomas.html . κ and μ may vary in
  space (a density image), which is how a rule's density map drives them.
- Forest ecologists fit Thomas and Matérn models to recruits after disturbance [S]
  https://nomos.is.cuni.cz/publication/487136?query=IPES , and the Thomas process "incorporates the idea of offspring
  dispersed by parent trees" [S] (Nguyen et al., above).
- **Use [SAM3D]:** shrub belts and hazel/elder clumps (Matérn, R 2-4 m), black locust coppice (stools by blue noise,
  stems per stool by a tight Matérn cluster), regeneration in gaps (Thomas with κ from a gap map), reed tussocks along
  water, and grass clumps in a verge.

### 2.4 Multi-class patterns and example-based synthesis (the plot method in mathematics)

- Point patterns can be analysed and re-synthesised from their pair correlation functions (Öztireli and Gross 2012,
  cited by WorldBrush) [S].
- **Ecormier-Nocca, Memari, Gain, Cani (Eurographics 2019)**: PCF synthesis extended to **several classes** with a
  dependency graph between categories (e.g. trees and understorey), a metric for overlapping disks, and checks for
  convergence against the input's variance [S] https://diglib.eg.org/handle/10.1111/cgf13627 ; PDF
  https://people.cs.uct.ac.za/~Jgain/wp-content/papercite-data/pdf/ecormier-nocca2019.pdf .
- **WorldBrush (Emilien et al., SIGGRAPH 2015)**: parameters learnt from example regions are stored as distributions
  in a palette and painted elsewhere; terrain slope taken into account [S] https://doi.org/10.1145/2766975 , PDF
  https://www.cs.purdue.edu/homes/bbenes/papers/Emilien15ToG.pdf .
- **EcoBrush (Gain et al., Eurographics 2017)**: biome-specific databases from ecosystem simulations, indexed by
  temperature, rainfall, sunlight and slope; brushes for age, density and variability; up to 5 x 5 km² interactively;
  output shows succession and self-thinning [S] https://diglib.eg.org/handle/10.1111/cgf13107 .

**For SAM3D [SAM3D]:** a perfected plot can export its statistics (stems/ha, size distribution, g(r) per class and
between classes, edge band widths). The map-wide generator is then tested against those numbers, not against the
plot's exact positions.

### 2.5 Ecological simulation

- **Deussen et al. (SIGGRAPH 1998)**: plant distribution "by hand, by ecosystem simulation, or by a combination";
  keywords include self-thinning; similar plants replaced by representative instances ("approximate instancing") [P
  abstract] https://graphics.stanford.edu/papers/ecosys/ . (The circle-competition details were not read on a page.)
- **Lane and Prusinkiewicz (Graphics Interface 2002)**: two classes of model, local-to-global (individual-based
  simulation) and global-to-local (positions from density maps), both extended with clustering and succession [S]
  https://algorithmicbotany.org/papers/eco.gi2002.html .
- **Makowski et al., "Synthetic Silviculture" (SIGGRAPH 2019)**: multi-scale plant ecosystems "adhering to biological
  priors known in forestry and botany research", up to 90K plants, a demo of about 500K [S]
  https://research.google/pubs/synthetic-silviculture-multi-scale-modeling-of-plant-ecosystems/ ; PDF
  https://wp.faculty.wmi.amu.edu.pl/papers/Makowski.etal-2019-Synthetic-Silviculture.pdf .
- **For SAM3D [SAM3D]:** a 1985 Hungarian wood is managed: compartments of one species and age, planted rows,
  thinnings (VEGETATION.md section 1). Simulation adds little to the main canopy, which is set by forestry. It is
  useful offline for **natural** parts (oak-hornbeam lower canopy, floodplain willow, edges left alone): run a
  self-thinning simulation once on a test plot, keep its statistics as the recipe's targets, and synthesise the map
  from those statistics (2.4).

### 2.6 Determinism for multiplayer

- Position-hashed seeds: every cell's random stream comes from an integer hash of (map seed, recipe id, layer, cell
  x, cell y), never from the order in which cells are generated or streamed. Hash quality and speed are compared in
  Jarzynski and Olano (JCGT 2020), which recommends PCG-family hashes on the Pareto front and a 3D hash (pcg3d) for
  multi-dimensional input [S] https://www.jcgt.org/published/0009/03/02/paper.pdf .
- HZD fixed its one non-deterministic step by giving each placement an ID from its tile and stencil index (1.1);
  CityEngine ties seeds to shapes (1.7); Wildlands' distributed erosion was not fully deterministic (1.2); PCG may
  differ by hardware (1.5).
- **For SAM3D [SAM3D]:** generation stays in simcore on the CPU, in integer hashes and doubles, as the clutter does
  already (each 4 m cell draws its plants from its own position, `data/scenery/clutter.json`). Floating-point results
  can still differ across compilers; keep decisions (exists or not, which species) on integer or fixed-point
  comparisons, and let only the final transform be floating point. A test can hash a plot's instance list and
  compare it with a stored value.

### 2.7 Chunked and streamed generation

- Generate per cell with a halo: a point belongs to the cell its position falls in, but a cell's generator sees its
  neighbours' candidates within the largest footprint so Poisson-disk and collision hold across seams [SAM3D].
- Cell size by footprint: HZD used 128 m blocks for trees and 32 m for grass (1.1); PCG puts small meshes on small
  grids (1.5); SAM3D's clutter uses 4 m cells in a 100 m ring [SAM3D].
- Trees and buildings that block sight are generated whole at map load (simcore owns them); only non-blocking clutter
  streams around the camera [SAM3D, as in the AAA notes lesson 7].

---

## 3. Real-world numbers for each plot type (Central European lowland, autumn)

Ranges, with the source and how it was read. "Not found" means a search was made and no page gave it.

### 3.1 Woods

| Type | Number | Source |
|---|---|---|
| Oak-hornbeam (sandy, Vértesalja) | Stands 22-28 m tall by age; upper canopy cover 50-85% (Carpinus, Quercus cerris, Q. robur, Tilia); lower canopy 15-20 m, 20-60% cover, mostly suppressed trees (Acer campestre, Carpinus); shrub layer 1.5-3 m, 1-60% cover, "changeable, mainly from forestry interventions"; regeneration 1-15%; herbs 15-95% | Kevey & Riezing 2023, Bot. Közlem. 110(1):27-42 [P] https://ojs.mtak.hu/index.php/BotKozlem/article/download/12014/9803 |
| Oak-hornbeam (Great Plain stands) | Lower canopy 12-20 m with 10-70% cover (Nyírség); shrub layer 1-5 m, 3-60% (Nyírség), 1.5-5 m, 20-85% (Szatmár-Bereg); a Szolnok-area stand 2-4 m shrubs at 40-75% | Kevey et al. papers, [S] via search summary (e.g. https://ojs.mtak.hu/index.php/BotKozlem/article/download/14583/12008 ) |
| Oak (managed) | 150-450 stems/ha at rotation of 100-130 years | Solymos 1993, in VEGETATION.md section 1 |
| Oak-hornbeam stems/ha (plot data) | 233 permanent plots in six Hungarian strict reserves, with stand height and density per plot | Szegleti et al. 2023, Data in Brief, data on Zenodo [S] https://zenodo.org/records/7311384 (not opened: an Excel download) |
| Black locust | About two-thirds of Hungary's black locust is coppice; planted 2000-2500 or (medium-good sites) 3500-4000 stems/ha, reduced to 900-1000 at 25-30 years, 500-600 at felling on the poorer model; sustainable stocking 450-660/ha; final felling 35-40 years; average harvest age 31 | Rédei et al. papers [S] https://epa.hu/04900/04996/00011/pdf/EPA04996_acta_2012_103-112.pdf , https://core.ac.uk/download/161872582.pdf ; planting 2.4 x 0.7-1.0 m in VEGETATION.md |
| Black locust height | 12-20 m | references T4 (hu.wikipedia Fehér akác) |
| Black locust coppice: stems per stool | Not found | |
| Hybrid poplar plantation | 6 x 6 m (about 278 per ha) in a Hungarian I-214 plantation project; I-214, Pannonia, Kopecky, Agathe-F, Koltay the common cultivars; veneer rotations 12-25 years; a Serbian I-214 stand at 5 x 5 m (400/ha) thinned at 11 years to 300 then 200/ha | [S] https://ec.europa.eu/enrd/sites/default/files/project/attachments/gp_hu_poplar_plantation_508_web_fin.pdf ; [S] https://www.mdpi.com/1999-4907/12/5/623/html ; [S] https://doaj.org/article/bd55cb7e3b4a4cb186f22fadbe4a72d1 |
| Hybrid poplar height | Not found for Hungary | |
| White willow floodplain (upper Tisza) | Deeper parts of the low floodplain on silt; lower canopy 10-18 m, 10-50% cover, mostly suppressed Salix alba; shrub layer underdeveloped, 1.5-3.5 m, 1-25%; white poplar groves stand 1-1.5 m higher on sandier soil with a well-developed shrub layer | Kevey & Barna 2016, Kitaibelia 21(1):78-100 [P abstract] https://dea.lib.unideb.hu/items/a60764e6-d489-4db1-8ffd-6a953875a437/full ; layer numbers [S] |
| White willow height | Mature stands at most 20-25 m (Szigetköz); white poplar 25-30 m; white poplar 20-30 m on the Rába, 22-30 m on Csepel Island | [S] https://ojs.mtak.hu/index.php/BotKozlem/article/download/14608/12033 ; https://core.ac.uk/download/187767775.pdf ; https://ojs.lib.unideb.hu/kitaibelia/article/download/5722/5374 |
| Willow on the Tisza | Often pollarded (Mártély) | [S] https://rsis.ramsar.org/RISapp/files/4828515/documents/HU186_lit160818.pdf |
| White willow stems/ha | Not found | |

### 3.2 Edges, rows and hedges

| Feature | Number | Source |
|---|---|---|
| Forest edge, three bands | Herb fringe at least 3 m, 5-10 m optimal; shrub belt at least 5 m; mantle at least 10 m, 15-20 m optimal | Naturschutzbund Austria [S] https://naturschutzbund.at/files/NATUR%20VERBINDET/Waldraender-Gestaltung-Pflege_NL4_17.pdf ; shrub belt 5-10 m with thorny shrubs [S] https://fr.ch/de/document/76011 ; herb fringe at least 3 m (Lucerne) [S] https://lawa.lu.ch/-/media/LAWA/Dokumente/Wald/waldbiodiversitaet/mb/mb_pflege_von_waldraendern.pdf ; Thurgau and WWF in VEGETATION.md |
| Swiss inventory's method | Widths measured horizontally at 1.3 m, the three zones added; a shrub belt counts when wider than 1.0 m over half the stretch | [S] https://www.lfi.ch/api/data-catalog/pdf/37 |
| Hungarian new-forest edge rule | At least 3 rows of shrubs, at least 3 plants per running metre per row; at row ends at least 6 m of edge; inside the edge trees' expected crown projection | 2025 agri-support annex [S] https://kap.gov.hu/sites/default/files/2025-03/3.%20mell%C3%A9klet%20-%20Kieg%C3%A9sz%C3%ADt%C5%91%20intezked%C3%A9sek.pdf (modern rule; the 1980s edge was thinner, VEGETATION.md) |
| Edge structure depends on aspect | "Strongly influenced" by the edge's compass facing | [S] https://erdeszetilapok.oszk.hu/00276/pdf/03bartha.pdf |
| Tree row (fasor), legal | Trees of one species in a line, spacing no more than twice the expected mature crown diameter | Hungarian rule quoted in a 2022 municipal decree [S] https://njt.jog.gov.hu/jogszabaly/2022-16-K3-5Y403 |
| Poplar row on the plain | A 29-year-old early poplar row at Doboz cemetery: 5 m spacing in the row, roots only to 60 cm, good growth | Erdészeti Lapok 1961/4 pp. 129-139 [P] https://erdeszetilapok.oszk.hu/01344/pdf/EL_1961_04_129-139.pdf |
| Public-land tree spacing | 7.00 m for poplar, willow, black locust, pine on municipal green space | [S] municipal annex (same search, or.njt.hu) |
| Field-protection belts | 1,500 km of belts planted on the Great Plain from the 1950s, on Soviet type plans; widths and row counts not found (lead: Magyar Pál, Alföldfásítás II, 1961) | [S] https://ojs.mtak.hu/index.php/termvedkozlem/article/download/10055/8041 ; https://erdeszetilapok.oszk.hu/00179/pdf/EL_1955_04_157-161.pdf |
| Field hedge | 3, 4 or 5 rows make 5, 8 or 10 m; 1.25-1.5 m between rows and plants; 3 rows need at least 6 m (Rotenburg); low hedge 2-3 m tall, tall hedge to about 5 m; a tree hedge adds trees | [S] https://www.lk-row.de/medien/dokumente/massnahmenblatt_v_feldhecke.pdf ; https://rotenburg.bund.net/fileadmin/rotenburg/Landnatur/koen_hecken_anlegen.pdf ; https://www.ag.ch/media/kanton-aargau/dfr/dokumente/landwirtschaft/programm-labiola/programm-labiola-bis-230331/merkblaetter-labiola/04-labiola-mb-hecken-feld-ufergehoelz-nov20.pdf . True hedgerows were rare among the big collective fields (VEGETATION.md). |

### 3.3 Water edges

| Feature | Number | Source |
|---|---|---|
| Oxbow reed fringe | Szarvas oxbow (Hármas-Körös, 29.2 km long, about 70 m wide): shore reed (Scirpeto-Phragmitetum) **0.5-1 m wide**; elsewhere a 1 m band of Typha latifolia | Békés Megyei Múzeumok Közleményei 1980 [S] https://epa.hu/01500/01577/00006/pdf/bmmk_1980_003-027.pdf |
| Canal reed belt | Canal banks in Vojvodina with a reed belt about 1-5 m wide | [S] https://acta-zoologica-bulgarica.eu/downloads/acta-zoologica-bulgarica/2014/66-4-511-516.pdf |
| Reed height | 1-4 m; Balaton's largest mean height 380 cm, 220 cm at flowering | [S] https://journal.uni-mate.hu/index.php/gfa/article/download/6229/6249/11765 |
| Shore strip by law | 6 m from the shore line along state-owned oxbows | [S] https://njt.jog.gov.hu/jogszabaly/2006-21-20-22 |
| Wide reed beds (fishponds, marsh) | Seen in photos P29, P30 and the Tápiószecső candidate; widths not sourced | TERRAIN_REFERENCES |

### 3.4 Roads, verges and ditches

| Feature | Number | Source |
|---|---|---|
| Design standard | ÚT 2-1.201 "Közutak tervezése" (2008); its cross-section values were not read | [S] course syllabus https://witch.mik.pte.hu/oktatas/Tantargyi_tematikak_2022_23_1/BSc_BA/Epitomernoki_BSc_nappali/05%20_%20MSB231MNEP_K%f6zleked%e9stervez%e9s1._Dr.%20Lindenbach%20%c1gnes.pdf |
| Outer minor road (local rule) | 3.5 m lanes, 2.0 m shoulders outside settlements | [S] a local regulation found by search (or.njt.hu annex) |
| Rural access road (modern works) | 4.00 m asphalt with 1.00 m gravel shoulders each side, a small ditch one side and an earth ditch the other (Hegyeshalom); 4 m with earth ditches (Bátaszék) | [S] https://hegyeshalom.hu/data/hhalom%20kulteruleti_ut.pdf ; https://bataszek.hu/files/tartalom/test_ules/2022/20220427/77_Bataszek_TOP2_Muszaki_leiras.pdf |
| Shoulder and side slope | 1.00 m shoulders and 6/4 (1:1.5) slopes in a Budapest design | [S] https://ttr2.ujbuda.hu/sites/default/files/attachments/files/2017_10/1229_ut_muleiras.pdf |
| Village streets | 2.8-3.2 m asphalt on side streets, 3.5-4.5 m main streets, earth soakaway ditches, grass verges to the fences | references T8 (Tiszacsege) |
| Ditch depth and width by the road | **Not found** (estimate for the plot: 0.5-1.0 m deep, 2-3 m across the top on 1:1.5 slopes [SAM3D est.]) | |

### 3.5 Fields in October

The October 1985 states and shares are in `TERRAIN_REFERENCES_2026-11.md` 1.5, the sown-wheat look in
`TERRAIN_TEXTURES_2026-11.md` 4. Added here:

| Feature | Number | Source |
|---|---|---|
| Wheat stubble | Combines usually leave about 100 mm (Lithuanian study); trials at 10 and 20 cm header height (Czech) | [S] https://www.vdu.lt/cris/handle/20.500.12259/82563 ; [S] https://rae.agriculturejournals.cz/artkey/rae-201503-0004_evaluation-of-wheat-stubble-management-practices-in-terms-of-the-fuel-consumption-and-field-capacity.php |
| Maize row spacing | 70-76 cm was the conventional Hungarian spacing in the 1970s-80s literature; a Hungarian stalk-shredding header for 70 cm rows | [S] https://core.ac.uk/download/161039296.pdf ; https://www.landwirt.com/en/used-farm-machinery,1748328,Oros-6211-HSA-70cm-elolzuzos-kukorica-adapter.html |
| Maize plant density | 35-40 thousand/ha optimum in the 1950s, 50 thousand in the 1960s; yields 3.23 t/ha (1966-70) to 5.86 t/ha (1981-90) | [S] Acta Agraria Debreceniensis review (same search) |
| Maize stalk stubble | US guidance: 10-12 in (25-30 cm) after harvest, some run the head at 18 in (46 cm); "low standing" under 10 in | [S] https://cropwatch.unl.edu/cw-09-10-27-qa-recommended-corn-cutting-height/ ; https://support.fieldtomarket.org/faq/what-is-the-difference-between-direct-low-standing-stubble-and-direct-high-standing-stubble-harvest-options/ . Hungarian 1985 practice (cob picking, stalks cut and baled, photo P10, P77) not sourced numerically. |
| Plough | Mouldboard bodies up to 45-54 cm wide, 25-35 cm deep (modern maximum ratings); depth about half the furrow width (forum) | [S] https://www.poettinger.at/en_us/produkte/detailtexthl/SE40/th/6/customised-mouldboards |
| Ploughland roughness | Random roughness of a mouldboard-ploughed surface 1.0-2.1 in (25-53 mm, New Mexico NRCS table) or 1.9 in (48 mm, NRCS WEQ guidance); clay soils rougher, sandy smoother | [S] https://webarchive.library.unt.edu/eot2008/20080917084927/http:/www.nrcs.usda.gov/technical/ECS/agronomy/randruf2.pdf ; [S] https://fargo.nserl.purdue.edu/rusle2_dataweb/Training/Slide%20Sets/RUSLE2%20slide%20set2_files/slide0135.htm . 1985's drought clods were "big" (references 1.5). |
| Wheat sowing | Sow 25 Sept-10 Oct for yield; 3-5 cm deep; 3-5 million seeds/ha in trials, 4.2-4.8 million in a late case | [S] https://www.kite.hu/assets/images/dyn_images/pdfs/1595918649_KALASZOS-2020.pdf |
| Wheat emergence | Slows when the daily mean falls below 15 °C; a leaf every ~122 °C-days (AHDB); a 2-3 leaf seedling's height not found | [S] https://www.cjae.net/EN/Y2023/V34/I3/679 ; https://ahdb.org.uk/knowledge-library/leaf-emergence-and-tillering-growth-stages-in-winter-wheat-gs1-gs2 |
| Headlands and field-edge strips | **Not found** for 1985 Hungary (estimate: a headland of 2-3 implement widths, about 10-25 m, worked across the furrows, plus a grassy margin of 1-3 m at tracks and ditches [SAM3D est.]) | |

### 3.6 Grass in October

| Feature | Number | Source |
|---|---|---|
| Sheep pasture | Optimal grass height 10-15 cm (Kertész 1993); rotation: sheep to 10-15 cm regrowth in 25 days, cattle to 25-30 cm in 30-40; "in autumn the stock grazes the sward bare" on the open puszta | [S] Gyepgazdálkodási Közlemények papers via search, e.g. https://ojs.lib.unideb.hu/gygk/article/download/9759/8688/18870 |
| Saline pasture | Sheep grazing starts mid-March to mid-April when grass reaches 100-150 mm (Karcag) | [S] same search |
| Hay meadow stubble trials | 4, 8 and 12 cm cutting heights (Gödöllő) | [S] https://ojs.lib.unideb.hu/gygk/article/download/9796/8719/18967 |
| Road verge | Welsh minimum 100 mm after cutting; a Dutch board's last round late August into October; no Hungarian rule found | [S] https://democracy.cyngor.gwynedd.gov.uk/documents/s32521/Report.pdf ; https://en.hhnk.nl/maaien-van-wegbermen |
| 1985 | Six dry weeks: dry, brown-green autumn grass | references 1.5 |

### 3.7 Microrelief of the plain

| Feature | Number | Source |
|---|---|---|
| Relative relief, floodplain | 0-3 m per km² in the Körös region (0-1.5 right bank of the Hármas-Körös, 1-3 left bank, 1.5-2 by the Kettős and Sebes-Körös); lowest ground the abandoned beds and oxbow shores, highest the old levees ("folyóhátak") | [S] https://ojs.bibl.u-szeged.hu/index.php/modszertani-kozlemenyek/article/download/44399/43198/53302 |
| Levee ridges | Form a network with backswamps in the meshes; asymmetric (steeper to the channel); a ridge's height above the swamp falling downstream from 5 to 3 to 2.5 m | [S] https://geosci.u-szeged.hu/intezetunkrol/tomus-50-2016/gabris-gyula-koros |
| Kunhalom (kurgan) | Diameter 20-90 m, relative height 0.5-12 m, footprint ("shield") 50-160 m; classic description 5-10 m high, 20-50 m across; Török-halom at Kígyós: 5 m high, 58 x 52 m, 3,670 m² | [S] Kitaibelia and Tájökológiai Lapok papers via search (e.g. https://journal.uni-mate.hu/index.php/tl/article/download/5553/6562/12484 ); Tóth Cs., county survey https://epa.oszk.hu/03000/03002/00171/pdf/EPA03002_jaszkunsag_20070102_042-059.pdf |
| Sand ridges (Danube-Tisza) | Dune forms 6-8 m mean relative height, about 150 m long (Dunavarsány); the ridge as a whole 30-70 m above the neighbouring plains; surface of sand-sheet flats, dune ridges and deflation hollows | [S] https://ojs3.mtak.hu/index.php/foldtanikozlony/article/download/2605/1849 ; https://termeszetvedelem.hu/_user/downloads/agrar/Homokhatsag.pdf |
| Flood dykes | Crown 4.0 m; slopes 1:3 water side, 1:2.5 land side; crest 1.0 m above the design flood level (modern Tisza works); older Tisza dykes 4 m crown, 1:3 and 1:2; Tisza dykes about 4-6 m high (weak source) | [S] https://kohesio.ec.europa.eu/hu/projektek/Q3943240 ; https://mnl.gov.hu/download/file/fid/663745 ; weak: https://miau.my-x.hu/nappalos/2008tavasz/tdk/last/gvam/gorog_georgina+/Tisza.doc |
| Oxbows | Szarvas: 29.2 km long, about 70 m wide | [S] (Szarvas 1980 above) |
| Drainage canals | **Not found** (estimate for the plot: 1.5-3 m deep, 1-3 m bottom, 1:1.5-1:2 slopes, spoil banks alongside [SAM3D est.]) | |

---

## 4. Relief at several scales on a flat plain

**What must not change.** simcore's heights are the truth for the sight lines and the radar horizon. Anything Unreal
draws must sit on them, and anything that could hide a vehicle or a man must be in them (AAA notes, lesson 5)
[SAM3D].

**The scales [SAM3D, from section 3.7]:**

| Band | Wavelength | Amplitude on the plain | Examples | Where it lives |
|---|---|---|---|---|
| Macro | > 2 km | tens of m | the sand ridge's rise, the river terraces, the northern hills | simcore's map data (fixed) |
| Meso | 50 m-2 km | 0.5-3 m (floodplain), 5-10 m (dunes) | levees, backswamps, old channels, dune ridges and hollows, kunhalmok | simcore generator, by rule |
| Stamps | 5-100 m across | 0.5-6 m | dykes, road embankments and cuttings, canals and their spoil banks, ditches, revetments | simcore generator, by shape |
| Micro | < 5 m | 0.02-0.5 m | furrows, clods (25-50 mm roughness), ruts, tussocks | Unreal material only (height and normal maps, light displacement) |

**Rules [SAM3D proposal, built on the cited techniques]:**

1. **Meso noise is band-limited and zero-mean.** Use fBm or ridged noise summed only over the meso octaves, then
   subtract a low-pass of itself (a box or Gaussian over about 2 km) so the regional heights stay what the data says.
   Amplitude comes from a relief-class map (floodplain 0.5-1.5 m, loess or alluvial plain 0.3-1 m, sand country
   3-8 m with dune-shaped ridged noise along the wind) and is clamped to zero under water, roads, settlements and
   positions.
2. **Mapped features are carved, not hoped for.** Génevaux et al. build terrain from a river network with blending
   and carving operators [S] https://www.cs.purdue.edu/homes/bbenes/papers/Genevaux13ToG.pdf ; MSFS terraforms with
   vector shapes and an influence radius merged through a distance field, never editing the source DEM [P via AAA
   notes]. For SAM3D: each old channel, levee, dyke, canal, road and kunhalom is a vector with a **cross-section
   profile** in data (depth or height as a function of signed distance from the line), applied as
   `h += profile(d)` or `h = max(h, crest(d))`, with a smooth blend to zero at the profile's outer edge.
3. **Profiles from section 3.** Dyke: crown 4 m, 1:3 water side, 1:2.5 land side, 3-6 m high. Old channel: 20-80 m
   wide, 1-3 m deep, its floor wetter (feeds the texture's wetness). Levee: 2.5-5 m above the backswamp, steeper
   toward the channel. Kunhalom: a cone or dome 20-90 m across, 2-10 m high, the plough pulling its foot out
   (Gaussian foot). Road: the crown about 0.3-1 m over the field on a floodplain, the ditch beside it (estimates where
   section 3.4 found nothing).
4. **Order:** macro data, then meso noise, then carve channels, then levees and dunes, then dykes and canals, then
   roads (embankment, ditches), then flatten under buildings and positions. Each step reads the previous result, as
   Wildlands' layers do [P].
5. **Resolution honesty.** A 50 m grid cannot hold a 30 m mound or a 2 m ditch. Outside detail areas, apply only the
   low-pass of each stamp (its volume, not its shape); inside 10-20 m detail areas apply it whole; anything still finer
   than the grid (ditches, furrows) is drawn by Unreal and, if it matters for concealment, given to the sight lines
   as a line feature with its profile rather than by refining the whole grid [SAM3D].
6. **Erosion only where it means something.** On a flat plain, hydraulic erosion mainly makes channels; take channels
   from the map's rivers instead. Use erosion (Houdini-style iterations, section 1.6) only on the northern hills'
   slopes, and run it once at map build, deterministically, at a single resolution (Wildlands' seams warn against
   distributed erosion).
7. **Test it like the trees.** A simcore test checks that the meso layer's 2 km low-pass is under a few centimetres,
   that dyke crowns are flat to the profile, that no road or water cell moved, and that the obstacle heights still
   bound everything drawn.

---

## 5. Texturing the whole map methodically

**The proven split** (Andersson 2007 [P]; Wildlands [P]; Arma and Reforger in the AAA notes): a unique, low-frequency
colour map for the whole map, and a few tiled detail layers near the camera, chosen by masks or per-pixel rules.

**Inputs a pixel can read in SAM3D [SAM3D]:**

| Input | From | Resolution |
|---|---|---|
| Cover class and layer weights | simcore's ground mask (`ground_layers.hpp`, `GroundMask`) | 25 m cells |
| Crop state, field id, furrow heading | the field parcels and the crop calendar (`ground_layers.json`, `LayerOnDay`) | per parcel |
| Distance to wood edge, road edge, water edge, parcel edge, settlement edge | signed distance fields rasterised from simcore's vectors per ground tile (MSFS does this with GPU distance fields "for smooth fade-outs" [P via AAA notes]) | 2-5 m |
| Height, slope, normal | the drawn ground (Frostbite's three per-pixel parameters [P]) | per pixel |
| Wetness | height above the nearest channel or water, from simcore's relief and rivers [SAM3D proposal] | 10-25 m |
| Macro variation | low-frequency noise per recipe (soil patches, moisture blotches) | 20-200 m |

**Rules [SAM3D proposal]:**

1. **A layer is chosen by rule, not painted.** Each recipe (section 6.3) says which detail layer(s) it uses and how
   distance to edges modifies them: a ploughed field's headland band within 10-25 m of the parcel edge turns its
   furrow layer 90°; a verge band within 1-3 m of a track uses short grass; a wood's litter extends 2-5 m into the
   field under the mantle; a wet band follows old channels.
2. **At most four detail layers per pixel**, blended by height-aware weights (Wildlands' material blends four;
   Reforger five per block; Arma five per tile). The ground material research already covers the blend and
   anti-tiling (`TERRAIN_GROUND_MATERIAL_2026-11.md` 2.3-2.5).
3. **The colour map is synthesised from the same recipes.** Its colour per pixel is the recipe's target colour
   (the soil targets in `TERRAIN_TEXTURES_2026-11.md` 1.5, the crop states), with the recipe's macro variation and
   the edge bands, drawn from vectors rather than the 25 m staircase. Trees and impostors take their tint from the
   same targets (AAA notes, lesson 9). One number in one file sets what a field looks like near and far.
4. **Edges between layers are rules too**: the blend band's width and noise per pair of recipes (PCG's blending range
   and noise, section 1.5), plus a height-blend so clods and grass interlock instead of cross-fading.
5. **Iterate on a plot, push everywhere**: because no map location stores a painted weight, changing a recipe changes
   every matching pixel; the only per-place data are hand overrides stored by map id (AAA notes, lesson 11).

---

## 6. A method for SAM3D

### 6.1 Principles

1. **A plot is not a hand-made scene.** It is a small synthetic world generated by the same simcore code as the map.
   Perfecting a plot perfects the rule; nothing is copied from the plot to the map.
2. **Every plot is checked twice:** in its synthetic plot world (controlled, repeatable) and at anchors found on the
   Kesmark map by kind (the real context), like `data/scenery/views.json`.
3. **Every number has a source or says "estimate".** The recipe files follow the project's `value`/`source`/
   `confidence` style.
4. **Sight-blocking content stays in simcore and never scales with quality.** Plots that change trees, rows, edges,
   dykes or embankments run the obstacle-height test.
5. **Statistics, not positions, carry over.** A plot's approved output is reduced to measured statistics (stems/ha,
   heights, g(r), band widths, colour means) that become the recipe's test targets.

### 6.2 The plot catalogue

Plot size: a 50 x 50 m core inside a 400 x 400 m context ring generated by the same rules, so the 25 m and 150 m
cameras see believable surroundings, and woods get a compartment or two around them [SAM3D]. A 50 x 50 m core is
0.25 ha: about 69 poplars at 6 x 6 m, 225 black locusts at 900/ha, 40-110 oaks at 150-450/ha [SAM3D arithmetic].

"Inputs" are the simcore data the recipe's rule reads to decide where it applies on the map.

| # | Plot | Must show | Real numbers (section) | Photos | Inputs from simcore |
|---|---|---|---|---|---|
| 1 | Ploughland (autumn 1985) | Furrows along the parcel's heading, big dry clods, pale dusty top; headland band across the furrows | Plough body 30-50 cm, roughness 25-50 mm (3.5); states (refs 1.5) | P01, P03, P12, P65 | cover Field, crop state `ploughed`, furrow heading, parcel edge distance |
| 2 | Maize stover (harvested) | Stalks lying and broken, stumps in 70-76 cm rows, bales or cut stalks in places | Rows 70-76 cm; stubble 25-30 cm (US) (3.5) | P10, P77, P08 | crop maize, state harvested, heading |
| 3 | Standing maize (dry) | A 2-3 m wall of dry tan stalks in rows; the field edge as a vertical face; a track beside it | Rows 70-76 cm; density 50-70 thousand/ha estimate (3.5); height not sourced here | P07, P09, P71, P77 | crop maize, state standing; parcel edge (the face) |
| 4 | Sown wheat (drilled or emerged) | Fine rolled seedbed, faint drill lines 10-15 cm apart, patchy green | TERRAIN_TEXTURES 4; refs 1.5 | (none sown in Oct; P03 ploughed for soil) | crop wheat, state drilled/emerged, heading |
| 5 | Wheat stubble (disced or not) | Short straw rows 10-20 cm, straw lines of the combine, weeds | 10-20 cm (3.5) | P04, P91 | crop wheat, state stubble/disced |
| 6 | Pasture (grazed, dry) | Short brown-green sward 5-15 cm, tussocks, sheep tracks, a sweep well | 10-15 cm optimum, grazed bare in autumn (3.6) | P56, P80, P59 | cover Field, crop pasture |
| 7 | Oak-hornbeam high forest | Two canopy layers (22-28 m and 15-20 m), patchy shrubs 1.5-3 m, litter, compartment edge | 3.1 | P22, P66 | wood stand `oak_hornbeam`, distance to wood edge |
| 8 | Black locust (coppice and high forest) | Dense thin multi-stem groups, 12-20 m, open light crowns turning yellow, elder below | 2000-4000 planted, 900-1000 at 25-30 y (3.1) | (none found; T4 trait; P72 row) | stand `black_locust` |
| 9 | Hybrid poplar plantation | Strict 6 x 6 m lattice, straight boles, grass or ploughed floor between rows, one age | 6 x 6 m, 278/ha (3.1) | P24, P21 | stand `poplar_plantation` |
| 10 | Floodplain willow-poplar | Leaning, pollarded willows to 20-25 m, weak shrub layer, mud and flood litter, white poplar on higher ground | 3.1 | P25, P27, P28 | stand wet, distance to river, wetness |
| 11 | Forest edge against a field | Herb fringe, shrub belt, mantle of lower light-demanding trees, then the stand; aspect-dependent | 3, 5, 10-20 m bands (3.2) | P20, P22, Üllő candidate | wood edge distance and facing, neighbour cover |
| 12 | Tree row on a farm track | Poplar or black locust at about 5-8 m, some gaps and replacements, two-rut track, grass verge | 5 m (Doboz), at most 2x crown (3.2) | P71, P72, P73, P90, P92 | track polyline, row flag, species rule |
| 13 | Road with verge and ditch | Asphalt 4-6 m, shoulders 1-2 m, ditch, pole line, kilometre stone, tree row on one side | 3.4 | P60, P61, P11, P76 | road class polyline |
| 14 | River or canal bank with reeds | Reed belt 1-5 m (canal), 0.5-1 m (oxbow shore), reeds 2-4 m, Typha patches, willow on the bank | 3.3 | P26, P29, P30 | river/canal polyline, water edge distance |
| 15 | Flood dyke | 4 m crown with a track, 1:3 and 1:2.5 grassed slopes, borrow-pit hollows, poles | 3.7 | P31, P69 | dyke polyline (the map has none yet) |
| 16 | Village street and garden | 3 m asphalt, earth ditches, wide verges, fences, gable-end houses and kocka houses, fruit trees, long gardens behind | references T6-T8 | P32, P33, P34, P85, P78 | settlement, street segment, house plots |
| 17 | Kunhalom and sand ridge | A 20-90 m mound in ploughland; dune ridge with black locust and pine on sand | 3.7 | Fülöpháza candidate, P92 | relief stamps, soil class sand |

**Transition plots** (edges are plotted too, section 6.4): field|wood (11 is one), field|field (two crops meeting at a
headland), field|track with tree row (12), field|road (13), wood|water (10 with 14), field|village fringe (household
strips against a TSZ field, P78), pasture|marsh.

### 6.3 How a recipe is stored and applied

One JSON per recipe in `data/scenery/recipes/`, read by a simcore loader, applied by simcore's dressing and ground
code. Sketch (values from section 3; layout follows the project's data rules) [SAM3D proposal]:

```json
{
  "recipe_id": "forest_edge_field",
  "applies": { "cover": "Forest", "edge_with": ["Field"], "distance_m": [0, 30] },
  "priority": 20,
  "bands": [
    { "id": "herb_fringe", "from_m": -5, "to_m": 0,  "ground": ["meadow_tall"], "clutter": { "grass": 1.0 } },
    { "id": "shrub_belt",  "from_m": 0,  "to_m": 6,  "scatter": { "process": "matern", "kappa_per_ha": 120, "mu": 4, "radius_m": 2.5,
                            "species": { "hazel": 0.5, "elder": 0.3, "blackthorn": 0.2 }, "height_m": [1.5, 4.0] } },
    { "id": "mantle",      "from_m": 6,  "to_m": 18, "scatter": { "process": "poisson_disk", "r_m": 5.5,
                            "species": { "oak": 0.4, "maple": 0.3, "acacia": 0.3 }, "height_of_stand": [0.6, 0.85] } }
  ],
  "edge_noise": { "amplitude_m": 3, "wavelength_m": 40 },
  "aspect_rule": { "south_facing_band_scale": 1.3, "north_facing_band_scale": 0.7 },
  "targets": { "shrub_cover": [0.3, 0.7], "mantle_stems_per_ha": [150, 450] },
  "source": "bands: Naturschutzbund Austria (herb >= 3 m, shrubs >= 5 m, mantle >= 10 m); thinner 1980s managed edge; aspect: Bartha (Erdészeti Lapok); densities: game estimate",
  "confidence": "estimate"
}
```

How it is applied [SAM3D]:

1. **Match.** Every map cell or shape is matched to recipes by its inputs (cover, crop state, stand, edge type and
   signed distance, wetness, soil, road class). Overlaps resolve by priority, as in PCG Biome Core.
2. **Generate per cell** with position-hashed seeds (2.6), big things first, children around parents (Wildlands
   cascade, PCG child assets), each band thresholding a fixed ordered point set (2.2).
3. **Emit three things**: instances (trees, shrubs, stumps, stalks, reeds, poles), ground weights and colours for the
   colour map, and relief stamps.
4. **Report**: hectares per recipe, instances per recipe, budget per ring, and the measured statistics against the
   recipe's `targets`. This is HZD's in-game interpreter in report form.

### 6.4 Edges and transitions

- **Edges are recipes keyed by a pair of neighbours** with bands across a signed distance (as above). The signed
  distance comes from simcore's vectors (wood polygons, parcel outlines, road and river lines), not from the 25 m
  raster, so bands are straight where the data is straight and curved where it is curved [SAM3D].
- **Bands get noise** along the edge (amplitude a few metres, wavelength tens of metres) so the edge is not a
  ruler line; Reforger's outline scale curve and HZD's edge trees are the same idea [P].
- **Field|field transitions** are a headland: the last 10-25 m (estimate) worked across the furrows, then a 1-3 m
  grassy margin or a track. Parcels of the same crop in the same state still differ by their own day in the crop
  calendar (the existing per-field draw in `ground_layers.json`).
- **Every transition plot** is a 50 x 100 m strip straddling the boundary, judged from the same cameras.

### 6.5 Where plots live

- **Plot worlds**: `data/scenery/plots/<plot>.json` defines a tiny synthetic map (a few vectors: a parcel, a wood
  outline, a track, a road, a stream; a cover raster; a sky clock) that simcore turns into a map through the same
  generator path. Unreal opens it with a command-line switch, like the existing views. Plots are deterministic, so a
  render today and a render next week differ only where a recipe changed.
- **Plot anchors on the real map**: each plot also names an anchor kind (e.g. `wood_edge` facing south against
  ploughland, `track_row` of black locust) resolved on Kesmark at load, exactly as `views.json` does, so the same
  recipe is judged in its real context and lighting.

### 6.6 The order to build them

Ordered by how much of a crew's view each covers in October 1985 and how much the horizon depends on it [SAM3D]:

1. Ploughland (1) and its headland transition: the biggest share of the October plain.
2. Maize stover (2) and standing maize (3): about half the maize still stood on 15 October 1985 (references 1.5), and
   standing maize hides men and vehicles.
3. Forest edge against a field (11) and tree row on a farm track (12): from a vehicle's roof the first treeline is
   the horizon (VEGETATION.md section 2).
4. Black locust (8), the plain's commonest tree, then the poplar plantation (9).
5. Road with verge and ditch (13), and the field|road transition.
6. Sown wheat (4) and wheat stubble (5).
7. Oak-hornbeam (7) and floodplain willow (10) with the river bank (14).
8. Pasture (6), village street and garden (16).
9. Dyke (15) and relief stamps (17), once the map has dykes and the relief rules exist.

### 6.7 How each plot is judged

**Three cameras per plot, each also at the plot's map anchor [SAM3D]:**

| Camera | Height | Look | What it judges |
|---|---|---|---|
| Eye | 1.7 m | level, across the plot and into the context | ground texture scale (furrows, clods, stubble), clutter heights, stems and bark, the edge's bands, the verge and ditch |
| Low oblique | 25 m | 30° down | spacing and pattern (lattice vs clusters), band widths, gaps in rows, repetition |
| High oblique | 150 m | 30° down (V10 style) | tone and colour against the colour map, tiling, the field pattern, agreement between meshes, impostors and colour map |

Lighting fixed per review: the scenario's 15 October sky clock at mid-morning and mid-afternoon, one overcast set
(many reference photos are overcast).

**Measurements beside the eye [SAM3D]:**

- simcore tests on the plot's instance list: stems/ha, height and crown ranges, spacing histogram and g(r) against the
  recipe's targets; band widths across edges; nothing above the obstacle heights; the hash of the list (determinism).
- Colour: the mean linear albedo of each ground layer in the 150 m view's buffer against the targets in
  `TERRAIN_TEXTURES_2026-11.md` 1.5, within a tolerance.
- Repetition: the autocorrelation of the 150 m render's luminance should show no peak at the texture's tile period.
- Side-by-side page with the plot's photos and the last approved render (extend `tools/terrain_views.py`); an
  approved render becomes the plot's ground truth, and later renders are diffed against it, as Unreal's Screenshot
  Comparison does with a tolerance (1.11).

**Review:** the owner judges by eye against the photos (Pannonia is fictional: look, not statistics); the
measurements only catch drift and mistakes.

### 6.8 Pushing a change map-wide

1. Edit the recipe JSON (one place).
2. Run the plot's simcore tests (numbers, determinism, obstacle heights).
3. Render the plot world and its map anchors from the three cameras; compare with photos and the ground truth.
4. Approve: the new renders become ground truth.
5. Run the map-wide report (hectares, instances and budgets per recipe); render the existing acceptance views
   (`-SimShotSet=views`) to see the change in the big picture.
6. Nothing else to do: no place on the map stores a painted value, so every matching cell already uses the new
   recipe. Hand overrides (a haystack, a particular hedge) stay as data keyed by map ids and are re-checked by the
   views.

---

## 7. Gaps and leads

- **Not found:** roadside ditch and drainage canal cross-sections (ÚT 2-1.201's values were not read; regional water
  directorates' design documents are the lead), poplar plantation heights by age in Hungary, willow stems/ha, black
  locust stems per coppice stool, Hungarian 1985 headland widths, mid-October wheat seedling height, Hungarian verge
  mowing rules, field-protection belt widths and row counts (lead: Magyar Pál, *Alföldfásítás II*, 1961), maize stalk
  stubble height under 1985 Hungarian harvest practice, standing maize height of 1980s hybrids.
- **Read only in summaries [S]:** the Far Cry 5 and Wildlands GDC talks themselves, the Kevey willow and Nyírség
  papers' layer numbers, the black locust and poplar figures, the kunhalom and dune dimensions, the dyke profiles.
  Each is worth a look in the full text before a value becomes a test target.
- **Lead:** the Zenodo dataset of 233 Hungarian strict-reserve plots (stand height and density per plot) would give
  oak-hornbeam stems/ha directly; it is an Excel download, so the owner decides.
- **Lead:** Deussen 1998's competition rules and Makowski 2019's priors were not read on the page; read before
  building an offline self-thinning step.

## 8. Main sources (by section)

- HZD: https://www.slideshare.net/slideshow/gpubased-procedural-placement-in-horizon-zero-dawn/72897079 ,
  https://www.guerrilla-games.com/read/gpu-based-procedural-placement-in-horizon-zero-dawn
- Wildlands: https://80.lv/articles/procedural-technology-in-ghost-recon-wildlands/ ,
  https://80.lv/articles/landscape-and-material-pipeline-of-ghost-recon-wildlands/ ,
  https://80.lv/articles/vegetation-generation-in-ghost-recon-wildlands/ , https://gdcvault.com/play/1024029/
- Far Cry 5: https://80.lv/articles/houdini-procedural-world-generation-of-far-cry-5/ ,
  https://gdcvault.com/play/1025557/Procedural-World-Generation-of-Far
- Arma Reforger: https://community.bistudio.com/wikidata/external-data/arma-reforger/ArmaReforgerScriptAPIPublic/interfaceForestGeneratorOutline.html
- UE PCG: https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-reference-guide-in-unreal-engine ,
  https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine
- Houdini: https://www.sidefx.com/docs/houdini/heightfields_cop/erosion.html
- CityEngine: https://doc.arcgis.com/en/cityengine/latest/cga/cga-seedian-attribute.htm
- MSFS 2024: https://docs.flightsimulator.com/msfs2024/retail/content-configuration/environment/vegetation/biome-xml-properties/index.md
- Frostbite: https://www.advances.realtimerendering.com/s2007/Andersson-TerrainRendering(Siggraph07)-CourseNotes.pdf ,
  https://www.slideshare.net/slideshow/terrain-in-battlefield-3-a-modern-complete-and-scalable-system/11989404
- Validation: https://dev.epicgames.com/documentation/en-us/unreal-engine/screenshot-comparison-tool-in-unreal-engine ,
  https://www.ea.com/frostbite/news/photogrammetry-and-star-wars-battlefront
- Scatter: https://www.cs.ubc.ca/~rbridson/docs/bridson-siggraph07-poissondisk.pdf ,
  https://www.johanneskopf.de/publications/blue_noise ,
  https://search.r-project.org/CRAN/refmans/spatstat.random/html/rThomas.html ,
  https://diglib.eg.org/handle/10.1111/cgf13627 , https://doi.org/10.1145/2766975 ,
  https://diglib.eg.org/handle/10.1111/cgf13107 , https://graphics.stanford.edu/papers/ecosys/ ,
  https://algorithmicbotany.org/papers/eco.gi2002.html ,
  https://research.google/pubs/synthetic-silviculture-multi-scale-modeling-of-plant-ecosystems/ ,
  https://www.jcgt.org/published/0009/03/02/paper.pdf
- Terrain: https://www.cs.purdue.edu/homes/bbenes/papers/Genevaux13ToG.pdf , https://diglib.eg.org/handle/10.1111/cgf13657
  (Galin et al. 2019, review of terrain modelling)
- Real-world numbers: the links in section 3's tables.
