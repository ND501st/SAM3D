# Project plan

Revised 2 October 2026 after the research review (`docs/BRIEFING_2026-10.md`,
`docs/research/OSA_RESEARCH_2026-10.md`).

## Goal

A first-person, realistic 3D air-defence game built on the mechanics of SAM Simulator v930.2, made with the creator's
blessing. The first system is the **9K33M2 Osa-AK as fielded in the 1980s**, played as a **battery**:

- drive the vehicle, choose a position, orient and deploy it, and stow it again
- take any of the five crew seats (commander, deputy commander, senior operator, operator, driver) or the battery
  commander's PU-12 command post; NPCs fill the other seats and work the same controls
- work the consoles with SAM Simulator's "realistic to the switch" fidelity, now as 3D cabins
- receive target designations from the PU-12 by radio, or search autonomously with the vehicle's own radar
- engage over real terrain, against training targets, aircraft, helicopters, jammers and anti-radiation missiles
- debrief what each crew member did; no points, as in SAM Simulator and in reality

**Crew and teamwork (the project owner's design pillars, 16 Oct 2026; `docs/crew/README.md`).** Teamwork with NPC
crewmates is paramount. The game has two halves: *in the crew*, the player is one member of the team, learning the
drill and how to operate each seat; *in command*, the player gives the orders, first for the single Osa, then for the
battery. Every NPC works with every other; every action and command is mapped and interconnected (the generated action
map), with behaviour paths, callouts and commands for every environment and outcome. NPC skill levels come from player
progression and quests and affect reaction, speed and accuracy everywhere. An ideal progression of events, the drill
master's perfect performance, is the reference for the training overlay, like a chess engine that always knows the
best move. One crewmate's action depends on others' knowledge and actions (someone must know where the target is,
someone must have driven within reach), and that interdependence is mapped.

What SAM3D adds to SAM Simulator, in the creator's own words its limit is "a 3~5 crews work, one handed": a crew,
a battery, driving and deploying, and a 3D world seen through the TV sight.

## Findings so far

- SAM Simulator v930.2 covers 7 systems: S-75 Dvina, S-75M Volkhov, S-125 Neva, 2K11 Krug, 9K33 Osa, S-200 Vega and
  ZSU-23-4 Shilka. Its `.dat` files are images and sounds only; the mechanics are in `sam.exe` (VB6 native code).
- Its Osa (added in v0.928, April 2013) starts deployed and powered, has three operator panels plus the PU-12 plotting
  board, a working TV sight, ARMs, jamming and night situations. The creator designed it with a Greek Osa officer on
  the SimHQ forum; that thread is now a primary source (`docs/research/OSA_RESEARCH_2026-10.md` §1).
- The real Osa-AK: crew of five; most targets came by voice from the battery's PU-12; six missiles, two channels;
  cold start-up about 6.8 min, march-to-combat ≤ 4 min, reaction 26–40 s. Details and sources in the research file.
- **The biggest shortcut is still the creator**: his VB6 source, written permission for his art, and answers to the
  conflicts listed in the research file (§10).

## Realism approach

| Layer | Source of truth |
|---|---|
| Consoles, switch order, lamps, procedures | SAM Simulator's behaviour first; corrected by operator-country documents and operator accounts where they disagree, and each disagreement recorded |
| Crew, organisation, timings, tactics | Operator-country documents (NVA, Soviet/Russian, Polish, Belarusian), then veteran accounts |
| What decides hits and misses | A gameplay-level model with tunable numbers (`data/gameplay/`), calibrated to published single-shot kill-probability ranges; no detailed weapon physics |
| Per-system numbers | Public and declassified, every one cited with tier and confidence (`docs/SOURCES.md`) |
| Validation | Osa events: training ranges (Ashuluk, Kapustin Yar, Ustka, NAMFI), Lebanon 24 Jul 1982, Angola 1987–88 |

Public and declassified material only. Western "facts and stats" pages are not used for decisions.

## Scope

- **Vertical slice:** one Osa-AK battery position: the player's vehicle with its crew, the PU-12, one complete
  mission from march to stow. Rough estimate: 6–12 months part-time, solo.
- Then: the other three vehicles of the battery, the TZM, historic scenarios, the Osa-AKM variant.
- Second system: ZSU-23-4 Shilka (it shares the PU-12). Fixed sites (S-75, S-125, S-200) later.

## Roadmap

Task-level order and status: `docs/DEVELOPMENT_ROADMAP.md`.

**M0 — Foundation (done)**
- [x] simcore: geodesy, atmosphere, radar equation, terrain LOS, world, ACMI export; Python prototype
- [x] UE5 project with the simulation at a fixed 100 Hz

**M1 — The Osa-AK console, correct (mostly done; accuracy pass next)**
- [x] Consoles and procedures from data; equipment, interlocks, crew order queue; debrief
- [x] Scenarios and a deterministic runner; gameplay-level engagement model
- [x] 3D cabin with working controls, lamps and instruments (search scope, range indicator, TV, dials, voltmeter)
- [ ] Accuracy pass: five-man crew and seats; real start-up and function check; missile load, selection, channels and
      lamps; search beams, IFF and range rings; vehicle heading and orientation; manual TV tracking; zones and
      guidance methods; kill chances calibrated

**M2 — The battery**
- [ ] PU-12 command post and plotting board; target designation by radio
- [ ] Four vehicles per battery, TZM reload, fuel
- [ ] Crew order menu, NPC bodies and voice; one mission from march to stow, with debrief

**M3 — Threats and history**
- [ ] Jamming, anti-radiation missiles and the emissions drill; target behaviour (pop-up helicopters, low strike)
- [ ] Training-range scenarios, Angola 1987–88, Lebanon 1982; instructor mode modelled on the 9Ф632 trainer

**M4+ — More**
- Osa-AKM; ZSU-23-4 Shilka; mission editor; multiplayer crew

## Open questions for the creator

1. Can he share the VB6 source, or notes on how the Osa's consoles, lamps and guidance phases behave?
2. Written permission to reuse or reference his panel art, sounds and procedures?
3. The conflicts we found: ЗАХВАТ / ВКЛ ВЫС timing, the middle search scale (35 or 30 km), preparation 13 or 15 s,
   the TV lens fields.
4. Who were his operator contacts, and would they review SAM3D's Osa?
