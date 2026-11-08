# The terrain: a roadmap (track T)

**Part A (5 Nov 2026) is the reworked roadmap and comes first.** Part B (from section 1 on) is the roadmap of 3 Nov,
kept for its research, its rules and the design of what was built (T0-T4, T5.1); where the two disagree, part A wins.

**The look target (6 Nov 2026, `LOOK_TARGET.md`):** the owner's quality bar is Kingdom Come: Deliverance II's
countryside, in six screenshots kept on the owner's PC. Every render is judged against them, trait by trait (K1-K14),
as well as against the place's photographs; the terrain's variation (rolling hills, valleys, land use that follows the
ground) is required, not optional (D10-D12, phases L11 and L12, A.5).

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
| D10 | **The look target** (6 Nov 2026): every plot pass and every view is also judged against the owner's KCD2 screenshots, by the traits K1-K14 of `LOOK_TARGET.md` (relief, land use, edges, trees, ground cover, crops, tracks, props, light, sky, colour, water, distance). Structure, density, light and value are compared; hue by season. Each pass's entry ends "Against the look target" | The owner: "the gold standard we should be aiming for" |
| D11 | **No plugin and no new terrain builder.** The hills are made where the ground is made, in simcore (`NaturalGround` and the bake), because the sight lines, radar masking and movement read it (D1, D4); a Landscape, Gaea or World Machine would make a second ground (D8). Unreal draws them with what it has built in: Nanite and its displacement, RVT, PCG (Epic's plugin, already enabled), Sky Atmosphere, Volumetric Cloud, Height Fog, Lumen, virtual shadow maps. Only Megaplants need Epic's experimental Nanite Foliage and Procedural Vegetation Editor plugins (D7) | `TERRAIN_UE_TOOLS` 7; A.5 |
| D12 | **Land use follows the ground.** Woods, meadows, fields, orchards, vineyards, villages and tracks are placed from the ground's slope, aspect, wetness and curvature as well as from the map's vectors; the map's data keeps the outlines it already names (the Lazne woods, the Dubnik forest), and the generator fills the rest by those rules | K2; how real hill country is used (A.5, to be sourced) |

Paid tools stay the owner's choice and are not assumed: SpeedTree Indie (USD 19 a month, under USD 100k revenue) is
the cheapest route to good trees under 10 m; The Grove (EUR 99 and up) the most realistic cheap generator; Houdini
Indie (about USD 299 a year) the strongest scripted pipeline (`TERRAIN_ASSET_TOOLS` 8.2).

### A.3 The phases

The new phases are **L0-L12** ("land"; L11 and L12 added 6 Nov 2026 for the look target). Part B's T0-T4 and T5.1
stay built and are used by them; Part B's T5-T9 are replaced by the phases named in the last column. The look target's
traits each phase answers for: L3 K1, K8; L4 K3, K8, K12, K14; L5 K6, K7, K14; L6 K5, K12; L7 K2, K4, K13; L8 all, at
the views; L9 K9; L10 K14; L11 K1, K2, K13; L12 K10, K11, K12.

| # | Phase | Done when | Replaces |
|---|---|---|---|
| L0 | **The rework**: the research, this part, the decisions | this part written | |
| L1 | **The plot lab.** Plot worlds: `data/scenery/plots/<plot>.json`, each a 50 x 50 m core in a 400 x 400 m ring, a tiny map (a parcel, a wood outline, a track, a road, a stream, a sky clock) turned into a map by the same generator path (`terrain_map.hpp`); the same plot also found on Kesmark by an anchor, as `views.json` does. `-SimShotSet=plots`: the three cameras per plot, each plot's photographs and its last approved render on one page (`tools/plot_lab.py`). The ladder bug fixed (A.1 item 6) | plot 1 (ploughland) renders from the three cameras **in under 90 s** from one command, the page beside its photographs | T2's loop for look work |
| L2 | **Recipes.** `data/scenery/recipes/` and its simcore loader: matching by cover, crop state, stand, edge pair and signed distance (from the vectors, not the 25 m raster), wetness, soil, road class; generation per cell with position-hashed seeds, big things first and children round them; density by thresholding a fixed ordered point set (so thinning never moves a tree); blue noise, lattices, Thomas and Matérn clusters; outputs: instances, ground weights, relief stamps; the report (hectares, instances and budget per recipe, measured statistics against each recipe's targets). Today's `map_dressing.json` rules move into recipes one by one | the tests: determinism (one hash per plot), every target met on its plot, the Kesmark map's obstacle heights unchanged where no recipe changed | part of T4, T5 |
| L3 | **The ground's shape** (simcore). Meso relief by recipe and by the map's lines: swells, palaeochannels, sand ridges, kunhalom mounds, road embankments and ditches, canals, dykes, headland ridges, as noise bands and cross-section stamps merged by distance; the ground's spacing per ring chosen by measure (plots at 1-2 m; the play area's rings at 5-10 m where the stamps need it). The balance runs re-read (sight lines move) | plots 13, 15 and 17 show their embankment, ditch, dyke and mound at 1.7 m; `ground_check`, `crew_balance`, `air_war_bench`, `em_demo`, `helper_coverage` re-read and the moved rows explained | part of T8, T9 |
| L4 | **The ground's surface.** The field-state texture (D6) at 2 m in the detail areas and 8 m over the map; the ground's layers chosen per pixel by recipe rules (cover, state, edge distance, wetness), at most four blended; the colour map synthesized from the vectors (Part B's T3 finished); anti-tiling (two variants per texture, macro noise); the ground tiles as static meshes with RVT (meshes blend into the ground) and Nanite displacement for the micro relief | plots 1-6 at 1.7 m and 150 m: furrows, clods, stubble rows and headlands read; no tiling peak in the 150 m render's luminance | T1's left, T3 |
| L5 | **The ground cover** (the near ring). Grass, weeds, stubble straw, maize stover and stumps, ploughed clods, verge and pasture sward, generated round the camera to 60-100 m from the field-state texture and the recipes: first the spike (PCG with a "Sim Cover Points" node on the GPU, against the map actor's own instanced ring), then the winner. Standing maize and sunflower as field blocks (rows of cards near, a block far), their height in simcore's obstacles | plots 1-6 and 13 at 1.7 m read as their photographs; no bare texture between the clumps at 1.7 m, and past the ring the cover's tint carried by the ground with no visible ring (K6, K14); the ring's cost within budget; points on the drawn ground within 1 cm; two clients alike | T6 |
| L6 | **The asset kit** (D7). The card-clump builder for grass, crops and reeds; the tree builder (skeleton, twig cards on the end branches, crown-hull normals, vertex-colour AO, LODs by card thinning, three or four variants per species); impostors baked in Unreal; registering an Unreal asset by its path; the Megaplants spike (1000 oaks as instanced skinned meshes, a Shipping build). One species judged at a time at its plots, starting with black locust and hybrid poplar | each kit item at its plot's three cameras beside the photographs; the owner's look | T5.2-T5.3 |
| L7 | **Woods, edges and lines** (simcore, sight-blocking). Stands as layers (canopy, sub-canopy, shrubs) by recipe: oak-hornbeam, black locust, poplar plantation, floodplain willow; the forest edge's herb fringe, shrub belt and mantle with noise along it and by aspect; tree rows on tracks and roads; hedges; reed belts on canals and oxbows. The wood masses far off take the same colour and outline | plots 7-12 and 14 at the three cameras; the obstacle heights test; the balance runs re-read | T5, part of T8 |
| L8 | **The whole map.** Every recipe on Kesmark (and the detail areas), the report's hectares and budgets, the views V1-V12 and each plot's map anchor | V1, V4, V6, V7, V9, V10, V12 against their photographs; the budgets held on this PC's RTX 3060 Ti | |
| L9 | **Villages and farms.** Plot 16 and its transitions; our own building generator (the Kádár cube first, then the long house, barns, the TSZ's sheds, water towers); fences, poles and wires; gardens and fruit trees by recipe; Part B's T7 for the rest | V2, V3, V5, V6 against their photographs | T7 |
| L10 | **Far field and cost.** Impostor rings and the masses meeting without a seam; baked Nanite ground tiles if they measure better; budgets per pass at every view | each view within budget | T9 |
| L11 | **Hill country** (simcore, A.5). A landform layer (regions of plain, rolling and hilly ground as data, shaped by warped and ridged noise); drainage (pits filled, flow accumulated, brooks and their valleys cut); slope, aspect, wetness and curvature baked; land use placed by them (D12); roads and tracks fitted to grades, cut and fill, sunken lanes in loess; a hill-country detail area and its plots (18-22). The balance runs re-read (sight lines and radar masking move) | the hill plots and a hilltop view against K-a, K-b and K-d at the three cameras: slopes, valleys, wooded crests, fields on the gentle ground; `ground_check`, `nav_check`, `crew_balance`, `air_war_bench`, `air_war_balance`, `em_demo`, `helper_coverage`, `mission_demo` re-read and the moved rows explained; `test_country_map` agreeing | (new) |
| L12 | **Light and air.** The scenario's clouds shown (today a "cloudy" preset renders clear or as a sheet); volumetric cumulus with shadows on the land; aerial perspective tuned so far ridges go bluer and paler, no brown haze; a fixed exposure for the plot lab, so passes compare; foliage lit as foliage (two-sided, subsurface, occlusion in crowns and grass); albedos and vertex colours calibrated, a neutral grade; a summer date for the plot lab beside October's | the eye and 150 m renders of plots 5, 11 and 19 beside K-b and K-d: cumulus and their shadows, layered blue distance, shadows filled with sky light; a measured albedo range per material (K10-K12) | (new) |

**Order:** L1 and L2 first, together (the lab is useless without recipes, recipes untestable without the lab). Then
**one vertical slice: plot 1, ploughland, through L3, L4 and L5**, to prove the whole chain on the October plain's
largest surface. Then plots 2-3 (maize), then the forest edge and the tree row (11, 12) through L6 and L7, then black
locust and poplar, the road (13), and the rest in `TERRAIN_PROCEDURAL_METHOD` 6.6's order. L8 after each group of plots
(a cheap check, since the recipes already apply map-wide). L9 and L10 last.

**Order from 6 Nov 2026 (the look target):** the two largest gaps (K1 relief, K2 land use) are under everything else:
a wood's edge, a field's margin or a track looks different on a slope, and every plot anchor and view moves when the
ground does. So, next:
1. **L12's quick part**: the clouds shown, cloud shadows, the brown haze, a fixed exposure for the plot lab, and the
   summer date. A few days at most, and every later render is judged in honest light.
2. **L11's first three steps** (A.5: the landform, drainage and valleys, the ground's measures), with the plots and
   views re-anchored and the balance runs re-read. Then the land use (D12), which also replaces the 100 m wood blocks
   and the field grid.
3. Then the plots as planned (black locust and poplar, the road, the rest), each on its new ground, and L11's roads,
   sunken lanes and hill plots alongside L3.

L12's foliage shading and colour calibration go with L6 (they are the asset kit's look). L8 still follows each group.

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
  neither from 150 m, and the furrows still read from 25 m and the eye.
- **a7-a9, a low sun** (`plot_lab.py run --extra "-SimTime=hour -SimHour=16"`, the sun some 10 degrees up in mid
  October): the furrows read strongly, as in the photographs of a late afternoon; the meso swells (1-2 degree slopes)
  do not show from 150 m even so, which is the plain's flatness, not a fault: they are there for the sight lines. The
  clods glowed orange (`kClodTop` 0.150, 0.098, 0.050); darkened by half they were grey stones at noon; at 0.110,
  0.076, 0.044 they are damp brown at noon, and their sunlit sides at 16:00 are bright, as a face turned to a low sun
  is (the clutter material's roughness is 0.95: no sheen). Next: the recipe file (L2), then the next plots.

### A.3.3 L2 built in first form (5 Nov 2026)

- **`data/scenery/recipes/`**, one JSON per land type (11: ploughland, sown, wheat and maize stubble, dry stalks,
  meadow, verge, trampled, bank, forest floor, marsh), read by `simcore/scenery/recipes.hpp` (`LoadRecipes`). A recipe
  `applies` to ground layers (a field's crop state is its layer already), has a `priority` (a layer takes the highest;
  two at one priority refused), `bands` (a point takes the first it lies in: `edge` any, ends or sides with `from_m` and
  `to_m` in from the parcel's edge, from the field function's exact rectangle; the last band has no distance, the rest
  of the land), each band's `scatter` (a clutter kind, a process, `per_ha`), `targets`, `source` and `confidence`.
- **Processes** (`ScatterInCell`): uniform; blue noise (a jittered lattice, none closer than a fifth of its spacing:
  measured 0.19 m at 18,000 a hectare, variance against mean 0.20); Thomas and Matérn clusters (measured 3.0). They run
  over the plane, not the cell, so cells meet without a seam; every candidate has a fixed threshold, a lattice's from a
  4 x 4 dither, so thinning (the ring's falloff) never moves a point and a thinned lattice stays even.
- **The clutter ring reads the recipes**: `clutter.json` keeps the ring and the kinds, its per-layer list is gone (and
  refused if it comes back); `ASimMapActor` loads both (`LoadClutterRecipes`). The ring at the battery: 6,374 plants
  in 12 ms. Each point is checked against the layer at its own point, so the points are the same in 4 m or 8 m cells
  (a sliver of another layer under a cell's samples is left out: 1 in 3,209).
- **The report** (`RecipeMeasure`): each band's hectares and each kind's instances a hectare against the targets. On
  the map's own ground near battery 21 every measured band is on target (ploughland's clods 18,028 a hectare, meadow's
  grass 9,018, and so on); bank, marsh, trampled and verge have no 120 m of their own ground there to measure on.
- **New in the look**: a weedy strip 0.6 m (ploughed, sown), 0.8 m (maize) or 1 m (wheat stubble) wide at every parcel
  edge, and more clods on a ploughed parcel's headlands (18 m at its ends); grass, reeds and weeds in clumps.
- **Left**: ground layers, relief stamps and the map dressing's trees are not recipes yet (D3 asks for them: the
  ground's surface values stay in `ground_layers.json`, the trees in `map_dressing.json`, to be moved one by one); the
  parcel's distance ignores an estate's border or a road cutting it; edge recipes by a pair of neighbours (6.4); the
  material does not yet turn the headland's furrows across.

### A.3.4 Plot 2, maize stover (5 Nov 2026)

- **b1** (L2's recipe, blue noise stumps 2,500 a hectare): sparse dark tufts anywhere, no rows, nothing lying; the
  ground's straw scan tiles at 25 m.
- **A `rows` process** (`ScatterInCell`): the crop's rows along its parcel's furrows (`FieldAt`'s heading), `row_m`
  apart, the plants along them at `per_ha`'s spacing (jittered a third of it along, 4 cm across, as a drill's row
  wanders), one lattice over the whole estate in the furrows' frame. A rows point lies on a field of the cell's heading
  or nowhere: off a field there are no rows. Every candidate carries its parcel's heading.
- **Lying stalks**: the kind `stover` (`clutter_stalk` in `ClutterMesh`, a stalk broken once, its instance width its
  length 0.6-1.4 m and its height its thickness 2-3.5 cm), laid by the kind's new `along_rows_deg` (35: within 35
  degrees of the rows, either end first). Unreal turns a part's local x (north) clockwise by the yaw, so the heading
  lays it along the rows.
- **b2** (a clump a metre along rows 75 cm apart, stover 8,000): the stalks lie right, but the near-square lattice
  reads as a scatter, not rows, and the stalks were near black and plank-thick.
- **b3** (a single cut stalk, 0.10-0.18 m across, every half metre: 26,667 a hectare; stover 5,000, paler and thinner):
  the rows read out to some 40 m; from 40 m up the stubble is gone into the ground, as it should be. The ring on a
  maize field is 21,043 plants (the stumps 26,714 a hectare, the stover 5,008, both on target), the map's thickest,
  held under 24,000 by `test_recipes.cpp`.
- **Left**: the ground's straw scan still tiles from above (the material's, not the recipe's); real stubble has a plant
  every 18-20 cm, drawn every 50 cm for the ring's budget; the maize ridges (the material's wobble cannot follow
  simcore's rows yet).

### A.3.5 Plot 3, standing maize, in simcore first (5 Nov 2026)

- **Heights in the data** (D4, L5): a crop state that stands carries `stand_m` (maize 2.4, sunflower 1.8, wheat 0.9,
  estimates) and `grow_days`, the days from its start to full height (`ground_layers.json`); `CropLayers::StandOnDay`
  gives a field's stand on a day.
- **`StandingCrops`** (`simcore/scenery/standing_crops.hpp`): one bake per map and day of what stands, a byte per 25 m
  cover cell (uniform, or mixed where a field's edge crosses it and the field is asked exactly), with 500 m blocks of
  bounds. It is a `MapStandingObstacles` overlay on `MapTerrain` (`SetStanding`): every obstacle query, the chords and the
  bounds take the max of R6's footprints and the crops, so the sight line, the radar's line of sight and the battery's
  sight see standing maize as a wall at its height. Exact at the field's edge (`test_standing_crops.cpp`: 421,201 points,
  0 wrong; 120 paths' chords, 0 missed; a man at 1.7 m blocked by 400 m of maize, clear from 5 m). A scenario loads it for
  its day (`ScenarioDef::crops`, cached), the runner sets it on its ground; the air war's raid generator does not.
- **Unreal** draws the same bake (`ASimMapActor::BuildCrops`, `StandingCrops::BlockMesh`): per 1 km tile a prism per field
  part at its stand, walls only where the neighbour stands lower, tan with a per-field shade and a mottle. Day 288: 385
  tiles, 387,636 triangles, 1.4 s.
- **The plot's camera** (`plots.json` anchor `"edge": true`): finds the field's edge on a side that opens on a field not in
  the layer and stands outside it looking at the face.
- **c1**: the camera stood under a road's tree row; the face a dark grey wall. **c2** (the open-side search, lighter, a
  mottle): the face stands right, 2.4 m over open stubble, with a clean edge from 25 m and 150 m; it reads as a tan wall,
  not as stalks.
- **Left**: rows of maize cards near the camera (within some 60 m, an atlas card per plant; registered as procedural);
  the camera inside a block sees out through its back faces; the dry_stalks recipe's reed clutter is still scattered
  under the block; the day is fixed at the session's start (a field harvested in a long session stays standing); the bake
  costs about 2 s a map and day.
- **The balance runs re-read**: simcore's 1061 tests pass. `helper_coverage` keeps its 25 gaps; only the dynamic
  campaign testbed on the Kesmark map moved, by a few per cent (the commander's first launch 814 s to 785 s, the senior
  operator's wheel moves 1616 to 1283). `ground_check` 0 of 15 scenarios with anything under the ground.
  `crew_balance` (no map) unchanged. `em_demo` and `air_war_bench` build no `MapTerrain` with crops, so they are
  untouched by construction.

### A.3.6 Plot 3, standing maize near the camera (5 Nov 2026)

- **The card** (`tools/make_cards.py` `maize`, `art/clutter/maize_card`, 512 x 2048 for a card about four times as tall as
  wide): two or three dry plants, a buff stalk, a dozen leaves arching out and hanging (one in three limp down the stalk,
  greyer and darker low down), an ear in its husk, a tassel. Our own work, registered (`assets.json`, licence Own) as the
  part `clutter_maize`, whose procedural placeholder is a thin spindle.
- **Near kinds** (`clutter.hpp`): a kind with `near_m` is drawn in full within it of the ring's centre and not beyond; with
  `to_stand` its height is a share of its field's stand on the day (`RecipeCandidate::stand_m`). The `dry_stalks` recipe
  plants the maize kind in rows 75 cm apart, a card every half metre (60 m, 0.90-1.06 of the stand).
- **The block dithers away near the camera** (`M_SimMapCrop`, `tools/ue_build_crop_material.py`; `BuildCrops` sets it from
  `clutter.json`'s `crop_fade_m`, 28-42 m, when the ring is drawn). The ring lags the camera by up to `rebuild_m`, so the
  loader refuses a fade whose far end plus that lag is beyond a near kind's reach (the block would stand bare).
- **Cost**: a ring on a dry maize field holds 30,147 cards (32,000 in all, 18 ms); 26,682 a hectare within 25 m
  (`test_clutter.cpp`).
- **c3**: from the eye camera the face is rows of dry stalks and hanging leaves at its 2.4 m; from 25 m the block dissolves
  within some 40 m into the cards (the dither is grainy in a single frame); from 150 m the block, as before.
- **Left**: the seam where the cards end and the block takes over along a face is visible (the block's flat tan against
  the cards); the block's top from above is a flat plane (rows in its colour, a darker shade at its edges); the cards are
  regular (leaves in near-mirror pairs, one card drawn: a second variant); sunflower fields in dry_stalks wear the maize
  card; the imported materials miss Unreal's Nanite and instanced usage flags (as the earlier cards and trees: a packaged
  build would draw them in the default material).

### A.3.7 Plot 3, the block drawn as plants (5 Nov 2026)

- **How games do it**: near plants as models or cards, far ones as billboards or baked impostors, dithered between
  (Epic's Impostor Baker; LOD write-ups); nothing published on maize itself. The block is our far form, so it should look
  like the cards it hands over to: the seam was the block's flat tan, not the hand-over.
- **The block's UVs** (`StandingCrops::BlockMesh` with `uvs`): on a wall, metres along its line and the height as a share
  of the stand; on a top, metres along the field's rows and across them; wrapped by whole periods of the patterns (230 m,
  225 m) for a GPU's floats. In that form the walls rise to 1.06 of the stand (`kFringe`) and the top lies at 0.88
  (`kTopShare`), so the walls' top can be cut ragged over the top; the sight lines' obstacle stays the stand.
- **`M_SimMapCrop` draws the plants** (two HLSL nodes): on a wall a plant every 23 cm with its own tone, a stalk, a dark gap
  where the rows go in, darker at the foot, its top cut at its own height (0.86-1.06); on the top the rows faint and two
  octaves of value noise (plants leaning together, gaps); the fine detail fades where a pixel spans it (`fwidth`), so afar
  it averages instead of shimmering. Without the ring the block is still drawn this way, whole.
- **c4** (sines on the top): a hard corduroy from 25 m and a herringbone from 150 m. **c6** (value noise, the rows faint):
  from 150 m a lumpy canopy; from 25 m faint rows and patches; at the eye camera the face beyond the cards is stalks with
  a ragged top in their tone, the hand-over soft.
- **Left**: a second card (the leaves in near-mirror pairs read regular in the first rows); the canopy is greyer than the
  near cards from above; sunflower wears the maize; the cards' and trees' Nanite and instanced usage flags.

### A.3.8 Plot 3, a second card and the block's tone (5 Nov 2026)

- **A kind's part may be a list** (`clutter.json`, 1-8 parts): each plant draws one by its own hash, so the same plant
  wears the same card wherever the ring is. Maize has two: `maize_card` and `maize_card_b` (`tools/make_cards.py`,
  seeds 23 and 41; the second limper, more leaves to one side, near half its plants snapped at 0.68-0.80 of their height
  with no tassel). In the ring 15001 of 30147 plants wear the second card.
- **Mirroring tried and dropped** (c7): an instance scaled by −1 across turns its card's normals from the sun and half the
  plants drew black. Variety comes from cards, not mirrored instances.
- **The block's tone** warmed to the cards' (top 0.30/0.225/0.11, walls 0.27/0.20/0.10).
- **c8**: no plant drawn black; at the eye camera the two cards mix in the first rows and the face beyond is the same tan;
  from 25 m the cards fade into the block's rows without a band of colour.
- **Left**: sunflower wears the maize; the cards' and trees' Nanite and instanced usage flags; the field's top reads flat
  from the eye height at the far edge (a few metres of tassels standing above it would break the line).

### A.3.9 Plot 3, six maize cards of uneven height and shade (6 Nov 2026)

- **The owner, 6 Nov 2026**: four more maize plants, of more drastic varying heights and slightly brighter or darker, for
  less uniformity.
- **Four cards more** (`tools/make_cards.py` `maize(heights=, tone=)`, seeds 57, 73, 89, 97): `maize_card_c` stunted (plants
  0.48-0.66 of the card, darker 0.84), `_d` the tallest (0.95-1.0, paler 1.12), `_e` uneven (each plant 0.45-1.0, 0.92),
  `_f` middling (0.66-0.84, paler 1.07, limp, a top snapped now and then). A stunted plant is smaller all over (its leaves,
  stalk, ear and tassel by its height); `tint` lifts a card towards a pale buff or sinks it towards a damp brown. The first
  two cards are drawn byte for byte as before.
- **The kind draws six** (`clutter.json`); in the test's ring each card takes a sixth (5077, 5047, 5022, 5049, 5030, 4922 of
  30147). The placeholder `clutter_maize*` mesh serves all six.
- **c9**: the front rows ragged, short and tall plants and darker and paler ones side by side, where c8 was an even face.
- **Left**: the block behind still stands at one height (the stand), so its top line beyond the cards stays even; a
  height that varies across the field (in simcore's obstacle too) would carry the cards' unevenness out to the far edge.

### A.3.10 Plot 3, a stand that varies over the field (6 Nov 2026)

- **One height function** (`GroundLayerRules::StandAtPoint`): the calendar's stand times `StandVary`, a field's own share
  (±0.05, by its number) and patches about 40 m across (±0.20, two octaves of value noise), from `ground_layers.json`'s
  `"stand_vary"` (the loader holds each share to 0..0.3 and both to 0.4). The plain's maize from about 1.8 m in a poor
  patch to 3 m in a good one.
- **Everything reads it**: what blocks sight (`StandingCrops::StandAt`, and `Chords`, a chord's top the highest of the
  field's stand at its ends and middle); the cells' bytes the bound (the field's stand times `MostVary`, so the tallest
  stand baked is now 3.0 m); the block's corners (its top and walls follow the height at each corner; a wall only where the
  crop beyond stands 2 cm or more lower, so none inside a field); the near cards' heights (`RecipeCandidate::stand_m`).
- **Tests**: along 400 m of a 2.4 m field, 2.21 to 2.84 m, at most 2.3 cm from one metre to the next; fields apart by
  their own shares; the chords, the bake and the drawn block still agree with the truth everywhere they did.
- **c10** (c10's first try lost the GPU on its fourth frame as Windows swapped audio devices; the second ran): the
  wall's top edge moves 1-2 px against c9. From the eye camera the wall is some 100 m off and its top a little above the
  eye, so ±0.3 m is a sixth of a degree: right for a real field at that range, but it does not break the line there.
- **Left**: whether to make the patches stronger or smaller (by eye: `"patch"`, `"patch_m"`); the line at the far edge
  is a fringe of tassels more than a slope of the canopy, which the material's ragged top draws on the wall but not
  along the top's skyline.

### A.3.11 Plots 3-5 in one batch: each crop its own plants, the sunflower, the wheat's swaths (6 Nov 2026)

- **The owner, 6 Nov 2026**: carry on with the roadmap, but make far more changes before each render. One batch, two
  renders (c11, then c12 after what c11 showed).
- **The materials' usage flags** (`tools/ue_fix_map_material_usage.py`): `M_SimFoliage`, `M_SimMapVertexColor` and
  `M_SimMapClutter` set for Nanite and instanced meshes, and the 68 instances under them resaved, so the editor no longer
  compiles them on the fly.
- **A scatter kept to a crop** (`RecipeScatter::crops`, ids from the dressing's crops): `dry_stalks` is maize's and
  sunflower's both, so it has a maize scatter and a sunflower scatter, each standing only on its own crop's fields
  (round the test points 11315 maize plants and 4263 sunflowers on their own fields, none on another's). Each target is
  measured over its own crop's area. **The old plot 3 stood on a sunflower field**, drawn as maize: its anchor now names
  the crop (`"crop": "maize"`), and the nearest dry maize with a whole square is about 4.6 km on, at (10.8, -11.5) km.
- **Plot 4, standing sunflower dried before the harvest** (`p04_standing_sunflower`): three cards
  (`make_cards.py` `sunflower`, a stalk to the neck, shrivelled leaves, the neck bent over 110-160 degrees, the head
  hanging face down; `_b` uneven, `_c` three plants), the kind `sunflower` in `clutter.json` like maize's (to the stand,
  in full within 60 m), and the far block darker by the crop's `block_tone` in `ground_layers.json`. No photo of it in
  the register yet: the plots test lets this one plot go without until the owner gives one.
- **c11**: the sunflowers stood but drawn tan, like dry maize; near-black was wanted. **c12**: the cards' colours some
  0.6 of what they were (the stalk #2a231c to #4c4032, the head's back #504230), the far block's tone as before. The
  stand's face in the eye camera is a fifth darker (mean RGB 101, 78, 45 to 81, 63, 37; the ground showing between the
  stalks holds it up): darker than c11, still a dark brown more than the near-black of the photos' fields.
- **Plot 3 on its maize field** (c12): the dry maize stands as a dark wall of stalks, its tassels ragged along the top,
  the six cards' heights and shades side by side.
- **Wheat stubble** (`wheat_stubble.json`): the stubble in the drill's rows (`"across_m"`: a row's sideways scatter in
  metres, held to 0.4 of the row spacing) and the combine's swaths in lines a header's width apart (6 m, an estimate),
  as overlapping pieces of a low mound (`clutter_swath`). In c11 they read as dark logs 1 m wide with gaps between; in
  c12 they are 0.36 of their length wide, 3-7 cm high, bleached straw colour, 2395 pieces a hectare (target 2000-2900),
  and read as pale lines 6 m apart, still beaded where the pieces meet.
- **Left**: a sunflower photo for plot 4 (the owner's say-so); the sunflower stand darker still, its block's wall
  too; a swath drawn as one strip along its line rather than pieces; by mid-October a real swath is mostly baled or ploughed
  in, so a stubble field's date should choose between swath, bales and bare stubble; the cards' leaves arch up and over
  where a dead sunflower's hang straight down.

### A.3.12 Quixel's Megascans on the ground (6 Nov 2026)

- **Quixel made the whole Megascans library free on Fab** (announced 5 Nov 2026; listings updating to 12 Nov). 55 items
  claimed on the owner's account (Professional tier; two rubble packs skipped, free only on the Personal tier): 24
  Megaplants, 17 texture sets, 13 scanned models and Unreal's Norway maple.
- **The texture sets** (`tools/fetch_textures.py`, site `quixel`): the owner's 2K zips in `art/thirdparty/quixel/zips/`,
  unpacked as albedo, normal, roughness, AO and ORM like Poly Haven's and ambientCG's, and registered in
  `textures.json` (Fab-Standard; `scan_m` from each set's own metadata). Their normals are OpenGL (green up the image:
  0.45 and 0.93 correlated with the bump map's slope down it, on two sets) and are flipped to DirectX.
- **Not ground**: arid grass and both wheat sets are atlases of plant cards; the concrete, asphalt and dirt roads wait
  for layers of their own (the roads are drawn as meshes today).
- **Three layers changed**: `marsh` from the forest's mud to `q_soil_mud` (1 m); `yard` (the towns' ground) from
  packed dirt to `q_soil_and_gravel` (0.8 m: a repeat must divide the 600 m period, the scan is 0.85 m); `trampled` (the
  military ground) from a reddish mud to `q_grassy_soil` (2 m).
- **c13-c14**: two plots added on those layers, `p15_town_yard` (in Vrbice) and `p16_battery_position` (battery 21).
  The tyre tracks tried first on `trampled` (c13) repeat every 2 m over the whole position and read as paving: tracks
  belong along routes, not as a tiling. The grassy soil (c14) reads as worn earth with grass left between. The village's
  soil and gravel shows its grit within a few metres, and from 25 m up is a pale sand, yellower than a village street.
  p01-p05 unchanged (their layers' textures were not touched; 0.3-2 levels of 255 from c12).
- **Left**: the yard's colour; tracks as decals along the routes vehicles use (the tyre-track decal is here); the 13
  models and the Megaplants as picks for their parts (the Megaplants come only through the editor's Fab window, the
  owner's to add); grass tufts drawn on the village's road (seen in c14, older than this change).

### A.3.13 The forest edge, first pass: the ladder bug, bays, a stepped mantle, the herb fringe (6 Nov 2026)

- **d0, the baseline**: a straight wall of lollipop crowns along the block's side; inside the oaks the red oak model's
  leaves and inside the hornbeams the card tree's white shards; green shrub cones every few metres in one line; nothing
  between the ploughland and the trees.
- **The ladder bug (A.1 item 6) found and settled.** An instanced component's start distance only fades its instances
  (it needs the material's per-instance fade); it never hides the nearer ones, so every rung of the ladder (the model,
  its two thinned copies and the procedural crown) was drawn from 0 m. The white shards were the card hornbeam drawn in
  the procedural trees' season-tint material (one material slot, so it was taken for a procedural mesh), which also
  lacked the Nanite flag and fell to the default material. Now a kind with a registered model draws that model alone,
  in its own materials, to the kind's distance, its thinned copies made its LODs by `tools/ue_tree_lods.py` (oak 12 267,
  3 948 and 1 234 vertices; hornbeam 829 and 340). The script runs in the full editor (`-ExecutePythonScript`; it quits
  when done): a commandlet has no static mesh editor subsystem and the older library's calls all return -1 there.
  `M_SimMapTree` has its Nanite and instancing flags (`ue_fix_map_material_usage.py`).
- **The edge's shape** (`map_dressing.hpp` `PlaceTrees`, rules in `map_dressing.json`): in a wood planted whole, the
  trees' line stands back from the block's side by 0-7 m in waves about 35 m long (`edge_setback_m`, `edge_wave_m`), so
  bays and tongues; across the 12 m mantle the trees rise from half the stand's height at the line to the stand's
  (`edge_height` 0.5) with crowns 1.3 times as wide for their height (`edge_crown`); the shrub belt is two places every 3
  m at depths from the inset to a wandering 1-7 m (`shrub_belt_m`), kept in runs and gaps by the same kind of wave
  (`shrub_keep` 0.35: 93 000 shrubs of 294 000 plants on the map; at 0.55 the shrubs were 42% of them, over the test's
  third). A far wood's two rows keep no bays (a bay would
  bare the mass behind). Test: trees within 4.8 m of the side 8.2 m high against 15.1 m past the band, crowns 0.65 of
  their height against 0.50, the first tree 1.2-13.2 m in along twenty sides.
- **The herb fringe**: a recipe band can be measured from a wood's open side (`"edge": "wood"`, `WoodEdgeDistance`: the
  land cover's nearest cell that is not forest); `forest_floor.json` has a fringe 0-3.5 m (grass in clumps, 45 000 a
  hectare, and dry stalks) and a mantle floor 3.5-12 m (grass in patches), before the body.
- **d1-d2**: the oaks draw as the oak model alone, no lollipop or shard inside; the hornbeam's cards in their own colour;
  the fringe a pale band of tufts at the wood's foot; the line of trees varies, the mantle's front trees lower. Still not
  the photographs.
- **Left** (most of it art, L6): the oak model is a northern red oak and its October red dominates the edge (the plain's
  oaks are pedunculate and Turkey oak, brown-green in mid-October): a Megaplant oak or the tree builder's; maple, black
  locust and poplar are still lollipops (the black locust scan is unpacked, the Megaplants await the editor's Fab
  window); the shrubs are still procedural cones; the grass card is too short for a fringe of October herbs (a tall herb
  card, 0.5-1.2 m); the stalks do not show at 30 m; the fringe's targets are not measured by the recipe report (its test
  area has no wood edge).
- **Against the look target** (judged after the fact, `LOOK_TARGET.md`): K4 moved (bays, a lower mantle in front) but
  is still far from a closed wall of leaves: trunks show through, the shrubs are cones. K5 the largest here (lollipops,
  the red oak). K6 the fringe reads as a pale band, not grass. K11 a cloudless sky where the scenario has cloud. K1 the
  wood stands on dead flat ground.

### A.3.14 The look target, and the hills (6 Nov 2026)

The owner sent six screenshots of KCD2's countryside as the quality bar (`LOOK_TARGET.md`: what each teaches, the
traits K1-K14, today's gap, and how the pictures may be used: on the owner's PC only, never committed, traced or
recreated). The renders c12, c14 and d2 were compared trait by trait; the gap is largest in relief (K1), land use
following the ground (K2), the trees (K5), ground cover (K6) and tracks (K8). The owner asked whether this needs a
plugin or another terrain builder: it does not (D11). Added: D10-D12, phases L11 (hill country) and L12 (light and air),
the new order, the loop's comparison step (A.4) and how the hills are made (A.5).

### A.3.15 L11 step 1, the landform; L12's first part, the plot lab's own sky (6 Nov 2026)

- **The owner chose the extent: west and north-west.** `terrain.landform` (`terrain_map.hpp` `MapLandformDef`,
  `LandformShape`, `LandformRelief`, added in `NaturalGround`; the same block in `kesmark_range.json` and `pannonia.json`):
  rolling ground (22 m) ringing hilly ground (70 m), each rising from the plain over a wandering 4-5 km blend; one field
  of forms (fBm on coordinates warped 0.8 km, a ridged share of 0.35, turned 27° off the noise's lattice, forms 1 km
  across), contrast-stretched so floors and crests each take about a tenth of the ground. Tuned with a Python copy of
  the noise against A.5's numbers. Measured (`test_landform.cpp`): 70 m from floor to crest in 2 km windows (p10 61 m),
  slopes median 4.1°, p95 13.8°, 0.7% over 20°; battery 21's detail area, the bridges, Kesmark, Vrbice, the P-40 on
  Polany hill, the Kecel airfield, Varos hill and the capital untouched to the last bit; the ground in the map now
  97-202 m. An `exaggeration` dial in the data.
- **Re-read:** the simcore suite (1071 tests) passes; `ground_check` (the testbed's hour and every map mission): nothing
  under the ground, the low raid from the west included; `nav_check`: 200 of 200 routes; `air_war_balance` over 20
  seeds identical with the hills and without (the same binary on a copy of the data with `exaggeration` 0: the air war's
  battery is on the plain). `crew_balance`, `em_demo`, `helper_coverage`, `mission_demo` and `air_war_bench` not re-run
  (their scenarios are on the plain or abstract; to run before the hills grow).
- **New anchors and cameras:** the anchor kind `high_ground` (`views.hpp`: the highest ground within 1.5 km of a point,
  looking along a bearing); the view V13 (a panorama from it) and the plots p18 (a field's edge on a slope) and p19 (the
  Lazne woods' edge on a hillside).
- **The plot lab's sky (L12):** the clouds did work; d2 had looked into a clear patch of the "cloudy" preset's grey
  stratocumulus, which is simply not the look target's weather. The plot lab now renders its own scenarios
  (`data/scenarios/lab/`, out of the missions menu): fair cumulus on 15 October at 13:00 (sun 31° up), or on 5 July at
  15:30 (39°) with `--season summer`. The bright brown veil of the 150 m camera is the sun's glare on the ground and haze
  ahead (the cameras face the sun), not a fog too thick: by the numbers the haze is light. Left: a fixed exposure.
- **e1 (plots) and l11a (views):** the ground rolls at last: p18 climbs a slope with the tree rows along the crests,
  p19's ploughland falls away from the wood. V13 from the crest reads as a sea of rolling ploughland: every slope is a
  field, no wood holds a slope, the far hills are pale bands.
- **Against the look target:** K1 moved most (relief at the large scale; none yet near the eye). K2 is now the largest
  gap: land use does not know the ground (the woods are still 100 m blocks, the fields cover the hills). K4 and K5
  unchanged; the wood mass on a hillside is an opaque dark slab draped over the slope (p19). K8 worse on slopes: the road
  strips show kinks where they cross the forms (p18 high). K11: with 3 oktas of fair cumulus the volumetric material
  shows little cloud and looking toward the sun the sky goes white; a cumulus material of our own is needed. Next: A.5
  steps 2-4 (drainage and valleys, the ground's measures, land use on them), then the road strips on slopes.

### A.3.16 L11 step 2, the hills drain (6 Nov 2026)

- **Why the step 1 forms could not stay.** Filled to their sills (a priority flood), the noise's forms were 43% closed
  basins, 29% of the hills more than 10 m deep: filling would have made lakes of the hills, cutting their outlets
  gorges. Noise alone cannot make ground that drains, so the design of A.5 step 2 changed from "carve valleys into the
  forms" to **bake the ground from its drainage**, first prototyped in Python (the scratch script, not kept).
- **How** (`terrain_map.hpp` `LandformBakeFor`, `landform_detail::Bake`; data `terrain.landform.drainage`, the same in
  both map files): on a 50 m grid over the regions (1221 x 921 cells), a priority flood from the hills' outline inwards
  with the forms (a share of them at 6 km, so trunk valleys run far, and a tilt rising inland) as the priority gives
  every cell one cell downstream; the area drained is summed down that tree; where it passes 0.15 km² a valley runs,
  its bed falling to the outline at 0.02 x (0.15 km² / area)^0.5 scaled by the region's relief; every cell takes its
  nearest valley's bed (jump flooding) and rises from it at 0.26, rounding off towards a crest at 0.45-1.05 of the
  region's relief (slow noise); big valleys get a flat floor up to 60 m each side. Where two valleys' sides meet the
  higher side is brought down to 27° at most (a chamfer pass each way, which only lowers a cell to just above a
  neighbour), then any hollow left is filled to its sill. The bake is built once per process and shared by every map
  carrying the same landform, so the range and the country hold the same ground to the last bit. `NaturalGround` adds
  its height (bilinear); the brooks (valleys draining 3 km² or more, 236 of them, lines smoothed off the cells) are laid
  in the core and the detail areas as water strips 1.5-4 m wide (`MapData::LayBrooks`), which keep the dressing's crops
  and trees and the air war's open-ground placements off them as any water does.
- **Measured** (`test_landform.cpp`): 2 km windows 73 m from floor to crest (p10 60, p90 86); slopes median 6.0°, p95
  11.4°, 0.2% over 20°; no closed hollow at all (a flood raises no cell); no step between two cells steeper than 27°;
  19225 of 19338 brook points lie below the ground 150 m to either side. The hills now climb up to 117 m over the plain
  inside the range (the valley floors rise inland), so the test's bound went from 70 to 130 m. Battery 21's detail
  area, the bridges, Kesmark, Vrbice, the P-40, Kecel, Varos hill and the capital still untouched to the last bit.
  `test_country_map`'s seam at the range's edge is now taken 10 cm beyond it, not a metre (the hills' sides rise up to
  half a metre in a metre): 0.05 m.
- **Re-read:** 1072 tests pass; `ground_check` PASS (0 of 15 scenarios with anything under the ground); `nav_check` 200
  of 200 routes. Not re-run: `air_war_balance` (identical with the step 1 hills; its battery is on the plain),
  `crew_balance`, `em_demo`, `helper_coverage`, `mission_demo`, `air_war_bench`.
- **e2 (p18, p19) and l11b (views):** V13 from the crest now looks down a ridge between valleys, spurs and ridges layered
  behind; p18 from above shows long smooth slopes; p19's ploughland climbs to the wood. Every slope is still ploughland
  or stubble and the woods are where the old outlines put them, so the hills read as bare.
- **Against the look target:** K1 moved again (forms that drain: valleys, spurs and ridges, not blobs); K13 began (brooks,
  not yet judged near the eye). K2 is the largest gap and the next step: A.5 steps 3-4, the ground's measures (slope,
  wetness from the area now baked, curvature) and land use on them (woods on the steep sides and crests, meadows on the
  valley floors along the brooks). Then the road strips on slopes (K8: p18's road still kinks).
- **Left:** brooks end where they leave the hills (none crosses the plain to the Rava yet); no bed cut under a brook (the
  bake's cells are 50 m), no culvert or bridge where a road crosses one; the bake costs about a second at load.

### A.3.17 L11 steps 3-4 in first form: the ground's measures, land use on them (7 Nov 2026)

- **The measures** (`LandformBake`: `SlopeDeg`, `Wetness`, `Rise`): the slope across a cell each way; the wetness index
  ln(a / tan b) (Beven and Kirkby 1979's form; the paper not read) from the area the bake now drains; how far a point
  stands above the ring 250 m round it (a crest or spur positive, a hollow negative). In the hills inside the map the
  wetness runs p10 6.0, p50 7.3, p90 9.6, p99 17.8.
- **The rules** (`data/scenery/land_use.json`, read by `LoadMap`; `MapData::LandUseWood`, `LandUseMeadow`; estimates):
  woods where the slope passes 11° (the threshold wandering 3° either way on slow noise, so an edge winds) and on half
  the crests 6 m above their surroundings; meadow (the dressing's `pasture`) on floors with a wetness of 11 or more and on
  slopes over 8° not wooded. Only where the hills add 3 m or more: the plain, the named forests and the copses as before.
  The woods are canopy blocks like every other wood (so sight lines, the dressing's trees and the edges of A.3.13 follow
  them), at 100 m in the map and 400 m beyond it, so the panorama's far ridges are wooded too.
  A block is wood only where five of its nine (itself and its neighbours) are, so no lone speck or one-block hole: the
  woods' outlines are whole woods' and their edges fewer (the first try, without it, planted 794 000 trees and shrubs,
  44% of them shrubs on the edges of ragged blocks).
- **Measured** (`test_landform.cpp`, every 100 m over the hills in the map): the rule woods 21%, meadow 27%; every point
  over 15° wooded, 18% of those under 5° (the crests); every wet floor meadow; 8279 canopy blocks over the hills.
  The map proper now has 12387 canopy blocks (was 8833; `test_world_map`'s pinned layout updated); the dressing plants
  about 500 000 trees and shrubs (its budget raised from 400 000 to 600 000: the views' GPU time measured unchanged at
  about 10 ms, l11b against lu2), shrubs about 40% of them (the bound raised from a third to a half: the hills' woods
  have long winding edges). The ground layer at a point now takes its cell's meadow (`ground_layers.hpp` `PointLayer`,
  which had read the field's own crop). 1074 tests pass (the whole suite: an earlier run filtered by name had reported
  the suite's count without running it); `ground_check` PASS; `nav_check` 200 of 200; citations clean.
- **The view V13** stood in a crest's new wood: the anchor `high_ground` now takes the highest open ground (nothing
  standing within 40 m).
- **lu3 (views) and e4 (p18, p19):** V13 west and north looks over ridges with woods on their sides and crests, layered
  to the horizon; south-west, a row of woods along a crest beyond a field. The woods are still boxes draped in a green
  slab with lollipop trees at their edges (K4, K5), and p18's slope woods (single blocks) went with the smoothing: its
  slopes are pale and dark fields again. p19 unchanged (the Lazne wood is a named forest).
- **Against the look target:** K2 moved (woods on slopes and crests, meadow in the valleys); K4 and K5 now the hills'
  largest gaps (block outlines, the mass a slab, lollipop trees). Left: the woods' outlines at the bake's 50 m or finer,
  alders and willows along the brooks, villages and orchards by the ground, fields' long sides along the contours, the
  road strips on slopes (K8).

### A.3.18 L11 step 5 in first form: the lines fit the ground (7 Nov 2026)

- **Why p18's road kinked:** the game laid each road ribbon flat across, at its centre line's height, so on a cross
  slope one edge sank into the ground and the other floated; and the roads, laid before the hills were, climbed them
  as they came (in the hills inside the map: p95 16% on main roads, 15% secondary, 15% tracks, 13% the railway, the
  worst 44-59%).
- **The ribbon drapes** (`SimMapActor.cpp` `BuildStrips`): each edge on the ground under it at the strip's own lift over
  its line, the points every 12.5 m; on a bridge or its ramp (a metre or more over the ground) the deck stays level
  across; water lies level across as before.
- **The lines' grades** (`land_use.json`'s `max_grade_*`, estimates from step 5's text: main 6%, secondary 8%, tracks
  14%, the railway 2.5%; `MapData::BakeLineFits`, `FitToLines`): each of the map's roads and railways sampled every 20 m
  on the natural ground, its profile the mean of the highest under the ground and the lowest over it that keep its
  grade (cut and fill balance), and the ground cut or filled to it in `Analytic` before the sites' flats: the
  formation (the strip, 6 m each side, never under 25 m so the 50 m ground has a node on it), then the side at 1 in 1.5
  out to the natural ground. Not within 40 m of a river's bank (the bridges' ramps), not beyond the map proper (faded
  over its last 500 m, so the country's ground still meets it). Only with land use laid, so a map loaded without
  `land_use.json` is unchanged. The deepest cut or fill under a line is 24 m.
- **The farm tracks** (generated along the fields' long sides) do not climb a bank steeper than a track's grade: they
  stop at its foot and go on beyond it. Following the valley floor, a contour or a crest instead is left.
- **Measured** (`test_landform.cpp`, the strips' spans over the hills in the map proper): p95 7.7% main, 9.5%
  secondary, 11.3% tracks, 5.9% the railway; the worst 11.9%, 15.3%, 19.5%, 8.9%. Steeper than the profiles because the
  50 m ground carries a profile only to a cell; a hill detail area at 10 m (step 6) keeps them. 1075 tests pass (the
  whole suite).
- **e5 (p18):** at eye height the road runs level across the slope on its bench, no break (e4 had it kinked and
  half buried). From above, the cutting's sides cast a scalloped shadow: the 50 m ground draws the side slope as
  triangles a cell wide. 1 in 1.5 needs the finer ground too. `ground_check` PASS; `nav_check` 200 of 200; citations clean.
- **Left:** sunken lanes; `nav` reading the grade; brooks crossing a fill (their strips ride over the embankment, no
  culvert); the tracks along the contours.

### A.3.19 L7 and L10 in first form: the woods' insides are trees near, masses far (7 Nov 2026)

- **The owner, 7 Nov:** "big green shapes in the middle of all the woods". Each 100 m wood block in the map proper had
  two rows of trees on its open sides and, inside, a lumpy box (`CanopyMassMesh`), level on the block's centre, so on
  a hillside a slab standing out of the slope.
- **The insides planted** (`map_dressing.hpp` `PlaceTrees`; `map_dressing.json` `interior_spacing_m` 12,
  `interior_crown` 1.4, estimates): behind the edge rows (the whole block where no side is open), the stand's canopy
  trees on a jittered lattice every 12 m, their crowns 1.4 times a lone tree's (at 1.0, w1, the forest floor showed
  between them), each flagged `interior`. Their tops keep under the obstacle heights as every tree's. The masses are
  kept, for far off. About 707 000 inside trees, 1.2 million trees and shrubs in all (`test_map_dressing`'s budget
  raised to 1.4 million).
- **Drawn near, the masses far** (`SimMapActor.cpp` `BuildTrees`, `UpdateMassTiles`): the inside trees in components of
  their own out to `InteriorTreeDistanceM` (2.5 km); the map proper's masses by 2 km tile, a tile hidden while its
  nearest point is within nine tenths of that distance, so near the camera every wood is trees throughout and farther
  off the edge rows and the masses as before. The far ground's masses unchanged.
- **Cost** (the views, w2 against lu3): GPU within 0.4 ms (7.2-11.4 ms); the game thread about 3 ms slower (7 to 10 ms),
  to be looked into (the tiles' update is a few hundred box distances a frame; more likely the instances).
- **w2:** V13 north looks over a wood that is trees through to its far side, dark floor between the crowns; p19 at eye
  height looks into the Lazne wood's trunks (a detail area's wood, planted whole as before). From above, the hills'
  woods are crowns throughout, open where a stand is young. 1075 tests pass (the whole suite); `ground_check` PASS;
  `nav_check` 200 of 200; citations clean. V13's camera stands elsewhere than in lu3: the highest open ground moved with
  A.3.18's cuts.
- **Left:** the masses themselves (a crowned shell draped on the ground, or impostors: L10); the tile's swap is a pop
  at 2.25 km; understorey and young growth inside (the stand's layers, L7); the game thread's 3 ms.

### A.3.20 Woods that slope with the hill, alders along the brooks, the plot lab at a fixed exposure (7 Nov 2026)

Three changes in one pass (the owner, 7 Nov: more changes before each test and render).

- **A wood's top slopes with its hill** (`terrain_map.hpp` `MapBox::rise_e`, `rise_n`, `TopAt`; set in `Canopy()` from
  the ground's central difference across the block where the gradient is 2% or more, so the plain stays level and
  bit-exact). The obstacle follows the plane: `Foot::TopAt` in `ObstacleTop`, the stamp and the chords, the chord sink
  given its top at both ends (`ObstacleChordSink::Chord(t_in, t_out, top_in, top_out)`). Before, a block on a hillside
  had one level top, its height over the ground uphill a fraction of downhill. Of 7808 sloping blocks, 200 checked:
  the height difference uphill to downhill at most 16.7 m (a level top: 34.6 m; what is left is the ground not being a
  plane); rays along their tops are blocked 1 m under and clear 1 m over (`test_map_obstacles`, a new test). The
  dressing's trees take their tops from the plane, the masses are tilted to it (`SimMapActor.cpp`).
- **Alders along the hills' brooks** (`LayFarmland`; `land_use.json` `brook_row_share` 0.6, an estimate): on 60% of
  each brook's 100 m pieces, a row on one bank, the bank drawn, its trees alders (`MapBox::by_water`). 2725 rows,
  19 926 alders, none on dry ground off a brook (`test_map_dressing`).
- **The plot lab at a fixed exposure** (`-SimFixedExposure=<ev>` in `SimSkyWeatherActor.cpp`: manual exposure, no
  physical camera; `plot_lab.py` passes 0 EV by default, `exposure=None` for the old eye adaptation). A pass now differs
  from the last by the scene, not by the eye re-exposing it. The plots spread: p18 at eye height against the sun 162 mean
  grey, p19 low over shaded ploughland 34 (auto-exposure kept every plot near 110).
- **s1:** 1076 tests pass (the whole suite); `ground_check` PASS; `nav_check` 200 of 200; citations clean. Cost against
  w2: GPU and game thread within 0.3 ms in every view. V13 east: the far hills' woods sit on their slopes, no level slab.
- **Left:** a plot's own exposure (one value per run now, too bright for p18 into the sun); a view down a brook to see
  the alders; the ground under a block that is no plane (a ridge or hollow inside 100 m).

### A.3.21 A plot's own exposure, a brook plot, the woods' understorey (7 Nov 2026)

- **A plot's own exposure** (`plots.json` `exposure_ev`; `plot_lab.py` launches the game once per exposure, the plots'
  pictures being their own files): p18 -1 EV (it looks into the sun), p19 +1 EV (a wood's shade). The u1 plots' mean
  grey 59-121 (s1 at one exposure: 34-162).
- **p20, alders along a brook** (`views.hpp` anchor `brook_row`: the nearest row along a brook to `close_km`, looked at
  across the brook from its nearest point). u1: from 150 m, the rows run in broken lengths on one bank, the bank changing
  piece by piece; at eye height, dark crowns over the water and meadow. The rows' trees are one height and evenly
  spaced, more a planting than a wild alder line. No photograph in the register yet (`test_plots` lets it be, as the
  sunflower's; fetching one waits for the owner).
- **The understorey** (L7; `map_dressing.hpp`, `map_dressing.json` `understorey_keep` 0.2, an estimate): in a share of
  the inside lattice's cells, half a step off the canopy tree, a shrub of the edge's mix (hazel, elder), in patches
  where a wave lets light in; flagged `interior`, drawn out to 800 m as the edge's shrubs (`SimMapActor.cpp`). 107 650
  of them; 1.33 million trees and shrubs in all (`test_map_dressing`'s budget 1.5 million).
- **u1:** 1076 tests pass (the whole suite); `ground_check` PASS; `nav_check` 200 of 200; citations clean. Cost
  against s1: GPU 10.37 ms against 10.35 ms, the game thread 10.50 ms against 10.35 ms (the views' means). The
  understorey not yet seen in a picture: the plots' woods are detail areas' (planted whole), the views' woods are
  farther than 800 m or seen from outside.
- **Left:** a plot inside a wood of the map proper, to see the understorey; alder lines of uneven height and spacing,
  with gaps where the bank is grazed; a photograph of a hill brook's alders.

### A.3.22 Alder lines grown wild; a plot inside a wood (7 Nov 2026)

- **The brooks' alder lines** (`map_dressing.hpp`, the TreeRow planting; `map_dressing.json` `brook_gap_share` 0.25,
  `brook_bush_share` 0.15, estimates): open where a smooth wave is low (a grazed or cut bank, gaps tens of metres long),
  a hazel or elder in place of an alder here and there, the alders 60-100% of the row's height and their crowns 0.8-1.2
  of a lone tree's. 87% of the lines' places planted (the wave's spread leaves about 13% open, not the quarter asked),
  2657 of them bushes (`test_map_dressing`). v1's p20 from 150 m: runs of uneven trees with gaps, no longer a planting.
- **p21, inside a wood** (`views.hpp` anchor `wood_inside`: the nearest block of the map proper with wood on its four
  sides; `plots.hpp` lets the eye stand among the trunks there, the wood being an obstacle to sight lines, not to a
  walker; `test_plots` likewise; +1.5 EV). v1 at eye height: trunks receding, the litter floor, crowns overhead, a
  hazel or elder in patches. It reads as a park: the floor sunlit, the crowns high and apart, the far edge seen through
  the stand, and the understorey's shrubs are the placeholder primitive (a green egg). No photograph (`test_plots`
  lets it be; fetching one waits for the owner).
- **v1:** 1076 tests pass (the whole suite); `ground_check` PASS; `nav_check` 200 of 200; citations clean. Cost against
  u1: GPU 10.37 ms both, the game thread 10.47 ms against 10.50 ms (the views' means).
- **Left:** a closed stand inside: lower, wider crowns touching, young trees and saplings (the stand's layers), the
  floor in shade; shrub models (hazel, elder) in place of the primitive; the gap share by rank, not by the wave's value.

### A.3.23 The stand's layers, hazel and elder as cards, the alder gaps by rank (7 Nov 2026)

Four changes in one pass (the owner, 7 Nov: more changes before each test and render).

- **A closed stand** (`map_dressing.hpp`; `map_dressing.json` `interior_crown` 1.6, from 1.4, and
  `interior_height_spread` 0.3, estimates): the inside trees' crowns wider, and each tree from the stand's height down
  by up to 30% (dominant, codominant, suppressed), its crown with it, so no crown is wider than its tree is tall (1.8
  was tried: crowns 1.5 times their trees' height) and none under the 3 m a tree is planted at.
- **Young growth** (`sapling_keep` 0.15, an estimate): in a share of the inside cells, in the shrubs' patches of light,
  a sapling of the stand's own kinds, a quarter to a half of its height. 100 623 saplings; 1.43 million trees and
  shrubs in all, under `test_map_dressing`'s 1.5 million.
- **Hazel and elder as cards** (`tools/make_cards.py` `bush`, our own drawing; three crossed cards each,
  `build_cards.py`; picks `hazel_card`, `elder_card`, imported and registered for the parts `tree_hazel` and
  `tree_elder`): a hazel's many stems fanning from one foot, broad round leaves turning yellow; an elder's arching
  stems, narrow leaflets still green, black-purple umbels. They replace the green primitive wherever a hazel or elder
  stands: the woods' edges, the understorey, the brooks.
- **The alder gaps by rank**: the wave measured at the brook rows' own places and cut at `brook_gap_share` of them;
  the lines now 76% planted (24% open, the quarter asked; A.3.22 left 13%).
- **x1:** 1076 tests pass (the whole suite); `ground_check` PASS; `nav_check` 200 of 200; citations and assets clean.
  Cost against v1: GPU 10.50 ms against 10.37 ms, the game thread 11.11 ms against 10.47 ms (the views' means). p21 at
  eye height: a hazel or elder card among the trunks; still light, the floor sunlit in patches, the crowns high on
  their trunks. From 150 m the stands of the map proper are closed in places and open in others (the stands' ages and
  kinds); p20 an elder bush among the alders.
- **Left:** the bushes drawn too narrow (a shrub's crown is half its height, as a tree's; a hazel is as wide as tall);
  the crowns high on their trunks, so the floor stays lit (the tree models' proportions); the game thread's growth.

### A.3.24 Six kinds of tree as cards; wider bushes; hedges under the rows (7 Nov 2026)

The owner, 7 Nov: build, place and develop more assets and changes before each test and render.

- **Tree cards, our own drawing** (`tools/make_cards.py` `tree_card`, `tree_cards`; three crossed cards each,
  `build_cards.py`; picks, imported and registered for their parts): poplar (a narrow lumpy column turning yellow),
  black alder (an oval crown, dark green), black locust (a thin open crown of leaflets in clumps, yellow-green), field
  maple (a round dense crown, orange and yellow), Scots pine (a tall bare trunk orange up top, a flat cap of needle
  tufts in clumps), beech (a broad dome on a grey trunk, copper). Each a whole tree on a 512 by 1024 card: the trunk,
  branches into the crown, the crown's outline lumpy and different each side, leaves in sprays, shaded inside and lit
  at the rim. With hornbeam's and oak's registered models, every kind but none is now drawn from something better than
  a primitive (the oak's taken out, A.3.12's note). 25 registered assets.
- **A crown from above, tried and left out**: a flat quad at the crown's top carrying a top view (the card an atlas,
  side and top). From 150 m the crossed planes still read as propellers, and from the ground the quad showed as a dark
  bar across the crown (p11, y2); the player is mostly on the ground. Kept as an option (`tree_card(top=True)`,
  `build_cards.py` `TOPS`), empty.
- **Bushes as wide as tall** (`shrub_crown` 1.8, an estimate): a hazel's or elder's crown 0.9 of its height, a tree's
  0.5 (`test_map_dressing` lets a bush's crown be up to 1.3 of its height).
- **Hedges under the fields' tree rows** (`row_bush_share` 0.3, an estimate): in a share of the gaps between a row's
  trees, in runs, a bush of the edge's mix. About 12 000 of them; 1.44 million trees and shrubs in all.
- **y3:** 1076 tests pass (the whole suite); `ground_check` PASS; `nav_check` 200 of 200; citations and assets clean.
  Cost against x1: GPU 10.95 ms against 10.50 ms (worst view 12.68 ms against 11.76 ms), the game thread 11.46 ms
  against 11.11 ms (the views' means). p11 at eye height: maples and beeches in their colours over the shrub belt, the
  bushes full; p13 the poplar rows on the skyline as columns; p21 copper and orange among the hornbeams.
- **Left:** from above, the crossed cards (the propellers; more cards or impostors, not a flat top); a near tree's
  planes seen crossing (an X) at 30-50 m; a second card per kind, so a row is not one tree repeated; the cards carry
  their own colour, so the stands' seasonal tints (`TreeTintOn`) no longer reach these kinds.

### A.3.25 Fab's Megaplants as map trees: built, drawn, measured, set aside (8 Nov 2026)

The owner, 8 Nov: consider replacing the assets with Fab's and Quixel's.

- **What the project has** (the owner's Fab library, added from the editor): Megaplants European Hornbeam, Black Poplar,
  Norway Maple (forest and saplings), Common Hazel, Dry Common Reed (skeletal, four variants each); Megascans Elderberry
  and four ground surfaces; the GV Free Shrubs Pack; unpacked Megascans zips (a black locust, grasses, a poppy, a dead tree,
  surfaces). The black locust scan is a sapling (1.8 m at most, its own metadata), not a tree; the elderberry stays out
  (white flowers in October, A.3.12's note); reeds are thin and abundant, so they stay cards.
- **`tools/ue_fab_part.py`** (D7: used in place, never exported): Geometry Script copies a Megaplant's or Megascans
  mesh into a dynamic mesh in Unreal, fits it to the tree frame (pivot at the trunk's foot), saves a Nanite static mesh
  under `/Game/ThirdParty/Fab/<id>/` with the source's materials, and registers it. A pick's `season` gives the foliage a
  child instance with `MA_Foliage_Trees`'s Seasons switch on and two colours (estimates). 13 made: hornbeam A, C, D
  (120-350 thousand triangles), poplar A-D (360-810 thousand), maple A-C (30-55 thousand), hazel A-C (30-390 thousand).
  Wind does not come across.
- **Alternates** (`AssetRegister` `alternates`, `AlternateCount`, `AlternateFor`, `AlternateAt`; test
  `test_scenery`): other models of a part, picked tree by tree by a hash of the place; the map actor splits each kind's
  instances among them. `variants` stay a tree's ladder of levels. A registered model with several material slots is drawn
  with world position offset off and its shadows cached as static.
- **Seen (z1-z3):** real trees at last: trunks, branching, crowns with light through them, four different hornbeams in a
  row. The hornbeam's October came out red (yellowed for z2); the root flare stands above the ground, stretched by the
  frame's height; the bark pale.
- **Measured, and why they are not drawn:** at eye height the GPU took 147-1340 ms a frame (p11, p19, p20, p21) against
  13 ms with the cards; 46-216 ms at 25 m. On p21: with masked Nanite materials off, 1340 to 196 ms (the leaves' cut-outs,
  close and many deep); with virtual shadow maps off, 25 m from 213 to 46 ms. Static copies of million-triangle masked
  plants on 1.4 million trees do not fit this PC's RTX 3060 Ti. The parts are back on the cards; the 13 stay registered,
  one line of `parts` each to draw them again.
- **y3 checks hold:** 1077 tests pass (the whole suite); `ground_check` PASS; `nav_check` 200 of 200; citations and
  assets clean (38 registered). `pytest tools`: 161 pass, one fails (`test_voice_lines`: the recording of
  `battery_commander/order_tzm_run` is missing on this branch, not this batch's).
- **Left:** Megaplants only near the camera: a ring of the nearest few hundred trees swapped from card to model as the
  camera moves (as the ground cover's ring does), with the cards beyond; or Unreal's own Nanite foliage (the Megaplants as
  instanced skinned meshes with their voxel far levels, D7's spike). The root flare (fit from the ground up, not the
  root's foot); the October colours by date, not baked.

### A.3.26 The Megaplants in a near ring: real trees within 80 m, cards beyond (8 Nov 2026)

The owner, 8 Nov: keep working through the roadmap. A.3.25's first Left: the Megaplants only near the camera.

- **Priced first** (`-SimFabTreeM=<m>`, a measurement flag: a kind drawn by its Fab models only that far): within 150 m the
  eye views took 23-49 ms, within 60 m 18-20 ms, against 147-1340 ms with no limit. The cost was the far instances.
- **Near models in the register** (`AssetRegister`: a part's `near`, `{"alternates": [...], "within_m": 80}`; `NearCount`,
  `NearFor`, `NearM`; test `test_scenery`): models drawn for the part's trees within that distance, picked tree by tree by
  place as alternates are; the part's own (its cards) beyond. `tools/ue_fab_part.py` now registers its models so; the four
  kinds' 13 Megaplants are near models of `tree_hornbeam`, `tree_poplar`, `tree_maple`, `tree_hazel`.
- **No work per frame:** the map actor adds each near model's instances culled at `within_m`, and gives the cards' material
  `M_SimFoliage`'s new `NearHideM` (`tools/ue_build_tree_material.py`'s `near_hide`, patched into the existing material
  with `SAM3D_FOLIAGE_NEAR_ONLY=1`): the opacity mask is cut for an instance whose pivot is within that distance of the
  camera. Both by distance on the GPU; nothing swapped as the camera moves. If a near model is not on the machine the
  cards draw everywhere and hide nothing (`FSimMapAssets::NearOnMachine`), so a machine without the Fab files runs as before.
- **The hornbeam's root flare:** its picks sink the model 0.6 m before the fit (`sink_m`).
- **Seen (n1, n2):** hornbeams, maples, poplars and hazels as real trees round the camera, the cards beyond; the change at
  80 m is not obvious from the eye views. The hornbeam's roots still stand clear of the ground on p21 at 0.6 m.
- **Costs (n2 against y3):** the plots' 42 views GPU mean 10.7 ms, max 18.1; the map views GPU mean 11.79 ms (y3 10.95),
  max 14.81 (12.68), game thread 12.00 ms (11.46).
- **Checks:** 1078 tests pass (the whole suite); `ground_check` PASS; `nav_check` 200 of 200; citations clean (159 files);
  assets clean (38).
- **Left:** the hornbeam's roots (a deeper sink, or the flare cut off at the ground); the bark pale; the cards' colours to
  the Megaplants' at the ring (the poplar's yellow card against its greener model; the hazel still green); the hidden cards'
  shadows may still be drawn (the shadow pass's camera is not the eye's); a ring for the other kinds; the game thread's
  0.5 ms; the October colours by date.

### A.4 How a plot is worked (the loop)

1. Write or change the recipe (one JSON); sources in its `source`, numbers an estimate where none was found.
2. simcore tests for the plot: its statistics against the recipe's targets, determinism, obstacle heights.
3. `python tools/plot_lab.py run <plot>`: the plot world rendered from the three cameras (and its map anchor's), the
   page beside the photographs and the last approved render; under 90 s.
4. Look at every picture; change; repeat. Compare with the look target too (the page shows its pictures; D10): write
   in the pass's A.3 entry, by trait number, what moved and the largest gaps left. When it reads right, show the owner.
5. On the owner's approval the renders become the plot's ground truth; later renders are compared against it.
6. Run the map-wide report and the views (`terrain_views.py`) to see the change everywhere; the balance runs if
   anything that blocks sight moved.

### A.5 Hill country: how (L11)

The look target's ground rolls at every distance; ours is flat to the horizon (`LOOK_TARGET.md` K1). The owner, 6 Nov
2026: "Although the terrain is slightly exaggerated, implementing this type of terrain variation is a necessity." It is
made in simcore, where the ground is made (D11): `NaturalGround` (`terrain_map.hpp`) already sums the relief bands, the
hills, the country's ranges and valleys (`MapRidgeDef`), the river valleys and beds, and every grid (the core's 50 m,
the far ground's 500 m, the detail areas' 10 m) and every sight line read it. What it lacks is **regions of hill
country, valleys that drain, and land use that knows the ground**.

**Where (the owner, 6 Nov 2026: "West and north-west").** The west and north-west of the 40 km core become hill
country of the Transdanubian kind (loess hills cut by brook valleys), the east along the Rava stays the plain (Kesmark, the two
bridges, the marshes), with rolling ground between, so the map has both and the contrast. Battery 21 (6.2, -10.1)
stays on the plain's edge on its rise, with the hills a few kilometres to its west. The hills that carry posts
(Polany's P-40, Ludas's P-19, Varos's sector bunker) keep their places and heights: the landform is added round them,
and their sites stay levelled. The country map gets matching regions round the district so the hills do not stop at
its edge (`docs/COUNTRY_MAP.md`).

**How high.** Estimates, to be judged by eye at the hill plots and against photographs of the real hill country: hilly
ground 40-80 m from valley floor to crest over 1-3 km, slopes mostly 3-12° and a few to 20°; rolling ground 10-25 m
over 1-2 km. A factor in the data (`exaggeration`) lets the owner turn it up or down without touching code.

**The steps** (each with its tests: determinism, the measured relief and slopes against the data's targets, the time
budget, the posts' hills unchanged at their summits):

1. **The landform.** `terrain.landform` in the map file: regions (polygons, each a class: plain, rolling, hilly; blended
   over 2-4 km) and per class the noise's bands. Inside a region, fBm whose input is displaced by another noise
   (domain warping, so the forms wind instead of sitting on a grid of blobs) plus a ridged term for crests and spurs.
   A pure function of position inside `NaturalGround`, so the district and the country agree (`test_country_map`) and a
   sight line meets the same ground at every grid spacing.
2. **Drainage and valleys.** *(First form 6 Nov 2026, A.3.16: the forms were too full of basins to carve, so the
   ground is baked from its drainage instead, the forms only steering the valleys; the text below is the first plan.)*
   Noise alone makes basins with no outlet, which read wrong at once (K-b's valley floors).
   On the core grid (800 x 800 cells): pits filled by priority-flood (Barnes, Lehman and Mulla 2014), flow routed
   downhill and accumulated; where the area drained passes a threshold, a **brook** (a map line of a new class
   `brook`, running into the Rava or out of the map), and along each a valley cut by a cross-section stamp whose width
   and depth grow with the area (a V high up, a flat meadow floor lower down) with its bed. Later, if the forms still
   look soft: a stream-power erosion with hillslope diffusion (Braun and Willett 2013's implicit method, linear in the
   cells) for spurs and branching valleys. This widens D8, which allowed erosion on the detail levels only, to a
   deterministic bake on the map's own grids, in simcore. **The design point to settle first:** the drainage is a bake
   on a grid, not a function of position, so the carved difference is kept as a grid the core, the detail areas and the
   country's district all interpolate; `test_country_map` must keep agreeing.
3. **The ground's measures.** Slope, aspect, a wetness index (from the area drained and the slope; Beven and Kirkby
   1979) and curvature (convex crests, concave hollows) baked per cell and given to the recipes (L2's matching gains
   `slope_deg`, `aspect`, `wetness`, `curvature`) and to the ground material (L4: crests and steep arable paler where
   the soil wears thin, valley floors darker and wetter; to be sourced).
4. **Land use follows the ground (D12).** Rules as data (`data/scenery/land_use.json`): woods on slopes over about
   10-12°, on crests and in steep valley heads, their outlines following the slope with noise, not 100 m blocks;
   meadows and pasture on wet valley floors, willows and alders along the brooks; fields on the gentle ground, their
   long sides along the contours, the collective farm's big fields broken where they meet the valleys; orchards and
   vineyards with their press-houses on south slopes near villages; villages in the valleys where brooks meet and roads
   run; lone trees and copses on crests and balks. The named woods keep their outlines; the generated copses and the
   estates' fields (`farmland`) follow the rules instead of the 4 km grid. Numbers are estimates until sourced.
5. **Lines fit the ground.** Grades checked per road class (main about 6%, secondary about 8%, tracks 12-15%;
   estimates, to be sourced): steeper, and a road gets a cut or fill (L3's stamps) and a generated track follows the
   valley floor, the contour or the crest instead; **sunken lanes** where a track climbs loess (1-4 m deep, steep
   grassed banks, trees over them; to be sourced with photographs). `nav` reads the grade (its movers' speeds on a
   slope).
6. **The hill country's detail area and its plots.** A detail area at 10 m in the hills and its plots (judged like the
   others, against the look target first): **p18** a field's edge on a slope (K-a; first form 6 Nov, on the map's 50 m
   ground); **p19** a wood's edge on a hillside (K-b; first form 6 Nov); **p20** a brook in a meadow valley with
   willows (K-e); **p21** a sunken lane; **p22** a village pond with reeds and a willow (K-f, in our village's kind).
   The view **V13** (first form 6 Nov): a panorama from the highest ground near the hills' middle (the anchor kind
   `high_ground`), layered hills to the horizon and, east, the hills falling to the plain (K-b, K-d).

**What it moves.** The sight lines (battery 21's horizon, the radar posts' masking, the P-19 and P-40 on their hills),
low approaches along valleys, the routes and the movers' times, the plots' and views' anchors, and anything under the
ground. Re-read: `ground_check`, `nav_check`, `crew_balance`, `air_war_bench`, `air_war_balance`, `em_demo`,
`helper_coverage`, `mission_demo`, `test_country_map`; the moved rows explained in the step's entry.

**Sources to find** (none read yet): the inventory of Hungary's micro-regions (Dövényi, ed., 2010) for the hill
regions' relief and land use; photographs of Transdanubian hill country (Zala, Somogy, Tolna, the loess lanes of the
Szekszárd hills) from Wikimedia Commons, registered as `docs/research/TERRAIN_REFERENCES_2026-11.md` registers its
photographs; the three papers named above, before anything is ported.

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
