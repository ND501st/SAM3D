# Vegetation: how the map's woods, edges and shrubs are planted (4 Nov 2026)

How the Kesmark range's woods are populated. It draws on forestry sources from the plain's countries and on how other
games do it. The rules live in `data/scenery/map_dressing.json` and are applied by
`simcore/scenery/map_dressing.hpp` (`PlaceTrees`). The kinds are in `map_meshes.hpp` (`TreeKind`) and their autumn
colours in `data/scenery/tree_seasons.json`.

All of this is presentation. The sight lines read the map's obstacle heights, and no tree or shrub stands above them
(`tests/test_map_dressing.cpp`). The nation is fictional, so the numbers are judged by eye against photographs; the
sources say what a real wood is like, not what this one must count.

## 1. What a wood on the plain is like (the sources)

**Stands.** A managed Central European wood is cut into compartments, one species and one age each, about 2-20 ha.
Rides run between them, and the canopy steps sharply at a compartment's line. This is the strongest look of a 1980s
state forest. Numbers:

- Oak: thinned to 150-450 stems/ha by its rotation of 100-130 years (Solymos 1993).
- Black locust: planted 2.4 x 0.7-1.0 m, 900-1000 stems/ha at felling (Rédei et al., *Acta Silvatica Lignaria
  Hungarica* 2012).

Plantations (pine, hybrid poplar) are rows; natural stands are near-random.

**Species by site.**

- Black locust is about 24% of Hungary's forest (Rédei et al.).
- The sand plains' state forest is pine 49%, poplar 25% and black locust 22% (KEFAG).
- One hill district's woods are sessile oak 35%, beech 30%, Turkey oak 18% and hornbeam 7%.
- The floodplains have white willow and poplar on the lowest ground, and alder-ash woods above them (Kevey and Barna).
- Beech grows only on the cooler hills.

**Edges.** A natural edge has three bands from the field in:

- the herb fringe, at least 5 m;
- the shrub belt;
- the mantle of light-demanding trees (oak, field maple, wild cherry, locust).

Mantle and shrubs are 10-20 m in practice (Thurgau forestry office) and 25-40 m at best (WWF Switzerland). A managed
edge against 1980s ploughland is thinner and patchy, often a few metres of shrubs (the Swiss inventory's shrub-belt
classes run from none to over 4 m).

**Lines.** The plain's signature is its lines:

- shelter belts 15-50 m wide, more of them in the 1980s than today (MDPI *Land* 10(9):930);
- poplar and locust rows along roads and ditches;
- willows on the ditches;
- fruit trees in the villages' long gardens.

True hedgerows are rare among the collective farms' large fields.

## 2. What other games do

- **Horizon Zero Dawn** (GDC 2017): density maps combined per species by rules, turned into points deterministically.
- **Ghost Recon Wildlands** (80.lv): a cascade, where big trees spawn small trees and bushes, by soil, height, slope
  and water.
- **Arma Reforger's** forest generator: an outline band with its own species and scale, and clusters of a species with
  their own density and radius.
- **Serious Sam 4** and most open worlds split the work: full trees near, single impostors to a few km, then a
  canopy shell or the ground's own colour.

Players notice two things most: tree heights, and density in towns and pastures (MSFS complaints). From a vehicle's roof
on the plain the first treeline is the horizon, so detail goes into edges and lines, not into a wood's inside.

SAM3D already had most of the far part: the canopy masses, the land-cover colour and the trees' LOD spindles. What it
lacked was the near part: stands, edges and shrubs.

## 3. What is built

- **Ten kinds.** Oak, poplar, pine and black locust (`acacia`), as before, then:
  - hornbeam and beech (the oak-hornbeam and beech woods);
  - black alder (wet ground);
  - field maple (the mantle);
  - hazel and elder, the fringe's shrubs (`IsShrub`).

  Each kind has its own autumn colour: hornbeam and maple yellow, beech orange-brown, alder late and dull.
- **Stands.** A wood is cut on a 150 m grid (`stand_m`, about 2 ha). Each compartment has a stand type, chosen by its
  site and weight:
  - The site is wet within 80 m of water (`wet_m`), dry elsewhere.
  - Each compartment has an age: its trees are 0.75-1.0 of the wood's height (`age_min`).
  - The dry types are oak-hornbeam, oak, black locust, pine plantation and beech-hornbeam. The wet types are alder,
    poplar plantation and poplar-alder (`stands`).
  - A plantation's trees keep to the lattice (`plantation_jitter`).
- **The mantle.** Trees within 12 m of an open side (`edge_band_m`) are the edge's kinds (`edge_mix`: oak, locust,
  hornbeam, field maple), from half the stand's height at the trees' line to the stand's at the band's inner edge
  (`edge_height`), their crowns up to 1.3 times as wide (`edge_crown`). In a wood planted whole the trees' line stands
  back 0-7 m from the side in waves about 35 m long (`edge_setback_m`, `edge_wave_m`): bays and tongues, not a wall
  (roadmap A.3.13).
- **The shrub belt.** Shrubs stand from 1.5 m inside each open side to a depth wandering between 1 and 7 m more
  (`shrub_belt_m`), two places every 3 m, kept in runs and gaps (`shrub_keep`). They are 1.5-4 m high. They stand inside
  the wood's block, so the sight lines' obstacle covers them.
- **The herb fringe.** The forest floor's recipe draws dense grass and dry stalks in the first 3.5 m in from the open
  side, and patchy grass under the mantle (`data/scenery/recipes/forest_floor.json`, edge `wood`).
- **Unreal.** Each kind is its own instance list. The shrubs stop drawing at 800 m.

On Kesmark this comes to about 270-300k trees and shrubs against a budget of 400k (the test prints the count by kind).

## 4. Art

The procedural meshes (`TreeMesh`) draw every kind until a real model is registered for its part (`tree_<kind>`). The
real models go through the oak's pipeline (`docs/scenery/T5_T8_PLAN.md` sections 12-14):

1. thin the model in Blender;
2. lay out a distance ladder of 0-150, 150-600 and 600-1500 m;
3. apply the foliage material;
4. register it with its licence.

Free sources found (4 Nov 2026):

| Kind | Source | Licence | Format |
|---|---|---|---|
| hornbeam, alder, maple, beech, hazel | Quixel Megascans tree packs on Fab (27, 22 and more trees each, with LODs, seasons and wind) | Fab Standard, Unreal only | Unreal Engine only: added to the owner's Fab library; they come into the project through the launcher's or the editor's Fab ("Add to project"), under `/Game/ThirdParty/` |
| black locust (a thicket of suckers, not a tree), elderberry, spindle, wild grass, dead shrubs | Megascans plants on Fab | Fab Standard | FBX or glTF, downloadable (the locust is a 214 MB raw scan) |
| oak | PlantCatalog's northern red oak (Sketchfab) | CC BY 4.0 | in use, thinned (section 13-14 of the plan) |
| poplar (Lombardy, hybrid), black locust tree, pine, fruit trees | none free and game-ready found | | to be generated: EZ-Tree (MIT; the output is ours) or Blender's Sapling, then the same pipeline |

Thin, abundant plants (grass, stubble, reeds, crops) stay cards (the owner's rule; `tools/make_cards.py`).

## 5. Left

- Field and road trees: solitary oaks, pears and walnuts on field boundaries and crossroads, and fruit trees in the
  villages' gardens (the gardens exist as cover; `garden_m`).
- Willows on the ditches.
- The fringe's other shrubs (blackthorn, hawthorn, dog rose) once art for them exists.
- Bays in the edge's outline, which is still the block's straight side.
- Shelter belts with a shrub row on each side (the tree rows exist, roadmap T4).
- Real models for the new kinds, as the art comes in (section 4).

## 6. The canopy masses (4 Nov 2026, run `veg2`)

`CanopyMassMesh` is no longer a chamfered box. Its top is a height field of two scales of value noise (a crown of
lumps, the highest at the box's top), its outline is pulled in raggedly, and the vertex colours blotch: lighter on the
lumps, darker in the hollows and down the sides. The wall has its own rim vertices, so the crease between crown and
side stays sharp. At V8 the wood masses now read as rolling canopy instead of lids. GPU against `veg1`: +0.1 to +0.9 ms
(V6 8.0 to 8.7, V7 h000 8.1 to 9.0, V8 9.0 to 9.2). Left: a noisy shell has no per-tree texture, so a wood close in
still shows its shrubs and edge trees against a smooth interior.

## 7. What the Fab packs turned out to be (5 Nov 2026, in the project under `Content/`)

Checked with `tools/ue_inspect_megaplants.py` and `tools/ue_export_megaplant.py` (Unreal's Python, headless).

- **Megaplant_Library** (black poplar, hornbeam, common hazel, Norway maple; 20 meshes A-D): all **skeletal** Nanite
  meshes with a wind rig, from 0.5 m to 32 m. Unreal's FBX exporter asserts on them (`SkinnedMeshComponent.cpp:4987`,
  with or without a render device), so they cannot go through the Blender pipeline as they are. The black poplar (31.8 m,
  four textures of 18-49 MB) is the one worth the work: it is the poplar nothing else free gave. A way in would be the
  engine's own static-mesh conversion of a skeletal mesh, not tried.
- **GV_FreeShrubsPack** (Greenleaf Vision): **static** Nanite foliage meshes, 2.4-13 m, a `full`, `branches`, `L1` and
  `L2` per type. They export (`GV_Vol7_Shrub_B_type1_L2`: 6.9k triangles, one material `MI_Shrub_Leaf_A_Master_Wind_LV3_Opaque_Inst`, 3.3 x 3.9 x 2.9 m). Hazel-like
  shrub sizes; they need thinning to about 1k triangles, a baked RGBA leaf texture and a check that the tint takes.
- A full disk (C: at 0 MB) crashed the first export; 30 GB of old session scratch under `AppData\Local\Temp\claude` was
  deleted with the owner's say-so.

## 8. The close look at a wood's edge (V12, 5 Nov 2026) and what was left out

`v12_wood_edge` (new view, anchor `wood_edge`: the nearest wood's edge, the eye 25 m off) is the first picture of the fringe from
where a person stands. What it showed, and what was done:

- The **GV hazel** (shrub B thinned to 1.5k triangles) came out a solid green column with pale patches: decimating leaf
  geometry closes it up. Not used; unregistered.
- The **Megascans elder** shows its white flowers in October. Not used; unregistered. Both stay in `model_picks.json`
  marked NOT USED.
- So the fringe is the procedural shrubs again (smooth spindles), and the trees behind are still lollipops on a flat
  wooded hill. The next gain is in those, not in more shrub models: a shrub with leaf-card clusters (not a thinned
  mesh), trees with real crowns, and a canopy mass with texture.
- The cost of V12 with the real models in: GPU 7.1 ms.
