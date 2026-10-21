# Modelling equipment from photographs

How SAM3D builds a 3D model of a real piece of equipment (a radio set, an intercom box, a console block) when good
photographs exist. First used for the R-123M (`tools/blender/r123m_trace.py`, `r123m_model.py`) and the R-124 boxes
(`r124_model.py`), October 2026. The R-123M's first, flat trace put labels and parts out by up to 10 mm because the photo
was taken from above and to the side and the front isn't flat; this process fixes both.

The rule: **trace in millimetres in a true front view, with every part's height out of the plate, and prove the model by
putting it back on the photograph.**

## 0. References (15 minutes, before any modelling)

* Collect several photographs at once. Search images in the built-in browser and read the result list with one script
  instead of opening each result: on Bing Images, `document.querySelectorAll('a.iusc')` gives each result's `m` JSON with
  the title (`t`), page (`purl`) and image URL (`murl`). Download the useful ones into the scratchpad (never into the repo).
* Pick the roles:
  * **the main photo**: the sharpest, most square-on, highest resolution, the whole front visible. Positions come from it.
  * **a second photo from another angle** (from above is best): heights of the raised parts, and whatever the first hides
    (a closed cover, a label).
  * close-ups or sale photos for the small print and colours.
* Find **one real dimension** from a cited source (height or width of the front). Everything is scaled from it.
* Register every photo in `docs/SOURCES.md` (link, tier, what it was used for). Prefer museum and operator photos.

## 1. Flatten the main photo (tools/photo_trace.py)

Write `tools/traces/<name>.json` (`name`, `photo_size`, `height_mm`, `edges`), then:

```bash
python tools/photo_trace.py grid tools/traces/<name>.json PHOTO       # gridded photo: pick edge windows from it
python tools/photo_trace.py rectify tools/traces/<name>.json PHOTO --check
```

* `edges`: 8-12 windows, each round **a straight edge that lies in the front's plane** (the plate's top and bottom edges,
  label-plate edges, panel frames), both horizontal (`h_...: [x0, x1, y0, y1]`) and vertical (`v_...: [y0, y1, x0, x1]`),
  spread over the whole front. Not: the edges of parts that stand out (a hood, a cap), knob levers, reflections, glass.
* `rectify` fits each edge (RANSAC on the strongest gradient), finds the two vanishing points and the homography that
  sends them to infinity. **Every edge should be within about 1 degree of its vanishing point**: one that isn't is on a
  raised part or a reflection: move or drop its window. All agreeing means the front is one plane and the view is now square.
* The first run has no scale: it writes `flat_unscaled.jpg` with a grid. Read the plate's top, left and bottom edges off it
  into `plate_edges` (keep `"offset": [500, 100]`), set `width_mm` from the flattened right edge, run again: `front_mm.png`
  is the front at 5 px/mm with a 40 mm margin, and `front_mm_grid.jpg` the same with a mm grid.
* Check: a round part's circle fit (next step) should come out round. The R-123M's band selector was round to 0.6 %.
* **A manual's drawing** of the front (the R-111's ТО fig. 4) is flattened the same way (`tools/traces/r111_drawing.json`)
  and names every control by its number, but take positions from the photograph: the circle fits showed the drawing
  about 4 % wide. Keep both and say which was used.

## 2. Read the front off it

```bash
python tools/photo_trace.py circles tools/traces/<name>.json well=278,162,42 drum=174,107,44 ...   # guesses in mm
python tools/photo_trace.py tiles tools/traces/<name>.json                                         # gridded mm tiles
```

* **Circles** (wells, rims, lenses) by fit: to about 0.3 mm. A fit can catch a well's outer chamfer instead of its
  opening; when a plate beside it then seems to overlap it, the opening is the smaller radius.
* **Everything else** off the 5 mm-grid tiles (look at each tile once, write positions straight into the trace): plates
  (rect), screws, holes, windows, studs, the outline's corners and steps. About 1 mm.
* **The second photo** into the same frame: `ellipses CONFIG PHOTO2 name=px,py,r0,r1 ...` fits the same round parts in
  the raw photo (centre guess and a radius band in px), then `align CONFIG PHOTO2 name=px,py:x_mm,y_mm ...` maps it onto
  the front with those centres. Agreement of 1-2 mm means the two photos show the same layout; the blend it writes shows
  what each photo adds.
* **Heights** (`Z` in the trace): from the second photo taken from above, the lit tops of raised parts show their height;
  recesses show their walls. Record each with its reading (good to about 2 mm).
* Where the photos disagree with each other or with the written sources, take the written source and note both
  (memory: written sources over game art).

## 3. The trace module (tools/blender/<name>_trace.py)

No `bpy`: plain data so Blender, the sketch tool, the cabin layout and the tests can all read it.

* Frame: x mm right, y mm down from the front's top left; `to_m(x, y)` gives the model's metres from the centre.
* Tables: the wells (with which cabin control stands in each), holes, screws, plates (rect, text, cap height, rotation,
  `cut` for a plate cut round a well), arcs (centre, radii, angles, words, studs), windows, the outline, `Z` heights,
  `CONTROL_MM` where the cabin's controls go.
* The docstring says how the trace was made and from which photos (with their SOURCES.md ids).

## 4. Overlap tests first (tools/tests/test_<name>_trace.py)

Before modelling, so the trace is right before anything is built on it (`test_r123m_trace.py` is the pattern): nothing
outside the front; no screw in an opening or on another screw; openings clear of each other; plates clear of openings
unless cut round them, and clear of each other; the cabin's controls in their wells or clear of everything; every word fits
its plate (font metrics); the outline is closed and its area right. These caught two real collisions on the R-123M.

## 5. The model (tools/blender/<name>_model.py), in the live Blender over MCP

* Build with the interior builder (`build_cabin_interior.Builder`, `finish`) and the helpers in `r123m_model.py`: `flat`
  (slab from an outline), `ring`, `disc`, `sector_pts` (arcs with round ends), `rect_minus_circle` (a plate cut round a
  well), `engrave` (raised letters, optional tilt for a sloping face), `engrave_arc` (letters one by one along an arc),
  `screw` (seated on whatever surface it is on: `surface_h`), knobs.
* Iterate in the open Blender through the MCP (`execute_blender_code`): `importlib.reload` the trace and model modules,
  rebuild, render. The splash screen can cover the viewport: render to files and read them instead of screenshots.
* Leave the wells of the controls the cabin simulates empty (the cabin draws their knobs); pass `knobs=True` for renders.
* `EDGE_SPLIT` at 30 degrees before export, so Unreal doesn't average the normals of a plate into its wells.

## 6. Prove it on the photographs (tools/blender/trace_check.py, then photo_trace overlay)

```python
import trace_check as C, json
cfg = json.load(open(r'tools/traces/<name>.json'))
front = {'x': <the front plane's Blender X>, 'y': <its centre's Y>, 'z': <its centre's Z>}
C.render_front(front, cfg, r'out/traces/<name>/render_front.png')
C.render_photo_camera(front, cfg, r'out/traces/<name>/camera.json', r'out/traces/<name>/render_photo.png')
C.render_views(target, r'out/traces/<name>/close', {'left': (0.22, -0.1, 0.12), ...})
```

```bash
python tools/photo_trace.py camera  tools/traces/<name>.json PHOTO              # solve the photo's camera first
python tools/photo_trace.py camera  tools/traces/<name>.json PHOTO --focal-35mm 34   # a near square-on photo: hold the focal
python tools/photo_trace.py overlay tools/traces/<name>.json out/traces/<name>/render_front.png
python tools/photo_trace.py overlay tools/traces/<name>.json out/traces/<name>/render_photo.png --photo PHOTO
```

* **Front overlay**: the square-on render on the flattened photo: positions.
* **Photo overlay**: the model rendered from the photo's own solved camera (1-2 px rms) with its edges in red on the
  original photo: positions *and* heights, since raised parts and recesses must line up in perspective too. This is the
  acceptance test.
* **Close-ups** from three-quarter angles: parts running into each other, letters standing on the right surface, screws
  seated.

* A photo taken nearly square-on leaves the focal length free (the solve returns f = 0 or runs off): hold it with
  `--focal-35mm`, from the EXIF (the R-111's 34 mm) or a phone's usual 26 mm when there is none (the R-407's).
* A part well proud of the front (the R-407's meter housing, 32 mm) lies off the flattened plane: read its face off the
  photo, then **back-project** it through the solved camera onto its own plane before tracing it, or it lands shifted by
  the parallax.
* **Sets that share a builder**: the PU-12M's radios have one trace schema (`r111_trace.py`, `r407_trace.py`: plate,
  regions, round parts with their `sim` control, lamps, tubes, letters, screws, `CONTROL_MM`) and one builder
  (`tools/blender/radio_front_model.py`); a new set of the kind is a trace module, a colour scheme and a line in
  `build_pu12m_radios.py`.

## 7. Into the cabin

* Export headless (`build_<name>.py`), which also writes `art/cabin_interior/manifest.json` (size, and the face plane).
* In `data/cabins/osa_ak.json`: the shell part (mesh, centre, size, yaw), a **bare** panel in the plane where the
  controls' shafts come out (the mesh carries its own front), the controls and lamps at `CONTROL_MM`; keep the set's back
  where it was when its depth changes.
* Unreal: `python tools/ue_import_cabin_interior.py meshes --only SM_...` with the mesh in the import tool's `PLAIN` list
  (no Nanite: its reduced fallback drops millimetre letters and screws), then photograph it in place with
  `tools/ue_panel_shot.py`. Close the editor and revert the material assets it re-saved unchanged before committing.

## Vehicles

A vehicle's side is not one plane, and a museum photograph of it is seldom square-on or taken from far away. First used
for the PU-12M (`tools/blender/pu12m_trace.py`, `pu12m_model.py`; October 2026):

* **Read the side** off a side photograph with `tools/vehicle_trace.py` (a 1-D projective map through the wheel hubs on
  the published wheelbase, heights from one published height). That is a first reading only: it assumes the camera far
  away.
* **Fit the camera and the main dimensions together** with `tools/vehicle_fit.py`: the hull as a handful of parameters
  (wheelbase, axle spacing, overhangs, the heights of its ends, the widths at the ends and the roof, the tyre), the
  camera as a pinhole, and the points read off the photo (hubs, ground contacts, the ends' corners). Published dimensions
  go in as priors: tight where a source is sure (the length, the roof's height, the track), loose for SAM3D's readings.
  **Take the focal length from the photo's EXIF** (35 mm-equivalent focal / 36 x the width in px) and hold it: free, a
  side view trades focal length against depth and the fit runs off to a wide lens close to the hull. Drop a point read
  too uncertainly (a soft corner) rather than let it pull the fit.
* **Back-project** the other features with the fitted camera onto the vertical plane each lies in (`vehicle_fit.py ...
  backproject`): the chine's ends, the bottom, the bow plate's foot.
* **Widths** come from a plan view (a manual's equipment plan, a top-view drawing) scaled to the published width
  separately from the length; a side view hardly constrains them.
* **Prove it** as for a front: render from the fitted camera (`trace_check.render_world_camera`, which renders the
  plates' creases for the edge finder) and `photo_trace.py overlay ... --photo`. A photograph of a sister vehicle (the
  BTR-60PB for the PU-12M's hull) proves the shared shape; the variant's own fittings are compared with its own photos.
* What a single photograph cannot settle, say: the PU-12M's photo reads its height about 5-8 % larger against its length
  than the published dimensions; the model keeps the published ones and the overlay shows the roof 6-10 cm low.

## Interiors from a plan drawing

A manual's equipment plan is a view from above: no perspective to undo, but no heights either. First used for the
PU-12M's fighting compartment (`tools/blender/pu12m_interior_trace.py`, `build_pu12m_interior.py`; October 2026):

* **Scale the plan on the hull's outline** (the bow's edge, the stern, the chines) to the published length and width,
  separately if the drawing's ratio is off, the same way the exterior's trace does, so the inside and outside agree.
* **Read each numbered item** on a 10 px grid (`zgrid`-style crops at 2-2.5x): its rectangle or circle, and which number
  it is from the labels' leader lines. Where two shapes overlap in the plan they stand at different heights.
* **Heights** from the workplace drawings and photographs, scaled against the seats and the hull: say "estimate" with
  them. Put the eye points where the crew's body model puts them (`data/crew/body.json`: 1.22 m above the floor).
* **The hull's inside** from the exterior's stations a few centimetres in. Test before modelling: every item's corners
  inside it, no two items overlapping, each eye point with room for the head and over its seat, every control in reach
  (`tools/tests/test_pu12m_interior_trace.py`, `simcore/tests/test_crew_body.cpp`). Move an item only as far as the hull
  makes it, and note it.
* **Generate the cabin layout from the trace** (`tools/pu12m_cabin.py`): the panels on the items' faces, so the meshes
  and the panels cannot drift apart.
* **Prove it on the plan**: render the model straight down with the roof clipped away and draw its edges over the scan
  in the plan's own pixels (the PU-12M: `out/pu12m_interior/overlay_plan.png`). Then photograph every seat in Unreal
  (`-SimCabinShots=<folder>`, `unreal/README.md`).

## Without a good reference

A part seen only from one angle, or only in a drawing: still flatten what there is, but mark sizes as scaled (with the
yardstick: "scaled from the photo with the switch knob as yardstick, about 15 %"), and say in the layout's notes which
positions are SAM3D's choice (the R-124 boxes).
