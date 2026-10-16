---
name: model-from-photos
description: Build a 3D model of real equipment (radio, intercom box, console block, instrument) from reference photographs for SAM3D, with a 3D trace, overlap tests and a check against the photos. Use whenever the user asks to model, rebuild, refine or trace a piece of equipment from photos or reference images.
---

# Model equipment from photographs

Follow `docs/MODELLING_FROM_PHOTOS.md` step by step: it is the process the user asked to be used every time an accurate
reference exists. Read it first. In short:

1. **References**: collect several photos in one pass (Bing Images in the built-in browser; read every result's
   `a.iusc` `m` JSON with one script). Choose the main photo (sharp, square-on, whole front) and a second from another angle
   (above is best); find one real dimension from a cited source. Register the photos in `docs/SOURCES.md`. Photos go in the
   scratchpad, never the repo.
2. **Flatten**: write `tools/traces/<name>.json`, run `python tools/photo_trace.py grid` then `rectify --check`. Edges must
   lie in the front's plane and agree within about 1 degree; then set `plate_edges` and `width_mm` and rectify again.
3. **Read**: `circles` for round parts, `tiles` for everything else (5 mm grid); the second photo with `ellipses` + `align`;
   heights out of the plate from the second photo.
4. **Trace module** `tools/blender/<name>_trace.py` (mm, y down, `Z` heights, plates, arcs, `CONTROL_MM`), no bpy.
5. **Overlap tests** `tools/tests/test_<name>_trace.py` before modelling (pattern: `test_r123m_trace.py`).
6. **Model** `tools/blender/<name>_model.py` with the helpers in `r123m_model.py`; iterate in the live Blender through the
   Blender MCP (`execute_blender_code`, reload modules, render to files: the splash can hide the viewport).
7. **Prove it**: `tools/blender/trace_check.py` renders (front ortho; from the photo's solved camera, after
   `photo_trace.py camera`; close-ups), then `photo_trace.py overlay` both ways. Look at the edges-on-photo image: that is
   the acceptance test. Look at the close-ups for parts running into each other.
8. **Cabin and Unreal**: export headless (`build_<name>.py` updates the manifest), shell part + **bare** panel + controls
   in `data/cabins/osa_ak.json`, import with no Nanite (`PLAIN` in `tools/ue_import_cabin_interior.py`), photograph with
   `tools/ue_panel_shot.py`, revert unchanged re-saved material assets, run all checks, commit.

Show the user the photo overlay and an in-cabin shot at the end. Say what is measured, what is scaled (with the yardstick
and its accuracy) and what is SAM3D's choice.
