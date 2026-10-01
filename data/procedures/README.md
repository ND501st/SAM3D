# Console data files

One JSON file per system: its operator controls, the equipment behind them, and its procedures. Loaded by
`simcore::LoadConsoleFile` (`simcore/include/simcore/console_data.hpp`), which rejects unknown keys, missing sources
and bad references, and reports the path to the problem (e.g. `controls[4].guard: unknown control 'x'`).

The rules in `data/systems/README.md` apply: public material only, best source tier first, and every item has a
`source` and a `confidence` (`high` / `medium` / `low` / `estimate`). Use `low` or `estimate` for anything inferred
rather than read in a source, and say why in `notes`.

**Planned change to the Osa file (roadmap 6.1, 6.3):** stations become the real five-man crew (commander, deputy
commander, senior operator, operator, driver) instead of SAM Simulator's three operator panels, and the missile
channels get their own lamps and missile containers. `docs/re/osa.md` → "Corrections from operators" has the
details.

## Format

```jsonc
{
  "system_id": "osa_ak",
  "display_name": "9K33M2 Osa-AK (SA-8B Gecko)",
  "status": "free text",
  "notes": "free text",
  "sources": [ { "id": "samsim_osa", "title": "...", "url": "...", "notes": "..." } ],

  // Optional: the push-button that launches, and the subsystem that must be ready for it to (used by the scenario runner).
  // "restarts" (optional): a subsystem each launch sends back to the start of its warm-up (the next missile's preparation).
  "launch": { "button": "fo.launch", "armed": "launch_armed", "restarts": "missile_prep", "source": "...", "confidence": "high" },

  // Yes/no signals the sim sets (target geometry etc.), so lamps and automatic sequences can use them.
  "inputs": [ { "id": "target_in_zone", "description": "...", "source": "...", "confidence": "high" } ],

  // In dependency order: a guard (cover or lock) before the control it guards.
  "controls": [
    { "id": "soc.hv_cover", "label": "...", "station": "range_officer", "kind": "switch",
      "position_labels": ["closed", "open"], "initial": 0, "source": "...", "confidence": "high" },
    { "id": "soc.hv", "label": "ВЫСОКОЕ", "kind": "switch", "guard": "soc.hv_cover",
      "position_labels": ["ВЫКЛ", "ВКЛ"], "source": "...", "confidence": "high" },
    { "id": "fo.key", "label": "ЗАПРЕТ ПУСКА", "kind": "key", "position_labels": ["no key", "inserted", "turned"], ... },
    { "id": "fo.launch", "label": "ПУСК", "kind": "button", ... },
    { "id": "ssc.az_wheel", "label": "АЗИМУТ", "kind": "wheel", "min": -330, "max": 330, "unit": "deg",
      "guard": "ssc.az_lock", ... }
  ],

  // In dependency order: a subsystem can only refer to subsystems above it.
  "subsystems": [
    { "id": "missile_prep", "name": "...",
      "enable": [ ... ],            // all must hold
      "enable_any": [ ... ],        // optional; at least one must also hold
      "prerequisites": [ ... ],     // must hold while starting/running, or it faults
      "latching": true, "release": [ ... ],   // a mode engaged by a button, until release holds
      "start_ready": false,         // optional; true = ready when the session starts (e.g. a mast already up)
      "start_time_s": { "value": 15, "source": "...", "confidence": "medium" },
      "stop_time_s":  { "value": 0,  "source": "...", "confidence": "estimate" },
      "source": "...", "confidence": "medium", "notes": "..." }
  ],

  "procedures": [
    { "id": "launch", "name": "...", "source": "...", "confidence": "high", "notes": "...",
      "steps": [
        { "control": "fo.key", "position": 2, "description": "...",
          "wait_for": [ { "subsystem": "missile_prep", "is": "ready" }, { "input": "target_in_zone", "is": true } ],
          "min_wait_s": 0, "notes": "..." } ] }
  ]
}
```

Control kinds: `switch` (toggles, selectors, covers, locks), `key` (moves one position at a time), `button`
(momentary push-button), `wheel` (continuous; needs `min` and `max`, optional `initial_value`, `wraps`).
Positions count from 0 and follow `position_labels` when given. Covers and locks use 0 = closed/locked,
1 = open/unlocked.

Conditions: `{ "control": id, "is": n }`, `{ "control": id, "at_least": n }`,
`{ "subsystem": id, "is": "off|starting|ready|stopping|fault" }`, `{ "input": id, "is": true|false }`.

Procedure steps name a discrete control or button and a position. Wheel work (turning onto a target, setting the
range gate) is described in the step before which it happens; it isn't a step of its own.

(The block above uses comments for explanation; real files are plain JSON.)
