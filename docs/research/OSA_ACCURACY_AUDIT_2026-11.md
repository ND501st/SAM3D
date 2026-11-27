# Where the Osa interior loses accuracy: the audit (checkpoint 1)

*10 Nov 2026. Part A of the rebuild set out in `docs/OSA_MODELLING_HANDOFF.md` section 11: find by experiment where
accuracy is lost, before the model or the process is touched. Nothing in the model, the data or the measuring pipeline
was changed. The scripts are in `tools/recon/audit/`; everything they write is under `out/recon/audit/` (git-ignored,
this worktree only). The overlays hold the book's photographs and are never committed.*

## 1. The answer in one table

Pixels are of the descreened photo (g098-a is 1563 px wide, g096-a 3267). "Of the width" lets the two be compared.

| Stage | What was measured | Result | Share of the loss |
|---|---|---|---|
| **Cameras** | SP-2M2's millimetre flat matched into every view; each match's pixel against its projection through the camera every judgement used | **39-97 px on SP-2M2 (3-5 % of the width, 3.4-6 cm at the rack)** in the four views that see it; **a fifth view's camera faces the opposite wall**; 14 of 24 views have nothing to check them against | Large, and it sits under every number measured by rays since section 32 |
| **Tying a name to geometry** | The cover's outline as the data has it, drawn in the photo it was read from, through the camera it was read with | **100-250 px from the housing's real outline (6-16 % of the width, 4-11 cm)**; overlap 0.11 | The largest loss on the hardest part. It is a reading of the wrong edges, not a 3-D error |
| **The lift by depth** | The model rendered as photographs with exact cameras, through MoGe, the fusion and the comparison | Free surfaces 2.0 cm median, **5.5 cm within 6 px of a part's edge, a tenth of pixels beyond 19-24 cm** | Large for every part that has no trace: 22-69 px, sizes 1.5-2 times too big |
| **Evaluation** | The same synthetic run, and known-wrong models scored on the real maps | **An exactly right model scores 64 %** (15-97 % by view) with 139 false findings. Taking the right post's leg, cover and latch away changes the real score by **-0.02 %**. Taking the chest away **raises** it 0.6 % | It could not have steered the work: the steps it approved (+0.1 to +1.1 %) are smaller than deleting a part |
| **Identification in 2-D** | The first pass's boxes against SAM 2's masks, nine parts | Boxes overlap the masks 0.19-0.95 (median 0.77), worst edge 0.6-4.8 % of the frame. **The first pass had the housing as one object, in the right place** | Small. Good enough to name things, not to measure |
| **Transcription** | What the data's numbers can hold | Metres typed to 0.01: ±11 px in g098-a. The cover: 13 typed points for an outline that needs about 40-60 for 3-5 px, so 26 px at best | Small beside the above, a floor later |
| **Representation, builder** | A part traced in millimetres and built by the builder, in the photo | The 0096-9M2's outline lands **12 px (0.8 cm)** from the photo's | Not the limit today. The placement errors are 5-20 times larger |

In short: the photos were understood well enough in 2-D on the first day. The loss is in three places after that. The
cameras were damaged by one step on 9 Nov and never checked again. Parts were then measured through those cameras from
depth, with numbers attributed to parts by judgement. And the score that was meant to catch mistakes cannot see a part.

## 2. Cameras (`camera_audit.py`)

**Method.** Two faces have a flat in millimetres made independently of any camera: SP-2M2 from g093-b
(`out/traces/sp2m/front_mm.png`) and the 0096-9M2's door from g096-a. ALIKED + LightGlue match each flat to each
view's photo; a RANSAC homography keeps the matches on the face. Each match is a point whose place in the cabin is
known (the trace's millimetres on the cabin's panel) and whose pixel is measured. Its error is the measured pixel less
its projection through `holistic_diff.registered_camera`, lens term included. The same matches then re-solve the
camera's pose from nothing.

| View | Face | Matches | Error through the pipeline's camera | After a resection on the face | The pipeline's eye is off by |
|---|---|---|---|---|---|
| g092-a | SP-2M2 | 1036 | **81.5 px**, 6.0 cm, 4.3 % | 2.2 px | 0.24 m |
| g092-e | SP-2M2 | 261 | **96.8 px**, 4.3 cm, 4.9 % | 10.8 px (2.9 with the focal length freed) | 0.22 m, 11 degrees |
| g093-a | SP-2M2 | 40 | **38.7 px**, 3.9 cm, 3.2 % | 2.6 px (both faces) | 0.33 m |
| g093-a | 0096 | 687 | 7.0 px, 0.65 cm | | |
| g093-b | SP-2M2 | 3462 | **the camera faces the left wall: turned 180 degrees** | 4.6 px | 0.67 m |
| g098-a | SP-2M2 | 112 | **79.5 px**, 3.4 cm, 5.1 % | 2.5 px | 0.16 m |
| g096-a | 0096 | 2609 | 10.7 px, 0.64 cm, 0.3 % | 1.4 px | 0.07 m |
| g097-a | 0096 | 605 | 17.3 px, 0.61 cm | 3.2 px | 0.02 m |
| g097-b | 0096 | 789 | 11.0 px, 0.66 cm | 1.6 px | 0.02 m |
| g100-b | 0096 | 202 | 13.7 px, 0.70 cm | 1.4 px | 0.08 m |
| 14 other views | none found | | **cannot be checked** | | |

The cabinet's small errors are no comfort: its place in the cabin was taken from these same cameras (section 33), so
it agrees with them by construction. SP-2M2 is the only face placed independently, and it is where the cameras fail.

**The cause is one step.** The same matches through each camera that exists for a view:

| View | Its own face camera (8 Nov) | The joint solve (9 Nov) | After `holistic_refine.py` (9 Nov), which everything since used |
|---|---|---|---|
| g092-a | **2.0 px** | 22.1 px | 81.5 px |
| g098-a | | **3.9 px** | 79.5 px |
| g092-e | | 22.6 px | 96.8 px |
| g093-a | | 21.7 px | 38.7 px |

The refinement was added to narrow the solve across the aisle (section 32). It moved the compartment's cameras by a
median 0.18 m (up to 0.63 m) and changed focal lengths by 0.82 to 1.62. `holistic_diff.camera_for` then replaced
g092-a's good face camera with the refined one. Every ray cut with a plane in sections 33-41 (the cabinet's size, the
housing, the post's leg, the cover) went through these cameras.

**g093-b.** The joint solve gave it 35 points and a camera facing the left wall from the start; the doc noted it
"solved badly". It stayed in the comparison, scored 53.5 % (third best), passed the withheld-pin gate at 0.6 cm, and
section 38 reasoned from it about the operators' seats. It is the square-on photograph of SP-2M2, the right wall.

**What the handoff expected, and what was found.** Section 6.5 put the cameras' weakness down to halftone, cropping and
the aisle's ambiguity. Those are real, but the measured loss is mostly self-inflicted and recoverable: a resection on a
traced face reaches 1.4-3 px. One plane cannot fix the focal length (freeing it moved g096-a's eye 0.5 m for 0.5 px),
so a view needs two faces at an angle, or its focal length held. The aisle's width is open again: the unrefined solve
fits SP-2M2 better than the narrowed one.

## 3. Tying a name to geometry (`ident_audit.py`, `part_audit.py`, `sam_tools.py`)

**The first pass saw it correctly.** In `observations/g098-a.json` (8 Nov) the big rounded form beside the right post
is one object, "curved grey sheet ... folded against the post", box 0.31-0.64 by 0.56-0.92 of the frame. SAM 2's mask of
it is 0.31-0.64 by 0.56-0.89: overlap 0.90. Across nine parts in g098-a and g096-a the eye-drawn boxes overlap the
masks 0.19-0.95, median 0.77.

**It was not used.** Sections 27-41 modelled that corner from depth points and findings. The same thing was in turn
the leg's depth, a longer leg, a strut and a cover (handoff 6.1). `out/recon/audit/reading/g098-a_cover.jpg` draws the
cover's outline from `posts.right.shroud` back into g098-a through the camera it was read with, so no lift and no
camera error enter. The line runs through the middle of the latch's plates. The housing's bright rim is 100-200 px to
its right and the outer edge of its dark band about 250 px. What was traced as "the cover's aisle edge" is most likely
the post's own edge; the housing itself, which the photo shows as a deep rounded form carrying the latch plates on
its face, is not in the model.

**Identity across photos was never checked.** The id `new.seat.rear_right_folding` is the housing in g098-a and a
different object, the pale wedge-shaped box on the floor, in g096-a.

**What SAM 2 can and cannot do here.** On the descreened scans it outlines parts that differ in tone from their
surroundings to a few pixels: the SP-3M flap 0.97, the first-aid box 0.96-0.98, the chest 0.93-0.95, the lamps
0.94-0.97, the cabinet 0.97, the housing's dark band 0.86. Pressed sheet metal against the same paint is another
matter: the post's channel scored 0.76 and leaked into its neighbours, the latch plates 0.56-0.62. Those parts show
as creases and highlights, so their evidence is edge lines, not regions. The handoff's premise (6.8) holds for the
first kind and needs edges for the second.

**Parts in the photo against the model** (`out/recon/audit/parts/g098-a.jpg`, `g096-a.jpg`): green the photo, red the
model through the pipeline's camera, yellow through the camera resected on the traced face. Median distance from the
photo's outline to the model's:

| Part | How it got its place | g098-a, pipeline / resected | g096-a, pipeline / resected | Model's area over the photo's |
|---|---|---|---|---|
| 0096-9M2 | traced in mm | | 12 / 16 px (0.8-1.0 cm) | 1.2 (the plugs below it) |
| SP-3M's flap | traced through the solve | 38 / 15 px | 38 / 17 px | 1.1 |
| Chest 5937 | lifted | 37 / 33 px | 22 / 39 px | **1.8-2.0** |
| First-aid box | lifted | 11 / 52 px, a tenth beyond 107-132 | 9 / 41 px | **1.5-1.9** |
| Bulkhead lamp | lifted | 47 / 45 px (4.2 cm) | 45 / 69 px (4.7-7.2 cm) | 0.9-1.1 |
| Housing ("cover") | lifted, misread | **56 / 88 px, a tenth beyond 167-226**; overlap 0.11 / 0.04 | | |

Two things follow. A traced part is within a centimetre once the camera is right: the flap's error is mostly camera.
A lifted part is 2-7 cm out under either camera, and several get *worse* under the better camera: they were placed
through the damaged one, so their errors partly cancel in the views they were measured in and show under a true one.

## 4. The lift and the evaluation, with the truth known (`synth_render.py`, `synth_audit.py`)

**Method.** The current model is rendered through each of the 24 registered cameras as a pinhole. Each render is then
handed to the pipeline as a photograph: MoGe-2, the tie to triangulated points (the real solve's points of that view,
moved onto the model, so the pins lie where real features were found), the fusion with the traced parts, the
registration and the comparison, all by `holistic_diff.compare_view` itself. The camera and the geometry are exact.
This is the pipeline's best case: clean images, no halftone, no flash, no crop, perfect pins.

| | Result |
|---|---|
| The network's map with one scale | 3.4 cm median off the truth, a tenth beyond 14.5 cm |
| After the fusion, pinned pixels (traced parts) | 0.1 cm: they are the model by construction |
| After the fusion, free flat surfaces | **2.0 cm median, a tenth beyond 19 cm** |
| After the fusion, free pixels within 6 px of a part's edge | **5.5 cm median, a tenth beyond 24 cm** |
| The registration | In 2 of 24 views it replaced the exactly right camera with one 0.14-0.18 m away |
| **The score of an exactly right model** | **64.4 % mean; by view 15 % (g100-b) to 97 % (g092-e)** |
| Findings reported against an exactly right model | **139 over 24 views**: "the floor is higher in the photos by 0.04 m", "the cabinet is at the wrong angle by 38 degrees" |

The real model scores 34.1 %. So about 30 points separate it from the ceiling, and the comparison cannot say how many
of those are the cameras.

**Known-wrong models, scored on the real photos' maps** (the stored scores are reproduced exactly first):

| Change to the model | Mean over the 19 used views | Views down / up |
|---|---|---|
| Seats 0.25 m too high (the trap of section 38) | -2.45 % | 6 / 0, the largest fall in g093-b, the wrong-wall view |
| Cabinet back at the plan's 38 degrees | -1.40 % | 2 / 1 |
| The whole cabinet taken away | -1.64 % | 2 / 1 |
| Right post's leg and cover raked 45 degrees, not 15 | -0.33 % | 4 / 0 |
| **Right post's leg, cover and latch taken away** | **-0.02 %** | 2 / 2 |
| The strut that is not there, put back | -0.07 % | 1 / 0 |
| **Bench taken away** | **-0.01 %** | 1 / 0 |
| First-aid box taken away | -0.31 % | 2 / 1 |
| **Chest 5937 taken away** | **+0.60 %** | 0 / 4 |
| **GA-3 taken away** | **+0.10 %** | 0 / 3 |

The session's accepted steps were +0.3 (sections 35-37), +0.1 (39), +1.1 (41). Deleting a photographed part moves the
score by as much, in either direction. On the synthetic maps every wrong model does score lower, but a missing leg and
cover still costs only 0.57 %: a share of pixels cannot weigh a narrow part.

**The gate is circular.** The fused map is pinned to the *model's* traced parts cast through the view's camera, and
the withheld pins are of the same kind. A wrong camera pins a flat map to whatever flat wall it faces and passes.
That is how g093-b passed at 0.6 cm.

## 5. Transcription and representation (`repr_audit.py`)

- The shell's numbers are metres to two places. In g098-a at the post's range one centimetre is 23 px.
- Following the housing's outline to 3 px takes about 60 vertices, to 5 px about 39. The data gives the cover 13 typed
  points, 26 px at best. Nobody can type the 40: outlines have to come from the masks and edges by code, or from a
  shape with a few physical parameters (a radius, a depth).
- Every primitive in `build_cabin_interior.py` is a section carried along a body axis, typed in body coordinates.
  There is no part with its own frame, so turning or moving a part means retyping its numbers, which is how the post's
  leg came to be a stitch of readings (handoff 6.3).
- **Not measured:** whether a single extrusion can carry the post's channel or the housing in all views. That needs
  their true shape, which is part C's work. On present evidence it is second order: the builder puts a traced part
  within 12 px, and the parts that look wrong are 50-250 px out for other reasons.

## 6. The handoff's diagnosis, checked

| Handoff | Verdict |
|---|---|
| 6.1 Identification is never tied to the geometry | **Confirmed, and worse:** the identification existed, was right, and was not consulted |
| 6.2 The representation cannot express the shapes | True of pressings, but not what is costing accuracy now |
| 6.3 Readings accumulate without a re-fit | Consistent with the data; not separately measured |
| 6.4 Pixels read by eye, frames mixed | The cover's outline is the measured case: 100-250 px, in its own photo |
| 6.5 The cameras are not good enough | **Confirmed, with a different cause:** the refinement of 9 Nov, not the prints. Recoverable to 2-3 px |
| 6.6 The score cannot see what the eye sees | **Confirmed and measured** |
| 6.7 Verification was Claude looking at small renders | g093-b's render shows another wall and went unnoticed |
| 6.8 Keep the evidence in 2-D and make 3-D answer to it | Supported. Masks for parts that contrast, edge lines for pressings |

## 7. What part B has to do about it

The design is `docs/OSA_MODELLING_PROCESS.md` (proposed 10 Nov 2026, with three probes of its own). The audit fixes
its requirements:

1. **Cameras from control, each one checked before it is used.** Resect every view on the faces traced in
   millimetres; carry control into views that see no traced face through matches to views that do; two faces at an
   angle or a held focal length. A view may judge a part only when control near that part reprojects within a stated
   number of pixels. `holistic_refine.py`'s cameras are retired.
2. **Evidence per part, in 2-D, with the register's id:** a mask where the part contrasts, edge polylines where it is
   a pressing, and the same object matched across photos before it shares an id.
3. **A part's shape is fitted to its outlines in all its views at once,** with depth and triangulated points as weak
   terms. Depth alone never places a part.
4. **Acceptance is per part and per view, in pixels of outline,** with the overlay for the owner's eye. The depth
   score stays only as a way to find where to look.
5. **Numbers come from code at full resolution.** Claude names parts, says how they join and judges overlays.

## 8. Not done, and limits

- Halftone, flash and cropping were not simulated: the synthetic run is the best case, so its errors are lower bounds.
- The joint solve was not re-run on synthetic images. Flat-shaded renders have none of the print and label detail
  that the matcher uses, so the result would say nothing about the photos. The cameras were measured on the real
  photos instead, which is the stronger test.
- 14 of 24 views have no check. Only two faces have flats; the other racks were traced through the solve.
- g098-a's resected camera rests on 112 matches at the frame's edge: good beside the rack, uncertain far from it.
- The post's channel has no clean mask yet, so the post itself is not in the outline table.
- The cab's group, and the whole left side, were not audited.

## 9. Running it again

```bash
PY=E:/SAM3D-tools/venv-depth/Scripts/python.exe
$PY -X utf8 tools/recon/audit/camera_audit.py                 # out/recon/audit/camera/<photo>.jpg, camera.json
$PY -X utf8 tools/recon/audit/sam_tools.py ask g098-a --parts tools/recon/audit/prompts/g098-a.json
$PY -X utf8 tools/recon/audit/part_audit.py g098-a g096-a     # out/recon/audit/parts/
$PY -X utf8 tools/recon/audit/ident_audit.py
$PY -X utf8 tools/recon/audit/repr_audit.py
$PY -X utf8 tools/recon/audit/synth_render.py                 # about 12 minutes
$PY -X utf8 tools/recon/audit/synth_audit.py mono             # then: run, lift, sense
```
