# Built models: free buildings, infrastructure and props for Pannonia 1985

Research notes for the terrain roadmap (SAM3D, Unreal Engine 5.8). Scope: free 3D models of buildings,
infrastructure and props for a 1985 Hungarian Great Plain countryside, its villages and small towns, the
battery's positions and the roads between. Written 3 Nov 2026. Pages were read only; no asset file was
downloaded. Every pick below is free at the time of reading and carries one of the four licences the
project accepts (CC0-1.0, CC-BY-4.0, Fab-Standard, Epic-Content-Only); anything else is called out.

How this fits the project: `docs/scenery/ASSETS.md` and `data/scenery/assets.json` already define the register
(part keys `house`, `barn`, `flats`, `industrial`, trees; non-redistributable files under
`unreal/SAM3D/Content/ThirdParty/`, git-ignored; `tools/check_assets.py`). The register is empty today: every
building is procedural (`simcore/scenery/map_meshes.hpp`). The items here are candidates for that register,
not decisions.

## 1. Licences as found (read on the pages named)

| Licence as shown | What it allows | Register value | Redistributable in git |
|---|---|---|---|
| Poly Haven: CC0 | "use for any purpose, including commercial", no attribution required - https://polyhaven.com/license | CC0-1.0 | yes |
| Sketchfab "CC Attribution" | Creative Commons Attribution 4.0: any use, redistribution allowed, credit the author - https://help.sketchfab.com/en/articles/16152217 | CC-BY-4.0 | yes, with credit |
| Sketchfab "CC0 / Public Domain" | no conditions - same page | CC0-1.0 | yes |
| Sketchfab "Free Standard" | the Sketchfab Standard licence on a free model: all uses incl. commercial, no credit; standalone redistribution of the files is not allowed (same page; license filters introduced https://sketchfab.com/blogs/community/refine-downloadable-model-searches-with-new-license-filters) | other (treat like Fab-Standard) | no |
| Sketchfab "CC Attribution-NonCommercial", "-NoDerivs", "-ShareAlike" | NC and ND are excluded by the task; SA would bind the game | excluded | - |
| Fab "Standard License" | use in projects; no redistribution of the asset files on their own; Personal and Professional tiers are price tiers with the same rights, Professional required above USD 100k gross revenue in 12 months - https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab, https://forums.unrealengine.com/t/how-to-determine-personal-vs-professional-license/2082366, https://forums.unrealengine.com/t/clarification-on-fab-standard-license-epic-reply-please/2083602 | Fab-Standard | no (ThirdParty folder) |
| Fab "CC BY 4.0" listings | as CC-BY above (licence chosen by the seller on Fab) - https://dev.epicgames.com/documentation/fab/licenses-and-pricing-in-fab | CC-BY-4.0 | yes, with credit |
| Fab "UE-Only Content" / "UE Marketplace License" (Epic samples, migrated Marketplace items) | Epic Content License: only inside Unreal Engine products - same page | Epic-Content-Only | no |
| Quixel Megascans on Fab | all Megascans free until 31 Dec 2024; since 1 Jan 2025 a selection of 1500+ stays free on Fab under the Standard licence - https://80.lv/articles/megascans-no-longer-free-after-2024/ ; assets claimed on Bridge before then stay usable under the old Epic terms - https://forums.unrealengine.com/t/access-legacy-megascans-on-bridge-and-quixel-com/2142664 | Fab-Standard | no |
| Blendkit (BlenderKit renamed June 2026 - https://www.blenderkit.com/articles/goodbye-blenderkit-hello-blendkit/) "Royalty Free" | use in games allowed if the asset cannot easily be extracted; no reselling; redistribution not addressed; CC0 is a separate, smaller set - https://www.blendkit.com/docs/licenses/licensing-faq/, https://www.blenderkit.com/articles/our-position-on-cc0-models/ | other (concern) | no |

Fab price trap: several Fab listings show "Personal: free, Professional: paid" ("From Free"). They are free only
while the project stays under the Professional threshold. Listings flagged like this below carry a concern. Some
Megascans listings show "UEFN - Reference only: 0, Personal: $..." which means paid for Unreal Engine; they were
avoided.

## 2. How the search was done

- Sketchfab: the public search API, downloadable models only, metadata only:
  `https://api.sketchfab.com/v3/search?type=models&downloadable=true&count=24&q=<term>` (about 180 terms in
  English, Russian, Hungarian, Czech and Polish; 1793 distinct results merged). Licence, author and face count
  come from the search records. The per-model API (`https://api.sketchfab.com/v3/models/<uid>`) was read for 70
  shortlisted models and then answered 429 Too Many Requests, so the remainder rely on the search record.
  Page link for any model: `https://sketchfab.com/models/<uid>` (redirects to its page).
- Fab: fab.com refuses scripted requests (Cloudflare challenge); it was not bypassed. Listings were read in the
  built-in browser through the site's own JSON endpoints, `https://www.fab.com/i/listings/search?is_free=1&listing_types=3d-model&q=<term>`
  and `https://www.fab.com/i/listings/<uid>`. Page link: `https://www.fab.com/listings/<uid>`.
- Poly Haven: `https://api.polyhaven.com/assets?t=models` and `https://api.polyhaven.com/info/<slug>`.
  Page link: `https://polyhaven.com/a/<slug>`. Files listed by `https://api.polyhaven.com/files/<slug>`.
- Blendkit: its public search API (no account); free results checked against the licence FAQ above.
- The web search budget for this turn ran out before the end of the work; claims below that would need an
  additional web search (for example Hungarian sign standards) are marked unsourced or left to the gaps.

## 3. Hungarian references used for fit

- The Kadar cube (kockahaz): square plan near 100 m2, single storey, hipped (tent) roof, street front of two window
  axes, built from the early 1960s to the late 1970s; L-plan and attic variants late in the period; more than
  800,000 still standing - https://welovebudapest.com/cikk/2022/12/08/latnivalok-es-kultura-kadar-kocka-epulet/,
  https://telex.hu/gazdasag/2021/04/24/kadar-kocka-kutatas,
  https://www.penzcentrum.hu/hellovidek/20220324/kadar-kocka-panel-lakopark-kiderult-milyen-otthont-vesz-most-a-legtobb-magyar-1168242,
  https://www.new-east-archive.org/features/show/13144/the-kadar-cube-comeback-how-the-humble-1960s-hungarian-home-inspired-high-fashion-and-eco-friendly-living,
  https://en.mandadb.hu/cikk/1285342/Kadar_cubes_blocks_of_houses_in_the_countryside, and the Ludwig Museum's
  booklet https://ludwigmuseum.hu/system/files/news/attachments/2024-08/KMK_fu%CC%88zet%20online_egybe.pdf.
- Low-voltage overhead lines (KIF) on 8-10 m poles; perforated-web (attort gerincu) reinforced concrete poles;
  pole types and tagging - https://www.tettyeforrashaz.hu/palyazatok-beszallitoknak/palyazat/csatolmanyok/cu9j7SSkV15KRNBdquNuOorBqIizSBL0,
  https://www.hte.hu/documents/4652308/4684563/10_2_Marcz_Laszlo.pdf,
  https://wiki.openstreetmap.org/wiki/Template:HU:Map_Features:power.
- Hungarian transmission tower families (the "Fenyo", "Duna" and others) - https://lakbermagazin.hu/design/977-formabonto-tavvezeteki-oszlopok-a-magyar-villamosenergia-atviteli-halozaton.html.

None of the free models found is Hungarian in origin except "House" by Gardony (a Hungarian author, generic
house), "Church of Jak" (a Romanesque abbey church, untypical of a plains village) and "Hungarian flooded
chapel". The closest substitutes are Czech, Slovak, Polish, Ukrainian and Russian: same building industry era,
similar panel systems, concrete fences and poles. Each pick says what differs.

## 4. Picks by category

Format: name - author (display name) - site - licence - size - page. "Primary" is the first choice for a
placeholder; "alternate" is the fallback or a kitbash source. CC-BY items need a credit line in the game's
credits and the register's `author` field.

### 4.1 Panel blocks, 4-10 floors (part key `flats`)

Hungarian towns of the 1980s had 4-5 storey walk-ups and 10-storey panel blocks from the house factories. The free
models are Czechoslovak and Soviet types; the Czech panelak is the nearest in look.

- PRIMARY: Panelak (communist era prefab building) - afbu (AfBu) - Sketchfab - CC-BY-4.0 - 7,850 faces -
  https://sketchfab.com/models/ab53faf1c0bb4313bd816bb1997cd27c . Classic Czechoslovak prefab, low-poly; good
  for the town's 8-12 storey blocks at distance.
- PRIMARY: 9-floor pannel building - Randonavt - Sketchfab - CC-BY-4.0 - 2,142 faces -
  https://sketchfab.com/models/0864d492795141ae98277ac564e3a657 . Nine floors, ceramic tile cladding, made from
  references (author's description).
- alternate: 121-60-25 panel building - Randonavt - Sketchfab - CC-BY-4.0 - 4,616 faces -
  https://sketchfab.com/models/ff12c3fb24c94d908bb9f48227e4b50f (nine floors, expanded clay concrete panels).
- alternate: Soviet panel house built in the 1970s - UnkaII (Unka II) - Sketchfab - CC-BY-4.0 - 10,068 faces -
  https://sketchfab.com/models/94996e8cf4a8452d91ec1e206b5b4137 (five floors, eight entrances; the small town's
  walk-up blocks).
- alternate: Small Soviet Apartment Building - Khrushchevka - AnatolApsik - Sketchfab - CC-BY-4.0 - 3,903 faces -
  https://sketchfab.com/models/feb9bd78483a4b71a13f3a6327aa0e2b .
- alternate: Khrushchevka USSR Building - koshakmi (Anthony36) - CC-BY-4.0 - 7,134 faces -
  https://sketchfab.com/models/a9b9f378081e456b8b5e221a8eb1546e .
- alternate: Panel house, 9 floors - hasped0 - CC-BY-4.0 - 760 faces - https://sketchfab.com/models/c3025d769d5647998e8bc897ab23d4d4 (far LOD).
- alternate: Soviet Residential Building - Ivan-Kanaev - CC-BY-4.0 - 4,554 faces - https://sketchfab.com/models/476d3e0ab446459992e876ed66cd535f (author: background use).
- alternate: Soviet Panel House [Lowpoly] - ivanrytp31 - CC-BY-4.0 - 12,389 faces - https://sketchfab.com/models/9db5ad5379534086834ad0bef89db33a ; Big Soviet Panel House [Lowpoly] - same author - 7,814 faces - https://sketchfab.com/models/a3508b75378948669c3155678b87f25b .
- alternate: LOW POLY - SOVIET APARTMENT BUILDING 8K - Colin.Greenall - CC-BY-4.0 - 10,392 faces, 8k textures - https://sketchfab.com/models/05229ac1d1f94e6c8cacaad91110c602 .
- alternate: Russian panel house (asset) - Tim0 (Yury Misiyuk) - CC-BY-4.0 - 34,569 faces, 1024 textures - https://sketchfab.com/models/d537595db4bf4a60b955b31cde198432 .
- alternate: Brezhnev House (Brezhnevka) - apolllo6 - CC-BY-4.0 - 24,228 faces, block system "any length and height" - https://sketchfab.com/models/6449c71152c1451387094058571bc9f5 .
- alternate (concern): Detailed 12 storey panel apartment building - alwayshasbean - CC-BY-4.0 - 3,443 faces -
  https://sketchfab.com/models/cb7064bec48845fea62a830b2c692fb1 . Concern: the author says the main texture came
  from texturelib.com; the texture's own terms are not the model's licence.
- alternate: Soviet Panel House (Svechka) - onedev46 - CC-BY-4.0 - 11,520 faces - https://sketchfab.com/models/4dc0bd374ce840b4a63c62847aed332d (Pripyat tower; Hungarian towns had few towers).
- alternate: nine-story building of the post soviet - ITG Models - Fab - CC-BY-4.0 - https://www.fab.com/listings/bc387245-1687-44b9-a37f-05a3bbabc327 .
- alternate: Panel House | Tile Texture - CATholic - Fab - CC-BY-4.0 - https://www.fab.com/listings/e29ba312-aa85-4819-b208-19bdc3fbdec5 .
- kitbash: Modular Urban Apartments Facade - James Ray Cock - Poly Haven - CC0 - 118,215 polys, 51.5 x 6.7 x 17 m - https://polyhaven.com/a/modular_urban_apartments_facade (older masonry facade; style check needed).
- tool: UNIBLOCKS FREE - Lex4art - Fab - Fab-Standard (Personal 0, Professional 0) - published 8 Jul 2025 -
  https://www.fab.com/listings/e36c2bc1-49d4-4918-a0d7-f09a90ec7a57 . A modular building framework inside UE;
  could build panel and kocka facades from the project's own textures.

### 4.2 Village houses: the kocka house (part key `house`)

No free model of a Kadar cube was found. The nearest:

- PRIMARY (placeholder): Ordinary Slovak/Slavic Village House - matusgls8 (matusgls) - Sketchfab - CC-BY-4.0 -
  35,596 faces - https://sketchfab.com/models/35bb639c11a4496ea3b3061070101861 . Author: "you can see this house in
  every village in slovakia". Square socialist-era family house with render; the closest silhouette to the kocka
  house. Differs in roof and window rhythm.
- alternate: Low-poly Brick House - Vitalii.Sandula - CC-BY-4.0 - 1,911 faces - https://sketchfab.com/models/ea82ea891829493aadeea889cd473e89 (one-storey brick house with veranda, "1990s rural style").
- alternate: Village House USSR CIS (Dom Derevnya SSSR SNG) - Ely Matew - Fab - CC-BY-4.0 - https://www.fab.com/listings/b0a5789a-8305-4f78-9432-ea9fb24976c3 (two-family village house).
- alternate: Post soviet house - Yokothy - Fab - CC-BY-4.0 - modular, 24,128 tris - https://www.fab.com/listings/b0a76790-9f97-496f-aa30-f8560514c08a .
- alternate: Postsoviet Ukrainian house - taras.yaremenco - CC-BY-4.0 - 9,746 faces - https://sketchfab.com/models/728e3581ff724666bc7b5b3304e441e1 .
- alternate: soviet village house - SnowziN2 - CC-BY-4.0 - 25,774 faces - https://sketchfab.com/models/bed5014e64364d61aaf3bde70572d03f .
- alternate: House - Gardony - CC-BY-4.0 - 1,108 faces - https://sketchfab.com/models/0f07dea04a154f27abf674a308d1e135 (Hungarian author; "separate elements that can be replaced"; generic).
- Recommendation: build the kocka house procedurally (gap list), using one of the above only until then.

### 4.3 Peasant long houses (hosszuhaz) and farmsteads (part keys `house`, `barn`)

- No free Hungarian long house with a tornac (arcaded porch) and gable to the street was found.
- reference/kitbash: XIX century village house porch - Zygomir - Fab - CC-BY-4.0 - https://www.fab.com/listings/ee3c097f-e66b-490b-953d-70b765d2fdd7 (Polish open-air museum scan; a porch close to the tornac).
- PRIMARY (farmstead): Rural Brick House with Barn and Chicken Coop - Vitalii.Sandula - CC-BY-4.0 - 4,318 faces -
  https://sketchfab.com/models/4708a315b2ff42d08c55c6f5d5ca7c21 ; the same model is on Fab by Luvisan, CC-BY:
  https://www.fab.com/listings/567c4ed8-12cf-4c46-9cf5-1c52f936c5dc . Author: "inspired by Eastern European
  countryside architecture".
- alternate: Old vilage house_LOD_0 - tomich516 (Nikolay Fedorov) - CC-BY-4.0 - 9,500 faces, with LODs - https://sketchfab.com/models/065cb734d68446ec90e3b43da86a5530 .
- alternate: Old Village House - SnowziN2 - CC-BY-4.0 - 7,058 faces - https://sketchfab.com/models/33d41e5097a64ebfb29f0d4ecfb98456 .

### 4.4 Barns, sheds, garages (part key `barn`)

- PRIMARY: Abandoned Soviet barn - matveysasin500 (Lepinsm) - CC-BY-4.0 - 11,860 faces - https://sketchfab.com/models/5b595c2b4df0467e96137d25021a0802 (a Soviet-era barn or warehouse, built from a real one per the author).
- PRIMARY: Soviet Brick Storage / Garage (Low Poly) - mindchamber93 - CC-BY-4.0 - 3,202 faces - https://sketchfab.com/models/c1e93ea3b27747d2a05f28c59d426600 (rows of brick sheds and garages behind the blocks).
- alternate: Old Wooden Shed - donnichols - CC-BY-4.0 - 18,278 faces, enterable - https://sketchfab.com/models/761c0879b18041e1a42bc10de88d2228 .
- alternate: Old wooden barn - mitin41401 (Elizabeth Watts) - CC-BY-4.0 - 23,968 faces - https://sketchfab.com/models/4887c7e3de244a9db3eb5b4974418b1b .
- alternate: Old Wooden Barn - Erwin's Studio - Fab - CC-BY-4.0 - https://www.fab.com/listings/18878d38-8287-4d2f-b9ef-633e2bbb8d64 ; Livestock Stable - same - https://www.fab.com/listings/982b4d6e-bc65-4bb0-910f-ad8c98d5991d (style to check: likely Western).

### 4.5 Collective farm (TSZ / state farm) buildings

- Cow sheds, machine halls: no free model of the long brick or concrete TSZ cow shed with a ridge vent, or of an
  open machine hall with corrugated asbestos-cement roof, was found. Gap (procedural).
- Halls as placeholders: Warehouse - Quixel Megascans - Fab - Fab-Standard (free) - https://www.fab.com/listings/a3149fab-3906-4043-b6ee-3937b752a06c ;
  Free sample Warehouse & Storage Vol 01 - Oleg Ushenok - Fab - Fab-Standard - https://www.fab.com/listings/683de298-9ce5-4d85-b8bd-dbdf47f83304 .
- Grain bins and silos:
  - PRIMARY: Grain Silo - Low Poly Game Ready - thegobby (Gobby) - CC-BY-4.0 - 230 faces - https://sketchfab.com/models/270b9d405fab4d0fbb6fd29b43e237f2 ; same on Fab, CC-BY: https://www.fab.com/listings/a9727fb7-c135-4abf-9fe8-e51d86d88a36 (a rusted steel bin).
  - alternate: Grain Elevators LowPoly - kantaphon.phum - CC-BY-4.0 - 3,326 faces - https://sketchfab.com/models/57c89263dd6241eda9883ea1b732c487 .
  - alternate (heavy, American): Concrete Grain Elevators - benjamintang2012 - CC-BY-4.0 - 469,323 faces - https://sketchfab.com/models/21c710f0406a4b109fd0c64bcf6abe1f .
- Excluded: Soviet Silo by Allen508 - https://sketchfab.com/models/073abcb12db14b7ca0a034d1ff31dfdf (the author's
  description: one of the Red Alert 1 buildings he made; fan art of a game's IP); Cowshed by MMD_SonicNewYear -
  https://sketchfab.com/models/af88768dd3d34d169e911e97fbce5649 (described only as "a game object", tagged for a
  commercial game). Farm Cow Barn by BytesCrafter (540 faces, CC-BY, Unity-optimised) -
  https://sketchfab.com/models/bbcb26e0d16944ff977b7c484cd761ac is Western in look.

### 4.6 Industrial halls (part key `industrial`)

- PRIMARY: Factory Pack Vol.1 - MeikWModels - Fab - CC-BY-4.0 - 151 meshes, UE - https://www.fab.com/listings/0c3f584b-98e6-416d-83bf-79a36de579db .
- alternate: Abandoned Building: Polygraphenwerk Leipzig - Geddart - Fab - CC-BY-4.0 - https://www.fab.com/listings/646e0010-c284-4e13-8597-2f14376f5ce4 (a scan of a GDR factory; same era and building industry).
- alternate: Modular Factory Facade - James Ray Cock - Poly Haven - CC0 - 175,014 polys, 53.3 x 5.4 x 29 m - https://polyhaven.com/a/modular_factory_facade (brick).
- not a fit: City Sample Buildings - Epic Games - Fab - UE-Only (Epic-Content-Only) - https://www.fab.com/listings/008fe959-5511-428e-93bd-f99b1179f6d5 (American city modules).

### 4.7 Church

- No free Hungarian village church (single west tower, baroque or classicist, Catholic cross or Reformed star) was found. Gap (procedural).
- alternate: Dorfkirche Warchau - Aphva - Sketchfab - CC0 - 969,288 faces (a scan; Nanite or decimation) - https://sketchfab.com/models/619e244314194871be9e173b5a7bf61f (a German village church).
- alternate: Hungarian flooded chapel - Aartee - CC-BY-4.0 - 15,691 faces - https://sketchfab.com/models/c6f2164f1b44404389962474f04a17f4 (a Hungarian chapel, Blender and Substance).
- alternate: Countryside chapel dataset - matousekfoto - Fab - CC-BY-4.0 - Capturing Reality scan, 5 x 8k - https://www.fab.com/listings/2b49b974-9a05-40ed-964e-4abe07a61d4f (roadside chapel; Czech).
- alternate: Church - Abandoned - reramil - CC-BY-4.0 - 14,202 faces - https://sketchfab.com/models/c95d57412b5c45218b244626830ad877 (Russian church; onion dome wrong for Hungary).
- not a fit: Church of Jak - gnadori - CC-BY-4.0 - 2,384 faces - https://sketchfab.com/models/c5bd51b727db4d718d3efe5c80c11f4a (the church of Jak in Hungary, built in the 15th century per the author's text; a medieval abbey church, untypical of a plains village).

### 4.8 Water towers

- No free Hungarian concrete water tower (mushroom, chalice or cylinder on a shaft) was found. Gap: build it
  with the body-of-revolution mesher in `simcore/scenery/munitions.hpp`.
- not a fit: Water Tower - Chamodp - CC-BY-4.0 - https://sketchfab.com/models/500cfe0cf4a74350bb49c5736b692339 (wooden, American);
  Old Water Tower Environment - mapleaber - https://sketchfab.com/models/3791b41608f842a3ab766b1763ebc3bd ;
  Weathered Water Tower - Planetrix23 (Pippa) - 4,968 faces - https://sketchfab.com/models/bff3a33dff32414e83fa57e650e701ce (steel, semi-modern; only as an alternate).

### 4.9 Electricity and telephone poles, wires

- PRIMARY: Modular Electricity Poles - James Ray Cock - Poly Haven - CC0 - 200,610 polys (the whole kit) - https://polyhaven.com/a/modular_electricity_poles . Wooden poles with crossarms, insulators, transformers and cable segments; redistributable; the village's low-voltage line.
- PRIMARY: Power pole (2x reinforced concrete poles) - piasecki.norbert2003 (AsXSn) - Sketchfab - CC-BY-4.0 - 17,922 faces - https://sketchfab.com/models/00e85ce2c5d745cca5eb924c527efa1e . A Polish 15 kV medium-voltage A-frame of two concrete poles; nearest to the Hungarian 20 kV lines.
- PRIMARY (telephone): Wooden Utility Pole / Derev'yanyi oporniy stovp - Amaranthus_retroflexus - CC-BY-4.0 - 7,303 faces - https://sketchfab.com/models/6daa230ee64446ff87b932ca5fc28f4f (a wooden pole on a concrete base, with brackets and insulators).
- alternate: Old Wooden Pole - Quixel Megascans - Fab - Fab-Standard (free) - 4.73 m - https://www.fab.com/listings/8980415b-6145-4a89-84e3-29ff404bf0a8 .
- alternate: Wood Utility Poles - Eleanie - Fab - CC-BY-4.0 - five variants with cables - https://www.fab.com/listings/f9234209-1878-478e-928e-56e043b341d0 .
- alternate: Sty and Telegraph Pole - Immersive-Games - Fab - CC-BY-4.0 - https://www.fab.com/listings/3a19868a-68d4-47cb-b5a0-580161377c54 .
- alternate: Soviet Environment Assets Pack - alex20010804 (Cherk) - CC-BY-4.0 - 29,714 faces, 8 models (concrete fence, power line, lighting post, curb, road signs) - https://sketchfab.com/models/3212fcb942e3445f943dfdceef3aabea .
- alternate: Power Line Pack - CGMeller - CC-BY-4.0 - 45,116 faces, made for a UE4 game, baked normal and AO - https://sketchfab.com/models/9dd9b47ae5d347cfbba1e36217cc3d07 .
- alternate: electric pole - pravda2033 - CC-BY-4.0 - 304 faces - https://sketchfab.com/models/20645d411e0c4631b8e073263f35b08a (Soviet concrete pole, far LOD).
- alternate: Telephone Pole with Wires - PolyDavid - CC-BY-4.0 - 5,200 faces - https://sketchfab.com/models/510d4e3fddfa44359d1f6595a00dcf8a .
- alternate (concern): Rural Area Old Concrete Electric Utility Pole - kuuttisiitonen (Kuutti Siitonen) - CC-BY-4.0 - 3,404 faces - https://sketchfab.com/models/f8b3249e56484746b193389914df9331 . Author: based on a pole in Half-Life 2 - a game design, though the model is his own.
- Wires: Modular Electric Cables - Kuutti Siitonen - Poly Haven - CC0 - 42,078 polys - https://polyhaven.com/a/modular_electric_cables . Recommendation (design, not sourced): draw spans procedurally as catenary tubes between the poles' insulator sockets, generated at load from the line's pole list, rather than cable components; the Poly Haven cable is a material and shape reference.

### 4.10 High-voltage pylons

- PRIMARY: High Voltage Transmission Line Tower (tileable) - mednios - CC-BY-4.0 - 2,763 faces - https://sketchfab.com/models/cbeec33de6d64374b6d22de2749c1313 (the author's note: free to download since 27 Aug 2026).
- alternate: 400Kv Transmission Tower Low Poly Model - manh21 - CC-BY-4.0 - 7,944 faces - https://sketchfab.com/models/675553d3b2004d72b68c04a5d147d0bd .
- alternate: Power Transmission Line - combine_soldier - CC-BY-4.0 - 6,529 faces - https://sketchfab.com/models/d72a69945cee44d8bc3b736e236c26e9 .
- alternate: Transmission tower - kentH - Fab - Fab-Standard - no materials - https://www.fab.com/listings/4e927ad5-3cf2-41ef-9fb0-6f6c28c73901 .
- excluded for shipping: Russian Power Transmission Tower - PavelDem - Sketchfab "Free Standard" - 579,512 faces - https://sketchfab.com/models/ec39aeadcfab45b2b0bae31beb75b6fc (not redistributable; heavy).
- Hungarian types (Fenyo, Duna) are not available free; build procedurally from the tower drawings if needed.

### 4.11 Street lights

- alternate: Soviet Streetlight Pack - UnkaII - CC-BY-4.0 - 1,500 faces - https://sketchfab.com/models/209fb45b77434af4b6830ff4da7d58b2 (and part 2, 1,726 faces: https://sketchfab.com/models/eff51e05a3b5449895f6eb1312d7e881).
- alternate: CSSR streetlamp Elektrosvit 444 231x - orphanrtg - CC-BY-4.0 - 749,947 faces, 8k, Slovak scan - https://sketchfab.com/models/6d3f37868cf74747b3b2153ca2674c99 (Czechoslovak lamp, Nanite).
- alternate: Street Lamp 01 / 02 - Josh Dean - Poly Haven - CC0 - https://polyhaven.com/a/street_lamp_01 , https://polyhaven.com/a/street_lamp_02 .

### 4.12 Fences and gates

- Concrete PO-2 panels (the Warsaw Pact's ribbed concrete fence round military sites, depots and works):
  - PRIMARY: PO2 Soviet fence - JLabus (Labus) - CC-BY-4.0 - 4,674 faces, with the FO-2 foundation block - https://sketchfab.com/models/08b74965892941478a161ab202788aea .
  - alternate: Soviet Concrete Fence PO-2 Game Ready - Reinaw13 - CC-BY-4.0 - 224 faces - https://sketchfab.com/models/98c8cbdb155c4552acad324e935053de .
  - alternate: Fence Plate Po-2 (FREE) - Andrey.Chegodaev - CC-BY-4.0 - 522 faces (143 polygons per plate and support) - https://sketchfab.com/models/e85ae2ef0cd34638899f87e6073aa4a3 .
  - alternate: PO-2 concrete fence wall - oyez (Cyril Demetrius) - CC-BY-4.0 - 5,648 faces - https://sketchfab.com/models/44a71074b609419685d29f7c19100fee (tagged "noai"; fine for use, noted).
  - alternate: Classic reinforced concrete fence - orphanrtg - CC-BY-4.0 - 19,999 faces, 2 x 8k, Slovak scan - https://sketchfab.com/models/ba665a1b019f411980f679448197fbd8 ; on Fab as SKRUNDA, CC-BY: https://www.fab.com/listings/1e5c9971-c92c-48a0-989e-38c073782843 . Also "Long dirty reinforced concrete fence" - https://sketchfab.com/models/7353556221504d4fbee635f9e80c552b .
- Wire mesh:
  - PRIMARY: Modular Chainlink Fence - James Ray Cock, Amal Kumar - Poly Haven - CC0 - 89,232 polys - https://polyhaven.com/a/modular_chainlink_fence .
  - alternate: Wire mesh fence (Zabor) - Every_3D - CC-BY-4.0 - 1,374 faces, modular - https://sketchfab.com/models/e5f5f7a619134a5f9143dcd52818a17f ; Wire mesh fence (Low Poly) - game_travel - 4,366 faces - https://sketchfab.com/models/21e5a71a8bdf479398bbaf27856a4420 .
- Wooden picket (village front fences):
  - PRIMARY: Fence pack (low poly, game ready) - TampaJoey - CC-BY-4.0 - 13,384 faces, modular, 3 texture options - https://sketchfab.com/models/f2ed9f555ff64eae95b408409cbff602 .
  - alternate: The fence is white (Zabor belyi) - marishka1611 - CC-BY-4.0 - 704 faces - https://sketchfab.com/models/47a64f7da74041a2938f95d549b42f9a .
  - alternate: Modular Fence Post - Megascans - Fab-Standard - https://www.fab.com/listings/832c3fcd-9066-4775-aa57-5b1a55cf6fc5 ; Modular Iron Fence - Megascans - https://www.fab.com/listings/19014eac-45aa-439a-b945-c9db9afd78ed .
- Gates:
  - PRIMARY: Wooden gate two-door - DimanRson (riotboy) - CC-BY-4.0 - 6,269 faces - https://sketchfab.com/models/8655738975204aff81065f611efa7ca2 .
  - alternate: Large Iron Gate - Josh Dean - Poly Haven - CC0 - 2.95 x 2.93 m - https://polyhaven.com/a/large_iron_gate .
  - alternate: Rusty gate - axonite - CC-BY-4.0 - 46,489 faces (scan; also on Fab) - https://sketchfab.com/models/658e2be775d74bc29e64f7336865013e .

### 4.13 Wells, haystacks, bales

- Sweep well (gemeskut): none free. Shadoof - lukus1 - CC-BY-4.0 - 49,150 faces - https://sketchfab.com/models/a5353d8a3b134c3fa0fdb6c54dcc2efd is an Egyptian shadoof (the author says so); same principle, wrong look. Gap (procedural: a forked post, a sweep pole with counterweight, a hanging pole, a bucket, a curb).
- Covered draw wells: Old Rural Well - brianjernigan - 4,328 faces - https://sketchfab.com/models/1163b47dc0aa4de2a19b23ab4539bef4 ; Well Low-Poly - Vladies - 9,188 faces - https://sketchfab.com/models/107f6ee63db1489a9764c3b176df78df (both CC-BY).
- Haystacks:
  - PRIMARY: Haystack 1 and 2 - Frice - CC-BY-4.0 - 1,172 and 932 faces - https://sketchfab.com/models/a1bbf38e91b74b08aba7851a00917843 , https://sketchfab.com/models/b0f9bf31bec0474fb20f3d911f427a8c .
  - alternate: Haystack - ljungman - 1,443 faces - https://sketchfab.com/models/f9e0ae0859fa4e71a2d7dbe55810c1a8 ; Haystack Bohmsdorf - mwintersberger - 46,227 faces, a scan - https://sketchfab.com/models/227474e19962479598106e41ddb478fe .
  - alternate: Hay bales - zbrojmistrz - 3,060 faces - https://sketchfab.com/models/d6e087f9a2a9416c94918f0503943c17 .
  - alternate (concern): Round Hay Bale - Megascans - Fab-Standard - https://www.fab.com/listings/fc0388e8-5443-4e54-b618-65790804779b ; Round straw bale - nedo - 398,114 faces - https://sketchfab.com/models/15d4c9584bfa4ce893cd72bdd6b23161 . Round bales may be anachronistic on a 1985 Hungarian farm (unverified in this pass); the long straw stack (kazal) is the period look and is a gap.

### 4.14 Roadside: bus shelters, signs, kilometre stones, guard rails

- Bus shelters:
  - PRIMARY: Old Soviet Bus Stop - GameDevNick (Coozy) - CC-BY-4.0 - 4,364 faces - https://sketchfab.com/models/fe4d132434c24e1c8447a6d1190e8c6d .
  - alternate: Soviet bus stop - Lordies - 1,464 faces - https://sketchfab.com/models/ffcf2f95eb1f4005a6e7555c2b6739c3 ; Outdated Soviet Bus Stop - Ottto3ds - 3,531 faces - https://sketchfab.com/models/1cc322c7d98f4f7794a326fa65809fb8 ; Soviet Bus Stop - dimmeyster - 16,836 faces - https://sketchfab.com/models/f8de51a1e7e547c4baabd51f1bd46709 ; Soviet Bus Stop - novusod - 172 faces - https://sketchfab.com/models/a07b714537134004b4d3930248d93a73 (all CC-BY). Hungarian shelters of the period were often concrete or brick; the Soviet ones are a stand-in.
- Road signs:
  - PRIMARY: European Road Signs - Satik64 - CC-BY-4.0 - 1,968 faces - https://sketchfab.com/models/25ebbe36a9274344bd779a55d97bfca3 (European-style sign shapes; faces to be retextured with Hungarian KRESZ symbols, which is a gap).
  - alternate: Modular Russian Road Signs Pack (URBAN) - UnkaII - CC-BY-4.0 - 10,934 faces - https://sketchfab.com/models/5d6e2672d73c44b3b0f7bf62e43fad55 ; the signs in the Soviet Environment Assets Pack above.
- Kilometre stones: Milestone marker stone - matousekfoto - CC-BY-4.0 - 100,000 faces, a Czech scan - https://sketchfab.com/models/de4cbf4ff7d24067b99b82cd35da3ebd ; Boundary way road marker stone - matousekfoto - Fab - CC-BY-4.0 - https://www.fab.com/listings/be05013d-3d0e-4c8c-95a3-8e0cf48ddc5f . Hungarian km and hectometre stones themselves: gap.
- Guard rails:
  - PRIMARY: Modular Metal Guardrail Kit - Megascans - Fab-Standard (free) - 3.73 m module - https://www.fab.com/listings/0b951efd-22b0-4c8c-a34b-420bad7e0977 .
  - alternate: Modular Safety Rail Kit - Megascans - https://www.fab.com/listings/8454a187-319b-4df5-a2c5-68fab8fe7452 ; guardrail road - danimow - CC-BY-4.0 - 524 faces - https://sketchfab.com/models/76e85683d30c4e6ba35da370467b7aa6 ; Steel Road Barrier - shockproof (Gianluca Gatto) - CC-BY-4.0 - 2,372 faces - https://sketchfab.com/models/438c36d48ab04ec4b5178c6989d3ab3b .
  - Traffic Delineator Post - Megascans - https://www.fab.com/listings/aa334de8-3fed-4c7d-96f6-1b21eb9048df (modern delineator; check period).

### 4.15 Railway

- Track:
  - PRIMARY: Railway Track Modular Kit - Megascans - Fab-Standard (free) - 3.21 x 5.0 x 1.36 m - https://www.fab.com/listings/01701fcb-08a4-4d67-8649-84474a723d12 ; with Modular Railway Track Crossing - https://www.fab.com/listings/0df7fe16-198d-48a7-976f-d2f9601e8613 , Modular Railway Track - https://www.fab.com/listings/014934a2-1772-4077-aadb-4f7850b57b3e and https://www.fab.com/listings/bb6f4e3b-bfc6-485b-8f9b-1ee857fa8cac , Modular Railway Track Kit - https://www.fab.com/listings/bfb70324-05a7-4fb8-bf6f-0e4feb5057e9 .
  - PRIMARY (redistributable): Railroad Tracks Straight And Curved Tileable - mednios - CC-BY-4.0 - 19,948 faces - https://sketchfab.com/models/b4b04de357c84532a74de68c223bc6af .
  - alternate: Railway Track 3D scan - zdenkoroman (grafi) - CC-BY-4.0 - 49,999 faces - https://sketchfab.com/models/21a0ed90f355495fb80c3dcc92a94b10 .
- Catenary: no MAV-style 25 kV catenary free. alternate: Railway_Power_lines - tennyson069 (Albedo) - CC-BY-4.0 - 9,466 faces - https://sketchfab.com/models/ab1be435d7c34161b9611f0d37426243 ; Low poly catenary train system (USA) - Ogfmh - CC-BY-4.0 - 1,236 faces - https://sketchfab.com/models/412b4ce3ca9345bfaf05f728500ce00a . A Dutch catenary pack exists only as Sketchfab Free Standard (not redistributable). Gap: procedural masts and wire.

### 4.16 Culverts and bridges

- Culverts: PRIMARY: Concrete Pipe - Megascans - Fab-Standard - 0.52 m diameter x 1.72 m - https://www.fab.com/listings/e0991087-ff11-466d-a01c-0fde7998a36c (and https://www.fab.com/listings/2800f1c0-d046-4b49-9099-289ea3d18eb9). alternate: Slab Culvert - gokuartz - CC-BY-4.0 - 968 faces - https://sketchfab.com/models/ebd41ec8133447b5a0eab917b597671f .
- Road bridges (concrete beam): PRIMARY: Concrete Girder Beam Bridges/Tunnels [Free] - Jotrain (Joshua Grech) - CC-BY-4.0 - 890 faces - https://sketchfab.com/models/1c5d6642796843c198b94124518b9f04 ; on Fab, CC-BY: https://www.fab.com/listings/b88147cb-3008-4077-96c5-a83d0730b542 . alternate: Concrete Bridge (low poly) - derfmode - CC-BY-4.0 - 7,826 faces - https://sketchfab.com/models/77d6c4481e6043889ea8cfe08c8fdcd2 .
- Rail bridges (steel truss): PRIMARY: Railway Bridge 76m - Grabka (Szakal) - CC-BY-4.0 - 55,098 faces - https://sketchfab.com/models/cace9b79f46f4e99bc76c0d6bbc1e8fc . alternate: Railway Bridges Pack 01 - Grabka - 420,480 faces - https://sketchfab.com/models/877cdcd7b6f14b72aaa88fa893b812c9 ; Steel truss bridge structure - soysascha - CC-BY-4.0 - 120,000 faces - https://sketchfab.com/models/ed8a104e1e2847eab43fccadcab197f6 ; Railway bridge - xENDing - Fab - CC-BY-4.0 - https://www.fab.com/listings/58a5920a-d2e9-429b-ae85-febb24404bff .
- The game cuts bridges (`simcore/country_map.hpp`): a cut bridge needs a broken state; none of these has one. Gap: the broken spans.

### 4.17 Military position items

- PRIMARY: Military Trench Megascans Sample - Quixel - Fab - Fab-Standard (free), a UE sample project published 18 Dec 2025 - https://www.fab.com/listings/f18c343f-b771-47b0-a02a-129771fd9804 . With the free Military Trenches parts: sandbag sets https://www.fab.com/listings/a5ad6a23-7e37-4b3c-ae91-063e8a4e0fd2 , https://www.fab.com/listings/b6989eea-8b04-409a-b008-52c90dbc1a48 ; Wire Barbed Set https://www.fab.com/listings/caec3ac3-300e-447a-aa1a-c72bf634e510 ; Emplacement Logs Wood https://www.fab.com/listings/06a9f5c3-31c4-4856-aab5-1fb0beb9e3ac ; Sandbag Pile https://www.fab.com/listings/5bb343d3-f805-4a3b-bced-d3cb7e5692e2 . The look is a dug trench line, closer to WW1/WW2; use the parts, not the layout.
- alternate (redistributable): Sandbag Wall 01-05 - PieterSnauwaert (Pypunk) - CC-BY-4.0 - about 1,176 faces - https://sketchfab.com/models/6ab8f2c7db3d46f49b5b58ae0b686e2e ; Sandbags - Evanz - CC-BY-4.0 - 5,824 faces - https://sketchfab.com/models/8cda4370170746d393aa311a7c080c50 (his "Sandbags - Defense line" is CC-BY-SA: excluded).
- alternate: Wire Obstacle/Defence Set - Richmond Nti - Fab - CC-BY-4.0 - https://www.fab.com/listings/c29187fe-cb1c-4984-9b3c-c909bc9f5dc9 .
- Barriers: PRIMARY: Concrete Road Barrier and Concrete Road Barrier 02 - Amal Kumar - Poly Haven - CC0 - 1.55 m and 1.57 m long - https://polyhaven.com/a/concrete_road_barrier , https://polyhaven.com/a/concrete_road_barrier_02 . alternate: Concrete Barrier - Megascans - 5.39 m / 3.04 m / 1.81 m - https://www.fab.com/listings/deab69d8-fee9-469b-8b45-579283a8ce46 , https://www.fab.com/listings/0f0cef28-61e0-463c-add3-c93e11f5130e , https://www.fab.com/listings/a2721342-a33c-40f2-9b22-c231aeabe716 .
- Anti-tank hedgehogs: Rusted Czech Hedgehog - GrostyBoi (Kane Griffiths) - CC-BY-4.0 - 1,972 faces - https://sketchfab.com/models/cfa712e8851a4881989ea890bfc6910e ; Jezek - Hedgehog 2021 - matousekfoto - CC-BY-4.0 - 188,997 faces, scan - https://sketchfab.com/models/4235fb23325e4a339f7efa4ebd3a22a1 .
- Bunkers and shelters: Soviet bunker terran - Medv3D - CC-BY-4.0 - 1,396 faces - https://sketchfab.com/models/c125c5c1e56840e68ea00068c5216279 ; Old Concrete Bunker PBR Scan - PersScans - Fab - CC-BY-4.0 (Croatia) - https://www.fab.com/listings/aaa7360f-ad97-4c5a-8982-6dd8ef641598 ; Hardened Aircraft Shelter HAS -DRAFT- - samuelbrunner - CC-BY-4.0 - 58,511 faces - https://sketchfab.com/models/ccc398978c0f4bb3955ada9d9a1dcf3c (an airfield's shelter for the theatre's air bases; a draft).
- Trenches: 3D trench system pack free - Nuclearwinter94 - CC-BY-4.0 - 122,252 faces - https://sketchfab.com/models/2abd39ba7ee84dabb28c484a3b09810c .
- Camouflage nets: only Arma Reforger LAV-25 Camonet - TheOrangeDealer (KPMC) - CC-BY-4.0 - 441 faces - https://sketchfab.com/models/36fa47c2a24c48e9b73ba2284bb8ef79 . Concern: the name says it is from Arma Reforger (Bohemia Interactive's game); whether the uploader holds the rights is not stated. Not recommended to ship. Gap: procedural nets over the Osa and PU-12M (a draped cloth on poles, `MaskedLit` material with a garnish texture).
- Tarps and stores: Wrinkled Tarp - Megascans - https://www.fab.com/listings/b4cd9451-85e9-434e-ab18-8f5a7a3e1f5d ; Rusty Metal Barrel - Megascans - https://www.fab.com/listings/5f4c0800-6c8d-4945-8fdf-6855ab6a566d ; Wooden Military Crate - Prabhjinder Singh - Poly Haven - CC0 - https://polyhaven.com/a/wooden_military_crate ; Ammo Box - DanKit - Poly Haven - CC0 - https://polyhaven.com/a/ammo_box .
- Earth revetments and dug-in pits for an Osa, a PU-12M and a TZM: none free that matches. Gap: dig them in the heightfield (the battery area's places, `simcore/campaign/battery_area.hpp`) with a parapet spline mesh.

### 4.18 Civilian vehicles, 1980s Eastern bloc (parked props)

Concern for all: trademarks (Trabant, Lada/VAZ, Ikarus, ZIL, Zetor, Dacia, Wartburg, IFA, MTZ/Belarus are brand
names; some models carry badges). The CC-BY licence covers the author's mesh, not the marque. Strip badges and
names before shipping and do not use the brand in the game's text.

- Trabant 601: PRIMARY Trabant - haricsiko - CC-BY-4.0 - 91,248 faces - https://sketchfab.com/models/c39a112a1d8242e5a913141f703f2687 ; alternates TRABANT low poly - jasmin.daniel - 5,330 faces - https://sketchfab.com/models/0a79e91d3c3447e19c8c0c3339557a0b ; Trabandt 601 Low Poly - SentientSundae - 8,080 faces - https://sketchfab.com/models/6c788eb1ec9944ab96338f5b392932d2 ; Trabant lowpoly - vjuhandkx - 7,490 faces - https://sketchfab.com/models/302a341602134d248cdb93ec149ad6ec ; Trabant 601 WIP - OMA - Fab - CC-BY-4.0 - https://www.fab.com/listings/dd05eafa-60fa-4372-8561-03c86ec7499e .
- Lada 2101 (Zhiguli): PRIMARY Lada-2101 - ratura5100 - CC-BY-4.0 - 18,589 faces - https://sketchfab.com/models/8c7e9ba402534bbdb9dbce548e9780a2 ; alternates VAZ 2101 - Javkal - 5,464 faces - https://sketchfab.com/models/95d34f63b5b14e2197fd41383776e031 ; Vaz 2101 (LOW POLY) - Lertys245 - 1,485 faces - https://sketchfab.com/models/8b432383935144049c9bf6f70a756a2d ; Low poly lada niva - RitiWox - 5,770 faces - https://sketchfab.com/models/b67df18b55ab497ba30cdbd6be7dee19 .
- Ikarus buses: PRIMARY Ikarus-260 - hans1240 - CC-BY-4.0 - 182,716 faces (decimate for a parked prop) - https://sketchfab.com/models/a5f8bf0d5642492cb2dbeb8da337ece3 ; alternates Ikarus-255.01 - hans1240 - 138,636 faces - https://sketchfab.com/models/d78912e37fd644889c857b4bc1ccc985 ; Ikarus 280 - Randonavt - 128,629 faces - https://sketchfab.com/models/66b07719e1fd42ecbcdbb889a1d1a09c . Excluded: "USSR bus Ikarus" from Everlasting Summer (likely ripped from that game).
- Small bus: PAZ-652 - Romunql - Fab - CC-BY-4.0 - https://www.fab.com/listings/aef21118-043a-4221-a954-06c598337d4e .
- Lorries: PRIMARY Zil 130 - javanilga - CC-BY-4.0 - 7,293 faces - https://sketchfab.com/models/f0eb399e57a249a7b7dd4058c64cf7b5 ; alternates Russian ZIL-130 - 42manako - 4,764 faces - https://sketchfab.com/models/d8ed0b9c2504498f991af9d338088fd3 ; ZIL 130 (body) - Romunql - Fab - CC-BY-4.0 - 63,964 polys - https://www.fab.com/listings/7f73e571-0674-487d-81ae-643792e01617 ; ifa w50 - nguyenson06 - 183,166 faces - https://sketchfab.com/models/42ccea9f916b4a8bb2328fc39b547cf7 .
- Tractors: PRIMARY MTZ-80 Lowpoly - Vebis_V_stolb_GT - CC-BY-4.0 - 10,662 faces - https://sketchfab.com/models/6c1da5c5d3aa424cb5ac35d4833af047 ; alternate Tractor Zetor - Klooney (Michael_P) - 69,232 faces - https://sketchfab.com/models/16d799890d6c499a8762677449b9e179 .
- Others: Dacia 1300 / Renault 12 LowPoly - solid3ddd - 6,591 faces - https://sketchfab.com/models/11ca7a926045419b93af0f87da295c84 ; Wartburg-3d-kleiner - JensTiburski - 91,748 faces - https://sketchfab.com/models/3e508b3958994db9982964049b8e4aa9 . The same author's "Old car" is Free Standard (not redistributable).
- Stylised only: Low Poly Soviet Cars - Anadolu Saykodeli - Fab - Fab-Standard - https://www.fab.com/listings/cab1012b-3315-4f5e-a9d3-5ddf3dca1d2f (a toy look; not for the realistic map).
- Note: 42manako's Kamaz-4310 and MT-LBM are CC-BY-NC: excluded; his MT-LB variants are CC-BY (not civil; noted for the military vehicle research, not here).
- Not a fit: City Sample Vehicles - Epic - https://www.fab.com/listings/2909157b-ddfa-4cef-a925-69dc2467021f and Vehicle Variety Pack - https://www.fab.com/listings/dc1ada50-2523-44b1-b0e2-a72d14076fb4 (modern cars).

### 4.19 Small props (all CC0, Poly Haven, redistributable)

Utility Box 01/02 (https://polyhaven.com/a/utility_box_01, https://polyhaven.com/a/utility_box_02), Old Tyre
(https://polyhaven.com/a/old_tyre), Covered Car (https://polyhaven.com/a/covered_car), Cement Bag
(https://polyhaven.com/a/cement_bag), Water Manhole Cover (https://polyhaven.com/a/water_manhole_cover).

## 5. Not suitable or not checked

- Rural Australia (Andrew Svanberg Hamilton) - Fab - UE Marketplace licence - https://www.fab.com/listings/1c1467ce-a2f5-4be1-8988-9069f90a8571 : Australian farm buildings; only corrugated iron sheds are reusable.
- Advanced Village Pack - Fab - https://www.fab.com/listings/250a5622-d5fd-49fc-b1c4-0550a444e117 : medieval.
- Modular Rural Cabins - Maarten Hof - Fab - Fab-Standard - https://www.fab.com/listings/508fe84a-4976-4cfe-9a40-c2b9533da601 : American cabins.
- Electrical Substation Free - Abandoned World - Fab - https://www.fab.com/listings/0b680515-bfa6-4958-8f80-d7a7fcf14428 : Personal free, Professional paid (concern).
- City Sample (Epic): American city; not a fit.
- Blendkit free "Concrete Fence Panel", "Concrete Slab Boundary Fence" (real scans) and "Hay Stack" (Reza NeoMasyaf): Royalty Free licence, account required, not recommended for shipping (section 1).
- Kenney, Quaternius: stylised CC0 packs, not checked further (the map is realistic).
- `art/placeholder cold war models/` in the repo: eight zips with no licence record; not used.

## 6. What must be built (procedural or Blender scripts)

1. The Kadar cube (kocka house): hipped roof, 10 x 10 m plan, two-axis street front, dash (fröcskölt) render in
   pastel colours, L-plan and attic variants; one generator with variants for `house`.
2. The peasant long house (hosszuhaz): narrow plan, gable to the street, tornac porch on the yard side, reed or tile roof.
3. TSZ buildings: long cow sheds (brick, ridge vent), open machine halls (concrete frame, corrugated
   asbestos-cement roof), the farm's grain store, concrete tower silos.
4. Concrete water towers (mushroom, chalice, cylinder) with the body-of-revolution mesher.
5. The village church (single west tower; Catholic cross or Reformed star on the spire).
6. Hungarian concrete poles (perforated web; 20 kV A-frames) and wooden telephone poles on concrete stubs; the
   wires as catenary tubes between poles.
7. Hungarian transmission towers (Fenyo, Duna families) if the country's HV lines are drawn near the player.
8. The sweep well (gemeskut), straw stacks (kazal), small rectangular bales, the corn crib (gore).
9. KRESZ road signs (textures on the free sign meshes), place-name boards, km and hectometre stones.
10. MAV catenary masts and wire.
11. Concrete road slabs (the PAG/IZS-type slabs of military roads and farm tracks) as a road surface mesh or decal.
12. Earth revetments, parapets and dug-in pits for the battery's vehicles; camouflage nets.
13. Broken states for the bridges the war cuts.
14. Hungarian panel blocks proper (house-factory types) if the Czech and Soviet stand-ins read wrong.

## 7. Process notes for the register

- CC0 and CC-BY files may go into git (`art/` or Content) with a register entry; CC-BY needs `author` and a
  credit line. Fab-Standard and Epic-Content-Only files go to `unreal/SAM3D/Content/ThirdParty/` only.
- Sketchfab downloads need an account (the download API takes an OAuth token:
  `https://api.sketchfab.com/v3/models/<uid>/download`); the person downloads, not a script run by Claude.
- Fab items: "Add to My Library", then the Fab plugin in the editor.
- Poly Haven: `https://api.polyhaven.com/files/<slug>` lists the glTF, FBX and .blend files.
- Every model needs a scale and pivot check against the map's unit cube (`scale`, `yaw_deg`, `z_offset_m` in
  `assets.json`) and a poly budget: the scans (Dorfkirche Warchau, the ČSSR streetlamp, the Ikarus buses) need
  Nanite or a decimated LOD before they can be scattered.
# Fill pass: built structures and props (area: built)

Date: 3 Nov 2026. Closes, as far as the sources allowed, the gaps the first pass (`built_models.md`) left for the
procedurally generated buildings and props, the trucks and tractors, and the plan for the building generator.

## 0. Method and limits

- **No web search was available in this pass.** The turn's shared WebSearch budget (200 calls) was already used up
  when this pass began, so every fact here comes from pages read directly by URL: Wikipedia (hu, en, ru, cs), the
  Magyar Néprajzi Lexikon on MEK (the Hungarian ethnographic encyclopaedia, written 1970-75), Wikimedia Commons file
  and category pages, OpenStreetMap data through the Overpass API, two utility presentations and a tender document
  (PDF/DOCX read as text), the Ludwig Museum's Kádár-kocka booklet, and the Sketchfab catalogue's own search API.
  Sources that needed a search to find (Hungarian type plans for farm buildings, the Szentendre Skanzen's measured
  drawings, the KRESZ sign-size standard, km-stone specifications) were not reached; they are listed in section 13.
- Every number is either **quoted from its URL** or marked **estimate**. Estimates are game values to be checked
  against the reference photos with the photo-camera method (`docs/MODELLING_FROM_PHOTOS.md`) before they go into
  data; in `data/` they would carry `"confidence": "estimate"` and a `source` naming this file.
- No asset file was downloaded. WebFetch saved three documents it read (two PDFs and a DOCX) to the session's
  tool-results folder; their text was extracted in the scratchpad only.
- Sketchfab's per-model API answered 429 Too Many Requests in this pass, so new Sketchfab picks below are from the
  search API's records (name, author, licence label, face count, date); their pages were not read.

## 1. The Kádár-kocka (part `house`, the 1960s-80s village house)

Sourced facts:
- Square plan, about 10 x 10 m, about 100 m²; 8 x 8 m and 11 x 11 m also occur. Mostly single storey. Tent (hipped)
  roof; mansard roofs on some larger two-storey houses. Typical wall: B30 block brick. Original windows: three-leaf
  timber casements. Entrance on the side of the house, up a few steps, often with a canopied terrace. A semi-basement
  (alagsor) reached by a side stair, used as a store or workshop. Set back from the street behind a front garden.
  Typically two-tone paint with discreet decoration. Variants: square (standard) and L-plan; two-storey and
  split-level; mansard; rare flat roofs. Built late 1950s to late 1980s (first in Újpest 1957; main period
  1957-1969; second wave in the 1970s; in the countryside into the first half of the 1980s); more than 800,000 built
  1959-1980, about 844,000 standing. Raw brick left unrendered in south-west Transdanubia; grey slag brick in north
  Borsod. - https://hu.wikipedia.org/wiki/Kádár-kocka
- The Ludwig Museum's definition: square plan (about 10 x 10 m), tent roof, single storey, free-standing, **two window
  axes on the main front**; parts: a mostly sunken, shuttered concrete base, brick walls (B30 block), floor slab, roof;
  the main front **divided horizontally: a clad plinth (burkolt lábazat), a middle band divided by bands, paint or
  mouldings (originally holding a pair of tripartite windows), the eaves, the tent roof**; the L-plan the most common
  variant. - https://ludwigmuseum.hu/system/files/news/attachments/2024-08/KMK_fu%CC%88zet%20online_egybe.pdf (the
  booklet's glossary page, Hungarian and English)
- 60-100 m² living space; usually single floor; rooms with windows on the street front; often a separate summer
  kitchen; TÜZÉP's building materials made the houses uniform; "pseudo-cubes": an older farmhouse extended with a
  street-front room and a tent roof; mainly in the east and north-east. -
  https://en.mandadb.hu/cikk/1285342/Kadar_cubes_blocks_of_houses_in_the_countryside
- About 800,000 built 1960-1980; colourful, "joyfully symmetrical" facades. -
  https://www.new-east-archive.org/features/show/13144/the-kadar-cube-comeback-how-the-humble-1960s-hungarian-home-inspired-high-fashion-and-eco-friendly-living

Not found in a source (estimates, to be checked against photo P34 and P37 of `references.md`):
- Plinth (raised ground floor over the semi-basement) 0.6-1.2 m; wall-plate height 3.6-4.2 m above ground; eaves
  overhang 0.4-0.6 m; roof pitch 30-38 degrees (clay tile); the apex of a 10 m square at 35 degrees about 3.5 m above
  the wall plate, so the whole house about 7-8 m high. Tripartite window about 1.8 m wide by 1.5 m high, sill about
  0.9 m above the floor; the two street-front axes about 2.5 m in from each corner. One chimney stack, 0.5 x 0.5 m,
  0.8-1.2 m above the roof. Render colours: pastel two-tone (a lighter field over a darker band or plinth).
- The map's houses today are 10-16 m long, 7-10 m wide and 4.5-7 m tall (`simcore/include/simcore/terrain_map.hpp`,
  `house_size`), placed end-on to the street. A kocka is about 10 x 10 x 7.5 m and set back behind a garden, so a
  kocka style needs its own box size in simcore (a simulation change: the box is the sight-line obstacle).

References in the repo's photo list (`references.md`): P34 (Söréd, the Wikipedia lead photo), P37 (Csongrád).

## 2. The peasant long house (hosszúház) with porch (tornác)

Sourced facts:
- Street villages on strip plots: the house stands on the plot's street boundary with its **gable to the street**;
  behind it the farmyard, outbuildings, stores, sometimes a barn across the plot, then the garden or field at the back.
  Many 18th-19th century villages were re-planned into multi-street strip-plot villages. -
  https://mek.oszk.hu/02100/02115/html/4-1459.html (Magyar Néprajzi Lexikon, "szalagtelkes falu")
- The Alföld house is "szoba-konyha-kamra" (room, kitchen, pantry), the three-room house mostly established by the
  17th-18th centuries; outbuildings join it or stand apart. Walls mostly earth: rammed earth (vert fal), adobe
  (vályog, widespread from the 19th century), layered mud (rakott sárfal), fecskerakás, wattle and daub. Roofs gabled
  on a purlin (szelemen) structure carried by posts (ágas). Covering: reed (nád) the main one, straw thatch (zsúp) in
  places, **tile (cserép) in the market towns**. The tornác (porch) is documented from the 18th century, as a side
  porch (oldaltornác) or gable porch (oromtornác); early pillars of wood, often carved, later of adobe or stone.
  Around 1900 most peasant houses were still earth-built. - https://hu.wikipedia.org/wiki/Magyar_népi_építészet
- The kitchen was the main room of the older peasant house type; rammed earth, adobe and layered-mud walls. -
  https://hu.wikipedia.org/wiki/Parasztház
- "Pseudo-cubes" (an old farmhouse given a street-front room and a tent roof) - mandadb page in section 1: by 1985
  some long houses had been converted this way.

Not found in a source (estimates; to check against the street-village photos P32, P33, P35, P36, P38 of
`references.md` and the 1982 aerials P16, P17):
- Outer width 5.5-7.0 m; length 12-25 m (three to five rooms in a row, plus a stable or store under the same roof);
  eaves 2.6-3.0 m above ground; roof pitch 40-50 degrees (45 for reed, about 40 for tile), ridge 5.5-7 m.
- Tornác along the yard side: 1.2-2.0 m deep, pillars 0.35-0.5 m square at 2.5-3.5 m centres, often under the main
  roof's continued slope. Street gable: one or two small windows, a gable board or vent; by 1985 most roofs tile.
- Whitewashed walls; a darker plinth band 0.4-0.6 m (the colour unverified here; blue-grey, ochre and black all seen
  in photos, not checked against a source).

## 3. TSZ buildings: cow sheds, machine halls, grain stores, tower silos

Sourced facts:
- Cattle-stall geometry (from the Pallas Nagy Lexikona text, flagged by Wikipedia as possibly outdated): cow stall
  1.90-2.50 m long by 1.00-1.20 m wide; ox stall 1.90-2.30 by 1.30-1.40 m; central feed aisle with manger
  1.90-2.50 m; side manure walks 1.00-1.90 m; **a double-row barn about 8-12 m deep; height 2.80-3.80 m**; doors at
  least 1.40 m wide by 2.10 m high; windows 1.75 m above the floor; manger 0.40-0.50 m wide, top 0.70 m above the
  floor. - https://hu.wikipedia.org/wiki/Istálló
- Silos: reinforced concrete silos with square or round cells; sheet-steel silos usually round, prefabricated units of
  10-50 t, **2.8 m diameter, up to 18 m high**; silo tanks 20-30 m high mentioned. - https://hu.wikipedia.org/wiki/Siló
- Tower silos in general: 3-27 m in diameter, 10-90 m high; blue glass-lined (Harvestore) silos once very common
  (where is not stated); bunker (trench) silos with concrete walls. - https://en.wikipedia.org/wiki/Silo
- Photos (from `references.md`): P41 Szalkszentmárton 1975, TSZ machine station
  (https://fortepan.hu/hu/photos/?id=70386); P42 Dunaegyháza 1975, a TSZ building
  (https://fortepan.hu/hu/photos/?id=70381); P45 Ercsi, grain silos and cow/pig houses; P46 Püspökladány, the HAGE
  grain silos at the station; P54 Orosháza, silos and a mill.

Not found in a source (estimates; a 1970s large-scale farm is not the Pallas-era stall, but the stall geometry above
bounds it):
- Cow shed (tie stalls, two or four rows): width 12-21 m, length 60-120 m, eaves 3.0-3.5 m, roof 15-20 degrees in
  corrugated asbestos-cement sheet with a continuous ridge vent, brick or concrete-panel walls with a row of small
  windows high up (sill 1.75 m as above), big doors in the gable ends for the feed trailer.
- Open machine hall (gépszín): concrete or steel portal frames at 6 m bays, span 12-18 m, eaves 4.5-5.5 m, low-pitch
  asbestos-cement roof, one or both long sides open.
- Farm grain store: closed hall 12-18 m by 40-80 m, eaves 5-6 m.
- Farm tower silo (silage or grain): concrete or steel, 4-7 m diameter, 12-24 m high, in groups of two to six,
  with a conical or flat cap and an outside ladder or elevator leg.
- The station grain elevator (P46): a block of concrete cells 30-50 m high with a head house; one per railway town.

## 4. Concrete water towers (mushroom, chalice, cylinder)

Sourced facts:
- Heights of Hungary's mapped water towers (OpenStreetMap, `man_made=water_tower` with a `height` tag, read through
  the Overpass API, 3 Nov 2026; about 90 rows): most between 25 and 60 m; dated ones: Szeged Tarján 1970 60 m,
  Újszeged 1973 60 m, Győr Marcalváros 1978 30 m, Szeged Rókus 1985 66.5 m; the pre-1915 ones 15-58 m (Margitsziget
  1911 57 m, Újpest 1912 27 m, Szeged Szent István tér 1904 54.9 m); village-scale entries 15-25 m (one tagged
  "hydroglobus" 25 m). Query:
  https://overpass-api.de/api/interpreter?data=[out:csv(name,height,start_date,::lat,::lon;true;"|")][timeout:50];area["ISO3166-1"="HU"][admin_level=2]->.hu;nwr["man_made"="water_tower"]["height"](area.hu);out center;
  (data © OpenStreetMap contributors, ODbL).
- Körmend 1971, 600 m³, 44.3 m; Pápa 41.5 m, 1,500 m³ on an 8.7 m base; Törökszentmiklós 1977 (from `references.md`
  T13, its Commons sources:
  https://commons.wikimedia.org/wiki/File:Bercsényi_Miklós_Straße,_Wasserturm,_2023_Körmend.jpg ,
  https://commons.wikimedia.org/wiki/File:Water_tower,_Bástya_Street,_2020_Pápa.jpg ,
  https://commons.wikimedia.org/wiki/File:Táncsics_Mihály_Straße_Wasserturm,_2023_Törökszentmiklós.jpg ).
- **Mushroom** towers: Szécsény, "mushroom-shaped concrete water tower 1970s (1980s)" -
  https://commons.wikimedia.org/wiki/File:Pilzf%C3%B6rmiger_Wasserturm,_2024_Sz%C3%A9cs%C3%A9ny.jpg ; Dunakeszi-Alag,
  mushroom-shaped, about 1960s-70s - https://commons.wikimedia.org/wiki/Category:Mushroom-shaped_water_tower_(Dunakeszi) ;
  Karcag, "concrete mushroom" type, a radio antenna on it 35 m above ground (so the tank top is near 30-35 m) -
  https://commons.wikimedia.org/wiki/Category:Water_tower_(Karcag)
- **"Star" type (csillag típusú)** towers of the 1950s-60s: Tiszaszederkény (Tiszaújváros, before 1966), with similar
  ones at Makó, Hódmezővásárhely, Dunaújváros and Nyíregyháza; Fortepan photos of 1966 in the category -
  https://commons.wikimedia.org/wiki/Category:Water_tower_in_Tisza%C3%BAjv%C3%A1ros
- **Round (cylindrical) concrete** towers: Tiszafüred (late 1960s / early 1970s, `references.md`) -
  https://commons.wikimedia.org/wiki/File:Runder_Wasserturm,_2026_Tiszaf%C3%BCred.jpg ; Orosháza -
  https://commons.wikimedia.org/wiki/File:Runder_Wasserturm,_NO,_2021_Orosh%C3%A1za.jpg
- **Hydroglobe (hidroglóbusz)**: a metal water tower, usually a spherical tank of 50-500 m³ on a 15-40 m cylindrical
  column, often guyed with steel cables; usually supplies a farm enterprise or a small settlement; a "hidrohenger"
  (cylinder) put up at Csömör in the 1970s. - https://hu.wikipedia.org/wiki/Hidroglóbusz
- Mostly concrete or brick; tanks divided into compartments. - https://hu.wikipedia.org/wiki/Víztorony
- All towers in Commons by town and type: https://commons.wikimedia.org/wiki/Category:Concrete_water_towers_in_Hungary
  (25 towers), https://commons.wikimedia.org/wiki/Category:Water_towers_in_Hungary

Not found in a source (estimates for the lathe profiles, `Lathe` in `scenery/map_meshes.hpp`):
- Mushroom: shaft 3-4 m diameter to 22-30 m; tank 12-16 m diameter, 5-7 m deep, its underside a cone or bowl; flat
  roof with a rail; total 30-40 m; 300-1,000 m³.
- Chalice (kehely): a shaft widening upward in a hyperbolic or conical flare into the tank; tank 10-14 m; total
  30-45 m.
- Cylinder: tank 8-12 m diameter, 6-10 m tall, on a shaft 3-5 m, or the whole tower one cylinder; total 25-45 m.
- Hydroglobe (the farm and village tower): column 1.5-2.5 m diameter, sphere 5-9 m diameter (50-300 m³ is a sphere
  of 4.6-8.3 m), total 20-35 m, guy cables.
- Colours: unpainted grey concrete for 1985 (advertising and mobile masts seen on the photos are later and are left
  off).

## 5. The single-tower village church

Sourced facts:
- OpenStreetMap heights on the Great Plain (`building=church` with `height`, bounding box 46.1-47.6 N, 19.0-21.8 E):
  a Reformed church at 46.9085 N, 20.3840 E tagged 44 m; the Szentháromság church at 46.1565 N, 21.0619 E tagged 49 m;
  village churches at Hosszúpályi tagged 7-9 m (the nave alone, not the tower). Towers tagged as `building:part=tower`
  on the plain are urban (Kecskemét co-cathedral 54 m, Szeged 72-81 m). Query (same Overpass endpoint):
  `[out:csv(name,height,denomination,"roof:height",start_date,::lat,::lon;true;"|")];nwr["building"="church"]["height"](46.1,19.0,47.6,21.8);out center;`
  (data © OpenStreetMap contributors, ODbL).
- Photos (`references.md`): P03 the Reformed church of Igar across a ploughed field
  (https://commons.wikimedia.org/wiki/File:Ploughed_fields,_near_Igar.jpg), P12 Pilismarót's Reformed church in fog,
  P16 Hort 1982 and P17 Nagyfüged 1982 from the air (Fortepan 84605, 84606): the church at the village's centre.

Not found in a source (estimates):
- Nave 10-14 m wide, 22-35 m long, eaves 7-10 m, gable roof 45-50 degrees in tile; a west tower 4-6 m square
  standing on the nave's axis, its body 18-28 m to the cornice, a spire or baroque helm 8-15 m above, the top
  30-45 m; a semicircular or polygonal east apse. Catholic towers carry a cross; Reformed towers commonly a star (a
  common sight, not checked against a source in this pass). Whitewashed or ochre render.
- The map has no church today; a 35-45 m tower is a sight-line obstacle and a crew landmark, so it must enter the
  map's boxes (simcore) as a footprint, not only as dressing.

## 6. Poles and lines: Hungarian concrete poles and 20 kV lines

Sourced facts:
- Distribution networks (E.ON Hungary, presentation of 27 Oct 2020): **high voltage (over 35 kV): lattice towers
  30-40 m, spans 250-350 m; medium voltage (1-35 kV): wood, concrete or lattice supports 12-16 m, spans 80-110 m, mostly
  one circuit of three wires; low voltage (under 1 kV): wood, concrete or lattice supports 7-12 m, spans 35-40 m, bare
  or insulated wires (4+1, 4+2 or a bundle)**. Type plans: **VÁT-H2** (medium-voltage networks 20-35 years old in
  2020, so built about 1985-2000) "wood, steel and concrete poles, triangular conductor arrangement"; **VÁT-H20**
  (0-20 years) steel and concrete poles with the conductors in one plane, and shared 20 + 0.4 kV + street-light lines
  on spun reinforced-concrete poles; **VÁT-H4** (low-voltage networks 15-35 years old, built about 1985-2005) "mostly
  wood and steel poles, and solid-body, soft-steel reinforced concrete poles, with bare conductors"; **VÁT-H40** (0-15
  years) "perforated-web (áttört gerincű) or spun concrete poles with a conductor bundle". -
  https://english.nmhh.hu/document/215849/8_Eon_prezentci_NMHH_ptetti_frum_v1.pdf (pages on the voltage levels and the
  type plans)
- Pole heights by use (Märcz László, 19 Nov 2020): low-voltage poles 8-10 m; medium-voltage 10-15 m; high-voltage
  35-40 m; lamp posts 4.5-12 m. - https://www.hte.hu/documents/4652308/4684563/10_2_Marcz_Laszlo.pdf
- A current perforated-web pole: "Áttört gerincű B12 - P28 (P)", 12 m high, 28 kN tip pull, prestressed strand, a
  support-and-branch pole on a 22 kV line, with porcelain, composite or resin pin insulators and a crossarm. -
  https://www.tettyeforrashaz.hu/palyazatok-beszallitoknak/palyazat/csatolmanyok/cu9j7SSkV15KRNBdquNuOorBqIizSBL0
  (a DOCX tender description)
- A medium-voltage pole about 10 m high (OSM Hungarian mapping guide). -
  https://wiki.openstreetmap.org/wiki/Template:HU:Map_Features:power
- Concrete utility poles stood in Budapest by 29 Sep 1971 (Willy Pragher's photo, CC BY 4.0, filed under concrete
  utility poles in Hungary). -
  https://commons.wikimedia.org/wiki/File:Budapest-_Sowjetisches_Befreiungsdenkmal_auf_dem_Gellerthegy_29_m_von_Zigismond_Kisfaludy_Strobis_-_LABW_-_Staatsarchiv_Freiburg_W_134_Nr._095119b.jpg ;
  46 photos of Hungarian concrete poles (all 2008 or later): https://commons.wikimedia.org/wiki/Category:Concrete_utility_poles_in_Hungary

What this changes: the first pass assumed the 1985 village line was on perforated-web concrete poles. E.ON's type-plan
ages put the 1985 low-voltage line on **wooden poles (often on concrete stubs), steel poles and solid concrete poles
with bare wires**, and the 1985 medium-voltage line on **wood, steel or concrete poles with a triangular head** (one
insulator on top, two on a crossarm). Perforated-web poles are tied to the later VÁT-H40 plan; whether some were
already standing in 1985 was not settled (a dated 1980s photo would settle it; Fortepan is the place to look).

Estimates for the meshes:
- LV pole: 9 m above ground (wood 0.20-0.25 m at the base; solid concrete 0.20 x 0.30 m tapering), four or five bare
  wires on pin insulators on a 1.2-1.6 m crossarm or on brackets, street-light arm at 6-7 m; spans 35-40 m (sourced).
- MV pole: 12-14 m; triangular head: one pin insulator at the top, two on a 1.6-2.0 m crossarm 0.8-1.0 m below;
  spans 80-110 m (sourced). Angle and terminal poles: an A-frame of two poles leaning together (bakoszlop), with
  tension insulator strings; a pole-top transformer (OTR) on a two-pole portal at village edges (photo P40).
- Wires as catenary tubes between insulator sockets (first pass's recommendation), sag about 1-1.5 % of the span.

## 7. The sweep well (gémeskút)

Sourced facts:
- The most widespread water-lifting structure in the Hungarian-speaking area, a two-armed lever. A post set in the
  ground (ágas), often a living tree, with a forked or notched top holding an iron axle; the pivoting beam (gém); a
  counterweight of stone or a wooden block (kolonc) on the short arm; the bucket pole (ostor, kútostor) hanging from
  the long arm, with a hooped wooden bucket. Most farmyards had one; villages and pastures had shared wells; **double
  wells at busy places such as pastures**. Regional names sujtó, hankalák, sudár, nyilas kút, komponos kút. -
  https://mek.oszk.hu/02100/02115/html/2-624.html (Magyar Néprajzi Lexikon, "gémeskút")
- Its size depends on the lifting height; the hook on the bucket pole is the hangarék; until a few decades ago the
  most common water-lifting device of the Alföld. - https://hu.wikipedia.org/wiki/Gémeskút
- Shadoofs lift 1-6 m; the beam hangs at about one fifth of its length from one end. -
  https://en.wikipedia.org/wiki/Shadoof
- Photos: P57 Hortobágy 1941 (https://commons.wikimedia.org/wiki/File:Hortobágy,_1941,_Gémeskút._Fortepan_12290.jpg),
  P58 from the air (https://commons.wikimedia.org/wiki/File:Gémeskút_a_levegőből._(Hortobágyi_Nemzeti_Park).jpg).

Estimates (geometry chosen so the bucket reaches the water and clears the curb; the Alföld's water table a few metres
down):
- Ágas 4.0-5.0 m above ground, 0.25-0.35 m thick, fork at the top; gém 9-12 m long, 0.20 m at the pivot tapering to
  0.10 m, pivot a quarter to a fifth of its length from the counterweight end; kolonc a stone or log 0.4-0.6 m;
  ostor 5-7 m, 0.08 m; bucket 0.3 m. Káva (curb): a square timber box 1.0 x 1.0 m or a brick/concrete ring 1.0-1.2 m
  across, 0.7-0.9 m high; a trough (vályú) 3-5 m long beside it for the stock. At rest the long arm is up (the bucket
  pole hangs above the curb).

## 8. Straw stacks (kazal) and bales

Sourced facts:
- A long straw stack (szalmakazal) on a field, 4 Aug 2015 -
  https://commons.wikimedia.org/wiki/File:Szalmakazal.JPG (CC BY-SA 4.0, Derzsi Elekes Andor); haystacks at the
  Selyem II farm, Kunszentmiklós, 2019 - https://commons.wikimedia.org/wiki/File:Selyem_II._farm,_haystacks,_2019_Kunszentmiklós.jpg
- No Wikipedia article for kazal or asztag exists (both URLs 404).

Estimates: a loaf-shaped stack with steep sides and a rounded top, 10-30 m long, 4-6 m wide, 4-6 m high, at field
edges and in the TSZ yard by mid-October; small rectangular bales (about 0.8 x 0.45 x 0.35 m) stacked in blocks.
Round bales: not checked for 1985 Hungary (the first pass's concern stands).

## 9. The corn crib (góré)

Sourced facts:
- Usually rectangular in plan (a few round ones known); the entrance on the short side; a load-bearing beam frame;
  woven or slatted (lécezett) walls; in eastern Hungary half brick and half lath, built into the street-front brick
  fence; on Alföld farmsteads combined with other farm buildings, its wall serving as a pigsty fence or cart-shed
  wall; roofed in reed or shingle earlier, tile in the 20th century; **the floor raised off the ground**; spread in
  Hungary from the 18th century with maize. - https://hu.wikipedia.org/wiki/Góré
- No "góré" entry in the Magyar Néprajzi Lexikon's G volume (the headwords run gócseji viselet, 2-680, to gödény,
  2-681).

Estimates: 1.0-1.5 m wide (narrow so the cobs dry), 3-4 m high to the eaves, 4-10 m long, floor 0.5-0.8 m up on
brick piers or posts, vertical laths with gaps of about their own width, a gable or lean-to tile roof.

## 10. Road signs, place-name boards, km stones

Sourced facts:
- KRESZ (1/1975. (II. 5.) KPM-BM joint decree; current consolidated text): signs may be placed on a yellow or
  retroreflective yellow background (§10 (9)); direction signs (advance, direction, route confirmation) on a green,
  blue or white background (§17 (4)); the built-up-area sign is figures 131-131/c (§17 (1) s)); lettering in the
  figures is illustrative (§20/A). - https://net.jogtar.hu/jogszabaly?docid=97500001.kpm (the text is today's, not
  1985's; check the 1985 state before using the colours).
- Hungary signed the 1968 Vienna Convention on 8 Nov 1968 and ratified it on 16 Mar 1976; sign codes A (warning),
  B (priority), C (prohibitory), D (mandatory), E (information), F (indication); obsolete 1953 and 1962 sets. -
  https://en.wikipedia.org/wiki/Road_signs_in_Hungary
- **Sign faces as free files**: Commons holds the Hungarian set as SVG under **PD-HU-exempt** ("This work is not
  covered by copyright", Hungarian Act LXXVI of 1999, sections 1(4) and 1(7)), e.g.
  https://commons.wikimedia.org/wiki/File:Hungary_road_sign_E-001.svg ; historic designs (filed under "Kresz 83",
  the 1983 edition) e.g. https://commons.wikimedia.org/wiki/File:Hungary_road_sign_A-020_(Historic).svg ; the whole
  tree: https://commons.wikimedia.org/wiki/Category:Diagrams_of_road_signs_of_Hungary ,
  https://commons.wikimedia.org/wiki/Category:SVG_road_signs_in_Hungary (warning 62, prohibitory 78, mandatory 38,
  priority 22, additional 124, service 25 files), https://commons.wikimedia.org/wiki/Category:SVG_historic_road_signs_of_Hungary
  (15 files: 1953, 1962 and "historic" A-020, A-021, A-054, B-002, D-025, D-026, E-038, the old border sign). Signs
  are also filed by KRESZ figure number ("Category:Kresz 131" holds the built-up-area sign's photos). Some newer
  diagrams are CC0, e.g. the direction sign https://commons.wikimedia.org/wiki/File:Hungary_Sign_140.svg (Tenervfe,
  CC0, 400 x 109 mm in its metadata).
- Km stones: Commons has only a 2023 photo at Bodajk on Route 7
  (https://commons.wikimedia.org/wiki/File:Magyar_K%C3%B6z%C3%BAt,_Meilenstein_Hauptstra%C3%9Fe_7,_2023_Bodajk.jpg)
  and old milestones (https://commons.wikimedia.org/wiki/Category:Milestones_in_Hungary). No specification of the
  1980s km and hectometre stones was found.

Estimates: sign sizes in the Vienna Convention's normal sizes (triangle about 900 mm, disc 600-750 mm) on a 76 mm
steel post with the sign's bottom edge 2.0-2.2 m up in villages; the built-up-area sign about 1.0-1.5 m wide; km stone
a concrete post about 0.3 m wide, 0.6-0.8 m above ground, white with black numerals (the colours unverified).
Pick the 1975-design faces (the historic and "Kresz 83" files) over today's where they differ.

## 11. Trucks and tractors

Measured specs (for the basic models and to check any asset's scale):
- ZIL-130: 6,675 x 2,500 x 2,400 mm, wheelbase 3,800 mm, curb 4,300 kg, 5-6 t class; built 1962 (series from 1964)
  to 2012; tipper chassis ZIL-130D1/D2/D3 for the ZIL-MMZ-555. - https://en.wikipedia.org/wiki/ZIL-130
- IFA W50: 6,530 x 2,500 x 2,600 mm (the technical table; the infobox says 5,300 mm long), wheelbase 3,200 mm (3,700
  long), track 1,700/1,778 mm, ground clearance 300 mm, 8.25-20 tyres, payload 5,300 kg (1967-1990), built July
  1965 - December 1990, about 572,000 at Ludwigsfelde; about sixty models; 4x4 LA versions; Ikarus's 211 midibus used
  its parts. - https://en.wikipedia.org/wiki/IFA_W50
- MTZ-80/82: 3,815 x 1,970 mm, height 2,470 mm as the table gives it (low for the cab roof; check), wheelbase
  2,370 mm, rear track 1,400-2,100 mm, front 1,200-1,800 mm, ground clearance 465-470 mm, 80 hp (58.8 kW), closed
  ventilated cab, built from 1974, 1,496,200 by 1995. - https://ru.wikipedia.org/wiki/МТЗ-80
- Zetor: Unified Range I model years 1968-69 (2511, 3511, 4511, 5611), 1972 (4712, 5711, 6711), 1977-78 (4911, 5911,
  6911), 1980-81 (5011, 6011, 6045, 7011, 7045), 1984 (5211, 5245, 6211, 6245, 7211, 7245), 1986 (7711, 7745);
  the first digits are about the horsepower; Unified Range II (80-160 hp) from 1968; Crystal 1969 to the early 1980s,
  more than 45,000 sold. - https://cs.wikipedia.org/wiki/Zetor ; Crystal 12011 designed in the 1970s -
  https://en.wikipedia.org/wiki/Zetor . Dimensions not found; estimate 3.9-4.2 m long, 1.9-2.1 m wide, 2.6-2.7 m to
  the cab roof.
- Hungarian makes worth a model: the Csepel lorry (D-344, D-450 families) and the Ikarus buses (first pass).

New free picks (Sketchfab search API, 3 Nov 2026,
https://api.sketchfab.com/v3/search?type=models&downloadable=true&q=... with q = "tractor mtz", "zil 130", "ifa w50",
"zetor", "csepel"; pages not read, see section 0):
- MTZ-80 (Belarus) - arxwizard (Stanz) - CC-BY - 208,011 faces - published 7 Jul 2026 -
  https://sketchfab.com/models/1d91100976d6454d940c0b006b563364
- MTZ Traktor - Ivan_57 - CC-BY - 46,937 faces - 12 Apr 2023 - https://sketchfab.com/models/fc482cfd799e4fc6a34a5f79070dde58
- Zil 130 3D Model - dzik (Dzikus) - CC-BY - 6,528 faces - 7 Sep 2025 - https://sketchfab.com/models/d71e38e4077f420781b5927d78d1e373
- Zil 130 Siluet - AndrewMad (Andrew Mad) - CC-BY - 3,686 faces - 10 Jul 2019 (far LOD) - https://sketchfab.com/models/1fc719c99c844f50ac5e368e91fce938
- Zil 130 - tornear - CC-BY - 48,533 faces - 31 Mar 2020 - https://sketchfab.com/models/4da577d89c0b4a61967236cee9d745b5
- IFA W50 attempted scan - Jordano_Lucas (JordanoLucas) - CC-BY - 1,000,080 faces (photogrammetry of a real truck) -
  29 Aug 2022 - https://sketchfab.com/models/dce86f2f8f04423f9304d212dc0fbf62
- Csepel D344 no texture - Bazsi1986 - CC-BY - 840,182 faces, untextured - 5 Feb 2026 -
  https://sketchfab.com/models/0e28704340d74d1bb604034794bce8bb (the Hungarian lorry of the period)
- Csepel D-750 (truck crane) - hans1240 - CC-BY - 17,392 faces - 31 Jul 2025 -
  https://sketchfab.com/models/f7d09e429ae64e5498a31a48b8724185
- Zetor 25K tractor raw scan - vardyart (shamanek) - CC-BY - 1,594,646 faces (a scan) - 3 Apr 2022 -
  https://sketchfab.com/models/5373c11399564735bf7c9f6cef168684 (a 1940s-50s tractor; an old one still on a small
  farm in 1985)
- Provenance flags: "ZiL-130" by nghesiyeubuom97 (2018) and "zil-130" by Ara_my45yt (2022) have the same face count
  (174,063), and "Tractor Zetor" by Klooney (2018, the first pass's alternate) and "Zetor Tractor Layered" by burst24
  (2021) the same (69,232): one of each pair is a re-upload; use only the earlier uploader, and only after reading its
  page. Excluded: ZIL 133540 and ZIL 5423 by Korolv (Free Standard, not redistributable and not in the allowed list);
  Csepel-800 APC by 42manako (CC-BY-NC).

Trademarks (not legal advice; for the owner): the CC-BY licence covers the author's mesh, not the marque. ZIL, IFA and
Csepel lorries are no longer made by their old makers, which lowers but does not remove the risk; MTZ ("BELARUS",
Minsk Tractor Works) and Zetor are trading companies with live marks. The safe course: strip badges, nameplates and
lettering from every vehicle, give them generic names in the game's text ("5 t lorry", "wheeled tractor"), and for
MTZ and Zetor prefer a basic model built from the dimensions above (the project's basic-model route,
`data/scenery/basic_models.json`) over a faithful copy of the bonnet and grille.

## 12. A plan for the procedural building generator

Where it fits (read in the repo, 3 Nov 2026):
- The map lays out boxes (`MapPart`: House, Flats, Industrial, Farm, Tower, Tank, Chimney, CoolingTower, ...;
  `simcore/include/simcore/terrain_map.hpp`), and the boxes are the sight lines' obstacles (`MapData::ObstacleTop`,
  the review's passes in roadmap R6). Houses are 10-16 x 7-10 x 4.5-7 m, end-on to the street; farmsteads get a
  14 x 8 x 5 m house and barns of 16-26 x 8-12 x 5-8 m; a "tower" site is a 6 x 6 x 36 m box.
- The meshes are drawn on the unit cube (`HouseMesh(variant, lod)`, `BarnMesh`, `FlatsMesh`, `IndustrialMesh`,
  `Lathe`, `PrismX`, `SolidBuilder` of closed convex solids; `simcore/include/simcore/scenery/map_meshes.hpp`), the
  variant chosen by place (`map_dressing.hpp`), any part replaceable by a registered asset (`asset_register.hpp`).

The plan (track R6, its buildings; each step a commit with tests):
1. **B0, styles in the map (simcore, affects the simulation).** Give each house box a style chosen in simcore:
   `kocka` (square 8-11 m, set back behind a 5-10 m garden, about 7.5 m to the apex), `long_house` (5.5-7 m wide,
   12-25 m long, gable on the street line), `kocka_l` (L-plan, two boxes); farm boxes a style too (`cow_shed`,
   `machine_hall`, `grain_store`, `silo`, `long_house`); new parts for landmarks: `WaterTower` (round footprint,
   30-45 m in towns and large villages, a hydroglobe 20-30 m at farms), `Church` (nave box plus tower box), `Silo`.
   The shares by settlement kind and distance from the centre go in `data/scenery/map_dressing.json` or a new
   `data/scenery/buildings.json` (game values, cited to this file). Re-run `ground_check`, `crew_balance` and
   `helper_coverage` (the boxes move sight lines); the prototype mirrors the layout.
2. **B1, the grammar (presentation only).** `simcore/include/simcore/scenery/buildings.hpp`: a `BuildingSpec`
   (footprint rectangle or L; plinth height and colour; storeys and wall height; roof kind hip, gable along or across,
   half-hip, mansard, sawtooth, mono-pitch, flat; pitch; eaves overhang; facade bays: window axes, door, their sizes and
   sill heights; porch: side, depth, pillar spacing; chimneys; ridge vent) and `BuildingMesh(spec, seed, lod)` built
   with the existing `SolidBuilder`, `PrismX` and `Lathe`. The mesh fits the map's box exactly (the apex at the box's
   top), so what is drawn is what blocks sight. Specs per style read from `data/scenery/buildings.json`, every number
   carrying `source` and `confidence` (most "estimate" until the photo checks).
3. **B2, openings and material slots.** Windows and doors as thin closed solids 2-5 cm proud of the wall (keeping the
   closed-solid invariant), with face material ids (render, plinth, brick, tile, asbestos-cement, reed, glass, timber,
   concrete) carried in the TriMesh beside the colour; UVs in metres (planar per face) so the PBR set of
   `pbr_textures.md` tiles at real scale. Unreal maps ids to material slots in `FSimMapAssets`.
4. **B3, the styles.** Kocka (square and L, the two-axis front of tripartite windows, clad plinth, band, tent roof,
   side steps and canopy), long house (tornác on the yard side, gable to the street, tile or reed roof), cow shed
   (long, low, ridge vent, small high windows), machine hall (open portal frames), grain store, the church (nave, apse,
   west tower, helm or spire, cross or star), the water towers (mushroom, chalice, cylinder, star, hydroglobe as lathe
   profiles), farm silos (lathe), station elevator (box cells and head house).
5. **B4, props.** Poles by era (LV wood on a concrete stub, solid concrete; MV triangle head; A-frames at angles;
   pole-top transformer), wires as catenaries between the insulator sockets laid from the road's pole list; gémeskút
   (the lever posed at rest, a double well at pastures); kazal and bale stacks; góré; signs (a post and a face
   textured from the PD-HU-exempt SVGs, the 1975 designs) at junctions and village ends from the road graph; km and
   hectometre stones every 1,000 and 100 m along numbered roads.
6. **B5, LODs and instancing.** LOD0 full (openings, porch, chimneys), LOD1 walls and roof only, LOD2 the unit box
   (`UnitBox`, as now); a fixed variant count per style (for example 8 kocka and 6 long-house variants, seeded by
   place) so Unreal instances them through HISM like the trees; screen sizes tuned in the editor by the owner.
7. **B6, checks.** `tests/test_buildings.cpp`: every style closed and positive-volume, inside its box, the apex at the
   box's top, deterministic per seed, dimensions within the data's ranges, no NaN; `tests/test_map_meshes.cpp`
   extended; the prototype mirrors the specs if it draws them; a Blender render of each style from the reference
   photo's camera (P34 kocka, P33/P35 long houses, P41/P45 farm, P47/P48 water towers, P03 church) as
   `docs/MODELLING_FROM_PHOTOS.md` asks; `-SimMapShots` views V2, V3, V5, V6, V7 of `references.md` for the owner.
8. **B7, vehicles.** Basic models of the ZIL-130, IFA W50, Csepel D-344, MTZ-80 and a Zetor UR I from section 11's
   dimensions (no badges), with the CC-BY picks registered as replacements only after their pages are read and their
   badges removed.

Source list for the milestones: sections 1-11 above; `references.md` T6-T13 and photos P03, P12, P16, P17, P32-P48,
P57-P60; `built_models.md` sections 4.7-4.9, 4.16-4.18 and 6.

## 13. Left open (needs a web search or an archive visit)

- Measured drawings of Alföld long houses (the Szentendre Skanzen's catalogue; the Magyar Néprajzi Lexikon's
  "lakóház" and "tornác" entries were not located in this pass) and of the 1970s TSZ type plans (cow sheds, machine
  halls, silos; ÁGROBER/MÉM type plans).
- The kocka's roof pitch, plinth and wall heights from a measured source or a photo check (P34).
- The water-tower type plans (heights, tank sizes for the mushroom and chalice types) and which type each 1970s town
  built.
- Village church dimensions from a listed-building record (muemlekem.hu) for two or three plain villages.
- Whether perforated-web concrete poles stood in Hungarian villages in 1985 (a dated Fortepan photo).
- KRESZ sign sizes and the 1985 colours of place-name and direction signs; the 1980s km and hectometre stones.
- Zetor UR I dimensions; the MTZ-80's height (2,470 mm in ru.wikipedia looks low).
- The Sketchfab pages of the new vehicle picks (descriptions, badges, provenance) once the API's rate limit clears.
