# Electromagnetic-layer data

Libraries of radars, passive receivers, jammers, radios, stand-alone emitters and threat libraries,
loaded by `simcore/em/data.hpp` (`LoadEmLibraryFile`) and used by the EM layer
(`docs/EM_ARCHITECTURE.md`). One file per library:

| File | Provenance | Contents |
|---|---|---|
| `osa_ak.json` | `per_value` | 9K33M2 Osa-AK search and tracking radars, R-123M radio, NATO's view of the SA-8 (blue threat library). Every value cited; spec and open items in `docs/research/OSA_EM_SPEC_2026-10.md` |
| `nato_1985.json` | `per_value` | NATO-era fighter radar, RWR, ESM station and stand-off jammer classes, and each side's threat library about the other; band/year facts cited, the rest labelled estimates (`docs/research/EM_E1_NATO_DATA_2026-10.md`) |
| `generic.json` | `placeholder` | Round generic numbers for tests, the MVP demo and the viewer. **Not data for any real system** |

## Rules

1. **Real systems use `"provenance": "per_value"`**: every physical value is
   `{"value": …, "source": "<id from docs/SOURCES.md, page>", "confidence": "high|medium|low|estimate", "notes": "…"}`.
   The loader refuses a plain number in such a file and records every value, so tools can print the spec.
2. **Placeholders use `"provenance": "placeholder"`** with plain numbers and `"source": "PLACEHOLDER"`.
3. **Public and declassified sources only, operator-country first** (`docs/SOURCES.md`). Estimates are
   allowed when labelled `estimate` and, where possible, derived from published numbers (say how in `notes`).
4. **Radars need a `calibration` anchor**: the published range against a stated target
   (`range_m`, `rcs_m2`, `target_altitude_m`, `pd`, `source`, `confidence`). The loop gain is fitted to it.
   A `low_altitude_check` is reported by the tests, not fitted.
5. **Radios may have a `range_anchor`** (two stations at `antenna_height_m` over flat ground just reach
   `range_m`; optional `cosite_range_m` with the platform's radars on). System loss and co-site desense are fitted.
6. **Threat libraries are one side's belief**, not the truth: `owner` of each entry is the side the
   emitter belongs to; `platform` (`air`, `ground`, `ship`, `unknown`) sets how fast it can move; `cw`
   marks continuous-wave or noise emitters. Western "facts" pages are the right source for a NATO
   library — that is what NATO believed.
7. Units: SI (Hz, W, dBW, m, s); angles in degrees where the key ends in `_deg` (stored in radians).
8. Strict loading: unknown keys are errors with the path of the offending key.

## Sections

* `radars[]`: `id`, `name`, `role` (`search`/`track`), `emitter` {frequency, PRF, pulse width, peak power,
  antenna {gain, beamwidths, sidelobe}, `scan`, `scan_period_s`, `beams[]`, `agility`, `channels_hz`},
  noise figure, Pfa, calibration, ranges and elevations, accuracy floors, `mti`, `clutter`, `tracker`,
  `track_gate_m`, `max_slew_deg_s`, `low_target_elevation_deg`, `iff`, `eccm`.
* `esm[]`: `kind` (`rwr`, `esm`, `elint`, `tdoa_site`, `comms_intercept`), band, sensitivity (dBW), gain,
  bearing σ, measurement σ, TDOA timing σ, `threat_library`.
* `jammers[]`: `technique` (`barrage`, `spot`, `rgpo`), `emitter` (band = `freq_hz` ± `bandwidth_hz`/2),
  RGPO walk rate, maximum pull, silent time, J/S needed.
* `radios[]`: band, power, gain, antenna height, sensitivity, required SNR, bandwidth, voice timings,
  tracks per message, `range_anchor`.
* `emitters[]`: stand-alone emitters (an aircraft's radar seen only as a signal, decoy emitters).
* `threat_libraries[]`: `id`, `owner`, `entries[]` with band, PRF and pulse-width windows.
