# The Osa interior rebuilt by the new process: part C

*10-11 Nov 2026. Part C of `docs/OSA_MODELLING_HANDOFF.md` section 11, by the process of
`docs/OSA_MODELLING_PROCESS.md`. **Two of the three hard parts are rebuilt and pass. The bench and the operators'
seats are not rebuilt** (section 6). On the way the compartment's right side and the 0096-9M2 were moved to where the
photographs have them, by the project owner's decision of 11 Nov. Nothing is committed. Overlays and renders are under
`out/recon/osa/` (git-ignored: they hold the book's photographs).*

## 1. What was built

| Stage | Tool | State |
|---|---|---|
| 1 Cameras | `tools/recon/osa/cameras.py` (`flats`, `match`, `solve`, `show`, `points_in`); control in `tools/traces/osa_interior/control.json`; result `cameras.json` | Built. Four control faces, 13 views grade A |
| 3 Evidence | `tools/recon/osa/evidence.py` (`tile`, `measure`, `draw`); picks and measured outlines in `tools/traces/osa_interior/evidence/<photo>.json` | Built |
| 4 Parts | `tools/recon/osa/parts.py` (`Extrusion`, `Tube`, `RoundedBox`), `store.py`; `data/cabins/osa_ak_parts.json` | Built for what was needed. `PressedChannel` and `Loft` are not |
| 5 Fit | `tools/recon/osa/fit.py` (`plane_of_curves`, `star_profile`, `curve_on_plane`, `depth_of`, `rounded_box`); recipes `recipes/right_post.py`, `recipes/right_kit.py` | Built |
| 6 Acceptance | `tools/recon/osa/accept.py`, `render.py`; `tools/tests/test_osa_parts.py` (34 tests, no photographs needed) | Built |
| 7 Into the game | `build_cabin_interior.py` builds every part of the parts file into `SM_OsaHull` and skips what a part supersedes; `jsonedit.py` changes single values in the hand-formatted data; `migrations/` holds the two moves | Built. Unreal has re-imported none of it |

## 2. Cameras

**Control faces.** SP-2M2 is the frame (fixed). Three plates are solved with the cameras: the 0096-9M2's door (its
traced flat), and two made from a rectangle in a photo, each side a line fitted along a picked edge chain (straight
to 0.3-0.5 px over 200-1200 points): SŁ-2M's face from g094-a and SP-3M's white panel from g098-b. A plate made this
way has no sourced size, so its scale and aspect are solved too.

| Grade | Views | Control (median px) | The same view solved without its own control |
|---|---|---|---|
| A | g092-a, g092-e, g098-a (SP-2M2) | 1.6, 1.7, 1.4 | 3.1, 40.0, 4.7 px |
| A | g096-a (SP-2M2 and the door) | 2.2, 1.2 | 2.9 |
| A | g093-a, g097-a, g097-b, g100-b (the door) | 1.4, 2.2, 1.6, 1.4 | 1.5, 4.1, 2.7, 10.4 |
| A | g094-a, g094-d, g095-c, g091-a (SŁ-2M) | 0.4, 1.0, 1.4, 2.0 | 2.9, 2.7, 2.3, 4.6 |
| A | g098-b (SP-3M's panel) | 0.9 | 15.1 |
| B | g094-b, g095-a, g095-b, g099-b: SŁ-2M in view but 4.4-6.6 px. g093-c, g098-c, g099-a, g099-d, g100-a, g100-c, g100-d: no control | | |
| C | g093-b | flats only | |

Tie points sit at 0.5-1.4 px. g092-e is grade A only beside its control (a close view of one plane). SP-3M's panel is
seen from nearly one direction in all its photos, so its proportions rest on little: g098-b is for looking.

**Rules that came out of it:** a face needs 60 matches to hold a camera (g093-a's 40 on SP-2M2 were wrong and pulled
it 20-40 px); a face placed through an earlier solve is never fixed control.

## 3. The frame, and what moved

**The racks' faces stand 2.01 m apart.** SŁ-2M's face solved upright and parallel to SP-2M2's within 1.5 degrees
without being told to be, its control at 0.4-1.4 px in four grade A views. The cabin had 1.71 m.

**Which side was wrong: the turret's axis says the right.** The canvas boot round the turret's central duct hangs on
its axis. Its mask in g096-a and g100-b (0.96, 0.98; the bearing steady to half a degree over its height) puts the
axis 0.914 m from SP-2M2's face and 1.096 m from SŁ-2M's. The cabin had SŁ-2M's face 1.104 m from the centre line
(section 28, from the cab's own camera and the left post): the two agree to a centimetre, by different roads. It had
SP-2M2's at 0.612 m, from mon_2112_80 Fig. 3. **So the left side stands, and the right racks with everything that
hangs on them moved 0.302 m outboard** (`tools/recon/osa/migrations/frame_2026_11.py`):

- `data/cabins/osa_ak.json`: the panels `sp2m_panel` (x 0.914) and `sp3m_panel`; the meshes of their faces and flap,
  the two lamps and the light; the racks' carcasses rebuilt (SP-2M is now 0.43 m deep, was 0.73).
- `data/cabins/osa_ak_shell.json`: the wheel arch, the aft right plinth, the forward step and arch, the trough at the
  racks' foot and the aisle sheet's edge with it, the corner slope, the lower wall's foot, the roof's insulators.
- The chest, in its builder, with them (not measured again).

**The 0096-9M2** (`migrations/cabinet_2026_11.py`): its door, as a plate solved with the cameras, stands **0.19 m
nearer the centre line, 0.05 m further forward, turned 16.4 degrees (was 15.7), and 6.4 % larger** (the body 0.607 m
wide). Changed with it: `cabinet0096_trace.py`'s `SCALE` (0.865 to 0.9199), the panel `pwr_cabinet` with its seven
controls and five lamps (re-read from the trace), the mesh's place, the plinth under it, two tests that pinned the
old values.

**Not moved, and now the plainest differences in the renders:** the bulkhead's door and its fittings, the bulkhead
lamp, the roof and the turret's opening, the floor's middle, the left post. They are in the audit's list of what
was measured through the damaged cameras.

**The scale is still open, and nothing was rescaled.** Three readings:

| Yardstick | Says the frame is |
|---|---|
| SP-2M2 is 1.89 m wide (assumed from SŁ-2M's) | right |
| SŁ-2M, sourced 1.89 m (mon_2112_80 Fig. 3), measures 1.98 m edge to edge here (1.94 over its plates) | 3-5 % large |
| The aft door, sourced 0.59 m in the same drawing; the earlier session read 0.65 through the old cameras | 9 % large (not measured again: its photos are grade B) |

The left racks' depth to the wall is 0.22 m at this scale, shallow for blocks that hold tubes, which argues the same
way. The earlier session's other two yardsticks (the R-123M's face, the distance to the seats) are in the cab's group,
which was never to scale with the compartment. A factor of about 0.93-0.96 is likely. Applying one would resize
SP-2M2's accepted trace and every part with it: it needs one object of sourced size on a control face, and the owner's
word.

## 4. Rebuilt and accepted

Median distance from the photo's measured outline to the part's, in pixels (the worst tenth in brackets). The gate is
the larger of 4 px and 0.25 % of the photo's width.

| Part | g098-a | g096-a | g092-a | As it was, through the camera it was judged by |
|---|---|---|---|---|
| Right post, aisle edge | **3.1** (7.0) | **3.0** (6.6) | **0.9** (2.8) | 50, 48, 26 |
| Housing behind the post | **3.1** (5.4) | **1.0** (1.9) | **0.9** (1.2) | 151, 116, 64 |
| Housing between it and the cabinet | **2.9** (6.5) | its source | | not measured |
| First-aid box | **1.6** (6.1) | **1.3** (5.5) | | 37, 35 (audit) |
| Latch keeper bracket | 9.5 (31) **fail** | its source | 14.2 (59) **fail** | |
| Slotted plate, strap | no clean mask | its source | no clean mask | |

About 1-2 mm at the part for those that pass.

**The post** is one pressed member whose forward face lies in one plane (y 0.460): its aisle edge is one unbroken line
from under the lamp to the floor. Which of its several parallel edges is the same edge in each photo was settled by
trying the combinations: the right ones fit one plane, the wrong ones none. It rakes **22 degrees** below the knee (the
model had 15, and 45 before that).

**The two housings are one wall.** Section 41's "cover" is a sheet-metal housing behind the post's lower part: a
flat face inside a rolled rim, a flat top, a flat side to the aisle, a large rounded corner. The "housing beside the
cabinet" of section 35 is its lower continuation towards the cabinet, its top edge sloping from z 1.14 down to 0.87.
Their faces share one plane, fitted to five outlines at once: **turned 12 degrees towards the aisle and leaning back
4** (the cabinet beside them stands at 16). Held square, the lower housing's face left 4 px and a worst tenth of
15-19; turned, 2-3 px and 5. Four tie points inside the lower face, placed by the certified cameras, lie on that
plane. Its outline was taken from g096-a alone and, drawn through g098-a's camera, fell on the photo's edges there:
2.9 px. Its sloped top runs back 0.26 m, to where g098-a shows it end (1.2 px).

**The first-aid box** is a box with rounded edges fitted to its masks in two photos from a blind start: 0.33 × 0.25
× 0.07 m. `SM_FirstAid` keeps its cross, seam and strap, set on the fitted box's front.

**What the process caught while this was done:**
- A chain taken for the right housing's foot was the strap plate's lower edge: the worst tenth of one view, and
  plain when drawn back.
- A pale panel in g095-c taken for the lower housing was the box on the floor forward of the cabinet: on the
  housings' plane it landed under the floor, and missed by 326 px. It is recorded as `not_this` in that photo's
  evidence.

**SAM3D's choice, not measured:** the right housing's depth (0.16 m: what shows), its side behind the post, its
foot; the post's thickness (0.045 m), its web behind the rack's end, its run beyond what is seen (z 0.93-1.75); the
first-aid box's thickness is weakly held. The post is one flat member: its flanges are not modelled.

## 5. Not done in these groups

- **The latch.** Its plates are outlines from one photo set in front of the housing's face. The bracket fits best
  8 cm in front, which says it is not a flat plate on that face. The bail is not built.
- **The chest** moved with the racks and was not measured again; in the renders it is still too large.
- **The pale box on the floor** forward of the cabinet, the panel under the right housing, the casting before the
  lower housing.

## 6. The bench and the operators' seats: not rebuilt

What the photographs show is not what the model has, and it needs part types that do not exist yet:

- **g093-a shows the seat backs standing up**, padded in olive with studs, on a steel pan. g094-d and g095-c show
  the same backs folded forward, their steel rear uppermost, with canvas bags strapped on. Section 38 read the
  folded backs as "pans on the bench" and took the backs out of the model. The game's seats have no backs now.
- Each seat is its own tubular frame carrying an olive cushion, on a base, not a pan on a bench.
- g094-d and g095-c are grade A now, and the left side stayed where it was, so nothing blocks the work. It needs a
  tube-frame part, a pan with a rolled rim, a hinge, and a decision the owner should make: **the backs drawn upright,
  as the crew sits, with the folded state only for comparing with the photos.**

## 7. Checks

`python -E -P -X utf8 -m pytest tools -q`: 321 passed (34 new). `tools/check_citations.py`: 107 files, no problem.
`simcore_tests`: 663, no failure (twice clean; two earlier runs stopped early at different places with no failure
reported, while Blender was building). `tools/recon/interior.py`: unchanged. `cabin_mesh_sync.py --check`: in step.
Rebuilt: `SM_OsaHull`, `SM_RackSP2M`, `SM_RackSP3M`, `SM_Cabinet0096`, `SM_FirstAid`, `SM_Chest5937`.

Pictures: `out/recon/osa/sheets/<photo>_before_after.jpg` (photo beside the model, as it was and now), and
`<part>_overlays.jpg` (green the photo's outline, yellow the part, red as it was).

## 8. Running it

```bash
PY=E:/SAM3D-tools/venv-depth/Scripts/python.exe
$PY -X utf8 tools/recon/osa/cameras.py flats && $PY -X utf8 tools/recon/osa/cameras.py match && $PY -X utf8 tools/recon/osa/cameras.py solve
$PY -X utf8 tools/recon/osa/evidence.py tile g098-a 350,950,1150,1950     # numbered chains to pick from (add --masks)
$PY -X utf8 tools/recon/osa/evidence.py measure g098-a                     # picks -> outlines
$PY -X utf8 tools/recon/osa/recipes/right_post.py && $PY -X utf8 tools/recon/osa/recipes/right_kit.py
$PY -X utf8 tools/recon/osa/accept.py --write                              # px per part and view; overlays
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --python tools/blender/build_cabin_interior.py -- --only SM_OsaHull,SM_FirstAid
python -E -P -X utf8 tools/cabin_mesh_sync.py osa_ak
$PY -X utf8 tools/recon/osa/render.py g098-a g096-a g092-a                 # photo beside the model
```
