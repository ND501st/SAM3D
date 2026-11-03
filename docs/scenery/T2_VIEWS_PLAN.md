# T2: the views and the costs — the plan (4 Nov 2026)

Phase T2 of `TERRAIN_ROADMAP.md`. Written on Opus as the design; to be built on Sonnet, step by step, each step ending
in its check. Done when: **one command renders V1-V10 on the Kesmark map, writes the milliseconds per pass at each,
and builds a page setting each shot beside its photographs' links; run before and after every later phase.**

The research behind it: `docs/research/TERRAIN_REFERENCES_2026-11.md`, second part, section 2 (a fuller design; this
plan cuts it down). Pannonia is fictional: the views are judged by eye against the photographs, not measured.

## Decisions

1. **Views are data, anchored to the map's ids, never coordinates** (as track G's routes): a changed map needs no
   edits here. File: `data/scenery/views.json`.
2. **simcore resolves them** (header-only, a test), Unreal only places the camera. One sky for now: the scenario's own
   (15 October 1985, morning). More skies later, if wanted.
3. **A view the map cannot give is reported missing with its reason**, never skipped silently: V5 (no farmyard site
   kind yet, T7) and V8's dyke (no dyke yet, T8; until then the eye stands on the bank).
4. **The photographs stay links.** The page loads them from their pages at view time; nothing is downloaded or
   committed. Their table is read from the references note's ids (P01-P92), copied into `data/scenery/views.json` as
   `photos` with `page_url`, `licence` and `author`, only for the photos the views name.
5. **Cost by Unreal's CSV profiler** with GPU stats, one short capture per view, so the numbers come from the same run
   as the shots.

## Steps

### 1. `data/scenery/views.json` (Sonnet)

```json
{
  "views_id": "terrain_views",
  "notes": "...",
  "source": "game value: sam3d estimate (docs/scenery/T2_VIEWS_PLAN.md)",
  "confidence": "estimate",
  "views": [
    { "id": "v1_field_edge", "anchor": { "kind": "parcel_edge", "near_km": [-4.0, -15.0] },
      "eye_m": 1.7, "look": "across_parcel", "fov_deg": 60, "photos": ["P01", "P03", "P71", "P77"] },
    { "id": "v7_osa_position", "anchor": { "kind": "site", "site": "bty21" },
      "eye_m": 4.0, "look": "panorama", "step_deg": 90, "fov_deg": 60, "photos": ["P03", "P12", "P40", "P83"] }
  ],
  "photos": { "P01": { "page_url": "...", "licence": "CC BY-SA 3.0", "author": "Jean-Pierre Bazard" } }
}
```

All ten views, with the anchors and photo ids of the references note's section 2.2: V2 and V3 on `vrbice`, V4 on
road `f_vrbice`, V6 the nearest farmstead to its `near_km`, V8 river `rava`, V9 the hill near `polany` (check the id in
`data/maps/kesmark_range.json`'s terrain), V10 from `vrbice`. Anchor kinds: `parcel_edge`, `settlement` (with
`from_m`, `bearing_deg`, or `along_street`), `road` (`at` fraction, `side_m`), `farmstead`, `site`, `river`, `hill`,
`farmyard`.

### 2. `simcore/include/simcore/scenery/views.hpp` + `simcore/tests/test_views.cpp` (Sonnet)

- `LoadViews(path_or_json, out, error)`: errors as values, ids unique, every photo id in `photos`, fov 20..120.
- `ResolveView(const MapData&, const DressingRules&, seed, const ViewDef&) -> std::vector<ResolvedView>`: each
  `{id_suffix, ok, missing_reason, eye_enu, target_enu, fov_rad}` (a panorama gives one per step, `_h000`...).
- `parcel_edge`: step outward from `near_km` along the parcel's short axis (`FieldHeading` + 90°) until `FieldParcel`
  changes; the eye 2 m outside, looking back across. Use the existing `FieldParcel`/`FieldHeading`, nothing new.
- Eye height is above the drawn ground (`MapData::GroundHeight`); if `ObstacleTop` there is above the eye, step 5 m
  back along the view line (up to 50 m) or report missing.
- Names avoid the macro list (`near`, `far`, `small`...): use `close_km` in code even where JSON says `near_km`.
- Test: every view on `kesmark_range.json` resolves or names its reason; eyes above ground and outside obstacles; the
  target differs from the eye; V5 is missing with "no farmyard"; a moved settlement moves V2 (anchors, not numbers).
- Run: `cmake --build simcore/build --config Release` and the tests (`tools/test.ps1 scenery`, then `-All`).

### 3. Unreal: `-SimShotSet=views` in `ASimMapActor::RunShots` (Sonnet)

- With the flag, the list is the resolved views instead of today's; without it, today's list stays.
- Set the camera's field of view per view; write `<folder>/<id>.png`, and `<id>.missing.txt` with the reason.
- After each shot, a cost capture: `csvprofile frames=60` (run with `-csvGpuStats`), wait for it to finish, then the
  next view. Name each capture after its view (`-csvname` or rename the file from `Saved/Profiling/CSV/`).
- Build with UBT; render with the command in section 4; look at every shot.

### 4. `tools/terrain_views.py` + `tools/tests/test_terrain_views.py` (Sonnet)

- `python tools/terrain_views.py run [--label before_t3]`: builds nothing; launches the offscreen render (the
  PowerShell route, since Git Bash mangles `/Game` paths), then writes `out/views/<label>/index.html`.
- The page: per view, the shot(s), the photos as thumbnails linked to their pages with author and licence, and the
  GPU milliseconds per pass (the CSV's GPU columns averaged over the 60 frames: total, base pass, shadows, Lumen,
  translucency, post), with the previous label's numbers beside them when there is one.
- Tests: the CSV parsing on a small sample file; licences only CC0, CC BY, CC BY-SA, public domain; every view in the
  data on the page.

### 5. The first run (Opus or Sonnet, then the owner)

`terrain_views.py run --label t1` gives the baseline. Look at every view against its photographs; write in the roadmap
what each view lacks and which phase brings it. Then T2 is done.

## Where to switch models

- Steps 1-4: Sonnet. Stop and come back to Opus if a resolved view is wrong and two fixes have not found why, or the
  CSV profiler's GPU columns are not what step 3 expects.
- Before the commit: Opus for the review (`/code-review`).
- After T2: **Opus for the design of T3 and T4** (the colour map and haze; the plain's layout in simcore, which changes
  the sight lines).
