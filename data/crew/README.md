# Crew data

The crew of each system, as data, for the crew pillars (`docs/crew/README.md`: teamwork with NPC crewmates is
paramount; every action and command mapped and interconnected; callouts for every action; skill levels; the drill
master's perfect performance).

| File | What | Read by |
|---|---|---|
| `<system>_crew.json` | Seats (titles, duties, displays), the callout catalogue (orders, acknowledgements, reports, warnings, reminders: Russian and English), orders (the procedures each starts, or the task it hands the drill master), tasks (named goals as console conditions), world facts (what makes each simulation input true: seats, conditions, knowledge) and automatic reports (the callout spoken when a condition becomes true) | `simcore/crew_data.hpp`; `tools/crew_map.py` |
| `skill_levels.json` | The skill tiers (recruit, 3rd, 2nd, 1st class, master) with every behaviour parameter, and the published crew times they are calibrated to | `simcore/crew_data.hpp` |

Rules (as everywhere in `data/`): every item has `source` and `confidence`; phrasing documented in a source cites it,
SAM3D's own phrasing says "sam3d"; skill numbers are game values. The loader checks every station, procedure,
control, subsystem and input named here against the system's console file (`data/procedures/<system>.json`).

Conditions use the console file's form: `{ "control": id, "is": position }`, `{ "subsystem": id, "is": "ready" | "off"
| … }`, `{ "input": id, "is": true | false }`.

After changing either file, run `python tools/crew_map.py` to update `docs/crew/generated/`.
