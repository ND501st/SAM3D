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
