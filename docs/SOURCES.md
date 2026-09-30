# Sources

Public and declassified material only. Every system number in `data/systems/` must cite one of these, or a
new entry added here.

## Physics textbooks

| Topic | Reference |
|---|---|
| Radar range equation, detection, noise | M. Skolnik, *Introduction to Radar Systems*, 3rd ed., ch. 1–2 |
| Propagation, 4/3-Earth refraction, multipath, clutter | M. Skolnik (ed.), *Radar Handbook*, 3rd ed. |
| Radar modelling with worked code | B. Mahafza, *Radar Systems Analysis and Design Using MATLAB* |
| Missile guidance (PN, command guidance, miss distance) | P. Zarchan, *Tactical and Strategic Missile Guidance* (AIAA) |
| Missile flyout, aerodynamics, propulsion | E. Fleeman, *Missile Design and System Engineering* (AIAA) |
| Standard atmosphere | ISO 2533:1975; U.S. Standard Atmosphere 1976 |
| Geodesy | NIMA TR8350.2 (WGS-84) |

## System-level information

| Source | Notes |
|---|---|
| Air Power Australia, C. Kopp, technical reports on Soviet/Russian SAM systems | Detailed public write-ups; cross-check the numbers |
| Air Power Australia, [Soviet SAM System Simulator](https://www.ausairpower.net/APA-Sov-SAM-Simulator.html) | Review of SAM Simulator's fidelity |
| CIA FOIA Electronic Reading Room | Declassified intelligence assessments (e.g. SA-2/SA-3 era) |
| Jane's Land-Based Air Defence (various editions) | Published performance figures |
| Manufacturer export brochures (e.g. Almaz-Antey) | Published envelopes for export variants |
| Former Warsaw Pact operator manuals (Hungarian, Czech, East German), museum sites | Procedures, crew roles, panel layouts |
| SAM Simulator (creator, [site](https://sites.google.com/site/samsimulator1972/home), now sign-in only; [archived copy](https://web.archive.org/web/20210120202958/https://sites.google.com/site/samsimulator1972/home)) | Reference for console behaviour and procedures |
| "2ПВО МСД. Часть 2. Зенитные ракетные полки (зрп)", Word attachment on the wrk.ru (Aviabaza) forum ([file](https://www.wrk.ru/forums/attachment.php?item=1005313)) | Osa crew, power sources, march/combat positions, readiness norms. Unsigned; medium confidence. Summarised in `docs/re/osa.md` |
| GlobalSecurity.org, [SA-8 GECKO / 9K33M3 Osa-AKM](https://www.globalsecurity.org/military/world/russia/sa-8.htm) | Osa deployment time, radar folding for transport |
| missilery.info, [9K33M3 Osa-AKM](https://en.missilery.info/missile/osa-akm) | Osa power sources, march-to-combat time |
| SAM Simulator documentation set v210109 (`SAMSIM_docs_210109.rar`, [MediaFire](https://www.mediafire.com/file/cgovijhk4hukl7n/SAMSIM_docs_210109.rar), linked from the creator's site) | Per-system manuals. Osa: `SA-8B_English.pdf` (v1, 2014), summarised in `docs/re/osa.md`. Kept in `reference/manuals/`, not in git |

## Validation scenarios

| Event | What to check |
|---|---|
| 1 May 1960 U-2 shootdown (S-75) | High-altitude engagement geometry and envelope |
| SA-2 effectiveness over North Vietnam, 1965–72 | Change in missiles per kill as countermeasures evolved |
| 27 March 1999 F-117 shootdown (S-125) | Low-RCS detection and tracking at short range |

## Tools and formats

| Item | Link |
|---|---|
| Tacview ACMI 2.2 format | https://www.tacview.net/documentation/acmi/en/ |
| Tacview real-time telemetry (needs Tacview Advanced) | https://www.tacview.net/documentation/realtime/ |
| Cesium for Unreal | https://cesium.com/platform/cesium-for-unreal/ |
