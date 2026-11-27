# The Osa as a 3-D vehicle: interior and exterior roadmap

Written 8 November 2026. **Priority (project owner, 8 Nov 2026): the interior first, with the most effort and close
attention to detail; the exterior second.** The player spends the game inside the 9A33BM2: at the console, in the cab, at
the hatch. The exterior is seen from outside the vehicle, on the march, and by other players' crews.

Inputs: *SA-8 Gecko in Detail* (`wwp_gecko`, research `docs/research/OSA_BOOK_WWP_2026-11.md`, per-photo notes in
`reference/sources/wwp_gecko/notes/`), the interior built in October (`docs/INTERIOR_HANDOFF.md`, `data/cabins/osa_ak.json`,
`art/cabin_interior/`), the method `docs/MODELLING_FROM_PHOTOS.md`, the exterior pipeline
`docs/OSA_EXTERIOR_RECONSTRUCTION.md`.

## 0. Where the interior stands, and what the book changes

The cabin today (October, `docs/INTERIOR_HANDOFF.md`): the compartment's plan from the Polish manual's Fig. 3
(`mon_2112_80`, 4.785 mm per pixel, SŁ-2M = 1.89 m), 17 panels, 62 blocks, 101 controls, 62 lamps, 12 displays, ~700
fixtures, 23 meshes built by code in Blender, baked panel faces, CRT displays, lighting fixed. Its operator columns are
exact (SAM Simulator's straight-on photos at 1 mm per pixel, `samsim_panels`). **Its weak parts are everywhere else:**

| Area | Today | What the book gives (photo ids) |
|---|---|---|
| Right-wall racks SP-1M, SP-2M, SP-3 | ~300 "rack fill" fixtures placed by rule, **no position measured** | SP-2M2 square-on with all 18 blocks and its drawing (g093-b, g093-d, g092-a/d/e); SP-1M drawing and top (g090-a/b); SP-3 face, its fold-down front and drawing (g098-b/c/d) |
| Left rack SŁ-1M (gyro, stabilisation) | blocks from rwd-mb3 names, faces estimated | whole rack square-on, five blocks, drawing (g091-a/c, g086-a) |
| SŁ-3M, power cabinet 0096-9M, GA-3 | plain boxes, "no source" | SŁ-3M elevation drawing (g099-c), the cabinet's face legible and two drawings (g097), GA-3 on its pedestal (g099-d) |
| Main console SŁ-2M | exact columns; range indicator name open; strip of "lamps" | whole console and a 17-block drawing (g094-a/c); **range indicator ОС81-9М**; the strip is **eight small dials** (g095-a); a ПАНЕЛЬ ИНДИКАЦИИ box beside the guidance column (g095-b) |
| Cab: dash, driver, commander | blank gauge discs, a 2×12 window matrix estimated, levers estimated | driver's station, wheel, pedals, gear lever (g083-a, g084-a/c, g085-c, g086-d); **ПУ-303 face and a labelled drawing** (g084-b/d); commander's dash with **ПРОХОД ПОД МОСТОМ** board, 9 toggles, DP-3B (g085-a/b, g087-d); right wall with the A-2 box, fuse panel, heater (g087-c) |
| Seats, bench, curtain | seat shapes estimated | front seats and their folding (g088-a/b/c, g095-c), operators' bench, cushions, belts and seat backs (g092-c, g093-a, g094-d, g095-d), the curtain and its rail (g083-c) |
| Radios | R-123M modelled from photos | both R-123M with БП-26 on shock mounts, the ЗИП box (g090-a/c/d, g091-b/d) |
| Roof, hatch, lamps | hatch from one USAF photo | roof hatch inside (g082-a), periscope blanks, visors, wiper motor, blue lamp (g089-a..d) |
| Rear of the compartment | bulkhead and door estimated | door to the engine compartment with its ГОМ plate, blower, outlets, turret pylon and its fabric skirt, floor covers (g096-a, g100-a..d, g097-b) |
| Floor | plain plates | floor mat, radio well, step and tunnel covers (g083-b, g093-c) |

**Decision D1 (project owner, 8 Nov 2026): the game keeps the 9A33BM2 (Osa-AK).** Every interior photo in the book is one ex-NVA **9A33BM3-1 (Osa-AKM)**: **Exception (9 Nov 2026):** the distribution cabinet follows the photos (the 0096-9M2, its stance and size), at the owner's word that its angle was wrong: `docs/OSA_INTERIOR_HOLISTIC.md` section 33.
roof plate 9А33БМ3-1, launch block 9М33М3, suffixes М1/М2/М3. The game's system is the **9A33BM2 (Osa-AK)**. Outside the
two look the same; inside the blocks are the same family with later suffixes, a few changed faces and, later, a ПКР toggle.
So: take shape, size, colour, material and fixings from the BM3-1 for every
block that the AK shares, and keep the AK's face and labels where a manual (`mon_2112_80`, `voenizdat_osa_1991`, `rwd_mb3`) says
the AK's block differs. Every block records which vehicle its face came from. The same data then gives a BM3 cabin later
(I9) for little work.

## 1. Accuracy targets

| What | Interior target | How it is reached and proved |
|---|---|---|
| Compartment (walls, floor, roof, bulkheads) | ±10 mm | `mon_2112_80` Fig. 3 plan; the racks' known widths; the exterior plate hull (X3) outside it, the lining inside it by the plates' thickness |
| Rack and block outlines | ±3 mm | each square-on photo flattened (`photo_trace.py rectify`) and scaled from a block whose size is known (SŁ-2M 1.89 m; blocks of one standard width in a rack; the R-123M's sourced front) |
| Controls, lamps, dials, plates on a block | ±1-2 mm | circle fits and 5 mm-grid tiles of the flattened face, as the R-123M (`docs/MODELLING_FROM_PHOTOS.md` §2) |
| Labels | the real text where legible; otherwise marked unread | the book's plates read by the readers; never invented. A plate read once is confirmed by a second photo or a manual before it goes into `data/` |
| Colours | from the photos, corrected for flash and the halftone | measured on descreened crops against a neutral (the white labels), per vehicle; NVA/Soviet paint for the 1980s, Czech 2000s repaint excluded |
| Small details (fasteners, seams, ribs, edges, clips, chains, seals, labels, profiles, wear) | every detail the photos show is listed, and each is built, or left out with a reason | each photo's detail inventory (H8), each detail cast into the cabin (H9), built by the detail kit (I10) or as a normal map, decal or texture by the build rule (`docs/OSA_INTERIOR_HOLISTIC.md` §12) |
| Proof | every face overlaid on its own photo (the §4 overlay of the method); the whole cabin rendered from the camera of each wide interior photo (g082-b, g092-a, g094-a, g096-a) and laid over it | **the look-alike check** (H7): for every photo of a zone, ≥ 90 % of the photo's structural edges explained by the model and ≥ 85 % of the model's edges supported, within 0.5 % of the width; every component `agrees` or has its reason; every high and medium detail built (`docs/OSA_INTERIOR_HOLISTIC.md` §11) |

## 2. Interior roadmap

Revised 8 November 2026 (project owner: "build out the entire interior ... an additional holistic layer ... fuse
sources ... maximum accuracy"). The interior is now built **component by component from one register**: every
physical thing in the compartment (hull, racks, blocks, boxes, seats, shelves, brackets, plates, cables, lamps, kit)
has an id in `data/cabins/osa_ak_components.json`, its sources, its place with an uncertainty, and a status
(`missing` → `placed` → `modelled` → `traced` → `face_traced`). The **holistic layer** (H, `docs/OSA_INTERIOR_HOLISTIC.md`)
fills and keeps that register; the **measurement pipeline** (`docs/OSA_INTERIOR_PIPELINE.md`) moves its entries up the
statuses; the **I steps** below build them into the cabin. Nothing is built that the register does not place from a
source, and nothing in the register stays unbuilt without a reason in the coverage report
(`docs/research/OSA_INTERIOR_COMPONENTS_2026-11.md`).

Revised 9 November 2026 (project owner: the major assets are placed well, but "surrounding details and structural
assets are way off or not even included ... Needs to be an extra step in the pipeline that cross-references imagery
with the 3d output"; leave the measuring pipeline unchanged). The holistic layer gains its second half, **the look-alike
check** (H6-H9): every photo is rendered against the built cabin from its own camera, and the differences are listed:
the photo's edges the model lacks, components out of place, parts the photo does not show, and every small detail. Its
results drive the I steps; **no I step is done until its photos pass** (§2.4). The measurement pipeline is unchanged.

### 2.1 The holistic layer

| Step | What | Done when |
|---|---|---|
| **H0. The register** (done) | Seeded from the cabin data (its panels, shell parts and blocks), Fig. 3's plan and sections (25 items with pixel boxes; calibrated at 4.86 mm a pixel, ±3-6 cm against the cabin's measured parts; 3 conflicts recorded) and the book's named items | `holistic.py check` clean; `tools/tests/test_holistic.py` |
| **H1. Every interior photo observed** (done, first pass) | Each of the 76 photos (g082-g101) looked at as it is: every visible component classified (55 classes in 7 groups) with its image box, zone, mount, relations to its neighbours, size cue and confidence; the doubts written down | one observation file per photo in `tools/traces/osa_interior/observations/`; `check` clean |
| **H2. Fused and drawn** (done, first pass) | Observations tied to the register by id; proposals seen with medium confidence or better become entries (the late Czech vehicle of g101 gives evidence only, never new entries); every unmeasured component's place estimated from its relations; the schematics drawn (plan over Fig. 3, both walls, front, rear, roof) and the 3-D blockout rendered from inside (`tools/blender/build_holistic_blockout.py`) | `out/recon/holistic/index.html`, `blockout_*.png`, the coverage report |
| **H3. A person's review** (done, first pass, 9 Nov 2026: `docs/OSA_INTERIOR_HOLISTIC.md` section 10; both R-123M moved to where the photos put them) | Read the observers' doubts and the contradictions, merge ids that are one thing (`same_as`), name the unknowns where a manual can, settle the plan conflicts by eye, mark what the photos cannot settle | the report's doubts each answered or carried to I1 |
| **H3b. The joint solve** (first pass, 9 Nov 2026: `docs/OSA_INTERIOR_HOLISTIC.md` section 16; 47 photos in one frame without the model, `camera_from: sfm`, floor and roof corrected from it; the cab not to scale; the aisle's width, 1.34 m in the cabin against 1.75-1.85 m in the photos, awaits a decision) | Solve the book's photos against each other, not against the cabin; the reloader's and the Czech launcher's plates excluded | every wide photo gets a camera | `tools/recon/holistic_sfm.py` |
| **H4. Places from the solved cameras** | Once I1 solves the wide photos' cameras, each observed image box is cast onto the wall, floor or roof it is on: estimates (±15-40 cm) become measured places (±2-5 cm) | every component seen in a solved photo has `box_m.from = solve:<photo>` |
| **H5. The register checks the cabin** | `holistic.py check --cabin`: every relation the photos agree on holds in the built cabin; every component with status ≥ `placed` is in the cabin data; every cabin part is in the register | run in `tools/verify.ps1` |
| **H6. Views** (pilot g092-a done; g094-a posed on ten console points, 1.4 % rms; g082-b, g089-d, g096-a and g100-a `pending` until I1, I4 and I5 give their zones nameable, self-consistent parts, 9 Nov 2026) | A camera for each photo, in the body frame: from a face the measurement pipeline traced (`face`), or from six or more named cabin points clicked in the photo (`anchors`, with each anchor's left-out residual); the pipeline's `room`/`ba` cameras replace them once built | `tools/traces/osa_interior/views/<photo>.json`; `holistic_view.py check` clean; the camera's reprojection of its own face within 1 % of the width |
| **H7. The look-alike check** (g092-a: 70 % of the model's edges supported, 51 % of the photo's explained, with the operators' seats folded as photographed; g094-a: 49 % and 31 %) | `holistic_view.py render` (Blender: the cabin from the photo's camera, a look pass and a part-id pass) and `compare`: edges both ways, components by their parts in the id pass, the model's parts nobody named, and the photo's details by status. Results are written to `sheet.jpg`, `lacking.jpg`, `compare.json` and the report `docs/research/OSA_INTERIOR_LOOKALIKE_2026-11.md` | a photo passes (§1's Proof row; `docs/OSA_INTERIOR_HOLISTIC.md` §11) |
| **H8. Detail inventories** (done for the six wide photos, 9 Nov 2026: 541 details, 401 absent, 109 blocked, 27 modelled, 4 left to a later fit) | Every small thing each photo shows (fastener, seam, edge, rib, opening, hinge, latch, handle, label, clip, grommet, gasket, pattern, profile, surface, small part), each with its parent component, place in the image, relief, size cue, how it is built (mesh, normal map, decal, texture) and status | `tools/traces/osa_interior/details/<photo>.json` for every photo of a zone before its I step starts; `check` clean |
| **H9. Details cast and handed on** (g092-a and g094-a cast; the four pending views' details wait for their cameras) | Each detail's pixel is cast as a ray into the built cabin, giving its surface and part, or its parent's register box, or a ray to be crossed by a second photo (H4) | `details_3d.json` per photo; every detail of a built zone has a 3-D place or a reason |

### 2.2 How each family is built

| Family (classes) | How it is built | Accuracy | Step |
|---|---|---|---|
| Shell (`hull_wall`, `floor`, `roof`, `bulkhead`, `frame`, `lining`, `sponson`, `hatch`, `window`, `door`, `pylon`) | Fig. 3's plan and the wide photos' solved cameras; the exterior hull (X2) outside it. **Lofted from cross-sections, not boxes**: the profiles that the detail inventories record (the roof's box-section cornice, the upper side plates sloping inboard, the wheel-arch housings, steps in the floor), cast from two or more photos | ±10 mm | I1 |
| Racks and blocks (`rack`, `block`, `console`) | the face trace, one square-on photo per rack and a second view for heights, as SP-2M2 (done: 18 blocks, 507 parts, ±0.4 mm edges, ±3 mm heights) | ±3 mm outlines, ±1-2 mm parts | I2, I3, I4 |
| Free-standing equipment (`box`, `radio`, `intercom`, `phone`, `instrument`, `fan`, `heater`, `filter`, `motor`, `power`, `periscope`) | a small trace (`model-from-photos`) when a photo shows a face square-on; otherwise a sized box from the register until one does | ±3-10 mm | I4, I5, I7 |
| Crew (`seat`, `bench`, `cushion`, `belt`, `footrest`, `curtain`, `handle`, `step`) | the seats' drawings and photos, checked against the crew bodies (`crew_pose_dump`) | ±10 mm | I5 |
| Fittings (`bracket`, `shelf`, `mount`, `clamp`, `hook`, `strap`, `cover`, `rail`, `plate`) | placed from the solved photos (H4), shaped by class rules (a bracket an angle of the observed size, a plate a sheet with its text where legible) | ±10-20 mm | I7 |
| Runs (`cable`, `loom`, `conduit`, `hose`, `duct`, `pipe`) | a path of points cast from two or more solved photos, swept with the observed diameter; a run seen in one photo stays on its wall at the observed height | ±20 mm | I7 |
| Lights and controls off the faces (`lamp`, `switch`, `connector`, `fuse_panel`, `control`) | traced where a photo shows them square-on; working ones get controls in `data/cabins/` and procedures | ±3 mm | I5, I7 |
| Kit (`kit`) | placed from the photos; the 1980s kit only (no Czech or museum additions) | ±20 mm | I7 |
| `unknown` | not built until H3 or a source names it | | H3 |
| Details on any of the above (the detail kinds of H8) | built with their parent, by the build rule: geometry for anything that makes the silhouette or has relief of 2 mm or more; a normal map from the kit's geometry for 0.3-2 mm; decals for plates, tables and stencils (flattened from the photo, text only where read and confirmed); textures for paint and wear. Parametric parts from the detail kit, instanced in Unreal | ±1-3 mm on a traced face, otherwise as their parent's place | I10, with each I step |

### 2.3 Steps

| Step | What | Inputs | Done when |
|---|---|---|---|
| **I0. The book's interior as references** | The per-component reference sheet: now generated from the observations (the coverage report's tables: every component and every photo of it), plus the readable plates | H1-H2, notes 081-101 | the console map takes the new plates and settles ОС81-9М |
| **I1. The compartment, re-measured** (two passes, 9 Nov 2026: the roof lofted from the photos' section over the compartment and cab, the box beam, the turret's opening as a stand-in, the rear bulkhead and its high door, the floor's housings, the plan conflicts settled: `data/cabins/osa_ak_shell.json`, `docs/OSA_INTERIOR_HOLISTIC.md` §15; g096-a still `pending` on SL-3M's and the aft racks' size, then H4; the pass marks not met) | Re-fit the shell from Fig. 3 and the wide photos: solve each wide photo's camera against the current cabin, then move what disagrees; H4 then places what those photos show. The inventories' shell (§13.1 of the spec): the roof is nowhere flat (the ledge sloping onto the racks, the box beam and the turret base's opening, the padded ledge over the console, the cab hatch's coaming), the lining light grey-green, the wheel-arch housings, the bulkhead's fairing, rib and ledge | Fig. 3, g082-b, g089-d, g092-a, g094-a, g096-a, g100-a, USAF -019; their views and detail inventories (H6, H8) | every wide photo's overlay within 10 mm at its edges; the plan conflicts (left R-123M, 0096-9M, TA-57) settled; the shell lofted from the cast profiles (cornice, sloped upper walls, wheel arches); the wide photos pass the look-alike check's edge measures for the shell |
| **I2. Main console SŁ-2M2 to the millimetre** | The three columns stay on `samsim_panels`, but run full height: g094-a shows the top and bottom blocks as whole blocks (a third and a half of the PPI block's height), where the cabin's 0.98 m faces have them as strips. Add the rack frame (top rail, chamfer, proud channel posts, foot rail) and its plate, the eight-dial strip, the side box, the range column's plates, cable entries | g094-a/b/c, g095-a/b | each column's overlay within 2 mm; the dial strip's dials live |
| **I3. The racks** | **SP-2M2 done** (8 Nov 2026), **SP-3M-2 done** (9 Nov 2026: `docs/OSA_INTERIOR_HOLISTIC.md` section 18), **SŁ-3M done** (9 Nov 2026: section 19), **SŁ-1M and SP-1M done** (9 Nov 2026: sections 20-21; lowered to their photographed height, tops at 1.65 m, section 22). **Frames, first pass (9 Nov 2026, section 23):** SŁ-2M's top rail and posts, SP-2M's uprights, the brow, pipe and cable over the front racks, no doors under them. Then each rack's frame and surround from the look-alike check: the channel uprights standing proud on the racks still without them, the plates' rounded corners, the cap chains and seal wires, the legends as decals, the ivory enamel, the cables and looms at its sides (g092-a found all of these missing on SP-2M2) | the trace queue; the racks' photos' detail inventories | `cabin_rack_fill.py` retired; every block `face_traced`; every photo of the rack passes the look-alike check |
| **I4. Power and rear** (first pass, 9 Nov 2026: the door's fittings, hood, blower, cover, the 0096-9M's body, GA-3, lamp guard, turret base and boot: `docs/OSA_INTERIOR_HOLISTIC.md` section 17; the cabinet square-on and its face layout open) | The 0096-9M cabinet square to the bulkhead, its face the whole front under the lid (g096-a; the cabin turns it 38° and draws a half-height plate), with its plugs, GA-3 with its pedestal and guard, the rear bulkhead's door (about 1.35 times as high as wide, hinged on its outboard side over a sill bead, in a pressed frame: g100-a; seal, latches, lever plate, warning plate), the blue lamp by the right corner, the hood over the blower and the outlets, the low flap, the 0096-10M plugs, the floor plinths and trays at the rear, the turret's underside (skirt, ring seal, drive, switch box, manual rod, stops, the hanging drum once named) | g096-a, g097-a..d, g099-a/d, g100-a..d | g096-a, g100-a and the rear's other photos pass the look-alike check; the cabinet's switches the procedures use sit on their traced places |
| **I5. The cab** | **The seats rebuilt (9 Nov 2026, section 23); the raised area under them, the footwell's mat and toe slope, the wheel housings and the gear lever's bracket built (section 24). Heights re-placed in stage S (section 26). Before that the cab was thought to keep its height, with the compartment behind it about 0.36 m too low against it, which waits for X2.** From g082-b's and g089-d's inventories: hanging visors on ball pivots, the white dash and its toggle row, the box under the right R-123M on its step, the roof's opening and coaming, the pillars and header with their looms. Driver's station and ПУ-303, the commander's dash, ПРОХОД ПОД МОСТОМ board, DP-3B, A-1 box, heater, fuse panel, both R-123M with БП-26 on their mounts and the ЗИП box, seats and their folding, the curtain and rail, the hatches, periscope blanks, visors, wiper motor, the blue lamp | g082-g091 | g082-b, g083-a and the cab's other photos pass the look-alike check |
| **I6. Materials and wear** | Measured on descreened crops against the white labels, NVA/Soviet 1980s paint only | | a material table with sources; side-by-side renders |
| **I7. Fittings, runs and kit** | Every register entry of the fittings, runs and kit families: brackets, shelves, clamps, plates, the looms along the racks, roof and bulkhead, hoses and pipes, connectors, belts, cushions, headsets, keys on their posts, first-aid box, fire bottles; each from H4's places, none invented | the register; H9's cast details | every such entry `placed` or better, or `missing` with its reason in the report |
| **I10. The detail kit** (with I3, before the second rack) | `tools/blender/detail_kit.py`: parametric parts for every detail kind built as a mesh, including screw, rivet and bolt heads, weld beads, rolled lips, channel, angle and tube sections, hinges, latches, handles, clips, ties and ball chains, grommets, louvres, perforated plate, quilting and cloth folds. Each is sized from its detail's size cue and the traces' measured sizes (SP-2M2: screws 6.2 mm, lamps 10.8, jacks 20.1, locks 27) and placed from `details_3d.json` along its surface's normal. A shared trim sheet carries the normal maps of what is too fine for geometry. Decals are flattened from the photos and vectorised, with text only where read | H8, H9; the traced faces' measured parts | every detail kind of H8 has a kit part, a normal map or a decal rule; the kit's parts are instanced in Unreal (one mesh, many transforms); the pilot's 29 absent details on g092-a are built or carry a reason |
| **I8. In Unreal** | Re-import, lighting and CRTs, crew bodies against the new seats and racks, the helper and lessons on the right controls; the blockout's views repeated with `tools/ue_cabin_shots.py` | | shots from each view's camera compared with the book's photos by the same look-alike check (Unreal's render in place of Blender's); H5 and the cabin tests clean |
| **I9. The BM3 cabin (optional)** | The same register with the BM3-1's faces and the ПКР toggle | I0-I8 | an AKM mission can use it |

**The pipeline:** `docs/OSA_INTERIOR_PIPELINE.md` (stages, algorithms, tools, the scale chain, how a face is traced);
`tools/recon/osa_interior.py` runs it; `tools/recon/holistic.py` runs the holistic layer. The pipeline's §10 maps each
stage to these steps.

**Order:** H6-H8 on the wide photos (done 9 Nov 2026: two posed, four pending, six inventories; findings in
`docs/OSA_INTERIOR_HOLISTIC.md` §13.1) → I1 (with H4, the shell from the cast profiles; then the pending views are
posed) → the rest of H3's open questions → I3 (SP-2M2's surround first, then SŁ-1M, SP-1M,
SP-3M-2, SŁ-3M) with I10 and I2 → I4 → I5 → I7 → I6 → I8. Once I1 has fixed the compartment, I3, I4 and I5 can run in
parallel sessions: they touch different panels and register zones (the walls, the rear, the cab). Each starts from the
trace queue in the coverage report and its photos' look-alike results.

### 2.4 The loop: every zone until its photos pass

Each I step runs the same loop over its zone. The holistic layer's spec (`docs/OSA_INTERIOR_HOLISTIC.md` §11-14) has the
detail; in short:

1. **Views and inventories first (H6, H8).** Before building a zone, every photo of it gets a camera and a detail
   inventory: what is there, down to the screws, chains and seams.
2. **Build (the I step, with I10).** Each component is built from the register and the measurement pipeline's traces.
   Each detail is built from its cast place (H9), by the build rule.
3. **Render and compare (H7).** Every photo of the zone is rendered from its camera and compared:
   - the photo's edges the model lacks (`lacking.jpg`, by tile);
   - components off;
   - parts the photo does not show;
   - details still absent.
   - **by depth and surface direction, per pixel (`tools/recon/holistic_diff.py`, §32 of the holistic spec)**: the conclusions in words and metres (which part, turned, moved, raised, missing, excess), the shared floor, roof and wall maps, and the score each edit has to raise.
4. **Fix and repeat.** Each difference goes back into the register's tables, an observation, an inventory or a builder.
   The traces themselves are changed only through the measurement pipeline: its trace queue takes the zones the check
   leaves unexplained.

A zone is done when every photo of it passes, and every face on it passes the pipeline's own overlay.

**What would make it more accurate than the book allows** (record, not a blocker): a visit to a preserved vehicle with a
tape measure and a phone (Lešany, Dresden, Koblenz, Polish museums: §8 of the exterior plan): the rack widths, the
bench, the cab's dash and the hatch, measured. The Bundesarchiv A 327/1/153 and /154 (`barch_a327`, `barch_a327_153`)
would give the NVA's own drawings of the compartment.

## 3. Exterior roadmap

The pipeline is planned and its first stages run (`docs/OSA_EXTERIOR_RECONSTRUCTION.md`): 450 photos catalogued and cut,
the dimensions sourced with their conflicts. Steps (the plan's R1-R5, renumbered here):

| Step | What | Done when |
|---|---|---|
| **X0. Catalogue checkpoint** | A person's pass over the catalogue: one key per physical vehicle and session (the readers' free text gives 129 groups from 450 photos), the 19 whole-vehicle photos confirmed | `groups.json` names five to eight vehicles; every solve candidate has an azimuth |
| **X1. Cameras** | Masks and camera solves for the whole-vehicle photos against the reference mesh; the focal ambiguity fixed by hub rows; the axle-spacing, hull-height and march-height conflicts decided from g011-a (near-orthographic left side) | every candidate at IoU ≥ 0.85 or with keypoints; the three conflicts settled with a residual |
| **X2. The plate hull** | The hull as welded plates back-projected from the solved photos and the square-on close-ups (nose g020-d, arches g038-a/c, rear) | every plate from two photos; the interior's shell inside it (checked against I1) |
| **X3. Parts** | Wheels (parametric, 1200×500-508, two treads), breakwater, light boxes (early, not the Czech late form), hatches (the crew hatch's D-bulge), grilles and thermal shield (early and late variants), water-jet covers, the turret and its antennas (front plate g050-c, the search antenna's elevations g068-a/g069-b, capture antennas, horns, TV sight), the containers (3 235 × 340 × 397 mm, 22.3°) | each part's size cited; overlays on its close-ups |
| **X4. Build and Unreal** | The game mesh with pivots (turret yaw, antenna and launcher pitch, hatches, steering), LODs, materials (Soviet 4BO or NVA olive) | overlays pass on every solve photo; the interior and exterior meet at the hatch and windows |
| **X5. The 9T217BM2** | The reload vehicle from g102-g122 with the same pipeline | its model replaces the placeholder TZM |

## 4. How the two meet

**Stage S, the shell first (9 Nov 2026; `docs/OSA_INTERIOR_HOLISTIC.md` sections 25-26; S0-S3 run in a first pass, the deck's aft edge left open; the aisle stays 1.34 m).** The interior's heights hang on estimates, so placing parts in it moves the error about. Until S3 is done, no more component placement or detail work in the cab or on the racks' heights.

| Stage | Work | Done when |
|---|---|---|
| **S0. The datum** | From the exterior: the hull's section, its roof crown, belly, windows and windscreen above the road (X1, X2's first pass: profiles, head-on and raised views, the 1.2 m wheel as yardstick) | the aisle floor's height above the road settled, the conflict in section 25 resolved with a residual |
| **S1. The frame** | One similarity for the joint solve from several yardsticks instead of one face; the cab group tied to the compartment; widths (the aisle, about 1.8 m) | every yardstick within 3 %, the cab's depth to scale |
| **S2. The shell** | Floor with the footwell, walls, roof, windows, wheel arches, as measured sections, against the exterior | wide photos from every direction overlay the empty shell |
| **S3. Blocks into it** | Every existing part re-placed by its measured height and place, as they stand: racks, shelves, seats, dash, wheel, pedals, radios; the crew's eyes and reach | the wide photos overlay the blocks; crew and helper checks pass |
| **S4. Back to detail** | The I and H stages resume | |

The interior's compartment (I1) and the exterior's plate hull (X2) are one vehicle: the lining sits inside the plates by
their thickness; the windscreens, quarter windows and the roof hatch are the same parts seen from both sides; the turret
pylon inside is the turret's base outside. Until X2 exists the interior keeps its October shell; when it lands, I1's
shell is checked against it and moved if they disagree (the photos decide).

## 5. Model and effort

| Work | Model |
|---|---|
| I0 reference sheet, plate reading, I3/I5 tracing face by face | Opus 5.5 (reading the photos is the hard part); bulk data entry from a finished trace: Sonnet 5.5 |
| I1 camera solves inside the cabin, X1, X2 | Opus 5.5 or Fable 5.1 |
| H8 detail inventories (reading every small thing in a photo against the render) and judging the look-alike sheets | Opus 5.5 |
| Blender scripts, the detail kit's parts (I10), materials, Unreal import, tests | Sonnet 5.5 |

Effort: the interior is roughly two to three times the exterior's work, by the number of faces to trace (about 60 blocks,
the cab's panels and the racks) against the hull's plates and a dozen parts.
