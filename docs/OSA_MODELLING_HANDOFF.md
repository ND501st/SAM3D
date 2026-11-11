# The Osa's interior from the book's photos: handoff, and why the geometry stays wrong

*Written 10 Nov 2026 at the end of the session that built the Osa-AK interior from* SA-8 Gecko in Detail *(source key
`wwp_gecko`), 6-10 Nov 2026. For the next session (Claude Fable 5.2), which is to find where accuracy is lost and rebuild
the process from the ground up. The prompt for that session is section 11.*

## 0. Where to work, and the state it is in

- **Worktree `C:\dev\SAM3D-wt\osa-book`, branch `feature/osa-book`.** Not pushed. Everything generated (`out/recon/...`:
  photos, depth maps, solve, renders, sheets) is git-ignored and exists **only in this worktree**. Start the session here.
- Committed: `da3119f` (the holistic layer and the comparison), `f0ecab2` (depth fused with traces; the sloped housing,
  rear fittings, floor and seats).
- **Uncommitted** (sections 38-41 of `docs/OSA_INTERIOR_HOLISTIC.md`): the seats on the bench, the right post rebuilt as
  a channel with the cover and latch, the strut added and then removed. Files: `art/cabin_interior/SM_OsaHull.fbx`,
  `SM_OperatorSeat.fbx`, `manifest.json`, `data/cabins/osa_ak.json`, `data/cabins/osa_ak_shell.json`,
  `docs/OSA_INTERIOR_HOLISTIC.md`, `docs/research/OSA_INTERIOR_DIFF_2026-11.md`,
  `tools/blender/build_cabin_interior.py`. The owner decides whether to commit them before the rebuild.
- Unreal has not re-imported `SM_OsaHull`, `SM_OperatorSeat` (and earlier meshes). Claude cannot run the editor.

## 1. The problem

**In the owner's words (10 Nov 2026):**

> If I passed a reference image to an image generation model, it would be able to accurately identify each component,
> and trace, edit or modify (e.g. change colour) each component accurately. Yet even with depth maps, perspective tools,
> traces, dot maps and various other tools being used, the exact geometry cannot be replicated in a 3D model, where the
> inaccuracies can be obviously identified by the human eye. Need to find out where the issue is in the pipeline (e.g.
> identification, 3D modelling process in Blender, tracing, comprehension) and rebuild from the ground up.

**What it looks like in practice.** For four days the owner has looked at a render beside a photo and seen errors at a
glance: a part at the wrong angle, two boxes where the photo has one sloping pressing, a post in two pieces, a leg raked
45 degrees where the photo has 15, seats floating, a missing raised floor. Each time, the pipeline had run, the score
had usually *risen*, and Claude had reported the part fixed. Each fix then took two or three rounds of the owner
pointing at the specific feature.

**The asymmetry to explain.** In 2-D, a photo's components are easy: an image model (or Claude's own vision) names each
one and could outline it to a few pixels. The loss happens somewhere between "this region of the photo is the leg of
the post" and "the leg in the 3-D model is a solid with these vertices". Section 6 sets out where, with the evidence.

## 2. Inputs, frames and conventions

| | |
|---|---|
| **Photos** | The book's interior plates g082-g101 (pp. 82-101), 76 photos. Scans in `out/recon/dryrun/photos/raw/<id>.jpg`, descreened in `.../clean/<id>.png`. Halftone print, flash, cropped prints (principal point unknown). |
| **Sessions** | g082-g096 one hangar session of an ex-NVA **9A33BM3-1**; g097-g100 "session A", probably the same vehicle. The game models the **9A33BM2 (AK)**: decision D1 keeps the AK's blocks; where only the BM3-1 is photographed (the 0096-9M2 cabinet), the photos win. |
| **Never used** | The reloader's cab (9T217, g110-g111) and the late Czech launcher (g101). |
| **Other references** | `mon_2112_80` Fig. 3 (plan and sections, a schematic: the photos overrule it, e.g. the aisle, section 28); the hull data `tools/recon/osa_9a33bm2.json`; the other project's game-extracted mesh (`osa-akm-sam-system.zip`): unknown licence, **reference only, never measured on, exported or committed**. |
| **Body frame** | x right, y forward, z up, metres. Floor (hull) 0.62; aisle walkway about 0.78-0.83; bench top 0.981; rack tops about 2.12; lining 2.10-2.36; rear bulkhead y -1.035. Blender's frame: `P(x, y, z) = (y, -x, z)` in `build_cabin_interior.py`. |
| **Photo coordinates** | The registered cameras (`holistic_diff.registered_camera(id)`) work at 800 px tall (e.g. g098-a 589 × 800, f 1217); the side-by-side renders are at 1200 px wide per half (scale 2.037 for g098-a). **Several of this session's mistakes were mixing these frames** (section 6.4). |

## 3. What exists: the three layers and their tools

The roadmap is `docs/OSA_MODEL_ROADMAP.md` (I0-I10, H0-H9, stage S). Three layers grew on top of each other.

### 3.1 The measurement pipeline (`docs/OSA_INTERIOR_PIPELINE.md`), 8 Nov

Per face, to the millimetre: `tools/recon/interior.py` (algorithms), `osa_interior.py` + `osa_interior.json`
(orchestrator): `refs`, `calib` (LSD segments, plumb-line k1, Manhattan vanishing points, intrinsics), `faces`
(flattening, `refine_quad`: corners from edge-profile fits, 0.15-0.8 px), `fuse`, `detect` (Hough, Tesseract),
`room` (PnP), `verify`. Planned and never built: `sfm`, `ba`, `drawings`, `overlay`.
**The owner's standing rule (8 Nov): leave this pipeline unchanged** (`git diff --quiet tools/recon/interior.py`).

**Its one end-to-end success:** SP-2M2 from g093-b (and g092-a for heights) via `docs/MODELLING_FROM_PHOTOS.md`
(skill `model-from-photos`): flattened face, 18 blocks, 507 parts, heights by ALIKED+LightGlue plane parallax,
`tools/blender/sp2m_trace.py` + `sp2m_model.py`, overlap tests, overlay on the photo. **The owner was pleased with it.**
It worked because a rack face is a plane seen square-on: the 2-D trace *is* the geometry, up to one scale. The same
method made the 0096-9M2 cabinet's face (`cabinet0096_trace.py`, section 33). Note this: the method that works never
lifts anything from 2-D to 3-D except along one known plane.

### 3.2 The holistic layer (`docs/OSA_INTERIOR_HOLISTIC.md` sections 1-14), 8-9 Nov

The owner asked for every component in every photo to be identified "at face value", with rough schematics.
- `tools/recon/holistic.py`: observations per photo (`tools/traces/osa_interior/observations/`, 76 files: components,
  classes, boxes drawn by eye), the component register `data/cabins/osa_ak_components.json`, schematics
  (`out/recon/holistic/*.svg`).
- Detail inventories (`tools/traces/osa_interior/details/`, 34 files: fasteners, seams, latches...).
- The look-alike check `tools/recon/holistic_view.py` (views in `tools/traces/osa_interior/views/`, 26 files; pose by
  traced face or by clicked anchors, render in Blender via `tools/blender/holistic_scene.py`, Canny edges "supported"
  and "explained" within 0.5 % of the width, component boxes by IoU).
- **The weakness:** the identification lives in prose and eye-drawn boxes. It is never tied to pixels or to the
  measurements. The doc itself says IoU on those boxes "is a screen, not a measurement".

### 3.3 The comparison layer (`docs/OSA_INTERIOR_HOLISTIC.md` sections 16, 25-41), 9-10 Nov

| Tool | Does |
|---|---|
| `tools/recon/holistic_sfm.py` | Joint solve: ALIKED + LightGlue, COLMAP, one camera per print. 47 photos, 10 741 points, 1.11 px. Aligned on SP-2M2's traced face. **Plain sheet metal (floor, lining, posts) yields almost no points**; dense stereo gave 60 000 points, nearly all on the racks. |
| `tools/recon/holistic_refine.py` | Bundle adjustment with what COLMAP could not know (SP-2M2's face, SŁ-2M's plane, the across-aisle narrowing). `sfm/refined/points.json`. |
| `tools/recon/holistic_mono_infer.py`, `holistic_depth.py` | MoGe-2 ViT-L monocular depth and normals for all 76 photos (`venv-depth`), aligned to the solve's points; cross-view confirmation; slices. |
| `tools/recon/holistic_metric.py` | Depth **fused** with the solve's points and the traced parts as pins (smooth correction field; withheld-pin test, median 2.6 cm). |
| `tools/recon/holistic_diff.py` | `scene` (cabin as triangles from Blender, Embree rays), `views` (~9 min; registration, per-pixel depth/normal comparison at 800 px, agreement within max(4-5 cm, 2.5 % of range) and 30 degrees), `findings` (regions: turned, raise, missing, excess, moved, shape), `poses`, `surfaces`, `score`/`baseline`, `measure <photo> --poly` (plane fit in metres), `render <photo>` (side by side in `diff/renders/`), sheets in `diff/sheets/`. |
| `tools/recon/shell_section.py` | Heights above the aisle floor from `tools/traces/osa_interior/shell_section.json` (stage S). |

Run them with `E:/SAM3D-tools/venv-depth/Scripts/python.exe -X utf8 tools/recon/holistic_diff.py <step>`.

### 3.4 The model

- `tools/blender/build_cabin_interior.py` (1 800 lines): every shell part built **procedurally from numbers** in
  `data/cabins/osa_ak_shell.json` (every value with `source` and `confidence`). Primitives: `box`, `obox`, `prism_y`
  (an x-z section extruded along y), `prism`, `loft_x` (two y-z sections joined), `cylinder`, `lathe`, `tube`,
  `rounded_box`, plus hand-written sweeps. Export to `art/cabin_interior/*.fbx`.
  Run: `"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_cabin_interior.py -- --only SM_OsaHull`,
  then `python -E -P -X utf8 tools/cabin_mesh_sync.py osa_ak`.
- Faces traced in mm: `sp2m_*`, `sp1m_trace.py`, `sp3m_trace.py`, `sl1m_trace.py`, `sl3m_trace.py`,
  `cabinet0096_*`, `cab_floor_trace.py`, `cab_seat_trace.py`.
- The cabin's placement: `data/cabins/osa_ak.json` (panels, shell parts, reuse meshes by `centre_m`, lamps, walk).

### 3.5 Tools installed (`E:\SAM3D-tools`, `tools/recon/setup_tools.ps1`; `docs/OSA_INTERIOR_PIPELINE.md` section 4)

PyTorch + CUDA (RTX 3060 Ti); **SAM 2.1** (installed, smoke-tested once, *never used in the interior loop*);
PaddleOCR, Tesseract; LightGlue + ALIKED/DISK, hloc; pycolmap and COLMAP (CUDA); GeoCalib; X-AnyLabeling (`venv-label`);
Brush (splats); MoGe-2, trimesh + Embree (`venv-depth`). Blender 5.2 headless and the Blender MCP (live).
Not adopted, licences: **SAM 3 and VGGT-Commercial exclude military use** (owner's decision needed);
MASt3R/DUSt3R/VGGT originals non-commercial (research starting values only). E: is exFAT: git clones fail there.

## 4. The session, in order, with the owner's verdicts

| When | Work | Owner |
|---|---|---|
| 6 Nov | Deep research on the book (`docs/research/OSA_BOOK_WWP_2026-11.md`), project plan | |
| 8 Nov | Interior first; roadmap; measurement pipeline; tools surveyed and installed; splats surveyed | |
| 8 Nov | SP-2M2 end to end | "Pleased with the test results" |
| 8 Nov | Holistic layer, whole interior first pass | "Placement of major assets accurate, but surrounding details and structural assets way off or not even included" |
| 9 Nov | Look-alike check, detail inventories; shell; joint solve; "trust the book over the weeks-old model" | |
| 9 Nov | | "Seats sit raised, consoles at the wrong height, windows off, shelving inaccurate ... get the shell right first?" |
| 9 Nov | Stage S (datum, frame, deck); raking walls; left racks moved 0.37 m outboard (photos over Fig. 3) | "The cabin seems perfectly rectangular" |
| 9 Nov | Same-camera renders; MoGe-2 depth for every photo | "Not quite happy ... cross-reference every angle" |
| 9 Nov | | "What is missing is a way to draw conclusions from the pipeline ... the 0096 at an incorrect angle, missing buttons, no lifted floor, sloping side, roofline" |
| 9 Nov | `holistic_diff.py` with findings and score; 0096-9M2 traced and turned | |
| 9-10 Nov | Depth fused with traces and perspective | "I like the idea of fusing" |
| 10 Nov | The housing beside the cabinet rebuilt as a loft | "Like two boxes on top of each other instead of sloping" (before) |
| 10 Nov | Rear fittings, rear floor, seats on the bench, strut | |
| 10 Nov | Right post as one channel (section 40) | "In two separate pieces and doesn't match" (before), then "Still off: more vertical; the element matching the curve sits to the side, with a latch" |
| 10 Nov | Section 41: leg 15 degrees, cover, latch, strut removed | asked for this handoff |

Score history (`holistic_diff.py score`; the measure and the set of views changed several times, so **these numbers are
not comparable across rows**): section 33 43.5 % on 11 views (old measure); section 34 28.5 % / 0.066 m on 21 views
(pinned pixels excluded); section 38 31.6 % / 0.055 m on 20; section 41 34.1 % / 0.039 m on 19. Baseline 30.4 % /
0.059 m. A third of the photographed surface agrees within 4-5 cm. Nobody would call that near-identical.

## 5. The owner's complaints, and what each one actually was

| Complaint | What it turned out to be | Stage that failed |
|---|---|---|
| Seats raised, consoles at the wrong height (9 Nov) | Every height hung on other estimates (floor 0.62, roof 2.10). The cab group in the joint solve is not to scale. g082-b's camera was assumed level; it looks down (eye 0.815 over the mat, not 0.475) | cameras, datum |
| Cabin perfectly rectangular | Walls built as boxes; the slope was the wheel boxes' inner plates | representation |
| 0096 at the wrong angle, no raised floor, roofline (9 Nov) | AK plan's 38-degree yaw kept against photos showing 15.7; floor never measured | identification (which vehicle), process |
| Side hull "two boxes" (10 Nov) | One x-z section extruded unchanged across the vehicle, where the photo has a pressing whose height changes | representation |
| Seats floating (10 Nov, found by Claude) | A reuse mesh is placed by its bounding box's centre; webbing grew the box downwards and lifted the pans 0.25 m | builder plumbing |
| Post "in two pieces" (10 Nov) | Built as two prisms at different y ranges with a step | representation |
| Post "still off: more vertical, element to the side, latch" (10 Nov) | Leg raked 45 degrees from stitching two readings; its 0.19 m depth and a separate "strut" came from points that belong to a **cover** nobody had identified | identification, transcription, evaluation |

## 6. Where accuracy is lost: a diagnosis from this session's evidence

These are hypotheses with evidence, ranked by how much of the error they seem to explain. **Test them before acting
on them** (section 8).

### 6.1 Identification is never tied to the geometry (comprehension → evidence)

The holistic layer names components in prose and boxes drawn by eye. The measurements (triangulated points, depth
regions, findings) carry **no part labels**. So every number is attributed to a part by Claude's judgement after the
fact, and wrongly when two parts overlap in the image:
- **The right post.** Four triangulated points at x 0.70, y 0.07-0.32, z 1.49-1.71 were read first as the leg's depth
  (section 29: leg y 0.28-0.47), then as a leg reaching y 0.07 (section 38, reverted), then as a separate strut
  (section 39, score +0.1). All three were the **cover** behind the leg (section 41). It was only seen as a cover when
  a full-resolution crop of g098-a was looked at as a picture and its parts named: channel, cover, latch, bracket.
- **Comprehension came last.** Numbers were measured first and the picture was understood afterwards. An image model
  works the other way round.
- SAM 2 is installed and could give pixel-exact masks per part, but the interior loop never used it.

### 6.2 The representation cannot express the shapes (modelling)

Shapes are typed in as polylines and ranges in JSON and built from extrusions and lofts along the body axes. Pressed
sheet metal (a channel that bends in two planes at once, a cover with a rolled edge, a housing whose section changes)
gets approximated by axis-aligned solids, which reads to the eye as "boxes". Parts are split for the builder's
convenience (the post as upright + leg at different y), which makes steps the photo does not have. There is **no part
as an object**: no part frame, no parameters that mean something physically (flange width, bend radius, rake), no
single place where one part's shape is defined.

### 6.3 Readings accumulate without a re-fit (process)

The post's aisle edge in the data at the end of section 40 was a stitch of readings from different days:
- section 27: g096-a, upright x 0.54 to z 1.55, then a rake of 0.34 per metre to x 0.34 at z 0.96;
- section 31: the upright moved to x 0.62-0.65 from depth maps, the leg's lower points left;
- result: 0.62 at 1.55, 0.51 at 1.36, 0.44 at 1.14, 0.36 at 0.96, a 45-degree leg nobody measured;
- section 41, re-read with rays: 0.61 at 1.42 to 0.51 at 1.06, about 15 degrees.

No step re-fits a whole part to all its evidence. An edit fixes one number and silently breaks the part's consistency.

### 6.4 Claude reads pixels by eye, and the frames get mixed (transcription)

Coordinates were read by Claude off gridded images that the harness shows **downscaled** (e.g. "original 1400 × 2115,
displayed at 1324 × 2000"), in crops with offsets, in three pixel frames (scan, 1200-px render half, 589-px camera).
This session mislabelled one grid's offset, mixed crop and full coordinates and picked "edge" pixels that were other
surfaces. A 20-px slip at 1.6 m is about 1.3 cm sideways. Along the line of sight a pixel decides nothing: the plane
or depth chosen decides everything, and that came from a monocular network.
**The blower** (section 39) was moved 8 cm on a raw-pixel comparison with no shared anchor, made three views worse and
was reverted. **Rule learned:** judge a part's place against an anchor seen in both photo and render.

### 6.5 The cameras are not good enough to judge small parts (registration)

- Halftone, flash, cropped prints: the principal point is unknown.
- Across the aisle the solve cannot separate distance from focal length: Fig. 3 gives an aisle of 1.34 m, the photos
  about 1.7-1.9.
- The cab group is not to scale.
- 13 of 24 compartment views failed the gates at section 33; g096-a, the owner's own example view, is still not used.
- A registration 20 degrees off once passed the numeric gate and only the side-by-side showed it (section 33).
- Renders sit "offset and scaled a little" against their photos. Every part judged in a view inherits that view's
  camera error, and nothing measures it per view before parts are judged in it.

### 6.6 The score cannot see what the eye sees (evaluation)

The loop's gate is the share of pixels whose depth agrees within 4-5 cm (and normal within 30 degrees), counted over
the photo. It is dominated by large surfaces and the depth network's error is the same size as the features.
- A narrow channel at the wrong rake changes few pixels: wrong shapes kept "passing".
- The strut, which was not there, raised the score.
- Section 40's two-piece fix raised the mean while two left-post views fell.

The eye judges **silhouettes, edges, angles of lines, continuity (one piece or two) and how parts join.** The edge
measure of section 11 points the right way, but Canny edges on halftone flash prints are noisy and it fell out of use.

### 6.7 Verification was Claude looking at small renders (judgement)

Claude declared parts fixed from downscaled side-by-sides; the owner saw the error in seconds at full size (section 40
is the clearest case). A self-check that cannot see what the owner sees is not a check.

### 6.8 Why an image model succeeds and this does not

An image model never leaves the image. It segments, outlines and recolours **in 2-D**, where the photo is the ground
truth pixel for pixel. This pipeline must **lift** each part into 3-D and then **build** it with code, and both steps
lose accuracy:
- **The lift** is ill-posed from one view. From several views it needs the same part's correspondences, which plain
  sheet metal does not give feature matchers.
- **The build** is limited by what the builder's primitives can express.

The working SP-2M2 method avoided both (one plane, square-on). The obvious inversion is to **keep the evidence in 2-D,
where it is reliable, and make the 3-D model answer to it**:
1. segment every part in every photo;
2. give each part a small parametric shape;
3. fit the parameters so the part's projected silhouette and edges match the masks in all views at once
   (analysis by synthesis);
4. accept a part only when its outline lands on the photo's to a few pixels in each view.

That is a direction to test, not a conclusion.

## 7. What worked, and should be kept

- Flattening a face seen square-on and tracing it in mm (SP-2M2, 0096-9M2, the other racks): the owner accepted these.
- Rays cut with a plane whose position is known independently (sections 15, 27, 41). Exact where the plane is right.
- Triangulated solve points as metric anchors (the cabinet's size 0.57 m by triangulation against 0.50 by depth).
- Depth maps for *shape* and for finding where to look, not for distances (sections 31-34).
- The side-by-side render through the photo's camera: the only check that caught a wrong camera and most wrong shapes.
- Every number in data with `source` and `confidence`; overlap tests for traced faces; withheld-pin tests for fusion.

## 8. Experiments that would locate the loss (suggested for the new session)

1. **Synthetic round trip.** Render the *current* model through the registered cameras, treat the renders as photos
   (with and without halftone, flash and crop simulated), run the whole pipeline, and compare what it recovers with
   the known geometry. The pipeline's own error, separated from the photos'. Stage by stage: camera, identification,
   lift, build.
2. **Error budget on one part across views.** The right post and its cover in g098-a, g098-b, g096-a and g092-a:
   for each stage, how far the result moves from the photo's outline (in px, per view).
3. **Camera audit.** Project the parts traced in mm (SP-2M2, SP-3M, SŁ, the 0096 cabinet, the door) into every view
   and report the outline error in px. That tells which views can judge a part of a given size at all.
4. **Identification audit.** SAM 2 masks of named parts against Claude's eye-drawn boxes and pixel readings in the
   same photos.
5. **Representation audit.** Can the builder express the post's channel, the cover and the housing beside the cabinet
   to within a few px of their outlines in every view, if their true shape were known? If not, no measurement will fix
   them.

## 9. Constraints that stand

- **Sources:** public and declassified only. Every value in `data/` needs `source` and `confidence` with a registered key
  (`wwp_gecko`, `holistic_diff`, ... in `docs/SOURCES.md`; `tools/check_citations.py` must pass; the shell tests
  require both on raised entries). The book is copyrighted: paraphrase it, never bake its photos into textures, never
  commit the photos (they live in git-ignored `out/` and `reference/`).
- **Never commit SAM Simulator's art, sounds, data or `sam.exe`.** The other project's mesh is reference only.
- "Trust the results from the book imagery over the existing model" (owner, 9 Nov); the photos overrule Fig. 3 and
  the AK plan where they conflict on shape and place (record both). Decision D1 still keeps the AK's blocks.
- **`tools/recon/interior.py` stays unchanged** unless the owner agrees otherwise (owner's rule, 8 Nov).
- Licences: no SAM 3 or VGGT-Commercial without the owner's decision; non-commercial weights for starting values only.
- Commit only when the owner asks; never push unasked. Claude cannot run Unreal: re-imports are the owner's step.
- simcore rules in `CLAUDE.md` apply if anything touches simcore (header-only, no exceptions, warnings as errors).
- Checks before handing back: `python -E -P -X utf8 -m pytest tools -q` (287),
  `python -E -P -X utf8 tools/check_citations.py`, `./simcore/build/Release/simcore_tests.exe ""` (663),
  `git diff --quiet tools/recon/interior.py`.

## 10. Practical traps met this session

- **Reuse meshes are placed by bounding-box centre** (`centre_m`); `cabin_mesh_sync.py` keeps it unless the part stands
  on the floor. Change a reuse mesh's height and the part moves: set `centre_m` by hand.
- A full `build_cabin_interior.py` run drops the face builders' manifest entries: re-run `build_cabinet0096.py`,
  `build_sp2m.py` and the other face builders after it.
- `holistic_diff.py views` takes about 9 minutes: run it in the background. One `test_recon` test is flaky; re-run.
- The shell JSON does not round-trip through `json.dumps`: edit it surgically and keep CRLF (`newline=''`). A value-span
  helper must handle strings containing commas.
- Git Bash drops backslashes in heredocs and `sed -i` turns CRLF to LF: write scripts to files.
- Blender is `C:/Program Files/Blender Foundation/Blender 5.2/blender.exe`; the depth tools need
  `E:/SAM3D-tools/venv-depth/Scripts/python.exe`.
- Images shown to Claude are downscaled; read numbers with code at full resolution, not off a displayed grid.
- Back-projecting a depth map at an edge mixes the near and far surfaces: cut rays with a plane instead.

## 11. The prompt for the new session

Paste this into a new session started in `C:\dev\SAM3D-wt\osa-book` on Claude Fable 5.2.

```text
You are taking over the Osa-AK interior model in SAM3D, in the worktree C:\dev\SAM3D-wt\osa-book (branch
feature/osa-book). Work only there: the generated photos, solve, depth maps and renders under out/recon/ exist only in
this worktree.

Read first, in this order:
1. docs/OSA_MODELLING_HANDOFF.md: the whole previous session, the pipeline, the tools, the failures and the
   constraints. Section 6 is a diagnosis to test, not to accept.
2. CLAUDE.md, then docs/OSA_INTERIOR_HOLISTIC.md sections 11, 16, 25-41, docs/OSA_INTERIOR_PIPELINE.md sections 3-4
   and 9, and docs/MODELLING_FROM_PHOTOS.md.
3. Look at the photos themselves at full resolution (out/recon/dryrun/photos/raw/g092-a.jpg, g093-b, g094-a, g096-a,
   g098-a, g098-b, g100-a) beside the current renders (out/recon/holistic/diff/renders/<id>.jpg).

THE PROBLEM. An image model given one of these photos can identify every component and outline, edit or recolour each
one accurately. Yet with depth maps, perspective solves, traces, triangulated points and a scored comparison, the 3-D
model still differs from the photos in ways the owner sees at a glance: wrong angles, wrong rakes, boxes where the photo
has a sloping pressing, one part built as two, parts that are not there, parts misidentified. Scores rose while the
shapes stayed wrong. The previous sessions fixed one complaint at a time.

YOUR JOB has four parts. Stop at each checkpoint and report to the owner before going on.

A. FIND WHERE ACCURACY IS LOST, BY EXPERIMENT. Do not edit the model yet. Take the candidate stages (identification
   and comprehension; camera registration; lifting 2-D evidence to 3-D; transcription of numbers; the modelling
   representation and the Blender builder; evaluation; the process itself) and measure each one's contribution. At
   least:
   - a synthetic round trip: render the current model through the registered cameras, treat the renders as photos
     (also with halftone, flash and crop simulated), run the pipeline, and see how much of the known geometry it gets
     back, stage by stage;
   - a camera audit: the parts traced in millimetres (SP-2M2, SP-3M, the SL racks, the 0096-9M2 cabinet, the door)
     projected into every view, with their outline error in pixels;
   - an identification audit: SAM 2 masks of named parts (it is installed in E:\SAM3D-tools\venv) against the
     observations' boxes and the readings the last session made by eye;
   - a representation audit: could the builder express the right post's channel, its cover and latch, and the housing
     beside the 0096 cabinet to within a few pixels of their outlines, if their true shape were known?
   - an evaluation audit: does the current score rank a known-wrong part below a known-right one?
   CHECKPOINT 1: an error budget per stage, with numbers and pictures (overlays on the photos at full resolution).

B. DESIGN THE PROCESS FROM THE GROUND UP. Keep what the evidence says works (handoff section 7). Replace what it says
   fails. It must close the gap between 2-D, where identification is reliable, and 3-D. Consider, and adopt or reject
   with reasons:
   - per-part masks and edge polylines in every photo as the primary evidence, linked to the component register's ids;
   - parts as objects with physical parameters (a pressed channel, a plate with a rolled edge, a housing), each defined
     in one place;
   - fitting each part's parameters to its silhouettes and edges in all views at once, with depth and triangulated
     points as softer terms and cameras refined with it;
   - acceptance per part, per view, in pixels: outline and edge distance, plus an overlay a person can judge;
   - which cameras may judge which parts.
   Claude's vision names parts, says what they are and how they join, and judges overlays. Numbers come from code at
   full resolution, never from coordinates read off displayed images. Write it as docs/OSA_MODELLING_PROCESS.md.
   CHECKPOINT 2: the design, with what it changes and what it costs, for the owner's approval.

C. PROVE IT ON THE HARDEST PARTS. Rebuild with the new process, from the photos and not from the current numbers:
   - the right post with its cover, latch and bracket (g098-a, g098-b, g096-a, g092-a);
   - the sloped housing beside the 0096-9M2 and the raised floor round it (g096-a, g095-c);
   - the bench and the operators' seats (g093-b, g094-d, g095-c, g100-b).
   Show before and after overlays at full resolution in at least three photos each, with the per-part pixel errors.
   CHECKPOINT 3: the owner judges these by eye. Their eye is the acceptance test.

D. MAKE IT THE ONLY WAY THE OSA IS MODELLED OR EDITED. Once the owner accepts C:
   - write a project skill .claude/skills/osa-model/SKILL.md that loads docs/OSA_MODELLING_PROCESS.md;
   - add a rule to CLAUDE.md: any modelling or editing of the Osa, interior or exterior, follows that process;
   - point docs/MODELLING_FROM_PHOTOS.md, docs/OSA_INTERIOR_PIPELINE.md, docs/OSA_INTERIOR_HOLISTIC.md and
     docs/OSA_MODEL_ROADMAP.md at it, and say which older steps it replaces;
   - add tests that fail when a part's outline error in its accepted views grows past its limit;
   - save a memory note pointing to the skill.
   Then plan the rest of the interior through it.

CONSTRAINTS (handoff section 9 has the detail):
- Public and declassified sources only. Every value in data/ carries source and confidence with a key registered in
  docs/SOURCES.md; tools/check_citations.py must pass.
- The book (wwp_gecko) is copyrighted: paraphrase it, never bake its photos into textures, never commit them.
- Never commit SAM Simulator assets. The other project's Osa mesh is reference only.
- Never use the reloader's photos (g110-g111) or the Czech launcher (g101).
- tools/recon/interior.py stays unchanged unless the owner agrees; build beside it.
- No SAM 3 or VGGT-Commercial (their licences exclude military use) without the owner's decision.
- Commit only when the owner asks; never push. Unreal re-imports are the owner's step.
- Before handing back: pytest tools, check_citations, simcore_tests, and git diff --quiet tools/recon/interior.py.

HOW TO WORK. Judge every change by its overlay on the photos at full resolution against an anchor seen in both, not
by an area score alone. Say plainly what is measured, what is scaled and what is your choice. If an experiment shows
the problem is somewhere the handoff did not expect, follow the evidence and say so. Use subagents for bulk work
(masks for many photos, many views); keep diagnosis and design yourself.
```

## 12. The panels pass (10-11 Nov 2026)

Every control, lamp and fitting of the cab and the three operators' columns checked against the book's photos, each
placed from numbers measured on a metric flat (`tools/recon/osa/panel_flats.py`), never read off a picture.

| Panel | How | State |
|---|---|---|
| Driver's and commander's dash | flat `dash` from the book's drawing, the cab photos registered on it (`match_flat`), picks in `tools/traces/osa_interior/panels/dash.json`, written by `panel_apply.py` | rebuilt: 18 new inert controls (lights, water jets, the switch strip, the interlock, the bilge pump), the ПРОХОД ПОД МОСТОМ board; the plane's depth, height and tilt are still estimates (no grade-A view of the cab) |
| PU-303 | flat `pu303` from the book's drawing (one side mirrored) | rebuilt: 10 new inert controls, ПУСК and ГОМ for the turbine and the generator |
| fo_panel (deputy commander) | view flat `fo_panel_g094a`, SAM 2 parts, `flat_snap.py` row by row, then picks by the SAM Simulator photo's order | re-placed; the two channel banks were in the data twice and mirrored: the lamps now sit on the buttons, the 17 duplicate legends are gone |
| so_panel (senior operator) | view flat `so_panel_g094a`, the same | re-placed; the PPI is centred on its ring (276 mm screen), the ГОТОВ/ВЫСОК. window is right of the button stack as in the book's photo |
| op_panel (operator) | view flat `op_panel_g094a`, `flat_snap.py --affine` | the data's items are right (1-5 mm once each block is fitted); the flat is stretched about 0.9 at the edge of g094-a. **Widened (11 Nov 2026)** from 0.61 to 0.68 m, its aft edge out 70 mm so the blocks' latches are on it (four chrome latch fixtures at u = -315 mm): centre y 0.875, every item u + 35 mm, the operator's eye and seat at y 0.875 (the bench recess and SM_OperatorSeat too); SŁ-2M's rack runs y 0.535-2.505 (1.99 m, against the 1.89 m of Fig. 3 that other scales were taken from: not rescaled) and SŁ-3M's post moved aft to 0.415-0.535 |
| pwr_cabinet (0096-9M2 door) | the trace `tools/blender/cabinet0096_trace.py` (every part of the mesh) drawn on view flats `pwr_cabinet_g096a`, `_g097b`: lands on the photo; the door's legends read on g096-a and matched to voenizdat_osa_1991 p50, p64-65, p100, p150, p163-173 | 5 new inert controls (РЕЛЕ ТЕПЛ., ЗИМА — ЛЕТО, ПОДКАЧКА ОТСЕКОВ ШТАТНАЯ and АВАРИЙНАЯ, ФАЗЫ АС — ВС), 5 lamps (ПРИВОДЫ СВР I and II, РАССТОПОРЕНО εн, ЭКВИВАЛЕНТ СПК I and II), 8 unlit lamp fixtures, and the voltmeter as a working meter (`pwr_voltmeter`, simcore `SupplyVoltage`: 220 V once a supply is on line, the drill's check before АППАРАТУРА ВКЛ.); every item on a traced part (`test_cabinet0096_trace.py`) |
| sl3m_panel (SŁ-3M) | the trace `sl3m_trace.py` drawn on view flat `sl3m_panel_g097b` | the mesh is right: blocks, jacks, fuses and rivets on the photo within a few mm (the 32 mm black jacks offset by parallax only); the manual gives it no crew controls, only fuses and their lamps (0096-10M, p165-166), so nothing interactive is added |
| sp2m_panel (SP-2M2) | the trace `sp2m_trace.py` (507 parts, already checked on g092-a) | 2 new inert controls on block 0091-18, ТРЕНИРОВКА — РАБОТА СОЦ and ССЦ (p49, p97; which toggle is which: SAM3D's choice), stood for by the senior operator; 0091-17М's КОНТРОЛЬ НАПР. and the НАКАЛ lamps (p49, p98, p150) are not on the photographed BM3-1's blocks and were not placed |
| gyro_panel (SŁ-1M) | the trace `sl1m_trace.py` and its flattened photo `out/sl1m/face.png` (g091-a), the legends read there; voenizdat_osa_1991 p50-52 (0005-8M, 0009-5M, OR-31M), p162 (0004-12M1), p72 (OS51-8M) | 16 new inert controls on traced parts: 0004-12M1's functional-check set (АНОД ВКЛ., КЗ, СМЕНА ЗНАКА, УРОВНИ КОМАНД, КОНТРОЛЬ СИСТЕМ, ПУСК — ИСХ. ПОЛОЖ., ПРОВЕРКА ПЕРЕХОДНЫХ ХАРАКТЕРИСТИК, ПРОВЕРКА СМЕНЫ ЛИТЕРА), 0005-8M's black panel ПУ-11 (φ АВТ. — РУЧН., ПИТАНИЕ СЕТЬ — ПТ, РЕЛЕ — Ψ, Θ, ОБОГРЕВ: the manual lists them, which toggle is which is SAM3D's choice, low), 0009-5M's guarded ПРИВОД Ψ, ПРИВОД γ and +27 В, OS51-8M's selector КОНТРОЛЬ РАБОТЫ; lamp fixtures КЗ and ОТКАЗ Qc. **АРРЕТИР. and ОТКАЗ Ψ, Θ moved off the driver's dash** (a placeholder) to 0005-8M where p247 and p51 put them: the button on a 30 mm panel of the driver's own (`gyro_arrest_panel`), 0.74 m left of his eye; the driver's lesson says so. The meters ИП1, ИП2 and КОНТРОЛЬ РАБОТЫ are the mesh's, not displays; OK81-4M-II's red button is not in the manual and its legend is not legible: not added |
| sp2m_panel, OR-31M | `sp2m_trace.py` M0 | its only toggle is СС — ОТКЛ. (p51), added inert, stood for by the senior operator |
| sp1m_panel (SP-1M) | `sp1m_trace.py` (drawing g090-b) | nothing added: the manual names only 0065-2M's АРУ — РРУ and gain pots under covers (p50), which the drawing shows as covers and flaps; 0093-14A's knob and pots, 0092-9M, 0093-19 and OK82-2M have no crew controls in the manual |
| sp3m_panel (SP-3M-2) | `sp3m_trace.py` (g098-b, c) | nothing added: the manual describes the AKM's 1L24-2, not the AK's RZM interrogator blocks; their knobs and selector stay the mesh's |
| Drawn-only knobs (11 Nov 2026) | every control-like fixture (knob, toggle, button, lever) on the panels | 64 turned into inert controls at the same places with the same looks, named from voenizdat_osa_1991 p49-50 (the PPI and OP63-6M), p98-100 (0062-6M1, OS81-9M, OS61-5M1, OS81-18M, OS81-10M, OS52-12M1, ВПУ-55), p119 (OK81-4M), p168, p189 and SAM Simulator's labels; the unnamed ones say so (low). Left as fixtures: the faces of controls already there (ВЫКЛ. buttons, beam buttons, ПЕРЕДАТЧИК ОТКЛ., БОРТ СПК ОТКЛ.) and the jacks, sockets, lamps and plates. АВТОМАТ and РУЧНОЕ (`ro.auto`, `ro.manual`) are candidates to wire |
| Walls (11 Nov 2026) | triangulated on g096-a and g097-b (grade A); g100-b, c; voenizdat_osa_1991 p167, p169-172 | new bare panels: `air_panel` (the gauge on the blue air line aft-left of the range operator, a fixture, and the brass cock under it, `air.cock`, low), `bulkhead_panel` (the engine door's two dog levers, ОТКРЫТО / ЗАКРЫТО / ГЕРМЕТИЗАЦИЯ, and the right ЗИМА—ЛЕТО cover's lever under the blue flap), `bulkhead_left_panel` (the left cover's lever under the sloped cover), `vent_hatch_right`/`_left` (ЛЮК ВЕНТИЛЯЦИИ over SP-1M and SŁ-1M: no photo, placed by p167's words); the A-1's volume knob (`intercom.a1_volume`) |
| Cab units (11 Nov 2026) | wwp_gecko g086-a, g086-c, g087-c, g087-d, g089-a, g089-b, g090-a, g090-d, g091-b, g091-d; voenizdat_osa_1991 p16, p101, p103, p168, p173-174, p189, p221. The cab photos' grade-B cameras sit in impossible places (g090-d's inside the radio) and g091-a's grade-A camera looks at SŁ-2M although the photo shows SŁ-1M: every place here is an estimate against the parts already built | new meshes `SM_FireSystem` (red control box, shelf, gauge, three bottles, leads), `SM_PowerBP26` (twice: on the right R-123M's frame, and beside the left one on their common tray), `SM_CabUnits` (the pre-heater's box, the DP-3B's measuring box, the post valve, the radio well's switch box, the header valves). New controls: `fire.*` (two guarded covers, ПРОВЕРКА ЦЕПЕЙ, the bottles' disc, the fire button, the bottle valve), `heat.plug`/`heat.start`, `dp3b.check` with `veh.dp3b` moved onto the DP-3B's box, `veh.heat_vent` (ВЕНТИЛЯЦИЯ — ОТОПЛЕНИЕ), `veh.well_switch`, `veh.header_valve_*`. The vent hatches' levers moved to the racks' aft halves as g090-d and g091-a show them. PB-107 was already built (the shell's blower cover over the fan's slit, p168; Fig. 3 writes PB-55 there): named, no controls |
| Driver's corner (11 Nov 2026, the owner's placing) | the owner: the fire system sits left of the driver against the hull's left side under his side window; wwp_gecko g083-a, g084-a, g084-b, g084-c, g086-a, g086-b, g086-c, g088-a; rwd_mb3 9a33_31. Places are estimates, as above | `SM_FireSystem` rebuilt on the left corner wall (facing 102.09, built from `corner_point`): the box under the window above the front wheel housing, its perforated bracket with the gauge to 250 on top, a pale canister aft, the 0-1 gauge forward (left of PU-303, not claimed for the fire system), two bottles lying on the housing and one standing at its foot with the red hand wheel. New `SM_DriverUnits`: the column stalk, the grey pillar by SŁ-1M's front end (junction box, ring bracket; its plates drawn as the cabin's nameplates: the heater warning, ВЕНТИЛЯЦИЯ ⇄ ОТОПЛЕНИЕ, the clutch instruction), the web into the corner with two lightening holes, two stowage covers and a small gauge (which surface carries them is SAM3D's reading of g086-b). `SM_Pedals` has three pedals (clutch, brake, accelerator, g084-c). New controls: `veh.gear` (H-gate three over three and neutral; slot labels SAM3D's), `veh.handbrake`, `veh.column_stalk`, `veh.stowage_upper`/`_lower`. Behaviour by data (all SAM3D's reading, low): subsystems `fire_circuit_check` (ПРОВЕРКА ЦЕПЕЙ lights the box's two red lamps until its cover shuts), `fire_discharged` (the fire button: the lamps flash until the cover shuts), `preheater` (СВЕЧА and the button; the glow window and the red lamp), `dp3b_test`, and the DP-3B's green and red lamps. The dash's plates (ВОДИТЕЛЬ, the gear plate, the water-jets' plate) were already nameplates on driver_panel; the gear lever and handbrake meshes were already in `SM_CabLevers` |
| SŁ-2M's housings (11 Nov 2026) | samsim_panels (columns_three: the Osa-AK's three columns) checked against wwp_gecko g094-a; sizes are proportions of the cabin file's displays, about +-5 mm, depths by eye, about +-1 cm | `SM_RackSL2M` now carries raised housings built from `tools/blender/sl2m_housings.py`, each centred on its display so displays, controls and lamps stay put: OS81-9M's white hood, OS81-18M's range-dial plate with its visor (notched round the range wheel), OP81-16M's flange (16 screws, 4 locking knobs), the βс and εс visors, VPU-44's dark cast bezel with a rounded mask (it hides 7 mm of the TV picture each side), OS81-10M's knurled ring, and the ДЦ meter's housing. The flat bezel, oval and window fixtures they replace are gone; КАНАЛ I and II are widened to 17 cm; the VPU-44 block is grey. `tools/tests/test_sl2m_housings.py` keeps each housing 3 mm clear of every item on its panel. Left: no frame round the 0094-5 strip (it reaches the panel's top edge); the βс dial straddles the OP63-6M2/OS52-12M seam in the cabin file, so its visor does too; the 0004-14M meters stay flat |
| SŁ-2M retraced (11 Nov 2026) | wwp_gecko g094-a through its certified grade-A camera (eye 0.824, 2.635, 1.924) and its view flat; the holistic depth map (`holistic_diff.py measure g094-a`) for depths; samsim_panels for the blanks and the controls' kinds. Places about +-3 mm on op and so, +-5 mm on fo; depths about +-5 mm | **Parallax**: the flats show a part h proud pushed away from the eye's foot (on op about 0.9 h, so 0.56 h, fo 0.23 h); `tools/recon/osa/unparallax_panel.py` moves controls, lamps and fixtures from seen to true places by a height per kind (run once on so and fo; do not run again). **op_panel rebuilt from the photo** (`tools/traces/osa_interior/panels/sl2m_op.json`, applied by `panel_apply.py`): SAM Simulator's layout is about 8 % larger and 60 mm high against the photo, so its looks were scaled by 0.923 and every block, display and control placed on the photo's own blocks (0062-6M, OS81-9M, OK62-5M, OS61-5M, OS81-18M, 0051-7M); the photographed vehicle lacks ПОЛУАВТОМАТ, СКОРОСТЬ П А, the control selector and the operator's headset socket, which stay at SAM Simulator's places carried onto the photo's blocks. The depth map gave OP81-16M as a 380 mm drum 46 mm deep with a dark scale cone (17.5 by 31 mm), the screen 15 mm out (the map says 25). **Hand wheels**: the range wheel is a white dome (new look kind `dome`, part `SM_WheelDome`), the azimuth and elevation wheels' centres were biased by their cranks and moved; in Unreal the crank stands at 0.4 D from the axis on the dome's shoulder. `test_sl2m_housings.py` sweeps each wheel and its crank through a full turn: range clears the range dial by 4.5 mm, azimuth and elevation by about 21 mm; each is worked against a display (`watch`). Check: `out/recon/osa/renders/g094-a_sl2m_certified.jpg` (photo, model, model's edges on the photo). The depth photo's registered camera sits 31 cm from the certified eye and draws everything about 64 px of 590 low: judge SŁ-2M through the certified camera only |

**The whole interior as one model (11 Nov 2026).** `python tools/build_osa_interior.py` rebuilds every interior mesh
in order (`build_cabin_interior.py` first, which rewrites the manifest with its own 29 meshes, then the photographed
parts' builders that put theirs back: cabinet 0096, R-123M, R-124, the SŁ-1M, SŁ-3M, SP-1M, SP-2M2 and SP-3M-2 faces),
checks that every mesh the cabin file places was exported, and runs `tools/blender/assemble_cabin.py`: the 39 meshes
where the cabin file puts them, the 35 panels with their baked faces, blocks and fixtures, and every control, lamp and
display (335) standing out of its panel, saved as `out/osa_interior/osa_interior.blend` and `osa_interior.fbx` with
overview renders (`view_*.png`, the top view with the roof off) and `contents.json`. It is a review copy: the game
assembles the same pieces at run time and never imports it. `tools/tests/test_osa_interior_complete.py` fails if a
placed mesh is not built or a built one is not placed. `--assemble-only` skips the rebuild.

**Scoring every panel against the photos (11 Nov 2026).** `tools/recon/osa/panel_score.py` renders the assembled
interior (controls included, `tools/blender/assembled_render.py`) through the nine trusted grade-A cameras (residual
under 5 px; not g091-a) and, per part and view, measures the median distance from the model's edges to the photo's,
in px and in mm at the part's range, and the share within 3 px. Parts the eye stands inside are hidden (g094-a's eye is
14 cm inside SP-1M's box). `panel_fit.py` turns per-view 2D shifts into a 3D move per part (reported, never applied; it
is noisy on cluttered views, so a move is kept only when re-scoring shows it helps in every view), and `panel_look.py`
crops one part beside the photo. Best view per panel: air_panel 1.4 mm, sp2m_panel 1.4, sp2m_face 1.4, pwr_cabinet 1.7,
cabinet_0096 1.9, op_panel 2.1, fo_panel 2.2, sl1m_face 2.3, so_panel 3.1, sl3m_face 3.5, sp3m_face 4.4, R-124 A-3 6.4.
The big numbers in oblique views (g093-a, g097-a) are the operators' seats folded up in front of the camera, which the
cabin does not model. **No trusted camera sees** the cab's panels (dash, PU-303, commander, radios, fire system, cab
units, gyro arrest), the R-124 A-1 and A-2, the vent hatches or the bulkhead's own panels: they stand as placed from
grade-B views and the book's drawings.

Changed from the scores the same day:
- **The turret's axis** (`osa_ak_shell.json` `turret_opening`): the boot's silhouette in g096-a and g100-b through the
  certified cameras puts it at (0.002, 0.056), where the data had (-0.16, 0.02) from the old frame (the frame fix had
  found it 1.096 m from SŁ-2M's face and left the roof unmoved). The opening, the boot and the base moved (+0.162, +0.036).
- **The turret's column below the boot** (new, `turret_opening.boot.column`): g097-b shows it grey from the boot's foot
  (z 1.81, not 1.72) to a base ring on the floor, radius 0.234 m widening to 0.27; it hides the bulkhead's dog levers
  there as the photo does, which was the 75 mm "error" of `bulkhead_panel`. 12 mm clear of cabinet 0096.
- `air_panel` (+19, +25, -2 mm), `sp2m_panel` (+6, -2, +4 mm), `lamp_blue_post` (+4, +29, +16 mm): each re-scored better
  in the views that see it (air 0.81 to 0.92 and 0.35 to 0.62 within 3 px; the lamp better in two of three).
- Tried and put back: SP-3M's flap (better in three views, worse in two).

**The operators' seats folded upright, the tin, the cab (11 Nov 2026, second pass).**
- **The seats.** g093-a shows two of them folded upright: an olive vinyl pad (rolled edge, domed studs round its side)
  standing over the aisle with its top facing the console, a grey pressed shell under it, a grey tube frame down to
  pivot brackets at the foot of the bench's aisle face, webbing straps. Folded down (g094-d, g095-c, g096-a, g097-b) the
  same seat lies on the bench shell up, the gas-mask bag on it: section 38's "pans". `SM_OperatorSeat` is now built
  folded upright: the pad's pose fitted to its outline in g093-a through the certified camera (a 0.50 x 0.44 x 0.07 m
  pad, 0.93 overlap), centre x -0.20, z 1.475, leaning back 13 degrees, the depth map within 0.09 m. The views whose
  photos have them folded down hide them (`state.hide`, now also g093-b, g094-d, g095-c, g097-b). **Open:** g093-a puts
  the two it sees at y 1.88 and 1.27, about 0.3 m aft of the deputy's and senior's stations; the seats stay at the
  stations because the operators sit at their columns. Seated on the bench (SEAT_Z) the crewman's seat is folded; a seat
  that comes down when he sits is not built.
- **Chest 5937** stays where it was (the owner's call, 11 Nov 2026). A move of (-0.077, +0.346, +0.05) m that the joint
  solve's points on its lid (g092-a, g096-a, g098-a-c) suggested was tried and put back.
- **The first-aid box is left as it was.** No single move fits: g098-a's red cross wants it about 0.1 m inboard, which
  g092-a and g096-a (the hangar session) refuse, and the forward move the edge scores liked takes the cross further
  from g098-a's. It is loose kit in a strap holder, carried in different places in the two sessions.
- **The cab** (no trusted camera sees it; the grade-B cab cameras put their eyes 1.5 m aft or above the roof, and only
  g082-b's measured camera of section 30, about +-5 cm, can judge a place). Changed: the steering wheel's spokes are
  thick and black at 3, 11 and 7 o'clock with a large cup hub (g084-c, g084-a; they were thin and steel at 12, 4 and 8);
  the gear lever is black with a large ball knob over a gate plate (g083-a, g084-c). **Open, from the review:**
  - **The radios, done 11 Nov 2026** (docs/OSA_INTERIOR_HOLISTIC.md section 42, through g082-b's measured camera): the
    left R-123M stands on its shock tray on the deck directly behind the driver's seat, its front inboard at x -0.34,
    under a tread cover with the Р-123М-1 end plate (new `SM_RadioTrayLeft`); the right one 0.38 m further aft and
    0.16 m higher, on the frame on ZIP box nr 2, which is rebuilt low and long along the hull (X of beads, legs; it
    was a tall box across it). Left open: both sides measure 5-10 cm further outboard than the racks' faces allow
    (held inboard); Fig. 3 puts the right set 0.37 m further forward than the photo (the photo is used).
  - The box beside the commander's seat reads А-2 in g087-c; the cabin keeps the commander's A-1 there (his chest
    switch, intercom and headset are A-1's) and the deputy's A-2 on the left.
  - **The dash, done 11 Nov 2026** (section 43): its gauges carry the figures and units read off g085-a (new `marks`
    and `unit` on gauge fixtures, simcore `CabinLook`), the hour meter its digit window (scale `hours`); the three bow
    handles and the plates' corner screws, measured on the dash flat, are in `SM_CabUnits`; the wheel's emblem is
    dark bakelite; five note-labels that were printed on the face are notes now.
  - Missing or flat: the DP-3B as a cast box on a bracket with its detector (g085-b); the heater duct under the
    commander's dash (g085-d, g085-b); the plates' raised frames; the khaki and black boxes on the roof over the
    driver's screen (g089-a); framed visors; the five unidentified dash controls' names. The commander's right
    corner: g087-c shows the front wheel housing's inner face flat and steel grey with a round keyed switch on it
    where the model has a rounded pale bulge (`ARCH_SECTION`); no camera that may judge sees it.
  - The gear lever's height (1.26 m from g083-a by eye) looks high against g082-b; the pedals hang on long rods where
    g084-c shows small pads low over the mat; neither is measured.
  - Reach: the commander now stands to work the right set's fixators and volume at its aft end (`stand_for`, the reach
    check); the driver's R-123M is 1.05-1.13 m away and behind him; his chest switch, `intercom.pv` and headset sit on
    `driver_panel` at x +0.2, 1.07 m from him; the dash plane is 0.88 m ahead of both seated eyes, where g084-c has the
    wheel's rim nearly at the cushion (eyes or dash perhaps 0.1 m apart in y).

**The dash round the wheel and the 0096's cover (11 Nov 2026, third pass;** `docs/OSA_INTERIOR_HOLISTIC.md` sections 44-45).
- **The dash** measured through g082-b's camera: the flat was 20 % small. Body x = -0.0406 + 1.1988 u, with misses under
  1 cm. Both dash panels were rescaled and the wheel measured (centre (-0.385, 4.30, 1.478), 0.606 m across); its hub
  now sits between the instrument and switch plates. The DP-3B's face is 0.20 x 0.13 m at (0.737, 4.517, 1.402), and
  the TA-57 moved inboard off it. The fit lives in `dash.json` `measured_fit`; `panel_apply.py` refuses that flat
  until it applies the fit itself.
- **The 0096's repair toggles and cover** (g097-a open, g096-a and g097-b shut, grade A):
  - 23 toggles in two rows on a cream plate on the slope (`repair_toggles_panel`, fixtures, legible legends printed);
  - the cover as its own hollow mesh `SM_Cabinet0096Lid`, its inside as g097-a shows it, hinged on its rear top edge
    and opening 76 degrees like an awning;
  - the new hinge `repair_lid` (`"hinges"` now take an `axis`, in simcore and Unreal), driven by `pwr.repair_lid` (the
    right captive screw, a rotary head) through the subsystem `repair_lid_shut`.
- **The owner's steps:**
  - build Unreal (`SimCabinComponent.h/.cpp`: hinge axes);
  - run `python tools/ue_import_cabin_interior.py` (new `SM_Cabinet0096Lid`, rebuilt `SM_Cabinet0096`, `SM_CabUnits`,
    `SM_DriverUnits`, `SM_SteeringWheel`, the panels).
- **Open:**
  - the cover's opening angle is low confidence (72-84 degrees fit);
  - the slope reads 120-127 mm deep in the casts against the trace's 108;
  - the stay rod, the plate's notched corners and the group legends are not drawn;
  - the commander's half of the dash has no grade-A check of its plane's depth and tilt.
- **The owner's review (12 Nov 2026, section 46):**
  - the repair toggles now drawn thrown up the face, as g097-a shows;
  - the 0096's five button-pair switches given the new look `in_mesh` (the mesh has the buttons, the game keeps the hit area);
  - КОНТР. ЛАМП a rotary;
  - the РАБОТАЕТ lamps alike;
  - the commander panel's unsourced `power_on` and `fault` lamps removed;
  - the preview draws switch-buttons, unlit lamps and toggle fixtures as Unreal does.
  - The owner rebuilds Unreal again (`SimCabinComponent.cpp`).
- **The audit of missing controls (12 Nov 2026, section 47):**
  - five drawn lamps now work: ПА, ТСЦ on the elevation block, ПОЛУАВТОМАТ, and PU-303's ПОДСВЕТКА domes;
  - everything else found is in the button map's not-built list, waiting on the owner.
- **The drawn meters and lamps wired (12 Nov 2026, section 48):**
  - gauges read the console by `levels`; 0062's ТОК and МОЩНОСТЬ (with the new `mon.tx_select`), 0004-14M's four
    centre-zero meters, PU-303's gauges and lamps, pressurisation, the compressor, ГАБАРИТ 3800 and the НАКАЛ lamps work;
  - 0091-18's training toggles un-swapped;
  - section 48 lists where the photos show the manual-only parts (the ПАНЕЛЬ ИНДИКАЦИИ with АВАР. ОТКЛ. СЭП, OK81-4M's
    ВОЗВРАТ and БАЛАНС, 0005-8M's МК — ГПК — АК, the PPI's course scale) and two placements to check on fo_panel;
  - the owner rebuilds Unreal (`SimCabinDisplayComponent.cpp`).

Open:
- SŁ-3M's rack below its blocks: the model has a perforated plate where g097-b shows a plain grey plate behind a folded
  seat; one view, not settled.
- SŁ-2M's top blocks (0062-6M's gauges, so's counter strip) reach about 3 cm above the 2.10 m top rail (Fig. 3) in
  g094-a; they are cut at the rail on all three columns. Raising the rack's top is a hull change for the owner.
- op_panel's width is done (above). SŁ-2M is now 1.99 m long against Fig. 3's 1.89 m, the scale several racks were
  taken from: whether to rescale them is open.
- **so_panel's counter strip** (the eight windows) sits about 38 mm above the panel's top edge in the photo; the
  display stays inside the panel until the panel's height is checked.
- No grade-A view sees the gyro rack, the radio sets or the R-124 boxes: they were not re-placed (grade B may not judge);
  SŁ-1M's new items stand on its trace's parts, as its old ones did.
- Unidentified fittings from the book's photo: three small lamps over each channel bank (fixtures); the switch right of
  the meters is taken for ВЕНТИЛЯЦИЯ РАБОЧИХ МЕСТ (p168, low); the dark knob above КОНТРАСТНОСТЬ stays unnamed (inert).
- Not modelled: the black box with round connectors on the floor in the back-right corner (unidentified), the cable
  connectors and test jacks round the racks (mesh only: not worked in operation); the BP-26s' fuses (spares, not
  worked in operation); the black lever under the roof forward of each vent hatch's plate (g090-d, g091-a; a hatch lock?).
- g091-a's grade-A camera looks at y 1.26-2.54 (SŁ-2M) though the photo is SŁ-1M: its solve needs a look before it judges anything.
- Who works the bulkhead's levers and the vent hatches is not in the sources: the nearest seat stands for them.
- Not found on the photo: the АПЧ lamps over the banks, the elevation and azimuth wheel locks, ОСВЕЩ. ШКАЛЫ.
