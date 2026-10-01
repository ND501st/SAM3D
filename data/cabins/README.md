# Cabin layouts

One JSON file per system: where its operator stations, panels, controls and lamps sit in the vehicle, for the
3D cabin. Loaded by `simcore::LoadCabinFile` (`simcore/include/simcore/cabin.hpp`) against the system's console
file (`data/procedures/<system>.json`), which still decides what every control does. The loader rejects unknown
keys and bad references, and reports the path to the problem (e.g. `controls[3].panel: unknown panel 'x'`).

Layouts are presentation data. Positions don't need a source, but say in `status` where they come from (the
current Osa layout is a SAM3D placeholder). **Lamps** say what the system shows the crew, so each lamp needs a
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
  "stations": [ { "id": "range_officer", "label": "Range officer", "eye_m": [-0.35, 1.9, 1.5],
                  "yaw_deg": -90, "pitch_deg": -10 } ],
  "panels": [ { "id": "ro_panel", "label": "...", "station": "range_officer", "centre_m": [-1.0, 1.9, 1.35],
                "facing_deg": 90, "tilt_deg": 15, "size_m": [1.0, 0.6] } ],
  "controls": [
    { "control": "soc.range", "panel": "ro_panel", "at_m": [-0.17, 0.2] },
    // optional: "style" (toggle, rotary, key, button, wheel, cover); wheels: "units_per_turn"
    { "control": "ssc.az_wheel", "panel": "ro_panel", "at_m": [-0.25, -0.17], "units_per_turn": 60 }
  ],
  "lamps": [
    { "id": "in_zone", "label": "ЦЕЛЬ В ЗОНЕ", "panel": "fo_panel", "at_m": [0.42, 0.2], "color": "green",
      "lit_when": [ { "input": "target_in_zone", "is": true } ],     // all must hold
      // optional "lit_when_any": [ ... ]                             // at least one must also hold
      "source": "samsim_osa p29", "confidence": "high" }
  ]
}
```

Rules the loader checks:

- Each console control is placed at most once, on a panel whose station matches the control's `station`.
- The default style follows the control: buttons and wheels by kind, a control that guards another is a `cover`,
  key switches are `key`, more than two positions is `rotary`, otherwise `toggle`. An explicit `style` must fit.
- A cover or lock that isn't placed itself is put over the control it guards.
- Lamp conditions use the console's condition format (`data/procedures/README.md`); `color` is green, red, amber
  or white.

The tests check that the Osa layout places every control of the console.
