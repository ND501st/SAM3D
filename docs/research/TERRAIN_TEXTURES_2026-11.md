# Free PBR textures for Pannonia 1985: ground and building surfaces

Research notes, 3 Nov 2026. Scope: one primary and one alternate free PBR texture set for every ground layer of the
Pannonia map (simcore's land-cover classes and crops) and every building or prop surface, for UE 5.8.
Nothing was downloaded: only catalogue metadata (JSON from the sources' public APIs) and preview images on the
sources' own pages were read.

## 1. Method

- **Poly Haven:** the whole texture catalogue was read from `https://api.polyhaven.com/assets?t=textures`
  (869 textures returned on 3 Nov 2026), filtered by category and tags, then each candidate's maps and resolutions
  were read from `https://api.polyhaven.com/files/<id>` and its real-world size, authors and description from the
  same catalogue (`dimensions` in mm). The diffuse previews (`https://cdn.polyhaven.com/asset_img/map_previews/<id>/<id>_diff_1k.jpg`)
  were looked at for the doubtful ones (roof tiles, soils, grass, concrete, plaster).
- **ambientCG:** the whole material catalogue was read from
  `https://ambientcg.com/api/v2/full_json?type=Material&limit=2500&include=tagData,displayData,dimensionsData`
  (2038 materials), then the chosen ones from `https://ambientcg.com/api/v3/assets?id=<ids>&include=maps,downloads,dimensions,tags`
  (API documented at https://docs.ambientcg.com/api/v3/assets/). Flat colour previews
  (`https://f003.backblazeb2.com/file/ambientCG-Web/media/surface-preview/<id>/<id>_SQ_Color.jpg`) were looked at.
- **Fab (Megascans free tier):** searched in a browser on `https://www.fab.com/search?q=<term>&is_free=1&listing_types=material`
  (fab.com's JSON endpoint answers scripts with a Cloudflare challenge, so it was not used); each listing page was read for
  price, licence, scan area, texel density and maps.
- **TextureCan, cgbookcase, 3dtextures.me, ShareTextures:** licence pages and category/search pages read.
- The web-search budget for this session ran out before the end; the last searches planned (ambientCG zip contents,
  cgbookcase and 3dtextures.me per-texture checks) were not made. What that leaves open is listed in section 6.

## 2. Licences (what may ship)

| Source | Licence | Redistributable (files in git) | Evidence |
|---|---|---|---|
| Poly Haven | CC0 ("You can use our assets for any purpose", no credit needed, may redistribute) | yes | https://polyhaven.com/license |
| Poly Haven API | Free for any purpose; a "Powered by Poly Haven" credit only when a product surfaces content through the *live* API; scripts must send an identifying User-Agent/Referer (ToS 2.4); no attribution for downloaded CC0 files (2.5) | n/a | https://github.com/Poly-Haven/Public-API , https://github.com/Poly-Haven/Public-API/blob/master/ToS.md |
| ambientCG | CC0 1.0; raw files may be included in a project such as a video game | yes | https://docs.ambientcg.com/license/ ; the asset page states "Creative Commons CC0 license" (https://ambientcg.com/view?id=Ground037) |
| TextureCan | CC0 1.0 ("All the PBR textures ... are under the Creative Commons CC0 1.0"); "allowed to be redistributed together with your projects" | yes | https://texturecan.com/terms/ |
| cgbookcase | CC0 1.0 ("published under the CC0 1.0 license") | yes | https://www.cgbookcase.com/textures |
| 3dtextures.me | CC0 (FAQ: "all textures on this site are licensed as CC0") | yes | https://3dtextures.me/about (via search summary) |
| ShareTextures | **Not plain CC0**: a "custom CC0-based license" that forbids redistribution "on other websites, in plugins, or as part of collections" without written permission, and automated downloads | **treat as no; not picked** | https://www.sharetextures.com/p/license |
| Fab, Quixel Megascans free listings | Fab **Standard License**: commercial use, any engine, ship inside a project, share with collaborators in a private repo; may **not** "Resell or redistribute the asset for free on a standalone basis"; no credit required | **no** (register as `Fab-Standard`, files under `/Game/ThirdParty/`) | https://www.fab.com/eula (summary at the top of the page); each listing shows "Free" and "License terms: Standard License" |
| Megascans history | Epic offered 1500+ Megascans free on Fab from 1 Jan 2025 under the Fab Standard License; some free items rotate | n/a | https://forums.unrealengine.com/t/access-legacy-megascans-on-bridge-and-quixel-com/2142664 (and search summary of the Megascans update posts) |

Project rule (docs/scenery/ASSETS.md): licences allowed are `CC0-1.0`, `CC-BY-4.0`, `Fab-Standard`, `Epic-Content-Only`,
`Own`; Fab and Epic content is never redistributable. Every primary below is CC0 (redistributable). Fab items are
alternates only, and are marked not redistributable.

## 3. How the files come down (no download made here)

- **Poly Haven:** `GET https://api.polyhaven.com/files/<id>` returns, per map and resolution, the file URL. Pattern
  (read from the API for `farm_furrows`): `https://dl.polyhaven.org/file/ph-assets/Textures/<jpg|png|exr>/<1k|2k|4k|8k|16k>/<id>/<id>_<map>_<res>.<ext>`
  with `<map>` = `diff`, `nor_gl`, `nor_dx`, `rough`, `ao`, `disp`, `arm` (AO/Rough/Metal packed, the same channel
  order as Unreal's ORM). A glTF bundle is `.../Textures/gltf/<res>/<id>/<id>_<res>.gltf`. Asset page:
  `https://polyhaven.com/a/<id>`. Send a User-Agent naming SAM3D (ToS 2.4).
- **ambientCG:** `https://ambientcg.com/get?file=<id>_<1K|2K|4K|8K>-<JPG|PNG>.zip` (some also 12K/16K), listed by
  `https://ambientcg.com/api/v3/assets?id=<id>&include=downloads,maps`. The maps the API lists: color, displacement,
  normal, roughness, ambient-occlusion (+ opacity for Pathway002). The site's previews are named
  `<id>_SQ_Color`, `_NormalDX`, `_Roughness`, `_AmbientOcclusion`, `_Displacement` (preview link in the v2 API); the
  zips are customarily `_Color`, `_NormalGL`, `_NormalDX`, `_Roughness`, `_AmbientOcclusion`, `_Displacement`, but
  the presence of both GL and DX normals in the zip was **not verified on a page** in this session. A 2K-JPG zip is
  about 30-37 MB, an 8K-PNG about 1 GB (API `size`).
- **TextureCan:** per texture page, 1K/2K/4K map packs and an `.sbsar` (Substance) file.
- **Fab:** "Add to My Library" (free) on fab.com, then the Fab plugin in UE 5.8 into `Content/ThirdParty/...`.

Unreal notes: use `nor_dx`/`NormalDX` (Unreal is DirectX-convention); a GL-only normal (beige_wall_002) needs
"Flip Green Channel" in the texture settings. Poly Haven's `arm` goes straight into an ORM slot (sRGB off).

## 4. Picks: ground

Sizes are the source's real-world scan size. "Tiles at 2-4 m" means the material is laid at its own scale (one
repeat per scan size) or up to twice that; the risk named is what shows when a player looks across a field from the
ground. Every ground material should get a macro variation (large noise, a far-distance blend to an aerial set, and
per-parcel hue/roughness from simcore's parcel id) whatever the pick.

| Layer | Primary | Alternate | Notes |
|---|---|---|---|
| Meadow / pasture grass | **Poly Haven `leafy_grass`** (https://polyhaven.com/a/leafy_grass): 2.0 m, 8K, diff, nor_gl, nor_dx, rough, ao, disp, arm, mask; Charlotte Baglioni. "trampled green blades with scattered brown leaves and twigs". | **ambientCG `Ground037`** (https://ambientcg.com/view?id=Ground037): 2.1 m photogrammetry, 1-8K, colour re-processed for better tiling (API description); green grass with moss and soil patches. | Autumn pasture: tint toward brown-green. Low-contrast, tiles well at 2 m; repetition mild. Fab Megascans "Wild Grass" (2x2 m, tags meadow/weed) is a further option: https://www.fab.com/listings/459a98c9-6424-4a96-a625-1d915f153c8b |
| Wheat stubble | **ambientCG `Ground071`** (https://ambientcg.com/view?id=Ground071): 1.75 m photogrammetry, 1-8K; brown soil strewn with pale straw/stalk pieces (seen on the preview). Matches a disced October stubble. | **Poly Haven `withered_grass`** (https://polyhaven.com/a/withered_grass): 2.0 m, 8K, full maps; "brittle, fibrous, sun-bleached blades in muted brown and tan". | **No free scan of standing wheat stubble in drill rows exists** at any source searched. Standing stalks need instanced cards/meshes (Fab Megascans "Dead Grass" atlas, 0.25 m, opacity + translucency, free, Standard: https://www.fab.com/listings/ecc8242f-c38f-47ea-b22b-5a39c7dce9ba) in rows along the parcel's heading. 1.75 m repeat: straw pieces will show a pattern beyond 20 m; break with macro noise. |
| Ploughed soil with furrows | **Poly Haven `farm_furrows`** (https://polyhaven.com/a/farm_furrows): 2.1 m, 8K, full maps; "plowed farm soil with deep ridged furrows, damp brown earth, rough clods and scattered straw bits"; about six ridges across the scan (preview). | **Poly Haven `dry_mud_field_001`** (https://polyhaven.com/a/dry_mud_field_001): 3.0 m, 16K, full maps; rough brown soil, compacted clumps, track marks. | Furrow direction must follow simcore's furrow heading per 3 km square: rotate the UVs per pixel from that heading. Furrows repeat anyway, so the tile is safe along them; clods repeat across. The alternate has no furrows: add a sine ridge normal oriented by the heading. |
| Harrowed soil | **Poly Haven `farm_soil`** (https://polyhaven.com/a/farm_soil): 2.0 m, 8K, full maps; "dark brown, slightly damp earth, crumbly clods". Dark like the Alföld's chernozem. | **Poly Haven `raked_dirt`** (https://polyhaven.com/a/raked_dirt): 2.0 m, 8K; "compact sandy soil with fine rake grooves" (harrow-like lines). | Very uniform; repetition low. raked_dirt is lighter, for the sandy Danube-Tisza interfluve. |
| Maize-field soil with stalk litter | **ambientCG `Ground072`** (https://ambientcg.com/view?id=Ground072): 1.75 m photogrammetry, 1-8K; dense pale chopped stalks and sticks on soil (preview), like chopped maize stover. | **ambientCG `Ground071`** (as above), used for a field with less residue. | No free scan of a maize stubble field (stalk stumps in 70-75 cm rows) was found; stumps and fallen stalks as instanced meshes. Ground072's bright sticks will show a repeat at 1.75 m: scale to 3-3.5 m and vary density with a mask. |
| Sunflower field after harvest | **ambientCG `Ground103`** (https://ambientcg.com/view?id=Ground103): photogrammetry (2026), 1-8K; brown earth with small green specks (like volunteer seedlings). Scan size not stated. | **Poly Haven `dirt_floor`** (https://polyhaven.com/a/dirt_floor): 2.07 m, 8K; "uneven brown dirt ... scattered twig and leaf debris". | The thick cut stalks (20-30 cm stumps) and broken heads are geometry. Ground103's size must be measured before tiling. |
| Forest floor, oak leaf litter | **Poly Haven `forest_leaves_03`** (https://polyhaven.com/a/forest_leaves_03): 2.3 m, 8K, full maps; "dense dry forest leaf litter - brown, yellow and green curled leaves". | **Fab Megascans "Dry Fallen Leaves"** (https://www.fab.com/listings/cf0e09ac-8e22-42d9-89d2-a991b8cadc0f): 4x4 m scan, 4096 px/m, Basecolor, Normal, Roughness, AO, Cavity, Gloss, Specular, Bump (no displacement listed); Free, Standard License, not redistributable. | Dense litter repeats little. The 4 m Fab scan halves the repeat. CC0 second choice: Poly Haven `dry_decay_leaves` (2 m, 16K). |
| Pine needles | **Poly Haven `forrest_ground_03`** (https://polyhaven.com/a/forrest_ground_03): 2.0 m, 8K (+bump, spec); "compact soil, fallen pine needles, leaf litter and twigs". | **ambientCG `Ground077`** (https://ambientcg.com/view?id=Ground077): photogrammetry, 1-8K; a dense orange-brown needle carpet with a cone (tags pine, cone). Size not stated. | Ground077 looks more like a thick needle mat under a pine plantation; measure its size. The Fab "Pine Needles" listings are 0.42 m atlases, not ground. |
| Marsh mud and wet ground | **Poly Haven `mud_forest`** (https://polyhaven.com/a/mud_forest): 2.35 m, 8K; "dark, wet forest mud ... wet sheen". | **TextureCan Ground 0034 "Muddy Ground Texture with Puddles"** (https://texturecan.com/details/261/): CC0, 1K/2K/4K + SBSAR, procedural; water level, wetness and puddle colour adjustable. | Procedural alternate: less real but the puddles are tunable. Wetness should also be a material parameter (darken albedo, lower roughness) driven by weather. |
| Reeds / wet grass | **Poly Haven `sparse_grass`** (https://polyhaven.com/a/sparse_grass): 2.0 m, 8K (+mask); "thin green tufts and visible roots over rough, damp soil". | **TextureCan Ground 0032 "Pond Side Grassy and Muddy Land"** (https://texturecan.com/details/258/): CC0, 1K-4K + SBSAR, procedural (water-lily leaves, grass, clover, pebbles adjustable). | Reed beds (Phragmites, 2-3 m tall) are foliage, not a texture: this layer is only the ground under them. |
| Dirt farm track with ruts | **Poly Haven `muddy_tracks`** (https://polyhaven.com/a/muddy_tracks): 2.25 m, 8K; "wet, earthy mud with deep tire tracks". | **ambientCG `Pathway002`** (https://ambientcg.com/view?id=Pathway002): 3.0 x 1.5 m photogrammetry, 1-16K, with an **opacity** map; a dirt path strip with grassy edges. | Lay along the track's direction (road UV). Ruts repeat every 2.25 m along the track: vary with a second offset sample. Pathway002's alpha edges blend into the verge. Fab "Mud Tire Tracks" (2x2 m, free, Standard): https://www.fab.com/listings/9c0bcc25-5ca5-4e7d-97f0-375fbcafaed0 |
| Gravel / macadam | **Poly Haven `gravel_road`** (https://polyhaven.com/a/gravel_road): 2.0 m, 16K; "worn, rough surface with compacted dirt and scattered stones". | **Poly Haven `tarred_gravel`** (https://polyhaven.com/a/tarred_gravel): 2.2 m, 8K; "dark, compacted asphalt with embedded stones" (tar-bound macadam). | Both tile well (fine, even grain). ambientCG `Gravel043` (1.6 m, grey crushed fine gravel, 2026) for loose grey chippings at sites. |
| Old cracked asphalt | **Poly Haven `asphalt_04`** (https://polyhaven.com/a/asphalt_04): 4.04 m, 16K; "weathered, rough asphalt with dry, discolored tones ... subtle cracks". | **Poly Haven `road_damaged`** (https://polyhaven.com/a/road_damaged): 2.2 m, 8K; "cracked, patchy asphalt with potholes". | asphalt_04's 4 m scan avoids the visible repeat of `asphalt_02` (3 m, one long sealed crack seen on the preview). Use road_damaged only as a blended patch layer: potholes repeat visibly at 2.2 m. |
| Concrete road slabs | **Poly Haven `concrete_floor_damaged_01`** (https://polyhaven.com/a/concrete_floor_damaged_01): 5.0 m, 16K; "rough, weathered concrete floor with cracks, chips, pitting and scuffs". | **Poly Haven `cracked_concrete`** (https://polyhaven.com/a/cracked_concrete): 2.0 m, 8K; "large fractured plates, deep jagged fissures". | No scan has slab joints at road spacing: draw the transverse and longitudinal joints in the material from the road's UV (slab length a parameter). The Soviet PAG-14 airfield plate is 6 x 2 m, 14 cm thick (IPMS kit review: https://reviews.ipmsusa.org/review/soviet-pag-14-airfield-plates, via search summary); no free texture of one was found. |
| Packed earth in yards | **Poly Haven `dirt`** (https://polyhaven.com/a/dirt): 2.0 m, 8K; "dusty, compacted earth with small rocks, debris and subtle footprints". | **Fab Megascans "Dry Trampled Soil"** (https://www.fab.com/listings/e9c8521d-0d32-46ee-8607-bca13605159a): 2x2 m, 4096 px/m, Displacement, Normal, Roughness, Basecolor, AO, Cavity, Specular, Gloss, Bump; tags Rural, Barn, Farm, Field, Trampled; Free, Standard License. | Footprints repeat at 2 m: keep yards small or vary. |
| River sand / mud bank | **ambientCG `Ground054`** (https://ambientcg.com/view?id=Ground054): 3.5 m photogrammetry, 1K-16K; smooth grey-beige silty mud/sand (tags beach, mud, sand). | **ambientCG `Ground106`** (https://ambientcg.com/view?id=Ground106): 2.5 m photogrammetry (Jul 2026), 1-8K; river mud with darker patches and scattered leaves (tags river, riverbed, muddy). | Ground054 is plain and large: tiles well. CC0 third choice: Poly Haven `damp_sand` (2 m). |
| Loess in hill cuts | **Poly Haven `excavated_soil_wall`** (https://polyhaven.com/a/excavated_soil_wall): 2.0 m, 16K; "dusty, dry sandy earth with deep grooves" in an ochre tone (preview). | **ambientCG `Ground104`** (https://ambientcg.com/view?id=Ground104): photogrammetry (Jan 2026), 1-8K; yellowish crumbly soil with faint horizontal layering (preview). Size not stated. | Vertical faces: use triplanar/world-aligned mapping. Real loess walls stand in vertical columns; neither scan shows that, so add a vertical crack normal. |
| Rock in hill cuts | **Poly Haven `rock_face_04`** (https://polyhaven.com/a/rock_face_04): 2.0 m, 8K; "dark, weathered slabs, deep fractured seams, and leaf litter in crevices" (suits dark volcanic rock of the northern hills). | **Poly Haven `quarry_wall`** (https://polyhaven.com/a/quarry_wall): 1.8 m, 8K; "chisel-like striations, coarse grain and mottled brown-grey". | Rock repeats strongly at 2 m: use 4-8 m on large faces with a detail layer at 2 m. |
| Grass verge | **Poly Haven `grass_ground`** (https://polyhaven.com/a/grass_ground): 2.51 m, 16K; "worn grass on soil ... natural brown-green variation". | **Poly Haven `grass_path_2`** (https://polyhaven.com/a/grass_path_2): 1.0 m, 8K (+bump, spec); "compacted dirt path with scattered stones and small grass tufts". | grass_path_2 is only 1 m: high repeat; use it as a narrow strip next to the road edge only. |
| Military ground (trampled dirt, tracks) | **Poly Haven `aerial_mud_1`** (https://polyhaven.com/a/aerial_mud_1): 8.0 m, 8K (+ref); "dark wet mud with pronounced tire tracks, ruts" (horizontal tracks on the preview). | **Poly Haven `dirt_aerial_02`** (https://polyhaven.com/a/dirt_aerial_02): 20 m, 8K (+rough_ao); "compacted, eroded brown dirt with muddy patches ... visible tracks". | Aerial scans have low texel density (8K over 8 m = 1024 px/m; over 20 m = 410 px/m): blend a 2 m detail layer (`dirt`, `brown_mud_dry`) near the camera. Their large size makes them the best anti-repetition far layers for every bare-soil class too. |

## 5. Picks: building and prop surfaces

| Surface | Primary | Alternate | Notes |
|---|---|---|---|
| Plaster / stucco, painted, weathered | **Poly Haven `painted_plaster_wall`** (https://polyhaven.com/a/painted_plaster_wall): 2.0 m, 16K; "slightly discolored exterior plaster with worn patches, subtle stains". Light grey (preview): tint per house. | **Poly Haven `beige_wall_002`** (https://polyhaven.com/a/beige_wall_002): 3.0 m, 32K; "rough, urban wall with fine granular bumps" (a dash-type render). Maps: diff, **nor_gl only**, rough, ao, disp, arm. | Paint colour as a material parameter. |
| Whitewash | **ambientCG `Plaster007`** (https://ambientcg.com/view?id=Plaster007): photogrammetry (Apr 2025), 1-8K; old white paint on plaster with grey-green weathering (preview). Size not stated. | **Poly Haven `whitewashed_brick`** (https://polyhaven.com/a/whitewashed_brick): 2.0 m, 16K; "whitewashed, weathered brick with chipped paint ... red-brown brick showing through". | whitewashed_brick for outbuildings and barns; Plaster007 for rendered houses. Poly Haven `white_rough_plaster` (1 m, heavy flaking) as a decal-style damage layer. |
| Red clay roof tiles, beaver-tail (hódfarkú) | **Poly Haven `ceramic_roof_01`** (https://polyhaven.com/a/ceramic_roof_01): 3.5 m, 8K (+rough_ao); flat tiles with segment-rounded ends laid in staggered rows (preview), moss patches. | **Poly Haven `roof_3`** (https://polyhaven.com/a/roof_3): 2.0 m, 8K; the same flat, round-ended staggered tile, moss-streaked. | Hódfarkú: rectangular with one curved edge, laid in two or three overlapping rows (https://lakbermagazin.hu/praktikus-lakberendezesi-otletek/236-csereptetk.html, via search summary). Both scans are moss-heavy, Western-European roofs: reduce the moss by desaturating green in the albedo. `clay_roof_tiles` (4 m, narrow pointed tiles) is a third option. |
| Red clay roof tiles, interlocking (hornyolt) | **Poly Haven `roof_tiles`** (https://polyhaven.com/a/roof_tiles): 2.0 m, 16K (+spec); Stephan Seeliger; tags "interlocking tiles"; ribbed interlocking terracotta in straight courses (preview). | **Poly Haven `clay_roof_tiles_03`** (https://polyhaven.com/a/clay_roof_tiles_03): 2.6 m, 8K; flat interlocking tiles with "embossed diamond shaped details". | Grooved tiles are common on houses and interlock watertight (lakbermagazin page above). Courses line up with the tile: map one repeat to whole tile courses. |
| Corrugated asbestos-cement (eternit) | **Poly Haven `asbestos_sheet_02`** (https://polyhaven.com/a/asbestos_sheet_02): 1.8 m, 16K; "corrugated asbestos sheet with weathered grey surface, dirt, rust streaks, screw holes". | **Poly Haven `asbestos_sheet`** (https://polyhaven.com/a/asbestos_sheet): 2.0 m, 16K; darker, dusty, fibrous. | Corrugation is periodic: tiling along the sheet is invisible; across sheets, the screw-hole line repeats at 1.8 m (true to life: sheet width). |
| Corrugated metal | **Poly Haven `corrugated_iron_02`** (https://polyhaven.com/a/corrugated_iron_02): 2.7 m, 16K, with a **Metal** map; "corrugated galvanized iron roofing ... subtle weathering, rivets". | **Poly Haven `rusty_corrugated_iron`** (https://polyhaven.com/a/rusty_corrugated_iron): 2.0 m, 16K; heavy orange-brown rust. | Blend the two by a rust mask for old sheds. |
| Brick | **Poly Haven `brick_wall_006`** (https://polyhaven.com/a/brick_wall_006): 3.0 m, 8K; Jan Burghardt; "weathered red brick wall: chipped, cracked and uneven bricks". | **Poly Haven `red_brick`** (https://polyhaven.com/a/red_brick): 1.4 m, 16K; "red brick wall ... recessed grey mortar". | Bond is regular, so repeat reads as brickwork; colour blotches repeat at 1.4 m on the alternate. |
| Precast concrete panel | **Poly Haven `concrete_slab_wall`** (https://polyhaven.com/a/concrete_slab_wall): 2.3 m, 8K; "weathered concrete slab wall ... horizontal joints" (smooth grey slab, one joint per tile on the preview). | **Fab Megascans "Concrete Slab"** (https://www.fab.com/listings/1607354c-659f-4db1-a0c2-dce2e09ff461): 8x4 m scan, 2048 px/m, Basecolor, Normal, Roughness, AO, Cavity, Gloss, Specular, Bump; tags Panel, Facade, Slab; Free, Standard License. | Panel joints should come from the building's own panel grid (UV one panel per repeat), so a jointless or panel-sized scan is best; the 8x4 m Fab scan fits one storey-height panel. Hungarian large-panel housing: Soviet technology adopted 1962-63, factories standardised 1970-72, ended 1990 (EMI presentation: https://epito.emi.hu/EMI/web.nsf/Pub/R6C2K9/$FILE/Construma2013-06-TothP-EMI-panelszerkezetek.pdf, via search summary). Their surface finish (plain, painted or exposed aggregate) was **not** confirmed; Poly Haven `pebble_embedded_concrete` (2.6 m, 16K) covers the exposed-aggregate case, `concrete` (4 m, vertical grooves) industrial halls. |
| Weathered wood planks | **Poly Haven `weathered_planks`** (https://polyhaven.com/a/weathered_planks): 2.0 m, 8K; "dark brown, cracked and chipped boards with nail holes". | **Poly Haven `raw_plank_wall`** (https://polyhaven.com/a/raw_plank_wall): 1.87 m, 8K; "roughhewn knotty-pine planks ... nail holes". | Untreated outdoor boards go silver-grey: Poly Haven `wood_planks_grey` (1.5 m) for fences and barns. |
| Rusty metal | **Poly Haven `rusty_metal_sheet`** (https://polyhaven.com/a/rusty_metal_sheet): 2.0 m, 16K; "bluish-gray painted steel with orange-brown rust streaks". | **Poly Haven `rust_coarse_01`** (https://polyhaven.com/a/rust_coarse_01): 2.2 m, 16K; "coarse, weathered rust - red-brown stained, pitted". | Painted steel with rust fits gates, water towers, farm machinery. |

## 6. Gaps and cautions

1. **Standing wheat stubble, maize stalk stumps, sunflower stalks**: no free scanned surface anywhere searched
   (Poly Haven, ambientCG catalogues by keyword "straw|hay|stubble|crop|corn|maize|wheat|field|farm"; Fab free
   "straw", "plowed" returned nothing). The ground under them is covered (Ground071, Ground072, Ground103); the
   stalks must be geometry (instanced cards in rows along the parcel's furrow heading) or a parallax layer.
2. **Reed beds**: foliage, not texture (vegetation research).
3. **Beaver-tail tiles**: the CC0 scans are Western-European segment-cut tiles, mossy; Hungarian pointed/round-cut
   hódfarkú specifically was not found. **Rhombic asbestos-cement slates** (common on village houses) were not found
   at any source; `asbestos_sheet_02` covers only corrugated sheet.
4. **Kádár-era textured render** (spattered / stone-dust renders): no specific scan; `beige_wall_002` is the proxy.
5. **Concrete road slab joints and PAG-14 plates**: no scan; joints must be procedural in the material.
6. **Loess columnar walls**: proxies only.
7. **Unknown scan sizes** (ambientCG Ground077, Ground103, Ground104, Plaster007): measure against a known object
   before setting tiling.
8. **ambientCG zip contents** (both NormalGL and NormalDX): customary, not verified on a page this session.
9. **Fab free listings rotate**; check "Free" and "Standard License" at the time of claiming; Fab files may not be
   committed (ASSETS.md).
10. **ShareTextures** is excluded: its licence forbids redistribution in collections without permission.
11. The web-search budget ran out: cgbookcase and 3dtextures.me were checked only at catalogue level (cgbookcase
    lists Grass 01, Dirt 01-03, Gravel 01, Asphalt 01-02, Stucco Wall 01, Brick Wall 01, Rusty Corrugated Metal Panel 01,
    Rock Face 01 at https://www.cgbookcase.com/textures; 3dtextures.me search "field" gave only Grass 004:
    https://3dtextures.me/2018/07/02/grass-004/), none picked.

## 7. Development process for these textures (proposal)

1. **Register first**: extend `data/scenery/assets.json` with a `materials` section (id, source page, author, licence,
   redistributable, chosen resolution, real size in m, fetched date, sha256 of each file), keyed to the land-cover
   class and crop (`field:ploughed`, `field:wheat`, `forest:oak`, `road:asphalt` ...) and to building surfaces, and
   check it in `tools/check_assets.py` like the meshes. CC0 files under `art/thirdparty/textures/<source>/<id>/`
   (git-ignored until sizes are agreed), Fab under `unreal/SAM3D/Content/ThirdParty/Textures/`.
2. **Fetch at 2K by script** from the two APIs (Poly Haven `files`, ambientCG `get?file=`), with a SAM3D User-Agent,
   one asset at a time and only with the owner's say-so (ASSETS.md); keep `diff`, `nor_dx`, `arm` (Poly Haven) or
   Color, NormalDX, Roughness, AO (ambientCG); displacement only for the few layers that get tessellation.
3. **Resolution budget**: 2K for ground layers at a 2 m repeat gives about 1000 px/m, enough at eye height from the
   cabin; 4K only for the battery position's roads and pads; 1K for far layers; buildings 2K. Use virtual texture
   streaming.
4. **Ground material**: the ground is a ProceduralMeshComponent coloured by vertex colour, with no UVs. Map by
   world position (large-world-coordinate safe), triplanar on slopes; select layers from the simcore 25 m land-cover
   raster uploaded as a small texture (class, crop, parcel id, furrow heading per 3 km square), so the drawing and
   the line of sight still come from the same simcore data. Rotate furrow and stubble-row UVs by the heading.
5. **Anti-repetition**: per layer a near detail scan (2-4 m), a far aerial scan (aerial_mud_1, dirt_aerial_02,
   Ground054 3.5 m), large-scale noise for hue and roughness, per-parcel variation from the parcel id, and a
   stochastic/texture-variation sampling function.
6. **Season and weather**: albedo tint and roughness/wetness parameters per layer for mid-October versus summer
   missions (stubble greyer, grass browner) and for rain (`data/weather/`).
7. **Check**: a look-dev level with every layer at 1x1 m, 2 m and 4 m tiling, seen from the Osa's cabin height and
   from 50 m, under the scenario's sun (`docs/LIGHT.md`), before any layer goes into the map.
# Texture gaps filled: water, sand, October winter wheat, soil colour targets

Research notes, 3 Nov 2026 (area: textures). Follows `pbr_textures.md` (the first texture pass) and `references.md`
(gap: "No written source was found for soil colour"). Nothing was downloaded: catalogue metadata from the Poly Haven
and ambientCG APIs, the sources' own thumbnails, public pages and one public soil database (ISRIC's) were read, and
the local engine install (`D:/UE_5.8`) was listed.

**Search budget:** the session's web-search budget was already used up when this pass started (the WebSearch tool
refused the first query). Everything below comes from pages fetched by known address, the two texture catalogues'
APIs, ISRIC's soil-profile API, Crossref and the local engine. Fab was **not searched** in this pass (its search needs
a browser or the search tool), so no Fab item is proposed here; see section 7.

## 1. Soil colour targets (the written sources found)

### 1.1 The sources

| # | Source | What it gives | URL |
|---|---|---|---|
| S1 | ISRIC World Soil Reference Collection, Hungarian profiles HU001-HU020 (described by D. Creutzberg; classification year 2015 in the database), read through ISRIC's own GraphQL API (`profiles(condition: {countryCode: "HU"})`, fields `colourDry*`, `colourMoist*`, `orgc`, `tceq`, `profileDescriptionSite`) | Munsell colours of real Hungarian horizons, organic carbon (g/kg) and CaCO3 (g/kg) | https://wsm.isric.org/ (collection site); API https://graphql.isric.org/isis/graphql (the endpoint named in https://wsm.isric.org/assets/js/profileDetails.js) |
| S2 | IUSS Working Group WRB, *World Reference Base for Soil Resources*, 4th ed., 18 Dec 2022: 3.1.6 chernic horizon (p. 40), 3.1.20 mollic horizon (p. 52) | The Munsell limits that define a chernozem's and a meadow soil's dark topsoil | https://files.isric.org/public/documents/WRB_fourth_edition_2022-12-18.pdf |
| S3 | Post, Fimbres, Matthias, Sano, Accioly, Batchily, Ferreira (2000), "Predicting Soil Albedo from Soil Color and Spectral Reflectance Data", *SSSA Journal* 64:1027-1034 (abstract read from Crossref) | "soil albedo (0.3-2.8 µm) = 0.069 (color value) - 0.114", 26 US soils, air-dry and wet, smooth surfaces | https://doi.org/10.2136/sssaj2000.6431027x ; abstract: https://api.crossref.org/works/10.2136/sssaj2000.6431027x |
| S4 | Munsell renotation data (1943), `real.dat`: x, y, Y for each hue/value/chroma under Illuminant C | Converts a Munsell notation to a colour | https://www.rit-mcsl.org/MunsellRenotation/real.dat |
| S5 | Unreal Engine 5.8 docs, *Physically Based Materials*: measured Base Color "intensity only" | Bare soil 0.13, green grass 0.21, desert sand 0.36, fresh asphalt 0.02, worn asphalt 0.08; Specular default 0.5 = about 4 % reflectance | https://dev.epicgames.com/documentation/en-us/unreal-engine/physically-based-materials-in-unreal-engine |
| S6 | physicallybased.info (CC0 database) | Sand base colour, linear sRGB (0.44, 0.386, 0.231), roughness 0.9 (from the DONTNOD chart); water IOR 1.3325 | https://api.physicallybased.info/materials ; site https://physicallybased.info/ |
| S7 | S. Lagarde, "Feeding a physically based shading model" (2011) | Albedo table from Wikipedia: bare soil 0.17, green grass 0.25, desert sand 0.40, worn asphalt 0.12; "a little less" for the diffuse colour because the values include specular; diffuse sRGB range 50-240 | https://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/ |
| S8 | DONTNOD PBR chart for UE4 (Lagarde, 2014) | Non-metal base colour must lie in sRGB 50-243 | https://seblagarde.wordpress.com/2014/04/14/dontnod-physically-based-rendering-chart-for-unreal-engine-4/ |

### 1.2 What the Hungarian profiles say (S1)

| Profile | Place and soil | Horizon | Munsell (as recorded) | Organic C, CaCO3 |
|---|---|---|---|---|
| HU007 | Hatvan, Cserhát foothills, eolian loess, **arable**; WRB Cambisol | Ap 0-21 cm | **10YR 4/2 dry**, "dark greyish brown" | OC 10.6 g/kg (1.1 %), CaCO3 4.0 % |
| HU007 | same | Bw 70-100 | 10YR 5/2 dry, "greyish brown" | OC 0.4 % |
| HU007 | same | Ck 100-120 (the loess) | **10YR 6/4 dry**, "light yellowish brown" | CaCO3 22 % |
| HU015 | Püspökladány, Hortobágy, grazed grassland; Solonetz | Ah 0-21 | **10YR 3/2 dry**, "very dark greyish brown" | OC 4.3 % |
| HU015 | same | Btn2 35-60, BCk | 10YR 3/1 dry, "very dark grey" | |
| HU014 | Püspökladány, Solonetz | Ah 0-3 / Btn1 / Btn2 | 10YR 5/2 / 10YR 6/3 / 2.5Y 5/2 dry | OC 4.8 % (0-3 cm crust) |
| HU011 | Apaj (Danube-Tisza interfluve), saline grassland; Solonchak | Ahgz 0-4, ABgz | **5Y 6/1 and 5Y 7/1 dry**, "(light) grey" | OC 0.4 %, CaCO3 11-16 % |
| HU002 | Sóskút, Transdanubian Mountains, on rock; Phaeozem | Ah1 0-10 / C | 10YR 3/2 moist / 10YR 8/2 moist "white" | OC 8.3 % |
| HU019 | Nádasladány, Sárrét (peat); Histosol | H1, H2 | 10YR 2/1 dry, "black" | OC 38 % |
| HU008, HU009, HU010 | Ozora; Jászkarajenő; Solt: **Chernozems** on loess (HU008 and HU009 arable, HU008 "notably wheat") | Ap / Ah | **no colour recorded** in the database | OC 2.1-2.9 % (Ap), CaCO3 2.6-9.5 % |
| HU017 | Bocsonád, northern plain, "classified as **Meadow Soil** locally"; WRB Vertisol | Ah1, Ah2 | **no colour recorded** | OC 4.7 % (0-20 cm) |
| HU001 | Pusztavacs, Danube-Tisza interfluve, flat dune plateau, "Calcareous Blownsand"; white poplar and black locust | Ah 0-15, C | **no colour recorded** | OC 0.23 % (0-10 cm), CaCO3 0.5-3 % |
| HU006 | Kál, Heves sand plateau, "Brown Forest Soil locally", "alternating clay layers" (the lamellae of a kovárványos soil), pH 6.7-7.5 | Ah, C | **no colour recorded** | OC 0.58 % (0-20 cm) |
| HU020 | Csepel Island, fluvial sand, grazed | Ah | no colour recorded | OC 1.35 %, CaCO3 31.8 % |

All rows: ISRIC ISIS API (S1), queried 3 Nov 2026. The profile site descriptions quoted are ISRIC's own
(`profileDescriptionSite`).

### 1.3 The rules that bound the missing colours (S2)

- **Chernic horizon** (the chernozem's topsoil, "from Russian chorniy, black"): in at least 90 % of the horizon (or
  below the plough layer) a Munsell **value ≤ 3 moist and ≤ 5 dry, chroma ≤ 2 moist**; at least 1 % organic C; at
  least 30 cm thick. For 15-40 % CaCO3 the limits are value ≤ 3 and chroma ≤ 2 moist with ≥ 1.5 % OC; for ≥ 40 %
  CaCO3 or loamy sand or coarser, value ≤ 5 and chroma ≤ 2 moist with ≥ 2.5 % OC. (S2, 3.1.6, p. 40.)
- **Mollic horizon** (the dark topsoil of phaeozems and of most Hungarian meadow soils): value ≤ 3 moist and ≤ 5 dry,
  **chroma ≤ 3 moist**, ≥ 0.6 % OC; for loamy sand or coarser, value ≤ 5 and chroma ≤ 3 moist with ≥ 2.5 % OC.
  (S2, 3.1.20, p. 52.)
- The limits put the **dry** colour up to two Munsell value steps above the **moist** colour (≤ 3 moist, ≤ 5 dry):
  this is the darkening range a wetness parameter must cover.
- Hungarian context: the three most widespread soil types are the non-podzolic clay-illuviated brown forest soil
  (8.77 %), the **carbonate meadow chernozem (7.12 %)** and the **carbonate meadow soil (6.18 %)** (Kocsis et al., as
  quoted on https://hu.wikipedia.org/wiki/Talaj). Chernozem is "feketeföld" (black earth) (same page).

### 1.4 Converting Munsell to linear base colour

Method: the renotation x, y, Y (S4, Illuminant C) to XYZ, Bradford adaptation C to D65, then linear sRGB; odd chromas
interpolated in xy between the neutral point (0.3101, 0.3162) and chroma 2 at the same value. The broadband albedo
column is S3's regression. Y is the renotation's luminous reflectance (relative to the renotation's white; the old
MgO reference may make absolute values about 2.5 % lower, not checked on a page). Sanity check: 10YR 5/2 came out as
sRGB (137, 120, 102); 10YR 3/2 as (85, 70, 54). The script that did it is a 60-line Python file using only the
standard library (reproducible from S4).

| Munsell | Y (luminous reflectance) | Linear sRGB (Unreal Base Color) | sRGB 8-bit | S3 broadband albedo |
|---|---|---|---|---|
| 10YR 2/1 | 0.031 | (0.039, 0.030, 0.023) | 56, 48, 42 | 0.02 |
| 10YR 3/1 | 0.066 | (0.079, 0.063, 0.050) | 80, 71, 63 | 0.09 |
| 10YR 3/2 | 0.066 | (0.091, 0.061, 0.036) | 85, 70, 54 | 0.09 |
| 10YR 4/1 | 0.120 | (0.141, 0.116, 0.094) | 105, 96, 87 | 0.16 |
| 10YR 4/2 | 0.120 | (0.160, 0.113, 0.072) | 111, 95, 76 | 0.16 |
| 10YR 5/2 | 0.198 | (0.249, 0.189, 0.132) | 137, 120, 102 | 0.23 |
| 10YR 5/3 | 0.198 | (0.274, 0.185, 0.105) | 143, 119, 91 | 0.23 |
| 10YR 6/2 | 0.300 | (0.368, 0.290, 0.209) | 163, 147, 126 | 0.30 |
| 10YR 6/3 | 0.300 | (0.397, 0.285, 0.173) | 169, 145, 116 | 0.30 |
| 10YR 6/4 | 0.300 | (0.425, 0.280, 0.140) | 174, 144, 105 | 0.30 |
| 10YR 7/2 | 0.431 | (0.513, 0.418, 0.312) | 190, 173, 152 | 0.37 |
| 10YR 7/3 | 0.431 | (0.551, 0.412, 0.264) | 196, 172, 140 | 0.37 |
| 2.5Y 5/2 | 0.198 | (0.240, 0.193, 0.127) | 134, 121, 100 | 0.23 |
| 5Y 6/1 | 0.300 | (0.320, 0.300, 0.245) | 153, 149, 136 | 0.30 |
| 5Y 7/1 | 0.431 | (0.456, 0.431, 0.359) | 180, 175, 162 | 0.37 |

### 1.5 Targets per soil (what a calibrated scan's mean must hit)

"Sourced" means a Hungarian profile gives the colour; "bounded" means WRB's limits and the profiles' organic carbon
bound it; "estimate" means no written colour was found and the value must be checked against the reference photos.

| Ground | Dry target (mean linear base colour) | Moist / after rain | Basis |
|---|---|---|---|
| **Chernozem ploughland** (plain's loess, Mezőföld, Hajdúság, Békés-Csanád) | 10YR 3.5/2: **(0.126, 0.087, 0.054)**, Y ≈ 0.09; range 10YR 3/2 (0.091, 0.061, 0.036) to 4/2 (0.160, 0.113, 0.072) | 10YR 3/1-3/2 moist: about **(0.085, 0.062, 0.043)**, Y ≈ 0.066; 10YR 2/1 (0.039, 0.030, 0.023) for a saturated furrow bottom | **Bounded**: WRB chernic ≤ 5 dry / ≤ 3 moist, chroma ≤ 2 moist (S2); arable loess topsoil with 1.1 % OC is 10YR 4/2 dry (HU007), a 4.3 % OC topsoil is 10YR 3/2 dry (HU015); the chernozems' Ap holds 2.1-2.9 % OC (HU008-HU010), between the two. |
| **Meadow soil** (réti talaj: floodplains, the Hortobágy, the Körös country) | 10YR 3/1-3/2: **(0.085, 0.062, 0.043)**, Y ≈ 0.066 (darker and greyer than chernozem) | 10YR 2/1: (0.039, 0.030, 0.023) | **Bounded**: mollic limits (S2); the local "Meadow Soil" profile HU017 holds 4.7 % OC (no colour); the Hortobágy clay profile HU015 with 4.3 % OC is 10YR 3/2 dry over 10YR 3/1 "very dark grey". Gley subsoil in banks and ditches: 2.5Y-5Y hues (HU014, HU011). |
| **Arable loess, eroded or thin** (northern foothills, Cserhát, Mátraalja) | **10YR 4/2: (0.160, 0.113, 0.072)** | 10YR 3/2 | **Sourced** (HU007 Ap, Hatvan). |
| **Loess wall in a cut or sunken road** | **10YR 6/4: (0.425, 0.280, 0.140)** | about 10YR 5/4 | **Sourced** (HU007 Ck, the loess itself). This replaces the eyeballed ochre of `excavated_soil_wall` in `pbr_textures.md`. |
| **Saline (szik) grassland surface** (Danube-Tisza interfluve, Hortobágy) | **5Y 6/1-7/1: (0.32-0.46, 0.30-0.43, 0.25-0.36)** | 5Y 5/1 (estimate) | **Sourced** (HU011 Apaj, light grey; HU014 Ah 10YR 5/2). |
| **Calcareous blown sand, bare** (Kiskunság dunes and sandy tracks) | 10YR 6.5/2: **(0.441, 0.354, 0.261)**, Y ≈ 0.36; range 10YR 6/2 to 7/3 | 10YR 5/2: (0.249, 0.189, 0.132) | **Estimate, bounded**: HU001's blown sand holds only 0.23 % OC (so pale) and 0.5-3 % CaCO3; UE's measured desert sand is 0.36 (S5); physicallybased's sand (0.44, 0.386, 0.231) has the same luminance but more chroma (S6); black pine was planted "a meszes homoktalajok megkötésére" (to bind the calcareous sand soils): https://hu.wikipedia.org/wiki/Feketefeny%C5%91 . |
| **Humous sand ploughland** (Kiskunság and Nyírség fields) | 10YR 5/2-5/3: **(0.26, 0.19, 0.12)** | 10YR 4/2 | **Estimate, bounded**: the mollic limit for loamy sand or coarser is value ≤ 5 moist with ≥ 2.5 % OC (S2), and most sand fields hold far less, so they sit lighter than that limit. |
| **Nyírség sand** (lime-free, brown, with lamellae) | 10YR 5/3-6/4: about **(0.35, 0.24, 0.13)** | 10YR 4/3 | **Estimate**: HU006 (Heves sand plateau, "Brown Forest Soil locally", alternating clay layers, pH 6.7-7.5, OC 0.58 %) has no colour; the Nyírség's surface is "lösz és futóhomok" with "barnaföld" (https://hu.wikipedia.org/wiki/Ny%C3%ADrs%C3%A9g); its north has "sandy forest soils", its south "loose wind-blown sand" (https://en.wikipedia.org/wiki/Ny%C3%ADrs%C3%A9g). Warmer and browner than the Kiskunság's calcareous grey. |
| Peat (Sárrét, drained marsh) | 10YR 2/1: (0.039, 0.030, 0.023) | same | **Sourced** (HU019). |

Cross-checks: UE's "bare soil" 0.13 (S5) and Lagarde's 0.17 (S7) sit between the chernozem and the sand targets, as
they should for an average soil; Post's regression (S3) gives 0.09-0.16 broadband for the chernozem's values 3-4 and
0.30-0.37 for the sands' 6-7. The DONTNOD floor of sRGB 50 (S8) is respected by everything except a saturated peat or
a water-filled furrow (10YR 2/1 is sRGB 56, 48, 42: just above it).

### 1.6 What this means for `data/scenery/map_dressing.json` today

The map's vertex colours (linear, "the plain's crops in summer") have `ploughed` = (0.24, 0.18, 0.12), luminance 0.19:
about **twice the chernozem target** (Y ≈ 0.09 dry) and close to the humous-sand target. A ploughed field on the
loess plain should be about (0.13, 0.09, 0.055) dry, (0.085, 0.06, 0.045) after rain; one on the Kiskunság or
Nyírség sand about (0.26, 0.19, 0.12). `water` = (0.05, 0.11, 0.15) is a clear blue that no Alföld water has (section
2.2). These are suggestions for the owner; the file says its colours are estimates to be tuned against
`-SimMapShots`.

## 2. Water (the raster's Water class: rivers between dykes, oxbows, fishponds)

### 2.1 No free water surface texture in the CC0 catalogues

- **Poly Haven:** no water surface among its 869 textures (catalogue https://api.polyhaven.com/assets?t=textures,
  filtered on water, river, lake, pond, puddle, wave, ripple); its 521 models have no water plants (duckweed, water
  lily, reed) either (https://api.polyhaven.com/assets?t=models).
- **ambientCG:** none among its 2038 materials (https://ambientcg.com/api/v2/full_json?type=Material&limit=2500&include=tagData):
  only frozen water (Ice001-Ice004), sea foam (Foam001-Foam003), a muddy puddle ground (Ground050) and smudge overlays.

### 2.2 What the Water class must look like (written sources)

- **The Tisza is "szőke" (blond)**: "A szőke jelző a Tisza színére utal, ami pedig a folyó vizében lebegő homok és
  iszap következménye" (the blond epithet refers to the river's colour, which comes from sand and silt suspended in
  its water); "zavaros" (muddy) is among its usual epithets. Mid-water channel width 191-236 m; the 19th-century
  regulation made 136 km of new channel and left 589 km of dead arms. https://hu.wikipedia.org/wiki/Tisza (part read
  from offset 100000). English: "blonde", "muddy": https://en.wikipedia.org/wiki/Tisza
- **Oxbows (holtág, morotva)** "gradually silt up and become overgrown" ("apránként feliszapolódik és
  benövényesedik"); the Szarvas-Békésszentandrás Holt-Körös is about 30 km long, 4.5 million m³, Hungary's fifth
  largest standing water. https://hu.wikipedia.org/wiki/Holt%C3%A1g
- **Fishponds** are built to be drained completely: a drain sluice with a fish pit behind it, where the fish are
  collected when the pond is fully drained (https://hu.wikipedia.org/wiki/Halast%C3%B3). Szeged's Fehér-tó fish farm:
  from 1969 sixteen ponds and 83 wintering basins, **1.25 m deep**, 1,324 ha; 557 ha of "New Lakes" added in 1982
  (https://hu.wikipedia.org/wiki/Szegedi_Feh%C3%A9r-t%C3%B3). The Biharugra ponds are nearly 1,900 ha, Hungary's
  second largest, with reed beds (https://hu.wikipedia.org/wiki/Biharugrai-halastavak). **The month of the autumn
  harvest (lehalászás) was not found on a page read in this pass**; that ponds stand drained in October is common
  knowledge of Central European carp farming but still needs a written source before a mission's ponds are drawn
  empty.
- **Fishpond water is turbid**: feeding carp spit out sediment, "which contributes to water turbidity", and carp
  farming is criticised as a driver of eutrophication; central European carp ponds yield 0.3-1 t/ha
  (https://en.wikipedia.org/wiki/Common_carp). Algae give water a green colour, tannins dark brown, suspended silt
  the river's own colour (https://en.wikipedia.org/wiki/Color_of_water).
- **Pure water's absorption** (Pope and Fry 1997): 0.0000922 /cm at 450 nm, 0.000565 at 550, 0.0034 at 650, 0.00624
  at 700, that is **0.0092, 0.057 and 0.34 per metre** for blue, green and red
  (https://omlc.org/spectra/water/data/pope97.txt).
- **Fresnel**: water's IOR 1.3325 (S6) gives F0 = ((n-1)/(n+1))² = 0.020; with Unreal's Specular 0.5 = about 4 %
  (S5), water's **Specular ≈ 0.25**.

### 2.3 How Unreal draws it (local engine and docs)

- **Single Layer Water shading model** (UE 5.8 docs): blend mode Opaque or Masked, shading model SingleLayerWater, a
  Single Layer Water Material output with Scattering Coefficients, Absorption Coefficients (the page says "expressed
  in reciprocal meters" and gives the example R 0.0033, G 0.0016, B 0.0011), PhaseG and Color Scale Behind Water;
  refraction is read from scene colour and depth below the surface; reflections come from a tiled SSR pass composited
  with reflection captures and the sky; a downsampled scene colour can be used for speed.
  https://dev.epicgames.com/documentation/en-us/unreal-engine/single-layer-water-shading-model-in-unreal-engine .
  The page's example red absorption (0.0033) equals pure water's 650 nm value **per centimetre** (0.0034 /cm above),
  not per metre: the unit must be checked in the editor (the plugin ships a `Debug_Absorption` material for this).
- **The Water plugin** in this install: `D:/UE_5.8/Engine/Plugins/Experimental/Water/Water.uplugin`,
  `"IsExperimentalVersion": true`, `"EnabledByDefault": false`. Its surface materials
  (`Content/Materials/WaterSurface/`): `Water_Material`, `Water_Material_River`, `Water_Material_Lake`,
  `Water_Material_Ocean`, `Water_Material_CustomMesh`, `Water_FarMesh`, river-to-lake and river-to-ocean
  transitions, LOD versions; each is `MSM_SingleLayerWater`, `BLEND_Masked` (strings in the assets), and River, Lake
  and the base material reference `T_Water_TilingNormal_With_Height_02_Softened`. The normals folder also has
  `T_Water_TilingNormal_Waves_02`; foam normals and a river flowmap normal sit beside them. `WaterExtras` (samples,
  "Not intended to be used directly in a shipping product") and `WaterAdvanced` are experimental too.
- Water bodies (river, lake, ocean) are splines that carve a **Landscape** through a brush and need the Landscape's
  Edit Layers; a **Custom** water body takes its shape from a Static Mesh and does not carve the terrain
  (https://dev.epicgames.com/documentation/en-us/unreal-engine/water-body-actors-in-unreal-engine ; overview
  https://dev.epicgames.com/documentation/en-us/unreal-engine/water-system-in-unreal-engine). SAM3D has no
  Landscape: its rivers are ribbons in `ASimMapActor::BuildStrips` (procedural mesh, UVs along the strip at one unit
  per 10 m and 0-1 across; banks 6 m each side), its lakes polygons in the country map. So the water bodies' spline
  workflow does not fit; the **shading model and the textures** do.

### 2.4 Starting parameters per kind of water (estimates to tune against the photos)

No measured Secchi depth, suspended-sediment concentration or water colour of the Tisza, its oxbows or the fishponds
was found on a page read in this pass. Starting values, all **estimates**, in per-metre units (convert if the editor
check shows per-centimetre):

| Kind | Absorption (R, G, B) /m | Scattering (R, G, B) /m | PhaseG | Notes |
|---|---|---|---|---|
| River (Tisza, Körös, Maros: "szőke") | 0.6, 0.7, 1.6 | 3.0, 2.4, 1.3 (the silt's colour: the loess's 10YR 6/4 hue, section 1.5) | 0.3 | Opaque-looking within 0.5-1 m; flow along the ribbon's U; the normal panned along it. |
| Oxbow (holtág) | 0.6, 0.5, 1.2 | 0.4, 0.6, 0.3 | 0.2 | Still, dark, green-brown; a floating-plant mask near the banks (no free texture found). |
| Fishpond (1.25 m deep) | 0.7, 0.5, 1.4 | 1.2, 1.6, 0.7 | 0.2 | Green-brown, turbid from carp and plankton; wind ripples only. |
| Drained fishpond (autumn, see 2.2) | n/a | n/a | n/a | Not water: the pond-bed ground layer (items `tex_pond_bed_*`). |

Pure water's own absorption (0.34, 0.057, 0.0092 /m) is the floor of every row. Specular 0.25. Roughness 0.02-0.08
by wind (ripples come from the normal).

## 3. Sand: the Kiskunság and the Nyírség

Written basis: black pine was planted in Hungary from the end of the 19th century, first on the dolomite barrens and
then on the Great Plain's sand, to bind the calcareous sand soils; two great planting waves in the 1950s and 1970s;
stands are too dense, 8-12 thousand stems per hectare, 89.5 % black pine, so little else grows under them
(https://hu.wikipedia.org/wiki/Feketefeny%C5%91). Blown sand is mostly quartz; calcite or gypsum sands are much
rarer; it occurs in the Duna valley, Somogy and Tolna, the Kisalföld, the Nyírség and the Danube-Tisza interfluve; the
last moving dune is at Fülöpháza (https://hu.wikipedia.org/wiki/Fut%C3%B3homok). The Kiskunság's soils are "mostly
sandy or saline, with meadow soils too", its woods "mostly of plantation origin"
(https://hu.wikipedia.org/wiki/Kiskuns%C3%A1g); the national park has open sand grassland with juniper at Orgovány,
and dunes, sand steppes, saline lakes and marshes at Bugac (https://hu.wikipedia.org/wiki/Kiskuns%C3%A1gi_Nemzeti_Park).
The Nyírség's southeast has asymmetric parabolic dunes 2-15 m high; black locust replaced most of its birch and oak
(oak only on 10-12 % now) (https://hu.wikipedia.org/wiki/Ny%C3%ADrs%C3%A9g). So the map needs three sand
surfaces: bare sand (dunes, sandy tracks, blow-outs), the floor of a black pine plantation on sand, and sandy
ploughland (section 1.5, the existing ploughed-soil scans retinted).

Thumbnails looked at (the sources' own 256 px previews): ambientCG Ground053, Ground058, Ground080, Ground084,
Ground087, Ground088, Ground102, Ground103; Poly Haven forrest_sand_01, dense_sand, sand_01, park_sand,
aerial_sand, raked_dirt.

| Layer | Primary | Alternate | Notes |
|---|---|---|---|
| Bare blown sand, sandy track | **ambientCG `Ground088`** (https://ambientcg.com/view?id=Ground088): photogrammetry (Aug 2025), 1-8K, color, displacement, normal, roughness, AO; tags brown, dirt, ground, sand. Thumbnail: pale, fine, dry, matte beige sand with small specks, no shells or ripples. | **Poly Haven `dense_sand`** (https://polyhaven.com/a/dense_sand): 1.8 m, 8K (diff, nor_dx, nor_gl, rough, ao, disp, arm), Dimitrios Savva; "compacted tan sand with crusted clumps, shallow pits". | Ground088's scan size is not stated in the API (0): measure before tiling. Retint to 10YR 6.5/2 (section 1.5): Ground088 is yellower than the calcareous Kiskunság target. Ground087 (same series) has long tyre-like grooves (sandy track). |
| Sand, far layer and military sand ground | **Poly Haven `aerial_sand`** (https://polyhaven.com/a/aerial_sand): 15 m, 8K, Rob Tuytel; "tan, compacted ripples with tire and foot ruts". | **Poly Haven `aerial_beach_02`** (https://polyhaven.com/a/aerial_beach_02): 20 m, 8K, Rob Tuytel; "flat coastal sand with fine grains, moist dark patches and subtle ripple patterns". | Both are coastal: grey them toward the target and mask the wet patches. 8K over 15-20 m is 410-550 px/m, a far or blend layer only. |
| Black pine plantation floor on sand | **ambientCG `Ground053`** (https://ambientcg.com/view?id=Ground053): photogrammetry (Apr 2022), 1-16K; tags forest, ground, leaves, sand. Thumbnail: red-brown needle and bark litter over pale sand. | **Poly Haven `forrest_sand_01`** (https://polyhaven.com/a/forrest_sand_01): 2.0 m, 8K (+bump, spec), Rob Tuytel; "granular sandy soil with compacted patches, scattered pine needles, twigs and organic litter". | A dense black pine stand drops a closed needle carpet: Ground053's cover is closer; forrest_sand_01 for the stand's edges and rides. ambientCG `Ground084` (yellow sand with roots and needles, 1-8K) is a third choice for the Nyírség's warmer sand. |

## 4. October's newly sown winter wheat

Written basis: winter wheat is sown in October (`references.md` T11, https://trademagazin.hu/en/keszul-a-vetomagszektor-az-oszi-kalaszosok-szeptemberben-kezdodo-vetesere);
Hungarian lore put the best sowing week as the one containing 4 October ("búzahét", Francis week)
(https://hu.wikipedia.org/wiki/Vet%C3%A9s); in the northern hemisphere winter wheat is planted September to
November and must establish itself before winter dormancy (https://en.wikipedia.org/wiki/Winter_wheat). A drill sows
"in well-spaced rows at specific depths" so a drilled field "is much more uniform, typically in rows"
(https://en.wikipedia.org/wiki/Seed_drill); the coulters cut grooves "several centimetres deep" and a trailing
harrow closes them (https://de.wikipedia.org/wiki/Drillsaat). The Russian article on winter wheat recommends narrow
rows ("междурядья не более 10 см", rows no more than 10 cm apart) and sowing 6-7 cm deep on chernozem
(https://ru.wikipedia.org/wiki/%D0%9E%D0%B7%D0%B8%D0%BC%D0%B0%D1%8F_%D0%BF%D1%88%D0%B5%D0%BD%D0%B8%D1%86%D0%B0).
**Not found on a page read:** Hungarian row spacing of the 1980s (12-15 cm is the usual cereal figure, unconfirmed),
days to emergence, and the leaf stage by mid-October.

So a mid-October winter wheat field is, in two states: (a) **just drilled**: a fine, rolled seedbed, darker and
smoother than ploughland, faint parallel lines; (b) **emerged**: the same seedbed with thin green rows 10-15 cm apart
(cover growing from 0 toward perhaps 10-20 % by late October: estimate), along the parcel's heading. No free scan of
either was found (Poly Haven: no sprout, seedling or crop-row texture or model; ambientCG: none tagged so).

| Layer | Primary | Alternate | Notes |
|---|---|---|---|
| Winter wheat seedbed (drilled) | **Poly Haven `raked_dirt`** (https://polyhaven.com/a/raked_dirt): 2.0 m, 8K (diff, nor_dx, nor_gl, rough, ao, disp, arm); photo Dimitrios Savva, processing Dario Barresi; "compact sandy soil with fine rake grooves". Thumbnail: dark grey-brown, fine parallel grooves. | **ambientCG `Ground103`** (https://ambientcg.com/view?id=Ground103): photogrammetry (Jan 2026), 1-8K; brown earth with small green specks (thumbnail). | raked_dirt's grooves stand in for the drill lines: rotate them to the parcel's furrow heading like `farm_furrows`. Retint to the chernozem dry target (section 1.5). Ground103's green specks read as emerging seedlings but are scattered, not in rows: use it where a field was broadcast or for a weedy seedbed. Its scan size is not stated. |
| Seedling rows (emerged) | No third-party pick: draw them in the material, a green line every 0.10-0.15 m along the heading, its width and cover a per-parcel growth parameter, with a wind-free normal bump; near the camera, small blade cards generated by the project (licence `Own`). | | The `pbr_textures.md` note on stubble rows applies: row direction from simcore's heading per 3 km square. |

## 5. Items returned (summary)

| id | Layer | Pick | Licence | Redistributable |
|---|---|---|---|---|
| tex_water_slw_river_ue | water: rivers, oxbows, ponds | Water plugin `Water_Material_River` / `_Lake` and `T_Water_TilingNormal_With_Height_02_Softened` (UE 5.8) | Epic-Content-Only | no |
| tex_water_normal_waves_ue | water: still water's ripples | Water plugin `T_Water_TilingNormal_Waves_02` | Epic-Content-Only | no |
| tex_sand_bare_01 | sand: bare, tracks | ambientCG Ground088 | CC0-1.0 | yes |
| tex_sand_bare_02 | sand: bare | Poly Haven dense_sand | CC0-1.0 | yes |
| tex_sand_far_aerial_01 | sand: far layer, military sand | Poly Haven aerial_sand | CC0-1.0 | yes |
| tex_sand_far_aerial_02 | sand: far layer | Poly Haven aerial_beach_02 | CC0-1.0 | yes |
| tex_sand_pine_litter_01 | black pine floor on sand | ambientCG Ground053 | CC0-1.0 | yes |
| tex_sand_pine_litter_02 | black pine floor on sand | Poly Haven forrest_sand_01 | CC0-1.0 | yes |
| tex_sand_pine_litter_03 | warmer (Nyírség) sand with roots | ambientCG Ground084 | CC0-1.0 | yes |
| tex_winter_wheat_seedbed_01 | October winter wheat, drilled | Poly Haven raked_dirt | CC0-1.0 | yes |
| tex_winter_wheat_emerged_02 | October winter wheat, emerging | ambientCG Ground103 | CC0-1.0 | yes |
| tex_pond_bed_wet_01 | drained fishpond bed, fresh | Poly Haven brown_mud_03 | CC0-1.0 | yes |
| tex_pond_bed_dry_02 | drained fishpond bed, dried | Poly Haven mud_cracked_dry_riverbed_002 | CC0-1.0 | yes |
| tex_pond_bed_algae_03 | drained pond bed with algae film | ambientCG Ground083 | CC0-1.0 | yes |

Licences: Poly Haven CC0 (https://polyhaven.com/license), ambientCG CC0 (https://docs.ambientcg.com/license/), both
confirmed in `pbr_textures.md` section 2. Download patterns are those of `pbr_textures.md` section 3 (Poly Haven
`https://api.polyhaven.com/files/<id>`, ambientCG `https://ambientcg.com/get?file=<id>_2K-JPG.zip`); the map lists
above were read from those APIs on 3 Nov 2026 (`https://api.polyhaven.com/files/<id>`,
`https://ambientcg.com/api/v3/assets?id=<ids>&include=maps,downloads,dimensions,tags`).

## 6. Development process (how the targets get used)

1. **Targets as data, each with its source** (the project's rule for data values): a `soil_targets` block in
   `data/scenery/map_dressing.json` (or its own file) with, per ground (chernozem, meadow, loess, sand_calcareous,
   sand_humous, sand_nyirseg, saline, peat), the Munsell notation, the linear dry and moist base colour from section
   1.5, the source (S1 profile code, S2 clause) and a confidence (`sourced`, `bounded`, `estimate`).
2. **Calibrate each scan by its mean only**: after a texture is fetched (with the owner's say-so, ASSETS.md), a small
   tool reads its diffuse at 1K, computes the mean linear colour (and luminance), and writes a per-layer gain (or a
   Lab shift) that brings the mean to the target while keeping the scan's own variation; the material instance reads
   it. A scan whose texture variation is wrong for the ground (shells, coastal ripples) is masked, not retinted.
3. **Wetness** from the weather: lerp the dry target toward the moist one (two Munsell value steps, S2) and lower
   roughness; furrow bottoms and ditch lines wet first.
4. **Water**: an own `M_SimWater` (SingleLayerWater, Masked) on the river ribbons (normal panned along U, the strip's
   flow) and on the lake and pond polygons as flat procedural meshes at their level; per-kind parameter sets from
   section 2.4 in a data file; the normal from the Water plugin's texture (plugin enabled, Epic-Content-Only, not
   redistributable) **or** a normal the project generates itself (as `tools/fx_textures.py` already does for the
   effects), which avoids depending on an experimental plugin. Check the absorption unit with the plugin's
   `Debug_Absorption` first. Far water (beyond the SLW material's range) keeps a vertex colour that is the river's
   silt-grey, not blue.
5. **Season state per parcel** from simcore: wheat parcels in October are stubble, ploughed, drilled or emerged;
   drilled and emerged use section 4's layer with a growth parameter (0 = just drilled). Fishponds: full or drained,
   per scenario date, once the harvest month has a written source.
6. **Check against the references**: for views V1, V8 and V9 (`references.md`) sample region means (ploughland,
   stubble, sand, river) in the render and in the reference photos, white-balanced on a neutral (asphalt or
   concrete) in each photo, and compare with the targets: the photos test the pattern, the targets the mean.

## 7. What is still open

- **Chernozem, meadow soil and sand colours are bounded, not measured**: ISRIC's Hungarian chernozems (HU008-HU010),
  meadow soil (HU017) and blown sand (HU001) carry no colour in its database. The National Atlas of Hungary's soil
  chapter (https://www.nemzetiatlasz.hu/MNA/National-Atlas-of-Hungary_Vol2_Ch6.pdf, 27.7 MB, too large to read here)
  and Hungarian soil-survey descriptions are the next places to look for Munsell colours of the Alföld's soils.
- **Water**: no measured colour, turbidity or Secchi depth for the Tisza, its oxbows or the fishponds; the month of
  the fishponds' autumn harvest; the Single Layer Water absorption unit (docs say per metre, the example values look
  per centimetre).
- **Winter wheat**: the 1980s Hungarian row spacing, days to emergence and leaf stage by mid-October.
- **No free water-plant texture** (duckweed, water lily) for oxbows, and no free free-standing sand-grassland
  (Festuca tussocks on sand) surface: blend Ground088 with Poly Haven `withered_grass` by a mask, tussocks as geometry.
- **Fab was not searched** (search budget spent): Megascans' free tier may hold a seedbed, sand-with-grass or
  water-normal scan; check in a browser with "Free" and "Standard License".
- Scan sizes not stated by ambientCG for Ground053, Ground083, Ground084, Ground088, Ground103: measure before tiling.
