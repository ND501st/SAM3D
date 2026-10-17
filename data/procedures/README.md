# Console data files

One JSON file per system: its operator controls, the equipment behind them, and its procedures. Loaded by
`simcore::LoadConsoleFile` (`simcore/include/simcore/console_data.hpp`), which rejects unknown keys, missing sources
and bad references, and reports the path to the problem (e.g. `controls[4].guard: unknown control 'x'`).

The rules in `data/systems/README.md` apply: public material only, best source tier first, and every item has a
`source` and a `confidence` (`high` / `medium` / `low` / `estimate`). Use `low` or `estimate` for anything inferred
rather than read in a source, and say why in `notes`.

**The Osa file** has the real five-man crew (6.1) and, since the eyes-on pass (6.0), every control seen on SAM
Simulator's panels with its start position as measured in the running game. Controls whose function isn't known
are present but inert (no subsystem uses them). Still planned (6.3): missile channels with their own lamps and
missile containers (`docs/re/osa.md` → "Corrections from operators").

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

  // Optional: the vehicle's fuel (roadmap 7.4, simcore/fuel.hpp): a tank, the power-source subsystems that burn it (litres an
  // hour stationary and moving), a reserve fraction kept for driving, and refuelling from the TZM. At the reserve the
  // power_cut_input is set; make the power sources' enable conditions require it false.
  "fuel": { "tank_l": 300, "reserve_fraction": 0.5,
            "sources": [ { "subsystem": "gta", "stationary_lph": 75, "moving_lph": 100 } ],
            "stopped_input": "vehicle_stopped", "refuel_input": "tzm_alongside", "refuel_lpm": 60, "power_cut_input": "fuel_power_cut",
            "source": "...", "confidence": "medium" },

  // Optional: states a session can start in instead of cold (roadmap 6.2; a scenario's own_unit.readiness picks one):
  // controls already set, inputs, subsystems warmed up at once (in dependency order: each must be commanded on by then),
  // and whether the vehicle is already oriented. "cold" is the default and has no entry.
  "readiness": {
    "deployed": { "description": "...", "controls": { "pwr.gta": 1, "apu.lock": 1 }, "inputs": { "vehicle_stopped": true },
                  "ready": [ "gta", "power", "equipment_warm" ], "oriented": true, "source": "...", "confidence": "medium" }
  },

  // Optional: the missile load and the two guidance channels (simcore/launcher.hpp, roadmap 6.3), run by the scenario
  // runner. At the attention input (ВНИМАНИЕ) or once pair_preparation is running, one missile is picked per channel the
  // channel_control allows (lowest "order" first), and the channel's transmitter input comes on. A launch takes the
  // first free channel in the selector's order; its inputs then follow the missile: away, captured (capture_s),
  // medium (medium_s), narrow (narrow_s), fuse (fuse_range_m from the target, or with medium when jam_control is on: method Н, voenizdat_osa_1991 p136).
  // Reloading from the TZM (roadmap 7.3): while reload_input is set and stopped_input is set, one missile goes into an
  // empty container every reload_s_per_missile, once the pair is spent and no channel is guiding.
  // Launches are min_interval_s apart: hold_input is set while the next must wait (make the armed subsystem need it
  // false, so ПУСК ГОТОВ goes out and the crew waits); when the picked missiles are spent pair_preparation is
  // restarted (instead of "launch.restarts"). Each container's input is its ammunition lamp.
  "launcher": {
    "containers": [ { "id": "3-I", "channel": 1, "order": 1, "input": "ammo_3_i" } ],
    "channels": [ { "transmitter": "ch1_vkl_vys", "away": "ch1_skhod", "captured": "ch1_zakhvat", "medium": "ch1_sr_l",
                    "narrow": "ch1_uzkiy_luch", "fuse": "ch1_k3" }, { } ],
    "channel_control": "msl.channels", "channel_positions": [ [2], [1], [1, 2] ],  // per position: channels, firing order
    "attention_input": "target_near_zone", "jam_control": "fo.pomekha", "pair_preparation": "missile_prep",
    "hold_input": "launch_hold",
    "capture_s": 0.8, "medium_s": 2.0, "narrow_s": 4.0, "fuse_range_m": 120, "min_interval_s": 4, "nominal_flight_s": 20,
    // Optional. Entering change_position of the control changes the command transmitters' letter: both lock inputs
    // (channel I, II) go out for relock_s and the next launch waits.
    "letter": { "control": "spk.letter", "change_position": 2, "lock_inputs": [ "ch1_apch_lock", "ch2_apch_lock" ], "relock_s": 1.0,
                "source": "...", "confidence": "medium" },
    // Optional. select_control picks a container per position ("" = off); fire_control throws it off unguided for
    // flight_s (no channel, no outcome); ready_input is set while the selector is on a loaded container.
    "emergency": { "select_control": "msl.emergency_select", "select_containers": [ "3-II", "", "3-I" ],
                   "fire_control": "msl.emergency", "ready_input": "emerg_ready", "flight_s": 25.5,
                   "source": "...", "confidence": "medium" },
    // Optional, top level (next to "launcher"): the two R-123M sets and the R-124 intercom (roadmap 7.8,
    // simcore/crew_comms.hpp). Each radio names its controls (power, mode, squelch, volume, antenna: 3 positions = 4 m whip,
    // 1 m whip, emergency wire) and two subsystems: `warm` (can receive, 10 min after switching on) and `transmit` (3 min after
    // duty reception). `ranges_km` are the published ranges, turned into receiving losses (40 log10 of the ratio against the
    // 4 m whip with the suppressor off). The intercom names the commander's box A-1 (with the position that is on the radio and
    // the one that is off), the deputy's A-2 and the driver's ПВ, the subsystem that says the intercom is up, and the times
    // an order takes when shouted (quiet, and noisy while any of `noisy` runs). The preset selectors and the headset switch
    // (radio.*.preset, radio.headset) belong to the command network (c2_sources.hpp), not to this block.
    //   "comms": { "radios": [ { "set": 0, "name": "left", "power": "radio.left.power", "mode": "radio.left.mode",
    //                            "squelch": "radio.left.squelch", "volume": "radio.left.volume", "antenna": "radio.left.antenna",
    //                            "warm": "r123m_left", "transmit": "r123m_left_tx" }, { "set": 1, ... } ],
    //              "ranges_km": { "whip_4m_squelch_off": 20, "whip_4m_squelch_on": 13, "whip_1m": 8, "emergency_wire": 4 },
    //              "intercom": { "commander": "intercom.a1", "commander_radio_position": 2, "commander_off_position": 0,
    //                            "deputy": "intercom.a2", "deputy_radio_position": 1, "driver": "intercom.pv", "driver_radio_position": 1,
    //                            "subsystem": "r124_intercom", "shout_quiet_s": 1.5, "shout_noisy_s": 4.0, "noisy": [ "gta" ] },
    //              "source": "...", "confidence": "medium" }
    // Optional. A prepared pair is held at most hold_s; then its gyros have overheated (the container's hot_input is
    // set, its lamp flashes), the next pair is prepared, and the hot missile rests rest_s (long_rest_s after `cycles` holds).
    "prep_hold": { "hold_s": 600, "rest_s": 600, "cycles": 3, "long_rest_s": 3600, "source": "...", "confidence": "medium" },
    // Optional. Per position of the control, the channel whose missile the range indicator follows (0 = the target).
    "range_sweep": { "control": "ssc.target_select", "channels": [ 1, 0, 2 ], "source": "...", "confidence": "medium" },
    "source": "...", "confidence": "medium", "notes": "..."
  },

  // Optional: the crew seats. With this list every control needs a "station" from it, and crew roles
  // (scenarios, the gameplay file's aim roles) must be one of these ids.
  "stations": [ { "id": "senior_operator", "label": "Senior operator", "source": "flak11 (9A33-3)", "confidence": "high" } ],

  // Yes/no signals the sim sets (target geometry etc.), so lamps and automatic sequences can use them.
  "inputs": [ { "id": "target_in_zone", "description": "...", "source": "...", "confidence": "high" } ],

  // In dependency order: a guard (cover or lock) before the control it guards.
  "controls": [
    { "id": "soc.hv_cover", "label": "...", "station": "senior_operator", "kind": "switch",
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

**One seat per procedure.** All the controls a procedure touches must belong to one station; the loader names that
station on the procedure, and only a crew member in that seat will carry it out (others refuse it and report
`Aborted`, "station ..."). Work that needs several seats (acquisition, stowing) is one procedure per seat, ordered
together; a step can `wait_for` what another seat does (e.g. the operator waits for `auto_acquisition`, which the
deputy commander's ЦУ АЗ starts).

(The block above uses comments for explanation; real files are plain JSON.)
