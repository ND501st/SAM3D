# Fab list: what to add from the launcher (4 Nov 2026)

All are free, by **Quixel Megascans** on Fab (licence: Fab Standard). In the Epic Games Launcher, open Fab, search the name,
open the listing and press **Add to My Library**. For the five tree packs then press **Add to project** and pick
`C:\dev\SAM3D-wt\terrain\unreal\SAM3D`. (They are Unreal Engine format only, so there is no file to download from the web.)

## Needed now: tree packs, Add to project

| Search for | Used as | Notes |
|---|---|---|
| European Hornbeam | `tree_hornbeam` (oak-hornbeam woods, the mantle) | Already added to the library of the account signed in to the launcher (4 Nov); "Add to project" not pressed |
| European Black Alder | `tree_alder` (wet ground) | |
| Norway Maple | `tree_maple` (stand in the edge mantle; stands in for field maple) | |
| European Beech | `tree_beech` (beech-hornbeam stands) | |
| Common Hazel | `tree_hazel` (shrub fringe) | A shrub pack |

## Optional: plants (FBX or glTF, can also be downloaded from the web)

| Search for | Used as | Notes |
|---|---|---|
| Elderberry | `tree_elder` | Already in the project |
| European Spindle | a second fringe shrub | Not used yet |
| Wild Grass | grass cards | Thin plants stay 2D cards |
| Dead Shrubs | marsh and field-edge dressing | Not used yet |
| Black Locust | not a tree: a 214 MB thicket of suckers | Skip; the locust tree is to be generated |

## Not on Fab for free

Poplar (Lombardy, hybrid), a black locust tree, pine and fruit trees. These are to be generated (EZ-Tree or Blender's
Sapling), then run through the same pipeline.

## After you add them

Tell me. I will move the packs under `/Game/ThirdParty/`, register each with its licence in `data/scenery/assets.json`, and
lay out the distance ladders (the near model, then the lower levels, then the spindle).

## Findings, 4 Nov 2026 (the launcher's Fab, signed in to the account that holds the library)

- **The five tree packs** (Hornbeam, Black Alder, Norway Maple, Beech, Common Hazel) offer only "Add to project", and the
  launcher's project list does not show SAM3D (only two "MyProject" entries), so they cannot be taken from the launcher.
  They would have to come in through the editor's own Fab window. Not done.
- **Megaplants** (hornbeam, hazel, maple, spindle...) are procedural assets for Unreal's vegetation editor. The USD
  download of "Megaplants: European Hornbeam" (114 MB) turned out to be 426 MB of raw Nanite geometry named
  `Tree_English_Oak_Forest_*`: an oak, not a hornbeam, and far too heavy for instancing. Left out.
- Downloads from the launcher land in `D:\VaultCache\FabLibrary\<name>-<id>\<format>\`.
