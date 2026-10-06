# Osa-AK 3D interior: handover for the next model

*Written 5 Oct 2026 (Australia/Sydney) at the end of the session that modelled, textured and furnished the 9A33BM2
fighting compartment. Read this with `CLAUDE.md`, `docs/HANDOFF.md`, `data/cabins/README.md` and `unreal/README.md`.*

## 1. Where things stand

**Branch `feature/osa-interior-3d`**, stacked on PR #19 (`feature/osa-interior`); local commits only, nothing pushed.
Merge #19 first, then retarget this branch's PR to `main`.

| Area | State |
|---|---|
| Cabin layout (`data/cabins/osa_ak.json`) | Re-measured from `mon_2112_80` Fig. 3 (see §3). 12 panels incl. the right-wall racks SP-1M, SP-2M, SP-3M-2 with their blocks; 29 modelled shell parts with sources and confidence; 4 lamps |
| Meshes (`art/cabin_interior/`, 23) | Built by code in Blender; imported to `/Game/SAM3D/Cabin/Interior`. Hull is a closed solid (0 non-manifold edges) with 12 cm lining |
| Textures (`art/cabin_textures/`) | Baked face per panel (enamel, wear, every label, position legends round selectors, printed tables) and 4 tiling detail maps; imported to `/Game/SAM3D/Cabin/Textures` |
| Materials | `M_CabinPBR` (colour, roughness, metallic, grime masks, detail normal, optional `BaseTexture`), `M_CabinGlass`, `MI_Cabin_<slot>` per Blender material, `MI_Cabin_PanelFace` |
| Unreal C++ | Builds the `mesh` shell parts, maps panel faces onto blocks (runtime text labels skipped where a face exists), shadowing dome lamps with lumens/colour from data, exterior placeholder hidden in the cabin, `SetCabinViewOverride` for scripted shots, look clamp ±135° |
| simcore | Shell kind `mesh` (`mesh`, `yaw_deg`), lights `lumens` and `blue`; 104 tests pass |
| Python tools tests | 13 pass (incl. `tools/tests/test_cabin_interior.py`: layout ↔ manifest) |
| Lighting | **Fixed 6 Oct 2026 (§6):** the white-out was the cabin camera's fixed exposure (3 cd/m², four stops too bright for the dome lamps), not the sky light. `CabinExposureBrightness` is now 12 |
| Look vs photos (6 Oct) | Lining grey-green and rack frames dark grey (opisybroni 54334/54335); dash plates grey with black-faced gauges (rwd_mb3 9a33_31/32; blocks `d_dash`, `c_dash`); dead search/elevation tubes amber with filter wires, dead TV glass pale grey-green (`SimCabinDisplayComponent`, screen gain 6, TV 4); seat vinyls muted olive/dark brown |

Uncommitted on purpose: `unreal/SAM3D/Config/DefaultEngine.ini` (someone switched the project to SM6 and ray-tracing
mode Full in the editor; not this session's change; ask the user before committing) and `blender/SAM3D.blend`
(the user's own Blender file).

## 2. Which model for what

| Task | Model |
|---|---|
| Lighting fix (§6), any change to layout geometry from drawings, new research/source reading, renderer settings | Opus 5.5 (or Fable 5.1) |
| Adding/refining meshes in the Blender script, new materials, texture tweaks, C++ placement code, tests, docs | Sonnet 5.5 |
| Bulk data entry (more blocks, fixtures, nameplates) | Haiku 4.5 or Sonnet |

## 3. Sources and the measured plan

- **`mon_2112_80` Fig. 3** (Polish manual, plan + sections A-A right wall, B-B left wall; extracted to
  `reference/sources/AntiaircraftOSA-AK-9K32M2rocketkit_extract/fig_p028|fig_p029/full.png`). Scale from SŁ-2M = 1.89 m:
  **4.785 mm per pixel**. "SŁ" = szafa lewa (left), "SP" = szafa prawa (right). Front is left on p028's plan, right in
  p029's B-B section.
- **`usaf_shaw_2017`**: US Air Force photos 170120-F-DV125-019 (looking aft along the bench), -024 (launch-block
  close-up: worn grey-cream paint, black condensed lettering, bakelite bar knobs, chrome toggles, black lamp caps),
  -036 (roof hatch). **Public domain**, at media.defense.gov (view in a browser; direct download is blocked). Not yet
  listed in `docs/SOURCES.md`; add it.
- `opisybroni` museum photos (`reference/sources/photos/opisybroni/`, esp. `full-17362-54334/54335/54539`), `rwd_mb3`
  300 px photos (`reference/photos/rwd_mb3/`), `samsim_panels` (panel positions).

Body frame (m): x right, y forward, z up. Key numbers now in the data / Blender script:

| Item | Value |
|---|---|
| Floor / roof lining | z 0.62 / 2.10 (estimates; racks reach the roof per both sections) |
| Side walls | x ±1.32 at rack height (drawing −1.333 / +1.306); cab narrows from y 4.06 to ±1.14 at the bow |
| Left rack faces | SŁ-1M −0.725 (y 2.65–3.96), SŁ-2M −0.734 (0.605–2.505), SŁ-3M −0.670 (−0.842–0.488) |
| Right rack faces | SP-1M +0.787, SP-2M +0.612, SP-3M-2 +0.691 (same y spans) |
| Bench | x −0.541 to −0.144, y 0.682–2.424, seat top z 1.05; operators' eyes x −0.33, z 1.83 (0.40 m from the console) |
| Cab | seats centre y 3.866 (driver x −0.36, commander x +0.33), eyes y 3.72; wheel centre (−0.36, 4.33, 1.40) |
| Rear bulkhead | y −0.936, door x 0.03–0.59 |
| Equipment (plan) | R-123M ×2, TA-57, ZCzZ1A boxes nr 1/2, GA-3, CGW-10P, AWO-5M1, 0096-9M (diagonal, facing −38°) |

Unidentified/unknown parts are deliberately plain and marked low confidence: CGW-10P, AWO-5M1, GA-3 housing, pedals,
the radios' front panels, the hatches' and door's exact sizes, the blue wall lamp's position.

## 4. The pipeline (run in this order)

```bash
"C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" --background --factory-startup --python tools/blender/build_cabin_interior.py -- --preview out/interior_preview
python tools/cabin_mesh_sync.py            # sizes/centres in the layout <- art/cabin_interior/manifest.json
python tools/cabin_rack_fill.py            # (re)generate the filler fixtures on the SP racks and gyro rack (§9)
python tools/cabin_textures.py             # panel faces + detail maps (re-run after any layout label change)
python tools/ue_import_cabin_interior.py   # textures, materials, meshes (steps can be named); Play must be stopped
python -m pytest tools -q && ctest --test-dir simcore/build -C Release
python tools/ue_cabin_shots.py out/cabin_shots   # Play + photographs from body-frame views
```

C++ changes: close the editor (`python tools/ue_remote.py "unreal.SystemLibrary.quit_editor()"`), then
`D:\UE_5.8\Engine\Build\BatchFiles\Build.bat SAM3DEditor Win64 Development -Project=C:\dev\SAM3D\unreal\SAM3D\SAM3D.uproject -WaitMutex -NoHotReload`, relaunch.
Run Blender headless, not through the Blender MCP session: the user's GUI Blender holds their own file.

Conventions inside the Blender script: geometry is written in body coordinates through `P()` (Blender X = forward,
Y = left), which Unreal's FBX import lands exactly in the cabin component's frame (verified: no mirroring). Every
mesh is exported around its bounding-box centre. Each slab's given plane is the inside face, `u × v` looks into the
cabin and thickness goes outwards. Material slots are named `MI_Cabin_<name>` so the import binds them by name.

## 5. Gotchas learned the hard way

- **Never import or build assets while Play is running.** It produced "Bad MeshDescription" and then crashed the
  editor in the Nanite builder. All tools that start Play leave it running; stop it first.
- Interchange FBX import creates material copies unless `import_materials` is off (it now is).
- `M_CabinPBR`'s grime sampler must be type **Masks** (texture is TC_MASKS) or the material fails and everything
  renders as a checkerboard.
- Don't weld vertices across solids in the Blender builder: touching solids then share four-face edges, and Unreal's
  distance-field builder dropped the whole roof from Lumen's view.
- **Exposure units:** `r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange = 0`, so the cabin camera's
  `AutoExposureMin/MaxBrightness` are luminance, not EV100 (now `CabinExposureBrightness` = 12). Setting
  `auto_exposure_bias` from Python without its override flag changes nothing; set the brightness instead.
- The sun and sky light live inside `CesiumSunSky_0` (components `DirectionalLight`, `SkyLight`), not as separate
  actors: `get_all_actors_of_class(world, unreal.SkyLight)` finds nothing.
- `simcore_tests` segfaults (rather than failing) if `data/cabins/osa_ak.json` doesn't load, e.g. a block with an empty
  `label`: `RequireString` rejects empty strings.
- The level's Cesium sun is at 13:00 **UTC−5**, which is dusk in Hungary: the pink light. Daytime is UTC+2 (CEST).
  That is a level setting; ask before changing it.
- Git Bash heredocs mangle backslashes and quotes in Python; write scripts with the file tool.

## 6. Daylight in the cabin: resolved (6 Oct 2026), history kept below

**Resolution:** with the sun and the sky light both set to 0 the cabin looked exactly as bright, so neither was the cause.
The lamps were being exposed at `AutoExposureMin/MaxBrightness` = 3 (cd/m²). Raising it to 12 (now
`ASimOsaVehicle::CabinExposureBrightness`; renamed from `CabinExposureEv100`, which was a misnomer) gives the
photographs' cream enamel, visible labels and real shadows, and the windows simply read bright. Because the unlit
screen material is dimmed by exposure too, `USimCabinDisplayComponent` multiplies it (tint gain 6, TV 4); the render
target stops at 1.0 so off-state glass colours are chosen against that gain. Tune the value in the editor first:
`cabin_camera.post_process_settings` via `tools/ue_remote.py -f` takes effect immediately in Play. Whether the sky
light still needs lowering with a midday sun has not been re-tested at the new exposure (the level's Cesium sun is at
dusk, UTC−5).

Original notes follow.

With a midday sun the interior washes out white with cool, blotchy light, even far from the windows. Findings:

- It is the **sky light**: with the SkyLight off (or the sun off, which darkens the captured sky) the cabin looks right;
  with the SkyLight at 3 % and the sun on, the operators' compartment looks right (warm lamps, faint blue bounce).
- Not caused by exposure, wall thickness or the GI method: switching the cabin camera to GI = None, scaling indirect
  lighting to 0.02 and turning on `r.Lumen.TraceMeshSDFs` changed nothing. The cabin parts sit on lighting channel 1
  only (the sun on channel 0), which is intended.
- Lumen runs in software (`r.RayTracing=0` at the time); mesh distance fields for the cabin exist (visualised).

Options, to discuss with the user:
1. **Quick:** while the camera is in the cabin, scale the world's SkyLight intensity down (≈0.03) and restore it on
   leaving (`ASimOsaVehicle::SetInCabin`); give the cabin camera auto exposure in luminance units (e.g. min 3,
   max ~300) so looking out of the windscreen adapts. The TV sight's scene capture would also see the dimmer sky.
2. **Proper:** hardware-ray-traced Lumen (SM6 + ray tracing; the uncommitted `DefaultEngine.ini` already points
   there), then re-test occlusion; disable Nanite on the hull/racks if the RT fallback meshes lose thin walls.

The user said not to spend more time on lighting for now: keep lamps, colours and visibility as they are.

## 7. Next work, in priority order

1. Lighting decision (§6), then tune lamps (250 lm dome lamps; per-seat 40 lm fills in `SimCabinComponent::Build`),
   reduce wall grime (`Paint_Interior` GrimeAmount 0.35 looks heavy on the ceiling), check the brown seat cushions.
2. Make the Blender `--preview` place reusable meshes from the layout's shell parts instead of `PREVIEW_PLACES`.
3. Parts library (`tools/blender/build_cabin_parts.py`): Soviet bar ("chicken-head") pointer knobs, toggles with
   knurled ring nuts, lamp caps with faceted lenses, meter housings, block pull handles; the samsim/USAF photos show
   them clearly. Re-import with `tools/ue_import_cabin_parts.py` (Play stopped).
4. Instrument faces: the driver's and commander's gauges are blank discs (data has no dial faces); DP-3B is drawn on
   the commander panel as fixtures; the 0004-14M meters and 0062-6M meters are windows only.
5. Cables and looms along racks and roof (photos show braided cables, 9a33_45); gas-mask bags, headsets; only with
   a photo to place them.
6. (`usaf_shaw_2017` is now in `docs/SOURCES.md`.) Still to do: record the Fig. 3 measurements in `docs/research/OSA_MANUALS_2026-10.md`.
7. Unresolved data conflict: Fig. 3 draws the launch-prohibit lock ("Zamek zakazu startu") on the post between SŁ-1M
   and SŁ-2M; the data keeps the ЗАПРЕТ ПУСКА key on block 0004-11M (samsim_panels). The signal board on that post is
   not modelled.

## 8. Checking your work

- Tests: `ctest` (simcore) and `pytest tools` must pass; `python tools/panel_sheet.py --check` clean;
  `python tools/cabin_mesh_sync.py --check` exits 0.
- Look at it: `python tools/ue_cabin_shots.py out/shots` and read the PNGs; the editor log line
  `Cabin built: 12 panels (12 with baked faces) … 33 shell parts` and no `Cabin: mesh … isn't imported` warnings.
- `python tools/ue_cabin_check.py` (cabin tour) still works for the procedure side.

## 9. Added 6 Oct 2026 (second session): dials, rack content, parts

- **Dials:** round and rectangular `gauge` fixtures are printed into their panel's baked face
  (`Face.dial` in `tools/cabin_textures.py`: black or white face, scale, needle at rest, hub) and the cabin builds only
  a rim (`SM_GaugeBezel`, ring; `SM_MeterHousing` flange). The scale is generic (0–10): no source gives the markings,
  only that the vehicle's dash gauges are black with white print (rwd_mb3 9a33_31/32).
- **Rack filler (`tools/cabin_rack_fill.py`):** the right-wall racks (SP-1M, SP-2M, SP-3M-2) and the gyro rack had no
  controls in the data. The script writes ~300 fixtures tagged `notes: "rack fill …"` (bronze knobs over coax sockets,
  lamps over toggles, printed tables) in rows, clear of each unit's latches and nameplate, chosen from the block id.
  They show the kinds of parts rwd_mb3 9a33_41/45/46/51/52 show; **no position is measured and nothing in the
  procedures refers to them**. Rerunning replaces them; `--check` verifies the file is current. Do not hand-edit them.
- **Printed tables** are ruled grids with a dash per cell: what they say is not legible in any source.
- **Parts:** `SM_KnobPointer` is the bar ("chicken-head") pointer knob; toggles have a knurled ring nut; lamp lenses
  are faceted (10 sides); `SM_GaugeBezel`, `SM_MeterHousing` are open rings (`lathe(..., cap=False)`: without it the
  helper fans a disc across any ring's hole, which hid the dials).
- **Not done:** cables and looms, gas-mask bags and headsets (rwd_mb3 9a33_45/50 show braided hose and
  54335 shows bags on the seats, but no photo fixes where they lie in this layout, and bags on the seats would clip
  the crew who sit there).
- **Compass card (third pass):** gauge fixtures take an optional `scale` ("linear" default, "compass"); the gyro rack's
  black window on 0005-8M is now a compass-card gauge (a generic 0-35 card: the real one isn't legible). The three
  orientation toggles moved right of it (positions are estimates either way), a flush cover plate with a latch
  replaced the grey slab on OK81-4M-II, and 0009-5M has a second row of small red lamps. Still plain on purpose:
  CGW-10P, AWO-5M1, GA-3, the pedals and the radios' front panels (no source for them).
- **Cold start:** after an editor restart the panel textures stream in slowly; for shots run
  `r.Streaming.FullyLoadUsedTextures 1` and wait ~25 s or the dials look blurred.
- **Tool gotcha:** `tools/ue_cabin_shots.py` leaves Play running and `ue_import_*` then refuses with "stop Play first";
  stop Play (`editor_request_end_play`) before each import. An editor start can also crash once in Niagara's type
  registry (engine assertion, unrelated); a second launch worked.

## 10. Fourth pass (6 Oct 2026, branch `feature/osa-interior-refine`)

- Paint normals on the lining and racks cut (strength 0.12, 3 per metre): the ceiling looked like popcorn.
- `SM_CabLevers` (built in place): gear lever with ball knob and handbrake between the cab seats, from rwd_mb3
  9a33_13/31; positions and heights are estimates (confidence low). They are dark on dark in the dim cab.
- Hinge plates with pins on every rack's end posts (opisybroni 54334), two per end at 20% and 80% of the panel
  height; positions are estimates.
- Looked at and left alone: floor tread (the photos show plain dark plates with seams and a corrugated mat, which the
  cab footwell already has), the open white tray right of the commander's seat in 54335 (no plan position), seat bags.

