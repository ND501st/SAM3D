# Free vegetation models for Pannonia 1985 (UE 5.8)

Research for the terrain roadmap (R6, "a more realistic map"). Date: 2026-11-03. Researcher: Claude (subagent).
Scope: free 3D vegetation for a 1985 Hungarian plain (Alfold) and its northern hills, mid-October (15 Oct 1985) with
summer missions possible. Every pick must be free and licensed CC0-1.0, CC-BY-4.0, Fab-Standard or Epic-Content-Only
(`docs/scenery/ASSETS.md`). Nothing was downloaded; only pages and catalogue APIs were read.

How the research was done:
- Fab: read through Fab's own catalogue JSON (`https://www.fab.com/i/listings/search?...&is_free=1` and
  `https://www.fab.com/i/listings/<uid>`) from a fab.com page in the browser pane, so price, discount and licence fields
  are Fab's own. A listing is called **permanently free** below when its price tier is $0 with no discount and no
  discount end date (`discountedPrice: null`, `discountEndDate: null`); a **limited-time free** item would show a
  discount with an end date. None of the picks is limited-time free.
- Sketchfab: the Sketchfab Data API (`https://api.sketchfab.com/v3/search`, `/v3/models/<uid>`, `/v3/models?user=`).
  Sketchfab began answering HTTP 429 (too many requests) part way through, so some Sketchfab details below come from
  the search listing only (name, author, licence label, face count, date) and are marked **not re-verified**.
- Poly Haven: `https://api.polyhaven.com/assets?t=models&c=nature` and `/info/<id>`.
- The session's web-search budget ran out near the end; a few questions are left open (see Gaps).

---

## 1. The three big free sources and what they mean for SAM3D

### 1.1 Quixel Megaplants (Fab, permanently free, Fab-Standard, UE 5.7/5.8, experimental)

- 41 Megaplants listings by seller "Quixel Megaplants", every one priced $0 for both Personal and Professional
  (Fab Standard) licences, no discount, read from the Fab catalogue API on 2026-11-03
  (https://www.fab.com/sellers/Quixel%20Megaplants). Quixel's own news posts say they are "free to use under the Fab
  Standard License" (https://quixel.com/news/quixel-on-fab-new-megascans-and-megaplants, 8 Apr 2026;
  https://quixel.com/news/discover-the-latest-quixel-megascans-and-free-megaplants, 16 Jul 2026).
- What they are: procedural plant recipes for the Procedural Vegetation Editor (PVE): skeletal meshes, a PVE graph,
  foliage meshes, 4K textures (8K for Silver Birch); Fab technical details say "Nanite: Yes, LODs: No" (catalogue API,
  e.g. https://www.fab.com/listings/83642c38-7661-4df1-8629-0422e1898d26).
- Seasons: the seller page says the library features "true-to-life growth, seasonal control, and natural variation"
  (https://www.fab.com/sellers/Quixel%20Megaplants/about).
- Requirements: enable the "Procedural Vegetation Editor" plugin and "Nanite Foliage (Experimental)"; Quixel: "Megaplants
  and the Procedural Vegetation Editor are currently Experimental" and does not recommend shipping with experimental
  features (https://forums.unrealengine.com/t/quixel-megaplants-megaplants-english-oak/2702708).
- Engine: compatible with UE 5.8; "future releases will support UE 5.8 and above only"
  (https://quixel.com/news/discover-the-latest-quixel-megascans-and-free-megaplants).
- PVE in 5.8 (official doc, https://dev.epicgames.com/documentation/unreal-engine/procedural-vegetation-editor-pve-in-unreal-engine):
  ships with the engine as an experimental plugin; output "is a Nanite asset that uses the latest Nanite Foliage
  technology, including instancing, GPU-based skeletal-driven animation, and voxel representation in the distance";
  the Export node "can also write geometry as a standard static mesh if needed"; "5.7 assets are not compatible with
  5.8"; the create menu offers "a pre-made sample graph". It also has a Grower node that grows vegetation from scratch,
  and Extract From Mesh, which takes any static mesh and extracts growth data (useful for the Robinia gap below).
- Known risk: packaged builds crashed on load with PVE/Megaplants Nanite Foliage meshes in 5.8 (UE-386321, marked
  fixed, target 5.8.2; users still saw it on 5.8.1 in August 2026)
  (https://forums.unrealengine.com/t/procedural-vegetation-nanite-foliage-meshes-crash-packaged-project/2730886).
  An earlier 5.7 report: non-Nanite static mesh export from PVE broken
  (https://forums.unrealengine.com/t/procedural-vegetation-editor-cannot-export-non-nanite-foliage/2718593).
- For SAM3D: Megaplants are the best species match for oak, poplar, Scots pine, birch, hornbeam, beech, willow, elder,
  reed and bulrush. Their natural path is Nanite Foliage (not our HISM of static meshes). Either (a) export static
  meshes from PVE and register them as HISM parts, or (b) add a Nanite Foliage instancing path in Unreal for near
  ground. Gate on 5.8.2+ and a packaged-build test.

### 1.2 Megascans free selection (Fab, permanently free, Fab-Standard)

- From 2 Jan 2025 Epic offers "1500+ assets" plus "5 tree packs and 11 scenes, the latter two for Unreal Engine only",
  under the Fab Standard licence, and intends to keep some free long-term
  (https://forums.unrealengine.com/t/megascans-starter-content-on-fab/2255322). The rest of Megascans became paid in
  2025 (https://80.lv/articles/megascans-no-longer-free-after-2024/).
- The 5 free tree packs (all $0, no discount, Fab catalogue API 2026-11-03): European Beech, European Hornbeam,
  European Black Alder, Norway Maple, Common Hazel. Each is "a complete tree species pack" (forest trees, field trees,
  saplings, seedlings) with "a dynamic shader ... procedural controls over seasons, health, wind", "unique LODs, shader
  quality tiers, and various wind implementations (both simple and Pivot Painter)" (Fab listing descriptions, e.g.
  https://www.fab.com/listings/d11cc01d-9422-41b7-950f-416c9ce79caf).
- Technical (from the first pack, Black Alder): 22 meshes; vertex counts LOD0 20k-640k down to LOD5 1k-13k for mature
  trees; 7 LODs for full trees, 4 for saplings; 44 impostor textures, 88 Pivot Painter textures; textures up to 8K;
  lightmap UVs and collision; ~8 GB (https://gamefromscratch.com/quixel-megascans-trees-released-free-for-unreal-engine-developers/).
  The packs support UE4 and UE5 (https://80.lv/articles/quixel-released-european-hornbeam-pack-for-megascans-trees).
- For SAM3D: **these are the only free, species-correct European broadleaf trees that are static meshes with LOD
  chains, impostors, wind and a season control**: they drop straight into our HISM path at hundreds of thousands of
  instances, and the season shader gives the 15 October colours. Hornbeam, beech, alder, hazel and (as a proxy) field
  maple are covered by them.
- Free Megascans plants (FBX/GLTF, "Plants" category, all $0): Black Locust (a low plant: robinia suckers/sprouts, seen in
  the listing image), Elderberry, European Spindle, Raspberry, Field Poppy, Dry Grass, Wild Grass (two), Wheat Grass,
  Ribbon Grass, Kikuyu Grass, Thatching Grass, Ground Cover, Curbside Weeds, Dead Shrubs, Greater Burdock, Common Hemp
  Nettle, Small Balsam, Bog Marshcress, Yellow Archangel, Lady Fern and others (enumerated through the Fab API for
  seller "Quixel Megascans" with `is_free=1`).
- **Paid, excluded** (Fab catalogue, seller Quixel Megascans): Sunflower $3.99, Wheat $3.99, Reed Grass $3.99, Giant
  Reed $3.99, Rose Hip $4.99, "Dense Flat - Sedge" $3.99, Swamp Grass $3.99, Cotton Grass $3.99, Tufted Grass $3.99
  (https://www.fab.com/sellers/Quixel%20Megascans).

### 1.3 PlantCatalog's free CC-BY models on Sketchfab

- e-on software's PlantCatalog (a paid subscription product: https://www.cgchannel.com/2020/02/e-on-software-launches-plantcatalog)
  publishes some species free on Sketchfab, all labelled **CC Attribution**, downloadable. Its collection is "PlantCatalog
  free models" (Sketchfab API `collections?user=PlantCatalog`, uid 51dc1d2c505f47b49576f31905a721eb). Enumerating
  `https://api.sketchfab.com/v3/models?user=PlantCatalog` gave 2,306 models.
- Species useful to us: **Black poplar** (Populus nigra, 105 variants), **Lombardy poplar** (Populus nigra 'Italica',
  75 variants), **Northern red oak** (Quercus rubra, 138 variants + seedlings + leaf litter), **Rosemary willow**
  (Salix elaeagnos, 99 variants), Norway spruce. Each variant is a preset (standard, urban, dying, snag, saw-cut) x
  season (summer, fall, winter, sometimes spring) x age (infant to old), with height in the description.
- Example: "Realistic HD Black poplar (5/105)": Populus nigra, old, thriving, summer, ~28.4 m, ~180,700 leaves, LODs,
  static mesh, 1,552,416 faces, 13 materials, 19 textures, CC BY 4.0, author PlantCatalog
  (https://sketchfab.com/3d-models/realistic-hd-black-poplar-5105-c0cfae097dec4fde9d7c20314a0c79aa).
- Face counts are heavy: Lombardy and black poplar variants range 16,849 to 1,731,226 faces, median 353,843
  (computed from the 200 variants' `faceCount` in the API listing). They need Nanite or baked impostors for mass use.
- CC-BY: credit "PlantCatalog (e-on software), CC BY 4.0, <model URL>" in the credits.

---

## 2. Picks by part

Part keys from `MapMeshParts()`: `tree_oak`, `tree_poplar`, `tree_pine`, `tree_acacia`, `canopy_mass`. New keys the
roadmap may add: hornbeam/beech for the hills, willow, birch, maple, shrubs, reeds, ground clutter, crops.

### 2.1 Oak (Quercus robur / cerris / petraea) - `tree_oak`

| Pri | Asset | Site, licence | Specs | Notes |
|---|---|---|---|---|
| primary | **Megaplants: English Oak** (Quercus robur) https://www.fab.com/listings/83642c38-7661-4df1-8629-0422e1898d26 | Fab, Fab-Standard, $0 permanent | 14 skeletal meshes, 18 foliage meshes, PVE graph, 4K, Nanite yes, LODs no, UE 5.7/5.8 (Fab API) | Species-correct; seasons per seller page; experimental (1.1). First published 27 Feb 2026, updated 17 Jun 2026. |
| alternate | **PlantCatalog: Realistic HD Northern red oak** (Quercus rubra), e.g. fall juvenile (37/138) https://sketchfab.com/models/b7ed18f291c3445a8b805da379e3de4f , winter mature (89/138) https://sketchfab.com/models/cdfbf14985d747caba0e953c23a805f9 , summer old (5/138) https://sketchfab.com/models/06474083fabe4e0f88e40848ea7aa8f9 | Sketchfab, CC-BY-4.0, author PlantCatalog | 40k-884k faces per variant; seasons incl. fall | Wrong species (North American, lobed pointed leaves, red autumn colour) but the only free oak with LODs and fall/winter variants; use for distant/wood-edge mass only. |

Rejected: "3 English Oak" (Yagiz, CC-BY, https://www.fab.com/listings/8a517c31-6e78-4a42-b84d-3dc65026660f): "dead
fantasy style oak trees", 8-10k tris; maybe for dead or shelled trees only. "Quercus Rubra Tree" (SachinGames, CC-BY,
https://www.fab.com/listings/3ccde119-1eef-4e5a-8962-877f5873a472): not inspected further. No free Turkey oak (Q. cerris)
or sessile oak (Q. petraea) was found.

### 2.2 Black locust (Robinia pseudoacacia) - `tree_acacia`

| Pri | Asset | Site, licence | Specs | Notes |
|---|---|---|---|---|
| primary | **Megascans: Black Locust** (plant) https://www.fab.com/listings/63577d63-cc5e-41a8-8038-614318314eb8 | Fab, Fab-Standard, $0 permanent | FBX + GLTF; maps normal, cavity, roughness, bump, translucency, gloss, opacity, specular, basecolor, displacement; raw zip 213.87 MB; published 14 Nov 2024 | A low sprout/sucker plant, not a tree (listing image). Right species for acacia-grove undergrowth and the suckers that edge every robinia stand. |
| alternate | **Robinia hispida** (UESReseach) https://sketchfab.com/3d-models/blenderrobinia-hispida-l-85fe17a0125a423dad3bf1b46a18223d | Sketchfab, CC-BY-4.0, author UESReseach | 956,148 faces, 3 materials, **0 textures**; generated with The Grove (Sketchfab API) | Same genus (bristly locust), pinnate leaves; would need texturing and decimation. Weak. |

Also seen: "Black Locust (Robinia pseudoacacia)" by rufusrockwell, CC-BY, 3,448 faces, "Black Locust plant"
(https://sketchfab.com/3d-models/black-locust-robinia-pseudoacacia-7373f939f20e485a96f1bf7e1cf493ac): a small plant.
False positives: "Robinia pseudoacacia - Black locust" by maksm (CC-BY) is a wood-science sample of the timber, not a
tree (https://sketchfab.com/models/8e41eb2b903545c2822a6aeed62fc089); vinciplay's CC0 "ROBINIA RB####" models are
playground equipment of the Robinia brand (same user's RECYCLED, SWING, PARK models), not trees.
**Gap: no free, realistic mature Robinia pseudoacacia tree exists in the sources searched.** Best route: grow one in
PVE (Grower node, 1.1) or with Extract From Mesh from a similar trunk, with a pinnate leaf card made from a CC0 photo.

### 2.3 Poplar - `tree_poplar` (hybrid plantation Populus x canadensis and Lombardy poplar)

| Pri | Asset | Site, licence | Specs | Notes |
|---|---|---|---|---|
| primary (plantation) | **Megaplants: Black Poplar** (Populus deltoides) https://www.fab.com/listings/6b22075c-53b6-4973-a6a5-9f4ccd6a72a8 | Fab, Fab-Standard, $0 permanent | 18 skeletal meshes, 21 foliage meshes, 4K, Nanite yes, LODs no, UE 5.8 only; published 31 Jul 2026 | P. deltoides is one parent of P. x canadensis, so the plantation hybrid's habit and leaf are close. |
| alternate (plantation / floodplain) | **PlantCatalog: Realistic HD Black poplar** (Populus nigra), e.g. summer old (5/105) https://sketchfab.com/3d-models/realistic-hd-black-poplar-5105-c0cfae097dec4fde9d7c20314a0c79aa , dying winter (60/105) https://sketchfab.com/models/cb3551af2b2644188f9f34b05971d57b , dead winter (65/105) https://sketchfab.com/models/3bab3a855b4846f7b6248c6641d63c63 | Sketchfab, CC-BY-4.0, PlantCatalog | 105 variants, summer/fall/winter, LODs, up to 1.55M faces | P. nigra is the other parent and the native floodplain poplar. |
| primary (Lombardy) | **PlantCatalog: Realistic HD Lombardy poplar** (Populus nigra 'Italica'), e.g. urban fall old 29.1 m (25/75) https://sketchfab.com/models/de5a5e802c334e8d891f5b0e3746c978 , dying fall mature 27.0 m (54/75) https://sketchfab.com/models/c15301c2a0c04072b7b473eb77369a4b , standard summer old 30.7 m (5/75) https://sketchfab.com/models/f14838fae0a747568e27c64394832baa , standard summer infant 3.9 m (1/75) https://sketchfab.com/models/9a7f3416eacd47c3b731bf171c4ae5db | Sketchfab, CC-BY-4.0, PlantCatalog | 75 variants incl. fall; 46,707 to 1,826,972 faces (search listing) | Species-correct roadside/windbreak poplar with autumn variants. |
| alternate (Lombardy) | **"Poplar tree" (Populus pyramidalis)** by Georgeous (@intice184) https://sketchfab.com/3d-models/poplar-tree-a5b96e45d2a3477ea46be04573abd72e | Sketchfab, CC-BY-4.0, author Georgeous (intice184) | 31.3k tris, 19k verts, LOD0 + LOD1, bark 512x2048, branches 2048, diffuse + normal, **impostor textures and a plane for Unreal's octahedral impostor plugin** (page read 2026-11-03) | Game-ready; the best-suited for hundreds of thousands of instances among the poplars. |

Also: the same author's "Poplar tree" (Populus alba, white poplar), LOD0/LOD1 + PBR, 26,415 faces, CC-BY
(https://sketchfab.com/models/b47ff662f35644638cbf4c95eca4772a; from the search listing, **not re-verified**), good
for sandy Alfold (Kiskunsag) white/grey poplar groves. "Fall Grijze Populier Tree 2K" (grey poplar, autumn) by HKhalife,
Fab-Standard, $0, Blender format, 9-13 m tall, "low-poly design"
(https://www.fab.com/listings/0cf4c676-e56f-4762-8c5d-60360543f66d).

### 2.4 Pine (Scots pine / Austrian black pine) - `tree_pine`

| Pri | Asset | Site, licence | Specs | Notes |
|---|---|---|---|---|
| primary | **Megaplants: Baltic Pine** (Pinus sylvestris) https://www.fab.com/listings/a2b04e81-5075-479f-a9d2-4940022f330a + **Baltic Pine Saplings** https://www.fab.com/listings/ae732b47-14db-422e-b11d-c93537cb5165 | Fab, Fab-Standard, $0 permanent | 20 skeletal meshes, 26 foliage meshes, 6 material instances, 12 textures, 4K, Nanite yes, LODs no, UE 5.7/5.8 | Scots pine is the Alfold's planted sand-binding pine. Also Megaplants: Dead Pine (https://www.fab.com/listings/bc9e4e89-fcf4-4be6-b0f0-8b37395a72de) for damage. |
| alternate | **Poly Haven: Pine Tree 01** https://polyhaven.com/a/pine_tree_01 | Poly Haven, CC0-1.0 | 17,427,094 polys, 8K textures, geometry-nodes build, published 23 Jun 2023 (API /info/pine_tree_01); authors Rob Tuytel (photography), Rico Cilliers (modeling) | Species not stated ("tall, slender pine"). Redistributable, but only usable as a Nanite hero or as a source for baking impostors. |

Also: "Pine Tree Pack (Imposter)" by Next Spring, CC-BY, 15 billboard textures + 3D impostor model
(https://www.fab.com/listings/1be6afff-f5db-4a8b-9fa3-ba554b9fe3f6), for far plantations.
**Gap: no free Austrian black pine (Pinus nigra)**; CGTrader/Xfrog versions are paid. Use Scots pine.

### 2.5 Birch

| Pri | Asset | Site, licence | Specs | Notes |
|---|---|---|---|---|
| primary | **Megaplants: Silver Birch** (Betula pendula / papyrifera) https://www.fab.com/listings/94262633-61ed-4f4b-993e-dbc24af3e6ba | Fab, Fab-Standard, $0 permanent | 27 skeletal meshes, 41 foliage meshes, 5 MIs, 8 textures, **8K**, Nanite yes, LODs no, UE 5.7/5.8 | |
| alternate | **Free Birch Biome Sample** (FreshCan) https://www.fab.com/listings/fd8126f6-4dea-4023-9213-2af433743995 | Fab, Fab-Standard, $0 | UE format; "PCG Enabled by default"; birch samples from their Countryside - Rural environment; published 30 Oct 2026 | Very new; LODs/tri counts not stated in the listing. |

### 2.6 White willow (Salix alba)

| Pri | Asset | Site, licence | Specs | Notes |
|---|---|---|---|---|
| primary | **Megaplants: Goat Willow** (Salix caprea) https://www.fab.com/listings/7186b73d-7bca-4095-b195-15f050262c64 | Fab, Fab-Standard, $0 permanent | 38 skeletal meshes, 40 foliage meshes, 4K, Nanite yes, UE 5.7/5.8 | Proxy: a willow, but broader leaves than S. alba. A forum user reports Goat Willow works as a static mesh in 5.8 packaged builds (crash thread, 1.1). |
| alternate | **PlantCatalog: Realistic HD Rosemary willow** (Salix elaeagnos), e.g. fall mature 1.7 m (14/99) https://sketchfab.com/models/57b283f813224873930b54ada6345fc9 , winter old 3.1 m (52/99) https://sketchfab.com/models/865ef5e25fdb45748c19a1eb6fa61856 | Sketchfab, CC-BY-4.0, PlantCatalog | 99 variants, spring/summer/fall/winter, 4.5k-270k faces, 0.2-3.2 m | Shrub willow with narrow leaves: riverbanks and marsh edges. |

**Gap: no free Salix alba**; white willow models found are paid (CGAxis, CGTrader, Blender Market). "Detailed Willow
Tree" by Montaha (CC-BY, FBX, https://www.fab.com/listings/f08679d9-b20d-46c3-8e26-594f3c2a08f5) was not inspected (likely
weeping willow).

### 2.7 Hornbeam and beech (northern hills)

| Pri | Asset | Site, licence | Specs | Notes |
|---|---|---|---|---|
| primary (hornbeam) | **Megascans Trees: European Hornbeam** https://www.fab.com/listings/c6f917b6-ffcb-4b86-9d9f-5274ba7f6a8e | Fab, Fab-Standard (UE only per Epic's 2 Jan 2025 note), $0 permanent | 27 trees: 10 forest, 4 field, 8 saplings, 5 seedlings; LODs, impostors, simple + Pivot Painter wind, season shader | Static meshes for HISM; season control gives October. |
| alternate (hornbeam) | **Megaplants: European Hornbeam** (Carpinus betulus) https://www.fab.com/listings/43679e75-6ed0-4b03-9288-9e2bc49376d2 | Fab, Fab-Standard, $0 | 11 skeletal meshes, 15 foliage meshes, Nanite, UE 5.7/5.8 | |
| primary (beech) | **Megascans Trees: European Beech** https://www.fab.com/listings/d11cc01d-9422-41b7-950f-416c9ce79caf | Fab, Fab-Standard, $0 permanent | 17 trees: 8 forest, 1 field, 4 saplings, 4 seedlings; LODs, wind, season shader | |
| alternate (beech) | **Megaplants: European Beech** (Fagus sylvatica) https://www.fab.com/listings/cefe5722-9c31-4aa2-9ee2-e5426610d5e6 | Fab, Fab-Standard, $0 | 18 skeletal meshes, 27 foliage meshes, Nanite | Also Megaplants: European Beech Saplings (https://www.fab.com/listings/6867d008-fc3a-404e-9e53-087eccdcd722). |

Also: "KV Beech Tree 01 - Cinematic Animated Tree | Nanite | Wind | Seasons" by Kraynikkk, CC-BY, UE format, seasons
spring to winter with leaf colour shift and falling leaves (https://www.fab.com/listings/fb155516-1b56-47b9-82ae-bba8343fad9b):
a cinematic hero tree, not for mass use.

### 2.8 Field maple (Acer campestre)

| Pri | Asset | Site, licence | Specs | Notes |
|---|---|---|---|---|
| primary | **Megascans Trees: Norway Maple** (Acer platanoides) https://www.fab.com/listings/1f65d477-f44e-4ccb-95c1-788c99e33b56 | Fab, Fab-Standard, $0 permanent | 15 trees: 8 forest, 2 field, 3 saplings, 2 seedlings; LODs, wind, season shader | Proxy (larger leaves than field maple); native to the Hungarian hills. |
| alternate | **Megaplants: Norway Maple Forest** (Acer platanoides) https://www.fab.com/listings/e789ad04-cf79-4973-adb0-85c794d77144 | Fab, Fab-Standard, $0 | 20 skeletal meshes, 30 foliage meshes, Nanite | Also Megaplants: Norway Maple Saplings. |

**Gap: no free Acer campestre.**

### 2.9 Extra trees worth having (riverside, hill woods)

- **Megascans Trees: European Black Alder**, 22 trees (9 forest, 5 field, 6 saplings, 2 seedlings), $0
  (https://www.fab.com/listings/9de7ce19-5813-42d2-a5f0-5e6447006f72); alternate **Megaplants: Black Alder**
  (https://www.fab.com/listings/9257b627-e37a-4112-82c7-4a99c1a8aec6; one user reported missing parent materials:
  https://forums.unrealengine.com/t/quixel-megaplants-megaplants-black-alder/2674606). Alder lines Alfold streams and
  canals.
- **Megascans Trees: Common Hazel** (https://www.fab.com/listings/81bc7ba6-4686-4f94-9d2b-83eb1fdc4079) and Megaplants:
  Common Hazel (https://www.fab.com/listings/27ded0d2-8bb2-4e44-a795-3a35725af218): hill-wood understorey.
- Megaplants: European Aspen (https://www.fab.com/listings/ffa90e1a-e420-43d6-ade3-daa4bc189a0a) and Aspen Saplings.

### 2.10 Shrubs for wood edges and hedges (hawthorn, blackthorn, elder, dog rose)

| Category | Pri | Asset | Licence | Notes |
|---|---|---|---|---|
| hawthorn/blackthorn (hedge) | primary | **Free Shrubs Pack (Ultra Realistic Wind)**, Greenleaf Vision https://www.fab.com/listings/7ca465ab-fb9c-4d6b-bddb-82c20f604657 | Fab-Standard, $0 permanent (published 23 Aug 2025, updated 17 Jun 2026) | "11 different shrubs", "high poly opaque nanite meshes" (no masked materials), Pivot Painter 2 wind, "summer, autumn and winter version", leaf density control, UE 5.4+, 1.3 GB. Species not named: generic hedge shrubs. |
| hawthorn/blackthorn (hedge) | alternate | **Megaplants: European Spindle** (Euonymus europaeus) https://www.fab.com/listings/f32d5cdc-de95-4ec1-a0b1-fc501ea5e0c5 | Fab-Standard, $0 | A true hedgerow and wood-edge shrub of the region; 11 skeletal meshes, Nanite. Megascans also has a free static "European Spindle" plant (https://www.fab.com/listings/f3c45cd9-f301-4358-8699-b5bbf436c851). |
| elder | primary | **Megaplants: Elder** (Sambucus nigra) https://www.fab.com/listings/fe7a58b3-19fe-4249-b496-2214b32650b3 | Fab-Standard, $0 | 12 skeletal meshes, 8 foliage meshes, UE 5.8; a user reports "Black Elder" working as a static mesh in packaged 5.8 (crash thread). |
| elder | alternate | **Megascans: Elderberry** (plant) https://www.fab.com/listings/d72dd7bb-d514-4a14-8b4b-3333c99057fb | Fab-Standard, $0 | FBX/GLTF static plant. |
| dog rose | primary | **Megaplants: Beach Rose** (Rosa rugosa) https://www.fab.com/listings/d70e1181-f58e-4e8d-b22a-e6c1864770e8 | Fab-Standard, $0 | Proxy for Rosa canina (denser, larger leaves); 13 skeletal meshes, Nanite. |
| dog rose / bramble | alternate | **Megascans: Raspberry** (plant) https://www.fab.com/listings/e1ad5dd1-b79e-45cc-9a32-3fca0ac78b4c | Fab-Standard, $0 | Bramble-like thicket for wood edges. Megascans "Rose Hip" is paid ($4.99): excluded. |

Also free: Megascans "Dead Shrubs" (https://www.fab.com/listings/d8fbec55-5d03-41ba-bce3-1c7ee42512a4) for winter and
burnt edges; Project Nature "Temperate Vegetation: Wild Berries"
(https://www.fab.com/listings/b6307eff-f99f-48ae-bc3e-4d15b56eda11). **Gap: no species-specific free hawthorn
(Crataegus), blackthorn (Prunus spinosa) or dog rose (Rosa canina).**

### 2.11 Marsh: reeds and sedge

| Category | Pri | Asset | Licence | Notes |
|---|---|---|---|---|
| reed | primary | **Megaplants: Dry Common Reed** (Phragmites australis) https://www.fab.com/listings/00299cb6-8602-4736-8565-4b19b41f5686 | Fab-Standard, $0 | 13 skeletal meshes, 9 foliage meshes, UE 5.8 only; dry (autumn/winter) reed is exactly mid-October Alfold marsh. |
| reed | alternate | **mediterranean Vegetation: giant Reed**, Project Nature https://www.fab.com/listings/b4c4cc88-d228-4961-9c18-9aa9c0d1292c | Fab-Standard, $0 permanent | "30 unique meshes" in growth phases, multi-hierarchy Pivot Painter wind, photo-scanned; Arundo donax, a proxy. |
| bulrush/cattail | primary | **Megaplants: Common Bulrush** (Typha latifolia) https://www.fab.com/listings/764be2a2-72e0-4fb8-8108-532a4a3dc046 | Fab-Standard, $0 | 14 skeletal meshes, 8 foliage meshes, UE 5.8; published 30 Sep 2026. |
| bulrush/reed (light) | alternate | **Gen04-ReedsFree**, Immersive-Games https://www.fab.com/listings/1bfa1992-e3cc-49e1-843d-4b55c6e00edf | CC-BY-4.0, author Immersive-Games, $0 | 12 reed plants, 225 verts / 195 tris each, 512-1024 textures; shipped as a **Unity** package (URP material), so meshes and maps must be extracted. Cheap enough for mass scatter. |
| sedge | primary (proxy) | **Megascans: Ribbon Grass** (plant) https://www.fab.com/listings/6b7af6c8-e24b-4beb-a943-4ddb35d2db87 | Fab-Standard, $0 | Tall wetland grass (Phalaris) as a sedge stand-in. |
| sedge | alternate (proxy) | **Megascans: Bog Marshcress** (plant) https://www.fab.com/listings/3f4d341e-fd84-4ae1-91d6-e0727a857ff7 | Fab-Standard, $0 | Low wet-ground herb. |

**Gap: no free sedge (Carex)**; Megascans "Dense Flat - Sedge" and "Swamp Grass" are paid.

### 2.12 Ground clutter: grass and wildflowers

| Category | Pri | Asset | Licence | Notes |
|---|---|---|---|---|
| grass (+ ground plants + flowers) | primary | **temperate Vegetation: Foliage Collection**, Project Nature https://www.fab.com/listings/6a5ae8db-d80f-4b23-b276-87da390cfe56 | Fab-Standard, $0 permanent (first listed 30 Aug 2018) | 277 foliage clusters (grass library + ground foliage + meadow flowers), multi-hierarchy Pivot Painter wind, shader interaction, multi-layer landscape material. UE format; engine versions not read (made for UE4; check it opens in 5.8). |
| grass | alternate | **Megascans: Dry Grass** https://www.fab.com/listings/472c99c7-2d67-4190-9ad4-3bb04cfc80f3 (and Wild Grass https://www.fab.com/listings/50d9a417-73ed-4132-9421-6be3d4f7432e) | Fab-Standard, $0 | Dry grass suits October pasture and verges. |
| grass (redistributable) | alternate | **Poly Haven: Grass Medium 01** https://polyhaven.com/a/grass_medium_01 | CC0-1.0 | 1,606,633 polys, 8K, LODs included, geometry-nodes scatter (API /info/grass_medium_01); also grass_medium_02 and grass_bermuda_01. Heavy: bake to cards. |
| wildflowers | primary | **temperate Vegetation: Meadow Flowers**, Project Nature https://www.fab.com/listings/e648b1bd-11f6-4895-bd43-15e399c80297 | Fab-Standard, $0 permanent | 20 flower kinds in 96 clusters, Pivot Painter wind (part of the Foliage Collection). |
| wildflowers | alternate | **Megascans: Field Poppy** https://www.fab.com/listings/66cb2706-bc30-4f26-92ec-cad48723bd4a | Fab-Standard, $0 | Summer field-margin poppies (not October). Poly Haven CC0 dandelion_01, celandine_01, periwinkle_plant are other options. |
| weeds / nettles | primary | **Megaplants: Stinging Nettle** (Urtica dioica) https://www.fab.com/listings/7b5e8db0-e9fa-499b-a16f-3bb3e130e92e | Fab-Standard, $0 | 29 skeletal meshes, 22 foliage meshes. |
| weeds / nettles | alternate | **Poly Haven: Nettle Plant** https://polyhaven.com/a/nettle_plant | CC0-1.0 | 46,195 polys. Megascans Greater Burdock, Common Hemp Nettle, Curbside Weeds are free Fab alternatives. |

Also: Project Nature "interactive Open World Foliage" (https://www.fab.com/listings/eba98f8c-d99a-4f60-8495-5db0e62666ff):
"only one triangle per blade of grass", meant for "large open world fields with a high culling radius": the cheapest
grass for far ground. Project Nature "optimized Grass Library": 12 grass types, 110 clusters, Pivot Painter on every
blade (https://www.fab.com/listings/8b68642e-35f4-438e-82b4-799fc2228303).

### 2.13 Crops (maize, sunflower, wheat and stubble)

| Crop | Pri | Asset | Licence | Notes |
|---|---|---|---|---|
| maize | primary | **"Maize Corn Plant"** by gilles.schaeck https://sketchfab.com/models/5fd3b104d8104519b061469c365d4974 | CC-BY-4.0, author gilles.schaeck | 3,108 faces, PBR textures from Substance Painter, published 30 May 2021 (search listing, **not re-verified**). Light enough for field rows; green: recolour for October's dry standing maize. |
| maize | alternate | **"Corn_{X}_Field-I"** by XX-XX https://sketchfab.com/models/ec38723ae23a49e9add79331f03d1e38 | CC-BY-4.0, author XX-XX | 349,694 faces, tagged lowpoly and PBR (search listing, not re-verified); a field patch. Also "Corn_{X}_Field-II" (438,602 faces, https://sketchfab.com/models/c4803959c2ad4715966402d414ebef2a). |
| sunflower | primary | **"SUNFLOWER (DRY)"** by jimmygunawan https://sketchfab.com/models/f47126093065446eabed1edaff0afb89 | CC-BY-4.0, author jimmygunawan | 49,999 faces; dried stage (search listing, not re-verified). Matches late-season standing sunflower. |
| sunflower | alternate | **"Sunflower"** (photogrammetry) by chimaera_studio https://sketchfab.com/models/698cc100e4194c51b62275664fd9b166 | CC-BY-4.0, author chimaera_studio | 251,544 faces, green (summer missions). Also "Sunflower" by Polygonal Miniatures, CC-BY, 42,492 faces, on Fab https://www.fab.com/listings/5f1db324-5cd9-4156-b3fb-537a5e3ebb27 . |
| wheat | primary | **"Golden Fields: Wheat 3D for Game"** by karankumarkk123 https://sketchfab.com/models/5b7f34b35b9b483080c3e93907f5f52e | CC-BY-4.0, author karankumarkk123 | 40,000 faces, described as realistic stalks and grains (search listing, not re-verified). Summer missions only: by 15 October wheat is stubble or ploughed. |
| wheat | alternate | **Megascans: Wheat Grass** https://www.fab.com/listings/9070350f-dea8-40ec-ba57-0777a2c1fb24 | Fab-Standard, $0 | A grass (wheatgrass), not the crop; usable for verges and young winter wheat. Megascans "Wheat" (the crop) is paid $3.99: excluded. |

**Gap: stubble.** No free stubble model was found; stubble is best made procedurally (short straw cards in drill rows
along the parcel's furrow heading, which simcore already gives per 3 km square) over a stubble ground texture.
**Gap: realistic maize with LODs and a dry autumn variant.**

### 2.14 Epic's free samples (vegetation content)

- **Open World Demo Collection** (the Kite demo assets), Epic Games, $0, Fab "Standard License", UE 4.7-4.27 and 5.0-5.2,
  photogrammetry with "normal maps, LODs and collision meshes"; vegetation from colour-calibrated reference photography
  (https://www.fab.com/listings/3262ab8f-f64a-4124-8efd-82cb19df6249). Epic's 2015 release post listed 34 tree and palm
  species (88 with variations) and 24 smaller plants (https://www.unrealengine.com/blog/marketplace-content-release-april-2,
  via search summary; the page returned 403 to a direct read). Species list not verified; worth opening in the editor
  as a backup source of LOD'd broadleaf and conifer trees.
- **Project Titan** ($0, https://www.fab.com/listings/c05aac82-4c1a-4e42-96b3-be668dc40fca): an 8x8 km open-world
  sample from a 2024 art jam; its vegetation species were not found.
- **Electric Dreams Env** ($0, https://www.fab.com/listings/d79688f5-29be-4fb2-a650-2d4a813f5306): a jungle built from
  "a handful of Quixel assets" with PCG; useful only as a PCG reference, not for species.
- **Paragon: Agora and Monolith** ($0): sample map "includes foliage and extra props from Kite Demo"
  (https://www.fab.com/listings/6f401fb5-88b5-41b4-bf1b-62321414e1f0).
- **PVE built-in content**: PVE ships with the engine and offers pre-made sample graphs (1.1); a forum user reports the
  plugin's "PVE_Conifer_01" trees work in shipping 5.8.1 builds (crash thread, 1.1).
- City Sample, Valley of the Ancient, Cassini Sample: no relevant temperate vegetation (urban, desert, space).

---

## 3. Scale: hundreds of thousands of instances

- **Megascans Trees** (static, 7 LODs ending in impostors, simple wind as the cheap tier): fit SAM3D's existing HISM
  path directly; the best choice for wood masses and field trees at distance.
- **Megaplants / PVE**: designed for Nanite Foliage (instancing, GPU skeletal wind, voxel representation far away), but
  experimental, LOD-less, and with a 5.8 packaged-build crash being fixed; static-mesh export exists. Use for near
  ground once 5.8.2+ is verified in a packaged build.
- **PlantCatalog** (16k-1.8M faces) and **Poly Haven** (up to 17M polys): only with Nanite or after baking octahedral
  impostors; the intice184 poplars ship impostor textures already.
- **Ground clutter**: Project Nature's open-world grass (one triangle per blade) for distance, Foliage Collection near.
- Simcore keeps placement and line-of-sight heights; any registered tree must be normalised to the part frame (tree one
  unit tall, crown one unit across) through `scale` in `data/scenery/assets.json`.

## 4. Licence handling for the register

- Fab Standard (Megaplants, Megascans, Project Nature, Greenleaf, FreshCan, HKhalife, Epic samples): `licence:
  "Fab-Standard"`, `redistributable: false`, files under `unreal/SAM3D/Content/ThirdParty/`. Epic notes the 5 Megascans
  tree packs and 11 scenes are "for Unreal Engine only" (2 Jan 2025 forum post above).
- CC-BY (PlantCatalog, Georgeous/intice184, gilles.schaeck, jimmygunawan, chimaera_studio, karankumarkk123, XX-XX,
  Immersive-Games, Kraynikkk, UESReseach): `licence: "CC-BY-4.0"`, `redistributable: true`, author recorded and credited.
- CC0 (Poly Haven): `licence: "CC0-1.0"`; Poly Haven: "You can use our assets for any purpose", "You do not need to give
  credit", "You can redistribute them" (https://polyhaven.com/license).
- Excluded on licence: Sketchfab models labelled CC-BY-NC, CC-BY-SA, CC-BY-NC-SA or "Free Standard" (e.g. "Lombardy poplar"
  by 1-3D.com is CC-BY-SA; "Corn old" is CC-BY-NC; "Ear of Maize" is Sketchfab "Free Standard").

## 5. Gaps

1. No free realistic mature black locust (Robinia pseudoacacia); plan: PVE Grower or retexture the CC-BY Robinia hispida.
2. No free Austrian black pine (Pinus nigra): Scots pine stands in.
3. No free white willow (Salix alba): goat willow and rosemary willow stand in.
4. No free field maple (Acer campestre): Norway maple stands in.
5. No free Turkey oak (Q. cerris) or sessile oak (Q. petraea): English oak stands in.
6. No species-specific free hawthorn, blackthorn or dog rose: generic Greenleaf shrubs, spindle and Rosa rugosa stand in.
7. No free sedge (Carex): ribbon grass and marshcress stand in.
8. Crops are weak: maize and sunflower only as single plants of uncertain quality; no stubble; no dry-maize variant.
9. Hybrid plantation poplar (P. x canadensis) itself is not modelled; P. deltoides and P. nigra are its parents.
10. Megaplants are experimental (PVE, Nanite Foliage), 5.7 PVE assets do not open in 5.8, and a packaged-build crash
    (UE-386321) was still reported on 5.8.1; their behaviour at hundreds of thousands of instances is untested here.
11. Sketchfab rate-limited the API (HTTP 429) during this research: crop picks and several CC-BY entries are from the
    search listing only and must be re-read (licence, faces, textures) before registering.
12. Not determined: Project Nature packs' supported engine versions (made 2018 for UE4); Greenleaf shrub species;
    Open World Demo Collection's species list; Megaplants' exact season parameters.

## 6. Download hints (for the owner; nothing was downloaded)

- Fab: "Add to My Library" on the listing, then add to the project from the Fab plugin in the editor or the Epic
  launcher, into `Content/ThirdParty/<group>/`. Megaplants need the PVE plugin and "Nanite Foliage (Experimental)" on
  before import.
- Sketchfab: the model page's "Download 3D Model" (signed in), or `GET https://api.sketchfab.com/v3/models/<uid>/download`
  with an API token (glTF/GLB and the original upload).
- Poly Haven: `https://api.polyhaven.com/files/<id>` lists blend/gltf/fbx at 1k-8k.
# Vegetation gap fill: black locust, fallbacks, fruit trees, crops (UE 5.8)

Research for the terrain roadmap (R6, "a more realistic map"), second pass on vegetation. Date: 2026-11-03.
Researcher: Claude (subagent). It fills the gaps left by `vegetation_models.md` (same folder): no mature black locust
(Robinia pseudoacacia), the risk that Megaplants and the Procedural Vegetation Editor (PVE) stay experimental or crash
packaged builds, the Megascans Trees' engine versions, crops, and fruit and walnut trees. Every pick is free and
licensed CC0-1.0, CC-BY-4.0, Fab-Standard or Epic-Content-Only (`docs/scenery/ASSETS.md`,
`tools/check_assets.py`). **Nothing was downloaded; only pages, catalogue APIs and the locally installed engine's own
files were read.**

How the research was done:
- Fab: Fab's own catalogue JSON, read with `fetch` from a fab.com page in the browser pane (curl gets a Cloudflare 403):
  `https://www.fab.com/i/listings/search?q=...&is_free=1`, `https://www.fab.com/i/listings/<uid>` and
  `https://www.fab.com/i/listings/<uid>/asset-formats/<format>` (the last gives "techDetails" and engine versions).
- Sketchfab: the Data API (`https://api.sketchfab.com/v3/search`, `/v3/models/<uid>`) plus page reads in the browser
  pane when the API answered HTTP 429.
- Wikimedia Commons: the MediaWiki API (`categorymembers` + `imageinfo/extmetadata`) for licences.
- The local engine: `D:/UE_5.8` (Build.version: 5.8.3, changelist 58210709) and its plugins.
- **The session's web-search budget ran out** before this pass; nothing below comes from a search engine queried by
  other means. Questions that needed a search are listed in Gaps (section 9).

---

## 1. Decisions in brief

1. **Black locust**: no free, realistic, mature Robinia pseudoacacia model exists in Fab, Sketchfab, Poly Haven or
   ambientCG. Two routes, both kept:
   - now, committable: **Jagobo "Locust Tree Pack"** (CC-BY, three game-ready trees, 18.1k tris for all three), a
     honey-locust (Gleditsia) shape with pinnate leaf cards, re-barked with a **CC0 robinia bark photo** from Commons;
   - later, species-correct: **grow a robinia in PVE** (engine-shipped, every 5.8 install) with a leaf card made from
     **CC0 robinia leaf photos** (Commons, AnRo0002), exported as a static mesh.
2. **If Megaplants/PVE prove unusable** (experimental, packaged crash UE-386321): the static fallback is a CC-BY set
   that can be committed to the public repo (Blackridge "European Forest Environment", LOLIPOP birches, Tree Master
   packs) plus the engine's own PVE sample trees (on every machine, nothing to download). The Megascans Trees packs
   (beech, hornbeam, maple, hazel, alder: LODs and impostors) are **listed on Fab only up to UE 5.6**; they need adding
   to a 5.6 project and migrating, which is untested.
3. **Fruit and walnut**: CC-BY walnut (a scanned black walnut, 29k tris) and two CC-BY apple trees; no free species-
   correct Juglans regia, plum or apricot tree with LODs.
4. **Crops**: CC-BY maize plants (3.1k and 1.1k tris) to recolour for October, a CC-BY dried sunflower, the Megascans
   sunflower atlas (Fab-Standard) for cards, and the Megascans round hay bale. Stubble and dry standing maize stay
   procedural (our own).
5. **Licences and the public repo**: the repo is public, so only CC0, CC-BY and own work may be committed. Fab-Standard
   and Epic content live under the git-ignored `ThirdParty/` folders (section 4).

---

## 2. Black locust (Robinia pseudoacacia), `tree_acacia`

Why it matters: black locust covers about 22.5% of Hungary's forests (`references.md`, same folder), and it lines the
Alfold's roads, sand dunes and field edges. The previous pass found only a sucker plant (Megascans) and a Grove-made
Robinia hispida with no textures.

### 2.1 Searched and not found

- Fab, free, for "black locust", "robinia", "acacia tree", "locust tree", "false acacia" (catalogue API,
  `https://www.fab.com/i/listings/search?q=<term>&is_free=1`): the only Robinia is the Megascans "Black Locust" plant
  (https://www.fab.com/listings/63577d63-cc5e-41a8-8038-614318314eb8).
- PlantCatalog (e-on software's free CC-BY Sketchfab models): a search for "Realistic HD black locust" returned nothing
  (`https://api.sketchfab.com/v3/search?type=models&q=Realistic%20HD%20black%20locust`), and its "PlantCatalog free
  models" collection (21 models, `https://api.sketchfab.com/v3/collections?user=PlantCatalog`, uid
  51dc1d2c505f47b49576f31905a721eb) has no robinia, walnut, apple or crop.
- Poly Haven models: no temperate broadleaf tree under 1.7M polys (island_tree_01/02/03, 1.7-4.7M polys;
  https://polyhaven.com/a/island_tree_01), no robinia.

### 2.2 Committable proxy now: Jagobo "Locust Tree Pack" (CC-BY)

- Page: https://sketchfab.com/3d-models/locust-tree-pack-7784d6ccdf314618aa3eb390808521c6 (read 2026-11-03).
- Licence: CC Attribution (CC BY 4.0), downloadable. Published 17 Oct 2023.
- Specs: 18.1k triangles and 18.9k vertices for the whole pack; the author: "Three semi-low poly tree variations -
  largest model containing 6.5k vertices and 4.5k tris"; a 4x4 foliage atlas at 4K; bark colour, normal, roughness and
  displacement maps; FBX and OBJ included (same page).
- Species: tagged "honeylocust" (same page), so it is honey locust (Gleditsia triacanthos), not black locust. Both have
  pinnate leaves and an open crown; the screenshot shows young trees with a pale, smooth trunk, while a mature robinia
  has deeply furrowed grey bark. Re-bark it with a CC0 robinia bark photo (2.4).
- Why: the only free, game-ready (thousands of tris, not hundreds of thousands) locust-like tree found; CC-BY, so it can
  be committed under `art/` with the author credited (`docs/scenery/ASSETS.md`).

### 2.3 Species-correct later: grow a robinia in PVE (engine content)

- PVE ships with every UE 5.8 install as an experimental plugin: `D:/UE_5.8/Engine/Plugins/Experimental/
  ProceduralVegetationEditor/ProceduralVegetationEditor.uplugin` has `"IsExperimentalVersion": true` and depends on the
  Dataflow, GeometryScripting, PCG and DynamicWind plugins; its runtime module is allowed on Win64, Linux and Mac.
- Its sample content (same plugin, `Content/SampleAssets/`): complete PVE assets `StarterContent/DeciduousTree_01`,
  `ConiferTree_01`, `Deciduous_Shrub_01`, `Plant_01` (graph, skeleton, materials, textures), ten presets
  (`StarterContent/Presets/PVE_Preset_Leaf_Tree_01..04`, `Pine_01`, `Sapling_01`, `Shrub_01`, `Spruce_01/02`,
  `Tropical_01`), and growth data for four European species as JSON (`Tree_European_Beech_01/Instances/Beech_01..04.json`,
  `Tree_Common_Hazel_01` (5), `Tree_European_QuakingAspen_01` (4), `Tree_Norway_Maple_01` (4)). Each JSON holds
  `globalAttributes` (32 growth parameters, among them phyllotaxy, phyllotaxyLeaf, phototropism, gravitationalForce,
  axialElongation, branchingCondition, trunkGrowth, leafGrowth, abscissionSenescense), `points` and `primitives`, so
  they are skeletons with growth settings, not finished meshes.
- UE 5.8 release notes: "The Procedural Vegetation Editor is an experimental plugin to create and edit vegetation
  meshes"; 5.8 adds growing without imported recipes, seed generation, skeleton extraction from 3D meshes or 2D images,
  foliage grafting, carving and trimming, and a trunk material compositor
  (https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes).
- PVE doc: a Grower node grows vegetation from scratch and Extract From Mesh takes growth data from any static mesh
  (https://dev.epicgames.com/documentation/unreal-engine/procedural-vegetation-editor-pve-in-unreal-engine).
- Export (`Source/ProceduralVegetation/Public/Params/PVExportParams.h` in the plugin): `EPVExportMeshType { StaticMesh,
  SkeletalMesh }`, skeletal by default (the header notes static mesh has no wind animation), `bCreateNaniteFoliage =
  true` by default, `NaniteShapePreservation = Voxelize`, collision options, **no LOD generation option**. So a
  static export for our HISM path needs LODs from the Static Mesh Editor's reduction or an impostor from the engine's
  `ImpostorBaker` plugin (`D:/UE_5.8/Engine/Plugins/Experimental/ImpostorBaker`, present on this install).
- Recipe: start from `PVE_Preset_Leaf_Tree_0x` or the Grower; set compound pinnate leaves (phyllotaxyLeaf), an open,
  irregular crown, a straight then forking trunk, about 15-25 m; leaf card from a CC0 photo (2.4); bark from a CC0
  photo; export as a static mesh with Nanite off for HISM and a second Nanite export for near ground.
- Licence: anything built from the engine's sample presets or materials is engine content (Epic-Content-Only):
  usable in our Unreal project, not committable to the public repo. A tree grown from our own parameters with only CC0
  textures may count as our own work; **owner's call** (section 4). The safe default is to keep the exported mesh under
  `Content/ThirdParty/` and commit the editor script and the parameter values that regenerate it.

### 2.4 CC0 robinia photos for our own leaf and bark textures (Commons)

- `Category:Robinia pseudoacacia (leaves)` (https://commons.wikimedia.org/wiki/Category:Robinia_pseudoacacia_(leaves)):
  72 files (MediaWiki API, 2026-11-03), of which 18 are **CC0** by AnRo0002 and one public domain; the rest are mostly
  CC BY-SA (excluded). CC0 examples:
  - https://commons.wikimedia.org/wiki/File:20171018Robinia_pseudoacacia1.jpg (2500x1875; 18 October, autumn),
    `...2.jpg`, `...3.jpg` (same day);
  - https://commons.wikimedia.org/wiki/File:20160902Robinia_pseudoacacia2.jpg (3000x2250, summer),
    https://commons.wikimedia.org/wiki/File:20180609Robinia_pseudoacacia1.jpg (3000x2250, June);
  - https://commons.wikimedia.org/wiki/File:20200923Robinia_pseudoacacia1.jpg (3500x2625, late September);
  - https://commons.wikimedia.org/wiki/File:Leaves_of_Robinia_in_january.jpg (public domain, winter).
  - The subcategory "Robinia pseudoacacia (autumn leaves)" has 11 files, all CC BY-SA: excluded.
- `Category:Robinia pseudoacacia (bark)` (https://commons.wikimedia.org/wiki/Category:Robinia_pseudoacacia_(bark)):
  65 files, of which at least 21 are CC0 or public domain, e.g.
  https://commons.wikimedia.org/wiki/File:20140211Robinia_pseudoacacia2.jpg (2736x3648, CC0, AnRo0002),
  https://commons.wikimedia.org/wiki/File:Black_locust_(40265110123).jpg (CC0, Dan Keck),
  https://commons.wikimedia.org/wiki/File:Friedhof_Halle-B%C3%BCschdorf_(6a)_Robinie.jpg (CC0, Catatine).
- Use: cut a pinnate leaf card (opacity, colour; normal from the photo's luminance) and a tiling furrowed bark. CC0, so
  the derived textures are committable as our own (`Own` in `data/scenery/assets.json`, with the source photos noted).
- ambientCG has CC0 leaf atlases (LeafSet001-030, e.g. https://ambientcg.com/view?id=LeafSet030, autumn/dried) whose
  species are not stated: a generic small-leaflet atlas can stand in until our robinia card exists.

### 2.5 Other robinia leads (rejected or weak)

- Megascans "Black Locust" plant (https://www.fab.com/listings/63577d63-cc5e-41a8-8038-614318314eb8): Personal and
  Professional both $0, Fab-Standard; FBX 1,805 to 742,312 vertices across its LODs, glTF 214,930 to 1,683,391; 8K maps
  (basecolor, opacity, translucency and more) (catalogue API). A sucker plant, but its scanned robinia leaves and twigs
  can be a leaf material for a PVE-grown tree **inside Unreal only** (Fab-Standard).
- "Black Locust (Robinia pseudoacacia)", uploader rufusrockwell, CC-BY, 3.4k tris, "Black Locust plant": a small plant
  (https://sketchfab.com/3d-models/black-locust-robinia-pseudoacacia-7373f939f20e485a96f1bf7e1cf493ac).
- "honey locust" by sananga, CC-BY, 57,329 faces, described as a tree limb structure
  (https://sketchfab.com/3d-models/b42beff7506349f8901de2d148e30b0f): not a whole tree.
- UESReseach "Sophora japonica", CC-BY, 1.67M faces, untextured, made with The Grove
  (https://sketchfab.com/3d-models/69bd5d3893d64602ba56b1d3141143f1): too heavy, no textures.

---

## 3. If Megaplants and PVE fail: what still works on 5.8

### 3.1 The packaged-build crash, UE-386321

- Thread: https://forums.unrealengine.com/t/procedural-vegetation-nanite-foliage-meshes-crash-packaged-project/2730886
  (read 2026-11-03). Packaged builds crash on load of PVE meshes, Megaplants included: an assertion at
  `RawIndexBuffer.h:198` reached from `FPVLightDetection::BuildLeafMeshGeometry` (the PVE runtime module; the class is
  in the local plugin, `Source/ProceduralVegetation/Private/Implementations/PVLightDetection.h`).
- Epic staff marked the issue Closed/Fixed with target 5.8.2 (7 Aug 2026, same thread); one user worked round it by
  copying the dev-5.8 PVE plugin into the project.
- What users report working: the plugin's own `PVE_Conifer_01` in a Shipping build on 5.8.1; Black Elder, Greasewood
  and Goat Willow only when exported as static meshes; Megaplants as skeletal meshes crash; Giant Bamboo crashes even
  as a static mesh; a 5.7 project packaged in 5.8 until its Megaplants were replaced by the 5.8 downloads (same thread).
- **This machine has 5.8.3** (`D:/UE_5.8/Engine/Build/Build.version`: PatchVersion 3), later than the fix's target,
  but the 5.8.2 and 5.8.3 hotfix notes were not found (search budget gone), so the fix is **not confirmed** here. A
  packaged Shipping test is required before any PVE content ships (section 6).

### 3.2 Engine versions on Fab (catalogue API, `asset-formats/unreal-engine`)

| Listing | Engine versions listed | Tech details |
|---|---|---|
| Megascans Trees: European Beech https://www.fab.com/listings/d11cc01d-9422-41b7-950f-416c9ce79caf | 5.1-5.6 | 17 meshes, 9 LODs on full trees, 36 impostor and 34 Pivot Painter textures |
| Megascans Trees: European Hornbeam https://www.fab.com/listings/c6f917b6-ffcb-4b86-9d9f-5274ba7f6a8e | 4.25-4.27, 5.0-5.6 | 27 meshes, 14 LODs |
| Megascans Trees: Black Alder https://www.fab.com/listings/9de7ce19-5813-42d2-a5f0-5e6447006f72 | UE4, 5.0-5.6 | |
| Megascans Trees: Norway Maple https://www.fab.com/listings/1f65d477-f44e-4ccb-95c1-788c99e33b56 | 5.1-5.6 | 7 LODs |
| Megascans Trees: Common Hazel https://www.fab.com/listings/81bc7ba6-4686-4f94-9d2b-83eb1fdc4079 | 5.1-5.6 | |
| Megaplants: English Oak https://www.fab.com/listings/83642c38-7661-4df1-8629-0422e1898d26 | 5.7, 5.8 | LODs: No; Nanite: Yes; 14 skeletal, 18 foliage meshes |
| Megaplants: Black Poplar https://www.fab.com/listings/6b22075c-53b6-4973-a6a5-9f4ccd6a72a8 | 5.8 only | |
| Megaplants: Baltic Pine https://www.fab.com/listings/a2b04e81-5075-479f-a9d2-4940022f330a | 5.7, 5.8 | |
| Megaplants: Yoshino Cherry https://www.fab.com/listings/97f6cdb0-810d-470d-9e3a-3c9f059e6763 | 5.7, 5.8 | |
| Open World Demo Collection https://www.fab.com/listings/3262ab8f-f64a-4124-8efd-82cb19df6249 | up to 5.2 | 42 meshes with LODs, 15 foliage, 6 trees |
| Greenleaf "Free Shrubs Pack" https://www.fab.com/listings/7ca465ab-fb9c-4d6b-bddb-82c20f604657 | 5.4-5.8 | "LODs: None (Nanite fallback)" |
| Project Nature "temperate Vegetation: Foliage Collection" https://www.fab.com/listings/6a5ae8db-d80f-4b23-b276-87da390cfe56 | 4.19-5.4 | |
| Medieval Village Megascans Sample https://www.fab.com/listings/2e11a225-a6ea-4781-a3e1-fe975b461894 | 5.3-5.8 | |

Meaning: none of the five Megascans Trees packs lists 5.7 or 5.8. The Fab plugin in a 5.8 editor may refuse them or
offer the 5.6 build; the known route is to add them to a 5.6 project (installed? only 5.7 and 5.8 are on `D:/`) and
migrate the folder. Static meshes with LODs, impostors and Pivot Painter wind normally carry forward, but this is
**untested** (section 6).

### 3.3 Fallback tiers

1. **Engine-shipped PVE samples** (2.3): `DeciduousTree_01`, `ConiferTree_01`, `Deciduous_Shrub_01`, `Plant_01` and the
   presets are on every UE 5.8 install, so fresh clones and the play copy (`C:\dev\SAM3D-play`) have them with no Fab
   step. `PVE_Conifer_01` is the one reported to package on 5.8.1 (3.1). Export as static meshes for HISM.
2. **CC-BY, committable** (can live in git, so the play copy gets them by pull):
   - **"European Forest Environment"**, Blackridge, https://www.fab.com/listings/c849e5f2-fb4a-4c22-ae57-36cb904a8a7c:
     "Creative Commons Attribution (CC BY 4.0)", $0; UE 5.0-5.8; 29 meshes; "LODs: Yes, Number of LODs: 1"; 6
     materials; 16 textures 540x360 to 4096; tags oak, birch, pine, PCG; published 12 Apr 2026, updated 5 Aug 2026
     (catalogue API). Mesh-level species and tri counts not stated.
   - **"Five Birch trees pack (lowpoly, LODs)"**, LOLIPOP, https://www.fab.com/listings/b993c6a4-09bc-4cb4-a0a3-0cfa68299d29:
     CC-BY, FBX; silver birch; LOD1 about 9.5k/7.6k/14.6k/20.6k/14.1k polys, LOD2 about 60%, LOD3 about 30%; two 4K
     texture sets (bark; leaf atlas with translucency) (listing description).
   - Tree Master **"Desktop Trees Pack 60 (PC/Console)"**, https://www.fab.com/listings/43b47da3-b552-4b6f-9deb-d638c26d3021:
     CC-BY; UE 4.26-5.8; 10 meshes of 6,000-7,500 vertices; species not stated. **"Mobile Trees Pack 44"**,
     https://www.fab.com/listings/ab02fda7-47fe-45bc-8d82-ea9c0570f324: CC-BY; UE 4.26-5.8 + FBX; 10 meshes of
     2,000-2,500 vertices; 2 materials, 2K. Also "Mobile Trees Model",
     https://www.fab.com/listings/87f57389-c909-415d-8533-863c221a1530 (CC-BY).
   - Next Spring "Pine Tree Pack (Imposter)", https://www.fab.com/listings/1be6afff-f5db-4a8b-9fa3-ba554b9fe3f6:
     CC-BY, 15 billboards, UE 5.3-5.8 (far plantations). Next Spring "HighPoly Tree Model",
     https://www.fab.com/listings/2f1f43fe-fd59-42ff-967d-c8eb1a0785e7: CC-BY, one Nanite tree of about 500k triangles,
     no LODs, 5.3-5.8 (near only).
   - Immersive-Games "Trees Gen02 Volume 01", https://www.fab.com/listings/b7281477-1014-489d-b063-b53ddd5a48bd: CC-BY;
     UE 4.26-5.5 and 5.8; 6 acacias (African umbrella form, wrong), **5 old leafless trees (about 4.1k tris)**, 5 pines
     (about 2.7k tris); no LODs. The leafless trees serve dead and winter trees.
   - SachinGames "Quercus Rubra Tree", https://www.fab.com/listings/3ccde119-1eef-4e5a-8962-877f5873a472: CC-BY, 288k
     vertices, 6K textures; red oak (wrong species), near only.
   - Also CC-BY billboards: sixonetwo "LOD/Billboard Summer Trees Pack",
     https://www.fab.com/listings/284c06eb-9633-4690-ab8e-ae5af1a4c3ec; danpetro "Tree",
     https://www.fab.com/listings/28820c60-aebe-441d-afbe-2113741b451c (low poly).
3. **Megascans Trees via a 5.6 project** (Fab-Standard, `ThirdParty/`): beech, hornbeam, maple, hazel, alder with LODs
   and impostors: the best static quality if the migration works.
4. **Megaplants as static meshes** (Fab-Standard, `ThirdParty/`): only after the packaged test passes on 5.8.3.

---

## 4. Licences, the public repo and the play copy

- Fab EULA (https://www.fab.com/eula, read in the browser pane; WebFetch got 403): under the Standard License you may
  "Share the asset (directly, via a private repository or in the Project) with your collaborators"; "usage is not
  limited to Unreal Engine"; you may not redistribute the asset on a standalone basis; section 5a allows sharing with
  collaborators "in a private online repository". For CC-BY listings: "The usage is governed by the terms of the
  Creative Commons license."
- SAM3D's repo is public on GitHub (ND501st/SAM3D) and uses Git LFS, so a Fab-Standard or engine asset committed there
  would be standalone redistribution. The repo already git-ignores `unreal/**/Content/ThirdParty/` and
  `art/thirdparty/`, and `tools/check_assets.py` requires non-redistributable entries to live under
  `/Game/ThirdParty/` and CC-BY entries to name an author.
- Consequences:
  - CC0, CC-BY (including CC-BY listings on Fab) and our own work: commit under `art/` (LFS), credit CC-BY authors.
  - Fab-Standard and Epic-Content-Only: `Content/ThirdParty/` on each machine. The play copy at `C:\dev\SAM3D-play`
    (a separate clone) will not have them: either a junction or sync of `ThirdParty/` from the main checkout, or an
    editor script that regenerates the PVE trees from engine content on each machine. The game must still run with
    `ThirdParty/` empty (procedural parts, `data/scenery/assets.json` all null today).

---

## 5. Fruit and walnut trees (orchards, farmsteads, village gardens)

| Part | Pri | Asset | Licence | Specs and notes |
|---|---|---|---|---|
| walnut | primary | **"Lowpoly Tree Scan Pack 3: Black Walnut+Dead Tree"**, sixonetwo, https://sketchfab.com/3d-models/lowpoly-tree-scan-pack-3-black-walnutdead-tree-6487231452e540e19cf8c572c8c07715 | CC-BY-4.0 (Sketchfab) | 29.1k tris, 36k verts; "Scanned in Polycam and tree built in blender, 2k color, normal, AO, 256px leaf tex"; tagged NoAI; published 9 Oct 2023. Juglans nigra, close in habit to the Hungarian village walnut (J. regia). On Fab the same pack is $0 Personal but AUD 2.84 Professional (https://www.fab.com/listings/3d514e60-45f6-40a5-b31a-325d2bacbd22), so take the Sketchfab CC-BY copy. |
| walnut | alternate | **"Black Walnut Tree"**, gwangju_inmunstory, https://sketchfab.com/3d-models/black-walnut-tree-c504c5ba66f54d3ea30c0e9bb57a8ab5 | CC-BY-4.0 | 69.1k tris; "A black walnut tree in Gwangju". Heavier; check its leaves. |
| apple | primary | **"Apple Tree"**, kelvladmail, https://sketchfab.com/3d-models/apple-tree-4596ba44e5e04578a5da022a33f40f3d | CC-BY-4.0 | 44.7k tris; no description; 2020. Quality to be checked on download. |
| apple | alternate | **"Apple Tree"**, rhcreations, https://sketchfab.com/3d-models/apple-tree-e5e1208e7e734f88b02c5f45c70f8db1 | CC-BY-4.0 | 67.6k tris, rigged, textures up to 1024; 2019. |
| cherry (orchard proxy) | alternate | **Megaplants: Yoshino Cherry**, https://www.fab.com/listings/97f6cdb0-810d-470d-9e3a-3c9f059e6763 | Fab-Standard, $0 | Ornamental Prunus, UE 5.7/5.8, PVE (same risks as 3.1). Stands in for sour cherry and plum rows. |
| cherry (winter) | alternate | **"Cherry tree"**, matousekfoto, https://sketchfab.com/3d-models/232e1f60165342afb09a40230f7a469d | CC-BY-4.0 on Sketchfab (on Fab $0 Personal, $7.12 Professional: https://www.fab.com/listings/e14295e5-a33a-41cf-ac37-f37c9e159437) | Photogrammetry trunk and branches in winter: leafless orchard tree. |

Rejected: CMBC "Farm Apple Tree" (https://sketchfab.com/3d-models/9b83669d10ac40f99a1925df4d28ede9) is tagged ai,
madewithai, copilot3d (AI-generated); Avramania "Apple Tree - Low Poly Medieval"
(https://sketchfab.com/3d-models/0fa93c551d8449808cc1f85c4238bf93) is stylised (1,044 faces); liviujurca "Old pear
tree" (https://sketchfab.com/3d-models/af34c0e3109744fd9afcf86db9c611cf) is 817.6k tris of photogrammetry (likely the
trunk only); local.yany "Cherry Tree" (https://sketchfab.com/3d-models/2e8111f87c2b4c06826406accccfbdc2) is 1.38-3.1M
faces; Zahaoha "Apricot 01 with 5 LODs" (https://www.fab.com/listings/d44d7fc7-3478-46a1-ad81-751e94b37b43) is the
fruit, not the tree. PlantCatalog searches ("Realistic HD walnut", "Realistic HD apple tree") found no such species.

---

## 6. Crops for 15 October 1985 (and summer missions)

| Crop | Pri | Asset | Licence | Specs and notes |
|---|---|---|---|---|
| maize | primary | **"Maize Corn Plant"**, gilles.schaeck, https://sketchfab.com/3d-models/maize-corn-plant-5fd3b104d8104519b061469c365d4974 | CC-BY-4.0 | 3.1k tris, 2.1k verts; "A simple Maize plant"; PBR from Substance Painter; 10,686 downloads; 30 May 2021 (page read 2026-11-03). Green: recolour the albedo to straw for October's standing dry maize. |
| maize (far rows) | alternate | **"Corn stalk"**, arigura, https://sketchfab.com/3d-models/a3c4fa7b1b4d4d28b5d480ee7f5c29c6 | CC-BY-4.0 | 1,104 faces, 674 verts, 1 texture; "Low-poly model one corn stalk", FBX (API, 2026-11-03). Green with tassel (page screenshot): retint. Cheap for dense rows. |
| maize | alternate | "Maiz Low Poly", Henry_Frean, https://sketchfab.com/3d-models/94b1a33c64fa40998273fc2bbcb06569 | CC-BY-4.0 | 10,328 faces, 3 materials, **no textures** (API): shape only. |
| sunflower | primary | **"SUNFLOWER (DRY)"**, jimmygunawan, https://sketchfab.com/3d-models/sunflower-dry-f47126093065446eabed1edaff0afb89 | CC-BY-4.0 | 50k tris, 25k verts; 22 Jul 2021. Dried heads: October's unharvested sunflower. Decimate for rows. |
| sunflower (cards) | alternate | **Megascans "Sunflower"** texture atlas, https://www.fab.com/listings/604bc6d3-5be0-41ef-b0fc-c804d3ca09a8 | Fab-Standard, $0 Personal and Professional | Atlas tagged Young, Perennial, Flower, Fallen, Dying, Old; 10,503 px/m; 0.39x0.39 m scan; opacity and translucency maps (catalogue API). Dying heads and leaves for cards; in Unreal only. The 3D Megascans Sunflower plant is paid ($3.99): excluded. |
| maize, wheat (stylised) | alternate | LOLIPOP "Farm plants models (mobile game-ready, lowpoly)", https://www.fab.com/listings/cd55e9c9-3d7f-43c9-bef5-a8e78a983dbb | CC-BY-4.0 | 8 crops incl. corn and wheat, about 300-600 tris per plant, 2K atlas: mobile quality, far only. |
| sunflower | alternate | Polygonal Miniatures "Sunflower", https://www.fab.com/listings/5f1db324-5cd9-4156-b3fb-537a5e3ebb27 | CC-BY-4.0 | OBJ (already noted in `vegetation_models.md`). |
| hay bale | primary | **Megascans "Round Hay Bale"**, https://www.fab.com/listings/fc0388e8-5443-4e54-b618-65790804779b | Fab-Standard, $0 | 1.32x1.6x1.48 m (catalogue API). Whether round bales fit Hungarian state farms in 1985 (against square bales and straw stacks) is not checked: compare with period reference photos first. |
| hay bale | alternate | ablaradi "Wrapped Hay Bale Stack", https://www.fab.com/listings/ee27354b-3aef-444e-80ee-bc86f33b22b4 | CC-BY-4.0 | Plastic-wrapped silage bales may be anachronistic for Hungary in 1985 (not checked): use unwrapped, or not at all. |

Also searched (Sketchfab CC-BY/CC0, 2026-11-03): "dry corn" (angelinart "Dried Corn" is a cob,
https://sketchfab.com/3d-models/72afa7a3213b45c2a920b205824d1438), "cornfield" (XX-XX "Corn_{X}_Field-I/II",
350-440k faces, already in `vegetation_models.md`; Metazeon's farmland scans are 1M-face photogrammetry of Asian
fields), "wheat field" (low-poly toys), "stubble" and "corn stalk": **no stubble, no dry standing maize**. ambientCG has
no corn, maize, hay, stubble, sunflower or wheat assets (its search, earlier in this session).

Own work, procedural: stubble as short straw cards in drill rows along each parcel's furrow heading (simcore gives it
per 3 km square, `data/scenery/map_dressing.json`), over the CC0 crop-ground textures in `pbr_textures.md`
(ambientCG Ground072/Ground103); dry maize by retinting the CC-BY maize and thinning its leaves.

---

## 7. Test plan before anything is registered

1. **Packaged PVE test on 5.8.3** (decides 3.1): a blank 5.8 project with PVE and Nanite Foliage enabled; place
   `PVE_Conifer_01`, `DeciduousTree_01` and one Megaplants tree, each exported (a) as Nanite foliage skeletal mesh and
   (b) as a static mesh; package Shipping for Win64; launch; record crash or not for each.
2. **Megascans Trees migration**: if a 5.6 engine can be installed, add European Beech and Hornbeam to a 5.6 project,
   migrate to a 5.8 copy of SAM3D, check LOD counts (9 and 14), impostors, Pivot Painter wind and the season shader.
3. **Scale test**: 200k HISM instances per candidate on the Kesmark range (simcore places them; `FSimBasicModels`
   draws), frame time and memory, static LOD/impostor path against Nanite Foliage.
4. **CC-BY picks**: with the owner's approval to download, check tri counts, textures, pivot and scale (normalise to the
   part frame: one unit tall, crown one unit across, via `scale` in `data/scenery/assets.json`), and record author,
   licence and URL; run `python tools/check_assets.py`.
5. **Robinia**: grow two or three robinia variants in PVE from a leaf-tree preset with the CC0 leaf and bark textures,
   export static + Nanite, bake an impostor with `ImpostorBaker`, compare against reference photos from the Alfold.

---

## 8. Draft register entries (`data/scenery/assets.json`, not written)

| Part | Asset | licence | redistributable | Location |
|---|---|---|---|---|
| tree_acacia | Jagobo Locust Tree Pack, re-barked | CC-BY-4.0 (author Jagobo) | true | committed under `art/` (LFS) |
| tree_acacia (later) | PVE-grown robinia | Epic-Content-Only (or Own, owner's call) | false | `Content/ThirdParty/PVE/` |
| tree_oak / tree_pine (fallback) | Blackridge European Forest Environment | CC-BY-4.0 (Blackridge) | true | committed |
| birch (fallback) | LOLIPOP Five Birch | CC-BY-4.0 (LOLIPOP) | true | committed |
| fruit tree | sixonetwo black walnut; kelvladmail apple | CC-BY-4.0 | true | committed |
| crops | gilles.schaeck maize; jimmygunawan dry sunflower | CC-BY-4.0 | true | committed |
| hay bale | Megascans Round Hay Bale | Fab-Standard | false | `Content/ThirdParty/` |

`assets.json` has parts tree_oak, tree_poplar, tree_pine, tree_acacia, canopy_mass, house, barn, flats, industrial; fruit
trees, crops and hay bales would need new parts in simcore's dressing (`map_dressing.hpp`) first.

---

## 9. Gaps

1. No free, realistic, mature Robinia pseudoacacia anywhere searched; the honey-locust proxy and a PVE-grown tree are
   the answers, neither verified in the editor.
2. No free stubble or dry standing maize model; both must be our own (procedural cards, retinted maize).
3. No free species-correct Persian walnut (Juglans regia), plum, sour cherry or apricot tree with LODs; the CC-BY
   apple trees' quality is unknown until downloaded.
4. Megascans Trees list engine versions only up to 5.6; whether the Fab plugin offers them to a 5.8 project, and
   whether a 5.6-to-5.8 migration keeps LODs, impostors and wind, is untested. UE 5.6 is not installed on this machine.
5. UE-386321's fix (target 5.8.2) is not confirmed on 5.8.3: the 5.8.2/5.8.3 hotfix notes were not found, and no
   packaged test has been run.
6. PVE's export has no LOD option; LODs and impostors for a static export are an extra step, untested.
7. Whether a PVE tree grown from our own parameters and CC0 textures counts as our own work or as engine content is an
   owner decision.
8. Nothing was opened in the editor; the PVE species JSONs (beech, hazel, aspen, maple) appear to be skeletons and
   growth settings without leaf meshes, unverified.
9. The Blackridge pack's per-mesh species and tri counts, and Tree Master packs' species, are not stated in their
   listings.
10. **The web-search budget ran out**; hotfix notes, Hungarian orchard and farm references for 1985 (bale types,
    orchard spacing) and any non-catalogue model sites (e.g. Blender community libraries) were not searched.
11. Sketchfab rate-limited the API (HTTP 429) for part of this pass; some details were read from model pages in the
    browser pane instead.

---

## 10. Sources read

- Fab catalogue API and listing pages (each URL above), Fab EULA https://www.fab.com/eula.
- Sketchfab Data API and model pages (each URL above).
- Wikimedia Commons categories https://commons.wikimedia.org/wiki/Category:Robinia_pseudoacacia_(leaves) and
  https://commons.wikimedia.org/wiki/Category:Robinia_pseudoacacia_(bark).
- UE 5.8 release notes https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes;
  PVE doc https://dev.epicgames.com/documentation/unreal-engine/procedural-vegetation-editor-pve-in-unreal-engine;
  crash thread https://forums.unrealengine.com/t/procedural-vegetation-nanite-foliage-meshes-crash-packaged-project/2730886.
- Local engine files under `D:/UE_5.8/Engine/` (Build.version; plugins ProceduralVegetationEditor, DynamicWind,
  ImpostorBaker).
- Previous pass: `vegetation_models.md`, `tech_geometry.md`, `pbr_textures.md`, `references.md` (same folder).
