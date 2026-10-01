# Studying `sam.exe` (SAM Simulator v930.2)

**First choice: ask the creator for the VB6 source.** Reverse-engineering is the fallback. Before starting, confirm
he's happy for you to decompile it.

**Less needed than it was (October 2026).** The research review (`docs/research/OSA_RESEARCH_2026-10.md`) found the
creator's own development thread, where he explains the Osa's stations, lamps, guidance phases and methods with a
Greek Osa officer, and an NVA veteran's record of crew, firing sequence and timings. SAM3D also decides outcomes with
a gameplay-level model, so the game's internal formulas matter less. What only `sam.exe` can still tell us: how its
search picture, clutter and jamming look and behave frame by frame, and the exact lock and break-lock conditions.

## What we already know

| Item | Finding |
|---|---|
| Installer | VB6 Package & Deployment Wizard (`setup.exe`, `SETUP.LST`, `SAM1.CAB`–`SAM7.CAB`) |
| Executable | `sam.exe`, ~100 MB, PE32 x86, VB6 **native code** (ProjectInfo `lpNativeCode` ≠ 0) |
| Objects | 67 forms named `Form1`…`Form67` plus one module (`Module1`), ~1,300 methods in total |
| Runtime deps | `MSVBVM60.DLL`, `COMCTL32.OCX`, `MSWINSCK.OCX` (Winsock), `MCI32.OCX` (sound), `MSSTDFMT.DLL`. **No DirectX.** |
| Data | ~1,600 `.dat` files, prefixed per system (`volhov`, `krug`, `neva`, `osa`, `dvina`, `vega`, `silka`, `world`). They are JPEG/BMP images and WAV audio (panel art, scopes, sounds). |
| Size | ~3.6 GB installed. Most of `sam.exe`'s size is embedded form resources (images). |

## Workflow

1. Extract the installer on the PC into `reference/samsim/` (7-Zip opens the `.rar`; run `setup.exe` or extract the CABs).
2. **VB Decompiler** (Lite is free) recovers form layouts, control names and event-handler boundaries. This gives the
   map: which form is which console, and which handler is which button or timer.
3. **Ghidra** (free) turns handlers into pseudo-C. Timer handlers (`TimerN_Timer`) are the most likely place for the
   simulation loop.
4. Identify variables from their use (range, azimuth, elevation, missile speed...) and write each recovered formula
   into `docs/re/<system>.md` with the address it came from.
5. Re-implement in `prototype/`, then compare with the running game: same inputs, same outputs. Then port to `simcore/`.

Suggested model: **Claude Opus 5.5** for reading decompiler output (paste one handler at a time plus your notes).

## What to extract (priority order, for the Osa)

1. How the search scope shows targets, clutter and jamming (normal video vs MTI, range-gate pull-off), for SAM3D's
   display behaviour
2. Lock-on and break-lock conditions (angle and range gates, low targets, jamming)
3. Missile-channel lamp timing after launch (to check the sequence from the forum and the NVA account)

Power-up and interlocks, launch conditions and fuzing are now covered well enough by the manual, the forum thread and
the NVA record; the Osa's start-up and stow aren't in SAM Simulator at all (its Osa starts deployed).
