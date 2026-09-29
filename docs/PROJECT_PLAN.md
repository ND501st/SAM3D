# Project plan

## Goal

A first-person, realistic 3D SAM game based on SAM Simulator v930.2, made with the creator's blessing:
- drive a SAM unit, choose a position, deploy and stow it
- work the operator consoles with the same procedures and mechanics as SAM Simulator, now as 3D cabins
- engage realistic air threats over real terrain
- give orders to NPC crew who operate the same controls the player does

## Findings so far

- SAM Simulator v930.2 covers 7 systems: S-75 Dvina, S-75M Volkhov, S-125 Neva, 2K11 Krug, 9K33 Osa, S-200 Vega and
  ZSU-23-4 Shilka. There are about 1,600 data files.
- The `.dat` files are **images and sounds only** (JPEG/BMP panel art, WAV). None of the mechanics are in them.
- `sam.exe` is VB6 compiled to **native x86** (not P-code). It has 67 unnamed forms (`Form1`–`Form67`) and about
  1,300 event handlers, and nearly all the logic is in them. See `REVERSE_ENGINEERING.md`.
- **The biggest shortcut: ask the creator for the VB6 source (`.frm`/`.bas`) and written permission to reuse his art.**
- Tacview isn't needed for the core game. ACMI export already exists in simcore for debriefs and debugging.

## Realism approach

| Layer | Source of truth |
|---|---|
| Consoles, switch order, operator procedures | SAM Simulator's behaviour, plus public Warsaw Pact operator manuals and veteran accounts |
| Physics (radar, propagation, clutter, terrain masking, missile flight, guidance) | Public textbooks, built from first principles (`SOURCES.md`) |
| Per-system numbers | Public and declassified figures, calibrated so simulated flyouts match published envelopes |
| Validation | Historical engagements (1960 U-2, Vietnam SA-2 kill rates, 1999 F-117) |

Public and declassified material only.

## Scope

- **Vertical slice: 9K33 Osa.** Drive, deploy, two NPC crew, one complete engagement. Rough estimate:
  6–12 months part-time, solo.
- Second system: ZSU-23-4 Shilka. Fixed sites (S-75, S-125, S-200) come later; they don't fit "drive around" and
  involve large crews.

## Roadmap

**M0 — Foundation (done)**
- [x] simcore: geodesy/ENU, ISA atmosphere, radar equation, beam loss, radar horizon, terrain LOS
- [x] simcore: aircraft waypoints, point-mass missile with PN, proximity fuze, world and events, ACMI export
- [x] simcore: control panels, procedures, NPC crew acting on the same controls
- [x] Python prototype mirroring simcore, with matching tests
- [x] UE5 project: sim subsystem at fixed 100 Hz, entity proxies, demo scenario

**M1 — Engagement loop that feels right**
- [ ] Radar model: scanning search radar, track radar with angle/range tracking loops, clutter and multipath at low altitude
- [ ] Radio command guidance (e.g. three-point / half-lead) driven by the tracker, alongside PN
- [ ] Guidance noise, autopilot lag, Mach-dependent drag, probabilistic warhead kill model
- [ ] Target AI: attack profiles, terrain masking, evasive manoeuvres, basic jamming
- [ ] Calibrate the Osa's parameters (`data/systems/osa.json`) against public sources

**M2 — Osa vertical slice in Unreal**
- [ ] Cesium for Unreal terrain, plus a `simcore::Terrain` backed by Cesium heights
- [ ] Drivable Osa (Chaos Vehicles), deploy/stow sequence
- [ ] 3D cabin: first-person, clickable controls bound to `ControlPanel`, radar scopes as render targets
- [ ] Two NPC crew with an order menu, animations, voice lines
- [ ] One scripted mission plus debrief (ACMI export)

**M3+ — More systems and content**
- Shilka, then fixed-site systems; mission editor; historical scenarios; multiplayer crew (maybe)

## Open questions for the creator

1. Can he share the VB6 source, or at least formulas/notes for radar, tracking and guidance behaviour?
2. Permission (in writing) to reuse or reference panel art, sounds and procedures?
3. Any operator manuals or veteran contacts he used?
