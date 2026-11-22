# Third-party art: the asset register

Roadmap R6 ("a more realistic map"). The map's trees, wood masses, houses, barns, flats and industrial halls are drawn
from simcore's own procedural meshes (`simcore/scenery/map_meshes.hpp`). Any of them may be replaced by a real model,
free for use in Unreal (Fab, Epic, Quixel) or online under a permissive licence (CC0, CC-BY), as a placeholder until
SAM3D's own art exists. Every such model is recorded in a register, as every data value's source is in
`docs/SOURCES.md`: where it came from, who made it, its licence and whether its files may be redistributed.

A model is added only with the project owner's say-so (nothing is downloaded otherwise). The map's parts are all still
procedural. **The aircraft and the air-to-ground weapons are drawn by models from Sketchfab** (the owner, 22 Nov 2026:
"replace the in-game models ... with various 3d models on sketchfab ... start with aircraft and air to ground weapon
systems"; "these will serve as superior placeholders"): see below. The eight zips in `art/placeholder cold war models/` have no licence
record: they are not registered, not used and not committed until the owner says where they came from.

## The register: `data/scenery/assets.json`

Read by `simcore::scenery::AssetRegister` (`simcore/scenery/asset_register.hpp`), in Unreal by `FSimMapAssets`.

```json
{
  "assets": [
    {"id": "oak_a", "unreal_path": "/Game/ThirdParty/Trees/SM_OakA.SM_OakA",
     "origin": "https://... (the page it was fetched from)", "author": "...", "licence": "CC-BY-4.0",
     "redistributable": true, "fetched": "2026-11-02", "sha256": "...",
     "scale": 0.0667, "yaw_deg": 0, "z_offset_m": 0, "notes": "..."}
  ],
  "parts": {
    "tree_oak": {"asset": "oak_a"},
    "house": {"asset": null, "variants": [null, "house_b"]}
  },
  "models": {"mig21": "sketchfab_mig21_manilov"},
  "munitions": {"mk82": "sketchfab_mk82_professore12"}
}
```

- `licence` is one of `CC0-1.0`, `CC-BY-4.0`, `Fab-Standard`, `Epic-Content-Only`, `Own`.
- `redistributable`: may the files themselves go into git? CC0 and CC-BY: yes. Fab's Standard licence and Epic's
  content-only licence allow use in a game built with Unreal but not handing the files on: **false**, and the register
  refuses them as redistributable.
- An asset that may not be redistributed **lives under `/Game/ThirdParty/`** (`unreal/SAM3D/Content/ThirdParty/`,
  ignored by git): the register refuses it anywhere else.
- **CC-BY names its author**, for the credits below.
- `scale`, `yaw_deg`, `z_offset_m`: the fix from the model's frame into the part's (a unit cube for a building or a
  wood's mass, centred, x along its length; a tree one unit tall from its base, its crown one unit across). A house
  modelled at full size, 10 m long, takes a scale of 0.1.
- `parts` maps each part key (`MapMeshParts()`: `tree_oak`, `tree_poplar`, `tree_pine`, `tree_acacia`,
  `canopy_mass`, `house`, `barn`, `flats`, `industrial`) to an asset id or `null` (procedural); `variants` gives one
  per variant (for `flats` the variant is the floor count). An unknown key, or an id not registered, is refused.
- `models` maps a basic model's id (`data/scenery/basic_models.json`) and `munitions` a munition's id
  (`data/scenery/munitions.json`) to an asset drawn whole in its place, or `null`. Such an asset is the whole model at
  full size in the model's own frame (x forward, y right, z up, metres; the origin at the middle of the fuselage or the
  body, on its axis), so a munition's takes no fix (scale 1, no yaw, no offset; the register refuses one). In Unreal
  `FSimBasicModels` draws the model by it (one mesh, the side's colour override on every slot) and
  `FSimMunitionModels` the munition (a state that changes its shape, the Snakeye's blades open or the wings folded,
  stays as the asset is).

In the game, a part whose asset is not on this machine is drawn procedurally, and the log says so once
(`Map assets: house#1's asset house_b is not on this machine ...`). Every machine therefore runs, with or without the
third-party files.

## Fetching the files on a machine

1. Get the model from the `origin` recorded in the register, under the licence recorded there (a Fab asset: add it to
   the project from the Fab plugin or the launcher into `Content/ThirdParty/<group>/`).
2. Import or move it so that its object path is the register's `unreal_path`.
3. Check the file's SHA-256 against `sha256` where one is recorded (the file as downloaded, before import).
4. Run `python tools/check_assets.py`: nothing under `unreal/SAM3D/Content/ThirdParty/` or `art/thirdparty/` may be
   tracked by git, and every non-redistributable asset's file must be ignored. Its test is
   `tools/tests/test_check_assets.py`.

The source files of redistributable models (CC0, CC-BY), if kept at all, go under `art/thirdparty/`, which is also
ignored until a model's licence and size have been agreed for git.

## Aircraft and munitions from Sketchfab

Each model is a downloadable Sketchfab model under CC-BY 4.0, listed with what it draws in
`tools/sketchfab/models.json` (its page, author, licence, the game's ids, and how to prepare it). The pipeline:

1. **Fetch**: `python tools/sketchfab_fetch.py --all`, with your own account's token in `SKETCHFAB_API_TOKEN`. Each GLB
   goes to `art/thirdparty/sketchfab/<id>/source.glb` and is checked against the register's `sha256`.
2. **Prepare** in Blender: `blender --background --python tools/blender/sketchfab_prepare.py -- --all`. Joined into one
   mesh, turned nose to +x (a model posed at an angle in its file straightened by its plane of symmetry), scaled to the
   game's `length_m` (a helicopter's fuselage, without the main rotor's overhanging blades), its origin put on the
   fuselage's or body's axis at the middle of its length, the landing gear removed from aircraft that fly with it up,
   look-alike materials merged, decimated (60,000 triangles an aircraft, 6,000 a munition) and textures capped (2,048
   and 1,024 px). It writes `SM_SF_<Name>.glb` and `check.png` (side, top and front with the origin's axes): look at
   every one before importing.
3. **Import** into Unreal: `UnrealEditor-Cmd.exe <project> -ExecutePythonScript="tools/ue_import_sketchfab.py"
   -unattended -nullrhi -nosplash` (the editor without a window, which it closes; the pythonscript commandlet cannot
   make LODs). Each mesh goes to `/Game/ThirdParty/Sketchfab/<id>/SM_SF_<Name>`, Nanite off (the canopies are
   translucent), four LODs (100, 50, 20 and 6 per cent), no collision. Its log line gives the bounds in cm and which
   end the nose is: check them against `length_m`. The editor may crash on its way out in the Cesium plugin, after
   everything is saved.

Every file stays out of git (`art/thirdparty/`, `Content/ThirdParty/`): CC-BY would allow committing them, but their
size has not been agreed. A machine without them draws the basic shapes and the procedural munitions.

**Left out.** Sketchfab models that are rips of other games (titles naming War Thunder, DCS World or WARNO, or uploads
whose face counts match those games' models) were not taken: their CC-BY is not the uploader's to give. No free model
was found for the BQM-74, Saab 105, G-2 Galeb or UTVA-75; they keep their basic shapes, as does every ground vehicle and
every munition not listed below. Known flaws: the An-26 is drawn by an An-24 and the Il-20 by an Il-18 (the same
airframes without the role's fit); the RC-135 by the KC-135R; liveries and markings are the models' own (US, Soviet,
Swedish, German, Aeroflot), not the war's factions; helicopters keep their gear (most are fixed) and their rotors are
still.

## Credits

The game's credits list every registered CC-BY asset: its title (the `notes`), its author, the licence and the origin,
as CC-BY 4.0 requires. CC0 and the owner's own models are credited as good practice. Each was modified (re-oriented,
scaled, its gear or a launcher removed, decimated, materials merged), as the register's `notes` say.

| Draws | Author | Model | Licence |
|---|---|---|---|
| mig21, mig21mf, mig21bis | manilov.ap | [Mig21](https://sketchfab.com/3d-models/mig21-4e2b33b33fcb43948c78783867e3b166) | CC-BY-4.0 |
| mig23ml_wp, mig23mld, mig23ml | Tim Samedov (citizensnip) | [Mig-23 MLD](https://sketchfab.com/3d-models/mig-23-mld-7a13c91f07e042a685b4d265644fdc06) | CC-BY-4.0 |
| mig27 | manilov.ap | [Mig27](https://sketchfab.com/3d-models/mig27-a9d22acdb582444a91c2adc8b123d674) | CC-BY-4.0 |
| mig25rb, mig25bm | manilov.ap | [Mig25](https://sketchfab.com/3d-models/mig25-afe67617c5294dce9056106a22f640ff) | CC-BY-4.0 |
| su24 | manilov.ap | [Su24](https://sketchfab.com/3d-models/su24-3439ae4000df438894a44127ebda538c) | CC-BY-4.0 |
| su25 | Immersive3D (Immersive3A) | [[PBR] Sukhoi Su-25](https://sketchfab.com/3d-models/pbr-sukhoi-su-25-956d0cb0bbe54fa8ad72e8c3e224fd2d) | CC-BY-4.0 |
| su17m3, su22m3 | manilov.ap | [Su17](https://sketchfab.com/3d-models/su17-a5dfc3d488c8441c8468a75ffad1ab75) | CC-BY-4.0 |
| yak28pp | Tim Samedov (citizensnip) | [Yakovlev Yak-28P](https://sketchfab.com/3d-models/yakovlev-yak-28p-d8fb2ad3d871437392453568c0c8bfe8) | CC-BY-4.0 |
| mi24, mi24d | manilov.ap | [Mi24](https://sketchfab.com/3d-models/mi24-5d0b5d8dc985401685f217a775deadad) | CC-BY-4.0 |
| mi8, mi8_doorgun | manilov.ap | [Mi8](https://sketchfab.com/3d-models/mi8-1bc2f674ad8345648710f16df847599a) | CC-BY-4.0 |
| mi2 | manilov.ap | [Mi2](https://sketchfab.com/3d-models/mi2-b3eac46b00c94b2d8f601ddee0150133) | CC-BY-4.0 |
| an2, an2_bomber | manilov.ap | [An2](https://sketchfab.com/3d-models/an2-8f62ab4eacbc430186d85a7029d7d156) | CC-BY-4.0 |
| l39 | manilov.ap | [L39](https://sketchfab.com/3d-models/l39-4b37b2b4ae9e40ac9a6fb876e2142672) | CC-BY-4.0 |
| an26 | manilov.ap | [An24](https://sketchfab.com/3d-models/an24-49b37176c8d34a028e22b99f78239bc3) | CC-BY-4.0 |
| il20 | manilov.ap | [Il18](https://sketchfab.com/3d-models/il18-8293d12da39d4b6a94d892f7c0dd288c) | CC-BY-4.0 |
| a50 | MaX3Dd | [Beriev A50](https://sketchfab.com/3d-models/beriev-a50-e0a7e6d5dfe54ce986870103fb9916c0) | CC-BY-4.0 |
| f4g, f4e, rf4c | manilov.ap | [F4](https://sketchfab.com/3d-models/f4-f7d8189af9fb40db9b4968830aabe29e) | CC-BY-4.0 |
| f16a | manilov.ap | [F16](https://sketchfab.com/3d-models/f16-4eecf423b8454c2ba2a371a7bfe9f157) | CC-BY-4.0 |
| f111f, ef111a | manilov.ap | [F111](https://sketchfab.com/3d-models/f111-00e9e6b9dbc34dc1ae691529688ab000) | CC-BY-4.0 |
| tornado_ids | manilov.ap | [Tornado](https://sketchfab.com/3d-models/tornado-9fd925dabddc4c1998b7c3df5ac47bf8) | CC-BY-4.0 |
| a10a | manilov.ap | [A10](https://sketchfab.com/3d-models/a10-8fec9884fe07476bbcaa34d9521726c4) | CC-BY-4.0 |
| f15c | manilov.ap | [F15](https://sketchfab.com/3d-models/f15-41f899ab2bef4cc3a1a40929569d5f96) | CC-BY-4.0 |
| ah1s | manilov.ap | [Ah1](https://sketchfab.com/3d-models/ah1-a30fb981effd439fb046a7318f0c2065) | CC-BY-4.0 |
| e3a | manilov.ap | [E3d](https://sketchfab.com/3d-models/e3d-72e914f9064546f7a0185bc2e80185e4) | CC-BY-4.0 |
| kc135, rc135 | Muhamad Mirza Arrafi (MirzaArrafiERV_45) | [Boeing KC-135R Stratotanker](https://sketchfab.com/3d-models/boeing-kc-135r-stratotanker-86f1e11310e546db9bf323fa1a62f069) | CC-BY-4.0 |
| draken_gun | manilov.ap | [J35](https://sketchfab.com/3d-models/j35-3d6ddbf606dc4047a15d37f4d5ac56ca) | CC-BY-4.0 |
| ab212 | manilov.ap | [Uh1](https://sketchfab.com/3d-models/uh1-3131460d00184d90855e4988c7d54101) | CC-BY-4.0 |
| alouette3 | DesertFox (desertfox2) | [Alouette III](https://sketchfab.com/3d-models/alouette-iii-23a6adddf7104c28b8535352589a16a7) | CC-BY-4.0 |
| agm65b, agm65d | xFORTUNAx | [AGM-65B Maverick Missile - Low Poly, Textured](https://sketchfab.com/3d-models/agm-65b-maverick-missile-low-poly-textured-a4336b95b8364dd78a52b8426598f789) | CC-BY-4.0 |
| kh29l | xFORTUNAx | [Kh-29T Air-to-Surface Missile — Low Poly](https://sketchfab.com/3d-models/kh-29t-air-to-surface-missile-low-poly-d13c7039142d46b5b61baef9db11fa80) | CC-BY-4.0 |
| agm45 | xFORTUNAx | [AGM-45 Shrike Missile - Low Poly, Textured](https://sketchfab.com/3d-models/agm-45-shrike-missile-low-poly-textured-fc7a02694f85481481e282ac853a5c76) | CC-BY-4.0 |
| kab500l | xFORTUNAx | [KAB-500L Laser-Guided Bomb - Low Poly, Textured](https://sketchfab.com/3d-models/kab-500l-laser-guided-bomb-low-poly-textured-1c62622133414296887802cc43e03f14) | CC-BY-4.0 |
| gbu12 | xFORTUNAx | [GBU-12 Paveway II Laser-Guided Bomb](https://sketchfab.com/3d-models/gbu-12-paveway-ii-laser-guided-bomb-c7451f92b15345819c566f4a5080bccc) | CC-BY-4.0 |
| s8 | xFORTUNAx | [S-8KOM 80 mm Rocket — Low Poly](https://sketchfab.com/3d-models/s-8kom-80-mm-rocket-low-poly-06f720ad261041719210e60de6a94dd7) | CC-BY-4.0 |
| hydra70 | xFORTUNAx | [Hydra 70 (2.75" FFAR) Rocket — Low Poly](https://sketchfab.com/3d-models/hydra-70-275-ffar-rocket-low-poly-50bf3b3cd8ec4823801ca8507ec78008) | CC-BY-4.0 |
| agm88a | Tihmed | [AGM-88](https://sketchfab.com/3d-models/agm-88-7e7141cb50d74faab5792ee38f27532b) | CC-BY-4.0 |
| kh58 | Narni_Tchereskov | [KH-58/U Anti-Radiation Missile](https://sketchfab.com/3d-models/kh-58u-anti-radiation-missile-765d0250b6a0434b97d6e0434032f4da) | CC-BY-4.0 |
| ofab250 | 4ikalka | [OFAB-250-270](https://sketchfab.com/3d-models/ofab-250-270-7276306219f5499296979764c3c50fe2) | CC-BY-4.0 |
| bgm109c | rickslash | [BGM-109 Tomahawk missile](https://sketchfab.com/3d-models/bgm-109-tomahawk-missile-da71eb1c023b444aba15ff37710e9613) | CC-BY-4.0 |
| agm86c | mahmadsaleem11111 | [AGM-86](https://sketchfab.com/3d-models/agm-86-de64a3d9282249e39760a5fee425e7ae) | CC-BY-4.0 |
| tow | yousef.hameed (yousef.abofara.pc) | [BGM-71 TOW](https://sketchfab.com/3d-models/bgm-71-tow-e886023693724445b3b00fea44025935) | CC-BY-4.0 |
| mk82 | photon (that one larry) (Professor_E12) | [MK82](https://sketchfab.com/3d-models/mk82-dc32a37d20cf41849d7021120095e1e0) | CC-BY-4.0 |

## Checks

- `simcore/tests/test_scenery.cpp`: the register loads, its `models` and `munitions` keys are the libraries' own and
  every part is procedural, and it refuses each broken rule (a non-redistributable asset outside `/Game/ThirdParty/`,
  Fab marked redistributable, CC-BY without an author, an unknown licence or part, an id twice, a part, model or
  munition naming an unregistered asset, a munition's asset with a fix).
- `simcore/tests/test_map_meshes.cpp`: every procedural part, variant and LOD is a closed mesh wound outward with
  positive volume; buildings fill the unit cube with their top at the box's top; trees stand one unit tall.
- `tools/check_assets.py` and its test: the repository against the register, and its `models` and `munitions` ids
  against the libraries.
