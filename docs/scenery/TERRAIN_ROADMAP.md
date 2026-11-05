# The terrain: a roadmap (track T)

**Part A (5 Nov 2026) is the reworked roadmap and comes first.** Part B (from section 1 on) is the roadmap of 3 Nov,
kept for its research, its rules and the design of what was built (T0-T4, T5.1); where the two disagree, part A wins.

## A. The rework (5 Nov 2026)

The project owner, 5 Nov 2026: "Something here is clearly not working. While the ground texturing and structuring
phase seems to have worked, the layering of trees, foliage, and other assets to make the world appear 'realistic' is
failing. Things are sparsely dispersed, the map is near completely flat, and many of the assets are difficult to import
and don't look good." Look at other games (Kingdom Come: Deliverance, farming simulators, games with forests, crops,
plains and hills); at how assets are better gathered, edited and built; at the full toolsets of the Unreal editor and
Blender, terrain tools above all; at proceduralism, since the map cannot be edited by hand; at a way to texture the
whole map methodically for the long term; and at **small test terrains (about 50 x 50 m: field types, forest types,
river, road), perfected and then applied procedurally to every matching part of the map**.

The research behind part A (5 Nov 2026, every claim linked):

| Note | What |
|---|---|
| `docs/research/TERRAIN_GAMES_2026-11.md` | KCD 1-2, Farming Simulator 22/25, Horizon, Far Cry 5, Ghost Recon Wildlands, Ghost of Tsushima, The Witcher 3, MSFS 2024, Hunt and others: relief, ground blending, vegetation layers and densities, edges, villages; 21 lessons |
| `docs/research/TERRAIN_UE_TOOLS_2026-11.md` | UE 5.8's Landscape (scriptable only from editor C++), PCG (production-ready; GPU scatter Beta), RVT, Nanite foliage, instanced skinned meshes for Megaplants, impostors; three ground options compared; a spike |
| `docs/research/TERRAIN_ASSET_TOOLS_2026-11.md` | Tree generators (Sapling 1.0, tree-gen, MTree, SpeedTree, The Grove, PVE), grass and crop cards, terrain generators, Material Maker, scanning, AI 3D (Meshy, Tripo, Hunyuan3D, TRELLIS, SF3D); a pipeline per asset kind, candid about what will not look good |
| `docs/research/TERRAIN_PROCEDURAL_METHOD_2026-11.md` | Recipes applied by rule (Horizon, Wildlands, Reforger, PCG Biome Core, MSFS), scatter mathematics, the plain's real numbers per plot type, relief at several scales, the test-plot catalogue (17 plots, 7 transitions) and how each is judged |

### A.1 What went wrong

Judged from the views of 4-5 Nov (`out/views/veg6/`): V1 is a bare textured field to a flat horizon; V10 fields with
dark box houses and almost no trees on the tracks; V12 low-poly "lollipop" trees in front of a flat green wall (the
wood mass). The causes, in order of weight:

1. **The wrong layer first.** Part B worked from the ground (T1) straight to hero trees (T5). In every game studied,
   what makes the view near the eye read as real is the **ground cover** (grass, stubble, stalks, weeds at about one
   clump per m² to 60-100 m: Farming Simulator, Arma, Tsushima) and **layered edges** (herb fringe, shrubs, mantle,
   trees), not the trees' detail. T6 (clutter) was never built, so every view is bare earth.
2. **Woods as a single layer.** simcore plants one kind of tree at one spacing in the woods' open sides and draws the
   rest as a smooth mass. Real woods and edges are a canopy, a sub-canopy and shrubs, with a 10-20 m mantle at the edge
   (Horizon's 20+ stacked layers, Wildlands' cascade of big trees spawning small trees spawning bushes, Reforger's edge
   levels).
3. **Relief at one scale only.** The map's relief is three noise bands (10 m over 18 km, 4 m over 6 km, 1.2 m over
   1.5 km) on a 50 m grid. The plain is flat, but not as flat as that: it has swells of 0.5-3 m, old river channels,
   sand ridges 6-8 m high, kunhalom mounds, dykes of 3-6 m, road embankments, ditches and canals, and its ground has
   clods, furrows and ruts at the centimetre scale. None of it is there.
4. **An asset pipeline fighting the engine.** Fab's assets were exported out of Unreal, through Blender and back in;
   Megaplants (skeletal) cannot be exported at all, Nanite shrubs decimate into solid blobs. Hours per asset, and the
   results looked wrong. The home-made card tree is a start, not a tree.
5. **A slow loop judged on the whole map.** One look meant loading the 40 km map and rendering twelve views (about
   four minutes), and nothing was perfected in one small place before being spread everywhere. Every studio studied
   defines a land type once on a test area, as data, and applies it by rule.
6. **A bug that spoiled the judging.** At V12 the procedural tree and the card tree were drawn in the same place:
   the distance ladder's start distances (`SimMapActor.cpp`, `Add(..., StartM)`) do not take effect, so the procedural
   crown is drawn from 0 m under every registered model.

### A.2 The decisions

| # | Decision | Why (the research) |
|---|---|---|
| D1 | **Keep the sim-owned mesh ground; do not move to a Landscape.** The ground tiles move from procedural mesh sections to static-mesh tiles (Part B 6.3 step 1, brought forward), for RVT, Nanite displacement of the centimetre scale and shadow caching | A Landscape can be made only by editor C++ before play, one grid per map, and the sim would follow the engine's grid. What it alone unlocks (Landscape Grass, Water carving, splines) PCG and simcore already cover (`TERRAIN_UE_TOOLS` 7). Kept as a fallback |
| D2 | **Test plots first, as small worlds generated by the same simcore code, then the same recipes on the whole map.** A plot is never a hand-made scene: perfecting a plot perfects the rule | The owner's method; Horizon, Wildlands, Reforger and PCG Biome Core all work this way (`TERRAIN_PROCEDURAL_METHOD` 1, 6) |
| D3 | **Recipes as data** (`data/scenery/recipes/`): per land type and per edge (a pair of neighbours), bands across a signed distance from simcore's vectors, scatter processes and densities per hectare, ground layers, relief stamps, test targets; every number sourced or marked estimate | One place per look; no painted value anywhere on the map, so a recipe change reaches every matching cell (`TERRAIN_PROCEDURAL_METHOD` 6.3) |
| D4 | **Anything that blocks sight or is over about 0.5 m stays in simcore** (trees, shrubs over a man's height, standing maize, dykes, embankments, ditches); **ground cover under that may be generated by Unreal** (PCG fed by simcore, or the map actor's own ring), never scaled by quality if it hides a man | Part B rules 1 and 2; PCG's points are not guaranteed identical on every client, so it may place only what nothing in the game reads (`TERRAIN_UE_TOOLS` 3, Epic staff, May 2026) |
| D5 | **Relief at three scales**: large forms from the map's data (unchanged); meso relief (0.3-8 m: swells, palaeochannels, sand ridges, mounds, dykes, embankments, ditches, headland ridges) in simcore by zero-mean band-limited noise and cross-section stamps along the map's lines; micro relief (under 0.3 m: clods, furrows, ruts) in the ground's normal and displacement only | `TERRAIN_GAMES` lesson 8, `TERRAIN_PROCEDURAL_METHOD` 4; anything a sight line could meet is simcore's |
| D6 | **Fields carry data, not a picture**: a field-state texture baked from simcore (crop, state, furrow heading, distance to the parcel's edge, the field's own day) read by both the ground material and the ground cover | Farming Simulator's density maps (`TERRAIN_GAMES` lessons 1-2) |
| D7 | **Assets: our own, built by script, from free photographs; Unreal assets used in place, never exported.** Trees from a branch skeleton (Blender's Sapling or tree-gen, headless) with our twig cards on its end branches, LODs by thinning cards, impostors baked in Unreal; grass, stubble, maize, reeds as our own card clumps from CC0 atlases; buildings from our own Blender generator. A Fab or Megascans asset is registered by its Unreal path and drawn as it is (Megaplants as instanced skinned meshes: a spike), never sent through Blender. AI 3D only for props. No more Fab gathering except leaf and ground textures | `TERRAIN_ASSET_TOOLS` 8; `TERRAIN_UE_TOOLS` 4 and 8 |
| D8 | **External terrain tools are not used** (Gaea, World Machine, World Creator, Houdini): non-commercial or paid to script, and they would take the ground from simcore. A clamped droplet erosion, ported from MIT code, may run on the detail levels only | `TERRAIN_ASSET_TOOLS` 3 |
| D9 | **Judged at three cameras per plot** (1.7 m eye; 25 m and 150 m at 30° down), beside the plot's photographs and its last approved render, with simcore's measurements (stems per hectare, spacing, band widths, obstacle heights, determinism) to catch drift; the owner judges by eye | `TERRAIN_PROCEDURAL_METHOD` 6.7 |

Paid tools stay the owner's choice and are not assumed: SpeedTree Indie (USD 19 a month, under USD 100k revenue) is
the cheapest route to good trees under 10 m; The Grove (EUR 99 and up) the most realistic cheap generator; Houdini
Indie (about USD 299 a year) the strongest scripted pipeline (`TERRAIN_ASSET_TOOLS` 8.2).

### A.3 The phases

The new phases are **L0-L10** ("land"). Part B's T0-T4 and T5.1 stay built and are used by them; Part B's T5-T9 are
replaced by the phases named in the last column.

| # | Phase | Done when | Replaces |
|---|---|---|---|
| L0 | **The rework**: the research, this part, the decisions | this part written | |
| L1 | **The plot lab.** Plot worlds: `data/scenery/plots/<plot>.json`, each a 50 x 50 m core in a 400 x 400 m ring, a tiny map (a parcel, a wood outline, a track, a road, a stream, a sky clock) turned into a map by the same generator path (`terrain_map.hpp`); the same plot also found on Kesmark by an anchor, as `views.json` does. `-SimShotSet=plots`: the three cameras per plot, each plot's photographs and its last approved render on one page (`tools/plot_lab.py`). The ladder bug fixed (A.1 item 6) | plot 1 (ploughland) renders from the three cameras **in under 90 s** from one command, the page beside its photographs | T2's loop for look work |
| L2 | **Recipes.** `data/scenery/recipes/` and its simcore loader: matching by cover, crop state, stand, edge pair and signed distance (from the vectors, not the 25 m raster), wetness, soil, road class; generation per cell with position-hashed seeds, big things first and children round them; density by thresholding a fixed ordered point set (so thinning never moves a tree); blue noise, lattices, Thomas and Matérn clusters; outputs: instances, ground weights, relief stamps; the report (hectares, instances and budget per recipe, measured statistics against each recipe's targets). Today's `map_dressing.json` rules move into recipes one by one | the tests: determinism (one hash per plot), every target met on its plot, the Kesmark map's obstacle heights unchanged where no recipe changed | part of T4, T5 |
| L3 | **The ground's shape** (simcore). Meso relief by recipe and by the map's lines: swells, palaeochannels, sand ridges, kunhalom mounds, road embankments and ditches, canals, dykes, headland ridges, as noise bands and cross-section stamps merged by distance; the ground's spacing per ring chosen by measure (plots at 1-2 m; the play area's rings at 5-10 m where the stamps need it). The balance runs re-read (sight lines move) | plots 13, 15 and 17 show their embankment, ditch, dyke and mound at 1.7 m; `ground_check`, `crew_balance`, `air_war_bench`, `em_demo`, `helper_coverage` re-read and the moved rows explained | part of T8, T9 |
| L4 | **The ground's surface.** The field-state texture (D6) at 2 m in the detail areas and 8 m over the map; the ground's layers chosen per pixel by recipe rules (cover, state, edge distance, wetness), at most four blended; the colour map synthesized from the vectors (Part B's T3 finished); anti-tiling (two variants per texture, macro noise); the ground tiles as static meshes with RVT (meshes blend into the ground) and Nanite displacement for the micro relief | plots 1-6 at 1.7 m and 150 m: furrows, clods, stubble rows and headlands read; no tiling peak in the 150 m render's luminance | T1's left, T3 |
| L5 | **The ground cover** (the near ring). Grass, weeds, stubble straw, maize stover and stumps, ploughed clods, verge and pasture sward, generated round the camera to 60-100 m from the field-state texture and the recipes: first the spike (PCG with a "Sim Cover Points" node on the GPU, against the map actor's own instanced ring), then the winner. Standing maize and sunflower as field blocks (rows of cards near, a block far), their height in simcore's obstacles | plots 1-6 and 13 at 1.7 m read as their photographs; the ring's cost within budget; points on the drawn ground within 1 cm; two clients alike | T6 |
| L6 | **The asset kit** (D7). The card-clump builder for grass, crops and reeds; the tree builder (skeleton, twig cards on the end branches, crown-hull normals, vertex-colour AO, LODs by card thinning, three or four variants per species); impostors baked in Unreal; registering an Unreal asset by its path; the Megaplants spike (1000 oaks as instanced skinned meshes, a Shipping build). One species judged at a time at its plots, starting with black locust and hybrid poplar | each kit item at its plot's three cameras beside the photographs; the owner's look | T5.2-T5.3 |
| L7 | **Woods, edges and lines** (simcore, sight-blocking). Stands as layers (canopy, sub-canopy, shrubs) by recipe: oak-hornbeam, black locust, poplar plantation, floodplain willow; the forest edge's herb fringe, shrub belt and mantle with noise along it and by aspect; tree rows on tracks and roads; hedges; reed belts on canals and oxbows. The wood masses far off take the same colour and outline | plots 7-12 and 14 at the three cameras; the obstacle heights test; the balance runs re-read | T5, part of T8 |
| L8 | **The whole map.** Every recipe on Kesmark (and the detail areas), the report's hectares and budgets, the views V1-V12 and each plot's map anchor | V1, V4, V6, V7, V9, V10, V12 against their photographs; the budgets held on this PC's RTX 3060 Ti | |
| L9 | **Villages and farms.** Plot 16 and its transitions; our own building generator (the Kádár cube first, then the long house, barns, the TSZ's sheds, water towers); fences, poles and wires; gardens and fruit trees by recipe; Part B's T7 for the rest | V2, V3, V5, V6 against their photographs | T7 |
| L10 | **Far field and cost.** Impostor rings and the masses meeting without a seam; baked Nanite ground tiles if they measure better; budgets per pass at every view | each view within budget | T9 |

**Order:** L1 and L2 first, together (the lab is useless without recipes, recipes untestable without the lab). Then
**one vertical slice: plot 1, ploughland, through L3, L4 and L5**, to prove the whole chain on the October plain's
largest surface. Then plots 2-3 (maize), then the forest edge and the tree row (11, 12) through L6 and L7, then black
locust and poplar, the road (13), and the rest in `TERRAIN_PROCEDURAL_METHOD` 6.6's order. L8 after each group of plots
(a cheap check, since the recipes already apply map-wide). L9 and L10 last.

**Paused:** gathering from Fab (except leaf and ground textures); the Megaplants and GV exports through Blender; the
hand tuning of `card_tree.py`'s hornbeam (it becomes L6's tree builder's leaves); the Sketchfab landmarks and the AI
prop test (after L9 starts).

### A.3.1 L1 built in first form (5 Nov 2026)

- `simcore/scenery/plots.hpp` (`LoadPlots`, `ResolvePlot`; tests `test_plots.cpp`), `data/scenery/plots.json` (7 plots: 1
  ploughland, 2 maize stover, 3 standing maize, 5 wheat stubble, 11 forest edge, 13 road, 14 river bank), the anchor kind
  `field_layer` (the nearest field whose whole 50 m square is one parcel in one ground layer on the day), the views'
  anchor reader and spot finder shared (`ParseAnchor`, `ResolveSpot` in `views.hpp`).
- `-SimShotSet=plots [-SimPlots=<id>,<id>]` in `ASimMapActor`; `python tools/plot_lab.py run|page|approve`.
- **The plots are found on the Kesmark map for now**, not in synthetic plot worlds: a run of one plot takes 74 s and
  all seven (21 cameras) 119 s, already inside the 90 s a plot, so plot worlds wait until a land type the map lacks
  (the dyke, plot 15) or a controlled layout is needed.
- **The baseline, `out/plots/a0/`** (what the slice must fix):
  - Plot 1: one corduroy furrow pattern with no clods and no change of tone, repeating plainly at 25 m; the parcel's
    edge a soft wavy blob at 25 and 150 m where it must be straight; dead flat; the trees low-poly lollipops.
  - Plot 3: standing maize drawn as scattered bulrush cards on bare stubble (the clutter ring's card for `dry_stalks`).
  - Plot 11: a wall of lollipop crowns with the card tree's white shards in them (the ladder bug) and small green
    shrub cones; no fringe, no shrub belt, no mantle.
  - Plot 13: the road a flat grey strip, no shoulder, ditch, poles or verge grass; sparse dark tufts on the field.
  - Plot 14: the cameras stood over the water (`side_m` 0; set to 60 m, not yet re-rendered).
- Left: the tree ladder's bug (the procedural crown drawn under the models from 0 m) is to be settled at plot 11 with
  the tree builder's own LODs (L6) rather than the ladder; the photos' page shows only the photographs with a page link
  that `terrain_views.py` can thumbnail.

### A.3.2 The ploughland slice, first pass (5 Nov 2026)

- **a1, the field's edges**: the material's field port found the plot's field only when its nominal centre's cell was
  on the mask; it now tries the field's centre, the pixel's own cell and the next cell towards the centre, so a
  parcel's edge is straight wherever the field lies (checked with `-SimGroundDebug=2`). The edge against a road verge
  is still a 25 m cell's (plot 13's job).
- **a2-a3, the surface**: a layer's `patch` (damp and dry patches 15-40 m across, darker and smoother when damp) and
  `break_up` (a second sample shifted along the furrows in blobs about 8 m across) in `ground_layers.json`, carried to
  the material as `PatchA..D` and `BreakA..D` by `ASimMapActor`; ploughed and sown 0.35 and 1.0.
- **Clods**: the clutter kind `clod` (`clutter_clod` in `ClutterMesh`, a rounded solid, not a card) on ploughed (1.8 a
  square metre at the camera) and sown (0.6) ground, 10-36 cm across.
- **Meso relief**: a detail area's `meso` bands (`MapDetailDef::meso`, zero-mean value noise, faded as the micro
  relief is, off the lines, sites and towns); battery 21's district has 2.5 m at 700 m, 1.2 m at 250 m and 0.3 m at
  90 m. The farm tracks (not among the lines the relief keeps off) take a point at each detail spacing, so they lie
  on the swells (within 0.14 m of the ground; 0.32 m with their 25 m points). **Sight lines in the district move**, re-read 5 Nov: `ground_check` passes (0 of 15); `crew_balance`'s rows are
  the same to the digit with and without the bands (its scenarios are not in the district); `air_war_bench` matches
  `09_BALANCE.md` and `em_demo` passes 11 of 11; `helper_coverage` keeps its 25 gaps, and only
  `kesmark_range_test` changes, for the better (the NPC commander launches at 785 s, not 814, and hits at 875).
- **What a3 shows**: clods read from the eye camera and the damp patches from 150 m; the furrows are still one
  corduroy scan repeating every 2 m, which `break_up` along the furrows cannot hide across them, and the swells do not
  read in a 50 m plot from these cameras. Next: a cloddy scan of its own for ploughland (fetched only with the owner's
  say-so) or a procedural furrow normal, the camera's sun lower to show the relief, then the recipe file (L2).
- **a4-a5, procedural furrows** (the owner chose them over a scan): a rows layer's `furrow_m` and `furrow_depth_m`
  (`FurrowA..D`, `FurrowDepthA..D`) draw a mouldboard's slice in the material, a long back and a steep face, each
  field's spacing its own within 12%, wavering over metres, broken into lumps a hand or two long, crests paler and
  troughs darker; the scan's furrows across are damped by a third under them and the second sample shifts across too.
  Ploughed 0.36 m and 0.09 m, sown (drill rows) 0.15 m and 0.03 m. a4 (0.12 m, smooth profile, scan damped 65%) read as
  corrugated sheet; a5 reads as ploughland from the eye. They fade once a pixel spans a fifth to a half of a furrow, so
  from 150 m the scan's own 2 m grid still showed at middle distance, on the stubble as well.
- **a6-a7, the middle distance**: from `MidStartM` to `MidEndM` (35-250 m, material parameters) every layer fades
  by `MidAmount` (0.7) towards its average colour varied by a noise about 3 m across (±7%), and its normal softens,
  before the far fade at 400 m. a6 (from 50 m, ±10%) left a faint grid near the camera and a cloudy stubble; a7 has
  neither from 150 m, and the furrows still read from 25 m and the eye. Next: the lower sun to show the relief, then
  the recipe file (L2) and the next plots.

### A.4 How a plot is worked (the loop)

1. Write or change the recipe (one JSON); sources in its `source`, numbers an estimate where none was found.
2. simcore tests for the plot: its statistics against the recipe's targets, determinism, obstacle heights.
3. `python tools/plot_lab.py run <plot>`: the plot world rendered from the three cameras (and its map anchor's), the
   page beside the photographs and the last approved render; under 90 s.
4. Look at every picture; change; repeat. When it reads right, show the owner.
5. On the owner's approval the renders become the plot's ground truth; later renders are compared against it.
6. Run the map-wide report and the views (`terrain_views.py`) to see the change everywhere; the balance runs if
   anything that blocks sight moved.

---

# B. The roadmap of 3 Nov 2026

The project owner, 3 Nov 2026: "Develop a roadmap - include terrain research (reference images), online/FAB/unreal
marketplace 3D model selection, PBR and texture selection (must be free), and the development process. Then begin
development." Asked after the question of how to make the map look like the AA and AAA maps of Eastern Europe, using
existing models, efficiently.

Track T carries on from R6 ("a more realistic map", `docs/DEVELOPMENT_ROADMAP.md`), which built the map's data (the
land cover, the dressing, the detail areas, the obstacles) and drew it plainly. Track T makes it look like the place:
the Great Hungarian Plain on 15 October 1985.

The research behind it (3 Nov 2026, every claim with its link):

| Note | What |
|---|---|
| `docs/research/TERRAIN_REFERENCES_2026-11.md` | 92 openly licensed reference photographs of the plain (1931-1986 and unchanged things today), 16 landscape traits with sources, 10 camera views to check the map against |
| `docs/research/TERRAIN_AAA_MAPS_2026-11.md` | How S.T.A.L.K.E.R. 2, Arma 3, DayZ, Reforger, Squad, Tarkov, Gray Zone Warfare, Hell Let Loose, Enlisted, MSFS, DCS and IL-2 build their ground, and 18 lessons for SAM3D |
| `docs/research/TERRAIN_TEXTURES_2026-11.md` | Free PBR textures for every ground layer and building surface, with licences and scan sizes |
| `docs/research/TERRAIN_VEGETATION_2026-11.md` | Free trees, shrubs, reeds, grass and crops (Megaplants, Megascans, PlantCatalog, Poly Haven) |
| `docs/research/TERRAIN_BUILT_2026-11.md` | Free buildings, props, infrastructure, military position items and period vehicles |
| `docs/research/TERRAIN_PICKS_2026-11.md` | Every pick above (215) with its licence and price checked again by a second reader: 205 confirmed, 9 uncertain, 1 refuted |
| `docs/research/TERRAIN_GROUND_MATERIAL_2026-11.md` | Texturing a runtime-generated ground in UE 5.8: the mask texture, world position under large world coordinates, layer blending, anti-tiling, texture arrays from a commandlet |
| `docs/research/TERRAIN_GEOMETRY_2026-11.md` | The ground's geometry (procedural mesh, runtime static meshes, baked Nanite tiles), 240 000 trees, clutter, the far field, measuring cost |

## 1. The rules

These hold through every phase.

1. **One ground** (R5). The ground drawn is simcore's triangles. Nothing render-only changes its shape by more than a
   normal map's few centimetres. Pads, road beds, ditches and revetments are terraformed in simcore, so the sight
   lines see them too.
2. **What blocks sight is drawn at every quality setting.** Every tree, tree row, wood mass and building in a sight
   line is drawn out to the crew's visual range, whatever the settings. Only grass, small clutter, shadows and light
   may scale (lesson 7 of the AAA notes: Gray Zone Warfare draws the same vegetation on every PC; Tarkov had to fix
   bots seeing through bushes).
3. **Free only, and registered.** Every third-party file is free and in a register before it is used: models in
   `data/scenery/assets.json`, textures in `data/scenery/textures.json`, each with its origin, author and licence.
   The licences allowed are CC0-1.0, CC-BY-4.0 (credited), Fab-Standard and Epic-Content-Only. A file is fetched only
   with the owner's say-so (`docs/scenery/ASSETS.md`). Files that may not be redistributed (Fab, Epic) live under
   `unreal/SAM3D/Content/ThirdParty/`, fetched per machine. CC0 and CC-BY files are fetched per machine too, into
   `art/thirdparty/` (ignored by git), until their size in git is agreed.
4. **The map's data is simcore's.** Where things stand, which class a cell is, what a field grows on the scenario's
   date: all of it is simcore data, read by every client alike (`docs/MULTIPLAYER.md`). Unreal draws it. A look that
   changes what blocks sight goes through simcore and the balance runs (R6's rows).
5. **Every machine runs without the third-party files.** A missing model is drawn procedurally and a missing texture
   in the land cover's colour, and the log says so once.
6. **Checked against the place.** Every phase ends with `-SimMapShots` at the reference views (section 2.3), set
   beside their photographs, and a look by the owner. Claude renders the shots offscreen and looks at them; in the
   live editor it follows `unreal/README.md`'s "Working in the live editor", and never places the map by hand there.

## 2. Terrain research: reference images

### 2.1 The photographs

92 photographs, each a link to its page (nothing is copied into the project), all openly licensed (CC BY-SA, CC BY or
public domain), mostly from Wikimedia Commons and Fortepan's copies there. Four further pages are leads, to be looked
at only. The period is covered by Jean-Pierre Bazard's colour road trip across the plain (1974), Fortepan's MHSZ
aerials (1975-1984), the 1983 Csanádapáca series and a 1986 roll film. Modern photographs are used only for what has
hardly changed: soil, crops, reeds, oxbows, oak and beech wood, shadoofs, the concrete water towers and silos of the
1960s-80s, pylons and poles. The table, with what each one shows and which view it serves:
`docs/research/TERRAIN_REFERENCES_2026-11.md`.

### 2.2 What the plain looks like (the traits that matter for the map)

| # | Trait | What it means for the map |
|---|---|---|
| T1 | Flat to rolling, the highest point 183 m; the horizon is trees, towers and spires, not hills, except the northern edge | Tree lines and landmarks carry the far view; haze matters more than relief |
| T2 | Collective (TSZ) fields of **100-200 ha** in the 1970s-80s; the groves between them cleared (Erdészeti Lapok 1986) | The dressing's parcels (250-700 m by 80-250 m, 2-17 ha) are far too small for TSZ land; small strips stay right for household plots at the village edge (phase T4) |
| T3 | Tree rows along the farm tracks (dűlőút), not belts round every field; farmyards planted with hybrid poplar and black locust from 1963 | Rows on tracks and round majors, not on every field edge |
| T4 | Black locust 22.5% of Hungary's forest; turning colour by late September; pine and locust on the sand | The autumn tint per species; locust everywhere |
| T5 | Each tanya (farmstead) in its own island of trees | A tanya kit: house, trees, shadoof, haystacks |
| T6 | Street villages on long strip plots: house at the street end, gable to the street, farm buildings and garden behind | The village kit's layout |
| T7 | The Kádár kocka: about 10 × 10 m, pyramid roof, two-colour render; 800 000 built 1959-80 | The commonest house of 1985, beside the older long houses |
| T8 | Village side streets 2.8-3.2 m of asphalt, main streets 3.5-4.5 m; earth ditches, grass verges | Road widths by class; the verge layer |
| T9 | Low-voltage poles of wood, concrete (the perforated B12) or lattice steel, 7-12 m, spans 35-40 m | The pole lines along roads |
| T10 | The shadoof (gémeskút) at tanyas and on pastures | A prop |
| T11 | **Mid-October: harvest and sowing under way**: a patchwork of fresh ploughland, harrowed and sown ground with a faint green, maize stubble with stalk bales, and maize still standing in some fields | The autumn crop state, a mix judged by eye against the photographs, not by statistics: Pannonia is fictional (phase T1's season, T4's calendar) |
| T12 | October in Budapest: 12.7 °C, 141 hours of sun, 39 mm rain; autumn night fog on the plain | The haze and fog presets |
| T13 | A concrete water tower in every town and many villages (1960s-70s, 40-45 m), grain silos at the stations, dryers at the majors | The landmarks a crew sees above the tree lines |
| T14 | The Tisza and the Körös between flood dykes, floodplain forest inside, oxbows with reeds, dyke-crest roads | The river kit |
| T15 | The northern hills: beech on the Mátra, oak and steppe grass on the lower slopes | The hill woods |
| T16 | Hungary's air defence of 1986-89: 20 strategic sites (S-75M, S-125M, one S-200VE); the army's mobile air defence used the Kub. No Hungarian Osa unit was found | Our battery is fictional in a real landscape, as the briefing has it |

### 2.3 The views the map is checked from

Each view is a camera to render with `-SimMapShots` and set beside its photographs (phase T2 places them on the
Kesmark map).

| View | Where | Height | Must show |
|---|---|---|---|
| V1 Field edge | A TSZ field's edge on a farm track | 1.7 m | Furrow direction and scale, a field 1-1.5 km deep, the ploughed, stubble and maize patchwork, a locust row at 0.6-1 km, pylons, haze |
| V2 Village street | The main street of a street village | 1.7 m | Long houses and kocka houses set back, fences, poles and wires, verges and ditches, narrow asphalt |
| V3 Village from the air | Over a village's main street | 300 m, 45° down | Strip plots, gardens, the church in the middle, fields beyond |
| V4 Secondary road | A district road's verge | 1.7 m | About 6 m of asphalt, earth shoulders, a ditch, a pole line, a tree row, a farm track joining |
| V5 TSZ farmyard | On the concrete access road, 150 m out | 2.5 m | Long sheds, a dryer or silo, poplar or locust rows, a wire fence |
| V6 Tanya | In a field, 300 m from a farmstead | 1.7 m | The farm in its trees, a shadoof, haystacks |
| V7 Osa position | A battery position on the open plain | 3.5-6 m | Tree lines layered at 1-3 km, water towers, silos and spires, pylons, haze at 10-20 km |
| V8 Floodplain | On a flood dyke's crest | 5-6 m | Willow and poplar floodplain forest, an oxbow with reeds, the dyke road, a bridge |
| V9 Northern hills | On the plain 10-20 km south of the hills | 1.7 m | The wooded hills as a low wall behind October ploughland |
| V10 Low pass | Over mixed fields and a village | 150-500 m, 30° down | Field sizes and furrows, tree rows on the tracks, villages |

The gaps the research left (no openly licensed photo of a multi-row field-protection belt, of a 1980s farmyard or
slab fence from the ground in the period itself, of a Pact SAM site in Hungary; no fog statistics; no 1980s road
cross-sections) are listed in the references note. Its second part also sketches the views as data (anchors by the
map's ids, the photos' links as a register) for T2, and notes that V5 and V8 need a farmyard and a flood dyke the
Kesmark map does not have yet (T7, T8).

## 3. What the AA and AAA maps teach

From `docs/research/TERRAIN_AAA_MAPS_2026-11.md` section 14, the lessons that shape this roadmap:

1. **One colour source for the whole view.** Chernarus, Reforger, Arma and MSFS drape one colour image over the ground
   and make everything agree with it. SAM3D has no aerials of 1985 Pannonia, so it **synthesizes** the colour from
   simcore's data (class, crop, parcel, furrows, woods, roads, water), as MSFS does where aerials are missing (T3).
2. **Vector edges, not a staircase.** Field edges, verges and wood margins from simcore's polygons, not the 25 m cells
   (T3; T1 hides the cells with a few metres' wander and a height blend meanwhile).
3. **Few blended surfaces per pixel, chosen by data.** Arma 5 per tile, Reforger 5 (start with 3). SAM3D: a layer per
   cell from the land cover, at most four blended at a pixel (T1).
4. **Detail near, colour far, a middle layer between.** Arma fades detail out at 15-40 m; MSFS has near, middle and far
   detail. Near PBR layers, a macro layer in the middle, the colour alone far off (T1, T3).
5. **Keep the ground's grid, seat things on pads** (rule 1).
6. **Clutter to about 100 m, coloured by the map** (Arma, DayZ: one clutter model per m² to 125 m). On 15 October the
   plain is mostly stubble, ploughland and cut maize, so clutter is short and cheap; standing maize is better as field
   blocks (T6).
7. **Sight-blocking vegetation never scales** (rule 2).
8. **Four distance rings with budgets**: full trees to about 1.5 km, impostors to 6 km, wood masses to 40 km, the far
   ground and the atmosphere beyond (T5, T9).
9. **Trees take their colour from the same map**, so meshes, impostors and masses meet without a seam (T5).
10. **Generate the ordinary, hand-place the special, as data**; roadsides and villages as kits with rules (T4, T7).
11. **Season from the scenario's date** (DCS Kola's winter from the mission's date) (T1, T4).
12. **Haze is physical and matches the visual range simcore uses** (`data/gameplay/visual_range.json`) (T3).
13. **Budgets in milliseconds per pass on a named GPU, measured before detail is added** (T2, T9).
14. **Do not depend on Landscape-only systems** (Landscape Grass spawns only on Landscape actors; SAM3D's ground is a
    procedural mesh), and prefer stable features to Experimental ones (PCG is production-ready; Nanite Foliage and the
    Procedural Vegetation Editor are Experimental).

## 4. Textures: free PBR

All CC0, from Poly Haven and ambientCG (redistributable, no credit required; both sites' APIs serve the files). Fab's
free Megascans surfaces are alternates only (Fab-Standard: usable, never redistributed). ShareTextures is excluded:
its licence forbids redistribution in collections. Full tables with alternates: `docs/research/TERRAIN_TEXTURES_2026-11.md`.

### 4.1 The ground layers (phase T1: in the register)

The layers and their rules are `data/scenery/ground_layers.json`; the textures `data/scenery/textures.json`. Each was
checked again on 3 Nov 2026 (confirmed CC0, free, the page the right texture).

| Layer | Where | Texture (site, author) | Scan | Laid every |
|---|---|---|---|---|
| meadow | pasture, meadows | `leafy_grass` (Poly Haven, Charlotte Baglioni) | 2.0 m | 2.0 m |
| stubble | wheat in autumn | `Ground071` (ambientCG): soil strewn with straw | 1.75 m | 1.875 m, turned with the furrows |
| maize_stubble | maize in autumn | `Ground072` (ambientCG): chopped stalks on soil | 1.75 m | 3.0 m (stover is coarser), turned |
| ploughed | ploughland; sunflower in autumn | `farm_furrows` (Poly Haven, Amal Kumar): ridged furrows | 2.1 m | 2.0 m, turned |
| forest_floor | woods | `forest_leaves_03` (Poly Haven, Rob Tuytel, Dimitrios Savva): dry leaf litter | 2.3 m | 2.4 m |
| marsh | marsh | `mud_forest` (Poly Haven, eye-candy.xyz): wet mud | 2.35 m | 2.4 m |
| yard | town and village ground | `dirt` (Poly Haven, Charlotte Baglioni): packed earth | 2.0 m | 2.0 m |
| trampled | military ground | `muddy_tracks` (Poly Haven, Amal Kumar): tyre-churned mud | 2.25 m | 2.4 m |
| verge | the road cells | `grass_ground` (Poly Haven, Charlotte Baglioni): worn grass | 2.51 m | 2.5 m |
| bank | the water cells | `Ground054` (ambientCG): silt and sand | 3.5 m | 3.75 m |
| wheat, maize, sunflower | the summer's standing crops | none: no free scan of a standing crop exists anywhere searched | | land cover colour |

The repeat is near each scan's own width and divides 600 m (section 6.1). Fetching all ten at 2k downloads 204 MB
(`python tools/fetch_textures.py --sizes`), about 13-17 MB per Poly Haven texture as JPEG and 33-37 MB per ambientCG
zip.

### 4.2 The next ground layers

Harrowed and sown ground (T11's faint green): Poly Haven `farm_soil`, `raked_dirt` for the sand. Pine needles:
`forrest_ground_03`, ambientCG `Ground077`. Reed ground: `sparse_grass`. Farm tracks: `muddy_tracks` along the track,
ambientCG `Pathway002` with its opacity for the edges. Loess and rock in hill cuts: `excavated_soil_wall`,
`rock_face_04`. Far, anti-repetition layers: `aerial_mud_1` (8 m), `dirt_aerial_02` (20 m).

Where no free scan exists (a standing crop's rows, a drilled seedbed), a texture can be made as our own: procedurally
in Blender's nodes or in Material Maker (MIT, node graphs to PBR maps; `docs/research/TOOLING_POSTS_2026-11.md`),
registered as own work.

### 4.3 Roads and buildings (phases T7, T8)

| Surface | Primary (all Poly Haven CC0 unless named) | Note |
|---|---|---|
| Old asphalt | `asphalt_04` (4 m scan) | `road_damaged` only as a blended patch |
| Gravel, macadam | `gravel_road`, `tarred_gravel` | ambientCG `Gravel043` at sites |
| Concrete road slabs | `concrete_floor_damaged_01` | the joints drawn by the material from the road's UV (no scan has them) |
| Painted render | `painted_plaster_wall`, `beige_wall_002` | the colour a parameter per house |
| Whitewash | ambientCG `Plaster007`, `whitewashed_brick` | |
| Roof tiles, beaver-tail | `ceramic_roof_01`, `roof_3` | Western scans, mossy: desaturate the green |
| Roof tiles, interlocking | `roof_tiles`, `clay_roof_tiles_03` | |
| Corrugated asbestos cement | `asbestos_sheet_02` | rhombic slates: none found |
| Corrugated metal | `corrugated_iron_02`, `rusty_corrugated_iron` | |
| Brick | `brick_wall_006`, `red_brick` | |
| Precast panels | `concrete_slab_wall` | the joints from the building's panel grid |
| Planks | `weathered_planks`, `wood_planks_grey` | |
| Rusty steel | `rusty_metal_sheet` | |

**Gaps:** standing stubble, maize stumps and sunflower stalks (geometry, T6); reed beds (foliage); Hungarian pointed
beaver-tails and rhombic asbestos slates; Kádár-era spattered render; slab joints and PAG-14 plates (procedural);
loess columns; the real size of some ambientCG scans (Ground077, Ground103, Ground104, Plaster007).

## 5. Models: Fab, Epic, Quixel and online

Every pick is free. Fab and Megascans items are under Fab's Standard licence (use in a game, never redistributed: per
machine under `Content/ThirdParty/`). Sketchfab items are CC-BY-4.0 (credited) or CC0; Poly Haven is CC0. Fab's free
listings rotate: check "Free" and the licence on the day it is claimed. The full list with every page, author, size
and the second reader's verdict: `docs/research/TERRAIN_PICKS_2026-11.md`.

| Part | Primary | Alternate |
|---|---|---|
| Oak (`tree_oak`) | Megaplants English Oak (Fab) | PlantCatalog northern red oak, autumn (Sketchfab CC-BY) |
| Black locust (`tree_acacia`) | Megascans Black Locust (Fab) | Robinia hispida (Sketchfab CC-BY) |
| Hybrid poplar (`tree_poplar`) | Megaplants Black Poplar (Fab) | PlantCatalog black poplar (CC-BY) |
| Lombardy poplar | PlantCatalog Lombardy poplar (CC-BY) | Populus pyramidalis, game-ready (CC-BY) |
| Pine (`tree_pine`) | Megaplants Baltic Pine (Fab) | Poly Haven Pine Tree 01 (CC0) |
| Willow, birch, hornbeam, beech, maple, alder | Megaplants and Megascans Trees (Fab) | PlantCatalog (CC-BY) |
| Shrubs (hedges, elder, rose) | Megaplants European Spindle, Elder (Fab) | Free Shrubs Pack (Fab) |
| Reeds, bulrush, sedge | Megaplants Dry Common Reed, Common Bulrush (Fab) | Gen04-ReedsFree (CC-BY) |
| Grass, weeds, wildflowers | Megascans Dry Grass, Field Poppy; Megaplants Stinging Nettle (Fab) | Poly Haven Grass Medium 01, Nettle (CC0) |
| Maize, sunflower, wheat | Maize Corn Plant, SUNFLOWER (DRY), Golden Fields: Wheat (Sketchfab CC-BY) | Megascans Wheat Grass (Fab) |
| Panel blocks (`flats`) | Panelak; 9-floor panel building; 121-60-25 panel building (Sketchfab CC-BY) | UNIBLOCKS FREE (Fab), a modular kit |
| Village houses (`house`) | Ordinary Slovak/Slavic Village House; Village House USSR CIS (CC-BY) | Post-soviet house (CC-BY) |
| Barns, sheds, garages (`barn`) | Abandoned Soviet barn; Soviet brick storage/garage (CC-BY) | Old wooden barn (CC-BY) |
| Halls, silos (`industrial`) | Megascans Warehouse (Fab); Grain Silo (CC-BY) | Factory Pack Vol.1 (CC-BY); Poly Haven Modular Factory Facade (CC0) |
| Church | Dorfkirche Warchau (Sketchfab CC0) | Countryside chapel dataset (CC-BY) |
| Poles and wires | Poly Haven Modular Electricity Poles, Modular Electric Cables (CC0) | concrete and wooden pole models (CC-BY) |
| Pylons | High Voltage Transmission Line Tower (CC-BY) | 400 kV tower (CC-BY) |
| Fences | PO-2 Soviet concrete fence (CC-BY); Poly Haven chain-link (CC0) | wooden fence pack (CC-BY) |
| Well, shadoof, haystacks, bales | Shadoof; Old Rural Well; Haystack Bohmsdorf (CC-BY) | Megascans Round Hay Bale (Fab) |
| Roadside | Soviet bus stops, European road signs, milestones (CC-BY) | Megascans guardrail kit (Fab) |
| Railway, culverts, bridges | Megascans railway kit, concrete pipe (Fab) | tileable tracks, slab culvert, girder bridges (CC-BY) |
| Military position | Megascans trench sample, sandbags, barbed wire, concrete barrier (Fab); Poly Haven barriers and crate (CC0) | Soviet bunker; Czech hedgehog (CC-BY) |
| Period vehicles (parked) | Trabant, Lada 2101, Ikarus 260 and 255 (CC-BY) | 8 more left uncertain by the check (licence not readable on the page) |

**Must be built** (no fit found): the kocka house with its pyramid roof and two-colour render, the gable-end long
house, the TSZ's long livestock sheds and dryers, the water towers of the 1960s-70s (Körmend, Pápa types), the
concrete panel fence, the B12 perforated concrete pole. They come from simcore's procedural meshes extended, or from
Blender by script with the photographs (`docs/MODELLING_FROM_PHOTOS.md`), as SAM3D's own art.

## 6. The technical design

### 6.1 The ground material (phase T1, built)

The ground stays the procedural mesh R5 and R6 made: simcore's triangles, coloured per vertex from the 25 m land cover.
Textures are added by one material, `M_SimGround`, made on each machine from the fetched textures.

- **The mask** (`simcore/scenery/ground_layers.hpp`, `BakeGroundMask`): one texel per land cover cell, baked by
  simcore for each land cover square (the map proper and each detail area) from the cover, the crop and the season:
  R the layer, G the furrows' heading (`FieldHeading`, one per 3 km square), B the field's own shade (a draw per
  parcel), A whether the layer turns with the furrows. The season is the scenario's month (`autumn_months`). Every
  client bakes the same mask.
- **The layers**: three texture arrays (albedo, normal, roughness and occlusion), one slice per layer, 2048 square,
  built by `tools/ue_build_ground_material.py` in a commandlet (`USimEditorScripting::MakeTextureArray`).
- **Where a pixel is**: from the world position, not the mesh's UVs (a procedural mesh may keep UVs as 16-bit floats:
  steps of 30 m at 40 km). The double-precision position is cut into 600 m squares by Frac before anything is a float,
  so the textures neither swim nor block up far from the scenario's origin. Each layer's repeat divides 600 m (the
  loader refuses one that does not), so a texture runs on unbroken across the squares. The world goes to the map's
  frame by `MapFrameLink`'s affine map, passed in by `ASimMapActor`.
- **Furrows**: a turned layer lays its texture's U along the field's furrows (the textures' furrows run across the
  image, as `farm_furrows`' do) and turns its normals with it. Mip levels come from the exact position within its
  square, unwrapped at the square's edge, so the edges leave no seam and the mips do not flicker far from the origin.
- **Cells' edges**: the four cells round a pixel are blended by distance after a wander of a few metres, and by the
  textures' own brightness as a height, so a 25 m staircase does not show.
- **Distance**: each layer fades to its own average colour from 400 m to 2 km, then to the land cover's own colour
  (the vertex colours, as before T1) from 6 km to 12 km. The far ground beyond the map proper is coloured otherwise
  (tints over the country's coarse cover), so an edge remains where the two meet: T3's colour map takes it away. A
  macro noise in patches about 125 m and 370 m across varies the brightness, and each field has its own shade.
- **Without the textures** (a machine that has not fetched them): no material is found, the ground keeps its colours,
  and the log says so once. A layer whose texture is missing is drawn in the land cover's colour.
- **The test patterns**: with `SAM3D_GROUND_PATTERN=1` the builder gives every layer a pattern of its own colour, with
  furrow lines and ridges along U and a border round each repeat, so the turn, the repeats and the cells can be
  checked in the shots before any texture is fetched.

Cost: per pixel of the map proper and the detail areas, four cells, each a mask load and four array samples (albedo,
its average for the far fade, normal, roughness and occlusion): 16 samples and 4 loads. The far ground is untouched.
To be measured in T2.

### 6.2 The colour map and the far field (phase T3)

A colour texture per ground tile, synthesized from simcore's vectors (field polygons, road and track lines, wood
outlines, gardens, water) at a few metres a texel, shaded by slope and by tree presence, replacing the vertex colours
as the colour far off and the base the layers sit on. The far ground (500 m to 4 km grids) gets a coarser colour map
from the country's land cover. The sky's Mie scattering and the height fog are driven from the weather preset's
visibility, so the haze the player sees is the visual range simcore uses. The atmosphere's aerial perspective is tuned
for 150 km, and checked in the TV sight's narrow field.

### 6.3 The ground's geometry (phase T9)

Three steps, each kept only if it measures better (`docs/research/TERRAIN_GEOMETRY_2026-11.md`):

1. Runtime-built static mesh tiles (`FSimRuntimeMesh`, which the buildings already use) instead of procedural mesh
   sections, for Lumen cards and static shadow caching; or a dynamic mesh component on the static draw path.
2. Baked Nanite ground tiles, written by an editor commandlet that links simcore, with a manifest of the input data's
   hashes; the map actor uses them when the hashes match and falls back to step 1 otherwise. A test reads the baked
   triangles back against simcore's.
3. Mesh Terrain (5.8, Experimental) only once it leaves Experimental.

### 6.4 Vegetation (phase T5)

simcore keeps planting (its positions are what blocks sight). Unreal draws them as one instanced static mesh per kind
per chunk, Nanite where the asset has it, with Preserve Area on foliage; per-instance custom data for the species'
autumn tint and leaf state; wind only near; shadow caching beyond. Rings: full meshes to about 1.5 km, octahedral
impostors to 6 km, the wood masses beyond. Field-of-view-scaled culling keeps the TV sight's far trees.

### 6.5 Clutter (phase T6)

A ring of instances round the camera, about 100 m, generated from simcore's land cover on the exact ground: grass,
stubble rows along the furrows, maize stumps, reeds at the marsh's edge. No shadows, no ray tracing. Standing crops
(summer missions, the uncut maize fields in October) as field-scale blocks of crop meshes.

### 6.6 Budgets (phases T2, T9)

Milliseconds per pass (ground, trees, clutter, shadows, Lumen, atmosphere) on a named target GPU, captured by the
shots' harness at the reference views, before and after each phase. A fallback without Lumen kept in mind.

## 7. The phases

**Superseded on 5 Nov 2026 for everything not yet built**: T5-T9 are replaced by part A's L1-L10 (A.3). The table is
kept as the record of T0-T4 and T5.1.

| # | Phase | Done when | Status |
|---|---|---|---|
| T0 | **Research**: reference photographs and traits, the AAA maps, free textures and models with their licences checked twice, the ground's material and geometry in UE 5.8 | the notes in `docs/research/TERRAIN_*` and this roadmap | **Done (3 Nov 2026)** |
| T1 | **The textured ground**: the layers by cover, crop and season, a mask from simcore, the texture register and its fetch tool, the material made per machine, the map actor's materials | the shots show each layer's texture on its cells, the furrows turned with the fields, no seams, no tiling at the views, the textures fading into the land cover's colours far off without a seam | **Built in first form (3 Nov 2026)**; left below |
| T2 | **The views and the costs**: V1-V10 as `-SimMapShots` cameras on the Kesmark map, a page setting each shot beside its photographs' links, the milliseconds per pass at each view | the page renders from one command, before and after each phase | **Built in first form (4 Nov 2026)**: `T2_VIEWS_PLAN.md`; baseline `t1`; left below |
| T3 | **The colour map and the far field**: the synthesized colour per tile from simcore's vectors, the far ground's colour map, haze from the weather's visibility | field edges straight, no 25 m steps, the far ground meeting the map proper without an edge, haze matching the visual range at V7 and V9 | **Designed (4 Nov 2026)**; T3.0 (the marsh) and T3.1 (exact field edges in the material, `t31` views) built in first form; T4.3 (the crop calendar, `t43`) built; T3.2 (the far ground's land cover, `t32`, view V11) and T3.4 (`tools/haze_reading.py`: the haze matches Koschmieder where it can be read) built: `T3_T4_PLAN.md` |
| T4 | **The plain's layout** (simcore data): TSZ fields of 100-200 ha with household strips at the villages (T2), tree rows along the farm tracks (T3), tanyas in their trees (T5), strip-plot villages (T6), the crop calendar for the scenario's date (T11: harvested and standing maize, ploughland, sown wheat, mixed by eye) | the views V1, V3, V6 and V10 against their photographs; the balance runs re-read (the trees block sight) | **T4.1 and T4.2 built in first form (4 Nov 2026)**, the sight-line re-read and T4.3 (calendar) left: `T3_T4_PLAN.md` |
| T5 | **Trees and reeds from models**: the vegetation picks registered one at a time, drawn at simcore's positions, the autumn tint, impostors and masses in rings | V1, V7 and V8 against their photographs; the tree budget held; sight lines unchanged | **T5.1 built (4 Nov 2026)** (the autumn tint per tree, `t51`); T5.2-T5.3 planned: `T5_T8_PLAN.md` |
| T6 | **Clutter and crops**: the camera's ring of grass, stubble and stumps; standing crops as blocks | V1 at 1.7 m reads as a field; the cost within budget | planned |
| T7 | **Villages and farms**: the kocka and long houses built, the picks registered, the village, tanya and farmyard kits as rules, building surfaces textured, poles and wires, fences, water towers, silos | V2, V3, V5 and V6 against their photographs | planned |
| T8 | **Roads, rivers and the battery's position**: road surfaces by class with slab joints, verges, ditches and tracks; rivers between dykes with reeds and floodplain wood; the position's pads and revetments in simcore (rule 1) | V4, V7 and V8 against their photographs | planned |
| T9 | **Geometry and cost**: runtime static mesh tiles, then baked Nanite tiles if they measure better; budgets on the target GPU | each view within budget | planned |

**T1 checked with the test patterns (3 Nov 2026)**, in `-SimMapShots` (`out/shots_pattern/`): every layer on its cells
by the mask, in the map proper and both detail areas; the cells' edges wandering, not stepped; no moiré far off once the
texture arrays have their mips (a new `UTexture2DArray` has none: `MakeTextureArray` now gives it the slices' settings
after its update); the textures fading into the land cover's colours far off without a seam (the edge between the map
proper and the far ground beyond, coloured otherwise, is as it was: T3's). A new view, `furrows`, stands
1.7 m over the turned field nearest the battery, well inside it, looking along its furrows: the pattern's furrow lines
run straight to the vanishing point, so the texture turns with the field and its ridges shade across it.

**T1 with the real textures (4 Nov 2026):** the ten fetched on the owner's say-so, their SHA-256s recorded, the
material rebuilt with them and the views rendered (`out/shots_textured/`). `farm_furrows` is a bed with a trench and
grass at each side, so laid end to end it broke the furrows every 2 m; the register now keeps only its ridged middle
(`crop_u`), and the furrows run unbroken to the horizon.

The ground then rendered as light sand: the outdoors had the engine's default auto exposure, whose +1 EV lifted the
dark soil (the furrows' scan, luminance 0.08, rendered at 0.22) and paled the sky. `ASimSkyWeatherActor` now sets
the outdoors' compensation, `OutdoorExposureBias` = 0 EV (an unbound post process; `-SimExposureBias=<ev>` to try
others), and the cabin camera keeps the engine's +1 its fixed exposure was tuned with. The ploughland renders at 0.09,
its own colour, under a blue sky.

**T2 built (4 Nov 2026):** `data/scenery/views.json` (V1-V10, anchored to the map's ids), `simcore/scenery/views.hpp`
(the resolver, `test_views.cpp`), `-SimShotSet=views` in the map actor (each view's picture, `<view>.cost.json` and, for
a view the map cannot give, `<view>.missing.txt`), and `python tools/terrain_views.py run --label <name> [--against
<name>]`, which renders them and writes `out/views/<name>/index.html`: each view's pictures, its photographs as
thumbnails and links (author and licence beside each), and its costs. Baseline `t1` (RTX-class GPU not named; this PC's
`UnrealEditor` offscreen at 1280x720, each the mean of 130-175 frames): GPU 5.9-7.0 ms (V3 from the air least, the
battery's north most), game thread 4.9-5.4 ms, render thread 8.6-11.5 ms (V4 along the road most).
V5 is missing (no farmyard site kind: T7); V8's eye stands on the bank (no dyke: T8).

**What the baseline views show** (judged by eye against the photographs):
- V4, V6, V9 (the ploughed, stubble and cloddy ground at 1.7 m): the real textures read as a field.
- **V8's flat green was the marsh, drawn as slabs (found and fixed 4 Nov):** `BuildBoxes` drew every `MapPart::Marsh`
  box (0.25 m tall, 100 m square) over the ground, so the ground's own marsh layer never showed. They are no longer
  drawn: the marsh is the ground's cover colour and texture (the boxes stay in the layout, not opaque to sight). Found
  by hiding the static meshes (`-ExecCmds="show StaticMeshes"`) and then the test patterns (`SAM3D_GROUND_PATTERN=1`),
  which showed V1's textures were there all along.
- **V7 (the battery): the flat olive surface is the position's camouflage net, not the ground (found 4 Nov).** With
  `-SimShotHide=SimEntityActor,SimPositionWork` reading its whole list (fixed in the T3/T4 review), V7 shows the textured
  fields to the horizon. `ASimPositionWork` hangs a faded olive net on four poles 4.2 m high, sagging 0.6 m to the middle and
  0.9 m at its ragged edges; V7's eye stands 3.75 m over the pad, just above the sag and below the edges, so the net fills the
  picture below the horizon and curves up at its edges, and the mast's shadow falls on it. The ground was never at fault (the
  earlier probes: `-SimGroundDebug` showed nothing at V7 because the ground there is hidden by the net). V7 now stands 25 m
  off the position, to its side (`from_m` 25, `bearing_deg` 110, square to its heading of 200), at the same 4 m: the view is for
  the ground round the position, and the crew's own view from under the net is the cabin's. A site anchor had ignored
  `from_m` and `bearing_deg` (`views.hpp`; fixed and tested), so V11 too had stood on its site, not 3 km west of it as written. The probes `-SimGroundDebug`,
  `-SimShotHide`, `-SimShotProbe` and `-SimAtmoGround` stay in the build.
- V2 and V3: the houses are dark boxes (their materials, lit from a low sun) with no verges or poles; V3 shows the
  land cover's 25 m cells as steps at every field edge (T3).
- V1, V10: fields are small patches (T4); no tree rows on the tracks, no pylons; the trees are the grey models (T5).
- Per-pass GPU times are not in the pages yet: `ProfileGPU` writes one frame's tree to the log per view; reading it is
  left (the totals above are from the frames' own timers).

**T1 left:**
- The layers' tints by eye against the photographs; the land cover's colours far off (the fields without a texture)
  still read lighter than the textured ones.
- The `street` view of `-SimMapShots` looks at the sky only (as before T1): its camera to be fixed.
- The tile sizes, the far distances (400 m to 2 km; 6 to 12 km) and the macro noise tuned there.
- A tint per layer and season (October grass browner, stubble greyer), and wetness from the weather.
- Harrowed and sown ground as a layer for T11's faint green, once T4 gives the calendar.
- The material's cost measured (T2).

**Order:** T1, then T2 (nothing after can be judged without the views and the costs). T3 and T4 next, side by side:
together they make the plain read right from the battery (designed together: `T3_T4_PLAN.md`). T5 and T7 then, model by model with the owner's say-so;
T6 and T8 after; T9 when a measure calls for it.

## 8. The development process

### 8.1 For every asset (a texture, a model)

1. **Pick** from the research, against the reference photographs and the period (1985: no later buildings, cars or
   signs; a modern photo only for what has not changed).
2. **Check the licence and the price on the day**, on the asset's own page: allowed licence, free, the page the right
   asset. Fab listings rotate.
3. **Register** it (`data/scenery/assets.json` for a model, `data/scenery/textures.json` for a texture): origin,
   author, licence, redistributable, the scan's or model's real size, what it is for. The register checks refuse a
   broken rule (`simcore/tests/test_scenery.cpp`, `tools/tests/test_fetch_textures.py`).
4. **Ask the owner**, naming the files, their source and their size (`fetch_textures.py --sizes`). Nothing is
   downloaded otherwise.
5. **Fetch per machine**: textures by `tools/fetch_textures.py` (Poly Haven's API, ambientCG's zips; SHA-256 recorded
   by `--record`), Fab assets by the Fab plugin into `Content/ThirdParty/<group>/`.
6. **Import by script**, never by hand, so every machine makes the same assets: `tools/ue_build_ground_material.py`
   for the ground; an import script per kind for models (as `tools/ue_import_*.py` do).
7. **Check**: `python tools/check_assets.py` (nothing third-party tracked by git), the import script's own checks, and
   the game's fallback without the files.
8. **Look**: `-SimMapShots` at the views the asset serves, beside their photographs; then the owner looks.
9. **Credit** a CC-BY asset in the game's credits (`docs/scenery/ASSETS.md`).

### 8.2 For every phase

1. Research what is missing first, into `docs/research/` with every claim linked; sources ranked as `docs/SOURCES.md`
   ranks them.
2. simcore first for anything that places, classes or blocks: header-only, tests by hand calculation, the prototype
   kept in step where it mirrors the model.
3. Unreal draws simcore's data; editor assets are made by scripts in a commandlet (`UnrealEditor-Cmd.exe ...
   -run=pythonscript`), from PowerShell (Git Bash mangles `/Game` paths).
4. Build: `cmake` and the simcore tests; UnrealBuildTool for the editor module.
5. Render the shots offscreen, before and after (`UnrealEditor.exe ... /Game/Maps/KesmarkRange -game
   -RenderOffscreen -SimScenario=... -SimMapShots=<folder>`), and look at every one.
6. Measure the cost at the views (T2).
7. If anything that blocks sight moved: the balance runs (`crew_balance`, `air_war_bench`, `em_demo`, `ground_check`,
   `helper_coverage`) against the previous rows, as R6 did.
8. Review: a second reader for the licences, and an adversarial review of the code before it is committed.
9. Write what was done and what is left in this roadmap's phase line, in `docs/DEVELOPMENT_ROADMAP.md` and in the
   scenery docs.

### 8.3 Tools

| Tool | What |
|---|---|
| `tools/fetch_textures.py` | The register's textures onto this machine (`--sizes`, `--list`, `--ids`, `--record`); packs `orm.png` |
| `tools/ue_build_ground_material.py` | The ground's texture arrays, `M_SimGround` and `MI_SimGround`, in a commandlet (`SAM3D_GROUND_PATTERN=1` for the test patterns) |
| `tools/check_assets.py` | Nothing third-party in git; the register against the files |
| `-SimMapShots=<folder>` | The map's views rendered offscreen (`ASimMapActor`, `SimViewComponent`) |
| `tools/ue_ground_check.py` | The drawn ground against simcore's (R5) |
