# Cabin layouts

One JSON file per system: where its operator stations, panels, controls and lamps sit in the vehicle, for the
3D cabin. Loaded by `simcore::LoadCabinFile` (`simcore/include/simcore/cabin.hpp`) against the system's console
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
      "source": "samsim_osa p29", "confidence": "high" }
  ]
}
```

## Displays

The instruments the crew reads. Each sits on a panel (`at_m` is its centre, `size_m` its width and height, all of it
inside the panel), says when it is powered (`on_when`, `on_when_any`, console conditions; empty = always) and needs a
`source` and `confidence`.

| `kind` | Shows | Extra keys |
|---|---|---|
| `ppi` | The search radar's picture on a rotating sweep with fading echoes, on the scale the range selector picks; the tracking radar's azimuth line and range gate | `sweep_period_s`, `afterglow_s` |
| `range_scope` | The tracking radar's range indicator: an upper sweep over its whole range with echoes as spikes, a lower sweep magnified around the range gate, and the range counter | `magnified_window_m` |
| `tv` | The TV sight's picture, looking along the tracking radar's azimuth and elevation with the selected lens, and a crosshair | none |
| `dial` | A needle turning with a control's value (one turn = `value_per_turn`), scale `0..labels_per_turn` in `marks` divisions; `lit_when` for its backlight | `control`, `marks`, `value_per_turn`, `labels_per_turn` |
| `meter` | A needle swinging with `value x scale`, full deflection at `full_scale` `unit`; rests at 0 when not powered | `control`, `marks`, `scale`, `full_scale`, `unit` |

```jsonc
{ "id": "beta_c", "label": "βс", "kind": "dial", "panel": "so_panel", "at_m": [0.3, 0.28], "size_m": [0.14, 0.14],
  "control": "ssc.az_wheel", "marks": 12, "value_per_turn": 360, "labels_per_turn": 60,
  "on_when": [ { "subsystem": "power", "is": "ready" } ], "lit_when": [ { "control": "ssc.az_light", "is": 1 } ],
  "source": "samsim_osa p16", "confidence": "high" }
```

What the radars see comes from the engagement model (`EngagementModel::SearchPicture`, `TrackReturns`); a display
only shows it. `ppi`, `range_scope` and `tv` need the console's `sensors` block.

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
