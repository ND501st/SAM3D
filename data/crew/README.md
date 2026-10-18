# Crew data

The crew of each system, as data, for the crew pillars (`docs/crew/README.md`: teamwork with NPC crewmates is
paramount; every action and command mapped and interconnected; callouts for every action; skill levels; the drill
master's perfect performance).

| File | What | Read by |
|---|---|---|
| `<system>_crew.json` | Seats (titles, duties, displays), the callout catalogue (orders, acknowledgements, reports, warnings, reminders: Russian and English), orders (the procedures each starts, work that is not a console drill in `actions` — `compass_survey`, the crew chief's compass work — or the task it hands the drill master), tasks (named goals as console conditions), world facts (what makes each simulation input true: seats, conditions, knowledge) and automatic reports (the callout spoken when a condition becomes true) | `simcore/crew_data.hpp`; `tools/crew_map.py` |
| `skill_levels.json` | The skill tiers (recruit, 3rd, 2nd, 1st class, master) with every behaviour parameter, and the published crew times they are calibrated to | `simcore/crew_data.hpp` |
| `body.json` | The crewman's body: stature, joint heights and segment lengths as fractions of it (`winter_biomech`), seat height and head turn (`gost_12_2_032_78`), bend, twist, turn and crouch limits, hand motion; each seat's gaze and rest posture (the driver's hands on the wheel, feet on the pedals) | `simcore/crew_body.hpp`; `tools/blender/build_crew_body.py` (the meshes take its sizes) |
| `voices.json` | The crew's voices: languages in order of preference, each seat's pitch and rate (the speech engine's −10..10), stress, the intercom's and the radio's bands, how the crew pronounces its abbreviations | `simcore/crew_voice.hpp`; Unreal `USimCrewVoiceComponent` |

In `<system>_crew.json` (roadmap 8.8, 8.10):

- **`commander_seat`** (default `commander`) is the seat that commands the crew: the NPC commander's seat, whose
  picture the decisions use, who gives the orders and is not acknowledged by himself. The PU-12M's is
  `battery_commander`.
- **A fact's `by_action`** means one of its seats makes the input *true* by an action of his own (the driver halts);
  making it false is not a way the drill master takes.
- **A seat's `stand_for`** lists the controls out of its seated reach. They are checked against the body model and the
  cabin by a test, and `standing` gives the time to get up (`stand_s`) and sit down (`sit_s`).
- **An order's `needs`** names what it can't do without: `contact`, `tracked`, `jamming`, `missiles`.
- **A fact's `unmet_ru` and `unmet_en`** are the words for "it isn't so" ("Цель не в зоне"). The order menu greys an
  order with them, and the seat answers "Не могу: ..." with them.

Rules (as everywhere in `data/`): every item has `source` and `confidence`; phrasing documented in a source cites it,
SAM3D's own phrasing says "sam3d"; skill numbers are game values. The loader checks every station, procedure,
control, subsystem and input named here against the system's console file (`data/procedures/<system>.json`).

Conditions use the console file's form: `{ "control": id, "is": position }`, `{ "subsystem": id, "is": "ready" | "off"
| … }`, `{ "input": id, "is": true | false }`.

After changing either file, run `python tools/crew_map.py <system>` (`osa_ak`, `pu12m`) to update `docs/crew/generated/`.
