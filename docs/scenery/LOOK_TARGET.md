# The look target

The project owner, 6 Nov 2026, with six screenshots of Kingdom Come: Deliverance II's countryside (Warhorse Studios,
2025): "This is the gold standard we should be aiming for. Use these as references against all the renders; analyze
this, note down each time what the differences are and aim for this quality. The lighting, the terrain variation, the
foliage and ground textures. [...] Although the terrain is slightly exaggerated, implementing this type of terrain
variation is a necessity."

So every render of the map, every plot and every view is judged against these pictures as well as against the
photographs of the place (`docs/research/TERRAIN_REFERENCES_2026-11.md`, `data/scenery/views.json`).

## What the pictures are for, and what they are not

- **A quality bar**, for light, relief, foliage, ground and how a landscape is layered. They show what a team with
  photogrammetry and years of work reaches in a Central European countryside; we aim at that quality with our own
  means (`TERRAIN_ROADMAP.md` part A).
- **Not a source of facts about our nation.** Pannonia is a fictional country of the Hungarian kind, on 15 October
  1985 (the scenarios' default date). The pictures are Bohemia in summer, about 1400, with sandstone rocks, thatched log
  huts and medieval strip fields. What the land grows, how big the collective farms' fields are, what the villages
  look like, the season's colours: those come from the photographs and sources of the place, as before. We compare
  **structure, density, light and value**; colour is compared by season.
- **Not copied.** The screenshots are the publisher's. They stay on the owner's PC in `reference/look/kcd2/`
  (git-ignored, like the other games' material in `reference/`), are never committed, traced, sampled or rebuilt, and
  no place, asset or layout of that game is made in ours. What we take from them is what they teach about real
  landscapes: relief at every scale, land use following the ground, closed edges, dense ground cover, worn tracks,
  light and air.

## The pictures (`reference/look/kcd2/`)

| Id | File | What it shows | What it teaches |
|---|---|---|---|
| K-a | `k1_track_between_wheat.jpg` | A rutted track between two wheat fields, dipping and rising to a village in a fold of the hills; wooded slopes either side; a castle on a crag; cumulus | The ground rolls at every distance; woods hold the slopes; a track is two ruts and a grass median, sunk into the ground with grassy shoulders; a boulder and a bush give the near field scale; flowers as points of colour |
| K-b | `k2_hill_meadow_valley.webp` | From a hilltop meadow over a valley: a wagon on a track, a wet meadow on the valley floor, woods on the far slopes, ridges fading to the horizon | Aerial perspective in steps; a valley floor of meadow and water; the meadow's grass dense with flowers and dry tall grass at its edge; trees as varied crowns, not a mass |
| K-c | `k3_fields_from_shade.webp` | From the shade of a tree: fields of different crops side by side, stony ground with dappled shade, blue flowers, a village and hills beyond | Patches of crop states with soft edges; dappled shade; the near ground strewn with stones and leaves |
| K-d | `k4_wheat_lone_tree.jpg` | Standing wheat beside a dirt road on a crest, a lone broad tree, a fence, a wide view over forested hills | A crop as a volume of ears; the field's edge belt of grass; a lone tree on a crest as a landmark; wooded hills to the horizon, layered and bluer with distance |
| K-e | `k5_rock_valley_meadow.jpg` | A meadow in a valley among sandstone rocks and pines, a track, a brook with stones, deer | The ground worn where the track runs; a brook's bed; poppies; light through haze in the valley. (Sandstone rocks are Bohemia's, not ours.) |
| K-f | `k6_village_pond.jpg` | A path past log huts and a willow by a pond, ferns, poppies, reeds, woods behind | Dense lush cover round a village; a pond with reeds and still water; a willow; the path's surface worn and muddy |

## The traits (K1-K14): what to judge every render against

| # | Trait | What the target shows | Where we deliver it |
|---|---|---|---|
| K1 | **Relief at every scale** | Rolling ground of tens of metres over hundreds of metres; ridges layered to the horizon; a track dipping and climbing; banks, hollows and humps of a metre or two near the eye | L11 (hill country), L3 (meso relief), L4 (micro relief) |
| K2 | **Land use follows the ground** | Woods on slopes, crests and valley sides; fields on gentle ground; meadows and wet patches on valley floors; villages in folds near water; a lone tree on a crest | L11, L2 (recipes matching on slope and wetness), L7 |
| K3 | **Patches with soft edges** | Fields of different states side by side, edges uneven, grass margins and balks between them, fences and hedges | L2, L4 (ground layers per pixel, edges by distance), L9 |
| K4 | **Edges layered and closed** | A wood's edge a closed wall of leaves from the ground to the crowns, in uneven heights: shrubs and young trees in front, crowns overlapping, no trunks seen from outside; an irregular canopy outline against the sky | L7, L6 |
| K5 | **Trees that read as trees** | Clumped foliage masses, dark inside and lit on the outside leaves; each species its own silhouette (a spreading oak, a tufted pine, a weeping willow); far woods as a canopy of crowns, not a box | L6, L10 |
| K6 | **Ground cover dense to the horizon** | Grass 0.2-1 m everywhere not worked, clumps of different heights and tints; flowers as small points of colour; dry tall grass at margins; far off, the cover becomes the ground's tint, never a bare texture | L5, L4 |
| K7 | **Crops as volume** | Standing wheat as a mass of ears with a soft edge and wind tilt, darker into its depth; a belt of grass between crop and track. (In our October: stubble, ploughland, standing maize and sunflower: the same principle, a crop is a volume of plants, not cards on bare earth) | L5 |
| K8 | **Tracks worn into the ground** | Two ruts of bare soil and a grass median, puddles, stones, the track a little sunk with grassy shoulders overhanging it; its edges irregular | L3 (the cross-section), L4 (a road mask in the ground material), plot 13 |
| K9 | **Things that give scale** | Boulders and stones, fallen branches, fences, poles, sheds, haystacks, carts, animals | L9, the recipes' scatter |
| K10 | **Light** | A warm, soft sun; a bright sky light filling the shadows with blue; occlusion inside foliage and grass; trees' shadows on the ground with soft edges; dappled shade | L12 (light and air) |
| K11 | **Sky and air** | Cumulus with shape and volume; their shadows on the land; aerial perspective in steps, far ridges bluer and paler; no brown haze | L12 |
| K12 | **Colour and value** | Natural, slightly muted greens (olive to blue-green) and the crop's golds; a full range from dark woods to bright sky; no neon greens or flat mid-greys; the ground's colour varies in patches | L12 (grading), L4, L6 (albedo calibrated against scans) |
| K13 | **Water with its banks** | A pond with reeds, still reflections and duckweed, a willow; a brook with a stony bed | L7 (reed belts), L11 (brooks), plot 14 |
| K14 | **Every distance has its detail, and no seams** | Near (1-10 m: blades, ruts, stones), middle (10-300 m: patches, edges, single trees), far (0.3-5 km: woods' outlines, ridges, villages); no visible ring where the cover ends, no grid in the land cover | L5 (the ring's falloff), L4, L10 |

## The gap, 6 Nov 2026 (renders c12, c14, d2)

The plot lab's latest renders against the traits. Mid-October, so the greens are fewer by right; the rest is not.

| # | Where we are | Gap |
|---|---|---|
| K1 | *(6 Nov, A.3.15: hill country in the west and north-west, 70 m forms with 4° median slopes; no relief near the eye yet, the plain unchanged)* Before: flat to the horizon. The map's relief is three gentle bands (10 m over 18 km, 4 m over 6 km, 1.2 m over 1.5 km) and some isolated hills of 8-70 m; the detail area's meso relief (0.3-2.5 m) does not show at all at the plots' cameras | **The largest.** No plot shows a slope |
| K2 | Woods are 100 m square blocks from the forest outlines and copses placed by noise; fields a grid of rectangles; neither knows the ground (there is none to know) | Large |
| K3 | Fields of 85-198 ha (right for 1985's collective farms, trait T2), household strips near villages; at 150 m the land cover's 25 m cells show as a staircase along a field's edge (p05 high) | Medium. Big fields stay (they are the period's); the hills will cut them, and their margins and balks are missing |
| K4 | First pass (A.3.13): bays and a lower mantle, but trunks seen through, lollipops and cone shrubs, a sparse grass fringe | Large |
| K5 | All kinds lollipops but the oak (a northern red oak in its October red, the wrong species) and the card hornbeam | Large (L6) |
| K6 | One to two clumps a square metre on a bare texture; black tufts on a pale ground (p03); nothing past the ring | Large |
| K7 | Standing maize a near-black wall of one card with a hard top (p03); the stubble's straw thin | Medium |
| K8 | Roads and tracks are flat pale strips laid on the ground (p16 low) | Large |
| K9 | Only the military boxes | Large (later, L9) |
| K10 | Shadows, but the foliage lit flat by its vertex colours; no occlusion in grass | Medium |
| K11 | The scenario's weather is broken stratocumulus, but the plots show a cloudless sky (d2) or a grey sheet (p03); a strong brown haze at 150 m (p05 high) | Medium; cheap to start |
| K12 | Lime and yellow crowns (d2), a uniform brown ground (p16), black maize (p03) | Medium |
| K13 | The river bank plot not judged since a0 | Unknown |
| K14 | The cover ring ends at a visible distance; the land cover's staircase (K3) | Medium |

## How it is used

- **Each plot lab pass** (`TERRAIN_ROADMAP.md` A.4): the page shows the look target's pictures beside the renders
  (`tools/plot_lab.py`, from `reference/look/` where the PC has them; `data/scenery/look_target.json` lists them and
  the traits). The pass's entry in the roadmap (A.3.x) ends with a line **"Against the look target"**: the traits it
  moved and the largest gaps left, by number.
- **This page's gap table** is rewritten when a phase ends, not at every pass.
- **Like with like.** The pictures are summer, under fair-weather cumulus. The plot lab renders its own scenarios
  (`data/scenarios/lab/`, kept out of the missions menu): fair cumulus on 15 October at 13:00 by default, or on 5 July at
  15:30 with `--season summer`, so structure and density can be compared at the same season (plots anchored on an
  October field state may find none in July). Judging the October renders, compare value, density, structure and
  light, not hue.
