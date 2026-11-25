# Third-party art on the map: the asset register

Roadmap R6 ("a more realistic map"). The map's trees, wood masses, houses, barns, flats and industrial halls are drawn
from simcore's own procedural meshes (`simcore/scenery/map_meshes.hpp`). Any of them may be replaced by a real model,
free for use in Unreal (Fab, Epic, Quixel) or online under a permissive licence (CC0, CC-BY), as a placeholder until
SAM3D's own art exists. Every such model is recorded in a register, as every data value's source is in
`docs/SOURCES.md`: where it came from, who made it, its licence and whether its files may be redistributed.

**The register starts empty: every part is procedural.** A model is added only with the project owner's say-so, one
file at a time (nothing is downloaded otherwise). The eight zips in `art/placeholder cold war models/` have no licence
record: they are not registered, not used and not committed until the owner says where they came from. Their listings
were read on 20 November 2026 (roadmap S9, nothing extracted): each holds only `source/<name>.zip` (an FBX and its
BMP textures) and `textures/*.png`, with no readme, licence or credits file; the FBX headers' author, title, subject,
keywords and comment are empty and the creator is "Blender (stable FBX IO) 2.79". So none states its origin, author
or licence: `9k31-strela-1-sa-9-gaskin.zip`, `bmp-1p.zip`, `btr-70.zip`, `mt-lbm.zip`, `t-62-obr-1975.zip`,
`t-72a-obr-1980.zip`, `zil-131.zip`, `zsu-23-4-shilka.zip`. The owner is to say where they came from.

The map's ground and ribbon textures (`art/map/textures/`, S9) are not third-party art: `tools/map_textures.py` draws
them from seeded noise, so they are the project's own and need no entry.

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
  }
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
  modelled at full size, 10 m long, takes a scale of 0.1. `z_offset_m` is in the part's frame too: for a tree, 1 is its
  height (the oak's -0.07 puts its exposed roots into the ground; a pick's `z_offset_m` carries it through import).
  A Fab pick's `"sink_m": "auto"` measures the root flare instead and lowers the mesh before the fit (A.3.27).
- `parts` maps each part key (`MapMeshParts()`: `tree_oak`, `tree_poplar`, `tree_pine`, `tree_acacia`,
  `canopy_mass`, `house`, `barn`, `flats`, `industrial`) to an asset id or `null` (procedural); `variants` gives one
  per variant (for `flats` the variant is the floor count). An unknown key, or an id not registered, is refused.

In the game, a part whose asset is not on this machine is drawn procedurally, and the log says so once
(`Map assets: house#1's asset house_b is not on this machine ...`). Every machine therefore runs, with or without the
third-party files.

## Getting a model onto the map (roadmap T5-T8)

`docs/scenery/T5_T8_PLAN.md` section 12: `tools/fetch_models.py` (Poly Haven), `tools/prepare_model.py` (Blender fits the part's
frame), `tools/ue_import_model.py` (imports it and writes the register); the picks are in `data/scenery/model_picks.json`. The
part keys now include the ground clutter's `clutter_grass`, `clutter_straw` and `clutter_reed`.

## A Fab asset already in the project (A.3.25)

A Megaplant (skeletal, from the Procedural Vegetation Editor) or a Megascans plant that Fab's window put in the project
(`Content/Megaplant_Library/`, `Content/Fab/`, both ignored by git) is never exported. `tools/ue_fab_part.py` copies it in
Unreal with Geometry Script, fits it to the part's frame and saves a Nanite static mesh under `/Game/ThirdParty/Fab/<id>/`,
with the source's materials (a pick's `season` gives the foliage a child instance with the master's Seasons switch on).
Its pick names the source by `unreal_source`. A Megaplant's wind (its bones) does not come across.

A part may list **`alternates`**: other models of the same thing (a Megaplant's variants A-D), the first its `asset`. The
map draws each tree with the alternate its place picks (`AssetRegister::AlternateAt`, a hash of the place: the same on every
machine), so a row is not one tree repeated. `variants` stay a tree's ladder of thinner levels (`tools/ue_tree_lods.py`);
a part drawn by a Fab model has none (Nanite is its level of detail).

A part may also list **`near`** models (A.3.26): `{"alternates": [...], "within_m": 80}`. Drawn everywhere, a Megaplant
cost 150-1340 ms a frame (A.3.25); so a tree within `within_m` of the camera is drawn by a near model (picked by its place,
as alternates are) and beyond by the part's own (its cards), whose material (`M_SimFoliage`'s `NearHideM`) hides each
instance whose pivot is within that distance. Both by distance on the GPU, no work per frame. If any near model is not on
the machine, the part's own draws everywhere and hides nothing. `tools/ue_fab_part.py` registers its models as near models.

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

## Textures: `data/scenery/textures.json`

The ground's textures (track T, `docs/scenery/TERRAIN_ROADMAP.md` section 4) have a register of their own, with the
same rules: each entry gives its `site` (`polyhaven` or `ambientcg`), the site's `asset` id, `licence`, `author`,
`origin`, `resolution`, the scan's real width `scan_m` and what it is `for`, and optionally `crop_u`, the part of the
scan's width kept (and stretched back) where only its middle tiles along U. All ten are CC0. They are fetched per
machine, never committed:

```bash
python tools/fetch_textures.py --sizes        # what would be downloaded, MB per texture (204 MB for all ten at 2k)
python tools/fetch_textures.py                # albedo, normal (DirectX) and orm.png into art/thirdparty/textures/<id>/
```

The tool takes Poly Haven's files from its API (JPEG first) and ambientCG's 2K-JPG zips, keeps only the colour, the
DirectX normal, the roughness and the occlusion, and packs `orm.png` (R occlusion, G roughness). `--record` writes
each file's SHA-256 into the register. A texture is added like a model: with the owner's say-so, its licence checked on
its page that day. Its test: `tools/tests/test_fetch_textures.py`.

## Credits

The game's credits list every registered CC-BY asset: its title (the `notes`), its author, the licence and the origin,
as CC-BY 4.0 requires. CC0 and the owner's own models are credited as good practice (the ground's textures: Poly Haven's
authors and ambientCG, as `textures.json` names them). No CC-BY asset is registered yet.

## Checks

- `simcore/tests/test_scenery.cpp`: the register loads, starts empty with every part procedural, and refuses each
  broken rule (a non-redistributable asset outside `/Game/ThirdParty/`, Fab marked redistributable, CC-BY without an
  author, an unknown licence or part, an id twice, a part naming an unregistered asset).
- `simcore/tests/test_map_meshes.cpp`: every procedural part, variant and LOD is a closed mesh wound outward with
  positive volume; buildings fill the unit cube with their top at the box's top; trees stand one unit tall.
- `tools/check_assets.py` and its test: the repository against the register.
