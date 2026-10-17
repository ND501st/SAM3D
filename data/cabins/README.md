# Cabin layouts

One JSON file per system: where its operator stations, panels, controls and lamps sit in the vehicle, for the
3D cabin. `pu12m.json` (the PU-12M command post) is written by `tools/pu12m_cabin.py` from the interior's trace
(`tools/blender/pu12m_interior_trace.py`): change those and run it again, not the file. Loaded by `simcore::LoadCabinFile` (`simcore/include/simcore/cabin.hpp`) against the system's console
file (`data/procedures/<system>.json`), which still decides what every control does. The loader rejects unknown
keys and bad references, and reports the path to the problem (e.g. `controls[3].panel: unknown panel 'x'`).

Layouts are presentation data. Positions don't need a source, but say in `status` where they come from (the
current Osa layout is a SAM3D placeholder). The real vehicle has the driver and commander in the front cab and an
operators' cabin behind it with three working seats (`zbiam_2019`, `flak11`); the layout gets a fifth seat with
roadmap 6.1. **Lamps** say what the system shows the crew, so each lamp needs a
`source` and `confidence` like the console data (`data/systems/README.md`).

## Frame

Metres in the vehicle's body frame: **x right, y forward, z up**, origin on the ground under the vehicle's
reference point. Angles are in degrees in the file.

- A **station** is a seat: `eye_m` is the eye point, `yaw_deg` the view heading (clockwise from forward),
  `pitch_deg` the view pitch (up positive). Its `id` is the `station` the console's controls use.
- A **panel** is a flat rectangle of `size_m` [width, height] around `centre_m`. `facing_deg` is the heading its
  front looks towards, clockwise from forward: an operator looking forward faces a panel with `facing_deg` 180.
  `tilt_deg` leans it back from vertical (90 = a desk).
- Items sit at `at_m` [u, v] from the panel's centre: u to the operator's right, v up the face. They must be inside
  the panel.

## Format

```jsonc
{
  "system_id": "osa_ak",              // must match the console file
  "status": "free text", "notes": "free text",
  "stations": [ { "id": "senior_operator", "label": "Senior operator", "eye_m": [-0.35, 2.4, 1.5],
                  "yaw_deg": -90, "pitch_deg": -10 } ],      // one seat per console station
  "panels": [ { "id": "so_panel", "label": "...", "station": "senior_operator", "centre_m": [-1.0, 2.4, 1.4],
                "facing_deg": 90, "tilt_deg": 15, "size_m": [0.95, 0.9] } ],
  "controls": [
    { "control": "soc.range", "panel": "so_panel", "at_m": [-0.15, 0.06] },
    // optional: "style" (toggle, rotary, key, button, wheel, cover); wheels: "units_per_turn";
    // toggles and rotaries: "red_dot", the position a red dot printed on the panel marks
    { "control": "ssc.az_wheel", "panel": "so_panel", "at_m": [-0.1, -0.3], "units_per_turn": 60 }
  ],
  "lamps": [
    { "id": "in_zone", "label": "ЦЕЛЬ В ЗОНЕ", "panel": "fo_panel", "at_m": [0.42, 0.2], "color": "green",
      "lit_when": [ { "input": "target_in_zone", "is": true } ],     // all must hold
      // optional "lit_when_any": [ ... ]                             // at least one must also hold
      // optional "flash_when": [ ... ]                                // while all hold (and the lamp is lit) it flashes
      "source": "samsim_osa p29", "confidence": "high" }
  ]
}
```

## Blocks, fixtures and looks

So the cabin resembles the real consoles, a layout can carry what the sim doesn't model, and say what every item looks
like. The Osa's operator columns are copied from SAM Simulator's panel photos at 1 mm per pixel (`samsim_panels`),
checked with `tools/cabin_preview.py` (draws a panel from this file alone, optionally beside and blended over a
reference picture kept in `reference/`).

- `blocks`: the instrument blocks of a column, `{ "id", "panel", "label" (type plate, e.g. "ОС81-9М"), "at_m",
  "size_m": [w, h], "color" }`, inside their panel. `color` (a look colour) paints the plate, e.g. the black PU-303;
  without it the plate is the consoles' cream.
- `fixtures`: items drawn but not operated, `{ "panel", "at_m", "kind", "size_m", "color", "ring", "label" }`.
- A panel's optional `color` (a look colour) paints a panel without blocks, e.g. the PU-12M's `bluegrey` ASPD-U blocks
  and `lightgrey` IT-45; without it the panel is the consoles' cream, darkened.
- `look` on a control or lamp placement: `{ "kind", "size_m", "color", "ring", "label" }`, all optional.

`kind`: `knob`, `pointer_knob`, `trim`, `toggle`, `lever`, `lamp`, `lamp_button` (cap `color` in a `ring`), `button`,
`jack`, `socket`, `latch`, `nameplate` (needs `label`), `bezel`, `window`, `diagram`, `oval`, `legend` (a
legend lamp: black glass with its `label` on it, the letters lit in the lamp's colour; needs `label`; round, or
rectangular for `[width, height]`), `gauge` (an instrument the console doesn't model: dark rim, face in `color`,
white by default, and a needle; round, or rectangular for `[width, height]`). `size_m` is a diameter
or `[width, height]`. Colours: black, white, grey, chrome, brass, red, green, amber, yellow, orange.

## Shell

`shell` is the room around the consoles: the modelled compartment, furniture and equipment, plus the interior lamps.
Each part is `{ "id", "label", "kind", "centre_m", "size_m": [x, y, z], "color", "source", "confidence" }`. `kind` is
`mesh` (a mesh of `art/cabin_interior`, built by `tools/blender/build_cabin_interior.py`: `"mesh": "SM_..."`, its
origin the centre of its bounding box, so `centre_m` is that centre and `size_m` the box; optional `"yaw_deg"`,
clockwise from forward; no `color`, the mesh has its own materials), `box`, `glass` (a window) or `light` (an interior
lamp reaching as far as `size_m`; optional `"lumens"`, and `"color": "blue"` for a blackout lamp). `color` takes the
look colours above, plus `paint` (the cabin's grey-green, the default) and `floor`. After rebuilding the meshes,
`python tools/cabin_mesh_sync.py` brings sizes (and the centres of meshes built in place) in line with
`art/cabin_interior/manifest.json`; `tools/tests/test_cabin_interior.py` checks they agree. In Unreal the cabin is
lit only by its own lamps (a lighting channel of its own), so the sun outside never reaches the panels.

## Displays

The instruments the crew reads. Each sits on a panel (`at_m` is its centre, `size_m` its width and height, all of it
inside the panel), says when it is powered (`on_when`, `on_when_any`, console conditions; empty = always) and needs a
`source` and `confidence`.

The search indicator (`ppi`), the elevation-search indicator (`elevation_scope`), the IT-45 (`aspd_scope`) and `dial`s
are round tubes or dials (a disc `size_m` across); the others are rectangular.

| `kind` | Shows | Extra keys |
|---|---|---|
| `ppi` | The search radar's picture on a rotating sweep with fading echoes, on the scale the range selector picks; the tracking radar's azimuth line and range gate | `sweep_period_s`, `afterglow_s` |
| `range_scope` | The tracking radar's range indicator: an upper sweep over its whole range with echoes as spikes, a lower sweep magnified around the range gate, and the range counter | `magnified_window_m` |
| `elevation_scope` | The tracking radar's elevation-search indicator: a rectangular raster (elevation up, range across) for the search beam the antenna searches, `range_window_m` of range centred on the range gate, the antenna's elevation as a line, and targets as fading marks when the antenna's beam passes them. The beams, the scan and the antenna's position are the console's (`sensors.track.elevation_search`, run by the engagement model); the layout gives one raster per beam, `{ "screen": [bottom, top] }` with screen heights 0..1 | `rasters`, `range_window_m`, `afterglow_s` |
| `tv` | The TV sight's picture, looking along the tracking radar's azimuth and elevation with the selected lens, and a crosshair | none |
| `dial` | A needle turning with a control's value (one turn = `value_per_turn`), scale `0..labels_per_turn` in `marks` divisions; `lit_when` for its backlight | `control`, `marks`, `value_per_turn`, `labels_per_turn` |
| `meter` | A needle swinging with `value x scale`, full deflection at `full_scale` `unit`; rests at 0 when not powered | `control`, `marks`, `scale`, `full_scale`, `unit` |
| `aspd_scope` | The command post's IT-45 (console `post` block, `simcore/post_feed.hpp`): an amber long-afterglow tube whose sweep turns once in `sweep_period_s` and lights the range marks and a cabled radar's echoes, which fade in `afterglow_s`; over it the ASPD-U's symbols on the scale the console's scale switch picks (number, height-zone mark, speed vector), the DT-2's marker and the marked target's time of arrival; with `glass`, the grid glass in front | `glass`, `sweep_period_s`, `afterglow_s` |
| `status_board` | The command post's IT-1M / IT-2M: a light plate, the groups КОМПЛЕКТ, РАБОТА and ТЕХ. СОСТОЯНИЕ over a row of amber lamps per launcher, lit as the post heard it on the battery net (`simcore::BoardLampsOf`) | none |
| `plot_board` | The plotter's air-situation map: etched plexiglass (squares, range circles) with the board's marks in grease pencil, each target's track, number and height over its code | none |
| `keypad_window` | The ДХ keypad's window (needs the console's post `keypad`): the number it is on, ЦУ flashing while it waits for the launcher's place, the last designation, as vacuum-fluorescent digits | none |

```jsonc
{ "id": "beta_c", "label": "βс", "kind": "dial", "panel": "so_panel", "at_m": [0.3, 0.28], "size_m": [0.14, 0.14],
  "control": "ssc.az_wheel", "marks": 12, "value_per_turn": 360, "labels_per_turn": 60,
  "on_when": [ { "subsystem": "power", "is": "ready" } ], "lit_when": [ { "control": "ssc.az_light", "is": 1 } ],
  "source": "samsim_osa p16", "confidence": "high" }
```

What the radars see comes from the engagement model (`EngagementModel::SearchPicture`, `TrackReturns`); a display
only shows it. `ppi`, `range_scope` and `tv` need the console's `sensors` block, `elevation_scope` its track `elevation_search`.

Rules the loader checks:

- When the console lists `stations`, the cabin has exactly one seat for each of them, and no others.
- Each console control is placed at most once, on a panel whose station matches the control's `station`.
- The default style follows the control: buttons and wheels by kind, a control that guards another is a `cover`,
  key switches are `key`, more than two positions is `rotary`, otherwise `toggle`. An explicit `style` must fit.
- A cover or lock that isn't placed itself is put over the control it guards.
- `red_dot` is only for toggles and rotaries, and must be one of the control's positions. The real panels have red
  dots beside some knobs and switches marking their working position for combat, which is often not where they
  start (РОД РАБОТЫ's dot is at БР; `docs/research/OSA_CONSOLE_MAP_2026-10.md` §12). Give one only where a picture
  shows the dot.
- Displays must fit inside their panel and only use the keys of their kind.
- Lamp conditions use the console's condition format (`data/procedures/README.md`); `color` is green, red, amber
  or white.

The tests check that the Osa layout places every control of the console.

## Looking at a layout without Unreal

`python tools/panel_sheet.py [system]` draws every panel flat and to scale, seat by seat, into
`out/panel_sheets/<system>.html`, with a table of each control's label, positions, start position, source and
confidence. It lists layout problems the loader doesn't catch (items that overlap or reach over a panel edge);
`--check` makes those fail the run. `--refs reference/panel_refs.json` (a `{"panel id": "photo path"}` file, kept
out of git) shows a reference photo under each panel for side-by-side comparison.
